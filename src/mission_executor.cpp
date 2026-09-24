#include "amr/mission_executor.hpp"

#include <ignition/msgs/stringmsg.pb.h>
#include <opencv2/core.hpp>
#include <opencv2/core/persistence.hpp>

#include <algorithm>
#include <sstream>

namespace amr {
namespace {
std::string fileName(const std::string &path) {
  const auto slash = path.find_last_of('/');
  return slash == std::string::npos ? path : path.substr(slash + 1);
}
}

MissionExecutor::MissionExecutor(std::shared_ptr<NavigationGraph> graph,
                                 std::shared_ptr<RouteManager> routes,
                                 std::string robotId)
    : graph_(std::move(graph)), routes_(std::move(routes)), robotId_(std::move(robotId)) {
  commandNode_.Subscribe<ignition::msgs::StringMsg>("/mission/command", [this](const auto &message) {
    if (handleCommand(message.data())) publishStatus();
  });
  statusPublisher_ = statusNode_.Advertise<ignition::msgs::StringMsg>("/mission/status");
  publishStatus();
}

const char *MissionExecutor::stateName(MissionState state) noexcept {
  switch (state) {
    case MissionState::Queued: return "queued";
    case MissionState::Navigating: return "navigating";
    case MissionState::Arrived: return "arrived";
    case MissionState::Paused: return "paused";
    case MissionState::Completed: return "completed";
    case MissionState::Cancelled: return "cancelled";
    case MissionState::Failed: return "failed";
  }
  return "unknown";
}

void MissionExecutor::setStateLocked(MissionState state, std::string event) {
  state_ = state;
  event_ = std::move(event);
  motionEnabled_ = state == MissionState::Navigating;
}

bool MissionExecutor::load(const std::string &path, std::string &error) {
  try {
    cv::FileStorage input(path, cv::FileStorage::READ | cv::FileStorage::FORMAT_JSON);
    if (!input.isOpened()) { error = "Cannot open mission: " + path; return false; }
    int version = 1; if (!input["version"].empty()) input["version"] >> version;
    if (version != 1) { error = "Unsupported mission version."; return false; }
    const cv::FileNode tasks = input["tasks"];
    if (!tasks.isSeq() || tasks.empty()) { error = "mission.tasks must be a non-empty array."; return false; }
    std::vector<MissionTask> loaded;
    for (const auto &node : tasks) {
      MissionTask task; std::string type;
      node["id"] >> task.id; node["type"] >> type; node["to"] >> task.destination;
      if (task.id.empty() || type != "navigate" || task.destination.empty() || !graph_->hasNode(task.destination)) {
        error = "Each mission task requires unique id, type 'navigate', and a graph checkpoint 'to'."; return false;
      }
      if (std::any_of(loaded.begin(), loaded.end(), [&](const auto &known) { return known.id == task.id; })) {
        error = "Duplicate mission task id: " + task.id; return false;
      }
      loaded.push_back(std::move(task));
    }
    bool autoAdvance = false; if (!input["auto_advance"].empty()) input["auto_advance"] >> autoAdvance;
    std::lock_guard<std::mutex> lock(mutex_);
    missionPath_ = path; tasks_ = std::move(loaded); activeTask_ = 0; autoAdvance_ = autoAdvance;
    current_ = graph_->defaultStart();
    for (auto &task : tasks_) task.state = "queued";
    routes_->setRoute(current_, "");
    setStateLocked(MissionState::Queued, "mission loaded; waiting for start");
  } catch (const cv::Exception &exception) { error = std::string("Invalid mission JSON: ") + exception.what(); return false; }
  publishStatus();
  return true;
}

bool MissionExecutor::dispatchLocked() {
  if (activeTask_ >= tasks_.size()) {
    routes_->setRoute(current_, "");
    setStateLocked(MissionState::Completed, "all mission tasks completed");
    return true;
  }
  if (current_.empty() || !graph_->hasNode(current_)) {
    setStateLocked(MissionState::Failed, "cannot dispatch: current checkpoint is unknown");
    return false;
  }
  auto &task = tasks_[activeTask_];
  if (task.destination == current_) {
    task.state = "arrived";
    routes_->setRoute(current_, task.destination);
    setStateLocked(MissionState::Arrived, "already at " + task.destination);
    return true;
  }
  routes_->setRoute(current_, task.destination);
  task.state = "active";
  setStateLocked(MissionState::Navigating, "navigating to " + task.destination);
  return true;
}

bool MissionExecutor::handleCommand(const std::string &command) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (command.rfind("create:", 0) == 0) {
    std::vector<MissionTask> created;
    std::istringstream input(command.substr(7));
    std::string destination;
    while (std::getline(input, destination, ',')) {
      if (destination.empty() || !graph_->hasNode(destination)) { event_ = "invalid checkpoint: " + destination; return true; }
      created.push_back({"task_" + std::to_string(created.size() + 1), destination, "queued"});
    }
    if (created.empty()) { event_ = "create ignored: add at least one checkpoint"; return true; }
    missionPath_ = "UI mission"; tasks_ = std::move(created); activeTask_ = 0; autoAdvance_ = false;
    current_ = graph_->defaultStart(); routes_->setRoute(current_, "");
    setStateLocked(MissionState::Queued, "mission created in UI; waiting for start");
    return true;
  }
  if (command == "start") {
    if (state_ != MissionState::Queued && state_ != MissionState::Completed) { event_ = "start ignored: mission is " + std::string(stateName(state_)); return true; }
    if (state_ == MissionState::Completed) { activeTask_ = 0; for (auto &task : tasks_) task.state = "queued"; }
    return dispatchLocked();
  }
  if (command == "pause" || command == "stop") {
    if (state_ == MissionState::Navigating) { resumeState_ = state_; setStateLocked(MissionState::Paused, command == "stop" ? "persistent stop requested" : "paused at next command cycle"); }
    else event_ = command + " ignored: mission is " + stateName(state_);
    return true;
  }
  if (command == "resume") {
    if (state_ == MissionState::Paused && resumeState_ == MissionState::Navigating) { setStateLocked(MissionState::Navigating, "resumed"); return true; }
    event_ = "resume ignored: no paused route"; return true;
  }
  if (command == "cancel") {
    if (state_ == MissionState::Navigating || state_ == MissionState::Paused || state_ == MissionState::Arrived) {
      if (activeTask_ < tasks_.size()) tasks_[activeTask_].state = "cancelled";
      routes_->setRoute(current_, ""); setStateLocked(MissionState::Cancelled, "active task cancelled");
    } else event_ = "cancel ignored: no active task";
    return true;
  }
  if (command == "retry") {
    if (activeTask_ < tasks_.size() && (state_ == MissionState::Arrived || state_ == MissionState::Cancelled || state_ == MissionState::Failed)) {
      tasks_[activeTask_].state = "queued"; return dispatchLocked();
    }
    event_ = "retry ignored: no retryable task"; return true;
  }
  if (command == "next") {
    if (state_ != MissionState::Arrived) { event_ = "next ignored: task has not arrived"; return true; }
    tasks_[activeTask_].state = "completed"; ++activeTask_; return dispatchLocked();
  }
  event_ = "unknown mission command: " + command;
  return true;
}

void MissionExecutor::onCheckpoint(const std::string &id) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!graph_->hasNode(id)) return;
  current_ = id;
  if (state_ != MissionState::Navigating || activeTask_ >= tasks_.size() || id != tasks_[activeTask_].destination) return;
  tasks_[activeTask_].state = "arrived";
  setStateLocked(MissionState::Arrived, "arrived at " + id);
  if (autoAdvance_) { tasks_[activeTask_].state = "completed"; ++activeTask_; dispatchLocked(); }
}

bool MissionExecutor::motionEnabled() const { std::lock_guard<std::mutex> lock(mutex_); return motionEnabled_; }

MissionSnapshot MissionExecutor::snapshot() const {
  std::lock_guard<std::mutex> lock(mutex_);
  MissionSnapshot snapshot;
  snapshot.mission = fileName(missionPath_); snapshot.robotId = robotId_; snapshot.state = stateName(state_);
  snapshot.current = current_; snapshot.event = event_; snapshot.tasks = tasks_; snapshot.motionEnabled = motionEnabled_;
  if (activeTask_ < tasks_.size()) snapshot.goal = tasks_[activeTask_].destination;
  return snapshot;
}

void MissionExecutor::publishStatus() {
  if (!statusPublisher_) return;
  const auto state = snapshot();
  ignition::msgs::StringMsg message;
  std::ostringstream text;
  text << "state=" << state.state << " | mission=" << state.mission << " | robot=" << state.robotId
       << " | current=" << state.current << " | goal=" << state.goal << " | event=" << state.event << " | tasks=";
  for (std::size_t i = 0; i < state.tasks.size(); ++i) {
    if (i) text << ",";
    text << state.tasks[i].id << ":" << state.tasks[i].state << "->" << state.tasks[i].destination;
  }
  message.set_data(text.str()); statusPublisher_.Publish(message);
}

}  // namespace amr

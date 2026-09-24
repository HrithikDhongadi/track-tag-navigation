#pragma once

#include "amr/navigation.hpp"
#include "amr/route_manager.hpp"

#include <ignition/transport/Node.hh>

#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace amr {

enum class MissionState { Queued, Navigating, Arrived, Paused, Completed, Cancelled, Failed };

struct MissionTask {
  std::string id;
  std::string destination;
  std::string state = "queued";
};

struct MissionSnapshot {
  std::string mission;
  std::string robotId;
  std::string state;
  std::string current;
  std::string goal;
  std::string event;
  std::vector<MissionTask> tasks;
  bool motionEnabled = false;
};

class MissionExecutor {
 public:
  MissionExecutor(std::shared_ptr<NavigationGraph> graph,
                  std::shared_ptr<RouteManager> routes, std::string robotId);

  bool load(const std::string &path, std::string &error);
  void onCheckpoint(const std::string &id);
  bool motionEnabled() const;
  MissionSnapshot snapshot() const;
  void publishStatus();

 private:
  bool handleCommand(const std::string &command);
  bool dispatchLocked();
  void setStateLocked(MissionState state, std::string event);
  static const char *stateName(MissionState state) noexcept;

  std::shared_ptr<NavigationGraph> graph_;
  std::shared_ptr<RouteManager> routes_;
  mutable std::mutex mutex_;
  std::string robotId_;
  std::string missionPath_;
  std::string current_;
  std::string event_ = "mission not loaded";
  std::vector<MissionTask> tasks_;
  std::size_t activeTask_ = 0;
  MissionState state_ = MissionState::Queued;
  MissionState resumeState_ = MissionState::Queued;
  bool autoAdvance_ = false;
  bool motionEnabled_ = false;
  ignition::transport::Node commandNode_;
  ignition::transport::Node statusNode_;
  ignition::transport::Node::Publisher statusPublisher_;
};

}  // namespace amr

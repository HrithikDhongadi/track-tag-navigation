#include "amr/ui/telemetry_store.hpp"
#include "amr/json_message.hpp"

#include <ignition/msgs/image.pb.h>
#include <ignition/msgs/stringmsg.pb.h>
#include <ignition/msgs/twist.pb.h>

#include <algorithm>
#include <cstring>
#include <iomanip>
#include <sstream>

namespace amr::ui {
namespace {
using Clock = std::chrono::steady_clock;
std::string stamped(const std::string &text) {
  const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(Clock::now().time_since_epoch()).count();
  std::ostringstream out;
  out << '[' << (seconds / 60) % 60 << ':' << std::setfill('0') << std::setw(2) << seconds % 60 << "] " << text;
  return out.str();
}
void addEvent(std::vector<std::string> &events, const std::string &event) {
  if (!events.empty() && events.back().substr(events.back().find("] ") + 2) == event) return;
  events.push_back(stamped(event));
  constexpr std::size_t maximumEvents = 100;
  if (events.size() > maximumEvents) events.erase(events.begin(), events.begin() + (events.size() - maximumEvents));
}
void copyImage(const ignition::msgs::Image &message, ImageFrame &target) {
  const std::size_t width = message.width(), height = message.height(), stride = message.step();
  if (message.pixel_format_type() != ignition::msgs::RGB_INT8 || !width || !height ||
      stride < width * 3 || message.data().size() < stride * height) return;
  std::vector<unsigned char> pixels(width * height * 3);
  for (std::size_t y = 0; y < height; ++y)
    std::memcpy(pixels.data() + y * width * 3, message.data().data() + y * stride, width * 3);
  target.rgb = std::move(pixels);
  target.width = static_cast<int>(width);
  target.height = static_cast<int>(height);
  ++target.sequence;
  target.received = Clock::now();
}
}

TelemetryStore::TelemetryStore() {
  imageNode_.Subscribe<ignition::msgs::Image>("/amr/front/image", [this](const auto &message) {
    std::lock_guard<std::mutex> lock(mutex_); copyImage(message, front_);
  });
  imageNode_.Subscribe<ignition::msgs::Image>("/amr/qr/image", [this](const auto &message) {
    std::lock_guard<std::mutex> lock(mutex_); copyImage(message, qr_);
  });
  statusNode_.Subscribe<ignition::msgs::StringMsg>("/amr/telemetry", [this](const auto &message) {
    std::lock_guard<std::mutex> lock(mutex_); telemetry_ = message.data(); telemetryReceived_ = Clock::now();
    if (telemetry_ != lastTelemetryEvent_) {
      addEvent(events_, jsonStringField(telemetry_, "message").value_or(telemetry_));
      lastTelemetryEvent_ = telemetry_;
    }
  });
  checkpointNode_.Subscribe<ignition::msgs::StringMsg>("/amr/checkpoint", [this](const auto &message) {
    std::lock_guard<std::mutex> lock(mutex_); checkpoint_ = message.data(); checkpointReceived_ = Clock::now();
    if (checkpoint_ != lastCheckpointEvent_) { addEvent(events_, "Checkpoint: " + checkpoint_); lastCheckpointEvent_ = checkpoint_; }
  });
  missionNode_.Subscribe<ignition::msgs::StringMsg>("/mission/status", [this](const auto &message) {
    std::lock_guard<std::mutex> lock(mutex_); mission_ = message.data(); missionReceived_ = Clock::now();
    if (mission_ != lastMissionEvent_) {
      addEvent(events_, "Mission: " + jsonStringField(mission_, "event").value_or(mission_));
      lastMissionEvent_ = mission_;
    }
  });
  stopPublisher_ = controlNode_.Advertise<ignition::msgs::Twist>("/amr/cmd_vel");
  missionPublisher_ = missionNode_.Advertise<ignition::msgs::StringMsg>("/mission/command");
}

bool TelemetryStore::sendMissionCommand(const std::string &command) {
  if (!missionPublisher_) return false;
  ignition::msgs::StringMsg message; message.set_data(command); missionPublisher_.Publish(message);
  std::lock_guard<std::mutex> lock(mutex_); addEvent(events_, "Mission command requested: " + command);
  return true;
}

bool TelemetryStore::stopRobot() {
  if (!stopPublisher_) return false;
  ignition::msgs::Twist stop;
  stop.mutable_linear()->set_x(0.0); stop.mutable_angular()->set_z(0.0);
  stopPublisher_.Publish(stop);
  std::lock_guard<std::mutex> lock(mutex_); addEvent(events_, "Emergency stop command sent");
  return true;
}
void TelemetryStore::clearEvents() { std::lock_guard<std::mutex> lock(mutex_); events_.clear(); }
std::uint64_t TelemetryStore::frontSequence() const { std::lock_guard<std::mutex> lock(mutex_); return front_.sequence; }
std::uint64_t TelemetryStore::qrSequence() const { std::lock_guard<std::mutex> lock(mutex_); return qr_.sequence; }
ImageFrame TelemetryStore::copyFront() const { std::lock_guard<std::mutex> lock(mutex_); return front_; }
ImageFrame TelemetryStore::copyQr() const { std::lock_guard<std::mutex> lock(mutex_); return qr_; }
StatusSnapshot TelemetryStore::status() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return {telemetry_, checkpoint_, mission_, events_, telemetryReceived_, checkpointReceived_, missionReceived_};
}
}

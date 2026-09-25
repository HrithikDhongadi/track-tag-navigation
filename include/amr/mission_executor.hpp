#pragma once

#include "amr/navigation.hpp"
#include "amr/route_manager.hpp"
#include "amr/control_config.hpp"

#include <ignition/transport/Node.hh>

#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace amr {

enum class MissionState { Queued, Navigating, Arrived, Paused, Completed, Cancelled, Failed, RecoveryRequired };

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
                  std::shared_ptr<RouteManager> routes, std::string robotId,
                  MissionConfig config = {});

  bool load(const std::string &path, std::string &error);
  // Used by the transport callback and tests; command semantics are identical.
  bool executeCommand(const std::string &command);
  void onCheckpoint(const std::string &id);
  void onRouteDeviation(const std::string &event);
  void onQrCameraTimeout();
  void checkTimeouts();
  bool motionEnabled() const;
  MissionSnapshot snapshot() const;
  void publishStatus();

 private:
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
  MissionConfig config_;
  std::chrono::steady_clock::time_point taskStartedAt_{};
  std::chrono::steady_clock::time_point lastProgressAt_{};
  ignition::transport::Node commandNode_;
  ignition::transport::Node statusNode_;
  ignition::transport::Node::Publisher statusPublisher_;
};

}  // namespace amr

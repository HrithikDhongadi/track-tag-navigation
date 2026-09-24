#pragma once

#include <chrono>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

#include <ignition/transport/Node.hh>

namespace amr::ui {

struct ImageFrame {
  std::vector<unsigned char> rgb;
  int width = 0;
  int height = 0;
  std::uint64_t sequence = 0;
  std::chrono::steady_clock::time_point received{};
};

struct StatusSnapshot {
  std::string telemetry;
  std::string checkpoint;
  std::string mission;
  std::vector<std::string> events;
  std::chrono::steady_clock::time_point telemetryReceived{};
  std::chrono::steady_clock::time_point checkpointReceived{};
  std::chrono::steady_clock::time_point missionReceived{};
};

class TelemetryStore {
 public:
  TelemetryStore();
  bool stopRobot();
  bool sendMissionCommand(const std::string &command);
  void clearEvents();
  std::uint64_t frontSequence() const;
  std::uint64_t qrSequence() const;
  ImageFrame copyFront() const;
  ImageFrame copyQr() const;
  StatusSnapshot status() const;

 private:
  mutable std::mutex mutex_;
  ImageFrame front_;
  ImageFrame qr_;
  std::string telemetry_;
  std::string checkpoint_;
  std::string mission_;
  std::string lastTelemetryEvent_;
  std::string lastCheckpointEvent_;
  std::string lastMissionEvent_;
  std::vector<std::string> events_;
  std::chrono::steady_clock::time_point telemetryReceived_{};
  std::chrono::steady_clock::time_point checkpointReceived_{};
  std::chrono::steady_clock::time_point missionReceived_{};
  ignition::transport::Node imageNode_;
  ignition::transport::Node statusNode_;
  ignition::transport::Node checkpointNode_;
  ignition::transport::Node controlNode_;
  ignition::transport::Node missionNode_;
  ignition::transport::Node::Publisher stopPublisher_;
  ignition::transport::Node::Publisher missionPublisher_;
};

}

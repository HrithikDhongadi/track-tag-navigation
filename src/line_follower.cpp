#include "amr/line_follower.hpp"
#include "amr/mission_executor.hpp"
#include "amr/junction_controller.hpp"
#include "amr/json_message.hpp"
#include "amr/logger.hpp"
#include "amr/runtime.hpp"
#include "amr/route_manager.hpp"
#include "amr_line_follower/control.hpp"

#include <ignition/msgs/image.pb.h>
#include <ignition/msgs/twist.pb.h>
#include <ignition/msgs/stringmsg.pb.h>
#include <ignition/transport/Node.hh>

#include <array>
#include <chrono>
#include <functional>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <thread>

namespace amr {
namespace {
using Clock = std::chrono::steady_clock;
struct Reading {
  double dark = 0, mean = 0;
  bool valid = false;
  Clock::time_point received{};
};
}  // namespace

int runLineFollower(const Options &options, std::atomic_bool &running,
                    std::shared_ptr<RouteManager> routes, std::shared_ptr<MissionExecutor> mission) {
  std::mutex mutex;
  std::array<Reading, 5> readings{};
  ignition::transport::Node node;
  ignition::transport::Node::Publisher publisher;
  auto telemetryPublisher = node.Advertise<ignition::msgs::StringMsg>("/amr/telemetry");
  if (!options.monitor) {
    publisher = node.Advertise<ignition::msgs::Twist>("/amr/cmd_vel");
    if (!publisher) { Logger::instance().error("Cannot advertise velocity topic"); return 1; }
  }
  for (int i = 0; i < 5; ++i) {
    std::function<void(const ignition::msgs::Image &)> callback =
        [&, i](const ignition::msgs::Image &image) {
          Reading reading;
          reading.received = Clock::now();
          const size_t width = image.width(), height = image.height(), stride = image.step();
          if (image.pixel_format_type() == ignition::msgs::RGB_INT8 && width > 0 &&
              height > 0 && width <= 4096 && height <= 4096 && stride >= 3 * width &&
              stride <= image.data().size() / height) {
            const auto *pixels = reinterpret_cast<const unsigned char *>(image.data().data());
            double sum = 0, dark = 0;
            for (size_t y = 0; y < height; ++y) for (size_t x = 0; x < width; ++x) {
              const auto *pixel = pixels + y * stride + 3 * x;
              const double gray = .299 * pixel[0] + .587 * pixel[1] + .114 * pixel[2];
              sum += gray;
              if (gray < options.control.lineFollower.darkThreshold) ++dark;
            }
            reading.mean = sum / (width * height);
            reading.dark = dark / (width * height);
            reading.valid = true;
          }
          std::lock_guard<std::mutex> guard(mutex);
          readings[i] = reading;
        };
    if (!node.Subscribe<ignition::msgs::Image>("/amr/line_" + std::to_string(i) +
                                                "/image", callback)) {
      Logger::instance().error("Cannot subscribe to camera ", i);
      return 1;
    }
  }
  Logger::instance().info(options.monitor ? "MONITOR: does not send movement commands." :
                          "DRIVE: stop on lost line/stale data; Ctrl+C sends stop.");
  Logger::instance().info("Sensors L -> R: 0 1 2 3 4. Mean: 0=black, 255=white.");
  const auto &lineConfig = options.control.lineFollower;
  const auto &junctionConfig = options.control.junction;
  Logger::instance().info("CONTROL: ", options.configPath.empty() ? "built-in defaults" : options.configPath,
                          " line(speed=", lineConfig.speedMps, ", kp=", lineConfig.kp,
                          ", threshold=", lineConfig.darkThreshold, ") junction(active=",
                          junctionConfig.minimumActiveSensors, ", total_dark=", junctionConfig.minimumTotalDark,
                          ", bias=L", junctionConfig.leftBiasRadps, "/R", junctionConfig.rightBiasRadps,
                          ", commit=", junctionConfig.commitDurationS, "s)");
  if (options.junctionTurn != TurnRequest::None || routes)
    Logger::instance().info("JUNCTION: fixed test turn enabled; it will commit only on a broad line pattern.");
  auto lastPrint = Clock::now() - std::chrono::seconds(1);
  JunctionController junction(options.junctionTurn, options.control.junction);
  auto previousLoop = Clock::now();
  while (isRunning(running)) {
    if (mission) mission->checkTimeouts();
    std::array<Reading, 5> snapshot;
    { std::lock_guard<std::mutex> guard(mutex); snapshot = readings; }
    const auto now = Clock::now();
    const double seconds = std::min(0.10,
        std::chrono::duration<double>(now - previousLoop).count());
    previousLoop = now;
    bool fresh = true;
    std::array<double, 5> dark{};
    for (int i = 0; i < 5; ++i) {
      fresh = fresh && snapshot[i].valid &&
          std::chrono::duration<double>(now - snapshot[i].received).count() < options.control.lineFollower.cameraTimeoutS;
      dark[i] = snapshot[i].dark;
    }
    const RouteSnapshot route = routes ? routes->snapshot() : RouteSnapshot{};
    const bool goalReached = route.goalReached;
    const MissionSnapshot missionSnapshot = mission ? mission->snapshot() : MissionSnapshot{};
    const bool routeRecoveryRequired = route.recoveryRequired || missionSnapshot.state == "recovery_required";
    const bool missionStopped = mission && !mission->motionEnabled();
    Command command;
    if (routes && !goalReached && !missionStopped && !routeRecoveryRequired) {
      if (const auto turn = routes->takeArmedTurn()) junction.setRequest(*turn);
    }
    if (goalReached || missionStopped || routeRecoveryRequired) {
      junction.reset();
      command = {};
    } else if (fresh)
      command = junction.update(dark, steer(dark, options.control.lineFollower.speedMps, options.control.lineFollower.kp), options.control.lineFollower.speedMps, seconds);
    else {
      junction.reset();
      command = {};
    }
    if (routes && junction.takeCompleted()) routes->onTurnCompleted();
    if (!options.monitor) {
      ignition::msgs::Twist message;
      message.mutable_linear()->set_x(command.speed);
      message.mutable_angular()->set_z(command.turn);
      publisher.Publish(message);
    }
    if (now - lastPrint >= std::chrono::milliseconds(500)) {
      std::ostringstream monitor;
      monitor << (routeRecoveryRequired ? "RECOVERY " : missionStopped ? "PAUSED  " : goalReached ? "GOAL    " : fresh ? (command.line ? "TRACKING " : "NO LINE  ") : "WAITING  ")
              << "bits=";
      for (const auto &reading : snapshot)
        monitor << (reading.valid ? (reading.dark >= .5 ? '1' : '0') : '?');
      monitor << " mean=[";
      for (const auto &reading : snapshot)
        monitor << std::fixed << std::setprecision(0) << reading.mean << ' ';
      monitor << "] dark=[";
      for (const auto &reading : snapshot)
        monitor << std::setprecision(2) << reading.dark << ' ';
      monitor << "] v=" << command.speed << " w=" << command.turn;
      if (options.junctionTurn != TurnRequest::None || routes)
        monitor << " junction=" << junction.stateName();
      Logger::instance().info(monitor.str());
      if (telemetryPublisher) {
        ignition::msgs::StringMsg telemetry;
        const std::string state = routeRecoveryRequired ? "recovery_required" : missionStopped ? "mission_paused" : goalReached ? "goal_reached" :
                                  fresh ? (command.line ? "tracking" : "no_line") : "waiting";
        const std::string message = routeRecoveryRequired ? "route deviation; stopped" : missionStopped ? "mission paused; stopped" : goalReached ? "goal reached; stopped" :
                                    fresh ? (command.line ? "tracking" : "no line") : "waiting for camera data";
        std::ostringstream text;
        text << "{\"schema_version\":1,\"state\":\"" << state
             << "\",\"message\":\"" << escapeJson(message) << "\""
             << ",\"linear_mps\":" << command.speed << ",\"angular_radps\":" << command.turn;
        if (routes) {
          text << ",\"route_current\":\"" << escapeJson(route.current)
               << "\",\"route_next\":\"" << escapeJson(route.next)
               << "\",\"route_heading\":\"" << escapeJson(route.heading)
               << "\",\"route_goal\":\"" << escapeJson(route.goal)
               << "\",\"route_event\":\"" << escapeJson(route.event) << "\"";
        }
        text << "}";
        telemetry.set_data(text.str());
        telemetryPublisher.Publish(telemetry);
        if (mission) mission->publishStatus();
      }
      lastPrint = now;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(33));
  }
    if (routes && junction.takeCompleted()) routes->onTurnCompleted();
  if (!options.monitor) {
    ignition::msgs::Twist zero;
    zero.mutable_linear()->set_x(0);
    zero.mutable_angular()->set_z(0);
    for (int i = 0; i < 5; ++i) {
      publisher.Publish(zero);
      std::this_thread::sleep_for(std::chrono::milliseconds(40));
    }
  }
  return 0;
}

}  // namespace amr

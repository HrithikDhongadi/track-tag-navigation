#include "amr/line_follower.hpp"
#include "amr/junction_controller.hpp"
#include "amr/runtime.hpp"
#include "amr_line_follower/control.hpp"

#include <ignition/msgs/image.pb.h>
#include <ignition/msgs/twist.pb.h>
#include <ignition/transport/Node.hh>

#include <array>
#include <chrono>
#include <functional>
#include <iomanip>
#include <iostream>
#include <mutex>
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

int runLineFollower(const Options &options, std::atomic_bool &running) {
  std::mutex mutex;
  std::array<Reading, 5> readings{};
  ignition::transport::Node node;
  ignition::transport::Node::Publisher publisher;
  if (!options.monitor) {
    publisher = node.Advertise<ignition::msgs::Twist>("/amr/cmd_vel");
    if (!publisher) { std::cerr << "Cannot advertise velocity topic\n"; return 1; }
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
      std::cerr << "Cannot subscribe to camera " << i << '\n';
      return 1;
    }
  }
  std::cout << (options.monitor ? "MONITOR: does not send movement commands.\n"
                                : "DRIVE: stop on lost line/stale data; Ctrl+C sends stop.\n")
            << "Sensors L -> R: 0 1 2 3 4. Mean: 0=black, 255=white.\n";
  const auto &lineConfig = options.control.lineFollower;
  const auto &junctionConfig = options.control.junction;
  std::cout << "CONTROL: " << (options.configPath.empty() ? "built-in defaults" : options.configPath)
            << " line(speed=" << lineConfig.speedMps << ", kp=" << lineConfig.kp
            << ", threshold=" << lineConfig.darkThreshold << ")"
            << " junction(active=" << junctionConfig.minimumActiveSensors
            << ", total_dark=" << junctionConfig.minimumTotalDark
            << ", bias=L" << junctionConfig.leftBiasRadps
            << "/R" << junctionConfig.rightBiasRadps
            << ", commit=" << junctionConfig.commitDurationS << "s)" << std::endl;
  if (options.junctionTurn != TurnRequest::None)
    std::cout << "JUNCTION: fixed test turn enabled; it will commit only on a broad line pattern.\n";
  auto lastPrint = Clock::now() - std::chrono::seconds(1);
  JunctionController junction(options.junctionTurn, options.control.junction);
  auto previousLoop = Clock::now();
  while (isRunning(running)) {
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
    Command command;
    if (fresh)
      command = junction.update(dark, steer(dark, options.control.lineFollower.speedMps, options.control.lineFollower.kp), options.control.lineFollower.speedMps, seconds);
    else {
      junction.reset();
      command = {};
    }
    if (!options.monitor) {
      ignition::msgs::Twist message;
      message.mutable_linear()->set_x(command.speed);
      message.mutable_angular()->set_z(command.turn);
      publisher.Publish(message);
    }
    if (now - lastPrint >= std::chrono::milliseconds(500)) {
      std::cout << (fresh ? (command.line ? "TRACKING " : "NO LINE  ") : "WAITING  ")
                << "bits=";
      for (const auto &reading : snapshot)
        std::cout << (reading.valid ? (reading.dark >= .5 ? '1' : '0') : '?');
      std::cout << " mean=[";
      for (const auto &reading : snapshot)
        std::cout << std::fixed << std::setprecision(0) << reading.mean << ' ';
      std::cout << "] dark=[";
      for (const auto &reading : snapshot)
        std::cout << std::setprecision(2) << reading.dark << ' ';
      std::cout << "] v=" << command.speed << " w=" << command.turn;
      if (options.junctionTurn != TurnRequest::None)
        std::cout << " junction=" << junction.stateName();
      std::cout << std::endl;
      lastPrint = now;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(33));
  }
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

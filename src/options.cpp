#include "amr/options.hpp"

#include <cmath>
#include <iostream>
#include <stdexcept>

namespace amr {
namespace {

void printUsage() {
  std::cout << "track_tag_navigation_controller [--config path.json] [--map map.json --goal checkpoint] [--mission mission.json] [--robot id] [--start checkpoint] [--view] [--front-view] "
               "[--turn left|right|straight|none] [--monitor] [--speed 0.10] [--kp 0.45] "
               "[--threshold 110] [--timeout 2.0]\n";
}

}  // namespace

bool parseOptions(int argc, char **argv, Options &options) {
  try {
    // Find the profile before processing overrides, so CLI values always win
    // regardless of their argument order.
    for (int i = 1; i < argc; ++i) {
      const std::string arg = argv[i];
      if (arg == "--help") {
        printUsage();
        return false;
      }
      if (arg == "--map") { if (++i >= argc) throw std::runtime_error("Missing value for --map"); options.mapPath = argv[i]; continue; }
      if (arg == "--goal") { if (++i >= argc) throw std::runtime_error("Missing value for --goal"); options.goal = argv[i]; continue; }
      if (arg == "--start") { if (++i >= argc) throw std::runtime_error("Missing value for --start"); options.start = argv[i]; continue; }
      if (arg == "--mission") { if (++i >= argc) throw std::runtime_error("Missing value for --mission"); options.missionPath = argv[i]; continue; }
      if (arg == "--robot") { if (++i >= argc) throw std::runtime_error("Missing value for --robot"); options.robotId = argv[i]; continue; }
      if (arg == "--config") {
        if (++i >= argc) throw std::runtime_error("Missing value for --config");
        options.configPath = argv[i];
      }
    }
    if (!options.configPath.empty()) {
      std::string error;
      if (!loadControlConfig(options.configPath, options.control, error))
        throw std::runtime_error(error);
    }

    for (int i = 1; i < argc; ++i) {
      const std::string arg = argv[i];
      if (arg == "--monitor") { options.monitor = true; continue; }
      if (arg == "--view") { options.view = true; continue; }
      if (arg == "--front-view") { options.frontView = true; continue; }
      if (arg == "--map") { if (++i >= argc) throw std::runtime_error("Missing value for --map"); options.mapPath = argv[i]; continue; }
      if (arg == "--goal") { if (++i >= argc) throw std::runtime_error("Missing value for --goal"); options.goal = argv[i]; continue; }
      if (arg == "--start") { if (++i >= argc) throw std::runtime_error("Missing value for --start"); options.start = argv[i]; continue; }
      if (arg == "--mission") { if (++i >= argc) throw std::runtime_error("Missing value for --mission"); options.missionPath = argv[i]; continue; }
      if (arg == "--robot") { if (++i >= argc) throw std::runtime_error("Missing value for --robot"); options.robotId = argv[i]; continue; }
      if (arg == "--config") { ++i; continue; }
      if (arg == "--turn") {
        if (++i >= argc) throw std::runtime_error("Missing value for --turn");
        const std::string value = argv[i];
        if (value == "left") options.junctionTurn = TurnRequest::Left;
        else if (value == "right") options.junctionTurn = TurnRequest::Right;
        else if (value == "straight") options.junctionTurn = TurnRequest::Straight;
        else if (value == "none") options.junctionTurn = TurnRequest::None;
        else throw std::runtime_error("--turn must be left, right, straight, or none.");
        continue;
      }
      if (arg == "--help") continue;
      if (i + 1 >= argc) throw std::runtime_error("Missing value for " + arg);
      const std::string value = argv[++i];
      size_t used = 0;
      const double number = std::stod(value, &used);
      if (used != value.size() || !std::isfinite(number))
        throw std::runtime_error("Invalid value for " + arg);
      if (arg == "--speed") options.control.lineFollower.speedMps = number;
      else if (arg == "--kp") options.control.lineFollower.kp = number;
      else if (arg == "--threshold") options.control.lineFollower.darkThreshold = number;
      else if (arg == "--timeout") options.control.lineFollower.cameraTimeoutS = number;
      else throw std::runtime_error("Unknown option: " + arg);
    }
    std::string error;
    if (!options.missionPath.empty() && options.mapPath.empty())
      throw std::runtime_error("--mission requires --map.");
    if (!options.missionPath.empty() && !options.goal.empty())
      throw std::runtime_error("Use either --mission or --goal, not both.");
    if (options.robotId.empty()) throw std::runtime_error("--robot cannot be empty.");
    if (!options.goal.empty() && options.mapPath.empty())
      throw std::runtime_error("Use --map and --goal together for A* routing.");
    if ((!options.mapPath.empty() || !options.missionPath.empty()) && options.junctionTurn != TurnRequest::None)
      throw std::runtime_error("Use either A* routing or --turn, not both.");
    if (!validateControlConfig(options.control, error)) throw std::runtime_error(error);
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return false;
  }
  return true;
}

}  // namespace amr

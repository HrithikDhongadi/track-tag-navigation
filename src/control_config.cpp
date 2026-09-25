#include "amr/control_config.hpp"

#include <opencv2/core.hpp>
#include <opencv2/core/persistence.hpp>

#include <cmath>
#include <sstream>

namespace amr {
namespace {

bool readDouble(const cv::FileNode &object, const char *key, double &value,
                std::string &error) {
  const cv::FileNode node = object[key];
  if (node.empty()) return true;
  if (!node.isReal() && !node.isInt()) {
    error = std::string("Expected a number for ") + key;
    return false;
  }
  node >> value;
  return std::isfinite(value);
}

bool readInt(const cv::FileNode &object, const char *key, int &value,
             std::string &error) {
  const cv::FileNode node = object[key];
  if (node.empty()) return true;
  if (!node.isInt()) {
    error = std::string("Expected an integer for ") + key;
    return false;
  }
  node >> value;
  return true;
}

bool mapOrEmpty(const cv::FileNode &node, const char *name, std::string &error) {
  if (node.empty() || node.isMap()) return true;
  error = std::string("Expected object for ") + name;
  return false;
}

}  // namespace

bool validateControlConfig(const ControlConfig &config, std::string &error) {
  const auto &line = config.lineFollower;
  const auto &junction = config.junction;
  const auto &mission = config.mission;
  if (config.version != 1) error = "Unsupported config version (expected 1).";
  else if (!(line.speedMps > 0.0 && line.speedMps <= .3))
    error = "line_follower.speed_mps must be in (0, 0.3].";
  else if (!(line.kp > 0.0 && line.kp <= 5.0))
    error = "line_follower.kp must be in (0, 5].";
  else if (!(line.darkThreshold > 0.0 && line.darkThreshold < 255.0))
    error = "line_follower.dark_threshold must be in (0, 255).";
  else if (!(line.cameraTimeoutS > 0.0 && line.cameraTimeoutS <= 60.0))
    error = "line_follower.camera_timeout_s must be in (0, 60].";
  else if (junction.minimumActiveSensors < 1 || junction.minimumActiveSensors > 5)
    error = "junction.minimum_active_sensors must be in [1, 5].";
  else if (!(junction.activeSensorDarkFraction > 0.0 && junction.activeSensorDarkFraction <= 1.0))
    error = "junction.active_sensor_dark_fraction must be in (0, 1].";
  else if (!(junction.minimumTotalDark > 0.0 && junction.minimumTotalDark <= 5.0))
    error = "junction.minimum_total_dark must be in (0, 5].";
  else if (!(junction.turnSpeedFactor > 0.0 && junction.turnSpeedFactor <= 1.0) ||
           !(junction.reacquireSpeedFactor > 0.0 && junction.reacquireSpeedFactor <= 1.0))
    error = "junction speed factors must be in (0, 1].";
  else if (junction.leftBiasRadps < 0.0 || junction.leftBiasRadps > junction.maxTurnRadps ||
           junction.rightBiasRadps < 0.0 || junction.rightBiasRadps > junction.maxTurnRadps ||
           junction.maxTurnRadps <= 0.0 || junction.maxTurnRadps > .8 ||
           junction.straightTurnLimitRadps < 0.0 || junction.straightTurnLimitRadps > junction.maxTurnRadps)
    error = "junction turn limits and biases must be in [0, 0.8].";
  else if (junction.commitDurationS <= 0.0 || junction.reacquireStableS < 0.0 ||
           junction.cooldownS < 0.0)
    error = "junction timings must be non-negative; commit_duration_s must be positive.";
  else if (!(mission.routeTimeoutS > 0.0 && mission.checkpointTimeoutS > 0.0 &&
             mission.qrCameraTimeoutS > 0.0 && mission.checkpointTimeoutS <= mission.routeTimeoutS))
    error = "mission timeouts must be positive; checkpoint_timeout_s cannot exceed route_timeout_s.";
  else
    return true;
  return false;
}

bool loadControlConfig(const std::string &path, ControlConfig &config, std::string &error) {
  try {
    cv::FileStorage input(path, cv::FileStorage::READ | cv::FileStorage::FORMAT_JSON);
    if (!input.isOpened()) {
      error = "Cannot open control config: " + path;
      return false;
    }
    const cv::FileNode root = input.root();
    if (!root.isMap()) {
      error = "Control config root must be a JSON object.";
      return false;
    }
    if (!readInt(root, "version", config.version, error)) return false;

    const cv::FileNode line = root["line_follower"];
    const cv::FileNode junction = root["junction"];
    const cv::FileNode mission = root["mission"];
    if (!mapOrEmpty(line, "line_follower", error) || !mapOrEmpty(junction, "junction", error) ||
        !mapOrEmpty(mission, "mission", error))
      return false;
    if (!line.empty() &&
        (!readDouble(line, "speed_mps", config.lineFollower.speedMps, error) ||
         !readDouble(line, "kp", config.lineFollower.kp, error) ||
         !readDouble(line, "dark_threshold", config.lineFollower.darkThreshold, error) ||
         !readDouble(line, "camera_timeout_s", config.lineFollower.cameraTimeoutS, error)))
      return false;
    if (!junction.empty() &&
        (!readInt(junction, "minimum_active_sensors", config.junction.minimumActiveSensors, error) ||
         !readDouble(junction, "active_sensor_dark_fraction", config.junction.activeSensorDarkFraction, error) ||
         !readDouble(junction, "minimum_total_dark", config.junction.minimumTotalDark, error) ||
         !readDouble(junction, "turn_speed_factor", config.junction.turnSpeedFactor, error) ||
         !readDouble(junction, "left_bias_radps", config.junction.leftBiasRadps, error) ||
         !readDouble(junction, "right_bias_radps", config.junction.rightBiasRadps, error) ||
         !readDouble(junction, "max_turn_radps", config.junction.maxTurnRadps, error) ||
         !readDouble(junction, "straight_turn_limit_radps", config.junction.straightTurnLimitRadps, error) ||
         !readDouble(junction, "commit_duration_s", config.junction.commitDurationS, error) ||
         !readDouble(junction, "reacquire_speed_factor", config.junction.reacquireSpeedFactor, error) ||
         !readDouble(junction, "reacquire_stable_s", config.junction.reacquireStableS, error) ||
         !readDouble(junction, "cooldown_s", config.junction.cooldownS, error)))
      return false;
    if (!mission.empty() &&
        (!readDouble(mission, "route_timeout_s", config.mission.routeTimeoutS, error) ||
         !readDouble(mission, "checkpoint_timeout_s", config.mission.checkpointTimeoutS, error) ||
         !readDouble(mission, "qr_camera_timeout_s", config.mission.qrCameraTimeoutS, error)))
      return false;
    return validateControlConfig(config, error);
  } catch (const cv::Exception &exception) {
    error = std::string("Invalid control config: ") + exception.what();
    return false;
  }
}

}  // namespace amr

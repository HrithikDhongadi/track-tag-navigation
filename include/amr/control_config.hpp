#pragma once

#include <string>

namespace amr {

struct LineFollowerConfig {
  double speedMps = .10;
  double kp = .45;
  double darkThreshold = 110;
  double cameraTimeoutS = 2.0;
};

struct JunctionConfig {
  int minimumActiveSensors = 4;
  double activeSensorDarkFraction = .10;
  double minimumTotalDark = 3.0;
  double turnSpeedFactor = .55;
  double leftBiasRadps = .45;
  double rightBiasRadps = .45;
  double maxTurnRadps = .8;
  double straightTurnLimitRadps = .20;
  double commitDurationS = .70;
  double reacquireSpeedFactor = .65;
  double reacquireStableS = .15;
  double cooldownS = .80;
};

struct ControlConfig {
  int version = 1;
  LineFollowerConfig lineFollower;
  JunctionConfig junction;
};

// Loads only fields present in a version-1 JSON profile. Missing fields retain
// their safe defaults. Returns false with a useful message on any invalid file.
bool loadControlConfig(const std::string &path, ControlConfig &config, std::string &error);
bool validateControlConfig(const ControlConfig &config, std::string &error);

}  // namespace amr

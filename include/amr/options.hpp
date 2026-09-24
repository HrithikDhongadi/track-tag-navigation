#pragma once

#include "amr/control_config.hpp"
#include "amr/junction_controller.hpp"

#include <string>

namespace amr {

struct Options {
  ControlConfig control;
  std::string configPath;
  std::string mapPath;
  std::string goal;
  std::string start;
  std::string missionPath;
  std::string robotId = "amr_1";
  TurnRequest junctionTurn = TurnRequest::None;
  bool monitor = false;
  bool view = false;
  bool frontView = false;
};

// Loads --config first, then applies all command-line overrides.
// Returns false after printing a usage or validation error.
bool parseOptions(int argc, char **argv, Options &options);

}  // namespace amr

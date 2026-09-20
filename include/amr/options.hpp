#pragma once

#include <string>

namespace amr {

struct Options {
  double speed = .10;
  double kp = .45;
  double threshold = 110;
  double timeout = 2.0;
  bool monitor = false;
  bool view = false;
};

// Returns false after printing a usage or validation error.
bool parseOptions(int argc, char **argv, Options &options);

}  // namespace amr

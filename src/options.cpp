#include "amr/options.hpp"

#include <cmath>
#include <iostream>
#include <stdexcept>

namespace amr {

bool parseOptions(int argc, char **argv, Options &options) {
  try {
    for (int i = 1; i < argc; ++i) {
      const std::string arg = argv[i];
      if (arg == "--monitor") { options.monitor = true; continue; }
      if (arg == "--view") { options.view = true; continue; }
      if (arg == "--help") {
        std::cout << "track_tag_navigation_controller [--view] [--monitor] [--speed 0.10] [--kp 0.45] "
                     "[--threshold 110] [--timeout 2.0]\n";
        return false;
      }
      if (i + 1 >= argc) throw std::runtime_error("Missing value for " + arg);
      const std::string value = argv[++i];
      size_t used = 0;
      const double number = std::stod(value, &used);
      if (used != value.size() || !std::isfinite(number))
        throw std::runtime_error("Invalid value");
      if (arg == "--speed") options.speed = number;
      else if (arg == "--kp") options.kp = number;
      else if (arg == "--threshold") options.threshold = number;
      else if (arg == "--timeout") options.timeout = number;
      else throw std::runtime_error("Unknown option: " + arg);
    }
    if (options.speed <= 0 || options.speed > .3 || options.kp <= 0 ||
        options.threshold <= 0 || options.threshold >= 255 || options.timeout <= 0)
      throw std::runtime_error("Use speed (0,0.3], positive kp/timeout, threshold (0,255).");
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return false;
  }
  return true;
}

}  // namespace amr

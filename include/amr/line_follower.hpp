#pragma once

#include "amr/options.hpp"

#include <atomic>

namespace amr {

int runLineFollower(const Options &options, std::atomic_bool &running);

}  // namespace amr

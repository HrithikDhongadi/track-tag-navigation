#pragma once

#include "amr/options.hpp"

#include <atomic>
#include <memory>
#include "amr/mission_executor.hpp"

namespace amr {

class RouteManager;
int runLineFollower(const Options &options, std::atomic_bool &running,
                    std::shared_ptr<RouteManager> routes = {},
                    std::shared_ptr<MissionExecutor> mission = {});

}  // namespace amr

#pragma once

#include <atomic>
#include <memory>
#include "amr/mission_executor.hpp"

namespace amr {

class RouteManager;
int runQrReader(bool view, bool frontView, double cameraTimeoutS, std::atomic_bool &running,
                std::shared_ptr<RouteManager> routes = {},
                std::shared_ptr<MissionExecutor> mission = {});

}  // namespace amr

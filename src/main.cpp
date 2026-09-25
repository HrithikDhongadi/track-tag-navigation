#include "amr/line_follower.hpp"
#include "amr/logger.hpp"
#include "amr/mission_executor.hpp"
#include "amr/navigation.hpp"
#include "amr/options.hpp"
#include "amr/qr_reader.hpp"
#include "amr/route_manager.hpp"
#include "amr/runtime.hpp"

#include <atomic>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

int main(int argc, char **argv) {
  amr::Options options;
  if (!amr::parseOptions(argc, argv, options)) {
    for (int i = 1; i < argc; ++i) if (std::string(argv[i]) == "--help") return 0;
    return 1;
  }
  amr::Logger::instance().initialize(amr::LogChannel::Controller);
  amr::Logger::instance().core("TrackTag Navigation process starting");
  amr::Logger::instance().info("TrackTag Navigation controller starting");

  std::shared_ptr<amr::NavigationGraph> graph;
  std::shared_ptr<amr::RouteManager> routes;
  std::shared_ptr<amr::MissionExecutor> mission;
  if (!options.mapPath.empty()) {
    graph = std::make_shared<amr::NavigationGraph>();
    std::string error;
    if (!graph->load(options.mapPath, error)) { amr::Logger::instance().error(error); return 1; }
    const std::string start = options.start.empty() ? graph->defaultStart() : options.start;
    if (start.empty() || !graph->hasNode(start)) { amr::Logger::instance().error("Route start must be a graph checkpoint ID."); return 1; }
    if (!options.missionPath.empty() || options.goal.empty()) {
      routes = std::make_shared<amr::RouteManager>(graph, start, "");
      mission = std::make_shared<amr::MissionExecutor>(graph, routes, options.robotId,
                                                        options.control.mission);
      if (!options.missionPath.empty()) {
        if (!mission->load(options.missionPath, error)) { amr::Logger::instance().error(error); return 1; }
        amr::Logger::instance().info("MISSION: ", options.missionPath, " robot=", options.robotId,
                                     " start=", start, "; use the UI Start command.");
      } else {
        amr::Logger::instance().info("MISSION EDITOR: map=", options.mapPath, " robot=", options.robotId,
                                     "; create a mission in the UI.");
      }
    } else {
      if (!graph->hasNode(options.goal)) { amr::Logger::instance().error("Route goal must be a graph checkpoint ID."); return 1; }
      routes = std::make_shared<amr::RouteManager>(graph, start, options.goal);
      const auto route = routes->snapshot();
      amr::Logger::instance().info("ROUTE: start=", start, " goal=", options.goal, "; ", route.event);
    }
  }

  amr::installSignalHandlers();
  std::atomic_bool running = true;
  int lineStatus = 0, qrStatus = 0;
  std::thread qrThread([&] {
    qrStatus = amr::runQrReader(options.view, options.frontView,
                                options.control.mission.qrCameraTimeoutS, running, routes, mission);
    if (qrStatus) running.store(false, std::memory_order_relaxed);
  });
  std::thread lineThread([&] {
    lineStatus = amr::runLineFollower(options, running, routes, mission);
    if (lineStatus) running.store(false, std::memory_order_relaxed);
  });
  qrThread.join();
  lineThread.join();
  amr::Logger::instance().core("TrackTag Navigation process stopped; line_status=", lineStatus,
                               " qr_status=", qrStatus);
  return lineStatus || qrStatus ? 1 : 0;
}

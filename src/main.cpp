#include "amr/line_follower.hpp"
#include "amr/mission_executor.hpp"
#include "amr/navigation.hpp"
#include "amr/options.hpp"
#include "amr/qr_reader.hpp"
#include "amr/route_manager.hpp"
#include "amr/runtime.hpp"

#include <atomic>
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

  std::shared_ptr<amr::NavigationGraph> graph;
  std::shared_ptr<amr::RouteManager> routes;
  std::shared_ptr<amr::MissionExecutor> mission;
  if (!options.mapPath.empty()) {
    graph = std::make_shared<amr::NavigationGraph>();
    std::string error;
    if (!graph->load(options.mapPath, error)) { std::cerr << error << '\n'; return 1; }
    const std::string start = options.start.empty() ? graph->defaultStart() : options.start;
    if (start.empty() || !graph->hasNode(start)) { std::cerr << "Route start must be a graph checkpoint ID.\n"; return 1; }
    if (!options.missionPath.empty() || options.goal.empty()) {
      routes = std::make_shared<amr::RouteManager>(graph, start, "");
      mission = std::make_shared<amr::MissionExecutor>(graph, routes, options.robotId);
      if (!options.missionPath.empty()) {
        if (!mission->load(options.missionPath, error)) { std::cerr << error << '\n'; return 1; }
        std::cout << "MISSION: " << options.missionPath << " robot=" << options.robotId
                  << " start=" << start << "; use the UI Start command.\n";
      } else {
        std::cout << "MISSION EDITOR: map=" << options.mapPath << " robot=" << options.robotId
                  << "; create a mission in the UI.\n";
      }
    } else {
      if (!graph->hasNode(options.goal)) { std::cerr << "Route goal must be a graph checkpoint ID.\n"; return 1; }
      routes = std::make_shared<amr::RouteManager>(graph, start, options.goal);
      const auto route = routes->snapshot();
      std::cout << "ROUTE: start=" << start << " goal=" << options.goal << "; " << route.event << '\n';
    }
  }

  amr::installSignalHandlers();
  std::atomic_bool running = true;
  int lineStatus = 0, qrStatus = 0;
  std::thread qrThread([&] {
    qrStatus = amr::runQrReader(options.view, options.frontView, running, routes, mission);
    if (qrStatus) running.store(false, std::memory_order_relaxed);
  });
  std::thread lineThread([&] {
    lineStatus = amr::runLineFollower(options, running, routes, mission);
    if (lineStatus) running.store(false, std::memory_order_relaxed);
  });
  qrThread.join();
  lineThread.join();
  return lineStatus || qrStatus ? 1 : 0;
}

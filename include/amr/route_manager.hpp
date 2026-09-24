#pragma once

#include "amr/junction_controller.hpp"
#include "amr/navigation.hpp"

#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace amr {

struct RouteSnapshot {
  std::string goal;
  std::string current;
  std::string next;
  std::string armed;
  std::string event;
  bool active = false;
  bool goalReached = false;
};

class RouteManager {
 public:
  RouteManager(std::shared_ptr<NavigationGraph> graph, std::string start,
               std::string goal);

  void onCheckpoint(const std::string &id);
  void setRoute(std::string start, std::string goal);
  std::optional<TurnRequest> takeArmedTurn();
  void onTurnCompleted();
  RouteSnapshot snapshot() const;

 private:
  static TurnRequest toTurnRequest(RouteManeuver maneuver);
  void planLocked();

  std::shared_ptr<NavigationGraph> graph_;
  mutable std::mutex mutex_;
  std::string goal_;
  std::string current_;
  std::vector<NavigationEdge> route_;
  std::optional<TurnRequest> armedTurn_;
  bool turnDelivered_ = false;
  std::string event_;
};

}  // namespace amr

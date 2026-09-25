#include "amr/route_manager.hpp"
#include "amr/logger.hpp"

#include <sstream>

namespace amr {

RouteManager::RouteManager(std::shared_ptr<NavigationGraph> graph, std::string start,
                           std::string goal)
    : graph_(std::move(graph)), goal_(std::move(goal)), current_(std::move(start)) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!graph_ || goal_.empty() || current_.empty()) event_ = "route inactive";
  else planLocked();
}

TurnRequest RouteManager::toTurnRequest(RouteManeuver maneuver) {
  switch (maneuver) {
    case RouteManeuver::Left: return TurnRequest::Left;
    case RouteManeuver::Right: return TurnRequest::Right;
    case RouteManeuver::Straight: return TurnRequest::Straight;
    case RouteManeuver::Follow: return TurnRequest::None;
  }
  return TurnRequest::None;
}

void RouteManager::planLocked() {
  if (!graph_ || current_.empty() || goal_.empty()) { route_.clear(); armedTurn_.reset(); turnDelivered_ = false; event_ = "route inactive"; return; }
  route_ = graph_->aStar(current_, goal_, heading_);
  armedTurn_.reset(); turnDelivered_ = false;
  if (current_ == goal_) { event_ = "goal reached: " + goal_; Logger::instance().navigation(event_); return; }
  if (route_.empty()) { event_ = "no route from " + current_ + " to " + goal_; Logger::instance().warning(event_); return; }
  const auto &next = route_.front();
  const TurnRequest turn = toTurnRequest(next.maneuver);
  if (turn != TurnRequest::None) armedTurn_ = turn;
  event_ = "next " + next.from + " -> " + next.to + " (" + routeManeuverName(next.maneuver) + ")";
  Logger::instance().navigation(event_, " heading=", travelHeadingName(heading_));
}

void RouteManager::setRoute(std::string start, std::string goal) {
  std::lock_guard<std::mutex> lock(mutex_);
  current_ = std::move(start);
  goal_ = std::move(goal);
  route_.clear();
  armedTurn_.reset();
  turnDelivered_ = false;
  recoveryRequired_ = false;
  if (!graph_ || current_.empty() || goal_.empty()) { event_ = "route idle"; return; }
  planLocked();
}

bool RouteManager::onCheckpoint(const std::string &id) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!graph_ || !graph_->hasNode(id)) {
    event_ = "unmapped QR: " + id;
    recoveryRequired_ = !goal_.empty();
    Logger::instance().warning(event_);
    return false;
  }

  const bool active = !goal_.empty() && !current_.empty();
  if (active) {
    // A repeat read of the current marker is valid but must not re-arm a turn.
    if (id == current_) return true;
    if (route_.empty() || id != route_.front().to) {
      event_ = "unexpected or skipped QR: " + id +
               (route_.empty() ? " (no expected next checkpoint)" : "; expected " + route_.front().to);
      recoveryRequired_ = true;
      Logger::instance().warning(event_);
      return false;
    }
  }
  if (!current_.empty()) heading_ = graph_->headingBetween(current_, id);
  current_ = id;
  planLocked();
  return true;
}

std::optional<TurnRequest> RouteManager::takeArmedTurn() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!armedTurn_ || turnDelivered_) return std::nullopt;
  turnDelivered_ = true;
  return armedTurn_;
}

void RouteManager::onTurnCompleted() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (armedTurn_) event_ += "; turn completed";
  armedTurn_.reset(); turnDelivered_ = false;
}

RouteSnapshot RouteManager::snapshot() const {
  std::lock_guard<std::mutex> lock(mutex_);
  RouteSnapshot snapshot; snapshot.goal = goal_; snapshot.current = current_; snapshot.event = event_;
  snapshot.heading = travelHeadingName(heading_);
  snapshot.active = graph_ && !goal_.empty() && !current_.empty();
  snapshot.goalReached = snapshot.active && current_ == goal_;
  snapshot.recoveryRequired = recoveryRequired_;
  if (!route_.empty()) snapshot.next = route_.front().to;
  if (armedTurn_) snapshot.armed = *armedTurn_ == TurnRequest::Left ? "left" : *armedTurn_ == TurnRequest::Right ? "right" : "straight";
  return snapshot;
}

}  // namespace amr

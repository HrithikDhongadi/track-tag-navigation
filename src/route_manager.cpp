#include "amr/route_manager.hpp"

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
  route_ = graph_->aStar(current_, goal_);
  armedTurn_.reset(); turnDelivered_ = false;
  if (current_ == goal_) { event_ = "goal reached: " + goal_; return; }
  if (route_.empty()) { event_ = "no route from " + current_ + " to " + goal_; return; }
  const auto &next = route_.front();
  const TurnRequest turn = toTurnRequest(next.maneuver);
  if (turn != TurnRequest::None) armedTurn_ = turn;
  event_ = "next " + next.from + " -> " + next.to + " (" + routeManeuverName(next.maneuver) + ")";
}

void RouteManager::setRoute(std::string start, std::string goal) {
  std::lock_guard<std::mutex> lock(mutex_);
  current_ = std::move(start);
  goal_ = std::move(goal);
  route_.clear();
  armedTurn_.reset();
  turnDelivered_ = false;
  if (!graph_ || current_.empty() || goal_.empty()) { event_ = "route idle"; return; }
  planLocked();
}

void RouteManager::onCheckpoint(const std::string &id) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!graph_ || !graph_->hasNode(id)) { event_ = "unmapped QR: " + id; return; }
  current_ = id;
  planLocked();
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
  snapshot.active = graph_ && !goal_.empty() && !current_.empty();
  snapshot.goalReached = snapshot.active && current_ == goal_;
  if (!route_.empty()) snapshot.next = route_.front().to;
  if (armedTurn_) snapshot.armed = *armedTurn_ == TurnRequest::Left ? "left" : *armedTurn_ == TurnRequest::Right ? "right" : "straight";
  return snapshot;
}

}  // namespace amr

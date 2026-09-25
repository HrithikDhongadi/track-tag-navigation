#include "amr/navigation.hpp"

#include <opencv2/core.hpp>
#include <opencv2/core/persistence.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <queue>
#include <unordered_set>

namespace amr {
namespace {

RouteManeuver parseManeuver(const cv::FileNode &node, std::string &error) {
  if (node.empty()) return RouteManeuver::Follow;
  if (!node.isMap()) { error = "navigation maneuver must be an object."; return RouteManeuver::Follow; }
  std::string type; node["type"] >> type;
  if (type == "follow") return RouteManeuver::Follow;
  if (type != "junction_turn") { error = "Unsupported navigation maneuver: " + type; return RouteManeuver::Follow; }
  std::string direction; node["direction"] >> direction;
  if (direction == "left") return RouteManeuver::Left;
  if (direction == "right") return RouteManeuver::Right;
  if (direction == "straight") return RouteManeuver::Straight;
  error = "Invalid junction turn direction: " + direction;
  return RouteManeuver::Follow;
}

}  // namespace

const char *routeManeuverName(RouteManeuver maneuver) noexcept {
  switch (maneuver) {
    case RouteManeuver::Follow: return "follow";
    case RouteManeuver::Left: return "left";
    case RouteManeuver::Right: return "right";
    case RouteManeuver::Straight: return "straight";
  }
  return "unknown";
}

const char *travelHeadingName(TravelHeading heading) noexcept {
  switch (heading) {
    case TravelHeading::North: return "north";
    case TravelHeading::East: return "east";
    case TravelHeading::South: return "south";
    case TravelHeading::West: return "west";
    case TravelHeading::Unknown: return "unknown";
  }
  return "unknown";
}

bool NavigationGraph::load(const std::string &path, std::string &error) {
  try {
    cv::FileStorage input(path, cv::FileStorage::READ | cv::FileStorage::FORMAT_JSON);
    if (!input.isOpened()) { error = "Cannot open navigation map: " + path; return false; }
    const cv::FileNode navigation = input["navigation"];
    if (navigation.empty() || !navigation.isMap()) { error = "Map has no navigation object."; return false; }
    int version = 1; if (!navigation["schema_version"].empty()) navigation["schema_version"] >> version;
    if (version != 1) { error = "Unsupported navigation schema version."; return false; }
    std::string start; navigation["default_start"] >> start;
    const cv::FileNode edges = navigation["edges"];
    if (!edges.isSeq()) { error = "navigation.edges must be an array."; return false; }
    std::unordered_map<std::string, std::vector<NavigationEdge>> outgoing;
    std::unordered_set<std::string> edgeIds, pairs, nodes;
    std::unordered_map<std::string, std::pair<double, double>> positions;
    const cv::FileNode checkpoints = input["checkpoints"];
    if (checkpoints.isSeq()) {
      for (const auto &checkpoint : checkpoints) {
        std::string id; double x = 0, y = 0;
        checkpoint["id"] >> id; checkpoint["x"] >> x; checkpoint["y"] >> y;
        if (id.empty() || !std::isfinite(x) || !std::isfinite(y) || !positions.emplace(id, std::make_pair(x, y)).second) {
          error = "Invalid or duplicate checkpoint position: " + id; return false;
        }
      }
    }
    for (const auto &item : edges) {
      NavigationEdge edge; item["id"] >> edge.id; item["from"] >> edge.from; item["to"] >> edge.to; item["cost_m"] >> edge.costM;
      edge.maneuver = parseManeuver(item["maneuver"], error);
      if (!error.empty()) return false;
      const std::string pair = edge.from + '\n' + edge.to;
      if (edge.id.empty() || edge.from.empty() || edge.to.empty() || edge.from == edge.to ||
          !std::isfinite(edge.costM) || edge.costM <= 0.0 || !edgeIds.insert(edge.id).second ||
          !pairs.insert(pair).second) { error = "Invalid or duplicate navigation edge: " + edge.id; return false; }
      nodes.insert(edge.from); nodes.insert(edge.to); outgoing[edge.from].push_back(edge);
    }
    if (outgoing.empty()) { error = "navigation.edges is empty."; return false; }
    if (!start.empty() && nodes.find(start) == nodes.end()) { error = "navigation.default_start is not a graph node."; return false; }
    defaultStart_ = std::move(start); outgoing_ = std::move(outgoing); positions_ = std::move(positions); return true;
  } catch (const cv::Exception &exception) { error = std::string("Invalid navigation map: ") + exception.what(); return false; }
}

bool NavigationGraph::hasNode(const std::string &id) const {
  if (outgoing_.find(id) != outgoing_.end()) return true;
  for (const auto &[_, edges] : outgoing_) for (const auto &edge : edges) if (edge.to == id) return true;
  return false;
}

TravelHeading NavigationGraph::headingBetween(const std::string &from, const std::string &to) const {
  const auto source = positions_.find(from), destination = positions_.find(to);
  if (source == positions_.end() || destination == positions_.end()) return TravelHeading::Unknown;
  const double dx = destination->second.first - source->second.first;
  const double dy = destination->second.second - source->second.second;
  if (dx == 0.0 && dy == 0.0) return TravelHeading::Unknown;
  if (std::abs(dx) >= std::abs(dy)) return dx >= 0.0 ? TravelHeading::East : TravelHeading::West;
  return dy >= 0.0 ? TravelHeading::North : TravelHeading::South;
}

std::vector<NavigationEdge> NavigationGraph::aStar(const std::string &start,
                                                    const std::string &goal,
                                                    TravelHeading arrivalHeading) const {
  struct QueueItem { double cost; std::string state; bool operator>(const QueueItem &other) const { return cost > other.cost; } };
  if (!hasNode(start) || !hasNode(goal)) return {};
  const auto stateKey = [](const std::string &node, TravelHeading heading) {
    return node + '\n' + std::to_string(static_cast<int>(heading));
  };
  const auto opposite = [](TravelHeading first, TravelHeading second) {
    return (first == TravelHeading::North && second == TravelHeading::South) ||
           (first == TravelHeading::South && second == TravelHeading::North) ||
           (first == TravelHeading::East && second == TravelHeading::West) ||
           (first == TravelHeading::West && second == TravelHeading::East);
  };
  std::priority_queue<QueueItem, std::vector<QueueItem>, std::greater<QueueItem>> open;
  std::unordered_map<std::string, double> distance;
  struct Previous { NavigationEdge edge; std::string state; };
  std::unordered_map<std::string, Previous> previous;
  const std::string initial = stateKey(start, arrivalHeading);
  distance[initial] = 0.0; open.push({0.0, initial});
  std::string goalState;
  while (!open.empty()) {
    const auto current = open.top(); open.pop();
    if (current.cost != distance[current.state]) continue;
    const auto split = current.state.rfind('\n');
    const std::string node = current.state.substr(0, split);
    const auto heading = static_cast<TravelHeading>(std::stoi(current.state.substr(split + 1)));
    if (node == goal) { goalState = current.state; break; }
    const auto found = outgoing_.find(node); if (found == outgoing_.end()) continue;
    for (const auto &edge : found->second) {
      const TravelHeading nextHeading = headingBetween(node, edge.to);
      if (heading != TravelHeading::Unknown && nextHeading != TravelHeading::Unknown && opposite(heading, nextHeading)) continue;
      const double candidate = current.cost + edge.costM;
      const std::string nextState = stateKey(edge.to, nextHeading);
      const auto old = distance.find(nextState);
      if (old == distance.end() || candidate < old->second) {
        distance[nextState] = candidate; previous[nextState] = {edge, current.state}; open.push({candidate, nextState});
      }
    }
  }
  if (goalState.empty()) return {};
  std::vector<NavigationEdge> route;
  for (std::string state = goalState; state != initial;) {
    const auto found = previous.find(state); if (found == previous.end()) return {};
    route.push_back(found->second.edge);
    state = found->second.state;
  }
  std::reverse(route.begin(), route.end()); return route;
}

}  // namespace amr

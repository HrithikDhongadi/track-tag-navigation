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
    defaultStart_ = std::move(start); outgoing_ = std::move(outgoing); return true;
  } catch (const cv::Exception &exception) { error = std::string("Invalid navigation map: ") + exception.what(); return false; }
}

bool NavigationGraph::hasNode(const std::string &id) const {
  if (outgoing_.find(id) != outgoing_.end()) return true;
  for (const auto &[_, edges] : outgoing_) for (const auto &edge : edges) if (edge.to == id) return true;
  return false;
}

std::vector<NavigationEdge> NavigationGraph::aStar(const std::string &start,
                                                    const std::string &goal) const {
  struct QueueItem { double cost; std::string node; bool operator>(const QueueItem &other) const { return cost > other.cost; } };
  if (!hasNode(start) || !hasNode(goal)) return {};
  std::priority_queue<QueueItem, std::vector<QueueItem>, std::greater<QueueItem>> open;
  std::unordered_map<std::string, double> distance;
  std::unordered_map<std::string, NavigationEdge> previous;
  distance[start] = 0.0; open.push({0.0, start});
  while (!open.empty()) {
    const auto current = open.top(); open.pop();
    if (current.cost != distance[current.node]) continue;
    if (current.node == goal) break;
    const auto found = outgoing_.find(current.node); if (found == outgoing_.end()) continue;
    for (const auto &edge : found->second) {
      const double candidate = current.cost + edge.costM;
      const auto old = distance.find(edge.to);
      if (old == distance.end() || candidate < old->second) { distance[edge.to] = candidate; previous[edge.to] = edge; open.push({candidate, edge.to}); }
    }
  }
  if (distance.find(goal) == distance.end()) return {};
  std::vector<NavigationEdge> route;
  for (std::string node = goal; node != start;) { const auto found = previous.find(node); if (found == previous.end()) return {}; route.push_back(found->second); node = found->second.from; }
  std::reverse(route.begin(), route.end()); return route;
}

}  // namespace amr

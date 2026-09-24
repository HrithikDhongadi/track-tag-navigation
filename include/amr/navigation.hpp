#pragma once

#include <string>
#include <unordered_map>
#include <vector>

namespace amr {

enum class RouteManeuver { Follow, Left, Right, Straight };

struct NavigationEdge {
  std::string id;
  std::string from;
  std::string to;
  double costM = 0.0;
  RouteManeuver maneuver = RouteManeuver::Follow;
};

class NavigationGraph {
 public:
  bool load(const std::string &path, std::string &error);
  bool hasNode(const std::string &id) const;
  const std::string &defaultStart() const noexcept { return defaultStart_; }
  std::vector<NavigationEdge> aStar(const std::string &start,
                                    const std::string &goal) const;

 private:
  std::string defaultStart_;
  std::unordered_map<std::string, std::vector<NavigationEdge>> outgoing_;
};

const char *routeManeuverName(RouteManeuver maneuver) noexcept;

}  // namespace amr

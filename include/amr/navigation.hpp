#pragma once

#include <string>
#include <unordered_map>
#include <vector>

namespace amr {

enum class RouteManeuver { Follow, Left, Right, Straight };
enum class TravelHeading { Unknown, North, East, South, West };

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
                                    const std::string &goal,
                                    TravelHeading arrivalHeading = TravelHeading::Unknown) const;
  TravelHeading headingBetween(const std::string &from, const std::string &to) const;

 private:
  std::string defaultStart_;
  std::unordered_map<std::string, std::vector<NavigationEdge>> outgoing_;
  std::unordered_map<std::string, std::pair<double, double>> positions_;
};

const char *routeManeuverName(RouteManeuver maneuver) noexcept;
const char *travelHeadingName(TravelHeading heading) noexcept;

}  // namespace amr

#include "amr/control_config.hpp"
#include "amr/junction_controller.hpp"
#include "amr/mission_executor.hpp"
#include "amr/json_message.hpp"
#include "amr/navigation.hpp"
#include "amr/route_manager.hpp"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace {

void require(bool condition, const std::string &message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    std::exit(EXIT_FAILURE);
  }
}

std::shared_ptr<amr::NavigationGraph> loadGraph() {
  auto graph = std::make_shared<amr::NavigationGraph>();
  std::string error;
  require(graph->load(std::string(TRACK_TAG_NAVIGATION_SOURCE_DIR) + "/maps/junction_track.json", error),
          "load junction map: " + error);
  return graph;
}

struct MissionHarness {
  std::shared_ptr<amr::NavigationGraph> graph = loadGraph();
  std::shared_ptr<amr::RouteManager> routes =
      std::make_shared<amr::RouteManager>(graph, graph->defaultStart(), "");
  amr::MissionExecutor mission;

  explicit MissionHarness(amr::MissionConfig config = {})
      : mission(graph, routes, "test_amr", config) {}
};

void advanceToGoal(MissionHarness &harness) {
  for (int guard = 0; guard < 32 && harness.mission.snapshot().state == "navigating"; ++guard) {
    const auto route = harness.routes->snapshot();
    require(!route.next.empty(), "navigating mission has an expected next QR");
    require(harness.routes->onCheckpoint(route.next), "expected route QR is accepted");
    harness.mission.onCheckpoint(route.next);
  }
  require(harness.mission.snapshot().state != "navigating", "mission reaches a terminal task state");
}

void advanceUntilCheckpoint(MissionHarness &harness, const std::string &checkpoint) {
  for (int guard = 0; guard < 32 && harness.routes->snapshot().current != checkpoint; ++guard) {
    const auto route = harness.routes->snapshot();
    require(!route.next.empty(), "route has an expected next QR before checkpoint");
    require(harness.routes->onCheckpoint(route.next), "expected route QR is accepted");
    harness.mission.onCheckpoint(route.next);
  }
  require(harness.routes->snapshot().current == checkpoint, "route reached requested checkpoint");
}

void testShortestPath(const amr::NavigationGraph &graph) {
  const auto route = graph.aStar("J1A", "Station B");
  const std::vector<std::string> expected{"J1A", "J1B", "Station B"};
  require(route.size() == expected.size() - 1, "shortest route has two edges");
  for (std::size_t index = 0; index < route.size(); ++index) {
    require(route[index].from == expected[index] && route[index].to == expected[index + 1],
            "shortest route uses the lower-cost J1B branch");
  }
}

void testOutOfSequenceQrIsRejected(const std::shared_ptr<amr::NavigationGraph> &graph) {
  amr::RouteManager routes(graph, "Station A", "Station B");
  const auto before = routes.snapshot();
  require(!routes.onCheckpoint("Station C"), "out-of-sequence QR is rejected");
  const auto after = routes.snapshot();
  require(after.current == before.current && after.next == before.next && after.armed == before.armed,
          "rejected QR leaves navigation state unchanged");
  require(after.event.find("unexpected or skipped QR") != std::string::npos,
          "rejected QR is reported to route telemetry");
  require(after.recoveryRequired, "unexpected QR requires immediate route recovery");
}

void testLineAndJunctionControl() {
  const Command centered = steer({0.0, 0.0, 1.0, 0.0, 0.0}, .1, .45);
  require(centered.line && centered.speed == .1 && centered.turn == 0.0,
          "centered line produces straight nominal-speed command");
  const Command left = steer({1.0, 0.0, 0.0, 0.0, 0.0}, .1, .45);
  require(left.line && left.speed < .1 && left.turn > 0.0,
          "left line slows and turns left");
  require(!steer({0.0, 0.0, 0.0, 0.0, 0.0}, .1, .45).line,
          "lost line produces a stop command");

  amr::JunctionController junction(amr::TurnRequest::Left, amr::JunctionConfig{});
  const std::array<double, 5> broad{.8, .8, .8, .8, .8};
  const Command line{.1, 0.0, true};
  const Command commit = junction.update(broad, line, .1, .01);
  require(std::string(junction.stateName()) == "commit" && commit.turn > 0.0 && commit.speed < .1,
          "junction enters left-biased commit state");
  junction.update(broad, line, .1, .70);
  require(std::string(junction.stateName()) == "reacquire", "junction enters reacquisition state");
  junction.update({0.0, 0.0, 1.0, 0.0, 0.0}, line, .1, .15);
  require(std::string(junction.stateName()) == "follow" && junction.takeCompleted(),
          "junction reacquires the line and reports completion");
}

void testMalformedJsonIsRejected() {
  const auto path = std::filesystem::temp_directory_path() / "track_tag_navigation_invalid.json";
  { std::ofstream output(path); output << "{ invalid JSON"; }
  amr::NavigationGraph graph;
  std::string error;
  require(!graph.load(path.string(), error), "malformed navigation JSON is rejected");
  amr::ControlConfig config;
  require(!amr::loadControlConfig(path.string(), config, error), "malformed control JSON is rejected");
  std::error_code ignored;
  std::filesystem::remove(path, ignored);
}

void testExpectedQrIsAccepted(const std::shared_ptr<amr::NavigationGraph> &graph) {
  amr::RouteManager routes(graph, "Station A", "Station B");
  const auto before = routes.snapshot();
  require(!before.next.empty(), "route has an expected next QR");
  require(routes.onCheckpoint(before.next), "expected QR is accepted");
  const auto after = routes.snapshot();
  require(after.current == before.next, "accepted QR advances current checkpoint");
}

void testHeadingAwareRoute() {
  const auto graph = loadGraph();
  amr::RouteManager routes(graph, "J1B", "Station B");
  require(routes.onCheckpoint("Station B"), "southbound arrival at Station B is accepted");
  require(routes.snapshot().heading == "south", "arrival heading is recorded from checkpoint geometry");
  routes.setRoute("Station B", "Station A");
  require(routes.snapshot().next == "J2B",
          "southbound Station B departure avoids an implicit U-turn and routes to J2B");
}

void testJunctionTurnIsArmed(const std::shared_ptr<amr::NavigationGraph> &graph) {
  amr::RouteManager routes(graph, "J1A", "Station B");
  require(routes.snapshot().armed == "left", "junction route arms its left turn");
  const auto turn = routes.takeArmedTurn();
  require(turn && *turn == amr::TurnRequest::Left, "junction emits the planned left turn");
  require(routes.onCheckpoint("J1B"), "junction exit QR is accepted");
  require(routes.snapshot().next == "Station B", "junction exit continues toward the goal");
}

void testMissionCommands() {
  MissionHarness harness;
  require(harness.mission.executeCommand("create:Station B,Station C"), "mission is created");
  require(harness.mission.executeCommand("start"), "mission starts");
  require(harness.mission.snapshot().state == "navigating" && harness.mission.motionEnabled(),
          "started mission enables motion");
  harness.mission.executeCommand("pause");
  require(harness.mission.snapshot().state == "paused" && !harness.mission.motionEnabled(),
          "pause stops motion");
  harness.mission.executeCommand("resume");
  require(harness.mission.snapshot().state == "navigating", "resume restarts navigation");
  harness.mission.executeCommand("cancel");
  require(harness.mission.snapshot().state == "cancelled" && !harness.mission.motionEnabled(),
          "cancel stops the active task");
  harness.mission.executeCommand("retry");
  require(harness.mission.snapshot().state == "navigating", "retry dispatches the cancelled task");

  advanceToGoal(harness);
  require(harness.mission.snapshot().state == "arrived", "first task arrives");
  harness.mission.executeCommand("next");
  require(harness.mission.snapshot().state == "navigating", "next dispatches the following task");
  advanceToGoal(harness);
  harness.mission.executeCommand("next");
  require(harness.mission.snapshot().state == "completed" && !harness.mission.motionEnabled(),
          "all tasks complete with motion stopped");
}

void testAutoAdvanceMission() {
  const auto path = std::filesystem::temp_directory_path() / "track_tag_navigation_auto_mission.json";
  {
    std::ofstream output(path);
    output << R"({"version":1,"auto_advance":true,"tasks":[
      {"id":"one","type":"navigate","to":"Station B"},
      {"id":"two","type":"navigate","to":"Station C"}
    ]})";
  }
  MissionHarness harness;
  std::string error;
  require(harness.mission.load(path.string(), error), "auto-advance mission loads: " + error);
  require(harness.mission.executeCommand("start"), "auto-advance mission starts");
  advanceUntilCheckpoint(harness, "Station B");
  require(harness.mission.snapshot().state == "navigating", "arrival auto-dispatches the next task");
  advanceUntilCheckpoint(harness, "Station C");
  require(harness.mission.snapshot().state == "completed", "last auto-advanced task completes mission");
  std::error_code ignored;
  std::filesystem::remove(path, ignored);
}

void testMissionTimeouts() {
  {
    amr::MissionConfig config;
    config.routeTimeoutS = .2;
    config.checkpointTimeoutS = .001;
    MissionHarness harness(config);
    harness.mission.executeCommand("create:Station B");
    harness.mission.executeCommand("start");
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    harness.mission.checkTimeouts();
    const auto timedOut = harness.mission.snapshot();
    require(timedOut.state == "failed" && !timedOut.motionEnabled,
            "checkpoint-progress timeout fails the mission and stops motion");
    require(timedOut.event.find("checkpoint progress timeout") != std::string::npos,
            "checkpoint timeout reports its cause");
  }
  {
    amr::MissionConfig config;
    config.routeTimeoutS = .001;
    config.checkpointTimeoutS = .2;
    MissionHarness harness(config);
    harness.mission.executeCommand("create:Station B");
    harness.mission.executeCommand("start");
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    harness.mission.checkTimeouts();
    const auto timedOut = harness.mission.snapshot();
    require(timedOut.state == "failed" && !timedOut.motionEnabled,
            "route timeout fails the mission and stops motion");
    require(timedOut.event.find("route timeout") != std::string::npos,
            "route timeout reports its cause");
  }
}

void testQrCameraRecoveryRequired() {
  MissionHarness harness;
  harness.mission.executeCommand("create:Station B");
  harness.mission.executeCommand("start");
  harness.mission.onQrCameraTimeout();
  const auto state = harness.mission.snapshot();
  require(state.state == "recovery_required" && !state.motionEnabled,
          "QR camera timeout stops motion and requires recovery");
  require(state.event.find("QR camera") != std::string::npos,
          "QR camera recovery event explains the failure");
  harness.mission.executeCommand("retry");
  require(harness.mission.snapshot().state == "navigating", "recovery-required task can be retried");
}

void testRouteDeviationRecoveryRequired() {
  MissionHarness harness;
  harness.mission.executeCommand("create:Station B");
  harness.mission.executeCommand("start");
  harness.mission.onRouteDeviation("unexpected or skipped QR: J2B; expected J1B");
  const auto state = harness.mission.snapshot();
  require(state.state == "recovery_required" && !state.motionEnabled,
          "route deviation stops motion and requires recovery");
  require(state.event.find("J2B") != std::string::npos,
          "route recovery keeps the offending checkpoint in diagnostics");
}

void testJsonTransportFields() {
  const std::string encoded = amr::escapeJson("Station \"A\"\nready");
  const std::string message = "{\"schema_version\":1,\"state\":\"tracking\",\"event\":\"" + encoded + "\"}";
  require(amr::jsonStringField(message, "state").value_or("") == "tracking",
          "JSON transport state is readable");
  require(amr::jsonStringField(message, "event").value_or("") == "Station \"A\"\nready",
          "JSON transport strings round-trip escaped characters");
  require(!amr::jsonStringField(message, "missing"), "missing JSON transport field is absent");
}

}  // namespace

int main() {
  const auto graph = loadGraph();
  testShortestPath(*graph);
  testOutOfSequenceQrIsRejected(graph);
  testExpectedQrIsAccepted(graph);
  testHeadingAwareRoute();
  testJunctionTurnIsArmed(graph);
  testLineAndJunctionControl();
  testMalformedJsonIsRejected();
  testMissionCommands();
  testAutoAdvanceMission();
  testMissionTimeouts();
  testQrCameraRecoveryRequired();
  testRouteDeviationRecoveryRequired();
  testJsonTransportFields();
  return EXIT_SUCCESS;
}

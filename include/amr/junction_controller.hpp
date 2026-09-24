#pragma once

#include "amr_line_follower/control.hpp"
#include "amr/control_config.hpp"

#include <algorithm>
#include <array>

namespace amr {

enum class TurnRequest { None, Left, Right, Straight };

// Executes a selected branch through an otherwise ambiguous line junction.
// The selection is deliberately independent of its source: a CLI flag is used
// today; a QR-derived route decision can supply the same TurnRequest later.
class JunctionController {
 public:
  JunctionController(TurnRequest request, JunctionConfig config)
      : request_(request), config_(config) {}

  Command update(const std::array<double, 5> &dark, const Command &lineCommand,
                 double nominalSpeed, double seconds) {
    if (request_ == TurnRequest::None) return lineCommand;

    cooldown_ = std::max(0.0, cooldown_ - seconds);
    switch (state_) {
      case State::Follow:
        if (cooldown_ == 0.0 && isJunction(dark)) {
          state_ = State::Commit;
          elapsed_ = 0.0;
        }
        return state_ == State::Commit ? commit(lineCommand, nominalSpeed, seconds)
                                       : lineCommand;
      case State::Commit:
        return commit(lineCommand, nominalSpeed, seconds);
      case State::Reacquire:
        return reacquire(lineCommand, nominalSpeed, seconds);
    }
    return {};
  }

  void reset() {
    state_ = State::Follow;
    elapsed_ = 0.0;
    stable_ = 0.0;
    cooldown_ = 0.0;
  }

  const char *stateName() const {
    switch (state_) {
      case State::Follow: return "follow";
      case State::Commit: return "commit";
      case State::Reacquire: return "reacquire";
    }
    return "unknown";
  }

 private:
  enum class State { Follow, Commit, Reacquire };

  bool isJunction(const std::array<double, 5> &dark) {
    int active = 0;
    double totalDark = 0.0;
    for (const double value : dark) {
      active += value >= config_.activeSensorDarkFraction;
      totalDark += value;
    }
    // Reject edge-only readings from a single wide line. A true junction must
    // cover four sensors and have substantially more black area overall.
    return active >= config_.minimumActiveSensors &&
           totalDark >= config_.minimumTotalDark;
  }

  Command commit(const Command &lineCommand, double nominalSpeed, double seconds) {
    elapsed_ += seconds;
    Command command;
    command.line = true;
    command.speed = nominalSpeed * config_.turnSpeedFactor;
    if (request_ == TurnRequest::Left)
      command.turn = std::clamp(lineCommand.turn + config_.leftBiasRadps, -config_.maxTurnRadps, config_.maxTurnRadps);
    else if (request_ == TurnRequest::Right)
      command.turn = std::clamp(lineCommand.turn - config_.rightBiasRadps, -config_.maxTurnRadps, config_.maxTurnRadps);
    else
      command.turn = lineCommand.line ? std::clamp(lineCommand.turn, -config_.straightTurnLimitRadps, config_.straightTurnLimitRadps) : 0.0;

    if (elapsed_ >= config_.commitDurationS) {
      state_ = State::Reacquire;
      elapsed_ = 0.0;
      stable_ = 0.0;
    }
    return command;
  }

  Command reacquire(const Command &lineCommand,
                    double nominalSpeed, double seconds) {
    if (lineCommand.line) {
      // The exit can still overlap the junction under the sensor strip. The
      // commit timer and cooldown prevent a new branch decision at this point.
      stable_ += seconds;
      Command command = lineCommand;
      command.speed = std::min(command.speed, nominalSpeed * config_.reacquireSpeedFactor);
      if (stable_ >= config_.reacquireStableS) {
        state_ = State::Follow;
        cooldown_ = config_.cooldownS;
        stable_ = 0.0;
      }
      return command;
    }
    stable_ = 0.0;
    return {};  // Preserve the tested safety behaviour when no exit is found.
  }

  TurnRequest request_;
  JunctionConfig config_;
  State state_ = State::Follow;
  double elapsed_ = 0.0;
  double stable_ = 0.0;
  double cooldown_ = 0.0;
};

}  // namespace amr

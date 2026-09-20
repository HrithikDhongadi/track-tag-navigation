#include "amr/runtime.hpp"

#include <csignal>

namespace amr {
namespace {
volatile std::sig_atomic_t stopRequested = 0;
void stopSignal(int) { stopRequested = 1; }
}  // namespace

void installSignalHandlers() {
  std::signal(SIGINT, stopSignal);
  std::signal(SIGTERM, stopSignal);
}

bool isRunning(const std::atomic_bool &running) {
  return !stopRequested && running.load(std::memory_order_relaxed);
}

}  // namespace amr

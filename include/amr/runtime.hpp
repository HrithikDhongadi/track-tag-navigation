#pragma once

#include <atomic>

namespace amr {

// A process-wide SIGINT/SIGTERM request, kept separate from thread state so
// the signal handler only writes a sig_atomic_t-compatible value.
void installSignalHandlers();
bool isRunning(const std::atomic_bool &running);

}  // namespace amr

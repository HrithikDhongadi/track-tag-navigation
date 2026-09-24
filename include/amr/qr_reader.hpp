#pragma once

#include <atomic>

namespace amr {

int runQrReader(bool view, bool frontView, std::atomic_bool &running);

}  // namespace amr

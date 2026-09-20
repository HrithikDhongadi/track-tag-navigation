#pragma once

#include <atomic>

namespace amr {

int runQrReader(bool view, std::atomic_bool &running);

}  // namespace amr

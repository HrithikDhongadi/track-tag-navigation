#pragma once

#include "amr/ui/telemetry_store.hpp"

#include <string>

namespace amr::ui {
std::string ageLabel(std::chrono::steady_clock::time_point received);
void drawImagePanel(const char *title, const ImageFrame &frame, unsigned int texture, const char *topic);
void drawKeyValue(const char *key, const std::string &value);
}

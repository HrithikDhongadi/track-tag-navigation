#pragma once

#include <imgui.h>
#include <string>

namespace amr::ui {

enum class StatusTone { Neutral, Ok, Warning, Danger };

void applyTheme();
void drawStatusPill(const char *label, StatusTone tone);
void drawMetric(const char *label, const std::string &value,
                StatusTone tone = StatusTone::Neutral);

}

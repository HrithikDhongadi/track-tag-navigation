#pragma once

#include "crawler_sensor_console/ui/render.hpp"

namespace crawler_sensor_console {

bool BeginAbsoluteImGuiWidget(const void* id, const UiRect& bounds,
                              bool draw_background = false) noexcept;
void EndAbsoluteImGuiWidget() noexcept;

} // namespace crawler_sensor_console

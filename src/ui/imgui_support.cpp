#include "crawler_sensor_console/ui/imgui_support.hpp"

#include <algorithm>
#include <cstdio>

#include <imgui.h>

namespace crawler_sensor_console {

bool BeginAbsoluteImGuiWidget(const void* id, const UiRect& bounds,
                              bool draw_background) noexcept {
  if (bounds.width <= 0.0 || bounds.height <= 0.0 ||
      ImGui::GetCurrentContext() == nullptr) {
    return false;
  }
  const ImVec2 scale = ImGui::GetIO().DisplayFramebufferScale;
  const float sx = std::max(scale.x, 1.0F);
  const float sy = std::max(scale.y, 1.0F);
  ImGui::SetNextWindowPos(
      {static_cast<float>(bounds.x) / sx, static_cast<float>(bounds.y) / sy});
  ImGui::SetNextWindowSize({static_cast<float>(bounds.width) / sx,
                            static_cast<float>(bounds.height) / sy});
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0.0F, 0.0F});
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0F);
  char name[48]{};
  std::snprintf(name, sizeof(name), "##absolute_%p", id);
  ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration |
                           ImGuiWindowFlags_NoMove |
                           ImGuiWindowFlags_NoSavedSettings |
                           ImGuiWindowFlags_NoNavFocus |
                           ImGuiWindowFlags_NoBringToFrontOnFocus;
  if (!draw_background) flags |= ImGuiWindowFlags_NoBackground;
  ImGui::Begin(name, nullptr, flags);
  return true;
}

void EndAbsoluteImGuiWidget() noexcept {
  ImGui::End();
  ImGui::PopStyleVar(2);
}

} // namespace crawler_sensor_console

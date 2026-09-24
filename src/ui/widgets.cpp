#include "amr/ui/widgets.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>

#include <imgui.h>

namespace amr::ui {

std::string ageLabel(std::chrono::steady_clock::time_point received) {
  if (received.time_since_epoch().count() == 0) return "not received";
  const double seconds = std::chrono::duration<double>(
      std::chrono::steady_clock::now() - received).count();
  char text[32]{};
  std::snprintf(text, sizeof(text), "%.1f s ago", std::max(0.0, seconds));
  return text;
}

void drawKeyValue(const char *key, const std::string &value) {
  ImGui::TextDisabled("%s", key);
  ImGui::SameLine(245.0f);
  ImGui::TextWrapped("%s", value.empty() ? "—" : value.c_str());
}

void drawImagePanel(const char *title, const ImageFrame &frame,
                    unsigned int texture, const char *topic) {
  ImGui::BeginChild(title, {0, 0}, true);
  ImGui::TextUnformatted(title);
  ImGui::SameLine();
  ImGui::TextDisabled("%s", topic);
  ImGui::SameLine();
  ImGui::TextDisabled("%s", ageLabel(frame.received).c_str());
  ImGui::Separator();
  if (!texture || frame.width <= 0 || frame.height <= 0) {
    ImGui::Spacing();
    ImGui::TextDisabled("Waiting for frames from %s", topic);
    ImGui::EndChild();
    return;
  }
  const ImVec2 available = ImGui::GetContentRegionAvail();
  const float aspect = static_cast<float>(frame.width) / frame.height;
  float width = available.x;
  float height = width / aspect;
  if (height > available.y) { height = available.y; width = height * aspect; }
  ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (available.x - width) * 0.5f);
  ImGui::Image(reinterpret_cast<void *>(static_cast<intptr_t>(texture)), {width, height});
  ImGui::EndChild();
}

}

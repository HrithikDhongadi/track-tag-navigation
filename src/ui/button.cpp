#include "crawler_sensor_console/ui/button.hpp"

#include <algorithm>
#include <utility>

#include <imgui.h>

#include "crawler_sensor_console/ui/imgui_support.hpp"

namespace crawler_sensor_console {
namespace {

struct Palette final {
  ImVec4 normal;
  ImVec4 hovered;
  ImVec4 active;
  ImVec4 border;
};

Palette Colors(UiButton::Tone tone) {
  switch (tone) {
    case UiButton::Tone::Primary:
      return {{0.06F,0.36F,0.44F,1},{0.08F,0.48F,0.56F,1},{0.05F,0.29F,0.36F,1},{0.28F,0.74F,0.82F,1}};
    case UiButton::Tone::Success:
      return {{0.07F,0.38F,0.24F,1},{0.10F,0.50F,0.32F,1},{0.05F,0.30F,0.19F,1},{0.32F,0.76F,0.50F,1}};
    case UiButton::Tone::Warning:
      return {{0.48F,0.31F,0.06F,1},{0.62F,0.42F,0.08F,1},{0.38F,0.24F,0.04F,1},{0.94F,0.68F,0.24F,1}};
    case UiButton::Tone::Danger:
      return {{0.46F,0.12F,0.15F,1},{0.60F,0.18F,0.21F,1},{0.36F,0.08F,0.11F,1},{0.94F,0.40F,0.44F,1}};
    case UiButton::Tone::Neutral:
    default:
      return {{0.16F,0.19F,0.22F,1},{0.23F,0.28F,0.32F,1},{0.12F,0.15F,0.17F,1},{0.48F,0.57F,0.62F,1}};
  }
}

} // namespace

UiButton::UiButton(std::string label, ClickHandler on_click)
    : label_(std::move(label)), on_click_(std::move(on_click)) {}

void UiButton::SetLabel(std::string label) { label_ = std::move(label); }
void UiButton::SetBounds(double x,double y,double width,double height) noexcept { x_px_=x;y_px_=y;width_px_=width;height_px_=height; }
void UiButton::SetCornerRadius(double radius) noexcept { corner_radius_px_=std::max(radius,0.0); }
void UiButton::SetBold(bool bold) noexcept { bold_=bold; }
void UiButton::SetTone(Tone tone) noexcept { tone_=tone; }
bool UiButton::HitTest(double,double) const noexcept { return false; }
void UiButton::UpdatePointer(double,double) noexcept {}
bool UiButton::HandleLeftMouse(bool,double,double) noexcept { return false; }

void UiButton::Draw(FreeTypeTextDriver&) const noexcept {
  if (!BeginAbsoluteImGuiWidget(this, {x_px_,y_px_,width_px_,height_px_})) return;
  const Palette colors = Colors(tone_);
  ImGui::PushStyleColor(ImGuiCol_Button, colors.normal);
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, colors.hovered);
  ImGui::PushStyleColor(ImGuiCol_ButtonActive, colors.active);
  ImGui::PushStyleColor(ImGuiCol_Border, colors.border);
  ImGui::PushStyleVar(
      ImGuiStyleVar_FrameRounding,
      static_cast<float>(std::max(corner_radius_px_, 10.0)));
  ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 4.0F);
  ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, {0.5F, 0.42F});
  ImGui::PushID(this);
  constexpr float kOuterInsetPx = 3.0F;
  ImGui::SetCursorPos({kOuterInsetPx, kOuterInsetPx});
  ImVec2 button_size = ImGui::GetContentRegionAvail();
  button_size.x = std::max(1.0F, button_size.x - kOuterInsetPx);
  button_size.y = std::max(1.0F, button_size.y - kOuterInsetPx);
  if (ImGui::Button(label_.c_str(), button_size) && on_click_) {
    on_click_();
  }
  if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
  ImGui::PopID();
  ImGui::PopStyleVar(3);
  ImGui::PopStyleColor(4);
  EndAbsoluteImGuiWidget();
}

} // namespace crawler_sensor_console

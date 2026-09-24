#include "crawler_sensor_console/ui/text_box.hpp"

#include <algorithm>
#include <cstring>
#include <utility>
#include <vector>

#include <imgui.h>

#include "crawler_sensor_console/ui/imgui_support.hpp"

namespace crawler_sensor_console {

UiTextBox::UiTextBox(std::string text, std::string label)
    : text_(std::move(text)), label_(std::move(label)) {}

void UiTextBox::SetBounds(double x,double y,double width,double height) noexcept { x_px_=x;y_px_=y;width_px_=width;height_px_=height; }
void UiTextBox::SetText(std::string text) { text_=std::move(text); }
void UiTextBox::SetLabel(std::string label) { label_=std::move(label); }
void UiTextBox::SetInputMode(InputMode mode,std::size_t max_length) noexcept { input_mode_=mode;maximum_length_=std::max<std::size_t>(1U,max_length); }
const std::string& UiTextBox::text() const noexcept { return text_; }
bool UiTextBox::focused() const noexcept { return focused_; }
bool UiTextBox::HitTest(double,double) const noexcept { return false; }
void UiTextBox::UpdatePointer(double,double) noexcept {}
bool UiTextBox::HandleLeftMouse(bool,double,double) noexcept { return false; }
bool UiTextBox::HandleChar(unsigned int) { return false; }
bool UiTextBox::HandleKey(int,int) { return false; }

void UiTextBox::Draw(FreeTypeTextDriver&) const noexcept {
  if (!BeginAbsoluteImGuiWidget(this,{x_px_,y_px_,width_px_,height_px_})) return;
  ImGui::PushID(this);
  const bool compact = height_px_ < 52.0;
  if (!compact) {
    ImGui::PushStyleColor(ImGuiCol_Text,{0.64F,0.78F,0.86F,1.0F});
    ImGui::TextUnformatted(label_.c_str());
    ImGui::PopStyleColor();
  }
  std::vector<char> buffer(maximum_length_ + 1U, '\0');
  std::memcpy(buffer.data(),text_.data(),std::min(text_.size(),maximum_length_));
  ImGuiInputTextFlags flags = ImGuiInputTextFlags_AutoSelectAll;
  if (input_mode_==InputMode::Number) flags |= ImGuiInputTextFlags_CharsScientific;
  ImGui::SetNextItemWidth(-1.0F);
  const bool changed = compact
      ? ImGui::InputTextWithHint("##value",label_.c_str(),buffer.data(),buffer.size(),flags)
      : ImGui::InputText("##value",buffer.data(),buffer.size(),flags);
  if (changed) text_.assign(buffer.data());
  focused_=ImGui::IsItemActive();
  if (ImGui::IsItemHovered() && compact) ImGui::SetTooltip("%s",label_.c_str());
  ImGui::PopID();
  EndAbsoluteImGuiWidget();
}

} // namespace crawler_sensor_console

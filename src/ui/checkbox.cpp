#include "crawler_sensor_console/ui/checkbox.hpp"

#include <algorithm>
#include <utility>

#include <imgui.h>

#include "crawler_sensor_console/ui/imgui_support.hpp"

namespace crawler_sensor_console {

UiCheckbox::UiCheckbox(std::string label,bool checked):label_(std::move(label)),checked_(checked){}
void UiCheckbox::SetBounds(double x,double y,double width,double height) noexcept{x_=x;y_=y;width_=width;height_=height;}
bool UiCheckbox::HitTest(double,double) const noexcept{return false;}
void UiCheckbox::UpdatePointer(double,double) noexcept{}
bool UiCheckbox::HandleLeftMouse(bool,double,double) noexcept{return false;}
void UiCheckbox::Draw(FreeTypeTextDriver&) noexcept{
  if(!BeginAbsoluteImGuiWidget(this,{x_,y_,width_,height_}))return;
  ImGui::PushID(this);
  ImGui::SetCursorPosY(std::max(0.0F,(ImGui::GetContentRegionAvail().y-ImGui::GetFrameHeight())*0.5F));
  ImGui::Checkbox(label_.c_str(),&checked_);
  ImGui::PopID();
  EndAbsoluteImGuiWidget();
}

} // namespace crawler_sensor_console

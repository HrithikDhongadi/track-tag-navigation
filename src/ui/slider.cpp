#include "crawler_sensor_console/ui/slider.hpp"

#include <algorithm>
#include <utility>

#include <imgui.h>

#include "crawler_sensor_console/ui/imgui_support.hpp"

namespace crawler_sensor_console {

UiSlider::UiSlider(std::string label,ChangeHandler handler):label_(std::move(label)),on_change_(std::move(handler)){}
void UiSlider::SetLabel(std::string label){label_=std::move(label);}
void UiSlider::SetBounds(double x,double y,double width,double height) noexcept{x_px_=x;y_px_=y;width_px_=width;height_px_=height;}
void UiSlider::SetRange(double minimum,double maximum) noexcept{minimum_=minimum;maximum_=std::max(maximum,minimum+1e-9);value_=std::clamp(value_,minimum_,maximum_);}
void UiSlider::SetValue(double value) noexcept{value_=std::clamp(value,minimum_,maximum_);}
void UiSlider::SetEnabled(bool enabled) noexcept{enabled_=enabled;}
bool UiSlider::HitTest(double,double) const noexcept{return false;}
void UiSlider::UpdatePointer(double,double) noexcept{}
bool UiSlider::HandleLeftMouse(bool,double,double) noexcept{return false;}
bool UiSlider::dragging() const noexcept{return dragging_;}
void UiSlider::ApplyPointer(double) noexcept{}
void UiSlider::Draw(FreeTypeTextDriver&) noexcept{
  if(!BeginAbsoluteImGuiWidget(this,{x_px_,y_px_,width_px_,height_px_}))return;
  ImGui::PushID(this);
  if(!enabled_)ImGui::BeginDisabled();
  float value=static_cast<float>(value_);
  ImGui::SetNextItemWidth(-1.0F);
  if(ImGui::SliderFloat(label_.c_str(),&value,static_cast<float>(minimum_),static_cast<float>(maximum_),"%.3f mm")){
    value_=value;
    if(on_change_)on_change_(value_);
  }
  dragging_=ImGui::IsItemActive();
  if(!enabled_)ImGui::EndDisabled();
  ImGui::PopID();
  EndAbsoluteImGuiWidget();
}

} // namespace crawler_sensor_console

#include "crawler_sensor_console/ui/combo_box.hpp"

#include <algorithm>
#include <utility>

#include <imgui.h>

#include "crawler_sensor_console/ui/imgui_support.hpp"

namespace crawler_sensor_console {

UiComboBox::UiComboBox(std::string label,std::vector<std::string> options):label_(std::move(label)),options_(std::move(options)){}
void UiComboBox::SetBounds(double x,double y,double width,double height) noexcept{x_=x;y_=y;width_=width;height_=height;}
bool UiComboBox::HitTest(double,double) const noexcept{return false;}
void UiComboBox::SetValue(const std::string& value) noexcept{const auto it=std::find(options_.begin(),options_.end(),value);if(it!=options_.end())selected_=static_cast<std::size_t>(it-options_.begin());}
const std::string& UiComboBox::value() const noexcept{static const std::string empty;return options_.empty()?empty:options_[selected_];}
void UiComboBox::UpdatePointer(double,double) noexcept{}
bool UiComboBox::HandleLeftMouse(bool,double,double) noexcept{return false;}
void UiComboBox::Draw(FreeTypeTextDriver&) noexcept{
  if(!BeginAbsoluteImGuiWidget(this,{x_,y_,width_,height_}))return;
  ImGui::PushID(this);
  ImGui::PushStyleColor(ImGuiCol_Text,{0.64F,0.78F,0.86F,1.0F});
  ImGui::TextUnformatted(label_.c_str());
  ImGui::PopStyleColor();
  ImGui::SetNextItemWidth(-1.0F);
  if(ImGui::BeginCombo("##combo",value().c_str())){
    for(std::size_t i=0;i<options_.size();++i){
      const bool selected=i==selected_;
      if(ImGui::Selectable(options_[i].c_str(),selected))selected_=i;
      if(selected)ImGui::SetItemDefaultFocus();
    }
    ImGui::EndCombo();
  }
  ImGui::PopID();
  EndAbsoluteImGuiWidget();
}

} // namespace crawler_sensor_console

#include "crawler_sensor_console/ui/tab_bar.hpp"

#include <algorithm>
#include <utility>

#include <imgui.h>

#include "crawler_sensor_console/ui/imgui_support.hpp"

namespace crawler_sensor_console {

void UiTabBar::SetLabels(std::vector<std::string> labels){labels_=std::move(labels);if(!labels_.empty())active_index_=std::clamp(active_index_,0,static_cast<int>(labels_.size()-1U));}
void UiTabBar::SetBounds(double x,double y,double width,double height) noexcept{x_px_=x;y_px_=y;width_px_=width;height_px_=height;}
void UiTabBar::SetActiveIndex(int index) noexcept{active_index_=labels_.empty()?0:std::clamp(index,0,static_cast<int>(labels_.size()-1U));}
bool UiTabBar::HitTest(double x,double y,int* index) const noexcept{if(index==nullptr||labels_.empty()||width_px_<=0.0||height_px_<=0.0||x<x_px_||x>x_px_+width_px_||y<y_px_||y>y_px_+height_px_)return false;const double tab_width=width_px_/labels_.size();*index=std::clamp(static_cast<int>((x-x_px_)/tab_width),0,static_cast<int>(labels_.size()-1U));return true;}
void UiTabBar::Draw(FreeTypeTextDriver&) const noexcept{
  if(labels_.empty()||!BeginAbsoluteImGuiWidget(this,{x_px_,y_px_,width_px_,height_px_},true))return;
  ImGui::PushID(this);
  const float gap=4.0F;
  const float tab_width=(ImGui::GetContentRegionAvail().x-gap*static_cast<float>(labels_.size()-1U))/static_cast<float>(labels_.size());
  for(std::size_t i=0;i<labels_.size();++i){
    if(i!=0)ImGui::SameLine(0.0F,gap);
    const bool active=static_cast<int>(i)==active_index_;
    if(active){
      ImGui::PushStyleColor(ImGuiCol_Button,{0.06F,0.38F,0.46F,1.0F});
      ImGui::PushStyleColor(ImGuiCol_ButtonHovered,{0.08F,0.48F,0.56F,1.0F});
    }
    ImGui::Button(labels_[i].c_str(),{tab_width,-1.0F});
    if(active)ImGui::PopStyleColor(2);
  }
  ImGui::PopID();
  EndAbsoluteImGuiWidget();
}
void UiTabBar::DrawIcon(int,double,double) const noexcept{}

} // namespace crawler_sensor_console

#include "crawler_sensor_console/ui/popup.hpp"

#include <algorithm>
#include <utility>

#include <GLFW/glfw3.h>
#include <imgui.h>

#include "crawler_sensor_console/ui/imgui_support.hpp"

namespace crawler_sensor_console {
namespace { constexpr double kHeaderHeightPx=52.0; constexpr double kPaddingPx=22.0; }

UiPopup::UiPopup(std::string title):title_(std::move(title)),close_button_("X",[this]{Close();}){}
void UiPopup::Open() noexcept{open_=true;ClampToViewport();UpdateLayout();}
void UiPopup::Close() noexcept{open_=false;dragging_=false;}
bool UiPopup::is_open() const noexcept{return open_;}
void UiPopup::SetBounds(double x,double y,double width,double height) noexcept{
  if(!positioned_){
    bounds_.x=x;
    bounds_.y=y;
    positioned_=true;
  }
  bounds_.width=std::max(width,0.0);
  bounds_.height=std::max(height,0.0);
  ClampToViewport();
  UpdateLayout();
}
UiRect UiPopup::content_bounds() const noexcept{return{bounds_.x+kPaddingPx,bounds_.y+kHeaderHeightPx+12.0,std::max(bounds_.width-kPaddingPx*2.0,0.0),std::max(bounds_.height-kHeaderHeightPx-kPaddingPx-12.0,0.0)};}
bool UiPopup::HeaderHitTest(double x,double y) const noexcept{
  return x>=bounds_.x&&x<=bounds_.x+bounds_.width&&
      y>=bounds_.y&&y<=bounds_.y+kHeaderHeightPx&&
      !(x>=close_bounds_.x&&x<=close_bounds_.x+close_bounds_.width&&
        y>=close_bounds_.y&&y<=close_bounds_.y+close_bounds_.height);
}
bool UiPopup::PanelHitTest(double x,double y) const noexcept{return x>=bounds_.x&&x<=bounds_.x+bounds_.width&&y>=bounds_.y&&y<=bounds_.y+bounds_.height;}
void UiPopup::ClampToViewport() noexcept{
  if(viewport_width_px_ <= 0 || viewport_height_px_ <= 0){
    return;
  }
  const double min_visible_width = std::min(bounds_.width, 120.0);
  const double min_visible_height = kHeaderHeightPx;
  const double min_x = min_visible_width - bounds_.width;
  const double max_x = std::max(0.0, static_cast<double>(viewport_width_px_) - min_visible_width);
  const double min_y = 0.0;
  const double max_y = std::max(0.0, static_cast<double>(viewport_height_px_) - min_visible_height);
  bounds_.x = std::clamp(bounds_.x, min_x, max_x);
  bounds_.y = std::clamp(bounds_.y, min_y, max_y);
}
void UiPopup::UpdateLayout() noexcept{
  close_bounds_={bounds_.x+bounds_.width-48.0,bounds_.y+9.0,36.0,34.0};
  close_button_.SetBounds(close_bounds_.x,close_bounds_.y,close_bounds_.width,close_bounds_.height);
}
void UiPopup::UpdatePointer(double x,double y) noexcept{
  if(open_&&dragging_){
    bounds_.x=x-drag_offset_x_px_;
    bounds_.y=y-drag_offset_y_px_;
    ClampToViewport();
    UpdateLayout();
  }
  close_button_.UpdatePointer(x,y);
}
bool UiPopup::HandleLeftMouse(bool pressed,double x,double y) noexcept{
  if(!open_)return false;
  const bool close_hit=x>=close_bounds_.x&&x<=close_bounds_.x+close_bounds_.width&&
      y>=close_bounds_.y&&y<=close_bounds_.y+close_bounds_.height;
  if(!pressed){
    const bool consumed=dragging_;
    dragging_=false;
    return consumed;
  }
  if(pressed&&close_hit){
    Close();
    return true;
  }
  if(HeaderHitTest(x,y)){
    dragging_=true;
    drag_offset_x_px_=x-bounds_.x;
    drag_offset_y_px_=y-bounds_.y;
    return true;
  }
  return false;
}
bool UiPopup::HandleKey(int key,int action) noexcept{if(!open_)return false;if(key==GLFW_KEY_ESCAPE&&action==GLFW_PRESS)Close();return true;}
void UiPopup::Draw(int viewport_width_px,int viewport_height_px,FreeTypeTextDriver& text_driver) noexcept{
  if(!open_||ImGui::GetCurrentContext()==nullptr)return;
  viewport_width_px_=viewport_width_px;
  viewport_height_px_=viewport_height_px;
  ClampToViewport();
  UpdateLayout();
  const ImVec2 scale=ImGui::GetIO().DisplayFramebufferScale;
  const float sx=std::max(scale.x,1.0F),sy=std::max(scale.y,1.0F);
  const ImVec2 p0{static_cast<float>(bounds_.x)/sx,static_cast<float>(bounds_.y)/sy};
  const ImVec2 p1{static_cast<float>(bounds_.x+bounds_.width)/sx,static_cast<float>(bounds_.y+bounds_.height)/sy};
  ImDrawList* background=ImGui::GetBackgroundDrawList();
  background->AddRectFilled(p0,p1,IM_COL32(12,15,18,254),6.0F);
  background->AddRect(p0,p1,IM_COL32(42,152,166,255),6.0F,0,1.0F);
  background->AddRectFilled(p0,{p1.x,p0.y+static_cast<float>(kHeaderHeightPx)/sy},IM_COL32(22,29,34,255),6.0F);
  background->AddLine({p0.x,p0.y+static_cast<float>(kHeaderHeightPx)/sy},{p1.x,p0.y+static_cast<float>(kHeaderHeightPx)/sy},IM_COL32(45,170,185,230),1.0F);
  background->AddText({p0.x+18.0F,p0.y+16.0F},IM_COL32(236,246,248,255),title_.c_str());
  close_button_.Draw(text_driver);
}

} // namespace crawler_sensor_console

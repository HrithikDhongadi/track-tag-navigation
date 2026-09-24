#include "amr/ui/theme.hpp"

namespace amr::ui {
namespace {
ImVec4 color(StatusTone tone) {
  switch (tone) {
    case StatusTone::Ok: return {0.07f,0.38f,0.24f,1};
    case StatusTone::Warning: return {0.48f,0.31f,0.06f,1};
    case StatusTone::Danger: return {0.46f,0.12f,0.15f,1};
    default: return {0.06f,0.36f,0.44f,1};
  }
}
}
void applyTheme() {
  ImGui::StyleColorsDark(); ImGuiStyle &s=ImGui::GetStyle();
  s.WindowRounding=6; s.ChildRounding=4; s.FrameRounding=5; s.PopupRounding=5; s.ScrollbarRounding=5; s.GrabRounding=4;
  s.FramePadding={10,8}; s.ItemSpacing={10,8}; s.WindowPadding={14,12};
  s.Colors[ImGuiCol_WindowBg]={.045f,.055f,.065f,.98f}; s.Colors[ImGuiCol_ChildBg]={.055f,.065f,.080f,1};
  s.Colors[ImGuiCol_FrameBg]={.08f,.10f,.12f,1}; s.Colors[ImGuiCol_FrameBgHovered]={.12f,.25f,.29f,1}; s.Colors[ImGuiCol_FrameBgActive]={.10f,.34f,.39f,1};
  s.Colors[ImGuiCol_Header]={.08f,.32f,.38f,1}; s.Colors[ImGuiCol_HeaderHovered]={.10f,.48f,.55f,1}; s.Colors[ImGuiCol_Button]={.06f,.36f,.44f,1}; s.Colors[ImGuiCol_ButtonHovered]={.08f,.48f,.56f,1}; s.Colors[ImGuiCol_ButtonActive]={.05f,.29f,.36f,1};
  s.Colors[ImGuiCol_Border]={.28f,.74f,.82f,.58f}; s.Colors[ImGuiCol_CheckMark]={.22f,.82f,.88f,1}; s.Colors[ImGuiCol_SliderGrab]={.22f,.72f,.80f,1};
}
void drawStatusPill(const char *label, StatusTone tone) { const ImVec4 c=color(tone); ImGui::PushStyleColor(ImGuiCol_Button,c); ImGui::PushStyleColor(ImGuiCol_ButtonHovered,c); ImGui::PushStyleColor(ImGuiCol_ButtonActive,c); ImGui::PushStyleColor(ImGuiCol_Text,{1,1,1,1}); ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding,10); ImGui::Button(label); ImGui::PopStyleVar(); ImGui::PopStyleColor(4); }
void drawMetric(const char *label,const std::string &value,StatusTone tone) { ImGui::BeginGroup(); ImGui::TextDisabled("%s",label); ImGui::TextColored(color(tone),"%s",value.c_str()); ImGui::EndGroup(); }
}

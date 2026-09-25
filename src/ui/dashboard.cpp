#include "amr/ui/dashboard.hpp"

#include "amr/json_message.hpp"
#include "amr/logger.hpp"
#include "amr/ui/telemetry_store.hpp"
#include "amr/ui/theme.hpp"
#include "amr/ui/widgets.hpp"

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

namespace amr::ui {
namespace {
using Clock = std::chrono::steady_clock;
constexpr float kTabBarHeight = 44.0f;

enum class Tab { Mission, Cameras, Diagnostics, Settings };

struct Texture {
  unsigned int id = 0;
  std::uint64_t uploaded = 0;
  void upload(const ImageFrame &frame) {
    if (frame.sequence == uploaded || frame.rgb.empty()) return;
    if (!id) glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, frame.width, frame.height, 0,
                 GL_RGB, GL_UNSIGNED_BYTE, frame.rgb.data());
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    uploaded = frame.sequence;
  }
  void destroy() { if (id) glDeleteTextures(1, &id); id = 0; uploaded = 0; }
};

struct SystemMetrics {
  double cpuPercent = -1.0, ramUsedGiB = -1.0, ramTotalGiB = -1.0;
  double gpuPercent = -1.0, gpuUsedMiB = -1.0, gpuTotalMiB = -1.0;
  std::uint64_t totalTicks = 0, idleTicks = 0;
  Clock::time_point sampled{};
  void sample() {
    if (sampled.time_since_epoch().count() && Clock::now() - sampled < std::chrono::seconds(1)) return;
    sampled = Clock::now();
    std::ifstream stat("/proc/stat"); std::string label; std::uint64_t user=0,nice=0,system=0,idle=0,iowait=0,irq=0,softirq=0,steal=0;
    if (stat >> label >> user >> nice >> system >> idle >> iowait >> irq >> softirq >> steal && label == "cpu") {
      const auto total = user + nice + system + idle + iowait + irq + softirq + steal, idleNow = idle + iowait;
      if (totalTicks && total > totalTicks) cpuPercent = 100.0 * (1.0 - static_cast<double>(idleNow-idleTicks) / (total-totalTicks));
      totalTicks = total; idleTicks = idleNow;
    }
    std::ifstream mem("/proc/meminfo"); std::string key, unit; double value=0, total=0, available=0;
    while (mem >> key >> value >> unit) { if (key == "MemTotal:") total=value; else if (key == "MemAvailable:") available=value; }
    if (total > 0 && available >= 0) { ramTotalGiB=total/(1024.0*1024.0); ramUsedGiB=(total-available)/(1024.0*1024.0); }
    gpuPercent = gpuUsedMiB = gpuTotalMiB = -1.0;
    if (FILE *gpu = popen("nvidia-smi --query-gpu=utilization.gpu,memory.used,memory.total --format=csv,noheader,nounits 2>/dev/null", "r")) {
      char row[128]{};
      if (std::fgets(row, sizeof(row), gpu)) std::sscanf(row, "%lf , %lf , %lf", &gpuPercent, &gpuUsedMiB, &gpuTotalMiB);
      pclose(gpu);
    }
  }
};

std::string number(double value, const char *suffix = "") {
  if (value < 0) return "n/a";
  char text[48]{}; std::snprintf(text, sizeof(text), "%.1f%s", value, suffix); return text;
}
StatusTone freshness(Clock::time_point received, double limit = 2.0) {
  if (received.time_since_epoch().count() == 0) return StatusTone::Danger;
  return std::chrono::duration<double>(Clock::now()-received).count() < limit ? StatusTone::Ok : StatusTone::Warning;
}
StatusTone controllerTone(const StatusSnapshot &s) {
  const std::string state = jsonStringField(s.telemetry, "state").value_or("");
  if (state == "goal_reached") return StatusTone::Ok;
  if (state == "no_line" || state == "waiting") return StatusTone::Warning;
  return freshness(s.telemetryReceived);
}
const char *tabLabel(Tab tab) {
  switch (tab) {
    case Tab::Mission: return "Mission";
    case Tab::Cameras: return "Cameras";
    case Tab::Diagnostics: return "Diagnostics";
    case Tab::Settings: return "Settings";
  }
  return "Mission";
}
std::string missionField(const std::string &text, const std::string &key) {
  return jsonStringField(text, key).value_or("—");
}


class DashboardApp {
 public:
  int run() {
    Logger::instance().initialize(LogChannel::Ui);
    Logger::instance().ui("TrackTag dashboard starting");
    if (!glfwInit()) { Logger::instance().error("TrackTag UI: GLFW initialization failed"); return 1; }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3); glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    window_ = glfwCreateWindow(1440, 900, "TrackTag Navigation Console", nullptr, nullptr);
    if (!window_) { Logger::instance().error("TrackTag UI window creation failed"); glfwTerminate(); return 1; }
    glfwMakeContextCurrent(window_); glfwSwapInterval(1);
    IMGUI_CHECKVERSION(); ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO(); io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; io.IniFilename = nullptr;
    if (std::ifstream regular{"/usr/share/fonts/opentype/urw-base35/NimbusSans-Regular.otf"}) io.Fonts->AddFontFromFileTTF("/usr/share/fonts/opentype/urw-base35/NimbusSans-Regular.otf", 18.0f);
    if (std::ifstream bold{"/usr/share/fonts/opentype/urw-base35/NimbusSans-Bold.otf"}) headingFont_ = io.Fonts->AddFontFromFileTTF("/usr/share/fonts/opentype/urw-base35/NimbusSans-Bold.otf", 19.0f);
    applyTheme();
    if (!ImGui_ImplGlfw_InitForOpenGL(window_, true) || !ImGui_ImplOpenGL3_Init("#version 130")) { close(); return 1; }
    while (!glfwWindowShouldClose(window_)) { glfwPollEvents(); draw(); }
    close(); return 0;
  }
 private:
  void close() {
    frontTexture_.destroy(); qrTexture_.destroy();
    if (ImGui::GetCurrentContext()) { ImGui_ImplOpenGL3_Shutdown(); ImGui_ImplGlfw_Shutdown(); ImGui::DestroyContext(); }
    if (window_) glfwDestroyWindow(window_);
    window_ = nullptr;
    glfwTerminate();
  }
  void heading(const char *text) const { if (headingFont_) ImGui::PushFont(headingFont_); ImGui::TextUnformatted(text); if (headingFont_) ImGui::PopFont(); }
  void setTab(Tab tab) {
    tab_ = tab;
    const std::string title = std::string("TrackTag Navigation Console - ") + tabLabel(tab_);
    glfwSetWindowTitle(window_, title.c_str());
  }
  void drawTabBar() {
    ImGui::BeginChild("Top navigation", {0, kTabBarHeight}, false, ImGuiWindowFlags_NoScrollbar);
    constexpr std::array<Tab, 4> tabs{Tab::Mission, Tab::Cameras, Tab::Diagnostics, Tab::Settings};
    const float gap = 5.0f;
    const float width = (ImGui::GetContentRegionAvail().x - gap * (tabs.size()-1)) / tabs.size();
    for (std::size_t i=0; i<tabs.size(); ++i) {
      if (i) ImGui::SameLine(0, gap);
      const bool selected = tab_ == tabs[i];
      if (selected) { ImGui::PushStyleColor(ImGuiCol_Button, {0.08f,0.52f,0.63f,1}); ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.10f,0.62f,0.72f,1}); }
      if (ImGui::Button(tabLabel(tabs[i]), {width, 34})) setTab(tabs[i]);
      if (selected) ImGui::PopStyleColor(2);
    }
    ImGui::EndChild();
  }
  void drawStatusStrip(const StatusSnapshot &s) {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {8, 5});
    ImGui::BeginChild("Status strip", {0, 48}, true, ImGuiWindowFlags_NoScrollbar);
    drawStatusPill(s.telemetry.empty() ? "CONTROLLER WAITING" : "CONTROLLER", controllerTone(s)); ImGui::SameLine();
    drawStatusPill(s.checkpoint.empty() ? "QR WAITING" : "QR ACTIVE", freshness(s.checkpointReceived, 4.0)); ImGui::SameLine();
    const std::string message = jsonStringField(s.telemetry, "message").value_or(s.telemetry);
    ImGui::TextDisabled("%s", s.telemetry.empty() ? "Waiting for native Ignition telemetry on /amr/telemetry" : message.c_str());
    ImGui::EndChild();
    ImGui::PopStyleVar();
  }
  void drawToolbar(const StatusSnapshot &s) {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0, 3});
    ImGui::BeginChild("Toolbar", {0, 42}, false, ImGuiWindowFlags_NoScrollbar);
    const ImVec2 row = ImGui::GetCursorPos();
    if (!s.checkpoint.empty()) ImGui::TextDisabled("Last QR: %s", s.checkpoint.c_str());
    const float right = ImGui::GetWindowContentRegionMax().x - 240.0f;
    ImGui::SetCursorPos({std::max(0.0f, right), row.y});
    if (ImGui::Button("Clear events", {112, 34})) store_.clearEvents();
    ImGui::SameLine(0, 8);
    ImGui::PushStyleColor(ImGuiCol_Button, {0.46f,0.12f,0.15f,1}); ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.60f,0.18f,0.21f,1});
    if (ImGui::Button("Send stop", {112, 34})) store_.stopRobot();
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("One zero-velocity message. Exit the controller for a persistent manual stop.");
    ImGui::PopStyleColor(2);
    ImGui::EndChild();
    ImGui::PopStyleVar();
  }
  void drawPerformanceOverlay() {
    metrics_.sample();
    const ImVec2 contentMax = ImGui::GetWindowContentRegionMax();
    ImGui::SetCursorPos({std::max(12.0f, contentMax.x - 152.0f), std::max(62.0f, contentMax.y - 40.0f)});
    ImGui::PushStyleColor(ImGuiCol_ChildBg, {0.015f,0.020f,0.030f,0.84f});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {7, 4});
    ImGui::BeginChild("Performance overlay", {150, 38}, false, ImGuiWindowFlags_NoScrollbar);
    ImGui::SetWindowFontScale(0.40f);
    ImGui::TextColored({0.65f,0.92f,1.0f,1}, "Render %.2f ms   FPS %.1f", ImGui::GetIO().DeltaTime * 1000.0f, ImGui::GetIO().Framerate);
    ImGui::Text("CPU %s   RAM %s", number(metrics_.cpuPercent, "%").c_str(), metrics_.ramUsedGiB < 0 ? "n/a" : (number(metrics_.ramUsedGiB, " /") + number(metrics_.ramTotalGiB, " GiB")).c_str());
    ImGui::Text("GPU %s   VRAM %s", number(metrics_.gpuPercent, "%").c_str(), metrics_.gpuUsedMiB < 0 ? "n/a" : (number(metrics_.gpuUsedMiB, " /") + number(metrics_.gpuTotalMiB, " MiB")).c_str());
    ImGui::EndChild(); ImGui::PopStyleVar(); ImGui::PopStyleColor();
  }
  void verticalSplitter(float height, float availableWidth) {
    const ImVec2 start = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton("Mission width splitter", ImVec2(7, height));
    if (ImGui::IsItemActive()) missionPaneWidth_ = std::clamp(missionPaneWidth_ + ImGui::GetIO().MouseDelta.x, 180.0f, std::max(180.0f, availableWidth - 220.0f));
    const ImU32 color = ImGui::IsItemActive() || ImGui::IsItemHovered() ? IM_COL32(50, 190, 220, 220) : IM_COL32(55, 95, 108, 180);
    ImGui::GetWindowDrawList()->AddRectFilled(start, ImVec2(start.x + 3, start.y + height), color);
  }
  void horizontalSplitter(float width, float availableHeight) {
    const ImVec2 start = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton("Camera height splitter", ImVec2(width, 7));
    if (ImGui::IsItemActive()) frontCameraHeight_ = std::clamp(frontCameraHeight_ + ImGui::GetIO().MouseDelta.y, 100.0f, std::max(100.0f, availableHeight - 150.0f));
    const ImU32 color = ImGui::IsItemActive() || ImGui::IsItemHovered() ? IM_COL32(50, 190, 220, 220) : IM_COL32(55, 95, 108, 180);
    ImGui::GetWindowDrawList()->AddRectFilled(start, ImVec2(start.x + width, start.y + 3), color);
  }
  const char *primaryAction(const std::string &state, const char *&command) const {
    if (state == "navigating") { command = "pause"; return "Pause mission"; }
    if (state == "paused") { command = "resume"; return "Resume mission"; }
    if (state == "arrived") { command = "next"; return "Next task"; }
    if (state == "cancelled" || state == "failed" || state == "recovery_required") { command = "retry"; return "Retry task"; }
    command = "start";
    return "Start mission";
  }
  void drawFullscreenCamera(const ImageFrame &front, const ImageFrame &qr) {
    if (!fullscreenCamera_) return;
    const ImGuiViewport *viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::Begin(fullscreenCamera_ == 1 ? "Front camera fullscreen" : "QR camera fullscreen", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);
    if (ImGui::Button("Close fullscreen")) fullscreenCamera_ = 0;
    ImGui::Separator();
    if (fullscreenCamera_ == 1) drawImagePanel("Front camera", front, frontTexture_.id, "/amr/front/image");
    else drawImagePanel("QR camera", qr, qrTexture_.id, "/amr/qr/image");
    ImGui::End();
  }
  void drawMission(const StatusSnapshot &s, const ImageFrame &front, const ImageFrame &qr) {
    const ImVec2 available = ImGui::GetContentRegionAvail();
    const float leftWidth = std::clamp(missionPaneWidth_, 180.0f, std::max(180.0f, available.x - 220.0f));
    ImGui::BeginChild("Mission control pane", ImVec2(leftWidth, available.y), ImGuiChildFlags_Borders);
    heading("Mission control");
    ImGui::TextDisabled("Build the queue and run the current task.");
    ImGui::Separator();
    ImGui::BeginTable("Checkpoint input row", 3, ImGuiTableFlags_SizingStretchProp);
    ImGui::TableSetupColumn("Checkpoint", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn("Add", ImGuiTableColumnFlags_WidthFixed, 58.0f);
    ImGui::TableSetupColumn("Clear", ImGuiTableColumnFlags_WidthFixed, 66.0f);
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##destination", "Checkpoint ID", destinationInput_, sizeof(destinationInput_));
    ImGui::TableSetColumnIndex(1);
    if (ImGui::Button("Add", ImVec2(-1, 0)) && destinationInput_[0]) { draftTasks_.emplace_back(destinationInput_); destinationInput_[0] = 0; }
    ImGui::TableSetColumnIndex(2);
    if (ImGui::Button("Clear", ImVec2(-1, 0))) draftTasks_.clear();
    ImGui::EndTable();
    if (ImGui::Button("Create mission", ImVec2(-1, 32)) && !draftTasks_.empty()) {
      std::string command = "create:";
      for (std::size_t i = 0; i < draftTasks_.size(); ++i) { if (i) command += ","; command += draftTasks_[i]; }
      store_.sendMissionCommand(command);
    }
    ImGui::Spacing();
    heading("Queue");
    if (draftTasks_.empty()) ImGui::TextDisabled("Add one or more checkpoints.");
    for (std::size_t i = 0; i < draftTasks_.size(); ++i) ImGui::BulletText("%zu. %s", i + 1, draftTasks_[i].c_str());
    ImGui::Separator();
    const std::string state = missionField(s.mission, "state");
    drawKeyValue("Mission", missionField(s.mission, "mission"));
    drawKeyValue("Robot", missionField(s.mission, "robot"));
    drawKeyValue("Status", state);
    drawKeyValue("Route", missionField(s.mission, "current") + "  →  " + missionField(s.mission, "goal"));
    ImGui::Spacing();
    const char *command = "start";
    const char *label = primaryAction(state, command);
    const bool missionReady = !missionField(s.mission, "mission").empty() && missionField(s.mission, "mission") != "—";
    const bool canCancel = state == "navigating" || state == "paused" || state == "arrived";
    if (!missionReady) ImGui::TextDisabled("Create a mission before starting.");
    else if (canCancel) {
      const float width = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
      if (ImGui::Button(label, ImVec2(width, 42))) store_.sendMissionCommand(command);
      ImGui::SameLine();
      ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.46f, 0.12f, 0.15f, 1));
      if (ImGui::Button("Cancel mission", ImVec2(-1, 42))) store_.sendMissionCommand("cancel");
      ImGui::PopStyleColor();
    } else if (ImGui::Button(label, ImVec2(-1, 42))) store_.sendMissionCommand(command);
    ImGui::Spacing();
    heading("Activity");
    ImGui::TextWrapped("%s", missionField(s.mission, "event").c_str());
    ImGui::EndChild();
    ImGui::SameLine(0, 0);
    verticalSplitter(available.y, available.x);
    ImGui::SameLine(0, 0);
    const float rightWidth = ImGui::GetContentRegionAvail().x;
    ImGui::BeginChild("Mission camera pane", ImVec2(0, available.y), ImGuiChildFlags_Borders);
    const float cameraSpace = ImGui::GetContentRegionAvail().y;
    const float topHeight = std::clamp(frontCameraHeight_, 100.0f, std::max(100.0f, cameraSpace - 150.0f));
    ImGui::TextUnformatted("Front camera");
    ImGui::SameLine();
    ImGui::SetCursorPosX(std::max(ImGui::GetCursorPosX(), ImGui::GetWindowContentRegionMax().x - 102.0f));
    if (ImGui::Button("Fullscreen##front")) fullscreenCamera_ = 1;
    ImGui::BeginChild("Mission front camera", ImVec2(0, topHeight - 27.0f), ImGuiChildFlags_None);
    drawImagePanel("Front view", front, frontTexture_.id, "/amr/front/image");
    ImGui::EndChild();
    horizontalSplitter(rightWidth, cameraSpace);
    ImGui::TextUnformatted("QR camera");
    ImGui::SameLine();
    ImGui::SetCursorPosX(std::max(ImGui::GetCursorPosX(), ImGui::GetWindowContentRegionMax().x - 102.0f));
    if (ImGui::Button("Fullscreen##qr")) fullscreenCamera_ = 2;
    ImGui::BeginChild("Mission QR camera", ImVec2(0, 0), ImGuiChildFlags_None);
    drawImagePanel("QR view", qr, qrTexture_.id, "/amr/qr/image");
    ImGui::EndChild();
    ImGui::EndChild();
    drawFullscreenCamera(front, qr);
  }

  void drawCameras(const ImageFrame &front, const ImageFrame &qr) {
    heading("Camera views"); ImGui::TextDisabled("Live front and checkpoint camera feeds."); ImGui::Separator();
    ImGui::BeginTable("Camera views", 2, ImGuiTableFlags_SizingStretchSame);
    ImGui::TableNextColumn(); drawImagePanel("Front camera", front, frontTexture_.id, "/amr/front/image");
    ImGui::TableNextColumn(); drawImagePanel("QR camera", qr, qrTexture_.id, "/amr/qr/image");
    ImGui::EndTable();
  }

  void drawDiagnostics(const StatusSnapshot &s, const ImageFrame &front, const ImageFrame &qr) {
    heading("Transport diagnostics"); ImGui::TextDisabled("Freshness uses the last received message timestamp, so stale sensors are visible without adding simulator load."); ImGui::Separator();
    ImGui::BeginTable("Topic diagnostics", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp);
    ImGui::TableSetupColumn("Topic"); ImGui::TableSetupColumn("Payload"); ImGui::TableSetupColumn("Last update"); ImGui::TableSetupColumn("Health"); ImGui::TableHeadersRow();
    const auto row=[](const char *topic,const std::string &value,Clock::time_point time,double limit){ ImGui::TableNextRow(); ImGui::TableNextColumn(); ImGui::TextUnformatted(topic); ImGui::TableNextColumn(); ImGui::TextWrapped("%s",value.c_str()); ImGui::TableNextColumn(); ImGui::TextUnformatted(ageLabel(time).c_str()); ImGui::TableNextColumn(); const auto tone=freshness(time,limit); drawStatusPill(tone==StatusTone::Ok?"LIVE":tone==StatusTone::Warning?"STALE":"WAITING",tone); };
    row("/amr/telemetry", s.telemetry.empty()?"none":s.telemetry, s.telemetryReceived, 2); row("/amr/checkpoint",s.checkpoint.empty()?"none":s.checkpoint,s.checkpointReceived,4); row("/mission/status",s.mission.empty()?"none":s.mission,s.missionReceived,2);
    row("/amr/front/image",front.width?std::to_string(front.width)+" x "+std::to_string(front.height):"none",front.received,2); row("/amr/qr/image",qr.width?std::to_string(qr.width)+" x "+std::to_string(qr.height):"none",qr.received,4);
    ImGui::EndTable(); ImGui::Spacing(); ImGui::TextDisabled("GPU values use nvidia-smi only while this viewer is open; unavailable means the NVIDIA driver or device is not currently accessible.");
  }
  void drawSettings() {
    heading("Settings");
    ImGui::TextDisabled("Presentation preferences for this console.");
    ImGui::Separator();
    ImGui::BeginChild("Preferences", ImVec2(420, 142), ImGuiChildFlags_Borders);
    if (ImGui::Checkbox("Vertical sync", &vsync_)) glfwSwapInterval(vsync_ ? 1 : 0);
    ImGui::SetNextItemWidth(230);
    ImGui::SliderFloat("Interface scale", &scale_, 0.8f, 1.4f, "%.2fx");
    ImGui::TextDisabled("Camera panels can be arranged on the Mission page.");
    ImGui::EndChild();
  }
  void drawContent(const StatusSnapshot &s, const ImageFrame &front, const ImageFrame &qr) {
    ImGui::BeginChild("Content", ImVec2(0, 0), ImGuiChildFlags_None);
    drawStatusStrip(s);
    drawToolbar(s);
    ImGui::Separator();
    switch (tab_) {
      case Tab::Mission: drawMission(s, front, qr); break;
      case Tab::Cameras: drawCameras(front, qr); break;
      case Tab::Diagnostics: drawDiagnostics(s, front, qr); break;
      case Tab::Settings: drawSettings(); break;
    }
    drawPerformanceOverlay();
    ImGui::EndChild();
  }
  void draw() {
    const auto front=store_.copyFront(), qr=store_.copyQr(); const auto status=store_.status(); frontTexture_.upload(front); qrTexture_.upload(qr);
    glfwGetFramebufferSize(window_,&width_,&height_); glViewport(0,0,width_,height_); glClearColor(.045f,.055f,.065f,1); glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_NewFrame(); ImGui_ImplGlfw_NewFrame(); ImGui::NewFrame(); ImGui::GetIO().FontGlobalScale=scale_;
    ImGui::SetNextWindowPos({0,0}); ImGui::SetNextWindowSize({static_cast<float>(width_),static_cast<float>(height_)});
    ImGui::Begin("TrackTag Console",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoSavedSettings);
    drawTabBar(); drawContent(status,front,qr); ImGui::End(); ImGui::Render(); ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData()); glfwSwapBuffers(window_);
  }
  GLFWwindow *window_=nullptr; int width_=1,height_=1; ImFont *headingFont_=nullptr; TelemetryStore store_; Texture frontTexture_,qrTexture_; SystemMetrics metrics_; Tab tab_=Tab::Mission; bool vsync_=true; float scale_=1; char destinationInput_[128]{}; std::vector<std::string> draftTasks_; float missionPaneWidth_=440.0f; float frontCameraHeight_=310.0f; int fullscreenCamera_=0;
};
}
int Dashboard::run() { return DashboardApp{}.run(); }
}

#pragma once

#include <string>

#include "crawler_sensor_console/freetype_text_driver.hpp"
#include "crawler_sensor_console/ui/button.hpp"
#include "crawler_sensor_console/ui/render.hpp"

namespace crawler_sensor_console {

class UiPopup final {
 public:
  explicit UiPopup(std::string title);

  void Open() noexcept;
  void Close() noexcept;
  bool is_open() const noexcept;
  void SetBounds(double x_px, double y_px, double width_px,
                 double height_px) noexcept;
  UiRect content_bounds() const noexcept;
  void UpdatePointer(double x_px, double y_px) noexcept;
  bool HandleLeftMouse(bool pressed, double x_px, double y_px) noexcept;
  bool HandleKey(int key, int action) noexcept;
  void Draw(int viewport_width_px, int viewport_height_px,
            FreeTypeTextDriver& text_driver) noexcept;

 private:
  bool HeaderHitTest(double x_px, double y_px) const noexcept;
  bool PanelHitTest(double x_px, double y_px) const noexcept;
  void ClampToViewport() noexcept;
  void UpdateLayout() noexcept;

  std::string title_;
  UiButton close_button_;
  UiRect bounds_{};
  UiRect close_bounds_{};
  bool open_{false};
  bool positioned_{false};
  bool dragging_{false};
  double drag_offset_x_px_{0.0};
  double drag_offset_y_px_{0.0};
  int viewport_width_px_{0};
  int viewport_height_px_{0};
};

} // namespace crawler_sensor_console

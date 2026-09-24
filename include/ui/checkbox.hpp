#pragma once

#include <string>

#include "crawler_sensor_console/freetype_text_driver.hpp"

namespace crawler_sensor_console {

class UiCheckbox final {
 public:
  explicit UiCheckbox(std::string label, bool checked = false);
  void SetBounds(double x, double y, double width, double height) noexcept;
  void SetChecked(bool checked) noexcept { checked_ = checked; }
  bool checked() const noexcept { return checked_; }
  void UpdatePointer(double x, double y) noexcept;
  bool HandleLeftMouse(bool pressed, double x, double y) noexcept;
  void Draw(FreeTypeTextDriver& text_driver) noexcept;

 private:
  bool HitTest(double x, double y) const noexcept;
  std::string label_;
  double x_{0.0}, y_{0.0}, width_{0.0}, height_{0.0};
  bool checked_{false};
  bool hovered_{false};
};

} // namespace crawler_sensor_console

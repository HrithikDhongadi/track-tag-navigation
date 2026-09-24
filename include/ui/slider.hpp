#pragma once

#include <functional>
#include <string>

#include "crawler_sensor_console/freetype_text_driver.hpp"

namespace crawler_sensor_console {

class UiSlider final {
 public:
  using ChangeHandler = std::function<void(double)>;

  explicit UiSlider(std::string label, ChangeHandler on_change);

  void SetLabel(std::string label);
  void SetBounds(double x_px, double y_px, double width_px,
                 double height_px) noexcept;
  void SetRange(double minimum, double maximum) noexcept;
  void SetValue(double value) noexcept;
  void SetEnabled(bool enabled) noexcept;
  bool HitTest(double x_px, double y_px) const noexcept;
  void UpdatePointer(double x_px, double y_px) noexcept;
  bool HandleLeftMouse(bool pressed, double x_px, double y_px) noexcept;
  bool dragging() const noexcept;
  void Draw(FreeTypeTextDriver& text_driver) noexcept;

 private:
  void ApplyPointer(double x_px) noexcept;

  std::string label_;
  ChangeHandler on_change_;
  double x_px_{0.0};
  double y_px_{0.0};
  double width_px_{0.0};
  double height_px_{0.0};
  double minimum_{0.0};
  double maximum_{1.0};
  double value_{0.0};
  bool enabled_{true};
  bool hovered_{false};
  bool dragging_{false};
};

} // namespace crawler_sensor_console

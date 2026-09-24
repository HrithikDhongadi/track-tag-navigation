#pragma once

#include <functional>
#include <string>

#include "crawler_sensor_console/freetype_text_driver.hpp"

namespace crawler_sensor_console {

class UiButton final {
 public:
  using ClickHandler = std::function<void()>;

  enum class Tone {
    Neutral,
    Primary,
    Success,
    Warning,
    Danger,
  };

  UiButton(std::string label, ClickHandler on_click);

  void SetLabel(std::string label);
  void SetBounds(double x_px, double y_px, double width_px,
                 double height_px) noexcept;
  void SetCornerRadius(double radius_px) noexcept;
  void SetBold(bool bold) noexcept;
  void SetTone(Tone tone) noexcept;
  bool HitTest(double x_px, double y_px) const noexcept;
  void UpdatePointer(double x_px, double y_px) noexcept;
  bool HandleLeftMouse(bool pressed, double x_px, double y_px) noexcept;
  void Draw(FreeTypeTextDriver& text_driver) const noexcept;

 private:
  std::string label_;
  ClickHandler on_click_;
  double x_px_{0.0};
  double y_px_{0.0};
  double width_px_{0.0};
  double height_px_{0.0};
  double corner_radius_px_{0.0};
  bool bold_{false};
  Tone tone_{Tone::Neutral};
  bool hovered_{false};
  bool pressed_{false};
};

} // namespace crawler_sensor_console

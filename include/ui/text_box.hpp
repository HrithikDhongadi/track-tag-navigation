#pragma once

#include <string>

#include <GLFW/glfw3.h>

#include "crawler_sensor_console/freetype_text_driver.hpp"

namespace crawler_sensor_console {

class UiTextBox final {
 public:
  enum class InputMode { Number, Text };
  UiTextBox(std::string text, std::string label = "step mm");

  void SetBounds(double x_px, double y_px, double width_px,
                 double height_px) noexcept;
  void SetText(std::string text);
  void SetLabel(std::string label);
  void SetInputMode(InputMode mode, std::size_t maximum_length = 64U) noexcept;
  const std::string& text() const noexcept;
  bool focused() const noexcept;
  bool HitTest(double x_px, double y_px) const noexcept;
  void UpdatePointer(double x_px, double y_px) noexcept;
  bool HandleLeftMouse(bool pressed, double x_px, double y_px) noexcept;
  bool HandleChar(unsigned int codepoint);
  bool HandleKey(int key, int action);
  void Draw(FreeTypeTextDriver& text_driver) const noexcept;

 private:
  mutable std::string text_;
  std::string label_;
  double x_px_{0.0};
  double y_px_{0.0};
  double width_px_{0.0};
  double height_px_{0.0};
  bool hovered_{false};
  mutable bool focused_{false};
  InputMode input_mode_{InputMode::Number};
  std::size_t maximum_length_{12U};
};

} // namespace crawler_sensor_console

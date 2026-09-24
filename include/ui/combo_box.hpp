#pragma once

#include <string>
#include <vector>

#include "crawler_sensor_console/freetype_text_driver.hpp"

namespace crawler_sensor_console {

class UiComboBox final {
 public:
  UiComboBox(std::string label, std::vector<std::string> options);
  void SetBounds(double x, double y, double width, double height) noexcept;
  void SetValue(const std::string& value) noexcept;
  const std::string& value() const noexcept;
  void UpdatePointer(double x, double y) noexcept;
  bool HandleLeftMouse(bool pressed, double x, double y) noexcept;
  void Draw(FreeTypeTextDriver& text_driver) noexcept;

 private:
  bool HitTest(double x, double y) const noexcept;
  std::string label_;
  std::vector<std::string> options_;
  std::size_t selected_{0};
  double x_{0.0}, y_{0.0}, width_{0.0}, height_{0.0};
  bool hovered_{false};
};

} // namespace crawler_sensor_console

#pragma once

#include <string>
#include <vector>

#include "crawler_sensor_console/freetype_text_driver.hpp"

namespace crawler_sensor_console {

class UiTabBar final {
 public:
  void SetLabels(std::vector<std::string> labels);
  void SetBounds(double x_px, double y_px, double width_px,
                 double height_px) noexcept;
  void SetActiveIndex(int active_index) noexcept;
  bool HitTest(double x_px, double y_px, int* index) const noexcept;
  void Draw(FreeTypeTextDriver& text_driver) const noexcept;

 private:
  void DrawIcon(int index, double x_px, double y_px) const noexcept;

  std::vector<std::string> labels_{};
  double x_px_{0.0};
  double y_px_{0.0};
  double width_px_{0.0};
  double height_px_{0.0};
  int active_index_{0};
};

} // namespace crawler_sensor_console

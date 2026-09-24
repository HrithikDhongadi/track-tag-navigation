#pragma once

namespace crawler_sensor_console {

class Chart2D final {
 public:
  void SetViewport(double min_x, double max_x, double min_y,
                   double max_y) noexcept;
  void Begin() const noexcept;
  void BeginCartesian() const noexcept;
  void DrawGrid(int divisions) const noexcept;
  void DrawAdaptiveGrid() const noexcept;
  double min_x() const noexcept;
  double max_x() const noexcept;
  double min_y() const noexcept;
  double max_y() const noexcept;

 private:
  double min_x_{0.0};
  double max_x_{1.0};
  double min_y_{0.0};
  double max_y_{1.0};
};

} // namespace crawler_sensor_console

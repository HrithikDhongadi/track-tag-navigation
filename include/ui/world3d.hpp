#pragma once

namespace crawler_sensor_console {

class World3D final {
 public:
  World3D(double yaw_deg, double pitch_deg, double distance_m,
          double minimum_distance_m, double maximum_distance_m,
          double pan_scale);

  void Reset(double yaw_deg, double pitch_deg, double distance_m) noexcept;
  void Begin(int width_px, int height_px,
             double distance_override_m = -1.0) const noexcept;
  void Rotate(double dx_px, double dy_px) noexcept;
  void Pan(double dx_px, double dy_px) noexcept;
  void Zoom(double wheel_offset) noexcept;
  bool PickOrbitFocus(double framebuffer_x_px, double framebuffer_y_px,
                      int framebuffer_height_px, int viewport_x_px,
                      int viewport_y_px, int viewport_width_px,
                      int viewport_height_px, double model_offset_x,
                      double model_offset_y,
                      double model_offset_z) noexcept;
  double distance_m() const noexcept;

 private:
  double yaw_deg_{0.0};
  double pitch_deg_{0.0};
  double distance_m_{1.0};
  double minimum_distance_m_{0.05};
  double maximum_distance_m_{10.0};
  double pan_scale_{0.001};
  /** World-space translation that keeps the panned focus at screen center. */
  double focus_offset_x_m_{0.0};
  double focus_offset_y_m_{0.0};
  double focus_offset_z_m_{0.0};
};

} // namespace crawler_sensor_console

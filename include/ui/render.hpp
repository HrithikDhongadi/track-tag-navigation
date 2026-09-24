#pragma once

namespace crawler_sensor_console {

struct UiRect final {
  double x{0.0};
  double y{0.0};
  double width{0.0};
  double height{0.0};
};

void SetViewportTopLeft(int framebuffer_height_px, int x_px, int y_px,
                        int width_px, int height_px) noexcept;
void ClearRectTopLeft(int framebuffer_height_px, int x_px, int y_px,
                      int width_px, int height_px) noexcept;
void BeginScreenSpace(int width_px, int height_px,
                      bool disable_depth = true) noexcept;
void EndScreenSpace(bool restore_depth = true) noexcept;
void DrawFilledRect(const UiRect& rect, double red, double green, double blue,
                    double alpha) noexcept;

} // namespace crawler_sensor_console

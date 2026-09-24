#include "crawler_sensor_console/ui/media_view.hpp"

#include "crawler_sensor_console/ui/render.hpp"

#include <GL/gl.h>

namespace crawler_sensor_console {

void MediaView::DrawTexture(unsigned int texture, std::uint32_t texture_width,
                            std::uint32_t texture_height, int width_px,
                            int height_px) const noexcept {
  if (texture == 0U || texture_width == 0U || texture_height == 0U) {
    return;
  }

  const double view_w = static_cast<double>(width_px);
  const double view_h = static_cast<double>(height_px);
  const double image_aspect =
      static_cast<double>(texture_width) / texture_height;
  double draw_w = view_w;
  double draw_h = draw_w / image_aspect;
  if (draw_h > view_h) {
    draw_h = view_h;
    draw_w = draw_h * image_aspect;
  }
  const double x0 = (view_w - draw_w) * 0.5;
  const double y0 = (view_h - draw_h) * 0.5;

  BeginScreenSpace(width_px, height_px);
  glEnable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, texture);
  glColor3d(1.0, 1.0, 1.0);
  glBegin(GL_QUADS);
  glTexCoord2d(0.0, 0.0);
  glVertex2d(x0, y0);
  glTexCoord2d(1.0, 0.0);
  glVertex2d(x0 + draw_w, y0);
  glTexCoord2d(1.0, 1.0);
  glVertex2d(x0 + draw_w, y0 + draw_h);
  glTexCoord2d(0.0, 1.0);
  glVertex2d(x0, y0 + draw_h);
  glEnd();
  glDisable(GL_TEXTURE_2D);
  EndScreenSpace();
}

} // namespace crawler_sensor_console

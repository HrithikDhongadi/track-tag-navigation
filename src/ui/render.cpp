#include "crawler_sensor_console/ui/render.hpp"

#include <GL/gl.h>

namespace crawler_sensor_console {

void SetViewportTopLeft(int framebuffer_height_px, int x_px, int y_px,
                        int width_px, int height_px) noexcept {
  glViewport(x_px, framebuffer_height_px - y_px - height_px, width_px,
             height_px);
}

void ClearRectTopLeft(int framebuffer_height_px, int x_px, int y_px,
                      int width_px, int height_px) noexcept {
  glEnable(GL_SCISSOR_TEST);
  glScissor(x_px, framebuffer_height_px - y_px - height_px, width_px,
            height_px);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glDisable(GL_SCISSOR_TEST);
}

void BeginScreenSpace(int width_px, int height_px,
                      bool disable_depth) noexcept {
  glMatrixMode(GL_PROJECTION);
  glPushMatrix();
  glLoadIdentity();
  glOrtho(0.0, static_cast<double>(width_px), static_cast<double>(height_px),
          0.0, -1.0, 1.0);
  glMatrixMode(GL_MODELVIEW);
  glPushMatrix();
  glLoadIdentity();
  if (disable_depth) {
    glDisable(GL_DEPTH_TEST);
  }
}

void EndScreenSpace(bool restore_depth) noexcept {
  if (restore_depth) {
    glEnable(GL_DEPTH_TEST);
  }
  glPopMatrix();
  glMatrixMode(GL_PROJECTION);
  glPopMatrix();
  glMatrixMode(GL_MODELVIEW);
}

void DrawFilledRect(const UiRect& rect, double red, double green, double blue,
                    double alpha) noexcept {
  glColor4d(red, green, blue, alpha);
  glBegin(GL_QUADS);
  glVertex2d(rect.x, rect.y);
  glVertex2d(rect.x + rect.width, rect.y);
  glVertex2d(rect.x + rect.width, rect.y + rect.height);
  glVertex2d(rect.x, rect.y + rect.height);
  glEnd();
}

} // namespace crawler_sensor_console

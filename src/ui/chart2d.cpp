#include "crawler_sensor_console/ui/chart2d.hpp"

#include <algorithm>
#include <cmath>

#include <GL/gl.h>

namespace crawler_sensor_console {

void Chart2D::SetViewport(double min_x, double max_x, double min_y,
                          double max_y) noexcept {
  min_x_ = min_x;
  max_x_ = std::max(max_x, min_x + 1e-9);
  min_y_ = min_y;
  max_y_ = std::max(max_y, min_y + 1e-9);
}

void Chart2D::Begin() const noexcept {
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(min_x_, max_x_, max_y_, min_y_, -1.0, 1.0);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  glDisable(GL_DEPTH_TEST);
}

void Chart2D::BeginCartesian() const noexcept {
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(min_x_, max_x_, min_y_, max_y_, -1.0, 1.0);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  glDisable(GL_DEPTH_TEST);
}

void Chart2D::DrawGrid(int divisions) const noexcept {
  divisions = std::max(divisions, 1);
  glBegin(GL_LINES);
  for (int i = 1; i < divisions; ++i) {
    const double t = static_cast<double>(i) / divisions;
    const double grid_x = min_x_ + (max_x_ - min_x_) * t;
    const double grid_y = min_y_ + (max_y_ - min_y_) * t;
    glVertex2d(grid_x, min_y_);
    glVertex2d(grid_x, max_y_);
    glVertex2d(min_x_, grid_y);
    glVertex2d(max_x_, grid_y);
  }
  glEnd();
}

void Chart2D::DrawAdaptiveGrid() const noexcept {
  const double span_x = std::max(max_x_ - min_x_, 1e-6);
  const double span_y = std::max(max_y_ - min_y_, 1e-6);
  const double major_step =
      std::max(0.001, std::pow(10.0, std::floor(
          std::log10(std::max(span_x, span_y) / 8.0))));
  const double first_x = std::floor(min_x_ / major_step) * major_step;
  const double first_y = std::floor(min_y_ / major_step) * major_step;

  glLineWidth(1.0F);
  glBegin(GL_LINES);
  for (double x = first_x; x <= max_x_; x += major_step) {
    const bool origin = std::abs(x) < major_step * 0.5;
    glColor3d(origin ? 0.65 : 0.20, origin ? 0.18 : 0.22,
              origin ? 0.18 : 0.28);
    glVertex2d(x, min_y_);
    glVertex2d(x, max_y_);
  }
  for (double y = first_y; y <= max_y_; y += major_step) {
    const bool origin = std::abs(y) < major_step * 0.5;
    glColor3d(origin ? 0.20 : 0.20, origin ? 0.55 : 0.22,
              origin ? 0.95 : 0.28);
    glVertex2d(min_x_, y);
    glVertex2d(max_x_, y);
  }
  glEnd();
}

double Chart2D::min_x() const noexcept { return min_x_; }
double Chart2D::max_x() const noexcept { return max_x_; }
double Chart2D::min_y() const noexcept { return min_y_; }
double Chart2D::max_y() const noexcept { return max_y_; }

} // namespace crawler_sensor_console

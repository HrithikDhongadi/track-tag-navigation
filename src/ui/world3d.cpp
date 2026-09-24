#include "crawler_sensor_console/ui/world3d.hpp"

#include <algorithm>
#include <cmath>

#include <GL/gl.h>
#include <Eigen/Core>
#include <Eigen/LU>

namespace crawler_sensor_console {

World3D::World3D(double yaw_deg, double pitch_deg, double distance_m,
                 double minimum_distance_m, double maximum_distance_m,
                 double pan_scale)
    : yaw_deg_(yaw_deg),
      pitch_deg_(pitch_deg),
      distance_m_(distance_m),
      minimum_distance_m_(minimum_distance_m),
      maximum_distance_m_(maximum_distance_m),
      pan_scale_(pan_scale) {}

void World3D::Reset(double yaw_deg, double pitch_deg,
                    double distance_m) noexcept {
  yaw_deg_ = yaw_deg;
  pitch_deg_ = pitch_deg;
  distance_m_ = distance_m;
  focus_offset_x_m_ = 0.0;
  focus_offset_y_m_ = 0.0;
  focus_offset_z_m_ = 0.0;
}

void World3D::Begin(int width_px, int height_px,
                    double distance_override_m) const noexcept {
  const double distance =
      distance_override_m > 0.0 ? distance_override_m : distance_m_;
  const double aspect =
      static_cast<double>(width_px) / std::max(height_px, 1);
  constexpr double kNearZ = 0.005;
  constexpr double kFarZ = 100.0;
  constexpr double kFovY = 60.0 * M_PI / 180.0;
  const double top = std::tan(kFovY * 0.5) * kNearZ;
  const double right = top * aspect;
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glFrustum(-right, right, -top, top, kNearZ, kFarZ);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  glTranslated(0.0, 0.0, -distance);
  glRotated(pitch_deg_, 1.0, 0.0, 0.0);
  glRotated(yaw_deg_, 0.0, 1.0, 0.0);
  glTranslated(focus_offset_x_m_, focus_offset_y_m_,
               focus_offset_z_m_);
}

void World3D::Rotate(double dx_px, double dy_px) noexcept {
  yaw_deg_ += dx_px * 0.25;
  pitch_deg_ = std::clamp(pitch_deg_ + dy_px * 0.25, -89.0, 89.0);
}

void World3D::Pan(double dx_px, double dy_px) noexcept {
  const double screen_x = dx_px * pan_scale_ * distance_m_;
  const double screen_y = -dy_px * pan_scale_ * distance_m_;
  const double pitch = pitch_deg_ * M_PI / 180.0;
  const double yaw = yaw_deg_ * M_PI / 180.0;

  // Transform the screen-space drag by the inverse camera rotation. Keeping
  // this offset inside the rotation makes later orbiting pivot at the panned
  // focus instead of at the original model origin.
  const double after_pitch_x = screen_x;
  const double after_pitch_y = std::cos(pitch) * screen_y;
  const double after_pitch_z = -std::sin(pitch) * screen_y;
  focus_offset_x_m_ += std::cos(yaw) * after_pitch_x -
                       std::sin(yaw) * after_pitch_z;
  focus_offset_y_m_ += after_pitch_y;
  focus_offset_z_m_ += std::sin(yaw) * after_pitch_x +
                       std::cos(yaw) * after_pitch_z;
}

void World3D::Zoom(double wheel_offset) noexcept {
  distance_m_ *= wheel_offset > 0.0 ? 0.9 : 1.1;
  distance_m_ = std::clamp(distance_m_, minimum_distance_m_,
                           maximum_distance_m_);
}

bool World3D::PickOrbitFocus(
    double framebuffer_x_px, double framebuffer_y_px,
    int framebuffer_height_px, int viewport_x_px, int viewport_y_px,
    int viewport_width_px, int viewport_height_px, double model_offset_x,
    double model_offset_y, double model_offset_z) noexcept {
  const int read_x = static_cast<int>(std::lround(framebuffer_x_px));
  const int read_y = framebuffer_height_px - 1 -
                     static_cast<int>(std::lround(framebuffer_y_px));
  const int viewport_bottom =
      framebuffer_height_px - viewport_y_px - viewport_height_px;
  if (read_x < viewport_x_px ||
      read_x >= viewport_x_px + viewport_width_px ||
      read_y < viewport_bottom ||
      read_y >= viewport_bottom + viewport_height_px) {
    return false;
  }

  float depth = 1.0F;
  glReadPixels(read_x, read_y, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &depth);
  if (!std::isfinite(depth) || depth >= 1.0F || depth <= 0.0F) {
    return false;
  }

  GLdouble modelview_values[16]{};
  GLdouble projection_values[16]{};
  glGetDoublev(GL_MODELVIEW_MATRIX, modelview_values);
  glGetDoublev(GL_PROJECTION_MATRIX, projection_values);
  const Eigen::Map<const Eigen::Matrix<double, 4, 4, Eigen::ColMajor>>
      modelview(modelview_values);
  const Eigen::Map<const Eigen::Matrix<double, 4, 4, Eigen::ColMajor>>
      projection(projection_values);
  const Eigen::Matrix4d view_projection = projection * modelview;
  if (std::abs(view_projection.determinant()) < 1e-15) return false;

  const double normalized_x =
      2.0 * (static_cast<double>(read_x - viewport_x_px) /
             std::max(viewport_width_px, 1)) -
      1.0;
  const double normalized_y =
      2.0 * (static_cast<double>(read_y - viewport_bottom) /
             std::max(viewport_height_px, 1)) -
      1.0;
  const Eigen::Vector4d clip(normalized_x, normalized_y,
                             static_cast<double>(depth) * 2.0 - 1.0, 1.0);
  Eigen::Vector4d object = view_projection.inverse() * clip;
  if (std::abs(object.w()) < 1e-12) return false;
  object /= object.w();

  focus_offset_x_m_ = -(object.x() + model_offset_x);
  focus_offset_y_m_ = -(object.y() + model_offset_y);
  focus_offset_z_m_ = -(object.z() + model_offset_z);
  return true;
}

double World3D::distance_m() const noexcept {
  return distance_m_;
}

} // namespace crawler_sensor_console

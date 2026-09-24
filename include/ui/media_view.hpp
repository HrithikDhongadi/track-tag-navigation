#pragma once

#include <cstdint>

namespace crawler_sensor_console {

class MediaView final {
 public:
  void DrawTexture(unsigned int texture, std::uint32_t texture_width,
                   std::uint32_t texture_height, int width_px,
                   int height_px) const noexcept;
};

} // namespace crawler_sensor_console

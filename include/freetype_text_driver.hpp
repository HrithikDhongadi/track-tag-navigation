#pragma once

#include <memory>
#include <string>
#include <string_view>

namespace crawler_sensor_console {

class FreeTypeTextDriver final {
 public:
  enum class Align {
    Left,
    Center,
    Right,
  };

  enum class VerticalAlign {
    Top,
    Middle,
    Baseline,
  };

  struct Style final {
    double height{1.0};
    double y_direction{1.0};
    Align align{Align::Left};
    VerticalAlign vertical_align{VerticalAlign::Top};
    bool bold{false};
  };

  FreeTypeTextDriver();
  ~FreeTypeTextDriver();

  FreeTypeTextDriver(const FreeTypeTextDriver&) = delete;
  FreeTypeTextDriver& operator=(const FreeTypeTextDriver&) = delete;

  bool Open(const std::string& font_path, const std::string& bold_font_path,
            unsigned int pixel_height = 64U) noexcept;
  void Close() noexcept;
  bool IsOpen() const noexcept;
  double MeasureWidth(std::string_view text, double height,
                      bool bold = false) const noexcept;
  void Draw(std::string_view text, double x, double y, double z,
            const Style& style) noexcept;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace crawler_sensor_console

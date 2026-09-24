#include "crawler_sensor_console/freetype_text_driver.hpp"

#include <array>
#include <cstdio>
#include <unordered_map>

#include <GL/gl.h>
#include <ft2build.h>
#include FT_FREETYPE_H

namespace crawler_sensor_console {

struct FreeTypeTextDriver::Impl final {
  struct Glyph final {
    unsigned int texture{0};
    int width_px{0};
    int height_px{0};
    int bearing_x_px{0};
    int bearing_y_px{0};
    int advance_px{0};
  };

  struct FaceData final {
    FT_Face face{nullptr};
    int ascender_px{0};
    int line_height_px{0};
    std::unordered_map<unsigned char, Glyph> glyphs{};
  };

  FT_Library library{nullptr};
  unsigned int pixel_height{64U};
  FaceData regular{};
  FaceData bold{};

  static void DeleteTextures(FaceData* face_data) noexcept {
    for (const auto& [character, glyph] : face_data->glyphs) {
      (void)character;
      if (glyph.texture != 0U) {
        glDeleteTextures(1, &glyph.texture);
      }
    }
    face_data->glyphs.clear();
  }

  bool OpenFace(const std::string& path, FaceData* face_data) noexcept {
    if (FT_New_Face(library, path.c_str(), 0, &face_data->face) != 0) {
      std::fprintf(stderr, "FreeType failed to load font: %s\n", path.c_str());
      return false;
    }
    if (FT_Set_Pixel_Sizes(face_data->face, 0, pixel_height) != 0) {
      std::fprintf(stderr, "FreeType failed to set font pixel size: %s\n",
                   path.c_str());
      return false;
    }
    face_data->ascender_px =
        static_cast<int>(face_data->face->size->metrics.ascender >> 6);
    face_data->line_height_px =
        static_cast<int>(face_data->face->size->metrics.height >> 6);
    if (face_data->line_height_px <= 0) {
      face_data->line_height_px = static_cast<int>(pixel_height);
    }
    return true;
  }

  bool LoadGlyph(FaceData* face_data, unsigned char character) noexcept {
    if (face_data->glyphs.contains(character)) {
      return true;
    }
    if (FT_Load_Char(face_data->face, character, FT_LOAD_RENDER) != 0) {
      return false;
    }

    const FT_GlyphSlot slot = face_data->face->glyph;
    Glyph glyph;
    glyph.width_px = static_cast<int>(slot->bitmap.width);
    glyph.height_px = static_cast<int>(slot->bitmap.rows);
    glyph.bearing_x_px = slot->bitmap_left;
    glyph.bearing_y_px = slot->bitmap_top;
    glyph.advance_px = static_cast<int>(slot->advance.x >> 6);

    glGenTextures(1, &glyph.texture);
    glBindTexture(GL_TEXTURE_2D, glyph.texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, glyph.width_px,
                 glyph.height_px, 0, GL_ALPHA, GL_UNSIGNED_BYTE,
                 slot->bitmap.buffer);
    face_data->glyphs.emplace(character, glyph);
    return true;
  }

  FaceData& SelectFace(bool use_bold) noexcept {
    return use_bold ? bold : regular;
  }

  const FaceData& SelectFace(bool use_bold) const noexcept {
    return use_bold ? bold : regular;
  }
};

FreeTypeTextDriver::FreeTypeTextDriver() : impl_(std::make_unique<Impl>()) {}

FreeTypeTextDriver::~FreeTypeTextDriver() {
  Close();
}

bool FreeTypeTextDriver::Open(const std::string& font_path,
                              const std::string& bold_font_path,
                              unsigned int pixel_height) noexcept {
  Close();
  if (FT_Init_FreeType(&impl_->library) != 0) {
    std::fprintf(stderr, "FreeType initialization failed\n");
    return false;
  }
  impl_->pixel_height = pixel_height;
  if (!impl_->OpenFace(font_path, &impl_->regular) ||
      !impl_->OpenFace(bold_font_path, &impl_->bold)) {
    Close();
    return false;
  }

  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  for (unsigned int character = 32U; character <= 126U; ++character) {
    (void)impl_->LoadGlyph(&impl_->regular,
                           static_cast<unsigned char>(character));
    (void)impl_->LoadGlyph(&impl_->bold,
                           static_cast<unsigned char>(character));
  }
  glBindTexture(GL_TEXTURE_2D, 0);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
  return true;
}

void FreeTypeTextDriver::Close() noexcept {
  if (!impl_) {
    return;
  }
  Impl::DeleteTextures(&impl_->regular);
  Impl::DeleteTextures(&impl_->bold);
  if (impl_->regular.face != nullptr) {
    FT_Done_Face(impl_->regular.face);
    impl_->regular.face = nullptr;
  }
  if (impl_->bold.face != nullptr) {
    FT_Done_Face(impl_->bold.face);
    impl_->bold.face = nullptr;
  }
  if (impl_->library != nullptr) {
    FT_Done_FreeType(impl_->library);
    impl_->library = nullptr;
  }
}

bool FreeTypeTextDriver::IsOpen() const noexcept {
  return impl_ && impl_->regular.face != nullptr &&
         impl_->bold.face != nullptr;
}

double FreeTypeTextDriver::MeasureWidth(std::string_view text, double height,
                                       bool bold) const noexcept {
  if (!IsOpen()) {
    return 0.0;
  }
  const Impl::FaceData& face_data = impl_->SelectFace(bold);
  if (face_data.line_height_px <= 0) {
    return 0.0;
  }
  long width_px = 0;
  for (const unsigned char character : text) {
    const auto found = face_data.glyphs.find(character);
    if (found != face_data.glyphs.end()) {
      width_px += found->second.advance_px;
    }
  }
  return static_cast<double>(width_px) * height /
         static_cast<double>(face_data.line_height_px);
}

void FreeTypeTextDriver::Draw(std::string_view text, double x, double y,
                              double z, const Style& style) noexcept {
  if (!IsOpen() || text.empty() || style.height <= 0.0) {
    return;
  }

  Impl::FaceData& face_data = impl_->SelectFace(style.bold);
  const double text_width =
      MeasureWidth(text, style.height, style.bold);
  if (style.align == Align::Center) {
    x -= text_width * 0.5;
  } else if (style.align == Align::Right) {
    x -= text_width;
  }

  const double scale =
      style.height / static_cast<double>(face_data.line_height_px);
  double top = y;
  if (style.vertical_align == VerticalAlign::Middle) {
    top -= style.height * 0.5 * style.y_direction;
  } else if (style.vertical_align == VerticalAlign::Baseline) {
    top -= static_cast<double>(face_data.ascender_px) * scale *
           style.y_direction;
  }
  const double baseline =
      top + static_cast<double>(face_data.ascender_px) * scale *
                style.y_direction;

  const GLboolean blend_enabled = glIsEnabled(GL_BLEND);
  const GLboolean texture_enabled = glIsEnabled(GL_TEXTURE_2D);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glEnable(GL_TEXTURE_2D);

  for (const unsigned char character : text) {
    const auto found = face_data.glyphs.find(character);
    if (found == face_data.glyphs.end()) {
      continue;
    }
    const Impl::Glyph& glyph = found->second;
    const double left =
        x + static_cast<double>(glyph.bearing_x_px) * scale;
    const double glyph_top =
        baseline - static_cast<double>(glyph.bearing_y_px) * scale *
                       style.y_direction;
    const double right =
        left + static_cast<double>(glyph.width_px) * scale;
    const double bottom =
        glyph_top + static_cast<double>(glyph.height_px) * scale *
                        style.y_direction;

    glBindTexture(GL_TEXTURE_2D, glyph.texture);
    glBegin(GL_QUADS);
    glTexCoord2d(0.0, 0.0);
    glVertex3d(left, glyph_top, z);
    glTexCoord2d(1.0, 0.0);
    glVertex3d(right, glyph_top, z);
    glTexCoord2d(1.0, 1.0);
    glVertex3d(right, bottom, z);
    glTexCoord2d(0.0, 1.0);
    glVertex3d(left, bottom, z);
    glEnd();
    x += static_cast<double>(glyph.advance_px) * scale;
  }

  glBindTexture(GL_TEXTURE_2D, 0);
  if (texture_enabled == GL_FALSE) {
    glDisable(GL_TEXTURE_2D);
  }
  if (blend_enabled == GL_FALSE) {
    glDisable(GL_BLEND);
  }
}

} // namespace crawler_sensor_console

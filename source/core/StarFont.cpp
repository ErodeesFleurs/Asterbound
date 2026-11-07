#include <ft2build.h>
#include FT_FREETYPE_H
import std;
import star.file;
import star.format;
import star.vector;

import star.string;
import star.image;
import star.font;

import star.byte_array;
import star.math_common;
namespace Star {

constexpr int FontLoadFlags = FT_LOAD_NO_SVG | FT_LOAD_COLOR;

struct FTContext {
  FT_Library library = nullptr;
  std::mutex mutex;

  ~FTContext() {
    if (library) {
      FT_Done_FreeType(library);
      library = nullptr;
    }
  }
};

std::expected<FontLibrary, StarException> FontLibrary::create() {
  FontLibrary result;
  result.m_context = std::make_shared<FTContext>();
  if (FT_Init_FreeType(&result.m_context->library))
    return std::unexpected(FontException("Could not initialize FreeType library."));
  return result;
}
FontLibrary::~FontLibrary() = default;

struct FontImpl {
  std::shared_ptr<FTContext> context;
  FT_Face face = nullptr;

  ~FontImpl() {
    if (face) {
      std::lock_guard lock(context->mutex);
      FT_Done_Face(face);
    }
  }
};

std::expected<std::shared_ptr<Font>, StarException> Font::loadFont(FontLibrary const& library, String const& fileName, unsigned pixelSize) {
  auto bytes = File::read_file(fileName);
  if (!bytes)
    return std::unexpected(std::move(bytes.error()));
  return loadFont(library, std::make_shared<ByteArray>(std::move(*bytes)), pixelSize);
}

std::expected<std::shared_ptr<Font>, StarException> Font::loadFont(FontLibrary const& library, std::shared_ptr<ByteArray const> const& bytes, unsigned pixelSize) {
  std::shared_ptr<Font> font = std::make_shared<Font>(library);
  font->m_fontBuffer = bytes;
  auto sizeResult = font->setPixelSize(pixelSize);
  if (!sizeResult)
    return std::unexpected(std::move(sizeResult.error()));
  auto loaded = font->loadFontImpl();
  if (!loaded)
    return std::unexpected(std::move(loaded.error()));
  return font;
}

Font::Font(FontLibrary const& library) : Font(library.m_context) {}

Font::Font(std::shared_ptr<FTContext> context)
  : m_context(std::move(context)), m_pixelSize(0), m_alphaThreshold(0) {}

Font::~Font() = default;

std::expected<std::shared_ptr<Font>, StarException> Font::clone() const {
  std::shared_ptr<Font> font(new Font(m_context));
  font->m_fontBuffer = m_fontBuffer;
  auto sizeResult = font->setPixelSize(m_pixelSize);
  if (!sizeResult)
    return std::unexpected(std::move(sizeResult.error()));
  auto loaded = font->loadFontImpl();
  if (!loaded)
    return std::unexpected(std::move(loaded.error()));
  return font;
}

std::expected<void, StarException> Font::setPixelSize(unsigned pixelSize) {
  if (pixelSize == 0)
    pixelSize = 1;

  if (m_pixelSize == pixelSize)
    return {};

  if (m_fontImpl && FT_Set_Pixel_Sizes(m_fontImpl->face, pixelSize, 0))
    return std::unexpected(FontException(strf("Cannot set font pixel size to: {}", pixelSize)));
  m_pixelSize = pixelSize;
  return {};
}

void Font::setAlphaThreshold(std::uint8_t alphaThreshold) {
  m_alphaThreshold = alphaThreshold;
}

unsigned Font::height() const {
  return m_pixelSize;
}

std::expected<unsigned, StarException> Font::width(String::Char c) {
  if (auto width = m_widthCache.maybe({c, m_pixelSize})) {
    return *width;
  } else {
    auto loaded = loadFontImpl();
    if (!loaded)
      return std::unexpected(std::move(loaded.error()));
    if (FT_Load_Char(m_fontImpl->face, c, FontLoadFlags))
      return std::unexpected(FontException::format("Cannot load font character: {}", static_cast<std::uint32_t>(c)));
    unsigned newWidth = (m_fontImpl->face->glyph->linearHoriAdvance + 32768) / 65536;
    m_widthCache.insert({c, m_pixelSize}, newWidth);
    return newWidth;
  }
}

std::expected<void, StarException> Font::loadFontImpl() {
  if (!m_fontImpl) {
    if (!m_fontBuffer || m_fontBuffer->empty())
      return std::unexpected(FontException("Font buffer is null or empty"));

    std::shared_ptr<FontImpl> fontImpl = std::make_shared<FontImpl>();
    fontImpl->context = m_context;
    {
      std::lock_guard lock(m_context->mutex);
      if (FT_New_Memory_Face(m_context->library, reinterpret_cast<FT_Byte const*>(m_fontBuffer->span().data()), m_fontBuffer->size(), 0, &fontImpl->face))
        return std::unexpected(FontException::format("Could not load font from buffer"));
    }

    if (FT_Set_Pixel_Sizes(fontImpl->face, m_pixelSize, 0))
      return std::unexpected(FontException::format("Cannot set font pixel size to: {}", m_pixelSize));

    m_fontImpl = fontImpl;
  }
  return {};
}

std::expected<std::tuple<Image, Vec2I, bool>, StarException> Font::render(String::Char c) {
  auto loaded = loadFontImpl();
  if (!loaded)
    return std::unexpected(std::move(loaded.error()));

  FT_Face face = m_fontImpl->face;
  if (FT_Load_Glyph(face, FT_Get_Char_Index(face, c), FontLoadFlags) != 0)
    return std::unexpected(FontException::format("Cannot load font glyph: {}", static_cast<std::uint32_t>(c)));

  if (FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL) != 0)
    return std::unexpected(FontException::format("Cannot render font glyph: {}", static_cast<std::uint32_t>(c)));

  FT_GlyphSlot slot = face->glyph;
  if (!slot->bitmap.buffer)
    return std::tuple<Image, Vec2I, bool>{};

  unsigned width = slot->bitmap.width;
  unsigned height = slot->bitmap.rows;
  bool colored = false;

  Image image(width + 2, height + 2, PixelFormat::BGRA32);
  if (slot->bitmap.pixel_mode == FT_PIXEL_MODE_GRAY) {
    Vec4B white(255, 255, 255, 0);
    image.fill(white);
    for (unsigned y = 0; y != height; ++y) {
      std::span<std::uint8_t const> row(slot->bitmap.buffer + y * slot->bitmap.pitch, width);
      for (unsigned x = 0; x != width; ++x) {
        if (x < width && y < height) {
          white[3] = m_alphaThreshold
            ? (row[x] >= m_alphaThreshold ? 255 : 0)
            : row[x];
          image.set32(x + 1, height - y, white);
        }
      }
    }
  } else if ((colored = (slot->bitmap.pixel_mode == FT_PIXEL_MODE_BGRA))) {
    unsigned bpp = image.bytesPerPixel();
    auto data = image.data();
    for (std::size_t y = 0; y != height; ++y) {
      auto row = data.subspan(bpp + (image.width() * (image.height() - 2 - y)) * bpp, slot->bitmap.pitch);
      std::memcpy(row.data(), slot->bitmap.buffer + y * slot->bitmap.pitch, row.size());
    }
    // unfortunately FreeType pre-multiplied the color channels :(
    image.forEachPixel([](unsigned, unsigned, Vec4B& pixel) {
      if (pixel[3] != 0 && pixel[3] != 255) {
        float a = byteToFloat(pixel[3]);
        pixel[0] /= a;
        pixel[1] /= a;
        pixel[2] /= a;
      }
    });
  } else {
    return std::unexpected(FontException::format("Unsupported font bitmap pixel mode: {}", slot->bitmap.pixel_mode));
  }

  return std::make_tuple(
    std::move(image),
    Vec2I(slot->bitmap_left - 1, (slot->bitmap_top - height) + (m_pixelSize / 4) - 1),
    colored);
}

std::expected<bool, StarException> Font::exists(String::Char c) {
  auto loaded = loadFontImpl();
  if (!loaded)
    return std::unexpected(std::move(loaded.error()));
  return FT_Get_Char_Index(m_fontImpl->face, c);
}

}

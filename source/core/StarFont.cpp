module;
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_TRUETYPE_TABLES_H
import std;
import star.file;
import star.format;
import star.string;
import star.font_bitmap;
import star.byte_array;

module star.font;

namespace Star {

constexpr int font_load_flags = FT_LOAD_NO_SVG | FT_LOAD_COLOR;

struct FontLibrary::Context {
  struct LibraryDeleter {
    void operator()(FT_Library library) const noexcept { FT_Done_FreeType(library); }
  };
  std::unique_ptr<FT_LibraryRec_, LibraryDeleter> library;
  std::mutex mutex;
};

std::expected<FontLibrary, StarException> FontLibrary::create() {
  FontLibrary result;
  result.m_context = std::make_shared<Context>();
  FT_Library library{};
  if (FT_Init_FreeType(&library))
    return std::unexpected(FontException("Could not initialize FreeType library."));
  result.m_context->library.reset(library);
  return result;
}
FontLibrary::~FontLibrary() = default;

void Font::FaceDeleter::operator()(FT_Face face) const noexcept {
  std::lock_guard lock(context->mutex);
  FT_Done_Face(face);
}

std::expected<Font::Face, StarException> Font::open_face(std::shared_ptr<FontLibrary::Context> context, std::shared_ptr<ByteArray const> bytes) {
  if (!context)
    return std::unexpected(FontException("Font library is not initialized"));
  if (!bytes || bytes->empty())
    return std::unexpected(FontException("Font buffer is null or empty"));
  if (!std::in_range<FT_Long>(bytes->size()))
    return std::unexpected(FontException("Font buffer is too large for FreeType"));

  FT_Face face{};
  {
    std::lock_guard lock(context->mutex);
    if (FT_New_Memory_Face(context->library.get(), reinterpret_cast<FT_Byte const*>(bytes->data()),
            static_cast<FT_Long>(bytes->size()), 0, &face))
      return std::unexpected(FontException("Could not load font from buffer"));
  }
  return Face(face, FaceDeleter{std::move(context), std::move(bytes)});
}

std::expected<std::shared_ptr<Font>, StarException> Font::load(FontLibrary const& library, String const& filename, unsigned pixel_size) {
  auto bytes = File::read_file(filename);
  if (!bytes)
    return std::unexpected(std::move(bytes.error()));
  return load(library, std::make_shared<ByteArray>(std::move(*bytes)), pixel_size);
}

std::expected<std::shared_ptr<Font>, StarException> Font::load(FontLibrary const& library, std::shared_ptr<ByteArray const> bytes, unsigned pixel_size) {
  if (bytes && bytes->size() >= 4 && std::string_view(bytes->data(), 4) == "wOF2") {
    // Keep the decoded SFNT data shared by clones, rather than asking FreeType
    // to decompress the same WOFF2 font for every text painter.
    auto compressed = open_face(library.m_context, std::move(bytes));
    if (!compressed)
      return std::unexpected(std::move(compressed.error()));
    FT_ULong size = 0;
    if (FT_Load_Sfnt_Table(compressed->get(), 0, 0, nullptr, &size)
        || size == 0 || size > ByteArray::max_size() || !std::in_range<FT_Long>(size))
      return std::unexpected(FontException("Could not read decoded font size"));
    auto decoded = std::make_shared<ByteArray>();
    decoded->resize(size);
    if (FT_Load_Sfnt_Table(compressed->get(), 0, 0,
            reinterpret_cast<FT_Byte*>(decoded->data()), &size)
        || size != decoded->size())
      return std::unexpected(FontException("Could not read decoded font data"));
    bytes = std::move(decoded);
  }
  return create(library.m_context, std::move(bytes), pixel_size, 0);
}

std::expected<std::shared_ptr<Font>, StarException> Font::create(std::shared_ptr<FontLibrary::Context> context, std::shared_ptr<ByteArray const> bytes,
    unsigned pixel_size, std::uint8_t alpha_threshold) {
  auto face = open_face(std::move(context), std::move(bytes));
  if (!face)
    return std::unexpected(std::move(face.error()));
  pixel_size = std::max(1u, pixel_size);
  if (FT_Set_Pixel_Sizes(face->get(), pixel_size, 0))
    return std::unexpected(FontException::format("Cannot set font pixel size to: {}", pixel_size));
  return std::make_shared<Font>(ConstructionKey{}, std::move(*face), pixel_size, alpha_threshold);
}

Font::Font(ConstructionKey, Face face, unsigned pixel_size, std::uint8_t alpha_threshold)
  : m_face(std::move(face)), m_pixel_size(pixel_size), m_alpha_threshold(alpha_threshold) {}

Font::~Font() = default;

std::expected<std::shared_ptr<Font>, StarException> Font::clone() const {
  auto const& backing = m_face.get_deleter();
  return create(backing.context, backing.bytes, m_pixel_size, m_alpha_threshold);
}

std::expected<void, StarException> Font::set_pixel_size(unsigned pixel_size) {
  pixel_size = std::max(1u, pixel_size);
  if (m_pixel_size == pixel_size)
    return {};
  if (FT_Set_Pixel_Sizes(m_face.get(), pixel_size, 0))
    return std::unexpected(FontException::format("Cannot set font pixel size to: {}", pixel_size));
  m_pixel_size = pixel_size;
  return {};
}

std::expected<unsigned, StarException> Font::width(String::Char c, unsigned pixel_size) {
  pixel_size = std::max(1u, pixel_size);
  auto key = std::pair(c, pixel_size);
  if (auto width = m_width_cache.maybe(key))
    return *width;

  auto sized = set_pixel_size(pixel_size);
  if (!sized)
    return std::unexpected(std::move(sized.error()));
  if (FT_Load_Char(m_face.get(), c, font_load_flags))
    return std::unexpected(FontException::format("Cannot load font character: {}", static_cast<std::uint32_t>(c)));
  unsigned width = (m_face->glyph->linearHoriAdvance + 32768) / 65536;
  m_width_cache.insert(key, width);
  return width;
}

std::expected<RenderedGlyph, StarException> Font::render(String::Char c) {
  if (FT_Load_Glyph(m_face.get(), FT_Get_Char_Index(m_face.get(), c), font_load_flags))
    return std::unexpected(FontException::format("Cannot load font glyph: {}", static_cast<std::uint32_t>(c)));

  if (FT_Render_Glyph(m_face->glyph, FT_RENDER_MODE_NORMAL))
    return std::unexpected(FontException::format("Cannot render font glyph: {}", static_cast<std::uint32_t>(c)));

  auto const& slot = *m_face->glyph;
  return render_font_bitmap(slot.bitmap, slot.bitmap_left, slot.bitmap_top, m_pixel_size, m_alpha_threshold);
}

bool Font::exists(String::Char c) const noexcept {
  return FT_Get_Char_Index(m_face.get(), c) != 0;
}

}

import std;
import star.format;
import star.json;
import star.string;
import star.color;
import star.font;
import star.image;
import star.bi_map;
import star.ref_ptr;
import star.renderer;
import star.list;
import star.image_processing;
import star.hash;
import star.data_stream;
import star.string_view;
import star.thread;
import star.directives;
import star.font_texture_group;
import star.logging;
import star.root;

import star.vector;
import star.algorithm;
namespace Star {

FontTextureGroup::FontTextureGroup(Root& root, std::shared_ptr<TextureGroup> textureGroup)
  : m_root(root), m_textureGroup(std::move(textureGroup)) {}

void FontTextureGroup::cleanup(std::int64_t timeout) {
  auto currentTime = std::chrono::time_point_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() + std::chrono::microseconds(500));
  auto timeToLive = std::chrono::milliseconds(timeout);
  eraseWhere(m_glyphs, [&](auto const& p) { return currentTime - p.second.time > timeToLive; });
}

std::expected<void, StarException> FontTextureGroup::switchFont(String const& font) {
  if (font.empty()) {
    m_activeFont = m_defaultFont;
    m_fontName.clear();
  } else if (m_fontName != font) {
    auto key = CaseInsensitiveStringView::create(font);
    if (!key)
      return std::unexpected(std::move(key.error()));
    auto find = m_fonts.find(*key);
    m_activeFont = find != m_fonts.end() ? find->second : m_defaultFont;
    m_fontName = font;
  }
  return {};
}

String const& FontTextureGroup::activeFont() {
  return m_fontName;
}

std::expected<void, StarException> FontTextureGroup::addFont(std::shared_ptr<Font> const& font, String const& name) {
  auto key = CaseInsensitiveString::create(name);
  if (!key)
    return std::unexpected(std::move(key.error()));
  if (m_fonts.contains(*key))
    m_glyphs.clear();
  m_fonts[std::move(*key)] = font;
  return {};
}

void FontTextureGroup::clearFonts() {
  m_glyphs.clear();
  m_fonts.clear();
  m_activeFont = m_defaultFont;
}

std::expected<void, StarException> FontTextureGroup::setFixedFonts(String const& defaultFontName, String const& fallbackFontName, String const& emojiFontName) {
  auto defaultKey = CaseInsensitiveStringView::create(defaultFontName);
  if (!defaultKey)
    return std::unexpected(std::move(defaultKey.error()));
  auto fallbackKey = CaseInsensitiveStringView::create(fallbackFontName);
  if (!fallbackKey)
    return std::unexpected(std::move(fallbackKey.error()));
  auto emojiKey = CaseInsensitiveStringView::create(emojiFontName);
  if (!emojiKey)
    return std::unexpected(std::move(emojiKey.error()));
  if (auto defaultFont = m_fonts.find_ref(*defaultKey))
    m_defaultFont = m_activeFont = defaultFont->get();
  if (auto fallbackFont = m_fonts.find_ref(*fallbackKey))
    m_fallbackFont = fallbackFont->get();
  if (auto emojiFont = m_fonts.find_ref(*emojiKey))
    m_emojiFont = emojiFont->get();
  return {};
}

std::expected<std::reference_wrapper<FontTextureGroup::GlyphTexture const>, StarException> FontTextureGroup::glyphTexture(String::Char c, unsigned size, std::optional<std::reference_wrapper<Directives const>> processingDirectives) {
  auto fontResult = getFontForCharacter(c);
  if (!fontResult)
    return std::unexpected(std::move(fontResult.error()));
  Font& font = fontResult->get();
  if (m_emojiFont && std::addressof(font) == std::addressof(*m_emojiFont))
    processingDirectives.reset();
  GlyphDescriptor descriptor{c, size, processingDirectives ? processingDirectives->get().hash() : 0, std::ref(font)};
  if (auto cached = m_glyphs.find_ref(descriptor)) {
    cached->get().time = std::chrono::time_point_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() + std::chrono::microseconds(500));
    return std::cref(cached->get());
  }

  auto pixelSize = font.setPixelSize(size);
  if (!pixelSize)
    return std::unexpected(std::move(pixelSize.error()));
  auto rendered = font.render(c);
  if (!rendered)
    return std::unexpected(std::move(rendered.error()));
  auto& renderResult = *rendered;
  Image& image = std::get<0>(renderResult);
  GlyphTexture glyphTexture;
  if (processingDirectives) {
    Vec2F preSize = Vec2F(image.size());
    bool broken = false;
    auto shared = processingDirectives->get().get();
    if (!shared)
      return std::unexpected(std::move(shared.error()));
    for (auto& entry : shared->get().entries) {
      if (std::holds_alternative<ErrorImageOperation>(entry.operation)) {
        auto& error = std::get<ErrorImageOperation>(entry.operation);
        if (!error.reported) {
          auto logged = m_root.logger().error("Error in parsed font directives: {}", error.cause.what());
          if (!logged)
            return std::unexpected(std::move(logged.error()));
          error.reported = true;
        }
        broken = true;
      } else {
        auto processed = process_image_operation(entry.operation, image);
        if (!processed) {
          broken = true;
          auto character = String::create(c);
          if (!character)
            return std::unexpected(std::move(character.error()));
          auto logged = m_root.logger().error("Exception processing font directives for {}px '{}': {}", size, *character, processed.error().what());
          if (!logged)
            return std::unexpected(std::move(logged.error()));
        }
      }
    }
    if (broken) {
      glyphTexture.colored = true;
      image.for_each_pixel([](unsigned x, unsigned y, Vec4B& pixel) {
        pixel = ((x + y) % 2 == 0) ? Vec4B(255, 0, 255, pixel[3]) : Vec4B(0, 0, 0, pixel[3]);
      });
    }
    glyphTexture.offset = (preSize - Vec2F(image.size())) / 2;
  }
  glyphTexture.colored |= std::get<2>(renderResult);
  glyphTexture.offset += Vec2F(std::get<1>(renderResult));
  auto texture = m_textureGroup->create(image);
  if (!texture)
    return std::unexpected(std::move(texture.error()));
  glyphTexture.texture = std::move(*texture);
  glyphTexture.time = std::chrono::time_point_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() + std::chrono::microseconds(500));
  auto inserted = m_glyphs.add(descriptor, std::move(glyphTexture));
  if (!inserted)
    return std::unexpected(std::move(inserted.error()));
  return std::cref(inserted->get());
}

std::expected<TexturePtr, StarException> FontTextureGroup::glyphTexturePtr(String::Char c, unsigned size) {
  return glyphTexturePtr(c, size, std::nullopt);
}

std::expected<TexturePtr, StarException> FontTextureGroup::glyphTexturePtr(String::Char c, unsigned size, std::optional<std::reference_wrapper<Directives const>> processingDirectives) {
  auto glyph = glyphTexture(c, size, processingDirectives);
  if (!glyph)
    return std::unexpected(std::move(glyph.error()));
  return glyph->get().texture;
}

std::expected<unsigned, StarException> FontTextureGroup::glyphWidth(String::Char c, unsigned fontSize) {
  auto font = getFontForCharacter(c);
  if (!font)
    return std::unexpected(std::move(font.error()));
  auto pixelSize = font->get().setPixelSize(fontSize);
  if (!pixelSize)
    return std::unexpected(std::move(pixelSize.error()));
  return font->get().width(c);
}

std::expected<std::reference_wrapper<Font>, StarException> FontTextureGroup::getFontForCharacter(String::Char c) {
  if (!m_activeFont)
    return std::unexpected(FontException("No default font is configured"));
  if (((c >= 0x1F600 && c <= 0x1F64F) || // Emoticons
       (c >= 0x1F300 && c <= 0x1F5FF) || // Misc Symbols and Pictographs
       (c >= 0x1F680 && c <= 0x1F6FF) || // Transport and Map
       (c >= 0x1F1E6 && c <= 0x1F1FF) || // Regional country flags
       (c >= 0x2600  && c <= 0x26FF ) || // Misc symbols 9728 - 9983
       (c >= 0x2700  && c <= 0x27BF ) || // Dingbats
       (c >= 0xFE00  && c <= 0xFE0F ) || // Variation Selectors
       (c >= 0x1F900 && c <= 0x1F9FF) || // Supplemental Symbols and Pictographs
       (c >= 0x1F018 && c <= 0x1F270) || // Various asian characters
       (c >= 65024   && c <= 65039  ) || // Variation selector
       (c >= 9100    && c <= 9300   ) || // Misc items
       (c >= 8400    && c <= 8447   ))) { // Combining Diacritical Marks for Symbols
    if (!m_emojiFont)
      return std::unexpected(FontException("No emoji font is configured"));
    auto exists = m_emojiFont->exists(c);
    if (!exists)
      return std::unexpected(std::move(exists.error()));
    if (*exists)
      return std::ref(*m_emojiFont);
  }
  auto exists = m_activeFont->exists(c);
  if (!exists)
    return std::unexpected(std::move(exists.error()));
  if (*exists || !m_fallbackFont)
    return std::ref(*m_activeFont);
  return std::ref(*m_fallbackFont);
}

}

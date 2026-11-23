import std;
import star.format;
import star.string;
import star.font;
import star.image;
import star.ref_ptr;
import star.renderer;
import star.image_processing;
import star.directives;
import star.font_texture_group;
import star.logging;
import star.root;

import star.vector;
import star.algorithm;
namespace Star {
namespace {
auto glyph_time() {
  return std::chrono::time_point_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() + std::chrono::microseconds(500));
}
}

FontTextureGroup::FontTextureGroup(Root& root, std::shared_ptr<TextureGroup> texture_group)
  : m_root(root), m_texture_group(std::move(texture_group)) {}

void FontTextureGroup::cleanup(std::int64_t timeout) {
  auto now = glyph_time();
  auto age = std::chrono::milliseconds(timeout);
  Star::erase_if(m_glyphs, [&](decltype(m_glyphs)::value_type const& entry) { return now - entry.second.time > age; });
}

std::expected<void, StarException> FontTextureGroup::FontSet::insert_or_assign(String name, std::shared_ptr<Font> font) {
  if (!font)
    return std::unexpected(FontException("Cannot register an empty font"));
  auto key = CaseInsensitiveString::create(std::move(name));
  if (!key)
    return std::unexpected(std::move(key.error()));
  m_fonts.insert_or_assign(std::move(*key), std::move(font));
  return {};
}

std::expected<std::shared_ptr<Font>, StarException> FontTextureGroup::FontSet::find(std::string_view name) const {
  auto key = CaseInsensitiveStringView::create(name);
  if (!key)
    return std::unexpected(std::move(key.error()));
  return m_fonts.value(*key);
}

std::expected<std::shared_ptr<Font>, StarException> FontTextureGroup::named_font(std::string_view name) const {
  if (name.empty())
    return m_default_font;
  auto found = m_fonts.find(name);
  if (!found)
    return std::unexpected(std::move(found.error()));
  return *found ? std::move(*found) : m_default_font;
}

std::expected<void, StarException> FontTextureGroup::select_font(String const& name) {
  if (name != m_font_name) {
    auto selected = named_font(name.utf8());
    if (!selected)
      return std::unexpected(std::move(selected.error()));
    m_active_font = std::move(*selected);
    m_font_name = name;
  }
  return {};
}

std::expected<void, StarException> FontTextureGroup::replace_fonts(FontSet&& fonts, String const& default_name,
    String const& fallback_name, String const& emoji_name) {
  auto default_font = fonts.find(default_name.utf8());
  if (!default_font)
    return std::unexpected(std::move(default_font.error()));
  if (!*default_font)
    return std::unexpected(FontException::format("Default font '{}' not found", default_name));
  auto fallback_font = fonts.find(fallback_name.utf8());
  if (!fallback_font)
    return std::unexpected(std::move(fallback_font.error()));
  auto emoji_font = fonts.find(emoji_name.utf8());
  if (!emoji_font)
    return std::unexpected(std::move(emoji_font.error()));
  auto selected = m_font_name.empty() ? default_font : fonts.find(m_font_name.utf8());
  if (!selected)
    return std::unexpected(std::move(selected.error()));

  // Missing named selections use this generation's default. Missing fallback
  // and emoji fonts are never inherited from the previous registry.
  m_glyphs.clear();
  m_fonts = std::move(fonts);
  m_active_font = *selected ? std::move(*selected) : *default_font;
  m_default_font = std::move(*default_font);
  m_fallback_font = std::move(*fallback_font);
  m_emoji_font = std::move(*emoji_font);
  return {};
}

std::expected<std::reference_wrapper<FontTextureGroup::GlyphTexture const>, StarException>
FontTextureGroup::glyph_texture(String::Char c, unsigned size,
    std::optional<std::reference_wrapper<Directives const>> directives, GlyphDirectivePolicy policy) & {
  auto font = resolve_font(m_active_font, c);
  if (!font)
    return std::unexpected(std::move(font.error()));
  return glyph_texture(font->get(), c, size, directives, policy);
}

std::expected<std::reference_wrapper<FontTextureGroup::GlyphTexture const>, StarException>
FontTextureGroup::glyph_texture(std::shared_ptr<Font> const& font, String::Char c, unsigned size,
    std::optional<std::reference_wrapper<Directives const>> processing_directives, GlyphDirectivePolicy policy) {
  static Directives const empty;
  auto const& source = font != m_emoji_font && processing_directives ? processing_directives->get() : empty;
  bool apply_directives = !source.empty();
  if (!apply_directives)
    policy = GlyphDirectivePolicy::Default;
  GlyphLookup lookup{c, size, source, policy, font};
  if (auto cached = m_glyphs.find_ref(lookup)) {
    cached->get().time = glyph_time();
    return std::cref(cached->get());
  }

  auto pixel_size = font->set_pixel_size(size);
  if (!pixel_size)
    return std::unexpected(std::move(pixel_size.error()));
  auto rendered = font->render(c);
  if (!rendered)
    return std::unexpected(std::move(rendered.error()));
  auto& image = rendered->image;
  GlyphTexture glyph_texture;
  GlyphKey key{c, size, source, policy, font};
  if (apply_directives) {
    auto const& directives = key.directives;
    Vec2F pre_size(image.size());
    bool broken = false;
    for (auto const& entry : directives.entries()) {
      auto const& operation = directives.entry_operation(entry);
      if (std::holds_alternative<ErrorImageOperation>(operation)) {
        auto const& error = std::get<ErrorImageOperation>(operation);
        auto reported = directives.report_error(entry, [&]() -> std::expected<void, StarException> {
          return m_root.logger().error("Error in parsed font directives: {}", error.cause.what());
        });
        if (!reported)
          return std::unexpected(std::move(reported.error()));
        broken = true;
      } else {
        auto processed = [&]() -> std::expected<void, StarException> {
          if (policy == GlyphDirectivePolicy::Text && std::holds_alternative<BorderImageOperation>(operation)) {
            ImageOperation text_operation = std::get<BorderImageOperation>(operation);
            std::get<BorderImageOperation>(text_operation).include_transparent = true;
            return process_image_operation(text_operation, image);
          }
          return process_image_operation(operation, image);
        }();
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
      glyph_texture.colored = true;
      image.for_each_pixel([](unsigned x, unsigned y, Vec4B& pixel) {
        pixel = ((x + y) % 2 == 0) ? Vec4B(255, 0, 255, pixel[3]) : Vec4B(0, 0, 0, pixel[3]);
      });
    }
    glyph_texture.offset = (pre_size - Vec2F(image.size())) / 2;
  }
  glyph_texture.colored |= rendered->colored;
  glyph_texture.offset += Vec2F(rendered->offset);
  auto texture = m_texture_group->create(image);
  if (!texture)
    return std::unexpected(std::move(texture.error()));
  glyph_texture.texture = std::move(*texture);
  glyph_texture.time = glyph_time();
  auto entry = m_glyphs.try_emplace(std::move(key), std::move(glyph_texture)).first;
  return std::cref(entry->second);
}

std::expected<unsigned, StarException> FontTextureGroup::glyph_width(String::Char c, unsigned size) const {
  return glyph_width(m_active_font, c, size);
}

std::expected<unsigned, StarException> FontTextureGroup::glyph_width(
    std::shared_ptr<Font> const& active, String::Char c, unsigned size) const {
  auto font = resolve_font(active, c);
  if (!font)
    return std::unexpected(std::move(font.error()));
  return font->get()->width(c, size);
}

std::expected<std::reference_wrapper<std::shared_ptr<Font> const>, StarException>
FontTextureGroup::resolve_font(std::shared_ptr<Font> const& active, String::Char c) const {
  if (!active)
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
       (c >= 9100    && c <= 9300   ) || // Misc items
       (c >= 8400    && c <= 8447   ))) { // Combining Diacritical Marks for Symbols
    if (!m_emoji_font)
      return std::unexpected(FontException("No emoji font is configured"));
    if (m_emoji_font->exists(c))
      return std::cref(m_emoji_font);
    if (active == m_emoji_font)
      return std::cref(m_fallback_font ? m_fallback_font : active);
  }
  if (!m_fallback_font || active == m_fallback_font || active->exists(c))
    return std::cref(active);
  return std::cref(m_fallback_font);
}

}

module;
import std;
import star.format;
import star.json;
import star.string;
import star.map;

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

import star.vector;
namespace Star {
class Root;


class FontTextureGroup {
public:
  using GlyphDescriptor = std::tuple<String::Char, unsigned, std::size_t, std::reference_wrapper<Font>>;
  struct GlyphDescriptorHash {
    std::size_t operator()(GlyphDescriptor const& descriptor) const {
      return hashOf(std::get<0>(descriptor), std::get<1>(descriptor), std::get<2>(descriptor),
          std::addressof(std::get<3>(descriptor).get()));
    }
  };
  struct GlyphDescriptorEqual {
    bool operator()(GlyphDescriptor const& lhs, GlyphDescriptor const& rhs) const {
      return std::get<0>(lhs) == std::get<0>(rhs) && std::get<1>(lhs) == std::get<1>(rhs)
          && std::get<2>(lhs) == std::get<2>(rhs)
          && std::addressof(std::get<3>(lhs).get()) == std::addressof(std::get<3>(rhs).get());
    }
  };

  struct GlyphTexture {
    TexturePtr texture;
    bool colored = false;
    std::chrono::time_point<std::chrono::steady_clock, std::chrono::milliseconds> time{};
    Vec2F offset;
  };

  FontTextureGroup(Root& root, std::shared_ptr<TextureGroup> textureGroup);

  [[nodiscard]] std::expected<std::reference_wrapper<GlyphTexture const>, StarException> glyphTexture(String::Char, unsigned fontSize, std::optional<std::reference_wrapper<Directives const>> processingDirectives = std::nullopt);

  [[nodiscard]] std::expected<TexturePtr, StarException> glyphTexturePtr(String::Char, unsigned fontSize);
  [[nodiscard]] std::expected<TexturePtr, StarException> glyphTexturePtr(String::Char, unsigned fontSize, std::optional<std::reference_wrapper<Directives const>> processingDirectives);

  [[nodiscard]] std::expected<unsigned, StarException> glyphWidth(String::Char c, unsigned fontSize);

  // Removes glyphs that haven't been used in more than the given timeout in
  // milliseconds (configuration units).
  void cleanup(std::int64_t timeout);
  // Switches the current font
  [[nodiscard]] std::expected<void, StarException> switchFont(String const& font);
  String const& activeFont();
  [[nodiscard]] std::expected<void, StarException> addFont(std::shared_ptr<Font> const& font, String const& name);
  void clearFonts();
  [[nodiscard]] std::expected<void, StarException> setFixedFonts(String const& defaultFontName, String const& fallbackFontName, String const& emojiFontName);
private:
  [[nodiscard]] std::expected<std::reference_wrapper<Font>, StarException> getFontForCharacter(String::Char);

  Root& m_root;
  CaseInsensitiveStringMap<std::shared_ptr<Font>> m_fonts;
  String m_fontName;
  std::shared_ptr<Font> m_activeFont;
  std::shared_ptr<Font> m_defaultFont;
  std::shared_ptr<Font> m_fallbackFont;
  std::shared_ptr<Font> m_emojiFont;

  std::shared_ptr<TextureGroup> m_textureGroup;
  HashMap<GlyphDescriptor, GlyphTexture, GlyphDescriptorHash, GlyphDescriptorEqual> m_glyphs;
};

}

export module star.font_texture_group;

export namespace Star {
  using ::Star::FontTextureGroup;
}

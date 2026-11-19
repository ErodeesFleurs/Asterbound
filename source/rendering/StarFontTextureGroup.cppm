module;
import std;
import star.format;
import star.directives;
import star.string;
import star.map;
import star.font;
import star.renderer;
import star.hash;
import star.vector;
namespace Star {
class Root;

enum class GlyphDirectivePolicy : std::uint8_t {
  Default,
  Text
};

class TextPainter;
class FontTextureGroup {
public:
  // Build the complete case-insensitive registry before replacing the live set.
  class FontSet {
  public:
    [[nodiscard]] std::expected<void, StarException> insert_or_assign(String name, std::shared_ptr<Font> font);
  private:
    friend class FontTextureGroup;
    [[nodiscard]] std::expected<std::shared_ptr<Font>, StarException> find(std::string_view name) const;
    CaseInsensitiveStringMap<std::shared_ptr<Font>> m_fonts;
  };

  struct GlyphTexture {
    TexturePtr texture;
    bool colored = false;
    std::chrono::time_point<std::chrono::steady_clock, std::chrono::milliseconds> time{};
    Vec2F offset;
  };

  FontTextureGroup(Root& root, std::shared_ptr<TextureGroup> texture_group);
  [[nodiscard]] std::expected<void, StarException> replace_fonts(FontSet&& fonts, String const& default_name,
      String const& fallback_name, String const& emoji_name);
  [[nodiscard]] std::expected<void, StarException> select_font(String const& name);
  [[nodiscard]] std::expected<unsigned, StarException> glyph_width(String::Char c, unsigned size) const;
  [[nodiscard]] std::expected<std::reference_wrapper<GlyphTexture const>, StarException> glyph_texture(String::Char c,
      unsigned size, std::optional<std::reference_wrapper<Directives const>> directives = {},
      GlyphDirectivePolicy policy = GlyphDirectivePolicy::Default) &;

  // Removes glyphs that haven't been used in more than the given timeout in
  // milliseconds (configuration units).
  void cleanup(std::int64_t timeout);
private:
  friend class TextPainter;
  struct GlyphLookup {
    String::Char c;
    unsigned size;
    Directives const& directives;
    GlyphDirectivePolicy policy;
    std::shared_ptr<Font> const& font;
  };
  struct GlyphKey {
    String::Char c;
    unsigned size;
    Directives directives;
    GlyphDirectivePolicy policy;
    std::shared_ptr<Font> font;
    operator GlyphLookup() const { return {c, size, directives, policy, font}; }
  };
  struct GlyphHash {
    using is_transparent = void;
    std::size_t operator()(GlyphLookup key) const {
      return hash_of(key.c, key.size, key.directives.hash(), key.policy, std::hash<std::shared_ptr<Font>>{}(key.font));
    }
  };
  struct GlyphEqual {
    using is_transparent = void;
    bool operator()(GlyphLookup lhs, GlyphLookup rhs) const {
      return lhs.c == rhs.c && lhs.size == rhs.size && lhs.policy == rhs.policy && lhs.font == rhs.font
          && lhs.directives.equals(rhs.directives);
    }
  };
  [[nodiscard]] std::expected<std::shared_ptr<Font>, StarException> named_font(std::string_view name) const;
  [[nodiscard]] std::expected<std::reference_wrapper<std::shared_ptr<Font> const>, StarException>
      resolve_font(std::shared_ptr<Font> const& active, String::Char c) const;
  [[nodiscard]] std::expected<unsigned, StarException> glyph_width(std::shared_ptr<Font> const& active,
      String::Char c, unsigned size) const;
  [[nodiscard]] std::expected<std::reference_wrapper<GlyphTexture const>, StarException> glyph_texture(
      std::shared_ptr<Font> const& font, String::Char c, unsigned size,
      std::optional<std::reference_wrapper<Directives const>> directives, GlyphDirectivePolicy policy);

  Root& m_root;
  FontSet m_fonts;
  String m_font_name;
  std::shared_ptr<Font> m_active_font;
  std::shared_ptr<Font> m_default_font;
  std::shared_ptr<Font> m_fallback_font;
  std::shared_ptr<Font> m_emoji_font;
  std::shared_ptr<TextureGroup> m_texture_group;
  HashMap<GlyphKey, GlyphTexture, GlyphHash, GlyphEqual> m_glyphs;
};

}

export module star.font_texture_group;

export namespace Star {
  using ::Star::FontTextureGroup;
  using ::Star::GlyphDirectivePolicy;
}

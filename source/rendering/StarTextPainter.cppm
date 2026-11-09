module;
import std;
import star.format;
import star.json;
import star.color;
import star.font;
import star.list;
import star.rect;
import star.image_processing;
import star.hash;
import star.data_stream;
import star.string_view;
import star.thread;
import star.directives;
import star.bi_map;
import star.ordered_map;
import star.io_device;
import star.asset_path;
import star.ref_ptr;
import star.logging;
import star.listener;
import star.version;
import star.string;
import star.vector;
import star.text;


import star.image;
import star.renderer;
import star.font_texture_group;
import star.anchor_types;
import star.asset_source;
import star.assets;
import star.root_base;
import star.configuration;
import star.root;

namespace Star {

// deprecated in favor of explicit shadow color
enum class FontMode : std::uint8_t {
  Normal,
  Shadow
};

inline Color const& fontModeToColor(FontMode mode) {
  return mode == FontMode::Shadow ? Color::Black : Color::Clear;
}

struct TextPositioning {
  TextPositioning();

  TextPositioning(Vec2F pos,
      HorizontalAnchor hAnchor = HorizontalAnchor::LeftAnchor,
      VerticalAnchor vAnchor = VerticalAnchor::BottomAnchor,
      std::optional<unsigned> wrapWidth = {},
      std::optional<unsigned> charLimit = {});

  [[nodiscard]] static std::expected<TextPositioning, StarException> create(Json const& v);
  [[nodiscard]] std::expected<Json, StarException> toJson() const;

  TextPositioning translated(Vec2F translation) const;

  Vec2F pos;
  HorizontalAnchor hAnchor;
  VerticalAnchor vAnchor;
  std::optional<unsigned> wrapWidth;
  std::optional<unsigned> charLimit;
};

// Renders text while caching individual glyphs for fast rendering but with *no
// kerning*.
class TextPainter {
public:
  // Root must outlive this painter and its scoped reload subscription.
  [[nodiscard]] static std::expected<std::shared_ptr<TextPainter>, StarException> create(Root& root, std::shared_ptr<Renderer> renderer, std::shared_ptr<TextureGroup> textureGroup);

  [[nodiscard]] std::expected<RectF, StarException> renderText(StringView s, TextPositioning const& position);
  [[nodiscard]] std::expected<RectF, StarException> renderLine(StringView s, TextPositioning const& position);
  [[nodiscard]] std::expected<RectF, StarException> renderGlyph(String::Char c, TextPositioning const& position);

  [[nodiscard]] std::expected<RectF, StarException> determineTextSize(StringView s, TextPositioning const& position);
  [[nodiscard]] std::expected<RectF, StarException> determineLineSize(StringView s, TextPositioning const& position);
  [[nodiscard]] std::expected<RectF, StarException> determineGlyphSize(String::Char c, TextPositioning const& position);

  [[nodiscard]] std::expected<int, StarException> glyphWidth(String::Char c);
  [[nodiscard]] std::expected<int, StarException> stringWidth(StringView s, unsigned charLimit = 0);

  using WrapTextCallback = std::function<std::expected<bool, StarException>(StringView, unsigned)>;
  [[nodiscard]] std::expected<bool, StarException> processWrapText(StringView s, std::optional<std::reference_wrapper<unsigned>> wrapWidth, WrapTextCallback textFunc);

  [[nodiscard]] std::expected<List<StringView>, StarException> wrapTextViews(StringView s, std::optional<unsigned> wrapWidth);
  [[nodiscard]] std::expected<StringList, StarException> wrapText(StringView s, std::optional<unsigned> wrapWidth);

  unsigned fontSize() const;
  void setFontSize(unsigned size);
  void setLineSpacing(float lineSpacing);
  void setMode(FontMode mode);
  void setFontColor(Vec4B color);
  [[nodiscard]] std::expected<void, StarException> setProcessingDirectives(StringView directives, bool back = false);
  [[nodiscard]] std::expected<void, StarException> setFont(String const& font);
  [[nodiscard]] std::expected<std::reference_wrapper<TextStyle>, StarException> setTextStyle(TextStyle const& textStyle);
  [[nodiscard]] std::expected<void, StarException> clearTextStyle();
  [[nodiscard]] std::expected<void, StarException> addFont(std::shared_ptr<Font> const& font, String const& name);
  [[nodiscard]] std::expected<void, StarException> reloadFonts();

  void cleanup(std::int64_t textureTimeout);
  [[nodiscard]] std::expected<void, StarException> applyCommands(StringView unsplitCommands);
private:
  TextPainter(Root& root, std::shared_ptr<Renderer> renderer, std::shared_ptr<TextureGroup> textureGroup);
  [[nodiscard]] std::expected<void, StarException> modifyDirectives(Directives& directives);
  [[nodiscard]] std::expected<RectF, StarException> doRenderText(StringView s, TextPositioning const& position, bool reallyRender, std::optional<std::reference_wrapper<unsigned>> charLimit);
  [[nodiscard]] std::expected<RectF, StarException> doRenderLine(StringView s, TextPositioning const& position, bool reallyRender, std::optional<std::reference_wrapper<unsigned>> charLimit);
  [[nodiscard]] std::expected<RectF, StarException> doRenderGlyph(String::Char c, TextPositioning const& position, bool reallyRender);

  void renderPrimitives();
  [[nodiscard]] std::expected<void, StarException> renderGlyph(String::Char c, Vec2F const& screenPos, List<RenderPrimitive>& out, unsigned fontSize, float scale, Vec4B color, std::optional<std::reference_wrapper<Directives const>> processingDirectives = std::nullopt);
  [[nodiscard]] std::expected<std::shared_ptr<Font>, StarException> loadFont(String const& fontPath, std::optional<String> fontName = {});

  Root& m_root;
  std::shared_ptr<Renderer> m_renderer;
  List<RenderPrimitive> m_shadowPrimitives;
  List<RenderPrimitive> m_backPrimitives;
  List<RenderPrimitive> m_frontPrimitives;
  FontTextureGroup m_fontTextureGroup;

  TextStyle m_defaultRenderSettings;
  TextStyle m_renderSettings;
  TextStyle m_savedRenderSettings;

  String m_nonRenderedCharacters;
  std::shared_ptr<TrackerListener> m_reloadTracker;
  ListenerSubscription m_reloadSubscription;
};

}

export module star.text_painter;

export namespace Star {
  using ::Star::FontMode;
  using ::Star::fontModeToColor;
  using ::Star::TextPositioning;
  using ::Star::TextPainter;
}

module;
import std;
import star.format;
import star.json;
import star.byte_array;

import star.string;
import star.poly;
import star.list;
import star.rect;
import star.image_processing;
import star.hash;
import star.data_stream;
import star.string_view;
import star.thread;
import star.directives;
import star.asset_path;
import star.input_event;
import star.vector;
import star.set;
import star.renderer;

namespace Star {
class Image;
class Root;
class Input;
class Voice;
class TileDrawer;
struct Drawable;
struct TextStyle;
struct TextPositioning;
enum class FontMode : std::uint8_t;
class ApplicationController;
class Renderer;
class Mixer;
class TextPainter;
class AssetTextureGroup;
class DrawablePainter;
class AudioInstance;
}

import star.gui_types;
import star.key_bindings;

namespace Star {

struct GuiContextExceptionTag {
  static constexpr std::string_view name() { return "GuiContextException"; }
};
using GuiContextException = StarError<GuiContextExceptionTag, StarException>;

class GuiContext {
public:
  [[nodiscard]] static std::expected<std::shared_ptr<GuiContext>, StarException> create(Root& root, std::shared_ptr<Mixer> mixer, std::shared_ptr<ApplicationController> appController);
  ~GuiContext();

  Root& root() const;
  void bindInput(Input& input);
  [[nodiscard]] std::expected<std::reference_wrapper<Input>, StarException> input() const;
  void bindVoice(Voice& voice);
  [[nodiscard]] std::expected<std::reference_wrapper<Voice>, StarException> voice() const;
  void setTileDrawer(std::optional<std::reference_wrapper<TileDrawer>> tileDrawer);
  std::optional<std::reference_wrapper<TileDrawer>> tileDrawer() const;

  GuiContext(GuiContext const&) = delete;
  GuiContext& operator=(GuiContext const&) = delete;

  [[nodiscard]] std::expected<void, StarException> renderInit(std::shared_ptr<Renderer> renderer);
  std::shared_ptr<Mixer> const& mixer() const;
  std::shared_ptr<ApplicationController> const& applicationController() const;
  [[nodiscard]] std::expected<std::reference_wrapper<Renderer>, StarException> renderer() const;
  [[nodiscard]] std::expected<std::reference_wrapper<AssetTextureGroup>, StarException> assetTextureGroup() const;
  [[nodiscard]] std::expected<std::reference_wrapper<TextPainter>, StarException> textPainter() const;

  [[nodiscard]] std::expected<unsigned, StarException> windowWidth() const;
  [[nodiscard]] std::expected<unsigned, StarException> windowHeight() const;
  [[nodiscard]] std::expected<Vec2U, StarException> windowSize() const;
  [[nodiscard]] std::expected<Vec2U, StarException> windowInterfaceSize() const;
  float interfaceScale() const;
  void setInterfaceScale(float interfaceScale);
  RenderPreferences& renderPreferences() { return m_renderPreferences; }
  std::optional<Vec2I> mousePosition(InputEvent const& event, float pixelRatio) const;
  std::optional<Vec2I> mousePosition(InputEvent const& event) const;
  Set<InterfaceAction> actions(InputEvent const& event) const;
  Set<InterfaceAction> actionsForKey(Key key) const;
  [[nodiscard]] std::expected<void, StarException> refreshKeybindings();

  [[nodiscard]] std::expected<void, StarException> setInterfaceScissorRect(RectI const& scissor);
  [[nodiscard]] std::expected<void, StarException> resetInterfaceScissorRect();
  [[nodiscard]] std::expected<Vec2U, StarException> textureSize(AssetPath const& texName);
  [[nodiscard]] std::expected<void, StarException> drawQuad(RectF const& screenCoords, Vec4B const& color = Vec4B::filled(255));
  [[nodiscard]] std::expected<void, StarException> drawQuad(AssetPath const& texName, RectF const& screenCoords, Vec4B const& color = Vec4B::filled(255));
  [[nodiscard]] std::expected<void, StarException> drawQuad(AssetPath const& texName, Vec2F const& screenPos, float pixelRatio, Vec4B const& color = Vec4B::filled(255));
  [[nodiscard]] std::expected<void, StarException> drawQuad(AssetPath const& texName, RectF const& texCoords, RectF const& screenCoords, Vec4B const& color = Vec4B::filled(255));
  [[nodiscard]] std::expected<void, StarException> drawDrawable(Drawable drawable, Vec2F const& screenPos, float pixelRatio, Vec4B const& color = Vec4B::filled(255));
  [[nodiscard]] std::expected<void, StarException> drawLine(Vec2F const& begin, Vec2F const end, Vec4B const& color, float lineWidth = 1);
  [[nodiscard]] std::expected<void, StarException> drawPolyLines(PolyF const& poly, Vec4B const& color, float lineWidth = 1);
  [[nodiscard]] std::expected<void, StarException> drawTriangles(List<std::tuple<Vec2F, Vec2F, Vec2F>> const& triangles, Vec4B const& color);
  [[nodiscard]] std::expected<void, StarException> drawInterfaceDrawable(Drawable drawable, Vec2F const& screenPos, Vec4B const& color = Vec4B::filled(255));
  [[nodiscard]] std::expected<void, StarException> drawInterfaceLine(Vec2F const& begin, Vec2F const end, Vec4B const& color, float lineWidth = 1);
  [[nodiscard]] std::expected<void, StarException> drawInterfacePolyLines(PolyF poly, Vec4B const& color, float lineWidth = 1);
  [[nodiscard]] std::expected<void, StarException> drawInterfaceTriangles(List<std::tuple<Vec2F, Vec2F, Vec2F>> const& triangles, Vec4B const& color);
  [[nodiscard]] std::expected<void, StarException> drawInterfaceQuad(RectF const& screenCoords, Vec4B const& color = Vec4B::filled(255));
  [[nodiscard]] std::expected<void, StarException> drawInterfaceQuad(AssetPath const& texName, Vec2F const& screenPos, Vec4B const& color = Vec4B::filled(255));
  [[nodiscard]] std::expected<void, StarException> drawInterfaceQuad(AssetPath const& texName, Vec2F const& screenPos, float scale, Vec4B const& color = Vec4B::filled(255));
  [[nodiscard]] std::expected<void, StarException> drawInterfaceQuad(AssetPath const& texName, RectF const& texCoords, RectF const& screenCoords, Vec4B const& color = Vec4B::filled(255));
  [[nodiscard]] std::expected<void, StarException> drawImageStretchSet(ImageStretchSet const& imageSet, RectF const& screenPos, GuiDirection direction = GuiDirection::Horizontal, Vec4B const& color = Vec4B::filled(255));
  [[nodiscard]] std::expected<bool, StarException> trySetCursor(Drawable const& drawable, Vec2I const& offset, int pixelRatio);
  [[nodiscard]] std::expected<RectF, StarException> renderText(String const& s, TextPositioning const& positioning);
  [[nodiscard]] std::expected<RectF, StarException> renderInterfaceText(String const& s, TextPositioning const& positioning);
  [[nodiscard]] std::expected<RectF, StarException> determineTextSize(String const& s, TextPositioning const& positioning);
  [[nodiscard]] std::expected<RectF, StarException> determineInterfaceTextSize(String const& s, TextPositioning const& positioning);
  [[nodiscard]] std::expected<void, StarException> setFontSize(unsigned size);
  [[nodiscard]] std::expected<void, StarException> setFontSize(unsigned size, float pixelRatio);
  [[nodiscard]] std::expected<void, StarException> setFontColor(Vec4B const& color);
  [[nodiscard]] std::expected<void, StarException> setFontMode(FontMode mode);
  [[nodiscard]] std::expected<void, StarException> setFontProcessingDirectives(String const& directives);
  [[nodiscard]] std::expected<void, StarException> setFont(String const& font);
  [[nodiscard]] std::expected<void, StarException> setDefaultFont();
  [[nodiscard]] std::expected<std::reference_wrapper<TextStyle>, StarException> setTextStyle(TextStyle const& textStyle, float pixelRatio);
  [[nodiscard]] std::expected<std::reference_wrapper<TextStyle>, StarException> setTextStyle(TextStyle const& textStyle);
  [[nodiscard]] std::expected<void, StarException> clearTextStyle();
  [[nodiscard]] std::expected<void, StarException> setLineSpacing(float lineSpacing);
  [[nodiscard]] std::expected<void, StarException> setDefaultLineSpacing();
  [[nodiscard]] std::expected<int, StarException> stringWidth(String const& s);
  [[nodiscard]] std::expected<int, StarException> stringInterfaceWidth(String const& s);
  [[nodiscard]] std::expected<StringList, StarException> wrapText(String const& s, std::optional<unsigned> wrapWidth);
  [[nodiscard]] std::expected<StringList, StarException> wrapInterfaceText(String const& s, std::optional<unsigned> wrapWidth);
  [[nodiscard]] std::expected<void, StarException> playAudio(std::shared_ptr<AudioInstance> audioInstance);
  [[nodiscard]] std::expected<std::shared_ptr<AudioInstance>, StarException> playAudio(String const& audioAsset, int loops = 0, float volume = 1.0f, float pitch = 1.0f);
  [[nodiscard]] std::expected<void, StarException> playOptionalAudio(String const& audioAsset);
  bool shiftHeld() const;
  void setShiftHeld(bool held);
  String getClipboard() const;
  bool setClipboard(String text);
  bool setClipboardData(StringMap<ByteArray> data);
  [[nodiscard]] std::expected<bool, StarException> setClipboardImage(Image const& image, std::optional<std::reference_wrapper<ByteArray>> png, std::optional<std::reference_wrapper<String const>> path = std::nullopt);
  [[nodiscard]] std::expected<bool, StarException> setClipboardFile(String const& path);
  float getDisplayScale() const;
  [[nodiscard]] std::expected<void, StarException> cleanup();

private:
  GuiContext(Root& root, std::shared_ptr<Mixer> mixer, std::shared_ptr<ApplicationController> appController);
  Root& m_root;
  std::optional<std::reference_wrapper<Input>> m_input;
  std::optional<std::reference_wrapper<Voice>> m_voice;
  std::optional<std::reference_wrapper<TileDrawer>> m_tileDrawer;

  std::shared_ptr<Mixer> m_mixer;
  std::shared_ptr<ApplicationController> m_applicationController;
  std::shared_ptr<Renderer> m_renderer;

  std::shared_ptr<AssetTextureGroup> m_textureCollection;
  std::shared_ptr<TextPainter> m_textPainter;
  std::shared_ptr<DrawablePainter> m_drawablePainter;

  KeyBindings m_keyBindings;

  float m_interfaceScale;
  RenderPreferences m_renderPreferences;

  bool m_shiftHeld;
};

}

export module star.gui_context;

export namespace Star {
  using ::Star::GuiContextExceptionTag;
  using ::Star::GuiContext;
  using ::Star::GuiContextException;
  using ::Star::Image;
  using ::Star::Drawable;
  using ::Star::TextStyle;
  using ::Star::TextPositioning;
  using ::Star::FontMode;
  using ::Star::ApplicationController;
  using ::Star::Renderer;
  using ::Star::Mixer;
  using ::Star::TextPainter;
  using ::Star::AssetTextureGroup;
  using ::Star::DrawablePainter;
  using ::Star::AudioInstance;
}

module;
import std;
import star.type_identity;
import star.json;

import star.vector;
import star.casting;
import star.input_event;
import star.color;
import star.list;
import star.rect;
import star.image_processing;
import star.hash;
import star.data_stream;
import star.string_view;
import star.thread;
import star.directives;
import star.string;
import star.text;
import star.poly;
import star.asset_path;
import star.text_painter;
import star.drawable;
import star.widget;

import star.format;
namespace Star {

// Very simple Widget that allows easy drawing to its surface, to easily tie a
// simplified rendering / input context into the regular widget / GuiReader
// system.
class CanvasWidget : public Widget {
public:
  void const* queryType(TypeKey key) const noexcept override {
    return queryTypeOf<CanvasWidget, Widget>(this, key);
  }
  std::string_view dynamicTypeName() const noexcept override {
    return Star::typeName<CanvasWidget>();
  }

  struct ClickEvent {
    Vec2I position;
    MouseButton button;
    // True when button down, false when key up
    bool buttonDown;
  };

  struct KeyEvent {
    Key key;
    // True when key down, false when key up
    bool keyDown;
  };

  unsigned MaximumEventBuffer = 16;

  [[nodiscard]] static std::expected<std::shared_ptr<CanvasWidget>, StarException> create(GuiContext& context);

  void setCaptureMouseEvents(bool captureMouse);
  void setCaptureKeyboardEvents(bool captureKeyboard);
  void setIgnoreInterfaceScale(bool ignoreInterfaceScale);
  bool ignoreInterfaceScale() const;

  // Returns mouse position relative to the lower left of the drawing region.
  Vec2I mousePosition() const;

  // Pulls recent click events relative to the lower left of the drawing
  // region, if configured to capture mouse events
  List<ClickEvent> pullClickEvents();

  // Pulls recent key events captured by this Canvas, if configured to capture
  // key events.
  List<KeyEvent> pullKeyEvents();

  [[nodiscard]] std::expected<bool, StarException> sendEvent(InputEvent const& event) override;
  [[nodiscard]] std::expected<KeyboardCaptureMode, StarException> keyboardCaptureMode() const override;

  // Call before drawing to clear old draw data.
  void clear();

  // Clips use the same Canvas-local units and scaling as draw commands.
  void pushClipRect(RectF const& rect);
  [[nodiscard]] std::expected<void, StarException> popClipRect();

  void drawImage(String texName, Vec2F const& position, float scale = 1.0f, Vec4B const& color = Vec4B(255, 255, 255, 255));
  void drawImageRect(String texName, RectF const& texCoords, RectF const& screenCoords, Vec4B const& color);
  void drawImageCentered(String texName, Vec2F const& position, float scale = 1.0f, Vec4B const& color = Vec4B(255, 255, 255, 255));

  void drawDrawable(Drawable drawable, Vec2F const& screenPos, float scale = 1, Vec2F const& translation = Vec2F(), float finalScale = 1);
  void drawDrawables(List<Drawable> drawables, Vec2F const& screenPos, float scale = 1, Vec2F const& translation = Vec2F(), float finalScale = 1);

  // Draw an image whose texture is applied over the entire screen rect in a
  // tiled manner, so that it wraps in X and Y.
  void drawTiledImage(String texName, float textureScale, Vec2D const& offset, RectF const& screenCoords, Vec4B const& color = Vec4B::filled(255));

  void drawLine(Vec2F const& begin, Vec2F const end, Vec4B const& color = Vec4B(255, 255, 255, 255), float lineWidth = 1.0f);
  void drawRect(RectF const& coords, Vec4B const& color = Vec4B(255, 255, 255, 255));
  void drawPoly(PolyF const& poly, Vec4B const& color = Vec4B(255, 255, 255, 255), float lineWidth = 1.0f);
  void drawTriangles(List<std::tuple<Vec2F, Vec2F, Vec2F>> const& poly, Vec4B const& color = Vec4B(255, 255, 255, 255));

  void drawText(String s, TextPositioning position, unsigned fontSize, Vec4B const& color = Vec4B(255, 255, 255, 255), FontMode mode = FontMode::Normal, float lineSpacing = Star::DefaultLineSpacing, String font = "", String processingDirectives = "");
  void drawText(String s, TextPositioning position, TextStyle style);
  // Returns layout bounds in Canvas-local units without queuing a draw operation.
  [[nodiscard]] std::expected<RectF, StarException> measureText(String const& s, TextPositioning const& position, TextStyle const& style) const;

protected:
  [[nodiscard]] std::expected<void, StarException> renderImpl() override;

  [[nodiscard]] std::expected<void, StarException> renderImage(Vec2F const& renderingOffset, String const& texName, Vec2F const& position, float scale, Vec4B const& color, bool centered);
  [[nodiscard]] std::expected<void, StarException> renderImageRect(Vec2F const& renderingOffset, String const& texName, RectF const& texCoords, RectF const& screenCoords, Vec4B const& color);
  [[nodiscard]] std::expected<void, StarException> renderDrawable(Vec2F const& renderingOffset, Drawable drawable, Vec2F const& screenPos);
  [[nodiscard]] std::expected<void, StarException> renderTiledImage(Vec2F const& renderingOffset, String const& texName, float textureScale, Vec2D const& offset, RectF const& screenCoords, Vec4B const& color);
  [[nodiscard]] std::expected<void, StarException> renderLine(Vec2F const& renderingOffset, Vec2F const& begin, Vec2F const end, Vec4B const& color, float lineWidth);
  [[nodiscard]] std::expected<void, StarException> renderRect(Vec2F const& renderingOffset, RectF const& coords, Vec4B const& color);
  [[nodiscard]] std::expected<void, StarException> renderPoly(Vec2F const& renderingOffset, PolyF poly, Vec4B const& color, float lineWidth);
  [[nodiscard]] std::expected<void, StarException> renderTriangles(Vec2F const& renderingOffset, List<std::tuple<Vec2F, Vec2F, Vec2F>> const& triangles, Vec4B const& color);
  [[nodiscard]] std::expected<void, StarException> renderText(Vec2F const& renderingOffset, String const& s, TextPositioning const& position, TextStyle const& style);

private:
  explicit CanvasWidget(GuiContext& context);
  bool m_ignoreInterfaceScale;
  bool m_captureKeyboard;
  bool m_captureMouse;
  Vec2I m_mousePosition;
  List<ClickEvent> m_clickEvents;
  List<KeyEvent> m_keyEvents;

  using RectOp = std::tuple<RectF, Vec4B>;
  using ImageOp = std::tuple<String, Vec2F, float, Vec4B, bool>;
  using ImageRectOp = std::tuple<String, RectF, RectF, Vec4B>;
  using DrawableOp = std::tuple<Drawable, Vec2F>;
  using TiledImageOp = std::tuple<String, float, Vec2D, RectF, Vec4B>;
  using LineOp = std::tuple<Vec2F, Vec2F, Vec4B, float>;
  using PolyOp = std::tuple<PolyF, Vec4B, float>;
  using TrianglesOp = std::tuple<List<std::tuple<Vec2F, Vec2F, Vec2F>>, Vec4B>;
  using TextOp = std::tuple<String, TextPositioning, TextStyle>;
  struct PushClipOp {
    RectF rect;
    std::optional<std::size_t> parent;
  };
  struct PopClipOp { std::optional<RectF> rect; };

  using RenderOp = std::variant<std::monostate, RectOp, ImageOp, ImageRectOp, DrawableOp, TiledImageOp, LineOp, PolyOp,
                                TrianglesOp, TextOp, PushClipOp, PopClipOp>;
  List<RenderOp> m_renderOps;
  std::optional<std::size_t> m_clipHead;
};

}

export module star.canvas_widget;

export namespace Star {
  using ::Star::CanvasWidget;
}

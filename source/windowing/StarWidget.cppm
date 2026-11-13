module;
import std;
import star.type_identity;
import star.format;
import star.text_formatter;
import star.json;

import star.vector;
import star.casting;
import star.input_event;
import star.string;
import star.poly;
import star.list;
import star.map;

import star.rect;
namespace Star {

class GuiContext;

struct GuiExceptionTag {
  static constexpr std::string_view name() { return "GuiException"; }
};
using GuiException = StarError<GuiExceptionTag, StarException>;

class Widget;
class Pane;

enum class KeyboardCaptureMode {
  None,
  KeyEvents,
  TextInput
};

using WidgetCallbackFunc = std::function<std::expected<void, StarException>(Widget&)>;

class Widget : public std::enable_shared_from_this<Widget> {
public:
  Widget(GuiContext& context);
  virtual ~Widget();

  // Project UI capability lookup; successful queries return the adjusted
  // subobject address, independently of the Widget entry point.
  virtual void const* query_type(TypeKey key) const noexcept { return query_type_of<Widget>(this, key); }
  virtual std::string_view dynamic_type_name() const noexcept { return Star::type_name<Widget>(); }

  Widget(Widget const& copy) = delete;
  Widget& operator=(Widget const&) = delete;

  [[nodiscard]] virtual std::expected<void, StarException> render(RectI const& region) final;
  [[nodiscard]] virtual std::expected<void, StarException> update(float dt);

  GuiContext& context() const;

  // Position of widget with drawing offset (useful for drawing)
  virtual Vec2I position() const;
  // Position of widget ignoring offset (useful for moving and placing widgets
  // relative to its current position)
  virtual Vec2I relativePosition() const;
  // Set position of widget, ignoring offset
  virtual void setPosition(Vec2I const& position);

  virtual Vec2I drawingOffset() const;
  virtual void setDrawingOffset(Vec2I const& offset);

  virtual Vec2I size() const;
  virtual void setSize(Vec2I const& size);

  virtual RectI relativeBoundRect() const;
  virtual RectI screenBoundRect() const;

  virtual bool inMember(Vec2I const& position) const;

  [[nodiscard]] virtual std::expected<bool, StarException> sendEvent(InputEvent const& event);

  [[nodiscard]] virtual std::expected<void, StarException> show();
  [[nodiscard]] virtual std::expected<void, StarException> hide();
  virtual bool visibility() const;
  [[nodiscard]] virtual std::expected<void, StarException> toggleVisibility();
  [[nodiscard]] virtual std::expected<void, StarException> setVisibility(bool visibility);

  [[nodiscard]] virtual std::expected<void, StarException> mouseOver();
  [[nodiscard]] virtual std::expected<void, StarException> mouseOut();
  [[nodiscard]] virtual std::expected<void, StarException> mouseReturnStillDown();
  virtual void setMouseTransparent(bool transparent);
  virtual bool mouseTransparent();

  virtual bool active() const;

  virtual bool interactive() const;

  [[nodiscard]] virtual std::expected<bool, StarException> hasFocus() const;
  [[nodiscard]] virtual std::expected<void, StarException> focus();
  [[nodiscard]] virtual std::expected<void, StarException> blur();

  virtual std::optional<std::reference_wrapper<Widget>> parent() const;
  virtual void setParent(std::optional<std::reference_wrapper<Widget>> parent);

  virtual std::optional<std::reference_wrapper<Pane const>> window() const;
  virtual std::optional<std::reference_wrapper<Pane>> window();

  [[nodiscard]] virtual std::expected<void, StarException> addChild(String const& name, std::shared_ptr<Widget> member);
  [[nodiscard]] virtual std::expected<void, StarException> addChildAt(String const& name, std::shared_ptr<Widget> member, std::size_t at);
  virtual bool removeChild(Widget& member);
  virtual bool removeChild(String const& name);
  virtual bool removeChildAt(std::size_t at);
  virtual std::shared_ptr<Widget> getChildAt(Vec2I const& pos);
  [[nodiscard]] virtual std::expected<bool, StarException> containsChild(String const& name);
  [[nodiscard]] virtual std::expected<std::shared_ptr<Widget>, StarException> fetchChild(String const& name);
  template <typename WidgetType>
  [[nodiscard]] std::expected<std::shared_ptr<WidgetType>, StarException> fetchChild(String const& name);
  template <typename WidgetType = Widget>
  [[nodiscard]] std::expected<std::shared_ptr<WidgetType>, StarException> requireChild(String const& name);

  [[nodiscard]] virtual std::expected<std::shared_ptr<Widget>, StarException> findChild(String const& name);
  template <typename WidgetType>
  [[nodiscard]] std::expected<std::shared_ptr<WidgetType>, StarException> findChild(String const& name);

  std::shared_ptr<Widget> childPtr(Widget const& widget) const;

  virtual std::size_t numChildren() const;
  [[nodiscard]] virtual std::expected<std::shared_ptr<Widget>, StarException> getChildNum(std::size_t num) const;
  template <typename WidgetType>
  [[nodiscard]] std::expected<std::shared_ptr<WidgetType>, StarException> getChildNum(std::size_t num) const;
  virtual void removeAllChildren();

  virtual String const& name() const;
  virtual void setName(String const& name);
  String fullName() const;

  [[nodiscard]] std::expected<unsigned, StarException> windowHeight() const;
  [[nodiscard]] std::expected<unsigned, StarException> windowWidth() const;
  [[nodiscard]] std::expected<Vec2I, StarException> windowSize() const;
  virtual Vec2I screenPosition() const;
  void disableScissoring();
  void enableScissoring();
  void determineSizeFromChildren();
  void markAsContainer();

  [[nodiscard]] virtual std::expected<std::shared_ptr<Widget>, StarException> keyboardCapturer() const;
  [[nodiscard]] virtual std::expected<KeyboardCaptureMode, StarException> keyboardCaptureMode() const;
  [[nodiscard]] virtual std::expected<std::optional<std::pair<RectI, int>>, StarException> keyboardCaptureArea() const;

  void setData(Json const& data);
  Json const& data();

  [[nodiscard]] std::expected<bool, StarException> setLabel(String const& name, String const& value);

protected:
  friend void write_text(TextBuffer& buffer, Widget const& widget);
  String toStringImpl(int indentLevel) const;

  [[nodiscard]] virtual std::expected<void, StarException> renderImpl();
  [[nodiscard]] virtual std::expected<void, StarException> drawChildren();
  [[nodiscard]] std::expected<bool, StarException> setupDrawRegion(RectI const& region);
  virtual RectI getScissorRect() const;
  virtual RectI noScissor() const;

  // The owning parent detaches children before removing its last owner or
  // destroying itself; reparenting first removes the previous owner's child.
  std::optional<std::reference_wrapper<Widget>> m_parent;

  GuiContext& m_context;

  bool m_visible;
  PolyF m_boundPoly;

  Vec2I m_position;
  Vec2I m_size;
  RectI m_drawingArea;
  Vec2I m_drawingOffset;
  String m_name;
  List<std::shared_ptr<Widget>> m_members;
  StringMap<std::shared_ptr<Widget>> m_memberHash;
  Vec2I m_memberSize;
  bool m_focus;
  bool m_doScissor;
  bool m_container;
  bool m_mouseTransparent;

  Json m_data;
};

void write_text(TextBuffer& buffer, Widget const& widget);

template <typename WidgetType>
[[nodiscard]] std::expected<std::shared_ptr<WidgetType>, StarException> Widget::getChildNum(std::size_t num) const {
  auto child = getChildNum(num);
  if (!child)
    return std::unexpected(std::move(child.error()));
  return as<WidgetType>(*child);
}

template <typename WidgetType>
[[nodiscard]] std::expected<std::shared_ptr<WidgetType>, StarException> Widget::fetchChild(String const& name) {
  auto child = fetchChild(name);
  if (!child)
    return std::unexpected(std::move(child.error()));
  return as<WidgetType>(*child);
}

template <typename WidgetType>
[[nodiscard]] std::expected<std::shared_ptr<WidgetType>, StarException> Widget::requireChild(String const& name) {
  auto child = fetchChild(name);
  if (!child)
    return std::unexpected(std::move(child.error()));
  if (!*child)
    return std::unexpected(GuiException::format("Required widget '{}' is missing", name));
  auto typed = as<WidgetType>(*child);
  if (!typed)
    return std::unexpected(GuiException::format("Required widget '{}' has an incompatible type", name));
  return typed;
}

template <typename WidgetType>
[[nodiscard]] std::expected<std::shared_ptr<WidgetType>, StarException> Widget::findChild(String const& name) {
  auto child = findChild(name);
  if (!child)
    return std::unexpected(std::move(child.error()));
  return as<WidgetType>(*child);
}

}

export module star.widget;

export template <> struct std::formatter<Star::Widget> : Star::TextFormatter {};

export namespace Star {
  using ::Star::GuiExceptionTag;
  using ::Star::KeyboardCaptureMode;
  using ::Star::Widget;
  using ::Star::Pane;
  using ::Star::GuiException;
  using ::Star::WidgetCallbackFunc;
  using ::Star::GuiContext;
  using ::Star::write_text;
}

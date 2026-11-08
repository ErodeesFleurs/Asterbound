import std;
import star.json;
import star.vector;
import star.casting;
import star.input_event;
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
import star.ref_ptr;
import star.logging;
import star.format;
import star.version;
import star.string;
import star.text;
import star.asset_path;
import star.listener;
import star.game_types;
import star.set;
import star.audio;
import star.map;

import star.application;
import star.statistics_service;
import star.p2p_networking_service;
import star.user_generated_content_service;
import star.desktop_service;
import star.image;
import star.application_controller;
import star.renderer;
import star.asset_source;
import star.assets;
import star.root_base;
import star.configuration;
import star.root;
import star.font_texture_group;
import star.anchor_types;
import star.text_painter;
import star.drawable;
import star.asset_texture_group;
import star.drawable_painter;
import star.gui_types;
import star.key_bindings;
import star.mixer;
import star.gui_context;
import star.widget;
import star.pane;

import star.label_widget;
import star.layout;
import star.flow_layout;

import star.algorithm;
namespace Star {

Widget::Widget(GuiContext& context) : m_context(context) {
  m_visible = true;
  m_focus = false;
  m_doScissor = true;
  m_container = false;
  m_mouseTransparent = false;
}

Widget::~Widget() {
  removeAllChildren();
}

std::expected<void, StarException> Widget::update(float dt) {
  for (auto& widget : m_members) {
    auto result = widget->update(dt);
    if (!result)
      return std::unexpected(std::move(result.error()));
  }
  return {};
}

GuiContext& Widget::context() const {
  return m_context;
}

std::expected<void, StarException> Widget::render(RectI const& region) {
  if (!m_visible)
    return {};

  auto drawRegion = setupDrawRegion(region);
  if (!drawRegion)
    return std::unexpected(std::move(drawRegion.error()));
  if (!*drawRegion)
    return {};

  auto rendered = renderImpl();
  if (!rendered)
    return std::unexpected(std::move(rendered.error()));
  return drawChildren();
}

std::expected<void, StarException> Widget::renderImpl() { return {}; }

std::expected<void, StarException> Widget::drawChildren() {
  for (auto const& child : m_members) {
    auto result = child->render(m_drawingArea);
    if (!result)
      return std::unexpected(std::move(result.error()));
  }
  return {};
}

Vec2I Widget::position() const {
  return m_position + m_drawingOffset;
}

Vec2I Widget::relativePosition() const {
  return m_position;
}

std::expected<bool, StarException> Widget::setupDrawRegion(RectI const& region) {
  RectI scissorRect;
  if (m_doScissor) {
    scissorRect = getScissorRect();
  } else {
    scissorRect = noScissor();
  }
  m_drawingArea = scissorRect.limited(region);
  if (m_drawingArea.isEmpty())
    return false;

  auto scissor = m_context.setInterfaceScissorRect(m_drawingArea);
  if (!scissor)
    return std::unexpected(std::move(scissor.error()));
  return true;
}

Vec2I Widget::drawingOffset() const {
  return m_drawingOffset;
}

void Widget::setDrawingOffset(Vec2I const& offset) {
  m_drawingOffset = offset;
}

RectI Widget::getScissorRect() const {
  return screenBoundRect();
}

RectI Widget::noScissor() const {
  return RectI::inf();
}

void Widget::disableScissoring() {
  m_doScissor = false;
}

void Widget::enableScissoring() {
  m_doScissor = true;
}

void Widget::setPosition(Vec2I const& position) {
  m_position = position;
}

Vec2I Widget::size() const {
  return m_size;
}

void Widget::setSize(Vec2I const& size) {
  m_size = size;
}

RectI Widget::relativeBoundRect() const {
  return RectI::withSize(relativePosition(), size());
}

RectI Widget::screenBoundRect() const {
  return relativeBoundRect().translated(screenPosition() - relativePosition());
}

void Widget::determineSizeFromChildren() {
  Vec2I max;

  for (auto& child : m_members) {
    Vec2I childMax = child->position() + child->size();
    max = max.piecewise_max(childMax);
  }

  setSize(max);
}

void Widget::markAsContainer() {
  m_container = true;
}

std::expected<std::shared_ptr<Widget>, StarException> Widget::keyboardCapturer() const {
  if (active()) {
    for (auto const& member : m_members) {
      auto mode = member->keyboardCaptureMode();
      if (!mode)
        return std::unexpected(std::move(mode.error()));
      if (*mode != KeyboardCaptureMode::None)
        return member;
      auto capturer = member->keyboardCapturer();
      if (!capturer)
        return std::unexpected(std::move(capturer.error()));
      if (*capturer)
        return capturer;
    }
  }
  return std::shared_ptr<Widget>{};
}

std::expected<KeyboardCaptureMode, StarException> Widget::keyboardCaptureMode() const {
  return KeyboardCaptureMode::None;
}

std::expected<std::optional<std::pair<RectI, int>>, StarException> Widget::keyboardCaptureArea() const {
  return {};
}

void Widget::setData(Json const& data) {
  m_data = data;
}

Json const& Widget::data() {
  return m_data;
}

Vec2I Widget::screenPosition() const {
  if (m_parent) {
    return m_parent->get().screenPosition() + position();
  } else {
    return position();
  }
}

bool Widget::inMember(Vec2I const& position) const {
  if (!m_visible)
    return false;

  if (m_mouseTransparent)
    return false;

  if (!m_drawingArea.isNull() && !m_drawingArea.contains(Vec2I::floor(position)))
    return false;

  if (m_container) {
    for (auto child : m_members)
      if (child->inMember(position))
        return true;
  } else {
    return screenBoundRect().contains(position);
  }

  return false;
}

std::expected<bool, StarException> Widget::sendEvent(InputEvent const& event) {
  if (!m_visible)
    return false;

  for (auto const& child : std::views::reverse(m_members)) {
    auto handled = child->sendEvent(event);
    if (!handled)
      return std::unexpected(std::move(handled.error()));
    if (*handled)
      return true;
  }

  return false;
}

std::expected<void, StarException> Widget::mouseOver() { return {}; }

std::expected<void, StarException> Widget::mouseOut() { return {}; }

std::expected<void, StarException> Widget::mouseReturnStillDown() { return {}; }

void Widget::setMouseTransparent(bool transparent) {
  m_mouseTransparent = transparent;
}

bool Widget::mouseTransparent() {
  return m_mouseTransparent;
}

std::optional<std::reference_wrapper<Widget>> Widget::parent() const {
  return m_parent;
}

void Widget::setParent(std::optional<std::reference_wrapper<Widget>> parent) {
  m_parent = parent;
}

std::expected<void, StarException> Widget::show() {
  m_visible = true;
  return {};
}

std::expected<void, StarException> Widget::hide() {
  m_visible = false;
  return {};
}

bool Widget::visibility() const {
  return m_visible;
}

std::expected<void, StarException> Widget::toggleVisibility() {
  m_visible = !m_visible;
  return {};
}

std::expected<void, StarException> Widget::setVisibility(bool visibility) {
  return visibility ? show() : hide();
}

bool Widget::active() const {
  return m_visible;
}

bool Widget::interactive() const {
  return true;
}

std::expected<bool, StarException> Widget::hasFocus() const {
  return m_focus;
}

std::expected<void, StarException> Widget::focus() {
  if (auto w = window()) {
    auto result = w->get().setFocus(*this);
    if (!result)
      return std::unexpected(std::move(result.error()));
  }
  m_focus = true;
  return {};
}

std::expected<void, StarException> Widget::blur() {
  m_focus = false;
  if (auto w = window())
    w->get().removeFocus(*this);
  return {};
}

std::expected<unsigned, StarException> Widget::windowHeight() const {
  return context().windowHeight();
}

std::expected<unsigned, StarException> Widget::windowWidth() const {
  return context().windowWidth();
}

std::expected<Vec2I, StarException> Widget::windowSize() const {
  auto size = context().windowSize();
  if (!size)
    return std::unexpected(std::move(size.error()));
  return Vec2I(*size);
}

std::optional<std::reference_wrapper<Pane>> Widget::window() {
  if (m_parent)
    return m_parent->get().window();
  return std::nullopt;
}

std::optional<std::reference_wrapper<Pane const>> Widget::window() const {
  if (m_parent)
    return std::as_const(m_parent->get()).window();
  return std::nullopt;
}

std::expected<void, StarException> Widget::addChild(String const& name, std::shared_ptr<Widget> member) {
  if (auto previous = member->parent())
    previous->get().removeChild(*member);
  member->setName(name);
  m_members.push_back(member);
  m_memberHash[member->name()] = member;
  member->setParent(std::ref(*this));
  return {};
}

std::expected<void, StarException> Widget::addChildAt(String const& name, std::shared_ptr<Widget> member, std::size_t at) {
  if (at > m_members.size())
    return std::unexpected(GuiException("Attempted to insert item after the end of the list."));
  if (auto previous = member->parent()) {
    if (std::addressof(previous->get()) == this) {
      for (std::size_t i = 0; i < at; ++i) {
        if (m_members[i] == member) {
          --at;
          break;
        }
      }
    }
    previous->get().removeChild(*member);
  }

  m_members.insert(m_members.begin() + at, member);
  m_memberHash[name] = member;
  member->setName(name);
  member->setParent(std::ref(*this));
  return {};
}

bool Widget::removeChild(Widget& member) {
  for (auto child = m_members.begin(); child != m_members.end(); ++child) {
    if (std::addressof(**child) == std::addressof(member)) {
      m_memberHash.erase(member.name());
      (*child)->setParent(std::nullopt);
      m_members.erase(child);
      return true;
    } else {
      if ((*child)->removeChild(member))
        return true;
    }
  }

  return false;
}

bool Widget::removeChild(String const& name) {
  m_memberHash.erase(name);
  for (auto child = m_members.begin(); child != m_members.end(); ++child) {
    auto ptr = *child;
    if (ptr->name() == name) {
      m_members.erase(child);
      ptr->setParent(std::nullopt);
      return true;
    }
  }

  return false;
}

bool Widget::removeChildAt(std::size_t at) {
  if (at >= m_members.size())
    return false;

  m_memberHash.erase(m_members[at]->name());

  m_members[at]->setParent(std::nullopt);
  m_members.erase(m_members.begin() + at);
  return true;
}

void Widget::removeAllChildren() {
  for (auto child : m_members)
    child->setParent(std::nullopt);

  m_members.clear();
  m_memberHash.clear();
}

std::expected<bool, StarException> Widget::containsChild(String const& name) {
  auto child = fetchChild(name);
  if (!child)
    return std::unexpected(std::move(child.error()));
  return static_cast<bool>(*child);
}

std::expected<std::shared_ptr<Widget>, StarException> Widget::fetchChild(String const& name) {
  auto dotted = name.contains(".");
  if (!dotted)
    return std::unexpected(std::move(dotted.error()));
  if (*dotted) {
    auto nameList = name.split(".", 1);
    if (!nameList)
      return std::unexpected(std::move(nameList.error()));
    if (auto child = m_memberHash.value((*nameList)[0], {}))
      return child->fetchChild((*nameList)[1]);
  } else {
    if (auto child = m_memberHash.value(name, {}))
      return child;
  }
  return std::shared_ptr<Widget>{};
}

std::expected<std::shared_ptr<Widget>, StarException> Widget::findChild(String const& name) {
  auto found = fetchChild(name);
  if (!found)
    return std::unexpected(std::move(found.error()));
  if (*found)
    return found;
  for (auto const& child : m_members) {
    auto nested = child->findChild(name);
    if (!nested)
      return std::unexpected(std::move(nested.error()));
    if (*nested)
      return nested;
  }
  return std::shared_ptr<Widget>{};
}

std::shared_ptr<Widget> Widget::childPtr(Widget const& child) const {
  for (auto const& m : m_members) {
    if (std::addressof(*m) == std::addressof(child))
      return m;
    if (auto c = m->childPtr(child))
      return c;
  }
  return {};
}

std::shared_ptr<Widget> Widget::getChildAt(Vec2I const& pos) {
  for (auto child : std::views::reverse(m_members)) {
    if (child->inMember(pos)) {
      auto res = child->getChildAt(pos);
      if (res) {
        return res;
      }
      return child;
    }
  }

  return {};
}

std::size_t Widget::numChildren() const {
  return m_members.size();
}

std::expected<std::shared_ptr<Widget>, StarException> Widget::getChildNum(std::size_t num) const {
  if (num >= m_members.size())
    return std::unexpected(GuiException::format("Widget child index {} is out of range", num));
  return m_members[num];
}

String const& Widget::name() const {
  return m_name;
}

void Widget::setName(String const& name) {
  m_name = name;
}

String Widget::fullName() const {
  if (m_parent) {
    return m_parent->get().fullName() + "." + name();
  }

  return name();
}

String Widget::toStringImpl(int indentLevel) const {
  auto leader = String(" ") * indentLevel;
  String childrenString;
  for (auto const& child : m_members) {
    childrenString.append(child->toStringImpl(indentLevel + 4));
  }
  String output = strf(R"OUTPUT({}{} : {{
{}  address : {},
{}  visible : {},
{}  position : {},
{}  size : {},
{}  children : {{
{}
{}  }}
{}}}
)OUTPUT",
      leader,
      m_name,
      leader,
      static_cast<void const*>(this),
      leader,
      m_visible ? "true" : "false",
      leader,
      m_position,
      leader,
      m_size,
      leader,
      childrenString,
      leader,
      leader);

  return output;
}

std::expected<bool, StarException> Widget::setLabel(String const& name, String const& value) {
  auto child = fetchChild(name);
  if (!child)
    return std::unexpected(std::move(child.error()));
  if (auto label = as<LabelWidget>(*child)) {
    auto result = label->setText(value);
    if (!result)
      return std::unexpected(std::move(result.error()));
    return true;
  }
  return false;
}

std::ostream& operator<<(std::ostream& os, Widget const& widget) {
  os << widget.toStringImpl(0);
  return os;
}

}

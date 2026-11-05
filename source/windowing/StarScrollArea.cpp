import std;
import star.format;
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
import star.button_group;
import star.button_widget;
import star.image_widget;
import star.scroll_area;

import star.math_common;
namespace Star {

// TODO: I hate these hardcoded values.  Please smite with fire.
static int const ScrollAreaBorder = 9;
static int const ScrollButtonStackSize = 6;
static int const ScrollThumbSize = 3;
static int const ScrollThumbOverhead = ScrollThumbSize + ScrollThumbSize;
static int const ScrollBarTrackOverhead = ScrollButtonStackSize + ScrollButtonStackSize + ScrollThumbOverhead;
static std::chrono::milliseconds const ScrollAdvanceTimer{100};

ScrollThumb::ScrollThumb(GuiContext& context, GuiDirection direction) : Widget(context) {
  m_hovered = false;
  m_pressed = false;
  m_direction = direction;
}

std::expected<std::shared_ptr<ScrollThumb>, StarException> ScrollThumb::create(GuiContext& context, GuiDirection direction) {
  auto thumb = std::shared_ptr<ScrollThumb>(new ScrollThumb(context, direction));
  auto assets = context.root().assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  auto images = (*assets)->json("/interface.config:scrollArea.thumbs");
  if (!images)
    return std::unexpected(std::move(images.error()));
  auto configured = thumb->setImages(*images);
  if (!configured)
    return std::unexpected(std::move(configured.error()));
  return thumb;
}

void ScrollThumb::setImages(ImageStretchSet const& base, ImageStretchSet const& hover, ImageStretchSet const& pressed) {
  m_baseThumb = base;
  m_hoverThumb = hover;
  m_pressedThumb = pressed;
}

std::expected<void, StarException> ScrollThumb::setImages(Json const& images) {
  auto direction = images.get(m_direction == GuiDirection::Vertical ? std::string_view("vertical") : std::string_view("horizontal"));
  if (!direction)
    return std::unexpected(std::move(direction.error()));
  ImageStretchSet base{}, hover{}, pressed{};
  for (auto entry : {std::pair{std::string_view("base"), std::ref(base)}, std::pair{std::string_view("hover"), std::ref(hover)}, std::pair{std::string_view("pressed"), std::ref(pressed)}}) {
    auto state = direction->get(entry.first);
    if (!state)
      return std::unexpected(std::move(state.error()));
    auto begin = state->getString("begin");
    if (!begin)
      return std::unexpected(std::move(begin.error()));
    auto end = state->getString("end");
    if (!end)
      return std::unexpected(std::move(end.error()));
    auto inner = state->getString("inner");
    if (!inner)
      return std::unexpected(std::move(inner.error()));
    entry.second.get().begin = std::move(*begin);
    entry.second.get().end = std::move(*end);
    entry.second.get().inner = std::move(*inner);
  }
  setImages(base, hover, pressed);
  return {};
}

bool ScrollThumb::isHovered() const {
  return m_hovered;
}

bool ScrollThumb::isPressed() const {
  return m_pressed;
}

void ScrollThumb::setHovered(bool hovered) {
  m_hovered = hovered;
}

void ScrollThumb::setPressed(bool pressed) {
  m_pressed = pressed;
}

std::expected<void, StarException> ScrollThumb::mouseOver() {
  setHovered(true);
  return {};
}

std::expected<void, StarException> ScrollThumb::mouseOut() {
  setHovered(false);
  return {};
}

std::expected<void, StarException> ScrollThumb::renderImpl() {
  ImageStretchSet workingSet = m_baseThumb;
  if (isHovered())
    workingSet = m_hoverThumb;
  if (isPressed())
    workingSet = m_pressedThumb;

  if (workingSet.fullyPopulated()) {
    return context().drawImageStretchSet(workingSet,
        RectF::withSize(Vec2F(screenPosition()), Vec2F(size())),
        m_direction);
  }
  return {};
}

std::expected<Vec2U, StarException> ScrollThumb::baseSize() const {
  return context().textureSize(m_baseThumb.begin);
}

ScrollBar::ScrollBar(GuiContext& context, GuiDirection direction) : Widget(context), m_direction(direction) {}

std::expected<std::shared_ptr<ScrollBar>, StarException> ScrollBar::create(GuiContext& context, GuiDirection direction, WidgetCallbackFunc forward, WidgetCallbackFunc backward) {
  auto bar = std::shared_ptr<ScrollBar>(new ScrollBar(context, direction));
  auto initialized = bar->initialize(std::move(forward), std::move(backward));
  if (!initialized)
    return std::unexpected(std::move(initialized.error()));
  return bar;
}

std::expected<void, StarException> ScrollBar::initialize(WidgetCallbackFunc forward, WidgetCallbackFunc backward) {
  auto forwardButton = ButtonWidget::create(context());
  if (!forwardButton)
    return std::unexpected(std::move(forwardButton.error()));
  m_forward = *forwardButton;
  m_forward->setCallback(std::move(forward));
  m_forward->setSustainCallbackOnDownHold(true);
  m_forward->setPressedOffset({0, 0});
  auto backwardButton = ButtonWidget::create(context());
  if (!backwardButton)
    return std::unexpected(std::move(backwardButton.error()));
  m_backward = *backwardButton;
  m_backward->setCallback(std::move(backward));
  m_backward->setSustainCallbackOnDownHold(true);
  m_backward->setPressedOffset({0, 0});
  auto thumb = ScrollThumb::create(context(), m_direction);
  if (!thumb)
    return std::unexpected(std::move(thumb.error()));
  m_thumb = *thumb;
  auto assets = context().root().assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  auto images = (*assets)->json("/interface.config:scrollArea.buttons");
  if (!images)
    return std::unexpected(std::move(images.error()));
  auto configured = setButtonImages(*images);
  if (!configured)
    return std::unexpected(std::move(configured.error()));
  auto addedThumb = addChild("thumb", m_thumb);
  if (!addedThumb)
    return std::unexpected(std::move(addedThumb.error()));
  auto addedForward = addChild("forward", m_forward);
  if (!addedForward)
    return std::unexpected(std::move(addedForward.error()));
  return addChild("backward", m_backward);
}

std::expected<void, StarException> ScrollBar::setButtonImages(Json const& images) {
  auto direction = images.get(m_direction == GuiDirection::Vertical ? std::string_view("vertical") : std::string_view("horizontal"));
  if (!direction)
    return std::unexpected(std::move(direction.error()));
  for (auto entry : {std::pair{std::string_view("forward"), std::ref(*m_forward)}, std::pair{std::string_view("backward"), std::ref(*m_backward)}}) {
    auto state = direction->get(entry.first);
    if (!state)
      return std::unexpected(std::move(state.error()));
    auto base = state->getString("base");
    if (!base)
      return std::unexpected(std::move(base.error()));
    auto hover = state->getString("hover");
    if (!hover)
      return std::unexpected(std::move(hover.error()));
    auto pressed = state->getString("pressed");
    if (!pressed)
      return std::unexpected(std::move(pressed.error()));
    auto configured = entry.second.get().setImages(*base, *hover, *pressed);
    if (!configured)
      return std::unexpected(std::move(configured.error()));
  }
  return {};
}

Vec2I ScrollBar::size() const {
  if (m_parent)
    return m_parent->get().size();
  return {};
}

std::expected<int, StarException> ScrollBar::trackSize() const {
  if (m_parent) {
    auto scrollArea = asRef<ScrollArea>(m_parent->get());
    if (!scrollArea)
      return std::unexpected(GuiException("Scroll bar has a non-scroll-area parent"));
    auto size = scrollArea->get().size();
    if (m_direction == GuiDirection::Vertical) {
      if (scrollArea->get().horizontalScroll()) {
        return size[1] - (ScrollBarTrackOverhead + ScrollAreaBorder);
      } else {
        return size[1] - ScrollBarTrackOverhead;
      }
    } else {
      return size[0] - ScrollBarTrackOverhead;
    }
  }

  return std::unexpected(GuiException("Somehow have a Scroll Bar without a parent."));
}

std::expected<float, StarException> ScrollBar::sizeRatio() const {
  if (m_parent) {
    auto scrollArea = asRef<ScrollArea>(m_parent->get());
    if (!scrollArea)
      return std::unexpected(GuiException("Scroll bar has a non-scroll-area parent"));
    if (m_direction == GuiDirection::Vertical) {
      return scrollArea->get().contentSize()[1] / (float)(scrollArea->get().areaSize()[1]);
    } else {
      return scrollArea->get().contentSize()[0] / (float)(scrollArea->get().areaSize()[0]);
    }
  }

  return std::unexpected(GuiException("Somehow have a Scroll Bar without a parent."));
}

std::expected<float, StarException> ScrollBar::scrollRatio() const {
  if (m_parent) {
    auto scrollArea = asRef<ScrollArea>(m_parent->get());
    if (!scrollArea)
      return std::unexpected(GuiException("Scroll bar has a non-scroll-area parent"));
    if (m_direction == GuiDirection::Vertical) {
      if (scrollArea->get().maxScrollPosition()[1] == 0)
        return 0;
      return scrollArea->get().scrollOffset()[1] / (float)(scrollArea->get().maxScrollPosition()[1]);
    } else {
      if (scrollArea->get().maxScrollPosition()[0] == 0)
        return 0;
      return scrollArea->get().scrollOffset()[0] / (float)(scrollArea->get().maxScrollPosition()[0]);
    }
  }
  return 0;
}

std::shared_ptr<ButtonWidget> ScrollBar::forwardButton() const {
  return m_forward;
}

std::shared_ptr<ButtonWidget> ScrollBar::backwardButton() const {
  return m_backward;
}

std::shared_ptr<ScrollThumb> ScrollBar::thumb() const {
  return m_thumb;
}

std::expected<void, StarException> ScrollBar::drawChildren() {
  if (m_parent) {
    auto scrollArea = asRef<ScrollArea>(m_parent->get());
    if (!scrollArea)
      return std::unexpected(GuiException("Scroll bar has a non-scroll-area parent"));
    auto sizeRatioResult = sizeRatio();
    if (!sizeRatioResult)
      return std::unexpected(std::move(sizeRatioResult.error()));
    auto track = trackSize();
    if (!track)
      return std::unexpected(std::move(track.error()));
    auto thumbSize = m_thumb->baseSize();
    if (!thumbSize)
      return std::unexpected(std::move(thumbSize.error()));
    float ratio = *sizeRatioResult;
    if (ratio < 1)
      ratio = 1;

    int innerSize = static_cast<int>(std::max(0.0f, std::ceil(*track / ratio)));
    auto offsetRatio = scrollRatio();
    if (!offsetRatio)
      return std::unexpected(std::move(offsetRatio.error()));
    int offsetBegin = static_cast<int>(std::ceil((*track - innerSize) * *offsetRatio));
    innerSize += ScrollThumbOverhead;

    if (m_direction == GuiDirection::Vertical) {
      if (scrollArea->get().horizontalScroll()) {
        m_forward->setPosition(m_parent->get().size() - Vec2I(ScrollAreaBorder, ScrollButtonStackSize));
        m_backward->setPosition({m_parent->get().size()[0] - ScrollAreaBorder, ScrollAreaBorder});
        m_thumb->setPosition({m_parent->get().size()[0] - ScrollAreaBorder, ScrollAreaBorder + ScrollButtonStackSize + offsetBegin});
      } else {
        m_forward->setPosition(m_parent->get().size() - Vec2I(ScrollAreaBorder, ScrollButtonStackSize));
        m_backward->setPosition({m_parent->get().size()[0] - ScrollAreaBorder, 0});
        m_thumb->setPosition({m_parent->get().size()[0] - ScrollAreaBorder, ScrollButtonStackSize + offsetBegin});
      }
      m_thumb->setSize(Vec2I((*thumbSize)[0], innerSize));
    } else {
      m_forward->setPosition({m_parent->get().size()[0] - ScrollButtonStackSize, 0});
      m_backward->setPosition({0, 0});
      m_thumb->setPosition({ScrollButtonStackSize + offsetBegin, 0});
      m_thumb->setSize(Vec2I(innerSize, (*thumbSize)[1]));
    }

    for (auto child : m_members) {
      auto rendered = child->render(m_drawingArea);
      if (!rendered)
        return std::unexpected(std::move(rendered.error()));
    }
  }
  return {};
}

std::expected<Vec2I, StarException> ScrollBar::offsetFromThumbPosition(Vec2I const& thumbPosition) const {
  auto scrollArea = m_parent ? asRef<ScrollArea>(m_parent->get()) : std::nullopt;
  if (!scrollArea)
    return std::unexpected(GuiException("Scroll bar has no scroll-area parent"));
  auto track = trackSize();
  if (!track)
    return std::unexpected(std::move(track.error()));
  if (m_direction == GuiDirection::Vertical) {
    int scrollSpan = *track - m_thumb->size()[1];
    int scrollOffset = clamp((thumbPosition[1] - ScrollButtonStackSize), 0, scrollSpan);
    float scrollRatio = (float)scrollOffset / (float)scrollSpan;
    return Vec2I{scrollArea->get().scrollOffset()[0], static_cast<int>(std::ceil(scrollArea->get().maxScrollPosition()[1] * scrollRatio))};
  } else {
    int scrollSpan = *track - m_thumb->size()[0];
    int scrollOffset = clamp((thumbPosition[0] - ScrollButtonStackSize), 0, scrollSpan);
    float scrollRatio = (float)scrollOffset / (float)scrollSpan;
    return Vec2I{static_cast<int>(std::ceil(scrollArea->get().maxScrollPosition()[0] * scrollRatio)), scrollArea->get().scrollOffset()[1]};
  }
}

ScrollArea::ScrollArea(GuiContext& context) : Widget(context), m_buttonAdvance(0),
    m_advanceLimiter(std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() + std::chrono::microseconds(500))), m_dragActive(false),
    m_horizontalScroll(false), m_verticalScroll(true), m_updatesChildren(false) {}

std::expected<std::shared_ptr<ScrollArea>, StarException> ScrollArea::create(GuiContext& context) {
  auto area = std::shared_ptr<ScrollArea>(new ScrollArea(context));
  auto initialized = area->initialize();
  if (!initialized)
    return std::unexpected(std::move(initialized.error()));
  return area;
}

std::expected<void, StarException> ScrollArea::initialize() {
  auto assets = context().root().assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  auto json = (*assets)->json("/interface.config:scrollArea.buttonAdvance");
  if (!json)
    return std::unexpected(std::move(json.error()));
  auto advance = json->toInt();
  if (!advance)
    return std::unexpected(std::move(advance.error()));
  m_buttonAdvance = *advance;
  auto vertical = ScrollBar::create(context(), GuiDirection::Vertical,
      [owner = std::weak_ptr<ScrollArea>(as<ScrollArea>(shared_from_this()))](Widget&) -> std::expected<void, StarException> {
        auto area = owner.lock();
        if (!area) return std::unexpected(GuiException("Scroll area callback target no longer exists"));
        area->scrollAreaBy({0, area->advanceFactorHelper()});
        return {};
      },
      [owner = std::weak_ptr<ScrollArea>(as<ScrollArea>(shared_from_this()))](Widget&) -> std::expected<void, StarException> {
        auto area = owner.lock();
        if (!area) return std::unexpected(GuiException("Scroll area callback target no longer exists"));
        area->scrollAreaBy({0, -area->advanceFactorHelper()});
        return {};
      });
  if (!vertical)
    return std::unexpected(std::move(vertical.error()));
  m_vBar = *vertical;
  auto horizontal = ScrollBar::create(context(), GuiDirection::Horizontal,
      [owner = std::weak_ptr<ScrollArea>(as<ScrollArea>(shared_from_this()))](Widget&) -> std::expected<void, StarException> {
        auto area = owner.lock();
        if (!area) return std::unexpected(GuiException("Scroll area callback target no longer exists"));
        area->scrollAreaBy({area->advanceFactorHelper(), 0});
        return {};
      },
      [owner = std::weak_ptr<ScrollArea>(as<ScrollArea>(shared_from_this()))](Widget&) -> std::expected<void, StarException> {
        auto area = owner.lock();
        if (!area) return std::unexpected(GuiException("Scroll area callback target no longer exists"));
        area->scrollAreaBy({-area->advanceFactorHelper(), 0});
        return {};
      });
  if (!horizontal)
    return std::unexpected(std::move(horizontal.error()));
  m_hBar = *horizontal;
  auto addedVertical = addChild("vScrollBar", m_vBar);
  if (!addedVertical)
    return std::unexpected(std::move(addedVertical.error()));
  return addChild("hScrollBar", m_hBar);
}

std::expected<void, StarException> ScrollArea::setButtonImages(Json const& images) {
  auto vertical = m_vBar->setButtonImages(images);
  if (!vertical)
    return std::unexpected(std::move(vertical.error()));
  return m_hBar->setButtonImages(images);
}

std::expected<void, StarException> ScrollArea::setThumbImages(Json const& images) {
  auto vertical = m_vBar->thumb()->setImages(images);
  if (!vertical)
    return std::unexpected(std::move(vertical.error()));
  return m_hBar->thumb()->setImages(images);
}

RectI ScrollArea::contentBoundRect() const {
  RectI res = RectI::null();
  for (auto child : m_members) {
    if (child == m_vBar || child == m_hBar) // scroll bars don't count
      continue;
    if (!child->active()) // neither do hidden members
      continue;
    res.combine(child->relativeBoundRect());
  }
  res.setMax({res.max()[0], res.max()[1] + 1});
  return res;
}

Vec2I ScrollArea::contentSize() const {
  return contentBoundRect().size();
}

Vec2I ScrollArea::areaSize() const {
  Vec2I s = size();
  if (horizontalScroll())
    s[1] -= ScrollAreaBorder;
  if (verticalScroll())
    s[0] -= ScrollAreaBorder;
  return s;
}

void ScrollArea::scrollAreaBy(Vec2I const& offset) {
  m_scrollOffset += offset;
}

Vec2I ScrollArea::scrollOffset() const {
  return m_scrollOffset;
}

bool ScrollArea::horizontalScroll() const {
  return m_horizontalScroll;
}

void ScrollArea::setHorizontalScroll(bool horizontal) {
  m_horizontalScroll = horizontal;
}

bool ScrollArea::verticalScroll() const {
  return m_verticalScroll;
}

void ScrollArea::setVerticalScroll(bool vertical) {
  m_verticalScroll = vertical;
}

std::expected<bool, StarException> ScrollArea::sendEvent(InputEvent const& event) {
  if (!m_visible)
    return false;

  if (m_dragActive) {
    if (std::holds_alternative<MouseButtonUpEvent>(event)) {
      auto blurred = blur();
      if (!blurred)
        return std::unexpected(std::move(blurred.error()));
      m_dragActive = false;
      m_vBar->thumb()->setPressed(false);
      m_hBar->thumb()->setPressed(false);
      return true;
    } else if (std::holds_alternative<MouseMoveEvent>(event)) {
      auto thumbDragPosition = *context().mousePosition(event) - screenPosition() - m_dragOffset;
      auto offset = (m_dragDirection == GuiDirection::Vertical ? m_vBar : m_hBar)->offsetFromThumbPosition(thumbDragPosition);
      if (!offset)
        return std::unexpected(std::move(offset.error()));
      m_scrollOffset = *offset;
      return true;
    }
  }

  auto mousePos = context().mousePosition(event);
  if (mousePos && !inMember(*mousePos))
    return false;

  if (std::holds_alternative<MouseButtonDownEvent>(event)) {
    if (m_vBar->thumb()->inMember(*mousePos)) {
      auto focused = focus();
      if (!focused)
        return std::unexpected(std::move(focused.error()));
      m_dragOffset = *mousePos - screenPosition() - m_vBar->thumb()->position();
      m_dragDirection = GuiDirection::Vertical;
      m_dragActive = true;
      m_vBar->thumb()->setPressed(true);
      return true;
    } else if (m_hBar->thumb()->inMember(*mousePos)) {
      auto focused = focus();
      if (!focused)
        return std::unexpected(std::move(focused.error()));
      m_dragOffset = *mousePos - screenPosition() - m_hBar->thumb()->position();
      m_dragDirection = GuiDirection::Horizontal;
      m_dragActive = true;
      m_hBar->thumb()->setPressed(true);
      return true;
    }
  }

  auto handled = Widget::sendEvent(event);
  if (!handled)
    return std::unexpected(std::move(handled.error()));
  if (*handled)
    return true;

  if (std::holds_alternative<MouseWheelEvent>(event)) {
    auto const& mouseWheel = std::get<MouseWheelEvent>(event);
    if (mouseWheel.mouseWheel == MouseWheel::Up)
      scrollAreaBy({0, m_buttonAdvance * 3});
    else
      scrollAreaBy({0, -m_buttonAdvance * 3});
    return true;
  }

  return true;
}

std::expected<void, StarException> ScrollArea::update(float dt) {
  if (m_updatesChildren) {
    auto updated = Widget::update(dt);
    if (!updated)
      return std::unexpected(std::move(updated.error()));
  }
  if (!m_visible)
    return {};
  
  auto maxScroll = maxScrollPosition();

  // keep vertical scroll bars same distance from the *top* on resize
  if (m_verticalScroll && maxScroll != m_lastMaxScroll)
    m_scrollOffset += (maxScroll - m_lastMaxScroll);

  m_scrollOffset = m_scrollOffset.piecewiseClamp(Vec2I(), maxScroll);
  m_lastMaxScroll = maxScroll;
  return {};
}

Vec2I ScrollArea::maxScrollPosition() const {
  return Vec2I().piecewiseMax(contentSize() - size());
}

std::expected<void, StarException> ScrollArea::drawChildren() {
  RectI innerDrawingArea = m_drawingArea;

  if (horizontalScroll())
    innerDrawingArea.setYMin(ScrollAreaBorder);
  if (verticalScroll())
    innerDrawingArea.setXMax(innerDrawingArea.xMax() - ScrollAreaBorder);

  auto contentBoundRect = ScrollArea::contentBoundRect();
  auto contentOffset = contentBoundRect.min();
  auto contentSize = contentBoundRect.size();

  auto offset = contentOffset + scrollOffset();

  auto areaSize = ScrollArea::areaSize();

  if (contentSize[1] < areaSize[1])
    offset[1] = offset[1] - (areaSize[1] - contentSize[1]);

  for (auto child : m_members) {
    if (child == m_vBar || child == m_hBar)
      continue;
    child->setDrawingOffset(-offset);
    auto rendered = child->render(innerDrawingArea);
    if (!rendered)
      return std::unexpected(std::move(rendered.error()));
  }

  if (m_horizontalScroll) {
    auto rendered = m_hBar->render(m_drawingArea);
    if (!rendered)
      return std::unexpected(std::move(rendered.error()));
  }
  if (m_verticalScroll)
    return m_vBar->render(m_drawingArea);
  return {};
}

int ScrollArea::advanceFactorHelper() {
  auto t = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() + std::chrono::microseconds(500)) - m_advanceLimiter;
  m_advanceLimiter = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() + std::chrono::microseconds(500));
  if ((t > ScrollAdvanceTimer) || (t < std::chrono::milliseconds::zero()))
    t = ScrollAdvanceTimer;
  return (int)std::ceil((m_buttonAdvance * t.count()) / (float)ScrollAdvanceTimer.count());
}

void ScrollArea::setUpdatesChildren(bool slop) {
  m_updatesChildren = slop;
}

}

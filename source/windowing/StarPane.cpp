import star.json_extra;
import std;
import star.json;
import star.strong_typedef;
import star.data_stream;
import star.bi_map;
import star.game_types;
import star.vector;
import star.casting;
import star.input_event;
import star.color;
import star.font;
import star.list;
import star.image_processing;
import star.hash;
import star.string_view;
import star.thread;
import star.directives;
import star.ordered_map;
import star.io_device;
import star.ref_ptr;
import star.logging;
import star.format;
import star.output_proxy;
import star.version;
import star.string;
import star.text;
import star.asset_path;
import star.listener;
import star.set;
import star.audio;
import star.map;
import star.lua;
import star.lua_converters;

import star.celestial_coordinate;
import star.quest_descriptor;
import star.item;
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
import star.pane;
import star.item_descriptor;
import star.item_recipe;
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
import star.widget_parsing;
import star.gui_reader;
import star.widget_lua_bindings;
import star.image_widget;


import star.item_database;

import star.random;
import star.algorithm;
namespace Star {

extern std::expected<EnumMap<PaneAnchor>, StarException> const PaneAnchorNames = EnumMap<PaneAnchor>::create({
    {PaneAnchor::None, "none"},
    {PaneAnchor::BottomLeft, "bottomLeft"},
    {PaneAnchor::BottomRight, "bottomRight"},
    {PaneAnchor::TopLeft, "topLeft"},
    {PaneAnchor::TopRight, "topRight"},
    {PaneAnchor::CenterBottom, "centerBottom"},
    {PaneAnchor::CenterTop, "centerTop"},
    {PaneAnchor::CenterLeft, "centerLeft"},
    {PaneAnchor::CenterRight, "centerRight"},
    {PaneAnchor::Center, "center"},
});

Vec2I paneAnchorBase(PaneAnchor anchor, Vec2I windowSize, Vec2I paneSize) {
  switch (anchor) {
  case PaneAnchor::BottomRight: return {windowSize[0] - paneSize[0], 0};
  case PaneAnchor::TopLeft: return {0, windowSize[1] - paneSize[1]};
  case PaneAnchor::TopRight: return windowSize - paneSize;
  case PaneAnchor::CenterTop: return {(windowSize[0] - paneSize[0]) / 2, windowSize[1] - paneSize[1]};
  case PaneAnchor::CenterBottom: return {(windowSize[0] - paneSize[0]) / 2, 0};
  case PaneAnchor::CenterLeft: return {0, (windowSize[1] - paneSize[1]) / 2};
  case PaneAnchor::CenterRight: return {windowSize[0] - paneSize[0], (windowSize[1] - paneSize[1]) / 2};
  case PaneAnchor::Center: return (windowSize - paneSize) / 2;
  default: return {};
  }
}

Pane::Pane(GuiContext& context) : Widget(context) {
  m_dragActive = m_lockPosition = false;
  m_dismissed = true;
  m_centerOffset = Vec2I();
  m_anchor = PaneAnchor::None;
  m_anchorOffset = Vec2I();
  m_visible = false;
  m_hasDisplayed = false;
}

std::expected<std::shared_ptr<Pane>, StarException> Pane::create(GuiContext& context) {
  auto pane = std::shared_ptr<Pane>(new Pane(context));
  auto initialized = pane->initializePane();
  if (!initialized)
    return std::unexpected(std::move(initialized.error()));
  return pane;
}

std::expected<void, StarException> Pane::initializePane() {
  auto assets = context().root().assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  auto text = (*assets)->json("/interface.config:paneTextStyle");
  if (!text)
    return std::unexpected(std::move(text.error()));
  auto style = TextStyle::create(*text);
  if (!style)
    return std::unexpected(std::move(style.error()));
  auto iconJson = (*assets)->json("/interface.config:paneIconOffset");
  if (!iconJson)
    return std::unexpected(std::move(iconJson.error()));
  auto icon = jsonToVec2I(*iconJson);
  if (!icon)
    return std::unexpected(std::move(icon.error()));
  auto titleJson = (*assets)->json("/interface.config:paneTitleOffset");
  if (!titleJson)
    return std::unexpected(std::move(titleJson.error()));
  auto title = jsonToVec2I(*titleJson);
  if (!title)
    return std::unexpected(std::move(title.error()));
  auto subtitleJson = (*assets)->json("/interface.config:paneSubTitleOffset");
  if (!subtitleJson)
    return std::unexpected(std::move(subtitleJson.error()));
  auto subtitle = jsonToVec2I(*subtitleJson);
  if (!subtitle)
    return std::unexpected(std::move(subtitle.error()));
  auto titleColorJson = (*assets)->json("/interface.config:paneTitleColor");
  if (!titleColorJson)
    return std::unexpected(std::move(titleColorJson.error()));
  auto titleColor = jsonToColor(*titleColorJson);
  if (!titleColor)
    return std::unexpected(std::move(titleColor.error()));
  auto subtitleColorJson = (*assets)->json("/interface.config:paneSubTitleColor");
  if (!subtitleColorJson)
    return std::unexpected(std::move(subtitleColorJson.error()));
  auto subtitleColor = jsonToColor(*subtitleColorJson);
  if (!subtitleColor)
    return std::unexpected(std::move(subtitleColor.error()));
  m_textStyle = std::move(*style);
  m_iconOffset = *icon;
  m_titleOffset = *title;
  m_subTitleOffset = *subtitle;
  m_titleColor = *titleColor;
  m_subTitleColor = *subtitleColor;
  return {};
}
Pane::~Pane() {
  if (m_icon)
    m_icon->setParent(std::nullopt);
}

bool Pane::removeChild(Widget& member) {
  if (m_icon && std::addressof(*m_icon) == std::addressof(member)) {
    m_icon->setParent(std::nullopt);
    m_icon.reset();
    return true;
  }
  return Widget::removeChild(member);
}



std::expected<void, StarException> Pane::dismissed() {
  std::optional<StarException> error;
  if (m_clickDown) {
    auto result = m_clickDown->mouseOut();
    if (!result)
      error = std::move(result.error());
  }
  m_clickDown.reset();
  if (m_mouseOver) {
    auto result = m_mouseOver->mouseOut();
    if (!result) {
      if (error)
        error = GuiException(strf("Pane dismissal also failed: {}", outputException(ExceptionDiagnostic(result.error()), true)),
            ExceptionDiagnostic(*error));
      else
        error = std::move(result.error());
    }
  }
  m_mouseOver.reset();
  auto hidden = hide();
  if (!hidden) {
    if (error)
      error = GuiException(strf("Hiding dismissed pane also failed: {}", outputException(ExceptionDiagnostic(hidden.error()), true)),
          ExceptionDiagnostic(*error));
    else
      error = std::move(hidden.error());
  }
  m_dismissed = true;
  if (error)
    return std::unexpected(std::move(*error));
  return {};
}

void Pane::dismiss() {
  m_dismissed = true;
}

bool Pane::isDismissed() const {
  return m_dismissed;
}

bool Pane::isDisplayed() const {
  return !m_dismissed;
}

std::expected<bool, StarException> Pane::sendEvent(InputEvent const& event) {
  if (m_visible) {
    auto const mousePosition = m_context.mousePosition(event);
    auto const buttonDown = std::get_if<MouseButtonDownEvent>(&event);
    if (mousePosition) {
      Vec2I mousePos = *mousePosition;
      // First, handle preliminary mouse out / click up events
      if (m_mouseOver) {
        if (!m_mouseOver->inMember(mousePos) || !m_mouseOver->active()) {
          auto result = m_mouseOver->mouseOut();
          if (!result)
            return std::unexpected(std::move(result.error()));
          m_mouseOver.reset();
        }
      }

      if (std::holds_alternative<MouseButtonUpEvent>(event))
        m_clickDown.reset();

      std::shared_ptr<Widget> newClickDown;
      std::shared_ptr<Widget> newMouseOver;
      std::shared_ptr<Widget> newFocusWidget;

      List<std::shared_ptr<Widget>> validWidgets;
      // gather valid widgets into our own list because any of them could mutate
      // m_members while processing the event and fuck everything up
      for (auto const& widget : std::views::reverse(m_members)) {
        if (widget->inMember(mousePos) && widget->active() && widget->interactive())
          validWidgets.append(widget);
      }

      // Then, go through widgets in highest to lowest z-order and handle mouse
      // over, focus, and capture events.
      for (auto const& widget : validWidgets) {
        std::shared_ptr<Widget> topWidget = widget;
        std::shared_ptr<Widget> child = getChildAt(mousePos);
        if (child->active() && child->interactive()) {
          if (buttonDown
              && (buttonDown->mouseButton == MouseButton::Left || buttonDown->mouseButton == MouseButton::Right)) {
            if (!newClickDown)
              newClickDown = child;

            if (!newFocusWidget)
              newFocusWidget = child;
          }

          if (!newMouseOver)
            newMouseOver = child;
        }
      }

      if (m_clickDown != newClickDown)
        m_clickDown = newClickDown;

      if (m_mouseOver != newMouseOver) {
        if (m_mouseOver) {
          auto result = m_mouseOver->mouseOut();
          if (!result)
            return std::unexpected(std::move(result.error()));
        }
        m_mouseOver = newMouseOver;
        if (m_mouseOver) {
          auto result = m_clickDown == m_mouseOver ? m_mouseOver->mouseReturnStillDown() : m_mouseOver->mouseOver();
          if (!result)
            return std::unexpected(std::move(result.error()));
        }
      }

      if (newFocusWidget && m_focusWidget != newFocusWidget) {
        if (auto focusWidget = m_focusWidget) {
          auto result = focusWidget->blur();
          if (!result)
            return std::unexpected(std::move(result.error()));
        }
        auto focused = newFocusWidget->focus();
        if (!focused)
          return std::unexpected(std::move(focused.error()));
        m_focusWidget = newFocusWidget;
      }

      // Finally go through widgets in highest to lowest z-order and send the
      // raw event, stopping further processing if the widget consumes it.
      for (auto const& widget : validWidgets) {
        auto handled = widget->sendEvent(event);
        if (!handled)
          return std::unexpected(std::move(handled.error()));
        if (*handled)
          return true;
      }
    }

    if (buttonDown) {
      Vec2I mousePos = *m_context.mousePosition(event);
      if (inDragArea(mousePos) && !m_lockPosition) {
        setDragActive(true, mousePos);
        return true;
      }
      if (inWindow(mousePos))
        return true;
    }

    if (m_focusWidget) {
      auto handled = m_focusWidget->sendEvent(event);
      if (!handled)
        return std::unexpected(std::move(handled.error()));
      if (*handled)
        return true;
    }
  }
  return false;
}

std::expected<void, StarException> Pane::setFocus(Widget const& focus) {
  if (m_focusWidget && std::addressof(*m_focusWidget) == std::addressof(focus))
    return {};
  auto child = childPtr(focus);
  if (!child)
    return std::unexpected(GuiException("Cannot set focus on a widget which is not a child of this pane"));
  if (m_focusWidget) {
    auto blurred = m_focusWidget->blur();
    if (!blurred)
      return std::unexpected(std::move(blurred.error()));
  }
  m_focusWidget = std::move(child);
  return {};
}

void Pane::removeFocus(Widget const& focus) {
  if (m_focusWidget && std::addressof(*m_focusWidget) == std::addressof(focus))
    m_focusWidget.reset();
}

void Pane::removeFocus() {
  m_focusWidget.reset();
}

std::optional<std::reference_wrapper<Pane const>> Pane::window() const {
  return std::cref(*this);
}

std::optional<std::reference_wrapper<Pane>> Pane::window() {
  return std::ref(*this);
}

std::expected<void, StarException> Pane::update(float dt) {
  if (m_visible) {
    for (auto const& widget : m_members) {
      auto updated = widget->update(dt);
      if (!updated)
        return std::unexpected(std::move(updated.error()));
      auto focused = widget->hasFocus();
      if (!focused)
        return std::unexpected(std::move(focused.error()));
      if ((m_focusWidget == widget) != *focused) {
        auto blurred = widget->blur();
        if (!blurred)
          return std::unexpected(std::move(blurred.error()));
        m_focusWidget.reset();
      }
    }
  }
  return {};
}

std::expected<void, StarException> Pane::tick(float) {
  m_playingSounds.filter([](std::pair<String, std::shared_ptr<AudioInstance>> const& p) {
    return p.second->finished() == false;
  });
  return {};
}

bool Pane::dragActive() const {
  return m_dragActive;
}

Vec2I Pane::dragMouseOrigin() const {
  return m_dragMouseOrigin;
}

void Pane::setDragActive(bool dragActive, Vec2I dragMouseOrigin) {
  m_dragActive = dragActive;
  m_dragMouseOrigin = dragMouseOrigin;
}

void Pane::drag(Vec2I mousePosition) {
  Vec2I delta = mousePosition - m_dragMouseOrigin;
  setPosition(relativePosition() + delta);
  m_dragMouseOrigin += delta;
}

bool Pane::inWindow(Vec2I const& position) const {
  return screenBoundRect().contains(position);
}

bool Pane::inDragArea(Vec2I const& position) const {
  return inWindow(position) && (position[1] < (this->position()[1] + m_footerSize[1])
                                   || position[1] > (this->position()[1] + (m_footerSize[1] + m_bodySize[1])));
}

Vec2I Pane::cursorRelativeToPane(Vec2I const& position) const {
  return position - this->position();
}

Vec2I Pane::centerOffset() const {
  return m_centerOffset;
}

std::expected<void, StarException> Pane::setBG(BGResult const& res) {
  return setBG(res.header, res.body, res.footer);
}

std::expected<void, StarException> Pane::setBG(String const& header, String const& body, String const& footer) {
  Vec2I headerSize, bodySize, footerSize;
  if (!header.empty()) {
    auto size = m_context.textureSize(header);
    if (!size)
      return std::unexpected(std::move(size.error()));
    headerSize = Vec2I(*size);
  }
  if (!body.empty()) {
    auto size = m_context.textureSize(body);
    if (!size)
      return std::unexpected(std::move(size.error()));
    bodySize = Vec2I(*size);
  }
  if (!footer.empty()) {
    auto size = m_context.textureSize(footer);
    if (!size)
      return std::unexpected(std::move(size.error()));
    footerSize = Vec2I(*size);
  }
  m_bgHeader = header;
  m_bgBody = body;
  m_bgFooter = footer;
  m_headerSize = headerSize;
  m_bodySize = bodySize;
  m_footerSize = footerSize;
  setSize(Vec2I(std::max(std::max(headerSize[0], bodySize[0]), footerSize[0]),
      headerSize[1] + bodySize[1] + footerSize[1]));
  return {};
}

Pane::BGResult Pane::getBG() const { return {.header = m_bgHeader, .body = m_bgBody, .footer = m_bgFooter}; }

void Pane::lockPosition() {
  m_lockPosition = true;
}

void Pane::unlockPosition() {
  m_lockPosition = false;
}

std::expected<void, StarException> Pane::setTitle(std::shared_ptr<Widget> icon, String const& title, String const& subTitle) {
  auto result = setTitleIcon(std::move(icon));
  if (!result)
    return std::unexpected(std::move(result.error()));
  m_title = title;
  m_subTitle = subTitle;
  return {};
}

void Pane::setTitleString(String const& title, String const& subTitle) {
  m_title = title;
  m_subTitle = subTitle;
}

std::expected<void, StarException> Pane::setTitleIcon(std::shared_ptr<Widget> icon) {
  if (icon) {
    auto shown = icon->show();
    if (!shown)
      return std::unexpected(std::move(shown.error()));
    if (auto previous = icon->parent())
      previous->get().removeChild(*icon);
  }
  if (m_icon)
    m_icon->setParent(std::nullopt);
  if (icon)
    icon->setParent(std::ref(*this));
  m_icon = std::move(icon);
  return {};
}

String Pane::title() const {
  return m_title;
}

String Pane::subTitle() const {
  return m_subTitle;
}

std::shared_ptr<Widget> Pane::titleIcon() const {
  return m_icon;
}

PaneAnchor Pane::anchor() {
  return m_anchor;
}

void Pane::setAnchor(PaneAnchor anchor) {
  m_anchor = anchor;
}

Vec2I Pane::anchorOffset() const {
  return m_anchorOffset;
}

void Pane::setAnchorOffset(Vec2I anchorOffset) {
  m_anchorOffset = anchorOffset;
}

bool Pane::hasDisplayed() const {
  return m_hasDisplayed;
}

std::expected<std::shared_ptr<Pane>, StarException> Pane::createTooltip(Vec2I const&) {
  return std::shared_ptr<Pane>{};
}

std::expected<std::optional<String>, StarException> Pane::cursorOverride(Vec2I const&) {
  return {};
}

std::expected<std::optional<std::shared_ptr<Item>>, StarException> Pane::shiftItemFromInventory(std::shared_ptr<Item> const&) {
  return {};
}

std::expected<LuaCallbacks, StarException> Pane::makePaneCallbacks() {
  LuaCallbacks callbacks;
  // Lua can retain these callbacks after their pane has been released.
  auto registerPaneCallback = [&]<typename Function>(String name, Function function) {
    auto guarded = [owner = weak_from_this(), function = std::move(function)]<typename... Args>(
                       std::type_identity<std::tuple<Pane&, Args...>>) mutable {
      return [owner, function = std::move(function)](Args... args) mutable
          -> std::invoke_result_t<Function&, Pane&, Args...> {
        auto pane = as<Pane>(owner.lock());
        if (!pane)
          return std::unexpected(GuiException("Pane callback target no longer exists"));
        return std::invoke(function, *pane, std::forward<Args>(args)...);
      };
    }(std::type_identity<typename FunctionTraits<Function>::ArgTuple>{});
    return callbacks.registerCallback(std::move(name), std::move(guarded));
  };

  auto toWidget = registerPaneCallback("toWidget", [](Pane& pane) -> std::expected<LuaCallbacks, StarException> {
    return LuaBindings::makeWidgetCallbacks(pane.context(), pane, pane.reader());
  });
  if (!toWidget)
    return std::unexpected(std::move(toWidget.error()));

  auto dismissCallback = registerPaneCallback("dismiss", [](Pane& pane) -> std::expected<void, StarException> {
    pane.dismiss();
    return {};
  });
  if (!dismissCallback)
    return std::unexpected(std::move(dismissCallback.error()));

  auto playSound = registerPaneCallback("playSound",
    [](Pane& pane, String const& audio, std::optional<int> loops, std::optional<float> volume) -> std::expected<void, StarException> {
      auto assets = pane.context().root().assets();
      if (!assets)
        return std::unexpected(std::move(assets.error()));
      auto sound = (*assets)->audio(audio);
      if (!sound)
        return std::unexpected(std::move(sound.error()));
      auto instance = AudioInstance::create(**sound);
      if (!instance)
        return std::unexpected(std::move(instance.error()));
      (*instance)->setVolume(volume.value_or(1.0));
      (*instance)->setLoops(loops.value_or(0));
      auto played = pane.context().playAudio(*instance);
      if (!played)
        return std::unexpected(std::move(played.error()));
      pane.m_playingSounds.append({audio, std::move(*instance)});
      return {};
    });
  if (!playSound)
    return std::unexpected(std::move(playSound.error()));

  auto stopSounds = registerPaneCallback("stopAllSounds", [](Pane& pane, std::optional<String> const& audio) -> std::expected<void, StarException> {
      pane.m_playingSounds.filter([audio](std::pair<String, std::shared_ptr<AudioInstance>> const& p) {
        if (!audio || p.first == *audio) {
          p.second->stop();
          return false;
        }
        return true;
      });
      return {};
    });
  if (!stopSounds)
    return std::unexpected(std::move(stopSounds.error()));

  auto titleCallback = registerPaneCallback("setTitle", [](Pane& pane, String const& title, String const& subTitle) -> std::expected<void, StarException> {
      pane.setTitleString(title, subTitle);
      return {};
    });
  if (!titleCallback)
    return std::unexpected(std::move(titleCallback.error()));

  auto titleIconCallback = registerPaneCallback("setTitleIcon", [](Pane& pane, String const& image) -> std::expected<void, StarException> {
    if (auto icon = as<ImageWidget>(pane.titleIcon()))
      return icon->setImage(image);
    return {};
  });
  if (!titleIconCallback)
    return std::unexpected(std::move(titleIconCallback.error()));

  auto getPosition = registerPaneCallback("getPosition", [](Pane& pane) -> std::expected<Vec2I, StarException> { return pane.relativePosition(); });
  if (!getPosition)
    return std::unexpected(std::move(getPosition.error()));
  auto setPositionCallback = registerPaneCallback("setPosition", [](Pane& pane, Vec2I const& position) -> std::expected<void, StarException> {
    pane.setPosition(position);
    return {};
  });
  if (!setPositionCallback)
    return std::unexpected(std::move(setPositionCallback.error()));
  auto getSize = registerPaneCallback("getSize", [](Pane& pane) -> std::expected<Vec2I, StarException> { return pane.size(); });
  if (!getSize)
    return std::unexpected(std::move(getSize.error()));
  auto setSizeCallback = registerPaneCallback("setSize", [](Pane& pane, Vec2I const& size) -> std::expected<void, StarException> {
    pane.setSize(size);
    return {};
  });
  if (!setSizeCallback)
    return std::unexpected(std::move(setSizeCallback.error()));
  auto setBackground = registerPaneCallback("setBackground", [](Pane& pane, String const& header, std::optional<String> const& body, std::optional<String> const& footer) {
    return pane.setBG(header, body.value_or(""), footer.value_or(""));
  });
  if (!setBackground)
    return std::unexpected(std::move(setBackground.error()));
  auto setScissoring = registerPaneCallback("setScissoringEnabled", [](Pane& pane, bool enabled) -> std::expected<void, StarException> {
    if (enabled)
      pane.enableScissoring();
    else
      pane.disableScissoring();
    return {};
  });
  if (!setScissoring)
    return std::unexpected(std::move(setScissoring.error()));

  auto addWidget = registerPaneCallback("addWidget",
    [](Pane& pane, Json const& config, std::optional<String> const& widgetName) -> std::expected<std::optional<LuaCallbacks>, StarException> {
      String name = widgetName.value_or(toString(pane.context().root().random().randu64()));
      auto guiReader = pane.reader();
      auto widget = guiReader->makeSingle(name, config);
      if (!widget)
        return std::unexpected(std::move(widget.error()));
      if (!*widget)
        return std::optional<LuaCallbacks>{};
      auto added = pane.addChild(name, *widget);
      if (!added)
        return std::unexpected(std::move(added.error()));
      auto bindings = LuaBindings::makeWidgetCallbacks(pane.context(), **widget, guiReader);
      if (!bindings) {
        pane.removeChild(**widget);
        return std::unexpected(std::move(bindings.error()));
      }
      return std::optional<LuaCallbacks>(std::move(*bindings));
    });
  if (!addWidget)
    return std::unexpected(std::move(addWidget.error()));

  auto removeWidget = registerPaneCallback("removeWidget", [](Pane& pane, String const& name) -> std::expected<bool, StarException> { return pane.removeChild(name); });
  if (!removeWidget)
    return std::unexpected(std::move(removeWidget.error()));
  auto scale = registerPaneCallback("scale", [](Pane& pane) -> std::expected<float, StarException> { return pane.context().interfaceScale(); });
  if (!scale)
    return std::unexpected(std::move(scale.error()));
  auto displayedCallback = registerPaneCallback("isDisplayed", [](Pane& pane) -> std::expected<bool, StarException> { return pane.isDisplayed(); });
  if (!displayedCallback)
    return std::unexpected(std::move(displayedCallback.error()));
  auto focusCallback = registerPaneCallback("hasFocus", [](Pane& pane) { return pane.hasFocus(); });
  if (!focusCallback)
    return std::unexpected(std::move(focusCallback.error()));
  auto showCallback = registerPaneCallback("show", [](Pane& pane) { return pane.show(); });
  if (!showCallback)
    return std::unexpected(std::move(showCallback.error()));
  auto hideCallback = registerPaneCallback("hide", [](Pane& pane) { return pane.hide(); });
  if (!hideCallback)
    return std::unexpected(std::move(hideCallback.error()));
  auto anchorCallback = registerPaneCallback("anchor", [](Pane& pane) -> std::expected<String, StarException> {
    if (!PaneAnchorNames)
      return std::unexpected(PaneAnchorNames.error());
    auto name = PaneAnchorNames->get_right(pane.anchor());
    if (!name)
      return std::unexpected(std::move(name.error()));
    return name->get();
  });
  if (!anchorCallback)
    return std::unexpected(std::move(anchorCallback.error()));
  auto setAnchorCallback = registerPaneCallback("setAnchor", [](Pane& pane, String const& name) -> std::expected<void, StarException> {
    if (!PaneAnchorNames)
      return std::unexpected(PaneAnchorNames.error());
    auto value = PaneAnchorNames->get_left(name);
    if (!value)
      return std::unexpected(std::move(value.error()));
    pane.setAnchor(value->get());
    return {};
  });
  if (!setAnchorCallback)
    return std::unexpected(std::move(setAnchorCallback.error()));
  auto getOffset = registerPaneCallback("anchorOffset", [](Pane& pane) -> std::expected<Vec2I, StarException> { return pane.anchorOffset(); });
  if (!getOffset)
    return std::unexpected(std::move(getOffset.error()));
  auto setOffset = registerPaneCallback("setAnchorOffset", [](Pane& pane, Vec2I offset) -> std::expected<void, StarException> {
    pane.setAnchorOffset(offset);
    return {};
  });
  if (!setOffset)
    return std::unexpected(std::move(setOffset.error()));
  auto getScreenPosition = registerPaneCallback("getScreenPosition", [](Pane& pane) -> std::expected<Vec2I, StarException> {
    auto window = pane.m_context.windowInterfaceSize();
    if (!window)
      return std::unexpected(std::move(window.error()));
    Vec2I offset = pane.anchorOffset() + paneAnchorBase(pane.anchor(), Vec2I(*window), pane.size());
    return offset + pane.relativePosition();
  });
  if (!getScreenPosition)
    return std::unexpected(std::move(getScreenPosition.error()));

  auto setScreenPosition = registerPaneCallback("setScreenPosition", [](Pane& pane, Vec2I screenPos) -> std::expected<void, StarException> {
    auto window = pane.m_context.windowInterfaceSize();
    if (!window)
      return std::unexpected(std::move(window.error()));
    Vec2I offset = pane.anchorOffset() + paneAnchorBase(pane.anchor(), Vec2I(*window), pane.size());

    pane.setPosition(screenPos - offset);
    return {};
  });
  if (!setScreenPosition)
    return std::unexpected(std::move(setScreenPosition.error()));

  return callbacks;
}

std::shared_ptr<GuiReader> Pane::reader() {
  return std::make_shared<GuiReader>(this->context());
}

std::expected<void, StarException> Pane::renderImpl() {
  if (!m_bgFooter.empty()) {
    auto drawn = m_context.drawInterfaceQuad(m_bgFooter, Vec2F(position()));
    if (!drawn)
      return std::unexpected(std::move(drawn.error()));
  }
  if (!m_bgBody.empty()) {
    auto drawn = m_context.drawInterfaceQuad(m_bgBody, Vec2F(position()) + Vec2F(0, m_footerSize[1]));
    if (!drawn)
      return std::unexpected(std::move(drawn.error()));
  }
  if (!m_bgHeader.empty()) {
    auto headerPos = Vec2F(position()) + Vec2F(0, m_footerSize[1] + m_bodySize[1]);
    auto header = m_context.drawInterfaceQuad(m_bgHeader, headerPos);
    if (!header)
      return std::unexpected(std::move(header.error()));
    if (m_icon) {
      m_icon->setPosition(Vec2I(0, m_footerSize[1] + m_bodySize[1]) + m_iconOffset);
      auto rendered = m_icon->render(m_drawingArea);
      auto reset = m_context.resetInterfaceScissorRect();
      if (!rendered && !reset)
        return std::unexpected(GuiException(
            strf("Restoring pane scissor also failed: {}", outputException(ExceptionDiagnostic(reset.error()), true)),
            ExceptionDiagnostic(rendered.error())));
      if (!rendered)
        return std::unexpected(std::move(rendered.error()));
      if (!reset)
        return std::unexpected(std::move(reset.error()));
    }
    auto style = m_context.setTextStyle(m_textStyle);
    if (!style)
      return std::unexpected(std::move(style.error()));
    auto drawTitles = [&]() -> std::expected<void, StarException> {
      auto color = m_context.setFontColor(m_titleColor.toRgba());
      if (!color)
        return std::unexpected(std::move(color.error()));
      auto mode = m_context.setFontMode(FontMode::Shadow);
      if (!mode)
        return std::unexpected(std::move(mode.error()));
      auto title = m_context.renderInterfaceText(m_title, {headerPos + Vec2F(m_titleOffset)});
      if (!title)
        return std::unexpected(std::move(title.error()));
      auto subtitleColor = m_context.setFontColor(m_subTitleColor.toRgba());
      if (!subtitleColor)
        return std::unexpected(std::move(subtitleColor.error()));
      auto subtitle = m_context.renderInterfaceText(m_subTitle, {headerPos + Vec2F(m_subTitleOffset)});
      if (!subtitle)
        return std::unexpected(std::move(subtitle.error()));
      return {};
    };
    auto drawn = drawTitles();
    auto cleared = m_context.clearTextStyle();
    if (!drawn && !cleared)
      return std::unexpected(GuiException(
          strf("Clearing pane text style also failed: {}", outputException(ExceptionDiagnostic(cleared.error()), true)),
          ExceptionDiagnostic(drawn.error())));
    if (!drawn)
      return std::unexpected(std::move(drawn.error()));
    if (!cleared)
      return std::unexpected(std::move(cleared.error()));
  }
  return {};
}

}

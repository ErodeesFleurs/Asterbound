import std;
import star.format;
import star.json;
import star.json_extra;
import star.game_types;
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
import star.io_device;
import star.ref_ptr;
import star.logging;
import star.version;
import star.string;
import star.text;
import star.asset_path;
import star.listener;
import star.set;
import star.audio;
import star.map;
import star.ordered_map;
import star.game_types;

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
import star.game_timers;
import star.pane_manager;

import star.matrix3;
import star.algorithm;
namespace Star {

extern std::expected<EnumMap<PaneLayer>, StarException> const PaneLayerNames = EnumMap<PaneLayer>::create({
  {PaneLayer::Tooltip, "Tooltip"},
  {PaneLayer::ModalWindow, "ModalWindow"},
  {PaneLayer::Window, "Window"},
  {PaneLayer::Hud, "Hud"},
  {PaneLayer::World, "World"},
  {PaneLayer::Background, "Background"}
});

PaneManager::PaneManager(GuiContext& context)
  : m_context(context), m_prevInterfaceScale(1), m_tooltipMouseoverRadius(0) {}

std::expected<std::shared_ptr<PaneManager>, StarException> PaneManager::create(GuiContext& context) {
  auto manager = std::make_shared<PaneManager>(context);
  auto initialized = manager->initializePaneManager();
  if (!initialized)
    return std::unexpected(std::move(initialized.error()));
  return manager;
}

std::expected<void, StarException> PaneManager::initializePaneManager() {
  auto assets = m_context.root().assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  auto radiusJson = (*assets)->json("/panes.config:tooltipMouseoverRadius");
  if (!radiusJson)
    return std::unexpected(std::move(radiusJson.error()));
  auto radius = radiusJson->toFloat();
  if (!radius)
    return std::unexpected(std::move(radius.error()));
  auto offsetJson = (*assets)->json("/panes.config:tooltipMouseoverOffset");
  if (!offsetJson)
    return std::unexpected(std::move(offsetJson.error()));
  auto offset = jsonToVec2I(*offsetJson);
  if (!offset)
    return std::unexpected(std::move(offset.error()));
  auto timeJson = (*assets)->json("/panes.config:tooltipMouseoverTime");
  if (!timeJson)
    return std::unexpected(std::move(timeJson.error()));
  auto time = timeJson->toFloat();
  if (!time)
    return std::unexpected(std::move(time.error()));
  m_tooltipMouseoverRadius = *radius;
  m_tooltipMouseOffset = *offset;
  m_tooltipShowTimer = GameTimer(*time);
  return {};
}

std::expected<void, StarException> PaneManager::displayPane(PaneLayer paneLayer, std::shared_ptr<Pane> const& pane, DismissCallback onDismiss) {
  if (!m_displayedPanes[paneLayer].insert_front(pane, std::move(onDismiss)).second)
    return std::unexpected(GuiException("Pane displayed twice in PaneManager::displayPane"));

  if (!pane->hasDisplayed() && pane->anchor() == PaneAnchor::None) {
    auto size = windowSize();
    if (!size)
      return std::unexpected(std::move(size.error()));
    pane->setPosition(Vec2I((*size - pane->size()) / 2) + pane->centerOffset()); // center it
  }

  return pane->displayed();
}

bool PaneManager::isDisplayed(std::shared_ptr<Pane> const& pane) const {
  for (auto const& layerPair : m_displayedPanes) {
    if (layerPair.second.contains(pane))
      return true;
  }

  return false;
}

std::expected<void, StarException> PaneManager::dismissPane(std::shared_ptr<Pane> const& pane) {
  auto dismissed = dismiss(pane);
  if (!dismissed)
    return std::unexpected(std::move(dismissed.error()));
  if (!*dismissed)
    return std::unexpected(GuiException("No such pane in PaneManager::dismissPane"));
  return {};
}

std::expected<void, StarException> PaneManager::dismissAllPanes(Set<PaneLayer> const& paneLayers) {
  for (auto const& paneLayer : paneLayers) {
    for (auto const& panePair : auto(m_displayedPanes[paneLayer])) {
      auto dismissed = dismiss(panePair.first);
      if (!dismissed)
        return std::unexpected(std::move(dismissed.error()));
    }
  }
  return {};
}

std::expected<void, StarException> PaneManager::dismissAllPanes() {
  for (auto const& layerPair : auto(m_displayedPanes)) {
    for (auto const& panePair : layerPair.second) {
      auto dismissed = dismiss(panePair.first);
      if (!dismissed)
        return std::unexpected(std::move(dismissed.error()));
    }
  }
  return {};
}

std::shared_ptr<Pane> PaneManager::topPane(Set<PaneLayer> const& paneLayers) const {
  for (auto const& layerPair : m_displayedPanes) {
    if (paneLayers.contains(layerPair.first) && !layerPair.second.empty())
      return layerPair.second.begin()->first;
  }
  return {};
}

std::shared_ptr<Pane> PaneManager::topPane() const {
  for (auto const& layerPair : m_displayedPanes) {
    if (!layerPair.second.empty())
      return layerPair.second.begin()->first;
  }
  return {};
}

std::expected<void, StarException> PaneManager::bringToTop(std::shared_ptr<Pane> const& pane) {
  for (auto& layerPair : m_displayedPanes) {
    if (layerPair.second.contains(pane))
      return layerPair.second.move_to_front(pane);
  }

  return std::unexpected(GuiException("Pane was not displayed in PaneManager::bringToTop"));
}

std::expected<void, StarException> PaneManager::bringPaneAdjacent(std::shared_ptr<Pane> const& anchor, std::shared_ptr<Pane> const& adjacent, int gap) {
  auto size = windowSize();
  if (!size)
    return std::unexpected(std::move(size.error()));
  Vec2I centerAdjacent = anchor->position() + (anchor->size() / 2) - (adjacent->size() / 2);
  centerAdjacent = centerAdjacent.piecewiseClamp(Vec2I(), *size - adjacent->size()); // keeps pane inside window

  if (anchor->position()[0] + anchor->size()[0] + gap + adjacent->size()[0] <= (*size)[0])
    adjacent->setPosition(Vec2I(anchor->position()[0] + anchor->size()[0] + gap, centerAdjacent[1])); // place to the right
  else if (anchor->position()[0] - gap - adjacent->size()[0] >= 0)
    adjacent->setPosition(Vec2I(anchor->position()[0] - gap - adjacent->size()[0], centerAdjacent[1])); // place to the left
  else if (anchor->position()[1] + anchor->size()[1] + gap + adjacent->size()[1] <= (*size)[1])
    adjacent->setPosition(Vec2I(centerAdjacent[0], anchor->position()[1] + anchor->size()[1] + gap)); // place above
  else if (anchor->position()[1] - gap - adjacent->size()[1] >= 0)
    adjacent->setPosition(Vec2I(centerAdjacent[0], anchor->position()[1] - gap - adjacent->size()[1])); // place below
  else
    adjacent->setPosition(centerAdjacent);

  return bringToTop(adjacent);
}

std::shared_ptr<Pane> PaneManager::getPaneAt(Set<PaneLayer> const& paneLayers, Vec2I const& position) const {
  for (auto const& layerPair : m_displayedPanes) {
    if (!paneLayers.contains(layerPair.first))
      continue;

    for (auto const& panePair : layerPair.second) {
      if (panePair.first->interactive() && panePair.first->inWindow(position) && panePair.first->active())
        return panePair.first;
    }
  }

  return {};
}

std::shared_ptr<Pane> PaneManager::getPaneAt(Vec2I const& position) const {
  for (auto const& layerPair : m_displayedPanes) {
    for (auto const& panePair : layerPair.second) {
      if (panePair.first != m_activeTooltip
        && panePair.first->interactive()
        && panePair.first->inWindow(position)
        && panePair.first->active())
        return panePair.first;
    }
  }

  return {};
}

List<std::shared_ptr<Pane>> PaneManager::getAllPanes() {
  List<std::shared_ptr<Pane>> list;
  for (auto const& layerPair : m_displayedPanes) {
    for (auto const& panePair : layerPair.second) {
      if (panePair.first != m_activeTooltip && panePair.first->active())
        list.push_back(panePair.first);
    }
  }
  return list;
}

void PaneManager::setBackgroundWidget(std::shared_ptr<Widget> bg) { m_backgroundWidget = std::move(bg); }

std::expected<void, StarException> PaneManager::dismissWhere(std::function<std::expected<bool, StarException>(std::shared_ptr<Pane> const&)> func) {
  if (!func)
    return {};

  for (auto& layerPair : m_displayedPanes) {
    for (auto it = layerPair.second.begin(); it != layerPair.second.end();) {
      auto matches = func(it->first);
      if (!matches)
        return std::unexpected(std::move(matches.error()));
      if (!*matches) {
        ++it;
        continue;
      }
      auto dismissed = it->first->dismissed();
      if (!dismissed)
        return std::unexpected(std::move(dismissed.error()));
      if (it->second) {
        auto notified = it->second(it->first);
        if (!notified)
          return std::unexpected(std::move(notified.error()));
      }
      it = layerPair.second.erase(it);
    }
  }
  return {};
}

std::expected<std::shared_ptr<Pane>, StarException> PaneManager::keyboardCapturedPane() const {
  for (auto const& layerPair : m_displayedPanes) {
    for (auto const& panePair : layerPair.second) {
      if (!panePair.first->interactive())
        continue;
      auto capturer = panePair.first->keyboardCapturer();
      if (!capturer)
        return std::unexpected(std::move(capturer.error()));
      if (*capturer)
        return panePair.first;
    }
  }

  return std::shared_ptr<Pane>{};
}

std::expected<std::shared_ptr<Widget>, StarException> PaneManager::keyboardCapturedWidget() const {
  for (auto const& layerPair : m_displayedPanes) {
    for (auto const& panePair : layerPair.second) {
      if (!panePair.first->interactive())
        continue;
      auto capturer = panePair.first->keyboardCapturer();
      if (!capturer)
        return std::unexpected(std::move(capturer.error()));
      if (*capturer)
        return std::move(*capturer);
    }
  }

  return std::shared_ptr<Widget>{};
}

std::expected<bool, StarException> PaneManager::keyboardCapturedForTextInput() const {
  auto widget = keyboardCapturedWidget();
  if (!widget)
    return std::unexpected(std::move(widget.error()));
  if (!*widget)
    return false;
  auto mode = (*widget)->keyboardCaptureMode();
  if (!mode)
    return std::unexpected(std::move(mode.error()));
  return *mode == KeyboardCaptureMode::TextInput;
}
std::expected<List<std::shared_ptr<Pane>>, StarException> PaneManager::inputTargetPanes() const {
  auto captured = keyboardCapturedPane();
  if (!captured) return std::unexpected(std::move(captured.error()));
  if (*captured) return List<std::shared_ptr<Pane>>{*captured};
  List<std::shared_ptr<Pane>> targets;
  for (auto const& layer : m_displayedPanes) {
    for (auto const& entry : layer.second) {
      if (!entry.first->interactive())
        continue;
      targets.push_back(entry.first);
      if (layer.first == PaneLayer::ModalWindow)
        return targets;
    }
  }
  return targets;
}


std::expected<bool, StarException> PaneManager::sendInputEvent(InputEvent const& event) {
  if (std::holds_alternative<MouseMoveEvent>(event)) {
    m_tooltipLastMousePos = *m_context.mousePosition(event);

    for (auto const& layerPair : m_displayedPanes) {
      for (auto const& panePair : layerPair.second) {
        if (panePair.first->interactive() && panePair.first->dragActive()) {
          panePair.first->drag(*m_context.mousePosition(event));
          return true;
        }
      }
    }
  }

  if (std::holds_alternative<MouseButtonDownEvent>(event)) {
    m_tooltipShowTimer.reset();
    if (m_activeTooltip) {
      auto dismissed = dismiss(m_activeTooltip);
      if (!dismissed)
        return std::unexpected(std::move(dismissed.error()));
      m_activeTooltip.reset();
      m_tooltipParentPane.reset();
      m_tooltipShowTimer.reset();
    }
  }

  if (std::holds_alternative<MouseButtonUpEvent>(event)) {
    for (auto const& layerPair : m_displayedPanes) {
      for (auto const& panePair : layerPair.second) {
        if (panePair.first->interactive() && panePair.first->dragActive()) {
          panePair.first->setDragActive(false, {});
          return true;
        }
      }
    }
  }

  // The gui close event can only be intercepted by a pane that has captured
  // the keyboard otherwise it will always be used to close first before being
  // a normal event. This is so a window can control its own closing if it
  // really needs to (like the keybindings window).
  if (std::holds_alternative<KeyDownEvent>(event) && m_context.actions(event).contains(InterfaceAction::GuiClose)) {
    for (auto const& layerPair : m_displayedPanes) {
      if (layerPair.first != PaneLayer::ModalWindow && layerPair.first != PaneLayer::Window)
        continue;
      for (auto const& panePair : layerPair.second) {
        if (!panePair.first->interactive())
          continue;
        auto dismissed = dismiss(panePair.first);
        if (!dismissed)
          return std::unexpected(std::move(dismissed.error()));
        return true;
      }
    }
  }

  // If there is a pane that has captured the keyboard, keyboard events will
  // ONLY be sent to it.
  auto keyCapturePane = keyboardCapturedPane();
  if (!keyCapturePane)
    return std::unexpected(std::move(keyCapturePane.error()));
  if (*keyCapturePane && (std::holds_alternative<KeyDownEvent>(event) || std::holds_alternative<KeyUpEvent>(event) || std::holds_alternative<TextInputEvent>(event)))
    return (*keyCapturePane)->sendEvent(event);

  bool foundModal = false;
  for (auto& layerPair : m_displayedPanes) {
    for (auto const& panePair : auto(layerPair.second)) {
      if (!panePair.first->interactive())
        continue;
      auto consumed = panePair.first->sendEvent(event);
      if (!consumed)
        return std::unexpected(std::move(consumed.error()));
      if (*consumed) {
        if (std::holds_alternative<MouseButtonDownEvent>(event)
            && (layerPair.first == PaneLayer::Window || layerPair.first == PaneLayer::ModalWindow)) {
          auto raised = layerPair.second.move_to_front(panePair.first);
          if (!raised)
            return std::unexpected(std::move(raised.error()));
        }
        return true;
      }
      // If any modal windows are shown, Only the first modal window should
      // have a chance to consume the input event and all other panes below it
      // including different layers should ignore it.
      if (layerPair.first == PaneLayer::ModalWindow) {
        foundModal = true;
        break;
      }
    }
    if (foundModal)
      break;
  }

  return false;
}

std::expected<void, StarException> PaneManager::render() {
  auto bounds = windowSize();
  if (!bounds)
    return std::unexpected(std::move(bounds.error()));
  if (m_backgroundWidget) {
    auto size = m_backgroundWidget->size();
    m_backgroundWidget->setPosition(Vec2I(((*bounds)[0] - size[0]) / 2, ((*bounds)[1] - size[1]) / 2));
    auto rendered = m_backgroundWidget->render(RectI(Vec2I(), *bounds));
    if (!rendered)
      return std::unexpected(std::move(rendered.error()));
  }
  return renderLayers(PaneLayer::Tooltip, PaneLayer::Background);
}

std::expected<void, StarException> PaneManager::renderLayers(PaneLayer front, PaneLayer back) {
  auto bounds = windowSize();
  if (!bounds)
    return std::unexpected(std::move(bounds.error()));

  // A split render must not let its first phase consume a scale change before
  // panes in later phases have had their relative positions adjusted.
  if (m_prevInterfaceScale != m_context.interfaceScale()) {
    for (auto const& layerPair : m_displayedPanes) {
      for (auto const& panePair : layerPair.second) {
        if (!panePair.first->active())
          continue;
        auto position = calculateNewInterfacePosition(panePair.first, m_context.interfaceScale() / m_prevInterfaceScale);
        if (!position)
          return std::unexpected(std::move(position.error()));
        panePair.first->setPosition(*position);
      }
    }
    m_prevInterfaceScale = m_context.interfaceScale();
  }

  for (auto const& layerPair : std::views::reverse(m_displayedPanes)) {
    if (layerPair.first < front || layerPair.first > back)
      continue;
    for (auto const& panePair : std::views::reverse(layerPair.second)) {
      if (panePair.first->active()) {
        auto offset = calculatePaneOffset(panePair.first);
        if (!offset)
          return std::unexpected(std::move(offset.error()));
        panePair.first->setDrawingOffset(*offset);
        auto rendered = panePair.first->render(RectI(Vec2I(), *bounds));
        if (!rendered)
          return std::unexpected(std::move(rendered.error()));
      }
    }
  }

  auto reset = m_context.resetInterfaceScissorRect();
  if (!reset)
    return std::unexpected(std::move(reset.error()));
  m_prevInterfaceScale = m_context.interfaceScale();
  return {};
}

std::expected<void, StarException> PaneManager::update(float dt) {
  auto newTooltipParentPane = getPaneAt(m_tooltipLastMousePos);

  bool updateTooltip = m_tooltipShowTimer.tick(dt) || (m_activeTooltip && (
    vmag(m_tooltipInitialPosition - m_tooltipLastMousePos) > m_tooltipMouseoverRadius
    || m_tooltipParentPane != newTooltipParentPane
    || !m_tooltipParentPane->inWindow(m_tooltipLastMousePos))); 

  if (updateTooltip) {
    if (m_activeTooltip) {
      auto dismissed = dismiss(m_activeTooltip);
      if (!dismissed)
        return std::unexpected(std::move(dismissed.error()));
      m_activeTooltip.reset();
      m_tooltipParentPane.reset();
    }

    m_tooltipShowTimer.reset();
    if (newTooltipParentPane) {
      auto tooltip = newTooltipParentPane->createTooltip(m_tooltipLastMousePos);
      if (!tooltip)
        return std::unexpected(std::move(tooltip.error()));
      if (*tooltip) {
        m_activeTooltip = std::move(*tooltip);
        m_tooltipParentPane = std::move(newTooltipParentPane);
        m_tooltipInitialPosition = m_tooltipLastMousePos;
        auto displayed = displayPane(PaneLayer::Tooltip, m_activeTooltip, {});
        if (!displayed)
          return std::unexpected(std::move(displayed.error()));
      }
    }
  }

  if (m_activeTooltip) {
    Vec2I offsetDirection = Vec2I::filled(1);
    Vec2I offsetAdjust = Vec2I();

    auto width = m_context.windowWidth();
    if (!width)
      return std::unexpected(std::move(width.error()));
    if (m_tooltipLastMousePos[0] + m_tooltipMouseOffset[0] + m_activeTooltip->size()[0]
        > static_cast<int>(*width) / m_context.interfaceScale()) {
      offsetDirection[0] = -1;
      offsetAdjust[0] = -m_activeTooltip->size()[0];
    }

    if (m_tooltipLastMousePos[1] + m_tooltipMouseOffset[1] - m_activeTooltip->size()[1] < 0)
      offsetDirection[1] = -1;
    else
      offsetAdjust[1] = -m_activeTooltip->size()[1];

    m_activeTooltip->setPosition(m_tooltipLastMousePos + (offsetAdjust + m_tooltipMouseOffset.piecewiseMultiply(offsetDirection)));
  }

  for (auto const& layerPair : m_displayedPanes) {
    for (auto const& panePair : auto(layerPair.second)) {
      if (panePair.first->isDismissed()) {
        auto dismissed = dismiss(panePair.first);
        if (!dismissed)
          return std::unexpected(std::move(dismissed.error()));
      }
    }
  }

  for (auto const& layerPair : std::views::reverse(m_displayedPanes)) {
    for (auto const& panePair : std::views::reverse(layerPair.second)) {
      auto ticked = panePair.first->tick(dt);
      if (!ticked)
        return std::unexpected(std::move(ticked.error()));
      if (panePair.first->active()) {
        auto updated = panePair.first->update(dt);
        if (!updated)
          return std::unexpected(std::move(updated.error()));
      }
    }
  }
  return {};
}

std::expected<Vec2I, StarException> PaneManager::windowSize() const {
  auto size = m_context.windowInterfaceSize();
  if (!size)
    return std::unexpected(std::move(size.error()));
  return Vec2I(*size);
}

std::expected<Vec2I, StarException> PaneManager::calculatePaneOffset(std::shared_ptr<Pane> const& pane) const {
  auto anchor = pane->anchor();
  if (anchor == PaneAnchor::None || anchor == PaneAnchor::BottomLeft)
    return pane->anchorOffset();
  auto bounds = windowSize();
  if (!bounds)
    return std::unexpected(std::move(bounds.error()));
  return pane->anchorOffset() + paneAnchorBase(anchor, *bounds, pane->size());
}

std::expected<Vec2I, StarException> PaneManager::calculateNewInterfacePosition(std::shared_ptr<Pane> const& pane, float interfaceScaleRatio) const {
  Vec2F position(pane->relativePosition());
  Vec2F size(pane->size());
  Mat3F scale;
  switch (pane->anchor()) {
    case PaneAnchor::None: {
      auto bounds = windowSize();
      if (!bounds)
        return std::unexpected(std::move(bounds.error()));
      scale = Mat3F::scaling(interfaceScaleRatio, Vec2F(*bounds) / 2);
      break;
    }
    case PaneAnchor::BottomLeft:
      scale = Mat3F::scaling(interfaceScaleRatio);
      break;
    case PaneAnchor::BottomRight:
      scale = Mat3F::scaling(interfaceScaleRatio, {size[0], 0});
      break;
    case PaneAnchor::TopLeft:
      scale = Mat3F::scaling(interfaceScaleRatio, {0, size[1]});
      break;
    case PaneAnchor::TopRight:
      scale = Mat3F::scaling(interfaceScaleRatio, size);
      break;
    case PaneAnchor::CenterTop:
      scale = Mat3F::scaling(interfaceScaleRatio, {size[0] / 2, size[1]});
      break;
    case PaneAnchor::CenterBottom:
      scale = Mat3F::scaling(interfaceScaleRatio, {size[0] / 2, 0});
      break;
    case PaneAnchor::CenterLeft:
      scale = Mat3F::scaling(interfaceScaleRatio, {0, size[1] / 2});
      break;
    case PaneAnchor::CenterRight:
      scale = Mat3F::scaling(interfaceScaleRatio, {size[0], size[1] / 2});
      break;
    case PaneAnchor::Center:
      scale = Mat3F::scaling(interfaceScaleRatio, size / 2);
      break;
    default: {
      auto bounds = windowSize();
      if (!bounds)
        return std::unexpected(std::move(bounds.error()));
      scale = Mat3F::scaling(interfaceScaleRatio, Vec2F(*bounds) / 2);
    }
  }
  return Vec2I::round((scale * Vec3F(position, 0)).vec2());
}

std::expected<bool, StarException> PaneManager::dismiss(std::shared_ptr<Pane> const& pane) {
  bool dismissed = false;
  for (auto& layerPair : m_displayedPanes) {
    if (auto panePair = layerPair.second.try_extract_value(pane)) {
      dismissed = true;
      auto hidden = panePair->first->dismissed();
      if (!hidden)
        return std::unexpected(std::move(hidden.error()));
      if (panePair->second) {
        auto notified = panePair->second(pane);
        if (!notified)
          return std::unexpected(std::move(notified.error()));
      }
    }
  }

  return dismissed;
}

}

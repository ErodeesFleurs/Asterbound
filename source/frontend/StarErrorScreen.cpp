import std;
import star.format;
import star.json;
import star.json_extra;
import star.vector;
import star.string;
import star.input_event;
import star.data_stream;
import star.color;
import star.list;
import star.image_processing;
import star.hash;
import star.string_view;
import star.thread;
import star.directives;
import star.asset_path;
import star.casting;
import star.font;
import star.bi_map;
import star.io_device;
import star.ref_ptr;
import star.logging;
import star.text;
import star.listener;
import star.game_types;
import star.set;
import star.audio;
import star.map;
import star.ordered_map;



import star.drawable;

import star.animation;


import star.interface_cursor;


import star.error_screen;
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
import star.asset_texture_group;
import star.drawable_painter;
import star.gui_types;
import star.key_bindings;
import star.mixer;
import star.gui_context;
import star.widget;
import star.widget_parsing;
import star.gui_reader;
import star.pane;
import star.game_timers;
import star.pane_manager;
import star.label_widget;

namespace Star {

ErrorScreen::ErrorScreen(GuiContext& context) : m_guiContext(context), m_accepted(true) {}

std::expected<std::shared_ptr<ErrorScreen>, StarException> ErrorScreen::create(GuiContext& context) {
  auto screen = std::shared_ptr<ErrorScreen>(new ErrorScreen(context));
  auto cursor = InterfaceCursor::create(context);
  if (!cursor)
    return std::unexpected(std::move(cursor.error()));
  screen->m_cursor = std::move(*cursor);
  auto manager = PaneManager::create(context);
  if (!manager)
    return std::unexpected(std::move(manager.error()));
  screen->m_paneManager = std::move(*manager);
  auto assets = context.root().assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  auto pane = Pane::create(context);
  if (!pane)
    return std::unexpected(std::move(pane.error()));
  screen->m_errorPane = std::move(*pane);
  GuiReader reader(context);
  reader.registerCallback("btnOk", [screenRef = std::ref(*screen)](Widget&) -> std::expected<void, StarException> {
    screenRef.get().m_accepted = true;
    return {};
  });
  auto layout = (*assets)->json("/interface/windowconfig/error.config:paneLayout");
  if (!layout)
    return std::unexpected(std::move(layout.error()));
  auto constructed = reader.construct(*layout, *screen->m_errorPane);
  if (!constructed)
    return std::unexpected(std::move(constructed.error()));
  return screen;
}

std::expected<void, StarException> ErrorScreen::setMessage(String const& errorMessage) {
  auto label = m_errorPane->fetchChild<LabelWidget>("labelError");
  if (!label)
    return std::unexpected(std::move(label.error()));
  if (!*label)
    return std::unexpected(GUIBuilderException("Missing error message label"));
  auto text = (*label)->setText(errorMessage);
  if (!text)
    return std::unexpected(std::move(text.error()));
  m_accepted = false;
  if (!m_paneManager->isDisplayed(m_errorPane)) {
    return m_paneManager->displayPane(PaneLayer::Window, m_errorPane, [this](std::shared_ptr<Pane> const&) -> std::expected<void, StarException> {
      m_accepted = true;
      return {};
    });
  }
  return {};
}

bool ErrorScreen::accepted() {
  return m_accepted;
}

std::expected<void, StarException> ErrorScreen::render() {
  return m_paneManager->render();
}

std::expected<bool, StarException> ErrorScreen::handleInputEvent(InputEvent const& event) {
  if (std::holds_alternative<MouseMoveEvent>(event))
    m_cursorScreenPos = Vec2I(std::get<MouseMoveEvent>(event).mouse_position);

  return m_paneManager->sendInputEvent(event);
}

std::expected<void, StarException> ErrorScreen::update(float dt) {
  auto updated = m_paneManager->update(dt);
  if (!updated)
    return std::unexpected(std::move(updated.error()));
  return m_cursor->update(dt);
}

std::expected<void, StarException> ErrorScreen::renderCursor() {
  Vec2I cursorPos = m_cursorScreenPos;
  Vec2I cursorSize = m_cursor->size();
  Vec2I cursorOffset = m_cursor->offset();
  float cursorScale = m_cursor->scale(interfaceScale());
  auto cursorDrawable = m_cursor->drawable();
  if (!cursorDrawable)
    return std::unexpected(std::move(cursorDrawable.error()));
  cursorPos[0] -= cursorOffset[0] * cursorScale;
  cursorPos[1] -= (cursorSize[1] - cursorOffset[1]) * cursorScale;
  auto setCursor = m_guiContext.get().trySetCursor(*cursorDrawable, cursorOffset, cursorScale);
  if (!setCursor)
    return std::unexpected(std::move(setCursor.error()));
  if (!*setCursor)
    return m_guiContext.get().drawDrawable(*cursorDrawable, Vec2F(cursorPos), cursorScale);
  return {};
}

float ErrorScreen::interfaceScale() const {
  return m_guiContext.get().interfaceScale();
}

std::expected<unsigned, StarException> ErrorScreen::windowHeight() const {
  return m_guiContext.get().windowHeight();
}

std::expected<unsigned, StarException> ErrorScreen::windowWidth() const {
  return m_guiContext.get().windowWidth();
}

}

import std;
import star.json;
import star.json_extra;
import star.casting;
import star.input_event;
import star.color;
import star.font;
import star.list;
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
import star.widget_parsing;
import star.gui_reader;
import star.pane;

namespace Star {

GuiReader::GuiReader(GuiContext& context) : WidgetParser(context) {
  m_constructors["background"] = [&reader = *this](
      String const& name, Json const& config) { return reader.backgroundHandler(name, config); };
  m_constructors["button"] = [&reader = *this](String const& name, Json const& config) { return reader.buttonHandler(name, config); };
  m_constructors["itemslot"] = [&reader = *this](String const& name, Json const& config) { return reader.itemSlotHandler(name, config); };
  m_constructors["itemgrid"] = [&reader = *this](String const& name, Json const& config) { return reader.itemGridHandler(name, config); };
  m_constructors["list"] = [&reader = *this](String const& name, Json const& config) { return reader.listHandler(name, config); };
  m_constructors["panefeature"] = [&reader = *this](
      String const& name, Json const& config) { return reader.paneFeatureHandler(name, config); };
  m_constructors["radioGroup"] = [&reader = *this](
      String const& name, Json const& config) { return reader.radioGroupHandler(name, config); };
  m_constructors["spinner"] = [&reader = *this](String const& name, Json const& config) { return reader.spinnerHandler(name, config); };
  m_constructors["slider"] = [&reader = *this](String const& name, Json const& config) { return reader.sliderHandler(name, config); };
  m_constructors["textbox"] = [&reader = *this](String const& name, Json const& config) { return reader.textboxHandler(name, config); };
  m_constructors["title"] = [&reader = *this](String const& name, Json const& config) { return reader.titleHandler(name, config); };
  m_constructors["stack"] = [&reader = *this](String const& name, Json const& config) { return reader.stackHandler(name, config); };
  m_constructors["tabSet"] = [&reader = *this](String const& name, Json const& config) { return reader.tabSetHandler(name, config); };
  m_constructors["scrollArea"] = [&reader = *this](
      String const& name, Json const& config) { return reader.scrollAreaHandler(name, config); };
}

std::expected<WidgetConstructResult, StarException> GuiReader::titleHandler(String const&, Json const& config) {
  auto pane = m_pane.lock();
  if (!pane)
    return std::unexpected(StarException("Only Pane controls support the 'title' command"));
  auto title = config.getString("title", "");
  if (!title)
    return std::unexpected(std::move(title.error()));
  auto subtitle = config.getString("subtitle", "");
  if (!subtitle)
    return std::unexpected(std::move(subtitle.error()));
  auto iconConfig = config.get("icon", Json());
  if (!iconConfig)
    return std::unexpected(std::move(iconConfig.error()));
  if (iconConfig->isNull()) {
    pane->setTitleString(*title, *subtitle);
  } else {
    auto icon = makeSingle("icon", *iconConfig);
    if (!icon)
      return std::unexpected(std::move(icon.error()));
    if (!*icon)
      return std::unexpected(WidgetParserException("Title specified an incompatible icon type"));
    auto result = pane->setTitle(*icon, *title, *subtitle);
    if (!result)
      return std::unexpected(std::move(result.error()));
  }
  return WidgetConstructResult{};
}

std::expected<WidgetConstructResult, StarException> GuiReader::paneFeatureHandler(String const&, Json const& config) {
  auto pane = m_pane.lock();
  if (!pane)
    return std::unexpected(StarException("Only Pane controls support the 'panefeature' command"));
  auto anchorName = config.getString("anchor", "None");
  if (!anchorName)
    return std::unexpected(std::move(anchorName.error()));
  if (!PaneAnchorNames)
    return std::unexpected(PaneAnchorNames.error());
  auto anchor = PaneAnchorNames->get_left(*anchorName);
  if (!anchor)
    return std::unexpected(std::move(anchor.error()));
  pane->setAnchor(anchor->get());
  auto hasOffset = config.contains("offset");
  if (!hasOffset)
    return std::unexpected(std::move(hasOffset.error()));
  if (*hasOffset) {
    auto offsetConfig = config.get("offset");
    if (!offsetConfig)
      return std::unexpected(std::move(offsetConfig.error()));
    auto offset = jsonToVec2I(*offsetConfig);
    if (!offset)
      return std::unexpected(std::move(offset.error()));
    pane->setAnchorOffset(*offset);
  }
  auto positionLocked = config.getBool("positionLocked", false);
  if (!positionLocked)
    return std::unexpected(std::move(positionLocked.error()));
  if (*positionLocked)
    pane->lockPosition();
  return WidgetConstructResult{};
}

std::expected<WidgetConstructResult, StarException> GuiReader::backgroundHandler(String const&, Json const& config) {
  auto pane = m_pane.lock();
  if (!pane)
    return std::unexpected(StarException("Only Pane controls support the 'background' command"));
  auto header = config.getString("fileHeader", "");
  if (!header)
    return std::unexpected(std::move(header.error()));
  auto body = config.getString("fileBody", "");
  if (!body)
    return std::unexpected(std::move(body.error()));
  auto footer = config.getString("fileFooter", "");
  if (!footer)
    return std::unexpected(std::move(footer.error()));
  auto result = pane->setBG(*header, *body, *footer);
  if (!result)
    return std::unexpected(std::move(result.error()));
  return WidgetConstructResult{};
}

}

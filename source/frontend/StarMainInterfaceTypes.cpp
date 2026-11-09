import std;
import star.format;
import star.json;
import star.json_extra;
import star.list;
import star.image_processing;
import star.hash;
import star.data_stream;
import star.string_view;
import star.thread;
import star.directives;
import star.asset_path;
import star.bi_map;
import star.game_types;
import star.casting;
import star.input_event;
import star.color;
import star.font;
import star.io_device;
import star.ref_ptr;
import star.logging;
import star.version;
import star.string;
import star.text;
import star.listener;
import star.set;
import star.audio;
import star.map;
import star.ordered_map;


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
import star.registered_pane_manager;


import star.main_interface_types;


import star.image_metadata_database;

namespace Star {

std::expected<std::unique_ptr<MainInterfaceConfig>, StarException> MainInterfaceConfig::loadFromAssets(GuiContext& context) {
  auto& root = context.root();
  auto assets = root.assets();
  if (!assets) return std::unexpected(std::move(assets.error()));


  auto config = std::make_unique<MainInterfaceConfig>();

  {
    auto json = (*assets)->json("/interface.config:textStyle");
    if (!json) return std::unexpected(std::move(json.error()));
    auto value = TextStyle::create(*json);
    if (!value) return std::unexpected(std::move(value.error()));
    config->textStyle = std::move(*value);
  }

  {
    auto json = (*assets)->json("/interface.config:itemCountRightAnchor");
    if (!json) return std::unexpected(std::move(json.error()));
    auto value = jsonToVec2I(*json);
    if (!value) return std::unexpected(std::move(value.error()));
    config->itemCountRightAnchor = std::move(*value);
  }
  {
    auto json = (*assets)->json("/interface.config:inventoryItemMouseOffset");
    if (!json) return std::unexpected(std::move(json.error()));
    auto value = jsonToVec2I(*json);
    if (!value) return std::unexpected(std::move(value.error()));
    config->inventoryItemMouseOffset = std::move(*value);
  }



  {
    auto json = (*assets)->json("/interface.config:renderVirtualCursor");
    if (!json) return std::unexpected(std::move(json.error()));
    auto value = json->toBool();
    if (!value) return std::unexpected(std::move(value.error()));
    config->renderVirtualCursor = std::move(*value);
  }
  {
    auto json = (*assets)->json("/interface.config:cursorItemSlot");
    if (!json) return std::unexpected(std::move(json.error()));
    config->cursorItemSlot = std::move(*json);
  }

  {
    auto json = (*assets)->json("/interface.config:debugOffset");
    if (!json) return std::unexpected(std::move(json.error()));
    auto value = jsonToVec2I(*json);
    if (!value) return std::unexpected(std::move(value.error()));
    config->debugOffset = std::move(*value);
  }
  {
    auto json = (*assets)->json("/interface.config:debugTextStyle");
    if (!json) return std::unexpected(std::move(json.error()));
    auto value = TextStyle::create(*json);
    if (!value) return std::unexpected(std::move(value.error()));
    config->debugTextStyle = std::move(*value);
  }
  {
    auto json = (*assets)->json("/interface.config:debugSpatialClearTime");
    if (!json) return std::unexpected(std::move(json.error()));
    auto value = json->toFloat();
    if (!value) return std::unexpected(std::move(value.error()));
    config->debugSpatialClearTime = std::move(*value);
  }
  {
    auto json = (*assets)->json("/interface.config:debugMapClearTime");
    if (!json) return std::unexpected(std::move(json.error()));
    auto value = json->toFloat();
    if (!value) return std::unexpected(std::move(value.error()));
    config->debugMapClearTime = std::move(*value);
  }
  {
    auto json = (*assets)->json("/interface.config:debugBackgroundColor");
    if (!json) return std::unexpected(std::move(json.error()));
    auto value = jsonToColor(*json);
    if (!value) return std::unexpected(std::move(value.error()));
    config->debugBackgroundColor = std::move(*value);
  }
  {
    auto json = (*assets)->json("/interface.config:debugBackgroundPad");
    if (!json) return std::unexpected(std::move(json.error()));
    auto value = json->toUInt();
    if (!value) return std::unexpected(std::move(value.error()));
    config->debugBackgroundPad = std::move(*value);
  }

  auto macros = (*assets)->scanExtension("macros");
  if (!macros) return std::unexpected(std::move(macros.error()));
  for (auto const& path : macros->get()) {
    auto json = (*assets)->json(path.string());
    if (!json) return std::unexpected(std::move(json.error()));
    auto entries = json->iterateObject();
    if (!entries) return std::unexpected(std::move(entries.error()));
    for (auto const& pair : *entries) {
      auto commands = jsonToStringList(pair.second);
      if (!commands) return std::unexpected(std::move(commands.error()));
      auto added = config->macroCommands.add(pair.first, std::move(*commands));
      if (!added) return std::unexpected(std::move(added.error()));
    }
  }

  return config;
}

}

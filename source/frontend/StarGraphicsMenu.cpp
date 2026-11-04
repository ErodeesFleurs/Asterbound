import std;
import star.math_common;
import star.json;
import star.xx_hash;
import star.json_extra;
import star.vector;
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
import star.io_device;
import star.ref_ptr;
import star.logging;
import star.string;
import star.text;
import star.asset_path;
import star.listener;
import star.game_types;
import star.set;
import star.audio;
import star.map;
import star.ordered_map;
import star.either;
import star.random;
import star.weighted_pool;
import star.strong_typedef;
import star.byte_array;
import star.data_stream_devices;

typedef struct ZSTD_CCtx_s ZSTD_CCtx;
typedef struct ZSTD_DCtx_s ZSTD_DCtx;
typedef ZSTD_DCtx ZSTD_DStream;
typedef ZSTD_CCtx ZSTD_CStream;
import star.zstd_compression;
import star.net_compatibility;
import star.format;
import star.id_map;
import star.worker_pool;

import star.sector_array_2d;
import star.perlin;
import star.btree;
import star.lru_cache;
import star.btree_database;
import star.rpc_promise;
import star.image;
import star.version;
import star.ordered_set;


import star.lua_root;
import star.lua_components;
import star.application;
import star.statistics_service;
import star.p2p_networking_service;
import star.user_generated_content_service;
import star.desktop_service;
import star.application_controller;
import star.renderer;
import star.asset_source;
import star.assets;
import star.root_base;
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
import star.host_address;
import star.celestial_coordinate;
import star.sky_types;
import star.animation;
import star.particle;
import star.weather_types;
import star.force_regions;
import star.world_parameters;
import star.celestial_parameters;
import star.chat_types;
import star.uuid;
import star.warping;
import star.item_descriptor;
import star.quest_descriptor;
import star.ai_types;
import star.socket;
import star.tcp;
import star.collision_block;
import star.liquid_types;
import star.tile_damage;
import star.tile_sector_array;
import star.world_layout;
import star.collision_generator;
import star.world_tiles;
import star.celestial_types;
import star.tile_modification;
import star.damage_types;
import star.status_types;
import star.damage;
import star.light_source;
import star.entity;
import star.interaction_types;
import star.world_geometry;
import star.wiring;
import star.interactive_entity;
import star.tile_entity;
import star.inspectable_entity;
import star.plant;
import star.plant_database;
import star.biome_placement;
import star.versioning_database;
import star.world_storage;
import star.player_types;
import star.sky_parameters;
import star.system_world;
import star.damage_manager;
import star.net_packets;
import star.net_packet_socket;
import star.universe_connection;
import star.entity_rendering_types;
import star.sky_render_data;
import star.parallax;
import star.cellular_light_array;
import star.cellular_lighting;
import star.world_render_data;
import star.world_structure;
import star.chat_action;
import star.entity_rendering;
import star.world;
import star.world_client_state;
import star.interpolation_tracker;
import star.ambient;
import star.weather;
import star.world_client;
import star.world_client_thread;
import star.universe;
// TODO: make this more thread safe
import star.universe_client;


import star.graphics_menu;
import star.configuration;
import star.widget_parsing;
import star.gui_reader;
import star.list_widget;
import star.label_widget;
import star.button_group;
import star.button_widget;
import star.image_widget;
import star.slider_bar;




import star.base_script_pane;


import star.shaders_menu;

namespace Star {

namespace {

std::expected<double, StarException> validatedRenderFpsLimit(Json const& value) {
  auto rate = value.toDouble();
  if (!rate)
    return std::unexpected(std::move(rate.error()));
  if (!isFinite(*rate) || *rate < 0 || *rate > std::numeric_limits<float>::max())
    return std::unexpected(GuiException("renderFpsLimit must be a finite nonnegative render rate"));
  return *rate;
}

}

GraphicsMenu::GraphicsMenu(GuiContext& guiContext, PaneManager& manager)
  : Pane(guiContext), m_paneManager(std::ref(manager)) {}

std::expected<std::shared_ptr<GraphicsMenu>, StarException> GraphicsMenu::create(GuiContext& guiContext, PaneManager& manager, std::shared_ptr<UniverseClient> client) {
  auto menu = std::shared_ptr<GraphicsMenu>(new GraphicsMenu(guiContext, manager));
  if (auto result = menu->initializePane(); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = menu->init(std::move(client)); !result)
    return std::unexpected(std::move(result.error()));
  return menu;
}
void GraphicsMenu::detachInterface() noexcept {
  m_paneManager.reset();
}

std::expected<void, StarException> GraphicsMenu::updateFloatSlider(String const& name, String const& key,
                                                                   List<float> const& values, SettingUpdate update) {
  auto slider = requireChild<SliderBarWidget>(name);
  if (!slider)
    return std::unexpected(std::move(slider.error()));
  auto value = values.at((*slider)->val());
  if (!value)
    return std::unexpected(std::move(value.error()));
  if (update == SettingUpdate::Live)
    return applyLiveSetting(key, value->get());
  m_localChanges.set(key, value->get());
  return syncGui();
}

std::expected<void, StarException> GraphicsMenu::applyLiveSetting(String const& key, Json const& value) {
  m_localChanges.set(key, value);
  auto config = context().root().configuration();
  if (!config)
    return std::unexpected(std::move(config.error()));
  if (auto result = (*config)->set(key, value); !result)
    return std::unexpected(std::move(result.error()));
  return syncGui();
}

std::expected<void, StarException> GraphicsMenu::init(std::shared_ptr<UniverseClient> client) {
  GuiReader reader(context());
  reader.registerCallback("cancel", [this](Widget&) -> std::expected<void, StarException> {
    dismiss();
    return {};
  });
  reader.registerCallback("accept", [this](Widget&) -> std::expected<void, StarException> {
    if (auto result = apply(); !result)
      return std::unexpected(std::move(result.error()));
    return applyWindowSettings();
  });
  reader.registerCallback("resSlider", [this](Widget&) -> std::expected<void, StarException> {
    auto slider = requireChild<SliderBarWidget>("resSlider");
    if (!slider)
      return std::unexpected(std::move(slider.error()));
    auto resolution = m_resList.at((*slider)->val());
    if (!resolution)
      return std::unexpected(std::move(resolution.error()));
    m_localChanges.set("fullscreenResolution", jsonFromVec2U(resolution->get()));
    return syncGui();
  });
  // Resolution and interface scale remain staged until the menu is accepted.
  reader.registerCallback("interfaceScaleSlider", [this](Widget&) -> std::expected<void, StarException> {
    return updateFloatSlider("interfaceScaleSlider", "interfaceScale", m_interfaceScaleList, SettingUpdate::Staged);
  });
  reader.registerCallback("renderFpsSlider", [this](Widget&) -> std::expected<void, StarException> {
    auto slider = requireChild<SliderBarWidget>("renderFpsSlider");
    if (!slider)
      return std::unexpected(std::move(slider.error()));
    auto rate = m_renderFpsList.at((*slider)->val());
    if (!rate)
      return std::unexpected(std::move(rate.error()));
    m_localChanges.set("renderFpsLimit", rate->get());
    return syncGui();
  });
  // Zoom and camera speed also update the running configuration immediately.
  reader.registerCallback("zoomSlider", [this](Widget&) -> std::expected<void, StarException> {
    return updateFloatSlider("zoomSlider", "zoomLevel", m_zoomList, SettingUpdate::Live);
  });
  reader.registerCallback("cameraSpeedSlider", [this](Widget&) -> std::expected<void, StarException> {
    return updateFloatSlider("cameraSpeedSlider", "cameraSpeedFactor", m_cameraSpeedList, SettingUpdate::Live);
  });
  for (auto const& [callback, key] : {
      std::pair{std::string_view("speechBubbleCheckbox"), std::string_view("speechBubbles")},
      std::pair{std::string_view("interactiveHighlightCheckbox"), std::string_view("interactiveHighlight")},
      std::pair{std::string_view("antiAliasingCheckbox"), std::string_view("antiAliasing")},
      std::pair{std::string_view("monochromeCheckbox"), std::string_view("monochromeLighting")},
      std::pair{std::string_view("newLightingCheckbox"), std::string_view("newLighting")},
      std::pair{std::string_view("hdrCheckbox"), std::string_view("hdr")}}) {
    reader.registerCallback(callback, [this, callback, key](Widget&) -> std::expected<void, StarException> {
      auto button = requireChild<ButtonWidget>(callback);
      if (!button)
        return std::unexpected(std::move(button.error()));
      return applyLiveSetting(key, (*button)->isChecked());
    });
  }
  reader.registerCallback("fullscreenCheckbox", [this](Widget&) -> std::expected<void, StarException> {
    auto button = requireChild<ButtonWidget>("fullscreenCheckbox");
    if (!button)
      return std::unexpected(std::move(button.error()));
    bool checked = (*button)->isChecked();
    m_localChanges.set("fullscreen", checked);
    if (checked)
      m_localChanges.set("borderless", !checked);
    return syncGui();
  });
  reader.registerCallback("borderlessCheckbox", [this](Widget&) -> std::expected<void, StarException> {
    auto button = requireChild<ButtonWidget>("borderlessCheckbox");
    if (!button)
      return std::unexpected(std::move(button.error()));
    bool checked = (*button)->isChecked();
    m_localChanges.set("borderless", checked);
    if (checked)
      m_localChanges.set("fullscreen", !checked);
    return syncGui();
  });
  // Texture settings and presentation timing remain staged until Apply.
  for (auto const& [callback, key] :
       {std::pair{std::string_view("textureLimitCheckbox"), std::string_view("limitTextureAtlasSize")},
        std::pair{std::string_view("multiTextureCheckbox"), std::string_view("useMultiTexturing")},
        std::pair{std::string_view("vsyncCheckbox"), std::string_view("vsync")}}) {
    reader.registerCallback(callback, [this, callback, key](Widget&) -> std::expected<void, StarException> {
      auto button = requireChild<ButtonWidget>(callback);
      if (!button)
        return std::unexpected(std::move(button.error()));
      m_localChanges.set(key, (*button)->isChecked());
      return syncGui();
    });
  }
  reader.registerCallback("hardwareCursorCheckbox", [this](Widget&) -> std::expected<void, StarException> {
    auto button = requireChild<ButtonWidget>("hardwareCursorCheckbox");
    if (!button)
      return std::unexpected(std::move(button.error()));
    bool checked = (*button)->isChecked();
    m_localChanges.set("hardwareCursor", checked);
    auto config = context().root().configuration();
    if (!config)
      return std::unexpected(std::move(config.error()));
    if (auto result = (*config)->set("hardwareCursor", checked); !result)
      return std::unexpected(std::move(result.error()));
    context().applicationController()->setCursorHardware(checked);
    return {};
  });
  reader.registerCallback("showShadersMenu", [this](Widget&) { return displayShaders(); });
  auto assets = context().root().assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  auto config = (*assets)->json("/interface/windowconfig/graphicsmenu.config");
  if (!config)
    return std::unexpected(std::move(config.error()));
  auto layout = config->get("paneLayout");
  if (!layout)
    return std::unexpected(std::move(layout.error()));
  auto interfaceScalesJson = (*assets)->json("/interface/windowconfig/graphicsmenu.config:interfaceScaleList");
  if (!interfaceScalesJson)
    return std::unexpected(std::move(interfaceScalesJson.error()));
  auto interfaceScales = jsonToFloatList(*interfaceScalesJson);
  if (!interfaceScales)
    return std::unexpected(std::move(interfaceScales.error()));
  m_interfaceScaleList = std::move(*interfaceScales);
  auto resolutionsJson = (*assets)->json("/interface/windowconfig/graphicsmenu.config:resolutionList");
  if (!resolutionsJson)
    return std::unexpected(std::move(resolutionsJson.error()));
  auto resolutions = jsonToVec2UList(*resolutionsJson);
  if (!resolutions)
    return std::unexpected(std::move(resolutions.error()));
  m_resList = std::move(*resolutions);
  auto zoomsJson = (*assets)->json("/interface/windowconfig/graphicsmenu.config:zoomList");
  if (!zoomsJson)
    return std::unexpected(std::move(zoomsJson.error()));
  auto zooms = jsonToFloatList(*zoomsJson);
  if (!zooms)
    return std::unexpected(std::move(zooms.error()));
  m_zoomList = std::move(*zooms);
  auto speedsJson = (*assets)->json("/interface/windowconfig/graphicsmenu.config:cameraSpeedList");
  if (!speedsJson)
    return std::unexpected(std::move(speedsJson.error()));
  auto speeds = jsonToFloatList(*speedsJson);
  if (!speeds)
    return std::unexpected(std::move(speeds.error()));
  m_cameraSpeedList = std::move(*speeds);
  auto renderFpsJson = config->getArray("renderFpsList");
  if (!renderFpsJson)
    return std::unexpected(std::move(renderFpsJson.error()));
  for (auto const& value : *renderFpsJson) {
    auto rate = validatedRenderFpsLimit(value);
    if (!rate)
      return std::unexpected(std::move(rate.error()));
    m_renderFpsPresets.append(*rate);
  }
  if (m_renderFpsPresets.empty() || m_renderFpsPresets.front() != 0
      || !std::is_sorted(m_renderFpsPresets.begin(), m_renderFpsPresets.end()))
    return std::unexpected(GuiException("Graphics menu render FPS presets must be sorted and begin with unlimited (0)"));
  if (m_interfaceScaleList.empty() || m_resList.empty() || m_zoomList.empty() || m_cameraSpeedList.empty())
    return std::unexpected(GuiException("Graphics menu slider value lists must not be empty"));
  if (auto result = reader.construct(*layout, *this); !result)
    return std::unexpected(std::move(result.error()));
  for (auto const& [name, count] : {
      std::pair{std::string_view("interfaceScaleSlider"), m_interfaceScaleList.size()},
      std::pair{std::string_view("resSlider"), m_resList.size()},
      std::pair{std::string_view("zoomSlider"), m_zoomList.size()},
      std::pair{std::string_view("cameraSpeedSlider"), m_cameraSpeedList.size()}}) {
    auto slider = requireChild<SliderBarWidget>(name);
    if (!slider)
      return std::unexpected(std::move(slider.error()));
    if (auto result = (*slider)->setRange(0, count - 1, 1); !result)
      return std::unexpected(std::move(result.error()));
  }
  if (auto result = initConfig(); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = syncGui(); !result)
    return std::unexpected(std::move(result.error()));
  auto shadersPath = config->getString("shadersPanePath", "/interface/opensb/shaders/shaders.config");
  if (!shadersPath)
    return std::unexpected(std::move(shadersPath.error()));
  auto shadersConfig = (*assets)->json(*shadersPath);
  if (!shadersConfig)
    return std::unexpected(std::move(shadersConfig.error()));
  auto shaders = ShadersMenu::create(context(), *shadersConfig, std::move(client));
  if (!shaders)
    return std::unexpected(std::move(shaders.error()));
  m_shadersMenu = std::move(*shaders);
  return {};
}

std::expected<void, StarException> GraphicsMenu::show() {
  if (auto result = Pane::show(); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = initConfig(); !result)
    return std::unexpected(std::move(result.error()));
  return syncGui();
}

std::expected<void, StarException> GraphicsMenu::dismissed() {
  return Pane::dismissed();
}

std::expected<void, StarException> GraphicsMenu::toggleFullscreen() {
  auto fullscreenSetting = m_localChanges.get("fullscreen");
  if (!fullscreenSetting)
    return std::unexpected(std::move(fullscreenSetting.error()));
  auto fullscreen = fullscreenSetting->get().toBool();
  if (!fullscreen)
    return std::unexpected(std::move(fullscreen.error()));
  auto borderlessSetting = m_localChanges.get("borderless");
  if (!borderlessSetting)
    return std::unexpected(std::move(borderlessSetting.error()));
  auto borderless = borderlessSetting->get().toBool();
  if (!borderless)
    return std::unexpected(std::move(borderless.error()));
  auto config = context().root().configuration();
  if (!config)
    return std::unexpected(std::move(config.error()));
  m_localChanges.set("fullscreen", !(*fullscreen || *borderless));
  if (auto result = (*config)->set("fullscreen", !(*fullscreen || *borderless)); !result)
    return std::unexpected(std::move(result.error()));
  m_localChanges.set("borderless", false);
  if (auto result = (*config)->set("borderless", false); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = applyWindowSettings(); !result)
    return std::unexpected(std::move(result.error()));
  return syncGui();
}

StringList const GraphicsMenu::ConfigKeys = {
  "fullscreenResolution",
  "interfaceScale",
  "zoomLevel",
  "cameraSpeedFactor",
  "renderFpsLimit",
  "vsync",
  "speechBubbles",
  "interactiveHighlight",
  "fullscreen",
  "borderless",
  "limitTextureAtlasSize",
  "useMultiTexturing",
  "antiAliasing",
  "hardwareCursor",
  "monochromeLighting",
  "newLighting",
  "hdr"
};

std::expected<void, StarException> GraphicsMenu::initConfig() {
  auto configuration = context().root().configuration();
  if (!configuration)
    return std::unexpected(std::move(configuration.error()));
  for (auto const& key : ConfigKeys) {
    auto value = (*configuration)->get(key);
    if (!value)
      return std::unexpected(std::move(value.error()));
    m_localChanges.set(key, std::move(*value));
  }
  auto fps = m_localChanges.get("renderFpsLimit");
  if (!fps)
    return std::unexpected(std::move(fps.error()));
  auto rate = validatedRenderFpsLimit(fps->get());
  if (!rate)
    return std::unexpected(std::move(rate.error()));
  // Include the exact configured value, rather than rounding custom limits to a preset.
  m_renderFpsList = m_renderFpsPresets;
  auto it = std::lower_bound(m_renderFpsList.begin(), m_renderFpsList.end(), *rate);
  if (it == m_renderFpsList.end() || *it != *rate)
    m_renderFpsList.insert(it, *rate);
  auto slider = requireChild<SliderBarWidget>("renderFpsSlider");
  if (!slider)
    return std::unexpected(std::move(slider.error()));
  if (auto result = (*slider)->setRange(0, m_renderFpsList.size() - 1, 1); !result)
    return std::unexpected(std::move(result.error()));
  return {};
}

std::expected<void, StarException> GraphicsMenu::syncGui() {
  auto setRequiredLabel = [this](String const& name, String const& text) -> std::expected<void, StarException> {
    auto label = requireChild<LabelWidget>(name);
    if (!label)
      return std::unexpected(std::move(label.error()));
    return (*label)->setText(text);
  };
  auto resolutionSetting = m_localChanges.get("fullscreenResolution");
  if (!resolutionSetting)
    return std::unexpected(std::move(resolutionSetting.error()));
  auto resolution = jsonToVec2U(resolutionSetting->get());
  if (!resolution)
    return std::unexpected(std::move(resolution.error()));
  auto resSlider = requireChild<SliderBarWidget>("resSlider");
  if (!resSlider)
    return std::unexpected(std::move(resSlider.error()));
  auto resIt = std::lower_bound(m_resList.begin(), m_resList.end(), *resolution, [](Vec2U const& a, Vec2U const& b) {
    return a[0] * a[1] < b[0] * b[1];
  });
  if (resIt != m_resList.end()) {
    std::size_t index = std::min<std::size_t>(resIt - m_resList.begin(), m_resList.size() - 1);
    if (auto result = (*resSlider)->setVal(index, false); !result)
      return std::unexpected(std::move(result.error()));
  } else if (auto result = (*resSlider)->setVal(m_resList.size() - 1); !result) {
    return std::unexpected(std::move(result.error()));
  }
  if (auto result = setRequiredLabel("resValueLabel", strf("{}x{}", (*resolution)[0], (*resolution)[1])); !result)
    return std::unexpected(std::move(result.error()));

  auto scaleSetting = m_localChanges.get("interfaceScale");
  if (!scaleSetting)
    return std::unexpected(std::move(scaleSetting.error()));
  auto scale = scaleSetting->get().optFloat();
  if (!scale)
    return std::unexpected(std::move(scale.error()));
  float interfaceScale = scale->value_or(0.0f);
  auto updateFloatSlider = [this](String const& name, List<float> const& values, float value) -> std::expected<void, StarException> {
    auto slider = requireChild<SliderBarWidget>(name);
    if (!slider)
      return std::unexpected(std::move(slider.error()));
    auto it = std::lower_bound(values.begin(), values.end(), value);
    if (it != values.end())
      return (*slider)->setVal(std::min<std::size_t>(it - values.begin(), values.size() - 1), false);
    return (*slider)->setVal(values.size() - 1);
  };
  if (auto result = updateFloatSlider("interfaceScaleSlider", m_interfaceScaleList, interfaceScale); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = setRequiredLabel("interfaceScaleValueLabel", interfaceScale != 0 ? toString(interfaceScale) : "AUTO"); !result)
    return std::unexpected(std::move(result.error()));
  auto zoomSetting = m_localChanges.get("zoomLevel");
  if (!zoomSetting)
    return std::unexpected(std::move(zoomSetting.error()));
  auto zoom = zoomSetting->get().toFloat();
  if (!zoom)
    return std::unexpected(std::move(zoom.error()));
  if (auto result = updateFloatSlider("zoomSlider", m_zoomList, *zoom); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = setRequiredLabel("zoomValueLabel", strf("{}x", *zoom)); !result)
    return std::unexpected(std::move(result.error()));
  auto speedSetting = m_localChanges.get("cameraSpeedFactor");
  if (!speedSetting)
    return std::unexpected(std::move(speedSetting.error()));
  auto speed = speedSetting->get().toFloat();
  if (!speed)
    return std::unexpected(std::move(speed.error()));
  if (auto result = updateFloatSlider("cameraSpeedSlider", m_cameraSpeedList, *speed); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = setRequiredLabel("cameraSpeedValueLabel", strf("{}x", *speed)); !result)
    return std::unexpected(std::move(result.error()));
  auto fpsSetting = m_localChanges.get("renderFpsLimit");
  if (!fpsSetting)
    return std::unexpected(std::move(fpsSetting.error()));
  auto fps = validatedRenderFpsLimit(fpsSetting->get());
  if (!fps)
    return std::unexpected(std::move(fps.error()));
  auto fpsSlider = requireChild<SliderBarWidget>("renderFpsSlider");
  if (!fpsSlider)
    return std::unexpected(std::move(fpsSlider.error()));
  auto fpsIt = std::lower_bound(m_renderFpsList.begin(), m_renderFpsList.end(), *fps);
  if (auto result = (*fpsSlider)->setVal(fpsIt - m_renderFpsList.begin(), false); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = setRequiredLabel("renderFpsValueLabel", *fps == 0 ? "UNLIMITED" : toString(*fps)); !result)
    return std::unexpected(std::move(result.error()));
  for (auto const& [name, key] : {
      std::pair{std::string_view("speechBubbleCheckbox"), std::string_view("speechBubbles")},
      std::pair{std::string_view("interactiveHighlightCheckbox"), std::string_view("interactiveHighlight")},
      std::pair{std::string_view("fullscreenCheckbox"), std::string_view("fullscreen")},
      std::pair{std::string_view("borderlessCheckbox"), std::string_view("borderless")},
      std::pair{std::string_view("vsyncCheckbox"), std::string_view("vsync")},
      std::pair{std::string_view("textureLimitCheckbox"), std::string_view("limitTextureAtlasSize")},
      std::pair{std::string_view("antiAliasingCheckbox"), std::string_view("antiAliasing")},
      std::pair{std::string_view("monochromeCheckbox"), std::string_view("monochromeLighting")},
      std::pair{std::string_view("hardwareCursorCheckbox"), std::string_view("hardwareCursor")}}) {
    auto button = requireChild<ButtonWidget>(name);
    if (!button)
      return std::unexpected(std::move(button.error()));
    auto setting = m_localChanges.get(key);
    if (!setting)
      return std::unexpected(std::move(setting.error()));
    auto checked = setting->get().toBool();
    if (!checked)
      return std::unexpected(std::move(checked.error()));
    (*button)->setChecked(*checked);
  }
  for (auto const& [name, key] : {
      std::pair{std::string_view("multiTextureCheckbox"), std::string_view("useMultiTexturing")},
      std::pair{std::string_view("newLightingCheckbox"), std::string_view("newLighting")},
      std::pair{std::string_view("hdrCheckbox"), std::string_view("hdr")}}) {
    auto button = requireChild<ButtonWidget>(name);
    if (!button)
      return std::unexpected(std::move(button.error()));
    auto setting = m_localChanges.get(key);
    if (!setting)
      return std::unexpected(std::move(setting.error()));
    auto checked = setting->get().optBool();
    if (!checked)
      return std::unexpected(std::move(checked.error()));
    (*button)->setChecked(checked->value_or(true));
  }
  return {};
}

std::expected<void, StarException> GraphicsMenu::apply() {
  auto configuration = context().root().configuration();
  if (!configuration)
    return std::unexpected(std::move(configuration.error()));
  auto fpsSetting = m_localChanges.get("renderFpsLimit");
  if (!fpsSetting)
    return std::unexpected(std::move(fpsSetting.error()));
  auto fps = validatedRenderFpsLimit(fpsSetting->get());
  if (!fps)
    return std::unexpected(std::move(fps.error()));
  auto vsyncSetting = m_localChanges.get("vsync");
  if (!vsyncSetting)
    return std::unexpected(std::move(vsyncSetting.error()));
  auto vsync = vsyncSetting->get().toBool();
  if (!vsync)
    return std::unexpected(std::move(vsync.error()));
  for (auto const& [key, value] : m_localChanges) {
    if (auto result = (*configuration)->set(key, value); !result)
      return std::unexpected(std::move(result.error()));
  }
  auto controller = context().applicationController();
  if (auto result = controller->setVSyncEnabled(*vsync); !result)
    return std::unexpected(std::move(result.error()));
  controller->setTargetRenderRate(static_cast<float>(*fps));
  return {};
}

std::expected<void, StarException> GraphicsMenu::displayShaders() {
  if (!m_paneManager)
    return std::unexpected(GuiException("Graphics menu is detached from its pane manager"));
  return m_paneManager->get().displayPane(PaneLayer::ModalWindow, m_shadersMenu);
}

std::expected<void, StarException> GraphicsMenu::applyWindowSettings() {
  auto configuration = context().root().configuration();
  if (!configuration)
    return std::unexpected(std::move(configuration.error()));
  auto getBool = [&](String const& key) -> std::expected<bool, StarException> {
    auto value = (*configuration)->get(key);
    if (!value)
      return std::unexpected(std::move(value.error()));
    return value->toBool();
  };
  auto appController = context().applicationController();
  auto fullscreen = getBool("fullscreen");
  if (!fullscreen)
    return std::unexpected(std::move(fullscreen.error()));
  if (*fullscreen) {
    auto value = (*configuration)->get("fullscreenResolution");
    if (!value)
      return std::unexpected(std::move(value.error()));
    auto resolution = jsonToVec2U(*value);
    if (!resolution)
      return std::unexpected(std::move(resolution.error()));
    return appController->setFullscreenWindow(*resolution);
  }
  auto borderless = getBool("borderless");
  if (!borderless)
    return std::unexpected(std::move(borderless.error()));
  if (*borderless)
    return appController->setBorderlessWindow();
  auto maximized = getBool("maximized");
  if (!maximized)
    return std::unexpected(std::move(maximized.error()));
  if (*maximized) {
    appController->setMaximizedWindow();
  } else {
    auto value = (*configuration)->get("windowedResolution");
    if (!value)
      return std::unexpected(std::move(value.error()));
    auto resolution = jsonToVec2U(*value);
    if (!resolution)
      return std::unexpected(std::move(resolution.error()));
    appController->setNormalWindow(*resolution);
  }
  return {};
}

}

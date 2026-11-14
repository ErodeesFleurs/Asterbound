import std;
import star.xx_hash;
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
import star.format;
import star.version;
import star.ordered_map;
import star.either;
import star.random;
import star.weighted_pool;
import star.strong_typedef;
import star.ordered_set;
import star.byte_array;
import star.data_stream_devices;

typedef struct ZSTD_CCtx_s ZSTD_CCtx;
typedef struct ZSTD_DCtx_s ZSTD_DCtx;
typedef ZSTD_DCtx ZSTD_DStream;
typedef ZSTD_CCtx ZSTD_CStream;
import star.zstd_compression;
import star.net_compatibility;
import star.id_map;
import star.worker_pool;

import star.sector_array_2d;
import star.perlin;
import star.btree;
import star.lru_cache;
import star.btree_database;
import star.rpc_promise;
import star.image;
import star.lexical_cast;
import star.periodic_function;


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
import star.configuration;

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


import star.options_menu;
import star.widget_parsing;
import star.gui_reader;
import star.button_group;
import star.button_widget;
import star.image_widget;
import star.slider_bar;
import star.label_widget;



import star.keybindings_menu;




import star.base_script_pane;


import star.voice_settings_menu;






import star.bindings_menu;






import star.graphics_menu;
import star.animated_part_set;
import star.networked_animator;
import star.humanoid;

namespace Star {

OptionsMenu::OptionsMenu(GuiContext& guiContext, PaneManager& manager)
  : Pane(guiContext), m_sfxRange(0, 100), m_musicRange(0, 100), m_paneManager(std::ref(manager)) {}

std::expected<std::shared_ptr<OptionsMenu>, StarException> OptionsMenu::create(GuiContext& guiContext, PaneManager& manager, std::shared_ptr<UniverseClient> client) {
  auto menu = std::shared_ptr<OptionsMenu>(new OptionsMenu(guiContext, manager));
  if (auto result = menu->initializePane(); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = menu->init(std::move(client)); !result)
    return std::unexpected(std::move(result.error()));
  return menu;
}
void OptionsMenu::detachInterface() noexcept {
  m_paneManager.reset();
  if (m_graphicsMenu)
    m_graphicsMenu->detachInterface();
}


std::expected<void, StarException> OptionsMenu::init(std::shared_ptr<UniverseClient> client) {
  auto assets = context().root().assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  GuiReader reader(context());
  for (auto const& [callback, key, slider, label] :
       {std::tuple{std::string_view("instrumentSlider"), std::string_view("instrumentVol"),
                   &OptionsMenu::m_instrumentSlider, &OptionsMenu::m_instrumentLabel},
        std::tuple{std::string_view("sfxSlider"), std::string_view("sfxVol"), &OptionsMenu::m_sfxSlider,
                   &OptionsMenu::m_sfxLabel},
        std::tuple{std::string_view("musicSlider"), std::string_view("musicVol"), &OptionsMenu::m_musicSlider,
                   &OptionsMenu::m_musicLabel}}) {
    reader.registerCallback(callback, [this, key = String(key), slider, label](Widget&) {
      return updateVolume(key, *(this->*slider), *(this->*label));
    });
  }
  reader.registerCallback("acceptButton", [this](Widget&) -> std::expected<void, StarException> {
    auto configuration = context().root().configuration();
    if (!configuration)
      return std::unexpected(std::move(configuration.error()));
    for (auto const& key : ConfigKeys) {
      auto value = m_localChanges.get(key);
      if (!value)
        return std::unexpected(std::move(value.error()));
      if (auto result = (*configuration)->set(key, value->get()); !result)
        return std::unexpected(std::move(result.error()));
    }
    dismiss();
    return {};
  });
  for (auto const& [callback, key, button] :
       {std::tuple{std::string_view("tutorialMessagesCheckbox"), std::string_view("tutorialMessages"),
                   &OptionsMenu::m_tutorialMessagesButton},
        std::tuple{std::string_view("clientIPJoinableCheckbox"), std::string_view("clientIPJoinable"),
                   &OptionsMenu::m_clientIPJoinableButton},
        std::tuple{std::string_view("clientP2PJoinableCheckbox"), std::string_view("clientP2PJoinable"),
                   &OptionsMenu::m_clientP2PJoinableButton},
        std::tuple{std::string_view("allowAssetsMismatchCheckbox"), std::string_view("allowAssetsMismatch"),
                   &OptionsMenu::m_allowAssetsMismatchButton}}) {
    reader.registerCallback(callback,
                            [this, key = String(key), button](Widget&) { return updateBool(key, *(this->*button)); });
  }
  reader.registerCallback("headRotationCheckbox", [this](Widget&) { return updateHeadRotation(); });
  reader.registerCallback("backButton", [this](Widget&) -> std::expected<void, StarException> {
    dismiss();
    return {};
  });
  reader.registerCallback("showKeybindings", [this](Widget&) { return displayControls(); });
  reader.registerCallback("showVoiceSettings", [this](Widget&) { return displayVoiceSettings(); });
  reader.registerCallback("showVoicePlayers", [](Widget&) -> std::expected<void, StarException> { return {}; });
  reader.registerCallback("showModBindings", [this](Widget&) { return displayModBindings(); });
  reader.registerCallback("showGraphics", [this](Widget&) { return displayGraphics(); });
  auto config = (*assets)->json("/interface/optionsmenu/optionsmenu.config");
  if (!config)
    return std::unexpected(std::move(config.error()));
  auto layout = config->get("paneLayout");
  if (!layout)
    return std::unexpected(std::move(layout.error()));
  if (auto result = reader.construct(*layout, *this); !result)
    return std::unexpected(std::move(result.error()));
  for (auto const& [name, member] : {
      std::pair{std::string_view("instrumentSlider"), &OptionsMenu::m_instrumentSlider},
      std::pair{std::string_view("sfxSlider"), &OptionsMenu::m_sfxSlider},
      std::pair{std::string_view("musicSlider"), &OptionsMenu::m_musicSlider}}) {
    auto child = requireChild<SliderBarWidget>(name);
    if (!child)
      return std::unexpected(std::move(child.error()));
    this->*member = std::move(*child);
  }
  for (auto const& [name, member] : {
      std::pair{std::string_view("tutorialMessagesCheckbox"), &OptionsMenu::m_tutorialMessagesButton},
      std::pair{std::string_view("clientIPJoinableCheckbox"), &OptionsMenu::m_clientIPJoinableButton},
      std::pair{std::string_view("clientP2PJoinableCheckbox"), &OptionsMenu::m_clientP2PJoinableButton},
      std::pair{std::string_view("allowAssetsMismatchCheckbox"), &OptionsMenu::m_allowAssetsMismatchButton},
      std::pair{std::string_view("headRotationCheckbox"), &OptionsMenu::m_headRotationButton}}) {
    auto child = requireChild<ButtonWidget>(name);
    if (!child)
      return std::unexpected(std::move(child.error()));
    this->*member = std::move(*child);
  }
  for (auto const& [name, member] : {
      std::pair{std::string_view("instrumentValueLabel"), &OptionsMenu::m_instrumentLabel},
      std::pair{std::string_view("sfxValueLabel"), &OptionsMenu::m_sfxLabel},
      std::pair{std::string_view("musicValueLabel"), &OptionsMenu::m_musicLabel},
      std::pair{std::string_view("clientP2PJoinableLabel"), &OptionsMenu::m_p2pJoinableLabel}}) {
    auto child = requireChild<LabelWidget>(name);
    if (!child)
      return std::unexpected(std::move(child.error()));
    this->*member = std::move(*child);
  }
  auto sfxDeltaJson = (*assets)->json("/interface/optionsmenu/optionsmenu.config:sfxDelta");
  if (!sfxDeltaJson)
    return std::unexpected(std::move(sfxDeltaJson.error()));
  auto sfxDelta = sfxDeltaJson->toInt();
  if (!sfxDelta)
    return std::unexpected(std::move(sfxDelta.error()));
  if (auto result = m_instrumentSlider->setRange(m_sfxRange, *sfxDelta); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = m_sfxSlider->setRange(m_sfxRange, *sfxDelta); !result)
    return std::unexpected(std::move(result.error()));
  auto musicDeltaJson = (*assets)->json("/interface/optionsmenu/optionsmenu.config:musicDelta");
  if (!musicDeltaJson)
    return std::unexpected(std::move(musicDeltaJson.error()));
  auto musicDelta = musicDeltaJson->toInt();
  if (!musicDelta)
    return std::unexpected(std::move(musicDelta.error()));
  if (auto result = m_musicSlider->setRange(m_musicRange, *musicDelta); !result)
    return std::unexpected(std::move(result.error()));
  auto voicePath = config->getString("voiceSettingsPanePath", "/interface/asterbound/voicechat/voicechat.config");
  if (!voicePath)
    return std::unexpected(std::move(voicePath.error()));
  auto voiceConfig = (*assets)->json(*voicePath);
  if (!voiceConfig)
    return std::unexpected(std::move(voiceConfig.error()));
  auto voiceMenu = VoiceSettingsMenu::create(context(), *voiceConfig);
  if (!voiceMenu)
    return std::unexpected(std::move(voiceMenu.error()));
  m_voiceSettingsMenu = std::move(*voiceMenu);
  auto bindingsPath = config->getString("bindingsPanePath", "/interface/asterbound/bindings/bindings.config");
  if (!bindingsPath)
    return std::unexpected(std::move(bindingsPath.error()));
  auto bindingsConfig = (*assets)->json(*bindingsPath);
  if (!bindingsConfig)
    return std::unexpected(std::move(bindingsConfig.error()));
  auto bindingsMenu = BindingsMenu::create(context(), *bindingsConfig);
  if (!bindingsMenu)
    return std::unexpected(std::move(bindingsMenu.error()));
  m_modBindingsMenu = std::move(*bindingsMenu);
  auto keybindingsMenu = KeybindingsMenu::create(context());
  if (!keybindingsMenu)
    return std::unexpected(std::move(keybindingsMenu.error()));
  m_keybindingsMenu = std::move(*keybindingsMenu);
  auto graphicsMenu = GraphicsMenu::create(context(), m_paneManager->get(), std::move(client));
  if (!graphicsMenu)
    return std::unexpected(std::move(graphicsMenu.error()));
  m_graphicsMenu = std::move(*graphicsMenu);
  return initConfig();
}

std::expected<void, StarException> OptionsMenu::show() {
  if (auto result = initConfig(); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = syncGuiToConf(); !result)
    return std::unexpected(std::move(result.error()));
  return Pane::show();
}

std::expected<void, StarException> OptionsMenu::toggleFullscreen() {
  if (auto result = m_graphicsMenu->toggleFullscreen(); !result)
    return std::unexpected(std::move(result.error()));
  return syncGuiToConf();
}

StringList const OptionsMenu::ConfigKeys = {
  "instrumentVol",
  "sfxVol",
  "musicVol",
  "tutorialMessages",
  "clientIPJoinable",
  "clientP2PJoinable",
  "allowAssetsMismatch",
  "humanoidHeadRotation"
};

std::expected<void, StarException> OptionsMenu::initConfig() {
  auto configuration = context().root().configuration();
  if (!configuration)
    return std::unexpected(std::move(configuration.error()));
  for (auto const& key : ConfigKeys) {
    auto original = (*configuration)->get(key);
    if (!original)
      return std::unexpected(std::move(original.error()));
    m_origConfig[key] = std::move(*original);
    auto local = (*configuration)->get(key);
    if (!local)
      return std::unexpected(std::move(local.error()));
    m_localChanges[key] = std::move(*local);
  }
  return {};
}

std::expected<void, StarException> OptionsMenu::updateVolume(String const& key, SliderBarWidget& slider,
                                                             LabelWidget& label) {
  m_localChanges.set(key, slider.val());
  auto configuration = context().root().configuration();
  if (!configuration)
    return std::unexpected(std::move(configuration.error()));
  if (auto result = (*configuration)->set(key, slider.val()); !result)
    return std::unexpected(std::move(result.error()));
  return label.setText(Star::to_string(slider.val()));
}

std::expected<void, StarException> OptionsMenu::updateBool(String const& key, ButtonWidget& button) {
  m_localChanges.set(key, button.isChecked());
  auto configuration = context().root().configuration();
  if (!configuration)
    return std::unexpected(std::move(configuration.error()));
  return (*configuration)->set(key, button.isChecked());
}

std::expected<void, StarException> OptionsMenu::updateHeadRotation() {
  if (auto result = updateBool("humanoidHeadRotation", *m_headRotationButton); !result)
    return std::unexpected(std::move(result.error()));
  auto headRotation = context().root().humanoidHeadRotation();
  if (!headRotation)
    return std::unexpected(std::move(headRotation.error()));
  headRotation->get() = m_headRotationButton->isChecked();
  return {};
}

std::expected<void, StarException> OptionsMenu::syncGuiToConf() {
  for (auto const& [key, slider, label] : {
      std::tuple{std::string_view("instrumentVol"), std::ref(*m_instrumentSlider), std::ref(*m_instrumentLabel)},
      std::tuple{std::string_view("sfxVol"), std::ref(*m_sfxSlider), std::ref(*m_sfxLabel)},
      std::tuple{std::string_view("musicVol"), std::ref(*m_musicSlider), std::ref(*m_musicLabel)}}) {
    auto setting = m_localChanges.get(key);
    if (!setting)
      return std::unexpected(std::move(setting.error()));
    auto value = setting->get().toInt();
    if (!value)
      return std::unexpected(std::move(value.error()));
    if (auto result = slider.get().setVal(*value, false); !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = label.get().setText(Star::to_string(slider.get().val())); !result)
      return std::unexpected(std::move(result.error()));
  }
  for (auto const& [key, button] : {
      std::pair{std::string_view("tutorialMessages"), std::ref(*m_tutorialMessagesButton)},
      std::pair{std::string_view("clientIPJoinable"), std::ref(*m_clientIPJoinableButton)},
      std::pair{std::string_view("clientP2PJoinable"), std::ref(*m_clientP2PJoinableButton)},
      std::pair{std::string_view("allowAssetsMismatch"), std::ref(*m_allowAssetsMismatchButton)}}) {
    auto setting = m_localChanges.get(key);
    if (!setting)
      return std::unexpected(std::move(setting.error()));
    auto value = setting->get().toBool();
    if (!value)
      return std::unexpected(std::move(value.error()));
    button.get().setChecked(*value);
  }
  auto headRotationSetting = m_localChanges.get("humanoidHeadRotation");
  if (!headRotationSetting)
    return std::unexpected(std::move(headRotationSetting.error()));
  auto headRotation = headRotationSetting->get().optBool();
  if (!headRotation)
    return std::unexpected(std::move(headRotation.error()));
  m_headRotationButton->setChecked(headRotation->value_or(true));
  if (!context().applicationController()->p2pNetworkingService()) {
    m_p2pJoinableLabel->setColor(Color::DarkGray);
    m_clientP2PJoinableButton->setEnabled(false);
    m_clientP2PJoinableButton->setChecked(false);
  }
  return {};
}

std::expected<void, StarException> OptionsMenu::displayControls() {
  if (!m_paneManager)
    return std::unexpected(GuiException("Options menu is detached from its pane manager"));
  return m_paneManager->get().displayPane(PaneLayer::ModalWindow, m_keybindingsMenu);
}

std::expected<void, StarException> OptionsMenu::displayVoiceSettings() {
  if (!m_paneManager)
    return std::unexpected(GuiException("Options menu is detached from its pane manager"));
  return m_paneManager->get().displayPane(PaneLayer::ModalWindow, m_voiceSettingsMenu);
}

std::expected<void, StarException> OptionsMenu::displayModBindings() {
  if (!m_paneManager)
    return std::unexpected(GuiException("Options menu is detached from its pane manager"));
  return m_paneManager->get().displayPane(PaneLayer::ModalWindow, m_modBindingsMenu);
}

std::expected<void, StarException> OptionsMenu::displayGraphics() {
  if (!m_paneManager)
    return std::unexpected(GuiException("Options menu is detached from its pane manager"));
  return m_paneManager->get().displayPane(PaneLayer::ModalWindow, m_graphicsMenu);
}

}

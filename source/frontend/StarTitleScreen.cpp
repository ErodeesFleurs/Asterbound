import std;
import star.memory;
import star.format;
import star.output_proxy;
import star.json;
import star.xx_hash;
import star.json_extra;
import star.id_map;
import star.ordered_map;
import star.rect;
import star.either;
import star.vector;
import star.game_types;
import star.color;
import star.bi_map;
import star.random;
import star.weighted_pool;
import star.list;
import star.image_processing;
import star.hash;
import star.data_stream;
import star.string_view;
import star.thread;
import star.directives;
import star.asset_path;
import star.string;
import star.byte_array;
import star.encode;
import star.casting;
import star.input_event;
import star.font;
import star.text;
import star.listener;
import star.set;
import star.audio;
import star.map;
import star.lua;
import star.version;
import star.image;
import star.strong_typedef;
import star.interpolation;
import star.perlin;
import star.random_point;
import star.io_device;
import star.ref_ptr;
import star.logging;
import star.ordered_set;
import star.data_stream_devices;

typedef struct ZSTD_CCtx_s ZSTD_CCtx;
typedef struct ZSTD_DCtx_s ZSTD_DCtx;
typedef ZSTD_DCtx ZSTD_DStream;
typedef ZSTD_CCtx ZSTD_CStream;
import star.zstd_compression;
import star.net_compatibility;
import star.worker_pool;

import star.sector_array_2d;
import star.btree;
import star.lru_cache;
import star.btree_database;
import star.rpc_promise;
import star.a_star;
import star.periodic_function;

import star.lua_root;
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
import star.host_address;
import star.chat_types;
import star.warping;
import star.item_descriptor;
import star.quest_descriptor;
import star.ai_types;
import star.socket;
import star.tcp;
import star.tile_sector_array;
import star.world_layout;
import star.tile_modification;
import star.interaction_types;
import star.wiring;
import star.interactive_entity;
import star.tile_entity;
import star.inspectable_entity;
import star.plant;
import star.biome_placement;
import star.versioning_database;
import star.world_storage;
import star.player_types;
import star.system_world;
import star.damage_manager;
import star.net_packets;
import star.net_packet_socket;
import star.universe_connection;
import star.world_structure;
import star.chat_action;
import star.entity_rendering;
import star.world;
import star.lua_components;
import star.world_client_state;
import star.interpolation_tracker;
import star.weather;
import star.world_client;
import star.world_client_thread;
import star.universe;
// TODO: make this more thread safe
import star.universe_client;
import star.celestial_coordinate;
import star.sky_types;
import star.animation;
import star.particle;
import star.weather_types;
import star.force_regions;
import star.world_parameters;
import star.celestial_parameters;
import star.celestial_types;
import star.interface_cursor;
import star.ambient;
import star.title_screen;
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
import star.animated_part_set;
import star.networked_animator;
import star.humanoid;
import star.physics_entity;
import star.anchorable_entity;
import star.mobile_entity;
import star.actor_entity;
import star.tool_user_entity;
import star.lounging_entities;
import star.chatty_entity;
import star.emote_entity;
import star.portrait_entity;
import star.damage_bar_entity;
import star.nametag_entity;
import star.inventory_types;
import star.movement_controller;
import star.platformer_astar_types;
import star.actor_movement_controller;
import star.lua_actor_movement_component;
import star.lua_animation_component;
import star.radio_message_database;
import star.player;
import star.pane;
import star.game_timers;
import star.pane_manager;
import star.button_group;
import star.button_widget;
import star.list_widget;
import star.label_widget;

import star.uuid;


import star.char_selection;



import star.char_creation;
import star.text_box_widget;
import star.canvas_widget;
import star.widget_lua_bindings;

import star.configuration;

import star.registered_pane_manager;


import star.main_interface_types;


import star.options_menu;



import star.mods_menu;
import star.tile_damage;
import star.plant_database;
import star.parallax;
import star.collision_block;
import star.liquid_types;
import star.collision_generator;
import star.world_tiles;
import star.entity_rendering_types;
import star.sky_render_data;
import star.damage_types;
import star.world_geometry;
import star.status_types;
import star.damage;
import star.light_source;
import star.entity;
import star.cellular_light_array;
import star.cellular_lighting;
import star.world_render_data;
import star.environment_painter;
import star.sky_parameters;


import star.celestial_database;

import star.player_storage;
import star.sky;

namespace Star {

TitleScreen::TitleScreen(GuiContext& context, std::shared_ptr<PlayerStorage> playerStorage, std::shared_ptr<Mixer> mixer)
  : m_guiContext(context), m_titleState(TitleState::Quit), m_playerStorage(std::move(playerStorage)),
    m_skipMultiPlayerConnection(false), m_forceLegacy(false), m_mixer(std::move(mixer)),
    m_musicTrackManager(context.root()) {}

TitleScreen::~TitleScreen() {
  if (auto r = close(); !r)
    EmergencyLog::error("Error closing title screen: {}", Star::output_exception(ExceptionDiagnostic(r.error()), true));
}

std::expected<void, StarException> TitleScreen::close() {
  if (auto menu = m_optionsMenu.lock())
    menu->detachInterface();
  m_optionsMenu.reset();
  std::optional<StarException> error;
  if (m_scriptComponent) {
    if (auto r = recoverLuaScriptError(m_scriptComponent->uninit()); !r)
      error = std::move(r.error());
    m_scriptComponent.reset();
  }
  if (m_celestialDatabase) {
    if (auto r = m_celestialDatabase->close(); !r) {
      if (error)
        error = StarException(Star::format("Title celestial cleanup failed: {}",
            Star::print_exception(ExceptionDiagnostic(r.error()), true)), ExceptionDiagnostic(*error));
      else
        error = std::move(r.error());
    }
    m_celestialDatabase.reset();
  }
  if (error)
    return std::unexpected(std::move(*error));
  return {};
}

std::expected<std::shared_ptr<TitleScreen>, StarException> TitleScreen::create(GuiContext& context,
    std::shared_ptr<PlayerStorage> playerStorage, std::shared_ptr<Mixer> mixer, std::shared_ptr<UniverseClient> client) {
  auto screen = std::shared_ptr<TitleScreen>(new TitleScreen(context, std::move(playerStorage), std::move(mixer)));
  auto initialized = screen->initialize(std::move(client));
  if (!initialized) {
    auto closed = screen->close();
    if (!closed)
      return std::unexpected(StarException(Star::format("Title initialization cleanup failed: {}",
          Star::print_exception(ExceptionDiagnostic(closed.error()), true)), ExceptionDiagnostic(initialized.error())));
    return std::unexpected(std::move(initialized.error()));
  }
  return screen;
}

std::expected<void, StarException> TitleScreen::initialize(std::shared_ptr<UniverseClient> client) {
  auto manager = TitlePaneManager::create(m_guiContext);
  if (!manager)
    return std::unexpected(std::move(manager.error()));
  m_paneManager = std::move(*manager);
  auto cursor = InterfaceCursor::create(m_guiContext);
  if (!cursor)
    return std::unexpected(std::move(cursor.error()));
  m_cursor = std::move(*cursor);
  auto assets = m_guiContext.root().assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  auto database = CelestialMasterDatabase::create(m_guiContext.root());
  if (!database)
    return std::unexpected(std::move(database.error()));
  m_celestialDatabase = std::move(*database);
  auto randomWorld = m_celestialDatabase->findRandomWorld(10, 50,
      [this](CelestialCoordinate const& coordinate) -> std::expected<bool, StarException> {
        auto parameters = m_celestialDatabase->parameters(coordinate);
        if (!parameters)
          return std::unexpected(std::move(parameters.error()));
        if (!*parameters)
          return std::unexpected(StarException("Title world has no celestial parameters"));
        return is<TerrestrialWorldParameters>((**parameters).visitableParameters());
      });
  if (!randomWorld)
    return std::unexpected(std::move(randomWorld.error()));
  if (!*randomWorld)
    return std::unexpected(StarException("Could not find a terrestrial world for the title screen"));
  auto name = m_celestialDatabase->name(**randomWorld);
  if (!name)
    return std::unexpected(std::move(name.error()));
  if (*name) {
    if (auto r = m_guiContext.root().logger().info("Title world is {} @ CelestialWorld:{}", Text::stripEscapeCodes(**name), **randomWorld); !r)
      return std::unexpected(std::move(r.error()));
  }
  auto skyParameters = SkyParameters::create(m_guiContext.root(), **randomWorld, m_celestialDatabase);
  if (!skyParameters)
    return std::unexpected(std::move(skyParameters.error()));
  auto sky = Sky::create(m_guiContext.root(), *skyParameters, true);
  if (!sky)
    return std::unexpected(std::move(sky.error()));
  m_skyBackdrop = std::move(*sky);
  auto musicConfig = (*assets)->json("/interface/windowconfig/title.config:music");
  if (!musicConfig)
    return std::unexpected(std::move(musicConfig.error()));
  auto musicObject = musicConfig->toObject();
  if (!musicObject)
    return std::unexpected(std::move(musicObject.error()));
  auto music = AmbientNoisesDescription::create(*musicObject, "/");
  if (!music)
    return std::unexpected(std::move(music.error()));
  m_musicTrack = std::make_shared<AmbientNoisesDescription>(std::move(*music));
  if (auto r = initMainMenu(); !r) return std::unexpected(std::move(r.error()));
  if (auto r = initCharSelectionMenu(); !r) return std::unexpected(std::move(r.error()));
  if (auto r = initCharCreationMenu(); !r) return std::unexpected(std::move(r.error()));
  if (auto r = initMultiPlayerMenu(); !r) return std::unexpected(std::move(r.error()));
  if (auto r = initOptionsMenu(std::move(client)); !r) return std::unexpected(std::move(r.error()));
  if (auto r = initModsMenu(); !r) return std::unexpected(std::move(r.error()));
  return resetState();
}

std::expected<void, StarException> TitleScreen::renderInit(std::shared_ptr<Renderer> renderer) {
  auto painter = EnvironmentPainter::create(m_guiContext.root(), renderer);
  if (!painter)
    return std::unexpected(std::move(painter.error()));
  m_environmentPainter = std::move(*painter);
  m_renderer = std::move(renderer);
  return {};
}

std::expected<void, StarException> TitleScreen::render(float interpolation) {
  auto assets = m_guiContext.root().assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  float pixelRatio = m_guiContext.interfaceScale();
  auto windowSize = m_guiContext.windowSize();
  if (!windowSize)
    return std::unexpected(std::move(windowSize.error()));
  Vec2F screenSize(*windowSize);
  auto skyRenderData = m_skyBackdrop->renderData(interpolation);
  if (!skyRenderData)
    return std::unexpected(std::move(skyRenderData.error()));
  float pixelRatioBasis = screenSize[1] / 1080.0f;
  float starAndDebrisRatio = std::lerp(pixelRatioBasis * 2.0f, pixelRatio, 0.0625f);
  float orbiterAndPlanetRatio = std::lerp(pixelRatioBasis * 3.0f, pixelRatio, 0.125f);
  if (auto r = m_environmentPainter->renderStars(starAndDebrisRatio, screenSize, *skyRenderData); !r) return std::unexpected(std::move(r.error()));
  if (auto r = m_environmentPainter->renderDebrisFields(starAndDebrisRatio, screenSize, *skyRenderData); !r) return std::unexpected(std::move(r.error()));
  if (auto r = m_environmentPainter->renderBackOrbiters(orbiterAndPlanetRatio, screenSize, *skyRenderData); !r) return std::unexpected(std::move(r.error()));
  if (auto r = m_environmentPainter->renderPlanetHorizon(orbiterAndPlanetRatio, screenSize, *skyRenderData); !r) return std::unexpected(std::move(r.error()));
  if (auto r = m_environmentPainter->renderSky(screenSize, *skyRenderData); !r) return std::unexpected(std::move(r.error()));
  if (auto r = m_environmentPainter->renderFrontOrbiters(orbiterAndPlanetRatio, screenSize, *skyRenderData); !r) return std::unexpected(std::move(r.error()));
  if (auto r = m_renderer->flush(); !r) return std::unexpected(std::move(r.error()));
  auto darkenConfig = (*assets)->json("/interface/windowconfig/title.config:skyBackdropDarken");
  if (!darkenConfig)
    return std::unexpected(std::move(darkenConfig.error()));
  auto darken = jsonToColor(*darkenConfig);
  if (!darken)
    return std::unexpected(std::move(darken.error()));
  m_renderer->render(renderFlatRect(RectF(0, 0, screenSize[0], screenSize[1]), darken->toRgba(), 0.0f));
  if (auto r = m_renderer->flush(); !r) return std::unexpected(std::move(r.error()));
  auto interfaceSize = m_guiContext.windowInterfaceSize();
  if (!interfaceSize)
    return std::unexpected(std::move(interfaceSize.error()));
  auto canvas = m_backgroundMenu->findChild("canvas");
  if (!canvas) return std::unexpected(std::move(canvas.error()));
  if (*canvas) {
    (*canvas)->setPosition(Vec2I());
    (*canvas)->setSize(Vec2I(*interfaceSize));
  }
  if (auto r = recoverLuaScriptError(m_scriptComponent->invoke("render", JsonObject{{"interfaceScale", interfaceScale()}})); !r)
    return std::unexpected(std::move(r.error()));
  if (auto r = m_renderer->flush(); !r) return std::unexpected(std::move(r.error()));
  if (auto r = m_backgroundMenu->render(RectI(Vec2I(), Vec2I(*interfaceSize))); !r) return std::unexpected(std::move(r.error()));
  if (auto r = m_paneManager->render(); !r) return std::unexpected(std::move(r.error()));
  if (auto r = renderCursor(); !r) return std::unexpected(std::move(r.error()));
  return m_renderer->flush();
}

std::expected<bool, StarException> TitleScreen::handleInputEvent(InputEvent const& event) {
  if (std::holds_alternative<MouseMoveEvent>(event))
    m_cursorScreenPos = Vec2I(std::get<MouseMoveEvent>(event).mousePosition);
  if (std::holds_alternative<KeyDownEvent>(event) && m_guiContext.actions(event).contains(InterfaceAction::TitleBack)) {
    if (auto r = back(); !r) return std::unexpected(std::move(r.error()));
    return true;
  }
  return m_paneManager->sendInputEvent(event);
}

std::expected<void, StarException> TitleScreen::update(float dt) {
  if (auto r = m_cursor->update(dt); !r) return std::unexpected(std::move(r.error()));
  auto width = m_guiContext.windowWidth();
  if (!width)
    return std::unexpected(std::move(width.error()));
  for (auto const& p : m_rightAnchoredButtons)
    p.first->setPosition(Vec2I(*width / m_guiContext.interfaceScale(), 0) + p.second);
  m_mainMenu->determineSizeFromChildren();
  m_backgroundMenu->determineSizeFromChildren();
  m_skyBackdrop->beginUpdate();
  if (auto r = m_skyBackdrop->update(dt); !r) return std::unexpected(std::move(r.error()));
  m_environmentPainter->update(dt);
  if (auto r = m_backgroundMenu->update(dt); !r) return std::unexpected(std::move(r.error()));
  if (auto r = m_paneManager->update(dt); !r) return std::unexpected(std::move(r.error()));
  if (auto r = recoverLuaScriptError(m_scriptComponent->update(dt)); !r) return std::unexpected(std::move(r.error()));
  if (!finishedState()) {
    auto audioSample = m_musicTrackManager.updateAmbient(m_musicTrack, m_skyBackdrop->isDayTime());
    if (!audioSample)
      return std::unexpected(std::move(audioSample.error()));
    if (*audioSample) {
      m_currentMusicTrack = *audioSample;
      (*audioSample)->setMixerGroup(MixerGroup::Music);
      (*audioSample)->setLoops(0);
      if (auto r = m_mixer->play(*audioSample); !r) return std::unexpected(std::move(r.error()));
    }
  }
  return {};
}

std::expected<bool, StarException> TitleScreen::textInputActive() const {
  return m_paneManager->keyboardCapturedForTextInput();
}

TitleScreen::TitlePaneManager& TitleScreen::paneManager() {
  return *m_paneManager;
}

TitleState TitleScreen::currentState() const {
  return m_titleState;
}

bool TitleScreen::finishedState() const {
  switch (m_titleState) {
    case TitleState::StartSinglePlayer:
    case TitleState::StartMultiPlayer:
    case TitleState::Quit:
      return true;
    default:
      return false;
  }
}

std::expected<void, StarException> TitleScreen::resetState() {
  if (auto r = switchState(TitleState::Main); !r) return std::unexpected(std::move(r.error()));
  if (m_currentMusicTrack)
    m_currentMusicTrack->setVolume(1.0f, 4.0f);
  return {};
}

std::expected<void, StarException> TitleScreen::goToMultiPlayerSelectCharacter(bool skipConnection) {
  m_skipMultiPlayerConnection = skipConnection;
  return switchState(TitleState::MultiPlayerSelectCharacter);
}

void TitleScreen::stopMusic() {
  if (m_currentMusicTrack)
    m_currentMusicTrack->stop(8.0f);
}

std::shared_ptr<Player> TitleScreen::currentlySelectedPlayer() const {
  return m_mainAppPlayer;
}

String TitleScreen::multiPlayerAddress() const {
  return m_connectionAddress;
}

std::expected<void, StarException> TitleScreen::setMultiPlayerAddress(String address) {
  auto widget = m_multiPlayerMenu->fetchChild<TextBoxWidget>("address");
  if (!widget) return std::unexpected(std::move(widget.error()));
  auto r = (*widget)->setText(address);
  if (!r) return std::unexpected(std::move(r.error()));
  m_connectionAddress = std::move(address);
  return {};
}

String TitleScreen::multiPlayerPort() const {
  return m_connectionPort;
}

std::expected<void, StarException> TitleScreen::setMultiPlayerPort(String port) {
  auto widget = m_multiPlayerMenu->fetchChild<TextBoxWidget>("port");
  if (!widget) return std::unexpected(std::move(widget.error()));
  auto r = (*widget)->setText(port);
  if (!r) return std::unexpected(std::move(r.error()));
  m_connectionPort = std::move(port);
  return {};
}

String TitleScreen::multiPlayerAccount() const {
  return m_account;
}

std::expected<void, StarException> TitleScreen::setMultiPlayerAccount(String account) {
  auto widget = m_multiPlayerMenu->fetchChild<TextBoxWidget>("account");
  if (!widget) return std::unexpected(std::move(widget.error()));
  auto r = (*widget)->setText(account);
  if (!r) return std::unexpected(std::move(r.error()));
  m_account = std::move(account);
  return {};
}

String TitleScreen::multiPlayerPassword() const {
  return m_password;
}

std::expected<void, StarException> TitleScreen::setMultiPlayerPassword(String password) {
  auto widget = m_multiPlayerMenu->fetchChild<TextBoxWidget>("password");
  if (!widget) return std::unexpected(std::move(widget.error()));
  auto r = (*widget)->setText(password);
  if (!r) return std::unexpected(std::move(r.error()));
  m_password = std::move(password);
  return {};
}

bool TitleScreen::multiPlayerForceLegacy() const {
  return m_forceLegacy;
}

std::expected<void, StarException> TitleScreen::setMultiPlayerForceLegacy(bool const& forceLegacy) {
  auto widget = m_multiPlayerMenu->fetchChild<ButtonWidget>("legacyCheckbox");
  if (!widget) return std::unexpected(std::move(widget.error()));
  (*widget)->setChecked(forceLegacy);
  m_forceLegacy = forceLegacy;
  return {};
}

std::expected<void, StarException> TitleScreen::initMainMenu() {
  auto mainMenu = Pane::create(m_guiContext);
  if (!mainMenu) return std::unexpected(std::move(mainMenu.error()));
  m_mainMenu = std::move(*mainMenu);
  auto backMenu = Pane::create(m_guiContext);
  if (!backMenu) return std::unexpected(std::move(backMenu.error()));
  auto assets = m_guiContext.root().assets();
  if (!assets) return std::unexpected(std::move(assets.error()));
  auto config = (*assets)->json("/interface/windowconfig/title.config");
  if (!config) return std::unexpected(std::move(config.error()));
  StringMap<WidgetCallbackFunc> buttonCallbacks;
  buttonCallbacks["singleplayer"] = [this](Widget&) { return switchState(TitleState::SinglePlayerSelectCharacter); };
  buttonCallbacks["multiplayer"] = [this](Widget&) { return switchState(TitleState::MultiPlayerSelectCharacter); };
  buttonCallbacks["options"] = [this](Widget&) { return switchState(TitleState::Options); };
  buttonCallbacks["quit"] = [this](Widget&) { return switchState(TitleState::Quit); };
  buttonCallbacks["back"] = [this](Widget&) { return back(); };
  buttonCallbacks["mods"] = [this](Widget&) { return switchState(TitleState::Mods); };
  auto buttons = config->getArray("mainMenuButtons");
  if (!buttons) return std::unexpected(std::move(buttons.error()));
  for (auto const& buttonConfig : *buttons) {
    auto key = buttonConfig.getString("key");
    if (!key) return std::unexpected(std::move(key.error()));
    auto image = buttonConfig.getString("button");
    if (!image) return std::unexpected(std::move(image.error()));
    auto hover = buttonConfig.getString("hover");
    if (!hover) return std::unexpected(std::move(hover.error()));
    auto offsetConfig = buttonConfig.get("offset");
    if (!offsetConfig) return std::unexpected(std::move(offsetConfig.error()));
    auto offset = jsonToVec2I(*offsetConfig);
    if (!offset) return std::unexpected(std::move(offset.error()));
    auto callback = buttonCallbacks.get(*key);
    if (!callback) return std::unexpected(std::move(callback.error()));
    auto rightAnchored = buttonConfig.getBool("rightAnchored", false);
    if (!rightAnchored) return std::unexpected(std::move(rightAnchored.error()));
    auto button = ButtonWidget::create(m_guiContext, callback->get(), *image, *hover, "", "");
    if (!button) return std::unexpected(std::move(button.error()));
    (*button)->setPosition(*offset);
    if (*rightAnchored)
      m_rightAnchoredButtons.push_back({*button, *offset});
    if (auto r = (*key == "back" ? *backMenu : m_mainMenu)->addChild(*key, *button); !r)
      return std::unexpected(std::move(r.error()));
  }
  m_mainMenu->setAnchor(PaneAnchor::BottomLeft);
  m_mainMenu->lockPosition();
  (*backMenu)->determineSizeFromChildren();
  (*backMenu)->setAnchor(PaneAnchor::BottomLeft);
  (*backMenu)->lockPosition();
  auto background = Pane::create(m_guiContext);
  if (!background) return std::unexpected(std::move(background.error()));
  m_backgroundMenu = std::move(*background);
  m_backgroundMenu->setAnchor(PaneAnchor::BottomLeft);
  m_backgroundMenu->lockPosition();
  auto canvas = CanvasWidget::create(m_guiContext);
  if (!canvas) return std::unexpected(std::move(canvas.error()));
  if (auto r = m_backgroundMenu->addChild("canvas", *canvas); !r) return std::unexpected(std::move(r.error()));
  if (auto r = m_backgroundMenu->show(); !r) return std::unexpected(std::move(r.error()));
  if (auto r = m_paneManager->registerPane("mainMenu", PaneLayer::Hud, m_mainMenu); !r) return std::unexpected(std::move(r.error()));
  if (auto r = m_paneManager->registerPane("backMenu", PaneLayer::Hud, *backMenu); !r) return std::unexpected(std::move(r.error()));
  m_scriptComponent = std::make_shared<ScriptComponent>(m_guiContext.root());
  auto luaRoot = LuaRoot::create(m_guiContext.root());
  if (!luaRoot) return std::unexpected(std::move(luaRoot.error()));
  if (auto r = m_scriptComponent->setLuaRoot(*luaRoot); !r) return std::unexpected(std::move(r.error()));
  auto backgroundCallbacks = LuaBindings::makeWidgetCallbacks(m_guiContext, *m_backgroundMenu);
  if (!backgroundCallbacks) return std::unexpected(std::move(backgroundCallbacks.error()));
  if (auto r = m_scriptComponent->addCallbacks("background", std::move(*backgroundCallbacks)); !r) return std::unexpected(std::move(r.error()));
  auto widgetCallbacks = LuaBindings::makeWidgetCallbacks(m_guiContext, *m_mainMenu);
  if (!widgetCallbacks) return std::unexpected(std::move(widgetCallbacks.error()));
  if (auto r = m_scriptComponent->addCallbacks("widget", std::move(*widgetCallbacks)); !r) return std::unexpected(std::move(r.error()));
  auto scriptConfig = config->getArray("scripts", JsonArray());
  if (!scriptConfig) return std::unexpected(std::move(scriptConfig.error()));
  auto scripts = jsonToStringList(*scriptConfig);
  if (!scripts) return std::unexpected(std::move(scripts.error()));
  if (auto r = m_scriptComponent->setScripts(std::move(*scripts)); !r) return std::unexpected(std::move(r.error()));
  auto initialized = recoverLuaScriptError(m_scriptComponent->init());
  if (!initialized) return std::unexpected(std::move(initialized.error()));
  return {};
}

std::expected<void, StarException> TitleScreen::initCharSelectionMenu() {
  auto deleteDialog = Pane::create(m_guiContext);
  if (!deleteDialog) return std::unexpected(std::move(deleteDialog.error()));
  GuiReader reader(m_guiContext);
  reader.registerCallback("delete", [dialog = *deleteDialog](Widget&) -> std::expected<void, StarException> { dialog->dismiss(); return {}; });
  reader.registerCallback("cancel", [dialog = *deleteDialog](Widget&) -> std::expected<void, StarException> { dialog->dismiss(); return {}; });
  auto assets = m_guiContext.root().assets();
  if (!assets) return std::unexpected(std::move(assets.error()));
  auto config = (*assets)->json("/interface/windowconfig/deletedialog.config");
  if (!config) return std::unexpected(std::move(config.error()));
  if (auto r = reader.construct(*config, **deleteDialog); !r) return std::unexpected(std::move(r.error()));
  auto charSelectionMenu = CharSelectionPane::create(m_guiContext, m_playerStorage,
      [this]() -> std::expected<void, StarException> {
        if (m_titleState == TitleState::SinglePlayerSelectCharacter)
          return switchState(TitleState::SinglePlayerCreateCharacter);
        if (m_titleState == TitleState::MultiPlayerSelectCharacter)
          return switchState(TitleState::MultiPlayerCreateCharacter);
        return {};
      }, [this](std::shared_ptr<Player> mainPlayer) -> std::expected<void, StarException> {
        m_mainAppPlayer = std::move(mainPlayer);
        auto uuid = m_mainAppPlayer->uuid();
        if (!uuid) return std::unexpected(std::move(uuid.error()));
        if (auto r = m_playerStorage->moveToFront(*uuid); !r) return std::unexpected(std::move(r.error()));
        if (m_titleState == TitleState::SinglePlayerSelectCharacter)
          return switchState(TitleState::StartSinglePlayer);
        if (m_titleState == TitleState::MultiPlayerSelectCharacter)
          return switchState(m_skipMultiPlayerConnection ? TitleState::StartMultiPlayer : TitleState::MultiPlayerConnect);
        return {};
      }, [this](Uuid playerUuid) -> std::expected<void, StarException> {
        auto dialog = m_paneManager->registeredPane("deleteDialog");
        if (!dialog) return std::unexpected(std::move(dialog.error()));
        auto button = (*dialog)->fetchChild<ButtonWidget>("delete");
        if (!button) return std::unexpected(std::move(button.error()));
        (*button)->setCallback([this, playerUuid, dialog = *dialog](Widget&) -> std::expected<void, StarException> {
          if (auto r = m_playerStorage->deletePlayer(playerUuid); !r) return std::unexpected(std::move(r.error()));
          dialog->dismiss();
          return {};
        });
        auto r = m_paneManager->displayRegisteredPane("deleteDialog");
        if (!r) return std::unexpected(std::move(r.error()));
        return {};
      });
  if (!charSelectionMenu) return std::unexpected(std::move(charSelectionMenu.error()));
  (*charSelectionMenu)->setAnchor(PaneAnchor::Center);
  (*charSelectionMenu)->lockPosition();
  if (auto r = m_paneManager->registerPane("deleteDialog", PaneLayer::ModalWindow, *deleteDialog,
      [menu = *charSelectionMenu](std::shared_ptr<Pane> const&) { return menu->updateCharacterPlates(); }); !r)
    return std::unexpected(std::move(r.error()));
  return m_paneManager->registerPane("charSelectionMenu", PaneLayer::Hud, *charSelectionMenu);
}

std::expected<void, StarException> TitleScreen::initCharCreationMenu() {
  auto menu = CharCreationPane::create(m_guiContext, [this](std::shared_ptr<Player> newPlayer) -> std::expected<void, StarException> {
    if (newPlayer) {
      m_mainAppPlayer = std::move(newPlayer);
      if (auto r = m_playerStorage->savePlayer(m_mainAppPlayer); !r) return std::unexpected(std::move(r.error()));
      auto uuid = m_mainAppPlayer->uuid();
      if (!uuid) return std::unexpected(std::move(uuid.error()));
      if (auto r = m_playerStorage->moveToFront(*uuid); !r) return std::unexpected(std::move(r.error()));
    }
    return back();
  });
  if (!menu) return std::unexpected(std::move(menu.error()));
  (*menu)->setAnchor(PaneAnchor::Center);
  (*menu)->lockPosition();
  return m_paneManager->registerPane("charCreationMenu", PaneLayer::Hud, *menu);
}


std::expected<void, StarException> TitleScreen::populateServerList(ListWidget& list) {
  if (!m_serverList.isNull()) {
    if (auto r = list.clear(); !r) return std::unexpected(std::move(r.error()));
    auto servers = m_serverList.iterateArray();
    if (!servers) return std::unexpected(std::move(servers.error()));
    for (auto const& server : *servers) {
      auto item = list.addItem();
      if (!item) return std::unexpected(std::move(item.error()));
      auto addressLabel = (*item)->fetchChild<LabelWidget>("address");
      if (!addressLabel) return std::unexpected(std::move(addressLabel.error()));
      auto address = server.getString("address");
      if (!address) return std::unexpected(std::move(address.error()));
      if (auto r = (*addressLabel)->setText(*address); !r) return std::unexpected(std::move(r.error()));
      auto accountLabel = (*item)->fetchChild<LabelWidget>("account");
      if (!accountLabel) return std::unexpected(std::move(accountLabel.error()));
      auto accountConfig = server.get("account", "");
      if (!accountConfig) return std::unexpected(std::move(accountConfig.error()));
      auto account = accountConfig->toString();
      if (!account) return std::unexpected(std::move(account.error()));
      if (auto r = (*accountLabel)->setText(*account); !r) return std::unexpected(std::move(r.error()));
      (*item)->setData(server);
    }
  }
  return {};
}

std::expected<void, StarException> TitleScreen::initMultiPlayerMenu() {
  auto multiplayer = Pane::create(m_guiContext);
  if (!multiplayer) return std::unexpected(std::move(multiplayer.error()));
  m_multiPlayerMenu = std::move(*multiplayer);
  auto serverSelect = Pane::create(m_guiContext);
  if (!serverSelect) return std::unexpected(std::move(serverSelect.error()));
  m_serverSelectPane = std::move(*serverSelect);
  GuiReader readerConnect(m_guiContext);
  GuiReader readerServer(m_guiContext);
  auto configuration = m_guiContext.root().configuration();
  if (!configuration) return std::unexpected(std::move(configuration.error()));
  auto servers = (*configuration)->get("serverList");
  if (!servers) return std::unexpected(std::move(servers.error()));
  m_serverList = std::move(*servers);
  if (!m_serverList.isType(Json::Type::Array))
    m_serverList = JsonArray();
  auto assets = m_guiContext.root().assets();
  if (!assets) return std::unexpected(std::move(assets.error()));
  readerServer.registerCallback("saveServer", [this](Widget&) -> std::expected<void, StarException> {
    Json serverData = JsonObject{
      {"address", multiPlayerAddress()}, {"account", multiPlayerAccount()},
      {"port", multiPlayerPort()}, {"forceLegacy", multiPlayerForceLegacy()}
    };
    auto list = m_serverSelectPane->fetchChild<ListWidget>("serverSelectArea.serverList");
    if (!list) return std::unexpected(std::move(list.error()));
    auto pos = (*list)->selectedItem();
    auto updated = pos != NPos ? m_serverList.set(pos, serverData) : m_serverList.insert(0, serverData);
    if (!updated) return std::unexpected(std::move(updated.error()));
    m_serverList = std::move(*updated);
    if (auto r = populateServerList(**list); !r) return std::unexpected(std::move(r.error()));
    auto configuration = m_guiContext.root().configuration();
    if (!configuration) return std::unexpected(std::move(configuration.error()));
    return (*configuration)->set("serverList", m_serverList);
  });
  auto serverConfig = (*assets)->json("/interface/windowconfig/serverselect.config");
  if (!serverConfig) return std::unexpected(std::move(serverConfig.error()));
  if (auto r = readerServer.construct(*serverConfig, *m_serverSelectPane); !r) return std::unexpected(std::move(r.error()));
  auto serverList = m_serverSelectPane->fetchChild<ListWidget>("serverSelectArea.serverList");
  if (!serverList) return std::unexpected(std::move(serverList.error()));
  (*serverList)->registerMemberCallback("delete", [this, list = *serverList](Widget&) -> std::expected<void, StarException> {
    if (auto pos = list->selectedItem(); pos != NPos) {
      auto updated = m_serverList.eraseIndex(pos);
      if (!updated) return std::unexpected(std::move(updated.error()));
      m_serverList = std::move(*updated);
    }
    if (auto r = populateServerList(*list); !r) return std::unexpected(std::move(r.error()));
    auto configuration = m_guiContext.root().configuration();
    if (!configuration) return std::unexpected(std::move(configuration.error()));
    return (*configuration)->set("serverList", m_serverList);
  });
  (*serverList)->setCallback([this, list = *serverList](Widget&) -> std::expected<void, StarException> {
    if (auto selected = list->selectedWidget()) {
      auto deleteButton = selected->findChild<ButtonWidget>("delete");
      if (!deleteButton) return std::unexpected(std::move(deleteButton.error()));
      if (*deleteButton && (*deleteButton)->isHovered())
        return {};
      auto const& data = selected->data();
      auto address = data.getString("address", "");
      if (!address) return std::unexpected(std::move(address.error()));
      if (auto r = setMultiPlayerAddress(std::move(*address)); !r) return std::unexpected(std::move(r.error()));
      auto port = data.getString("port", "");
      if (!port) return std::unexpected(std::move(port.error()));
      if (auto r = setMultiPlayerPort(std::move(*port)); !r) return std::unexpected(std::move(r.error()));
      auto account = data.getString("account", "");
      if (!account) return std::unexpected(std::move(account.error()));
      if (auto r = setMultiPlayerAccount(std::move(*account)); !r) return std::unexpected(std::move(r.error()));
      auto password = data.getString("password", "");
      if (!password) return std::unexpected(std::move(password.error()));
      if (auto r = setMultiPlayerPassword(std::move(*password)); !r) return std::unexpected(std::move(r.error()));
      auto legacy = data.getBool("forceLegacy", false);
      if (!legacy) return std::unexpected(std::move(legacy.error()));
      if (auto r = setMultiPlayerForceLegacy(*legacy); !r) return std::unexpected(std::move(r.error()));
      auto passwordWidget = m_multiPlayerMenu->fetchChild("password");
      if (!passwordWidget) return std::unexpected(std::move(passwordWidget.error()));
      if (*passwordWidget)
        return (*passwordWidget)->focus();
    }
    return {};
  });
  readerConnect.registerCallback("address", [this](Widget& obj) -> std::expected<void, StarException> {
    auto widget = convert_ref<TextBoxWidget>(obj);
    if (!widget)
      return std::unexpected(std::move(widget.error()));
    auto text = widget->get().getText().trim();
    if (!text)
      return std::unexpected(std::move(text.error()));
    m_connectionAddress = std::move(*text);
    auto save = m_serverSelectPane->fetchChild<ButtonWidget>("save");
    if (!save)
      return std::unexpected(std::move(save.error()));
    auto length = m_connectionAddress.length();
    if (!length)
      return std::unexpected(std::move(length.error()));
    return (*save)->setVisibility(*length > 0);
  });
  readerConnect.registerCallback("port", [this](Widget& obj) -> std::expected<void, StarException> {
    auto widget = convert_ref<TextBoxWidget>(obj);
    if (!widget)
      return std::unexpected(std::move(widget.error()));
    auto text = widget->get().getText().trim();
    if (!text)
      return std::unexpected(std::move(text.error()));
    m_connectionPort = std::move(*text);
    return {};
  });
  readerConnect.registerCallback("account", [this](Widget& obj) -> std::expected<void, StarException> {
    auto widget = convert_ref<TextBoxWidget>(obj);
    if (!widget)
      return std::unexpected(std::move(widget.error()));
    auto text = widget->get().getText().trim();
    if (!text)
      return std::unexpected(std::move(text.error()));
    m_account = std::move(*text);
    return {};
  });
  readerConnect.registerCallback("password", [this](Widget& obj) -> std::expected<void, StarException> {
    auto widget = convert_ref<TextBoxWidget>(obj);
    if (!widget)
      return std::unexpected(std::move(widget.error()));
    auto text = widget->get().getText().trim();
    if (!text)
      return std::unexpected(std::move(text.error()));
    m_password = std::move(*text);
    return {};
  });
  readerConnect.registerCallback("legacyCheckbox", [this](Widget& obj) -> std::expected<void, StarException> {
    auto widget = convert_ref<ButtonWidget>(obj);
    if (!widget)
      return std::unexpected(std::move(widget.error()));
    m_forceLegacy = widget->get().isChecked();
    return {};
  });
  readerConnect.registerCallback("connect", [this](Widget&) { return switchState(TitleState::StartMultiPlayer); });
  auto connectConfig = (*assets)->json("/interface/windowconfig/multiplayer.config");
  if (!connectConfig) return std::unexpected(std::move(connectConfig.error()));
  if (auto r = readerConnect.construct(*connectConfig, *m_multiPlayerMenu); !r) return std::unexpected(std::move(r.error()));
  if (auto r = populateServerList(**serverList); !r) return std::unexpected(std::move(r.error()));
  if (auto r = m_paneManager->registerPane("multiplayerMenu", PaneLayer::Hud, m_multiPlayerMenu); !r) return std::unexpected(std::move(r.error()));
  return m_paneManager->registerPane("serverSelect", PaneLayer::Hud, m_serverSelectPane,
      [list = *serverList](std::shared_ptr<Pane> const&) { return list->clearSelected(); });
}

std::expected<void, StarException> TitleScreen::initOptionsMenu(std::shared_ptr<UniverseClient> client) {
  auto menu = OptionsMenu::create(m_guiContext, *m_paneManager, std::move(client));
  if (!menu) return std::unexpected(std::move(menu.error()));
  m_optionsMenu = *menu;
  (*menu)->setAnchor(PaneAnchor::Center);
  (*menu)->lockPosition();
  return m_paneManager->registerPane("optionsMenu", PaneLayer::Hud, *menu,
      [this](std::shared_ptr<Pane> const&) { return back(); });
}

std::expected<void, StarException> TitleScreen::initModsMenu() {
  auto menu = ModsMenu::create(m_guiContext);
  if (!menu) return std::unexpected(std::move(menu.error()));
  (*menu)->setAnchor(PaneAnchor::Center);
  (*menu)->lockPosition();
  return m_paneManager->registerPane("modsMenu", PaneLayer::Hud, *menu,
      [this](std::shared_ptr<Pane> const&) { return back(); });
}

std::expected<void, StarException> TitleScreen::switchState(TitleState titleState) {
  if (m_titleState == titleState)
    return {};
  if (auto r = m_paneManager->dismissAllPanes(); !r) return std::unexpected(std::move(r.error()));
  m_titleState = titleState;
  // Leaving the multiplayer menus forgets the skipped connection screen.
  if (m_titleState < TitleState::MultiPlayerSelectCharacter || m_titleState > TitleState::MultiPlayerConnect)
    m_skipMultiPlayerConnection = false;
  if (titleState == TitleState::Main) {
    if (auto r = m_paneManager->displayRegisteredPane("mainMenu"); !r) return std::unexpected(std::move(r.error()));
  } else {
    if (auto r = m_paneManager->displayRegisteredPane("backMenu"); !r) return std::unexpected(std::move(r.error()));
    if (titleState == TitleState::Options) {
      if (auto r = m_paneManager->displayRegisteredPane("optionsMenu"); !r) return std::unexpected(std::move(r.error()));
    } else if (titleState == TitleState::Mods) {
      if (auto r = m_paneManager->displayRegisteredPane("modsMenu"); !r) return std::unexpected(std::move(r.error()));
    } else if (titleState == TitleState::SinglePlayerSelectCharacter || titleState == TitleState::MultiPlayerSelectCharacter) {
      if (auto r = m_paneManager->displayRegisteredPane("charSelectionMenu"); !r) return std::unexpected(std::move(r.error()));
    } else if (titleState == TitleState::SinglePlayerCreateCharacter || titleState == TitleState::MultiPlayerCreateCharacter) {
      if (auto r = m_paneManager->displayRegisteredPane("charCreationMenu"); !r) return std::unexpected(std::move(r.error()));
    } else if (titleState == TitleState::MultiPlayerConnect) {
      if (auto r = m_paneManager->displayRegisteredPane("multiplayerMenu"); !r) return std::unexpected(std::move(r.error()));
      if (auto r = m_paneManager->displayRegisteredPane("serverSelect"); !r) return std::unexpected(std::move(r.error()));
      auto addressWidget = m_multiPlayerMenu->fetchChild("address");
      if (!addressWidget) return std::unexpected(std::move(addressWidget.error()));
      if (*addressWidget) {
        if (auto r = (*addressWidget)->focus(); !r) return std::unexpected(std::move(r.error()));
      }
    }
  }
  if (titleState == TitleState::Quit)
    m_musicTrackManager.cancelAll();
  return {};
}

std::expected<void, StarException> TitleScreen::back() {
  if (m_titleState == TitleState::Options || m_titleState == TitleState::Mods ||
      m_titleState == TitleState::SinglePlayerSelectCharacter || m_titleState == TitleState::MultiPlayerSelectCharacter)
    return switchState(TitleState::Main);
  if (m_titleState == TitleState::SinglePlayerCreateCharacter)
    return switchState(TitleState::SinglePlayerSelectCharacter);
  if (m_titleState == TitleState::MultiPlayerCreateCharacter || m_titleState == TitleState::MultiPlayerConnect)
    return switchState(TitleState::MultiPlayerSelectCharacter);
  return {};
}

std::expected<void, StarException> TitleScreen::renderCursor() {
  Vec2I cursorPos = m_cursorScreenPos;
  Vec2I cursorSize = m_cursor->size();
  Vec2I cursorOffset = m_cursor->offset();
  float cursorScale = m_cursor->scale(interfaceScale());
  auto drawable = m_cursor->drawable();
  if (!drawable) return std::unexpected(std::move(drawable.error()));
  cursorPos[0] -= cursorOffset[0] * cursorScale;
  cursorPos[1] -= (cursorSize[1] - cursorOffset[1]) * cursorScale;
  auto hardwareCursor = m_guiContext.trySetCursor(*drawable, cursorOffset, cursorScale);
  if (!hardwareCursor) return std::unexpected(std::move(hardwareCursor.error()));
  if (!*hardwareCursor)
    return m_guiContext.drawDrawable(*drawable, Vec2F(cursorPos), cursorScale);
  return {};
}

float TitleScreen::interfaceScale() const {
  return m_guiContext.interfaceScale();
}


}

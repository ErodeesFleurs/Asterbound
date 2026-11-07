#ifdef STAR_SYSTEM_WINDOWS
#include <windows.h>
#include <stdio.h>
#endif

#include "imgui.h"
#include "imgui_freetype.h"
import std;
import star.optional;
import star.world_lua_bindings;
import star.memory;
import star.xx_hash;
import star.json;
import star.json_extra;
import star.thread;
import star.time;
import star.version;
import star.file;
import star.string;
import star.byte_array;
import star.encode;
import star.logging;
import star.ordered_map;
import star.ordered_set;
import star.format;
import star.output_proxy;
import star.native_runtime;
import star.data_stream_devices;
import star.interpolation;
import star.bi_map;
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
import star.directives;
import star.text;
import star.asset_path;
import star.listener;
import star.set;
import star.audio;
import star.map;
import star.io_device;
import star.ref_ptr;
import star.id_map;
import star.rpc_promise;
import star.strong_typedef;
import star.a_star;
import star.either;
import star.random;
import star.periodic_function;
import star.lru_cache;
import star.perlin;
import star.weighted_pool;
import star.image;
import star.math_common;
import star.net_compatibility;
import star.worker_pool;

import star.sector_array_2d;
import star.btree;
import star.btree_database;

import star.client_application;
import star.ttl_cache;
import star.random_point;
import star.lock_file;
import star.zstd_compression;
import star.universe;
import star.host_address;
import star.cellular_liquid;
import star.spawn_type_database;
import star.spawner;
import star.world_server;
import star.world_server_thread;
import star.system_world_server;
import star.system_world_server_thread;
import star.socket;
import star.tcp;
import star.net_packet_socket;
import star.universe_connection;
import star.universe_settings;
import star.universe_server;
import star.world_client_thread;
import star.universe_client;
import star.material_render_profile;
import star.tile_drawer;
import star.tile_painter;
import star.environment_painter;
import star.world_camera;
import star.world_painter;
import star.container_entity;
import star.container_interactor;
import star.inventory;
import star.main_interface;
import star.main_mixer;
import star.widget_parsing;
import star.gui_reader;
import star.list_widget;
import star.title_screen;
import star.interface_cursor;
import star.error_screen;
import star.cinematic;
import star.main_application;
import star.lua_root;
import star.configuration;
import star.asset_source;
import star.assets;
import star.root_base;
import star.root;
import star.option_parser;
import star.version_option_parser;
import star.uuid;
import star.item_descriptor;
import star.animation;
import star.particle;
import star.animated_part_set;
import star.light_source;
import star.networked_animator;
import star.humanoid;
import star.damage_types;
import star.world_geometry;
import star.status_types;
import star.damage;
import star.entity;
import star.interaction_types;
import star.tile_damage;
import star.interactive_entity;
import star.collision_block;
import star.tile_entity;
import star.tile_modification;
import star.force_regions;
import star.world;
import star.physics_entity;
import star.anchorable_entity;
import star.mobile_entity;
import star.actor_entity;
import star.tool_user_entity;
import star.lounging_entities;
import star.chat_action;
import star.chatty_entity;
import star.emote_entity;
import star.portrait_entity;
import star.damage_bar_entity;
import star.nametag_entity;
import star.inspectable_entity;
import star.inventory_types;
import star.movement_controller;
import star.platformer_astar_types;
import star.actor_movement_controller;
import star.celestial_coordinate;
import star.quest_descriptor;
import star.ai_types;
import star.entity_rendering_types;
import star.entity_rendering;
import star.player_types;
import star.lua_components;
import star.lua_actor_movement_component;
import star.lua_animation_component;
import star.radio_message_database;
import star.player;
import star.player_log;
import star.weather_types;
import star.sky_types;
import star.world_parameters;
import star.celestial_parameters;
import star.world_layout;
import star.plant;
import star.plant_database;
import star.biome_placement;
import star.celestial_types;
import star.sky_parameters;
import star.world_template;
import star.liquid_types;
import star.tile_sector_array;
import star.collision_generator;
import star.world_tiles;
import star.chat_types;
import star.warping;
import star.wiring;
import star.versioning_database;
import star.world_storage;
import star.system_world;
import star.damage_manager;
import star.net_packets;
import star.sky_render_data;
import star.parallax;
import star.cellular_light_array;
import star.cellular_lighting;
import star.world_render_data;
import star.world_structure;
import star.world_client_state;
import star.interpolation_tracker;
import star.ambient;
import star.weather;
import star.world_client;
import star.root_loader;
import star.input;
import star.lexical_cast;
import star.bytes;
struct OpusDecoder;
struct OpusEncoder;
import star.voice;


import star.application;
import star.statistics_service;
import star.p2p_networking_service;
import star.user_generated_content_service;
import star.desktop_service;
import star.application_controller;
import star.renderer;
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


import star.curve25519;
import star.camera_lua_bindings;
import star.http_trust_dialog;
import star.lua_http_bindings;
import star.clipboard_lua_bindings;
import star.input_lua_bindings;
import star.team_client_lua_bindings;
import star.voice_lua_bindings;
import star.interface_lua_bindings;
import star.rendering_lua_bindings;
import star.universe_client_lua_bindings;
import star.celestial_lua_bindings;

#if defined STAR_SYSTEM_WINDOWS
#include <windows.h>
// graphics driver is told by these exports to default to the dedicated GPU
extern "C" __declspec(dllexport) DWORD NvOptimusEnablement = 1;
extern "C" __declspec(dllexport) DWORD AmdPowerXpressRequestHighPerformance = 1;

// https://docs.kicad.org/doxygen/windows_2app_8cpp_source.html L45
extern "C" __declspec(dllexport) void NoHotPatch() { return; }
#endif 

import star.player_storage;
import star.statistics;
import star.client_context;

import star.algorithm;
namespace Star {

constexpr char AdditionalAssetsSettings[] = R"JSON(
    {
      "missingImage" : "/assetmissing.png",
      "missingAudio" : "/assetmissing.wav"
    }
  )JSON";

constexpr char AdditionalDefaultConfiguration[] = R"JSON(
    {
      "configurationVersion" : {
        "client" : 8
      },

      "allowAssetsMismatch" : false,
      "vsync" : true,
      "renderFpsLimit" : 0,
      "limitTextureAtlasSize" : false,
      "useMultiTexturing" : true,
      "audioChannelSeparation" : [-25, 25],

      "sfxVol" : 100,
      "instrumentVol" : 100,
      "musicVol" : 70,
      "hardwareCursor" : true,
      "windowedResolution" : [1000, 600],
      "fullscreenResolution" : [1920, 1080],
      "fullscreen" : false,
      "borderless" : false,
      "maximized" : true,
      "antiAliasing" : false,
      "hdr": true,
      "zoomLevel" : 3.0,
      "cameraSpeedFactor" : 1.0,
      "interfaceScale" : 0,
      "speechBubbles" : true,

      "title" : {
        "multiPlayerAddress" : "",
        "multiPlayerPort" : "",
        "multiPlayerAccount" : "",
        "multiPlayerForceLegacy" : false
      },

      "bindings" : {
        "PlayerUp" :  [ { "type" : "key", "value" : "W", "mods" : [] } ],
        "PlayerDown" :  [ { "type" : "key", "value" : "S", "mods" : [] } ],
        "PlayerLeft" :  [ { "type" : "key", "value" : "A", "mods" : [] } ],
        "PlayerRight" :  [ { "type" : "key", "value" : "D", "mods" : [] } ],
        "PlayerJump" :  [ { "type" : "key", "value" : "Space", "mods" : [] } ],
        "PlayerDropItem" :  [ { "type" : "key", "value" : "Q", "mods" : [] } ],
        "PlayerInteract" :  [ { "type" : "key", "value" : "E", "mods" : [] } ],
        "PlayerShifting" :  [ { "type" : "key", "value" : "RShift", "mods" : [] }, { "type" : "key", "value" : "LShift", "mods" : [] } ],
        "PlayerTechAction1" :  [ { "type" : "key", "value" : "F", "mods" : [] } ],
        "PlayerTechAction2" :  [],
        "PlayerTechAction3" :  [],
        "EmoteBlabbering" :  [ { "type" : "key", "value" : "Right", "mods" : ["LCtrl", "LShift"] } ],
        "EmoteShouting" :  [ { "type" : "key", "value" : "Up", "mods" : ["LCtrl", "LAlt"] } ],
        "EmoteHappy" :  [ { "type" : "key", "value" : "Up", "mods" : [] } ],
        "EmoteSad" :  [ { "type" : "key", "value" : "Down", "mods" : [] } ],
        "EmoteNeutral" :  [ { "type" : "key", "value" : "Left", "mods" : [] } ],
        "EmoteLaugh" :  [ { "type" : "key", "value" : "Left", "mods" : [ "LCtrl" ] } ],
        "EmoteAnnoyed" :  [ { "type" : "key", "value" : "Right", "mods" : [] } ],
        "EmoteOh" :  [ { "type" : "key", "value" : "Right", "mods" : [ "LCtrl" ] } ],
        "EmoteOooh" :  [ { "type" : "key", "value" : "Down", "mods" : [ "LCtrl" ] } ],
        "EmoteBlink" :  [ { "type" : "key", "value" : "Up", "mods" : [ "LCtrl" ] } ],
        "EmoteWink" :  [ { "type" : "key", "value" : "Up", "mods" : ["LCtrl", "LShift"] } ],
        "EmoteEat" :  [ { "type" : "key", "value" : "Down", "mods" : ["LCtrl", "LShift"] } ],
        "EmoteSleep" :  [ { "type" : "key", "value" : "Left", "mods" : ["LCtrl", "LShift"] } ],
        "ShowLabels" :  [ { "type" : "key", "value" : "RAlt", "mods" : [] }, { "type" : "key", "value" : "LAlt", "mods" : [] } ],
        "CameraShift" :  [ { "type" : "key", "value" : "RCtrl", "mods" : [] }, { "type" : "key", "value" : "LCtrl", "mods" : [] } ],
        "TitleBack" :  [ { "type" : "key", "value" : "Esc", "mods" : [] } ],
        "CinematicSkip" :  [ { "type" : "key", "value" : "Esc", "mods" : [] } ],
        "CinematicNext" :  [ { "type" : "key", "value" : "Right", "mods" : [] }, { "type" : "key", "value" : "Return", "mods" : [] } ],
        "GuiClose" :  [ { "type" : "key", "value" : "Esc", "mods" : [] } ],
        "GuiShifting" :  [ { "type" : "key", "value" : "RShift", "mods" : [] }, { "type" : "key", "value" : "LShift", "mods" : [] } ],
        "KeybindingCancel" :  [ { "type" : "key", "value" : "Esc", "mods" : [] } ],
        "KeybindingClear" :  [ { "type" : "key", "value" : "Del", "mods" : [] }, { "type" : "key", "value" : "Backspace", "mods" : [] } ],
        "ChatPageUp" :  [ { "type" : "key", "value" : "PageUp", "mods" : [] } ],
        "ChatPageDown" :  [ { "type" : "key", "value" : "PageDown", "mods" : [] } ],
        "ChatPreviousLine" :  [ { "type" : "key", "value" : "Up", "mods" : [] } ],
        "ChatNextLine" :  [ { "type" : "key", "value" : "Down", "mods" : [] } ],
        "ChatSendLine" :  [ { "type" : "key", "value" : "Return", "mods" : [] } ],
        "ChatBegin" :  [ { "type" : "key", "value" : "Return", "mods" : [] } ],
        "ChatBeginCommand" :  [ { "type" : "key", "value" : "/", "mods" : [] } ],
        "ChatStop" :  [ { "type" : "key", "value" : "Esc", "mods" : [] } ],
        "InterfaceHideHud" :  [ { "type" : "key", "value" : "F1", "mods" : [] } ],
        "InterfaceChangeBarGroup" :  [ { "type" : "key", "value" : "X", "mods" : [] } ],
        "InterfaceDeselectHands" :  [ { "type" : "key", "value" : "Z", "mods" : [] } ],
        "InterfaceBar1" :  [ { "type" : "key", "value" : "1", "mods" : [] } ],
        "InterfaceBar2" :  [ { "type" : "key", "value" : "2", "mods" : [] } ],
        "InterfaceBar3" :  [ { "type" : "key", "value" : "3", "mods" : [] } ],
        "InterfaceBar4" :  [ { "type" : "key", "value" : "4", "mods" : [] } ],
        "InterfaceBar5" :  [ { "type" : "key", "value" : "5", "mods" : [] } ],
        "InterfaceBar6" :  [ { "type" : "key", "value" : "6", "mods" : [] } ],
        "InterfaceBar7" :  [],
        "InterfaceBar8" :  [],
        "InterfaceBar9" :  [],
        "InterfaceBar10" :  [],
        "EssentialBar1" :  [ { "type" : "key", "value" : "R", "mods" : [] } ],
        "EssentialBar2" :  [ { "type" : "key", "value" : "T", "mods" : [] } ],
        "EssentialBar3" :  [ { "type" : "key", "value" : "Y", "mods" : [] } ],
        "EssentialBar4" :  [ { "type" : "key", "value" : "N", "mods" : [] } ],
        "InterfaceRepeatCommand" :  [ { "type" : "key", "value" : "P", "mods" : [] } ],
        "InterfaceToggleFullscreen" :  [ { "type" : "key", "value" : "F11", "mods" : [] } ],
        "InterfaceReload" :  [],
        "InterfaceEscapeMenu" :  [ { "type" : "key", "value" : "Esc", "mods" : [] } ],
        "InterfaceInventory" :  [ { "type" : "key", "value" : "I", "mods" : [] } ],
        "InterfaceCodex" :  [ { "type" : "key", "value" : "L", "mods" : [] } ],
        "InterfaceQuest" :  [ { "type" : "key", "value" : "J", "mods" : [] } ],
        "InterfaceCrafting" :  [ { "type" : "key", "value" : "C", "mods" : [] } ]
      }
    }
  )JSON";

ClientApplication::~ClientApplication() {
  m_reloadSubscription.disconnect();
}

std::expected<void, StarException> ClientApplication::startup(StringList const& cmdLineArgs) {
  auto assetsSettings = Json::parseJson(AdditionalAssetsSettings);
  if (!assetsSettings)
    return std::unexpected(std::move(assetsSettings.error()));
  auto defaults = Json::parseJson(AdditionalDefaultConfiguration);
  if (!defaults)
    return std::unexpected(std::move(defaults.error()));
  auto loader = RootLoader::create({std::move(*assetsSettings), std::move(*defaults), String("starbound.log"), LogLevel::Info, false, String("starbound.config")});
  if (!loader)
    return std::unexpected(std::move(loader.error()));
  loader->setVersionName("Client");
  auto initialized = loader->initOrDie(cmdLineArgs);
  if (!initialized)
    return std::unexpected(std::move(initialized.error()));
  m_root = std::move(initialized->first);
  if (auto logged = m_root->logger().info("{}", loader->getVersionString()); !logged)
    return std::unexpected(std::move(logged.error()));
  #ifdef __clang__
  if (auto logged = m_root->logger().info("Compiled with Clang {}", __clang_version__); !logged)
    return std::unexpected(std::move(logged.error()));
  #endif
  return {};
}

std::expected<void, StarException> ClientApplication::shutdown() {
  m_reloadSubscription.disconnect();
  std::optional<StarException> failure;
  auto remember = [&](std::expected<void, StarException> result) {
    if (!result) {
      auto logged = m_root->logger().error("Client shutdown: {}", outputException(ExceptionDiagnostic(result.error()), true));
      if (!logged)
        EmergencyLog::error("Client shutdown logging failed: {}", outputException(ExceptionDiagnostic(logged.error()), true));
      if (!failure)
        failure = std::move(result.error());
    }
  };
  if (m_rootLoader)
    remember(m_rootLoader.finish());
  if (m_httpSession)
    remember(m_httpSession->stop());
  if (m_player)
    m_player->setInput(std::nullopt);
  // Lua uninit may still invoke the interface and camera callbacks.
  if (m_universeClient)
    remember(m_universeClient->disconnect());
  if (m_mainInterface)
    remember(m_mainInterface->close());
  m_mainInterface.reset();
  if (m_universeServer) {
    remember(m_universeServer->close());
    m_universeServer.reset();
  }
  if (m_statistics)
    remember(m_statistics->writeStatistics());
  m_universeClient.reset();
  m_statistics.reset();
  m_httpSession.reset();
  if (m_titleScreen)
    remember(m_titleScreen->close());
  m_titleScreen.reset();
  m_player.reset();
  m_playerStorage.reset();
  m_cinematicOverlay.reset();
  m_errorScreen.reset();
  if (m_guiContext)
    m_guiContext->setTileDrawer(std::nullopt);
  if (m_mainMixer) {
    m_mainMixer->setVoice(std::nullopt);
    m_mainMixer->setUniverseClient({});
    m_mainMixer->setWorldPainter(std::nullopt);
  }
  m_worldPainter.reset();
  m_voice.reset();
  m_input.reset();
  m_guiContext.reset();
  m_mainMixer.reset();
  if (failure)
    return std::unexpected(std::move(*failure));
  return {};
}

std::expected<void, StarException> ClientApplication::applicationInit(std::shared_ptr<ApplicationController> appController) {
  auto base = Application::applicationInit(appController);
  if (!base)
    return std::unexpected(std::move(base.error()));
  appController->setCursorVisible(true);
  auto configResult = m_root->configuration();
  if (!configResult)
    return std::unexpected(std::move(configResult.error()));
  auto configuration = *configResult;
  auto windowedSize = configuration->get("windowedResolution").and_then([](Json j) { return jsonToVec2U(j); });
  if (!windowedSize)
    return std::unexpected(std::move(windowedSize.error()));
  auto fullscreenSize = configuration->get("fullscreenResolution").and_then([](Json j) { return jsonToVec2U(j); });
  if (!fullscreenSize)
    return std::unexpected(std::move(fullscreenSize.error()));
  auto fullscreen = configuration->get("fullscreen").and_then([](Json j) { return j.toBool(); });
  if (!fullscreen)
    return std::unexpected(std::move(fullscreen.error()));
  auto borderless = configuration->get("borderless").and_then([](Json j) { return j.toBool(); });
  if (!borderless)
    return std::unexpected(std::move(borderless.error()));
  auto maximized = configuration->get("maximized").and_then([](Json j) { return j.toBool(); });
  if (!maximized)
    return std::unexpected(std::move(maximized.error()));
  auto controller = configuration->get("controllerInput").and_then([](Json j) { return j.optBool(); });
  if (!controller)
    return std::unexpected(std::move(controller.error()));
  m_controllerInput = controller->value_or(false);
  #ifdef STAR_SYSTEM_WINDOWS
  auto workaround = configuration->get("borderlessWorkaround", true).and_then([](Json j) { return j.toBool(); });
  if (!workaround)
    return std::unexpected(std::move(workaround.error()));
  appController->setBorderlessWorkaround(*workaround);
  #endif
  if (*fullscreen) {
    auto changed = appController->setFullscreenWindow(*fullscreenSize);
    if (!changed)
      return std::unexpected(std::move(changed.error()));
  } else if (*borderless) {
    auto changed = appController->setBorderlessWindow();
    if (!changed)
      return std::unexpected(std::move(changed.error()));
  } else if (*maximized) {
    appController->setMaximizedWindow();
  } else {
    appController->setNormalWindow(*windowedSize);
  }
  float updateRate = 1.0f / m_root->timing().timestep;
  auto jUpdateRate = configuration->get("updateRate");
  if (!jUpdateRate)
    return std::unexpected(std::move(jUpdateRate.error()));
  if (*jUpdateRate) {
    auto rate = jUpdateRate->toFloat();
    if (!rate)
      return std::unexpected(std::move(rate.error()));
    if (!isFinite(*rate) || *rate <= 0.0f)
      return std::unexpected(StarException("updateRate must be finite and positive"));
    updateRate = *rate;
    m_root->timing().timestep = 1.0f / updateRate;
  }
  auto jServerRate = configuration->get("serverUpdateRate");
  if (!jServerRate)
    return std::unexpected(std::move(jServerRate.error()));
  if (*jServerRate) {
    auto rate = jServerRate->toFloat();
    if (!rate)
      return std::unexpected(std::move(rate.error()));
    if (!isFinite(*rate) || *rate <= 0.0f)
      return std::unexpected(StarException("serverUpdateRate must be finite and positive"));
    m_root->timing().serverTimestep = 1.0f / *rate;
  }
  appController->setTargetUpdateRate(updateRate);
  if (auto applied = applyRenderSettings(*configuration); !applied)
    return std::unexpected(std::move(applied.error()));
  auto hardware = configuration->get("hardwareCursor").and_then([](Json j) { return j.optBool(); });
  if (!hardware)
    return std::unexpected(std::move(hardware.error()));
  appController->setCursorHardware(hardware->value_or(true));
  // Must precede anything that can load assets.
  auto mods = loadMods();
  if (!mods)
    return std::unexpected(std::move(mods.error()));
  AudioFormat audioFormat = appController->enableAudio();
  m_mainMixer = std::make_unique<MainMixer>(*m_root, audioFormat.sampleRate, audioFormat.channels);
  m_mainMixer->setVolume(0.5);
  auto painter = WorldPainter::create(*m_root);
  if (!painter)
    return std::unexpected(std::move(painter.error()));
  m_worldPainter = std::move(*painter);
  auto gui = GuiContext::create(*m_root, m_mainMixer->mixer(), appController);
  if (!gui)
    return std::unexpected(std::move(gui.error()));
  m_guiContext = std::move(*gui);
  auto input = Input::create(*m_root);
  if (!input)
    return std::unexpected(std::move(input.error()));
  m_input = std::move(*input);
  auto voice = Voice::create(*m_root, appController);
  if (!voice)
    return std::unexpected(std::move(voice.error()));
  m_voice = std::move(*voice);
  m_guiContext->bindInput(*m_input);
  m_guiContext->bindVoice(*m_voice);
  m_mainMixer->setVoice(optionalRef(m_voice));
  auto assetsResult = m_root->assets();
  if (!assetsResult)
    return std::unexpected(std::move(assetsResult.error()));
  auto assets = *assetsResult;
  {
    auto& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
    auto font = assets->bytes("/hobo.ttf");
    if (!font)
      return std::unexpected(std::move(font.error()));
    m_immediateFont = **font;
    ImFontConfig config{};
    config.FontDataOwnedByAtlas = false;
    config.FontLoaderFlags = ImGuiFreeTypeLoaderFlags_ForceAutoHint;
    io.Fonts->AddFontFromMemoryTTF(m_immediateFont.ptr(), m_immediateFont.size(),
      16, &config, io.Fonts->GetGlyphRangesDefault());
  }
  auto minScale = assets->json("/interface.config:minInterfaceScale").and_then([](Json j) { return j.toFloat(); });
  if (!minScale)
    return std::unexpected(std::move(minScale.error()));
  m_minInterfaceScale = *minScale;
  auto maxScale = assets->json("/interface.config:maxInterfaceScale").and_then([](Json j) { return j.toFloat(); });
  if (!maxScale)
    return std::unexpected(std::move(maxScale.error()));
  m_maxInterfaceScale = *maxScale;
  auto crossover = assets->json("/interface.config:interfaceCrossoverRes").and_then([](Json j) { return jsonToVec2F(j); });
  if (!crossover)
    return std::unexpected(std::move(crossover.error()));
  m_crossoverRes = *crossover;
  auto title = assets->json("/client.config:windowTitle").and_then([](Json j) { return j.toString(); });
  if (!title)
    return std::unexpected(std::move(title.error()));
  appController->setApplicationTitle(*title);
  auto frameSkip = assets->json("/client.config:maxFrameSkip").and_then([](Json j) { return j.toUInt(); });
  if (!frameSkip)
    return std::unexpected(std::move(frameSkip.error()));
  appController->setMaxFrameSkip(*frameSkip);
  auto jVoice = configuration->get("voice");
  if (!jVoice)
    return std::unexpected(std::move(jVoice.error()));
  if (*jVoice) {
    auto loaded = m_voice->loadJson(*jVoice, true);
    if (!loaded)
      return std::unexpected(std::move(loaded.error()));
  }
  auto initialized = m_voice->init();
  if (!initialized)
    return std::unexpected(std::move(initialized.error()));
  m_voice->setLocalSpeaker(0);
  return {};
}

std::expected<void, StarException> ClientApplication::renderInit(std::shared_ptr<Renderer> renderer) {
  auto base = Application::renderInit(renderer);
  if (!base)
    return std::unexpected(std::move(base.error()));
  auto reloaded = renderReload();
  if (!reloaded)
    return std::unexpected(std::move(reloaded.error()));
  m_reloadSubscription.disconnect();
  m_reloadSubscription = m_root->registerReloadListener(
      m_reloadListener = std::make_shared<CallbackListener>([app = std::ref(*this)] { return app.get().renderReload(); }));
  auto configResult = m_root->configuration();
  if (!configResult)
    return std::unexpected(std::move(configResult.error()));
  auto config = *configResult;
  auto limit = config->get("limitTextureAtlasSize").and_then([](Json j) { return j.optBool(); });
  if (!limit)
    return std::unexpected(std::move(limit.error()));
  if (limit->value_or(false))
    renderer->setSizeLimitEnabled(true);
  auto multi = config->get("useMultiTexturing").and_then([](Json j) { return j.optBool(); });
  if (!multi)
    return std::unexpected(std::move(multi.error()));
  renderer->setMultiTexturingEnabled(multi->value_or(true));
  auto gui = m_guiContext->renderInit(renderer);
  if (!gui)
    return std::unexpected(std::move(gui.error()));
  auto cinematic = Cinematic::create(*m_guiContext);
  if (!cinematic)
    return std::unexpected(std::move(cinematic.error()));
  m_cinematicOverlay = std::move(*cinematic);
  auto errorScreen = ErrorScreen::create(*m_guiContext);
  if (!errorScreen)
    return std::unexpected(std::move(errorScreen.error()));
  m_errorScreen = std::move(*errorScreen);
  if (m_titleScreen) {
    auto title = m_titleScreen->renderInit(renderer);
    if (!title)
      return std::unexpected(std::move(title.error()));
  }
  if (m_worldPainter) {
    auto painter = m_worldPainter->renderInit(renderer);
    if (!painter)
      return std::unexpected(std::move(painter.error()));
    m_guiContext->setTileDrawer(m_worldPainter->tileDrawer());
  }
  #ifdef STAR_ENABLE_STEAM_INTEGRATION
  #ifdef STAR_SYSTEM_LINUX
  if (appController()->steamIsFlatpak()) {
    auto shown = config->get("steamFlatpakWarningShown").and_then([](Json j) { return j.optBool(); });
    if (!shown)
      return std::unexpected(std::move(shown.error()));
    if (!shown->value_or(false)) {
      auto saved = config->set("steamFlatpakWarningShown", true);
      if (!saved)
        return std::unexpected(std::move(saved.error()));
      auto message = m_root->assets().and_then([](auto assets) { return assets->json("/interface.config:steamFlatpakWarning"); })
          .and_then([](Json j) { return j.toString(); });
      if (!message)
        return std::unexpected(std::move(message.error()));
      auto displayed = m_errorScreen->setMessage(*message);
      if (!displayed)
        return std::unexpected(std::move(displayed.error()));
      return changeState(MainAppState::SteamFlatpakWarning);
    }
  }
  #endif
  #endif
  return changeState(MainAppState::Mods);
}

std::expected<void, StarException> ClientApplication::windowChanged(WindowMode windowMode, Vec2U screenSize) {
  auto configResult = m_root->configuration();
  if (!configResult)
    return std::unexpected(std::move(configResult.error()));
  auto config = *configResult;
  if (windowMode == WindowMode::Fullscreen) {
    if (auto saved = config->set("fullscreenResolution", jsonFromVec2U(screenSize)); !saved)
      return std::unexpected(std::move(saved.error()));
    if (auto saved = config->set("fullscreen", true); !saved)
      return std::unexpected(std::move(saved.error()));
    if (auto saved = config->set("borderless", false); !saved)
      return std::unexpected(std::move(saved.error()));
  } else if (windowMode == WindowMode::Borderless) {
    if (auto saved = config->set("borderless", true); !saved)
      return std::unexpected(std::move(saved.error()));
    if (auto saved = config->set("fullscreen", false); !saved)
      return std::unexpected(std::move(saved.error()));
  } else if (windowMode == WindowMode::Maximized) {
    if (auto saved = config->set("maximized", true); !saved)
      return std::unexpected(std::move(saved.error()));
    if (auto saved = config->set("fullscreen", false); !saved)
      return std::unexpected(std::move(saved.error()));
    if (auto saved = config->set("borderless", false); !saved)
      return std::unexpected(std::move(saved.error()));
  } else {
    if (auto saved = config->set("maximized", false); !saved)
      return std::unexpected(std::move(saved.error()));
    if (auto saved = config->set("fullscreen", false); !saved)
      return std::unexpected(std::move(saved.error()));
    if (auto saved = config->set("borderless", false); !saved)
      return std::unexpected(std::move(saved.error()));
    if (auto saved = config->set("windowedResolution", jsonFromVec2U(screenSize)); !saved)
      return std::unexpected(std::move(saved.error()));
  }
  return {};
}

std::expected<void, StarException> ClientApplication::processInput(InputEvent const& event) {
  restorePresentedCamera();
  if (std::holds_alternative<KeyDownEvent>(event)) {
    auto const& keyDown = std::get<KeyDownEvent>(event);
    m_heldKeyEvents.append(keyDown);
    m_edgeKeyEvents.append(keyDown);
  } else if (std::holds_alternative<KeyUpEvent>(event)) {
    auto const& keyUp = std::get<KeyUpEvent>(event);
    eraseWhere(m_heldKeyEvents, [&](auto& keyEvent) {
      return keyEvent.key == keyUp.key;
    });

    auto keyName = enumName<KeyNames>(keyUp.key);
    if (!keyName)
      return std::unexpected(std::move(keyName.error()));
    auto modKey = maybeEnumValue<KeyModNames>(*keyName);
    if (!modKey)
      return std::unexpected(std::move(modKey.error()));
    if (*modKey)
      m_heldKeyEvents.transform([&](auto& keyEvent) {
        return KeyDownEvent{keyEvent.key, keyEvent.mods & ~**modKey};
      });
  }
  else if (std::holds_alternative<ControllerAxisEvent>(event)) {
    auto const& cAxis = std::get<ControllerAxisEvent>(event);
    if (cAxis.controllerAxis == ControllerAxis::LeftX)
      m_controllerLeftStick[0] = cAxis.controllerAxisValue;
    else if (cAxis.controllerAxis == ControllerAxis::LeftY)
      m_controllerLeftStick[1] = cAxis.controllerAxisValue;
    else if (cAxis.controllerAxis == ControllerAxis::RightX)
      m_controllerRightStick[0] = cAxis.controllerAxisValue;
    else if (cAxis.controllerAxis == ControllerAxis::RightY)
      m_controllerRightStick[1] = cAxis.controllerAxisValue;
  }

  bool processed = false;
  if (!m_errorScreen->accepted()) {
    auto handled = m_errorScreen->handleInputEvent(event);
    if (!handled)
      return std::unexpected(std::move(handled.error()));
    processed = *handled;
  }
  if (!processed && (m_state == MainAppState::Splash || m_state >= MainAppState::Title)) {
    auto handled = m_cinematicOverlay->handleInputEvent(event);
    if (!handled)
      return std::unexpected(std::move(handled.error()));
    processed = *handled;
    if (!processed && m_state == MainAppState::Title) {
      auto title = m_titleScreen->handleInputEvent(event);
      if (!title)
        return std::unexpected(std::move(title.error()));
      processed = *title;
    } else if (!processed && m_state > MainAppState::Title) {
      auto interfaceInput = m_mainInterface->handleInputEvent(event);
      if (!interfaceInput)
        return std::unexpected(std::move(interfaceInput.error()));
      processed = *interfaceInput;
    }
  }
  auto input = m_input->handleInput(event, processed);
  if (!input)
    return std::unexpected(std::move(input.error()));
  return {};
}

std::expected<void, StarException> ClientApplication::update() {
  restorePresentedCamera();
  // Input between logic ticks must keep using the transform actually shown,
  // even when a low render cap allows several updates before the next present.
  struct DisplayCameraScope {
    std::shared_ptr<WorldPainter> painter;
    WorldCamera displayed;
    ~DisplayCameraScope() { painter->camera().restoreRenderTransform(displayed); }
  } displayCamera{m_worldPainter, m_worldPainter->camera()};
  auto configuration = m_root->configuration();
  if (!configuration)
    return std::unexpected(std::move(configuration.error()));
  if (auto applied = applyRenderSettings(**configuration); !applied)
    return std::unexpected(std::move(applied.error()));
  float dt = m_root->timing().timestep * m_root->timing().timescale;
  auto& app = appController();
  if (m_state >= MainAppState::Title) {
    if (auto service = app->p2pNetworkingService()) {
      auto join = service->pullPendingJoin();
      if (!std::holds_alternative<std::monostate>(join)) {
        if (std::holds_alternative<P2PNetworkingPeerId>(join))
          m_pendingMultiPlayerConnection =
            PendingMultiPlayerConnection{.server = std::move(std::get<P2PNetworkingPeerId>(join)),
                                         .account = {},
                                         .password = {},
                                         .forceLegacy = false};
        else
          m_pendingMultiPlayerConnection =
            PendingMultiPlayerConnection{.server = std::move(std::get<HostAddressWithPort>(join)),
                                         .account = {},
                                         .password = {},
                                         .forceLegacy = false};
        join.emplace<0>();
        auto changed = changeState(MainAppState::Title);
        if (!changed)
          return std::unexpected(std::move(changed.error()));
      }
      if (auto request = service->pullJoinRequest()) {
        auto queued = m_mainInterface->queueJoinRequest(*request);
        if (!queued)
          return std::unexpected(std::move(queued.error()));
      }
      auto updated = service->update();
      if (!updated)
        return std::unexpected(std::move(updated.error()));
    }
  }
  if (!m_errorScreen->accepted()) {
    auto updated = m_errorScreen->update(dt);
    if (!updated)
      return std::unexpected(std::move(updated.error()));
  }
  #ifdef STAR_ENABLE_STEAM_INTEGRATION
  #ifdef STAR_SYSTEM_LINUX
  if (m_state == MainAppState::SteamFlatpakWarning) {
    auto updated = updateSteamFlatpakWarning(dt);
    if (!updated)
      return std::unexpected(std::move(updated.error()));
  } else
  #endif
  #endif
  if (m_state == MainAppState::Mods) {
    auto updated = updateMods(dt);
    if (!updated)
      return std::unexpected(std::move(updated.error()));
  } else if (m_state == MainAppState::ModsWarning) {
    auto updated = updateModsWarning(dt);
    if (!updated)
      return std::unexpected(std::move(updated.error()));
  }
  std::expected<void, StarException> stateUpdate;
  if (m_state == MainAppState::Splash)
    stateUpdate = updateSplash(dt);
  else if (m_state == MainAppState::Error)
    stateUpdate = updateError(dt);
  else if (m_state == MainAppState::Title)
    stateUpdate = updateTitle(dt);
  else if (m_state > MainAppState::Title) {
    stateUpdate = updateRunning(dt);
    if (!stateUpdate)
      stateUpdate = setError("Exception caught in client main-loop", ExceptionDiagnostic(stateUpdate.error()));
  }
  if (!stateUpdate)
    return std::unexpected(std::move(stateUpdate.error()));
  // Drain encoded voice outside gameplay so microphone settings remain live.
  if (m_state <= MainAppState::Title) {
    DataStreamBuffer data;
    auto sent = m_voice->send(data);
    if (!sent)
      return std::unexpected(std::move(sent.error()));
  }
  auto cleaned = m_guiContext->cleanup();
  if (!cleaned)
    return std::unexpected(std::move(cleaned.error()));
  m_edgeKeyEvents.clear();
  m_input->update();
  ++m_framesSkipped;
  return {};
}

std::expected<void, StarException> ClientApplication::applyRenderSettings(Configuration const& configuration) {
  auto limit = configuration.get("renderFpsLimit").and_then([](Json value) { return value.toDouble(); });
  if (!limit)
    return std::unexpected(std::move(limit.error()));
  if (!isFinite(*limit) || *limit < 0.0 || *limit > std::numeric_limits<float>::max())
    return std::unexpected(StarException("renderFpsLimit must be finite, nonnegative and representable as a float"));
  auto vsync = configuration.get("vsync").and_then([](Json value) { return value.toBool(); });
  if (!vsync)
    return std::unexpected(std::move(vsync.error()));
  if (m_appliedVSync != *vsync) {
    if (auto applied = appController()->setVSyncEnabled(*vsync); !applied)
      return std::unexpected(std::move(applied.error()));
    m_appliedVSync = *vsync;
  }
  float rate = static_cast<float>(*limit);
  if (m_appliedRenderFpsLimit != rate) {
    appController()->setTargetRenderRate(rate);
    m_appliedRenderFpsLimit = rate;
  }
  return {};
}

std::expected<void, StarException> ClientApplication::render() {
  m_preparedCamera.reset();
  m_framesSkipped = 0;
  auto configResult = m_root->configuration();
  if (!configResult)
    return std::unexpected(std::move(configResult.error()));
  auto config = *configResult;
  auto& renderer = Application::renderer();

  auto aa = config->get("antiAliasing").and_then([](Json j) { return j.optBool(); });
  if (!aa)
    return std::unexpected(std::move(aa.error()));
  auto sampling = renderer->setMultiSampling(aa->value_or(false) ? 4 : 0);
  if (!sampling)
    return std::unexpected(std::move(sampling.error()));
  auto hdr = config->get("hdr").and_then([](Json j) { return j.optBool(); });
  if (!hdr)
    return std::unexpected(std::move(hdr.error()));
  auto mainHdr = renderer->setMainHDR(hdr->value_or(true));
  if (!mainHdr)
    return std::unexpected(std::move(mainHdr.error()));
  auto effect = renderer->switchEffectConfig("interface");
  if (!effect)
    return std::unexpected(std::move(effect.error()));
  auto scale = config->get("interfaceScale").and_then([](Json j) { return j.optFloat(); });
  if (!scale)
    return std::unexpected(std::move(scale.error()));
  if (scale->value_or(0.0f) != 0) {
    m_guiContext->setInterfaceScale(**scale);
  } else {
    auto width = m_guiContext->windowWidth();
    if (!width)
      return std::unexpected(std::move(width.error()));
    auto height = m_guiContext->windowHeight();
    if (!height)
      return std::unexpected(std::move(height.error()));
    m_guiContext->setInterfaceScale(*width >= m_crossoverRes[0] && *height >= m_crossoverRes[1] ? m_maxInterfaceScale : m_minInterfaceScale);
  }

  if (m_state == MainAppState::Mods || m_state == MainAppState::Splash) {
    auto cinematic = m_cinematicOverlay->render();
    if (!cinematic)
      return std::unexpected(std::move(cinematic.error()));

  } else if (m_state == MainAppState::Title) {
    auto title = m_titleScreen->render();
    if (!title)
      return std::unexpected(std::move(title.error()));
    auto cinematic = m_cinematicOverlay->render();
    if (!cinematic)
      return std::unexpected(std::move(cinematic.error()));

  } else if (m_state > MainAppState::Title) {
    std::shared_ptr<WorldClient> worldClient = m_universeClient->worldClient();
    PresentationContext const presentation(
        m_universeClient->paused() || m_root->timing().timescale == 0.0f
            ? 1.0f : appController()->renderInterpolation(),
        appController()->renderEpoch());
    if (worldClient) {
      auto& camera = m_worldPainter->camera();
      auto playerEpoch = m_player ? m_player->presentationEpoch() : 0;
      auto renderEpoch = presentation.epoch();
      if (m_presentedWorld.lock() != worldClient || m_presentedPlayer.lock() != m_player
          || m_presentedPlayerEpoch != playerEpoch || m_presentedRenderEpoch != renderEpoch) {
        worldClient->resetPresentation();
        camera.resetPresentation();
        m_presentedWorld = worldClient;
        m_presentedPlayer = m_player;
        m_presentedPlayerEpoch = playerEpoch;
        m_presentedRenderEpoch = renderEpoch;
      }
      camera.setScreenSize(renderer->screenSize());
      camera.setRenderInterpolation(presentation.interpolation());
      auto totalStart = std::chrono::time_point_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() + std::chrono::nanoseconds(500));
      auto worldEffect = renderer->switchEffectConfig("world");
      if (!worldEffect)
        return std::unexpected(std::move(worldEffect.error()));
      auto clientStart = totalStart;
      auto rendered = worldClient->render(m_renderData, TilePainter::BorderTileSize, m_worldPainter->tileDrawer(), presentation);
      if (!rendered)
        return std::unexpected(std::move(rendered.error()));
      m_root->logMap().set("client_render_world_client", strf("{:05d}\u00b5s", (std::chrono::time_point_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() + std::chrono::nanoseconds(500)) - clientStart).count()));

      auto paintStart = std::chrono::time_point_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() + std::chrono::nanoseconds(500));
      auto painted = m_worldPainter->render(m_renderData, [&]() -> std::expected<bool, StarException> {
        return worldClient->waitForLighting(std::ref(m_renderData));
      });
      if (!painted)
        return std::unexpected(std::move(painted.error()));
      m_root->logMap().set("client_render_world_painter", strf("{:05d}\u00b5s", (std::chrono::time_point_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() + std::chrono::nanoseconds(500)) - paintStart).count()));
      m_root->logMap().set("client_render_world_total", strf("{:05d}\u00b5s", (std::chrono::time_point_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() + std::chrono::nanoseconds(500)) - totalStart).count()));
      m_preparedCamera = PresentedCamera{camera, worldClient, m_player, playerEpoch, renderEpoch};
      
      auto size = Vec2F(renderer->screenSize());
      auto quad = renderFlatRect(RectF::withSize(size / -2, size), Vec4B::filled(0), 0.0f);
      for (auto& layer : m_postProcessLayers) {
        if (!layer.group || layer.group->get().enabled) {
          for (unsigned i = 0; i < layer.passes; i++) {
            for (auto& effect : layer.effects) {
              auto switched = renderer->switchEffectConfig(effect);
              if (!switched)
                return std::unexpected(std::move(switched.error()));
              renderer->render(quad);
            }
          }
        }
      }
    }
    auto interfaceEffect = renderer->switchEffectConfig("interface");
    if (!interfaceEffect)
      return std::unexpected(std::move(interfaceEffect.error()));
    auto start = std::chrono::time_point_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() + std::chrono::nanoseconds(500));
    auto inWorld = m_mainInterface->renderInWorldElements(presentation);
    if (!inWorld)
      return std::unexpected(std::move(inWorld.error()));
    auto interfaceRendered = m_mainInterface->render();
    if (!interfaceRendered)
      return std::unexpected(std::move(interfaceRendered.error()));
    auto cinematic = m_cinematicOverlay->render();
    if (!cinematic)
      return std::unexpected(std::move(cinematic.error()));
    m_root->logMap().set("client_render_interface", strf("{:05d}\u00b5s", (std::chrono::time_point_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() + std::chrono::nanoseconds(500)) - start).count()));
  }

  if (!m_errorScreen->accepted())
    return m_errorScreen->render();
  return {};
}

void ClientApplication::restorePresentedCamera() {
  if (!m_inputCamera || !m_worldPainter)
    return;
  auto world = m_universeClient ? m_universeClient->worldClient() : nullptr;
  if (m_state <= MainAppState::Title || !world
      || m_inputCamera->world.lock() != world || m_inputCamera->player.lock() != m_player
      || m_inputCamera->playerEpoch != (m_player ? m_player->presentationEpoch() : 0)
      || m_inputCamera->renderEpoch != appController()->renderEpoch()) {
    m_inputCamera.reset();
    m_worldPainter->camera().resetPresentation();
    return;
  }
  m_worldPainter->camera().restoreRenderTransform(m_inputCamera->camera);
}

std::expected<void, StarException> ClientApplication::framePresented() {
  m_inputCamera = std::move(m_preparedCamera);
  m_preparedCamera.reset();
  restorePresentedCamera();
  return {};
}

std::expected<void, StarException> ClientApplication::getAudioData(std::span<std::int16_t> sampleData, std::size_t frameCount) {
  if (!m_mainMixer)
    return {};
  return m_mainMixer->read(sampleData, frameCount, [&](std::span<std::int16_t> buffer, std::size_t frames, unsigned channels) -> std::expected<void, StarException> {
    if (m_voice)
      m_voice->mix(buffer, frames, channels);
    return {};
  });
}

constexpr char postProcessGroupsRoot[] = "postProcessGroups";

std::expected<void, StarException> ClientApplication::renderReload() {
  m_presentedWorld.reset();
  auto assetsResult = m_root->assets();
  if (!assetsResult)
    return std::unexpected(std::move(assetsResult.error()));
  auto assets = *assetsResult;
  auto renderer = Application::renderer();
  auto loadEffectConfig = [&](String const& name) -> std::expected<void, StarException> {
    String path = strf("/rendering/effects/{}.config", name);
    auto exists = assets->assetExists(path);
    if (!exists)
      return std::unexpected(std::move(exists.error()));
    if (!*exists) {
      return m_root->logger().warn("No rendering config found for renderer with id '{}'", renderer->rendererId());
    }
    auto config = assets->json(path);
    if (!config)
      return std::unexpected(std::move(config.error()));
    auto shaderConfig = config->getObject("effectShaders");
    if (!shaderConfig)
      return std::unexpected(std::move(shaderConfig.error()));
    StringMap<String> shaders;
    for (auto& entry : *shaderConfig) {
      if (!entry.second.isType(Json::Type::String))
        continue;
      auto shader = entry.second.toString();
      if (!shader)
        return std::unexpected(std::move(shader.error()));
      auto multiline = shader->hasChar('\n');
      if (!multiline)
        return std::unexpected(std::move(multiline.error()));
      if (!*multiline) {
        auto relative = AssetPath::relativeTo(path, *shader);
        auto bytes = assets->bytes(relative);
        if (!bytes)
          return std::unexpected(std::move(bytes.error()));
        *shader = std::string((*bytes)->begin(), (*bytes)->end());
      }
      shaders[entry.first] = std::move(*shader);
    }
    return renderer->loadEffectConfig(name, *config, shaders);
  };
  auto glConfig = assets->json("/rendering/opengl.config");
  if (!glConfig)
    return std::unexpected(std::move(glConfig.error()));
  auto loaded = renderer->loadConfig(*glConfig);
  if (!loaded)
    return std::unexpected(std::move(loaded.error()));
  auto world = loadEffectConfig("world");
  if (!world)
    return std::unexpected(std::move(world.error()));
  auto configResult = m_root->configuration();
  if (!configResult)
    return std::unexpected(std::move(configResult.error()));
  auto config = *configResult;
  auto groupsConfig = config->get(postProcessGroupsRoot);
  if (!groupsConfig)
    return std::unexpected(std::move(groupsConfig.error()));
  if (!groupsConfig->isType(Json::Type::Object)) {
    auto saved = config->set(postProcessGroupsRoot, JsonObject());
    if (!saved)
      return std::unexpected(std::move(saved.error()));
    *groupsConfig = JsonObject();
  }
  m_postProcessGroups.clear();
  auto groups = assets->json("/client.config:postProcessGroups").and_then([](Json j) { return j.toObject(); });
  if (!groups)
    return std::unexpected(std::move(groups.error()));
  for (auto& [name, definition] : *groups) {
    auto groupConfig = groupsConfig->opt(name);
    if (!groupConfig)
      return std::unexpected(std::move(groupConfig.error()));
    auto enabled = definition.getBool("enabledDefault", true);
    if (!enabled)
      return std::unexpected(std::move(enabled.error()));
    if (*groupConfig) {
      enabled = (**groupConfig).getBool("enabled", *enabled);
      if (!enabled)
        return std::unexpected(std::move(enabled.error()));
    } else {
      auto saved = config->setPath(strf("{}.{}", postProcessGroupsRoot, name), JsonObject());
      if (!saved)
        return std::unexpected(std::move(saved.error()));
    }
    auto added = m_postProcessGroups.add(name, PostProcessGroup{*enabled});
    if (!added)
      return std::unexpected(std::move(added.error()));
  }
  m_postProcessLayers.clear();
  m_labelledPostProcessLayers.clear();
  auto layers = assets->json("/client.config:postProcessLayers").and_then([](Json j) { return j.iterateArray(); });
  if (!layers)
    return std::unexpected(std::move(layers.error()));
  for (auto const& layer : *layers) {
    auto effects = layer.get("effects").and_then(jsonToStringList);
    if (!effects)
      return std::unexpected(std::move(effects.error()));
    for (auto& effect : *effects) {
      auto loadedEffect = loadEffectConfig(effect);
      if (!loadedEffect)
        return std::unexpected(std::move(loadedEffect.error()));
    }
    std::optional<std::reference_wrapper<PostProcessGroup>> group;
    auto groupName = layer.optString("group");
    if (!groupName)
      return std::unexpected(std::move(groupName.error()));
    if (*groupName) {
      auto found = m_postProcessGroups.get(**groupName);
      if (!found)
        return std::unexpected(std::move(found.error()));
      group = *found;
    }
    auto label = layer.optString("name");
    if (!label)
      return std::unexpected(std::move(label.error()));
    if (*label) {
      auto added = m_labelledPostProcessLayers.add(**label, m_postProcessLayers.count());
      if (!added)
        return std::unexpected(std::move(added.error()));
    }
    auto passes = layer.getUInt("passes", 1);
    if (!passes)
      return std::unexpected(std::move(passes.error()));
    m_postProcessLayers.append(
      PostProcessLayer{.effects = std::move(*effects), .passes = static_cast<unsigned>(*passes), .group = group});
  }
  return loadEffectConfig("interface");
}

std::expected<void, StarException> ClientApplication::setPostProcessLayerPasses(String const& layer, unsigned const& passes) {
  auto index = m_labelledPostProcessLayers.get(layer);
  if (!index)
    return std::unexpected(std::move(index.error()));
  auto found = m_postProcessLayers.at(index->get());
  if (!found)
    return std::unexpected(std::move(found.error()));
  found->get().passes = passes;
  return {};
}
std::expected<void, StarException> ClientApplication::setPostProcessGroupEnabled(String const& group, bool const& enabled, std::optional<bool> const& save) {
  auto found = m_postProcessGroups.get(group);
  if (!found)
    return std::unexpected(std::move(found.error()));
  found->get().enabled = enabled;
  if (save.value_or(false)) {
    auto config = m_root->configuration();
    if (!config)
      return std::unexpected(std::move(config.error()));
    return (*config)->setPath(strf("{}.{}.enabled", postProcessGroupsRoot, group), enabled);
  }
  return {};
}
std::expected<bool, StarException> ClientApplication::postProcessGroupEnabled(String const& group) {
  auto found = m_postProcessGroups.get(group);
  if (!found)
    return std::unexpected(std::move(found.error()));
  return found->get().enabled;
}

std::expected<Json, StarException> ClientApplication::postProcessGroups() {
  return m_root->assets().and_then([](auto assets) { return assets->json("/client.config:postProcessGroups"); });
}

unsigned ClientApplication::framesSkipped() const {
  return m_framesSkipped;
}

std::expected<void, StarException> ClientApplication::changeState(MainAppState newState) {
  auto oldState = m_state;
  // Publish before any phase: callbacks and recovery observe the requested state.
  m_state = newState;
  if (newState == MainAppState::Quit)
    appController()->quit();
  if (auto prepared = prepareStateCinematic(newState); !prepared)
    return prepared;
  if (auto tornDown = teardownTransition(oldState, newState); !tornDown)
    return tornDown;
  if (oldState < MainAppState::Title && newState >= MainAppState::Title) {
    if (auto prepared = prepareTitleServices(); !prepared)
      return prepared;
  }
  if (newState == MainAppState::Title)
    return prepareTitleForm();
  if (newState > MainAppState::Title)
    return startSession(newState);
  return {};
}

std::expected<void, StarException> ClientApplication::prepareStateCinematic(MainAppState newState) {
  if (newState == MainAppState::Mods || newState == MainAppState::Splash) {
    auto cinematic = m_root->assets().and_then([&](auto assets) {
      return assets->json(newState == MainAppState::Mods ? std::string_view("/cinematics/mods/modloading.cinematic") : std::string_view("/cinematics/splash.cinematic"));
    });
    if (!cinematic)
      return std::unexpected(std::move(cinematic.error()));
    if (auto loaded = m_cinematicOverlay->load(*cinematic); !loaded)
      return std::unexpected(std::move(loaded.error()));
    if (newState == MainAppState::Splash) {
      auto loader = Thread::invoke("Async root loader", [root = std::ref(*m_root)] { return root.get().fullyLoad(); });
      if (!loader)
        return std::unexpected(std::move(loader.error()));
      m_rootLoader = std::move(*loader);
    }
  }
  return {};
}

std::expected<void, StarException> ClientApplication::teardownTransition(MainAppState oldState, MainAppState newState) {
  auto& app = appController();
  // Teardown must finish even when disconnect, persistence or server shutdown fails.
  std::optional<StarException> cleanupError;
  auto remember = [&](auto result) {
    if (!result) {
      if (cleanupError)
        cleanupError = cleanupError->withContext(strf("Additional client cleanup failure: {}", outputException(ExceptionDiagnostic(result.error()), true)));
      else
        cleanupError = std::move(result.error());
    }
  };
  if (oldState > MainAppState::Title && newState <= MainAppState::Title) {
    if (m_universeClient)
      remember(m_universeClient->disconnect());
    if (m_universeServer) {
      remember(m_universeServer->close());
      m_universeServer.reset();
    }
    m_cinematicOverlay->stop();
    if (m_httpSession)
      m_httpSession->clearHttpTrustRequestCallback();
    if (m_player)
      m_player->setInput(std::nullopt);
    if (m_mainInterface)
      remember(m_mainInterface->close());
    m_mainInterface.reset();
    m_voice->clearSpeakers();
    if (auto service = app->p2pNetworkingService()) {
      remember(service->setJoinUnavailable());
      service->setAcceptingP2PConnections(false);
    }
  }
  if (oldState > MainAppState::Title && newState == MainAppState::Title) {
    remember(m_titleScreen->resetState());
    m_mainMixer->setUniverseClient({});
  }
  if (oldState >= MainAppState::Title && newState < MainAppState::Title) {
    m_playerStorage.reset();
    if (m_statistics) {
      remember(m_statistics->writeStatistics());
      m_statistics.reset();
    }
    if (m_httpSession)
      remember(m_httpSession->stop());
    m_universeClient.reset();
    m_httpSession.reset();
    m_mainMixer->setUniverseClient({});
    if (m_titleScreen)
      remember(m_titleScreen->close());
    m_titleScreen.reset();
  }
  if (cleanupError)
    return std::unexpected(std::move(*cleanupError));
  return {};
}

std::expected<void, StarException>
ClientApplication::installLuaCallbacks(String const& name, std::expected<LuaCallbacks, StarException> callbacks) {
  if (!callbacks)
    return std::unexpected(std::move(callbacks.error()));
  return m_universeClient->setLuaCallbacks(name, std::move(*callbacks));
}

std::expected<void, StarException> ClientApplication::prepareTitleServices() {
  auto& app = appController();
  if (m_rootLoader) {
    auto loaded = m_rootLoader.finish();
    if (!loaded)
      return std::unexpected(std::move(loaded.error()));
  }
  m_cinematicOverlay->stop();
  auto storage = PlayerStorage::create(*m_root, m_root->toStoragePath("player"));
  if (!storage)
    return std::unexpected(std::move(storage.error()));
  m_playerStorage = std::move(*storage);
  auto statistics = Statistics::create(*m_root, m_root->toStoragePath("player"), app->statisticsService());
  if (!statistics)
    return std::unexpected(std::move(statistics.error()));
  m_statistics = std::move(*statistics);
  auto client = UniverseClient::create(*m_root, m_playerStorage, m_statistics, m_root->toStoragePath("universeclient"));
  if (!client)
    return std::unexpected(std::move(client.error()));
  m_universeClient = std::move(*client);
  auto http = LuaHttpSession::create(*m_root);
  if (!http)
    return std::unexpected(std::move(http.error()));
  m_httpSession = std::move(*http);
  if (auto installed =
        installLuaCallbacks("universe", LuaBindings::makeUniverseClientCallbacks(*m_root, *m_universeClient));
      !installed)
    return installed;
  if (auto installed = installLuaCallbacks("input", LuaBindings::makeInputCallbacks(*m_input)); !installed)
    return installed;
  if (auto installed = installLuaCallbacks("voice", LuaBindings::makeVoiceCallbacks(*m_voice)); !installed)
    return installed;
  if (auto installed =
        installLuaCallbacks("camera", LuaBindings::makeCameraCallbacks(*m_root, m_worldPainter->camera()));
      !installed)
    return installed;
  if (auto installed = installLuaCallbacks("renderer", LuaBindings::makeRenderingCallbacks(*this)); !installed)
    return installed;
  auto config = m_root->configuration();
  if (!config)
    return std::unexpected(std::move(config.error()));
  auto alwaysAllow = (*config)->getPath("safe.alwaysAllowClipboard");
  if (!alwaysAllow)
    return std::unexpected(std::move(alwaysAllow.error()));
  bool allowClipboard = false;
  if (*alwaysAllow) {
    auto allowed = alwaysAllow->toBool();
    if (!allowed)
      return std::unexpected(std::move(allowed.error()));
    allowClipboard = *allowed;
  }
  if (auto installed = installLuaCallbacks(
        "clipboard", LuaBindings::makeClipboardCallbacks(*m_input, *m_root, m_root->logger(), app, allowClipboard));
      !installed)
    return installed;
  auto httpEnabled = (*config)->getPath("safe.luaHttp.enabled").and_then([](Json j) { return j.optBool(); });
  if (!httpEnabled)
    return std::unexpected(std::move(httpEnabled.error()));
  if (auto installed =
        installLuaCallbacks("http", LuaBindings::makeHttpCallbacks(m_httpSession, httpEnabled->value_or(false)));
      !installed)
    return installed;
  installPlayerReloadCallbacks();
  m_mainMixer->setUniverseClient(m_universeClient);
  auto title = TitleScreen::create(*m_guiContext, m_playerStorage, m_mainMixer->mixer(), m_universeClient);
  if (!title)
    return std::unexpected(std::move(title.error()));
  m_titleScreen = std::move(*title);
  if (auto renderer = Application::renderer()) {
    if (auto initialized = m_titleScreen->renderInit(renderer); !initialized)
      return initialized;
  }
  return {};
}

void ClientApplication::installPlayerReloadCallbacks() {
  auto heldScriptPanes = std::make_shared<List<MainInterface::ScriptPaneInfo>>();
  m_universeClient->playerReloadPreCallback() =
    [app = std::ref(*this), heldScriptPanes](bool resetInterface) -> std::expected<void, StarException> {
    if (!resetInterface)
      return {};
    return app.get().m_mainInterface->takeScriptPanes(*heldScriptPanes);
  };
  m_universeClient->playerReloadCallback() =
    [app = std::ref(*this), heldScriptPanes](bool resetInterface) -> std::expected<void, StarException> {
    auto inventory =
      app.get().m_mainInterface->paneManager().registeredPane<InventoryPane>(MainInterfacePanes::Inventory);
    if (!inventory)
      return std::unexpected(std::move(inventory.error()));
    if (*inventory) {
      auto cleared = (*inventory)->clearChangedSlots();
      if (!cleared)
        return cleared;
    }
    if (resetInterface) {
      auto revived = app.get().m_mainInterface->reviveScriptPanes(*heldScriptPanes);
      if (!revived)
        return revived;
      heldScriptPanes->clear();
    }
    return {};
  };
}

std::expected<void, StarException> ClientApplication::prepareTitleForm() {
  auto config = m_root->configuration();
  if (!config)
    return std::unexpected(std::move(config.error()));
  if (m_pendingMultiPlayerConnection
      && std::holds_alternative<P2PNetworkingPeerId>(m_pendingMultiPlayerConnection->server))
    return m_titleScreen->goToMultiPlayerSelectCharacter(true);
  auto address = (*config)->getPath("title.multiPlayerAddress").and_then([](Json j) { return j.toString(); });
  if (!address)
    return std::unexpected(std::move(address.error()));
  auto port = (*config)->getPath("title.multiPlayerPort").and_then([](Json j) { return j.toString(); });
  if (!port)
    return std::unexpected(std::move(port.error()));
  if (m_pendingMultiPlayerConnection) {
    auto const& pending = std::get<HostAddressWithPort>(m_pendingMultiPlayerConnection->server);
    *address = toString(pending.address());
    *port = toString(pending.port());
  }
  if (auto updated = m_titleScreen->setMultiPlayerAddress(*address); !updated)
    return updated;
  if (auto updated = m_titleScreen->setMultiPlayerPort(*port); !updated)
    return updated;
  auto account = (*config)->getPath("title.multiPlayerAccount").and_then([](Json j) { return j.toString(); });
  if (!account)
    return std::unexpected(std::move(account.error()));
  if (auto updated = m_titleScreen->setMultiPlayerAccount(*account); !updated)
    return updated;
  auto legacy = (*config)->getPath("title.multiPlayerForceLegacy").and_then([](Json j) { return j.optBool(); });
  if (!legacy)
    return std::unexpected(std::move(legacy.error()));
  if (auto updated = m_titleScreen->setMultiPlayerForceLegacy(legacy->value_or(false)); !updated)
    return updated;
  if (m_pendingMultiPlayerConnection)
    return m_titleScreen->goToMultiPlayerSelectCharacter(false);
  return {};
}

std::expected<void, StarException> ClientApplication::startSession(MainAppState newState) {
  auto activated = activateSessionPlayer();
  if (!activated)
    return std::unexpected(std::move(activated.error()));
  if (!*activated)
    return {};
  // Keep this generation lease through connection and interface/Lua startup.
  auto assets = m_root->assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  auto loading = (*assets)->json("/client.config:loadingCinematic").and_then([](Json j) { return j.toString(); });
  if (!loading)
    return std::unexpected(std::move(loading.error()));
  auto cinematic = (*assets)->json(*loading);
  if (!cinematic)
    return std::unexpected(std::move(cinematic.error()));
  if (auto loaded = m_cinematicOverlay->load(*cinematic); !loaded)
    return loaded;
  if (!m_player->log()->introComplete()) {
    auto intro = (*assets)->json("/client.config:introCinematic").and_then([](Json j) { return j.toString(); });
    if (!intro)
      return std::unexpected(std::move(intro.error()));
    if (auto pending =
          m_player->setPendingCinematic(Json(intro->replaceTags(StringMap<String>{{"species", m_player->species()}})));
        !pending)
      return pending;
  } else if (auto pending = m_player->setPendingCinematic(Json()); !pending) {
    return pending;
  }
  auto connected = newState == MainAppState::MultiPlayer ? connectMultiPlayer() : connectSinglePlayer();
  if (!connected)
    return std::unexpected(std::move(connected.error()));
  if (!*connected)
    return {};
  return prepareSessionInterface();
}

std::expected<bool, StarException> ClientApplication::activateSessionPlayer() {
  if (m_titleScreen->currentlySelectedPlayer()) {
    m_player = m_titleScreen->currentlySelectedPlayer();
  } else {
    if (auto uuid = m_playerStorage->playerUuidAt(0)) {
      auto player = m_playerStorage->loadPlayer(*uuid);
      if (!player)
        return std::unexpected(std::move(player.error()));
      m_player = std::move(*player);
    }
    if (!m_player)
      return setError("Error loading player!").transform([] { return false; });
  }
  m_player->setInput(optionalRef(m_input));
  m_mainMixer->setUniverseClient(m_universeClient);
  if (auto activated = m_universeClient->setMainPlayer(m_player); !activated)
    return std::unexpected(std::move(activated.error()));
  m_cinematicOverlay->setPlayer(m_player);
  m_timeSinceJoin = static_cast<std::int64_t>(Time::millisecondsSinceEpoch()) / 1000;
  return true;
}

std::expected<bool, StarException> ClientApplication::connectMultiPlayer() {
  auto pending = takeOptional(m_pendingMultiPlayerConnection);
  if (!pending)
    return std::unexpected(std::move(pending.error()));
  std::unique_ptr<PacketSocket> packetSocket;
  auto opened = std::visit(
    [&]<typename Server>(Server const& server) -> std::expected<bool, StarException> {
      if constexpr (std::same_as<Server, HostAddressWithPort>) {
        auto socket = TcpSocket::connectTo(server).and_then(
          [&](auto tcp) { return TcpPacketSocket::open(m_root->logger(), std::move(tcp)); });
        if (!socket)
          return setError(strf("Join failed! Error connecting to '{}'", server), ExceptionDiagnostic(socket.error()))
            .transform([] { return false; });
        packetSocket = std::move(*socket);
      } else if constexpr (std::same_as<Server, P2PNetworkingPeerId>) {
        if (auto service = appController()->p2pNetworkingService()) {
          auto peer = service->connectToPeer(server);
          if (!peer)
            return setError("Cannot join peer", ExceptionDiagnostic(peer.error())).transform([] { return false; });
          auto socket = P2PPacketSocket::open(m_root->logger(), std::move(*peer));
          if (!socket)
            return setError("Cannot join peer", ExceptionDiagnostic(socket.error())).transform([] { return false; });
          packetSocket = std::move(*socket);
        } else {
          return setError("Internal error, no p2p networking service when joining p2p networking peer").transform([] {
            return false;
          });
        }
      } else {
        static_assert(std::same_as<Server, void>, "Unhandled multiplayer transport");
      }
      return true;
    },
    pending->server);
  if (!opened)
    return opened;
  if (!*opened)
    return false;
  auto mismatch = m_root->configuration()
                    .and_then([](auto config) { return config->get("allowAssetsMismatch"); })
                    .and_then([](Json json) { return json.toBool(); });
  if (!mismatch)
    return std::unexpected(std::move(mismatch.error()));
  auto connected = m_universeClient->connect(UniverseConnection(std::move(packetSocket)), *mismatch, pending->account,
                                             pending->password, pending->forceLegacy);
  if (!connected)
    return setError("Join failed", ExceptionDiagnostic(connected.error())).transform([] { return false; });
  if (*connected)
    return setError(**connected).transform([] { return false; });
  if (std::holds_alternative<HostAddressWithPort>(pending->server))
    m_currentRemoteJoin = std::get<HostAddressWithPort>(pending->server);
  else
    m_currentRemoteJoin.reset();
  return true;
}

std::expected<bool, StarException> ClientApplication::connectSinglePlayer() {
  if (!m_universeServer) {
    auto server = UniverseServer::create(*m_root, m_root->toStoragePath("universe"), true);
    if (!server)
      return setError("Unable to start local server", ExceptionDiagnostic(server.error())).transform([] {
        return false;
      });
    m_universeServer = std::move(*server);
    auto started = m_universeServer->start();
    if (!started)
      return setError("Unable to start local server", ExceptionDiagnostic(started.error())).transform([] {
        return false;
      });
  }
  auto local = m_universeServer->addLocalClient();
  if (!local)
    return setError("Error connecting locally", ExceptionDiagnostic(local.error())).transform([] { return false; });
  auto connected = m_universeClient->connect(std::move(*local), "", "");
  if (!connected)
    return setError("Error connecting locally", ExceptionDiagnostic(connected.error())).transform([] { return false; });
  if (*connected)
    return setError(strf("Error connecting locally: {}", **connected)).transform([] { return false; });
  return true;
}

std::expected<void, StarException> ClientApplication::prepareSessionInterface() {
  m_titleScreen->stopMusic();
  if (auto restarted = m_universeClient->restartLua(); !restarted)
    return restarted;
  auto createdInterface = MainInterface::create(*m_guiContext, m_universeClient, m_worldPainter, m_cinematicOverlay);
  if (!createdInterface)
    return std::unexpected(std::move(createdInterface.error()));
  m_mainInterface = std::move(*createdInterface);
  if (auto installed = installLuaCallbacks("interface", LuaBindings::makeInterfaceCallbacks(*m_mainInterface));
      !installed)
    return installed;
  if (auto installed = installLuaCallbacks("chat", LuaBindings::makeChatCallbacks(*m_mainInterface, *m_universeClient));
      !installed)
    return installed;
  if (auto installed =
        installLuaCallbacks("celestial", LuaBindings::makeCelestialCallbacks(*m_root, *m_universeClient));
      !installed)
    return installed;
  if (auto installed =
        installLuaCallbacks("team", LuaBindings::makeTeamClientCallbacks(*m_universeClient->teamClient()));
      !installed)
    return installed;
  if (auto installed =
        installLuaCallbacks("world", LuaBindings::makeWorldCallbacks(*m_root, *m_universeClient->worldClient()));
      !installed)
    return installed;
  m_httpSession->setHttpTrustRequestCallback(
    [session = std::weak_ptr<LuaHttpSession>(m_httpSession),
     mainInterface = std::ref(*m_mainInterface)](String const& domain) -> std::expected<void, StarException> {
      auto& manager = mainInterface.get().paneManager();
      auto dialog = manager.registeredPane<HttpTrustDialog>(MainInterfacePanes::HttpTrustDialog);
      if (!dialog)
        return std::unexpected(std::move(dialog.error()));
      auto displayed = (*dialog)->displayRequest(
        domain, [session, domain](HttpTrustReply reply, bool) -> std::expected<void, StarException> {
          if (auto owner = session.lock())
            return owner->handleHttpTrustReply(domain, reply == HttpTrustReply::Allow);
          return {};
        });
      if (!displayed)
        return std::unexpected(std::move(displayed.error()));
      return manager.displayRegisteredPane(MainInterfacePanes::HttpTrustDialog).transform([](bool) {});
    });
  if (auto started = m_universeClient->startLuaScripts(); !started)
    return started;
  m_mainMixer->setWorldPainter(optionalRef(m_worldPainter));
  if (auto renderer = Application::renderer()) {
    if (auto initialized = m_worldPainter->renderInit(renderer); !initialized)
      return initialized;
    m_guiContext->setTileDrawer(m_worldPainter->tileDrawer());
  }
  return {};
}

std::expected<void, StarException> ClientApplication::recoverToTitle(std::expected<void, StarException> logged,
                                                                     std::expected<void, StarException> displayed) {
  // Always release live world/server resources, even if logging or the pane failed.
  auto reset = m_titleScreen->resetState();
  auto changed = changeState(MainAppState::Title);
  std::optional<StarException> failure;
  for (auto const& reference : std::array{std::ref(logged), std::ref(displayed), std::ref(reset), std::ref(changed)}) {
    auto& result = reference.get();
    if (!result) {
      if (failure)
        failure = failure->withContext(strf("Additional client recovery failure: {}", outputException(ExceptionDiagnostic(result.error()), true)));
      else
        failure = std::move(result.error());
    }
  }
  if (failure)
    return std::unexpected(std::move(*failure));
  return {};
}

std::expected<void, StarException> ClientApplication::setError(String const& error) {
  auto logged = m_root->logger().error("{}", error);
  auto displayed = m_errorScreen->setMessage(error);
  auto recovered = recoverToTitle(std::move(logged), std::move(displayed));
  if (!recovered)
    return std::unexpected(recovered.error().withContext(error.utf8()));
  return recovered;
}

std::expected<void, StarException> ClientApplication::setError(String const& error, ExceptionDiagnostic const& diagnostic) {
  auto logged = m_root->logger().error("{}\n{}", error, outputException(diagnostic, true));
  auto displayed = m_errorScreen->setMessage(strf("{}\n{}", error, outputException(diagnostic, false)));
  auto recovered = recoverToTitle(std::move(logged), std::move(displayed));
  if (!recovered)
    return std::unexpected(recovered.error().withContext(strf("{}\n{}", error, outputException(diagnostic, true))));
  return recovered;
}

std::expected<void, StarException> ClientApplication::loadMods() {
  auto service = appController()->userGeneratedContentService();
  auto include = m_root->configuration().and_then([&](auto config) { return config->get("includeUGC", m_root->settings().includeUGC); })
      .and_then([](Json j) { return j.toBool(); });
  if (!include)
    return std::unexpected(std::move(include.error()));
  if (!service || !*include)
    return {};
  StringList directories;
  if (auto logged = m_root->logger().info("Checking for user generated content..."); !logged)
    return std::unexpected(std::move(logged.error()));
  for (auto& id : service->subscribedContentIds()) {
    auto directory = service->contentDownloadDirectory(id);
    if (!directory) {
      if (auto logged = m_root->logger().warn("User generated content with id '{}' is not available: {}", id, outputException(ExceptionDiagnostic(directory.error()), true)); !logged)
        return std::unexpected(std::move(logged.error()));
      continue;
    }
    if (*directory) {
      if (auto logged = m_root->logger().info("Loading mods from user generated content with id '{}' from directory '{}'", id, **directory); !logged)
        return std::unexpected(std::move(logged.error()));
      directories.append(**directory);
    } else {
      if (auto logged = m_root->logger().warn("User generated content with id '{}' is not available", id); !logged)
        return std::unexpected(std::move(logged.error()));
    }
  }
  if (directories.empty())
    return m_root->logger().info("No subscribed user generated content");
  if (auto loaded = m_root->loadMods(directories, false); !loaded)
    return std::unexpected(std::move(loaded.error()));
  auto assets = m_root->assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  return {};
}

std::expected<void, StarException> ClientApplication::updateSteamFlatpakWarning(float) {
  if (m_errorScreen->accepted())
    return changeState(MainAppState::Mods);
  return {};
}

std::expected<void, StarException> ClientApplication::updateMods(float dt) {
  m_cinematicOverlay->update(dt);
  auto service = appController()->userGeneratedContentService();
  auto config = m_root->configuration();
  if (!config)
    return std::unexpected(std::move(config.error()));
  auto include = (*config)->get("includeUGC", m_root->settings().includeUGC).and_then([](Json j) { return j.toBool(); });
  if (!include)
    return std::unexpected(std::move(include.error()));
  if (!service || !*include)
    return changeState(MainAppState::Splash);
  if (!m_loggedUGCCheck) {
    if (auto logged = m_root->logger().info("Checking for user generated content updates..."); !logged)
      return std::unexpected(std::move(logged.error()));
    m_loggedUGCCheck = true;
  }
  auto download = service->triggerContentDownload();
  if (download && *download == UserGeneratedContentService::UGCState::NoDownload)
    return changeState(MainAppState::Splash);
  if (download)
    download = service->triggerContentDownload();
  if (download && *download != UserGeneratedContentService::UGCState::Finished)
    return {};
  const bool downloadFailed = !download;
  if (downloadFailed) {
    // Preserve installed/local-mod recovery without retrying or calling a failed download successful.
    if (auto logged = m_root->logger().warn("Could not update subscribed user generated content; loading available installed content: {}", outputException(ExceptionDiagnostic(download.error()), true)); !logged)
      return std::unexpected(std::move(logged.error()));
  }
  if (auto logged = m_root->logger().info("{}", downloadFailed ? "Loading available installed user generated content..." : "Loading updated user generated content..."); !logged)
    return std::unexpected(std::move(logged.error()));
  StringList directories;
  for (auto& id : service->subscribedContentIds()) {
    auto directory = service->contentDownloadDirectory(id);
    if (!directory) {
      if (auto logged = m_root->logger().warn("User generated content with id '{}' is not available: {}", id, outputException(ExceptionDiagnostic(directory.error()), true)); !logged)
        return std::unexpected(std::move(logged.error()));
      continue;
    }
    if (*directory) {
      if (auto logged = m_root->logger().info("Loading mods from user generated content with id '{}' from directory '{}'", id, **directory); !logged)
        return std::unexpected(std::move(logged.error()));
      directories.append(**directory);
    } else {
      if (auto logged = m_root->logger().warn("User generated content with id '{}' is not available", id); !logged)
        return std::unexpected(std::move(logged.error()));
    }
  }
  if (directories.empty()) {
    if (auto changed = changeState(MainAppState::Splash); !changed)
      return std::unexpected(std::move(changed.error()));
  } else {
    if (auto logged = m_root->logger().info("{}", downloadFailed ? "Reloading to include available installed user generated content" : "Reloading to include updated user generated content"); !logged)
      return std::unexpected(std::move(logged.error()));
    if (auto loaded = m_root->loadMods(directories); !loaded)
      return std::unexpected(std::move(loaded.error()));
    // Reload replaces Configuration; write the warning acknowledgement to the new owner.
    config = m_root->configuration();
    if (!config)
      return std::unexpected(std::move(config.error()));
  }
  auto assets = m_root->assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  auto shown = (*config)->get("modsWarningShown").and_then([](Json j) { return j.optBool(); });
  if (!shown)
    return std::unexpected(std::move(shown.error()));
  if (shown->value_or(false))
    return changeState(MainAppState::Splash);
  if (auto saved = (*config)->set("modsWarningShown", true); !saved)
    return std::unexpected(std::move(saved.error()));
  auto message = (*assets)->json("/interface.config:modsWarningMessage").and_then([](Json j) { return j.toString(); });
  if (!message)
    return std::unexpected(std::move(message.error()));
  if (auto displayed = m_errorScreen->setMessage(*message); !displayed)
    return std::unexpected(std::move(displayed.error()));
  return changeState(MainAppState::ModsWarning);
}

std::expected<void, StarException> ClientApplication::updateModsWarning(float) {
  if (m_errorScreen->accepted())
    return changeState(MainAppState::Splash);
  return {};
}

std::expected<void, StarException> ClientApplication::updateSplash(float dt) {
  m_cinematicOverlay->update(dt);
  if (!m_rootLoader.isRunning() && (m_cinematicOverlay->completable() || m_cinematicOverlay->completed()))
    return changeState(MainAppState::Title);
  return {};
}

std::expected<void, StarException> ClientApplication::updateError(float) {
  if (m_errorScreen->accepted())
    return changeState(MainAppState::Title);
  return {};
}

std::expected<void, StarException> ClientApplication::updateTitle(float dt) {
  m_cinematicOverlay->update(dt);

  if (auto updated = m_titleScreen->update(dt); !updated)
    return std::unexpected(std::move(updated.error()));
  if (auto mixed = m_mainMixer->update(dt); !mixed)
    return std::unexpected(std::move(mixed.error()));
  m_mainMixer->setSpeed(m_root->timing().timescale);

  auto& app = appController();
  auto active = m_titleScreen->textInputActive();
  if (!active)
    return std::unexpected(std::move(active.error()));
  bool inputActive = *active;
  m_input->setTextInputActive(inputActive);
  if (inputActive) {
    auto widget = m_titleScreen->paneManager().keyboardCapturedWidget();
    if (!widget)
      return std::unexpected(std::move(widget.error()));
    auto area = (*widget)->keyboardCaptureArea();
    if (!area)
      return std::unexpected(std::move(area.error()));
    app->setTextArea(*area);
  } else {
    app->setTextArea();
  }
  app->setAcceptingTextInput(inputActive);

  auto p2pNetworkingService = app->p2pNetworkingService();
  if (p2pNetworkingService) {
    auto getStateString = [](TitleState state) -> std::string_view {
      switch (state) {
        case TitleState::Main:
          return "In Main Menu";
        case TitleState::Options:
          return "In Options";
        case TitleState::Mods:
          return "In Mods";
        case TitleState::SinglePlayerSelectCharacter:
          return "Selecting a character for singleplayer";
        case TitleState::SinglePlayerCreateCharacter:
          return "Creating a character for singleplayer";
        case TitleState::MultiPlayerSelectCharacter:
          return "Selecting a character for multiplayer";
        case TitleState::MultiPlayerCreateCharacter:
          return "Creating a character for multiplayer";
        case TitleState::MultiPlayerConnect:
          return "Awaiting multiplayer connection info";
        case TitleState::StartSinglePlayer:
          return "Loading Singleplayer";
        case TitleState::StartMultiPlayer:
          return "Connecting to Multiplayer";
        default:
          return "";
      }
    };

    if (auto activity = p2pNetworkingService->setActivityData("Not In Game", getStateString(m_titleScreen->currentState()), 0, {}); !activity)
      return std::unexpected(std::move(activity.error()));
  }

  if (m_titleScreen->currentState() == TitleState::StartSinglePlayer) {
    return changeState(MainAppState::SinglePlayer);

  } else if (m_titleScreen->currentState() == TitleState::StartMultiPlayer) {
    if (!m_pendingMultiPlayerConnection || std::holds_alternative<HostAddressWithPort>(m_pendingMultiPlayerConnection->server)) {
      auto addressString = m_titleScreen->multiPlayerAddress().trim();
      if (!addressString)
        return std::unexpected(std::move(addressString.error()));
      auto portString = m_titleScreen->multiPlayerPort().trim();
      if (!portString)
        return std::unexpected(std::move(portString.error()));
      auto config = m_root->configuration();
      if (!config)
        return std::unexpected(std::move(config.error()));
      if (portString->empty()) {
        auto defaultPort = (*config)->get("gameServerPort").and_then([](Json j) { return j.toUInt(); });
        if (!defaultPort)
          return std::unexpected(std::move(defaultPort.error()));
        *portString = toString(*defaultPort);
      }
      auto port = maybeLexicalCast<std::uint16_t>(*portString);
      if (!port)
        return setError(strf("invalid port: {}", *portString));
      auto address = HostAddressWithPort::lookup(*addressString, *port);
      if (!address)
        return setError("Error resolving server address", ExceptionDiagnostic(address.error()));
      m_pendingMultiPlayerConnection =
        PendingMultiPlayerConnection{.server = *address,
                                     .account = m_titleScreen->multiPlayerAccount(),
                                     .password = m_titleScreen->multiPlayerPassword(),
                                     .forceLegacy = m_titleScreen->multiPlayerForceLegacy()};
      if (auto saved = (*config)->setPath("title.multiPlayerAddress", m_titleScreen->multiPlayerAddress()); !saved)
        return std::unexpected(std::move(saved.error()));
      if (auto saved = (*config)->setPath("title.multiPlayerPort", m_titleScreen->multiPlayerPort()); !saved)
        return std::unexpected(std::move(saved.error()));
      if (auto saved = (*config)->setPath("title.multiPlayerAccount", m_titleScreen->multiPlayerAccount()); !saved)
        return std::unexpected(std::move(saved.error()));
      if (auto saved = (*config)->setPath("title.multiPlayerForceLegacy", m_titleScreen->multiPlayerForceLegacy()); !saved)
        return std::unexpected(std::move(saved.error()));
    }
    return changeState(MainAppState::MultiPlayer);
  } else if (m_titleScreen->currentState() == TitleState::Quit) {
    return changeState(MainAppState::Quit);
  }
  return {};
}

std::expected<void, StarException> ClientApplication::updateRunning(float dt) {
    auto& app = appController();
    auto worldClient = m_universeClient->worldClient();
    auto p2pNetworkingService = app->p2pNetworkingService();
    auto configResult = m_root->configuration();
    if (!configResult)
      return std::unexpected(std::move(configResult.error()));
    auto config = *configResult;
    auto clientIPJoinable = config->get("clientIPJoinable").and_then([](Json j) { return j.toBool(); });
    if (!clientIPJoinable)
      return std::unexpected(std::move(clientIPJoinable.error()));
    auto clientP2PJoinable = config->get("clientP2PJoinable").and_then([](Json j) { return j.toBool(); });
    if (!clientP2PJoinable)
      return std::unexpected(std::move(clientP2PJoinable.error()));
    std::optional<std::pair<std::uint16_t, std::uint16_t>> party = std::make_pair(m_universeClient->players(), m_universeClient->maxPlayers());

    if (m_state == MainAppState::MultiPlayer) {
      if (p2pNetworkingService) {
        p2pNetworkingService->setAcceptingP2PConnections(false);
        auto join = *clientP2PJoinable && m_currentRemoteJoin
            ? p2pNetworkingService->setJoinRemote(*m_currentRemoteJoin)
            : p2pNetworkingService->setJoinUnavailable();
        if (!join)
          return std::unexpected(std::move(join.error()));
      }
    } else {
      m_universeServer->setListeningTcp(*clientIPJoinable);
      if (p2pNetworkingService) {
        p2pNetworkingService->setAcceptingP2PConnections(*clientP2PJoinable);
        if (*clientP2PJoinable) {
          if (auto join = p2pNetworkingService->setJoinLocal(m_universeServer->maxClients()); !join)
            return std::unexpected(std::move(join.error()));
        } else {
          if (auto join = p2pNetworkingService->setJoinUnavailable(); !join)
            return std::unexpected(std::move(join.error()));
          party = {};
        }
      }
    }
    
    if (p2pNetworkingService) {
      auto getActivityDetail = [&](String const& tag) -> std::expected<String, StarException> {
        if (tag == "playerName")
          return m_player->name().transform([](String name) { return Text::stripEscapeCodes(name); });
        if (tag == "playerHealth")
          return toString(m_player->health());
        if (tag == "playerMaxHealth")
          return m_player->maxHealth().transform([](float value) { return toString(value); });
        if (tag == "playerEnergy")
          return m_player->energy().transform([](float value) { return toString(value); });
        if (tag == "playerMaxEnergy")
          return m_player->maxEnergy().transform([](float value) { return toString(value); });
        if (tag == "playerBreath")
          return toString(m_player->breath());
        if (tag == "playerMaxBreath")
          return m_player->maxBreath().transform([](float value) { return toString(value); });
        if (tag == "playerXPos")
          return toString(std::round(m_player->position().x()));
        if (tag == "playerYPos")
          return toString(std::round(m_player->position().y()));
        if (tag == "worldName") {
          if (std::holds_alternative<ClientShipWorldId>(m_universeClient->clientContext()->playerWorldId()))
            return "Player Ship";
          auto worldTemplate = worldClient ? worldClient->currentTemplate() : nullptr;
          if (!worldTemplate)
            return "Nowhere";
          auto worldName = worldTemplate->worldName();
          if (worldName.empty())
            return "In World";
          return Text::stripEscapeCodes(worldName);
        }
        return "";
      };

      String finalDetails = "";
      auto activityDetails = config->getPath("discord.activityDetails");
      if (!activityDetails)
        return std::unexpected(std::move(activityDetails.error()));
      if (activityDetails->isType(Json::Type::Array)) {
        auto details = activityDetails->iterateArray();
        if (!details)
          return std::unexpected(std::move(details.error()));
        StringList detailsList;
        for (auto& detail : *details) {
          auto tag = detail.toString();
          if (!tag)
            return std::unexpected(std::move(tag.error()));
          auto text = getActivityDetail(*tag);
          if (!text)
            return std::unexpected(std::move(text.error()));
          detailsList.append(std::move(*text));
        }
        finalDetails = detailsList.join("\n");
      } else if (activityDetails->isType(Json::Type::String)) {
        auto text = activityDetails->toString();
        if (!text)
          return std::unexpected(std::move(text.error()));
        auto details = text->lookupTags(getActivityDetail);
        if (!details)
          return std::unexpected(std::move(details.error()));
        finalDetails = std::move(*details);
      }
      if (auto activity = p2pNetworkingService->setActivityData("In Game", finalDetails.utf8(), m_timeSinceJoin, party); !activity)
        return std::unexpected(std::move(activity.error()));
    }

    auto focused = m_mainInterface->inputFocus();
    if (!focused)
      return std::unexpected(std::move(focused.error()));
    if (!*focused && !m_cinematicOverlay->suppressInput()) {
      // Events and bindings are stable during this gameplay-input interval.
      // Resolve each event once, after UI focus/consumption and any rebinding,
      // and discard both snapshots before the next input/update dispatch.
      auto resolveActions = [&](List<KeyDownEvent> const& events) {
        std::bitset<std::to_underlying(InterfaceAction::Count)> actions;
        for (auto const& event : events) {
          for (auto action : m_guiContext->actions(event))
            actions[std::to_underlying(action)] = true;
        }
        return actions;
      };
      auto heldActions = resolveActions(m_heldKeyEvents);
      auto edgeActions = resolveActions(m_edgeKeyEvents);
      auto isActionTaken = [&](InterfaceAction action) {
        return heldActions[std::to_underlying(action)];
      };
      auto isActionTakenEdge = [&](InterfaceAction action) {
        return edgeActions[std::to_underlying(action)];
      };

      m_player->setShifting(isActionTaken(InterfaceAction::PlayerShifting));

      if (isActionTaken(InterfaceAction::PlayerRight))
        m_player->moveRight();
      if (isActionTaken(InterfaceAction::PlayerLeft))
        m_player->moveLeft();
      if (isActionTaken(InterfaceAction::PlayerUp))
        m_player->moveUp();
      if (isActionTaken(InterfaceAction::PlayerDown))
        m_player->moveDown();
      if (isActionTaken(InterfaceAction::PlayerJump))
        m_player->jump();

      if (isActionTaken(InterfaceAction::PlayerTechAction1)) {
        if (auto special = m_player->special(1); !special)
          return std::unexpected(std::move(special.error()));
      }
      if (isActionTaken(InterfaceAction::PlayerTechAction2)) {
        if (auto special = m_player->special(2); !special)
          return std::unexpected(std::move(special.error()));
      }
      if (isActionTaken(InterfaceAction::PlayerTechAction3)) {
        if (auto special = m_player->special(3); !special)
          return std::unexpected(std::move(special.error()));
      }

      if (isActionTakenEdge(InterfaceAction::PlayerInteract))
        m_player->beginTrigger();
      else if (!isActionTaken(InterfaceAction::PlayerInteract))
        m_player->endTrigger();

      if (isActionTakenEdge(InterfaceAction::PlayerDropItem)) {
        if (auto dropped = m_player->dropItem(); !dropped)
          return std::unexpected(std::move(dropped.error()));
      }

      if (isActionTakenEdge(InterfaceAction::EmoteBlabbering))
        m_player->addEmote(HumanoidEmote::Blabbering);
      if (isActionTakenEdge(InterfaceAction::EmoteShouting))
        m_player->addEmote(HumanoidEmote::Shouting);
      if (isActionTakenEdge(InterfaceAction::EmoteHappy))
        m_player->addEmote(HumanoidEmote::Happy);
      if (isActionTakenEdge(InterfaceAction::EmoteSad))
        m_player->addEmote(HumanoidEmote::Sad);
      if (isActionTakenEdge(InterfaceAction::EmoteNeutral))
        m_player->addEmote(HumanoidEmote::NEUTRAL);
      if (isActionTakenEdge(InterfaceAction::EmoteLaugh))
        m_player->addEmote(HumanoidEmote::Laugh);
      if (isActionTakenEdge(InterfaceAction::EmoteAnnoyed))
        m_player->addEmote(HumanoidEmote::Annoyed);
      if (isActionTakenEdge(InterfaceAction::EmoteOh))
        m_player->addEmote(HumanoidEmote::Oh);
      if (isActionTakenEdge(InterfaceAction::EmoteOooh))
        m_player->addEmote(HumanoidEmote::OOOH);
      if (isActionTakenEdge(InterfaceAction::EmoteBlink))
        m_player->addEmote(HumanoidEmote::Blink);
      if (isActionTakenEdge(InterfaceAction::EmoteWink))
        m_player->addEmote(HumanoidEmote::Wink);
      if (isActionTakenEdge(InterfaceAction::EmoteEat))
        m_player->addEmote(HumanoidEmote::Eat);
      if (isActionTakenEdge(InterfaceAction::EmoteSleep))
        m_player->addEmote(HumanoidEmote::Sleep);

      if (int newZoomDirection = static_cast<int>(m_input->bindHeld("opensb", "zoomIn"))
            - static_cast<int>(m_input->bindHeld("opensb", "zoomOut")))
        m_cameraZoomDirection = newZoomDirection;
    }
    if (m_cameraZoomDirection != 0) {
      const float threshold = 0.01f;
      bool goingIn = m_cameraZoomDirection == 1;
      auto zoom = config->get("zoomLevel").and_then([](Json j) { return j.toFloat(); });
      if (!zoom)
        return std::unexpected(std::move(zoom.error()));
      float curZoom = *zoom,
            newZoom = std::max(
              1.f,
              curZoom * std::powf(1.f + static_cast<float>(m_cameraZoomDirection) * 0.5f, std::min(1.f, dt * 5.f))),
            intZoom = std::max(1.f, (goingIn ? std::floor(curZoom) : std::ceil(curZoom)) + m_cameraZoomDirection);
      bool pastInt = goingIn ? newZoom + threshold > intZoom
                             : newZoom - threshold < intZoom;
      if (pastInt) {
        float intNewZoom = goingIn ? std::ceil(newZoom) : std::floor(newZoom);
        newZoom = std::lerp(intZoom, intNewZoom, std::clamp(std::abs(intZoom - newZoom) - 1.f, 0.f, 1.f));
        m_cameraZoomDirection = 0;
      }
      if (auto saved = config->set("zoomLevel", std::min(1000000.f, newZoom)); !saved)
        return std::unexpected(std::move(saved.error()));
    }

    if (m_controllerInput && m_controllerLeftStick.magnitudeSquared() > 0.01f)
      m_player->setMoveVector(m_controllerLeftStick);
    else
      m_player->setMoveVector(Vec2F());

    m_voice->setInput(m_input->bindHeld("opensb", "pushToTalk"));
    DataStreamBuffer voiceData;
    voiceData.setByteOrder(ByteOrder::LittleEndian);
    auto voice = m_voice->send(voiceData, 5000);
    if (!voice)
      return std::unexpected(std::move(voice.error()));
    bool needstoSendVoice = *voice;

    auto checkDisconnection = [app = std::ref(*this)]() -> std::expected<bool, StarException> {
      if (!app.get().m_universeClient->isConnected()) {
        app.get().m_cinematicOverlay->stop();
        String errMessage;
        if (auto disconnectReason = app.get().m_universeClient->disconnectReason())
          errMessage = strf("You were disconnected from the server for the following reason:\n{}", *disconnectReason);
        else
          errMessage = "Client-server connection no longer valid!";
        if (auto displayed = app.get().setError(errMessage); !displayed)
          return std::unexpected(std::move(displayed.error()));
        if (auto changed = app.get().changeState(MainAppState::Title); !changed)
          return std::unexpected(std::move(changed.error()));
        return true;
      }

      return false;
    };

    auto disconnected = checkDisconnection();
    if (!disconnected)
      return std::unexpected(std::move(disconnected.error()));
    if (*disconnected)
      return {};
    if (auto updated = m_mainInterface->preUpdate(dt); !updated)
      return std::unexpected(std::move(updated.error()));
    if (auto updated = m_universeClient->update(dt); !updated)
      return std::unexpected(std::move(updated.error()));
    disconnected = checkDisconnection();
    if (!disconnected)
      return std::unexpected(std::move(disconnected.error()));
    if (*disconnected)
      return {};

    if (worldClient) {
      m_worldPainter->update(dt);
      auto& broadcastCallback = worldClient->broadcastCallback();
      if (!broadcastCallback) {
        broadcastCallback = [&](std::shared_ptr<Player> player, StringView broadcast) -> std::expected<bool, StarException> {
          auto& view = broadcast.utf8();
          if (view.rfind(VoiceBroadcastPrefix.utf8(), 0) != NPos) {
            auto entityId = player->entityId();
            auto speaker = m_voice->speaker(connectionForEntity(entityId));
            if (!speaker)
              return std::unexpected(std::move(speaker.error()));
            (*speaker)->entityId = entityId;
            auto name = player->name();
            if (!name)
              return std::unexpected(std::move(name.error()));
            (*speaker)->name = std::move(*name);
            auto mouth = player->mouthPosition();
            if (!mouth)
              return std::unexpected(std::move(mouth.error()));
            (*speaker)->position = *mouth;
            auto received = m_voice->receive(*speaker, view.substr(VoiceBroadcastPrefix.utf8Size()));
            if (!received)
              return std::unexpected(std::move(received.error()));
          }
          return true;
        };
      }

      if (worldClient->inWorld()) {
        if (needstoSendVoice) {
          auto signature = m_root->identity().sign(voiceData.span());
          std::string broadcast("data\0voice\0", sizeof("data\0voice\0") - 1);
          broadcast.reserve((sizeof("data\0voice\0") - 1) + signature.size() + voiceData.size());
          broadcast.append(reinterpret_cast<char const*>(signature.data()), signature.size());
          auto voiceBytes = voiceData.span();
          broadcast.append(voiceBytes.begin(), voiceBytes.end());
          worldClient->sendSecretBroadcast(broadcast, true, false); // Packet queueing is nonfallible.
        }
        if (auto mainPlayer = m_universeClient->mainPlayer()) {
          auto localSpeaker = m_voice->localSpeaker();
          localSpeaker->position = mainPlayer->position();
          localSpeaker->entityId = mainPlayer->entityId();
          auto name = mainPlayer->name();
          if (!name)
            return std::unexpected(std::move(name.error()));
          localSpeaker->name = std::move(*name);
        }
        m_voice->setLocalSpeaker(worldClient->connection());
      }
      worldClient->setInteractiveHighlightMode(isHeldActionTaken(InterfaceAction::ShowLabels));
    }
    if (auto camera = updateCamera(dt); !camera)
      return std::unexpected(std::move(camera.error()));

    m_cinematicOverlay->update(dt);
    if (auto updated = m_mainInterface->update(dt); !updated)
      return std::unexpected(std::move(updated.error()));
    if (auto mixed = m_mainMixer->update(dt, m_cinematicOverlay->muteSfx(), m_cinematicOverlay->muteMusic()); !mixed)
      return std::unexpected(std::move(mixed.error()));
    m_mainMixer->setSpeed(m_root->timing().timescale);

    auto active = m_mainInterface->textInputActive();
    if (!active)
      return std::unexpected(std::move(active.error()));
    bool inputActive = *active;
    m_input->setTextInputActive(inputActive);
    if (inputActive) {
      auto widget = m_mainInterface->paneManager().keyboardCapturedWidget();
      if (!widget)
        return std::unexpected(std::move(widget.error()));
      auto area = (*widget)->keyboardCaptureArea();
      if (!area)
        return std::unexpected(std::move(area.error()));
      app->setTextArea(*area);
    } else {
      app->setTextArea();
    }
    app->setAcceptingTextInput(inputActive);

    auto interactions = m_player->pullInteractActions();
    if (!interactions)
      return std::unexpected(std::move(interactions.error()));
    for (auto const& interactAction : *interactions) {
      if (auto handled = m_mainInterface->handleInteractAction(interactAction); !handled)
        return std::unexpected(std::move(handled.error()));
    }

    if (m_universeServer) {
      if (auto p2pNetworkingService = app->p2pNetworkingService()) {
        for (auto& p2pClient : p2pNetworkingService->acceptP2PConnections()) {
          auto socket = P2PPacketSocket::open(m_root->logger(), std::move(p2pClient));
          if (!socket)
            return std::unexpected(std::move(socket.error()));
          if (auto added = m_universeServer->addClient(UniverseConnection(std::move(*socket))); !added)
            return std::unexpected(std::move(added.error()));
        }
      }

      auto paused = m_mainInterface->pauseRequested();
      if (!paused)
        return std::unexpected(std::move(paused.error()));
      if (auto pause = m_universeServer->setPause(*paused); !pause)
        return std::unexpected(std::move(pause.error()));
    }

    auto aimResult = m_player->aimPosition();
    if (!aimResult)
      return std::unexpected(std::move(aimResult.error()));
    Vec2F aimPosition = *aimResult;
    float fps = app->renderFps();
    m_root->logMap().set("client_render_rate", strf("{:4.2f} FPS ({:4.2f}ms)", fps, (1.0f / app->renderFps()) * 1000.0f));
    m_root->logMap().set("client_update_rate", strf("{:4.2f}Hz", app->updateRate()));
    if (m_mainInterface->isDebugDisplayed()) {
      auto const& frame = frameTimings();
      m_root->logMap().set("client_frame_previous",
                           strf("{}us; events={}us; updates={}us/{}; throttle={}us", frame.total, frame.events,
                                frame.updates, frame.updateCount, frame.throttle));
      m_root->logMap().set("client_frame_submission",
                           strf("render={}us; finish={}us; ImGui={}us; present={}us (CPU, not GPU)", frame.render,
                                frame.finish, frame.gui, frame.present));
    }
    m_root->logMap().set("player_pos", strf("[ ^#f45;{:4.2f}^reset;, ^#49f;{:4.2f}^reset; ]", m_player->position()[0], m_player->position()[1]));
    m_root->logMap().set("player_vel", strf("[ ^#f45;{:4.2f}^reset;, ^#49f;{:4.2f}^reset; ]", m_player->velocity()[0], m_player->velocity()[1]));
    m_root->logMap().set("player_aim", strf("[ ^#f45;{:4.2f}^reset;, ^#49f;{:4.2f}^reset; ]", aimPosition[0], aimPosition[1]));
    if (auto world = m_universeClient->worldClient()) {
      auto aim = Vec2I::floor(aimPosition);
      m_root->logMap().set("tile_liquid_level", toString(world->liquidLevel(aim).level));
      m_root->logMap().set("tile_dungeon_id", world->isTileProtected(aim) ? strf("^red;{}", world->dungeonId(aim)) : toString(world->dungeonId(aim)));
    }

    if (m_mainInterface->currentState() == MainInterface::ReturnToTitle)
      return changeState(MainAppState::Title);
    return {};
}

bool ClientApplication::isHeldActionTaken(InterfaceAction action) const {
  // Labels and camera controls are queried outside the gameplay-focus interval.
  for (auto const& keyEvent : m_heldKeyEvents) {
    if (m_guiContext->actions(keyEvent).contains(action))
      return true;
  }
  return false;
}


std::expected<void, StarException> ClientApplication::updateCamera(float dt) {
  if (!m_universeClient->worldClient())
    return {};

  WorldCamera& camera = m_worldPainter->camera();
  camera.beginUpdate();
  camera.setScreenSize(Application::renderer()->screenSize());
  auto zoom = m_root->configuration().and_then([](auto config) { return config->get("zoomLevel"); })
      .and_then([](Json value) { return value.toFloat(); });
  if (!zoom)
    return std::unexpected(std::move(zoom.error()));
  camera.setTargetPixelRatio(*zoom);
  camera.update(dt);

  if (m_mainInterface->fixedCamera() || dt <= 0.0f) {
    m_universeClient->worldClient()->setClientWindow(camera.worldTileRect());
    return {};
  }

  auto assetsResult = m_root->assets();
  if (!assetsResult)
    return std::unexpected(std::move(assetsResult.error()));
  auto assets = *assetsResult;

  const float triggerRadius = 100.0f;
  const float deadzone = 0.1f;
  const float panFactor = 1.5f;
  auto speed = m_root->configuration().and_then([](auto config) { return config->get("cameraSpeedFactor"); })
      .and_then([](Json j) { return j.toFloat(); });
  if (!speed)
    return std::unexpected(std::move(speed.error()));
  float cameraSpeedFactor = 30.0f / *speed;
  cameraSpeedFactor /= (dt * 60.f);

  auto cameraPosition = m_player->cameraPosition();
  if (!cameraPosition)
    return std::unexpected(std::move(cameraPosition.error()));
  auto playerCameraPosition = *cameraPosition;

  if (isHeldActionTaken(InterfaceAction::CameraShift)) {
    m_snapBackCameraOffset = false;
    m_cameraOffsetDownTime += dt;
    Vec2F aim = m_universeClient->worldClient()->geometry().diff(m_mainInterface->cursorWorldPosition(), playerCameraPosition);

    float magnitude = aim.magnitude() / (triggerRadius / camera.pixelRatio());
    if (magnitude > deadzone) {
      float cameraXOffset = aim.x() / magnitude;
      float cameraYOffset = aim.y() / magnitude;
      magnitude = (magnitude - deadzone) / (1.0 - deadzone);
      if (magnitude > 1)
        magnitude = 1;
      cameraXOffset *= magnitude * 0.5f * camera.pixelRatio() * panFactor;
      cameraYOffset *= magnitude * 0.5f * camera.pixelRatio() * panFactor;
      m_cameraXOffset = (m_cameraXOffset * (cameraSpeedFactor - 1.0) + cameraXOffset) / cameraSpeedFactor;
      m_cameraYOffset = (m_cameraYOffset * (cameraSpeedFactor - 1.0) + cameraYOffset) / cameraSpeedFactor;
    }
  } else {
    if (m_cameraOffsetDownTime > 0.0f && m_cameraOffsetDownTime < 0.333333f)
      m_snapBackCameraOffset = true;
    if (m_snapBackCameraOffset) {
      m_cameraXOffset = (m_cameraXOffset * (cameraSpeedFactor - 1.0)) / cameraSpeedFactor;
      m_cameraYOffset = (m_cameraYOffset * (cameraSpeedFactor - 1.0)) / cameraSpeedFactor;
    }
    m_cameraOffsetDownTime = 0.f;
  }
  Vec2F newCameraPosition;

  newCameraPosition.setX(playerCameraPosition.x());
  newCameraPosition.setY(playerCameraPosition.y());

  auto baseCamera = newCameraPosition;

  auto smoothRadius = assets->json("/interface.config:cameraSmoothRadius").and_then([](Json j) { return j.toFloat(); });
  if (!smoothRadius)
    return std::unexpected(std::move(smoothRadius.error()));
  const float cameraSmoothRadius = *smoothRadius;
  auto smoothFactor = assets->json("/interface.config:cameraSmoothFactor").and_then([](Json j) { return j.toFloat(); });
  if (!smoothFactor)
    return std::unexpected(std::move(smoothFactor.error()));
  const float cameraSmoothFactor = *smoothFactor;

  auto cameraSmoothDistance = m_universeClient->worldClient()->geometry().diff(m_cameraPositionSmoother, newCameraPosition).magnitude();
  if (cameraSmoothDistance > cameraSmoothRadius) {
    auto cameraDelta = m_universeClient->worldClient()->geometry().diff(m_cameraPositionSmoother, newCameraPosition);
    m_cameraPositionSmoother = newCameraPosition + cameraDelta.normalized() * cameraSmoothRadius;
    m_cameraSmoothDelta = {};
  }

  auto cameraDelta = m_universeClient->worldClient()->geometry().diff(m_cameraPositionSmoother, newCameraPosition);
  auto deadzoneValue = assets->json("/interface.config:cameraSmoothDeadzone").and_then([](Json j) { return j.toFloat(); });
  if (!deadzoneValue)
    return std::unexpected(std::move(deadzoneValue.error()));
  if (cameraDelta.magnitude() > *deadzoneValue)
    newCameraPosition = newCameraPosition + cameraDelta * (cameraSmoothFactor - 1.0) / cameraSmoothFactor;
  m_cameraPositionSmoother = newCameraPosition;

  newCameraPosition.setX(newCameraPosition.x() + m_cameraXOffset / camera.pixelRatio());
  newCameraPosition.setY(newCameraPosition.y() + m_cameraYOffset / camera.pixelRatio());

  auto smoothDelta = newCameraPosition - baseCamera;

  m_worldPainter->setCameraPosition(m_universeClient->worldClient()->geometry(), baseCamera + (smoothDelta + m_cameraSmoothDelta) * 0.5f);
  m_cameraSmoothDelta = smoothDelta;

  m_universeClient->worldClient()->setClientWindow(camera.worldTileRect());
  return {};
}

}

#if defined STAR_SYSTEM_WINDOWS

int __stdcall WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
  auto reportStartupFailure = [](std::string const& diagnostic) {
    Star::EmergencyLog::log(Star::LogLevel::Error, diagnostic);
    auto text = Star::String(diagnostic).wstring();
    if (text) {
      MessageBoxW(nullptr, text->c_str(), L"OpenStarbound startup error", MB_OK | MB_ICONERROR);
      return;
    }
    Star::EmergencyLog::error("Could not convert startup diagnostic to UTF-16: {}", Star::outputException(Star::ExceptionDiagnostic(text.error()), true));
    // Preserve the actual diagnostic even when its bytes are not valid UTF-8.
    std::wstring escaped = L"Startup failed. Diagnostic bytes (non-ASCII bytes escaped):\n";
    constexpr wchar_t digits[] = L"0123456789ABCDEF";
    for (unsigned char byte : diagnostic) {
      if ((byte >= 0x20 && byte < 0x7f) || byte == '\n' || byte == '\r' || byte == '\t') {
        escaped.push_back(static_cast<wchar_t>(byte));
      } else {
        escaped.append(L"\\x");
        escaped.push_back(digits[byte >> 4]);
        escaped.push_back(digits[byte & 0xf]);
      }
    }
    MessageBoxW(nullptr, escaped.c_str(), L"OpenStarbound startup error", MB_OK | MB_ICONERROR);
  };
  auto nativeRuntime = Star::NativeRuntime::create();
  if (!nativeRuntime) {
    reportStartupFailure(Star::strf("Client native initialization failed: {}", Star::outputException(Star::ExceptionDiagnostic(nativeRuntime.error()), true)));
    return 1;
  }
  int nArgs;
  LPWSTR* argsList = CommandLineToArgvW(GetCommandLineW(), &nArgs);
  if (!argsList) {
    auto error = GetLastError();
    reportStartupFailure(Star::strf("Could not read Windows command line: Windows error {}", error));
    return 1;
  }
  std::unique_ptr<void, decltype(&LocalFree)> argsOwner(argsList, &LocalFree);
  Star::StringList args;
  for (int i = 0; i < nArgs; ++i) {
    auto arg = Star::String::create(argsList[i]);
    if (!arg) {
      reportStartupFailure(Star::strf("Invalid Windows command line: {}", Star::outputException(Star::ExceptionDiagnostic(arg.error()), true)));
      return 1;
    }
    args.append(std::move(*arg));
  }
  if (IsDebuggerPresent() && AllocConsole()) {
    std::freopen("CONOUT$", "w", stdout);
    std::freopen("CONOUT$", "w", stderr);
  }
  unsigned long exceptionStackSize = 131072;
  SetThreadStackGuarantee(&exceptionStackSize);
  return Star::runMainApplication(std::make_unique<Star::ClientApplication>(), args);
}
#else
int main(int argc, char** argv) {
  auto nativeRuntime = Star::NativeRuntime::create();
  if (!nativeRuntime) {
    Star::EmergencyLog::error("Client native initialization failed: {}", Star::outputException(Star::ExceptionDiagnostic(nativeRuntime.error()), true));
    return 1;
  }
  Star::StringList args;
  args.reserve(argc);
  for (int i = 0; i < argc; ++i)
    args.append(Star::String(std::string_view(argv[i])));
  return Star::runMainApplication(std::make_unique<Star::ClientApplication>(), args);
}
#endif

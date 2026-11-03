#if defined STAR_SYSTEM_WINDOWS
#include <windows.h>
#endif

import star.xx_hash;
import star.native_runtime;
import star.json;
import star.file;
import star.random;
import star.lexical_cast;
import star.logging;
import star.id_map;
import star.format;
import star.string;
import star.lock_file;
import star.game_types;
import star.either;
import star.data_stream;
import star.rpc_promise;
import star.io_device;
import star.byte_array;
import star.data_stream_devices;

#if defined STAR_SYSTEM_WINDOWS
import star.string_windows;
#endif
import std;
typedef struct ZSTD_CCtx_s ZSTD_CCtx;
typedef struct ZSTD_DCtx_s ZSTD_DCtx;
typedef ZSTD_DCtx ZSTD_DStream;
typedef ZSTD_CCtx ZSTD_CStream;
import star.zstd_compression;
import star.net_compatibility;
import star.set;
import star.btree;
import star.ordered_map;
import star.lru_cache;
import star.thread;
import star.btree_database;
import star.casting;
import star.list;
import star.bi_map;
import star.perlin;
import star.weighted_pool;
import star.strong_typedef;
import star.worker_pool;

import star.sector_array_2d;
import star.color;
import star.image_processing;
import star.hash;
import star.string_view;
import star.directives;
import star.asset_path;
import star.image;
import star.map;
import star.ref_ptr;
import star.listener;
import star.version;
import star.ordered_set;

import star.lua_root;
import star.universe;
import star.celestial_coordinate;
import star.host_address;
import star.uuid;
import star.damage_types;
import star.world_geometry;
import star.status_types;
import star.damage;
import star.light_source;
import star.entity;
import star.collision_block;
import star.liquid_types;
import star.tile_damage;
import star.tile_sector_array;
import star.animation;
import star.particle;
import star.weather_types;
import star.sky_types;
import star.force_regions;
import star.world_parameters;
import star.celestial_parameters;
import star.world_layout;
import star.collision_generator;
import star.world_tiles;
import star.interaction_types;
import star.item_descriptor;
import star.quest_descriptor;
import star.interactive_entity;
import star.tile_entity;
import star.inspectable_entity;
import star.plant;
import star.plant_database;
import star.biome_placement;
import star.versioning_database;
import star.world_storage;
import star.tile_modification;
import star.world;
import star.celestial_types;
import star.chat_types;
import star.warping;
import star.wiring;
import star.player_types;
import star.sky_parameters;
import star.system_world;
import star.damage_manager;
import star.net_packets;
import star.cellular_light_array;
import star.cellular_lighting;
import star.cellular_liquid;
import star.world_structure;
import star.lua_components;
import star.drawable;
import star.entity_rendering_types;
import star.sky_render_data;
import star.parallax;
import star.world_render_data;
import star.world_client_state;
import star.interpolation_tracker;
import star.spawn_type_database;
import star.spawner;
import star.world_server;
import star.world_server_thread;
import star.world_template;
import star.system_world_server;
import star.system_world_server_thread;
import star.socket;
import star.tcp;
import star.p2p_networking_service;
import star.net_packet_socket;
import star.universe_connection;
import star.universe_settings;
import star.universe_server;
import star.asset_source;
import star.assets;
import star.root_base;
import star.root;
import star.root_loader;
import star.configuration;
import star.option_parser;
import star.version_option_parser;


import star.signal_handler;
import star.server_query;
import star.server_rcon;

using namespace Star;

Json const AdditionalDefaultConfiguration = JsonObject{
    {"configurationVersion", JsonObject{{"server", 4}}},
    {"runQueryServer", false},
    {"queryServerPort", 21025},
    {"queryServerBind", "::"},
    {"runRconServer", false},
    {"rconServerPort", 21026},
    {"rconServerBind", "::"},
    {"rconServerPassword", ""},
    {"rconServerTimeout", 1000},
    {"allowAssetsMismatch", true},
    {"serverOverrideAssetsDigest", Json()}};

static std::expected<void, StarException> runServer(int argc, char** argv) {
  #if defined STAR_SYSTEM_WINDOWS
  unsigned long exceptionStackSize = 131072;
  SetThreadStackGuarantee(&exceptionStackSize);
  #endif
  auto rootLoader = RootLoader::create({{}, AdditionalDefaultConfiguration, String("starbound_server.log"), LogLevel::Info, false, String("starbound_server.config")});
  if (!rootLoader)
    return std::unexpected(std::move(rootLoader.error()));
  rootLoader->setVersionName("Server");
  auto initialized = rootLoader->commandInitOrDie(argc, argv);
  if (!initialized)
    return std::unexpected(std::move(initialized.error()));
  auto root = std::move(initialized->first);
  auto loaded = root->fullyLoad();
  if (!loaded)
    return std::unexpected(std::move(loaded.error()));

  SignalHandler signalHandler;
  auto fatalSignals = signalHandler.setHandleFatal(true);
  if (!fatalSignals)
    return std::unexpected(std::move(fatalSignals.error()));
  auto interrupts = signalHandler.setHandleInterrupt(true);
  if (!interrupts)
    return std::unexpected(std::move(interrupts.error()));

  auto configuration = root->configuration();
  if (!configuration)
    return std::unexpected(std::move(configuration.error()));
  {
    auto versionLogged = root->logger().info("{}", rootLoader->getVersionString());
    if (!versionLogged)
      return std::unexpected(std::move(versionLogged.error()));
    auto rateSetting = (*configuration)->get("updateRate");
    if (!rateSetting)
      return std::unexpected(std::move(rateSetting.error()));
    if (*rateSetting) {
      auto updateRate = rateSetting->toFloat();
      if (!updateRate)
        return std::unexpected(std::move(updateRate.error()));
      root->timing().serverTimestep = root->timing().timestep = 1.0f / *updateRate;
      auto rateLogged = root->logger().info("Configured tick rate is {:4.2f}hz", *updateRate);
      if (!rateLogged)
        return std::unexpected(std::move(rateLogged.error()));
    }

    auto created = UniverseServer::create(*root, root->toStoragePath("universe"));
    if (!created)
      return std::unexpected(std::move(created.error()));
    auto server = std::move(*created);
    server->setListeningTcp(true);
    auto started = server->start();
    if (!started) {
      auto closed = server->close();
      if (!closed)
        EmergencyLog::error("Server cleanup after failed start also failed: {}", closed.error().what());
      return std::unexpected(std::move(started.error()));
    }

    std::unique_ptr<ServerQueryThread> queryServer;
    std::unique_ptr<ServerRconThread> rconServer;
    auto session = [&]() -> std::expected<void, StarException> {
    auto bindAddress = [&configuration](String const& bindKey, String const& portKey) -> std::expected<HostAddressWithPort, StarException> {
      auto bind = (*configuration)->get(bindKey);
      if (!bind)
        return std::unexpected(std::move(bind.error()));
      auto host = bind->toString();
      if (!host)
        return std::unexpected(std::move(host.error()));
      auto port = (*configuration)->get(portKey);
      if (!port)
        return std::unexpected(std::move(port.error()));
      auto number = port->toInt();
      if (!number)
        return std::unexpected(std::move(number.error()));
      return HostAddressWithPort::create(*host, *number);
    };

    auto querySetting = (*configuration)->get("runQueryServer");
    if (!querySetting)
      return std::unexpected(std::move(querySetting.error()));
    auto queryEnabled = querySetting->toBool();
    if (!queryEnabled)
      return std::unexpected(std::move(queryEnabled.error()));
    if (*queryEnabled) {
      auto address = bindAddress("queryServerBind", "queryServerPort");
      if (!address)
        return std::unexpected(std::move(address.error()));
      auto query = ServerQueryThread::create(*server, *address);
      if (!query)
        return std::unexpected(std::move(query.error()));
      queryServer = std::move(*query);
      auto queryStarted = queryServer->start();
      if (!queryStarted)
        return std::unexpected(std::move(queryStarted.error()));
    }

    auto rconSetting = (*configuration)->get("runRconServer");
    if (!rconSetting)
      return std::unexpected(std::move(rconSetting.error()));
    auto rconEnabled = rconSetting->toBool();
    if (!rconEnabled)
      return std::unexpected(std::move(rconEnabled.error()));
    if (*rconEnabled) {
      auto address = bindAddress("rconServerBind", "rconServerPort");
      if (!address)
        return std::unexpected(std::move(address.error()));
      auto rcon = ServerRconThread::create(*server, *address);
      if (!rcon)
        return std::unexpected(std::move(rcon.error()));
      rconServer = std::move(*rcon);
      auto rconStarted = rconServer->start();
      if (!rconStarted)
        return std::unexpected(std::move(rconStarted.error()));
    }

    while (server->isRunning()) {
      if (signalHandler.interruptCaught()) {
        auto interruptLogged = root->logger().info("Interrupt caught!");
        if (!interruptLogged)
          return std::unexpected(std::move(interruptLogged.error()));
        auto stopped = server->stop();
        if (!stopped)
          return std::unexpected(std::move(stopped.error()));
        break;
      }
      Thread::sleep(100);
    }
    return {};
    }();

    std::optional<StarException> shutdownError;
    auto collect = [&shutdownError](auto result) {
      if (!result) {
        if (!shutdownError)
          shutdownError = std::move(result.error());
        else
          EmergencyLog::error("Additional server shutdown error: {}", result.error().what());
      }
    };
    if (!session) {
      collect(std::move(session));
      collect(server->stop());
    }
    collect(server->join());
    if (queryServer) {
      queryServer->stop();
      collect(queryServer->join());
    }
    if (rconServer) {
      collect(rconServer->stop());
      collect(rconServer->join());
    }
    collect(server->close());
    if (shutdownError)
      return std::unexpected(std::move(*shutdownError));
  }
  return root->logger().info("Server shutdown gracefully");
}

int main(int argc, char** argv) {
  auto nativeRuntime = NativeRuntime::create();
  auto result = nativeRuntime ? runServer(argc, argv) : std::expected<void, StarException>(std::unexpected(std::move(nativeRuntime.error())));
  if (!result) {
    auto text = strf("{}\nDetails:\n{}", exceptionSummary(result.error()),
        outputException(ExceptionDiagnostic(result.error()), true));
    EmergencyLog::error("Fatal server error: {}", text);
    #if defined STAR_SYSTEM_WINDOWS
    auto wideText = stringToUtf16(text);
    MessageBoxW(nullptr, wideText ? wideText->get() : L"Server startup failed; diagnostic conversion failed. See the log.",
        L"OpenStarbound server error", MB_OK | MB_ICONERROR | MB_SYSTEMMODAL);
    #endif
    return 1;
  }
  return 0;
}

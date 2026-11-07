module;
import star.json;
import star.json_extra;
import star.xx_hash;
import star.lua;
import star.game_types;
import star.rpc_promise;
import star.strong_typedef;
import star.data_stream;
import star.vector;
import star.string;
import star.either;
import star.color;
import star.bi_map;
import star.random;
import star.weighted_pool;
import star.list;
import star.image_processing;
import star.hash;
import star.string_view;
import star.thread;
import star.directives;
import star.asset_path;
import star.ordered_set;
import star.io_device;
import star.byte_array;
import star.data_stream_devices;

import std;
using ZSTD_CCtx = struct ZSTD_CCtx_s;
using ZSTD_DCtx = struct ZSTD_DCtx_s;
using ZSTD_DStream = ZSTD_DCtx;
using ZSTD_CStream = ZSTD_CCtx;
import star.zstd_compression;
import star.net_compatibility;
import std;
import star.format;
import star.id_map;
import star.version;
import star.ordered_map;
import star.casting;
import star.set;
import star.worker_pool;

import star.sector_array_2d;
import star.perlin;
import star.btree;
import star.lru_cache;
import star.btree_database;
import star.image;
import star.audio;
import star.map;


// Match client include order for SIMD intrinsics used by xxhash.
import star.uuid;
import star.celestial_coordinate;
import star.warping;
import star.lua_game_converters;
import star.host_address;
import star.sky_types;
import star.animation;
import star.particle;
import star.weather_types;
import star.force_regions;
import star.world_parameters;
import star.celestial_parameters;
import star.chat_types;
import star.item_descriptor;
import star.quest_descriptor;
import star.ai_types;
import star.socket;
import star.tcp;
import star.p2p_networking_service;
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
import star.lua_root;
import star.versioning_database;
import star.world_storage;
import star.player_types;
import star.sky_parameters;
import star.system_world;
import star.damage_manager;
import star.net_packets;
import star.net_packet_socket;
import star.universe_connection;
import star.drawable;
import star.entity_rendering_types;
import star.sky_render_data;
import star.parallax;
import star.cellular_light_array;
import star.cellular_lighting;
import star.world_render_data;
import star.world_structure;
import star.chat_action;
import star.mixer;
import star.entity_rendering;
import star.world;
import star.lua_components;
import star.world_client_state;
import star.interpolation_tracker;
import star.ambient;
import star.weather;
import star.game_timers;
import star.world_client;
import star.world_client_thread;
import star.universe;
// TODO: make this more thread safe
import star.universe_client;
import star.world_template;

module star.universe_client_lua_bindings;
import star.client_context;
import star.root;

namespace Star {

std::expected<LuaCallbacks, StarException> LuaBindings::makeUniverseClientThreadCallbacks(Root& root, UniverseClient& universe) {
  LuaCallbacks callbacks;
  
  if (auto registration = callbacks.registerCallback("sendOwnUniverseMessage", [&universe](String const& message, LuaVariadic<Json> args) -> std::expected<RpcPromise<Json>, StarException> {
    auto pair = RpcPromise<Json>::createPair();
    auto passed =
      universe.passMessage({.message = message, .args = JsonArray::from(std::move(args)), .keeper = pair.second});
    if (!passed)
      return std::unexpected(std::move(passed.error()));
    return std::move(pair.first);
  }); !registration)
      return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.registerCallback("sendUniverseMessage", [&universe](ConnectionId const& connection, String const& message, LuaVariadic<Json> args) -> std::expected<RpcPromise<Json>, StarException> {
    return universe.sendUniverseMessage(connection,message,JsonArray::from(std::move(args)));
  }); !registration)
      return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.registerCallback("sendServerUniverseMessage", [&universe](String const& message, LuaVariadic<Json> args) -> std::expected<RpcPromise<Json>, StarException> {
    return universe.sendUniverseMessage(ServerConnectionId,message,JsonArray::from(std::move(args)));
  }); !registration)
      return std::unexpected(std::move(registration.error()));

  if (auto registration =
        callbacks.registerCallback("createClientCustomWorld",
                                   [&universe](String name, Json templateData) -> std::expected<void, StarException> {
                                     return universe.createCustomWorld(name, std::move(templateData));
                                   });
      !registration)
    return std::unexpected(std::move(registration.error()));

  if (auto registration = callbacks.registerCallback("createClientCustomWorldFromConfig", [&root, &universe](String name, Json worldConfig)  -> std::expected<void, StarException> {
    std::uint64_t worldSeed;
    auto hasSeed = worldConfig.contains("seed");
    if (!hasSeed)
      return std::unexpected(std::move(hasSeed.error()));
    if (*hasSeed) {
      auto seed = worldConfig.getUInt("seed");
      if (!seed)
        return std::unexpected(std::move(seed.error()));
      worldSeed = *seed;
    } else {
      worldSeed = root.random().randu64();
    }
    auto worldType = worldConfig.getString("type");
    if (!worldType)
      return std::unexpected(std::move(worldType.error()));
    std::expected<std::shared_ptr<VisitableWorldParameters>, StarException> worldParameters;
    if (auto terrestrialMatch = worldType->equalsIgnoreCase("Terrestrial"); !terrestrialMatch) {
      return std::unexpected(std::move(terrestrialMatch.error()));
    } else if (*terrestrialMatch) {
      auto planetType = worldConfig.getString("planetType");
      if (!planetType)
        return std::unexpected(std::move(planetType.error()));
      auto planetSize = worldConfig.getString("planetSize");
      if (!planetSize)
        return std::unexpected(std::move(planetSize.error()));
      worldParameters = generateTerrestrialWorldParameters(root, *planetType, *planetSize, worldSeed);
    } else if (auto asteroidMatch = worldType->equalsIgnoreCase("Asteroids"); !asteroidMatch) {
      return std::unexpected(std::move(asteroidMatch.error()));
    } else if (*asteroidMatch) {
      worldParameters = generateAsteroidsWorldParameters(root, worldSeed);
    } else if (auto dungeonMatch = worldType->equalsIgnoreCase("FloatingDungeon"); !dungeonMatch) {
      return std::unexpected(std::move(dungeonMatch.error()));
    } else if (*dungeonMatch) {
      auto dungeonWorld = worldConfig.getString("dungeonWorld");
      if (!dungeonWorld)
        return std::unexpected(std::move(dungeonWorld.error()));
      worldParameters = generateFloatingDungeonWorldParameters(root, *dungeonWorld);
    } else {
      return std::unexpected(StarException::format("Unknown world type: '{}'\n", *worldType));
    }
    if (!worldParameters)
      return std::unexpected(std::move(worldParameters.error()));
    auto level = worldConfig.optFloat("level");
    if (!level)
      return std::unexpected(std::move(level.error()));
    if (*level)
      (*worldParameters)->threatLevel = **level;
    auto beamUpRule = worldConfig.optString("beamUpRule");
    if (!beamUpRule)
      return std::unexpected(std::move(beamUpRule.error()));
    if (*beamUpRule) {
      if (!BeamUpRuleNames)
        return std::unexpected(BeamUpRuleNames.error());
      auto rule = BeamUpRuleNames->get_left(**beamUpRule);
      if (!rule)
        return std::unexpected(std::move(rule.error()));
      (*worldParameters)->beamUpRule = rule->get();
    }
    auto disableDeathDrops = worldConfig.getBool("disableDeathDrops", false);
    if (!disableDeathDrops)
      return std::unexpected(std::move(disableDeathDrops.error()));
    (*worldParameters)->disableDeathDrops = *disableDeathDrops;
    auto skyConfig = worldConfig.get("skyParameters", Json());
    if (!skyConfig)
      return std::unexpected(std::move(skyConfig.error()));
    auto skyParameters = SkyParameters::create(*skyConfig);
    if (!skyParameters)
      return std::unexpected(std::move(skyParameters.error()));
    auto worldTemplate = WorldTemplate::create(root, *worldParameters, *skyParameters, worldSeed);
    if (!worldTemplate)
      return std::unexpected(std::move(worldTemplate.error()));
    auto storedTemplate = (*worldTemplate)->store();
    if (!storedTemplate)
      return std::unexpected(std::move(storedTemplate.error()));
    return universe.createCustomWorld(name, std::move(*storedTemplate));
  }); !registration)
      return std::unexpected(std::move(registration.error()));
  
  if (auto registration = callbacks.registerCallback("connectionId", [&universe]()  -> std::expected<ConnectionId, StarException> {
    if (!universe.isConnected())
      return std::unexpected(StarException("Universe is not connected"));
    return universe.clientContext()->connectionId();
  }); !registration)
      return std::unexpected(std::move(registration.error()));
  
  if (auto registration = callbacks.registerCallback("clientUuid", [&universe]()  -> std::expected<String, StarException> {
    if (!universe.isConnected())
      return std::unexpected(StarException("Universe is not connected"));
    return universe.clientContext()->playerUuid().hex();
  }); !registration)
      return std::unexpected(std::move(registration.error()));
  
  if (auto registration = callbacks.registerCallback("serverOpenProtocolVersion", [&universe]() {
    return universe.connectionVersion();
  }); !registration)
      return std::unexpected(std::move(registration.error()));
  
  if (auto registration = callbacks.registerCallback("playerCount", [&universe]() -> Vec2U {
    if (!universe.isConnected())
      return Vec2U();
    return Vec2U(universe.players(),universe.maxPlayers());
  }); !registration)
      return std::unexpected(std::move(registration.error()));
  
  return callbacks;
}

std::expected<LuaCallbacks, StarException> LuaBindings::makeUniverseClientCallbacks(Root& root, UniverseClient& universe) {
  auto createdCallbacks = makeUniverseClientThreadCallbacks(root, universe);
  if (!createdCallbacks)
    return std::unexpected(std::move(createdCallbacks.error()));
  LuaCallbacks callbacks = std::move(*createdCallbacks);
  
  if (auto registration = callbacks.registerCallback("subWorldActive", [&universe](String const& worldId) -> std::expected<bool, StarException> {
    auto id = parseWorldId(worldId);
    if (!id)
      return std::unexpected(std::move(id.error()));
    return universe.subWorldExistsOnWorld(*id);
  }); !registration)
      return std::unexpected(std::move(registration.error()));
  
  if (auto registration = callbacks.registerCallback("loadSubWorld", [&universe](String const& worldId)  -> std::expected<void, StarException> {
    auto id = parseWorldId(worldId);
    if (!id)
      return std::unexpected(std::move(id.error()));
    auto result = universe.getSubWorldOnWorld(*id);
    if (!result)
      return std::unexpected(std::move(result.error()));
    return {};
  }); !registration)
      return std::unexpected(std::move(registration.error()));
  
  if (auto registration = callbacks.registerCallback("unloadSubWorld", [&universe](String const& worldId)  -> std::expected<void, StarException> {
    auto id = parseWorldId(worldId);
    if (!id)
      return std::unexpected(std::move(id.error()));
    auto result = universe.destroySubWorldOnWorld(*id);
    if (!result)
      return std::unexpected(std::move(result.error()));
    return {};
  }); !registration)
      return std::unexpected(std::move(registration.error()));
  
  if (auto registration = callbacks.registerCallback("sendSubWorldMessage", [&universe](String const& worldId, String const& message, LuaVariadic<Json> args) -> std::expected<RpcPromise<Json>, StarException> {
    auto id = parseWorldId(worldId);
    if (!id)
      return std::unexpected(std::move(id.error()));
    return universe.sendSubWorldOnWorldMessage(*id, message, JsonArray::from(std::move(args)));
  }); !registration)
      return std::unexpected(std::move(registration.error()));
  
  if (auto registration = callbacks.registerCallback("sendMainWorldMessage", [&universe](String const& message, LuaVariadic<Json> args) -> std::expected<RpcPromise<Json>, StarException> {
    return universe.sendMainWorldMessage(message, JsonArray::from(std::move(args)));
  }); !registration)
      return std::unexpected(std::move(registration.error()));
  
  // primarily useful on the main world and other non-universe contexts, as universe is accessible everywhere on the main client thread
  if (auto registration = callbacks.registerCallback("callScriptContext", [&universe](String const& contextName, String const& function, LuaVariadic<LuaValue> const& args) -> std::expected<std::optional<LuaValue>, StarException> {
    auto context = universe.scriptContext(contextName);
    if (!context)
      return std::unexpected(StarException::format("Context {} does not exist", contextName));
    return context->invoke(function, args);
  }); !registration)
      return std::unexpected(std::move(registration.error()));
  
  // primarily useful on universe contexts
  if (auto registration = callbacks.registerCallback("callMainWorldScriptContext", [&universe](String const& contextName, String const& function, LuaVariadic<LuaValue> const& args) -> std::expected<std::optional<LuaValue>, StarException> {
    if (!universe.isConnected())
      return std::unexpected(StarException("Universe is not connected"));
    auto world = universe.worldClient();
    if (!world->inWorld())
      return std::unexpected(StarException("Not in a world"));
    auto context = world->scriptContext(contextName);
    if (!context)
      return std::unexpected(StarException::format("Context {} does not exist", contextName));
    return context->invoke(function, args);
  }); !registration)
      return std::unexpected(std::move(registration.error()));

  return callbacks;
}

}

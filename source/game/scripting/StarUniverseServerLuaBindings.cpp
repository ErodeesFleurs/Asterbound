module;
import star.json;
import star.json_extra;
import star.xx_hash;
import star.lua;
import star.game_types;
import star.rpc_promise;
import star.id_map;
import std;
import star.format;
import star.string;
import star.lock_file;
import star.thread;
import star.either;
import star.data_stream;
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
import star.set;
import star.btree;
import star.ordered_map;
import star.lru_cache;
import star.btree_database;
import star.casting;
import star.ordered_set;
import star.list;
import star.bi_map;
import star.version;
import star.random;
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
import star.logging;
import star.listener;


// Match client include order for SIMD intrinsics used by xxhash.
import star.lua_game_converters;
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
import star.lua_root;
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
import star.configuration;
import star.root;

module star.universe_server_lua_bindings;

namespace Star::LuaBindings::UniverseServerCallbacks {
  [[nodiscard]] std::expected<std::optional<String>, StarException> uuidForClient(UniverseServer& universe, ConnectionId arg1);
  [[nodiscard]] std::expected<List<ConnectionId>, StarException> clientIds(UniverseServer& universe);
  [[nodiscard]] std::expected<std::size_t, StarException> numberOfClients(UniverseServer& universe);
  [[nodiscard]] std::expected<bool, StarException> isConnectedClient(UniverseServer& universe, ConnectionId arg1);
  [[nodiscard]] std::expected<String, StarException> clientNick(UniverseServer& universe, ConnectionId arg1);
  [[nodiscard]] std::expected<std::optional<ConnectionId>, StarException> findNick(UniverseServer& universe, String const& arg1);
  [[nodiscard]] std::expected<String, StarException> clientAccount(UniverseServer& universe, ConnectionId arg1);
  [[nodiscard]] std::expected<void, StarException> adminBroadcast(UniverseServer& universe, String const& arg1);
  [[nodiscard]] std::expected<void, StarException> adminWhisper(UniverseServer& universe, ConnectionId arg1, String const& arg2);
  [[nodiscard]] std::expected<bool, StarException> isAdmin(UniverseServer& universe, ConnectionId arg1);
  [[nodiscard]] std::expected<void, StarException> setAdmin(UniverseServer& universe, ConnectionId arg1, std::optional<bool> arg2);
  [[nodiscard]] std::expected<bool, StarException> isPvp(UniverseServer& universe, ConnectionId arg1);
  [[nodiscard]] std::expected<void, StarException> setPvp(UniverseServer& universe, ConnectionId arg1, std::optional<bool> arg2);
  [[nodiscard]] std::expected<bool, StarException> isWorldActive(UniverseServer& universe, String const& worldId);
  [[nodiscard]] std::expected<StringList, StarException> activeWorlds(UniverseServer& universe);
  [[nodiscard]] std::expected<RpcPromise<Json>, StarException> sendWorldMessage(UniverseServer& universe, String const& worldId, String const& message, LuaVariadic<Json> args);
  [[nodiscard]] std::expected<bool, StarException> sendPacket(UniverseServer& universe, ConnectionId clientId, String const& packetTypeName, Json const& args);
  [[nodiscard]] std::expected<String, StarException> clientWorld(UniverseServer& universe, ConnectionId clientId);
  [[nodiscard]] std::expected<void, StarException> disconnectClient(UniverseServer& universe, ConnectionId clientId, std::optional<String> const& reason);
  [[nodiscard]] std::expected<void, StarException> banClient(UniverseServer& universe, ConnectionId clientId, std::optional<String> const& reason, bool banIp, bool banUuid, std::optional<int> timeout);
}

namespace Star {

std::expected<LuaCallbacks, StarException> LuaBindings::makeUniverseServerCallbacks(Root& root, UniverseServer& universe) {
  LuaCallbacks callbacks;

  if (auto registration =
        callbacks.registerCallbackWithSignature<std::expected<std::optional<String>, StarException>, ConnectionId>(
          "uuidForClient",
          [&universe](ConnectionId client) { return UniverseServerCallbacks::uuidForClient(universe, client); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.registerCallbackWithSignature<std::expected<List<ConnectionId>, StarException>>(
        "clientIds", [&universe]() { return UniverseServerCallbacks::clientIds(universe); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.registerCallbackWithSignature<std::expected<std::size_t, StarException>>(
        "numberOfClients", [&universe]() { return UniverseServerCallbacks::numberOfClients(universe); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.registerCallbackWithSignature<std::expected<bool, StarException>, ConnectionId>(
        "isConnectedClient",
        [&universe](ConnectionId client) { return UniverseServerCallbacks::isConnectedClient(universe, client); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.registerCallbackWithSignature<std::expected<String, StarException>, ConnectionId>(
        "clientNick",
        [&universe](ConnectionId client) { return UniverseServerCallbacks::clientNick(universe, client); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration =
        callbacks.registerCallbackWithSignature<std::expected<std::optional<ConnectionId>, StarException>, String>(
          "findNick", [&universe](String const& nick) { return UniverseServerCallbacks::findNick(universe, nick); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.registerCallbackWithSignature<std::expected<String, StarException>, ConnectionId>(
        "clientAccount",
        [&universe](ConnectionId client) { return UniverseServerCallbacks::clientAccount(universe, client); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.registerCallbackWithSignature<std::expected<void, StarException>, String>(
        "adminBroadcast",
        [&universe](String const& message) { return UniverseServerCallbacks::adminBroadcast(universe, message); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration =
        callbacks.registerCallbackWithSignature<std::expected<void, StarException>, ConnectionId, String>(
          "adminWhisper",
          [&universe](ConnectionId client, String const& message) {
            return UniverseServerCallbacks::adminWhisper(universe, client, message);
          });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.registerCallbackWithSignature<std::expected<bool, StarException>, ConnectionId>(
        "isAdmin", [&universe](ConnectionId client) { return UniverseServerCallbacks::isAdmin(universe, client); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.registerCallbackWithSignature<std::expected<bool, StarException>, ConnectionId>(
        "isPvp", [&universe](ConnectionId client) { return UniverseServerCallbacks::isPvp(universe, client); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration =
        callbacks.registerCallbackWithSignature<std::expected<void, StarException>, ConnectionId, bool>(
          "setPvp",
          [&universe](ConnectionId client, bool enabled) {
            return UniverseServerCallbacks::setPvp(universe, client, enabled);
          });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.registerCallbackWithSignature<std::expected<bool, StarException>, String>(
        "isWorldActive",
        [&universe](String const& world) { return UniverseServerCallbacks::isWorldActive(universe, world); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.registerCallbackWithSignature<std::expected<StringList, StarException>>(
        "activeWorlds", [&universe]() { return UniverseServerCallbacks::activeWorlds(universe); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.registerCallbackWithSignature<std::expected<RpcPromise<Json>, StarException>,
                                                                  String, String, LuaVariadic<Json>>(
        "sendWorldMessage",
        [&universe](String const& world, String const& message, LuaVariadic<Json>&& args) {
          return UniverseServerCallbacks::sendWorldMessage(universe, world, message, std::move(args));
        });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration =
        callbacks.registerCallbackWithSignature<std::expected<bool, StarException>, ConnectionId, String, Json>(
          "sendPacket",
          [&universe](ConnectionId client, String const& type, Json const& args) {
            return UniverseServerCallbacks::sendPacket(universe, client, type, args);
          });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.registerCallbackWithSignature<std::expected<String, StarException>, ConnectionId>(
        "clientWorld",
        [&universe](ConnectionId client) { return UniverseServerCallbacks::clientWorld(universe, client); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration =
        callbacks
          .registerCallbackWithSignature<std::expected<void, StarException>, ConnectionId, std::optional<String>>(
            "disconnectClient",
            [&universe](ConnectionId client, std::optional<String> const& reason) {
              return UniverseServerCallbacks::disconnectClient(universe, client, reason);
            });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration =
        callbacks.registerCallbackWithSignature<std::expected<void, StarException>, ConnectionId, std::optional<String>,
                                                bool, bool, std::optional<int>>(
          "banClient",
          [&universe](ConnectionId client, std::optional<String> const& reason, bool banIp, bool banUuid,
                      std::optional<int> timeout) {
            return UniverseServerCallbacks::banClient(universe, client, reason, banIp, banUuid, timeout);
          });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.registerCallback("warpClient", [&universe](ConnectionId clientId, String action, std::optional<bool> deploy)  -> std::expected<void, StarException> {
    auto warp = parseWarpAction(action);
    if (!warp)
      return std::unexpected(std::move(warp.error()));
    return universe.clientWarpPlayer(clientId, std::move(*warp), deploy.value_or(false));
  }); !registration)
      return std::unexpected(std::move(registration.error()));

  if (auto registration = callbacks.registerCallback("setServerAccount", [&root](String const& account, String const& password, std::optional<bool> admin)  -> std::expected<void, StarException> {
    auto config = root.configuration();
    if (!config)
      return std::unexpected(std::move(config.error()));
    auto users = (*config)->get("serverUsers");
    if (!users)
      return std::unexpected(std::move(users.error()));
    auto serverUsers = users->toObject();
    if (!serverUsers)
      return std::unexpected(std::move(serverUsers.error()));
    (*serverUsers)[account] = JsonObject{{"password", password}, {"admin", admin.value_or(false)}};
    return (*config)->set("serverUsers", std::move(*serverUsers));
  }); !registration)
      return std::unexpected(std::move(registration.error()));

  if (auto registration = callbacks.registerCallback("removeServerAccount", [&root](String const& account)  -> std::expected<void, StarException> {
    auto config = root.configuration();
    if (!config)
      return std::unexpected(std::move(config.error()));
    auto users = (*config)->get("serverUsers");
    if (!users)
      return std::unexpected(std::move(users.error()));
    auto serverUsers = users->toObject();
    if (!serverUsers)
      return std::unexpected(std::move(serverUsers.error()));
    serverUsers->erase(account);
    return (*config)->set("serverUsers", std::move(*serverUsers));
  }); !registration)
      return std::unexpected(std::move(registration.error()));

  if (auto registration = callbacks.registerCallback("getServerAccounts", [&root]() -> std::expected<Json, StarException> {
    auto config = root.configuration();
    if (!config)
      return std::unexpected(std::move(config.error()));
    auto users = (*config)->get("serverUsers");
    if (!users)
      return std::unexpected(std::move(users.error()));
    auto serverUsers = users->iterateObject();
    if (!serverUsers)
      return std::unexpected(std::move(serverUsers.error()));
    JsonObject result;
    for (auto const& p : *serverUsers) {
      auto admin = p.second.getBool("admin", false);
      if (!admin)
        return std::unexpected(std::move(admin.error()));
      result[p.first] = JsonObject{{"admin", *admin}};
    }
    return result;
  }); !registration)
      return std::unexpected(std::move(registration.error()));

  
  if (auto registration = callbacks.registerCallback("clientOpenProtocolVersion", [&universe](ConnectionId clientId) {
    return universe.clientConnectionVersion(clientId);
  }); !registration)
      return std::unexpected(std::move(registration.error()));
  
  if (auto registration = callbacks.registerCallback("createCustomWorld", [&root, &universe](String name, Json templateData)  -> std::expected<void, StarException> {
    auto worldTemplate = WorldTemplate::create(root, templateData);
    if (!worldTemplate)
      return std::unexpected(std::move(worldTemplate.error()));
    return universe.createCustomWorld(CustomWorldId(name), std::move(*worldTemplate));
  }); !registration)
      return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.registerCallback("createCustomWorldFromConfig", [&root, &universe](String name, Json worldConfig)  -> std::expected<void, StarException> {
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
    if (auto terrestrialMatch = worldType->equals_ignore_case("Terrestrial"); !terrestrialMatch) {
      return std::unexpected(std::move(terrestrialMatch.error()));
    } else if (*terrestrialMatch) {
      auto planetType = worldConfig.getString("planetType");
      if (!planetType)
        return std::unexpected(std::move(planetType.error()));
      auto planetSize = worldConfig.getString("planetSize");
      if (!planetSize)
        return std::unexpected(std::move(planetSize.error()));
      worldParameters = generateTerrestrialWorldParameters(root, *planetType, *planetSize, worldSeed);
    } else if (auto asteroidMatch = worldType->equals_ignore_case("Asteroids"); !asteroidMatch) {
      return std::unexpected(std::move(asteroidMatch.error()));
    } else if (*asteroidMatch) {
      worldParameters = generateAsteroidsWorldParameters(root, worldSeed);
    } else if (auto dungeonMatch = worldType->equals_ignore_case("FloatingDungeon"); !dungeonMatch) {
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
    return universe.createCustomWorld(CustomWorldId(name), std::move(*worldTemplate));
  }); !registration)
      return std::unexpected(std::move(registration.error()));
  
  if (auto registration = callbacks.registerCallback("sendOwnUniverseMessage", [&universe](String const& message, LuaVariadic<Json> args) -> std::expected<RpcPromise<Json>, StarException> {
    return universe.sendUniverseMessage(ServerConnectionId,message,JsonArray::from(std::move(args)));
  }); !registration)
      return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.registerCallback("sendUniverseMessage", [&universe](ConnectionId const& connection, String const& message, LuaVariadic<Json> args) -> std::expected<RpcPromise<Json>, StarException> {
    return universe.sendUniverseMessage(connection,message,JsonArray::from(std::move(args)));
  }); !registration)
      return std::unexpected(std::move(registration.error()));
  
  if (auto registration = callbacks.registerCallback("connectionId", []() {
    return ServerConnectionId;
  }); !registration)
      return std::unexpected(std::move(registration.error()));

  return callbacks;
}

// Gets a list of client ids
//
// @return A list of numerical client IDs.
std::expected<std::optional<String>, StarException> LuaBindings::UniverseServerCallbacks::uuidForClient(UniverseServer& universe, ConnectionId arg1) {
  return universe.uuidForClient(arg1).transform([](Uuid const& str) { return str.hex(); });
}

// Gets a list of client ids
//
// @return A list of numerical client IDs.
std::expected<List<ConnectionId>, StarException> LuaBindings::UniverseServerCallbacks::clientIds(UniverseServer& universe) {
  return universe.clientIds();
}

// Gets the number of logged in clients
//
// @return An integer containing the number of logged in clients
std::expected<std::size_t, StarException> LuaBindings::UniverseServerCallbacks::numberOfClients(UniverseServer& universe) {
  return universe.numberOfClients();
}

// Returns whether or not the provided client ID is currently connected
//
// @param clientId the client ID in question
// @return A bool that is true if the client is connected and false otherwise
std::expected<bool, StarException> LuaBindings::UniverseServerCallbacks::isConnectedClient(UniverseServer& universe, ConnectionId arg1) {
  return universe.isConnectedClient(arg1);
}

// Returns the nickname for the given client ID
//
// @param clientId the client ID in question
// @return A string containing the nickname of the given client
std::expected<String, StarException> LuaBindings::UniverseServerCallbacks::clientNick(UniverseServer& universe, ConnectionId arg1) {
  return universe.clientNick(arg1);
}

// Returns the client ID for the given nick
//
// @param nick the nickname of the client to search for
// @return An integer containing the clientID of the nick in question
std::expected<std::optional<ConnectionId>, StarException> LuaBindings::UniverseServerCallbacks::findNick(UniverseServer& universe, String const& arg1) {
  return universe.findNick(arg1);
}

// Sends a message to all logged in clients
//
// @param message the message to broadcast
// @return nil
std::expected<void, StarException> LuaBindings::UniverseServerCallbacks::adminBroadcast(UniverseServer& universe, String const& arg1) {
  return universe.adminBroadcast(arg1);
}

std::expected<String, StarException> LuaBindings::UniverseServerCallbacks::clientAccount(UniverseServer& universe, ConnectionId arg1) {
  return universe.clientAccount(arg1);
}


// Sends a message to a specific client
//
// @param clientId the client id to whisper
// @param message the message to whisper
// @return nil
std::expected<void, StarException> LuaBindings::UniverseServerCallbacks::adminWhisper(UniverseServer& universe, ConnectionId arg1, String const& arg2) {
  ConnectionId client = arg1;
  String const& message = arg2;
  return universe.adminWhisper(client, message);
}

// Returns whether or not a specific client is flagged as an admin
//
// @param clientId the client id to check
// @return a boolean containing true if the client is an admin, false otherwise
std::expected<bool, StarException> LuaBindings::UniverseServerCallbacks::isAdmin(UniverseServer& universe, ConnectionId arg1) {
  return universe.isAdmin(arg1);
}

// Set (or unset) the admin status of a specific user
//
// @param clientId the client id to modify
// @param setAdminTo set admin status to this bool, defaults to true
// @return nil
std::expected<void, StarException> LuaBindings::UniverseServerCallbacks::setAdmin(UniverseServer& universe, ConnectionId arg1, std::optional<bool> arg2) {
  ConnectionId client = arg1;
  bool setAdminTo = arg2.value_or(true);
  return universe.setAdmin(client, setAdminTo);
}

// Returns whether or not a specific client is flagged as pvp
//
// @param clientId the client id to check
// @return a boolean containing true if the client is flagged as pvp, false
// otherwise
std::expected<bool, StarException> LuaBindings::UniverseServerCallbacks::isPvp(UniverseServer& universe, ConnectionId arg1) {
  return universe.isPvp(arg1);
}

// Set (or unset) the pvp status of a specific user
//
// @param clientId the client id to check
// @param setPvp set pvp status to this bool, defaults to true
// @return nil
std::expected<void, StarException> LuaBindings::UniverseServerCallbacks::setPvp(UniverseServer& universe, ConnectionId arg1, std::optional<bool> arg2) {
  ConnectionId client = arg1;
  bool setPvpTo = arg2.value_or(true);
  return universe.setPvp(client, setPvpTo);
}

std::expected<bool, StarException> LuaBindings::UniverseServerCallbacks::isWorldActive(UniverseServer& universe, String const& worldId) {
  auto id = parseWorldId(worldId);
  if (!id)
    return std::unexpected(std::move(id.error()));
  return universe.isWorldActive(*id);
}

std::expected<StringList, StarException> LuaBindings::UniverseServerCallbacks::activeWorlds(UniverseServer& universe) {
  auto worlds = universe.activeWorlds();
  if (!worlds)
    return std::unexpected(std::move(worlds.error()));
  StringList result;
  for (auto const& world : *worlds) {
    auto id = Star::print_world_id(world);
    if (!id)
      return std::unexpected(std::move(id.error()));
    result.push_back(std::move(*id));
  }
  return result;
}

std::expected<RpcPromise<Json>, StarException> LuaBindings::UniverseServerCallbacks::sendWorldMessage(UniverseServer& universe, String const& worldId, String const& message, LuaVariadic<Json> args) {
  auto id = parseWorldId(worldId);
  if (!id)
    return std::unexpected(std::move(id.error()));
  return universe.sendWorldMessage(*id, message, JsonArray::from(std::move(args)));
}

std::expected<bool, StarException> LuaBindings::UniverseServerCallbacks::sendPacket(UniverseServer& universe, ConnectionId clientId, String const& packetTypeName, Json const& args) {
  if (!PacketTypeNames)
    return std::unexpected(PacketTypeNames.error());
  auto packetType = PacketTypeNames->get_left(packetTypeName);
  if (!packetType)
    return std::unexpected(std::move(packetType.error()));
  auto packet = createPacket(universe.root().random(), packetType->get(), args);
  if (!packet)
    return std::unexpected(std::move(packet.error()));
  return universe.sendPacket(clientId, std::move(*packet));
}

std::expected<String, StarException> LuaBindings::UniverseServerCallbacks::clientWorld(UniverseServer& universe, ConnectionId clientId) {
  auto world = universe.clientWorld(clientId);
  if (!world)
    return std::unexpected(std::move(world.error()));
  return Star::print_world_id(*world);
}

std::expected<void, StarException> LuaBindings::UniverseServerCallbacks::disconnectClient(UniverseServer& universe, ConnectionId clientId, std::optional<String> const& reason) {
  return universe.disconnectClient(clientId, reason.value_or(String{}));
}

std::expected<void, StarException> LuaBindings::UniverseServerCallbacks::banClient(UniverseServer& universe, ConnectionId clientId, std::optional<String> const& reason, bool banIp, bool banUuid, std::optional<int> timeout) {
  return universe.banUser(clientId, reason.value_or(String{}), std::make_pair(banIp, banUuid), timeout);
}

}

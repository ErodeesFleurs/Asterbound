import star.json_extra;
import star.json;
import star.xx_hash;
import star.id_map;
import star.ordered_map;
import star.lru_cache;
import star.thread;
import star.time;
import star.random;
import star.ttl_cache;
import star.casting;
import star.game_types;
import star.strong_typedef;
import star.list;
import star.rpc_promise;
import star.vector;
import star.rect;
import star.bi_map;
import star.a_star;
import star.data_stream;
import star.string;
import star.byte_array;
import star.encode;
import star.bytes;
import star.format;
import star.optional;
import star.lexical_cast;
import star.version;
import star.weighted_pool;
import star.parametric_function;
import star.logging;
import star.set;
import star.color;
import star.image_processing;
import star.hash;
import star.string_view;
import star.directives;
import star.asset_path;
import star.lua;
import star.periodic_function;
import star.audio;
import star.map;
import star.either;
import star.image;
import star.math_common;
import star.worker_pool;

import std;
import star.output_proxy;
import star.sector_array_2d;
import star.net_compatibility;
import star.perlin;
import star.btree;
import star.data_stream_devices;
import star.btree_database;
import star.ordered_set;
import star.lock_file;
import star.io_device;

typedef struct ZSTD_CCtx_s ZSTD_CCtx;
typedef struct ZSTD_DCtx_s ZSTD_DCtx;
typedef ZSTD_DCtx ZSTD_DStream;
typedef ZSTD_CCtx ZSTD_CStream;
import star.zstd_compression;
import star.ref_ptr;
import star.listener;

import star.lua_root;

import star.damage_types;
import star.world_geometry;
import star.status_types;
import star.damage;
import star.light_source;
import star.entity;
import star.tile_damage;
import star.interaction_types;
import star.celestial_coordinate;
import star.quest_descriptor;
import star.interactive_entity;
import star.collision_block;
import star.tile_entity;
import star.tile_modification;
import star.force_regions;
import star.world;
import star.physics_entity;
import star.movement_controller;
import star.platformer_astar_types;
import star.anchorable_entity;

import star.game_timers;
import star.actor_movement_controller;
import star.drawable;
import star.item;
import star.item_descriptor;
import star.item_recipe;
import star.shell_parser;
import star.lua_components;

import star.command_processor;
import star.animation;
import star.particle;
import star.animated_part_set;
import star.mixer;
import star.networked_animator;
import star.humanoid;
import star.portrait_entity;
import star.damage_bar_entity;
import star.nametag_entity;
import star.scripted_entity;
import star.chat_action;
import star.chatty_entity;
import star.emote_entity;
import star.entity_rendering_types;
import star.lounging_entities;
import star.mobile_entity;
import star.actor_entity;
import star.tool_user_entity;
import star.lua_actor_movement_component;
import star.lua_animation_component;
import star.npc_database;
import star.effect_emitter;
import star.npc;
import star.tile_sector_array;
import star.liquid_types;
import star.weather_types;
import star.sky_types;
import star.world_parameters;
import star.celestial_parameters;
import star.world_layout;
import star.collision_generator;
import star.world_tiles;
import star.celestial_types;
import star.chat_types;
import star.uuid;
import star.warping;
import star.wiring;
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
import star.cellular_light_array;
import star.cellular_lighting;
import star.cellular_liquid;
import star.world_structure;
import star.sky_render_data;
import star.parallax;
import star.world_render_data;
import star.world_client_state;
import star.interpolation_tracker;
import star.spawn_type_database;
import star.spawner;
import star.world_server;
import star.universe;
import star.host_address;
import star.world_server_thread;
import star.world_template;
import star.system_world_server;
import star.system_world_server_thread;
import star.socket;
import star.tcp;
import star.p2p_networking_service;
import star.net_packet_socket;
import star.universe_connection;
import star.universe_server;
import star.universe_settings;
import star.asset_source;
import star.assets;
import star.root;
import star.configuration;
import star.item_drop;
import star.inventory_types;
import star.ai_types;
import star.entity_rendering;
import star.radio_message_database;
import star.player;
import star.aggressive_entity;
import star.monster_database;
import star.monster;
import star.stagehand;
import star.chat_processor;
import star.vehicle;
import star.treasure;

import star.vehicle_database;
import star.liquids_database;
import star.universe_server_lua_bindings;
import star.celestial_lua_bindings;
import star.stagehand_database;
import star.item_database;
import star.celestial_database;


import star.algorithm;
namespace Star {

CommandProcessor::CommandProcessor(Root& root, UniverseServer& universe)
  : m_root(root), m_universe(universe), m_scriptComponent(root) {}

std::expected<std::shared_ptr<CommandProcessor>, StarException> CommandProcessor::create(Root& root, UniverseServer& universe) {
  auto processor = std::shared_ptr<CommandProcessor>(new CommandProcessor(root, universe));
  auto assets = root.assets();
  if (!assets) return std::unexpected(std::move(assets.error()));
  auto config = (*assets)->json("/universe_server.config");
  if (!config) return std::unexpected(std::move(config.error()));
  auto luaRoot = LuaRoot::create(root);
  if (!luaRoot) return std::unexpected(std::move(luaRoot.error()));
  processor->m_luaRoot = std::move(*luaRoot);
  auto pause = config->getFloat("luaGcPause");
  if (!pause) return std::unexpected(std::move(pause.error()));
  auto step = config->getFloat("luaGcStepMultiplier");
  if (!step) return std::unexpected(std::move(step.error()));
  processor->m_luaRoot->tuneAutoGarbageCollection(*pause, *step);
  auto universeCallbacks = LuaBindings::makeUniverseServerCallbacks(root, universe);
  if (!universeCallbacks) return std::unexpected(std::move(universeCallbacks.error()));
  auto celestialCallbacks = LuaBindings::makeCelestialCallbacks(root, universe);
  if (!celestialCallbacks) return std::unexpected(std::move(celestialCallbacks.error()));
  auto universeAdded = processor->m_scriptComponent.addCallbacks("universe", *universeCallbacks);
  if (!universeAdded) return std::unexpected(std::move(universeAdded.error()));
  auto celestialAdded = processor->m_scriptComponent.addCallbacks("celestial", *celestialCallbacks);
  if (!celestialAdded) return std::unexpected(std::move(celestialAdded.error()));
  auto universeThreadAdded = processor->m_scriptComponent.addThreadCallbacks("universe", *universeCallbacks);
  if (!universeThreadAdded) return std::unexpected(std::move(universeThreadAdded.error()));
  auto celestialThreadAdded = processor->m_scriptComponent.addThreadCallbacks("celestial", *celestialCallbacks);
  if (!celestialThreadAdded) return std::unexpected(std::move(celestialThreadAdded.error()));
  auto commandCallbacks = processor->makeCommandCallbacks();
  if (!commandCallbacks) return std::unexpected(std::move(commandCallbacks.error()));
  auto commandsAdded = processor->m_scriptComponent.addCallbacks("CommandProcessor", std::move(*commandCallbacks));
  if (!commandsAdded) return std::unexpected(std::move(commandsAdded.error()));
  auto scriptJson = config->get("commandProcessorScripts");
  if (!scriptJson) return std::unexpected(std::move(scriptJson.error()));
  auto scripts = jsonToStringList(*scriptJson);
  if (!scripts) return std::unexpected(std::move(scripts.error()));
  auto scriptsSet = processor->m_scriptComponent.setScripts(std::move(*scripts));
  if (!scriptsSet) return std::unexpected(std::move(scriptsSet.error()));
  auto rootSet = processor->m_scriptComponent.setLuaRoot(processor->m_luaRoot);
  if (!rootSet) return std::unexpected(std::move(rootSet.error()));
  auto initialized = processor->m_scriptComponent.init();
  if (!initialized) return std::unexpected(std::move(initialized.error()));
  return processor;
}

std::expected<void, StarException> CommandProcessor::close() {
  std::lock_guard locker(m_mutex);
  return m_scriptComponent.uninit();
}

std::expected<ServerCommandResult, StarException> CommandProcessor::adminCommand(String const& command, String const& argumentString) {
  std::lock_guard locker(m_mutex);
  return handleCommand(ServerConnectionId, command, argumentString);
}

std::expected<ServerCommandResult, StarException> CommandProcessor::userCommand(ConnectionId connectionId, String const& command, String const& argumentString) {
  std::lock_guard locker(m_mutex);
  if (connectionId == ServerConnectionId)
    return std::unexpected(StarException("CommandProcessor::userCommand called with ServerConnectionId"));
  return handleCommand(connectionId, command, argumentString);
}

std::expected<String, StarException> CommandProcessor::help(ConnectionId connectionId, String const& argumentString) {
  auto parsedArguments = m_parser.tokenizeToStringList(argumentString);
  if (!parsedArguments) return std::unexpected(std::move(parsedArguments.error()));
  auto arguments = std::move(*parsedArguments);
  auto assetsResult = m_root.assets();
  if (!assetsResult) return std::unexpected(std::move(assetsResult.error()));
  auto assets = std::move(*assetsResult);
  std::array<Json, 5> commands;
  std::array<String, 5> names{"basic", "openSb", "admin", "debug", "openSbDebug"};
  for (std::size_t i = 0; i != names.size(); ++i) {
    auto config = assets->json("/help.config:" + names[i] + "Commands");
    if (!config) return std::unexpected(std::move(config.error()));
    commands[i] = std::move(*config);
  }
  if (!arguments.empty()) {
    for (auto const& config : commands) {
      auto text = config.optString(arguments[0]);
      if (!text) return std::unexpected(std::move(text.error()));
      if (*text) return **text;
    }
  }
  auto adminError = adminCheck(connectionId, "");
  if (!adminError) return std::unexpected(std::move(adminError.error()));
  String result;
  for (std::size_t i = 0; i != commands.size(); ++i) {
    if (i >= 2 && *adminError) break;
    auto object = commands[i].toObject();
    if (!object) return std::unexpected(std::move(object.error()));
    StringList list = object->keys();
    std::sort(list.begin(), list.end(), std::less<String>());
    auto templateJson = assets->json("/help.config:" + names[i] + "HelpText");
    if (!templateJson) return std::unexpected(std::move(templateJson.error()));
    auto text = templateJson->toString();
    if (!text) return std::unexpected(std::move(text.error()));
    String description = "/" + list.join(", /");
    std::array<std::pair<std::string_view, std::string_view>, 1> bindings{{{"commands", description.utf8()}}};
    auto formatted = formatTags(text->utf8(), bindings);
    if (!formatted) return std::unexpected(std::move(formatted.error()));
    if (i) result += "\n";
    result += *formatted;
  }
  auto help = commands[0].getString("help");
  if (!help) return std::unexpected(std::move(help.error()));
  return result + "\n" + *help;
}

std::expected<String, StarException> CommandProcessor::admin(ConnectionId connectionId, String const& argumentString) {
  auto config = m_root.configuration();
  if (!config) return std::unexpected(std::move(config.error()));
  auto arguments = m_parser.tokenizeToStringList(argumentString);
  if (!arguments) return std::unexpected(std::move(arguments.error()));
  ConnectionId targetClientId = connectionId;
  if (!arguments->empty()) {
    auto permission = adminCheck(connectionId, "make user admin");
    if (!permission) return std::unexpected(std::move(permission.error()));
    if (*permission) return **permission;
    auto target = playerCidFromCommand((*arguments)[0], m_universe);
    if (!target) return std::unexpected(std::move(target.error()));
    if (!*target) return strf("No user with specifier {} found.", (*arguments)[0]);
    targetClientId = **target;
  } else {
    auto canBecome = m_universe.canBecomeAdmin(connectionId);
    if (!canBecome) return std::unexpected(std::move(canBecome.error()));
    if (!*canBecome) {
      auto admin = m_universe.isAdmin(connectionId);
      if (!admin) return std::unexpected(std::move(admin.error()));
      if (!*admin) return "Insufficient privileges to make self admin.";
    }
  }
  if (targetClientId == ServerConnectionId) return "Invalid client state";
  auto allowJson = (*config)->get("allowAdminCommands");
  if (!allowJson) return std::unexpected(std::move(allowJson.error()));
  auto allow = allowJson->toBool();
  if (!allow) return std::unexpected(std::move(allow.error()));
  if (!*allow) return "Admin commands disabled on this server.";
  auto wasAdmin = m_universe.isAdmin(targetClientId);
  if (!wasAdmin) return std::unexpected(std::move(wasAdmin.error()));
  auto set = m_universe.setAdmin(targetClientId, !*wasAdmin);
  if (!set) return std::unexpected(std::move(set.error()));
  auto nick = m_universe.clientNick(targetClientId);
  if (!nick) return std::unexpected(std::move(nick.error()));
  if (!*wasAdmin) return strf("Admin privileges now given to {}", *nick);
  return strf("Admin privileges taken away from {}", *nick);
}

std::expected<String, StarException> CommandProcessor::serverDebug(ConnectionId connectionId, String const&) {
  auto permission = adminCheck(connectionId, "debug server");
  if (!permission) return std::unexpected(std::move(permission.error()));
  if (*permission) return **permission;
  auto local = m_universe.isLocal(connectionId);
  if (!local) return std::unexpected(std::move(local.error()));
  if (*local) return "Server is local, server debug already active";
  auto version = m_universe.clientConnectionVersion(connectionId);
  if (!version) return std::unexpected(std::move(version.error()));
  if (*version < 17) return "Client is not new enough to debug server";
  auto enabled = m_universe.serverDebug(connectionId);
  if (!enabled) return std::unexpected(std::move(enabled.error()));
  auto set = m_universe.setServerDebug(connectionId, !*enabled);
  if (!set) return std::unexpected(std::move(set.error()));
  return strf("Server debug {}", !*enabled ? std::string_view("enabled") : std::string_view("disabled"));
}

std::expected<String, StarException> CommandProcessor::pvp(ConnectionId connectionId, String const&) {
  auto wasPvp = m_universe.isPvp(connectionId);
  if (!wasPvp) return std::unexpected(std::move(wasPvp.error()));
  auto set = m_universe.setPvp(connectionId, !*wasPvp);
  if (!set) return std::unexpected(std::move(set.error()));
  auto nowPvp = m_universe.isPvp(connectionId);
  if (!nowPvp) return std::unexpected(std::move(nowPvp.error()));
  if (*nowPvp != *wasPvp) {
    auto nick = m_universe.clientNick(connectionId);
    if (!nick) return std::unexpected(std::move(nick.error()));
    auto sent = m_universe.adminBroadcast(*nowPvp ? strf("Player {} is now PVP", *nick) : strf("Player {} is a big wimp and is no longer PVP", *nick));
    if (!sent) return std::unexpected(std::move(sent.error()));
  }
  auto pvp = m_universe.isPvp(connectionId);
  if (!pvp) return std::unexpected(std::move(pvp.error()));
  return *pvp ? String("PVP active") : String("PVP inactive");
}

std::expected<String, StarException> CommandProcessor::whoami(ConnectionId connectionId, String const&) {
  auto nick = m_universe.clientNick(connectionId);
  if (!nick) return std::unexpected(std::move(nick.error()));
  auto admin = m_universe.isAdmin(connectionId);
  if (!admin) return std::unexpected(std::move(admin.error()));
  return strf("Server: You are {}. You are {}an Admin", *nick, *admin ? std::string_view() : std::string_view("not "));
}

std::expected<String, StarException> CommandProcessor::warp(ConnectionId connectionId, String const& argumentString) {
  auto adminCheckError = adminCheck(connectionId, "do the space warp again");
  if (!adminCheckError) return std::unexpected(std::move(adminCheckError.error()));
  if (*adminCheckError) return **adminCheckError;

  auto result = [&]() -> std::expected<String, StarException> {
    auto action = parseWarpAction(argumentString);
    if (!action) return std::unexpected(std::move(action.error()));
    auto warped = m_universe.clientWarpPlayer(connectionId, *action);
    if (!warped) return std::unexpected(std::move(warped.error()));
    return "Lets do the space warp again";
  
  }();
  if (result) return std::move(*result);
  auto const& exception = result.error();
  {
    auto logged = m_root.logger().warnException("Could not parse warp target", ExceptionDiagnostic(exception));
    if (!logged) return std::unexpected(std::move(logged.error()));
    return strf("Could not parse the argument {} as a warp target", argumentString);
    }

}

std::expected<String, StarException> CommandProcessor::warpRandom(ConnectionId connectionId, String const& typeName) {
  auto permission = adminCheck(connectionId, "warp to random world");
  if (!permission) return std::unexpected(std::move(permission.error()));
  if (*permission) return **permission;
  Vec2I size = {2, 2};
  auto celestialDatabase = m_universe.celestialDatabase();
  std::optional<CelestialCoordinate> target;
  auto validPlanet = [&](CelestialCoordinate const& planet) -> std::expected<bool, StarException> {
    auto parameters = celestialDatabase->parameters(planet);
    if (!parameters) return std::unexpected(std::move(parameters.error()));
    if (*parameters) {
      if (auto visitable = (*parameters)->visitableParameters())
        return visitable->typeName == typeName;
    }
    return false;
  };
  while (!target) {
    RectI region = RectI::withSize(Vec2I(m_root.random().randi32(), m_root.random().randi32()), size);
    while (true) {
      auto loaded = celestialDatabase->scanRegionFullyLoaded(region);
      if (!loaded) return std::unexpected(std::move(loaded.error()));
      if (*loaded) break;
      auto scanned = celestialDatabase->scanSystems(region);
      if (!scanned) return std::unexpected(std::move(scanned.error()));
    }
    auto systems = celestialDatabase->scanSystems(region);
    if (!systems) return std::unexpected(std::move(systems.error()));
    for (auto const& system : *systems) {
      auto planets = celestialDatabase->children(system);
      if (!planets) return std::unexpected(std::move(planets.error()));
      for (auto const& planet : *planets) {
        auto valid = validPlanet(planet);
        if (!valid) return std::unexpected(std::move(valid.error()));
        if (*valid) target = planet;
        if (!target) {
          auto moons = celestialDatabase->children(planet);
          if (!moons) return std::unexpected(std::move(moons.error()));
          for (auto const& moon : *moons) {
            auto validMoon = validPlanet(moon);
            if (!validMoon) return std::unexpected(std::move(validMoon.error()));
            if (*validMoon) { target = moon; break; }
          }
        }
      }
    }
    if (size.magnitude() > 1024) return "could not find a matching world";
    size *= 2;
  }
  auto warped = m_universe.clientWarpPlayer(connectionId, WarpToWorld(CelestialWorldId(*target)));
  if (!warped) return std::unexpected(std::move(warped.error()));
  return strf("warping to {}", *target);
}

std::expected<String, StarException> CommandProcessor::timewarp(ConnectionId connectionId, String const& argumentsString) {
  auto adminCheckError = adminCheck(connectionId, "do the time warp again");
  if (!adminCheckError) return std::unexpected(std::move(adminCheckError.error()));
  if (*adminCheckError) return **adminCheckError;

  auto parsedArguments = m_parser.tokenizeToStringList(argumentsString);
  if (!parsedArguments) return std::unexpected(std::move(parsedArguments.error()));
  auto arguments = std::move(*parsedArguments);
  if (arguments.empty())
    return "Not enough arguments to /timewarp";

  auto result = [&]() -> std::expected<String, StarException> {
    auto timeResult = lexicalCast<double>(arguments[0]);
    if (!timeResult) return std::unexpected(std::move(timeResult.error()));
    auto time = *timeResult;
    if (time == 0.0)
      return "You suck at time travel.";
    else if (time < 0.0 && (arguments.size() < 2 || arguments[1] != "please"))
      return "Great Scott! We can't go back in time!";

    m_universe.universeClock()->adjustTime(time);
    return time > 0.0 ? String("It's just a jump to the left...") : String("And then a step to the right...");
  
  }();
  if (result) return std::move(*result);
  auto const& exception = result.error();
  if (exception.is<BadLexicalCast>()) {
    return strf("Could not parse the argument {} as a time adjustment", arguments[0]);
    }
  return std::unexpected(std::move(result.error()));

}

std::expected<String, StarException> CommandProcessor::timescale(ConnectionId connectionId, String const& argumentsString) {
  auto adminCheckError = adminCheck(connectionId, "mess with time");
  if (!adminCheckError) return std::unexpected(std::move(adminCheckError.error()));
  if (*adminCheckError) return **adminCheckError;

  auto parsedArguments = m_parser.tokenizeToStringList(argumentsString);
  if (!parsedArguments) return std::unexpected(std::move(parsedArguments.error()));
  auto arguments = std::move(*parsedArguments);

  if (arguments.empty())
    return strf("Current timescale is {:6.6f}x", m_root.timing().timescale);

  auto parsedScale = lexicalCast<float>(arguments[0]);
  if (!parsedScale) return std::unexpected(std::move(parsedScale.error()));
  float timescale = std::clamp(*parsedScale, 0.001f, 32.0f);
  auto set = m_universe.setTimescale(timescale);
  if (!set) return std::unexpected(std::move(set.error()));
  return strf("Set timescale to {:6.6f}x", timescale);
}

std::expected<String, StarException> CommandProcessor::tickrate(ConnectionId connectionId, String const& argumentsString) {
  auto adminCheckError = adminCheck(connectionId, "change the tick rate");
  if (!adminCheckError) return std::unexpected(std::move(adminCheckError.error()));
  if (*adminCheckError) return **adminCheckError;

  auto parsedArguments = m_parser.tokenizeToStringList(argumentsString);
  if (!parsedArguments) return std::unexpected(std::move(parsedArguments.error()));
  auto arguments = std::move(*parsedArguments);

  if (arguments.empty())
    return strf("Current tick rate is {:4.2f}Hz", 1.0f / m_root.timing().serverTimestep);

  auto parsedRate = lexicalCast<float>(arguments[0]);
  if (!parsedRate) return std::unexpected(std::move(parsedRate.error()));
  float tickRate = std::clamp(*parsedRate, 5.f, 500.f);
  m_universe.setTickRate(tickRate);
  return strf("Set tick rate to {:4.2f}Hz", tickRate);
}

std::expected<String, StarException> CommandProcessor::setTileProtection(ConnectionId connectionId, String const& argumentString) {
  auto adminCheckError = adminCheck(connectionId, "modify world properties");
  if (!adminCheckError) return std::unexpected(std::move(adminCheckError.error()));
  if (*adminCheckError) return **adminCheckError;

  auto parsedArguments = m_parser.tokenizeToStringList(argumentString);
  if (!parsedArguments) return std::unexpected(std::move(parsedArguments.error()));
  auto arguments = std::move(*parsedArguments);

  if (arguments.size() < 2)
    return "Not enough arguments to /settileprotection. Use /settileprotection <dungeonId> <protected>";

  auto result = [&]() -> std::expected<String, StarException> {
    auto last = arguments.takeLast();
    if (!last) return std::unexpected(std::move(last.error()));
    auto protectedJson = Json::parse(*last);
    if (!protectedJson) return std::unexpected(std::move(protectedJson.error()));
    auto protectedValue = protectedJson->toBool();
    if (!protectedValue) return std::unexpected(std::move(protectedValue.error()));
    bool isProtected = *protectedValue;
    List<DungeonId> dungeonIds;
    for (auto& banana : arguments) {
      auto sliced = banana.split("..");
      if (!sliced) return std::unexpected(std::move(sliced.error()));
      auto slices = std::move(*sliced);
      auto it = slices.begin();
      DungeonId previous = 0;
      while (it != slices.end()) {
        auto currentResult = lexicalCast<DungeonId>(*it);
        if (!currentResult) return std::unexpected(std::move(currentResult.error()));
        DungeonId current = *currentResult;
        dungeonIds.append(current);
        if (it++ != slices.begin() && previous != current) {
          if (current < previous) std::swap(previous, current);
          for (DungeonId id = previous + 1; id != current; ++id)
            dungeonIds.append(id);
        }
        previous = current;
      }
    }
    std::size_t changed = 0;
    auto executed = m_universe.executeForClient(connectionId, [&](WorldServer& world, std::shared_ptr<Player> const&) -> std::expected<void, StarException> {
       auto count = world.setTileProtection(dungeonIds, isProtected);
       if (!count) return std::unexpected(std::move(count.error()));
       changed = *count;
       return {};
      });
    if (!executed) return std::unexpected(std::move(executed.error()));
    if (!*executed) {
      return "Invalid client state";
    }
    String output = strf("{} {} dungeon IDs", isProtected ? std::string_view("Protected") : std::string_view("Unprotected"), changed);
    return changed < dungeonIds.size() ? strf("{} ({} unchanged)", output, dungeonIds.size() - changed) : output;
  
  }();
  if (result) return std::move(*result);
  auto const& exception = result.error();
  if (exception.is<BadLexicalCast>()) {
    return strf("Could not parse /settileprotection parameters. Use /settileprotection <dungeonId...> <protected>", argumentString);
    }
  return std::unexpected(std::move(result.error()));

}

std::expected<String, StarException> CommandProcessor::setDungeonId(ConnectionId connectionId, String const& argumentString) {
  auto adminCheckError = adminCheck(connectionId, "set dungeon id");
  if (!adminCheckError) return std::unexpected(std::move(adminCheckError.error()));
  if (*adminCheckError) return **adminCheckError;

  auto parsedArguments = m_parser.tokenizeToStringList(argumentString);
  if (!parsedArguments) return std::unexpected(std::move(parsedArguments.error()));
  auto arguments = std::move(*parsedArguments);
  if (arguments.size() < 1)
    return "Not enough arguments to /setdungeonid. Use /setdungeonid <dungeonId>";

  auto result = [&]() -> std::expected<String, StarException> {
    auto dungeonIdResult = lexicalCast<DungeonId>(arguments[0]);
    if (!dungeonIdResult) return std::unexpected(std::move(dungeonIdResult.error()));
    DungeonId dungeonId = *dungeonIdResult;

    auto done = m_universe.executeForClient(connectionId, [dungeonId](WorldServer& world, std::shared_ptr<Player> const& player) -> std::expected<void, StarException> {
        auto aimPosition = player->aimPosition();
        if (!aimPosition) return std::unexpected(std::move(aimPosition.error()));
        return world.setDungeonId(RectI::withSize(Vec2I((*aimPosition)), Vec2I(1, 1)), dungeonId);
      });

    if (!done) return std::unexpected(std::move(done.error()));
    return *done ? String() : String("Failed to set dungeon id.");
  
  }();
  if (result) return std::move(*result);
  auto const& exception = result.error();
  if (exception.is<BadLexicalCast>()) {
    return strf("Could not parse /setdungeonid parameters. Use /setdungeonid <dungeonId>!", argumentString);
    }
  return std::unexpected(std::move(result.error()));

}

std::expected<String, StarException> CommandProcessor::setPlayerStart(ConnectionId connectionId, String const&) {
  auto adminCheckError = adminCheck(connectionId, "modify world properties");
  if (!adminCheckError) return std::unexpected(std::move(adminCheckError.error()));
  if (*adminCheckError) return **adminCheckError;

  auto executed = m_universe.executeForClient(connectionId, [](WorldServer& world, std::shared_ptr<Player> const& player) -> std::expected<void, StarException> {
      auto feetOffset = player->feetOffset();
      if (!feetOffset) return std::unexpected(std::move(feetOffset.error()));
      world.setPlayerStart(player->position() + *feetOffset);
      return {};
    });
  if (!executed) return std::unexpected(std::move(executed.error()));

  return "";
}

std::expected<String, StarException> CommandProcessor::spawnItem(ConnectionId connectionId, String const& argumentString) {
  auto adminCheckError = adminCheck(connectionId, "spawn items");
  if (!adminCheckError) return std::unexpected(std::move(adminCheckError.error()));
  if (*adminCheckError) return **adminCheckError;

  auto parsedArguments = m_parser.tokenizeToStringList(argumentString);
  if (!parsedArguments) return std::unexpected(std::move(parsedArguments.error()));
  auto arguments = std::move(*parsedArguments);

  if (arguments.empty())
    return "Not enough arguments to /spawnitem";

  auto result = [&]() -> std::expected<String, StarException> {
    String kind = arguments[0];
    Json parameters = JsonObject();
    std::uint64_t amount = 1;
    std::optional<float> level;
    std::optional<std::uint64_t> seed;

    if (arguments.size() >= 2) {
      auto amountResult = lexicalCast<std::uint64_t>(arguments[1]);
      if (!amountResult) return std::unexpected(std::move(amountResult.error()));
      amount = *amountResult;
    }

    if (arguments.size() >= 3) {
      auto parametersJson = Json::parse(arguments[2]);
      if (!parametersJson) return std::unexpected(std::move(parametersJson.error()));
      parameters = *parametersJson;
    }

    if (arguments.size() >= 4) {
      auto levelResult = lexicalCast<float>(arguments[3]);
      if (!levelResult) return std::unexpected(std::move(levelResult.error()));
      level = *levelResult;
    }

    if (arguments.size() >= 5) {
      auto seedResult = lexicalCast<std::uint64_t>(arguments[4]);
      if (!seedResult) return std::unexpected(std::move(seedResult.error()));
      seed = *seedResult;
    }

    auto done = m_universe.executeForClient(connectionId, [&](WorldServer& world, std::shared_ptr<Player> const& player) -> std::expected<void, StarException> {
        auto itemDatabaseResult = m_root.itemDatabase();
        if (!itemDatabaseResult) return std::unexpected(std::move(itemDatabaseResult.error()));
        auto itemDatabase = std::move(*itemDatabaseResult);
        auto descriptor = ItemDescriptor::create(kind, amount, parameters);
        if (!descriptor) return std::unexpected(std::move(descriptor.error()));
        auto item = itemDatabase->item(*descriptor, level, seed, true);
        if (!item) return std::unexpected(std::move(item.error()));
        auto aimPosition = player->aimPosition();
        if (!aimPosition) return std::unexpected(std::move(aimPosition.error()));
        auto drop = ItemDrop::createRandomizedDrop(m_root, *item, (*aimPosition));
        if (!drop) return std::unexpected(std::move(drop.error()));
        return world.addEntity(*drop);
      });

    if (!done) return std::unexpected(std::move(done.error()));
    return *done ? String() : String("Invalid client state");
  
  }();
  if (result) return std::move(*result);
  auto const& exception = result.error();
  if (exception.is<JsonParsingException>()) {
    auto logged = m_root.logger().warnException(strf("Error while processing /spawnitem '{}' command. Json parse problem", arguments[0]), ExceptionDiagnostic(exception));
    if (!logged) return std::unexpected(std::move(logged.error()));
    return "Could not parse item parameters";
    }
  if (exception.is<ItemException>()) {
    auto logged = m_root.logger().warnException(strf("Error while processing /spawnitem '{}' command. Item instantiation problem", arguments[0]), ExceptionDiagnostic(exception));
    if (!logged) return std::unexpected(std::move(logged.error()));
    return strf("Could not load item '{}'", arguments[0]);
    }
  if (exception.is<BadLexicalCast>()) {
    auto logged = m_root.logger().warnException("Error while processing /spawnitem command. Number expected", ExceptionDiagnostic(exception));
    if (!logged) return std::unexpected(std::move(logged.error()));
    return strf("Could not load item '{}'", arguments[0]);
    }
  {
    auto logged = m_root.logger().warnException(strf("Error while processing /spawnitem '{}' command", arguments[0]), ExceptionDiagnostic(exception));
    if (!logged) return std::unexpected(std::move(logged.error()));
    return strf("Could not load item '{}'", arguments[0]);
    }

}

std::expected<String, StarException> CommandProcessor::spawnTreasure(ConnectionId connectionId, String const& argumentString) {
  auto adminCheckError = adminCheck(connectionId, "spawn items");
  if (!adminCheckError) return std::unexpected(std::move(adminCheckError.error()));
  if (*adminCheckError) return **adminCheckError;

  auto parsedArguments = m_parser.tokenizeToStringList(argumentString);
  if (!parsedArguments) return std::unexpected(std::move(parsedArguments.error()));
  auto arguments = std::move(*parsedArguments);

  if (arguments.empty())
    return "Not enough arguments to /spawntreasure";

  auto result = [&]() -> std::expected<String, StarException> {
    String treasurePool = arguments[0];
    float level = 1;

    if (arguments.size() >= 2) {
      auto levelResult = lexicalCast<float>(arguments[1]);
      if (!levelResult) return std::unexpected(std::move(levelResult.error()));
      level = *levelResult;
    }

    auto done = m_universe.executeForClient(connectionId, [&](WorldServer& world, std::shared_ptr<Player> const& player) -> std::expected<void, StarException> {
        auto treasureDatabaseResult = m_root.treasureDatabase();
        if (!treasureDatabaseResult) return std::unexpected(std::move(treasureDatabaseResult.error()));
        auto treasureDatabase = std::move(*treasureDatabaseResult);
        auto treasure = treasureDatabase->createTreasure(treasurePool, level, m_root.random().randu64());
        if (!treasure) return std::unexpected(std::move(treasure.error()));
        for (auto const& treasureItem : *treasure) {
          auto aimPosition = player->aimPosition();
          if (!aimPosition) return std::unexpected(std::move(aimPosition.error()));
          auto drop = ItemDrop::createRandomizedDrop(m_root, treasureItem, (*aimPosition));
          if (!drop) return std::unexpected(std::move(drop.error()));
          auto added = world.addEntity(*drop);
          if (!added) return std::unexpected(std::move(added.error()));
        }
        return {};
      });

    if (!done) return std::unexpected(std::move(done.error()));
    return *done ? String() : String("Invalid client state");
  
  }();
  if (result) return std::move(*result);
  auto const& exception = result.error();
  if (exception.is<JsonParsingException>()) {
    auto logged = m_root.logger().warnException(strf("Error while processing /spawntreasure '{}' command. Json parse problem", arguments[0]), ExceptionDiagnostic(exception));
    if (!logged) return std::unexpected(std::move(logged.error()));
    return "Could not parse item parameters";
    }
  if (exception.is<ItemException>()) {
    auto logged = m_root.logger().warnException(strf("Error while processing /spawntreasure '{}' command. Item instantiation problem", arguments[0]), ExceptionDiagnostic(exception));
    if (!logged) return std::unexpected(std::move(logged.error()));
    return strf("Could not load item '{}'", arguments[0]);
    }
  if (exception.is<BadLexicalCast>()) {
    auto logged = m_root.logger().warnException("Error while processing /spawntreasure command. Number expected", ExceptionDiagnostic(exception));
    if (!logged) return std::unexpected(std::move(logged.error()));
    return strf("Could not load item '{}'", arguments[0]);
    }
  {
    auto logged = m_root.logger().warnException(strf("Error while processing /spawntreasure '{}' command", arguments[0]), ExceptionDiagnostic(exception));
    if (!logged) return std::unexpected(std::move(logged.error()));
    return strf("Could not load item '{}'", arguments[0]);
    }

}

std::expected<String, StarException> CommandProcessor::spawnMonster(ConnectionId connectionId, String const& argumentString) {
  auto adminCheckError = adminCheck(connectionId, "spawn monsters");
  if (!adminCheckError) return std::unexpected(std::move(adminCheckError.error()));
  if (*adminCheckError) return **adminCheckError;

  StringList arguments;
  auto result = [&]() -> std::expected<String, StarException> {
    auto parsedArguments = m_parser.tokenizeToStringList(argumentString);
  if (!parsedArguments) return std::unexpected(std::move(parsedArguments.error()));
  arguments = std::move(*parsedArguments);
  auto requiredArgument = arguments.at(0);
  if (!requiredArgument) return std::unexpected(std::move(requiredArgument.error()));

    auto monsterDatabaseResult = m_root.monsterDatabase();
    if (!monsterDatabaseResult) return std::unexpected(std::move(monsterDatabaseResult.error()));
    auto monsterDatabase = std::move(*monsterDatabaseResult);
    std::shared_ptr<Monster> monster;

    float level = 1;
    if (arguments.size() >= 2) {
      auto levelResult = lexicalCast<float>(arguments[1]);
      if (!levelResult) return std::unexpected(std::move(levelResult.error()));
      level = *levelResult;
    }

    Json parameters = JsonObject();
    if (arguments.size() >= 3) {
      auto parsed = Json::parse(arguments[2]);
      if (!parsed) return std::unexpected(std::move(parsed.error()));
      auto object = parsed->toObject();
      if (!object) return std::unexpected(std::move(object.error()));
      auto merged = parameters.setAll(*object);
      if (!merged) return std::unexpected(std::move(merged.error()));
      parameters = std::move(*merged);
    }

    auto parameterObject = parameters.toObject();
    if (!parameterObject) return std::unexpected(std::move(parameterObject.error()));
    auto variant = monsterDatabase->randomMonster(arguments[0], *parameterObject);
    if (!variant) return std::unexpected(std::move(variant.error()));
    auto created = monsterDatabase->createMonster(*variant, level);
    if (!created) return std::unexpected(std::move(created.error()));
    monster = std::move(*created);
    auto done = m_universe.executeForClient(connectionId,
        [&](WorldServer& world, std::shared_ptr<Player> const& player) -> std::expected<void, StarException> {
          auto aimPosition = player->aimPosition();
          if (!aimPosition) return std::unexpected(std::move(aimPosition.error()));
          monster->setPosition((*aimPosition));
          return world.addEntity(monster);
        });

    if (!done) return std::unexpected(std::move(done.error()));
    return *done ? String() : String("Invalid client state");
  
  }();
  if (result) return std::move(*result);
  auto const& exception = result.error();
  {
    auto logged = m_root.logger().warnException(strf("Could not spawn Monster of type '{}'", arguments.empty() ? String() : arguments[0]), ExceptionDiagnostic(exception));
    if (!logged) return std::unexpected(std::move(logged.error()));
    return strf("Could not spawn Monster of type '{}'", argumentString);
    }

}

std::expected<String, StarException> CommandProcessor::spawnNpc(ConnectionId connectionId, String const& argumentString) {
  auto adminCheckError = adminCheck(connectionId, "spawn NPCs");
  if (!adminCheckError) return std::unexpected(std::move(adminCheckError.error()));
  if (*adminCheckError) return **adminCheckError;

  auto parsedArguments = m_parser.tokenizeToStringList(argumentString);
  if (!parsedArguments) return std::unexpected(std::move(parsedArguments.error()));
  auto arguments = std::move(*parsedArguments);

  auto result = [&]() -> std::expected<String, StarException> {
    auto npcDatabaseResult = m_root.npcDatabase();
    if (!npcDatabaseResult) return std::unexpected(std::move(npcDatabaseResult.error()));
    auto npcDatabase = std::move(*npcDatabaseResult);
    float npcLevel = 1;
    std::uint64_t seed = m_root.random().randu64();
    Json overrides;

    if (arguments.size() < 2)
      return "You must specify a species and NPC type to spawn.";

    if (arguments.size() >= 3) {
      auto npcLevelResult = lexicalCast<float>(arguments[2]);
      if (!npcLevelResult) return std::unexpected(std::move(npcLevelResult.error()));
      npcLevel = *npcLevelResult;
    }
    if (arguments.size() >= 4) {
      auto seedResult = lexicalCast<std::uint64_t>(arguments[3]);
      if (!seedResult) return std::unexpected(std::move(seedResult.error()));
      seed = *seedResult;
    }
    if (arguments.size() >= 5) {
      auto overridesJson = Json::parse(arguments[4]);
      if (!overridesJson) return std::unexpected(std::move(overridesJson.error()));
      auto overridesObject = overridesJson->toObject();
      if (!overridesObject) return std::unexpected(std::move(overridesObject.error()));
      overrides = Json(*overridesObject);
    }

    auto variant = npcDatabase->generateNpcVariant(arguments[0], arguments[1], npcLevel, seed, overrides);
    if (!variant) return std::unexpected(std::move(variant.error()));
    auto npcResult = npcDatabase->createNpc(*variant);
    if (!npcResult) return std::unexpected(std::move(npcResult.error()));
    auto npc = std::move(*npcResult);
    auto done = m_universe.executeForClient(connectionId, [&](WorldServer& world, std::shared_ptr<Player> const& player) -> std::expected<void, StarException> {
        auto aimPosition = player->aimPosition();
        if (!aimPosition) return std::unexpected(std::move(aimPosition.error()));
        npc->setPosition((*aimPosition));
        return world.addEntity(npc);
      });

    if (!done) return std::unexpected(std::move(done.error()));
    return *done ? String() : String("Invalid client state");
  
  }();
  if (result) return std::move(*result);
  auto const& exception = result.error();
  {
    auto logged = m_root.logger().warnException(strf("Could not spawn NPC of species '{}'", arguments.empty() ? String() : arguments[0]), ExceptionDiagnostic(exception));
    if (!logged) return std::unexpected(std::move(logged.error()));
    return strf("Could not spawn NPC of species '{}'", argumentString);
    }

}

std::expected<String, StarException> CommandProcessor::spawnVehicle(ConnectionId connectionId, String const& argumentString) {
  auto adminCheckError = adminCheck(connectionId, "spawn vehicles");
  if (!adminCheckError) return std::unexpected(std::move(adminCheckError.error()));
  if (*adminCheckError) return **adminCheckError;

  auto result = [&]() -> std::expected<String, StarException> {
    auto vehicleDatabaseResult = m_root.vehicleDatabase();
    if (!vehicleDatabaseResult) return std::unexpected(std::move(vehicleDatabaseResult.error()));
    auto vehicleDatabase = std::move(*vehicleDatabaseResult);
    auto parsedArguments = m_parser.tokenizeToStringList(argumentString);
  if (!parsedArguments) return std::unexpected(std::move(parsedArguments.error()));
  auto arguments = std::move(*parsedArguments);
  auto requiredArgument = arguments.at(0);
  if (!requiredArgument) return std::unexpected(std::move(requiredArgument.error()));

    std::shared_ptr<Vehicle> vehicle;

    String name = arguments[0];

    Json parameters = JsonObject();
    if (arguments.size() >= 2) {
      auto parametersJson = Json::parse(arguments[1]);
      if (!parametersJson) return std::unexpected(std::move(parametersJson.error()));
      auto parametersObject = parametersJson->toObject();
      if (!parametersObject) return std::unexpected(std::move(parametersObject.error()));
      parameters = Json(*parametersObject);
    }

    auto created = vehicleDatabase->create(name, parameters);
    if (!created) return std::unexpected(std::move(created.error()));
    vehicle = std::move(*created);
    auto done = m_universe.executeForClient(connectionId,
        [&](WorldServer& world, std::shared_ptr<Player> const& player) -> std::expected<void, StarException> {
          auto aimPosition = player->aimPosition();
          if (!aimPosition) return std::unexpected(std::move(aimPosition.error()));
          vehicle->setPosition((*aimPosition));
          return world.addEntity(std::move(vehicle));
        });

    if (!done) return std::unexpected(std::move(done.error()));
    return *done ? String() : String("Invalid client state");
  
  }();
  if (result) return std::move(*result);
  auto const& exception = result.error();
  {
    auto logged = m_root.logger().warnException("Could not spawn vehicle", ExceptionDiagnostic(exception));
    if (!logged) return std::unexpected(std::move(logged.error()));
    return strf("Could not spawn vehicle");
    }

}

std::expected<String, StarException> CommandProcessor::spawnStagehand(ConnectionId connectionId, String const& argumentString) {
  auto adminCheckError = adminCheck(connectionId, "spawn stagehands");
  if (!adminCheckError) return std::unexpected(std::move(adminCheckError.error()));
  if (*adminCheckError) return **adminCheckError;

  StringList arguments;
  auto result = [&]() -> std::expected<String, StarException> {
    auto parsedArguments = m_parser.tokenizeToStringList(argumentString);
  if (!parsedArguments) return std::unexpected(std::move(parsedArguments.error()));
  arguments = std::move(*parsedArguments);
  auto requiredArgument = arguments.at(0);
  if (!requiredArgument) return std::unexpected(std::move(requiredArgument.error()));

    auto stagehandDatabaseResult = m_root.stagehandDatabase();
    if (!stagehandDatabaseResult) return std::unexpected(std::move(stagehandDatabaseResult.error()));
    auto stagehandDatabase = std::move(*stagehandDatabaseResult);

    Json parameters = JsonObject();
    if (arguments.size() >= 2) {
      auto parametersJson = Json::parse(arguments[1]);
      if (!parametersJson) return std::unexpected(std::move(parametersJson.error()));
      auto parametersObject = parametersJson->toObject();
      if (!parametersObject) return std::unexpected(std::move(parametersObject.error()));
      parameters = Json(*parametersObject);
    }

    auto created = stagehandDatabase->createStagehand(arguments[0], parameters);
    if (!created) return std::unexpected(std::move(created.error()));
    auto stagehand = std::move(*created);
    auto done = m_universe.executeForClient(connectionId, [&](WorldServer& world, std::shared_ptr<Player> player) -> std::expected<void, StarException> {
        auto aimPosition = player->aimPosition();
        if (!aimPosition) return std::unexpected(std::move(aimPosition.error()));
        stagehand->setPosition((*aimPosition));
        return world.addEntity(stagehand);
      });

    if (!done) return std::unexpected(std::move(done.error()));
    return *done ? String() : String("Invalid client state");
  
  }();
  if (result) return std::move(*result);
  auto const& exception = result.error();
  {
    auto logged = m_root.logger().warnException(strf("Could not spawn Stagehand of type '{}'", arguments.empty() ? String() : arguments[0]), ExceptionDiagnostic(exception));
    if (!logged) return std::unexpected(std::move(logged.error()));
    return strf("Could not spawn Stagehand of type '{}'", argumentString);
    }

}

std::expected<String, StarException> CommandProcessor::clearStagehand(ConnectionId connectionId, String const&) {
  auto adminCheckError = adminCheck(connectionId, "remove stagehands");
  if (!adminCheckError) return std::unexpected(std::move(adminCheckError.error()));
  if (*adminCheckError) return **adminCheckError;

  unsigned removed = 0;
  auto done = m_universe.executeForClient(connectionId,
      [&](WorldServer& world, std::shared_ptr<Player> player) -> std::expected<void, StarException> {
        auto aimPosition = player->aimPosition();
        if (!aimPosition) return std::unexpected(std::move(aimPosition.error()));
        auto queryRect = RectF::withCenter((*aimPosition), Vec2F{2, 2});
        auto stagehands = world.query<Stagehand>(queryRect);
        if (!stagehands) return std::unexpected(std::move(stagehands.error()));
        for (auto stagehand : *stagehands) {
          auto removedEntity = world.removeEntity(stagehand->entityId(), true);
          if (!removedEntity) return std::unexpected(std::move(removedEntity.error()));
          ++removed;
        }
        return {};
      });
  if (!done) return std::unexpected(std::move(done.error()));
  return *done ? strf("Removed {} stagehands", removed) : String("Invalid client state");
}

std::expected<String, StarException> CommandProcessor::spawnLiquid(ConnectionId connectionId, String const& argumentString) {
  auto adminCheckError = adminCheck(connectionId, "spawn liquid");
  if (!adminCheckError) return std::unexpected(std::move(adminCheckError.error()));
  if (*adminCheckError) return **adminCheckError;

  StringList arguments;
  auto result = [&]() -> std::expected<String, StarException> {
    auto parsedArguments = m_parser.tokenizeToStringList(argumentString);
  if (!parsedArguments) return std::unexpected(std::move(parsedArguments.error()));
  arguments = std::move(*parsedArguments);
  auto requiredArgument = arguments.at(0);
  if (!requiredArgument) return std::unexpected(std::move(requiredArgument.error()));

    auto liquidsDatabaseResult = m_root.liquidsDatabase();
    if (!liquidsDatabaseResult) return std::unexpected(std::move(liquidsDatabaseResult.error()));
    auto liquidsDatabase = std::move(*liquidsDatabaseResult);

    if (!liquidsDatabase->isLiquidName(arguments[0]))
      return strf("No such liquid {}", arguments[0]);

    auto liquidResult = liquidsDatabase->liquidId(arguments[0]);
    if (!liquidResult) return std::unexpected(std::move(liquidResult.error()));
    LiquidId liquid = *liquidResult;

    float quantity = 1.0f;
    if (arguments.size() > 1) {
      if (auto maybeQuantity = maybeLexicalCast<float>(arguments[1]))
        quantity = *maybeQuantity;
      else
        return strf("Could not parse quantity value '{}'", arguments[1]);
    }

    auto done = m_universe.executeForClient(connectionId, [&](WorldServer& world, std::shared_ptr<Player> const& player) -> std::expected<void, StarException> {
        auto aimPosition = player->aimPosition();
        if (!aimPosition) return std::unexpected(std::move(aimPosition.error()));
        auto modified = world.modifyTile(Vec2I((*aimPosition).floor()), PlaceLiquid{liquid, quantity}, true);
        if (!modified) return std::unexpected(std::move(modified.error()));
        return {};
      });
    if (!done) return std::unexpected(std::move(done.error()));
    return *done ? String() : String("Invalid client state");

  
  }();
  if (result) return std::move(*result);
  auto const& exception = result.error();
  {
    auto logged = m_root.logger().warnException(strf("Could not spawn liquid '{}'", arguments.empty() ? String() : arguments[0]), ExceptionDiagnostic(exception));
    if (!logged) return std::unexpected(std::move(logged.error()));
    return "Could not spawn liquid.";
    }

}

std::expected<String, StarException> CommandProcessor::kick(ConnectionId connectionId, String const& argumentString) {
  auto adminCheckError = adminCheck(connectionId, "kick a user");
  if (!adminCheckError) return std::unexpected(std::move(adminCheckError.error()));
  if (*adminCheckError) return **adminCheckError;

  auto parsedArguments = m_parser.tokenizeToStringList(argumentString);
  if (!parsedArguments) return std::unexpected(std::move(parsedArguments.error()));
  auto arguments = std::move(*parsedArguments);

  if (arguments.empty())
    return "No player specified";

  auto toKickResult = playerCidFromCommand(arguments[0], m_universe);
  if (!toKickResult) return std::unexpected(std::move(toKickResult.error()));
  auto toKick = *toKickResult;
  if (!toKick)
    return strf("No user with specifier {} found.", arguments[0]);

  // Like IRC, if only the nick is passed then the nick is used as the reason
  if (arguments.size() == 1) {
    auto nick = m_universe.clientNick(*toKick);
    if (!nick) return std::unexpected(std::move(nick.error()));
    arguments.append(*nick);
  }

  auto disconnected = m_universe.disconnectClient(*toKick, arguments[1]);
  if (!disconnected) return std::unexpected(std::move(disconnected.error()));

  return strf("Successfully kicked user with specifier {}. ConnectionId: {}. Reason given: {}",
      arguments[0],
      outputOptional(toKick),
      arguments[1]);
}

std::expected<String, StarException> CommandProcessor::ban(ConnectionId connectionId, String const& argumentString) {
  auto adminCheckError = adminCheck(connectionId, "ban a user");
  if (!adminCheckError) return std::unexpected(std::move(adminCheckError.error()));
  if (*adminCheckError) return **adminCheckError;

  auto parsedArguments = m_parser.tokenizeToStringList(argumentString);
  if (!parsedArguments) return std::unexpected(std::move(parsedArguments.error()));
  auto arguments = std::move(*parsedArguments);

  if (arguments.empty())
    return "No player specified";

  auto toKickResult = playerCidFromCommand(arguments[0], m_universe);
  if (!toKickResult) return std::unexpected(std::move(toKickResult.error()));
  auto toKick = *toKickResult;
  if (!toKick)
    return strf("No user with specifier {} found.", arguments[0]);

  String reason = arguments[0];
  if (arguments.size() < 2) {
    auto nick = m_universe.clientNick(*toKick);
    if (!nick) return std::unexpected(std::move(nick.error()));
    reason = *nick;
  }
  else
    reason = arguments[1];

  std::pair<bool, bool> type = {true, true};

  if (arguments.size() >= 3) {
    if (arguments[2] == "ip") {
      type = {true, false};
    } else if (arguments[2] == "uuid") {
      type = {false, true};
    } else if (arguments[2] == "both") {
      type = {true, true};
    } else {
      return strf("Invalid argument {} passed as ban type to /ban.  Options are ip, uuid, or both.", arguments[2]);
    }
  }

  std::optional<int> banTime;
  if (arguments.size() == 4) {
    auto parsedTime = lexicalCast<int>(arguments[3]);
    if (!parsedTime) {
      if (parsedTime.error().is<BadLexicalCast>()) return strf("Invalid argument {} passed as ban time to /ban.", arguments[3]);
      return std::unexpected(std::move(parsedTime.error()));
    }
    banTime = *parsedTime;
  }

  auto banned = m_universe.banUser(*toKick, reason, type, banTime);
  if (!banned) return std::unexpected(std::move(banned.error()));

  return strf("Successfully kicked user with specifier {}. ConnectionId: {}. Reason given: {}",
      arguments[0], outputOptional(toKick), reason);
}

std::expected<String, StarException> CommandProcessor::unbanIp(ConnectionId connectionId, String const& argumentString) {
  auto adminCheckError = adminCheck(connectionId, "unban a user");
  if (!adminCheckError) return std::unexpected(std::move(adminCheckError.error()));
  if (*adminCheckError) return **adminCheckError;

  auto parsedArguments = m_parser.tokenizeToStringList(argumentString);
  if (!parsedArguments) return std::unexpected(std::move(parsedArguments.error()));
  auto arguments = std::move(*parsedArguments);

  if (arguments.empty())
    return "No IP specified";

  auto success = m_universe.unbanIp(arguments[0]);
  if (!success) return std::unexpected(std::move(success.error()));

  if (*success)
    return strf("Successfully removed IP {} from ban list", arguments[0]);
  else
    return strf("'{}' is not a valid IP or was not found in the bans list", arguments[0]);
}

std::expected<String, StarException> CommandProcessor::unbanUuid(ConnectionId connectionId, String const& argumentString) {
  auto adminCheckError = adminCheck(connectionId, "unban a user");
  if (!adminCheckError) return std::unexpected(std::move(adminCheckError.error()));
  if (*adminCheckError) return **adminCheckError;

  auto parsedArguments = m_parser.tokenizeToStringList(argumentString);
  if (!parsedArguments) return std::unexpected(std::move(parsedArguments.error()));
  auto arguments = std::move(*parsedArguments);

  if (arguments.empty())
    return "No UUID specified";

  auto success = m_universe.unbanUuid(arguments[0]);
  if (!success) return std::unexpected(std::move(success.error()));

  if (*success)
    return strf("Successfully removed UUID {} from ban list", arguments[0]);
  else
    return strf("'{}' is not a valid UUID or was not found in the bans list", arguments[0]);
}

std::expected<String, StarException> CommandProcessor::list(ConnectionId connectionId, String const&) {
  auto adminCheckError = adminCheck(connectionId, "list clients");
  if (!adminCheckError) return std::unexpected(std::move(adminCheckError.error()));
  if (*adminCheckError) return **adminCheckError;

  StringList res;

  auto assetsResult = m_root.assets();
  if (!assetsResult) return std::unexpected(std::move(assetsResult.error()));
  auto assets = std::move(*assetsResult);
  for (auto cid : m_universe.clientIds()) {
    auto nick = m_universe.clientNick(cid);
    if (!nick) return std::unexpected(std::move(nick.error()));
    res.append(strf("${} : {} : ${}", cid, *nick, m_universe.uuidForClient(cid)->hex()));
  }

  return res.join("\n");
}

std::expected<String, StarException> CommandProcessor::clientCoordinate(ConnectionId connectionId, String const& argumentString) {
  ConnectionId targetClientId = connectionId;
  String targetLabel = "Your";
  auto parsedArguments = m_parser.tokenizeToStringList(argumentString);
  if (!parsedArguments) return std::unexpected(std::move(parsedArguments.error()));
  auto arguments = std::move(*parsedArguments);
  auto permission = adminCheck(connectionId, "find other players");
  if (!permission) return std::unexpected(std::move(permission.error()));
  if (!*permission) {
    if (arguments.size() > 0) {
      auto cidResult = playerCidFromCommand(arguments[0], m_universe);
      if (!cidResult) return std::unexpected(std::move(cidResult.error()));
      auto cid = *cidResult;
      if (!cid)
        return strf("No user with specifier {} found.", arguments[0]);
      targetClientId = *cid;
      targetLabel = strf("Client {}'s", arguments[0]);
    }
  }

  if (targetClientId) {
    auto worldId = m_universe.clientWorld(targetClientId);
    if (!worldId) return std::unexpected(std::move(worldId.error()));
    auto world = printWorldId(*worldId);
    if (!world) return std::unexpected(std::move(world.error()));
    return strf("{} current location is {}", targetLabel, *world);
  } else {
    return "";
  }
}

std::expected<String, StarException> CommandProcessor::serverReload(ConnectionId connectionId, String const&) {
  auto adminCheckError = adminCheck(connectionId, "trigger root reload");
  if (!adminCheckError) return std::unexpected(std::move(adminCheckError.error()));
  if (*adminCheckError) return **adminCheckError;

  auto& root = m_root;
  auto reloaded = root.reload();
  if (!reloaded) return std::unexpected(std::move(reloaded.error()));
  auto loaded = root.fullyLoad();
  if (!loaded) return std::unexpected(std::move(loaded.error()));
  return "";
}

std::expected<String, StarException> CommandProcessor::eval(ConnectionId connectionId, String const& lua) {
  auto localCheckError = localCheck(connectionId, "execute server script");
  if (!localCheckError) return std::unexpected(std::move(localCheckError.error()));
  if (*localCheckError) return **localCheckError;

  auto adminCheckError = adminCheck(connectionId, "execute server script");
  if (!adminCheckError) return std::unexpected(std::move(adminCheckError.error()));
  if (*adminCheckError) return **adminCheckError;

  if (!m_scriptComponent.context()) return std::unexpected(LuaException("Lua script component is not initialized"));
  auto value = m_scriptComponent.context()->eval(lua);
  if (!value) return std::unexpected(std::move(value.error()));
  return printLuaValue(*value);
}

std::expected<String, StarException> CommandProcessor::entityEval(ConnectionId connectionId, String const& lua) {
  auto localCheckError = localCheck(connectionId, "execute server entity script");
  if (!localCheckError) return std::unexpected(std::move(localCheckError.error()));
  if (*localCheckError) return **localCheckError;

  auto adminCheckError = adminCheck(connectionId, "execute server entity script");
  if (!adminCheckError) return std::unexpected(std::move(adminCheckError.error()));
  if (*adminCheckError) return **adminCheckError;

  String message;
  auto done = m_universe.executeForClient(connectionId,
      [&lua, &message](WorldServer& world, std::shared_ptr<Player> const& player) -> std::expected<void, StarException> {
        auto aimPosition = player->aimPosition();
        if (!aimPosition) return std::unexpected(std::move(aimPosition.error()));
        auto queryRect = RectF::withCenter((*aimPosition), Vec2F{2, 2});
        auto query = world.query<ScriptedEntity>(queryRect);
        if (!query) return std::unexpected(std::move(query.error()));
        auto entities = std::move(*query);
        if (entities.empty()) {
          message = "Could not find scripted entity at cursor";
          return {};
        }

        std::shared_ptr<ScriptedEntity> targetEntity;
        for (auto const& entity : entities) {
          if (!targetEntity
              || vmagSquared(entity->position() - (*aimPosition))
                  < vmagSquared(targetEntity->position() - (*aimPosition)))
            targetEntity = entity;
        }

        auto res = targetEntity->evalScript(lua);
        if (!res) return std::unexpected(std::move(res.error()));
        if (*res) {
          auto printed = printLuaValue(**res);
          if (!printed) return std::unexpected(std::move(printed.error()));
          message = std::move(*printed);
        } else {
          message = "Error evaluating script in entity context, check log";
        }
        return {};
      });

  if (!done) return std::unexpected(std::move(done.error()));
  return *done ? message : String("failed to do entity eval");
}

std::expected<String, StarException> CommandProcessor::enableSpawning(ConnectionId connectionId, String const&) {
  auto adminCheckError = adminCheck(connectionId, "enable world spawning");
  if (!adminCheckError) return std::unexpected(std::move(adminCheckError.error()));
  if (*adminCheckError) return **adminCheckError;

  auto done = m_universe.executeForClient(
      connectionId, [](WorldServer& world, std::shared_ptr<Player> const&) -> std::expected<void, StarException> { world.setSpawningEnabled(true); return {}; });
  if (!done) return std::unexpected(std::move(done.error()));
  return *done ? String("enabled monster spawning") : String("enabling monster spawning failed");
}

std::expected<String, StarException> CommandProcessor::disableSpawning(ConnectionId connectionId, String const&) {
  auto adminCheckError = adminCheck(connectionId, "disable world spawning");
  if (!adminCheckError) return std::unexpected(std::move(adminCheckError.error()));
  if (*adminCheckError) return **adminCheckError;

  auto done = m_universe.executeForClient(
      connectionId, [](WorldServer& world, std::shared_ptr<Player> const&) -> std::expected<void, StarException> { world.setSpawningEnabled(false); return {}; });
  if (!done) return std::unexpected(std::move(done.error()));
  return *done ? String("disabled monster spawning") : String("disabling monster spawning failed");
}

std::expected<String, StarException> CommandProcessor::placeDungeon(ConnectionId connectionId, String const& argumentString) {
  auto adminCheckError = adminCheck(connectionId, "place dungeons");
  if (!adminCheckError) return std::unexpected(std::move(adminCheckError.error()));
  if (*adminCheckError) return **adminCheckError;

  auto parsedArguments = m_parser.tokenizeToStringList(argumentString);
  if (!parsedArguments) return std::unexpected(std::move(parsedArguments.error()));
  auto arguments = std::move(*parsedArguments);
  auto requiredArgument = arguments.at(0);
  if (!requiredArgument) return std::unexpected(std::move(requiredArgument.error()));
  String dungeonName = arguments[0];

  std::optional<Vec2I> targetPosition;
  if (arguments.size() > 1) {
    auto pos = arguments[1].split(",", 1);
    if (!pos) return std::unexpected(std::move(pos.error()));
    auto xText = pos->at(0);
    if (!xText) return std::unexpected(std::move(xText.error()));
    auto yText = pos->at(1);
    if (!yText) return std::unexpected(std::move(yText.error()));
    auto x = lexicalCast<int>(xText->get());
    if (!x) return std::unexpected(std::move(x.error()));
    auto y = lexicalCast<int>(yText->get());
    if (!y) return std::unexpected(std::move(y.error()));
    targetPosition = Vec2I(*x, *y);
  }

  auto done = m_universe.executeForClient(connectionId,
      [dungeonName, targetPosition](WorldServer& world, std::shared_ptr<Player> const& player) -> std::expected<void, StarException> {
        auto aimPosition = player->aimPosition();
        if (!aimPosition) return std::unexpected(std::move(aimPosition.error()));
        auto placed = world.placeDungeon(dungeonName, targetPosition.value_or(Vec2I::floor((*aimPosition))), true);
        if (!placed) return std::unexpected(std::move(placed.error()));
        return {};
      });

  if (!done) return std::unexpected(std::move(done.error()));
  return *done ? String() : "Unable to place dungeon " + dungeonName;
}

std::expected<String, StarException> CommandProcessor::setUniverseFlag(ConnectionId connectionId, String const& argumentString) {
  auto adminCheckError = adminCheck(connectionId, "set universe flags");
  if (!adminCheckError) return std::unexpected(std::move(adminCheckError.error()));
  if (*adminCheckError) return **adminCheckError;

  auto parsedArguments = m_parser.tokenizeToStringList(argumentString);
  if (!parsedArguments) return std::unexpected(std::move(parsedArguments.error()));
  auto arguments = std::move(*parsedArguments);
  auto requiredArgument = arguments.at(0);
  if (!requiredArgument) return std::unexpected(std::move(requiredArgument.error()));
  String flag = arguments[0];
  auto set = m_universe.universeSettings()->setFlag(flag);
  if (!set) return std::unexpected(std::move(set.error()));

  return "set universe flag " + flag;
}

std::expected<String, StarException> CommandProcessor::resetUniverseFlags(ConnectionId connectionId, String const&) {
  auto adminCheckError = adminCheck(connectionId, "reset universe flags");
  if (!adminCheckError) return std::unexpected(std::move(adminCheckError.error()));
  if (*adminCheckError) return **adminCheckError;

  m_universe.universeSettings()->resetFlags();
  return "universe flags reset!";
}

std::expected<String, StarException> CommandProcessor::addBiomeRegion(ConnectionId connectionId, String const& argumentString) {
  auto adminCheckError = adminCheck(connectionId, "add biome regions");
  if (!adminCheckError) return std::unexpected(std::move(adminCheckError.error()));
  if (*adminCheckError) return **adminCheckError;

  auto parsedArguments = m_parser.tokenizeToStringList(argumentString);
  if (!parsedArguments) return std::unexpected(std::move(parsedArguments.error()));
  auto arguments = std::move(*parsedArguments);
  auto requiredArgument = arguments.at(1);
  if (!requiredArgument) return std::unexpected(std::move(requiredArgument.error()));

  String biomeName = arguments[0];
  auto widthResult = lexicalCast<int>(arguments[1]);
  if (!widthResult) return std::unexpected(std::move(widthResult.error()));
  int width = *widthResult;

  String subBlockSelector = "largeClumps";
  if (arguments.size() > 2)
    subBlockSelector = arguments[2];

  auto done = m_universe.executeForClient(connectionId,
      [biomeName, width, subBlockSelector](WorldServer& world, std::shared_ptr<Player> const& player) -> std::expected<void, StarException> {
        auto aimPosition = player->aimPosition();
        if (!aimPosition) return std::unexpected(std::move(aimPosition.error()));
        return world.addBiomeRegion(Vec2I::floor((*aimPosition)), biomeName, subBlockSelector, width);
      });

  if (!done) return std::unexpected(std::move(done.error()));
  return *done ? strf("added region of biome {} with width {}", biomeName, width) : String("failed to add biome region");
}

std::expected<String, StarException> CommandProcessor::expandBiomeRegion(ConnectionId connectionId, String const& argumentString) {
  auto adminCheckError = adminCheck(connectionId, "expand biome regions");
  if (!adminCheckError) return std::unexpected(std::move(adminCheckError.error()));
  if (*adminCheckError) return **adminCheckError;

  auto parsedArguments = m_parser.tokenizeToStringList(argumentString);
  if (!parsedArguments) return std::unexpected(std::move(parsedArguments.error()));
  auto arguments = std::move(*parsedArguments);
  auto requiredArgument = arguments.at(0);
  if (!requiredArgument) return std::unexpected(std::move(requiredArgument.error()));

  auto newWidthResult = lexicalCast<int>(arguments[0]);
  if (!newWidthResult) return std::unexpected(std::move(newWidthResult.error()));
  int newWidth = *newWidthResult;

  auto done = m_universe.executeForClient(connectionId,
      [newWidth](WorldServer& world, std::shared_ptr<Player> const& player) -> std::expected<void, StarException> {
        auto aimPosition = player->aimPosition();
        if (!aimPosition) return std::unexpected(std::move(aimPosition.error()));
        return world.expandBiomeRegion(Vec2I::floor((*aimPosition)), newWidth);
      });

  if (!done) return std::unexpected(std::move(done.error()));
  return *done ? strf("expanded region to width {}", newWidth) : String("failed to expand biome region");
}

std::expected<String, StarException> CommandProcessor::updatePlanetType(ConnectionId connectionId, String const& argumentString) {
  auto adminCheckError = adminCheck(connectionId, "update planet type");
  if (!adminCheckError) return std::unexpected(std::move(adminCheckError.error()));
  if (*adminCheckError) return **adminCheckError;

  auto parsedArguments = m_parser.tokenizeToStringList(argumentString);
  if (!parsedArguments) return std::unexpected(std::move(parsedArguments.error()));
  auto arguments = std::move(*parsedArguments);
  auto requiredArgument = arguments.at(2);
  if (!requiredArgument) return std::unexpected(std::move(requiredArgument.error()));

  auto coordinateResult = CelestialCoordinate::create(Json(arguments[0]));
  if (!coordinateResult) return std::unexpected(std::move(coordinateResult.error()));
  auto coordinate = *coordinateResult;
  auto newType = arguments[1];
  auto weatherBiome = arguments[2];

  auto done = m_universe.updatePlanetType(coordinate, newType, weatherBiome);

  if (!done) return std::unexpected(std::move(done.error()));
  return *done ? strf("set planet at {} to type {} weatherBiome {}", coordinate, newType, weatherBiome) : String("failed to update planet type");
}

std::expected<String, StarException> CommandProcessor::setWeather(ConnectionId connectionId, String const& argumentString) {
  auto permission = adminCheck(connectionId, "set weather");
  if (!permission) return std::unexpected(std::move(permission.error()));
  if (*permission) return **permission;
  auto parsed = m_parser.tokenizeToStringList(argumentString);
  if (!parsed) return std::unexpected(std::move(parsed.error()));
  auto arguments = std::move(*parsed);
  if (arguments.empty()) {
    StringList list;
    auto done = m_universe.executeForClient(connectionId, [&list](WorldServer& world, std::shared_ptr<Player> const& player) -> std::expected<void, StarException> {
      auto weather = world.weatherList(player->position());
      if (!weather) return std::unexpected(std::move(weather.error()));
      list = std::move(*weather);
      return {};
    });
    if (!done) return std::unexpected(std::move(done.error()));
    return *done ? strf("weathers: {}", list.join(", ")) : String("failed to query weather");
  }
  String weatherName = arguments[0];
  bool force = false;
  CelestialCoordinate coordinate;
  if (arguments.size() >= 2) {
    if (arguments[1] == "force") {
      force = true;
      if (arguments.size() >= 3) {
        auto parsedCoordinate = CelestialCoordinate::create(Json(arguments[2]));
        if (!parsedCoordinate) return std::unexpected(std::move(parsedCoordinate.error()));
        coordinate = *parsedCoordinate;
      }
    } else {
      auto parsedCoordinate = CelestialCoordinate::create(Json(arguments[1]));
      if (!parsedCoordinate) return std::unexpected(std::move(parsedCoordinate.error()));
      coordinate = *parsedCoordinate;
    }
  }
  std::expected<bool, StarException> done;
  if (coordinate.isNull()) {
    done = m_universe.executeForClient(connectionId, [weatherName, force](WorldServer& world, std::shared_ptr<Player> const& player) -> std::expected<void, StarException> { return world.setWeather(player->position(), weatherName, force); });
  } else {
    done = m_universe.setWeather(coordinate, weatherName, force);
  }
  if (!done) return std::unexpected(std::move(done.error()));
  return *done ? (coordinate.isNull() ? strf("set weather to {}{}", weatherName, force ? std::string_view(" (forced)") : std::string_view()) : strf("set weather for {} to {}{}", coordinate, weatherName, force ? std::string_view(" (forced)") : std::string_view())) : String("failed to set weather");
}


std::expected<String, StarException> CommandProcessor::setEnvironmentBiome(ConnectionId connectionId, String const&) {
  auto adminCheckError = adminCheck(connectionId, "update layer environment biome");
  if (!adminCheckError) return std::unexpected(std::move(adminCheckError.error()));
  if (*adminCheckError) return **adminCheckError;

  auto done = m_universe.executeForClient(connectionId,
      [](WorldServer& world, std::shared_ptr<Player> const& player) -> std::expected<void, StarException> {
        auto aimPosition = player->aimPosition();
        if (!aimPosition) return std::unexpected(std::move(aimPosition.error()));
        return world.setLayerEnvironmentBiome(Vec2I::floor((*aimPosition)));
      });

  if (!done) return std::unexpected(std::move(done.error()));
  return *done ? String("set environment biome for world layer") : String("failed to set environment biome");
}

std::expected<std::optional<ConnectionId>, StarException> CommandProcessor::playerCidFromCommand(String const& player, UniverseServer& universe) {
  constexpr std::string_view UsernamePrefix = "@";
  constexpr std::string_view CidPrefix = "$";
  constexpr std::string_view UUIDPrefix = "$$";
  auto username = player.beginsWith(UsernamePrefix);
  if (!username) return std::unexpected(std::move(username.error()));
  if (*username) {
    auto name = player.substr(UsernamePrefix.size());
    if (!name) return std::unexpected(std::move(name.error()));
    return universe.findNick(*name);
  }
  auto uuidPrefix = player.beginsWith(UUIDPrefix);
  if (!uuidPrefix) return std::unexpected(std::move(uuidPrefix.error()));
  if (*uuidPrefix) {
    auto text = player.substr(UUIDPrefix.size());
    if (!text) return std::unexpected(std::move(text.error()));
    auto uuid = Uuid::create(*text);
    if (uuid) return universe.clientForUuid(*uuid);
    if (!uuid.error().is<UuidException>()) return std::unexpected(std::move(uuid.error()));
  } else {
    auto cidPrefix = player.beginsWith(CidPrefix);
    if (!cidPrefix) return std::unexpected(std::move(cidPrefix.error()));
    if (*cidPrefix) {
      auto text = player.substr(CidPrefix.size());
      if (!text) return std::unexpected(std::move(text.error()));
      auto cid = maybeLexicalCast<ConnectionId>(*text).value_or(ServerConnectionId);
      if (universe.isConnectedClient(cid)) return std::optional<ConnectionId>(cid);
    }
  }
  return universe.findNick(player);
}

std::expected<CaseInsensitiveStringMap<CommandProcessor::Command>, StarException> CommandProcessor::makeCommandMap() {
  CaseInsensitiveStringMap<Command> map;

  auto add = [&map](std::string_view cmd, Command func) -> std::expected<void, StarException> {
    auto key = CaseInsensitiveString::create(String(cmd));
    if (!key) return std::unexpected(std::move(key.error()));
    map[*key] = func;
    return {};
  };

  // Register all commands
  if (auto added = add("admin", &CommandProcessor::admin); !added) return std::unexpected(std::move(added.error()));
  if (auto added = add("serverdebug", &CommandProcessor::serverDebug); !added) return std::unexpected(std::move(added.error()));
  if (auto added = add("timewarp", &CommandProcessor::timewarp); !added) return std::unexpected(std::move(added.error()));
  if (auto added = add("timescale", &CommandProcessor::timescale); !added) return std::unexpected(std::move(added.error()));
  if (auto added = add("tickrate", &CommandProcessor::tickrate); !added) return std::unexpected(std::move(added.error()));
  if (auto added = add("settileprotection", &CommandProcessor::setTileProtection); !added) return std::unexpected(std::move(added.error()));
  if (auto added = add("setdungeonid", &CommandProcessor::setDungeonId); !added) return std::unexpected(std::move(added.error()));
  if (auto added = add("setspawnpoint", &CommandProcessor::setPlayerStart); !added) return std::unexpected(std::move(added.error()));
  if (auto added = add("spawnitem", &CommandProcessor::spawnItem); !added) return std::unexpected(std::move(added.error()));
  if (auto added = add("spawntreasure", &CommandProcessor::spawnTreasure); !added) return std::unexpected(std::move(added.error()));
  if (auto added = add("spawnmonster", &CommandProcessor::spawnMonster); !added) return std::unexpected(std::move(added.error()));
  if (auto added = add("spawnnpc", &CommandProcessor::spawnNpc); !added) return std::unexpected(std::move(added.error()));
  if (auto added = add("spawnstagehand", &CommandProcessor::spawnStagehand); !added) return std::unexpected(std::move(added.error()));
  if (auto added = add("clearstagehand", &CommandProcessor::clearStagehand); !added) return std::unexpected(std::move(added.error()));
  if (auto added = add("spawnvehicle", &CommandProcessor::spawnVehicle); !added) return std::unexpected(std::move(added.error()));
  if (auto added = add("spawnliquid", &CommandProcessor::spawnLiquid); !added) return std::unexpected(std::move(added.error()));
  if (auto added = add("pvp", &CommandProcessor::pvp); !added) return std::unexpected(std::move(added.error()));
  if (auto added = add("serverwhoami", &CommandProcessor::whoami); !added) return std::unexpected(std::move(added.error()));
  if (auto added = add("kick", &CommandProcessor::kick); !added) return std::unexpected(std::move(added.error()));
  if (auto added = add("ban", &CommandProcessor::ban); !added) return std::unexpected(std::move(added.error()));
  if (auto added = add("unbanip", &CommandProcessor::unbanIp); !added) return std::unexpected(std::move(added.error()));
  if (auto added = add("unbanuuid", &CommandProcessor::unbanUuid); !added) return std::unexpected(std::move(added.error()));
  if (auto added = add("list", &CommandProcessor::list); !added) return std::unexpected(std::move(added.error()));
  if (auto added = add("help", &CommandProcessor::help); !added) return std::unexpected(std::move(added.error()));
  if (auto added = add("warp", &CommandProcessor::warp); !added) return std::unexpected(std::move(added.error()));
  if (auto added = add("warprandom", &CommandProcessor::warpRandom); !added) return std::unexpected(std::move(added.error()));
  if (auto added = add("whereami", &CommandProcessor::clientCoordinate); !added) return std::unexpected(std::move(added.error()));
  if (auto added = add("whereis", &CommandProcessor::clientCoordinate); !added) return std::unexpected(std::move(added.error()));
  if (auto added = add("serverreload", &CommandProcessor::serverReload); !added) return std::unexpected(std::move(added.error()));
  if (auto added = add("eval", &CommandProcessor::eval); !added) return std::unexpected(std::move(added.error()));
  if (auto added = add("entityeval", &CommandProcessor::entityEval); !added) return std::unexpected(std::move(added.error()));
  if (auto added = add("enablespawning", &CommandProcessor::enableSpawning); !added) return std::unexpected(std::move(added.error()));
  if (auto added = add("disablespawning", &CommandProcessor::disableSpawning); !added) return std::unexpected(std::move(added.error()));
  if (auto added = add("placedungeon", &CommandProcessor::placeDungeon); !added) return std::unexpected(std::move(added.error()));
  if (auto added = add("setuniverseflag", &CommandProcessor::setUniverseFlag); !added) return std::unexpected(std::move(added.error()));
  if (auto added = add("resetuniverseflags", &CommandProcessor::resetUniverseFlags); !added) return std::unexpected(std::move(added.error()));
  if (auto added = add("addbiomeregion", &CommandProcessor::addBiomeRegion); !added) return std::unexpected(std::move(added.error()));
  if (auto added = add("expandbiomeregion", &CommandProcessor::expandBiomeRegion); !added) return std::unexpected(std::move(added.error()));
  if (auto added = add("updateplanettype", &CommandProcessor::updatePlanetType); !added) return std::unexpected(std::move(added.error()));
  if (auto added = add("setweather", &CommandProcessor::setWeather); !added) return std::unexpected(std::move(added.error()));
  if (auto added = add("setenvironmentbiome", &CommandProcessor::setEnvironmentBiome); !added) return std::unexpected(std::move(added.error()));

  return map;
}

const std::expected<CaseInsensitiveStringMap<CommandProcessor::Command>, StarException> CommandProcessor::s_commandMap = makeCommandMap();

std::expected<ServerCommandResult, StarException> CommandProcessor::handleCommand(ConnectionId connectionId, String const& command, String const& argumentString) {
  if (!s_commandMap) return std::unexpected(s_commandMap.error());
  auto key = CaseInsensitiveStringView::create(command);
  if (!key) return std::unexpected(std::move(key.error()));
  auto it = s_commandMap->find(*key);
  if (it != s_commandMap->end()) {
    auto response = ((*this).*it->second)(connectionId, argumentString);
    if (!response) return std::unexpected(std::move(response.error()));
    return ServerCommandResult(std::move(*response));
  }
  auto arguments = m_parser.tokenizeToStringList(argumentString);
  if (!arguments) return std::unexpected(std::move(arguments.error()));
  auto invoked = m_scriptComponent.invoke<std::variant<RpcPromise<Json>, LuaValue>>("command", command, connectionId, jsonFromStringList(*arguments));
  if (!invoked) return std::unexpected(std::move(invoked.error()));
  if (*invoked) {
    auto& result = **invoked;
    if (std::holds_alternative<RpcPromise<Json>>(result)) {
      auto& promise = std::get<RpcPromise<Json>>(result);
      auto wrapped = promise.wrap([](Json res) -> std::expected<String, StarException> {
        if (res.isType(Json::Type::String)) {
          auto text = res.stringPtr();
          if (!text) return std::unexpected(std::move(text.error()));
          return **text;
        }
        if (!res.isNull()) return res.repr(1, true);
        return String();
      });
      if (!wrapped) return std::unexpected(std::move(wrapped.error()));
      return ServerCommandResult(std::move(*wrapped));
    }
    auto printed = printLuaValue(std::get<LuaValue>(result));
    if (!printed) return std::unexpected(std::move(printed.error()));
    return ServerCommandResult(std::move(*printed));
  }
  return ServerCommandResult(strf("No such command {}", command));
}

std::expected<std::optional<String>, StarException> CommandProcessor::adminCheck(ConnectionId connectionId, String const& commandDescription) const {
  if (connectionId == ServerConnectionId) return std::optional<String>{};
  auto configResult = m_root.configuration();
  if (!configResult) return std::unexpected(std::move(configResult.error()));
  auto config = std::move(*configResult);
  auto allowJson = config->get("allowAdminCommands");
  if (!allowJson) return std::unexpected(std::move(allowJson.error()));
  auto allow = allowJson->toBool();
  if (!allow) return std::unexpected(std::move(allow.error()));
  if (!*allow) return std::optional<String>("Admin commands disabled on this server.");
  auto anyoneJson = config->get("allowAdminCommandsFromAnyone");
  if (!anyoneJson) return std::unexpected(std::move(anyoneJson.error()));
  auto anyone = anyoneJson->toBool();
  if (!anyone) return std::unexpected(std::move(anyone.error()));
  if (!*anyone) {
    auto admin = m_universe.isAdmin(connectionId);
    if (!admin) return std::unexpected(std::move(admin.error()));
    if (!*admin) return std::optional<String>(strf("Insufficient privileges to {}.", commandDescription));
  }
  return std::optional<String>{};
}

std::expected<std::optional<String>, StarException> CommandProcessor::localCheck(ConnectionId connectionId, String const& commandDescription) const {
  if (connectionId == ServerConnectionId) return std::optional<String>{};
  auto local = m_universe.isLocal(connectionId);
  if (!local) return std::unexpected(std::move(local.error()));
  if (!*local) return std::optional<String>(strf("The {} command can only be used locally.", commandDescription));
  return std::optional<String>{};
}

std::expected<LuaCallbacks, StarException> CommandProcessor::makeCommandCallbacks() {
  LuaCallbacks callbacks;
  auto registered = callbacks.registerCallbackWithSignature<std::expected<std::optional<String>, StarException>, ConnectionId, String>("adminCheck", std::bind(&CommandProcessor::adminCheck, std::ref(*this), std::placeholders::_1, std::placeholders::_2));
  if (!registered) return std::unexpected(std::move(registered.error()));
  return callbacks;
}

}

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
import star.message_types;
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
import star.io_device;

using ZSTD_CCtx = struct ZSTD_CCtx_s;
using ZSTD_DCtx = struct ZSTD_DCtx_s;
using ZSTD_DStream = ZSTD_DCtx;
using ZSTD_CStream = ZSTD_CCtx;
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

namespace {

template <typename T>
concept command_number = (std::integral<T> || std::floating_point<T>) && !std::same_as<T, bool>;

template <command_number T>
std::expected<std::optional<T>, StarException> number_argument(StringList const& arguments, std::size_t index) {
  if (index >= arguments.size()) return std::optional<T>{};
  return lexical_cast<T>(arguments[index]).transform([](T value) { return std::optional<T>(value); });
}

std::expected<Json, StarException> object_argument(StringList const& arguments, std::size_t index) {
  if (index >= arguments.size()) return JsonObject();
  auto parsed = Json::parse(arguments[index]);
  if (!parsed) return parsed;
  auto object = parsed->object_ptr();
  if (!object) return std::unexpected(std::move(object.error()));
  return parsed;
}

template <typename T>
concept aim_placeable = std::derived_from<T, Entity> && requires(T& entity, Vec2F const& position) {
  { entity.setPosition(position) } -> std::same_as<void>;
};

template <aim_placeable T>
std::expected<String, StarException> place_at_aim(UniverseServer& universe, ConnectionId connection_id, std::shared_ptr<T> entity) {
  auto done = universe.executeForClient(connection_id, [&entity](WorldServer& world, std::shared_ptr<Player> const& player) -> std::expected<void, StarException> {
    auto position = player->aimPosition();
    if (!position) return std::unexpected(std::move(position.error()));
    entity->setPosition(*position);
    return world.addEntity(std::move(entity));
  });
  if (!done) return std::unexpected(std::move(done.error()));
  return *done ? String() : String("Invalid client state");
}

} // namespace
CommandProcessor::CommandProcessor(ConstructionKey, Root& root, UniverseServer& universe)
  : m_root(root), m_universe(universe), m_script_component(root) {}

std::expected<std::shared_ptr<CommandProcessor>, StarException> CommandProcessor::create(Root& root, UniverseServer& universe) {
  auto processor = std::make_shared<CommandProcessor>(ConstructionKey{}, root, universe);
  auto assets = root.assets();
  if (!assets) return std::unexpected(std::move(assets.error()));
  auto config = (*assets)->json("/universe_server.config");
  if (!config) return std::unexpected(std::move(config.error()));
  auto lua_root = LuaRoot::create(root);
  if (!lua_root) return std::unexpected(std::move(lua_root.error()));
  processor->m_lua_root = std::move(*lua_root);
  auto pause = config->get_float("luaGcPause");
  if (!pause) return std::unexpected(std::move(pause.error()));
  auto step = config->get_float("luaGcStepMultiplier");
  if (!step) return std::unexpected(std::move(step.error()));
  processor->m_lua_root->tune_auto_garbage_collection(*pause, *step);
  auto universe_callbacks = LuaBindings::makeUniverseServerCallbacks(root, universe);
  if (!universe_callbacks) return std::unexpected(std::move(universe_callbacks.error()));
  auto celestial_callbacks = LuaBindings::make_celestial_callbacks(root, universe);
  if (!celestial_callbacks) return std::unexpected(std::move(celestial_callbacks.error()));
  auto universe_added = processor->m_script_component.add_callbacks("universe", *universe_callbacks);
  if (!universe_added) return std::unexpected(std::move(universe_added.error()));
  auto celestial_added = processor->m_script_component.add_callbacks("celestial", *celestial_callbacks);
  if (!celestial_added) return std::unexpected(std::move(celestial_added.error()));
  auto universe_thread_added = processor->m_script_component.add_thread_callbacks("universe", *universe_callbacks);
  if (!universe_thread_added) return std::unexpected(std::move(universe_thread_added.error()));
  auto celestial_thread_added = processor->m_script_component.add_thread_callbacks("celestial", *celestial_callbacks);
  if (!celestial_thread_added) return std::unexpected(std::move(celestial_thread_added.error()));
  auto command_callbacks = processor->make_command_callbacks();
  if (!command_callbacks) return std::unexpected(std::move(command_callbacks.error()));
  auto commands_added = processor->m_script_component.add_callbacks("CommandProcessor", std::move(*command_callbacks));
  if (!commands_added) return std::unexpected(std::move(commands_added.error()));
  auto script_json = config->get("commandProcessorScripts");
  if (!script_json) return std::unexpected(std::move(script_json.error()));
  auto scripts = json_to_string_list(*script_json);
  if (!scripts) return std::unexpected(std::move(scripts.error()));
  auto scripts_set = processor->m_script_component.set_scripts(std::move(*scripts));
  if (!scripts_set) return std::unexpected(std::move(scripts_set.error()));
  auto root_set = processor->m_script_component.set_lua_root(processor->m_lua_root);
  if (!root_set) return std::unexpected(std::move(root_set.error()));
  auto initialized = recover_lua_script_error(processor->m_script_component.init());
  if (!initialized) return std::unexpected(std::move(initialized.error()));
  return processor;
}

std::expected<void, StarException> CommandProcessor::close() {
  std::lock_guard locker(m_mutex);
  return recover_lua_script_error(m_script_component.uninit());
}

std::expected<ServerCommandResult, StarException> CommandProcessor::admin_command(String const& command, String const& argument_string) {
  std::lock_guard locker(m_mutex);
  return handle_command(server_connection_id, command, argument_string);
}

std::expected<ServerCommandResult, StarException> CommandProcessor::user_command(ConnectionId connection_id, String const& command, String const& argument_string) {
  std::lock_guard locker(m_mutex);
  if (connection_id == server_connection_id)
    return std::unexpected(StarException("CommandProcessor::userCommand called with server_connection_id"));
  return handle_command(connection_id, command, argument_string);
}

std::expected<String, StarException> CommandProcessor::help(ConnectionId connection_id, String const& argument_string) {
  auto arguments = split_shell_words(argument_string);
  if (!arguments) return std::unexpected(std::move(arguments.error()));
  auto assets = m_root.assets();
  if (!assets) return std::unexpected(std::move(assets.error()));
  auto config = (*assets)->json("/help.config");
  if (!config) return std::unexpected(std::move(config.error()));
  struct Section {
    String commands;
    String heading;
    bool admin_only;
  };
  static const Section sections[] = {
    {"basicCommands", "basicHelpText", false}, {"openSbCommands", "openSbHelpText", false},
    {"adminCommands", "adminHelpText", true}, {"debugCommands", "debugHelpText", true},
    {"openSbDebugCommands", "openSbDebugHelpText", true},
  };
  std::array<Json, std::size(sections)> groups;
  bool const has_topic = !arguments->empty();
  auto load_group = [&](std::size_t i) -> std::expected<void, StarException> {
    auto value = config->get(sections[i].commands);
    if (!value) return std::unexpected(std::move(value.error()));
    groups[i] = std::move(*value);
    return {};
  };
  if (has_topic) {
    for (std::size_t i = 0; i < groups.size(); ++i) {
      if (auto loaded = load_group(i); !loaded) return std::unexpected(std::move(loaded.error()));
      auto text = groups[i].opt_string((*arguments)[0]);
      if (!text) return std::unexpected(std::move(text.error()));
      if (*text) return std::move(**text);
    }
  }
  auto denied = admin_check(connection_id, {});
  if (!denied) return std::unexpected(std::move(denied.error()));
  String result;
  for (std::size_t i = 0; i < groups.size(); ++i) {
    auto const& section = sections[i];
    if (section.admin_only && *denied) break;
    if (!has_topic) {
      if (auto loaded = load_group(i); !loaded) return std::unexpected(std::move(loaded.error()));
    }
    auto object = groups[i].object_ptr();
    if (!object) return std::unexpected(std::move(object.error()));
    std::vector<std::string_view> names;
    names.reserve((*object)->size());
    std::size_t bytes = 1;
    for (auto const& name : (*object)->key_view()) {
      names.emplace_back(name.utf8());
      bytes += name.utf8().size();
    }
    std::ranges::sort(names);
    String description;
    description.reserve(bytes + (names.empty() ? 0 : 3 * (names.size() - 1)));
    description += "/";
    for (std::size_t j = 0; j < names.size(); ++j) {
      if (j) description += ", /";
      description += names[j];
    }
    auto heading = config->get_string(section.heading);
    if (!heading) return std::unexpected(std::move(heading.error()));
    std::array<std::pair<std::string_view, std::string_view>, 1> bindings{{{"commands", description.utf8()}}};
    auto formatted = Star::format_tags(heading->utf8(), bindings);
    if (!formatted) return std::unexpected(std::move(formatted.error()));
    result += *formatted;
    result += "\n";
  }
  auto help = groups.front().get_string("help");
  if (!help) return std::unexpected(std::move(help.error()));
  result += *help;
  return result;
}

std::expected<String, StarException> CommandProcessor::admin(ConnectionId connection_id, String const& argument_string) {
  auto config = m_root.configuration();
  if (!config) return std::unexpected(std::move(config.error()));
  auto arguments = split_shell_words(argument_string);
  if (!arguments) return std::unexpected(std::move(arguments.error()));
  ConnectionId target_client_id = connection_id;
  if (!arguments->empty()) {
    auto permission = admin_check(connection_id, "make user admin");
    if (!permission) return std::unexpected(std::move(permission.error()));
    if (*permission) return **permission;
    auto target = player_cid_from_command((*arguments)[0], m_universe);
    if (!target) return std::unexpected(std::move(target.error()));
    if (!*target) return Star::format("No user with specifier {} found.", (*arguments)[0]);
    target_client_id = **target;
  } else {
    auto can_become = m_universe.canBecomeAdmin(connection_id);
    if (!can_become) return std::unexpected(std::move(can_become.error()));
    if (!*can_become) {
      auto admin = m_universe.isAdmin(connection_id);
      if (!admin) return std::unexpected(std::move(admin.error()));
      if (!*admin) return "Insufficient privileges to make self admin.";
    }
  }
  if (target_client_id == server_connection_id) return "Invalid client state";
  auto allow_json = (*config)->get("allowAdminCommands");
  if (!allow_json) return std::unexpected(std::move(allow_json.error()));
  auto allow = allow_json->to_bool();
  if (!allow) return std::unexpected(std::move(allow.error()));
  if (!*allow) return "Admin commands disabled on this server.";
  auto was_admin = m_universe.isAdmin(target_client_id);
  if (!was_admin) return std::unexpected(std::move(was_admin.error()));
  auto set = m_universe.setAdmin(target_client_id, !*was_admin);
  if (!set) return std::unexpected(std::move(set.error()));
  auto nick = m_universe.client_nick(target_client_id);
  if (!nick) return std::unexpected(std::move(nick.error()));
  if (!*was_admin) return Star::format("Admin privileges now given to {}", *nick);
  return Star::format("Admin privileges taken away from {}", *nick);
}

std::expected<String, StarException> CommandProcessor::server_debug(ConnectionId connection_id, String const&) {
  auto local = m_universe.isLocal(connection_id);
  if (!local) return std::unexpected(std::move(local.error()));
  if (*local) return "Server is local, server debug already active";
  auto version = m_universe.clientConnectionVersion(connection_id);
  if (!version) return std::unexpected(std::move(version.error()));
  if (*version < 17) return "Client is not new enough to debug server";
  auto enabled = m_universe.serverDebug(connection_id);
  if (!enabled) return std::unexpected(std::move(enabled.error()));
  auto set = m_universe.setServerDebug(connection_id, !*enabled);
  if (!set) return std::unexpected(std::move(set.error()));
  return Star::format("Server debug {}", !*enabled ? std::string_view("enabled") : std::string_view("disabled"));
}

std::expected<String, StarException> CommandProcessor::pvp(ConnectionId connection_id, String const&) {
  auto was_pvp = m_universe.isPvp(connection_id);
  if (!was_pvp) return std::unexpected(std::move(was_pvp.error()));
  auto set = m_universe.setPvp(connection_id, !*was_pvp);
  if (!set) return std::unexpected(std::move(set.error()));
  auto now_pvp = m_universe.isPvp(connection_id);
  if (!now_pvp) return std::unexpected(std::move(now_pvp.error()));
  if (*now_pvp != *was_pvp) {
    auto nick = m_universe.client_nick(connection_id);
    if (!nick) return std::unexpected(std::move(nick.error()));
    auto sent = m_universe.admin_broadcast(*now_pvp ? Star::format("Player {} is now PVP", *nick) : Star::format("Player {} is a big wimp and is no longer PVP", *nick));
    if (!sent) return std::unexpected(std::move(sent.error()));
  }
  auto pvp = m_universe.isPvp(connection_id);
  if (!pvp) return std::unexpected(std::move(pvp.error()));
  return *pvp ? String("PVP active") : String("PVP inactive");
}

std::expected<String, StarException> CommandProcessor::whoami(ConnectionId connection_id, String const&) {
  auto nick = m_universe.client_nick(connection_id);
  if (!nick) return std::unexpected(std::move(nick.error()));
  auto admin = m_universe.isAdmin(connection_id);
  if (!admin) return std::unexpected(std::move(admin.error()));
  return Star::format("Server: You are {}. You are {}an Admin", *nick, *admin ? std::string_view() : std::string_view("not "));
}

std::expected<String, StarException> CommandProcessor::warp(ConnectionId connection_id, String const& argument_string) {
  auto result = [&]() -> std::expected<String, StarException> {
    auto action = parseWarpAction(argument_string);
    if (!action) return std::unexpected(std::move(action.error()));
    auto warped = m_universe.clientWarpPlayer(connection_id, *action);
    if (!warped) return std::unexpected(std::move(warped.error()));
    return "Lets do the space warp again";
  }();
  if (result) return std::move(*result);
  auto const& exception = result.error();
  auto logged = m_root.logger().warn_exception("Could not parse warp target", ExceptionDiagnostic(exception));
  if (!logged) return std::unexpected(std::move(logged.error()));
  return Star::format("Could not parse the argument {} as a warp target", argument_string);
}

std::expected<String, StarException> CommandProcessor::warp_random(ConnectionId connection_id, String const& type_name) {
  Vec2I size = {2, 2};
  auto celestial_database = m_universe.celestial_database();
  std::optional<CelestialCoordinate> target;
  auto valid_planet = [&](CelestialCoordinate const& planet) -> std::expected<bool, StarException> {
    auto parameters = celestial_database->parameters(planet);
    if (!parameters) return std::unexpected(std::move(parameters.error()));
    if (*parameters) {
      if (auto visitable = (*parameters)->visitable_parameters())
        return visitable->typeName == type_name;
    }
    return false;
  };
  while (!target) {
    RectI region = RectI::with_size(Vec2I(m_root.random().next_i32(), m_root.random().next_i32()), size);
    while (true) {
      auto loaded = celestial_database->scan_region_fully_loaded(region);
      if (!loaded) return std::unexpected(std::move(loaded.error()));
      if (*loaded) break;
      auto scanned = celestial_database->scan_systems(region);
      if (!scanned) return std::unexpected(std::move(scanned.error()));
    }
    auto systems = celestial_database->scan_systems(region);
    if (!systems) return std::unexpected(std::move(systems.error()));
    for (auto const& system : *systems) {
      auto planets = celestial_database->children(system);
      if (!planets) return std::unexpected(std::move(planets.error()));
      for (auto const& planet : *planets) {
        auto valid = valid_planet(planet);
        if (!valid) return std::unexpected(std::move(valid.error()));
        if (*valid) target = planet;
        if (!target) {
          auto moons = celestial_database->children(planet);
          if (!moons) return std::unexpected(std::move(moons.error()));
          for (auto const& moon : *moons) {
            auto valid_moon = valid_planet(moon);
            if (!valid_moon) return std::unexpected(std::move(valid_moon.error()));
            if (*valid_moon) { target = moon; break; }
          }
        }
      }
    }
    if (size.magnitude() > 1024) return "could not find a matching world";
    size *= 2;
  }
  auto warped = m_universe.clientWarpPlayer(connection_id, WarpToWorld(CelestialWorldId(*target)));
  if (!warped) return std::unexpected(std::move(warped.error()));
  return Star::format("warping to {}", *target);
}

std::expected<String, StarException> CommandProcessor::timewarp(ConnectionId, String const& arguments_string) {
  auto parsed_arguments = split_shell_words(arguments_string);
  if (!parsed_arguments) return std::unexpected(std::move(parsed_arguments.error()));
  auto& arguments = *parsed_arguments;
  if (arguments.empty())
    return "Not enough arguments to /timewarp";

  auto time = maybe_lexical_cast<double>(arguments[0]);
  if (!time)
    return Star::format("Could not parse the argument {} as a time adjustment", arguments[0]);
  if (*time == 0.0)
    return "You suck at time travel.";
  if (*time < 0.0 && (arguments.size() < 2 || arguments[1] != "please"))
    return "Great Scott! We can't go back in time!";
  m_universe.universeClock()->adjust_time(*time);
  return *time > 0.0 ? String("It's just a jump to the left...") : String("And then a step to the right...");
}

std::expected<String, StarException> CommandProcessor::timescale(ConnectionId, String const& arguments_string) {
  auto parsed_arguments = split_shell_words(arguments_string);
  if (!parsed_arguments) return std::unexpected(std::move(parsed_arguments.error()));
  auto& arguments = *parsed_arguments;

  if (arguments.empty())
    return Star::format("Current timescale is {:6.6f}x", m_root.timing().time_scale);

  auto parsed_scale = lexical_cast<float>(arguments[0]);
  if (!parsed_scale) return std::unexpected(std::move(parsed_scale.error()));
  float timescale = std::clamp(*parsed_scale, 0.001f, 32.0f);
  auto set = m_universe.setTimescale(timescale);
  if (!set) return std::unexpected(std::move(set.error()));
  return Star::format("Set timescale to {:6.6f}x", timescale);
}

std::expected<String, StarException> CommandProcessor::tickrate(ConnectionId, String const& arguments_string) {
  auto parsed_arguments = split_shell_words(arguments_string);
  if (!parsed_arguments) return std::unexpected(std::move(parsed_arguments.error()));
  auto& arguments = *parsed_arguments;

  if (arguments.empty())
    return Star::format("Current tick rate is {:4.2f}Hz", 1.0f / m_root.timing().server_time_step);

  auto parsed_rate = lexical_cast<float>(arguments[0]);
  if (!parsed_rate) return std::unexpected(std::move(parsed_rate.error()));
  float tick_rate = std::clamp(*parsed_rate, 5.f, 500.f);
  m_universe.setTickRate(tick_rate);
  return Star::format("Set tick rate to {:4.2f}Hz", tick_rate);
}

std::expected<String, StarException> CommandProcessor::set_tile_protection(ConnectionId connection_id, String const& argument_string) {
  auto parsed_arguments = split_shell_words(argument_string);
  if (!parsed_arguments) return std::unexpected(std::move(parsed_arguments.error()));
  auto& arguments = *parsed_arguments;

  if (arguments.size() < 2)
    return "Not enough arguments to /settileprotection. Use /settileprotection <dungeonId> <protected>";

  auto result = [&]() -> std::expected<String, StarException> {
    auto last = arguments.take_back();
    if (!last)
      return std::unexpected(std::move(last.error()));
    auto protected_json = Json::parse(*last);
    if (!protected_json)
      return std::unexpected(std::move(protected_json.error()));
    auto protected_value = protected_json->to_bool();
    if (!protected_value)
      return std::unexpected(std::move(protected_value.error()));
    bool is_protected = *protected_value;
    List<DungeonId> dungeon_ids;
    for (auto& banana : arguments) {
      auto sliced = banana.split("..");
      if (!sliced)
        return std::unexpected(std::move(sliced.error()));
      auto slices = std::move(*sliced);
      auto it = slices.begin();
      DungeonId previous = 0;
      while (it != slices.end()) {
        auto current_result = lexical_cast<DungeonId>(*it);
        if (!current_result)
          return std::unexpected(std::move(current_result.error()));
        DungeonId current = *current_result;
        dungeon_ids.push_back(current);
        if (it++ != slices.begin() && previous != current) {
          if (current < previous)
            std::swap(previous, current);
          for (DungeonId id = previous + 1; id != current; ++id)
            dungeon_ids.push_back(id);
        }
        previous = current;
      }
    }
    std::size_t changed = 0;
    auto executed = m_universe.executeForClient(connection_id, [&](WorldServer& world, std::shared_ptr<Player> const&) -> std::expected<void, StarException> {
       auto count = world.setTileProtection(dungeon_ids, is_protected);
       if (!count) return std::unexpected(std::move(count.error()));
       changed = *count;
       return {};
      });
    if (!executed)
      return std::unexpected(std::move(executed.error()));
    if (!*executed) {
      return "Invalid client state";
    }
    String output = Star::format("{} {} dungeon IDs", is_protected ? std::string_view("Protected") : std::string_view("Unprotected"), changed);
    return changed < dungeon_ids.size() ? Star::format("{} ({} unchanged)", output, dungeon_ids.size() - changed) : output;
  }();
  if (result) return std::move(*result);
  if (result.error().is<BadLexicalCast>())
    return "Could not parse /settileprotection parameters. Use /settileprotection <dungeonId...> <protected>";
  return std::unexpected(std::move(result.error()));
}

std::expected<String, StarException> CommandProcessor::set_dungeon_id(ConnectionId connection_id, String const& argument_string) {
  auto parsed_arguments = split_shell_words(argument_string);
  if (!parsed_arguments) return std::unexpected(std::move(parsed_arguments.error()));
  auto& arguments = *parsed_arguments;
  if (arguments.empty())
    return "Not enough arguments to /setdungeonid. Use /setdungeonid <dungeonId>";

  auto result = [&]() -> std::expected<String, StarException> {
    auto dungeon_id_result = lexical_cast<DungeonId>(arguments[0]);
    if (!dungeon_id_result) return std::unexpected(std::move(dungeon_id_result.error()));
    DungeonId dungeon_id = *dungeon_id_result;

    auto done = m_universe.executeForClient(connection_id, [dungeon_id](WorldServer& world, std::shared_ptr<Player> const& player) -> std::expected<void, StarException> {
        auto aim_position = player->aimPosition();
        if (!aim_position) return std::unexpected(std::move(aim_position.error()));
        return world.setDungeonId(RectI::with_size(Vec2I((*aim_position)), Vec2I(1, 1)), dungeon_id);
      });

    if (!done) return std::unexpected(std::move(done.error()));
    return *done ? String() : String("Failed to set dungeon id.");
  }();
  if (result) return std::move(*result);
  if (result.error().is<BadLexicalCast>())
    return "Could not parse /setdungeonid parameters. Use /setdungeonid <dungeonId>!";
  return std::unexpected(std::move(result.error()));
}

std::expected<String, StarException> CommandProcessor::set_player_start(ConnectionId connection_id, String const&) {
  auto executed = m_universe.executeForClient(connection_id, [](WorldServer& world, std::shared_ptr<Player> const& player) -> std::expected<void, StarException> {
      auto feet_offset = player->feetOffset();
      if (!feet_offset) return std::unexpected(std::move(feet_offset.error()));
      world.setPlayerStart(player->position() + *feet_offset);
      return {};
    });
  if (!executed) return std::unexpected(std::move(executed.error()));

  return "";
}
namespace {

std::expected<String, StarException> item_spawn_error(Root& root, std::string_view command, String const& kind, StarException const& error) {
  std::string diagnostic;
  if (error.is<JsonParsingException>())
    diagnostic = Star::format("Error while processing /{} '{}' command. Json parse problem", command, kind);
  else if (error.is<ItemException>())
    diagnostic = Star::format("Error while processing /{} '{}' command. Item instantiation problem", command, kind);
  else if (error.is<BadLexicalCast>())
    diagnostic = Star::format("Error while processing /{} command. Number expected", command);
  else
    diagnostic = Star::format("Error while processing /{} '{}' command", command, kind);
  auto logged = root.logger().warn_exception(diagnostic, ExceptionDiagnostic(error));
  if (!logged) return std::unexpected(std::move(logged.error()));
  if (error.is<JsonParsingException>()) return "Could not parse item parameters";
  return Star::format("Could not load item '{}'", kind);
}

} // namespace

std::expected<String, StarException> CommandProcessor::spawn_item(ConnectionId connection_id, String const& argument_string) {
  auto parsed_arguments = split_shell_words(argument_string);
  if (!parsed_arguments) return std::unexpected(std::move(parsed_arguments.error()));
  auto& arguments = *parsed_arguments;

  if (arguments.empty())
    return "Not enough arguments to /spawnitem";

  auto result = [&]() -> std::expected<String, StarException> {
    auto amount = number_argument<std::uint64_t>(arguments, 1);
    if (!amount) return std::unexpected(std::move(amount.error()));
    auto parameters = arguments.size() > 2 ? Json::parse(arguments[2]) : std::expected<Json, StarException>(JsonObject());
    if (!parameters) return std::unexpected(std::move(parameters.error()));
    auto level = number_argument<float>(arguments, 3);
    if (!level) return std::unexpected(std::move(level.error()));
    auto seed = number_argument<std::uint64_t>(arguments, 4);
    if (!seed) return std::unexpected(std::move(seed.error()));

    auto done = m_universe.executeForClient(connection_id, [&](WorldServer& world, std::shared_ptr<Player> const& player) -> std::expected<void, StarException> {
        auto item_database_result = m_root.itemDatabase();
        if (!item_database_result) return std::unexpected(std::move(item_database_result.error()));
        auto const& item_database = *item_database_result;
        auto descriptor = ItemDescriptor::create(arguments[0], amount->value_or(1), std::move(*parameters));
        if (!descriptor) return std::unexpected(std::move(descriptor.error()));
        auto item = item_database->item(*descriptor, *level, *seed, true);
        if (!item) return std::unexpected(std::move(item.error()));
        auto aim_position = player->aimPosition();
        if (!aim_position) return std::unexpected(std::move(aim_position.error()));
        auto drop = ItemDrop::create_randomized_drop(m_root, *item, (*aim_position));
        if (!drop) return std::unexpected(std::move(drop.error()));
        return world.addEntity(*drop);
      });

    if (!done) return std::unexpected(std::move(done.error()));
    return *done ? String() : String("Invalid client state");
  }();
  if (result) return std::move(*result);
  return item_spawn_error(m_root, "spawnitem", arguments[0], result.error());
}

std::expected<String, StarException> CommandProcessor::spawn_treasure(ConnectionId connection_id, String const& argument_string) {
  auto parsed_arguments = split_shell_words(argument_string);
  if (!parsed_arguments) return std::unexpected(std::move(parsed_arguments.error()));
  auto& arguments = *parsed_arguments;

  if (arguments.empty())
    return "Not enough arguments to /spawntreasure";

  auto result = [&]() -> std::expected<String, StarException> {
    auto level = number_argument<float>(arguments, 1);
    if (!level) return std::unexpected(std::move(level.error()));

    auto done = m_universe.executeForClient(connection_id, [&](WorldServer& world, std::shared_ptr<Player> const& player) -> std::expected<void, StarException> {
        auto treasure_database_result = m_root.treasureDatabase();
        if (!treasure_database_result) return std::unexpected(std::move(treasure_database_result.error()));
        auto treasure_database = std::move(*treasure_database_result);
        auto treasure = treasure_database->createTreasure(arguments[0], level->value_or(1), m_root.random().next_u64());
        if (!treasure) return std::unexpected(std::move(treasure.error()));
        for (auto const& treasure_item : *treasure) {
          auto aim_position = player->aimPosition();
          if (!aim_position) return std::unexpected(std::move(aim_position.error()));
          auto drop = ItemDrop::create_randomized_drop(m_root, treasure_item, (*aim_position));
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
  return item_spawn_error(m_root, "spawntreasure", arguments[0], result.error());
}

std::expected<String, StarException> CommandProcessor::spawn_monster(ConnectionId connection_id, String const& argument_string) {
  StringList arguments;
  auto result = [&]() -> std::expected<String, StarException> {
    auto parsed_arguments = split_shell_words(argument_string);
    if (!parsed_arguments) return std::unexpected(std::move(parsed_arguments.error()));
    arguments = std::move(*parsed_arguments);
    auto required_argument = arguments.at(0);
    if (!required_argument) return std::unexpected(std::move(required_argument.error()));

    auto monster_database_result = m_root.monsterDatabase();
    if (!monster_database_result) return std::unexpected(std::move(monster_database_result.error()));
    auto monster_database = std::move(*monster_database_result);
    auto level = number_argument<float>(arguments, 1);
    if (!level) return std::unexpected(std::move(level.error()));
    auto parameters = object_argument(arguments, 2);
    if (!parameters) return std::unexpected(std::move(parameters.error()));
    auto variant = monster_database->randomMonster(arguments[0], *parameters);
    if (!variant) return std::unexpected(std::move(variant.error()));
    auto created = monster_database->createMonster(*variant, level->value_or(1));
    if (!created) return std::unexpected(std::move(created.error()));
    return place_at_aim(m_universe, connection_id, std::move(*created));
  }();
  if (result) return std::move(*result);
  auto const& exception = result.error();
  auto logged = m_root.logger().warn_exception(
    Star::format("Could not spawn Monster of type '{}'", arguments.empty() ? std::string_view{} : std::string_view(arguments[0].utf8())),
    ExceptionDiagnostic(exception));
  if (!logged) return std::unexpected(std::move(logged.error()));
  return Star::format("Could not spawn Monster of type '{}'", argument_string);
}

std::expected<String, StarException> CommandProcessor::spawn_npc(ConnectionId connection_id, String const& argument_string) {
  auto parsed_arguments = split_shell_words(argument_string);
  if (!parsed_arguments) return std::unexpected(std::move(parsed_arguments.error()));
  auto& arguments = *parsed_arguments;

  auto result = [&]() -> std::expected<String, StarException> {
    auto npc_database_result = m_root.npcDatabase();
    if (!npc_database_result) return std::unexpected(std::move(npc_database_result.error()));
    auto npc_database = std::move(*npc_database_result);
    auto default_seed = m_root.random().next_u64();

    if (arguments.size() < 2)
      return "You must specify a species and NPC type to spawn.";

    auto level = number_argument<float>(arguments, 2);
    if (!level) return std::unexpected(std::move(level.error()));
    auto seed = number_argument<std::uint64_t>(arguments, 3);
    if (!seed) return std::unexpected(std::move(seed.error()));
    auto overrides = arguments.size() > 4 ? object_argument(arguments, 4) : std::expected<Json, StarException>(Json());
    if (!overrides) return std::unexpected(std::move(overrides.error()));
    auto variant = npc_database->generateNpcVariant(arguments[0], arguments[1], level->value_or(1), seed->value_or(default_seed), *overrides);
    if (!variant) return std::unexpected(std::move(variant.error()));
    auto npc_result = npc_database->createNpc(*variant);
    if (!npc_result) return std::unexpected(std::move(npc_result.error()));
    return place_at_aim(m_universe, connection_id, std::move(*npc_result));
  }();
  if (result) return std::move(*result);
  auto const& exception = result.error();
  auto logged = m_root.logger().warn_exception(
    Star::format("Could not spawn NPC of species '{}'", arguments.empty() ? std::string_view{} : std::string_view(arguments[0].utf8())),
    ExceptionDiagnostic(exception));
  if (!logged) return std::unexpected(std::move(logged.error()));
  return Star::format("Could not spawn NPC of species '{}'", argument_string);
}

std::expected<String, StarException> CommandProcessor::spawn_vehicle(ConnectionId connection_id, String const& argument_string) {
  auto result = [&]() -> std::expected<String, StarException> {
    auto vehicle_database_result = m_root.vehicleDatabase();
    if (!vehicle_database_result) return std::unexpected(std::move(vehicle_database_result.error()));
    auto vehicle_database = std::move(*vehicle_database_result);
    auto parsed_arguments = split_shell_words(argument_string);
    if (!parsed_arguments) return std::unexpected(std::move(parsed_arguments.error()));
    auto& arguments = *parsed_arguments;
    auto required_argument = arguments.at(0);
    if (!required_argument) return std::unexpected(std::move(required_argument.error()));

    auto parameters = object_argument(arguments, 1);
    if (!parameters) return std::unexpected(std::move(parameters.error()));
    auto created = vehicle_database->create(arguments[0], *parameters);
    if (!created) return std::unexpected(std::move(created.error()));
    return place_at_aim(m_universe, connection_id, std::move(*created));
  }();
  if (result) return std::move(*result);
  auto const& exception = result.error();
  auto logged = m_root.logger().warn_exception("Could not spawn vehicle", ExceptionDiagnostic(exception));
  if (!logged) return std::unexpected(std::move(logged.error()));
  return "Could not spawn vehicle";
}

std::expected<String, StarException> CommandProcessor::spawn_stagehand(ConnectionId connection_id, String const& argument_string) {
  StringList arguments;
  auto result = [&]() -> std::expected<String, StarException> {
    auto parsed_arguments = split_shell_words(argument_string);
    if (!parsed_arguments) return std::unexpected(std::move(parsed_arguments.error()));
    arguments = std::move(*parsed_arguments);
    auto required_argument = arguments.at(0);
    if (!required_argument) return std::unexpected(std::move(required_argument.error()));

    auto stagehand_database_result = m_root.stagehandDatabase();
    if (!stagehand_database_result) return std::unexpected(std::move(stagehand_database_result.error()));
    auto stagehand_database = std::move(*stagehand_database_result);

    auto parameters = object_argument(arguments, 1);
    if (!parameters) return std::unexpected(std::move(parameters.error()));
    auto created = stagehand_database->createStagehand(arguments[0], *parameters);
    if (!created) return std::unexpected(std::move(created.error()));
    return place_at_aim(m_universe, connection_id, std::move(*created));
  }();
  if (result) return std::move(*result);
  auto const& exception = result.error();
  auto logged = m_root.logger().warn_exception(
    Star::format("Could not spawn Stagehand of type '{}'", arguments.empty() ? std::string_view{} : std::string_view(arguments[0].utf8())),
    ExceptionDiagnostic(exception));
  if (!logged) return std::unexpected(std::move(logged.error()));
  return Star::format("Could not spawn Stagehand of type '{}'", argument_string);
}

std::expected<String, StarException> CommandProcessor::clear_stagehand(ConnectionId connection_id, String const&) {
  unsigned removed = 0;
  auto done = m_universe.executeForClient(connection_id,
      [&](WorldServer& world, std::shared_ptr<Player> player) -> std::expected<void, StarException> {
        auto aim_position = player->aimPosition();
        if (!aim_position) return std::unexpected(std::move(aim_position.error()));
        auto query_rect = RectF::with_center((*aim_position), Vec2F{2, 2});
        auto stagehands = world.query<Stagehand>(query_rect);
        if (!stagehands) return std::unexpected(std::move(stagehands.error()));
        for (auto const& stagehand : *stagehands) {
          auto removed_entity = world.removeEntity(stagehand->entityId(), true);
          if (!removed_entity) return std::unexpected(std::move(removed_entity.error()));
          ++removed;
        }
        return {};
      });
  if (!done) return std::unexpected(std::move(done.error()));
  return *done ? Star::format("Removed {} stagehands", removed) : String("Invalid client state");
}

std::expected<String, StarException> CommandProcessor::spawn_liquid(ConnectionId connection_id, String const& argument_string) {
  StringList arguments;
  auto result = [&]() -> std::expected<String, StarException> {
    auto parsed_arguments = split_shell_words(argument_string);
    if (!parsed_arguments) return std::unexpected(std::move(parsed_arguments.error()));
    arguments = std::move(*parsed_arguments);
    auto required_argument = arguments.at(0);
    if (!required_argument) return std::unexpected(std::move(required_argument.error()));

    auto liquids_database_result = m_root.liquids_database();
    if (!liquids_database_result) return std::unexpected(std::move(liquids_database_result.error()));
    auto liquids_database = std::move(*liquids_database_result);

    if (!liquids_database->is_liquid_name(arguments[0]))
      return Star::format("No such liquid {}", arguments[0]);

    auto liquid_result = liquids_database->liquid_id(arguments[0]);
    if (!liquid_result) return std::unexpected(std::move(liquid_result.error()));
    LiquidId liquid = *liquid_result;

    float quantity = 1.0f;
    if (arguments.size() > 1) {
      if (auto maybe_quantity = maybe_lexical_cast<float>(arguments[1]))
        quantity = *maybe_quantity;
      else
        return Star::format("Could not parse quantity value '{}'", arguments[1]);
    }

    auto done = m_universe.executeForClient(connection_id, [&](WorldServer& world, std::shared_ptr<Player> const& player) -> std::expected<void, StarException> {
        auto aim_position = player->aimPosition();
        if (!aim_position) return std::unexpected(std::move(aim_position.error()));
        auto modified = world.modifyTile(Vec2I((*aim_position).floor()), PlaceLiquid{liquid, quantity}, true);
        if (!modified) return std::unexpected(std::move(modified.error()));
        return {};
      });
    if (!done) return std::unexpected(std::move(done.error()));
    return *done ? String() : String("Invalid client state");

  }();
  if (result) return std::move(*result);
  auto const& exception = result.error();
  auto logged = m_root.logger().warn_exception(
    Star::format("Could not spawn liquid '{}'", arguments.empty() ? std::string_view{} : std::string_view(arguments[0].utf8())),
    ExceptionDiagnostic(exception));
  if (!logged) return std::unexpected(std::move(logged.error()));
  return "Could not spawn liquid.";
}

std::expected<String, StarException> CommandProcessor::kick(ConnectionId, String const& argument_string) {
  auto parsed_arguments = split_shell_words(argument_string);
  if (!parsed_arguments) return std::unexpected(std::move(parsed_arguments.error()));
  auto& arguments = *parsed_arguments;

  if (arguments.empty())
    return "No player specified";

  auto to_kick_result = player_cid_from_command(arguments[0], m_universe);
  if (!to_kick_result) return std::unexpected(std::move(to_kick_result.error()));
  auto to_kick = *to_kick_result;
  if (!to_kick)
    return Star::format("No user with specifier {} found.", arguments[0]);

  // Like IRC, if only the nick is passed then the nick is used as the reason
  if (arguments.size() == 1) {
    auto nick = m_universe.client_nick(*to_kick);
    if (!nick) return std::unexpected(std::move(nick.error()));
    arguments.push_back(*nick);
  }

  auto disconnected = m_universe.disconnectClient(*to_kick, arguments[1]);
  if (!disconnected) return std::unexpected(std::move(disconnected.error()));

  return Star::format("Successfully kicked user with specifier {}. ConnectionId: {}. Reason given: {}",
      arguments[0],
      Star::output_optional(to_kick),
      arguments[1]);
}

std::expected<String, StarException> CommandProcessor::ban(ConnectionId, String const& argument_string) {
  auto parsed_arguments = split_shell_words(argument_string);
  if (!parsed_arguments) return std::unexpected(std::move(parsed_arguments.error()));
  auto& arguments = *parsed_arguments;

  if (arguments.empty())
    return "No player specified";

  auto to_kick_result = player_cid_from_command(arguments[0], m_universe);
  if (!to_kick_result) return std::unexpected(std::move(to_kick_result.error()));
  auto to_kick = *to_kick_result;
  if (!to_kick)
    return Star::format("No user with specifier {} found.", arguments[0]);

  String reason = arguments[0];
  if (arguments.size() < 2) {
    auto nick = m_universe.client_nick(*to_kick);
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
      return Star::format("Invalid argument {} passed as ban type to /ban.  Options are ip, uuid, or both.", arguments[2]);
    }
  }

  std::optional<int> ban_time;
  if (arguments.size() == 4) {
    auto parsed_time = lexical_cast<int>(arguments[3]);
    if (!parsed_time) {
      if (parsed_time.error().is<BadLexicalCast>()) return Star::format("Invalid argument {} passed as ban time to /ban.", arguments[3]);
      return std::unexpected(std::move(parsed_time.error()));
    }
    ban_time = *parsed_time;
  }

  auto banned = m_universe.banUser(*to_kick, reason, type, ban_time);
  if (!banned) return std::unexpected(std::move(banned.error()));

  return Star::format("Successfully kicked user with specifier {}. ConnectionId: {}. Reason given: {}",
      arguments[0], Star::output_optional(to_kick), reason);
}

std::expected<String, StarException> CommandProcessor::unban_ip(ConnectionId, String const& argument_string) {
  auto parsed_arguments = split_shell_words(argument_string);
  if (!parsed_arguments) return std::unexpected(std::move(parsed_arguments.error()));
  auto& arguments = *parsed_arguments;

  if (arguments.empty())
    return "No IP specified";

  auto success = m_universe.unbanIp(arguments[0]);
  if (!success) return std::unexpected(std::move(success.error()));

  if (*success)
    return Star::format("Successfully removed IP {} from ban list", arguments[0]);
  else
    return Star::format("'{}' is not a valid IP or was not found in the bans list", arguments[0]);
}

std::expected<String, StarException> CommandProcessor::unban_uuid(ConnectionId, String const& argument_string) {
  auto parsed_arguments = split_shell_words(argument_string);
  if (!parsed_arguments) return std::unexpected(std::move(parsed_arguments.error()));
  auto& arguments = *parsed_arguments;

  if (arguments.empty())
    return "No UUID specified";

  auto success = m_universe.unbanUuid(arguments[0]);
  if (!success) return std::unexpected(std::move(success.error()));

  if (*success)
    return Star::format("Successfully removed UUID {} from ban list", arguments[0]);
  else
    return Star::format("'{}' is not a valid UUID or was not found in the bans list", arguments[0]);
}

std::expected<String, StarException> CommandProcessor::list(ConnectionId, String const&) {
  StringList res;
  for (auto cid : m_universe.clientIds()) {
    auto nick = m_universe.client_nick(cid);
    if (!nick) return std::unexpected(std::move(nick.error()));
    res.push_back(Star::format("${} : {} : ${}", cid, *nick, m_universe.uuidForClient(cid)->hex()));
  }

  return res.join("\n");
}

std::expected<String, StarException> CommandProcessor::client_coordinate(ConnectionId connection_id, String const& argument_string) {
  ConnectionId target_client_id = connection_id;
  String target_label = "Your";
  auto parsed_arguments = split_shell_words(argument_string);
  if (!parsed_arguments) return std::unexpected(std::move(parsed_arguments.error()));
  auto& arguments = *parsed_arguments;
  auto permission = admin_check(connection_id, "find other players");
  if (!permission) return std::unexpected(std::move(permission.error()));
  if (!*permission) {
    if (!arguments.empty()) {
      auto cid_result = player_cid_from_command(arguments[0], m_universe);
      if (!cid_result) return std::unexpected(std::move(cid_result.error()));
      auto cid = *cid_result;
      if (!cid)
        return Star::format("No user with specifier {} found.", arguments[0]);
      target_client_id = *cid;
      target_label = Star::format("Client {}'s", arguments[0]);
    }
  }

  if (target_client_id) {
    auto world_id = m_universe.clientWorld(target_client_id);
    if (!world_id) return std::unexpected(std::move(world_id.error()));
    auto world = Star::print_world_id(*world_id);
    if (!world) return std::unexpected(std::move(world.error()));
    return Star::format("{} current location is {}", target_label, *world);
  } else {
    return "";
  }
}

std::expected<String, StarException> CommandProcessor::server_reload(ConnectionId, String const&) {
  auto& root = m_root;
  auto reloaded = root.reload();
  if (!reloaded) return std::unexpected(std::move(reloaded.error()));
  auto loaded = root.fullyLoad();
  if (!loaded) return std::unexpected(std::move(loaded.error()));
  return "";
}

std::expected<String, StarException> CommandProcessor::eval(ConnectionId, String const& lua) {
  if (!m_script_component.context()) return std::unexpected(LuaException("Lua script component is not initialized"));
  auto value = m_script_component.context()->eval(lua);
  if (!value) return std::unexpected(std::move(value.error()));
  return Star::print_lua_value(*value);
}

std::expected<String, StarException> CommandProcessor::entity_eval(ConnectionId connection_id, String const& lua) {
  String message;
  auto done = m_universe.executeForClient(connection_id,
      [&lua, &message](WorldServer& world, std::shared_ptr<Player> const& player) -> std::expected<void, StarException> {
        auto aim_position = player->aimPosition();
        if (!aim_position) return std::unexpected(std::move(aim_position.error()));
        auto query_rect = RectF::with_center((*aim_position), Vec2F{2, 2});
        auto query = world.query<ScriptedEntity>(query_rect);
        if (!query) return std::unexpected(std::move(query.error()));
        auto entities = std::move(*query);
        if (entities.empty()) {
          message = "Could not find scripted entity at cursor";
          return {};
        }

        std::shared_ptr<ScriptedEntity> target_entity;
        for (auto const& entity : entities) {
          if (!target_entity
              || vmag_squared(entity->position() - (*aim_position))
                < vmag_squared(target_entity->position() - (*aim_position)))
            target_entity = entity;
        }

        auto res = target_entity->eval_script(lua);
        if (!res) return std::unexpected(std::move(res.error()));
        if (*res) {
          auto printed = Star::print_lua_value(**res);
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

template <bool enabled>
std::expected<String, StarException> CommandProcessor::set_spawning(ConnectionId connection_id, String const&) {
  auto done = m_universe.executeForClient(connection_id, [](WorldServer& world, std::shared_ptr<Player> const&) -> std::expected<void, StarException> {
    world.setSpawningEnabled(enabled);
    return {};
  });
  if (!done) return std::unexpected(std::move(done.error()));
  if (*done) return enabled ? String("enabled monster spawning") : String("disabled monster spawning");
  return enabled ? String("enabling monster spawning failed") : String("disabling monster spawning failed");
}

std::expected<String, StarException> CommandProcessor::place_dungeon(ConnectionId connection_id, String const& argument_string) {
  auto parsed_arguments = split_shell_words(argument_string);
  if (!parsed_arguments) return std::unexpected(std::move(parsed_arguments.error()));
  auto& arguments = *parsed_arguments;
  auto required_argument = arguments.at(0);
  if (!required_argument) return std::unexpected(std::move(required_argument.error()));
  String const& dungeon_name = arguments[0];

  std::optional<Vec2I> target_position;
  if (arguments.size() > 1) {
    auto pos = arguments[1].split(",", 1);
    if (!pos) return std::unexpected(std::move(pos.error()));
    auto x_text = pos->at(0);
    if (!x_text) return std::unexpected(std::move(x_text.error()));
    auto y_text = pos->at(1);
    if (!y_text) return std::unexpected(std::move(y_text.error()));
    auto x = lexical_cast<int>(x_text->get());
    if (!x) return std::unexpected(std::move(x.error()));
    auto y = lexical_cast<int>(y_text->get());
    if (!y) return std::unexpected(std::move(y.error()));
    target_position = Vec2I(*x, *y);
  }

  auto done = m_universe.executeForClient(connection_id,
      [dungeon_name, target_position](WorldServer& world, std::shared_ptr<Player> const& player) -> std::expected<void, StarException> {
        auto aim_position = player->aimPosition();
        if (!aim_position) return std::unexpected(std::move(aim_position.error()));
        auto placed = world.placeDungeon(dungeon_name, target_position.value_or(Vec2I::floor((*aim_position))), true);
        if (!placed) return std::unexpected(std::move(placed.error()));
        return {};
      });

  if (!done) return std::unexpected(std::move(done.error()));
  return *done ? String() : "Unable to place dungeon " + dungeon_name;
}

std::expected<String, StarException> CommandProcessor::set_universe_flag(ConnectionId, String const& argument_string) {
  auto parsed_arguments = split_shell_words(argument_string);
  if (!parsed_arguments) return std::unexpected(std::move(parsed_arguments.error()));
  auto& arguments = *parsed_arguments;
  auto required_argument = arguments.at(0);
  if (!required_argument) return std::unexpected(std::move(required_argument.error()));
  String const& flag = arguments[0];
  auto set = m_universe.universeSettings()->setFlag(flag);
  if (!set) return std::unexpected(std::move(set.error()));

  return "set universe flag " + flag;
}

std::expected<String, StarException> CommandProcessor::reset_universe_flags(ConnectionId, String const&) {
  m_universe.universeSettings()->resetFlags();
  return "universe flags reset!";
}

std::expected<String, StarException> CommandProcessor::add_biome_region(ConnectionId connection_id, String const& argument_string) {
  auto parsed_arguments = split_shell_words(argument_string);
  if (!parsed_arguments) return std::unexpected(std::move(parsed_arguments.error()));
  auto& arguments = *parsed_arguments;
  auto required_argument = arguments.at(1);
  if (!required_argument) return std::unexpected(std::move(required_argument.error()));

  String const& biome_name = arguments[0];
  auto width_result = lexical_cast<int>(arguments[1]);
  if (!width_result) return std::unexpected(std::move(width_result.error()));
  int width = *width_result;

  String sub_block_selector = "largeClumps";
  if (arguments.size() > 2)
    sub_block_selector = arguments[2];

  auto done = m_universe.executeForClient(connection_id,
      [biome_name, width, sub_block_selector](WorldServer& world, std::shared_ptr<Player> const& player) -> std::expected<void, StarException> {
        auto aim_position = player->aimPosition();
        if (!aim_position) return std::unexpected(std::move(aim_position.error()));
        return world.addBiomeRegion(Vec2I::floor((*aim_position)), biome_name, sub_block_selector, width);
      });

  if (!done) return std::unexpected(std::move(done.error()));
  return *done ? Star::format("added region of biome {} with width {}", biome_name, width) : String("failed to add biome region");
}

std::expected<String, StarException> CommandProcessor::expand_biome_region(ConnectionId connection_id, String const& argument_string) {
  auto parsed_arguments = split_shell_words(argument_string);
  if (!parsed_arguments) return std::unexpected(std::move(parsed_arguments.error()));
  auto& arguments = *parsed_arguments;
  auto required_argument = arguments.at(0);
  if (!required_argument) return std::unexpected(std::move(required_argument.error()));

  auto new_width_result = lexical_cast<int>(arguments[0]);
  if (!new_width_result) return std::unexpected(std::move(new_width_result.error()));
  int new_width = *new_width_result;

  auto done = m_universe.executeForClient(connection_id,
      [new_width](WorldServer& world, std::shared_ptr<Player> const& player) -> std::expected<void, StarException> {
        auto aim_position = player->aimPosition();
        if (!aim_position) return std::unexpected(std::move(aim_position.error()));
        return world.expandBiomeRegion(Vec2I::floor((*aim_position)), new_width);
      });

  if (!done) return std::unexpected(std::move(done.error()));
  return *done ? Star::format("expanded region to width {}", new_width) : String("failed to expand biome region");
}

std::expected<String, StarException> CommandProcessor::update_planet_type(ConnectionId, String const& argument_string) {
  auto parsed_arguments = split_shell_words(argument_string);
  if (!parsed_arguments) return std::unexpected(std::move(parsed_arguments.error()));
  auto& arguments = *parsed_arguments;
  auto required_argument = arguments.at(2);
  if (!required_argument) return std::unexpected(std::move(required_argument.error()));

  auto coordinate_result = CelestialCoordinate::create(Json(arguments[0]));
  if (!coordinate_result) return std::unexpected(std::move(coordinate_result.error()));
  auto coordinate = *coordinate_result;
  auto new_type = arguments[1];
  auto weather_biome = arguments[2];

  auto done = m_universe.updatePlanetType(coordinate, new_type, weather_biome);

  if (!done) return std::unexpected(std::move(done.error()));
  return *done ? Star::format("set planet at {} to type {} weatherBiome {}", coordinate, new_type, weather_biome) : String("failed to update planet type");
}

std::expected<String, StarException> CommandProcessor::set_weather(ConnectionId connection_id, String const& argument_string) {
  auto parsed = split_shell_words(argument_string);
  if (!parsed) return std::unexpected(std::move(parsed.error()));
  auto arguments = std::move(*parsed);
  if (arguments.empty()) {
    StringList list;
    auto done = m_universe.executeForClient(connection_id, [&list](WorldServer& world, std::shared_ptr<Player> const& player) -> std::expected<void, StarException> {
      auto weather = world.weatherList(player->position());
      if (!weather) return std::unexpected(std::move(weather.error()));
      list = std::move(*weather);
      return {};
    });
    if (!done) return std::unexpected(std::move(done.error()));
    return *done ? Star::format("weathers: {}", list.join(", ")) : String("failed to query weather");
  }
  String const& weather_name = arguments[0];
  bool force = false;
  CelestialCoordinate coordinate;
  if (arguments.size() >= 2) {
    if (arguments[1] == "force") {
      force = true;
      if (arguments.size() >= 3) {
        auto parsed_coordinate = CelestialCoordinate::create(Json(arguments[2]));
        if (!parsed_coordinate) return std::unexpected(std::move(parsed_coordinate.error()));
        coordinate = *parsed_coordinate;
      }
    } else {
      auto parsed_coordinate = CelestialCoordinate::create(Json(arguments[1]));
      if (!parsed_coordinate) return std::unexpected(std::move(parsed_coordinate.error()));
      coordinate = *parsed_coordinate;
    }
  }
  std::expected<bool, StarException> done;
  if (coordinate.is_null()) {
    done = m_universe.executeForClient(connection_id, [weather_name, force](WorldServer& world, std::shared_ptr<Player> const& player) -> std::expected<void, StarException> { return world.setWeather(player->position(), weather_name, force); });
  } else {
    done = m_universe.setWeather(coordinate, weather_name, force);
  }
  if (!done) return std::unexpected(std::move(done.error()));
  return *done ? (coordinate.is_null() ? Star::format("set weather to {}{}", weather_name, force ? std::string_view(" (forced)") : std::string_view()) : Star::format("set weather for {} to {}{}", coordinate, weather_name, force ? std::string_view(" (forced)") : std::string_view())) : String("failed to set weather");
}


std::expected<String, StarException> CommandProcessor::set_environment_biome(ConnectionId connection_id, String const&) {
  auto done = m_universe.executeForClient(connection_id,
      [](WorldServer& world, std::shared_ptr<Player> const& player) -> std::expected<void, StarException> {
        auto aim_position = player->aimPosition();
        if (!aim_position) return std::unexpected(std::move(aim_position.error()));
        return world.setLayerEnvironmentBiome(Vec2I::floor((*aim_position)));
      });

  if (!done) return std::unexpected(std::move(done.error()));
  return *done ? String("set environment biome for world layer") : String("failed to set environment biome");
}

std::expected<std::optional<ConnectionId>, StarException> CommandProcessor::player_cid_from_command(String const& player, UniverseServer& universe) {
  constexpr std::string_view username_marker = "@";
  constexpr std::string_view cid_marker = "$";
  constexpr std::string_view uuid_marker = "$$";
  auto username = player.starts_with(username_marker);
  if (!username) return std::unexpected(std::move(username.error()));
  if (*username) {
    auto name = player.substr(username_marker.size());
    if (!name) return std::unexpected(std::move(name.error()));
    return universe.find_nick(*name);
  }
  auto uuid_prefix = player.starts_with(uuid_marker);
  if (!uuid_prefix) return std::unexpected(std::move(uuid_prefix.error()));
  if (*uuid_prefix) {
    auto text = player.substr(uuid_marker.size());
    if (!text) return std::unexpected(std::move(text.error()));
    auto uuid = Uuid::from_hex((*text).utf8());
    if (uuid) return universe.clientForUuid(*uuid);
    if (!uuid.error().is<UuidException>()) return std::unexpected(std::move(uuid.error()));
  } else {
    auto cid_prefix = player.starts_with(cid_marker);
    if (!cid_prefix) return std::unexpected(std::move(cid_prefix.error()));
    if (*cid_prefix) {
      auto text = player.substr(cid_marker.size());
      if (!text) return std::unexpected(std::move(text.error()));
      auto cid = maybe_lexical_cast<ConnectionId>(*text).value_or(server_connection_id);
      if (universe.isConnectedClient(cid)) return std::optional<ConnectionId>(cid);
    }
  }
  return universe.find_nick(player);
}

std::expected<CommandProcessor::CommandMap, StarException> CommandProcessor::make_command_map() {
  struct Registration {
    std::string_view name;
    Command command;
  };
  static constexpr Registration registrations[] = {
    {"admin", {&CommandProcessor::admin, {}}},
    {"serverdebug", {&CommandProcessor::server_debug, "debug server"}},
    {"timewarp", {&CommandProcessor::timewarp, "do the time warp again"}},
    {"timescale", {&CommandProcessor::timescale, "mess with time"}},
    {"tickrate", {&CommandProcessor::tickrate, "change the tick rate"}},
    {"settileprotection", {&CommandProcessor::set_tile_protection, "modify world properties"}},
    {"setdungeonid", {&CommandProcessor::set_dungeon_id, "set dungeon id"}},
    {"setspawnpoint", {&CommandProcessor::set_player_start, "modify world properties"}},
    {"spawnitem", {&CommandProcessor::spawn_item, "spawn items"}},
    {"spawntreasure", {&CommandProcessor::spawn_treasure, "spawn items"}},
    {"spawnmonster", {&CommandProcessor::spawn_monster, "spawn monsters"}},
    {"spawnnpc", {&CommandProcessor::spawn_npc, "spawn NPCs"}},
    {"spawnstagehand", {&CommandProcessor::spawn_stagehand, "spawn stagehands"}},
    {"clearstagehand", {&CommandProcessor::clear_stagehand, "remove stagehands"}},
    {"spawnvehicle", {&CommandProcessor::spawn_vehicle, "spawn vehicles"}},
    {"spawnliquid", {&CommandProcessor::spawn_liquid, "spawn liquid"}},
    {"pvp", {&CommandProcessor::pvp, {}}},
    {"serverwhoami", {&CommandProcessor::whoami, {}}},
    {"kick", {&CommandProcessor::kick, "kick a user"}},
    {"ban", {&CommandProcessor::ban, "ban a user"}},
    {"unbanip", {&CommandProcessor::unban_ip, "unban a user"}},
    {"unbanuuid", {&CommandProcessor::unban_uuid, "unban a user"}},
    {"list", {&CommandProcessor::list, "list clients"}},
    {"help", {&CommandProcessor::help, {}}},
    {"warp", {&CommandProcessor::warp, "do the space warp again"}},
    {"warprandom", {&CommandProcessor::warp_random, "warp to random world"}},
    {"whereami", {&CommandProcessor::client_coordinate, {}}},
    {"whereis", {&CommandProcessor::client_coordinate, {}}},
    {"serverreload", {&CommandProcessor::server_reload, "trigger root reload"}},
    {"eval", {&CommandProcessor::eval, "execute server script", true}},
    {"entityeval", {&CommandProcessor::entity_eval, "execute server entity script", true}},
    {"enablespawning", {&CommandProcessor::set_spawning<true>, "enable world spawning"}},
    {"disablespawning", {&CommandProcessor::set_spawning<false>, "disable world spawning"}},
    {"placedungeon", {&CommandProcessor::place_dungeon, "place dungeons"}},
    {"setuniverseflag", {&CommandProcessor::set_universe_flag, "set universe flags"}},
    {"resetuniverseflags", {&CommandProcessor::reset_universe_flags, "reset universe flags"}},
    {"addbiomeregion", {&CommandProcessor::add_biome_region, "add biome regions"}},
    {"expandbiomeregion", {&CommandProcessor::expand_biome_region, "expand biome regions"}},
    {"updateplanettype", {&CommandProcessor::update_planet_type, "update planet type"}},
    {"setweather", {&CommandProcessor::set_weather, "set weather"}},
    {"setenvironmentbiome", {&CommandProcessor::set_environment_biome, "update layer environment biome"}},
  };
  CommandMap map;
  map.reserve(std::size(registrations));
  for (auto const& entry : registrations) {
    auto key = CaseInsensitiveString::create(String(entry.name));
    if (!key) return std::unexpected(std::move(key.error()));
    map.try_emplace(std::move(*key), entry.command);
  }
  return map;
}

const std::expected<CommandProcessor::CommandMap, StarException> CommandProcessor::s_command_map = make_command_map();

std::expected<ServerCommandResult, StarException> CommandProcessor::handle_command(ConnectionId connection_id, String const& command, String const& argument_string) {
  if (!s_command_map) return std::unexpected(s_command_map.error());
  auto key = CaseInsensitiveStringView::create(command);
  if (!key) return std::unexpected(std::move(key.error()));
  auto it = s_command_map->find(*key);
  if (it != s_command_map->end()) {
    auto const& entry = it->second;
    if (entry.local_only) {
      auto denied = local_check(connection_id, entry.admin_action);
      if (!denied) return std::unexpected(std::move(denied.error()));
      if (*denied) return ServerCommandResult(std::move(**denied));
    }
    if (!entry.admin_action.empty()) {
      auto denied = admin_check(connection_id, entry.admin_action);
      if (!denied) return std::unexpected(std::move(denied.error()));
      if (*denied) return ServerCommandResult(std::move(**denied));
    }
    auto response = (this->*entry.handler)(connection_id, argument_string);
    if (!response) return std::unexpected(std::move(response.error()));
    return ServerCommandResult(std::move(*response));
  }
  auto arguments = split_shell_words(argument_string);
  if (!arguments) return std::unexpected(std::move(arguments.error()));
  auto invoked = recover_lua_script_error(m_script_component.invoke<std::variant<RpcPromise<Json>, LuaValue>>("command", command, connection_id, json_from_string_list(*arguments)));
  if (!invoked) return std::unexpected(std::move(invoked.error()));
  if (*invoked) {
    auto& result = **invoked;
    if (std::holds_alternative<RpcPromise<Json>>(result)) {
      auto& promise = std::get<RpcPromise<Json>>(result);
      auto wrapped = promise.wrap([](Json res) -> std::expected<String, StarException> {
        if (res.is_type(Json::Type::String)) {
          auto text = res.string_ptr();
          if (!text) return std::unexpected(std::move(text.error()));
          return **text;
        }
        if (!res.is_null()) return res.repr(1, true);
        return String();
      });
      if (!wrapped) return std::unexpected(std::move(wrapped.error()));
      return ServerCommandResult(std::move(*wrapped));
    }
    auto printed = Star::print_lua_value(std::get<LuaValue>(result));
    if (!printed) return std::unexpected(std::move(printed.error()));
    return ServerCommandResult(std::move(*printed));
  }
  return ServerCommandResult(Star::format("No such command {}", command));
}

std::expected<std::optional<String>, StarException> CommandProcessor::admin_check(ConnectionId connection_id, std::string_view command_description) const {
  if (connection_id == server_connection_id) return std::optional<String>{};
  auto config_result = m_root.configuration();
  if (!config_result) return std::unexpected(std::move(config_result.error()));
  auto config = std::move(*config_result);
  auto allow_json = config->get("allowAdminCommands");
  if (!allow_json) return std::unexpected(std::move(allow_json.error()));
  auto allow = allow_json->to_bool();
  if (!allow) return std::unexpected(std::move(allow.error()));
  if (!*allow) return std::optional<String>("Admin commands disabled on this server.");
  auto anyone_json = config->get("allowAdminCommandsFromAnyone");
  if (!anyone_json) return std::unexpected(std::move(anyone_json.error()));
  auto anyone = anyone_json->to_bool();
  if (!anyone) return std::unexpected(std::move(anyone.error()));
  if (!*anyone) {
    auto admin = m_universe.isAdmin(connection_id);
    if (!admin) return std::unexpected(std::move(admin.error()));
    if (!*admin) return std::optional<String>(Star::format("Insufficient privileges to {}.", command_description));
  }
  return std::optional<String>{};
}

std::expected<std::optional<String>, StarException> CommandProcessor::local_check(ConnectionId connection_id, std::string_view command_description) const {
  if (connection_id == server_connection_id) return std::optional<String>{};
  auto local = m_universe.isLocal(connection_id);
  if (!local) return std::unexpected(std::move(local.error()));
  if (!*local) return std::optional<String>(Star::format("The {} command can only be used locally.", command_description));
  return std::optional<String>{};
}

std::expected<LuaCallbacks, StarException> CommandProcessor::make_command_callbacks() {
  LuaCallbacks callbacks;
  auto registered =
    callbacks.register_callback_with_signature<std::expected<std::optional<String>, StarException>, ConnectionId, String>(
      "adminCheck", [this](ConnectionId connection_id, String const& command_description) {
        return admin_check(connection_id, command_description.utf8());
      });
  if (!registered) return std::unexpected(std::move(registered.error()));
  return callbacks;
}

}

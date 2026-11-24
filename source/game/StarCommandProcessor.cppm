module;

import std;
import star.thread;
import star.lua;
import star.json;
import star.game_types;
import star.message_types;
import star.string;
import star.byte_array;
import star.encode;
import star.bytes;
import star.format;
import star.lua_components;
import star.lua_root;

namespace Star {

class UniverseServer;
class Root;

class CommandProcessor {
  struct ConstructionKey { explicit ConstructionKey() = default; };
public:
  CommandProcessor(ConstructionKey, Root& root, UniverseServer& universe);
  [[nodiscard]] static std::expected<std::shared_ptr<CommandProcessor>, StarException> create(Root& root, UniverseServer& universe);
  [[nodiscard]] std::expected<void, StarException> close();

  [[nodiscard]] std::expected<ServerCommandResult, StarException> admin_command(String const& command, String const& argument_string);
  [[nodiscard]] std::expected<ServerCommandResult, StarException> user_command(ConnectionId client_id, String const& command, String const& argument_string);
private:
  [[nodiscard]] static std::expected<std::optional<ConnectionId>, StarException> player_cid_from_command(String const& player, UniverseServer& universe);

  [[nodiscard]] std::expected<String, StarException> help(ConnectionId connection_id, String const& argument_string);
  [[nodiscard]] std::expected<String, StarException> admin(ConnectionId connection_id, String const& argument_string);
  [[nodiscard]] std::expected<String, StarException> server_debug(ConnectionId connection_id, String const& argument_string);
  [[nodiscard]] std::expected<String, StarException> pvp(ConnectionId connection_id, String const& argument_string);
  [[nodiscard]] std::expected<String, StarException> whoami(ConnectionId connection_id, String const& argument_string);
	
  [[nodiscard]] std::expected<String, StarException> warp(ConnectionId connection_id, String const& argument_string);
  [[nodiscard]] std::expected<String, StarException> warp_random(ConnectionId connection_id, String const& argument_string);
  [[nodiscard]] std::expected<String, StarException> timewarp(ConnectionId connection_id, String const& argument_string);
  [[nodiscard]] std::expected<String, StarException> timescale(ConnectionId connection_id, String const& argument_string);
  [[nodiscard]] std::expected<String, StarException> tickrate(ConnectionId connection_id, String const& argument_string);
  [[nodiscard]] std::expected<String, StarException> set_tile_protection(ConnectionId connection_id, String const& argument_string);
  [[nodiscard]] std::expected<String, StarException> set_dungeon_id(ConnectionId connection_id, String const& argument_string);
  [[nodiscard]] std::expected<String, StarException> set_player_start(ConnectionId connection_id, String const& argument_string);
  [[nodiscard]] std::expected<String, StarException> spawn_item(ConnectionId connection_id, String const& argument_string);
  [[nodiscard]] std::expected<String, StarException> spawn_treasure(ConnectionId connection_id, String const& argument_string);
  [[nodiscard]] std::expected<String, StarException> spawn_monster(ConnectionId connection_id, String const& argument_string);
  [[nodiscard]] std::expected<String, StarException> spawn_npc(ConnectionId connection_id, String const& argument_string);
  [[nodiscard]] std::expected<String, StarException> spawn_vehicle(ConnectionId connection_id, String const& argument_string);
  [[nodiscard]] std::expected<String, StarException> spawn_stagehand(ConnectionId connection_id, String const& argument_string);
  [[nodiscard]] std::expected<String, StarException> clear_stagehand(ConnectionId connection_id, String const& argument_string);
  [[nodiscard]] std::expected<String, StarException> spawn_liquid(ConnectionId connection_id, String const& argument_string);
  [[nodiscard]] std::expected<String, StarException> kick(ConnectionId connection_id, String const& argument_string);
  [[nodiscard]] std::expected<String, StarException> ban(ConnectionId connection_id, String const& argument_string);
  [[nodiscard]] std::expected<String, StarException> unban_ip(ConnectionId connection_id, String const& argument_string);
  [[nodiscard]] std::expected<String, StarException> unban_uuid(ConnectionId connection_id, String const& argument_string);
  [[nodiscard]] std::expected<String, StarException> list(ConnectionId connection_id, String const& argument_string);
  [[nodiscard]] std::expected<String, StarException> client_coordinate(ConnectionId connection_id, String const& argument_string);
  [[nodiscard]] std::expected<String, StarException> server_reload(ConnectionId connection_id, String const& argument_string);
  [[nodiscard]] std::expected<String, StarException> eval(ConnectionId connection_id, String const& lua);
  [[nodiscard]] std::expected<String, StarException> entity_eval(ConnectionId connection_id, String const& lua);
  template <bool enabled>
  [[nodiscard]] std::expected<String, StarException> set_spawning(ConnectionId connection_id, String const&);
  [[nodiscard]] std::expected<String, StarException> place_dungeon(ConnectionId connection_id, String const& argument_string);
  [[nodiscard]] std::expected<String, StarException> set_universe_flag(ConnectionId connection_id, String const& argument_string);
  [[nodiscard]] std::expected<String, StarException> reset_universe_flags(ConnectionId connection_id, String const& argument_string);
  [[nodiscard]] std::expected<String, StarException> add_biome_region(ConnectionId connection_id, String const& argument_string);
  [[nodiscard]] std::expected<String, StarException> expand_biome_region(ConnectionId connection_id, String const& argument_string);
  [[nodiscard]] std::expected<String, StarException> update_planet_type(ConnectionId connection_id, String const& argument_string);
  [[nodiscard]] std::expected<String, StarException> set_weather(ConnectionId connection_id, String const& argument_string);
  [[nodiscard]] std::expected<String, StarException> set_environment_biome(ConnectionId connection_id, String const& argument_string);

  using CommandHandler = std::expected<String, StarException> (CommandProcessor::*)(ConnectionId, String const&);
  struct Command {
    CommandHandler handler;
    std::string_view admin_action;
    bool local_only = false;
  };
  using CommandMap = CaseInsensitiveStringMap<Command>;
  static const std::expected<CommandMap, StarException> s_command_map;
  [[nodiscard]] static std::expected<CommandMap, StarException> make_command_map();

  mutable std::mutex m_mutex;

  [[nodiscard]] std::expected<ServerCommandResult, StarException> handle_command(ConnectionId connection_id, String const& command, String const& argument_string);
  [[nodiscard]] std::expected<std::optional<String>, StarException> admin_check(ConnectionId connection_id, std::string_view command_description) const;
  [[nodiscard]] std::expected<std::optional<String>, StarException> local_check(ConnectionId connection_id, std::string_view command_description) const;
  [[nodiscard]] std::expected<LuaCallbacks, StarException> make_command_callbacks();

  Root& m_root;
  UniverseServer& m_universe;

  // CommandProcessor can be accessed from multiple threads.
  // To avoid issues of thread unsafe accesses where an rcon command might be handled while a universe server script context is running, CommandProcessor instances have their own lua root instead.
  std::shared_ptr<LuaRoot> m_lua_root;
  LuaBaseComponent m_script_component;
};

}

export module star.command_processor;

export namespace Star {
  using ::Star::UniverseServer;
  using ::Star::CommandProcessor;
}

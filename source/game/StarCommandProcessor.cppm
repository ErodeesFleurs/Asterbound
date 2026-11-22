module;

import std;
import star.thread;
import star.lua;
import star.json;
import star.game_types;
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
public:
  [[nodiscard]] static std::expected<std::shared_ptr<CommandProcessor>, StarException> create(Root& root, UniverseServer& universe);
  [[nodiscard]] std::expected<void, StarException> close();

  [[nodiscard]] std::expected<ServerCommandResult, StarException> adminCommand(String const& command, String const& argumentString);
  [[nodiscard]] std::expected<ServerCommandResult, StarException> userCommand(ConnectionId clientId, String const& command, String const& argumentString);
private:
  CommandProcessor(Root& root, UniverseServer& universe);
  [[nodiscard]] static std::expected<std::optional<ConnectionId>, StarException> playerCidFromCommand(String const& player, UniverseServer& universe);

  [[nodiscard]] std::expected<String, StarException> help(ConnectionId connectionId, String const& argumentString);
  [[nodiscard]] std::expected<String, StarException> admin(ConnectionId connectionId, String const& argumentString);
  [[nodiscard]] std::expected<String, StarException> serverDebug(ConnectionId connectionId, String const& argumentString);
  [[nodiscard]] std::expected<String, StarException> pvp(ConnectionId connectionId, String const& argumentString);
  [[nodiscard]] std::expected<String, StarException> whoami(ConnectionId connectionId, String const& argumentString);
	
  [[nodiscard]] std::expected<String, StarException> warp(ConnectionId connectionId, String const& argumentString);
  [[nodiscard]] std::expected<String, StarException> warpRandom(ConnectionId connectionId, String const& argumentString);
  [[nodiscard]] std::expected<String, StarException> timewarp(ConnectionId connectionId, String const& argumentString);
  [[nodiscard]] std::expected<String, StarException> timescale(ConnectionId connectionId, String const& argumentString);
  [[nodiscard]] std::expected<String, StarException> tickrate(ConnectionId connectionId, String const& argumentString);
  [[nodiscard]] std::expected<String, StarException> setTileProtection(ConnectionId connectionId, String const& argumentString);
  [[nodiscard]] std::expected<String, StarException> setDungeonId(ConnectionId connectionId, String const& argumentString);
  [[nodiscard]] std::expected<String, StarException> setPlayerStart(ConnectionId connectionId, String const& argumentString);
  [[nodiscard]] std::expected<String, StarException> spawnItem(ConnectionId connectionId, String const& argumentString);
  [[nodiscard]] std::expected<String, StarException> spawnTreasure(ConnectionId connectionId, String const& argumentString);
  [[nodiscard]] std::expected<String, StarException> spawnMonster(ConnectionId connectionId, String const& argumentString);
  [[nodiscard]] std::expected<String, StarException> spawnNpc(ConnectionId connectionId, String const& argumentString);
  [[nodiscard]] std::expected<String, StarException> spawnVehicle(ConnectionId connectionId, String const& argumentString);
  [[nodiscard]] std::expected<String, StarException> spawnStagehand(ConnectionId connectionId, String const& argumentString);
  [[nodiscard]] std::expected<String, StarException> clearStagehand(ConnectionId connectionId, String const& argumentString);
  [[nodiscard]] std::expected<String, StarException> spawnLiquid(ConnectionId connectionId, String const& argumentString);
  [[nodiscard]] std::expected<String, StarException> kick(ConnectionId connectionId, String const& argumentString);
  [[nodiscard]] std::expected<String, StarException> ban(ConnectionId connectionId, String const& argumentString);
  [[nodiscard]] std::expected<String, StarException> unbanIp(ConnectionId connectionId, String const& argumentString);
  [[nodiscard]] std::expected<String, StarException> unbanUuid(ConnectionId connectionId, String const& argumentString);
  [[nodiscard]] std::expected<String, StarException> list(ConnectionId connectionId, String const& argumentString);
  [[nodiscard]] std::expected<String, StarException> clientCoordinate(ConnectionId connectionId, String const& argumentString);
  [[nodiscard]] std::expected<String, StarException> serverReload(ConnectionId connectionId, String const& argumentString);
  [[nodiscard]] std::expected<String, StarException> eval(ConnectionId connectionId, String const& lua);
  [[nodiscard]] std::expected<String, StarException> entityEval(ConnectionId connectionId, String const& lua);
  [[nodiscard]] std::expected<String, StarException> enableSpawning(ConnectionId connectionId, String const& argumentString);
  [[nodiscard]] std::expected<String, StarException> disableSpawning(ConnectionId connectionId, String const& argumentString);
  [[nodiscard]] std::expected<String, StarException> placeDungeon(ConnectionId connectionId, String const& argumentString);
  [[nodiscard]] std::expected<String, StarException> setUniverseFlag(ConnectionId connectionId, String const& argumentString);
  [[nodiscard]] std::expected<String, StarException> resetUniverseFlags(ConnectionId connectionId, String const& argumentString);
  [[nodiscard]] std::expected<String, StarException> addBiomeRegion(ConnectionId connectionId, String const& argumentString);
  [[nodiscard]] std::expected<String, StarException> expandBiomeRegion(ConnectionId connectionId, String const& argumentString);
  [[nodiscard]] std::expected<String, StarException> updatePlanetType(ConnectionId connectionId, String const& argumentString);
  [[nodiscard]] std::expected<String, StarException> setWeather(ConnectionId connectionId, String const& argumentString);
  [[nodiscard]] std::expected<String, StarException> setEnvironmentBiome(ConnectionId connectionId, String const& argumentString);

  using Command = std::expected<String, StarException> (CommandProcessor::*)(ConnectionId, String const&);
  static const std::expected<CaseInsensitiveStringMap<Command>, StarException> s_commandMap;
  [[nodiscard]] static std::expected<CaseInsensitiveStringMap<Command>, StarException> makeCommandMap();

  mutable std::mutex m_mutex;

  [[nodiscard]] std::expected<ServerCommandResult, StarException> handleCommand(ConnectionId connectionId, String const& command, String const& argumentString);
  [[nodiscard]] std::expected<std::optional<String>, StarException> adminCheck(ConnectionId connectionId, String const& commandDescription) const;
  [[nodiscard]] std::expected<std::optional<String>, StarException> localCheck(ConnectionId connectionId, String const& commandDescription) const;
  [[nodiscard]] std::expected<LuaCallbacks, StarException> makeCommandCallbacks();

  Root& m_root;
  UniverseServer& m_universe;

  // CommandProcessor can be accessed from multiple threads.
  // To avoid issues of thread unsafe accesses where an rcon command might be handled while a universe server script context is running, CommandProcessor instances have their own lua root instead.
  std::shared_ptr<LuaRoot> m_luaRoot;
  LuaBaseComponent m_scriptComponent;
};

}

export module star.command_processor;

export namespace Star {
  using ::Star::UniverseServer;
  using ::Star::CommandProcessor;
}

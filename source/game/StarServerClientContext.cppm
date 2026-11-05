module;
import star.format;
import star.xx_hash;
import star.json;
import star.net_compatibility;
import star.byte_array;
import star.thread;
import star.data_stream;
import star.json_rpc;
import star.rpc_promise;
import star.game_types;
import star.string;
import star.either;
import star.strong_typedef;
import star.bi_map;
import star.id_map;
import star.set;
import star.btree;
import star.ordered_map;
import star.lru_cache;
import star.data_stream_devices;
import star.btree_database;
import star.casting;
import star.ordered_set;
import star.list;
import star.map;
import star.version;
import star.random;
import star.perlin;
import star.weighted_pool;
import std;
import star.worker_pool;

import star.sector_array_2d;
import star.color;
import star.image_processing;
import star.hash;
import star.string_view;
import star.directives;
import star.asset_path;


import star.uuid;
import star.damage_types;
import star.host_address;
import star.celestial_coordinate;
import star.warping;
import star.player_types;
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
import star.sky_parameters;
import star.system_world;

import star.net_element_basic_fields;
import star.net_element_system;

namespace Star {

class WorldServerThread;
class SystemWorldServerThread;
class Root;

class ServerClientContext {
public:
  [[nodiscard]] static std::expected<std::shared_ptr<ServerClientContext>, StarException> create(Root& root, ConnectionId clientId, std::optional<HostAddress> remoteAddress, NetCompatibilityRules netRules, Uuid playerUuid,
      String playerName, String shipSpecies, bool canBecomeAdmin, WorldChunks initialShipChunks, StringSet validShipSpecies, String account);

  ConnectionId clientId() const;
  std::optional<HostAddress> const& remoteAddress() const;
  Uuid const& playerUuid() const;
  String const& playerName() const;
  String const& shipSpecies() const;
  bool canBecomeAdmin() const;
  NetCompatibilityRules netRules() const;
  String descriptiveName() const;
  String const& account() const;

  // Register additional rpc methods from other server side services.
  [[nodiscard]] std::expected<void, StarException> registerRpcHandlers(JsonRpcHandlers const& rpcHandlers);

  // The coordinate for the world which the *player's* ship is currently
  // orbiting, if it is currently orbiting a world.
  CelestialCoordinate shipCoordinate() const;
  void setShipCoordinate(CelestialCoordinate shipCoordinate);

  SystemLocation shipLocation() const;
  void setShipLocation(SystemLocation location);

  // Warp action and warp mode to the planet the player is currently orbiting
  // valid when the player is on any ship world orbiting a location
  std::optional<std::pair<WarpAction, WarpMode>> orbitWarpAction() const;
  void setOrbitWarpAction(std::optional<std::pair<WarpAction, WarpMode>> warpAction);

  bool isAdmin() const;
  void setAdmin(bool admin);

  bool serverDebug() const;
  void setServerDebug(bool serverDebug);

  EntityDamageTeam team() const;
  void setTeam(EntityDamageTeam team);

  ShipUpgrades shipUpgrades() const;
  void setShipUpgrades(ShipUpgrades shipUpgrades);
  void setShipSpecies(String shipSpecies);

  WorldChunks shipChunks() const;
  void updateShipChunks(WorldChunks newShipChunks);


  [[nodiscard]] std::expected<void, StarException> readUpdate(ByteArray data);
  [[nodiscard]] std::expected<ByteArray, StarException> writeUpdate();

  void setPlayerWorld(std::shared_ptr<WorldServerThread> worldThread);
  std::shared_ptr<WorldServerThread> playerWorld() const;
  WorldId playerWorldId() const;
  void clearPlayerWorld();

  void setSubWorld(ClientSubWorldId subWorldId, std::shared_ptr<WorldServerThread> worldThread);
  std::shared_ptr<WorldServerThread> subWorld(ClientSubWorldId subWorldId) const;
  bool hasSubWorld(ClientSubWorldId subWorldId) const;
  void clearSubWorld(ClientSubWorldId subWorldId);
  List<ClientSubWorldId> subWorlds() const;

  void setSystemWorld(std::shared_ptr<SystemWorldServerThread> systemWorldThread);
  std::shared_ptr<SystemWorldServerThread> systemWorld() const;
  void clearSystemWorld();

  WarpToWorld playerReturnWarp() const;
  void setPlayerReturnWarp(WarpToWorld warp);

  WarpToWorld playerReviveWarp() const;
  void setPlayerReviveWarp(WarpToWorld warp);
  
  [[nodiscard]] std::expected<void, StarException> customWorldRequested(String name, RpcPromiseKeeper<WorldChunks> promise);
  [[nodiscard]] std::expected<void, StarException> customWorldReceived(String name, WorldChunks chunks);
  [[nodiscard]] std::expected<void, StarException> failWorldRequests();
  
  std::optional<WorldChunks> customWorldChunks(String name) const;
  void updateCustomWorldChunks(String name, WorldChunks newWorldChunks);
  void setCustomWorldActive(String name, bool active);
  List<String> customWorlds() const;
  
  [[nodiscard]] std::expected<void, StarException> cleanInactiveCustomWorlds();

  // Store and load the data for this client that should be persisted on the
  // server, such as celestial log data, admin state, team, and current ship
  // location, and warp history.  Does not store ship data or ship upgrades.
  [[nodiscard]] std::expected<void, StarException> loadServerData(Json const& store);
  [[nodiscard]] std::expected<Json, StarException> storeServerData();

  std::chrono::time_point<std::chrono::steady_clock, std::chrono::milliseconds> creationTime() const;

private:
  ServerClientContext(Root& root, ConnectionId clientId, std::optional<HostAddress> remoteAddress, NetCompatibilityRules netRules, Uuid playerUuid,
      String playerName, String shipSpecies, bool canBecomeAdmin, WorldChunks initialShipChunks, StringSet validShipSpecies, String account);
  [[nodiscard]] std::expected<void, StarException> initialize();
  struct CustomWorld {
    WorldChunks chunks;
    WorldChunks chunksUpdate;
    
    bool active;
    
    CustomWorld(WorldChunks initialChunks);
    CustomWorld();
  };
  
  Root& m_root;
  ConnectionId const m_clientId;
  std::optional<HostAddress> const m_remoteAddress;
  NetCompatibilityRules m_netRules;
  Uuid const m_playerUuid;
  String const m_playerName;
  String const m_account;
  String m_shipSpecies;
  // The species the server has ship worlds for; the client may set its own
  // ship species through the 'ship.setShipSpecies' rpc, so it is validated
  // against this.
  StringSet m_validShipSpecies;
  bool const m_canBecomeAdmin;
  
  bool m_serverDebug = false;

  mutable std::recursive_mutex m_mutex;

  WorldChunks m_shipChunks;
  WorldChunks m_shipChunksUpdate;

  SystemLocation m_shipSystemLocation;
  JsonRpc m_rpc;
  std::shared_ptr<WorldServerThread> m_worldThread;
  WarpToWorld m_returnWarp;
  WarpToWorld m_reviveWarp;
  
  HashMap<ClientSubWorldId, std::shared_ptr<WorldServerThread>> m_subWorldThreads;

  std::shared_ptr<SystemWorldServerThread> m_systemWorldThread;

  NetElementTopGroup m_netGroup;
  std::uint64_t m_netVersion = 0;
  std::chrono::time_point<std::chrono::steady_clock, std::chrono::milliseconds> m_creationTime;
  
  StringMap<RpcPromiseKeeper<WorldChunks>> m_worldRequests;
  StringMap<CustomWorld> m_customWorlds;

  NetElementData<std::optional<std::pair<WarpAction, WarpMode>>> m_orbitWarpActionNetState;
  NetElementData<WorldId> m_playerWorldIdNetState;
  NetElementBool m_isAdminNetState;
  NetElementData<EntityDamageTeam> m_teamNetState;
  NetElementData<ShipUpgrades> m_shipUpgrades;
  NetElementData<CelestialCoordinate> m_shipCoordinate;
};

}

export module star.server_client_context;

export namespace Star {
using ::Star::WorldServerThread;
using ::Star::SystemWorldServerThread;
using ::Star::ServerClientContext;
}

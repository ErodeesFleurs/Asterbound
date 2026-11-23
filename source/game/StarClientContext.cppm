module;
import star.format;
import star.json;
import star.xx_hash;
import star.json_rpc;
import star.game_types;
import star.data_stream;
import star.strong_typedef;
import star.id_map;
import star.set;
import star.btree;
import star.ordered_map;
import star.lru_cache;
import star.data_stream_devices;
import star.thread;
import star.btree_database;
import star.casting;
import star.ordered_set;
import star.list;
import star.string;
import star.rpc_promise;
import star.bi_map;
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


import star.damage_types;
import star.celestial_coordinate;
import star.uuid;
import star.warping;
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
import star.player_types;

import star.net_element_basic_fields;
import star.net_element_system;

import star.net_compatibility;
import star.byte_array;
namespace Star {

class Root;

class ClientContext {
public:
  [[nodiscard]] static std::expected<std::shared_ptr<ClientContext>, StarException> create(Root& root, Uuid serverUuid, Uuid playerUuid);

  Uuid serverUuid() const;
  // The player Uuid can differ from the mainPlayer's Uuid
  //  if the player has swapped character - use this for ship saving.
  Uuid playerUuid() const;

  // The coordinate for the world which the player's ship is currently
  // orbiting.
  CelestialCoordinate shipCoordinate() const;

  std::optional<std::pair<WarpAction, WarpMode>> orbitWarpAction() const;

  // The current world id of the player
  WorldId playerWorldId() const;

  bool isAdmin() const;
  EntityDamageTeam team() const;

  std::shared_ptr<JsonRpcInterface> rpcInterface() const;

  WorldChunks newShipUpdates();
  ShipUpgrades shipUpgrades() const;
  
  StringMap<WorldChunks> newCustomWorldUpdates();

  [[nodiscard]] std::expected<void, StarException> read_update(ByteArray data);
  [[nodiscard]] std::expected<ByteArray, StarException> write_update();

  void setConnectionId(ConnectionId connectionId);
  ConnectionId connectionId() const;

  void setNetCompatibilityRules(NetCompatibilityRules netCompatibilityRules);
  NetCompatibilityRules netCompatibilityRules() const;

private:
  ClientContext(Root& root, Uuid serverUuid, Uuid playerUuid);
  [[nodiscard]] std::expected<void, StarException> initialize();
  Root& m_root;
  Uuid m_serverUuid;
  Uuid m_playerUuid;
  ConnectionId m_connectionId = 0;
  NetCompatibilityRules m_netCompatibilityRules;

  std::shared_ptr<JsonRpc> m_rpc;

  NetElementTopGroup m_netGroup;
  NetElementData<std::optional<std::pair<WarpAction, WarpMode>>> m_orbitWarpActionNetState;
  NetElementData<WorldId> m_playerWorldIdNetState;
  NetElementBool m_isAdminNetState;
  NetElementData<EntityDamageTeam> m_teamNetState;
  NetElementData<ShipUpgrades> m_shipUpgrades;
  NetElementData<CelestialCoordinate> m_shipCoordinate;
  WorldChunks m_newShipUpdates;
  StringMap<WorldChunks> m_newCustomWorldUpdates;
};

}

export module star.client_context;

export namespace Star {
using ::Star::ClientContext;
}

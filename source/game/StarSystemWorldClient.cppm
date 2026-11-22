module;
import star.format;
import star.xx_hash;
import star.json;
import star.byte_array;
import star.vector;
import star.game_types;
import star.color;
import star.bi_map;
import star.random;
import star.weighted_pool;
import star.list;
import star.image_processing;
import star.hash;
import star.data_stream;
import star.string_view;
import star.thread;
import star.directives;
import star.asset_path;
import star.strong_typedef;
import star.id_map;
import star.string;
import star.map;
import star.ordered_map;
import star.casting;
import star.net_compatibility;
import std;
import star.set;
import star.worker_pool;

import star.sector_array_2d;
import star.perlin;
import star.btree;
import star.lru_cache;
import star.data_stream_devices;
import star.btree_database;
import star.ordered_set;
import star.rpc_promise;

import star.celestial_coordinate;
import star.sky_types;
import star.animation;
import star.particle;
import star.weather_types;
import star.force_regions;
import star.world_parameters;
import star.celestial_parameters;
import star.uuid;
import star.warping;
import star.sky_parameters;
import star.system_world;
import star.collision_block;
import star.liquid_types;
import star.tile_damage;
import star.tile_sector_array;
import star.world_layout;
import star.collision_generator;
import star.world_tiles;
import star.item_descriptor;
import star.celestial_types;
import star.chat_types;
import star.tile_modification;
import star.damage_types;
import star.status_types;
import star.damage;
import star.light_source;
import star.entity;
import star.interaction_types;
import star.world_geometry;
import star.wiring;
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
import star.damage_manager;
import star.net_packets;

namespace Star {

class Root;

class CelestialDatabase;
class PlayerUniverseMap;
class Celestial;
class Clock;
class ClientContext;

class SystemWorldClient : public SystemWorld {
public:
  [[nodiscard]] static std::expected<std::shared_ptr<SystemWorldClient>, StarException> create(Root& root, std::shared_ptr<Clock const> universeClock, std::shared_ptr<CelestialDatabase> celestialDatabase, std::shared_ptr<PlayerUniverseMap> clientContext);

  CelestialCoordinate currentSystem() const;

  std::optional<Vec2F> shipPosition() const;
  SystemLocation shipLocation() const;
  SystemLocation shipDestination() const;
  bool flying() const;

  [[nodiscard]] std::expected<void, StarException> update(float dt);

  List<std::shared_ptr<SystemObject>> objects() const override;
  List<Uuid> objectKeys() const override;
  std::shared_ptr<SystemObject> getObject(Uuid const& uuid) const override;

  List<std::shared_ptr<SystemClientShip>> ships() const;
  std::shared_ptr<SystemClientShip> getShip(Uuid const& uuid) const;

  Uuid spawnObject(String typeName, std::optional<Vec2F> position = {}, std::optional<Uuid> const& uuid = {}, JsonObject parameters = {});

  // returns whether the packet was handled
  [[nodiscard]] std::expected<bool, StarException> handleIncomingPacket(std::shared_ptr<Packet> packet);
  List<std::shared_ptr<Packet>> pullOutgoingPackets();
private:
  SystemWorldClient(Root& root, std::shared_ptr<Clock const> universeClock, std::shared_ptr<CelestialDatabase> celestialDatabase, std::shared_ptr<PlayerUniverseMap> universeMap);
  [[nodiscard]] std::expected<std::shared_ptr<SystemObject>, StarException> net_load_object(ByteArray net_store);
  [[nodiscard]] std::expected<std::shared_ptr<SystemClientShip>, StarException> net_load_ship(ByteArray net_store);

  // m_ship can be a null pointer, indicating that the system is not initialized
  std::shared_ptr<SystemClientShip> m_ship;
  HashMap<Uuid, std::shared_ptr<SystemObject>> m_objects;
  HashMap<Uuid, std::shared_ptr<SystemClientShip>> m_clientShips;

  std::shared_ptr<PlayerUniverseMap> m_universeMap;

  List<std::shared_ptr<Packet>> m_outgoingPackets;
};


}

export module star.system_world_client;

export namespace Star {
  using ::Star::CelestialDatabase;
  using ::Star::PlayerUniverseMap;
  using ::Star::Celestial;
  using ::Star::Clock;
  using ::Star::ClientContext;
  using ::Star::SystemWorldClient;
}

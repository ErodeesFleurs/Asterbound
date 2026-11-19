module;
import star.format;
import std;
import star.map;
import star.json;
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

namespace Star {

class Root;

struct Packet;

class SystemWorldServer : public SystemWorld {
public:
  // create new system world server
  [[nodiscard]] static std::expected<std::shared_ptr<SystemWorldServer>, StarException> create(Root& root, Vec3I location, std::shared_ptr<Clock const> universeClock, std::shared_ptr<CelestialDatabase> celestialDatabase);
  // load system world server from storage
  [[nodiscard]] static std::expected<std::shared_ptr<SystemWorldServer>, StarException> create(Root& root, Json const& diskStore, std::shared_ptr<Clock const> universeClock, std::shared_ptr<CelestialDatabase> celestialDatabase);

  [[nodiscard]] std::expected<void, StarException> setClientDestination(ConnectionId const& clientId, SystemLocation const& destination);

  // null while flying, system coordinate when in space or at a system object
  // planet coordinates while orbiting a planet
  std::shared_ptr<SystemClientShip> clientShip(ConnectionId clientId) const;
  [[nodiscard]] std::expected<SystemLocation, StarException> clientShipLocation(ConnectionId clientId) const;
  [[nodiscard]] std::expected<std::optional<std::pair<WarpAction, WarpMode>>, StarException> clientWarpAction(ConnectionId clientId) const;
  [[nodiscard]] std::expected<SkyParameters, StarException> clientSkyParameters(ConnectionId clientId) const;

  List<ConnectionId> clients() const;
  [[nodiscard]] std::expected<void, StarException> addClientShip(ConnectionId clientId, Uuid const& uuid, float shipSpeed, SystemLocation location);
  [[nodiscard]] std::expected<void, StarException> removeClientShip(ConnectionId clientId);
  List<std::shared_ptr<SystemClientShip>> shipsAtLocation(SystemLocation const& location) const;
  [[nodiscard]] std::expected<List<InstanceWorldId>, StarException> activeInstanceWorlds() const;

  // removeObject queues up object for destruction, any ships at the location
  // are moved away
  [[nodiscard]] std::expected<void, StarException> removeObject(Uuid objectUuid);
  [[nodiscard]] std::expected<bool, StarException> addObject(std::shared_ptr<SystemObject> object, bool doRangeCheck = false);

  [[nodiscard]] std::expected<void, StarException> update(float dt);

  List<std::shared_ptr<SystemObject>> objects() const override;
  List<Uuid> objectKeys() const override;
  std::shared_ptr<SystemObject> getObject(Uuid const& uuid) const override;

  List<ConnectionId> pullShipFlights();

  [[nodiscard]] std::expected<void, StarException> handleIncomingPacket(ConnectionId clientId, std::shared_ptr<Packet> packet);
  List<std::shared_ptr<Packet>> pullOutgoingPackets(ConnectionId clientId);

  bool triggeredStorage();
  [[nodiscard]] std::expected<Json, StarException> diskStore();

private:
  SystemWorldServer(Root& root, std::shared_ptr<Clock const> universeClock, std::shared_ptr<CelestialDatabase> celestialDatabase);
  struct ClientNetVersions {
    HashMap<Uuid, std::uint64_t> ships;
    HashMap<Uuid, std::uint64_t> objects;
  };

  [[nodiscard]] std::expected<void, StarException> queueUpdatePackets();

  // random position for new ships entering the system
  [[nodiscard]] std::expected<void, StarException> placeInitialObjects();
  [[nodiscard]] std::expected<void, StarException> spawnObjects();
  [[nodiscard]] std::expected<Vec2F, StarException> randomObjectSpawnPosition(RandomSource& rand) const;

  [[nodiscard]] std::expected<SkyParameters, StarException> locationSkyParameters(SystemLocation const& location) const;

  // setting this to true asynchronously triggers storage from the server thread
  bool m_triggerStorage{false};
  
  double m_lastSpawn;
  double m_objectSpawnTime;

  // objects to be destroyed as soon as there are no ships at the location
  List<Uuid> m_objectDestroyQueue;
  // ships to be destroyed after update packets have been queued
  List<Uuid> m_shipDestroyQueue;

  HashMap<ConnectionId, ClientNetVersions> m_clientNetVersions;
  HashMap<ConnectionId, Uuid> m_clientShips;
  HashMap<Uuid, std::shared_ptr<SystemObject>> m_objects;
  HashMap<Uuid, std::shared_ptr<SystemClientShip>> m_ships;
  // client ID and flight start position
  List<ConnectionId> m_shipFlights;

  HashMap<ConnectionId, List<std::shared_ptr<Packet>>> m_outgoingPackets;
};


}


export module star.system_world_server;

export namespace Star {
  using ::Star::SystemWorldServer;
  using ::Star::Packet;
}


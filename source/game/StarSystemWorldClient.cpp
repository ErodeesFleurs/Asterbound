import star.format;
import star.xx_hash;
import star.json;
import star.data_stream_extra;
import star.ordered_map;
import star.either;
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
import star.version;
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
import star.io_device;
import star.ref_ptr;
import star.logging;
import star.listener;

import star.lua_root;
import star.celestial_coordinate;
import star.sky_types;
import star.animation;
import star.particle;
import star.weather_types;
import star.force_regions;
import star.world_parameters;
import star.celestial_parameters;
import star.celestial_types;
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
import star.versioning_database;
import star.world_storage;
import star.player_types;
import star.damage_manager;
import star.net_packets;
import star.system_world_client;
import star.asset_source;
import star.assets;
import star.root_base;
import star.configuration;
import star.root;
import star.player_universe_map;


import star.celestial_database;

import star.byte_array;
import star.algorithm;
namespace Star {

SystemWorldClient::SystemWorldClient(Root& root, std::shared_ptr<Clock const> universeClock, std::shared_ptr<CelestialDatabase> celestialDatabase, std::shared_ptr<PlayerUniverseMap> universeMap)
  : SystemWorld(root, std::move(universeClock), std::move(celestialDatabase)), m_universeMap(std::move(universeMap)) {}

std::expected<std::shared_ptr<SystemWorldClient>, StarException> SystemWorldClient::create(Root& root, std::shared_ptr<Clock const> universeClock, std::shared_ptr<CelestialDatabase> celestialDatabase, std::shared_ptr<PlayerUniverseMap> universeMap) {
  auto client = std::shared_ptr<SystemWorldClient>(new SystemWorldClient(root, std::move(universeClock), std::move(celestialDatabase), std::move(universeMap)));
  auto initialized = client->initialize();
  if (!initialized)
    return std::unexpected(std::move(initialized.error()));
  return client;
}

CelestialCoordinate SystemWorldClient::currentSystem() const {
  return CelestialCoordinate(m_location);
}

std::optional<Vec2F> SystemWorldClient::shipPosition() const {
  if (m_ship)
    return m_ship->position();
  else
    return {};
}

SystemLocation SystemWorldClient::shipLocation() const {
  if (m_ship)
    return m_ship->systemLocation();
  else
    return {};
}

SystemLocation SystemWorldClient::shipDestination() const {
  if (m_ship)
    return m_ship->destination();
  else
    return {};
}

// ship is flying if the system world is uninitialized or the ship doesn't have a location
bool SystemWorldClient::flying() const {
  if (m_ship)
    return std::holds_alternative<std::monostate>(m_ship->systemLocation());
  return true;
}

std::expected<void, StarException> SystemWorldClient::update(float dt) {
  if (!m_ship)
    return {};
  auto updated = m_ship->clientUpdate(dt);
  if (!updated)
    return std::unexpected(std::move(updated.error()));
  auto location = m_ship->systemLocation();
  if (std::holds_alternative<Uuid>(location)) {
    auto const& uuid = std::get<Uuid>(location);
    if (auto object = getObject(uuid)) {
      if (object->permanent()) {
        auto parameters = object->parameters();
        if (!parameters)
          return std::unexpected(std::move(parameters.error()));
        auto mapped = m_universeMap->addMappedObject(CelestialCoordinate(m_location), uuid, object->name(), object->orbit(), std::move(*parameters));
        if (!mapped)
          return std::unexpected(std::move(mapped.error()));
      } else {
        auto mapped = m_universeMap->addMappedObject(CelestialCoordinate(m_location), uuid, object->name());
        if (!mapped)
          return std::unexpected(std::move(mapped.error()));
      }
    }
  } else if (std::holds_alternative<CelestialCoordinate>(location)) {
    auto const& coordinate = std::get<CelestialCoordinate>(location);
    if (coordinate.isPlanetaryBody() || coordinate.isSatelliteBody()) {
      auto planet = coordinate.planet();
      if (!planet)
        return std::unexpected(std::move(planet.error()));
      auto mapped = m_universeMap->addMappedCoordinate(*planet);
      if (!mapped)
        return std::unexpected(std::move(mapped.error()));
    }
  } else if (std::holds_alternative<CelestialOrbit>(location)) {
    auto const& orbit = std::get<CelestialOrbit>(location);
    if (orbit.target.isPlanetaryBody() || orbit.target.isSatelliteBody()) {
      auto planet = orbit.target.planet();
      if (!planet)
        return std::unexpected(std::move(planet.error()));
      auto mapped = m_universeMap->addMappedCoordinate(*planet);
      if (!mapped)
        return std::unexpected(std::move(mapped.error()));
    }
  }
  for (auto const& entry : m_clientShips) {
    auto updated = entry.second->clientUpdate(dt);
    if (!updated)
      return std::unexpected(std::move(updated.error()));
  }
  for (auto const& entry : m_objects) {
    auto updated = entry.second->clientUpdate(dt);
    if (!updated)
      return std::unexpected(std::move(updated.error()));
  }
  if (currentSystem().isNull()) {
    m_objects.clear();
    m_clientShips.clear();
    m_ship.reset();
    m_location = Vec3I();
  } else if (auto slave = as<CelestialSlaveDatabase>(m_celestialDatabase)) {
    return slave->signalSystem(currentSystem());
  }
  return {};
}

List<std::shared_ptr<SystemObject>> SystemWorldClient::objects() const {
  return m_objects.values();
}

List<Uuid> SystemWorldClient::objectKeys() const {
  return m_objects.keys();
}

std::shared_ptr<SystemObject> SystemWorldClient::getObject(Uuid const& uuid) const {
  return m_objects.maybe(uuid).value_or(std::shared_ptr<SystemObject>{});
}

List<std::shared_ptr<SystemClientShip>> SystemWorldClient::ships() const
{
  return m_clientShips.values();
}

std::shared_ptr<SystemClientShip> SystemWorldClient::getShip(Uuid const & uuid) const
{
  return m_clientShips.maybe(uuid).value_or(std::shared_ptr<SystemClientShip>{});
}

Uuid SystemWorldClient::spawnObject(String typeName, std::optional<Vec2F> position, std::optional<Uuid> const& uuid, JsonObject overrides) {
  Uuid objectUuid = uuid.value_or(Uuid(this->root().random()));
  m_outgoingPackets.push_back(std::make_shared<SystemObjectSpawnPacket>(std::move(typeName), objectUuid,
                                                                        std::move(position), std::move(overrides)));
  return objectUuid;
}

std::expected<bool, StarException> SystemWorldClient::handleIncomingPacket(std::shared_ptr<Packet> packet) {
  if (auto updatePacket = as<SystemWorldUpdatePacket>(packet)) {
    for (auto const& entry : updatePacket->shipUpdates) {
      auto ship = m_ship && entry.first == m_ship->uuid() ? m_ship : getShip(entry.first);
      if (!ship)
        return std::unexpected(StarPacketException::format("System update references missing ship {}", entry.first.hex()));
      auto updated = ship->readNetState(entry.second, SystemWorldTimestep);
      if (!updated)
        return std::unexpected(std::move(updated.error()));
    }
    for (auto const& entry : updatePacket->objectUpdates) {
      auto object = getObject(entry.first);
      if (!object)
        return std::unexpected(StarPacketException::format("System update references missing object {}", entry.first.hex()));
      auto updated = object->readNetState(entry.second, SystemWorldTimestep);
      if (!updated)
        return std::unexpected(std::move(updated.error()));
    }
  } else if (auto created = as<SystemObjectCreatePacket>(packet)) {
    auto object = netLoadObject(created->objectStore);
    if (!object)
      return std::unexpected(std::move(object.error()));
    auto uuid = (*object)->uuid();
    m_objects.set(uuid, std::move(*object));
  } else if (auto destroyed = as<SystemObjectDestroyPacket>(packet)) {
    m_objects.remove(destroyed->objectUuid);
    auto removed = m_universeMap->removeMappedObject(CelestialCoordinate(m_location), destroyed->objectUuid);
    if (!removed)
      return std::unexpected(std::move(removed.error()));
  } else if (auto created = as<SystemShipCreatePacket>(packet)) {
    auto ship = netLoadShip(created->shipStore);
    if (!ship)
      return std::unexpected(std::move(ship.error()));
    auto uuid = (*ship)->uuid();
    m_clientShips.set(uuid, std::move(*ship));
  } else if (auto destroyed = as<SystemShipDestroyPacket>(packet)) {
    m_clientShips.remove(destroyed->shipUuid);
  } else if (auto started = as<SystemWorldStartPacket>(packet)) {
    m_objects.clear();
    m_clientShips.clear();
    m_location = started->location;
    for (auto const& store : started->objectStores) {
      auto object = netLoadObject(store);
      if (!object)
        return std::unexpected(std::move(object.error()));
      auto uuid = (*object)->uuid();
      m_objects.set(uuid, std::move(*object));
    }
    for (auto const& store : started->shipStores) {
      auto ship = netLoadShip(store);
      if (!ship)
        return std::unexpected(std::move(ship.error()));
      auto uuid = (*ship)->uuid();
      m_clientShips.set(uuid, std::move(*ship));
    }
    auto ship = SystemClientShip::create(*this, started->clientShip.first, started->clientShip.second);
    if (!ship)
      return std::unexpected(std::move(ship.error()));
    m_ship = std::move(*ship);
    auto mapped = m_universeMap->addMappedCoordinate(CelestialCoordinate(m_location));
    if (!mapped)
      return std::unexpected(std::move(mapped.error()));
    auto filtered = m_universeMap->filterMappedObjects(CelestialCoordinate(m_location), m_objects.keys());
    if (!filtered)
      return std::unexpected(std::move(filtered.error()));
  } else {
    return false;
  }
  return true;
}

List<std::shared_ptr<Packet>> SystemWorldClient::pullOutgoingPackets() { return std::exchange(m_outgoingPackets, {}); }

std::expected<std::shared_ptr<SystemObject>, StarException> SystemWorldClient::netLoadObject(ByteArray netStore) {
  DataStreamBuffer ds(std::move(netStore));
  auto uuid = ds.read<Uuid>();
  if (!uuid)
    return std::unexpected(std::move(uuid.error()));
  auto name = ds.read<String>();
  if (!name)
    return std::unexpected(std::move(name.error()));
  auto config = systemObjectConfig(*name, *uuid);
  if (!config)
    return std::unexpected(std::move(config.error()));
  auto position = ds.read<Vec2F>();
  if (!position)
    return std::unexpected(std::move(position.error()));
  auto parameters = ds.read<JsonObject>();
  if (!parameters)
    return std::unexpected(std::move(parameters.error()));
  return SystemObject::create(root(), std::move(*config), std::move(*uuid), *position, std::move(*parameters));
}

std::expected<std::shared_ptr<SystemClientShip>, StarException> SystemWorldClient::netLoadShip(ByteArray netStore) {
  DataStreamBuffer ds(std::move(netStore));
  auto uuid = ds.read<Uuid>();
  if (!uuid)
    return std::unexpected(std::move(uuid.error()));
  auto location = ds.read<SystemLocation>();
  if (!location)
    return std::unexpected(std::move(location.error()));
  return SystemClientShip::create(*this, std::move(*uuid), *location);
}

}

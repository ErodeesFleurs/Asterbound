import star.json;
import star.xx_hash;
import star.json_extra;
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
import star.io_device;
import star.ref_ptr;
import star.logging;
import star.listener;
import star.string;
import star.map;
import star.format;
import star.version;
import star.id_map;
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
import star.math_common;

import star.system_world_server;
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
import star.asset_source;
import star.assets;
import star.root_base;
import star.configuration;
import star.root;
import star.celestial_graphics;
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
import star.lua_root;
import star.versioning_database;
import star.world_storage;
import star.player_types;
import star.damage_manager;
import star.net_packets;


import star.celestial_database;

import star.byte_array;
import star.static_random;
import star.algorithm;
import star.pythonic;
namespace Star {

SystemWorldServer::SystemWorldServer(Root& root, std::shared_ptr<Clock const> universeClock, std::shared_ptr<CelestialDatabase> celestialDatabase)
  : SystemWorld(root, std::move(universeClock), std::move(celestialDatabase)), m_lastSpawn(0), m_objectSpawnTime(0) {}

std::expected<std::shared_ptr<SystemWorldServer>, StarException> SystemWorldServer::create(Root& root, Vec3I location, std::shared_ptr<Clock const> universeClock, std::shared_ptr<CelestialDatabase> celestialDatabase) {
  auto server = std::shared_ptr<SystemWorldServer>(new SystemWorldServer(root, std::move(universeClock), std::move(celestialDatabase)));
  auto initialized = server->initialize();
  if (!initialized)
    return std::unexpected(std::move(initialized.error()));
  server->m_location = std::move(location);
  auto placed = server->placeInitialObjects();
  if (!placed)
    return std::unexpected(std::move(placed.error()));
  server->m_lastSpawn = server->time() - server->systemConfig().objectSpawnCycle;
  auto interval = root.random().randf(server->systemConfig().objectSpawnInterval[0], server->systemConfig().objectSpawnInterval[1]);
  if (!interval)
    return std::unexpected(std::move(interval.error()));
  server->m_objectSpawnTime = *interval;
  auto spawned = server->spawnObjects();
  if (!spawned)
    return std::unexpected(std::move(spawned.error()));
  return server;
}

std::expected<std::shared_ptr<SystemWorldServer>, StarException> SystemWorldServer::create(Root& root, Json const& diskStore, std::shared_ptr<Clock const> universeClock, std::shared_ptr<CelestialDatabase> celestialDatabase) {
  auto server = std::shared_ptr<SystemWorldServer>(new SystemWorldServer(root, std::move(universeClock), std::move(celestialDatabase)));
  auto initialized = server->initialize();
  if (!initialized)
    return std::unexpected(std::move(initialized.error()));
  auto locationData = diskStore.get("location");
  if (!locationData)
    return std::unexpected(std::move(locationData.error()));
  auto location = jsonToVec3I(*locationData);
  if (!location)
    return std::unexpected(std::move(location.error()));
  server->m_location = *location;
  auto objects = diskStore.getArray("objects");
  if (!objects)
    return std::unexpected(std::move(objects.error()));
  for (auto const& store : *objects) {
    auto object = SystemObject::create(root, *server, store);
    if (!object)
      return std::unexpected(std::move(object.error()));
    auto uuid = (*object)->uuid();
    server->m_objects.set(uuid, std::move(*object));
  }
  auto lastSpawn = diskStore.getDouble("lastSpawn");
  if (!lastSpawn)
    return std::unexpected(std::move(lastSpawn.error()));
  server->m_lastSpawn = *lastSpawn;
  auto interval = diskStore.getDouble("objectSpawnTime");
  if (!interval)
    return std::unexpected(std::move(interval.error()));
  server->m_objectSpawnTime = *interval;
  auto spawned = server->spawnObjects();
  if (!spawned)
    return std::unexpected(std::move(spawned.error()));
  return server;
}


std::expected<void, StarException> SystemWorldServer::setClientDestination(ConnectionId const& clientId, SystemLocation const& destination) {
  auto ship = clientShip(clientId);
  if (!ship)
    return std::unexpected(StarException::format("No system ship for client {}", clientId));
  ship->setDestination(destination);
  return {};
}

std::shared_ptr<SystemClientShip> SystemWorldServer::clientShip(ConnectionId clientId) const {
  if (auto uuid = m_clientShips.maybeRef(clientId))
    return m_ships.value(uuid->get());
  return {};
}

std::expected<SystemLocation, StarException> SystemWorldServer::clientShipLocation(ConnectionId clientId) const {
  auto ship = clientShip(clientId);
  if (!ship)
    return std::unexpected(StarException::format("No system ship for client {}", clientId));
  return ship->systemLocation();
}

std::expected<std::optional<std::pair<WarpAction, WarpMode>>, StarException> SystemWorldServer::clientWarpAction(ConnectionId clientId) const {
  auto ship = clientShip(clientId);
  if (!ship)
    return std::unexpected(StarException::format("No system ship for client {}", clientId));
  auto location = ship->systemLocation();
  if (std::holds_alternative<Uuid>(location)) {
    auto action = objectWarpAction(std::get<Uuid>(location));
    if (!action)
      return std::unexpected(std::move(action.error()));
    if (*action)
      return std::optional(std::pair<WarpAction, WarpMode>(**action, WarpMode::DeployOnly));
  } else if (std::holds_alternative<CelestialCoordinate>(location)) {
    return std::optional(std::pair<WarpAction, WarpMode>(WarpToWorld(CelestialWorldId(std::get<CelestialCoordinate>(location))), WarpMode::BeamOrDeploy));
  } else if (std::holds_alternative<Vec2F>(location)) {
    auto const& position = std::get<Vec2F>(location);
    auto allPlanets = planets();
    if (!allPlanets)
      return std::unexpected(std::move(allPlanets.error()));
    for (auto const& planet : *allPlanets) {
      auto planetPos = planetPosition(planet);
      if (!planetPos)
        return std::unexpected(std::move(planetPos.error()));
      if (std::abs(planetPos->magnitude() - position.magnitude()) > systemConfig().asteroidBeamDistance)
        continue;
      auto parameters = m_celestialDatabase->parameters(planet);
      if (!parameters)
        return std::unexpected(std::move(parameters.error()));
      if (*parameters) {
        if (auto asteroids = as<AsteroidsWorldParameters>((**parameters).visitableParameters())) {
          float targetX = (position.angle() / (2 * std::numbers::pi)) * asteroids->worldSize[0];
          return std::optional(std::pair<WarpAction, WarpMode>(WarpToWorld(CelestialWorldId(planet), SpawnTargetX(targetX)), WarpMode::DeployOnly));
        }
      }
    }
  }
  return std::optional<std::pair<WarpAction, WarpMode>>{};
}

std::expected<SkyParameters, StarException> SystemWorldServer::clientSkyParameters(ConnectionId clientId) const {
  auto ship = clientShip(clientId);
  if (!ship)
    return std::unexpected(StarException::format("No system ship for client {}", clientId));
  return locationSkyParameters(ship->systemLocation());
}

List<ConnectionId> SystemWorldServer::clients() const {
  return m_clientShips.keys();
}

std::expected<void, StarException> SystemWorldServer::addClientShip(ConnectionId clientId, Uuid const& uuid, float shipSpeed, SystemLocation location) {
  if (std::holds_alternative<Uuid>(location))
    if (!getObject(std::get<Uuid>(location)))
      location.emplace<0>();
  if (std::holds_alternative<std::monostate>(location))
    location = randomArrivalPosition();
  auto created = SystemClientShip::create(*this, uuid, shipSpeed, location);
  if (!created)
    return std::unexpected(std::move(created.error()));
  auto ship = std::move(*created);
  m_clientShips.set(clientId, ship->uuid());
  m_ships.set(ship->uuid(), ship);
  m_clientNetVersions.set(clientId, {{}, {}});
  m_outgoingPackets.set(clientId, {});
  List<ByteArray> objectStores;
  objectStores.reserve(m_objects.size());
  for (auto const& object : m_objects.valueView()) {
    auto store = object->netStore();
    if (!store)
      return std::unexpected(std::move(store.error()));
    objectStores.append(std::move(*store));
  }
  List<ByteArray> shipStores;
  shipStores.reserve(m_ships.size() - 1);
  for (auto const& otherShip : m_ships.valueView()) {
    if (otherShip->uuid() != uuid) {
      auto store = otherShip->netStore();
      if (!store)
        return std::unexpected(std::move(store.error()));
      shipStores.append(std::move(*store));
    }
  }
  std::pair<Uuid, SystemLocation> clientShip{ship->uuid(), ship->systemLocation()};
  m_outgoingPackets[clientId].append(std::make_shared<SystemWorldStartPacket>(m_location, std::move(objectStores), std::move(shipStores), std::move(clientShip)));
  for (auto otherClient : m_clientShips.keys()) {
    if (otherClient != clientId) {
      auto store = ship->netStore();
      if (!store)
        return std::unexpected(std::move(store.error()));
      m_outgoingPackets[otherClient].append(std::make_shared<SystemShipCreatePacket>(std::move(*store)));
    }
  }
  return {};
}

std::expected<void, StarException> SystemWorldServer::removeClientShip(ConnectionId clientId) {
  auto uuid = m_clientShips.get(clientId);
  if (!uuid)
    return std::unexpected(std::move(uuid.error()));
  m_shipDestroyQueue.append(uuid->get());
  m_clientShips.remove(clientId);
  m_clientNetVersions.remove(clientId);
  m_outgoingPackets.remove(clientId);
  return {};
}

List<std::shared_ptr<SystemClientShip>> SystemWorldServer::shipsAtLocation(SystemLocation const& location) const {
  List<std::shared_ptr<SystemClientShip>> ships;
  for (auto const& ship : m_ships | std::views::values) {
    if (ship->systemLocation() == location)
      ships.append(ship);
  }
  return ships;
}

std::expected<List<InstanceWorldId>, StarException> SystemWorldServer::activeInstanceWorlds() const {
  List<InstanceWorldId> worlds;
  for (auto clientId : m_clientShips | std::views::keys) {
    auto warp = clientWarpAction(clientId);
    if (!warp)
      return std::unexpected(std::move(warp.error()));
    if (*warp && std::holds_alternative<WarpToWorld>((**warp).first)) {
      auto const& action = std::get<WarpToWorld>((**warp).first);
      if (std::holds_alternative<InstanceWorldId>(action.world))
        worlds.append(std::get<InstanceWorldId>(action.world));
    }
  }
  return worlds;
}

std::expected<void, StarException> SystemWorldServer::removeObject(Uuid objectUuid) {
  auto object = getObject(objectUuid);
  if (!object)
    return std::unexpected(StarException::format("Cannot remove object with uuid '{}', object doesn't exist.", objectUuid.hex()));
  if (object->permanent())
    return std::unexpected(StarException::format("Cannot remove object with uuid '{}', object is marked permanent", objectUuid.hex()));
  if (m_objectDestroyQueue.contains(objectUuid))
    return {};
  for (auto const& entry : m_clientShips) {
    auto ship = clientShip(entry.first);
    if (!ship)
      return std::unexpected(StarException::format("No system ship for client {}", entry.first));
    auto location = ship->systemLocation();
    auto destination = ship->destination();
    if ((std::holds_alternative<Uuid>(location) && std::get<Uuid>(location) == objectUuid)
        || (std::holds_alternative<Uuid>(destination) && std::get<Uuid>(destination) == objectUuid)) {
      auto position = systemLocationPosition(objectUuid);
      if (!position)
        return std::unexpected(std::move(position.error()));
      if (!*position)
        return std::unexpected(StarException("Cannot leave a system object with no position"));
      ship->setDestination(**position);
      if (!ship->flying())
        m_shipFlights.append(entry.first);
    }
  }
  m_objectDestroyQueue.append(objectUuid);
  return {};
}

std::expected<bool, StarException> SystemWorldServer::addObject(std::shared_ptr<SystemObject> object, bool doRangeCheck) {
  if (doRangeCheck) {
    CelestialCoordinate system(m_location);
    auto orbits = m_celestialDatabase->childOrbits(system);
    if (!orbits)
      return std::unexpected(std::move(orbits.error()));
    auto outerOrbit = std::ranges::max_element(*orbits);
    if (outerOrbit == orbits->end())
      return std::unexpected(OutOfRangeException("last() called on empty list"));
    auto outer = system.child(*outerOrbit);
    if (!outer)
      return std::unexpected(std::move(outer.error()));
    auto allPlanets = planets();
    if (!allPlanets)
      return std::unexpected(std::move(allPlanets.error()));
    List<std::pair<float, float>> distances;
    for (auto const& planet : *allPlanets) {
      auto distance = planetOrbitDistance(planet);
      if (!distance)
        return std::unexpected(std::move(distance.error()));
      auto size = clusterSize(planet);
      if (!size)
        return std::unexpected(std::move(size.error()));
      distances.append({*distance, *size / 2.0});
    }
    for (auto const& existing : m_objects | std::views::values)
      if (existing->permanent())
        distances.append({existing->position().magnitude(), 0});
    auto outerDistance = planetOrbitDistance(*outer);
    if (!outerDistance)
      return std::unexpected(std::move(outerDistance.error()));
    auto outerSize = clusterSize(*outer);
    if (!outerSize)
      return std::unexpected(std::move(outerSize.error()));
    float maxRange = *outerDistance + *outerSize / 2.0 + systemConfig().clientObjectSpawnPadding;
    maxRange += systemConfig().clientObjectSpawnPadding;
    auto starSize = planetSize(system);
    if (!starSize)
      return std::unexpected(std::move(starSize.error()));
    float minRange = *starSize / 2.0 + systemConfig().clientObjectSpawnPadding;
    float radius = object->position().magnitude();
    if (radius > maxRange || radius < minRange)
      return false;
    for (auto const& distance : distances)
      if (std::abs(radius - distance.first) < distance.second + systemConfig().clientObjectSpawnPadding)
        return false;
  }
  m_objects.set(object->uuid(), object);
  auto store = object->netStore();
  if (!store)
    return std::unexpected(std::move(store.error()));
  for (auto clientId : m_clientShips | std::views::keys)
    m_outgoingPackets[clientId].append(std::make_shared<SystemObjectCreatePacket>(*store));
  m_triggerStorage = true;
  return true;
}

std::expected<void, StarException> SystemWorldServer::update(float dt) {
  for (auto const& entry : m_ships) {
    auto updated = entry.second->serverUpdate(*this, dt);
    if (!updated)
      return std::unexpected(std::move(updated.error()));
  }
  for (auto const& entry : m_objects) {
    auto updated = entry.second->serverUpdate(*this, dt);
    if (!updated)
      return std::unexpected(std::move(updated.error()));
    if (entry.second->shouldDestroy() && shipsAtLocation(entry.first).empty()) {
      auto removed = removeObject(entry.first);
      if (!removed)
        return std::unexpected(std::move(removed.error()));
    }
  }
  auto spawned = spawnObjects();
  if (!spawned)
    return std::unexpected(std::move(spawned.error()));
  auto queued = queueUpdatePackets();
  if (!queued)
    return std::unexpected(std::move(queued.error()));
  for (auto uuid : std::exchange(m_objectDestroyQueue, {})) {
    for (auto& entry : m_clientNetVersions) {
      entry.second.objects.remove(uuid);
      m_outgoingPackets[entry.first].append(std::make_shared<SystemObjectDestroyPacket>(uuid));
    }
    m_objects.remove(uuid);
    m_triggerStorage = true;
  }
  for (auto uuid : std::exchange(m_shipDestroyQueue, {})) {
    for (auto& entry : m_clientNetVersions) {
      entry.second.ships.remove(uuid);
      m_outgoingPackets[entry.first].append(std::make_shared<SystemShipDestroyPacket>(uuid));
    }
    m_ships.remove(uuid);
    m_triggerStorage = true;
  }
  return {};
}

List<std::shared_ptr<SystemObject>> SystemWorldServer::objects() const {
  return m_objects.values();
}

List<Uuid> SystemWorldServer::objectKeys() const {
  return m_objects.keys();
}

std::shared_ptr<SystemObject> SystemWorldServer::getObject(Uuid const& uuid) const {
  return m_objects.maybe(uuid).value_or(std::shared_ptr<SystemObject>{});
}

List<ConnectionId> SystemWorldServer::pullShipFlights() { return std::exchange(m_shipFlights, {}); }

std::expected<void, StarException> SystemWorldServer::queueUpdatePackets() {
  for (auto clientId : m_clientNetVersions.keys()) {
    auto& versions = m_clientNetVersions.maybeRef(clientId)->get();
    HashMap<Uuid, ByteArray> shipUpdates;
    for (auto const& ship : m_ships.valueView()) {
      auto update = ship->writeNetState(versions.ships.value(ship->uuid(), 0), {});
      if (!update)
        return std::unexpected(std::move(update.error()));
      versions.ships.set(ship->uuid(), update->second);
      if (!update->first.empty())
        shipUpdates.set(ship->uuid(), std::move(update->first));
    }
    HashMap<Uuid, ByteArray> objectUpdates;
    for (auto const& object : m_objects.valueView()) {
      auto update = object->writeNetState(versions.objects.value(object->uuid(), 0), {});
      if (!update)
        return std::unexpected(std::move(update.error()));
      versions.objects.set(object->uuid(), update->second);
      if (!update->first.empty())
        objectUpdates.set(object->uuid(), std::move(update->first));
    }
    m_outgoingPackets[clientId].append(std::make_shared<SystemWorldUpdatePacket>(std::move(objectUpdates), std::move(shipUpdates)));
  }
  return {};
}

std::expected<void, StarException> SystemWorldServer::handleIncomingPacket(ConnectionId, std::shared_ptr<Packet> packet) {
  if (auto spawn = as<SystemObjectSpawnPacket>(packet)) {
    RandomSource random(root().random().randu64());
    // value_or previously evaluated this draw even for an explicit position.
    auto randomPosition = randomObjectSpawnPosition(random);
    if (!randomPosition)
      return std::unexpected(std::move(randomPosition.error()));
    auto position = spawn->position.value_or(*randomPosition);
    auto config = systemObjectConfig(spawn->typeName, spawn->uuid);
    if (!config)
      return std::unexpected(std::move(config.error()));
    auto object = SystemObject::create(root(), std::move(*config), spawn->uuid, position, time(), spawn->parameters);
    if (!object)
      return std::unexpected(std::move(object.error()));
    auto added = addObject(std::move(*object), spawn->position.has_value());
    if (!added)
      return std::unexpected(std::move(added.error()));
  }
  return {};
}

List<std::shared_ptr<Packet>> SystemWorldServer::pullOutgoingPackets(ConnectionId clientId) {
  return std::exchange(m_outgoingPackets[clientId], {});
}

bool SystemWorldServer::triggeredStorage() {
  bool store = m_triggerStorage;
  m_triggerStorage = false;
  return store;
}

std::expected<Json, StarException> SystemWorldServer::diskStore() {
  JsonArray objects;
  objects.reserve(m_objects.size());
  for (auto const& entry : m_objects) {
    auto store = entry.second->diskStore();
    if (!store)
      return std::unexpected(std::move(store.error()));
    objects.append(std::move(*store));
  }
  return JsonObject{{"location", jsonFromVec3I(m_location)}, {"objects", std::move(objects)},
    {"lastSpawn", m_lastSpawn}, {"objectSpawnTime", m_objectSpawnTime}};
}

std::expected<void, StarException> SystemWorldServer::placeInitialObjects() {
  auto assets = root().assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  auto config = (*assets)->json("/systemworld.config");
  if (!config)
    return std::unexpected(std::move(config.error()));
  RandomSource random(staticRandomU64("SystemWorldGeneration", toString(m_location)));
  auto poolData = config->getArray("initialObjectPools");
  if (!poolData)
    return std::unexpected(std::move(poolData.error()));
  auto pools = jsonToWeightedPool<JsonArray>(*poolData);
  if (!pools)
    return std::unexpected(std::move(pools.error()));
  Json spawn(pools->select(random));
  auto count = spawn.getInt(0);
  if (!count)
    return std::unexpected(std::move(count.error()));
  if (*count > 0) {
    auto objectPoolData = spawn.getArray(1);
    if (!objectPoolData)
      return std::unexpected(std::move(objectPoolData.error()));
    auto pool = jsonToWeightedPool<String>(*objectPoolData);
    if (!pool)
      return std::unexpected(std::move(pool.error()));
    for (int i = 0; i < (int)*count; ++i) {
      Uuid uuid(root().random());
      auto config = systemObjectConfig(pool->select(random), uuid);
      if (!config)
        return std::unexpected(std::move(config.error()));
      auto position = randomObjectSpawnPosition(random);
      if (!position)
        return std::unexpected(std::move(position.error()));
      auto object = SystemObject::create(root(), std::move(*config), uuid, *position, time());
      if (!object)
        return std::unexpected(std::move(object.error()));
      (*object)->enterOrbit(CelestialCoordinate(m_location), {0, 0}, time());
      m_objects.set(uuid, std::move(*object));
    }
  }
  return {};
}

std::expected<void, StarException> SystemWorldServer::spawnObjects() {
  double diff = std::min(systemConfig().objectSpawnCycle, time() - m_lastSpawn);
  m_lastSpawn = time() - diff;
  while (diff > m_objectSpawnTime) {
    m_lastSpawn += m_objectSpawnTime;
    auto interval = root().random().randf(systemConfig().objectSpawnInterval[0], systemConfig().objectSpawnInterval[1]);
    if (!interval)
      return std::unexpected(std::move(interval.error()));
    m_objectSpawnTime = *interval;
    diff = time() - m_lastSpawn;
    auto assets = root().assets();
    if (!assets)
      return std::unexpected(std::move(assets.error()));
    auto poolJson = (*assets)->json("/systemworld.config:objectSpawnPool");
    if (!poolJson)
      return std::unexpected(std::move(poolJson.error()));
    auto poolArray = poolJson->toArray();
    if (!poolArray)
      return std::unexpected(std::move(poolArray.error()));
    auto pool = jsonToWeightedPool<String>(*poolArray);
    if (!pool)
      return std::unexpected(std::move(pool.error()));
    String name = pool->select(root().random());
    Uuid uuid(root().random());
    auto config = systemObjectConfig(name, uuid);
    if (!config)
      return std::unexpected(std::move(config.error()));
    RandomSource random(root().random().randu64());
    auto position = randomObjectSpawnPosition(random);
    if (!position)
      return std::unexpected(std::move(position.error()));
    std::shared_ptr<SystemObject> object;
    if (time() > m_lastSpawn + m_objectSpawnTime && config->moving) {
      auto allPlanets = planets();
      if (!allPlanets)
        return std::unexpected(std::move(allPlanets.error()));
      auto targets = allPlanets->filtered([this](CelestialCoordinate const& p) {
        return std::ranges::none_of(objects(), [&p](std::shared_ptr<SystemObject> const& object) { return object->orbitTarget() == p; });
      });
      if (!targets.empty()) {
        auto selected = root().random().randFrom(targets);
        if (!selected)
          return std::unexpected(std::move(selected.error()));
        auto const& target = selected->get();
        auto targetPosition = planetPosition(target);
        if (!targetPosition)
          return std::unexpected(std::move(targetPosition.error()));
        auto size = clusterSize(target);
        if (!size)
          return std::unexpected(std::move(size.error()));
        auto relative = (*position - *targetPosition).normalized() * (*size / 2.0 + config->orbitDistance);
        auto created = SystemObject::create(root(), std::move(*config), uuid, *targetPosition + relative, m_lastSpawn);
        if (!created)
          return std::unexpected(std::move(created.error()));
        object = std::move(*created);
        object->enterOrbit(target, *targetPosition, m_lastSpawn);
      }
    }
    if (!object) {
      auto created = SystemObject::create(root(), std::move(*config), uuid, *position, m_lastSpawn);
      if (!created)
        return std::unexpected(std::move(created.error()));
      object = std::move(*created);
    }
    auto added = addObject(std::move(object), false);
    if (!added)
      return std::unexpected(std::move(added.error()));
  }
  return {};
}

std::expected<Vec2F, StarException> SystemWorldServer::randomObjectSpawnPosition(RandomSource& random) const {
  List<Vec2F> ranges;
  CelestialCoordinate system(m_location);
  auto const& config = systemConfig();
  auto orbitList = m_celestialDatabase->childOrbits(system);
  if (!orbitList)
    return std::unexpected(std::move(orbitList.error()));
  auto orbits = orbitList->sorted();
  if (orbits.empty())
    return std::unexpected(OutOfRangeException("Cannot choose a spawn range in a system with no orbits"));
  auto addRange = [&](CelestialCoordinate const& inner, CelestialCoordinate const& outer) -> std::expected<void, StarException> {
    auto innerDistance = planetOrbitDistance(inner);
    if (!innerDistance)
      return std::unexpected(std::move(innerDistance.error()));
    auto innerSize = clusterSize(inner);
    if (!innerSize)
      return std::unexpected(std::move(innerSize.error()));
    auto outerDistance = planetOrbitDistance(outer);
    if (!outerDistance)
      return std::unexpected(std::move(outerDistance.error()));
    auto outerSize = clusterSize(outer);
    if (!outerSize)
      return std::unexpected(std::move(outerSize.error()));
    ranges.append(Vec2F(*innerDistance + *innerSize / 2.0 + config.objectSpawnPadding,
      *outerDistance - *outerSize / 2.0 - config.objectSpawnPadding));
    return {};
  };
  auto first = system.child(orbits[0]);
  if (!first)
    return std::unexpected(std::move(first.error()));
  auto added = addRange(system, *first);
  if (!added)
    return std::unexpected(std::move(added.error()));
  for (std::size_t i = 1; i < orbits.size(); ++i) {
    auto inner = system.child(orbits[i - 1]);
    if (!inner)
      return std::unexpected(std::move(inner.error()));
    auto outer = system.child(orbits[i]);
    if (!outer)
      return std::unexpected(std::move(outer.error()));
    auto added = addRange(*inner, *outer);
    if (!added)
      return std::unexpected(std::move(added.error()));
  }
  auto outer = system.child(orbits[orbits.size() - 1]);
  if (!outer)
    return std::unexpected(std::move(outer.error()));
  auto distance = planetOrbitDistance(*outer);
  if (!distance)
    return std::unexpected(std::move(distance.error()));
  auto size = clusterSize(*outer);
  if (!size)
    return std::unexpected(std::move(size.error()));
  float rim = *distance + *size / 2.0 + config.objectSpawnPadding;
  ranges.append(Vec2F(rim, rim + config.objectSpawnPadding));
  auto selected = random.randFrom(ranges);
  if (!selected)
    return std::unexpected(std::move(selected.error()));
  auto const& range = selected->get();
  return Vec2F::withAngle(random.randf() * std::numbers::pi * 2.0, range[0] + random.randf() * (range[1] - range[0]));
}

std::expected<SkyParameters, StarException> SystemWorldServer::locationSkyParameters(SystemLocation const& location) const {
  SkyParameters sky = systemConfig().emptySkyParameters;
  if (std::holds_alternative<CelestialCoordinate>(location)) {
    return SkyParameters::create(root(), std::get<CelestialCoordinate>(location), m_celestialDatabase);
  } else if (std::holds_alternative<Vec2F>(location)) {
    auto const& position = std::get<Vec2F>(location);
    auto allPlanets = planets();
    if (!allPlanets)
      return std::unexpected(std::move(allPlanets.error()));
    for (auto const& planet : *allPlanets) {
      auto planetPos = planetPosition(planet);
      if (!planetPos)
        return std::unexpected(std::move(planetPos.error()));
      if (std::abs(position.magnitude() - planetPos->magnitude()) > systemConfig().asteroidBeamDistance)
        continue;
      auto parameters = m_celestialDatabase->parameters(planet);
      if (!parameters)
        return std::unexpected(std::move(parameters.error()));
      if (*parameters && is<AsteroidsWorldParameters>((**parameters).visitableParameters()))
        return SkyParameters::create(root(), planet, m_celestialDatabase);
    }
  } else if (!std::holds_alternative<std::monostate>(location)) {
    CelestialCoordinate target;
    if (std::holds_alternative<Uuid>(location)) {
      auto const& uuid = std::get<Uuid>(location);
      auto object = getObject(uuid);
      if (!object)
        return std::unexpected(StarException::format("No system object {} for sky parameters", uuid.hex()));
      sky = object->skyParameters();
      if (auto orbitTarget = object->orbitTarget())
        target = *orbitTarget;
    } else if (std::holds_alternative<CelestialOrbit>(location)) {
      target = std::get<CelestialOrbit>(location).target;
    }
    if (target.isPlanetaryBody()) {
      auto parameters = m_celestialDatabase->parameters(target);
      if (!parameters)
        return std::unexpected(std::move(parameters.error()));
      if (!*parameters)
        return std::unexpected(StarException("No celestial parameters for orbited planet"));
      if (auto visitable = (**parameters).visitableParameters()) {
        if (is<TerrestrialWorldParameters>(visitable)) {
          std::uint64_t seed = staticRandomU64(toString(m_location));
          List<CelestialParameters> worlds;
          auto planet = m_celestialDatabase->parameters(target);
          if (!planet)
            return std::unexpected(std::move(planet.error()));
          if (*planet)
            worlds.append(**planet);
          auto children = m_celestialDatabase->children(target);
          if (!children)
            return std::unexpected(std::move(children.error()));
          for (auto const& coordinate : *children) {
            auto satellite = m_celestialDatabase->parameters(coordinate);
            if (!satellite)
              return std::unexpected(std::move(satellite.error()));
            if (*satellite)
              worlds.append(**satellite);
          }
          for (std::size_t i = 0; i < worlds.size(); ++i) {
            auto const& world = worlds[i];
            Vec2F position{staticRandomFloat(seed, world.seed(), "x"), staticRandomFloat(seed, world.seed(), "y")};
            auto drawables = CelestialGraphics::drawWorld(root(), world, i > 0 ? worlds[0] : CelestialParameters());
            if (!drawables)
              return std::unexpected(std::move(drawables.error()));
            sky.nearbyMoons.append({std::move(*drawables), position});
          }
        } else {
          auto horizons = CelestialGraphics::worldHorizonImages(root(), **parameters);
          if (!horizons)
            return std::unexpected(std::move(horizons.error()));
          sky.horizonImages.insertAllAt(0, *horizons);
        }
      }
    }
  }
  return sky;
}

}

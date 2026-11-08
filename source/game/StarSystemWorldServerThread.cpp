import star.xx_hash;
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
import star.either;
import star.id_map;
import star.math_common;
import star.string;
import star.format;
import star.version;
import star.ordered_map;
import star.casting;
import star.net_compatibility;
import std;
import star.optional;
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
import star.output_proxy;
import star.listener;

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
import star.system_world_server;
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
import star.world_storage;
import star.player_types;
import star.damage_manager;
import star.net_packets;
import star.system_world_server_thread;
import star.asset_source;
import star.assets;
import star.root_base;
import star.configuration;
import star.root;

import star.versioning_database;
import star.tick_rate_monitor;

import star.algorithm;
namespace Star {

SystemWorldServerThread::SystemWorldServerThread(Vec3I const& location, std::shared_ptr<SystemWorldServer> systemWorld, String storageFile)
  : Thread(Star::format("SystemWorldServer: {}", location))
  , m_systemLocation(location)
  , m_systemWorld(std::move(systemWorld))
  , m_storageFile(std::move(storageFile))
{
}

SystemWorldServerThread::~SystemWorldServerThread() {
  m_stopSource.request_stop();
  auto joined = join();
  if (!joined)
    EmergencyLog::error("System world thread cleanup failed: {}", Star::output_exception(joined.error(), true));
  if (!isJoined()) {
    EmergencyLog::error("System world thread {} remains unjoined during owner destruction", name());
    Star::fatal_error("Cannot destroy a system world thread with an unjoined native worker", false);
  }
}

Vec3I SystemWorldServerThread::location() const {
  return m_systemLocation;
}

List<ConnectionId> SystemWorldServerThread::clients() {
  return m_clients.values();
}

std::expected<void, StarException> SystemWorldServerThread::addClient(ConnectionId clientId, Uuid const& uuid, float shipSpeed, SystemLocation const& location) {
  std::unique_lock locker(m_mutex);
  m_clients.add(clientId);
  m_outgoingPacketQueue.set(clientId, {});
  auto added = m_systemWorld->addClientShip(clientId, uuid, shipSpeed, location);
  if (!added)
    return std::unexpected(std::move(added.error()));
  auto shipLocation = m_systemWorld->clientShipLocation(clientId);
  if (!shipLocation)
    return std::unexpected(std::move(shipLocation.error()));
  auto sky = m_systemWorld->clientSkyParameters(clientId);
  if (!sky)
    return std::unexpected(std::move(sky.error()));
  m_clientShipLocations.set(clientId, {std::move(*shipLocation), std::move(*sky)});
  auto warp = m_systemWorld->clientWarpAction(clientId);
  if (!warp)
    return std::unexpected(std::move(warp.error()));
  if (*warp)
    m_clientWarpActions.set(clientId, std::move(**warp));
  return {};
}

std::expected<void, StarException> SystemWorldServerThread::removeClient(ConnectionId clientId) {
  std::unique_lock locker(m_mutex);
  auto removed = m_systemWorld->removeClientShip(clientId);
  if (!removed)
    return std::unexpected(std::move(removed.error()));
  m_clients.remove(clientId);
  m_clientShipDestinations.remove(clientId);
  m_clientShipLocations.remove(clientId);
  m_outgoingPacketQueue.remove(clientId);
  return {};
}

void SystemWorldServerThread::setPause(std::shared_ptr<const std::atomic<bool>> pause) {
  m_pause = std::move(pause);
}

std::expected<bool, StarException> SystemWorldServerThread::start() {
  if (!isJoined())
    return false;
  m_stopSource = std::stop_source();
  return Thread::start();
}

std::expected<void, StarException> SystemWorldServerThread::run() {
  auto stopToken = m_stopSource.get_token();
  TickRateApproacher tickApproacher(1.0 / SystemWorldTimestep, 0.5);
  while (!stopToken.stop_requested()) {
    m_systemWorld->root().logMap().set(Star::format("system_{}_update_rate", m_systemLocation), Star::format("{:4.2f}Hz", tickApproacher.rate()));
    auto updated = update();
    if (!updated)
      return std::unexpected(std::move(updated.error()));
    m_periodicStorage -= 1.0 / tickApproacher.rate();
    if (m_triggerStorage || m_periodicStorage <= 0.0) {
      m_triggerStorage = false;
      m_periodicStorage = 300.0;
      auto stored = store();
      if (!stored)
        return std::unexpected(std::move(stored.error()));
    }
    tickApproacher.tick();
    double spareTime = tickApproacher.spareTime();
    if (spareTime > 0)
      sleepPrecise(static_cast<std::uint64_t>(std::floor(spareTime * 1000)));
  }
  return store();
}

void SystemWorldServerThread::stop() {
  m_stopSource.request_stop();
}

std::expected<void, StarException> SystemWorldServerThread::update() {
  std::unique_lock queueLocker(m_queueMutex);
  std::unique_lock locker(m_mutex);
  for (auto const& packet : std::exchange(m_incomingPacketQueue, {})) {
    auto handled = m_systemWorld->handleIncomingPacket(packet.first, packet.second);
    if (!handled)
      return std::unexpected(std::move(handled.error()));
  }
  for (auto const& entry : std::exchange(m_clientShipActions, {})) {
    auto ship = m_systemWorld->clientShip(entry.first);
    auto acted = entry.second(optionalRef(ship));
    if (!acted)
      return std::unexpected(std::move(acted.error()));
  }
  if (!m_pause || !*m_pause) {
    auto updated = m_systemWorld->update(SystemWorldTimestep * m_systemWorld->root().timing().timescale);
    if (!updated)
      return std::unexpected(std::move(updated.error()));
  }
  m_triggerStorage = m_systemWorld->triggeredStorage();
  for (auto const& destination : std::exchange(m_clientShipDestinations, {})) {
    auto set = m_systemWorld->setClientDestination(destination.first, destination.second);
    if (!set)
      return std::unexpected(std::move(set.error()));
  }
  auto active = m_systemWorld->activeInstanceWorlds();
  if (!active)
    return std::unexpected(std::move(active.error()));
  m_activeInstanceWorlds = std::move(*active);
  for (auto clientId : m_clients) {
    m_outgoingPacketQueue[clientId].append_range(m_systemWorld->pullOutgoingPackets(clientId));
    auto location = m_systemWorld->clientShipLocation(clientId);
    if (!location)
      return std::unexpected(std::move(location.error()));
    auto& stored = m_clientShipLocations[clientId];
    if (stored.first != *location) {
      stored.first = std::move(*location);
      auto sky = m_systemWorld->clientSkyParameters(clientId);
      if (!sky)
        return std::unexpected(std::move(sky.error()));
      stored.second = std::move(*sky);
    }
    auto warp = m_systemWorld->clientWarpAction(clientId);
    if (!warp)
      return std::unexpected(std::move(warp.error()));
    if (*warp)
      m_clientWarpActions.set(clientId, std::move(**warp));
    else
      m_clientWarpActions.remove(clientId);
  }
  queueLocker.unlock();
  if (m_updateAction)
    return m_updateAction(*this);
  return {};
}

void SystemWorldServerThread::setClientDestination(ConnectionId clientId, SystemLocation const& destination) {
  std::unique_lock locker(m_queueMutex);
  m_clientShipDestinations.set(clientId, destination);
}

std::expected<void, StarException> SystemWorldServerThread::executeClientShipAction(ConnectionId clientId, ClientShipAction action) {
  if (!action)
    return std::unexpected(StarException("System client ship action cannot be empty"));
  std::unique_lock locker(m_queueMutex);
  m_clientShipActions.push_back({clientId, std::move(action)});
  return {};
}

std::expected<SystemLocation, StarException> SystemWorldServerThread::clientShipLocation(ConnectionId clientId) {
  std::shared_lock locker(m_queueMutex);
  if (m_clientShipDestinations.contains(clientId))
    return SystemLocation{};
  auto location = m_clientShipLocations.get(clientId);
  if (!location)
    return std::unexpected(std::move(location.error()));
  return location->get().first;
}

std::optional<std::pair<WarpAction, WarpMode>> SystemWorldServerThread::clientWarpAction(ConnectionId clientId) {
  std::shared_lock locker(m_queueMutex);
  if (m_clientShipDestinations.contains(clientId))
    return {};
  return m_clientWarpActions.maybe(clientId);
}

std::expected<SkyParameters, StarException> SystemWorldServerThread::clientSkyParameters(ConnectionId clientId) {
  std::shared_lock locker(m_queueMutex);
  auto location = m_clientShipLocations.get(clientId);
  if (!location)
    return std::unexpected(std::move(location.error()));
  return location->get().second;
}

List<InstanceWorldId> SystemWorldServerThread::activeInstanceWorlds() const {
  return m_activeInstanceWorlds;
}

void SystemWorldServerThread::setUpdateAction(std::function<std::expected<void, StarException>(SystemWorldServerThread&)> updateAction) {
  m_updateAction = std::move(updateAction);
}

void SystemWorldServerThread::pushIncomingPacket(ConnectionId clientId, std::shared_ptr<Packet> packet) {
  std::unique_lock locker(m_queueMutex);
  m_incomingPacketQueue.push_back({std::move(clientId), std::move(packet)});
}

List<std::shared_ptr<Packet>> SystemWorldServerThread::pullOutgoingPackets(ConnectionId clientId) {
  std::unique_lock locker(m_queueMutex);
  return std::exchange(m_outgoingPacketQueue[clientId], {});
}

std::expected<void, StarException> SystemWorldServerThread::store() {
  std::shared_lock locker(m_mutex);
  auto data = m_systemWorld->diskStore();
  if (!data)
    return std::unexpected(std::move(data.error()));
  locker.unlock();
  auto logged = m_systemWorld->root().logger().debug("Trigger disk storage for system world {}:{}:{}", m_systemLocation.x(), m_systemLocation.y(), m_systemLocation.z());
  if (!logged)
    return std::unexpected(std::move(logged.error()));
  auto versioning = m_systemWorld->root().versioningDatabase();
  if (!versioning)
    return std::unexpected(std::move(versioning.error()));
  auto versioned = (*versioning)->makeCurrentVersionedJson("System", *data);
  if (!versioned)
    return std::unexpected(std::move(versioned.error()));
  return VersionedJson::write_file(*versioned, m_storageFile);
}

}

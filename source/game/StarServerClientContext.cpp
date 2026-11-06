import star.net_element;
import star.json;
import star.xx_hash;
import star.json_extra;
import star.id_map;
import star.ordered_map;
import star.lru_cache;
import star.thread;
import star.random;
import star.ttl_cache;
import star.casting;
import star.data_stream;
import star.bi_map;
import star.game_types;
import star.json_rpc;
import star.rpc_promise;
import star.string;
import star.either;
import star.strong_typedef;
import star.data_stream_extra;
import star.logging;
import star.lua;
import star.list;
import star.set;
import star.color;
import star.image_processing;
import star.hash;
import star.string_view;
import star.directives;
import star.asset_path;
import star.btree;
import star.data_stream_devices;
import star.btree_database;
import star.ordered_set;
import star.format;
import star.version;
import star.perlin;
import star.weighted_pool;
import std;
import star.worker_pool;

import star.sector_array_2d;
import star.image;
import star.net_compatibility;
import star.map;
import star.io_device;
import star.ref_ptr;
import star.listener;

import star.lua_root;
import star.drawable;
import star.item;
import star.item_descriptor;
import star.item_database;
import star.item_recipe;
import star.uuid;
import star.damage_types;
import star.host_address;
import star.celestial_coordinate;
import star.warping;
import star.player_types;
import star.liquid_types;
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
import star.inspectable_entity;
import star.plant;
import star.plant_database;
import star.biome_placement;
import star.versioning_database;
import star.world_storage;
import star.sky_parameters;
import star.system_world;
import star.tile_modification;
import star.world;
import star.celestial_types;
import star.chat_types;
import star.wiring;
import star.damage_manager;
import star.net_packets;
import star.cellular_light_array;
import star.cellular_lighting;
import star.cellular_liquid;
import star.world_structure;
import star.lua_components;
import star.entity_rendering_types;
import star.sky_render_data;
import star.parallax;
import star.world_render_data;
import star.world_client_state;
import star.interpolation_tracker;
import star.spawn_type_database;
import star.spawner;
import star.world_server;
import star.world_server_thread;
import star.world_geometry;
import star.status_types;
import star.damage;
import star.light_source;
import star.entity;
import star.scripted_entity;
import star.tile_damage;
import star.interaction_types;
import star.quest_descriptor;
import star.interactive_entity;
import star.collision_block;
import star.tile_entity;
import star.container_entity;
import star.asset_source;
import star.assets;
import star.configuration;
import star.root;
import star.server_client_context;
import star.universe_settings;



import star.byte_array;
import star.algorithm;
namespace Star {

ServerClientContext::ServerClientContext(Root& root, ConnectionId clientId, std::optional<HostAddress> remoteAddress, NetCompatibilityRules netRules, Uuid playerUuid,
    String playerName, String shipSpecies, bool canBecomeAdmin, WorldChunks initialShipChunks, StringSet validShipSpecies, String account)
  : m_root(root),
    m_clientId(clientId),
    m_remoteAddress(remoteAddress),
    m_netRules(netRules),
    m_playerUuid(playerUuid),
    m_playerName(std::move(playerName)),
    m_account(std::move(account)),
    m_shipSpecies(std::move(shipSpecies)),
    m_validShipSpecies(std::move(validShipSpecies)),
    m_canBecomeAdmin(canBecomeAdmin),
    m_shipChunks(std::move(initialShipChunks)),
    m_rpc(root.logger()) {
}

std::expected<std::shared_ptr<ServerClientContext>, StarException> ServerClientContext::create(
    Root& root, ConnectionId clientId, std::optional<HostAddress> remoteAddress, NetCompatibilityRules netRules, Uuid playerUuid,
    String playerName, String shipSpecies, bool canBecomeAdmin, WorldChunks initialShipChunks, StringSet validShipSpecies, String account) {
  auto context = std::shared_ptr<ServerClientContext>(new ServerClientContext(root, clientId, std::move(remoteAddress), netRules, std::move(playerUuid),
      std::move(playerName), std::move(shipSpecies), canBecomeAdmin, std::move(initialShipChunks), std::move(validShipSpecies), std::move(account)));
  auto initialized = context->initialize();
  if (!initialized)
    return std::unexpected(std::move(initialized.error()));
  return context;
}

std::expected<void, StarException> ServerClientContext::initialize() {
  auto upgradeHandler = m_rpc.registerHandler("ship.applyShipUpgrades", [&self = *this](Json const& args) -> std::expected<Json, StarException> {
    std::unique_lock locker(self.m_mutex, std::defer_lock);
    lockWithDiagnostics(locker);
    auto upgrades = self.shipUpgrades();
    auto applied = upgrades.apply(args);
    if (!applied)
      return std::unexpected(std::move(applied.error()));
    self.setShipUpgrades(std::move(upgrades));
    return Json(true);
  });
  if (!upgradeHandler)
    return std::unexpected(std::move(upgradeHandler.error()));

  auto speciesHandler = m_rpc.registerHandler("ship.setShipSpecies", [&self = *this](Json const& species) -> std::expected<Json, StarException> {
    std::unique_lock locker(self.m_mutex, std::defer_lock);
    lockWithDiagnostics(locker);
    auto newSpecies = species.toString();
    if (!newSpecies)
      return std::unexpected(std::move(newSpecies.error()));
    if (!self.m_validShipSpecies.contains(*newSpecies)) {
      auto logged = self.m_root.logger().warn("ServerClientContext: <User: {}> tried to set unknown ship species '{}'", self.m_playerName, *newSpecies);
      if (!logged)
        return std::unexpected(std::move(logged.error()));
      return Json(false);
    }
    self.setShipSpecies(std::move(*newSpecies));
    return Json(true);
  });
  if (!speciesHandler)
    return std::unexpected(std::move(speciesHandler.error()));

  auto containerHandler = m_rpc.registerHandler("world.containerPutItems", [&self = *this](Json const& args) -> std::expected<Json, StarException> {
    auto items = args.getArray("items");
    if (!items)
      return std::unexpected(std::move(items.error()));
    List<ItemDescriptor> overflow;
    overflow.reserve(items->size());
    for (auto const& item : *items) {
      auto descriptor = ItemDescriptor::create(item);
      if (!descriptor)
        return std::unexpected(std::move(descriptor.error()));
      overflow.append(std::move(*descriptor));
    }
    std::unique_lock locker(self.m_mutex, std::defer_lock);
    lockWithDiagnostics(locker);
    if (self.m_worldThread) {
      auto executed = self.m_worldThread->executeAction([args, items = std::move(*items), &overflow](WorldServerThread&, WorldServer& server) -> std::expected<void, StarException> {
        auto entityId = args.getInt("entityId");
        if (!entityId)
          return std::unexpected(std::move(entityId.error()));
        auto itemDatabase = server.root().itemDatabase();
        if (!itemDatabase)
          return std::unexpected(std::move(itemDatabase.error()));
        if (auto containerEntity = as<ContainerEntity>(server.entity(*entityId))) {
          overflow.clear();
          for (auto const& itemData : items) {
            auto descriptor = ItemDescriptor::create(itemData);
            if (!descriptor)
              return std::unexpected(std::move(descriptor.error()));
            auto item = (*itemDatabase)->item(*descriptor);
            if (!item)
              return std::unexpected(std::move(item.error()));
            auto added = containerEntity->addItems(std::move(*item));
            if (!added)
              return std::unexpected(std::move(added.error()));
            auto result = added->result();
            if (!result)
              return std::unexpected(std::move(result.error()));
            if (*result && **result)
              overflow.append((**result)->descriptor());
          }
        }
        return {};
      });
      if (!executed)
        return std::unexpected(std::move(executed.error()));
    }
    return Json(overflow.transformed(std::mem_fn(&ItemDescriptor::toJson)));
  });
  if (!containerHandler)
    return std::unexpected(std::move(containerHandler.error()));

  auto flagHandler = m_rpc.registerHandler("universe.setFlag", [&self = *this](Json const& args) -> std::expected<Json, StarException> {
    auto flagName = args.toString();
    if (!flagName)
      return std::unexpected(std::move(flagName.error()));
    std::unique_lock locker(self.m_mutex, std::defer_lock);
    lockWithDiagnostics(locker);
    if (self.m_worldThread) {
      auto executed = self.m_worldThread->executeAction([flagName = std::move(*flagName)](WorldServerThread&, WorldServer& server) -> std::expected<void, StarException> {
        return server.universeSettings()->setFlag(flagName);
      });
      if (!executed)
        return std::unexpected(std::move(executed.error()));
    }
    return Json();
  });
  if (!flagHandler)
    return std::unexpected(std::move(flagHandler.error()));

  for (auto element : std::initializer_list<std::reference_wrapper<NetElement>>{
      m_orbitWarpActionNetState, m_playerWorldIdNetState, m_isAdminNetState,
      m_teamNetState, m_shipUpgrades, m_shipCoordinate}) {
    auto added = m_netGroup.addNetElement(element);
    if (!added)
      return std::unexpected(std::move(added.error()));
  }
  m_creationTime = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() + std::chrono::microseconds(500));
  return {};
}

ConnectionId ServerClientContext::clientId() const {
  return m_clientId;
}

std::optional<HostAddress> const& ServerClientContext::remoteAddress() const {
  return m_remoteAddress;
}

Uuid const& ServerClientContext::playerUuid() const {
  return m_playerUuid;
}

String const& ServerClientContext::playerName() const {
  return m_playerName;
}

String const& ServerClientContext::account() const {
  return m_account;
}

String const& ServerClientContext::shipSpecies() const {
  return m_shipSpecies;
}

bool ServerClientContext::canBecomeAdmin() const {
  return m_canBecomeAdmin;
}

NetCompatibilityRules ServerClientContext::netRules() const {
  return m_netRules;
}

String ServerClientContext::descriptiveName() const {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lockWithDiagnostics(locker);
  String hostName = m_remoteAddress ? toString(*m_remoteAddress) : "local";
  return strf("'{}' <{}> ({})", m_playerName, m_clientId, hostName);
}

std::expected<void, StarException> ServerClientContext::registerRpcHandlers(JsonRpcHandlers const& rpcHandlers) {
  return m_rpc.registerHandlers(rpcHandlers);
}

CelestialCoordinate ServerClientContext::shipCoordinate() const {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lockWithDiagnostics(locker);
  return m_shipCoordinate.get();
}

void ServerClientContext::setShipCoordinate(CelestialCoordinate system) {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lockWithDiagnostics(locker);
  m_shipCoordinate.set(system);
}

SystemLocation ServerClientContext::shipLocation() const {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lockWithDiagnostics(locker);
  return m_shipSystemLocation;
}

void ServerClientContext::setShipLocation(SystemLocation location) {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lockWithDiagnostics(locker);
  m_shipSystemLocation = location;
}

std::optional<std::pair<WarpAction, WarpMode>> ServerClientContext::orbitWarpAction() const {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lockWithDiagnostics(locker);
  return m_orbitWarpActionNetState.get();
}

void ServerClientContext::setOrbitWarpAction(std::optional<std::pair<WarpAction, WarpMode>> warpAction) {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lockWithDiagnostics(locker);
  m_orbitWarpActionNetState.set(warpAction);
}

bool ServerClientContext::isAdmin() const {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lockWithDiagnostics(locker);
  return m_isAdminNetState.get();
}

void ServerClientContext::setAdmin(bool admin) {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lockWithDiagnostics(locker);
  m_isAdminNetState.set(admin);
}

bool ServerClientContext::serverDebug() const {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lockWithDiagnostics(locker);
  return m_serverDebug;
}

void ServerClientContext::setServerDebug(bool serverDebug) {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lockWithDiagnostics(locker);
  m_serverDebug = serverDebug;
}

EntityDamageTeam ServerClientContext::team() const {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lockWithDiagnostics(locker);
  return m_teamNetState.get();
}

void ServerClientContext::setTeam(EntityDamageTeam team) {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lockWithDiagnostics(locker);
  m_teamNetState.set(team);
}

ShipUpgrades ServerClientContext::shipUpgrades() const {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lockWithDiagnostics(locker);
  return m_shipUpgrades.get();
}

void ServerClientContext::setShipUpgrades(ShipUpgrades upgrades) {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lockWithDiagnostics(locker);
  m_shipUpgrades.set(upgrades);
}

void ServerClientContext::setShipSpecies(String shipSpecies) {
  m_shipSpecies = shipSpecies;
}

WorldChunks ServerClientContext::shipChunks() const {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lockWithDiagnostics(locker);
  return m_shipChunks;
}

void ServerClientContext::updateShipChunks(WorldChunks newShipChunks) {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lockWithDiagnostics(locker);
  m_shipChunksUpdate.merge(WorldStorage::getWorldChunksUpdate(m_shipChunks, newShipChunks), true);
  m_shipChunks = std::move(newShipChunks);
}

std::expected<void, StarException> ServerClientContext::readUpdate(ByteArray data) {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lockWithDiagnostics(locker);
  return m_rpc.receive(data);
}

std::expected<ByteArray, StarException> ServerClientContext::writeUpdate() {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lockWithDiagnostics(locker);
  auto rpcUpdate = m_rpc.send();
  if (!rpcUpdate)
    return std::unexpected(std::move(rpcUpdate.error()));
  ByteArray shipChunksUpdate;
  if (!m_shipChunksUpdate.empty()) {
    auto serialized = DataStreamBuffer::serialize(std::exchange(m_shipChunksUpdate, {}));
    if (!serialized)
      return std::unexpected(std::move(serialized.error()));
    shipChunksUpdate = std::move(*serialized);
  }
  auto netState = m_netGroup.writeNetState(m_netVersion, m_netRules);
  if (!netState)
    return std::unexpected(std::move(netState.error()));
  auto& [netGroupUpdate, version] = *netState;
  m_netVersion = version;
  StringMap<ByteArray> customWorldChunksUpdate;
  for (auto& p : m_customWorlds) {
    if (!p.second.chunksUpdate.empty()) {
      auto serialized = DataStreamBuffer::serialize(std::exchange(p.second.chunksUpdate, {}));
      if (!serialized)
        return std::unexpected(std::move(serialized.error()));
      customWorldChunksUpdate[p.first] = std::move(*serialized);
    }
  }
  if (rpcUpdate->empty() && shipChunksUpdate.empty() && netGroupUpdate.empty() && (m_netRules.version() < 15 || customWorldChunksUpdate.empty()))
    return ByteArray{};
  DataStreamBuffer ds;
  auto rpcWritten = ds.write(*rpcUpdate);
  if (!rpcWritten)
    return std::unexpected(std::move(rpcWritten.error()));
  auto shipWritten = ds.write(shipChunksUpdate);
  if (!shipWritten)
    return std::unexpected(std::move(shipWritten.error()));
  if (m_netRules.version() >= 15) {
    auto customWritten = ds.write(customWorldChunksUpdate);
    if (!customWritten)
      return std::unexpected(std::move(customWritten.error()));
  }
  auto stateWritten = ds.write(netGroupUpdate);
  if (!stateWritten)
    return std::unexpected(std::move(stateWritten.error()));
  return ds.takeData();
}

void ServerClientContext::setSystemWorld(std::shared_ptr<SystemWorldServerThread> systemWorldThread) {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lockWithDiagnostics(locker);
  if (m_systemWorldThread == systemWorldThread)
    return;

  m_systemWorldThread = std::move(systemWorldThread);
}

std::shared_ptr<SystemWorldServerThread> ServerClientContext::systemWorld() const {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lockWithDiagnostics(locker);
  return m_systemWorldThread;
}

void ServerClientContext::clearSystemWorld() {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lockWithDiagnostics(locker);
  setSystemWorld({});
}

void ServerClientContext::setPlayerWorld(std::shared_ptr<WorldServerThread> worldThread) {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lockWithDiagnostics(locker);
  if (m_worldThread == worldThread)
    return;

  m_worldThread = std::move(worldThread);
  if (m_worldThread)
    m_playerWorldIdNetState.set(m_worldThread->worldId());
  else
    m_playerWorldIdNetState.set(WorldId());
}

std::shared_ptr<WorldServerThread> ServerClientContext::playerWorld() const {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lockWithDiagnostics(locker);
  return m_worldThread;
}

WorldId ServerClientContext::playerWorldId() const {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lockWithDiagnostics(locker);
  return m_playerWorldIdNetState.get();
}

void ServerClientContext::clearPlayerWorld() {
  setPlayerWorld({});
}

void ServerClientContext::setSubWorld(ClientSubWorldId subWorldId, std::shared_ptr<WorldServerThread> worldThread) {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lockWithDiagnostics(locker);
  if (m_subWorldThreads[subWorldId] == worldThread)
    return;

  m_subWorldThreads[subWorldId] = std::move(worldThread);
}

std::shared_ptr<WorldServerThread> ServerClientContext::subWorld(ClientSubWorldId subWorldId) const {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lockWithDiagnostics(locker);
  return m_subWorldThreads.value(subWorldId);
}

bool ServerClientContext::hasSubWorld(ClientSubWorldId subWorldId) const {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lockWithDiagnostics(locker);
  return m_subWorldThreads.contains(subWorldId);
}

void ServerClientContext::clearSubWorld(ClientSubWorldId subWorldId) {
  m_subWorldThreads.remove(subWorldId);
}

List<ClientSubWorldId> ServerClientContext::subWorlds() const {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lockWithDiagnostics(locker);
  return m_subWorldThreads.keys();
}

WarpToWorld ServerClientContext::playerReturnWarp() const {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lockWithDiagnostics(locker);
  return m_returnWarp;
}

void ServerClientContext::setPlayerReturnWarp(WarpToWorld warp) {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lockWithDiagnostics(locker);
  m_returnWarp = std::move(warp);
}

WarpToWorld ServerClientContext::playerReviveWarp() const {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lockWithDiagnostics(locker);
  return m_reviveWarp;
}

void ServerClientContext::setPlayerReviveWarp(WarpToWorld warp) {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lockWithDiagnostics(locker);
  m_reviveWarp = std::move(warp);
}

ServerClientContext::CustomWorld::CustomWorld() : chunks(WorldChunks()), chunksUpdate(WorldChunks()), active(false) {}
ServerClientContext::CustomWorld::CustomWorld(WorldChunks initialChunks) : chunks(std::move(initialChunks)), chunksUpdate(WorldChunks()), active(false) {}

std::expected<void, StarException> ServerClientContext::customWorldRequested(String name, RpcPromiseKeeper<WorldChunks> promise) {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lockWithDiagnostics(locker);
  auto added = m_worldRequests.add(std::move(name), std::move(promise));
  if (!added)
    return std::unexpected(std::move(added.error()));
  return {};
}

std::expected<void, StarException> ServerClientContext::customWorldReceived(String name, WorldChunks chunks) {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lockWithDiagnostics(locker);
  if (auto promise = m_worldRequests.maybeTake(name)) {
    auto fulfilled = promise->fulfill(chunks);
    if (!fulfilled)
      return std::unexpected(std::move(fulfilled.error()));
  }
  auto added = m_customWorlds.add(std::move(name), CustomWorld(std::move(chunks)));
  if (!added)
    return std::unexpected(std::move(added.error()));
  return {};
}

std::expected<void, StarException> ServerClientContext::failWorldRequests() {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lockWithDiagnostics(locker);
  std::optional<StarException> firstError;
  for (auto& p : m_worldRequests) {
    auto failed = p.second.fail("Client disconnected");
    if (!failed && !firstError)
      firstError = std::move(failed.error());
  }
  m_worldRequests.clear();
  if (firstError)
    return std::unexpected(std::move(*firstError));
  return {};
}

std::optional<WorldChunks> ServerClientContext::customWorldChunks(String name) const {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lockWithDiagnostics(locker);
  if (auto world = m_customWorlds.maybeRef(name))
    return world->get().chunks;
  return {};
}

void ServerClientContext::updateCustomWorldChunks(String name, WorldChunks newWorldChunks) {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lockWithDiagnostics(locker);
  auto& world = m_customWorlds[std::move(name)];
  world.chunksUpdate.merge(WorldStorage::getWorldChunksUpdate(world.chunks, newWorldChunks), true);
  world.chunks = std::move(newWorldChunks);
}

void ServerClientContext::setCustomWorldActive(String name, bool active) {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lockWithDiagnostics(locker);
  if (auto world = m_customWorlds.maybeRef(name))
    world->get().active = active;
}

List<String> ServerClientContext::customWorlds() const {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lockWithDiagnostics(locker);
  return m_customWorlds.keys();
}

std::expected<void, StarException> ServerClientContext::cleanInactiveCustomWorlds() {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lockWithDiagnostics(locker);
  for (auto const& worldName : m_customWorlds.keys()) {
    if (auto world = m_customWorlds.maybeRef(worldName)) {
      if (!world->get().active && world->get().chunksUpdate.empty()) {
        auto logged = m_root.logger().info("Removing inactive custom world '{}' for client '{}'", worldName, m_playerUuid.hex());
        if (!logged)
          return std::unexpected(std::move(logged.error()));
        m_customWorlds.remove(worldName);
      }
    }
  }
  return {};
}

std::expected<void, StarException> ServerClientContext::loadServerData(Json const& store) {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lockWithDiagnostics(locker);
  auto coordinateData = store.get("shipCoordinate");
  if (!coordinateData)
    return std::unexpected(std::move(coordinateData.error()));
  auto coordinate = CelestialCoordinate::create(*coordinateData);
  if (!coordinate)
    return std::unexpected(std::move(coordinate.error()));
  m_shipCoordinate.set(std::move(*coordinate));
  auto locationData = store.get("systemLocation");
  if (!locationData)
    return std::unexpected(std::move(locationData.error()));
  auto location = jsonToSystemLocation(*locationData);
  if (!location)
    return std::unexpected(std::move(location.error()));
  m_shipSystemLocation = std::move(*location);
  auto admin = store.getBool("isAdmin");
  if (!admin)
    return std::unexpected(std::move(admin.error()));
  setAdmin(*admin);
  auto teamData = store.get("team");
  if (!teamData)
    return std::unexpected(std::move(teamData.error()));
  auto team = EntityDamageTeam::create(*teamData);
  if (!team)
    return std::unexpected(std::move(team.error()));
  setTeam(std::move(*team));
  auto reviveData = store.get("reviveWarp");
  if (!reviveData)
    return std::unexpected(std::move(reviveData.error()));
  auto revive = WarpToWorld::create(*reviveData);
  if (!revive)
    return std::unexpected(std::move(revive.error()));
  m_reviveWarp = std::move(*revive);
  auto returnData = store.get("returnWarp");
  if (!returnData)
    return std::unexpected(std::move(returnData.error()));
  auto returnWarp = WarpToWorld::create(*returnData);
  if (!returnWarp)
    return std::unexpected(std::move(returnWarp.error()));
  m_returnWarp = std::move(*returnWarp);
  return {};
}

std::expected<Json, StarException> ServerClientContext::storeServerData() {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lockWithDiagnostics(locker);
  auto location = jsonFromSystemLocation(m_shipSystemLocation);
  auto teamData = team().toJson();
  if (!teamData)
    return std::unexpected(std::move(teamData.error()));
  auto revive = m_reviveWarp.toJson();
  if (!revive)
    return std::unexpected(std::move(revive.error()));
  auto returnWarp = m_returnWarp.toJson();
  if (!returnWarp)
    return std::unexpected(std::move(returnWarp.error()));
  return JsonObject{
    {"shipCoordinate", m_shipCoordinate.get().toJson()},
    {"systemLocation", std::move(location)},
    {"isAdmin", m_isAdminNetState.get()},
    {"team", std::move(*teamData)},
    {"reviveWarp", std::move(*revive)},
    {"returnWarp", std::move(*returnWarp)}
  };
}

std::chrono::time_point<std::chrono::steady_clock, std::chrono::milliseconds> ServerClientContext::creationTime() const {
  return m_creationTime;
}

}

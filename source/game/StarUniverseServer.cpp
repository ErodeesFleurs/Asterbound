import star.json;
import star.xx_hash;
import star.json_extra;
import star.ordered_map;
import star.either;
import star.vector;
import star.game_types;
import star.message_types;
import star.color;
import star.bi_map;
import star.random;
import star.weighted_pool;
import star.small_vector;
import star.list;
import star.image_processing;
import star.hash;
import star.data_stream;
import star.string_view;
import star.thread;
import star.time;
import star.directives;
import star.asset_path;
import star.id_map;
import star.string;
import star.lock_file;
import star.rpc_promise;
import star.byte_array;
import star.data_stream_devices;

import std;
import star.optional;
using ZSTD_CCtx = struct ZSTD_CCtx_s;
using ZSTD_DCtx = struct ZSTD_DCtx_s;
using ZSTD_DStream = ZSTD_DCtx;
using ZSTD_CStream = ZSTD_CCtx;
import star.zstd_compression;
import star.net_compatibility;
import star.set;
import star.btree;
import star.lru_cache;
import star.btree_database;
import star.casting;
import star.ordered_set;
import star.perlin;
import star.strong_typedef;
import star.worker_pool;

import star.sector_array_2d;
import star.image;
import star.map;
import star.ref_ptr;
import star.encode;
import star.bytes;
import star.format;
import star.output_proxy;
import star.version;
import star.file;
import star.logging;
import star.listener;
import star.io_device;

import star.universe_server;
import star.lua_components;
import star.lua_converters;
import star.lua_game_converters;
import star.celestial_coordinate;
import star.sky_types;
import star.animation;
import star.particle;
import star.weather_types;
import star.force_regions;
import star.world_parameters;
import star.celestial_parameters;
import star.celestial_types;
import star.universe;
import star.uuid;
import star.damage_types;
import star.world_geometry;
import star.status_types;
import star.damage;
import star.light_source;
import star.entity;
import star.collision_block;
import star.liquid_types;
import star.tile_damage;
import star.tile_sector_array;
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
import star.world_storage;
import star.tile_modification;
import star.world;
import star.chat_types;
import star.warping;
import star.wiring;
import star.player_types;
import star.sky_parameters;
import star.system_world;
import star.damage_manager;
import star.net_packets;
import star.cellular_light_array;
import star.cellular_lighting;
import star.cellular_liquid;
import star.world_structure;
import star.drawable;
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
import star.world_template;
import star.system_world_server;
import star.system_world_server_thread;
import star.p2p_networking_service;
import star.net_packet_socket;
import star.universe_connection;
import star.universe_settings;
import star.asset_source;
import star.assets;
import star.chat_processor;
import star.lua_root;
import star.configuration;
import star.root;
import star.host_address;
import star.socket;
import star.tcp;

import star.ai_database;
import star.versioning_database;
import star.universe_server_lua_bindings;
import star.celestial_lua_bindings;
import star.secure_random;
import star.biome_database;
import star.celestial_database;


import star.server_client_context;
import star.team_manager;

import star.sky;

import star.command_processor;

import star.algorithm;
namespace Star {

namespace {
constexpr char universe_lock_filename[] = "universe.lock";
}

UniverseServer::UniverseServer(ConstructionKey, Root& root, String const& storage_dir, bool is_local, LockFile&& directory_lock)
  : Thread("UniverseServer"), m_root(root), m_isLocal(is_local), m_storageDirectory(storage_dir),
    m_storage_directory_lock(std::move(directory_lock)),
    m_workerPool("UniverseServerWorkerPool"), m_storageTriggerDeadline(), m_clearBrokenWorldsDeadline(),
    m_lastClockUpdateSent(), m_tcpState(TcpState::No),
    m_maxPlayers(0), m_secureWarps(true) {}

std::expected<std::shared_ptr<UniverseServer>, StarException> UniverseServer::create(Root& root, String const& storage_dir, bool is_local) {
  if (auto result = File::create_directories(storage_dir); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = root.logger().info("UniverseServer: Acquiring universe lock file"); !result)
    return std::unexpected(std::move(result.error()));
  auto acquired = LockFile::try_acquire(File::relative_to(storage_dir, universe_lock_filename));
  if (!acquired)
    return std::unexpected(std::move(acquired.error()));
  if (!*acquired)
    return std::unexpected(UniverseServerException("Could not acquire lock for the universe directory"));
  auto server = std::make_shared<UniverseServer>(ConstructionKey{}, root, storage_dir, is_local, std::move(**acquired));
  if (auto result = server->initialize(); !result)
    return std::unexpected(std::move(result.error()));
  server->m_initialized = true;
  return server;
}

std::expected<void, StarException> UniverseServer::initialize() {
  auto assetsResult = m_root.assets();
  if (!assetsResult)
    return std::unexpected(std::move(assetsResult.error()));
  auto assets = std::move(*assetsResult);
  auto configurationResult = m_root.configuration();
  if (!configurationResult)
    return std::unexpected(std::move(configurationResult.error()));
  auto configuration = std::move(*configurationResult);
  auto digestConfig = configuration->get("serverOverrideAssetsDigest");
  if (!digestConfig)
    return std::unexpected(std::move(digestConfig.error()));
  auto digestOverride = digestConfig->opt_string();
  if (!digestOverride)
    return std::unexpected(std::move(digestOverride.error()));
  if (*digestOverride) {
    m_assetsDigest = hex_decode((**digestOverride).utf8());
    if (auto result = m_root.logger().info("UniverseServer: Overriding assets digest as '{}'", **digestOverride); !result)
      return std::unexpected(std::move(result.error()));
  } else {
    m_assetsDigest = assets->digest();
  }
  if (auto result = startLuaScripts(); !result)
    return std::unexpected(std::move(result.error()));
  auto clearConfig = configuration->get("clearUniverseFiles");
  if (!clearConfig)
    return std::unexpected(std::move(clearConfig.error()));
  auto clear = clearConfig->to_bool();
  if (!clear)
    return std::unexpected(std::move(clear.error()));
  if (*clear) {
    if (auto result = m_root.logger().info("UniverseServer: Clearing all universe files"); !result)
      return std::unexpected(std::move(result.error()));
    auto files = File::directory_entries(m_storageDirectory);
    if (!files)
      return std::unexpected(std::move(files.error()));
    for (auto const& file : *files) {
      if (!file.second && file.first != universe_lock_filename) {
        if (auto result = File::remove(File::relative_to(m_storageDirectory, file.first)); !result)
          return std::unexpected(std::move(result.error()));
      }
    }
  }
  auto celestial = CelestialMasterDatabase::create(m_root, File::relative_to(m_storageDirectory, "universe.chunks"));
  if (!celestial)
    return std::unexpected(std::move(celestial.error()));
  m_celestial_database = std::move(*celestial);
  if (auto result = m_root.logger().info("UniverseServer: Loading settings"); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = loadSettings(); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = loadTempWorldIndex(); !result)
    return std::unexpected(std::move(result.error()));
  auto playersConfig = configuration->get("maxPlayers");
  if (!playersConfig)
    return std::unexpected(std::move(playersConfig.error()));
  auto players = playersConfig->to_uint();
  if (!players)
    return std::unexpected(std::move(players.error()));
  m_maxPlayers = *players;
  auto universeConfig = assets->json("/universe_server.config");
  if (!universeConfig)
    return std::unexpected(std::move(universeConfig.error()));
  auto speciesShips = universeConfig->get_object("speciesShips");
  if (!speciesShips)
    return std::unexpected(std::move(speciesShips.error()));
  for (auto const& pair : *speciesShips) {
    auto ships = json_to_string_list(pair.second);
    if (!ships)
      return std::unexpected(std::move(ships.error()));
    m_speciesShips[pair.first] = std::move(*ships);
  }
  auto teamManager = TeamManager::create(m_root);
  if (!teamManager)
    return std::unexpected(std::move(teamManager.error()));
  m_teamManager = std::move(*teamManager);
  auto workerThreads = universeConfig->get_uint("workerPoolThreads");
  if (!workerThreads)
    return std::unexpected(std::move(workerThreads.error()));
  if (auto result = m_workerPool.start(*workerThreads); !result)
    return std::unexpected(std::move(result.error()));
  auto commands = CommandProcessor::create(m_root, *this);
  if (!commands)
    return std::unexpected(std::move(commands.error()));
  m_commandProcessor = std::move(*commands);
  m_chat_processor.set_command_handler(
    [processor = m_commandProcessor](ConnectionId client_id, String const& command, String const& arguments) {
      return processor->userCommand(client_id, command, arguments);
    });
  auto networkThreads = universeConfig->opt_uint("networkWorkerThreads");
  if (!networkThreads)
    return std::unexpected(std::move(networkThreads.error()));
  auto connections = UniverseConnectionServer::create(
    m_root.logger(),
    [this](UniverseConnectionServer& connectionServer, ConnectionId clientId, List<std::shared_ptr<Packet>> packets) {
      return packetsReceived(connectionServer, clientId, std::move(packets));
    },
    networkThreads->value_or(0));
  if (!connections)
    return std::unexpected(std::move(connections.error()));
  m_connectionServer = std::move(*connections);
  m_pause = std::make_shared<std::atomic<bool>>(false);
  auto secure_config = configuration->get_path("security.secureWarps");
  if (!secure_config)
    return std::unexpected(std::move(secure_config.error()));
  auto secureWarps = secure_config->opt_bool();
  if (!secureWarps)
    return std::unexpected(std::move(secureWarps.error()));
  m_secureWarps = secureWarps->value_or(true);
  return {};
}

Root& UniverseServer::root() const {
  return m_root;
}

UniverseServer::~UniverseServer() {
  if (auto result = close(); !result)
    EmergencyLog::error("UniverseServer: final shutdown failed: {}", Star::output_exception(ExceptionDiagnostic(result.error()), true));
  if (!shutdownThreadsJoined())
    Star::fatal_error("Cannot destroy UniverseServer with unjoined borrowed-state threads", false);
}

std::expected<void, StarException> UniverseServer::close() {
  if (m_closed)
    return {};
  m_stopSource.request_stop();
  auto joined = join();
  // A failed native join still owns a running runLoop/shutdown: do not race it.
  if (!joined && !is_joined())
    return std::unexpected(std::move(joined.error()));
  auto stopped = shutdown();
  if (stopped)
    m_closed = true;
  if (!joined) {
    if (!stopped)
      return std::unexpected(joined.error().with_related(stopped.error(), "Additional universe shutdown failure"));
    return std::unexpected(std::move(joined.error()));
  }
  return stopped;
}

void UniverseServer::setListeningTcp(bool listenTcp) {
  if (!listenTcp || m_tcpState != TcpState::Fuck)
    m_tcpState = listenTcp ? TcpState::Yes : TcpState::No;
}

std::expected<void, StarException> UniverseServer::addClient(UniverseConnection remoteConnection) {
  std::unique_lock acceptThreadsLocker(m_connectionAcceptThreadsMutex, std::defer_lock);
  lock_with_diagnostics(acceptThreadsLocker);
  if (m_stopSource.stop_requested())
    return std::unexpected(UniverseServerException("Cannot accept a connection while the universe is stopping"));
  auto thread = Thread::invoke("UniverseServer::acceptConnection", [&universe = *this, conn = std::move(remoteConnection)]() mutable {
    return universe.acceptConnection(std::move(conn), {});
  });
  if (!thread)
    return std::unexpected(std::move(thread.error()));
  m_connectionAcceptThreads.push_back(std::move(*thread));
  return {};
}

std::expected<UniverseConnection, StarException> UniverseServer::addLocalClient() {
  auto pair = LocalPacketSocket::openPair();
  if (auto result = addClient(UniverseConnection(std::move(pair.first))); !result)
    return std::unexpected(std::move(result.error()));
  return UniverseConnection(std::move(pair.second));
}

std::expected<void, StarException> UniverseServer::stop() {
  m_stopSource.request_stop();
  auto joined = join();
  if (!joined)
    return std::unexpected(std::move(joined.error()));
  return {};
}

std::expected<void, StarException> UniverseServer::setPause(bool pause) {
  std::shared_lock clientsLocker(m_clientsLock);
  // Pausing is disabled for multiplayer
  if (m_clients.size() > 1)
    pause = false;

  if (pause == *m_pause)
    return {};

  *m_pause = pause;

  if (pause)
    m_universeClock->stop();
  else
    m_universeClock->start();

  std::unique_lock locker(m_mainLock, std::defer_lock);
  lock_with_diagnostics(locker);
  for (auto const& worldId : m_worlds.keys()) {
    auto world = getWorld(worldId);
    if (!world)
      return std::unexpected(std::move(world.error()));
    if (*world) {
      locker.unlock();
      auto result = (*world)->executeAction([&pause](WorldServerThread&, WorldServer& world) -> std::expected<void, StarException> {
        world.setPause(pause);
        return {};
      });
      lock_with_diagnostics(locker);
      if (!result)
        return std::unexpected(std::move(result.error()));
    }
  }
  locker.unlock();

  for (auto& p : m_clients) {
    if (auto result = m_connectionServer->sendPackets(p.first, {std::make_shared<PausePacket>(*m_pause, m_root.timing().time_scale)}); !result)
      return std::unexpected(std::move(result.error()));
  }
  return {};
}

std::expected<void, StarException> UniverseServer::setTimescale(float timescale) {
  std::shared_lock clientsLocker(m_clientsLock);
  m_root.timing().time_scale = timescale;
  for (auto& p : m_clients) {
    if (auto result = m_connectionServer->sendPackets(p.first, {std::make_shared<PausePacket>(*m_pause, m_root.timing().time_scale)}); !result)
      return std::unexpected(std::move(result.error()));
  }
  return {};
}

void UniverseServer::setTickRate(float tickRate) {
  m_root.timing().server_time_step = 1.0f / tickRate;
}

std::expected<List<WorldId>, StarException> UniverseServer::activeWorlds() const {
  std::unique_lock locker(m_mainLock, std::defer_lock);
  lock_with_diagnostics(locker);
  return m_worlds.keys();
}

bool UniverseServer::isWorldActive(WorldId const& worldId) const {
  std::unique_lock locker(m_mainLock, std::defer_lock);
  lock_with_diagnostics(locker);
  return m_worlds.contains(worldId);
}

List<ConnectionId> UniverseServer::clientIds() const {
  std::shared_lock clientsLocker(m_clientsLock);
  return m_clients.keys();
}

std::expected<List<std::pair<ConnectionId, std::chrono::time_point<std::chrono::steady_clock, std::chrono::milliseconds>>>, StarException> UniverseServer::clientIdsAndCreationTime() const {
  List<std::pair<ConnectionId, std::chrono::time_point<std::chrono::steady_clock, std::chrono::milliseconds>>> result;
  std::shared_lock clientsLocker(m_clientsLock);
  result.reserve(m_clients.size());
  for (auto& pair : m_clients)
    result.emplace_back(pair.first, pair.second->creation_time());
  return result;
}

std::size_t UniverseServer::numberOfClients() const {
  std::shared_lock clientsLocker(m_clientsLock);
  return m_clients.size();
}

std::uint32_t UniverseServer::maxClients() const {
  return m_maxPlayers;
}

bool UniverseServer::isConnectedClient(ConnectionId clientId) const {
  std::shared_lock clientsLocker(m_clientsLock);
  return m_clients.contains(clientId);
}

std::expected<String, StarException> UniverseServer::clientDescriptor(ConnectionId clientId) const {
  std::shared_lock clientsLocker(m_clientsLock);
  if (auto client_context = m_clients.value(clientId))
    return client_context->descriptive_name();
  else
    return "disconnected_client";
}

std::expected<unsigned, StarException> UniverseServer::clientConnectionVersion(ConnectionId clientId) const {
  std::shared_lock clientsLocker(m_clientsLock);
  if (auto client_context = m_clients.value(clientId))
    return client_context->net_rules().version();
  else
    return 0;
}

std::expected<String, StarException> UniverseServer::client_nick(ConnectionId client_id) const {
  return m_chat_processor.connection_nick(client_id);
}

std::optional<ConnectionId> UniverseServer::find_nick(String const& nick) const {
  return m_chat_processor.find_nick(nick);
}

std::expected<String, StarException> UniverseServer::clientAccount(ConnectionId clientId) const {
  std::shared_lock clientsLocker(m_clientsLock);
  if (auto client_context = m_clients.value(clientId)) {
    if (!client_context->account().empty())
      return client_context->account();
  }
  return "<anonymous>";
}


std::optional<Uuid> UniverseServer::uuidForClient(ConnectionId clientId) const {
  std::shared_lock clientsLocker(m_clientsLock);
  if (auto client_context = m_clients.value(clientId))
    return client_context->player_uuid();
  return {};
}

std::optional<ConnectionId> UniverseServer::clientForUuid(Uuid const& uuid) const {
  std::shared_lock clientsLocker(m_clientsLock);
  return getClientForUuid(uuid);
}

std::expected<void, StarException> UniverseServer::admin_broadcast(String const& text) {
  return m_chat_processor.admin_broadcast(text);
}

std::expected<void, StarException> UniverseServer::admin_whisper(ConnectionId client_id, String const& text) {
  return m_chat_processor.admin_whisper(client_id, text);
}

std::expected<ServerCommandResult, StarException> UniverseServer::adminCommand(String text) {
  auto command = text.extract();
  if (!command)
    return std::unexpected(std::move(command.error()));
  return m_commandProcessor->adminCommand(*command, text);
}

std::expected<bool, StarException> UniverseServer::isAdmin(ConnectionId clientId) const {
  std::shared_lock clientsLocker(m_clientsLock);
  if (auto client_context = m_clients.value(clientId))
    return client_context->is_admin();
  return false;
}

std::expected<bool, StarException> UniverseServer::canBecomeAdmin(ConnectionId clientId) const {
  std::shared_lock clientsLocker(m_clientsLock);
  if (auto client_context = m_clients.value(clientId))
    return client_context->can_become_admin();
  return false;
}

std::expected<void, StarException> UniverseServer::setAdmin(ConnectionId clientId, bool admin) {
  std::shared_lock clientsLocker(m_clientsLock);
  if (auto client_context = m_clients.value(clientId))
    client_context->set_admin(admin);
  return {};
}

std::expected<bool, StarException> UniverseServer::serverDebug(ConnectionId clientId) const {
  std::shared_lock clientsLocker(m_clientsLock);
  if (auto client_context = m_clients.value(clientId))
    return client_context->server_debug();
  return false;
}

std::expected<void, StarException> UniverseServer::setServerDebug(ConnectionId clientId, bool serverDebug) {
  std::shared_lock clientsLocker(m_clientsLock);
  if (auto client_context = m_clients.value(clientId))
    client_context->set_server_debug(serverDebug);
  return {};
}

std::expected<bool, StarException> UniverseServer::isLocal(ConnectionId clientId) const {
  std::shared_lock clientsLocker(m_clientsLock);
  if (auto client_context = m_clients.value(clientId))
    return !client_context->remote_address();
  return false;
}

std::expected<bool, StarException> UniverseServer::isPvp(ConnectionId clientId) const {
  std::shared_lock clientsLocker(m_clientsLock);
  if (auto client_context = m_clients.value(clientId))
    return client_context->team().type == TeamType::PVP;
  return false;
}

std::expected<void, StarException> UniverseServer::setPvp(ConnectionId clientId, bool pvp) {
  std::unique_lock locker(m_mainLock, std::defer_lock);
  lock_with_diagnostics(locker);
  std::shared_lock clientsLocker(m_clientsLock);
  if (auto client_context = m_clients.value(clientId)) {
    if (pvp) {
      TeamNumber pvpTeam = m_teamManager->getPvpTeam(client_context->player_uuid());
      if (pvpTeam == 0)
        pvpTeam = soloPvpTeam(clientId);
      client_context->set_team(EntityDamageTeam(TeamType::PVP, pvpTeam));
    } else
      client_context->set_team(EntityDamageTeam(TeamType::Friendly));
  }
  return {};
}

std::expected<RpcPromise<Json>, StarException> UniverseServer::sendWorldMessage(WorldId const& worldId, String const& message, JsonArray const& args) {
  auto pair = RpcPromise<Json>::make_pair();
  std::unique_lock locker(m_mainLock, std::defer_lock);
  lock_with_diagnostics(locker);
  m_pendingWorldMessages[worldId].push_back({message, args, pair.second});
  return pair.first;
}

std::expected<void, StarException> UniverseServer::clientWarpPlayer(ConnectionId clientId, WarpAction action, bool deploy) {
  std::unique_lock locker(m_mainLock, std::defer_lock);
  lock_with_diagnostics(locker);
  m_pendingPlayerWarps[clientId] = std::pair<WarpAction, bool>(std::move(action), std::move(deploy));
  return {};
}

std::expected<void, StarException> UniverseServer::clientFlyShip(ConnectionId clientId, Vec3I const& system, SystemLocation const& location, Json const& settings) {
  std::unique_lock locker(m_mainLock, std::defer_lock);
  lock_with_diagnostics(locker);
  std::shared_lock clientsLocker(m_clientsLock);

  if (m_pendingFlights.contains(clientId) || m_queuedFlights.contains(clientId))
    return {};

  auto context = m_clients.get(clientId);
  if (!context)
    return std::unexpected(std::move(context.error()));
  auto client_context = context->get();
  if (!client_context)
    return {};

  if (system == Vec3I()) {
    m_pendingFlights.set(clientId, std::make_tuple(Vec3I(), SystemLocation(), settings));// find starter world
    return {};
  }

  auto clientSystem = client_context->system_world();
  bool sameSystem = clientSystem && clientSystem->location() == system;
  bool sameLocation = false;
  if (clientSystem) {
    auto shipLocation = clientSystem->clientShipLocation(clientId);
    if (!shipLocation)
      return std::unexpected(std::move(shipLocation.error()));
    sameLocation = *shipLocation == location;
  }
  if (m_pendingArrivals.contains(clientId) && sameSystem && !std::holds_alternative<std::monostate>(location) && !sameLocation) {
    // for continuing flight within a system, set the new destination immediately
    clientSystem->setClientDestination(clientId, location);
    return {};
  }

  // don't switch systems while already flying
  if (!m_pendingArrivals.contains(clientId) || sameSystem)
    m_pendingFlights.set(clientId, std::make_tuple(system, location, settings));
  return {};
}

std::expected<WorldId, StarException> UniverseServer::clientWorld(ConnectionId clientId) const {
  std::unique_lock locker(m_mainLock, std::defer_lock);
  lock_with_diagnostics(locker);
  std::shared_lock clientsLocker(m_clientsLock);
  if (auto client_context = m_clients.value(clientId))
    return client_context->player_world_id();
  return WorldId();
}

std::expected<CelestialCoordinate, StarException> UniverseServer::clientShipCoordinate(ConnectionId clientId) const {
  std::shared_lock clientsLocker(m_clientsLock);
  if (auto client_context = m_clients.value(clientId))
    return client_context->ship_coordinate();
  return CelestialCoordinate();
}

std::shared_ptr<Clock> UniverseServer::universeClock() const {
  return m_universeClock;
}

std::shared_ptr<UniverseSettings> UniverseServer::universeSettings() const {
  return m_universeSettings;
}

std::shared_ptr<CelestialDatabase> UniverseServer::celestial_database() {
  return m_celestial_database;
}

std::expected<bool, StarException> UniverseServer::executeForClient(ConnectionId clientId, std::function<std::expected<void, StarException>(WorldServer&, std::shared_ptr<Player>)> action) {
  std::unique_lock locker(m_mainLock, std::defer_lock);
  lock_with_diagnostics(locker);
  std::shared_lock clientsLocker(m_clientsLock);
  bool success = false;
  if (auto client_context = m_clients.value(clientId)) {
    if (auto currentWorld = client_context->player_world()) {
      locker.unlock();
      auto result = currentWorld->executeAction([clientId, action, &success, &locker](WorldServerThread&, WorldServer& worldServer) -> std::expected<void, StarException> {
        lock_with_diagnostics(locker);
        if (auto player = worldServer.clientPlayer(clientId)) {
          if (auto applied = action(worldServer, player); !applied)
            return std::unexpected(std::move(applied.error()));
          success = true;
        }
        return {};
      });
      if (!result)
        return std::unexpected(std::move(result.error()));
    }
  }
  return success;
}

std::expected<void, StarException> UniverseServer::disconnectClient(ConnectionId clientId, String const& reason) {
  std::unique_lock locker(m_mainLock, std::defer_lock);
  lock_with_diagnostics(locker);
  if (auto result = m_pendingDisconnections.add(clientId, reason); !result)
    return std::unexpected(std::move(result.error()));
  return {};
}

std::expected<void, StarException> UniverseServer::banUser(ConnectionId clientId, String const& reason, std::pair<bool, bool> banType, std::optional<int> timeout) {
  std::unique_lock locker(m_mainLock, std::defer_lock);
  lock_with_diagnostics(locker);

  auto banned = timeout ? doTempBan(clientId, reason, banType, *timeout) : doPermBan(clientId, reason, banType);
  if (!banned)
    return std::unexpected(std::move(banned.error()));
  if (auto result = m_pendingDisconnections.add(clientId, reason); !result)
    return std::unexpected(std::move(result.error()));
  return {};
}

std::expected<bool, StarException> UniverseServer::unbanUuid(String const& uuidString) {
  std::unique_lock locker(m_mainLock, std::defer_lock);
  lock_with_diagnostics(locker);
  auto config = m_root.configuration();
  if (!config)
    return std::unexpected(std::move(config.error()));
  auto json = (*config)->get("bannedUuids");
  if (!json)
    return std::unexpected(std::move(json.error()));
  auto entries = json->to_array();
  if (!entries)
    return std::unexpected(std::move(entries.error()));
  bool found = false;
  for (auto it = entries->begin(); it != entries->end();) {
    auto uuid = it->get_string("uuid");
    if (!uuid)
      return std::unexpected(std::move(uuid.error()));
    if (*uuid == uuidString) {
      found = true;
      it = entries->erase(it);
    } else {
      ++it;
    }
  }
  if (auto stored = (*config)->set("bannedUuids", *entries); !stored)
    return std::unexpected(std::move(stored.error()));
  m_tempBans.filter([&](TimeoutBan const& ban) {
    if (ban.uuid && ban.uuid->hex() == uuidString) {
      found = true;
      return false;
    }
    return true;
  });
  return found;
}

std::expected<bool, StarException> UniverseServer::unbanIp(String const& addressString) {
  std::unique_lock locker(m_mainLock, std::defer_lock);
  lock_with_diagnostics(locker);
  auto address = HostAddress::lookup(addressString);
  if (!address)
    return false;
  String clean = Star::to_string(*address);
  auto config = m_root.configuration();
  if (!config)
    return std::unexpected(std::move(config.error()));
  auto json = (*config)->get("bannedIPs");
  if (!json)
    return std::unexpected(std::move(json.error()));
  auto entries = json->to_array();
  if (!entries)
    return std::unexpected(std::move(entries.error()));
  bool found = false;
  for (auto it = entries->begin(); it != entries->end();) {
    auto ip = it->get_string("ip");
    if (!ip)
      return std::unexpected(std::move(ip.error()));
    if (*ip == clean) {
      found = true;
      it = entries->erase(it);
    } else {
      ++it;
    }
  }
  if (auto stored = (*config)->set("bannedIPs", *entries); !stored)
    return std::unexpected(std::move(stored.error()));
  m_tempBans.filter([&](TimeoutBan const& ban) {
    if (ban.ip && *ban.ip == *address) {
      found = true;
      return false;
    }
    return true;
  });
  return found;
}

std::expected<bool, StarException> UniverseServer::updatePlanetType(CelestialCoordinate const& coordinate, String const& newType, String const& weatherBiome) {
  std::unique_lock locker(m_mainLock, std::defer_lock);
  lock_with_diagnostics(locker);
  if (coordinate.is_null())
    return false;
  auto valid = m_celestial_database->coordinate_valid(coordinate);
  if (!valid)
    return std::unexpected(std::move(valid.error()));
  if (!*valid)
    return false;
  auto parameters = m_celestial_database->parameters(coordinate);
  if (!parameters)
    return std::unexpected(std::move(parameters.error()));
  if (!*parameters)
    return false;
  auto terrestrial = as<TerrestrialWorldParameters>((*parameters)->visitable_parameters());
  if (!terrestrial)
    return false;
  auto replacement = std::make_shared<TerrestrialWorldParameters>(*terrestrial);
  replacement->typeName = newType;
  auto database = m_root.biome_database();
  if (!database)
    return std::unexpected(std::move(database.error()));
  auto weather = (*database)->biome_weathers(weatherBiome, (*parameters)->seed(), terrestrial->threatLevel);
  if (!weather)
    return std::unexpected(std::move(weather.error()));
  replacement->weatherPool = std::move(*weather);
  replacement->terraformed = true;
  (*parameters)->set_visitable_parameters(replacement);
  if (auto updated = m_celestial_database->update_parameters(coordinate, **parameters); !updated)
    return std::unexpected(std::move(updated.error()));
  std::shared_lock clientsLocker(m_clientsLock);
  for (auto clientId : m_clients | std::views::keys) {
    if (auto sent = m_connectionServer->sendPackets(clientId, {std::make_shared<PlanetTypeUpdatePacket>(coordinate)}); !sent)
      return std::unexpected(std::move(sent.error()));
  }
  return true;
}

std::expected<bool, StarException> UniverseServer::setWeather(CelestialCoordinate const& coordinate, String const& weatherName, bool force) {
  std::unique_lock locker(m_mainLock, std::defer_lock);
  lock_with_diagnostics(locker);
  if (coordinate.is_null())
    return false;
  auto valid = m_celestial_database->coordinate_valid(coordinate);
  if (!valid)
    return std::unexpected(std::move(valid.error()));
  if (!*valid)
    return false;
  auto world = createWorld(CelestialWorldId(coordinate));
  if (!world)
    return std::unexpected(std::move(world.error()));
  if (!*world)
    return false;
  locker.unlock();
  if (auto set = (*world)->executeAction([&](WorldServerThread&, WorldServer& server) { return server.setWeather(weatherName, force); }); !set)
    return std::unexpected(std::move(set.error()));
  return true;
}

std::expected<StringList, StarException> UniverseServer::weatherList(CelestialCoordinate const& coordinate) {
  std::unique_lock locker(m_mainLock, std::defer_lock);
  lock_with_diagnostics(locker);
  StringList names;
  if (coordinate.is_null())
    return names;
  auto valid = m_celestial_database->coordinate_valid(coordinate);
  if (!valid)
    return std::unexpected(std::move(valid.error()));
  if (!*valid)
    return names;
  auto world = createWorld(CelestialWorldId(coordinate));
  if (!world)
    return std::unexpected(std::move(world.error()));
  if (!*world)
    return names;
  locker.unlock();
  auto action = (*world)->executeAction([&](WorldServerThread&, WorldServer& server) -> std::expected<void, StarException> {
    auto result = server.weatherList();
    if (!result)
      return std::unexpected(std::move(result.error()));
    names = std::move(*result);
    return {};
  });
  if (!action)
    return std::unexpected(std::move(action.error()));
  return names;
}

std::expected<bool, StarException> UniverseServer::sendPacket(ConnectionId clientId, std::shared_ptr<Packet> packet) {
  std::unique_lock locker(m_mainLock, std::defer_lock);
  lock_with_diagnostics(locker);
  std::shared_lock clientsLocker(m_clientsLock);
  if (!m_clients.contains(clientId))
    return false;
  clientsLocker.unlock();
  if (auto sent = m_connectionServer->sendPackets(clientId, {std::move(packet)}); !sent)
    return std::unexpected(std::move(sent.error()));
  return true;
}

std::expected<void, StarException> UniverseServer::run() {
  auto result = runLoop(m_stopSource.get_token());
  auto stopped = shutdown();
  if (!result) {
    if (!stopped)
      return std::unexpected(result.error().with_related(stopped.error(), "Additional shutdown failure"));
    return std::unexpected(std::move(result.error()));
  }
  return stopped;
}

std::expected<void, StarException> UniverseServer::runLoop(std::stop_token stopToken) {
  if (auto result = m_root.logger().info("UniverseServer: Starting UniverseServer with UUID: {}", m_universeSettings->uuid().hex()); !result)
    return std::unexpected(std::move(result.error()));
  auto assetsResult = m_root.assets();
  if (!assetsResult)
    return std::unexpected(std::move(assetsResult.error()));
  auto assets = *assetsResult;
  auto intervalJson = assets->json("/universe_server.config:mainWakeupInterval");
  if (!intervalJson)
    return std::unexpected(std::move(intervalJson.error()));
  auto interval = intervalJson->to_int();
  if (!interval)
    return std::unexpected(std::move(interval.error()));

  while (!stopToken.stop_requested()) {
    if (m_tcpState == TcpState::Yes && !m_tcpServer) {
      auto setup = [&]() -> std::expected<void, StarException> {
        auto configuration = m_root.configuration();
        if (!configuration)
          return std::unexpected(std::move(configuration.error()));
        auto bind_json = (*configuration)->get("gameServerBind");
        if (!bind_json)
          return std::unexpected(std::move(bind_json.error()));
        auto bind = bind_json->to_string();
        if (!bind)
          return std::unexpected(std::move(bind.error()));
        auto port_json = (*configuration)->get("gameServerPort");
        if (!port_json)
          return std::unexpected(std::move(port_json.error()));
        auto port = port_json->to_uint();
        if (!port)
          return std::unexpected(std::move(port.error()));
        if (!std::in_range<std::uint16_t>(*port))
          return std::unexpected(NetworkException("gameServerPort must be a port between 0 and 65535"));
        auto address = HostAddressWithPort::lookup(*bind, static_cast<std::uint16_t>(*port));
        if (!address)
          return std::unexpected(std::move(address.error()));
        auto pendingJson = assets->json("/universe_server.config:maxPendingConnections");
        if (!pendingJson)
          return std::unexpected(std::move(pendingJson.error()));
        auto pending = pendingJson->to_uint();
        if (!pending)
          return std::unexpected(std::move(pending.error()));
        if (auto result = m_root.logger().info("UniverseServer: listening for incoming TCP connections on {}", *address); !result)
          return std::unexpected(std::move(result.error()));
        auto server = TcpServer::create(m_root.logger(), *address);
        if (!server)
          return std::unexpected(std::move(server.error()));
        m_tcpServer = std::move(*server);
        return m_tcpServer->set_accept_callback([&universe = *this, maxPendingConnections = *pending, stopToken](std::shared_ptr<TcpSocket> socket) -> std::expected<void, StarException> {
          std::unique_lock acceptThreadsLocker(universe.m_connectionAcceptThreadsMutex, std::defer_lock);
          lock_with_diagnostics(acceptThreadsLocker);
          if (stopToken.stop_requested())
            return {};
          if (universe.m_connectionAcceptThreads.size() >= maxPendingConnections)
            return universe.m_root.logger().warn("UniverseServer: maximum pending connections, dropping connection from: {}", socket->remote_address().address());
          if (auto result = universe.m_root.logger().info("UniverseServer: Connection received from: {}", socket->remote_address()); !result)
            return std::unexpected(std::move(result.error()));
          auto thread = Thread::invoke("UniverseServer::acceptConnection", [&universe, socket]() -> std::expected<void, StarException> {
            auto packetSocket = TcpPacketSocket::open(universe.m_root.logger(), socket);
            if (!packetSocket)
              return std::unexpected(std::move(packetSocket.error()));
            return universe.acceptConnection(UniverseConnection(std::move(*packetSocket)), socket->remote_address().address());
          });
          if (!thread)
            return std::unexpected(std::move(thread.error()));
          universe.m_connectionAcceptThreads.push_back(std::move(*thread));
          return {};
        });
      }();
      if (!setup) {
        m_tcpState = TcpState::Fuck;
        if (m_tcpServer) {
          if (auto stopped = m_tcpServer->stop(); !stopped) {
            if (!m_tcpServer->is_closed())
              return std::unexpected(setup.error().with_related(stopped.error(), "TCP setup rollback failed"));
            setup = std::unexpected(setup.error().with_related(stopped.error(), "TCP setup rollback failed"));
          }
          m_tcpServer.reset();
        }
        if (auto logged = m_root.logger().error_exception(
              "UniverseServer: Error setting up TCP, cannot accept connections", ExceptionDiagnostic(setup.error()));
            !logged)
          return std::unexpected(std::move(logged.error()));
      }
    } else if (m_tcpState == TcpState::No && m_tcpServer) {
      if (auto logged = m_root.logger().info("UniverseServer: Not listening for incoming TCP connections"); !logged)
        return std::unexpected(std::move(logged.error()));
      if (auto stopped = m_tcpServer->stop(); !stopped) {
        if (!m_tcpServer->is_closed())
          return std::unexpected(std::move(stopped.error()));
        if (auto logged = m_root.logger().error_exception("UniverseServer: Error stopping TCP server",
                                                          ExceptionDiagnostic(stopped.error()));
            !logged)
          return std::unexpected(std::move(logged.error()));
      }
      m_tcpServer.reset();
    }

    m_root.logMap().set("universe_time", m_universeClock->time());
    auto update = [&]() -> std::expected<void, StarException> {
      if (auto result = updateLua(); !result)
        return std::unexpected(std::move(result.error()));
      if (auto result = processUniverseFlags(); !result)
        return std::unexpected(std::move(result.error()));
      if (auto result = removeTimedBan(); !result)
        return std::unexpected(std::move(result.error()));
      if (auto result = send_pending_chat(); !result)
        return std::unexpected(std::move(result.error()));
      if (auto result = updateTeams(); !result)
        return std::unexpected(std::move(result.error()));
      if (auto result = update_ships(); !result)
        return std::unexpected(std::move(result.error()));
      if (auto result = sendClockUpdates(); !result)
        return std::unexpected(std::move(result.error()));
      if (auto result = kickErroredPlayers(); !result)
        return std::unexpected(std::move(result.error()));
      if (auto result = reapConnections(); !result)
        return std::unexpected(std::move(result.error()));
      if (auto result = processPlanetTypeChanges(); !result)
        return std::unexpected(std::move(result.error()));
      if (auto result = warpPlayers(); !result)
        return std::unexpected(std::move(result.error()));
      if (auto result = flyShips(); !result)
        return std::unexpected(std::move(result.error()));
      if (auto result = arriveShips(); !result)
        return std::unexpected(std::move(result.error()));
      if (auto result = process_chat(); !result)
        return std::unexpected(std::move(result.error()));
      if (auto result = send_client_context_updates(); !result)
        return std::unexpected(std::move(result.error()));
      if (auto result = respond_to_celestial_requests(); !result)
        return std::unexpected(std::move(result.error()));
      if (auto result = clearBrokenWorlds(); !result)
        return std::unexpected(std::move(result.error()));
      if (auto result = handleWorldMessages(); !result)
        return std::unexpected(std::move(result.error()));
      if (auto result = handleUniverseMessages(); !result)
        return std::unexpected(std::move(result.error()));
      if (auto result = shutdownInactiveWorlds(); !result)
        return std::unexpected(std::move(result.error()));
      if (auto result = doTriggeredStorage(); !result)
        return std::unexpected(std::move(result.error()));
      return {};
    }();
    if (!update) {
      if (auto logged =
            m_root.logger().error_exception("UniverseServer: exception caught", ExceptionDiagnostic(update.error()));
          !logged)
        return std::unexpected(std::move(logged.error()));
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<unsigned>(*interval)));
  }
  return {};
}

bool UniverseServer::shutdownThreadsJoined() const {
  if (!is_joined() || m_workerPool.worker_count() != 0
      || (m_tcpServer && !m_tcpServer->is_closed())
      || (m_connectionServer && !m_connectionServer->isClosed()))
    return false;
  for (auto const& thread : m_connectionAcceptThreads) {
    if (!thread.is_finished())
      return false;
  }
  for (auto const& world : m_worlds) {
    if (!world.second)
      continue;
    if (std::holds_alternative<WorkerPoolPromise<std::shared_ptr<WorldServerThread>>>(world.second->currentPromise)) {
      auto const& promise = std::get<WorkerPoolPromise<std::shared_ptr<WorldServerThread>>>(world.second->currentPromise);
      if (!promise.done())
        return false;
      auto server = promise.get();
      if (!server)
        EmergencyLog::error("UniverseServer: completed world producer failed: {}", server.error().what());
      else if (server->get() && !server->get()->is_joined())
        return false;
    }
  }
  for (auto const& world : m_retiringWorlds) {
    if (!world->is_joined())
      return false;
  }
  for (auto const& system : m_systemWorlds) {
    if (!system.second->is_joined())
      return false;
  }
  return true;
}

std::expected<void, StarException> UniverseServer::shutdown() {
  m_stopSource.request_stop();
  std::optional<StarException> error;
  auto record = [&](auto&& result) {
    if (!result) {
      if (error)
        *error = error->with_related(result.error(), "Additional universe shutdown failure");
      else
        error = std::move(result.error());
    }
  };
  auto rejectMessages = [&]() {
    std::unique_lock messageLocker(m_messageLock, std::defer_lock);
    lock_with_diagnostics(messageLocker);
    auto messages = std::exchange(m_universeMessages, {});
    auto responses = std::exchange(m_universeMessageResponses, {});
    auto promises = std::exchange(m_universeMessagePromises, {});
    messageLocker.unlock();
    auto rejectRemote = [&](ConnectionId clientId, Uuid const& uuid) {
      if (m_connectionServer && m_connectionServer->hasConnection(clientId))
        record(m_connectionServer->sendPackets(
          clientId, {std::make_shared<UniverseMessageResponse>(std::unexpected(String("ServerShutdown")), uuid)}));
    };
    for (auto& message : messages) {
      if (std::holds_alternative<RpcPromiseKeeper<Json>>(message.keeper))
        record(std::get<RpcPromiseKeeper<Json>>(message.keeper).fail("ServerShutdown"));
      else {
        auto const& remote = std::get<std::pair<Uuid, ConnectionId>>(message.keeper);
        rejectRemote(remote.second, remote.first);
      }
    }
    for (auto&& response : responses) {
      if (std::holds_alternative<RpcPromiseKeeper<Json>>(response.second.second))
        record(std::get<RpcPromiseKeeper<Json>>(response.second.second).fail("ServerShutdown"));
      else if (std::holds_alternative<ConnectionId>(response.second.second))
        rejectRemote(std::get<ConnectionId>(response.second.second), response.first);
    }
    for (auto const& promise : promises)
      rejectRemote(promise.second.first, promise.first);
  };
  if (m_initialized)
    record(m_root.logger().info("UniverseServer: Stopping UniverseServer"));
  record(m_workerPool.stop());
  bool producersJoined = m_workerPool.worker_count() == 0;
  bool tcpJoined = true;
  if (m_tcpServer) {
    record(m_root.logger().info("UniverseServer: Stopping TCP Server"));
    record(m_tcpServer->stop());
    tcpJoined = m_tcpServer->is_closed();
    if (tcpJoined)
      m_tcpServer.reset();
  }
  bool acceptsJoined = true;
  {
    std::unique_lock acceptThreadsLocker(m_connectionAcceptThreadsMutex, std::defer_lock);
    lock_with_diagnostics(acceptThreadsLocker);
    for (auto& thread : m_connectionAcceptThreads) {
      record(thread.finish());
      acceptsJoined &= thread.is_finished();
    }
    if (acceptsJoined)
      m_connectionAcceptThreads.clear();
  }
  bool connectionsJoined = true;
  if (m_connectionServer) {
    if (producersJoined && tcpJoined && acceptsJoined) {
      rejectMessages();
      for (auto clientId : clientIds())
        record(doDisconnection(clientId, "ServerShutdown"));
    }
    record(m_connectionServer->close());
    connectionsJoined = m_connectionServer->isClosed();
  }
  if (connectionsJoined)
    rejectMessages();
  if (!producersJoined || !tcpJoined || !acceptsJoined || !connectionsJoined) {
    if (!error)
      error = UniverseServerException("Universe shutdown still owns unjoined connection or producer threads");
    return std::unexpected(std::move(*error));
  }
  if (m_initialized) {
    record(saveSettings());
    record(saveTempWorldIndex());
  }
  bool worldsJoined = true;
  for (auto& world : m_worlds) {
    if (!world.second)
      continue;
    auto done = world.second->done();
    if (!done) {
      record(done);
      continue;
    }
    if (!*done)
      continue;
    auto server = world.second->get();
    if (!server) {
      record(server);
    } else if (*server) {
      record((*server)->close());
      worldsJoined &= (*server)->is_joined();
    }
  }
  for (auto const& world : m_retiringWorlds) {
    record(world->close());
    worldsJoined &= world->is_joined();
  }
  for (auto const& system : m_systemWorlds) {
    system.second->stop();
    record(system.second->join());
    bool joined = system.second->is_joined();
    worldsJoined &= joined;
    if (joined)
      record(system.second->store());
  }
  if (!worldsJoined) {
    if (!error)
      error = UniverseServerException("Universe shutdown still owns unjoined world threads");
    return std::unexpected(std::move(*error));
  }
  m_commandProcessor.reset();
  m_chat_processor.clear_command_handler();
  record(stopLua());
  {
    std::unique_lock locker(m_mainLock, std::defer_lock);
    lock_with_diagnostics(locker);
    auto messages = std::exchange(m_pendingWorldMessages, {});
    locker.unlock();
    for (auto& worldMessages : messages) {
      for (auto& message : worldMessages.second)
        record(message.promise.cancel("ServerShutdown"));
    }
  }
  if (m_celestial_database)
    record(m_celestial_database->close());
  if (error)
    return std::unexpected(std::move(*error));
  m_initialized = false;
  return {};
}

UniverseServer::WorldServerPromise::WorldServerPromise(std::function<WorkerPoolPromise<std::shared_ptr<WorldServerThread>>(WorldChunks)> producer, RpcPromise<WorldChunks> promise)
  : currentPromise(promise), producer(producer) {}
  
UniverseServer::WorldServerPromise::WorldServerPromise(WorkerPoolPromise<std::shared_ptr<WorldServerThread>> promise)
  : currentPromise(promise) {}
  
std::expected<bool, StarException> UniverseServer::WorldServerPromise::done() const {
  if (std::holds_alternative<WorkerPoolPromise<std::shared_ptr<WorldServerThread>>>(currentPromise)) {
    return std::get<WorkerPoolPromise<std::shared_ptr<WorldServerThread>>>(currentPromise).done();
  } else if (std::holds_alternative<RpcPromise<WorldChunks>>(currentPromise)) {
    return std::get<RpcPromise<WorldChunks>>(currentPromise).finished();
  }
  return false;
}
std::expected<bool, StarException> UniverseServer::WorldServerPromise::poll() {
  if (std::holds_alternative<WorkerPoolPromise<std::shared_ptr<WorldServerThread>>>(currentPromise))
    return std::get<WorkerPoolPromise<std::shared_ptr<WorldServerThread>>>(currentPromise).poll();
  auto& rpc = std::get<RpcPromise<WorldChunks>>(currentPromise);
  auto result = rpc.result();
  if (!result)
    return std::unexpected(std::move(result.error()));
  if (*result) {
    currentPromise = (*producer)(std::move(**result));
    producer.reset();
    return false;
  }
  auto error = rpc.error();
  if (!error)
    return std::unexpected(std::move(error.error()));
  if (*error)
    return std::unexpected(UniverseServerException(Star::format("UniverseServer: World server promise failed! {}", **error)));
  return false;
}

std::expected<std::shared_ptr<WorldServerThread>, StarException> UniverseServer::WorldServerPromise::get() {
  if (std::holds_alternative<WorkerPoolPromise<std::shared_ptr<WorldServerThread>>>(currentPromise)) {
    auto result = std::get<WorkerPoolPromise<std::shared_ptr<WorldServerThread>>>(currentPromise).get();
    if (!result)
      return std::unexpected(std::move(result.error()));
    return result->get();
  }
  return nullptr;
}

std::expected<void, StarException> UniverseServer::processUniverseFlags() {
  std::unique_lock locker(m_mainLock, std::defer_lock);
  lock_with_diagnostics(locker);
  std::shared_lock clientsLocker(m_clientsLock);
  if (auto actions = m_universeSettings->pullPendingFlagActions()) {
    for (auto const& action : *actions) {
      if (std::holds_alternative<PlaceDungeonFlagAction>(action)) {
        auto const& dungeon = std::get<PlaceDungeonFlagAction>(action);
        auto active = instanceWorldStoredOrActive(dungeon.targetInstance);
        if (!active)
          return std::unexpected(std::move(active.error()));
        if (*active)
          m_pendingFlagActions.push_back({InstanceWorldId(dungeon.targetInstance), action});
      }
    }
  }
  for (auto it = m_pendingFlagActions.begin(); it != m_pendingFlagActions.end();) {
    bool active = false;
    if (std::holds_alternative<InstanceWorldId>(it->first)) {
      auto result = instanceWorldStoredOrActive(std::get<InstanceWorldId>(it->first));
      if (!result)
        return std::unexpected(std::move(result.error()));
      active = *result;
    }
    if (!active) {
      it = m_pendingFlagActions.erase(it);
      continue;
    }
    auto created = triggerWorldCreation(it->first);
    if (!created)
      return std::unexpected(std::move(created.error()));
    if (!*created || !**created) {
      ++it;
      continue;
    }
    if (std::holds_alternative<PlaceDungeonFlagAction>(it->second)) {
      auto const& dungeon = std::get<PlaceDungeonFlagAction>(it->second);
      locker.unlock();
      auto placed = (**created)->executeAction([&](WorldServerThread&, WorldServer& world) -> std::expected<void, StarException> {
        auto result = world.placeDungeon(dungeon.dungeonId, dungeon.targetPosition, 0);
        if (!result)
          return std::unexpected(std::move(result.error()));
        return {};
      });
      lock_with_diagnostics(locker);
      if (!placed)
        return std::unexpected(std::move(placed.error()));
    }
    it = m_pendingFlagActions.erase(it);
  }
  return {};
}

std::expected<void, StarException> UniverseServer::send_pending_chat() {
  std::unique_lock locker(m_mainLock, std::defer_lock);
  lock_with_diagnostics(locker);
  std::shared_lock clients_locker(m_clientsLock);
  for (auto const& client : m_clients) {
    if (m_disconnectingClients.contains(client.first))
      continue;
    auto messages = m_chat_processor.pull_pending_messages(client.first);
    if (!messages)
      return std::unexpected(std::move(messages.error()));
    if (!messages->empty()) {
      List<std::shared_ptr<Packet>> packets;
      packets.reserve(messages->size());
      for (auto& message : *messages)
        packets.push_back(std::make_shared<ChatReceivePacket>(std::move(message)));
      if (auto sent = m_connectionServer->sendPackets(client.first, std::move(packets)); !sent)
        return std::unexpected(std::move(sent.error()));
    }
  }
  return {};
}

std::expected<void, StarException> UniverseServer::updateTeams() {
  std::unique_lock locker(m_mainLock, std::defer_lock);
  lock_with_diagnostics(locker);
  std::shared_lock clientsLocker(m_clientsLock);

  StringMap<List<Uuid>> connectedPlayers;
  auto teams = m_teamManager->getPvpTeams();
  for (auto const& p : m_clients) {
    if (m_disconnectingClients.contains(p.first))
      continue;
    connectedPlayers[p.second->player_name()].push_back(p.second->player_uuid());

    if (p.second->team().type == TeamType::PVP)
      p.second->set_team(EntityDamageTeam(TeamType::PVP, teams.value(p.second->player_uuid(), soloPvpTeam(p.second->client_id()))));
    else
      p.second->set_team(EntityDamageTeam(TeamType::Friendly));

    auto channels = m_chat_processor.client_channels(p.first);
    auto team = m_teamManager->getTeam(p.second->player_uuid());
    auto worldName = Star::print_world_id(p.second->player_world_id());
    if (!worldName)
      return std::unexpected(std::move(worldName.error()));
    for (auto const& channel : channels) {
      if (channel != *worldName && (!team || channel != team->hex()))
        m_chat_processor.leave_channel(p.first, channel);
    }
    if (team) {
      auto team_channel = team->hex();
      auto subscribed = channels.contains(team_channel);
      if (!subscribed)
        return std::unexpected(std::move(subscribed.error()));
      if (!*subscribed)
        m_chat_processor.join_channel(p.first, team_channel);
    }
  }

  m_teamManager->setConnectedPlayers(connectedPlayers);
  return {};
}

std::expected<void, StarException> UniverseServer::update_ships() {
  using ShipWork = std::pair<std::shared_ptr<ServerClientContext>, std::shared_ptr<WorldServerThread>>;
  SmallVector<ShipWork, 8> clients;
  {
    std::unique_lock locker(m_mainLock, std::defer_lock);
    lock_with_diagnostics(locker);
    std::shared_lock clients_lock(m_clientsLock);
    clients.reserve(m_clients.size());
    for (auto const& [id, client] : m_clients) {
      if (m_disconnectingClients.contains(id))
        continue;
      auto ship = getWorld(ClientShipWorldId(client->player_uuid()));
      if (!ship)
        return std::unexpected(std::move(ship.error()));
      clients.emplace_back(client, std::move(*ship));
    }
  }
  for (auto const& [client, ship] : clients) {
    auto client_id = client->client_id();
    if (ship) {
      auto upgrades = client->ship_upgrades();
      auto species = client->ship_species();
      std::optional<WorldChunks> updated_chunks;
      auto action = ship->executeAction([&](WorldServerThread&, WorldServer& world) -> std::expected<void, StarException> {
        auto species_json = world.getProperty("ship.species");
        if (!species_json)
          return std::unexpected(std::move(species_json.error()));
        if (species_json->is_type(Json::Type::String)) {
          auto parsed = species_json->to_string();
          if (!parsed)
            return std::unexpected(std::move(parsed.error()));
          species = std::move(*parsed);
        } else {
          if (auto set = world.setProperty("ship.species", species); !set)
            return std::unexpected(std::move(set.error()));
        }
        if (!m_speciesShips.contains(species))
          return m_root.logger().warn("UniverseServer: Client {} has unknown ship species '{}', skipping ship update", client_id, species);
        auto definitions = m_speciesShips.get(species);
        if (!definitions)
          return std::unexpected(std::move(definitions.error()));
        auto const& ships = definitions->get();
        auto level = world.getProperty("ship.level");
        if (!level)
          return std::unexpected(std::move(level.error()));
        unsigned new_level = ships.empty() ? 0u : std::min<unsigned>(ships.size() - 1, upgrades.ship_level);
        if (level->is_type(Json::Type::Int)) {
          auto old_level = level->to_uint();
          if (!old_level)
            return std::unexpected(std::move(old_level.error()));
          if (*old_level < new_level) {
            for (unsigned i = *old_level + 1; i <= new_level; ++i) {
              auto structure = WorldStructure::create(m_root, ships[i]);
              if (!structure)
                return std::unexpected(std::move(structure.error()));
              if (auto set = world.setCentralStructure(*structure); !set)
                return std::unexpected(std::move(set.error()));
              auto config = structure->configValue("shipUpgrades");
              if (!config)
                return std::unexpected(std::move(config.error()));
              if (auto applied = upgrades.apply(*config); !applied)
                return std::unexpected(std::move(applied.error()));
            }
            auto chunks = world.readChunks();
            if (!chunks)
              return std::unexpected(std::move(chunks.error()));
            updated_chunks.emplace(std::move(*chunks));
          }
        }
        if (auto set = world.setProperty("ship.level", upgrades.ship_level); !set)
          return std::unexpected(std::move(set.error()));
        if (auto set = world.setProperty("ship.maxFuel", upgrades.max_fuel); !set)
          return std::unexpected(std::move(set.error()));
        if (auto set = world.setProperty("ship.crewSize", upgrades.crew_size); !set)
          return std::unexpected(std::move(set.error()));
        if (auto set = world.setProperty("ship.fuelEfficiency", upgrades.fuel_efficiency); !set)
          return std::unexpected(std::move(set.error()));
        return {};
      });
      if (!action)
        return std::unexpected(std::move(action.error()));
      client->set_ship_species(std::move(species));
      if (updated_chunks) {
        client->set_ship_upgrades(std::move(upgrades));
        client->update_ship_chunks(std::move(*updated_chunks));
      }
    }
    if (auto system = client->system_world()) {
      auto action = system->executeClientShipAction(client_id, [speed = client->ship_speed()](std::optional<std::reference_wrapper<SystemClientShip>> ship) -> std::expected<void, StarException> {
        if (ship)
          ship->get().setSpeed(speed);
        return {};
      });
      if (!action)
        return std::unexpected(std::move(action.error()));
    }
  }
  return {};
}

std::expected<void, StarException> UniverseServer::sendClockUpdates() {
  std::unique_lock locker(m_mainLock, std::defer_lock);
  lock_with_diagnostics(locker);
  std::shared_lock clientsLocker(m_clientsLock);
  auto assets = m_root.assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  auto json = (*assets)->json("/universe_server.config:clockUpdatePacketInterval");
  if (!json)
    return std::unexpected(std::move(json.error()));
  auto interval = json->to_int();
  if (!interval)
    return std::unexpected(std::move(interval.error()));
  auto now = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() + std::chrono::microseconds(500));
  if (now > m_lastClockUpdateSent + std::chrono::milliseconds(*interval)) {
    auto packet = std::make_shared<UniverseTimeUpdatePacket>(m_universeClock->time());
    for (auto clientId : m_clients | std::views::keys) {
      if (m_disconnectingClients.contains(clientId))
        continue;
      if (auto sent = m_connectionServer->sendPackets(clientId, {packet}); !sent)
        return std::unexpected(std::move(sent.error()));
    }
    m_lastClockUpdateSent = now;
  }
  return {};
}

std::expected<void, StarException> UniverseServer::send_client_context_update(ServerClientContext& client) {
  return client.send_update([&](std::shared_ptr<ClientContextUpdatePacket> const& packet) {
    return m_connectionServer->enqueue_packet(client.client_id(), packet);
  });
}

std::expected<void, StarException> UniverseServer::send_client_context_updates() {
  std::unique_lock locker(m_mainLock, std::defer_lock);
  lock_with_diagnostics(locker);
  std::shared_lock clients_lock(m_clientsLock);

  auto& log_map = m_root.logMap();
  std::shared_ptr<LogMapUpdate> debug_update;
  // A failed context write or debug send must leave the batch available for retry.
  auto restore = make_scope_exit([&] {
    if (!m_isLocal && debug_update)
      log_map.restore_missing(debug_update->map);
  });
  for (auto const& [id, client] : m_clients) {
    if (m_disconnectingClients.contains(id))
      continue;
    if (client->server_debug() && client->net_rules().version() >= 17) {
      // The send paths serialize const packets or forward them to the local reader.
      if (!debug_update) {
        debug_update = std::make_shared<LogMapUpdate>();
        debug_update->map = m_isLocal ? log_map.snapshot() : log_map.drain();
      }
      if (auto sent = m_connectionServer->enqueue_packet(id, debug_update); !sent)
        return std::unexpected(std::move(sent.error()));
    }
    if (auto sent = send_client_context_update(*client); !sent)
      return sent;
  }
  restore.release();
  if (!m_isLocal && !debug_update)
    log_map.clear();
  return {};
}

std::expected<void, StarException> UniverseServer::kickErroredPlayers() {
  std::unique_lock locker(m_mainLock, std::defer_lock);
  lock_with_diagnostics(locker);
  for (auto const& worldId : m_worlds.keys()) {
    auto world = getWorld(worldId);
    if (!world)
      return std::unexpected(std::move(world.error()));
    if (*world) {
      locker.unlock();
      auto clients = (*world)->erroredClients();
      lock_with_diagnostics(locker);
      for (auto clientId : clients)
        m_pendingDisconnections[clientId] = "Incoming client packet has caused exception";
    }
  }
  return {};
}

std::expected<void, StarException> UniverseServer::reapConnections() {
  auto start = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() + std::chrono::microseconds(500));
  auto assets = m_root.assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  auto json = (*assets)->json("/universe_server.config:connectionTimeout");
  if (!json)
    return std::unexpected(std::move(json.error()));
  auto timeout = json->to_int();
  if (!timeout)
    return std::unexpected(std::move(timeout.error()));
  {
    std::unique_lock acceptThreadsLocker(m_connectionAcceptThreadsMutex, std::defer_lock);
    lock_with_diagnostics(acceptThreadsLocker);
    for (auto it = m_connectionAcceptThreads.begin(); it != m_connectionAcceptThreads.end();) {
      if (!it->is_running()) {
        if (auto finished = it->finish(); !finished) {
          if (auto logged = m_root.logger().error_exception("UniverseServer: Exception caught accepting new connection",
                                                            ExceptionDiagnostic(finished.error()));
              !logged)
            return std::unexpected(std::move(logged.error()));
        }
      }
      if (it->is_finished())
        it = m_connectionAcceptThreads.erase(it);
      else
        ++it;
    }
  }
  std::unique_lock locker(m_mainLock, std::defer_lock);
  lock_with_diagnostics(locker);
  auto pending = std::exchange(m_pendingDisconnections, {});
  locker.unlock();
  for (auto const& connection : pending) {
    if (auto disconnected = doDisconnection(connection.first, connection.second); !disconnected)
      return std::unexpected(std::move(disconnected.error()));
  }
  std::shared_lock clientsLocker(m_clientsLock);
  for (auto clientId : m_clients.keys()) {
    auto client = m_clients.value(clientId);
    if (!client) // Another disconnect can complete while the clients lock is released below.
      continue;
    std::expected<bool, StarException> open = false;
    if (m_connectionServer->hasConnection(clientId))
      open = m_connectionServer->connectionIsOpen(clientId);
    if (!open)
      return std::unexpected(std::move(open.error()));
    String reason;
    if (!*open) {
      if (auto logged = m_root.logger().info("UniverseServer: Client {} connection lost", client->descriptive_name()); !logged)
        return std::unexpected(std::move(logged.error()));
      reason = "Disconnected due to connection lost";
    } else if (client->remote_address()) {
      auto activity = m_connectionServer->lastActivityTime(clientId);
      if (!activity)
        return std::unexpected(std::move(activity.error()));
      if (start - *activity > std::chrono::milliseconds(*timeout)) {
        if (auto logged = m_root.logger().info("UniverseServer: Kicking client {} due to inactivity", client->descriptive_name()); !logged)
          return std::unexpected(std::move(logged.error()));
        reason = "Disconnected due to inactivity";
      }
    }
    if (!reason.empty()) {
      clientsLocker.unlock();
      auto disconnected = doDisconnection(clientId, reason);
      clientsLocker.lock();
      if (!disconnected)
        return std::unexpected(std::move(disconnected.error()));
    }
  }
  lock_with_diagnostics(locker);
  auto previous = m_deadConnections.size();
  std::optional<StarException> error;
  m_deadConnections.filter([&](auto& connection) {
    if (error)
      return true;
    auto sent = connection.first.send();
    if (!sent) {
      error = std::move(sent.error());
      return true;
    }
    if (*sent)
      connection.second = start;
    return connection.first.isOpen() && start - connection.second < std::chrono::milliseconds(*timeout);
  });
  if (error)
    return std::unexpected(std::move(*error));
  if (previous > m_deadConnections.size())
    return m_root.logger().info("UniverseServer: Reaped {} dead connections", previous);
  return {};
}

std::expected<void, StarException> UniverseServer::processPlanetTypeChanges() {
  std::unique_lock locker(m_mainLock, std::defer_lock);
  lock_with_diagnostics(locker);
  for (auto const& worldId : m_worlds.keys()) {
    if (std::holds_alternative<CelestialWorldId>(worldId)) {
      auto world = getWorld(worldId);
      if (!world)
        return std::unexpected(std::move(world.error()));
      if (*world) {
        locker.unlock();
        auto type = (*world)->pullNewPlanetType();
        lock_with_diagnostics(locker);
        if (!type)
          return std::unexpected(std::move(type.error()));
        if (*type) {
          if (auto updated = updatePlanetType(std::get<CelestialWorldId>(worldId), (*type)->first, (*type)->second); !updated)
            return std::unexpected(std::move(updated.error()));
        }
      }
    }
  }
  return {};
}

std::expected<void, StarException> UniverseServer::warpPlayers() {
  std::unique_lock locker(m_mainLock, std::defer_lock);
  lock_with_diagnostics(locker);
  std::shared_lock clientsLocker(m_clientsLock);

  for (auto const& clientId : m_pendingPlayerWarps.keys()) {
    auto warpEntry = m_pendingPlayerWarps.get(clientId);
    if (!warpEntry)
      return std::unexpected(std::move(warpEntry.error()));
    auto& warp = warpEntry->get();
    WarpAction warpAction = warp.first;
    bool deploy = warp.second;

    auto client_context = m_clients.value(clientId);
    if (!client_context)
      continue;

    if (std::holds_alternative<WarpToPlayer>(warpAction)) {
      auto const& toPlayerUuid = std::get<WarpToPlayer>(warpAction);
      bool authorized = client_context->is_admin() || !m_secureWarps;
      if (!authorized) {
        auto requesterTeam = m_teamManager->getTeam(client_context->player_uuid());
        auto targetTeam = m_teamManager->getTeam(toPlayerUuid);
        if (requesterTeam && targetTeam && *requesterTeam == *targetTeam)
          authorized = true;
      }
      if (!authorized) {
        if (auto logged = m_root.logger().warn("UniverseServer: Denied WarpToPlayer from client {} ({}) to UUID {}",
            clientId, client_context->descriptive_name(), toPlayerUuid.hex()); !logged)
          return std::unexpected(std::move(logged.error()));
        if (auto sent = m_connectionServer->sendPackets(clientId, {std::make_shared<PlayerWarpResultPacket>(false, warpAction, true)}); !sent)
          return std::unexpected(std::move(sent.error()));
        m_pendingPlayerWarps.remove(clientId);
        continue;
      }
    }

    auto resolved = resolveWarpAction(warpAction, clientId, deploy);
    if (!resolved)
      return std::unexpected(std::move(resolved.error()));
    auto warpToWorld = std::move(*resolved);
    auto created = triggerWorldCreation(warpToWorld.world);
    if (!created)
      return std::unexpected(std::move(created.error()));
    if (*created) {
      auto warpName = Star::print_warp_action(warpToWorld);
      if (!warpName)
        return std::unexpected(std::move(warpName.error()));
      if (auto logged = m_root.logger().info("UniverseServer: Warping player {} to {}", clientId, *warpName); !logged)
        return std::unexpected(std::move(logged.error()));
      if (auto toWorld = **created) {
        locker.unlock();
        auto valid = toWorld->spawnTargetValid(warpToWorld.target);
        if (!valid)
          return std::unexpected(std::move(valid.error()));
        if (*valid) {
          if (auto currentWorld = client_context->player_world()) {
            auto revive = currentWorld->playerRevivePosition(clientId);
            if (!revive)
              return std::unexpected(std::move(revive.error()));
            if (*revive)
              client_context->set_player_return_warp(WarpToWorld{currentWorld->worldId(), SpawnTargetPosition(**revive)});
            auto packets = currentWorld->removeClient(clientId);
            if (!packets) {
              auto finalPackets = currentWorld->pullOutgoingPackets(clientId);
              if (!finalPackets.empty()) {
                if (auto sent = m_connectionServer->sendPackets(clientId, std::move(finalPackets)); !sent)
                  return std::unexpected(packets.error().with_related(sent.error(), "Sending final world packets failed"));
              }
              if (!currentWorld->hasClient(clientId))
                client_context->clear_player_world();
              return std::unexpected(std::move(packets.error()));
            }
            client_context->clear_player_world();
            if (auto sent = m_connectionServer->sendPackets(clientId, std::move(*packets)); !sent)
              return std::unexpected(std::move(sent.error()));
            auto name = Star::print_world_id(currentWorld->worldId());
            if (!name)
              return std::unexpected(std::move(name.error()));
            m_chat_processor.leave_channel(clientId, *name);
          }
          client_context->set_orbit_warp_action({});

          // having stale world ids in the client context is bad,
          // make sure it's at least null until the next client context update
          if (auto sent = send_client_context_update(*client_context); !sent)
            return std::unexpected(std::move(sent.error()));

          // Checking the spawn target validity then adding the client is not
          // perfect, it can still become invalid in between, if we fail at
          // adding the client we need to warp them back.
          auto clientAdded = toWorld->addClient(clientId, warpToWorld.target, !client_context->remote_address(), client_context->can_become_admin(), client_context->net_rules());

          lock_with_diagnostics(locker);
          if (!clientAdded)
            return std::unexpected(std::move(clientAdded.error()));
          if (*clientAdded) {
            client_context->set_player_world(toWorld);
            auto name = Star::print_world_id(warpToWorld.world);
            if (!name)
              return std::unexpected(std::move(name.error()));
            m_chat_processor.join_channel(clientId, *name);

            if (std::holds_alternative<ClientShipWorldId>(warpToWorld.world)) {
              if (auto clientId = getClientForUuid(std::get<ClientShipWorldId>(warpToWorld.world))) {
                auto owner = m_clients.get(*clientId);
                if (!owner)
                  return std::unexpected(std::move(owner.error()));
                if (auto systemWorld = owner->get()->system_world())
                  client_context->set_orbit_warp_action(systemWorld->clientWarpAction(*clientId));
              }
            }
          } else if (auto returnWarp = client_context->player_return_warp()) {
            auto name = Star::print_warp_action(returnWarp);
            if (!name)
              return std::unexpected(std::move(name.error()));
            if (auto logged = m_root.logger().info("UniverseServer: Warping player {} failed, returning to '{}'", clientId, *name); !logged)
              return std::unexpected(std::move(logged.error()));
            m_pendingPlayerWarps[clientId] = {returnWarp, false};
          } else {
            if (auto logged = m_root.logger().info("UniverseServer: Warping player {} failed, returning to ship", clientId); !logged)
              return std::unexpected(std::move(logged.error()));
            m_pendingPlayerWarps[clientId] = {WarpAlias::OwnShip, false};
          }
          if (auto sent = m_connectionServer->sendPackets(clientId, {std::make_shared<PlayerWarpResultPacket>(true, warpAction, false)}); !sent)
            return std::unexpected(std::move(sent.error()));
          m_pendingPlayerWarps.remove(clientId);
        } else {
          if (auto logged = m_root.logger().info("UniverseServer: Warping player {} failed, invalid spawn target '{}'", clientId, Star::print_spawn_target(warpToWorld.target)); !logged)
            return std::unexpected(std::move(logged.error()));
          lock_with_diagnostics(locker);
          if (auto sent = m_connectionServer->sendPackets(clientId, {std::make_shared<PlayerWarpResultPacket>(false, warpAction, true)}); !sent)
            return std::unexpected(std::move(sent.error()));
          m_pendingPlayerWarps.remove(clientId);
        }
      } else {
        auto name = Star::print_world_id(warpToWorld.world);
        if (!name)
          return std::unexpected(std::move(name.error()));
        if (auto logged = m_root.logger().info("UniverseServer: Warping player {} failed, invalid world '{}' or world failed to load", clientId, *name); !logged)
          return std::unexpected(std::move(logged.error()));
        if (auto sent = m_connectionServer->sendPackets(clientId, {std::make_shared<PlayerWarpResultPacket>(false, warpAction, false)}); !sent)
          return std::unexpected(std::move(sent.error()));
        m_pendingPlayerWarps.remove(clientId);
      }
    } else {
      // If the world is not created yet, just wait for it to create.
      //m_pendingPlayerWarps[clientId] = {warpAction, deploy}; // Bott: ...there's no point to setting it again like this since it's already set. Nothing above removes it by this point.
    }
  }
  
  for (auto const& clientId : m_pendingSubWorlds.keys()) {
    auto pending = m_pendingSubWorlds.get(clientId);
    if (!pending)
      return std::unexpected(std::move(pending.error()));
    auto& pendingSubWorlds = pending->get();
    auto client_context = m_clients.value(clientId);
    if (!client_context)
      continue;

    for (auto const& swid : pendingSubWorlds.keys()) {
      auto worldEntry = pendingSubWorlds.get(swid);
      if (!worldEntry)
        return std::unexpected(std::move(worldEntry.error()));
      auto const& worldId = worldEntry->get();
      auto created = triggerWorldCreation(worldId);
      if (!created)
        return std::unexpected(std::move(created.error()));
      if (*created) {
        auto name = Star::print_world_id(worldId);
        if (!name)
          return std::unexpected(std::move(name.error()));
        if (auto logged = m_root.logger().info("UniverseServer: Sending player {} subworld {} to {}", clientId, swid, *name); !logged)
          return std::unexpected(std::move(logged.error()));
        if (auto toWorld = **created) {
          locker.unlock();
          auto clientAdded = toWorld->addClient(main_to_sub_world_connection_id(clientId), SpawnTarget(), !client_context->remote_address(), client_context->can_become_admin(), client_context->net_rules(), swid);
          lock_with_diagnostics(locker);
          if (!clientAdded)
            return std::unexpected(std::move(clientAdded.error()));
          if (*clientAdded) {
            client_context->set_sub_world(swid,toWorld);
          } else {
            if (auto logged = m_root.logger().info("UniverseServer: Player {} subworld {} failed", clientId, swid); !logged)
              return std::unexpected(std::move(logged.error()));
            if (auto sent = m_connectionServer->sendPackets(clientId, {std::make_shared<ClientSubWorldReject>(swid)}); !sent)
              return std::unexpected(std::move(sent.error()));
          }
        } else {
          if (auto logged = m_root.logger().info("UniverseServer: Player {} subworld {} failed, invalid world '{}' or world failed to load", clientId, swid, *name); !logged)
            return std::unexpected(std::move(logged.error()));
          if (auto sent = m_connectionServer->sendPackets(clientId, {std::make_shared<ClientSubWorldReject>(swid)}); !sent)
            return std::unexpected(std::move(sent.error()));
        }
        pendingSubWorlds.remove(swid);
      } else {
        // If the world is not created yet, wait for it to create.
      }
    }
    if (pendingSubWorlds.empty()) {
      m_pendingSubWorlds.remove(clientId);
    }
  }
  return {};
}

std::expected<void, StarException> UniverseServer::flyShips() {
  std::unique_lock locker(m_mainLock, std::defer_lock);
  lock_with_diagnostics(locker);
  std::shared_lock clientsLocker(m_clientsLock);

  auto assets = m_root.assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  auto waitJson = (*assets)->json("/universe_server.config:queuedFlightWaitTime");
  if (!waitJson)
    return std::unexpected(std::move(waitJson.error()));
  auto wait = waitJson->to_double();
  if (!wait)
    return std::unexpected(std::move(wait.error()));
  double queuedFlightWaitTime = *wait;
  for (auto clientId : m_queuedFlights.keys()) {
    if (!m_pendingFlights.contains(clientId) && !m_pendingArrivals.contains(clientId)) {
      auto flightEntry = m_queuedFlights.get(clientId);
      if (!flightEntry)
        return std::unexpected(std::move(flightEntry.error()));
      auto& flight = flightEntry->get();
      if (!flight.second.has_value())
        flight.second = m_universeClock->time() + queuedFlightWaitTime;
      else if (m_universeClock->time() > *flight.second)
        m_pendingFlights.set(clientId, flight.first);

      if (m_pendingFlights.contains(clientId))
        m_queuedFlights.remove(clientId);
    }
  }

  for (auto it = m_pendingFlights.begin(); it != m_pendingFlights.end();) {
    auto process = [&]() -> std::expected<bool, StarException> {
    auto const& p = *it;
    ConnectionId clientId = p.first;
    Vec3I system = std::get<0>(p.second);
    SystemLocation location = std::get<1>(p.second);
    Json settings = std::get<2>(p.second);

    auto client_context = m_clients.value(clientId);
    if (!client_context)
      return true;

    auto clientSystem = client_context->system_world();
    if (!clientSystem)
      system = Vec3I();

    if (system != Vec3I() && client_context->ship_coordinate().location() == system && client_context->ship_location() == location)
      return true;

    // if the ship is flying to another system do nothing
    // if the ship is flying within the target system, just update the ship destination
    if (m_pendingArrivals.contains(clientId)) {
      return true;
    }

    auto maybeClientShip = triggerWorldCreation(ClientShipWorldId(client_context->player_uuid()));
    if (!maybeClientShip)
      return std::unexpected(std::move(maybeClientShip.error()));
    if (!*maybeClientShip)
      return false;// ship is not loaded yet
    auto clientShip = **maybeClientShip;
    if (!clientShip)
      return true;// ship is broken

    CelestialCoordinate destination = std::holds_alternative<CelestialCoordinate>(location) ? std::get<CelestialCoordinate>(location) : CelestialCoordinate(system);
    bool interstellar = clientSystem ? client_context->ship_coordinate().location() != system : true;
    if (!interstellar) {
      // don't fly to null locations in the same system
      if (std::holds_alternative<std::monostate>(location))
        return true;

      clientSystem->setClientDestination(clientId, location);
    } else if (system != Vec3I()) {
      // changing systems
      if (auto removed = clientSystem->removeClient(clientId); !removed)
        return std::unexpected(std::move(removed.error()));
      client_context->set_system_world({});

      if (!std::holds_alternative<std::monostate>(location))
        m_queuedFlights.set(clientId, {std::make_tuple(system, location, settings), {}});

      destination = CelestialCoordinate(system);
    }

    auto logged = destination.is_null()
        ? m_root.logger().info("Flying ship for player {} to new starter world", clientId)
        : m_root.logger().info("Flying ship for player {} to {}", clientId, destination);
    if (!logged)
      return std::unexpected(std::move(logged.error()));

    bool startInWarp = system == Vec3I();
    locker.unlock();
    auto action = clientShip->executeAction([&](WorldServerThread&, WorldServer& worldServer) {
      return worldServer.startFlyingSky(interstellar, startInWarp, settings);
    });
    auto clients = clientShip->clients();
    lock_with_diagnostics(locker);
    if (!action)
      return std::unexpected(std::move(action.error()));

    client_context->set_ship_coordinate(CelestialCoordinate(system));
    client_context->set_orbit_warp_action({});
    for (auto clientId : clients) {
      if (auto client_context = m_clients.value(clientId))
        client_context->set_orbit_warp_action({});
    }

    m_pendingArrivals.set(clientId, destination);

    return true;
    }();
    if (!process)
      return std::unexpected(std::move(process.error()));
    if (*process)
      it = m_pendingFlights.erase(it);
    else
      ++it;
  }
  return {};
}

std::expected<void, StarException> UniverseServer::arriveShips() {
  std::unique_lock locker(m_mainLock, std::defer_lock);
  lock_with_diagnostics(locker);
  std::shared_lock clientsLocker(m_clientsLock);

  for (auto it = m_pendingArrivals.begin(); it != m_pendingArrivals.end();) {
    auto process = [&]() -> std::expected<bool, StarException> {
      auto&& p = *it;
      auto& clientId = p.first;
      auto& coordinate = p.second;

      if (!coordinate) {
        auto starter = nextStarterWorld();
        if (!starter)
          return std::unexpected(std::move(starter.error()));
        coordinate = starter->value_or(CelestialCoordinate{});
      }

      if (!coordinate)
        return false;

      auto client_context = m_clients.value(clientId);
      if (!client_context)
        return true;

      auto clientSystem = client_context->system_world();
      if (!clientSystem) {
        auto created = createSystemWorld(coordinate.location());
        if (!created)
          return std::unexpected(std::move(created.error()));
        clientSystem = std::move(*created);
        auto added =
          clientSystem->addClient(clientId, client_context->player_uuid(), client_context->ship_speed(),
                                  coordinate.is_system() ? SystemLocation{} : SystemLocation{coordinate});
        if (!added)
          return std::unexpected(std::move(added.error()));

        client_context->set_system_world(clientSystem);
      }

      auto location = clientSystem->clientShipLocation(clientId);
      if (!location)
        return std::unexpected(std::move(location.error()));
      if (std::holds_alternative<std::monostate>(*location))
        return false;
      if (!coordinate.is_system()) {
        auto created = triggerWorldCreation(CelestialWorldId(coordinate));
        if (!created)
          return std::unexpected(std::move(created.error()));
        if (!*created)
          return false;
      }
      if (auto logged = m_root.logger().info("UniverseServer: Arriving ship for player {} at {}", clientId, coordinate);
          !logged)
        return std::unexpected(std::move(logged.error()));

      // world is loaded, ship has arrived
      client_context->set_ship_coordinate(coordinate);
      client_context->set_ship_location(*location);
      auto clientShip = createWorld(ClientShipWorldId(client_context->player_uuid()));
      if (!clientShip)
        return std::unexpected(std::move(clientShip.error()));
      if (*clientShip) {
        auto skyParameters = clientSystem->clientSkyParameters(clientId);
        if (!skyParameters)
          return std::unexpected(std::move(skyParameters.error()));
        locker.unlock();
        auto action = (*clientShip)->executeAction([&](WorldServerThread&, WorldServer& worldServer) {
          return worldServer.stopFlyingSkyAt(*skyParameters);
        });
        auto clients = (*clientShip)->clients();
        lock_with_diagnostics(locker);
        if (!action)
          return std::unexpected(std::move(action.error()));

        for (auto shipClientId : clients) {
          if (auto client_context = m_clients.value(shipClientId))
            client_context->set_orbit_warp_action(clientSystem->clientWarpAction(clientId));
        }
      }
      return true;
    }();
    if (!process)
      return std::unexpected(std::move(process.error()));
    if (*process)
      it = m_pendingArrivals.erase(it);
    else
      ++it;
  }
  return {};
}

std::expected<void, StarException> UniverseServer::respond_to_celestial_requests() {
  std::unique_lock locker(m_mainLock, std::defer_lock);
  lock_with_diagnostics(locker);
  std::shared_lock clientsLocker(m_clientsLock);
  for (auto&& entry : m_pending_celestial_requests) {
    List<CelestialResponse> responses;
    for (auto it = entry.second.begin(); it != entry.second.end();) {
      auto ready = it->poll();
      if (!ready)
        return std::unexpected(std::move(ready.error()));
      if (*ready) {
        auto response = it->get();
        if (!response)
          return std::unexpected(std::move(response.error()));
        // add_celestial_requests puts the only consumer handle directly in this queue.
        if (responses.empty())
          responses.reserve(entry.second.size());
        responses.push_back(std::move(response->get()));
        it = entry.second.erase(it);
      } else {
        ++it;
      }
    }
    if (m_clients.contains(entry.first)) {
      if (auto sent = m_connectionServer->sendPackets(entry.first, {std::make_shared<CelestialResponsePacket>(std::move(responses))}); !sent)
        return std::unexpected(std::move(sent.error()));
    }
  }
  Star::erase_if(m_pending_celestial_requests, [](auto const& entry) { return entry.second.empty(); });
  return {};
}

std::expected<void, StarException> UniverseServer::process_chat() {
  std::unique_lock locker(m_mainLock, std::defer_lock);
  lock_with_diagnostics(locker);
  std::shared_lock clients_locker(m_clientsLock);

  for (auto&& [client_id, pending] : std::exchange(m_pending_chat, {})) {
    if (m_disconnectingClients.contains(client_id))
      continue;
    if (auto client_context = m_clients.value(client_id)) {
      for (auto& [origin, message, send_mode, data] : pending) {
        if (origin.lock() != client_context)
          continue;
        if (client_context->remote_address()) {
          if (auto logged = m_root.logger().info("Chat: <{}> {}", client_context->player_name(), message); !logged)
            return std::unexpected(std::move(logged.error()));
        }

        auto team = m_teamManager->getTeam(client_context->player_uuid());
        locker.unlock();
        auto sent = [&]() -> std::expected<void, StarException> {
          if (send_mode == ChatSendMode::Broadcast)
            return m_chat_processor.broadcast(client_id, message, std::move(data));
          if (send_mode == ChatSendMode::Party && team)
            return m_chat_processor.message(client_id, MessageContext::Mode::Party, team->hex(), message, std::move(data));
          auto name = Star::print_world_id(client_context->player_world_id());
          if (!name)
            return std::unexpected(std::move(name.error()));
          return m_chat_processor.message(client_id, MessageContext::Mode::Local, *name, message, std::move(data));
        }();
        lock_with_diagnostics(locker);
        if (!sent)
          return std::unexpected(std::move(sent.error()));
      }
    }
  }

  return m_chat_processor.update_promises();
}

std::expected<void, StarException> UniverseServer::clearBrokenWorlds() {
  std::unique_lock locker(m_mainLock, std::defer_lock);
  lock_with_diagnostics(locker);
  if (std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() + std::chrono::microseconds(500)) >= m_clearBrokenWorldsDeadline) {
    for (auto it = m_worlds.begin(); it != m_worlds.end();) {
      if (!it->second) {
        auto name = Star::print_world_id(it->first);
        if (!name)
          return std::unexpected(std::move(name.error()));
        if (auto logged = m_root.logger().info("UniverseServer: Clearing broken world {}", *name); !logged)
          return std::unexpected(std::move(logged.error()));
        it = m_worlds.erase(it);
      } else {
        ++it;
      }
    }
    auto assets = m_root.assets();
    if (!assets)
      return std::unexpected(std::move(assets.error()));
    auto json = (*assets)->json("/universe_server.config:clearBrokenWorldsInterval");
    if (!json)
      return std::unexpected(std::move(json.error()));
    auto interval = json->to_int();
    if (!interval)
      return std::unexpected(std::move(interval.error()));
    m_clearBrokenWorldsDeadline = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() + std::chrono::microseconds(500)) + std::chrono::milliseconds(*interval);
  }
  return {};
}

std::expected<void, StarException> UniverseServer::handleWorldMessages() {
  std::unique_lock locker(m_mainLock, std::defer_lock);
  lock_with_diagnostics(locker);
  std::shared_lock clientsLocker(m_clientsLock);
  for (auto it = m_pendingWorldMessages.begin(); it != m_pendingWorldMessages.end();) {
    auto created = triggerWorldCreation(it->first);
    if (!created)
      return std::unexpected(std::move(created.error()));
    if (!*created) {
      ++it;
      continue;
    }
    auto const& world = **created;
    if (world) {
      if (!world->is_running()) {
        ++it;
        continue;
      }
      world->passMessages(std::move(it->second));
    } else {
      for (auto& message : it->second) {
        if (auto failed = message.promise.fail("Error creating world"); !failed)
          return std::unexpected(std::move(failed.error()));
      }
    }
    it = m_pendingWorldMessages.erase(it);
  }
  return {};
}

std::expected<void, StarException> UniverseServer::handleUniverseMessages() {
  for (auto const& uuid : m_universeMessagePromises.keys()) {
    auto& entry = m_universeMessagePromises[uuid];
    {
      std::shared_lock clientsLocker(m_clientsLock);
      if (!m_clients.value(entry.first)) {
        m_universeMessagePromises.remove(uuid);
        continue;
      }
    }
    auto finished = entry.second.finished();
    if (!finished)
      return std::unexpected(std::move(finished.error()));
    if (!*finished)
      continue;
    auto result = entry.second.result();
    if (!result)
      return std::unexpected(std::move(result.error()));
    if (*result) {
      if (auto sent =
            m_connectionServer->sendPackets(entry.first, {std::make_shared<UniverseMessageResponse>(**result, uuid)});
          !sent)
        return std::unexpected(std::move(sent.error()));
    } else {
      auto error = entry.second.error();
      if (!error)
        return std::unexpected(std::move(error.error()));
      if (auto sent = m_connectionServer->sendPackets(
            entry.first, {std::make_shared<UniverseMessageResponse>(std::unexpected(**error), uuid)});
          !sent)
        return std::unexpected(std::move(sent.error()));
    }
    m_universeMessagePromises.remove(uuid);
  }
  std::unique_lock locker(m_messageLock, std::defer_lock);
  lock_with_diagnostics(locker);
  auto messages = std::exchange(m_universeMessages, {});
  locker.unlock();
  std::optional<StarException> error;
  for (auto& message : messages) {
    auto processed = [&]() -> std::expected<void, StarException> {
      bool remote = std::holds_alternative<std::pair<Uuid, ConnectionId>>(message.keeper);
      auto response = receiveMessage(message.message, !remote, message.args);
      if (remote) {
        auto [uuid, clientId] = std::get<std::pair<Uuid, ConnectionId>>(message.keeper);
        if (!response)
          return m_connectionServer->sendPackets(
            clientId,
            {std::make_shared<UniverseMessageResponse>(
              std::unexpected(String(std::string_view(response.error().what()))), uuid)});
        if (!*response)
          return m_connectionServer->sendPackets(clientId,
                                                 {std::make_shared<UniverseMessageResponse>(
                                                   std::unexpected(String("Message not handled by universe")), uuid)});
        if (std::holds_alternative<RpcPromise<Json>>(**response)) {
          m_universeMessagePromises[uuid] = {clientId, std::get<RpcPromise<Json>>(**response)};
          return {};
        }
        return m_connectionServer->sendPackets(
          clientId, {std::make_shared<UniverseMessageResponse>(std::get<Json>(**response), uuid)});
      }
      auto& keeper = std::get<RpcPromiseKeeper<Json>>(message.keeper);
      if (!response)
        return keeper.fail(String(std::string_view(response.error().what())));
      if (!*response)
        return keeper.fail("Message not handled by universe");
      if (std::holds_alternative<RpcPromise<Json>>(**response))
        return keeper.chain(std::get<RpcPromise<Json>>(**response));
      return keeper.fulfill(std::get<Json>(**response));
    }();
    if (!processed) {
      if (error)
        *error = error->with_related(processed.error(), "Additional universe message failure");
      else
        error = std::move(processed.error());
    }
  }
  if (error)
    return std::unexpected(std::move(*error));
  return {};
}

std::expected<void, StarException> UniverseServer::shutdownInactiveWorlds() {
  std::unique_lock locker(m_mainLock, std::defer_lock);
  lock_with_diagnostics(locker);
  std::shared_lock clientsLocker(m_clientsLock);

  // Shutdown idle and errored worlds.
  for (auto const& worldId : m_worlds.keys()) {
    auto loaded = getWorld(worldId);
    if (!loaded)
      return std::unexpected(std::move(loaded.error()));
    if (auto world = *loaded) {
      clientsLocker.unlock();
      locker.unlock();
      if (world->serverErrorOccurred()) {
        auto stopped = world->stop();
        if (!stopped && !world->is_joined())
          return std::unexpected(std::move(stopped.error()));
        auto name = Star::print_world_id(worldId);
        if (!name)
          return std::unexpected(std::move(name.error()));
        if (auto logged = m_root.logger().error("UniverseServer: World {} has stopped due to an error", *name); !logged)
          return std::unexpected(std::move(logged.error()));
        if (!stopped) {
          if (auto logged = m_root.logger().error_exception("UniverseServer: World shutdown error",
                                                            ExceptionDiagnostic(stopped.error()));
              !logged)
            return std::unexpected(std::move(logged.error()));
        }
        if (auto died = worldDiedWithError(world->worldId()); !died)
          return std::unexpected(std::move(died.error()));
      } else if (world->noClients() && world->shouldExpire()) {
        bool anyPendingWarps = false;
        for (auto const& p : m_pendingPlayerWarps) {
          auto warp = resolveWarpAction(p.second.first, p.first, p.second.second);
          if (!warp)
            return std::unexpected(std::move(warp.error()));
          if (warp->world == world->worldId()) {
            anyPendingWarps = true;
            break;
          }
        }
        if (!anyPendingWarps) {
          for (auto const& p : m_pendingSubWorlds) {
            for (auto const& ps : p.second) {
              if (ps.second == world->worldId()) {
                // not really a warp, but close enough.
                anyPendingWarps = true;
                break;
              } 
            }
            if (anyPendingWarps) {
              break;
            }
          }
        }

        if (!anyPendingWarps) {
          auto name = Star::print_world_id(worldId);
          if (!name)
            return std::unexpected(std::move(name.error()));
          if (auto logged = m_root.logger().info("UniverseServer: Stopping idle world {}", *name); !logged)
            return std::unexpected(std::move(logged.error()));
          if (auto stopped = world->stop(); !stopped)
            return std::unexpected(std::move(stopped.error()));
        }
      }
      lock_with_diagnostics(locker);
      clientsLocker.lock();
      if (world->is_joined()) {
        auto kickClients = world->clients();
        if (!kickClients.empty()) {
          auto name = Star::print_world_id(worldId);
          if (!name)
            return std::unexpected(std::move(name.error()));
          if (auto logged = m_root.logger().info("UniverseServer: World {} shutdown, kicking/removing {} players/subworlds", *name, world->clients().size()); !logged)
            return std::unexpected(std::move(logged.error()));
          for (auto clientId : world->clients()) {
            if (clientId >= min_client_sub_world_connection_id) {
              // subworlds are immediately removed
              auto swid = world->clientSubWorld(clientId);
              auto cid = sub_world_to_main_connection_id(clientId);
              auto packets = world->removeClient(clientId);
              if (!packets) {
                auto finalPackets = world->pullOutgoingPackets(clientId);
                if (!finalPackets.empty()) {
                  if (auto sent = m_connectionServer->sendPackets(cid, std::move(finalPackets)); !sent)
                    return std::unexpected(packets.error().with_related(sent.error(), "Sending final subworld packets failed"));
                }
                if (!world->hasClient(clientId)) {
                  auto client = m_clients.get(cid);
                  if (!client)
                    return std::unexpected(packets.error().with_related(client.error(), "Finding departing subworld client failed"));
                  client->get()->clear_sub_world(swid);
                }
                return std::unexpected(std::move(packets.error()));
              }
              if (auto sent = m_connectionServer->sendPackets(cid, std::move(*packets)); !sent)
                return std::unexpected(std::move(sent.error()));
              auto client = m_clients.get(cid);
              if (!client)
                return std::unexpected(std::move(client.error()));
              client->get()->clear_sub_world(swid);
            } else {
              if (auto warped = clientWarpPlayer(clientId, WarpAlias::OwnShip, false); !warped)
                return std::unexpected(std::move(warped.error()));
            }
          }
        }

        if (std::holds_alternative<ClientShipWorldId>(worldId)) {
          if (auto unloaded = world->unloadAll(true); !unloaded)
            return std::unexpected(std::move(unloaded.error()));
          if (auto clientId = getClientForUuid(std::get<ClientShipWorldId>(worldId))) {
            auto client = m_clients.get(*clientId);
            if (!client)
              return std::unexpected(std::move(client.error()));
            auto chunks = world->readChunks();
            if (!chunks)
              return std::unexpected(std::move(chunks.error()));
            client->get()->update_ship_chunks(std::move(*chunks));
          }
        } else if (std::holds_alternative<ClientCustomWorldId>(worldId)) {
          if (auto unloaded = world->unloadAll(true); !unloaded)
            return std::unexpected(std::move(unloaded.error()));
          auto ccwId = std::get<ClientCustomWorldId>(worldId);
          if (auto clientId = getClientForUuid(ccwId.uuid)) {
            auto client = m_clients.get(*clientId);
            if (!client)
              return std::unexpected(std::move(client.error()));
            auto chunks = world->readChunks();
            if (!chunks)
              return std::unexpected(std::move(chunks.error()));
            client->get()->update_custom_world_chunks(ccwId.name, std::move(*chunks));
            client->get()->set_custom_world_active(ccwId.name, false);
          }
        }

        if (world->noClients()) {
          if (auto closed = world->close(); !closed)
            return std::unexpected(std::move(closed.error()));
        } else {
          m_retiringWorlds.push_back(world);
        }

        m_worlds.remove(worldId);
        // Once a world is shutdown, mark its shutdown time in m_tempWorldIndex
        if (std::holds_alternative<InstanceWorldId>(worldId)) {
          auto const& instanceWorldId = std::get<InstanceWorldId>(worldId);
          if (m_tempWorldIndex.contains(instanceWorldId))
            m_tempWorldIndex[instanceWorldId].first = m_universeClock->milliseconds();
        }
      }
    }
  }
  clientsLocker.unlock();

  // A queued warp is not a completed departure: warpPlayers() must first
  // remove the client and send its GiveItem/WorldStop (and other final) packets.
  // Keep only ownership here, not routing by ID, so an errored ship can be
  // recreated while clients still reference its stopped predecessor.
  for (std::size_t i = 0; i < m_retiringWorlds.size();) {
    if (m_retiringWorlds[i]->noClients()) {
      if (auto closed = m_retiringWorlds[i]->close(); !closed)
        return std::unexpected(std::move(closed.error()));
      m_retiringWorlds.erase_at(i);
    } else {
      ++i;
    }
  }

  // Clear out all temporary worlds shut down more than tempWorldDeleteTime time ago.
  // Keep around worlds that are currently running or are active in system worlds
  Set<InstanceWorldId> systemLocationWorlds;
  for (auto const& p : m_systemWorlds) {
    for (auto const& instanceWorldId : p.second->activeInstanceWorlds()) {
      if (m_tempWorldIndex.contains(instanceWorldId))
        systemLocationWorlds.add(instanceWorldId);
    }
  }
  for (auto it = m_tempWorldIndex.begin(); it != m_tempWorldIndex.end();) {
    auto const& entry = *it;
    if (!m_worlds.contains(WorldId(entry.first)) && !systemLocationWorlds.contains(entry.first)
        && m_universeClock->milliseconds() > std::int64_t(entry.second.first + entry.second.second)) {
      auto name = Star::print_world_id(entry.first);
      if (!name)
        return std::unexpected(std::move(name.error()));
      if (auto logged = m_root.logger().info("UniverseServer: Expiring temporary world {}", *name); !logged)
        return std::unexpected(std::move(logged.error()));
      auto path = tempWorldFile(entry.first);
      auto exists = File::is_regular_file(path);
      if (!exists)
        return std::unexpected(std::move(exists.error()));
      if (*exists) {
        if (auto removed = File::remove(path); !removed)
          return std::unexpected(std::move(removed.error()));
      }
      it = m_tempWorldIndex.erase(it);
    } else {
      ++it;
    }
  }
  for (auto it = m_systemWorlds.begin(); it != m_systemWorlds.end();) {
    if (!it->second->clients().empty()) {
      ++it;
      continue;
    }
    auto system = it->second;
    system->stop();
    auto joined = system->join();
    if (!joined)
      return std::unexpected(std::move(joined.error()));
    if (auto stored = system->store(); !stored)
      return std::unexpected(std::move(stored.error()));
    it = m_systemWorlds.erase(it);
  }
  return {};
}

std::expected<void, StarException> UniverseServer::doTriggeredStorage() {
  std::unique_lock locker(m_mainLock, std::defer_lock);
  lock_with_diagnostics(locker);
  std::shared_lock clientsLocker(m_clientsLock);
  if (std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() + std::chrono::microseconds(500)) < m_storageTriggerDeadline)
    return {};
  if (auto logged = m_root.logger().debug("UniverseServer: periodic sync to disk"); !logged)
    return std::unexpected(std::move(logged.error()));
  if (auto saved = saveSettings(); !saved)
    return std::unexpected(std::move(saved.error()));
  if (auto saved = saveTempWorldIndex(); !saved)
    return std::unexpected(std::move(saved.error()));
  clientsLocker.unlock();
  locker.unlock();
  for (auto const& entry : m_clients) {
    auto const& client = entry.second;
    auto ship = getWorld(ClientShipWorldId(client->player_uuid()));
    if (!ship)
      return std::unexpected(std::move(ship.error()));
    if (*ship) {
      auto chunks = (*ship)->readChunks();
      if (!chunks)
        return std::unexpected(std::move(chunks.error()));
      client->update_ship_chunks(std::move(*chunks));
    }
    for (auto const& name : client->custom_worlds()) {
      auto world = getWorld(ClientCustomWorldId(client->player_uuid(), name));
      if (!world)
        return std::unexpected(std::move(world.error()));
      if (*world) {
        auto chunks = (*world)->readChunks();
        if (!chunks)
          return std::unexpected(std::move(chunks.error()));
        client->update_custom_world_chunks(name, std::move(*chunks));
      }
    }
    if (auto cleaned = client->clean_inactive_custom_worlds(); !cleaned)
      return std::unexpected(std::move(cleaned.error()));
    auto database = m_root.versioningDatabase();
    if (!database)
      return std::unexpected(std::move(database.error()));
    auto store = client->store_server_data();
    if (!store)
      return std::unexpected(std::move(store.error()));
    auto versioned = (*database)->makeCurrentVersionedJson("ClientContext", *store);
    if (!versioned)
      return std::unexpected(std::move(versioned.error()));
    auto file = File::relative_to(m_storageDirectory, Star::format("{}.clientcontext", client->player_uuid().hex()));
    if (auto saved = VersionedJson::write_file(*versioned, file); !saved)
      return std::unexpected(std::move(saved.error()));
  }
  lock_with_diagnostics(locker);
  clientsLocker.lock();
  auto assets = m_root.assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  auto json = (*assets)->json("/universe_server.config:universeStorageInterval");
  if (!json)
    return std::unexpected(std::move(json.error()));
  auto interval = json->to_int();
  if (!interval)
    return std::unexpected(std::move(interval.error()));
  m_storageTriggerDeadline = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() + std::chrono::microseconds(500)) + std::chrono::milliseconds(*interval);
  return m_celestial_database->cleanup_and_commit();
}

std::expected<void, StarException> UniverseServer::saveSettings() {
  std::unique_lock locker(m_mainLock, std::defer_lock);
  lock_with_diagnostics(locker);
  auto database = m_root.versioningDatabase();
  if (!database)
    return std::unexpected(std::move(database.error()));
  auto settings = m_universeSettings->toJson().set("time", m_universeClock->time());
  if (!settings)
    return std::unexpected(std::move(settings.error()));
  auto versioned = (*database)->makeCurrentVersionedJson("UniverseSettings", *settings);
  if (!versioned)
    return std::unexpected(std::move(versioned.error()));
  return VersionedJson::write_file(*versioned, File::relative_to(m_storageDirectory, "universe.dat"));
}

std::expected<void, StarException> UniverseServer::loadSettings() {
  std::unique_lock locker(m_mainLock, std::defer_lock);
  lock_with_diagnostics(locker);
  auto defaults = [&universe = *this]() -> std::expected<void, StarException> {
    auto settings = UniverseSettings::create(universe.m_root);
    if (!settings)
      return std::unexpected(std::move(settings.error()));
    universe.m_universeClock = std::make_shared<Clock>();
    universe.m_universeSettings = std::move(*settings);
    return {};
  };
  auto database = m_root.versioningDatabase();
  if (!database)
    return std::unexpected(std::move(database.error()));
  auto file = File::relative_to(m_storageDirectory, "universe.dat");
  auto exists = File::is_regular_file(file);
  if (!exists)
    return std::unexpected(std::move(exists.error()));
  if (*exists) {
    auto loaded = [&]() -> std::expected<void, StarException> {
      auto versioned = VersionedJson::read_file(file);
      if (!versioned)
        return std::unexpected(std::move(versioned.error()));
      auto json = (*database)->loadVersionedJson(*versioned, "UniverseSettings");
      if (!json)
        return std::unexpected(std::move(json.error()));
      auto settings = UniverseSettings::create(m_root, *json);
      if (!settings)
        return std::unexpected(std::move(settings.error()));
      auto time = json->get_double("time");
      if (!time)
        return std::unexpected(std::move(time.error()));
      m_universeSettings = std::move(*settings);
      m_universeClock = std::make_shared<Clock>();
      m_universeClock->set_time(*time);
      return {};
    }();
    if (!loaded) {
      if (auto logged =
            m_root.logger().error_exception("UniverseServer: Could not load universe settings file, loading defaults",
                                            ExceptionDiagnostic(loaded.error()));
          !logged)
        return std::unexpected(std::move(logged.error()));
      if (auto renamed = File::rename(file, Star::format("{}.{}.fail", file, Time::milliseconds_since_epoch())); !renamed)
        return std::unexpected(std::move(renamed.error()));
      if (auto result = defaults(); !result)
        return std::unexpected(std::move(result.error()));
    }
  } else if (auto result = defaults(); !result) {
    return std::unexpected(std::move(result.error()));
  }
  m_universeClock->start();
  return {};
}

std::expected<std::optional<CelestialCoordinate>, StarException> UniverseServer::nextStarterWorld() {
  std::unique_lock locker(m_mainLock, std::defer_lock);
  lock_with_diagnostics(locker);
  auto assets = m_root.assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  auto defaultJson = (*assets)->json("/universe_server.config:defaultWorldCoordinate");
  if (!defaultJson)
    return std::unexpected(std::move(defaultJson.error()));
  auto defaultCoordinate = defaultJson->to_string();
  if (!defaultCoordinate)
    return std::unexpected(std::move(defaultCoordinate.error()));
  if (!defaultCoordinate->empty()) {
    auto coordinate = CelestialCoordinate::create(*defaultCoordinate);
    if (!coordinate)
      return std::unexpected(std::move(coordinate.error()));
    return std::optional<CelestialCoordinate>(std::move(*coordinate));
  }
  if (m_nextRandomizedStarterWorld && m_nextRandomizedStarterWorld->done()) {
    auto next = m_nextRandomizedStarterWorld->get();
    if (!next) {
      auto error = std::move(next.error());
      m_nextRandomizedStarterWorld.reset();
      return std::unexpected(std::move(error));
    }
    auto coordinate = next->get();
    m_nextRandomizedStarterWorld.reset();
    return std::optional<CelestialCoordinate>(std::move(coordinate));
  }
  if (!m_nextRandomizedStarterWorld) {
    m_nextRandomizedStarterWorld = m_workerPool.add_producer<CelestialCoordinate>([&root = m_root, assets = *assets, database = m_celestial_database]() -> std::expected<CelestialCoordinate, StarException> {
      if (auto logged = root.logger().info("Searching for new randomized starter world"); !logged)
        return std::unexpected(std::move(logged.error()));
      auto filterWorld = [database](CelestialCoordinate const& coordinate, Json const& filter) -> std::expected<bool, StarException> {
        auto parameters = database->parameters(coordinate);
        if (!parameters)
          return std::unexpected(std::move(parameters.error()));
        if (!*parameters)
          return false;
        auto visitable = (*parameters)->visitable_parameters();
        if (!visitable)
          return false;
        auto biome = filter.opt_string("terrestrialBiome");
        if (!biome)
          return std::unexpected(std::move(biome.error()));
        if (*biome) {
          auto terrestrial = as<TerrestrialWorldParameters>(visitable);
          if (!terrestrial || **biome != terrestrial->primaryBiome)
            return false;
        }
        auto size = filter.opt_string("terrestrialSize");
        if (!size)
          return std::unexpected(std::move(size.error()));
        if (*size) {
          auto terrestrial = as<TerrestrialWorldParameters>(visitable);
          if (!terrestrial || **size != terrestrial->sizeName)
            return false;
        }
        auto dungeon = filter.opt_string("floatingDungeon");
        if (!dungeon)
          return std::unexpected(std::move(dungeon.error()));
        if (*dungeon) {
          auto floating = as<FloatingDungeonWorldParameters>(visitable);
          if (!floating || **dungeon != floating->primaryDungeon)
            return false;
        }
        return true;
      };
      auto config = assets->json("/universe_server.config:findStarterWorldParameters");
      if (!config)
        return std::unexpected(std::move(config.error()));
      auto tries = config->get_uint("tries");
      if (!tries)
        return std::unexpected(std::move(tries.error()));
      auto range = config->get_uint("range");
      if (!range)
        return std::unexpected(std::move(range.error()));
      auto random = database->find_random_world(*tries, *range, [&](CelestialCoordinate const& coordinate) -> std::expected<bool, StarException> {
        auto starter = config->get("starterWorld");
        if (!starter)
          return std::unexpected(std::move(starter.error()));
        auto matches = filterWorld(coordinate, *starter);
        if (!matches)
          return std::unexpected(std::move(matches.error()));
        if (!*matches)
          return false;
        auto system = coordinate.system();
        if (!system)
          return std::unexpected(std::move(system.error()));
        auto planets = database->children(*system);
        if (!planets)
          return std::unexpected(std::move(planets.error()));
        List<CelestialCoordinate> children;
        for (auto const& planet : *planets) {
          children.push_back(planet);
          auto satellites = database->children(planet);
          if (!satellites)
            return std::unexpected(std::move(satellites.error()));
          children.append_range(*satellites);
        }
        auto required = config->get_array("requiredSystemWorlds", {});
        if (!required)
          return std::unexpected(std::move(required.error()));
        for (auto const& filter : *required) {
          bool found = false;
          for (auto const& world : children) {
            auto match = filterWorld(world, filter);
            if (!match)
              return std::unexpected(std::move(match.error()));
            if (*match) {
              found = true;
              break;
            }
          }
          if (!found)
            return false;
        }
        return true;
      });
      if (!random)
        return std::unexpected(std::move(random.error()));
      auto logged = *random
          ? root.logger().info("UniverseServer: Found randomized starter world at {}", **random)
          : root.logger().error("UniverseServer: Could not find randomized starter world!");
      if (!logged)
        return std::unexpected(std::move(logged.error()));
      return random->value_or(CelestialCoordinate{});
    });
  }
  return std::nullopt;
}

std::expected<void, StarException> UniverseServer::loadTempWorldIndex() {
  auto database = m_root.versioningDatabase();
  if (!database)
    return std::unexpected(std::move(database.error()));
  auto storageFile = File::relative_to(m_storageDirectory, "tempworlds.index");
  auto exists = File::is_regular_file(storageFile);
  if (!exists)
    return std::unexpected(std::move(exists.error()));
  if (*exists) {
    auto loaded = [&]() -> std::expected<void, StarException> {
      m_tempWorldIndex.clear();
      auto versioned = VersionedJson::read_file(storageFile);
      if (!versioned)
        return std::unexpected(std::move(versioned.error()));
      auto settings = (*database)->loadVersionedJson(*versioned, "TempWorldIndex");
      if (!settings)
        return std::unexpected(std::move(settings.error()));
      auto entries = settings->to_object();
      if (!entries)
        return std::unexpected(std::move(entries.error()));
      for (auto const& entry : *entries) {
        auto world = parseWorldId(entry.first);
        if (!world)
          return std::unexpected(std::move(world.error()));
        if (!std::holds_alternative<InstanceWorldId>(*world))
          return std::unexpected(UniverseServerException("Temporary world index contains a non-instance world"));
        auto first = entry.second.get(0);
        if (!first)
          return std::unexpected(std::move(first.error()));
        auto second = entry.second.get(1);
        if (!second)
          return std::unexpected(std::move(second.error()));
        auto timestamp = first->to_uint();
        if (!timestamp)
          return std::unexpected(std::move(timestamp.error()));
        auto lifetime = second->to_uint();
        if (!lifetime)
          return std::unexpected(std::move(lifetime.error()));
        m_tempWorldIndex.insert(std::get<InstanceWorldId>(*world), {*timestamp, *lifetime});
      }
      return {};
    }();
    if (!loaded) {
      if (auto logged = m_root.logger().error_exception("UniverseServer: Could not load temp world index file",
                                                        ExceptionDiagnostic(loaded.error()));
          !logged)
        return std::unexpected(std::move(logged.error()));
      if (auto renamed = File::rename(storageFile, Star::format("{}.{}.fail", storageFile, Time::milliseconds_since_epoch())); !renamed)
        return std::unexpected(std::move(renamed.error()));
    }
  }

  List<String> indexed;
  indexed.reserve(m_tempWorldIndex.size());
  for (auto const& entry : m_tempWorldIndex)
    indexed.push_back(tempWorldFile(entry.first));
  auto files = File::directory_entries(m_storageDirectory);
  if (!files)
    return std::unexpected(std::move(files.error()));
  for (auto const& file : *files) {
    if (file.second)
      continue;
    auto temporary = file.first.ends_with(".tempworld");
    if (!temporary)
      return std::unexpected(std::move(temporary.error()));
    if (*temporary) {
      String path = File::relative_to(m_storageDirectory, file.first);
      if (!indexed.contains(path)) {
        if (auto logged = m_root.logger().info("UniverseServer: Removing unindexed temporary world {}", file.first); !logged)
          return std::unexpected(std::move(logged.error()));
        if (auto removed = File::remove(path); !removed)
          return std::unexpected(std::move(removed.error()));
      }
    }
  }
  return {};
}

std::expected<void, StarException> UniverseServer::saveTempWorldIndex() {
  JsonObject index;
  for (auto const& entry : m_tempWorldIndex) {
    auto name = Star::print_world_id(entry.first);
    if (!name)
      return std::unexpected(std::move(name.error()));
    index.set(*name, JsonArray{entry.second.first, entry.second.second});
  }
  auto database = m_root.versioningDatabase();
  if (!database)
    return std::unexpected(std::move(database.error()));
  auto versioned = (*database)->makeCurrentVersionedJson("TempWorldIndex", index);
  if (!versioned)
    return std::unexpected(std::move(versioned.error()));
  return VersionedJson::write_file(*versioned, File::relative_to(m_storageDirectory, "tempworlds.index"));
}

String UniverseServer::tempWorldFile(InstanceWorldId const& worldId) const {
  String identifier = worldId.instance;
  if (worldId.uuid)
    identifier = Star::format("{}-{}", identifier, worldId.uuid->hex());
  if (worldId.level)
    identifier = Star::format("{}-{}", identifier, worldId.level.value_or(0.0f));
  return File::relative_to(m_storageDirectory, Star::format("{}.tempworld", identifier));
}

std::expected<std::optional<String>, StarException> UniverseServer::isBannedUser(std::optional<HostAddress> hostAddress, Uuid playerUuid) const {
  std::unique_lock locker(m_mainLock, std::defer_lock);
  lock_with_diagnostics(locker);
  auto config = m_root.configuration();
  if (!config)
    return std::unexpected(std::move(config.error()));
  if (hostAddress) {
    for (auto const& ban : m_tempBans) {
      if (ban.ip && *ban.ip == *hostAddress)
        return std::optional<String>(ban.reason);
    }
    auto json = (*config)->get("bannedIPs");
    if (!json)
      return std::unexpected(std::move(json.error()));
    auto bans = json->to_array();
    if (!bans)
      return std::unexpected(std::move(bans.error()));
    for (auto const& ban : *bans) {
      auto text = ban.get_string("ip");
      if (!text)
        return std::unexpected(std::move(text.error()));
      auto address = HostAddress::lookup(*text);
      if (!address)
        return std::unexpected(std::move(address.error()));
      if (*address == *hostAddress) {
        auto reason = ban.get_string("reason");
        if (!reason)
          return std::unexpected(std::move(reason.error()));
        return std::optional<String>(std::move(*reason));
      }
    }
  }
  for (auto const& ban : m_tempBans) {
    if (ban.uuid && *ban.uuid == playerUuid)
      return std::optional<String>(ban.reason);
  }
  auto json = (*config)->get("bannedUuids");
  if (!json)
    return std::unexpected(std::move(json.error()));
  auto bans = json->to_array();
  if (!bans)
    return std::unexpected(std::move(bans.error()));
  for (auto const& ban : *bans) {
    auto text = ban.get_string("uuid");
    if (!text)
      return std::unexpected(std::move(text.error()));
    auto uuid = Uuid::from_hex((*text).utf8());
    if (!uuid)
      return std::unexpected(std::move(uuid.error()));
    if (*uuid == playerUuid) {
      auto reason = ban.get_string("reason");
      if (!reason)
        return std::unexpected(std::move(reason.error()));
      return std::optional<String>(std::move(*reason));
    }
  }
  return std::nullopt;
}

std::expected<void, StarException> UniverseServer::doTempBan(ConnectionId clientId, String const& reason, std::pair<bool, bool> banType, int timeout) {
  std::unique_lock locker(m_mainLock, std::defer_lock);
  lock_with_diagnostics(locker);
  std::shared_lock clientsLocker(m_clientsLock);

  if (auto client_context = m_clients.value(clientId)) {
    if (!client_context->remote_address())
      return {};

    auto banExpiry = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() + std::chrono::microseconds(500)) + std::chrono::milliseconds(timeout * 1000);
    std::optional<HostAddress> ip;
    if (banType.first)
      ip = client_context->remote_address();

    std::optional<Uuid> uuid;

    if (banType.second)
      uuid = client_context->player_uuid();

    m_tempBans.push_back({banExpiry, reason, ip, uuid});
  }
  return {};
}

std::expected<void, StarException> UniverseServer::doPermBan(ConnectionId clientId, String const& reason, std::pair<bool, bool> banType) {
  std::unique_lock locker(m_mainLock, std::defer_lock);
  lock_with_diagnostics(locker);
  std::shared_lock clientsLocker(m_clientsLock);

  if (auto client_context = m_clients.value(clientId)) {
    if (!client_context->remote_address())
      return {};

    auto config = m_root.configuration();
    if (!config)
      return std::unexpected(std::move(config.error()));
    if (banType.first) {
      auto json = (*config)->get("bannedIPs");
      if (!json)
        return std::unexpected(std::move(json.error()));
      auto bannedIPs = json->to_array();
      if (!bannedIPs)
        return std::unexpected(std::move(bannedIPs.error()));
      bannedIPs->push_back(JsonObject{
        {"ip", Star::to_string(*client_context->remote_address())},
        {"reason", reason},
      });

      if (auto stored = (*config)->set("bannedIPs", *bannedIPs); !stored)
        return std::unexpected(std::move(stored.error()));
    }

    if (banType.second) {
      auto json = (*config)->get("bannedUuids");
      if (!json)
        return std::unexpected(std::move(json.error()));
      auto bannedUuids = json->to_array();
      if (!bannedUuids)
        return std::unexpected(std::move(bannedUuids.error()));
      bannedUuids->push_back(JsonObject{
        {"uuid", client_context->player_uuid().hex()},
        {"reason", reason},
      });

      if (auto stored = (*config)->set("bannedUuids", *bannedUuids); !stored)
        return std::unexpected(std::move(stored.error()));
    }
  }
  return {};
}

std::expected<void, StarException> UniverseServer::removeTimedBan() {
  std::unique_lock locker(m_mainLock, std::defer_lock);
  lock_with_diagnostics(locker);
  auto currentTime = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() + std::chrono::microseconds(500));
  m_tempBans.filter([currentTime](TimeoutBan const& b) { return b.banExpiry > currentTime; });
  return {};
}

std::expected<void, StarException> UniverseServer::add_celestial_requests(ConnectionId clientId, List<CelestialRequest> requests) {
  std::unique_lock locker(m_mainLock, std::defer_lock);
  lock_with_diagnostics(locker);
  for (auto const& request : requests) {
    m_pending_celestial_requests[clientId].push_back(m_workerPool.add_producer<CelestialResponse>(
      [&database = *m_celestial_database, request]() { return database.respond_to_request(request); }));
  }
  return {};
}

std::expected<void, StarException> UniverseServer::worldUpdated(WorldServerThread& server) {
  for (auto clientId : server.clients()) {
    auto packets = server.pullOutgoingPackets(clientId);
    auto target = clientId >= min_client_sub_world_connection_id ? sub_world_to_main_connection_id(clientId) : clientId;
    if (auto sent = m_connectionServer->sendPackets(target, std::move(packets)); !sent)
      return std::unexpected(std::move(sent.error()));
  }
  return {};
}

std::expected<void, StarException> UniverseServer::systemWorldUpdated(SystemWorldServerThread& systemWorldServer) {
  for (auto clientId : systemWorldServer.clients()) {
    auto packets = systemWorldServer.pullOutgoingPackets(clientId);
    if (auto sent = m_connectionServer->sendPackets(clientId, std::move(packets)); !sent)
      return std::unexpected(std::move(sent.error()));
  }
  return {};
}

std::expected<void, StarException> UniverseServer::packetsReceived(UniverseConnectionServer&, ConnectionId clientId, List<std::shared_ptr<Packet>> packets) {
  std::shared_lock clientsLocker(m_clientsLock);
  if (auto client_context = m_clients.value(clientId); client_context && !m_disconnectingClients.contains(clientId)) {
    clientsLocker.unlock();

    for (auto& packet : packets) {
      if (auto warpAction = as<PlayerWarpPacket>(packet)) {
        auto const& action = warpAction->action;
        bool blocked = m_secureWarps;

        if (std::holds_alternative<WarpAlias>(action) || std::holds_alternative<WarpToPlayer>(action)) {
          blocked = false;
        } else if (std::holds_alternative<WarpToWorld>(action)) {
          auto const& warpToWorld = std::get<WarpToWorld>(action);
          if (std::holds_alternative<std::monostate>(warpToWorld.world) ||
              std::holds_alternative<ClientShipWorldId>(warpToWorld.world) ||
              std::holds_alternative<CelestialWorldId>(warpToWorld.world) ||
              std::holds_alternative<InstanceWorldId>(warpToWorld.world) ||
              std::holds_alternative<CustomWorldId>(warpToWorld.world) ||
              std::holds_alternative<ClientCustomWorldId>(warpToWorld.world)) {
            blocked = false;
          }
        }

        if (blocked) {
          if (auto logged = m_root.logger().warn("UniverseServer: Blocked invalid warp action from client {}", clientId); !logged)
            return std::unexpected(std::move(logged.error()));
          if (auto sent = m_connectionServer->sendPackets(clientId, {std::make_shared<PlayerWarpResultPacket>(false, action, true)}); !sent)
            return std::unexpected(std::move(sent.error()));
          continue;
        }

        if (auto warped = clientWarpPlayer(clientId, warpAction->action, warpAction->deploy); !warped)
          return std::unexpected(std::move(warped.error()));

      } else if (auto flyShip = as<FlyShipPacket>(packet)) {
        if (auto flown = clientFlyShip(clientId, flyShip->system, flyShip->location, flyShip->settings); !flown)
          return std::unexpected(std::move(flown.error()));

      } else if (auto chat_send = as<ChatSendPacket>(packet)) {
        std::unique_lock locker(m_mainLock, std::defer_lock);
        lock_with_diagnostics(locker);
        m_pending_chat[clientId].emplace_back(client_context, std::move(chat_send->text), chat_send->send_mode, std::move(chat_send->data));

      } else if (auto client_context_update_packet = as<ClientContextUpdatePacket>(packet)) {
        if (auto updated = client_context->read_update(client_context_update_packet->update_data); !updated)
          return std::unexpected(std::move(updated.error()));

      } else if (auto clientDisconnectPacket = as<ClientDisconnectRequestPacket>(packet)) {
        if (auto disconnected = disconnectClient(clientId, String()); !disconnected)
          return std::unexpected(std::move(disconnected.error()));

      } else if (auto celestialRequest = as<CelestialRequestPacket>(packet)) {
        if (auto added = add_celestial_requests(clientId, std::move(celestialRequest->requests)); !added)
          return std::unexpected(std::move(added.error()));

      } else if (auto entityMessage = as<EntityMessagePacket>(packet)) {
        entityMessage->fromConnection = clientId;

        if (m_secureWarps && entityMessage->message == "warp") {
          bool blocked = false;

          if (entityMessage->args.empty() || !entityMessage->args[0].can_convert(Json::Type::String)) {
            if (auto logged = m_root.logger().warn("UniverseServer: Blocked warp entity message with invalid args from client {}", clientId); !logged)
              return std::unexpected(std::move(logged.error()));
            blocked = true;
          } else {
            auto parsed = [&]() -> std::expected<WarpAction, StarException> {
              auto text = entityMessage->args[0].to_string();
              if (!text)
                return std::unexpected(std::move(text.error()));
              return parseWarpAction(*text);
            }();
            if (!parsed) {
              if (auto logged = m_root.logger().warn("UniverseServer: Blocked warp entity message with unparseable warp action from client {}", clientId); !logged)
                return std::unexpected(std::move(logged.error()));
              blocked = true;
            }
          }

          if (!blocked) {
            if (std::holds_alternative<EntityId>(entityMessage->entityId)) {
              auto target_owner = connection_for_entity(std::get<EntityId>(entityMessage->entityId));
              if (!target_owner) {
                blocked = true;
              } else if (*target_owner != clientId) {
                clientsLocker.lock();
                bool isAdmin = client_context->is_admin();
                clientsLocker.unlock();
                if (!isAdmin) {
                  if (auto logged = m_root.logger().warn("UniverseServer: Blocked warp entity message from non-admin client {} targeting entity owned by connection {}", clientId, *target_owner); !logged)
                    return std::unexpected(std::move(logged.error()));
                  blocked = true;
                }
              }
            }
          }

          if (blocked) {
            if (auto sent = m_connectionServer->sendPackets(
                  clientId,
                  {std::make_shared<EntityMessageResponsePacket>(
                    std::unexpected(String("Warp entity message blocked by server")), entityMessage->uuid)});
                !sent)
              return std::unexpected(std::move(sent.error()));
            continue;
          }
        }

        if (auto currentWorld = client_context->player_world())
          currentWorld->pushIncomingPackets(clientId, {std::move(packet)});

      } else if (auto clientCustomWorldResponse = as<ClientCustomWorldResponse>(packet)) {
        if (auto received = client_context->custom_world_received(std::move(clientCustomWorldResponse->name), std::move(clientCustomWorldResponse->chunks)); !received)
          return std::unexpected(std::move(received.error()));
      } else if (auto clientCustomWorldCreate = as<ClientCustomWorldCreate>(packet)) {
        std::unique_lock locker(m_mainLock, std::defer_lock);
        lock_with_diagnostics(locker);
        clientsLocker.lock();
        
        auto worldId = ClientCustomWorldId(client_context->player_uuid(),clientCustomWorldCreate->name);
        if (!m_worlds.contains(worldId)) {
          auto worldTemplate = WorldTemplate::create(m_root, clientCustomWorldCreate->templateData);
          if (!worldTemplate)
            return std::unexpected(std::move(worldTemplate.error()));
          auto promise = clientCustomWorldPromise(worldId, *worldTemplate);
          if (!promise)
            return std::unexpected(std::move(promise.error()));
          if (*promise) {
            if (auto added = m_worlds.add(worldId, std::move(*promise)); !added)
              return std::unexpected(std::move(added.error()));
          }
        }
        
        clientsLocker.unlock();
        locker.unlock();
      } else if (auto cwtRequest = as<ClientSubWorldRequest>(packet)) {
        if (auto currentWorld = client_context->sub_world(cwtRequest->subWorldId)) {
          auto removed = currentWorld->removeClient(main_to_sub_world_connection_id(clientId));
          if (!removed)
            return std::unexpected(std::move(removed.error()));
          if (auto sent = m_connectionServer->sendPackets(clientId, std::move(*removed)); !sent)
            return std::unexpected(std::move(sent.error()));
          client_context->clear_sub_world(cwtRequest->subWorldId);
        }
        if (!std::holds_alternative<std::monostate>(cwtRequest->worldId)) {
          auto configuration = m_root.configuration();
          if (!configuration)
            return std::unexpected(std::move(configuration.error()));
          auto option = [&](String const& name, bool fallback) -> std::expected<bool, StarException> {
            auto json = (*configuration)->get(name);
            if (!json)
              return std::unexpected(std::move(json.error()));
            auto value = json->opt_bool();
            if (!value)
              return std::unexpected(std::move(value.error()));
            return value->value_or(fallback);
          };
          bool ownWorld = false;
          if (std::holds_alternative<ClientShipWorldId>(cwtRequest->worldId)) {
            ownWorld = std::get<ClientShipWorldId>(cwtRequest->worldId) == client_context->player_uuid();
          } else if (std::holds_alternative<ClientCustomWorldId>(cwtRequest->worldId)) {
            ownWorld = std::get<ClientCustomWorldId>(cwtRequest->worldId).uuid == client_context->player_uuid();
          }
          auto disallow = option("disallowClientSubWorlds", false);
          if (!disallow)
            return std::unexpected(std::move(disallow.error()));
          bool allowed = ownWorld;
          if (!*disallow && !allowed) {
            auto all = option("allowClientSubWorldsAllWorlds", true);
            if (!all)
              return std::unexpected(std::move(all.error()));
            allowed = *all || client_context->is_admin();
            if (!allowed && cwtRequest->worldId == client_context->player_world_id()) {
              auto current = option("allowClientSubWorldsCurrentWorld", true);
              if (!current)
                return std::unexpected(std::move(current.error()));
              allowed = *current;
            }
          }
          if (*disallow || !allowed) {
            auto name = Star::print_world_id(cwtRequest->worldId);
            if (!name)
              return std::unexpected(std::move(name.error()));
            if (auto logged = m_root.logger().info("UniverseServer: Rejecting client {} subworld {} request for world {}", clientId, cwtRequest->subWorldId, *name); !logged)
              return std::unexpected(std::move(logged.error()));
            if (auto sent = m_connectionServer->sendPackets(clientId, {std::make_shared<ClientSubWorldReject>(cwtRequest->subWorldId)}); !sent)
              return std::unexpected(std::move(sent.error()));
          } else {
            m_pendingSubWorlds[clientId][cwtRequest->subWorldId] = cwtRequest->worldId;
          }
        } else {
          if (m_pendingSubWorlds.contains(clientId)) {
            if (m_pendingSubWorlds[clientId].contains(cwtRequest->subWorldId)) {
              m_pendingSubWorlds[clientId].remove(cwtRequest->subWorldId);
            }
          }
        }
      } else if (auto cwtPackets = as<ClientSubWorldPackets>(packet)) {
        if (auto currentWorld = client_context->sub_world(cwtPackets->subWorldId))
          currentWorld->pushIncomingPackets(main_to_sub_world_connection_id(clientId), std::move(cwtPackets->packets));
      } else if (auto universeMessage = as<UniverseMessage>(packet)) {
        if (universeMessage->connection == server_connection_id) {
          if (auto passed = passMessage({universeMessage->message, universeMessage->args, std::make_pair(universeMessage->uuid, clientId)}); !passed)
            return std::unexpected(std::move(passed.error()));
        } else {
          clientsLocker.lock();
          if (auto client = m_clients.value(universeMessage->connection); client && !m_disconnectingClients.contains(universeMessage->connection)) {
            clientsLocker.unlock();
            if (client->net_rules().version() < 18) {
              if (auto sent = m_connectionServer->sendPackets(
                    clientId,
                    {std::make_shared<UniverseMessageResponse>(std::unexpected(String("Client is not new enough")),
                                                               universeMessage->uuid)});
                  !sent)
                return std::unexpected(std::move(sent.error()));
            } else {
              std::unique_lock messageLocker(m_messageLock, std::defer_lock);
              lock_with_diagnostics(messageLocker);
              m_universeMessageResponses[universeMessage->uuid] = {universeMessage->connection, clientId};
              messageLocker.unlock();
              universeMessage->fromConnection = clientId;
              if (auto sent = m_connectionServer->sendPackets(universeMessage->connection, {std::move(universeMessage)}); !sent)
                return std::unexpected(std::move(sent.error()));
            }
          } else {
            clientsLocker.unlock();
            if (auto sent = m_connectionServer->sendPackets(
                  clientId,
                  {std::make_shared<UniverseMessageResponse>(std::unexpected(String("Client does not exist")),
                                                             universeMessage->uuid)});
                !sent)
              return std::unexpected(std::move(sent.error()));
          }
        }
      } else if (auto universeMessageResponse = as<UniverseMessageResponse>(packet)) {
        std::unique_lock messageLocker(m_messageLock, std::defer_lock);
        lock_with_diagnostics(messageLocker);
        if (!m_universeMessageResponses.contains(universeMessageResponse->uuid)) {
          if (auto logged = m_root.logger().warn("UniverseServer: UniverseMessageResponse received for unknown context [{}]!", universeMessageResponse->uuid.hex()); !logged)
            return std::unexpected(std::move(logged.error()));
        } else {
          auto entry = m_universeMessageResponses.extract_value(universeMessageResponse->uuid);
          if (!entry)
            return std::unexpected(std::move(entry.error()));
          auto response = std::move(entry->second);
          messageLocker.unlock();
          if (std::holds_alternative<ConnectionId>(response)) {
            clientsLocker.lock();
            if (auto client = m_clients.value(std::get<ConnectionId>(response))) {
              if (auto sent = m_connectionServer->sendPackets(std::get<ConnectionId>(response), {std::move(universeMessageResponse)}); !sent)
                return std::unexpected(std::move(sent.error()));
            }
            clientsLocker.unlock();
          } else {
            auto settled = universeMessageResponse->response.has_value()
              ? std::get<RpcPromiseKeeper<Json>>(response).fulfill(*universeMessageResponse->response)
              : std::get<RpcPromiseKeeper<Json>>(response).fail(universeMessageResponse->response.error());
            if (!settled)
              return std::unexpected(std::move(settled.error()));
          }
        }
      } else if (is<SystemObjectSpawnPacket>(packet)) {
        if (auto currentSystem = client_context->system_world())
          currentSystem->pushIncomingPacket(clientId, std::move(packet));
      } else {
        if (auto currentWorld = client_context->player_world())
          currentWorld->pushIncomingPackets(clientId, {std::move(packet)});
      }
    }
  }
  return {};
}

std::expected<void, StarException> UniverseServer::acceptConnection(UniverseConnection connection, std::optional<HostAddress> remoteAddress) {
  std::optional<ConnectionId> registered;
  auto accepted = [&]() -> std::expected<void, StarException> {
    auto assetsResult = m_root.assets();
    if (!assetsResult)
      return std::unexpected(std::move(assetsResult.error()));
    auto assets = *assetsResult;
    auto configurationResult = m_root.configuration();
    if (!configurationResult)
      return std::unexpected(std::move(configurationResult.error()));
    auto configuration = *configurationResult;
    auto databaseResult = m_root.versioningDatabase();
    if (!databaseResult)
      return std::unexpected(std::move(databaseResult.error()));
    auto database = *databaseResult;
    auto waitJson = assets->json("/universe_server.config:clientWaitLimit");
    if (!waitJson)
      return std::unexpected(std::move(waitJson.error()));
    auto wait = waitJson->to_int();
    if (!wait)
      return std::unexpected(std::move(wait.error()));
    int clientWaitLimit = *wait;
    auto serverMismatchJson = assets->json("/universe_server.config:serverAssetsMismatchMessage");
    if (!serverMismatchJson)
      return std::unexpected(std::move(serverMismatchJson.error()));
    auto serverMismatch = serverMismatchJson->to_string();
    if (!serverMismatch)
      return std::unexpected(std::move(serverMismatch.error()));
    auto clientMismatchJson = assets->json("/universe_server.config:clientAssetsMismatchMessage");
    if (!clientMismatchJson)
      return std::unexpected(std::move(clientMismatchJson.error()));
    auto clientMismatch = clientMismatchJson->to_string();
    if (!clientMismatch)
      return std::unexpected(std::move(clientMismatch.error()));
    auto settings = configuration->get("connectionSettings");
    if (!settings)
      return std::unexpected(std::move(settings.error()));
    std::unique_lock mainLocker(m_mainLock, std::defer_lock);
    if (auto received = connection.receiveAny(clientWaitLimit); !received)
      return std::unexpected(std::move(received.error()));
    auto packet = connection.pullSingle();
    if (!packet)
      return std::unexpected(std::move(packet.error()));
    auto request = as<ProtocolRequestPacket>(*packet);
    if (!request)
      return m_root.logger().warn("UniverseServer: client connection aborted, expected ProtocolRequestPacket");
    bool legacyClient = request->compressionMode() != PacketCompressionMode::Enabled;
    auto socket = connection.packetSocket();
    if (!socket)
      return std::unexpected(std::move(socket.error()));
    if (legacyClient)
      socket->get().setNetRules(legacy_version);
    auto response = std::make_shared<ProtocolResponsePacket>();
    response->setCompressionMode(PacketCompressionMode::Enabled);
    if (request->requestProtocolVersion != StarProtocolVersion) {
      if (auto logged = m_root.logger().warn("UniverseServer: client connection aborted, unsupported protocol version {}, supported version {}", request->requestProtocolVersion, StarProtocolVersion); !logged)
        return std::unexpected(std::move(logged.error()));
      response->allowed = false;
      if (auto pushed = connection.pushSingle(response); !pushed)
        return std::unexpected(std::move(pushed.error()));
      if (auto sent = connection.sendAll(clientWaitLimit); !sent)
        return std::unexpected(std::move(sent.error()));
      lock_with_diagnostics(mainLocker);
      m_deadConnections.push_back({std::move(connection),
                                   std::chrono::time_point_cast<std::chrono::milliseconds>(
                                     std::chrono::steady_clock::now() + std::chrono::microseconds(500))});
      return {};
    }
    bool compressionStream = false;
    response->allowed = true;
    if (!legacyClient) {
      auto name = settings->get_string("compression", "None");
      if (!name)
        return std::unexpected(std::move(name.error()));
      if (!NetCompressionModeNames)
        return std::unexpected(NetCompressionModeNames.error());
      auto modeResult = NetCompressionModeNames->maybe_left(*name);
      if (!modeResult)
        return std::unexpected(std::move(modeResult.error()));
      auto mode = modeResult->value_or(NetCompressionMode::None);
      compressionStream = mode == NetCompressionMode::Zstd;
      auto modeName = NetCompressionModeNames->get_right(mode);
      if (!modeName)
        return std::unexpected(std::move(modeName.error()));
      response->info = JsonObject{{"compression", modeName->get()}, {"openProtocolVersion", open_protocol_version}};
    }
    if (auto pushed = connection.pushSingle(response); !pushed)
      return std::unexpected(std::move(pushed.error()));
    if (auto sent = connection.sendAll(clientWaitLimit); !sent)
      return std::unexpected(std::move(sent.error()));
    if (auto compressed = as_ref<CompressedPacketSocket>(socket->get()))
      compressed->get().setCompressionStreamEnabled(compressionStream);
    String remote = remoteAddress ? Star::to_string(*remoteAddress) : "local";
    if (auto logged = m_root.logger().info("UniverseServer: Awaiting connection info from {} ({} client)", remote, legacyClient ? "vanilla" : "custom"); !logged)
      return std::unexpected(std::move(logged.error()));
    if (auto received = connection.receiveAny(clientWaitLimit); !received)
      return std::unexpected(std::move(received.error()));
    auto connectPacket = connection.pullSingle();
    if (!connectPacket)
      return std::unexpected(std::move(connectPacket.error()));
    auto connect = as<ClientConnectPacket>(*connectPacket);
    if (!connect) {
      if (auto logged = m_root.logger().warn("UniverseServer: client connection aborted"); !logged)
        return std::unexpected(std::move(logged.error()));
      if (auto pushed = connection.pushSingle(std::make_shared<ConnectFailurePacket>("connect timeout")); !pushed)
        return std::unexpected(std::move(pushed.error()));
      lock_with_diagnostics(mainLocker);
      m_deadConnections.push_back({std::move(connection),
                                   std::chrono::time_point_cast<std::chrono::milliseconds>(
                                     std::chrono::steady_clock::now() + std::chrono::microseconds(500))});
      return {};
    }
    bool administrator = false;
    String accountString = !connect->account.empty() ? Star::format("'{}'", connect->account) : "<anonymous>";
    auto fail = [&](String message) -> std::expected<void, StarException> {
      if (auto logged = m_root.logger().warn("UniverseServer: Login attempt failed with account '{}' as player '{}' from address {}, error: {}", accountString, connect->playerName, remote, message); !logged)
        return std::unexpected(std::move(logged.error()));
      if (auto pushed = connection.pushSingle(std::make_shared<ConnectFailurePacket>(std::move(message))); !pushed)
        return std::unexpected(std::move(pushed.error()));
      lock_with_diagnostics(mainLocker);
      m_deadConnections.push_back({std::move(connection),
                                   std::chrono::time_point_cast<std::chrono::milliseconds>(
                                     std::chrono::steady_clock::now() + std::chrono::microseconds(500))});
      return {};
    };
    auto latest = settings->get_bool("requireLatestVersion", false);
    if (!latest)
      return std::unexpected(std::move(latest.error()));
    VersionNumber peer_version = legacyClient ? legacy_version : 1;
    if (!legacyClient && (*latest || connect->info)) {
      auto version = read_open_protocol_version(connect->info, *latest ? legacy_version : 1);
      if (!version)
        return std::unexpected(std::move(version.error()));
      peer_version = *version;
    }
    if (*latest && peer_version < open_protocol_version)
      return fail(Star::format("OpenStarbound protocol {} or later is required.\nAsterbound v{}, Source ID: {}...", open_protocol_version, product_version, source_identifier.substr(0, 8)));
    if (!m_speciesShips.contains(connect->shipSpecies))
      return fail("Unknown ship species");
    if (!remoteAddress) {
      administrator = true;
      if (auto logged = m_root.logger().info("UniverseServer: Logged in player '{}' locally", connect->playerName); !logged)
        return std::unexpected(std::move(logged.error()));
    } else {
      if (connect->assetsDigest != m_assetsDigest) {
        auto mismatchJson = configuration->get("allowAssetsMismatch");
        if (!mismatchJson)
          return std::unexpected(std::move(mismatchJson.error()));
        auto mismatch = mismatchJson->to_bool();
        if (!mismatch)
          return std::unexpected(std::move(mismatch.error()));
        if (!*mismatch)
          return fail(*serverMismatch);
        if (!connect->allowAssetsMismatch)
          return fail(*clientMismatch);
      }
      if (!connect->account.empty()) {
        auto lengthJson = assets->json("/universe_server.config:passwordSaltLength");
        if (!lengthJson)
          return std::unexpected(std::move(lengthJson.error()));
        auto length = lengthJson->to_uint();
        if (!length)
          return std::unexpected(std::move(length.error()));
        auto salt = secure_random_bytes(*length);
        if (!salt)
          return std::unexpected(std::move(salt.error()));
        if (auto logged = m_root.logger().info("UniverseServer: Sending Handshake Challenge"); !logged)
          return std::unexpected(std::move(logged.error()));
        if (auto pushed = connection.pushSingle(std::make_shared<HandshakeChallengePacket>(*salt)); !pushed)
          return std::unexpected(std::move(pushed.error()));
        if (auto sent = connection.sendAll(clientWaitLimit); !sent)
          return std::unexpected(std::move(sent.error()));
        if (auto received = connection.receiveAny(clientWaitLimit); !received)
          return std::unexpected(std::move(received.error()));
        auto handshakePacket = connection.pullSingle();
        if (!handshakePacket)
          return std::unexpected(std::move(handshakePacket.error()));
        auto handshake = as<HandshakeResponsePacket>(*handshakePacket);
        if (!handshake)
          return fail("Expected HandshakeResponsePacket.");
        auto users = configuration->get("serverUsers");
        if (!users)
          return std::unexpected(std::move(users.error()));
        auto account = users->get(connect->account, {});
        if (!account)
          return std::unexpected(std::move(account.error()));
        bool success = false;
        if (*account) {
          auto admin = account->get_bool("admin", false);
          if (!admin)
            return std::unexpected(std::move(admin.error()));
          administrator = *admin;
          auto password = account->get_string("password");
          if (!password)
            return std::unexpected(std::move(password.error()));
          success = HandshakeResponsePacket::password_digest(*password, connect->account, *salt) == handshake->passHash;
        }
        if (!success)
          return fail(Star::format("No such account '{}' or incorrect password", connect->account));
      } else {
        auto anonymousJson = configuration->get("allowAnonymousConnections");
        if (!anonymousJson)
          return std::unexpected(std::move(anonymousJson.error()));
        auto anonymous = anonymousJson->to_bool();
        if (!anonymous)
          return std::unexpected(std::move(anonymous.error()));
        if (!*anonymous)
          return fail("Anonymous connections disallowed");
        auto adminJson = configuration->get("anonymousConnectionsAreAdmin");
        if (!adminJson)
          return std::unexpected(std::move(adminJson.error()));
        auto admin = adminJson->to_bool();
        if (!admin)
          return std::unexpected(std::move(admin.error()));
        administrator = *admin;
      }
      auto banned = isBannedUser(remoteAddress, connect->playerUuid);
      if (!banned)
        return std::unexpected(std::move(banned.error()));
      if (*banned)
        return fail("You are banned: " + **banned);
    }
    String connectionLog = Star::format("UniverseServer: Logged in account '{}' as player '{}' from address {}", accountString, connect->playerName, remote);
    if (connect->info) {
      auto brand = connect->info.get("brand", "custom");
      if (!brand)
        return std::unexpected(std::move(brand.error()));
      if (*brand) {
        auto text = brand->to_string();
        if (!text)
          return std::unexpected(std::move(text.error()));
        connectionLog += Star::format(" ({} client)", *text);
      }
      auto legacy = connect->info.get_bool("legacy", false);
      if (!legacy)
        return std::unexpected(std::move(legacy.error()));
      if (*legacy)
        peer_version = legacy_version;
    }
    NetCompatibilityRules rules(peer_version, administrator);
    socket->get().setNetRules(rules);
    if (auto logged = m_root.logger().log(LogLevel::Info, connectionLog.utf8()); !logged)
      return std::unexpected(std::move(logged.error()));
    std::unique_lock clientsLocker(m_clientsLock);
    if (auto clash = getClientForUuid(connect->playerUuid)) {
      if (!administrator)
        return fail("Duplicate player UUID");
      clientsLocker.unlock();
      auto disconnected = doDisconnection(*clash, "Duplicate UUID joined and is Administrator so has priority.");
      clientsLocker.lock();
      if (!disconnected)
        return std::unexpected(std::move(disconnected.error()));
    }
    if (m_clients.size() + 1 > m_maxPlayers && !administrator)
      return fail("Max player connections");
    auto next_id = m_clients.next_id();
    if (!next_id)
      return std::unexpected(std::move(next_id.error()));
    auto clientId = *next_id;
    StringSet species;
    for (auto const& entry : m_speciesShips)
      species.add(entry.first);
    auto created = ServerClientContext::create(m_root, clientId, remoteAddress, rules, connect->playerUuid,
        connect->playerName, connect->shipSpecies, administrator, std::move(connect->shipChunks), std::move(species), connect->account);
    if (!created)
      return std::unexpected(std::move(created.error()));
    auto client = std::move(*created);
    if (auto handlers = client->register_rpc_handlers(m_teamManager->authenticatedRpcHandlers(client->player_uuid())); !handlers)
      return std::unexpected(std::move(handlers.error()));
    auto path = File::relative_to(m_storageDirectory, Star::format("{}.clientcontext", connect->playerUuid.hex()));
    auto exists = File::is_regular_file(path);
    if (!exists)
      return std::unexpected(std::move(exists.error()));
    if (*exists) {
      auto loaded = [&]() -> std::expected<void, StarException> {
        auto versioned = VersionedJson::read_file(path);
        if (!versioned)
          return std::unexpected(std::move(versioned.error()));
        auto store = database->loadVersionedJson(*versioned, "ClientContext");
        if (!store)
          return std::unexpected(std::move(store.error()));
        return client->load_server_data(*store);
      }();
      if (!loaded) {
        if (auto logged = m_root.logger().error_exception(
              Star::format("UniverseServer: Could not load client context file for <User: {}>, ignoring",
                           connect->playerName),
              ExceptionDiagnostic(loaded.error()));
            !logged)
          return std::unexpected(std::move(logged.error()));
        if (auto renamed = File::rename(path, Star::format("{}.{}.fail", path, Time::milliseconds_since_epoch())); !renamed)
          return std::unexpected(std::move(renamed.error()));
      }
    }
    if (!administrator)
      client->set_admin(false);
    client->set_ship_upgrades(std::move(connect->ship_upgrades));
    if (auto added = m_connectionServer->addConnection(clientId, std::move(connection)); !added)
      return std::unexpected(std::move(added.error()));
    registered = clientId;
    if (auto sent = m_connectionServer->sendPackets(clientId, {std::make_shared<ConnectSuccessPacket>(clientId, m_universeSettings->uuid(), m_celestial_database->base_information()), std::make_shared<UniverseTimeUpdatePacket>(m_universeClock->time()), std::make_shared<PausePacket>(*m_pause, m_root.timing().time_scale)}); !sent)
      return std::unexpected(std::move(sent.error()));
    if (auto added = m_clients.try_emplace(clientId, client); !added)
      return std::unexpected(std::move(added.error()));
    if (auto joined = m_chat_processor.connect_client(clientId, connect->playerName); !joined)
      return std::unexpected(std::move(joined.error()));
    clientsLocker.unlock();
    if (auto pvp = setPvp(clientId, false); !pvp)
      return std::unexpected(std::move(pvp.error()));
    auto location = client->ship_coordinate().location();
    if (location != Vec3I()) {
      auto placed = [&]() -> std::expected<void, StarException> {
        auto system = createSystemWorld(location);
        if (!system)
          return std::unexpected(std::move(system.error()));
        if (auto added = (*system)->addClient(clientId, client->player_uuid(), client->ship_speed(), client->ship_location()); !added)
          return std::unexpected(std::move(added.error()));
        if (auto requested = add_celestial_requests(clientId, {make_left(location.vec2()), make_right(location)}); !requested) {
          auto removed = (*system)->removeClient(clientId);
          if (!removed)
            return std::unexpected(requested.error().with_related(removed.error(), "Removing failed system client failed"));
          return std::unexpected(std::move(requested.error()));
        }
        client->set_system_world(*system);
        return {};
      }();
      if (!placed) {
        if (auto logged = m_root.logger().error_exception(
              Star::format("Failed to place client ship at {}, resetting coordinate", client->ship_coordinate()),
              ExceptionDiagnostic(placed.error()));
            !logged)
          return std::unexpected(std::move(logged.error()));
        client->set_ship_coordinate({});
      }
    }
    auto intro = assets->json("/universe_server.config:introInstance");
    if (!intro)
      return std::unexpected(std::move(intro.error()));
    auto fallback = intro->get_string("default", "");
    if (!fallback)
      return std::unexpected(std::move(fallback.error()));
    auto speciesIntro = intro->get_string(connect->shipSpecies, *fallback);
    if (!speciesIntro)
      return std::unexpected(std::move(speciesIntro.error()));
    if (!speciesIntro->empty() && !connect->introComplete) {
      if (auto logged = m_root.logger().info("UniverseServer: Spawning player in intro instance {}", *speciesIntro); !logged)
        return std::unexpected(std::move(logged.error()));
      if (auto warped = clientWarpPlayer(clientId, WarpToWorld{InstanceWorldId(*speciesIntro, client->player_uuid()), {}}, false); !warped)
        return std::unexpected(std::move(warped.error()));
    } else if (auto revive = client->player_revive_warp()) {
      bool useRevive = true;
      if (std::holds_alternative<InstanceWorldId>(revive.world)) {
        auto instances = assets->json("/instance_worlds.config");
        if (!instances)
          return std::unexpected(std::move(instances.error()));
        auto config = instances->opt(std::get<InstanceWorldId>(revive.world).instance);
        if (!config)
          return std::unexpected(std::move(config.error()));
        if (!*config)
          useRevive = false;
        else {
          auto persistent = (*config)->get_bool("persistent", false);
          if (!persistent)
            return std::unexpected(std::move(persistent.error()));
          useRevive = *persistent;
        }
      }
      if ((std::holds_alternative<ClientShipWorldId>(revive.world) && std::get<ClientShipWorldId>(revive.world) != connect->playerUuid)
          || std::holds_alternative<ClientCustomWorldId>(revive.world))
        useRevive = false;
      if (useRevive) {
        auto name = Star::print_world_id(revive.world);
        if (!name)
          return std::unexpected(std::move(name.error()));
        if (auto logged = m_root.logger().info("UniverseServer: Reviving player at {}", *name); !logged)
          return std::unexpected(std::move(logged.error()));
        if (auto warped = clientWarpPlayer(clientId, revive, false); !warped)
          return std::unexpected(std::move(warped.error()));
      } else {
        if (auto logged = m_root.logger().info("UniverseServer: Player revive position is expired, spawning back at own ship"); !logged)
          return std::unexpected(std::move(logged.error()));
        if (auto warped = clientWarpPlayer(clientId, WarpAlias::OwnShip, false); !warped)
          return std::unexpected(std::move(warped.error()));
      }
    } else {
      auto config = assets->json("/universe_server.config");
      if (!config)
        return std::unexpected(std::move(config.error()));
      auto defaultWarp = config->opt_string("defaultReviveWarp");
      if (!defaultWarp)
        return std::unexpected(std::move(defaultWarp.error()));
      if (*defaultWarp) {
        if (auto logged = m_root.logger().info("UniverseServer: Spawning player at default warp"); !logged)
          return std::unexpected(std::move(logged.error()));
        auto warp = parseWarpAction(**defaultWarp);
        if (!warp)
          return std::unexpected(std::move(warp.error()));
        if (auto warped = clientWarpPlayer(clientId, *warp, false); !warped)
          return std::unexpected(std::move(warped.error()));
      } else {
        if (auto logged = m_root.logger().info("UniverseServer: Spawning player at ship"); !logged)
          return std::unexpected(std::move(logged.error()));
        if (auto warped = clientWarpPlayer(clientId, WarpAlias::OwnShip, false); !warped)
          return std::unexpected(std::move(warped.error()));
      }
    }
    if (auto flown = clientFlyShip(clientId, client->ship_coordinate().location(), client->ship_location(), {}); !flown)
      return std::unexpected(std::move(flown.error()));
    if (auto logged = m_root.logger().info("UniverseServer: Client {} connected", client->descriptive_name()); !logged)
      return std::unexpected(std::move(logged.error()));
    std::shared_lock clientsReadGuard(m_clientsLock);
    auto players = static_cast<std::uint16_t>(m_clients.size());
    auto clients = m_clients.keys();
    clientsReadGuard.unlock();
    for (auto id : clients) {
      if (auto sent = m_connectionServer->sendPackets(id, {std::make_shared<ServerInfoPacket>(players, static_cast<std::uint16_t>(m_maxPlayers))}); !sent)
        return std::unexpected(std::move(sent.error()));
    }
    for (auto&& script : m_scriptContexts) {
      if (auto invoked = recover_lua_script_error(script.second->invoke("acceptConnection", clientId)); !invoked)
        return std::unexpected(std::move(invoked.error()));
    }
    return {};
  }();
  if (!accepted && registered) {
    if (isConnectedClient(*registered)) {
      if (auto disconnected = doDisconnection(*registered, "Connection initialization failed"); !disconnected)
        return std::unexpected(accepted.error().with_related(disconnected.error(), "Connection initialization rollback failed"));
    } else {
      auto removed = m_connectionServer->removeConnection(*registered);
      if (!removed)
        return std::unexpected(accepted.error().with_related(removed.error(), "Removing failed connection failed"));
      std::unique_lock locker(m_mainLock, std::defer_lock);
      lock_with_diagnostics(locker);
      m_deadConnections.push_back({std::move(*removed),
                                   std::chrono::time_point_cast<std::chrono::milliseconds>(
                                     std::chrono::steady_clock::now() + std::chrono::microseconds(500))});
    }
  }
  return accepted;
}

std::expected<WarpToWorld, StarException> UniverseServer::resolveWarpAction(WarpAction warpAction, ConnectionId clientId, bool deploy) const {
  auto client_context = m_clients.value(clientId);
  if (!client_context)
    return {};

  WorldId toWorldId;
  SpawnTarget spawnTarget;

  if (std::holds_alternative<WarpToWorld>(warpAction)) {
    auto const& toWorld = std::get<WarpToWorld>(warpAction);
    if (std::holds_alternative<std::monostate>(toWorld.world))
      toWorldId = client_context->player_world_id();
    else
      toWorldId = toWorld.world;
    spawnTarget = toWorld.target;
  } else if (std::holds_alternative<WarpToPlayer>(warpAction)) {
    if (auto toClientId = getClientForUuid(std::get<WarpToPlayer>(warpAction))) {
      auto target = m_clients.get(*toClientId);
      if (!target)
        return std::unexpected(std::move(target.error()));
      if (auto toClientWorld = target->get()->player_world()) {
        auto position = toClientWorld->playerRevivePosition(*toClientId);
        if (!position)
          return std::unexpected(std::move(position.error()));
        if (auto toClientPosition = *position) {
          toWorldId = toClientWorld->worldId();
          if (deploy)
            spawnTarget.emplace<0>();
          else
            spawnTarget = SpawnTargetPosition(*toClientPosition);
        }
      }
    }
  } else if (std::holds_alternative<WarpAlias>(warpAction)) {
    auto shortcut = std::get<WarpAlias>(warpAction);
    if (shortcut == WarpAlias::Return) {
      if (auto returnWarp = client_context->player_return_warp()) {
        toWorldId = returnWarp.world;
        spawnTarget = returnWarp.target;
      }
    } else if (shortcut == WarpAlias::OrbitedWorld) {
      if (auto warpAction = client_context->orbit_warp_action()) {
        if (std::holds_alternative<WarpToWorld>(warpAction->first)) {
          auto const& warpToWorld = std::get<WarpToWorld>(warpAction->first);
          toWorldId = warpToWorld.world;
          spawnTarget = warpToWorld.target;
        }
      }
    } else if (shortcut == WarpAlias::OwnShip) {
      toWorldId = ClientShipWorldId(client_context->player_uuid());
    }
  }

  if (std::holds_alternative<ClientShipWorldId>(toWorldId)) {
    if (m_secureWarps) {
      auto allowed = canWarpToShip(clientId, std::get<ClientShipWorldId>(toWorldId));
      if (!allowed)
        return std::unexpected(std::move(allowed.error()));
      if (!*allowed)
        return WarpToWorld{};
    }
  }

  return WarpToWorld(toWorldId, spawnTarget);
}

std::expected<bool, StarException> UniverseServer::canWarpToShip(ConnectionId clientId, Uuid const& targetShipUuid) const {
  auto client_context = m_clients.value(clientId);
  if (!client_context)
    return false;

  Uuid callerUuid = client_context->player_uuid();

  if (targetShipUuid == callerUuid || client_context->is_admin())
    return true;

  auto callerTeam = m_teamManager->getTeam(callerUuid);
  if (!callerTeam)
    return false;

  if (auto ownerTeam = m_teamManager->getTeam(targetShipUuid)) {
    if (*ownerTeam == *callerTeam)
      return true;
  }

  for (auto const& otherClient : m_clients) {
    if (otherClient.first == clientId)
      continue;
    auto otherTeam = m_teamManager->getTeam(otherClient.second->player_uuid());
    if (!otherTeam || *otherTeam != *callerTeam)
      continue;
    WorldId otherWorldId = otherClient.second->player_world_id();
    if (std::holds_alternative<ClientShipWorldId>(otherWorldId)) {
      if (std::get<ClientShipWorldId>(otherWorldId) == targetShipUuid)
        return true;
    }
  }

  if (auto logged = m_root.logger().warn("UniverseServer: Rejected ship warp from {} to ship {} - not in team and no teammate present",
      callerUuid.hex(), targetShipUuid.hex()); !logged)
    return std::unexpected(std::move(logged.error()));
  return false;
}

std::expected<void, StarException> UniverseServer::doDisconnection(ConnectionId clientId, String const& reason) {
  std::unique_lock locker(m_mainLock, std::defer_lock);
  lock_with_diagnostics(locker);
  std::unique_lock clientsLocker(m_clientsLock);
  auto client = m_clients.value(clientId);
  if (!client)
    return {};
  std::optional<StarException> error;
  auto record = [&](auto&& result) {
    if (result)
      return true;
    if (error)
      *error = error->with_related(result.error(), "Additional client disconnection failure");
    else
      error = std::move(result.error());
    return false;
  };
  if (m_disconnectingClients.add(clientId)) {
    for (auto&& script : m_scriptContexts)
      record(recover_lua_script_error(script.second->invoke("doDisconnection", clientId)));
    m_teamManager->playerDisconnected(client->player_uuid());
  }
  clientsLocker.unlock();
  {
    std::unique_lock messageLocker(m_messageLock, std::defer_lock);
    lock_with_diagnostics(messageLocker);
    for (auto it = m_universeMessageResponses.begin(); it != m_universeMessageResponses.end();) {
      auto origin = std::holds_alternative<ConnectionId>(it->second.second)
          ? std::optional{std::get<ConnectionId>(it->second.second)} : std::nullopt;
      if (it->second.first != clientId) {
        if (origin && *origin == clientId)
          it = m_universeMessageResponses.erase(it);
        else
          ++it;
        continue;
      }
      if (std::holds_alternative<RpcPromiseKeeper<Json>>(it->second.second))
        record(std::get<RpcPromiseKeeper<Json>>(it->second.second).fail("Client disconnected"));
      else if (origin && m_connectionServer->hasConnection(*origin))
        record(m_connectionServer->sendPackets(
          *origin,
          {std::make_shared<UniverseMessageResponse>(std::unexpected(String("Client disconnected")), it->first)}));
      it = m_universeMessageResponses.erase(it);
    }
  }
  bool departed = true;
  auto revive = client->player_revive_warp();
  if (!revive)
    revive = WarpToWorld(ClientShipWorldId(client->player_uuid()));
  if (auto world = client->player_world()) {
    locker.unlock();
    auto position = world->playerRevivePosition(clientId);
    if (record(position) && *position)
      revive = WarpToWorld(world->worldId(), SpawnTargetPosition(**position));
    auto packets = world->removeClient(clientId);
    if (record(packets)) {
      if (m_connectionServer->hasConnection(clientId))
        record(m_connectionServer->sendPackets(clientId, std::move(*packets)));
      client->clear_player_world();
    } else {
      departed = false;
      auto finalPackets = world->pullOutgoingPackets(clientId);
      if (!finalPackets.empty() && m_connectionServer->hasConnection(clientId))
        record(m_connectionServer->sendPackets(clientId, std::move(finalPackets)));
    }
    auto name = Star::print_world_id(world->worldId());
    if (record(name))
      m_chat_processor.leave_channel(clientId, *name);
    lock_with_diagnostics(locker);
  }
  for (auto const& subWorld : client->sub_worlds()) {
    auto world = client->sub_world(subWorld);
    locker.unlock();
    auto packets = world->removeClient(main_to_sub_world_connection_id(clientId));
    bool removed = record(packets);
    if (removed && m_connectionServer->hasConnection(clientId))
      record(m_connectionServer->sendPackets(clientId, std::move(*packets)));
    if (!removed) {
      auto finalPackets = world->pullOutgoingPackets(main_to_sub_world_connection_id(clientId));
      if (!finalPackets.empty() && m_connectionServer->hasConnection(clientId))
        record(m_connectionServer->sendPackets(clientId, std::move(finalPackets)));
    }
    lock_with_diagnostics(locker);
    if (removed)
      client->clear_sub_world(subWorld);
    else
      departed = false;
  }
  client->set_player_revive_warp(revive);
  if (auto system = client->system_world()) {
    if (record(system->removeClient(clientId)))
      client->clear_system_world();
    else
      departed = false;
  }
  record(client->fail_world_requests());
  if (m_chat_processor.has_client(clientId))
    record(m_chat_processor.disconnect_client(clientId));
  bool registered = m_connectionServer->hasConnection(clientId);
  if (registered) {
    auto open = m_connectionServer->connectionIsOpen(clientId);
    if (record(open) && *open) {
      auto ship = getWorld(ClientShipWorldId(client->player_uuid()));
      if (record(ship) && *ship) {
        locker.unlock();
        record((*ship)->unloadAll(true));
        auto chunks = (*ship)->readChunks();
        if (record(chunks))
          client->update_ship_chunks(std::move(*chunks));
        record((*ship)->stop());
        lock_with_diagnostics(locker);
      }
      for (auto const& name : client->custom_worlds()) {
        auto world = getWorld(ClientCustomWorldId(client->player_uuid(), name));
        if (record(world) && *world) {
          locker.unlock();
          record((*world)->unloadAll(true));
          auto chunks = (*world)->readChunks();
          if (record(chunks))
            client->update_custom_world_chunks(name, std::move(*chunks));
          client->set_custom_world_active(name, false);
          record((*world)->stop());
          lock_with_diagnostics(locker);
        }
      }
      if (!record(send_client_context_update(*client)))
        departed = false;
      // World final packets and context always precede the disconnect packet.
      if (departed)
        record(m_connectionServer->sendPackets(clientId, {std::make_shared<ServerDisconnectPacket>(reason)}));
    }
  }
  auto saved = [&]() -> std::expected<void, StarException> {
    auto database = m_root.versioningDatabase();
    if (!database)
      return std::unexpected(std::move(database.error()));
    auto store = client->store_server_data();
    if (!store)
      return std::unexpected(std::move(store.error()));
    auto versioned = (*database)->makeCurrentVersionedJson("ClientContext", *store);
    if (!versioned)
      return std::unexpected(std::move(versioned.error()));
    return VersionedJson::write_file(
      *versioned, File::relative_to(m_storageDirectory, Star::format("{}.clientcontext", client->player_uuid().hex())));
  }();
  bool persisted = record(saved);
  clientsLocker.lock();
  if (persisted && departed) {
    m_clients.remove(clientId);
    m_disconnectingClients.remove(clientId);
  } else {
    m_pendingDisconnections[clientId] = reason;
  }
  if (registered && departed) {
    auto removed = m_connectionServer->removeConnection(clientId);
    if (record(removed))
      m_deadConnections.push_back({std::move(*removed),
                                   std::chrono::time_point_cast<std::chrono::milliseconds>(
                                     std::chrono::steady_clock::now() + std::chrono::microseconds(500))});
  }
  if (persisted && departed)
    record(m_root.logger().info("UniverseServer: Client {} disconnected for reason: {}", client->descriptive_name(), reason));
  auto players = static_cast<std::uint16_t>(m_clients.size());
  for (auto id : m_clients | std::views::keys) {
    if (m_connectionServer->hasConnection(id))
      record(m_connectionServer->sendPackets(id, {std::make_shared<ServerInfoPacket>(players, static_cast<std::uint16_t>(m_maxPlayers))}));
  }
  clientsLocker.unlock();
  if (error)
    return std::unexpected(std::move(*error));
  return {};
}

std::optional<ConnectionId> UniverseServer::getClientForUuid(Uuid const& uuid) const {
  for (auto const& p : m_clients) {
    if (p.second->player_uuid() == uuid)
      return p.second->client_id();
  }

  return {};
}

std::expected<std::shared_ptr<WorldServerThread>, StarException> UniverseServer::getWorld(WorldId const& worldId) {
  auto position = m_worlds.find(worldId);
  if (position == m_worlds.end() || !position->second)
    return nullptr;
  auto& promise = position->second;
  auto result = [&]() -> std::expected<std::shared_ptr<WorldServerThread>, StarException> {
    auto ready = promise->poll();
    if (!ready)
      return std::unexpected(std::move(ready.error()));
    if (!*ready)
      return nullptr;
    return promise->get();
  }();
  if (result)
    return result;
  if (auto retired = retireFailedWorldCreation(worldId, promise, result.error()); !retired)
    return std::unexpected(std::move(retired.error()));
  return nullptr;
}

std::expected<std::shared_ptr<WorldServerThread>, StarException> UniverseServer::createWorld(WorldId const& worldId) {
  if (!m_worlds.contains(worldId)) {
    auto promise = makeWorldPromise(worldId);
    if (!promise)
      return std::unexpected(std::move(promise.error()));
    if (!*promise)
      return nullptr;
    if (auto added = m_worlds.add(worldId, std::move(*promise)); !added)
      return std::unexpected(std::move(added.error()));
  }
  auto entry = m_worlds.get(worldId);
  if (!entry)
    return std::unexpected(std::move(entry.error()));
  auto& promise = entry->get();
  if (!promise)
    return nullptr;
  auto result = promise->get();
  if (result)
    return result;
  if (auto retired = retireFailedWorldCreation(worldId, promise, result.error()); !retired)
    return std::unexpected(std::move(retired.error()));
  return nullptr;
}

std::expected<std::optional<std::shared_ptr<WorldServerThread>>, StarException> UniverseServer::triggerWorldCreation(WorldId const& worldId) {
  if (!m_worlds.contains(worldId)) {
    auto promise = makeWorldPromise(worldId);
    if (!promise)
      return std::unexpected(std::move(promise.error()));
    if (!*promise)
      return std::optional<std::shared_ptr<WorldServerThread>>(nullptr);
    if (auto added = m_worlds.add(worldId, std::move(*promise)); !added)
      return std::unexpected(std::move(added.error()));
    return std::nullopt;
  }
  auto entry = m_worlds.get(worldId);
  if (!entry)
    return std::unexpected(std::move(entry.error()));
  auto& promise = entry->get();
  if (!promise)
    return std::optional<std::shared_ptr<WorldServerThread>>(nullptr);
  auto result = [&]() -> std::expected<std::optional<std::shared_ptr<WorldServerThread>>, StarException> {
    auto ready = promise->poll();
    if (!ready)
      return std::unexpected(std::move(ready.error()));
    if (!*ready)
      return std::nullopt;
    auto world = promise->get();
    if (!world)
      return std::unexpected(std::move(world.error()));
    return std::optional<std::shared_ptr<WorldServerThread>>(std::move(*world));
  }();
  if (result)
    return result;
  if (auto retired = retireFailedWorldCreation(worldId, promise, result.error()); !retired)
    return std::unexpected(std::move(retired.error()));
  return std::optional<std::shared_ptr<WorldServerThread>>(nullptr);
}

std::expected<void, StarException> UniverseServer::retireFailedWorldCreation(
    WorldId const& worldId, std::optional<WorldServerPromise>& promise, StarException const& error) {
  promise.reset();
  if (auto logged =
        m_root.logger().error_exception("UniverseServer: error during world create", ExceptionDiagnostic(error));
      !logged)
    return std::unexpected(logged.error().with_related(error, "Original world creation failure"));
  if (auto died = worldDiedWithError(worldId); !died)
    return std::unexpected(died.error().with_related(error, "Original world creation failure"));
  return {};
}

std::expected<std::optional<UniverseServer::WorldServerPromise>, StarException> UniverseServer::makeWorldPromise(WorldId const& worldId) {
  if (std::holds_alternative<CelestialWorldId>(worldId))
    return celestial_world_promise(std::get<CelestialWorldId>(worldId));
  else if (std::holds_alternative<ClientShipWorldId>(worldId))
    return shipWorldPromise(std::get<ClientShipWorldId>(worldId));
  else if (std::holds_alternative<InstanceWorldId>(worldId))
    return instanceWorldPromise(std::get<InstanceWorldId>(worldId));
  else if (std::holds_alternative<CustomWorldId>(worldId))
    return customWorldPromise(std::get<CustomWorldId>(worldId));
  else if (std::holds_alternative<ClientCustomWorldId>(worldId))
    return clientCustomWorldPromise(std::get<ClientCustomWorldId>(worldId));
  else
    return {};
}

std::expected<std::optional<UniverseServer::WorldServerPromise>, StarException> UniverseServer::shipWorldPromise(ClientShipWorldId const& id) {
  auto clientId = clientForUuid(id);
  if (!clientId)
    return std::nullopt;
  auto context = m_clients.get(*clientId);
  if (!context)
    return std::unexpected(std::move(context.error()));
  auto client = context->get();
  return WorldServerPromise(m_workerPool.add_producer<std::shared_ptr<WorldServerThread>>([&universe = *this, &m_root = m_root, &m_universeSettings = m_universeSettings, &m_pause = m_pause, id, client, speciesShips = m_speciesShips]() -> std::expected<std::shared_ptr<WorldServerThread>, StarException> {
    std::shared_ptr<WorldServer> ship;
    auto chunks = client->ship_chunks();
    if (!chunks.empty()) {
      auto loaded = [&]() -> std::expected<std::shared_ptr<WorldServer>, StarException> {
        if (auto logged = m_root.logger().info("UniverseServer: Loading client ship world {}", id); !logged)
          return std::unexpected(std::move(logged.error()));
        return WorldServer::create(m_root, chunks);
      }();
      if (loaded)
        ship = std::move(*loaded);
      else if (auto logged = m_root.logger().error_exception(
                 Star::format("UniverseServer: Could not load client ship {}, resetting ship to default state", id),
                 ExceptionDiagnostic(loaded.error()));
               !logged)
        return std::unexpected(std::move(logged.error()));
    }
    if (!ship) {
      if (auto logged = m_root.logger().info("UniverseServer: Creating new client ship world {}", id); !logged)
        return std::unexpected(std::move(logged.error()));
      auto species = client->ship_species();
      auto definitions = speciesShips.get(species);
      if (!definitions)
        return std::unexpected(UniverseServerException(Star::format("UniverseServer: cannot create ship world {} for unknown ship species '{}'", id, species)));
      auto first = definitions->get().first();
      if (!first)
        return std::unexpected(std::move(first.error()));
      auto structure = WorldStructure::create(m_root, first->get());
      if (!structure)
        return std::unexpected(std::move(structure.error()));
      Vec2U size(2048, 2048);
      auto configuredSize = structure->configValue("worldSize");
      if (!configuredSize)
        return std::unexpected(std::move(configuredSize.error()));
      if (*configuredSize) {
        auto parsed = json_to_vec2u(*configuredSize);
        if (!parsed)
          return std::unexpected(std::move(parsed.error()));
        size = *parsed;
      }
      auto storage = File::ephemeral_file(m_root.random());
      if (!storage)
        return std::unexpected(std::move(storage.error()));
      auto created = WorldServer::create(m_root, size, *storage);
      if (!created)
        return std::unexpected(std::move(created.error()));
      ship = std::move(*created);
      auto central = ship->setCentralStructure(std::move(*structure));
      if (!central)
        return std::unexpected(std::move(central.error()));
      auto upgrades = client->ship_upgrades();
      auto assets = m_root.assets();
      if (!assets)
        return std::unexpected(std::move(assets.error()));
      auto base = (*assets)->json("/ships/shipupgrades.config");
      if (!base)
        return std::unexpected(std::move(base.error()));
      if (auto applied = upgrades.apply(*base); !applied)
        return std::unexpected(std::move(applied.error()));
      auto configured = central->configValue("shipUpgrades");
      if (!configured)
        return std::unexpected(std::move(configured.error()));
      if (auto applied = upgrades.apply(*configured); !applied)
        return std::unexpected(std::move(applied.error()));
      client->set_ship_upgrades(upgrades);
      ship->setSpawningEnabled(false);
      if (auto set = ship->setProperty("invinciblePlayers", true); !set)
        return std::unexpected(std::move(set.error()));
      if (auto set = ship->setProperty("ship.level", 0); !set)
        return std::unexpected(std::move(set.error()));
      if (auto set = ship->setProperty("ship.species", species); !set)
        return std::unexpected(std::move(set.error()));
      if (auto set = ship->setProperty("ship.fuel", 0); !set)
        return std::unexpected(std::move(set.error()));
      if (auto set = ship->setProperty("ship.maxFuel", upgrades.max_fuel); !set)
        return std::unexpected(std::move(set.error()));
      if (auto set = ship->setProperty("ship.crewSize", upgrades.crew_size); !set)
        return std::unexpected(std::move(set.error()));
      if (auto set = ship->setProperty("ship.fuelEfficiency", upgrades.fuel_efficiency); !set)
        return std::unexpected(std::move(set.error()));
      if (auto set = ship->setProperty("ship.epoch", Time::time_since_epoch()); !set)
        return std::unexpected(std::move(set.error()));
    }
    auto clock = std::make_shared<Clock>();
    auto epoch = ship->getProperty("ship.epoch");
    if (!epoch)
      return std::unexpected(std::move(epoch.error()));
    if (!epoch->can_convert(Json::Type::Float)) {
      if (auto set = ship->setProperty("ship.epoch", Time::time_since_epoch()); !set)
        return std::unexpected(std::move(set.error()));
    } else {
      auto time = epoch->to_double();
      if (!time)
        return std::unexpected(std::move(time.error()));
      clock->set_time(Time::time_since_epoch() - *time);
    }
    ship->setUniverseSettings(m_universeSettings);
    ship->setReferenceClock(clock);
    clock->start();
    auto system = client->system_world();
    auto sky = system ? system->clientSkyParameters(client->client_id()) : universe.celestial_sky_parameters(client->ship_coordinate());
    if (!sky)
      return std::unexpected(std::move(sky.error()));
    if (auto set = ship->setOrbitalSky(*sky); !set)
      return std::unexpected(std::move(set.error()));
    if (auto lua = ship->initLua(universe); !lua)
      return std::unexpected(std::move(lua.error()));
    auto thread = WorldServerThread::create(ship, ClientShipWorldId(id));
    if (!thread)
      return std::unexpected(std::move(thread.error()));
    (*thread)->setPause(m_pause);
    auto initial = (*thread)->readChunks();
    if (!initial)
      return std::unexpected(std::move(initial.error()));
    client->update_ship_chunks(std::move(*initial));
    return universe.startWorldThread(std::move(*thread), id);
  }));
}

std::expected<std::optional<UniverseServer::WorldServerPromise>, StarException> UniverseServer::celestial_world_promise(CelestialWorldId const& id) {
  if (!id)
    return std::nullopt;
  return WorldServerPromise(m_workerPool.add_producer<std::shared_ptr<WorldServerThread>>([&universe = *this, &m_root = m_root, &m_universeSettings = m_universeSettings, &m_pause = m_pause, id, directory = m_storageDirectory, database = m_celestial_database, clock = m_universeClock]() -> std::expected<std::shared_ptr<WorldServerThread>, StarException> {
    std::shared_ptr<WorldServer> world;
    auto path = File::relative_to(directory, Star::format("{}.world", id.filename()));
    auto exists = File::is_regular_file(path);
    if (!exists)
      return std::unexpected(std::move(exists.error()));
    if (*exists) {
      auto loaded = [&]() -> std::expected<std::shared_ptr<WorldServer>, StarException> {
        if (auto logged = m_root.logger().info("UniverseServer: Loading celestial world {}", id); !logged)
          return std::unexpected(std::move(logged.error()));
        auto file = File::open(path, IOMode::ReadWrite);
        if (!file)
          return std::unexpected(std::move(file.error()));
        return WorldServer::create(m_root, *file);
      }();
      if (loaded)
        world = std::move(*loaded);
      else {
        if (auto logged = m_root.logger().error_exception(
              Star::format("UniverseServer: Could not load celestial world {}, removing", id),
              ExceptionDiagnostic(loaded.error()));
            !logged)
          return std::unexpected(std::move(logged.error()));
        if (auto renamed = File::rename(path, Star::format("{}.{}.fail", path, Time::milliseconds_since_epoch())); !renamed)
          return std::unexpected(std::move(renamed.error()));
      }
    }
    if (!world) {
      if (auto logged = m_root.logger().info("UniverseServer: Creating celestial world {}", id); !logged)
        return std::unexpected(std::move(logged.error()));
      auto worldTemplate = WorldTemplate::create(m_root, id, database);
      if (!worldTemplate)
        return std::unexpected(std::move(worldTemplate.error()));
      auto file = File::open(path, IOMode::ReadWrite | IOMode::Truncate);
      if (!file)
        return std::unexpected(std::move(file.error()));
      auto created = WorldServer::create(m_root, *worldTemplate, *file);
      if (!created)
        return std::unexpected(std::move(created.error()));
      world = std::move(*created);
      if (auto persistent = database->mark_persistent(id); !persistent)
        return std::unexpected(std::move(persistent.error()));
    }
    world->setUniverseSettings(m_universeSettings);
    world->setReferenceClock(clock);
    if (auto lua = world->initLua(universe); !lua)
      return std::unexpected(std::move(lua.error()));
    auto thread = WorldServerThread::create(world, id);
    if (!thread)
      return std::unexpected(std::move(thread.error()));
    (*thread)->setPause(m_pause);
    return universe.startWorldThread(std::move(*thread), id);
  }));
}

std::expected<std::optional<UniverseServer::WorldServerPromise>, StarException> UniverseServer::instanceWorldPromise(InstanceWorldId const& id) {
  return WorldServerPromise(m_workerPool.add_producer<std::shared_ptr<WorldServerThread>>([&universe = *this, &m_root = m_root, &m_universeSettings = m_universeSettings, &m_pause = m_pause, &m_tempWorldIndex = m_tempWorldIndex, &m_universeClock = m_universeClock, id, directory = m_storageDirectory, clock = m_universeClock]() -> std::expected<std::shared_ptr<WorldServerThread>, StarException> {
    auto assets = m_root.assets();
    if (!assets)
      return std::unexpected(std::move(assets.error()));
    auto instances = (*assets)->json("/instance_worlds.config");
    if (!instances)
      return std::unexpected(std::move(instances.error()));
    auto config = instances->get(id.instance);
    if (!config)
      return std::unexpected(std::move(config.error()));
    auto hasSeed = config->contains("seed");
    if (!hasSeed)
      return std::unexpected(std::move(hasSeed.error()));
    std::uint64_t seed;
    if (*hasSeed) {
      auto configuredSeed = config->get_uint("seed");
      if (!configuredSeed)
        return std::unexpected(std::move(configuredSeed.error()));
      seed = *configuredSeed;
    } else {
      seed = m_root.random().next_u64();
    }
    auto type = config->get_string("type");
    if (!type)
      return std::unexpected(std::move(type.error()));
    std::shared_ptr<VisitableWorldParameters> parameters;
    auto terrestrial = type->equals_ignore_case("Terrestrial");
    if (!terrestrial)
      return std::unexpected(std::move(terrestrial.error()));
    if (*terrestrial) {
      auto planetType = config->get_string("planetType");
      if (!planetType)
        return std::unexpected(std::move(planetType.error()));
      auto size = config->get_string("planetSize");
      if (!size)
        return std::unexpected(std::move(size.error()));
      auto generated = generateTerrestrialWorldParameters(m_root, *planetType, *size, seed);
      if (!generated)
        return std::unexpected(std::move(generated.error()));
      parameters = std::move(*generated);
    } else {
      auto asteroids = type->equals_ignore_case("Asteroids");
      if (!asteroids)
        return std::unexpected(std::move(asteroids.error()));
      if (*asteroids) {
        auto generated = generateAsteroidsWorldParameters(m_root, seed);
        if (!generated)
          return std::unexpected(std::move(generated.error()));
        parameters = std::move(*generated);
      } else {
        auto floating = type->equals_ignore_case("FloatingDungeon");
        if (!floating)
          return std::unexpected(std::move(floating.error()));
        if (!*floating)
          return std::unexpected(UniverseServerException(Star::format("Unknown world type: '{}'\n", *type)));
        auto dungeon = config->get_string("dungeonWorld");
        if (!dungeon)
          return std::unexpected(std::move(dungeon.error()));
        auto generated = generateFloatingDungeonWorldParameters(m_root, *dungeon);
        if (!generated)
          return std::unexpected(std::move(generated.error()));
        parameters = std::move(*generated);
      }
    }
    if (id.level)
      parameters->threatLevel = *id.level;
    auto hasBeam = config->contains("beamUpRule");
    if (!hasBeam)
      return std::unexpected(std::move(hasBeam.error()));
    if (*hasBeam) {
      auto beam = config->get_string("beamUpRule");
      if (!beam)
        return std::unexpected(std::move(beam.error()));
      if (!BeamUpRuleNames)
        return std::unexpected(BeamUpRuleNames.error());
      auto rule = BeamUpRuleNames->get_left(*beam);
      if (!rule)
        return std::unexpected(std::move(rule.error()));
      parameters->beamUpRule = rule->get();
    }
    auto deathDrops = config->get_bool("disableDeathDrops", false);
    if (!deathDrops)
      return std::unexpected(std::move(deathDrops.error()));
    parameters->disableDeathDrops = *deathDrops;
    auto skyJson = config->get("skyParameters", Json());
    if (!skyJson)
      return std::unexpected(std::move(skyJson.error()));
    auto sky = SkyParameters::create(*skyJson);
    if (!sky)
      return std::unexpected(std::move(sky.error()));
    auto worldTemplate = WorldTemplate::create(m_root, parameters, *sky, seed);
    if (!worldTemplate)
      return std::unexpected(std::move(worldTemplate.error()));
    auto properties = config->get_object("worldProperties", {});
    if (!properties)
      return std::unexpected(std::move(properties.error()));
    auto spawning = config->get_bool("spawningEnabled", true);
    if (!spawning)
      return std::unexpected(std::move(spawning.error()));
    auto persistent = config->get_bool("persistent", false);
    if (!persistent)
      return std::unexpected(std::move(persistent.error()));
    auto useClock = config->get_bool("useUniverseClock", false);
    if (!useClock)
      return std::unexpected(std::move(useClock.error()));

    std::shared_ptr<WorldServer> world;
    bool existed = false;
    if (*persistent) {
      String identifier = id.instance;
      if (id.uuid)
        identifier = Star::format("{}-{}", identifier, id.uuid->hex());
      if (id.level)
        identifier = Star::format("{}-{}", identifier, *id.level);
      auto path = File::relative_to(directory, Star::format("unique-{}.world", identifier));
      auto exists = File::is_regular_file(path);
      if (!exists)
        return std::unexpected(std::move(exists.error()));
      if (*exists) {
        auto loaded = [&]() -> std::expected<std::shared_ptr<WorldServer>, StarException> {
          if (auto logged = m_root.logger().info("UniverseServer: Loading persistent unique instance world {}", id.instance); !logged)
            return std::unexpected(std::move(logged.error()));
          auto file = File::open(path, IOMode::ReadWrite);
          if (!file)
            return std::unexpected(std::move(file.error()));
          return WorldServer::create(m_root, *file);
        }();
        if (loaded) {
          world = std::move(*loaded);
          existed = true;
        } else {
          if (auto logged = m_root.logger().error_exception(
                Star::format("UniverseServer: Could not load persistent unique instance world {}, removing",
                             id.instance),
                ExceptionDiagnostic(loaded.error()));
              !logged)
            return std::unexpected(std::move(logged.error()));
          if (auto renamed = File::rename(path, Star::format("{}.{}.fail", path, Time::milliseconds_since_epoch())); !renamed)
            return std::unexpected(std::move(renamed.error()));
        }
      }
      if (!world) {
        if (auto logged = m_root.logger().info("UniverseServer: Creating persistent unique instance world {}", id.instance); !logged)
          return std::unexpected(std::move(logged.error()));
        auto file = File::open(path, IOMode::ReadWrite | IOMode::Truncate);
        if (!file)
          return std::unexpected(std::move(file.error()));
        auto created = WorldServer::create(m_root, *worldTemplate, *file);
        if (!created)
          return std::unexpected(std::move(created.error()));
        world = std::move(*created);
      }
    } else {
      auto path = universe.tempWorldFile(id);
      auto expiry = config->opt_int("tempWorldDeleteTime");
      if (!expiry)
        return std::unexpected(std::move(expiry.error()));
      std::uint64_t deleteTime = expiry->value_or(0);
      auto exists = File::is_regular_file(path);
      if (!exists)
        return std::unexpected(std::move(exists.error()));
      auto worldName = Star::print_world_id(id);
      if (!worldName)
        return std::unexpected(std::move(worldName.error()));
      if (*exists) {
        if (m_tempWorldIndex.contains(id)) {
          auto file = File::open(path, IOMode::ReadWrite);
          if (!file)
            return std::unexpected(std::move(file.error()));
          auto size = (*file)->size();
          if (!size)
            return std::unexpected(std::move(size.error()));
          if (*size > 0) {
            if (auto logged = m_root.logger().info("UniverseServer: Loading temporary instance world {} from storage", *worldName); !logged)
              return std::unexpected(std::move(logged.error()));
            auto loaded = WorldServer::create(m_root, *file);
            if (loaded) {
              world = std::move(*loaded);
              existed = true;
            } else if (auto logged = m_root.logger().error_exception(
                         Star::format("UniverseServer: Could not load temporary instance world '{}', re-creating",
                                      *worldName),
                         ExceptionDiagnostic(loaded.error()));
                       !logged) {
              return std::unexpected(std::move(logged.error()));
            }
          }
        } else if (auto removed = File::remove(path); !removed) {
          return std::unexpected(std::move(removed.error()));
        }
      }
      if (!world) {
        if (auto logged = m_root.logger().info("UniverseServer: Creating temporary instance world '{}' with expiry time {}", *worldName, deleteTime); !logged)
          return std::unexpected(std::move(logged.error()));
        auto file = File::open(path, IOMode::ReadWrite);
        if (!file)
          return std::unexpected(std::move(file.error()));
        auto created = WorldServer::create(m_root, *worldTemplate, *file);
        if (!created)
          return std::unexpected(std::move(created.error()));
        world = std::move(*created);
        m_tempWorldIndex.set(id, {m_universeClock->milliseconds(), deleteTime});
      }
    }
    world->setUniverseSettings(m_universeSettings);
    for (auto const& property : *properties) {
      if (auto set = world->setProperty(property.first, property.second); !set)
        return std::unexpected(std::move(set.error()));
    }
    if (auto set = world->setProperty("ephemeral", !*persistent); !set)
      return std::unexpected(std::move(set.error()));
    world->setSpawningEnabled(*spawning);
    if (*useClock)
      world->setReferenceClock(clock);
    if (!existed) {
      for (auto const& action : m_universeSettings->currentFlagActionsForInstanceWorld(id.instance)) {
        if (std::holds_alternative<PlaceDungeonFlagAction>(action)) {
          auto const& dungeon = std::get<PlaceDungeonFlagAction>(action);
          if (auto placed = world->placeDungeon(dungeon.dungeonId, dungeon.targetPosition, 0); !placed)
            return std::unexpected(std::move(placed.error()));
        }
      }
    }
    if (auto lua = world->initLua(universe); !lua)
      return std::unexpected(std::move(lua.error()));
    auto thread = WorldServerThread::create(world, id);
    if (!thread)
      return std::unexpected(std::move(thread.error()));
    (*thread)->setPause(m_pause);
    return universe.startWorldThread(std::move(*thread), id);
  }));
}

std::expected<std::optional<UniverseServer::WorldServerPromise>, StarException> UniverseServer::customWorldPromise(CustomWorldId const& id, std::optional<std::shared_ptr<WorldTemplate>> worldTemplate) {
  return WorldServerPromise(m_workerPool.add_producer<std::shared_ptr<WorldServerThread>>([&universe = *this, &m_root = m_root, &m_universeSettings = m_universeSettings, &m_pause = m_pause, id, worldTemplate, directory = m_storageDirectory, clock = m_universeClock]() -> std::expected<std::shared_ptr<WorldServerThread>, StarException> {
    auto forward = id.contains("../");
    if (!forward)
      return std::unexpected(std::move(forward.error()));
    bool upward = *forward;
    if (!upward) {
      auto backward = id.contains("..\\");
      if (!backward)
        return std::unexpected(std::move(backward.error()));
      upward = *backward;
    }
    if (upward)
      return std::unexpected(UniverseServerException(Star::format("UniverseServer: Custom world '{}' contains upwards paths!\n", id)));
    auto path = File::relative_to(directory, Star::format("custom_{}.world", id));
    auto exists = File::is_regular_file(path);
    if (!exists)
      return std::unexpected(std::move(exists.error()));
    std::shared_ptr<WorldServer> world;
    if (*exists) {
      if (auto logged = m_root.logger().info("UniverseServer: Loading custom world '{}'", id); !logged)
        return std::unexpected(std::move(logged.error()));
      auto file = File::open(path, IOMode::ReadWrite);
      if (!file)
        return std::unexpected(std::move(file.error()));
      auto loaded = WorldServer::create(m_root, *file);
      if (!loaded)
        return std::unexpected(std::move(loaded.error()));
      world = std::move(*loaded);
    } else if (worldTemplate) {
      if (auto logged = m_root.logger().info("UniverseServer: Creating custom world '{}'", id); !logged)
        return std::unexpected(std::move(logged.error()));
      auto file = File::open(path, IOMode::ReadWrite | IOMode::Truncate);
      if (!file)
        return std::unexpected(std::move(file.error()));
      auto created = WorldServer::create(m_root, *worldTemplate, *file);
      if (!created)
        return std::unexpected(std::move(created.error()));
      world = std::move(*created);
    } else {
      return std::unexpected(UniverseServerException(Star::format("UniverseServer: Custom world '{}' does not exist!\n", id)));
    }
    world->setUniverseSettings(m_universeSettings);
    world->setReferenceClock(clock);
    if (auto lua = world->initLua(universe); !lua)
      return std::unexpected(std::move(lua.error()));
    auto thread = WorldServerThread::create(world, id);
    if (!thread)
      return std::unexpected(std::move(thread.error()));
    (*thread)->setPause(m_pause);
    return universe.startWorldThread(std::move(*thread), id);
  }));
}

std::expected<std::optional<UniverseServer::WorldServerPromise>, StarException> UniverseServer::clientCustomWorldPromise(ClientCustomWorldId const& id, std::optional<std::shared_ptr<WorldTemplate>> worldTemplate) {
  auto configuration = m_root.configuration();
  if (!configuration)
    return std::unexpected(std::move(configuration.error()));
  auto disallowJson = (*configuration)->get("disallowClientCustomWorlds");
  if (!disallowJson)
    return std::unexpected(std::move(disallowJson.error()));
  auto disallow = disallowJson->opt_bool();
  if (!disallow)
    return std::unexpected(std::move(disallow.error()));
  if (disallow->value_or(false))
    return std::nullopt;
  auto clientId = clientForUuid(id.uuid);
  if (!clientId)
    return std::nullopt;
  auto context = m_clients.get(*clientId);
  if (!context)
    return std::unexpected(std::move(context.error()));
  auto client = context->get();
  if (client->net_rules().version() < 15) {
    if (auto logged = m_root.logger().warn("UniverseServer: Attempting to request custom world from old client!"); !logged)
      return std::unexpected(std::move(logged.error()));
    return std::nullopt;
  }
  auto producer = [&universe = *this, id, worldTemplate, client](WorldChunks chunks) -> WorkerPoolPromise<std::shared_ptr<WorldServerThread>> {
    return universe.m_workerPool.add_producer<std::shared_ptr<WorldServerThread>>([&universe, &m_root = universe.m_root, &m_universeSettings = universe.m_universeSettings, &m_pause = universe.m_pause, id, worldTemplate, client, chunks = std::move(chunks)]() -> std::expected<std::shared_ptr<WorldServerThread>, StarException> {
      std::shared_ptr<WorldServer> world;
      if (!chunks.empty()) {
        if (auto logged = m_root.logger().info("UniverseServer: Loading client custom world '{}' for '{}'", id.name, id.uuid.hex()); !logged)
          return std::unexpected(std::move(logged.error()));
        auto loaded = WorldServer::create(m_root, chunks);
        if (!loaded)
          return std::unexpected(std::move(loaded.error()));
        world = std::move(*loaded);
      } else if (worldTemplate) {
        if (auto logged = m_root.logger().info("UniverseServer: Creating client custom world '{}' for '{}'", id.name, id.uuid.hex()); !logged)
          return std::unexpected(std::move(logged.error()));
        auto file = File::ephemeral_file(m_root.random());
        if (!file)
          return std::unexpected(std::move(file.error()));
        auto created = WorldServer::create(m_root, *worldTemplate, *file);
        if (!created)
          return std::unexpected(std::move(created.error()));
        world = std::move(*created);
      } else {
        return std::unexpected(UniverseServerException(Star::format("UniverseServer: Client custom world '{}' for '{}' does not exist!\n", id.name, id.uuid.hex())));
      }
      auto clock = std::make_shared<Clock>();
      auto epoch = world->getProperty("customWorld.epoch");
      if (!epoch)
        return std::unexpected(std::move(epoch.error()));
      if (!epoch->can_convert(Json::Type::Float)) {
        if (auto set = world->setProperty("customWorld.epoch", Time::time_since_epoch()); !set)
          return std::unexpected(std::move(set.error()));
      } else {
        auto time = epoch->to_double();
        if (!time)
          return std::unexpected(std::move(time.error()));
        clock->set_time(Time::time_since_epoch() - *time);
      }
      world->setUniverseSettings(m_universeSettings);
      world->setReferenceClock(clock);
      clock->start();
      if (auto lua = world->initLua(universe); !lua)
        return std::unexpected(std::move(lua.error()));
      auto thread = WorldServerThread::create(world, id);
      if (!thread)
        return std::unexpected(std::move(thread.error()));
      (*thread)->setPause(m_pause);
      auto initial = (*thread)->readChunks();
      if (!initial)
        return std::unexpected(std::move(initial.error()));
      client->update_custom_world_chunks(id.name, std::move(*initial));
      client->set_custom_world_active(id.name, true);
      return universe.startWorldThread(std::move(*thread), id);
    });
  };
  if (auto chunks = client->custom_world_chunks(id.name))
    return WorldServerPromise(producer(std::move(*chunks)));
  if (auto logged = m_root.logger().info("UniverseServer: Requesting client custom world '{}' for '{}'", id.name, id.uuid.hex()); !logged)
    return std::unexpected(std::move(logged.error()));
  auto pair = RpcPromise<WorldChunks>::make_pair();
  if (auto requested = client->custom_world_requested(id.name, pair.second); !requested)
    return std::unexpected(std::move(requested.error()));
  if (auto sent = m_connectionServer->sendPackets(*clientId, {std::make_shared<ClientCustomWorldRequest>(id.name)}); !sent) {
    auto failed = pair.second.fail(String(std::string_view(sent.error().what())));
    if (!failed)
      return std::unexpected(sent.error().with_related(failed.error(), "Settling failed world request failed"));
    return std::unexpected(std::move(sent.error()));
  }
  return WorldServerPromise(std::move(producer), pair.first);
}

std::expected<std::shared_ptr<WorldServerThread>, StarException> UniverseServer::startWorldThread(std::shared_ptr<WorldServerThread> thread, WorldId const& worldId) {
  if (auto started = thread->start(); !started)
    return std::unexpected(std::move(started.error()));
  thread->setUpdateAction([&universe = *this](WorldServerThread& thread, WorldServer&) { return universe.worldUpdated(thread); });
  if (auto notified = notifyWorldCreated(worldId); !notified)
    return std::unexpected(std::move(notified.error()));
  return thread;
}

std::expected<void, StarException> UniverseServer::notifyWorldCreated(WorldId const& worldId) {
  auto configuration = m_root.configuration();
  if (!configuration)
    return std::unexpected(std::move(configuration.error()));
  std::shared_lock clientsLocker(m_clientsLock);
  auto allJson = (*configuration)->get("notifyClientsOnWorldCreate");
  if (!allJson)
    return std::unexpected(std::move(allJson.error()));
  auto all = allJson->opt_bool();
  if (!all)
    return std::unexpected(std::move(all.error()));
  bool notifyAll = all->value_or(false);
  bool notifyAdmins = false;
  if (!notifyAll) {
    auto adminsJson = (*configuration)->get("notifyAdminsOnWorldCreate");
    if (!adminsJson)
      return std::unexpected(std::move(adminsJson.error()));
    auto admins = adminsJson->opt_bool();
    if (!admins)
      return std::unexpected(std::move(admins.error()));
    notifyAdmins = admins->value_or(false);
  }
  for (auto const& entry : m_clients) {
    if (m_disconnectingClients.contains(entry.first))
      continue;
    auto const& client = entry.second;
    if (client->net_rules().version() < 16)
      continue;
    bool own = false;
    if (std::holds_alternative<ClientShipWorldId>(worldId))
      own = std::get<ClientShipWorldId>(worldId) == client->player_uuid();
    else if (std::holds_alternative<ClientCustomWorldId>(worldId))
      own = std::get<ClientCustomWorldId>(worldId).uuid == client->player_uuid();
    if (notifyAll || (notifyAdmins && client->is_admin()) || own) {
      if (auto sent = m_connectionServer->sendPackets(entry.first, {std::make_shared<NotifyWorldLoad>(worldId)}); !sent)
        return std::unexpected(std::move(sent.error()));
    }
  }
  return {};
}

std::expected<void, StarException> UniverseServer::createCustomWorld(CustomWorldId const& customWorld, std::shared_ptr<WorldTemplate> worldTemplate) {
  std::unique_lock locker(m_mainLock, std::defer_lock);
  lock_with_diagnostics(locker);
  if (!m_worlds.contains(customWorld)) {
    auto promise = customWorldPromise(customWorld, worldTemplate);
    if (!promise)
      return std::unexpected(std::move(promise.error()));
    if (*promise) {
      if (auto added = m_worlds.add(customWorld, std::move(*promise)); !added)
        return std::unexpected(std::move(added.error()));
    }
  }
  return {};
}

std::expected<std::shared_ptr<SystemWorldServerThread>, StarException> UniverseServer::createSystemWorld(Vec3I const& location) {
  if (!m_systemWorlds.contains(location)) {
    std::shared_ptr<SystemWorldServer> world;
    auto path = File::relative_to(m_storageDirectory, Star::format("{}_{}_{}.system", location[0], location[1], location[2]));
    auto exists = File::is_regular_file(path);
    if (!exists)
      return std::unexpected(std::move(exists.error()));
    if (*exists) {
      if (auto logged = m_root.logger().info("UniverseServer: Loading system world {} from disk storage", location); !logged)
        return std::unexpected(std::move(logged.error()));
      auto loaded = [&]() -> std::expected<std::shared_ptr<SystemWorldServer>, StarException> {
        auto database = m_root.versioningDatabase();
        if (!database)
          return std::unexpected(std::move(database.error()));
        auto versioned = VersionedJson::read_file(path);
        if (!versioned)
          return std::unexpected(std::move(versioned.error()));
        auto store = (*database)->loadVersionedJson(*versioned, "System");
        if (!store)
          return std::unexpected(std::move(store.error()));
        return SystemWorldServer::create(m_root, *store, m_universeClock, m_celestial_database);
      }();
      if (loaded)
        world = std::move(*loaded);
      else {
        if (auto logged = m_root.logger().error_exception(
              Star::format("UniverseServer: Failed to load system {} from disk storage, re-creating", location),
              ExceptionDiagnostic(loaded.error()));
            !logged)
          return std::unexpected(std::move(logged.error()));
        if (auto renamed = File::rename(path, Star::format("{}.{}.fail", path, Time::milliseconds_since_epoch())); !renamed)
          return std::unexpected(std::move(renamed.error()));
      }
    }
    if (!world) {
      if (auto logged = m_root.logger().info("UniverseServer: Creating new system world at location {}", location); !logged)
        return std::unexpected(std::move(logged.error()));
      auto created = SystemWorldServer::create(m_root, location, m_universeClock, m_celestial_database);
      if (!created)
        return std::unexpected(std::move(created.error()));
      world = std::move(*created);
      if (auto persistent = m_celestial_database->mark_persistent(CelestialCoordinate(location)); !persistent)
        return std::unexpected(std::move(persistent.error()));
    }
    auto thread = std::make_shared<SystemWorldServerThread>(location, world, path);
    thread->setUpdateAction([&universe = *this](SystemWorldServerThread& thread) { return universe.systemWorldUpdated(thread); });
    if (auto started = thread->start(); !started)
      return std::unexpected(std::move(started.error()));
    m_systemWorlds.set(location, std::move(thread));
  }
  auto world = m_systemWorlds.get(location);
  if (!world)
    return std::unexpected(std::move(world.error()));
  return world->get();
}

std::expected<bool, StarException> UniverseServer::instanceWorldStoredOrActive(InstanceWorldId const& worldId) const {
  if (m_worlds.value(worldId).has_value() || m_tempWorldIndex.contains(worldId))
    return true;
  return File::is_regular_file(File::relative_to(m_storageDirectory, Star::format("unique-{}.world", worldId.instance)));
}

std::expected<void, StarException> UniverseServer::worldDiedWithError(WorldId world) {
  if (std::holds_alternative<ClientShipWorldId>(world)) {
    if (auto clientId = getClientForUuid(std::get<ClientShipWorldId>(world))) {
      if (auto added = m_pendingDisconnections.add(*clientId, "Client ship world has errored"); !added)
        return std::unexpected(std::move(added.error()));
    }
  }
  return {};
}

std::expected<SkyParameters, StarException> UniverseServer::celestial_sky_parameters(CelestialCoordinate const& coordinate) const {
  auto valid = m_celestial_database->coordinate_valid(coordinate);
  if (!valid)
    return std::unexpected(std::move(valid.error()));
  if (*valid)
    return SkyParameters::create(m_root, coordinate, m_celestial_database);
  return SkyParameters();
}

std::expected<void, StarException> UniverseServer::startLuaScripts() {
  auto assets = m_root.assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  auto config = (*assets)->json("/universe_server.config");
  if (!config)
    return std::unexpected(std::move(config.error()));
  auto root = LuaRoot::create(m_root);
  if (!root)
    return std::unexpected(std::move(root.error()));
  m_luaRoot = std::move(*root);
  auto pause = config->get_float("luaGcPause");
  if (!pause)
    return std::unexpected(std::move(pause.error()));
  auto multiplier = config->get_float("luaGcStepMultiplier");
  if (!multiplier)
    return std::unexpected(std::move(multiplier.error()));
  m_luaRoot->tune_auto_garbage_collection(*pause, *multiplier);
  auto contexts = config->get_object("scriptContexts");
  if (!contexts)
    return std::unexpected(std::move(contexts.error()));
  for (auto const& context : *contexts) {
    auto script = std::make_shared<ScriptComponent>(m_root);
    if (auto set = script->set_lua_root(m_luaRoot); !set)
      return std::unexpected(std::move(set.error()));
    auto universe = LuaBindings::makeUniverseServerCallbacks(m_root, *this);
    if (!universe)
      return std::unexpected(std::move(universe.error()));
    auto celestial = LuaBindings::make_celestial_callbacks(m_root, *this);
    if (!celestial)
      return std::unexpected(std::move(celestial.error()));
    if (auto added = script->add_callbacks("universe", *universe); !added)
      return std::unexpected(std::move(added.error()));
    if (auto added = script->add_callbacks("celestial", *celestial); !added)
      return std::unexpected(std::move(added.error()));
    if (auto added = script->add_thread_callbacks("universe", *universe); !added)
      return std::unexpected(std::move(added.error()));
    if (auto added = script->add_thread_callbacks("celestial", *celestial); !added)
      return std::unexpected(std::move(added.error()));
    auto scripts = json_to_string_list(context.second);
    if (!scripts)
      return std::unexpected(std::move(scripts.error()));
    if (auto set = script->set_scripts(std::move(*scripts)); !set)
      return std::unexpected(std::move(set.error()));
    m_scriptContexts.set(context.first, script);
    if (auto initialized = recover_lua_script_error(script->init()); !initialized)
      return std::unexpected(std::move(initialized.error()));
  }
  return {};
}

std::expected<void, StarException> UniverseServer::updateLua() {
  for (auto&& context : m_scriptContexts) {
    if (auto updated = recover_lua_script_error(context.second->update()); !updated)
      return std::unexpected(std::move(updated.error()));
  }
  return {};
}

std::expected<void, StarException> UniverseServer::stopLua() {
  std::optional<StarException> error;
  for (auto&& context : m_scriptContexts) {
    if (auto stopped = recover_lua_script_error(context.second->uninit()); !stopped) {
      if (error)
        *error = error->with_related(stopped.error(), "Additional universe Lua shutdown failure");
      else
        error = std::move(stopped.error());
    }
  }
  m_scriptContexts.clear();
  if (error)
    return std::unexpected(std::move(*error));
  return {};
}

std::expected<void, StarException> UniverseServer::passMessage(Universe::Message&& message) {
  std::unique_lock locker(m_messageLock, std::defer_lock);
  lock_with_diagnostics(locker);
  m_universeMessages.push_back(std::move(message));
  return {};
}

std::expected<RpcPromise<Json>, StarException> UniverseServer::sendUniverseMessage(ConnectionId const& connectionId, String const& message, JsonArray const& args) {
  if (connectionId == server_connection_id) {
    auto pair = RpcPromise<Json>::make_pair();
    if (auto passed = passMessage({message, args, pair.second}); !passed)
      return std::unexpected(std::move(passed.error()));
    return pair.first;
  } else {
    std::shared_lock clientsLocker(m_clientsLock);
    if (auto client = m_clients.value(connectionId); client && !m_disconnectingClients.contains(connectionId)) {
      clientsLocker.unlock();
      if (client->net_rules().version() < 18) {
        return RpcPromise<Json>::make_failed("Client is not new enough");
      }
      auto pair = RpcPromise<Json>::make_pair();
      Uuid uuid(m_root.random());
      std::unique_lock messageLocker(m_messageLock, std::defer_lock);
      lock_with_diagnostics(messageLocker);
      m_universeMessageResponses[uuid] = {connectionId, pair.second};
      messageLocker.unlock();
      if (auto sent = m_connectionServer->sendPackets(connectionId, {std::make_shared<UniverseMessage>(connectionId, message, args, uuid)}); !sent) {
        m_universeMessageResponses.remove(uuid);
        return std::unexpected(std::move(sent.error()));
      }
      return pair.first;
    }
    return RpcPromise<Json>::make_failed("Client does not exist");
  }
}

MessageResponseResult UniverseServer::receiveMessage(String const& message, bool const& local, JsonArray const& args) {
  for (auto&& context : m_scriptContexts) {
    auto response = recover_lua_script_error(context.second->handle_message(message, local, args));
    if (!response)
      return std::unexpected(std::move(response.error()));
    if (*response)
      return response;
  }
  return std::nullopt;
}
}// namespace Star

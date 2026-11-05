import star.json;
import star.xx_hash;
import star.io_device;
import star.string;
import star.either;
import star.thread;
import std;
import star.id_map;
import star.data_stream;
import star.list;
import star.bi_map;
import star.game_types;
import star.version;
import star.ordered_map;
import star.color;
import star.random;
import star.weighted_pool;
import star.image_processing;
import star.hash;
import star.string_view;
import star.directives;
import star.asset_path;
import star.casting;
import star.strong_typedef;
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
import star.byte_array;

typedef struct ZSTD_CCtx_s ZSTD_CCtx;
typedef struct ZSTD_DCtx_s ZSTD_DCtx;
typedef ZSTD_DCtx ZSTD_DStream;
typedef ZSTD_CCtx ZSTD_CStream;
import star.zstd_compression;
import star.net_compatibility;
import star.format;
import star.logging;
import star.output_proxy;

import star.lua_root;
import star.host_address;
import star.socket;
import star.tcp;

import star.p2p_networking_service;
import star.collision_block;
import star.liquid_types;
import star.tile_damage;
import star.tile_sector_array;
import star.animation;
import star.particle;
import star.weather_types;
import star.celestial_coordinate;
import star.sky_types;
import star.force_regions;
import star.world_parameters;
import star.celestial_parameters;
import star.world_layout;
import star.collision_generator;
import star.world_tiles;
import star.item_descriptor;
import star.celestial_types;
import star.chat_types;
import star.uuid;
import star.tile_modification;
import star.damage_types;
import star.status_types;
import star.damage;
import star.light_source;
import star.entity;
import star.interaction_types;
import star.warping;
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
import star.sky_parameters;
import star.system_world;
import star.damage_manager;
import star.net_packets;
import star.net_packet_socket;
import star.universe_connection;

import star.algorithm;
namespace Star {

static constexpr auto PacketSocketPollInterval = std::chrono::milliseconds(1);

UniverseConnection::UniverseConnection(std::unique_ptr<PacketSocket> packetSocket)
    : m_packetSocket(std::move(packetSocket)) {}

UniverseConnection::UniverseConnection(UniverseConnection&& rhs) {
  operator=(std::move(rhs));
}

UniverseConnection::~UniverseConnection() {
  if (m_packetSocket)
    m_packetSocket->close();
}

UniverseConnection& UniverseConnection::operator=(UniverseConnection&& rhs) {
  std::lock_guard locker(m_mutex);
  m_sendQueue = take(rhs.m_sendQueue);
  m_receiveQueue = take(rhs.m_receiveQueue);
  m_packetSocket = take(rhs.m_packetSocket);
  return *this;
}

bool UniverseConnection::isOpen() const {
  std::lock_guard locker(m_mutex);
  return m_packetSocket && m_packetSocket->isOpen();
}

void UniverseConnection::close() {
  std::lock_guard locker(m_mutex);
  if (m_packetSocket)
    m_packetSocket->close();
}

std::expected<void, StarException> UniverseConnection::push(List<std::shared_ptr<Packet>> packets) {
  std::lock_guard locker(m_mutex);
  m_sendQueue.appendAll(std::move(packets));
  return {};
}

std::expected<void, StarException> UniverseConnection::pushSingle(std::shared_ptr<Packet> packet) {
  std::lock_guard locker(m_mutex);
  m_sendQueue.append(std::move(packet));
  return {};
}

std::expected<List<std::shared_ptr<Packet>>, StarException> UniverseConnection::pull() {
  std::lock_guard locker(m_mutex);
  return List<std::shared_ptr<Packet>>::from(take(m_receiveQueue));
}

std::expected<std::shared_ptr<Packet>, StarException> UniverseConnection::pullSingle() {
  std::lock_guard locker(m_mutex);
  if (m_receiveQueue.empty())
    return std::shared_ptr<Packet>{};
  return m_receiveQueue.takeFirst();
}

std::expected<bool, StarException> UniverseConnection::send() {
  std::lock_guard locker(m_mutex);
  if (!m_packetSocket)
    return std::unexpected(UniverseConnectionException("Cannot send on an empty connection"));
  auto queued = m_packetSocket->sendPackets(take(m_sendQueue));
  if (!queued)
    return std::unexpected(std::move(queued.error()));
  return m_packetSocket->writeData();
}

std::expected<bool, StarException> UniverseConnection::sendAll(unsigned timeout) {
  auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout);
  std::unique_lock locker(m_mutex);
  if (!m_packetSocket)
    return std::unexpected(UniverseConnectionException("Cannot send on an empty connection"));
  auto queued = m_packetSocket->sendPackets(take(m_sendQueue));
  if (!queued)
    return std::unexpected(std::move(queued.error()));
  while (true) {
    auto written = m_packetSocket->writeData();
    if (!written)
      return std::unexpected(std::move(written.error()));
    if (!m_packetSocket->sentPacketsPending())
      return true;
    if (std::chrono::steady_clock::now() >= deadline || !m_packetSocket->isOpen())
      return false;

    auto& socket = *m_packetSocket;
    locker.unlock();
    auto waited = socket.waitForData(true, deadline);
    locker.lock();
    if (!waited)
      return std::unexpected(std::move(waited.error()));
  }
}

std::expected<bool, StarException> UniverseConnection::receive() {
  std::lock_guard locker(m_mutex);
  if (!m_packetSocket)
    return std::unexpected(UniverseConnectionException("Cannot receive on an empty connection"));
  auto received = m_packetSocket->readData();
  if (!received)
    return std::unexpected(std::move(received.error()));
  auto packets = m_packetSocket->receivePackets();
  if (!packets)
    return std::unexpected(std::move(packets.error()));
  m_receiveQueue.appendAll(std::move(*packets));
  return *received;
}

std::expected<bool, StarException> UniverseConnection::receiveAny(unsigned timeout) {
  auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout);
  std::unique_lock locker(m_mutex);
  if (!m_packetSocket)
    return std::unexpected(UniverseConnectionException("Cannot receive on an empty connection"));
  if (!m_receiveQueue.empty())
    return true;
  while (true) {
    auto received = m_packetSocket->readData();
    if (!received)
      return std::unexpected(std::move(received.error()));
    auto packets = m_packetSocket->receivePackets();
    if (!packets)
      return std::unexpected(std::move(packets.error()));
    m_receiveQueue.appendAll(std::move(*packets));
    if (!m_receiveQueue.empty())
      return true;
    if (std::chrono::steady_clock::now() >= deadline || !m_packetSocket->isOpen())
      return false;

    auto& socket = *m_packetSocket;
    locker.unlock();
    auto waited = socket.waitForData(false, deadline);
    locker.lock();
    if (!waited)
      return std::unexpected(std::move(waited.error()));
  }
}

std::expected<std::reference_wrapper<PacketSocket>, StarException> UniverseConnection::packetSocket() {
  if (!m_packetSocket)
    return std::unexpected(UniverseConnectionException("Connection has no packet socket"));
  return std::ref(*m_packetSocket);
}

std::optional<PacketStats> UniverseConnection::incomingStats() const {
  std::lock_guard locker(m_mutex);
  return m_packetSocket->incomingStats();
}

std::optional<PacketStats> UniverseConnection::outgoingStats() const {
  std::lock_guard locker(m_mutex);
  return m_packetSocket->outgoingStats();
}

UniverseConnectionServer::UniverseConnectionServer(Logger& logger, PacketReceiveCallback packetReceiver, std::size_t numWorkerThreads)
    : m_logger(logger), m_packetReceiver(std::move(packetReceiver)),
      m_numWorkerThreads(numWorkerThreads ? numWorkerThreads : std::max<std::size_t>(2, std::thread::hardware_concurrency() / 4)) {}

std::expected<std::shared_ptr<UniverseConnectionServer>, StarException> UniverseConnectionServer::create(
    Logger& logger, PacketReceiveCallback packetReceiver, std::size_t numWorkerThreads) {
  if (!packetReceiver)
    return std::unexpected(UniverseConnectionException("Universe connection server requires a packet receiver"));
  auto server = std::shared_ptr<UniverseConnectionServer>(new UniverseConnectionServer(logger, std::move(packetReceiver), numWorkerThreads));
  auto initialized = server->initialize();
  if (!initialized)
    return std::unexpected(std::move(initialized.error()));
  return server;
}

std::expected<void, StarException> UniverseConnectionServer::initialize() {
  auto logged = m_logger.info("UniverseConnectionServer: Starting {} network worker threads", m_numWorkerThreads);
  if (!logged)
    return std::unexpected(std::move(logged.error()));
  m_connections.resize(m_numWorkerThreads);
  m_workerStats.resize(m_numWorkerThreads);
  m_processingThreads.reserve(m_numWorkerThreads);
  m_stopSource = std::stop_source{};
  auto stopToken = m_stopSource.get_token();
  for (std::size_t i = 0; i < m_numWorkerThreads; ++i) {
    auto thread = Thread::invoke(strf("UniverseConnectionServer::worker_{}", i), [&server = *this, i, stopToken]() -> std::expected<void, StarException> {
      auto processed = server.processConnections(i, stopToken);
      if (!processed) {
        std::unique_lock connectionsLocker(server.m_connectionsMutex, std::defer_lock);
        lockWithDiagnostics(connectionsLocker);
        for (auto& p : server.m_connections[i]) {
          std::lock_guard connectionLocker(p.second->mutex);
          if (p.second->packetSocket)
            p.second->packetSocket->close();
        }
      }
      return processed;
    });
    if (!thread) {
      auto failure = std::move(thread.error());
      if (auto closed = close(); !closed)
        failure = failure.withRelated(closed.error(), "Connection worker startup rollback failure");
      return std::unexpected(std::move(failure));
    }
    m_processingThreads.append(std::move(*thread));
  }
  return {};
}

std::expected<void, StarException> UniverseConnectionServer::processConnections(std::size_t i, std::stop_token stopToken) {
  while (!stopToken.stop_requested()) {
    std::unique_lock connectionsLocker(m_connectionsMutex, std::defer_lock);
    lockWithDiagnostics(connectionsLocker);
    auto connections = m_connections[i].pairs();
    connectionsLocker.unlock();
    bool dataTransmitted = false;
    std::size_t handledCount = 0;
    for (auto& p : connections) {
      ++handledCount;
      std::unique_lock connectionLocker(p.second->mutex);
      if (!p.second->packetSocket || !p.second->packetSocket->isOpen())
        continue;
      auto transmit = [&]() -> std::expected<void, StarException> {
        auto queued = p.second->packetSocket->sendPackets(take(p.second->sendQueue));
        if (!queued)
          return std::unexpected(std::move(queued.error()));
        auto written = p.second->packetSocket->writeData();
        if (!written)
          return std::unexpected(std::move(written.error()));
        dataTransmitted |= *written;
        auto read = p.second->packetSocket->readData();
        if (!read)
          return std::unexpected(std::move(read.error()));
        dataTransmitted |= *read;
        auto packets = p.second->packetSocket->receivePackets();
        if (!packets)
          return std::unexpected(std::move(packets.error()));
        if (!packets->empty()) {
          p.second->lastActivityTime = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() + std::chrono::microseconds(500));
          m_workerStats[i].packetsProcessed += packets->size();
          p.second->receiveQueue.appendAll(std::move(*packets));
        }
        return {};
      };
      auto transmitted = transmit();
      if (!transmitted) {
        p.second->packetSocket->close();
        connectionLocker.unlock();
        auto logged = m_logger.errorException(strf("Error receiving incoming server packets, disconnecting client '{}'", p.first), ExceptionDiagnostic(transmitted.error()));
        if (!logged)
          return std::unexpected(std::move(logged.error()));
        continue;
      }
      if (!p.second->receiveQueue.empty()) {
        auto toReceive = List<std::shared_ptr<Packet>>::from(take(p.second->receiveQueue));
        connectionLocker.unlock();
        auto received = m_packetReceiver(*this, p.first, std::move(toReceive));
        if (!received) {
          connectionLocker.lock();
          if (p.second->packetSocket)
            p.second->packetSocket->close();
          connectionLocker.unlock();
          auto logged = m_logger.errorException(strf("Error handling incoming server packets, disconnecting client '{}'", p.first), ExceptionDiagnostic(received.error()));
          if (!logged)
            return std::unexpected(std::move(logged.error()));
        }
      }
    }
    m_workerStats[i].connectionsHandled = handledCount;
    if (!dataTransmitted) {
      std::unique_lock workLocker(m_workMutex);
      if (!stopToken.stop_requested())
        m_workCondition.wait_for(workLocker, PacketSocketPollInterval);
    }
  }
  return {};
}

UniverseConnectionServer::~UniverseConnectionServer() {
  auto closed = close();
  if (!closed)
    EmergencyLog::error("UniverseConnectionServer worker cleanup failed: {}", outputException(closed.error(), true));
  if (!isClosed()) {
    EmergencyLog::error("UniverseConnectionServer retains unjoined native workers during owner destruction");
    fatalError("Cannot destroy a universe connection server with unjoined native workers", false);
  }
}

std::expected<void, StarException> UniverseConnectionServer::close() {
  if (m_closed)
    return {};
  {
    std::unique_lock connectionsLocker(m_connectionsMutex, std::defer_lock);
    lockWithDiagnostics(connectionsLocker);
    std::lock_guard workLocker(m_workMutex);
    m_stopSource.request_stop();
  }
  m_workCondition.notify_all();
  std::optional<StarException> failure;
  bool joined = true;
  for (auto& thread : m_processingThreads) {
    auto finished = thread.finish();
    if (!finished) {
      if (failure)
        failure = failure->withRelated(finished.error(), "Additional connection worker shutdown failure");
      else
        failure = std::move(finished.error());
    }
    joined = thread.isFinished() && joined;
  }
  if (!joined) {
    if (failure)
      return std::unexpected(std::move(*failure));
    return std::unexpected(UniverseConnectionException("Connection worker remained active after drain"));
  }
  std::unique_lock connectionsLocker(m_connectionsMutex, std::defer_lock);
  lockWithDiagnostics(connectionsLocker);
  for (auto& shard : m_connections) {
    for (auto& entry : shard) {
      std::lock_guard connectionLocker(entry.second->mutex);
      if (entry.second->packetSocket)
        entry.second->packetSocket->close();
    }
    shard.clear();
  }
  m_closed = true;
  if (failure)
    return std::unexpected(std::move(*failure));
  return {};
}

bool UniverseConnectionServer::isClosed() const {
  return m_closed;
}

bool UniverseConnectionServer::hasConnection(ConnectionId clientId) const {
  std::unique_lock connectionsLocker(m_connectionsMutex, std::defer_lock);
  lockWithDiagnostics(connectionsLocker);
  return m_connections[clientId % m_numWorkerThreads].contains(clientId);
}

List<ConnectionId> UniverseConnectionServer::allConnections() const {
  std::unique_lock connectionsLocker(m_connectionsMutex, std::defer_lock);
  lockWithDiagnostics(connectionsLocker);
  List<ConnectionId> connections;
  for (auto const& shard : m_connections)
    connections.appendAll(shard.keys());
  return connections;
}

std::expected<bool, StarException> UniverseConnectionServer::connectionIsOpen(ConnectionId clientId) const {
  std::unique_lock connectionsLocker(m_connectionsMutex, std::defer_lock);
  lockWithDiagnostics(connectionsLocker);
  if (auto conn = m_connections[clientId % m_numWorkerThreads].value(clientId)) {
    std::lock_guard connectionLocker(conn->mutex);
    connectionsLocker.unlock();
    return conn->packetSocket && conn->packetSocket->isOpen();
  }

  return std::unexpected(UniverseConnectionException::format("No such client '{}' in UniverseConnectionServer::connectionIsOpen", clientId));
}

std::expected<std::chrono::time_point<std::chrono::steady_clock, std::chrono::milliseconds>, StarException> UniverseConnectionServer::lastActivityTime(ConnectionId clientId) const {
  std::unique_lock connectionsLocker(m_connectionsMutex, std::defer_lock);
  lockWithDiagnostics(connectionsLocker);
  if (auto conn = m_connections[clientId % m_numWorkerThreads].value(clientId)) {
    std::lock_guard connectionLocker(conn->mutex);
    connectionsLocker.unlock();
    return conn->lastActivityTime;
  }
  return std::unexpected(UniverseConnectionException::format("No such client '{}' in UniverseConnectionServer::lastRemoteActivityTime", clientId));
}

std::expected<void, StarException> UniverseConnectionServer::addConnection(ConnectionId clientId, UniverseConnection uc) {
  std::unique_lock connectionsLocker(m_connectionsMutex, std::defer_lock);
  lockWithDiagnostics(connectionsLocker);
  if (m_stopSource.stop_requested())
    return std::unexpected(UniverseConnectionException("Cannot add a connection after network shutdown begins"));
  auto& shard = m_connections[clientId % m_numWorkerThreads];
  if (shard.contains(clientId))
    return std::unexpected(UniverseConnectionException::format("Client '{}' already exists in UniverseConnectionServer::addConnection", clientId));

  auto connection = std::make_shared<Connection>();
  connection->packetSocket = std::move(uc.m_packetSocket);
  connection->sendQueue = std::move(uc.m_sendQueue);
  connection->receiveQueue = std::move(uc.m_receiveQueue);
  connection->lastActivityTime = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() + std::chrono::microseconds(500));
  auto added = shard.add(clientId, std::move(connection));
  if (!added)
    return std::unexpected(std::move(added.error()));
  m_workCondition.notify_all();
  return {};
}

std::expected<UniverseConnection, StarException> UniverseConnectionServer::removeConnection(ConnectionId clientId) {
  std::unique_lock connectionsLocker(m_connectionsMutex, std::defer_lock);
  lockWithDiagnostics(connectionsLocker);
  auto& shard = m_connections[clientId % m_numWorkerThreads];
  if (!shard.contains(clientId))
    return std::unexpected(UniverseConnectionException::format("Client '{}' does not exist in UniverseConnectionServer::removeConnection", clientId));

  auto taken = shard.take(clientId);
  if (!taken)
    return std::unexpected(std::move(taken.error()));
  auto conn = std::move(*taken);
  std::lock_guard connectionLocker(conn->mutex);
  connectionsLocker.unlock();

  UniverseConnection uc;
  uc.m_packetSocket = take(conn->packetSocket);
  uc.m_sendQueue = std::move(conn->sendQueue);
  uc.m_receiveQueue = std::move(conn->receiveQueue);
  return uc;
}

List<UniverseConnection> UniverseConnectionServer::removeAllConnections() {
  List<UniverseConnection> removedConnections;
  std::unique_lock connectionsLocker(m_connectionsMutex, std::defer_lock);
  lockWithDiagnostics(connectionsLocker);
  for (auto& shard : m_connections) {
    for (auto& p : shard) {
      std::lock_guard connectionLocker(p.second->mutex);
      UniverseConnection uc;
      uc.m_packetSocket = take(p.second->packetSocket);
      uc.m_sendQueue = std::move(p.second->sendQueue);
      uc.m_receiveQueue = std::move(p.second->receiveQueue);
      removedConnections.append(std::move(uc));
    }
    shard.clear();
  }
  return removedConnections;
}

std::expected<void, StarException> UniverseConnectionServer::sendPackets(ConnectionId clientId, List<std::shared_ptr<Packet>> packets) {
  std::unique_lock connectionsLocker(m_connectionsMutex, std::defer_lock);
  lockWithDiagnostics(connectionsLocker);
  if (auto conn = m_connections[clientId % m_numWorkerThreads].value(clientId)) {
    std::lock_guard connectionLocker(conn->mutex);
    connectionsLocker.unlock();
    conn->sendQueue.appendAll(std::move(packets));

    if (conn->packetSocket && conn->packetSocket->isOpen()) {
      auto queued = conn->packetSocket->sendPackets(take(conn->sendQueue));
      if (!queued)
        return std::unexpected(std::move(queued.error()));
      auto written = conn->packetSocket->writeData();
      if (!written)
        return std::unexpected(std::move(written.error()));
    }
    m_workCondition.notify_all();
    return {};
  } else {
    return std::unexpected(UniverseConnectionException::format("No such client '{}' in UniverseConnectionServer::sendPackets", clientId));
  }
}

std::uint64_t UniverseConnectionServer::totalPacketsProcessed() const {
  std::uint64_t total = 0;
  for (auto const& stats : m_workerStats)
    total += stats.packetsProcessed.load();
  return total;
}

std::size_t UniverseConnectionServer::numWorkerThreads() const {
  return m_numWorkerThreads;
}

}// namespace Star

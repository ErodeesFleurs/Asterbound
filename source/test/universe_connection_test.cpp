#ifdef STAR_SYSTEM_FAMILY_WINDOWS
#include <winsock2.h>
#else
#include <sys/socket.h>
#endif

import star.xx_hash;
import star.algorithm;
import star.json;
import star.io_device;
import star.string;
import star.thread;
import star.logging;
import std;
import star.unique_function;
import star.id_map;
import star.data_stream;
import star.list;
import star.bi_map;
import star.game_types;
import star.format;
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

import star.host_address;
import star.socket;
import star.tcp;
import star.net_impl;

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
import star.lua_root;
import star.versioning_database;
import star.world_storage;
import star.player_types;
import star.sky_parameters;
import star.system_world;
import star.damage_manager;
import star.net_packets;
import star.net_packet_socket;
import star.universe_connection;
import star.game_test;
import star.root;

#include "gtest/gtest.h"

#ifdef STAR_TEST_WRAP_PTHREAD_CREATE
#include <pthread.h>
#include <errno.h>
#include <unistd.h>

namespace {
thread_local int threadStartsBeforeFailure = -1;
thread_local std::optional<std::reference_wrapper<std::atomic<unsigned>>> completedInjectedThreads;

struct InjectedThreadStart {
  void* (*function)(void*);
  void* argument;
  std::reference_wrapper<std::atomic<unsigned>> completed;
};
}

extern "C" int __real_pthread_create(pthread_t*, pthread_attr_t const*, void* (*)(void*), void*);

extern "C" int __wrap_pthread_create(pthread_t* thread, pthread_attr_t const* attributes,
    void* (*function)(void*), void* argument) {
  if (threadStartsBeforeFailure < 0)
    return __real_pthread_create(thread, attributes, function, argument);
  if (threadStartsBeforeFailure-- == 0) {
    threadStartsBeforeFailure = -1;
    return EAGAIN;
  }
  auto start = std::unique_ptr<InjectedThreadStart>(
      new (std::nothrow) InjectedThreadStart{function, argument, *completedInjectedThreads});
  if (!start) {
    threadStartsBeforeFailure = -1;
    return EAGAIN;
  }
  auto run = [](void* argument) -> void* {
    std::unique_ptr<InjectedThreadStart> start(static_cast<InjectedThreadStart*>(argument));
    auto result = start->function(start->argument);
    ++start->completed.get();
    return result;
  };
  int result = __real_pthread_create(thread, attributes, run, start.get());
  if (result == 0)
    start.release();
  return result;
}
#endif

using namespace Star;

namespace Star {
// Only this real-socket fixture changes native buffer sizes; production sockets
// retain their normal settings and expose no test/failure control API.
struct SocketTestAccess {
  [[nodiscard]] static std::expected<void, StarException> setBufferSize(Socket& socket, int option, int bytes) {
    std::shared_lock locker(socket.m_mutex);
    return socket.m_impl->setSockOpt(SOL_SOCKET, option, &bytes, sizeof(bytes));
  }
  static bool operationHoldsLock(Socket& socket) {
    std::unique_lock locker(socket.m_mutex, std::try_to_lock);
    return !locker.owns_lock();
  }
  static std::span<char const> pendingPlain(TcpPacketSocket const& socket) {
    return socket.m_outputBuffer.span();
  }
  static std::span<char const> pendingCompressed(TcpPacketSocket const& socket) {
    return socket.m_compressedOutputBuffer.span();
  }
  [[nodiscard]] static std::expected<void, StarException> resetOnClose(Socket& socket) {
    std::shared_lock locker(socket.m_mutex);
    linger reset{1, 0};
    return socket.m_impl->setSockOpt(SOL_SOCKET, SO_LINGER, &reset, sizeof(reset));
  }
};
}

unsigned const PacketCount = 20;
std::uint16_t const ServerPort = 55555;

unsigned const NumLocalASyncConnections = 5;
unsigned const NumRemoteASyncConnections = 5;
unsigned const ASyncSleepMillis = 5;

unsigned const NumLocalSyncConnections = 5;
unsigned const NumRemoteSyncConnections = 5;
unsigned const SyncWaitMillis = 10000;

class ASyncClientThread : public Thread {
public:
  [[nodiscard]] static std::expected<std::unique_ptr<ASyncClientThread>, StarException> create(UniverseConnection conn) {
    auto client = std::unique_ptr<ASyncClientThread>(new ASyncClientThread(std::move(conn)));
    auto started = client->start();
    if (!started)
      return std::unexpected(std::move(started.error()));
    return client;
  }

  ~ASyncClientThread() override {
    auto joined = join();
    EXPECT_TRUE(joined);
  }

  [[nodiscard]] std::expected<void, StarException> run() override {
    auto result = runConnection();
    if (!result)
      ADD_FAILURE() << result.error().what();
    return result;
  }

private:
  [[nodiscard]] std::expected<void, StarException> runConnection() {
    unsigned read = 0;
    unsigned written = 0;
    while (read < PacketCount || written < PacketCount) {
      auto received = m_connection.receive();
      if (!received)
        return std::unexpected(std::move(received.error()));
      if (read < PacketCount) {
        auto packet = m_connection.pullSingle();
        if (!packet)
          return std::unexpected(std::move(packet.error()));
        if (*packet) {
          auto request = convert<ProtocolRequestPacket>(*packet);
          if (!request)
            return std::unexpected(std::move(request.error()));
          EXPECT_EQ((*request)->requestProtocolVersion, read);
          ++read;
        }
      }
      if (written < PacketCount) {
        auto pushed = m_connection.push({std::make_shared<ProtocolRequestPacket>(written)});
        if (!pushed)
          return std::unexpected(std::move(pushed.error()));
        ++written;
      }
      auto sent = m_connection.send();
      if (!sent)
        return std::unexpected(std::move(sent.error()));
      std::this_thread::sleep_for(std::chrono::milliseconds(ASyncSleepMillis));
      if (!m_connection.isOpen())
        break;
    }
    EXPECT_EQ(PacketCount, read);
    EXPECT_EQ(PacketCount, written);
    m_connection.close();
    auto packets = m_connection.pull();
    if (!packets)
      return std::unexpected(std::move(packets.error()));
    EXPECT_TRUE(packets->empty());
    return {};
  }

private:
  ASyncClientThread(UniverseConnection conn)
    : Thread("UniverseConnectionTestClientThread"), m_connection(std::move(conn)) {}

  UniverseConnection m_connection;
};

class SyncClientThread : public Thread {
public:
  [[nodiscard]] static std::expected<std::unique_ptr<SyncClientThread>, StarException> create(UniverseConnection conn) {
    auto client = std::unique_ptr<SyncClientThread>(new SyncClientThread(std::move(conn)));
    auto started = client->start();
    if (!started)
      return std::unexpected(std::move(started.error()));
    return client;
  }

  ~SyncClientThread() override {
    auto joined = join();
    EXPECT_TRUE(joined);
  }

  [[nodiscard]] std::expected<void, StarException> run() override {
    auto result = runConnection();
    if (!result)
      ADD_FAILURE() << result.error().what();
    return result;
  }

private:
  [[nodiscard]] std::expected<void, StarException> runConnection() {
    for (unsigned i = 0; i < PacketCount; ++i) {
      auto pushed = m_connection.pushSingle(std::make_shared<ProtocolRequestPacket>(i));
      if (!pushed)
        return std::unexpected(std::move(pushed.error()));
      auto sent = m_connection.sendAll(SyncWaitMillis);
      if (!sent)
        return std::unexpected(std::move(sent.error()));
      EXPECT_TRUE(*sent);
      auto received = m_connection.receiveAny(SyncWaitMillis);
      if (!received)
        return std::unexpected(std::move(received.error()));
      EXPECT_TRUE(*received);
      auto packet = m_connection.pullSingle();
      if (!packet)
        return std::unexpected(std::move(packet.error()));
      auto request = convert<ProtocolRequestPacket>(*packet);
      if (!request)
        return std::unexpected(std::move(request.error()));
      EXPECT_EQ((*request)->requestProtocolVersion, i);
      if (!m_connection.isOpen())
        break;
    }
    m_connection.close();
    auto packets = m_connection.pull();
    if (!packets)
      return std::unexpected(std::move(packets.error()));
    EXPECT_TRUE(packets->empty());
    return {};
  }

private:
  SyncClientThread(UniverseConnection conn)
    : Thread("UniverseConnectionTestClientThread"), m_connection(std::move(conn)) {}

  UniverseConnection m_connection;
};

const GameTestRegistration universeConnectionsAll("UniverseConnections", "All", [](GameTest& fixture) {
  auto& logger = fixture.root().logger();
  auto server = UniverseConnectionServer::create(logger, [](UniverseConnectionServer& server, ConnectionId clientId,
      List<std::shared_ptr<::Star::Packet>> packets) -> std::expected<void, StarException> {
    return server.sendPackets(clientId, std::move(packets));
  });
  ASSERT_TRUE(server) << server.error().what();

  ConnectionId clientId = ServerConnectionId;
  auto tcpServer = TcpServer::create(logger, HostAddressWithPort(HostAddress::localhost(), ServerPort));
  ASSERT_TRUE(tcpServer) << tcpServer.error().what();
  auto acceptCallback = (*tcpServer)->setAcceptCallback([&logger, &server, &clientId](
      std::shared_ptr<::Star::TcpSocket> socket) -> std::expected<void, StarException> {
    auto nonblocking = socket->setNonBlocking(true);
    if (!nonblocking)
      return std::unexpected(std::move(nonblocking.error()));
    auto packetSocket = TcpPacketSocket::open(logger, std::move(socket));
    if (!packetSocket)
      return std::unexpected(std::move(packetSocket.error()));
    return (*server)->addConnection(++clientId, UniverseConnection(std::move(*packetSocket)));
  });
  ASSERT_TRUE(acceptCallback) << acceptCallback.error().what();

  List<std::unique_ptr<ASyncClientThread>> localASyncClients;
  for (unsigned i = 0; i < NumLocalASyncConnections; ++i) {
    auto pair = LocalPacketSocket::openPair();
    ASSERT_TRUE((*server)->addConnection(++clientId, UniverseConnection(std::move(pair.first))));
    auto client = ASyncClientThread::create(UniverseConnection(std::move(pair.second)));
    ASSERT_TRUE(client) << client.error().what();
    localASyncClients.push_back(std::move(*client));
  }

  List<std::unique_ptr<SyncClientThread>> localSyncClients;
  for (unsigned i = 0; i < NumLocalSyncConnections; ++i) {
    auto pair = LocalPacketSocket::openPair();
    ASSERT_TRUE((*server)->addConnection(++clientId, UniverseConnection(std::move(pair.first))));
    auto client = SyncClientThread::create(UniverseConnection(std::move(pair.second)));
    ASSERT_TRUE(client) << client.error().what();
    localSyncClients.push_back(std::move(*client));
  }

  List<std::unique_ptr<ASyncClientThread>> remoteASyncClients;
  for (unsigned i = 0; i < NumRemoteASyncConnections; ++i) {
    auto socket = TcpSocket::connectTo({HostAddress::localhost(), ServerPort});
    ASSERT_TRUE(socket) << socket.error().what();
    ASSERT_TRUE((*socket)->setNonBlocking(true));
    auto packetSocket = TcpPacketSocket::open(logger, std::move(*socket));
    ASSERT_TRUE(packetSocket) << packetSocket.error().what();
    auto client = ASyncClientThread::create(UniverseConnection(std::move(*packetSocket)));
    ASSERT_TRUE(client) << client.error().what();
    remoteASyncClients.push_back(std::move(*client));
  }

  List<std::unique_ptr<SyncClientThread>> remoteSyncClients;
  for (unsigned i = 0; i < NumRemoteSyncConnections; ++i) {
    auto socket = TcpSocket::connectTo({HostAddress::localhost(), ServerPort});
    ASSERT_TRUE(socket) << socket.error().what();
    ASSERT_TRUE((*socket)->setNonBlocking(true));
    auto packetSocket = TcpPacketSocket::open(logger, std::move(*socket));
    ASSERT_TRUE(packetSocket) << packetSocket.error().what();
    auto client = SyncClientThread::create(UniverseConnection(std::move(*packetSocket)));
    ASSERT_TRUE(client) << client.error().what();
    remoteSyncClients.push_back(std::move(*client));
  }

  for (auto& c : localASyncClients)
    EXPECT_TRUE(c->join());

  for (auto& c : remoteASyncClients)
    EXPECT_TRUE(c->join());

  for (auto& c : localSyncClients)
    EXPECT_TRUE(c->join());

  for (auto& c : remoteSyncClients)
    EXPECT_TRUE(c->join());

  EXPECT_TRUE((*tcpServer)->stop());
  (*server)->removeAllConnections();
});

namespace {

[[nodiscard]] std::expected<std::pair<std::shared_ptr<TcpSocket>, std::shared_ptr<TcpSocket>>, StarException> connectionTestTcpPair(Logger& logger) {
  auto listener = TcpSocket::listen(logger, {HostAddress::localhost(), 0});
  if (!listener)
    return std::unexpected(std::move(listener.error()));
  auto nonblocking = (*listener)->setNonBlocking(true);
  if (!nonblocking)
    return std::unexpected(std::move(nonblocking.error()));
  auto client = TcpSocket::connectTo((*listener)->localAddress());
  if (!client)
    return std::unexpected(std::move(client.error()));
  auto ready = (*listener)->waitForData(false, std::chrono::steady_clock::now() + std::chrono::seconds(2));
  if (!ready)
    return std::unexpected(std::move(ready.error()));
  auto server = (*listener)->accept(logger);
  if (!server)
    return std::unexpected(std::move(server.error()));
  if (!*server)
    return std::unexpected(StarException("Loopback connection was not accepted"));
  return std::pair{std::move(*client), std::move(*server)};
}

[[nodiscard]] std::expected<ByteArray, StarException> connectionTestFrame(VersionNumber version) {
  ProtocolRequestPacket packet(version);
  DataStreamBuffer payload;
  auto packetWritten = packet.write(payload);
  if (!packetWritten)
    return std::unexpected(std::move(packetWritten.error()));
  DataStreamBuffer frame;
  auto typeWritten = frame.write(PacketType::ProtocolRequest);
  if (!typeWritten)
    return std::unexpected(std::move(typeWritten.error()));
  auto sizeWritten = frame.write_vlq_i(payload.size());
  if (!sizeWritten)
    return std::unexpected(std::move(sizeWritten.error()));
  auto dataWritten = frame.write_data(payload.span());
  if (!dataWritten)
    return std::unexpected(std::move(dataWritten.error()));
  return frame.take_data();
}

[[nodiscard]] std::expected<VersionNumber, StarException> connectionTestVersion(UniverseConnection& connection) {
  auto packet = connection.pullSingle();
  if (!packet)
    return std::unexpected(std::move(packet.error()));
  if (!*packet)
    return std::unexpected(StarException("Expected a queued protocol request packet"));
  auto request = convert<ProtocolRequestPacket>(*packet);
  if (!request)
    return std::unexpected(std::move(request.error()));
  return (*request)->requestProtocolVersion;
}

// std::async cannot report native thread-start failures without throwing.
// Keep the future's timed readiness assertions, but start and join through the
// production expected-valued thread API.
template <typename T>
class ConnectionTestAsync {
public:
  template <typename Function>
  [[nodiscard]] static std::expected<std::unique_ptr<ConnectionTestAsync>, StarException> create(Function function) {
    auto task = std::unique_ptr<ConnectionTestAsync>(new ConnectionTestAsync);
    auto thread = ThreadFunction<void>::create(
        [&task = *task, function = std::move(function)]() mutable -> std::expected<void, StarException> {
          task.m_promise.set_value(function());
          return {};
        }, "UniverseConnectionTestAsync");
    if (!thread)
      return std::unexpected(std::move(thread.error()));
    task->m_thread.emplace(std::move(*thread));
    return task;
  }

  ~ConnectionTestAsync() {
    if (m_thread) {
      auto finished = m_thread->finish();
      EXPECT_TRUE(finished);
    }
  }

  template <typename Rep, typename Period>
  std::future_status wait_for(std::chrono::duration<Rep, Period> const& duration) {
    return m_future.wait_for(duration);
  }

  [[nodiscard]] std::expected<T, StarException> get() {
    return m_future.get();
  }

private:
  ConnectionTestAsync() : m_future(m_promise.get_future()) {}

  std::promise<std::expected<T, StarException>> m_promise;
  std::future<std::expected<T, StarException>> m_future;
  std::optional<ThreadFunction<void>> m_thread;
};

std::expected<void, StarException> wait_for_wire_progress(
    std::shared_ptr<TcpSocket> const& incoming, std::shared_ptr<TcpSocket> const& outgoing,
    bool pending, std::chrono::steady_clock::time_point deadline) {
  auto remaining = std::chrono::ceil<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now());
  if (remaining.count() <= 0)
    return {};
  SocketPollQuery query{{incoming, {true, false}}};
  if (pending)
    query.emplace(outgoing, SocketPollQueryEntry{false, true});
  auto ready = Socket::poll(query, static_cast<unsigned>(remaining.count()));
  if (!ready)
    return std::unexpected(std::move(ready.error()));
  return {};
}

const GameTestRegistration universeConnectionsWorldUpdatePacketsTransferRvaluesAndCopyLvalues(
    "UniverseConnections", "WorldUpdatePacketsTransferRvaluesAndCopyLvalues", [](GameTest&) {
  ByteArray parameters(4096, 'p');
  auto parametersAllocation = parameters.data();
  WorldParametersUpdatePacket movedParameters(std::move(parameters));
  EXPECT_EQ(movedParameters.parametersData.data(), parametersAllocation);
  EXPECT_TRUE(parameters.empty());
  WorldParametersUpdatePacket copiedParameters(movedParameters.parametersData);
  EXPECT_NE(copiedParameters.parametersData.data(), parametersAllocation);
  EXPECT_EQ(copiedParameters.parametersData, movedParameters.parametersData);

  ByteArray delta(4096, 'd');
  auto deltaAllocation = delta.data();
  WorldClientStateUpdatePacket movedDelta(std::move(delta));
  EXPECT_EQ(movedDelta.worldClientStateDelta.data(), deltaAllocation);
  EXPECT_TRUE(delta.empty());
  WorldClientStateUpdatePacket copiedDelta(movedDelta.worldClientStateDelta);
  EXPECT_NE(copiedDelta.worldClientStateDelta.data(), deltaAllocation);
  EXPECT_EQ(copiedDelta.worldClientStateDelta, movedDelta.worldClientStateDelta);
});

const GameTestRegistration universeConnectionsTcpPartialDrainPreservesWireAndAppendOrder(
    "UniverseConnections", "TcpPartialDrainPreservesWireAndAppendOrder", [](GameTest& fixture) {
  auto pair = connectionTestTcpPair(fixture.root().logger());
  ASSERT_TRUE(pair) << pair.error().what();
  auto [outgoing, incoming] = std::move(*pair);
  ASSERT_TRUE(SocketTestAccess::setBufferSize(*outgoing, SO_SNDBUF, 16384));
  ASSERT_TRUE(incoming->setNonBlocking(true));
  auto transport = TcpPacketSocket::open(fixture.root().logger(), outgoing);
  ASSERT_TRUE(transport) << transport.error().what();
  auto parameters = std::make_shared<WorldParametersUpdatePacket>(ByteArray(4 << 20, 'p'));
  parameters->setCompressionMode(PacketCompressionMode::Disabled);
  DataStreamBuffer payload;
  ASSERT_TRUE(parameters->write(payload));
  DataStreamBuffer expected;
  ASSERT_TRUE(expected.write(parameters->type()));
  ASSERT_TRUE(expected.write_vlq_i(payload.size()));
  ASSERT_TRUE(expected.write_data(payload.span()));
  ASSERT_TRUE((*transport)->sendPackets({parameters}));
  ASSERT_EQ((*transport)->writeData(), true);
  ASSERT_TRUE((*transport)->sentPacketsPending());
  auto pending = SocketTestAccess::pendingPlain(**transport);
  ASSERT_LT(pending.size(), expected.size());
  ASSERT_FALSE(pending.empty());
  EXPECT_TRUE(std::ranges::equal(pending, expected.span().last(pending.size())));

  auto delta = std::make_shared<WorldClientStateUpdatePacket>(ByteArray(4096, 'd'));
  delta->setCompressionMode(PacketCompressionMode::Disabled);
  DataStreamBuffer deltaPayload;
  ASSERT_TRUE(delta->write(deltaPayload));
  ASSERT_TRUE(expected.write(delta->type()));
  ASSERT_TRUE(expected.write_vlq_i(deltaPayload.size()));
  ASSERT_TRUE(expected.write_data(deltaPayload.span()));
  ASSERT_TRUE((*transport)->sendPackets({delta}));

  ByteArray wire;
  auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  while (wire.size() < expected.size() && (deadline - std::chrono::steady_clock::now()).count() > 0) {
    std::array<char, 65536> buffer;
    auto received = incoming->receive(buffer);
    ASSERT_TRUE(received) << received.error().what();
    wire.append(std::span<char const>(buffer).first(*received));
    auto written = (*transport)->writeData();
    ASSERT_TRUE(written) << written.error().what();
    if (*received == 0 && !*written)
      ASSERT_TRUE(wait_for_wire_progress(incoming, outgoing, (*transport)->sentPacketsPending(), deadline));
  }
  ASSERT_EQ(wire, expected.data()) << "received=" << wire.size() << " expected=" << expected.size();
  EXPECT_FALSE((*transport)->sentPacketsPending());
  // Decode the actual captured wire, including both differently typed frames.
  DataStreamExternalBuffer frames(wire.span());
  for (auto const& packet : List<std::shared_ptr<Packet>>{parameters, delta}) {
    ASSERT_EQ(frames.read<PacketType>(), packet->type());
    auto length = frames.read_vlq_i();
    ASSERT_TRUE(length) << length.error().what();
    auto position = frames.pos();
    ASSERT_TRUE(position) << position.error().what();
    ASSERT_GE(*length, 0);
    DataStreamExternalBuffer body(frames.span().subspan(*position, *length));
    auto decoded = createPacket(packet->type());
    ASSERT_TRUE(decoded) << decoded.error().what();
    ASSERT_TRUE((*decoded)->read(body));
    ASSERT_EQ(body.at_end(), true);
    DataStreamBuffer roundTrip;
    ASSERT_TRUE((*decoded)->write(roundTrip));
    DataStreamBuffer original;
    ASSERT_TRUE(packet->write(original));
    EXPECT_EQ(roundTrip.data(), original.data());
    ASSERT_TRUE(frames.seek(*length, IOSeek::Relative));
  }
  EXPECT_EQ(frames.at_end(), true);
});

const GameTestRegistration universeConnectionsTcpCompressedTailPrecedesNewPlainFrames(
    "UniverseConnections", "TcpCompressedTailPrecedesNewPlainFrames", [](GameTest& fixture) {
  auto pair = connectionTestTcpPair(fixture.root().logger());
  ASSERT_TRUE(pair) << pair.error().what();
  auto [outgoing, incoming] = std::move(*pair);
  ASSERT_TRUE(SocketTestAccess::setBufferSize(*outgoing, SO_SNDBUF, 16384));
  ASSERT_TRUE(incoming->setNonBlocking(true));
  auto transport = TcpPacketSocket::open(fixture.root().logger(), outgoing);
  ASSERT_TRUE(transport) << transport.error().what();
  (*transport)->setCompressionStreamEnabled(true);
  ByteArray bytes(4 << 20, 0);
  std::uint32_t state = 0x12345678;
  for (auto& byte : bytes) {
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    byte = static_cast<char>(state);
  }
  auto parameters = std::make_shared<WorldParametersUpdatePacket>(std::move(bytes));
  ASSERT_TRUE((*transport)->sendPackets({parameters}));
  ByteArray original(SocketTestAccess::pendingPlain(**transport));
  auto compressor = CompressionStream::create();
  ASSERT_TRUE(compressor) << compressor.error().what();
  auto encoded = (*compressor)->compress(original);
  ASSERT_TRUE(encoded) << encoded.error().what();
  ByteArray expected = std::move(*encoded);
  auto compressedSize = expected.size();
  ASSERT_EQ((*transport)->writeData(), true);
  ASSERT_TRUE((*transport)->sentPacketsPending());
  ASSERT_FALSE(SocketTestAccess::pendingCompressed(**transport).empty());
  EXPECT_TRUE(SocketTestAccess::pendingPlain(**transport).empty());

  (*transport)->setCompressionStreamEnabled(false);
  auto plain = connectionTestFrame(73);
  ASSERT_TRUE(plain) << plain.error().what();
  expected.append(*plain);
  auto request = std::make_shared<ProtocolRequestPacket>(73);
  request->setCompressionMode(PacketCompressionMode::Disabled);
  ASSERT_TRUE((*transport)->sendPackets({request}));
  EXPECT_TRUE((*transport)->sentPacketsPending());
  ByteArray wire;
  auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  while (wire.size() < expected.size() && (deadline - std::chrono::steady_clock::now()).count() > 0) {
    std::array<char, 65536> buffer;
    auto received = incoming->receive(buffer);
    ASSERT_TRUE(received) << received.error().what();
    wire.append(std::span<char const>(buffer).first(*received));
    auto written = (*transport)->writeData();
    ASSERT_TRUE(written) << written.error().what();
    if (*received == 0 && !*written)
      ASSERT_TRUE(wait_for_wire_progress(incoming, outgoing, (*transport)->sentPacketsPending(), deadline));
  }
  ASSERT_EQ(wire, expected) << "received=" << wire.size() << " expected=" << expected.size();
  EXPECT_FALSE((*transport)->sentPacketsPending());
  auto decompressor = DecompressionStream::create();
  ASSERT_TRUE(decompressor) << decompressor.error().what();
  auto decoded = (*decompressor)->decompress(wire.span().first(compressedSize));
  ASSERT_TRUE(decoded) << decoded.error().what();
  EXPECT_EQ(*decoded, original);
  EXPECT_TRUE(std::ranges::equal(wire.span().subspan(compressedSize), plain->span()));
});

const GameTestRegistration universeConnectionsTcpEnablingCompressionUsesOnlyUnsentPlainTail(
    "UniverseConnections", "TcpEnablingCompressionUsesOnlyUnsentPlainTail", [](GameTest& fixture) {
  auto pair = connectionTestTcpPair(fixture.root().logger());
  ASSERT_TRUE(pair) << pair.error().what();
  auto [outgoing, incoming] = std::move(*pair);
  ASSERT_TRUE(SocketTestAccess::setBufferSize(*outgoing, SO_SNDBUF, 16384));
  ASSERT_TRUE(incoming->setNonBlocking(true));
  auto transport = TcpPacketSocket::open(fixture.root().logger(), outgoing);
  ASSERT_TRUE(transport) << transport.error().what();
  auto parameters = std::make_shared<WorldParametersUpdatePacket>(ByteArray(4 << 20, 'p'));
  parameters->setCompressionMode(PacketCompressionMode::Disabled);
  ASSERT_TRUE((*transport)->sendPackets({parameters}));
  ByteArray original(SocketTestAccess::pendingPlain(**transport));
  ASSERT_EQ((*transport)->writeData(), true);
  auto pending = SocketTestAccess::pendingPlain(**transport);
  ASSERT_FALSE(pending.empty());
  ASSERT_LT(pending.size(), original.size());
  auto consumed = original.size() - pending.size();
  ByteArray expected(original.span().first(consumed));

  (*transport)->setCompressionStreamEnabled(true);
  ASSERT_TRUE((*transport)->sendPackets({std::make_shared<ProtocolRequestPacket>(74)}));
  ByteArray compressionInput(SocketTestAccess::pendingPlain(**transport));
  auto compressor = CompressionStream::create();
  ASSERT_TRUE(compressor) << compressor.error().what();
  auto encoded = (*compressor)->compress(compressionInput);
  ASSERT_TRUE(encoded) << encoded.error().what();
  expected.append(*encoded);
  ByteArray wire;
  auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  while (wire.size() < expected.size() && (deadline - std::chrono::steady_clock::now()).count() > 0) {
    std::array<char, 65536> buffer;
    auto received = incoming->receive(buffer);
    ASSERT_TRUE(received) << received.error().what();
    wire.append(std::span<char const>(buffer).first(*received));
    auto written = (*transport)->writeData();
    ASSERT_TRUE(written) << written.error().what();
    if (*received == 0 && !*written)
      ASSERT_TRUE(incoming->waitForData(false, deadline));
  }
  ASSERT_EQ(wire, expected);
  EXPECT_FALSE((*transport)->sentPacketsPending());
  auto decompressor = DecompressionStream::create();
  ASSERT_TRUE(decompressor) << decompressor.error().what();
  auto decoded = (*decompressor)->decompress(wire.span().subspan(consumed));
  ASSERT_TRUE(decoded) << decoded.error().what();
  EXPECT_EQ(*decoded, compressionInput);
});

const GameTestRegistration universeConnectionsTcpDrainCommitsPrefixBeforeNativeReset(
    "UniverseConnections", "TcpDrainCommitsPrefixBeforeNativeReset", [](GameTest& fixture) {
  for (bool compressed : {false, true}) {
    SCOPED_TRACE(compressed);
    auto pair = connectionTestTcpPair(fixture.root().logger());
    ASSERT_TRUE(pair) << pair.error().what();
    auto [outgoing, incoming] = std::move(*pair);
    ASSERT_TRUE(SocketTestAccess::setBufferSize(*outgoing, SO_SNDBUF, 16384));
    ASSERT_TRUE(SocketTestAccess::resetOnClose(*incoming));
    // A blocking native send returns its successful prefix on peer reset; the
    // next send in the same drain sees the real transport error.
    auto transport = TcpPacketSocket::open(fixture.root().logger(), outgoing);
    ASSERT_TRUE(transport) << transport.error().what();
    ASSERT_TRUE(outgoing->setNonBlocking(false));
    ASSERT_TRUE(outgoing->setTimeout(500));
    (*transport)->setCompressionStreamEnabled(compressed);
    ByteArray bytes(8 << 20, 0);
    std::uint32_t state = 0x12345678;
    for (auto& byte : bytes) {
      state ^= state << 13;
      state ^= state >> 17;
      state ^= state << 5;
      byte = static_cast<char>(state);
    }
    auto packet = std::make_shared<WorldParametersUpdatePacket>(std::move(bytes));
    packet->setCompressionMode(PacketCompressionMode::Disabled);
    ASSERT_TRUE((*transport)->sendPackets({packet}));
    ByteArray expected(SocketTestAccess::pendingPlain(**transport));
    if (compressed) {
      auto compressor = CompressionStream::create();
      ASSERT_TRUE(compressor) << compressor.error().what();
      auto encoded = (*compressor)->compress(expected);
      ASSERT_TRUE(encoded) << encoded.error().what();
      expected = std::move(*encoded);
    }
    auto drain = ConnectionTestAsync<bool>::create([&] { return (*transport)->writeData(); });
    ASSERT_TRUE(drain) << drain.error().what();
    auto cleanup = finally([&] {
      incoming->close();
      outgoing->shutdown();
    });
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    ASSERT_TRUE(incoming->waitForData(false, deadline));
    std::array<char, 1> prefix;
    ASSERT_EQ(incoming->receive(prefix), 1u);
    EXPECT_EQ(prefix[0], expected[0]);
    incoming->close();
    EXPECT_EQ((*drain)->wait_for(std::chrono::milliseconds(500)), std::future_status::ready);
    auto written = (*drain)->get();
    ASSERT_TRUE(written) << written.error().what();
    EXPECT_TRUE(*written);
    EXPECT_FALSE(outgoing->isActive());
    auto pending = compressed ? SocketTestAccess::pendingCompressed(**transport)
                              : SocketTestAccess::pendingPlain(**transport);
    ASSERT_FALSE(pending.empty());
    ASSERT_LT(pending.size(), expected.size());
    EXPECT_TRUE(std::ranges::equal(pending, expected.span().last(pending.size())));
    EXPECT_TRUE((*transport)->sentPacketsPending());
  }
});

}

const GameTestRegistration universeConnectionsLocalZeroDeadlineAndQueuedPacketAfterPeerClose(
    "UniverseConnections", "LocalZeroDeadlineAndQueuedPacketAfterPeerClose", [](GameTest&) {
  auto pair = LocalPacketSocket::openPair();
  UniverseConnection sender(std::move(pair.first));
  UniverseConnection receiver(std::move(pair.second));
  EXPECT_EQ(receiver.receiveAny(0), false);
  ASSERT_TRUE(sender.pushSingle(std::make_shared<ProtocolRequestPacket>(41)));
  EXPECT_EQ(sender.sendAll(0), true);
  sender.close();
  EXPECT_FALSE(receiver.isOpen());
  ASSERT_EQ(receiver.receiveAny(0), true);
  EXPECT_EQ(connectionTestVersion(receiver), 41u);
  EXPECT_EQ(receiver.receiveAny(0), false);
});

const GameTestRegistration universeConnectionsLocalDeliveryAndPeerCloseWakeReadiness(
    "UniverseConnections", "LocalDeliveryAndPeerCloseWakeReadiness", [](GameTest&) {
  auto pair = LocalPacketSocket::openPair();
  UniverseConnection sender(std::move(pair.first));
  UniverseConnection receiver(std::move(pair.second));
  std::promise<void> starting;
  auto started = starting.get_future();
  auto receiveTask = ConnectionTestAsync<bool>::create([&] {
    starting.set_value();
    return receiver.receiveAny(2000);
  });
  ASSERT_TRUE(receiveTask) << receiveTask.error().what();
  auto& received = **receiveTask;
  started.wait();
  ASSERT_TRUE(sender.pushSingle(std::make_shared<ProtocolRequestPacket>(42)));
  EXPECT_EQ(sender.sendAll(0), true);
  EXPECT_EQ(received.wait_for(std::chrono::milliseconds(500)), std::future_status::ready);
  EXPECT_EQ(received.get(), true);
  auto packet = receiver.pullSingle();
  ASSERT_TRUE(packet) << packet.error().what();
  ASSERT_TRUE(*packet);

  auto closeTask = ConnectionTestAsync<bool>::create([&] { return receiver.receiveAny(2000); });
  ASSERT_TRUE(closeTask) << closeTask.error().what();
  auto& closed = **closeTask;
  sender.close();
  EXPECT_EQ(closed.wait_for(std::chrono::milliseconds(500)), std::future_status::ready);
  EXPECT_EQ(closed.get(), false);
});

const GameTestRegistration universeConnectionsLocalCloseDoesNotWaitForReceiveDeadline(
    "UniverseConnections", "LocalCloseDoesNotWaitForReceiveDeadline", [](GameTest&) {
  auto pair = LocalPacketSocket::openPair();
  UniverseConnection receiver(std::move(pair.first));
  std::promise<void> starting;
  auto started = starting.get_future();
  auto receiveTask = ConnectionTestAsync<bool>::create([&] {
    starting.set_value();
    return receiver.receiveAny(2000);
  });
  ASSERT_TRUE(receiveTask) << receiveTask.error().what();
  auto& received = **receiveTask;
  started.wait();
  EXPECT_EQ(received.wait_for(std::chrono::milliseconds(20)), std::future_status::timeout);
  auto before = std::chrono::steady_clock::now();
  receiver.close();
  EXPECT_LT(std::chrono::steady_clock::now() - before, std::chrono::milliseconds(500));
  EXPECT_EQ(received.wait_for(std::chrono::milliseconds(500)), std::future_status::ready);
  EXPECT_EQ(received.get(), false);
});

const GameTestRegistration universeConnectionsTcpPartialHeaderBodyAndZeroDeadline(
    "UniverseConnections", "TcpPartialHeaderBodyAndZeroDeadline", [](GameTest& fixture) {
  auto pair = connectionTestTcpPair(fixture.root().logger());
  ASSERT_TRUE(pair) << pair.error().what();
  auto [sender, socket] = std::move(*pair);
  auto packetSocket = TcpPacketSocket::open(fixture.root().logger(), socket);
  ASSERT_TRUE(packetSocket) << packetSocket.error().what();
  UniverseConnection receiver(std::move(*packetSocket));
  auto frameResult = connectionTestFrame(43);
  ASSERT_TRUE(frameResult) << frameResult.error().what();
  auto& frame = *frameResult;
  ASSERT_GT(frame.size(), 2u);
  ASSERT_EQ(sender->send(std::span<char const>(frame).first(1)), 1u);
  ASSERT_TRUE(socket->waitForData(false, std::chrono::steady_clock::now() + std::chrono::seconds(2)));
  EXPECT_EQ(receiver.receiveAny(0), false);
  ASSERT_EQ(sender->send(std::span<char const>(frame).subspan(1, frame.size() - 2)), frame.size() - 2);
  ASSERT_TRUE(socket->waitForData(false, std::chrono::steady_clock::now() + std::chrono::seconds(2)));
  EXPECT_EQ(receiver.receiveAny(0), false);
  ASSERT_EQ(sender->send(std::span<char const>(frame).last(1)), 1u);
  ASSERT_TRUE(socket->waitForData(false, std::chrono::steady_clock::now() + std::chrono::seconds(2)));
  ASSERT_EQ(receiver.receiveAny(0), true);
  EXPECT_EQ(connectionTestVersion(receiver), 43u);
  EXPECT_EQ(receiver.receiveAny(0), false);
});

const GameTestRegistration universeConnectionsTcpCompletePacketBeforeEofAndTruncatedPacketAtEof(
    "UniverseConnections", "TcpCompletePacketBeforeEofAndTruncatedPacketAtEof", [](GameTest& fixture) {
  {
    auto pair = connectionTestTcpPair(fixture.root().logger());
    ASSERT_TRUE(pair) << pair.error().what();
    auto [sender, socket] = std::move(*pair);
    auto packetSocket = TcpPacketSocket::open(fixture.root().logger(), socket);
    ASSERT_TRUE(packetSocket) << packetSocket.error().what();
    UniverseConnection receiver(std::move(*packetSocket));
    auto frameResult = connectionTestFrame(44);
    ASSERT_TRUE(frameResult) << frameResult.error().what();
    auto& frame = *frameResult;
    ASSERT_EQ(sender->send(frame), frame.size());
    sender->close();
    ASSERT_EQ(receiver.receiveAny(1000), true);
    EXPECT_EQ(connectionTestVersion(receiver), 44u);
    EXPECT_EQ(receiver.receiveAny(1000), false);
    EXPECT_FALSE(receiver.isOpen());
  }
  {
    auto pair = connectionTestTcpPair(fixture.root().logger());
    ASSERT_TRUE(pair) << pair.error().what();
    auto [sender, socket] = std::move(*pair);
    auto packetSocket = TcpPacketSocket::open(fixture.root().logger(), socket);
    ASSERT_TRUE(packetSocket) << packetSocket.error().what();
    UniverseConnection receiver(std::move(*packetSocket));
    auto frameResult = connectionTestFrame(45);
    ASSERT_TRUE(frameResult) << frameResult.error().what();
    auto& frame = *frameResult;
    ASSERT_EQ(sender->send(std::span<char const>(frame).first(frame.size() - 1)), frame.size() - 1);
    sender->close();
    EXPECT_EQ(receiver.receiveAny(1000), false);
    EXPECT_FALSE(receiver.isOpen());
    auto packet = receiver.pullSingle();
    ASSERT_TRUE(packet) << packet.error().what();
    EXPECT_FALSE(*packet);
  }
});

const GameTestRegistration universeConnectionsTcpCloseWakesNativeReadinessWithoutHoldingConnectionLock(
    "UniverseConnections", "TcpCloseWakesNativeReadinessWithoutHoldingConnectionLock", [](GameTest& fixture) {
  auto pair = connectionTestTcpPair(fixture.root().logger());
  ASSERT_TRUE(pair) << pair.error().what();
  auto [sender, socket] = std::move(*pair);
  auto packetSocket = TcpPacketSocket::open(fixture.root().logger(), socket);
  ASSERT_TRUE(packetSocket) << packetSocket.error().what();
  UniverseConnection receiver(std::move(*packetSocket));
  std::promise<void> starting;
  auto started = starting.get_future();
  auto receiveTask = ConnectionTestAsync<bool>::create([&] {
    starting.set_value();
    return receiver.receiveAny(2000);
  });
  ASSERT_TRUE(receiveTask) << receiveTask.error().what();
  auto& received = **receiveTask;
  started.wait();
  EXPECT_EQ(received.wait_for(std::chrono::milliseconds(20)), std::future_status::timeout);
  auto before = std::chrono::steady_clock::now();
  receiver.close();
  EXPECT_LT(std::chrono::steady_clock::now() - before, std::chrono::milliseconds(500));
  EXPECT_EQ(received.wait_for(std::chrono::milliseconds(500)), std::future_status::ready);
  EXPECT_EQ(received.get(), false);
});

const GameTestRegistration universeConnectionsTcpCloseInterruptsBlockingReceive(
    "UniverseConnections", "TcpCloseInterruptsBlockingReceive", [](GameTest& fixture) {
  auto pair = connectionTestTcpPair(fixture.root().logger());
  ASSERT_TRUE(pair) << pair.error().what();
  auto [sender, socket] = std::move(*pair);
  ASSERT_TRUE(socket->setNonBlocking(false));
  ASSERT_TRUE(socket->setTimeout(5000));
  std::array<char, 1> buffer{};
  auto receiveTask = ConnectionTestAsync<std::size_t>::create([&] { return socket->receive(buffer); });
  ASSERT_TRUE(receiveTask) << receiveTask.error().what();
  // Peer shutdown is only a failure-cleanup escape hatch, never the wake being
  // measured. Keep it before the asynchronous receiver's joining destructor.
  auto cleanup = finally([&] {
    sender->close();
    socket->close();
  });
  auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
  bool receiving = false;
  while ((deadline - std::chrono::steady_clock::now()).count() > 0) {
    if (SocketTestAccess::operationHoldsLock(*socket)) {
      receiving = true;
      break;
    }
    std::this_thread::yield();
  }
  EXPECT_TRUE(receiving);
  if (!receiving)
    return;
  auto closeTask = ConnectionTestAsync<void>::create([&]() -> std::expected<void, StarException> {
    socket->close();
    return {};
  });
  ASSERT_TRUE(closeTask) << closeTask.error().what();
  EXPECT_EQ((*closeTask)->wait_for(std::chrono::milliseconds(500)), std::future_status::ready);
  sender->close();
  EXPECT_TRUE((*closeTask)->get());
  auto received = (*receiveTask)->get();
  if (received)
    EXPECT_EQ(*received, 0u);
  else
    EXPECT_TRUE(received.error().is<SocketClosedException>()) << received.error().what();
  EXPECT_FALSE(socket->isOpen());
});

const GameTestRegistration universeConnectionsTcpBufferedPacketSurvivesLocalClose(
    "UniverseConnections", "TcpBufferedPacketSurvivesLocalClose", [](GameTest& fixture) {
  auto pair = connectionTestTcpPair(fixture.root().logger());
  ASSERT_TRUE(pair) << pair.error().what();
  auto [sender, socket] = std::move(*pair);
  auto packetSocket = TcpPacketSocket::open(fixture.root().logger(), socket);
  ASSERT_TRUE(packetSocket) << packetSocket.error().what();
  auto frame = connectionTestFrame(49);
  ASSERT_TRUE(frame) << frame.error().what();
  ASSERT_EQ(sender->send(*frame), frame->size());
  ASSERT_TRUE(socket->waitForData(false, std::chrono::steady_clock::now() + std::chrono::seconds(2)));
  ASSERT_EQ((*packetSocket)->readData(), true);

  UniverseConnection receiver(std::move(*packetSocket));
  receiver.close();
  EXPECT_FALSE(receiver.isOpen());
  ASSERT_EQ(receiver.receiveAny(0), true);
  EXPECT_EQ(connectionTestVersion(receiver), 49u);
  EXPECT_EQ(receiver.receiveAny(0), false);
});

const GameTestRegistration universeConnectionsTcpShutdownWakesEveryNativeReadinessWaiter(
    "UniverseConnections", "TcpShutdownWakesEveryNativeReadinessWaiter", [](GameTest& fixture) {
  auto pair = connectionTestTcpPair(fixture.root().logger());
  ASSERT_TRUE(pair) << pair.error().what();
  auto [sender, socket] = std::move(*pair);
  auto wait = [&] {
    return socket->waitForData(false, std::chrono::steady_clock::now() + std::chrono::seconds(2));
  };
  auto firstTask = ConnectionTestAsync<void>::create(wait);
  ASSERT_TRUE(firstTask) << firstTask.error().what();
  auto& first = **firstTask;
  auto secondTask = ConnectionTestAsync<void>::create(wait);
  ASSERT_TRUE(secondTask) << secondTask.error().what();
  auto& second = **secondTask;
  EXPECT_EQ(first.wait_for(std::chrono::milliseconds(20)), std::future_status::timeout);
  EXPECT_EQ(second.wait_for(std::chrono::milliseconds(20)), std::future_status::timeout);
  auto before = std::chrono::steady_clock::now();
  socket->shutdown();
  EXPECT_LT(std::chrono::steady_clock::now() - before, std::chrono::milliseconds(500));
  EXPECT_EQ(first.wait_for(std::chrono::milliseconds(500)), std::future_status::ready);
  EXPECT_EQ(second.wait_for(std::chrono::milliseconds(500)), std::future_status::ready);
  EXPECT_TRUE(first.get());
  EXPECT_TRUE(second.get());
  before = std::chrono::steady_clock::now();
  EXPECT_TRUE(socket->waitForData(false, before + std::chrono::seconds(2)));
  EXPECT_LT(std::chrono::steady_clock::now() - before, std::chrono::milliseconds(500));
});

const GameTestRegistration universeConnectionsTcpPeerCloseWakesReceiveDeadline(
    "UniverseConnections", "TcpPeerCloseWakesReceiveDeadline", [](GameTest& fixture) {
  auto pair = connectionTestTcpPair(fixture.root().logger());
  ASSERT_TRUE(pair) << pair.error().what();
  auto [sender, socket] = std::move(*pair);
  auto packetSocket = TcpPacketSocket::open(fixture.root().logger(), socket);
  ASSERT_TRUE(packetSocket) << packetSocket.error().what();
  UniverseConnection receiver(std::move(*packetSocket));
  auto receiveTask = ConnectionTestAsync<bool>::create([&] { return receiver.receiveAny(2000); });
  ASSERT_TRUE(receiveTask) << receiveTask.error().what();
  auto& received = **receiveTask;
  EXPECT_EQ(received.wait_for(std::chrono::milliseconds(20)), std::future_status::timeout);
  sender->close();
  EXPECT_EQ(received.wait_for(std::chrono::milliseconds(500)), std::future_status::ready);
  EXPECT_EQ(received.get(), false);
  EXPECT_FALSE(receiver.isOpen());
});

const GameTestRegistration universeConnectionsLocalAndTcpReadinessRespectFiniteDeadline(
    "UniverseConnections", "LocalAndTcpReadinessRespectFiniteDeadline", [](GameTest& fixture) {
  {
    auto pair = LocalPacketSocket::openPair();
    UniverseConnection receiver(std::move(pair.first));
    auto before = std::chrono::steady_clock::now();
    EXPECT_EQ(receiver.receiveAny(25), false);
    auto elapsed = std::chrono::steady_clock::now() - before;
    EXPECT_GE(elapsed, std::chrono::milliseconds(20));
    EXPECT_LT(elapsed, std::chrono::milliseconds(500));
  }
  {
    auto pair = connectionTestTcpPair(fixture.root().logger());
    ASSERT_TRUE(pair) << pair.error().what();
    auto [sender, socket] = std::move(*pair);
    auto packetSocket = TcpPacketSocket::open(fixture.root().logger(), socket);
    ASSERT_TRUE(packetSocket) << packetSocket.error().what();
    UniverseConnection receiver(std::move(*packetSocket));
    auto before = std::chrono::steady_clock::now();
    EXPECT_EQ(receiver.receiveAny(25), false);
    auto elapsed = std::chrono::steady_clock::now() - before;
    EXPECT_GE(elapsed, std::chrono::milliseconds(20));
    EXPECT_LT(elapsed, std::chrono::milliseconds(500));
  }
});

const GameTestRegistration universeConnectionsTcpCompressedBackpressureRetainsAndDrainsPendingTail(
    "UniverseConnections", "TcpCompressedBackpressureRetainsAndDrainsPendingTail", [](GameTest& fixture) {
  auto pair = connectionTestTcpPair(fixture.root().logger());
  ASSERT_TRUE(pair) << pair.error().what();
  auto [outgoing, incoming] = std::move(*pair);
  ASSERT_TRUE(SocketTestAccess::setBufferSize(*outgoing, SO_SNDBUF, 16384));
  ASSERT_TRUE(SocketTestAccess::setBufferSize(*incoming, SO_RCVBUF, 65536));
  auto sendSocket = TcpPacketSocket::open(fixture.root().logger(), outgoing);
  ASSERT_TRUE(sendSocket) << sendSocket.error().what();
  auto& sendTransport = **sendSocket;
  (*sendSocket)->setCompressionStreamEnabled(true);
  auto receiveSocket = TcpPacketSocket::open(fixture.root().logger(), incoming);
  ASSERT_TRUE(receiveSocket) << receiveSocket.error().what();
  (*receiveSocket)->setCompressionStreamEnabled(true);
  UniverseConnection sender(std::move(*sendSocket));
  UniverseConnection receiver(std::move(*receiveSocket));
  auto nextPayload = [](std::uint32_t& state) {
    ByteArray payload(256 << 10, 0);
    for (auto& byte : payload) {
      state ^= state << 13;
      state ^= state >> 17;
      state ^= state << 5;
      byte = static_cast<char>(state);
    }
    return payload;
  };
  std::uint32_t sendState = 0x12345678;
  std::size_t packetCount = 0;
  bool backpressured = false;
  // Stop on observed native backpressure, not an assumed OS buffer capacity.
  // The cap is a runaway-fixture guard; buffers above are local to this pair.
  while (!backpressured && packetCount < 512) {
    ASSERT_TRUE(sender.pushSingle(std::make_shared<ClientContextUpdatePacket>(nextPayload(sendState))));
    ++packetCount;
    auto sent = sender.sendAll(0);
    ASSERT_TRUE(sent) << sent.error().what();
    backpressured = !*sent;
  }
  ASSERT_TRUE(backpressured) << "Native socket never reported backpressure after " << packetCount << " packets";
  ASSERT_TRUE(sendTransport.sentPacketsPending());
  // Already-compressed bytes retain their wire encoding even if no new stream
  // compression is requested. Disabling the mode must not hide that tail.
  sendTransport.setCompressionStreamEnabled(false);
  EXPECT_TRUE(sendTransport.sentPacketsPending());
  auto receiveTask = ConnectionTestAsync<bool>::create([&]() -> std::expected<bool, StarException> {
    std::uint32_t receiveState = 0x12345678;
    for (std::size_t i = 0; i < packetCount; ++i) {
      auto ready = receiver.receiveAny(5000);
      if (!ready)
        return std::unexpected(std::move(ready.error()));
      if (!*ready)
        return false;
      auto incoming = receiver.pullSingle();
      if (!incoming)
        return std::unexpected(std::move(incoming.error()));
      auto packet = convert<ClientContextUpdatePacket>(*incoming);
      if (!packet)
        return std::unexpected(std::move(packet.error()));
      if ((*packet)->updateData != nextPayload(receiveState))
        return false;
    }
    return true;
  });
  ASSERT_TRUE(receiveTask) << receiveTask.error().what();
  auto& received = **receiveTask;
  EXPECT_EQ(sender.sendAll(5000), true);
  EXPECT_EQ(received.get(), true);
  EXPECT_FALSE(sendTransport.sentPacketsPending());
});

const GameTestRegistration universeConnectionsShardCallbacksReenterAndRemovalReturnsUnprocessedQueues(
    "UniverseConnections", "ShardCallbacksReenterAndRemovalReturnsUnprocessedQueues", [](GameTest& fixture) {
  std::promise<void> entered;
  auto callbackEntered = entered.get_future();
  std::promise<void> release;
  auto callbackRelease = release.get_future().share();
  std::atomic<unsigned> calls = 0;
  std::atomic<unsigned> active = 0;
  auto server = UniverseConnectionServer::create(fixture.root().logger(), [&](UniverseConnectionServer& server, ConnectionId id,
      List<std::shared_ptr<Packet>> packets) -> std::expected<void, StarException> {
    if (id == 2) {
      EXPECT_EQ(active.fetch_add(1), 0u);
      auto leave = finally([&]() { --active; });
      if (calls++ == 0) {
        EXPECT_TRUE(server.hasConnection(id));
        auto connection = server.removeConnection(id);
        if (!connection)
          return std::unexpected(std::move(connection.error()));
        auto added = server.addConnection(id, std::move(*connection));
        if (!added)
          return std::unexpected(std::move(added.error()));
        entered.set_value();
        callbackRelease.wait_for(std::chrono::seconds(2));
      }
      return server.sendPackets(id, std::move(packets));
    }
    return server.sendPackets(id, std::move(packets));
  }, 2);
  ASSERT_TRUE(server) << server.error().what();
  auto gatePair = LocalPacketSocket::openPair();
  ASSERT_TRUE((*server)->addConnection(2, UniverseConnection(std::move(gatePair.first))));
  UniverseConnection gateClient(std::move(gatePair.second));
  ASSERT_TRUE(gateClient.pushSingle(std::make_shared<ProtocolRequestPacket>(1)));
  ASSERT_EQ(gateClient.sendAll(0), true);
  ASSERT_EQ(callbackEntered.wait_for(std::chrono::seconds(1)), std::future_status::ready);

  // Same shard: the worker is inside the callback, so neither queue can have
  // been processed before removal. The returned connection owns both queues.
  auto queuedPair = LocalPacketSocket::openPair();
  UniverseConnection queued(std::move(queuedPair.first));
  UniverseConnection queuedClient(std::move(queuedPair.second));
  ASSERT_TRUE(queued.pushSingle(std::make_shared<ProtocolRequestPacket>(46)));
  ASSERT_TRUE(queuedClient.pushSingle(std::make_shared<ProtocolRequestPacket>(47)));
  ASSERT_EQ(queuedClient.sendAll(0), true);
  ASSERT_EQ(queued.receiveAny(0), true);
  ASSERT_TRUE((*server)->addConnection(4, std::move(queued)));
  auto returned = (*server)->removeConnection(4);
  ASSERT_TRUE(returned) << returned.error().what();
  ASSERT_EQ(returned->receiveAny(0), true);
  EXPECT_EQ(connectionTestVersion(*returned), 47u);
  ASSERT_EQ(returned->sendAll(0), true);
  ASSERT_EQ(queuedClient.receiveAny(0), true);
  EXPECT_EQ(connectionTestVersion(queuedClient), 46u);

  // Another shard continues while the first shard is in its reentrant callback.
  auto otherPair = LocalPacketSocket::openPair();
  ASSERT_TRUE((*server)->addConnection(3, UniverseConnection(std::move(otherPair.first))));
  UniverseConnection otherClient(std::move(otherPair.second));
  ASSERT_TRUE(otherClient.pushSingle(std::make_shared<ProtocolRequestPacket>(48)));
  ASSERT_EQ(otherClient.sendAll(0), true);
  ASSERT_EQ(otherClient.receiveAny(1000), true);
  EXPECT_EQ(connectionTestVersion(otherClient), 48u);
  ASSERT_TRUE(gateClient.pushSingle(std::make_shared<ProtocolRequestPacket>(2)));
  ASSERT_EQ(gateClient.sendAll(0), true);
  EXPECT_EQ(gateClient.receiveAny(0), false);
  release.set_value();
  for (VersionNumber version : {1u, 2u}) {
    ASSERT_EQ(gateClient.receiveAny(1000), true);
    EXPECT_EQ(connectionTestVersion(gateClient), version);
  }
});

#ifdef STAR_TEST_WRAP_PTHREAD_CREATE
const GameTestRegistration universeConnectionsStartupFailureJoinsEveryPreviouslyStartedWorker(
    "UniverseConnectionsDeathTest", "StartupFailureJoinsEveryPreviouslyStartedWorker", [](GameTest& fixture) {
  auto& logger = fixture.root().logger();
  auto oldStyle = testing::GTEST_FLAG(death_test_style);
  testing::GTEST_FLAG(death_test_style) = "threadsafe";
  EXPECT_EXIT(([&logger] {
    // A regression must fail this subprocess, not hang the whole asset suite.
    alarm(5);
    for (int failAfter : {0, 1, 3}) {
      std::atomic<unsigned> completed = 0;
      completedInjectedThreads = std::ref(completed);
      threadStartsBeforeFailure = failAfter;
      auto server = UniverseConnectionServer::create(logger,
          [](auto&, auto, auto) -> std::expected<void, StarException> { return {}; }, 4);
      threadStartsBeforeFailure = -1;
      completedInjectedThreads.reset();
      if (server || completed.load() != static_cast<unsigned>(failAfter))
        std::_Exit(1);
    }
    // Injection resets on failure; a subsequent ordinary startup still works.
    {
      auto server = UniverseConnectionServer::create(logger,
          [](auto&, auto, auto) -> std::expected<void, StarException> { return {}; }, 2);
      if (!server)
        std::_Exit(1);
    }
    std::_Exit(0);
  }()), testing::ExitedWithCode(0), "");
  testing::GTEST_FLAG(death_test_style) = oldStyle;
});
#endif

namespace {

template <typename ResponsePacket> void messageResponseWireCompatibility() {
  auto uuid = Uuid::create("00112233445566778899aabbccddeeff");
  ASSERT_TRUE(uuid);

  ResponsePacket defaultPacket;
  ASSERT_FALSE(defaultPacket.response.has_value());
  EXPECT_TRUE(defaultPacket.response.error().empty());
  DataStreamBuffer defaultWire;
  ASSERT_TRUE(defaultPacket.write(defaultWire));
  DataStreamBuffer expectedDefault;
  ASSERT_TRUE(expectedDefault.write<std::uint8_t>(1));
  ASSERT_TRUE(expectedDefault.write(String{}));
  ASSERT_TRUE(expectedDefault.write(defaultPacket.uuid));
  EXPECT_EQ(defaultWire.data(), expectedDefault.data());

  std::array<std::expected<Json, String>, 4> responses{std::unexpected(String{}),
                                                       std::unexpected(String("message failed")), Json(),
                                                       Json(JsonObject{{"answer", 42}, {"message", "received"}})};
  for (auto const& response : responses) {
    ResponsePacket packet(response, *uuid);
    DataStreamBuffer wire;
    ASSERT_TRUE(packet.write(wire));

    // These are the pre-migration domain bytes, not a generic expected codec.
    DataStreamBuffer legacyWire;
    ASSERT_TRUE(legacyWire.write<std::uint8_t>(response.has_value() ? 2 : 1));
    if (response.has_value())
      ASSERT_TRUE(legacyWire.write(*response));
    else
      ASSERT_TRUE(legacyWire.write(response.error()));
    ASSERT_TRUE(legacyWire.write(*uuid));
    EXPECT_EQ(wire.data(), legacyWire.data());

    ResponsePacket decoded;
    ASSERT_TRUE(wire.seek(0));
    ASSERT_TRUE(decoded.read(wire));
    EXPECT_EQ(decoded.response, response);
    EXPECT_EQ(decoded.uuid, *uuid);
    auto atEnd = wire.at_end();
    ASSERT_TRUE(atEnd);
    EXPECT_TRUE(*atEnd);
  }
}

template <typename ResponsePacket> void messageResponseUnknownTags() {
  auto uuid = Uuid::create("00112233445566778899aabbccddeeff");
  ASSERT_TRUE(uuid);
  std::array<std::expected<Json, String>, 3> previousResponses{
    std::unexpected(String{}), std::unexpected(String("previous error")), Json("previous success")};
  for (std::uint8_t tag : {0, 3, 255}) {
    DataStreamBuffer wire;
    ASSERT_TRUE(wire.write(tag));
    ASSERT_TRUE(wire.write(*uuid));
    for (auto const& previous : previousResponses) {
      ResponsePacket decoded(previous, Uuid::nil());
      ASSERT_TRUE(wire.seek(0));
      ASSERT_TRUE(decoded.read(wire));
      EXPECT_EQ(decoded.response, previous);
      EXPECT_EQ(decoded.uuid, *uuid);
      auto atEnd = wire.at_end();
      ASSERT_TRUE(atEnd);
      EXPECT_TRUE(*atEnd);

      for (std::size_t length = 0; length < wire.size(); ++length) {
        SCOPED_TRACE(length);
        DataStreamExternalBuffer truncated(wire.span().first(length));
        ResponsePacket failed(previous, Uuid::nil());
        EXPECT_FALSE(failed.read(truncated));
        EXPECT_EQ(failed.response, previous);
        EXPECT_EQ(failed.uuid, Uuid::nil());
      }
    }
  }
}

template <typename ResponsePacket> void messageResponseDecodeFailures() {
  auto uuid = Uuid::create("00112233445566778899aabbccddeeff");
  ASSERT_TRUE(uuid);
  std::array<std::expected<Json, String>, 2> responses{std::unexpected(String("new error")),
                                                       Json(JsonObject{{"new", JsonArray{1, 2, 3}}})};
  std::array<std::expected<Json, String>, 3> previousResponses{
    std::unexpected(String{}), std::unexpected(String("previous error")), Json("previous success")};
  for (auto const& response : responses) {
    DataStreamBuffer wire;
    ASSERT_TRUE(wire.write<std::uint8_t>(response.has_value() ? 2 : 1));
    if (response.has_value())
      ASSERT_TRUE(wire.write(*response));
    else
      ASSERT_TRUE(wire.write(response.error()));
    auto payloadEnd = wire.size();
    ASSERT_TRUE(wire.write(*uuid));

    for (auto const& previous : previousResponses) {
      for (std::size_t length = 0; length < wire.size(); ++length) {
        SCOPED_TRACE(length);
        DataStreamExternalBuffer truncated(wire.span().first(length));
        ResponsePacket decoded(previous, Uuid::nil());
        EXPECT_FALSE(decoded.read(truncated));
        // Payload failure leaves the response intact; a complete payload is
        // committed even when the following UUID fails to decode.
        EXPECT_EQ(decoded.response, length < payloadEnd ? previous : response);
        EXPECT_EQ(decoded.uuid, Uuid::nil());
      }
    }
  }
}

}// namespace

const GameTestRegistration
  entityMessageResponseWireCompatibility("MessageResponsePackets", "EntityWireCompatibility", [](GameTest&) {
    messageResponseWireCompatibility<EntityMessageResponsePacket>();
  });

const GameTestRegistration
  universeMessageResponseWireCompatibility("MessageResponsePackets", "UniverseWireCompatibility", [](GameTest&) {
    messageResponseWireCompatibility<UniverseMessageResponse>();
  });

const GameTestRegistration entityMessageResponseUnknownTags("MessageResponsePackets", "EntityUnknownTags",
                                                            [](GameTest&) {
                                                              messageResponseUnknownTags<EntityMessageResponsePacket>();
                                                            });

const GameTestRegistration universeMessageResponseUnknownTags("MessageResponsePackets", "UniverseUnknownTags",
                                                              [](GameTest&) {
                                                                messageResponseUnknownTags<UniverseMessageResponse>();
                                                              });

const GameTestRegistration
  entityMessageResponseDecodeFailures("MessageResponsePackets", "EntityDecodeFailures",
                                      [](GameTest&) { messageResponseDecodeFailures<EntityMessageResponsePacket>(); });

const GameTestRegistration
  universeMessageResponseDecodeFailures("MessageResponsePackets", "UniverseDecodeFailures",
                                        [](GameTest&) { messageResponseDecodeFailures<UniverseMessageResponse>(); });

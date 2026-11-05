module;
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
import star.map;
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


namespace Star {
struct Packet;
}

import star.net_packet_socket;

namespace Star {

class Logger;

class UniverseConnectionServer;

struct UniverseConnectionExceptionTag {
  static constexpr std::string_view name() { return "UniverseConnectionException"; }
};
using UniverseConnectionException = StarError<UniverseConnectionExceptionTag, StarException>;

// Symmetric NetPacket based connection between the UniverseServer and the
// UniverseClient.
class UniverseConnection {
public:
  explicit UniverseConnection(std::unique_ptr<PacketSocket> packetSocket);
  UniverseConnection(UniverseConnection&& rhs);
  ~UniverseConnection();

  UniverseConnection& operator=(UniverseConnection&& rhs);

  bool isOpen() const;
  void close();

  // Push packets onto the send queue.
  [[nodiscard]] std::expected<void, StarException> push(List<std::shared_ptr<Packet>> packets);
  [[nodiscard]] std::expected<void, StarException> pushSingle(std::shared_ptr<Packet> packet);

  // Pull packets from the receive queue.
  [[nodiscard]] std::expected<List<std::shared_ptr<Packet>>, StarException> pull();
  [[nodiscard]] std::expected<std::shared_ptr<Packet>, StarException> pullSingle();

  // Send all data that we can without blocking, returns true if any data was
  // sent.
  [[nodiscard]] std::expected<bool, StarException> send();

  // Block, trying to send the entire send queue before the given timeout.
  // Returns true if the entire send queue was sent before the timeout, false
  // otherwise.
  [[nodiscard]] std::expected<bool, StarException> sendAll(unsigned timeout);

  // Receive all the data that we can without blocking, returns true if any
  // data was received.
  [[nodiscard]] std::expected<bool, StarException> receive();

  // Block, trying to read at least one packet into the receive queue before
  // the timeout.  Returns true once any packets are on the receive queue,
  // false if the timeout was reached with no packets receivable.
  [[nodiscard]] std::expected<bool, StarException> receiveAny(unsigned timeout);

  // Returns a reference to the packet socket.
  [[nodiscard]] std::expected<std::reference_wrapper<PacketSocket>, StarException> packetSocket();

  // Packet stats for the most recent one second window of activity incoming
  // and outgoing.  Will only return valid stats if the underlying PacketSocket
  // implements stat collection.
  std::optional<PacketStats> incomingStats() const;
  std::optional<PacketStats> outgoingStats() const;

private:
  friend class UniverseConnectionServer;

  UniverseConnection() = default;

  mutable std::mutex m_mutex;
  std::unique_ptr<PacketSocket> m_packetSocket;
  List<std::shared_ptr<Packet>> m_sendQueue;
  Deque<std::shared_ptr<Packet>> m_receiveQueue;
};

// Manage a set of UniverseConnections cheaply and in an asynchronous way.
// Uses multiple background threads to handle remote sending and receiving.
class UniverseConnectionServer {
public:
  // The packet receive callback is called asynchronously on every packet group
  // received.  It may recursively call connection-management methods without
  // deadlocking, but not the owner-side close().  The receive callback will not
  // be called for any client until the previous callback for that client is
  // complete.
  typedef std::function<std::expected<void, StarException>(UniverseConnectionServer&, ConnectionId, List<std::shared_ptr<Packet>>)> PacketReceiveCallback;

  [[nodiscard]] static std::expected<std::shared_ptr<UniverseConnectionServer>, StarException> create(Logger& logger, PacketReceiveCallback packetReceiver, std::size_t numWorkerThreads = 0);
  ~UniverseConnectionServer();

  // Serialized owner-side drain; never call while holding locks needed by the
  // packet callback.  A worker diagnostic can accompany a completed drain.
  // Borrowed callback state must remain alive until isClosed() is true.
  [[nodiscard]] std::expected<void, StarException> close();
  bool isClosed() const;

  bool hasConnection(ConnectionId clientId) const;
  List<ConnectionId> allConnections() const;
  [[nodiscard]] std::expected<bool, StarException> connectionIsOpen(ConnectionId clientId) const;
  [[nodiscard]] std::expected<std::chrono::time_point<std::chrono::steady_clock, std::chrono::milliseconds>, StarException> lastActivityTime(ConnectionId clientId) const;

  [[nodiscard]] std::expected<void, StarException> addConnection(ConnectionId clientId, UniverseConnection connection);
  [[nodiscard]] std::expected<UniverseConnection, StarException> removeConnection(ConnectionId clientId);
  List<UniverseConnection> removeAllConnections();

  [[nodiscard]] std::expected<void, StarException> sendPackets(ConnectionId clientId, List<std::shared_ptr<Packet>> packets);

  // Get total packets processed across all worker threads
  std::uint64_t totalPacketsProcessed() const;
  // Get number of worker threads
  std::size_t numWorkerThreads() const;

private:
  UniverseConnectionServer(Logger& logger, PacketReceiveCallback packetReceiver, std::size_t numWorkerThreads);
  [[nodiscard]] std::expected<void, StarException> initialize();
  [[nodiscard]] std::expected<void, StarException> processConnections(std::size_t shard, std::stop_token stopToken);
  struct Connection {
    std::mutex mutex;
    std::unique_ptr<PacketSocket> packetSocket;
    List<std::shared_ptr<Packet>> sendQueue;
    Deque<std::shared_ptr<Packet>> receiveQueue;
    std::chrono::time_point<std::chrono::steady_clock, std::chrono::milliseconds> lastActivityTime;
  };

  struct WorkerStats {
    std::atomic<std::uint64_t> packetsProcessed{0};
    std::atomic<std::uint64_t> bytesReceived{0};
    std::atomic<std::uint64_t> bytesSent{0};
    std::atomic<std::uint64_t> connectionsHandled{0};

    WorkerStats() = default;
    WorkerStats(WorkerStats&& other) noexcept : packetsProcessed(other.packetsProcessed.load()), bytesReceived(other.bytesReceived.load()), bytesSent(other.bytesSent.load()), connectionsHandled(other.connectionsHandled.load()) {
                                                };
    WorkerStats(const WorkerStats&) = delete;
    WorkerStats& operator=(WorkerStats&& other) noexcept {
      if (this != &other) {
        packetsProcessed = other.packetsProcessed.load();
        bytesReceived = other.bytesReceived.load();
        bytesSent = other.bytesSent.load();
        connectionsHandled = other.connectionsHandled.load();
      }
      return *this;
    };
    WorkerStats& operator=(const WorkerStats&) = delete;
  };

  // The logger outlives this server and its joined processing threads.
  Logger& m_logger;

  PacketReceiveCallback const m_packetReceiver;

  mutable std::recursive_mutex m_connectionsMutex;
  // The registry itself is partitioned; workers never snapshot other shards.
  List<HashMap<ConnectionId, std::shared_ptr<Connection>>> m_connections;

  std::stop_source m_stopSource{std::nostopstate};
  std::atomic<bool> m_closed{false};
  std::size_t m_numWorkerThreads;
  std::mutex m_workMutex;
  std::condition_variable m_workCondition;
  List<WorkerStats> m_workerStats;
  List<ThreadFunction<void>> m_processingThreads;
};

}// namespace Star

export module star.universe_connection;

export namespace Star {
  using ::Star::UniverseConnectionExceptionTag;
  using ::Star::UniverseConnectionException;
  using ::Star::UniverseConnection;
  using ::Star::UniverseConnectionServer;
}

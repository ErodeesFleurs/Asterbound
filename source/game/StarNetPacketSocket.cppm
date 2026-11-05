module;

import star.format;
import star.list;
import star.io_device;
import star.string;
import star.either;
import star.host_address;
import star.thread;
import star.socket;
import star.tcp;
import std;
import star.type_identity;

import star.p2p_networking_service;
import star.byte_array;
import star.map;
import star.data_stream_devices;

typedef struct ZSTD_CCtx_s ZSTD_CCtx;
typedef struct ZSTD_DCtx_s ZSTD_DCtx;
typedef ZSTD_DCtx ZSTD_DStream;
typedef ZSTD_CCtx ZSTD_CStream;
import star.zstd_compression;
import star.net_compatibility;

namespace Star {

class Logger;
struct Packet;
enum class PacketType : std::uint8_t;

struct PacketStats {
  HashMap<PacketType, float> packetBytesPerSecond;
  float bytesPerSecond;
  PacketType worstPacketType;
  std::size_t worstPacketSize;
};

// Collects PacketStats over a given window of time.
class PacketStatCollector {
public:
  PacketStatCollector();
  PacketStatCollector(float calculationWindow);

  void mix(std::size_t size);
  void mix(PacketType type, std::size_t size, bool addToTotal = true);
  void mix(HashMap<PacketType, std::size_t> const& sizes, bool addToTotal = true);

  // Should always return packet statistics for the most recent completed
  // window of time
  PacketStats stats() const;

private:
  void calculate();

  std::chrono::duration<float> m_calculationWindow;
  PacketStats m_stats;
  Map<PacketType, float> m_unmixed;
  std::size_t m_totalBytes;
  std::chrono::time_point<std::chrono::steady_clock, std::chrono::milliseconds> m_lastMixTime;
};

// Interface for bidirectional communication using NetPackets, based around a
// simple non-blocking polling interface.  Communication is assumed to be done
// via writeData() and readData(), and any delay in calling writeData or
// readData may translate directly into increased latency.
class PacketSocket {
public:
  virtual void const* queryType(TypeKey key) const noexcept { return queryTypeOf<PacketSocket>(this, key); }
  virtual std::string_view dynamicTypeName() const noexcept { return Star::typeName<PacketSocket>(); }

  virtual ~PacketSocket() = default;

  virtual bool isOpen() const = 0;
  virtual void close() = 0;

  // Takes all packets from the given list and queues them for sending.
  [[nodiscard]] virtual std::expected<void, StarException> sendPackets(List<std::shared_ptr<Packet>> packets) = 0;
  // Receives any packets from the incoming queue, if available
  [[nodiscard]] virtual std::expected<List<std::shared_ptr<Packet>>, StarException> receivePackets() = 0;

  // Returns true if any sent packets on the queue are still not completely
  // written.
  virtual bool sentPacketsPending() const = 0;

  // Write all data possible without blocking, returns true if any data was
  // actually written.
  [[nodiscard]] virtual std::expected<bool, StarException> writeData() = 0;
  // Read all data available without blocking, returns true if any data was
  // actually received.
  [[nodiscard]] virtual std::expected<bool, StarException> readData() = 0;

  // Called after a nonblocking I/O attempt found no complete packet / pending
  // output. May return spuriously; callers retry I/O before checking the deadline.
  // Safe concurrently with close(), and never holds the caller's connection lock.
  [[nodiscard]] virtual std::expected<void, StarException> waitForData(bool writable, std::chrono::steady_clock::time_point deadline) = 0;

  // Should return incoming / outgoing packet stats, if they are tracked.
  // Default implementations return nothing.
  virtual std::optional<PacketStats> incomingStats() const;
  virtual std::optional<PacketStats> outgoingStats() const;

  virtual void setNetRules(NetCompatibilityRules netRules);
  virtual NetCompatibilityRules netRules() const;

private:
  NetCompatibilityRules m_netRules;
};

class CompressedPacketSocket : public PacketSocket {
public:
  void const* queryType(TypeKey key) const noexcept override { return queryTypeOf<CompressedPacketSocket, PacketSocket>(this, key); }
  std::string_view dynamicTypeName() const noexcept override { return Star::typeName<CompressedPacketSocket>(); }

  virtual ~CompressedPacketSocket() = default;

  virtual void setCompressionStreamEnabled(bool enabled);
  virtual bool compressionStreamEnabled() const;
private:
  bool m_useCompressionStream = false;
protected:
  [[nodiscard]] std::expected<void, StarException> initializeCompression();
  std::unique_ptr<CompressionStream> m_compressionStream;
  std::unique_ptr<DecompressionStream> m_decompressionStream;
};

// PacketSocket for local communication.
class LocalPacketSocket : public PacketSocket {
public:
  void const* queryType(TypeKey key) const noexcept override { return queryTypeOf<LocalPacketSocket, PacketSocket>(this, key); }
  std::string_view dynamicTypeName() const noexcept override { return Star::typeName<LocalPacketSocket>(); }

  static std::pair<std::unique_ptr<LocalPacketSocket>, std::unique_ptr<LocalPacketSocket>> openPair();
  ~LocalPacketSocket() override;

  bool isOpen() const override;
  void close() override;

  [[nodiscard]] std::expected<void, StarException> sendPackets(List<std::shared_ptr<Packet>> packets) override;
  [[nodiscard]] std::expected<List<std::shared_ptr<Packet>>, StarException> receivePackets() override;

  bool sentPacketsPending() const override;

  // write / read for local sockets is actually a no-op, sendPackets places
  // packets directly in the incoming queue of the paired local socket.
  [[nodiscard]] std::expected<bool, StarException> writeData() override;
  [[nodiscard]] std::expected<bool, StarException> readData() override;
  [[nodiscard]] std::expected<void, StarException> waitForData(bool writable, std::chrono::steady_clock::time_point deadline) override;

private:
  struct Pipe {
    std::mutex mutex;
    std::condition_variable ready;
    Deque<std::shared_ptr<Packet>> queue;
    bool open = true;
    bool peerOpen = true;
  };

  LocalPacketSocket(std::shared_ptr<Pipe> incomingPipe, std::weak_ptr<Pipe> outgoingPipe);

  std::shared_ptr<Pipe> m_incomingPipe;
  std::weak_ptr<Pipe> m_outgoingPipe;
};

// Wraps a TCP socket into a PacketSocket.
class TcpPacketSocket : public CompressedPacketSocket {
public:
  void const* queryType(TypeKey key) const noexcept override { return queryTypeOf<TcpPacketSocket, CompressedPacketSocket>(this, key); }
  std::string_view dynamicTypeName() const noexcept override { return Star::typeName<TcpPacketSocket>(); }

  [[nodiscard]] static std::expected<std::unique_ptr<TcpPacketSocket>, StarException> open(Logger& logger, std::shared_ptr<TcpSocket> socket);

  bool isOpen() const override;
  void close() override;

  [[nodiscard]] std::expected<void, StarException> sendPackets(List<std::shared_ptr<Packet>> packets) override;
  [[nodiscard]] std::expected<List<std::shared_ptr<Packet>>, StarException> receivePackets() override;

  bool sentPacketsPending() const override;

  [[nodiscard]] std::expected<bool, StarException> writeData() override;
  [[nodiscard]] std::expected<bool, StarException> readData() override;
  [[nodiscard]] std::expected<void, StarException> waitForData(bool writable, std::chrono::steady_clock::time_point deadline) override;

  std::optional<PacketStats> incomingStats() const override;
  std::optional<PacketStats> outgoingStats() const override;
private:
  TcpPacketSocket(Logger& logger, std::shared_ptr<TcpSocket> socket);

  Logger& m_logger;
  std::shared_ptr<TcpSocket> m_socket;

  PacketStatCollector m_incomingStats;
  PacketStatCollector m_outgoingStats;
  ByteArray m_outputBuffer;
  ByteArray m_inputBuffer;
  ByteArray m_compressedOutputBuffer;
};

// Wraps a P2PSocket into a PacketSocket
class P2PPacketSocket : public CompressedPacketSocket {
public:
  void const* queryType(TypeKey key) const noexcept override { return queryTypeOf<P2PPacketSocket, CompressedPacketSocket>(this, key); }
  std::string_view dynamicTypeName() const noexcept override { return Star::typeName<P2PPacketSocket>(); }

  [[nodiscard]] static std::expected<std::unique_ptr<P2PPacketSocket>, StarException> open(Logger& logger, std::unique_ptr<P2PSocket> socket);

  bool isOpen() const override;
  void close() override;

  [[nodiscard]] std::expected<void, StarException> sendPackets(List<std::shared_ptr<Packet>> packets) override;
  [[nodiscard]] std::expected<List<std::shared_ptr<Packet>>, StarException> receivePackets() override;

  bool sentPacketsPending() const override;

  [[nodiscard]] std::expected<bool, StarException> writeData() override;
  [[nodiscard]] std::expected<bool, StarException> readData() override;
  [[nodiscard]] std::expected<void, StarException> waitForData(bool writable, std::chrono::steady_clock::time_point deadline) override;

  std::optional<PacketStats> incomingStats() const override;
  std::optional<PacketStats> outgoingStats() const override;

private:
  P2PPacketSocket(Logger& logger, std::shared_ptr<P2PSocket> socket);

  Logger& m_logger;
  std::shared_ptr<P2PSocket> m_socket;
  std::mutex m_waitMutex;
  std::condition_variable m_waitCondition;
  bool m_closed = false;

  PacketStatCollector m_incomingStats;
  PacketStatCollector m_outgoingStats;
  Deque<ByteArray> m_outputMessages;
  Deque<ByteArray> m_inputMessages;
};

}

export module star.net_packet_socket;

export namespace Star {
  using ::Star::PacketStats;
  using ::Star::PacketStatCollector;
  using ::Star::CompressedPacketSocket;
  using ::Star::PacketSocket;
  using ::Star::LocalPacketSocket;
  using ::Star::TcpPacketSocket;
  using ::Star::P2PPacketSocket;
}

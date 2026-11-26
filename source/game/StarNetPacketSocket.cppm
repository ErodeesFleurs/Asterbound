module;

import star.format;
import star.list;
import star.io_device;
import star.string;
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

import star.zstd_compression;
import star.net_compatibility;

namespace Star {

class Logger;
struct Packet;
enum class PacketType : std::uint8_t;

struct PacketStats {
  HashMap<PacketType, float> packet_bytes_per_second;
  float bytes_per_second;
  PacketType worst_packet_type;
  std::size_t worst_packet_size;
};

// Collects PacketStats over a given window of time.
class PacketStatCollector {
public:
  PacketStatCollector();
  PacketStatCollector(float calculation_window);

  void mix(std::size_t size);
  void mix(PacketType type, std::size_t size, bool add_to_total = true);
  void mix(HashMap<PacketType, std::size_t> const& sizes, bool add_to_total = true);

  // Should always return packet statistics for the most recent completed
  // window of time
  PacketStats stats() const;

private:
  friend struct SocketTestAccess;
  void calculate();

  std::chrono::duration<float> m_calculation_window;
  PacketStats m_stats;
  Map<PacketType, float> m_unmixed;
  std::size_t m_total_bytes;
  std::chrono::time_point<std::chrono::steady_clock, std::chrono::milliseconds> m_last_mix_time;
};

// Interface for bidirectional communication using NetPackets, based around a
// simple non-blocking polling interface.  Communication is assumed to be done
// via write_data() and read_data(), and any delay in calling write_data or
// read_data may translate directly into increased latency.
class PacketSocket {
public:
  virtual void query_type(TypeQuery& query) const noexcept { query_type_of<PacketSocket>(*this, query); }
  virtual std::string_view dynamic_type_name() const noexcept { return Star::type_name<PacketSocket>(); }

  virtual ~PacketSocket() = default;

  virtual bool is_open() const = 0;
  virtual void close() = 0;

  // Takes all packets from the given list and queues them for sending.
  [[nodiscard]] virtual std::expected<void, StarException> send_packets(List<std::shared_ptr<Packet>> packets) = 0;
  // Receives any packets from the incoming queue, if available
  [[nodiscard]] virtual std::expected<List<std::shared_ptr<Packet>>, StarException> receive_packets() = 0;

  // Returns true if any sent packets on the queue are still not completely
  // written.
  virtual bool sent_packets_pending() const = 0;

  // Write all data possible without blocking, returns true if any data was
  // actually written.
  [[nodiscard]] virtual std::expected<bool, StarException> write_data() = 0;
  // Read all data available without blocking, returns true if any data was
  // actually received.
  [[nodiscard]] virtual std::expected<bool, StarException> read_data() = 0;

  // Called after a nonblocking I/O attempt found no complete packet / pending
  // output. May return spuriously; callers retry I/O before checking the deadline.
  // Safe concurrently with close(), and never holds the caller's connection lock.
  [[nodiscard]] virtual std::expected<void, StarException> wait_for_data(bool writable, std::chrono::steady_clock::time_point deadline) = 0;

  // Should return incoming / outgoing packet stats, if they are tracked.
  // Default implementations return nothing.
  virtual std::optional<PacketStats> incoming_stats() const;
  virtual std::optional<PacketStats> outgoing_stats() const;

  virtual void set_net_rules(NetCompatibilityRules net_rules);
  virtual NetCompatibilityRules net_rules() const;

private:
  NetCompatibilityRules m_net_rules;
};

class CompressedPacketSocket : public PacketSocket {
public:
  void query_type(TypeQuery& query) const noexcept override {
    query_type_of<CompressedPacketSocket, PacketSocket>(*this, query);
  }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<CompressedPacketSocket>(); }

  ~CompressedPacketSocket() override = default;

  virtual void set_compression_stream_enabled(bool enabled);
  virtual bool compression_stream_enabled() const;
private:
  bool m_use_compression_stream = false;
protected:
  [[nodiscard]] std::expected<void, StarException> initialize_compression();
  std::optional<CompressionStream> m_compression_stream;
  std::optional<DecompressionStream> m_decompression_stream;

  DataStreamBuffer m_packet_payload;
  ByteArray m_codec_scratch;
  HashMap<PacketType, std::size_t> m_encoded_packet_sizes;
};

// PacketSocket for local communication.
class LocalPacketSocket : public PacketSocket {
  struct ConstructionKey { explicit ConstructionKey() = default; };
  struct Pipe;

public:
  void query_type(TypeQuery& query) const noexcept override {
    query_type_of<LocalPacketSocket, PacketSocket>(*this, query);
  }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<LocalPacketSocket>(); }

  static std::pair<std::unique_ptr<LocalPacketSocket>, std::unique_ptr<LocalPacketSocket>> open_pair();
  LocalPacketSocket(ConstructionKey, std::shared_ptr<Pipe> incoming_pipe, std::weak_ptr<Pipe> outgoing_pipe);
  ~LocalPacketSocket() override;

  bool is_open() const override;
  void close() override;

  [[nodiscard]] std::expected<void, StarException> send_packets(List<std::shared_ptr<Packet>> packets) override;
  [[nodiscard]] std::expected<List<std::shared_ptr<Packet>>, StarException> receive_packets() override;

  bool sent_packets_pending() const override;

  // write / read for local sockets is actually a no-op, send_packets places
  // packets directly in the incoming queue of the paired local socket.
  [[nodiscard]] std::expected<bool, StarException> write_data() override;
  [[nodiscard]] std::expected<bool, StarException> read_data() override;
  [[nodiscard]] std::expected<void, StarException> wait_for_data(bool writable, std::chrono::steady_clock::time_point deadline) override;

private:
  struct Pipe {
    std::mutex mutex;
    std::condition_variable ready;
    Deque<std::shared_ptr<Packet>> queue;
    bool open = true;
    bool peer_open = true;
  };

  std::shared_ptr<Pipe> m_incoming_pipe;
  std::weak_ptr<Pipe> m_outgoing_pipe;
};

// Wraps a TCP socket into a PacketSocket.
class TcpPacketSocket : public CompressedPacketSocket {
  struct ConstructionKey { explicit ConstructionKey() = default; };

public:
  void query_type(TypeQuery& query) const noexcept override {
    query_type_of<TcpPacketSocket, CompressedPacketSocket>(*this, query);
  }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<TcpPacketSocket>(); }

  [[nodiscard]] static std::expected<std::unique_ptr<TcpPacketSocket>, StarException> open(Logger& logger, std::shared_ptr<TcpSocket> socket);
  TcpPacketSocket(ConstructionKey, Logger& logger, std::shared_ptr<TcpSocket> socket);

  bool is_open() const override;
  void close() override;

  [[nodiscard]] std::expected<void, StarException> send_packets(List<std::shared_ptr<Packet>> packets) override;
  [[nodiscard]] std::expected<List<std::shared_ptr<Packet>>, StarException> receive_packets() override;

  bool sent_packets_pending() const override;

  [[nodiscard]] std::expected<bool, StarException> write_data() override;
  [[nodiscard]] std::expected<bool, StarException> read_data() override;
  [[nodiscard]] std::expected<void, StarException> wait_for_data(bool writable, std::chrono::steady_clock::time_point deadline) override;

  std::optional<PacketStats> incoming_stats() const override;
  std::optional<PacketStats> outgoing_stats() const override;
private:
  friend struct SocketTestAccess;

  Logger& m_logger;
  std::shared_ptr<TcpSocket> m_socket;

  PacketStatCollector m_incoming_stats;
  PacketStatCollector m_outgoing_stats;
  ByteArray m_output_buffer;
  ByteArray m_input_buffer;
  ByteArray m_compressed_output_buffer;
  ByteArray m_compressed_input;
  bool m_decompression_pending = false;
};

// Wraps a P2PSocket into a PacketSocket
class P2PPacketSocket : public CompressedPacketSocket {
  struct ConstructionKey { explicit ConstructionKey() = default; };

public:
  void query_type(TypeQuery& query) const noexcept override {
    query_type_of<P2PPacketSocket, CompressedPacketSocket>(*this, query);
  }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<P2PPacketSocket>(); }

  [[nodiscard]] static std::expected<std::unique_ptr<P2PPacketSocket>, StarException> open(Logger& logger, std::unique_ptr<P2PSocket> socket);
  P2PPacketSocket(ConstructionKey, Logger& logger, std::unique_ptr<P2PSocket> socket);

  bool is_open() const override;
  void close() override;

  [[nodiscard]] std::expected<void, StarException> send_packets(List<std::shared_ptr<Packet>> packets) override;
  [[nodiscard]] std::expected<List<std::shared_ptr<Packet>>, StarException> receive_packets() override;

  bool sent_packets_pending() const override;

  [[nodiscard]] std::expected<bool, StarException> write_data() override;
  [[nodiscard]] std::expected<bool, StarException> read_data() override;
  [[nodiscard]] std::expected<void, StarException> wait_for_data(bool writable, std::chrono::steady_clock::time_point deadline) override;

  std::optional<PacketStats> incoming_stats() const override;
  std::optional<PacketStats> outgoing_stats() const override;

private:
  Logger& m_logger;
  std::unique_ptr<P2PSocket> m_socket;
  std::mutex m_wait_mutex;
  std::condition_variable m_wait_condition;
  bool m_closed = false;

  PacketStatCollector m_incoming_stats;
  PacketStatCollector m_outgoing_stats;
  Deque<ByteArray> m_output_messages;
  Deque<ByteArray> m_input_messages;
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

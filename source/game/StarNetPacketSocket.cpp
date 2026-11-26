import std;
import star.algorithm;
import star.byte_array;
import star.compression;
import star.data_stream;
import star.data_stream_devices;
import star.format;
import star.io_device;
import star.list;
import star.logging;
import star.map;
import star.net_compatibility;
import star.net_packet_socket;
import star.net_packets;
import star.output_proxy;
import star.p2p_networking_service;
import star.socket;
import star.tcp;
import star.vlq;
import star.zstd_compression;

namespace Star {

namespace {
constexpr std::size_t packet_size_limit = 64 * 1024 * 1024;
// TCP starts with a type byte and signed VLQ length; P2P has a type and bool byte.
constexpr std::size_t tcp_header_capacity = 1 + max_vlq_bytes;
constexpr std::size_t p2p_header_size = 2;
constexpr std::size_t input_buffer_limit = packet_size_limit + tcp_header_capacity;

using PacketList = List<std::shared_ptr<Packet>>;

enum class PacketGrouping { Single, Type, TypeAndCompression };

struct PacketGroup {
  PacketType type;
  PacketCompressionMode compression_mode;
};

std::expected<PacketGroup, StarException> encode_packet_group(PacketList::const_iterator& next,
    PacketList::const_iterator end, PacketGrouping grouping, DataStreamBuffer& payload) {
  payload.clear();
  PacketGroup group{(*next)->type(), grouping == PacketGrouping::TypeAndCompression
      ? (*next)->compression_mode() : PacketCompressionMode::Disabled};
  do {
    auto written = (*next++)->write(payload);
    if (!written)
      return std::unexpected(std::move(written.error()));
  } while (grouping != PacketGrouping::Single && next != end && (*next)->type() == group.type
      && (grouping != PacketGrouping::TypeAndCompression || (*next)->compression_mode() == group.compression_mode));

  if constexpr (debug_enabled) {
    // Only legacy batching depends on each group containing data for its packet count.
    if (grouping == PacketGrouping::TypeAndCompression && payload.empty())
      return std::unexpected(StarPacketException("Packet contains no serialized data"));
  }
  return group;
}

struct WirePayload {
  std::span<char const> bytes;
  bool compressed;
};

std::expected<WirePayload, StarException> select_wire_payload(std::span<char const> payload,
    PacketCompressionMode mode, ByteArray& scratch) {
  bool required = mode == PacketCompressionMode::Enabled;
  if (required || (mode == PacketCompressionMode::Automatic && payload.size() > 64)) {
    auto compressed = compress(payload, scratch);
    if (!compressed)
      return std::unexpected(std::move(compressed.error()));
    if (!scratch.empty() && (required || scratch.size() < payload.size()))
      return WirePayload{scratch.span(), true};
  }
  return WirePayload{payload, false};
}
}

PacketStatCollector::PacketStatCollector() : PacketStatCollector(1.0f) {}

PacketStatCollector::PacketStatCollector(float calculation_window)
  : m_calculation_window(calculation_window), m_stats(), m_total_bytes(0), m_last_mix_time() {}

void PacketStatCollector::mix(std::size_t size) {
  calculate();
  m_total_bytes += size;
}

void PacketStatCollector::mix(PacketType type, std::size_t size, bool add_to_total) {
  calculate();
  m_unmixed[type] += size;
  if (add_to_total)
    m_total_bytes += size;
}

void PacketStatCollector::mix(HashMap<PacketType, std::size_t> const& sizes, bool add_to_total) {
  calculate();
  for (auto const& p : sizes) {
    if (add_to_total)
      m_total_bytes += p.second;
    m_unmixed[p.first] += p.second;
  }
}

PacketStats PacketStatCollector::stats() const {
  const_cast<PacketStatCollector&>(*this).calculate();
  return m_stats;
}

void PacketStatCollector::calculate() {
  auto current_time = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() + std::chrono::microseconds(500));
  std::chrono::duration<float> elapsed_time((current_time - m_last_mix_time).count() / 1000.0f);
  if (elapsed_time >= m_calculation_window) {
    m_last_mix_time = current_time;
    m_stats.worst_packet_size = 0;

    for (auto& pair : m_unmixed) {
      if (pair.second > m_stats.worst_packet_size) {
        m_stats.worst_packet_type = pair.first;
        m_stats.worst_packet_size = pair.second;
      }

      m_stats.packet_bytes_per_second[pair.first] = std::round(pair.second / elapsed_time.count());
    }
    m_stats.bytes_per_second = std::round(static_cast<float>(m_total_bytes) / elapsed_time.count());
    m_total_bytes = 0;
    m_unmixed.clear();
  }
}

std::optional<PacketStats> PacketSocket::incoming_stats() const {
  return {};
}

std::optional<PacketStats> PacketSocket::outgoing_stats() const {
  return {};
}
void PacketSocket::set_net_rules(NetCompatibilityRules net_rules) { m_net_rules = net_rules; }
NetCompatibilityRules PacketSocket::net_rules() const { return m_net_rules; }

void CompressedPacketSocket::set_compression_stream_enabled(bool enabled) { m_use_compression_stream = enabled; }
bool CompressedPacketSocket::compression_stream_enabled() const { return m_use_compression_stream; }

std::expected<void, StarException> CompressedPacketSocket::initialize_compression() {
  auto compression = CompressionStream::create();
  if (!compression)
    return std::unexpected(std::move(compression.error()));
  auto decompression = DecompressionStream::create();
  if (!decompression)
    return std::unexpected(std::move(decompression.error()));
  m_compression_stream.emplace(std::move(*compression));
  m_decompression_stream.emplace(std::move(*decompression));
  return {};
}

std::pair<std::unique_ptr<LocalPacketSocket>, std::unique_ptr<LocalPacketSocket>> LocalPacketSocket::open_pair() {
  auto lhs_incoming_pipe = std::make_shared<Pipe>();
  auto rhs_incoming_pipe = std::make_shared<Pipe>();

  std::weak_ptr<Pipe> lhs_outgoing_pipe(rhs_incoming_pipe);
  std::weak_ptr<Pipe> rhs_outgoing_pipe(lhs_incoming_pipe);
  return {
    std::make_unique<LocalPacketSocket>(ConstructionKey{}, std::move(lhs_incoming_pipe), std::move(lhs_outgoing_pipe)),
    std::make_unique<LocalPacketSocket>(ConstructionKey{}, std::move(rhs_incoming_pipe), std::move(rhs_outgoing_pipe))
  };
}

LocalPacketSocket::~LocalPacketSocket() {
  close();
}

bool LocalPacketSocket::is_open() const {
  auto incoming_pipe = std::atomic_load(&m_incoming_pipe);
  if (!incoming_pipe)
    return false;
  std::lock_guard locker(incoming_pipe->mutex);
  return incoming_pipe->open && incoming_pipe->peer_open;
}

void LocalPacketSocket::close() {
  if (auto incoming_pipe = std::atomic_exchange(&m_incoming_pipe, std::shared_ptr<Pipe>())) {
    {
      std::lock_guard locker(incoming_pipe->mutex);
      incoming_pipe->open = false;
    }
    incoming_pipe->ready.notify_all();
    if (auto outgoing_pipe = m_outgoing_pipe.lock()) {
      {
        std::lock_guard locker(outgoing_pipe->mutex);
        outgoing_pipe->peer_open = false;
      }
      outgoing_pipe->ready.notify_all();
    }
  }
}

std::expected<void, StarException> LocalPacketSocket::send_packets(List<std::shared_ptr<Packet>> packets) {
  if (!is_open() || packets.empty())
    return {};

  if (auto outgoing_pipe = m_outgoing_pipe.lock()) {
    std::lock_guard locker(outgoing_pipe->mutex);
    if (!outgoing_pipe->open)
      return {};

    if constexpr (debug_enabled) {
      // Check serialization without replacing the original packets. The round
      // trip is not bijective: ProtocolResponsePacket::read keys off the
      // compression mode, and using its round-trip result breaks the handshake.
      DataStreamBuffer buffer;
      buffer.set_stream_compatibility_version(net_rules());
      for (auto const& in_packet : packets) {
        buffer.clear();
        auto written = in_packet->write(buffer);
        if (!written)
          return std::unexpected(std::move(written.error()));
        auto created = create_packet(in_packet->type());
        if (!created)
          return std::unexpected(std::move(created.error()));
        auto& out_packet = **created;
        out_packet.set_compression_mode(in_packet->compression_mode());
        auto rewind = buffer.seek(0);
        if (!rewind)
          return std::unexpected(std::move(rewind.error()));
        // This read is observational; a non-bijective read is not a rejection.
        (void)out_packet.read(buffer);
      }
    }

    outgoing_pipe->queue.append_range(std::move(packets));
    outgoing_pipe->ready.notify_all();
  }
  return {};
}

std::expected<List<std::shared_ptr<Packet>>, StarException> LocalPacketSocket::receive_packets() {
  // One snapshot: the mutex and the queue must belong to the same pipe, and the
  // pipe can be closed by another thread between two loads.
  auto incoming_pipe = std::atomic_load(&m_incoming_pipe);
  if (!incoming_pipe)
    return List<std::shared_ptr<Packet>>{};

  std::lock_guard locker(incoming_pipe->mutex);
  List<std::shared_ptr<Packet>> packets;
  packets.append_range(std::exchange(incoming_pipe->queue, {}));
  return packets;
}

bool LocalPacketSocket::sent_packets_pending() const {
  return false;
}

std::expected<bool, StarException> LocalPacketSocket::write_data() {
  return false;
}

std::expected<bool, StarException> LocalPacketSocket::read_data() {
  return false;
}

std::expected<void, StarException> LocalPacketSocket::wait_for_data(bool writable, std::chrono::steady_clock::time_point deadline) {
  if (writable)
    return {};
  auto incoming_pipe = std::atomic_load(&m_incoming_pipe);
  if (!incoming_pipe)
    return {};
  std::unique_lock locker(incoming_pipe->mutex);
  incoming_pipe->ready.wait_until(locker, deadline, [&] {
    return !incoming_pipe->queue.empty() || !incoming_pipe->open || !incoming_pipe->peer_open;
  });
  return {};
}

LocalPacketSocket::LocalPacketSocket(ConstructionKey, std::shared_ptr<Pipe> incoming_pipe, std::weak_ptr<Pipe> outgoing_pipe)
  : m_incoming_pipe(std::move(incoming_pipe)), m_outgoing_pipe(std::move(outgoing_pipe)) {}

std::expected<std::unique_ptr<TcpPacketSocket>, StarException> TcpPacketSocket::open(Logger& logger, std::shared_ptr<TcpSocket> socket) {
  auto no_delay = socket->set_no_delay(true);
  if (!no_delay)
    return std::unexpected(std::move(no_delay.error()));
  auto non_blocking = socket->set_non_blocking(true);
  if (!non_blocking)
    return std::unexpected(std::move(non_blocking.error()));
  auto result = std::make_unique<TcpPacketSocket>(ConstructionKey{}, logger, std::move(socket));
  auto initialized = result->initialize_compression();
  if (!initialized)
    return std::unexpected(std::move(initialized.error()));
  return result;
}

bool TcpPacketSocket::is_open() const {
  return m_socket->is_active();
}

void TcpPacketSocket::close() {
  m_socket->close();
}

std::expected<void, StarException> TcpPacketSocket::send_packets(PacketList packets) {
  if (packets.empty())
    return {};

  // Callers serialize socket I/O. Append directly to the unsent tail and roll
  // back the batch on failure, without moving or copying already queued bytes.
  auto first = m_output_buffer.size();
  auto rollback = make_scope_exit([&] { m_output_buffer.resize(first); });
  bool streaming = compression_stream_enabled();
  auto grouping = streaming ? PacketGrouping::Single : PacketGrouping::TypeAndCompression;
  m_packet_payload.set_stream_compatibility_version(net_rules());
  m_encoded_packet_sizes.clear();
  auto next = packets.cbegin();
  auto end = packets.cend();
  while (next != end) {
    auto group = encode_packet_group(next, end, grouping, m_packet_payload);
    if (!group)
      return std::unexpected(std::move(group.error()));
    auto payload = select_wire_payload(m_packet_payload.span(), group->compression_mode, m_codec_scratch);
    if (!payload)
      return std::unexpected(std::move(payload.error()));

    std::array<char, tcp_header_capacity> header;
    header.front() = static_cast<char>(std::to_underlying(group->type));
    auto size = static_cast<std::int64_t>(payload->bytes.size());
    auto header_size = 1 + write_vlq_i(payload->compressed ? -size : size, std::span(header).subspan<1>());
    m_output_buffer.reserve(m_output_buffer.size() + header_size + payload->bytes.size());
    m_output_buffer.append(std::span(header).first(header_size));
    m_output_buffer.append(payload->bytes);
    m_encoded_packet_sizes[group->type] += payload->bytes.size();
  }
  m_outgoing_stats.mix(m_encoded_packet_sizes, !streaming);
  rollback.release();
  return {};
}

std::expected<List<std::shared_ptr<Packet>>, StarException> TcpPacketSocket::receive_packets() {
  constexpr std::size_t packet_batch_limit = 131072;
  List<std::shared_ptr<Packet>> packets;
  auto parse = [&]() -> std::expected<void, StarException> {
    DataStreamReader ds{std::span<char const>(m_input_buffer)};
    std::size_t trim_position = 0;
    while (true) {
      auto at_end = ds.at_end();
      if (!at_end)
        return std::unexpected(std::move(at_end.error()));
      if (*at_end)
        break;
      auto type = ds.read<PacketType>();
      if (!type) {
        if (type.error().is<EofException>())
          break; // Keep the incomplete header for the next network read.
        return std::unexpected(std::move(type.error()));
      }
      auto length = ds.read_vlq_i();
      if (!length) {
        if (length.error().is<EofException>())
          break;
        return std::unexpected(std::move(length.error()));
      }
      bool packet_compressed = *length < 0;
      // Avoid negating INT64_MIN while rejecting oversized frames normally.
      std::uint64_t packet_size =
        packet_compressed ? static_cast<std::uint64_t>(-(*length + 1)) + 1 : static_cast<std::uint64_t>(*length);
      if (packet_size > packet_size_limit) {
        auto name = packet_type_name(*type);
        if (!name)
          return std::unexpected(std::move(name.error()));
        return std::unexpected(IOException::format("{} bytes large {} exceeds max size!", packet_size, *name));
      }
      auto remaining = ds.remaining();
      if (!remaining)
        return std::unexpected(std::move(remaining.error()));
      if (packet_size > *remaining)
        break;
      m_incoming_stats.mix(*type, packet_size, !compression_stream_enabled());
      auto position = ds.pos();
      if (!position)
        return std::unexpected(std::move(position.error()));
      DataStreamReader packet_stream(ds.span().subspan(*position, packet_size));
      packet_stream.set_stream_compatibility_version(net_rules());
      if (packet_compressed) {
        auto decoded = decompress(packet_stream.span(), m_codec_scratch, packet_size_limit);
        if (!decoded)
          return std::unexpected(std::move(decoded.error()));
        packet_stream.reset(m_codec_scratch.span());
      }
      auto advanced = ds.seek(packet_size, IOSeek::Relative);
      if (!advanced)
        return std::unexpected(std::move(advanced.error()));
      position = ds.pos();
      if (!position)
        return std::unexpected(std::move(position.error()));
      trim_position = *position;
      std::size_t count = 0;
      do {
        if (++count > packet_batch_limit) {
          auto name = packet_type_name(*type);
          if (!name)
            return std::unexpected(std::move(name.error()));
          return std::unexpected(IOException::format("Packet batch limit {} reached while reading {}s!", packet_batch_limit, *name));
        }
        auto created = create_packet(*type);
        if (!created)
          return std::unexpected(std::move(created.error()));
        auto packet = std::move(*created);
        packet->set_compression_mode(packet_compressed ? PacketCompressionMode::Enabled : PacketCompressionMode::Disabled);
        auto read = packet->read(packet_stream);
        if (!read)
          return std::unexpected(std::move(read.error()));
        packets.push_back(std::move(packet));
        auto ended = packet_stream.at_end();
        if (!ended)
          return std::unexpected(std::move(ended.error()));
        if (*ended)
          break;
      } while (true);
    }
    if (trim_position)
      m_input_buffer.remove_prefix(trim_position);
    return {};
  };
  auto parsed = parse();
  if (!parsed) {
    if (!parsed.error().is<IOException>())
      return std::unexpected(std::move(parsed.error()));
    m_input_buffer.clear();
    m_socket->shutdown();
    auto logged = m_logger.warn_exception("I/O error in TcpPacketSocket::receive_packets, closing",
                                          ExceptionDiagnostic(parsed.error()));
    if (!logged)
      return std::unexpected(std::move(logged.error()));
  }
  return packets;
}

bool TcpPacketSocket::sent_packets_pending() const {
  return !m_output_buffer.empty() || !m_compressed_output_buffer.empty();
}

std::expected<bool, StarException> TcpPacketSocket::write_data() {
  if (!is_open())
    return false;
  bool data_sent = false;
  std::size_t compressed_consumed = 0;
  std::size_t plain_consumed = 0;
  auto write = [&]() -> std::expected<void, StarException> {
    if (compression_stream_enabled() && !m_output_buffer.empty()) {
      auto compressed = m_compression_stream->compress(m_output_buffer, m_compressed_output_buffer);
      if (!compressed)
        return std::unexpected(std::move(compressed.error()));
      m_output_buffer.clear();
    }
    // Drain any previously compressed tail even after a mode change.
    while (compressed_consumed < m_compressed_output_buffer.size()) {
      auto written = m_socket->send(m_compressed_output_buffer.span().subspan(compressed_consumed));
      if (!written)
        return std::unexpected(std::move(written.error()));
      if (*written == 0)
        return {};
      data_sent = true;
      compressed_consumed += *written;
      m_outgoing_stats.mix(*written);
    }
    while (plain_consumed < m_output_buffer.size()) {
      auto written = m_socket->send(m_output_buffer.span().subspan(plain_consumed));
      if (!written)
        return std::unexpected(std::move(written.error()));
      if (*written == 0)
        break;
      data_sent = true;
      plain_consumed += *written;
    }
    return {};
  };
  auto written = write();
  // Commit successful sends even when a later send stops or fails. Compact
  // each queue only once per drain, never after every partial native write.
  if (compressed_consumed)
    m_compressed_output_buffer.remove_prefix(compressed_consumed);
  if (plain_consumed)
    m_output_buffer.remove_prefix(plain_consumed);
  if (!written) {
    if (written.error().is<SocketClosedException>()) {
      auto logged = m_logger.debug("TcpPacketSocket socket closed: {}", Star::output_exception(written.error(), false));
      if (!logged)
        return std::unexpected(std::move(logged.error()));
    } else if (written.error().is<IOException>()) {
      m_socket->shutdown();
      auto logged =
        m_logger.warn_exception("I/O error in TcpPacketSocket::write_data", ExceptionDiagnostic(written.error()));
      if (!logged)
        return std::unexpected(std::move(logged.error()));
    } else {
      return std::unexpected(std::move(written.error()));
    }
  }
  return data_sent;
}

std::expected<bool, StarException> TcpPacketSocket::read_data() {
  // Drain codec-owned output and saved compressed input even after peer EOF.
  // A local close is terminal for reads, not for already buffered packets.
  if (!m_socket->is_open())
    return false;
  bool data_received = false;
  auto read = [&]() -> std::expected<void, StarException> {
    std::array<char, 1024> read_buffer;
    for (;;) {
      if (m_input_buffer.size() == input_buffer_limit)
        break; // receive_packets() must drain complete frames before more input.
      if (m_decompression_pending || !m_compressed_input.empty()) {
        auto committed = m_input_buffer.size();
        auto chunk_size = std::min(std::size_t(32768), input_buffer_limit - committed);
        m_input_buffer.reserve(committed + chunk_size);
        auto decoded = m_decompression_stream->decompress(m_compressed_input.span(),
            m_input_buffer.spare_capacity().first(chunk_size));
        if (!decoded)
          return std::unexpected(std::move(decoded.error()));
        m_input_buffer.resize(committed + decoded->produced);
        m_compressed_input.remove_prefix(decoded->consumed);
        m_decompression_pending = decoded->produced == chunk_size;
        data_received |= decoded->produced != 0;
        if (!decoded->consumed && !decoded->produced && !m_compressed_input.empty())
          return std::unexpected(IOException("ZSTD network decompression made no progress"));
        continue;
      }
      auto amount = m_socket->receive(std::span(read_buffer).first(
          std::min(read_buffer.size(), input_buffer_limit - m_input_buffer.size())));
      if (!amount)
        return std::unexpected(std::move(amount.error()));
      if (!*amount)
        break;
      data_received = true;
      if (compression_stream_enabled()) {
        m_incoming_stats.mix(*amount);
        m_compressed_input.append(std::span<char const>(read_buffer).first(*amount));
      } else {
        m_input_buffer.append(std::span<char const>(read_buffer).first(*amount));
      }
    }
    return {};
  };
  auto received = read();
  if (!received) {
    if (received.error().is<SocketClosedException>()) {
      auto logged = m_logger.debug("TcpPacketSocket socket closed: {}", Star::output_exception(received.error(), false));
      if (!logged)
        return std::unexpected(std::move(logged.error()));
    } else if (received.error().is<IOException>()) {
      m_socket->shutdown();
      auto logged =
        m_logger.warn_exception("I/O error in TcpPacketSocket::read_data", ExceptionDiagnostic(received.error()));
      if (!logged)
        return std::unexpected(std::move(logged.error()));
    } else {
      return std::unexpected(std::move(received.error()));
    }
  }
  return data_received;
}

std::expected<void, StarException> TcpPacketSocket::wait_for_data(bool writable, std::chrono::steady_clock::time_point deadline) {
  return m_socket->wait_for_data(writable, deadline);
}

std::optional<PacketStats> TcpPacketSocket::incoming_stats() const {
  return m_incoming_stats.stats();
}

std::optional<PacketStats> TcpPacketSocket::outgoing_stats() const {
  return m_outgoing_stats.stats();
}

TcpPacketSocket::TcpPacketSocket(ConstructionKey, Logger& logger, std::shared_ptr<TcpSocket> socket)
  : m_logger(logger), m_socket(std::move(socket)) {}

std::expected<std::unique_ptr<P2PPacketSocket>, StarException> P2PPacketSocket::open(Logger& logger, std::unique_ptr<P2PSocket> socket) {
  auto result = std::make_unique<P2PPacketSocket>(ConstructionKey{}, logger, std::move(socket));
  auto initialized = result->initialize_compression();
  if (!initialized)
    return std::unexpected(std::move(initialized.error()));
  return result;
}

bool P2PPacketSocket::is_open() const {
  return m_socket && m_socket->isOpen();
}

void P2PPacketSocket::close() {
  {
    std::lock_guard locker(m_wait_mutex);
    m_closed = true;
  }
  m_wait_condition.notify_all();
  m_socket.reset();
}

std::expected<void, StarException> P2PPacketSocket::send_packets(PacketList packets) {
  if (packets.empty())
    return {};

  auto first = m_output_messages.size();
  auto rollback = make_scope_exit([&] { m_output_messages.resize(first); });
  bool streaming = compression_stream_enabled();
  auto grouping = streaming ? PacketGrouping::Type : PacketGrouping::TypeAndCompression;
  m_packet_payload.set_stream_compatibility_version(net_rules());
  m_encoded_packet_sizes.clear();
  auto next = packets.cbegin();
  auto end = packets.cend();
  while (next != end) {
    auto group = encode_packet_group(next, end, grouping, m_packet_payload);
    if (!group)
      return std::unexpected(std::move(group.error()));
    auto payload = select_wire_payload(m_packet_payload.span(), group->compression_mode, m_codec_scratch);
    if (!payload)
      return std::unexpected(std::move(payload.error()));

    std::array<char, p2p_header_size> header{
        static_cast<char>(std::to_underlying(group->type)), static_cast<char>(payload->compressed)};
    auto& message = m_output_messages.emplace_back();
    message.reserve(header.size() + payload->bytes.size());
    message.append(header);
    message.append(payload->bytes);
    m_encoded_packet_sizes[group->type] += payload->bytes.size();
  }

  if (streaming) {
    // Do not advance the stateful encoder until every packet can be serialized.
    // A codec error itself is terminal, but never exposes a partial batch.
    for (auto& message : m_output_messages | std::views::drop(first)) {
      m_codec_scratch.clear();
      auto compressed = m_compression_stream->compress(message.span(), m_codec_scratch);
      if (!compressed)
        return std::unexpected(std::move(compressed.error()));
      std::swap(message, m_codec_scratch);
    }
  }
  // Retain legacy payload accounting along with the native datagram byte count in write_data().
  m_outgoing_stats.mix(m_encoded_packet_sizes, !streaming);
  rollback.release();
  return {};
}

std::expected<List<std::shared_ptr<Packet>>, StarException> P2PPacketSocket::receive_packets() {
  List<std::shared_ptr<Packet>> packets;
  auto parse = [&]() -> std::expected<void, StarException> {
    for (auto& input_message : std::exchange(m_input_messages, {})) {
      DataStreamBuffer ds(std::move(input_message));
      auto type = ds.read<PacketType>();
      if (!type)
        return std::unexpected(std::move(type.error()));
      auto compressed = ds.read<bool>();
      if (!compressed)
        return std::unexpected(std::move(compressed.error()));
      auto position = ds.pos();
      if (!position)
        return std::unexpected(std::move(position.error()));
      std::size_t packet_size = ds.size() - *position;
      if (packet_size > packet_size_limit)
        return std::unexpected(IOException("P2P packet exceeds size limit"));
      auto packet_bytes = ds.span().subspan(*position);
      if (*compressed) {
        auto decoded = decompress(packet_bytes, m_codec_scratch, packet_size_limit);
        if (!decoded)
          return std::unexpected(std::move(decoded.error()));
        packet_bytes = m_codec_scratch.span();
      }
      m_incoming_stats.mix(*type, packet_size, !compression_stream_enabled());
      DataStreamReader packet_stream{packet_bytes};
      packet_stream.set_stream_compatibility_version(net_rules());
      do {
        auto created = create_packet(*type);
        if (!created)
          return std::unexpected(std::move(created.error()));
        auto packet = std::move(*created);
        packet->set_compression_mode(*compressed ? PacketCompressionMode::Enabled : PacketCompressionMode::Disabled);
        auto read = packet->read(packet_stream);
        if (!read)
          return std::unexpected(std::move(read.error()));
        packets.push_back(std::move(packet));
        auto ended = packet_stream.at_end();
        if (!ended)
          return std::unexpected(std::move(ended.error()));
        if (*ended)
          break;
      } while (true);
    }
    return {};
  };
  auto parsed = parse();
  if (!parsed) {
    if (!parsed.error().is<IOException>())
      return std::unexpected(std::move(parsed.error()));
    m_socket.reset();
    auto logged = m_logger.warn_exception("I/O error in P2PPacketSocket::receive_packets, closing",
                                          ExceptionDiagnostic(parsed.error()));
    if (!logged)
      return std::unexpected(std::move(logged.error()));
  }
  return packets;
}

bool P2PPacketSocket::sent_packets_pending() const {
  return !m_output_messages.empty();
}

std::expected<bool, StarException> P2PPacketSocket::write_data() {
  bool work_done = false;
  while (m_socket && !m_output_messages.empty()) {
    auto front = m_output_messages.first();
    if (!front)
      return std::unexpected(std::move(front.error()));
    auto sent = m_socket->sendMessage(front->get());
    if (!sent) {
      m_socket.reset();
      auto logged =
        m_logger.warn_exception("Exception in P2PPacketSocket::write_data, closing", ExceptionDiagnostic(sent.error()));
      if (!logged)
        return std::unexpected(std::move(logged.error()));
      break;
    }
    if (!*sent)
      break;
    m_outgoing_stats.mix(front->get().size());
    auto removed = m_output_messages.pop_front_checked();
    if (!removed)
      return std::unexpected(std::move(removed.error()));
    work_done = true;
  }
  return work_done;
}

std::expected<bool, StarException> P2PPacketSocket::read_data() {
  bool work_done = false;
  auto receive = [&]() -> std::expected<void, StarException> {
    while (m_socket) {
      auto message = m_socket->receiveMessage();
      if (!message)
        return std::unexpected(std::move(message.error()));
      if (!*message)
        break;
      m_incoming_stats.mix((**message).size());
      if (compression_stream_enabled()) {
        auto decompressed = m_decompression_stream->decompress(**message, packet_size_limit + p2p_header_size);
        if (!decompressed)
          return std::unexpected(std::move(decompressed.error()));
        m_input_messages.push_back(std::move(*decompressed));
      } else {
        m_input_messages.push_back(std::move(**message));
      }
      work_done = true;
    }
    return {};
  };
  auto received = receive();
  if (!received) {
    m_socket.reset();
    auto logged =
      m_logger.warn_exception("Exception in P2PPacketSocket::read_data, closing", ExceptionDiagnostic(received.error()));
    if (!logged)
      return std::unexpected(std::move(logged.error()));
  }
  return work_done;
}

std::expected<void, StarException> P2PPacketSocket::wait_for_data(bool, std::chrono::steady_clock::time_point deadline) {
  // Steam/Discord expose no socket readiness event here. Retry their actual
  // sendMessage/receiveMessage operations at most 1 ms later, interruptible by
  // local close; this is a bounded polling fallback, not a network notification.
  std::unique_lock locker(m_wait_mutex);
  m_wait_condition.wait_until(locker,
      std::min(deadline, std::chrono::steady_clock::now() + std::chrono::milliseconds(1)),
      [&] { return m_closed; });
  return {};
}

std::optional<PacketStats> P2PPacketSocket::incoming_stats() const {
  return m_incoming_stats.stats();
}

std::optional<PacketStats> P2PPacketSocket::outgoing_stats() const {
  return m_outgoing_stats.stats();
}

P2PPacketSocket::P2PPacketSocket(ConstructionKey, Logger& logger, std::unique_ptr<P2PSocket> socket)
  : m_logger(logger), m_socket(std::move(socket)) {}

}

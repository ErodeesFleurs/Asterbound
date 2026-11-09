import star.xx_hash;
import star.json;
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
import star.math_common;
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

using ZSTD_CCtx = struct ZSTD_CCtx_s;
using ZSTD_DCtx = struct ZSTD_DCtx_s;
using ZSTD_DStream = ZSTD_DCtx;
using ZSTD_CStream = ZSTD_CCtx;
import star.zstd_compression;
import star.net_compatibility;
import star.logging;
import star.output_proxy;

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
import star.lua_root;
import star.versioning_database;
import star.world_storage;
import star.player_types;
import star.sky_parameters;
import star.system_world;
import star.damage_manager;
import star.net_packets;
import star.net_packet_socket;
import star.compression;

import star.algorithm;
namespace Star {

PacketStatCollector::PacketStatCollector() : PacketStatCollector(1.0f) {}

PacketStatCollector::PacketStatCollector(float calculationWindow)
  : m_calculationWindow(calculationWindow), m_stats(), m_totalBytes(0), m_lastMixTime() {}

void PacketStatCollector::mix(std::size_t size) {
  calculate();
  m_totalBytes += size;
}

void PacketStatCollector::mix(PacketType type, std::size_t size, bool addToTotal) {
  calculate();
  m_unmixed[type] += size;
  if (addToTotal)
    m_totalBytes += size;
}

void PacketStatCollector::mix(HashMap<PacketType, std::size_t> const& sizes, bool addToTotal) {
  calculate();
  for (auto const& p : sizes) {
    if (addToTotal)
      m_totalBytes += p.second;
    m_unmixed[p.first] += p.second;
  }
}

PacketStats PacketStatCollector::stats() const {
  const_cast<PacketStatCollector&>(*this).calculate();
  return m_stats;
}

void PacketStatCollector::calculate() {
  auto currentTime = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() + std::chrono::microseconds(500));
  std::chrono::duration<float> elapsedTime((currentTime - m_lastMixTime).count() / 1000.0f);
  if (elapsedTime >= m_calculationWindow) {
    m_lastMixTime = currentTime;
    m_stats.worstPacketSize = 0;

    for (auto& pair : m_unmixed) {
      if (pair.second > m_stats.worstPacketSize) {
        m_stats.worstPacketType = pair.first;
        m_stats.worstPacketSize = pair.second;
      }

      m_stats.packetBytesPerSecond[pair.first] = std::round(pair.second / elapsedTime.count());
    }
    m_stats.bytesPerSecond = std::round(static_cast<float>(m_totalBytes) / elapsedTime.count());
    m_totalBytes = 0;
    m_unmixed.clear();
  }
}

std::optional<PacketStats> PacketSocket::incomingStats() const {
  return {};
}

std::optional<PacketStats> PacketSocket::outgoingStats() const {
  return {};
}
void PacketSocket::setNetRules(NetCompatibilityRules netRules) { m_netRules = netRules; }
NetCompatibilityRules PacketSocket::netRules() const { return m_netRules; }

void CompressedPacketSocket::setCompressionStreamEnabled(bool enabled) { m_useCompressionStream = enabled; }
bool CompressedPacketSocket::compressionStreamEnabled() const { return m_useCompressionStream; }

std::expected<void, StarException> CompressedPacketSocket::initializeCompression() {
  auto compression = CompressionStream::create();
  if (!compression)
    return std::unexpected(std::move(compression.error()));
  auto decompression = DecompressionStream::create();
  if (!decompression)
    return std::unexpected(std::move(decompression.error()));
  m_compressionStream = std::move(*compression);
  m_decompressionStream = std::move(*decompression);
  return {};
}

std::pair<std::unique_ptr<LocalPacketSocket>, std::unique_ptr<LocalPacketSocket>> LocalPacketSocket::openPair() {
  auto lhsIncomingPipe = std::make_shared<Pipe>();
  auto rhsIncomingPipe = std::make_shared<Pipe>();

  return {
    std::unique_ptr<LocalPacketSocket>(new LocalPacketSocket(lhsIncomingPipe, std::weak_ptr<Pipe>(rhsIncomingPipe))),
    std::unique_ptr<LocalPacketSocket>(new LocalPacketSocket(rhsIncomingPipe, std::weak_ptr<Pipe>(lhsIncomingPipe)))
  };
}

LocalPacketSocket::~LocalPacketSocket() {
  close();
}

bool LocalPacketSocket::isOpen() const {
  auto incomingPipe = std::atomic_load(&m_incomingPipe);
  if (!incomingPipe)
    return false;
  std::lock_guard locker(incomingPipe->mutex);
  return incomingPipe->open && incomingPipe->peerOpen;
}

void LocalPacketSocket::close() {
  if (auto incomingPipe = std::atomic_exchange(&m_incomingPipe, std::shared_ptr<Pipe>())) {
    {
      std::lock_guard locker(incomingPipe->mutex);
      incomingPipe->open = false;
    }
    incomingPipe->ready.notify_all();
    if (auto outgoingPipe = m_outgoingPipe.lock()) {
      {
        std::lock_guard locker(outgoingPipe->mutex);
        outgoingPipe->peerOpen = false;
      }
      outgoingPipe->ready.notify_all();
    }
  }
}

std::expected<void, StarException> LocalPacketSocket::sendPackets(List<std::shared_ptr<Packet>> packets) {
  if (!isOpen() || packets.empty())
    return {};

  if (auto outgoingPipe = m_outgoingPipe.lock()) {
    std::lock_guard locker(outgoingPipe->mutex);
    if (!outgoingPipe->open)
      return {};

#ifndef NDEBUG
    // Self test the serialization when assertions are enabled, but forward the
    // original packets.  The round trip is not bijective for every packet (see
    // the compatibility hack in ProtocolResponsePacket::read, which keys off the
    // compression mode), so replacing the packets with their round tripped
    // copies corrupted the local handshake instead of testing it.
    for (auto const& inPacket : packets) {
      DataStreamBuffer buffer;
      buffer.setStreamCompatibilityVersion(netRules());
      auto written = inPacket->write(buffer, netRules());
      if (!written)
        return std::unexpected(std::move(written.error()));
      auto created = createPacket(inPacket->type());
      if (!created)
        return std::unexpected(std::move(created.error()));
      auto outPacket = std::move(*created);
      outPacket->setCompressionMode(inPacket->compressionMode());
      auto rewind = buffer.seek(0);
      if (!rewind)
        return std::unexpected(std::move(rewind.error()));
      // The existing local serialization probe is observational only: a
      // non-bijective read must not replace or reject the original packet.
      auto probe = outPacket->read(buffer);
      if (!probe) {
        // Deliberately retain the original packet at this recovery boundary.
      }
    }
#endif

    outgoingPipe->queue.append_range(std::move(packets));
    outgoingPipe->ready.notify_all();
  }
  return {};
}

std::expected<List<std::shared_ptr<Packet>>, StarException> LocalPacketSocket::receivePackets() {
  // One snapshot: the mutex and the queue must belong to the same pipe, and the
  // pipe can be closed by another thread between two loads.
  auto incomingPipe = std::atomic_load(&m_incomingPipe);
  if (!incomingPipe)
    return List<std::shared_ptr<Packet>>{};

  std::lock_guard locker(incomingPipe->mutex);
  List<std::shared_ptr<Packet>> packets;
  packets.append_range(std::exchange(incomingPipe->queue, {}));
  return packets;
}

bool LocalPacketSocket::sentPacketsPending() const {
  return false;
}

std::expected<bool, StarException> LocalPacketSocket::writeData() {
  return false;
}

std::expected<bool, StarException> LocalPacketSocket::readData() {
  return false;
}

std::expected<void, StarException> LocalPacketSocket::waitForData(bool writable, std::chrono::steady_clock::time_point deadline) {
  if (writable)
    return {};
  auto incomingPipe = std::atomic_load(&m_incomingPipe);
  if (!incomingPipe)
    return {};
  std::unique_lock locker(incomingPipe->mutex);
  incomingPipe->ready.wait_until(locker, deadline, [&] {
    return !incomingPipe->queue.empty() || !incomingPipe->open || !incomingPipe->peerOpen;
  });
  return {};
}

LocalPacketSocket::LocalPacketSocket(std::shared_ptr<Pipe> incomingPipe, std::weak_ptr<Pipe> outgoingPipe)
  : m_incomingPipe(std::move(incomingPipe)), m_outgoingPipe(std::move(outgoingPipe)) {}

std::expected<std::unique_ptr<TcpPacketSocket>, StarException> TcpPacketSocket::open(Logger& logger, std::shared_ptr<TcpSocket> socket) {
  auto noDelay = socket->setNoDelay(true);
  if (!noDelay)
    return std::unexpected(std::move(noDelay.error()));
  auto nonBlocking = socket->setNonBlocking(true);
  if (!nonBlocking)
    return std::unexpected(std::move(nonBlocking.error()));
  auto result = std::unique_ptr<TcpPacketSocket>(new TcpPacketSocket(logger, std::move(socket)));
  auto initialized = result->initializeCompression();
  if (!initialized)
    return std::unexpected(std::move(initialized.error()));
  return result;
}

bool TcpPacketSocket::isOpen() const {
  return m_socket->isActive();
}

void TcpPacketSocket::close() {
  m_socket->close();
}

std::expected<void, StarException> TcpPacketSocket::sendPackets(List<std::shared_ptr<Packet>> packets) {
  auto it = packets.begin();
  if (compressionStreamEnabled()) {
    DataStreamBuffer outBuffer;
    while (it != packets.end()) {
      std::shared_ptr<Packet>& packet = *it++;
      auto packetType = packet->type();
      DataStreamBuffer packetBuffer;
      packetBuffer.setStreamCompatibilityVersion(netRules());
      auto written = packet->write(packetBuffer, netRules());
      if (!written)
        return std::unexpected(std::move(written.error()));
      auto typeWritten = outBuffer.write(packetType);
      if (!typeWritten)
        return std::unexpected(std::move(typeWritten.error()));
      auto lengthWritten = outBuffer.writeVlqI(static_cast<int>(packetBuffer.size()));
      if (!lengthWritten)
        return std::unexpected(std::move(lengthWritten.error()));
      auto bodyWritten = outBuffer.writeData(packetBuffer.span());
      if (!bodyWritten)
        return std::unexpected(std::move(bodyWritten.error()));
      m_outgoingStats.mix(packetType, packetBuffer.size(), false);
    }
    if (m_outputBuffer.empty())
      m_outputBuffer = outBuffer.takeData();
    else
      m_outputBuffer.append(outBuffer.span());
  } else {
    while (it != packets.end()) {
      PacketType currentType = (*it)->type();
      PacketCompressionMode currentCompressionMode = (*it)->compressionMode();

      DataStreamBuffer packetBuffer;
      packetBuffer.setStreamCompatibilityVersion(netRules());
      while (it != packets.end()
             && (*it)->type() == currentType
             && (*it)->compressionMode() == currentCompressionMode) {
        auto written = (*it++)->write(packetBuffer, netRules());
        if (!written)
          return std::unexpected(std::move(written.error()));
      }

      // Packets must read and write actual data, because this is used to
      // determine packet count
      if constexpr (Star::DebugEnabled) {
        if (packetBuffer.empty())
          return std::unexpected(StarPacketException("Packet contains no serialized data"));
      }

      ByteArray compressedPackets;
      bool mustCompress = currentCompressionMode == PacketCompressionMode::Enabled;
      bool perhapsCompress = currentCompressionMode == PacketCompressionMode::Automatic && packetBuffer.size() > 64;
      if (mustCompress || perhapsCompress) {
        auto compressed = compressData(packetBuffer.data());
        if (!compressed)
          return std::unexpected(std::move(compressed.error()));
        compressedPackets = std::move(*compressed);
      }

      DataStreamBuffer outBuffer;
      auto typeWritten = outBuffer.write(currentType);
      if (!typeWritten)
        return std::unexpected(std::move(typeWritten.error()));

      if (!compressedPackets.empty() && (mustCompress || compressedPackets.size() < packetBuffer.size())) {
        auto lengthWritten = outBuffer.writeVlqI(-static_cast<int>(compressedPackets.size()));
        if (!lengthWritten)
          return std::unexpected(std::move(lengthWritten.error()));
        auto bodyWritten = outBuffer.writeData(compressedPackets.span());
        if (!bodyWritten)
          return std::unexpected(std::move(bodyWritten.error()));
        m_outgoingStats.mix(currentType, compressedPackets.size(), true);
      } else {
        auto lengthWritten = outBuffer.writeVlqI(static_cast<int>(packetBuffer.size()));
        if (!lengthWritten)
          return std::unexpected(std::move(lengthWritten.error()));
        auto bodyWritten = outBuffer.writeData(packetBuffer.span());
        if (!bodyWritten)
          return std::unexpected(std::move(bodyWritten.error()));
        m_outgoingStats.mix(currentType, packetBuffer.size(), true);
      }
      if (m_outputBuffer.empty())
        m_outputBuffer = outBuffer.takeData();
      else
        m_outputBuffer.append(outBuffer.span());
    }
  }
  return {};
}

std::expected<List<std::shared_ptr<Packet>>, StarException> TcpPacketSocket::receivePackets() {
  std::uint64_t const PacketSizeLimit = 64 << 20;
  std::uint64_t const PacketBatchLimit = 131072;
  List<std::shared_ptr<Packet>> packets;
  auto parse = [&]() -> std::expected<void, StarException> {
    DataStreamExternalBuffer ds{std::span<char const>(m_inputBuffer)};
    std::size_t trimPos = 0;
    while (true) {
      auto atEnd = ds.at_end();
      if (!atEnd)
        return std::unexpected(std::move(atEnd.error()));
      if (*atEnd)
        break;
      auto type = ds.read<PacketType>();
      if (!type) {
        if (type.error().is<EofException>())
          break; // Keep the incomplete header for the next network read.
        return std::unexpected(std::move(type.error()));
      }
      auto length = ds.readVlqI();
      if (!length) {
        if (length.error().is<EofException>())
          break;
        return std::unexpected(std::move(length.error()));
      }
      bool packetCompressed = *length < 0;
      // Avoid negating INT64_MIN while rejecting oversized frames normally.
      std::uint64_t packetSize =
        packetCompressed ? static_cast<std::uint64_t>(-(*length + 1)) + 1 : static_cast<std::uint64_t>(*length);
      auto packetName = [&]() -> std::expected<String, StarException> {
        if (!PacketTypeNames)
          return std::unexpected(PacketTypeNames.error());
        auto name = PacketTypeNames->get_right(*type);
        if (!name)
          return std::unexpected(std::move(name.error()));
        return name->get();
      };
      if (packetSize > PacketSizeLimit) {
        auto name = packetName();
        if (!name)
          return std::unexpected(std::move(name.error()));
        return std::unexpected(IOException::format("{} bytes large {} exceeds max size!", packetSize, *name));
      }
      auto remaining = ds.remaining();
      if (!remaining)
        return std::unexpected(std::move(remaining.error()));
      if (packetSize > *remaining)
        break;
      m_incomingStats.mix(*type, packetSize, !compressionStreamEnabled());
      auto position = ds.pos();
      if (!position)
        return std::unexpected(std::move(position.error()));
      DataStreamExternalBuffer packetStream(ds.span().subspan(*position, packetSize));
      packetStream.setStreamCompatibilityVersion(netRules());
      ByteArray uncompressed;
      if (packetCompressed) {
        auto decoded = uncompressData(packetStream.span(), PacketSizeLimit);
        if (!decoded)
          return std::unexpected(std::move(decoded.error()));
        uncompressed = std::move(*decoded);
        packetStream.reset(uncompressed.span());
      }
      auto advanced = ds.seek(packetSize, IOSeek::Relative);
      if (!advanced)
        return std::unexpected(std::move(advanced.error()));
      position = ds.pos();
      if (!position)
        return std::unexpected(std::move(position.error()));
      trimPos = *position;
      std::size_t count = 0;
      do {
        if (++count > PacketBatchLimit) {
          auto name = packetName();
          if (!name)
            return std::unexpected(std::move(name.error()));
          return std::unexpected(IOException::format("Packet batch limit {} reached while reading {}s!", PacketBatchLimit, *name));
        }
        auto created = createPacket(*type);
        if (!created)
          return std::unexpected(std::move(created.error()));
        auto packet = std::move(*created);
        packet->setCompressionMode(packetCompressed ? PacketCompressionMode::Enabled : PacketCompressionMode::Disabled);
        auto read = packet->read(packetStream, netRules());
        if (!read)
          return std::unexpected(std::move(read.error()));
        packets.push_back(std::move(packet));
        auto ended = packetStream.at_end();
        if (!ended)
          return std::unexpected(std::move(ended.error()));
        if (*ended)
          break;
      } while (true);
    }
    if (trimPos)
      m_inputBuffer.remove_prefix(trimPos);
    return {};
  };
  auto parsed = parse();
  if (!parsed) {
    if (!parsed.error().is<IOException>())
      return std::unexpected(std::move(parsed.error()));
    m_inputBuffer.clear();
    m_socket->shutdown();
    auto logged = m_logger.warnException("I/O error in TcpPacketSocket::receivePackets, closing", ExceptionDiagnostic(parsed.error()));
    if (!logged)
      return std::unexpected(std::move(logged.error()));
  }
  return packets;
}

bool TcpPacketSocket::sentPacketsPending() const {
  return !m_outputBuffer.empty() || !m_compressedOutputBuffer.empty();
}

std::expected<bool, StarException> TcpPacketSocket::writeData() {
  if (!isOpen())
    return false;
  bool dataSent = false;
  std::size_t compressedConsumed = 0;
  std::size_t plainConsumed = 0;
  auto write = [&]() -> std::expected<void, StarException> {
    if (compressionStreamEnabled() && !m_outputBuffer.empty()) {
      auto compressed = m_compressionStream->compress(m_outputBuffer, m_compressedOutputBuffer);
      if (!compressed)
        return std::unexpected(std::move(compressed.error()));
      m_outputBuffer.clear();
    }
    // Drain any previously compressed tail even after a mode change.
    while (compressedConsumed < m_compressedOutputBuffer.size()) {
      auto written = m_socket->send(m_compressedOutputBuffer.span().subspan(compressedConsumed));
      if (!written)
        return std::unexpected(std::move(written.error()));
      if (*written == 0)
        return {};
      dataSent = true;
      compressedConsumed += *written;
      m_outgoingStats.mix(*written);
    }
    while (plainConsumed < m_outputBuffer.size()) {
      auto written = m_socket->send(m_outputBuffer.span().subspan(plainConsumed));
      if (!written)
        return std::unexpected(std::move(written.error()));
      if (*written == 0)
        break;
      dataSent = true;
      plainConsumed += *written;
    }
    return {};
  };
  auto written = write();
  // Commit successful sends even when a later send stops or fails. Compact
  // each queue only once per drain, never after every partial native write.
  if (compressedConsumed)
    m_compressedOutputBuffer.remove_prefix(compressedConsumed);
  if (plainConsumed)
    m_outputBuffer.remove_prefix(plainConsumed);
  if (!written) {
    if (written.error().is<SocketClosedException>()) {
      auto logged = m_logger.debug("TcpPacketSocket socket closed: {}", Star::output_exception(written.error(), false));
      if (!logged)
        return std::unexpected(std::move(logged.error()));
    } else if (written.error().is<IOException>()) {
      m_socket->shutdown();
      auto logged = m_logger.warnException("I/O error in TcpPacketSocket::writeData", ExceptionDiagnostic(written.error()));
      if (!logged)
        return std::unexpected(std::move(logged.error()));
    } else {
      return std::unexpected(std::move(written.error()));
    }
  }
  return dataSent;
}

std::expected<bool, StarException> TcpPacketSocket::readData() {
  // A local close is terminal for native reads, not a socket error. Buffered
  // frames remain available to receivePackets(), including after close.
  if (!m_socket->isOpen())
    return false;
  bool dataReceived = false;
  auto read = [&]() -> std::expected<void, StarException> {
    char readBuffer[1024];
    while (true) {
      auto readAmount = m_socket->receive(readBuffer);
      if (!readAmount)
        return std::unexpected(std::move(readAmount.error()));
      if (*readAmount == 0)
        break;
      dataReceived = true;
      if (compressionStreamEnabled()) {
        m_incomingStats.mix(*readAmount);
        auto decompressed = m_decompressionStream->decompress(std::span<char const>(readBuffer).first(*readAmount), m_inputBuffer);
        if (!decompressed)
          return std::unexpected(std::move(decompressed.error()));
      } else {
        m_inputBuffer.append(std::span<char const>(readBuffer).first(*readAmount));
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
      auto logged = m_logger.warnException("I/O error in TcpPacketSocket::receiveData", ExceptionDiagnostic(received.error()));
      if (!logged)
        return std::unexpected(std::move(logged.error()));
    } else {
      return std::unexpected(std::move(received.error()));
    }
  }
  return dataReceived;
}

std::expected<void, StarException> TcpPacketSocket::waitForData(bool writable, std::chrono::steady_clock::time_point deadline) {
  return m_socket->waitForData(writable, deadline);
}

std::optional<PacketStats> TcpPacketSocket::incomingStats() const {
  return m_incomingStats.stats();
}

std::optional<PacketStats> TcpPacketSocket::outgoingStats() const {
  return m_outgoingStats.stats();
}

TcpPacketSocket::TcpPacketSocket(Logger& logger, std::shared_ptr<TcpSocket> socket)
  : m_logger(logger), m_socket(std::move(socket)) {}

std::expected<std::unique_ptr<P2PPacketSocket>, StarException> P2PPacketSocket::open(Logger& logger, std::unique_ptr<P2PSocket> socket) {
  auto result = std::unique_ptr<P2PPacketSocket>(new P2PPacketSocket(logger, std::move(socket)));
  auto initialized = result->initializeCompression();
  if (!initialized)
    return std::unexpected(std::move(initialized.error()));
  return result;
}

bool P2PPacketSocket::isOpen() const {
  return m_socket && m_socket->isOpen();
}

void P2PPacketSocket::close() {
  {
    std::lock_guard locker(m_waitMutex);
    m_closed = true;
  }
  m_waitCondition.notify_all();
  m_socket.reset();
}

std::expected<void, StarException> P2PPacketSocket::sendPackets(List<std::shared_ptr<Packet>> packets) {
  auto it = packets.begin();

  if (compressionStreamEnabled()) {
    DataStreamBuffer outBuffer;
    while (it != packets.end()) {
      PacketType currentType = (*it)->type();
      DataStreamBuffer packetBuffer;
      packetBuffer.setStreamCompatibilityVersion(netRules());
      while (it != packets.end() && (*it)->type() == currentType) {
        auto written = (*it++)->write(packetBuffer, netRules());
        if (!written)
          return std::unexpected(std::move(written.error()));
      }
      auto typeWritten = outBuffer.write(currentType);
      if (!typeWritten)
        return std::unexpected(std::move(typeWritten.error()));
      auto modeWritten = outBuffer.write<bool>(false);
      if (!modeWritten)
        return std::unexpected(std::move(modeWritten.error()));
      auto bodyWritten = outBuffer.writeData(packetBuffer.span());
      if (!bodyWritten)
        return std::unexpected(std::move(bodyWritten.error()));
      m_outgoingStats.mix(currentType, packetBuffer.size(), false);
      auto compressed = m_compressionStream->compress(outBuffer.takeData());
      if (!compressed)
        return std::unexpected(std::move(compressed.error()));
      m_outputMessages.push_back(std::move(*compressed));
    }
  } else {
    while (it != packets.end()) {
      PacketType currentType = (*it)->type();
      PacketCompressionMode currentCompressionMode = (*it)->compressionMode();

      DataStreamBuffer packetBuffer;
      packetBuffer.setStreamCompatibilityVersion(netRules());
      while (it != packets.end()
             && (*it)->type() == currentType
             && (*it)->compressionMode() == currentCompressionMode) {
        auto written = (*it++)->write(packetBuffer, netRules());
        if (!written)
          return std::unexpected(std::move(written.error()));
      }

      // Packets must read and write actual data, because this is used to
      // determine packet count
      if constexpr (Star::DebugEnabled) {
        if (packetBuffer.empty())
          return std::unexpected(StarPacketException("Packet contains no serialized data"));
      }

      ByteArray compressedPackets;
      bool mustCompress = currentCompressionMode == PacketCompressionMode::Enabled;
      bool perhapsCompress = currentCompressionMode == PacketCompressionMode::Automatic && packetBuffer.size() > 64;
      if (mustCompress || perhapsCompress) {
        auto compressed = compressData(packetBuffer.data());
        if (!compressed)
          return std::unexpected(std::move(compressed.error()));
        compressedPackets = std::move(*compressed);
      }

      DataStreamBuffer outBuffer;
      auto typeWritten = outBuffer.write(currentType);
      if (!typeWritten)
        return std::unexpected(std::move(typeWritten.error()));

      if (!compressedPackets.empty() && (mustCompress || compressedPackets.size() < packetBuffer.size())) {
        auto modeWritten = outBuffer.write<bool>(true);
        if (!modeWritten)
          return std::unexpected(std::move(modeWritten.error()));
        auto bodyWritten = outBuffer.writeData(compressedPackets.span());
        if (!bodyWritten)
          return std::unexpected(std::move(bodyWritten.error()));
        m_outgoingStats.mix(currentType, compressedPackets.size(), true);
      } else {
        auto modeWritten = outBuffer.write<bool>(false);
        if (!modeWritten)
          return std::unexpected(std::move(modeWritten.error()));
        auto bodyWritten = outBuffer.writeData(packetBuffer.span());
        if (!bodyWritten)
          return std::unexpected(std::move(bodyWritten.error()));
        m_outgoingStats.mix(currentType, packetBuffer.size(), true);
      }
      m_outputMessages.push_back(outBuffer.takeData());
    }
  }
  return {};
}

std::expected<List<std::shared_ptr<Packet>>, StarException> P2PPacketSocket::receivePackets() {
  List<std::shared_ptr<Packet>> packets;
  auto parse = [&]() -> std::expected<void, StarException> {
    for (auto& inputMessage : std::exchange(m_inputMessages, {})) {
      DataStreamBuffer ds(std::move(inputMessage));
      auto type = ds.read<PacketType>();
      if (!type)
        return std::unexpected(std::move(type.error()));
      auto compressed = ds.read<bool>();
      if (!compressed)
        return std::unexpected(std::move(compressed.error()));
      auto position = ds.pos();
      if (!position)
        return std::unexpected(std::move(position.error()));
      std::size_t packetSize = ds.size() - *position;
      if (packetSize > std::size_t(std::numeric_limits<std::ptrdiff_t>::max()))
        return std::unexpected(DataStreamException("Byte stream request exceeds addressable size"));
      auto packetBytes = ds.span().subspan(*position);
      ByteArray uncompressed;
      if (*compressed) {
        auto decoded = uncompressData(packetBytes);
        if (!decoded)
          return std::unexpected(std::move(decoded.error()));
        uncompressed = std::move(*decoded);
        packetBytes = uncompressed.span();
      }
      m_incomingStats.mix(*type, packetSize, !compressionStreamEnabled());
      DataStreamExternalBuffer packetStream{packetBytes};
      packetStream.setStreamCompatibilityVersion(netRules());
      do {
        auto created = createPacket(*type);
        if (!created)
          return std::unexpected(std::move(created.error()));
        auto packet = std::move(*created);
        packet->setCompressionMode(*compressed ? PacketCompressionMode::Enabled : PacketCompressionMode::Disabled);
        auto read = packet->read(packetStream, netRules());
        if (!read)
          return std::unexpected(std::move(read.error()));
        packets.push_back(std::move(packet));
        auto ended = packetStream.at_end();
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
    auto logged = m_logger.warnException("I/O error in P2PPacketSocket::receivePackets, closing", ExceptionDiagnostic(parsed.error()));
    if (!logged)
      return std::unexpected(std::move(logged.error()));
  }
  return packets;
}

bool P2PPacketSocket::sentPacketsPending() const {
  return !m_outputMessages.empty();
}

std::expected<bool, StarException> P2PPacketSocket::writeData() {
  bool workDone = false;
  while (m_socket && !m_outputMessages.empty()) {
    auto front = m_outputMessages.first();
    if (!front)
      return std::unexpected(std::move(front.error()));
    auto sent = m_socket->sendMessage(front->get());
    if (!sent) {
      m_socket.reset();
      auto logged = m_logger.warnException("Exception in P2PPacketSocket::writeData, closing", ExceptionDiagnostic(sent.error()));
      if (!logged)
        return std::unexpected(std::move(logged.error()));
      break;
    }
    if (!*sent)
      break;
    m_outgoingStats.mix(front->get().size());
    auto removed = m_outputMessages.pop_front_checked();
    if (!removed)
      return std::unexpected(std::move(removed.error()));
    workDone = true;
  }
  return workDone;
}

std::expected<bool, StarException> P2PPacketSocket::readData() {
  bool workDone = false;
  auto receive = [&]() -> std::expected<void, StarException> {
    while (m_socket) {
      auto message = m_socket->receiveMessage();
      if (!message)
        return std::unexpected(std::move(message.error()));
      if (!*message)
        break;
      m_incomingStats.mix((**message).size());
      if (compressionStreamEnabled()) {
        auto decompressed = m_decompressionStream->decompress(**message);
        if (!decompressed)
          return std::unexpected(std::move(decompressed.error()));
        m_inputMessages.push_back(std::move(*decompressed));
      } else {
        m_inputMessages.push_back(std::move(**message));
      }
      workDone = true;
    }
    return {};
  };
  auto received = receive();
  if (!received) {
    m_socket.reset();
    auto logged = m_logger.warnException("Exception in P2PPacketSocket::readData, closing", ExceptionDiagnostic(received.error()));
    if (!logged)
      return std::unexpected(std::move(logged.error()));
  }
  return workDone;
}

std::expected<void, StarException> P2PPacketSocket::waitForData(bool, std::chrono::steady_clock::time_point deadline) {
  // Steam/Discord expose no socket readiness event here. Retry their actual
  // sendMessage/receiveMessage operations at most 1 ms later, interruptible by
  // local close; this is a bounded polling fallback, not a network notification.
  std::unique_lock locker(m_waitMutex);
  m_waitCondition.wait_until(locker,
      std::min(deadline, std::chrono::steady_clock::now() + std::chrono::milliseconds(1)),
      [&] { return m_closed; });
  return {};
}

std::optional<PacketStats> P2PPacketSocket::incomingStats() const {
  return m_incomingStats.stats();
}

std::optional<PacketStats> P2PPacketSocket::outgoingStats() const {
  return m_outgoingStats.stats();
}

P2PPacketSocket::P2PPacketSocket(Logger& logger, std::shared_ptr<P2PSocket> socket)
  : m_logger(logger), m_socket(std::move(socket)) {}

}

#include <benchmark/benchmark.h>

import std;
import star.performance_benchmarks;
import star.algorithm;
import star.assets;
import star.byte_array;
import star.casting;
import star.cellular_liquid;
import star.data_stream;
import star.data_stream_devices;
import star.entity;
import star.entity_factory;
import star.file;
import star.format;
import star.game_types;
import star.host_address;
import star.json;
import star.list;
import star.liquid_types;
import star.liquids_database;
import star.map;
import star.ordered_set;
import star.set;
import star.net_compatibility;
import star.net_packets;
import star.net_packet_socket;
import star.projectile;
import star.projectile_database;
import star.rect;
import star.root;
import star.tcp;
import star.universe_connection;
import star.vector;
import star.warping;
import star.world_client_state;
import star.tile_sector_array;
import star.world_generation;
import star.world_server;
import star.world_template;
import star.world_tiles;

namespace Star {

struct WorldServerTestAccess {
  static std::expected<void, StarException> seedSnapshotLiquidEngine(Root& root, WorldServer& world,
                                                                     std::uint64_t seed) {
    // A newly created blank world has generated sectors but has never run
    // simulation. Check both scheduled liquid work and the actual tile data:
    // activeCells() alone does not include pending generation-region visits.
    if (!world.m_liquidEngine || world.m_currentStep != 0 || world.m_liquidEngine->activeCells() != 0)
      return std::unexpected(StarException("Snapshot liquid seed requires a world without active simulation"));
    auto sectors = world.m_tileArray->loadedSectors();
    for (auto const& sector : sectors) {
      auto region = world.m_tileArray->sectorRegion(sector);
      for (int x = region.xMin(); x < region.xMax(); ++x) {
        for (int y = region.yMin(); y < region.yMax(); ++y) {
          auto const& liquid = world.m_tileArray->tile(Vec2I(x, y)).liquid;
          if (liquid.liquid != EmptyLiquidId || liquid.level != 0.0f || liquid.source)
            return std::unexpected(StarException("Snapshot liquid seed would discard generated liquid"));
        }
      }
    }
    auto database = root.liquidsDatabase();
    if (!database)
      return std::unexpected(std::move(database.error()));
    auto liquidWorld = LiquidWorld::create(world);
    if (!liquidWorld)
      return std::unexpected(std::move(liquidWorld.error()));
    auto engine = std::make_shared<LiquidCellEngine<LiquidId>>(seed, (*database)->liquidEngineParameters(),
                                                               std::move(*liquidWorld));
    for (auto const& settings : (*database)->allLiquidSettings())
      engine->setLiquidTickDelta(settings->id, settings->tickDelta);
    // World generation schedules visits even for empty sectors. Replay those
    // visits so replacing the empty engine does not remove the queued scan.
    for (auto const& sector : sectors)
      engine->visitRegion(world.m_tileArray->sectorRegion(sector));
    world.m_liquidEngine = std::move(engine);
    return {};
  }
};

namespace {

constexpr unsigned NetworkTicks = 8;
constexpr unsigned NetworkEntities = 64;
constexpr float NetworkDt = 1.0f / 60.0f;

struct SnapshotClient {
  ConnectionId id;
  NetCompatibilityRules rules;
  std::shared_ptr<WorldClientState> state;
  std::unique_ptr<UniverseConnection> sender;
  std::unique_ptr<UniverseConnection> receiver;
  std::unordered_map<EntityId, std::shared_ptr<Entity>> entities;
};

struct SnapshotFixture {
  std::shared_ptr<WorldServer> world;
  std::vector<SnapshotClient> clients;
  std::vector<EntityId> presence;
  std::uint64_t packets = 0;
  std::uint64_t payloadBytes = 0;
  std::uint64_t creates = 0;
  std::uint64_t destroys = 0;
  std::uint64_t deltas = 0;
  std::uint64_t sectors = 0;
  std::uint64_t liquids = 0;
  std::uint64_t sourceWireDigest = 14695981039346656037ull;
};

std::expected<std::pair<std::unique_ptr<UniverseConnection>, std::unique_ptr<UniverseConnection>>, StarException>
openSnapshotTransport(Root& root, NetCompatibilityRules rules) {
  auto listener = TcpSocket::listen(root.logger(), {HostAddress::localhost(), 0});
  if (!listener)
    return std::unexpected(std::move(listener.error()));
  auto nonblocking = (*listener)->setNonBlocking(true);
  if (!nonblocking)
    return std::unexpected(std::move(nonblocking.error()));
  auto outgoing = TcpSocket::connectTo((*listener)->localAddress());
  if (!outgoing)
    return std::unexpected(std::move(outgoing.error()));
  auto ready = (*listener)->waitForData(false, std::chrono::steady_clock::now() + std::chrono::seconds(2));
  if (!ready)
    return std::unexpected(std::move(ready.error()));
  auto incoming = (*listener)->accept(root.logger());
  if (!incoming)
    return std::unexpected(std::move(incoming.error()));
  if (!*incoming)
    return std::unexpected(StarException("Snapshot benchmark loopback connection was not accepted"));
  auto outgoingNonblocking = (*outgoing)->setNonBlocking(true);
  if (!outgoingNonblocking)
    return std::unexpected(std::move(outgoingNonblocking.error()));
  auto incomingNonblocking = (*incoming)->setNonBlocking(true);
  if (!incomingNonblocking)
    return std::unexpected(std::move(incomingNonblocking.error()));
  auto sender = TcpPacketSocket::open(root.logger(), std::move(*outgoing));
  if (!sender)
    return std::unexpected(std::move(sender.error()));
  auto receiver = TcpPacketSocket::open(root.logger(), std::move(*incoming));
  if (!receiver)
    return std::unexpected(std::move(receiver.error()));
  (*sender)->setNetRules(rules);
  (*receiver)->setNetRules(rules);
  return std::pair{std::make_unique<UniverseConnection>(std::move(*sender)),
                   std::make_unique<UniverseConnection>(std::move(*receiver))};
}

std::expected<ByteArray, StarException> snapshotPacketBytes(Packet const& packet, NetCompatibilityRules rules) {
  DataStreamBuffer buffer;
  buffer.setStreamCompatibilityVersion(rules);
  auto written = packet.write(buffer, rules);
  if (!written)
    return std::unexpected(std::move(written.error()));
  return buffer.takeData();
}

bool snapshotFieldsEqual(std::shared_ptr<Packet> const& before, std::shared_ptr<Packet> const& after, bool sameBytes) {
  // JSON objects and unordered maps are decoded into new containers. Their
  // traversal order is not a wire round-trip invariant; compare every field.
  if (auto expected = as<WorldStartPacket>(before)) {
    auto actual = as<WorldStartPacket>(after);
    return actual
      && std::tie(expected->templateData, expected->skyData, expected->weatherData, expected->playerStart,
                  expected->playerRespawn, expected->respawnInWorld, expected->worldProperties,
                  expected->dungeonIdGravity, expected->dungeonIdBreathable, expected->protectedDungeonIds,
                  expected->clientId, expected->localInterpolationMode)
      == std::tie(actual->templateData, actual->skyData, actual->weatherData, actual->playerStart,
                  actual->playerRespawn, actual->respawnInWorld, actual->worldProperties, actual->dungeonIdGravity,
                  actual->dungeonIdBreathable, actual->protectedDungeonIds, actual->clientId,
                  actual->localInterpolationMode);
  }
  if (auto expected = as<WorldLayoutUpdatePacket>(before)) {
    auto actual = as<WorldLayoutUpdatePacket>(after);
    return actual && expected->layoutData == actual->layoutData;
  }
  if (auto expected = as<CentralStructureUpdatePacket>(before)) {
    auto actual = as<CentralStructureUpdatePacket>(after);
    return actual && expected->structureData == actual->structureData;
  }
  if (auto expected = as<EntityUpdateSetPacket>(before)) {
    auto actual = as<EntityUpdateSetPacket>(after);
    return actual && expected->forConnection == actual->forConnection && expected->deltas == actual->deltas;
  }
  return sameBytes;
}

void digestSnapshotSource(SnapshotFixture& fixture, SnapshotClient const& client, Packet const& packet,
                          ByteArray const& payload) {
  auto feedInteger = [&](std::uint64_t value) {
    for (unsigned shift = 0; shift < 64; shift += 8)
      fixture.sourceWireDigest = (fixture.sourceWireDigest ^ ((value >> shift) & 0xff)) * 1099511628211ull;
  };
  feedInteger(client.id);
  feedInteger(client.rules.version());
  feedInteger(static_cast<unsigned>(packet.type()));
  feedInteger(payload.size());
  for (auto byte : payload)
    fixture.sourceWireDigest = (fixture.sourceWireDigest ^ static_cast<unsigned char>(byte)) * 1099511628211ull;
}

// The real TCP codec and UniverseConnection queue are consumers of every world
// packet. Entity creation/deltas additionally run the same factory/state readers
// used by WorldClient, without constructing a fake UniverseClient or renderer.
std::expected<void, StarException> consumeSnapshot(Root& root, SnapshotFixture& fixture, SnapshotClient& client) {
  auto outgoing = fixture.world->getOutgoingPackets(client.id);
  if (!outgoing)
    return std::unexpected(std::move(outgoing.error()));
  if (outgoing->empty())
    return std::unexpected(StarException("World update produced no snapshot packets"));
  auto queued = client.sender->push(*outgoing);
  if (!queued)
    return std::unexpected(std::move(queued.error()));
  List<std::shared_ptr<Packet>> delivered;
  auto start = std::chrono::steady_clock::now();
  while (delivered.size() < outgoing->size()) {
    auto sent = client.sender->send();
    if (!sent)
      return std::unexpected(std::move(sent.error()));
    auto received = client.receiver->receive();
    if (!received)
      return std::unexpected(std::move(received.error()));
    auto packets = client.receiver->pull();
    if (!packets)
      return std::unexpected(std::move(packets.error()));
    delivered.appendAll(std::move(*packets));
    if ((std::chrono::steady_clock::now() - start).count()
        > std::chrono::steady_clock::duration(std::chrono::seconds(2)).count())
      return std::unexpected(StarException("Snapshot benchmark TCP delivery timed out"));
  }
  if (delivered.size() != outgoing->size())
    return std::unexpected(StarException("Snapshot transport changed packet count"));
  auto factory = root.entityFactory();
  if (!factory)
    return std::unexpected(std::move(factory.error()));
  bool updateSetSeen = false;
  for (std::size_t index = 0; index < delivered.size(); ++index) {
    auto const& packet = delivered[index];
    if (packet->type() != (*outgoing)[index]->type())
      return std::unexpected(StarException("Snapshot transport changed packet ordering"));
    auto before = snapshotPacketBytes(*(*outgoing)[index], client.rules);
    if (!before)
      return std::unexpected(std::move(before.error()));
    auto after = snapshotPacketBytes(*packet, client.rules);
    if (!after)
      return std::unexpected(std::move(after.error()));
    if (!snapshotFieldsEqual((*outgoing)[index], packet, *before == *after))
      return std::unexpected(StarException::format("Snapshot TCP payload changed: type={} rules={} index={}",
                                                   static_cast<unsigned>(packet->type()), client.rules.version(),
                                                   index));
    digestSnapshotSource(fixture, client, *(*outgoing)[index], *before);
    ++fixture.packets;
    fixture.payloadBytes += before->size();
    if (auto create = as<EntityCreatePacket>(packet)) {
      if (updateSetSeen || client.entities.contains(create->entityId))
        return std::unexpected(
          StarException("Entity creation did not precede update sets or duplicated a visible entity"));
      auto entity = (*factory)->netLoadEntity(create->entityType, create->storeData, client.rules);
      if (!entity)
        return std::unexpected(std::move(entity.error()));
      auto read = (*entity)->readNetState(create->firstNetState, 0.0f, client.rules);
      if (!read)
        return std::unexpected(std::move(read.error()));
      client.entities.emplace(create->entityId, std::move(*entity));
      ++fixture.creates;
    } else if (auto update = as<EntityUpdateSetPacket>(packet)) {
      updateSetSeen = true;
      for (auto const& [id, delta] : update->deltas) {
        auto entity = client.entities.find(id);
        if (entity == client.entities.end())
          return std::unexpected(StarException("Entity delta arrived before creation"));
        auto read = entity->second->readNetState(delta, 0.0f, client.rules);
        if (!read)
          return std::unexpected(std::move(read.error()));
        ++fixture.deltas;
      }
    } else if (auto destroy = as<EntityDestroyPacket>(packet)) {
      if (client.entities.erase(destroy->entityId) != 1)
        return std::unexpected(StarException("Entity destroy arrived for an unknown entity"));
      ++fixture.destroys;
    } else if (packet->type() == PacketType::TileArrayUpdate) {
      ++fixture.sectors;
    } else if (packet->type() == PacketType::TileLiquidUpdate) {
      ++fixture.liquids;
    }
  }
  return {};
}

std::expected<void, StarException> moveSnapshotWindow(SnapshotFixture& fixture, SnapshotClient& client,
                                                      unsigned phase) {
  // Four heavily overlapping groups. The distant window forces real visibility
  // exits/re-entry; presence entities remain monitored outside the camera.
  int offset = phase % 2 ? 512 : 0;
  client.state->setWindow(RectI::withSize(Vec2I(960 + offset + int(client.id % 4) * 8, 960), Vec2I(128, 96)));
  auto delta = client.state->writeDelta();
  if (!delta)
    return std::unexpected(std::move(delta.error()));
  return fixture.world->handleIncomingPackets(client.id,
                                              {std::make_shared<WorldClientStateUpdatePacket>(std::move(*delta))});
}

std::expected<SnapshotFixture, StarException> prepareSnapshotFixture(Root& root, unsigned clientCount) {
  SnapshotFixture fixture;
  auto storage = File::ephemeral_file(root.random());
  if (!storage)
    return std::unexpected(std::move(storage.error()));
  auto worldTemplate = WorldTemplate::create(root, Vec2U(2048, 2048));
  if (!worldTemplate)
    return std::unexpected(std::move(worldTemplate.error()));
  auto storedTemplate = (*worldTemplate)->store();
  if (!storedTemplate)
    return std::unexpected(std::move(storedTemplate.error()));
  auto seededTemplate = storedTemplate->set("seed", Json(std::uint64_t(0x4e4554534e415000)));
  if (!seededTemplate)
    return std::unexpected(std::move(seededTemplate.error()));
  auto deterministicTemplate = WorldTemplate::create(root, *seededTemplate);
  if (!deterministicTemplate)
    return std::unexpected(std::move(deterministicTemplate.error()));
  auto world = WorldServer::create(root, *deterministicTemplate, *storage);
  if (!world)
    return std::unexpected(std::move(world.error()));
  fixture.world = std::move(*world);
  auto cleanup = finally([&] {
    if (fixture.world)
      (void)fixture.world->close();
  });
  auto seededLiquid = WorldServerTestAccess::seedSnapshotLiquidEngine(root, *fixture.world, 0x4e45544c49515549ull);
  if (!seededLiquid)
    return std::unexpected(std::move(seededLiquid.error()));
  fixture.world->setSpawningEnabled(false);
  fixture.world->setTileProtectionEnabled(false);
  auto fidelity = fixture.world->setFidelity(WorldServerFidelity::High);
  if (!fidelity)
    return std::unexpected(std::move(fidelity.error()));
  auto generated = fixture.world->generateRegion(RectI(896, 896, 1664, 1152));
  if (!generated)
    return std::unexpected(std::move(generated.error()));
  auto database = root.projectileDatabase();
  if (!database)
    return std::unexpected(std::move(database.error()));
  auto config = Json::parse(R"({
    "timeToLive":3600,"power":0,"onlyHitTerrain":true,
    "scripts":[],"periodicActions":[],"actionOnCollide":[],
    "actionOnReap":[],"actionOnHit":[],"actionOnTimeout":[],
    "movementSettings":{"collisionEnabled":false,"gravityEnabled":false,"frictionEnabled":false}
  })");
  if (!config)
    return std::unexpected(std::move(config.error()));
  for (unsigned i = 0; i < NetworkEntities; ++i) {
    auto projectile = (*database)->createProjectile("grenade", *config);
    if (!projectile)
      return std::unexpected(std::move(projectile.error()));
    (*projectile)->setInitialPosition(Vec2F(964 + int(i % 16) * 10, 972 + int(i / 16) * 16));
    (*projectile)->setInitialVelocity(Vec2F(1, 0));
    (*projectile)->setSourceEntity(NullEntityId, false);
    auto added = fixture.world->addEntity(*projectile);
    if (!added)
      return std::unexpected(std::move(added.error()));
    if (i < 4)
      fixture.presence.push_back((*projectile)->entityId());
  }
  fixture.clients.reserve(clientCount);
  for (unsigned i = 0; i < clientCount; ++i) {
    ConnectionId id = MinClientConnectionId + i;
    NetCompatibilityRules rules(i % 2 ? LegacyVersion : OpenProtocolVersion);
    auto added = fixture.world->addClient(id, SpawnTargetPosition(Vec2F(1024, 1024)), false, false, rules);
    if (!added)
      return std::unexpected(std::move(added.error()));
    if (!*added)
      return std::unexpected(StarException("Snapshot benchmark client was not added"));
    auto state = WorldClientState::create(root);
    if (!state)
      return std::unexpected(std::move(state.error()));
    (*state)->setNetCompatibilityRules(rules);
    (*state)->setPlayer(fixture.presence[0]);
    (*state)->setClientPresenceEntities(List<EntityId>::from(fixture.presence));
    auto transport = openSnapshotTransport(root, rules);
    if (!transport)
      return std::unexpected(std::move(transport.error()));
    fixture.clients.push_back(
      {id, rules, std::move(*state), std::move(transport->first), std::move(transport->second), {}});
    auto acknowledged = fixture.world->handleIncomingPackets(id, {std::make_shared<WorldStartAcknowledgePacket>()});
    if (!acknowledged)
      return std::unexpected(std::move(acknowledged.error()));
    auto moved = moveSnapshotWindow(fixture, fixture.clients.back(), 0);
    if (!moved)
      return std::unexpected(std::move(moved.error()));
  }
  auto updated = fixture.world->update(NetworkDt);
  if (!updated)
    return std::unexpected(std::move(updated.error()));
  for (auto& client : fixture.clients) {
    auto consumed = consumeSnapshot(root, fixture, client);
    if (!consumed)
      return std::unexpected(std::move(consumed.error()));
  }
  // Explicitly transfer the live world's checked-close responsibility.
  auto result = std::move(fixture);
  fixture.world.reset();
  return result;
}

void snapshotManyClients(benchmark::State& state, PerformanceBenchmarkSession& session) {
  auto clientCount = static_cast<unsigned>(state.range(0));
  std::uint64_t packets = 0, payloadBytes = 0, creates = 0, destroys = 0, deltas = 0, sectors = 0, liquids = 0;
  std::optional<std::uint64_t> previousWireDigest;
  std::uint64_t wireDigestChanges = 0;
  state.SetLabel("64 production grenades; 4 presence regions/client; alternating overlapping/distant windows; mixed "
                 "legacy/current; High; 60Hz; 8 ticks; TCP consumers untimed");
  for (auto _ : state) {
    (void)_;
    state.PauseTiming();
    auto prepared = prepareSnapshotFixture(session.root, clientCount);
    if (!prepared) {
      session.fail(state, prepared.error());
      break;
    }
    auto& fixture = *prepared;
    auto close = finally([&] {
      auto closed = fixture.world->close();
      if (!closed)
        session.fail(state, closed.error());
    });
    bool success = true;
    // Initial world/sector/create delivery is setup. Each measured tick has
    // identical work inputs after reconstructing the complete fixture.
    fixture.packets = fixture.payloadBytes = fixture.creates = fixture.destroys = fixture.deltas = fixture.sectors =
      fixture.liquids = 0;
    fixture.sourceWireDigest = 14695981039346656037ull;
    for (unsigned tick = 0; tick < NetworkTicks; ++tick) {
      for (auto& client : fixture.clients) {
        auto moved = moveSnapshotWindow(fixture, client, tick);
        if (!moved) {
          session.fail(state, moved.error());
          success = false;
          break;
        }
      }
      if (!success)
        break;
      auto liquid = fixture.world->setLiquid(Vec2I(1024 + int(tick), 1024), LiquidId(1), 0.5f, 0.5f);
      if (!liquid) {
        session.fail(state, liquid.error());
        success = false;
        break;
      }
      state.ResumeTiming();
      auto updated = fixture.world->update(NetworkDt);
      benchmark::ClobberMemory();
      state.PauseTiming();
      if (!updated) {
        session.fail(state, updated.error());
        success = false;
        break;
      }
      for (auto& client : fixture.clients) {
        auto consumed = consumeSnapshot(session.root, fixture, client);
        if (!consumed) {
          session.fail(state, consumed.error());
          success = false;
          break;
        }
      }
      if (!success)
        break;
    }
    if (success && (!fixture.creates || !fixture.destroys || !fixture.deltas || !fixture.sectors || !fixture.liquids)) {
      session.fail(state, "Snapshot fixture omitted visibility, delta, sector, or liquid work");
      success = false;
    }
    packets += fixture.packets;
    payloadBytes += fixture.payloadBytes;
    creates += fixture.creates;
    destroys += fixture.destroys;
    deltas += fixture.deltas;
    sectors += fixture.sectors;
    liquids += fixture.liquids;
    if (previousWireDigest && *previousWireDigest != fixture.sourceWireDigest)
      ++wireDigestChanges;
    previousWireDigest = fixture.sourceWireDigest;
    auto closed = fixture.world->close();
    close.cancel();
    if (!closed) {
      session.fail(state, closed.error());
      success = false;
    }
    fixture.clients.clear();
    fixture.world.reset();
    state.ResumeTiming();
    if (!success)
      break;
  }
  state.SetItemsProcessed(state.iterations() * NetworkTicks * clientCount);
  state.counters["packets"] = benchmark::Counter(double(packets), benchmark::Counter::kAvgIterations);
  state.counters["payload_bytes"] = benchmark::Counter(double(payloadBytes), benchmark::Counter::kAvgIterations);
  state.counters["entity_creates"] = benchmark::Counter(double(creates), benchmark::Counter::kAvgIterations);
  state.counters["entity_destroys"] = benchmark::Counter(double(destroys), benchmark::Counter::kAvgIterations);
  state.counters["entity_deltas"] = benchmark::Counter(double(deltas), benchmark::Counter::kAvgIterations);
  state.counters["sector_packets"] = benchmark::Counter(double(sectors), benchmark::Counter::kAvgIterations);
  state.counters["liquid_packets"] = benchmark::Counter(double(liquids), benchmark::Counter::kAvgIterations);
  if (previousWireDigest) {
    // Split into exactly representable JSON numbers; no canonicalization of
    // source packet bytes or entity/create ordering is performed.
    state.counters["source_wire_last_hi"] = double(*previousWireDigest >> 32);
    state.counters["source_wire_last_lo"] = double(*previousWireDigest & 0xffffffffull);
    state.counters["source_wire_changes"] = double(wireDigestChanges);
  }
}

}// namespace

std::expected<void, StarException> registerNetworkPerformanceBenchmarks(PerformanceBenchmarkSession& session) {
  benchmark::RegisterBenchmark("Network/SnapshotManyClients",
                               [&session](benchmark::State& state) { snapshotManyClients(state, session); })
    ->Arg(4)
    ->Arg(16)
    ->Arg(64)
    ->Unit(benchmark::kMillisecond);
  return {};
}

}// namespace Star

import std;
import star.byte_array;
import star.celestial_coordinate;
import star.client_context;
import star.damage_types;
import star.data_stream;
import star.data_stream_devices;
import star.data_stream_extra;
import star.format;
import star.file;
import star.game_test;
import star.json;
import star.json_rpc;
import star.list;
import star.logging;
import star.net_compatibility;
import star.player_types;
import star.root;
import star.test_universe;
import star.universe_client;
import star.vector;
import star.string;
import star.uuid;
import star.warping;
import star.world_storage;
#include "gtest/gtest.h"

using namespace Star;

namespace {

std::expected<ByteArray, StarException> update_packet(NetCompatibilityRules rules, ByteArray const& rpc,
    ByteArray const& ship, StringMap<ByteArray> const& custom, ByteArray const& net) {
  DataStreamBuffer out;
  out.set_stream_compatibility_version(rules);
  if (auto fields = out << rpc << ship; !fields)
    return std::unexpected(std::move(fields.error()));
  if (rules.version() >= 15) {
    if (auto field = out.write(custom); !field)
      return std::unexpected(std::move(field.error()));
  }
  if (auto field = out.write(net); !field)
    return std::unexpected(std::move(field.error()));
  return out.take_data();
}

TEST(ContextClient, HandshakeAndHistoricalFullAndSparseUpdates) {
  Logger logger;
  auto server = Uuid::from_hex("11111111111111111111111111111111");
  auto player = Uuid::from_hex("22222222222222222222222222222222");
  ASSERT_TRUE(server && player);
  for (unsigned version : {14u, 15u}) {
    SCOPED_TRACE(version);
    NetCompatibilityRules rules(version, true);
    auto created = ClientContext::create(logger, *server, *player, 93, rules);
    ASSERT_TRUE(created);
    auto& client = **created;
    EXPECT_EQ(client.server_uuid(), *server);
    EXPECT_EQ(client.player_uuid(), *player);
    EXPECT_EQ(client.connection_id(), 93);
    EXPECT_EQ(client.net_compatibility_rules(), rules);

    std::optional<std::pair<WarpAction, WarpMode>> orbit(std::in_place, WarpAlias::OwnShip, WarpMode::BeamOnly);
    WorldId world = ClientShipWorldId(*player);
    EntityDamageTeam team(TeamType::PVP, 19);
    ShipUpgrades upgrades;
    upgrades.ship_level = 6;
    upgrades.capabilities.add("teleport");
    CelestialCoordinate coordinate({-2, 3, 4}, 2);
    DataStreamBuffer full;
    full.set_stream_compatibility_version(rules);
    // Encode the historical order independently of either endpoint's group.
    ASSERT_TRUE(full << true << orbit << world << true << team << upgrades << coordinate);
    auto key = ByteArray::from_string("k");
    WorldChunks ship{{key, ByteArray::from_string("first")}};
    auto chunks = DataStreamBuffer::serialize(ship);
    ASSERT_TRUE(chunks);
    auto packet = update_packet(rules, {}, *chunks, {{"a", *chunks}, {"b", *chunks}}, full.data());
    ASSERT_TRUE(packet);
    ASSERT_TRUE(client.read_update(packet->span()));
    EXPECT_EQ(client.orbit_warp_action(), orbit);
    EXPECT_EQ(client.player_world_id(), world);
    EXPECT_TRUE(client.is_admin());
    EXPECT_EQ(client.team(), team);
    EXPECT_EQ(client.ship_level(), 6u);
    EXPECT_TRUE(client.has_ship_capability("teleport"));
    EXPECT_EQ(client.ship_coordinate(), coordinate);
    EXPECT_EQ(client.pending_ship_updates(), ship);
    EXPECT_EQ(client.pending_custom_world_updates().size(), version >= 15 ? 2u : 0u);

    // The later delta only clears admin. Pending storage writes are not taken
    // destructively; later updates supersede earlier keys without losing others.
    std::array<char, 4> const admin_delta{0, 3, 0, 0};
    WorldChunks replacement{{key, std::nullopt}, {ByteArray::from_string("empty"), ByteArray{}}};
    auto changed = DataStreamBuffer::serialize(replacement);
    ASSERT_TRUE(changed);
    auto next = update_packet(rules, {}, *changed, {{"a", *changed}}, ByteArray(admin_delta));
    ASSERT_TRUE(next);
    ASSERT_TRUE(client.read_update(next->span()));
    EXPECT_FALSE(client.is_admin());
    EXPECT_EQ(client.orbit_warp_action(), orbit);
    EXPECT_EQ(client.player_world_id(), world);
    EXPECT_EQ(client.ship_upgrades(), upgrades);
    EXPECT_EQ(client.ship_coordinate(), coordinate);
    EXPECT_EQ(client.team(), team);
    EXPECT_EQ(client.pending_ship_updates(), replacement);
    client.acknowledge_ship_updates();
    EXPECT_TRUE(client.pending_ship_updates().empty());
    if (version >= 15) {
      auto const& pending = client.pending_custom_world_updates();
      auto a = pending.find_ref("a");
      auto b = pending.find_ref("b");
      ASSERT_TRUE(a && b);
      EXPECT_EQ(a->get(), replacement);
      EXPECT_EQ(b->get(), ship);
      client.acknowledge_custom_world_updates("a");
      EXPECT_FALSE(pending.contains("a"));
      EXPECT_TRUE(pending.contains("b"));
    }
  }
}

TEST(ContextClient, MalformedUpdatesDoNotDispatchRpcPublishChunksOrPoisonLaterDeltas) {
  Logger logger;
  NetCompatibilityRules rules(15u);
  auto created = ClientContext::create(logger, Uuid::nil(), Uuid::nil(), 1, rules);
  ASSERT_TRUE(created);
  auto& client = **created;
  unsigned calls = 0;
  auto endpoint = std::static_pointer_cast<JsonRpc>(client.rpc_interface());
  auto nested = update_packet(rules, {}, {}, {}, {});
  ASSERT_TRUE(nested);
  ASSERT_TRUE(endpoint->register_handler("probe", [&](Json const& value) -> std::expected<Json, StarException> {
    EXPECT_FALSE(client.read_update(nested->span())); // The active record owns the scratch net fields.
    ++calls;
    return value;
  }));
  JsonRpc peer(logger);
  auto result = peer.invoke_remote("probe", 11);
  ASSERT_TRUE(result);
  auto rpc = peer.send();
  ASSERT_TRUE(rpc);
  auto chunks = DataStreamBuffer::serialize(WorldChunks{{ByteArray::from_string("k"), ByteArray::from_string("v")}});
  ASSERT_TRUE(chunks);
  std::array<char, 4> const admin_delta{0, 3, 1, 0};
  auto packet = update_packet(rules, *rpc, *chunks, {{"good", *chunks}}, ByteArray(admin_delta));
  ASSERT_TRUE(packet);
  auto assert_unmodified = [&] {
    EXPECT_EQ(calls, 0u);
    EXPECT_FALSE(client.is_admin());
    EXPECT_TRUE(client.pending_ship_updates().empty());
    EXPECT_TRUE(client.pending_custom_world_updates().empty());
    EXPECT_FALSE(endpoint->has_pending());
  };
  for (std::size_t n = 1; n < packet->size(); ++n) {
    SCOPED_TRACE(n);
    EXPECT_FALSE(client.read_update(packet->span().first(n)));
    assert_unmodified();
  }
  auto reject = [&](ByteArray const& rpc_frame, ByteArray const& ship_frame,
      StringMap<ByteArray> const& custom, ByteArray const& net_frame) {
    auto malformed = update_packet(rules, rpc_frame, ship_frame, custom, net_frame);
    ASSERT_TRUE(malformed);
    EXPECT_FALSE(client.read_update(malformed->span()));
    assert_unmodified();
  };
  auto trailing = *chunks;
  trailing.push_back(0);
  reject(*rpc, trailing, {}, {});
  reject(*rpc, *chunks, {{"good", *chunks}, {"bad", trailing}}, {});
  auto oversized = ByteArray(std::array<char, 4>{1, 0, 1, 127});
  reject(*rpc, oversized, {}, {});
  // The staged admin field has already been read before the invalid next index.
  reject(*rpc, *chunks, {}, ByteArray(std::array<char, 4>{0, 3, 1, 7}));
  reject(*rpc, *chunks, {}, ByteArray(std::array<char, 5>{0, 3, 1, 0, 0}));
  auto bad_rpc = *rpc;
  bad_rpc.push_back(0);
  reject(bad_rpc, *chunks, {}, ByteArray(admin_delta));
  auto bad_outer = *packet;
  bad_outer.push_back(0);
  EXPECT_FALSE(client.read_update(bad_outer.span()));
  assert_unmodified();

  // A valid delta that omits admin must not publish the abandoned field.
  DataStreamBuffer delta;
  EntityDamageTeam team(TeamType::Enemy, 5);
  ASSERT_TRUE(delta << false << std::uint8_t{4} << team << std::uint8_t{0});
  auto team_only = update_packet(rules, {}, {}, {}, delta.data());
  ASSERT_TRUE(team_only);
  ASSERT_TRUE(client.read_update(team_only->span()));
  EXPECT_EQ(client.team(), team);
  assert_unmodified();
  ASSERT_TRUE(client.read_update(packet->span()));
  EXPECT_TRUE(client.is_admin());
  EXPECT_EQ(calls, 1u);
  auto reply = client.write_update();
  ASSERT_TRUE(reply);
  ASSERT_TRUE(peer.receive(reply->span())); // Client-to-server is a bare RPC batch.
  auto response = result->result();
  ASSERT_TRUE(response);
  EXPECT_EQ(*response, std::optional<Json>(11));
}

const GameTestRegistration disk_retry("ContextPersistence", "DiskFailureRetiresSessionButPreservesWorldUpdate", [](GameTest& fixture) {
  auto universe = TestUniverse::create(fixture.root(), Vec2U(100, 100));
  ASSERT_TRUE(universe) << universe.error().what();
  auto& client = *(*universe)->client();
  auto context = client.client_context();
  ASSERT_TRUE(context);
  String const world("retry");
  auto directory = File::relative_to((*universe)->storage_directory(), "universeclient");
  auto path = File::relative_to(directory, "retry.world");
  ASSERT_TRUE(File::create_directory(directory));
  ASSERT_TRUE(File::create_directory(path)); // A directory cannot be opened as a World4 database.
  WorldChunks expected{{ByteArray::from_string("12345"), ByteArray::from_string("pending")}};
  auto chunks = DataStreamBuffer::serialize(expected);
  ASSERT_TRUE(chunks);
  auto packet = update_packet(context->net_compatibility_rules(), {}, {}, {{world, *chunks}}, {});
  ASSERT_TRUE(packet);
  ASSERT_TRUE(context->read_update(packet->span()));
  EXPECT_FALSE(client.disconnect());
  EXPECT_FALSE(client.client_context());
  EXPECT_TRUE(context->pending_custom_world_updates().contains(world));
  ASSERT_TRUE(File::remove_all(path));
  auto retried = client.disconnect();
  ASSERT_TRUE(retried) << retried.error().what();
  EXPECT_TRUE(context->pending_custom_world_updates().empty());
  auto saved = WorldStorage::getWorldChunksFromFile(fixture.root().logger(), path);
  ASSERT_TRUE(saved) << saved.error().what();
  EXPECT_EQ(*saved, expected);
  ASSERT_TRUE((*universe)->close());
});

}

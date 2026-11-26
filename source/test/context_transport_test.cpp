#include "gtest/gtest.h"

import std;
import star.byte_array;
import star.casting;
import star.client_context;
import star.data_stream;
import star.data_stream_devices;
import star.file;
import star.format;
import star.game_test;
import star.game_types;
import star.host_address;
import star.json;
import star.json_rpc;
import star.list;
import star.map;
import star.net_compatibility;
import star.net_packet_socket;
import star.net_packets;
import star.root;
import star.rpc_promise;
import star.server_client_context;
import star.set;
import star.string;
import star.universe_connection;
import star.universe_settings;
import star.uuid;
import star.vector;
import star.warping;
import star.world_server;
import star.world_server_thread;
import star.world_storage;

using namespace Star;

namespace {

auto make_session(Root& root, ConnectionId id, NetCompatibilityRules rules) {
  return ServerClientContext::create(root, id, {}, rules, Uuid::nil(), "crew", "human", false, {}, {}, {});
}

const GameTestRegistration retry_enqueued_packets("ContextTransport", "FailedSessionRetriesOldFrameBeforeFreshChanges", [](GameTest& fixture) {
  auto& root = fixture.root();
  NetCompatibilityRules rules(15u);
  auto first = make_session(root, 1, rules);
  auto second = make_session(root, 2, rules);
  auto receiver = ClientContext::create(root.logger(), Uuid::nil(), Uuid::nil(), 2, rules);
  ASSERT_TRUE(first && second && receiver);
  auto server = UniverseConnectionServer::create(root.logger(), [](UniverseConnectionServer&, ConnectionId,
      List<std::shared_ptr<Packet>>) -> std::expected<void, StarException> {
    return std::unexpected(StarException("Unexpected inbound test packet"));
  }, 1);
  ASSERT_TRUE(server);
  auto pair = LocalPacketSocket::open_pair();
  ASSERT_TRUE((*server)->addConnection(1, UniverseConnection(std::move(pair.first))));
  UniverseConnection first_wire(std::move(pair.second));
  auto send = [&](ServerClientContext& session) {
    return session.send_update([&](std::shared_ptr<ClientContextUpdatePacket> const& packet) {
      return (*server)->enqueue_packet(session.client_id(), packet);
    });
  };

  // Actual handler fallback: no world, no entity ID, non-canonical descriptor.
  JsonObject parameters{{"paint", "red"}};
  auto call = (*receiver)->rpc_interface()->invoke_remote("world.containerPutItems", JsonObject{
      {"items", JsonArray{JsonArray{"wire", 3, parameters}, "rope"}}});
  ASSERT_TRUE(call);
  auto request = (*receiver)->write_update();
  ASSERT_TRUE(request);
  ASSERT_TRUE((*second)->read_update(*request));
  auto key = ByteArray::from_string("chunk");
  WorldChunks old_chunks{{key, ByteArray::from_string("before")}};
  (*second)->update_ship_chunks(old_chunks);
  (*second)->update_custom_world_chunks("garden", old_chunks);
  (*second)->set_admin(true);

  ASSERT_TRUE(send(**first));
  ASSERT_FALSE(send(**second)); // No registered local queue for this session yet.
  ASSERT_EQ(first_wire.receiveAny(2000), true);
  auto accepted = first_wire.pullSingle();
  ASSERT_TRUE(accepted && as<ClientContextUpdatePacket>(*accepted));
  ASSERT_TRUE(send(**first));
  EXPECT_EQ(first_wire.receiveAny(0), false);

  WorldChunks new_chunks{{key, ByteArray{}}}; // Present, zero-length chunk, not a deletion.
  (*second)->update_ship_chunks(new_chunks);
  (*second)->update_custom_world_chunks("garden", {});
  (*second)->set_admin(false);
  auto other_pair = LocalPacketSocket::open_pair();
  ASSERT_TRUE((*server)->addConnection(2, UniverseConnection(std::move(other_pair.first))));
  UniverseConnection second_wire(std::move(other_pair.second));
  ASSERT_TRUE(send(**second));
  for (bool old_frame : {true, false}) {
    ASSERT_EQ(second_wire.receiveAny(2000), true);
    auto received = second_wire.pullSingle();
    ASSERT_TRUE(received);
    auto packet = as<ClientContextUpdatePacket>(*received);
    ASSERT_TRUE(packet);
    DataStreamReader stream(packet->update_data.span());
    auto rpc = stream.read<ByteArray>();
    ASSERT_TRUE(rpc);
    EXPECT_EQ(!rpc->empty(), old_frame);
    ASSERT_TRUE((*receiver)->read_update(packet->update_data.span()));
    EXPECT_EQ((*receiver)->is_admin(), old_frame);
    EXPECT_EQ((*receiver)->pending_ship_updates(), old_frame ? old_chunks : new_chunks);
    auto world = (*receiver)->pending_custom_world_updates().find_ref("garden");
    ASSERT_TRUE(world);
    WorldChunks deletion{{key, std::nullopt}};
    EXPECT_EQ(world->get(), old_frame ? old_chunks : deletion);
    (*receiver)->acknowledge_ship_updates();
    (*receiver)->acknowledge_custom_world_updates("garden");
  }
  auto overflow = call->result();
  ASSERT_TRUE(overflow && *overflow);
  EXPECT_EQ(**overflow, (JsonArray{JsonObject{{"name", "wire"}, {"count", 3}, {"parameters", parameters}},
                                  JsonObject{{"name", "rope"}, {"count", 1}, {"parameters", JsonObject{}}}}));
  ASSERT_TRUE(send(**second));
  EXPECT_EQ(second_wire.receiveAny(0), false);
});

const GameTestRegistration legacy_custom_chunks("ContextTransport", "CustomOnlyDeltasStayOffLegacyWire", [](GameTest& fixture) {
  NetCompatibilityRules rules(14u);
  auto server = make_session(fixture.root(), 3, rules);
  ASSERT_TRUE(server);
  unsigned frames = 0;
  auto receive = [&](std::shared_ptr<ClientContextUpdatePacket> const& packet) -> std::expected<void, StarException> {
    ++frames;
    DataStreamReader stream(packet->update_data.span());
    auto rpc = stream.read<ByteArray>();
    auto ship = stream.read<ByteArray>();
    auto net = stream.read<ByteArray>();
    EXPECT_TRUE(rpc && rpc->empty());
    EXPECT_TRUE(ship && ship->empty());
    EXPECT_TRUE(net && !net->empty());
    EXPECT_EQ(stream.at_end(), true);
    return {};
  };
  ASSERT_TRUE((*server)->send_update(receive));
  auto key = ByteArray::from_string("sector");
  (*server)->update_custom_world_chunks("legacy", {{key, ByteArray::from_string("tiles")}});
  ASSERT_TRUE((*server)->send_update(receive));
  EXPECT_EQ(frames, 1u);
  ASSERT_TRUE((*server)->clean_inactive_custom_worlds());
  EXPECT_FALSE((*server)->custom_world_chunks("legacy"));
});

const GameTestRegistration world_membership("ContextTransport", "WorldHandlerNeedsCurrentClientMembership", [](GameTest& fixture) {
  auto& root = fixture.root();
  auto storage = File::ephemeral_file(root.random());
  ASSERT_TRUE(storage);
  auto world = WorldServer::create(root, Vec2U(2048, 2048), *storage);
  ASSERT_TRUE(world);
  auto settings = UniverseSettings::create(root);
  ASSERT_TRUE(settings);
  (*world)->setUniverseSettings(*settings);
  auto worker = WorldServerThread::create(*world, ClientShipWorldId(Uuid::nil()));
  ASSERT_TRUE(worker);
  NetCompatibilityRules rules(15u);
  auto server = make_session(root, min_client_connection_id, rules);
  ASSERT_TRUE(server);
  auto added = (*worker)->addClient(min_client_connection_id, SpawnTargetPosition(1024, 1024), true, false, rules);
  ASSERT_EQ(added, true);
  (*server)->set_player_world(*worker);
  JsonRpc peer(root.logger());
  auto set_flag = [&](String const& flag) -> std::expected<void, StarException> {
    auto request = peer.invoke_remote("universe.setFlag", flag);
    if (!request) return std::unexpected(std::move(request.error()));
    auto bytes = peer.send();
    if (!bytes) return std::unexpected(std::move(bytes.error()));
    return (*server)->read_update(*bytes);
  };
  ASSERT_TRUE(set_flag("attached"));
  EXPECT_TRUE((*settings)->flags().contains("attached"));
  ASSERT_TRUE((*worker)->removeClient(min_client_connection_id));
  // The world stays alive and the session still references it. Neither is
  // proof that this client can still act on its universe settings.
  ASSERT_TRUE(set_flag("departed"));
  EXPECT_FALSE((*settings)->flags().contains("departed"));
  (*server)->clear_player_world();
  EXPECT_TRUE((*worker)->close());
});

const GameTestRegistration handler_lock_order("ContextTransport", "BlockedRpcDoesNotHoldContextOrBlockPublisher", [](GameTest& fixture) {
  auto server = make_session(fixture.root(), 4, NetCompatibilityRules(15u));
  ASSERT_TRUE(server);
  std::promise<void> entered, release;
  auto entered_future = entered.get_future();
  auto gate = release.get_future();
  JsonRpcHandlers handlers;
  handlers["test.gate"] = [&](Json const&) -> std::expected<Json, StarException> {
    entered.set_value();
    if (gate.wait_for(std::chrono::seconds(5)) != std::future_status::ready)
      return std::unexpected(StarException("Publisher waited on a blocked RPC handler"));
    return Json(true);
  };
  ASSERT_TRUE((*server)->register_rpc_handlers(handlers));
  JsonRpc peer(fixture.root().logger());
  auto promise = peer.invoke_remote("test.gate", {});
  ASSERT_TRUE(promise);
  auto request = peer.send();
  ASSERT_TRUE(request);
  auto dispatch = std::async(std::launch::async, [&] { return (*server)->read_update(*request); });
  ASSERT_EQ(entered_future.wait_for(std::chrono::seconds(2)), std::future_status::ready);
  auto publish = std::async(std::launch::async, [&] {
    (*server)->set_admin(true);
    return (*server)->send_update([&](std::shared_ptr<ClientContextUpdatePacket> const& packet) -> std::expected<void, StarException> {
      EXPECT_TRUE((*server)->is_admin()); // The enqueue callback also runs outside the data lock.
      DataStreamReader stream(packet->update_data.span());
      EXPECT_EQ(stream.read<ByteArray>(), ByteArray{});
      return {};
    });
  });
  EXPECT_EQ(publish.wait_for(std::chrono::seconds(2)), std::future_status::ready);
  release.set_value();
  EXPECT_TRUE(dispatch.get());
  EXPECT_TRUE(publish.get());
  ASSERT_TRUE((*server)->send_update([&](std::shared_ptr<ClientContextUpdatePacket> const& packet) -> std::expected<void, StarException> {
    DataStreamReader stream(packet->update_data.span());
    auto rpc = stream.read<ByteArray>();
    if (!rpc) return std::unexpected(std::move(rpc.error()));
    return peer.receive(rpc->span());
  }));
  EXPECT_EQ(promise->result().value_or(std::nullopt), Json(true));
});

} // namespace

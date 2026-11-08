#include "gtest/gtest.h"

import std;
import star.algorithm;
import star.assets;
import star.byte_array;
import star.client_context;
import star.data_stream_devices;
import star.file;
import star.format;
import star.game_types;
import star.game_test;
import star.json;
import star.json_rpc;
import star.list;
import star.logging;
import star.map;
import star.player;
import star.player_factory;
import star.root;
import star.string;
import star.team_client;
import star.team_manager;
import star.uuid;

using namespace Star;

namespace {

std::expected<Json, StarException> invokeTeam(JsonRpcHandlers const& handlers, String const& method, Json const& args) {
  auto handler = handlers.find_ref(method);
  if (!handler)
    return std::unexpected(StarException("Missing team handler in fixture", false));
  return handler->get()(args);
}

const GameTestRegistration teamStatusSynchronization(
  "TeamClientTest", "AppearanceSyncRetainsDynamicStatusAndPendingForceAcrossReplies", [](GameTest& fixture) {
    auto directory = File::temporary_directory(fixture.root().random());
    ASSERT_TRUE(directory);
    auto cleanup = finally([&] { EXPECT_TRUE(File::remove_all(*directory)); });
    auto overlay = File::relative_to(*directory, "assets");
    ASSERT_TRUE(File::create_directories(overlay));
    // Drive each RPC explicitly; ordinary polling must not add unrelated requests.
    String intervalPatch = R"([
      {"op":"replace","path":"/invitationPollInterval","value":1000000000000},
      {"op":"replace","path":"/fullUpdateInterval","value":1000000000000},
      {"op":"replace","path":"/statusUpdateInterval","value":1000000000000}
    ])";
    auto intervalPath = File::relative_to(overlay, "interface.config.patch");
    ASSERT_TRUE(File::write_file(intervalPatch, intervalPath));
    auto settings = fixture.rootSettings();
    settings.assetSources.push_back(overlay);
    settings.storageDirectory = File::relative_to(*directory, "storage");
    settings.runtimeConfigFile = {};
    settings.logDirectory = {};
    settings.logFile = {};
    settings.includeUGC = false;
    auto root = Root::create(std::move(settings));
    ASSERT_TRUE(root);
    auto factory = (*root)->playerFactory();
    ASSERT_TRUE(factory);
    auto player = (*factory)->create();
    ASSERT_TRUE(player);
    ASSERT_TRUE((*player)->finalizeCreation());
    auto uuid = (*player)->uuid();
    ASSERT_TRUE(uuid);
    auto name = (*player)->name();
    ASSERT_TRUE(name);
    Uuid peer((*root)->random());
    auto manager = TeamManager::create(**root);
    ASSERT_TRUE(manager);
    StringMap<List<Uuid>> connected;
    connected[*name] = {*uuid};
    connected["Team fixture peer"] = {peer};
    (*manager)->setConnectedPlayers(std::move(connected));
    auto ownHandlers = (*manager)->authenticatedRpcHandlers(*uuid);
    auto peerHandlers = (*manager)->authenticatedRpcHandlers(peer);
    auto formTeam = [&]() -> std::expected<void, StarException> {
      auto invited = invokeTeam(ownHandlers, "team.invite", JsonObject{
          {"inviteeName", "Team fixture peer"}, {"inviterUuid", uuid->hex()}, {"inviterName", *name}});
      if (!invited) return std::unexpected(std::move(invited.error()));
      auto polled = invokeTeam(peerHandlers, "team.pollInvitation", JsonObject{{"playerUuid", peer.hex()}});
      if (!polled) return std::unexpected(std::move(polled.error()));
      auto accepted = invokeTeam(peerHandlers, "team.acceptInvitation", JsonObject{
          {"inviterUuid", uuid->hex()}, {"inviteeUuid", peer.hex()}});
      if (!accepted) return std::unexpected(std::move(accepted.error()));
      if (!accepted->isNull())
        return std::unexpected(StarException("Real team invitation was rejected", false));
      return {};
    };
    ASSERT_TRUE(formTeam());
    auto context = ClientContext::create(**root, Uuid((*root)->random()), *uuid);
    ASSERT_TRUE(context);
    // ClientContext owns this concrete RPC implementation; transport remains real.
    auto clientRpc = std::static_pointer_cast<JsonRpc>((*context)->rpcInterface());
    Logger logger;
    logger.removeStdoutSink();
    JsonRpc server(logger);
    ASSERT_TRUE(server.registerHandlers(ownHandlers));
    TeamClient team(**root, *player, *context);
    auto exchange = [&](ByteArray const& packet) -> std::expected<void, StarException> {
      if (auto received = server.receive(packet); !received)
        return std::unexpected(std::move(received.error()));
      auto reply = server.send();
      if (!reply) return std::unexpected(std::move(reply.error()));
      if (auto received = clientRpc->receive(*reply); !received)
        return std::unexpected(std::move(received.error()));
      return team.update();
    };
    auto fetchTeam = [&]() -> std::expected<void, StarException> {
      if (auto pulled = team.pullFullUpdate(); !pulled)
        return std::unexpected(std::move(pulled.error()));
      auto packet = (*context)->writeUpdate({});
      if (!packet) return std::unexpected(std::move(packet.error()));
      return exchange(*packet);
    };
    auto submit = [&]() -> std::expected<ByteArray, StarException> {
      if (auto sent = team.statusUpdate(); !sent)
        return std::unexpected(std::move(sent.error()));
      return (*context)->writeUpdate({});
    };
    auto checkPacket = [&](ByteArray const& packet, bool full, std::string_view phase) {
      SCOPED_TRACE(phase);
      DataStreamExternalBuffer stream(packet.span());
      List<Json> messages;
      ASSERT_TRUE(stream.readContainer(messages));
      ASSERT_EQ(messages.size(), 1u);
      EXPECT_EQ(messages.front().getString("handler"), "team.updateStatus");
      auto args = messages.front().get("arguments");
      ASSERT_TRUE(args);
      EXPECT_EQ(args->contains("name"), full);
      EXPECT_EQ(args->contains("portrait"), full);
      for (auto key : {"health", "energy", "x", "y", "world", "warpMode"})
        EXPECT_EQ(args->contains(String(std::string_view(key))), true);
    };
    ASSERT_TRUE(fetchTeam());
    ASSERT_TRUE(team.currentTeam());
    auto firstTeam = team.currentTeam();
    auto initial = submit();
    ASSERT_TRUE(initial);
    checkPacket(*initial, true, "first membership");
    ASSERT_TRUE(exchange(*initial));
    ASSERT_TRUE(fetchTeam());
    auto initialPortrait = team.memberPortrait(*uuid, {});
    ASSERT_TRUE(initialPortrait);
    ASSERT_TRUE(*initialPortrait);
    auto savedPortrait = (**initialPortrait).get("portrait");
    ASSERT_TRUE(savedPortrait);
    auto delta = submit();
    ASSERT_TRUE(delta);
    checkPacket(*delta, false, "ordinary dynamic update");
    ASSERT_TRUE(exchange(*delta));
    ASSERT_TRUE(fetchTeam());
    auto ownMember = std::find_if(team.members().begin(), team.members().end(), [&](auto const& member) {
      return member.uuid == *uuid;
    });
    ASSERT_NE(ownMember, team.members().end());
    EXPECT_EQ(ownMember->name, *name);
    auto retainedPortrait = team.memberPortrait(*uuid, {});
    ASSERT_TRUE(retainedPortrait);
    ASSERT_TRUE(*retainedPortrait);
    auto actualPortrait = (**retainedPortrait).get("portrait");
    ASSERT_TRUE(actualPortrait);
    EXPECT_EQ(*actualPortrait, *savedPortrait);

    team.forceUpdate();
    auto forced = submit();
    ASSERT_TRUE(forced);
    checkPacket(*forced, true, "explicit force");
    team.forceUpdate(); // A late acknowledgment must not consume this new force.
    ASSERT_TRUE(exchange(*forced));
    auto stillForced = submit();
    ASSERT_TRUE(stillForced);
    checkPacket(*stillForced, true, "force while full reply is pending");
    ASSERT_TRUE(exchange(*stillForced));

    team.forceUpdate();
    auto failed = submit();
    ASSERT_TRUE(failed);
    checkPacket(*failed, true, "full update to be rejected");
    DataStreamExternalBuffer requestStream(failed->span());
    List<Json> requests;
    ASSERT_TRUE(requestStream.readContainer(requests));
    ASSERT_EQ(requests.size(), 1u);
    auto id = requests.front().get("id");
    ASSERT_TRUE(id);
    auto failure = DataStreamBuffer::serializeContainer(List<Json>{JsonObject{{"command", "fail"}, {"id", *id}}});
    ASSERT_TRUE(failure);
    ASSERT_TRUE(clientRpc->receive(*failure));
    ASSERT_TRUE(team.update());
    auto retried = submit();
    ASSERT_TRUE(retried);
    checkPacket(*retried, false, "legacy remote failure releases the in-flight gate");
    ASSERT_TRUE(exchange(*retried));
    auto resumedDelta = submit();
    ASSERT_TRUE(resumedDelta);
    checkPacket(*resumedDelta, false, "dynamic update after recovery");
    ASSERT_TRUE(exchange(*resumedDelta));

    (*manager)->playerDisconnected(*uuid);
    ASSERT_TRUE(formTeam());
    ASSERT_TRUE(fetchTeam());
    EXPECT_NE(team.currentTeam(), firstTeam);
    auto changedTeam = submit();
    ASSERT_TRUE(changedTeam);
    checkPacket(*changedTeam, true, "changed team");
    ASSERT_TRUE(exchange(*changedTeam));

    ASSERT_TRUE(File::write_file(String(R"([{"op":"remove","path":"/invitationPollInterval"}])"), intervalPath));
    ASSERT_TRUE((*root)->hotReload());
    auto invalidInterval = team.update();
    ASSERT_FALSE(invalidInterval);
    EXPECT_TRUE(invalidInterval.error().is<AssetException>());
    EXPECT_EQ(invalidInterval.error().diagnosticSummary()->asset, "/interface.config:invitationPollInterval");
    ASSERT_TRUE(File::write_file(intervalPatch, intervalPath));
    ASSERT_TRUE((*root)->hotReload());
    ASSERT_TRUE(team.update());
  });
}

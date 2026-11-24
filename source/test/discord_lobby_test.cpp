#include "discord/ffi.h"

import std;
import star.logging;
import star.string;
import star.list;
import star.byte_array;
import star.host_address;
import star.rpc_promise;
import star.p2p_networking_service;
import star.platform_services_pc;

#include "gtest/gtest.h"

namespace {

// Only the SDK C ABI is replaced. The SDK C++ wrappers and both PC services are
// real. Completions are delivered by the real platform event thread, never
// inline under a service lock. Tests explicitly release create/activity completions.
class DiscordApi {
public:
  using LobbyCallback = void(DISCORD_API*)(void*, EDiscordResult, DiscordLobby*);
  struct Creation {
    void* data;
    LobbyCallback callback;
  };
  struct ActivityCompletion {
    void* data;
    void(DISCORD_API* callback)(void*, EDiscordResult);
  };
  struct Calls {
    unsigned transactions = 0;
    unsigned creations = 0;
    unsigned remoteConnections = 0;
    std::vector<DiscordLobbyId> networks;
    std::vector<std::tuple<DiscordLobbyId, uint8_t, bool>> channels;
    std::vector<DiscordLobbyId> disconnectedNetworks;
    std::vector<DiscordLobbyId> deletedLobbies;
    std::vector<DiscordActivity> activities;
    std::vector<std::pair<DiscordUserId, EDiscordActivityJoinRequestReply>> replies;
  };

  DiscordApi() {
    core.destroy = destroy;
    core.run_callbacks = runCallbacks;
    core.set_log_hook = setLogHook;
    core.get_user_manager = getUserManager;
    core.get_activity_manager = getActivityManager;
    core.get_lobby_manager = getLobbyManager;
    user.get_current_user = getCurrentUser;
    activity.register_command = registerCommand;
    activity.update_activity = updateActivity;
    activity.send_request_reply = sendRequestReply;
    lobby.get_lobby_create_transaction = getCreateTransaction;
    lobby.create_lobby = createLobby;
    lobby.delete_lobby = deleteLobby;
    lobby.disconnect_lobby = disconnectLobby;
    lobby.connect_lobby = connectLobby;
    lobby.connect_network = connectNetwork;
    lobby.disconnect_network = disconnectNetwork;
    lobby.open_network_channel = openNetworkChannel;
    lobby.flush_network = flushNetwork;
    transaction.set_capacity = setCapacity;
    transaction.set_type = setType;
  }

  void enqueue(std::function<void()> callback) {
    std::lock_guard lock(mutex);
    callbacks.push_back(std::move(callback));
  }

  bool synchronize() {
    auto completed = std::make_shared<std::promise<void>>();
    auto future = completed->get_future();
    enqueue([completed] { completed->set_value(); });
    return future.wait_for(std::chrono::seconds(5)) == std::future_status::ready;
  }

  void publishUser(EDiscordResult result = DiscordResult_Ok, DiscordUserId id = 7) {
    enqueue([this, result, id] {
      userResult = result;
      userId = id;
      params.user_events->on_current_user_update(params.event_data);
    });
  }

  bool completeCreation(EDiscordResult result) {
    Creation creation;
    {
      std::lock_guard lock(mutex);
      if (pendingCreations.empty())
        return false;
      creation = pendingCreations.front();
      pendingCreations.pop_front();
    }
    enqueue([creation, result] {
      DiscordLobby created{};
      created.id = 42;
      created.owner_id = 7;
      std::strcpy(created.secret, "test-lobby-secret");
      // The SDK wrapper dereferences this pointer even on failed completions.
      creation.callback(creation.data, result, &created);
    });
    return true;
  }

  bool completeActivity(EDiscordResult result) {
    ActivityCompletion completion;
    {
      std::lock_guard lock(mutex);
      if (pendingActivities.empty())
        return false;
      completion = pendingActivities.front();
      pendingActivities.pop_front();
    }
    enqueue([completion, result] { completion.callback(completion.data, result); });
    return true;
  }

  Calls snapshot() {
    std::lock_guard lock(mutex);
    return calls;
  }

  static DiscordApi* active;
  IDiscordCore core{};
  DiscordCreateParams params{};
  bool destroyed = false;
  EDiscordResult registrationResult = DiscordResult_Ok;

private:
  // Used exclusively inside the SDK callback thread.
  EDiscordResult userResult = DiscordResult_Ok;
  DiscordUserId userId = 7;
  static void DISCORD_API destroy(IDiscordCore*) {
    std::lock_guard lock(active->mutex);
    EXPECT_TRUE(active->callbacks.empty());
    EXPECT_TRUE(active->pendingCreations.empty());
    EXPECT_TRUE(active->pendingActivities.empty());
    active->destroyed = true;
  }

  static EDiscordResult DISCORD_API runCallbacks(IDiscordCore*) {
    for (;;) {
      std::function<void()> callback;
      {
        std::lock_guard lock(active->mutex);
        if (active->callbacks.empty())
          break;
        callback = std::move(active->callbacks.front());
        active->callbacks.pop_front();
      }
      callback();
    }
    return DiscordResult_Ok;
  }

  static void DISCORD_API setLogHook(IDiscordCore*, EDiscordLogLevel, void*,
      void(DISCORD_API*)(void*, EDiscordLogLevel, char const*)) {}
  static IDiscordUserManager* DISCORD_API getUserManager(IDiscordCore*) {
    return &active->user;
  }
  static IDiscordActivityManager* DISCORD_API getActivityManager(IDiscordCore*) {
    return &active->activity;
  }
  static IDiscordLobbyManager* DISCORD_API getLobbyManager(IDiscordCore*) {
    return &active->lobby;
  }
  static EDiscordResult DISCORD_API getCurrentUser(IDiscordUserManager*, DiscordUser* out) {
    if (active->userResult != DiscordResult_Ok)
      return active->userResult;
    *out = {};
    out->id = active->userId;
    std::strcpy(out->username, "regression-user");
    return DiscordResult_Ok;
  }
  static EDiscordResult DISCORD_API registerCommand(IDiscordActivityManager*, char const*) {
    return active->registrationResult;
  }
  static void DISCORD_API updateActivity(IDiscordActivityManager*, DiscordActivity* value,
      void* data, void(DISCORD_API* callback)(void*, EDiscordResult)) {
    std::lock_guard lock(active->mutex);
    active->calls.activities.push_back(*value);
    active->pendingActivities.push_back({data, callback});
  }
  static void DISCORD_API sendRequestReply(IDiscordActivityManager*, DiscordUserId id,
      EDiscordActivityJoinRequestReply reply, void* data, void(DISCORD_API* callback)(void*, EDiscordResult)) {
    {
      std::lock_guard lock(active->mutex);
      active->calls.replies.emplace_back(id, reply);
    }
    active->enqueue([data, callback] { callback(data, DiscordResult_NotFound); });
  }
  static EDiscordResult DISCORD_API getCreateTransaction(IDiscordLobbyManager*,
      IDiscordLobbyTransaction** out) {
    std::lock_guard lock(active->mutex);
    ++active->calls.transactions;
    *out = &active->transaction;
    return DiscordResult_Ok;
  }
  static EDiscordResult DISCORD_API setCapacity(IDiscordLobbyTransaction*, uint32_t) {
    return DiscordResult_Ok;
  }
  static EDiscordResult DISCORD_API setType(IDiscordLobbyTransaction*, EDiscordLobbyType) {
    return DiscordResult_Ok;
  }
  static void DISCORD_API createLobby(IDiscordLobbyManager*, IDiscordLobbyTransaction*,
      void* data, LobbyCallback callback) {
    std::lock_guard lock(active->mutex);
    ++active->calls.creations;
    active->pendingCreations.push_back({data, callback});
  }
  static void DISCORD_API deleteLobby(IDiscordLobbyManager*, DiscordLobbyId id,
      void* data, void(DISCORD_API* callback)(void*, EDiscordResult)) {
    {
      std::lock_guard lock(active->mutex);
      active->calls.deletedLobbies.push_back(id);
    }
    active->enqueue([data, callback] { callback(data, DiscordResult_Ok); });
  }
  static void DISCORD_API disconnectLobby(IDiscordLobbyManager*, DiscordLobbyId,
      void* data, void(DISCORD_API* callback)(void*, EDiscordResult)) {
    active->enqueue([data, callback] { callback(data, DiscordResult_Ok); });
  }
  static void DISCORD_API connectLobby(IDiscordLobbyManager*, DiscordLobbyId,
      DiscordLobbySecret, void* data, LobbyCallback callback) {
    {
      std::lock_guard lock(active->mutex);
      ++active->calls.remoteConnections;
    }
    active->enqueue([data, callback] {
      DiscordLobby value{};
      callback(data, DiscordResult_InvalidCommand, &value);
    });
  }
  static EDiscordResult DISCORD_API connectNetwork(IDiscordLobbyManager*, DiscordLobbyId id) {
    std::lock_guard lock(active->mutex);
    active->calls.networks.push_back(id);
    return DiscordResult_Ok;
  }
  static EDiscordResult DISCORD_API disconnectNetwork(IDiscordLobbyManager*, DiscordLobbyId id) {
    std::lock_guard lock(active->mutex);
    active->calls.disconnectedNetworks.push_back(id);
    return DiscordResult_Ok;
  }
  static EDiscordResult DISCORD_API openNetworkChannel(IDiscordLobbyManager*, DiscordLobbyId id,
      uint8_t channel, bool reliable) {
    std::lock_guard lock(active->mutex);
    active->calls.channels.emplace_back(id, channel, reliable);
    return DiscordResult_Ok;
  }
  static EDiscordResult DISCORD_API flushNetwork(IDiscordLobbyManager*) {
    return DiscordResult_Ok;
  }

  IDiscordUserManager user{};
  IDiscordActivityManager activity{};
  IDiscordLobbyManager lobby{};
  IDiscordLobbyTransaction transaction{};
  std::mutex mutex;
  std::deque<std::function<void()>> callbacks;
  std::deque<Creation> pendingCreations;
  std::deque<ActivityCompletion> pendingActivities;
  Calls calls;
};

DiscordApi* DiscordApi::active = nullptr;

class DiscordLobbyTest : public testing::Test {
protected:
  void SetUp() override {
    ASSERT_EQ(DiscordApi::active, nullptr);
    DiscordApi::active = &api;
    logger.remove_stdout_sink();
    auto created = Star::PcPlatformServices::create(logger, "test-starbound", {});
    ASSERT_TRUE(created) << created.error().what();
    platform = std::move(*created);
    networking = platform->p2pNetworkingService();
    ASSERT_TRUE(networking);
    api.publishUser();
    ASSERT_TRUE(api.synchronize());
    ASSERT_TRUE(platform->update());
  }

  void TearDown() override {
    if (networking) {
      EXPECT_TRUE(networking->setJoinUnavailable());
      // Drain every SDK-owned std::function before releasing its service/state.
      while (api.completeCreation(DiscordResult_InvalidCommand)) {}
      while (api.completeActivity(DiscordResult_Ok)) {}
      EXPECT_TRUE(api.synchronize());
      EXPECT_TRUE(api.synchronize());
    }
    networking.reset();
    platform.reset();
    EXPECT_TRUE(api.destroyed);
    DiscordApi::active = nullptr;
  }

  DiscordApi api;
  Star::Logger logger;
  std::unique_ptr<Star::PcPlatformServices> platform;
  std::shared_ptr<Star::P2PNetworkingService> networking;
};

struct DiscordRegistrationFailure : DiscordLobbyTest {
  DiscordRegistrationFailure() { api.registrationResult = DiscordResult_InvalidCommand; }
};

} // namespace

extern "C" EDiscordResult DISCORD_API DiscordCreate(DiscordVersion version,
    DiscordCreateParams* params, IDiscordCore** result) {
  if (!DiscordApi::active || version != DISCORD_VERSION)
    return DiscordResult_InternalError;
  EXPECT_EQ(params->flags, DiscordCreateFlags_NoRequireDiscord);
  DiscordApi::active->params = *params;
  *result = &DiscordApi::active->core;
  return DiscordResult_Ok;
}

TEST_F(DiscordRegistrationFailure, CanStillPublishActivity) {
  ASSERT_TRUE(networking->setActivityData("In Game", "On ship", 123, {}));
  ASSERT_TRUE(api.completeActivity(DiscordResult_Ok));
  ASSERT_TRUE(api.synchronize());
  ASSERT_TRUE(platform->update());
  auto calls = api.snapshot();
  ASSERT_EQ(calls.activities.size(), 1u);
  EXPECT_STREQ(calls.activities.front().details, "On ship");
}

TEST_F(DiscordLobbyTest, UnsupportedCreationKeepsPlatformAndRichPresenceAliveWithoutRetrying) {
  ASSERT_TRUE(networking->setJoinLocal(8));
  ASSERT_TRUE(networking->setActivityData("Exploring", "Before downgrade", 123, {}));
  ASSERT_TRUE(api.completeActivity(DiscordResult_Ok));
  ASSERT_TRUE(api.synchronize());
  ASSERT_TRUE(api.completeCreation(DiscordResult_InvalidCommand));
  ASSERT_TRUE(api.synchronize());

  // This is the consumer-visible failure boundary that used to terminate play.
  ASSERT_TRUE(platform->update());
  ASSERT_TRUE(networking->update());

  for (unsigned capacity : {4u, 8u, 16u}) {
    ASSERT_TRUE(networking->setJoinUnavailable());
    ASSERT_TRUE(networking->setJoinLocal(capacity));
    ASSERT_TRUE(platform->update());
  }
  ASSERT_TRUE(networking->setActivityData("Exploring", "After downgrade", 123, {}));
  ASSERT_TRUE(api.completeActivity(DiscordResult_Ok));
  ASSERT_TRUE(api.synchronize());
  ASSERT_TRUE(platform->update());

  auto calls = api.snapshot();
  EXPECT_EQ(calls.transactions, 1u);
  EXPECT_EQ(calls.creations, 1u);
  EXPECT_TRUE(calls.networks.empty());
  ASSERT_EQ(calls.activities.size(), 2u);
  EXPECT_STREQ(calls.activities.back().state, "Exploring");
  EXPECT_STREQ(calls.activities.back().details, "After downgrade");
  EXPECT_STREQ(calls.activities.back().secrets.join, "");

  auto connected = networking->connectToPeer(Star::P2PNetworkingPeerId("discord_9_42_secret"));
  ASSERT_FALSE(connected);
  EXPECT_EQ(api.snapshot().remoteConnections, 0u);
  EXPECT_TRUE(platform->update());
}

TEST_F(DiscordLobbyTest, ActivityFailureAfterUnsupportedLobbyIsNonfatalAndDoesNotRetryUnchangedPayload) {
  ASSERT_TRUE(networking->setJoinLocal(8));
  ASSERT_TRUE(api.completeCreation(DiscordResult_InvalidCommand));
  ASSERT_TRUE(api.synchronize());
  ASSERT_TRUE(platform->update());

  ASSERT_TRUE(networking->setActivityData("Exploring", "Failed presence", 123, {}));
  // Identical requests must not add work while the RPC is outstanding.
  ASSERT_TRUE(networking->setActivityData("Exploring", "Failed presence", 123, {}));
  ASSERT_EQ(api.snapshot().activities.size(), 1u);
  ASSERT_TRUE(api.completeActivity(DiscordResult_InternalError));
  ASSERT_TRUE(api.synchronize());
  ASSERT_TRUE(platform->update());
  ASSERT_TRUE(networking->update());

  // A failed optional publish is still the last attempted payload, not a retry
  // trigger on every game update.
  for (unsigned i = 0; i != 3; ++i) {
    ASSERT_TRUE(networking->setActivityData("Exploring", "Failed presence", 123, {}));
    ASSERT_TRUE(platform->update());
    ASSERT_TRUE(networking->update());
  }
  EXPECT_EQ(api.snapshot().activities.size(), 1u);

  // Failure must release the in-flight guard so a changed payload can publish.
  ASSERT_TRUE(networking->setActivityData("Exploring", "Recovered presence", 123, {}));
  ASSERT_EQ(api.snapshot().activities.size(), 2u);
  ASSERT_TRUE(api.completeActivity(DiscordResult_Ok));
  ASSERT_TRUE(api.synchronize());
  ASSERT_TRUE(platform->update());
  ASSERT_TRUE(networking->update());
  ASSERT_TRUE(networking->setActivityData("Exploring", "Recovered presence", 123, {}));
  auto calls = api.snapshot();
  ASSERT_EQ(calls.activities.size(), 2u);
  EXPECT_STREQ(calls.activities.back().details, "Recovered presence");
  EXPECT_STREQ(calls.activities.back().secrets.join, "");
  EXPECT_EQ(calls.creations, 1u);
  EXPECT_TRUE(calls.networks.empty());
}

TEST_F(DiscordLobbyTest, SuccessfulCreationConnectsReliableChannelAndPublishesJoinSecret) {
  ASSERT_TRUE(networking->setJoinLocal(8));
  ASSERT_TRUE(api.completeCreation(DiscordResult_Ok));
  ASSERT_TRUE(api.synchronize());
  ASSERT_TRUE(platform->update());
  ASSERT_TRUE(networking->setActivityData("Hosting", "A local game", 456, {}));
  ASSERT_TRUE(api.completeActivity(DiscordResult_Ok));
  ASSERT_TRUE(api.synchronize());

  auto calls = api.snapshot();
  EXPECT_EQ(calls.creations, 1u);
  EXPECT_EQ(calls.networks, (std::vector<DiscordLobbyId>{42}));
  EXPECT_EQ(calls.channels, (std::vector<std::tuple<DiscordLobbyId, uint8_t, bool>>{{42, 0, true}}));
  ASSERT_EQ(calls.activities.size(), 1u);
  EXPECT_STREQ(calls.activities.back().party.id, "42");
  EXPECT_STREQ(calls.activities.back().secrets.join, "connect:discord_7_42_test-lobby-secret");

  ASSERT_TRUE(networking->setJoinUnavailable());
  ASSERT_TRUE(api.synchronize());
  EXPECT_EQ(api.snapshot().disconnectedNetworks, (std::vector<DiscordLobbyId>{42}));
  EXPECT_EQ(api.snapshot().deletedLobbies, (std::vector<DiscordLobbyId>{42}));
  EXPECT_TRUE(platform->update());
}

TEST_F(DiscordLobbyTest, CancelledCreationFailureDoesNotDisableTheNextGeneration) {
  ASSERT_TRUE(networking->setJoinLocal(4));
  ASSERT_TRUE(networking->setJoinUnavailable());
  ASSERT_TRUE(networking->setJoinLocal(8));
  ASSERT_TRUE(api.completeCreation(DiscordResult_InvalidCommand));
  ASSERT_TRUE(api.synchronize());
  ASSERT_TRUE(platform->update());
  ASSERT_TRUE(api.completeCreation(DiscordResult_Ok));
  ASSERT_TRUE(api.synchronize());
  ASSERT_TRUE(platform->update());

  ASSERT_TRUE(networking->setJoinUnavailable());
  ASSERT_TRUE(networking->setJoinLocal(16));
  EXPECT_EQ(api.snapshot().creations, 3u);
  ASSERT_TRUE(api.completeCreation(DiscordResult_Ok));
  ASSERT_TRUE(api.synchronize());
  EXPECT_TRUE(platform->update());
  EXPECT_EQ(api.snapshot().networks, (std::vector<DiscordLobbyId>{42, 42}));
}

TEST_F(DiscordLobbyTest, OtherCreationFailuresRemainDiagnosedAtThePlatformBoundary) {
  ASSERT_TRUE(networking->setJoinLocal(8));
  ASSERT_TRUE(api.completeCreation(DiscordResult_InternalError));
  ASSERT_TRUE(api.synchronize());
  auto updated = platform->update();
  ASSERT_FALSE(updated);
  EXPECT_TRUE(platform->update());
}

TEST_F(DiscordLobbyTest, InvalidInvitationsPreservePendingDestination) {
  api.enqueue([&] {
    auto join = api.params.activity_events->on_activity_join;
    join(api.params.event_data, "connect:address_127.0.0.1:21025");
    join(api.params.event_data, "connect:address_127.0.0.1:not-a-port");
    join(api.params.event_data, "connect:discord_9_42_");
    join(api.params.event_data, "connect:unknown_9");
  });
  ASSERT_TRUE(api.synchronize());
  ASSERT_TRUE(platform->update());
  auto pending = networking->pullPendingJoin();
  auto destination = std::get_if<Star::HostAddressWithPort>(&pending);
  ASSERT_NE(destination, nullptr);
  EXPECT_EQ(destination->port(), 21025);
  EXPECT_TRUE(std::holds_alternative<std::monostate>(networking->pullPendingJoin()));
}

TEST_F(DiscordLobbyTest, UndeliverableRepliesAreConsumedOnce) {
  for (auto decision : {Star::P2PJoinRequestReply::Yes, Star::P2PJoinRequestReply::Ignore}) {
    api.enqueue([&] {
      DiscordUser requester{};
      requester.id = 9;
      api.params.activity_events->on_activity_join_request(api.params.event_data, &requester);
    });
    ASSERT_TRUE(api.synchronize());
    auto request = networking->pullJoinRequest();
    ASSERT_TRUE(request);
    ASSERT_TRUE(request->second.fulfill(decision));
    ASSERT_TRUE(networking->update());
    ASSERT_TRUE(api.synchronize());
    ASSERT_TRUE(platform->update());
    ASSERT_TRUE(networking->update());
  }
  EXPECT_EQ(api.snapshot().replies, (std::vector<std::pair<DiscordUserId, EDiscordActivityJoinRequestReply>>{
    {9, DiscordActivityJoinRequestReply_Yes}, {9, DiscordActivityJoinRequestReply_Ignore}}));
}

TEST_F(DiscordLobbyTest, IdentityLossWithdrawsSecretWithoutDisconnectingPeer) {
  ASSERT_TRUE(networking->setJoinLocal(8));
  ASSERT_TRUE(api.completeCreation(DiscordResult_Ok));
  ASSERT_TRUE(api.synchronize());
  networking->setAcceptingP2PConnections(true);
  api.enqueue([&] { api.params.lobby_events->on_member_connect(api.params.event_data, 42, 9); });
  ASSERT_TRUE(api.synchronize());
  auto peers = networking->acceptP2PConnections();
  ASSERT_EQ(peers.size(), 1u);

  auto publish = [&] { return networking->setActivityData("Hosting", "On ship", 123, {}); };
  ASSERT_TRUE(publish());
  api.publishUser(DiscordResult_NotFound);
  ASSERT_TRUE(api.synchronize());
  ASSERT_TRUE(platform->update());
  ASSERT_TRUE(api.completeActivity(DiscordResult_Ok)); // Old secret was already in flight.
  ASSERT_TRUE(api.synchronize());
  ASSERT_TRUE(publish());
  auto activities = api.snapshot().activities;
  ASSERT_EQ(activities.size(), 2u);
  EXPECT_STREQ(activities[0].secrets.join, "connect:discord_7_42_test-lobby-secret");
  EXPECT_STREQ(activities[1].secrets.join, "");
  ASSERT_TRUE(api.completeActivity(DiscordResult_Ok));

  api.enqueue([&] {
    auto events = api.params.lobby_events;
    events->on_member_update(api.params.event_data, 42, 10);
    uint8_t payload = 42;
    events->on_network_message(api.params.event_data, 42, 10, 0, &payload, 1);
    events->on_network_message(api.params.event_data, 42, 9, 0, &payload, 1);
  });
  ASSERT_TRUE(api.synchronize());
  ASSERT_TRUE(platform->update());
  EXPECT_TRUE(networking->acceptP2PConnections().empty());
  EXPECT_TRUE(peers.front()->isOpen());
  auto received = peers.front()->receiveMessage();
  ASSERT_TRUE(received && *received);
  EXPECT_EQ((**received).size(), 1u);

  api.enqueue([&] { api.params.lobby_events->on_member_disconnect(api.params.event_data, 42, 9); });
  ASSERT_TRUE(api.synchronize());
  ASSERT_TRUE(platform->update());
  EXPECT_FALSE(peers.front()->isOpen());
  api.publishUser();
  ASSERT_TRUE(api.synchronize());
  ASSERT_TRUE(publish());
  activities = api.snapshot().activities;
  ASSERT_EQ(activities.size(), 3u);
  EXPECT_STREQ(activities.back().secrets.join, "connect:discord_7_42_test-lobby-secret");
}

TEST_F(DiscordLobbyTest, ClosedAdmissionDrainsLobbyAfterLastSocket) {
  networking->setAcceptingP2PConnections(true);
  ASSERT_TRUE(networking->setJoinLocal(8));
  ASSERT_TRUE(api.completeCreation(DiscordResult_Ok));
  ASSERT_TRUE(api.synchronize());
  api.enqueue([&] {
    for (int id : {8, 9})
      api.params.lobby_events->on_member_connect(api.params.event_data, 42, id);
  });
  ASSERT_TRUE(api.synchronize());
  auto peers = networking->acceptP2PConnections();
  ASSERT_EQ(peers.size(), 2u);

  networking->setAcceptingP2PConnections(false);
  ASSERT_TRUE(networking->setJoinUnavailable());
  ASSERT_TRUE(networking->setActivityData("Hosting", "On ship", 123, {}));
  auto calls = api.snapshot();
  ASSERT_EQ(calls.activities.size(), 1u);
  EXPECT_STREQ(calls.activities.front().secrets.join, "");
  EXPECT_TRUE(calls.disconnectedNetworks.empty());
  api.enqueue([&] {
    api.params.lobby_events->on_member_connect(api.params.event_data, 42, 10);
    api.params.lobby_events->on_member_disconnect(api.params.event_data, 42, 8);
    uint8_t payload = 1;
    api.params.lobby_events->on_network_message(api.params.event_data, 42, 9, 0, &payload, 1);
  });
  ASSERT_TRUE(api.synchronize());
  ASSERT_TRUE(platform->update());
  EXPECT_TRUE(networking->acceptP2PConnections().empty());
  EXPECT_FALSE(peers[0]->isOpen());
  EXPECT_TRUE(peers[1]->isOpen());
  auto received = peers[1]->receiveMessage();
  ASSERT_TRUE(received && *received);
  EXPECT_TRUE(api.snapshot().deletedLobbies.empty());
  peers.clear();
  ASSERT_TRUE(api.synchronize());
  calls = api.snapshot();
  EXPECT_EQ(calls.disconnectedNetworks, (std::vector<DiscordLobbyId>{42}));
  EXPECT_EQ(calls.deletedLobbies, (std::vector<DiscordLobbyId>{42}));
  EXPECT_TRUE(platform->update());
}

TEST_F(DiscordLobbyTest, RemoteJoinFailureIsReportedByItsSocket) {
  auto connection = networking->connectToPeer(Star::P2PNetworkingPeerId("discord_9_42_test-secret"));
  ASSERT_TRUE(connection);
  ASSERT_TRUE(api.synchronize());
  EXPECT_TRUE(platform->update());
  EXPECT_FALSE((*connection)->isOpen());
  EXPECT_FALSE((*connection)->receiveMessage());
}

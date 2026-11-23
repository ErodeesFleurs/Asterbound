#include "gtest/gtest.h"

import std;
import star.chat_processor;
import star.chat_types;
import star.format;
import star.game_types;
import star.json;
import star.list;
import star.message_types;
import star.rpc_promise;
import star.string;

using namespace Star;

namespace {

using CommandResult = std::expected<ServerCommandResult, StarException>;

void expect_inbox(ChatProcessor& chat, ConnectionId id, std::initializer_list<String> texts) {
  auto messages = chat.pull_pending_messages(id);
  ASSERT_TRUE(messages);
  ASSERT_EQ(messages->size(), texts.size());
  auto message = messages->begin();
  for (auto const& text : texts)
    EXPECT_EQ((message++)->text, text);
}

TEST(ChatRouting, NickIndexReservesServerAndTracksTheActualSession) {
  ChatProcessor chat;
  ASSERT_EQ(chat.connect_client(1, "Alpha"), String("Alpha"));
  ASSERT_EQ(chat.connect_client(2, "Beta"), String("Beta"));
  expect_inbox(chat, 1, {"Player 'Beta' connected"});
  EXPECT_FALSE(chat.connect_client(2, "Impostor"));
  EXPECT_FALSE(chat.connect_client(server_connection_id, "Impostor"));
  EXPECT_FALSE(chat.find_nick("Impostor"));
  expect_inbox(chat, 1, {});
  expect_inbox(chat, 2, {});

  ASSERT_EQ(chat.renick(2, "Alpha"), String("Alpha_"));
  EXPECT_EQ(chat.renick(2, "Alpha"), String("Alpha_")); // The first free suffix is already ours.
  EXPECT_EQ(chat.renick(2, "Alpha_"), String("Alpha_"));
  EXPECT_FALSE(chat.find_nick("Beta"));
  EXPECT_EQ(chat.find_nick("Alpha"), 1);
  EXPECT_EQ(chat.find_nick("Alpha_"), 2);
  EXPECT_EQ(chat.connection_nick(2), String("Alpha_"));
  ASSERT_TRUE(chat.broadcast(1, "/w Alpha_ hello"));
  auto received = chat.pull_pending_messages(2);
  ASSERT_TRUE(received);
  ASSERT_EQ(received->size(), 1u);
  EXPECT_EQ(received->front().context.mode, MessageContext::Whisper);
  EXPECT_EQ(received->front().from_connection, 1);
  EXPECT_EQ(received->front().from_nick, "Alpha");
  EXPECT_EQ(received->front().text, "hello");
  expect_inbox(chat, 1, {"hello"});

  ASSERT_TRUE(chat.disconnect_client(2));
  EXPECT_FALSE(chat.find_nick("Alpha_"));
  EXPECT_EQ(chat.find_nick("Alpha"), 1);
  ASSERT_TRUE(chat.connect_client(3, "server"));
  EXPECT_EQ(chat.connection_nick(3), String("server_"));
  ASSERT_TRUE(chat.broadcast(1, "/nick server"));
  EXPECT_EQ(chat.connection_nick(1), String("server__"));
  EXPECT_EQ(chat.find_nick("server"), server_connection_id);
  EXPECT_EQ(chat.find_nick("server_"), 3);
  EXPECT_EQ(chat.find_nick("server__"), 1);
  EXPECT_FALSE(chat.find_nick("Alpha"));
  ASSERT_TRUE(chat.broadcast(3, "/w server__ hi"));
  expect_inbox(chat, 1, {"Player 'Alpha_' disconnected", "Player 'server_' connected", "Nick changed to server__", "hi"});
}

TEST(ChatRouting, WhispersValidateBothEndpointsBeforeCommandsOrEcho) {
  ChatProcessor chat;
  ASSERT_TRUE(chat.connect_client(1));
  ASSERT_TRUE(chat.connect_client(2));
  expect_inbox(chat, 1, {"Player 'Player_2' connected"});
  EXPECT_FALSE(chat.whisper(1, 42, "no echo"));
  EXPECT_FALSE(chat.whisper(42, 2, "no receipt"));
  EXPECT_FALSE(chat.whisper(1, 42, "/nick changed"));
  EXPECT_EQ(chat.connection_nick(1), String("Player_1"));
  EXPECT_FALSE(chat.broadcast(1, String(std::string("hi\xff", 3))));
  expect_inbox(chat, 1, {});
  expect_inbox(chat, 2, {});
  ASSERT_TRUE(chat.whisper(1, 1, "//escaped", {{"tag", 42}}));
  auto self = chat.pull_pending_messages(1);
  ASSERT_TRUE(self);
  ASSERT_EQ(self->size(), 2u);
  for (auto const& msg : *self) {
    EXPECT_EQ(msg.context.mode, MessageContext::Whisper);
    EXPECT_EQ(msg.from_connection, 1);
    EXPECT_EQ(msg.text, "/escaped");
    EXPECT_EQ(msg.data, (JsonObject{{"tag", 42}}));
  }
  ASSERT_TRUE(chat.admin_whisper(2, "private"));
  expect_inbox(chat, 1, {});
  expect_inbox(chat, 2, {"private"});
}

TEST(ChatRouting, ChannelMembershipExistsOnlyForConnectedMembers) {
  ChatProcessor chat;
  ASSERT_TRUE(chat.connect_client(1));
  ASSERT_TRUE(chat.connect_client(2));
  expect_inbox(chat, 1, {"Player 'Player_2' connected"});
  EXPECT_FALSE(chat.join_channel(3, "unknown"));
  EXPECT_FALSE(chat.leave_channel(1, "unknown"));
  ASSERT_TRUE(chat.message(1, MessageContext::Local, "unknown", "ignored"));
  EXPECT_TRUE(chat.active_channels().empty());
  EXPECT_TRUE(chat.join_channel(1, "shared"));
  EXPECT_FALSE(chat.join_channel(1, "shared"));
  EXPECT_TRUE(chat.join_channel(2, "shared"));
  EXPECT_TRUE(chat.leave_channel(1, "shared"));
  EXPECT_FALSE(chat.leave_channel(1, "shared"));
  EXPECT_TRUE(chat.join_channel(1, "shared"));
  for (unsigned i = 0; i < 80; ++i)
    ASSERT_TRUE(chat.join_channel(1, Star::format("solo-{}", i)));
  ASSERT_TRUE(chat.disconnect_client(1));
  EXPECT_TRUE(chat.client_channels(1).empty());
  EXPECT_EQ(chat.active_channels(), StringList{"shared"});
  EXPECT_EQ(chat.client_channels(2), StringList{"shared"});
  ASSERT_TRUE(chat.admin_message(MessageContext::Local, "shared", "still connected"));
  expect_inbox(chat, 2, {"Player 'Player_1' disconnected", "still connected"});
  EXPECT_TRUE(chat.leave_channel(2, "shared"));
  EXPECT_TRUE(chat.active_channels().empty());
}

TEST(ChatRouting, AbandonedCommandsCannotStopLiveRepliesOrReachReusedIds) {
  for (bool reuse_id : {false, true}) {
    ChatProcessor chat;
    ASSERT_TRUE(chat.connect_client(1, "old"));
    ASSERT_TRUE(chat.connect_client(2, "live"));
    auto [old, old_keeper] = RpcPromise<String>::make_pair();
    auto [live, live_keeper] = RpcPromise<String>::make_pair();
    chat.set_command_handler([&](ConnectionId id, String, String) -> CommandResult { return id == 1 ? old : live; });
    ASSERT_TRUE(chat.broadcast(1, "/old"));
    ASSERT_TRUE(chat.broadcast(2, "/live"));
    ASSERT_TRUE(chat.update_promises());
    ASSERT_TRUE(chat.disconnect_client(1));
    if (reuse_id) ASSERT_TRUE(chat.connect_client(1, "new"));
    ASSERT_TRUE(chat.pull_pending_messages(2));
    ASSERT_TRUE(old_keeper.fulfill("old secret"));
    EXPECT_EQ(old.status(), RpcPromiseStatus::Succeeded); // Dropping the inbox did not cancel the operation.
    ASSERT_TRUE(live_keeper.fail("live failure"));
    ASSERT_TRUE(chat.update_promises());
    expect_inbox(chat, 2, {"Error: live failure"});
    ASSERT_TRUE(chat.update_promises());
    expect_inbox(chat, 1, {});
    expect_inbox(chat, 2, {});
  }
}

TEST(ChatRouting, HandlerCanReplaceItselfAndRouteBeforeReturning) {
  ChatProcessor chat;
  ASSERT_TRUE(chat.connect_client(1));
  auto capture = std::make_shared<int>(0);
  std::weak_ptr<int> lifetime = capture;
  chat.set_command_handler([&, capture = std::move(capture), calls = 0](ConnectionId id, String command, String args) mutable -> CommandResult {
    if (++calls == 1) return String("first");
    EXPECT_EQ(command, "again");
    EXPECT_EQ(args.trim(), String("remaining words"));
    chat.set_command_handler([](ConnectionId, String, String) -> CommandResult { return String("nested result"); });
    EXPECT_FALSE(lifetime.expired());
    auto sent = chat.admin_whisper(id, "before");
    if (!sent) return std::unexpected(std::move(sent.error()));
    sent = chat.broadcast(id, "/nested");
    if (!sent) return std::unexpected(std::move(sent.error()));
    return String("outer result");
  });
  ASSERT_TRUE(chat.broadcast(1, "/first"));
  ASSERT_TRUE(chat.broadcast(1, "/again remaining words"));
  EXPECT_TRUE(lifetime.expired());
  expect_inbox(chat, 1, {"first", "before", "nested result", "outer result"});
  chat.clear_command_handler();
  ASSERT_TRUE(chat.broadcast(1, "/gone"));
  expect_inbox(chat, 1, {"No such command gone"});
}

TEST(ChatRouting, ResultsBelongToSessionBeforeHandlerReentry) {
  for (bool asynchronous : {false, true}) {
    ChatProcessor chat;
    ASSERT_TRUE(chat.connect_client(1));
    auto [promise, keeper] = RpcPromise<String>::make_pair();
    chat.set_command_handler([&](ConnectionId id, String, String) -> CommandResult {
      auto removed = chat.disconnect_client(id);
      if (!removed) return std::unexpected(std::move(removed.error()));
      auto added = chat.connect_client(id);
      if (!added) return std::unexpected(std::move(added.error()));
      if (asynchronous) return promise;
      return String("secret");
    });
    ASSERT_TRUE(chat.broadcast(1, "/replace"));
    ASSERT_TRUE(keeper.fulfill("deferred secret"));
    ASSERT_TRUE(chat.update_promises());
    expect_inbox(chat, 1, {});
  }
}

TEST(ChatRouting, PollCallbacksCanResetReuseAndQueueAnotherPoll) {
  ChatProcessor chat;
  ASSERT_TRUE(chat.connect_client(1));
  auto source = RpcPromise<String>::make_fulfilled("abandoned");
  auto wrapped = source.wrap([&](String) -> std::expected<String, StarException> {
    chat.reset();
    auto added = chat.connect_client(1);
    if (!added) return std::unexpected(std::move(added.error()));
    auto sent = chat.broadcast(1, "/new");
    if (!sent) return std::unexpected(std::move(sent.error()));
    auto nested = chat.update_promises();
    if (!nested) return std::unexpected(std::move(nested.error()));
    return std::unexpected(StarException("abandoned transform diagnostic"));
  });
  ASSERT_TRUE(wrapped);
  chat.set_command_handler([&](ConnectionId, String command, String) -> CommandResult {
    if (command == "reset") return *wrapped;
    return RpcPromise<String>::make_fulfilled(std::move(command));
  });
  ASSERT_TRUE(chat.broadcast(1, "/reset"));
  ASSERT_TRUE(chat.broadcast(1, "/old"));
  ASSERT_TRUE(chat.update_promises());
  expect_inbox(chat, 1, {});
  ASSERT_TRUE(chat.update_promises());
  expect_inbox(chat, 1, {"new"});
  ASSERT_TRUE(chat.update_promises());
  expect_inbox(chat, 1, {});
}

} // namespace

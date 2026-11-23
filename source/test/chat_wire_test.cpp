import std;
import star.chat_types;
import star.data_stream;
import star.data_stream_devices;
import star.format;
import star.json;
import star.net_packets;
import star.string;
#include "gtest/gtest.h"

using namespace Star;

namespace {

void expect_message(ChatReceivedMessage const& actual, ChatReceivedMessage const& expected) {
  EXPECT_EQ(actual.context.mode, expected.context.mode);
  EXPECT_EQ(actual.context.channel_name, expected.context.channel_name);
  EXPECT_EQ(actual.from_connection, expected.from_connection);
  EXPECT_EQ(actual.from_nick, expected.from_nick);
  EXPECT_EQ(actual.portrait, expected.portrait);
  EXPECT_EQ(actual.text, expected.text);
  EXPECT_EQ(actual.data, expected.data);
}

TEST(ChatWire, ReceivedMessageBytesAndReusedTarget) {
  constexpr std::array<char, 16> fixture{1, 1, 'c', 0x12, 0x34, 1, 'n', 1, 'p', 1, 't', 1, 1, 'd', 3, 1};
  ChatReceivedMessage message{{MessageContext::Party, "c"}, 0x1234, "n", "t", "p", {{"d", true}}};
  ChatReceivedMessage sentinel{{MessageContext::World, "before"}, 7, "sender", "old text", "old portrait", {{"x", 0}}};
  ChatReceivedMessage target;
  for (unsigned version : {5, 4}) {
    SCOPED_TRACE(version);
    auto bytes = std::span(fixture).first(version == 5 ? 16 : 11);
    DataStreamBuffer encoded;
    encoded.set_stream_compatibility_version(version);
    ASSERT_TRUE(encoded.write(message));
    EXPECT_TRUE(std::ranges::equal(encoded.span(), bytes));

    auto unchanged = sentinel;
    for (std::size_t n = 0; n < bytes.size(); ++n) {
      SCOPED_TRACE(n);
      DataStreamExternalBuffer truncated(bytes.first(n));
      truncated.set_stream_compatibility_version(version);
      EXPECT_FALSE(truncated.read(unchanged));
      expect_message(unchanged, sentinel);
    }

    DataStreamExternalBuffer stream(bytes);
    stream.set_stream_compatibility_version(version);
    ASSERT_TRUE(stream.read(target));
    EXPECT_EQ(stream.remaining(), 0u);
    auto expected = message;
    if (version == 4)
      expected.data.clear();
    expect_message(target, expected);
  }
}

TEST(ChatWire, TruncatedStandaloneContextPreservesModeAndChannel) {
  constexpr std::array<char, 3> fixture{0, 1, 'c'};
  MessageContext target{MessageContext::Whisper, "original"};
  for (std::size_t n = 0; n < fixture.size(); ++n) {
    DataStreamExternalBuffer stream(std::span{fixture}.first(n));
    EXPECT_FALSE(stream.read(target));
    EXPECT_EQ(target.mode, MessageContext::Whisper);
    EXPECT_EQ(target.channel_name, "original");
  }
}

TEST(ChatWire, SendPacketBytesAndReusedTarget) {
  constexpr std::array<char, 8> fixture{1, 't', 2, 1, 1, 'd', 3, 1};
  ChatSendPacket const packet("t", ChatSendMode::Party, {{"d", true}});
  ChatSendPacket target;
  for (unsigned version : {5, 4}) {
    SCOPED_TRACE(version);
    auto bytes = std::span(fixture).first(version == 5 ? 8 : 3);
    DataStreamBuffer encoded;
    encoded.set_stream_compatibility_version(version);
    ASSERT_TRUE(packet.write(encoded));
    EXPECT_TRUE(std::ranges::equal(encoded.span(), bytes));

    JsonObject const old_data{{"x", 0}};
    ChatSendPacket unchanged("original", ChatSendMode::Local, old_data);
    for (std::size_t n = 0; n < bytes.size(); ++n) {
      SCOPED_TRACE(n);
      DataStreamExternalBuffer truncated(bytes.first(n));
      truncated.set_stream_compatibility_version(version);
      EXPECT_FALSE(unchanged.read(truncated));
      EXPECT_EQ(unchanged.text, "original");
      EXPECT_EQ(unchanged.send_mode, ChatSendMode::Local);
      EXPECT_EQ(unchanged.data, old_data);
    }

    DataStreamExternalBuffer stream(bytes);
    stream.set_stream_compatibility_version(version);
    ASSERT_TRUE(target.read(stream));
    EXPECT_EQ(stream.remaining(), 0u);
    EXPECT_EQ(target.text, packet.text);
    EXPECT_EQ(target.send_mode, packet.send_mode);
    if (version == 5) {
      EXPECT_EQ(target.data, packet.data);
    } else {
      EXPECT_TRUE(target.data.empty());
    }
  }
}

TEST(ChatWire, JsonPreservesModeNamesOrdinalsAndDefaultKeys) {
  constexpr std::array<std::string_view, 7> names{"Local", "Party", "Broadcast", "Whisper", "CommandResult", "RadioMessage", "World"};
  for (std::size_t i = 0; i < names.size(); ++i) {
    auto message = ChatReceivedMessage::create(JsonObject{{"context", JsonObject{{"mode", names[i]}}}});
    ASSERT_TRUE(message);
    EXPECT_EQ(std::to_underlying(message->context.mode), i);
    Json expected = JsonObject{{"context", JsonObject{{"mode", names[i]}, {"channelName", Json()}}},
        {"fromConnection", 0}, {"fromNick", Json()}, {"text", ""}, {"portrait", Json()}, {"data", JsonObject{}}};
    EXPECT_EQ(message->to_json(), expected);
    auto round_trip = ChatReceivedMessage::create(expected);
    ASSERT_TRUE(round_trip);
    expect_message(*round_trip, *message);
  }
}

TEST(ChatWire, JsonAcceptsOnlyExactRepresentableConnectionIds) {
  EXPECT_EQ(chat_connection_id(JsonObject{}, 37), 37u);
  EXPECT_EQ(chat_connection_id(JsonObject{{"fromConnection", Json()}}, 37), 37u);
  EXPECT_EQ(chat_connection_id(JsonObject{{"fromConnection", 0}}, 37), 0u);
  auto parse_id = [](Json value) {
    return ChatReceivedMessage::create(JsonObject{{"context", JsonObject{{"mode", "Broadcast"}}},
        {"fromConnection", std::move(value)}});
  };
  for (auto const& [value, id] : std::array<std::pair<Json, unsigned>, 6>{{
           {Json(), 0}, {0, 0}, {-0.0, 0}, {1.0, 1}, {65535, 65535}, {65535.0, 65535}}}) {
    auto result = parse_id(value);
    ASSERT_TRUE(result);
    EXPECT_EQ(result->from_connection, id);
  }
  for (Json const& value : {Json(-1), Json(65536), Json(65536.0), Json(0.5), Json(-0.5), Json(65534.5),
           Json(std::numeric_limits<double>::denorm_min()), Json(-std::numeric_limits<double>::denorm_min()),
           Json(std::numeric_limits<std::int64_t>::max()), Json(std::numeric_limits<double>::infinity()),
           Json(std::numeric_limits<double>::quiet_NaN()), Json("1"), Json(true), Json(JsonArray{})})
    EXPECT_FALSE(parse_id(value)) << value.type_name().utf8() << ", binary64 = "
        << std::bit_cast<std::uint64_t>(value.to_double().value_or(0.0));
}

}

import star.format;
import std;
import star.string;
import star.data_stream;
import star.bi_map;
import star.json;
import star.game_types;
import star.chat_types;

namespace Star {

std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, MessageContext& context) {
  MessageContext decoded;
  auto result = ds >> decoded.mode >> decoded.channel_name;
  if (!result) return result;
  context = std::move(decoded);
  return std::ref(ds);
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, MessageContext const& context) {
  return ds << context.mode << context.channel_name;
}

std::expected<ConnectionId, StarException> chat_connection_id(Json const& json, ConnectionId fallback) {
  auto value = json.get_double("fromConnection", fallback);
  if (!value) return std::unexpected(std::move(value.error()));

  // Classify the encoding before any float-to-int conversion. In a fast-math
  // build a floating comparison can treat a subnormal as zero.
  static_assert(std::numeric_limits<double>::is_iec559);
  auto bits = std::bit_cast<std::uint64_t>(*value);
  if ((bits << 1) == 0) return 0; // Both signed zeros.
  constexpr auto maximum = static_cast<double>(std::numeric_limits<ConnectionId>::max());
  if (bits >= std::bit_cast<std::uint64_t>(1.0) && bits <= std::bit_cast<std::uint64_t>(maximum)) {
    // Exponent 0..15: the low 52 - exponent significand bits must all be zero.
    auto fractional_bits = 52 - ((bits >> 52) - 1023);
    if ((bits & ((std::uint64_t{1} << fractional_bits) - 1)) == 0)
      return static_cast<ConnectionId>(*value);
  }
  return std::unexpected(JsonException("fromConnection must be an integer between 0 and 65535"));
}

std::expected<ChatReceivedMessage, StarException> ChatReceivedMessage::create(Json const& json) {
  ChatReceivedMessage message;
  auto context = json.get("context");
  if (!context) return std::unexpected(std::move(context.error()));
  auto mode_name = context->get_string("mode");
  if (!mode_name) return std::unexpected(std::move(mode_name.error()));
  auto mode = enum_value<message_context_mode_names>(*mode_name);
  if (!mode) return std::unexpected(std::move(mode.error()));
  auto channel = context->get_string("channelName", "");
  if (!channel) return std::unexpected(std::move(channel.error()));
  message.context = {*mode, std::move(*channel)};
  auto from = chat_connection_id(json, server_connection_id);
  if (!from) return std::unexpected(std::move(from.error()));
  message.from_connection = *from;
  auto nick = json.get_string("fromNick", "");
  if (!nick) return std::unexpected(std::move(nick.error()));
  message.from_nick = std::move(*nick);
  auto portrait = json.get_string("portrait", "");
  if (!portrait) return std::unexpected(std::move(portrait.error()));
  message.portrait = std::move(*portrait);
  auto text = json.get_string("text", "");
  if (!text) return std::unexpected(std::move(text.error()));
  message.text = std::move(*text);
  auto data = json.get_object("data", JsonObject());
  if (!data) return std::unexpected(std::move(data.error()));
  message.data = std::move(*data);
  return message;
}

std::expected<Json, StarException> ChatReceivedMessage::to_json() const {
  auto mode = enum_name<message_context_mode_names>(context.mode);
  if (!mode) return std::unexpected(std::move(mode.error()));
  return Json(JsonObject{
    {"context",
     JsonObject{{"mode", *mode}, {"channelName", context.channel_name.empty() ? Json() : Json(context.channel_name)}}},
    {"fromConnection", from_connection},
    {"fromNick", from_nick.empty() ? Json() : from_nick},
    {"portrait", portrait.empty() ? Json() : portrait},
    {"text", text},
    {"data", data}});
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, ChatReceivedMessage& message) {
  ChatReceivedMessage decoded;
  auto result = ds >> decoded.context >> decoded.from_connection >> decoded.from_nick >> decoded.portrait >> decoded.text;
  if (!result) return result;
  if (ds.stream_compatibility_version() >= 5) {
    result = ds >> decoded.data;
    if (!result) return result;
  }
  message = std::move(decoded);
  return std::ref(ds);
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, ChatReceivedMessage const& message) {
  auto result = ds << message.context << message.from_connection << message.from_nick << message.portrait << message.text;
  if (!result || ds.stream_compatibility_version() < 5) return result;
  return ds << message.data;
}

}

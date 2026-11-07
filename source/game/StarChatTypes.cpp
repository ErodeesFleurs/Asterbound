import star.format;
import std;
import star.string;
import star.data_stream;
import star.bi_map;
import star.json;
import star.game_types;
import star.chat_types;

namespace Star {


MessageContext::MessageContext() : mode() {}

MessageContext::MessageContext(Mode mode) : mode(mode) {}

MessageContext::MessageContext(Mode mode, String const& channelName) : mode(mode), channelName(channelName) {}


std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, MessageContext& messageContext) {
  auto mode = ds.read(messageContext.mode);
  if (!mode) return std::unexpected(std::move(mode.error()));
  auto channel = ds.read(messageContext.channelName);
  if (!channel) return std::unexpected(std::move(channel.error()));
  return std::ref(ds);
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, MessageContext const& messageContext) {
  auto mode = ds.write(messageContext.mode);
  if (!mode) return std::unexpected(std::move(mode.error()));
  auto channel = ds.write(messageContext.channelName);
  if (!channel) return std::unexpected(std::move(channel.error()));
  return std::ref(ds);
}

ChatReceivedMessage::ChatReceivedMessage() : fromConnection() {}

ChatReceivedMessage::ChatReceivedMessage(MessageContext context, ConnectionId fromConnection, String const& fromNick,
                                         String const& text)
    : context(std::move(context)), fromConnection(fromConnection), fromNick(fromNick), text(text) {}

ChatReceivedMessage::ChatReceivedMessage(MessageContext context, ConnectionId fromConnection, String const& fromNick,
                                         String const& text, String const& portrait)
    : context(std::move(context)), fromConnection(fromConnection), fromNick(fromNick), portrait(portrait), text(text) {}

std::expected<ChatReceivedMessage, StarException> ChatReceivedMessage::create(Json const& json) {
  ChatReceivedMessage message;
  auto context = json.get("context");
  if (!context) return std::unexpected(std::move(context.error()));
  auto modeName = context->getString("mode");
  if (!modeName) return std::unexpected(std::move(modeName.error()));
  auto mode = enumValue<MessageContextModeNames>(*modeName);
  if (!mode) return std::unexpected(std::move(mode.error()));
  auto channel = context->getString("channelName", "");
  if (!channel) return std::unexpected(std::move(channel.error()));
  message.context = MessageContext(*mode, *channel);
  auto from = json.getUInt("fromConnection", 0);
  if (!from) return std::unexpected(std::move(from.error()));
  message.fromConnection = *from;
  auto nick = json.getString("fromNick", "");
  if (!nick) return std::unexpected(std::move(nick.error()));
  message.fromNick = std::move(*nick);
  auto portrait = json.getString("portrait", "");
  if (!portrait) return std::unexpected(std::move(portrait.error()));
  message.portrait = std::move(*portrait);
  auto text = json.getString("text", "");
  if (!text) return std::unexpected(std::move(text.error()));
  message.text = std::move(*text);
  auto data = json.getObject("data", JsonObject());
  if (!data) return std::unexpected(std::move(data.error()));
  message.data = std::move(*data);
  return message;
}

std::expected<Json, StarException> ChatReceivedMessage::toJson() const {
  auto mode = enumName<MessageContextModeNames>(context.mode);
  if (!mode) return std::unexpected(std::move(mode.error()));
  return Json(JsonObject{
    {"context",
     JsonObject{{"mode", *mode}, {"channelName", context.channelName.empty() ? Json() : Json(context.channelName)}}},
    {"fromConnection", fromConnection},
    {"fromNick", fromNick.empty() ? Json() : fromNick},
    {"portrait", portrait.empty() ? Json() : portrait},
    {"text", text},
    {"data", data}});
}


std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, ChatReceivedMessage& receivedMessage) {
  auto context = ds.read(receivedMessage.context);
  if (!context) return std::unexpected(std::move(context.error()));
  auto from = ds.read(receivedMessage.fromConnection);
  if (!from) return std::unexpected(std::move(from.error()));
  auto nick = ds.read(receivedMessage.fromNick);
  if (!nick) return std::unexpected(std::move(nick.error()));
  auto portrait = ds.read(receivedMessage.portrait);
  if (!portrait) return std::unexpected(std::move(portrait.error()));
  auto text = ds.read(receivedMessage.text);
  if (!text) return std::unexpected(std::move(text.error()));
  if (ds.streamCompatibilityVersion() >= 5) {
    auto data = ds.read(receivedMessage.data);
    if (!data) return std::unexpected(std::move(data.error()));
  }
  return std::ref(ds);
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, ChatReceivedMessage const& receivedMessage) {
  auto context = ds.write(receivedMessage.context);
  if (!context) return std::unexpected(std::move(context.error()));
  auto from = ds.write(receivedMessage.fromConnection);
  if (!from) return std::unexpected(std::move(from.error()));
  auto nick = ds.write(receivedMessage.fromNick);
  if (!nick) return std::unexpected(std::move(nick.error()));
  auto portrait = ds.write(receivedMessage.portrait);
  if (!portrait) return std::unexpected(std::move(portrait.error()));
  auto text = ds.write(receivedMessage.text);
  if (!text) return std::unexpected(std::move(text.error()));
  if (ds.streamCompatibilityVersion() >= 5) {
    auto data = ds.write(receivedMessage.data);
    if (!data) return std::unexpected(std::move(data.error()));
  }
  return std::ref(ds);
}

}

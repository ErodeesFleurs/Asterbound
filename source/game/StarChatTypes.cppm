module;
import star.format;

import std;
import star.string;
import star.data_stream;
import star.bi_map;
import star.json;
import star.game_types;

namespace Star {

enum class ChatSendMode : std::uint8_t {
  Broadcast,
  Local,
  Party
};

inline constexpr auto ChatSendModeNames = sortedEnumNames(std::to_array<std::pair<ChatSendMode, std::string_view>>(
  {{ChatSendMode::Broadcast, "Broadcast"}, {ChatSendMode::Local, "Local"}, {ChatSendMode::Party, "Party"}}));

struct MessageContext {
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, MessageContext& messageContext);
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, MessageContext const& messageContext);
  enum Mode : std::uint8_t {
    Local,
    Party,
    Broadcast,
    Whisper,
    CommandResult,
    RadioMessage,
    World
  };

  MessageContext();
  MessageContext(Mode mode);
  MessageContext(Mode mode, String const& channelName);

  Mode mode;

  // only for Local and Party modes
  String channelName;
};

inline constexpr auto MessageContextModeNames =
  sortedEnumNames(std::to_array<std::pair<MessageContext::Mode, std::string_view>>(
    {{MessageContext::Mode::Local, "Local"},
     {MessageContext::Mode::Party, "Party"},
     {MessageContext::Mode::Broadcast, "Broadcast"},
     {MessageContext::Mode::Whisper, "Whisper"},
     {MessageContext::Mode::CommandResult, "CommandResult"},
     {MessageContext::Mode::RadioMessage, "RadioMessage"},
     {MessageContext::Mode::World, "World"}}));

[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, MessageContext& messageContext);
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, MessageContext const& messageContext);

struct ChatReceivedMessage {
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, ChatReceivedMessage& receivedMessage);
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, ChatReceivedMessage const& receivedMessage);
  ChatReceivedMessage();
  ChatReceivedMessage(MessageContext context, ConnectionId fromConnection, String const& fromNick, String const& text);
  ChatReceivedMessage(MessageContext context, ConnectionId fromConnection, String const& fromNick, String const& text, String const& portrait);
  [[nodiscard]] static std::expected<ChatReceivedMessage, StarException> create(Json const& json);

  [[nodiscard]] std::expected<Json, StarException> toJson() const;

  MessageContext context;

  ConnectionId fromConnection;
  String fromNick;
  String portrait;

  String text;

  JsonObject data;
};

[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, ChatReceivedMessage& receivedMessage);
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, ChatReceivedMessage const& receivedMessage);

};

export module star.chat_types;

export namespace Star {
  using ::Star::ChatSendMode;
  using ::Star::ChatSendModeNames;
  using ::Star::MessageContext;
  using ::Star::MessageContextModeNames;
  using ::Star::ChatReceivedMessage;
}

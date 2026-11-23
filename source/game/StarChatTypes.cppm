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

inline constexpr auto chat_send_mode_names = sorted_enum_names(std::to_array<std::pair<ChatSendMode, std::string_view>>(
  {{ChatSendMode::Broadcast, "Broadcast"}, {ChatSendMode::Local, "Local"}, {ChatSendMode::Party, "Party"}}));

struct MessageContext {
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream&, MessageContext&);
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream&, MessageContext const&);
  enum Mode : std::uint8_t {
    Local,
    Party,
    Broadcast,
    Whisper,
    CommandResult,
    RadioMessage,
    World
  };

  Mode mode = Local;

  // only for Local and Party modes
  String channel_name{};
};

inline constexpr auto message_context_mode_names =
  sorted_enum_names(std::to_array<std::pair<MessageContext::Mode, std::string_view>>(
    {{MessageContext::Mode::Local, "Local"},
     {MessageContext::Mode::Party, "Party"},
     {MessageContext::Mode::Broadcast, "Broadcast"},
     {MessageContext::Mode::Whisper, "Whisper"},
     {MessageContext::Mode::CommandResult, "CommandResult"},
     {MessageContext::Mode::RadioMessage, "RadioMessage"},
     {MessageContext::Mode::World, "World"}}));

[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream&, MessageContext&);
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream&, MessageContext const&);

[[nodiscard]] std::expected<ConnectionId, StarException> chat_connection_id(Json const& json, ConnectionId fallback);

struct ChatReceivedMessage {
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream&, ChatReceivedMessage&);
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream&, ChatReceivedMessage const&);
  [[nodiscard]] static std::expected<ChatReceivedMessage, StarException> create(Json const& json);

  [[nodiscard]] std::expected<Json, StarException> to_json() const;

  MessageContext context;
  ConnectionId from_connection = 0;
  String from_nick;
  String text;
  String portrait{};
  JsonObject data{};
};

[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream&, ChatReceivedMessage&);
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream&, ChatReceivedMessage const&);

}

export module star.chat_types;

export namespace Star {
  using ::Star::ChatSendMode;
  using ::Star::chat_send_mode_names;
  using ::Star::chat_connection_id;
  using ::Star::MessageContext;
  using ::Star::message_context_mode_names;
  using ::Star::ChatReceivedMessage;
}

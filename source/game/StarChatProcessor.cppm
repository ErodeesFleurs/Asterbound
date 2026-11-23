module;
import star.format;
import std;
import star.string;
import star.list;
import star.map;
import star.json;

import star.rpc_promise;
import star.game_types;
import star.message_types;
import star.chat_types;
import star.set;

namespace Star {

// Handles all chat routing and command parsing for client / server chat.
// Thread safe.
class ChatProcessor {
public:
  static constexpr std::string_view server_nick = "server";

  // CommandHandler is passed the origin connection, the command portion
  // excluding the '/' character, and the remaining command line in full.
  using CommandHandler = std::function<std::expected<ServerCommandResult, StarException>(ConnectionId, String, String)>;

  [[nodiscard]] std::expected<String, StarException> connect_client(ConnectionId client_id, String nick = {});
  // Returns any pending messages.
  [[nodiscard]] std::expected<List<ChatReceivedMessage>, StarException> disconnect_client(ConnectionId client_id);

  List<ConnectionId> clients() const;
  bool has_client(ConnectionId client_id) const;

  // Clears all clients and channels
  void reset();

  // Will return nothing if nick is not found.
  std::optional<ConnectionId> find_nick(String const& nick) const;
  [[nodiscard]] std::expected<String, StarException> connection_nick(ConnectionId client_id) const;
  [[nodiscard]] std::expected<String, StarException> renick(ConnectionId client_id, String const& nick);

  // True only if a connected client's membership changed.
  bool join_channel(ConnectionId client_id, String const& channel_name);
  bool leave_channel(ConnectionId client_id, String const& channel_name);

  StringList client_channels(ConnectionId client_id) const;
  StringList active_channels() const;

  [[nodiscard]] std::expected<void, StarException> broadcast(ConnectionId source, String const& text, JsonObject data = {});
  [[nodiscard]] std::expected<void, StarException> message(ConnectionId source, MessageContext::Mode mode, String const& channel_name, String const& text, JsonObject data = {});
  [[nodiscard]] std::expected<void, StarException> whisper(ConnectionId source, ConnectionId target, String const& text, JsonObject data = {});

  [[nodiscard]] std::expected<void, StarException> admin_broadcast(String const& text);
  [[nodiscard]] std::expected<void, StarException> admin_message(MessageContext::Mode mode, String const& channel_name, String const& text);
  [[nodiscard]] std::expected<void, StarException> admin_whisper(ConnectionId target, String const& text);

  [[nodiscard]] std::expected<List<ChatReceivedMessage>, StarException> pull_pending_messages(ConnectionId client_id);

  void set_command_handler(CommandHandler handler);
  void clear_command_handler();
  [[nodiscard]] std::expected<void, StarException> update_promises();

private:
  struct ClientInfo {
    String nick;
    List<ChatReceivedMessage> pending_messages;
  };
  struct PendingCommand {
    std::weak_ptr<ClientInfo> recipient;
    RpcPromise<String> promise;
  };
  enum class Audience { All, Channel, Whisper };

  [[nodiscard]] std::unique_lock<std::recursive_mutex> lock() const;
  String unique_nick(String nick, ConnectionId owner) const;
  [[nodiscard]] std::expected<String, StarException> renick_locked(ConnectionId client_id, String const& nick);
  [[nodiscard]] std::expected<void, StarException> send_locked(Audience audience, ConnectionId source,
      ConnectionId target, MessageContext context, String const& text, JsonObject data);
  [[nodiscard]] std::expected<void, StarException> handle_command(ConnectionId source, std::weak_ptr<ClientInfo> recipient,
      String command_line);

  // The externally supplied handler can synchronously route another message.
  mutable std::recursive_mutex m_mutex;
  HashMap<ConnectionId, std::shared_ptr<ClientInfo>> m_clients;
  StringMap<ConnectionId> m_nicks;
  StringMap<Set<ConnectionId>> m_channels;
  std::shared_ptr<CommandHandler const> m_command_handler;
  std::list<PendingCommand> m_command_promises;
  bool m_polling = false;
};

}

export module star.chat_processor;

export namespace Star {
  using ::Star::ChatProcessor;
}

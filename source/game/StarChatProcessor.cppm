module;
import star.format;
import std;
import star.string;
import star.list;
import star.map;
import star.json;

import star.data_stream;
import star.rpc_promise;
import star.game_types;
import star.chat_types;
import star.set;
import star.thread;
import star.uuid;

namespace Star {

class RandomService;

// Handles all chat routing and command parsing for client / server chat.
// Thread safe.
class ChatProcessor {
public:
  static std::string_view const ServerNick;

  explicit ChatProcessor(RandomService& random);

  // CommandHandler is passed the origin connection, the command portion
  // excluding the '/' character, and the remaining command line in full.
  using CommandHandler = std::function<std::expected<ServerCommandResult, StarException>(ConnectionId, String, String)>;

  [[nodiscard]] std::expected<String, StarException> connectClient(ConnectionId clientId, String nick = "");
  // Returns any pending messages.
  [[nodiscard]] std::expected<List<ChatReceivedMessage>, StarException> disconnectClient(ConnectionId clientId);

  List<ConnectionId> clients() const;
  bool hasClient(ConnectionId clientId) const;

  // Clears all clients and channels
  void reset();

  // Will return nothing if nick is not found.
  std::optional<ConnectionId> findNick(String const& nick) const;
  [[nodiscard]] std::expected<String, StarException> connectionNick(ConnectionId connectionId) const;
  [[nodiscard]] std::expected<String, StarException> renick(ConnectionId clientId, String const& nick);

  // join / leave return true in the even that the client channel state was
  // actually changed.
  bool joinChannel(ConnectionId clientId, String const& channelName);
  bool leaveChannel(ConnectionId clientId, String const& channelName);

  StringList clientChannels(ConnectionId clientId) const;
  StringList activeChannels() const;

  [[nodiscard]] std::expected<void, StarException> broadcast(ConnectionId sourceConnectionId, String const& text, JsonObject data = {});
  [[nodiscard]] std::expected<void, StarException> message(ConnectionId sourceConnectionId, MessageContext::Mode context, String const& channelName, String const& text, JsonObject data = {});
  [[nodiscard]] std::expected<void, StarException> whisper(ConnectionId sourceConnectionId, ConnectionId targetClientId, String const& text, JsonObject data = {});

  // Shorthand for passing ServerConnectionId as sourceConnectionId to
  // broadcast / message / whisper
  [[nodiscard]] std::expected<void, StarException> adminBroadcast(String const& text);
  [[nodiscard]] std::expected<void, StarException> adminMessage(MessageContext::Mode context, String const& channelName, String const& text);
  [[nodiscard]] std::expected<void, StarException> adminWhisper(ConnectionId targetClientId, String const& text);

  [[nodiscard]] std::expected<List<ChatReceivedMessage>, StarException> pullPendingMessages(ConnectionId clientId);

  void setCommandHandler(CommandHandler commandHandler);
  void clearCommandHandler();
  
  [[nodiscard]] std::expected<void, StarException> updatePromises();

private:
  struct ClientInfo {
    ClientInfo(ConnectionId clientId, String const& nick);

    ConnectionId clientId;
    String nick;
    List<ChatReceivedMessage> pendingMessages;
  };

  String makeNickUnique(String nick);

  // Returns true if message was handled completely and needs no further
  // processing.
  [[nodiscard]] std::expected<bool, StarException> handleCommand(ChatReceivedMessage& message);

  mutable std::recursive_mutex m_mutex;
  RandomService& m_random;

  HashMap<ConnectionId, ClientInfo> m_clients;
  StringMap<ConnectionId> m_nicks;
  StringMap<Set<ConnectionId>> m_channels;

  CommandHandler m_commandHandler;
  
  HashMap<Uuid,std::pair<ConnectionId,RpcPromise<String>>> m_commandPromises;
};

}

export module star.chat_processor;

export namespace Star {
  using ::Star::ChatProcessor;
}

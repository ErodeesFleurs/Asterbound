import std;
import star.string;
import star.list;
import star.format;
import star.json;
import star.map;
import star.message_types;
import star.rpc_promise;
import star.game_types;
import star.chat_types;
import star.set;
import star.thread;

import star.chat_processor;

import star.algorithm;
namespace Star {

std::unique_lock<std::recursive_mutex> ChatProcessor::lock() const {
  std::unique_lock guard(m_mutex, std::defer_lock);
  lock_with_diagnostics(guard);
  return guard;
}

String ChatProcessor::unique_nick(String nick, ConnectionId owner) const {
  for (;;) {
    auto taken = m_nicks.find(nick);
    if (nick != server_nick && (taken == m_nicks.end() || taken->second == owner))
      return nick;
    nick.append("_");
  }
}

std::expected<String, StarException> ChatProcessor::connect_client(ConnectionId client_id, String nick) {
  auto guard = lock();
  if (client_id == server_connection_id || m_clients.contains(client_id))
    return std::unexpected(StarException(Star::format("Chat connection {} is already in use", client_id)));
  if (nick.empty())
    nick = Star::format("Player_{}", client_id);
  nick = unique_nick(std::move(nick), client_id);
  auto session = std::make_shared<ClientInfo>(nick, List<ChatReceivedMessage>{});
  m_clients.try_emplace(client_id, std::move(session));
  m_nicks.try_emplace(nick, client_id);
  if (m_clients.size() > 1) {
    ChatReceivedMessage notice{{MessageContext::Broadcast}, server_connection_id, server_nick,
        Star::format("Player '{}' connected", nick)};
    for (auto const& [peer, client] : m_clients)
      if (peer != client_id)
        client->pending_messages.push_back(notice);
  }
  return nick;
}

std::expected<List<ChatReceivedMessage>, StarException> ChatProcessor::disconnect_client(ConnectionId client_id) {
  std::list<PendingCommand> retired;
  auto guard = lock();
  auto session = m_clients.extract_value(client_id);
  if (!session) return std::unexpected(std::move(session.error()));
  auto& client = **session;
  m_nicks.remove(client.nick);
  for (auto channel = m_channels.begin(); channel != m_channels.end();) {
    channel->second.remove(client_id);
    if (channel->second.empty())
      channel = m_channels.erase(channel);
    else
      ++channel;
  }
  if (!m_clients.empty()) {
    ChatReceivedMessage notice{{MessageContext::Broadcast}, server_connection_id, server_nick,
        Star::format("Player '{}' disconnected", client.nick)};
    for (auto const& peer : m_clients)
      peer.second->pending_messages.push_back(notice);
  }
  auto messages = std::move(client.pending_messages);
  session->reset();
  // Detach all abandoned observations before releasing any callback captures:
  // capture destruction can itself enter the processor.
  for (auto it = m_command_promises.begin(); it != m_command_promises.end();) {
    auto entry = it++;
    if (entry->recipient.expired())
      retired.splice(retired.end(), m_command_promises, entry);
  }
  return messages;
}

List<ConnectionId> ChatProcessor::clients() const {
  auto guard = lock();
  return m_clients.keys();
}

bool ChatProcessor::has_client(ConnectionId client_id) const {
  auto guard = lock();
  return m_clients.contains(client_id);
}

void ChatProcessor::reset() {
  std::list<PendingCommand> retired;
  auto guard = lock();
  m_clients.clear();
  m_nicks.clear();
  m_channels.clear();
  // Release callback captures only after the processor is in its reset state.
  retired.swap(m_command_promises);
}

std::optional<ConnectionId> ChatProcessor::find_nick(String const& nick) const {
  if (nick == server_nick) return server_connection_id;
  auto guard = lock();
  return m_nicks.maybe(nick);
}

std::expected<String, StarException> ChatProcessor::connection_nick(ConnectionId client_id) const {
  if (client_id == server_connection_id) return server_nick;
  auto guard = lock();
  auto client = m_clients.get(client_id);
  if (!client) return std::unexpected(std::move(client.error()));
  return client->get()->nick;
}

std::expected<String, StarException> ChatProcessor::renick(ConnectionId client_id, String const& nick) {
  auto guard = lock();
  return renick_locked(client_id, nick);
}

std::expected<String, StarException> ChatProcessor::renick_locked(ConnectionId client_id, String const& nick) {
  auto client = m_clients.get(client_id);
  if (!client) return std::unexpected(std::move(client.error()));
  auto& current = client->get()->nick;
  if (current == nick) return current;
  auto available = unique_nick(nick, client_id);
  if (available != current) {
    m_nicks.try_emplace(available, client_id);
    m_nicks.remove(current);
    current = std::move(available);
  }
  return current;
}

bool ChatProcessor::join_channel(ConnectionId client_id, String const& channel_name) {
  auto guard = lock();
  return m_clients.contains(client_id) && m_channels[channel_name].add(client_id);
}

bool ChatProcessor::leave_channel(ConnectionId client_id, String const& channel_name) {
  auto guard = lock();
  auto channel = m_channels.find(channel_name);
  if (channel == m_channels.end() || !channel->second.remove(client_id))
    return false;
  if (channel->second.empty())
    m_channels.erase(channel);
  return true;
}

StringList ChatProcessor::client_channels(ConnectionId client_id) const {
  auto guard = lock();
  StringList channels;
  for (auto const& [name, members] : m_channels)
    if (members.contains(client_id))
      channels.push_back(name);
  return channels;
}

StringList ChatProcessor::active_channels() const {
  auto guard = lock();
  return m_channels.keys();
}

std::expected<void, StarException> ChatProcessor::broadcast(ConnectionId source, String const& text, JsonObject data) {
  auto guard = lock();
  return send_locked(Audience::All, source, {}, {MessageContext::Broadcast}, text, std::move(data));
}

std::expected<void, StarException> ChatProcessor::message(ConnectionId source, MessageContext::Mode mode,
    String const& channel_name, String const& text, JsonObject data) {
  auto guard = lock();
  return send_locked(Audience::Channel, source, {}, {mode, channel_name}, text, std::move(data));
}

std::expected<void, StarException> ChatProcessor::whisper(ConnectionId source, ConnectionId target,
    String const& text, JsonObject data) {
  auto guard = lock();
  return send_locked(Audience::Whisper, source, target, {MessageContext::Whisper}, text, std::move(data));
}

std::expected<void, StarException> ChatProcessor::send_locked(Audience audience, ConnectionId source,
    ConnectionId target, MessageContext context, String const& text, JsonObject data) {
  auto sender = source == server_connection_id ? m_clients.end() : m_clients.find(source);
  if (source != server_connection_id && sender == m_clients.end())
    return std::unexpected(StarException(Star::format("Unknown chat sender {}", source)));
  auto receiver = audience == Audience::Whisper ? m_clients.find(target) : m_clients.end();
  if (audience == Audience::Whisper && receiver == m_clients.end())
    return std::unexpected(StarException(Star::format("Unknown whisper recipient {}", target)));

  // Preserve the original whole-message UTF-8 check, but classify the ASCII
  // slash prefixes and slice the already validated bytes without rescanning.
  auto length = text.size();
  if (!length) return std::unexpected(std::move(length.error()));
  std::string_view bytes(text.utf8());
  if (bytes.starts_with('/')) {
    bytes.remove_prefix(1);
    if (!bytes.starts_with('/')) {
      std::weak_ptr<ClientInfo> recipient;
      if (sender != m_clients.end()) recipient = sender->second;
      return handle_command(source, std::move(recipient), String(bytes));
    }
  }
  ChatReceivedMessage received{std::move(context), source, sender == m_clients.end() ? String(server_nick) : sender->second->nick,
      String(bytes), {}, std::move(data)};
  if (audience == Audience::Whisper) {
    // A whisper to oneself still has both the outgoing echo and the receipt.
    if (sender != m_clients.end())
      sender->second->pending_messages.push_back(received);
    receiver->second->pending_messages.push_back(std::move(received));
  } else if (audience == Audience::All) {
    for (auto it = m_clients.begin(); it != m_clients.end();) {
      auto& inbox = it->second->pending_messages;
      if (++it == m_clients.end()) inbox.push_back(std::move(received));
      else inbox.push_back(received);
    }
  } else if (auto channel = m_channels.find_ref(received.context.channel_name)) {
    auto const& members = channel->get();
    for (auto it = members.begin(); it != members.end();) {
      auto& inbox = m_clients.find(*it)->second->pending_messages;
      if (++it == members.end()) inbox.push_back(std::move(received));
      else inbox.push_back(received);
    }
  }
  return {};
}

std::expected<void, StarException> ChatProcessor::admin_broadcast(String const& text) {
  return broadcast(server_connection_id, text, {});
}

std::expected<void, StarException> ChatProcessor::admin_message(MessageContext::Mode mode, String const& channel_name, String const& text) {
  return message(server_connection_id, mode, channel_name, text, {});
}

std::expected<void, StarException> ChatProcessor::admin_whisper(ConnectionId target, String const& text) {
  return whisper(server_connection_id, target, text, {});
}

std::expected<List<ChatReceivedMessage>, StarException> ChatProcessor::pull_pending_messages(ConnectionId client_id) {
  auto guard = lock();
  if (auto client = m_clients.find_ref(client_id))
    return std::exchange(client->get()->pending_messages, {});
  return List<ChatReceivedMessage>{};
}

void ChatProcessor::set_command_handler(CommandHandler handler) {
  auto next = handler ? std::make_shared<CommandHandler const>(std::move(handler)) : nullptr;
  std::shared_ptr<CommandHandler const> retired;
  auto guard = lock();
  retired = std::exchange(m_command_handler, std::move(next));
}

void ChatProcessor::clear_command_handler() {
  set_command_handler({});
}

std::expected<void, StarException> ChatProcessor::handle_command(ConnectionId source, std::weak_ptr<ClientInfo> recipient,
    String command_line) {
  auto command = command_line.extract();
  if (!command) return std::unexpected(std::move(command.error()));
  String response;
  if (*command == "nick") {
    auto trimmed = command_line.trim();
    if (!trimmed) return std::unexpected(std::move(trimmed.error()));
    auto nick = renick_locked(source, *trimmed);
    if (!nick) return std::unexpected(std::move(nick.error()));
    response = Star::format("Nick changed to {}", *nick);
  } else if (*command == "w") {
    auto target = command_line.extract();
    if (!target) return std::unexpected(std::move(target.error()));
    if (auto id = m_nicks.maybe(*target)) {
      auto trimmed = command_line.trim();
      if (!trimmed) return std::unexpected(std::move(trimmed.error()));
      return send_locked(Audience::Whisper, source, *id, {MessageContext::Whisper}, *trimmed, {});
    }
    response = Star::format("No such nick {}", *target);
  } else if (auto handler = m_command_handler) {
    // Pin the installed callable, not a copy of its mutable captures. It can
    // replace itself, disconnect its caller, and synchronously route messages.
    auto result = (*handler)(source, std::move(*command), std::move(command_line));
    if (!result) return std::unexpected(std::move(result.error()));
    if (std::holds_alternative<RpcPromise<String>>(*result)) {
      if (!recipient.expired())
        m_command_promises.push_back({std::move(recipient), std::get<RpcPromise<String>>(std::move(*result))});
      return {};
    }
    response = std::get<String>(std::move(*result));
  } else {
    response = Star::format("No such command {}", *command);
  }
  if (!response.empty()) {
    if (auto client = recipient.lock())
      client->pending_messages.push_back({{MessageContext::CommandResult}, server_connection_id, server_nick, std::move(response)});
    else if (source == server_connection_id)
      return std::unexpected(StarException("Server has no chat inbox for command results"));
  }
  return {};
}

std::expected<void, StarException> ChatProcessor::update_promises() {
  auto guard = lock();
  if (m_command_promises.empty() || m_polling) return {};

  // Polling may invoke a lazy transform that mutates clients, queues another
  // command, or calls update_promises again. Isolate this batch in O(1);
  // unfinished entries stay before newly enqueued ones for the next tick.
  std::list<PendingCommand> batch;
  batch.swap(m_command_promises);
  m_polling = true;
  auto restore = make_scope_exit([&] {
    m_command_promises.splice(m_command_promises.begin(), batch);
    m_polling = false;
  });
  for (auto it = batch.begin(); it != batch.end();) {
    auto entry = it++;
    if (!entry->recipient.expired()) {
      auto status = entry->promise.status();
      if (status && *status == RpcPromiseStatus::Pending) continue;
      // A hook can abandon its own session even when it returns a diagnostic.
      // A terminal outcome is immutable; extracting it won't run another hook.
      if (auto client = entry->recipient.lock()) {
        if (!status) return std::unexpected(std::move(status.error()));
        bool success = *status == RpcPromiseStatus::Succeeded;
        auto result = success ? entry->promise.result() : entry->promise.error();
        if (!result) return std::unexpected(std::move(result.error()));
        if (!*result) return std::unexpected(RpcPromiseException("Completed command has no outcome"));
        auto response = success ? std::move(**result) : String(Star::format("Error: {}", **result));
        if (!response.empty())
          client->pending_messages.push_back({{MessageContext::CommandResult}, server_connection_id, server_nick, std::move(response)});
      }
    }
    batch.erase(entry);
  }
  return {};
}

}

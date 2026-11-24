module star.server_rcon;

import std;
import star.bytes;
import star.configuration;
import star.data_stream;
import star.data_stream_devices;
import star.format;
import star.game_types;
import star.message_types;
import star.host_address;
import star.logging;
import star.output_proxy;
import star.rpc_promise;
import star.root;
import star.socket;
import star.string;
import star.tcp;
import star.thread;
import star.universe_server;

namespace Star {

std::expected<std::shared_ptr<ServerRconClient>, StarException>
ServerRconClient::create(UniverseServer& universe, std::shared_ptr<TcpSocket> socket, unsigned timeout_ms) {
  auto configuration = universe.root().configuration();
  if (!configuration)
    return std::unexpected(std::move(configuration.error()));
  auto password = (*configuration)->get("rconServerPassword");
  if (!password)
    return std::unexpected(std::move(password.error()));
  auto text = password->to_string();
  if (!text)
    return std::unexpected(std::move(text.error()));
  if (auto configured = socket->set_non_blocking(true); !configured)
    return std::unexpected(std::move(configured.error()));
  return std::make_shared<ServerRconClient>(ConstructionKey{}, universe, std::move(socket), std::move(*text),
      std::chrono::milliseconds(timeout_ms));
}

ServerRconClient::ServerRconClient(ConstructionKey, UniverseServer& universe, std::shared_ptr<TcpSocket> socket,
    String password, std::chrono::milliseconds timeout)
    : Thread("RconClient"), m_universe(universe), m_socket(std::move(socket)), m_timeout(timeout),
      m_authed(false), m_rcon_password(std::move(password)) {
  m_request.set_byte_order(ByteOrder::LittleEndian);
  m_request.set_null_terminated_strings(true);
  m_response.set_byte_order(ByteOrder::LittleEndian);
}

ServerRconClient::~ServerRconClient() {
  stop();
  if (auto joined = join(); !joined)
    EmergencyLog::error("RCON client shutdown failed: {}", joined.error().what());
  if (!is_joined())
    Star::fatal_error("Cannot destroy an RCON client with an unjoined native worker", false);
}

std::expected<ServerCommandResult, StarException> ServerRconClient::handle_command(String command_line) {
  auto extracted = command_line.extract();
  if (!extracted)
    return std::unexpected(std::move(extracted.error()));
  auto const& command = *extracted;
  if (command == "echo") {
    return ServerCommandResult(std::move(command_line));
  } else if (command == "broadcast" || command == "say") {
    auto broadcast = m_universe.get().admin_broadcast(command_line);
    if (!broadcast)
      return std::unexpected(std::move(broadcast.error()));
    return ServerCommandResult(String(Star::format("OK: said {}", command_line)));
  } else if (command == "stop") {
    auto stopped = m_universe.get().stop();
    if (!stopped)
      return std::unexpected(std::move(stopped.error()));
    return ServerCommandResult(String("OK: shutting down"));
  }
  return m_universe.get().admin_command(command, command_line);
}

std::chrono::steady_clock::time_point ServerRconClient::io_deadline() const {
  return m_timeout.count() ? std::chrono::steady_clock::now() + m_timeout
                           : std::chrono::steady_clock::time_point::max();
}

std::expected<void, StarException> ServerRconClient::receive(std::size_t size) {
  // Refuse client-controlled allocation sizes before resizing.
  if (size > max_receive_packet_size)
    return std::unexpected(OversizedPacket(
        Star::format("RCON packet of {} bytes is above the {} byte limit", size, max_receive_packet_size)));
  m_request.reset(size);
  auto remaining = m_request.span();
  std::optional<std::chrono::steady_clock::time_point> deadline;
  while (!remaining.empty()) {
    // Replies have their own buffer: flushing cannot overwrite a partial length or body.
    if (!m_command_promises.empty()) {
      if (auto flushed = flush_replies(); !flushed)
        return flushed;
    }
    auto received = m_socket->receive(remaining);
    if (!received)
      return std::unexpected(std::move(received.error()));
    if (*received) {
      remaining = remaining.subspan(*received);
      deadline.reset();
      continue;
    }
    if (!m_socket->is_active())
      return std::unexpected(NoMoreRequests());
    if (!deadline)
      deadline = io_deadline();
    auto now = std::chrono::steady_clock::now();
    if (now >= *deadline)
      return std::unexpected(NoMoreRequests());
    auto wake = m_command_promises.empty() ? *deadline : std::min(*deadline, now + std::chrono::milliseconds(20));
    if (auto waited = m_socket->wait_for_data(false, wake); !waited)
      return waited;
  }
  return {};
}

std::expected<void, StarException> ServerRconClient::send(std::uint32_t request_id, std::uint32_t cmd,
    std::string_view str) {
  m_response.clear();
  auto header = m_response << static_cast<std::uint32_t>(str.size() + 10) << request_id << cmd;
  if (!header)
    return std::unexpected(std::move(header.error()));
  if (!str.empty()) {
    auto data = m_response.write_data(std::span(str));
    if (!data)
      return std::unexpected(std::move(data.error()));
  }
  auto terminator = m_response << std::uint8_t{0} << std::uint8_t{0};
  if (!terminator)
    return std::unexpected(std::move(terminator.error()));
  auto remaining = m_response.span();
  std::optional<std::chrono::steady_clock::time_point> deadline;
  while (!remaining.empty()) {
    auto sent = m_socket->send(remaining);
    if (!sent)
      return std::unexpected(std::move(sent.error()));
    if (*sent) {
      remaining = remaining.subspan(*sent);
      deadline.reset();
      continue;
    }
    if (!deadline)
      deadline = io_deadline();
    if (!m_socket->is_active() || std::chrono::steady_clock::now() >= *deadline)
      return std::unexpected(NetworkException("RCON response write made no progress"));
    if (auto waited = m_socket->wait_for_data(true, *deadline); !waited)
      return waited;
  }
  return {};
}

std::expected<void, StarException> ServerRconClient::send_auth_failure() {
  return send(serverdata_auth_failure, serverdata_auth_response);
}

std::expected<void, StarException> ServerRconClient::send_command_response(std::uint32_t request_id, String const& response) {
  auto begin = response.begin();
  if (!begin)
    return std::unexpected(std::move(begin.error()));
  auto cursor = *begin;
  // Preserve code-point chunk boundaries and the packet for an empty response.
  do {
    auto first = cursor;
    std::size_t count = 0;
    while (cursor != response.end() && count < max_packet_size) {
      ++cursor;
      ++count;
    }
    auto sent = send(request_id, serverdata_response_value, std::string_view(first.base(), cursor.base()));
    if (!sent)
      return std::unexpected(std::move(sent.error()));
  } while (cursor != response.end());
  return {};
}

std::expected<void, StarException> ServerRconClient::start() {
  // Do not replace a source until the previous native run has been joined.
  if (!is_joined())
    return {};
  m_stop_source = std::stop_source{};
  auto started = Thread::start();
  if (!started) {
    m_stop_source.request_stop();
    return std::unexpected(std::move(started.error()));
  }
  return {};
}

void ServerRconClient::stop() {
  m_stop_source.request_stop();
  m_socket->close();
}

std::expected<void, StarException> ServerRconClient::process_request() {
  auto prefix = receive(4);
  if (!prefix)
    return std::unexpected(std::move(prefix.error()));
  auto size = m_request.read<std::uint32_t>();
  if (!size)
    return std::unexpected(std::move(size.error()));
  auto payload = receive(*size);
  if (!payload)
    return std::unexpected(std::move(payload.error()));
  std::uint32_t request_id, cmd;
  auto header = m_request >> request_id >> cmd;
  if (!header)
    return std::unexpected(std::move(header.error()));

  switch (cmd) {
  case serverdata_auth: {
    String password;
    auto decoded = m_request >> password;
    if (!decoded)
      return std::unexpected(std::move(decoded.error()));
    m_authed = false;
    if (!m_rcon_password.empty()) {
      auto matches = m_rcon_password.equals(password);
      if (!matches)
        return std::unexpected(std::move(matches.error()));
      m_authed = *matches;
    }
    if (m_authed) {
      auto response = send(request_id, serverdata_response_value);
      if (!response)
        return std::unexpected(std::move(response.error()));
      response = send(request_id, serverdata_auth_response);
      if (!response)
        return std::unexpected(std::move(response.error()));
    } else {
      auto response = send_auth_failure();
      if (!response)
        return std::unexpected(std::move(response.error()));
    }
    break;
  }
  case serverdata_exec_command:
    if (m_authed) {
      String command;
      auto decoded = m_request >> command;
      if (!decoded)
        return std::unexpected(std::move(decoded.error()));
      auto logged = m_universe.get().root().logger().info("RCON {}: {}", m_socket->remote_address(), command);
      if (!logged)
        return std::unexpected(std::move(logged.error()));
      // Keep the original wire text for diagnostics without copying it before extraction.
      auto original = std::string_view(m_request.span().subspan(2 * sizeof(std::uint32_t), command.utf8_size()));
      auto result = handle_command(std::move(command));
      if (!result) {
        auto response = send_command_response(request_id, Star::format("RCON: Error executing: {}: {}", original,
            Star::output_exception(ExceptionDiagnostic(result.error()), true)));
        if (!response)
          return std::unexpected(std::move(response.error()));
      } else if (std::holds_alternative<String>(*result)) {
        auto response = send_command_response(request_id, std::get<String>(*result));
        if (!response)
          return std::unexpected(std::move(response.error()));
      } else {
        m_command_promises[request_id] = std::get<RpcPromise<String>>(std::move(*result));
      }
    } else {
      auto response = send_auth_failure();
      if (!response)
        return std::unexpected(std::move(response.error()));
    }
    break;
  default: {
    auto response = send_command_response(request_id, Star::format("Unknown request {:06x}", cmd));
    if (!response)
      return std::unexpected(std::move(response.error()));
  }
  }
  return {};
}

std::expected<void, StarException> ServerRconClient::flush_replies() {
  for (auto it = m_command_promises.begin(); it != m_command_promises.end();) {
    auto const& [request_id, promise] = *it;
    auto status = promise.status();
    if (!status)
      return std::unexpected(std::move(status.error()));
    if (*status == RpcPromiseStatus::Pending) {
      ++it;
      continue;
    }
    std::expected<void, StarException> response;
    if (*status == RpcPromiseStatus::Succeeded) {
      auto result = promise.result();
      if (!result)
        return std::unexpected(std::move(result.error()));
      if (!*result)
        return std::unexpected(StarException("RCON command promise completed without a result"));
      response = send_command_response(request_id, **result);
    } else {
      auto error = promise.error();
      if (!error)
        return std::unexpected(std::move(error.error()));
      if (!*error)
        return std::unexpected(StarException("RCON command promise failed without an error"));
      response = send_command_response(request_id, Star::format("RCON: Command promise failed: {}", **error));
    }
    if (!response)
      return std::unexpected(std::move(response.error()));
    it = m_command_promises.erase(it);
  }
  return {};
}

std::expected<void, StarException> ServerRconClient::run() {
  auto stop_token = m_stop_source.get_token();
  while (!stop_token.stop_requested()) {
    auto request = process_request();
    if (!request) {
      m_socket->close();
      if (!stop_token.stop_requested() && !request.error().is<NoMoreRequests>()) {
        auto logged = m_universe.get().root().logger().error(
          "ServerRconClient error: {}", Star::output_exception(ExceptionDiagnostic(request.error()), false));
        if (!logged)
          return std::unexpected(std::move(logged.error()));
      }
      break;
    }
  }
  return {};
}

std::expected<std::unique_ptr<ServerRconThread>, StarException>
ServerRconThread::create(UniverseServer& universe, HostAddressWithPort const& address) {
  auto server = TcpServer::create(universe.root().logger(), address);
  if (!server)
    return std::unexpected(std::move(server.error()));
  auto configuration = universe.root().configuration();
  if (!configuration)
    return std::unexpected(std::move(configuration.error()));
  auto password = (*configuration)->get("rconServerPassword");
  if (!password)
    return std::unexpected(std::move(password.error()));
  auto text = password->to_string();
  if (!text)
    return std::unexpected(std::move(text.error()));
  if (text->empty()) {
    auto logged = universe.root().logger().warn("rconServerPassword is not configured requests will NOT be processed");
    if (!logged)
      return std::unexpected(std::move(logged.error()));
  }
  return std::make_unique<ServerRconThread>(ConstructionKey{}, universe, std::move(*server));
}

ServerRconThread::ServerRconThread(ConstructionKey, UniverseServer& universe, std::unique_ptr<TcpServer> server)
    : Thread("RconServer"), m_universe(universe), m_rcon_server(std::move(server)) {}

ServerRconThread::~ServerRconThread() {
  if (auto stopped = stop(); !stopped)
    EmergencyLog::error("RCON server stop failed: {}", stopped.error().what());
  if (auto joined = join(); !joined)
    EmergencyLog::error("RCON server shutdown failed: {}", joined.error().what());
  if (!is_joined())
    Star::fatal_error("Cannot destroy an RCON server with an unjoined native worker", false);
}

void ServerRconThread::clear_clients(bool all) {
  std::lock_guard lock(m_clients_mutex);
  for (auto it = m_clients.begin(); it != m_clients.end();) {
    auto const& pair = *it;
    auto const& client = pair.second;
    if (all)
      client->stop();
    else if (!client->is_running()) {
      it = m_clients.erase(it);
      continue;
    }
    ++it;
  }
}

std::expected<void, StarException> ServerRconThread::start() {
  // Do not replace a source until the previous native run has been joined.
  if (!is_joined())
    return {};
  m_stop_source = std::stop_source{};
  auto started = Thread::start();
  if (!started) {
    m_stop_source.request_stop();
    return std::unexpected(std::move(started.error()));
  }
  return {};
}

std::expected<void, StarException> ServerRconThread::stop() {
  {
    // Serialize the stop transition with accepted-client publication.
    std::lock_guard lock(m_clients_mutex);
    m_stop_source.request_stop();
  }
  auto stopped = m_rcon_server->stop();
  clear_clients(true);
  return stopped;
}

std::expected<void, StarException> ServerRconThread::run() {
  auto stop_token = m_stop_source.get_token();
  auto loop = [this, stop_token]() -> std::expected<void, StarException> {
    auto configuration = m_universe.get().root().configuration();
    if (!configuration)
      return std::unexpected(std::move(configuration.error()));
    auto setting = (*configuration)->get("rconServerTimeout");
    if (!setting)
      return std::unexpected(std::move(setting.error()));
    auto timeout = setting->to_int();
    if (!timeout)
      return std::unexpected(std::move(timeout.error()));
    while (!stop_token.stop_requested()) {
      auto client = m_rcon_server->accept(100);
      if (!client)
        return std::unexpected(std::move(client.error()));
      if (!*client)
        continue;
      if (stop_token.stop_requested())
        break;
      auto rcon_client = ServerRconClient::create(m_universe.get(), *client, static_cast<unsigned>(*timeout));
      if (!rcon_client)
        return std::unexpected(std::move(rcon_client.error()));
      auto started = (*rcon_client)->start();
      if (!started)
        return std::unexpected(std::move(started.error()));
      {
        std::lock_guard lock(m_clients_mutex);
        if (stop_token.stop_requested())
          break;
        m_clients[(*client)->remote_address().address()] = std::move(*rcon_client);
      }
      clear_clients();
    }
    return {};
  };
  if (auto result = loop(); !result) {
    auto logged = m_universe.get().root().logger().error("ServerRconThread error: {}", result.error().what());
    if (!logged)
      return std::unexpected(std::move(logged.error()));
  }
  return {};
}

}// namespace Star

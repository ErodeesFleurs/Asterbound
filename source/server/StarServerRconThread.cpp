module star.server_rcon;

import std;
import star.bytes;
import star.configuration;
import star.data_stream;
import star.data_stream_devices;
import star.format;
import star.game_types;
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
ServerRconClient::create(UniverseServer& universe, std::shared_ptr<TcpSocket> socket) {
  auto configuration = universe.root().configuration();
  if (!configuration)
    return std::unexpected(std::move(configuration.error()));
  auto password = (*configuration)->get("rconServerPassword");
  if (!password)
    return std::unexpected(std::move(password.error()));
  auto text = password->toString();
  if (!text)
    return std::unexpected(std::move(text.error()));
  return std::shared_ptr<ServerRconClient>(new ServerRconClient(universe, std::move(socket), std::move(*text)));
}

ServerRconClient::ServerRconClient(UniverseServer& universe, std::shared_ptr<TcpSocket> socket, String password)
    : Thread("RconClient"), m_universe(universe), m_socket(std::move(socket)), m_packetBuffer(MaxPacketSize),
      m_stop(true), m_authed(false), m_rconPassword(std::move(password)) {
  m_packetBuffer.setByteOrder(ByteOrder::LittleEndian);
  m_packetBuffer.setNullTerminatedStrings(true);
}

ServerRconClient::~ServerRconClient() {
  stop();
  if (auto joined = join(); !joined)
    EmergencyLog::error("RCON client shutdown failed: {}", joined.error().what());
}

std::expected<ServerCommandResult, StarException> ServerRconClient::handleCommand(String commandLine) {
  auto extracted = commandLine.extract();
  if (!extracted)
    return std::unexpected(std::move(extracted.error()));
  String command = std::move(*extracted);
  if (command == "echo") {
    return ServerCommandResult(std::move(commandLine));
  } else if (command == "broadcast" || command == "say") {
    auto broadcast = m_universe.get().adminBroadcast(commandLine);
    if (!broadcast)
      return std::unexpected(std::move(broadcast.error()));
    return ServerCommandResult(String(strf("OK: said {}", commandLine)));
  } else if (command == "stop") {
    auto stopped = m_universe.get().stop();
    if (!stopped)
      return std::unexpected(std::move(stopped.error()));
    return ServerCommandResult(String("OK: shutting down"));
  }
  return m_universe.get().adminCommand(strf("{} {}", command, commandLine));
}

std::expected<void, StarException> ServerRconClient::receive(std::size_t size) {
  // Refuse client-controlled allocation sizes before resizing.
  if (size > MaxReceivePacketSize)
    return std::unexpected(
      OversizedPacket(strf("RCON packet of {} bytes is above the {} byte limit", size, MaxReceivePacketSize)));
  m_packetBuffer.reset(size);
  auto remaining = m_packetBuffer.span();
  while (!remaining.empty()) {
    auto received = m_socket->receive(remaining);
    if (!received)
      return std::unexpected(std::move(received.error()));
    if (*received == 0)
      return std::unexpected(NoMoreRequests());
    remaining = remaining.subspan(*received);
  }
  return {};
}

std::expected<void, StarException> ServerRconClient::send(std::uint32_t requestId, std::uint32_t cmd,
                                                          std::string_view str) {
  m_packetBuffer.clear();
  auto header = m_packetBuffer << static_cast<std::uint32_t>(str.size() + 10) << requestId << cmd;
  if (!header)
    return std::unexpected(std::move(header.error()));
  if (!str.empty()) {
    auto data = m_packetBuffer.writeData(std::span(str));
    if (!data)
      return std::unexpected(std::move(data.error()));
  }
  auto terminator = m_packetBuffer << std::uint8_t{0} << std::uint8_t{0};
  if (!terminator)
    return std::unexpected(std::move(terminator.error()));
  auto sent = m_socket->send(m_packetBuffer.span());
  if (!sent)
    return std::unexpected(std::move(sent.error()));
  return {};
}

std::expected<void, StarException> ServerRconClient::sendAuthFailure() {
  return send(SERVERDATA_AUTH_FAILURE, SERVERDATA_AUTH_RESPONSE, "");
}

std::expected<void, StarException> ServerRconClient::sendCmdResponse(std::uint32_t requestId, String const& response) {
  auto begin = response.begin();
  if (!begin)
    return std::unexpected(std::move(begin.error()));
  auto cursor = *begin;
  // Preserve code-point chunk boundaries and the packet for an empty response.
  do {
    auto first = cursor;
    std::size_t count = 0;
    while (cursor != response.end() && count < MaxPacketSize) {
      ++cursor;
      ++count;
    }
    auto sent = send(requestId, SERVERDATA_RESPONSE_VALUE, std::string_view(first.base(), cursor.base()));
    if (!sent)
      return std::unexpected(std::move(sent.error()));
  } while (cursor != response.end());
  return {};
}

std::expected<void, StarException> ServerRconClient::start() {
  m_stop = false;
  auto started = Thread::start();
  if (!started) {
    m_stop = true;
    return std::unexpected(std::move(started.error()));
  }
  return {};
}

void ServerRconClient::stop() {
  m_stop = true;
  m_socket->close();
}

std::expected<void, StarException> ServerRconClient::processRequest() {
  auto prefix = receive(4);
  if (!prefix)
    return std::unexpected(std::move(prefix.error()));
  auto size = m_packetBuffer.read<std::uint32_t>();
  if (!size)
    return std::unexpected(std::move(size.error()));
  auto payload = receive(*size);
  if (!payload)
    return std::unexpected(std::move(payload.error()));
  std::uint32_t requestId, cmd;
  auto header = m_packetBuffer >> requestId >> cmd;
  if (!header)
    return std::unexpected(std::move(header.error()));

  switch (cmd) {
  case SERVERDATA_AUTH: {
    String password;
    auto decoded = m_packetBuffer >> password;
    if (!decoded)
      return std::unexpected(std::move(decoded.error()));
    m_authed = false;
    if (!m_rconPassword.empty()) {
      auto matches = m_rconPassword.equals(password);
      if (!matches)
        return std::unexpected(std::move(matches.error()));
      m_authed = *matches;
    }
    if (m_authed) {
      auto response = send(requestId, SERVERDATA_RESPONSE_VALUE);
      if (!response)
        return std::unexpected(std::move(response.error()));
      response = send(requestId, SERVERDATA_AUTH_RESPONSE);
      if (!response)
        return std::unexpected(std::move(response.error()));
    } else {
      auto response = sendAuthFailure();
      if (!response)
        return std::unexpected(std::move(response.error()));
    }
    break;
  }
  case SERVERDATA_EXECCOMMAND:
    if (m_authed) {
      String command;
      auto decoded = m_packetBuffer >> command;
      if (!decoded)
        return std::unexpected(std::move(decoded.error()));
      auto logged = m_universe.get().root().logger().info("RCON {}: {}", m_socket->remoteAddress(), command);
      if (!logged)
        return std::unexpected(std::move(logged.error()));
      auto result = handleCommand(command);
      if (!result) {
        auto response = sendCmdResponse(
          requestId,
          strf("RCON: Error executing: {}: {}", command, outputException(ExceptionDiagnostic(result.error()), true)));
        if (!response)
          return std::unexpected(std::move(response.error()));
      } else if (std::holds_alternative<String>(*result)) {
        auto response = sendCmdResponse(requestId, std::get<String>(*result));
        if (!response)
          return std::unexpected(std::move(response.error()));
      } else {
        m_commandPromises[requestId] = std::get<RpcPromise<String>>(*result);
      }
    } else {
      auto response = sendAuthFailure();
      if (!response)
        return std::unexpected(std::move(response.error()));
    }
    break;
  default: {
    auto response = sendCmdResponse(requestId, strf("Unknown request {:06x}", cmd));
    if (!response)
      return std::unexpected(std::move(response.error()));
  }
  }
  for (auto const& requestId : m_commandPromises.keys()) {
    auto& promise = m_commandPromises[requestId];
    auto finished = promise.finished();
    if (!finished)
      return std::unexpected(std::move(finished.error()));
    if (!*finished)
      continue;
    auto succeeded = promise.succeeded();
    if (!succeeded)
      return std::unexpected(std::move(succeeded.error()));
    std::expected<void, StarException> response;
    if (*succeeded) {
      auto result = promise.result();
      if (!result)
        return std::unexpected(std::move(result.error()));
      if (!*result)
        return std::unexpected(StarException("RCON command promise completed without a result"));
      response = sendCmdResponse(requestId, **result);
    } else {
      auto error = promise.error();
      if (!error)
        return std::unexpected(std::move(error.error()));
      if (!*error)
        return std::unexpected(StarException("RCON command promise failed without an error"));
      response = sendCmdResponse(requestId, strf("RCON: Command promise failed: {}", **error));
    }
    if (!response)
      return std::unexpected(std::move(response.error()));
    m_commandPromises.remove(requestId);
  }
  return {};
}

std::expected<void, StarException> ServerRconClient::run() {
  while (!m_stop) {
    auto request = processRequest();
    if (!request) {
      if (!request.error().is<NoMoreRequests>()) {
        auto logged = m_universe.get().root().logger().error(
          "ServerRconClient error: {}", outputException(ExceptionDiagnostic(request.error()), false));
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
  auto text = password->toString();
  if (!text)
    return std::unexpected(std::move(text.error()));
  if (text->empty()) {
    auto logged = universe.root().logger().warn("rconServerPassword is not configured requests will NOT be processed");
    if (!logged)
      return std::unexpected(std::move(logged.error()));
  }
  return std::unique_ptr<ServerRconThread>(new ServerRconThread(universe, std::move(*server)));
}

ServerRconThread::ServerRconThread(UniverseServer& universe, std::unique_ptr<TcpServer> server)
    : Thread("RconServer"), m_universe(universe), m_rconServer(std::move(server)), m_stop(true) {}

ServerRconThread::~ServerRconThread() {
  if (auto stopped = stop(); !stopped)
    EmergencyLog::error("RCON server stop failed: {}", stopped.error().what());
  if (auto joined = join(); !joined)
    EmergencyLog::error("RCON server shutdown failed: {}", joined.error().what());
}

void ServerRconThread::clearClients(bool all) {
  for (auto it = m_clients.begin(); it != m_clients.end();) {
    auto const& pair = *it;
    auto client = pair.second;
    if (all)
      client->stop();
    else if (!client->isRunning()) {
      it = m_clients.erase(it);
      continue;
    }
    ++it;
  }
}

std::expected<void, StarException> ServerRconThread::start() {
  m_stop = false;
  auto started = Thread::start();
  if (!started) {
    m_stop = true;
    return std::unexpected(std::move(started.error()));
  }
  return {};
}

std::expected<void, StarException> ServerRconThread::stop() {
  m_stop = true;
  auto stopped = m_rconServer->stop();
  clearClients(true);
  return stopped;
}

std::expected<void, StarException> ServerRconThread::run() {
  auto loop = [this]() -> std::expected<void, StarException> {
    auto configuration = m_universe.get().root().configuration();
    if (!configuration)
      return std::unexpected(std::move(configuration.error()));
    auto setting = (*configuration)->get("rconServerTimeout");
    if (!setting)
      return std::unexpected(std::move(setting.error()));
    auto timeout = setting->toInt();
    if (!timeout)
      return std::unexpected(std::move(timeout.error()));
    while (!m_stop) {
      auto client = m_rconServer->accept(100);
      if (!client)
        return std::unexpected(std::move(client.error()));
      if (!*client)
        continue;
      auto configured = (*client)->setTimeout(*timeout);
      if (!configured)
        return std::unexpected(std::move(configured.error()));
      auto rconClient = ServerRconClient::create(m_universe.get(), *client);
      if (!rconClient)
        return std::unexpected(std::move(rconClient.error()));
      auto started = (*rconClient)->start();
      if (!started)
        return std::unexpected(std::move(started.error()));
      m_clients[(*client)->remoteAddress().address()] = std::move(*rconClient);
      clearClients();
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

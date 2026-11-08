module star.server_query;

import std;
import star.bytes;
import star.configuration;
import star.data_stream;
import star.data_stream_devices;
import star.format;
import star.host_address;
import star.list;
import star.logging;
import star.output_proxy;
import star.random;
import star.root;
import star.socket;
import star.string;
import star.thread;
import star.udp;
import star.universe_server;
import star.version;
import star.warping;

namespace Star {

std::expected<std::unique_ptr<ServerQueryThread>, StarException>
ServerQueryThread::create(UniverseServer& universe, HostAddressWithPort const& bindAddress) {
  auto server = UdpServer::create(universe.root().logger(), bindAddress);
  if (!server)
    return std::unexpected(std::move(server.error()));
  auto configuration = universe.root().configuration();
  if (!configuration)
    return std::unexpected(std::move(configuration.error()));
  auto maxPlayers = (*configuration)->get("maxPlayers");
  if (!maxPlayers)
    return std::unexpected(std::move(maxPlayers.error()));
  auto count = maxPlayers->toUInt();
  if (!count)
    return std::unexpected(std::move(count.error()));
  auto name = (*configuration)->get("serverName");
  if (!name)
    return std::unexpected(std::move(name.error()));
  auto text = name->toString();
  if (!text)
    return std::unexpected(std::move(text.error()));
  return std::unique_ptr<ServerQueryThread>(
    new ServerQueryThread(universe, std::move(*server), *count, std::move(*text)));
}

ServerQueryThread::ServerQueryThread(UniverseServer& universe, std::unique_ptr<UdpServer> server,
                                     std::uint8_t maxPlayers, String serverName)
    : Thread("QueryServer"), m_universe(universe), m_queryServer(std::move(server)),
      m_playersResponse(A2S_PACKET_SIZE), m_rulesResponse(A2S_PACKET_SIZE), m_generalResponse(A2S_PACKET_SIZE),
      m_serverPort(0), m_maxPlayers(maxPlayers), m_serverName(std::move(serverName)),
      m_lastChallengeCheck(std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() + std::chrono::microseconds(500))), m_lastPlayersResponse(), m_lastRulesResponse(),
      m_lastActiveTime() {
  m_playersResponse.setByteOrder(ByteOrder::LittleEndian);
  m_playersResponse.setNullTerminatedStrings(true);
  m_rulesResponse.setByteOrder(ByteOrder::LittleEndian);
  m_rulesResponse.setNullTerminatedStrings(true);
  m_generalResponse.setByteOrder(ByteOrder::LittleEndian);
  m_generalResponse.setNullTerminatedStrings(true);
}

ServerQueryThread::~ServerQueryThread() {
  stop();
  if (auto joined = join(); !joined)
    EmergencyLog::error("Query server shutdown failed: {}", joined.error().what());
  if (!isJoined())
    fatalError("Cannot destroy a query server with an unjoined native worker", false);
}

std::expected<void, StarException> ServerQueryThread::start() {
  // A completed but unjoined run still owns its cancellation state.
  if (!isJoined())
    return {};
  m_stopSource = std::stop_source{};
  m_lastActiveTime = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() + std::chrono::microseconds(500));
  auto started = Thread::start();
  if (!started) {
    m_stopSource.request_stop();
    return std::unexpected(std::move(started.error()));
  }
  return {};
}

void ServerQueryThread::stop() {
  m_stopSource.request_stop();
  m_queryServer->close();
}

std::expected<void, StarException> ServerQueryThread::sendTo(HostAddressWithPort const& address,
                                                             DataStreamBuffer const& ds) {
  auto sent = m_queryServer->send(address, ds.span());
  if (!sent)
    return std::unexpected(std::move(sent.error()));
  return {};
}

std::uint8_t ServerQueryThread::serverPlayerCount() { return m_universe.get().numberOfClients(); }

bool ServerQueryThread::serverPassworded() {
  // TODO: implement
  return false;
}

std::expected<String, StarException> ServerQueryThread::serverWorldNames() {
  auto activeWorlds = m_universe.get().activeWorlds();
  if (!activeWorlds)
    return std::unexpected(std::move(activeWorlds.error()));
  if (activeWorlds->empty())
    return String("Unknown");
  StringList names;
  for (auto const& world : *activeWorlds) {
    auto name = printWorldId(world);
    if (!name)
      return std::unexpected(std::move(name.error()));
    names.push_back(std::move(*name));
  }
  return names.join(",");
}

std::string_view ServerQueryThread::serverPlugins() {
  // TODO: implement
  return "none";
}

std::expected<bool, StarException> ServerQueryThread::processPacket(HostAddressWithPort const& address,
                                                                    std::span<std::uint8_t const> buf) {
  if (buf.size() < 5 || buf[0] != 0xff || buf[1] != 0xff || buf[2] != 0xff || buf[3] != 0xff) {
    // short packet or missing header
    return false;
  }

  // Process packet
  switch (buf[4]) {
  case A2S_INFO_REQUEST: {
    // Exclude the trailing byte, preserving the existing accepted payload.
    if (buf.size() < 6)
      return false;
    std::string_view str(reinterpret_cast<char const*>(buf.data() + 5), buf.size() - 6);
    if (str.compare(A2S_INFO_REQUEST_STRING) != 0) {
      // Invalid request
      return false;
    }

    auto worlds = serverWorldNames();
    if (!worlds)
      return std::unexpected(std::move(worlds.error()));
    m_generalResponse.clear();
    auto response = m_generalResponse << A2S_HEAD_INT << A2S_INFO_REPLY << A2S_VERSION << m_serverName << *worlds
                                      << GAME_DIR << GAME_DESC
                                      << A2S_APPID// Should be SteamAppId but this isn't a short :(
                                      << serverPlayerCount() << m_maxPlayers << static_cast<std::uint8_t>(0x00)// bots
                                      << A2S_TYPE_DEDICATED// dedicated
#ifdef STAR_SYSTEM_FAMILY_WINDOWS
                                      << A2S_ENV_WINDOWS// os
#elif defined(STAR_SYSTEM_MACOS)
                                      << A2S_ENV_MAC// os
#else
                                      << A2S_ENV_LINUX// os
#endif
                                      << serverPassworded() << A2S_VAC_OFF// secure
                                      << StarVersionString << A2S_EDF_PORT// EDF
                                      << m_serverPort;
    if (!response)
      return std::unexpected(std::move(response.error()));

    auto sent = sendTo(address, m_generalResponse);
    if (!sent)
      return std::unexpected(std::move(sent.error()));
    return true;
  }
  case A2S_CHALLENGE_REQUEST: {
    auto sent = sendChallenge(address);
    if (!sent)
      return std::unexpected(std::move(sent.error()));
    return true;
  }

  case A2S_PLAYER_REQUEST: {
    auto challenge = challengeRequest(address, buf);
    if (!challenge)
      return std::unexpected(std::move(challenge.error()));
    if (*challenge)
      return true;

    if (!validChallenge(address, buf))
      return false;

    auto response = buildPlayerResponse();
    if (!response)
      return std::unexpected(std::move(response.error()));
    auto sent = sendTo(address, m_playersResponse);
    if (!sent)
      return std::unexpected(std::move(sent.error()));
    return true;
  }

  case A2S_RULES_REQUEST: {
    auto challenge = challengeRequest(address, buf);
    if (!challenge)
      return std::unexpected(std::move(challenge.error()));
    if (*challenge)
      return true;

    if (!validChallenge(address, buf))
      return false;

    auto response = buildRuleResponse();
    if (!response)
      return std::unexpected(std::move(response.error()));
    auto sent = sendTo(address, m_rulesResponse);
    if (!sent)
      return std::unexpected(std::move(sent.error()));
    return true;
  }
  }

  return false;
}

std::expected<void, StarException> ServerQueryThread::buildPlayerResponse() {
  auto now = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() + std::chrono::microseconds(500));
  if (now < m_lastPlayersResponse + responseCacheTime)
    return {};
  auto clientIds = m_universe.get().clientIdsAndCreationTime();
  if (!clientIds)
    return std::unexpected(std::move(clientIds.error()));
  std::uint8_t count = static_cast<std::uint8_t>(clientIds->size());
  std::int32_t kills = 0;
  m_playersResponse.clear();
  auto header = m_playersResponse << A2S_HEAD_INT << A2S_PLAYER_REPLY << count;
  if (!header)
    return std::unexpected(std::move(header.error()));
  std::uint8_t i = 0;
  for (auto const& pair : *clientIds) {
    auto name = m_universe.get().clientNick(pair.first);
    if (!name)
      return std::unexpected(std::move(name.error()));
    auto timeConnected = static_cast<float>((now - pair.second).count()) / 1000.f;
    auto player = m_playersResponse << i++ << *name << kills << timeConnected;
    if (!player)
      return std::unexpected(std::move(player.error()));
  }
  m_lastPlayersResponse = now;
  return {};
}

std::expected<void, StarException> ServerQueryThread::buildRuleResponse() {
  auto now = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() + std::chrono::microseconds(500));
  if (now < m_lastRulesResponse + responseCacheTime)
    return {};
  std::uint16_t count = 1;
  m_rulesResponse.clear();
  auto rules = m_rulesResponse << A2S_HEAD_INT << A2S_RULES_REPLY << count << "plugins" << serverPlugins();
  if (!rules)
    return std::unexpected(std::move(rules.error()));
  m_lastRulesResponse = now;
  return {};
}

std::expected<void, StarException> ServerQueryThread::sendChallenge(HostAddressWithPort const& address) {
  auto challenge = std::make_shared<RequestChallenge>(m_universe.get().root().random());
  m_validChallenges[address.address()] = challenge;
  m_generalResponse.clear();
  auto response = m_generalResponse << A2S_HEAD_INT << A2S_CHALLENGE_RESPONSE << challenge->getChallenge();
  if (!response)
    return std::unexpected(std::move(response.error()));
  return sendTo(address, m_generalResponse);
}

void ServerQueryThread::pruneChallenges() {
  auto now = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() + std::chrono::microseconds(500));
  if (now < m_lastChallengeCheck + challengeCheckInterval) {
    return;
  }

  auto expire = now - challengeCheckInterval;
  for (auto it = m_validChallenges.begin(); it != m_validChallenges.end();) {
    if (it->second->before(expire))
      it = m_validChallenges.erase(it);
    else
      ++it;
  }
  m_lastChallengeCheck = now;
}

std::expected<void, StarException> ServerQueryThread::run() {
  auto stopToken = m_stopSource.get_token();
  HostAddressWithPort udpAddress;
  char udpData[MaxUdpData];
  auto report = [this](StarException const& error) -> std::expected<void, StarException> {
    if (!error.is<SocketClosedException>())
      return m_universe.get().root().logger().error("ServerQueryThread error: {}",
                                                    outputException(ExceptionDiagnostic(error), true));
    return {};
  };
  while (!stopToken.stop_requested()) {
    auto received = m_queryServer->receive(std::ref(udpAddress), std::span(udpData), 100);
    if (!received) {
      auto logged = report(received.error());
      if (!logged)
        return std::unexpected(std::move(logged.error()));
      continue;
    }
    pruneChallenges();
    if (*received != 0) {
      auto processed = processPacket(udpAddress, std::span(reinterpret_cast<std::uint8_t const*>(udpData), *received));
      if (!processed) {
        auto logged = report(processed.error());
        if (!logged)
          return std::unexpected(std::move(logged.error()));
      }
    }
  }
  return {};
}

ServerQueryThread::RequestChallenge::RequestChallenge(RandomService& random)
    : m_time(std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() + std::chrono::microseconds(500))), m_challenge(random.randi32()) {}

bool ServerQueryThread::RequestChallenge::before(std::chrono::time_point<std::chrono::steady_clock, std::chrono::duration<std::uint64_t, std::milli>> time) { return m_time < time; }

int ServerQueryThread::RequestChallenge::getChallenge() { return m_challenge; }

bool ServerQueryThread::validChallenge(HostAddressWithPort const& address, std::span<std::uint8_t const> b) {
  if (b.size() != 9) {
    // too much or too little data
    return false;
  }

  auto knownChallenge = m_validChallenges.find_ref(address.address());
  if (!knownChallenge) {
    // Don't know this source address ignore
    return false;
  }

  std::int32_t challenge = (static_cast<std::int32_t>(b[8]) & 0xff) << 24
    | (static_cast<std::int32_t>(b[7]) & 0xff) << 16 | (static_cast<std::int32_t>(b[6]) & 0xff) << 8
    | (static_cast<std::int32_t>(b[5]) & 0xff);
  // Note: No byte order swapping needed as protcol performs no conversion
  if (knownChallenge->get()->getChallenge() != challenge) {
    // Challenges didnt match ignore
    return false;
  }

  // All good
  return true;
}

std::expected<bool, StarException> ServerQueryThread::challengeRequest(HostAddressWithPort const& address,
                                                                       std::span<std::uint8_t const> buf) {
  if (buf.size() != 9) {
    // too much or too little data
    return false;
  }

  if ((buf[5] == 0xff) && (buf[6] == 0xff) && (buf[7] == 0xff) && (buf[8] == 0xff)) {
    auto sent = sendChallenge(address);
    if (!sent)
      return std::unexpected(std::move(sent.error()));
    return true;
  }

  return false;
}

}// namespace Star

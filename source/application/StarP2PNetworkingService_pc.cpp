#ifdef STAR_ENABLE_STEAM_INTEGRATION
#include "steam/steam_api.h"
#include "steam/steam_api_flat.h"
#endif
#ifdef STAR_ENABLE_DISCORD_INTEGRATION
#include "discord/discord.h"
#endif
import std;
import star.format;
import star.string;
import star.list;
import star.logging;
import star.random;
import star.input_event;
import star.application;
import star.host_address;
import star.statistics_service;
import star.p2p_networking_service;
import star.user_generated_content_service;
import star.desktop_service;

#ifdef STAR_ENABLE_STEAM_INTEGRATION
import star.steam_dispatch;
#endif

import star.platform_services_pc;
import star.algorithm;
import star.strong_typedef;
import star.rpc_promise;
import star.p2p_networking_service_pc;
import star.lexical_cast;
import star.logging;
import star.string;
import star.byte_array;
import star.encode;
import star.data_stream;
import star.uuid;

namespace Star {

#ifdef STAR_ENABLE_DISCORD_INTEGRATION

discord::NetworkChannelId const DiscordMainNetworkChannel = 0;

// SDK callbacks cannot return diagnostics; the owning state carries their errors
// to the application's checked update boundary.
static void recordDiscordCallbackError(PcPlatformServicesState& state, StarException error) {
  auto logged = state.logger.error("{}", error.what());
  if (!logged)
    error = StarException(strf("Discord callback logging failed: {}", logged.error().what()), ExceptionDiagnostic(error));
  state.discordError = std::move(error);
}

static void deleteDiscordLobby(PcPlatformServicesState& state, discord::LobbyId lobbyId) {
  state.discordCore->LobbyManager().DeleteLobby(lobbyId, [lobbyId, &state](discord::Result res) {
    if (res != discord::Result::Ok)
      recordDiscordCallbackError(state, ApplicationException::format("Could not delete Discord lobby {} (err {})", lobbyId, (int)res));
  });
}

static void disconnectDiscordLobby(PcPlatformServicesState& state, discord::LobbyId lobbyId) {
  state.discordCore->LobbyManager().DisconnectLobby(lobbyId, [lobbyId, &state](discord::Result res) {
    if (res != discord::Result::Ok)
      recordDiscordCallbackError(state, ApplicationException::format("Could not leave Discord lobby {} (err {})", lobbyId, (int)res));
  });
}

#endif

PcP2PNetworkingService::PcP2PNetworkingService(std::shared_ptr<PcPlatformServicesState> state)
  : m_state(std::move(state))
#ifdef STAR_ENABLE_STEAM_INTEGRATION
  , m_callbackConnectionFailure(m_state->steamDispatch, *this, &PcP2PNetworkingService::steamOnConnectionFailure),
    m_callbackJoinRequested(m_state->steamDispatch, *this, &PcP2PNetworkingService::steamOnJoinRequested),
    m_callbackSessionRequest(m_state->steamDispatch, *this, &PcP2PNetworkingService::steamOnSessionRequest)
#endif
{

#ifdef STAR_ENABLE_DISCORD_INTEGRATION
  if (m_state->discordAvailable) {
    std::lock_guard discordLocker(m_state->discordMutex);
    m_discordLifetime->service = std::ref(*this);

    m_discordPartySize = {};

    m_discordOnActivityJoinToken = m_state->discordCore->ActivityManager().OnActivityJoin.Connect([&service = *this](char const* peerId) {
        if (auto logged = service.m_state->logger.info("Joining Discord peer at '{}'", peerId); !logged) {
          service.m_state->discordError = std::move(logged.error());
          return;
        }
        auto joined = service.addPendingJoin(strf("+platform:{}", peerId));
        if (!joined) {
          std::lock_guard serviceLocker(service.m_mutex);
          recordDiscordCallbackError(*service.m_state, std::move(joined.error()));
        }
      });
    m_discordOnActivityRequestToken = m_state->discordCore->ActivityManager().OnActivityJoinRequest.Connect([&service = *this](discord::User const& user) {
        std::lock_guard serviceLocker(service.m_mutex);
        String userName(std::string_view(user.GetUsername()));
        if (auto logged = service.m_state->logger.info("Received join request from user '{}'", userName); !logged) {
          service.m_state->discordError = std::move(logged.error());
          return;
        }
        service.m_discordJoinRequests.emplace_back(std::make_pair(user.GetId(), userName));
      });
    m_discordOnReceiveMessage = m_state->discordCore->LobbyManager().OnNetworkMessage.Connect([&service = *this](discord::LobbyId lobby, discord::UserId user, discord::NetworkChannelId channel, std::uint8_t* data, std::uint32_t size) {
      service.discordOnReceiveMessage(lobby, user, channel, {data, size});
    });
    m_discordOnLobbyMemberConnect = m_state->discordCore->LobbyManager().OnMemberConnect.Connect([&service = *this](discord::LobbyId lobby, discord::UserId user) { service.discordOnLobbyMemberConnect(lobby, user); });
    m_discordOnLobbyMemberUpdate = m_state->discordCore->LobbyManager().OnMemberUpdate.Connect([&service = *this](discord::LobbyId lobby, discord::UserId user) { service.discordOnLobbyMemberUpdate(lobby, user); });
    m_discordOnLobbyMemberDisconnect = m_state->discordCore->LobbyManager().OnMemberDisconnect.Connect([&service = *this](discord::LobbyId lobby, discord::UserId user) { service.discordOnLobbyMemberDisconnect(lobby, user); });
  }
#endif
}

PcP2PNetworkingService::~PcP2PNetworkingService() {
#ifdef STAR_ENABLE_DISCORD_INTEGRATION
  if (m_state->discordAvailable) {
    std::lock_guard discordLocker(m_state->discordMutex);
    m_discordLifetime->service.reset();
    for (auto const& entry : m_discordOpenSockets) {
      std::lock_guard socketLocker(entry.second.get().mutex);
      entry.second.get().mode = DiscordSocketMode::Disconnected;
    }
    m_discordOpenSockets.clear();
    if (m_discordServerLobby) {
      auto const& lobby = *m_discordServerLobby;
      auto res = m_state->discordCore->LobbyManager().DisconnectNetwork(lobby.first);
      if (res != discord::Result::Ok) {
        auto logged = m_state->logger.error("Could not disconnect Discord lobby network {} (err {})", lobby.first, (int)res);
        if (!logged)
          EmergencyLog::error("Could not log Discord cleanup failure: {}", logged.error().what());
      }
      if (lobby.second.empty())
        disconnectDiscordLobby(*m_state, lobby.first);
      else
        deleteDiscordLobby(*m_state, lobby.first);
      m_discordServerLobby = {};
    }

    discordDisconnectCallbacks();
  }
#endif
}

#ifdef STAR_ENABLE_DISCORD_INTEGRATION
void PcP2PNetworkingService::discordDisconnectCallbacks() {
  m_state->discordCore->ActivityManager().OnActivityJoin.Disconnect(std::exchange(m_discordOnActivityJoinToken, -1));
  m_state->discordCore->ActivityManager().OnActivityJoinRequest.Disconnect(std::exchange(m_discordOnActivityRequestToken, -1));
  m_state->discordCore->LobbyManager().OnNetworkMessage.Disconnect(std::exchange(m_discordOnReceiveMessage, -1));
  m_state->discordCore->LobbyManager().OnMemberConnect.Disconnect(std::exchange(m_discordOnLobbyMemberConnect, -1));
  m_state->discordCore->LobbyManager().OnMemberUpdate.Disconnect(std::exchange(m_discordOnLobbyMemberUpdate, -1));
  m_state->discordCore->LobbyManager().OnMemberDisconnect.Disconnect(std::exchange(m_discordOnLobbyMemberDisconnect, -1));
}
#endif

std::expected<void, StarException> PcP2PNetworkingService::setJoinUnavailable() {
  return setJoinLocation(JoinUnavailable());
}

std::expected<void, StarException> PcP2PNetworkingService::setJoinLocal(std::uint32_t capacity) {
  return setJoinLocation(JoinLocal{capacity});
}

std::expected<void, StarException> PcP2PNetworkingService::setJoinRemote(HostAddressWithPort location) {
  return setJoinLocation(JoinRemote(location));
}

std::expected<void, StarException> Star::PcP2PNetworkingService::setActivityData(
  [[maybe_unused]] std::string_view title,
  [[maybe_unused]] std::string_view details,
  [[maybe_unused]] std::int64_t startTime,
  [[maybe_unused]] std::optional<std::pair<std::uint16_t, std::uint16_t>> party) {
#ifdef STAR_ENABLE_DISCORD_INTEGRATION
  std::lock_guard discordLocker(m_state->discordMutex);
#endif
  std::lock_guard serviceLocker(m_mutex);

#ifdef STAR_ENABLE_DISCORD_INTEGRATION
  // Presence-only RPC clients can accept activities without exposing a user.
  if (m_state->discordAvailable) {
    if (m_discordUpdatingActivity)
      return {};

    if (title != std::string_view(m_discordActivityTitle.utf8())
     || details != std::string_view(m_discordActivityDetails.utf8())
     || startTime != m_discordActivityStartTime || party != m_discordPartySize || m_discordForceUpdateActivity) {

      discord::Activity activity = {};
      activity.SetType(discord::ActivityType::Playing);
      activity.SetName("Starbound");
      String activityTitle(title);
      String activityDetails(details);
      activity.SetState(activityTitle.utf8Ptr());
      activity.SetDetails(activityDetails.utf8Ptr());
      activity.GetTimestamps().SetStart(startTime);
      if (party) {
        auto& size = activity.GetParty().GetSize();
        size.SetCurrentSize(party->first);
        size.SetMaxSize(party->second);
      }
  
      if (auto lobby = m_discordServerLobby)
        activity.GetParty().SetId(toString(lobby->first).c_str());

      if (std::holds_alternative<JoinLocal>(m_joinLocation)) {
        if (auto lobby = m_discordServerLobby; lobby && m_state->discordCurrentUser) {
          String joinSecret = strf("connect:discord_{}_{}_{}", m_state->discordCurrentUser->GetId(), lobby->first, lobby->second);
          if (auto logged = m_state->logger.info("Setting Discord join secret as {}", joinSecret); !logged)
            return std::unexpected(std::move(logged.error()));
          activity.GetSecrets().SetJoin(joinSecret.utf8Ptr());
        }
      } else if (std::holds_alternative<JoinRemote>(m_joinLocation)) {
        String address = toString((HostAddressWithPort)std::get<JoinRemote>(m_joinLocation));
        String joinSecret = strf("connect:address_{}", address);
        if (auto logged = m_state->logger.info("Setting Discord join secret as {}", joinSecret); !logged)
          return std::unexpected(std::move(logged.error()));
        activity.GetSecrets().SetJoin(joinSecret.utf8Ptr());
      
        activity.GetParty().SetId(address.utf8Ptr());
      }
    
      m_discordForceUpdateActivity = false;
      m_discordPartySize = party;
      m_discordActivityTitle = std::move(activityTitle);
      m_discordActivityDetails = std::move(activityDetails);
      m_discordActivityStartTime = startTime;
      m_discordUpdatingActivity = true;
      m_state->discordCore->ActivityManager().UpdateActivity(activity, [lifetime = m_discordLifetime, &state = *m_state](discord::Result res) {
          // Rich Presence is optional; only a failure to report its diagnostic
          // belongs at the platform's checked error boundary.
          if (auto service = lifetime->service) {
            std::lock_guard serviceLocker(service->get().m_mutex);
            service->get().m_discordUpdatingActivity = false;
          }
          if (res != discord::Result::Ok) {
            if (auto logged = state.logger.warn("Failed to set Discord activity (err {})", (int)res); !logged)
              state.discordError = StarException(strf("Discord activity failure logging failed: {}", logged.error().what()),
                ExceptionDiagnostic(ApplicationException::format("Failed to set Discord activity (err {})", (int)res)));
          }
        });
    }
  }
#endif
  return {};
}

std::variant<std::monostate, P2PNetworkingPeerId, HostAddressWithPort> PcP2PNetworkingService::pullPendingJoin() {
  std::lock_guard serviceLocker(m_mutex);
  auto join = std::move(m_pendingJoin);
  m_pendingJoin.emplace<0>();
  return join;
}

std::optional<std::pair<String, RpcPromiseKeeper<P2PJoinRequestReply>>> Star::PcP2PNetworkingService::pullJoinRequest() {
  std::lock_guard serviceLocker(m_mutex);

#ifdef STAR_ENABLE_DISCORD_INTEGRATION
  if (auto request = m_discordJoinRequests.maybeTakeLast()) {
    auto promisePair = RpcPromise<P2PJoinRequestReply>::createPair();
    m_pendingDiscordJoinRequests.push_back(std::make_pair(request->first, promisePair.first));
    return std::make_pair(request->second, promisePair.second);
  }
#endif

  return {};
}

void PcP2PNetworkingService::setAcceptingP2PConnections(bool acceptingP2PConnections) {
  List<std::unique_ptr<P2PSocket>> rejectedConnections;
  {
    std::lock_guard serviceLocker(m_mutex);
    m_acceptingP2PConnections = acceptingP2PConnections;
    if (!m_acceptingP2PConnections)
      rejectedConnections = std::exchange(m_pendingIncomingConnections, {});
  }
  // Socket destructors acquire SDK/service/socket locks in that order.
}

List<std::unique_ptr<P2PSocket>> PcP2PNetworkingService::acceptP2PConnections() {
  std::lock_guard serviceLocker(m_mutex);
  return std::exchange(m_pendingIncomingConnections, {});
}

std::expected<void, StarException> Star::PcP2PNetworkingService::update() {
#ifdef STAR_ENABLE_DISCORD_INTEGRATION
  std::lock_guard discordLocker(m_state->discordMutex);
#endif
  std::lock_guard serviceLocker(m_mutex);
  
#ifdef STAR_ENABLE_DISCORD_INTEGRATION
  for (auto& p : m_pendingDiscordJoinRequests) {
    auto result = p.second.result();
    if (!result)
      return std::unexpected(std::move(result.error()));
    if (auto const& res = *result) {
      auto reply = discord::ActivityJoinRequestReply::Ignore;
      switch (*res) {
        case P2PJoinRequestReply::Yes:
          reply = discord::ActivityJoinRequestReply::Yes;
          break;
        case P2PJoinRequestReply::No:
          reply = discord::ActivityJoinRequestReply::No;
          break;
        case P2PJoinRequestReply::Ignore:
          reply = discord::ActivityJoinRequestReply::Ignore;
          break;
      }

      m_state->discordCore->ActivityManager().SendRequestReply(p.first, reply, [&state = *m_state](discord::Result res) {
          if (res != discord::Result::Ok)
            recordDiscordCallbackError(state, ApplicationException::format("Could not send Discord activity join response (err {})", (int)res));
        });
    }
  }
  for (auto it = m_pendingDiscordJoinRequests.begin(); it != m_pendingDiscordJoinRequests.end();) {
    auto finished = it->second.finished();
    if (!finished)
      return std::unexpected(std::move(finished.error()));
    if (*finished)
      it = m_pendingDiscordJoinRequests.erase(it);
    else
      ++it;
  }
#endif
  return {};
}

std::expected<std::unique_ptr<P2PSocket>, StarException> PcP2PNetworkingService::connectToPeer(P2PNetworkingPeerId peerId) {
#ifdef STAR_ENABLE_DISCORD_INTEGRATION
  std::lock_guard discordLocker(m_state->discordMutex);
#endif
  std::lock_guard serviceLocker(m_mutex);
  auto extracted = peerId.extract("_");
  if (!extracted)
    return std::unexpected(std::move(extracted.error()));
  String type = std::move(*extracted);

#ifdef STAR_ENABLE_STEAM_INTEGRATION
  if (m_state->steamAvailable) {
    if (type == "steamid") {
      auto parsed = lexicalCast<uint64>(peerId);
      if (!parsed)
        return std::unexpected(std::move(parsed.error()));
      return std::unique_ptr<P2PSocket>(createSteamP2PSocket(CSteamID(*parsed)));
    }
  }
#endif

#ifdef STAR_ENABLE_DISCORD_INTEGRATION
  if (m_state->discordAvailable) {
    if (type == "discord") {
      if (!m_discordLobbiesAvailable)
        return std::unexpected(ApplicationException("Discord lobby networking is not supported by this Discord client"));
      auto userPart = peerId.extract("_");
      if (!userPart)
        return std::unexpected(std::move(userPart.error()));
      auto remoteUserId = lexicalCast<discord::UserId>(*userPart);
      if (!remoteUserId)
        return std::unexpected(std::move(remoteUserId.error()));
      auto lobbyPart = peerId.extract("_");
      if (!lobbyPart)
        return std::unexpected(std::move(lobbyPart.error()));
      auto lobbyId = lexicalCast<discord::LobbyId>(*lobbyPart);
      if (!lobbyId)
        return std::unexpected(std::move(lobbyId.error()));
      return discordConnectRemote(*remoteUserId, *lobbyId, peerId);
    }
  }
#endif

  return std::unexpected(ApplicationException::format("Unsupported peer type '{}'", type));
}

std::expected<void, StarException> PcP2PNetworkingService::addPendingJoin(String connectionString) {
  std::lock_guard serviceLocker(m_mutex);
  auto platform = connectionString.extract(":");
  if (!platform)
    return std::unexpected(std::move(platform.error()));
  if (*platform != "+platform")
    return std::unexpected(ApplicationException::format("malformed connection string '{}'", connectionString));
  auto command = connectionString.extract(":");
  if (!command)
    return std::unexpected(std::move(command.error()));
  if (*command != "connect")
    return std::unexpected(ApplicationException::format("malformed connection string '{}'", connectionString));
  auto targetType = connectionString.extract("_");
  if (!targetType)
    return std::unexpected(std::move(targetType.error()));
  if (*targetType == "address") {
    auto address = HostAddressWithPort::create(connectionString);
    if (!address)
      return std::unexpected(std::move(address.error()));
    m_pendingJoin = std::move(*address);
  } else {
    m_pendingJoin = P2PNetworkingPeerId(strf("{}_{}", *targetType, connectionString));
  }
  return {};
}

#ifdef STAR_ENABLE_STEAM_INTEGRATION

PcP2PNetworkingService::SteamP2PSocket::~SteamP2PSocket() {
  std::lock_guard serviceLocker(parent.get().m_mutex);
  std::lock_guard socketLocker(mutex);
  parent.get().steamCloseSocket(*this);
}

bool PcP2PNetworkingService::SteamP2PSocket::isOpen() {
  std::lock_guard socketLocker(mutex);
  return connected;
}

std::expected<bool, StarException> PcP2PNetworkingService::SteamP2PSocket::sendMessage(ByteArray const& message) {
  std::lock_guard socketLocker(mutex);
  if (error)
    return std::unexpected(*error);
  if (!connected)
    return false;

  if (!SteamAPI_ISteamNetworking_SendP2PPacket(SteamAPI_SteamNetworking(), steamId.ConvertToUint64(),
      message.ptr(), message.size(), k_EP2PSendReliable, 0))
    return std::unexpected(ApplicationException("SteamNetworking::SendP2PPacket unexpectedly returned false"));
  return true;
}

std::expected<std::optional<ByteArray>, StarException> PcP2PNetworkingService::SteamP2PSocket::receiveMessage() {
  std::unique_lock socketLocker(mutex);
  if (error)
    return std::unexpected(*error);
  if (!incoming.empty()) {
    auto message = incoming.takeFirst();
    if (!message)
      return std::unexpected(std::move(message.error()));
    return std::optional<ByteArray>(std::move(*message));
  }

  if (connected) {
    socketLocker.unlock();
    {
      std::lock_guard serviceLocker(parent.get().m_mutex);
      auto received = parent.get().steamReceiveAll();
      if (!received)
        return std::unexpected(std::move(received.error()));
    }

    socketLocker.lock();
    if (!incoming.empty()) {
      auto message = incoming.takeFirst();
      if (!message)
        return std::unexpected(std::move(message.error()));
      return std::optional<ByteArray>(std::move(*message));
    }
  }

  return {};
}

auto PcP2PNetworkingService::createSteamP2PSocket(CSteamID steamId) -> std::unique_ptr<SteamP2PSocket> {
  if (auto oldSocket = m_steamOpenSockets.find_ref(steamId.ConvertToUint64())) {
    std::lock_guard socketLocker(oldSocket->get().get().mutex);
    steamCloseSocket(oldSocket->get().get());
  }

  auto socket = std::make_unique<SteamP2PSocket>(*this);
  socket->steamId = steamId;
  socket->connected = true;

  m_steamOpenSockets.insert_or_assign(steamId.ConvertToUint64(), std::ref(*socket));

  return socket;
}

std::expected<void, StarException> PcP2PNetworkingService::steamOnConnectionFailure(P2PSessionConnectFail_t& callback) {
  std::lock_guard serviceLocker(m_mutex);
  auto logged = m_state->logger.warn("Connection with Steam user {} failed (error code: {})", callback.m_steamIDRemote.ConvertToUint64(), callback.m_eP2PSessionError);
  if (auto socket = m_steamOpenSockets.find_ref(callback.m_steamIDRemote.ConvertToUint64())) {
    std::lock_guard socketLocker(socket->get().get().mutex);
    socket->get().get().error = ApplicationException::format("Connection with Steam user {} failed (error code: {})", callback.m_steamIDRemote.ConvertToUint64(), callback.m_eP2PSessionError);
    steamCloseSocket(socket->get().get());
  }
  if (!logged)
    return std::unexpected(std::move(logged.error()));
  return {};
}

std::expected<void, StarException> PcP2PNetworkingService::steamOnJoinRequested(GameRichPresenceJoinRequested_t& callback) {
  if (auto logged = m_state->logger.info("Enqueueing join request with Steam friend id {} to address {}", callback.m_steamIDFriend.ConvertToUint64(), callback.m_rgchConnect); !logged)
    return std::unexpected(std::move(logged.error()));
  return addPendingJoin(String(std::string_view(callback.m_rgchConnect)));
}

std::expected<void, StarException> PcP2PNetworkingService::steamOnSessionRequest(P2PSessionRequest_t& callback) {
  std::lock_guard serviceLocker(m_mutex);
  // Not sure whether this HasFriend call is actually necessary, or whether
  // non-friends can even initiate P2P sessions.
  if (m_acceptingP2PConnections && SteamAPI_ISteamFriends_HasFriend(SteamAPI_SteamFriends(),
      callback.m_steamIDRemote.ConvertToUint64(), k_EFriendFlagImmediate)) {
    if (SteamAPI_ISteamNetworking_AcceptP2PSessionWithUser(SteamAPI_SteamNetworking(),
        callback.m_steamIDRemote.ConvertToUint64())) {
      m_pendingIncomingConnections.append(createSteamP2PSocket(callback.m_steamIDRemote));
      return m_state->logger.info("Accepted Steam P2P connection with user {}", callback.m_steamIDRemote.ConvertToUint64());
    } else {
      return m_state->logger.error("Accepting Steam P2P connection from user {} failed!", callback.m_steamIDRemote.ConvertToUint64());
    }
  } else {
    return m_state->logger.error("Ignoring Steam P2P connection from user {}", callback.m_steamIDRemote.ConvertToUint64());
  }
  return {};
}

void PcP2PNetworkingService::steamCloseSocket(SteamP2PSocket& socket) {
  if (socket.connected) {
    auto logged = m_state->logger.info("Closing P2P connection with Steam user {}", socket.steamId.ConvertToUint64());
    if (!logged)
      EmergencyLog::error("Could not log Steam socket cleanup: {}", logged.error().what());
    m_steamOpenSockets.remove(socket.steamId.ConvertToUint64());
    socket.connected = false;
  }
  SteamAPI_ISteamNetworking_CloseP2PSessionWithUser(SteamAPI_SteamNetworking(), socket.steamId.ConvertToUint64());
}

std::expected<void, StarException> PcP2PNetworkingService::steamReceiveAll() {
  std::uint32_t messageSize;
  CSteamID messageRemoteUser;
  while (SteamAPI_ISteamNetworking_IsP2PPacketAvailable(SteamAPI_SteamNetworking(), &messageSize, 0)) {
    ByteArray data(messageSize, 0);
    if (!SteamAPI_ISteamNetworking_ReadP2PPacket(SteamAPI_SteamNetworking(), data.ptr(), messageSize,
        &messageSize, &messageRemoteUser, 0))
      return std::unexpected(ApplicationException("SteamNetworking::ReadP2PPacket returned false"));
    data.resize(messageSize);
    if (auto openSocket = m_steamOpenSockets.find_ref(messageRemoteUser.ConvertToUint64())) {
      std::lock_guard socketLocker(openSocket->get().get().mutex);
      openSocket->get().get().incoming.append(std::move(data));
    }
  }
  return {};
}

#endif

#ifdef STAR_ENABLE_DISCORD_INTEGRATION

PcP2PNetworkingService::DiscordP2PSocket::~DiscordP2PSocket() {
  std::lock_guard discordLocker(state->discordMutex);
  if (auto service = lifetime->service) {
    std::lock_guard serviceLocker(service->get().m_mutex);
    std::lock_guard socketLocker(mutex);
    service->get().discordCloseSocket(*this);
  }
}

bool PcP2PNetworkingService::DiscordP2PSocket::isOpen() {
  std::lock_guard socketLocker(mutex);
  return mode != DiscordSocketMode::Disconnected;
}

std::expected<bool, StarException> PcP2PNetworkingService::DiscordP2PSocket::sendMessage(ByteArray const& message) {
  std::lock_guard discordLocker(state->discordMutex);
  std::lock_guard socketLocker(mutex);
  if (error)
    return std::unexpected(*error);
  if (!lifetime->service || mode != DiscordSocketMode::Connected)
    return false;

  discord::Result res = state->discordCore->LobbyManager().SendNetworkMessage(lobbyId, remoteUserId, DiscordMainNetworkChannel, (std::uint8_t*)message.ptr(), message.size());
  if (res != discord::Result::Ok)
    return std::unexpected(ApplicationException::format("discord::Network::Send returned error (err {})", (int)res));

  return true;
}

std::expected<std::optional<ByteArray>, StarException> PcP2PNetworkingService::DiscordP2PSocket::receiveMessage() {
  std::lock_guard socketLocker(mutex);
  if (error)
    return std::unexpected(*error);
  if (!incoming.empty()) {
    auto message = incoming.takeFirst();
    if (!message)
      return std::unexpected(std::move(message.error()));
    return std::optional<ByteArray>(std::move(*message));
  }
  return {};
}

void PcP2PNetworkingService::discordCloseSocket(DiscordP2PSocket& socket) {
  if (socket.mode != DiscordSocketMode::Disconnected) {
    m_discordOpenSockets.remove(socket.remoteUserId);

    if (socket.mode == DiscordSocketMode::Connected) {
      if (!std::holds_alternative<JoinLocal>(m_joinLocation) && m_discordOpenSockets.empty()) {
        auto res = m_state->discordCore->LobbyManager().DisconnectNetwork(socket.lobbyId);
        if (res != discord::Result::Ok) {
          auto logged = m_state->logger.error("Failed to leave network for lobby {} (err {})", socket.lobbyId, (int)res);
          if (!logged)
            EmergencyLog::error("Could not log Discord socket cleanup: {}", logged.error().what());
        }

        disconnectDiscordLobby(*m_state, socket.lobbyId);
        if (m_discordServerLobby && m_discordServerLobby->first == socket.lobbyId) {
          m_discordServerLobby = {};
          m_discordForceUpdateActivity = true;
        }
      }
    }

    socket.mode = DiscordSocketMode::Disconnected;
  }
}

std::expected<std::unique_ptr<P2PSocket>, StarException> PcP2PNetworkingService::discordConnectRemote(discord::UserId remoteUserId, discord::LobbyId lobbyId, String const& lobbySecret) {
  if (auto logged = m_state->logger.info("Connecting to Discord lobby {}", lobbyId); !logged)
    return std::unexpected(std::move(logged.error()));
  if (auto oldSocket = m_discordOpenSockets.find_ref(remoteUserId)) {
    std::lock_guard socketLocker(oldSocket->get().get().mutex);
    discordCloseSocket(oldSocket->get().get());
  }

  std::unique_ptr<DiscordP2PSocket> socket(new DiscordP2PSocket);
  socket->state = m_state;
  socket->lifetime = m_discordLifetime;
  socket->connectionGeneration = ++m_discordConnectionGeneration;
  socket->mode = DiscordSocketMode::Startup;
  socket->remoteUserId = remoteUserId;
  socket->lobbyId = lobbyId;
  m_discordOpenSockets.insert_or_assign(remoteUserId, std::ref(*socket));

  m_state->discordCore->LobbyManager().ConnectLobby(lobbyId, lobbySecret.utf8Ptr(),
    [lifetime = m_discordLifetime, &state = *m_state, remoteUserId, lobbyId,
     generation = socket->connectionGeneration](discord::Result res, discord::Lobby const&) {
      if (res != discord::Result::Ok)
        recordDiscordCallbackError(state, ApplicationException::format("Failed to connect to remote lobby {} (err {})", lobbyId, (int)res));
      auto service = lifetime->service;
      if (!service) {
        if (res == discord::Result::Ok)
          disconnectDiscordLobby(state, lobbyId);
        return;
      }
      std::lock_guard serviceLocker(service->get().m_mutex);
      auto socketRef = service->get().m_discordOpenSockets.find_ref(remoteUserId);
      if (!socketRef || socketRef->get().get().connectionGeneration != generation) {
        if (res == discord::Result::Ok) {
          // A replacement connection to this lobby owns the shared membership.
          bool inUse = service->get().m_discordServerLobby && service->get().m_discordServerLobby->first == lobbyId;
          for (auto const& entry : service->get().m_discordOpenSockets)
            inUse = inUse || entry.second.get().lobbyId == lobbyId;
          if (!inUse)
            disconnectDiscordLobby(state, lobbyId);
        }
        return;
      }
      auto& socket = socketRef->get().get();
      std::lock_guard socketLocker(socket.mutex);
      if (res != discord::Result::Ok) {
        socket.error = ApplicationException::format("Discord lobby {} connection failed (err {})", lobbyId, (int)res);
        service->get().discordCloseSocket(socket);
        return;
      }
      res = state.discordCore->LobbyManager().ConnectNetwork(lobbyId);
      if (res != discord::Result::Ok) {
        socket.error = ApplicationException::format("Discord lobby {} network connection failed (err {})", lobbyId, (int)res);
        recordDiscordCallbackError(state, *socket.error);
        service->get().discordCloseSocket(socket);
        disconnectDiscordLobby(state, lobbyId);
        return;
      }
      res = state.discordCore->LobbyManager().OpenNetworkChannel(lobbyId, DiscordMainNetworkChannel, true);
      if (res != discord::Result::Ok) {
        socket.error = ApplicationException::format("Discord lobby {} channel opening failed (err {})", lobbyId, (int)res);
        recordDiscordCallbackError(state, *socket.error);
        auto disconnectResult = state.discordCore->LobbyManager().DisconnectNetwork(lobbyId);
        if (disconnectResult != discord::Result::Ok)
          recordDiscordCallbackError(state, ApplicationException::format("Could not disconnect Discord lobby network {} (err {})", lobbyId, (int)disconnectResult));
        service->get().discordCloseSocket(socket);
        disconnectDiscordLobby(state, lobbyId);
        return;
      }
      socket.mode = DiscordSocketMode::Connected;
      if (auto logged = state.logger.info("Discord P2P connection opened to remote user {} via lobby {}", remoteUserId, lobbyId); !logged)
        state.discordError = std::move(logged.error());
      service->get().m_discordServerLobby = std::make_pair(lobbyId, String());
      service->get().m_discordForceUpdateActivity = true;
    });

  return std::unique_ptr<P2PSocket>(std::move(socket));
}

void PcP2PNetworkingService::discordOnReceiveMessage(discord::LobbyId lobbyId, discord::UserId userId, discord::NetworkChannelId channel, std::span<std::uint8_t const> data) {
  std::lock_guard serviceLocker(m_mutex);

  if (!m_discordServerLobby || lobbyId != m_discordServerLobby->first) {
    recordDiscordCallbackError(*m_state, ApplicationException::format("Received message from unexpected lobby {}", lobbyId));
    return;
  }

  if (auto socket = m_discordOpenSockets.find_ref(userId)) {
    if (channel == DiscordMainNetworkChannel) {
      std::lock_guard socketLocker(socket->get().get().mutex);
      socket->get().get().incoming.append(
        ByteArray(std::span(reinterpret_cast<char const*>(data.data()), data.size())));
    } else {
      recordDiscordCallbackError(
        *m_state, ApplicationException::format("Received Discord message on unexpected channel {}, ignoring", channel));
    }
  } else {
    recordDiscordCallbackError(
      *m_state, ApplicationException::format("Could not find associated Discord socket for user id {}", userId));
  }
  }

void PcP2PNetworkingService::discordOnLobbyMemberConnect(discord::LobbyId lobbyId, discord::UserId userId) {
  std::lock_guard serviceLocker(m_mutex);
  if (m_discordServerLobby && m_discordServerLobby->first == lobbyId && !m_state->discordCurrentUser) {
    recordDiscordCallbackError(*m_state, ApplicationException("Discord current user is unavailable during lobby member connection"));
    return;
  }

  if (m_discordServerLobby && m_discordServerLobby->first == lobbyId && m_state->discordCurrentUser && userId != m_state->discordCurrentUser->GetId()) {
    if (!m_discordOpenSockets.contains(userId)) {
      std::unique_ptr<DiscordP2PSocket> socket(new DiscordP2PSocket);
      socket->state = m_state;
      socket->lifetime = m_discordLifetime;
      socket->lobbyId = lobbyId;
      socket->remoteUserId = userId;
      socket->mode = DiscordSocketMode::Connected;

      m_discordOpenSockets.insert_or_assign(userId, std::ref(*socket));
      m_pendingIncomingConnections.append(std::move(socket));
      if (auto logged = m_state->logger.info("Accepted new Discord connection from remote user {}", userId); !logged)
        m_state->discordError = std::move(logged.error());
    }
  }
}

void PcP2PNetworkingService::discordOnLobbyMemberUpdate(discord::LobbyId lobbyId, discord::UserId userId) {
  discordOnLobbyMemberConnect(lobbyId, userId);
}

void PcP2PNetworkingService::discordOnLobbyMemberDisconnect(discord::LobbyId lobbyId, discord::UserId userId) {
  std::lock_guard serviceLocker(m_mutex);
  if (m_discordServerLobby && m_discordServerLobby->first == lobbyId && !m_state->discordCurrentUser) {
    recordDiscordCallbackError(*m_state, ApplicationException("Discord current user is unavailable during lobby member disconnection"));
    return;
  }

  if (m_discordServerLobby && m_discordServerLobby->first == lobbyId && m_state->discordCurrentUser && userId != m_state->discordCurrentUser->GetId()) {
    if (auto socket = m_discordOpenSockets.find_ref(userId)) {
      std::lock_guard socketLocker(socket->get().get().mutex);
      discordCloseSocket(socket->get().get());
    }
  }
}

#endif

std::expected<void, StarException> PcP2PNetworkingService::setJoinLocation(JoinLocation location) {
#ifdef STAR_ENABLE_DISCORD_INTEGRATION
  std::lock_guard discordLocker(m_state->discordMutex);
#endif
  std::lock_guard serviceLocker(m_mutex);

  if (location == m_joinLocation)
    return {};
#ifdef STAR_ENABLE_DISCORD_INTEGRATION
  std::optional<discord::LobbyTransaction> createLobby;
  if (m_state->discordAvailable && m_state->discordCurrentUser && m_discordLobbiesAvailable) {
    if (std::holds_alternative<JoinLocal>(location)) {
      createLobby.emplace();
      auto result = m_state->discordCore->LobbyManager().GetLobbyCreateTransaction(&*createLobby);
      if (result != discord::Result::Ok)
        return std::unexpected(ApplicationException::format("discord::Lobbies::CreateLobbyTransaction failed (err {})", (int)result));
      result = createLobby->SetCapacity(std::get<JoinLocal>(location).capacity);
      if (result != discord::Result::Ok)
        return std::unexpected(ApplicationException::format("Discord lobby capacity was rejected (err {})", (int)result));
      result = createLobby->SetType(discord::LobbyType::Private);
      if (result != discord::Result::Ok)
        return std::unexpected(ApplicationException::format("Discord lobby type was rejected (err {})", (int)result));
    }
  }
#endif

#ifdef STAR_ENABLE_STEAM_INTEGRATION
  if (m_state->steamAvailable) {
    if (std::holds_alternative<JoinUnavailable>(location)) {
      if (auto logged = m_state->logger.info("Clearing Steam rich presence connection"); !logged)
        return std::unexpected(std::move(logged.error()));
      if (!SteamAPI_ISteamFriends_SetRichPresence(SteamAPI_SteamFriends(), "connect", ""))
        return std::unexpected(ApplicationException("Steam rejected clearing rich presence"));

    } else if (std::holds_alternative<JoinLocal>(location)) {
      auto steamId = SteamAPI_ISteamUser_GetSteamID(SteamAPI_SteamUser());
      if (auto logged = m_state->logger.info("Setting Steam rich presence connection as steamid_{}", steamId); !logged)
        return std::unexpected(std::move(logged.error()));
      if (!SteamAPI_ISteamFriends_SetRichPresence(SteamAPI_SteamFriends(), "connect",
          strf("+platform:connect:steamid_{}", steamId).c_str()))
        return std::unexpected(ApplicationException("Steam rejected local rich presence"));

    } else if (std::holds_alternative<JoinRemote>(location)) {
      auto address = (HostAddressWithPort)std::get<JoinRemote>(location);
      if (auto logged = m_state->logger.info("Setting Steam rich presence connection as address_{}", address); !logged)
        return std::unexpected(std::move(logged.error()));
      if (!SteamAPI_ISteamFriends_SetRichPresence(SteamAPI_SteamFriends(), "connect",
          strf("+platform:connect:address_{}", address).c_str()))
        return std::unexpected(ApplicationException("Steam rejected remote rich presence"));
    }
  }
#endif
  m_joinLocation = location;

#ifdef STAR_ENABLE_DISCORD_INTEGRATION
  ++m_discordLobbyGeneration;
  if (m_state->discordAvailable && m_state->discordCurrentUser && m_discordLobbiesAvailable) {
    if (m_discordServerLobby) {
      auto lobby = std::exchange(m_discordServerLobby, {});
      auto res = m_state->discordCore->LobbyManager().DisconnectNetwork(lobby->first);
      if (res != discord::Result::Ok)
        recordDiscordCallbackError(*m_state, ApplicationException::format("Could not disconnect Discord lobby network {} (err {})", lobby->first, (int)res));
      if (lobby->second.empty())
        disconnectDiscordLobby(*m_state, lobby->first);
      else
        deleteDiscordLobby(*m_state, lobby->first);
      m_discordForceUpdateActivity = true;
    }

    if (createLobby) {
      m_state->discordCore->LobbyManager().CreateLobby(*createLobby,
        [lifetime = m_discordLifetime, &state = *m_state, generation = m_discordLobbyGeneration]
        (discord::Result res, discord::Lobby const& lobby) {
          auto service = lifetime->service;
          if (!service) {
            if (res == discord::Result::Ok)
              deleteDiscordLobby(state, lobby.GetId());
            return;
          }
          std::lock_guard serviceLocker(service->get().m_mutex);
          if (generation != service->get().m_discordLobbyGeneration) {
            if (res == discord::Result::Ok)
              deleteDiscordLobby(state, lobby.GetId());
            return;
          }
          if (res == discord::Result::InvalidCommand) {
            service->get().m_discordLobbiesAvailable = false;
            service->get().m_discordForceUpdateActivity = true;
            if (auto logged = state.logger.warn("Discord client does not support lobby networking; disabling Discord lobbies for this session (Rich Presence and other networking remain available)"); !logged)
              state.discordError = std::move(logged.error());
            return;
          }
          if (res != discord::Result::Ok) {
            recordDiscordCallbackError(state, ApplicationException::format("Failed to create Discord lobby (err {})", (int)res));
            return;
          }
          auto lobbyId = lobby.GetId();
          res = state.discordCore->LobbyManager().ConnectNetwork(lobbyId);
          if (res == discord::Result::Ok) {
            res = state.discordCore->LobbyManager().OpenNetworkChannel(lobbyId, DiscordMainNetworkChannel, true);
            if (res == discord::Result::Ok) {
              service->get().m_discordServerLobby = std::make_pair(lobbyId, String(std::string_view(lobby.GetSecret())));
              service->get().m_discordForceUpdateActivity = true;
              return;
            }
            recordDiscordCallbackError(state, ApplicationException::format("Failed to open Discord main network channel (err {})", (int)res));
            auto disconnectResult = state.discordCore->LobbyManager().DisconnectNetwork(lobbyId);
            if (disconnectResult != discord::Result::Ok)
              recordDiscordCallbackError(state, ApplicationException::format("Could not disconnect Discord lobby network {} (err {})", lobbyId, (int)disconnectResult));
          } else {
            recordDiscordCallbackError(state, ApplicationException::format("Failed to join Discord lobby network (err {})", (int)res));
          }
          deleteDiscordLobby(state, lobbyId);
        });
    }
  }
#endif
  return {};
}

}

module;

import std;
import star.data_stream_devices;
import star.format;
import star.host_address;
import star.map;
import star.random;
import star.string;
import star.thread;
import star.udp;
import star.universe_server;

export module star.server_query;

export namespace Star {

class ServerQueryThread : public Thread {
public:
  [[nodiscard]] static std::expected<std::unique_ptr<ServerQueryThread>, StarException> create(UniverseServer& universe, HostAddressWithPort const& bindAddress);
  ~ServerQueryThread();

  [[nodiscard]] std::expected<void, StarException> start();
  void stop();

protected:
  [[nodiscard]] std::expected<void, StarException> run() override;

private:
  ServerQueryThread(UniverseServer& universe, std::unique_ptr<UdpServer> server, std::uint8_t maxPlayers, String serverName);
  static constexpr std::uint8_t A2A_PING_REQUEST = 0x69;
  static constexpr std::uint8_t A2A_PING_REPLY = 0x6a;
  static constexpr std::uint8_t A2S_CHALLENGE_REQUEST = 0x57;
  static constexpr std::uint8_t A2S_CHALLENGE_RESPONSE = 0x41;
  static constexpr std::uint8_t A2S_INFO_REQUEST = 0x54;
  static constexpr std::uint8_t A2S_INFO_REPLY = 0x49;
  static constexpr std::uint8_t A2S_PLAYER_REQUEST = 0x55;
  static constexpr std::uint8_t A2S_PLAYER_REPLY = 0x44;
  static constexpr std::uint8_t A2S_RULES_REQUEST = 0x56;
  static constexpr std::uint8_t A2S_RULES_REPLY = 0x45;
  static constexpr std::uint8_t A2S_VERSION = 0x07;
  static constexpr std::uint8_t A2S_STR_TERM = 0x00;
  static constexpr std::uint8_t A2S_EDF_GID = 0x01;
  static constexpr std::uint8_t A2S_EDF_SID = 0x10;
  static constexpr std::uint8_t A2S_EDF_TAGS = 0x20;
  static constexpr std::uint8_t A2S_EDF_STV = 0x40;
  static constexpr std::uint8_t A2S_EDF_PORT = 0x80;
  static constexpr std::uint8_t A2S_ENV_WINDOWS = 'w';
  static constexpr std::uint8_t A2S_ENV_LINUX = 'l';
  static constexpr std::uint8_t A2S_ENV_MAC = 'm';
  static constexpr std::uint8_t A2S_TYPE_DEDICATED = 'd';
  static constexpr std::uint8_t A2S_TYPE_LISTEN = 'l';
  static constexpr std::uint8_t A2S_TYPE_TV = 'p';
  static constexpr std::uint8_t A2S_VAC_OFF = 0x00;
  static constexpr std::uint8_t A2S_VAC_ON = 0x01;
  static constexpr std::string_view A2S_INFO_REQUEST_STRING = "Source Engine Query";
  static constexpr std::uint16_t A2S_APPID = (std::uint16_t)0xfffe;
  static constexpr std::uint16_t A2S_PACKET_SIZE = (std::uint16_t)0x4e0;
  static constexpr std::uint32_t A2S_HEAD_INT = 0xffffffff;
  static constexpr std::string_view GAME_DIR = "starbound";
  static constexpr std::string_view GAME_DESC = "Starbound";
  static constexpr std::string_view GAME_TYPE = "SMP";
  static constexpr std::chrono::milliseconds challengeCheckInterval{30000};
  static constexpr std::chrono::milliseconds responseCacheTime{5000};

  [[nodiscard]] std::expected<void, StarException> sendTo(HostAddressWithPort const& address, DataStreamBuffer const& ds);
  [[nodiscard]] std::expected<bool, StarException> processPacket(HostAddressWithPort const& address, std::span<std::uint8_t const> data);
  [[nodiscard]] std::expected<void, StarException> buildPlayerResponse();
  [[nodiscard]] std::expected<void, StarException> buildRuleResponse();
  bool validChallenge(HostAddressWithPort const& address, std::span<std::uint8_t const> data);
  [[nodiscard]] std::expected<void, StarException> sendChallenge(HostAddressWithPort const& address);
  void pruneChallenges();
  [[nodiscard]] std::expected<bool, StarException> challengeRequest(HostAddressWithPort const& address, std::span<std::uint8_t const> data);

  // Server API
  std::uint8_t serverPlayerCount();
  bool serverPassworded();
  std::string_view serverPlugins();
  [[nodiscard]] std::expected<String, StarException> serverWorldNames();

  std::reference_wrapper<UniverseServer> m_universe;
  std::unique_ptr<UdpServer> m_queryServer;
  bool m_stop;
  DataStreamBuffer m_playersResponse;
  DataStreamBuffer m_rulesResponse;
  DataStreamBuffer m_generalResponse;

  class RequestChallenge {
  public:
    explicit RequestChallenge(RandomService& random);
    bool before(std::chrono::time_point<std::chrono::steady_clock, std::chrono::duration<std::uint64_t, std::milli>> time);
    std::int32_t getChallenge();

  private:
    std::chrono::time_point<std::chrono::steady_clock, std::chrono::duration<std::uint64_t, std::milli>> m_time;
    std::int32_t m_challenge;
  };

  std::uint16_t m_serverPort;
  std::uint8_t m_maxPlayers;
  String m_serverName;
  HashMap<HostAddress, std::shared_ptr<RequestChallenge>> m_validChallenges;
  std::chrono::time_point<std::chrono::steady_clock, std::chrono::milliseconds> m_lastChallengeCheck;
  std::chrono::time_point<std::chrono::steady_clock, std::chrono::milliseconds> m_lastPlayersResponse;
  std::chrono::time_point<std::chrono::steady_clock, std::chrono::milliseconds> m_lastRulesResponse;
  std::chrono::time_point<std::chrono::steady_clock, std::chrono::milliseconds> m_lastActiveTime;
};

}


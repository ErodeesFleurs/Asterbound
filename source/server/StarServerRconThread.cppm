module;

import std;
import star.data_stream_devices;
import star.format;
import star.host_address;
import star.game_types;
import star.map;
import star.rpc_promise;
import star.string;
import star.tcp;
import star.thread;
import star.universe_server;

export module star.server_rcon;

export namespace Star {

class ServerRconClient : public Thread {
public:
  static const std::uint32_t SERVERDATA_AUTH = 0x03;
  static const std::uint32_t SERVERDATA_EXECCOMMAND = 0x02;
  static const std::uint32_t SERVERDATA_RESPONSE_VALUE = 0x00;
  static const std::uint32_t SERVERDATA_AUTH_RESPONSE = 0x02;
  static const std::uint32_t SERVERDATA_AUTH_FAILURE = 0xffffffff;
  [[nodiscard]] static std::expected<std::shared_ptr<ServerRconClient>, StarException> create(UniverseServer& universe, std::shared_ptr<TcpSocket> socket);
  ~ServerRconClient() override;

  [[nodiscard]] std::expected<void, StarException> start();
  void stop();

protected:
  [[nodiscard]] std::expected<void, StarException> run() override;

private:
  ServerRconClient(UniverseServer& universe, std::shared_ptr<TcpSocket> socket, String password);
  static constexpr std::size_t MaxPacketSize = 4096;
  // Requests are small (a command line); anything larger is refused instead of
  // being allocated, since the length is client supplied.
  static constexpr std::size_t MaxReceivePacketSize = MaxPacketSize * 4;
  struct NoMoreRequestsTag {
    static constexpr std::string_view name() { return "NoMoreRequests"; }
  };
  using NoMoreRequests = StarError<NoMoreRequestsTag, StarException>;
  struct OversizedPacketTag {
    static constexpr std::string_view name() { return "OversizedPacket"; }
  };
  using OversizedPacket = StarError<OversizedPacketTag, StarException>;

  [[nodiscard]] std::expected<void, StarException> receive(std::size_t size);
  [[nodiscard]] std::expected<void, StarException> send(std::uint32_t requestId, std::uint32_t cmd, std::string_view str = {});
  [[nodiscard]] std::expected<void, StarException> sendAuthFailure();
  [[nodiscard]] std::expected<void, StarException> sendCmdResponse(std::uint32_t requestId, String const& response);
  [[nodiscard]] std::expected<void, StarException> processRequest();
  [[nodiscard]] std::expected<ServerCommandResult, StarException> handleCommand(String commandLine);

  std::reference_wrapper<UniverseServer> m_universe;
  std::shared_ptr<TcpSocket> m_socket;
  DataStreamBuffer m_packetBuffer;
  std::stop_source m_stopSource{std::nostopstate};
  bool m_authed;
  String m_rconPassword;

  HashMap<std::uint32_t,RpcPromise<String>> m_commandPromises;
};

class ServerRconThread : public Thread {
public:
  [[nodiscard]] static std::expected<std::unique_ptr<ServerRconThread>, StarException> create(UniverseServer& universe, HostAddressWithPort const& address);
  ~ServerRconThread() override;

  [[nodiscard]] std::expected<void, StarException> start();
  [[nodiscard]] std::expected<void, StarException> stop();

protected:
  [[nodiscard]] std::expected<void, StarException> run() override;

private:
  ServerRconThread(UniverseServer& universe, std::unique_ptr<TcpServer> server);
  void clearClients(bool all = false);

  std::reference_wrapper<UniverseServer> m_universe;
  std::unique_ptr<TcpServer> m_rconServer;
  std::stop_source m_stopSource{std::nostopstate};
  std::mutex m_clientsMutex;
  HashMap<HostAddress, std::shared_ptr<ServerRconClient>> m_clients;
};

}


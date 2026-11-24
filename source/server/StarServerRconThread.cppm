module;

import std;
import star.data_stream_devices;
import star.format;
import star.host_address;
import star.game_types;
import star.message_types;
import star.map;
import star.rpc_promise;
import star.string;
import star.tcp;
import star.thread;
import star.universe_server;

export module star.server_rcon;

export namespace Star {

class ServerRconClient : public Thread {
  struct ConstructionKey { explicit ConstructionKey() = default; };
public:
  ServerRconClient(ConstructionKey, UniverseServer& universe, std::shared_ptr<TcpSocket> socket, String password,
      std::chrono::milliseconds timeout);
  [[nodiscard]] static std::expected<std::shared_ptr<ServerRconClient>, StarException> create(UniverseServer& universe,
      std::shared_ptr<TcpSocket> socket, unsigned timeout_ms);
  ~ServerRconClient() override;

  [[nodiscard]] std::expected<void, StarException> start();
  void stop();

protected:
  [[nodiscard]] std::expected<void, StarException> run() override;

private:
  static constexpr std::uint32_t serverdata_auth = 3;
  static constexpr std::uint32_t serverdata_exec_command = 2;
  static constexpr std::uint32_t serverdata_response_value = 0;
  static constexpr std::uint32_t serverdata_auth_response = 2;
  static constexpr std::uint32_t serverdata_auth_failure = 0xffffffff;
  static constexpr std::size_t max_packet_size = 4096;
  // Requests are small (a command line); anything larger is refused instead of
  // being allocated, since the length is client supplied.
  static constexpr std::size_t max_receive_packet_size = max_packet_size * 4;
  struct NoMoreRequestsTag {
    static constexpr std::string_view name() { return "NoMoreRequests"; }
  };
  using NoMoreRequests = StarError<NoMoreRequestsTag, StarException>;
  struct OversizedPacketTag {
    static constexpr std::string_view name() { return "OversizedPacket"; }
  };
  using OversizedPacket = StarError<OversizedPacketTag, StarException>;

  std::chrono::steady_clock::time_point io_deadline() const;
  [[nodiscard]] std::expected<void, StarException> receive(std::size_t size);
  [[nodiscard]] std::expected<void, StarException> send(std::uint32_t request_id, std::uint32_t cmd, std::string_view str = {});
  [[nodiscard]] std::expected<void, StarException> send_auth_failure();
  [[nodiscard]] std::expected<void, StarException> send_command_response(std::uint32_t request_id, String const& response);
  [[nodiscard]] std::expected<void, StarException> process_request();
  [[nodiscard]] std::expected<void, StarException> flush_replies();
  [[nodiscard]] std::expected<ServerCommandResult, StarException> handle_command(String command_line);

  std::reference_wrapper<UniverseServer> m_universe;
  std::shared_ptr<TcpSocket> m_socket;
  DataStreamBuffer m_request;
  DataStreamBuffer m_response;
  std::chrono::milliseconds m_timeout;
  std::stop_source m_stop_source{std::nostopstate};
  bool m_authed;
  String m_rcon_password;

  HashMap<std::uint32_t, RpcPromise<String>> m_command_promises;
};

class ServerRconThread : public Thread {
  struct ConstructionKey { explicit ConstructionKey() = default; };
public:
  [[nodiscard]] static std::expected<std::unique_ptr<ServerRconThread>, StarException> create(UniverseServer& universe, HostAddressWithPort const& address);
  ServerRconThread(ConstructionKey, UniverseServer& universe, std::unique_ptr<TcpServer> server);
  ~ServerRconThread() override;

  [[nodiscard]] std::expected<void, StarException> start();
  [[nodiscard]] std::expected<void, StarException> stop();

protected:
  [[nodiscard]] std::expected<void, StarException> run() override;

private:
  void clear_clients(bool all = false);

  std::reference_wrapper<UniverseServer> m_universe;
  std::unique_ptr<TcpServer> m_rcon_server;
  std::stop_source m_stop_source{std::nostopstate};
  std::mutex m_clients_mutex;
  HashMap<HostAddress, std::shared_ptr<ServerRconClient>> m_clients;
};

}


module;
import std;
import star.format;
import star.io_device;
import star.string;
import star.either;
import star.host_address;
import star.thread;
import star.socket;

namespace Star {

class Logger;

class TcpSocket : public Socket {
public:
  [[nodiscard]] static std::expected<std::shared_ptr<TcpSocket>, StarException> connectTo(HostAddressWithPort const& address);
  [[nodiscard]] static std::expected<std::shared_ptr<TcpSocket>, StarException> listen(Logger& logger, HostAddressWithPort const& address);

  [[nodiscard]] std::expected<std::shared_ptr<TcpSocket>, StarException> accept(Logger& logger);

  // Must be called after connect.  Sets TCP_NODELAY option.
  [[nodiscard]] std::expected<void, StarException> setNoDelay(bool noDelay);

  [[nodiscard]] std::expected<std::size_t, StarException> receive(std::span<char> data);
  [[nodiscard]] std::expected<std::size_t, StarException> send(std::span<char const> data);

  HostAddressWithPort localAddress() const;
  HostAddressWithPort remoteAddress() const;

private:
  TcpSocket(NetworkMode networkMode);
  TcpSocket(NetworkMode networkMode, std::shared_ptr<SocketImpl> impl);

  [[nodiscard]] std::expected<void, StarException> connect(HostAddressWithPort const& address);

  HostAddressWithPort m_remoteAddress;
};

// Simple class to listen for and open TcpSocket instances.
class TcpServer {
public:
  using AcceptCallback = std::function<std::expected<void, StarException>(std::shared_ptr<TcpSocket> socket)>;

  [[nodiscard]] static std::expected<std::unique_ptr<TcpServer>, StarException> create(Logger& logger, HostAddressWithPort const& address);
  [[nodiscard]] static std::expected<std::unique_ptr<TcpServer>, StarException> create(Logger& logger, std::uint16_t port);
  ~TcpServer();

  [[nodiscard]] std::expected<void, StarException> stop();
  bool isListening() const;

  // True only after the listener is closed and the callback thread is joined.
  // A failed stop() may still satisfy this when it reports a callback error.
  bool isClosed() const;

  // Blocks until a connection or timeout; a closed listener yields a null socket.
  [[nodiscard]] std::expected<std::shared_ptr<TcpSocket>, StarException> accept(unsigned timeout);

  // Callback failures are logged and close the listener.
  [[nodiscard]] std::expected<void, StarException> setAcceptCallback(AcceptCallback callback, unsigned timeout = 20);

private:
  TcpServer(Logger& logger, HostAddressWithPort const& address);
  Logger& m_logger;
  mutable std::mutex m_mutex;

  AcceptCallback m_callback;
  ThreadFunction<void> m_callbackThread;
  HostAddressWithPort m_hostAddress;
  std::shared_ptr<TcpSocket> m_listenSocket;
};

}

export module star.tcp;

export namespace Star {
  using ::Star::TcpSocket;
  using ::Star::TcpServer;
}

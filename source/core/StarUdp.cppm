module;
#ifdef STAR_SYSTEM_FAMILY_WINDOWS
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/types.h>
#include <sys/socket.h>
#endif

import std;
import star.format;
import star.string;
import star.host_address;
import star.thread;
import star.socket;
import star.logging;
import star.net_impl;

export module star.udp;

export namespace Star {

// Conservative default payload size for an unfragmented UDP datagram.
inline constexpr unsigned max_udp_data = 1460;

class UdpSocket : public Socket {
  struct ConstructionKey { explicit ConstructionKey() = default; };

public:
  UdpSocket(ConstructionKey, NetworkMode network_mode);
  [[nodiscard]] static std::expected<std::shared_ptr<UdpSocket>, StarException> create(NetworkMode network_mode);

  [[nodiscard]] std::expected<std::size_t, StarException> receive(std::optional<std::reference_wrapper<HostAddressWithPort>> address, std::span<char> data);
  [[nodiscard]] std::expected<std::size_t, StarException> send(HostAddressWithPort const& address, std::span<char const> data);
};

class UdpServer {
  struct ConstructionKey { explicit ConstructionKey() = default; };

public:
  UdpServer(ConstructionKey, std::shared_ptr<UdpSocket> socket);
  [[nodiscard]] static std::expected<std::unique_ptr<UdpServer>, StarException> create(Logger& logger, HostAddressWithPort const& address);
  ~UdpServer();

  void close();
  bool is_listening() const;

  [[nodiscard]] std::expected<std::size_t, StarException> receive(std::optional<std::reference_wrapper<HostAddressWithPort>> address, std::span<char> data, unsigned timeout);
  [[nodiscard]] std::expected<std::size_t, StarException> send(HostAddressWithPort const& address, std::span<char const> data);

private:
  std::shared_ptr<UdpSocket> const m_listen_socket;
};

}

namespace Star {

UdpSocket::UdpSocket(ConstructionKey, NetworkMode network_mode) : Socket(network_mode) {}

[[nodiscard]] std::expected<std::shared_ptr<UdpSocket>, StarException> UdpSocket::create(NetworkMode network_mode) {
  auto socket = std::make_shared<UdpSocket>(ConstructionKey{}, network_mode);
  if (auto status = socket->initialize(SocketType::Udp); !status)
    return std::unexpected(std::move(status.error()));
  return socket;
}

[[nodiscard]] std::expected<std::size_t, StarException> UdpSocket::receive(std::optional<std::reference_wrapper<HostAddressWithPort>> address, std::span<char> data) {
  std::shared_lock locker(m_mutex);
  if (auto status = check_open("UdpSocket::receive"); !status)
    return std::unexpected(std::move(status.error()));

  sockaddr_storage storage;
  socklen_t length = sizeof(storage);
  auto received = ::recvfrom(m_impl->socket_desc, data.data(), data.size(), 0,
      address ? reinterpret_cast<sockaddr*>(&storage) : nullptr, address ? &length : nullptr);

  if (received < 0) {
    int error = last_socket_error();
    if (!is_active())
      return std::unexpected(SocketClosedException("Connection closed"));
    if (is_interrupted(error) || would_block(error))
      return 0;
    return std::unexpected(NetworkException::format("udp recv error: {}", socket_error_message(error)));
  }

  if (address) {
    auto peer = address_from_native(storage, length, native_address_family(m_network_mode));
    if (!peer)
      return std::unexpected(std::move(peer.error()));
    address->get() = std::move(*peer);
  }

  return received;
}

[[nodiscard]] std::expected<std::size_t, StarException> UdpSocket::send(HostAddressWithPort const& address, std::span<char const> data) {
  std::shared_lock locker(m_mutex);
  if (auto status = check_open("UdpSocket::send"); !status)
    return std::unexpected(std::move(status.error()));

  sockaddr_storage storage;
  auto length = native_from_address(address, storage);
  auto sent = ::sendto(m_impl->socket_desc, data.data(), data.size(), 0,
      reinterpret_cast<sockaddr const*>(&storage), length);
  if (sent < 0) {
    int error = last_socket_error();
    if (!is_active())
      return std::unexpected(SocketClosedException("Connection closed"));
    if (is_interrupted(error) || would_block(error))
      return 0;
    return std::unexpected(NetworkException::format("udp send error: {}", socket_error_message(error)));
  }

  return sent;
}

UdpServer::UdpServer(ConstructionKey, std::shared_ptr<UdpSocket> socket) : m_listen_socket(std::move(socket)) {}

[[nodiscard]] std::expected<std::unique_ptr<UdpServer>, StarException> UdpServer::create(Logger& logger, HostAddressWithPort const& address) {
  auto socket = UdpSocket::create(address.address().mode());
  if (!socket)
    return std::unexpected(std::move(socket.error()));
  if (auto status = (*socket)->set_non_blocking(true); !status)
    return std::unexpected(std::move(status.error()));
  if (auto status = (*socket)->bind(logger, address); !status)
    return std::unexpected(std::move(status.error()));
  if (auto status = logger.debug("UdpServer listening on: {}", address); !status)
    return std::unexpected(std::move(status.error()));
  return std::make_unique<UdpServer>(ConstructionKey{}, std::move(*socket));
}

UdpServer::~UdpServer() {
  close();
}

[[nodiscard]] std::expected<std::size_t, StarException> UdpServer::receive(std::optional<std::reference_wrapper<HostAddressWithPort>> address, std::span<char> data, unsigned timeout) {
  auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout);
  if (auto status = m_listen_socket->wait_for_data(false, deadline); !status)
    return std::unexpected(std::move(status.error()));
  return m_listen_socket->receive(address, data);
}

[[nodiscard]] std::expected<std::size_t, StarException> UdpServer::send(HostAddressWithPort const& address, std::span<char const> data) {
  return m_listen_socket->send(address, data);
}

void UdpServer::close() {
  m_listen_socket->close();
}

bool UdpServer::is_listening() const {
  return m_listen_socket->is_active();
}

}

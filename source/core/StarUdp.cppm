module;
#ifdef STAR_SYSTEM_FAMILY_WINDOWS
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#else
#ifdef STAR_SYSTEM_FREEBSD
#include <sys/types.h>
#include <sys/socket.h>
#endif
#include <errno.h>
#include <string.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/udp.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#endif

import std;
import star.format;
import star.string;
import star.either;
import star.host_address;
import star.thread;
import star.socket;
import star.logging;
import star.net_impl;

export module star.udp;

export namespace Star {

// A Good default assumption for a maximum size of a UDP datagram without
// fragmentation
inline constexpr unsigned MaxUdpData = 1460;

class UdpSocket : public Socket {
public:
  [[nodiscard]] static std::expected<std::shared_ptr<UdpSocket>, StarException> create(NetworkMode networkMode);

  [[nodiscard]] std::expected<std::size_t, StarException> receive(std::optional<std::reference_wrapper<HostAddressWithPort>> address, std::span<char> data);
  [[nodiscard]] std::expected<std::size_t, StarException> send(HostAddressWithPort const& address, std::span<char const> data);
private:
  explicit UdpSocket(NetworkMode networkMode);
};

class UdpServer {
public:
  [[nodiscard]] static std::expected<std::unique_ptr<UdpServer>, StarException> create(Logger& logger, HostAddressWithPort const& address);
  ~UdpServer();

  void close();
  bool isListening() const;

  [[nodiscard]] std::expected<std::size_t, StarException> receive(std::optional<std::reference_wrapper<HostAddressWithPort>> address, std::span<char> data, unsigned timeout);
  [[nodiscard]] std::expected<std::size_t, StarException> send(HostAddressWithPort const& address, std::span<char const> data);

private:
  explicit UdpServer(HostAddressWithPort const& address);
  HostAddressWithPort const m_hostAddress;
  std::shared_ptr<UdpSocket> m_listenSocket;
};

}

namespace Star {

UdpSocket::UdpSocket(NetworkMode networkMode) : Socket(networkMode) {}

[[nodiscard]] std::expected<std::shared_ptr<UdpSocket>, StarException> UdpSocket::create(NetworkMode networkMode) {
  auto socket = std::shared_ptr<UdpSocket>(new UdpSocket(networkMode));
  if (auto status = socket->initialize(SocketType::Udp); !status)
    return std::unexpected(std::move(status.error()));
  return socket;
}

[[nodiscard]] std::expected<std::size_t, StarException> UdpSocket::receive(std::optional<std::reference_wrapper<HostAddressWithPort>> address, std::span<char> data) {
  std::shared_lock locker(m_mutex);
  if (auto status = checkOpen("UdpSocket::receive"); !status)
    return std::unexpected(std::move(status.error()));

  int flags = 0;
  int len;
  struct sockaddr_storage sockAddr;
  socklen_t sockAddrLen = sizeof(sockAddr);

  len = ::recvfrom(m_impl->socketDesc, data.data(), data.size(), flags, (struct sockaddr*)&sockAddr, &sockAddrLen);

  if (len < 0) {
    if (!isActive())
      return std::unexpected(SocketClosedException("Connection closed"));
    else if (netErrorInterrupt())
      return 0;
    else
      return std::unexpected(NetworkException(Star::format("udp recv error: {}", netErrorString())));
  }

  if (address) {
    if (auto status = setAddressFromNative(address->get(), m_localAddress.address().mode(), &sockAddr); !status)
      return std::unexpected(std::move(status.error()));
  }

  return len;
}

[[nodiscard]] std::expected<std::size_t, StarException> UdpSocket::send(HostAddressWithPort const& address, std::span<char const> data) {
  std::shared_lock locker(m_mutex);
  if (auto status = checkOpen("UdpSocket::send"); !status)
    return std::unexpected(std::move(status.error()));

  struct sockaddr_storage sockAddr;
  socklen_t sockAddrLen;
  if (auto status = setNativeFromAddress(address, &sockAddr, &sockAddrLen); !status)
    return std::unexpected(std::move(status.error()));

  int len = ::sendto(m_impl->socketDesc, data.data(), data.size(), 0, (struct sockaddr*)&sockAddr, sockAddrLen);
  if (len < 0) {
    if (!isActive())
      return std::unexpected(SocketClosedException("Connection closed"));
    else if (netErrorInterrupt())
      len = 0;
    else
      return std::unexpected(NetworkException(Star::format("udp send error: {}", netErrorString())));
  }

  return len;
}

UdpServer::UdpServer(HostAddressWithPort const& address) : m_hostAddress(address) {}

[[nodiscard]] std::expected<std::unique_ptr<UdpServer>, StarException> UdpServer::create(Logger& logger, HostAddressWithPort const& address) {
  auto server = std::unique_ptr<UdpServer>(new UdpServer(address));
  auto socket = UdpSocket::create(address.address().mode());
  if (!socket)
    return std::unexpected(std::move(socket.error()));
  server->m_listenSocket = std::move(*socket);
  if (auto status = server->m_listenSocket->setNonBlocking(true); !status)
    return std::unexpected(std::move(status.error()));
  if (auto status = server->m_listenSocket->bind(logger, address); !status)
    return std::unexpected(std::move(status.error()));
  if (auto status = logger.debug("UdpServer listening on: {}", address); !status)
    return std::unexpected(std::move(status.error()));
  return server;
}

UdpServer::~UdpServer() {
  close();
}

[[nodiscard]] std::expected<std::size_t, StarException> UdpServer::receive(std::optional<std::reference_wrapper<HostAddressWithPort>> address, std::span<char> data, unsigned timeout) {
  if (auto status = Socket::poll({{m_listenSocket, {true, false}}}, timeout); !status)
    return std::unexpected(std::move(status.error()));
  return m_listenSocket->receive(address, data);
}

[[nodiscard]] std::expected<std::size_t, StarException> UdpServer::send(HostAddressWithPort const& address, std::span<char const> data) {
  return m_listenSocket->send(address, data);
}

void UdpServer::close() {
  if (m_listenSocket)
    m_listenSocket->close();
}

bool UdpServer::isListening() const {
  return m_listenSocket && m_listenSocket->isActive();
}

}

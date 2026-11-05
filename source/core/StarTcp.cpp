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
import star.output_proxy;
import star.io_device;
import star.string;
import star.either;
import star.host_address;
import star.thread;
import star.socket;
import star.tcp;
import star.logging;
import star.net_impl;

namespace Star {

std::expected<std::shared_ptr<TcpSocket>, StarException> TcpSocket::connectTo(HostAddressWithPort const& addressWithPort) {
  auto socket = std::shared_ptr<TcpSocket>(new TcpSocket(addressWithPort.address().mode()));
  if (auto status = socket->initialize(SocketType::Tcp); !status)
    return std::unexpected(std::move(status.error()));
  if (auto status = socket->connect(addressWithPort); !status)
    return std::unexpected(std::move(status.error()));
  return socket;
}

std::expected<std::shared_ptr<TcpSocket>, StarException> TcpSocket::listen(Logger& logger, HostAddressWithPort const& addressWithPort) {
  auto socket = std::shared_ptr<TcpSocket>(new TcpSocket(addressWithPort.address().mode()));
  if (auto status = socket->initialize(SocketType::Tcp); !status)
    return std::unexpected(std::move(status.error()));
  if (auto status = socket->bind(logger, addressWithPort); !status)
    return std::unexpected(std::move(status.error()));
  if (auto status = socket->Socket::listen(32); !status)
    return std::unexpected(std::move(status.error()));
  return socket;
}

std::expected<std::shared_ptr<TcpSocket>, StarException> TcpSocket::accept(Logger& logger) {
  std::shared_lock locker(m_mutex);

  if (m_socketMode != SocketMode::Bound)
    return std::unexpected(SocketClosedException("TcpSocket not bound in TcpSocket::accept"));

  struct sockaddr_storage sockAddr;
  socklen_t sockAddrLen = sizeof(sockAddr);

  auto socketDesc = ::accept(m_impl->socketDesc, (struct sockaddr*)&sockAddr, &sockAddrLen);

  if (invalidSocketDescriptor(socketDesc)) {
    if (netErrorInterrupt())
      return std::shared_ptr<TcpSocket>{};
    return std::unexpected(NetworkException(strf("Cannot accept connection: {}", netErrorString())));
  }

  auto socketImpl = std::make_shared<SocketImpl>();
  socketImpl->socketDesc = socketDesc;

#if defined STAR_SYSTEM_MACOS || defined STAR_SYSTEM_FREEBSD
  // Don't generate sigpipe
  int set = 1;
  if (auto status = socketImpl->setSockOpt(SOL_SOCKET, SO_NOSIGPIPE, (void*)&set, sizeof(int)); !status)
    return std::unexpected(std::move(status.error()));
#endif

  std::shared_ptr<TcpSocket> sockPtr(new TcpSocket(m_localAddress.address().mode(), socketImpl));
  if (auto status = sockPtr->configure(); !status)
    return std::unexpected(std::move(status.error()));

  sockPtr->m_localAddress = m_localAddress;
  if (auto status = setAddressFromNative(sockPtr->m_remoteAddress, m_localAddress.address().mode(), &sockAddr); !status)
    return std::unexpected(std::move(status.error()));
  if (auto status = logger.debug("accept from {} ({})", sockPtr->m_remoteAddress, sockPtr->m_impl->socketDesc); !status)
    return std::unexpected(std::move(status.error()));

  return sockPtr;
}

std::expected<void, StarException> TcpSocket::setNoDelay(bool noDelay) {
  std::shared_lock locker(m_mutex);
  if (auto status = checkOpen("TcpSocket::setNoDelay"); !status)
    return std::unexpected(std::move(status.error()));

  int flag = noDelay ? 1 : 0;
  return m_impl->setSockOpt(IPPROTO_TCP, TCP_NODELAY, (char*)&flag, sizeof(flag));
}

std::expected<std::size_t, StarException> TcpSocket::receive(std::span<char> data) {
  std::shared_lock locker(m_mutex);
  if (auto status = checkOpen("TcpSocket::receive"); !status)
    return std::unexpected(std::move(status.error()));

  int flags = 0;
#ifdef STAR_SYSTEM_LINUX
  // Don't generate sigpipe
  flags |= MSG_NOSIGNAL;
#endif

  auto r = ::recv(m_impl->socketDesc, data.data(), data.size(), flags);
  if (r < 0) {
    if (m_socketMode == SocketMode::Shutdown) {
      return std::unexpected(SocketClosedException("Connection closed"));
    } else if (netErrorConnectionReset()) {
      doShutdown();
      return std::unexpected(SocketClosedException("Connection reset"));
    } else if (netErrorInterrupt()) {
      r = 0;
    } else {
      return std::unexpected(NetworkException(strf("tcp recv error: {}", netErrorString())));
    }
  } else if (r == 0 && !data.empty()) {
    doShutdown();
  }

  return r;
}

std::expected<std::size_t, StarException> TcpSocket::send(std::span<char const> data) {
  std::shared_lock locker(m_mutex);
  if (auto status = checkOpen("TcpSocket::send"); !status)
    return std::unexpected(std::move(status.error()));

  int flags = 0;
#ifdef STAR_SYSTEM_LINUX
  // Don't generate sigpipe
  flags |= MSG_NOSIGNAL;
#endif

  auto w = ::send(m_impl->socketDesc, data.data(), data.size(), flags);
  if (w < 0) {
    if (m_socketMode == SocketMode::Shutdown) {
      return std::unexpected(SocketClosedException("Connection closed"));
    } else if (netErrorConnectionReset()) {
      doShutdown();
      return std::unexpected(SocketClosedException("Connection reset"));
    } else if (netErrorInterrupt()) {
      w = 0;
    } else {
      return std::unexpected(NetworkException(strf("tcp send error: {}", netErrorString())));
    }
  }

  return w;
}

HostAddressWithPort TcpSocket::localAddress() const {
  std::shared_lock locker(m_mutex);
  return m_localAddress;
}

HostAddressWithPort TcpSocket::remoteAddress() const {
  std::shared_lock locker(m_mutex);
  return m_remoteAddress;
}

TcpSocket::TcpSocket(NetworkMode networkMode) : Socket(networkMode) {}

TcpSocket::TcpSocket(NetworkMode networkMode, std::shared_ptr<SocketImpl> impl) : Socket(networkMode, impl, SocketMode::Connected) {}

std::expected<void, StarException> TcpSocket::connect(HostAddressWithPort const& addressWithPort) {
  std::unique_lock locker(m_mutex);
  if (auto status = checkOpen("TcpSocket::connect"); !status)
    return std::unexpected(std::move(status.error()));

  if (m_networkMode != addressWithPort.address().mode())
    return std::unexpected(NetworkException("Socket address type mismatch between address and socket."));

  struct sockaddr_storage sockAddr;
  socklen_t sockAddrLen;
  if (auto status = setNativeFromAddress(addressWithPort, &sockAddr, &sockAddrLen); !status)
    return std::unexpected(std::move(status.error()));
  if (::connect(m_impl->socketDesc, (struct sockaddr*)&sockAddr, sockAddrLen) < 0)
    return std::unexpected(NetworkException(strf("cannot connect to {}: {}", addressWithPort, netErrorString())));

#if defined STAR_SYSTEM_MACOS || defined STAR_SYSTEM_FREEBSD
  // Don't generate sigpipe
  int set = 1;
  if (auto status = m_impl->setSockOpt(SOL_SOCKET, SO_NOSIGPIPE, (void*)&set, sizeof(set)); !status)
    return std::unexpected(std::move(status.error()));
#endif

  m_socketMode = SocketMode::Connected;
  m_remoteAddress = addressWithPort;
  return {};
}

TcpServer::TcpServer(Logger& logger, HostAddressWithPort const& address)
  : m_logger(logger), m_hostAddress(address) {}

std::expected<std::unique_ptr<TcpServer>, StarException> TcpServer::create(Logger& logger, HostAddressWithPort const& address) {
  auto server = std::unique_ptr<TcpServer>(new TcpServer(logger, address));
  auto socket = TcpSocket::listen(logger, address);
  if (!socket)
    return std::unexpected(std::move(socket.error()));
  server->m_listenSocket = std::move(*socket);
  if (auto status = server->m_listenSocket->setNonBlocking(true); !status)
    return std::unexpected(std::move(status.error()));
  if (auto status = logger.debug("TcpServer listening on: {}", address); !status)
    return std::unexpected(std::move(status.error()));
  return server;
}

std::expected<std::unique_ptr<TcpServer>, StarException> TcpServer::create(Logger& logger, std::uint16_t port) {
  return create(logger, HostAddressWithPort(HostAddress(), port));
}

TcpServer::~TcpServer() {
  if (auto status = stop(); !status) {
    if (auto logged = m_logger.error("TcpServer shutdown failed: {}", outputException(ExceptionDiagnostic(status.error()), true)); !logged)
      EmergencyLog::error("TcpServer shutdown diagnostic failed: {}", outputException(ExceptionDiagnostic(logged.error()), true));
  }
  if (!isClosed())
    fatalError("Cannot destroy a TCP server with an unjoined callback thread", false);
}

std::expected<void, StarException> TcpServer::stop() {
  if (m_listenSocket)
    m_listenSocket->shutdown();
  std::expected<void, StarException> status;
  // Keep the control block in place on native join failure so stop() can retry.
  if (!m_callbackThread.isFinished())
    status = m_callbackThread.finish();
  if (m_listenSocket)
    m_listenSocket->close();
  if (!status)
    return std::unexpected(std::move(status.error()));
  return {};
}

bool TcpServer::isListening() const {
  return m_listenSocket && m_listenSocket->isActive();
}

bool TcpServer::isClosed() const {
  return (!m_listenSocket || !m_listenSocket->isOpen()) && m_callbackThread.isFinished();
}

std::expected<std::shared_ptr<TcpSocket>, StarException> TcpServer::accept(unsigned timeout) {
  std::lock_guard locker(m_mutex);
  if (auto status = Socket::poll({{m_listenSocket, {true, false}}}, timeout); !status)
    return std::unexpected(std::move(status.error()));
  auto socket = m_listenSocket->accept(m_logger);
  if (!socket && socket.error().is<SocketClosedException>())
    return std::shared_ptr<TcpSocket>{};
  return socket;
}

std::expected<void, StarException> TcpServer::setAcceptCallback(AcceptCallback callback, unsigned timeout) {
  std::lock_guard locker(m_mutex);
  if (!callback)
    return std::unexpected(NetworkException("Cannot install an empty TCP accept callback"));
  m_callback = std::move(callback);
  if (m_listenSocket->isActive() && !m_callbackThread) {
    auto thread = Thread::invoke("TcpServer::acceptCallback", [this, timeout]() -> std::expected<void, StarException> {
      while (true) {
        auto connection = accept(timeout);
        if (!connection) {
          if (auto logged = m_logger.error("TcpServer caught error accepting connection {}", outputException(ExceptionDiagnostic(connection.error()), false)); !logged) {
            m_listenSocket->close();
            return std::unexpected(std::move(logged.error()));
          }
        } else if (*connection) {
          auto status = m_callback(*connection);
          if (!status) {
            auto logged = m_logger.error("TcpServer will close, listener callback failed: {}", outputException(ExceptionDiagnostic(status.error()), true));
            m_listenSocket->close();
            if (!logged)
              return std::unexpected(std::move(logged.error()));
            return {};
          }
        }
        if (!m_listenSocket->isActive())
          return {};
      }
    });
    if (!thread)
      return std::unexpected(std::move(thread.error()));
    m_callbackThread = std::move(*thread);
  }
  return {};
}

}

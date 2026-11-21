#ifdef STAR_SYSTEM_FAMILY_WINDOWS
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#endif

import std;
import star.format;
import star.output_proxy;
import star.io_device;
import star.string;
import star.host_address;
import star.thread;
import star.socket;
import star.tcp;
import star.logging;
import star.net_impl;

namespace Star {

std::expected<std::shared_ptr<TcpSocket>, StarException> TcpSocket::connect_to(HostAddressWithPort const& address) {
  auto socket = std::make_shared<TcpSocket>(ConstructionKey{}, address.address().mode());
  if (auto status = socket->initialize(SocketType::Tcp); !status)
    return std::unexpected(std::move(status.error()));
  if (auto status = socket->connect(address); !status)
    return std::unexpected(std::move(status.error()));
  return socket;
}

std::expected<std::shared_ptr<TcpSocket>, StarException> TcpSocket::listen(Logger& logger, HostAddressWithPort const& address) {
  auto socket = std::make_shared<TcpSocket>(ConstructionKey{}, address.address().mode());
  if (auto status = socket->initialize(SocketType::Tcp); !status)
    return std::unexpected(std::move(status.error()));
  if (auto status = socket->bind(logger, address); !status)
    return std::unexpected(std::move(status.error()));
  if (auto status = socket->Socket::listen(32); !status)
    return std::unexpected(std::move(status.error()));
  return socket;
}

std::expected<std::shared_ptr<TcpSocket>, StarException> TcpSocket::accept(Logger& logger) {
  std::shared_lock locker(m_mutex);

  if (m_socket_mode != SocketMode::Bound)
    return std::unexpected(SocketClosedException("TcpSocket not bound in TcpSocket::accept"));

  sockaddr_storage storage;
  socklen_t length = sizeof(storage);
  SocketHandle accepted(::accept(m_impl->socket_desc, reinterpret_cast<sockaddr*>(&storage), &length));

  if (invalid_socket_descriptor(accepted.get())) {
    int error = last_socket_error();
    if (is_interrupted(error) || would_block(error))
      return std::shared_ptr<TcpSocket>{};
    return std::unexpected(NetworkException::format("Cannot accept connection: {}", socket_error_message(error)));
  }

  auto remote = address_from_native(storage, length, native_address_family(m_network_mode));
  if (!remote)
    return std::unexpected(std::move(remote.error()));

  auto socket_impl = std::make_shared<SocketImpl>();
  socket_impl->socket_desc = accepted.release();
#if defined STAR_SYSTEM_MACOS || defined STAR_SYSTEM_FREEBSD
  // Don't generate sigpipe
  if (auto status = socket_impl->set_option(SOL_SOCKET, SO_NOSIGPIPE, 1); !status)
    return std::unexpected(std::move(status.error()));
#endif

  auto socket = std::make_shared<TcpSocket>(ConstructionKey{}, m_network_mode, std::move(socket_impl));
  if (auto status = socket->configure(); !status)
    return std::unexpected(std::move(status.error()));

  socket->m_local_address = m_local_address;
  socket->m_remote_address = std::move(*remote);
  if (auto status = logger.debug("accept from {} ({})", socket->m_remote_address, socket->m_impl->socket_desc); !status)
    return std::unexpected(std::move(status.error()));

  return socket;
}

std::expected<void, StarException> TcpSocket::set_no_delay(bool no_delay) {
  std::shared_lock locker(m_mutex);
  if (auto status = check_open("TcpSocket::set_no_delay"); !status)
    return std::unexpected(std::move(status.error()));

  return m_impl->set_option(IPPROTO_TCP, TCP_NODELAY, no_delay ? 1 : 0);
}

std::expected<std::size_t, StarException> TcpSocket::receive(std::span<char> data) {
  std::shared_lock locker(m_mutex);
  if (auto status = check_open("TcpSocket::receive"); !status)
    return std::unexpected(std::move(status.error()));

  int flags = 0;
#ifdef STAR_SYSTEM_LINUX
  // Don't generate sigpipe
  flags |= MSG_NOSIGNAL;
#endif

  auto received = ::recv(m_impl->socket_desc, data.data(), data.size(), flags);
  if (received < 0) {
    int error = last_socket_error();
    if (m_socket_mode == SocketMode::Shutdown) {
      return std::unexpected(SocketClosedException("Connection closed"));
    } else if (is_connection_reset(error)) {
      do_shutdown();
      return std::unexpected(SocketClosedException("Connection reset"));
    } else if (is_interrupted(error) || would_block(error)) {
      return 0;
    } else {
      return std::unexpected(NetworkException::format("tcp recv error: {}", socket_error_message(error)));
    }
  } else if (received == 0 && !data.empty()) {
    do_shutdown();
  }

  return received;
}

std::expected<std::size_t, StarException> TcpSocket::send(std::span<char const> data) {
  std::shared_lock locker(m_mutex);
  if (auto status = check_open("TcpSocket::send"); !status)
    return std::unexpected(std::move(status.error()));

  int flags = 0;
#ifdef STAR_SYSTEM_LINUX
  // Don't generate sigpipe
  flags |= MSG_NOSIGNAL;
#endif

  auto sent = ::send(m_impl->socket_desc, data.data(), data.size(), flags);
  if (sent < 0) {
    int error = last_socket_error();
    if (m_socket_mode == SocketMode::Shutdown) {
      return std::unexpected(SocketClosedException("Connection closed"));
    } else if (is_connection_reset(error)) {
      do_shutdown();
      return std::unexpected(SocketClosedException("Connection reset"));
    } else if (is_interrupted(error) || would_block(error)) {
      return 0;
    } else {
      return std::unexpected(NetworkException::format("tcp send error: {}", socket_error_message(error)));
    }
  }

  return sent;
}

HostAddressWithPort TcpSocket::local_address() const {
  std::shared_lock locker(m_mutex);
  return m_local_address;
}

HostAddressWithPort TcpSocket::remote_address() const {
  std::shared_lock locker(m_mutex);
  return m_remote_address;
}

TcpSocket::TcpSocket(ConstructionKey, NetworkMode network_mode) : Socket(network_mode) {}

TcpSocket::TcpSocket(ConstructionKey, NetworkMode network_mode, std::shared_ptr<SocketImpl> impl)
    : Socket(network_mode, std::move(impl), SocketMode::Connected) {}

std::expected<void, StarException> TcpSocket::connect(HostAddressWithPort const& address) {
  std::unique_lock locker(m_mutex);
  if (auto status = check_open("TcpSocket::connect"); !status)
    return std::unexpected(std::move(status.error()));

  if (m_network_mode != address.address().mode())
    return std::unexpected(NetworkException("Socket address type mismatch between address and socket."));

  sockaddr_storage storage;
  auto length = native_from_address(address, storage);
  if (::connect(m_impl->socket_desc, reinterpret_cast<sockaddr const*>(&storage), length) < 0) {
    int error = last_socket_error();
    return std::unexpected(NetworkException::format("cannot connect to {}: {}", address, socket_error_message(error)));
  }

#if defined STAR_SYSTEM_MACOS || defined STAR_SYSTEM_FREEBSD
  // Don't generate sigpipe
  if (auto status = m_impl->set_option(SOL_SOCKET, SO_NOSIGPIPE, 1); !status)
    return std::unexpected(std::move(status.error()));
#endif

  m_socket_mode = SocketMode::Connected;
  m_remote_address = address;
  return {};
}

TcpServer::TcpServer(ConstructionKey, Logger& logger, std::shared_ptr<TcpSocket> socket)
  : m_logger(logger), m_listen_socket(std::move(socket)) {}

std::expected<std::unique_ptr<TcpServer>, StarException> TcpServer::create(Logger& logger, HostAddressWithPort const& address) {
  auto socket = TcpSocket::listen(logger, address);
  if (!socket)
    return std::unexpected(std::move(socket.error()));
  if (auto status = (*socket)->set_non_blocking(true); !status)
    return std::unexpected(std::move(status.error()));
  if (auto status = logger.debug("TcpServer listening on: {}", address); !status)
    return std::unexpected(std::move(status.error()));
  return std::make_unique<TcpServer>(ConstructionKey{}, logger, std::move(*socket));
}

std::expected<std::unique_ptr<TcpServer>, StarException> TcpServer::create(Logger& logger, std::uint16_t port) {
  return create(logger, HostAddressWithPort(HostAddress(), port));
}

TcpServer::~TcpServer() {
  if (auto status = stop(); !status) {
    if (auto logged = m_logger.error("TcpServer shutdown failed: {}", Star::output_exception(ExceptionDiagnostic(status.error()), true)); !logged)
      EmergencyLog::error("TcpServer shutdown diagnostic failed: {}", Star::output_exception(ExceptionDiagnostic(logged.error()), true));
  }
  if (!is_closed())
    Star::fatal_error("Cannot destroy a TCP server with an unjoined callback thread", false);
}

std::expected<void, StarException> TcpServer::stop() {
  m_listen_socket->shutdown();
  std::expected<void, StarException> status;
  // Keep the control block in place on native join failure so stop() can retry.
  if (!m_callback_thread.is_finished())
    status = m_callback_thread.finish();
  m_listen_socket->close();
  if (!status)
    return std::unexpected(std::move(status.error()));
  return {};
}

bool TcpServer::is_listening() const {
  return m_listen_socket->is_active();
}

bool TcpServer::is_closed() const {
  return !m_listen_socket->is_open() && m_callback_thread.is_finished();
}

std::expected<std::shared_ptr<TcpSocket>, StarException> TcpServer::accept(unsigned timeout) {
  auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout);
  std::lock_guard locker(m_mutex);
  if (auto status = m_listen_socket->wait_for_data(false, deadline); !status)
    return std::unexpected(std::move(status.error()));
  auto socket = m_listen_socket->accept(m_logger);
  if (!socket && socket.error().is<SocketClosedException>())
    return std::shared_ptr<TcpSocket>{};
  return socket;
}

std::expected<void, StarException> TcpServer::set_accept_callback(AcceptCallback callback, unsigned timeout) {
  std::lock_guard locker(m_mutex);
  if (!callback)
    return std::unexpected(NetworkException("Cannot install an empty TCP accept callback"));
  m_callback = std::move(callback);
  if (m_listen_socket->is_active() && !m_callback_thread) {
    auto thread = Thread::invoke("TcpServer::accept_callback", [this, timeout]() -> std::expected<void, StarException> {
      while (true) {
        auto connection = accept(timeout);
        if (!connection) {
          if (auto logged = m_logger.error("TcpServer caught error accepting connection {}", Star::output_exception(ExceptionDiagnostic(connection.error()), false)); !logged) {
            m_listen_socket->close();
            return std::unexpected(std::move(logged.error()));
          }
        } else if (*connection) {
          auto status = m_callback(*connection);
          if (!status) {
            auto logged = m_logger.error("TcpServer will close, listener callback failed: {}", Star::output_exception(ExceptionDiagnostic(status.error()), true));
            m_listen_socket->close();
            if (!logged)
              return std::unexpected(std::move(logged.error()));
            return {};
          }
        }
        if (!m_listen_socket->is_active())
          return {};
      }
    });
    if (!thread)
      return std::unexpected(std::move(thread.error()));
    m_callback_thread = std::move(*thread);
  }
  return {};
}

}

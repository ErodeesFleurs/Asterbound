#ifdef STAR_SYSTEM_FAMILY_WINDOWS
#include <winsock2.h>
#else
#include <sys/socket.h>
#include <sys/time.h>
#include <fcntl.h>
#include <poll.h>
#endif

import std;
import star.format;
import star.string;
import star.host_address;
import star.thread;
import star.socket;
import star.logging;
import star.net_impl;

namespace Star {
namespace {

timeval to_timeval(unsigned milliseconds) {
  return {static_cast<decltype(timeval::tv_sec)>(milliseconds / 1000),
      static_cast<decltype(timeval::tv_usec)>((milliseconds % 1000) * 1000)};
}

// poll takes a signed int. Longer deadlines require multiple finite waits.
int remaining_wait_ms(std::chrono::steady_clock::time_point deadline) {
  auto now = std::chrono::steady_clock::now();
  if (deadline <= now)
    return 0;
  auto ms = std::chrono::ceil<std::chrono::milliseconds>(deadline - now).count();
  return static_cast<int>(std::min<std::int64_t>(ms, std::numeric_limits<int>::max()));
}

}

std::expected<std::optional<SocketPollResult>, StarException> Socket::poll(SocketPollQuery const& query, unsigned timeout) {
  if (query.empty())
    return std::nullopt;

  auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout);
  std::vector<std::shared_lock<ReadersWriterMutex>> locks;
  locks.reserve(query.size());
  SocketPollResult result;
  for (auto const& [socket, events] : query) {
    locks.emplace_back(socket->m_mutex);
    if (!socket->is_open())
      result[socket] = {false, false, true};
  }
  if (result.size() == query.size())
    return std::optional<SocketPollResult>{std::move(result)};
  if (!result.empty())
    deadline = std::chrono::steady_clock::now();

#ifdef STAR_SYSTEM_FAMILY_WINDOWS
  if (query.size() - result.size() > FD_SETSIZE)
    return std::unexpected(NetworkException("Too many sockets for select"));
  fd_set reads, writes, exceptions;
#else
  std::vector<pollfd> descriptors;
  descriptors.reserve(query.size());
  for (auto const& [socket, events] : query) {
    short mask = (events.readable ? POLLIN : 0) | (events.writable ? POLLOUT : 0);
    descriptors.push_back({socket->is_open() ? socket->m_impl->socket_desc : -1, mask, 0});
  }
#endif

  while (true) {
    int wait_ms = remaining_wait_ms(deadline);
#ifdef STAR_SYSTEM_FAMILY_WINDOWS
    // select overwrites the sets; rebuild them on a retry.
    FD_ZERO(&reads);
    FD_ZERO(&writes);
    FD_ZERO(&exceptions);
    for (auto const& [socket, events] : query) {
      if (!socket->is_open())
        continue;
      auto fd = socket->m_impl->socket_desc;
      if (events.readable)
        FD_SET(fd, &reads);
      if (events.writable)
        FD_SET(fd, &writes);
      FD_SET(fd, &exceptions);
    }
    auto time = to_timeval(static_cast<unsigned>(wait_ms));
    int ready = ::select(0, &reads, &writes, &exceptions, &time);
#else
    int ready = ::poll(descriptors.data(), descriptors.size(), wait_ms);
#endif
    if (ready > 0)
      break;
    if (ready < 0) {
      int error = last_socket_error();
      if (!is_interrupted(error) && !would_block(error))
        return std::unexpected(NetworkException::format("Error polling sockets: {}", socket_error_message(error)));
    }
    if (std::chrono::steady_clock::now() >= deadline)
      return result.empty() ? std::optional<SocketPollResult>{} : std::optional<SocketPollResult>{std::move(result)};
  }

  for (auto [index, entry] : std::views::enumerate(query)) {
    auto const& socket = entry.first;
    if (!socket->is_open())
      continue;
    auto& events = result[socket];
#ifdef STAR_SYSTEM_FAMILY_WINDOWS
    auto fd = socket->m_impl->socket_desc;
    events.readable = FD_ISSET(fd, &reads);
    events.writable = FD_ISSET(fd, &writes);
    events.exception = FD_ISSET(fd, &exceptions);
    if (events.exception)
#else
    auto flags = descriptors[static_cast<std::size_t>(index)].revents;
    events.readable = flags & POLLIN;
    events.writable = flags & POLLOUT;
    events.exception = flags & (POLLHUP | POLLNVAL | POLLERR);
    if (flags & POLLHUP)
#endif
      socket->do_shutdown();
  }
  return std::optional<SocketPollResult>{std::move(result)};
}

std::expected<void, StarException> Socket::wait_for_data(bool writable, std::chrono::steady_clock::time_point deadline) const {
  std::shared_lock locker(m_mutex);
  if (!is_active())
    return {};
  auto impl = m_impl;
#ifdef STAR_SYSTEM_FAMILY_WINDOWS
  auto wake_socket = impl->prepare_wait();
  if (!wake_socket)
    return std::unexpected(std::move(wake_socket.error()));
#endif
  locker.unlock();
  // Shutdown may race wake-channel initialization before native waiting begins.
  if (!is_active())
    return {};

  while (true) {
    int timeout = remaining_wait_ms(deadline);
#ifdef STAR_SYSTEM_FAMILY_WINDOWS
    fd_set reads;
    fd_set writes;
    fd_set exceptions;
    FD_ZERO(&reads);
    FD_ZERO(&writes);
    FD_ZERO(&exceptions);
    FD_SET(*wake_socket, &reads);
    if (writable)
      FD_SET(impl->socket_desc, &writes);
    else
      FD_SET(impl->socket_desc, &reads);
    FD_SET(impl->socket_desc, &exceptions);
    auto time = to_timeval(static_cast<unsigned>(timeout));
    int result = ::select(0, &reads, &writes, &exceptions, &time);
#else
    pollfd descriptor{};
    descriptor.fd = impl->socket_desc;
    descriptor.events = writable ? POLLOUT : POLLIN;
    int result = ::poll(&descriptor, 1, timeout);
#endif
    if (result > 0)
      return {};
    if (result < 0) {
      int error = last_socket_error();
      if (!is_interrupted(error) && !would_block(error))
        return std::unexpected(NetworkException::format("Error waiting for socket readiness: {}", socket_error_message(error)));
    }
    if (!is_active() || std::chrono::steady_clock::now() >= deadline)
      return {};
  }
}

Socket::~Socket() {
  close();
}

std::expected<void, StarException> Socket::bind(Logger& logger, HostAddressWithPort const& address_with_port) {
  std::unique_lock locker(m_mutex);
  if (auto status = check_open("Socket::bind"); !status)
    return std::unexpected(std::move(status.error()));

  if (address_with_port.address().mode() != m_network_mode)
    return std::unexpected(NetworkException("Bind address does not match socket mode"));

  // Ensure quick restarts don't prevent us binding
  if (auto status = m_impl->set_option(SOL_SOCKET, SO_REUSEADDR, 1); !status)
    return std::unexpected(std::move(status.error()));

  sockaddr_storage storage;
  auto length = native_from_address(address_with_port, storage);
  if (::bind(m_impl->socket_desc, reinterpret_cast<sockaddr const*>(&storage), length) < 0)
    return std::unexpected(NetworkException(Star::format("Cannot bind socket to {}: {}", address_with_port, socket_error_message(last_socket_error()))));
  if (address_with_port.port() == 0) {
    length = sizeof(storage);
    if (::getsockname(m_impl->socket_desc, reinterpret_cast<sockaddr*>(&storage), &length) != 0)
      return std::unexpected(NetworkException::format("Cannot query bound socket address: {}", socket_error_message(last_socket_error())));
    auto local = address_from_native(storage, length, native_address_family(m_network_mode));
    if (!local)
      return std::unexpected(std::move(local.error()));
    m_local_address = std::move(*local);
  } else {
    m_local_address = address_with_port;
  }

  m_socket_mode = SocketMode::Bound;

  return logger.debug("bind {} ({})", address_with_port, m_impl->socket_desc);
}

std::expected<void, StarException> Socket::listen(int backlog) {
  std::unique_lock locker(m_mutex);
  if (auto status = check_open("Socket::listen"); !status)
    return std::unexpected(std::move(status.error()));

  if (::listen(m_impl->socket_desc, backlog) != 0)
    return std::unexpected(NetworkException(Star::format("Could not listen on socket: '{}'", socket_error_message(last_socket_error()))));
  return {};
}

std::expected<void, StarException> Socket::set_timeout(unsigned timeout) {
  std::shared_lock locker(m_mutex);
  if (auto status = check_open("Socket::set_timeout"); !status)
    return std::unexpected(std::move(status.error()));

#ifdef STAR_SYSTEM_FAMILY_WINDOWS
  DWORD value = timeout;
#else
  auto value = to_timeval(timeout);
#endif
  if (auto status = m_impl->set_option(SOL_SOCKET, SO_RCVTIMEO, value); !status)
    return std::unexpected(std::move(status.error()));
  return m_impl->set_option(SOL_SOCKET, SO_SNDTIMEO, value);
}

std::expected<void, StarException> Socket::set_non_blocking(bool non_blocking) {
  std::shared_lock locker(m_mutex);
  if (auto status = check_open("Socket::set_non_blocking"); !status)
    return std::unexpected(std::move(status.error()));
#ifdef STAR_SYSTEM_FAMILY_WINDOWS
  unsigned long mode = non_blocking ? 1 : 0;
  if (ioctlsocket(m_impl->socket_desc, FIONBIO, &mode) != 0)
    return std::unexpected(NetworkException::format("Cannot set socket non-blocking mode: {}", socket_error_message(last_socket_error())));
#else
  int flags = fcntl(m_impl->socket_desc, F_GETFL, 0);
  if (flags < 0)
    return std::unexpected(NetworkException::format("fcntl failure getting socket flags: {}", socket_error_message(last_socket_error())));
  flags = non_blocking ? (flags | O_NONBLOCK) : (flags & ~O_NONBLOCK);
  if (fcntl(m_impl->socket_desc, F_SETFL, flags) != 0)
    return std::unexpected(NetworkException::format("fcntl failure setting non-blocking mode: {}", socket_error_message(last_socket_error())));
#endif
  return {};
}

NetworkMode Socket::network_mode() const {
  std::shared_lock locker(m_mutex);
  return m_network_mode;
}

SocketMode Socket::socket_mode() const {
  std::shared_lock locker(m_mutex);
  return m_socket_mode;
}

bool Socket::is_active() const {
  return m_socket_mode > SocketMode::Shutdown;
}

bool Socket::is_open() const {
  return m_socket_mode != SocketMode::Closed;
}

void Socket::shutdown() {
  std::shared_lock locker(m_mutex);
  do_shutdown();
}

void Socket::close() {
  {
    // Blocking send/recv retain shared ownership. Wake them before waiting for
    // exclusive ownership to release the descriptor.
    std::shared_lock locker(m_mutex);
    do_shutdown();
  }
  std::unique_lock locker(m_mutex);
  do_close();
}

Socket::Socket(NetworkMode network_mode)
  : m_network_mode(network_mode), m_socket_mode(SocketMode::Closed) {}

std::expected<void, StarException> Socket::initialize(SocketType type) {
  m_impl = std::make_shared<SocketImpl>();
  int family = native_address_family(m_network_mode);
  if (family == AF_UNSPEC)
    return std::unexpected(NetworkException("Invalid socket network mode"));
  m_impl->socket_desc = ::socket(family,
      type == SocketType::Tcp ? SOCK_STREAM : SOCK_DGRAM, 0);
  if (invalid_socket_descriptor(m_impl->socket_desc))
    return std::unexpected(NetworkException::format("cannot create socket: {}", socket_error_message(last_socket_error())));
  m_socket_mode = SocketMode::Shutdown;
  return configure();
}

Socket::Socket(NetworkMode network_mode, std::shared_ptr<SocketImpl> impl, SocketMode socket_mode)
  : m_network_mode(network_mode), m_impl(std::move(impl)), m_socket_mode(socket_mode) {}

std::expected<void, StarException> Socket::configure() {
  if (auto status = set_timeout(60000); !status)
    return std::unexpected(std::move(status.error()));
  return set_non_blocking(false);
}

std::expected<void, StarException> Socket::check_open(std::string_view method_name) const {
  if (m_socket_mode == SocketMode::Closed)
    return std::unexpected(SocketClosedException::format("Socket not open in {}", method_name));
  return {};
}

void Socket::do_shutdown() {
  if (m_socket_mode <= SocketMode::Shutdown)
    return;

  // Set socket mode first so that if this causes an exception the exception
  // handlers know the socket is being shut down.
  m_socket_mode = SocketMode::Shutdown;

  if (m_impl && !invalid_socket_descriptor(m_impl->socket_desc)) {
#ifdef STAR_SYSTEM_FAMILY_WINDOWS
    m_impl->signal_waiters();
    ::shutdown(m_impl->socket_desc, SD_BOTH);
#else
    ::shutdown(m_impl->socket_desc, SHUT_RDWR);
#endif
  }
}

void Socket::do_close() {
  if (m_socket_mode == SocketMode::Closed)
    return;

  // Set socket mode first so that if this causes an exception the exception
  // handlers know the socket is being closed.
  m_socket_mode = SocketMode::Closed;

  // A readiness waiter keeps the descriptor alive until its native wait exits.
  // do_shutdown above wakes it without waiting for the waiter or risking fd reuse.
  m_impl.reset();
}

}

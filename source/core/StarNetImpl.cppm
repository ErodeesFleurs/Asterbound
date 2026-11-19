module;
#include <stddef.h>

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
#ifdef STAR_SYSTEM_FAMILY_WINDOWS
import star.string_windows;
#endif
import star.string;
import star.format;
import star.host_address;

#ifndef AI_ADDRCONFIG
#define AI_ADDRCONFIG 0
#endif

namespace Star {


inline String netErrorString() {
#ifdef STAR_SYSTEM_WINDOWS
  LPWSTR lpMsgBuf = NULL;
  int error = WSAGetLastError();

  FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM
              | FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_MAX_WIDTH_MASK,
      NULL,
      error,
      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
      (LPTSTR)&lpMsgBuf,
      0,
      NULL);

  auto text = lpMsgBuf ? utf16ToString(lpMsgBuf) : std::expected<String, StarException>(String{});
  if (lpMsgBuf != NULL)
    LocalFree(lpMsgBuf);
  // This is already a diagnostic boundary: preserve the native status even if
  // its human-readable message cannot be converted to UTF-8.
  if (!text)
    return Star::format("{} - native error text conversion failed: {}", error, text.error().what());
  return Star::format("{} - {}", error, *text);
#else
  return Star::format("{} - {}", errno, std::strerror(errno));
#endif
}

inline bool netErrorConnectionReset() {
#ifdef STAR_SYSTEM_FAMILY_WINDOWS
  return WSAGetLastError() == WSAECONNRESET || WSAGetLastError() == WSAENETRESET;
#else
  return errno == ECONNRESET || errno == ETIMEDOUT;
#endif
}

inline bool netErrorInterrupt() {
#ifdef STAR_SYSTEM_FAMILY_WINDOWS
  return WSAGetLastError() == WSAEINTR || WSAGetLastError() == WSAEWOULDBLOCK;
#else
  return errno == EAGAIN || errno == EINTR || errno == EWOULDBLOCK;
#endif
}

inline int native_address_family(NetworkMode mode) {
  switch (mode) {
    case NetworkMode::IPv4: return AF_INET;
    case NetworkMode::IPv6: return AF_INET6;
    default: return AF_UNSPEC;
  }
}

[[nodiscard]] inline std::expected<HostAddressWithPort, StarException> address_from_native(
    std::span<std::byte const> bytes, int expected_family) {
  // sockaddr's family is not necessarily the first field (Darwin/FreeBSD).
  // Resolver records and byte views need not have sockaddr_in/in6 alignment.
  decltype(sockaddr::sa_family) family;
  constexpr auto family_offset = offsetof(sockaddr, sa_family);
  if (bytes.size() < family_offset + sizeof(family))
    return std::unexpected(NetworkException("Missing native address family"));
  std::memcpy(&family, bytes.data() + family_offset, sizeof(family));
  if (family != expected_family)
    return std::unexpected(NetworkException("Native address family mismatch"));
#if defined STAR_SYSTEM_MACOS || defined STAR_SYSTEM_FREEBSD
  if (std::to_integer<unsigned>(bytes[offsetof(sockaddr, sa_len)]) != bytes.size())
    return std::unexpected(NetworkException("Inconsistent native address length"));
#endif
  auto octets = std::span{reinterpret_cast<std::uint8_t const*>(bytes.data()), bytes.size()};
  auto port_at = [&](std::size_t offset) {
    std::uint16_t port;
    std::memcpy(&port, bytes.data() + offset, sizeof(port));
    return ntohs(port);
  };
  switch (family) {
    case AF_INET:
      if (bytes.size() != sizeof(sockaddr_in))
        return std::unexpected(NetworkException("Invalid IPv4 socket address length"));
      return HostAddressWithPort(HostAddress(octets.subspan<offsetof(sockaddr_in, sin_addr), sizeof(in_addr)>()),
          port_at(offsetof(sockaddr_in, sin_port)));
    case AF_INET6:
      if (bytes.size() != sizeof(sockaddr_in6))
        return std::unexpected(NetworkException("Invalid IPv6 socket address length"));
      return HostAddressWithPort(HostAddress(octets.subspan<offsetof(sockaddr_in6, sin6_addr), sizeof(in6_addr)>()),
          port_at(offsetof(sockaddr_in6, sin6_port)));
    default:
      return std::unexpected(NetworkException("Unsupported native address family"));
  }
}

[[nodiscard]] inline std::expected<HostAddressWithPort, StarException> address_from_native(
    sockaddr_storage const& storage, socklen_t length, int expected_family) {
  auto size = static_cast<std::size_t>(length);
  if (size > sizeof(storage))
    return std::unexpected(NetworkException("Native address exceeds socket buffer"));
  return address_from_native(std::as_bytes(std::span{&storage, 1}).first(size), expected_family);
}

[[nodiscard]] inline socklen_t native_from_address(HostAddressWithPort const& endpoint, sockaddr_storage& storage) {
  auto const& host = endpoint.address();
  auto bytes = host.bytes();
  if (host.mode() == NetworkMode::IPv4) {
    sockaddr_in address{};
#if defined STAR_SYSTEM_MACOS || defined STAR_SYSTEM_FREEBSD
    address.sin_len = sizeof(address);
#endif
    address.sin_family = AF_INET;
    address.sin_port = htons(endpoint.port());
    std::memcpy(&address.sin_addr, bytes.data(), sizeof(address.sin_addr));
    std::memcpy(&storage, &address, sizeof(address));
    return sizeof(address);
  }
  sockaddr_in6 address{};
#if defined STAR_SYSTEM_MACOS || defined STAR_SYSTEM_FREEBSD
  address.sin6_len = sizeof(address);
#endif
  address.sin6_family = AF_INET6;
  address.sin6_port = htons(endpoint.port());
  std::memcpy(&address.sin6_addr, bytes.data(), sizeof(address.sin6_addr));
  std::memcpy(&storage, &address, sizeof(address));
  return sizeof(address);
}

#ifdef STAR_SYSTEM_FAMILY_WINDOWS
inline bool invalidSocketDescriptor(SOCKET socket) {
  return socket == INVALID_SOCKET;
}
#else
inline bool invalidSocketDescriptor(int socket) {
  return socket < 0;
}
#endif

struct SocketImpl {
  SocketImpl() = default;
  SocketImpl(SocketImpl const&) = delete;
  SocketImpl& operator=(SocketImpl const&) = delete;

  ~SocketImpl() {
#ifdef STAR_SYSTEM_FAMILY_WINDOWS
    if (!invalidSocketDescriptor(socketDesc))
      ::closesocket(socketDesc);
    if (!invalidSocketDescriptor(wakeReader))
      ::closesocket(wakeReader);
    if (!invalidSocketDescriptor(wakeWriter))
      ::closesocket(wakeWriter);
#else
    if (!invalidSocketDescriptor(socketDesc))
      ::close(socketDesc);
#endif
  }

  [[nodiscard]] std::expected<void, StarException> setSockOpt(int level, int optname, const void* optval, socklen_t len) {
#ifdef STAR_SYSTEM_FAMILY_WINDOWS
    int ret = ::setsockopt(socketDesc, level, optname, (const char*)optval, len);
#else
    int ret = ::setsockopt(socketDesc, level, optname, optval, len);
#endif
    if (ret < 0)
      return std::unexpected(NetworkException(Star::format("setSockOpt failed to set {}, {}: {}", level, optname, netErrorString())));
    return {};
  }

#ifdef STAR_SYSTEM_FAMILY_WINDOWS
  [[nodiscard]] std::expected<SOCKET, StarException> prepareWait() {
    std::lock_guard locker(waitMutex);
    if (invalidSocketDescriptor(wakeReader)) {
      // select does not reliably wake when a retained data socket is shut down.
      // A persistent readable socket is a level-triggered wake for every waiter.
      SocketImpl reader;
      SocketImpl writer;
      reader.socketDesc = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
      writer.socketDesc = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
      if (invalidSocketDescriptor(reader.socketDesc) || invalidSocketDescriptor(writer.socketDesc))
        return std::unexpected(NetworkException::format("Cannot create socket readiness wake channel: {}", netErrorString()));
      sockaddr_in address{};
      address.sin_family = AF_INET;
      address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
      if (::bind(reader.socketDesc, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0)
        return std::unexpected(NetworkException::format("Cannot bind socket readiness wake channel: {}", netErrorString()));
      int length = sizeof(address);
      if (::getsockname(reader.socketDesc, reinterpret_cast<sockaddr*>(&address), &length) != 0
          || ::connect(writer.socketDesc, reinterpret_cast<sockaddr*>(&address), length) != 0)
        return std::unexpected(NetworkException::format("Cannot connect socket readiness wake channel: {}", netErrorString()));
      unsigned long nonblocking = 1;
      if (::ioctlsocket(reader.socketDesc, FIONBIO, &nonblocking) != 0
          || ::ioctlsocket(writer.socketDesc, FIONBIO, &nonblocking) != 0)
        return std::unexpected(NetworkException::format("Cannot configure socket readiness wake channel: {}", netErrorString()));
      wakeReader = std::exchange(reader.socketDesc, INVALID_SOCKET);
      wakeWriter = std::exchange(writer.socketDesc, INVALID_SOCKET);
    }
    return wakeReader;
  }

  void signalWaiters() {
    std::lock_guard locker(waitMutex);
    if (!invalidSocketDescriptor(wakeWriter)) {
      // Never drain the byte: shutdown is permanent, and all concurrent waits
      // must wake. A full send buffer already has a wake queued.
      ::send(wakeWriter, "", 1, 0);
    }
  }

  std::mutex waitMutex;
  SOCKET wakeReader = INVALID_SOCKET;
  SOCKET wakeWriter = INVALID_SOCKET;
#endif

#ifdef STAR_SYSTEM_FAMILY_WINDOWS
  SOCKET socketDesc = INVALID_SOCKET;
#else
  int socketDesc = -1;
#endif
};

}

export module star.net_impl;

export namespace Star {
  using ::Star::netErrorString;
  using ::Star::netErrorConnectionReset;
  using ::Star::netErrorInterrupt;
  using ::Star::native_address_family;
  using ::Star::address_from_native;
  using ::Star::native_from_address;
  using ::Star::invalidSocketDescriptor;
  using ::Star::SocketImpl;
}


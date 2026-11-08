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
#ifdef STAR_SYSTEM_FAMILY_WINDOWS
import star.string_windows;
#endif
import star.string;
import star.format;
import star.either;
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

[[nodiscard]] inline std::expected<void, StarException> setAddressFromNative(HostAddressWithPort& addressWithPort, NetworkMode mode, struct sockaddr_storage* sockAddr) {
  switch (mode) {
    case NetworkMode::IPv4: {
      struct sockaddr_in* addr4 = (struct sockaddr_in*)sockAddr;
      addressWithPort = HostAddressWithPort(mode, {(std::uint8_t*)&(addr4->sin_addr.s_addr), sizeof(addr4->sin_addr.s_addr)}, ntohs(addr4->sin_port));
      break;
    }
    case NetworkMode::IPv6: {
      struct sockaddr_in6* addr6 = (struct sockaddr_in6*)sockAddr;
      addressWithPort = HostAddressWithPort(mode, {(std::uint8_t*)&addr6->sin6_addr.s6_addr, sizeof(addr6->sin6_addr.s6_addr)}, ntohs(addr6->sin6_port));
      break;
    }
    default:
      return std::unexpected(NetworkException("Invalid network mode for setAddressFromNative"));
  }
  return {};
}

[[nodiscard]] inline std::expected<void, StarException> setNativeFromAddress(HostAddressWithPort const& addressWithPort, struct sockaddr_storage* sockAddr, socklen_t* sockAddrLen) {
  switch (addressWithPort.address().mode()) {
    case NetworkMode::IPv4: {
      struct sockaddr_in* addr4 = (struct sockaddr_in*)sockAddr;
      *sockAddrLen = sizeof(*addr4);

      std::memset(addr4, 0, *sockAddrLen);
      addr4->sin_family = AF_INET;
      addr4->sin_port = htons(addressWithPort.port());
      std::memcpy(((char*)&addr4->sin_addr.s_addr), addressWithPort.address().bytes().data(), addressWithPort.address().size());

      break;
    }
    case NetworkMode::IPv6: {
      struct sockaddr_in6* addr6 = (struct sockaddr_in6*)sockAddr;
      *sockAddrLen = sizeof(*addr6);

      std::memset(addr6, 0, *sockAddrLen);
      addr6->sin6_family = AF_INET6;
      addr6->sin6_port = htons(addressWithPort.port());
      std::memcpy(((char*)&addr6->sin6_addr.s6_addr), addressWithPort.address().bytes().data(), addressWithPort.address().size());
      break;
    }
    default:
      return std::unexpected(NetworkException("Invalid network mode for setNativeFromAddress"));
  }
  return {};
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
  using ::Star::setAddressFromNative;
  using ::Star::setNativeFromAddress;
  using ::Star::invalidSocketDescriptor;
  using ::Star::SocketImpl;
}


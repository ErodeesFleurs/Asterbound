module;
#include <stddef.h>

#ifdef STAR_SYSTEM_FAMILY_WINDOWS
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <errno.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#endif
import std;
import star.string;
import star.format;
import star.host_address;

namespace Star {

[[nodiscard]] inline int last_socket_error() noexcept {
#ifdef STAR_SYSTEM_FAMILY_WINDOWS
  return ::WSAGetLastError();
#else
  return errno;
#endif
}

[[nodiscard]] inline String socket_error_message(int error) {
#ifdef STAR_SYSTEM_FAMILY_WINDOWS
  // FormatMessageW requires its allocated buffer as a type-erased out parameter.
  std::unique_ptr<wchar_t, decltype([](LPWSTR text) { ::LocalFree(text); })> buffer;
  auto length = ::FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM
          | FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_MAX_WIDTH_MASK,
      nullptr, static_cast<DWORD>(error), 0, reinterpret_cast<LPWSTR>(static_cast<LPWSTR*>(std::out_ptr(buffer))), 0, nullptr);
  if (length == 0)
    return Star::format("{} - native error text unavailable (FormatMessageW error {})", error, ::GetLastError());
  auto text = String::from_wstring(std::wstring_view{buffer.get(), length});
  if (!text)
    return Star::format("{} - native error text conversion failed: {}", error, text.error().what());
  return Star::format("{} - {}", error, *text);
#else
  return Star::format("{} - {}", error, std::generic_category().message(error));
#endif
}

[[nodiscard]] inline bool is_connection_reset(int error) noexcept {
#ifdef STAR_SYSTEM_FAMILY_WINDOWS
  return error == WSAECONNRESET || error == WSAENETRESET;
#else
  return error == ECONNRESET || error == ETIMEDOUT;
#endif
}

[[nodiscard]] inline bool is_interrupted(int error) noexcept {
#ifdef STAR_SYSTEM_FAMILY_WINDOWS
  return error == WSAEINTR;
#else
  return error == EINTR;
#endif
}

[[nodiscard]] inline bool would_block(int error) noexcept {
#ifdef STAR_SYSTEM_FAMILY_WINDOWS
  return error == WSAEWOULDBLOCK;
#else
  return error == EAGAIN || error == EWOULDBLOCK;
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
using SocketDescriptor = SOCKET;
inline constexpr SocketDescriptor invalid_descriptor = INVALID_SOCKET;
#else
using SocketDescriptor = int;
inline constexpr SocketDescriptor invalid_descriptor = -1;
#endif

[[nodiscard]] inline bool invalid_socket_descriptor(SocketDescriptor descriptor) noexcept {
#ifdef STAR_SYSTEM_FAMILY_WINDOWS
  return descriptor == invalid_descriptor;
#else
  return descriptor < 0;
#endif
}

inline void close_socket_descriptor(SocketDescriptor descriptor) noexcept {
  if (!invalid_socket_descriptor(descriptor)) {
#ifdef STAR_SYSTEM_FAMILY_WINDOWS
    ::closesocket(descriptor);
#else
    ::close(descriptor);
#endif
  }
}

class SocketHandle {
public:
  explicit SocketHandle(SocketDescriptor descriptor = invalid_descriptor) noexcept : descriptor(descriptor) {}
  SocketHandle(SocketHandle const&) = delete;
  SocketHandle& operator=(SocketHandle const&) = delete;
  SocketHandle(SocketHandle&& other) noexcept : descriptor(other.release()) {}
  SocketHandle& operator=(SocketHandle&& other) noexcept {
    if (this != &other)
      close_socket_descriptor(std::exchange(descriptor, other.release()));
    return *this;
  }
  ~SocketHandle() { close_socket_descriptor(descriptor); }

  [[nodiscard]] SocketDescriptor get() const noexcept { return descriptor; }
  [[nodiscard]] SocketDescriptor release() noexcept { return std::exchange(descriptor, invalid_descriptor); }

private:
  SocketDescriptor descriptor;
};

template <typename T>
concept native_socket_option = std::same_as<T, int> || std::same_as<T, linger>
#ifdef STAR_SYSTEM_FAMILY_WINDOWS
    || std::same_as<T, DWORD>;
#else
    || std::same_as<T, timeval>;
#endif

struct SocketImpl {
  SocketImpl() = default;
  SocketImpl(SocketImpl const&) = delete;
  SocketImpl& operator=(SocketImpl const&) = delete;

  ~SocketImpl() { close_socket_descriptor(socket_desc); }

  SocketDescriptor socket_desc = invalid_descriptor;

  template <native_socket_option T>
  [[nodiscard]] std::expected<void, StarException> set_option(int level, int name, T const& value) const {
#ifdef STAR_SYSTEM_FAMILY_WINDOWS
    auto result = ::setsockopt(socket_desc, level, name, reinterpret_cast<char const*>(&value), sizeof(value));
#else
    auto result = ::setsockopt(socket_desc, level, name, &value, sizeof(value));
#endif
    if (result != 0) {
      auto error = last_socket_error();
      return std::unexpected(NetworkException::format("setsockopt failed for {}, {}: {}", level, name, socket_error_message(error)));
    }
    return {};
  }

#ifdef STAR_SYSTEM_FAMILY_WINDOWS
  [[nodiscard]] std::expected<SOCKET, StarException> prepare_wait() {
    std::lock_guard lock(wait_mutex);
    if (invalid_socket_descriptor(wake_reader.get())) {
      // select does not reliably wake when a retained data socket is shut down.
      // A persistent readable socket is a level-triggered wake for every waiter.
      SocketHandle reader(::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP));
      if (invalid_socket_descriptor(reader.get()))
        return std::unexpected(NetworkException::format("Cannot create socket readiness wake reader: {}", socket_error_message(last_socket_error())));
      SocketHandle writer(::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP));
      if (invalid_socket_descriptor(writer.get()))
        return std::unexpected(NetworkException::format("Cannot create socket readiness wake writer: {}", socket_error_message(last_socket_error())));
      sockaddr_in address{};
      address.sin_family = AF_INET;
      address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
      if (::bind(reader.get(), reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0)
        return std::unexpected(NetworkException::format("Cannot bind socket readiness wake channel: {}", socket_error_message(last_socket_error())));
      int length = sizeof(address);
      if (::getsockname(reader.get(), reinterpret_cast<sockaddr*>(&address), &length) != 0)
        return std::unexpected(NetworkException::format("Cannot query socket readiness wake address: {}", socket_error_message(last_socket_error())));
      if (::connect(writer.get(), reinterpret_cast<sockaddr const*>(&address), length) != 0)
        return std::unexpected(NetworkException::format("Cannot connect socket readiness wake channel: {}", socket_error_message(last_socket_error())));
      unsigned long nonblocking = 1;
      if (::ioctlsocket(reader.get(), FIONBIO, &nonblocking) != 0
          || ::ioctlsocket(writer.get(), FIONBIO, &nonblocking) != 0)
        return std::unexpected(NetworkException::format("Cannot configure socket readiness wake channel: {}", socket_error_message(last_socket_error())));
      wake_reader = std::move(reader);
      wake_writer = std::move(writer);
    }
    return wake_reader.get();
  }

  void signal_waiters() {
    std::lock_guard lock(wait_mutex);
    if (!invalid_socket_descriptor(wake_writer.get())) {
      // Never drain the byte: shutdown is permanent, and all concurrent waits
      // must wake. A full send buffer already has a wake queued.
      ::send(wake_writer.get(), "", 1, 0);
    }
  }

private:
  std::mutex wait_mutex;
  SocketHandle wake_reader;
  SocketHandle wake_writer;
#endif
};

}

export module star.net_impl;

export namespace Star {
  using ::Star::last_socket_error;
  using ::Star::socket_error_message;
  using ::Star::is_connection_reset;
  using ::Star::is_interrupted;
  using ::Star::would_block;
  using ::Star::native_address_family;
  using ::Star::address_from_native;
  using ::Star::native_from_address;
  using ::Star::invalid_socket_descriptor;
  using ::Star::native_socket_option;
  using ::Star::SocketHandle;
  using ::Star::SocketImpl;
}


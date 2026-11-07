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

#ifndef AI_ADDRCONFIG
#define AI_ADDRCONFIG 0
#endif

import std;
import star.format;
import star.string;
import star.either;
import star.host_address;
import star.lexical_cast;
import star.net_impl;
import star.hash;

namespace Star {


HostAddress HostAddress::localhost(NetworkMode mode) {
  if (mode == NetworkMode::IPv4) {
    std::uint8_t addr[4] = {127, 0, 0, 1};
    return HostAddress(mode, addr);
  } else if (mode == NetworkMode::IPv6) {
    std::uint8_t addr[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};
    return HostAddress(mode, addr);
  }

  return HostAddress();
}

std::expected<HostAddress, StarException> HostAddress::lookup(String const& address) {
  HostAddress result;
  auto status = result.set(address);
  if (!status)
    return std::unexpected(std::move(status.error()));
  return result;
}

HostAddress::HostAddress() : HostAddress(NetworkMode::IPv4) {}

HostAddress::HostAddress(NetworkMode mode, std::span<std::uint8_t const> address) {
  set(mode, address);
}

std::expected<HostAddress, StarException> HostAddress::create(String const& address) {
  return lookup(address);
}

NetworkMode HostAddress::mode() const {
  return m_mode;
}

std::span<std::uint8_t const> HostAddress::bytes() const {
  return {m_address, size()};
}

std::uint8_t HostAddress::octet(std::size_t i) const {
  return m_address[i];
}

bool HostAddress::isLocalHost() const {
  if (m_mode == NetworkMode::IPv4) {
    return (m_address[0] == 127 && m_address[1] == 0 && m_address[2] == 0 && m_address[3] == 1);

  } else {
    for (std::size_t i = 0; i < 15; ++i) {
      if (m_address[i] != 0)
        return false;
    }

    return m_address[15] == 1;
  }
}

bool HostAddress::isZero() const {
  if (mode() == NetworkMode::IPv4)
    return m_address[0] == 0 && m_address[1] == 0 && m_address[2] == 0 && m_address[3] == 0;

  if (mode() == NetworkMode::IPv6) {
    for (std::size_t i = 0; i < 16; i++) {
      if (m_address[i] != 0)
        return false;
    }
    return true;
  }

  return false;
}

std::size_t HostAddress::size() const {
  switch (m_mode) {
    case NetworkMode::IPv4:
      return 4;
    case NetworkMode::IPv6:
      return 16;
    default:
      return 0;
  }
}

bool HostAddress::operator==(HostAddress const& a) const {
  if (m_mode != a.m_mode)
    return false;

  std::size_t len = a.size();
  for (std::size_t i = 0; i < len; i++) {
    if (m_address[i] != a.m_address[i])
      return false;
  }

  return true;
}

std::expected<void, StarException> HostAddress::set(String const& address) {
  if (address.empty())
    return {};

  if (address == "*" || address == "0.0.0.0") {
    std::uint8_t inaddr_any[4];
    std::memset(inaddr_any, 0, sizeof(inaddr_any));
    set(NetworkMode::IPv4, inaddr_any);
  } else if (address == "::") {
    // NOTE: This will likely bind to both IPv6 and IPv4, but it does depending
    // on the OS settings
    std::uint8_t inaddr_any[16];
    std::memset(inaddr_any, 0, sizeof(inaddr_any));
    set(NetworkMode::IPv6, inaddr_any);
  } else {
    struct addrinfo* result = NULL;
    struct addrinfo* ptr = NULL;
    struct addrinfo hints;

    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    // Eliminate duplicates being returned one for each socket type.
    // As we're not using the return socket type or protocol this doesn't effect
    // us.
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    // Request only usable addresses e.g. IPv6 only if IPv6 is available
    hints.ai_flags = AI_ADDRCONFIG;

    int status = ::getaddrinfo(address.utf8Ptr(), NULL, &hints, &result);
    if (status != 0)
      return std::unexpected(NetworkException::format("Failed to determine address for '{}' (getaddrinfo status {})", address, status));

    for (ptr = result; ptr != NULL; ptr = ptr->ai_next) {
      NetworkMode mode;
      switch (ptr->ai_family) {
        case AF_INET:
          mode = NetworkMode::IPv4;
          break;
        case AF_INET6:
          mode = NetworkMode::IPv6;
          break;
        default:
          continue;
      }
      if (mode == NetworkMode::IPv4) {
        struct sockaddr_in* info = (struct sockaddr_in*)ptr->ai_addr;
        set(mode, {reinterpret_cast<std::uint8_t const*>(&info->sin_addr), sizeof(info->sin_addr)});
      } else {
        struct sockaddr_in6* info = (struct sockaddr_in6*)ptr->ai_addr;
        set(mode, {reinterpret_cast<std::uint8_t const*>(&info->sin6_addr), sizeof(info->sin6_addr)});
      }
      break;
    }
    freeaddrinfo(result);
    if (!ptr)
      return std::unexpected(NetworkException::format("No usable address for '{}'", address));
  }
  return {};
}

void HostAddress::set(NetworkMode mode, std::span<std::uint8_t const> addr) {
  m_mode = mode;
  if (!addr.empty())
    std::memcpy(m_address, addr.data(), size());
  else
    std::memset(m_address, 0, 16);
}

std::ostream& operator<<(std::ostream& os, HostAddress const& address) {
  switch (address.mode()) {
    case NetworkMode::IPv4:
      format(os, "{}.{}.{}.{}", address.octet(0), address.octet(1), address.octet(2), address.octet(3));
      break;

    case NetworkMode::IPv6:
      format(os,
          "{:02x}{:02x}:{:02x}{:02x}:{:02x}{:02x}:{:02x}{:02x}:{:02x}{:02x}:{:02x}{:02x}:{:02x}{:02x}:{:02x}{:02x}",
          address.octet(0),
          address.octet(1),
          address.octet(2),
          address.octet(3),
          address.octet(4),
          address.octet(5),
          address.octet(6),
          address.octet(7),
          address.octet(8),
          address.octet(9),
          address.octet(10),
          address.octet(11),
          address.octet(12),
          address.octet(13),
          address.octet(14),
          address.octet(15));
      break;

    default:
      std::unreachable(); // NetworkMode is established by construction/native conversion.
  }
  return os;
}

std::size_t hash<HostAddress>::operator()(HostAddress const& address) const {
  PLHasher hash;
  for (std::size_t i = 0; i < address.size(); ++i)
    hash.put(address.octet(i));
  return hash.hash();
}

std::expected<HostAddressWithPort, StarException> HostAddressWithPort::lookup(String const& address, std::uint16_t port) {
  auto hostAddress = HostAddress::lookup(address);
  if (!hostAddress)
    return std::unexpected(std::move(hostAddress.error()));
  return HostAddressWithPort(*hostAddress, port);
}

std::expected<HostAddressWithPort, StarException> HostAddressWithPort::lookupWithPort(String const& address) {
  auto const& text = address.utf8();
  auto separator = text.rfind(':');
  auto hostBytes = separator == std::string::npos ? text : text.substr(0, separator);
  String port(separator == std::string::npos ? std::string{} : text.substr(separator + 1));
  if (hostBytes.size() >= 2 && hostBytes.front() == '[' && hostBytes.back() == ']')
    hostBytes = hostBytes.substr(1, hostBytes.size() - 2);
  String host(std::move(hostBytes));
  auto portNum = maybeLexicalCast<std::uint16_t>(port);
  if (!portNum)
    return std::unexpected(NetworkException::format("Could not parse port portion of HostAddressWithPort '{}'", port));
  return lookup(host, *portNum);
}

HostAddressWithPort::HostAddressWithPort() : m_port(0) {}

HostAddressWithPort::HostAddressWithPort(HostAddress const& address, std::uint16_t port)
  : m_address(address), m_port(port) {}

HostAddressWithPort::HostAddressWithPort(NetworkMode mode, std::span<std::uint8_t const> address, std::uint16_t port) {
  m_address = HostAddress(mode, address);
  m_port = port;
}

std::expected<HostAddressWithPort, StarException> HostAddressWithPort::create(String const& address, std::uint16_t port) {
  return lookup(address, port);
}

std::expected<HostAddressWithPort, StarException> HostAddressWithPort::create(String const& address) {
  return lookupWithPort(address);
}

HostAddress HostAddressWithPort::address() const {
  return m_address;
}

std::uint16_t HostAddressWithPort::port() const {
  return m_port;
}

bool HostAddressWithPort::operator==(HostAddressWithPort const& rhs) const {
  return std::tie(m_address, m_port) == std::tie(rhs.m_address, rhs.m_port);
}

std::ostream& operator<<(std::ostream& os, HostAddressWithPort const& addressWithPort) {
  os << addressWithPort.address() << ":" << addressWithPort.port();
  return os;
}

std::size_t hash<HostAddressWithPort>::operator()(HostAddressWithPort const& addressWithPort) const {
  return hash_of(addressWithPort.address(), addressWithPort.port());
}

}

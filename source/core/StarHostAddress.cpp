#ifdef STAR_SYSTEM_FAMILY_WINDOWS
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#endif

#ifndef AI_ADDRCONFIG
#define AI_ADDRCONFIG 0
#endif

import std;
import star.text_formatter;
import star.format;
import star.string;
import star.host_address;
import star.net_impl;

namespace Star {

std::expected<HostAddress, StarException> HostAddress::from_bytes(NetworkMode mode, std::span<std::uint8_t const> bytes) {
  switch (mode) {
    case NetworkMode::IPv4:
      if (bytes.size() == 4)
        return HostAddress(bytes.first<4>());
      break;
    case NetworkMode::IPv6:
      if (bytes.size() == 16)
        return HostAddress(bytes.first<16>());
      break;
    default:
      return std::unexpected(NetworkException("Invalid host address family"));
  }
  return std::unexpected(NetworkException::format("Invalid IPv{} address length: {}", mode == NetworkMode::IPv4 ? 4 : 6, bytes.size()));
}

std::expected<HostAddress, StarException> HostAddress::lookup(String const& address) {
  auto const& text = address.utf8();
  if (text.empty() || text == "*" || text == "0.0.0.0")
    return HostAddress{};
  if (text == "::")
    return HostAddress(std::array<std::uint8_t, 16>{});
  if (text.find('\0') != std::string::npos)
    return std::unexpected(NetworkException("Host address contains an embedded NUL"));

  addrinfo hints{};
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;
  hints.ai_flags = AI_ADDRCONFIG;

  addrinfo* head = nullptr;
  if (int status = ::getaddrinfo(text.c_str(), nullptr, &hints, &head); status != 0)
    return std::unexpected(NetworkException::format("Failed to determine address for '{}' (getaddrinfo status {})", address, status));
  std::unique_ptr<addrinfo, decltype([](addrinfo* list) { ::freeaddrinfo(list); })> results(head);
  for (auto entry = results.get(); entry; entry = entry->ai_next) {
    if ((entry->ai_family != AF_INET && entry->ai_family != AF_INET6) || !entry->ai_addr
        || entry->ai_addrlen > sizeof(sockaddr_storage))
      continue;
    auto native_bytes = std::span(reinterpret_cast<std::byte const*>(entry->ai_addr), entry->ai_addrlen);
    if (auto endpoint = address_from_native(native_bytes, entry->ai_family))
      return std::move(*endpoint).address();
  }
  return std::unexpected(NetworkException::format("No usable address for '{}'", address));
}

void write_text(TextBuffer& buffer, HostAddress const& address) {
  auto bytes = address.bytes();
  if (address.mode() == NetworkMode::IPv4) {
    std::array<char, 15> text;
    auto out = text.data();
    for (auto byte : bytes) {
      if (out != text.data())
        *out++ = '.';
      out = std::to_chars(out, text.data() + text.size(), byte).ptr;
    }
    buffer.append({text.data(), out});
  } else {
    constexpr std::string_view hex = "0123456789abcdef";
    std::array<char, 39> text;
    auto out = text.begin();
    for (std::size_t i = 0; i < bytes.size(); i += 2) {
      if (i != 0)
        *out++ = ':';
      for (auto byte : bytes.subspan(i, 2)) {
        *out++ = hex[byte >> 4];
        *out++ = hex[byte & 15];
      }
    }
    buffer.append({text.data(), text.size()});
  }
}

std::expected<HostAddressWithPort, StarException> HostAddressWithPort::lookup(String const& address, std::uint16_t port) {
  auto host = HostAddress::lookup(address);
  if (!host)
    return std::unexpected(std::move(host.error()));
  return HostAddressWithPort(*host, port);
}

std::optional<std::uint16_t> HostAddressWithPort::parse_port(std::string_view text) noexcept {
  if (text.empty())
    return {};
  std::uint16_t port;
  auto end = text.data() + text.size();
  auto [ptr, error] = std::from_chars(text.data(), end, port);
  if (error != std::errc{} || ptr != end)
    return {};
  return port;
}

std::expected<HostAddressWithPort, StarException> HostAddressWithPort::lookup(String const& address) {
  std::string_view text = address.utf8();
  auto separator = text.rfind(':');
  auto suffix = separator == text.npos ? std::string_view{} : text.substr(separator + 1);
  auto port = parse_port(suffix);
  if (!port)
    return std::unexpected(NetworkException::format("Could not parse port portion of HostAddressWithPort '{}'", suffix));

  auto host = text.substr(0, separator);
  if (host.starts_with('[') && host.ends_with(']')) {
    host.remove_prefix(1);
    host.remove_suffix(1);
  }
  return lookup(String(host), *port);
}

void write_text(TextBuffer& buffer, HostAddressWithPort const& endpoint) {
  write_text(buffer, endpoint.address());
  buffer.push_back(':');
  std::array<char, 5> port;
  auto out = std::to_chars(port.data(), port.data() + port.size(), endpoint.port());
  buffer.append({port.data(), out.ptr});
}

}

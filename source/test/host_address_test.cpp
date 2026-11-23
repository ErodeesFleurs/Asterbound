#ifdef STAR_SYSTEM_FAMILY_WINDOWS
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <cerrno>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#endif

import std;
import star.string;
import star.host_address;
import star.hash;
import star.net_impl;
import star.logging;
import star.socket;
import star.tcp;
import star.udp;

#include "gtest/gtest.h"

using namespace Star;

namespace {

constexpr std::array<std::uint8_t, 4> bytes4{128, 255, 1, 254};
constexpr std::array<std::uint8_t, 16> bytes6{0x20, 1, 0x0d, 0xb8, 0, 0, 0, 0, 0, 0, 0, 0, 128, 255, 1, 254};

template <typename T>
concept borrows_bytes = requires(T&& address) { std::forward<T>(address).bytes(); };

static_assert(borrows_bytes<HostAddress const&> && borrows_bytes<HostAddress&>);
static_assert(!borrows_bytes<HostAddress> && !borrows_bytes<HostAddress const>);
static_assert(std::same_as<decltype(std::declval<HostAddressWithPort const&>().address()), HostAddress const&>);
static_assert(std::same_as<decltype(std::declval<HostAddressWithPort&&>().address()), HostAddress>);
static_assert(std::same_as<decltype(std::declval<HostAddressWithPort const&&>().address()), HostAddress>);
static_assert(!std::constructible_from<HostAddress, NetworkMode>);

TEST(HostAddress, BinaryConstructionValidatesFamilyAndExtent) {
  for (auto mode : {NetworkMode::IPv4, NetworkMode::IPv6, static_cast<NetworkMode>(-1)}) {
    std::array<std::uint8_t, 17> data{};
    for (std::size_t length = 0; length <= data.size(); ++length) {
      auto address = HostAddress::from_bytes(mode, std::span(data).first(length));
      auto valid = (mode == NetworkMode::IPv4 && length == 4) || (mode == NetworkMode::IPv6 && length == 16);
      ASSERT_EQ(address.has_value(), valid) << static_cast<int>(mode) << ':' << length;
      if (address) {
        EXPECT_EQ(address->mode(), mode);
        EXPECT_EQ(address->bytes().size(), length);
        EXPECT_TRUE(address->is_zero());
      }
    }
  }
  auto v4 = HostAddress::from_bytes(NetworkMode::IPv4, bytes4);
  ASSERT_TRUE(v4);
  EXPECT_EQ(*v4, HostAddress(bytes4));
  auto v6 = HostAddress::from_bytes(NetworkMode::IPv6, bytes6);
  ASSERT_TRUE(v6);
  EXPECT_EQ(*v6, HostAddress(bytes6));
  EXPECT_NE(*v4, *v6);
}

TEST(HostAddress, OwnsBytesBeyondInputAndEndpointScopes) {
  auto endpoint = [] {
    auto input = bytes6;
    HostAddress address(input);
    input.fill(0);
    return HostAddressWithPort(address, 80);
  }();
  EXPECT_TRUE(std::ranges::equal(endpoint.address().bytes(), bytes6));
  auto owned = std::move(endpoint).address();
  endpoint = {};
  EXPECT_TRUE(std::ranges::equal(owned.bytes(), bytes6));
}

TEST(HostAddress, WildcardsAndLocalhost) {
  HostAddress zero6(std::array<std::uint8_t, 16>{});
  for (std::string_view text : {"", "*", "0.0.0.0", "::"}) {
    auto address = HostAddress::lookup(text);
    ASSERT_TRUE(address) << text;
    EXPECT_EQ(*address, text == "::" ? zero6 : HostAddress{});
    EXPECT_TRUE(address->is_zero());
    EXPECT_FALSE(address->is_localhost());
  }
  for (auto mode : {NetworkMode::IPv4, NetworkMode::IPv6}) {
    auto local = HostAddress::localhost(mode);
    EXPECT_EQ(local.mode(), mode);
    EXPECT_TRUE(local.is_localhost());
    EXPECT_FALSE(local.is_zero());
  }
  auto local4 = HostAddress::lookup("127.0.0.1");
  ASSERT_TRUE(local4);
  EXPECT_EQ(*local4, HostAddress::localhost());
}

TEST(HostAddressWithPort, StrictDecimalPortAndLegacyAddressSpellings) {
  EXPECT_EQ(HostAddressWithPort().address(), HostAddress());
  EXPECT_EQ(HostAddressWithPort().port(), 0);
  for (std::string_view text : {"*:0", ":0", "0.0.0.0:00000", "127.0.0.1:65535", "[::]:80", ":::80"}) {
    auto endpoint = HostAddressWithPort::lookup(text);
    ASSERT_TRUE(endpoint) << text;
    auto local = text.starts_with("127.");
    auto v6 = text.starts_with("[") || text.starts_with("::");
    EXPECT_EQ(endpoint->port(), local ? 65535 : v6 ? 80 : 0);
    EXPECT_EQ(endpoint->address().mode(), v6 ? NetworkMode::IPv6 : NetworkMode::IPv4);
    EXPECT_EQ(endpoint->address().is_zero(), !local);
  }
  for (std::string_view text : {"", "*", "*:", "*:65536", "*:-1", "*:+1", "*: 1", "*:1 ", "*:2x", "*:0x10",
           "*:9999999999999999999999999999999999999"})
    EXPECT_FALSE(HostAddressWithPort::lookup(text)) << text;
  EXPECT_FALSE(HostAddressWithPort::lookup(String(std::string("*:1\0x", 5))));
  EXPECT_FALSE(HostAddressWithPort::lookup(String(std::string("127.0.0.1\0x:80", 14))));
  auto explicit_port = HostAddressWithPort::lookup("::", 65535);
  ASSERT_TRUE(explicit_port);
  EXPECT_EQ(explicit_port->address(), HostAddress(std::array<std::uint8_t, 16>{}));
  EXPECT_EQ(explicit_port->port(), 65535);
}

TEST(HostAddress, ExactFormatAndUnsignedLegacyHashes) {
  HostAddress v4(bytes4), v6(bytes6);
  HostAddressWithPort ep4(v4, 65534), ep6(v6, 65534);
  EXPECT_EQ(std::format("{}", v4), "128.255.1.254");
  EXPECT_EQ(std::format("{}", ep4), "128.255.1.254:65534");
  EXPECT_EQ(std::format("{}", v6), "2001:0db8:0000:0000:0000:0000:80ff:01fe");
  EXPECT_EQ(std::format("{}", ep6), "2001:0db8:0000:0000:0000:0000:80ff:01fe:65534");
  EXPECT_EQ(ValueHash<HostAddress>{}(v4), 0x0804010au);
  EXPECT_EQ(ValueHash<HostAddress>{}(v6), static_cast<std::size_t>(0x36fc49c0be26fcfcull));
  EXPECT_EQ(ValueHash<HostAddressWithPort>{}(ep4), static_cast<std::size_t>(0x9e38d67ec813ull));
  EXPECT_EQ(ValueHash<HostAddressWithPort>{}(ep6), static_cast<std::size_t>(0xfa2e69162ae4786aull));
  auto host = HostAddress::lookup(String(std::format("{}", v4)));
  ASSERT_TRUE(host);
  EXPECT_EQ(*host, v4);
  auto endpoint = HostAddressWithPort::lookup(String(std::format("{}", ep4)));
  ASSERT_TRUE(endpoint);
  EXPECT_EQ(*endpoint, ep4);
}

TEST(HostAddressWithPort, NumericIPv6ResolverRoundTrip) {
  // AI_ADDRCONFIG may exclude IPv6 even when a loopback socket can bind ::1.
  addrinfo hints{};
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;
  hints.ai_flags = AI_NUMERICHOST;
#ifdef AI_ADDRCONFIG
  hints.ai_flags |= AI_ADDRCONFIG;
#endif
  addrinfo* result = nullptr;
  if (auto error = ::getaddrinfo("::1", nullptr, &hints, &result))
    GTEST_SKIP() << "Native numeric IPv6 lookup unavailable: " << error;
  ::freeaddrinfo(result);

  HostAddress host(bytes6);
  auto formatted = std::format("{}", host);
  auto parsed = HostAddress::lookup(String(formatted));
  ASSERT_TRUE(parsed);
  EXPECT_EQ(*parsed, host);
  HostAddressWithPort endpoint(host, 80);
  for (auto const& text : {std::format("{}", endpoint), std::format("[{}]:80", host)}) {
    auto address = HostAddressWithPort::lookup(String(text));
    ASSERT_TRUE(address) << text;
    EXPECT_EQ(*address, endpoint);
  }
  auto local = HostAddressWithPort::lookup("[::1]:80");
  ASSERT_TRUE(local);
  EXPECT_TRUE(local->address().is_localhost());
}

TEST(HostAddressWithPort, NativeFamilyAndLengthAreCheckedBeforeDecoding) {
  for (auto const& host : {HostAddress(bytes4), HostAddress(bytes6)}) {
    HostAddressWithPort endpoint(host, 0x80ff);
    sockaddr_storage storage{};
    auto length = native_from_address(endpoint, storage);
    auto family = host.mode() == NetworkMode::IPv4 ? AF_INET : AF_INET6;
    ASSERT_EQ(length, family == AF_INET ? sizeof(sockaddr_in) : sizeof(sockaddr_in6));
    ASSERT_EQ(storage.ss_family, family);
    auto decoded = address_from_native(storage, length, family);
    ASSERT_TRUE(decoded);
    EXPECT_EQ(*decoded, endpoint);
    EXPECT_FALSE(address_from_native(storage, length, family == AF_INET ? AF_INET6 : AF_INET));
    EXPECT_FALSE(address_from_native(storage, length, AF_UNSPEC));
    EXPECT_FALSE(address_from_native(storage, sizeof(storage) + 1, family));
    auto bytes = std::as_bytes(std::span(&storage, 1));
    for (std::size_t size = 0; size < length; ++size)
      EXPECT_FALSE(address_from_native(bytes.first(size), family)) << size;
    EXPECT_FALSE(address_from_native(bytes.first(length + 1), family));
#if defined(STAR_SYSTEM_MACOS) || defined(STAR_SYSTEM_FREEBSD)
    ASSERT_EQ(storage.ss_len, length);
    --storage.ss_len;
    EXPECT_FALSE(address_from_native(storage, length, family));
    ++storage.ss_len;
#endif
    storage.ss_family = AF_UNSPEC;
    EXPECT_FALSE(address_from_native(storage, length, family));
  }
}

void check_tcp_loopback(NetworkMode mode) {
  Logger logger;
  auto host = HostAddress::localhost(mode);
  auto listener = TcpSocket::listen(logger, HostAddressWithPort(host, 0));
  ASSERT_TRUE(listener) << listener.error().what();
  auto destination = (*listener)->local_address();
  ASSERT_EQ(destination.address(), host);
  ASSERT_NE(destination.port(), 0);
  auto client = TcpSocket::connect_to(destination);
  ASSERT_TRUE(client) << client.error().what();
  auto peer = (*listener)->accept(logger);
  ASSERT_TRUE(peer) << peer.error().what();
  ASSERT_TRUE(*peer);
  EXPECT_EQ((*client)->remote_address(), destination);
  EXPECT_EQ((*peer)->local_address(), destination);
  EXPECT_EQ((*peer)->remote_address().address(), host);
  EXPECT_NE((*peer)->remote_address().port(), 0);
}

TEST(HostAddressWithPort, TcpIPv4Loopback) {
  check_tcp_loopback(NetworkMode::IPv4);
}

TEST(HostAddressWithPort, TcpIPv6Loopback) {
  // Probe the OS independently; a conversion regression must never cause a skip.
  SocketImpl probe;
  probe.socket_desc = ::socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
  sockaddr_in6 address{};
#if defined(STAR_SYSTEM_MACOS) || defined(STAR_SYSTEM_FREEBSD)
  address.sin6_len = sizeof(address);
#endif
  address.sin6_family = AF_INET6;
  address.sin6_addr = in6addr_loopback;
  if (invalid_socket_descriptor(probe.socket_desc)
      || ::bind(probe.socket_desc, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
#ifdef STAR_SYSTEM_FAMILY_WINDOWS
    auto error = WSAGetLastError();
    if (error == WSAEAFNOSUPPORT || error == WSAEPROTONOSUPPORT || error == WSAEADDRNOTAVAIL)
#else
    auto error = errno;
    if (error == EAFNOSUPPORT || error == EPROTONOSUPPORT || error == EADDRNOTAVAIL)
#endif
      GTEST_SKIP() << "No IPv6 loopback: " << error;
    FAIL() << "Native IPv6 loopback probe failed: " << error;
  }
  check_tcp_loopback(NetworkMode::IPv6);
}

TEST(HostAddressWithPort, UdpLoopbackReturnsReplyAddress) {
  SocketImpl server;
  server.socket_desc = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  ASSERT_FALSE(invalid_socket_descriptor(server.socket_desc));
  auto local = HostAddress::localhost();
  sockaddr_storage bound{};
  auto length = native_from_address(HostAddressWithPort(local, 0), bound);
  ASSERT_EQ(::bind(server.socket_desc, reinterpret_cast<sockaddr const*>(&bound), length), 0);
  ASSERT_EQ(::getsockname(server.socket_desc, reinterpret_cast<sockaddr*>(&bound), &length), 0);
  auto destination = address_from_native(bound, length, AF_INET);
  ASSERT_TRUE(destination);

  Logger logger;
  auto client = UdpSocket::create(NetworkMode::IPv4);
  ASSERT_TRUE(client);
  ASSERT_TRUE((*client)->bind(logger, HostAddressWithPort(local, 0)));
  std::array<char, 3> payload{'a', '\0', 'z'};
  ASSERT_EQ((*client)->send(*destination, payload), payload.size());
#ifdef STAR_SYSTEM_FAMILY_WINDOWS
  DWORD timeout = 2000;
#else
  timeval timeout{2, 0};
#endif
  ASSERT_TRUE(server.set_option(SOL_SOCKET, SO_RCVTIMEO, timeout));
  sockaddr_storage sender{};
  socklen_t sender_length = sizeof(sender);
  std::array<char, 3> buffer{};
  ASSERT_EQ(::recvfrom(server.socket_desc, buffer.data(), buffer.size(), 0,
      reinterpret_cast<sockaddr*>(&sender), &sender_length), 3);
  ASSERT_EQ(buffer, payload);
  ASSERT_EQ(::sendto(server.socket_desc, buffer.data(), buffer.size(), 0,
      reinterpret_cast<sockaddr const*>(&sender), sender_length), 3);
  ASSERT_TRUE((*client)->set_timeout(2000));
  HostAddressWithPort peer;
  buffer.fill(0);
  ASSERT_EQ((*client)->receive(std::ref(peer), buffer), payload.size());
  EXPECT_EQ(peer, *destination);
  EXPECT_EQ(buffer, payload);
}

TEST(SocketPoll, ReportsClosedAndReadyDescriptors) {
  auto closed = UdpSocket::create(NetworkMode::IPv4);
  ASSERT_TRUE(closed);
  (*closed)->close();
  SocketPollQuery query{{*closed, {true, false}}};
  auto result = Socket::poll(query, 0);
  ASSERT_TRUE(result && *result);
  auto const& closed_event = (**result)[*closed];
  EXPECT_TRUE(closed_event.exception);
  EXPECT_FALSE(closed_event.readable);
  EXPECT_FALSE(closed_event.writable);

  Logger logger;
  auto ready = UdpSocket::create(NetworkMode::IPv4);
  ASSERT_TRUE(ready);
  ASSERT_TRUE((*ready)->bind(logger, {HostAddress::localhost(), 0}));
  query.emplace(*ready, SocketPollQueryEntry{false, true});
  result = Socket::poll(query, 1000);
  ASSERT_TRUE(result && *result);
  EXPECT_TRUE((**result)[*closed].exception);
  EXPECT_TRUE((**result)[*ready].writable);
}

}

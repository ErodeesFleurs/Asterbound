module;
import std;
import star.format;
import star.hash;
import star.text_formatter;

import star.string;

namespace Star {

struct NetworkExceptionTag {
  static constexpr std::string_view name() { return "NetworkException"; }
};
using NetworkException = StarError<NetworkExceptionTag, IOException>;

enum class NetworkMode {
  IPv4,
  IPv6
};

class HostAddress {
public:
  static constexpr HostAddress localhost(NetworkMode mode = NetworkMode::IPv4) noexcept {
    HostAddress result;
    if (mode == NetworkMode::IPv4) {
      result.m_bytes[0] = 127;
      result.m_bytes[3] = 1;
    } else if (mode == NetworkMode::IPv6) {
      result.m_mode = mode;
      result.m_bytes.back() = 1;
    }
    return result;
  }

  [[nodiscard]] static std::expected<HostAddress, StarException> lookup(String const& address);
  [[nodiscard]] static std::expected<HostAddress, StarException> from_bytes(NetworkMode mode, std::span<std::uint8_t const> bytes);

  // The default address is the IPv4 zero address.
  constexpr HostAddress() noexcept = default;
  explicit constexpr HostAddress(std::span<std::uint8_t const, 4> bytes) noexcept {
    std::ranges::copy(bytes, m_bytes.begin());
  }
  explicit constexpr HostAddress(std::span<std::uint8_t const, 16> bytes) noexcept : m_mode(NetworkMode::IPv6) {
    std::ranges::copy(bytes, m_bytes.begin());
  }

  constexpr NetworkMode mode() const noexcept { return m_mode; }
  constexpr std::size_t size() const noexcept { return m_mode == NetworkMode::IPv4 ? 4 : 16; }
  constexpr std::span<std::uint8_t const> bytes() const & noexcept { return std::span(m_bytes).first(size()); }
  std::span<std::uint8_t const> bytes() const && = delete;

  constexpr bool is_localhost() const noexcept { return *this == localhost(m_mode); }
  constexpr bool is_zero() const noexcept { return m_bytes == std::array<std::uint8_t, 16>{}; }

  constexpr bool operator==(HostAddress const&) const noexcept = default;

private:
  NetworkMode m_mode = NetworkMode::IPv4;
  std::array<std::uint8_t, 16> m_bytes{};
};

void write_text(TextBuffer& buffer, HostAddress const& address);

template <>
struct hash<HostAddress> {
  constexpr std::size_t operator()(HostAddress const& address) const noexcept {
    PLHasher hasher;
    for (auto byte : address.bytes())
      hasher.put(byte);
    return hasher.hash();
  }
};

class HostAddressWithPort {
public:
  [[nodiscard]] static std::expected<HostAddressWithPort, StarException> lookup(String const& address, std::uint16_t port);
  // Split off the final decimal port; brackets around the host are optional.
  [[nodiscard]] static std::expected<HostAddressWithPort, StarException> lookup(String const& address);
  [[nodiscard]] static std::optional<std::uint16_t> parse_port(std::string_view text) noexcept;

  constexpr HostAddressWithPort() noexcept = default;
  constexpr HostAddressWithPort(HostAddress const& address, std::uint16_t port) noexcept : m_address(address), m_port(port) {}

  constexpr HostAddress const& address() const & noexcept { return m_address; }
  constexpr HostAddress address() && noexcept { return m_address; }
  constexpr HostAddress address() const && noexcept { return m_address; }
  constexpr std::uint16_t port() const noexcept { return m_port; }

  constexpr bool operator==(HostAddressWithPort const&) const noexcept = default;

private:
  HostAddress m_address;
  std::uint16_t m_port = 0;
};

void write_text(TextBuffer& buffer, HostAddressWithPort const& address);

template <>
struct hash<HostAddressWithPort> {
  std::size_t operator()(HostAddressWithPort const& endpoint) const noexcept {
    return hash_of(endpoint.address(), endpoint.port());
  }
};

}

export module star.host_address;

export namespace Star {
  template <>
  struct hash<HostAddress>;
  template <>
  struct hash<HostAddressWithPort>;
}


export template <> struct std::formatter<Star::HostAddress> : Star::TextFormatter {};
export template <> struct std::formatter<Star::HostAddressWithPort> : Star::TextFormatter {};

export namespace Star {
  using ::Star::NetworkExceptionTag;
  using ::Star::NetworkException;
  using ::Star::NetworkMode;
  using ::Star::HostAddressWithPort;
  using ::Star::HostAddress;
  using ::Star::write_text;
}

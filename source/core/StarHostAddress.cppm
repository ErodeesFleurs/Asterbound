module;
import std;
import star.format;
import star.text_formatter;

import star.string;
import star.either;

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
  friend void write_text(TextBuffer& buffer, HostAddress const& address);
  friend struct hash<HostAddress>;

  static HostAddress localhost(NetworkMode mode = NetworkMode::IPv4);

  [[nodiscard]] static std::expected<HostAddress, StarException> lookup(String const& address);

  // The default address is the IPv4 zero address.
  HostAddress();
  // An empty address constructs the zero address for the given mode.
  HostAddress(NetworkMode mode, std::span<std::uint8_t const> address = {});
  [[nodiscard]] static std::expected<HostAddress, StarException> create(String const& address);

  NetworkMode mode() const;
  std::span<std::uint8_t const> bytes() const;
  std::uint8_t octet(std::size_t i) const;
  std::size_t size() const;

  bool isLocalHost() const;
  bool isZero() const;

  bool operator==(HostAddress const& a) const;

private:
  [[nodiscard]] std::expected<void, StarException> set(String const& address);
  void set(NetworkMode mode, std::span<std::uint8_t const> addr);

  NetworkMode m_mode;
  std::uint8_t m_address[16];
};

void write_text(TextBuffer& buffer, HostAddress const& address);

template <>
struct hash<HostAddress> {
  std::size_t operator()(HostAddress const& address) const;
};

class HostAddressWithPort {
public:
  friend void write_text(TextBuffer& buffer, HostAddressWithPort const& address);
  friend struct hash<HostAddressWithPort>;

  [[nodiscard]] static std::expected<HostAddressWithPort, StarException> lookup(String const& address, std::uint16_t port);
  // Format may have [] brackets around address or not, to distinguish address
  // portion from port portion.
  [[nodiscard]] static std::expected<HostAddressWithPort, StarException> lookupWithPort(String const& address);

  HostAddressWithPort();
  HostAddressWithPort(HostAddress const& address, std::uint16_t port);
  HostAddressWithPort(NetworkMode mode, std::span<std::uint8_t const> address, std::uint16_t port);
  [[nodiscard]] static std::expected<HostAddressWithPort, StarException> create(String const& address, std::uint16_t port);
  [[nodiscard]] static std::expected<HostAddressWithPort, StarException> create(String const& address);

  HostAddress address() const;
  std::uint16_t port() const;

  bool operator==(HostAddressWithPort const& a) const;

private:
  HostAddress m_address;
  std::uint16_t m_port;
};

void write_text(TextBuffer& buffer, HostAddressWithPort const& address);

template <>
struct hash<HostAddressWithPort> {
  std::size_t operator()(HostAddressWithPort const& address) const;
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

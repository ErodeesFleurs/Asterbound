module;
import std;
import star.hash;
import star.version;

namespace Star {

inline constexpr VersionNumber open_protocol_version = 18;

inline constexpr VersionNumber any_version = 0xFFFFFFFF;
inline constexpr VersionNumber legacy_version = 0;

class NetCompatibilityRules {
public:
  constexpr NetCompatibilityRules() = default;
  NetCompatibilityRules(std::uint64_t, bool = false) = delete;
  constexpr NetCompatibilityRules(VersionNumber version, bool admin = false)
    : m_version(version), m_admin(admin) {}

  constexpr VersionNumber version() const { return m_version; }
  constexpr bool is_admin() const { return m_admin; }
  constexpr bool is_legacy() const { return m_version == legacy_version; }

  bool operator==(NetCompatibilityRules const&) const = default;

private:
  VersionNumber m_version = open_protocol_version;
  bool m_admin = false;
};

template <>
struct ValueHash<NetCompatibilityRules> {
  std::size_t operator()(NetCompatibilityRules const& rules) const {
    return (static_cast<std::size_t>(rules.version()) << 1) | rules.is_admin();
  }
};

}

export module star.net_compatibility;

export namespace Star {
  template <>
  struct ValueHash<NetCompatibilityRules>;
}


export namespace Star {
  using ::Star::open_protocol_version;
  using ::Star::any_version;
  using ::Star::legacy_version;
  using ::Star::NetCompatibilityRules;
  using ::Star::ValueHash;
}

module;
import std;
import star.type_identity;
import star.net_compatibility;
import star.version;
import star.data_stream;
import star.format;

namespace Star {

// Monotonically increasing NetElementVersion shared between all NetElements in
// a network.
class NetElementVersion {
public:
  std::uint64_t current() const;
  std::uint64_t increment();

private:
  std::uint64_t m_version = 0;
};

// Primary interface for the composable network synchronizable element system.
class NetElement {
public:
  virtual void query_type(TypeQuery& query) const noexcept { query_type_of<NetElement>(*this, query); }
  virtual std::string_view dynamic_type_name() const noexcept { return Star::type_name<NetElement>(); }

  virtual ~NetElement() = default;

  // A network of NetElements will have a shared monotonically increasing
  // NetElementVersion.  When elements are updated, they will mark the version
  // number at the time they are updated so that a delta can be constructed
  // that contains only changes since any past version.
  virtual void init_net_version(std::optional<std::reference_wrapper<NetElementVersion const>> version = std::nullopt) = 0;

  // Full store / load of the entire element.
  [[nodiscard]] virtual std::expected<void, StarException> net_store(DataStream& ds, NetCompatibilityRules rules) const = 0;
  [[nodiscard]] virtual std::expected<void, StarException> net_load(DataStream& ds, NetCompatibilityRules rules) = 0;

  // Enables interpolation mode.  If interpolation mode is enabled, then
  // NetElements will delay presenting incoming delta data for the
  // 'interpolation_time' parameter given in read_net_delta, and smooth between
  // received values.  When interpolation is enabled, tick_net_interpolation must
  // be periodically called to smooth values forward in time.  If
  // extrapolation_hint is given, this may be used as a hint for the amount of
  // time to extrapolate forward if no deltas are received.
  [[nodiscard]] virtual std::expected<void, StarException> enable_net_interpolation(float extrapolation_hint = 0.0f);
  [[nodiscard]] virtual std::expected<void, StarException> disable_net_interpolation();
  [[nodiscard]] virtual std::expected<void, StarException> tick_net_interpolation(float dt);

  // Write all the state changes that have happened since (and including)
  // from_version.  The normal way to use this would be to call write_net_delta with
  // the version at the time of the *last* call to write_net_delta, + 1.  If
  // from_version is 0, this will always write the full state.  Should return
  // true if a delta was needed and was written to DataStream, false otherwise.
  [[nodiscard]] virtual std::expected<bool, StarException> write_net_delta(DataStream& ds, std::uint64_t from_version, NetCompatibilityRules rules) const = 0;
  // Read a delta written by write_net_delta.  'interpolation_time' is the time in
  // the future that data from this delta should be delayed and smoothed into,
  // if interpolation is enabled.
  [[nodiscard]] virtual std::expected<void, StarException> read_net_delta(DataStream& ds, float interpolation_time = 0.0f, NetCompatibilityRules rules = {}) = 0;
  // When extrapolating, it is important to notify when a delta WOULD have been
  // received even if no deltas are produced, so no extrapolation takes place.
  [[nodiscard]] virtual std::expected<void, StarException> blank_net_delta(float interpolation_time);

  VersionNumber compatibility_version() const;
  void set_compatibility_version(VersionNumber version);
  bool check_with_rules(NetCompatibilityRules const& rules) const;
private:
  VersionNumber m_net_compatibility_version = any_version;
};

inline VersionNumber NetElement::compatibility_version() const {
  return m_net_compatibility_version;
}

inline void NetElement::set_compatibility_version(VersionNumber version) {
  m_net_compatibility_version = version;
}

inline bool NetElement::check_with_rules(NetCompatibilityRules const& rules) const {
  if (m_net_compatibility_version != any_version)
    return rules.version() >= m_net_compatibility_version;
  return true;
}

}

export module star.net_element;

export namespace Star {
  using ::Star::NetElementVersion;
  using ::Star::NetElement;
}

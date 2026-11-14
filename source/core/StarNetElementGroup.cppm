module;
import std;
import star.format;
import star.type_identity;
import star.net_compatibility;
import star.version;
import star.list;
import star.map;
import star.data_stream;
import star.set;
import star.data_stream_devices;

import star.net_element;

namespace Star {

// A static group of NetElements that itself is a NetElement and serializes
// changes based on the order in which elements are added.  All participants
// must externally add elements of the correct type in the correct order.
class NetElementGroup : public NetElement {
public:
  void const* query_type(TypeKey key) const noexcept override {
    return query_type_of<NetElementGroup, NetElement>(this, key);
  }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<NetElementGroup>(); }

  NetElementGroup() = default;

  NetElementGroup(NetElementGroup const&) = delete;
  NetElementGroup& operator=(NetElementGroup const&) = delete;

  // Add an element to the group.
  [[nodiscard]] std::expected<void, StarException> addNetElement(NetElement& element, bool propagateInterpolation = true);

  // Removes all previously added elements
  void clearNetElements();

  void initNetVersion(std::optional<std::reference_wrapper<NetElementVersion const>> version = std::nullopt) override;

  [[nodiscard]] std::expected<void, StarException> netStore(DataStream& ds, NetCompatibilityRules rules = {}) const override;
  [[nodiscard]] std::expected<void, StarException> netLoad(DataStream& ds, NetCompatibilityRules rules) override;

  [[nodiscard]] std::expected<void, StarException> enableNetInterpolation(float extrapolationHint = 0.0f) override;
  [[nodiscard]] std::expected<void, StarException> disableNetInterpolation() override;
  [[nodiscard]] std::expected<void, StarException> tickNetInterpolation(float dt) override;

  [[nodiscard]] std::expected<bool, StarException> writeNetDelta(DataStream& ds, std::uint64_t fromVersion, NetCompatibilityRules rules = {}) const override;
  [[nodiscard]] std::expected<void, StarException> readNetDelta(DataStream& ds, float interpolationTime = 0.0f, NetCompatibilityRules rules = {}) override;
  [[nodiscard]] std::expected<void, StarException> blankNetDelta(float interpolationTime) override;

  std::optional<std::reference_wrapper<NetElementVersion const>> netVersion() const;
  bool netInterpolationEnabled() const;
  float netExtrapolationHint() const;

private:
  List<std::pair<std::reference_wrapper<NetElement>, bool>> m_elements;
  std::optional<std::reference_wrapper<NetElementVersion const>> m_version = std::nullopt;
  bool m_interpolationEnabled = false;
  float m_extrapolationHint = 0.0f;

  HashMap<VersionNumber, std::size_t> m_elementCounts;

  mutable DataStreamBuffer m_buffer;
};

inline std::optional<std::reference_wrapper<NetElementVersion const>> NetElementGroup::netVersion() const {
  return m_version;
}

inline bool NetElementGroup::netInterpolationEnabled() const {
  return m_interpolationEnabled;
}

inline float NetElementGroup::netExtrapolationHint() const {
  return m_extrapolationHint;
}

}

export module star.net_element_group;

export namespace Star {
  using ::Star::NetElementGroup;
}

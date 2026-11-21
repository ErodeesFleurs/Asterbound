module;
import std;
import star.format;
import star.type_identity;
import star.net_compatibility;
import star.list;
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
  [[nodiscard]] std::expected<void, StarException> add_net_element(NetElement& element, bool propagate_interpolation = true);

  // Removes all previously added elements
  void clear_net_elements();

  void init_net_version(std::optional<std::reference_wrapper<NetElementVersion const>> version = std::nullopt) override;

  [[nodiscard]] std::expected<void, StarException> net_store(DataStream& ds, NetCompatibilityRules rules = {}) const override;
  [[nodiscard]] std::expected<void, StarException> net_load(DataStream& ds, NetCompatibilityRules rules) override;

  [[nodiscard]] std::expected<void, StarException> enable_net_interpolation(float extrapolation_hint = 0.0f) override;
  [[nodiscard]] std::expected<void, StarException> disable_net_interpolation() override;
  [[nodiscard]] std::expected<void, StarException> tick_net_interpolation(float dt) override;

  [[nodiscard]] std::expected<bool, StarException> write_net_delta(DataStream& ds, std::uint64_t from_version, NetCompatibilityRules rules = {}) const override;
  [[nodiscard]] std::expected<void, StarException> read_net_delta(DataStream& ds, float interpolation_time = 0.0f, NetCompatibilityRules rules = {}) override;
  [[nodiscard]] std::expected<void, StarException> blank_net_delta(float interpolation_time) override;

  std::optional<std::reference_wrapper<NetElementVersion const>> net_version() const;
  bool net_interpolation_enabled() const;
  float net_extrapolation_hint() const;

private:
  using GroupEntry = std::pair<std::reference_wrapper<NetElement>, bool>;
  std::span<GroupEntry const> visible_bounds(NetCompatibilityRules const& rules) const;

  List<GroupEntry> m_elements;
  std::optional<std::reference_wrapper<NetElementVersion const>> m_version = std::nullopt;
  bool m_interpolation_enabled = false;
  float m_extrapolation_hint = 0.0f;

  mutable DataStreamBuffer m_buffer;
};

inline std::optional<std::reference_wrapper<NetElementVersion const>> NetElementGroup::net_version() const {
  return m_version;
}

inline bool NetElementGroup::net_interpolation_enabled() const {
  return m_interpolation_enabled;
}

inline float NetElementGroup::net_extrapolation_hint() const {
  return m_extrapolation_hint;
}

}

export module star.net_element_group;

export namespace Star {
  using ::Star::NetElementGroup;
}

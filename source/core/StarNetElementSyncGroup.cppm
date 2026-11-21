module;
import std;
import star.format;
import star.type_identity;
import star.net_compatibility;
import star.data_stream;

import star.net_element_group;

namespace Star {

// NetElementGroup class that works with NetElements that are not automatically
// kept up to date with working data, and users need to be notified when to
// synchronize with working data.
class NetElementSyncGroup : public NetElementGroup {
public:
  void const* query_type(TypeKey key) const noexcept override {
    return query_type_of<NetElementSyncGroup, NetElementGroup>(this, key);
  }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<NetElementSyncGroup>(); }

  [[nodiscard]] std::expected<void, StarException> enable_net_interpolation(float extrapolation_hint = 0.0f) override;
  [[nodiscard]] std::expected<void, StarException> disable_net_interpolation() override;
  [[nodiscard]] std::expected<void, StarException> tick_net_interpolation(float dt) override;

  [[nodiscard]] std::expected<void, StarException> net_store(DataStream& ds, NetCompatibilityRules rules = {}) const override;
  [[nodiscard]] std::expected<void, StarException> net_load(DataStream& ds, NetCompatibilityRules rules) override;

  [[nodiscard]] std::expected<bool, StarException> write_net_delta(DataStream& ds, std::uint64_t from_version, NetCompatibilityRules rules = {}) const override;
  [[nodiscard]] std::expected<void, StarException> read_net_delta(DataStream& ds, float interpolation_time = 0.0f, NetCompatibilityRules rules = {}) override;
  [[nodiscard]] std::expected<void, StarException> blank_net_delta(float interpolation_time = 0.0f) override;

protected:
  // Notifies when data needs to be pulled from NetElements, load is true if
  // this is due to a net_load call
  [[nodiscard]] virtual std::expected<void, StarException> net_elements_need_load(bool load);
  // Notifies when data needs to be pushed to NetElements
  [[nodiscard]] virtual std::expected<void, StarException> net_elements_need_store();

private:
  bool m_has_recent_changes = false;
  float m_recent_delta_time = 0.0f;
  bool m_recent_delta_was_blank = false;
};

// Same as a NetElementSyncGroup, except instead of protected methods, calls
// optional callback functions.
class NetElementCallbackGroup : public NetElementSyncGroup {
public:
  void const* query_type(TypeKey key) const noexcept override {
    return query_type_of<NetElementCallbackGroup, NetElementSyncGroup>(this, key);
  }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<NetElementCallbackGroup>(); }

  void set_needs_load_callback(std::function<std::expected<void, StarException>(bool)> needs_load_callback);
  void set_needs_store_callback(std::function<std::expected<void, StarException>()> needs_store_callback);

private:
  [[nodiscard]] std::expected<void, StarException> net_elements_need_load(bool load) override;
  [[nodiscard]] std::expected<void, StarException> net_elements_need_store() override;

  std::function<std::expected<void, StarException>(bool)> m_net_elements_need_load;
  std::function<std::expected<void, StarException>()> m_net_elements_need_store;
};

}

export module star.net_element_sync_group;

export namespace Star {
  using ::Star::NetElementSyncGroup;
  using ::Star::NetElementCallbackGroup;
}

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

  [[nodiscard]] std::expected<void, StarException> enableNetInterpolation(float extrapolationHint = 0.0f) override;
  [[nodiscard]] std::expected<void, StarException> disableNetInterpolation() override;
  [[nodiscard]] std::expected<void, StarException> tickNetInterpolation(float dt) override;

  [[nodiscard]] std::expected<void, StarException> netStore(DataStream& ds, NetCompatibilityRules rules = {}) const override;
  [[nodiscard]] std::expected<void, StarException> netLoad(DataStream& ds, NetCompatibilityRules rules) override;

  [[nodiscard]] std::expected<bool, StarException> writeNetDelta(DataStream& ds, std::uint64_t fromVersion, NetCompatibilityRules rules = {}) const override;
  [[nodiscard]] std::expected<void, StarException> readNetDelta(DataStream& ds, float interpolationTime = 0.0f, NetCompatibilityRules rules = {}) override;
  [[nodiscard]] std::expected<void, StarException> blankNetDelta(float interpolationTime = 0.0f) override;

protected:
  // Notifies when data needs to be pulled from NetElements, load is true if
  // this is due to a netLoad call
  [[nodiscard]] virtual std::expected<void, StarException> netElementsNeedLoad(bool load);
  // Notifies when data needs to be pushed to NetElements
  [[nodiscard]] virtual std::expected<void, StarException> netElementsNeedStore();

private:
  bool m_hasRecentChanges = false;
  float m_recentDeltaTime = 0.0f;
  bool m_recentDeltaWasBlank = false;
};

// Same as a NetElementSyncGroup, except instead of protected methods, calls
// optional callback functions.
class NetElementCallbackGroup : public NetElementSyncGroup {
public:
  void const* query_type(TypeKey key) const noexcept override {
    return query_type_of<NetElementCallbackGroup, NetElementSyncGroup>(this, key);
  }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<NetElementCallbackGroup>(); }

  void setNeedsLoadCallback(std::function<std::expected<void, StarException>(bool)> needsLoadCallback);
  void setNeedsStoreCallback(std::function<std::expected<void, StarException>()> needsStoreCallback);

private:
  [[nodiscard]] std::expected<void, StarException> netElementsNeedLoad(bool load) override;
  [[nodiscard]] std::expected<void, StarException> netElementsNeedStore() override;

  std::function<std::expected<void, StarException>(bool)> m_netElementsNeedLoad;
  std::function<std::expected<void, StarException>()> m_netElementsNeedStore;
};

}

export module star.net_element_sync_group;

export namespace Star {
  using ::Star::NetElementSyncGroup;
  using ::Star::NetElementCallbackGroup;
}

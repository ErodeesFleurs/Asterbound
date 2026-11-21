module;

import star.format;
import std;
import star.string;
import star.list;
import star.json;
import star.strong_typedef;
import star.data_stream;
import star.id_map;
import star.status_types;

import star.stat_set;

import star.net_element_basic_fields;
import star.net_element_float_fields;
import star.net_element_containers;
import star.net_element_sync_group;

namespace Star {

// Extension of StatSet that can easily be set up from config, and is network
// capable.
class StatCollection : public NetElementSyncGroup {
public:
  [[nodiscard]] static std::expected<std::shared_ptr<StatCollection>, StarException> create(Json const& config);

  StringList statNames() const;
  float stat(String const& statName) const;
  // Returns true if the stat is strictly greater than zero
  bool statPositive(String const& statName) const;

  StringList resourceNames() const;
  bool isResource(String const& resourceName) const;
  float resource(String const& resourceName) const;
  // Returns true if the resource is strictly greater than zero
  bool resourcePositive(String const& resourceName) const;

  [[nodiscard]] std::expected<void, StarException> setResource(String const& resourceName, float value);
  [[nodiscard]] std::expected<void, StarException> modifyResource(String const& resourceName, float amount);

  float giveResource(String const& resourceName, float amount);

  [[nodiscard]] std::expected<bool, StarException> consumeResource(String const& resourceName, float amount);
  [[nodiscard]] std::expected<bool, StarException> overConsumeResource(String const& resourceName, float amount);

  [[nodiscard]] std::expected<bool, StarException> resourceLocked(String const& resourceName) const;
  [[nodiscard]] std::expected<void, StarException> setResourceLocked(String const& resourceName, bool locked);

  // Resetting a resource also clears any locked states
  [[nodiscard]] std::expected<void, StarException> resetResource(String const& resourceName);
  [[nodiscard]] std::expected<void, StarException> resetAllResources();

  [[nodiscard]] std::expected<std::optional<float>, StarException> resourceMax(String const& resourceName) const;
  [[nodiscard]] std::expected<std::optional<float>, StarException> resourcePercentage(String const& resourceName) const;
  [[nodiscard]] std::expected<float, StarException> setResourcePercentage(String const& resourceName, float resourcePercentage);
  [[nodiscard]] std::expected<float, StarException> modifyResourcePercentage(String const& resourceName, float resourcePercentage);

  [[nodiscard]] std::expected<StatModifierGroupId, StarException> addStatModifierGroup(List<StatModifier> modifiers = {});
  [[nodiscard]] std::expected<void, StarException> setStatModifierGroup(StatModifierGroupId modifierGroupId, List<StatModifier> modifiers);
  void removeStatModifierGroup(StatModifierGroupId modifierGroupId);
  void clearStatModifiers();

  void tickMaster(float dt);
  void tickSlave(float dt);

private:
  struct AbsoluteResourceValue {
    float value;
  };

  struct ResourceMaximumFraction {
    float fraction;
  };

  StatCollection() = default;
  [[nodiscard]] std::expected<void, StarException> net_elements_need_load(bool full) override;
  [[nodiscard]] std::expected<void, StarException> net_elements_need_store() override;

  StatSet m_stats;
  // Config-only reset defaults: initialValue is absolute; initialPercentage is
  // a fraction of the current maximum (1.0 means full), not a 0..100 percentage.
  // Keep the configured, unclamped amount. Only actual values and locks are
  // stored on disk or sent over the network, never these alternatives.
  StringMap<std::variant<AbsoluteResourceValue, ResourceMaximumFraction>> m_defaultResourceValues;

  NetElementMap<StatModifierGroupId, List<StatModifier>> m_statModifiersNetState;
  StableStringMap<NetElementFloat> m_resourceValuesNetStates;
  StableStringMap<NetElementBool> m_resourceLockedNetStates;
};

}

export module star.stat_collection;

export namespace Star {
  using ::Star::StatCollection;
}

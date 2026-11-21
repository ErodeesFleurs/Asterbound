import star.format;
import std;
import star.string;
import star.list;
import star.json;
import star.strong_typedef;
import star.data_stream;
import star.id_map;
import star.status_types;
import star.data_stream_extra;
import star.game_types;
import star.logging;

import star.stat_set;
import star.stat_collection;

namespace Star {

std::expected<std::shared_ptr<StatCollection>, StarException> StatCollection::create(Json const& config) {
  auto collection = std::shared_ptr<StatCollection>(new StatCollection());
  auto stats = config.get_object("stats", {});
  if (!stats) return std::unexpected(std::move(stats.error()));
  for (auto const& stat : *stats) {
    auto baseValue = stat.second.get_float("baseValue", 0.0);
    if (!baseValue) return std::unexpected(std::move(baseValue.error()));
    auto added = collection->m_stats.addStat(stat.first, *baseValue);
    if (!added) return std::unexpected(std::move(added.error()));
  }

  auto resources = config.get_object("resources", {});
  if (!resources) return std::unexpected(std::move(resources.error()));
  for (auto const& resource : *resources) {
    auto statOrValue = [&resource](String const& statName, String const& valueName) -> std::expected<std::variant<std::monostate, String, float>, StarException> {
      auto stat = resource.second.opt_string(statName);
      if (!stat) return std::unexpected(std::move(stat.error()));
      if (*stat)
        return std::variant<std::monostate, String, float>(std::move(**stat));
      auto value = resource.second.opt_float(valueName);
      if (!value) return std::unexpected(std::move(value.error()));
      if (*value)
        return std::variant<std::monostate, String, float>(**value);
      return std::variant<std::monostate, String, float>{};
    };
    auto resourceMax = statOrValue("maxStat", "maxValue");
    if (!resourceMax) return std::unexpected(std::move(resourceMax.error()));
    auto resourceDelta = statOrValue("deltaStat", "deltaValue");
    if (!resourceDelta) return std::unexpected(std::move(resourceDelta.error()));
    auto added = collection->m_stats.addResource(resource.first, std::move(*resourceMax), std::move(*resourceDelta));
    if (!added) return std::unexpected(std::move(added.error()));
    auto initialValue = resource.second.opt_float("initialValue");
    if (!initialValue) return std::unexpected(std::move(initialValue.error()));
    if (*initialValue) {
      auto set = collection->m_stats.setResourceValue(resource.first, **initialValue);
      if (!set) return std::unexpected(std::move(set.error()));
      collection->m_defaultResourceValues[resource.first] = AbsoluteResourceValue{**initialValue};
    } else {
      auto maximumFraction = resource.second.opt_float("initialPercentage");
      if (!maximumFraction)
        return std::unexpected(std::move(maximumFraction.error()));
      if (*maximumFraction) {
        auto set = collection->m_stats.setResourcePercentage(resource.first, **maximumFraction);
        if (!set) return std::unexpected(std::move(set.error()));
        collection->m_defaultResourceValues[resource.first] = ResourceMaximumFraction{**maximumFraction};
      } else {
        auto maximum = collection->m_stats.resourceMax(resource.first);
        if (!maximum) return std::unexpected(std::move(maximum.error()));
        if (!std::holds_alternative<std::monostate>(*maximum)) {
          auto set = collection->m_stats.setResourcePercentage(resource.first, 1.0f);
          if (!set) return std::unexpected(std::move(set.error()));
          collection->m_defaultResourceValues[resource.first] = ResourceMaximumFraction{1.0f};
        } else {
          auto set = collection->m_stats.setResourceValue(resource.first, 0.0f);
          if (!set) return std::unexpected(std::move(set.error()));
          collection->m_defaultResourceValues[resource.first] = AbsoluteResourceValue{0.0f};
        }
      }
    }
  }

  auto modifiers = collection->add_net_element(collection->m_statModifiersNetState);
  if (!modifiers) return std::unexpected(std::move(modifiers.error()));
  // Sort resource names alphabetically to ensure the same order on master and slaves.
  auto resourceNames = collection->m_stats.resourceNames();
  std::ranges::sort(resourceNames);
  for (auto const& resource : resourceNames) {
    auto& resourceNetState = collection->m_resourceValuesNetStates[resource];
    auto valueAdded = collection->add_net_element(resourceNetState);
    if (!valueAdded) return std::unexpected(std::move(valueAdded.error()));
    auto lockAdded = collection->add_net_element(collection->m_resourceLockedNetStates[resource]);
    if (!lockAdded) return std::unexpected(std::move(lockAdded.error()));
  }
  return collection;
}

StringList StatCollection::statNames() const {
  return m_stats.effectiveStatNames();
}

float StatCollection::stat(String const& statName) const {
  return m_stats.statEffectiveValue(statName);
}

bool StatCollection::statPositive(String const& statName) const {
  return stat(statName) > 0.0f;
}

StringList StatCollection::resourceNames() const {
  return m_stats.resourceNames();
}

bool StatCollection::isResource(String const& resourceName) const {
  return m_stats.isResource(resourceName);
}

float StatCollection::resource(String const& resourceName) const {
  return m_stats.resourceValue(resourceName);
}

bool StatCollection::resourcePositive(String const& resourceName) const {
  return resource(resourceName) > 0.0f;
}

std::expected<void, StarException> StatCollection::setResource(String const& resourceName, float value) {
  auto set = m_stats.setResourceValue(resourceName, value);
  if (!set) return std::unexpected(std::move(set.error()));
  return {};
}

std::expected<void, StarException> StatCollection::modifyResource(String const& resourceName, float amount) {
  auto modified = m_stats.modifyResourceValue(resourceName, amount);
  if (!modified) return std::unexpected(std::move(modified.error()));
  return {};
}

float StatCollection::giveResource(String const& resourceName, float amount) {
  return m_stats.giveResourceValue(resourceName, amount);
}

std::expected<bool, StarException> StatCollection::consumeResource(String const& resourceName, float amount) {
  return m_stats.consumeResourceValue(resourceName, amount);
}

std::expected<bool, StarException> StatCollection::overConsumeResource(String const& resourceName, float amount) {
  return m_stats.overConsumeResourceValue(resourceName, amount);
}

std::expected<bool, StarException> StatCollection::resourceLocked(String const& resourceName) const {
  return m_stats.resourceLocked(resourceName);
}

std::expected<void, StarException> StatCollection::setResourceLocked(String const& resourceName, bool locked) {
  return m_stats.setResourceLocked(resourceName, locked);
}

std::expected<void, StarException> StatCollection::resetResource(String const& resourceName) {
  auto unlocked = m_stats.setResourceLocked(resourceName, false);
  if (!unlocked) return std::unexpected(std::move(unlocked.error()));
  auto defaultValue = m_defaultResourceValues.get(resourceName);
  if (!defaultValue)
    return std::unexpected(std::move(defaultValue.error()));
  return std::visit(
    [&](auto const& initial) -> std::expected<void, StarException> {
      if constexpr (std::is_same_v<std::decay_t<decltype(initial)>, AbsoluteResourceValue>)
        return m_stats.setResourceValue(resourceName, initial.value).transform([](float) {});
      else
        return m_stats.setResourcePercentage(resourceName, initial.fraction).transform([](float) {});
    },
    defaultValue->get());
}

std::expected<void, StarException> StatCollection::resetAllResources() {
  for (auto const& resourceName : m_stats.resourceNames()) {
    auto reset = resetResource(resourceName);
    if (!reset) return std::unexpected(std::move(reset.error()));
  }
  return {};
}

std::expected<std::optional<float>, StarException> StatCollection::resourceMax(String const& resourceName) const {
  return m_stats.resourceMaxValue(resourceName);
}

std::expected<std::optional<float>, StarException> StatCollection::resourcePercentage(String const& resourceName) const {
  return m_stats.resourcePercentage(resourceName);
}

std::expected<float, StarException> StatCollection::setResourcePercentage(String const& resourceName, float resourcePercentage) {
  return m_stats.setResourcePercentage(resourceName, resourcePercentage);
}

std::expected<float, StarException> StatCollection::modifyResourcePercentage(String const& resourceName, float resourcePercentage) {
  return m_stats.modifyResourcePercentage(resourceName, resourcePercentage);
}

std::expected<StatModifierGroupId, StarException> StatCollection::addStatModifierGroup(List<StatModifier> modifiers) {
  return m_stats.addStatModifierGroup(std::move(modifiers));
}

std::expected<void, StarException> StatCollection::setStatModifierGroup(StatModifierGroupId modifierGroupId, List<StatModifier> modifiers) {
  auto set = m_stats.setStatModifierGroup(modifierGroupId, std::move(modifiers));
  if (!set) return std::unexpected(std::move(set.error()));
  return {};
}

void StatCollection::removeStatModifierGroup(StatModifierGroupId modifierGroupId) {
  m_stats.removeStatModifierGroup(modifierGroupId);
}

void StatCollection::clearStatModifiers() {
  m_stats.clearStatModifiers();
}

void StatCollection::tickMaster(float dt) {
  m_stats.update(dt);
}

void StatCollection::tickSlave(float) {
  m_stats.update(0.0f);
}

std::expected<void, StarException> StatCollection::net_elements_need_load(bool) {
  if (m_statModifiersNetState.pull_updated()) {
    StatModifierGroupMap all_modifiers;
    for (auto const& [id, modifiers] : m_statModifiersNetState) {
      auto added = all_modifiers.try_emplace(id, modifiers);
      if (!added) return std::unexpected(std::move(added.error()));
    }
    m_stats.setAllStatModifierGroups(std::move(all_modifiers));
  }
  for (auto const& pair : m_resourceValuesNetStates) {
    auto set = m_stats.setResourceValue(pair.first, pair.second.get());
    if (!set) return std::unexpected(std::move(set.error()));
  }
  for (auto& pair : m_resourceLockedNetStates) {
    auto set = m_stats.setResourceLocked(pair.first, pair.second.get());
    if (!set) return std::unexpected(std::move(set.error()));
  }
  return {};
}

std::expected<void, StarException> StatCollection::net_elements_need_store() {
  m_statModifiersNetState.set_contents(m_stats.allStatModifierGroups());
  for (auto& pair : m_resourceValuesNetStates)
    pair.second.set(m_stats.resourceValue(pair.first));
  for (auto& pair : m_resourceLockedNetStates) {
    auto locked = m_stats.resourceLocked(pair.first);
    if (!locked) return std::unexpected(std::move(locked.error()));
    pair.second.set(*locked);
  }
  return {};
}

}

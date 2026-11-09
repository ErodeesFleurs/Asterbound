import star.format;
import std;
import star.string;
import star.list;
import star.json;
import star.strong_typedef;
import star.data_stream;
import star.id_map;
import star.status_types;
import star.math_common;

import star.stat_set;

namespace Star {

std::expected<void, StarException> StatSet::addStat(String statName, float baseValue) {
  if (m_baseStats.contains(statName))
    return std::unexpected(StatusException::format("Added duplicate stat named '{}' in StatSet", statName));
  m_baseStats.insert(std::move(statName), baseValue);
  update(0.0f);
  return {};
}

std::expected<void, StarException> StatSet::removeStat(String const& statName) {
  if (!m_baseStats.remove(statName))
    return std::unexpected(StatusException::format("No such base stat '{}' in StatSet", statName));
  update(0.0f);
  return {};
}

StringList StatSet::baseStatNames() const {
  return m_baseStats.keys();
}

bool StatSet::isBaseStat(String const& statName) const {
  return m_baseStats.contains(statName);
}

std::expected<float, StarException> StatSet::statBaseValue(String const& statName) const {
  if (auto s = m_baseStats.find_ref(statName))
    return s->get();
  return std::unexpected(StatusException::format("No such base stat '{}' in StatSet", statName));
}

std::expected<void, StarException> StatSet::setStatBaseValue(String const& statName, float value) {
  if (auto s = m_baseStats.find_ref(statName)) {
    if (s->get() != value) {
      s->get() = value;
      update(0.0f);
    }
    return {};
  }
  return std::unexpected(StatusException::format("No such base stat '{}' in StatSet", statName));
}

std::expected<StatModifierGroupId, StarException> StatSet::addStatModifierGroup(List<StatModifier> modifiers) {
  bool empty = modifiers.empty();
  auto id = m_statModifierGroups.add(std::move(modifiers));
  if (!id) return std::unexpected(std::move(id.error()));
  if (!empty)
    update(0.0f);
  return *id;
}

List<StatModifierGroupId> StatSet::statModifierGroupIds() const {
  return m_statModifierGroups.keys();
}

std::expected<List<StatModifier>, StarException> StatSet::statModifierGroup(StatModifierGroupId modifierGroupId) const {
  auto group = m_statModifierGroups.get(modifierGroupId);
  if (!group) return std::unexpected(std::move(group.error()));
  return group->get();
}

std::expected<void, StarException> StatSet::addStatModifierGroup(StatModifierGroupId groupId, List<StatModifier> modifiers) {
  bool empty = modifiers.empty();
  auto added = m_statModifierGroups.add(groupId, std::move(modifiers));
  if (!added) return std::unexpected(std::move(added.error()));
  if (!empty)
    update(0.0f);
  return {};
}

std::expected<bool, StarException> StatSet::setStatModifierGroup(StatModifierGroupId groupId, List<StatModifier> modifiers) {
  auto group = m_statModifierGroups.get(groupId);
  if (!group) return std::unexpected(std::move(group.error()));
  auto& list = group->get();
  if (list != modifiers) {
    list = std::move(modifiers);
    update(0.0f);
    return true;
  }
  return false;
}

bool StatSet::removeStatModifierGroup(StatModifierGroupId modifierSetId) {
  if (m_statModifierGroups.remove(modifierSetId)) {
    update(0.0f);
    return true;
  }
  return false;
}

void StatSet::clearStatModifiers() {
  if (!m_statModifierGroups.empty()) {
    m_statModifierGroups.clear();
    update(0.0f);
  }
}

StatModifierGroupMap const& StatSet::allStatModifierGroups() const {
  return m_statModifierGroups;
}

void StatSet::setAllStatModifierGroups(StatModifierGroupMap map) {
  if (m_statModifierGroups != map) {
    m_statModifierGroups = std::move(map);
    update(0.0f);
  }
}

StringList StatSet::effectiveStatNames() const {
  return m_effectiveStats.keys();
}

bool StatSet::isEffectiveStat(String const& statName) const {
  return m_effectiveStats.contains(statName);
}

float StatSet::statEffectiveValue(String const& statName) const {
  // All stat values will be added to m_effectiveStats regardless of whether a
  // modifier is applied for it.
  if (auto modified = m_effectiveStats.find_ref(statName))
    return modified->get().effectiveModifiedValue;
  else
    return 0.0f;
}

std::expected<void, StarException> StatSet::addResource(String resourceName, std::variant<std::monostate, String, float> max, std::variant<std::monostate, String, float> delta) {
  if (m_resources.contains(resourceName))
    return std::unexpected(StatusException::format("Added duplicate resource named '{}' in StatSet", resourceName));
  m_resources.insert({std::move(resourceName), Resource{std::move(max), std::move(delta), false, 0.0f, {}}});
  update(0.0f);
  return {};
}

std::expected<void, StarException> StatSet::removeResource(String const& resourceName) {
  if (!m_resources.remove(resourceName))
    return std::unexpected(StatusException::format("No such resource named '{}' in StatSet", resourceName));
  return {};
}

StringList StatSet::resourceNames() const {
  return m_resources.keys();
}

std::expected<std::variant<std::monostate, String, float>, StarException> StatSet::resourceMax(String const& resourceName) const {
  auto resource = getResource(resourceName);
  if (!resource) return std::unexpected(std::move(resource.error()));
  return resource->get().max;
}

std::expected<std::variant<std::monostate, String, float>, StarException> StatSet::resourceDelta(String const& resourceName) const {
  auto resource = getResource(resourceName);
  if (!resource) return std::unexpected(std::move(resource.error()));
  return resource->get().delta;
}

bool StatSet::isResource(String const& resourceName) const {
  return m_resources.contains(resourceName);
}

float StatSet::resourceValue(String const& resourceName) const {
  if (auto r = m_resources.find_ref(resourceName))
    return r->get().value;
  return 0.0f;
}

std::expected<float, StarException> StatSet::setResourceValue(String const& resourceName, float value) {
  auto resource = getResource(resourceName);
  if (!resource) return std::unexpected(std::move(resource.error()));
  return resource->get().setValue(value);
}

std::expected<float, StarException> StatSet::modifyResourceValue(String const& resourceName, float amount) {
  auto resource = getResource(resourceName);
  if (!resource) return std::unexpected(std::move(resource.error()));
  return resource->get().setValue(resource->get().value + amount);
}

float StatSet::giveResourceValue(String const& resourceName, float amount) {
  if (auto r = m_resources.find_ref(resourceName)) {
    float previousValue = r->get().value;
    r->get().setValue(r->get().value + amount);
    return r->get().value - previousValue;
  }
  return 0;
}

std::expected<bool, StarException> StatSet::consumeResourceValue(String const& resourceName, float amount) {
  return consumeResourceValue(resourceName, amount, false);
}

std::expected<bool, StarException> StatSet::overConsumeResourceValue(String const& resourceName, float amount) {
  return consumeResourceValue(resourceName, amount, true);
}

std::expected<bool, StarException> StatSet::resourceLocked(String const& resourceName) const {
  auto resource = getResource(resourceName);
  if (!resource) return std::unexpected(std::move(resource.error()));
  return resource->get().locked;
}

std::expected<void, StarException> StatSet::setResourceLocked(String const& resourceName, bool locked) {
  auto resource = getResource(resourceName);
  if (!resource) return std::unexpected(std::move(resource.error()));
  resource->get().locked = locked;
  return {};
}

std::expected<std::optional<float>, StarException> StatSet::resourceMaxValue(String const& resourceName) const {
  auto resource = getResource(resourceName);
  if (!resource) return std::unexpected(std::move(resource.error()));
  return resource->get().maxValue;
}

std::expected<std::optional<float>, StarException> StatSet::resourcePercentage(String const& resourceName) const {
  auto resource = getResource(resourceName);
  if (!resource) return std::unexpected(std::move(resource.error()));
  if (!resource->get().maxValue)
    return std::optional<float>{};
  return resource->get().value / *resource->get().maxValue;
}

std::expected<float, StarException> StatSet::setResourcePercentage(String const& resourceName, float resourcePercentage) {
  auto resource = getResource(resourceName);
  if (!resource) return std::unexpected(std::move(resource.error()));
  if (!resource->get().maxValue)
    return std::unexpected(StatusException::format("setResourcePersentage called on resource '{}' which has no maximum", resourceName));
  return resource->get().setValue(resourcePercentage * *resource->get().maxValue);
}

std::expected<float, StarException> StatSet::modifyResourcePercentage(String const& resourceName, float resourcePercentage) {
  auto resource = getResource(resourceName);
  if (!resource) return std::unexpected(std::move(resource.error()));
  if (!resource->get().maxValue)
    return std::unexpected(StatusException::format("modifyResourcePercentage called on resource '{}' which has no maximum", resourceName));
  return resource->get().setValue(resource->get().value + resourcePercentage * *resource->get().maxValue);
}

void StatSet::update(float dt) {
  // We use two intermediate values for calculating the effective stat value.
  // The baseModifiedValue represents the application of the base percentage
  // modifiers and the value modifiers, which only depend on the baseValue.
  // The effectiveModifiedValue is the application of all effective percentage
  // modifiers successively on the baseModifiedValue, causing them to stack with
  // each other in addition to base multipliers and value modifiers

  // First, clear the modified values to get rid of temporary stats applied
  // from modifiers that may no longer be there
  m_effectiveStats.clear();

  // Then we do all the StatValueModifiers and StatBaseMultipliers and
  // compute the baseModifiedValue

  for (auto&& p : m_baseStats) {
    auto& stat = m_effectiveStats[p.first];
    stat.baseValue = p.second;
    stat.baseModifiedValue = stat.baseValue;
  }

  for (auto const& p : m_statModifierGroups) {
    for (auto const& modifier : p.second) {
      if (std::holds_alternative<StatBaseMultiplier>(modifier)) {
        auto const& baseMultiplier = std::get<StatBaseMultiplier>(modifier);
        auto& stat = m_effectiveStats[baseMultiplier.statName];
        stat.baseModifiedValue += (baseMultiplier.baseMultiplier - 1.0f) * stat.baseValue;
      } else if (std::holds_alternative<StatValueModifier>(modifier)) {
        auto const& valueModifier = std::get<StatValueModifier>(modifier);
        auto& stat = m_effectiveStats[valueModifier.statName];
        stat.baseModifiedValue += valueModifier.value;
      }
    }
  }

  // Then we do all the StatEffectiveMultipliers and compute the
  // final effectiveModifiedValue

  for (auto&& p : m_effectiveStats)
    p.second.effectiveModifiedValue = p.second.baseModifiedValue;

  for (auto const& p : m_statModifierGroups) {
    for (auto const& modifier : p.second) {
      if (std::holds_alternative<StatEffectiveMultiplier>(modifier)) {
        auto const& effectiveMultiplier = std::get<StatEffectiveMultiplier>(modifier);
        auto& stat = m_effectiveStats[effectiveMultiplier.statName];
        stat.effectiveModifiedValue *= effectiveMultiplier.effectiveMultiplier;
      }
    }
  }

  // Then update all the resources due to charging and percentage tracking,
  // after updating the stats.

  for (auto&& p : m_resources) {
    std::optional<float> newMaxValue;
    if (std::holds_alternative<String>(p.second.max))
      newMaxValue = statEffectiveValue(std::get<String>(p.second.max));
    else if (std::holds_alternative<float>(p.second.max))
      newMaxValue = std::get<float>(p.second.max);

    // If the resource has a maximum value, rather than keeping the absolute
    // value of the resource the same between updates, the resource value
    // should instead track the percentage.
    if (p.second.maxValue && newMaxValue && *p.second.maxValue > 0.0f)
      p.second.value *= *newMaxValue / *p.second.maxValue;

    p.second.maxValue = newMaxValue;
    if (p.second.maxValue)
      p.second.value = clamp(p.second.value, 0.0f, *p.second.maxValue);

    if (dt != 0.0f) {
      float delta = 0.0f;
      if (std::holds_alternative<String>(p.second.delta))
        delta = statEffectiveValue(std::get<String>(p.second.delta));
      else if (std::holds_alternative<float>(p.second.delta))
        delta = std::get<float>(p.second.delta);
      p.second.setValue(p.second.value + delta * dt);
    }
  }
}

float StatSet::Resource::setValue(float v) {
  if (maxValue)
    value = clamp(v, 0.0f, *maxValue);
  else
    value = std::max(v, 0.0f);
  return value;
}

std::expected<std::reference_wrapper<StatSet::Resource const>, StarException> StatSet::getResource(String const& resourceName) const {
  if (auto r = m_resources.find_ref(resourceName))
    return *r;
  return std::unexpected(StatusException::format("No such resource '{}' in StatSet", resourceName));
}

std::expected<std::reference_wrapper<StatSet::Resource>, StarException> StatSet::getResource(String const& resourceName) {
  if (auto r = m_resources.find_ref(resourceName))
    return *r;
  return std::unexpected(StatusException::format("No such resource '{}' in StatSet", resourceName));
}

std::expected<bool, StarException> StatSet::consumeResourceValue(String const& resourceName, float amount, bool allowOverConsume) {
  if (amount < 0.0f)
    return std::unexpected(StatusException::format("StatSet, consumeResource called with negative amount '{}' {}", resourceName, amount));

  if (auto r = m_resources.find_ref(resourceName)) {
    if (r->get().locked)
      return false;

    if (r->get().value >= amount) {
      r->get().setValue(r->get().value - amount);
      return true;
    } else if (r->get().value > 0.0f && allowOverConsume) {
      r->get().setValue(0.0f);
      return true;
    }
  }
  return false;
}

}

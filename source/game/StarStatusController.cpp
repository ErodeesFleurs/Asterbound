import star.format;
import star.game_timers;
import star.actor_movement_controller;
import star.json;
import star.json_extra;
import star.id_map;
import star.data_stream_extra;
import star.poly;
import star.casting;
import star.data_stream;
import star.game_types;
import star.strong_typedef;
import star.bi_map;
import star.list;
import star.rpc_promise;
import star.observer_stream;
import star.either;
import star.interpolation;
import star.random;
import star.periodic_function;
import star.ordered_map;
import star.string;
import star.color;
import star.image_processing;
import star.hash;
import star.string_view;
import star.thread;
import star.directives;
import star.asset_path;
import star.audio;
import star.map;
import star.set;
import star.io_device;
import star.ref_ptr;
import star.logging;
import star.output_proxy;
import star.listener;
import std;
import star.optional;
import star.lua;
import star.version;

import star.lua_components;
import star.lua_actor_movement_component;
import star.animated_part_set;
import star.drawable;
import star.animation;
import star.particle;
import star.mixer;
import star.networked_animator;
import star.entity_rendering_types;
import star.stat_collection;
import star.status_effect_database;
import star.status_controller;
import star.asset_source;
import star.assets;
import star.root_base;
import star.configuration;
import star.root;
import star.lua_game_converters;
import star.damage_types;
import star.world_geometry;
import star.status_types;
import star.damage;
import star.light_source;
import star.entity;
import star.tile_damage;
import star.interaction_types;
import star.item_descriptor;
import star.celestial_coordinate;
import star.quest_descriptor;
import star.interactive_entity;
import star.collision_block;
import star.tile_entity;
import star.tile_modification;
import star.lua_root;
import star.force_regions;
import star.world;
import star.status_effect_entity;

import star.liquids_database;
import star.networked_animator_lua_bindings;
import star.entity_lua_bindings;
import star.config_lua_bindings;
import star.status_controller_lua_bindings;

import star.net_element;
import star.net_element_containers;

import star.net_compatibility;
import star.algorithm;
namespace Star {

StatusController::StatusController(Root& root)
  : m_root(root), m_uniqueEffectMetadata([] { return UniqueEffectMetadata::create(); }),
    m_effectAnimators([&root] { return EffectAnimator::create(root); }),
    m_primaryScript(root),
    m_primaryAnimatorId(EffectAnimatorGroup::NullElementId) {}

std::expected<std::shared_ptr<StatusController>, StarException> StatusController::create(Root& root, Json const& config) {
  auto controller = std::shared_ptr<StatusController>(new StatusController(root));
  if (auto result = controller->configure(config); !result) return std::unexpected(std::move(result.error()));
  return controller;
}

std::expected<void, StarException> StatusController::configure(Json const& config) {
  auto stats = StatCollection::create(config);
  if (!stats) return std::unexpected(std::move(stats.error()));
  m_statCollection = std::move(*stats);
  auto properties = config.getObject("statusProperties", {});
  if (!properties) return std::unexpected(std::move(properties.error()));
  m_statusProperties.reset(std::move(*properties));
  m_statusProperties.setOverrides(
    [&statusProperties = m_statusProperties](DataStream& ds, NetCompatibilityRules rules) -> std::expected<void, StarException> {
      if (rules.version() <= 1) {
        auto result = ds << statusProperties.baseMap();
        if (!result) return std::unexpected(std::move(result.error()));
        return {};
      }
      return statusProperties.NetElementHashMap<String, Json>::netStore(ds, rules);
    },
    [&statusProperties = m_statusProperties](DataStream& ds, NetCompatibilityRules rules) -> std::expected<void, StarException> {
      if (rules.version() <= 1) {
        auto properties = ds.read<JsonObject>();
        if (!properties) return std::unexpected(std::move(properties.error()));
        statusProperties.reset(std::move(*properties));
        return {};
      }
      return statusProperties.NetElementHashMap<String, Json>::netLoad(ds, rules);
    },
    [&statusProperties = m_statusProperties](DataStream& ds, std::uint64_t fromVersion, NetCompatibilityRules rules) -> std::expected<bool, StarException> {
      if (rules.version() <= 1) {
        if (statusProperties.shouldWriteNetDelta(fromVersion, rules)) {
          auto result = ds << statusProperties.baseMap();
          if (!result) return std::unexpected(std::move(result.error()));
          return true;
        }
        return false;
      }
      return statusProperties.NetElementHashMap<String, Json>::writeNetDelta(ds, fromVersion, rules);
    },
    [&statusProperties = m_statusProperties](DataStream& ds, float interp, NetCompatibilityRules rules) -> std::expected<void, StarException> {
      if (rules.version() <= 1) {
        auto properties = ds.read<JsonObject>();
        if (!properties) return std::unexpected(std::move(properties.error()));
        statusProperties.reset(std::move(*properties));
        return {};
      }
      return statusProperties.NetElementHashMap<String, Json>::readNetDelta(ds, interp, rules);
    });
  auto minimumLiquidStatusEffectPercentage = config.getFloat("minimumLiquidStatusEffectPercentage");
  if (!minimumLiquidStatusEffectPercentage) return std::unexpected(std::move(minimumLiquidStatusEffectPercentage.error()));
  m_minimumLiquidStatusEffectPercentage = *minimumLiquidStatusEffectPercentage;
  auto appliesEnvironmentStatusEffects = config.getBool("appliesEnvironmentStatusEffects");
  if (!appliesEnvironmentStatusEffects) return std::unexpected(std::move(appliesEnvironmentStatusEffects.error()));
  m_appliesEnvironmentStatusEffects = *appliesEnvironmentStatusEffects;
  auto appliesWeatherStatusEffects = config.getBool("appliesWeatherStatusEffects");
  if (!appliesWeatherStatusEffects) return std::unexpected(std::move(appliesWeatherStatusEffects.error()));
  m_appliesWeatherStatusEffects = *appliesWeatherStatusEffects;
  auto timer = config.getFloat("environmentStatusEffectUpdateTimer", 0.15f);
  if (!timer) return std::unexpected(std::move(timer.error()));
  m_environmentStatusEffectUpdateTimer = GameTimer(*timer);
  auto animation = config.optString("primaryAnimationConfig");
  if (!animation) return std::unexpected(std::move(animation.error()));
  m_primaryAnimationConfig = std::move(*animation);
  auto scriptsJson = config.get("primaryScriptSources", JsonArray());
  if (!scriptsJson) return std::unexpected(std::move(scriptsJson.error()));
  auto scripts = jsonToStringList(*scriptsJson);
  if (!scripts) return std::unexpected(std::move(scripts.error()));
  if (auto result = m_primaryScript.setScripts(std::move(*scripts)); !result) return std::unexpected(std::move(result.error()));
  auto delta = config.getUInt("primaryScriptDelta", 1);
  if (!delta) return std::unexpected(std::move(delta.error()));
  m_primaryScript.setUpdateDelta(*delta);
  auto keepSteps = config.getUInt("keepDamageNotificationSteps", 120);
  if (!keepSteps) return std::unexpected(std::move(keepSteps.error()));
  m_recentHitsGiven.setHistoryLimit(*keepSteps);
  m_recentDamageGiven.setHistoryLimit(*keepSteps);
  m_recentDamageTaken.setHistoryLimit(*keepSteps);
  if (auto result = m_netGroup.addNetElement(*m_statCollection); !result) return std::unexpected(std::move(result.error()));
  if (auto result = m_netGroup.addNetElement(m_statusProperties); !result) return std::unexpected(std::move(result.error()));
  if (auto result = m_netGroup.addNetElement(m_parentDirectives); !result) return std::unexpected(std::move(result.error()));
  if (auto result = m_netGroup.addNetElement(m_uniqueEffectMetadata); !result) return std::unexpected(std::move(result.error()));
  if (auto result = m_netGroup.addNetElement(m_effectAnimators); !result) return std::unexpected(std::move(result.error()));
  m_toolUsageSuppressed.setCompatibilityVersion(12);
  if (auto result = m_netGroup.addNetElement(m_toolUsageSuppressed); !result) return std::unexpected(std::move(result.error()));
  if (m_primaryAnimationConfig) {
    auto animator = EffectAnimator::create(m_root, m_primaryAnimationConfig);
    if (!animator) return std::unexpected(std::move(animator.error()));
    auto id = m_effectAnimators.addNetElement(std::move(*animator));
    if (!id) return std::unexpected(std::move(id.error()));
    m_primaryAnimatorId = *id;
  }
  return {};
}

std::expected<Json, StarException> StatusController::diskStore() const {
  JsonObject resourceValues;
  JsonObject resourcesLocked;
  for (auto const& resourceName : resourceNames()) {
    resourceValues[resourceName] = resource(resourceName);
    auto locked = resourceLocked(resourceName);
    if (!locked) return std::unexpected(std::move(locked.error()));
    resourcesLocked[resourceName] = *locked;
  }

  JsonObject persistentEffectCategories;
  for (auto const& pair : m_persistentEffects) {
    List<PersistentStatusEffect> persistentEffects;
    persistentEffects.reserve(pair.second.statModifiers.size() + pair.second.uniqueEffects.size());
    std::ranges::transform(pair.second.statModifiers, std::back_inserter(persistentEffects), construct<PersistentStatusEffect>());
    std::ranges::transform(pair.second.uniqueEffects, std::back_inserter(persistentEffects), construct<PersistentStatusEffect>());
    JsonArray effects;
    for (auto const& effect : persistentEffects) {
      auto json = jsonFromPersistentStatusEffect(effect);
      if (!json) return std::unexpected(std::move(json.error()));
      effects.push_back(std::move(*json));
    }
    persistentEffectCategories[pair.first] = std::move(effects);
  }

  JsonArray ephemeralEffects;
  for (auto const& pair : m_uniqueEffects) {
    // Store ephemeral effects in the disk store based on remaining duration.
    // TODO: Need to store maximum duration as well in the store, otherwise the
    // effect will always appear "full" on reload (but just last less time)
    auto metadata = m_uniqueEffectMetadata.getNetElement(pair.second.metadataId);
    if (!metadata) return std::unexpected(std::move(metadata.error()));
    if ((*metadata)->duration) {
      ephemeralEffects.push_back(
        jsonFromEphemeralStatusEffect(EphemeralStatusEffect{pair.first, *(*metadata)->duration}));
    }
  }

  return JsonObject{
      {"statusProperties", m_statusProperties.baseMap()},
      {"persistentEffectCategories", std::move(persistentEffectCategories)},
      {"ephemeralEffects", std::move(ephemeralEffects)},
      {"resourceValues", std::move(resourceValues)},
      {"resourcesLocked", std::move(resourcesLocked)},
  };
}

std::expected<void, StarException> StatusController::diskLoad(Json const& store) {
  if (auto result = clearAllPersistentEffects(); !result) return std::unexpected(std::move(result.error()));
  if (auto result = clearEphemeralEffects(); !result) return std::unexpected(std::move(result.error()));
  auto properties = store.getObject("statusProperties");
  if (!properties) return std::unexpected(std::move(properties.error()));
  m_statusProperties.reset(std::move(*properties));
  auto categories = store.getObject("persistentEffectCategories", {});
  if (!categories) return std::unexpected(std::move(categories.error()));
  for (auto const& category : *categories) {
    auto values = category.second.toArray();
    if (!values) return std::unexpected(std::move(values.error()));
    List<PersistentStatusEffect> effects;
    for (auto const& value : *values) {
      auto effect = jsonToPersistentStatusEffect(value);
      if (!effect) return std::unexpected(std::move(effect.error()));
      effects.push_back(std::move(*effect));
    }
    if (auto result = addPersistentEffects(category.first, effects); !result) return std::unexpected(std::move(result.error()));
  }
  auto ephemeral = store.getArray("ephemeralEffects");
  if (!ephemeral) return std::unexpected(std::move(ephemeral.error()));
  List<EphemeralStatusEffect> effects;
  for (auto const& value : *ephemeral) {
    auto effect = jsonToEphemeralStatusEffect(value);
    if (!effect) return std::unexpected(std::move(effect.error()));
    effects.push_back(std::move(*effect));
  }
  if (auto result = addEphemeralEffects(effects); !result) return std::unexpected(std::move(result.error()));
  auto values = store.getObject("resourceValues", {});
  if (!values) return std::unexpected(std::move(values.error()));
  for (auto const& value : *values) {
    if (isResource(value.first)) {
      auto amount = value.second.toFloat();
      if (!amount) return std::unexpected(std::move(amount.error()));
      if (auto result = setResource(value.first, *amount); !result) return std::unexpected(std::move(result.error()));
    }
  }
  auto locks = store.getObject("resourcesLocked", {});
  if (!locks) return std::unexpected(std::move(locks.error()));
  for (auto const& value : *locks) {
    if (isResource(value.first)) {
      auto locked = value.second.toBool();
      if (!locked) return std::unexpected(std::move(locked.error()));
      if (auto result = setResourceLocked(value.first, *locked); !result) return std::unexpected(std::move(result.error()));
    }
  }
  return {};
}

Json StatusController::statusProperty(String const& name, Json const& def) const {
  return m_statusProperties.value(name, def);
}

void StatusController::setStatusProperty(String const& name, Json value) {
  if (value.isNull())
    m_statusProperties.remove(name);
  else
    m_statusProperties.set(name, value);
}

StringList StatusController::statNames() const {
  return m_statCollection->statNames();
}

float StatusController::stat(String const& statName) const {
  return m_statCollection->stat(statName);
}

bool StatusController::statPositive(String const& statName) const {
  return m_statCollection->statPositive(statName);
}

StringList StatusController::resourceNames() const {
  return m_statCollection->resourceNames();
}

bool StatusController::isResource(String const& resourceName) const {
  return m_statCollection->isResource(resourceName);
}

float StatusController::resource(String const& resourceName) const {
  return m_statCollection->resource(resourceName);
}

bool StatusController::resourcePositive(String const& resourceName) const {
  return m_statCollection->resourcePositive(resourceName);
}

std::expected<void, StarException> StatusController::setResource(String const& resourceName, float value) {
  return m_statCollection->setResource(resourceName, value);
}

std::expected<void, StarException> StatusController::modifyResource(String const& resourceName, float amount) {
  return m_statCollection->modifyResource(resourceName, amount);
}

float StatusController::giveResource(String const& resourceName, float amount) {
  return m_statCollection->giveResource(resourceName, amount);
}

std::expected<bool, StarException> StatusController::consumeResource(String const& resourceName, float amount) {
  auto consumed = m_statCollection->consumeResource(resourceName, amount);
  if (!consumed) return std::unexpected(std::move(consumed.error()));
  if (*consumed) {
    auto notified = m_primaryScript.invoke("notifyResourceConsumed", resourceName, amount);
    if (!notified && !notified.error().is<LuaException>()) return std::unexpected(std::move(notified.error()));
  }
  return *consumed;
}

std::expected<bool, StarException> StatusController::overConsumeResource(String const& resourceName, float amount) {
  auto consumed = m_statCollection->overConsumeResource(resourceName, amount);
  if (!consumed) return std::unexpected(std::move(consumed.error()));
  if (*consumed) {
    auto notified = m_primaryScript.invoke("notifyResourceConsumed", resourceName, amount);
    if (!notified && !notified.error().is<LuaException>()) return std::unexpected(std::move(notified.error()));
  }
  return *consumed;
}

std::expected<bool, StarException> StatusController::resourceLocked(String const& resourceName) const {
  return m_statCollection->resourceLocked(resourceName);
}

std::expected<void, StarException> StatusController::setResourceLocked(String const& resourceName, bool locked) {
  return m_statCollection->setResourceLocked(resourceName, locked);
}

std::expected<void, StarException> StatusController::resetResource(String const& resourceName) {
  return m_statCollection->resetResource(resourceName);
}

std::expected<void, StarException> StatusController::resetAllResources() {
  return m_statCollection->resetAllResources();
}

std::expected<std::optional<float>, StarException> StatusController::resourceMax(String const& resourceName) const {
  return m_statCollection->resourceMax(resourceName);
}

std::expected<std::optional<float>, StarException> StatusController::resourcePercentage(String const& resourceName) const {
  return m_statCollection->resourcePercentage(resourceName);
}

std::expected<float, StarException> StatusController::setResourcePercentage(String const& resourceName, float resourcePercentage) {
  return m_statCollection->setResourcePercentage(resourceName, resourcePercentage);
}

std::expected<float, StarException> StatusController::modifyResourcePercentage(String const& resourceName, float resourcePercentage) {
  return m_statCollection->modifyResourcePercentage(resourceName, resourcePercentage);
}

List<PersistentStatusEffect> StatusController::getPersistentEffects(String const& statEffectCategory) const {
  // Keep the category snapshot's hash-set iteration order in the returned list.
  auto category = m_persistentEffects.maybe(statEffectCategory).value_or(PersistentEffectCategory{});
  List<PersistentStatusEffect> persistentEffects;
  persistentEffects.reserve(category.statModifiers.size() + category.uniqueEffects.size());
  std::ranges::transform(category.statModifiers, std::back_inserter(persistentEffects), construct<PersistentStatusEffect>());
  std::ranges::transform(category.uniqueEffects, std::back_inserter(persistentEffects), construct<PersistentStatusEffect>());
  return persistentEffects;
}

std::expected<void, StarException> StatusController::addPersistentEffect(
    String const& statusEffectCategory, PersistentStatusEffect const& persistentEffect) {
  return addPersistentEffects(statusEffectCategory, {persistentEffect});
}

std::expected<void, StarException> StatusController::addPersistentEffects(
    String const& statusEffectCategory, List<PersistentStatusEffect> const& effectList) {
  auto& persistentEffectCategory = m_persistentEffects[statusEffectCategory];
  if (!persistentEffectCategory.modifierEffectsGroupId) {
    auto id = m_statCollection->addStatModifierGroup();
    if (!id) return std::unexpected(std::move(id.error()));
    persistentEffectCategory.modifierEffectsGroupId = *id;
  }

  for (auto const& effect : effectList) {
    if (std::holds_alternative<StatModifier>(effect))
      persistentEffectCategory.statModifiers.push_back(std::get<StatModifier>(effect));
    else if (std::holds_alternative<UniqueStatusEffect>(effect))
      persistentEffectCategory.uniqueEffects.add(std::get<UniqueStatusEffect>(effect));
  }
  if (auto result = m_statCollection->setStatModifierGroup(*persistentEffectCategory.modifierEffectsGroupId, persistentEffectCategory.statModifiers); !result) return std::unexpected(std::move(result.error()));

  return updatePersistentUniqueEffects();
}

std::expected<void, StarException> StatusController::setPersistentEffects(
    String const& statusEffectCategory, List<PersistentStatusEffect> const& effectList) {
  if (effectList.empty()) {
    if (auto groupId = m_persistentEffects[statusEffectCategory].modifierEffectsGroupId)
      m_statCollection->removeStatModifierGroup(*groupId);
    m_persistentEffects.remove(statusEffectCategory);

    return updatePersistentUniqueEffects();
  } else {
    auto& persistentEffectCategory = m_persistentEffects[statusEffectCategory];
    persistentEffectCategory.statModifiers.clear();
    persistentEffectCategory.uniqueEffects.clear();
    return addPersistentEffects(statusEffectCategory, effectList);
  }
}

std::expected<void, StarException> StatusController::clearPersistentEffects(String const& statusEffectCategory) {
  return setPersistentEffects(statusEffectCategory, {});
}

std::expected<void, StarException> StatusController::clearAllPersistentEffects() {
  for (auto const& category : m_persistentEffects.keys()) {
    if (auto result = clearPersistentEffects(category); !result) return std::unexpected(std::move(result.error()));
  }
  return {};
}

std::expected<void, StarException> StatusController::addEphemeralEffect(EphemeralStatusEffect const& effect, std::optional<EntityId> sourceEntityId) {
  return addEphemeralEffects({effect}, sourceEntityId);
}

std::expected<void, StarException> StatusController::addEphemeralEffects(List<EphemeralStatusEffect> const& effectList, std::optional<EntityId> sourceEntityId) {
  for (auto const& effect : effectList) {
    if (auto existingEffect = m_uniqueEffects.find_ref(effect.uniqueEffect)) {
      auto metadata = m_uniqueEffectMetadata.getNetElement(existingEffect->get().metadataId);
      if (!metadata) return std::unexpected(std::move(metadata.error()));
      if ((*metadata)->duration) {
        auto defaultDuration = defaultUniqueEffectDuration(effect.uniqueEffect);
        if (!defaultDuration) return std::unexpected(std::move(defaultDuration.error()));
        auto newDuration = effect.duration.value_or(*defaultDuration);
        if (newDuration > *(*metadata)->duration) {
          (*metadata)->sourceEntityId.set(sourceEntityId);
          (*metadata)->duration = newDuration;
        }
        (*metadata)->maxDuration.set(std::max((*metadata)->maxDuration.get(), newDuration));
      }
    } else {
      auto defaultDuration = defaultUniqueEffectDuration(effect.uniqueEffect);
      if (!defaultDuration) return std::unexpected(std::move(defaultDuration.error()));
      auto added = addUniqueEffect(effect.uniqueEffect, effect.duration.value_or(*defaultDuration), sourceEntityId);
      if (!added) return std::unexpected(std::move(added.error()));
    }
  }
  return {};
}

std::expected<bool, StarException> StatusController::removeEphemeralEffect(UniqueStatusEffect const& effect) {
  if (auto uniqueEffect = m_uniqueEffects.find_ref(effect)) {
    auto metadata = m_uniqueEffectMetadata.getNetElement(uniqueEffect->get().metadataId);
    if (!metadata) return std::unexpected(std::move(metadata.error()));
    if ((*metadata)->duration) {
      if (auto result = removeUniqueEffect(effect); !result) return std::unexpected(std::move(result.error()));
      return true;
    }
  }
  return false;
}

std::expected<void, StarException> StatusController::clearEphemeralEffects() {
  for (auto const& key : m_uniqueEffects.keys()) {
    if (auto result = removeEphemeralEffect(key); !result) return std::unexpected(std::move(result.error()));
  }
  return {};
}

bool StatusController::appliesEnvironmentStatusEffects() const {
  return m_appliesEnvironmentStatusEffects;
}

void StatusController::setAppliesEnvironmentStatusEffects(bool appliesEnvironmentStatusEffects) {
  m_appliesEnvironmentStatusEffects = appliesEnvironmentStatusEffects;
}

ActiveUniqueStatusEffectSummary StatusController::activeUniqueStatusEffectSummary() const {
  ActiveUniqueStatusEffectSummary summary;
  for (auto const& metadata : m_uniqueEffectMetadata.netElements()) {
    if (metadata->duration)
      summary.push_back({metadata->effect, *metadata->duration / metadata->maxDuration.get()});
    else
      summary.push_back({metadata->effect, 1.0f});
  }
  return summary;
}

bool StatusController::uniqueStatusEffectActive(String const& effectName) const {
  for (auto const& metadata : m_uniqueEffectMetadata.netElements()) {
    if (metadata->effect == effectName)
      return true;
  }

  return false;
}

const Directives& StatusController::primaryDirectives() const {
  return m_primaryDirectives;
}

void StatusController::setPrimaryDirectives(Directives const& directives) {
  m_primaryDirectives = directives;
}

std::expected<List<DamageNotification>, StarException> StatusController::applyDamageRequest(DamageRequest const& damageRequest) {
  auto notifications = m_primaryScript.invoke<List<DamageNotification>>("applyDamageRequest", damageRequest);
  if (!notifications) {
    if (notifications.error().is<LuaException>()) return List<DamageNotification>{};
    return std::unexpected(std::move(notifications.error()));
  }
  if (*notifications) {
    for (auto const& notification : **notifications) m_recentDamageTaken.add(notification);
    return std::move(**notifications);
  }
  return List<DamageNotification>{};
}

void StatusController::hitOther(EntityId targetEntityId, DamageRequest damageRequest) {
  m_recentHitsGiven.add({targetEntityId, std::move(damageRequest)});
}

void StatusController::damagedOther(DamageNotification damageNotification) {
  m_recentDamageGiven.add(std::move(damageNotification));
}

List<DamageNotification> StatusController::pullSelfDamageNotifications() {
  return std::exchange(m_pendingSelfDamageNotifications, {});
}

std::expected<void, StarException> StatusController::applySelfDamageRequest(DamageRequest dr) {
  auto notifications = applyDamageRequest(std::move(dr));
  if (!notifications) return std::unexpected(std::move(notifications.error()));
  for (auto const& notification : *notifications) m_recentDamageTaken.add(notification);
  m_pendingSelfDamageNotifications.append_range(std::move(*notifications));
  return {};
}

std::pair<List<DamageNotification>, std::uint64_t> StatusController::damageTakenSince(std::uint64_t since) const {
  return m_recentDamageTaken.query(since);
}

std::pair<List<std::pair<EntityId, DamageRequest>>, std::uint64_t> StatusController::inflictedHitsSince(std::uint64_t since) const {
  return m_recentHitsGiven.query(since);
}

std::pair<List<DamageNotification>, std::uint64_t> StatusController::inflictedDamageSince(std::uint64_t since) const {
  return m_recentDamageGiven.query(since);
}

std::expected<void, StarException> StatusController::init(Entity& parentEntity, ActorMovementController& movementController) {
  if (auto result = uninit(); !result) return std::unexpected(std::move(result.error()));

  m_parentEntity = std::ref(parentEntity);
  m_movementController = std::ref(movementController);

  if (m_parentEntity->get().isMaster()) {
    if (auto result = initPrimaryScript(); !result) return std::unexpected(std::move(result.error()));
    for (auto& p : m_uniqueEffects.keys())
      if (auto effect = m_uniqueEffects.find_ref(p))
        if (auto result = initUniqueEffectScript(effect->get()); !result) return std::unexpected(std::move(result.error()));
  }

  m_environmentStatusEffectUpdateTimer.reset();
  return {};
}

std::expected<void, StarException> StatusController::uninit() {
  std::optional<StarException> error;
  for (auto const& key : m_uniqueEffects.keys()) {
    if (auto effect = m_uniqueEffects.find_ref(key)) {
      auto result = uninitUniqueEffectScript(effect->get());
      if (!result && !error) error = std::move(result.error());
    }
  }
  auto primary = uninitPrimaryScript();
  if (!primary && !error) error = std::move(primary.error());
  m_recentHitsGiven.reset();
  m_recentDamageGiven.reset();
  m_recentDamageTaken.reset();
  m_parentEntity.reset();
  m_movementController.reset();
  if (error) return std::unexpected(std::move(*error));
  return {};
}

void StatusController::initNetVersion(std::optional<std::reference_wrapper<NetElementVersion const>> version) {
  m_netGroup.initNetVersion(version);
}

std::expected<void, StarException> StatusController::netStore(DataStream& ds, NetCompatibilityRules rules) const {
  if (!checkWithRules(rules)) return {};
  return m_netGroup.netStore(ds, rules);
}

std::expected<void, StarException> StatusController::netLoad(DataStream& ds, NetCompatibilityRules rules) {
  if (!checkWithRules(rules)) return {};
  if (auto result = clearAllPersistentEffects(); !result) return std::unexpected(std::move(result.error()));
  if (auto result = clearEphemeralEffects(); !result) return std::unexpected(std::move(result.error()));
  return m_netGroup.netLoad(ds, rules);
}

std::expected<void, StarException> StatusController::enableNetInterpolation(float extrapolationHint) {
  return m_netGroup.enableNetInterpolation(extrapolationHint);
}

std::expected<void, StarException> StatusController::disableNetInterpolation() {
  return m_netGroup.disableNetInterpolation();
}

std::expected<void, StarException> StatusController::tickNetInterpolation(float dt) {
  return m_netGroup.tickNetInterpolation(dt);
}

std::expected<bool, StarException> StatusController::writeNetDelta(DataStream& ds, std::uint64_t fromVersion, NetCompatibilityRules rules) const {
  return m_netGroup.writeNetDelta(ds, fromVersion, rules);
}

std::expected<void, StarException> StatusController::readNetDelta(DataStream& ds, float interpolationTime, NetCompatibilityRules rules) {
  return m_netGroup.readNetDelta(ds, interpolationTime, rules);
}

std::expected<void, StarException> StatusController::blankNetDelta(float interpolationTime) {
  return m_netGroup.blankNetDelta(interpolationTime);
}

std::expected<void, StarException> StatusController::tickMaster(float dt) {
  m_statCollection->tickMaster(dt);
  m_recentHitsGiven.tick(1);
  m_recentDamageGiven.tick(1);
  m_recentDamageTaken.tick(1);
  bool statusImmune = statPositive("statusImmunity");
  if (!statusImmune && m_movementController->get().liquidPercentage() > m_minimumLiquidStatusEffectPercentage) {
    auto database = m_root.liquidsDatabase();
    if (!database) return std::unexpected(std::move(database.error()));
    if (auto settings = (*database)->liquidSettings(m_movementController->get().liquidId())) {
      for (auto const& value : settings->statusEffects) {
        auto effect = jsonToEphemeralStatusEffect(value);
        if (!effect) return std::unexpected(std::move(effect.error()));
        if (auto result = addEphemeralEffect(*effect, {}); !result) return std::unexpected(std::move(result.error()));
      }
    }
  }
  if (m_environmentStatusEffectUpdateTimer.wrapTick(m_root.timing().timestep)) {
    PolyF collisionBody = m_movementController->get().collisionBody();
    auto world = m_parentEntity->get().world();
    if (!world) return std::unexpected(std::move(world.error()));
    List<PersistentStatusEffect> entityEffects;
    if (!statusImmune) {
      auto visited = world->get().forEachEntity(collisionBody.boundBox(),
        [world = *world, collisionBody, &entityEffects](std::shared_ptr<Entity> const& entity) -> std::expected<void, StarException> {
          if (auto effectEntity = as<StatusEffectEntity>(entity)) {
            auto area = effectEntity->statusEffectArea();
            area.translate(effectEntity->position());
            auto intersects = world.get().geometry().polyIntersectsPoly(area, collisionBody);
            if (!intersects) return std::unexpected(std::move(intersects.error()));
            if (*intersects) {
              auto effects = effectEntity->statusEffects();
              if (!effects) return std::unexpected(std::move(effects.error()));
              entityEffects.append_range(std::move(*effects));
            }
          }
          return {};
        });
      if (!visited) return std::unexpected(std::move(visited.error()));
    }
    if (auto result = setPersistentEffects("entities", entityEffects); !result) return std::unexpected(std::move(result.error()));
    if (!statusImmune && m_appliesEnvironmentStatusEffects) {
      auto values = world->get().environmentStatusEffects(m_parentEntity->get().position());
      if (!values) return std::unexpected(std::move(values.error()));
      List<PersistentStatusEffect> effects;
      for (auto const& value : *values) {
        auto effect = jsonToPersistentStatusEffect(value);
        if (!effect) return std::unexpected(std::move(effect.error()));
        effects.push_back(std::move(*effect));
      }
      if (auto result = setPersistentEffects("environment", effects); !result) return std::unexpected(std::move(result.error()));
    }
    if (!statusImmune && m_appliesWeatherStatusEffects) {
      auto values = world->get().weatherStatusEffects(m_parentEntity->get().position());
      if (!values) return std::unexpected(std::move(values.error()));
      List<EphemeralStatusEffect> effects;
      for (auto const& value : *values) {
        auto effect = jsonToEphemeralStatusEffect(value);
        if (!effect) return std::unexpected(std::move(effect.error()));
        effects.push_back(std::move(*effect));
      }
      if (auto result = addEphemeralEffects(effects, {}); !result) return std::unexpected(std::move(result.error()));
    }
  }
  auto primary = m_primaryScript.update(m_primaryScript.updateDt(dt));
  if (!primary && !primary.error().is<LuaException>()) return std::unexpected(std::move(primary.error()));
  for (auto& entry : m_uniqueEffects) {
    auto updated = entry.second.script.update(entry.second.script.updateDt(dt));
    // Each failing effect has already been logged and disabled by its script component.
    if (!updated && !updated.error().is<LuaException>()) return std::unexpected(std::move(updated.error()));
    auto metadata = m_uniqueEffectMetadata.getNetElement(entry.second.metadataId);
    if (!metadata) return std::unexpected(std::move(metadata.error()));
    if ((*metadata)->duration) *(*metadata)->duration -= dt;
  }
  for (auto const& key : m_uniqueEffects.keys()) {
    auto effect = m_uniqueEffects.get(key);
    if (!effect) return std::unexpected(std::move(effect.error()));
    auto metadata = m_uniqueEffectMetadata.getNetElement(effect->get().metadataId);
    if (!metadata) return std::unexpected(std::move(metadata.error()));
    if (((*metadata)->duration && *(*metadata)->duration <= 0.0f)
        || (((*metadata)->duration && statPositive("statusImmunity"))
          || (effect->get().effectConfig.blockingStat && statPositive(*effect->get().effectConfig.blockingStat)))) {
      if (auto result = removeUniqueEffect(key); !result) return std::unexpected(std::move(result.error()));
    }
  }
  DirectivesGroup directives;
  directives.append(m_primaryDirectives);
  for (auto const& entry : m_uniqueEffects) directives.append(entry.second.parentDirectives);
  m_parentDirectives.set(std::move(directives));
  return updateAnimators(dt);
}

std::expected<void, StarException> StatusController::tickSlave(float dt) {
  m_statCollection->tickSlave(dt);
  return updateAnimators(dt);
}

const DirectivesGroup& StatusController::parentDirectives() const {
  return m_parentDirectives.get();
}

std::expected<List<Drawable>, StarException> StatusController::drawables(float interpolation) const {
  List<Drawable> output;
  for (auto const& animator : m_effectAnimators.netElements()) {
    auto sample = animator->animator->sample(interpolation);
    auto values = sample.drawables(m_movementController->get().position());
    if (!values) return std::unexpected(std::move(values.error()));
    output.append_range(std::move(*values));
  }
  return output;
}

std::expected<List<LightSource>, StarException> StatusController::lightSources(float interpolation) const {
  List<LightSource> output;
  for (auto const& animator : m_effectAnimators.netElements()) {
    auto sample = animator->animator->sample(interpolation);
    auto values = sample.lightSources(m_movementController->get().position());
    if (!values) return std::unexpected(std::move(values.error()));
    output.append_range(std::move(*values));
  }
  return output;
}

std::expected<List<OverheadBar>, StarException> StatusController::overheadBars() {
  auto json = m_primaryScript.invoke<JsonArray>("overheadBars");
  if (!json) {
    if (json.error().is<LuaException>()) return List<OverheadBar>{};
    return std::unexpected(std::move(json.error()));
  }
  List<OverheadBar> bars;
  if (*json) {
    for (auto const& value : **json) {
      auto bar = OverheadBar::create(value);
      if (!bar) return std::unexpected(std::move(bar.error()));
      bars.push_back(std::move(*bar));
    }
  }
  return bars;
}

bool StatusController::toolUsageSuppressed() const {
  return m_toolUsageSuppressed.get();
}

List<std::shared_ptr<AudioInstance>> StatusController::pullNewAudios() {
  List<std::shared_ptr<AudioInstance>> newAudios;
  for (auto const& animator : m_effectAnimators.netElements())
    newAudios.append_range(animator->dynamicTarget.pullNewAudios());
  return newAudios;
}

List<Particle> StatusController::pullNewParticles() {
  List<Particle> newParticles;
  for (auto const& animator : m_effectAnimators.netElements())
    newParticles.append_range(animator->dynamicTarget.pullNewParticles());
  return newParticles;
}

std::expected<std::optional<ChainableJsonMessageResponse>, StarException> StatusController::receiveMessage(String const& message, bool localMessage, JsonArray const& args) {
  auto result = m_primaryScript.handleMessage(message, localMessage, args);
  if (!result) {
    if (!result.error().is<LuaException>()) return std::unexpected(std::move(result.error()));
    result = std::optional<ChainableJsonMessageResponse>{};
  }
  for (auto& entry : m_uniqueEffects) {
    auto response = entry.second.script.handleMessage(message, localMessage, args);
    if (!response) {
      if (!response.error().is<LuaException>()) return std::unexpected(std::move(response.error()));
      continue;
    }
    if (!*result) *result = std::move(*response);
  }
  return result;
}

StatusController::EffectAnimator::EffectAnimator(Root& root) : m_root(root) {}

std::expected<std::shared_ptr<StatusController::EffectAnimator>, StarException> StatusController::EffectAnimator::create(Root& root, std::optional<String> config) {
  auto result = std::make_shared<EffectAnimator>(root);
  result->animationConfig = std::move(config);
  if (result->animationConfig) {
    auto animator = NetworkedAnimator::create(root, *result->animationConfig);
    if (!animator) return std::unexpected(std::move(animator.error()));
    result->animator = std::move(*animator);
  } else {
    auto animator = NetworkedAnimator::create(root);
    if (!animator) return std::unexpected(std::move(animator.error()));
    result->animator = std::move(*animator);
  }
  return result;
}

void StatusController::EffectAnimator::initNetVersion(std::optional<std::reference_wrapper<NetElementVersion const>> version) {
  animator->initNetVersion(version);
}

std::expected<void, StarException> StatusController::EffectAnimator::netStore(DataStream& ds, NetCompatibilityRules rules) const {
  if (!checkWithRules(rules)) return {};
  if (auto result = ds.write(animationConfig); !result) return std::unexpected(std::move(result.error()));
  return animator->netStore(ds, rules);
}

std::expected<void, StarException> StatusController::EffectAnimator::netLoad(DataStream& ds, NetCompatibilityRules rules) {
  if (!checkWithRules(rules)) return {};
  if (auto result = ds.read(animationConfig); !result) return std::unexpected(std::move(result.error()));
  if (animationConfig) {
    auto created = NetworkedAnimator::create(m_root, *animationConfig);
    if (!created) return std::unexpected(std::move(created.error()));
    animator = std::move(*created);
  } else {
    auto created = NetworkedAnimator::create(m_root);
    if (!created) return std::unexpected(std::move(created.error()));
    animator = std::move(*created);
  }
  return animator->netLoad(ds, rules);
}

std::expected<void, StarException> StatusController::EffectAnimator::enableNetInterpolation(float extrapolationHint) {
  return animator->enableNetInterpolation(extrapolationHint);
}

std::expected<void, StarException> StatusController::EffectAnimator::disableNetInterpolation() {
  return animator->disableNetInterpolation();
}

std::expected<void, StarException> StatusController::EffectAnimator::tickNetInterpolation(float dt) {
  return animator->tickNetInterpolation(dt);
}

std::expected<bool, StarException> StatusController::EffectAnimator::writeNetDelta(DataStream& ds, std::uint64_t fromVersion, NetCompatibilityRules rules) const {
  return animator->writeNetDelta(ds, fromVersion, rules);
}

std::expected<void, StarException> StatusController::EffectAnimator::readNetDelta(DataStream& ds, float interpolationTime, NetCompatibilityRules rules) {
  return animator->readNetDelta(ds, interpolationTime, rules);
}

std::expected<void, StarException> StatusController::EffectAnimator::blankNetDelta(float interpolationTime) {
  return animator->blankNetDelta(interpolationTime);
}

StatusController::UniqueEffectMetadata::UniqueEffectMetadata() {
  durationNetState.setFixedPointBase(0.01f);
  durationNetState.setInterpolator(lerp<float, float>);
}

std::expected<std::shared_ptr<StatusController::UniqueEffectMetadata>, StarException> StatusController::UniqueEffectMetadata::create(UniqueStatusEffect effect, std::optional<float> duration, std::optional<EntityId> sourceEntityId) {
  auto result = std::make_shared<UniqueEffectMetadata>();
  if (auto registered = result->addNetElement(result->durationNetState); !registered) return std::unexpected(std::move(registered.error()));
  if (auto registered = result->addNetElement(result->maxDuration); !registered) return std::unexpected(std::move(registered.error()));
  if (auto registered = result->addNetElement(result->sourceEntityId); !registered) return std::unexpected(std::move(registered.error()));
  result->effect = std::move(effect);
  result->duration = duration;
  result->maxDuration.set(duration.value_or(0.0f));
  result->sourceEntityId.set(sourceEntityId);
  return result;
}

std::expected<void, StarException> StatusController::UniqueEffectMetadata::netElementsNeedLoad(bool) {
  duration = durationNetState.get() >= 0.0f ? std::optional<float>(durationNetState.get()) : std::optional<float>();
  return {};
}

std::expected<void, StarException> StatusController::UniqueEffectMetadata::netElementsNeedStore() {
  durationNetState.set(duration ? *duration : -1.0f);
  return {};
}

std::expected<void, StarException> StatusController::updateAnimators(float dt) {
  auto world = m_parentEntity->get().world();
  if (!world) return std::unexpected(std::move(world.error()));
  for (auto const& animator : m_effectAnimators.netElements()) {
    if (world->get().isServer()) {
      if (auto result = animator->animator->update(dt, std::nullopt); !result) return std::unexpected(std::move(result.error()));
    } else {
      if (auto result = animator->animator->update(dt, std::ref(animator->dynamicTarget)); !result) return std::unexpected(std::move(result.error()));
      animator->dynamicTarget.updatePosition(m_movementController->get().position());
    }
  }
  return {};
}

std::expected<void, StarException> StatusController::updatePersistentUniqueEffects() {
  Set<UniqueStatusEffect> active;
  for (auto&& category : m_persistentEffects) {
    for (auto it = category.second.uniqueEffects.begin(); it != category.second.uniqueEffects.end();) {
      auto const& name = *it++;
      if (auto existing = m_uniqueEffects.find_ref(name)) {
        auto metadata = m_uniqueEffectMetadata.getNetElement(existing->get().metadataId);
        if (!metadata) return std::unexpected(std::move(metadata.error()));
        (*metadata)->duration.reset();
        active.add(name);
      } else {
        auto added = addUniqueEffect(name, {}, {});
        if (!added) return std::unexpected(std::move(added.error()));
        if (*added) active.add(name);
        else category.second.uniqueEffects.remove(name);
      }
    }
  }
  for (auto const& key : m_uniqueEffects.keys()) {
    auto effect = m_uniqueEffects.get(key);
    if (!effect) return std::unexpected(std::move(effect.error()));
    auto metadata = m_uniqueEffectMetadata.getNetElement(effect->get().metadataId);
    if (!metadata) return std::unexpected(std::move(metadata.error()));
    if (!(*metadata)->duration && !active.contains(key)) {
      if (auto result = removeUniqueEffect(key); !result) return std::unexpected(std::move(result.error()));
    }
  }
  return {};
}

std::expected<float, StarException> StatusController::defaultUniqueEffectDuration(UniqueStatusEffect const& effect) const {
  auto database = m_root.statusEffectDatabase();
  if (!database) return std::unexpected(std::move(database.error()));
  auto config = (*database)->uniqueEffectConfig(effect);
  if (!config) return std::unexpected(std::move(config.error()));
  return config->defaultDuration;
}

std::expected<bool, StarException> StatusController::addUniqueEffect(UniqueStatusEffect const& effect, std::optional<float> duration, std::optional<EntityId> sourceEntityId) {
  auto database = m_root.statusEffectDatabase();
  if (!database) return std::unexpected(std::move(database.error()));
  if (!(*database)->isUniqueEffect(effect)) {
    if (auto result = m_root.logger().warn("Unique status effect '{}' not found in status effect database", effect); !result) return std::unexpected(std::move(result.error()));
    return false;
  }
  auto config = (*database)->uniqueEffectConfig(effect);
  if (!config) return std::unexpected(std::move(config.error()));
  if ((duration && statPositive("statusImmunity")) || (config->blockingStat && statPositive(*config->blockingStat))) return false;
  auto& instance = m_uniqueEffects.try_emplace(effect, m_root).first->second;
  instance.effectConfig = std::move(*config);
  if (auto result = instance.script.setScripts(instance.effectConfig.scripts); !result) {
    m_uniqueEffects.remove(effect);
    return std::unexpected(std::move(result.error()));
  }
  instance.script.setUpdateDelta(instance.effectConfig.scriptDelta);
  auto createdMetadata = UniqueEffectMetadata::create(effect, duration, sourceEntityId);
  if (!createdMetadata) { m_uniqueEffects.remove(effect); return std::unexpected(std::move(createdMetadata.error())); }
  auto metadata = m_uniqueEffectMetadata.addNetElement(std::move(*createdMetadata));
  if (!metadata) { m_uniqueEffects.remove(effect); return std::unexpected(std::move(metadata.error())); }
  instance.metadataId = *metadata;
  instance.animatorId = EffectAnimatorGroup::NullElementId;
  if (instance.effectConfig.animationConfig) {
    auto animator = EffectAnimator::create(m_root, instance.effectConfig.animationConfig);
    if (!animator) {
      m_uniqueEffectMetadata.removeNetElement(instance.metadataId);
      m_uniqueEffects.remove(effect);
      return std::unexpected(std::move(animator.error()));
    }
    auto id = m_effectAnimators.addNetElement(std::move(*animator));
    if (!id) {
      m_uniqueEffectMetadata.removeNetElement(instance.metadataId);
      m_uniqueEffects.remove(effect);
      return std::unexpected(std::move(id.error()));
    }
    instance.animatorId = *id;
  }
  instance.toolUsageSuppressed = false;
  if (m_parentEntity) {
    if (auto result = initUniqueEffectScript(instance); !result) {
      auto cleanup = uninitUniqueEffectScript(instance);
      m_uniqueEffectMetadata.removeNetElement(instance.metadataId);
      if (instance.animatorId != EffectAnimatorGroup::NullElementId) m_effectAnimators.removeNetElement(instance.animatorId);
      m_uniqueEffects.remove(effect);
      if (!cleanup) return std::unexpected(cleanup.error().withContext(result.error().what()));
      return std::unexpected(std::move(result.error()));
    }
  }
  return true;
}

std::expected<void, StarException> StatusController::removeUniqueEffect(UniqueStatusEffect const& effect) {
  auto instance = m_uniqueEffects.get(effect);
  if (!instance) return std::unexpected(std::move(instance.error()));
  auto expired = instance->get().script.invoke("onExpire");
  auto uninitialized = uninitUniqueEffectScript(instance->get());
  m_uniqueEffectMetadata.removeNetElement(instance->get().metadataId);
  if (instance->get().animatorId != EffectAnimatorGroup::NullElementId)
    m_effectAnimators.removeNetElement(instance->get().animatorId);
  m_uniqueEffects.remove(effect);
  if (!expired && !expired.error().is<LuaException>()) return std::unexpected(std::move(expired.error()));
  if (!uninitialized) return std::unexpected(std::move(uninitialized.error()));
  return {};
}

std::expected<void, StarException> StatusController::initPrimaryScript() {
  auto statusCallbacks = LuaBindings::makeStatusControllerCallbacks(*this);
  if (!statusCallbacks) return std::unexpected(std::move(statusCallbacks.error()));
  if (auto result = m_primaryScript.addCallbacks("status", std::move(*statusCallbacks)); !result) return std::unexpected(std::move(result.error()));
  auto entityCallbacks = LuaBindings::makeEntityCallbacks(m_parentEntity->get());
  if (!entityCallbacks) return std::unexpected(std::move(entityCallbacks.error()));
  if (auto result = m_primaryScript.addCallbacks("entity", std::move(*entityCallbacks)); !result) return std::unexpected(std::move(result.error()));
  if (m_primaryAnimatorId != EffectAnimatorGroup::NullElementId) {
    auto animator = m_effectAnimators.getNetElement(m_primaryAnimatorId);
    if (!animator) return std::unexpected(std::move(animator.error()));
    auto callbacks = LuaBindings::makeNetworkedAnimatorCallbacks(*(*animator)->animator);
    if (!callbacks) return std::unexpected(std::move(callbacks.error()));
    if (auto result = m_primaryScript.addCallbacks("animator", std::move(*callbacks)); !result) return std::unexpected(std::move(result.error()));
  }
  if (auto result = m_primaryScript.addActorMovementCallbacks(m_movementController->get()); !result) return std::unexpected(std::move(result.error()));
  auto world = m_parentEntity->get().world();
  if (!world) return std::unexpected(std::move(world.error()));
  auto initialized = m_primaryScript.init(world->get());
  if (!initialized && !initialized.error().is<LuaException>()) return std::unexpected(std::move(initialized.error()));
  return {};
}

std::expected<void, StarException> StatusController::uninitPrimaryScript() {
  auto uninitialized = m_primaryScript.uninit();
  std::optional<StarException> error;
  if (!uninitialized && !uninitialized.error().is<LuaException>()) error = std::move(uninitialized.error());
  if (auto result = m_primaryScript.removeCallbacks("status"); !result && !error) error = std::move(result.error());
  if (auto result = m_primaryScript.removeCallbacks("entity"); !result && !error) error = std::move(result.error());
  if (auto result = m_primaryScript.removeCallbacks("animator"); !result && !error) error = std::move(result.error());
  if (auto result = m_primaryScript.removeActorMovementCallbacks(); !result && !error) error = std::move(result.error());
  if (error) return std::unexpected(std::move(*error));
  return {};
}

std::expected<void, StarException> StatusController::initUniqueEffectScript(UniqueEffectInstance& uniqueEffect) {
  auto effectCallbacks = makeUniqueEffectCallbacks(uniqueEffect);
  if (!effectCallbacks) return std::unexpected(std::move(effectCallbacks.error()));
  if (auto result = uniqueEffect.script.addCallbacks("effect", std::move(*effectCallbacks)); !result) return std::unexpected(std::move(result.error()));
  auto statusCallbacks = LuaBindings::makeStatusControllerCallbacks(*this);
  if (!statusCallbacks) return std::unexpected(std::move(statusCallbacks.error()));
  if (auto result = uniqueEffect.script.addCallbacks("status", std::move(*statusCallbacks)); !result) return std::unexpected(std::move(result.error()));
  auto configCallbacks = LuaBindings::makeConfigCallbacks([&uniqueEffect](String const& name, Json const& def) {
    return uniqueEffect.effectConfig.effectConfig.query(name, def);
  });
  if (!configCallbacks) return std::unexpected(std::move(configCallbacks.error()));
  if (auto result = uniqueEffect.script.addCallbacks("config", std::move(*configCallbacks)); !result) return std::unexpected(std::move(result.error()));
  auto entityCallbacks = LuaBindings::makeEntityCallbacks(m_parentEntity->get());
  if (!entityCallbacks) return std::unexpected(std::move(entityCallbacks.error()));
  if (auto result = uniqueEffect.script.addCallbacks("entity", std::move(*entityCallbacks)); !result) return std::unexpected(std::move(result.error()));
  if (uniqueEffect.animatorId != EffectAnimatorGroup::NullElementId) {
    auto animator = m_effectAnimators.getNetElement(uniqueEffect.animatorId);
    if (!animator) return std::unexpected(std::move(animator.error()));
    auto callbacks = LuaBindings::makeNetworkedAnimatorCallbacks(*(*animator)->animator);
    if (!callbacks) return std::unexpected(std::move(callbacks.error()));
    if (auto result = uniqueEffect.script.addCallbacks("animator", std::move(*callbacks)); !result) return std::unexpected(std::move(result.error()));
  }
  if (auto result = uniqueEffect.script.addActorMovementCallbacks(m_movementController->get()); !result) return std::unexpected(std::move(result.error()));
  auto world = m_parentEntity->get().world();
  if (!world) return std::unexpected(std::move(world.error()));
  auto initialized = uniqueEffect.script.init(world->get());
  if (!initialized && !initialized.error().is<LuaException>()) return std::unexpected(std::move(initialized.error()));
  return {};
}

std::expected<void, StarException> StatusController::uninitUniqueEffectScript(UniqueEffectInstance& uniqueEffect) {
  auto uninitialized = uniqueEffect.script.uninit();
  std::optional<StarException> error;
  if (!uninitialized && !uninitialized.error().is<LuaException>()) error = std::move(uninitialized.error());
  if (auto result = uniqueEffect.script.removeCallbacks("effect"); !result && !error) error = std::move(result.error());
  if (auto result = uniqueEffect.script.removeCallbacks("status"); !result && !error) error = std::move(result.error());
  if (auto result = uniqueEffect.script.removeCallbacks("config"); !result && !error) error = std::move(result.error());
  if (auto result = uniqueEffect.script.removeCallbacks("entity"); !result && !error) error = std::move(result.error());
  if (auto result = uniqueEffect.script.removeCallbacks("animator"); !result && !error) error = std::move(result.error());
  if (auto result = uniqueEffect.script.removeActorMovementCallbacks(); !result && !error) error = std::move(result.error());
  for (auto group : uniqueEffect.modifierGroups) m_statCollection->removeStatModifierGroup(group);
  uniqueEffect.modifierGroups.clear();
  if (error) return std::unexpected(std::move(*error));
  return {};
}

std::expected<LuaCallbacks, StarException> StatusController::makeUniqueEffectCallbacks(UniqueEffectInstance& uniqueEffect) {
  LuaCallbacks callbacks;
  if (auto result = callbacks.registerCallback("name", [&controller = *this, &uniqueEffect]() -> std::expected<UniqueStatusEffect, StarException> {
    auto metadata = controller.m_uniqueEffectMetadata.getNetElement(uniqueEffect.metadataId);
    if (!metadata) return std::unexpected(std::move(metadata.error()));
    return (*metadata)->effect;
  }); !result) return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.registerCallback("duration", [&controller = *this, &uniqueEffect]() -> std::expected<std::optional<float>, StarException> {
    auto metadata = controller.m_uniqueEffectMetadata.getNetElement(uniqueEffect.metadataId);
    if (!metadata) return std::unexpected(std::move(metadata.error()));
    return (*metadata)->duration;
  }); !result) return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.registerCallback("modifyDuration", [&controller = *this, &uniqueEffect](float duration) -> std::expected<void, StarException> {
    auto metadata = controller.m_uniqueEffectMetadata.getNetElement(uniqueEffect.metadataId);
    if (!metadata) return std::unexpected(std::move(metadata.error()));
    if ((*metadata)->duration) *(*metadata)->duration += duration;
    return {};
  }); !result) return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.registerCallback("setDuration", [&controller = *this, &uniqueEffect](float duration) -> std::expected<void, StarException> {
    auto metadata = controller.m_uniqueEffectMetadata.getNetElement(uniqueEffect.metadataId);
    if (!metadata) return std::unexpected(std::move(metadata.error()));
    if ((*metadata)->duration) *(*metadata)->duration = duration;
    return {};
  }); !result) return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.registerCallback("expire", [&controller = *this, &uniqueEffect]() -> std::expected<void, StarException> {
    auto metadata = controller.m_uniqueEffectMetadata.getNetElement(uniqueEffect.metadataId);
    if (!metadata) return std::unexpected(std::move(metadata.error()));
    if ((*metadata)->duration) (*metadata)->duration = 0.0f;
    return {};
  }); !result) return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.registerCallback("sourceEntity", [&controller = *this, &uniqueEffect]() -> std::expected<std::optional<EntityId>, StarException> {
    auto metadata = controller.m_uniqueEffectMetadata.getNetElement(uniqueEffect.metadataId);
    if (!metadata) return std::unexpected(std::move(metadata.error()));
    auto source = (*metadata)->sourceEntityId.get();
    if (!source) return std::optional<EntityId>(controller.m_parentEntity->get().entityId());
    if (source == NullEntityId) return std::optional<EntityId>{};
    return source;
  }); !result) return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.registerCallback("setParentDirectives", [&uniqueEffect](std::optional<String> const& directives) -> void {
    uniqueEffect.parentDirectives = directives.value_or(String{});
  }); !result) return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.registerCallback("getParameter", [&uniqueEffect](String const& name, Json const& def) -> std::expected<Json, StarException> {
    return uniqueEffect.effectConfig.effectConfig.query(name, def);
  }); !result) return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.registerCallback("addStatModifierGroup", [&controller = *this, &uniqueEffect](List<StatModifier> const& modifiers) -> std::expected<StatModifierGroupId, StarException> {
    auto id = controller.m_statCollection->addStatModifierGroup(modifiers);
    if (!id) return std::unexpected(std::move(id.error()));
    uniqueEffect.modifierGroups.add(*id);
    return *id;
  }); !result) return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.registerCallback("setStatModifierGroup", [&controller = *this, &uniqueEffect](StatModifierGroupId groupId, List<StatModifier> const& modifiers) -> std::expected<void, StarException> {
    if (!uniqueEffect.modifierGroups.contains(groupId))
      return std::unexpected(StatusException("Cannot set stat modifier group that was not added from this effect"));
    return controller.m_statCollection->setStatModifierGroup(groupId, modifiers);
  }); !result) return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.registerCallback("removeStatModifierGroup", [&controller = *this, &uniqueEffect](StatModifierGroupId groupId) -> std::expected<void, StarException> {
    if (!uniqueEffect.modifierGroups.contains(groupId))
      return std::unexpected(StatusException("Cannot remove stat modifier group that was not added from this effect"));
    controller.m_statCollection->removeStatModifierGroup(groupId);
    uniqueEffect.modifierGroups.remove(groupId);
    return {};
  }); !result) return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.registerCallback("setToolUsageSuppressed", [&controller = *this, &uniqueEffect](bool suppressed) -> void {
    if (uniqueEffect.toolUsageSuppressed == suppressed) return;
    uniqueEffect.toolUsageSuppressed = suppressed;
    bool anySuppressed = false;
    for (auto& entry : controller.m_uniqueEffects) anySuppressed = anySuppressed || entry.second.toolUsageSuppressed;
    controller.m_toolUsageSuppressed.set(anySuppressed);
  }); !result) return std::unexpected(std::move(result.error()));
  return callbacks;
}

}

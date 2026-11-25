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
import star.message_types;
import star.strong_typedef;
import star.bi_map;
import star.list;
import star.rpc_promise;
import star.observer_stream;
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

std::expected<void, StarException> StatusPropertyMap::net_store(DataStream& ds, NetCompatibilityRules rules) const {
  if (rules.version() > 1)
    return NetElementHashMap<String, Json>::net_store(ds, rules);
  auto result = ds << base_map();
  if (!result) return std::unexpected(std::move(result.error()));
  return {};
}

std::expected<void, StarException> StatusPropertyMap::net_load(DataStream& ds, NetCompatibilityRules rules) {
  if (rules.version() > 1)
    return NetElementHashMap<String, Json>::net_load(ds, rules);
  auto properties = ds.read<JsonObject>();
  if (!properties) return std::unexpected(std::move(properties.error()));
  reset(std::move(*properties));
  return {};
}

std::expected<bool, StarException> StatusPropertyMap::write_net_delta(DataStream& ds, std::uint64_t from_version, NetCompatibilityRules rules) const {
  if (rules.version() > 1)
    return NetElementHashMap<String, Json>::write_net_delta(ds, from_version, rules);
  if (!should_write_delta(from_version, rules))
    return false;
  auto result = net_store(ds, rules);
  if (!result) return std::unexpected(std::move(result.error()));
  return true;
}

std::expected<void, StarException> StatusPropertyMap::read_net_delta(DataStream& ds, float interpolation_time, NetCompatibilityRules rules) {
  if (rules.version() > 1)
    return NetElementHashMap<String, Json>::read_net_delta(ds, interpolation_time, rules);
  return net_load(ds, rules);
}

StatusController::StatusController(Root& root)
  : m_root(root), m_uniqueEffectMetadata([] { return UniqueEffectMetadata::create(); }),
    m_effectAnimators([&root] { return EffectAnimator::create(root); }),
    m_primaryScript(root),
    m_primaryAnimatorId(EffectAnimatorGroup::null_element_id) {}

std::expected<std::shared_ptr<StatusController>, StarException> StatusController::create(Root& root, Json const& config) {
  auto controller = std::shared_ptr<StatusController>(new StatusController(root));
  if (auto result = controller->configure(config); !result) return std::unexpected(std::move(result.error()));
  return controller;
}

std::expected<void, StarException> StatusController::configure(Json const& config) {
  auto stats = StatCollection::create(config);
  if (!stats) return std::unexpected(std::move(stats.error()));
  m_statCollection = std::move(*stats);
  auto properties = config.get_object("statusProperties", {});
  if (!properties) return std::unexpected(std::move(properties.error()));
  m_statusProperties.reset(std::move(*properties));
  auto minimumLiquidStatusEffectPercentage = config.get_float("minimumLiquidStatusEffectPercentage");
  if (!minimumLiquidStatusEffectPercentage) return std::unexpected(std::move(minimumLiquidStatusEffectPercentage.error()));
  m_minimumLiquidStatusEffectPercentage = *minimumLiquidStatusEffectPercentage;
  auto appliesEnvironmentStatusEffects = config.get_bool("appliesEnvironmentStatusEffects");
  if (!appliesEnvironmentStatusEffects) return std::unexpected(std::move(appliesEnvironmentStatusEffects.error()));
  m_appliesEnvironmentStatusEffects = *appliesEnvironmentStatusEffects;
  auto appliesWeatherStatusEffects = config.get_bool("appliesWeatherStatusEffects");
  if (!appliesWeatherStatusEffects) return std::unexpected(std::move(appliesWeatherStatusEffects.error()));
  m_appliesWeatherStatusEffects = *appliesWeatherStatusEffects;
  auto timer = config.get_float("environmentStatusEffectUpdateTimer", 0.15f);
  if (!timer) return std::unexpected(std::move(timer.error()));
  m_environment_status_effect_update_timer = GameTimer(*timer);
  auto animation = config.opt_string("primaryAnimationConfig");
  if (!animation) return std::unexpected(std::move(animation.error()));
  m_primaryAnimationConfig = std::move(*animation);
  auto scriptsJson = config.get("primaryScriptSources", JsonArray());
  if (!scriptsJson) return std::unexpected(std::move(scriptsJson.error()));
  auto scripts = json_to_string_list(*scriptsJson);
  if (!scripts) return std::unexpected(std::move(scripts.error()));
  if (auto result = m_primaryScript.set_scripts(std::move(*scripts)); !result) return std::unexpected(std::move(result.error()));
  auto delta = config.get_uint("primaryScriptDelta", 1);
  if (!delta) return std::unexpected(std::move(delta.error()));
  m_primaryScript.set_update_delta(*delta);
  auto keep_steps = config.get_uint("keepDamageNotificationSteps", 120);
  if (!keep_steps) return std::unexpected(std::move(keep_steps.error()));
  m_recent_hits_given.set_history_limit(*keep_steps);
  m_recent_damage_given.set_history_limit(*keep_steps);
  m_recent_damage_taken.set_history_limit(*keep_steps);
  if (auto result = m_netGroup.add_net_element(*m_statCollection); !result) return std::unexpected(std::move(result.error()));
  if (auto result = m_netGroup.add_net_element(m_statusProperties); !result) return std::unexpected(std::move(result.error()));
  if (auto result = m_netGroup.add_net_element(m_parentDirectives); !result) return std::unexpected(std::move(result.error()));
  if (auto result = m_netGroup.add_net_element(m_uniqueEffectMetadata); !result) return std::unexpected(std::move(result.error()));
  if (auto result = m_netGroup.add_net_element(m_effectAnimators); !result) return std::unexpected(std::move(result.error()));
  m_toolUsageSuppressed.set_compatibility_version(12);
  if (auto result = m_netGroup.add_net_element(m_toolUsageSuppressed); !result) return std::unexpected(std::move(result.error()));
  if (m_primaryAnimationConfig) {
    auto animator = EffectAnimator::create(m_root, m_primaryAnimationConfig);
    if (!animator) return std::unexpected(std::move(animator.error()));
    auto id = m_effectAnimators.add_net_element(std::move(*animator));
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
    std::ranges::transform(pair.second.statModifiers, std::back_inserter(persistentEffects), Construct<PersistentStatusEffect>());
    std::ranges::transform(pair.second.uniqueEffects, std::back_inserter(persistentEffects), Construct<PersistentStatusEffect>());
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
    if (auto duration = pair.second->metadata->duration) {
      ephemeralEffects.push_back(jsonFromEphemeralStatusEffect(EphemeralStatusEffect{pair.first, *duration}));
    }
  }

  return JsonObject{
      {"statusProperties", m_statusProperties.base_map()},
      {"persistentEffectCategories", std::move(persistentEffectCategories)},
      {"ephemeralEffects", std::move(ephemeralEffects)},
      {"resourceValues", std::move(resourceValues)},
      {"resourcesLocked", std::move(resourcesLocked)},
  };
}

std::expected<void, StarException> StatusController::diskLoad(Json const& store) {
  if (auto result = clearAllPersistentEffects(); !result) return std::unexpected(std::move(result.error()));
  if (auto result = clearEphemeralEffects(); !result) return std::unexpected(std::move(result.error()));
  auto properties = store.get_object("statusProperties");
  if (!properties) return std::unexpected(std::move(properties.error()));
  m_statusProperties.reset(std::move(*properties));
  auto categories = store.get_object("persistentEffectCategories", {});
  if (!categories) return std::unexpected(std::move(categories.error()));
  for (auto const& category : *categories) {
    auto values = category.second.to_array();
    if (!values) return std::unexpected(std::move(values.error()));
    List<PersistentStatusEffect> effects;
    for (auto const& value : *values) {
      auto effect = jsonToPersistentStatusEffect(value);
      if (!effect) return std::unexpected(std::move(effect.error()));
      effects.push_back(std::move(*effect));
    }
    if (auto result = addPersistentEffects(category.first, effects); !result) return std::unexpected(std::move(result.error()));
  }
  auto ephemeral = store.get_array("ephemeralEffects");
  if (!ephemeral) return std::unexpected(std::move(ephemeral.error()));
  List<EphemeralStatusEffect> effects;
  for (auto const& value : *ephemeral) {
    auto effect = jsonToEphemeralStatusEffect(value);
    if (!effect) return std::unexpected(std::move(effect.error()));
    effects.push_back(std::move(*effect));
  }
  if (auto result = addEphemeralEffects(effects); !result) return std::unexpected(std::move(result.error()));
  auto values = store.get_object("resourceValues", {});
  if (!values) return std::unexpected(std::move(values.error()));
  for (auto const& value : *values) {
    if (isResource(value.first)) {
      auto amount = value.second.to_float();
      if (!amount) return std::unexpected(std::move(amount.error()));
      if (auto result = setResource(value.first, *amount); !result) return std::unexpected(std::move(result.error()));
    }
  }
  auto locks = store.get_object("resourcesLocked", {});
  if (!locks) return std::unexpected(std::move(locks.error()));
  for (auto const& value : *locks) {
    if (isResource(value.first)) {
      auto locked = value.second.to_bool();
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
  if (value.is_null())
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
    if (!notified && !notified.error().is<LuaScriptError>()) return std::unexpected(std::move(notified.error()));
  }
  return *consumed;
}

std::expected<bool, StarException> StatusController::overConsumeResource(String const& resourceName, float amount) {
  auto consumed = m_statCollection->overConsumeResource(resourceName, amount);
  if (!consumed) return std::unexpected(std::move(consumed.error()));
  if (*consumed) {
    auto notified = m_primaryScript.invoke("notifyResourceConsumed", resourceName, amount);
    if (!notified && !notified.error().is<LuaScriptError>()) return std::unexpected(std::move(notified.error()));
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
  std::ranges::transform(category.statModifiers, std::back_inserter(persistentEffects), Construct<PersistentStatusEffect>());
  std::ranges::transform(category.uniqueEffects, std::back_inserter(persistentEffects), Construct<PersistentStatusEffect>());
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
  if (auto existing = m_uniqueEffects.find_ref(effect.uniqueEffect)) {
    auto const& instance = *existing->get();
    auto& metadata = *instance.metadata;
    if (metadata.duration) {
      float duration = effect.duration.value_or(instance.definition.defaultDuration);
      if (duration > *metadata.duration) {
        metadata.sourceEntityId.set(sourceEntityId);
        metadata.duration = duration;
      }
      metadata.maxDuration.set(std::max(metadata.maxDuration.get(), duration));
    }
    return {};
  }
  auto added = add_unique_effect(effect.uniqueEffect, true, effect.duration, sourceEntityId);
  if (!added) return std::unexpected(std::move(added.error()));
  return {};
}

std::expected<void, StarException> StatusController::addEphemeralEffects(List<EphemeralStatusEffect> const& effectList, std::optional<EntityId> sourceEntityId) {
  for (auto const& effect : effectList) {
    if (auto result = addEphemeralEffect(effect, sourceEntityId); !result)
      return std::unexpected(std::move(result.error()));
  }
  return {};
}

std::expected<bool, StarException> StatusController::removeEphemeralEffect(UniqueStatusEffect const& effect) {
  if (auto instance = m_uniqueEffects.find_ref(effect); instance && instance->get()->metadata->duration) {
    if (auto result = remove_unique_effect(instance->get()); !result) return std::unexpected(std::move(result.error()));
    return true;
  }
  return false;
}

std::expected<void, StarException> StatusController::clearEphemeralEffects() {
  for (auto effects = effect_snapshot(); auto const& instance : *effects) {
    if (instance->active && instance->metadata->duration)
      if (auto result = remove_unique_effect(instance); !result) return std::unexpected(std::move(result.error()));
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
  for (auto const& metadata : m_uniqueEffectMetadata.net_elements()) {
    if (metadata->duration)
      summary.push_back({metadata->effect, *metadata->duration / metadata->maxDuration.get()});
    else
      summary.push_back({metadata->effect, 1.0f});
  }
  return summary;
}

bool StatusController::uniqueStatusEffectActive(String const& effectName) const {
  for (auto const& metadata : m_uniqueEffectMetadata.net_elements()) {
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

std::expected<List<DamageNotification>, StarException> StatusController::apply_damage_request(DamageRequest const& request) {
  auto notifications = m_primaryScript.invoke<List<DamageNotification>>("applyDamageRequest", request);
  if (!notifications) {
    if (notifications.error().is<LuaScriptError>()) return List<DamageNotification>{};
    return std::unexpected(std::move(notifications.error()));
  }
  if (*notifications) {
    for (auto const& notification : **notifications) m_recent_damage_taken.push_back(notification);
    return std::move(**notifications);
  }
  return List<DamageNotification>{};
}

void StatusController::hit_other(EntityId target_id, DamageRequest const& request) {
  if (m_recent_hits_given.history_limit() == 0)
    m_recent_hits_given.tick();
  else
    m_recent_hits_given.push_back({target_id, request});
}

void StatusController::damaged_other(DamageNotification const& notification) {
  m_recent_damage_given.push_back(notification);
}

List<DamageNotification> StatusController::pull_self_damage_notifications() {
  return std::exchange(m_pending_self_damage_notifications, {});
}

std::expected<void, StarException> StatusController::apply_self_damage_request(DamageRequest const& request) {
  auto notifications = apply_damage_request(request);
  if (!notifications) return std::unexpected(std::move(notifications.error()));
  if (m_pending_self_damage_notifications.empty())
    m_pending_self_damage_notifications = std::move(*notifications);
  else
    m_pending_self_damage_notifications.append_range(std::move(*notifications));
  return {};
}

std::expected<StatusController::DamageHistory, StarException> StatusController::damage_taken_since(std::uint64_t since) const {
  return m_recent_damage_taken.transform_since<Json>(since, [](DamageNotification const& notification) {
    return notification.to_json();
  });
}

std::expected<StatusController::DamageHistory, StarException> StatusController::inflicted_hits_since(std::uint64_t since) const {
  return m_recent_hits_given.transform_since<Json>(since, [](std::pair<EntityId, DamageRequest> const& hit) -> std::expected<Json, StarException> {
    auto json = hit.second.to_json();
    if (!json) return std::unexpected(std::move(json.error()));
    return json->set("targetEntityId", hit.first);
  });
}

std::expected<StatusController::DamageHistory, StarException> StatusController::inflicted_damage_since(std::uint64_t since) const {
  return m_recent_damage_given.transform_since<Json>(since, [](DamageNotification const& notification) {
    return notification.to_json();
  });
}

std::expected<void, StarException> StatusController::init(Entity& parentEntity, ActorMovementController& movementController) {
  if (auto result = uninit(); !result) return std::unexpected(std::move(result.error()));

  m_parentEntity = std::ref(parentEntity);
  m_movement_controller = std::ref(movementController);

  if (m_parentEntity->get().isMaster()) {
    auto effects = effect_snapshot();
    m_run_effect_scripts = true;
    if (auto result = initPrimaryScript(); !result) return std::unexpected(std::move(result.error()));
    for (auto const& instance : *effects) {
      if (!instance->active) continue;
      auto result = dispatch_effect(*instance, [&] { return initUniqueEffectScript(*instance); });
      if (!result) return std::unexpected(std::move(result.error()));
    }
  }

  m_environment_status_effect_update_timer.reset();
  return {};
}

std::expected<void, StarException> StatusController::uninit() {
  std::optional<StarException> error;
  m_run_effect_scripts = false;
  for (auto effects = effect_snapshot(); auto const& instance : *effects) {
    if (instance->active) {
      auto result = uninitUniqueEffectScript(*instance);
      if (!result && !error) error = std::move(result.error());
    }
  }
  auto primary = uninitPrimaryScript();
  if (!primary && !error) error = std::move(primary.error());
  m_recent_hits_given.reset();
  m_recent_damage_given.reset();
  m_recent_damage_taken.reset();
  m_parentEntity.reset();
  m_movement_controller.reset();
  if (error) return std::unexpected(std::move(*error));
  return {};
}

void StatusController::init_net_version(std::optional<std::reference_wrapper<NetElementVersion const>> version) {
  m_netGroup.init_net_version(version);
}

std::expected<void, StarException> StatusController::net_store(DataStream& ds, NetCompatibilityRules rules) const {
  if (!check_with_rules(rules)) return {};
  return m_netGroup.net_store(ds, rules);
}

std::expected<void, StarException> StatusController::net_load(DataStream& ds, NetCompatibilityRules rules) {
  if (!check_with_rules(rules)) return {};
  if (auto result = clearAllPersistentEffects(); !result) return std::unexpected(std::move(result.error()));
  if (auto result = clearEphemeralEffects(); !result) return std::unexpected(std::move(result.error()));
  return m_netGroup.net_load(ds, rules);
}

std::expected<void, StarException> StatusController::enable_net_interpolation(float extrapolation_hint) {
  return m_netGroup.enable_net_interpolation(extrapolation_hint);
}

std::expected<void, StarException> StatusController::disable_net_interpolation() {
  return m_netGroup.disable_net_interpolation();
}

std::expected<void, StarException> StatusController::tick_net_interpolation(float dt) {
  return m_netGroup.tick_net_interpolation(dt);
}

std::expected<bool, StarException> StatusController::write_net_delta(DataStream& ds, std::uint64_t fromVersion, NetCompatibilityRules rules) const {
  return m_netGroup.write_net_delta(ds, fromVersion, rules);
}

std::expected<void, StarException> StatusController::read_net_delta(DataStream& ds, float interpolationTime, NetCompatibilityRules rules) {
  return m_netGroup.read_net_delta(ds, interpolationTime, rules);
}

std::expected<void, StarException> StatusController::blank_net_delta(float interpolationTime) {
  return m_netGroup.blank_net_delta(interpolationTime);
}

std::expected<void, StarException> StatusController::tickMaster(float dt) {
  m_statCollection->tickMaster(dt);
  m_recent_hits_given.tick();
  m_recent_damage_given.tick();
  m_recent_damage_taken.tick();
  bool statusImmune = statPositive("statusImmunity");
  if (!statusImmune && m_movement_controller->get().liquidPercentage() > m_minimumLiquidStatusEffectPercentage) {
    auto database = m_root.liquidsDatabase();
    if (!database) return std::unexpected(std::move(database.error()));
    if (auto settings = (*database)->liquidSettings(m_movement_controller->get().liquidId())) {
      for (auto const& value : settings->statusEffects) {
        auto effect = jsonToEphemeralStatusEffect(value);
        if (!effect) return std::unexpected(std::move(effect.error()));
        if (auto result = addEphemeralEffect(*effect, {}); !result) return std::unexpected(std::move(result.error()));
      }
    }
  }
  if (m_environment_status_effect_update_timer.wrap_tick(m_root.timing().time_step)) {
    PolyF collisionBody = m_movement_controller->get().collisionBody();
    auto world = m_parentEntity->get().world();
    if (!world) return std::unexpected(std::move(world.error()));
    List<PersistentStatusEffect> entityEffects;
    if (!statusImmune) {
      auto visited = world->get().forEachEntity(collisionBody.bounding_box(),
        [world = *world, collisionBody, &entityEffects](std::shared_ptr<Entity> const& entity) -> std::expected<void, StarException> {
          if (auto effectEntity = as<StatusEffectEntity>(entity)) {
            auto area = effectEntity->statusEffectArea();
            area.translate(effectEntity->position());
            auto intersects = world.get().geometry().poly_intersects_poly(area, collisionBody);
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
  auto primary = m_primaryScript.update(m_primaryScript.update_dt(dt));
  if (!primary && !primary.error().is<LuaScriptError>()) return std::unexpected(std::move(primary.error()));
  auto effects = effect_snapshot();
  for (auto const& instance : *effects) {
    if (!instance->active) continue;
    auto updated = dispatch_effect(*instance, [&] { return instance->script.update(instance->script.update_dt(dt)); });
    // Each failing effect has already been logged and disabled by its script component.
    if (!updated && !updated.error().is<LuaScriptError>()) return std::unexpected(std::move(updated.error()));
    if (instance->active && instance->metadata->duration) *instance->metadata->duration -= dt;
  }
  for (auto const& instance : *effects) {
    if (!instance->active) continue;
    auto duration = instance->metadata->duration;
    auto const& blocking_stat = instance->definition.blockingStat;
    if ((duration && (*duration <= 0.0f || statPositive("statusImmunity")))
        || (blocking_stat && statPositive(*blocking_stat))) {
      if (auto result = remove_unique_effect(instance); !result) return std::unexpected(std::move(result.error()));
    }
  }
  DirectivesGroup directives;
  directives.append(m_primaryDirectives);
  for (auto const& entry : m_uniqueEffects) directives.append(entry.second->parentDirectives);
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
  for (auto const& animator : m_effectAnimators.net_elements()) {
    auto sample = animator->animator->sample(interpolation);
    auto values = sample.drawables(m_movement_controller->get().position());
    if (!values) return std::unexpected(std::move(values.error()));
    output.append_range(std::move(*values));
  }
  return output;
}

std::expected<List<LightSource>, StarException> StatusController::lightSources(float interpolation) const {
  List<LightSource> output;
  for (auto const& animator : m_effectAnimators.net_elements()) {
    auto sample = animator->animator->sample(interpolation);
    auto values = sample.lightSources(m_movement_controller->get().position());
    if (!values) return std::unexpected(std::move(values.error()));
    output.append_range(std::move(*values));
  }
  return output;
}

std::expected<List<OverheadBar>, StarException> StatusController::overhead_bars() {
  auto json = m_primaryScript.invoke<JsonArray>("overheadBars");
  if (!json) {
    if (json.error().is<LuaScriptError>()) return List<OverheadBar>{};
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
  for (auto const& animator : m_effectAnimators.net_elements())
    newAudios.append_range(animator->dynamicTarget.pullNewAudios());
  return newAudios;
}

List<Particle> StatusController::pullNewParticles() {
  List<Particle> newParticles;
  for (auto const& animator : m_effectAnimators.net_elements())
    newParticles.append_range(animator->dynamicTarget.pullNewParticles());
  return newParticles;
}

MessageResponseResult StatusController::receiveMessage(String const& message, bool localMessage, JsonArray const& args) {
  auto result = m_primaryScript.handle_message(message, localMessage, args);
  if (!result) {
    if (!result.error().is<LuaScriptError>()) return std::unexpected(std::move(result.error()));
    result = std::optional<ChainableJsonMessageResponse>{};
  }
  for (auto effects = effect_snapshot(); auto const& instance : *effects) {
    if (!instance->active) continue;
    auto response = dispatch_effect(*instance, [&] { return instance->script.handle_message(message, localMessage, args); });
    if (!response) {
      if (!response.error().is<LuaScriptError>()) return std::unexpected(std::move(response.error()));
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

void StatusController::EffectAnimator::init_net_version(std::optional<std::reference_wrapper<NetElementVersion const>> version) {
  animator->init_net_version(version);
}

std::expected<void, StarException> StatusController::EffectAnimator::net_store(DataStream& ds, NetCompatibilityRules rules) const {
  if (!check_with_rules(rules)) return {};
  if (auto result = ds.write(animationConfig); !result) return std::unexpected(std::move(result.error()));
  return animator->net_store(ds, rules);
}

std::expected<void, StarException> StatusController::EffectAnimator::net_load(DataStream& ds, NetCompatibilityRules rules) {
  if (!check_with_rules(rules)) return {};
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
  return animator->net_load(ds, rules);
}

std::expected<void, StarException> StatusController::EffectAnimator::enable_net_interpolation(float extrapolation_hint) {
  return animator->enable_net_interpolation(extrapolation_hint);
}

std::expected<void, StarException> StatusController::EffectAnimator::disable_net_interpolation() {
  return animator->disable_net_interpolation();
}

std::expected<void, StarException> StatusController::EffectAnimator::tick_net_interpolation(float dt) {
  return animator->tick_net_interpolation(dt);
}

std::expected<bool, StarException> StatusController::EffectAnimator::write_net_delta(DataStream& ds, std::uint64_t fromVersion, NetCompatibilityRules rules) const {
  return animator->write_net_delta(ds, fromVersion, rules);
}

std::expected<void, StarException> StatusController::EffectAnimator::read_net_delta(DataStream& ds, float interpolationTime, NetCompatibilityRules rules) {
  return animator->read_net_delta(ds, interpolationTime, rules);
}

std::expected<void, StarException> StatusController::EffectAnimator::blank_net_delta(float interpolationTime) {
  return animator->blank_net_delta(interpolationTime);
}

StatusController::UniqueEffectMetadata::UniqueEffectMetadata() {
  durationNetState.set_fixed_point_base(0.01f);
  durationNetState.set_interpolator(lerp<float, float>);
}

std::expected<std::shared_ptr<StatusController::UniqueEffectMetadata>, StarException> StatusController::UniqueEffectMetadata::create(UniqueStatusEffect effect, std::optional<float> duration, std::optional<EntityId> sourceEntityId) {
  auto result = std::make_shared<UniqueEffectMetadata>();
  if (auto registered = result->add_net_element(result->durationNetState); !registered) return std::unexpected(std::move(registered.error()));
  if (auto registered = result->add_net_element(result->maxDuration); !registered) return std::unexpected(std::move(registered.error()));
  if (auto registered = result->add_net_element(result->sourceEntityId); !registered) return std::unexpected(std::move(registered.error()));
  result->effect = std::move(effect);
  result->duration = duration;
  result->maxDuration.set(duration.value_or(0.0f));
  result->sourceEntityId.set(sourceEntityId);
  return result;
}

std::expected<void, StarException> StatusController::UniqueEffectMetadata::net_elements_need_load(bool) {
  duration = durationNetState.get() >= 0.0f ? std::optional<float>(durationNetState.get()) : std::optional<float>();
  return {};
}

std::expected<void, StarException> StatusController::UniqueEffectMetadata::net_elements_need_store() {
  durationNetState.set(duration ? *duration : -1.0f);
  return {};
}

std::expected<void, StarException> StatusController::updateAnimators(float dt) {
  auto world = m_parentEntity->get().world();
  if (!world) return std::unexpected(std::move(world.error()));
  for (auto const& animator : m_effectAnimators.net_elements()) {
    if (world->get().isServer()) {
      if (auto result = animator->animator->update(dt, std::nullopt); !result) return std::unexpected(std::move(result.error()));
    } else {
      if (auto result = animator->animator->update(dt, std::ref(animator->dynamicTarget)); !result) return std::unexpected(std::move(result.error()));
      animator->dynamicTarget.updatePosition(m_movement_controller->get().position());
    }
  }
  return {};
}

std::expected<void, StarException> StatusController::updatePersistentUniqueEffects() {
  auto revision = ++m_persistent_revision;
  Set<UniqueStatusEffect> active;
  List<UniqueStatusEffect> missing;
  for (auto const& category : m_persistentEffects) {
    for (auto const& name : category.second.uniqueEffects) {
      if (!active.add(name)) continue;
      if (auto existing = m_uniqueEffects.find_ref(name))
        existing->get()->metadata->duration.reset();
      else
        missing.push_back(name);
    }
  }
  // init and onExpire can synchronously reconcile a newer set of categories.
  for (auto const& name : missing) {
    if (auto existing = m_uniqueEffects.find_ref(name)) {
      existing->get()->metadata->duration.reset();
      continue;
    }
    auto added = add_unique_effect(name, false, {}, {});
    if (!added) return std::unexpected(std::move(added.error()));
    if (revision != m_persistent_revision) return {};
    if (!*added)
      for (auto&& category : m_persistentEffects) category.second.uniqueEffects.remove(name);
  }
  for (auto effects = effect_snapshot(); auto const& instance : *effects) {
    if (instance->active && !instance->metadata->duration && !active.contains(instance->definition.name)) {
      if (auto result = remove_unique_effect(instance); !result) return std::unexpected(std::move(result.error()));
      if (revision != m_persistent_revision) return {};
    }
  }
  return {};
}

std::shared_ptr<StatusController::EffectSnapshot const> StatusController::effect_snapshot() {
  if (m_uniqueEffects.empty()) {
    static auto const empty = std::make_shared<EffectSnapshot const>();
    return empty;
  }
  if (!m_effect_snapshot)
    m_effect_snapshot = std::make_shared<EffectSnapshot>(m_uniqueEffects.values());
  return m_effect_snapshot;
}

template <status_effect_call call_type>
std::invoke_result_t<call_type&> StatusController::dispatch_effect(UniqueEffectInstance& instance, call_type&& call) {
  bool nested = std::exchange(instance.executing, true);
  auto result = call();
  instance.executing = nested;
  // A self-removing script finishes its current native/Lua frame with its
  // original callbacks intact. Only the outermost caller performs teardown.
  if (!nested && !instance.active) {
    if (auto stopped = uninitUniqueEffectScript(instance); !stopped) {
      if (!result) return std::unexpected(stopped.error().with_related(result.error(), "Effect callback failed"));
      return std::unexpected(std::move(stopped.error()));
    }
  }
  return result;
}

std::expected<bool, StarException> StatusController::add_unique_effect(UniqueStatusEffect const& effect,
    bool ephemeral, std::optional<float> duration, std::optional<EntityId> source_id) {
  auto database = m_root.statusEffectDatabase();
  if (!database) return std::unexpected(std::move(database.error()));
  auto config = (*database)->unique_effect_config(effect);
  if (!config) {
    if (ephemeral) return std::unexpected(std::move(config.error()));
    if (auto result = m_root.logger().warn("Unique status effect '{}' not found in status effect database", effect); !result) return std::unexpected(std::move(result.error()));
    return false;
  }
  auto const& definition = config->get();
  if ((ephemeral && statPositive("statusImmunity")) || (definition.blockingStat && statPositive(*definition.blockingStat))) return false;
  if (ephemeral && !duration) duration = definition.defaultDuration;
  auto metadata = UniqueEffectMetadata::create(effect, duration, source_id);
  if (!metadata) return std::unexpected(std::move(metadata.error()));
  auto instance = std::make_shared<UniqueEffectInstance>(m_root, std::move(*database), definition, std::move(*metadata));
  if (auto result = instance->script.set_scripts(definition.scripts); !result) return std::unexpected(std::move(result.error()));
  instance->script.set_update_delta(definition.scriptDelta);
  if (definition.animationConfig) {
    auto animator = EffectAnimator::create(m_root, definition.animationConfig);
    if (!animator) return std::unexpected(std::move(animator.error()));
    instance->animator = std::move(*animator);
  }
  auto metadata_id = m_uniqueEffectMetadata.add_net_element(instance->metadata);
  if (!metadata_id) return std::unexpected(std::move(metadata_id.error()));
  instance->metadata_id = *metadata_id;
  if (instance->animator) {
    auto animator_id = m_effectAnimators.add_net_element(instance->animator);
    if (!animator_id) {
      m_uniqueEffectMetadata.remove_net_element(*metadata_id);
      return std::unexpected(std::move(animator_id.error()));
    }
    instance->animator_id = *animator_id;
  }
  m_uniqueEffects.try_emplace(effect, instance);
  m_effect_snapshot.reset();
  if (m_run_effect_scripts) {
    if (auto result = dispatch_effect(*instance, [&] { return initUniqueEffectScript(*instance); }); !result) {
      if (instance->active) {
        if (auto cleanup = remove_unique_effect(instance, false); !cleanup)
          return std::unexpected(cleanup.error().with_related(result.error(), "Effect initialization failed"));
      }
      return std::unexpected(std::move(result.error()));
    }
  }
  return true;
}

std::expected<void, StarException> StatusController::remove_unique_effect(std::shared_ptr<UniqueEffectInstance> instance, bool expire) {
  // Unpublish identity and wire entries before running arbitrary Lua. The
  // detached instance still owns the metadata and animator its callbacks use.
  instance->active = false;
  m_uniqueEffects.remove(instance->definition.name);
  m_effect_snapshot.reset();
  m_uniqueEffectMetadata.remove_net_element(instance->metadata_id);
  if (instance->animator) m_effectAnimators.remove_net_element(instance->animator_id);
  if (instance->tool_usage_suppressed) update_tool_suppression();
  std::optional<StarException> error;
  if (expire) {
    auto expired = instance->script.invoke("onExpire");
    if (!expired && !expired.error().is<LuaScriptError>()) error = std::move(expired.error());
  }
  if (!instance->executing) {
    auto stopped = uninitUniqueEffectScript(*instance);
    if (!stopped && !error) error = std::move(stopped.error());
  }
  if (error) return std::unexpected(std::move(*error));
  return {};
}

void StatusController::update_tool_suppression() {
  m_toolUsageSuppressed.set(std::ranges::any_of(m_uniqueEffects.value_view(),
      [](std::shared_ptr<UniqueEffectInstance> const& instance) { return instance->tool_usage_suppressed; }));
}

std::expected<void, StarException> StatusController::initPrimaryScript() {
  auto statusCallbacks = LuaBindings::makeStatusControllerCallbacks(*this);
  if (!statusCallbacks) return std::unexpected(std::move(statusCallbacks.error()));
  if (auto result = m_primaryScript.add_callbacks("status", std::move(*statusCallbacks)); !result) return std::unexpected(std::move(result.error()));
  auto entityCallbacks = LuaBindings::makeEntityCallbacks(m_parentEntity->get());
  if (!entityCallbacks) return std::unexpected(std::move(entityCallbacks.error()));
  if (auto result = m_primaryScript.add_callbacks("entity", std::move(*entityCallbacks)); !result) return std::unexpected(std::move(result.error()));
  if (m_primaryAnimatorId != EffectAnimatorGroup::null_element_id) {
    auto animator = m_effectAnimators.get_net_element(m_primaryAnimatorId);
    if (!animator) return std::unexpected(std::move(animator.error()));
    auto callbacks = LuaBindings::makeNetworkedAnimatorCallbacks(*(*animator)->animator);
    if (!callbacks) return std::unexpected(std::move(callbacks.error()));
    if (auto result = m_primaryScript.add_callbacks("animator", std::move(*callbacks)); !result) return std::unexpected(std::move(result.error()));
  }
  if (auto result = m_primaryScript.add_actor_movement_callbacks(m_movement_controller->get()); !result) return std::unexpected(std::move(result.error()));
  auto world = m_parentEntity->get().world();
  if (!world) return std::unexpected(std::move(world.error()));
  auto initialized = m_primaryScript.init(world->get());
  if (!initialized && !initialized.error().is<LuaScriptError>()) return std::unexpected(std::move(initialized.error()));
  return {};
}

std::expected<void, StarException> StatusController::uninitPrimaryScript() {
  auto uninitialized = m_primaryScript.uninit();
  std::optional<StarException> error;
  if (!uninitialized && !uninitialized.error().is<LuaScriptError>()) error = std::move(uninitialized.error());
  if (auto result = m_primaryScript.remove_callbacks("status"); !result && !error) error = std::move(result.error());
  if (auto result = m_primaryScript.remove_callbacks("entity"); !result && !error) error = std::move(result.error());
  if (auto result = m_primaryScript.remove_callbacks("animator"); !result && !error) error = std::move(result.error());
  if (auto result = m_primaryScript.remove_actor_movement_callbacks(); !result && !error) error = std::move(result.error());
  if (error) return std::unexpected(std::move(*error));
  return {};
}

std::expected<void, StarException> StatusController::initUniqueEffectScript(UniqueEffectInstance& uniqueEffect) {
  auto effectCallbacks = makeUniqueEffectCallbacks(uniqueEffect);
  if (!effectCallbacks) return std::unexpected(std::move(effectCallbacks.error()));
  if (auto result = uniqueEffect.script.add_callbacks("effect", std::move(*effectCallbacks)); !result) return std::unexpected(std::move(result.error()));
  auto statusCallbacks = LuaBindings::makeStatusControllerCallbacks(*this);
  if (!statusCallbacks) return std::unexpected(std::move(statusCallbacks.error()));
  if (auto result = uniqueEffect.script.add_callbacks("status", std::move(*statusCallbacks)); !result) return std::unexpected(std::move(result.error()));
  auto configCallbacks = LuaBindings::makeConfigCallbacks([&uniqueEffect](String const& name, Json const& def) {
    return uniqueEffect.definition.effectConfig.query(name, def);
  });
  if (!configCallbacks) return std::unexpected(std::move(configCallbacks.error()));
  if (auto result = uniqueEffect.script.add_callbacks("config", std::move(*configCallbacks)); !result) return std::unexpected(std::move(result.error()));
  auto entityCallbacks = LuaBindings::makeEntityCallbacks(m_parentEntity->get());
  if (!entityCallbacks) return std::unexpected(std::move(entityCallbacks.error()));
  if (auto result = uniqueEffect.script.add_callbacks("entity", std::move(*entityCallbacks)); !result) return std::unexpected(std::move(result.error()));
  if (uniqueEffect.animator) {
    auto callbacks = LuaBindings::makeNetworkedAnimatorCallbacks(*uniqueEffect.animator->animator);
    if (!callbacks) return std::unexpected(std::move(callbacks.error()));
    if (auto result = uniqueEffect.script.add_callbacks("animator", std::move(*callbacks)); !result) return std::unexpected(std::move(result.error()));
  }
  if (auto result = uniqueEffect.script.add_actor_movement_callbacks(m_movement_controller->get()); !result) return std::unexpected(std::move(result.error()));
  auto world = m_parentEntity->get().world();
  if (!world) return std::unexpected(std::move(world.error()));
  auto initialized = uniqueEffect.script.init(world->get());
  if (!initialized && !initialized.error().is<LuaScriptError>()) return std::unexpected(std::move(initialized.error()));
  return {};
}

std::expected<void, StarException> StatusController::uninitUniqueEffectScript(UniqueEffectInstance& uniqueEffect) {
  bool nested = std::exchange(uniqueEffect.executing, true);
  auto reset = make_scope_exit([&] { uniqueEffect.executing = nested; });
  auto uninitialized = uniqueEffect.script.uninit();
  std::optional<StarException> error;
  if (!uninitialized && !uninitialized.error().is<LuaScriptError>()) error = std::move(uninitialized.error());
  if (auto result = uniqueEffect.script.remove_callbacks("effect"); !result && !error) error = std::move(result.error());
  if (auto result = uniqueEffect.script.remove_callbacks("status"); !result && !error) error = std::move(result.error());
  if (auto result = uniqueEffect.script.remove_callbacks("config"); !result && !error) error = std::move(result.error());
  if (auto result = uniqueEffect.script.remove_callbacks("entity"); !result && !error) error = std::move(result.error());
  if (auto result = uniqueEffect.script.remove_callbacks("animator"); !result && !error) error = std::move(result.error());
  if (auto result = uniqueEffect.script.remove_actor_movement_callbacks(); !result && !error) error = std::move(result.error());
  for (auto group : uniqueEffect.modifierGroups) m_statCollection->removeStatModifierGroup(group);
  uniqueEffect.modifierGroups.clear();
  if (error) return std::unexpected(std::move(*error));
  return {};
}

std::expected<LuaCallbacks, StarException> StatusController::makeUniqueEffectCallbacks(UniqueEffectInstance& uniqueEffect) {
  LuaCallbacks callbacks;
  if (auto result = callbacks.register_callback("name", [&uniqueEffect]() { return uniqueEffect.definition.name; }); !result) return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback("duration", [&uniqueEffect]() { return uniqueEffect.metadata->duration; }); !result) return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback("modifyDuration", [&uniqueEffect](float duration) -> void {
    if (uniqueEffect.metadata->duration) *uniqueEffect.metadata->duration += duration;
  }); !result) return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback("setDuration", [&uniqueEffect](float duration) -> void {
    if (uniqueEffect.metadata->duration) uniqueEffect.metadata->duration = duration;
  }); !result) return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback("expire", [&uniqueEffect]() -> void {
    if (uniqueEffect.metadata->duration) uniqueEffect.metadata->duration = 0.0f;
  }); !result) return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback("sourceEntity", [&controller = *this, &uniqueEffect]() -> std::expected<std::optional<EntityId>, StarException> {
    auto source = uniqueEffect.metadata->sourceEntityId.get();
    if (!source) return std::optional<EntityId>(controller.m_parentEntity->get().entityId());
    if (source == null_entity_id) return std::optional<EntityId>{};
    return source;
  }); !result) return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback("setParentDirectives", [&uniqueEffect](std::optional<String> const& directives) -> void {
    uniqueEffect.parentDirectives = directives.value_or(String{});
  }); !result) return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback("getParameter", [&uniqueEffect](String const& name, Json const& def) -> std::expected<Json, StarException> {
    return uniqueEffect.definition.effectConfig.query(name, def);
  }); !result) return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback("addStatModifierGroup", [&controller = *this, &uniqueEffect](List<StatModifier> const& modifiers) -> std::expected<StatModifierGroupId, StarException> {
    auto id = controller.m_statCollection->addStatModifierGroup(modifiers);
    if (!id) return std::unexpected(std::move(id.error()));
    uniqueEffect.modifierGroups.add(*id);
    return *id;
  }); !result) return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback("setStatModifierGroup", [&controller = *this, &uniqueEffect](StatModifierGroupId groupId, List<StatModifier> const& modifiers) -> std::expected<void, StarException> {
    if (!uniqueEffect.modifierGroups.contains(groupId))
      return std::unexpected(StatusException("Cannot set stat modifier group that was not added from this effect"));
    return controller.m_statCollection->setStatModifierGroup(groupId, modifiers);
  }); !result) return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback("removeStatModifierGroup", [&controller = *this, &uniqueEffect](StatModifierGroupId groupId) -> std::expected<void, StarException> {
    if (!uniqueEffect.modifierGroups.contains(groupId))
      return std::unexpected(StatusException("Cannot remove stat modifier group that was not added from this effect"));
    controller.m_statCollection->removeStatModifierGroup(groupId);
    uniqueEffect.modifierGroups.remove(groupId);
    return {};
  }); !result) return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback("setToolUsageSuppressed", [&controller = *this, &uniqueEffect](bool suppressed) -> void {
    if (uniqueEffect.tool_usage_suppressed == suppressed) return;
    uniqueEffect.tool_usage_suppressed = suppressed;
    if (uniqueEffect.active) controller.update_tool_suppression();
  }); !result) return std::unexpected(std::move(result.error()));
  return callbacks;
}

}

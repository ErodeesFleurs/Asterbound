import std;
import star.format;
import star.json;
import star.json_extra;
import star.id_map;
import star.casting;
import star.vector;
import star.data_stream;
import star.game_types;
import star.strong_typedef;
import star.bi_map;
import star.color;
import star.list;
import star.image_processing;
import star.hash;
import star.string_view;
import star.thread;
import star.directives;
import star.asset_path;
import star.rpc_promise;
import star.a_star;
import star.set;
import star.string;

import star.damage_types;
import star.world_geometry;
import star.status_types;
import star.damage;
import star.light_source;
import star.entity;
import star.animation;
import star.particle;
import star.interaction_types;
import star.tile_damage;
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
import star.physics_entity;
import star.movement_controller;
import star.platformer_astar_types;
import star.anchorable_entity;
import star.game_timers;
import star.actor_movement_controller;
import star.mobile_entity;
import star.actor_entity;
import star.tool_user_entity;
import star.tool_user_item;
import star.status_effect_item;
import star.lua_components;
import star.fireable_item;
import star.drawable;
import star.item;

import star.config_lua_bindings;
import star.fireable_item_lua_bindings;
import star.item_lua_bindings;

namespace Star {

FireableItem::FireableItem()
  : m_fireTimer(0),
    m_cooldownTime(10),
    m_windupTime(0),
    m_fireWhenReady(false),
    m_startWhenReady(false),
    m_cooldown(false),
    m_alreadyInit(false),
    m_requireEdgeTrigger(false),
    m_attemptedFire(false),
    m_fireOnRelease(false),
    m_timeFiring(0.0f),
    m_startTimingFire(false),
    m_inUse(false),
    m_walkWhileFiring(false),
    m_stopWhileFiring(false),
    m_mode(FireMode::None) {}

std::expected<void, StarException> FireableItem::initializeFireable(Json const& params) {
  auto initialized = setParams(params);
  if (!initialized)
    return std::unexpected(std::move(initialized.error()));
  m_fireableParams = params;
  return {};
}

FireableItem::FireableItem(FireableItem const& rhs) : ToolUserItem(rhs), StatusEffectItem(rhs) {
  m_fireTimer = rhs.m_fireTimer;
  m_cooldownTime = rhs.m_cooldownTime;
  m_windupTime = rhs.m_windupTime;
  m_fireWhenReady = rhs.m_fireWhenReady;
  m_startWhenReady = rhs.m_startWhenReady;
  m_cooldown = rhs.m_cooldown;
  m_alreadyInit = rhs.m_alreadyInit;
  m_requireEdgeTrigger = rhs.m_requireEdgeTrigger;
  m_attemptedFire = rhs.m_attemptedFire;
  m_fireOnRelease = rhs.m_fireOnRelease;
  m_timeFiring = rhs.m_timeFiring;
  m_startTimingFire = rhs.m_startTimingFire;
  m_inUse = rhs.m_inUse;
  m_walkWhileFiring = rhs.m_walkWhileFiring;
  m_stopWhileFiring = rhs.m_stopWhileFiring;
  m_fireableParams = rhs.m_fireableParams;
  m_handPosition = rhs.m_handPosition;
  m_mode = rhs.m_mode;
}

std::expected<void, StarException> FireableItem::init(ToolUserEntity& owner, ToolHand hand) {
  auto initialized = ToolUserItem::init(owner, hand);
  if (!initialized)
    return std::unexpected(std::move(initialized.error()));
  m_fireWhenReady = false;
  m_startWhenReady = false;
  auto scriptValue = m_fireableParams.opt("scripts");
  if (!scriptValue)
    return std::unexpected(std::move(scriptValue.error()));
  auto mode = entityMode();
  if (!mode)
    return std::unexpected(std::move(mode.error()));
  if (*mode == EntityMode::Master && *scriptValue) {
    auto scripts = jsonToStringList(**scriptValue);
    if (!scripts)
      return std::unexpected(std::move(scripts.error()));
    if (!m_scriptComponent) {
      m_scriptComponent.emplace(owner.root());
      auto setScripts = m_scriptComponent->setScripts(std::move(*scripts));
      if (!setScripts)
        return std::unexpected(std::move(setScripts.error()));
    }
    auto item = rconvert<Item>(*this);
    if (!item)
      return std::unexpected(std::move(item.error()));
    auto configCallbacks = LuaBindings::makeConfigCallbacks(
      [item = *item](String const& name, Json const& defaultValue) -> std::expected<Json, StarException> {
        return item.get().instanceValue(name, defaultValue);
      });
    if (!configCallbacks)
      return std::unexpected(std::move(configCallbacks.error()));
    auto addedConfig = m_scriptComponent->addCallbacks("config", std::move(*configCallbacks));
    if (!addedConfig)
      return std::unexpected(std::move(addedConfig.error()));
    auto fireableCallbacks = LuaBindings::makeFireableItemCallbacks(*this);
    if (!fireableCallbacks)
      return std::unexpected(std::move(fireableCallbacks.error()));
    auto addedFireable = m_scriptComponent->addCallbacks("fireableItem", std::move(*fireableCallbacks));
    if (!addedFireable)
      return std::unexpected(std::move(addedFireable.error()));
    auto itemCallbacks = LuaBindings::makeItemCallbacks(item->get());
    if (!itemCallbacks)
      return std::unexpected(std::move(itemCallbacks.error()));
    auto addedItem = m_scriptComponent->addCallbacks("item", std::move(*itemCallbacks));
    if (!addedItem)
      return std::unexpected(std::move(addedItem.error()));
    auto itemWorld = world();
    if (!itemWorld)
      return std::unexpected(std::move(itemWorld.error()));
    return recoverLuaScriptError(m_scriptComponent->init(itemWorld->get()));
  }
  return {};
}

std::expected<void, StarException> FireableItem::uninit() {
  std::optional<StarException> error;
  auto remember = [&](auto result) {
    if (!result) {
      if (error)
        *error = error->with_context(result.error().what());
      else
        error = std::move(result.error());
    }
  };
  if (m_scriptComponent) {
    remember(recoverLuaScriptError(m_scriptComponent->uninit()));
    for (auto const& name : std::initializer_list<std::string_view>{"config", "fireableItem", "item"})
      remember(m_scriptComponent->removeCallbacks(name));
  }
  remember(ToolUserItem::uninit());
  if (error)
    return std::unexpected(std::move(*error));
  return {};
}

std::expected<void, StarException> FireableItem::fire(FireMode mode, bool, bool edgeTriggered) {
  m_attemptedFire = true;
  if (ready()) {
    m_inUse = true;
    m_startTimingFire = true;
    m_mode = mode;
    if (!m_requireEdgeTrigger || edgeTriggered) {
      setFireTimer(windupTime() + cooldownTime());
      if (!m_fireOnRelease) {
        m_fireWhenReady = true;
        m_startWhenReady = true;
      }
    }
  }

  if (m_scriptComponent) {
    auto invoked = recoverLuaScriptError(m_scriptComponent->invoke("attemptedFire"));
    if (!invoked)
      return std::unexpected(std::move(invoked.error()));
  }
  return {};
}

std::expected<void, StarException> FireableItem::endFire(FireMode mode, bool) {
  if (m_scriptComponent) {
    auto invoked = recoverLuaScriptError(m_scriptComponent->invoke("endFire"));
    if (!invoked)
      return std::unexpected(std::move(invoked.error()));
  }
  m_attemptedFire = false;
  if (m_fireOnRelease && m_timeFiring) {
    m_mode = mode;
    auto cooldown = triggerCooldown();
    if (!cooldown)
      return std::unexpected(std::move(cooldown.error()));
    return fireTriggered();
  }
  return {};
}

FireMode FireableItem::fireMode() const {
  return m_mode;
}

float FireableItem::cooldownTime() const {
  return m_cooldownTime;
}

void FireableItem::setCooldownTime(float cooldownTime) {
  m_cooldownTime = cooldownTime;
}

float FireableItem::fireTimer() const {
  return m_fireTimer;
}

void FireableItem::setFireTimer(float fireTimer) {
  m_fireTimer = fireTimer;
}

bool FireableItem::ready() const {
  return fireTimer() <= 0;
}

bool FireableItem::firing() const {
  return m_timeFiring > 0;
}

bool FireableItem::inUse() const {
  return m_inUse;
}

bool FireableItem::walkWhileFiring() const {
  return m_walkWhileFiring;
}

bool FireableItem::stopWhileFiring() const {
  return m_stopWhileFiring;
}

std::expected<bool, StarException> FireableItem::windup() const {
  if (ready())
    return false;

  if (m_scriptComponent) {
    auto invoked = recoverLuaScriptError(m_scriptComponent->invoke("triggerWindup"));
    if (!invoked)
      return std::unexpected(std::move(invoked.error()));
  }

  return fireTimer() > cooldownTime();
}

std::expected<void, StarException> FireableItem::update(float dt, FireMode fireMode, bool shifting, HashSet<MoveControlType> const&) {
  if (m_scriptComponent) {
    auto modeName = enumName<FireModeNames>(fireMode);
    if (!modeName)
      return std::unexpected(std::move(modeName.error()));
    auto invoked = recoverLuaScriptError(m_scriptComponent->invoke("update", dt, *modeName, shifting));
    if (!invoked)
      return std::unexpected(std::move(invoked.error()));
  }

  if (m_attemptedFire) {
    if (m_startTimingFire) {
      m_timeFiring += dt;
      if (m_scriptComponent) {
        auto invoked = recoverLuaScriptError(m_scriptComponent->invoke("continueFire", dt));
        if (!invoked)
          return std::unexpected(std::move(invoked.error()));
      }
    }
  } else {
    m_timeFiring = 0.0f;
    m_startTimingFire = false;
  }
  m_attemptedFire = false;

  auto mode = entityMode();
  if (!mode)
    return std::unexpected(std::move(mode.error()));
  if (*mode == EntityMode::Master) {
    if (fireTimer() > 0.0f) {
      setFireTimer(fireTimer() - dt);
      if (fireTimer() < 0.0f) {
        setFireTimer(0.0f);
        m_inUse = false;
      }
    }
    if (fireTimer() <= 0) {
      m_cooldown = false;
    }
    if (m_startWhenReady) {
      m_startWhenReady = false;
      auto started = startTriggered();
      if (!started)
        return std::unexpected(std::move(started.error()));
    }
    if (m_fireWhenReady) {
      if (fireTimer() <= cooldownTime()) {
        m_fireWhenReady = false;
        auto fired = fireTriggered();
        if (!fired)
          return std::unexpected(std::move(fired.error()));
      }
    }
  }
  return {};
}

std::expected<void, StarException> FireableItem::triggerCooldown() {
  setFireTimer(cooldownTime());
  m_cooldown = true;
  if (m_scriptComponent) {
    auto invoked = recoverLuaScriptError(m_scriptComponent->invoke("triggerCooldown"));
    if (!invoked)
      return std::unexpected(std::move(invoked.error()));
  }
  return {};
}

bool FireableItem::coolingDown() const {
  return m_cooldown;
}

void FireableItem::setCoolingDown(bool coolingdown) {
  m_cooldown = coolingdown;
}

float FireableItem::timeFiring() const {
  return m_timeFiring;
}

void FireableItem::setTimeFiring(float timeFiring) {
  m_timeFiring = timeFiring;
}

Vec2F FireableItem::handPosition() const {
  return m_handPosition;
}

Vec2F FireableItem::firePosition() const {
  return Vec2F();
}

std::expected<Json, StarException> FireableItem::fireableParam(String const& key) const {
  return m_fireableParams.get(key);
}

std::expected<Json, StarException> FireableItem::fireableParam(String const& key, Json const& defaultVal) const {
  return m_fireableParams.get(key, defaultVal);
}

bool FireableItem::validAimPos(Vec2F const&) {
  return true;
}

std::expected<void, StarException> FireableItem::setParams(Json const& params) {
  if (!m_alreadyInit) {
    auto windup = params.getFloat("windupTime", 0.0f);
    if (!windup)
      return std::unexpected(std::move(windup.error()));
    m_windupTime = *windup;
    auto fireTime = params.getFloat("fireTime", 0.15f);
    if (!fireTime)
      return std::unexpected(std::move(fireTime.error()));
    auto cooldown = params.getFloat("cooldown", *fireTime - m_windupTime);
    if (!cooldown)
      return std::unexpected(std::move(cooldown.error()));
    m_cooldownTime = *cooldown;
    auto hasPosition = params.contains("handPosition");
    if (!hasPosition)
      return std::unexpected(std::move(hasPosition.error()));
    if (*hasPosition) {
      auto position = params.get("handPosition").and_then(jsonToVec2F);
      if (!position)
        return std::unexpected(std::move(position.error()));
      m_handPosition = *position;
    }
    auto edge = params.getBool("edgeTrigger", false);
    if (!edge)
      return std::unexpected(std::move(edge.error()));
    m_requireEdgeTrigger = *edge;
    auto release = params.getBool("fireOnRelease", false);
    if (!release)
      return std::unexpected(std::move(release.error()));
    m_fireOnRelease = *release;
    auto walk = params.getBool("walkWhileFiring", false);
    if (!walk)
      return std::unexpected(std::move(walk.error()));
    m_walkWhileFiring = *walk;
    auto stop = params.getBool("stopWhileFiring", false);
    if (!stop)
      return std::unexpected(std::move(stop.error()));
    m_stopWhileFiring = *stop;
    m_alreadyInit = true;
  }
  return {};
}

std::expected<void, StarException> FireableItem::setFireableParam(String const& key, Json const& value) {
  auto updated = m_fireableParams.set(key, value);
  if (!updated)
    return std::unexpected(std::move(updated.error()));
  m_fireableParams = std::move(*updated);
  return {};
}

std::expected<void, StarException> FireableItem::startTriggered() {
  if (m_scriptComponent) {
    auto invoked = recoverLuaScriptError(m_scriptComponent->invoke("startTriggered"));
    if (!invoked)
      return std::unexpected(std::move(invoked.error()));
  }
  return {};
}

std::expected<void, StarException> FireableItem::fireTriggered() {
  if (m_scriptComponent) {
    auto invoked = recoverLuaScriptError(m_scriptComponent->invoke("fireTriggered"));
    if (!invoked)
      return std::unexpected(std::move(invoked.error()));
  }
  return {};
}

std::expected<Vec2F, StarException> FireableItem::ownerFirePosition() const {
  if (!initialized())
    return std::unexpected(StarException("FireableItem uninitialized in ownerFirePosition"));
  auto ownerResult = owner();
  if (!ownerResult)
    return std::unexpected(std::move(ownerResult.error()));
  auto handResult = hand();
  if (!handResult)
    return std::unexpected(std::move(handResult.error()));
  return ownerResult->get().handPosition(*handResult, (this->firePosition() - handPosition()) / TilePixels);
}

float FireableItem::windupTime() const {
  return m_windupTime;
}

void FireableItem::setWindupTime(float time) {
  m_windupTime = time;
}

std::expected<List<PersistentStatusEffect>, StarException> FireableItem::statusEffects() const {
  return List<PersistentStatusEffect>{};
}

}

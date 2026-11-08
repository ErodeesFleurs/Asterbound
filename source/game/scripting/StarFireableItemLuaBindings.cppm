module;
import std;
import star.string;
import star.format;
import star.json;
import star.json_extra;
import star.id_map;
import star.lua;
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

import star.lua_game_converters;
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

export module star.fireable_item_lua_bindings;

export namespace Star::LuaBindings {
  [[nodiscard]] std::expected<LuaCallbacks, StarException> makeFireableItemCallbacks(FireableItem& fireableItem);
}

namespace Star::LuaBindings::FireableItemCallbacks {
  [[nodiscard]] std::expected<void, StarException> fire(FireableItem& fireableItem, std::optional<String> const& mode);
  [[nodiscard]] std::expected<void, StarException> triggerCooldown(FireableItem& fireableItem);
  void setCooldown(FireableItem& fireableItem, float cooldownTime);
  void endCooldown(FireableItem& fireableItem);
  float cooldownTime(FireableItem& fireableItem);
  [[nodiscard]] std::expected<Json, StarException> fireableParam(FireableItem& fireableItem, String const& name, Json const& def);
  String fireMode(FireableItem& fireableItem);
  bool ready(FireableItem& fireableItem);
  bool firing(FireableItem& fireableItem);
  [[nodiscard]] std::expected<bool, StarException> windingUp(FireableItem& fireableItem);
  bool coolingDown(FireableItem& fireableItem);
  [[nodiscard]] std::expected<bool, StarException> ownerFullEnergy(FireableItem& fireableItem);
  [[nodiscard]] std::expected<bool, StarException> ownerEnergy(FireableItem& fireableItem);
  [[nodiscard]] std::expected<bool, StarException> ownerEnergyLocked(FireableItem& fireableItem);
  [[nodiscard]] std::expected<bool, StarException> ownerConsumeEnergy(FireableItem& fireableItem, float energy);
}

namespace Star {

std::expected<LuaCallbacks, StarException> LuaBindings::makeFireableItemCallbacks(FireableItem& fireableItem) {
  LuaCallbacks callbacks;

  if (auto registration = callbacks.registerCallbackWithSignature<void, std::optional<String>>(
        "fire",
        [&fireableItem](std::optional<String> const& mode) { return FireableItemCallbacks::fire(fireableItem, mode); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.registerCallbackWithSignature<void>(
        "triggerCooldown", [&fireableItem]() { return FireableItemCallbacks::triggerCooldown(fireableItem); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.registerCallbackWithSignature<void, float>(
        "setCooldown", [&fireableItem](float time) { return FireableItemCallbacks::setCooldown(fireableItem, time); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.registerCallbackWithSignature<void>(
        "endCooldown", [&fireableItem]() { return FireableItemCallbacks::endCooldown(fireableItem); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.registerCallbackWithSignature<float>(
        "cooldownTime", [&fireableItem]() { return FireableItemCallbacks::cooldownTime(fireableItem); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.registerCallbackWithSignature<Json, String, Json>(
        "fireableParam",
        [&fireableItem](String const& name, Json const& def) {
          return FireableItemCallbacks::fireableParam(fireableItem, name, def);
        });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.registerCallbackWithSignature<String>(
        "fireMode", [&fireableItem]() { return FireableItemCallbacks::fireMode(fireableItem); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.registerCallbackWithSignature<bool>(
        "ready", [&fireableItem]() { return FireableItemCallbacks::ready(fireableItem); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.registerCallbackWithSignature<bool>(
        "firing", [&fireableItem]() { return FireableItemCallbacks::firing(fireableItem); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.registerCallbackWithSignature<bool>(
        "windingUp", [&fireableItem]() { return FireableItemCallbacks::windingUp(fireableItem); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.registerCallbackWithSignature<bool>(
        "coolingDown", [&fireableItem]() { return FireableItemCallbacks::coolingDown(fireableItem); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.registerCallbackWithSignature<bool>(
        "ownerFullEnergy", [&fireableItem]() { return FireableItemCallbacks::ownerFullEnergy(fireableItem); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.registerCallbackWithSignature<bool>(
        "ownerEnergy", [&fireableItem]() { return FireableItemCallbacks::ownerEnergy(fireableItem); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.registerCallbackWithSignature<bool>(
        "ownerEnergyLocked", [&fireableItem]() { return FireableItemCallbacks::ownerEnergyLocked(fireableItem); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.registerCallbackWithSignature<bool, float>(
        "ownerConsumeEnergy",
        [&fireableItem](float energy) { return FireableItemCallbacks::ownerConsumeEnergy(fireableItem, energy); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.registerCallbackWithSignature<Vec2F>("ownerAimPosition", [&fireableItem]() -> std::expected<Vec2F, StarException> {
      auto owner = fireableItem.owner();
      if (!owner)
        return std::unexpected(std::move(owner.error()));
      return owner->get().aimPosition();
    }); !registration)
    return std::unexpected(std::move(registration.error()));

  return callbacks;
}

// Triggers the item to fire
//
// @param[opt] fireMode fireMode to trigger; should be 'Primary' or 'Alt'
// (defaults to Primary)
// @return nil
std::expected<void, StarException> LuaBindings::FireableItemCallbacks::fire(FireableItem& fireableItem, std::optional<String> const& mode) {
  auto fireMode = FireMode::Primary;
  if (mode) {
    auto alt = mode->equals_ignore_case("Alt");
    if (!alt)
      return std::unexpected(std::move(alt.error()));
    if (*alt) {
      fireMode = FireMode::Alt;
    } else {
      auto primary = mode->equals_ignore_case("Primary");
      if (!primary)
        return std::unexpected(std::move(primary.error()));
      if (!*primary)
        return std::unexpected(StarException("Invalid fire mode specified! Must be 'Primary' or 'Alt'"));
    }
  }
  if (fireableItem.ready())
    return fireableItem.fire(fireMode, false, true);
  return {};
}

// Triggers the item's cooldown
//
// @return nil
std::expected<void, StarException> LuaBindings::FireableItemCallbacks::triggerCooldown(FireableItem& fireableItem) {
  return fireableItem.triggerCooldown();
}

// Sets the item's current cooldown to the specified time (will not change the
// default cooldown)
//
// @param cooldownTime time in seconds for this cooldown period
// @return nil
void LuaBindings::FireableItemCallbacks::setCooldown(FireableItem& fireableItem, float cooldownTime) {
  fireableItem.setCoolingDown(cooldownTime > 0);
  fireableItem.setFireTimer(cooldownTime);
}

// Ends the item's cooldown, readying it to fire
//
// @return nil
void LuaBindings::FireableItemCallbacks::endCooldown(FireableItem& fireableItem) {
  fireableItem.setCoolingDown(false);
  fireableItem.setFireTimer(0);
}

// Returns the item's default cooldown time
//
// @return the default cooldown time in seconds
float LuaBindings::FireableItemCallbacks::cooldownTime(FireableItem& fireableItem) {
  return fireableItem.cooldownTime();
}

// Gets the value of a configuration option for this item.
//
// @string name the name of the configuration parameter to get, as specified in
// the
// item's configuration
// @param[opt] default a default value that will be returned if the given
// configuration key does not exist in the item's configuration.
// @return[1] nil if the item has no configuration option with the given name,
// and no default value is given.
// @return[2] the value of the configuration option, or the given default value.
std::expected<Json, StarException> LuaBindings::FireableItemCallbacks::fireableParam(
    FireableItem& fireableItem, String const& name, Json const& def) {
  return fireableItem.fireableParam(name, def);
}

// Gets the current fire mode of the item
//
// @return string representation of the fire mode: 'Primary', 'Alt' or 'None'
String LuaBindings::FireableItemCallbacks::fireMode(FireableItem& fireableItem) {
  if (fireableItem.fireMode() == FireMode::Primary)
    return "Primary";
  else if (fireableItem.fireMode() == FireMode::Alt)
    return "Alt";
  else
    return "None";
}

// Determine whether the item is currently ready to be fired
//
// @return true if the item is ready
bool LuaBindings::FireableItemCallbacks::ready(FireableItem& fireableItem) {
  return fireableItem.ready();
}

// Determine whether the item is currently firing
//
// @return true if the item is firing
bool LuaBindings::FireableItemCallbacks::firing(FireableItem& fireableItem) {
  return fireableItem.firing();
}

// Determine whether the item is currently winding up to fire
//
// @return true if the item is winding up
std::expected<bool, StarException> LuaBindings::FireableItemCallbacks::windingUp(FireableItem& fireableItem) {
  return fireableItem.windup();
}

// Determine whether the item is currently cooling down from firing
//
// @return true if the item is cooling down
bool LuaBindings::FireableItemCallbacks::coolingDown(FireableItem& fireableItem) {
  return fireableItem.coolingDown();
}

// Determine whether the item's owner has full energy
//
// @return true if the owner's energy is full
std::expected<bool, StarException> LuaBindings::FireableItemCallbacks::ownerFullEnergy(FireableItem& fireableItem) {
  auto owner = fireableItem.owner();
  if (!owner)
    return std::unexpected(std::move(owner.error()));
  return owner->get().fullEnergy();
}

// Determine the amount of energy that the item's owner currently has
//
// @return the owner's current energy
std::expected<bool, StarException> LuaBindings::FireableItemCallbacks::ownerEnergy(FireableItem& fireableItem) {
  auto owner = fireableItem.owner();
  if (!owner)
    return std::unexpected(std::move(owner.error()));
  auto energy = owner->get().energy();
  if (!energy)
    return std::unexpected(std::move(energy.error()));
  return static_cast<bool>(*energy);
}

// Determine whether the item's owner's energy pool is currently locked
//
// @return true if the owner's energy pool is currently locked
std::expected<bool, StarException> LuaBindings::FireableItemCallbacks::ownerEnergyLocked(FireableItem& fireableItem) {
  auto owner = fireableItem.owner();
  if (!owner)
    return std::unexpected(std::move(owner.error()));
  return owner->get().energyLocked();
}

// Attempt to consume the specified amount of the owner's energy
//
// @param amount the amount of energy to consume
// @return true if the energy was consumed successfully
std::expected<bool, StarException> LuaBindings::FireableItemCallbacks::ownerConsumeEnergy(FireableItem& fireableItem, float energy) {
  auto owner = fireableItem.owner();
  if (!owner)
    return std::unexpected(std::move(owner.error()));
  return owner->get().consumeEnergy(energy);
}

}

module;
import std;
import star.format;
import star.json;
import star.json_extra;
import star.lua;
import star.list;
import star.observer_stream;
import star.strong_typedef;
import star.data_stream;
import star.id_map;
import star.either;
import star.game_types;
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
import star.bi_map;
import star.audio;
import star.map;
import star.set;


// Match client include order for SIMD intrinsics used by xxhash.
import star.status_types;
import star.damage_types;
import star.world_geometry;
import star.damage;
import star.lua_components;
import star.lua_actor_movement_component;
import star.animated_part_set;
import star.drawable;
import star.animation;
import star.particle;
import star.mixer;
import star.light_source;
import star.networked_animator;
import star.entity_rendering_types;
import star.stat_collection;
import star.status_effect_database;
import star.status_controller;
import star.lua_game_converters;

module star.status_controller_lua_bindings;

namespace Star::LuaBindings::StatusControllerCallbacks {
  Json statusProperty(StatusController& statController, String const& arg1, Json const& arg2);
  void setStatusProperty(StatusController& statController, String const& arg1, Json const& arg2);
  float stat(StatusController& statController, String const& arg1);
  bool statPositive(StatusController& statController, String const& arg1);
  StringList resourceNames(StatusController& statController);
  bool isResource(StatusController& statController, String const& arg1);
  float resource(StatusController& statController, String const& arg1);
  bool resourcePositive(StatusController& statController, String const& arg1);
  [[nodiscard]] std::expected<void, StarException> setResource(StatusController& statController, String const& arg1, float arg2);
  [[nodiscard]] std::expected<void, StarException> modifyResource(StatusController& statController, String const& arg1, float arg2);
  float giveResource(StatusController& statController, String const& resourceName, float amount);
  [[nodiscard]] std::expected<bool, StarException> consumeResource(StatusController& statController, String const& arg1, float arg2);
  [[nodiscard]] std::expected<bool, StarException> overConsumeResource(StatusController& statController, String const& arg1, float arg2);
  [[nodiscard]] std::expected<bool, StarException> resourceLocked(StatusController& statController, String const& arg1);
  [[nodiscard]] std::expected<void, StarException> setResourceLocked(StatusController& statController, String const& arg1, bool arg2);
  [[nodiscard]] std::expected<void, StarException> resetResource(StatusController& statController, String const& arg1);
  [[nodiscard]] std::expected<void, StarException> resetAllResources(StatusController& statController);
  [[nodiscard]] std::expected<std::optional<float>, StarException> resourceMax(StatusController& statController, String const& arg1);
  [[nodiscard]] std::expected<std::optional<float>, StarException> resourcePercentage(StatusController& statController, String const& arg1);
  [[nodiscard]] std::expected<float, StarException> setResourcePercentage(StatusController& statController, String const& arg1, float arg2);
  [[nodiscard]] std::expected<float, StarException> modifyResourcePercentage(StatusController& statController, String const& arg1, float arg2);
  [[nodiscard]] std::expected<JsonArray, StarException> getPersistentEffects(StatusController& statController, String const& arg1);
  [[nodiscard]] std::expected<void, StarException> addPersistentEffect(StatusController& statController, String const& arg1, Json const& arg2);
  [[nodiscard]] std::expected<void, StarException> addPersistentEffects(StatusController& statController, String const& arg1, JsonArray const& arg2);
  [[nodiscard]] std::expected<void, StarException> setPersistentEffects(StatusController& statController, String const& arg1, JsonArray const& arg2);
  [[nodiscard]] std::expected<void, StarException> clearPersistentEffects(StatusController& statController, String const& arg1);
  [[nodiscard]] std::expected<void, StarException> clearAllPersistentEffects(StatusController& statController);
  [[nodiscard]] std::expected<void, StarException> addEphemeralEffect(StatusController& statController,
      String const& uniqueEffect,
      std::optional<float> duration,
      std::optional<EntityId> sourceEntityId);
  [[nodiscard]] std::expected<void, StarException> addEphemeralEffects(StatusController& statController, JsonArray const& arg1, std::optional<EntityId> sourceEntityId);
  [[nodiscard]] std::expected<void, StarException> removeEphemeralEffect(StatusController& statController, String const& arg1);
  [[nodiscard]] std::expected<void, StarException> clearEphemeralEffects(StatusController& statController);
  [[nodiscard]] std::expected<LuaTupleReturn<List<Json>, std::uint64_t>, StarException> damageTakenSince(StatusController& statController, std::optional<std::uint64_t> timestep);
  [[nodiscard]] std::expected<LuaTupleReturn<List<Json>, std::uint64_t>, StarException> inflictedHitsSince(StatusController& statController, std::optional<std::uint64_t> timestep);
  [[nodiscard]] std::expected<LuaTupleReturn<List<Json>, std::uint64_t>, StarException> inflictedDamageSince(
      StatusController& statController, std::optional<std::uint64_t> timestep);
  List<JsonArray> activeUniqueStatusEffectSummary(StatusController& statController);
  bool uniqueStatusEffectActive(StatusController& statController, String const& effectName);
}

namespace Star {

std::expected<LuaCallbacks, StarException> LuaBindings::makeStatusControllerCallbacks(StatusController& statController) {
  LuaCallbacks callbacks;

  if (auto result = callbacks.registerCallbackWithSignature<Json, String, Json>(
        "statusProperty",
        [&statController](String const& name, Json const& def) {
          return StatusControllerCallbacks::statusProperty(statController, name, def);
        });
      !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.registerCallbackWithSignature<void, String, Json>(
        "setStatusProperty",
        [&statController](String const& name, Json const& value) {
          return StatusControllerCallbacks::setStatusProperty(statController, name, value);
        });
      !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.registerCallbackWithSignature<float, String>(
        "stat",
        [&statController](String const& name) { return StatusControllerCallbacks::stat(statController, name); });
      !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.registerCallbackWithSignature<bool, String>(
        "statPositive",
        [&statController](String const& name) {
          return StatusControllerCallbacks::statPositive(statController, name);
        });
      !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.registerCallbackWithSignature<StringList>(
        "resourceNames", [&statController]() { return StatusControllerCallbacks::resourceNames(statController); });
      !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.registerCallbackWithSignature<bool, String>(
        "isResource",
        [&statController](String const& name) { return StatusControllerCallbacks::isResource(statController, name); });
      !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.registerCallbackWithSignature<float, String>(
        "resource",
        [&statController](String const& name) { return StatusControllerCallbacks::resource(statController, name); });
      !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.registerCallbackWithSignature<bool, String>(
        "resourcePositive",
        [&statController](String const& name) {
          return StatusControllerCallbacks::resourcePositive(statController, name);
        });
      !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.registerCallbackWithSignature<void, String, float>(
        "setResource",
        [&statController](String const& name, float value) {
          return StatusControllerCallbacks::setResource(statController, name, value);
        });
      !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.registerCallbackWithSignature<void, String, float>(
        "modifyResource",
        [&statController](String const& name, float value) {
          return StatusControllerCallbacks::modifyResource(statController, name, value);
        });
      !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.registerCallbackWithSignature<float, String, float>(
        "giveResource",
        [&statController](String const& name, float amount) {
          return StatusControllerCallbacks::giveResource(statController, name, amount);
        });
      !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.registerCallbackWithSignature<bool, String, float>(
        "consumeResource",
        [&statController](String const& name, float amount) {
          return StatusControllerCallbacks::consumeResource(statController, name, amount);
        });
      !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.registerCallbackWithSignature<bool, String, float>(
        "overConsumeResource",
        [&statController](String const& name, float amount) {
          return StatusControllerCallbacks::overConsumeResource(statController, name, amount);
        });
      !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.registerCallbackWithSignature<bool, String>(
        "resourceLocked",
        [&statController](String const& name) {
          return StatusControllerCallbacks::resourceLocked(statController, name);
        });
      !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.registerCallbackWithSignature<void, String, bool>(
        "setResourceLocked",
        [&statController](String const& name, bool locked) {
          return StatusControllerCallbacks::setResourceLocked(statController, name, locked);
        });
      !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.registerCallbackWithSignature<void, String>(
        "resetResource",
        [&statController](String const& name) {
          return StatusControllerCallbacks::resetResource(statController, name);
        });
      !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.registerCallbackWithSignature<void>(
        "resetAllResources",
        [&statController]() { return StatusControllerCallbacks::resetAllResources(statController); });
      !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.registerCallbackWithSignature<std::optional<float>, String>(
        "resourceMax",
        [&statController](String const& name) { return StatusControllerCallbacks::resourceMax(statController, name); });
      !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.registerCallbackWithSignature<std::optional<float>, String>(
        "resourcePercentage",
        [&statController](String const& name) {
          return StatusControllerCallbacks::resourcePercentage(statController, name);
        });
      !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.registerCallbackWithSignature<float, String, float>(
        "setResourcePercentage",
        [&statController](String const& name, float value) {
          return StatusControllerCallbacks::setResourcePercentage(statController, name, value);
        });
      !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.registerCallbackWithSignature<float, String, float>(
        "modifyResourcePercentage",
        [&statController](String const& name, float value) {
          return StatusControllerCallbacks::modifyResourcePercentage(statController, name, value);
        });
      !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.registerCallbackWithSignature<JsonArray, String>(
        "getPersistentEffects",
        [&statController](String const& group) {
          return StatusControllerCallbacks::getPersistentEffects(statController, group);
        });
      !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.registerCallbackWithSignature<void, String, Json>(
        "addPersistentEffect",
        [&statController](String const& group, Json const& effect) {
          return StatusControllerCallbacks::addPersistentEffect(statController, group, effect);
        });
      !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.registerCallbackWithSignature<void, String, JsonArray>(
        "addPersistentEffects",
        [&statController](String const& group, JsonArray const& effects) {
          return StatusControllerCallbacks::addPersistentEffects(statController, group, effects);
        });
      !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.registerCallbackWithSignature<void, String, JsonArray>(
        "setPersistentEffects",
        [&statController](String const& group, JsonArray const& effects) {
          return StatusControllerCallbacks::setPersistentEffects(statController, group, effects);
        });
      !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.registerCallbackWithSignature<void, String>(
        "clearPersistentEffects",
        [&statController](String const& group) {
          return StatusControllerCallbacks::clearPersistentEffects(statController, group);
        });
      !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.registerCallbackWithSignature<void>(
        "clearAllPersistentEffects",
        [&statController]() { return StatusControllerCallbacks::clearAllPersistentEffects(statController); });
      !result)
    return std::unexpected(std::move(result.error()));
  if (auto result =
        callbacks.registerCallbackWithSignature<void, String, std::optional<float>, std::optional<EntityId>>(
          "addEphemeralEffect",
          [&statController](String const& effect, std::optional<float> duration, std::optional<EntityId> source) {
            return StatusControllerCallbacks::addEphemeralEffect(statController, effect, duration, source);
          });
      !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.registerCallbackWithSignature<void, JsonArray, std::optional<EntityId>>(
        "addEphemeralEffects",
        [&statController](JsonArray const& effects, std::optional<EntityId> source) {
          return StatusControllerCallbacks::addEphemeralEffects(statController, effects, source);
        });
      !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.registerCallbackWithSignature<void, String>(
        "removeEphemeralEffect",
        [&statController](String const& effect) {
          return StatusControllerCallbacks::removeEphemeralEffect(statController, effect);
        });
      !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.registerCallbackWithSignature<void>(
        "clearEphemeralEffects",
        [&statController]() { return StatusControllerCallbacks::clearEphemeralEffects(statController); });
      !result)
    return std::unexpected(std::move(result.error()));
  if (auto result =
        callbacks
          .registerCallbackWithSignature<LuaTupleReturn<List<Json>, std::uint64_t>, std::optional<std::uint64_t>>(
            "damageTakenSince",
            [&statController](std::optional<std::uint64_t> timestep) {
              return StatusControllerCallbacks::damageTakenSince(statController, timestep);
            });
      !result)
    return std::unexpected(std::move(result.error()));
  if (auto result =
        callbacks
          .registerCallbackWithSignature<LuaTupleReturn<List<Json>, std::uint64_t>, std::optional<std::uint64_t>>(
            "inflictedHitsSince",
            [&statController](std::optional<std::uint64_t> timestep) {
              return StatusControllerCallbacks::inflictedHitsSince(statController, timestep);
            });
      !result)
    return std::unexpected(std::move(result.error()));
  if (auto result =
        callbacks
          .registerCallbackWithSignature<LuaTupleReturn<List<Json>, std::uint64_t>, std::optional<std::uint64_t>>(
            "inflictedDamageSince",
            [&statController](std::optional<std::uint64_t> timestep) {
              return StatusControllerCallbacks::inflictedDamageSince(statController, timestep);
            });
      !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.registerCallbackWithSignature<List<JsonArray>>(
        "activeUniqueStatusEffectSummary",
        [&statController]() { return StatusControllerCallbacks::activeUniqueStatusEffectSummary(statController); });
      !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.registerCallbackWithSignature<bool, String>(
        "uniqueStatusEffectActive",
        [&statController](String const& name) {
          return StatusControllerCallbacks::uniqueStatusEffectActive(statController, name);
        });
      !result)
    return std::unexpected(std::move(result.error()));

  if (auto result = callbacks.registerCallbackWithSignature<Directives>(
        "primaryDirectives", [&statController]() -> Directives const& { return statController.primaryDirectives(); });
      !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.registerCallback("setPrimaryDirectives", [&statController](std::optional<String> const& directives) { statController.setPrimaryDirectives(directives.value_or(String{})); }); !result) return std::unexpected(std::move(result.error()));

  if (auto result = callbacks.registerCallbackWithSignature<void, DamageRequest>(
        "applySelfDamageRequest",
        [&statController](DamageRequest&& request) {
          return statController.applySelfDamageRequest(std::move(request));
        });
      !result)
    return std::unexpected(std::move(result.error()));

  return callbacks;
}

Json LuaBindings::StatusControllerCallbacks::statusProperty(
    StatusController& statController, String const& arg1, Json const& arg2) {
  return statController.statusProperty(arg1, arg2);
}

void LuaBindings::StatusControllerCallbacks::setStatusProperty(
    StatusController& statController, String const& arg1, Json const& arg2) {
  statController.setStatusProperty(arg1, arg2);
}

float LuaBindings::StatusControllerCallbacks::stat(StatusController& statController, String const& arg1) {
  return statController.stat(arg1);
}

bool LuaBindings::StatusControllerCallbacks::statPositive(StatusController& statController, String const& arg1) {
  return statController.statPositive(arg1);
}

StringList LuaBindings::StatusControllerCallbacks::resourceNames(StatusController& statController) {
  return statController.resourceNames();
}

bool LuaBindings::StatusControllerCallbacks::isResource(StatusController& statController, String const& arg1) {
  return statController.isResource(arg1);
}

float LuaBindings::StatusControllerCallbacks::resource(StatusController& statController, String const& arg1) {
  return statController.resource(arg1);
}

bool LuaBindings::StatusControllerCallbacks::resourcePositive(StatusController& statController, String const& arg1) {
  return statController.resourcePositive(arg1);
}

std::expected<void, StarException> LuaBindings::StatusControllerCallbacks::setResource(
    StatusController& statController, String const& arg1, float arg2) {
  return statController.setResource(arg1, arg2);
}

std::expected<void, StarException> LuaBindings::StatusControllerCallbacks::modifyResource(
    StatusController& statController, String const& arg1, float arg2) {
  return statController.modifyResource(arg1, arg2);
}

float LuaBindings::StatusControllerCallbacks::giveResource(
    StatusController& statController, String const& resourceName, float amount) {
  return statController.giveResource(resourceName, amount);
}

std::expected<bool, StarException> LuaBindings::StatusControllerCallbacks::consumeResource(
    StatusController& statController, String const& arg1, float arg2) {
  return statController.consumeResource(arg1, arg2);
}

std::expected<bool, StarException> LuaBindings::StatusControllerCallbacks::overConsumeResource(
    StatusController& statController, String const& arg1, float arg2) {
  return statController.overConsumeResource(arg1, arg2);
}

std::expected<bool, StarException> LuaBindings::StatusControllerCallbacks::resourceLocked(StatusController& statController, String const& arg1) {
  return statController.resourceLocked(arg1);
}

std::expected<void, StarException> LuaBindings::StatusControllerCallbacks::setResourceLocked(
    StatusController& statController, String const& arg1, bool arg2) {
  return statController.setResourceLocked(arg1, arg2);
}

std::expected<void, StarException> LuaBindings::StatusControllerCallbacks::resetResource(StatusController& statController, String const& arg1) {
  return statController.resetResource(arg1);
}

std::expected<void, StarException> LuaBindings::StatusControllerCallbacks::resetAllResources(StatusController& statController) {
  return statController.resetAllResources();
}

std::expected<std::optional<float>, StarException> LuaBindings::StatusControllerCallbacks::resourceMax(StatusController& statController, String const& arg1) {
  return statController.resourceMax(arg1);
}

std::expected<std::optional<float>, StarException> LuaBindings::StatusControllerCallbacks::resourcePercentage(
    StatusController& statController, String const& arg1) {
  return statController.resourcePercentage(arg1);
}

std::expected<float, StarException> LuaBindings::StatusControllerCallbacks::setResourcePercentage(
    StatusController& statController, String const& arg1, float arg2) {
  return statController.setResourcePercentage(arg1, arg2);
}

std::expected<float, StarException> LuaBindings::StatusControllerCallbacks::modifyResourcePercentage(
    StatusController& statController, String const& arg1, float arg2) {
  return statController.modifyResourcePercentage(arg1, arg2);
}

std::expected<JsonArray, StarException> LuaBindings::StatusControllerCallbacks::getPersistentEffects(
    StatusController& statController, String const& arg1) {
  JsonArray result;
  for (auto const& effect : statController.getPersistentEffects(arg1)) {
    auto json = jsonFromPersistentStatusEffect(effect);
    if (!json) return std::unexpected(std::move(json.error()));
    result.push_back(std::move(*json));
  }
  return result;
}

std::expected<void, StarException> LuaBindings::StatusControllerCallbacks::addPersistentEffect(
    StatusController& statController, String const& arg1, Json const& arg2) {
  return addPersistentEffects(std::ref(statController), arg1, JsonArray{arg2});
}

std::expected<void, StarException> LuaBindings::StatusControllerCallbacks::addPersistentEffects(
    StatusController& statController, String const& arg1, JsonArray const& arg2) {
  List<PersistentStatusEffect> effects;
  for (auto const& json : arg2) {
    auto effect = jsonToPersistentStatusEffect(json);
    if (!effect) return std::unexpected(std::move(effect.error()));
    effects.push_back(std::move(*effect));
  }
  return statController.addPersistentEffects(arg1, effects);
}

std::expected<void, StarException> LuaBindings::StatusControllerCallbacks::setPersistentEffects(
    StatusController& statController, String const& arg1, JsonArray const& arg2) {
  List<PersistentStatusEffect> effects;
  for (auto const& json : arg2) {
    auto effect = jsonToPersistentStatusEffect(json);
    if (!effect) return std::unexpected(std::move(effect.error()));
    effects.push_back(std::move(*effect));
  }
  return statController.setPersistentEffects(arg1, effects);
}

std::expected<void, StarException> LuaBindings::StatusControllerCallbacks::clearPersistentEffects(
    StatusController& statController, String const& arg1) {
  return statController.clearPersistentEffects(arg1);
}

std::expected<void, StarException> LuaBindings::StatusControllerCallbacks::clearAllPersistentEffects(StatusController& statController) {
  return statController.clearAllPersistentEffects();
}

std::expected<void, StarException> LuaBindings::StatusControllerCallbacks::addEphemeralEffect(
    StatusController& statController, String const& arg1, std::optional<float> arg2, std::optional<EntityId> sourceEntityId) {
  return statController.addEphemeralEffect(
    EphemeralStatusEffect{.uniqueEffect = UniqueStatusEffect{arg1}, .duration = arg2}, sourceEntityId);
}

std::expected<void, StarException> LuaBindings::StatusControllerCallbacks::addEphemeralEffects(
    StatusController& statController, JsonArray const& arg1, std::optional<EntityId> sourceEntityId) {
  List<EphemeralStatusEffect> effects;
  for (auto const& json : arg1) {
    auto effect = jsonToEphemeralStatusEffect(json);
    if (!effect) return std::unexpected(std::move(effect.error()));
    effects.push_back(std::move(*effect));
  }
  return statController.addEphemeralEffects(effects, sourceEntityId);
}

std::expected<void, StarException> LuaBindings::StatusControllerCallbacks::removeEphemeralEffect(
    StatusController& statController, String const& arg1) {
  auto removed = statController.removeEphemeralEffect(UniqueStatusEffect{arg1});
  if (!removed) return std::unexpected(std::move(removed.error()));
  return {};
}

std::expected<void, StarException> LuaBindings::StatusControllerCallbacks::clearEphemeralEffects(StatusController& statController) {
  return statController.clearEphemeralEffects();
}

std::expected<LuaTupleReturn<List<Json>, std::uint64_t>, StarException> LuaBindings::StatusControllerCallbacks::damageTakenSince(
    StatusController& statController, std::optional<std::uint64_t> timestep) {
  auto pair = statController.damageTakenSince(timestep.value_or(std::uint64_t{}));
  List<Json> notifications;
  for (auto const& notification : pair.first) {
    auto json = notification.toJson();
    if (!json) return std::unexpected(std::move(json.error()));
    notifications.push_back(std::move(*json));
  }
  return luaTupleReturn(std::move(notifications), pair.second);
}

std::expected<LuaTupleReturn<List<Json>, std::uint64_t>, StarException> LuaBindings::StatusControllerCallbacks::inflictedHitsSince(
    StatusController& statController, std::optional<std::uint64_t> timestep) {
  auto pair = statController.inflictedHitsSince(timestep.value_or(std::uint64_t{}));
  List<Json> hits;
  for (auto const& hit : pair.first) {
    auto json = hit.second.toJson();
    if (!json) return std::unexpected(std::move(json.error()));
    auto tagged = json->set("targetEntityId", hit.first);
    if (!tagged) return std::unexpected(std::move(tagged.error()));
    hits.push_back(std::move(*tagged));
  }
  return luaTupleReturn(std::move(hits), pair.second);
}

std::expected<LuaTupleReturn<List<Json>, std::uint64_t>, StarException> LuaBindings::StatusControllerCallbacks::inflictedDamageSince(
    StatusController& statController, std::optional<std::uint64_t> timestep) {
  auto pair = statController.inflictedDamageSince(timestep.value_or(std::uint64_t{}));
  List<Json> notifications;
  for (auto const& notification : pair.first) {
    auto json = notification.toJson();
    if (!json) return std::unexpected(std::move(json.error()));
    notifications.push_back(std::move(*json));
  }
  return luaTupleReturn(std::move(notifications), pair.second);
}

List<JsonArray> LuaBindings::StatusControllerCallbacks::activeUniqueStatusEffectSummary(
    StatusController& statController) {
  auto summary = statController.activeUniqueStatusEffectSummary();
  return summary.transformed([](std::pair<UniqueStatusEffect, std::optional<float>> effect) {
    JsonArray effectJson = {effect.first};
    if (effect.second)
      effectJson.push_back(*effect.second);
    return effectJson;
  });
}

bool LuaBindings::StatusControllerCallbacks::uniqueStatusEffectActive(
    StatusController& statController, String const& effectName) {
  return statController.uniqueStatusEffectActive(effectName);
}

}

module;
import std;
import star.format;
import star.string;
import star.list;
import star.json;
import star.lua;
import star.lua_converters;
import star.input_event;
import star.bi_map;
import star.thread;
import star.listener;
import star.hash;


// Match client include order for SIMD intrinsics used by xxhash.
import star.input;

module star.input_lua_bindings;

import star.vector;
namespace Star {

std::expected<LuaCallbacks, StarException> LuaBindings::makeInputCallbacks(Input& owner) {
  LuaCallbacks callbacks;

  auto& input = owner;

  if (auto registration = callbacks.registerCallbackWithSignature<std::optional<unsigned>, String, String>(
        "bindDown",
        [&input](String const& categoryId, String const& bindId) { return input.bindDown(categoryId, bindId); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.registerCallbackWithSignature<bool, String, String>(
        "bindHeld",
        [&input](String const& categoryId, String const& bindId) { return input.bindHeld(categoryId, bindId); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.registerCallbackWithSignature<bool, String, String>(
        "bind",
        [&input](String const& categoryId, String const& bindId) { return input.bindHeld(categoryId, bindId); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.registerCallbackWithSignature<std::optional<unsigned>, String, String>(
        "bindUp",
        [&input](String const& categoryId, String const& bindId) { return input.bindUp(categoryId, bindId); });
      !registration)
    return std::unexpected(std::move(registration.error()));

  if (auto registration = callbacks.registerCallback("keyDown", [&input](String const& keyName, std::optional<StringList> const& modNames) -> std::expected<std::optional<unsigned>, StarException> {
    auto key = enumValue<KeyNames>(keyName);
    if (!key)
      return std::unexpected(std::move(key.error()));
    std::optional<KeyMod> mod;
    if (modNames) {
      mod = KeyMod::NoMod;
      for (auto& modName : *modNames) {
        auto modifier = enumValue<KeyModNames>(modName);
        if (!modifier)
          return std::unexpected(std::move(modifier.error()));
        *mod |= *modifier;
      }
    }
    return input.keyDown(*key, mod);
  }); !registration)
    return std::unexpected(std::move(registration.error()));
  auto keyHeld = [&input](String const& keyName) -> std::expected<bool, StarException> {
    auto key = enumValue<KeyNames>(keyName);
    if (!key)
      return std::unexpected(std::move(key.error()));
    return input.keyHeld(*key);
  };
  if (auto registration = callbacks.registerCallback("keyHeld", keyHeld); !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.registerCallback("key",     keyHeld); !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.registerCallback("keyUp", [&input](String const& keyName) -> std::expected<std::optional<unsigned>, StarException> {
    auto key = enumValue<KeyNames>(keyName);
    if (!key)
      return std::unexpected(std::move(key.error()));
    return input.keyUp(*key);
  }); !registration)
    return std::unexpected(std::move(registration.error()));

  if (auto registration = callbacks.registerCallback("mouseDown", [&input](String const& buttonName) -> std::expected<std::optional<List<Vec2F>>, StarException> {
    auto button = enumValue<MouseButtonNames>(buttonName);
    if (!button)
      return std::unexpected(std::move(button.error()));
    return input.mouseDown(*button);
  }); !registration)
    return std::unexpected(std::move(registration.error()));

  auto mouseHeld = [&input](String const& buttonName) -> std::expected<bool, StarException> {
    auto button = enumValue<MouseButtonNames>(buttonName);
    if (!button)
      return std::unexpected(std::move(button.error()));
    return input.mouseHeld(*button);
  };
  if (auto registration = callbacks.registerCallback("mouseHeld", mouseHeld); !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.registerCallback("mouse",     mouseHeld); !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.registerCallback("mouseUp", [&input](String const& buttonName) -> std::expected<std::optional<List<Vec2F>>, StarException> {
    auto button = enumValue<MouseButtonNames>(buttonName);
    if (!button)
      return std::unexpected(std::move(button.error()));
    return input.mouseUp(*button);
  }); !registration)
    return std::unexpected(std::move(registration.error()));

  if (auto registration = callbacks.registerCallbackWithSignature<void, String, String>(
        "resetBinds",
        [&input](String const& categoryId, String const& bindId) { return input.resetBinds(categoryId, bindId); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.registerCallbackWithSignature<void, String, String, Json>(
        "setBinds",
        [&input](String const& categoryId, String const& bindId, Json const& binds) {
          return input.setBinds(categoryId, bindId, binds);
        });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.registerCallbackWithSignature<Json, String, String>(
        "getDefaultBinds",
        [&input](String const& categoryId, String const& bindId) { return input.getDefaultBinds(categoryId, bindId); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.registerCallbackWithSignature<Json, String, String>(
        "getBinds",
        [&input](String const& categoryId, String const& bindId) { return input.getBinds(categoryId, bindId); });
      !registration)
    return std::unexpected(std::move(registration.error()));

  if (auto registration = callbacks.registerCallback("events", [&input]() -> std::expected<Json, StarException> {
    JsonArray result;
    for (auto& pair : input.inputEventsThisFrame()) {
      auto event = Input::inputEventToJson(pair.first);
      if (!event)
        return std::unexpected(std::move(event.error()));
      if (*event) {
        auto processed = event->set("processed", pair.second);
        if (!processed)
          return std::unexpected(std::move(processed.error()));
        result.emplace_back(std::move(*processed));
      }
    }
    return result;
  }); !registration)
    return std::unexpected(std::move(registration.error()));

  if (auto registration =
        callbacks.registerCallbackWithSignature<Vec2F>("mousePosition", [&input]() { return input.mousePosition(); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.registerCallbackWithSignature<unsigned, String>(
        "getTag", [&input](String const& name) { return input.getTag(name); });
      !registration)
    return std::unexpected(std::move(registration.error()));

  return callbacks;
}

}

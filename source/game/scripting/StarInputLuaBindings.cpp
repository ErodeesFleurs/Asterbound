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

// Copy before Lua allocation: a finalizer may rebind and invalidate the view.
static std::optional<List<Vec2F>> snapshot_positions(std::span<Vec2F const> positions) {
  if (positions.empty())
    return {};
  return List<Vec2F>(positions.begin(), positions.end());
}

std::expected<LuaCallbacks, StarException> LuaBindings::make_input_callbacks(Input& input) {
  LuaCallbacks callbacks;

  if (auto registration = callbacks.register_callback_with_signature<std::optional<unsigned>, String, String>(
        "bindDown",
        [&input](String const& category_id, String const& bind_id) { return input.bind_down(category_id, bind_id); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback_with_signature<bool, String, String>(
        "bindHeld",
        [&input](String const& category_id, String const& bind_id) { return input.bind_held(category_id, bind_id); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback_with_signature<bool, String, String>(
        "bind",
        [&input](String const& category_id, String const& bind_id) { return input.bind_held(category_id, bind_id); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback_with_signature<std::optional<unsigned>, String, String>(
        "bindUp",
        [&input](String const& category_id, String const& bind_id) { return input.bind_up(category_id, bind_id); });
      !registration)
    return std::unexpected(std::move(registration.error()));

  if (auto registration = callbacks.register_callback("keyDown", [&input](String const& key_name, std::optional<StringList> const& mod_names) -> std::expected<std::optional<unsigned>, StarException> {
    auto key = enum_value<key_names>(key_name);
    if (!key)
      return std::unexpected(std::move(key.error()));
    std::optional<KeyMod> mod;
    if (mod_names) {
      mod = KeyMod::NoMod;
      for (auto& mod_name : *mod_names) {
        auto modifier = enum_value<key_mod_names>(mod_name);
        if (!modifier)
          return std::unexpected(std::move(modifier.error()));
        *mod |= *modifier;
      }
    }
    return input.key_down(*key, mod);
  }); !registration)
    return std::unexpected(std::move(registration.error()));
  auto key_held = [&input](String const& key_name) -> std::expected<bool, StarException> {
    auto key = enum_value<key_names>(key_name);
    if (!key)
      return std::unexpected(std::move(key.error()));
    return input.key_held(*key);
  };
  if (auto registration = callbacks.register_callback("keyHeld", key_held); !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback("key",     key_held); !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback("keyUp", [&input](String const& key_name) -> std::expected<std::optional<unsigned>, StarException> {
    auto key = enum_value<key_names>(key_name);
    if (!key)
      return std::unexpected(std::move(key.error()));
    return input.key_up(*key);
  }); !registration)
    return std::unexpected(std::move(registration.error()));

  if (auto registration = callbacks.register_callback("mouseDown", [&input](String const& button_name) -> std::expected<std::optional<List<Vec2F>>, StarException> {
    auto button = enum_value<mouse_button_names>(button_name);
    if (!button)
      return std::unexpected(std::move(button.error()));
    return snapshot_positions(input.mouse_down(*button));
  }); !registration)
    return std::unexpected(std::move(registration.error()));

  auto mouse_held = [&input](String const& button_name) -> std::expected<bool, StarException> {
    auto button = enum_value<mouse_button_names>(button_name);
    if (!button)
      return std::unexpected(std::move(button.error()));
    return input.mouse_held(*button);
  };
  if (auto registration = callbacks.register_callback("mouseHeld", mouse_held); !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback("mouse",     mouse_held); !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback("mouseUp", [&input](String const& button_name) -> std::expected<std::optional<List<Vec2F>>, StarException> {
    auto button = enum_value<mouse_button_names>(button_name);
    if (!button)
      return std::unexpected(std::move(button.error()));
    return snapshot_positions(input.mouse_up(*button));
  }); !registration)
    return std::unexpected(std::move(registration.error()));

  if (auto registration = callbacks.register_callback_with_signature<void, String, String>(
        "resetBinds",
        [&input](String const& category_id, String const& bind_id) { return input.reset_binds(category_id, bind_id); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback_with_signature<void, String, String, Json>(
        "setBinds",
        [&input](String const& category_id, String const& bind_id, Json const& binds) {
          return input.set_binds(category_id, bind_id, binds);
        });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback_with_signature<Json, String, String>(
        "getDefaultBinds",
        [&input](String const& category_id, String const& bind_id) { return input.get_default_binds(category_id, bind_id); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback_with_signature<Json, String, String>(
        "getBinds",
        [&input](String const& category_id, String const& bind_id) { return input.get_binds(category_id, bind_id); });
      !registration)
    return std::unexpected(std::move(registration.error()));

  if (auto registration = callbacks.register_callback("events", [&input]() -> std::expected<Json, StarException> {
    JsonArray result;
    auto const& events = input.input_events_this_frame();
    result.reserve(events.size());
    for (auto const& [input_event, processed] : events) {
      auto event = Input::input_event_to_json(input_event, processed);
      if (!event)
        return std::unexpected(std::move(event.error()));
      if (*event)
        result.emplace_back(std::move(*event));
    }
    return result;
  }); !registration)
    return std::unexpected(std::move(registration.error()));

  if (auto registration =
        callbacks.register_callback_with_signature<Vec2F>("mousePosition", [&input]() { return input.mouse_position(); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback_with_signature<unsigned, String>(
        "getTag", [&input](String const& name) { return input.get_tag(name); });
      !registration)
    return std::unexpected(std::move(registration.error()));

  return callbacks;
}

}

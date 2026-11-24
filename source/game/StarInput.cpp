import star.hash;
import star.json;
import star.json_extra;
import star.input_event;
import star.thread;
import star.listener;
import star.ordered_map;
import star.bi_map;
import star.io_device;
import star.list;
import star.image_processing;
import star.data_stream;
import star.string_view;
import star.directives;
import star.asset_path;
import star.ref_ptr;
import star.logging;
import std;
import star.string;
import star.format;

import star.input;
import star.asset_source;
import star.assets;
import star.root_base;
import star.configuration;
import star.root;

import star.vector;
import star.algorithm;
namespace Star {

static String const input_binding_config_root = "modBindings";

namespace {
KeyMod const lock_mods = KeyMod::Num | KeyMod::Caps | KeyMod::Scroll;

KeyMod physical_modifier(Key key) {
  switch (key) {
    case Key::LShift: return KeyMod::LShift;
    case Key::RShift: return KeyMod::RShift;
    case Key::LCtrl: return KeyMod::LCtrl;
    case Key::RCtrl: return KeyMod::RCtrl;
    case Key::LAlt: return KeyMod::LAlt;
    case Key::RAlt: return KeyMod::RAlt;
    case Key::LGui: return KeyMod::LGui;
    case Key::RGui: return KeyMod::RGui;
    case Key::AltGr: return KeyMod::AltGr;
    default: return KeyMod::NoMod;
  }
}
}

// Keep mod binding wire names separate from the game's LMeta/RMeta spellings.
constexpr auto mod_bind_modifier_names = sorted_enum_names(std::to_array<std::pair<KeyMod, std::string_view>>({
  {KeyMod::LShift, "LShift"}, {KeyMod::RShift, "RShift"}, {KeyMod::LCtrl, "LCtrl"}, {KeyMod::RCtrl, "RCtrl"},
  {KeyMod::LAlt, "LAlt"}, {KeyMod::RAlt, "RAlt"}, {KeyMod::LGui, "LGui"}, {KeyMod::RGui, "RGui"},
  {KeyMod::Num, "Num"}, {KeyMod::Caps, "Caps"}, {KeyMod::AltGr, "AltGr"}, {KeyMod::Scroll, "Scroll"}
}));

Json key_mods_to_json(KeyMod mod) {
  JsonArray array;

  for (auto const& [bit, name] : mod_bind_modifier_names)
    if (static_cast<bool>(mod & bit))
      array.emplace_back(name);

  return array.empty() ? Json() : std::move(array);
}

[[nodiscard]] static std::expected<KeyMod, StarException> key_mods_from_json(Json const& json) {
  KeyMod mod = KeyMod::NoMod;
  if (json.is_null())
    return mod;
  auto array = json.iterate_array();
  if (!array) return std::unexpected(std::move(array.error()));
  for (Json const& j_mod : *array) {
    auto text = j_mod.to_string();
    if (!text) return std::unexpected(std::move(text.error()));
    auto named = maybe_enum_value<mod_bind_modifier_names>(*text);
    if (!named) return std::unexpected(std::move(named.error()));
    KeyMod bit;
    if (*named) {
      bit = **named;
    } else {
      auto legacy = enum_value<key_mod_names>(*text);
      if (!legacy) return std::unexpected(std::move(legacy.error()));
      bit = *legacy;
    }
    mod |= bit;
  }
  return mod;
}

std::size_t ValueHash<InputVariant>::operator()(InputVariant const& source) const {
  // Tag disjoint device namespaces; preserve the entire controller instance ID.
  std::uint64_t value;
  if (std::holds_alternative<Key>(source))
    value = std::to_underlying(std::get<Key>(source));
  else if (std::holds_alternative<MouseButton>(source))
    value = std::to_underlying(std::get<MouseButton>(source));
  else {
    auto pad = std::get<ControllerSource>(source);
    value = (std::uint64_t{pad.controller} << 8) | std::to_underlying(pad.button);
  }
  return hash_of((value << 2) | source.index());
}

std::expected<Json, StarException> Input::input_event_to_json(InputEvent const& input, std::optional<bool> processed) {
  String type;
  Json data;

  if (std::holds_alternative<KeyDownEvent>(input)) {
    auto const& key_down = std::get<KeyDownEvent>(input);
    type = "KeyDown";
    auto name = enum_name<key_names>(key_down.key);
    if (!name) return std::unexpected(std::move(name.error()));
    data = JsonObject{
      {"key", String(*name)},
      {"mods", key_mods_to_json(key_down.mods)}
    };
  } else if (std::holds_alternative<KeyUpEvent>(input)) {
    auto const& key_up = std::get<KeyUpEvent>(input);
    type = "KeyUp";
    auto name = enum_name<key_names>(key_up.key);
    if (!name) return std::unexpected(std::move(name.error()));
    data = JsonObject{{"key", String(*name)}};
  } else if (std::holds_alternative<MouseButtonDownEvent>(input)) {
    auto const& mouse_down = std::get<MouseButtonDownEvent>(input);
    type = "MouseButtonDown";
    auto name = enum_name<mouse_button_names>(mouse_down.mouse_button);
    if (!name) return std::unexpected(std::move(name.error()));
    data = JsonObject{
      {"mouseButton", String(*name)},
      {"mousePosition", json_from_vec2f(mouse_down.mouse_position)}
    };
  } else if (std::holds_alternative<MouseButtonUpEvent>(input)) {
    auto const& mouse_up = std::get<MouseButtonUpEvent>(input);
    type = "MouseButtonUp";
    auto name = enum_name<mouse_button_names>(mouse_up.mouse_button);
    if (!name) return std::unexpected(std::move(name.error()));
    data = JsonObject{
      {"mouseButton", String(*name)},
      {"mousePosition", json_from_vec2f(mouse_up.mouse_position)}
    };
  } else if (std::holds_alternative<MouseWheelEvent>(input)) {
    auto const& mouse_wheel = std::get<MouseWheelEvent>(input);
    type = "MouseWheel";
    data = JsonObject{
      {"mouseWheel", mouse_wheel.mouse_wheel == MouseWheel::Up ? 1 : -1},
      {"mousePosition", json_from_vec2f(mouse_wheel.mouse_position)}
    };
  } else if (std::holds_alternative<MouseMoveEvent>(input)) {
    auto const& mouse_move = std::get<MouseMoveEvent>(input);
    type = "MouseMove";
    data = JsonObject{
      {"mouseMove", json_from_vec2f(mouse_move.mouse_move)},
      {"mousePosition", json_from_vec2f(mouse_move.mouse_position)}
    };
  } else if (std::holds_alternative<ControllerButtonDownEvent>(input)) {
    auto const& controller_down = std::get<ControllerButtonDownEvent>(input);
    type = "ControllerButtonDown";
    auto name = enum_name<controller_button_names>(controller_down.controller_button);
    if (!name) return std::unexpected(std::move(name.error()));
    data = JsonObject{
      {"controllerButton", String(*name)},
      {"controller", controller_down.controller}};
  } else if (std::holds_alternative<ControllerButtonUpEvent>(input)) {
    auto const& controller_up = std::get<ControllerButtonUpEvent>(input);
    type = "ControllerButtonUp";
    auto name = enum_name<controller_button_names>(controller_up.controller_button);
    if (!name) return std::unexpected(std::move(name.error()));
    data = JsonObject{
      {"controllerButton", String(*name)},
      {"controller", controller_up.controller}};
  } else if (std::holds_alternative<ControllerAxisEvent>(input)) {
    auto const& controller_axis = std::get<ControllerAxisEvent>(input);
    type = "ControllerAxis";
    auto name = enum_name<controller_axis_names>(controller_axis.controller_axis);
    if (!name) return std::unexpected(std::move(name.error()));
    data = JsonObject{
      {"controllerAxis", String(*name)},
      {"controllerAxisValue", controller_axis.controller_axis_value},
      {"controller", controller_axis.controller}};
  }

  if (!data)
    return data;
  JsonObject event{{"type", std::move(type)}, {"data", std::move(data)}};
  if (processed)
    event.emplace("processed", *processed);
  return event;
}

std::expected<Input::Bind, StarException> Input::bind_from_json(Json const& json) {
  Bind bind;
  if (json.is_null())
    return bind;

  auto type_result = json.get_string("type");
  if (!type_result) return std::unexpected(std::move(type_result.error()));
  String const& type = *type_result;
  if (type != "key" && type != "mouse" && type != "controller")
    return bind;
  auto value_result = json.get("value", {});
  if (!value_result) return std::unexpected(std::move(value_result.error()));
  Json const& value = *value_result;
  auto text = value.to_string();
  if (!text)
    return std::unexpected(std::move(text.error()));

  if (type == "key") {
    KeyBind key_bind;
    auto key = maybe_enum_value<key_names>(*text);
    if (!key) return std::unexpected(std::move(key.error()));
    if (!*key) return bind;
    key_bind.key = **key;
    auto mods = json.get("mods", Json());
    if (!mods) return std::unexpected(std::move(mods.error()));
    auto parsed = key_mods_from_json(*mods);
    if (!parsed) return std::unexpected(std::move(parsed.error()));
    key_bind.mods = *parsed;
    key_bind.priority = static_cast<std::uint8_t>(std::popcount(std::to_underlying(*parsed)));
    bind = std::move(key_bind);
  }
  else if (type == "mouse") {
    MouseBind mouse_bind;
    auto button = maybe_enum_value<mouse_button_names>(*text);
    if (!button) return std::unexpected(std::move(button.error()));
    if (!*button) return bind;
    mouse_bind.button = **button;
    auto mods = json.get("mods", Json());
    if (!mods) return std::unexpected(std::move(mods.error()));
    auto parsed = key_mods_from_json(*mods);
    if (!parsed) return std::unexpected(std::move(parsed.error()));
    mouse_bind.mods = *parsed;
    mouse_bind.priority = static_cast<std::uint8_t>(std::popcount(std::to_underlying(*parsed)));
    bind = std::move(mouse_bind);
  }
  else if (type == "controller") {
    ControllerBind controller_bind;
    auto button = maybe_enum_value<controller_button_names>(*text);
    if (!button) return std::unexpected(std::move(button.error()));
    if (!*button) return bind;
    controller_bind.button = **button;
    auto controller = json.get_uint("controller", 0);
    if (!controller) return std::unexpected(std::move(controller.error()));
    if (!std::in_range<ControllerId>(*controller))
      return std::unexpected(InputException("Controller identifier is out of range"));
    controller_bind.controller = static_cast<ControllerId>(*controller);
    bind = std::move(controller_bind);
  }

  return bind;
}

std::expected<Json, StarException> Input::bind_to_json(Bind const& bind) {
  if (std::holds_alternative<KeyBind>(bind)) {
    auto const& key_bind = std::get<KeyBind>(bind);
    auto name = enum_name<key_names>(key_bind.key);
    if (!name) return std::unexpected(std::move(name.error()));
    auto obj = JsonObject{
      {"type", "key"},
      {"value", String(*name)}
    }; // don't want empty mods to exist as null entry
    if (auto mods = key_mods_to_json(key_bind.mods))
      obj.emplace("mods", std::move(mods));
    return obj;
  }
  else if (std::holds_alternative<MouseBind>(bind)) {
    auto const& mouse_bind = std::get<MouseBind>(bind);
    auto name = enum_name<mouse_button_names>(mouse_bind.button);
    if (!name) return std::unexpected(std::move(name.error()));
    auto obj = JsonObject{
      {"type", "mouse"},
      {"value", String(*name)}
    };
    if (auto mods = key_mods_to_json(mouse_bind.mods))
      obj.emplace("mods", std::move(mods));
    return obj;
  }
  else if (std::holds_alternative<ControllerBind>(bind)) {
    auto const& controller_bind = std::get<ControllerBind>(bind);
    auto name = enum_name<controller_button_names>(controller_bind.button);
    if (!name) return std::unexpected(std::move(name.error()));
    return JsonObject{
      {"type", "controller"},
      {"value", String(*name)},
      {"controller", controller_bind.controller}
    };
  }

  return Json();
}

namespace {

// The editor owns display names. Keep rejecting ill-typed names without storing
// a second copy of either the display name or the map's ID on every entry.
std::expected<void, StarException> validate_bind_name(Json const& config) {
  auto field = config.find("name");
  if (!field) return std::unexpected(std::move(field.error()));
  if (*field && !(*field)->get().is_null() && !(*field)->get().is_type(Json::Type::String))
    return std::unexpected(JsonException("Bind name must be a string"));
  return {};
}

std::expected<void, StarException> load_bind_list(Root& root, JsonArray const& descriptors,
    String const& category_id, String const& bind_id, std::string_view origin, List<Input::Bind>& binds) {
  binds.reserve(descriptors.size());
  for (auto const& descriptor : descriptors) {
    auto bind = Input::bind_from_json(descriptor);
    if (!bind) {
      if (!bind.error().is<JsonException>()) return std::unexpected(std::move(bind.error()));
      auto logged = root.logger().error_exception(
          Star::format("Binds: Error loading {} bind in {}.{}", origin, category_id, bind_id),
          ExceptionDiagnostic(bind.error()));
      if (!logged) return std::unexpected(std::move(logged.error()));
    } else {
      binds.emplace_back(std::move(*bind));
    }
  }
  return {};
}

std::expected<void, StarException> load_category(Root& root, String const& category_id, Json const& config,
    Json const& overrides, Input::BindCategory& entries, std::size_t& next_id) {
  if (auto name = validate_bind_name(config); !name) return name;
  auto configured = config.get("binds", {});
  if (!configured) return std::unexpected(std::move(configured.error()));
  if (configured->is_null()) return {};
  auto members = configured->object_ptr();
  if (!members) return std::unexpected(std::move(members.error()));
  entries.reserve((*members)->size());
  for (auto const& [bind_id, definition] : **members) {
    if (!definition.is_type(Json::Type::Object)) continue;
    if (auto name = validate_bind_name(definition); !name) return name;
    auto& entry = entries[bind_id];
    entry.state_id = next_id++;
    auto tags = definition.find("tags");
    if (!tags) return std::unexpected(std::move(tags.error()));
    if (*tags) {
      auto list = json_to_string_list((*tags)->get());
      if (!list) return std::unexpected(std::move(list.error()));
      entry.tags = std::move(*list);
    }
    auto defaults = definition.get("default", {});
    if (!defaults) return std::unexpected(std::move(defaults.error()));
    if (!defaults->is_null()) {
      auto descriptors = defaults->array_ptr();
      if (!descriptors) return std::unexpected(std::move(descriptors.error()));
      if (auto result = load_bind_list(root, **descriptors, category_id, bind_id, "default", entry.default_binds); !result)
        return result;
    }
    if (!overrides.is_type(Json::Type::Object)) continue;
    auto saved = overrides.find(bind_id);
    if (!saved) return std::unexpected(std::move(saved.error()));
    if (!*saved || !(*saved)->get().is_type(Json::Type::Array)) continue;
    auto descriptors = (*saved)->get().array_ptr();
    if (!descriptors) return std::unexpected(std::move(descriptors.error()));
    auto& custom = entry.custom_binds.emplace();
    if (auto result = load_bind_list(root, **descriptors, category_id, bind_id, "user", custom); !result)
      return result;
    // Malformed descriptors are skipped; only a literally empty override unbinds.
    if (!(*descriptors)->empty() && custom.empty())
      entry.custom_binds.reset();
  }
  return {};
}

std::expected<Json, StarException> serialize_binds(List<Input::Bind> const& binds) {
  JsonArray array;
  array.reserve(binds.size());
  for (auto const& bind : binds) {
    auto descriptor = Input::bind_to_json(bind);
    if (!descriptor) return std::unexpected(std::move(descriptor.error()));
    array.emplace_back(std::move(*descriptor));
  }
  return Json(std::move(array));
}

}

void Input::press_source(InputVariant source, KeyMod mods) {
  auto found = m_bind_mappings.find(source);
  if (found == m_bind_mappings.end())
    return;
  auto& mapping = found->second;
  if (mapping.held) {
    for (auto id : mapping.held_entries)
      ++m_bind_states.find(id)->second.presses;
    return;
  }

  mapping.held = true;
  std::uint8_t priority = 0;
  // Mod bindings allow extra modifiers and ignore lock state for matching.
  auto available = mods | lock_mods;
  for (auto const& candidate : mapping.candidates) {
    if (candidate.priority < priority)
      break;
    if ((available & candidate.mods) != candidate.mods)
      continue;
    priority = candidate.priority;
    auto const& entry = candidate.entry.get();
    // Candidates are sorted by priority, then by entry ID.
    if (!mapping.held_entries.empty() && mapping.held_entries[mapping.held_entries.size() - 1] == entry.state_id)
      continue;
    mapping.held_entries.emplace_back(entry.state_id);
    auto [it, inserted] = m_bind_states.try_emplace(entry.state_id, entry);
    ++it->second.presses;
    ++it->second.sources;
    if (inserted)
      for (auto const& tag : entry.tags)
        ++m_active_tags[tag];
  }
}

void Input::release_source(InputVariant source) {
  auto found = m_bind_mappings.find(source);
  if (found == m_bind_mappings.end())
    return;
  auto& mapping = found->second;
  for (auto id : mapping.held_entries) {
    auto& state = m_bind_states.find(id)->second;
    ++state.releases;
    --state.sources;
  }
  mapping.held_entries.clear();
  mapping.held = false;
}

std::optional<std::reference_wrapper<Input::BindEntry>> Input::maybe_bind_entry(String const& category_id, String const& bind_id) {
  if (auto category = m_bind_categories.find_ref(category_id))
    return category->get().find_ref(bind_id);
  return std::nullopt;
}

std::expected<std::reference_wrapper<Input::BindEntry>, StarException> Input::bind_entry(String const& category_id, String const& bind_id) {
  if (auto entry = maybe_bind_entry(category_id, bind_id))
    return *entry;
  return std::unexpected(InputException::format("Could not find bind entry {}.{}", category_id, bind_id));
}

std::optional<std::reference_wrapper<Input::BindInputState>> Input::maybe_bind_state(String const& category_id, String const& bind_id) {
  if (auto entry = maybe_bind_entry(category_id, bind_id))
    return m_bind_states.find_ref(entry->get().state_id);
  return std::nullopt;
}

Input::Input(ConstructionKey, Root& root) : m_root(root) {}

std::expected<std::shared_ptr<Input>, StarException> Input::create(Root& root) {
  auto result = std::make_shared<Input>(ConstructionKey{}, root);
  if (auto r = result->reload(); !r) return std::unexpected(std::move(r.error()));
  result->m_root_reload_listener = std::make_shared<CallbackListener>([input = std::ref(*result)]() {
    return input.get().reload();
  });
  result->m_root_reload_subscription = root.subscribe_reload(result->m_root_reload_listener);
  return result;
}

Input::~Input() {
  m_root_reload_subscription.disconnect();
}

List<std::pair<InputEvent, bool>> const& Input::input_events_this_frame() const {
  return m_input_events;
}

void Input::finish_tick() {
  m_input_events.clear();
  Star::erase_if(m_key_states, [](decltype(m_key_states)::iterator::reference item) {
    auto& state = item.second;
    if (!state.held) return true;
    state.presses.clear();
    state.releases = 0;
    return false;
  });
  Star::erase_if(m_mouse_states, [](decltype(m_mouse_states)::iterator::reference item) {
    auto& state = item.second;
    if (!state.held) return true;
    state.press_positions.clear();
    state.release_positions.clear();
    return false;
  });
  Star::erase_if(m_bind_states, [this](decltype(m_bind_states)::iterator::reference item) {
    auto& state = item.second;
    if (state.sources) {
      state.presses = state.releases = 0;
      return false;
    }
    for (auto const& tag : state.entry.get().tags) {
      auto it = m_active_tags.find(tag);
      if (!--it->second)
        m_active_tags.erase(it);
    }
    return true;
  });
}

void Input::clear_binding_state() {
  m_key_states.clear();
  m_mouse_states.clear();
  m_bind_states.clear();
  m_active_tags.clear();
  for (auto&& [source, mapping] : m_bind_mappings) {
    mapping.held_entries.clear();
    mapping.held = false;
  }
}

void Input::clear_input_state() {
  clear_binding_state();
  m_input_events.clear();
  m_pressed_mods = KeyMod::NoMod;
}

void Input::handle_input(InputEvent const& input, bool game_processed) {
  m_input_events.emplace_back(input, game_processed);
  if (std::holds_alternative<KeyDownEvent>(input)) {
    auto const& event = std::get<KeyDownEvent>(input);
    m_pressed_mods = event.mods | physical_modifier(event.key);
    if (!game_processed && !m_text_input_active) {
      auto& state = m_key_states[event.key];
      auto& presses = state.presses;
      if (!presses.empty() && presses[presses.size() - 1].mods == event.mods)
        ++presses[presses.size() - 1].count;
      else
        presses.emplace_back(ModifierPresses{event.mods, 1});
      state.held = true;
      press_source(event.key, event.mods);
    }
  } else if (std::holds_alternative<KeyUpEvent>(input)) {
    auto key = std::get<KeyUpEvent>(input).key;
    m_pressed_mods &= ~physical_modifier(key);
    if (auto state = m_key_states.find_ref(key); state && std::exchange(state->get().held, false)) {
      ++state->get().releases;
      release_source(key);
    }
  } else if (std::holds_alternative<MouseButtonDownEvent>(input)) {
    auto const& event = std::get<MouseButtonDownEvent>(input);
    m_mouse_position = event.mouse_position;
    if (!game_processed) {
      auto& state = m_mouse_states[event.mouse_button];
      state.press_positions.emplace_back(event.mouse_position);
      state.held = true;
      press_source(event.mouse_button, m_pressed_mods);
    }
  } else if (std::holds_alternative<MouseButtonUpEvent>(input)) {
    auto const& event = std::get<MouseButtonUpEvent>(input);
    m_mouse_position = event.mouse_position;
    if (auto state = m_mouse_states.find_ref(event.mouse_button); state && std::exchange(state->get().held, false)) {
      state->get().release_positions.emplace_back(event.mouse_position);
      release_source(event.mouse_button);
    }
  } else if (std::holds_alternative<MouseMoveEvent>(input)) {
    m_mouse_position = std::get<MouseMoveEvent>(input).mouse_position;
  } else if (std::holds_alternative<ControllerButtonDownEvent>(input)) {
    if (!game_processed) {
      auto const& event = std::get<ControllerButtonDownEvent>(input);
      press_source(ControllerSource{event.controller, event.controller_button}, m_pressed_mods);
    }
  } else if (std::holds_alternative<ControllerButtonUpEvent>(input)) {
    auto const& event = std::get<ControllerButtonUpEvent>(input);
    release_source(ControllerSource{event.controller, event.controller_button});
  }
}

Input::BindMappings Input::rebuild_mappings(BindCategories const& categories, std::optional<Replacement> replacement) {
  BindMappings mappings;
  for (auto const& category : categories.value_view()) {
    for (auto const& entry : category.value_view()) {
      auto const& binds = replacement && replacement->state_id == entry.state_id ? replacement->binds : entry.binds();
      for (auto const& bind : binds) {
        if (std::holds_alternative<KeyBind>(bind)) {
          auto const& key = std::get<KeyBind>(bind);
          mappings[key.key].candidates.emplace_back(key.mods, key.priority, std::cref(entry));
        } else if (std::holds_alternative<MouseBind>(bind)) {
          auto const& mouse = std::get<MouseBind>(bind);
          mappings[mouse.button].candidates.emplace_back(mouse.mods, mouse.priority, std::cref(entry));
        } else if (std::holds_alternative<ControllerBind>(bind)) {
          auto const& controller = std::get<ControllerBind>(bind);
          mappings[controller].candidates.emplace_back(
              KeyMod::NoMod, std::uint8_t{0}, std::cref(entry));
        }
      }
    }
  }
  for (auto& mapping : mappings.value_view()) {
    mapping.candidates.sort([](BindRef const& a, BindRef const& b) {
      if (a.priority != b.priority)
        return a.priority > b.priority;
      return a.entry.get().state_id < b.entry.get().state_id;
    });
  }
  return mappings;
}

std::expected<void, StarException> Input::reload() {
  auto assets = m_root.assets();
  if (!assets) return std::unexpected(std::move(assets.error()));
  auto paths = (*assets)->scan_extension("binds");
  if (!paths) return std::unexpected(std::move(paths.error()));
  StringMap<Json> definitions;
  for (auto const& path : paths->get()) {
    auto json = (*assets)->json(path.string());
    if (!json) return std::unexpected(std::move(json.error()));
    auto categories = json->iterate_object();
    if (!categories) return std::unexpected(std::move(categories.error()));
    for (auto const& [category_id, config] : *categories) {
      if (!config.is_type(Json::Type::Object)) continue;
      auto [it, inserted] = definitions.try_emplace(category_id, config);
      if (!inserted) it->second = json_merge(it->second, config);
    }
  }
  auto config = m_root.configuration();
  if (!config) return std::unexpected(std::move(config.error()));
  auto user_binds = (*config)->get(input_binding_config_root);
  if (!user_binds) return std::unexpected(std::move(user_binds.error()));
  BindCategories categories;
  categories.reserve(definitions.size());
  std::size_t count = 0;
  for (auto const& [category_id, definition] : definitions) {
    Json overrides;
    if (user_binds->is_type(Json::Type::Object)) {
      auto saved = user_binds->get(category_id, {});
      if (!saved) return std::unexpected(std::move(saved.error()));
      overrides = std::move(*saved);
    }
    if (auto r = load_category(m_root, category_id, definition, overrides, categories[category_id], count); !r)
      return r;
  }
  auto mappings = rebuild_mappings(categories, std::nullopt);
  if (auto r = m_root.logger().info("Binds: Loaded {} bind{}", count, count == 1 ? "" : "s"); !r)
    return std::unexpected(std::move(r.error()));
  clear_binding_state();
  m_bind_categories.swap(categories);
  m_bind_mappings.swap(mappings);
  return {};
}

void Input::set_text_input_active(bool active) {
  m_text_input_active = active;
}

std::optional<unsigned> Input::bind_down(String const& category_id, String const& bind_id) {
  if (auto state = maybe_bind_state(category_id, bind_id); state && state->get().presses)
    return state->get().presses;
  return {};
}

bool Input::bind_held(String const& category_id, String const& bind_id) {
  auto state = maybe_bind_state(category_id, bind_id);
  return state && state->get().sources;
}

std::optional<unsigned> Input::bind_up(String const& category_id, String const& bind_id) {
  if (auto state = maybe_bind_state(category_id, bind_id); state && state->get().releases)
    return state->get().releases;
  return {};
}

std::optional<unsigned> Input::key_down(Key key, std::optional<KeyMod> key_mod) const {
  if (auto state = m_key_states.find_ref(key)) {
    unsigned matches = 0;
    auto requested = key_mod.value_or(KeyMod::NoMod);
    auto mask = ~lock_mods | requested;
    for (auto const& press : state->get().presses)
      if (!key_mod || (press.mods & mask) == requested)
        matches += press.count;
    if (matches) return matches;
  }
  return {};
}

bool Input::key_held(Key key) const {
  auto state = m_key_states.find_ref(key);
  return state && state->get().held;
}

std::optional<unsigned> Input::key_up(Key key) const {
  if (auto state = m_key_states.find_ref(key); state && state->get().releases)
    return state->get().releases;
  return {};
}

std::span<Vec2F const> Input::mouse_down(MouseButton button) const {
  if (auto state = m_mouse_states.find_ref(button))
    return state->get().press_positions;
  return {};
}

bool Input::mouse_held(MouseButton button) const {
  auto state = m_mouse_states.find_ref(button);
  return state && state->get().held;
}

std::span<Vec2F const> Input::mouse_up(MouseButton button) const {
  if (auto state = m_mouse_states.find_ref(button))
    return state->get().release_positions;
  return {};
}

Vec2F Input::mouse_position() const {
  return m_mouse_position;
}

std::expected<void, StarException> Input::write_binds(String const& category_id, String const& bind_id,
    BindEntry& entry, std::optional<List<Bind>> replacement) {
  auto const& binds = replacement ? *replacement : entry.default_binds;
  auto serialized = serialize_binds(binds);
  if (!serialized) return std::unexpected(std::move(serialized.error()));
  auto config = m_root.configuration();
  if (!config) return std::unexpected(std::move(config.error()));
  auto mappings = rebuild_mappings(m_bind_categories, Replacement{entry.state_id, binds});
  std::array keys{std::cref(input_binding_config_root), std::cref(category_id), std::cref(bind_id)};
  if (auto result = (*config)->set_object_path(keys, std::move(*serialized)); !result)
    return result;
  clear_binding_state();
  entry.custom_binds = std::move(replacement);
  m_bind_mappings.swap(mappings);
  return {};
}

std::expected<void, StarException> Input::reset_binds(String const& category_id, String const& bind_id) {
  auto entry = bind_entry(category_id, bind_id);
  if (!entry) return std::unexpected(std::move(entry.error()));
  return write_binds(category_id, bind_id, entry->get(), {});
}

std::expected<Json, StarException> Input::get_default_binds(String const& category_id, String const& bind_id) {
  auto entry = bind_entry(category_id, bind_id);
  if (!entry) return std::unexpected(std::move(entry.error()));
  return serialize_binds(entry->get().default_binds);
}

std::expected<Json, StarException> Input::get_binds(String const& category_id, String const& bind_id) {
  auto entry = bind_entry(category_id, bind_id);
  if (!entry) return std::unexpected(std::move(entry.error()));
  return serialize_binds(entry->get().binds());
}

std::expected<void, StarException> Input::set_binds(String const& category_id, String const& bind_id, Json const& binds) {
  auto entry = bind_entry(category_id, bind_id);
  if (!entry) return std::unexpected(std::move(entry.error()));
  auto array = binds.array_ptr();
  if (!array) return std::unexpected(std::move(array.error()));
  List<Bind> replacement;
  replacement.reserve((*array)->size());
  for (auto const& descriptor : **array) {
    auto bind = bind_from_json(descriptor);
    if (!bind) return std::unexpected(std::move(bind.error()));
    replacement.emplace_back(std::move(*bind));
  }
  return write_binds(category_id, bind_id, entry->get(), std::move(replacement));
}

unsigned Input::get_tag(String const& tag_name) const {
  auto tag = m_active_tags.find_ref(tag_name);
  return tag ? tag->get() : 0;
}

Input::ClipboardUnlock::ClipboardUnlock(Input& input) : m_input(input) {
  ++input.m_clipboard_allowed;
}

Input::ClipboardUnlock::ClipboardUnlock(ClipboardUnlock&& other) noexcept
    : m_input(std::exchange(other.m_input, std::nullopt)) {}

Input::ClipboardUnlock::~ClipboardUnlock() {
  if (m_input)
    --m_input->get().m_clipboard_allowed;
}

Input::ClipboardUnlock Input::unlock_clipboard() {
  return Input::ClipboardUnlock(*this);
}

bool Input::clipboard_allowed() const {
  return m_clipboard_allowed != 0 || get_tag("clipboard") > 0;
}

}
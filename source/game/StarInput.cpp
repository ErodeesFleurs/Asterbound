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
import star.version;

import star.input;
import star.asset_source;
import star.assets;
import star.root_base;
import star.configuration;
import star.root;

import star.vector;
import star.algorithm;
namespace Star {

constexpr std::string_view InputBindingConfigRoot = "modBindings";

std::expected<BiMap<Key, KeyMod>, StarException> const KeysToMods = BiMap<Key, KeyMod>::create({
  {Key::LShift, KeyMod::LShift},
  {Key::RShift, KeyMod::RShift},
  {Key::LCtrl, KeyMod::LCtrl},
  {Key::RCtrl, KeyMod::RCtrl},
  {Key::LAlt, KeyMod::LAlt},
  {Key::RAlt, KeyMod::RAlt},
  {Key::LGui, KeyMod::LGui},
  {Key::RGui, KeyMod::RGui},
  {Key::AltGr, KeyMod::AltGr},
  {Key::ScrollLock, KeyMod::Scroll}
});

const KeyMod KeyModOptional = KeyMod::Num | KeyMod::Caps | KeyMod::Scroll;

inline bool compareKeyModLenient(KeyMod input, KeyMod test) {
	input |= KeyModOptional;
	test |= KeyModOptional;
	return (test & input) == test;
}

inline bool compareKeyMod(KeyMod input, KeyMod test) {
	return (input | (KeyModOptional & ~test)) == (test | KeyModOptional);
}

Json keyModsToJson(KeyMod mod) {
  JsonArray array;

  if (static_cast<bool>(mod & KeyMod::LShift))
    array.emplace_back("LShift");
  if (static_cast<bool>(mod & KeyMod::RShift))
    array.emplace_back("RShift");
  if (static_cast<bool>(mod & KeyMod::LCtrl))
    array.emplace_back("LCtrl");
  if (static_cast<bool>(mod & KeyMod::RCtrl))
    array.emplace_back("RCtrl");
  if (static_cast<bool>(mod & KeyMod::LAlt))
    array.emplace_back("LAlt");
  if (static_cast<bool>(mod & KeyMod::RAlt))
    array.emplace_back("RAlt");
  if (static_cast<bool>(mod & KeyMod::LGui))
    array.emplace_back("LGui");
  if (static_cast<bool>(mod & KeyMod::RGui))
    array.emplace_back("RGui");
  if (static_cast<bool>(mod & KeyMod::Num))
    array.emplace_back("Num");
  if (static_cast<bool>(mod & KeyMod::Caps))
    array.emplace_back("Caps");
  if (static_cast<bool>(mod & KeyMod::AltGr))
    array.emplace_back("AltGr");
  if (static_cast<bool>(mod & KeyMod::Scroll))
    array.emplace_back("Scroll");

  return array.empty() ? Json() : std::move(array);
}

// Optional borrowed output for the calculated priority.
[[nodiscard]] std::expected<KeyMod, StarException> keyModsFromJson(Json const& json, std::optional<std::reference_wrapper<std::uint8_t>> priority = std::nullopt) {
  KeyMod mod = KeyMod::NoMod;
  if (json.isNull()) {
    if (priority)
      priority->get() = 0;
    return mod;
  }

  std::uint8_t modPriority = 0;
  auto array = json.iterateArray();
  if (!array) return std::unexpected(std::move(array.error()));
  for (Json const& jMod : *array) {
    auto text = jMod.toString();
    if (!text) return std::unexpected(std::move(text.error()));
    auto named = enumValue<KeyModNames>(*text);
    if (!named) return std::unexpected(std::move(named.error()));
    KeyMod changedMod = mod | *named;
    if (mod != changedMod) {
      mod = changedMod;
      ++modPriority;
    }
  }
  if (priority)
    priority->get() = modPriority;

  return mod;
}

std::size_t hash<InputVariant>::operator()(InputVariant const& v) const {
  std::size_t indexHash = hashOf(static_cast<std::uint8_t>(v.index()));
  if (std::holds_alternative<Key>(v))
    hashCombine(indexHash, hashOf(std::get<Key>(v)));
  else if (std::holds_alternative<MouseButton>(v))
    hashCombine(indexHash, hashOf(std::get<MouseButton>(v)));
  else if (std::holds_alternative<ControllerButton>(v))
    hashCombine(indexHash, hashOf(std::get<ControllerButton>(v)));

  return indexHash;
}

std::expected<Json, StarException> Input::inputEventToJson(InputEvent const& input) {
  String type;
  Json data;

  if (std::holds_alternative<KeyDownEvent>(input)) {
    auto const& keyDown = std::get<KeyDownEvent>(input);
    type = "KeyDown";
    auto name = enumName<KeyNames>(keyDown.key);
    if (!name) return std::unexpected(std::move(name.error()));
    data = JsonObject{
      {"key", String(*name)},
      {"mods", keyModsToJson(keyDown.mods)}
    };
  } else if (std::holds_alternative<KeyUpEvent>(input)) {
    auto const& keyUp = std::get<KeyUpEvent>(input);
    type = "KeyUp";
    auto name = enumName<KeyNames>(keyUp.key);
    if (!name) return std::unexpected(std::move(name.error()));
    data = JsonObject{{"key", String(*name)}};
  } else if (std::holds_alternative<MouseButtonDownEvent>(input)) {
    auto const& mouseDown = std::get<MouseButtonDownEvent>(input);
    type = "MouseButtonDown";
    auto name = enumName<MouseButtonNames>(mouseDown.mouseButton);
    if (!name) return std::unexpected(std::move(name.error()));
    data = JsonObject{
      {"mouseButton", String(*name)},
      {"mousePosition", jsonFromVec2F(mouseDown.mousePosition)}
    };
  } else if (std::holds_alternative<MouseButtonUpEvent>(input)) {
    auto const& mouseUp = std::get<MouseButtonUpEvent>(input);
    type = "MouseButtonUp";
    auto name = enumName<MouseButtonNames>(mouseUp.mouseButton);
    if (!name) return std::unexpected(std::move(name.error()));
    data = JsonObject{
      {"mouseButton", String(*name)},
      {"mousePosition", jsonFromVec2F(mouseUp.mousePosition)}
    };
  } else if (std::holds_alternative<MouseWheelEvent>(input)) {
    auto const& mouseWheel = std::get<MouseWheelEvent>(input);
    type = "MouseWheel";
    data = JsonObject{
      {"mouseWheel", mouseWheel.mouseWheel == MouseWheel::Up ? 1 : -1},
      {"mousePosition", jsonFromVec2F(mouseWheel.mousePosition)}
    };
  } else if (std::holds_alternative<MouseMoveEvent>(input)) {
    auto const& mouseMove = std::get<MouseMoveEvent>(input);
    type = "MouseMove";
    data = JsonObject{
      {"mouseMove", jsonFromVec2F(mouseMove.mouseMove)},
      {"mousePosition", jsonFromVec2F(mouseMove.mousePosition)}
    };
  } else if (std::holds_alternative<ControllerButtonDownEvent>(input)) {
    auto const& controllerDown = std::get<ControllerButtonDownEvent>(input);
    type = "ControllerButtonDown";
    auto name = enumName<ControllerButtonNames>(controllerDown.controllerButton);
    if (!name) return std::unexpected(std::move(name.error()));
    data = JsonObject{
      {"controllerButton", String(*name)},
      {"controller", controllerDown.controller}};
  } else if (std::holds_alternative<ControllerButtonUpEvent>(input)) {
    auto const& controllerUp = std::get<ControllerButtonUpEvent>(input);
    type = "ControllerButtonUp";
    auto name = enumName<ControllerButtonNames>(controllerUp.controllerButton);
    if (!name) return std::unexpected(std::move(name.error()));
    data = JsonObject{
      {"controllerButton", String(*name)},
      {"controller", controllerUp.controller}};
  } else if (std::holds_alternative<ControllerAxisEvent>(input)) {
    auto const& controllerAxis = std::get<ControllerAxisEvent>(input);
    type = "ControllerAxis";
    auto name = enumName<ControllerAxisNames>(controllerAxis.controllerAxis);
    if (!name) return std::unexpected(std::move(name.error()));
    data = JsonObject{
      {"controllerAxis", String(*name)},
      {"controllerAxisValue", controllerAxis.controllerAxisValue},
      {"controller", controllerAxis.controller}};
  }

  if (data) {
    return JsonObject{
      {"type", type},
      {"data", data}
    };
  }

  return data;
}

std::expected<Input::Bind, StarException> Input::bindFromJson(Json const& json) {
  Bind bind;
  if (json.isNull())
    return bind;

  auto typeResult = json.getString("type");
  if (!typeResult) return std::unexpected(std::move(typeResult.error()));
  String const& type = *typeResult;
  auto valueResult = json.get("value", {});
  if (!valueResult) return std::unexpected(std::move(valueResult.error()));
  Json const& value = *valueResult;
  auto text = value.toString();
  if ((type == "key" || type == "mouse" || type == "controller") && !text)
    return std::unexpected(std::move(text.error()));

  if (type == "key") {
    KeyBind keyBind;
    auto key = maybeEnumValue<KeyNames>(*text);
    if (!key) return std::unexpected(std::move(key.error()));
    if (!*key) return bind;
    keyBind.key = **key;
    auto mods = json.get("mods", Json());
    if (!mods) return std::unexpected(std::move(mods.error()));
    auto parsed = keyModsFromJson(*mods, std::ref(keyBind.priority));
    if (!parsed) return std::unexpected(std::move(parsed.error()));
    keyBind.mods = *parsed;
    bind = std::move(keyBind);
  }
  else if (type == "mouse") {
    MouseBind mouseBind;
    auto button = maybeEnumValue<MouseButtonNames>(*text);
    if (!button) return std::unexpected(std::move(button.error()));
    if (!*button) return bind;
    mouseBind.button = **button;
    auto mods = json.get("mods", Json());
    if (!mods) return std::unexpected(std::move(mods.error()));
    auto parsed = keyModsFromJson(*mods, std::ref(mouseBind.priority));
    if (!parsed) return std::unexpected(std::move(parsed.error()));
    mouseBind.mods = *parsed;
    bind = std::move(mouseBind);
  }
  else if (type == "controller") {
    ControllerBind controllerBind;
    auto button = maybeEnumValue<ControllerButtonNames>(*text);
    if (!button) return std::unexpected(std::move(button.error()));
    if (!*button) return bind;
    controllerBind.button = **button;
    auto controller = json.getUInt("controller", 0);
    if (!controller) return std::unexpected(std::move(controller.error()));
    controllerBind.controller = *controller;
    bind = std::move(controllerBind);
  }

  return bind;
}

std::expected<Json, StarException> Input::bindToJson(Bind const& bind) {
  if (std::holds_alternative<KeyBind>(bind)) {
    auto const& keyBind = std::get<KeyBind>(bind);
    auto name = enumName<KeyNames>(keyBind.key);
    if (!name) return std::unexpected(std::move(name.error()));
    auto obj = JsonObject{
      {"type", "key"},
      {"value", String(*name)}
    }; // don't want empty mods to exist as null entry
    if (auto mods = keyModsToJson(keyBind.mods))
      obj.emplace("mods", std::move(mods));
    return obj;
  }
  else if (std::holds_alternative<MouseBind>(bind)) {
    auto const& mouseBind = std::get<MouseBind>(bind);
    auto name = enumName<MouseButtonNames>(mouseBind.button);
    if (!name) return std::unexpected(std::move(name.error()));
    auto obj = JsonObject{
      {"type", "mouse"},
      {"value", String(*name)}
    };
    if (auto mods = keyModsToJson(mouseBind.mods))
      obj.emplace("mods", std::move(mods));
    return obj;
  }
  else if (std::holds_alternative<ControllerBind>(bind)) {
    auto const& controllerBind = std::get<ControllerBind>(bind);
    auto name = enumName<ControllerButtonNames>(controllerBind.button);
    if (!name) return std::unexpected(std::move(name.error()));
    return JsonObject{
      {"type", "controller"},
      {"value", String(*name)},
      {"controller", controllerBind.controller}
    };
  }

  return Json();
}

Input::BindEntry::BindEntry(Input& input, BindCategory const& parentCategory)
  : owner(input), category(parentCategory), stateId(input.m_nextBindEntryId++) {}

std::expected<Input::BindEntry, StarException> Input::BindEntry::create(Input& input, String entryId, Json const& config, BindCategory const& parentCategory) {
  BindEntry result(input, parentCategory);
  result.id = std::move(entryId);
  auto name = config.getString("name", result.id);
  if (!name) return std::unexpected(std::move(name.error()));
  result.name = std::move(*name);
  auto tags = config.get("tags", JsonArray());
  if (!tags) return std::unexpected(std::move(tags.error()));
  auto tagList = jsonToStringList(*tags);
  if (!tagList) return std::unexpected(std::move(tagList.error()));
  result.tags = std::move(*tagList);
  auto defaults = config.getArray("default", {});
  if (!defaults) return std::unexpected(std::move(defaults.error()));
  for (Json const& jBind : *defaults) {
    auto bind = bindFromJson(jBind);
    if (!bind) {
      if (!bind.error().is<JsonException>()) return std::unexpected(std::move(bind.error()));
      auto logged = input.m_root.logger().errorException(strf("Binds: Error loading default bind in {}.{}", parentCategory.id, result.id), ExceptionDiagnostic(bind.error()));
      if (!logged) return std::unexpected(std::move(logged.error()));
    } else result.defaultBinds.emplace_back(std::move(*bind));
  }
  return result;
}

std::expected<void, StarException> Input::BindEntry::updated() {
  auto config = owner.m_root.configuration();
  if (!config) return std::unexpected(std::move(config.error()));
  JsonArray array;
  for (auto const& bind : customBinds) {
    auto json = bindToJson(bind);
    if (!json) return std::unexpected(std::move(json.error()));
    array.emplace_back(std::move(*json));
  }
  auto root = (*config)->get(InputBindingConfigRoot);
  if (!root) return std::unexpected(std::move(root.error()));
  if (!root->isType(Json::Type::Object)) {
    if (auto r = (*config)->set(InputBindingConfigRoot, JsonObject()); !r) return std::unexpected(std::move(r.error()));
  }
  String path = strf("{}.{}", InputBindingConfigRoot, category.get().id);
  auto category = (*config)->getPath(path);
  if (!category) return std::unexpected(std::move(category.error()));
  if (!category->isType(Json::Type::Object)) {
    if (auto r = (*config)->setPath(path, JsonObject{{id, std::move(array)}}); !r) return std::unexpected(std::move(r.error()));
  } else {
    path = strf("{}.{}", path, id);
    if (auto r = (*config)->setPath(path, array); !r) return std::unexpected(std::move(r.error()));
  }
  owner.rebuildMappings();
  return {};
}

Input::BindRef::BindRef(BindEntry& bindEntry, KeyBind& keyBind) : entry(bindEntry) {
  priority = keyBind.priority;
  mods = keyBind.mods;
}

Input::BindRef::BindRef(BindEntry& bindEntry, MouseBind& mouseBind) : entry(bindEntry) {
  priority = mouseBind.priority;
  mods = mouseBind.mods;
}

Input::BindRef::BindRef(BindEntry& bindEntry) : entry(bindEntry) {
  priority = 0;
  mods = KeyMod::NoMod;
}

Input::BindCategory::BindCategory(Input& input) : owner(input) {}

std::expected<void, StarException> Input::BindCategory::mergeJson(String const& categoryId, Json const& categoryConfig) {
  id = categoryId;
  config = jsonMerge(config, categoryConfig);
  auto categoryName = config.getString("name", id);
  if (!categoryName) return std::unexpected(std::move(categoryName.error()));
  name = std::move(*categoryName);
  entries.clear();
  auto userConfig = owner.m_root.configuration();
  if (!userConfig) return std::unexpected(std::move(userConfig.error()));
  auto userBindings = (*userConfig)->get(InputBindingConfigRoot);
  if (!userBindings) return std::unexpected(std::move(userBindings.error()));
  auto configured = config.getObject("binds", {});
  if (!configured) return std::unexpected(std::move(configured.error()));
  for (auto const& [bindId, bindConfig] : *configured) {
    if (!bindConfig.isType(Json::Type::Object)) continue;
    auto built = BindEntry::create(owner, bindId, bindConfig, *this);
    if (!built) return std::unexpected(std::move(built.error()));
    auto& entry = entries.try_emplace(bindId, std::move(*built)).first->second;
    if (userBindings->isType(Json::Type::Object)) {
      auto bindings = userBindings->queryArray(strf("{}.{}", id, bindId), {});
      if (!bindings) return std::unexpected(std::move(bindings.error()));
      for (auto const& jBind : *bindings) {
        auto bind = bindFromJson(jBind);
        if (!bind) {
          if (!bind.error().is<JsonException>()) return std::unexpected(std::move(bind.error()));
          auto logged = owner.m_root.logger().errorException(strf("Binds: Error loading user bind in {}.{}", id, bindId), ExceptionDiagnostic(bind.error()));
          if (!logged) return std::unexpected(std::move(logged.error()));
        } else entry.customBinds.emplace_back(std::move(*bind));
      }
    }
    if (entry.customBinds.empty()) entry.customBinds = entry.defaultBinds;
  }
  return {};
}

List<std::reference_wrapper<Input::BindEntry>> Input::filterBindEntries(List<Input::BindRef> const& binds, KeyMod mods) const {
  std::uint8_t maxPriority = 0;
  List<std::reference_wrapper<BindEntry>> result{};
  for (const BindRef& bind : binds) {
    if (bind.priority < maxPriority)
      break;
    else if (compareKeyModLenient(mods, bind.mods)) {
      maxPriority = bind.priority;
      result.emplace_back(bind.entry);
    }
  }
  return result;
}

std::optional<std::reference_wrapper<Input::BindEntry>> Input::maybeBindEntry(String const& categoryId, String const& bindId) {
  if (auto category = m_bindCategories.find_ref(categoryId))
    return category->get().entries.find_ref(bindId);
  return std::nullopt;
}

std::expected<std::reference_wrapper<Input::BindEntry>, StarException> Input::bindEntry(String const& categoryId, String const& bindId) {
  if (auto entry = maybeBindEntry(categoryId, bindId))
    return *entry;
  else
    return std::unexpected(InputException::format("Could not find bind entry {}.{}", categoryId, bindId));
}

std::optional<std::reference_wrapper<Input::InputState>> Input::maybeBindState(String const& categoryId, String const& bindId) {
  if (auto entry = maybeBindEntry(categoryId, bindId)) {
    if (auto state = m_bindStates.find_ref(entry->get().stateId))
      return std::ref(static_cast<InputState&>(state->get()));
  }
  return std::nullopt;
}

Input::InputState& Input::addBindState(BindEntry const& bindEntry) {
  auto insertion = m_bindStates.insert(bindEntry.stateId, BindInputState(bindEntry));
  if (insertion.second) {
    for (auto& tag : bindEntry.tags)
      ++m_activeTags[tag];
  }
  return insertion.first->second;
}

Input::Input(Root& root) : m_root(root), m_pressedMods(KeyMod::NoMod) {}

std::expected<std::shared_ptr<Input>, StarException> Input::create(Root& root) {
  auto result = std::shared_ptr<Input>(new Input(root));
  if (auto r = result->reload(); !r) return std::unexpected(std::move(r.error()));
  result->m_rootReloadListener = std::make_shared<CallbackListener>([input = std::ref(*result)]() {
    return input.get().reload();
  });
  result->m_rootReloadSubscription = root.registerReloadListener(result->m_rootReloadListener);
  return result;
}

Input::~Input() {
  m_rootReloadSubscription.disconnect();
}

List<std::pair<InputEvent, bool>> const& Input::inputEventsThisFrame() const {
  return m_inputEvents;
}



void Input::reset(bool clear) {
  m_inputEvents.clear();
  if (clear) {
    m_keyStates.clear();
    m_mouseStates.clear();
    m_controllerStates.clear();
    m_bindStates.clear();
  } else {
    auto eraseCond = [](auto&& p) {
      if (p.second.held)
        p.second.reset();
      return !p.second.held;
    };

    eraseWhere(m_keyStates,        eraseCond);
    eraseWhere(m_mouseStates,      eraseCond);
    eraseWhere(m_controllerStates, eraseCond);
    eraseWhere(m_bindStates, [&](auto&& p) {
      if (p.second.held)
        p.second.reset();
      else {
        for (auto& tag : p.second.entry.get().tags) {
          auto find = m_activeTags.find(tag);
          if (find != m_activeTags.end() && !--find->second)
            m_activeTags.erase(find);
        }
        return true;
      }
      return false;
    });
  }
}

void Input::update() {
  reset(false);
}

std::expected<bool, StarException> Input::handleInput(InputEvent const& input, bool gameProcessed) {
  if (!KeysToMods) return std::unexpected(KeysToMods.error());
  m_inputEvents.emplace_back(input, gameProcessed);
  if (std::holds_alternative<KeyDownEvent>(input)) {
    auto const& keyDown = std::get<KeyDownEvent>(input);
    auto keyToMod = KeysToMods->find_right_ref(keyDown.key);
    if (keyToMod)
      m_pressedMods |= keyToMod->get();

    if (!gameProcessed && !m_textInputActive) {
      auto& state = m_keyStates[keyDown.key];
      if (keyToMod)
        state.mods |= keyToMod->get();
      state.press();

      if (auto binds = m_bindMappings.find_ref(keyDown.key)) {
        for (auto bind : filterBindEntries(binds->get(), keyDown.mods))
          addBindState(bind.get()).press();
      }
    }
  }
  else if (std::holds_alternative<KeyUpEvent>(input)) {
    auto const& keyUp = std::get<KeyUpEvent>(input);
    auto keyToMod = KeysToMods->find_right_ref(keyUp.key);
    if (keyToMod)
      m_pressedMods &= ~keyToMod->get();

    // We need to be able to release input even when gameProcessed is true, but only if it's already down.
    if (auto state = m_keyStates.find_ref(keyUp.key)) {
      if (keyToMod)
        state->get().mods &= ~keyToMod->get();
      state->get().release();
    }

    if (auto binds = m_bindMappings.find_ref(keyUp.key)) {
      for (auto& bind : binds->get()) {
        if (auto state = m_bindStates.find_ref(bind.entry.get().stateId))
          state->get().release();
      }
    }
  }
  else if (std::holds_alternative<MouseButtonDownEvent>(input)) {
    auto const& mouseDown = std::get<MouseButtonDownEvent>(input);
    m_mousePosition = mouseDown.mousePosition;
    if (!gameProcessed) {
      auto& state = m_mouseStates[mouseDown.mouseButton];
      state.pressPositions.append(mouseDown.mousePosition);
      state.press();

      if (auto binds = m_bindMappings.find_ref(mouseDown.mouseButton)) {
        for (auto bind : filterBindEntries(binds->get(), m_pressedMods))
          addBindState(bind.get()).press();
      }
    }
  }
  else if (std::holds_alternative<MouseButtonUpEvent>(input)) {
    auto const& mouseUp = std::get<MouseButtonUpEvent>(input);
    m_mousePosition = mouseUp.mousePosition;
    if (auto state = m_mouseStates.find_ref(mouseUp.mouseButton)) {
      state->get().releasePositions.append(mouseUp.mousePosition);
      state->get().release();
    }

    if (auto binds = m_bindMappings.find_ref(mouseUp.mouseButton)) {
      for (auto& bind : binds->get()) {
        if (auto state = m_bindStates.find_ref(bind.entry.get().stateId))
          state->get().release();
      }
    }
  }
  else if (std::holds_alternative<MouseMoveEvent>(input)) {
    auto const& mouseMove = std::get<MouseMoveEvent>(input);
    m_mousePosition = mouseMove.mousePosition;
  }
  else if (std::holds_alternative<ControllerButtonDownEvent>(input)) {
    auto const& controllerDown = std::get<ControllerButtonDownEvent>(input);
    if (!gameProcessed) {
      auto& state = m_controllerStates[controllerDown.controllerButton];
      state.press();

      if (auto binds = m_bindMappings.find_ref(controllerDown.controllerButton)) {
        for (auto bind : filterBindEntries(binds->get(), m_pressedMods))
          addBindState(bind.get()).press();
      }
    }
  }
  else if (std::holds_alternative<ControllerButtonUpEvent>(input)) {
    auto const& controllerUp = std::get<ControllerButtonUpEvent>(input);
    if (auto state = m_controllerStates.find_ref(controllerUp.controllerButton))
      state->get().release();

    if (auto binds = m_bindMappings.find_ref(controllerUp.controllerButton)) {
      for (auto& bind : binds->get()) {
        if (auto state = m_bindStates.find_ref(bind.entry.get().stateId))
          state->get().release();
      }
    }
  }

  return false;
}

void Input::rebuildMappings() {
  reset(true);
  m_bindMappings.clear();

  for (auto& category : m_bindCategories) {
    for (auto& pair : category.second.entries) {
      auto& entry = pair.second;
      for (auto& bind : entry.customBinds) {
        if (std::holds_alternative<KeyBind>(bind)) {
          auto& keyBind = std::get<KeyBind>(bind);
          m_bindMappings[keyBind.key].emplace_back(entry, keyBind);
        }
        if (std::holds_alternative<MouseBind>(bind)) {
          auto& mouseBind = std::get<MouseBind>(bind);
          m_bindMappings[mouseBind.button].emplace_back(entry, mouseBind);
        }
        if (std::holds_alternative<ControllerBind>(bind))
          m_bindMappings[std::get<ControllerBind>(bind).button].emplace_back(entry);
      }
    }
  }

  for (auto&& pair : m_bindMappings) {
    pair.second.sort([](BindRef const& a, BindRef const& b)
      { return a.priority > b.priority; });
  }
}

std::expected<void, StarException> Input::reload() {
  reset(true);
  m_bindMappings.clear();
  m_bindCategories.clear();
  auto assets = m_root.assets();
  if (!assets) return std::unexpected(std::move(assets.error()));
  auto paths = (*assets)->scanExtension("binds");
  if (!paths) return std::unexpected(std::move(paths.error()));
  for (auto const& bindPath : paths->get()) {
    auto json = (*assets)->json(bindPath.string());
    if (!json) return std::unexpected(std::move(json.error()));
    auto categories = json->toObject();
    if (!categories) return std::unexpected(std::move(categories.error()));
    for (auto const& pair : *categories) {
      if (!pair.second.isType(Json::Type::Object)) continue;
      auto r = m_bindCategories.try_emplace(pair.first, *this).first->second.mergeJson(pair.first, pair.second);
      if (!r) return std::unexpected(std::move(r.error()));
    }
  }
  std::size_t count = 0;
  for (auto const& pair : m_bindCategories) count += pair.second.entries.size();
  if (auto r = m_root.logger().info("Binds: Loaded {} bind{}", count, count == 1 ? "" : "s"); !r) return std::unexpected(std::move(r.error()));
  rebuildMappings();
  return {};
}

void Input::setTextInputActive(bool active) {
  m_textInputActive = active;
}

std::optional<unsigned> Input::bindDown(String const& categoryId, String const& bindId) {
  if (auto state = maybeBindState(categoryId, bindId)) {
    if (state->get().presses)
      return state->get().presses;
  }
  return {};
}

bool Input::bindHeld(String const& categoryId, String const& bindId) {
  if (auto state = maybeBindState(categoryId, bindId))
    return state->get().held;
  else
    return false;
}

std::optional<unsigned> Input::bindUp(String const& categoryId, String const& bindId) {
  if (auto state = maybeBindState(categoryId, bindId)) {
    if (state->get().releases)
      return state->get().releases;
  }
  return {};
}

std::optional<unsigned> Input::keyDown(Key key, std::optional<KeyMod> keyMod) {
  if (auto state = m_keyStates.find_ref(key)) {
    if (state->get().presses && (!keyMod || compareKeyMod(*keyMod, state->get().mods)))
      return state->get().presses;
  }
  return {};
}

bool Input::keyHeld(Key key) {
  auto state = m_keyStates.find_ref(key);
  return state && state->get().held;
}

std::optional<unsigned> Input::keyUp(Key key) {
  if (auto state = m_keyStates.find_ref(key)) {
    if (state->get().releases)
      return state->get().releases;
  }
  return {};
}

std::optional<List<Vec2F>> Input::mouseDown(MouseButton button) {
  if (auto state = m_mouseStates.find_ref(button)) {
    if (state->get().presses)
      return state->get().pressPositions;
  }
  return {};
}

bool Input::mouseHeld(MouseButton button) {
  auto state = m_mouseStates.find_ref(button);
  return state && state->get().held;
}

std::optional<List<Vec2F>> Input::mouseUp(MouseButton button) {
  if (auto state = m_mouseStates.find_ref(button)) {
    if (state->get().releases)
      return state->get().releasePositions;
  }
  return {};
}

Vec2F Input::mousePosition() const {
  return m_mousePosition;
}

std::expected<void, StarException> Input::resetBinds(String const& categoryId, String const& bindId) {
  auto entry = bindEntry(categoryId, bindId);
  if (!entry) return std::unexpected(std::move(entry.error()));
  entry->get().customBinds = entry->get().defaultBinds;
  return entry->get().updated();
}

std::expected<Json, StarException> Input::getDefaultBinds(String const& categoryId, String const& bindId) {
  auto entry = bindEntry(categoryId, bindId);
  if (!entry) return std::unexpected(std::move(entry.error()));
  JsonArray array;
  for (Bind const& bind : entry->get().defaultBinds) {
    auto json = bindToJson(bind);
    if (!json) return std::unexpected(std::move(json.error()));
    array.emplace_back(std::move(*json));
  }
  return array;
}

std::expected<Json, StarException> Input::getBinds(String const& categoryId, String const& bindId) {
  auto entry = bindEntry(categoryId, bindId);
  if (!entry) return std::unexpected(std::move(entry.error()));
  JsonArray array;
  for (Bind const& bind : entry->get().customBinds) {
    auto json = bindToJson(bind);
    if (!json) return std::unexpected(std::move(json.error()));
    array.emplace_back(std::move(*json));
  }
  return array;
}

std::expected<void, StarException> Input::setBinds(String const& categoryId, String const& bindId, Json const& jBinds) {
  auto entry = bindEntry(categoryId, bindId);
  if (!entry) return std::unexpected(std::move(entry.error()));
  auto array = jBinds.toArray();
  if (!array) return std::unexpected(std::move(array.error()));
  List<Bind> binds;
  for (Json const& jBind : *array) {
    auto bind = bindFromJson(jBind);
    if (!bind) return std::unexpected(std::move(bind.error()));
    binds.emplace_back(std::move(*bind));
  }
  entry->get().customBinds = std::move(binds);
  return entry->get().updated();
}

unsigned Input::getTag(String const& tagName) const {
  if (auto tag = m_activeTags.find_ref(tagName))
    return tag->get();
  else
    return 0;
}

Input::ClipboardUnlock::ClipboardUnlock(Input& input)
    : m_input(std::ref(input)) { ++m_input->get().m_clipboardAllowed; };

Input::ClipboardUnlock::ClipboardUnlock(ClipboardUnlock&& other) : m_input(std::exchange(other.m_input, std::nullopt)) {};

Input::ClipboardUnlock::~ClipboardUnlock() { if (m_input) --m_input->get().m_clipboardAllowed; };


Input::ClipboardUnlock Input::unlockClipboard() {
  return Input::ClipboardUnlock(*this);
}

bool Input::clipboardAllowed() const {
  return m_clipboardAllowed > 0 ? true : getTag("clipboard") > 0;
}

}
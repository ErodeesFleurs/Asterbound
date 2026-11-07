import std;
import star.format;
import star.json;
import star.json_extra;
import star.strong_typedef;
import star.data_stream;
import star.bi_map;
import star.game_types;
import star.vector;
import star.casting;
import star.input_event;
import star.color;
import star.font;
import star.list;
import star.image_processing;
import star.hash;
import star.string_view;
import star.thread;
import star.directives;
import star.ordered_map;
import star.io_device;
import star.ref_ptr;
import star.logging;
import star.version;
import star.string;
import star.text;
import star.asset_path;
import star.listener;
import star.set;
import star.audio;
import star.map;
import star.lua;

import star.celestial_coordinate;
import star.quest_descriptor;
import star.item;
import star.item_descriptor;
import star.item_recipe;

import star.application;
import star.statistics_service;
import star.p2p_networking_service;
import star.user_generated_content_service;
import star.desktop_service;
import star.image;
import star.application_controller;
import star.renderer;
import star.asset_source;
import star.assets;
import star.root_base;
import star.configuration;
import star.root;
import star.font_texture_group;
import star.anchor_types;
import star.text_painter;
import star.drawable;
import star.asset_texture_group;
import star.drawable_painter;
import star.gui_types;
import star.key_bindings;
import star.mixer;
import star.gui_context;
import star.widget;
import star.pane;
import star.lua_components;
import star.lua_root;
import star.widget_parsing;
import star.gui_reader;


import star.base_script_pane;
import star.lua_game_converters;
import star.widget_lua_bindings;
import star.canvas_widget;
import star.progress_widget;
import star.animation;
import star.item_slot_widget;
import star.item_grid_widget;
import star.image_widget;

import star.simple_tooltip;
import star.item_tooltip;
import star.config_lua_bindings;
import star.item_database;

namespace Star {


BaseScriptPane::BaseScriptPane(GuiContext& guiContext)
  : Pane(guiContext), m_interactive(true), m_callbacksAdded(false), m_script(guiContext.root()) {}

std::expected<std::shared_ptr<BaseScriptPane>, StarException> BaseScriptPane::create(GuiContext& guiContext, Json config, bool construct) {
  auto pane = std::shared_ptr<BaseScriptPane>(new BaseScriptPane(guiContext));
  auto initialized = pane->init(std::move(config), construct);
  if (!initialized)
    return std::unexpected(std::move(initialized.error()));
  return pane;
}

std::expected<void, StarException> BaseScriptPane::init(Json config, bool construct) {
  auto initialized = initializePane();
  if (!initialized)
    return std::unexpected(std::move(initialized.error()));
  m_rawConfig = config;
  auto assets = context().root().assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  bool hasBase = false;
  if (config.type() == Json::Type::Object) {
    auto contains = config.contains("baseConfig");
    if (!contains)
      return std::unexpected(std::move(contains.error()));
    hasBase = *contains;
  }
  if (hasBase) {
    auto path = config.getString("baseConfig");
    if (!path)
      return std::unexpected(std::move(path.error()));
    auto baseConfig = (*assets)->fetchJson(*path);
    if (!baseConfig)
      return std::unexpected(std::move(baseConfig.error()));
    m_config = jsonMerge(*baseConfig, config);
  } else {
    auto fetched = (*assets)->fetchJson(config);
    if (!fetched)
      return std::unexpected(std::move(fetched.error()));
    m_config = std::move(*fetched);
  }
  auto interactive = m_config.getBool("interactive", true);
  if (!interactive)
    return std::unexpected(std::move(interactive.error()));
  m_interactive = *interactive;
  m_reader = std::make_shared<GuiReader>(context());
  m_reader->registerCallback("close", [this](Widget&) -> std::expected<void, StarException> { dismiss(); return {}; });
  auto callbacksJson = m_config.get("scriptWidgetCallbacks", JsonArray{});
  if (!callbacksJson)
    return std::unexpected(std::move(callbacksJson.error()));
  auto callbacks = jsonToStringList(*callbacksJson);
  if (!callbacks)
    return std::unexpected(std::move(callbacks.error()));
  for (auto const& callbackName : *callbacks) {
    m_reader->registerCallback(callbackName, [this, callbackName](Widget& widget) -> std::expected<void, StarException> {
      auto result = m_script.invoke(callbackName, widget.name(), widget.data());
      if (!result)
        return std::unexpected(std::move(result.error()));
      return {};
    });
  }
  if (construct) {
    auto gui = m_config.get("gui");
    if (!gui)
      return std::unexpected(std::move(gui.error()));
    auto layout = (*assets)->fetchJson(*gui);
    if (!layout)
      return std::unexpected(std::move(layout.error()));
    return this->construct(std::move(*layout));
  }
  return {};
}

std::expected<void, StarException> BaseScriptPane::show() {
  return Pane::show();
}

std::expected<void, StarException> BaseScriptPane::displayed() {
  auto displayed = Pane::displayed();
  if (!displayed)
    return std::unexpected(std::move(displayed.error()));
  if (!m_callbacksAdded) {
    auto paneCallbacks = makePaneCallbacks();
    if (!paneCallbacks)
      return std::unexpected(std::move(paneCallbacks.error()));
    auto paneAdded = m_script.addCallbacks("pane", std::move(*paneCallbacks));
    if (!paneAdded)
      return std::unexpected(std::move(paneAdded.error()));
    auto widgetCallbacks = LuaBindings::makeWidgetCallbacks(context(), *this, m_reader);
    if (!widgetCallbacks)
      return std::unexpected(std::move(widgetCallbacks.error()));
    auto widgetAdded = m_script.addCallbacks("widget", std::move(*widgetCallbacks));
    if (!widgetAdded)
      return std::unexpected(std::move(widgetAdded.error()));
    auto configCallbacks = LuaBindings::makeConfigCallbacks([this](String const& name, Json const& def) {
      return m_config.query(name, def);
    });
    if (!configCallbacks)
      return std::unexpected(std::move(configCallbacks.error()));
    auto configAdded = m_script.addCallbacks("config", std::move(*configCallbacks));
    if (!configAdded)
      return std::unexpected(std::move(configAdded.error()));
    m_callbacksAdded = true;
  }
  auto initialized = m_script.init();
  if (!initialized)
    return std::unexpected(std::move(initialized.error()));
  auto invoked = m_script.invoke("displayed");
  if (!invoked)
    return std::unexpected(std::move(invoked.error()));
  return {};
}

std::expected<void, StarException> BaseScriptPane::stopScripts() {
  return m_script.uninit();
}

std::expected<void, StarException> BaseScriptPane::resumeScripts() {
  if (!m_script.initialized()) {
    auto initialized = m_script.init();
    if (!initialized)
      return std::unexpected(std::move(initialized.error()));
  }
  return {};
}

std::expected<void, StarException> BaseScriptPane::dismissed() {
  auto dismissed = Pane::dismissed();
  auto uninitialized = m_script.uninit("dismissed");
  if (!dismissed)
    return std::unexpected(std::move(dismissed.error()));
  return uninitialized;
}

std::expected<void, StarException> BaseScriptPane::tick(float dt) {
  auto ticked = Pane::tick(dt);
  if (!ticked)
    return std::unexpected(std::move(ticked.error()));
  for (auto const& p : m_canvasClickCallbacks) {
    for (auto const& clickEvent : p.first->pullClickEvents()) {
      auto result = m_script.invoke(p.second, jsonFromVec2I(clickEvent.position),
                                    static_cast<std::uint8_t>(clickEvent.button), clickEvent.buttonDown);
      if (!result)
        return std::unexpected(std::move(result.error()));
    }
  }
  for (auto const& p : m_canvasKeyCallbacks) {
    for (auto const& keyEvent : p.first->pullKeyEvents()) {
      auto keyName = enumName<KeyNames>(keyEvent.key);
      if (!keyName)
        return std::unexpected(std::move(keyName.error()));
      // KeyNames borrows whole static string literals; Lua copies their bytes.
      auto result = m_script.invoke(p.second, static_cast<int>(keyEvent.key), keyEvent.keyDown, *keyName);
      if (!result)
        return std::unexpected(std::move(result.error()));
    }
  }
  auto updated = m_script.update(m_script.updateDt(dt));
  if (!updated)
    return std::unexpected(std::move(updated.error()));
  return {};
}

std::expected<bool, StarException> BaseScriptPane::sendEvent(InputEvent const& event) {
  if (!visibility() || !interactive())
    return false;
  auto handlesInput = m_script.hasMethod("onInput");
  if (!handlesInput)
    return std::unexpected(std::move(handlesInput.error()));
  if (*handlesInput) {
    auto data = inputJson(event);
    if (!data)
      return std::unexpected(std::move(data.error()));
    auto consumed = m_script.invoke<bool>("onInput", *data);
    if (!consumed)
      return std::unexpected(std::move(consumed.error()));
    if (consumed->value_or(false))
      return true;
  }
  // GuiClose must dismiss even when the canvas does not implement it.
  if (context().actions(event).contains(InterfaceAction::GuiClose)) {
    auto dismissable = m_config.getBool("dismissable", true);
    if (!dismissable)
      return std::unexpected(std::move(dismissable.error()));
    if (*dismissable) {
      dismiss();
      return true;
    }
  }
  return Pane::sendEvent(event);
}

Json const& BaseScriptPane::config() const { return m_config; }
Json const& BaseScriptPane::rawConfig() const { return m_rawConfig; }

bool BaseScriptPane::interactive() const { return m_interactive; }

std::expected<std::shared_ptr<Pane>, StarException> BaseScriptPane::createTooltip(Vec2I const& screenPosition) {
  auto result = m_script.invoke<Json>("createTooltip", screenPosition);
  if (!result)
    return std::unexpected(std::move(result.error()));
  if (*result && !(**result).isNull()) {
    auto const& value = **result;
    if (value.type() == Json::Type::String) {
      auto text = value.toString();
      if (!text)
        return std::unexpected(std::move(text.error()));
      return SimpleTooltipBuilder::buildTooltip(context(), *text);
    }
    auto tooltip = Pane::create(context());
    if (!tooltip)
      return std::unexpected(std::move(tooltip.error()));
    auto constructed = m_reader->construct(value, **tooltip);
    if (!constructed)
      return std::unexpected(std::move(constructed.error()));
    return std::move(*tooltip);
  }
  std::shared_ptr<Item> item;
  auto authoredItem = m_script.invoke<LuaValue>("tooltipItem", screenPosition);
  if (!authoredItem)
    return std::unexpected(std::move(authoredItem.error()));
  if (*authoredItem) {
    if (**authoredItem == LuaNil)
      return std::shared_ptr<Pane>{};
    auto handle = std::get_if<LuaUserData>(&**authoredItem);
    if (!handle)
      return std::unexpected(GuiException("tooltipItem must return an Item handle or nil"));
    auto value = handle->get<std::shared_ptr<Item>>();
    if (!value)
      return std::unexpected(std::move(value.error()));
    item = value->get();
  } else if (auto child = getChildAt(screenPosition)) {
    if (auto itemSlot = as<ItemSlotWidget>(child))
      item = itemSlot->item();
    if (auto itemGrid = as<ItemGridWidget>(child)) {
      auto selected = itemGrid->itemAt(screenPosition);
      if (!selected)
        return std::unexpected(std::move(selected.error()));
      item = std::move(*selected);
    }
  }
  if (item) {
    auto available = m_script.hasMethod("interface.itemTooltip");
    if (!available) return std::unexpected(std::move(available.error()));
    if (*available) {
      auto handle = m_script.context()->createUserData(std::move(item));
      if (!handle) return std::unexpected(std::move(handle.error()));
      auto tooltip = m_script.invoke<LuaUserData>("interface.itemTooltip", *handle);
      if (!tooltip) return std::unexpected(std::move(tooltip.error()));
      if (!*tooltip) return std::unexpected(GuiException("Item tooltip service became unavailable"));
      auto pane = (**tooltip).get<std::shared_ptr<Pane>>();
      if (!pane) return std::unexpected(std::move(pane.error()));
      return pane->get();
    }
    return ItemTooltipBuilder::buildItemTooltip(context(), item);
  }
  return std::shared_ptr<Pane>{};
}

std::expected<std::optional<String>, StarException> BaseScriptPane::cursorOverride(Vec2I const& screenPosition) {
  auto result = m_script.invoke<std::optional<String>>("cursorOverride", screenPosition);
  if (!result)
    return std::unexpected(std::move(result.error()));
  if (*result)
    return std::move(**result);
  return std::optional<String>{};
}

std::expected<std::optional<std::shared_ptr<Item>>, StarException> BaseScriptPane::shiftItemFromInventory(std::shared_ptr<Item> const& input) {
  auto result = m_script.invoke<Json>("shiftItemFromInventory", input->descriptor().toJson());
  if (!result)
    return std::unexpected(std::move(result.error()));
  if (!*result || (**result).isNull())
    return std::optional<std::shared_ptr<Item>>{};
  auto database = context().root().itemDatabase();
  if (!database)
    return std::unexpected(std::move(database.error()));
  ItemDescriptor descriptor;
  if ((**result).type() == Json::Type::Bool) {
    auto accepted = (**result).toBool();
    if (!accepted)
      return std::unexpected(std::move(accepted.error()));
    if (!*accepted)
      return std::optional<std::shared_ptr<Item>>{};
  } else {
    auto parsed = ItemDescriptor::create(**result);
    if (!parsed)
      return std::unexpected(std::move(parsed.error()));
    descriptor = std::move(*parsed);
  }
  auto item = (*database)->item(descriptor);
  if (!item)
    return std::unexpected(std::move(item.error()));
  return std::optional<std::shared_ptr<Item>>(std::move(*item));
}

std::shared_ptr<GuiReader> BaseScriptPane::reader() {
  return m_reader;
}

std::expected<void, StarException> BaseScriptPane::construct(Json config) {
  auto constructed = m_reader->construct(config, *this);
  if (!constructed)
    return std::unexpected(std::move(constructed.error()));
  auto dimensions = m_config.opt("size");
  if (!dimensions)
    return std::unexpected(std::move(dimensions.error()));
  if (*dimensions) {
    auto configuredSize = jsonToVec2I(**dimensions);
    if (!configuredSize)
      return std::unexpected(std::move(configuredSize.error()));
    setSize(*configuredSize);
  } else {
    // A widget-only script pane has no native background to establish bounds.
    // Preserve background dimensions while including its declarative children.
    auto backgroundSize = size();
    determineSizeFromChildren();
    setSize(size().piecewiseMax(backgroundSize));
  }
  auto clicks = m_config.getObject("canvasClickCallbacks", {});
  if (!clicks)
    return std::unexpected(std::move(clicks.error()));
  for (auto const& pair : *clicks) {
    auto callback = pair.second.toString();
    if (!callback)
      return std::unexpected(std::move(callback.error()));
    auto canvas = findChild<CanvasWidget>(pair.first);
    if (!canvas)
      return std::unexpected(std::move(canvas.error()));
    if (!*canvas)
      return std::unexpected(GUIBuilderException::format("Missing canvas widget '{}'", pair.first));
    m_canvasClickCallbacks.set(std::move(*canvas), std::move(*callback));
  }
  auto keys = m_config.getObject("canvasKeyCallbacks", {});
  if (!keys)
    return std::unexpected(std::move(keys.error()));
  for (auto const& pair : *keys) {
    auto callback = pair.second.toString();
    if (!callback)
      return std::unexpected(std::move(callback.error()));
    auto canvas = findChild<CanvasWidget>(pair.first);
    if (!canvas)
      return std::unexpected(std::move(canvas.error()));
    if (!*canvas)
      return std::unexpected(GUIBuilderException::format("Missing canvas widget '{}'", pair.first));
    m_canvasKeyCallbacks.set(std::move(*canvas), std::move(*callback));
  }
  auto scriptsJson = m_config.get("scripts", JsonArray());
  if (!scriptsJson)
    return std::unexpected(std::move(scriptsJson.error()));
  auto scripts = jsonToStringList(*scriptsJson);
  if (!scripts)
    return std::unexpected(std::move(scripts.error()));
  auto configured = m_script.setScripts(std::move(*scripts));
  if (!configured)
    return std::unexpected(std::move(configured.error()));
  auto delta = m_config.getUInt("scriptDelta", 1);
  if (!delta)
    return std::unexpected(std::move(delta.error()));
  m_script.setUpdateDelta(*delta);
  m_script.setAutoReInit(true);
  return {};
}

std::expected<void, StarException> BaseScriptPane::addCallbacks(String name, LuaCallbacks callbacks) {
  return m_script.addCallbacks(std::move(name), std::move(callbacks));
}

std::expected<bool, StarException> BaseScriptPane::removeCallbacks(String const& name) {
  return m_script.removeCallbacks(name);
}

std::expected<void, StarException> BaseScriptPane::setLuaRoot(std::shared_ptr<LuaRoot> root) {
  return m_script.setLuaRoot(std::move(root));
}

std::expected<std::optional<Json>, StarException> BaseScriptPane::call(String const& method, JsonArray const& args) {
  return m_script.invoke<Json>(method, luaUnpack(args));
}

std::expected<bool, StarException> BaseScriptPane::hasMethod(String const& method) {
  return m_script.hasMethod(method);
}


std::expected<void, StarException> BaseScriptPane::renderImpl() {
  if (auto result = call("render", {}); !result)
    return std::unexpected(std::move(result.error()));
  return Pane::renderImpl();
}

std::expected<LuaCallbacks, StarException> BaseScriptPane::makePaneCallbacks() {
  auto callbacks = Pane::makePaneCallbacks();
  if (!callbacks)
    return std::unexpected(std::move(callbacks.error()));
  if (auto result = callbacks->registerCallback("isVisible", [owner = weak_from_this()]() -> std::expected<bool, StarException> {
    auto pane = owner.lock();
    if (!pane)
      return std::unexpected(GuiException("Pane callback target no longer exists"));
    return pane->visibility();
  }); !result)
    return std::unexpected(std::move(result.error()));
  return callbacks;
}

std::expected<Json, StarException> BaseScriptPane::inputJson(InputEvent const& event) const {
  JsonObject result;
  JsonArray actions;
  if (!InterfaceActionNames)
    return std::unexpected(InterfaceActionNames.error());
  for (auto action : context().actions(event)) {
    auto name = InterfaceActionNames->getRight(action);
    if (!name)
      return std::unexpected(std::move(name.error()));
    actions.emplace_back(name->get());
  }
  result.set("actions", std::move(actions));
  // Non-pointer input does not depend on window geometry or display scaling.
  if (std::holds_alternative<MouseMoveEvent>(event)
      || std::holds_alternative<MouseButtonDownEvent>(event)
      || std::holds_alternative<MouseButtonUpEvent>(event)
      || std::holds_alternative<MouseWheelEvent>(event)) {
    auto position = *context().mousePosition(event);
    result.set("position", jsonFromVec2I(position - screenPosition()));
    result.set("screenPosition", jsonFromVec2I(position));
  }
  if (auto move = std::get_if<MouseMoveEvent>(&event))
    result.set("physicalPosition", jsonFromVec2F(move->mousePosition));
  else if (auto down = std::get_if<MouseButtonDownEvent>(&event))
    result.set("physicalPosition", jsonFromVec2F(down->mousePosition));
  else if (auto up = std::get_if<MouseButtonUpEvent>(&event))
    result.set("physicalPosition", jsonFromVec2F(up->mousePosition));
  if (auto down = std::get_if<KeyDownEvent>(&event)) {
    auto name = enumName<KeyNames>(down->key);
    if (!name)
      return std::unexpected(std::move(name.error()));
    result.set("type", "keyDown");
    result.set("key", String(*name));
    result.set("mods", static_cast<int>(down->mods));
  } else if (auto up = std::get_if<KeyUpEvent>(&event)) {
    auto name = enumName<KeyNames>(up->key);
    if (!name)
      return std::unexpected(std::move(name.error()));
    result.set("type", "keyUp");
    result.set("key", String(*name));
  } else if (auto text = std::get_if<TextInputEvent>(&event)) {
    result.set("type", "textInput");
    result.set("text", text->text);
  } else if (std::holds_alternative<MouseMoveEvent>(event)) {
    result.set("type", "move");
  } else if (auto down = std::get_if<MouseButtonDownEvent>(&event)) {
    result.set("type", "buttonDown");
    result.set("button", static_cast<int>(down->mouseButton));
  } else if (auto up = std::get_if<MouseButtonUpEvent>(&event)) {
    result.set("type", "buttonUp");
    result.set("button", static_cast<int>(up->mouseButton));
  } else if (auto wheel = std::get_if<MouseWheelEvent>(&event)) {
    result.set("type", "wheel");
    result.set("direction", wheel->mouseWheel == MouseWheel::Up ? 1 : -1);
  } else {
    result.set("type", "controller");
  }
  return Json(std::move(result));
}

std::expected<bool, StarException> BaseScriptPane::forwardEvent(InputEvent const& event) {
  return Pane::sendEvent(event);
}

}

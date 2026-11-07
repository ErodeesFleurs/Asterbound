import std;
import star.json;
import star.json_extra;
import star.casting;
import star.input_event;
import star.color;
import star.font;
import star.list;
import star.image_processing;
import star.hash;
import star.data_stream;
import star.string_view;
import star.thread;
import star.directives;
import star.bi_map;
import star.ordered_map;
import star.io_device;
import star.ref_ptr;
import star.logging;
import star.string;
import star.text;
import star.asset_path;
import star.listener;
import star.game_types;
import star.set;
import star.audio;
import star.map;
import star.format;
import star.version;
import star.ordered_set;


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


import star.keybindings_menu;
import star.configuration;
import star.widget_parsing;
import star.gui_reader;
import star.list_widget;
import star.label_widget;
import star.button_group;
import star.button_widget;

namespace Star {

KeybindingsMenu::KeybindingsMenu(GuiContext& guiContext) : Pane(guiContext), m_currentMods(KeyMod::NoMod) {}

std::expected<std::shared_ptr<KeybindingsMenu>, StarException> KeybindingsMenu::create(GuiContext& guiContext) {
  auto menu = std::shared_ptr<KeybindingsMenu>(new KeybindingsMenu(guiContext));
  if (auto result = menu->initializePane(); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = menu->init(); !result)
    return std::unexpected(std::move(result.error()));
  return menu;
}

std::expected<void, StarException> KeybindingsMenu::init() {
  GuiReader reader(context());
  reader.registerCallback("cancel", [this](Widget&) -> std::expected<void, StarException> {
    if (auto result = revert(); !result)
      return std::unexpected(std::move(result.error()));
    dismiss();
    return {};
  });
  reader.registerCallback("accept", [this](Widget&) -> std::expected<void, StarException> {
    if (auto result = apply(); !result)
      return std::unexpected(std::move(result.error()));
    dismiss();
    return {};
  });
  reader.registerCallback("setDefault", [this](Widget&) { return resetDefaults(); });
  auto assets = context().root().assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  auto maxBindingsConfig = (*assets)->json("/interface/windowconfig/keybindingsmenu.config:maxBindings");
  if (!maxBindingsConfig)
    return std::unexpected(std::move(maxBindingsConfig.error()));
  auto maxBindings = maxBindingsConfig->toUInt();
  if (!maxBindings)
    return std::unexpected(std::move(maxBindings.error()));
  m_maxBindings = *maxBindings;
  auto layout = (*assets)->json("/interface/windowconfig/keybindingsmenu.config:paneLayout");
  if (!layout)
    return std::unexpected(std::move(layout.error()));
  if (auto result = reader.construct(*layout, *this); !result)
    return std::unexpected(std::move(result.error()));
  return buildListsFromConfig();
}

std::expected<KeyboardCaptureMode, StarException> KeybindingsMenu::keyboardCaptureMode() const {
  return !m_activeKeybinding.expired() ? KeyboardCaptureMode::KeyEvents : KeyboardCaptureMode::None;
}

std::expected<bool, StarException> KeybindingsMenu::sendEvent(InputEvent const& event) {
  if (!m_visible)
    return false;
  if (auto activeKeybinding = m_activeKeybinding.lock()) {
    if (m_context.actions(event).contains(InterfaceAction::KeybindingClear)) {
      if (auto result = clearActive(); !result)
        return std::unexpected(std::move(result.error()));
      return true;
    }
    if (m_context.actions(event).contains(InterfaceAction::KeybindingCancel)) {
      if (auto result = exitActiveMode(); !result)
        return std::unexpected(std::move(result.error()));
      return true;
    }
    // HACK: I need to pass events only to the trash button first.
    auto parent = activeKeybinding->parent();
    if (!parent) return std::unexpected(GuiException("Active keybinding is detached"));
    auto deleteButton = parent->get().requireChild<ButtonWidget>("deleteBinding");
    if (!deleteButton)
      return std::unexpected(std::move(deleteButton.error()));
    auto handled = (*deleteButton)->sendEvent(event);
    if (!handled)
      return std::unexpected(std::move(handled.error()));
    if (*handled)
      return true;
    if (std::holds_alternative<KeyUpEvent>(event)) {
      auto const& keyUp = std::get<KeyUpEvent>(event);
      if (auto modKey = std::ranges::find(KeyChordMods, keyUp.key, &std::pair<Key, KeyMod>::first);
          modKey != KeyChordMods.end()) {
        m_currentMods &= ~modKey->second;
        if (auto result = setKeybinding(KeyChord{.key = keyUp.key, .mods = m_currentMods}); !result)
          return std::unexpected(std::move(result.error()));
        return true;
      }
    } else if (std::holds_alternative<KeyDownEvent>(event)) {
      auto const& keyDown = std::get<KeyDownEvent>(event);
      auto keyName = enumName<KeyNames>(keyDown.key);
      if (!keyName)
        return std::unexpected(std::move(keyName.error()));
      auto modKey = maybeEnumValue<KeyModNames>(*keyName);
      if (!modKey)
        return std::unexpected(std::move(modKey.error()));
      if (*modKey) {
        m_currentMods |= **modKey;
      } else if (auto result = setKeybinding(KeyChord{.key = keyDown.key, .mods = m_currentMods}); !result) {
        return std::unexpected(std::move(result.error()));
      }
      return true;
    }
  }
  if (m_context.actions(event).contains(InterfaceAction::GuiClose)) {
    dismiss();
    return true;
  }
  return Pane::sendEvent(event);
}

std::expected<void, StarException> KeybindingsMenu::show() {
  auto config = context().root().configuration();
  if (!config)
    return std::unexpected(std::move(config.error()));
  auto bindings = (*config)->get("bindings");
  if (!bindings)
    return std::unexpected(std::move(bindings.error()));
  m_origConfiguration = std::move(*bindings);
  return Pane::show();
}

std::expected<void, StarException> KeybindingsMenu::dismissed() {
  if (auto result = exitActiveMode(); !result)
    return std::unexpected(std::move(result.error()));
  return Pane::dismissed();
}

std::expected<void, StarException> KeybindingsMenu::buildListsFromConfig() {
  auto playerList = requireChild<ListWidget>("categories.tabs.player.scrollArea.keyList");
  if (!playerList)
    return std::unexpected(std::move(playerList.error()));
  m_playerList = std::move(*playerList);
  auto toolBarList = requireChild<ListWidget>("categories.tabs.toolbar.scrollArea.keyList");
  if (!toolBarList)
    return std::unexpected(std::move(toolBarList.error()));
  m_toolBarList = std::move(*toolBarList);
  auto gameList = requireChild<ListWidget>("categories.tabs.game.scrollArea.keyList");
  if (!gameList)
    return std::unexpected(std::move(gameList.error()));
  m_gameList = std::move(*gameList);
  m_activeKeybinding.reset();
  m_childToAction.clear();
  auto doKeybindingsFor = [&](ListWidget& list, Json const& keybinds) -> std::expected<void, StarException> {
    if (auto result = list.clear(); !result)
      return std::unexpected(std::move(result.error()));
    list.registerMemberCallback("activateBinding", [this](Widget& widget) { return activateBinding(widget); });
    list.registerMemberCallback("deleteBinding", [this](Widget&) { return clearActive(); });
    auto config = context().root().configuration();
    if (!config)
      return std::unexpected(std::move(config.error()));
    auto bindings = (*config)->get("bindings");
    if (!bindings)
      return std::unexpected(std::move(bindings.error()));
    auto entries = keybinds.iterateArray();
    if (!entries)
      return std::unexpected(std::move(entries.error()));
    for (auto const& keybind : *entries) {
      auto newListMember = list.addItem();
      if (!newListMember)
        return std::unexpected(std::move(newListMember.error()));
      auto actionString = keybind.getString("action");
      if (!actionString)
        return std::unexpected(std::move(actionString.error()));
      if (!InterfaceActionNames)
        return std::unexpected(InterfaceActionNames.error());
      auto action = InterfaceActionNames->getLeft(*actionString);
      if (!action)
        return std::unexpected(std::move(action.error()));
      List<KeyChord> inputDesc;
      auto loadBindings = [&]() -> std::expected<void, StarException> {
        auto actionBindings = bindings->get(*actionString);
        if (!actionBindings)
          return std::unexpected(std::move(actionBindings.error()));
        auto bindingEntries = actionBindings->iterateArray();
        if (!bindingEntries)
          return std::unexpected(std::move(bindingEntries.error()));
        for (auto const& bindingEntry : *bindingEntries) {
          auto descriptor = inputDescriptorFromJson(bindingEntry);
          if (!descriptor)
            return std::unexpected(std::move(descriptor.error()));
          inputDesc.append(std::move(*descriptor));
        }
        return {};
      };
      if (auto result = loadBindings(); !result) {
        if (auto logged = context().root().logger().warn("Could not load keybinding for {}. {}\n", *actionString, result.error().what()); !logged)
          return std::unexpected(std::move(logged.error()));
      }
      auto boundKeys = (*newListMember)->requireChild<ButtonWidget>("boundKeys");
      if (!boundKeys)
        return std::unexpected(std::move(boundKeys.error()));
      auto actionName = (*newListMember)->requireChild<LabelWidget>("actionName");
      if (!actionName)
        return std::unexpected(std::move(actionName.error()));
      auto label = keybind.getString("label");
      if (!label)
        return std::unexpected(std::move(label.error()));
      if (auto result = (*actionName)->setText(*label); !result)
        return std::unexpected(std::move(result.error()));
      m_childToAction.insert({std::weak_ptr<Widget>(*boundKeys), action->get()});
      StringList descriptions;
      for (auto const& descriptor : inputDesc) {
        auto description = printInputDescriptor(descriptor);
        if (!description)
          return std::unexpected(std::move(description.error()));
        descriptions.append(std::move(*description));
      }
      (*boundKeys)->setText(descriptions.join(", "));
      auto deleteButton = (*newListMember)->requireChild<ButtonWidget>("deleteBinding");
      if (!deleteButton)
        return std::unexpected(std::move(deleteButton.error()));
      if (auto result = (*deleteButton)->hide(); !result)
        return std::unexpected(std::move(result.error()));
    }
    return {};
  };
  auto assets = context().root().assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  for (auto const& category : {std::pair{std::ref(*m_playerList), std::string_view("player")}, std::pair{std::ref(*m_toolBarList), std::string_view("toolbar")}, std::pair{std::ref(*m_gameList), std::string_view("game")}}) {
    auto keybinds = (*assets)->json(strf("/interface/windowconfig/keybindingsmenu.config:keyActions.{}", category.second));
    if (!keybinds)
      return std::unexpected(std::move(keybinds.error()));
    if (auto result = doKeybindingsFor(category.first.get(), *keybinds); !result)
      return std::unexpected(std::move(result.error()));
  }
  return {};
}

std::expected<void, StarException> KeybindingsMenu::activateBinding(Widget& widget) {
  if (auto result = exitActiveMode(); !result)
    return std::unexpected(std::move(result.error()));
  m_activeKeybinding = widget.weak_from_this();
  auto parent = widget.parent();
  if (!parent) return std::unexpected(GuiException("Active keybinding is detached"));
  auto deleteButton = parent->get().requireChild<ButtonWidget>("deleteBinding");
  if (!deleteButton)
    return std::unexpected(std::move(deleteButton.error()));
  if (auto result = (*deleteButton)->show(); !result)
    return std::unexpected(std::move(result.error()));
  auto button = rconvert<ButtonWidget>(widget);
  if (!button)
    return std::unexpected(std::move(button.error()));
  button->get().setHighlighted(true);
  return {};
}

std::expected<void, StarException> KeybindingsMenu::setKeybinding(KeyChord desc) {
  auto activeKeybinding = m_activeKeybinding.lock();
  if (!activeKeybinding)
    return {};
  auto out = inputDescriptorToJson(desc);
  if (!out)
    return std::unexpected(std::move(out.error()));
  auto config = context().root().configuration();
  if (!config)
    return std::unexpected(std::move(config.error()));
  auto base = (*config)->get("bindings");
  if (!base)
    return std::unexpected(std::move(base.error()));
  auto action = m_childToAction.get(m_activeKeybinding);
  if (!action)
    return std::unexpected(std::move(action.error()));
  if (!InterfaceActionNames)
    return std::unexpected(InterfaceActionNames.error());
  auto key = InterfaceActionNames->getRight(action->get());
  if (!key)
    return std::unexpected(std::move(key.error()));
  auto storedBindings = base->get(key->get());
  if (!storedBindings)
    return std::unexpected(std::move(storedBindings.error()));
  auto array = storedBindings->toArray();
  if (!array)
    return std::unexpected(std::move(array.error()));
  auto bindings = OrderedHashSet<Json>::from(*array);
  if (bindings.contains(*out))
    bindings.clear();
  bindings.add(*out);
  if (bindings.size() > m_maxBindings) {
    if (auto result = bindings.removeFirst(); !result)
      return std::unexpected(std::move(result.error()));
  }
  auto updated = base->set(key->get(), JsonArray::from(bindings));
  if (!updated)
    return std::unexpected(std::move(updated.error()));
  if (auto result = (*config)->set("bindings", *updated); !result)
    return std::unexpected(std::move(result.error()));
  auto actionBindings = updated->get(key->get());
  if (!actionBindings)
    return std::unexpected(std::move(actionBindings.error()));
  auto entries = actionBindings->iterateArray();
  if (!entries)
    return std::unexpected(std::move(entries.error()));
  StringList buttonText;
  for (auto const& entry : *entries) {
    auto description = [&]() -> std::expected<String, StarException> {
      auto stored = inputDescriptorFromJson(entry);
      if (!stored)
        return std::unexpected(std::move(stored.error()));
      return printInputDescriptor(*stored);
    }();
    buttonText.push_back(description ? std::move(*description) : String("unknown"));
  }
  auto button = rconvert<ButtonWidget>(*activeKeybinding);
  if (!button)
    return std::unexpected(std::move(button.error()));
  button->get().setText(buttonText.join(", "));
  if (auto result = apply(); !result)
    return std::unexpected(std::move(result.error()));
  return exitActiveMode();
}

std::expected<void, StarException> KeybindingsMenu::clearActive() {
  auto activeKeybinding = m_activeKeybinding.lock();
  if (!activeKeybinding)
    return {};
  auto config = context().root().configuration();
  if (!config)
    return std::unexpected(std::move(config.error()));
  auto bindings = (*config)->get("bindings");
  if (!bindings)
    return std::unexpected(std::move(bindings.error()));
  auto base = bindings->toObject();
  if (!base)
    return std::unexpected(std::move(base.error()));
  auto action = m_childToAction.get(m_activeKeybinding);
  if (!action)
    return std::unexpected(std::move(action.error()));
  if (!InterfaceActionNames)
    return std::unexpected(InterfaceActionNames.error());
  auto key = InterfaceActionNames->getRight(action->get());
  if (!key)
    return std::unexpected(std::move(key.error()));
  (*base)[key->get()] = JsonArray{};
  if (auto result = (*config)->set("bindings", *base); !result)
    return std::unexpected(std::move(result.error()));
  auto button = rconvert<ButtonWidget>(*activeKeybinding);
  if (!button)
    return std::unexpected(std::move(button.error()));
  button->get().setText("<Unbound>");
  if (auto result = apply(); !result)
    return std::unexpected(std::move(result.error()));
  return exitActiveMode();
}

std::expected<void, StarException> KeybindingsMenu::exitActiveMode() {
  auto activeKeybinding = m_activeKeybinding.lock();
  if (!activeKeybinding)
    return {};
  auto parent = activeKeybinding->parent();
  if (!parent) return std::unexpected(GuiException("Active keybinding is detached"));
  auto deleteButton = parent->get().requireChild<ButtonWidget>("deleteBinding");
  if (!deleteButton)
    return std::unexpected(std::move(deleteButton.error()));
  if (auto result = (*deleteButton)->hide(); !result)
    return std::unexpected(std::move(result.error()));
  auto button = rconvert<ButtonWidget>(*activeKeybinding);
  if (!button)
    return std::unexpected(std::move(button.error()));
  button->get().setHighlighted(false);
  m_activeKeybinding.reset();
  m_currentMods = KeyMod::NoMod;
  return {};
}

std::expected<void, StarException> KeybindingsMenu::apply() {
  return m_context.refreshKeybindings();
}

std::expected<void, StarException> KeybindingsMenu::revert() {
  auto config = context().root().configuration();
  if (!config)
    return std::unexpected(std::move(config.error()));
  if (auto result = (*config)->set("bindings", m_origConfiguration); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = apply(); !result)
    return std::unexpected(std::move(result.error()));
  return buildListsFromConfig();
}

std::expected<void, StarException> KeybindingsMenu::resetDefaults() {
  auto config = context().root().configuration();
  if (!config)
    return std::unexpected(std::move(config.error()));
  auto bindings = (*config)->getDefault("bindings");
  if (!bindings)
    return std::unexpected(std::move(bindings.error()));
  if (auto result = (*config)->set("bindings", *bindings); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = apply(); !result)
    return std::unexpected(std::move(result.error()));
  return buildListsFromConfig();
}

}

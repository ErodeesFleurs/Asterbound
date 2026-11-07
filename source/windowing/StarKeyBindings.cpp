import std;
import star.json;
import star.json_extra;
import star.input_event;
import star.set;
import star.ordered_map;
import star.map;
import star.string;
import star.bi_map;
import star.io_device;
import star.list;
import star.image_processing;
import star.hash;
import star.data_stream;
import star.string_view;
import star.thread;
import star.directives;
import star.asset_path;
import star.ref_ptr;
import star.listener;
import star.format;
import star.output_proxy;
import star.version;
import star.logging;

import star.key_bindings;
import star.asset_source;
import star.assets;
import star.root_base;
import star.root;
import star.configuration;


namespace Star {

constexpr unsigned KeyChordModMask = [] {
  unsigned mask = 0;
  for (auto const& pair : KeyChordMods)
    mask |= std::to_underlying(pair.second);
  return mask;
}();

extern std::expected<EnumMap<InterfaceAction>, StarException> const InterfaceActionNames = EnumMap<InterfaceAction>::create({
    {InterfaceAction::None, "None"},
    {InterfaceAction::PlayerUp, "PlayerUp"},
    {InterfaceAction::PlayerDown, "PlayerDown"},
    {InterfaceAction::PlayerLeft, "PlayerLeft"},
    {InterfaceAction::PlayerRight, "PlayerRight"},
    {InterfaceAction::PlayerJump, "PlayerJump"},
    {InterfaceAction::PlayerMainItem, "PlayerMainItem"},
    {InterfaceAction::PlayerAltItem, "PlayerAltItem"},
    {InterfaceAction::PlayerDropItem, "PlayerDropItem"},
    {InterfaceAction::PlayerInteract, "PlayerInteract"},
    {InterfaceAction::PlayerShifting, "PlayerShifting"},
    {InterfaceAction::PlayerTechAction1, "PlayerTechAction1"},
    {InterfaceAction::PlayerTechAction2, "PlayerTechAction2"},
    {InterfaceAction::PlayerTechAction3, "PlayerTechAction3"},
    {InterfaceAction::EmoteBlabbering, "EmoteBlabbering"},
    {InterfaceAction::EmoteShouting, "EmoteShouting"},
    {InterfaceAction::EmoteHappy, "EmoteHappy"},
    {InterfaceAction::EmoteSad, "EmoteSad"},
    {InterfaceAction::EmoteNeutral, "EmoteNeutral"},
    {InterfaceAction::EmoteLaugh, "EmoteLaugh"},
    {InterfaceAction::EmoteAnnoyed, "EmoteAnnoyed"},
    {InterfaceAction::EmoteOh, "EmoteOh"},
    {InterfaceAction::EmoteOooh, "EmoteOooh"},
    {InterfaceAction::EmoteBlink, "EmoteBlink"},
    {InterfaceAction::EmoteWink, "EmoteWink"},
    {InterfaceAction::EmoteEat, "EmoteEat"},
    {InterfaceAction::EmoteSleep, "EmoteSleep"},
    {InterfaceAction::ShowLabels, "ShowLabels"},
    {InterfaceAction::CameraShift, "CameraShift"},
    {InterfaceAction::TitleBack, "TitleBack"},
    {InterfaceAction::CinematicSkip, "CinematicSkip"},
    {InterfaceAction::CinematicNext, "CinematicNext"},
    {InterfaceAction::GuiClose, "GuiClose"},
    {InterfaceAction::GuiShifting, "GuiShifting"},
    {InterfaceAction::KeybindingClear, "KeybindingClear"},
    {InterfaceAction::KeybindingCancel, "KeybindingCancel"},
    {InterfaceAction::ChatPageUp, "ChatPageUp"},
    {InterfaceAction::ChatPageDown, "ChatPageDown"},
    {InterfaceAction::ChatPreviousLine, "ChatPreviousLine"},
    {InterfaceAction::ChatNextLine, "ChatNextLine"},
    {InterfaceAction::ChatSendLine, "ChatSendLine"},
    {InterfaceAction::ChatBegin, "ChatBegin"},
    {InterfaceAction::ChatBeginCommand, "ChatBeginCommand"},
    {InterfaceAction::ChatStop, "ChatStop"},
    {InterfaceAction::InterfaceShowHelp, "InterfaceShowHelp"},
    {InterfaceAction::InterfaceHideHud, "InterfaceHideHud"},
    {InterfaceAction::InterfaceChangeBarGroup, "InterfaceChangeBarGroup"},
    {InterfaceAction::InterfaceDeselectHands, "InterfaceDeselectHands"},
    {InterfaceAction::InterfaceBar1, "InterfaceBar1"},
    {InterfaceAction::InterfaceBar2, "InterfaceBar2"},
    {InterfaceAction::InterfaceBar3, "InterfaceBar3"},
    {InterfaceAction::InterfaceBar4, "InterfaceBar4"},
    {InterfaceAction::InterfaceBar5, "InterfaceBar5"},
    {InterfaceAction::InterfaceBar6, "InterfaceBar6"},
    {InterfaceAction::InterfaceBar7, "InterfaceBar7"},
    {InterfaceAction::InterfaceBar8, "InterfaceBar8"},
    {InterfaceAction::InterfaceBar9, "InterfaceBar9"},
    {InterfaceAction::InterfaceBar10, "InterfaceBar10"},
    {InterfaceAction::EssentialBar1, "EssentialBar1"},
    {InterfaceAction::EssentialBar2, "EssentialBar2"},
    {InterfaceAction::EssentialBar3, "EssentialBar3"},
    {InterfaceAction::EssentialBar4, "EssentialBar4"},
    {InterfaceAction::InterfaceRepeatCommand, "InterfaceRepeatCommand"},
    {InterfaceAction::InterfaceToggleFullscreen, "InterfaceToggleFullscreen"},
    {InterfaceAction::InterfaceReload, "InterfaceReload"},
    {InterfaceAction::InterfaceEscapeMenu, "InterfaceEscapeMenu"},
    {InterfaceAction::InterfaceInventory, "InterfaceInventory"},
    {InterfaceAction::InterfaceCodex, "InterfaceCodex"},
    {InterfaceAction::InterfaceQuest, "InterfaceQuest"},
    {InterfaceAction::InterfaceCrafting, "InterfaceCrafting"},
});

std::expected<KeyChord, StarException> inputDescriptorFromJson(Json const& json) {
  Key key;
  auto type = json.getString("type");
  if (!type)
    return std::unexpected(std::move(type.error()));
  if (*type != "key")
    return std::unexpected(StarException::format("Improper bindings type '{}'", *type));
  auto value = json.get("value");
  if (!value)
    return std::unexpected(std::move(value.error()));
  if (value->isType(Json::Type::String)) {
    auto name = value->toString();
    if (!name)
      return std::unexpected(std::move(name.error()));
    auto parsed = enumValue<KeyNames>(*name);
    if (!parsed)
      return std::unexpected(std::move(parsed.error()));
    key = *parsed;
  } else if (value->canConvert(Json::Type::Int)) {
    auto number = value->toUInt();
    if (!number)
      return std::unexpected(std::move(number.error()));
    key = static_cast<Key>(*number);
  } else {
    auto repr = value->repr();
    if (!repr)
      return std::unexpected(std::move(repr.error()));
    return std::unexpected(StarException::format("Improper key value '{}'", *repr));
  }
  auto modsJson = json.get("mods");
  if (!modsJson)
    return std::unexpected(std::move(modsJson.error()));
  auto entries = modsJson->iterateArray();
  if (!entries)
    return std::unexpected(std::move(entries.error()));
  KeyMod mods = KeyMod::NoMod;
  for (auto const& entry : *entries) {
    auto name = entry.toString();
    if (!name)
      return std::unexpected(std::move(name.error()));
    auto parsed = enumValue<KeyModNames>(*name);
    if (!parsed)
      return std::unexpected(std::move(parsed.error()));
    mods |= *parsed;
  }
  return KeyChord{.key = key, .mods = mods};
}

std::expected<Json, StarException> inputDescriptorToJson(KeyChord const& chord) {
  JsonArray modNames;
  for (auto const& p : KeyModNames) {
    if ((chord.mods & p.first) != KeyMod::NoMod)
      modNames.append(String(p.second));
  }
  auto name = enumName<KeyNames>(chord.key);
  if (!name)
    return std::unexpected(std::move(name.error()));
  return JsonObject{{"type", "key"}, {"value", String(*name)}, {"mods", modNames}};
}

std::expected<String, StarException> printInputDescriptor(KeyChord chord) {
  StringList modNames;
  for (auto const& p : KeyModNames) {
    if ((chord.mods & p.first) != KeyMod::NoMod)
      modNames.append(String(p.second));
  }
  auto name = enumName<KeyNames>(chord.key);
  if (!name)
    return std::unexpected(std::move(name.error()));
  return String::joinWith(" + ", modNames.join(" + "), String(*name));
}

KeyBindings::KeyBindings() = default;

std::expected<KeyBindings, StarException> KeyBindings::create(Root& root, Json const& json) {
  if (!InterfaceActionNames)
    return std::unexpected(InterfaceActionNames.error().withContext("Could not set keybindings from configuration"));
  KeyBindings bindings;
  auto entries = json.iterateObject();
  if (!entries)
    return std::unexpected(entries.error().withContext("Could not set keybindings from configuration"));
  for (auto const& entry : *entries) {
    auto action = InterfaceActionNames->getLeft(entry.first);
    if (!action)
      return std::unexpected(action.error().withContext("Could not set keybindings from configuration"));
    auto inputs = entry.second.iterateArray();
    if (!inputs)
      return std::unexpected(inputs.error().withContext("Could not set keybindings from configuration"));
    for (auto const& input : *inputs) {
      auto chord = inputDescriptorFromJson(input);
      if (!chord) {
        auto logged = root.logger().warn("Could not load keybinding for {}: {}\n",
            entry.first, outputException(ExceptionDiagnostic(chord.error()), false));
        if (!logged)
          return std::unexpected(std::move(logged.error()));
        continue;
      }
      bindings.m_actions[chord->key].append({chord->mods, action->get()});
    }
  }
  return bindings;
}

Set<InterfaceAction> KeyBindings::actions(Key key) const {
  return actions(KeyChord{.key = key, .mods = KeyMod::NoMod});
}

Set<InterfaceAction> KeyBindings::actions(InputEvent const& event) const {
  if (std::holds_alternative<KeyDownEvent>(event))
    return actions(KeyChord{.key = std::get<KeyDownEvent>(event).key, .mods = std::get<KeyDownEvent>(event).mods});
  return {};
}

Set<InterfaceAction> KeyBindings::actions(KeyChord chord) const {
  std::size_t mostMatchedMods = 0;
  Set<InterfaceAction> matching;
  auto bindings = m_actions.maybeRef(chord.key);
  if (!bindings)
    return matching;
  for (auto const& pair : bindings->get()) {
    // first make sure that all required mods for the binding are held
    if ((pair.first & chord.mods) == pair.first) {
      // now count the number of mods in the binding
      auto matchedMods = static_cast<std::size_t>(
          std::popcount(static_cast<unsigned>(std::to_underlying(pair.first)) & KeyChordModMask));

      if (matchedMods > mostMatchedMods) {
        matching.clear();
        mostMatchedMods = matchedMods;
      }

      // only activate the binding(s) with the most mods
      if (matchedMods == mostMatchedMods)
        matching.add(pair.second);
    }
  }
  return matching;
}

Set<InterfaceAction> KeyBindings::actionsForKey(Key key) const {
  Set<InterfaceAction> matching;
  if (auto bindings = m_actions.maybeRef(key)) {
    for (auto const& pair : bindings->get())
      matching.add(pair.second);
  }
  return matching;
}

}

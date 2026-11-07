import star.json;
import star.json_extra;
import star.thread;
import star.data_stream;
import star.game_types;
import star.ordered_map;
import star.bi_map;
import star.io_device;
import star.list;
import star.image_processing;
import star.hash;
import star.string_view;
import star.directives;
import star.asset_path;
import star.ref_ptr;
import star.logging;
import star.listener;
import std;
import star.string;
import star.format;
import star.version;

import star.uuid;
import star.asset_source;
import star.assets;
import star.configuration;
import star.root;

import star.universe_settings;

import star.algorithm;
namespace Star {

std::expected<UniverseFlagAction, StarException> parseUniverseFlagAction(Json const& json) {
  auto actionType = json.getString("type");
  if (!actionType)
    return std::unexpected(std::move(actionType.error()));
  if (*actionType != "placeDungeon")
    return std::unexpected(StarException::format("Unsupported universe flag action type {}", *actionType));
  auto dungeon = json.getString("dungeonId");
  if (!dungeon)
    return std::unexpected(std::move(dungeon.error()));
  auto instance = json.getString("targetInstance");
  if (!instance)
    return std::unexpected(std::move(instance.error()));
  auto positionData = json.get("targetPosition");
  if (!positionData)
    return std::unexpected(std::move(positionData.error()));
  auto position = jsonToVec2I(*positionData);
  if (!position)
    return std::unexpected(std::move(position.error()));
  return PlaceDungeonFlagAction{std::move(*dungeon), std::move(*instance), *position};
}

UniverseSettings::UniverseSettings(Root& root) : m_root(root), m_uuid(root.random()) {}

std::expected<std::shared_ptr<UniverseSettings>, StarException> UniverseSettings::create(Root& root) {
  auto settings = std::shared_ptr<UniverseSettings>(new UniverseSettings(root));
  auto loaded = settings->loadFlagActions();
  if (!loaded)
    return std::unexpected(std::move(loaded.error()));
  return settings;
}

std::expected<std::shared_ptr<UniverseSettings>, StarException> UniverseSettings::create(Root& root, Json const& json) {
  auto settings = std::shared_ptr<UniverseSettings>(new UniverseSettings(root));
  auto uuidText = json.getString("uuid");
  if (!uuidText)
    return std::unexpected(std::move(uuidText.error()));
  auto uuid = Uuid::create(*uuidText);
  if (!uuid)
    return std::unexpected(std::move(uuid.error()));
  auto flagData = json.get("flags");
  if (!flagData)
    return std::unexpected(std::move(flagData.error()));
  auto flags = jsonToStringSet(*flagData);
  if (!flags)
    return std::unexpected(std::move(flags.error()));
  settings->m_uuid = std::move(*uuid);
  settings->m_flags = std::move(*flags);
  auto loaded = settings->loadFlagActions();
  if (!loaded)
    return std::unexpected(std::move(loaded.error()));
  return settings;
}

Json UniverseSettings::toJson() const {
  std::lock_guard locker(m_lock);

  return JsonObject{
      {"uuid", m_uuid.hex()},
      {"flags", jsonFromStringSet(m_flags)}
    };
}

Uuid UniverseSettings::uuid() const {
  return m_uuid;
}

StringSet UniverseSettings::flags() const {
  std::lock_guard locker(m_lock);

  return m_flags;
}

std::expected<void, StarException> UniverseSettings::setFlag(String const& flag) {
  std::lock_guard locker(m_lock);

  if (m_flags.add(flag)) {
    auto logged = m_root.logger().info("Universe flags set to {}", m_flags);
    if (!logged)
      return std::unexpected(std::move(logged.error()));
    if (auto flagActions = m_flagActions.maybe(flag)) {
      m_pendingFlagActions.appendAll(*flagActions);
    } else {
      auto logged = m_root.logger().info("No actions configured for universe flag {}", flag);
      if (!logged)
        return std::unexpected(std::move(logged.error()));
    }
  }
  return {};
}

std::optional<List<UniverseFlagAction>> UniverseSettings::pullPendingFlagActions() {
  if (!m_pendingFlagActions.empty())
    return std::exchange(m_pendingFlagActions, {});
  return {};
}

List<UniverseFlagAction> UniverseSettings::currentFlagActions() const {
  List<UniverseFlagAction> res;
  for (String const& flag : m_flags) {
    if (auto flagActions = m_flagActions.maybe(flag))
      res.appendAll(*flagActions);
  }
  return res;
}

List<UniverseFlagAction> UniverseSettings::currentFlagActionsForInstanceWorld(String const& instanceName) const {
  List<UniverseFlagAction> res;
  for (String const& flag : m_flags) {
    for (auto flagAction : m_flagActions.maybe(flag).value_or(List<UniverseFlagAction>{})) {
      if (std::holds_alternative<PlaceDungeonFlagAction>(flagAction) && std::get<PlaceDungeonFlagAction>(flagAction).targetInstance == instanceName)
        res.append(flagAction);
    }
  }
  return res;
}

void UniverseSettings::resetFlags() {
  m_flags.clear();
}

std::expected<void, StarException> UniverseSettings::loadFlagActions() {
  auto assets = m_root.assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  auto config = (*assets)->json("/universeflags.config");
  if (!config)
    return std::unexpected(std::move(config.error()));
  auto entries = config->iterateObject();
  if (!entries)
    return std::unexpected(std::move(entries.error()));
  StringMap<List<UniverseFlagAction>> actionsByFlag;
  for (auto const& entry : *entries) {
    auto actionsData = entry.second.get("actions");
    if (!actionsData)
      return std::unexpected(std::move(actionsData.error()));
    auto actionArray = actionsData->iterateArray();
    if (!actionArray)
      return std::unexpected(std::move(actionArray.error()));
    List<UniverseFlagAction> actions;
    for (auto const& actionConfig : *actionArray) {
      auto action = parseUniverseFlagAction(actionConfig);
      if (!action)
        return std::unexpected(std::move(action.error()));
      actions.append(std::move(*action));
    }
    actionsByFlag[entry.first] = std::move(actions);
  }
  m_flagActions = std::move(actionsByFlag);
  return {};
}

}

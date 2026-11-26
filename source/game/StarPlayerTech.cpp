import star.format;
import star.json;
import star.json_extra;
import star.bi_map;
import star.game_types;
import star.logging;
import star.ordered_map;
import star.thread;
import star.io_device;
import star.list;
import star.image_processing;
import star.hash;
import star.data_stream;
import star.string_view;
import star.directives;
import star.asset_path;
import star.ref_ptr;
import star.listener;
import std;
import star.string;
import star.map;

import star.asset_source;
import star.assets;
import star.root_base;
import star.configuration;
import star.root;

import star.tech_database;
import star.player_tech;

namespace Star {

PlayerTech::PlayerTech(Root& root) : m_root(root) {}

std::expected<std::shared_ptr<PlayerTech>, StarException> PlayerTech::create(Root& root, Json const& json) {
  auto playerTech = std::make_shared<PlayerTech>(root);
  auto availableJson = json.get("availableTechs");
  if (!availableJson)
    return std::unexpected(std::move(availableJson.error()));
  auto available = json_to_string_set(*availableJson);
  if (!available)
    return std::unexpected(std::move(available.error()));
  playerTech->m_availableTechs = std::move(*available);
  auto enabledJson = json.get("enabledTechs");
  if (!enabledJson)
    return std::unexpected(std::move(enabledJson.error()));
  auto enabled = json_to_string_set(*enabledJson);
  if (!enabled)
    return std::unexpected(std::move(enabled.error()));
  playerTech->m_enabledTechs = std::move(*enabled);
  auto techDatabase = root.techDatabase();
  if (!techDatabase)
    return std::unexpected(std::move(techDatabase.error()));
  auto equipped = json.get_object("equippedTechs");
  if (!equipped)
    return std::unexpected(std::move(equipped.error()));
  if (!TechTypeNames)
    return std::unexpected(TechTypeNames.error());
  for (auto const& entry : *equipped) {
    auto techName = entry.second.to_string();
    if (!techName)
      return std::unexpected(std::move(techName.error()));
    if ((*techDatabase)->contains(*techName)) {
      auto type = TechTypeNames->get_left(entry.first);
      if (!type)
        return std::unexpected(std::move(type.error()));
      playerTech->m_equipped_techs[type->get()] = std::move(*techName);
    } else {
      auto logged = root.logger().warn("Unequipping unknown tech '{}' from slot '{}'", *techName, entry.first);
      if (!logged)
        return std::unexpected(std::move(logged.error()));
    }
  }
  return playerTech;
}

std::expected<Json, StarException> PlayerTech::toJson() const {
  if (!TechTypeNames)
    return std::unexpected(TechTypeNames.error());
  JsonObject equipped;
  for (auto const& entry : m_equipped_techs) {
    auto type = TechTypeNames->get_right(entry.first);
    if (!type)
      return std::unexpected(std::move(type.error()));
    equipped[type->get()] = entry.second;
  }
  return JsonObject{
      {"availableTechs", json_from_string_set(m_availableTechs)},
      {"enabledTechs", json_from_string_set(m_enabledTechs)},
      {"equippedTechs", std::move(equipped)}};
}

bool PlayerTech::isAvailable(String const& techModule) const {
  return m_availableTechs.contains(techModule);
}

void PlayerTech::makeAvailable(String const& techModule) {
  m_availableTechs.add(techModule);
}

void PlayerTech::makeUnavailable(String const& techModule) {
  disable(techModule);
  m_availableTechs.remove(techModule);
}

bool PlayerTech::isEnabled(String const& techModule) const {
  return m_enabledTechs.contains(techModule);
}

std::expected<void, StarException> PlayerTech::enable(String const& techModule) {
  if (!m_availableTechs.contains(techModule))
    return std::unexpected(PlayerTechException::format("Enabling tech module '{}' when not available", techModule));
  m_enabledTechs.add(techModule);
  return {};
}

void PlayerTech::disable(String const& techModule) {
  unequip(techModule);
  m_enabledTechs.remove(techModule);
}

bool PlayerTech::isEquipped(String const& techModule) const {
  for (auto const& entry : m_equipped_techs) {
    if (entry.second == techModule)
      return true;
  }
  return false;
}

std::expected<void, StarException> PlayerTech::equip(String const& techModule) {
  if (!m_enabledTechs.contains(techModule))
    return std::unexpected(PlayerTechException::format("Equipping tech module '{}' when not enabled", techModule));
  auto techDatabase = m_root.techDatabase();
  if (!techDatabase)
    return std::unexpected(std::move(techDatabase.error()));
  auto tech = (*techDatabase)->tech(techModule);
  if (!tech)
    return std::unexpected(std::move(tech.error()));
  m_equipped_techs[tech->type] = techModule;
  return {};
}

void PlayerTech::unequip(String const& techModule) {
  for (auto entry = m_equipped_techs.begin(); entry != m_equipped_techs.end();) {
    if (entry->second == techModule)
      entry = m_equipped_techs.erase(entry);
    else
      ++entry;
  }
}

StringSet const& PlayerTech::availableTechs() const {
  return m_availableTechs;
}

StringSet const& PlayerTech::enabledTechs() const {
  return m_enabledTechs;
}

HashMap<TechType, String> const& PlayerTech::equipped_techs() const {
  return m_equipped_techs;
}

StringList PlayerTech::techModules() const {
  return m_equipped_techs.values();
}

}

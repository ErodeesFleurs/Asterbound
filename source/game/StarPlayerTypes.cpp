import star.format;
import std;
import star.string;
import star.json;
import star.json_extra;
import star.bi_map;
import star.either;
import star.player_types;

import star.algorithm;
namespace Star {

extern std::expected<EnumMap<PlayerMode>, StarException> const PlayerModeNames = EnumMap<PlayerMode>::create({
  {PlayerMode::Casual, "casual"},
  {PlayerMode::Survival, "survival"},
  {PlayerMode::Hardcore, "hardcore"}
});

extern std::expected<EnumMap<PlayerBusyState>, StarException> const PlayerBusyStateNames = EnumMap<PlayerBusyState>::create({
  {PlayerBusyState::None, "none"},
  {PlayerBusyState::Chatting, "chatting"},
  {PlayerBusyState::Menu, "menu"}
});

PlayerModeConfig::PlayerModeConfig()
  : hunger(true), allowBeamUpUnderground(false), reviveCostPercentile(0.0f),
    deathDropItemTypes(makeLeft(String("none"))), permadeath(false) {}

std::expected<PlayerModeConfig, StarException> PlayerModeConfig::create(Json config) {
  if (config.isNull())
    config = JsonObject();
  PlayerModeConfig result;
  auto hunger = config.getBool("hunger", true);
  if (!hunger)
    return std::unexpected(std::move(hunger.error()));
  result.hunger = *hunger;
  auto beam = config.getBool("allowBeamUpUnderground", false);
  if (!beam)
    return std::unexpected(std::move(beam.error()));
  result.allowBeamUpUnderground = *beam;
  auto cost = config.getFloat("reviveCostPercentile", 0.0f);
  if (!cost)
    return std::unexpected(std::move(cost.error()));
  result.reviveCostPercentile = *cost;
  auto dropTypes = config.get("deathDropItemTypes", "none");
  if (!dropTypes)
    return std::unexpected(std::move(dropTypes.error()));
  if (dropTypes->isType(Json::Type::Array)) {
    auto types = jsonToStringList(*dropTypes);
    if (!types)
      return std::unexpected(std::move(types.error()));
    result.deathDropItemTypes.setRight(std::move(*types));
  } else {
    auto type = dropTypes->toString();
    if (!type)
      return std::unexpected(std::move(type.error()));
    result.deathDropItemTypes.setLeft(std::move(*type));
  }
  auto permadeath = config.getBool("permadeath", false);
  if (!permadeath)
    return std::unexpected(std::move(permadeath.error()));
  result.permadeath = *permadeath;
  return result;
}

ShipUpgrades::ShipUpgrades()
  : shipLevel(0), maxFuel(0), crewSize(0), fuelEfficiency(1.0f), shipSpeed(0) {}

std::expected<ShipUpgrades, StarException> ShipUpgrades::create(Json config) {
  if (config.isNull())
    config = JsonObject();
  ShipUpgrades result;
  auto level = config.getUInt("shipLevel", 0);
  if (!level)
    return std::unexpected(std::move(level.error()));
  result.shipLevel = *level;
  auto fuel = config.getUInt("maxFuel", 0);
  if (!fuel)
    return std::unexpected(std::move(fuel.error()));
  result.maxFuel = *fuel;
  auto crew = config.getUInt("crewSize", 0);
  if (!crew)
    return std::unexpected(std::move(crew.error()));
  result.crewSize = *crew;
  auto efficiency = config.getFloat("fuelEfficiency", 1.0f);
  if (!efficiency)
    return std::unexpected(std::move(efficiency.error()));
  result.fuelEfficiency = *efficiency;
  auto speed = config.getUInt("shipSpeed", 0);
  if (!speed)
    return std::unexpected(std::move(speed.error()));
  result.shipSpeed = *speed;
  auto capabilitiesJson = config.get("capabilities", JsonArray{});
  if (!capabilitiesJson)
    return std::unexpected(std::move(capabilitiesJson.error()));
  auto capabilities = jsonToStringList(*capabilitiesJson);
  if (!capabilities)
    return std::unexpected(std::move(capabilities.error()));
  result.capabilities.insert_range(*capabilities);
  return result;
}

Json ShipUpgrades::toJson() const {
  JsonArray capabilitiesJson;
  capabilitiesJson.reserve(capabilities.size());
  for (auto const& capability : capabilities)
    capabilitiesJson.push_back(capability);
  return JsonObject{{"shipLevel", shipLevel},
      {"maxFuel", maxFuel},
      {"crewSize", crewSize},
      {"fuelEfficiency", fuelEfficiency},
      {"shipSpeed", shipSpeed},
      {"capabilities", std::move(capabilitiesJson)}};
}

std::expected<std::reference_wrapper<ShipUpgrades>, StarException> ShipUpgrades::apply(Json const& upgrades) {
  auto level = upgrades.optUInt("shipLevel");
  if (!level)
    return std::unexpected(std::move(level.error()));
  shipLevel = std::max(shipLevel, static_cast<unsigned>(level->value_or(shipLevel)));
  auto fuel = upgrades.optUInt("maxFuel");
  if (!fuel)
    return std::unexpected(std::move(fuel.error()));
  maxFuel = fuel->value_or(maxFuel);
  auto crew = upgrades.optUInt("crewSize");
  if (!crew)
    return std::unexpected(std::move(crew.error()));
  crewSize = std::max(crewSize, static_cast<unsigned>(crew->value_or(crewSize)));
  auto efficiency = upgrades.optFloat("fuelEfficiency");
  if (!efficiency)
    return std::unexpected(std::move(efficiency.error()));
  fuelEfficiency = efficiency->value_or(fuelEfficiency);
  auto speed = upgrades.optFloat("shipSpeed");
  if (!speed)
    return std::unexpected(std::move(speed.error()));
  shipSpeed = speed->value_or(shipSpeed);
  auto hasCapabilities = upgrades.contains("capabilities");
  if (!hasCapabilities)
    return std::unexpected(std::move(hasCapabilities.error()));
  if (*hasCapabilities) {
    auto json = upgrades.get("capabilities", JsonArray{});
    if (!json)
      return std::unexpected(std::move(json.error()));
    auto additional = jsonToStringList(*json);
    if (!additional)
      return std::unexpected(std::move(additional.error()));
    capabilities.insert_range(*additional);
  }
  return std::ref(*this);
}

bool ShipUpgrades::operator==(ShipUpgrades const& rhs) const {
  return std::tie(shipLevel, maxFuel, crewSize, fuelEfficiency, shipSpeed, capabilities)
      == std::tie(rhs.shipLevel, rhs.maxFuel, rhs.crewSize, rhs.fuelEfficiency, rhs.shipSpeed, rhs.capabilities);
}

}

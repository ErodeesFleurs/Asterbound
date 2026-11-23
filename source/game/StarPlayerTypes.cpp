import star.format;
import std;
import star.string;
import star.json;
import star.json_extra;
import star.bi_map;
import star.either;
import star.player_types;
import star.data_stream;
import star.data_stream_extra;

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
    deathDropItemTypes(make_left(String("none"))), permadeath(false) {}

std::expected<PlayerModeConfig, StarException> PlayerModeConfig::create(Json config) {
  if (config.is_null())
    config = JsonObject();
  PlayerModeConfig result;
  auto hunger = config.get_bool("hunger", true);
  if (!hunger)
    return std::unexpected(std::move(hunger.error()));
  result.hunger = *hunger;
  auto beam = config.get_bool("allowBeamUpUnderground", false);
  if (!beam)
    return std::unexpected(std::move(beam.error()));
  result.allowBeamUpUnderground = *beam;
  auto cost = config.get_float("reviveCostPercentile", 0.0f);
  if (!cost)
    return std::unexpected(std::move(cost.error()));
  result.reviveCostPercentile = *cost;
  auto dropTypes = config.get("deathDropItemTypes", "none");
  if (!dropTypes)
    return std::unexpected(std::move(dropTypes.error()));
  if (dropTypes->is_type(Json::Type::Array)) {
    auto types = json_to_string_list(*dropTypes);
    if (!types)
      return std::unexpected(std::move(types.error()));
    result.deathDropItemTypes.set_right(std::move(*types));
  } else {
    auto type = dropTypes->to_string();
    if (!type)
      return std::unexpected(std::move(type.error()));
    result.deathDropItemTypes.set_left(std::move(*type));
  }
  auto permadeath = config.get_bool("permadeath", false);
  if (!permadeath)
    return std::unexpected(std::move(permadeath.error()));
  result.permadeath = *permadeath;
  return result;
}

static std::expected<void, StarException> add_ship_capabilities(StringSet& target, Json const& config) {
  auto entry = config.find("capabilities");
  if (!entry)
    return std::unexpected(std::move(entry.error()));
  if (*entry) {
    auto capabilities = json_to_string_list((*entry)->get());
    if (!capabilities)
      return std::unexpected(std::move(capabilities.error()));
    for (auto& capability : *capabilities)
      target.insert(std::move(capability));
  }
  return {};
}

std::expected<ShipUpgrades, StarException> ShipUpgrades::create(Json const& config) {
  if (config.is_null())
    return ShipUpgrades();
  ShipUpgrades result;
  auto level = config.get_uint("shipLevel", 0);
  if (!level)
    return std::unexpected(std::move(level.error()));
  result.ship_level = *level;
  auto fuel = config.get_uint("maxFuel", 0);
  if (!fuel)
    return std::unexpected(std::move(fuel.error()));
  result.max_fuel = *fuel;
  auto crew = config.get_uint("crewSize", 0);
  if (!crew)
    return std::unexpected(std::move(crew.error()));
  result.crew_size = *crew;
  auto efficiency = config.get_float("fuelEfficiency", 1.0f);
  if (!efficiency)
    return std::unexpected(std::move(efficiency.error()));
  result.fuel_efficiency = *efficiency;
  auto speed = config.get_uint("shipSpeed", 0);
  if (!speed)
    return std::unexpected(std::move(speed.error()));
  result.ship_speed = *speed;
  if (auto added = add_ship_capabilities(result.capabilities, config); !added)
    return std::unexpected(std::move(added.error()));
  return result;
}

Json ShipUpgrades::to_json() const {
  JsonArray capabilities_json;
  capabilities_json.reserve(capabilities.size());
  for (auto const& capability : capabilities)
    capabilities_json.push_back(capability);
  return JsonObject{{"shipLevel", ship_level},
      {"maxFuel", max_fuel},
      {"crewSize", crew_size},
      {"fuelEfficiency", fuel_efficiency},
      {"shipSpeed", ship_speed},
      {"capabilities", std::move(capabilities_json)}};
}

std::expected<std::reference_wrapper<ShipUpgrades>, StarException> ShipUpgrades::apply(Json const& upgrades) {
  auto level = upgrades.opt_uint("shipLevel");
  if (!level)
    return std::unexpected(std::move(level.error()));
  auto fuel = upgrades.opt_uint("maxFuel");
  if (!fuel)
    return std::unexpected(std::move(fuel.error()));
  auto crew = upgrades.opt_uint("crewSize");
  if (!crew)
    return std::unexpected(std::move(crew.error()));
  auto efficiency = upgrades.opt_float("fuelEfficiency");
  if (!efficiency)
    return std::unexpected(std::move(efficiency.error()));
  auto speed = upgrades.opt_float("shipSpeed");
  if (!speed)
    return std::unexpected(std::move(speed.error()));
  if (auto added = add_ship_capabilities(capabilities, upgrades); !added)
    return std::unexpected(std::move(added.error()));
  ship_level = std::max(ship_level, static_cast<unsigned>(level->value_or(ship_level)));
  max_fuel = fuel->value_or(max_fuel);
  crew_size = std::max(crew_size, static_cast<unsigned>(crew->value_or(crew_size)));
  fuel_efficiency = efficiency->value_or(fuel_efficiency);
  ship_speed = speed->value_or(ship_speed);
  return std::ref(*this);
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, ShipUpgrades& upgrades) {
  ShipUpgrades decoded;
  auto result = ds >> decoded.ship_level >> decoded.max_fuel >> decoded.crew_size
      >> decoded.fuel_efficiency >> decoded.ship_speed >> decoded.capabilities;
  if (result)
    upgrades = std::move(decoded);
  return result;
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, ShipUpgrades const& upgrades) {
  return ds << upgrades.ship_level << upgrades.max_fuel << upgrades.crew_size
      << upgrades.fuel_efficiency << upgrades.ship_speed << upgrades.capabilities;
}

}

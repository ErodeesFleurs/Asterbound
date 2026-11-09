import std;
import star.json;
import star.string;
import star.data_stream;
import star.bi_map;
import star.game_types;
import star.damage_types;
import star.format;

namespace Star {

EntityDamageTeam::EntityDamageTeam() : type(TeamType::Null), team(0) {}

EntityDamageTeam::EntityDamageTeam(TeamType type, TeamNumber team) : type(type), team(team) {}

std::expected<EntityDamageTeam, StarException> EntityDamageTeam::create(Json const& json) {
  auto name = json.getString("type");
  if (!name)
    return std::unexpected(std::move(name.error()));
  auto type = enumValue<TeamTypeNames>(*name);
  if (!type)
    return std::unexpected(std::move(type.error()));
  auto team = json.getUInt("team", 0);
  if (!team)
    return std::unexpected(std::move(team.error()));
  return EntityDamageTeam(*type, *team);
}

std::expected<Json, StarException> EntityDamageTeam::toJson() const {
  auto name = enumName<TeamTypeNames>(type);
  if (!name)
    return std::unexpected(std::move(name.error()));
  return JsonObject{{"type", String(*name)}, {"team", team}};
}

bool EntityDamageTeam::canDamage(EntityDamageTeam victim, bool victimIsSelf) const {
  if (victimIsSelf) {
    if (type == TeamType::Indiscriminate)
      return true;
  } else if (type == TeamType::Friendly) {
    if (victim.type == TeamType::Enemy || victim.type == TeamType::Passive || victim.type == TeamType::Environment || victim.type == TeamType::Indiscriminate)
      return true;
  } else if (type == TeamType::Enemy) {
    if (victim.type == TeamType::Friendly || victim.type == TeamType::PVP || victim.type == TeamType::Indiscriminate)
      return true;
    else if (victim.type == TeamType::Enemy && team != victim.team)
      return true;
  } else if (type == TeamType::PVP) {
    if (victim.type == TeamType::Enemy || victim.type == TeamType::Passive || victim.type == TeamType::Environment || victim.type == TeamType::Indiscriminate)
      return true;
    else if (victim.type == TeamType::PVP && (team == 0 || team != victim.team))
      return true;
  } else if (type == TeamType::Passive) {
    // never deal damage
  } else if (type == TeamType::Ghostly) {
    // never deal damage
  } else if (type == TeamType::Environment) {
    if (victim.type == TeamType::Friendly || victim.type == TeamType::PVP || victim.type == TeamType::Indiscriminate)
      return true;
  } else if (type == TeamType::Indiscriminate) {
    if (victim.type == TeamType::Friendly || victim.type == TeamType::Enemy || victim.type == TeamType::PVP
        || victim.type == TeamType::Passive
        || victim.type == TeamType::Environment
        || victim.type == TeamType::Indiscriminate)
      return true;
  } else if (type == TeamType::Assistant) {
    if (victim.type == TeamType::Enemy || victim.type == TeamType::Passive || victim.type == TeamType::Environment || victim.type == TeamType::Indiscriminate)
      return true;
  }

  return false;
}

bool EntityDamageTeam::operator==(EntityDamageTeam const& rhs) const {
  return std::tie(type, team) == std::tie(rhs.type, rhs.team);
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, EntityDamageTeam const& team) {
  auto result = ds << team.type << team.team;
  if (!result)
    return std::unexpected(std::move(result.error()));
  return std::ref(ds);
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, EntityDamageTeam& team) {
  auto result = ds >> team.type >> team.team;
  if (!result)
    return std::unexpected(std::move(result.error()));
  return std::ref(ds);
}

TeamNumber soloPvpTeam(ConnectionId clientId) { return clientId; }
}

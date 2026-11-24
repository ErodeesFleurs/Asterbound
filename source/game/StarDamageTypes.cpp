import std;
import star.json;
import star.string;
import star.data_stream;
import star.bi_map;
import star.game_types;
import star.damage_types;
import star.format;

namespace Star {

EntityDamageTeam::EntityDamageTeam(TeamType type, TeamNumber team) : type(type), team(team) {}

std::expected<EntityDamageTeam, StarException> EntityDamageTeam::create(Json const& json) {
  auto name = json.get_string("type");
  if (!name)
    return std::unexpected(std::move(name.error()));
  auto type = enum_value<TeamTypeNames>(*name);
  if (!type)
    return std::unexpected(std::move(type.error()));
  auto team = json.get_uint("team", 0);
  if (!team)
    return std::unexpected(std::move(team.error()));
  return EntityDamageTeam(*type, *team);
}

std::expected<Json, StarException> EntityDamageTeam::to_json() const {
  auto name = enum_name<TeamTypeNames>(type);
  if (!name)
    return std::unexpected(std::move(name.error()));
  return JsonObject{{"type", String(*name)}, {"team", team}};
}

bool EntityDamageTeam::can_damage(EntityDamageTeam victim, bool victim_is_self) const {
  using enum TeamType;
  if (victim_is_self)
    return type == Indiscriminate;

  bool const allied_targets = victim.type == Enemy || victim.type == Passive
      || victim.type == Environment || victim.type == Indiscriminate;
  bool const environment_targets = victim.type == Friendly || victim.type == PVP || victim.type == Indiscriminate;
  switch (type) {
    case Friendly:
    case Assistant:
      return allied_targets;
    case PVP:
      return allied_targets || (victim.type == PVP && (team == 0 || team != victim.team));
    case Enemy:
      return environment_targets || (victim.type == Enemy && team != victim.team);
    case Environment:
      return environment_targets;
    case Indiscriminate:
      return allied_targets || environment_targets;
    default:
      return false;
  }
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

}

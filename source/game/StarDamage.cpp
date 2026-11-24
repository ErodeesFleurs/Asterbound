import star.json;
import star.json_extra;
import star.vector;
import star.data_stream;
import star.game_types;
import star.poly;
import star.data_stream_extra;
import star.list;
import std;
import star.string;
import star.string_view;
import star.bi_map;
import star.format;

import star.damage_types;
import star.world_geometry;
import star.status_types;
import star.damage;
import star.line;
namespace Star {

namespace {
std::expected<DamageSource::DamageArea, StarException> read_damage_area(Json const& config) {
  // Configuration keys take precedence, including the historical poly-before-line rule.
  auto poly = config.find("poly");
  if (!poly)
    return std::unexpected(std::move(poly.error()));
  if (*poly)
    return json_to_polyf((*poly)->get());
  auto line = config.find("line");
  if (!line)
    return std::unexpected(std::move(line.error()));
  if (*line)
    return json_to_line2f((*line)->get());

  auto area = config.get("damageArea");
  if (!area)
    return std::unexpected(std::move(area.error()));
  auto type = config.opt_string("damageAreaType");
  if (!type)
    return std::unexpected(std::move(type.error()));
  if (area->is_null() && !*type)
    return std::monostate{};

  auto size = area->size();
  if (!size)
    return std::unexpected(std::move(size.error()));
  if (*type && **type == "line") {
    if (*size != 2)
      return std::unexpected(JsonException("DamageSource line requires two endpoints"));
    return json_to_line2f(*area);
  }
  if (*type && **type != "poly")
    return std::unexpected(JsonException("Unknown DamageSource damageAreaType"));
  if (!*type && *size == 2)
    return std::unexpected(JsonException("Ambiguous two-point damageArea: specify damageAreaType 'poly' or 'line'"));
  // Stored polygons already have their native winding. Only config polygons are normalized.
  auto vertices = json_to_list<Vec2F>(*area, json_to_vec2f);
  if (!vertices)
    return std::unexpected(std::move(vertices.error()));
  return PolyF(std::move(*vertices));
}

std::expected<List<EphemeralStatusEffect>, StarException> read_status_effects(Json const& config) {
  auto effects = config.find("statusEffects");
  if (!effects)
    return std::unexpected(std::move(effects.error()));
  if (!*effects || (*effects)->get().is_null())
    return List<EphemeralStatusEffect>{};
  return json_to_list<EphemeralStatusEffect>((*effects)->get(), jsonToEphemeralStatusEffect);
}
}

std::expected<DamageSource, StarException> DamageSource::create(Json const& config) {
  DamageSource source;
  auto dtype = config.opt_string("damageType");
  if (!dtype)
    return std::unexpected(std::move(dtype.error()));
  source.damage_type = DamageType::Damage;
  if (*dtype) {
    auto type = enum_value<DamageTypeNames>(**dtype);
    if (!type)
      return std::unexpected(std::move(type.error()));
    source.damage_type = *type;
  }
  auto area = read_damage_area(config);
  if (!area)
    return std::unexpected(std::move(area.error()));
  source.damage_area = std::move(*area);
  auto damage = config.get_float("damage");
  if (!damage)
    return std::unexpected(std::move(damage.error()));
  source.damage = *damage;
  auto track = config.get_bool("trackSourceEntity", true);
  if (!track)
    return std::unexpected(std::move(track.error()));
  source.track_source_entity = *track;
  auto id = config.get_int("sourceEntityId", null_entity_id);
  if (!id)
    return std::unexpected(std::move(id.error()));
  source.source_entity_id = *id;
  auto team_config = config.opt("team");
  if (!team_config)
    return std::unexpected(std::move(team_config.error()));
  if (*team_config) {
    auto team = EntityDamageTeam::create(**team_config);
    if (!team)
      return std::unexpected(std::move(team.error()));
    source.team = *team;
  } else {
    auto name = config.get_string("teamType", "passive");
    if (!name)
      return std::unexpected(std::move(name.error()));
    auto type = enum_value<TeamTypeNames>(*name);
    if (!type)
      return std::unexpected(std::move(type.error()));
    source.team.type = *type;
    auto number = config.get_uint("teamNumber", 0);
    if (!number)
      return std::unexpected(std::move(number.error()));
    source.team.team = *number;
  }
  auto group = config.opt_string("damageRepeatGroup");
  if (!group)
    return std::unexpected(std::move(group.error()));
  source.damage_repeat_group = std::move(*group);
  auto timeout = config.opt_float("damageRepeatTimeout");
  if (!timeout)
    return std::unexpected(std::move(timeout.error()));
  source.damage_repeat_timeout = *timeout;
  auto kind = config.get_string("damageSourceKind", "");
  if (!kind)
    return std::unexpected(std::move(kind.error()));
  source.damage_source_kind = std::move(*kind);
  auto effects = read_status_effects(config);
  if (!effects)
    return std::unexpected(std::move(effects.error()));
  source.status_effects = std::move(*effects);
  auto knockback = config.get("knockback", Json(0.0f));
  if (!knockback)
    return std::unexpected(std::move(knockback.error()));
  if (knockback->is_type(Json::Type::Array)) {
    auto parsed = json_to_vec2f(*knockback);
    if (!parsed)
      return std::unexpected(std::move(parsed.error()));
    source.knockback = *parsed;
  } else if (!knockback->is_null()) {
    auto parsed = knockback->to_float();
    if (!parsed)
      return std::unexpected(std::move(parsed.error()));
    source.knockback = *parsed;
  }
  auto ray_check = config.get_bool("rayCheck", false);
  if (!ray_check)
    return std::unexpected(std::move(ray_check.error()));
  source.ray_check = *ray_check;
  return source;
}

std::expected<Json, StarException> DamageSource::to_json() const {
  Json area;
  if (std::holds_alternative<PolyF>(damage_area))
    area = json_from_polyf(std::get<PolyF>(damage_area));
  else if (std::holds_alternative<Line2F>(damage_area))
    area = json_from_line2f(std::get<Line2F>(damage_area));

  Json knockback_json;
  if (std::holds_alternative<float>(knockback))
    knockback_json = std::get<float>(knockback);
  else if (std::holds_alternative<Vec2F>(knockback))
    knockback_json = json_from_vec2f(std::get<Vec2F>(knockback));

  auto type_name = enum_name<DamageTypeNames>(damage_type);
  if (!type_name)
    return std::unexpected(std::move(type_name.error()));
  auto team_json = team.to_json();
  if (!team_json)
    return std::unexpected(std::move(team_json.error()));
  JsonObject result{{"damageType", String(*type_name)},
                    {"damageArea", std::move(area)},
                    {"damage", damage},
                    {"trackSourceEntity", track_source_entity},
                    {"sourceEntityId", source_entity_id},
                    {"team", std::move(*team_json)},
                    {"damageRepeatGroup", json_from_optional(damage_repeat_group)},
                    {"damageRepeatTimeout", json_from_optional(damage_repeat_timeout)},
                    {"damageSourceKind", damage_source_kind},
                    {"statusEffects", status_effects.transformed(jsonFromEphemeralStatusEffect)},
                    {"knockback", std::move(knockback_json)},
                    {"rayCheck", ray_check}};
  // Keep the legacy Lua-visible array. Only two-point shapes need a discriminator.
  if (std::holds_alternative<Line2F>(damage_area))
    result["damageAreaType"] = "line";
  else if (std::holds_alternative<PolyF>(damage_area) && std::get<PolyF>(damage_area).sides() == 2)
    result["damageAreaType"] = "poly";
  return result;
}

std::expected<bool, StarException> DamageSource::intersects_with_poly(WorldGeometry const& geometry, PolyF const& target_poly) const {
  if (std::holds_alternative<PolyF>(damage_area))
    return geometry.poly_intersects_poly(std::get<PolyF>(damage_area), target_poly);
  else if (std::holds_alternative<Line2F>(damage_area))
    return geometry.line_intersects_poly(std::get<Line2F>(damage_area), target_poly);
  else
    return false;
}

Vec2F DamageSource::knockback_momentum(WorldGeometry const& world_geometry, Vec2F const& target_center) const {
  if (std::holds_alternative<Vec2F>(knockback)) {
    return std::get<Vec2F>(knockback);
  } else if (std::holds_alternative<float>(knockback)) {
    float strength = std::get<float>(knockback);
    if (strength != 0) {
      if (std::holds_alternative<PolyF>(damage_area))
        return world_geometry.diff(target_center, std::get<PolyF>(damage_area).center()).normalized() * strength;
      else if (std::holds_alternative<Line2F>(damage_area))
        return vnorm(std::get<Line2F>(damage_area).diff()) * strength;
    }
  }

  return Vec2F();
}

DamageSource& DamageSource::translate(Vec2F const& position) {
  if (std::holds_alternative<PolyF>(damage_area))
    std::get<PolyF>(damage_area).translate(position);
  else if (std::holds_alternative<Line2F>(damage_area))
    std::get<Line2F>(damage_area).translate(position);
  return *this;
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, DamageSource const& source) {
  return ds << source.damage_type << source.damage_area << source.damage
      << source.track_source_entity << source.source_entity_id << source.team
      << source.damage_repeat_group << source.damage_repeat_timeout << source.damage_source_kind
      << source.status_effects << source.knockback << source.ray_check;
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, DamageSource& source) {
  return ds >> source.damage_type >> source.damage_area >> source.damage
      >> source.track_source_entity >> source.source_entity_id >> source.team
      >> source.damage_repeat_group >> source.damage_repeat_timeout >> source.damage_source_kind
      >> source.status_effects >> source.knockback >> source.ray_check;
}

std::expected<DamageRequest, StarException> DamageRequest::create(Json const& v) {
  DamageRequest request;
  auto hit_name = v.get_string("hitType", "hit");
  if (!hit_name)
    return std::unexpected(std::move(hit_name.error()));
  auto hit = enum_value<HitTypeNames>(*hit_name);
  if (!hit)
    return std::unexpected(std::move(hit.error()));
  request.hit_type = *hit;
  auto type_name = v.get_string("damageType", "damage");
  if (!type_name)
    return std::unexpected(std::move(type_name.error()));
  auto type = enum_value<DamageTypeNames>(*type_name);
  if (!type)
    return std::unexpected(std::move(type.error()));
  request.damage_type = *type;
  auto damage = v.get_float("damage");
  if (!damage)
    return std::unexpected(std::move(damage.error()));
  request.damage = *damage;
  auto momentum_json = v.get("knockbackMomentum", JsonArray{0, 0});
  if (!momentum_json)
    return std::unexpected(std::move(momentum_json.error()));
  auto momentum = json_to_vec2f(*momentum_json);
  if (!momentum)
    return std::unexpected(std::move(momentum.error()));
  request.knockback_momentum = *momentum;
  auto id = v.get_int("sourceEntityId", null_entity_id);
  if (!id)
    return std::unexpected(std::move(id.error()));
  request.source_entity_id = *id;
  auto kind = v.get_string("damageSourceKind", "");
  if (!kind)
    return std::unexpected(std::move(kind.error()));
  request.damage_source_kind = std::move(*kind);
  auto effects = read_status_effects(v);
  if (!effects)
    return std::unexpected(std::move(effects.error()));
  request.status_effects = std::move(*effects);
  return request;
}

std::expected<Json, StarException> DamageRequest::to_json() const {
  auto hit = enum_name<HitTypeNames>(hit_type);
  if (!hit)
    return std::unexpected(std::move(hit.error()));
  auto type = enum_name<DamageTypeNames>(damage_type);
  if (!type)
    return std::unexpected(std::move(type.error()));
  return JsonObject{{"hitType", String(*hit)},
                    {"damageType", String(*type)},
                    {"damage", damage},
                    {"knockbackMomentum", json_from_vec2f(knockback_momentum)},
                    {"sourceEntityId", source_entity_id},
                    {"damageSourceKind", damage_source_kind},
                    {"statusEffects", status_effects.transformed(jsonFromEphemeralStatusEffect)}};
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, DamageRequest const& request) {
  return ds << request.hit_type << request.damage_type << request.damage
      << request.knockback_momentum << request.source_entity_id << request.damage_source_kind << request.status_effects;
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, DamageRequest& request) {
  return ds >> request.hit_type >> request.damage_type >> request.damage
      >> request.knockback_momentum >> request.source_entity_id >> request.damage_source_kind >> request.status_effects;
}

std::expected<DamageNotification, StarException> DamageNotification::create(Json const& v) {
  DamageNotification notification;
  auto source = v.get_int("sourceEntityId");
  if (!source)
    return std::unexpected(std::move(source.error()));
  notification.source_entity_id = *source;
  auto target = v.get_int("targetEntityId");
  if (!target)
    return std::unexpected(std::move(target.error()));
  notification.target_entity_id = *target;
  auto position_json = v.get("position");
  if (!position_json)
    return std::unexpected(std::move(position_json.error()));
  auto position = json_to_vec2f(*position_json);
  if (!position)
    return std::unexpected(std::move(position.error()));
  notification.position = *position;
  auto damage = v.get_float("damageDealt");
  if (!damage)
    return std::unexpected(std::move(damage.error()));
  notification.damage_dealt = *damage;
  auto health = v.get_float("healthLost");
  if (!health)
    return std::unexpected(std::move(health.error()));
  notification.health_lost = *health;
  auto hit_name = v.get_string("hitType");
  if (!hit_name)
    return std::unexpected(std::move(hit_name.error()));
  auto hit = enum_value<HitTypeNames>(*hit_name);
  if (!hit)
    return std::unexpected(std::move(hit.error()));
  notification.hit_type = *hit;
  auto source_kind = v.get_string("damageSourceKind");
  if (!source_kind)
    return std::unexpected(std::move(source_kind.error()));
  notification.damage_source_kind = std::move(*source_kind);
  auto material = v.get_string("targetMaterialKind");
  if (!material)
    return std::unexpected(std::move(material.error()));
  notification.target_material_kind = std::move(*material);
  return notification;
}

std::expected<Json, StarException> DamageNotification::to_json() const {
  auto hit = enum_name<HitTypeNames>(hit_type);
  if (!hit)
    return std::unexpected(std::move(hit.error()));
  return JsonObject{{"sourceEntityId", source_entity_id},
                    {"targetEntityId", target_entity_id},
                    {"position", json_from_vec2f(position)},
                    {"damageDealt", damage_dealt},
                    {"healthLost", health_lost},
                    {"hitType", String(*hit)},
                    {"damageSourceKind", damage_source_kind},
                    {"targetMaterialKind", target_material_kind}};
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, DamageNotification const& notification) {
  auto source = ds.viwrite(notification.source_entity_id);
  if (!source) return std::unexpected(std::move(source.error()));
  auto target = ds.viwrite(notification.target_entity_id);
  if (!target) return std::unexpected(std::move(target.error()));
  auto x = ds.vfwrite(notification.position[0], 0.01f);
  if (!x) return std::unexpected(std::move(x.error()));
  auto y = ds.vfwrite(notification.position[1], 0.01f);
  if (!y) return std::unexpected(std::move(y.error()));
  return ds << notification.damage_dealt << notification.health_lost << notification.hit_type
      << notification.damage_source_kind << notification.target_material_kind;
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, DamageNotification& notification) {
  auto source = ds.viread(notification.source_entity_id);
  if (!source) return std::unexpected(std::move(source.error()));
  auto target = ds.viread(notification.target_entity_id);
  if (!target) return std::unexpected(std::move(target.error()));
  auto x = ds.vfread(notification.position[0], 0.01f);
  if (!x) return std::unexpected(std::move(x.error()));
  auto y = ds.vfread(notification.position[1], 0.01f);
  if (!y) return std::unexpected(std::move(y.error()));
  return ds >> notification.damage_dealt >> notification.health_lost >> notification.hit_type
      >> notification.damage_source_kind >> notification.target_material_kind;
}

}

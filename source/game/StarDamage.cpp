import star.json;
import star.json_extra;
import star.vector;
import star.data_stream;
import star.game_types;
import star.poly;
import star.strong_typedef;
import star.id_map;
import star.data_stream_extra;
import star.ordered_map;
import star.bi_map;
import star.thread;
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

import star.damage_types;
import star.world_geometry;
import star.status_types;
import star.damage;
import star.asset_source;
import star.assets;
import star.root_base;
import star.configuration;
import star.root;

import star.line;
namespace Star {

DamageSource::DamageSource()
  : damageType(DamageType::NoDamage), damage(0.0), sourceEntityId(NullEntityId), rayCheck(false) {}

std::expected<DamageSource, StarException> DamageSource::create(Json const& config) {
  DamageSource source;
  auto dtype = config.optString("damageType");
  if (!dtype)
    return std::unexpected(std::move(dtype.error()));
  source.damageType = DamageType::Damage;
  if (*dtype) {
    auto type = enumValue<DamageTypeNames>(**dtype);
    if (!type)
      return std::unexpected(std::move(type.error()));
    source.damageType = *type;
  }
  auto hasPoly = config.contains("poly");
  if (!hasPoly)
    return std::unexpected(std::move(hasPoly.error()));
  auto hasLine = config.contains("line");
  if (!hasLine)
    return std::unexpected(std::move(hasLine.error()));
  if (*hasPoly) {
    auto value = config.get("poly");
    if (!value)
      return std::unexpected(std::move(value.error()));
    auto poly = jsonToPolyF(*value);
    if (!poly)
      return std::unexpected(std::move(poly.error()));
    source.damageArea = std::move(*poly);
  } else if (*hasLine) {
    auto value = config.get("line");
    if (!value)
      return std::unexpected(std::move(value.error()));
    auto line = jsonToLine2F(*value);
    if (!line)
      return std::unexpected(std::move(line.error()));
    source.damageArea = std::move(*line);
  } else {
    return std::unexpected(StarException("No 'poly' or 'line' key in DamageSource config"));
  }
  auto damage = config.getFloat("damage");
  if (!damage)
    return std::unexpected(std::move(damage.error()));
  source.damage = *damage;
  auto track = config.getBool("trackSourceEntity", true);
  if (!track)
    return std::unexpected(std::move(track.error()));
  source.trackSourceEntity = *track;
  auto id = config.getInt("sourceEntityId", NullEntityId);
  if (!id)
    return std::unexpected(std::move(id.error()));
  source.sourceEntityId = *id;
  auto teamConfig = config.opt("team");
  if (!teamConfig)
    return std::unexpected(std::move(teamConfig.error()));
  if (*teamConfig) {
    auto team = EntityDamageTeam::create(**teamConfig);
    if (!team)
      return std::unexpected(std::move(team.error()));
    source.team = *team;
  } else {
    auto name = config.getString("teamType", "passive");
    if (!name)
      return std::unexpected(std::move(name.error()));
    auto type = enumValue<TeamTypeNames>(*name);
    if (!type)
      return std::unexpected(std::move(type.error()));
    source.team.type = *type;
    auto number = config.getUInt("teamNumber", 0);
    if (!number)
      return std::unexpected(std::move(number.error()));
    source.team.team = *number;
  }
  auto group = config.optString("damageRepeatGroup");
  if (!group)
    return std::unexpected(std::move(group.error()));
  source.damageRepeatGroup = std::move(*group);
  auto timeout = config.optFloat("damageRepeatTimeout");
  if (!timeout)
    return std::unexpected(std::move(timeout.error()));
  source.damageRepeatTimeout = *timeout;
  auto kind = config.getString("damageSourceKind", "");
  if (!kind)
    return std::unexpected(std::move(kind.error()));
  source.damageSourceKind = std::move(*kind);
  auto effects = config.getArray("statusEffects", {});
  if (!effects)
    return std::unexpected(std::move(effects.error()));
  for (auto const& effect : *effects) {
    auto parsed = jsonToEphemeralStatusEffect(effect);
    if (!parsed)
      return std::unexpected(std::move(parsed.error()));
    source.statusEffects.push_back(std::move(*parsed));
  }
  auto knockback = config.get("knockback", Json(0.0f));
  if (!knockback)
    return std::unexpected(std::move(knockback.error()));
  if (knockback->isType(Json::Type::Array)) {
    auto parsed = jsonToVec2F(*knockback);
    if (!parsed)
      return std::unexpected(std::move(parsed.error()));
    source.knockback = *parsed;
  } else {
    auto parsed = knockback->toFloat();
    if (!parsed)
      return std::unexpected(std::move(parsed.error()));
    source.knockback = *parsed;
  }
  auto rayCheck = config.getBool("rayCheck", false);
  if (!rayCheck)
    return std::unexpected(std::move(rayCheck.error()));
  source.rayCheck = *rayCheck;
  return source;
}

DamageSource::DamageSource(DamageType damageType,
    DamageArea damageArea,
    float damage,
    bool trackSourceEntity,
    EntityId sourceEntityId,
    EntityDamageTeam team,
    std::optional<String> damageRepeatGroup,
    std::optional<float> damageRepeatTimeout,
    String damageSourceKind,
    List<EphemeralStatusEffect> statusEffects,
    Knockback knockback,
    bool rayCheck)
  : damageType(std::move(damageType)),
    damageArea(std::move(damageArea)),
    damage(std::move(damage)),
    trackSourceEntity(std::move(trackSourceEntity)),
    sourceEntityId(std::move(sourceEntityId)),
    team(std::move(team)),
    damageRepeatGroup(std::move(damageRepeatGroup)),
    damageRepeatTimeout(std::move(damageRepeatTimeout)),
    damageSourceKind(std::move(damageSourceKind)),
    statusEffects(std::move(statusEffects)),
    knockback(std::move(knockback)),
    rayCheck(std::move(rayCheck)) {}

std::expected<Json, StarException> DamageSource::toJson() const {
  Json damageAreaJson;
  if (std::holds_alternative<PolyF>(damageArea))
    damageAreaJson = jsonFromPolyF(std::get<PolyF>(damageArea));
  else if (std::holds_alternative<Line2F>(damageArea))
    damageAreaJson = jsonFromLine2F(std::get<Line2F>(damageArea));

  Json knockbackJson;
  if (std::holds_alternative<float>(knockback))
    knockbackJson = std::get<float>(knockback);
  else if (std::holds_alternative<Vec2F>(knockback))
    knockbackJson = jsonFromVec2F(std::get<Vec2F>(knockback));

  auto typeName = enumName<DamageTypeNames>(damageType);
  if (!typeName)
    return std::unexpected(std::move(typeName.error()));
  auto teamJson = team.toJson();
  if (!teamJson)
    return std::unexpected(std::move(teamJson.error()));
  return JsonObject{{"damageType", String(*typeName)},
                    {"damageArea", damageAreaJson},
                    {"damage", damage},
                    {"trackSourceEntity", trackSourceEntity},
                    {"sourceEntityId", sourceEntityId},
                    {"team", std::move(*teamJson)},
                    {"damageRepeatGroup", jsonFromOptional(damageRepeatGroup)},
                    {"damageRepeatTimeout", jsonFromOptional(damageRepeatTimeout)},
                    {"damageSourceKind", damageSourceKind},
                    {"statusEffects", statusEffects.transformed(jsonFromEphemeralStatusEffect)},
                    {"knockback", knockbackJson},
                    {"rayCheck", rayCheck}};
}

std::expected<bool, StarException> DamageSource::intersectsWithPoly(WorldGeometry const& geometry, PolyF const& targetPoly) const {
  if (std::holds_alternative<PolyF>(damageArea))
    return geometry.polyIntersectsPoly(std::get<PolyF>(damageArea), targetPoly);
  else if (std::holds_alternative<Line2F>(damageArea))
    return geometry.lineIntersectsPoly(std::get<Line2F>(damageArea), targetPoly);
  else
    return false;
}

Vec2F DamageSource::knockbackMomentum(WorldGeometry const& worldGeometry, Vec2F const& targetCenter) const {
  if (std::holds_alternative<Vec2F>(knockback)) {
    return std::get<Vec2F>(knockback);
  } else if (std::holds_alternative<float>(knockback)) {
    float strength = std::get<float>(knockback);
    if (strength != 0) {
      if (std::holds_alternative<PolyF>(damageArea))
        return worldGeometry.diff(targetCenter, std::get<PolyF>(damageArea).center()).normalized() * strength;
      else if (std::holds_alternative<Line2F>(damageArea))
        return vnorm(std::get<Line2F>(damageArea).diff()) * strength;
    }
  }

  return Vec2F();
}

bool DamageSource::operator==(DamageSource const& rhs) const {
  return std::tie(damageType, damageArea, damage, trackSourceEntity, sourceEntityId, team, damageSourceKind, statusEffects, knockback, rayCheck)
      == std::tie(rhs.damageType,
             rhs.damageArea,
             rhs.damage,
             rhs.trackSourceEntity,
             rhs.sourceEntityId,
             rhs.team,
             rhs.damageSourceKind,
             rhs.statusEffects,
             rhs.knockback,
             rhs.rayCheck);
}

DamageSource& DamageSource::translate(Vec2F const& position) {
  if (std::holds_alternative<PolyF>(damageArea))
    std::get<PolyF>(damageArea).translate(position);
  else if (std::holds_alternative<Line2F>(damageArea))
    std::get<Line2F>(damageArea).translate(position);
  return *this;
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, DamageSource const& source) {
  auto result = ds << source.damageType << source.damageArea << source.damage
      << source.trackSourceEntity << source.sourceEntityId << source.team
      << source.damageRepeatGroup << source.damageRepeatTimeout << source.damageSourceKind
      << source.statusEffects << source.knockback << source.rayCheck;
  if (!result)
    return std::unexpected(std::move(result.error()));
  return std::ref(ds);
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, DamageSource& source) {
  auto result = ds >> source.damageType >> source.damageArea >> source.damage
      >> source.trackSourceEntity >> source.sourceEntityId >> source.team
      >> source.damageRepeatGroup >> source.damageRepeatTimeout >> source.damageSourceKind
      >> source.statusEffects >> source.knockback >> source.rayCheck;
  if (!result)
    return std::unexpected(std::move(result.error()));
  return std::ref(ds);
}

DamageRequest::DamageRequest()
  : hitType(HitType::Hit), damageType(DamageType::Damage), damage(0.0f), sourceEntityId(NullEntityId) {}

std::expected<DamageRequest, StarException> DamageRequest::create(Json const& v) {
  DamageRequest request;
  auto hitName = v.getString("hitType", "hit");
  if (!hitName)
    return std::unexpected(std::move(hitName.error()));
  auto hit = enumValue<HitTypeNames>(*hitName);
  if (!hit)
    return std::unexpected(std::move(hit.error()));
  request.hitType = *hit;
  auto typeName = v.getString("damageType", "damage");
  if (!typeName)
    return std::unexpected(std::move(typeName.error()));
  auto type = enumValue<DamageTypeNames>(*typeName);
  if (!type)
    return std::unexpected(std::move(type.error()));
  request.damageType = *type;
  auto damage = v.getFloat("damage");
  if (!damage)
    return std::unexpected(std::move(damage.error()));
  request.damage = *damage;
  auto momentumJson = v.get("knockbackMomentum", JsonArray{0, 0});
  if (!momentumJson)
    return std::unexpected(std::move(momentumJson.error()));
  auto momentum = jsonToVec2F(*momentumJson);
  if (!momentum)
    return std::unexpected(std::move(momentum.error()));
  request.knockbackMomentum = *momentum;
  auto id = v.getInt("sourceEntityId", NullEntityId);
  if (!id)
    return std::unexpected(std::move(id.error()));
  request.sourceEntityId = *id;
  auto kind = v.getString("damageSourceKind", "");
  if (!kind)
    return std::unexpected(std::move(kind.error()));
  request.damageSourceKind = std::move(*kind);
  auto effects = v.getArray("statusEffects", {});
  if (!effects)
    return std::unexpected(std::move(effects.error()));
  for (auto const& effect : *effects) {
    auto parsed = jsonToEphemeralStatusEffect(effect);
    if (!parsed)
      return std::unexpected(std::move(parsed.error()));
    request.statusEffects.push_back(std::move(*parsed));
  }
  return request;
}

DamageRequest::DamageRequest(HitType hitType,
    DamageType damageType,
    float damage,
    Vec2F const& knockbackMomentum,
    EntityId sourceEntityId,
    String const& damageSourceKind,
    List<EphemeralStatusEffect> const& statusEffects)
  : hitType(hitType),
    damageType(damageType),
    damage(damage),
    knockbackMomentum(knockbackMomentum),
    sourceEntityId(sourceEntityId),
    damageSourceKind(damageSourceKind),
    statusEffects(statusEffects) {}

std::expected<Json, StarException> DamageRequest::toJson() const {
  auto hit = enumName<HitTypeNames>(hitType);
  if (!hit)
    return std::unexpected(std::move(hit.error()));
  auto type = enumName<DamageTypeNames>(damageType);
  if (!type)
    return std::unexpected(std::move(type.error()));
  return JsonObject{{"hitType", String(*hit)},
                    {"damageType", String(*type)},
                    {"damage", damage},
                    {"knockbackMomentum", jsonFromVec2F(knockbackMomentum)},
                    {"sourceEntityId", sourceEntityId},
                    {"damageSourceKind", damageSourceKind},
                    {"statusEffects", statusEffects.transformed(jsonFromEphemeralStatusEffect)}};
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, DamageRequest const& request) {
  auto result = ds << request.hitType << request.damageType << request.damage
      << request.knockbackMomentum << request.sourceEntityId << request.damageSourceKind << request.statusEffects;
  if (!result)
    return std::unexpected(std::move(result.error()));
  return std::ref(ds);
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, DamageRequest& request) {
  auto result = ds >> request.hitType >> request.damageType >> request.damage
      >> request.knockbackMomentum >> request.sourceEntityId >> request.damageSourceKind >> request.statusEffects;
  if (!result)
    return std::unexpected(std::move(result.error()));
  return std::ref(ds);
}

DamageNotification::DamageNotification() : sourceEntityId(), targetEntityId(), damageDealt(), healthLost() {}

std::expected<DamageNotification, StarException> DamageNotification::create(Json const& v) {
  DamageNotification notification;
  auto source = v.getInt("sourceEntityId");
  if (!source)
    return std::unexpected(std::move(source.error()));
  notification.sourceEntityId = *source;
  auto target = v.getInt("targetEntityId");
  if (!target)
    return std::unexpected(std::move(target.error()));
  notification.targetEntityId = *target;
  auto positionJson = v.get("position");
  if (!positionJson)
    return std::unexpected(std::move(positionJson.error()));
  auto position = jsonToVec2F(*positionJson);
  if (!position)
    return std::unexpected(std::move(position.error()));
  notification.position = *position;
  auto damage = v.getFloat("damageDealt");
  if (!damage)
    return std::unexpected(std::move(damage.error()));
  notification.damageDealt = *damage;
  auto health = v.getFloat("healthLost");
  if (!health)
    return std::unexpected(std::move(health.error()));
  notification.healthLost = *health;
  auto hitName = v.getString("hitType");
  if (!hitName)
    return std::unexpected(std::move(hitName.error()));
  auto hit = enumValue<HitTypeNames>(*hitName);
  if (!hit)
    return std::unexpected(std::move(hit.error()));
  notification.hitType = *hit;
  auto sourceKind = v.getString("damageSourceKind");
  if (!sourceKind)
    return std::unexpected(std::move(sourceKind.error()));
  notification.damageSourceKind = std::move(*sourceKind);
  auto material = v.getString("targetMaterialKind");
  if (!material)
    return std::unexpected(std::move(material.error()));
  notification.targetMaterialKind = std::move(*material);
  return notification;
}

DamageNotification::DamageNotification(EntityId sourceEntityId,
    EntityId targetEntityId,
    Vec2F position,
    float damageDealt,
    float healthLost,
    HitType hitType,
    String damageSourceKind,
    String targetMaterialKind)
  : sourceEntityId(sourceEntityId),
    targetEntityId(targetEntityId),
    position(position),
    damageDealt(damageDealt),
    healthLost(healthLost),
    hitType(hitType),
    damageSourceKind(std::move(damageSourceKind)),
    targetMaterialKind(std::move(targetMaterialKind)) {}

std::expected<Json, StarException> DamageNotification::toJson() const {
  auto hit = enumName<HitTypeNames>(hitType);
  if (!hit)
    return std::unexpected(std::move(hit.error()));
  return JsonObject{{"sourceEntityId", sourceEntityId},
                    {"targetEntityId", targetEntityId},
                    {"position", jsonFromVec2F(position)},
                    {"damageDealt", damageDealt},
                    {"healthLost", healthLost},
                    {"hitType", String(*hit)},
                    {"damageSourceKind", damageSourceKind},
                    {"targetMaterialKind", targetMaterialKind}};
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, DamageNotification const& notification) {
  auto source = ds.viwrite(notification.sourceEntityId);
  if (!source) return std::unexpected(std::move(source.error()));
  auto target = ds.viwrite(notification.targetEntityId);
  if (!target) return std::unexpected(std::move(target.error()));
  auto x = ds.vfwrite(notification.position[0], 0.01f);
  if (!x) return std::unexpected(std::move(x.error()));
  auto y = ds.vfwrite(notification.position[1], 0.01f);
  if (!y) return std::unexpected(std::move(y.error()));
  auto result = ds << notification.damageDealt << notification.healthLost << notification.hitType
      << notification.damageSourceKind << notification.targetMaterialKind;
  if (!result) return std::unexpected(std::move(result.error()));
  return std::ref(ds);
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, DamageNotification& notification) {
  auto source = ds.viread(notification.sourceEntityId);
  if (!source) return std::unexpected(std::move(source.error()));
  auto target = ds.viread(notification.targetEntityId);
  if (!target) return std::unexpected(std::move(target.error()));
  auto x = ds.vfread(notification.position[0], 0.01f);
  if (!x) return std::unexpected(std::move(x.error()));
  auto y = ds.vfread(notification.position[1], 0.01f);
  if (!y) return std::unexpected(std::move(y.error()));
  auto result = ds >> notification.damageDealt >> notification.healthLost >> notification.hitType
      >> notification.damageSourceKind >> notification.targetMaterialKind;
  if (!result) return std::unexpected(std::move(result.error()));
  return std::ref(ds);
}

}

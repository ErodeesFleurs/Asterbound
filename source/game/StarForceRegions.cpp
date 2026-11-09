import star.format;
import std;
import star.string;
import star.data_stream;
import star.json;
import star.json_extra;
import star.poly;
import star.data_stream_extra;

import star.force_regions;

import star.vector;
import star.rect;
namespace Star {

PhysicsCategoryFilter PhysicsCategoryFilter::whitelist(StringSet categories) {
  return PhysicsCategoryFilter(Whitelist, std::move(categories));
}

PhysicsCategoryFilter PhysicsCategoryFilter::blacklist(StringSet categories) {
  return PhysicsCategoryFilter(Blacklist, std::move(categories));
}

PhysicsCategoryFilter::PhysicsCategoryFilter() : PhysicsCategoryFilter(Blacklist, {}) {}

PhysicsCategoryFilter::PhysicsCategoryFilter(Type type, StringSet categories)
  : type(type), categories(std::move(categories)) {}

bool PhysicsCategoryFilter::check(StringSet const& otherCategories) const {
  bool intersection = categories.has_intersection(otherCategories);
  if (type == Whitelist)
    return intersection;
  else
    return !intersection;
}

bool PhysicsCategoryFilter::operator==(PhysicsCategoryFilter const& rhs) const {
  return std::tie(type, categories) == std::tie(rhs.type, rhs.categories);
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, PhysicsCategoryFilter& pcf) {
  return ds >> pcf.type >> pcf.categories;
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, PhysicsCategoryFilter const& pcf) {
  return ds << pcf.type << pcf.categories;
}

std::expected<PhysicsCategoryFilter, StarException> jsonToPhysicsCategoryFilter(Json const& json) {
  auto whitelist = json.opt("categoryWhitelist");
  if (!whitelist) return std::unexpected(std::move(whitelist.error()));
  auto blacklist = json.opt("categoryBlacklist");
  if (!blacklist) return std::unexpected(std::move(blacklist.error()));
  if (*whitelist && *blacklist)
    return std::unexpected(JsonException::format("Cannot specify both a physics category whitelist and blacklist"));
  if (*whitelist) {
    auto categories = jsonToStringSet(**whitelist);
    if (!categories) return std::unexpected(std::move(categories.error()));
    return PhysicsCategoryFilter::whitelist(std::move(*categories));
  }
  if (*blacklist) {
    auto categories = jsonToStringSet(**blacklist);
    if (!categories) return std::unexpected(std::move(categories.error()));
    return PhysicsCategoryFilter::blacklist(std::move(*categories));
  }
  return PhysicsCategoryFilter();
}

std::expected<DirectionalForceRegion, StarException> DirectionalForceRegion::fromJson(Json const& json) {
  DirectionalForceRegion dfr;
  auto hasPoly = json.contains("polyRegion");
  if (!hasPoly) return std::unexpected(std::move(hasPoly.error()));
  if (*hasPoly) {
    auto config = json.get("polyRegion");
    if (!config) return std::unexpected(std::move(config.error()));
    auto region = jsonToPolyF(*config);
    if (!region) return std::unexpected(std::move(region.error()));
    dfr.region = std::move(*region);
  } else {
    auto config = json.get("rectRegion");
    if (!config) return std::unexpected(std::move(config.error()));
    auto region = jsonToRectF(*config);
    if (!region) return std::unexpected(std::move(region.error()));
    dfr.region = PolyF(*region);
  }
  auto xTargetVelocity = json.optFloat("xTargetVelocity");
  if (!xTargetVelocity) return std::unexpected(std::move(xTargetVelocity.error()));
  dfr.xTargetVelocity = *xTargetVelocity;
  auto yTargetVelocity = json.optFloat("yTargetVelocity");
  if (!yTargetVelocity) return std::unexpected(std::move(yTargetVelocity.error()));
  dfr.yTargetVelocity = *yTargetVelocity;
  auto controlForce = json.getFloat("controlForce");
  if (!controlForce) return std::unexpected(std::move(controlForce.error()));
  dfr.controlForce = *controlForce;
  auto filter = jsonToPhysicsCategoryFilter(json);
  if (!filter) return std::unexpected(std::move(filter.error()));
  dfr.categoryFilter = std::move(*filter);
  return dfr;
}

RectF DirectionalForceRegion::boundBox() const {
  return region.boundBox();
}

void DirectionalForceRegion::translate(Vec2F const& pos) {
  region.translate(pos);
}

bool DirectionalForceRegion::operator==(DirectionalForceRegion const& rhs) const {
  return std::tie(region, xTargetVelocity, yTargetVelocity, controlForce, categoryFilter)
      == std::tie(rhs.region, rhs.xTargetVelocity, rhs.yTargetVelocity, rhs.controlForce, rhs.categoryFilter);
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, DirectionalForceRegion& dfr) {
  return ds >> dfr.region >> dfr.xTargetVelocity >> dfr.yTargetVelocity >> dfr.controlForce >> dfr.categoryFilter;
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, DirectionalForceRegion const& dfr) {
  return ds << dfr.region << dfr.xTargetVelocity << dfr.yTargetVelocity << dfr.controlForce << dfr.categoryFilter;
}

std::expected<RadialForceRegion, StarException> RadialForceRegion::fromJson(Json const& json) {
  RadialForceRegion rfr;
  auto centerConfig = json.opt("center");
  if (!centerConfig) return std::unexpected(std::move(centerConfig.error()));
  if (*centerConfig) {
    auto center = jsonToVec2F(**centerConfig);
    if (!center) return std::unexpected(std::move(center.error()));
    rfr.center = *center;
  } else {
    rfr.center = Vec2F{};
  }
  auto outerRadius = json.getFloat("outerRadius");
  if (!outerRadius) return std::unexpected(std::move(outerRadius.error()));
  rfr.outerRadius = *outerRadius;
  auto innerRadius = json.getFloat("innerRadius");
  if (!innerRadius) return std::unexpected(std::move(innerRadius.error()));
  rfr.innerRadius = *innerRadius;
  auto targetRadialVelocity = json.getFloat("targetRadialVelocity");
  if (!targetRadialVelocity) return std::unexpected(std::move(targetRadialVelocity.error()));
  rfr.targetRadialVelocity = *targetRadialVelocity;
  auto controlForce = json.getFloat("controlForce");
  if (!controlForce) return std::unexpected(std::move(controlForce.error()));
  rfr.controlForce = *controlForce;
  auto filter = jsonToPhysicsCategoryFilter(json);
  if (!filter) return std::unexpected(std::move(filter.error()));
  rfr.categoryFilter = std::move(*filter);
  return rfr;
}

RectF RadialForceRegion::boundBox() const {
  return RectF::withCenter(center, Vec2F::filled(outerRadius));
}

void RadialForceRegion::translate(Vec2F const& pos) {
  center += pos;
}

bool RadialForceRegion::operator==(RadialForceRegion const& rhs) const {
  return std::tie(center, outerRadius, innerRadius, targetRadialVelocity, controlForce, categoryFilter)
      == std::tie(rhs.center, rhs.outerRadius, rhs.innerRadius, rhs.targetRadialVelocity, rhs.controlForce, rhs.categoryFilter);
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, RadialForceRegion& rfr) {
  return ds >> rfr.center >> rfr.outerRadius >> rfr.innerRadius >> rfr.targetRadialVelocity >> rfr.controlForce >> rfr.categoryFilter;
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, RadialForceRegion const& rfr) {
  return ds << rfr.center << rfr.outerRadius << rfr.innerRadius << rfr.targetRadialVelocity << rfr.controlForce << rfr.categoryFilter;
}

std::expected<GradientForceRegion, StarException> GradientForceRegion::fromJson(Json const& json) {
  GradientForceRegion gfr;
  auto hasPoly = json.contains("polyRegion");
  if (!hasPoly) return std::unexpected(std::move(hasPoly.error()));
  if (*hasPoly) {
    auto config = json.get("polyRegion");
    if (!config) return std::unexpected(std::move(config.error()));
    auto region = jsonToPolyF(*config);
    if (!region) return std::unexpected(std::move(region.error()));
    gfr.region = std::move(*region);
  } else {
    auto config = json.get("rectRegion");
    if (!config) return std::unexpected(std::move(config.error()));
    auto region = jsonToRectF(*config);
    if (!region) return std::unexpected(std::move(region.error()));
    gfr.region = PolyF(*region);
  }
  auto gradientConfig = json.get("gradient");
  if (!gradientConfig) return std::unexpected(std::move(gradientConfig.error()));
  auto gradient = jsonToLine2F(*gradientConfig);
  if (!gradient) return std::unexpected(std::move(gradient.error()));
  gfr.gradient = *gradient;
  auto baseTargetVelocity = json.getFloat("baseTargetVelocity");
  if (!baseTargetVelocity) return std::unexpected(std::move(baseTargetVelocity.error()));
  gfr.baseTargetVelocity = *baseTargetVelocity;
  auto baseControlForce = json.getFloat("baseControlForce");
  if (!baseControlForce) return std::unexpected(std::move(baseControlForce.error()));
  gfr.baseControlForce = *baseControlForce;
  auto filter = jsonToPhysicsCategoryFilter(json);
  if (!filter) return std::unexpected(std::move(filter.error()));
  gfr.categoryFilter = std::move(*filter);
  return gfr;
}

RectF GradientForceRegion::boundBox() const {
  return region.boundBox();
}

void GradientForceRegion::translate(Vec2F const& pos) {
  region.translate(pos);
  gradient.translate(pos);
}

bool GradientForceRegion::operator==(GradientForceRegion const& rhs) const {
  return std::tie(region, gradient, baseTargetVelocity, baseControlForce, categoryFilter)
      == std::tie(rhs.region, rhs.gradient, rhs.baseTargetVelocity, rhs.baseControlForce, rhs.categoryFilter);
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, GradientForceRegion& gfr) {
  return ds >> gfr.region >> gfr.gradient >> gfr.baseTargetVelocity >> gfr.baseControlForce >> gfr.categoryFilter;
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, GradientForceRegion const& gfr) {
  return ds << gfr.region << gfr.gradient << gfr.baseTargetVelocity << gfr.baseControlForce << gfr.categoryFilter;
}

std::expected<PhysicsForceRegion, StarException> jsonToPhysicsForceRegion(Json const& json) {
  auto type = json.getString("type");
  if (!type) return std::unexpected(std::move(type.error()));
  auto directional = type->equals_ignore_case("DirectionalForceRegion");
  if (!directional) return std::unexpected(std::move(directional.error()));
  if (*directional) {
    auto region = DirectionalForceRegion::fromJson(json);
    if (!region) return std::unexpected(std::move(region.error()));
    return PhysicsForceRegion(std::move(*region));
  }
  auto radial = type->equals_ignore_case("RadialForceRegion");
  if (!radial) return std::unexpected(std::move(radial.error()));
  if (*radial) {
    auto region = RadialForceRegion::fromJson(json);
    if (!region) return std::unexpected(std::move(region.error()));
    return PhysicsForceRegion(std::move(*region));
  }
  auto gradient = type->equals_ignore_case("GradientForceRegion");
  if (!gradient) return std::unexpected(std::move(gradient.error()));
  if (*gradient) {
    auto region = GradientForceRegion::fromJson(json);
    if (!region) return std::unexpected(std::move(region.error()));
    return PhysicsForceRegion(std::move(*region));
  }
  return std::unexpected(JsonException::format("No such physics force region type '{}'", *type));
}

}

import star.format;
import std;
import star.string;
import star.data_stream;
import star.json;
import star.json_extra;
import star.poly;
import star.math_common;
import star.data_stream_extra;

import star.force_regions;

import star.vector;
import star.line;
import star.rect;
namespace Star {
namespace {

std::expected<PolyF, StarException> parse_region_polygon(Json const& json) {
  auto has_poly = json.contains("polyRegion");
  if (!has_poly) return std::unexpected(std::move(has_poly.error()));
  if (*has_poly)
    return json.get("polyRegion").and_then(json_to_polyf);
  return json.get("rectRegion").and_then(json_to_rectf).transform([](RectF const& rect) { return PolyF(rect); });
}

bool finite_point(Vec2F const& point) {
  return is_finite(point[0]) && is_finite(point[1]);
}

std::string_view invalid_region_reason(DirectionalForceRegion const& region) {
  if (!std::ranges::all_of(region.region, finite_point))
    return "Force region polygon coordinates must be finite";
  if ((region.x_target_velocity && !is_finite(*region.x_target_velocity))
      || (region.y_target_velocity && !is_finite(*region.y_target_velocity)) || !is_finite(region.control_force))
    return "Directional force region velocities and controlForce must be finite";
  return {};
}

std::string_view invalid_region_reason(RadialForceRegion const& region) {
  if (!is_finite(region.inner_radius) || !is_finite(region.outer_radius)
      || region.inner_radius < 0 || region.outer_radius < region.inner_radius)
    return "Radial force region requires finite radii with 0 <= innerRadius <= outerRadius";
  if (!finite_point(region.center))
    return "Radial force region center must be finite";
  auto bounds = region.bounding_box();
  if (!finite_point(bounds.min()) || !finite_point(bounds.max()))
    return "Radial force region bounds must be finite";
  if (!is_finite(region.target_radial_velocity) || !is_finite(region.control_force))
    return "Radial force region targetRadialVelocity and controlForce must be finite";
  return {};
}

std::string_view invalid_region_reason(GradientForceRegion const& region) {
  if (!std::ranges::all_of(region.region, finite_point))
    return "Force region polygon coordinates must be finite";
  if (!finite_point(region.gradient.min()) || !finite_point(region.gradient.max()))
    return "Force region gradient coordinates must be finite";
  auto length = region.gradient.length();
  if (!is_finite(length) || length <= 0)
    return "Force region gradient must have a finite, positive length";
  if (!is_finite(region.base_target_velocity) || !is_finite(region.base_control_force))
    return "Gradient force region baseTargetVelocity and baseControlForce must be finite";
  return {};
}

template <physics_force_region Region>
std::expected<Region, StarException> checked_json_region(Region region) {
  if (auto reason = invalid_region_reason(region); !reason.empty())
    return std::unexpected(JsonException(std::string(reason)));
  return region;
}

template <physics_force_region Region>
std::expected<std::reference_wrapper<DataStream>, StarException> commit_region_read(
    std::expected<std::reference_wrapper<DataStream>, StarException> read, Region& decoded, Region& destination) {
  if (!read) return read;
  if (auto reason = invalid_region_reason(decoded); !reason.empty())
    return std::unexpected(DataStreamException(std::string(reason)));
  destination = std::move(decoded);
  return read;
}

} // namespace

PhysicsCategoryFilter PhysicsCategoryFilter::whitelist(StringSet categories) {
  return PhysicsCategoryFilter(Whitelist, std::move(categories));
}

PhysicsCategoryFilter PhysicsCategoryFilter::blacklist(StringSet categories) {
  return PhysicsCategoryFilter(Blacklist, std::move(categories));
}

PhysicsCategoryFilter::PhysicsCategoryFilter(Type type, StringSet categories)
  : type(type), categories(std::move(categories)) {}

bool PhysicsCategoryFilter::check(StringSet const& other_categories) const {
  return (type == Whitelist) == categories.has_intersection(other_categories);
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, PhysicsCategoryFilter& pcf) {
  auto type = ds.read<std::underlying_type_t<PhysicsCategoryFilter::Type>>();
  if (!type) return std::unexpected(std::move(type.error()));
  if (*type != PhysicsCategoryFilter::Whitelist && *type != PhysicsCategoryFilter::Blacklist)
    return std::unexpected(DataStreamException("Invalid physics category filter type"));
  auto categories = ds.read<StringSet>();
  if (!categories) return std::unexpected(std::move(categories.error()));
  pcf = PhysicsCategoryFilter(static_cast<PhysicsCategoryFilter::Type>(*type), std::move(*categories));
  return std::ref(ds);
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, PhysicsCategoryFilter const& pcf) {
  return ds << pcf.type << pcf.categories;
}

std::expected<PhysicsCategoryFilter, StarException> json_to_physics_category_filter(Json const& json) {
  auto whitelist = json.opt("categoryWhitelist");
  if (!whitelist) return std::unexpected(std::move(whitelist.error()));
  auto blacklist = json.opt("categoryBlacklist");
  if (!blacklist) return std::unexpected(std::move(blacklist.error()));
  if (*whitelist && *blacklist)
    return std::unexpected(JsonException::format("Cannot specify both a physics category whitelist and blacklist"));
  if (*whitelist) {
    auto categories = json_to_string_set(**whitelist);
    if (!categories) return std::unexpected(std::move(categories.error()));
    return PhysicsCategoryFilter::whitelist(std::move(*categories));
  }
  if (*blacklist) {
    auto categories = json_to_string_set(**blacklist);
    if (!categories) return std::unexpected(std::move(categories.error()));
    return PhysicsCategoryFilter::blacklist(std::move(*categories));
  }
  return PhysicsCategoryFilter();
}

std::expected<DirectionalForceRegion, StarException> DirectionalForceRegion::from_json(Json const& json) {
  auto polygon = parse_region_polygon(json);
  if (!polygon) return std::unexpected(std::move(polygon.error()));
  auto x_target_velocity = json.opt_float("xTargetVelocity");
  if (!x_target_velocity) return std::unexpected(std::move(x_target_velocity.error()));
  auto y_target_velocity = json.opt_float("yTargetVelocity");
  if (!y_target_velocity) return std::unexpected(std::move(y_target_velocity.error()));
  auto control_force = json.get_float("controlForce");
  if (!control_force) return std::unexpected(std::move(control_force.error()));
  auto filter = json_to_physics_category_filter(json);
  if (!filter) return std::unexpected(std::move(filter.error()));
  return checked_json_region(DirectionalForceRegion{std::move(*polygon), *x_target_velocity, *y_target_velocity,
      *control_force, std::move(*filter)});
}

RectF DirectionalForceRegion::bounding_box() const {
  return region.bounding_box();
}

void DirectionalForceRegion::translate(Vec2F const& pos) {
  region.translate(pos);
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, DirectionalForceRegion& dfr) {
  DirectionalForceRegion decoded;
  auto read = ds >> decoded.region >> decoded.x_target_velocity >> decoded.y_target_velocity >> decoded.control_force
      >> decoded.category_filter;
  return commit_region_read(std::move(read), decoded, dfr);
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, DirectionalForceRegion const& dfr) {
  return ds << dfr.region << dfr.x_target_velocity << dfr.y_target_velocity << dfr.control_force << dfr.category_filter;
}

std::expected<RadialForceRegion, StarException> RadialForceRegion::from_json(Json const& json) {
  Vec2F center{};
  auto center_config = json.opt("center");
  if (!center_config) return std::unexpected(std::move(center_config.error()));
  if (*center_config) {
    auto parsed = json_to_vec2f(**center_config);
    if (!parsed) return std::unexpected(std::move(parsed.error()));
    center = *parsed;
  }
  auto outer_radius = json.get_float("outerRadius");
  if (!outer_radius) return std::unexpected(std::move(outer_radius.error()));
  auto inner_radius = json.get_float("innerRadius");
  if (!inner_radius) return std::unexpected(std::move(inner_radius.error()));
  auto target_radial_velocity = json.get_float("targetRadialVelocity");
  if (!target_radial_velocity) return std::unexpected(std::move(target_radial_velocity.error()));
  auto control_force = json.get_float("controlForce");
  if (!control_force) return std::unexpected(std::move(control_force.error()));
  auto filter = json_to_physics_category_filter(json);
  if (!filter) return std::unexpected(std::move(filter.error()));
  return checked_json_region(RadialForceRegion{center, *outer_radius, *inner_radius, *target_radial_velocity,
      *control_force, std::move(*filter)});
}

RectF RadialForceRegion::bounding_box() const {
  auto extent = Vec2F::filled(outer_radius);
  return RectF(center - extent, center + extent);
}

void RadialForceRegion::translate(Vec2F const& pos) {
  center += pos;
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, RadialForceRegion& rfr) {
  RadialForceRegion decoded;
  auto read = ds >> decoded.center >> decoded.outer_radius >> decoded.inner_radius >> decoded.target_radial_velocity
      >> decoded.control_force >> decoded.category_filter;
  return commit_region_read(std::move(read), decoded, rfr);
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, RadialForceRegion const& rfr) {
  return ds << rfr.center << rfr.outer_radius << rfr.inner_radius << rfr.target_radial_velocity << rfr.control_force
      << rfr.category_filter;
}

std::expected<GradientForceRegion, StarException> GradientForceRegion::from_json(Json const& json) {
  auto polygon = parse_region_polygon(json);
  if (!polygon) return std::unexpected(std::move(polygon.error()));
  auto gradient = json.get("gradient").and_then(json_to_line2f);
  if (!gradient) return std::unexpected(std::move(gradient.error()));
  auto base_target_velocity = json.get_float("baseTargetVelocity");
  if (!base_target_velocity) return std::unexpected(std::move(base_target_velocity.error()));
  auto base_control_force = json.get_float("baseControlForce");
  if (!base_control_force) return std::unexpected(std::move(base_control_force.error()));
  auto filter = json_to_physics_category_filter(json);
  if (!filter) return std::unexpected(std::move(filter.error()));
  return checked_json_region(GradientForceRegion{std::move(*polygon), *gradient, *base_target_velocity,
      *base_control_force, std::move(*filter)});
}

RectF GradientForceRegion::bounding_box() const {
  return region.bounding_box();
}

void GradientForceRegion::translate(Vec2F const& pos) {
  region.translate(pos);
  gradient.translate(pos);
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, GradientForceRegion& gfr) {
  GradientForceRegion decoded;
  auto read = ds >> decoded.region >> decoded.gradient >> decoded.base_target_velocity >> decoded.base_control_force
      >> decoded.category_filter;
  return commit_region_read(std::move(read), decoded, gfr);
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, GradientForceRegion const& gfr) {
  return ds << gfr.region << gfr.gradient << gfr.base_target_velocity << gfr.base_control_force << gfr.category_filter;
}

std::expected<PhysicsForceRegion, StarException> json_to_physics_force_region(Json const& json) {
  auto type = json.get_string("type");
  if (!type) return std::unexpected(std::move(type.error()));
  auto directional = type->equals_ignore_case("DirectionalForceRegion");
  if (!directional) return std::unexpected(std::move(directional.error()));
  if (*directional)
    return DirectionalForceRegion::from_json(json);
  auto radial = type->equals_ignore_case("RadialForceRegion");
  if (!radial) return std::unexpected(std::move(radial.error()));
  if (*radial)
    return RadialForceRegion::from_json(json);
  auto gradient = type->equals_ignore_case("GradientForceRegion");
  if (!gradient) return std::unexpected(std::move(gradient.error()));
  if (*gradient)
    return GradientForceRegion::from_json(json);
  return std::unexpected(JsonException::format("No such physics force region type '{}'", *type));
}

}

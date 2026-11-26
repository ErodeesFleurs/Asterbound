import star.format;
import star.json;
import star.json_extra;
import star.interpolation;
import star.id_map;
import star.line;
import star.poly;
import star.casting;
import star.vector;
import star.data_stream;
import star.game_types;
import star.liquid_types;
import star.strong_typedef;
import star.bi_map;
import star.list;
import star.rpc_promise;
import star.data_stream_extra;
import star.random;
import star.ordered_map;
import star.rect;
import star.thread;
import star.io_device;
import star.image_processing;
import star.hash;
import star.string_view;
import star.directives;
import star.asset_path;
import star.ref_ptr;
import star.logging;
import star.listener;
import std;
import star.set;
import star.string;

import star.damage_types;
import star.world_geometry;
import star.status_types;
import star.damage;
import star.light_source;
import star.entity;
import star.tile_damage;
import star.interaction_types;
import star.item_descriptor;
import star.celestial_coordinate;
import star.quest_descriptor;
import star.interactive_entity;
import star.collision_block;
import star.tile_entity;
import star.tile_modification;
import star.lua_root;
import star.force_regions;
import star.world;
import star.physics_entity;
import star.movement_controller;
import star.asset_source;
import star.assets;
import star.configuration;
import star.root;

import star.math_common;
import star.algorithm;
namespace Star {

std::expected<MovementParameters, StarException> MovementParameters::sensible_defaults(Root& root) {
  auto assets = root.assets();
  if (!assets) return std::unexpected(std::move(assets.error()));
  auto config = (*assets)->json("/default_movement.config");
  if (!config) return std::unexpected(std::move(config.error()));
  if (config->type() != Json::Type::Object)
    return std::unexpected(config->object_ptr().error());
  return create(*config);
}

MovementParameters::MovementParameters() = default;

std::expected<MovementParameters, StarException> MovementParameters::create(Json const& config) {
  MovementParameters value;
  if (config.is_null())
    return value;

  if (auto result = config.opt_float("mass"); result)
    value.mass = *result;
  else return std::unexpected(std::move(result.error()));
  if (auto result = config.opt_float("gravityMultiplier"); result)
    value.gravity_multiplier = *result;
  else return std::unexpected(std::move(result.error()));
  if (auto result = config.opt_float("liquidBuoyancy"); result)
    value.liquid_buoyancy = *result;
  else return std::unexpected(std::move(result.error()));
  if (auto result = config.opt_float("airBuoyancy"); result)
    value.air_buoyancy = *result;
  else return std::unexpected(std::move(result.error()));
  if (auto result = config.opt_float("bounceFactor"); result)
    value.bounce_factor = *result;
  else return std::unexpected(std::move(result.error()));
  if (auto result = config.opt_bool("stopOnFirstBounce"); result)
    value.stop_on_first_bounce = *result;
  else return std::unexpected(std::move(result.error()));
  if (auto result = config.opt_bool("enableSurfaceSlopeCorrection"); result)
    value.enable_surface_slope_correction = *result;
  else return std::unexpected(std::move(result.error()));
  if (auto result = config.opt_float("slopeSlidingFactor"); result)
    value.slope_sliding_factor = *result;
  else return std::unexpected(std::move(result.error()));
  if (auto result = config.opt_float("maxMovementPerStep"); result)
    value.max_movement_per_step = *result;
  else return std::unexpected(std::move(result.error()));
  if (auto result = config.opt_float("maximumCorrection"); result)
    value.maximum_correction = *result;
  else return std::unexpected(std::move(result.error()));
  if (auto result = config.opt_float("speedLimit"); result)
    value.speed_limit = *result;
  else return std::unexpected(std::move(result.error()));
  if (auto result = config.opt_float("discontinuityThreshold"); result)
    value.discontinuity_threshold = *result;
  else return std::unexpected(std::move(result.error()));
  auto collision_poly_config = config.opt("collisionPoly");
  if (!collision_poly_config) return std::unexpected(std::move(collision_poly_config.error()));
  if (*collision_poly_config) {
    auto collision_poly_value = json_to_polyf(**collision_poly_config);
    if (!collision_poly_value) return std::unexpected(std::move(collision_poly_value.error()));
    value.collision_poly = std::move(*collision_poly_value);
  }
  if (auto result = config.opt_bool("stickyCollision"); result)
    value.sticky_collision = *result;
  else return std::unexpected(std::move(result.error()));
  if (auto result = config.opt_float("stickyForce"); result)
    value.sticky_force = *result;
  else return std::unexpected(std::move(result.error()));
  if (auto result = config.opt_float("airFriction"); result)
    value.air_friction = *result;
  else return std::unexpected(std::move(result.error()));
  if (auto result = config.opt_float("liquidFriction"); result)
    value.liquid_friction = *result;
  else return std::unexpected(std::move(result.error()));
  if (auto result = config.opt_float("groundFriction"); result)
    value.ground_friction = *result;
  else return std::unexpected(std::move(result.error()));
  if (auto result = config.opt_bool("collisionEnabled"); result)
    value.collision_enabled = *result;
  else return std::unexpected(std::move(result.error()));
  if (auto result = config.opt_bool("frictionEnabled"); result)
    value.friction_enabled = *result;
  else return std::unexpected(std::move(result.error()));
  if (auto result = config.opt_bool("gravityEnabled"); result)
    value.gravity_enabled = *result;
  else return std::unexpected(std::move(result.error()));
  if (auto result = config.opt_bool("ignorePlatformCollision"); result)
    value.ignore_platform_collision = *result;
  else return std::unexpected(std::move(result.error()));
  if (auto result = config.opt_float("maximumPlatformCorrection"); result)
    value.maximum_platform_correction = *result;
  else return std::unexpected(std::move(result.error()));
  if (auto result = config.opt_float("maximumPlatformCorrectionVelocityFactor"); result)
    value.maximum_platform_correction_velocity_factor = *result;
  else return std::unexpected(std::move(result.error()));
  auto categories_config = config.opt("physicsEffectCategories");
  if (!categories_config) return std::unexpected(std::move(categories_config.error()));
  if (*categories_config) {
    auto categories = json_to_string_set(**categories_config);
    if (!categories) return std::unexpected(std::move(categories.error()));
    value.physics_effect_categories = std::move(*categories);
  }
  if (auto result = config.opt_int("restDuration"); result)
    value.rest_duration = *result;
  else return std::unexpected(std::move(result.error()));
  return value;
}

namespace {
template <std::copyable T>
void merge_field(std::optional<T>& dest, std::optional<T> const& patch) {
  if (patch)
    dest = patch;
}

void merge_field(std::optional<PolyF>& dest, std::optional<PolyF> const& patch) {
  // Use coordinate bits so even a serialized signed-zero change is retained.
  auto same_vertex = [](Vec2F const& lhs, Vec2F const& rhs) {
    return std::bit_cast<std::uint32_t>(lhs[0]) == std::bit_cast<std::uint32_t>(rhs[0])
        && std::bit_cast<std::uint32_t>(lhs[1]) == std::bit_cast<std::uint32_t>(rhs[1]);
  };
  if (patch && (!dest || !std::ranges::equal(*dest, *patch, same_vertex)))
    dest = patch;
}

bool same_category_sequence(StringSet const& lhs, StringSet const& rhs) {
  if (lhs.size() != rhs.size())
    return false;
  auto other = rhs.begin();
  for (auto const& category : lhs) {
    if (category != *other)
      return false;
    ++other;
  }
  return true;
}

void merge_field(std::optional<StringSet>& dest, std::optional<StringSet> const& patch) {
  // JSON and wire serialize the set in iteration order.
  if (patch && (!dest || !same_category_sequence(*dest, *patch)))
    dest = patch;
}

template <std::copyable T>
void merge_field(std::optional<T>& dest, std::optional<T> const& base, std::optional<T> const& patch) {
  dest = patch ? patch : base;
}

template <class T>
concept movement_parameter_source = std::same_as<T, MovementParameters>;

template <movement_parameter_source... Sources>
  requires (sizeof...(Sources) == 1 || sizeof...(Sources) == 2)
void merge_fields(MovementParameters& dest, Sources const&... sources) {
  merge_field(dest.mass, sources.mass...);
  merge_field(dest.gravity_multiplier, sources.gravity_multiplier...);
  merge_field(dest.liquid_buoyancy, sources.liquid_buoyancy...);
  merge_field(dest.air_buoyancy, sources.air_buoyancy...);
  merge_field(dest.bounce_factor, sources.bounce_factor...);
  merge_field(dest.stop_on_first_bounce, sources.stop_on_first_bounce...);
  merge_field(dest.enable_surface_slope_correction, sources.enable_surface_slope_correction...);
  merge_field(dest.slope_sliding_factor, sources.slope_sliding_factor...);
  merge_field(dest.max_movement_per_step, sources.max_movement_per_step...);
  merge_field(dest.maximum_correction, sources.maximum_correction...);
  merge_field(dest.speed_limit, sources.speed_limit...);
  merge_field(dest.discontinuity_threshold, sources.discontinuity_threshold...);
  merge_field(dest.collision_poly, sources.collision_poly...);
  merge_field(dest.sticky_collision, sources.sticky_collision...);
  merge_field(dest.sticky_force, sources.sticky_force...);
  merge_field(dest.air_friction, sources.air_friction...);
  merge_field(dest.liquid_friction, sources.liquid_friction...);
  merge_field(dest.ground_friction, sources.ground_friction...);
  merge_field(dest.collision_enabled, sources.collision_enabled...);
  merge_field(dest.friction_enabled, sources.friction_enabled...);
  merge_field(dest.gravity_enabled, sources.gravity_enabled...);
  merge_field(dest.ignore_platform_collision, sources.ignore_platform_collision...);
  merge_field(dest.maximum_platform_correction, sources.maximum_platform_correction...);
  merge_field(dest.maximum_platform_correction_velocity_factor, sources.maximum_platform_correction_velocity_factor...);
  merge_field(dest.physics_effect_categories, sources.physics_effect_categories...);
  merge_field(dest.rest_duration, sources.rest_duration...);
}
}

MovementParameters MovementParameters::merge(MovementParameters const& patch) const {
  MovementParameters merged;
  merge_fields(merged, *this, patch);
  return merged;
}

std::expected<Json, StarException> MovementParameters::to_json() const {
  auto collision_poly_json = json_from_optional(collision_poly, json_from_polyf);
  if (!collision_poly_json) return std::unexpected(std::move(collision_poly_json.error()));
  auto categories_json = json_from_optional(physics_effect_categories, json_from_string_set);
  if (!categories_json) return std::unexpected(std::move(categories_json.error()));

  return JsonObject{
    {"mass", json_from_optional(mass)},
    {"gravityMultiplier", json_from_optional(gravity_multiplier)},
    {"liquidBuoyancy", json_from_optional(liquid_buoyancy)},
    {"airBuoyancy", json_from_optional(air_buoyancy)},
    {"stopOnFirstBounce", json_from_optional(stop_on_first_bounce)},
    {"enableSurfaceSlopeCorrection", json_from_optional(enable_surface_slope_correction)},
    {"slopeSlidingFactor", json_from_optional(slope_sliding_factor)},
    {"maxMovementPerStep", json_from_optional(max_movement_per_step)},
    {"maximumCorrection", json_from_optional(maximum_correction)},
    {"speedLimit", json_from_optional(speed_limit)},
    {"discontinuityThreshold", json_from_optional(discontinuity_threshold)},
    {"collisionPoly", *collision_poly_json},
    {"stickyCollision", json_from_optional(sticky_collision)},
    {"stickyForce", json_from_optional(sticky_force)},
    {"airFriction", json_from_optional(air_friction)},
    {"liquidFriction", json_from_optional(liquid_friction)},
    {"groundFriction", json_from_optional(ground_friction)},
    {"collisionEnabled", json_from_optional(collision_enabled)},
    {"frictionEnabled", json_from_optional(friction_enabled)},
    {"gravityEnabled", json_from_optional(gravity_enabled)},
    {"ignorePlatformCollision", json_from_optional(ignore_platform_collision)},
    {"maximumPlatformCorrection", json_from_optional(maximum_platform_correction)},
    {"maximumPlatformCorrectionVelocityFactor", json_from_optional(maximum_platform_correction_velocity_factor)},
    {"physicsEffectCategories", *categories_json},
    {"restDuration", json_from_optional(rest_duration)}
  };
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, MovementParameters& movement_parameters) {
  if (auto result = ds.read(movement_parameters.mass); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.read(movement_parameters.gravity_multiplier); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.read(movement_parameters.liquid_buoyancy); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.read(movement_parameters.air_buoyancy); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.read(movement_parameters.stop_on_first_bounce); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.read(movement_parameters.enable_surface_slope_correction); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.read(movement_parameters.slope_sliding_factor); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.read(movement_parameters.max_movement_per_step); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.read(movement_parameters.maximum_correction); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.read(movement_parameters.speed_limit); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.read(movement_parameters.discontinuity_threshold); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.read(movement_parameters.collision_poly); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.read(movement_parameters.sticky_collision); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.read(movement_parameters.sticky_force); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.read(movement_parameters.air_friction); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.read(movement_parameters.liquid_friction); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.read(movement_parameters.ground_friction); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.read(movement_parameters.collision_enabled); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.read(movement_parameters.friction_enabled); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.read(movement_parameters.gravity_enabled); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.read(movement_parameters.ignore_platform_collision); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.read(movement_parameters.maximum_platform_correction); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.read(movement_parameters.maximum_platform_correction_velocity_factor); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.read(movement_parameters.physics_effect_categories); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.read(movement_parameters.rest_duration); !result) return std::unexpected(std::move(result.error()));

  return std::ref(ds);
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, MovementParameters const& movement_parameters) {
  if (auto result = ds.write(movement_parameters.mass); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.write(movement_parameters.gravity_multiplier); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.write(movement_parameters.liquid_buoyancy); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.write(movement_parameters.air_buoyancy); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.write(movement_parameters.stop_on_first_bounce); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.write(movement_parameters.enable_surface_slope_correction); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.write(movement_parameters.slope_sliding_factor); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.write(movement_parameters.max_movement_per_step); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.write(movement_parameters.maximum_correction); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.write(movement_parameters.speed_limit); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.write(movement_parameters.discontinuity_threshold); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.write(movement_parameters.collision_poly); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.write(movement_parameters.sticky_collision); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.write(movement_parameters.sticky_force); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.write(movement_parameters.air_friction); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.write(movement_parameters.liquid_friction); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.write(movement_parameters.ground_friction); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.write(movement_parameters.collision_enabled); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.write(movement_parameters.friction_enabled); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.write(movement_parameters.gravity_enabled); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.write(movement_parameters.ignore_platform_collision); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.write(movement_parameters.maximum_platform_correction); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.write(movement_parameters.maximum_platform_correction_velocity_factor); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.write(movement_parameters.physics_effect_categories); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.write(movement_parameters.rest_duration); !result) return std::unexpected(std::move(result.error()));

  return std::ref(ds);
}

MovementController::MovementController(Root& root) : m_root(root) {
  m_resting = false;

  m_time_step = root.timing().time_step;

  m_liquid_percentage = 0.0f;
  m_liquid_id = empty_liquid_id;

  m_x_position.set_fixed_point_base(0.0125f);
  m_y_position.set_fixed_point_base(0.0125f);
  m_x_velocity.set_fixed_point_base(0.00625f);
  m_y_velocity.set_fixed_point_base(0.00625f);
  m_rotation.set_fixed_point_base(0.01f);
  m_x_relative_surface_moving_collision_position.set_fixed_point_base(0.0125f);
  m_y_relative_surface_moving_collision_position.set_fixed_point_base(0.0125f);

  m_x_velocity.set_interpolator(lerp<float, float>);
  m_y_velocity.set_interpolator(lerp<float, float>);
  m_rotation.set_interpolator(angle_lerp<float, float>);
  m_x_relative_surface_moving_collision_position.set_interpolator(lerp<float, float>);
  m_y_relative_surface_moving_collision_position.set_interpolator(lerp<float, float>);

  m_world.reset();
}

std::expected<std::shared_ptr<MovementController>, StarException> MovementController::create(Root& root, MovementParameters const& parameters) {
  struct MakeShared final : MovementController {
    explicit MakeShared(Root& root) : MovementController(root) {}
  };
  auto controller = std::make_shared<MakeShared>(root);
  auto initialized = controller->initialize(parameters);
  if (!initialized) return std::unexpected(std::move(initialized.error()));
  return controller;
}

std::expected<void, StarException> MovementController::initialize(MovementParameters const& parameters) {
  if (auto result = add_net_element(m_collision_poly); !result) return std::unexpected(std::move(result.error()));
  if (auto result = add_net_element(m_mass); !result) return std::unexpected(std::move(result.error()));
  if (auto result = add_net_element(m_x_position); !result) return std::unexpected(std::move(result.error()));
  if (auto result = add_net_element(m_y_position); !result) return std::unexpected(std::move(result.error()));
  if (auto result = add_net_element(m_x_velocity); !result) return std::unexpected(std::move(result.error()));
  if (auto result = add_net_element(m_y_velocity); !result) return std::unexpected(std::move(result.error()));
  if (auto result = add_net_element(m_rotation); !result) return std::unexpected(std::move(result.error()));
  if (auto result = add_net_element(m_colliding); !result) return std::unexpected(std::move(result.error()));
  if (auto result = add_net_element(m_collision_stuck); !result) return std::unexpected(std::move(result.error()));
  if (auto result = add_net_element(m_null_colliding); !result) return std::unexpected(std::move(result.error()));
  if (auto result = add_net_element(m_sticking_direction); !result) return std::unexpected(std::move(result.error()));
  if (auto result = add_net_element(m_on_ground); !result) return std::unexpected(std::move(result.error()));
  if (auto result = add_net_element(m_zero_g); !result) return std::unexpected(std::move(result.error()));

  if (auto result = add_net_element(m_surface_moving_collision); !result) return std::unexpected(std::move(result.error()));
  if (auto result = add_net_element(m_x_relative_surface_moving_collision_position); !result) return std::unexpected(std::move(result.error()));
  if (auto result = add_net_element(m_y_relative_surface_moving_collision_position); !result) return std::unexpected(std::move(result.error()));

  return reset_parameters(parameters);
}

MovementParameters const& MovementController::parameters() const {
  return m_parameters;
}

std::expected<void, StarException> MovementController::apply_parameters(MovementParameters const& parameters) {
  // An engaged mass marks a set that has already passed full validation.
  if (!m_parameters.mass) {
    if (auto validated = validate_parameters(parameters); !validated)
      return validated;
    auto complete = parameters;
    m_parameters = std::move(complete);
    update_parameters(true);
    return {};
  }
  bool const refresh_interpolators = parameters.discontinuity_threshold
      && m_parameters.discontinuity_threshold != parameters.discontinuity_threshold;
  merge_fields(m_parameters, parameters);
  update_parameters(refresh_interpolators);
  return {};
}

std::expected<void, StarException> MovementController::reset_parameters(MovementParameters const& parameters) {
  auto defaults = MovementParameters::sensible_defaults(m_root);
  if (!defaults) return std::unexpected(std::move(defaults.error()));
  merge_fields(*defaults, parameters);
  if (auto validated = validate_parameters(*defaults); !validated)
    return validated;
  bool const refresh_interpolators = !m_parameters.mass
      || m_parameters.discontinuity_threshold != defaults->discontinuity_threshold;
  m_parameters = std::move(*defaults);
  update_parameters(refresh_interpolators);
  return {};
}

Root& MovementController::root() const {
  return m_root;
}

Json MovementController::store_state() const {
  return JsonObject{
    {"position", json_from_vec2f(position())},
    {"velocity", json_from_vec2f(velocity())},
    {"rotation", rotation()}
  };
}

std::expected<void, StarException> MovementController::load_state(Json const& state) {
  auto position_json = state.get("position");
  if (!position_json) return std::unexpected(std::move(position_json.error()));
  auto position_value = json_to_vec2f(*position_json);
  if (!position_value) return std::unexpected(std::move(position_value.error()));
  auto velocity_json = state.get("velocity");
  if (!velocity_json) return std::unexpected(std::move(velocity_json.error()));
  auto velocity_value = json_to_vec2f(*velocity_json);
  if (!velocity_value) return std::unexpected(std::move(velocity_value.error()));
  auto rotation_value = state.get_float("rotation");
  if (!rotation_value) return std::unexpected(std::move(rotation_value.error()));
  set_position(*position_value);
  set_velocity(*velocity_value);
  set_rotation(*rotation_value);
  return {};
}

float MovementController::mass() const {
  return m_mass.get();
}

PolyF const& MovementController::collision_poly() const {
  return m_collision_poly.get();
}

void MovementController::set_collision_poly(PolyF const& poly) {
  m_collision_poly.set(poly);
}

Vec2F MovementController::position() const {
  return {m_x_position.get(), m_y_position.get()};
}

float MovementController::x_position() const {
  return m_x_position.get();
}

float MovementController::y_position() const {
  return m_y_position.get();
}

Vec2F MovementController::velocity() const {
  return {m_x_velocity.get(), m_y_velocity.get()};
}

float MovementController::x_velocity() const {
  return m_x_velocity.get();
}

float MovementController::y_velocity() const {
  return m_y_velocity.get();
}

float MovementController::rotation() const {
  return m_rotation.get();
}

PolyF MovementController::collision_body() const {
  auto poly = collision_poly();
  poly.rotate(rotation());
  poly.translate(position());
  return poly;
}

RectF MovementController::local_bound_box() const {
  auto poly = collision_poly();
  poly.rotate(rotation());
  return poly.bounding_box();
}

RectF MovementController::collision_bound_box() const {
  return collision_body().bounding_box();
}

bool MovementController::is_colliding() const {
  return m_colliding.get();
}

bool MovementController::is_null_colliding() const {
  return m_null_colliding.get();
}

bool MovementController::is_collision_stuck() const {
  return m_collision_stuck.get();
}

std::optional<float> MovementController::sticking_direction() const {
  return m_sticking_direction.get();
}

float MovementController::liquid_percentage() const {
  return m_liquid_percentage;
}

LiquidId MovementController::liquid_id() const {
  return m_liquid_id;
}

bool MovementController::on_ground() const {
  return m_on_ground.get();
}

bool MovementController::zero_g() const {
  return m_zero_g.get();
}

bool MovementController::at_world_limit(bool bottom_only) const {
  if (m_world) {
    if (!collision_poly().empty()) {
      auto bounds = collision_bound_box();
      return bounds.y_min() <= 0 || (!bottom_only && bounds.y_max() >= m_world->get().geometry().height());
    } else {
      return y_position() <= 0 || (!bottom_only && y_position() >= m_world->get().geometry().height());
    }
  }
  return false;
}

void MovementController::set_position(Vec2F position) {
  if (m_world)
    position = m_world->get().geometry().limit(position);

  if (position[0] != m_x_position.get() || position[1] != m_y_position.get())
    m_resting = false;

  m_x_position.set(position[0]);
  m_y_position.set(position[1]);
}

void MovementController::set_x_position(float x_position) {
  set_position({x_position, y_position()});
}

void MovementController::set_y_position(float y_position) {
  set_position({x_position(), y_position});
}

void MovementController::translate(Vec2F const& direction) {
  set_position(position() + direction);
}

void MovementController::set_velocity(Vec2F vel) {
  if (m_parameters.speed_limit && vel.magnitude() > (*m_parameters.speed_limit)) {
    vel.normalize();
    vel *= (*m_parameters.speed_limit);
  }

  if ((velocity() - vel).magnitude() > 0.0001)
    m_resting = false;

  m_x_velocity.set(vel[0]);
  m_y_velocity.set(vel[1]);
}

void MovementController::set_x_velocity(float x_velocity) {
  set_velocity({x_velocity, y_velocity()});
}

void MovementController::set_y_velocity(float y_velocity) {
  set_velocity({x_velocity(), y_velocity});
}

void MovementController::add_momentum(Vec2F const& momentum) {
  set_velocity(velocity() + momentum / mass());
}

void MovementController::set_rotation(float rotation) {
  m_resting = false;

  m_rotation.set(rotation);
}

void MovementController::rotate(float rotation_rate) {
  if (rotation_rate == 0.0)
    return;

  m_resting = false;
  m_rotation.set(std::fmod(rotation() + rotation_rate * m_time_step, 2 * std::numbers::pi));
}

void MovementController::accelerate(Vec2F const& acceleration) {
  set_velocity(velocity() + acceleration * m_time_step);
}

void MovementController::force(Vec2F const& force) {
  set_velocity(velocity() + force / mass() * m_time_step);
}

void MovementController::approach_velocity(Vec2F const& target_velocity, float max_control_force) {
  // Instead of applying the force directly, work backwards and figure out the
  // maximum acceleration that could be achieved by the current control force,
  // and maximize the change in velocity based on that.

  Vec2F diff = target_velocity - velocity();
  float diff_magnitude = vmag(diff);

  if (diff_magnitude == 0.0f)
    return;

  float maximum_acceleration = max_control_force / mass() * m_time_step;
  float clamped_magnitude = clamp(diff_magnitude, 0.0f, maximum_acceleration);

  set_velocity(velocity() + diff * (clamped_magnitude / diff_magnitude));
}

void MovementController::approach_velocity_along_angle(float angle, float target_velocity, float max_control_force, bool positive_only) {
  // Same strategy as approach_velocity, work backwards to figure out the
  // maximum acceleration and apply that.

  // Project the current velocity along the axis normal, the velocity
  // difference is the difference between the target_velocity and this
  // projection.

  Vec2F axis = Vec2F::with_angle(angle, 1.0f);

  float velocity_along_axis = velocity() * axis;
  float diff = target_velocity - velocity_along_axis;
  if (diff == 0.0f)
    return;
  if (positive_only && diff < 0)
    return;

  float maximum_acceleration = max_control_force / mass() * m_time_step;

  float diff_magnitude = std::fabs(diff);
  float clamped_magnitude = clamp(diff_magnitude, 0.0f, maximum_acceleration);

  set_velocity(velocity() + axis * diff * (clamped_magnitude / diff_magnitude));
}

void MovementController::approach_x_velocity(float target_x_velocity, float max_control_force) {
  approach_velocity_along_angle(0.0f, target_x_velocity, max_control_force, false);
}

void MovementController::approach_y_velocity(float target_y_velocity, float max_control_force) {
  approach_velocity_along_angle(std::numbers::pi / 2, target_y_velocity, max_control_force, false);
}

void MovementController::init(World& world) {
  m_world = std::ref(world);
  set_position(position());
  update_position_interpolators();
}

void MovementController::uninit() {
  m_world.reset();
  update_position_interpolators();
}

void MovementController::set_timestep(float dt) {
  m_time_step = dt;
}

namespace {
std::expected<std::optional<Vec2F>, StarException> surface_position(World& world, std::optional<MovingCollisionId> const& id) {
  if (id) {
    if (auto entity = world.get<PhysicsEntity>(id->physics_entity_id)) {
      auto collision = entity->movingCollision(id->collision_index);
      if (!collision) return std::unexpected(std::move(collision.error()));
      if (*collision)
        return (*collision)->position;
    }
  }
  return {};
}
}

std::expected<void, StarException> MovementController::tick_master(float dt) {
  auto active_world = world();
  if (!active_world) return std::unexpected(std::move(active_world.error()));
  World& world_ref = active_world->get();
  set_timestep(dt);
  auto geometry = world_ref.geometry();

  bool zero_gravity = !(*m_parameters.gravity_enabled) || (*m_parameters.gravity_multiplier) == 0;
  if (!zero_gravity) {
    auto gravity_value = world_ref.gravity(position());
    if (!gravity_value) return std::unexpected(std::move(gravity_value.error()));
    zero_gravity = *gravity_value == 0;
  }
  m_zero_g.set(zero_gravity);

  auto previous_surface_id = m_surface_moving_collision.get();
  auto previous_surface_pos = surface_position(world_ref, previous_surface_id);
  if (!previous_surface_pos) return std::unexpected(std::move(previous_surface_pos.error()));
  Vec2F carried_velocity;
  if (*previous_surface_pos) {
    auto surface_delta = geometry.diff(**previous_surface_pos, m_surface_moving_collision_position);
    set_position(position() + surface_delta);
    if (dt == 0.0f) {
      // A zero-time update can sync the anchor but cannot measure velocity.
      carried_velocity = m_surface_velocity;
    } else {
      carried_velocity = surface_delta / dt;
      set_velocity(velocity() - m_surface_velocity + carried_velocity);
    }
  }

  if (m_resting) {
    if (m_rest_ticks <= 0)
      m_resting = false;
    else
      --m_rest_ticks;
  }

  // Skip integration and collision checks when resting.
  Vec2F relative_velocity = m_resting ? Vec2F() : velocity();
  Vec2F original_movement = relative_velocity * dt;
  relative_velocity -= carried_velocity;

  unsigned steps = 0;
  if (!m_resting) {
    steps = 1;
    float max_movement_per_step = *m_parameters.max_movement_per_step;
    if (max_movement_per_step > 0.0f) {
      float step_count = std::floor(vmag(relative_velocity) * dt / max_movement_per_step) + 1;
      if (!is_finite(step_count) || step_count < 1 || static_cast<double>(step_count) > std::numeric_limits<unsigned>::max())
        return std::unexpected(MovementControllerException("Movement substep count is out of range"));
      steps = static_cast<unsigned>(step_count);
    }
  }

  m_collision_correction = {};
  m_surface_slope = Vec2F(1, 0);
  std::optional<MovingCollisionId> current_surface_id;
  float step_dt = steps ? dt / steps : 0.0f;
  bool check_collision = *m_parameters.collision_enabled && !collision_poly().empty();
  for (unsigned i = 0; i < steps; ++i) {
    Vec2F movement = relative_velocity * step_dt;

    if (!check_collision) {
      set_position(position() + movement);
      m_colliding.set(false);
      m_collision_stuck.set(false);
      m_null_colliding.set(false);
      m_sticking_direction.set({});
      m_on_ground.set(false);

    } else {
      auto body = collision_body();

      float velocity_magnitude = vmag(relative_velocity);
      bool ignore_platforms = *m_parameters.ignore_platform_collision || relative_velocity[1] > 0;
      float maximum_correction = *m_parameters.maximum_correction;
      float maximum_platform_correction = *m_parameters.maximum_platform_correction
          + *m_parameters.maximum_platform_correction_velocity_factor * velocity_magnitude;
      Vec2F body_center = body.center();

      RectF query_bounds = body.bounding_box().padded(maximum_correction);
      query_bounds.combine(query_bounds.translated(movement));
      if (auto query = query_collisions(query_bounds); !query) return std::unexpected(std::move(query.error()));
      auto result = collision_move(m_working_collisions, body, movement, ignore_platforms, *m_parameters.enable_surface_slope_correction && !zero_g(),
          maximum_correction, maximum_platform_correction, body_center, step_dt);

      set_position(position() + result.movement);

      if (result.collision_kind == CollisionKind::Null) {
        m_null_colliding.set(true);
        break;
      } else {
        m_null_colliding.set(false);
      }

      Vec2F correction = result.correction;

      m_surface_slope = result.ground_slope;
      current_surface_id = result.surface_moving_collision_id;
      m_collision_correction += correction;
      m_colliding.set(correction != Vec2F() || result.is_stuck);
      m_on_ground.set(!zero_g() && result.on_ground);
      m_collision_stuck.set(result.is_stuck);

      // If we have collided, apply either sticky or normal (bouncing) collision physics
      if (correction != Vec2F()) {
        if ((*m_parameters.sticky_collision) && result.collision_kind != CollisionKind::Slippery) {
          // When sticking, cancel all velocity and apply sticky_force in the
          // opposite of the direction of collision correction.
          Vec2F norm_correction = vnorm(correction);
          relative_velocity = -norm_correction * *m_parameters.sticky_force / mass() * step_dt;
          m_sticking_direction.set(-norm_correction.angle());
          break;
        } else {
          m_sticking_direction.set({});

          if (*m_parameters.bounce_factor != 0.0f) {
            if (!(velocity_magnitude > 0.0f))
              continue;
            Vec2F velocity_direction = relative_velocity / velocity_magnitude;
            Vec2F correction_direction = correction / vmag(correction);
            Vec2F adjustment = correction_direction * (velocity_magnitude * (correction_direction * -velocity_direction));
            relative_velocity += adjustment + *m_parameters.bounce_factor * adjustment;
            if ((*m_parameters.stop_on_first_bounce)) {
              // When bouncing, stop integrating at the moment of bounce.  This
              // prevents the frame of contact from being missed due to multiple
              // iterations per frame.
              break;
            }
          } else if (dt != 0.0f) {
            // Only adjust the velocity to the extent that the collision was
            // caused by the velocity in each axis, to eliminate collision
            // induced velocity in a platformery way (each axis considered
            // independently).

            if (relative_velocity[0] < 0 && correction[0] > 0)
              relative_velocity[0] = std::min(0.0f, relative_velocity[0] + correction[0] / dt);
            else if (relative_velocity[0] > 0 && correction[0] < 0)
              relative_velocity[0] = std::max(0.0f, relative_velocity[0] + correction[0] / dt);

            if (relative_velocity[1] < 0 && correction[1] > 0)
              relative_velocity[1] = std::min(0.0f, relative_velocity[1] + correction[1] / dt);
            else if (relative_velocity[1] > 0 && correction[1] < 0)
              relative_velocity[1] = std::max(0.0f, relative_velocity[1] + correction[1] / dt);
          }
        }
      }
    }
  }
  
  if (!m_colliding.get())
    m_sticking_direction.set({});

  // Keep the velocity inherited from the previous platform even if we no
  // longer use that platform as the frame of reference for ground friction.
  Vec2F new_velocity = relative_velocity + carried_velocity;
  auto const final_pos = position();
  auto const final_body = collision_body();
  auto const final_bounds = final_body.empty() ? RectF(final_pos, final_pos) : final_body.bounding_box();
  if (auto result = sample_liquid(world_ref, final_pos, final_bounds); !result)
    return std::unexpected(std::move(result.error()));

  auto current_surface_pos = surface_position(world_ref, current_surface_id);
  if (!current_surface_pos) return std::unexpected(std::move(current_surface_pos.error()));
  m_surface_moving_collision_position = current_surface_pos->value_or(Vec2F());
  m_surface_velocity = {};
  if (*current_surface_pos) {
    if (*previous_surface_pos && *previous_surface_id == *current_surface_id)
      m_surface_velocity = carried_velocity;
    m_x_relative_surface_moving_collision_position.set(geometry.diff(x_position(), m_surface_moving_collision_position[0]));
    m_y_relative_surface_moving_collision_position.set(y_position() - m_surface_moving_collision_position[1]);
  } else {
    current_surface_id.reset();
  }
  m_surface_moving_collision.set(current_surface_id);

  auto calculate_final_gravity = [&]() -> std::expected<float, StarException> {
    float buoyancy = *m_parameters.liquid_buoyancy * m_liquid_percentage + *m_parameters.air_buoyancy * (1.0f - liquid_percentage());
    auto gravity_value = world_ref.gravity(final_pos);
    if (!gravity_value) return std::unexpected(std::move(gravity_value.error()));
    return *gravity_value * *m_parameters.gravity_multiplier * (1.0f - buoyancy);
  };
  std::optional<float> final_gravity;

  // In order to make control work accurately, passive forces need to be
  // applied to velocity *after* integrating.  This prevents control from
  // having to account for one timestep of passive forces in order to result
  // in the correct controlled movement.
  if (!zero_g() && !sticking_direction()) {
    auto gravity = calculate_final_gravity();
    if (!gravity) return std::unexpected(std::move(gravity.error()));
    final_gravity = *gravity;
    Vec2F environment_velocity;
    environment_velocity[1] -= *final_gravity * dt;

    if (on_ground() && (*m_parameters.slope_sliding_factor) != 0 && m_surface_slope[1] != 0)
      environment_velocity += -m_surface_slope * (m_surface_slope[0] * m_surface_slope[1]) * *m_parameters.slope_sliding_factor;

    new_velocity += environment_velocity;
  }

  // If original movement was entirely (almost) in the direction of gravity
  // and was entirely (almost) cancelled by collision correction, put the
  // entity into rest for rest_duration
  if (!m_resting && !sticking_direction() && std::abs(original_movement[0]) < 0.0001) {
    if (!final_gravity) {
      auto gravity = calculate_final_gravity();
      if (!gravity) return std::unexpected(std::move(gravity.error()));
      final_gravity = *gravity;
    }
    if (original_movement[1] * *final_gravity <= 0.0 && std::abs(original_movement[1] + m_collision_correction[1]) < 0.0001) {
      m_resting = true;
      m_rest_ticks = m_parameters.rest_duration.value_or(0);
    }
  }

  if ((*m_parameters.friction_enabled)) {
    Vec2F reference_velocity;
    float friction = liquid_percentage() * (*m_parameters.liquid_friction) + (1.0f - liquid_percentage()) * (*m_parameters.air_friction);
    if (on_ground()) {
      friction = std::max(friction, (*m_parameters.ground_friction));
      reference_velocity = m_surface_velocity;
    }

    // The equation for friction here is effectively:
    // frictionForce = friction * (refVel - velocity)
    // but it is applied here as a multiplicitave factor from [0, 1] so it does
    // not induce oscillation at very high friction and so it cannot be
    // negative.
    float friction_factor = std::clamp(friction / mass() * dt, 0.0f, 1.0f);
    new_velocity = lerp(friction_factor, new_velocity, reference_velocity);
  }

  set_velocity(new_velocity);
  return apply_force_regions(world_ref, final_pos, final_body, final_bounds);
}

std::expected<void, StarException> MovementController::tick_slave(float dt) {
  auto active_world = world();
  if (!active_world) return std::unexpected(std::move(active_world.error()));
  World& world_ref = active_world->get();
  set_timestep(dt);
  auto surface_pos = surface_position(world_ref, m_surface_moving_collision.get());
  if (!surface_pos) return std::unexpected(std::move(surface_pos.error()));
  if (*surface_pos) {
    m_x_position.set(m_x_relative_surface_moving_collision_position.get() + (**surface_pos)[0]);
    m_y_position.set(m_y_relative_surface_moving_collision_position.get() + (**surface_pos)[1]);
  }

  return sample_liquid(world_ref, position(), collision_body().bounding_box());
}

void MovementController::set_ignore_physics_entities(Set<EntityId> ignore_physics_entities) {
  m_ignore_physics_entities = std::move(ignore_physics_entities);
}

std::expected<void, StarException> MovementController::for_each_moving_collision(RectF const& region, MovingCollisionCallback const& callback) {
  auto active_world = world();
  if (!active_world) return std::unexpected(std::move(active_world.error()));
  World& world_ref = active_world->get();
  auto geometry = world_ref.geometry();
  auto physics_entities = world_ref.query<PhysicsEntity>(region);
  if (!physics_entities) return std::unexpected(std::move(physics_entities.error()));
  static StringSet const empty_categories;
  for (auto const& entity : *physics_entities) {
    if (m_ignore_physics_entities.contains(entity->entityId()))
      continue;
    for (std::size_t i = 0; i < entity->movingCollisionCount(); ++i) {
      auto collision = entity->movingCollision(i);
      if (!collision) return std::unexpected(std::move(collision.error()));
      if (!*collision) continue;
      auto& candidate = **collision;
      auto const& categories = m_parameters.physics_effect_categories;
      if (!candidate.categoryFilter.check(categories ? *categories : empty_categories))
        continue;
      auto& poly = candidate.collision;
      poly.translate(geometry.nearest_to(region.min(), candidate.position));
      auto bounds = poly.bounding_box();
      if (!region.intersects(bounds)) continue;
      auto keep_going = callback({entity->entityId(), i}, candidate.collisionKind, std::move(poly), bounds);
      if (!keep_going) return std::unexpected(std::move(keep_going.error()));
      if (!*keep_going) return {};
    }
  }
  return {};
}

namespace {
// Keep normalization and projection as two rounded operations. Inlining across
// the zero-length guard lets fast-math cancel both lengths into a squared length.
[[clang::noinline]] float gradient_falloff(Line2F const& gradient, Vec2F const& offset) {
  auto normal = gradient.direction();
  float projected = offset[0] * normal[0] + offset[1] * normal[1];
  return 1.0 - std::clamp(projected / gradient.length(), -1.0f, 1.0f);
}
}

std::expected<void, StarException> MovementController::update_force_regions() {
  auto active_world = world();
  if (!active_world) return std::unexpected(std::move(active_world.error()));
  World& world_ref = active_world->get();
  auto const pos = position();
  auto const body = collision_body();
  // An empty collision polygon still has a position for point-based radial forces.
  auto const bounds = body.empty() ? RectF(pos, pos) : body.bounding_box();
  return apply_force_regions(world_ref, pos, body, bounds);
}

std::expected<void, StarException> MovementController::apply_force_regions(World& world_ref, Vec2F const& pos, PolyF const& body, RectF const& bounds) {
  auto const geometry = world_ref.geometry();
  static StringSet const empty_categories;
  auto const& categories = m_parameters.physics_effect_categories ? *m_parameters.physics_effect_categories : empty_categories;
  std::optional<float> body_area;
  m_applied_force_region = false;

  auto apply_force = [&]<physics_force_region Region>(Region const& region) -> std::expected<bool, StarException> {
    if (!region.category_filter.check(categories))
      return false;
    auto hit = geometry.rect_intersects_rect(bounds, region.bounding_box());
    if (!hit || !*hit)
      return hit;

    if constexpr (std::same_as<Region, RadialForceRegion>) {
      auto direction = geometry.diff(pos, region.center);
      float distance = vmag(direction);
      if (!(distance > 0 && distance < region.outer_radius) || region.inner_radius < 0 || region.inner_radius > region.outer_radius)
        return false;
      // At equal radii only the inner ramp exists; at zero inner radius only the outer ramp exists.
      float incidence = distance < region.inner_radius
          ? distance / region.inner_radius
          : 1.0f - (distance - region.inner_radius) / (region.outer_radius - region.inner_radius);
      float control = region.control_force * incidence;
      if (!(control > 0))
        return false;
      if (region.target_radial_velocity < 0)
        direction = -direction;
      approach_velocity_along_angle(direction.angle(), std::abs(region.target_radial_velocity), control, true);
    } else {
      if constexpr (std::same_as<Region, DirectionalForceRegion>) {
        if ((!region.x_target_velocity && !region.y_target_velocity) || region.control_force == 0)
          return false;
      }
      if (!body_area)
        body_area = body.signed_area();
      if (*body_area == 0)
        return false;
      auto overlap = geometry.poly_overlap_area(region.region, body);
      if (!overlap) return std::unexpected(std::move(overlap.error()));
      if (*overlap == 0)
        return false;
      if constexpr (std::same_as<Region, DirectionalForceRegion>) {
        float overlap_factor = *overlap / *body_area;
        float control = region.control_force * overlap_factor;
        if (!(control > 0))
          return false;
        if (region.x_target_velocity)
          approach_x_velocity(*region.x_target_velocity, control);
        if (region.y_target_velocity)
          approach_y_velocity(*region.y_target_velocity, control);
      } else {
        if (!(region.gradient.length() > 0))
          return false;
        float falloff = gradient_falloff(region.gradient, geometry.diff(pos, region.gradient.min()));
        float effect = (*overlap * falloff) / *body_area;
        float control = region.base_control_force * effect;
        if (!(control > 0))
          return false;
        approach_velocity_along_angle(region.gradient.angle(), region.base_target_velocity * effect, control, true);
      }
    }
    return true;
  };

  auto apply_forces = [&](std::span<PhysicsForceRegion const> forces) -> std::expected<void, StarException> {
    for (auto const& force : forces) {
      auto applied = std::visit(apply_force, force);
      if (!applied) return std::unexpected(std::move(applied.error()));
      m_applied_force_region |= *applied;
    }
    return {};
  };
  auto entities = world_ref.query<PhysicsEntity>(bounds);
  if (!entities) return std::unexpected(std::move(entities.error()));
  for (auto const& entity : *entities) {
    if (m_ignore_physics_entities.contains(entity->entityId()))
      continue;
    auto forces = entity->force_regions();
    if (!forces) return std::unexpected(std::move(forces.error()));
    if (auto result = apply_forces(*forces); !result) return std::unexpected(std::move(result.error()));
  }
  return apply_forces(world_ref.force_regions());
}

std::expected<void, StarException> MovementController::update_liquid_percentage() {
  auto active_world = world();
  if (!active_world) return std::unexpected(std::move(active_world.error()));
  return sample_liquid(active_world->get(), position(), collision_body().bounding_box());
}

std::expected<void, StarException> MovementController::sample_liquid(World& world_ref, Vec2F const& pos, RectF const& bounds) {
  LiquidLevel level;
  if (bounds.is_empty()) {
    level = world_ref.liquidLevel(Vec2I::floor(pos));
  } else {
    auto liquid = world_ref.liquidLevel(bounds);
    if (!liquid) return std::unexpected(std::move(liquid.error()));
    level = *liquid;
  }

  m_liquid_percentage = std::clamp(level.level, 0.0f, 1.0f);
  m_liquid_id = level.liquid;
  return {};
}

void MovementController::set_on_ground(bool on_ground) {
  m_on_ground.set(on_ground);
}

bool MovementController::applied_force_region() const {
  return m_applied_force_region;
}

Vec2F MovementController::collision_correction() const {
  return m_collision_correction;
}

Vec2F MovementController::surface_slope() const {
  return m_surface_slope;
}

Vec2F MovementController::surface_velocity() const {
  return m_surface_velocity;
}

std::expected<std::reference_wrapper<World>, StarException> MovementController::world() {
  if (!m_world)
    return std::unexpected(MovementControllerException("MovementController not initialized!"));
  return *m_world;
}

CollisionKind MovementController::max_or_null_collision(CollisionKind a, CollisionKind b) {
  if (a == CollisionKind::Null || b == CollisionKind::Null)
    return CollisionKind::Null;
  else
    return std::max(a, b);
}

MovementController::CollisionResult MovementController::collision_move(List<CollisionPoly>& collision_polys, PolyF const& body, Vec2F const& movement,
    bool ignore_platforms, bool enable_surface_slope_correction, float maximum_correction, float maximum_platform_correction, Vec2F sort_center, float dt) {
  constexpr unsigned maximum_separation_loops = 3;
  constexpr float slide_angle = std::numbers::pi / 3;
  constexpr float slide_correction_limit = 0.2f;
  float separation_tolerance = 0.001f * (dt * 60.f);
  maximum_platform_correction *= (dt * 60.f);

  if (body.empty())
    return {movement, Vec2F(), {}, false, false, Vec2F(1, 0), CollisionKind::None};

  // The center and candidate positions stay fixed through all separation passes.
  // Cache their distances, but not an unstable sort's permutation of equal keys.
  for (auto& cp : collision_polys)
    cp.sort_distance = vmag_squared(cp.sort_position - sort_center);

  PolyF check_body = body;
  check_body.translate(movement);
  PolyF scratch;
  Vec2F total_correction{};
  CollisionKind max_collided = CollisionKind::None;
  std::optional<MovingCollisionId> surface_moving_collision_id;

  CollisionSeparation separation = {};
  bool first_separation = true;
  std::optional<bool> repeat_sort;
  auto separate = [&](bool platforms_ignored, bool upward) {
    bool sort_required = std::exchange(first_separation, false);
    if (!sort_required) {
      // Strictly increasing keys have exactly one order. Ties and unordered
      // comparisons must retain every legacy sort, including its tie reshuffle.
      if (!repeat_sort.has_value()) {
        repeat_sort = std::adjacent_find(collision_polys.begin(), collision_polys.end(),
            [](CollisionPoly const& a, CollisionPoly const& b) { return !(a.sort_distance < b.sort_distance); }) != collision_polys.end();
      }
      sort_required = *repeat_sort;
    }
    if (sort_required) {
      std::sort(collision_polys.begin(), collision_polys.end(),
          [](CollisionPoly const& a, CollisionPoly const& b) { return a.sort_distance < b.sort_distance; });
    }
    return collision_separate(collision_polys, check_body, platforms_ignored, maximum_platform_correction, upward,
        separation_tolerance, scratch);
  };

  if (enable_surface_slope_correction) {
    // Try separating upward first. Leave the translated body intact for the
    // normal pass unless the slope correction is accepted.
    separation = separate(ignore_platforms, true);
    total_correction += separation.correction;
    max_collided = max_or_null_collision(max_collided, separation.collision_kind);
    surface_moving_collision_id = separation.moving_collision_id;
    if (separation.solution_found) {
      Vec2F upward_result = movement + separation.correction;
      float up_mag = upward_result.magnitude();
      if (!(up_mag < slide_correction_limit)) {
        float angle_horiz = std::min(Vec2F(1, 0).angle_between_normalized(upward_result / up_mag),
                                    Vec2F(-1, 0).angle_between_normalized(upward_result / up_mag));
        separation.solution_found = angle_horiz < slide_angle;
      }
      if (separation.solution_found && total_correction.magnitude() > maximum_correction)
        separation.solution_found = false;
      if (separation.solution_found)
        check_body.translate(separation.correction);
    }
  }

  if (!separation.solution_found) {
    total_correction = Vec2F();
    for (std::size_t i = 0; i < maximum_separation_loops; ++i) {
      separation = separate(ignore_platforms, false);
      total_correction += separation.correction;
      check_body.translate(separation.correction);
      max_collided = max_or_null_collision(max_collided, separation.collision_kind);
      surface_moving_collision_id = {};

      if (total_correction.magnitude() > maximum_correction) {
        separation.solution_found = false;
        break;
      }
      if (separation.solution_found)
        break;
    }
  }

  if (!separation.solution_found && movement != Vec2F()) {
    // Retry from the original body, ignoring platforms.
    check_body = body;
    total_correction = -movement;
    for (std::size_t i = 0; i < maximum_separation_loops; ++i) {
      separation = separate(true, false);
      total_correction += separation.correction;
      check_body.translate(separation.correction);
      max_collided = max_or_null_collision(max_collided, separation.collision_kind);

      if (total_correction.magnitude() > maximum_correction) {
        separation.solution_found = false;
        break;
      }
      if (separation.solution_found)
        break;
    }
  }

  if (!separation.solution_found)
    return {Vec2F(), -movement, {}, true, true, Vec2F(1, 0), max_collided};

  CollisionResult result;
  result.movement = movement + total_correction;
  result.correction = total_correction;
  result.on_ground = result.correction[1] > separation_tolerance;
  result.surface_moving_collision_id = surface_moving_collision_id;
  result.collision_kind = max_collided;
  result.ground_slope = Vec2F(1, 0);
  if (result.on_ground) {
    // Ground slope comes from the touching terrain edge with the greatest
    // horizontal overlap, not from an edge of the moving body.
    float max_side_horizontal_overlap = 0.0f;
    RectF touching_bounds = check_body.bounding_box();
    touching_bounds.pad(separation_tolerance);
    for (auto const& cp : collision_polys) {
      if (!cp.poly_bounds.intersects(touching_bounds))
        continue;

      for (std::size_t i = 0; i < cp.poly.sides(); ++i) {
        auto side = cp.poly.side(i);
        RectF side_bounds = RectF::bound_box_of(side.min(), side.max());
        float side_horizontal_overlap = side_bounds.overlap(touching_bounds).width();
        if (side_horizontal_overlap > max_side_horizontal_overlap) {
          for (auto const& body_vertex : check_body) {
            float t = std::clamp(side.line_projection(body_vertex), 0.0f, 1.0f);
            Vec2F near_point = side.eval(t);
            if (near_point[1] > cp.sort_position[1]) {
              // Keep the original vertex walk: an early break changes Release
              // vectorization and the published slope by one ULP on flat edges.
              if (vmag_squared(body_vertex - near_point) <= square(separation_tolerance)) {
                max_side_horizontal_overlap = side_horizontal_overlap;
                result.ground_slope = side.diff().normalized();
                if (result.ground_slope[0] < 0)
                  result.ground_slope *= -1;
              }
            }
          }
        }
      }
    }
  }
  return result;
}

MovementController::CollisionSeparation MovementController::collision_separate(List<CollisionPoly> const& collision_polys,
    PolyF const& poly, bool ignore_platforms, float maximum_platform_correction, bool upward, float separation_tolerance, PolyF& scratch) {
  CollisionSeparation separation = {};
  separation.collision_kind = CollisionKind::None;
  separation.solution_found = true;
  if (collision_polys.empty())
    return separation;

  bool corrected = false;
  RectF corrected_bounds = poly.bounding_box();
  for (auto const& cp : collision_polys) {
    if ((ignore_platforms && cp.collision_kind == CollisionKind::Platform) || !corrected_bounds.intersects(cp.poly_bounds, false))
      continue;

    auto const& current = corrected ? scratch : poly;
    PolyF::IntersectResult hit;
    if (upward)
      hit = current.directional_sat_intersection(cp.poly, Vec2F(0, 1), false);
    else if (cp.collision_kind == CollisionKind::Platform)
      hit = current.directional_sat_intersection(cp.poly, Vec2F(0, 1), true);
    else
      hit = current.sat_intersection(cp.poly);

    if (!hit.intersects || (cp.collision_kind == CollisionKind::Platform
        && (hit.overlap[1] <= 0 || hit.overlap[1] > maximum_platform_correction)))
      continue;

    // Copy only at the first accepted correction, reusing capacity from any
    // previous pass. The caller moves its body once by the accumulated vector;
    // this scratch must instead receive each individual overlap in order.
    if (!corrected) {
      scratch = poly;
      corrected = true;
    }
    scratch.translate(hit.overlap);
    corrected_bounds = scratch.bounding_box();
    separation.correction += hit.overlap;
    if (cp.moving_collision_id)
      separation.moving_collision_id = cp.moving_collision_id;
    separation.collision_kind = max_or_null_collision(separation.collision_kind, cp.collision_kind);
  }

  if (corrected) {
    float separation_tolerance_squared = square(separation_tolerance);
    for (auto const& cp : collision_polys) {
      if (cp.collision_kind == CollisionKind::Platform || !corrected_bounds.intersects(cp.poly_bounds, false))
        continue;
      auto hit = scratch.sat_intersection(cp.poly);
      if (hit.intersects && hit.overlap.magnitude_squared() > separation_tolerance_squared) {
        separation.collision_kind = max_or_null_collision(separation.collision_kind, cp.collision_kind);
        separation.solution_found = false;
        break;
      }
    }
  }
  return separation;
}

std::expected<void, StarException> MovementController::validate_parameters(MovementParameters const& parameters) {
  if (!parameters.speed_limit) return std::unexpected(MovementControllerException("Missing movement parameter 'speedLimit'"));
  if (!parameters.gravity_enabled) return std::unexpected(MovementControllerException("Missing movement parameter 'gravityEnabled'"));
  if (!parameters.gravity_multiplier) return std::unexpected(MovementControllerException("Missing movement parameter 'gravityMultiplier'"));
  if (!parameters.max_movement_per_step) return std::unexpected(MovementControllerException("Missing movement parameter 'maxMovementPerStep'"));
  if (!parameters.collision_enabled) return std::unexpected(MovementControllerException("Missing movement parameter 'collisionEnabled'"));
  if (!parameters.ignore_platform_collision) return std::unexpected(MovementControllerException("Missing movement parameter 'ignorePlatformCollision'"));
  if (!parameters.maximum_correction) return std::unexpected(MovementControllerException("Missing movement parameter 'maximumCorrection'"));
  if (!parameters.maximum_platform_correction) return std::unexpected(MovementControllerException("Missing movement parameter 'maximumPlatformCorrection'"));
  if (!parameters.maximum_platform_correction_velocity_factor) return std::unexpected(MovementControllerException("Missing movement parameter 'maximumPlatformCorrectionVelocityFactor'"));
  if (!parameters.enable_surface_slope_correction) return std::unexpected(MovementControllerException("Missing movement parameter 'enableSurfaceSlopeCorrection'"));
  if (!parameters.sticky_collision) return std::unexpected(MovementControllerException("Missing movement parameter 'stickyCollision'"));
  if (!parameters.sticky_force) return std::unexpected(MovementControllerException("Missing movement parameter 'stickyForce'"));
  if (!parameters.bounce_factor) return std::unexpected(MovementControllerException("Missing movement parameter 'bounceFactor'"));
  if (!parameters.stop_on_first_bounce) return std::unexpected(MovementControllerException("Missing movement parameter 'stopOnFirstBounce'"));
  if (!parameters.liquid_buoyancy) return std::unexpected(MovementControllerException("Missing movement parameter 'liquidBuoyancy'"));
  if (!parameters.air_buoyancy) return std::unexpected(MovementControllerException("Missing movement parameter 'airBuoyancy'"));
  if (!parameters.slope_sliding_factor) return std::unexpected(MovementControllerException("Missing movement parameter 'slopeSlidingFactor'"));
  if (!parameters.friction_enabled) return std::unexpected(MovementControllerException("Missing movement parameter 'frictionEnabled'"));
  if (!parameters.liquid_friction) return std::unexpected(MovementControllerException("Missing movement parameter 'liquidFriction'"));
  if (!parameters.air_friction) return std::unexpected(MovementControllerException("Missing movement parameter 'airFriction'"));
  if (!parameters.ground_friction) return std::unexpected(MovementControllerException("Missing movement parameter 'groundFriction'"));
  if (!parameters.collision_poly) return std::unexpected(MovementControllerException("Missing movement parameter 'collisionPoly'"));
  if (!parameters.mass) return std::unexpected(MovementControllerException("Missing movement parameter 'mass'"));
  return {};
}

void MovementController::update_parameters(bool refresh_interpolators) {
  // Live network fields can diverge from their configured values. Restore
  // them even for a patch that omits collision_poly and mass.
  m_collision_poly.set(*m_parameters.collision_poly);
  m_mass.set(*m_parameters.mass);
  if (refresh_interpolators)
    update_position_interpolators();
}

void MovementController::update_position_interpolators() {
  if (m_world)
    m_x_position.set_interpolator(m_world->get().geometry().x_lerp_function(m_parameters.discontinuity_threshold));
  else
    m_x_position.set_interpolator(
      [limit = m_parameters.discontinuity_threshold](float const& offset, float const& from, float const& to) {
        return lerp_with_limit<float, float>(limit, offset, from, to);
      });
  m_y_position.set_interpolator(
    [limit = m_parameters.discontinuity_threshold](float const& offset, float const& from, float const& to) {
      return lerp_with_limit<float, float>(limit, offset, from, to);
    });
}

std::expected<void, StarException> MovementController::query_collisions(RectF const& region) {
  auto active_world = world();
  if (!active_world) return std::unexpected(std::move(active_world.error()));
  World& world_ref = active_world->get();
  while (!m_working_collisions.empty()) {
    auto& candidate = m_working_collisions.back();
    if (!candidate.moving_collision_id)
      m_collision_buffers.push_back(std::move(candidate.poly));
    m_working_collisions.pop_back();
  }

  auto geometry = world_ref.geometry();
  auto blocks = world_ref.for_each_collision_block(RectI::integral(region.padded(1)), [&](CollisionBlock const& block) -> std::expected<void, StarException> {
    if (block.kind == CollisionKind::None || block.poly.empty())
      return {};
    RectF bounds = block.poly_bounds;
    Vec2F base_position = block.poly.vertex(0);
    Vec2F translation = geometry.nearest_to(region.min(), base_position) - base_position;
    bounds.translate(translation);
    if (region.intersects(bounds)) {
      auto& candidate = m_working_collisions.emplace_back();
      if (!m_collision_buffers.empty()) {
        candidate.poly = std::move(m_collision_buffers.back());
        m_collision_buffers.pop_back();
      }
      candidate.poly = block.poly;
      candidate.poly.translate(translation);
      candidate.poly_bounds = bounds;
      candidate.sort_position = center_of_tile(block.space);
      candidate.collision_kind = block.kind;
    }
    return {};
  });
  if (!blocks) return std::unexpected(std::move(blocks.error()));

  return for_each_moving_collision(region, [&](MovingCollisionId id, CollisionKind kind, PolyF&& poly, RectF const& bounds) -> std::expected<bool, StarException> {
    auto center = poly.center();
    m_working_collisions.emplace_back(CollisionPoly{std::move(poly), bounds, center, id, kind, {}});
    return true;
  });
}

}

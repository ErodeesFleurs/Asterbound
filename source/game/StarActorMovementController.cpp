import star.json;
import star.json_extra;
import star.id_map;
import star.bi_map;
import star.a_star;
import star.poly;
import star.casting;
import star.data_stream;
import star.strong_typedef;
import star.list;
import star.rpc_promise;
import star.vector;
import star.rect;
import star.game_types;
import star.data_stream_extra;
import star.ordered_map;
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
import star.format;
import star.math_common;
import star.random;
import star.periodic;
import star.periodic_function;
import star.set;
import star.color;
import star.lua;
import star.audio;
import star.map;
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
import star.platformer_astar_types;

import star.physics_entity;
import star.movement_controller;
import star.anchorable_entity;

import star.game_timers;
import star.actor_movement_controller;
import star.asset_source;
import star.assets;
import star.configuration;
import star.root;
import star.lua_components;
import star.lua_animation_component;
import star.status_effect_entity;
import star.scripted_entity;
import star.chat_action;
import star.chatty_entity;
import star.wiring;
import star.wire_entity;
import star.inspectable_entity;
import star.animated_part_set;
import star.drawable;
import star.animation;
import star.particle;
import star.mixer;
import star.networked_animator;
import star.entity_rendering_types;
import star.entity_rendering;
import star.object;

import star.platformer_astar;

import star.algorithm;
namespace Star {

ActorJumpProfile::ActorJumpProfile() = default;

std::expected<ActorJumpProfile, StarException> ActorJumpProfile::create(Json const& config) {
  ActorJumpProfile value;
  auto jump_speed_result = config.opt_float("jumpSpeed");
  if (!jump_speed_result) return std::unexpected(std::move(jump_speed_result.error()));
  value.jump_speed = *jump_speed_result;
  auto jump_control_force_result = config.opt_float("jumpControlForce");
  if (!jump_control_force_result) return std::unexpected(std::move(jump_control_force_result.error()));
  value.jump_control_force = *jump_control_force_result;
  auto jump_initial_percentage_result = config.opt_float("jumpInitialPercentage");
  if (!jump_initial_percentage_result) return std::unexpected(std::move(jump_initial_percentage_result.error()));
  value.jump_initial_percentage = *jump_initial_percentage_result;
  auto jump_hold_time_result = config.opt_float("jumpHoldTime");
  if (!jump_hold_time_result) return std::unexpected(std::move(jump_hold_time_result.error()));
  value.jump_hold_time = *jump_hold_time_result;
  auto jump_total_hold_time_result = config.opt_float("jumpTotalHoldTime");
  if (!jump_total_hold_time_result) return std::unexpected(std::move(jump_total_hold_time_result.error()));
  value.jump_total_hold_time = *jump_total_hold_time_result;
  auto multi_jump_result = config.opt_bool("multiJump");
  if (!multi_jump_result) return std::unexpected(std::move(multi_jump_result.error()));
  value.multi_jump = *multi_jump_result;
  auto re_jump_delay_result = config.opt_float("reJumpDelay");
  if (!re_jump_delay_result) return std::unexpected(std::move(re_jump_delay_result.error()));
  value.re_jump_delay = *re_jump_delay_result;
  auto auto_jump_result = config.opt_bool("autoJump");
  if (!auto_jump_result) return std::unexpected(std::move(auto_jump_result.error()));
  value.auto_jump = *auto_jump_result;
  auto collision_cancelled_result = config.opt_bool("collisionCancelled");
  if (!collision_cancelled_result) return std::unexpected(std::move(collision_cancelled_result.error()));
  value.collision_cancelled = *collision_cancelled_result;
  return value;
}

Json ActorJumpProfile::to_json() const {
  return JsonObject{
    {"jumpSpeed", json_from_optional(jump_speed)},
    {"jumpControlForce", json_from_optional(jump_control_force)},
    {"jumpInitialPercentage", json_from_optional(jump_initial_percentage)},
    {"jumpHoldTime", json_from_optional(jump_hold_time)},
    {"jumpTotalHoldTime", json_from_optional(jump_total_hold_time)},
    {"multiJump", json_from_optional(multi_jump)},
    {"reJumpDelay", json_from_optional(re_jump_delay)},
    {"autoJump", json_from_optional(auto_jump)},
    {"collisionCancelled", json_from_optional(collision_cancelled)}
  };
}

namespace {
using ActorWireResult = std::expected<std::reference_wrapper<DataStream>, StarException>;

template <bool writing, typename... Fields>
  requires ((writing && (stream_writable<Fields> && ...)) || (!writing && (stream_readable<Fields> && ...)))
ActorWireResult transfer_fields(DataStream& ds, Fields&... fields) {
  std::expected<void, StarException> result;
  auto transfer = [&](auto& field) {
    if constexpr (writing)
      result = ds.write(field);
    else
      result = ds.read(field);
    return result.has_value();
  };
  if (!(transfer(fields) && ...))
    return std::unexpected(std::move(result.error()));
  return std::ref(ds);
}

template <typename T>
  requires std::same_as<std::remove_const_t<T>, ActorJumpProfile>
ActorWireResult transfer_jump_profile(DataStream& ds, T& p) {
  return transfer_fields<std::is_const_v<T>>(ds, p.jump_speed, p.jump_control_force, p.jump_initial_percentage,
    p.jump_hold_time, p.jump_total_hold_time, p.multi_jump, p.re_jump_delay, p.auto_jump, p.collision_cancelled);
}
}

ActorWireResult operator>>(DataStream& ds, ActorJumpProfile& profile) {
  return transfer_jump_profile(ds, profile);
}

ActorWireResult operator<<(DataStream& ds, ActorJumpProfile const& profile) {
  return transfer_jump_profile(ds, profile);
}

std::expected<ActorMovementParameters, StarException> ActorMovementParameters::sensible_defaults(Root& root) {
  auto assets = root.assets();
  if (!assets) return std::unexpected(std::move(assets.error()));
  auto config = (*assets)->json("/default_actor_movement.config");
  if (!config) return std::unexpected(std::move(config.error()));
  auto object = config->to_object();
  if (!object) return std::unexpected(std::move(object.error()));
  return create(Json(std::move(*object)));
}

ActorMovementParameters::ActorMovementParameters() = default;

std::expected<ActorMovementParameters, StarException> ActorMovementParameters::create(Json const& config) {
  ActorMovementParameters value;
  if (config.is_null())
    return value;

  auto mass_result = config.opt_float("mass");
  if (!mass_result) return std::unexpected(std::move(mass_result.error()));
  value.mass = *mass_result;
  auto gravity_multiplier_result = config.opt_float("gravityMultiplier");
  if (!gravity_multiplier_result) return std::unexpected(std::move(gravity_multiplier_result.error()));
  value.gravity_multiplier = *gravity_multiplier_result;
  auto liquid_buoyancy_result = config.opt_float("liquidBuoyancy");
  if (!liquid_buoyancy_result) return std::unexpected(std::move(liquid_buoyancy_result.error()));
  value.liquid_buoyancy = *liquid_buoyancy_result;
  auto air_buoyancy_result = config.opt_float("airBuoyancy");
  if (!air_buoyancy_result) return std::unexpected(std::move(air_buoyancy_result.error()));
  value.air_buoyancy = *air_buoyancy_result;
  auto bounce_factor_result = config.opt_float("bounceFactor");
  if (!bounce_factor_result) return std::unexpected(std::move(bounce_factor_result.error()));
  value.bounce_factor = *bounce_factor_result;
  auto stop_on_first_bounce_result = config.opt_bool("stopOnFirstBounce");
  if (!stop_on_first_bounce_result) return std::unexpected(std::move(stop_on_first_bounce_result.error()));
  value.stop_on_first_bounce = *stop_on_first_bounce_result;
  auto enable_surface_slope_correction_result = config.opt_bool("enableSurfaceSlopeCorrection");
  if (!enable_surface_slope_correction_result) return std::unexpected(std::move(enable_surface_slope_correction_result.error()));
  value.enable_surface_slope_correction = *enable_surface_slope_correction_result;
  auto slope_sliding_factor_result = config.opt_float("slopeSlidingFactor");
  if (!slope_sliding_factor_result) return std::unexpected(std::move(slope_sliding_factor_result.error()));
  value.slope_sliding_factor = *slope_sliding_factor_result;
  auto max_movement_per_step_result = config.opt_float("maxMovementPerStep");
  if (!max_movement_per_step_result) return std::unexpected(std::move(max_movement_per_step_result.error()));
  value.max_movement_per_step = *max_movement_per_step_result;
  auto maximum_correction_result = config.opt_float("maximumCorrection");
  if (!maximum_correction_result) return std::unexpected(std::move(maximum_correction_result.error()));
  value.maximum_correction = *maximum_correction_result;
  auto speed_limit_result = config.opt_float("speedLimit");
  if (!speed_limit_result) return std::unexpected(std::move(speed_limit_result.error()));
  value.speed_limit = *speed_limit_result;

  // "collisionPoly" is used as a synonym for setting both the standing and
  // crouching polys.

  auto collision_poly_config_result = config.get("collisionPoly", {});
  if (!collision_poly_config_result) return std::unexpected(std::move(collision_poly_config_result.error()));
  auto const& collision_poly_config = *collision_poly_config_result;
  auto standing_poly_config_result = config.get("standingPoly", {});
  if (!standing_poly_config_result) return std::unexpected(std::move(standing_poly_config_result.error()));
  auto const& standing_poly_config = *standing_poly_config_result;
  auto crouching_poly_config_result = config.get("crouchingPoly", {});
  if (!crouching_poly_config_result) return std::unexpected(std::move(crouching_poly_config_result.error()));
  auto const& crouching_poly_config = *crouching_poly_config_result;

  if (!standing_poly_config.is_null())
{
    auto standing_poly_value = json_to_polyf(standing_poly_config);
    if (!standing_poly_value) return std::unexpected(std::move(standing_poly_value.error()));
    value.standing_poly = std::move(*standing_poly_value);
  }
  else if (!collision_poly_config.is_null())
{
    auto standing_poly_value = json_to_polyf(collision_poly_config);
    if (!standing_poly_value) return std::unexpected(std::move(standing_poly_value.error()));
    value.standing_poly = std::move(*standing_poly_value);
  }

  if (!crouching_poly_config.is_null())
{
    auto crouching_poly_value = json_to_polyf(crouching_poly_config);
    if (!crouching_poly_value) return std::unexpected(std::move(crouching_poly_value.error()));
    value.crouching_poly = std::move(*crouching_poly_value);
  }
  else if (!collision_poly_config.is_null())
{
    auto crouching_poly_value = json_to_polyf(collision_poly_config);
    if (!crouching_poly_value) return std::unexpected(std::move(crouching_poly_value.error()));
    value.crouching_poly = std::move(*crouching_poly_value);
  }

  auto sticky_collision_result = config.opt_bool("stickyCollision");
  if (!sticky_collision_result) return std::unexpected(std::move(sticky_collision_result.error()));
  value.sticky_collision = *sticky_collision_result;
  auto sticky_force_result = config.opt_float("stickyForce");
  if (!sticky_force_result) return std::unexpected(std::move(sticky_force_result.error()));
  value.sticky_force = *sticky_force_result;

  auto walk_speed_result = config.opt_float("walkSpeed");
  if (!walk_speed_result) return std::unexpected(std::move(walk_speed_result.error()));
  value.walk_speed = *walk_speed_result;
  auto run_speed_result = config.opt_float("runSpeed");
  if (!run_speed_result) return std::unexpected(std::move(run_speed_result.error()));
  value.run_speed = *run_speed_result;
  auto fly_speed_result = config.opt_float("flySpeed");
  if (!fly_speed_result) return std::unexpected(std::move(fly_speed_result.error()));
  value.fly_speed = *fly_speed_result;
  auto air_friction_result = config.opt_float("airFriction");
  if (!air_friction_result) return std::unexpected(std::move(air_friction_result.error()));
  value.air_friction = *air_friction_result;
  auto liquid_friction_result = config.opt_float("liquidFriction");
  if (!liquid_friction_result) return std::unexpected(std::move(liquid_friction_result.error()));
  value.liquid_friction = *liquid_friction_result;
  auto minimum_liquid_percentage_result = config.opt_float("minimumLiquidPercentage");
  if (!minimum_liquid_percentage_result) return std::unexpected(std::move(minimum_liquid_percentage_result.error()));
  value.minimum_liquid_percentage = *minimum_liquid_percentage_result;
  auto liquid_impedance_result = config.opt_float("liquidImpedance");
  if (!liquid_impedance_result) return std::unexpected(std::move(liquid_impedance_result.error()));
  value.liquid_impedance = *liquid_impedance_result;
  auto normal_ground_friction_result = config.opt_float("normalGroundFriction");
  if (!normal_ground_friction_result) return std::unexpected(std::move(normal_ground_friction_result.error()));
  value.normal_ground_friction = *normal_ground_friction_result;
  auto ambulating_ground_friction_result = config.opt_float("ambulatingGroundFriction");
  if (!ambulating_ground_friction_result) return std::unexpected(std::move(ambulating_ground_friction_result.error()));
  value.ambulating_ground_friction = *ambulating_ground_friction_result;
  auto ground_force_result = config.opt_float("groundForce");
  if (!ground_force_result) return std::unexpected(std::move(ground_force_result.error()));
  value.ground_force = *ground_force_result;
  auto air_force_result = config.opt_float("airForce");
  if (!air_force_result) return std::unexpected(std::move(air_force_result.error()));
  value.air_force = *air_force_result;
  auto liquid_force_result = config.opt_float("liquidForce");
  if (!liquid_force_result) return std::unexpected(std::move(liquid_force_result.error()));
  value.liquid_force = *liquid_force_result;

  auto air_jump_profile_config = config.opt("airJumpProfile");
  if (!air_jump_profile_config) return std::unexpected(std::move(air_jump_profile_config.error()));
  if (*air_jump_profile_config) {
    auto air_jump_profile_value = ActorJumpProfile::create(**air_jump_profile_config);
    if (!air_jump_profile_value) return std::unexpected(std::move(air_jump_profile_value.error()));
    value.air_jump_profile = std::move(*air_jump_profile_value);
  }
  auto liquid_jump_profile_config = config.opt("liquidJumpProfile");
  if (!liquid_jump_profile_config) return std::unexpected(std::move(liquid_jump_profile_config.error()));
  if (*liquid_jump_profile_config) {
    auto liquid_jump_profile_value = ActorJumpProfile::create(**liquid_jump_profile_config);
    if (!liquid_jump_profile_value) return std::unexpected(std::move(liquid_jump_profile_value.error()));
    value.liquid_jump_profile = std::move(*liquid_jump_profile_value);
  }

  auto fall_status_speed_min_result = config.opt_float("fallStatusSpeedMin");
  if (!fall_status_speed_min_result) return std::unexpected(std::move(fall_status_speed_min_result.error()));
  value.fall_status_speed_min = *fall_status_speed_min_result;
  auto fall_through_sustain_frames_result = config.opt_int("fallThroughSustainFrames");
  if (!fall_through_sustain_frames_result) return std::unexpected(std::move(fall_through_sustain_frames_result.error()));
  value.fall_through_sustain_frames = *fall_through_sustain_frames_result;
  auto maximum_platform_correction_result = config.opt_float("maximumPlatformCorrection");
  if (!maximum_platform_correction_result) return std::unexpected(std::move(maximum_platform_correction_result.error()));
  value.maximum_platform_correction = *maximum_platform_correction_result;
  auto maximum_platform_correction_velocity_factor_result = config.opt_float("maximumPlatformCorrectionVelocityFactor");
  if (!maximum_platform_correction_velocity_factor_result) return std::unexpected(std::move(maximum_platform_correction_velocity_factor_result.error()));
  value.maximum_platform_correction_velocity_factor = *maximum_platform_correction_velocity_factor_result;

  auto physics_effect_categories_config = config.opt("physicsEffectCategories");
  if (!physics_effect_categories_config) return std::unexpected(std::move(physics_effect_categories_config.error()));
  if (*physics_effect_categories_config) {
    auto physics_effect_categories_value = json_to_string_set(**physics_effect_categories_config);
    if (!physics_effect_categories_value) return std::unexpected(std::move(physics_effect_categories_value.error()));
    value.physics_effect_categories = std::move(*physics_effect_categories_value);
  }

  auto ground_movement_minimum_sustain_result = config.opt_float("groundMovementMinimumSustain");
  if (!ground_movement_minimum_sustain_result) return std::unexpected(std::move(ground_movement_minimum_sustain_result.error()));
  value.ground_movement_minimum_sustain = *ground_movement_minimum_sustain_result;
  auto ground_movement_maximum_sustain_result = config.opt_float("groundMovementMaximumSustain");
  if (!ground_movement_maximum_sustain_result) return std::unexpected(std::move(ground_movement_maximum_sustain_result.error()));
  value.ground_movement_maximum_sustain = *ground_movement_maximum_sustain_result;
  auto ground_movement_check_distance_result = config.opt_float("groundMovementCheckDistance");
  if (!ground_movement_check_distance_result) return std::unexpected(std::move(ground_movement_check_distance_result.error()));
  value.ground_movement_check_distance = *ground_movement_check_distance_result;

  auto collision_enabled_result = config.opt_bool("collisionEnabled");
  if (!collision_enabled_result) return std::unexpected(std::move(collision_enabled_result.error()));
  value.collision_enabled = *collision_enabled_result;
  auto friction_enabled_result = config.opt_bool("frictionEnabled");
  if (!friction_enabled_result) return std::unexpected(std::move(friction_enabled_result.error()));
  value.friction_enabled = *friction_enabled_result;
  auto gravity_enabled_result = config.opt_bool("gravityEnabled");
  if (!gravity_enabled_result) return std::unexpected(std::move(gravity_enabled_result.error()));
  value.gravity_enabled = *gravity_enabled_result;

  auto path_explore_rate_result = config.opt_float("pathExploreRate");
  if (!path_explore_rate_result) return std::unexpected(std::move(path_explore_rate_result.error()));
  value.path_explore_rate = *path_explore_rate_result;
  return value;
}

std::expected<Json, StarException> ActorMovementParameters::to_json() const {
  auto standing_poly_json = json_from_optional(standing_poly, json_from_polyf);
  if (!standing_poly_json) return std::unexpected(std::move(standing_poly_json.error()));
  auto crouching_poly_json = json_from_optional(crouching_poly, json_from_polyf);
  if (!crouching_poly_json) return std::unexpected(std::move(crouching_poly_json.error()));
  auto physics_effect_categories_json = json_from_optional(physics_effect_categories, json_from_string_set);
  if (!physics_effect_categories_json) return std::unexpected(std::move(physics_effect_categories_json.error()));

  return JsonObject{{"mass", json_from_optional(mass)},
      {"gravityMultiplier", json_from_optional(gravity_multiplier)},
      {"liquidBuoyancy", json_from_optional(liquid_buoyancy)},
      {"airBuoyancy", json_from_optional(air_buoyancy)},
      {"bounceFactor", json_from_optional(bounce_factor)},
      {"stopOnFirstBounce", json_from_optional(stop_on_first_bounce)},
      {"enableSurfaceSlopeCorrection", json_from_optional(enable_surface_slope_correction)},
      {"slopeSlidingFactor", json_from_optional(slope_sliding_factor)},
      {"maxMovementPerStep", json_from_optional(max_movement_per_step)},
      {"maximumCorrection", json_from_optional(maximum_correction)},
      {"speedLimit", json_from_optional(speed_limit)},
      {"standingPoly", *standing_poly_json},
      {"crouchingPoly", *crouching_poly_json},
      {"stickyCollision", json_from_optional(sticky_collision)},
      {"stickyForce", json_from_optional(sticky_force)},
      {"walkSpeed", json_from_optional(walk_speed)},
      {"runSpeed", json_from_optional(run_speed)},
      {"flySpeed", json_from_optional(fly_speed)},
      {"airFriction", json_from_optional(air_friction)},
      {"liquidFriction", json_from_optional(liquid_friction)},
      {"minimumLiquidPercentage", json_from_optional(minimum_liquid_percentage)},
      {"liquidImpedance", json_from_optional(liquid_impedance)},
      {"normalGroundFriction", json_from_optional(normal_ground_friction)},
      {"ambulatingGroundFriction", json_from_optional(ambulating_ground_friction)},
      {"groundForce", json_from_optional(ground_force)},
      {"airForce", json_from_optional(air_force)},
      {"liquidForce", json_from_optional(liquid_force)},
      {"airJumpProfile", air_jump_profile.to_json()},
      {"liquidJumpProfile", liquid_jump_profile.to_json()},
      {"fallStatusSpeedMin", json_from_optional(fall_status_speed_min)},
      {"fallThroughSustainFrames", json_from_optional(fall_through_sustain_frames)},
      {"maximumPlatformCorrection", json_from_optional(maximum_platform_correction)},
      {"maximumPlatformCorrectionVelocityFactor", json_from_optional(maximum_platform_correction_velocity_factor)},
      {"physicsEffectCategories", *physics_effect_categories_json},
      {"groundMovementMinimumSustain", json_from_optional(ground_movement_minimum_sustain)},
      {"groundMovementMaximumSustain", json_from_optional(ground_movement_maximum_sustain)},
      {"groundMovementCheckDistance", json_from_optional(ground_movement_check_distance)},
      {"collisionEnabled", json_from_optional(collision_enabled)},
      {"frictionEnabled", json_from_optional(friction_enabled)},
      {"gravityEnabled", json_from_optional(gravity_enabled)},
      {"pathExploreRate", json_from_optional(path_explore_rate)}};
}

namespace {
// A single patch writes only engaged fields. Resolving base + patch selects
// the source before copying, so overwritten polygons and sets are never copied.
template <class T, class Patch>
  requires std::same_as<std::remove_cvref_t<Patch>, std::optional<T>> && std::assignable_from<std::optional<T>&, Patch>
void merge_field(std::optional<T>& dest, Patch&& patch) {
  if (patch)
    dest = std::forward<Patch>(patch);
}

template <std::copyable T>
void merge_field(std::optional<T>& dest, std::optional<T> const& base, std::optional<T> const& patch) {
  dest = patch ? patch : base;
}

template <class T>
concept jump_profile_source = std::same_as<std::remove_cvref_t<T>, ActorJumpProfile>;

template <jump_profile_source... Sources>
  requires (sizeof...(Sources) == 1 || sizeof...(Sources) == 2)
void merge_fields(ActorJumpProfile& dest, Sources&&... sources) {
  merge_field(dest.jump_speed, sources.jump_speed...);
  merge_field(dest.jump_control_force, sources.jump_control_force...);
  merge_field(dest.jump_initial_percentage, sources.jump_initial_percentage...);
  merge_field(dest.jump_hold_time, sources.jump_hold_time...);
  merge_field(dest.jump_total_hold_time, sources.jump_total_hold_time...);
  merge_field(dest.multi_jump, sources.multi_jump...);
  merge_field(dest.re_jump_delay, sources.re_jump_delay...);
  merge_field(dest.auto_jump, sources.auto_jump...);
  merge_field(dest.collision_cancelled, sources.collision_cancelled...);
}

template <class T>
concept actor_parameter_source = std::same_as<std::remove_cvref_t<T>, ActorMovementParameters>;

template <actor_parameter_source... Sources>
  requires (sizeof...(Sources) == 1 || sizeof...(Sources) == 2)
void merge_fields(ActorMovementParameters& dest, Sources&&... sources) {
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
  merge_field(dest.standing_poly, std::forward<Sources>(sources).standing_poly...);
  merge_field(dest.crouching_poly, std::forward<Sources>(sources).crouching_poly...);
  merge_field(dest.sticky_collision, sources.sticky_collision...);
  merge_field(dest.sticky_force, sources.sticky_force...);
  merge_field(dest.walk_speed, sources.walk_speed...);
  merge_field(dest.run_speed, sources.run_speed...);
  merge_field(dest.fly_speed, sources.fly_speed...);
  merge_field(dest.air_friction, sources.air_friction...);
  merge_field(dest.liquid_friction, sources.liquid_friction...);
  merge_field(dest.minimum_liquid_percentage, sources.minimum_liquid_percentage...);
  merge_field(dest.liquid_impedance, sources.liquid_impedance...);
  merge_field(dest.normal_ground_friction, sources.normal_ground_friction...);
  merge_field(dest.ambulating_ground_friction, sources.ambulating_ground_friction...);
  merge_field(dest.ground_force, sources.ground_force...);
  merge_field(dest.air_force, sources.air_force...);
  merge_field(dest.liquid_force, sources.liquid_force...);
  merge_fields(dest.air_jump_profile, sources.air_jump_profile...);
  merge_fields(dest.liquid_jump_profile, sources.liquid_jump_profile...);
  merge_field(dest.fall_status_speed_min, sources.fall_status_speed_min...);
  merge_field(dest.fall_through_sustain_frames, sources.fall_through_sustain_frames...);
  merge_field(dest.maximum_platform_correction, sources.maximum_platform_correction...);
  merge_field(dest.maximum_platform_correction_velocity_factor, sources.maximum_platform_correction_velocity_factor...);
  merge_field(dest.physics_effect_categories, std::forward<Sources>(sources).physics_effect_categories...);
  merge_field(dest.ground_movement_minimum_sustain, sources.ground_movement_minimum_sustain...);
  merge_field(dest.ground_movement_maximum_sustain, sources.ground_movement_maximum_sustain...);
  merge_field(dest.ground_movement_check_distance, sources.ground_movement_check_distance...);
  merge_field(dest.collision_enabled, sources.collision_enabled...);
  merge_field(dest.friction_enabled, sources.friction_enabled...);
  merge_field(dest.gravity_enabled, sources.gravity_enabled...);
  merge_field(dest.path_explore_rate, sources.path_explore_rate...);
}
} // namespace

ActorMovementParameters ActorMovementParameters::merge(ActorMovementParameters const& patch) const {
  ActorMovementParameters merged;
  merge_fields(merged, *this, patch);
  return merged;
}

void ActorMovementParameters::merge_from(ActorMovementParameters const& patch) {
  merge_fields(*this, patch);
}

void ActorMovementParameters::merge_from(ActorMovementParameters&& patch) {
  merge_fields(*this, std::move(patch));
}

namespace {
template <typename T>
  requires std::same_as<std::remove_const_t<T>, ActorMovementParameters>
ActorWireResult transfer_movement_parameters(DataStream& ds, T& p) {
  // The three groundMovement sustain/distance settings are JSON-only.
  return transfer_fields<std::is_const_v<T>>(ds, p.mass, p.gravity_multiplier, p.liquid_buoyancy, p.air_buoyancy,
    p.bounce_factor, p.stop_on_first_bounce, p.enable_surface_slope_correction, p.slope_sliding_factor, p.max_movement_per_step,
    p.maximum_correction, p.speed_limit, p.standing_poly, p.crouching_poly, p.sticky_collision, p.sticky_force,
    p.walk_speed, p.run_speed, p.fly_speed, p.air_friction, p.liquid_friction, p.minimum_liquid_percentage,
    p.liquid_impedance, p.normal_ground_friction, p.ambulating_ground_friction, p.ground_force, p.air_force, p.liquid_force,
    p.air_jump_profile, p.liquid_jump_profile, p.fall_status_speed_min, p.fall_through_sustain_frames,
    p.maximum_platform_correction, p.maximum_platform_correction_velocity_factor, p.physics_effect_categories,
    p.collision_enabled, p.friction_enabled, p.gravity_enabled, p.path_explore_rate);
}
}

ActorWireResult operator>>(DataStream& ds, ActorMovementParameters& parameters) {
  return transfer_movement_parameters(ds, parameters);
}

ActorWireResult operator<<(DataStream& ds, ActorMovementParameters const& parameters) {
  return transfer_movement_parameters(ds, parameters);
}

ActorMovementModifiers::ActorMovementModifiers() {
  ground_movement_modifier = 1.0f;
  liquid_movement_modifier = 1.0f;
  speed_modifier = 1.0f;
  air_jump_modifier = 1.0f;
  liquid_jump_modifier = 1.0f;
  running_suppressed = false;
  jumping_suppressed = false;
  facing_suppressed = false;
  movement_suppressed = false;

 }

std::expected<ActorMovementModifiers, StarException> ActorMovementModifiers::create(Json const& config) {
  ActorMovementModifiers value;
  if (!config.is_null()) {
    auto ground_movement_modifier_result = config.get_float("groundMovementModifier", 1.0f);
    if (!ground_movement_modifier_result) return std::unexpected(std::move(ground_movement_modifier_result.error()));
    value.ground_movement_modifier = *ground_movement_modifier_result;
    auto liquid_movement_modifier_result = config.get_float("liquidMovementModifier", 1.0f);
    if (!liquid_movement_modifier_result) return std::unexpected(std::move(liquid_movement_modifier_result.error()));
    value.liquid_movement_modifier = *liquid_movement_modifier_result;
    auto speed_modifier_result = config.get_float("speedModifier", 1.0f);
    if (!speed_modifier_result) return std::unexpected(std::move(speed_modifier_result.error()));
    value.speed_modifier = *speed_modifier_result;
    auto air_jump_modifier_result = config.get_float("airJumpModifier", 1.0f);
    if (!air_jump_modifier_result) return std::unexpected(std::move(air_jump_modifier_result.error()));
    value.air_jump_modifier = *air_jump_modifier_result;
    auto liquid_jump_modifier_result = config.get_float("liquidJumpModifier", 1.0f);
    if (!liquid_jump_modifier_result) return std::unexpected(std::move(liquid_jump_modifier_result.error()));
    value.liquid_jump_modifier = *liquid_jump_modifier_result;
    auto running_suppressed_result = config.get_bool("runningSuppressed", false);
    if (!running_suppressed_result) return std::unexpected(std::move(running_suppressed_result.error()));
    value.running_suppressed = *running_suppressed_result;
    auto jumping_suppressed_result = config.get_bool("jumpingSuppressed", false);
    if (!jumping_suppressed_result) return std::unexpected(std::move(jumping_suppressed_result.error()));
    value.jumping_suppressed = *jumping_suppressed_result;
    auto facing_suppressed_result = config.get_bool("facingSuppressed", false);
    if (!facing_suppressed_result) return std::unexpected(std::move(facing_suppressed_result.error()));
    value.facing_suppressed = *facing_suppressed_result;
    auto movement_suppressed_result = config.get_bool("movementSuppressed", false);
    if (!movement_suppressed_result) return std::unexpected(std::move(movement_suppressed_result.error()));
    value.movement_suppressed = *movement_suppressed_result;
  }
  return value;
}

Json ActorMovementModifiers::to_json() const {
  return JsonObject{
    {"groundMovementModifier", ground_movement_modifier},
    {"liquidMovementModifier", liquid_movement_modifier},
    {"speedModifier", speed_modifier},
    {"airJumpModifier", air_jump_modifier},
    {"liquidJumpModifier", liquid_jump_modifier},
    {"runningSuppressed", running_suppressed},
    {"jumpingSuppressed", jumping_suppressed},
    {"facingSuppressed", facing_suppressed},
    {"movementSuppressed", movement_suppressed}
  };
}

ActorMovementModifiers ActorMovementModifiers::combine(ActorMovementModifiers const& rhs) const {
  ActorMovementModifiers res = *this;

  res.ground_movement_modifier *= rhs.ground_movement_modifier;
  res.liquid_movement_modifier *= rhs.liquid_movement_modifier;
  res.speed_modifier *= rhs.speed_modifier;
  res.air_jump_modifier *= rhs.air_jump_modifier;
  res.liquid_jump_modifier *= rhs.liquid_jump_modifier;
  res.running_suppressed = res.running_suppressed || rhs.running_suppressed;
  res.jumping_suppressed = res.jumping_suppressed || rhs.jumping_suppressed;
  res.facing_suppressed = res.facing_suppressed || rhs.facing_suppressed;
  res.movement_suppressed = res.movement_suppressed || rhs.movement_suppressed;

  return res;
}

namespace {
template <typename T>
  requires std::same_as<std::remove_const_t<T>, ActorMovementModifiers>
ActorWireResult transfer_movement_modifiers(DataStream& ds, T& m) {
  return transfer_fields<std::is_const_v<T>>(ds, m.ground_movement_modifier, m.liquid_movement_modifier,
    m.speed_modifier, m.air_jump_modifier, m.liquid_jump_modifier, m.running_suppressed, m.jumping_suppressed,
    m.facing_suppressed, m.movement_suppressed);
}
}

ActorWireResult operator>>(DataStream& ds, ActorMovementModifiers& modifiers) {
  return transfer_movement_modifiers(ds, modifiers);
}

ActorWireResult operator<<(DataStream& ds, ActorMovementModifiers const& modifiers) {
  return transfer_movement_modifiers(ds, modifiers);
}

ActorMovementController::ActorMovementController(ConstructionKey, Root& root) : MovementController(root) {
  m_control_rotation_rate = 0.0f;
  m_control_run = false;
  m_control_crouch = false;
  m_control_down = false;
  m_control_jump = false;
  m_control_jump_anyway = false;

  m_fall_through_sustain = 0;
  m_last_control_jump = false;
  m_last_control_down = false;
  m_target_horizontal_ambulating_velocity = 0.0f;
  m_move_speed_multiplier = 1.0f;

}

std::expected<std::shared_ptr<ActorMovementController>, StarException> ActorMovementController::create(Root& root, ActorMovementParameters const& parameters) {
  auto controller = std::make_shared<ActorMovementController>(ConstructionKey{}, root);
  auto initialized = controller->initialize({});
  if (!initialized) return std::unexpected(std::move(initialized.error()));
  if (auto result = controller->add_net_element(controller->m_walking); !result) return std::unexpected(std::move(result.error()));
  if (auto result = controller->add_net_element(controller->m_running); !result) return std::unexpected(std::move(result.error()));
  if (auto result = controller->add_net_element(controller->m_moving_direction); !result) return std::unexpected(std::move(result.error()));
  if (auto result = controller->add_net_element(controller->m_facing_direction); !result) return std::unexpected(std::move(result.error()));
  if (auto result = controller->add_net_element(controller->m_crouching); !result) return std::unexpected(std::move(result.error()));
  if (auto result = controller->add_net_element(controller->m_flying); !result) return std::unexpected(std::move(result.error()));
  if (auto result = controller->add_net_element(controller->m_falling); !result) return std::unexpected(std::move(result.error()));
  if (auto result = controller->add_net_element(controller->m_can_jump); !result) return std::unexpected(std::move(result.error()));
  if (auto result = controller->add_net_element(controller->m_jumping); !result) return std::unexpected(std::move(result.error()));
  if (auto result = controller->add_net_element(controller->m_ground_movement); !result) return std::unexpected(std::move(result.error()));
  if (auto result = controller->add_net_element(controller->m_liquid_movement); !result) return std::unexpected(std::move(result.error()));
  if (auto result = controller->add_net_element(controller->m_anchor_state); !result) return std::unexpected(std::move(result.error()));

  auto configured = controller->reset_base_parameters(parameters);
  if (!configured) return std::unexpected(std::move(configured.error()));
  return controller;
}

ActorMovementParameters const& ActorMovementController::base_parameters() const {
  return m_base_parameters;
}

std::expected<void, StarException> ActorMovementController::update_base_parameters(ActorMovementParameters const& parameters) {
  auto merged = m_base_parameters.merge(parameters);
  auto applied = apply_mc_parameters(merged);
  if (!applied) return std::unexpected(std::move(applied.error()));
  m_base_parameters = std::move(merged);
  return {};
}

std::expected<void, StarException> ActorMovementController::reset_base_parameters(ActorMovementParameters const& parameters) {
  auto defaults = ActorMovementParameters::sensible_defaults(root());
  if (!defaults) return std::unexpected(std::move(defaults.error()));
  defaults->merge_from(parameters);
  auto applied = apply_mc_parameters(*defaults);
  if (!applied) return std::unexpected(std::move(applied.error()));
  m_base_parameters = std::move(*defaults);
  return {};
}

ActorMovementModifiers const& ActorMovementController::base_modifiers() const {
  return m_base_modifiers;
}

void ActorMovementController::update_base_modifiers(ActorMovementModifiers const& modifiers) {
  m_base_modifiers = m_base_modifiers.combine(modifiers);
}

void ActorMovementController::reset_base_modifiers(ActorMovementModifiers const& modifiers) {
  m_base_modifiers = modifiers;
}

void ActorMovementController::uninit() {
  m_control_path_move.reset();
  m_path_move_result.reset();
  m_path_controller.reset();
  MovementController::uninit();
}

std::expected<Json, StarException> ActorMovementController::store_state() const {
  auto moving_name = enum_name<direction_names>(moving_direction());
  if (!moving_name) return std::unexpected(std::move(moving_name.error()));
  auto facing_name = enum_name<direction_names>(m_facing_direction.get());
  if (!facing_name) return std::unexpected(std::move(facing_name.error()));
  return JsonObject{{"position", json_from_vec2f(MovementController::position())},
                    {"velocity", json_from_vec2f(velocity())},
                    {"rotation", MovementController::rotation()},
                    {"movingDirection", String(*moving_name)},
                    {"facingDirection", String(*facing_name)},
                    {"crouching", m_crouching.get()}};
}

std::expected<void, StarException> ActorMovementController::load_state(Json const& state) {
  auto loaded = MovementController::loadState(state);
  if (!loaded) return std::unexpected(std::move(loaded.error()));
  auto moving_name = state.get_string("movingDirection");
  if (!moving_name) return std::unexpected(std::move(moving_name.error()));
  auto moving_direction = enum_value<direction_names>(*moving_name);
  if (!moving_direction) return std::unexpected(std::move(moving_direction.error()));
  auto facing_name = state.get_string("facingDirection");
  if (!facing_name) return std::unexpected(std::move(facing_name.error()));
  auto facing_direction = enum_value<direction_names>(*facing_name);
  if (!facing_direction) return std::unexpected(std::move(facing_direction.error()));
  auto crouching_value = state.get_bool("crouching");
  if (!crouching_value) return std::unexpected(std::move(crouching_value.error()));
  set_moving_direction(*moving_direction);
  m_facing_direction.set(*facing_direction);
  m_crouching.set(*crouching_value);
  return {};
}

std::expected<void, StarException> ActorMovementController::set_anchor_state(EntityAnchorState anchor_state) {
  return do_set_anchor_state(anchor_state);
}

std::expected<void, StarException> ActorMovementController::reset_anchor_state() {
  return do_set_anchor_state({});
}

std::optional<EntityAnchorState> ActorMovementController::anchor_state() const {
  return m_anchor_state.get();
}

std::shared_ptr<EntityAnchor const> ActorMovementController::entity_anchor() const {
  return m_entity_anchor;
}

Vec2F ActorMovementController::position() const {
  if (m_entity_anchor)
    return m_entity_anchor->position;
  return MovementController::position();
}

float ActorMovementController::rotation() const {
  if (m_entity_anchor)
    return m_entity_anchor->angle;
  return MovementController::rotation();
}

bool ActorMovementController::walking() const {
  return m_walking.get();
}

bool ActorMovementController::running() const {
  return m_running.get();
}

Direction ActorMovementController::moving_direction() const {
  return m_native_moving_direction.value_or(m_moving_direction.get());
}

void ActorMovementController::set_moving_direction(Direction direction) {
  if (m_native_moving_direction)
    m_native_moving_direction = direction;
  else
    m_moving_direction.set(direction);
}

void ActorMovementController::set_network_moving_direction(std::optional<Direction> direction) {
  if (direction) {
    if (!m_native_moving_direction)
      m_native_moving_direction = m_moving_direction.get();
    m_moving_direction.set(*direction);
  } else if (m_native_moving_direction) {
    m_moving_direction.set(*m_native_moving_direction);
    m_native_moving_direction.reset();
  }
}

Direction ActorMovementController::facing_direction() const {
  if (m_entity_anchor)
    return m_entity_anchor->direction;
  return m_facing_direction.get();
}

bool ActorMovementController::crouching() const {
  return m_crouching.get();
}

bool ActorMovementController::flying() const {
  return m_flying.get();
}

bool ActorMovementController::falling() const {
  return m_falling.get();
}

bool ActorMovementController::can_jump() const {
  return m_can_jump.get();
}

bool ActorMovementController::jumping() const {
  return m_jumping.get();
}

bool ActorMovementController::ground_movement() const {
  return m_ground_movement.get();
}

bool ActorMovementController::liquid_movement() const {
  return m_liquid_movement.get();
}

bool ActorMovementController::pathfinding() const {
  return m_path_controller && !m_path_move_result && m_path_controller->pathfinding();
}

void ActorMovementController::control_rotation(float rotation_rate) {
  m_control_rotation_rate += rotation_rate;
}

void ActorMovementController::control_acceleration(Vec2F const& acceleration) {
  m_control_acceleration += acceleration;
}

void ActorMovementController::control_force(Vec2F const& force) {
  m_control_force += force;
}

void ActorMovementController::control_approach_velocity(Vec2F const& target_velocity, float max_control_force) {
  m_control_approach_velocities.push_back({target_velocity, max_control_force});
}

void ActorMovementController::control_approach_velocity_along_angle(
    float angle, float target_velocity, float max_control_force, bool positive_only) {
  m_control_approach_velocity_along_angles.push_back({angle, target_velocity, max_control_force, positive_only});
}

void ActorMovementController::control_approach_x_velocity(float target_xvelocity, float max_control_force) {
  control_approach_velocity_along_angle(0, target_xvelocity, max_control_force, false);
}

void ActorMovementController::control_approach_y_velocity(float target_yvelocity, float max_control_force) {
  control_approach_velocity_along_angle(std::numbers::pi / 2, target_yvelocity, max_control_force, false);
}

void ActorMovementController::control_parameters(ActorMovementParameters const& parameters) {
  if (m_control_parameters)
    m_control_parameters->merge_from(parameters);
  else
    m_control_parameters.emplace(parameters);
}

void ActorMovementController::control_modifiers(ActorMovementModifiers const& modifiers) {
  m_control_modifiers = m_control_modifiers.combine(modifiers);
}

void ActorMovementController::control_move(Direction direction, bool run) {
  m_control_move = direction;
  m_control_run = run;
}

void ActorMovementController::control_face(Direction direction) {
  m_control_face = direction;
}

void ActorMovementController::control_down() {
  m_control_down = true;
}

void ActorMovementController::control_crouch() {
  m_control_crouch = true;
}

void ActorMovementController::control_jump(bool jump_even_if_unable) {
  m_control_jump = true;
  m_control_jump_anyway |= jump_even_if_unable;
}

void ActorMovementController::control_fly(Vec2F const& velocity) {
  m_control_fly = velocity;
}

std::expected<std::optional<bool>, StarException>
ActorMovementController::path_move(Vec2F const& position, std::optional<PlatformerAStar::Parameters> const& parameters) {
  if (!m_path_controller) {
    auto active_world = world();
    if (!active_world) return std::unexpected(std::move(active_world.error()));
    m_path_controller = std::make_unique<PathController>(active_world->get());
  }

  if (parameters && m_path_controller->set_parameters(*parameters))
    m_path_move_result.reset();

  if (auto completed = std::exchange(m_path_move_result, {}); completed && completed->first == position) {
    m_path_controller->reset();
    m_control_path_move.reset();
    return completed->second;
  }

  auto result = m_path_controller->find_path(*this, position);
  if (result && result->has_value()) {
    m_path_controller->reset();
    m_control_path_move.reset();
  }
  return result;
}

std::expected<std::optional<bool>, StarException>
ActorMovementController::control_path_move(Vec2F const& position, bool run,
                                          std::optional<PlatformerAStar::Parameters> const& parameters) {
  auto result = path_move(position, parameters);
  if (result && !result->has_value())
    m_control_path_move.emplace(position, run);
  return result;
}

void ActorMovementController::set_move_speed_multiplier(float scale) {
  m_move_speed_multiplier = scale;
}

void ActorMovementController::clear_controls() {
  m_control_rotation_rate = 0.0f;
  m_control_acceleration = Vec2F();
  m_control_force = Vec2F();
  m_control_approach_velocities.clear();
  m_control_approach_velocity_along_angles.clear();
  m_control_move = {};
  m_control_face = {};
  m_control_run = false;
  m_control_crouch = false;
  m_control_down = false;
  m_control_jump = false;
  m_control_jump_anyway = false;
  m_control_fly = {};
  m_control_path_move.reset();
  m_control_parameters.reset();
  m_control_modifiers = ActorMovementModifiers();
}

std::expected<void, StarException> ActorMovementController::tick_master(float dt) {
  auto active_world = world();
  if (!active_world) return std::unexpected(std::move(active_world.error()));
  World& world_ref = active_world->get();
  std::shared_ptr<EntityAnchor const> new_anchor;
  if (auto anchor_state = m_anchor_state.get()) {
    if (auto anchorable_entity = as<AnchorableEntity>(world_ref.entity(anchor_state->entityId)))
      {
        auto anchor_value = anchorable_entity->anchor(anchor_state->positionIndex);
        if (!anchor_value) return std::unexpected(std::move(anchor_value.error()));
        new_anchor = std::move(*anchor_value);
      }
  }

  if (new_anchor)
    m_entity_anchor = new_anchor;
  else
    if (auto result = reset_anchor_state(); !result) return std::unexpected(std::move(result.error()));

  if (m_entity_anchor) {
    m_walking.set(false);
    m_running.set(false);
    m_crouching.set(false);
    m_flying.set(false);
    m_falling.set(false);
    m_can_jump.set(false);
    m_jumping.set(false);
    m_ground_movement.set(false);
    m_liquid_movement.set(false);

    setVelocity((m_entity_anchor->position - MovementController::position()) / dt);
    if (auto result = MovementController::tickMaster(dt); !result) return std::unexpected(std::move(result.error()));
    setPosition(m_entity_anchor->position);
  } else {
    auto const merged = m_control_parameters.transform([this](ActorMovementParameters const& patch) {
      return m_base_parameters.merge(patch);
    });
    auto const& active_parameters = merged ? *merged : m_base_parameters;
    auto active_modifiers = m_base_modifiers.combine(m_control_modifiers);

    if (active_modifiers.movement_suppressed) {
      m_control_move = {};
      m_control_run = false;
      m_control_crouch = false;
      m_control_down = false;
      m_control_jump = false;
      m_control_fly = {};
      m_control_path_move.reset();
    }

    if (m_control_move.has_value()
        || m_control_crouch
        || m_control_down
        || m_control_jump
        || m_control_fly.has_value()
        || !m_control_approach_velocities.empty()
        || !m_control_approach_velocity_along_angles.empty()) {
      // controlling any other movement overrides the pathing
      m_control_path_move.reset();
    }

    if (m_control_path_move && !m_path_move_result) {
      if (applied_force_region()) {
        m_path_controller->reset();
      } else if (!m_path_controller->pathfinding()) {
        auto path_result = m_path_controller->move(*this, active_parameters, active_modifiers, m_control_path_move->second, dt);
        if (!path_result) return std::unexpected(std::move(path_result.error()));
        if (*path_result)
          m_path_move_result.emplace(m_control_path_move->first, **path_result);

        auto action = m_path_controller->current_action();
        bool on_ground = false;
        if (auto a = action) {
          using namespace PlatformerAStar;
          m_walking.set(a == Action::Walk && !m_control_path_move->second);
          m_running.set(a == Action::Walk && m_control_path_move->second);
          m_flying.set(a == Action::Fly || a == Action::Swim);
          m_falling.set((a == Action::Arc && yVelocity() < 0.0f) || a == Action::Drop);
          m_jumping.set(a == Action::Arc && yVelocity() >= 0.0f);

          on_ground = a == Action::Walk || a == Action::Drop || a == Action::Jump;

          if (a == Action::Land || a== Action::Jump) {
            auto in_liquid = liquidPercentage() >= active_parameters.minimum_liquid_percentage.value_or(1.0);
            m_liquid_movement.set(in_liquid);
            m_ground_movement.set(!in_liquid);
            on_ground = !in_liquid && on_ground;
          } else {
            m_liquid_movement.set(a == Action::Swim);
            m_ground_movement.set(a != Action::Arc && a != Action::Swim);
          }
        } else {
          m_walking.set(false);
          m_running.set(false);
        }
        auto path_facing = m_path_controller->facing();
        auto facing = (m_control_face ? m_control_face : path_facing).value_or(m_facing_direction.get());
        m_facing_direction.set(facing);
        set_moving_direction(path_facing.value_or(m_facing_direction.get()));

        if (auto result = apply_mc_parameters(active_parameters); !result) return std::unexpected(std::move(result.error()));

        // MovementController still handles updating liquid percentage and updating force regions
        if (auto result = updateLiquidPercentage(); !result) return std::unexpected(std::move(result.error()));
        if (auto result = update_force_regions(); !result) return std::unexpected(std::move(result.error()));
        // onGround flag needs to be manually set, won't be set by MovementController::tickMaster
        setOnGround(on_ground);
        // Hold the receipt until the next request, not the finished edge list.
        if (m_path_move_result)
          m_path_controller->reset();
        clear_controls();
        return {};
      } else {
        auto path_result = m_path_controller->find_path(*this, m_control_path_move->first);
        if (!path_result) return std::unexpected(std::move(path_result.error()));
        if (*path_result)
          m_path_move_result.emplace(m_control_path_move->first, **path_result);
      }
    } else if (!m_path_move_result) {
      m_path_controller.reset();
    }

    // Do some basic movement consistency checks.
    if (m_control_fly)
      m_control_move = {};

    if ((m_control_down && !m_last_control_down) || m_control_fly)
      m_fall_through_sustain = (*active_parameters.fall_through_sustain_frames);
    else if (m_fall_through_sustain > 0)
      --m_fall_through_sustain;

    if (auto result = apply_mc_parameters(active_parameters); !result) return std::unexpected(std::move(result.error()));

    m_target_horizontal_ambulating_velocity = 0.0f;

    rotate(m_control_rotation_rate);
    accelerate(m_control_acceleration);
    force(m_control_force);

    for (auto const& approach : m_control_approach_velocities)
      approachVelocity(approach.target_velocity * active_modifiers.speed_modifier, approach.max_control_force);

    for (auto const& approach : m_control_approach_velocity_along_angles)
      approachVelocityAlongAngle(approach.along_angle, approach.target_velocity * active_modifiers.speed_modifier, approach.max_control_force, approach.positive_only);

    m_liquid_movement.set(liquidPercentage() >= (*active_parameters.minimum_liquid_percentage));
    float liquid_impedance = (*active_parameters.liquid_impedance) * liquidPercentage();

    std::optional<Direction> updated_moving_direction;
    bool running = m_control_run && !active_modifiers.running_suppressed;

    if (m_control_fly) {
      Vec2F fly_velocity = *m_control_fly;
      if (fly_velocity.magnitude_squared() != 0)
        fly_velocity = fly_velocity.normalized() * (*active_parameters.fly_speed);

      if (m_liquid_movement.get())
        approachVelocity(fly_velocity * (1.0f - liquid_impedance) * active_modifiers.speed_modifier, (*active_parameters.liquid_force) * active_modifiers.liquid_movement_modifier);
      else
        approachVelocity(fly_velocity * active_modifiers.speed_modifier, (*active_parameters.air_force));

      if (fly_velocity[0] > 0)
        updated_moving_direction = Direction::Right;
      else if (fly_velocity[0] < 0)
        updated_moving_direction = Direction::Left;

      m_ground_movement_sustain_timer = GameTimer(0);

    } else {
      float jump_modifier;
      ActorJumpProfile jump_profile;
      if (m_liquid_movement.get()) {
        jump_modifier = active_modifiers.liquid_jump_modifier;
        jump_profile = active_parameters.liquid_jump_profile;
        (*jump_profile.jump_speed) *= (1.0f - liquid_impedance);
      } else {
        jump_modifier = active_modifiers.air_jump_modifier;
        jump_profile = active_parameters.air_jump_profile;
      }

      bool start_jump = false;
      bool hold_jump = false;

      // If we are on the ground, then reset the ground movement sustain timer
      // to the maximum.  If we are not on the ground or near the ground
      // according to the nearGroundCheckDistance, and we are past the minimum
      // sustain time, then go ahead and immediately clear the ground movement
      // sustain timer.
      float min_ground_sustain = (*active_parameters.ground_movement_minimum_sustain);
      float max_ground_sustain = (*active_parameters.ground_movement_maximum_sustain);
      float ground_check_distance = (*active_parameters.ground_movement_check_distance);
      m_ground_movement_sustain_timer.tick(dt);
      if (onGround()) {
        m_ground_movement_sustain_timer = GameTimer(max_ground_sustain);
      } else if (!m_ground_movement_sustain_timer.ready() && ground_check_distance > 0.0f && max_ground_sustain - m_ground_movement_sustain_timer.remaining > min_ground_sustain) {
        PolyF collision_body = MovementController::collisionBody();
        collision_body.translate(Vec2F(0, -ground_check_distance));
        auto collision = world_ref.poly_collision(collision_body, {CollisionKind::Block, CollisionKind::Dynamic, CollisionKind::Platform, CollisionKind::Slippery});
        if (!collision) return std::unexpected(std::move(collision.error()));
        if (!*collision)
          m_ground_movement_sustain_timer = GameTimer(0);
      }

      bool standing_jumpable = !m_ground_movement_sustain_timer.ready();
      bool control_jump = m_control_jump && (!active_modifiers.jumping_suppressed || m_control_jump_anyway);

      // We are doing a jump if m_reJumpTimer has run out and there has been a
      // new m_controlJump command which was just recently triggered.  If
      // jumpProfile.autoJump is set, then we don't care whether it is a new
      // m_controlJump command, m_controlJump can be held.
      if (m_re_jump_timer.ready() && control_jump && ((*jump_profile.auto_jump) || !m_last_control_jump)) {
        if (standing_jumpable || (*jump_profile.multi_jump) || m_control_jump_anyway)
          start_jump = true;
      } else if (m_jumping.get() && control_jump && (!m_jump_hold_timer || !m_jump_hold_timer->ready())) {
        if (!(*jump_profile.collision_cancelled) || collisionCorrection()[1] >= 0.0f)
          hold_jump = true;
      }

      if (start_jump) {
        m_jumping.set(true);

        m_re_jump_timer = GameTimer((*jump_profile.re_jump_delay));
        if ((*jump_profile.jump_hold_time) >= 0.0f)
          m_jump_hold_timer = GameTimer((*jump_profile.jump_hold_time));
        else
          m_jump_hold_timer = {};

        setYVelocity(yVelocity() + (*jump_profile.jump_speed) * (*jump_profile.jump_initial_percentage) * jump_modifier);

        m_ground_movement_sustain_timer = GameTimer(0);

      } else if (hold_jump) {
        m_re_jump_timer.tick(dt);
        if (m_jump_hold_timer)
          m_jump_hold_timer->tick(dt);

        approachYVelocity((*jump_profile.jump_speed) * jump_modifier, (*jump_profile.jump_control_force) * jump_modifier);

      } else {
        m_jumping.set(false);
        m_re_jump_timer.tick(dt);
      }

      if (m_control_move == Direction::Left) {
        updated_moving_direction = Direction::Left;
        m_target_horizontal_ambulating_velocity =
            -1.0f * (running ? (*active_parameters.run_speed) * active_modifiers.speed_modifier
                             : (*active_parameters.walk_speed) * active_modifiers.speed_modifier);
      } else if (m_control_move == Direction::Right) {
        updated_moving_direction = Direction::Right;
        m_target_horizontal_ambulating_velocity =
            1.0f * (running ? (*active_parameters.run_speed) * active_modifiers.speed_modifier
                            : (*active_parameters.walk_speed) * active_modifiers.speed_modifier);
      }

      m_target_horizontal_ambulating_velocity *= m_move_speed_multiplier;

      if (m_liquid_movement.get())
        m_target_horizontal_ambulating_velocity *= (1.0f - liquid_impedance);

      Vec2F surface_velocity = MovementController::surfaceVelocity();

      // don't ambulate if we're already moving faster than the target velocity in the direction of ambulation
      bool ambulation_would_accelerate = std::abs(m_target_horizontal_ambulating_velocity + surface_velocity[0]) > std::abs(xVelocity())
          || (m_target_horizontal_ambulating_velocity < 0) != (xVelocity() < 0);

      if (m_target_horizontal_ambulating_velocity != 0.0f && ambulation_would_accelerate) {
        float ambulating_accel;
        if (onGround())
          ambulating_accel = (*active_parameters.ground_force) * active_modifiers.ground_movement_modifier;
        else if (m_liquid_movement.get())
          ambulating_accel = (*active_parameters.liquid_force) * active_modifiers.liquid_movement_modifier;
        else
          ambulating_accel = (*active_parameters.air_force);

        approachXVelocity(m_target_horizontal_ambulating_velocity + surface_velocity[0], ambulating_accel);
      }
    }

    if (updated_moving_direction)
      set_moving_direction(*updated_moving_direction);

    if (!active_modifiers.facing_suppressed) {
      if (m_control_face)
        m_facing_direction.set(*m_control_face);
      else if (updated_moving_direction)
        m_facing_direction.set(*updated_moving_direction);
      else if (m_control_path_move && m_path_controller && m_path_controller->facing())
        m_facing_direction.set(*m_path_controller->facing());
    }

    m_ground_movement.set(!m_ground_movement_sustain_timer.ready());
    if (m_ground_movement.get()) {
      m_running.set(running && m_control_move);
      m_walking.set(!running && m_control_move);
      m_crouching.set(m_control_crouch && !m_control_move);
    }
    m_flying.set(static_cast<bool>(m_control_fly));

    bool falling = (yVelocity() < (*active_parameters.fall_status_speed_min)) && !m_ground_movement.get();
    m_falling.set(falling);

    if (auto result = MovementController::tickMaster(dt); !result) return std::unexpected(std::move(result.error()));

    m_last_control_jump = m_control_jump;
    m_last_control_down = m_control_down;

    if (m_liquid_movement.get())
      m_can_jump.set(m_re_jump_timer.ready() && (!m_ground_movement_sustain_timer.ready() || (*active_parameters.liquid_jump_profile.multi_jump)));
    else
      m_can_jump.set(m_re_jump_timer.ready() && (!m_ground_movement_sustain_timer.ready() || (*active_parameters.air_jump_profile.multi_jump)));
  }

  clear_controls();
  return {};
}

std::expected<void, StarException> ActorMovementController::tick_slave(float dt) {
  auto active_world = world();
  if (!active_world) return std::unexpected(std::move(active_world.error()));
  World& world_ref = active_world->get();
  if (auto result = MovementController::tickSlave(dt); !result) return std::unexpected(std::move(result.error()));

  m_entity_anchor.reset();
  if (auto anchor_state = m_anchor_state.get()) {
    if (auto anchorable_entity = as<AnchorableEntity>(world_ref.entity(anchor_state->entityId)))
      {
        auto anchor_value = anchorable_entity->anchor(anchor_state->positionIndex);
        if (!anchor_value) return std::unexpected(std::move(anchor_value.error()));
        m_entity_anchor = std::move(*anchor_value);
      }
  }
  return {};
}

std::expected<void, StarException> ActorMovementController::apply_mc_parameters(ActorMovementParameters const& parameters) {
  if (!parameters.fall_through_sustain_frames) return std::unexpected(ActorMovementControllerException("Missing actor movement parameter 'fallThroughSustainFrames'"));
  if (!parameters.minimum_liquid_percentage) return std::unexpected(ActorMovementControllerException("Missing actor movement parameter 'minimumLiquidPercentage'"));
  if (!parameters.liquid_impedance) return std::unexpected(ActorMovementControllerException("Missing actor movement parameter 'liquidImpedance'"));
  if (!parameters.fly_speed) return std::unexpected(ActorMovementControllerException("Missing actor movement parameter 'flySpeed'"));
  if (!parameters.liquid_force) return std::unexpected(ActorMovementControllerException("Missing actor movement parameter 'liquidForce'"));
  if (!parameters.air_force) return std::unexpected(ActorMovementControllerException("Missing actor movement parameter 'airForce'"));
  if (!parameters.ground_movement_minimum_sustain) return std::unexpected(ActorMovementControllerException("Missing actor movement parameter 'groundMovementMinimumSustain'"));
  if (!parameters.ground_movement_maximum_sustain) return std::unexpected(ActorMovementControllerException("Missing actor movement parameter 'groundMovementMaximumSustain'"));
  if (!parameters.ground_movement_check_distance) return std::unexpected(ActorMovementControllerException("Missing actor movement parameter 'groundMovementCheckDistance'"));
  if (!parameters.run_speed) return std::unexpected(ActorMovementControllerException("Missing actor movement parameter 'runSpeed'"));
  if (!parameters.walk_speed) return std::unexpected(ActorMovementControllerException("Missing actor movement parameter 'walkSpeed'"));
  if (!parameters.ground_force) return std::unexpected(ActorMovementControllerException("Missing actor movement parameter 'groundForce'"));
  if (!parameters.fall_status_speed_min) return std::unexpected(ActorMovementControllerException("Missing actor movement parameter 'fallStatusSpeedMin'"));
  if (!parameters.gravity_multiplier) return std::unexpected(ActorMovementControllerException("Missing actor movement parameter 'gravityMultiplier'"));
  if (!parameters.crouching_poly) return std::unexpected(ActorMovementControllerException("Missing actor movement parameter 'crouchingPoly'"));
  if (!parameters.standing_poly) return std::unexpected(ActorMovementControllerException("Missing actor movement parameter 'standingPoly'"));
  if (!parameters.air_jump_profile.jump_speed) return std::unexpected(ActorMovementControllerException("Missing actor movement parameter 'airJumpProfile.jumpSpeed'"));
  if (!parameters.air_jump_profile.auto_jump) return std::unexpected(ActorMovementControllerException("Missing actor movement parameter 'airJumpProfile.autoJump'"));
  if (!parameters.air_jump_profile.multi_jump) return std::unexpected(ActorMovementControllerException("Missing actor movement parameter 'airJumpProfile.multiJump'"));
  if (!parameters.air_jump_profile.collision_cancelled) return std::unexpected(ActorMovementControllerException("Missing actor movement parameter 'airJumpProfile.collisionCancelled'"));
  if (!parameters.air_jump_profile.re_jump_delay) return std::unexpected(ActorMovementControllerException("Missing actor movement parameter 'airJumpProfile.reJumpDelay'"));
  if (!parameters.air_jump_profile.jump_hold_time) return std::unexpected(ActorMovementControllerException("Missing actor movement parameter 'airJumpProfile.jumpHoldTime'"));
  if (!parameters.air_jump_profile.jump_initial_percentage) return std::unexpected(ActorMovementControllerException("Missing actor movement parameter 'airJumpProfile.jumpInitialPercentage'"));
  if (!parameters.air_jump_profile.jump_control_force) return std::unexpected(ActorMovementControllerException("Missing actor movement parameter 'airJumpProfile.jumpControlForce'"));
  if (!parameters.liquid_jump_profile.jump_speed) return std::unexpected(ActorMovementControllerException("Missing actor movement parameter 'liquidJumpProfile.jumpSpeed'"));
  if (!parameters.liquid_jump_profile.auto_jump) return std::unexpected(ActorMovementControllerException("Missing actor movement parameter 'liquidJumpProfile.autoJump'"));
  if (!parameters.liquid_jump_profile.multi_jump) return std::unexpected(ActorMovementControllerException("Missing actor movement parameter 'liquidJumpProfile.multiJump'"));
  if (!parameters.liquid_jump_profile.collision_cancelled) return std::unexpected(ActorMovementControllerException("Missing actor movement parameter 'liquidJumpProfile.collisionCancelled'"));
  if (!parameters.liquid_jump_profile.re_jump_delay) return std::unexpected(ActorMovementControllerException("Missing actor movement parameter 'liquidJumpProfile.reJumpDelay'"));
  if (!parameters.liquid_jump_profile.jump_hold_time) return std::unexpected(ActorMovementControllerException("Missing actor movement parameter 'liquidJumpProfile.jumpHoldTime'"));
  if (!parameters.liquid_jump_profile.jump_initial_percentage) return std::unexpected(ActorMovementControllerException("Missing actor movement parameter 'liquidJumpProfile.jumpInitialPercentage'"));
  if (!parameters.liquid_jump_profile.jump_control_force) return std::unexpected(ActorMovementControllerException("Missing actor movement parameter 'liquidJumpProfile.jumpControlForce'"));
  MovementParameters mc_parameters;

  mc_parameters.mass = parameters.mass;

  mc_parameters.gravityMultiplier = parameters.gravity_multiplier;

  mc_parameters.liquidBuoyancy = parameters.liquid_buoyancy;
  mc_parameters.airBuoyancy = parameters.air_buoyancy;
  mc_parameters.bounceFactor = parameters.bounce_factor;
  mc_parameters.stopOnFirstBounce = parameters.stop_on_first_bounce;
  mc_parameters.enableSurfaceSlopeCorrection = parameters.enable_surface_slope_correction;
  mc_parameters.slopeSlidingFactor = parameters.slope_sliding_factor;
  mc_parameters.maxMovementPerStep = parameters.max_movement_per_step;

  if (m_crouching.get())
    mc_parameters.collisionPoly = (*parameters.crouching_poly);
  else
    mc_parameters.collisionPoly = (*parameters.standing_poly);

  mc_parameters.stickyCollision = parameters.sticky_collision;
  mc_parameters.stickyForce = parameters.sticky_force;

  mc_parameters.airFriction = parameters.air_friction;
  mc_parameters.liquidFriction = parameters.liquid_friction;

  // If we are traveling in the correct direction while in a movement mode that
  // requires contact with the ground (ambulating i.e. walking or running), and
  // not traveling faster than our target horizontal movement, then apply the
  // special 'ambulatingGroundFriction'.
  float relative_xvelocity = xVelocity() - surfaceVelocity()[0];
  bool use_ambulating_ground_friction = (m_walking.get() || m_running.get())
      && std::copysign(1, m_target_horizontal_ambulating_velocity) == std::copysign(1, relative_xvelocity)
      && std::fabs(relative_xvelocity) <= std::fabs(m_target_horizontal_ambulating_velocity);

  if (use_ambulating_ground_friction)
    mc_parameters.groundFriction = parameters.ambulating_ground_friction;
  else
    mc_parameters.groundFriction = parameters.normal_ground_friction;

  mc_parameters.collisionEnabled = parameters.collision_enabled;
  mc_parameters.frictionEnabled = parameters.friction_enabled;
  mc_parameters.gravityEnabled = parameters.gravity_enabled;

  mc_parameters.ignorePlatformCollision = m_fall_through_sustain > 0 || m_control_fly || m_control_down;
  mc_parameters.maximumPlatformCorrection = parameters.maximum_platform_correction;
  mc_parameters.maximumPlatformCorrectionVelocityFactor = parameters.maximum_platform_correction_velocity_factor;

  mc_parameters.physics_effect_categories = parameters.physics_effect_categories;

  mc_parameters.maximumCorrection = parameters.maximum_correction;
  mc_parameters.speedLimit = parameters.speed_limit;

  return MovementController::applyParameters(mc_parameters);
}

std::expected<void, StarException> ActorMovementController::do_set_anchor_state(std::optional<EntityAnchorState> anchor_state) {
  std::shared_ptr<EntityAnchor const> entity_anchor;
  if (anchor_state) {
    auto active_world = world();
    if (!active_world) return std::unexpected(std::move(active_world.error()));
    World& world_ref = active_world->get();
    auto anchorable_entity = as<AnchorableEntity>(world_ref.entity(anchor_state->entityId));
    if (!anchorable_entity)
      return std::unexpected(ActorMovementControllerException::format("No such anchorable entity id {} in ActorMovementController::set_anchor_state", anchor_state->entityId));
    auto anchor_value = anchorable_entity->anchor(anchor_state->positionIndex);
    if (!anchor_value) return std::unexpected(std::move(anchor_value.error()));
    entity_anchor = std::move(*anchor_value);
    if (!entity_anchor)
      return std::unexpected(ActorMovementControllerException::format("Anchor position {} is disabled ActorMovementController::set_anchor_state", anchor_state->positionIndex));
  }
  auto previous_anchor = std::move(m_entity_anchor);
  m_anchor_state.set(anchor_state);
  m_entity_anchor = std::move(entity_anchor);

  if (m_entity_anchor) {
    setPosition(m_entity_anchor->position);
  } else if (previous_anchor && previous_anchor->exitBottomPosition) {
    auto bounds = MovementController::localBoundBox();
    Vec2F bottom_center = {bounds.center()[0], bounds.y_min()};
    setPosition(*previous_anchor->exitBottomPosition - bottom_center);
  }
  return {};
}

PathController::PathController(World& world) : m_world(world), m_edge_index(0), m_edge_timer(0.0) {}

bool PathController::set_parameters(PlatformerAStar::Parameters const& parameters) {
  if (m_parameters == parameters)
    return false;
  reset();
  m_parameters = parameters;
  return true;
}

void PathController::reset() {
  m_start_position = {};
  m_target_position = {};
  m_control_face = {};
  m_path_finder = {};
  m_path = {};
  m_edge_index = 0;
  m_edge_timer = 0.0;
}

bool PathController::pathfinding() const {
  return !m_path.has_value();
}

std::optional<Vec2F> PathController::target_position() const { return m_target_position; }

std::optional<Direction> PathController::facing() const { return m_control_face; }

std::optional<PlatformerAStar::Action> PathController::current_action() const {
  if (m_path && m_edge_index < m_path->size()) {
    return (*m_path)[m_edge_index].action;
  }
  return {};
}

std::expected<std::optional<bool>, StarException>
PathController::find_path(ActorMovementController& movement_controller, Vec2F const& target_position) {
  using namespace PlatformerAStar;

  // reached the end of the last path and we have a new target position to move toward
  if (m_path && m_edge_index == m_path->size()
      && m_world.get().geometry().diff(*m_target_position, target_position).magnitude() > 0.001) {
    reset();
    m_target_position = target_position;
  }

  // starting a new path, or the target position moved by more than 2 blocks
  if (!m_target_position || (!m_path && !m_path_finder)
      || m_world.get().geometry().diff(*m_target_position, target_position).magnitude() > 2.0) {
    auto grounded = movement_controller.onGround();
    if (m_path) {
      // if already moving on a path, collision will be disabled and we can't use MovementController::onGround() to check for ground collision
      auto const ground_collision =
        CollisionSet{CollisionKind::Null, CollisionKind::Block, CollisionKind::Slippery, CollisionKind::Platform};
      auto grounded_result = on_ground(movement_controller, movement_controller.position(), ground_collision);
      if (!grounded_result)
        return std::unexpected(std::move(grounded_result.error()));
      grounded = *grounded_result;
    }
    if (*movement_controller.parameters().gravityEnabled && !grounded && !movement_controller.liquid_movement()) {
      return {};
    }
    m_start_position = movement_controller.position();
    m_target_position = target_position;
    auto finder = PathFinder::create(m_world.get(), movement_controller.position(), *m_target_position,
                                     movement_controller.base_parameters(), m_parameters);
    if (!finder) return std::unexpected(std::move(finder.error()));
    m_path_finder = std::move(*finder);
  }

  if (!m_path_finder && m_path && m_edge_index == m_path->size())
    return true; // Reached goal

  if (m_path_finder) {
    auto exploration = m_path_finder->explore(movement_controller.base_parameters().path_explore_rate.value_or(100.0));
    if (!exploration) return std::unexpected(std::move(exploration.error()));
    auto explored = *exploration;
    if (explored) {
      auto pathfinder = std::move(m_path_finder);

      if (*explored) {
        auto path_result = pathfinder->extract_result();
        if (!path_result)
          return std::unexpected(ActorMovementControllerException("Successful path search has no path"));
        auto path = std::move(*path_result);

        float new_edge_timer = 0.0;
        std::size_t new_edge_index = 0;

        // if we have a path already, see if our paths can be merged either by splicing or fast forwarding
        bool merged = false;
        if (m_path && !path.empty()) {
          // try to fast forward on the new path
          std::size_t i = 0;
          // fast forward from current edge, or the last edge of the path
          auto current_edge_index = std::min(m_edge_index, m_path->size() - 1);
          auto current_edge_result = m_path->at(current_edge_index);
          if (!current_edge_result)
            return std::unexpected(std::move(current_edge_result.error()));
          auto& current_edge = current_edge_result->get();
          for (auto& edge : path) {
            if (current_edge.action == edge.action && current_edge.source.position == edge.source.position
                && current_edge.target.position == edge.target.position) {
              // fast forward on the new path
              new_edge_timer = m_edge_timer;
              new_edge_index = i;

              merged = true;
              break;
            }
            i++;
          }

          if (!merged) {
            // try to splice the new path onto the current path
            auto& new_path_start = path[0];
            for (std::size_t i = m_edge_index; i < m_path->size(); i += 2) {
              auto& edge = (*m_path)[i];
              if (edge.target.position == new_path_start.source.position) {
                // splice the new path onto our current path up to this index
                auto new_path = m_path->slice(0, i + 1);
                new_path.append_range(path);
                path = std::move(new_path);

                new_edge_timer = m_edge_timer;
                new_edge_index = m_edge_index;

                merged = true;
                break;
              }
            }
          }
        }

        if (!merged && movement_controller.position() != *m_start_position) {
          // merging the paths failed, and the entity has moved from the path start position
          // try to bridge the gap from the current position to the new path
          auto bridge_result = PathFinder::create(m_world.get(), movement_controller.position(), *m_start_position,
                                                  movement_controller.base_parameters(), m_parameters);
          if (!bridge_result)
            return std::unexpected(std::move(bridge_result.error()));
          auto bridge_path_finder = std::move(*bridge_result);
          auto bridge_exploration =
            bridge_path_finder->explore(movement_controller.base_parameters().path_explore_rate.value_or(100.0));
          if (!bridge_exploration)
            return std::unexpected(std::move(bridge_exploration.error()));
          auto explored = *bridge_exploration;

          if (explored && *explored) {
            // concatenate the bridge path with the new path
            auto bridge_path = bridge_path_finder->extract_result();
            if (!bridge_path)
              return std::unexpected(ActorMovementControllerException("Successful bridge search has no path"));
            auto new_path = std::move(*bridge_path);
            new_path.append_range(path);
            path = std::move(new_path);
          } else {
            // if the gap isn't bridged in a single tick, reset and start over
            reset();
            return {};
          }
        }

        bool valid = true;
        if (!path.empty()) {
          auto edge_valid = validate_edge(movement_controller, path[0]);
          if (!edge_valid)
            return std::unexpected(std::move(edge_valid.error()));
          valid = *edge_valid;
        }
        if (!valid) {
          // reset if the first edge is invalid
          reset();
          return false;
        }

        m_edge_timer = new_edge_timer;
        m_edge_index = new_edge_index;
        m_path = std::move(path);
        if (m_path->empty())
          return true;
      } else {
        reset();
        return false;
      }
    }
  }
  return {};
}

std::expected<std::optional<bool>, StarException> PathController::move(ActorMovementController& movement_controller,
                                                                       ActorMovementParameters const& parameters,
                                                                       ActorMovementModifiers const& modifiers,
                                                                       bool run, float dt) {
  using namespace PlatformerAStar;

  // pathfind to a new target position in the background while moving on the current path
  if (m_path_finder && m_target_position) {
    auto result = find_path(movement_controller, *m_target_position);
    if (!result || result->has_value())
      return result;
  }

  if (!m_path)
    return {};

  m_control_face = {};

  while (m_edge_index < m_path->size()) {
    auto& edge = (*m_path)[m_edge_index];
    Vec2F delta = m_world.get().geometry().diff(edge.target.position, edge.source.position);

    Vec2F source_velocity;
    Vec2F target_velocity;
    switch (edge.action) {
      case Action::Jump:
        if (modifiers.jumping_suppressed) {
          reset();
          return {};
        }
        break;
      case Action::Arc:
        source_velocity = edge.source.velocity.value_or(Vec2F());
        target_velocity = edge.target.velocity.value_or(Vec2F());
        break;
      case Action::Drop: target_velocity = edge.target.velocity.value_or(Vec2F()); break;
      case Action::Fly:
        {
          // accelerate along path using airForce
          float angle_factor = movement_controller.velocity().normalized() * delta.normalized();
          float speed_along_angle = angle_factor * movement_controller.velocity().magnitude();
          auto acc = parameters.air_force.value_or(0.0) / movement_controller.mass();
          source_velocity =
            delta.normalized() * std::fmin(parameters.fly_speed.value_or(0.0), speed_along_angle + acc * dt);
          target_velocity = source_velocity;
        }
        break;
      case Action::Swim:
        source_velocity = target_velocity =
          delta.normalized() * parameters.fly_speed.value_or(0.0f) * (1.0f - parameters.liquid_impedance.value_or(0.0f));
        break;
      case Action::Walk:
        source_velocity =
          delta.normalized() * (run ? parameters.run_speed.value_or(0.0f) : parameters.walk_speed.value_or(0.0f));
        source_velocity *= modifiers.speed_modifier;
        target_velocity = source_velocity;
        break;
      default: {}
    }

    Vec2F average_velocity = (source_velocity + target_velocity) / 2.0;
    float average_speed = average_velocity.magnitude();
    auto edge_time = average_speed > 0.0f ? delta.magnitude() / average_speed : 0.2;

    auto edge_progress = m_edge_timer / edge_time;
    if (edge_progress > 1.0f) {
      m_edge_timer -= edge_time;
      m_edge_index++;
      if (m_edge_index < m_path->size()) {
        auto valid = validate_edge(movement_controller, (*m_path)[m_edge_index]);
        if (!valid) return std::unexpected(std::move(valid.error()));
        if (!*valid) {
          reset();
          return {};
        }
      }
      continue;
    }

    auto current_velocity = source_velocity + ((target_velocity - source_velocity) * edge_progress);
    movement_controller.setVelocity(current_velocity);
    auto movement = (current_velocity + source_velocity) / 2.0 * m_edge_timer;
    movement_controller.setPosition(edge.source.position + movement);

    if (auto direction = direction_of(delta[0]))
      m_control_face = direction;

    m_edge_timer += dt;
    return {};
  }

  if (auto last_edge = m_path->back_value()) {
    movement_controller.setPosition(last_edge->target.position);
    movement_controller.setVelocity(Vec2F());
  }

  // reached the end of the path, success unless we're also currently pathfinding to a new position
  if (m_path_finder)
    return {};
  else
    return true;
}

std::expected<bool, StarException> PathController::validate_edge(ActorMovementController& movement_controller,
                                                                 PlatformerAStar::Edge const& edge) {
  using namespace PlatformerAStar;
  auto const ground_collision =
    CollisionSet{CollisionKind::Null, CollisionKind::Block, CollisionKind::Slippery, CollisionKind::Platform};
  auto const solid_collision = CollisionSet{CollisionKind::Null, CollisionKind::Block, CollisionKind::Slippery};
  auto open_doors = [&world = m_world.get()](RectF const& bounds) -> std::expected<bool, StarException> {
    auto objects = world.entityQuery(bounds, entityTypeFilter<Object>());
    if (!objects)
      return std::unexpected(std::move(objects.error()));
    bool opened = false;
    for (auto const& entity : *objects) {
      if (auto object = as<Object>(entity); object && object->isMaster()) {
        auto arg = world.luaRoot()->lua_engine().create_string("closedDoor");
        if (!arg)
          return std::unexpected(std::move(arg.error()));
        auto response = object->call_script("hasCapability", LuaVariadic<LuaValue>{*arg});
        if (!response)
          return std::unexpected(std::move(response.error()));
        if (*response && std::holds_alternative<LuaBoolean>(**response) && std::get<LuaBoolean>(**response)) {
          auto sent = world.sendEntityMessage(entity->entityId(), "openDoor");
          if (!sent)
            return std::unexpected(std::move(sent.error()));
          opened = true;
        }
      }
    }
    return opened;
  };
  auto poly = movement_controller.collisionPoly();
  poly.translate(edge.target.position);
  auto world_collision = m_world.get().poly_collision(poly);
  if (!world_collision)
    return std::unexpected(std::move(world_collision.error()));
  bool collided = *world_collision;
  if (!collided) {
    auto moving = moving_collision(movement_controller, poly);
    if (!moving) return std::unexpected(std::move(moving.error()));
    collided = *moving;
  }
  if (collided) {
    auto bounds = RectI::integral(poly.bounding_box());
    auto tile_collision = m_world.get().rectTileCollision(bounds);
    if (!tile_collision)
      return std::unexpected(std::move(tile_collision.error()));
    if (!*tile_collision)
      return false;
    auto solid = m_world.get().rectTileCollision(bounds, solid_collision);
    if (!solid) return std::unexpected(std::move(solid.error()));
    if (*solid) return false;
    auto opened = open_doors(poly.bounding_box());
    if (!opened) return std::unexpected(std::move(opened.error()));
    if (!*opened) return false;
  }
  auto in_liquid = [&movement_controller,
                    &world = m_world.get()](Vec2F const& position) -> std::expected<bool, StarException> {
    auto bounds = movement_controller.localBoundBox().translated(position);
    auto liquid = world.liquidLevel(bounds);
    if (!liquid)
      return std::unexpected(std::move(liquid.error()));
    return liquid->level >= movement_controller.base_parameters().minimum_liquid_percentage.value_or(1.0);
  };
  switch (edge.action) {
  case Action::Walk: return on_ground(movement_controller, edge.source.position, ground_collision);
  case Action::Swim: return in_liquid(edge.target.position);
  case Action::Land: {
    auto grounded = on_ground(movement_controller, edge.target.position, ground_collision);
    if (!grounded)
      return std::unexpected(std::move(grounded.error()));
    if (*grounded)
      return true;
    return in_liquid(edge.target.position);
  }
    case Action::Drop: {
      auto grounded = on_ground(movement_controller, edge.source.position, ground_collision);
      if (!grounded) return std::unexpected(std::move(grounded.error()));
      if (!*grounded) return false;
      auto solid = on_ground(movement_controller, edge.source.position, solid_collision);
      if (!solid) return std::unexpected(std::move(solid.error()));
      return !*solid;
    }
    default: return true;
  }
}

std::expected<bool, StarException> PathController::moving_collision(ActorMovementController& movement_controller,
                                                                    PolyF const& collision_poly) {
  bool collided = false;
  auto scanned = movement_controller.for_each_moving_collision(
    collision_poly.bounding_box(),
    [&](MovingCollisionId, CollisionKind, PolyF const& poly, RectF const&) -> std::expected<bool, StarException> {
      if (poly.intersects(collision_poly)) {
        // set collided and stop iterating
        collided = true;
        return false;
      }
      return true;
    });
  if (!scanned) return std::unexpected(std::move(scanned.error()));
  return collided;
}

std::expected<bool, StarException> PathController::on_ground(ActorMovementController const& movement_controller,
                                                             Vec2F const& position,
                                                             CollisionSet const& collision_set) const {
  auto bounds = RectI::integral(movement_controller.localBoundBox().translated(position));
  Vec2I min = Vec2I(bounds.x_min(), bounds.y_min() - 1);
  Vec2I max = Vec2I(bounds.x_max(), bounds.y_min());
  return m_world.get().rectTileCollision(RectI(min, max), collision_set);
}
}

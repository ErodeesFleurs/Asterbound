module;
import std;
import star.format;
import star.json;
import star.id_map;
import star.lua;
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
import star.lua_game_converters;

module star.movement_controller_lua_bindings;

import star.rect;
namespace Star {

std::expected<LuaCallbacks, StarException> LuaBindings::make_movement_controller_callbacks(MovementController& movement_controller) {
  LuaCallbacks callbacks;

  if (auto registration = callbacks.register_callback(
      "parameters", [&movement_controller]() { return movement_controller.parameters().to_json(); }); !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback("applyParameters", [&movement_controller](Json const& config) -> std::expected<void, StarException> {
      auto parameters = MovementParameters::create(config);
      if (!parameters)
        return std::unexpected(std::move(parameters.error()));
      return movement_controller.apply_parameters(*parameters);
    }); !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback("resetParameters", [&movement_controller](Json const& config) -> std::expected<void, StarException> {
      auto parameters = MovementParameters::create(config);
      if (!parameters)
        return std::unexpected(std::move(parameters.error()));
      return movement_controller.reset_parameters(*parameters);
    }); !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback_with_signature<float>(
        "mass", [&movement_controller]() -> decltype(auto) { return movement_controller.mass(); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback_with_signature<PolyF>(
        "collisionPoly", [&movement_controller]() -> decltype(auto) { return movement_controller.collision_poly(); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback("boundBox", [&movement_controller]() -> RectF {
      return movement_controller.collision_poly().bounding_box();
    }); !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback("collisionArea", [&movement_controller]() -> float {
      return movement_controller.collision_poly().signed_area();
    }); !registration)
    return std::unexpected(std::move(registration.error()));

  if (auto registration = callbacks.register_callback_with_signature<Vec2F>(
        "position", [&movement_controller]() -> decltype(auto) { return movement_controller.position(); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback_with_signature<float>(
        "xPosition", [&movement_controller]() -> decltype(auto) { return movement_controller.x_position(); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback_with_signature<float>(
        "yPosition", [&movement_controller]() -> decltype(auto) { return movement_controller.y_position(); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback_with_signature<Vec2F>(
        "velocity", [&movement_controller]() -> decltype(auto) { return movement_controller.velocity(); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback_with_signature<float>(
        "xVelocity", [&movement_controller]() -> decltype(auto) { return movement_controller.x_velocity(); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback_with_signature<float>(
        "yVelocity", [&movement_controller]() -> decltype(auto) { return movement_controller.y_velocity(); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback_with_signature<float>(
        "rotation", [&movement_controller]() -> decltype(auto) { return movement_controller.rotation(); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback_with_signature<PolyF>(
        "collisionBody", [&movement_controller]() -> decltype(auto) { return movement_controller.collision_body(); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback_with_signature<RectF>(
        "collisionBoundBox",
        [&movement_controller]() -> decltype(auto) { return movement_controller.collision_bound_box(); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback_with_signature<RectF>(
        "localBoundBox", [&movement_controller]() -> decltype(auto) { return movement_controller.local_bound_box(); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback_with_signature<bool>(
        "isColliding", [&movement_controller]() -> decltype(auto) { return movement_controller.is_colliding(); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback_with_signature<bool>(
        "isNullColliding", [&movement_controller]() -> decltype(auto) { return movement_controller.is_null_colliding(); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback_with_signature<bool>(
        "isCollisionStuck",
        [&movement_controller]() -> decltype(auto) { return movement_controller.is_collision_stuck(); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback_with_signature<std::optional<float>>(
        "stickingDirection",
        [&movement_controller]() -> decltype(auto) { return movement_controller.sticking_direction(); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback_with_signature<float>(
        "liquidPercentage",
        [&movement_controller]() -> decltype(auto) { return movement_controller.liquid_percentage(); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback_with_signature<LiquidId>(
        "liquidId", [&movement_controller]() -> decltype(auto) { return movement_controller.liquid_id(); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback_with_signature<bool>(
        "onGround", [&movement_controller]() -> decltype(auto) { return movement_controller.on_ground(); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback_with_signature<bool>(
        "zeroG", [&movement_controller]() -> decltype(auto) { return movement_controller.zero_g(); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback_with_signature<bool, bool>(
        "atWorldLimit", [&movement_controller](bool bottom_only) { return movement_controller.at_world_limit(bottom_only); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback_with_signature<void>(
        "resetPresentation", [&movement_controller]() { return movement_controller.reset_presentation(); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback_with_signature<void, Vec2F>(
        "setPosition", [&movement_controller](Vec2F position) { return movement_controller.set_position(position); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback_with_signature<void, float>(
        "setXPosition", [&movement_controller](float position) { return movement_controller.set_x_position(position); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback_with_signature<void, float>(
        "setYPosition", [&movement_controller](float position) { return movement_controller.set_y_position(position); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback_with_signature<void, Vec2F>(
        "translate", [&movement_controller](Vec2F offset) { return movement_controller.translate(offset); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback_with_signature<void, Vec2F>(
        "setVelocity", [&movement_controller](Vec2F velocity) { return movement_controller.set_velocity(velocity); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback_with_signature<void, float>(
        "setXVelocity", [&movement_controller](float velocity) { return movement_controller.set_x_velocity(velocity); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback_with_signature<void, float>(
        "setYVelocity", [&movement_controller](float velocity) { return movement_controller.set_y_velocity(velocity); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback_with_signature<void, Vec2F>(
        "addMomentum", [&movement_controller](Vec2F momentum) { return movement_controller.add_momentum(momentum); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback_with_signature<void, float>(
        "setRotation", [&movement_controller](float rotation) { return movement_controller.set_rotation(rotation); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback_with_signature<void, float>(
        "rotate", [&movement_controller](float rotation) { return movement_controller.rotate(rotation); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback_with_signature<void, Vec2F>(
        "accelerate",
        [&movement_controller](Vec2F acceleration) { return movement_controller.accelerate(acceleration); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback_with_signature<void, Vec2F>(
        "force", [&movement_controller](Vec2F force) { return movement_controller.force(force); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback_with_signature<void, Vec2F, float>(
        "approachVelocity",
        [&movement_controller](Vec2F velocity, float force) {
          return movement_controller.approach_velocity(velocity, force);
        });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback_with_signature<void, float, float, float, bool>(
        "approachVelocityAlongAngle",
        [&movement_controller](float angle, float velocity, float force, bool positive_only) {
          return movement_controller.approach_velocity_along_angle(angle, velocity, force, positive_only);
        });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback_with_signature<void, float, float>(
        "approachXVelocity",
        [&movement_controller](float velocity, float force) {
          return movement_controller.approach_x_velocity(velocity, force);
        });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback_with_signature<void, float, float>(
        "approachYVelocity",
        [&movement_controller](float velocity, float force) {
          return movement_controller.approach_y_velocity(velocity, force);
        });
      !registration)
    return std::unexpected(std::move(registration.error()));

  return callbacks;
}

}

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
      "parameters", [&movement_controller]() { return movement_controller.parameters().toJson(); }); !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback("applyParameters", [&movement_controller](Json const& config) -> std::expected<void, StarException> {
      auto parameters = MovementParameters::create(config);
      if (!parameters)
        return std::unexpected(std::move(parameters.error()));
      return movement_controller.applyParameters(*parameters);
    }); !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback("resetParameters", [&movement_controller](Json const& config) -> std::expected<void, StarException> {
      auto parameters = MovementParameters::create(config);
      if (!parameters)
        return std::unexpected(std::move(parameters.error()));
      return movement_controller.resetParameters(*parameters);
    }); !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback_with_signature<float>(
        "mass", [&movement_controller]() -> decltype(auto) { return movement_controller.mass(); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback_with_signature<PolyF>(
        "collisionPoly", [&movement_controller]() -> decltype(auto) { return movement_controller.collisionPoly(); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback("boundBox", [&movement_controller]() -> RectF {
      return movement_controller.collisionPoly().bounding_box();
    }); !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback("collisionArea", [&movement_controller]() -> float {
      return movement_controller.collisionPoly().signed_area();
    }); !registration)
    return std::unexpected(std::move(registration.error()));

  if (auto registration = callbacks.register_callback_with_signature<Vec2F>(
        "position", [&movement_controller]() -> decltype(auto) { return movement_controller.position(); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback_with_signature<float>(
        "xPosition", [&movement_controller]() -> decltype(auto) { return movement_controller.xPosition(); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback_with_signature<float>(
        "yPosition", [&movement_controller]() -> decltype(auto) { return movement_controller.yPosition(); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback_with_signature<Vec2F>(
        "velocity", [&movement_controller]() -> decltype(auto) { return movement_controller.velocity(); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback_with_signature<float>(
        "xVelocity", [&movement_controller]() -> decltype(auto) { return movement_controller.xVelocity(); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback_with_signature<float>(
        "yVelocity", [&movement_controller]() -> decltype(auto) { return movement_controller.yVelocity(); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback_with_signature<float>(
        "rotation", [&movement_controller]() -> decltype(auto) { return movement_controller.rotation(); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback_with_signature<PolyF>(
        "collisionBody", [&movement_controller]() -> decltype(auto) { return movement_controller.collisionBody(); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback_with_signature<RectF>(
        "collisionBoundBox",
        [&movement_controller]() -> decltype(auto) { return movement_controller.collisionBoundBox(); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback_with_signature<RectF>(
        "localBoundBox", [&movement_controller]() -> decltype(auto) { return movement_controller.localBoundBox(); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback_with_signature<bool>(
        "isColliding", [&movement_controller]() -> decltype(auto) { return movement_controller.isColliding(); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback_with_signature<bool>(
        "isNullColliding", [&movement_controller]() -> decltype(auto) { return movement_controller.isNullColliding(); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback_with_signature<bool>(
        "isCollisionStuck",
        [&movement_controller]() -> decltype(auto) { return movement_controller.isCollisionStuck(); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback_with_signature<std::optional<float>>(
        "stickingDirection",
        [&movement_controller]() -> decltype(auto) { return movement_controller.stickingDirection(); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback_with_signature<float>(
        "liquidPercentage",
        [&movement_controller]() -> decltype(auto) { return movement_controller.liquidPercentage(); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback_with_signature<LiquidId>(
        "liquidId", [&movement_controller]() -> decltype(auto) { return movement_controller.liquidId(); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback_with_signature<bool>(
        "onGround", [&movement_controller]() -> decltype(auto) { return movement_controller.onGround(); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback_with_signature<bool>(
        "zeroG", [&movement_controller]() -> decltype(auto) { return movement_controller.zeroG(); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback_with_signature<bool, bool>(
        "atWorldLimit", [&movement_controller](bool bottom_only) { return movement_controller.atWorldLimit(bottom_only); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback_with_signature<void>(
        "resetPresentation", [&movement_controller]() { return movement_controller.resetPresentation(); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback_with_signature<void, Vec2F>(
        "setPosition", [&movement_controller](Vec2F position) { return movement_controller.setPosition(position); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback_with_signature<void, float>(
        "setXPosition", [&movement_controller](float position) { return movement_controller.setXPosition(position); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback_with_signature<void, float>(
        "setYPosition", [&movement_controller](float position) { return movement_controller.setYPosition(position); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback_with_signature<void, Vec2F>(
        "translate", [&movement_controller](Vec2F offset) { return movement_controller.translate(offset); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback_with_signature<void, Vec2F>(
        "setVelocity", [&movement_controller](Vec2F velocity) { return movement_controller.setVelocity(velocity); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback_with_signature<void, float>(
        "setXVelocity", [&movement_controller](float velocity) { return movement_controller.setXVelocity(velocity); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback_with_signature<void, float>(
        "setYVelocity", [&movement_controller](float velocity) { return movement_controller.setYVelocity(velocity); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback_with_signature<void, Vec2F>(
        "addMomentum", [&movement_controller](Vec2F momentum) { return movement_controller.addMomentum(momentum); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback_with_signature<void, float>(
        "setRotation", [&movement_controller](float rotation) { return movement_controller.setRotation(rotation); });
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
          return movement_controller.approachVelocity(velocity, force);
        });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback_with_signature<void, float, float, float, bool>(
        "approachVelocityAlongAngle",
        [&movement_controller](float angle, float velocity, float force, bool positive_only) {
          return movement_controller.approachVelocityAlongAngle(angle, velocity, force, positive_only);
        });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback_with_signature<void, float, float>(
        "approachXVelocity",
        [&movement_controller](float velocity, float force) {
          return movement_controller.approachXVelocity(velocity, force);
        });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback_with_signature<void, float, float>(
        "approachYVelocity",
        [&movement_controller](float velocity, float force) {
          return movement_controller.approachYVelocity(velocity, force);
        });
      !registration)
    return std::unexpected(std::move(registration.error()));

  return callbacks;
}

}

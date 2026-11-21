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

std::expected<LuaCallbacks, StarException> LuaBindings::makeMovementControllerCallbacks(MovementController& movementController) {
  LuaCallbacks callbacks;

  if (auto registration = callbacks.register_callback(
      "parameters", [&movementController]() { return movementController.parameters().toJson(); }); !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback("applyParameters", [&movementController](Json const& config) -> std::expected<void, StarException> {
      auto parameters = MovementParameters::create(config);
      if (!parameters)
        return std::unexpected(std::move(parameters.error()));
      return movementController.applyParameters(*parameters);
    }); !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback("resetParameters", [&movementController](Json const& config) -> std::expected<void, StarException> {
      auto parameters = MovementParameters::create(config);
      if (!parameters)
        return std::unexpected(std::move(parameters.error()));
      return movementController.resetParameters(*parameters);
    }); !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback_with_signature<float>(
        "mass", [&movementController]() -> decltype(auto) { return movementController.mass(); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback_with_signature<PolyF>(
        "collisionPoly", [&movementController]() -> decltype(auto) { return movementController.collisionPoly(); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback("boundBox", [&movementController]() -> RectF {
      return movementController.collisionPoly().bounding_box();
    }); !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback("collisionArea", [&movementController]() -> float {
      return movementController.collisionPoly().signed_area();
    }); !registration)
    return std::unexpected(std::move(registration.error()));

  if (auto registration = callbacks.register_callback_with_signature<Vec2F>(
        "position", [&movementController]() -> decltype(auto) { return movementController.position(); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback_with_signature<float>(
        "xPosition", [&movementController]() -> decltype(auto) { return movementController.xPosition(); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback_with_signature<float>(
        "yPosition", [&movementController]() -> decltype(auto) { return movementController.yPosition(); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback_with_signature<Vec2F>(
        "velocity", [&movementController]() -> decltype(auto) { return movementController.velocity(); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback_with_signature<float>(
        "xVelocity", [&movementController]() -> decltype(auto) { return movementController.xVelocity(); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback_with_signature<float>(
        "yVelocity", [&movementController]() -> decltype(auto) { return movementController.yVelocity(); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback_with_signature<float>(
        "rotation", [&movementController]() -> decltype(auto) { return movementController.rotation(); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback_with_signature<PolyF>(
        "collisionBody", [&movementController]() -> decltype(auto) { return movementController.collisionBody(); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback_with_signature<RectF>(
        "collisionBoundBox",
        [&movementController]() -> decltype(auto) { return movementController.collisionBoundBox(); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback_with_signature<RectF>(
        "localBoundBox", [&movementController]() -> decltype(auto) { return movementController.localBoundBox(); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback_with_signature<bool>(
        "isColliding", [&movementController]() -> decltype(auto) { return movementController.isColliding(); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback_with_signature<bool>(
        "isNullColliding", [&movementController]() -> decltype(auto) { return movementController.isNullColliding(); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback_with_signature<bool>(
        "isCollisionStuck",
        [&movementController]() -> decltype(auto) { return movementController.isCollisionStuck(); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback_with_signature<std::optional<float>>(
        "stickingDirection",
        [&movementController]() -> decltype(auto) { return movementController.stickingDirection(); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback_with_signature<float>(
        "liquidPercentage",
        [&movementController]() -> decltype(auto) { return movementController.liquidPercentage(); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback_with_signature<LiquidId>(
        "liquidId", [&movementController]() -> decltype(auto) { return movementController.liquidId(); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback_with_signature<bool>(
        "onGround", [&movementController]() -> decltype(auto) { return movementController.onGround(); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback_with_signature<bool>(
        "zeroG", [&movementController]() -> decltype(auto) { return movementController.zeroG(); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback_with_signature<bool, bool>(
        "atWorldLimit", [&movementController](bool bottomOnly) { return movementController.atWorldLimit(bottomOnly); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback_with_signature<void>(
        "resetPresentation", [&movementController]() { return movementController.resetPresentation(); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback_with_signature<void, Vec2F>(
        "setPosition", [&movementController](Vec2F position) { return movementController.setPosition(position); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback_with_signature<void, float>(
        "setXPosition", [&movementController](float position) { return movementController.setXPosition(position); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback_with_signature<void, float>(
        "setYPosition", [&movementController](float position) { return movementController.setYPosition(position); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback_with_signature<void, Vec2F>(
        "translate", [&movementController](Vec2F offset) { return movementController.translate(offset); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback_with_signature<void, Vec2F>(
        "setVelocity", [&movementController](Vec2F velocity) { return movementController.setVelocity(velocity); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback_with_signature<void, float>(
        "setXVelocity", [&movementController](float velocity) { return movementController.setXVelocity(velocity); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback_with_signature<void, float>(
        "setYVelocity", [&movementController](float velocity) { return movementController.setYVelocity(velocity); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback_with_signature<void, Vec2F>(
        "addMomentum", [&movementController](Vec2F momentum) { return movementController.addMomentum(momentum); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback_with_signature<void, float>(
        "setRotation", [&movementController](float rotation) { return movementController.setRotation(rotation); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback_with_signature<void, float>(
        "rotate", [&movementController](float rotation) { return movementController.rotate(rotation); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback_with_signature<void, Vec2F>(
        "accelerate",
        [&movementController](Vec2F acceleration) { return movementController.accelerate(acceleration); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback_with_signature<void, Vec2F>(
        "force", [&movementController](Vec2F force) { return movementController.force(force); });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback_with_signature<void, Vec2F, float>(
        "approachVelocity",
        [&movementController](Vec2F velocity, float force) {
          return movementController.approachVelocity(velocity, force);
        });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback_with_signature<void, float, float, float, bool>(
        "approachVelocityAlongAngle",
        [&movementController](float angle, float velocity, float force, bool positiveOnly) {
          return movementController.approachVelocityAlongAngle(angle, velocity, force, positiveOnly);
        });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback_with_signature<void, float, float>(
        "approachXVelocity",
        [&movementController](float velocity, float force) {
          return movementController.approachXVelocity(velocity, force);
        });
      !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback_with_signature<void, float, float>(
        "approachYVelocity",
        [&movementController](float velocity, float force) {
          return movementController.approachYVelocity(velocity, force);
        });
      !registration)
    return std::unexpected(std::move(registration.error()));

  return callbacks;
}

}

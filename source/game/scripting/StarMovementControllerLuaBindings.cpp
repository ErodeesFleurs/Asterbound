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

  if (auto registration = callbacks.registerCallback(
      "parameters", [&movementController]() { return movementController.parameters().toJson(); }); !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.registerCallback("applyParameters", [&movementController](Json const& config) -> std::expected<void, StarException> {
      auto parameters = MovementParameters::create(config);
      if (!parameters)
        return std::unexpected(std::move(parameters.error()));
      return movementController.applyParameters(*parameters);
    }); !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.registerCallback("resetParameters", [&movementController](Json const& config) -> std::expected<void, StarException> {
      auto parameters = MovementParameters::create(config);
      if (!parameters)
        return std::unexpected(std::move(parameters.error()));
      return movementController.resetParameters(*parameters);
    }); !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.registerCallbackWithSignature<float>("mass", std::bind(&MovementController::mass, std::ref(movementController))); !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.registerCallbackWithSignature<PolyF>(
      "collisionPoly", std::bind(&MovementController::collisionPoly, std::ref(movementController))); !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.registerCallback("boundBox", [&movementController]() -> RectF {
      return movementController.collisionPoly().boundBox();
    }); !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.registerCallback("collisionArea", [&movementController]() -> float {
      return movementController.collisionPoly().convexArea();
    }); !registration)
    return std::unexpected(std::move(registration.error()));

  if (auto registration = callbacks.registerCallbackWithSignature<Vec2F>("position", std::bind(&MovementController::position, std::ref(movementController))); !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.registerCallbackWithSignature<float>("xPosition", std::bind(&MovementController::xPosition, std::ref(movementController))); !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.registerCallbackWithSignature<float>("yPosition", std::bind(&MovementController::yPosition, std::ref(movementController))); !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.registerCallbackWithSignature<Vec2F>("velocity", std::bind(&MovementController::velocity, std::ref(movementController))); !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.registerCallbackWithSignature<float>("xVelocity", std::bind(&MovementController::xVelocity, std::ref(movementController))); !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.registerCallbackWithSignature<float>("yVelocity", std::bind(&MovementController::yVelocity, std::ref(movementController))); !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.registerCallbackWithSignature<float>("rotation", std::bind(&MovementController::rotation, std::ref(movementController))); !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.registerCallbackWithSignature<PolyF>(
      "collisionBody", std::bind(&MovementController::collisionBody, std::ref(movementController))); !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.registerCallbackWithSignature<RectF>(
      "collisionBoundBox", std::bind(&MovementController::collisionBoundBox, std::ref(movementController))); !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.registerCallbackWithSignature<RectF>(
      "localBoundBox", std::bind(&MovementController::localBoundBox, std::ref(movementController))); !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.registerCallbackWithSignature<bool>(
      "isColliding", std::bind(&MovementController::isColliding, std::ref(movementController))); !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.registerCallbackWithSignature<bool>(
      "isNullColliding", std::bind(&MovementController::isNullColliding, std::ref(movementController))); !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.registerCallbackWithSignature<bool>(
      "isCollisionStuck", std::bind(&MovementController::isCollisionStuck, std::ref(movementController))); !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.registerCallbackWithSignature<std::optional<float>>(
      "stickingDirection", std::bind(&MovementController::stickingDirection, std::ref(movementController))); !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.registerCallbackWithSignature<float>(
      "liquidPercentage", std::bind(&MovementController::liquidPercentage, std::ref(movementController))); !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.registerCallbackWithSignature<LiquidId>(
      "liquidId", std::bind(&MovementController::liquidId, std::ref(movementController))); !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.registerCallbackWithSignature<bool>("onGround", std::bind(&MovementController::onGround, std::ref(movementController))); !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.registerCallbackWithSignature<bool>("zeroG", std::bind(&MovementController::zeroG, std::ref(movementController))); !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.registerCallbackWithSignature<bool, bool>("atWorldLimit", std::bind(&MovementController::atWorldLimit, std::ref(movementController), std::placeholders::_1)); !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.registerCallbackWithSignature<void>(
      "resetPresentation", std::bind(&MovementController::resetPresentation, std::ref(movementController))); !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.registerCallbackWithSignature<void, Vec2F>(
      "setPosition", std::bind(&MovementController::setPosition, std::ref(movementController), std::placeholders::_1)); !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.registerCallbackWithSignature<void, float>(
      "setXPosition", std::bind(&MovementController::setXPosition, std::ref(movementController), std::placeholders::_1)); !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.registerCallbackWithSignature<void, float>(
      "setYPosition", std::bind(&MovementController::setYPosition, std::ref(movementController), std::placeholders::_1)); !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.registerCallbackWithSignature<void, Vec2F>(
      "translate", std::bind(&MovementController::translate, std::ref(movementController), std::placeholders::_1)); !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.registerCallbackWithSignature<void, Vec2F>(
      "setVelocity", std::bind(&MovementController::setVelocity, std::ref(movementController), std::placeholders::_1)); !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.registerCallbackWithSignature<void, float>(
      "setXVelocity", std::bind(&MovementController::setXVelocity, std::ref(movementController), std::placeholders::_1)); !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.registerCallbackWithSignature<void, float>(
      "setYVelocity", std::bind(&MovementController::setYVelocity, std::ref(movementController), std::placeholders::_1)); !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.registerCallbackWithSignature<void, Vec2F>(
      "addMomentum", std::bind(&MovementController::addMomentum, std::ref(movementController), std::placeholders::_1)); !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.registerCallbackWithSignature<void, float>(
      "setRotation", std::bind(&MovementController::setRotation, std::ref(movementController), std::placeholders::_1)); !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.registerCallbackWithSignature<void, float>(
      "rotate", std::bind(&MovementController::rotate, std::ref(movementController), std::placeholders::_1)); !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.registerCallbackWithSignature<void, Vec2F>(
      "accelerate", std::bind(&MovementController::accelerate, std::ref(movementController), std::placeholders::_1)); !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.registerCallbackWithSignature<void, Vec2F>(
      "force", std::bind(&MovementController::force, std::ref(movementController), std::placeholders::_1)); !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.registerCallbackWithSignature<void, Vec2F, float>(
      "approachVelocity", std::bind(&MovementController::approachVelocity, std::ref(movementController), std::placeholders::_1, std::placeholders::_2)); !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.registerCallbackWithSignature<void, float, float, float, bool>("approachVelocityAlongAngle",
      std::bind(&MovementController::approachVelocityAlongAngle, std::ref(movementController), std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4)); !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.registerCallbackWithSignature<void, float, float>(
      "approachXVelocity", std::bind(&MovementController::approachXVelocity, std::ref(movementController), std::placeholders::_1, std::placeholders::_2)); !registration)
    return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.registerCallbackWithSignature<void, float, float>(
      "approachYVelocity", std::bind(&MovementController::approachYVelocity, std::ref(movementController), std::placeholders::_1, std::placeholders::_2)); !registration)
    return std::unexpected(std::move(registration.error()));

  return callbacks;
}

}

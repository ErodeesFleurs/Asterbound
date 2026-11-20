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
import star.version;
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
  auto jumpSpeedResult = config.opt_float("jumpSpeed");
  if (!jumpSpeedResult) return std::unexpected(std::move(jumpSpeedResult.error()));
  value.jumpSpeed = *jumpSpeedResult;
  auto jumpControlForceResult = config.opt_float("jumpControlForce");
  if (!jumpControlForceResult) return std::unexpected(std::move(jumpControlForceResult.error()));
  value.jumpControlForce = *jumpControlForceResult;
  auto jumpInitialPercentageResult = config.opt_float("jumpInitialPercentage");
  if (!jumpInitialPercentageResult) return std::unexpected(std::move(jumpInitialPercentageResult.error()));
  value.jumpInitialPercentage = *jumpInitialPercentageResult;
  auto jumpHoldTimeResult = config.opt_float("jumpHoldTime");
  if (!jumpHoldTimeResult) return std::unexpected(std::move(jumpHoldTimeResult.error()));
  value.jumpHoldTime = *jumpHoldTimeResult;
  auto jumpTotalHoldTimeResult = config.opt_float("jumpTotalHoldTime");
  if (!jumpTotalHoldTimeResult) return std::unexpected(std::move(jumpTotalHoldTimeResult.error()));
  value.jumpTotalHoldTime = *jumpTotalHoldTimeResult;
  auto multiJumpResult = config.opt_bool("multiJump");
  if (!multiJumpResult) return std::unexpected(std::move(multiJumpResult.error()));
  value.multiJump = *multiJumpResult;
  auto reJumpDelayResult = config.opt_float("reJumpDelay");
  if (!reJumpDelayResult) return std::unexpected(std::move(reJumpDelayResult.error()));
  value.reJumpDelay = *reJumpDelayResult;
  auto autoJumpResult = config.opt_bool("autoJump");
  if (!autoJumpResult) return std::unexpected(std::move(autoJumpResult.error()));
  value.autoJump = *autoJumpResult;
  auto collisionCancelledResult = config.opt_bool("collisionCancelled");
  if (!collisionCancelledResult) return std::unexpected(std::move(collisionCancelledResult.error()));
  value.collisionCancelled = *collisionCancelledResult;
  return value;
}

Json ActorJumpProfile::toJson() const {
  return JsonObject{
    {"jumpSpeed", json_from_optional(jumpSpeed)},
    {"jumpControlForce", json_from_optional(jumpControlForce)},
    {"jumpInitialPercentage", json_from_optional(jumpInitialPercentage)},
    {"jumpHoldTime", json_from_optional(jumpHoldTime)},
    {"jumpTotalHoldTime", json_from_optional(jumpTotalHoldTime)},
    {"multiJump", json_from_optional(multiJump)},
    {"reJumpDelay", json_from_optional(reJumpDelay)},
    {"autoJump", json_from_optional(autoJump)},
    {"collisionCancelled", json_from_optional(collisionCancelled)}
  };
}

ActorJumpProfile ActorJumpProfile::merge(ActorJumpProfile const& rhs) const {
  ActorJumpProfile merged;

  merged.jumpSpeed = (rhs.jumpSpeed ? rhs.jumpSpeed : jumpSpeed);
  merged.jumpControlForce = (rhs.jumpControlForce ? rhs.jumpControlForce : jumpControlForce);
  merged.jumpInitialPercentage = (rhs.jumpInitialPercentage ? rhs.jumpInitialPercentage : jumpInitialPercentage);
  merged.jumpHoldTime = (rhs.jumpHoldTime ? rhs.jumpHoldTime : jumpHoldTime);
  merged.jumpTotalHoldTime = (rhs.jumpTotalHoldTime ? rhs.jumpTotalHoldTime : jumpTotalHoldTime);
  merged.multiJump = (rhs.multiJump ? rhs.multiJump : multiJump);
  merged.reJumpDelay = (rhs.reJumpDelay ? rhs.reJumpDelay : reJumpDelay);
  merged.autoJump = (rhs.autoJump ? rhs.autoJump : autoJump);
  merged.collisionCancelled = (rhs.collisionCancelled ? rhs.collisionCancelled : collisionCancelled);

  return merged;
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, ActorJumpProfile& movementParameters) {
  if (auto result = ds.read(movementParameters.jumpSpeed); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.read(movementParameters.jumpControlForce); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.read(movementParameters.jumpInitialPercentage); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.read(movementParameters.jumpHoldTime); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.read(movementParameters.jumpTotalHoldTime); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.read(movementParameters.multiJump); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.read(movementParameters.reJumpDelay); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.read(movementParameters.autoJump); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.read(movementParameters.collisionCancelled); !result) return std::unexpected(std::move(result.error()));

  return std::ref(ds);
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, ActorJumpProfile const& movementParameters) {
  if (auto result = ds.write(movementParameters.jumpSpeed); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.write(movementParameters.jumpControlForce); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.write(movementParameters.jumpInitialPercentage); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.write(movementParameters.jumpHoldTime); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.write(movementParameters.jumpTotalHoldTime); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.write(movementParameters.multiJump); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.write(movementParameters.reJumpDelay); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.write(movementParameters.autoJump); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.write(movementParameters.collisionCancelled); !result) return std::unexpected(std::move(result.error()));

  return std::ref(ds);
}

std::expected<ActorMovementParameters, StarException> ActorMovementParameters::sensibleDefaults(Root& root) {
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

  auto massResult = config.opt_float("mass");
  if (!massResult) return std::unexpected(std::move(massResult.error()));
  value.mass = *massResult;
  auto gravityMultiplierResult = config.opt_float("gravityMultiplier");
  if (!gravityMultiplierResult) return std::unexpected(std::move(gravityMultiplierResult.error()));
  value.gravityMultiplier = *gravityMultiplierResult;
  auto liquidBuoyancyResult = config.opt_float("liquidBuoyancy");
  if (!liquidBuoyancyResult) return std::unexpected(std::move(liquidBuoyancyResult.error()));
  value.liquidBuoyancy = *liquidBuoyancyResult;
  auto airBuoyancyResult = config.opt_float("airBuoyancy");
  if (!airBuoyancyResult) return std::unexpected(std::move(airBuoyancyResult.error()));
  value.airBuoyancy = *airBuoyancyResult;
  auto bounceFactorResult = config.opt_float("bounceFactor");
  if (!bounceFactorResult) return std::unexpected(std::move(bounceFactorResult.error()));
  value.bounceFactor = *bounceFactorResult;
  auto stopOnFirstBounceResult = config.opt_bool("stopOnFirstBounce");
  if (!stopOnFirstBounceResult) return std::unexpected(std::move(stopOnFirstBounceResult.error()));
  value.stopOnFirstBounce = *stopOnFirstBounceResult;
  auto enableSurfaceSlopeCorrectionResult = config.opt_bool("enableSurfaceSlopeCorrection");
  if (!enableSurfaceSlopeCorrectionResult) return std::unexpected(std::move(enableSurfaceSlopeCorrectionResult.error()));
  value.enableSurfaceSlopeCorrection = *enableSurfaceSlopeCorrectionResult;
  auto slopeSlidingFactorResult = config.opt_float("slopeSlidingFactor");
  if (!slopeSlidingFactorResult) return std::unexpected(std::move(slopeSlidingFactorResult.error()));
  value.slopeSlidingFactor = *slopeSlidingFactorResult;
  auto maxMovementPerStepResult = config.opt_float("maxMovementPerStep");
  if (!maxMovementPerStepResult) return std::unexpected(std::move(maxMovementPerStepResult.error()));
  value.maxMovementPerStep = *maxMovementPerStepResult;
  auto maximumCorrectionResult = config.opt_float("maximumCorrection");
  if (!maximumCorrectionResult) return std::unexpected(std::move(maximumCorrectionResult.error()));
  value.maximumCorrection = *maximumCorrectionResult;
  auto speedLimitResult = config.opt_float("speedLimit");
  if (!speedLimitResult) return std::unexpected(std::move(speedLimitResult.error()));
  value.speedLimit = *speedLimitResult;

  // "collisionPoly" is used as a synonym for setting both the standing and
  // crouching polys.

  auto collisionPolyConfigResult = config.get("collisionPoly", {});
  if (!collisionPolyConfigResult) return std::unexpected(std::move(collisionPolyConfigResult.error()));
  auto const& collisionPolyConfig = *collisionPolyConfigResult;
  auto standingPolyConfigResult = config.get("standingPoly", {});
  if (!standingPolyConfigResult) return std::unexpected(std::move(standingPolyConfigResult.error()));
  auto const& standingPolyConfig = *standingPolyConfigResult;
  auto crouchingPolyConfigResult = config.get("crouchingPoly", {});
  if (!crouchingPolyConfigResult) return std::unexpected(std::move(crouchingPolyConfigResult.error()));
  auto const& crouchingPolyConfig = *crouchingPolyConfigResult;

  if (!standingPolyConfig.is_null())
{
    auto standingPolyValue = json_to_polyf(standingPolyConfig);
    if (!standingPolyValue) return std::unexpected(std::move(standingPolyValue.error()));
    value.standingPoly = std::move(*standingPolyValue);
  }
  else if (!collisionPolyConfig.is_null())
{
    auto standingPolyValue = json_to_polyf(collisionPolyConfig);
    if (!standingPolyValue) return std::unexpected(std::move(standingPolyValue.error()));
    value.standingPoly = std::move(*standingPolyValue);
  }

  if (!crouchingPolyConfig.is_null())
{
    auto crouchingPolyValue = json_to_polyf(crouchingPolyConfig);
    if (!crouchingPolyValue) return std::unexpected(std::move(crouchingPolyValue.error()));
    value.crouchingPoly = std::move(*crouchingPolyValue);
  }
  else if (!collisionPolyConfig.is_null())
{
    auto crouchingPolyValue = json_to_polyf(collisionPolyConfig);
    if (!crouchingPolyValue) return std::unexpected(std::move(crouchingPolyValue.error()));
    value.crouchingPoly = std::move(*crouchingPolyValue);
  }

  auto stickyCollisionResult = config.opt_bool("stickyCollision");
  if (!stickyCollisionResult) return std::unexpected(std::move(stickyCollisionResult.error()));
  value.stickyCollision = *stickyCollisionResult;
  auto stickyForceResult = config.opt_float("stickyForce");
  if (!stickyForceResult) return std::unexpected(std::move(stickyForceResult.error()));
  value.stickyForce = *stickyForceResult;

  auto walkSpeedResult = config.opt_float("walkSpeed");
  if (!walkSpeedResult) return std::unexpected(std::move(walkSpeedResult.error()));
  value.walkSpeed = *walkSpeedResult;
  auto runSpeedResult = config.opt_float("runSpeed");
  if (!runSpeedResult) return std::unexpected(std::move(runSpeedResult.error()));
  value.runSpeed = *runSpeedResult;
  auto flySpeedResult = config.opt_float("flySpeed");
  if (!flySpeedResult) return std::unexpected(std::move(flySpeedResult.error()));
  value.flySpeed = *flySpeedResult;
  auto airFrictionResult = config.opt_float("airFriction");
  if (!airFrictionResult) return std::unexpected(std::move(airFrictionResult.error()));
  value.airFriction = *airFrictionResult;
  auto liquidFrictionResult = config.opt_float("liquidFriction");
  if (!liquidFrictionResult) return std::unexpected(std::move(liquidFrictionResult.error()));
  value.liquidFriction = *liquidFrictionResult;
  auto minimumLiquidPercentageResult = config.opt_float("minimumLiquidPercentage");
  if (!minimumLiquidPercentageResult) return std::unexpected(std::move(minimumLiquidPercentageResult.error()));
  value.minimumLiquidPercentage = *minimumLiquidPercentageResult;
  auto liquidImpedanceResult = config.opt_float("liquidImpedance");
  if (!liquidImpedanceResult) return std::unexpected(std::move(liquidImpedanceResult.error()));
  value.liquidImpedance = *liquidImpedanceResult;
  auto normalGroundFrictionResult = config.opt_float("normalGroundFriction");
  if (!normalGroundFrictionResult) return std::unexpected(std::move(normalGroundFrictionResult.error()));
  value.normalGroundFriction = *normalGroundFrictionResult;
  auto ambulatingGroundFrictionResult = config.opt_float("ambulatingGroundFriction");
  if (!ambulatingGroundFrictionResult) return std::unexpected(std::move(ambulatingGroundFrictionResult.error()));
  value.ambulatingGroundFriction = *ambulatingGroundFrictionResult;
  auto groundForceResult = config.opt_float("groundForce");
  if (!groundForceResult) return std::unexpected(std::move(groundForceResult.error()));
  value.groundForce = *groundForceResult;
  auto airForceResult = config.opt_float("airForce");
  if (!airForceResult) return std::unexpected(std::move(airForceResult.error()));
  value.airForce = *airForceResult;
  auto liquidForceResult = config.opt_float("liquidForce");
  if (!liquidForceResult) return std::unexpected(std::move(liquidForceResult.error()));
  value.liquidForce = *liquidForceResult;

  auto airJumpProfileConfig = config.opt("airJumpProfile");
  if (!airJumpProfileConfig) return std::unexpected(std::move(airJumpProfileConfig.error()));
  if (*airJumpProfileConfig) {
    auto airJumpProfileValue = ActorJumpProfile::create(**airJumpProfileConfig);
    if (!airJumpProfileValue) return std::unexpected(std::move(airJumpProfileValue.error()));
    value.airJumpProfile = std::move(*airJumpProfileValue);
  }
  auto liquidJumpProfileConfig = config.opt("liquidJumpProfile");
  if (!liquidJumpProfileConfig) return std::unexpected(std::move(liquidJumpProfileConfig.error()));
  if (*liquidJumpProfileConfig) {
    auto liquidJumpProfileValue = ActorJumpProfile::create(**liquidJumpProfileConfig);
    if (!liquidJumpProfileValue) return std::unexpected(std::move(liquidJumpProfileValue.error()));
    value.liquidJumpProfile = std::move(*liquidJumpProfileValue);
  }

  auto fallStatusSpeedMinResult = config.opt_float("fallStatusSpeedMin");
  if (!fallStatusSpeedMinResult) return std::unexpected(std::move(fallStatusSpeedMinResult.error()));
  value.fallStatusSpeedMin = *fallStatusSpeedMinResult;
  auto fallThroughSustainFramesResult = config.opt_int("fallThroughSustainFrames");
  if (!fallThroughSustainFramesResult) return std::unexpected(std::move(fallThroughSustainFramesResult.error()));
  value.fallThroughSustainFrames = *fallThroughSustainFramesResult;
  auto maximumPlatformCorrectionResult = config.opt_float("maximumPlatformCorrection");
  if (!maximumPlatformCorrectionResult) return std::unexpected(std::move(maximumPlatformCorrectionResult.error()));
  value.maximumPlatformCorrection = *maximumPlatformCorrectionResult;
  auto maximumPlatformCorrectionVelocityFactorResult = config.opt_float("maximumPlatformCorrectionVelocityFactor");
  if (!maximumPlatformCorrectionVelocityFactorResult) return std::unexpected(std::move(maximumPlatformCorrectionVelocityFactorResult.error()));
  value.maximumPlatformCorrectionVelocityFactor = *maximumPlatformCorrectionVelocityFactorResult;

  auto physicsEffectCategoriesConfig = config.opt("physicsEffectCategories");
  if (!physicsEffectCategoriesConfig) return std::unexpected(std::move(physicsEffectCategoriesConfig.error()));
  if (*physicsEffectCategoriesConfig) {
    auto physicsEffectCategoriesValue = json_to_string_set(**physicsEffectCategoriesConfig);
    if (!physicsEffectCategoriesValue) return std::unexpected(std::move(physicsEffectCategoriesValue.error()));
    value.physicsEffectCategories = std::move(*physicsEffectCategoriesValue);
  }

  auto groundMovementMinimumSustainResult = config.opt_float("groundMovementMinimumSustain");
  if (!groundMovementMinimumSustainResult) return std::unexpected(std::move(groundMovementMinimumSustainResult.error()));
  value.groundMovementMinimumSustain = *groundMovementMinimumSustainResult;
  auto groundMovementMaximumSustainResult = config.opt_float("groundMovementMaximumSustain");
  if (!groundMovementMaximumSustainResult) return std::unexpected(std::move(groundMovementMaximumSustainResult.error()));
  value.groundMovementMaximumSustain = *groundMovementMaximumSustainResult;
  auto groundMovementCheckDistanceResult = config.opt_float("groundMovementCheckDistance");
  if (!groundMovementCheckDistanceResult) return std::unexpected(std::move(groundMovementCheckDistanceResult.error()));
  value.groundMovementCheckDistance = *groundMovementCheckDistanceResult;

  auto collisionEnabledResult = config.opt_bool("collisionEnabled");
  if (!collisionEnabledResult) return std::unexpected(std::move(collisionEnabledResult.error()));
  value.collisionEnabled = *collisionEnabledResult;
  auto frictionEnabledResult = config.opt_bool("frictionEnabled");
  if (!frictionEnabledResult) return std::unexpected(std::move(frictionEnabledResult.error()));
  value.frictionEnabled = *frictionEnabledResult;
  auto gravityEnabledResult = config.opt_bool("gravityEnabled");
  if (!gravityEnabledResult) return std::unexpected(std::move(gravityEnabledResult.error()));
  value.gravityEnabled = *gravityEnabledResult;

  auto pathExploreRateResult = config.opt_float("pathExploreRate");
  if (!pathExploreRateResult) return std::unexpected(std::move(pathExploreRateResult.error()));
  value.pathExploreRate = *pathExploreRateResult;
  return value;
}

std::expected<Json, StarException> ActorMovementParameters::toJson() const {
  auto standingPolyJson = json_from_optional(standingPoly, json_from_polyf);
  if (!standingPolyJson) return std::unexpected(std::move(standingPolyJson.error()));
  auto crouchingPolyJson = json_from_optional(crouchingPoly, json_from_polyf);
  if (!crouchingPolyJson) return std::unexpected(std::move(crouchingPolyJson.error()));
  auto physicsEffectCategoriesJson = json_from_optional(physicsEffectCategories, json_from_string_set);
  if (!physicsEffectCategoriesJson) return std::unexpected(std::move(physicsEffectCategoriesJson.error()));

  return JsonObject{{"mass", json_from_optional(mass)},
      {"gravityMultiplier", json_from_optional(gravityMultiplier)},
      {"liquidBuoyancy", json_from_optional(liquidBuoyancy)},
      {"airBuoyancy", json_from_optional(airBuoyancy)},
      {"bounceFactor", json_from_optional(bounceFactor)},
      {"stopOnFirstBounce", json_from_optional(stopOnFirstBounce)},
      {"enableSurfaceSlopeCorrection", json_from_optional(enableSurfaceSlopeCorrection)},
      {"slopeSlidingFactor", json_from_optional(slopeSlidingFactor)},
      {"maxMovementPerStep", json_from_optional(maxMovementPerStep)},
      {"maximumCorrection", json_from_optional(maximumCorrection)},
      {"speedLimit", json_from_optional(speedLimit)},
      {"standingPoly", *standingPolyJson},
      {"crouchingPoly", *crouchingPolyJson},
      {"stickyCollision", json_from_optional(stickyCollision)},
      {"stickyForce", json_from_optional(stickyForce)},
      {"walkSpeed", json_from_optional(walkSpeed)},
      {"runSpeed", json_from_optional(runSpeed)},
      {"flySpeed", json_from_optional(flySpeed)},
      {"airFriction", json_from_optional(airFriction)},
      {"liquidFriction", json_from_optional(liquidFriction)},
      {"minimumLiquidPercentage", json_from_optional(minimumLiquidPercentage)},
      {"liquidImpedance", json_from_optional(liquidImpedance)},
      {"normalGroundFriction", json_from_optional(normalGroundFriction)},
      {"ambulatingGroundFriction", json_from_optional(ambulatingGroundFriction)},
      {"groundForce", json_from_optional(groundForce)},
      {"airForce", json_from_optional(airForce)},
      {"liquidForce", json_from_optional(liquidForce)},
      {"airJumpProfile", airJumpProfile.toJson()},
      {"liquidJumpProfile", liquidJumpProfile.toJson()},
      {"fallStatusSpeedMin", json_from_optional(fallStatusSpeedMin)},
      {"fallThroughSustainFrames", json_from_optional(fallThroughSustainFrames)},
      {"maximumPlatformCorrection", json_from_optional(maximumPlatformCorrection)},
      {"maximumPlatformCorrectionVelocityFactor", json_from_optional(maximumPlatformCorrectionVelocityFactor)},
      {"physicsEffectCategories", *physicsEffectCategoriesJson},
      {"groundMovementMinimumSustain", json_from_optional(groundMovementMinimumSustain)},
      {"groundMovementMaximumSustain", json_from_optional(groundMovementMaximumSustain)},
      {"groundMovementCheckDistance", json_from_optional(groundMovementCheckDistance)},
      {"collisionEnabled", json_from_optional(collisionEnabled)},
      {"frictionEnabled", json_from_optional(frictionEnabled)},
      {"gravityEnabled", json_from_optional(gravityEnabled)},
      {"pathExploreRate", json_from_optional(pathExploreRate)}};
}

ActorMovementParameters ActorMovementParameters::merge(ActorMovementParameters const& rhs) const {
  ActorMovementParameters merged;

  merged.mass = (rhs.mass ? rhs.mass : mass);
  merged.gravityMultiplier = (rhs.gravityMultiplier ? rhs.gravityMultiplier : gravityMultiplier);
  merged.liquidBuoyancy = (rhs.liquidBuoyancy ? rhs.liquidBuoyancy : liquidBuoyancy);
  merged.airBuoyancy = (rhs.airBuoyancy ? rhs.airBuoyancy : airBuoyancy);
  merged.bounceFactor = (rhs.bounceFactor ? rhs.bounceFactor : bounceFactor);
  merged.stopOnFirstBounce = (rhs.stopOnFirstBounce ? rhs.stopOnFirstBounce : stopOnFirstBounce);
  merged.enableSurfaceSlopeCorrection = (rhs.enableSurfaceSlopeCorrection ? rhs.enableSurfaceSlopeCorrection : enableSurfaceSlopeCorrection);
  merged.slopeSlidingFactor = (rhs.slopeSlidingFactor ? rhs.slopeSlidingFactor : slopeSlidingFactor);
  merged.maxMovementPerStep = (rhs.maxMovementPerStep ? rhs.maxMovementPerStep : maxMovementPerStep);
  merged.maximumCorrection = (rhs.maximumCorrection ? rhs.maximumCorrection : maximumCorrection);
  merged.speedLimit = (rhs.speedLimit ? rhs.speedLimit : speedLimit);
  merged.standingPoly = (rhs.standingPoly ? rhs.standingPoly : standingPoly);
  merged.crouchingPoly = (rhs.crouchingPoly ? rhs.crouchingPoly : crouchingPoly);
  merged.stickyCollision = (rhs.stickyCollision ? rhs.stickyCollision : stickyCollision);
  merged.stickyForce = (rhs.stickyForce ? rhs.stickyForce : stickyForce);
  merged.walkSpeed = (rhs.walkSpeed ? rhs.walkSpeed : walkSpeed);
  merged.runSpeed = (rhs.runSpeed ? rhs.runSpeed : runSpeed);
  merged.flySpeed = (rhs.flySpeed ? rhs.flySpeed : flySpeed);
  merged.airFriction = (rhs.airFriction ? rhs.airFriction : airFriction);
  merged.liquidFriction = (rhs.liquidFriction ? rhs.liquidFriction : liquidFriction);
  merged.minimumLiquidPercentage = (rhs.minimumLiquidPercentage ? rhs.minimumLiquidPercentage : minimumLiquidPercentage);
  merged.liquidImpedance = (rhs.liquidImpedance ? rhs.liquidImpedance : liquidImpedance);
  merged.normalGroundFriction = (rhs.normalGroundFriction ? rhs.normalGroundFriction : normalGroundFriction);
  merged.ambulatingGroundFriction = (rhs.ambulatingGroundFriction ? rhs.ambulatingGroundFriction : ambulatingGroundFriction);
  merged.groundForce = (rhs.groundForce ? rhs.groundForce : groundForce);
  merged.airForce = (rhs.airForce ? rhs.airForce : airForce);
  merged.liquidForce = (rhs.liquidForce ? rhs.liquidForce : liquidForce);

  merged.airJumpProfile = airJumpProfile.merge(rhs.airJumpProfile);
  merged.liquidJumpProfile = liquidJumpProfile.merge(rhs.liquidJumpProfile);

  merged.fallStatusSpeedMin = (rhs.fallStatusSpeedMin ? rhs.fallStatusSpeedMin : fallStatusSpeedMin);
  merged.fallThroughSustainFrames = (rhs.fallThroughSustainFrames ? rhs.fallThroughSustainFrames : fallThroughSustainFrames);
  merged.maximumPlatformCorrection = (rhs.maximumPlatformCorrection ? rhs.maximumPlatformCorrection : maximumPlatformCorrection);
  merged.maximumPlatformCorrectionVelocityFactor = (rhs.maximumPlatformCorrectionVelocityFactor ? rhs.maximumPlatformCorrectionVelocityFactor : maximumPlatformCorrectionVelocityFactor);

  merged.physicsEffectCategories = (rhs.physicsEffectCategories ? rhs.physicsEffectCategories : physicsEffectCategories);

  merged.groundMovementMinimumSustain = (rhs.groundMovementMinimumSustain ? rhs.groundMovementMinimumSustain : groundMovementMinimumSustain);
  merged.groundMovementMaximumSustain = (rhs.groundMovementMaximumSustain ? rhs.groundMovementMaximumSustain : groundMovementMaximumSustain);
  merged.groundMovementCheckDistance = (rhs.groundMovementCheckDistance ? rhs.groundMovementCheckDistance : groundMovementCheckDistance);

  merged.collisionEnabled = (rhs.collisionEnabled ? rhs.collisionEnabled : collisionEnabled);
  merged.frictionEnabled = (rhs.frictionEnabled ? rhs.frictionEnabled : frictionEnabled);
  merged.gravityEnabled = (rhs.gravityEnabled ? rhs.gravityEnabled : gravityEnabled);

  merged.pathExploreRate = (rhs.pathExploreRate ? rhs.pathExploreRate : pathExploreRate);

  return merged;
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, ActorMovementParameters& movementParameters) {
  if (auto result = ds.read(movementParameters.mass); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.read(movementParameters.gravityMultiplier); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.read(movementParameters.liquidBuoyancy); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.read(movementParameters.airBuoyancy); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.read(movementParameters.bounceFactor); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.read(movementParameters.stopOnFirstBounce); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.read(movementParameters.enableSurfaceSlopeCorrection); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.read(movementParameters.slopeSlidingFactor); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.read(movementParameters.maxMovementPerStep); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.read(movementParameters.maximumCorrection); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.read(movementParameters.speedLimit); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.read(movementParameters.standingPoly); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.read(movementParameters.crouchingPoly); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.read(movementParameters.stickyCollision); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.read(movementParameters.stickyForce); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.read(movementParameters.walkSpeed); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.read(movementParameters.runSpeed); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.read(movementParameters.flySpeed); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.read(movementParameters.airFriction); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.read(movementParameters.liquidFriction); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.read(movementParameters.minimumLiquidPercentage); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.read(movementParameters.liquidImpedance); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.read(movementParameters.normalGroundFriction); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.read(movementParameters.ambulatingGroundFriction); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.read(movementParameters.groundForce); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.read(movementParameters.airForce); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.read(movementParameters.liquidForce); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.read(movementParameters.airJumpProfile); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.read(movementParameters.liquidJumpProfile); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.read(movementParameters.fallStatusSpeedMin); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.read(movementParameters.fallThroughSustainFrames); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.read(movementParameters.maximumPlatformCorrection); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.read(movementParameters.maximumPlatformCorrectionVelocityFactor); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.read(movementParameters.physicsEffectCategories); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.read(movementParameters.collisionEnabled); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.read(movementParameters.frictionEnabled); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.read(movementParameters.gravityEnabled); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.read(movementParameters.pathExploreRate); !result) return std::unexpected(std::move(result.error()));

  return std::ref(ds);
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, ActorMovementParameters const& movementParameters) {
  if (auto result = ds.write(movementParameters.mass); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.write(movementParameters.gravityMultiplier); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.write(movementParameters.liquidBuoyancy); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.write(movementParameters.airBuoyancy); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.write(movementParameters.bounceFactor); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.write(movementParameters.stopOnFirstBounce); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.write(movementParameters.enableSurfaceSlopeCorrection); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.write(movementParameters.slopeSlidingFactor); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.write(movementParameters.maxMovementPerStep); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.write(movementParameters.maximumCorrection); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.write(movementParameters.speedLimit); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.write(movementParameters.standingPoly); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.write(movementParameters.crouchingPoly); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.write(movementParameters.stickyCollision); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.write(movementParameters.stickyForce); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.write(movementParameters.walkSpeed); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.write(movementParameters.runSpeed); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.write(movementParameters.flySpeed); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.write(movementParameters.airFriction); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.write(movementParameters.liquidFriction); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.write(movementParameters.minimumLiquidPercentage); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.write(movementParameters.liquidImpedance); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.write(movementParameters.normalGroundFriction); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.write(movementParameters.ambulatingGroundFriction); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.write(movementParameters.groundForce); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.write(movementParameters.airForce); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.write(movementParameters.liquidForce); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.write(movementParameters.airJumpProfile); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.write(movementParameters.liquidJumpProfile); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.write(movementParameters.fallStatusSpeedMin); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.write(movementParameters.fallThroughSustainFrames); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.write(movementParameters.maximumPlatformCorrection); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.write(movementParameters.maximumPlatformCorrectionVelocityFactor); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.write(movementParameters.physicsEffectCategories); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.write(movementParameters.collisionEnabled); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.write(movementParameters.frictionEnabled); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.write(movementParameters.gravityEnabled); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.write(movementParameters.pathExploreRate); !result) return std::unexpected(std::move(result.error()));

  return std::ref(ds);
}

ActorMovementModifiers::ActorMovementModifiers() {
  groundMovementModifier = 1.0f;
  liquidMovementModifier = 1.0f;
  speedModifier = 1.0f;
  airJumpModifier = 1.0f;
  liquidJumpModifier = 1.0f;
  runningSuppressed = false;
  jumpingSuppressed = false;
  facingSuppressed = false;
  movementSuppressed = false;

 }

std::expected<ActorMovementModifiers, StarException> ActorMovementModifiers::create(Json const& config) {
  ActorMovementModifiers value;
  if (!config.is_null()) {
    auto groundMovementModifierResult = config.get_float("groundMovementModifier", 1.0f);
    if (!groundMovementModifierResult) return std::unexpected(std::move(groundMovementModifierResult.error()));
    value.groundMovementModifier = *groundMovementModifierResult;
    auto liquidMovementModifierResult = config.get_float("liquidMovementModifier", 1.0f);
    if (!liquidMovementModifierResult) return std::unexpected(std::move(liquidMovementModifierResult.error()));
    value.liquidMovementModifier = *liquidMovementModifierResult;
    auto speedModifierResult = config.get_float("speedModifier", 1.0f);
    if (!speedModifierResult) return std::unexpected(std::move(speedModifierResult.error()));
    value.speedModifier = *speedModifierResult;
    auto airJumpModifierResult = config.get_float("airJumpModifier", 1.0f);
    if (!airJumpModifierResult) return std::unexpected(std::move(airJumpModifierResult.error()));
    value.airJumpModifier = *airJumpModifierResult;
    auto liquidJumpModifierResult = config.get_float("liquidJumpModifier", 1.0f);
    if (!liquidJumpModifierResult) return std::unexpected(std::move(liquidJumpModifierResult.error()));
    value.liquidJumpModifier = *liquidJumpModifierResult;
    auto runningSuppressedResult = config.get_bool("runningSuppressed", false);
    if (!runningSuppressedResult) return std::unexpected(std::move(runningSuppressedResult.error()));
    value.runningSuppressed = *runningSuppressedResult;
    auto jumpingSuppressedResult = config.get_bool("jumpingSuppressed", false);
    if (!jumpingSuppressedResult) return std::unexpected(std::move(jumpingSuppressedResult.error()));
    value.jumpingSuppressed = *jumpingSuppressedResult;
    auto facingSuppressedResult = config.get_bool("facingSuppressed", false);
    if (!facingSuppressedResult) return std::unexpected(std::move(facingSuppressedResult.error()));
    value.facingSuppressed = *facingSuppressedResult;
    auto movementSuppressedResult = config.get_bool("movementSuppressed", false);
    if (!movementSuppressedResult) return std::unexpected(std::move(movementSuppressedResult.error()));
    value.movementSuppressed = *movementSuppressedResult;
  }
  return value;
}

Json ActorMovementModifiers::toJson() const {
  return JsonObject{
    {"groundMovementModifier", groundMovementModifier},
    {"liquidMovementModifier", liquidMovementModifier},
    {"speedModifier", speedModifier},
    {"airJumpModifier", airJumpModifier},
    {"liquidJumpModifier", liquidJumpModifier},
    {"runningSuppressed", runningSuppressed},
    {"jumpingSuppressed", jumpingSuppressed},
    {"facingSuppressed", facingSuppressed},
    {"movementSuppressed", movementSuppressed}
  };
}

ActorMovementModifiers ActorMovementModifiers::combine(ActorMovementModifiers const& rhs) const {
  ActorMovementModifiers res = *this;

  res.groundMovementModifier *= rhs.groundMovementModifier;
  res.liquidMovementModifier *= rhs.liquidMovementModifier;
  res.speedModifier *= rhs.speedModifier;
  res.airJumpModifier *= rhs.airJumpModifier;
  res.liquidJumpModifier *= rhs.liquidJumpModifier;
  res.runningSuppressed = res.runningSuppressed || rhs.runningSuppressed;
  res.jumpingSuppressed = res.jumpingSuppressed || rhs.jumpingSuppressed;
  res.facingSuppressed = res.facingSuppressed || rhs.facingSuppressed;
  res.movementSuppressed = res.movementSuppressed || rhs.movementSuppressed;

  return res;
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, ActorMovementModifiers& movementModifiers) {
  if (auto result = ds.read(movementModifiers.groundMovementModifier); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.read(movementModifiers.liquidMovementModifier); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.read(movementModifiers.speedModifier); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.read(movementModifiers.airJumpModifier); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.read(movementModifiers.liquidJumpModifier); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.read(movementModifiers.runningSuppressed); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.read(movementModifiers.jumpingSuppressed); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.read(movementModifiers.facingSuppressed); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.read(movementModifiers.movementSuppressed); !result) return std::unexpected(std::move(result.error()));

  return std::ref(ds);
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, ActorMovementModifiers const& movementModifiers) {
  if (auto result = ds.write(movementModifiers.groundMovementModifier); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.write(movementModifiers.liquidMovementModifier); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.write(movementModifiers.speedModifier); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.write(movementModifiers.airJumpModifier); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.write(movementModifiers.liquidJumpModifier); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.write(movementModifiers.runningSuppressed); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.write(movementModifiers.jumpingSuppressed); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.write(movementModifiers.facingSuppressed); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.write(movementModifiers.movementSuppressed); !result) return std::unexpected(std::move(result.error()));

  return std::ref(ds);
}

ActorMovementController::ActorMovementController(Root& root) : MovementController(root) {
  m_controlRotationRate = 0.0f;
  m_controlRun = false;
  m_controlCrouch = false;
  m_controlDown = false;
  m_controlJump = false;
  m_controlJumpAnyway = false;
  m_controlPathMove = {};

  m_fallThroughSustain = 0;
  m_lastControlJump = false;
  m_lastControlDown = false;
  m_targetHorizontalAmbulatingVelocity = 0.0f;
  m_moveSpeedMultiplier = 1.0f;

}

std::expected<std::shared_ptr<ActorMovementController>, StarException> ActorMovementController::create(Root& root, ActorMovementParameters const& parameters) {
  auto controller = std::shared_ptr<ActorMovementController>(new ActorMovementController(root));
  auto initialized = controller->initialize({});
  if (!initialized) return std::unexpected(std::move(initialized.error()));
  if (auto result = controller->addNetElement(controller->m_walking); !result) return std::unexpected(std::move(result.error()));
  if (auto result = controller->addNetElement(controller->m_running); !result) return std::unexpected(std::move(result.error()));
  if (auto result = controller->addNetElement(controller->m_movingDirection); !result) return std::unexpected(std::move(result.error()));
  if (auto result = controller->addNetElement(controller->m_facingDirection); !result) return std::unexpected(std::move(result.error()));
  if (auto result = controller->addNetElement(controller->m_crouching); !result) return std::unexpected(std::move(result.error()));
  if (auto result = controller->addNetElement(controller->m_flying); !result) return std::unexpected(std::move(result.error()));
  if (auto result = controller->addNetElement(controller->m_falling); !result) return std::unexpected(std::move(result.error()));
  if (auto result = controller->addNetElement(controller->m_canJump); !result) return std::unexpected(std::move(result.error()));
  if (auto result = controller->addNetElement(controller->m_jumping); !result) return std::unexpected(std::move(result.error()));
  if (auto result = controller->addNetElement(controller->m_groundMovement); !result) return std::unexpected(std::move(result.error()));
  if (auto result = controller->addNetElement(controller->m_liquidMovement); !result) return std::unexpected(std::move(result.error()));
  if (auto result = controller->addNetElement(controller->m_anchorState); !result) return std::unexpected(std::move(result.error()));

  auto configured = controller->resetBaseParameters(parameters);
  if (!configured) return std::unexpected(std::move(configured.error()));
  return controller;
}

ActorMovementParameters const& ActorMovementController::baseParameters() const {
  return m_baseParameters;
}

std::expected<void, StarException> ActorMovementController::updateBaseParameters(ActorMovementParameters const& parameters) {
  auto merged = m_baseParameters.merge(parameters);
  auto applied = applyMCParameters(merged);
  if (!applied) return std::unexpected(std::move(applied.error()));
  m_baseParameters = std::move(merged);
  return {};
}

std::expected<void, StarException> ActorMovementController::resetBaseParameters(ActorMovementParameters const& parameters) {
  auto defaults = ActorMovementParameters::sensibleDefaults(root());
  if (!defaults) return std::unexpected(std::move(defaults.error()));
  auto merged = defaults->merge(parameters);
  auto applied = applyMCParameters(merged);
  if (!applied) return std::unexpected(std::move(applied.error()));
  m_baseParameters = std::move(merged);
  return {};
}

ActorMovementModifiers const& ActorMovementController::baseModifiers() const {
  return m_baseModifiers;
}

void ActorMovementController::updateBaseModifiers(ActorMovementModifiers const& modifiers) {
  m_baseModifiers = m_baseModifiers.combine(modifiers);
}

void ActorMovementController::resetBaseModifiers(ActorMovementModifiers const& modifiers) {
  m_baseModifiers = modifiers;
}

void ActorMovementController::uninit() {
  m_pathController.reset();
  MovementController::uninit();
}

std::expected<Json, StarException> ActorMovementController::storeState() const {
  auto movingName = enum_name<DirectionNames>(movingDirection());
  if (!movingName) return std::unexpected(std::move(movingName.error()));
  auto facingName = enum_name<DirectionNames>(m_facingDirection.get());
  if (!facingName) return std::unexpected(std::move(facingName.error()));
  return JsonObject{{"position", json_from_vec2f(MovementController::position())},
                    {"velocity", json_from_vec2f(velocity())},
                    {"rotation", MovementController::rotation()},
                    {"movingDirection", String(*movingName)},
                    {"facingDirection", String(*facingName)},
                    {"crouching", m_crouching.get()}};
}

std::expected<void, StarException> ActorMovementController::loadState(Json const& state) {
  auto loaded = MovementController::loadState(state);
  if (!loaded) return std::unexpected(std::move(loaded.error()));
  auto movingName = state.get_string("movingDirection");
  if (!movingName) return std::unexpected(std::move(movingName.error()));
  auto movingDirection = enum_value<DirectionNames>(*movingName);
  if (!movingDirection) return std::unexpected(std::move(movingDirection.error()));
  auto facingName = state.get_string("facingDirection");
  if (!facingName) return std::unexpected(std::move(facingName.error()));
  auto facingDirection = enum_value<DirectionNames>(*facingName);
  if (!facingDirection) return std::unexpected(std::move(facingDirection.error()));
  auto crouchingValue = state.get_bool("crouching");
  if (!crouchingValue) return std::unexpected(std::move(crouchingValue.error()));
  setMovingDirection(*movingDirection);
  m_facingDirection.set(*facingDirection);
  m_crouching.set(*crouchingValue);
  return {};
}

std::expected<void, StarException> ActorMovementController::setAnchorState(EntityAnchorState anchorState) {
  return doSetAnchorState(anchorState);
}

std::expected<void, StarException> ActorMovementController::resetAnchorState() {
  return doSetAnchorState({});
}

std::optional<EntityAnchorState> ActorMovementController::anchorState() const {
  return m_anchorState.get();
}

std::shared_ptr<EntityAnchor const> ActorMovementController::entityAnchor() const {
  return m_entityAnchor;
}

Vec2F ActorMovementController::position() const {
  if (m_entityAnchor)
    return m_entityAnchor->position;
  return MovementController::position();
}

float ActorMovementController::rotation() const {
  if (m_entityAnchor)
    return m_entityAnchor->angle;
  return MovementController::rotation();
}

bool ActorMovementController::walking() const {
  return m_walking.get();
}

bool ActorMovementController::running() const {
  return m_running.get();
}

Direction ActorMovementController::movingDirection() const {
  return m_nativeMovingDirection.value_or(m_movingDirection.get());
}

void ActorMovementController::setMovingDirection(Direction direction) {
  if (m_nativeMovingDirection)
    m_nativeMovingDirection = direction;
  else
    m_movingDirection.set(direction);
}

void ActorMovementController::setNetworkMovingDirection(std::optional<Direction> direction) {
  if (direction) {
    if (!m_nativeMovingDirection)
      m_nativeMovingDirection = m_movingDirection.get();
    m_movingDirection.set(*direction);
  } else if (m_nativeMovingDirection) {
    m_movingDirection.set(*m_nativeMovingDirection);
    m_nativeMovingDirection.reset();
  }
}

Direction ActorMovementController::facingDirection() const {
  if (m_entityAnchor)
    return m_entityAnchor->direction;
  return m_facingDirection.get();
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

bool ActorMovementController::canJump() const {
  return m_canJump.get();
}

bool ActorMovementController::jumping() const {
  return m_jumping.get();
}

bool ActorMovementController::groundMovement() const {
  return m_groundMovement.get();
}

bool ActorMovementController::liquidMovement() const {
  return m_liquidMovement.get();
}

bool ActorMovementController::pathfinding() const {
  if (m_pathController)
    return m_pathController->pathfinding();
  else
    return false;
}

void ActorMovementController::controlRotation(float rotationRate) {
  m_controlRotationRate += rotationRate;
}

void ActorMovementController::controlAcceleration(Vec2F const& acceleration) {
  m_controlAcceleration += acceleration;
}

void ActorMovementController::controlForce(Vec2F const& force) {
  m_controlForce += force;
}

void ActorMovementController::controlApproachVelocity(Vec2F const& targetVelocity, float maxControlForce) {
  m_controlApproachVelocities.push_back({targetVelocity, maxControlForce});
}

void ActorMovementController::controlApproachVelocityAlongAngle(
    float angle, float targetVelocity, float maxControlForce, bool positiveOnly) {
  m_controlApproachVelocityAlongAngles.push_back({angle, targetVelocity, maxControlForce, positiveOnly});
}

void ActorMovementController::controlApproachXVelocity(float targetXVelocity, float maxControlForce) {
  controlApproachVelocityAlongAngle(0, targetXVelocity, maxControlForce, false);
}

void ActorMovementController::controlApproachYVelocity(float targetYVelocity, float maxControlForce) {
  controlApproachVelocityAlongAngle(std::numbers::pi / 2, targetYVelocity, maxControlForce, false);
}

void ActorMovementController::controlParameters(ActorMovementParameters const& parameters) {
  m_controlParameters = m_controlParameters.merge(parameters);
}

void ActorMovementController::controlModifiers(ActorMovementModifiers const& modifiers) {
  m_controlModifiers = m_controlModifiers.combine(modifiers);
}

void ActorMovementController::controlMove(Direction direction, bool run) {
  m_controlMove = direction;
  m_controlRun = run;
}

void ActorMovementController::controlFace(Direction direction) {
  m_controlFace = direction;
}

void ActorMovementController::controlDown() {
  m_controlDown = true;
}

void ActorMovementController::controlCrouch() {
  m_controlCrouch = true;
}

void ActorMovementController::controlJump(bool jumpEvenIfUnable) {
  m_controlJump = true;
  m_controlJumpAnyway |= jumpEvenIfUnable;
}

void ActorMovementController::controlFly(Vec2F const& velocity) {
  m_controlFly = velocity;
}

std::expected<std::optional<std::pair<Vec2F, bool>>, StarException> ActorMovementController::pathMove(Vec2F const& position, bool, std::optional<PlatformerAStar::Parameters> const& parameters) {
  if (!m_pathController) {
    auto activeWorld = world();
    if (!activeWorld) return std::unexpected(std::move(activeWorld.error()));
    m_pathController = std::make_shared<PathController>(activeWorld->get());
  }

  // set new parameters if they have changed
  if (!m_pathController->target_position().has_value()
      || (parameters && m_pathController->parameters() != *parameters)) {
    if (parameters)
      m_pathController->set_parameters(*parameters);
    auto pathResult = m_pathController->find_path(*this, position);
    if (!pathResult) return std::unexpected(std::move(pathResult.error()));
    m_pathMoveResult = pathResult->transform([position](bool result) { return std::pair<Vec2F, bool>(position, result); });
  }

  // update target position if it has changed
  if (m_pathController) {
    if (auto result = m_pathController->find_path(*this, position); !result)
      return std::unexpected(std::move(result.error()));
  }

  if (m_pathMoveResult.has_value()) {
    // path controller failed or succeeded, return the result and reset the controller
    m_pathController->reset();
  }

  return std::exchange(m_pathMoveResult, {});
}

std::expected<std::optional<std::pair<Vec2F, bool>>, StarException> ActorMovementController::controlPathMove(Vec2F const& position, bool run, std::optional<PlatformerAStar::Parameters> const& parameters) {
  auto result = pathMove(position, run, parameters);
  if (!result) return std::unexpected(std::move(result.error()));

  if (!result->has_value())
    m_controlPathMove = std::pair<Vec2F, bool>(position, run);

  return result;
}

void ActorMovementController::setMoveSpeedMultiplier(float scale) {
  m_moveSpeedMultiplier = scale;
}

void ActorMovementController::clearControls() {
  m_controlRotationRate = 0.0f;
  m_controlAcceleration = Vec2F();
  m_controlForce = Vec2F();
  m_controlApproachVelocities.clear();
  m_controlApproachVelocityAlongAngles.clear();
  m_controlMove = {};
  m_controlFace = {};
  m_controlRun = false;
  m_controlCrouch = false;
  m_controlDown = false;
  m_controlJump = false;
  m_controlJumpAnyway = false;
  m_controlFly = {};
  m_controlPathMove = {};
  m_controlParameters = ActorMovementParameters();
  m_controlModifiers = ActorMovementModifiers();
}

std::expected<void, StarException> ActorMovementController::tickMaster(float dt) {
  auto activeWorld = world();
  if (!activeWorld) return std::unexpected(std::move(activeWorld.error()));
  World& worldRef = activeWorld->get();
  std::shared_ptr<EntityAnchor const> newAnchor;
  if (auto anchorState = m_anchorState.get()) {
    if (auto anchorableEntity = as<AnchorableEntity>(worldRef.entity(anchorState->entityId)))
      {
        auto anchorValue = anchorableEntity->anchor(anchorState->positionIndex);
        if (!anchorValue) return std::unexpected(std::move(anchorValue.error()));
        newAnchor = std::move(*anchorValue);
      }
  }

  if (newAnchor)
    m_entityAnchor = newAnchor;
  else
    if (auto result = resetAnchorState(); !result) return std::unexpected(std::move(result.error()));

  if (m_entityAnchor) {
    m_walking.set(false);
    m_running.set(false);
    m_crouching.set(false);
    m_flying.set(false);
    m_falling.set(false);
    m_canJump.set(false);
    m_jumping.set(false);
    m_groundMovement.set(false);
    m_liquidMovement.set(false);

    setVelocity((m_entityAnchor->position - MovementController::position()) / dt);
    if (auto result = MovementController::tickMaster(dt); !result) return std::unexpected(std::move(result.error()));
    setPosition(m_entityAnchor->position);
  } else {
    auto activeParameters = m_baseParameters.merge(m_controlParameters);
    auto activeModifiers = m_baseModifiers.combine(m_controlModifiers);

    if (activeModifiers.movementSuppressed) {
      m_controlMove = {};
      m_controlRun = false;
      m_controlCrouch = false;
      m_controlDown = false;
      m_controlJump = false;
      m_controlFly = {};
      m_controlPathMove = {};
    }

    if (m_controlMove.has_value()
        || m_controlCrouch
        || m_controlDown
        || m_controlJump
        || m_controlFly.has_value()
        || !m_controlApproachVelocities.empty()
        || !m_controlApproachVelocityAlongAngles.empty()) {
      // controlling any other movement overrides the pathing
      m_controlPathMove.reset();
    }

    if (m_controlPathMove && !m_pathMoveResult.has_value()) {
      if (appliedForceRegion()) {
        m_pathController->reset();
      } else if (!m_pathController->pathfinding()) {
        auto pathResult = m_pathController->move(*this, activeParameters, activeModifiers, m_controlPathMove->second, dt);
        if (!pathResult) return std::unexpected(std::move(pathResult.error()));
        m_pathMoveResult = pathResult->transform([&pathMove = m_controlPathMove](bool result) { return std::pair<Vec2F, bool>(pathMove->first, result); });

        auto action = m_pathController->current_action();
        bool onGround = false;
        if (auto a = action) {
          using namespace PlatformerAStar;
          m_walking.set(a == Action::Walk && !m_controlPathMove->second);
          m_running.set(a == Action::Walk && m_controlPathMove->second);
          m_flying.set(a == Action::Fly || a == Action::Swim);
          m_falling.set((a == Action::Arc && yVelocity() < 0.0f) || a == Action::Drop);
          m_jumping.set(a == Action::Arc && yVelocity() >= 0.0f);

          onGround = a == Action::Walk || a == Action::Drop || a == Action::Jump;

          if (a == Action::Land || a== Action::Jump) {
            auto inLiquid = liquidPercentage() >= activeParameters.minimumLiquidPercentage.value_or(1.0);
            m_liquidMovement.set(inLiquid);
            m_groundMovement.set(!inLiquid);
            onGround = !inLiquid && onGround;
          } else {
            m_liquidMovement.set(a == Action::Swim);
            m_groundMovement.set(a != Action::Arc && a != Action::Swim);
          }
        } else {
          m_walking.set(false);
          m_running.set(false);
        }
        auto pathFacing = m_pathController->facing();
        auto facing = (m_controlFace ? m_controlFace : pathFacing).value_or(m_facingDirection.get());
        m_facingDirection.set(facing);
        setMovingDirection(m_pathController->facing().value_or(m_facingDirection.get()));

        if (auto result = applyMCParameters(activeParameters); !result) return std::unexpected(std::move(result.error()));

        // MovementController still handles updating liquid percentage and updating force regions
        if (auto result = updateLiquidPercentage(); !result) return std::unexpected(std::move(result.error()));
        if (auto result = updateForceRegions(dt); !result) return std::unexpected(std::move(result.error()));
        // onGround flag needs to be manually set, won't be set by MovementController::tickMaster
        setOnGround(onGround);
        clearControls();
        return {};
      } else {
        auto pathResult = m_pathController->find_path(*this, m_controlPathMove->first);
        if (!pathResult) return std::unexpected(std::move(pathResult.error()));
        m_pathMoveResult = pathResult->transform([&pathMove = m_controlPathMove](bool result) {
            return std::pair<Vec2F, bool>(pathMove->first, result);
          });
      }
    } else {
      m_pathController = {};
    }

    // Do some basic movement consistency checks.
    if (m_controlFly)
      m_controlMove = {};

    if ((m_controlDown && !m_lastControlDown) || m_controlFly)
      m_fallThroughSustain = (*activeParameters.fallThroughSustainFrames);
    else if (m_fallThroughSustain > 0)
      --m_fallThroughSustain;

    if (auto result = applyMCParameters(activeParameters); !result) return std::unexpected(std::move(result.error()));

    m_targetHorizontalAmbulatingVelocity = 0.0f;

    rotate(m_controlRotationRate);
    accelerate(m_controlAcceleration);
    force(m_controlForce);

    for (auto const& approach : m_controlApproachVelocities)
      approachVelocity(approach.targetVelocity * activeModifiers.speedModifier, approach.maxControlForce);

    for (auto const& approach : m_controlApproachVelocityAlongAngles)
      approachVelocityAlongAngle(approach.alongAngle, approach.targetVelocity * activeModifiers.speedModifier, approach.maxControlForce, approach.positiveOnly);

    m_liquidMovement.set(liquidPercentage() >= (*activeParameters.minimumLiquidPercentage));
    float liquidImpedance = (*activeParameters.liquidImpedance) * liquidPercentage();

    std::optional<Direction> updatedMovingDirection;
    bool running = m_controlRun && !activeModifiers.runningSuppressed;

    if (m_controlFly) {
      Vec2F flyVelocity = *m_controlFly;
      if (flyVelocity.magnitude_squared() != 0)
        flyVelocity = flyVelocity.normalized() * (*activeParameters.flySpeed);

      if (m_liquidMovement.get())
        approachVelocity(flyVelocity * (1.0f - liquidImpedance) * activeModifiers.speedModifier, (*activeParameters.liquidForce) * activeModifiers.liquidMovementModifier);
      else
        approachVelocity(flyVelocity * activeModifiers.speedModifier, (*activeParameters.airForce));

      if (flyVelocity[0] > 0)
        updatedMovingDirection = Direction::Right;
      else if (flyVelocity[0] < 0)
        updatedMovingDirection = Direction::Left;

      m_groundMovementSustainTimer = GameTimer(0);

    } else {
      float jumpModifier;
      ActorJumpProfile jumpProfile;
      if (m_liquidMovement.get()) {
        jumpModifier = activeModifiers.liquidJumpModifier;
        jumpProfile = activeParameters.liquidJumpProfile;
        (*jumpProfile.jumpSpeed) *= (1.0f - liquidImpedance);
      } else {
        jumpModifier = activeModifiers.airJumpModifier;
        jumpProfile = activeParameters.airJumpProfile;
      }

      bool startJump = false;
      bool holdJump = false;

      // If we are on the ground, then reset the ground movement sustain timer
      // to the maximum.  If we are not on the ground or near the ground
      // according to the nearGroundCheckDistance, and we are past the minimum
      // sustain time, then go ahead and immediately clear the ground movement
      // sustain timer.
      float minGroundSustain = (*activeParameters.groundMovementMinimumSustain);
      float maxGroundSustain = (*activeParameters.groundMovementMaximumSustain);
      float groundCheckDistance = (*activeParameters.groundMovementCheckDistance);
      m_groundMovementSustainTimer.tick(dt);
      if (onGround()) {
        m_groundMovementSustainTimer = GameTimer(maxGroundSustain);
      } else if (!m_groundMovementSustainTimer.ready() && groundCheckDistance > 0.0f && maxGroundSustain - m_groundMovementSustainTimer.timer > minGroundSustain) {
        PolyF collisionBody = MovementController::collisionBody();
        collisionBody.translate(Vec2F(0, -groundCheckDistance));
        auto collision = worldRef.polyCollision(collisionBody, {CollisionKind::Block, CollisionKind::Dynamic, CollisionKind::Platform, CollisionKind::Slippery});
        if (!collision) return std::unexpected(std::move(collision.error()));
        if (!*collision)
          m_groundMovementSustainTimer = GameTimer(0);
      }

      bool standingJumpable = !m_groundMovementSustainTimer.ready();
      bool controlJump = m_controlJump && (!activeModifiers.jumpingSuppressed || m_controlJumpAnyway);

      // We are doing a jump if m_reJumpTimer has run out and there has been a
      // new m_controlJump command which was just recently triggered.  If
      // jumpProfile.autoJump is set, then we don't care whether it is a new
      // m_controlJump command, m_controlJump can be held.
      if (m_reJumpTimer.ready() && controlJump && ((*jumpProfile.autoJump) || !m_lastControlJump)) {
        if (standingJumpable || (*jumpProfile.multiJump) || m_controlJumpAnyway)
          startJump = true;
      } else if (m_jumping.get() && controlJump && (!m_jumpHoldTimer || !m_jumpHoldTimer->ready())) {
        if (!(*jumpProfile.collisionCancelled) || collisionCorrection()[1] >= 0.0f)
          holdJump = true;
      }

      if (startJump) {
        m_jumping.set(true);

        m_reJumpTimer = GameTimer((*jumpProfile.reJumpDelay));
        if ((*jumpProfile.jumpHoldTime) >= 0.0f)
          m_jumpHoldTimer = GameTimer((*jumpProfile.jumpHoldTime));
        else
          m_jumpHoldTimer = {};

        setYVelocity(yVelocity() + (*jumpProfile.jumpSpeed) * (*jumpProfile.jumpInitialPercentage) * jumpModifier);

        m_groundMovementSustainTimer = GameTimer(0);

      } else if (holdJump) {
        m_reJumpTimer.tick(dt);
        if (m_jumpHoldTimer)
          m_jumpHoldTimer->tick(dt);

        approachYVelocity((*jumpProfile.jumpSpeed) * jumpModifier, (*jumpProfile.jumpControlForce) * jumpModifier);

      } else {
        m_jumping.set(false);
        m_reJumpTimer.tick(dt);
      }

      if (m_controlMove == Direction::Left) {
        updatedMovingDirection = Direction::Left;
        m_targetHorizontalAmbulatingVelocity =
            -1.0f * (running ? (*activeParameters.runSpeed) * activeModifiers.speedModifier
                             : (*activeParameters.walkSpeed) * activeModifiers.speedModifier);
      } else if (m_controlMove == Direction::Right) {
        updatedMovingDirection = Direction::Right;
        m_targetHorizontalAmbulatingVelocity =
            1.0f * (running ? (*activeParameters.runSpeed) * activeModifiers.speedModifier
                            : (*activeParameters.walkSpeed) * activeModifiers.speedModifier);
      }

      m_targetHorizontalAmbulatingVelocity *= m_moveSpeedMultiplier;

      if (m_liquidMovement.get())
        m_targetHorizontalAmbulatingVelocity *= (1.0f - liquidImpedance);

      Vec2F surfaceVelocity = MovementController::surfaceVelocity();

      // don't ambulate if we're already moving faster than the target velocity in the direction of ambulation
      bool ambulationWouldAccelerate = std::abs(m_targetHorizontalAmbulatingVelocity + surfaceVelocity[0]) > std::abs(xVelocity())
          || (m_targetHorizontalAmbulatingVelocity < 0) != (xVelocity() < 0);

      if (m_targetHorizontalAmbulatingVelocity != 0.0f && ambulationWouldAccelerate) {
        float ambulatingAccel;
        if (onGround())
          ambulatingAccel = (*activeParameters.groundForce) * activeModifiers.groundMovementModifier;
        else if (m_liquidMovement.get())
          ambulatingAccel = (*activeParameters.liquidForce) * activeModifiers.liquidMovementModifier;
        else
          ambulatingAccel = (*activeParameters.airForce);

        approachXVelocity(m_targetHorizontalAmbulatingVelocity + surfaceVelocity[0], ambulatingAccel);
      }
    }

    if (updatedMovingDirection)
      setMovingDirection(*updatedMovingDirection);

    if (!activeModifiers.facingSuppressed) {
      if (m_controlFace)
        m_facingDirection.set(*m_controlFace);
      else if (updatedMovingDirection)
        m_facingDirection.set(*updatedMovingDirection);
      else if (m_controlPathMove && m_pathController && m_pathController->facing())
        m_facingDirection.set(*m_pathController->facing());
    }

    m_groundMovement.set(!m_groundMovementSustainTimer.ready());
    if (m_groundMovement.get()) {
      m_running.set(running && m_controlMove);
      m_walking.set(!running && m_controlMove);
      m_crouching.set(m_controlCrouch && !m_controlMove);
    }
    m_flying.set(static_cast<bool>(m_controlFly));

    bool falling = (yVelocity() < (*activeParameters.fallStatusSpeedMin)) && !m_groundMovement.get();
    m_falling.set(falling);

    if (auto result = MovementController::tickMaster(dt); !result) return std::unexpected(std::move(result.error()));

    m_lastControlJump = m_controlJump;
    m_lastControlDown = m_controlDown;

    if (m_liquidMovement.get())
      m_canJump.set(m_reJumpTimer.ready() && (!m_groundMovementSustainTimer.ready() || (*activeParameters.liquidJumpProfile.multiJump)));
    else
      m_canJump.set(m_reJumpTimer.ready() && (!m_groundMovementSustainTimer.ready() || (*activeParameters.airJumpProfile.multiJump)));
  }

  clearControls();
  return {};
}

std::expected<void, StarException> ActorMovementController::tickSlave(float dt) {
  auto activeWorld = world();
  if (!activeWorld) return std::unexpected(std::move(activeWorld.error()));
  World& worldRef = activeWorld->get();
  if (auto result = MovementController::tickSlave(dt); !result) return std::unexpected(std::move(result.error()));

  m_entityAnchor.reset();
  if (auto anchorState = m_anchorState.get()) {
    if (auto anchorableEntity = as<AnchorableEntity>(worldRef.entity(anchorState->entityId)))
      {
        auto anchorValue = anchorableEntity->anchor(anchorState->positionIndex);
        if (!anchorValue) return std::unexpected(std::move(anchorValue.error()));
        m_entityAnchor = std::move(*anchorValue);
      }
  }
  return {};
}

std::expected<void, StarException> ActorMovementController::applyMCParameters(ActorMovementParameters const& parameters) {
  if (!parameters.fallThroughSustainFrames) return std::unexpected(ActorMovementControllerException("Missing actor movement parameter 'fallThroughSustainFrames'"));
  if (!parameters.minimumLiquidPercentage) return std::unexpected(ActorMovementControllerException("Missing actor movement parameter 'minimumLiquidPercentage'"));
  if (!parameters.liquidImpedance) return std::unexpected(ActorMovementControllerException("Missing actor movement parameter 'liquidImpedance'"));
  if (!parameters.flySpeed) return std::unexpected(ActorMovementControllerException("Missing actor movement parameter 'flySpeed'"));
  if (!parameters.liquidForce) return std::unexpected(ActorMovementControllerException("Missing actor movement parameter 'liquidForce'"));
  if (!parameters.airForce) return std::unexpected(ActorMovementControllerException("Missing actor movement parameter 'airForce'"));
  if (!parameters.groundMovementMinimumSustain) return std::unexpected(ActorMovementControllerException("Missing actor movement parameter 'groundMovementMinimumSustain'"));
  if (!parameters.groundMovementMaximumSustain) return std::unexpected(ActorMovementControllerException("Missing actor movement parameter 'groundMovementMaximumSustain'"));
  if (!parameters.groundMovementCheckDistance) return std::unexpected(ActorMovementControllerException("Missing actor movement parameter 'groundMovementCheckDistance'"));
  if (!parameters.runSpeed) return std::unexpected(ActorMovementControllerException("Missing actor movement parameter 'runSpeed'"));
  if (!parameters.walkSpeed) return std::unexpected(ActorMovementControllerException("Missing actor movement parameter 'walkSpeed'"));
  if (!parameters.groundForce) return std::unexpected(ActorMovementControllerException("Missing actor movement parameter 'groundForce'"));
  if (!parameters.fallStatusSpeedMin) return std::unexpected(ActorMovementControllerException("Missing actor movement parameter 'fallStatusSpeedMin'"));
  if (!parameters.gravityMultiplier) return std::unexpected(ActorMovementControllerException("Missing actor movement parameter 'gravityMultiplier'"));
  if (!parameters.crouchingPoly) return std::unexpected(ActorMovementControllerException("Missing actor movement parameter 'crouchingPoly'"));
  if (!parameters.standingPoly) return std::unexpected(ActorMovementControllerException("Missing actor movement parameter 'standingPoly'"));
  if (!parameters.airJumpProfile.jumpSpeed) return std::unexpected(ActorMovementControllerException("Missing actor movement parameter 'airJumpProfile.jumpSpeed'"));
  if (!parameters.airJumpProfile.autoJump) return std::unexpected(ActorMovementControllerException("Missing actor movement parameter 'airJumpProfile.autoJump'"));
  if (!parameters.airJumpProfile.multiJump) return std::unexpected(ActorMovementControllerException("Missing actor movement parameter 'airJumpProfile.multiJump'"));
  if (!parameters.airJumpProfile.collisionCancelled) return std::unexpected(ActorMovementControllerException("Missing actor movement parameter 'airJumpProfile.collisionCancelled'"));
  if (!parameters.airJumpProfile.reJumpDelay) return std::unexpected(ActorMovementControllerException("Missing actor movement parameter 'airJumpProfile.reJumpDelay'"));
  if (!parameters.airJumpProfile.jumpHoldTime) return std::unexpected(ActorMovementControllerException("Missing actor movement parameter 'airJumpProfile.jumpHoldTime'"));
  if (!parameters.airJumpProfile.jumpInitialPercentage) return std::unexpected(ActorMovementControllerException("Missing actor movement parameter 'airJumpProfile.jumpInitialPercentage'"));
  if (!parameters.airJumpProfile.jumpControlForce) return std::unexpected(ActorMovementControllerException("Missing actor movement parameter 'airJumpProfile.jumpControlForce'"));
  if (!parameters.liquidJumpProfile.jumpSpeed) return std::unexpected(ActorMovementControllerException("Missing actor movement parameter 'liquidJumpProfile.jumpSpeed'"));
  if (!parameters.liquidJumpProfile.autoJump) return std::unexpected(ActorMovementControllerException("Missing actor movement parameter 'liquidJumpProfile.autoJump'"));
  if (!parameters.liquidJumpProfile.multiJump) return std::unexpected(ActorMovementControllerException("Missing actor movement parameter 'liquidJumpProfile.multiJump'"));
  if (!parameters.liquidJumpProfile.collisionCancelled) return std::unexpected(ActorMovementControllerException("Missing actor movement parameter 'liquidJumpProfile.collisionCancelled'"));
  if (!parameters.liquidJumpProfile.reJumpDelay) return std::unexpected(ActorMovementControllerException("Missing actor movement parameter 'liquidJumpProfile.reJumpDelay'"));
  if (!parameters.liquidJumpProfile.jumpHoldTime) return std::unexpected(ActorMovementControllerException("Missing actor movement parameter 'liquidJumpProfile.jumpHoldTime'"));
  if (!parameters.liquidJumpProfile.jumpInitialPercentage) return std::unexpected(ActorMovementControllerException("Missing actor movement parameter 'liquidJumpProfile.jumpInitialPercentage'"));
  if (!parameters.liquidJumpProfile.jumpControlForce) return std::unexpected(ActorMovementControllerException("Missing actor movement parameter 'liquidJumpProfile.jumpControlForce'"));
  MovementParameters mcParameters;

  mcParameters.mass = parameters.mass;

  if (!onGround() && yVelocity() < 0)
    mcParameters.gravityMultiplier = (*parameters.gravityMultiplier);
  else
    mcParameters.gravityMultiplier = parameters.gravityMultiplier;

  mcParameters.liquidBuoyancy = parameters.liquidBuoyancy;
  mcParameters.airBuoyancy = parameters.airBuoyancy;
  mcParameters.bounceFactor = parameters.bounceFactor;
  mcParameters.stopOnFirstBounce = parameters.stopOnFirstBounce;
  mcParameters.enableSurfaceSlopeCorrection = parameters.enableSurfaceSlopeCorrection;
  mcParameters.slopeSlidingFactor = parameters.slopeSlidingFactor;
  mcParameters.maxMovementPerStep = parameters.maxMovementPerStep;

  if (m_crouching.get())
    mcParameters.collisionPoly = (*parameters.crouchingPoly);
  else
    mcParameters.collisionPoly = (*parameters.standingPoly);

  mcParameters.stickyCollision = parameters.stickyCollision;
  mcParameters.stickyForce = parameters.stickyForce;

  mcParameters.airFriction = parameters.airFriction;
  mcParameters.liquidFriction = parameters.liquidFriction;

  // If we are traveling in the correct direction while in a movement mode that
  // requires contact with the ground (ambulating i.e. walking or running), and
  // not traveling faster than our target horizontal movement, then apply the
  // special 'ambulatingGroundFriction'.
  float relativeXVelocity = xVelocity() - surfaceVelocity()[0];
  bool useAmbulatingGroundFriction = (m_walking.get() || m_running.get())
      && std::copysign(1, m_targetHorizontalAmbulatingVelocity) == std::copysign(1, relativeXVelocity)
      && std::fabs(relativeXVelocity) <= std::fabs(m_targetHorizontalAmbulatingVelocity);

  if (useAmbulatingGroundFriction)
    mcParameters.groundFriction = parameters.ambulatingGroundFriction;
  else
    mcParameters.groundFriction = parameters.normalGroundFriction;

  mcParameters.collisionEnabled = parameters.collisionEnabled;
  mcParameters.frictionEnabled = parameters.frictionEnabled;
  mcParameters.gravityEnabled = parameters.gravityEnabled;

  mcParameters.ignorePlatformCollision = m_fallThroughSustain > 0 || m_controlFly || m_controlDown;
  mcParameters.maximumPlatformCorrection = parameters.maximumPlatformCorrection;
  mcParameters.maximumPlatformCorrectionVelocityFactor = parameters.maximumPlatformCorrectionVelocityFactor;

  mcParameters.physicsEffectCategories = parameters.physicsEffectCategories;

  mcParameters.maximumCorrection = parameters.maximumCorrection;
  mcParameters.speedLimit = parameters.speedLimit;

  return MovementController::applyParameters(mcParameters);
}

std::expected<void, StarException> ActorMovementController::doSetAnchorState(std::optional<EntityAnchorState> anchorState) {
  std::shared_ptr<EntityAnchor const> entityAnchor;
  if (anchorState) {
    auto activeWorld = world();
    if (!activeWorld) return std::unexpected(std::move(activeWorld.error()));
    World& worldRef = activeWorld->get();
    auto anchorableEntity = as<AnchorableEntity>(worldRef.entity(anchorState->entityId));
    if (!anchorableEntity)
      return std::unexpected(ActorMovementControllerException::format("No such anchorable entity id {} in ActorMovementController::setAnchorState", anchorState->entityId));
    auto anchorValue = anchorableEntity->anchor(anchorState->positionIndex);
    if (!anchorValue) return std::unexpected(std::move(anchorValue.error()));
    entityAnchor = std::move(*anchorValue);
    if (!entityAnchor)
      return std::unexpected(ActorMovementControllerException::format("Anchor position {} is disabled ActorMovementController::setAnchorState", anchorState->positionIndex));
  }
  auto prevAnchor = m_entityAnchor;
  m_anchorState.set(anchorState);
  m_entityAnchor = std::move(entityAnchor);

  if (!entityAnchor && prevAnchor && prevAnchor->exitBottomPosition) {
    auto boundBox = MovementController::localBoundBox();
    Vec2F bottomMid = {boundBox.center()[0], boundBox.yMin()};
    setPosition(*prevAnchor->exitBottomPosition - bottomMid);
  }

  if (m_entityAnchor)
    setPosition(m_entityAnchor->position);
  return {};
}

PathController::PathController(World& world) : m_world(world), m_edge_index(0), m_edge_timer(0.0) {}

PlatformerAStar::Parameters const& PathController::parameters() const { return m_parameters; }

void PathController::set_parameters(PlatformerAStar::Parameters const& parameters) { m_parameters = parameters; }

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
    if (*movement_controller.parameters().gravityEnabled && !grounded && !movement_controller.liquidMovement()) {
      return {};
    }
    m_start_position = movement_controller.position();
    m_target_position = target_position;
    auto finder = PathFinder::create(m_world.get(), movement_controller.position(), *m_target_position,
                                     movement_controller.baseParameters(), m_parameters);
    if (!finder) return std::unexpected(std::move(finder.error()));
    m_path_finder = std::move(*finder);
  }

  if (!m_path_finder && m_path && m_edge_index == m_path->size())
    return true; // Reached goal

  if (m_path_finder) {
    auto exploration = m_path_finder->explore(movement_controller.baseParameters().pathExploreRate.value_or(100.0));
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
                                                  movement_controller.baseParameters(), m_parameters);
          if (!bridge_result)
            return std::unexpected(std::move(bridge_result.error()));
          auto bridge_path_finder = std::move(*bridge_result);
          auto bridge_exploration =
            bridge_path_finder->explore(movement_controller.baseParameters().pathExploreRate.value_or(100.0));
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
    if (auto result = find_path(movement_controller, *m_target_position); !result)
      return std::unexpected(std::move(result.error()));
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
        if (modifiers.jumpingSuppressed) {
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
          auto acc = parameters.airForce.value_or(0.0) / movement_controller.mass();
          source_velocity =
            delta.normalized() * std::fmin(parameters.flySpeed.value_or(0.0), speed_along_angle + acc * dt);
          target_velocity = source_velocity;
        }
        break;
      case Action::Swim:
        source_velocity = target_velocity =
          delta.normalized() * parameters.flySpeed.value_or(0.0f) * (1.0f - parameters.liquidImpedance.value_or(0.0f));
        break;
      case Action::Walk:
        source_velocity =
          delta.normalized() * (run ? parameters.runSpeed.value_or(0.0f) : parameters.walkSpeed.value_or(0.0f));
        source_velocity *= modifiers.speedModifier;
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

    if (auto direction = directionOf(delta[0]))
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
        auto response = object->callScript("hasCapability", LuaVariadic<LuaValue>{*arg});
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
  auto world_collision = m_world.get().polyCollision(poly);
  if (!world_collision)
    return std::unexpected(std::move(world_collision.error()));
  bool collided = *world_collision;
  if (!collided) {
    auto moving = moving_collision(movement_controller, poly);
    if (!moving) return std::unexpected(std::move(moving.error()));
    collided = *moving;
  }
  if (collided) {
    auto bounds = RectI::integral(poly.boundBox());
    auto tile_collision = m_world.get().rectTileCollision(bounds);
    if (!tile_collision)
      return std::unexpected(std::move(tile_collision.error()));
    if (!*tile_collision)
      return false;
    auto solid = m_world.get().rectTileCollision(bounds, solid_collision);
    if (!solid) return std::unexpected(std::move(solid.error()));
    if (*solid) return false;
    auto opened = open_doors(poly.boundBox());
    if (!opened) return std::unexpected(std::move(opened.error()));
    if (!*opened) return false;
  }
  auto in_liquid = [&movement_controller,
                    &world = m_world.get()](Vec2F const& position) -> std::expected<bool, StarException> {
    auto bounds = movement_controller.localBoundBox().translated(position);
    auto liquid = world.liquidLevel(bounds);
    if (!liquid)
      return std::unexpected(std::move(liquid.error()));
    return liquid->level >= movement_controller.baseParameters().minimumLiquidPercentage.value_or(1.0);
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
  auto scanned = movement_controller.forEachMovingCollision(
    collision_poly.boundBox(),
    [&](MovingCollisionId, PhysicsMovingCollision, PolyF poly, RectF) -> std::expected<bool, StarException> {
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
  Vec2I min = Vec2I(bounds.xMin(), bounds.yMin() - 1);
  Vec2I max = Vec2I(bounds.xMax(), bounds.yMin());
  return m_world.get().rectTileCollision(RectI(min, max), collision_set);
}
}

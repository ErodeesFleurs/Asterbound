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
  auto jumpSpeedResult = config.optFloat("jumpSpeed");
  if (!jumpSpeedResult) return std::unexpected(std::move(jumpSpeedResult.error()));
  value.jumpSpeed = *jumpSpeedResult;
  auto jumpControlForceResult = config.optFloat("jumpControlForce");
  if (!jumpControlForceResult) return std::unexpected(std::move(jumpControlForceResult.error()));
  value.jumpControlForce = *jumpControlForceResult;
  auto jumpInitialPercentageResult = config.optFloat("jumpInitialPercentage");
  if (!jumpInitialPercentageResult) return std::unexpected(std::move(jumpInitialPercentageResult.error()));
  value.jumpInitialPercentage = *jumpInitialPercentageResult;
  auto jumpHoldTimeResult = config.optFloat("jumpHoldTime");
  if (!jumpHoldTimeResult) return std::unexpected(std::move(jumpHoldTimeResult.error()));
  value.jumpHoldTime = *jumpHoldTimeResult;
  auto jumpTotalHoldTimeResult = config.optFloat("jumpTotalHoldTime");
  if (!jumpTotalHoldTimeResult) return std::unexpected(std::move(jumpTotalHoldTimeResult.error()));
  value.jumpTotalHoldTime = *jumpTotalHoldTimeResult;
  auto multiJumpResult = config.optBool("multiJump");
  if (!multiJumpResult) return std::unexpected(std::move(multiJumpResult.error()));
  value.multiJump = *multiJumpResult;
  auto reJumpDelayResult = config.optFloat("reJumpDelay");
  if (!reJumpDelayResult) return std::unexpected(std::move(reJumpDelayResult.error()));
  value.reJumpDelay = *reJumpDelayResult;
  auto autoJumpResult = config.optBool("autoJump");
  if (!autoJumpResult) return std::unexpected(std::move(autoJumpResult.error()));
  value.autoJump = *autoJumpResult;
  auto collisionCancelledResult = config.optBool("collisionCancelled");
  if (!collisionCancelledResult) return std::unexpected(std::move(collisionCancelledResult.error()));
  value.collisionCancelled = *collisionCancelledResult;
  return value;
}

Json ActorJumpProfile::toJson() const {
  return JsonObject{
    {"jumpSpeed", jsonFromOptional(jumpSpeed)},
    {"jumpControlForce", jsonFromOptional(jumpControlForce)},
    {"jumpInitialPercentage", jsonFromOptional(jumpInitialPercentage)},
    {"jumpHoldTime", jsonFromOptional(jumpHoldTime)},
    {"jumpTotalHoldTime", jsonFromOptional(jumpTotalHoldTime)},
    {"multiJump", jsonFromOptional(multiJump)},
    {"reJumpDelay", jsonFromOptional(reJumpDelay)},
    {"autoJump", jsonFromOptional(autoJump)},
    {"collisionCancelled", jsonFromOptional(collisionCancelled)}
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
  auto object = config->toObject();
  if (!object) return std::unexpected(std::move(object.error()));
  return create(Json(std::move(*object)));
}

ActorMovementParameters::ActorMovementParameters() = default;

std::expected<ActorMovementParameters, StarException> ActorMovementParameters::create(Json const& config) {
  ActorMovementParameters value;
  if (config.isNull())
    return value;

  auto massResult = config.optFloat("mass");
  if (!massResult) return std::unexpected(std::move(massResult.error()));
  value.mass = *massResult;
  auto gravityMultiplierResult = config.optFloat("gravityMultiplier");
  if (!gravityMultiplierResult) return std::unexpected(std::move(gravityMultiplierResult.error()));
  value.gravityMultiplier = *gravityMultiplierResult;
  auto liquidBuoyancyResult = config.optFloat("liquidBuoyancy");
  if (!liquidBuoyancyResult) return std::unexpected(std::move(liquidBuoyancyResult.error()));
  value.liquidBuoyancy = *liquidBuoyancyResult;
  auto airBuoyancyResult = config.optFloat("airBuoyancy");
  if (!airBuoyancyResult) return std::unexpected(std::move(airBuoyancyResult.error()));
  value.airBuoyancy = *airBuoyancyResult;
  auto bounceFactorResult = config.optFloat("bounceFactor");
  if (!bounceFactorResult) return std::unexpected(std::move(bounceFactorResult.error()));
  value.bounceFactor = *bounceFactorResult;
  auto stopOnFirstBounceResult = config.optBool("stopOnFirstBounce");
  if (!stopOnFirstBounceResult) return std::unexpected(std::move(stopOnFirstBounceResult.error()));
  value.stopOnFirstBounce = *stopOnFirstBounceResult;
  auto enableSurfaceSlopeCorrectionResult = config.optBool("enableSurfaceSlopeCorrection");
  if (!enableSurfaceSlopeCorrectionResult) return std::unexpected(std::move(enableSurfaceSlopeCorrectionResult.error()));
  value.enableSurfaceSlopeCorrection = *enableSurfaceSlopeCorrectionResult;
  auto slopeSlidingFactorResult = config.optFloat("slopeSlidingFactor");
  if (!slopeSlidingFactorResult) return std::unexpected(std::move(slopeSlidingFactorResult.error()));
  value.slopeSlidingFactor = *slopeSlidingFactorResult;
  auto maxMovementPerStepResult = config.optFloat("maxMovementPerStep");
  if (!maxMovementPerStepResult) return std::unexpected(std::move(maxMovementPerStepResult.error()));
  value.maxMovementPerStep = *maxMovementPerStepResult;
  auto maximumCorrectionResult = config.optFloat("maximumCorrection");
  if (!maximumCorrectionResult) return std::unexpected(std::move(maximumCorrectionResult.error()));
  value.maximumCorrection = *maximumCorrectionResult;
  auto speedLimitResult = config.optFloat("speedLimit");
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

  if (!standingPolyConfig.isNull())
{
    auto standingPolyValue = jsonToPolyF(standingPolyConfig);
    if (!standingPolyValue) return std::unexpected(std::move(standingPolyValue.error()));
    value.standingPoly = std::move(*standingPolyValue);
  }
  else if (!collisionPolyConfig.isNull())
{
    auto standingPolyValue = jsonToPolyF(collisionPolyConfig);
    if (!standingPolyValue) return std::unexpected(std::move(standingPolyValue.error()));
    value.standingPoly = std::move(*standingPolyValue);
  }

  if (!crouchingPolyConfig.isNull())
{
    auto crouchingPolyValue = jsonToPolyF(crouchingPolyConfig);
    if (!crouchingPolyValue) return std::unexpected(std::move(crouchingPolyValue.error()));
    value.crouchingPoly = std::move(*crouchingPolyValue);
  }
  else if (!collisionPolyConfig.isNull())
{
    auto crouchingPolyValue = jsonToPolyF(collisionPolyConfig);
    if (!crouchingPolyValue) return std::unexpected(std::move(crouchingPolyValue.error()));
    value.crouchingPoly = std::move(*crouchingPolyValue);
  }

  auto stickyCollisionResult = config.optBool("stickyCollision");
  if (!stickyCollisionResult) return std::unexpected(std::move(stickyCollisionResult.error()));
  value.stickyCollision = *stickyCollisionResult;
  auto stickyForceResult = config.optFloat("stickyForce");
  if (!stickyForceResult) return std::unexpected(std::move(stickyForceResult.error()));
  value.stickyForce = *stickyForceResult;

  auto walkSpeedResult = config.optFloat("walkSpeed");
  if (!walkSpeedResult) return std::unexpected(std::move(walkSpeedResult.error()));
  value.walkSpeed = *walkSpeedResult;
  auto runSpeedResult = config.optFloat("runSpeed");
  if (!runSpeedResult) return std::unexpected(std::move(runSpeedResult.error()));
  value.runSpeed = *runSpeedResult;
  auto flySpeedResult = config.optFloat("flySpeed");
  if (!flySpeedResult) return std::unexpected(std::move(flySpeedResult.error()));
  value.flySpeed = *flySpeedResult;
  auto airFrictionResult = config.optFloat("airFriction");
  if (!airFrictionResult) return std::unexpected(std::move(airFrictionResult.error()));
  value.airFriction = *airFrictionResult;
  auto liquidFrictionResult = config.optFloat("liquidFriction");
  if (!liquidFrictionResult) return std::unexpected(std::move(liquidFrictionResult.error()));
  value.liquidFriction = *liquidFrictionResult;
  auto minimumLiquidPercentageResult = config.optFloat("minimumLiquidPercentage");
  if (!minimumLiquidPercentageResult) return std::unexpected(std::move(minimumLiquidPercentageResult.error()));
  value.minimumLiquidPercentage = *minimumLiquidPercentageResult;
  auto liquidImpedanceResult = config.optFloat("liquidImpedance");
  if (!liquidImpedanceResult) return std::unexpected(std::move(liquidImpedanceResult.error()));
  value.liquidImpedance = *liquidImpedanceResult;
  auto normalGroundFrictionResult = config.optFloat("normalGroundFriction");
  if (!normalGroundFrictionResult) return std::unexpected(std::move(normalGroundFrictionResult.error()));
  value.normalGroundFriction = *normalGroundFrictionResult;
  auto ambulatingGroundFrictionResult = config.optFloat("ambulatingGroundFriction");
  if (!ambulatingGroundFrictionResult) return std::unexpected(std::move(ambulatingGroundFrictionResult.error()));
  value.ambulatingGroundFriction = *ambulatingGroundFrictionResult;
  auto groundForceResult = config.optFloat("groundForce");
  if (!groundForceResult) return std::unexpected(std::move(groundForceResult.error()));
  value.groundForce = *groundForceResult;
  auto airForceResult = config.optFloat("airForce");
  if (!airForceResult) return std::unexpected(std::move(airForceResult.error()));
  value.airForce = *airForceResult;
  auto liquidForceResult = config.optFloat("liquidForce");
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

  auto fallStatusSpeedMinResult = config.optFloat("fallStatusSpeedMin");
  if (!fallStatusSpeedMinResult) return std::unexpected(std::move(fallStatusSpeedMinResult.error()));
  value.fallStatusSpeedMin = *fallStatusSpeedMinResult;
  auto fallThroughSustainFramesResult = config.optInt("fallThroughSustainFrames");
  if (!fallThroughSustainFramesResult) return std::unexpected(std::move(fallThroughSustainFramesResult.error()));
  value.fallThroughSustainFrames = *fallThroughSustainFramesResult;
  auto maximumPlatformCorrectionResult = config.optFloat("maximumPlatformCorrection");
  if (!maximumPlatformCorrectionResult) return std::unexpected(std::move(maximumPlatformCorrectionResult.error()));
  value.maximumPlatformCorrection = *maximumPlatformCorrectionResult;
  auto maximumPlatformCorrectionVelocityFactorResult = config.optFloat("maximumPlatformCorrectionVelocityFactor");
  if (!maximumPlatformCorrectionVelocityFactorResult) return std::unexpected(std::move(maximumPlatformCorrectionVelocityFactorResult.error()));
  value.maximumPlatformCorrectionVelocityFactor = *maximumPlatformCorrectionVelocityFactorResult;

  auto physicsEffectCategoriesConfig = config.opt("physicsEffectCategories");
  if (!physicsEffectCategoriesConfig) return std::unexpected(std::move(physicsEffectCategoriesConfig.error()));
  if (*physicsEffectCategoriesConfig) {
    auto physicsEffectCategoriesValue = jsonToStringSet(**physicsEffectCategoriesConfig);
    if (!physicsEffectCategoriesValue) return std::unexpected(std::move(physicsEffectCategoriesValue.error()));
    value.physicsEffectCategories = std::move(*physicsEffectCategoriesValue);
  }

  auto groundMovementMinimumSustainResult = config.optFloat("groundMovementMinimumSustain");
  if (!groundMovementMinimumSustainResult) return std::unexpected(std::move(groundMovementMinimumSustainResult.error()));
  value.groundMovementMinimumSustain = *groundMovementMinimumSustainResult;
  auto groundMovementMaximumSustainResult = config.optFloat("groundMovementMaximumSustain");
  if (!groundMovementMaximumSustainResult) return std::unexpected(std::move(groundMovementMaximumSustainResult.error()));
  value.groundMovementMaximumSustain = *groundMovementMaximumSustainResult;
  auto groundMovementCheckDistanceResult = config.optFloat("groundMovementCheckDistance");
  if (!groundMovementCheckDistanceResult) return std::unexpected(std::move(groundMovementCheckDistanceResult.error()));
  value.groundMovementCheckDistance = *groundMovementCheckDistanceResult;

  auto collisionEnabledResult = config.optBool("collisionEnabled");
  if (!collisionEnabledResult) return std::unexpected(std::move(collisionEnabledResult.error()));
  value.collisionEnabled = *collisionEnabledResult;
  auto frictionEnabledResult = config.optBool("frictionEnabled");
  if (!frictionEnabledResult) return std::unexpected(std::move(frictionEnabledResult.error()));
  value.frictionEnabled = *frictionEnabledResult;
  auto gravityEnabledResult = config.optBool("gravityEnabled");
  if (!gravityEnabledResult) return std::unexpected(std::move(gravityEnabledResult.error()));
  value.gravityEnabled = *gravityEnabledResult;

  auto pathExploreRateResult = config.optFloat("pathExploreRate");
  if (!pathExploreRateResult) return std::unexpected(std::move(pathExploreRateResult.error()));
  value.pathExploreRate = *pathExploreRateResult;
  return value;
}

std::expected<Json, StarException> ActorMovementParameters::toJson() const {
  auto standingPolyJson = jsonFromOptional(standingPoly, jsonFromPolyF);
  if (!standingPolyJson) return std::unexpected(std::move(standingPolyJson.error()));
  auto crouchingPolyJson = jsonFromOptional(crouchingPoly, jsonFromPolyF);
  if (!crouchingPolyJson) return std::unexpected(std::move(crouchingPolyJson.error()));
  auto physicsEffectCategoriesJson = jsonFromOptional(physicsEffectCategories, jsonFromStringSet);
  if (!physicsEffectCategoriesJson) return std::unexpected(std::move(physicsEffectCategoriesJson.error()));

  return JsonObject{{"mass", jsonFromOptional(mass)},
      {"gravityMultiplier", jsonFromOptional(gravityMultiplier)},
      {"liquidBuoyancy", jsonFromOptional(liquidBuoyancy)},
      {"airBuoyancy", jsonFromOptional(airBuoyancy)},
      {"bounceFactor", jsonFromOptional(bounceFactor)},
      {"stopOnFirstBounce", jsonFromOptional(stopOnFirstBounce)},
      {"enableSurfaceSlopeCorrection", jsonFromOptional(enableSurfaceSlopeCorrection)},
      {"slopeSlidingFactor", jsonFromOptional(slopeSlidingFactor)},
      {"maxMovementPerStep", jsonFromOptional(maxMovementPerStep)},
      {"maximumCorrection", jsonFromOptional(maximumCorrection)},
      {"speedLimit", jsonFromOptional(speedLimit)},
      {"standingPoly", *standingPolyJson},
      {"crouchingPoly", *crouchingPolyJson},
      {"stickyCollision", jsonFromOptional(stickyCollision)},
      {"stickyForce", jsonFromOptional(stickyForce)},
      {"walkSpeed", jsonFromOptional(walkSpeed)},
      {"runSpeed", jsonFromOptional(runSpeed)},
      {"flySpeed", jsonFromOptional(flySpeed)},
      {"airFriction", jsonFromOptional(airFriction)},
      {"liquidFriction", jsonFromOptional(liquidFriction)},
      {"minimumLiquidPercentage", jsonFromOptional(minimumLiquidPercentage)},
      {"liquidImpedance", jsonFromOptional(liquidImpedance)},
      {"normalGroundFriction", jsonFromOptional(normalGroundFriction)},
      {"ambulatingGroundFriction", jsonFromOptional(ambulatingGroundFriction)},
      {"groundForce", jsonFromOptional(groundForce)},
      {"airForce", jsonFromOptional(airForce)},
      {"liquidForce", jsonFromOptional(liquidForce)},
      {"airJumpProfile", airJumpProfile.toJson()},
      {"liquidJumpProfile", liquidJumpProfile.toJson()},
      {"fallStatusSpeedMin", jsonFromOptional(fallStatusSpeedMin)},
      {"fallThroughSustainFrames", jsonFromOptional(fallThroughSustainFrames)},
      {"maximumPlatformCorrection", jsonFromOptional(maximumPlatformCorrection)},
      {"maximumPlatformCorrectionVelocityFactor", jsonFromOptional(maximumPlatformCorrectionVelocityFactor)},
      {"physicsEffectCategories", *physicsEffectCategoriesJson},
      {"groundMovementMinimumSustain", jsonFromOptional(groundMovementMinimumSustain)},
      {"groundMovementMaximumSustain", jsonFromOptional(groundMovementMaximumSustain)},
      {"groundMovementCheckDistance", jsonFromOptional(groundMovementCheckDistance)},
      {"collisionEnabled", jsonFromOptional(collisionEnabled)},
      {"frictionEnabled", jsonFromOptional(frictionEnabled)},
      {"gravityEnabled", jsonFromOptional(gravityEnabled)},
      {"pathExploreRate", jsonFromOptional(pathExploreRate)}};
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
  if (!config.isNull()) {
    auto groundMovementModifierResult = config.getFloat("groundMovementModifier", 1.0f);
    if (!groundMovementModifierResult) return std::unexpected(std::move(groundMovementModifierResult.error()));
    value.groundMovementModifier = *groundMovementModifierResult;
    auto liquidMovementModifierResult = config.getFloat("liquidMovementModifier", 1.0f);
    if (!liquidMovementModifierResult) return std::unexpected(std::move(liquidMovementModifierResult.error()));
    value.liquidMovementModifier = *liquidMovementModifierResult;
    auto speedModifierResult = config.getFloat("speedModifier", 1.0f);
    if (!speedModifierResult) return std::unexpected(std::move(speedModifierResult.error()));
    value.speedModifier = *speedModifierResult;
    auto airJumpModifierResult = config.getFloat("airJumpModifier", 1.0f);
    if (!airJumpModifierResult) return std::unexpected(std::move(airJumpModifierResult.error()));
    value.airJumpModifier = *airJumpModifierResult;
    auto liquidJumpModifierResult = config.getFloat("liquidJumpModifier", 1.0f);
    if (!liquidJumpModifierResult) return std::unexpected(std::move(liquidJumpModifierResult.error()));
    value.liquidJumpModifier = *liquidJumpModifierResult;
    auto runningSuppressedResult = config.getBool("runningSuppressed", false);
    if (!runningSuppressedResult) return std::unexpected(std::move(runningSuppressedResult.error()));
    value.runningSuppressed = *runningSuppressedResult;
    auto jumpingSuppressedResult = config.getBool("jumpingSuppressed", false);
    if (!jumpingSuppressedResult) return std::unexpected(std::move(jumpingSuppressedResult.error()));
    value.jumpingSuppressed = *jumpingSuppressedResult;
    auto facingSuppressedResult = config.getBool("facingSuppressed", false);
    if (!facingSuppressedResult) return std::unexpected(std::move(facingSuppressedResult.error()));
    value.facingSuppressed = *facingSuppressedResult;
    auto movementSuppressedResult = config.getBool("movementSuppressed", false);
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
  auto movingName = enumName<DirectionNames>(movingDirection());
  if (!movingName) return std::unexpected(std::move(movingName.error()));
  auto facingName = enumName<DirectionNames>(m_facingDirection.get());
  if (!facingName) return std::unexpected(std::move(facingName.error()));
  return JsonObject{{"position", jsonFromVec2F(MovementController::position())},
                    {"velocity", jsonFromVec2F(velocity())},
                    {"rotation", MovementController::rotation()},
                    {"movingDirection", String(*movingName)},
                    {"facingDirection", String(*facingName)},
                    {"crouching", m_crouching.get()}};
}

std::expected<void, StarException> ActorMovementController::loadState(Json const& state) {
  auto loaded = MovementController::loadState(state);
  if (!loaded) return std::unexpected(std::move(loaded.error()));
  auto movingName = state.getString("movingDirection");
  if (!movingName) return std::unexpected(std::move(movingName.error()));
  auto movingDirection = enumValue<DirectionNames>(*movingName);
  if (!movingDirection) return std::unexpected(std::move(movingDirection.error()));
  auto facingName = state.getString("facingDirection");
  if (!facingName) return std::unexpected(std::move(facingName.error()));
  auto facingDirection = enumValue<DirectionNames>(*facingName);
  if (!facingDirection) return std::unexpected(std::move(facingDirection.error()));
  auto crouchingValue = state.getBool("crouching");
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
  if (!m_pathController->targetPosition().has_value() || (parameters && m_pathController->parameters() != *parameters)) {
    if (parameters)
      m_pathController->setParameters(*parameters);
    auto pathResult = m_pathController->findPath(*this, position);
    if (!pathResult) return std::unexpected(std::move(pathResult.error()));
    m_pathMoveResult = pathResult->transform([position](bool result) { return std::pair<Vec2F, bool>(position, result); });
  }

  // update target position if it has changed
  if (m_pathController) {
    if (auto result = m_pathController->findPath(*this, position); !result) return std::unexpected(std::move(result.error()));
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

        auto action = m_pathController->curAction();
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
        auto pathResult = m_pathController->findPath(*this, m_controlPathMove->first);
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


PathController::PathController(World& world)
  : m_world(world), m_edgeIndex(0), m_edgeTimer(0.0) { }

PlatformerAStar::Parameters const& PathController::parameters() {
  return m_parameters;
}

void PathController::setParameters(PlatformerAStar::Parameters const& parameters) {
  m_parameters = parameters;
}

void PathController::reset() {
  m_startPosition = {};
  m_targetPosition = {};
  m_controlFace = {};
  m_pathFinder = {};
  m_path = {};
  m_edgeIndex = 0;
  m_edgeTimer = 0.0;
}

bool PathController::pathfinding() const {
  return !m_path.has_value();
}

std::optional<Vec2F> PathController::targetPosition() const {
  return m_targetPosition;
}

std::optional<Direction> PathController::facing() const {
  return m_controlFace;
}

std::optional<PlatformerAStar::Action> PathController::curAction() const {
  if (m_path && m_edgeIndex < m_path->size()) {
    return (*m_path)[m_edgeIndex].action;
  }
  return {};
}

std::expected<std::optional<bool>, StarException> PathController::findPath(ActorMovementController& movementController, Vec2F const& targetPosition) {
  using namespace PlatformerAStar;

  // reached the end of the last path and we have a new target position to move toward
  if (m_path && m_edgeIndex == m_path->size() && m_world.get().geometry().diff(*m_targetPosition, targetPosition).magnitude() > 0.001) {
    reset();
    m_targetPosition = targetPosition;
  }

  // starting a new path, or the target position moved by more than 2 blocks
  if (!m_targetPosition || (!m_path && !m_pathFinder) || m_world.get().geometry().diff(*m_targetPosition, targetPosition).magnitude() > 2.0) {
    auto grounded = movementController.onGround();
    if (m_path) {
      // if already moving on a path, collision will be disabled and we can't use MovementController::onGround() to check for ground collision
      auto const groundCollision = CollisionSet{ CollisionKind::Null, CollisionKind::Block, CollisionKind::Slippery, CollisionKind::Platform };
      auto groundedResult = onGround(movementController, movementController.position(), groundCollision);
      if (!groundedResult) return std::unexpected(std::move(groundedResult.error()));
      grounded = *groundedResult;
    }
    if (*movementController.parameters().gravityEnabled && !grounded && !movementController.liquidMovement()) {
      return {};
    }
    m_startPosition = movementController.position();
    m_targetPosition = targetPosition;
    auto finder = PathFinder::create(m_world.get(), movementController.position(), *m_targetPosition, movementController.baseParameters(), m_parameters);
    if (!finder) return std::unexpected(std::move(finder.error()));
    m_pathFinder = std::move(*finder);
  }

  if (!m_pathFinder && m_path && m_edgeIndex == m_path->size())
    return true; // Reached goal

  if (m_pathFinder) {
    auto exploration = m_pathFinder->explore(movementController.baseParameters().pathExploreRate.value_or(100.0));
    if (!exploration) return std::unexpected(std::move(exploration.error()));
    auto explored = *exploration;
    if (explored) {
      auto pathfinder = m_pathFinder;
      m_pathFinder = {};

      if (*explored) {
        if (!pathfinder->result()) return std::unexpected(ActorMovementControllerException("Successful path search has no path"));
        auto path = *pathfinder->result();

        float newEdgeTimer = 0.0;
        float newEdgeIndex = 0.0;

        // if we have a path already, see if our paths can be merged either by splicing or fast forwarding
        bool merged = false;
        if (m_path && !path.empty()) {
          // try to fast forward on the new path
          std::size_t i = 0;
          // fast forward from current edge, or the last edge of the path
          auto curEdgeIndex = std::min(m_edgeIndex, m_path->size() - 1);
          auto curEdgeResult = m_path->at(curEdgeIndex);
          if (!curEdgeResult) return std::unexpected(std::move(curEdgeResult.error()));
          auto& curEdge = curEdgeResult->get();
          for (auto& edge : path) {
            if (curEdge.action == edge.action && curEdge.source.position == edge.source.position && edge.target.position == edge.target.position) {
              // fast forward on the new path
              newEdgeTimer = m_edgeTimer;
              newEdgeIndex = i;

              merged = true;
              break;
            }
            i++;
          }

          if (!merged) {
            // try to splice the new path onto the current path
            auto& newPathStart = path[0];
            for (std::size_t i = m_edgeIndex; i < m_path->size(); i += 2) {
              auto& edge = (*m_path)[i];
              if (edge.target.position == newPathStart.source.position) {
                // splice the new path onto our current path up to this index
                auto newPath = m_path->slice(0, i + 1);
                newPath.append_range(path);
                path = std::move(newPath);

                newEdgeTimer = m_edgeTimer;
                newEdgeIndex = m_edgeIndex;

                merged = true;
                break;
              }
            }
          }
        }

        if (!merged && movementController.position() != *m_startPosition) {
          // merging the paths failed, and the entity has moved from the path start position
          // try to bridge the gap from the current position to the new path
          auto bridgeResult = PathFinder::create(m_world.get(), movementController.position(), *m_startPosition, movementController.baseParameters(), m_parameters);
          if (!bridgeResult) return std::unexpected(std::move(bridgeResult.error()));
          auto bridgePathFinder = std::move(*bridgeResult);
          auto bridgeExploration = bridgePathFinder->explore(movementController.baseParameters().pathExploreRate.value_or(100.0));
          if (!bridgeExploration) return std::unexpected(std::move(bridgeExploration.error()));
          auto explored = *bridgeExploration;

          if (explored && *explored) {
            // concatenate the bridge path with the new path
            if (!bridgePathFinder->result()) return std::unexpected(ActorMovementControllerException("Successful bridge search has no path"));
            auto newPath = *bridgePathFinder->result();
            newPath.append_range(path);
            path = newPath;
          } else {
            // if the gap isn't bridged in a single tick, reset and start over
            reset();
            return {};
          }
        }

        bool valid = true;
        if (!path.empty()) {
          auto edgeValid = validateEdge(movementController, path[0]);
          if (!edgeValid) return std::unexpected(std::move(edgeValid.error()));
          valid = *edgeValid;
        }
        if (!valid) {
          // reset if the first edge is invalid
          reset();
          return false;
        }

        m_edgeTimer = newEdgeTimer;
        m_edgeIndex = newEdgeIndex;
        m_path = path;
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

std::expected<std::optional<bool>, StarException> PathController::move(ActorMovementController& movementController, ActorMovementParameters const& parameters, ActorMovementModifiers const& modifiers, bool run, float dt) {
  using namespace PlatformerAStar;

  // pathfind to a new target position in the background while moving on the current path
  if (m_pathFinder && m_targetPosition) {
    if (auto result = findPath(movementController, *m_targetPosition); !result) return std::unexpected(std::move(result.error()));
  }

  if (!m_path)
    return {};

  m_controlFace = {};

  while (m_edgeIndex < m_path->size()) {
    auto& edge = (*m_path)[m_edgeIndex];
    Vec2F delta = m_world.get().geometry().diff(edge.target.position, edge.source.position);

    Vec2F sourceVelocity;
    Vec2F targetVelocity;
    switch (edge.action) {
      case Action::Jump:
        if (modifiers.jumpingSuppressed) {
          reset();
          return {};
        }
        break;
      case Action::Arc:
        sourceVelocity = edge.source.velocity.value_or(Vec2F());
        targetVelocity = edge.target.velocity.value_or(Vec2F());
        break;
      case Action::Drop:
        targetVelocity = edge.target.velocity.value_or(Vec2F());
        break;
      case Action::Fly:
        {
          // accelerate along path using airForce
          float angleFactor = movementController.velocity().normalized() * delta.normalized();
          float speedAlongAngle = angleFactor * movementController.velocity().magnitude();
          auto acc = parameters.airForce.value_or(0.0) / movementController.mass();
          sourceVelocity = delta.normalized() * std::fmin(parameters.flySpeed.value_or(0.0), speedAlongAngle + acc * dt);
          targetVelocity = sourceVelocity;
        }
        break;
      case Action::Swim:
        sourceVelocity = targetVelocity = delta.normalized() * parameters.flySpeed.value_or(0.0f) * (1.0f - parameters.liquidImpedance.value_or(0.0f));
        break;
      case Action::Walk:
        sourceVelocity = delta.normalized() * (run ? parameters.runSpeed.value_or(0.0f) : parameters.walkSpeed.value_or(0.0f));
        sourceVelocity *= modifiers.speedModifier;
        targetVelocity = sourceVelocity;
        break;
      default: {}
    }

    Vec2F avgVelocity = (sourceVelocity + targetVelocity) / 2.0;
    float avgSpeed = avgVelocity.magnitude();
    auto edgeTime = avgSpeed > 0.0f ? delta.magnitude() / avgSpeed : 0.2;

    auto edgeProgress = m_edgeTimer / edgeTime;
    if (edgeProgress > 1.0f) {
      m_edgeTimer -= edgeTime;
      m_edgeIndex++;
      if (m_edgeIndex < m_path->size()) {
        auto valid = validateEdge(movementController, (*m_path)[m_edgeIndex]);
        if (!valid) return std::unexpected(std::move(valid.error()));
        if (!*valid) {
          reset();
          return {};
        }
      }
      continue;
    }

    auto curVelocity = sourceVelocity + ((targetVelocity - sourceVelocity) * edgeProgress);
    movementController.setVelocity(curVelocity);
    auto movement = (curVelocity + sourceVelocity) / 2.0 * m_edgeTimer;
    movementController.setPosition(edge.source.position + movement);

    if (auto direction = directionOf(delta[0]))
      m_controlFace = direction;

    m_edgeTimer += dt;
    return {};
  }

  if (auto lastEdge = m_path->back_value()) {
    movementController.setPosition(lastEdge->target.position);
    movementController.setVelocity(Vec2F());
  }

  // reached the end of the path, success unless we're also currently pathfinding to a new position
  if (m_pathFinder)
    return {};
  else
    return true;
}

std::expected<bool, StarException> PathController::validateEdge(ActorMovementController& movementController, PlatformerAStar::Edge const& edge) {
  using namespace PlatformerAStar;
  auto const groundCollision = CollisionSet{CollisionKind::Null, CollisionKind::Block, CollisionKind::Slippery, CollisionKind::Platform};
  auto const solidCollision = CollisionSet{CollisionKind::Null, CollisionKind::Block, CollisionKind::Slippery};
  auto openDoors = [&world = m_world.get()](RectF const& bounds) -> std::expected<bool, StarException> {
    auto objects = world.entityQuery(bounds, entityTypeFilter<Object>());
    if (!objects) return std::unexpected(std::move(objects.error()));
    bool opened = false;
    for (auto const& entity : *objects) {
      if (auto object = as<Object>(entity); object && object->isMaster()) {
        auto arg = world.luaRoot()->luaEngine().createString("closedDoor");
        if (!arg) return std::unexpected(std::move(arg.error()));
        auto response = object->callScript("hasCapability", LuaVariadic<LuaValue>{*arg});
        if (!response) return std::unexpected(std::move(response.error()));
        if (*response && std::holds_alternative<LuaBoolean>(**response) && std::get<LuaBoolean>(**response)) {
          auto sent = world.sendEntityMessage(entity->entityId(), "openDoor");
          if (!sent) return std::unexpected(std::move(sent.error()));
          opened = true;
        }
      }
    }
    return opened;
  };
  auto poly = movementController.collisionPoly();
  poly.translate(edge.target.position);
  auto worldCollision = m_world.get().polyCollision(poly);
  if (!worldCollision) return std::unexpected(std::move(worldCollision.error()));
  bool collided = *worldCollision;
  if (!collided) {
    auto moving = movingCollision(movementController, poly);
    if (!moving) return std::unexpected(std::move(moving.error()));
    collided = *moving;
  }
  if (collided) {
    auto bounds = RectI::integral(poly.boundBox());
    auto tileCollision = m_world.get().rectTileCollision(bounds);
    if (!tileCollision) return std::unexpected(std::move(tileCollision.error()));
    if (!*tileCollision) return false;
    auto solid = m_world.get().rectTileCollision(bounds, solidCollision);
    if (!solid) return std::unexpected(std::move(solid.error()));
    if (*solid) return false;
    auto opened = openDoors(poly.boundBox());
    if (!opened) return std::unexpected(std::move(opened.error()));
    if (!*opened) return false;
  }
  auto inLiquid = [&movementController, &world = m_world.get()](Vec2F const& position) -> std::expected<bool, StarException> {
    auto bounds = movementController.localBoundBox().translated(position);
    auto liquid = world.liquidLevel(bounds);
    if (!liquid) return std::unexpected(std::move(liquid.error()));
    return liquid->level >= movementController.baseParameters().minimumLiquidPercentage.value_or(1.0);
  };
  switch (edge.action) {
    case Action::Walk: return onGround(movementController, edge.source.position, groundCollision);
    case Action::Swim: return inLiquid(edge.target.position);
    case Action::Land: {
      auto grounded = onGround(movementController, edge.target.position, groundCollision);
      if (!grounded) return std::unexpected(std::move(grounded.error()));
      if (*grounded) return true;
      return inLiquid(edge.target.position);
    }
    case Action::Drop: {
      auto grounded = onGround(movementController, edge.source.position, groundCollision);
      if (!grounded) return std::unexpected(std::move(grounded.error()));
      if (!*grounded) return false;
      auto solid = onGround(movementController, edge.source.position, solidCollision);
      if (!solid) return std::unexpected(std::move(solid.error()));
      return !*solid;
    }
    default: return true;
  }
}

std::expected<bool, StarException> PathController::movingCollision(ActorMovementController& movementController, PolyF const& collisionPoly) {
  bool collided = false;
  auto scanned = movementController.forEachMovingCollision(collisionPoly.boundBox(), [&](MovingCollisionId, PhysicsMovingCollision, PolyF poly, RectF) -> std::expected<bool, StarException> {
      if (poly.intersects(collisionPoly)) {
        // set collided and stop iterating
        collided = true;
        return false;
      }
      return true;
    });
  if (!scanned) return std::unexpected(std::move(scanned.error()));
  return collided;
}

std::expected<bool, StarException> PathController::onGround(ActorMovementController const& movementController, Vec2F const& position, CollisionSet const& collisionSet) const {
    auto bounds = RectI::integral(movementController.localBoundBox().translated(position));
    Vec2I min = Vec2I(bounds.xMin(), bounds.yMin() - 1);
    Vec2I max = Vec2I(bounds.xMax(), bounds.yMin());
    return m_world.get().rectTileCollision(RectI(min, max), collisionSet);
}

}

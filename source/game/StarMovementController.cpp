import star.format;
import star.json;
import star.json_extra;
import star.interpolation;
import star.id_map;
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

std::expected<MovementParameters, StarException> MovementParameters::sensibleDefaults(Root& root) {
  auto assets = root.assets();
  if (!assets) return std::unexpected(std::move(assets.error()));
  auto config = (*assets)->json("/default_movement.config");
  if (!config) return std::unexpected(std::move(config.error()));
  auto object = config->to_object();
  if (!object) return std::unexpected(std::move(object.error()));
  return create(Json(std::move(*object)));
}

MovementParameters::MovementParameters() = default;

std::expected<MovementParameters, StarException> MovementParameters::create(Json const& config) {
  MovementParameters value;
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
  auto discontinuityThresholdResult = config.opt_float("discontinuityThreshold");
  if (!discontinuityThresholdResult) return std::unexpected(std::move(discontinuityThresholdResult.error()));
  value.discontinuityThreshold = *discontinuityThresholdResult;
  auto collisionPolyConfig = config.opt("collisionPoly");
  if (!collisionPolyConfig) return std::unexpected(std::move(collisionPolyConfig.error()));
  if (*collisionPolyConfig) {
    auto collisionPolyValue = json_to_polyf(**collisionPolyConfig);
    if (!collisionPolyValue) return std::unexpected(std::move(collisionPolyValue.error()));
    value.collisionPoly = std::move(*collisionPolyValue);
  }
  auto stickyCollisionResult = config.opt_bool("stickyCollision");
  if (!stickyCollisionResult) return std::unexpected(std::move(stickyCollisionResult.error()));
  value.stickyCollision = *stickyCollisionResult;
  auto stickyForceResult = config.opt_float("stickyForce");
  if (!stickyForceResult) return std::unexpected(std::move(stickyForceResult.error()));
  value.stickyForce = *stickyForceResult;
  auto airFrictionResult = config.opt_float("airFriction");
  if (!airFrictionResult) return std::unexpected(std::move(airFrictionResult.error()));
  value.airFriction = *airFrictionResult;
  auto liquidFrictionResult = config.opt_float("liquidFriction");
  if (!liquidFrictionResult) return std::unexpected(std::move(liquidFrictionResult.error()));
  value.liquidFriction = *liquidFrictionResult;
  auto groundFrictionResult = config.opt_float("groundFriction");
  if (!groundFrictionResult) return std::unexpected(std::move(groundFrictionResult.error()));
  value.groundFriction = *groundFrictionResult;
  auto collisionEnabledResult = config.opt_bool("collisionEnabled");
  if (!collisionEnabledResult) return std::unexpected(std::move(collisionEnabledResult.error()));
  value.collisionEnabled = *collisionEnabledResult;
  auto frictionEnabledResult = config.opt_bool("frictionEnabled");
  if (!frictionEnabledResult) return std::unexpected(std::move(frictionEnabledResult.error()));
  value.frictionEnabled = *frictionEnabledResult;
  auto gravityEnabledResult = config.opt_bool("gravityEnabled");
  if (!gravityEnabledResult) return std::unexpected(std::move(gravityEnabledResult.error()));
  value.gravityEnabled = *gravityEnabledResult;
  auto ignorePlatformCollisionResult = config.opt_bool("ignorePlatformCollision");
  if (!ignorePlatformCollisionResult) return std::unexpected(std::move(ignorePlatformCollisionResult.error()));
  value.ignorePlatformCollision = *ignorePlatformCollisionResult;
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
  auto restDurationResult = config.opt_int("restDuration");
  if (!restDurationResult) return std::unexpected(std::move(restDurationResult.error()));
  value.restDuration = *restDurationResult;
  return value;
}

MovementParameters MovementParameters::merge(MovementParameters const& rhs) const {
  MovementParameters merged;

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
  merged.discontinuityThreshold = (rhs.discontinuityThreshold ? rhs.discontinuityThreshold : discontinuityThreshold);
  merged.collisionPoly = (rhs.collisionPoly ? rhs.collisionPoly : collisionPoly);
  merged.stickyCollision = (rhs.stickyCollision ? rhs.stickyCollision : stickyCollision);
  merged.stickyForce = (rhs.stickyForce ? rhs.stickyForce : stickyForce);
  merged.airFriction = (rhs.airFriction ? rhs.airFriction : airFriction);
  merged.liquidFriction = (rhs.liquidFriction ? rhs.liquidFriction : liquidFriction);
  merged.groundFriction = (rhs.groundFriction ? rhs.groundFriction : groundFriction);
  merged.collisionEnabled = (rhs.collisionEnabled ? rhs.collisionEnabled : collisionEnabled);
  merged.frictionEnabled = (rhs.frictionEnabled ? rhs.frictionEnabled : frictionEnabled);
  merged.gravityEnabled = (rhs.gravityEnabled ? rhs.gravityEnabled : gravityEnabled);
  merged.ignorePlatformCollision = (rhs.ignorePlatformCollision ? rhs.ignorePlatformCollision : ignorePlatformCollision);
  merged.maximumPlatformCorrection = (rhs.maximumPlatformCorrection ? rhs.maximumPlatformCorrection : maximumPlatformCorrection);
  merged.maximumPlatformCorrectionVelocityFactor = (rhs.maximumPlatformCorrectionVelocityFactor ? rhs.maximumPlatformCorrectionVelocityFactor : maximumPlatformCorrectionVelocityFactor);
  merged.physicsEffectCategories = (rhs.physicsEffectCategories ? rhs.physicsEffectCategories : physicsEffectCategories);
  merged.restDuration = (rhs.restDuration ? rhs.restDuration : restDuration);

  return merged;
}

std::expected<Json, StarException> MovementParameters::toJson() const {
  auto collisionPolyJson = json_from_optional(collisionPoly, json_from_polyf);
  if (!collisionPolyJson) return std::unexpected(std::move(collisionPolyJson.error()));
  auto physicsEffectCategoriesJson = json_from_optional(physicsEffectCategories, json_from_string_set);
  if (!physicsEffectCategoriesJson) return std::unexpected(std::move(physicsEffectCategoriesJson.error()));

  return JsonObject{
    {"mass", json_from_optional(mass)},
    {"gravityMultiplier", json_from_optional(gravityMultiplier)},
    {"liquidBuoyancy", json_from_optional(liquidBuoyancy)},
    {"airBuoyancy", json_from_optional(airBuoyancy)},
    {"stopOnFirstBounce", json_from_optional(stopOnFirstBounce)},
    {"enableSurfaceSlopeCorrection", json_from_optional(enableSurfaceSlopeCorrection)},
    {"slopeSlidingFactor", json_from_optional(slopeSlidingFactor)},
    {"maxMovementPerStep", json_from_optional(maxMovementPerStep)},
    {"maximumCorrection", json_from_optional(maximumCorrection)},
    {"speedLimit", json_from_optional(speedLimit)},
    {"discontinuityThreshold", json_from_optional(discontinuityThreshold)},
    {"collisionPoly", *collisionPolyJson},
    {"stickyCollision", json_from_optional(stickyCollision)},
    {"stickyForce", json_from_optional(stickyForce)},
    {"airFriction", json_from_optional(airFriction)},
    {"liquidFriction", json_from_optional(liquidFriction)},
    {"groundFriction", json_from_optional(groundFriction)},
    {"collisionEnabled", json_from_optional(collisionEnabled)},
    {"frictionEnabled", json_from_optional(frictionEnabled)},
    {"gravityEnabled", json_from_optional(gravityEnabled)},
    {"ignorePlatformCollision", json_from_optional(ignorePlatformCollision)},
    {"maximumPlatformCorrection", json_from_optional(maximumPlatformCorrection)},
    {"maximumPlatformCorrectionVelocityFactor", json_from_optional(maximumPlatformCorrectionVelocityFactor)},
    {"physicsEffectCategories", *physicsEffectCategoriesJson},
    {"restDuration", json_from_optional(restDuration)}
  };
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, MovementParameters& movementParameters) {
  if (auto result = ds.read(movementParameters.mass); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.read(movementParameters.gravityMultiplier); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.read(movementParameters.liquidBuoyancy); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.read(movementParameters.airBuoyancy); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.read(movementParameters.stopOnFirstBounce); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.read(movementParameters.enableSurfaceSlopeCorrection); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.read(movementParameters.slopeSlidingFactor); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.read(movementParameters.maxMovementPerStep); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.read(movementParameters.maximumCorrection); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.read(movementParameters.speedLimit); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.read(movementParameters.discontinuityThreshold); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.read(movementParameters.collisionPoly); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.read(movementParameters.stickyCollision); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.read(movementParameters.stickyForce); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.read(movementParameters.airFriction); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.read(movementParameters.liquidFriction); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.read(movementParameters.groundFriction); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.read(movementParameters.collisionEnabled); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.read(movementParameters.frictionEnabled); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.read(movementParameters.gravityEnabled); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.read(movementParameters.ignorePlatformCollision); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.read(movementParameters.maximumPlatformCorrection); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.read(movementParameters.maximumPlatformCorrectionVelocityFactor); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.read(movementParameters.physicsEffectCategories); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.read(movementParameters.restDuration); !result) return std::unexpected(std::move(result.error()));

  return std::ref(ds);
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, MovementParameters const& movementParameters) {
  if (auto result = ds.write(movementParameters.mass); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.write(movementParameters.gravityMultiplier); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.write(movementParameters.liquidBuoyancy); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.write(movementParameters.airBuoyancy); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.write(movementParameters.stopOnFirstBounce); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.write(movementParameters.enableSurfaceSlopeCorrection); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.write(movementParameters.slopeSlidingFactor); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.write(movementParameters.maxMovementPerStep); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.write(movementParameters.maximumCorrection); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.write(movementParameters.speedLimit); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.write(movementParameters.discontinuityThreshold); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.write(movementParameters.collisionPoly); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.write(movementParameters.stickyCollision); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.write(movementParameters.stickyForce); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.write(movementParameters.airFriction); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.write(movementParameters.liquidFriction); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.write(movementParameters.groundFriction); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.write(movementParameters.collisionEnabled); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.write(movementParameters.frictionEnabled); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.write(movementParameters.gravityEnabled); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.write(movementParameters.ignorePlatformCollision); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.write(movementParameters.maximumPlatformCorrection); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.write(movementParameters.maximumPlatformCorrectionVelocityFactor); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.write(movementParameters.physicsEffectCategories); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.write(movementParameters.restDuration); !result) return std::unexpected(std::move(result.error()));

  return std::ref(ds);
}

MovementController::MovementController(Root& root) : m_root(root) {
  m_resting = false;

  m_timeStep = root.timing().time_step;

  m_liquidPercentage = 0.0f;
  m_liquidId = EmptyLiquidId;

  m_xPosition.set_fixed_point_base(0.0125f);
  m_yPosition.set_fixed_point_base(0.0125f);
  m_xVelocity.set_fixed_point_base(0.00625f);
  m_yVelocity.set_fixed_point_base(0.00625f);
  m_rotation.set_fixed_point_base(0.01f);
  m_xRelativeSurfaceMovingCollisionPosition.set_fixed_point_base(0.0125f);
  m_yRelativeSurfaceMovingCollisionPosition.set_fixed_point_base(0.0125f);

  m_xVelocity.set_interpolator(lerp<float, float>);
  m_yVelocity.set_interpolator(lerp<float, float>);
  m_rotation.set_interpolator(angle_lerp<float, float>);
  m_xRelativeSurfaceMovingCollisionPosition.set_interpolator(lerp<float, float>);
  m_yRelativeSurfaceMovingCollisionPosition.set_interpolator(lerp<float, float>);

  m_world.reset();
}

std::expected<std::shared_ptr<MovementController>, StarException> MovementController::create(Root& root, MovementParameters const& parameters) {
  auto controller = std::shared_ptr<MovementController>(new MovementController(root));
  auto initialized = controller->initialize(parameters);
  if (!initialized) return std::unexpected(std::move(initialized.error()));
  return controller;
}

std::expected<void, StarException> MovementController::initialize(MovementParameters const& parameters) {
  if (auto result = add_net_element(m_collisionPoly); !result) return std::unexpected(std::move(result.error()));
  if (auto result = add_net_element(m_mass); !result) return std::unexpected(std::move(result.error()));
  if (auto result = add_net_element(m_xPosition); !result) return std::unexpected(std::move(result.error()));
  if (auto result = add_net_element(m_yPosition); !result) return std::unexpected(std::move(result.error()));
  if (auto result = add_net_element(m_xVelocity); !result) return std::unexpected(std::move(result.error()));
  if (auto result = add_net_element(m_yVelocity); !result) return std::unexpected(std::move(result.error()));
  if (auto result = add_net_element(m_rotation); !result) return std::unexpected(std::move(result.error()));
  if (auto result = add_net_element(m_colliding); !result) return std::unexpected(std::move(result.error()));
  if (auto result = add_net_element(m_collisionStuck); !result) return std::unexpected(std::move(result.error()));
  if (auto result = add_net_element(m_nullColliding); !result) return std::unexpected(std::move(result.error()));
  if (auto result = add_net_element(m_stickingDirection); !result) return std::unexpected(std::move(result.error()));
  if (auto result = add_net_element(m_onGround); !result) return std::unexpected(std::move(result.error()));
  if (auto result = add_net_element(m_zeroG); !result) return std::unexpected(std::move(result.error()));

  if (auto result = add_net_element(m_surfaceMovingCollision); !result) return std::unexpected(std::move(result.error()));
  if (auto result = add_net_element(m_xRelativeSurfaceMovingCollisionPosition); !result) return std::unexpected(std::move(result.error()));
  if (auto result = add_net_element(m_yRelativeSurfaceMovingCollisionPosition); !result) return std::unexpected(std::move(result.error()));

  return resetParameters(parameters);
}

MovementParameters const& MovementController::parameters() const {
  return m_parameters;
}

std::expected<void, StarException> MovementController::applyParameters(MovementParameters const& parameters) {
  return updateParameters(m_parameters.merge(parameters));
}

std::expected<void, StarException> MovementController::resetParameters(MovementParameters const& parameters) {
  auto defaults = MovementParameters::sensibleDefaults(m_root);
  if (!defaults) return std::unexpected(std::move(defaults.error()));
  return updateParameters(defaults->merge(parameters));
}

Root& MovementController::root() const {
  return m_root;
}

Json MovementController::storeState() const {
  return JsonObject{
    {"position", json_from_vec2f(position())},
    {"velocity", json_from_vec2f(velocity())},
    {"rotation", rotation()}
  };
}

std::expected<void, StarException> MovementController::loadState(Json const& state) {
  auto positionJson = state.get("position");
  if (!positionJson) return std::unexpected(std::move(positionJson.error()));
  auto positionValue = json_to_vec2f(*positionJson);
  if (!positionValue) return std::unexpected(std::move(positionValue.error()));
  auto velocityJson = state.get("velocity");
  if (!velocityJson) return std::unexpected(std::move(velocityJson.error()));
  auto velocityValue = json_to_vec2f(*velocityJson);
  if (!velocityValue) return std::unexpected(std::move(velocityValue.error()));
  auto rotationValue = state.get_float("rotation");
  if (!rotationValue) return std::unexpected(std::move(rotationValue.error()));
  setPosition(*positionValue);
  setVelocity(*velocityValue);
  setRotation(*rotationValue);
  return {};
}

float MovementController::mass() const {
  return m_mass.get();
}

PolyF const& MovementController::collisionPoly() const {
  return m_collisionPoly.get();
}

void MovementController::setCollisionPoly(PolyF const& poly) {
  m_collisionPoly.set(poly);
}

Vec2F MovementController::position() const {
  return {m_xPosition.get(), m_yPosition.get()};
}

float MovementController::xPosition() const {
  return m_xPosition.get();
}

float MovementController::yPosition() const {
  return m_yPosition.get();
}

Vec2F MovementController::velocity() const {
  return {m_xVelocity.get(), m_yVelocity.get()};
}

float MovementController::xVelocity() const {
  return m_xVelocity.get();
}

float MovementController::yVelocity() const {
  return m_yVelocity.get();
}

float MovementController::rotation() const {
  return m_rotation.get();
}

PolyF MovementController::collisionBody() const {
  auto poly = collisionPoly();
  poly.rotate(rotation());
  poly.translate(position());
  return poly;
}

RectF MovementController::localBoundBox() const {
  auto poly = collisionPoly();
  poly.rotate(rotation());
  return poly.bounding_box();
}

RectF MovementController::collisionBoundBox() const {
  return collisionBody().bounding_box();
}

bool MovementController::isColliding() const {
  return m_colliding.get();
}

bool MovementController::isNullColliding() const {
  return m_nullColliding.get();
}

bool MovementController::isCollisionStuck() const {
  return m_collisionStuck.get();
}

std::optional<float> MovementController::stickingDirection() const {
  return m_stickingDirection.get();
}

float MovementController::liquidPercentage() const {
  return m_liquidPercentage;
}

LiquidId MovementController::liquidId() const {
  return m_liquidId;
}

bool MovementController::onGround() const {
  return m_onGround.get();
}

bool MovementController::zeroG() const {
  return m_zeroG.get();
}

bool MovementController::atWorldLimit(bool bottomOnly) const {
  if (m_world) {
    if (!collisionPoly().empty()) {
      auto bounds = collisionBoundBox();
      return bounds.y_min() <= 0 || (!bottomOnly && bounds.y_max() >= m_world->get().geometry().height());
    } else {
      return yPosition() <= 0 || (!bottomOnly && yPosition() >= m_world->get().geometry().height());
    }
  }
  return false;
}

void MovementController::setPosition(Vec2F position) {
  if (m_world)
    position = m_world->get().geometry().limit(position);

  if (position[0] != m_xPosition.get() || position[1] != m_yPosition.get())
    m_resting = false;

  m_xPosition.set(position[0]);
  m_yPosition.set(position[1]);
}

void MovementController::setXPosition(float xPosition) {
  setPosition({xPosition, yPosition()});
}

void MovementController::setYPosition(float yPosition) {
  setPosition({xPosition(), yPosition});
}

void MovementController::translate(Vec2F const& direction) {
  setPosition(position() + direction);
}

void MovementController::setVelocity(Vec2F vel) {
  if (m_parameters.speedLimit && vel.magnitude() > (*m_parameters.speedLimit)) {
    vel.normalize();
    vel *= (*m_parameters.speedLimit);
  }

  if ((velocity() - vel).magnitude() > 0.0001)
    m_resting = false;

  m_xVelocity.set(vel[0]);
  m_yVelocity.set(vel[1]);
}

void MovementController::setXVelocity(float xVelocity) {
  setVelocity({xVelocity, yVelocity()});
}

void MovementController::setYVelocity(float yVelocity) {
  setVelocity({xVelocity(), yVelocity});
}

void MovementController::addMomentum(Vec2F const& momentum) {
  setVelocity(velocity() + momentum / mass());
}

void MovementController::setRotation(float rotation) {
  m_resting = false;

  m_rotation.set(rotation);
}

void MovementController::rotate(float rotationRate) {
  if (rotationRate == 0.0)
    return;

  m_resting = false;
  m_rotation.set(std::fmod(rotation() + rotationRate * m_timeStep, 2 * std::numbers::pi));
}

void MovementController::accelerate(Vec2F const& acceleration) {
  setVelocity(velocity() + acceleration * m_timeStep);
}

void MovementController::force(Vec2F const& force) {
  setVelocity(velocity() + force / mass() * m_timeStep);
}

void MovementController::approachVelocity(Vec2F const& targetVelocity, float maxControlForce) {
  // Instead of applying the force directly, work backwards and figure out the
  // maximum acceleration that could be achieved by the current control force,
  // and maximize the change in velocity based on that.

  Vec2F diff = targetVelocity - velocity();
  float diffMagnitude = vmag(diff);

  if (diffMagnitude == 0.0f)
    return;

  float maximumAcceleration = maxControlForce / mass() * m_timeStep;
  float clampedMagnitude = clamp(diffMagnitude, 0.0f, maximumAcceleration);

  setVelocity(velocity() + diff * (clampedMagnitude / diffMagnitude));
}

void MovementController::approachVelocityAlongAngle(float angle, float targetVelocity, float maxControlForce, bool positiveOnly) {
  // Same strategy as approachVelocity, work backwards to figure out the
  // maximum acceleration and apply that.

  // Project the current velocity along the axis normal, the velocity
  // difference is the difference between the targetVelocity and this
  // projection.

  Vec2F axis = Vec2F::with_angle(angle, 1.0f);

  float velocityAlongAxis = velocity() * axis;
  float diff = targetVelocity - velocityAlongAxis;
  if (diff == 0.0f)
    return;
  if (positiveOnly && diff < 0)
    return;

  float maximumAcceleration = maxControlForce / mass() * m_timeStep;

  float diffMagnitude = std::fabs(diff);
  float clampedMagnitude = clamp(diffMagnitude, 0.0f, maximumAcceleration);

  setVelocity(velocity() + axis * diff * (clampedMagnitude / diffMagnitude));
}

void MovementController::approachXVelocity(float targetXVelocity, float maxControlForce) {
  approachVelocityAlongAngle(0.0f, targetXVelocity, maxControlForce, false);
}

void MovementController::approachYVelocity(float targetYVelocity, float maxControlForce) {
  approachVelocityAlongAngle(std::numbers::pi / 2, targetYVelocity, maxControlForce, false);
}

void MovementController::init(World& world) {
  m_world = std::ref(world);
  setPosition(position());
  updatePositionInterpolators();
}

void MovementController::uninit() {
  m_world.reset();
  updatePositionInterpolators();
}

void MovementController::setTimestep(float dt) {
  m_timeStep = dt;
}

std::expected<void, StarException> MovementController::tickMaster(float dt) {
  auto activeWorld = world();
  if (!activeWorld) return std::unexpected(std::move(activeWorld.error()));
  World& worldRef = activeWorld->get();
  setTimestep(dt);
  auto geometry = worldRef.geometry();

  bool zeroGravity = !(*m_parameters.gravityEnabled) || (*m_parameters.gravityMultiplier) == 0;
  if (!zeroGravity) {
    auto gravityValue = worldRef.gravity(position());
    if (!gravityValue) return std::unexpected(std::move(gravityValue.error()));
    zeroGravity = *gravityValue == 0;
  }
  m_zeroG.set(zeroGravity);

  std::optional<PhysicsMovingCollision> surfaceCollision;
  if (auto movingCollisionId = m_surfaceMovingCollision.get()) {
    if (auto physicsEntity = worldRef.get<PhysicsEntity>(movingCollisionId->physicsEntityId))
      {
      auto collision = physicsEntity->movingCollision(movingCollisionId->collisionIndex);
      if (!collision) return std::unexpected(std::move(collision.error()));
      surfaceCollision = std::move(*collision);
    }
  }

  if (surfaceCollision) {
    Vec2F surfacePositionDelta = geometry.diff(surfaceCollision->position, m_surfaceMovingCollisionPosition);
    setPosition(position() + surfacePositionDelta);
    Vec2F newSurfaceVelocity = surfacePositionDelta / dt;
    setVelocity(velocity() - m_surfaceVelocity + newSurfaceVelocity);
    m_surfaceVelocity = newSurfaceVelocity;
  } else {
    m_surfaceMovingCollision.set({});
    m_surfaceMovingCollisionPosition = {};
    m_surfaceVelocity = {};
  }

  if (m_resting) {
    m_restTicks -= 1;
    if (m_restTicks < 0) {
      m_resting = false;
    }
  }

  // don't integrate velocity when resting
  Vec2F relativeVelocity = m_resting ? Vec2F() : velocity();
  Vec2F originalMovement = relativeVelocity * dt;
  if (surfaceCollision)
    relativeVelocity -= m_surfaceVelocity;

  m_collisionCorrection = {};
  m_surfaceSlope = Vec2F(1, 0);
  m_surfaceMovingCollision.set({});

  unsigned steps;
  float maxMovementPerStep = (*m_parameters.maxMovementPerStep);
  if (maxMovementPerStep > 0.0f)
    steps = std::floor(vmag(relativeVelocity) * dt / maxMovementPerStep) + 1;
  else
    steps = 1;

  // skip collision checks when resting (there's no movement anyway)
  if (m_resting)
    steps = 0;

  for (unsigned i = 0; i < steps; ++i) {
    float dtSteps = dt / steps;
    Vec2F movement = relativeVelocity * dtSteps;

    if (!(*m_parameters.collisionEnabled) || collisionPoly().empty()) {
      setPosition(position() + movement);
      m_surfaceSlope = Vec2F(1, 0);
      m_surfaceVelocity = Vec2F(0, 0);

      m_colliding.set(false);
      m_collisionStuck.set(false);
      m_nullColliding.set(false);
      m_stickingDirection.set({});
      m_onGround.set(false);

    } else {
      auto body = collisionBody();

      float velocityMagnitude = vmag(relativeVelocity);
      Vec2F velocityDirection = relativeVelocity / velocityMagnitude;

      bool ignorePlatforms = (*m_parameters.ignorePlatformCollision) || relativeVelocity[1] > 0;
      float maximumCorrection = (*m_parameters.maximumCorrection);
      float maximumPlatformCorrection = (*m_parameters.maximumPlatformCorrection)
          + (*m_parameters.maximumPlatformCorrectionVelocityFactor) * velocityMagnitude;
      Vec2F bodyCenter = body.center();

      RectF queryBounds = body.bounding_box().padded(maximumCorrection);
      queryBounds.combine(queryBounds.translated(movement));
      if (auto result = queryCollisions(queryBounds); !result) return std::unexpected(std::move(result.error()));
      auto result = collisionMove(m_workingCollisions, body, movement, ignorePlatforms, (*m_parameters.enableSurfaceSlopeCorrection) && !zeroG(),
          maximumCorrection, maximumPlatformCorrection, bodyCenter, dtSteps);

      setPosition(position() + result.movement);

      if (result.collisionKind == CollisionKind::Null) {
        m_nullColliding.set(true);
        break;
      } else {
        m_nullColliding.set(false);
      }

      Vec2F correction = result.correction;
      Vec2F normCorrection = vnorm(correction);

      m_surfaceSlope = result.groundSlope;
      m_surfaceMovingCollision.set(result.surfaceMovingCollisionId);
      m_collisionCorrection += correction;
      m_colliding.set(correction != Vec2F() || result.isStuck);
      m_onGround.set(!zeroG() && result.onGround);
      m_collisionStuck.set(result.isStuck);

      // If we have collided, apply either sticky or normal (bouncing) collision physics
      if (correction != Vec2F()) {
        if ((*m_parameters.stickyCollision) && result.collisionKind != CollisionKind::Slippery) {
          // When sticking, cancel all velocity and apply stickyForce in the
          // opposite of the direction of collision correction.
          relativeVelocity = -normCorrection * (*m_parameters.stickyForce) / mass() * dtSteps;
          m_stickingDirection.set(-normCorrection.angle());
          break;
        } else {
          m_stickingDirection.set({});

          float correctionMagnitude = vmag(correction);
          Vec2F correctionDirection = correction / correctionMagnitude;

          if ((*m_parameters.bounceFactor) != 0.0f) {
            Vec2F adjustment = correctionDirection * (velocityMagnitude * (correctionDirection * -velocityDirection));
            relativeVelocity += adjustment + (*m_parameters.bounceFactor) * adjustment;
            if ((*m_parameters.stopOnFirstBounce)) {
              // When bouncing, stop integrating at the moment of bounce.  This
              // prevents the frame of contact from being missed due to multiple
              // iterations per frame.
              break;
            }
          } else {
            // Only adjust the velocity to the extent that the collision was
            // caused by the velocity in each axis, to eliminate collision
            // induced velocity in a platformery way (each axis considered
            // independently).

            if (relativeVelocity[0] < 0 && correction[0] > 0)
              relativeVelocity[0] = std::min(0.0f, relativeVelocity[0] + correction[0] / dt);
            else if (relativeVelocity[0] > 0 && correction[0] < 0)
              relativeVelocity[0] = std::max(0.0f, relativeVelocity[0] + correction[0] / dt);

            if (relativeVelocity[1] < 0 && correction[1] > 0)
              relativeVelocity[1] = std::min(0.0f, relativeVelocity[1] + correction[1] / dt);
            else if (relativeVelocity[1] > 0 && correction[1] < 0)
              relativeVelocity[1] = std::max(0.0f, relativeVelocity[1] + correction[1] / dt);
          }
        }
      }
    }
  }
  
  if (!m_colliding.get())
    m_stickingDirection.set({});
  
  Vec2F newVelocity = relativeVelocity + m_surfaceVelocity;

  PolyF body = collisionBody();

  if (auto result = updateLiquidPercentage(); !result) return std::unexpected(std::move(result.error()));

  if (auto movingCollisionId = m_surfaceMovingCollision.get()) {
    if (auto physicsEntity = worldRef.get<PhysicsEntity>(movingCollisionId->physicsEntityId))
      {
      auto collision = physicsEntity->movingCollision(movingCollisionId->collisionIndex);
      if (!collision) return std::unexpected(std::move(collision.error()));
      surfaceCollision = std::move(*collision);
    }
  }

  if (surfaceCollision) {
    m_surfaceMovingCollisionPosition = surfaceCollision->position;
    m_xRelativeSurfaceMovingCollisionPosition.set(geometry.diff(xPosition(), m_surfaceMovingCollisionPosition[0]));
    m_yRelativeSurfaceMovingCollisionPosition.set(yPosition() - m_surfaceMovingCollisionPosition[1]);
  } else {
    m_surfaceMovingCollisionPosition = {};
    m_surfaceVelocity = {};
  }
  
  // In order to make control work accurately, passive forces need to be
  // applied to velocity *after* integrating.  This prevents control from
  // having to account for one timestep of passive forces in order to result
  // in the correct controlled movement.
  if (!zeroG() && !stickingDirection()) {
    float buoyancy = (*m_parameters.liquidBuoyancy) * m_liquidPercentage + (*m_parameters.airBuoyancy) * (1.0f - liquidPercentage());
    auto gravityValue = worldRef.gravity(position());
    if (!gravityValue) return std::unexpected(std::move(gravityValue.error()));
    float gravity = *gravityValue * (*m_parameters.gravityMultiplier) * (1.0f - buoyancy);
    Vec2F environmentVelocity;
    environmentVelocity[1] -= gravity * dt;

    if (onGround() && (*m_parameters.slopeSlidingFactor) != 0 && m_surfaceSlope[1] != 0)
      environmentVelocity += -m_surfaceSlope * (m_surfaceSlope[0] * m_surfaceSlope[1]) * (*m_parameters.slopeSlidingFactor);

    newVelocity += environmentVelocity;
  }

  // If original movement was entirely (almost) in the direction of gravity
  // and was entirely (almost) cancelled by collision correction, put the
  // entity into rest for restDuration
  if (!m_resting && !stickingDirection() && std::abs(originalMovement[0]) < 0.0001) {
    auto restGravity = gravity();
    if (!restGravity) return std::unexpected(std::move(restGravity.error()));
    if (originalMovement[1] * *restGravity <= 0.0 && std::abs(originalMovement[1] + m_collisionCorrection[1]) < 0.0001) {
      m_resting = true;
      m_restTicks = m_parameters.restDuration.value_or(0);
    }
  }

  if ((*m_parameters.frictionEnabled)) {
    Vec2F refVel;
    float friction = liquidPercentage() * (*m_parameters.liquidFriction) + (1.0f - liquidPercentage()) * (*m_parameters.airFriction);
    if (onGround()) {
      friction = std::max(friction, (*m_parameters.groundFriction));
      refVel = m_surfaceVelocity;
    }

    // The equation for friction here is effectively:
    // frictionForce = friction * (refVel - velocity)
    // but it is applied here as a multiplicitave factor from [0, 1] so it does
    // not induce oscillation at very high friction and so it cannot be
    // negative.
    float frictionFactor = std::clamp(friction / mass() * dt, 0.0f, 1.0f);
    newVelocity = lerp(frictionFactor, newVelocity, refVel);
  }
  
  setVelocity(newVelocity);

  if (auto result = updateForceRegions(dt); !result) return std::unexpected(std::move(result.error()));
  return {};
}

std::expected<void, StarException> MovementController::tickSlave(float dt) {
  auto activeWorld = world();
  if (!activeWorld) return std::unexpected(std::move(activeWorld.error()));
  World& worldRef = activeWorld->get();
  setTimestep(dt);
  if (auto movingCollisionId = m_surfaceMovingCollision.get()) {
    if (auto physicsEntity = worldRef.get<PhysicsEntity>(movingCollisionId->physicsEntityId)) {
      auto collisionResult = physicsEntity->movingCollision(movingCollisionId->collisionIndex);
      if (!collisionResult) return std::unexpected(std::move(collisionResult.error()));
      if (auto collision = *collisionResult) {
        m_xPosition.set(m_xRelativeSurfaceMovingCollisionPosition.get() + collision->position[0]);
        m_yPosition.set(m_yRelativeSurfaceMovingCollisionPosition.get() + collision->position[1]);
      }
    }
  }

  auto liquid = worldRef.liquidLevel(collisionBody().bounding_box());
  if (!liquid) return std::unexpected(std::move(liquid.error()));
  LiquidLevel cll = *liquid;
  m_liquidPercentage = std::clamp(cll.level, 0.0f, 1.0f);
  m_liquidId = cll.liquid;
  return {};
}

void MovementController::setIgnorePhysicsEntities(Set<EntityId> ignorePhysicsEntities) {
  m_ignorePhysicsEntities = std::move(ignorePhysicsEntities);
}

std::expected<void, StarException> MovementController::forEachMovingCollision(RectF const& region, std::function<std::expected<bool, StarException>(MovingCollisionId id, PhysicsMovingCollision, PolyF, RectF)> callback) {
  auto activeWorld = world();
  if (!activeWorld) return std::unexpected(std::move(activeWorld.error()));
  World& worldRef = activeWorld->get();
  auto geometry = worldRef.geometry();
  auto physicsEntities = worldRef.query<PhysicsEntity>(region);
  if (!physicsEntities) return std::unexpected(std::move(physicsEntities.error()));
  for (auto& physicsEntity : *physicsEntities) {
    if (m_ignorePhysicsEntities.contains(physicsEntity->entityId()))
      continue;
    for (std::size_t i = 0; i < physicsEntity->movingCollisionCount(); ++i) {
      auto collisionResult = physicsEntity->movingCollision(i);
      if (!collisionResult) return std::unexpected(std::move(collisionResult.error()));
      if (auto mc = *collisionResult) {
        if (mc->categoryFilter.check(m_parameters.physicsEffectCategories.value_or(StringSet{}))) {
          PolyF poly = std::move(mc->collision);
          poly.translate(geometry.nearest_to(region.min(), mc->position));
          RectF polyBounds = poly.bounding_box();

          if (region.intersects(polyBounds)) {
            // early exit if the callback returns false
            auto keepGoing = callback({physicsEntity->entityId(), i}, *mc, poly, polyBounds);
            if (!keepGoing) return std::unexpected(std::move(keepGoing.error()));
            if (!*keepGoing) return {};
          }
        }
      }
    }
  }
  return {};
}

std::expected<void, StarException> MovementController::updateForceRegions(float) {
  auto activeWorld = world();
  if (!activeWorld) return std::unexpected(std::move(activeWorld.error()));
  World& worldRef = activeWorld->get();
  auto geometry = worldRef.geometry();
  auto pos = position();
  auto body = collisionBody();
  RectF boundBox = body.bounding_box();

  m_appliedForceRegion = false;
  auto handleForceRegions = [&, &controller = *this](List<PhysicsForceRegion> const& forces) -> std::expected<void, StarException> {
      for (auto const& force : forces) {
        bool categoryCheck = std::visit([myCategories = controller.m_parameters.physicsEffectCategories.value_or(StringSet{})](auto& fr) {
            return fr.categoryFilter.check(myCategories);
          }, force);
        if (!categoryCheck)
          continue;

        auto boundsCheck = std::visit([geometry, myBounds = controller.collisionBoundBox()](auto& fr) {
            return geometry.rect_intersects_rect(myBounds, fr.boundBox());
          }, force);
        if (!boundsCheck) return std::unexpected(std::move(boundsCheck.error()));
        if (!*boundsCheck)
          continue;

        controller.m_appliedForceRegion = true;
        if (std::holds_alternative<DirectionalForceRegion>(force)) {
          auto const& directionalForceRegion = std::get<DirectionalForceRegion>(force);
          auto overlap = geometry.poly_overlap_area(directionalForceRegion.region, body);
          if (!overlap) return std::unexpected(std::move(overlap.error()));
          float forceEffect = *overlap / body.signed_area();
          if (directionalForceRegion.xTargetVelocity)
            controller.approachXVelocity(*directionalForceRegion.xTargetVelocity, directionalForceRegion.controlForce * forceEffect);
          if (directionalForceRegion.yTargetVelocity)
            controller.approachYVelocity(*directionalForceRegion.yTargetVelocity, directionalForceRegion.controlForce * forceEffect);

        } else if (std::holds_alternative<RadialForceRegion>(force)) {
          auto const& radialForceRegion = std::get<RadialForceRegion>(force);
          Vec2F direction = geometry.diff(pos, radialForceRegion.center);
          float distance = vmag(direction);
          if (distance > 0 && distance < radialForceRegion.outerRadius) {
            float incidence = std::min(1.0f - (distance - radialForceRegion.innerRadius) / (radialForceRegion.outerRadius - radialForceRegion.innerRadius), distance / radialForceRegion.innerRadius);
            if (radialForceRegion.targetRadialVelocity < 0)
              direction = -direction;
            controller.approachVelocityAlongAngle(direction.angle(),
                std::abs(radialForceRegion.targetRadialVelocity),
                radialForceRegion.controlForce * incidence,
                true);
          }
        } else if (std::holds_alternative<GradientForceRegion>(force)) {
          auto const& gradientForceRegion = std::get<GradientForceRegion>(force);
          auto overlap = geometry.poly_overlap_area(gradientForceRegion.region, body);
          if (!overlap) return std::unexpected(std::move(overlap.error()));
          float overlapFactor = *overlap / body.signed_area();

          Vec2F gNorm = gradientForceRegion.gradient.direction();
          Vec2F pDiff = geometry.diff(pos, gradientForceRegion.gradient.min());
          float projected = pDiff[0] * gNorm[0] + pDiff[1] * gNorm[1];
          float gradientFactor = 1.0 - std::clamp(projected / gradientForceRegion.gradient.length(), -1.0f, 1.0f);

          controller.approachVelocityAlongAngle(gradientForceRegion.gradient.angle(),
              gradientForceRegion.baseTargetVelocity * overlapFactor * gradientFactor,
              gradientForceRegion.baseControlForce * overlapFactor * gradientFactor,
              true);
        }
      }
      return {};
    };

  auto physicsEntities = worldRef.query<PhysicsEntity>(boundBox);
  if (!physicsEntities) return std::unexpected(std::move(physicsEntities.error()));
  for (auto& physicsEntity : *physicsEntities) {
    if (m_ignorePhysicsEntities.contains(physicsEntity->entityId()))
      continue;

    auto forces = physicsEntity->forceRegions();
    if (!forces) return std::unexpected(std::move(forces.error()));
    if (auto result = handleForceRegions(*forces); !result) return std::unexpected(std::move(result.error()));
  }

  auto forces = worldRef.forceRegions();
  if (!forces) return std::unexpected(std::move(forces.error()));
  if (auto result = handleForceRegions(*forces); !result) return std::unexpected(std::move(result.error()));
  return {};
}

std::expected<void, StarException> MovementController::updateLiquidPercentage() {
  auto activeWorld = world();
  if (!activeWorld) return std::unexpected(std::move(activeWorld.error()));
  World& worldRef = activeWorld->get();
  auto pos = position();
  auto body = collisionBody();
  RectF boundBox = body.bounding_box();

  LiquidLevel cll;
  if (boundBox.is_empty())
    cll = worldRef.liquidLevel(Vec2I::floor(pos));
  else {
    auto liquid = worldRef.liquidLevel(boundBox);
    if (!liquid) return std::unexpected(std::move(liquid.error()));
    cll = *liquid;
  }

  m_liquidPercentage = std::clamp(cll.level, 0.0f, 1.0f);
  m_liquidId = cll.liquid;
  return {};
}

void MovementController::setOnGround(bool onGround) {
  m_onGround.set(onGround);
}

bool MovementController::appliedForceRegion() const {
  return m_appliedForceRegion;
}

Vec2F MovementController::collisionCorrection() const {
  return m_collisionCorrection;
}

Vec2F MovementController::surfaceSlope() const {
  return m_surfaceSlope;
}

Vec2F MovementController::surfaceVelocity() const {
  return m_surfaceVelocity;
}

std::expected<std::reference_wrapper<World>, StarException> MovementController::world() {
  if (!m_world)
    return std::unexpected(MovementControllerException("MovementController not initialized!"));
  return *m_world;
}

CollisionKind MovementController::maxOrNullCollision(CollisionKind a, CollisionKind b) {
  if (a == CollisionKind::Null || b == CollisionKind::Null)
    return CollisionKind::Null;
  else
    return std::max(a, b);
}

MovementController::CollisionResult MovementController::collisionMove(List<CollisionPoly>& collisionPolys, PolyF const& body, Vec2F const& movement,
    bool ignorePlatforms, bool enableSurfaceSlopeCorrection, float maximumCorrection, float maximumPlatformCorrection, Vec2F sortCenter, float dt) {
  unsigned const MaximumSeparationLoops = 3;
  float const SlideAngle = std::numbers::pi / 3;
  float const SlideCorrectionLimit = 0.2f;
  float separationTolerance = 0.001f * (dt * 60.f);
  maximumPlatformCorrection *= (dt * 60.f);

  if (body.empty())
    return {movement, Vec2F(), {}, false, false, Vec2F(1, 0), CollisionKind::None};

  // The center and candidate positions stay fixed through all separation passes.
  // Cache their distances, but not an unstable sort's permutation of equal keys.
  for (auto& cp : collisionPolys)
    cp.sortDistance = vmag_squared(cp.sortPosition - sortCenter);

  PolyF translatedBody = body;
  translatedBody.translate(movement);
  PolyF checkBody = translatedBody;
  Vec2F totalCorrection = Vec2F();
  CollisionKind maxCollided = CollisionKind::None;
  std::optional<MovingCollisionId> surfaceMovingCollisionId;

  CollisionSeparation separation = {};
  bool firstSeparation = true;
  std::optional<bool> repeatSort;
  auto separate = [&](bool platformsIgnored, bool upward) {
    bool sortRequired = std::exchange(firstSeparation, false);
    if (!sortRequired) {
      // Only inspect the sorted keys if a second pass is actually needed.
      // Strictly increasing keys have exactly one order. Ties and unordered
      // comparisons must retain every legacy sort, including its tie reshuffle:
      // contact corrections, moving-platform identity and ground slope use it.
      if (!repeatSort.has_value()) {
        repeatSort = std::adjacent_find(collisionPolys.begin(), collisionPolys.end(),
                                        [](auto const& a, auto const& b) { return !(a.sortDistance < b.sortDistance); })
          != collisionPolys.end();
      }
      sortRequired = *repeatSort;
    }
    if (sortRequired) {
      std::sort(collisionPolys.begin(), collisionPolys.end(),
                [](auto const& a, auto const& b) { return a.sortDistance < b.sortDistance; });
    }
    return collisionSeparate(collisionPolys, checkBody, platformsIgnored, maximumPlatformCorrection, upward,
                             separationTolerance);
  };

  if (enableSurfaceSlopeCorrection) {
    // First try separating with our ground sliding cheat.
    separation = separate(ignorePlatforms, true);
    totalCorrection += separation.correction;
    checkBody.translate(separation.correction);
    maxCollided = maxOrNullCollision(maxCollided, separation.collisionKind);
    surfaceMovingCollisionId = separation.movingCollisionId;
    Vec2F upwardResult = movement + separation.correction;
    float upMag = upwardResult.magnitude();
    // Angle off of horizontal (minimum of either direction)
    float angleHoriz = std::min(Vec2F(1, 0).angle_between_normalized(upwardResult / upMag),
                                Vec2F(-1, 0).angle_between_normalized(upwardResult / upMag));

    // We need to make sure that even if we found a solution with the sliding
    // cheat, we are not beyond the angle and correction limits for the ground
    // cheat correction.
    if (separation.solutionFound)
      separation.solutionFound = upMag < SlideCorrectionLimit || angleHoriz < SlideAngle;

    if (separation.solutionFound) {
      if (totalCorrection.magnitude() > maximumCorrection)
        separation.solutionFound = false;
    }
  }

  if (!separation.solutionFound) {
    checkBody = translatedBody;
    totalCorrection = Vec2F();
    for (std::size_t i = 0; i < MaximumSeparationLoops; ++i) {
      separation = separate(ignorePlatforms, false);
      totalCorrection += separation.correction;
      checkBody.translate(separation.correction);
      maxCollided = maxOrNullCollision(maxCollided, separation.collisionKind);
      surfaceMovingCollisionId = {};

      if (totalCorrection.magnitude() > maximumCorrection) {
        separation.solutionFound = false;
        break;
      }

      if (separation.solutionFound)
        break;
    }
  }

  if (!separation.solutionFound && movement != Vec2F()) {
    // No collision solution found! Move checkBody back to original body before
    // applying movement and try one last time to correct.
    checkBody = body;
    totalCorrection = -movement;
    for (std::size_t i = 0; i < MaximumSeparationLoops; ++i) {
      separation = separate(true, false);
      totalCorrection += separation.correction;
      checkBody.translate(separation.correction);
      maxCollided = maxOrNullCollision(maxCollided, separation.collisionKind);

      if (totalCorrection.magnitude() > maximumCorrection) {
        separation.solutionFound = false;
        break;
      }

      if (separation.solutionFound)
        break;
    }
  }

  if (separation.solutionFound) {
    CollisionResult result;
    result.movement = movement + totalCorrection;
    result.correction = totalCorrection;
    result.isStuck = false;
    result.onGround = result.correction[1] > separationTolerance;
    result.surfaceMovingCollisionId = surfaceMovingCollisionId;
    result.collisionKind = maxCollided;
    result.groundSlope = Vec2F(1, 0);
    if (result.onGround) {
      // If we are on the ground and need to find the ground slope, look for a
      // vertex on the body being moved that is touching an edge of one of the
      // collision polys.  We only want a slope to be produced from an edge of
      // colision geometry, not an edge of the colliding body.  Pick the
      // touching edge that is the most horizontally overlapped with the
      // geometry, rather than off to the side.
      float maxSideHorizontalOverlap = 0.0f;
      RectF touchingBounds = checkBody.bounding_box();
      touchingBounds.pad(separationTolerance);
      for (auto const& cp : collisionPolys) {
        if (!cp.polyBounds.intersects(touchingBounds))
          continue;

        for (std::size_t i = 0; i < cp.poly.sides(); ++i) {
          auto side = cp.poly.side(i);
          RectF sideBounds = RectF::bound_box_of(side.min(), side.max());
          float thisSideHorizontalOverlap = sideBounds.overlap(touchingBounds).width();

          if (thisSideHorizontalOverlap > maxSideHorizontalOverlap) {
            for (auto const& bodyVertex : checkBody) {
              float t = std::clamp(side.line_projection(bodyVertex), 0.0f, 1.0f);
              Vec2F nearPoint = side.eval(t);
              if (nearPoint[1] > cp.sortPosition[1]) {
                if (vmag_squared(bodyVertex - nearPoint) <= square(separationTolerance)) {
                  maxSideHorizontalOverlap = thisSideHorizontalOverlap;
                  result.groundSlope = side.diff().normalized();
                  if (result.groundSlope[0] < 0)
                    result.groundSlope *= -1;
                }
              }
            }
          }
        }
      }
    }

    return result;
  } else {
    return {Vec2F(), -movement, {}, true, true, Vec2F(1, 0), maxCollided};
  }
}

MovementController::CollisionSeparation MovementController::collisionSeparate(List<CollisionPoly> const& collisionPolys,
                                                                              PolyF const& poly, bool ignorePlatforms,
                                                                              float maximumPlatformCorrection,
                                                                              bool upward, float separationTolerance) {

  CollisionSeparation separation = {};
  separation.collisionKind = CollisionKind::None;
  bool intersects = false;

  PolyF::IntersectResult intersectResult;
  PolyF correctedPoly = poly;
  RectF correctedBoundBox = correctedPoly.bounding_box();
  for (auto const& cp : collisionPolys) {
    if ((ignorePlatforms && cp.collisionKind == CollisionKind::Platform) || !correctedBoundBox.intersects(cp.polyBounds, false))
      continue;

    if (upward)
      intersectResult = correctedPoly.directional_sat_intersection(cp.poly, Vec2F(0, 1), false);
    else if (cp.collisionKind == CollisionKind::Platform)
      intersectResult = correctedPoly.directional_sat_intersection(cp.poly, Vec2F(0, 1), true);
    else
      intersectResult = correctedPoly.sat_intersection(cp.poly);

    if (cp.collisionKind == CollisionKind::Platform && intersectResult.intersects) {
      if (intersectResult.overlap[1] <= 0 || intersectResult.overlap[1] > maximumPlatformCorrection)
        intersectResult.intersects = false;
    }

    if (intersectResult.intersects) {
      intersects = true;
      correctedPoly.translate(intersectResult.overlap);
      correctedBoundBox = correctedPoly.bounding_box();
      separation.correction += intersectResult.overlap;
      if (cp.movingCollisionId)
        separation.movingCollisionId = cp.movingCollisionId;
      separation.collisionKind = maxOrNullCollision(separation.collisionKind, cp.collisionKind);
    }
  }

  separation.solutionFound = true;
  float separationToleranceSquared = square(separationTolerance);
  if (intersects) {
    for (auto const& cp : collisionPolys) {
      if (cp.collisionKind == CollisionKind::Platform || !correctedBoundBox.intersects(cp.polyBounds, false))
        continue;

      intersectResult = correctedPoly.sat_intersection(cp.poly);
      if (intersectResult.intersects && intersectResult.overlap.magnitude_squared() > separationToleranceSquared) {
        separation.collisionKind = maxOrNullCollision(separation.collisionKind, cp.collisionKind);
        separation.solutionFound = false;
        break;
      }
    }
  }

  return separation;
}

std::expected<void, StarException> MovementController::updateParameters(MovementParameters parameters) {
  if (!parameters.speedLimit) return std::unexpected(MovementControllerException("Missing movement parameter 'speedLimit'"));
  if (!parameters.gravityEnabled) return std::unexpected(MovementControllerException("Missing movement parameter 'gravityEnabled'"));
  if (!parameters.gravityMultiplier) return std::unexpected(MovementControllerException("Missing movement parameter 'gravityMultiplier'"));
  if (!parameters.maxMovementPerStep) return std::unexpected(MovementControllerException("Missing movement parameter 'maxMovementPerStep'"));
  if (!parameters.collisionEnabled) return std::unexpected(MovementControllerException("Missing movement parameter 'collisionEnabled'"));
  if (!parameters.ignorePlatformCollision) return std::unexpected(MovementControllerException("Missing movement parameter 'ignorePlatformCollision'"));
  if (!parameters.maximumCorrection) return std::unexpected(MovementControllerException("Missing movement parameter 'maximumCorrection'"));
  if (!parameters.maximumPlatformCorrection) return std::unexpected(MovementControllerException("Missing movement parameter 'maximumPlatformCorrection'"));
  if (!parameters.maximumPlatformCorrectionVelocityFactor) return std::unexpected(MovementControllerException("Missing movement parameter 'maximumPlatformCorrectionVelocityFactor'"));
  if (!parameters.enableSurfaceSlopeCorrection) return std::unexpected(MovementControllerException("Missing movement parameter 'enableSurfaceSlopeCorrection'"));
  if (!parameters.stickyCollision) return std::unexpected(MovementControllerException("Missing movement parameter 'stickyCollision'"));
  if (!parameters.stickyForce) return std::unexpected(MovementControllerException("Missing movement parameter 'stickyForce'"));
  if (!parameters.bounceFactor) return std::unexpected(MovementControllerException("Missing movement parameter 'bounceFactor'"));
  if (!parameters.stopOnFirstBounce) return std::unexpected(MovementControllerException("Missing movement parameter 'stopOnFirstBounce'"));
  if (!parameters.liquidBuoyancy) return std::unexpected(MovementControllerException("Missing movement parameter 'liquidBuoyancy'"));
  if (!parameters.airBuoyancy) return std::unexpected(MovementControllerException("Missing movement parameter 'airBuoyancy'"));
  if (!parameters.slopeSlidingFactor) return std::unexpected(MovementControllerException("Missing movement parameter 'slopeSlidingFactor'"));
  if (!parameters.frictionEnabled) return std::unexpected(MovementControllerException("Missing movement parameter 'frictionEnabled'"));
  if (!parameters.liquidFriction) return std::unexpected(MovementControllerException("Missing movement parameter 'liquidFriction'"));
  if (!parameters.airFriction) return std::unexpected(MovementControllerException("Missing movement parameter 'airFriction'"));
  if (!parameters.groundFriction) return std::unexpected(MovementControllerException("Missing movement parameter 'groundFriction'"));
  if (!parameters.collisionPoly) return std::unexpected(MovementControllerException("Missing movement parameter 'collisionPoly'"));
  if (!parameters.mass) return std::unexpected(MovementControllerException("Missing movement parameter 'mass'"));
  m_parameters = std::move(parameters);
  m_collisionPoly.set((*m_parameters.collisionPoly));
  m_mass.set((*m_parameters.mass));
  updatePositionInterpolators();
  return {};
}

void MovementController::updatePositionInterpolators() {
  if (m_world)
    m_xPosition.set_interpolator(m_world->get().geometry().x_lerp_function(m_parameters.discontinuityThreshold));
  else
    m_xPosition.set_interpolator(
      [limit = m_parameters.discontinuityThreshold](float const& offset, float const& from, float const& to) {
        return lerp_with_limit<float, float>(limit, offset, from, to);
      });
  m_yPosition.set_interpolator(
    [limit = m_parameters.discontinuityThreshold](float const& offset, float const& from, float const& to) {
      return lerp_with_limit<float, float>(limit, offset, from, to);
    });
}

std::expected<void, StarException> MovementController::queryCollisions(RectF const& region) {
  auto activeWorld = world();
  if (!activeWorld) return std::unexpected(std::move(activeWorld.error()));
  World& worldRef = activeWorld->get();
  while (!m_workingCollisions.empty()) {
    m_collisionBuffers.push_back(std::move(m_workingCollisions.back().poly));
    m_workingCollisions.pop_back();
  }

  auto newCollisionPoly = [&collisionBuffers = m_collisionBuffers, &workingCollisions = m_workingCollisions]() -> CollisionPoly& {
    if (!collisionBuffers.empty()) {
      auto poly = std::move(collisionBuffers.back());
      collisionBuffers.pop_back();
      return workingCollisions.emplace_back(CollisionPoly{std::move(poly), {}, {}, {}, {}, {}});
    }
    else
      return workingCollisions.emplace_back(CollisionPoly{});
  };

  auto geometry = worldRef.geometry();

  auto blocks = worldRef.forEachCollisionBlock(RectI::integral(region.padded(1)), [&](CollisionBlock const& block) -> std::expected<void, StarException> {
      if (block.kind != CollisionKind::None && !block.poly.empty()) {
        RectF polyBounds = block.polyBounds;
        Vec2F basePosition = block.poly.vertex(0);
        Vec2F nearTranslation = geometry.nearest_to(region.min(), basePosition) - basePosition;
        polyBounds.translate(nearTranslation);

        if (region.intersects(polyBounds)) {
          CollisionPoly& collisionPoly = newCollisionPoly();
          collisionPoly.poly = block.poly;
          collisionPoly.poly.translate(nearTranslation);
          collisionPoly.polyBounds = polyBounds;
          collisionPoly.sortPosition = center_of_tile(block.space);
          collisionPoly.movingCollisionId = {};
          collisionPoly.collisionKind = block.kind;
        }
      }
      return {};
    });
  if (!blocks) return std::unexpected(std::move(blocks.error()));

  auto moving = forEachMovingCollision(region, [&](MovingCollisionId id, PhysicsMovingCollision mc, PolyF poly, RectF bounds) -> std::expected<bool, StarException> {
    CollisionPoly& collisionPoly = newCollisionPoly();
    collisionPoly.poly = std::move(poly);
    collisionPoly.polyBounds = bounds;
    collisionPoly.sortPosition = collisionPoly.poly.center();
    collisionPoly.movingCollisionId = id;
    collisionPoly.collisionKind = mc.collisionKind;
    return true;
  });
  if (!moving) return std::unexpected(std::move(moving.error()));
  return {};
}

std::expected<float, StarException> MovementController::gravity() {
  auto activeWorld = world();
  if (!activeWorld) return std::unexpected(std::move(activeWorld.error()));
  World& worldRef = activeWorld->get();
  float buoyancy = (*m_parameters.liquidBuoyancy) * m_liquidPercentage + (*m_parameters.airBuoyancy) * (1.0f - liquidPercentage());
  auto gravityValue = worldRef.gravity(position());
  if (!gravityValue) return std::unexpected(std::move(gravityValue.error()));
  return *gravityValue * (*m_parameters.gravityMultiplier) * (1.0f - buoyancy);
}

}

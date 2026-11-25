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
  auto categories_config = config.opt("physicsEffectCategories");
  if (!categories_config) return std::unexpected(std::move(categories_config.error()));
  if (*categories_config) {
    auto categories = json_to_string_set(**categories_config);
    if (!categories) return std::unexpected(std::move(categories.error()));
    value.physics_effect_categories = std::move(*categories);
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
  merged.physics_effect_categories = rhs.physics_effect_categories ? rhs.physics_effect_categories : physics_effect_categories;
  merged.restDuration = (rhs.restDuration ? rhs.restDuration : restDuration);

  return merged;
}

std::expected<Json, StarException> MovementParameters::toJson() const {
  auto collisionPolyJson = json_from_optional(collisionPoly, json_from_polyf);
  if (!collisionPolyJson) return std::unexpected(std::move(collisionPolyJson.error()));
  auto categories_json = json_from_optional(physics_effect_categories, json_from_string_set);
  if (!categories_json) return std::unexpected(std::move(categories_json.error()));

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
    {"physicsEffectCategories", *categories_json},
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
  if (auto result = ds.read(movementParameters.physics_effect_categories); !result) return std::unexpected(std::move(result.error()));
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
  if (auto result = ds.write(movementParameters.physics_effect_categories); !result) return std::unexpected(std::move(result.error()));
  if (auto result = ds.write(movementParameters.restDuration); !result) return std::unexpected(std::move(result.error()));

  return std::ref(ds);
}

MovementController::MovementController(Root& root) : m_root(root) {
  m_resting = false;

  m_timeStep = root.timing().time_step;

  m_liquidPercentage = 0.0f;
  m_liquidId = empty_liquid_id;

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
      if (auto result = query_collisions(queryBounds); !result) return std::unexpected(std::move(result.error()));
      auto result = collision_move(m_working_collisions, body, movement, ignorePlatforms, (*m_parameters.enableSurfaceSlopeCorrection) && !zeroG(),
          maximumCorrection, maximumPlatformCorrection, bodyCenter, dtSteps);

      setPosition(position() + result.movement);

      if (result.collision_kind == CollisionKind::Null) {
        m_nullColliding.set(true);
        break;
      } else {
        m_nullColliding.set(false);
      }

      Vec2F correction = result.correction;
      Vec2F normCorrection = vnorm(correction);

      m_surfaceSlope = result.ground_slope;
      m_surfaceMovingCollision.set(result.surface_moving_collision_id);
      m_collisionCorrection += correction;
      m_colliding.set(correction != Vec2F() || result.is_stuck);
      m_onGround.set(!zeroG() && result.on_ground);
      m_collisionStuck.set(result.is_stuck);

      // If we have collided, apply either sticky or normal (bouncing) collision physics
      if (correction != Vec2F()) {
        if ((*m_parameters.stickyCollision) && result.collision_kind != CollisionKind::Slippery) {
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

  if (auto result = update_force_regions(); !result) return std::unexpected(std::move(result.error()));
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

std::expected<void, StarException> MovementController::for_each_moving_collision(RectF const& region, MovingCollisionCallback const& callback) {
  auto active_world = world();
  if (!active_world) return std::unexpected(std::move(active_world.error()));
  World& world_ref = active_world->get();
  auto geometry = world_ref.geometry();
  auto physics_entities = world_ref.query<PhysicsEntity>(region);
  if (!physics_entities) return std::unexpected(std::move(physics_entities.error()));
  static StringSet const empty_categories;
  for (auto const& entity : *physics_entities) {
    if (m_ignorePhysicsEntities.contains(entity->entityId()))
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
  auto const geometry = world_ref.geometry();
  auto const pos = position();
  auto const body = collisionBody();
  // An empty collision polygon still has a position for point-based radial forces.
  auto const bounds = body.empty() ? RectF(pos, pos) : body.bounding_box();
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
      approachVelocityAlongAngle(direction.angle(), std::abs(region.target_radial_velocity), control, true);
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
          approachXVelocity(*region.x_target_velocity, control);
        if (region.y_target_velocity)
          approachYVelocity(*region.y_target_velocity, control);
      } else {
        if (!(region.gradient.length() > 0))
          return false;
        float falloff = gradient_falloff(region.gradient, geometry.diff(pos, region.gradient.min()));
        float effect = (*overlap * falloff) / *body_area;
        float control = region.base_control_force * effect;
        if (!(control > 0))
          return false;
        approachVelocityAlongAngle(region.gradient.angle(), region.base_target_velocity * effect, control, true);
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
    if (m_ignorePhysicsEntities.contains(entity->entityId()))
      continue;
    auto forces = entity->force_regions();
    if (!forces) return std::unexpected(std::move(forces.error()));
    if (auto result = apply_forces(*forces); !result) return std::unexpected(std::move(result.error()));
  }
  return apply_forces(world_ref.force_regions());
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

bool MovementController::applied_force_region() const {
  return m_applied_force_region;
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
  bool const refresh_interpolators = !m_parameters.mass || m_parameters.discontinuityThreshold != parameters.discontinuityThreshold;
  m_parameters = std::move(parameters);
  m_collisionPoly.set((*m_parameters.collisionPoly));
  m_mass.set((*m_parameters.mass));
  if (refresh_interpolators)
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

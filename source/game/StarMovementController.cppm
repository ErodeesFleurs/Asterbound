module;
import std;
import star.string;
import star.set;
import star.format;
import star.json;
import star.id_map;
import star.poly;
import star.game_types;
import star.liquid_types;
import star.casting;
import star.vector;
import star.data_stream;
import star.strong_typedef;
import star.bi_map;
import star.list;

import star.damage_types;
import star.world_geometry;
import star.status_types;
import star.damage;
import star.light_source;
import star.entity;
import star.collision_block;
import star.force_regions;
import star.physics_entity;

import star.net_element_basic_fields;
import star.net_element_float_fields;
import star.net_element_group;

import star.rect;
namespace Star {

class World;
class Root;

struct MovementControllerExceptionTag {
  static constexpr std::string_view name() { return "MovementControllerException"; }
};
using MovementControllerException = StarError<MovementControllerExceptionTag, StarException>;

// List of all movement parameters that define a specific sort of movable
// object.  Each parameter is optional so that this structure can be used to
// selectively merge a specific set of parameters on top of another.
struct MovementParameters {
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, MovementParameters& value);
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, MovementParameters const& value);
  // Load sensible defaults from a config file.
  [[nodiscard]] static std::expected<MovementParameters, StarException> sensibleDefaults(Root& root);

  // Construct parameters from config with only those specified in the config
  // set, if any.
  MovementParameters();
  [[nodiscard]] static std::expected<MovementParameters, StarException> create(Json const& config);

  // Merge the given set of movement parameters on top of this one, with any
  // set parameters in rhs overwriting the ones in this set.
  MovementParameters merge(MovementParameters const& rhs) const;

  [[nodiscard]] std::expected<Json, StarException> toJson() const;

  std::optional<float> mass;
  std::optional<float> gravityMultiplier;
  std::optional<float> liquidBuoyancy;
  std::optional<float> airBuoyancy;
  std::optional<float> bounceFactor;
  // If set to true, during an update that has more than one internal movement
  // step, the movement will stop on the first bounce.
  std::optional<bool> stopOnFirstBounce;
  // Cheat when sliding on the ground, by trying to correct upwards before
  // other directions (within a set limit).  Allows smooth sliding along
  // horizontal ground without losing horizontal speed.
  std::optional<bool> enableSurfaceSlopeCorrection;
  std::optional<float> slopeSlidingFactor;
  std::optional<float> maxMovementPerStep;
  std::optional<float> maximumCorrection;
  std::optional<float> speedLimit;
  std::optional<float> discontinuityThreshold;

  std::optional<PolyF> collisionPoly;

  std::optional<bool> stickyCollision;
  std::optional<float> stickyForce;

  std::optional<float> airFriction;
  std::optional<float> liquidFriction;
  std::optional<float> groundFriction;

  std::optional<bool> collisionEnabled;
  std::optional<bool> frictionEnabled;
  std::optional<bool> gravityEnabled;

  std::optional<bool> ignorePlatformCollision;
  std::optional<float> maximumPlatformCorrection;
  std::optional<float> maximumPlatformCorrectionVelocityFactor;

  std::optional<StringSet> physicsEffectCategories;

  std::optional<int> restDuration;
};

[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, MovementParameters& movementParameters);
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, MovementParameters const& movementParameters);

class MovementController : public NetElementGroup {
public:
  // Constructs a MovementController with parameters loaded from sensible
  // defaults, and the given parameters (if any) applied on top of them.
  [[nodiscard]] static std::expected<std::shared_ptr<MovementController>, StarException> create(Root& root, MovementParameters const& parameters = {});

  MovementParameters const& parameters() const;

  // Apply any set parameters from the given set on top of the current set.
  [[nodiscard]] std::expected<void, StarException> applyParameters(MovementParameters const& parameters);

  // Reset the parameters from the sensible defaults, and apply the given
  // parameters (if any) on top of them.
  [[nodiscard]] std::expected<void, StarException> resetParameters(MovementParameters const& parameters = MovementParameters());

  // Stores and loads position, velocity, and rotation.
  Json storeState() const;
  [[nodiscard]] std::expected<void, StarException> loadState(Json const& state);

  // Currently active mass parameter
  float mass() const;

  // Currently active collisionPoly parameter
  PolyF const& collisionPoly() const;
  void setCollisionPoly(PolyF const& poly);

  virtual Vec2F position() const;
  float xPosition() const;
  float yPosition() const;

  Vec2F velocity() const;
  float xVelocity() const;
  float yVelocity() const;

  virtual float rotation() const;

  // CollisionPoly rotated and translated by position
  PolyF collisionBody() const;

  // Gets the bounding box of the collisionPoly() rotated by current rotation,
  // but not translated into world space
  RectF localBoundBox() const;

  // Shorthand for getting the bound box of the current collisionBody()
  RectF collisionBoundBox() const;

  // Is the collision body colliding with any collision geometry.
  bool isColliding() const;
  // Is the collision body colliding with special "Null" collision blocks.
  bool isNullColliding() const;

  // Is the body currently stuck in an un-solvable collision.
  bool isCollisionStuck() const;

  // If this body is sticking, this is the angle toward the surface it's stuck to
  std::optional<float> stickingDirection() const;

  // From 0.0 to 1.0, the amount of the collision body (or if the collision
  // body is null, just the center position) that is in liquid.
  float liquidPercentage() const;

  // Returns the liquid that the body is most in, if any
  LiquidId liquidId() const;

  bool onGround() const;
  bool zeroG() const;

  bool atWorldLimit(bool bottomOnly = false) const;

  void setPosition(Vec2F position);
  // Explicit presentation discontinuities are separate from physics integration,
  // which also uses setPosition internally.
  void resetPresentation() { ++m_presentationEpoch; }
  std::uint64_t presentationEpoch() const { return m_presentationEpoch; }
  void setXPosition(float xPosition);
  void setYPosition(float yPosition);

  void translate(Vec2F const& direction);

  void setVelocity(Vec2F velocity);
  void setXVelocity(float xVelocity);
  void setYVelocity(float yVelocity);

  void addMomentum(Vec2F const& momentum);

  void setRotation(float angle);

  // Apply one timestep of rotation.
  void rotate(float rotationRate);

  // Apply one timestep of acceleration.
  void accelerate(Vec2F const& acceleration);

  // Apply one timestep of force.
  void force(Vec2F const& force);

  // Apply up to the maxControlForce of force to approach the given velocity.
  void approachVelocity(Vec2F const& targetVelocity, float maxControlForce);

  // Approach a velocity in the given angle, ignoring the component of velocity
  // normal to that angle.  If positiveOnly is true, then only approaches the
  // velocity by applying force in the direction of the given angle, never
  // opposite it, so avoids slowing down.
  void approachVelocityAlongAngle(float angle, float targetVelocity, float maxControlForce, bool positiveOnly = false);

  // Shorthand for approachVelocityAlongAngle with 0 and pi/2.
  void approachXVelocity(float targetXVelocity, float maxControlForce);
  void approachYVelocity(float targetYVelocity, float maxControlForce);

  void init(World& world);
  virtual void uninit();

  // Stores dt value for Lua calls.
  void setTimestep(float dt);

  // Integrates the ActorMovementController for dt and applies all
  // forces.
  [[nodiscard]] std::expected<void, StarException> tickMaster(float dt);

  // Does not integrate, only tracks master state and updates non-networked
  // fields based on local data
  [[nodiscard]] std::expected<void, StarException> tickSlave(float dt);

  void setIgnorePhysicsEntities(Set<EntityId> ignorePhysicsEntities);
  // iterate over all physics entity collision polys in the region, iteration stops if the callback returns false
  [[nodiscard]] std::expected<void, StarException> forEachMovingCollision(RectF const& region, std::function<std::expected<bool, StarException>(MovingCollisionId, PhysicsMovingCollision, PolyF, RectF)> callback);

protected:
  // forces the movement controller onGround status, used when manually controlling movement outside the movement controller
  [[nodiscard]] std::expected<void, StarException> updateForceRegions(float dt);
  [[nodiscard]] std::expected<void, StarException> updateLiquidPercentage();
  void setOnGround(bool onGround);

  // whether force regions were applied in the last update
  bool appliedForceRegion() const;
  // The collision correction applied during the most recent update, if any.
  Vec2F collisionCorrection() const;
  // Horizontal slope of the ground the collision body has collided with, if
  // any.
  Vec2F surfaceSlope() const;
  // Velocity of the surface that the body is resting on, if any
  Vec2F surfaceVelocity() const;

  [[nodiscard]] std::expected<std::reference_wrapper<World>, StarException> world();

protected:
  explicit MovementController(Root& root);
  [[nodiscard]] std::expected<void, StarException> initialize(MovementParameters const& parameters);
  Root& root() const;

private:
  // The collision benchmark invokes the production solver without a substitute world.
  friend struct MovementCollisionBenchmarkAccess;

  struct CollisionResult {
    Vec2F movement;
    Vec2F correction;
    std::optional<MovingCollisionId> surfaceMovingCollisionId;
    bool isStuck = false;
    bool onGround = false;
    Vec2F groundSlope;
    CollisionKind collisionKind = CollisionKind::Null;
  };

  struct CollisionSeparation {
    Vec2F correction;
    bool solutionFound = false;
    std::optional<MovingCollisionId> movingCollisionId;
    CollisionKind collisionKind = CollisionKind::Null;
  };

  struct CollisionPoly {
    PolyF poly;
    RectF polyBounds;
    Vec2F sortPosition;
    std::optional<MovingCollisionId> movingCollisionId;
    CollisionKind collisionKind = CollisionKind::Null;
    float sortDistance;
  };

  static CollisionKind maxOrNullCollision(CollisionKind a, CollisionKind b);
  static CollisionResult collisionMove(List<CollisionPoly>& collisionPolys, PolyF const& body, Vec2F const& movement,
      bool ignorePlatforms, bool enableSurfaceSlopeCorrection, float maximumCorrection, float maximumPlatformCorrection, Vec2F sortCenter, float dt);
  static CollisionSeparation collisionSeparate(List<CollisionPoly> const& collisionPolys, PolyF const& poly,
                                               bool ignorePlatforms, float maximumPlatformCorrection, bool upward,
                                               float separationTolerance);

  [[nodiscard]] std::expected<void, StarException> updateParameters(MovementParameters parameters);
  void updatePositionInterpolators();

  [[nodiscard]] std::expected<void, StarException> queryCollisions(RectF const& region);

  [[nodiscard]] std::expected<float, StarException> gravity();

  MovementParameters m_parameters;
  Root& m_root;

  std::optional<std::reference_wrapper<World>> m_world;

  Set<EntityId> m_ignorePhysicsEntities;

  NetElementData<PolyF> m_collisionPoly;
  NetElementFloat m_mass;
  NetElementFloat m_xPosition;
  NetElementFloat m_yPosition;
  NetElementFloat m_xVelocity;
  NetElementFloat m_yVelocity;
  NetElementFloat m_rotation;

  NetElementBool m_colliding;
  NetElementBool m_collisionStuck;
  NetElementBool m_nullColliding;
  NetElementData<std::optional<float>> m_stickingDirection;
  NetElementBool m_onGround;
  NetElementBool m_zeroG;

  float m_liquidPercentage;
  LiquidId m_liquidId;

  NetElementData<std::optional<MovingCollisionId>> m_surfaceMovingCollision;
  NetElementFloat m_xRelativeSurfaceMovingCollisionPosition;
  NetElementFloat m_yRelativeSurfaceMovingCollisionPosition;

  bool m_appliedForceRegion = false;
  Vec2F m_collisionCorrection;
  Vec2F m_surfaceSlope;
  Vec2F m_surfaceMovingCollisionPosition;
  Vec2F m_surfaceVelocity;
  Vec2F m_environmentVelocity;

  bool m_resting;
  int m_restTicks;
  float m_timeStep;
  std::uint64_t m_presentationEpoch = 0;

  List<CollisionPoly> m_workingCollisions;
  List<PolyF> m_collisionBuffers;
};

}

export module star.movement_controller;

export namespace Star {
  using ::Star::MovementControllerExceptionTag;
  using ::Star::MovementControllerException;
  using ::Star::MovementParameters;
  using ::Star::MovementController;
}

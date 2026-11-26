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
  [[nodiscard]] static std::expected<MovementParameters, StarException> sensible_defaults(Root& root);

  // Construct parameters from config with only those specified in the config
  // set, if any.
  MovementParameters();
  [[nodiscard]] static std::expected<MovementParameters, StarException> create(Json const& config);

  // Merge the given set of movement parameters on top of this one, with any
  // set parameters in rhs overwriting the ones in this set.
  MovementParameters merge(MovementParameters const& rhs) const;

  [[nodiscard]] std::expected<Json, StarException> to_json() const;

  std::optional<float> mass;
  std::optional<float> gravity_multiplier;
  std::optional<float> liquid_buoyancy;
  std::optional<float> air_buoyancy;
  std::optional<float> bounce_factor;
  // If set to true, during an update that has more than one internal movement
  // step, the movement will stop on the first bounce.
  std::optional<bool> stop_on_first_bounce;
  // Cheat when sliding on the ground, by trying to correct upwards before
  // other directions (within a set limit).  Allows smooth sliding along
  // horizontal ground without losing horizontal speed.
  std::optional<bool> enable_surface_slope_correction;
  std::optional<float> slope_sliding_factor;
  std::optional<float> max_movement_per_step;
  std::optional<float> maximum_correction;
  std::optional<float> speed_limit;
  std::optional<float> discontinuity_threshold;

  std::optional<PolyF> collision_poly;

  std::optional<bool> sticky_collision;
  std::optional<float> sticky_force;

  std::optional<float> air_friction;
  std::optional<float> liquid_friction;
  std::optional<float> ground_friction;

  std::optional<bool> collision_enabled;
  std::optional<bool> friction_enabled;
  std::optional<bool> gravity_enabled;

  std::optional<bool> ignore_platform_collision;
  std::optional<float> maximum_platform_correction;
  std::optional<float> maximum_platform_correction_velocity_factor;

  std::optional<StringSet> physics_effect_categories;

  std::optional<int> rest_duration;
};

[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, MovementParameters& movement_parameters);
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, MovementParameters const& movement_parameters);

class MovementController : public NetElementGroup {
public:
  // Constructs a MovementController with parameters loaded from sensible
  // defaults, and the given parameters (if any) applied on top of them.
  [[nodiscard]] static std::expected<std::shared_ptr<MovementController>, StarException> create(Root& root, MovementParameters const& parameters = {});

  MovementParameters const& parameters() const;

  // Apply any set parameters from the given set on top of the current set.
  [[nodiscard]] std::expected<void, StarException> apply_parameters(MovementParameters const& parameters);

  // Reset the parameters from the sensible defaults, and apply the given
  // parameters (if any) on top of them.
  [[nodiscard]] std::expected<void, StarException> reset_parameters(MovementParameters const& parameters = MovementParameters());

  // Stores and loads position, velocity, and rotation.
  Json store_state() const;
  [[nodiscard]] std::expected<void, StarException> load_state(Json const& state);

  // Currently active mass parameter
  float mass() const;

  // Currently active collisionPoly parameter
  PolyF const& collision_poly() const;
  void set_collision_poly(PolyF const& poly);

  virtual Vec2F position() const;
  float x_position() const;
  float y_position() const;

  Vec2F velocity() const;
  float x_velocity() const;
  float y_velocity() const;

  virtual float rotation() const;

  // CollisionPoly rotated and translated by position
  PolyF collision_body() const;

  // Gets the bounding box of the collisionPoly() rotated by current rotation,
  // but not translated into world space
  RectF local_bound_box() const;

  // Shorthand for getting the bound box of the current collisionBody()
  RectF collision_bound_box() const;

  // Is the collision body colliding with any collision geometry.
  bool is_colliding() const;
  // Is the collision body colliding with special "Null" collision blocks.
  bool is_null_colliding() const;

  // Is the body currently stuck in an un-solvable collision.
  bool is_collision_stuck() const;

  // If this body is sticking, this is the angle toward the surface it's stuck to
  std::optional<float> sticking_direction() const;

  // From 0.0 to 1.0, the amount of the collision body (or if the collision
  // body is null, just the center position) that is in liquid.
  float liquid_percentage() const;

  // Returns the liquid that the body is most in, if any
  LiquidId liquid_id() const;

  bool on_ground() const;
  bool zero_g() const;

  bool at_world_limit(bool bottom_only = false) const;

  void set_position(Vec2F position);
  // Explicit presentation discontinuities are separate from physics integration,
  // which also uses setPosition internally.
  void reset_presentation() { ++m_presentation_epoch; }
  std::uint64_t presentation_epoch() const { return m_presentation_epoch; }
  void set_x_position(float x_position);
  void set_y_position(float y_position);

  void translate(Vec2F const& direction);

  void set_velocity(Vec2F velocity);
  void set_x_velocity(float x_velocity);
  void set_y_velocity(float y_velocity);

  void add_momentum(Vec2F const& momentum);

  void set_rotation(float angle);

  // Apply one timestep of rotation.
  void rotate(float rotation_rate);

  // Apply one timestep of acceleration.
  void accelerate(Vec2F const& acceleration);

  // Apply one timestep of force.
  void force(Vec2F const& force);

  // Apply up to the maxControlForce of force to approach the given velocity.
  void approach_velocity(Vec2F const& target_velocity, float max_control_force);

  // Approach a velocity in the given angle, ignoring the component of velocity
  // normal to that angle.  If positiveOnly is true, then only approaches the
  // velocity by applying force in the direction of the given angle, never
  // opposite it, so avoids slowing down.
  void approach_velocity_along_angle(float angle, float target_velocity, float max_control_force, bool positive_only = false);

  // Shorthand for approachVelocityAlongAngle with 0 and pi/2.
  void approach_x_velocity(float target_x_velocity, float max_control_force);
  void approach_y_velocity(float target_y_velocity, float max_control_force);

  void init(World& world);
  virtual void uninit();

  // Stores dt value for Lua calls.
  void set_timestep(float dt);

  // Integrates the ActorMovementController for dt and applies all
  // forces.
  [[nodiscard]] std::expected<void, StarException> tick_master(float dt);

  // Does not integrate, only tracks master state and updates non-networked
  // fields based on local data
  [[nodiscard]] std::expected<void, StarException> tick_slave(float dt);

  void set_ignore_physics_entities(Set<EntityId> ignore_physics_entities);
  // Transfers each world-space shape to the callback; false stops enumeration.
  using MovingCollisionCallback = std::function<std::expected<bool, StarException>(MovingCollisionId, CollisionKind, PolyF&&, RectF const&)>;
  [[nodiscard]] std::expected<void, StarException> for_each_moving_collision(RectF const& region, MovingCollisionCallback const& callback);

protected:
  [[nodiscard]] std::expected<void, StarException> update_force_regions();
  [[nodiscard]] std::expected<void, StarException> update_liquid_percentage();
  // Force the onGround status when movement is controlled outside this controller.
  void set_on_ground(bool on_ground);

  // True for an actionable force, even when velocity already matches its target.
  bool applied_force_region() const;
  // The collision correction applied during the most recent update, if any.
  Vec2F collision_correction() const;
  // Horizontal slope of the ground the collision body has collided with, if
  // any.
  Vec2F surface_slope() const;
  // Velocity of the surface that the body is resting on, if any
  Vec2F surface_velocity() const;

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
    std::optional<MovingCollisionId> surface_moving_collision_id;
    bool is_stuck = false;
    bool on_ground = false;
    Vec2F ground_slope;
    CollisionKind collision_kind = CollisionKind::Null;
  };

  struct CollisionSeparation {
    Vec2F correction;
    bool solution_found = false;
    std::optional<MovingCollisionId> moving_collision_id;
    CollisionKind collision_kind = CollisionKind::Null;
  };

  struct CollisionPoly {
    PolyF poly;
    RectF poly_bounds;
    Vec2F sort_position;
    std::optional<MovingCollisionId> moving_collision_id;
    CollisionKind collision_kind = CollisionKind::Null;
    float sort_distance;
  };

  static CollisionKind max_or_null_collision(CollisionKind a, CollisionKind b);
  static CollisionResult collision_move(List<CollisionPoly>& collision_polys, PolyF const& body, Vec2F const& movement,
      bool ignore_platforms, bool enable_surface_slope_correction, float maximum_correction, float maximum_platform_correction, Vec2F sort_center, float dt);
  static CollisionSeparation collision_separate(List<CollisionPoly> const& collision_polys, PolyF const& poly,
      bool ignore_platforms, float maximum_platform_correction, bool upward, float separation_tolerance, PolyF& scratch);

  [[nodiscard]] static std::expected<void, StarException> validate_parameters(MovementParameters const& parameters);
  void update_parameters(bool refresh_interpolators);
  void update_position_interpolators();

  [[nodiscard]] std::expected<void, StarException> query_collisions(RectF const& region);
  [[nodiscard]] std::expected<void, StarException> sample_liquid(World& world, Vec2F const& pos, RectF const& bounds);
  [[nodiscard]] std::expected<void, StarException> apply_force_regions(World& world, Vec2F const& pos, PolyF const& body, RectF const& bounds);

  MovementParameters m_parameters;
  Root& m_root;

  std::optional<std::reference_wrapper<World>> m_world;

  Set<EntityId> m_ignore_physics_entities;

  NetElementData<PolyF> m_collision_poly;
  NetElementFloat m_mass;
  NetElementFloat m_x_position;
  NetElementFloat m_y_position;
  NetElementFloat m_x_velocity;
  NetElementFloat m_y_velocity;
  NetElementFloat m_rotation;

  NetElementBool m_colliding;
  NetElementBool m_collision_stuck;
  NetElementBool m_null_colliding;
  NetElementData<std::optional<float>> m_sticking_direction;
  NetElementBool m_on_ground;
  NetElementBool m_zero_g;

  float m_liquid_percentage;
  LiquidId m_liquid_id;

  NetElementData<std::optional<MovingCollisionId>> m_surface_moving_collision;
  NetElementFloat m_x_relative_surface_moving_collision_position;
  NetElementFloat m_y_relative_surface_moving_collision_position;

  bool m_applied_force_region = false;
  Vec2F m_collision_correction;
  Vec2F m_surface_slope;
  Vec2F m_surface_moving_collision_position;
  Vec2F m_surface_velocity;

  bool m_resting;
  int m_rest_ticks;
  float m_time_step;
  std::uint64_t m_presentation_epoch = 0;

  List<CollisionPoly> m_working_collisions;
  List<PolyF> m_collision_buffers;
};

}

export module star.movement_controller;

export namespace Star {
  using ::Star::MovementControllerExceptionTag;
  using ::Star::MovementControllerException;
  using ::Star::MovementParameters;
  using ::Star::MovementController;
}

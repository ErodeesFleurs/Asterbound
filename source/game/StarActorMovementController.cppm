module;
import std;
import star.string;
import star.format;
import star.json;
import star.id_map;
import star.game_types;
import star.poly;
import star.casting;
import star.data_stream;
import star.strong_typedef;
import star.list;
import star.vector;
import star.bi_map;
import star.a_star;

import star.damage_types;
import star.world_geometry;
import star.status_types;
import star.damage;
import star.light_source;
import star.entity;
import star.collision_block;
import star.force_regions;
import star.physics_entity;
import star.movement_controller;
import star.platformer_astar_types;
import star.anchorable_entity;

import star.game_timers;

import star.net_element_basic_fields;

namespace Star {

class World;
class Root;
class PathController;

struct ActorMovementControllerExceptionTag {
  static constexpr std::string_view name() { return "ActorMovementControllerException"; }
};
using ActorMovementControllerException = StarError<ActorMovementControllerExceptionTag, MovementControllerException>;

struct ActorJumpProfile {
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, ActorJumpProfile& movement_parameters);
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, ActorJumpProfile const& movement_parameters);

  ActorJumpProfile();
  [[nodiscard]] static std::expected<ActorJumpProfile, StarException> create(Json const& config);

  Json to_json() const;

  std::optional<float> jump_speed;
  std::optional<float> jump_control_force;
  std::optional<float> jump_initial_percentage;

  // If this is greater than 0.0, jump hold time is limited by this factor.
  std::optional<float> jump_hold_time;
  // If this is greater than 0.0, then the total jump time for *all jumps in a
  // multi jump set* is limited by this factor.
  std::optional<float> jump_total_hold_time;

  std::optional<bool> multi_jump;
  std::optional<float> re_jump_delay;
  std::optional<bool> auto_jump;
  std::optional<bool> collision_cancelled;
};

[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, ActorJumpProfile& movement_parameters);
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, ActorJumpProfile const& movement_parameters);

// A not-quite superset of MovementParameters, with some fields from
// MovementParameters ignored because they make no sense, and other fields
// expanded out to different cases based on Actor specific things.
struct ActorMovementParameters {
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, ActorMovementParameters& movement_parameters);
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, ActorMovementParameters const& movement_parameters);

  // Load sensible defaults from a config file.
  [[nodiscard]] static std::expected<ActorMovementParameters, StarException> sensible_defaults(Root& root);

  // Construct parameters from config with only those specified in the config
  // set, if any.
  explicit ActorMovementParameters();
  [[nodiscard]] static std::expected<ActorMovementParameters, StarException> create(Json const& config);

  [[nodiscard]] std::expected<Json, StarException> to_json() const;

  // Merge the given set of movement parameters on top of this one, with any
  // set parameters in rhs overwriting the ones in this set.
  ActorMovementParameters merge(ActorMovementParameters const& rhs) const;
  void merge_from(ActorMovementParameters const& patch);
  void merge_from(ActorMovementParameters&& patch);

  std::optional<float> mass;
  std::optional<float> gravity_multiplier;
  std::optional<float> liquid_buoyancy;
  std::optional<float> air_buoyancy;
  std::optional<float> bounce_factor;
  std::optional<bool> stop_on_first_bounce;
  std::optional<bool> enable_surface_slope_correction;
  std::optional<float> slope_sliding_factor;
  std::optional<float> max_movement_per_step;
  std::optional<float> maximum_correction;
  std::optional<float> speed_limit;

  std::optional<PolyF> standing_poly;
  std::optional<PolyF> crouching_poly;

  std::optional<bool> sticky_collision;
  std::optional<float> sticky_force;

  std::optional<float> walk_speed;
  std::optional<float> run_speed;
  std::optional<float> fly_speed;

  std::optional<float> air_friction;
  std::optional<float> liquid_friction;

  std::optional<float> minimum_liquid_percentage;
  std::optional<float> liquid_impedance;

  std::optional<float> normal_ground_friction;
  std::optional<float> ambulating_ground_friction;

  std::optional<float> ground_force;
  std::optional<float> air_force;
  std::optional<float> liquid_force;

  ActorJumpProfile air_jump_profile;
  ActorJumpProfile liquid_jump_profile;

  std::optional<float> fall_status_speed_min;
  std::optional<int> fall_through_sustain_frames;
  std::optional<float> maximum_platform_correction;
  std::optional<float> maximum_platform_correction_velocity_factor;

  std::optional<StringSet> physics_effect_categories;

  std::optional<float> ground_movement_minimum_sustain;
  std::optional<float> ground_movement_maximum_sustain;
  std::optional<float> ground_movement_check_distance;

  std::optional<bool> collision_enabled;
  std::optional<bool> friction_enabled;
  std::optional<bool> gravity_enabled;

  std::optional<float> path_explore_rate;
};

[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, ActorMovementParameters& movement_parameters);
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, ActorMovementParameters const& movement_parameters);

// A set of normalized values that act as "modifiers" or "bonuses" to movement,
// and can be combined sensibly.  A modifier of 0.0 represents a 0% change, a
// modifier of 0.2 represents a 20% increase, and a modifier of -0.2 represents
// a 20% decrease.  Also includes some flags that disable functionality
// combined with logical OR.
struct ActorMovementModifiers {
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, ActorMovementModifiers& movement_modifiers);
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, ActorMovementModifiers const& movement_modifiers);

  explicit ActorMovementModifiers();
  [[nodiscard]] static std::expected<ActorMovementModifiers, StarException> create(Json const& config);

  Json to_json() const;

  // Combines each modifier value through addition.
  ActorMovementModifiers combine(ActorMovementModifiers const& rhs) const;

  float ground_movement_modifier;
  float liquid_movement_modifier;
  float speed_modifier;
  float air_jump_modifier;
  float liquid_jump_modifier;

  bool running_suppressed;
  bool jumping_suppressed;
  // Suppresses left, right, down, crouch, jump, and fly controls
  bool movement_suppressed;
  bool facing_suppressed;
};

[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, ActorMovementModifiers& movement_modifiers);
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, ActorMovementModifiers const& movement_modifiers);

class ActorMovementController : public virtual MovementController {
  class ConstructionKey {
    friend class ActorMovementController;
    ConstructionKey() = default;

  public:
    ConstructionKey(ConstructionKey const&) = default;
  };

public:
  ActorMovementController(ConstructionKey, Root& root);
  // Constructs an ActorMovementController with parameters loaded from sensible
  // defaults, and the given parameters (if any) applied on top of them.
  [[nodiscard]] static std::expected<std::shared_ptr<ActorMovementController>, StarException> create(Root& root, ActorMovementParameters const& parameters = ActorMovementParameters());

  void uninit() override;

  // Currently active parameters.
  ActorMovementParameters const& base_parameters() const;

  // Apply any set parameters from the given set on top of the current set.
  [[nodiscard]] std::expected<void, StarException> update_base_parameters(ActorMovementParameters const& parameters);

  // Reset the parameters from the sensible defaults, and apply the given
  // parameters (if any) on top of them.
  [[nodiscard]] std::expected<void, StarException> reset_base_parameters(ActorMovementParameters const& parameters = ActorMovementParameters());

  // Currently active modifiers.
  ActorMovementModifiers const& base_modifiers() const;

  // Combine the given modifiers with the already active modifiers.
  void update_base_modifiers(ActorMovementModifiers const& modifiers);

  // Reset all modifiers to the given values
  void reset_base_modifiers(ActorMovementModifiers const& modifiers = ActorMovementModifiers());

  // Stores and loads position, velocity, rotation, movingDirection,
  // facingDirection, and crouching
  [[nodiscard]] std::expected<Json, StarException> store_state() const;
  [[nodiscard]] std::expected<void, StarException> load_state(Json const& state);

  // Optionaly anchor this ActorMovementController to the given
  // AnchorableEntity.  position, rotation, and facing direction will be set
  // based on the entity anchor alone every tick, and on slaved
  // ActorMovementControllers it will be updated based on the actual slave-side
  // AnchorableEntity state.
  [[nodiscard]] std::expected<void, StarException> set_anchor_state(EntityAnchorState anchor_state);
  [[nodiscard]] std::expected<void, StarException> reset_anchor_state();
  std::optional<EntityAnchorState> anchor_state() const;
  std::shared_ptr<EntityAnchor const> entity_anchor() const;

  // ActorMovementController position and rotation honor the entity anchor, if
  // an anchor is set.
  Vec2F position() const override;
  float rotation() const override;

  bool walking() const;
  bool running() const;
  Direction moving_direction() const;
  // Override only the existing network presentation field; local movement
  // direction, facing, velocity and physics remain unchanged.
  void set_network_moving_direction(std::optional<Direction> direction);
  Direction facing_direction() const;
  bool crouching() const;
  bool flying() const;
  bool falling() const;
  bool can_jump() const;
  bool jumping() const;
  // Slightly different than onGround, in that this is sustained for a few
  // extra frames of movement before it becomes false.
  bool ground_movement() const;
  bool liquid_movement() const;
  bool pathfinding() const;

  // Basic direct physics controls that can be called multiple times per
  // update and will be combined.
  void control_rotation(float rotation_rate);
  void control_acceleration(Vec2F const& acceleration);
  void control_force(Vec2F const& force);
  void control_approach_velocity(Vec2F const& target_velocity, float max_control_force);
  void control_approach_velocity_along_angle(float angle, float target_velocity, float max_control_force, bool positive_only = false);
  void control_approach_x_velocity(float target_xvelocity, float max_control_force);
  void control_approach_y_velocity(float target_yvelocity, float max_control_force);

  // Apply ActorMovementParameters / ActorMovementModifiers only as long as
  // the controls are active.  Can be called multiple times per update and
  // will be combined.
  void control_parameters(ActorMovementParameters const& parameters);
  void control_modifiers(ActorMovementModifiers const& modifiers);

  // Higher level movement controls that use forces defined in the
  // ActorMovementParameters.  Calling more than once per update will override
  // previous calls.
  void control_move(Direction direction, bool run = true);
  void control_face(Direction direction);
  void control_down();
  void control_crouch();
  void control_jump(bool jump_even_if_unable = false);
  void control_fly(Vec2F const& velocity);

  // Advance one search step or collect the completed tick for this request.
  [[nodiscard]] std::expected<std::optional<bool>, StarException>
  path_move(Vec2F const& position, std::optional<PlatformerAStar::Parameters> const& parameters = {});
  [[nodiscard]] std::expected<std::optional<bool>, StarException>
  control_path_move(Vec2F const& position, bool run = false, std::optional<PlatformerAStar::Parameters> const& parameters = {});

  // Used for user controller input.
  void set_move_speed_multiplier(float multiplier = 1.0f);

  // Clears all control data.
  void clear_controls();

  // Integrates the ActorMovementController and applies all
  // the control data and clears it for the next step.
  [[nodiscard]] std::expected<void, StarException> tick_master(float dt);

  [[nodiscard]] std::expected<void, StarException> tick_slave(float dt);


private:
  struct ApproachVelocityCommand {
    Vec2F target_velocity;
    float max_control_force;
  };

  struct ApproachVelocityAlongAngleCommand {
    float along_angle;
    float target_velocity;
    float max_control_force;
    bool positive_only;
  };

  [[nodiscard]] std::expected<void, StarException> apply_mc_parameters(ActorMovementParameters const& parameters);
  [[nodiscard]] std::expected<void, StarException> do_set_anchor_state(std::optional<EntityAnchorState> anchor_state);
  void set_moving_direction(Direction direction);

  ActorMovementParameters m_base_parameters;
  ActorMovementModifiers m_base_modifiers;

  // State data

  NetElementBool m_walking;
  NetElementBool m_running;
  NetElementEnum<Direction> m_moving_direction;
  std::optional<Direction> m_native_moving_direction;
  NetElementEnum<Direction> m_facing_direction;
  NetElementBool m_crouching;
  NetElementBool m_flying;
  NetElementBool m_falling;
  NetElementBool m_can_jump;
  NetElementBool m_jumping;
  NetElementBool m_ground_movement;
  NetElementBool m_liquid_movement;
  NetElementData<std::optional<EntityAnchorState>> m_anchor_state;
  std::shared_ptr<EntityAnchor const> m_entity_anchor;

  // Command data

  float m_control_rotation_rate;
  Vec2F m_control_acceleration;
  Vec2F m_control_force;
  List<ApproachVelocityCommand> m_control_approach_velocities;
  List<ApproachVelocityAlongAngleCommand> m_control_approach_velocity_along_angles;

  std::optional<Direction> m_control_move;
  std::optional<Direction> m_control_face;
  bool m_control_run;
  bool m_control_crouch;
  bool m_control_down;
  bool m_control_jump;
  bool m_control_jump_anyway;

  std::optional<Vec2F> m_control_fly;

  std::optional<std::pair<Vec2F, bool>> m_control_path_move;
  std::optional<std::pair<Vec2F, bool>> m_path_move_result;
  std::unique_ptr<PathController> m_path_controller;

  std::optional<ActorMovementParameters> m_control_parameters;
  ActorMovementModifiers m_control_modifiers;

  // Internal state data

  int m_fall_through_sustain;
  bool m_last_control_jump;
  bool m_last_control_down;
  float m_move_speed_multiplier;

  GameTimer m_re_jump_timer;
  std::optional<GameTimer> m_jump_hold_timer;
  GameTimer m_ground_movement_sustain_timer;

  // Target horizontal velocity for walking / running
  float m_target_horizontal_ambulating_velocity;
};

class PathController {
public:
  PathController(World& world);

  // A changed search policy invalidates both an in-flight finder and a ready path.
  bool set_parameters(PlatformerAStar::Parameters const& parameters);
  void reset();
  bool pathfinding() const;
  std::optional<Vec2F> target_position() const;
  std::optional<Direction> facing() const;
  std::optional<PlatformerAStar::Action> current_action() const;

  // return true for reaching goal, false for failing to find path, nothing for running
  [[nodiscard]] std::expected<std::optional<bool>, StarException>
  find_path(ActorMovementController& movement_controller, Vec2F const& target_position);
  [[nodiscard]] std::expected<std::optional<bool>, StarException> move(ActorMovementController& movement_controller,
                                                                       ActorMovementParameters const& parameters,
                                                                       ActorMovementModifiers const& modifiers,
                                                                       bool run, float dt);

private:
  [[nodiscard]] std::expected<bool, StarException> validate_edge(ActorMovementController& movement_controller,
                                                                 PlatformerAStar::Edge const& edge);
  [[nodiscard]] std::expected<bool, StarException> moving_collision(ActorMovementController& movement_controller,
                                                                    PolyF const& collision_poly);

private:
  [[nodiscard]] std::expected<bool, StarException> on_ground(ActorMovementController const& movement_controller,
                                                             Vec2F const& position,
                                                             CollisionSet const& collision_set) const;

  std::reference_wrapper<World> m_world;
  PlatformerAStar::Parameters m_parameters;

  std::optional<Vec2F> m_start_position;
  std::optional<Vec2F> m_target_position;
  std::shared_ptr<PlatformerAStar::PathFinder> m_path_finder;

  std::optional<Direction> m_control_face;

  std::size_t m_edge_index;
  float m_edge_timer;
  std::optional<PlatformerAStar::Path> m_path;
};

}

export module star.actor_movement_controller;

export namespace Star {
  using ::Star::ActorMovementControllerExceptionTag;
  using ::Star::ActorMovementControllerException;
  using ::Star::ActorJumpProfile;
  using ::Star::ActorMovementParameters;
  using ::Star::ActorMovementModifiers;
  using ::Star::ActorMovementController;
  using ::Star::PathController;
}

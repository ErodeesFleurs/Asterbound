module;
import std;
import star.format;
import star.lua;
import star.lua_components;
import star.root;
import star.world;
import star.string;
import star.rpc_promise;
import star.json;
import star.game_types;
import star.message_types;
import star.list;
import star.vector;
import star.platformer_astar_types;
import star.actor_movement_controller;
import star.lua_converters;
import star.lua_game_converters;
import star.movement_controller_lua_bindings;
import star.math_common;
import star.algorithm;
import star.output_proxy;


namespace Star {

template <typename result_type, typename... arg_types>
concept lua_movement_update_call = requires(LuaContext const& context, arg_types&&... args) {
  context.invoke_path_if_present<result_type>("update", std::forward<arg_types>(args)...);
};

// Buffered actor controls belong to one fixed-address script host. The host
// explicitly clears before updating and applies controls afterwards, even on error.
class LuaScriptMovement {
public:
  explicit LuaScriptMovement(LuaBaseComponent& host) : m_host(host) {}
  LuaScriptMovement(LuaScriptMovement const&) = delete;
  LuaScriptMovement& operator=(LuaScriptMovement const&) = delete;
  [[nodiscard]] std::expected<void, StarException> add_actor_movement_callbacks(ActorMovementController& controller);
  [[nodiscard]] std::expected<void, StarException> remove_actor_movement_callbacks();
  bool auto_clear_controls() const;
  void set_auto_clear_controls(bool enabled);
  void before_update(bool update_ready) {
    if (update_ready && m_auto_clear_controls)
      clear_controls();
  }
  [[nodiscard]] std::expected<void, StarException> perform_controls();
private:
  LuaBaseComponent& m_host;
  void clear_controls();
  std::optional<std::reference_wrapper<ActorMovementController>> m_movement_controller;
  bool m_auto_clear_controls = true;
  float m_control_rotation = 0.0f;
  Vec2F m_control_acceleration;
  Vec2F m_control_force;
  std::optional<std::tuple<Vec2F, float>> m_control_approach_velocity;
  std::optional<std::tuple<float, float, float, bool>> m_control_approach_velocity_along_angle;
  std::optional<ActorMovementParameters> m_control_parameters;
  std::optional<ActorMovementModifiers> m_control_modifiers;
  std::optional<std::tuple<Direction, bool>> m_control_move;
  std::optional<Direction> m_control_face;
  bool m_control_down = false;
  bool m_control_crouch = false;
  std::optional<bool> m_control_jump;
  bool m_control_hold_jump = false;
  std::optional<Vec2F> m_control_fly;
  bool m_reset_path_move = false;
  std::optional<std::pair<Vec2F, bool>> m_control_path_move;
  std::optional<std::pair<Vec2F, bool>> m_path_move_result;
  std::optional<PlatformerAStar::Parameters> m_path_parameters;
};

[[nodiscard]] inline std::expected<void, StarException> LuaScriptMovement::add_actor_movement_callbacks(ActorMovementController& controller) {
  auto built = LuaBindings::make_movement_controller_callbacks(controller);
  if (!built)
    return std::unexpected(std::move(built.error()));
  auto callbacks = std::move(*built);
  callbacks.remove_callback("setVelocity");
  if (auto result = callbacks.register_callback("setVelocity", [&self = *this](Vec2F const& velocity) {
    self.m_reset_path_move = true;
    self.m_movement_controller->get().setVelocity(velocity);
  }); !result)
    return std::unexpected(std::move(result.error()));
  callbacks.remove_callback("setXVelocity");
  if (auto result = callbacks.register_callback("setXVelocity", [&self = *this](float velocity) {
    self.m_reset_path_move = true;
    self.m_movement_controller->get().setXVelocity(velocity);
  }); !result)
    return std::unexpected(std::move(result.error()));
  callbacks.remove_callback("setYVelocity");
  if (auto result = callbacks.register_callback("setYVelocity", [&self = *this](float velocity) {
    self.m_reset_path_move = true;
    self.m_movement_controller->get().setYVelocity(velocity);
  }); !result)
    return std::unexpected(std::move(result.error()));
  callbacks.remove_callback("addMomentum");
  if (auto result = callbacks.register_callback("addMomentum", [&self = *this](Vec2F const& momentum) {
    self.m_reset_path_move = true;
    self.m_movement_controller->get().addMomentum(momentum);
  }); !result)
    return std::unexpected(std::move(result.error()));
  callbacks.remove_callback("setRotation");
  if (auto result = callbacks.register_callback("setRotation", [&self = *this](float rotation) {
    self.m_reset_path_move = true;
    self.m_movement_controller->get().setRotation(rotation);
  }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback("setAnchorState",
                                               [&self = *this](EntityId entity, std::size_t position) {
                                                 return self.m_movement_controller->get().set_anchor_state(
                                                   {.entityId = entity, .positionIndex = position});
                                               });
      !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback("resetAnchorState", [&self = *this]() { return self.m_movement_controller->get().reset_anchor_state(); }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback("anchorState", [&self = *this]() {
    if (auto state = self.m_movement_controller->get().anchor_state())
      return LuaVariadic<LuaValue>{LuaInt(state->entityId), LuaInt(state->positionIndex)};
    return LuaVariadic<LuaValue>{};
  }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback("baseParameters", [&self = *this]() { return self.m_movement_controller->get().base_parameters(); }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback("walking", [&self = *this]() { return self.m_movement_controller->get().walking(); }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback("running", [&self = *this]() { return self.m_movement_controller->get().running(); }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback("movingDirection", [&self = *this]() { return numerical_direction(self.m_movement_controller->get().moving_direction()); }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback("facingDirection", [&self = *this]() { return numerical_direction(self.m_movement_controller->get().facing_direction()); }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback("crouching", [&self = *this]() { return self.m_movement_controller->get().crouching(); }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback("flying", [&self = *this]() { return self.m_movement_controller->get().flying(); }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback("falling", [&self = *this]() { return self.m_movement_controller->get().falling(); }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback("canJump", [&self = *this]() { return self.m_movement_controller->get().can_jump(); }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback("jumping", [&self = *this]() { return self.m_movement_controller->get().jumping(); }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback("groundMovement", [&self = *this]() { return self.m_movement_controller->get().ground_movement(); }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback("liquidMovement", [&self = *this]() { return self.m_movement_controller->get().liquid_movement(); }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback("controlRotation", [&self = *this](float rotation) { self.m_control_rotation += rotation; }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback("controlAcceleration", [&self = *this](Vec2F const& acceleration) { self.m_control_acceleration += acceleration; }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback("controlForce", [&self = *this](Vec2F const& force) { self.m_control_force += force; }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback("controlApproachVelocity", [&self = *this](Vec2F const& velocity, float force) {
    self.m_control_approach_velocity.emplace(velocity, force);
  }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback("controlApproachVelocityAlongAngle", [&self = *this](float angle, float velocity, float force, bool positive) {
    self.m_control_approach_velocity_along_angle.emplace(angle, velocity, force, positive);
  }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback("controlApproachXVelocity", [&self = *this](float velocity, float force) {
    self.m_control_approach_velocity_along_angle.emplace(0.0f, velocity, force, false);
  }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback("controlApproachYVelocity", [&self = *this](float velocity, float force) {
    self.m_control_approach_velocity_along_angle.emplace(std::numbers::pi / 2.0f, velocity, force, false);
  }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback("controlParameters", [&self = *this](ActorMovementParameters patch) {
    if (self.m_control_parameters)
      self.m_control_parameters->merge_from(std::move(patch));
    else
      self.m_control_parameters.emplace(std::move(patch));
  }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback("controlModifiers", [&self = *this](ActorMovementModifiers const& modifiers) {
    self.m_control_modifiers = self.m_control_modifiers.value_or(ActorMovementModifiers{}).combine(modifiers);
  }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback("controlMove", [&self = *this](std::optional<float> const& direction, std::optional<bool> const& run) {
    if (auto value = direction_of(direction.value_or(float{})))
      self.m_control_move.emplace(*value, run.value_or(true));
  }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback("controlFace", [&self = *this](std::optional<float> const& direction) {
    if (auto value = direction_of(direction.value_or(float{})))
      self.m_control_face = *value;
  }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback("controlDown", [&self = *this]() { self.m_control_down = true; }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback("controlCrouch", [&self = *this]() { self.m_control_crouch = true; }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback("controlJump", [&self = *this](bool jump) { self.m_control_jump = jump; }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback("controlHoldJump", [&self = *this]() { self.m_control_hold_jump = true; }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback("controlFly", [&self = *this](Vec2F const& velocity) { self.m_control_fly = velocity; }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback("controlPathMove", [&self = *this](Vec2F const& position, std::optional<bool> run, std::optional<PlatformerAStar::Parameters> parameters) -> std::expected<std::optional<bool>, StarException> {
    self.m_control_path_move.reset();
    if (auto completed = std::exchange(self.m_path_move_result, {}); completed && completed->first == position
        && (!parameters || parameters == self.m_path_parameters))
      return completed->second;

    // Scripts can observe an immediate terminal result. Only pending requests
    // are replayed after the script update (including ticks between updates).
    auto result = self.m_movement_controller->get().path_move(position, parameters);
    if (result && !result->has_value()) {
      self.m_control_path_move.emplace(position, run.value_or(false));
      self.m_path_parameters = std::move(parameters);
    }
    return result;
  }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback("pathfinding", [&self = *this]() { return self.m_movement_controller->get().pathfinding(); }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback_with_signature<bool>("autoClearControls", std::bind_front(&LuaScriptMovement::auto_clear_controls, std::ref(*this))); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback_with_signature<void, bool>("setAutoClearControls", std::bind_front(&LuaScriptMovement::set_auto_clear_controls, std::ref(*this))); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback_with_signature<void>("clearControls", std::bind_front(&LuaScriptMovement::clear_controls, std::ref(*this))); !result)
    return std::unexpected(std::move(result.error()));
  auto previous = m_movement_controller;
  m_movement_controller = std::ref(controller);
  if (auto added = m_host.add_callbacks("mcontroller", std::move(callbacks)); !added) {
    m_movement_controller = previous;
    return std::unexpected(std::move(added.error()));
  }
  return {};
}

[[nodiscard]] inline std::expected<void, StarException> LuaScriptMovement::remove_actor_movement_callbacks() {
  auto removed = m_host.remove_callbacks("mcontroller");
  if (!removed)
    return std::unexpected(std::move(removed.error()));
  m_movement_controller.reset();
  return {};
}

inline bool LuaScriptMovement::auto_clear_controls() const { return m_auto_clear_controls; }

inline void LuaScriptMovement::set_auto_clear_controls(bool enabled) { m_auto_clear_controls = enabled; }

[[nodiscard]] inline std::expected<void, StarException> LuaScriptMovement::perform_controls() {
  if (!m_movement_controller)
    return {};
  m_movement_controller->get().control_rotation(m_control_rotation);
  m_movement_controller->get().control_acceleration(m_control_acceleration);
  m_movement_controller->get().control_force(m_control_force);
  if (m_control_approach_velocity)
    std::apply(std::bind_front(&ActorMovementController::control_approach_velocity, *m_movement_controller), *m_control_approach_velocity);
  if (m_control_approach_velocity_along_angle)
    std::apply(std::bind_front(&ActorMovementController::control_approach_velocity_along_angle, *m_movement_controller), *m_control_approach_velocity_along_angle);
  if (m_control_parameters)
    m_movement_controller->get().control_parameters(*m_control_parameters);
  if (m_control_modifiers)
    m_movement_controller->get().control_modifiers(*m_control_modifiers);
  if (m_control_move)
    std::apply(std::bind_front(&ActorMovementController::control_move, *m_movement_controller), *m_control_move);
  if (m_control_face)
    m_movement_controller->get().control_face(*m_control_face);
  if (m_control_down)
    m_movement_controller->get().control_down();
  if (m_control_crouch)
    m_movement_controller->get().control_crouch();
  if (m_control_jump)
    m_movement_controller->get().control_jump(*m_control_jump);
  if (m_control_hold_jump && !m_movement_controller->get().onGround())
    m_movement_controller->get().control_jump();
  if (m_control_fly)
    m_movement_controller->get().control_fly(*m_control_fly);
  if (m_reset_path_move)
    m_control_path_move.reset();
  if (m_control_path_move && !m_path_move_result) {
    auto const& [position, run] = *m_control_path_move;
    auto result = m_movement_controller->get().control_path_move(position, run, m_path_parameters);
    if (!result)
      return std::unexpected(std::move(result.error()));
    if (*result)
      m_path_move_result.emplace(position, **result);
  }
  return {};
}

inline void LuaScriptMovement::clear_controls() {
  m_control_rotation = {};
  m_control_acceleration = {};
  m_control_force = {};
  m_control_approach_velocity.reset();
  m_control_approach_velocity_along_angle.reset();
  m_control_parameters.reset();
  m_control_modifiers.reset();
  m_control_move.reset();
  m_control_face.reset();
  m_control_down = false;
  m_control_crouch = false;
  m_control_jump.reset();
  m_control_hold_jump = false;
  m_control_fly.reset();
  m_reset_path_move = false;
  if (!m_control_path_move)
    m_path_move_result.reset();
  m_control_path_move.reset();
}

// Persistent entity scripts snapshot storage before stopping child threads.
class LuaActorScript final : public LuaBaseComponent {
public:
  explicit LuaActorScript(Root& root) : LuaBaseComponent(root), m_world(*this), m_update(*this), m_storage(*this), m_messages(*this), m_movement(*this) {}
  ~LuaActorScript() override { m_update.destroy(); }
  [[nodiscard]] std::expected<void, StarException> init(World& world) { return m_world.init(world); }
  [[nodiscard]] std::expected<void, StarException> uninit() { return m_world.uninit(); }
  unsigned update_delta() const { return m_update.update_delta(); }
  float update_dt(float dt) const { return m_update.update_dt(dt); }
  float update_dt() const { return m_update.update_dt(); }
  void set_update_delta(unsigned delta) { m_update.set_update_delta(delta); }
  bool update_ready() const { return m_update.update_ready(); }
  template <typename result_type = LuaValue, typename... arg_types>
    requires lua_movement_update_call<result_type, arg_types...>
  [[nodiscard]] std::expected<std::optional<result_type>, StarException> update(arg_types&&... args) {
    m_movement.before_update(m_update.update_ready());
    auto result = m_update.template update<result_type>(std::forward<arg_types>(args)...);
    auto controls = m_movement.perform_controls();
    if (!controls) {
      if (!result)
        return std::unexpected(controls.error().with_related(result.error(), "Original Lua movement update failure"));
      return std::unexpected(std::move(controls.error()));
    }
    return result;
  }
  RpcPromise<Json> thread_pass_message(String const& thread, String const& message, JsonArray const& args) override {
    return m_update.thread_pass_message(thread, message, args);
  }
  [[nodiscard]] std::expected<JsonObject, StarException> get_script_storage() const { return m_storage.get_script_storage(); }
  [[nodiscard]] std::expected<void, StarException> set_script_storage(JsonObject storage) { return m_storage.set_script_storage(std::move(storage)); }
  [[nodiscard]] MessageResponseResult handle_message(String const& message, bool local_message, JsonArray const& args = {}) {
    return m_messages.handle_message(message, local_message, args);
  }
  [[nodiscard]] std::expected<void, StarException> add_actor_movement_callbacks(ActorMovementController& controller) { return m_movement.add_actor_movement_callbacks(controller); }
  [[nodiscard]] std::expected<void, StarException> remove_actor_movement_callbacks() { return m_movement.remove_actor_movement_callbacks(); }
  bool auto_clear_controls() const { return m_movement.auto_clear_controls(); }
  void set_auto_clear_controls(bool enabled) { m_movement.set_auto_clear_controls(enabled); }
protected:
  using LuaBaseComponent::set_lua_root;
  [[nodiscard]] std::expected<void, StarException> context_setup() override {
    if (auto result = LuaBaseComponent::context_setup(); !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = m_update.context_setup(); !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = m_storage.context_setup(); !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = m_messages.context_setup(); !result)
      return std::unexpected(std::move(result.error()));
    return {};
  }
  [[nodiscard]] std::expected<void, StarException> context_shutdown() override {
    m_messages.context_shutdown();
    auto storage = m_storage.read_storage();
    auto stopped = m_update.context_shutdown();
    auto shutdown = LuaScriptUpdate::finish_shutdown(std::move(stopped), LuaBaseComponent::context_shutdown());
    return m_storage.finish_shutdown(std::move(storage), std::move(shutdown));
  }
  [[nodiscard]] std::expected<LuaCallbacks, StarException> make_threads_callbacks() override {
    auto callbacks = LuaBaseComponent::make_threads_callbacks();
    if (!callbacks)
      return std::unexpected(std::move(callbacks.error()));
    if (auto result = m_update.add_threads_callbacks(*callbacks); !result)
      return std::unexpected(std::move(result.error()));
    return callbacks;
  }
private:
  LuaScriptWorld m_world;
  LuaScriptUpdate m_update;
  LuaScriptStorage m_storage;
  LuaScriptMessages m_messages;
  LuaScriptMovement m_movement;
};

// Active-item / quest scripts install storage before update callbacks, and read
// it back after stopping child threads. This differs intentionally from actors.
class LuaActionScript final : public LuaBaseComponent {
public:
  explicit LuaActionScript(Root& root) : LuaBaseComponent(root), m_world(*this), m_update(*this), m_storage(*this), m_messages(*this), m_movement(*this) {}
  ~LuaActionScript() override { m_update.destroy(); }
  [[nodiscard]] std::expected<void, StarException> init(World& world) { return m_world.init(world); }
  [[nodiscard]] std::expected<void, StarException> uninit() { return m_world.uninit(); }
  unsigned update_delta() const { return m_update.update_delta(); }
  float update_dt(float dt) const { return m_update.update_dt(dt); }
  float update_dt() const { return m_update.update_dt(); }
  void set_update_delta(unsigned delta) { m_update.set_update_delta(delta); }
  bool update_ready() const { return m_update.update_ready(); }
  template <typename result_type = LuaValue, typename... arg_types>
    requires lua_movement_update_call<result_type, arg_types...>
  [[nodiscard]] std::expected<std::optional<result_type>, StarException> update(arg_types&&... args) {
    m_movement.before_update(m_update.update_ready());
    auto result = m_update.template update<result_type>(std::forward<arg_types>(args)...);
    auto controls = m_movement.perform_controls();
    if (!controls) {
      if (!result)
        return std::unexpected(controls.error().with_related(result.error(), "Original Lua movement update failure"));
      return std::unexpected(std::move(controls.error()));
    }
    return result;
  }
  RpcPromise<Json> thread_pass_message(String const& thread, String const& message, JsonArray const& args) override {
    return m_update.thread_pass_message(thread, message, args);
  }
  [[nodiscard]] std::expected<JsonObject, StarException> get_script_storage() const { return m_storage.get_script_storage(); }
  [[nodiscard]] std::expected<void, StarException> set_script_storage(JsonObject storage) { return m_storage.set_script_storage(std::move(storage)); }
  [[nodiscard]] MessageResponseResult handle_message(String const& message, bool local_message, JsonArray const& args = {}) {
    return m_messages.handle_message(message, local_message, args);
  }
  [[nodiscard]] std::expected<void, StarException> add_actor_movement_callbacks(ActorMovementController& controller) { return m_movement.add_actor_movement_callbacks(controller); }
  [[nodiscard]] std::expected<void, StarException> remove_actor_movement_callbacks() { return m_movement.remove_actor_movement_callbacks(); }
  bool auto_clear_controls() const { return m_movement.auto_clear_controls(); }
  void set_auto_clear_controls(bool enabled) { m_movement.set_auto_clear_controls(enabled); }
protected:
  using LuaBaseComponent::set_lua_root;
  [[nodiscard]] std::expected<void, StarException> context_setup() override {
    if (auto result = LuaBaseComponent::context_setup(); !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = m_storage.context_setup(); !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = m_update.context_setup(); !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = m_messages.context_setup(); !result)
      return std::unexpected(std::move(result.error()));
    return {};
  }
  [[nodiscard]] std::expected<void, StarException> context_shutdown() override {
    m_messages.context_shutdown();
    auto stopped = m_update.context_shutdown();
    auto storage = m_storage.read_storage();
    auto shutdown = m_storage.finish_shutdown(std::move(storage), LuaBaseComponent::context_shutdown());
    return LuaScriptUpdate::finish_shutdown(std::move(stopped), std::move(shutdown));
  }
  [[nodiscard]] std::expected<LuaCallbacks, StarException> make_threads_callbacks() override {
    auto callbacks = LuaBaseComponent::make_threads_callbacks();
    if (!callbacks)
      return std::unexpected(std::move(callbacks.error()));
    if (auto result = m_update.add_threads_callbacks(*callbacks); !result)
      return std::unexpected(std::move(result.error()));
    return callbacks;
  }
private:
  LuaScriptWorld m_world;
  LuaScriptUpdate m_update;
  LuaScriptStorage m_storage;
  LuaScriptMessages m_messages;
  LuaScriptMovement m_movement;
};

// Status-effect scripts retain actor controls without allocating script storage.
class LuaStatusScript final : public LuaBaseComponent {
public:
  explicit LuaStatusScript(Root& root) : LuaBaseComponent(root), m_world(*this), m_update(*this), m_messages(*this), m_movement(*this) {}
  ~LuaStatusScript() override { m_update.destroy(); }
  [[nodiscard]] std::expected<void, StarException> init(World& world) { return m_world.init(world); }
  [[nodiscard]] std::expected<void, StarException> uninit() { return m_world.uninit(); }
  unsigned update_delta() const { return m_update.update_delta(); }
  float update_dt(float dt) const { return m_update.update_dt(dt); }
  float update_dt() const { return m_update.update_dt(); }
  void set_update_delta(unsigned delta) { m_update.set_update_delta(delta); }
  bool update_ready() const { return m_update.update_ready(); }
  template <typename result_type = LuaValue, typename... arg_types>
    requires lua_movement_update_call<result_type, arg_types...>
  [[nodiscard]] std::expected<std::optional<result_type>, StarException> update(arg_types&&... args) {
    m_movement.before_update(m_update.update_ready());
    auto result = m_update.template update<result_type>(std::forward<arg_types>(args)...);
    auto controls = m_movement.perform_controls();
    if (!controls) {
      if (!result)
        return std::unexpected(controls.error().with_related(result.error(), "Original Lua movement update failure"));
      return std::unexpected(std::move(controls.error()));
    }
    return result;
  }
  RpcPromise<Json> thread_pass_message(String const& thread, String const& message, JsonArray const& args) override {
    return m_update.thread_pass_message(thread, message, args);
  }
  [[nodiscard]] MessageResponseResult handle_message(String const& message, bool local_message, JsonArray const& args = {}) {
    return m_messages.handle_message(message, local_message, args);
  }
  [[nodiscard]] std::expected<void, StarException> add_actor_movement_callbacks(ActorMovementController& controller) { return m_movement.add_actor_movement_callbacks(controller); }
  [[nodiscard]] std::expected<void, StarException> remove_actor_movement_callbacks() { return m_movement.remove_actor_movement_callbacks(); }
  bool auto_clear_controls() const { return m_movement.auto_clear_controls(); }
  void set_auto_clear_controls(bool enabled) { m_movement.set_auto_clear_controls(enabled); }
protected:
  using LuaBaseComponent::set_lua_root;
  [[nodiscard]] std::expected<void, StarException> context_setup() override {
    if (auto result = LuaBaseComponent::context_setup(); !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = m_update.context_setup(); !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = m_messages.context_setup(); !result)
      return std::unexpected(std::move(result.error()));
    return {};
  }
  [[nodiscard]] std::expected<void, StarException> context_shutdown() override {
    m_messages.context_shutdown();
    auto stopped = m_update.context_shutdown();
    return LuaScriptUpdate::finish_shutdown(std::move(stopped), LuaBaseComponent::context_shutdown());
  }
  [[nodiscard]] std::expected<LuaCallbacks, StarException> make_threads_callbacks() override {
    auto callbacks = LuaBaseComponent::make_threads_callbacks();
    if (!callbacks)
      return std::unexpected(std::move(callbacks.error()));
    if (auto result = m_update.add_threads_callbacks(*callbacks); !result)
      return std::unexpected(std::move(result.error()));
    return callbacks;
  }
private:
  LuaScriptWorld m_world;
  LuaScriptUpdate m_update;
  LuaScriptMessages m_messages;
  LuaScriptMovement m_movement;
};

}

export module star.lua_actor_movement_component;

export namespace Star {
  using ::Star::LuaScriptMovement;
  using ::Star::LuaActorScript;
  using ::Star::LuaActionScript;
  using ::Star::LuaStatusScript;
}

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
  void before_update(bool updateReady) {
    if (updateReady && m_autoClearControls)
      clear_controls();
  }
  [[nodiscard]] std::expected<void, StarException> perform_controls();
private:
  LuaBaseComponent& m_host;
  void clear_controls();
  std::optional<std::reference_wrapper<ActorMovementController>> m_movementController;
  bool m_autoClearControls = true;
  float m_controlRotation = 0.0f;
  Vec2F m_controlAcceleration;
  Vec2F m_controlForce;
  std::optional<std::tuple<Vec2F, float>> m_controlApproachVelocity;
  std::optional<std::tuple<float, float, float, bool>> m_controlApproachVelocityAlongAngle;
  std::optional<ActorMovementParameters> m_controlParameters;
  std::optional<ActorMovementModifiers> m_controlModifiers;
  std::optional<std::tuple<Direction, bool>> m_controlMove;
  std::optional<Direction> m_controlFace;
  bool m_controlDown = false;
  bool m_controlCrouch = false;
  std::optional<bool> m_controlJump;
  bool m_controlHoldJump = false;
  std::optional<Vec2F> m_controlFly;
  bool m_reset_path_move = false;
  std::optional<std::pair<Vec2F, bool>> m_control_path_move;
  std::optional<std::pair<Vec2F, bool>> m_path_move_result;
  std::optional<PlatformerAStar::Parameters> m_path_parameters;
};

[[nodiscard]] inline std::expected<void, StarException> LuaScriptMovement::add_actor_movement_callbacks(ActorMovementController& controller) {
  auto built = LuaBindings::makeMovementControllerCallbacks(controller);
  if (!built)
    return std::unexpected(std::move(built.error()));
  auto callbacks = std::move(*built);
  callbacks.remove_callback("setVelocity");
  if (auto result = callbacks.register_callback("setVelocity", [&self = *this](Vec2F const& velocity) {
    self.m_reset_path_move = true;
    self.m_movementController->get().setVelocity(velocity);
  }); !result)
    return std::unexpected(std::move(result.error()));
  callbacks.remove_callback("setXVelocity");
  if (auto result = callbacks.register_callback("setXVelocity", [&self = *this](float velocity) {
    self.m_reset_path_move = true;
    self.m_movementController->get().setXVelocity(velocity);
  }); !result)
    return std::unexpected(std::move(result.error()));
  callbacks.remove_callback("setYVelocity");
  if (auto result = callbacks.register_callback("setYVelocity", [&self = *this](float velocity) {
    self.m_reset_path_move = true;
    self.m_movementController->get().setYVelocity(velocity);
  }); !result)
    return std::unexpected(std::move(result.error()));
  callbacks.remove_callback("addMomentum");
  if (auto result = callbacks.register_callback("addMomentum", [&self = *this](Vec2F const& momentum) {
    self.m_reset_path_move = true;
    self.m_movementController->get().addMomentum(momentum);
  }); !result)
    return std::unexpected(std::move(result.error()));
  callbacks.remove_callback("setRotation");
  if (auto result = callbacks.register_callback("setRotation", [&self = *this](float rotation) {
    self.m_reset_path_move = true;
    self.m_movementController->get().setRotation(rotation);
  }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback("setAnchorState",
                                               [&self = *this](EntityId entity, std::size_t position) {
                                                 return self.m_movementController->get().setAnchorState(
                                                   {.entityId = entity, .positionIndex = position});
                                               });
      !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback("resetAnchorState", [&self = *this]() { return self.m_movementController->get().resetAnchorState(); }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback("anchorState", [&self = *this]() {
    if (auto state = self.m_movementController->get().anchorState())
      return LuaVariadic<LuaValue>{LuaInt(state->entityId), LuaInt(state->positionIndex)};
    return LuaVariadic<LuaValue>{};
  }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback("baseParameters", [&self = *this]() { return self.m_movementController->get().baseParameters(); }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback("walking", [&self = *this]() { return self.m_movementController->get().walking(); }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback("running", [&self = *this]() { return self.m_movementController->get().running(); }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback("movingDirection", [&self = *this]() { return numerical_direction(self.m_movementController->get().movingDirection()); }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback("facingDirection", [&self = *this]() { return numerical_direction(self.m_movementController->get().facingDirection()); }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback("crouching", [&self = *this]() { return self.m_movementController->get().crouching(); }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback("flying", [&self = *this]() { return self.m_movementController->get().flying(); }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback("falling", [&self = *this]() { return self.m_movementController->get().falling(); }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback("canJump", [&self = *this]() { return self.m_movementController->get().canJump(); }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback("jumping", [&self = *this]() { return self.m_movementController->get().jumping(); }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback("groundMovement", [&self = *this]() { return self.m_movementController->get().groundMovement(); }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback("liquidMovement", [&self = *this]() { return self.m_movementController->get().liquidMovement(); }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback("controlRotation", [&self = *this](float rotation) { self.m_controlRotation += rotation; }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback("controlAcceleration", [&self = *this](Vec2F const& acceleration) { self.m_controlAcceleration += acceleration; }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback("controlForce", [&self = *this](Vec2F const& force) { self.m_controlForce += force; }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback("controlApproachVelocity", [&self = *this](Vec2F const& velocity, float force) {
    self.m_controlApproachVelocity.emplace(velocity, force);
  }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback("controlApproachVelocityAlongAngle", [&self = *this](float angle, float velocity, float force, bool positive) {
    self.m_controlApproachVelocityAlongAngle.emplace(angle, velocity, force, positive);
  }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback("controlApproachXVelocity", [&self = *this](float velocity, float force) {
    self.m_controlApproachVelocityAlongAngle.emplace(0.0f, velocity, force, false);
  }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback("controlApproachYVelocity", [&self = *this](float velocity, float force) {
    self.m_controlApproachVelocityAlongAngle.emplace(std::numbers::pi / 2.0f, velocity, force, false);
  }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback("controlParameters", [&self = *this](ActorMovementParameters patch) {
    if (self.m_controlParameters)
      self.m_controlParameters->merge_from(std::move(patch));
    else
      self.m_controlParameters.emplace(std::move(patch));
  }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback("controlModifiers", [&self = *this](ActorMovementModifiers const& modifiers) {
    self.m_controlModifiers = self.m_controlModifiers.value_or(ActorMovementModifiers{}).combine(modifiers);
  }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback("controlMove", [&self = *this](std::optional<float> const& direction, std::optional<bool> const& run) {
    if (auto value = direction_of(direction.value_or(float{})))
      self.m_controlMove.emplace(*value, run.value_or(true));
  }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback("controlFace", [&self = *this](std::optional<float> const& direction) {
    if (auto value = direction_of(direction.value_or(float{})))
      self.m_controlFace = *value;
  }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback("controlDown", [&self = *this]() { self.m_controlDown = true; }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback("controlCrouch", [&self = *this]() { self.m_controlCrouch = true; }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback("controlJump", [&self = *this](bool jump) { self.m_controlJump = jump; }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback("controlHoldJump", [&self = *this]() { self.m_controlHoldJump = true; }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback("controlFly", [&self = *this](Vec2F const& velocity) { self.m_controlFly = velocity; }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback("controlPathMove", [&self = *this](Vec2F const& position, std::optional<bool> run, std::optional<PlatformerAStar::Parameters> parameters) -> std::expected<std::optional<bool>, StarException> {
    self.m_control_path_move.reset();
    if (auto completed = std::exchange(self.m_path_move_result, {}); completed && completed->first == position
        && (!parameters || parameters == self.m_path_parameters))
      return completed->second;

    // Scripts can observe an immediate terminal result. Only pending requests
    // are replayed after the script update (including ticks between updates).
    auto result = self.m_movementController->get().path_move(position, parameters);
    if (result && !result->has_value()) {
      self.m_control_path_move.emplace(position, run.value_or(false));
      self.m_path_parameters = std::move(parameters);
    }
    return result;
  }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback("pathfinding", [&self = *this]() { return self.m_movementController->get().pathfinding(); }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback_with_signature<bool>("autoClearControls", std::bind_front(&LuaScriptMovement::auto_clear_controls, std::ref(*this))); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback_with_signature<void, bool>("setAutoClearControls", std::bind_front(&LuaScriptMovement::set_auto_clear_controls, std::ref(*this))); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback_with_signature<void>("clearControls", std::bind_front(&LuaScriptMovement::clear_controls, std::ref(*this))); !result)
    return std::unexpected(std::move(result.error()));
  auto previous = m_movementController;
  m_movementController = std::ref(controller);
  if (auto added = m_host.add_callbacks("mcontroller", std::move(callbacks)); !added) {
    m_movementController = previous;
    return std::unexpected(std::move(added.error()));
  }
  return {};
}

[[nodiscard]] inline std::expected<void, StarException> LuaScriptMovement::remove_actor_movement_callbacks() {
  auto removed = m_host.remove_callbacks("mcontroller");
  if (!removed)
    return std::unexpected(std::move(removed.error()));
  m_movementController.reset();
  return {};
}

inline bool LuaScriptMovement::auto_clear_controls() const { return m_autoClearControls; }

inline void LuaScriptMovement::set_auto_clear_controls(bool enabled) { m_autoClearControls = enabled; }

[[nodiscard]] inline std::expected<void, StarException> LuaScriptMovement::perform_controls() {
  if (!m_movementController)
    return {};
  m_movementController->get().controlRotation(m_controlRotation);
  m_movementController->get().controlAcceleration(m_controlAcceleration);
  m_movementController->get().controlForce(m_controlForce);
  if (m_controlApproachVelocity)
    std::apply(std::bind_front(&ActorMovementController::controlApproachVelocity, *m_movementController), *m_controlApproachVelocity);
  if (m_controlApproachVelocityAlongAngle)
    std::apply(std::bind_front(&ActorMovementController::controlApproachVelocityAlongAngle, *m_movementController), *m_controlApproachVelocityAlongAngle);
  if (m_controlParameters)
    m_movementController->get().controlParameters(*m_controlParameters);
  if (m_controlModifiers)
    m_movementController->get().controlModifiers(*m_controlModifiers);
  if (m_controlMove)
    std::apply(std::bind_front(&ActorMovementController::controlMove, *m_movementController), *m_controlMove);
  if (m_controlFace)
    m_movementController->get().controlFace(*m_controlFace);
  if (m_controlDown)
    m_movementController->get().controlDown();
  if (m_controlCrouch)
    m_movementController->get().controlCrouch();
  if (m_controlJump)
    m_movementController->get().controlJump(*m_controlJump);
  if (m_controlHoldJump && !m_movementController->get().onGround())
    m_movementController->get().controlJump();
  if (m_controlFly)
    m_movementController->get().controlFly(*m_controlFly);
  if (m_reset_path_move)
    m_control_path_move.reset();
  if (m_control_path_move && !m_path_move_result) {
    auto const& [position, run] = *m_control_path_move;
    auto result = m_movementController->get().control_path_move(position, run, m_path_parameters);
    if (!result)
      return std::unexpected(std::move(result.error()));
    if (*result)
      m_path_move_result.emplace(position, **result);
  }
  return {};
}

inline void LuaScriptMovement::clear_controls() {
  m_controlRotation = {};
  m_controlAcceleration = {};
  m_controlForce = {};
  m_controlApproachVelocity.reset();
  m_controlApproachVelocityAlongAngle.reset();
  m_controlParameters.reset();
  m_controlModifiers.reset();
  m_controlMove.reset();
  m_controlFace.reset();
  m_controlDown = false;
  m_controlCrouch = false;
  m_controlJump.reset();
  m_controlHoldJump = false;
  m_controlFly.reset();
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
  template <typename Ret = LuaValue, typename... V>
  [[nodiscard]] std::expected<std::optional<Ret>, StarException> update(V&&... args) {
    m_movement.before_update(m_update.update_ready());
    auto result = m_update.template update<Ret>(std::forward<V>(args)...);
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
  [[nodiscard]] MessageResponseResult handle_message(String const& message, bool localMessage, JsonArray const& args = {}) {
    return m_messages.handle_message(message, localMessage, args);
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
  template <typename Ret = LuaValue, typename... V>
  [[nodiscard]] std::expected<std::optional<Ret>, StarException> update(V&&... args) {
    m_movement.before_update(m_update.update_ready());
    auto result = m_update.template update<Ret>(std::forward<V>(args)...);
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
  [[nodiscard]] MessageResponseResult handle_message(String const& message, bool localMessage, JsonArray const& args = {}) {
    return m_messages.handle_message(message, localMessage, args);
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
  template <typename Ret = LuaValue, typename... V>
  [[nodiscard]] std::expected<std::optional<Ret>, StarException> update(V&&... args) {
    m_movement.before_update(m_update.update_ready());
    auto result = m_update.template update<Ret>(std::forward<V>(args)...);
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
  [[nodiscard]] MessageResponseResult handle_message(String const& message, bool localMessage, JsonArray const& args = {}) {
    return m_messages.handle_message(message, localMessage, args);
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

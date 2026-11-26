module;
import star.format;
import std;
import star.lua;
import star.json;
import star.net_compatibility;
import star.random;
import star.periodic_function;
import star.ordered_map;
import star.string;
import star.data_stream;
import star.color;
import star.list;
import star.image_processing;
import star.hash;
import star.string_view;
import star.thread;
import star.directives;
import star.asset_path;
import star.bi_map;
import star.vector;
import star.audio;
import star.map;
import star.set;


import star.animated_part_set;
import star.drawable;
import star.animation;
import star.particle;
import star.mixer;
import star.light_source;
import star.networked_animator;
import star.lua_components;
import star.lua_actor_movement_component;

import star.tech_database;

import star.game_types;
import star.message_types;
import star.net_element;
import star.net_element_basic_fields;
import star.net_element_float_fields;
import star.net_element_dynamic_group;
import star.net_element_group;

namespace Star {

class Root;

class StatusController;
class Entity;
class ActorMovementController;

// Class that acts as a movement controller for the parent entity that supports
// a variety scriptable "Tech" that the entity can use that affect movement,
// physics, sounds, particles, damage regions, etc.  Network capable, and all
// flags are sensibly set on both the client and server.
class TechController : public NetElementGroup {
public:
  enum class ParentState {
    Stand,
    Fly,
    Fall,
    Sit,
    Lay,
    Duck,
    Walk,
    Run,
    Swim,
    SwimIdle
  };
  static std::expected<EnumMap<ParentState>, StarException> const ParentStateNames;

  [[nodiscard]] static std::expected<std::shared_ptr<TechController>, StarException> create(Root& root);

  [[nodiscard]] std::expected<Json, StarException> disk_store();
  [[nodiscard]] std::expected<void, StarException> disk_load(Json const& store);

  [[nodiscard]] std::expected<void, StarException> init(Entity& parent_entity, ActorMovementController& movement_controller, StatusController& status_controller);
  [[nodiscard]] std::expected<void, StarException> uninit();

  [[nodiscard]] std::expected<void, StarException> sync_equipped_tech(HashMap<TechType, String> const& equipped);
  [[nodiscard]] std::expected<void, StarException> reload_tech();

  bool tech_overridden() const;
  [[nodiscard]] std::expected<void, StarException> set_override_tech(StringList const& tech_modules);
  [[nodiscard]] std::expected<void, StarException> clear_override_tech();

  void setShouldRun(bool shouldRun);

  void beginPrimaryFire();
  void beginAltFire();
  void endPrimaryFire();
  void endAltFire();

  void moveUp();
  void moveDown();
  void moveLeft();
  void moveRight();
  void jump();
  void special(int specialKey);

  void setAimPosition(Vec2F const& aimPosition);

  [[nodiscard]] std::expected<void, StarException> tickMaster(float dt);
  [[nodiscard]] std::expected<void, StarException> tickSlave(float dt);

  std::optional<ParentState> parentState() const;
  DirectivesGroup const& parentDirectives() const;
  Vec2F parentOffset() const;
  bool toolUsageSuppressed() const;

  bool parentHidden() const;

  [[nodiscard]] std::expected<List<Drawable>, StarException> backDrawables(float interpolation = 1.0f);
  [[nodiscard]] std::expected<List<Drawable>, StarException> frontDrawables(float interpolation = 1.0f);

  [[nodiscard]] std::expected<void, StarException> append_light_sources(List<LightSource>& lights, float interpolation = 1.0f) const;

  List<std::shared_ptr<AudioInstance>> pullNewAudios();
  List<Particle> pullNewParticles();

  [[nodiscard]] MessageResponseResult receive_message(String const& message, bool local_message, JsonArray const& args = {});

private:
  explicit TechController(Root& root);
  Root& m_root;
  struct TechAnimator : public NetElement {
    TechAnimator(Root& root);
    [[nodiscard]] static std::expected<std::shared_ptr<TechAnimator>, StarException> create(Root& root, std::optional<String> animationConfig = {});

    void init_net_version(std::optional<std::reference_wrapper<NetElementVersion const>> version = std::nullopt) override;

    [[nodiscard]] std::expected<void, StarException> net_store(DataStream& ds, NetCompatibilityRules rules = {}) const override;
    [[nodiscard]] std::expected<void, StarException> net_load(DataStream& ds, NetCompatibilityRules rules) override;

    [[nodiscard]] std::expected<void, StarException> enable_net_interpolation(float extrapolation_hint = 0.0f) override;
    [[nodiscard]] std::expected<void, StarException> disable_net_interpolation() override;
    [[nodiscard]] std::expected<void, StarException> tick_net_interpolation(float dt) override;

    [[nodiscard]] std::expected<bool, StarException> write_net_delta(DataStream& ds, std::uint64_t fromVersion, NetCompatibilityRules rules = {}) const override;
    [[nodiscard]] std::expected<void, StarException> read_net_delta(DataStream& ds, float interpolationTime = 0.0f, NetCompatibilityRules rules = {}) override;
    [[nodiscard]] std::expected<void, StarException> blank_net_delta(float interpolationTime) override;

    // If setting invisible, stops all playing audio
    void setVisible(bool visible);
    bool isVisible() const;

    Root& m_root;
    std::optional<String> animationConfig;
    std::shared_ptr<NetworkedAnimator> animator;
    NetworkedAnimator::DynamicTarget dynamicTarget;
    NetElementBool visible;
    NetElementGroup netGroup;
  };

  using TechAnimatorGroup = NetElementDynamicGroup<TechAnimator>;

  struct TechModule {
    explicit TechModule(Root& root) : script_component(root) {}

    TechConfig config;

    LuaActorScript
        script_component;
    bool visible;
    bool tool_usage_suppressed;
    Directives parentDirectives;
    TechAnimatorGroup::ElementId animator_id;
  };

  template <std::ranges::input_range Names>
  [[nodiscard]] std::expected<void, StarException> load_tech_names(Names&& names);

  // Name of module, any existing module script data.
  [[nodiscard]] std::expected<void, StarException> setup_tech_modules(List<std::tuple<String, JsonObject>> const& module_inits);

  [[nodiscard]] std::expected<void, StarException> unload_module(TechModule& tech_module);

  [[nodiscard]] std::expected<void, StarException> initialize_modules();

  void resetMoves();
  [[nodiscard]] std::expected<void, StarException> updateAnimators(float dt);

  [[nodiscard]] std::expected<LuaCallbacks, StarException> make_tech_callbacks(TechModule& tech_module);

  std::optional<StringList> m_overridden_tech;
  bool m_tech_sync_pending = false;
  LinkedList<TechModule> m_tech_modules;
  TechAnimatorGroup m_tech_animators;

  std::optional<std::reference_wrapper<Entity>> m_parent_entity;
  std::optional<std::reference_wrapper<ActorMovementController>> m_movement_controller;
  std::optional<std::reference_wrapper<StatusController>> m_status_controller;

  bool m_moveRun;
  bool m_movePrimaryFire;
  bool m_moveAltFire;
  bool m_moveUp;
  bool m_moveDown;
  bool m_moveLeft;
  bool m_moveRight;
  bool m_moveJump;
  bool m_moveSpecial1;
  bool m_moveSpecial2;
  bool m_moveSpecial3;

  Vec2F m_aimPosition;

  NetElementData<std::optional<ParentState>> m_parentState;
  NetElementData<DirectivesGroup> m_parentDirectives;
  NetElementFloat m_xParentOffset;
  NetElementFloat m_yParentOffset;
  NetElementBool m_parentHidden;
  NetElementBool m_toolUsageSuppressed;
};

}

export module star.tech_controller;

export namespace Star {
  using ::Star::TechController;
  using ::Star::StatusController;
}

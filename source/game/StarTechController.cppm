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

  [[nodiscard]] std::expected<Json, StarException> diskStore();
  [[nodiscard]] std::expected<void, StarException> diskLoad(Json const& store);

  [[nodiscard]] std::expected<void, StarException> init(Entity& parentEntity, ActorMovementController& movementController, StatusController& statusController);
  [[nodiscard]] std::expected<void, StarException> uninit();

  [[nodiscard]] std::expected<void, StarException> setLoadedTech(StringList const& techModules, bool forceLoad = false);
  StringList loadedTech() const;
  [[nodiscard]] std::expected<void, StarException> reloadTech();

  bool techOverridden() const;
  [[nodiscard]] std::expected<void, StarException> setOverrideTech(StringList const& techModules);
  [[nodiscard]] std::expected<void, StarException> clearOverrideTech();

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

  [[nodiscard]] std::expected<List<LightSource>, StarException> lightSources(float interpolation = 1.0f) const;

  List<std::shared_ptr<AudioInstance>> pullNewAudios();
  List<Particle> pullNewParticles();

  [[nodiscard]] std::expected<std::optional<ChainableJsonMessageResponse>, StarException> receiveMessage(String const& message, bool localMessage, JsonArray const& args = {});

private:
  explicit TechController(Root& root);
  Root& m_root;
  struct TechAnimator : public NetElement {
    TechAnimator(Root& root);
    [[nodiscard]] static std::expected<std::shared_ptr<TechAnimator>, StarException> create(Root& root, std::optional<String> animationConfig = {});

    void initNetVersion(std::optional<std::reference_wrapper<NetElementVersion const>> version = std::nullopt) override;

    [[nodiscard]] std::expected<void, StarException> netStore(DataStream& ds, NetCompatibilityRules rules = {}) const override;
    [[nodiscard]] std::expected<void, StarException> netLoad(DataStream& ds, NetCompatibilityRules rules) override;

    [[nodiscard]] std::expected<void, StarException> enableNetInterpolation(float extrapolationHint = 0.0f) override;
    [[nodiscard]] std::expected<void, StarException> disableNetInterpolation() override;
    [[nodiscard]] std::expected<void, StarException> tickNetInterpolation(float dt) override;

    [[nodiscard]] std::expected<bool, StarException> writeNetDelta(DataStream& ds, std::uint64_t fromVersion, NetCompatibilityRules rules = {}) const override;
    [[nodiscard]] std::expected<void, StarException> readNetDelta(DataStream& ds, float interpolationTime = 0.0f, NetCompatibilityRules rules = {}) override;
    [[nodiscard]] std::expected<void, StarException> blankNetDelta(float interpolationTime) override;

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

  typedef NetElementDynamicGroup<TechAnimator> TechAnimatorGroup;

  struct TechModule {
    explicit TechModule(Root& root) : scriptComponent(root) {}

    TechConfig config;

    LuaActorScript
        scriptComponent;
    bool visible;
    bool toolUsageSuppressed;
    Directives parentDirectives;
    TechAnimatorGroup::ElementId animatorId;
  };

  // Name of module, any existing module script data.
  [[nodiscard]] std::expected<void, StarException> setupTechModules(List<std::tuple<String, JsonObject>> const& moduleInits);

  [[nodiscard]] std::expected<void, StarException> unloadModule(TechModule& techModule);

  [[nodiscard]] std::expected<void, StarException> initializeModules();

  void resetMoves();
  [[nodiscard]] std::expected<void, StarException> updateAnimators(float dt);

  [[nodiscard]] std::expected<LuaCallbacks, StarException> makeTechCallbacks(TechModule& techModule);

  std::optional<StringList> m_overriddenTech;
  LinkedList<TechModule> m_techModules;
  TechAnimatorGroup m_techAnimators;

  std::optional<std::reference_wrapper<Entity>> m_parentEntity;
  std::optional<std::reference_wrapper<ActorMovementController>> m_movementController;
  std::optional<std::reference_wrapper<StatusController>> m_statusController;

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

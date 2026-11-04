module;
import std;
import star.format;
import star.type_identity;
import star.json;
import star.data_stream_extra;
import star.id_map;
import star.ordered_map;
import star.lru_cache;
import star.thread;
import star.time;
import star.random;
import star.ttl_cache;
import star.casting;
import star.strong_typedef;
import star.audio;
import star.list;
import star.map;
import star.set;
import star.vector;
import star.bi_map;
import star.color;
import star.string;
import star.data_stream;
import star.poly;
import star.rect;
import star.image_processing;
import star.hash;
import star.string_view;
import star.directives;
import star.asset_path;
import star.game_types;
import star.rpc_promise;
import star.a_star;
import star.lua;
import star.periodic_function;

import star.net_element_basic_fields;
import star.net_element_containers;
import star.net_element_system;

import star.net_compatibility;
import star.byte_array;
namespace Star {
class Root;
class Monster;
}

import star.damage_types;
import star.world_geometry;
import star.status_types;
import star.damage;
import star.entity;
import star.mixer;
import star.drawable;
import star.entity_rendering_types;
import star.animation;
import star.particle;
import star.light_source;
import star.entity_rendering;
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
import star.platformer_astar_types;
import star.anchorable_entity;
import star.game_timers;
import star.actor_movement_controller;
import star.animated_part_set;
import star.networked_animator;
import star.portrait_entity;
import star.damage_bar_entity;
import star.nametag_entity;
import star.aggressive_entity;
import star.scripted_entity;
import star.chat_action;
import star.chatty_entity;
import star.lua_components;
import star.lua_animation_component;
import star.lua_actor_movement_component;
import star.mobile_entity;
import star.actor_entity;
import star.monster_database;
import star.effect_emitter;

namespace Star {

class StatusController;
class BehaviorState;

class Monster
  : public virtual DamageBarEntity,
    public virtual AggressiveEntity,
    public virtual ScriptedEntity,
    public virtual PhysicsEntity,
    public virtual NametagEntity,
    public virtual ChattyEntity,
    public virtual InteractiveEntity,
    public virtual ActorEntity {
public:
  void const* queryType(TypeKey key) const noexcept override {
    return queryTypeOf<Monster, DamageBarEntity, AggressiveEntity, ScriptedEntity,
        PhysicsEntity, NametagEntity, ChattyEntity, InteractiveEntity,
        ActorEntity>(this, key);
  }
  std::string_view dynamicTypeName() const noexcept override {
    return Star::typeName<Monster>();
  }

  struct SkillInfo {
    String label;
    String image;
  };

  [[nodiscard]] static std::expected<std::shared_ptr<Monster>, StarException> create(Root& rootContext, MonsterVariant const& variant, std::optional<float> level = {});
  [[nodiscard]] static std::expected<std::shared_ptr<Monster>, StarException> create(Root& rootContext, Json const& diskStore);

  [[nodiscard]] std::expected<Json, StarException> diskStore() const;
  [[nodiscard]] std::expected<ByteArray, StarException> netStore(NetCompatibilityRules rules = {});

  EntityType entityType() const override;
  ClientEntityMode clientEntityMode() const override;

  [[nodiscard]] std::expected<void, StarException> init(World& world, EntityId entityId, EntityMode mode) override;
  [[nodiscard]] std::expected<void, StarException> uninit() override;

  Vec2F position() const override;
  RectF metaBoundBox() const override;

  Vec2F velocity() const;

  [[nodiscard]] std::expected<Vec2F, StarException> mouthOffset() const;
  [[nodiscard]] std::expected<Vec2F, StarException> feetOffset() const;

  RectF collisionArea() const override;

  [[nodiscard]] std::expected<std::pair<ByteArray, std::uint64_t>, StarException> writeNetState(std::uint64_t fromVersion = 0, NetCompatibilityRules rules = {}) override;
  [[nodiscard]] std::expected<void, StarException> readNetState(ByteArray data, float interpolationTime = 0.0f, NetCompatibilityRules rules = {}) override;

  [[nodiscard]] std::expected<void, StarException> enableInterpolation(float extrapolationHint) override;
  [[nodiscard]] std::expected<void, StarException> disableInterpolation() override;

  [[nodiscard]] std::expected<String, StarException> name() const override;
  [[nodiscard]] std::expected<String, StarException> description() const override;

  [[nodiscard]] std::expected<List<LightSource>, StarException> lightSources() const override;

  [[nodiscard]] std::expected<std::optional<HitType>, StarException> queryHit(DamageSource const& source) const override;
  [[nodiscard]] std::expected<std::optional<PolyF>, StarException> hitPoly() const override;

  [[nodiscard]] std::expected<void, StarException> hitOther(EntityId targetEntityId, DamageRequest const& damageRequest) override;
  [[nodiscard]] std::expected<void, StarException> damagedOther(DamageNotification const& damage) override;

  [[nodiscard]] std::expected<List<DamageNotification>, StarException> applyDamage(DamageRequest const& damage) override;
  [[nodiscard]] std::expected<List<DamageNotification>, StarException> selfDamageNotifications() override;

  [[nodiscard]] std::expected<List<DamageSource>, StarException> damageSources() const override;

  [[nodiscard]] std::expected<bool, StarException> shouldDie();
  [[nodiscard]] std::expected<void, StarException> knockout();

  [[nodiscard]] std::expected<bool, StarException> shouldDestroy() const override;
  [[nodiscard]] std::expected<void, StarException> destroy(std::optional<std::reference_wrapper<RenderCallback>> renderCallback) override;

  [[nodiscard]] std::expected<void, StarException> update(float dt, std::uint64_t currentStep) override;

  std::uint64_t presentationEpoch() const override;

  [[nodiscard]] std::expected<void, StarException> collectRenderEvents(RenderCallback& renderCallback) override;

  [[nodiscard]] std::expected<void, StarException> render(RenderCallback& renderCallback) override;

  [[nodiscard]] std::expected<void, StarException> renderLightSources(RenderCallback& renderCallback) override;

  void setPosition(Vec2F const& pos);

  [[nodiscard]] std::expected<std::optional<ChainableJsonMessageResponse>, StarException> receiveMessage(ConnectionId sendingConnection, String const& message, JsonArray const& args) override;

  [[nodiscard]] std::expected<float, StarException> maxHealth() const override;
  float health() const override;
  DamageBarType damageBar() const override;

  [[nodiscard]] std::expected<float, StarException> monsterLevel() const;
  [[nodiscard]] std::expected<SkillInfo, StarException> activeSkillInfo() const;

  [[nodiscard]] std::expected<List<Drawable>, StarException> portrait(PortraitMode mode) const override;
  String typeName() const;
  MonsterVariant monsterVariant() const;

  std::optional<String> statusText() const override;
  bool displayNametag() const override;
  [[nodiscard]] std::expected<Vec3B, StarException> nametagColor() const override;
  [[nodiscard]] std::expected<Vec2F, StarException> nametagOrigin() const override;
  [[nodiscard]] std::expected<String, StarException> nametag() const override;

  bool aggressive() const override;

  [[nodiscard]] std::expected<std::optional<LuaValue>, StarException> callScript(String const& func, LuaVariadic<LuaValue> const& args) override;
  [[nodiscard]] std::expected<std::optional<LuaValue>, StarException> evalScript(String const& code) override;

  [[nodiscard]] virtual std::expected<Vec2F, StarException> mouthPosition() const override;
  [[nodiscard]] virtual std::expected<Vec2F, StarException> mouthPosition(bool ignoreAdjustments) const override;
  virtual List<ChatAction> pullPendingChatActions() override;

  [[nodiscard]] std::expected<List<PhysicsForceRegion>, StarException> forceRegions() const override;

  [[nodiscard]] std::expected<InteractAction, StarException> interact(InteractRequest const& request) override;
  bool isInteractive() const override;

  [[nodiscard]] std::expected<Vec2F, StarException> questIndicatorPosition() const override;

  ActorMovementController& movementController() override;
  StatusController& statusController() override;

  using Entity::setKeepAlive;
  using Entity::setUniqueId;

private:
  explicit Monster(Root& rootContext);
  [[nodiscard]] std::expected<void, StarException> initialize(MonsterVariant const& monsterVariant, std::optional<float> level);
  [[nodiscard]] std::expected<void, StarException> loadState(Json const& diskStore);

  [[nodiscard]] std::expected<Vec2F, StarException> getAbsolutePosition(Vec2F relativePosition) const;

  [[nodiscard]] std::expected<void, StarException> updateStatus(float dt);
  [[nodiscard]] std::expected<LuaCallbacks, StarException> makeMonsterCallbacks();

  [[nodiscard]] std::expected<void, StarException> addChatMessage(String const& message, String const& portrait = "");

  [[nodiscard]] std::expected<void, StarException> setupNetStates();
  [[nodiscard]] std::expected<void, StarException> getNetStates(bool initial);
  [[nodiscard]] std::expected<void, StarException> setNetStates();

  NetElementTopGroup m_netGroup;

  NetElementData<std::optional<String>> m_uniqueIdNetState;
  NetElementData<EntityDamageTeam> m_teamNetState;
  MonsterVariant m_monsterVariant;
  std::optional<float> m_monsterLevel;

  std::shared_ptr<NetworkedAnimator> m_networkedAnimator;
  NetworkedAnimator::DynamicTarget m_networkedAnimatorDynamicTarget;

  std::shared_ptr<ActorMovementController> m_movementController;
  std::shared_ptr<StatusController> m_statusController;

  std::shared_ptr<EffectEmitter> m_effectEmitter;

  // The set of damage source kinds that were used to kill this entity.
  StringSet m_deathDamageSourceKinds;

  bool m_damageOnTouch;
  bool m_aggressive;

  bool m_knockedOut;
  double m_knockoutTimer;
  String m_deathParticleBurst;
  String m_deathSound;

  String m_activeSkillName;
  Json m_dropPool;

  Vec2F m_questIndicatorOffset;

  List<std::shared_ptr<BehaviorState>> m_behaviors;
  mutable LuaActorScript m_scriptComponent;
  LuaAnimationScript m_scriptedAnimator;

  NetElementData<List<PhysicsForceRegion>> m_physicsForces;

  NetElementData<std::optional<float>> m_monsterLevelNetState;
  NetElementBool m_damageOnTouchNetState;
  NetElementData<StringSet> m_animationDamageParts;
  NetElementData<List<DamageSource>> m_damageSources;
  NetElementData<Json> m_dropPoolNetState;
  NetElementBool m_aggressiveNetState;
  NetElementBool m_knockedOutNetState;
  NetElementString m_deathParticleBurstNetState;
  NetElementString m_deathSoundNetState;
  NetElementString m_activeSkillNameNetState;
  NetElementData<std::optional<String>> m_name;
  NetElementBool m_displayNametag;
  NetElementBool m_interactive;

  List<ChatAction> m_pendingChatActions;
  NetElementEvent m_newChatMessageEvent;
  NetElementString m_chatMessage;
  NetElementString m_chatPortrait;

  NetElementData<DamageBarType> m_damageBar;

  NetElementHashMap<String, Json> m_scriptedAnimationParameters;
};

}


export module star.monster;

export namespace Star {
  using ::Star::Monster;
  using ::Star::StatusController;
  using ::Star::BehaviorState;
}


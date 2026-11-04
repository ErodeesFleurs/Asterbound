module;
import std;
import star.format;
import star.type_identity;
import star.json;
import star.id_map;
import star.casting;
import star.data_stream;
import star.poly;
import star.strong_typedef;
import star.game_types;
import star.list;
import star.rpc_promise;
import star.vector;
import star.rect;
import star.bi_map;
import star.a_star;
import star.string;
import star.color;
import star.image_processing;
import star.hash;
import star.string_view;
import star.thread;
import star.directives;
import star.asset_path;
import star.lua;
import star.random;
import star.periodic_function;
import star.ordered_map;
import star.audio;
import star.map;
import star.set;

import star.net_element_basic_fields;
import star.net_element_float_fields;
import star.net_element_containers;
import star.net_element_dynamic_group;
import star.net_element_system;

import star.net_compatibility;
import star.byte_array;
namespace Star {
class Root;
class Npc;
}

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
import star.platformer_astar_types;
import star.anchorable_entity;
import star.game_timers;
import star.actor_movement_controller;
import star.drawable;
import star.animation;
import star.particle;
import star.animated_part_set;
import star.mixer;
import star.networked_animator;
import star.humanoid;
import star.portrait_entity;
import star.damage_bar_entity;
import star.nametag_entity;
import star.scripted_entity;
import star.chat_action;
import star.chatty_entity;
import star.emote_entity;
import star.entity_rendering_types;
import star.lounging_entities;
import star.mobile_entity;
import star.actor_entity;
import star.tool_user_entity;
import star.lua_components;
import star.lua_actor_movement_component;
import star.lua_animation_component;
import star.npc_database;
import star.effect_emitter;

namespace Star {

class ArmorWearer;
class ToolUser;

class Songbook;
class Item;
class RenderCallback;
class StatusController;
class BehaviorState;

class Npc
  : public virtual DamageBarEntity,
    public virtual PortraitEntity,
    public virtual NametagEntity,
    public virtual ScriptedEntity,
    public virtual ChattyEntity,
    public virtual InteractiveEntity,
    public virtual LoungingEntity,
    public virtual ToolUserEntity,
    public virtual PhysicsEntity,
    public virtual EmoteEntity {
public:
  void const* queryType(TypeKey key) const noexcept override {
    return queryTypeOf<Npc, DamageBarEntity, PortraitEntity, NametagEntity,
        ScriptedEntity, ChattyEntity, InteractiveEntity, LoungingEntity,
        ToolUserEntity, PhysicsEntity, EmoteEntity>(this, key);
  }
  std::string_view dynamicTypeName() const noexcept override {
    return Star::typeName<Npc>();
  }

  [[nodiscard]] static std::expected<std::shared_ptr<Npc>, StarException> create(Root& rootContext, ByteArray const& netStore, NetCompatibilityRules rules = {});
  [[nodiscard]] static std::expected<std::shared_ptr<Npc>, StarException> create(Root& rootContext, NpcVariant const& npcVariant);
  [[nodiscard]] static std::expected<std::shared_ptr<Npc>, StarException> create(Root& rootContext, NpcVariant const& npcVariant, Json const& initialState);

  [[nodiscard]] std::expected<Json, StarException> diskStore() const;
  [[nodiscard]] std::expected<ByteArray, StarException> netStore(NetCompatibilityRules rules = {});

  EntityType entityType() const override;
  ClientEntityMode clientEntityMode() const override;

  [[nodiscard]] std::expected<void, StarException> init(World& world, EntityId entityId, EntityMode mode) override;
  [[nodiscard]] std::expected<void, StarException> uninit() override;

  Vec2F position() const override;
  RectF metaBoundBox() const override;

  [[nodiscard]] std::expected<Vec2F, StarException> mouthOffset(bool ignoreAdjustments = true) const;
  [[nodiscard]] std::expected<Vec2F, StarException> feetOffset() const;
  [[nodiscard]] std::expected<Vec2F, StarException> headArmorOffset() const;
  [[nodiscard]] std::expected<Vec2F, StarException> chestArmorOffset() const;
  [[nodiscard]] std::expected<Vec2F, StarException> legsArmorOffset() const;
  [[nodiscard]] std::expected<Vec2F, StarException> backArmorOffset() const;

  RectF collisionArea() const override;

  [[nodiscard]] std::expected<std::pair<ByteArray, std::uint64_t>, StarException> writeNetState(std::uint64_t fromVersion = 0, NetCompatibilityRules rules = {}) override;
  [[nodiscard]] std::expected<void, StarException> readNetState(ByteArray data, float interpolationTime = 0.0f, NetCompatibilityRules rules = {}) override;

  [[nodiscard]] std::expected<void, StarException> enableInterpolation(float extrapolationHint = 0.0f) override;
  [[nodiscard]] std::expected<void, StarException> disableInterpolation() override;

  [[nodiscard]] std::expected<String, StarException> description() const override;
  String species() const override;
  Gender gender() const;
  String npcType() const;

  [[nodiscard]] std::expected<Json, StarException> scriptConfigParameter(String const& parameterName, Json const& defaultValue = Json()) const;

  [[nodiscard]] std::expected<std::optional<HitType>, StarException> queryHit(DamageSource const& source) const override;
  [[nodiscard]] std::expected<std::optional<PolyF>, StarException> hitPoly() const override;

  [[nodiscard]] std::expected<void, StarException> damagedOther(DamageNotification const& damage) override;

  [[nodiscard]] std::expected<List<DamageNotification>, StarException> applyDamage(DamageRequest const& damage) override;
  [[nodiscard]] std::expected<List<DamageNotification>, StarException> selfDamageNotifications() override;

  [[nodiscard]] std::expected<bool, StarException> shouldDestroy() const override;
  [[nodiscard]] std::expected<void, StarException> destroy(std::optional<std::reference_wrapper<RenderCallback>> renderCallback) override;

  [[nodiscard]] std::expected<void, StarException> update(float dt, std::uint64_t currentVersion) override;

  std::uint64_t presentationEpoch() const override;

  [[nodiscard]] std::expected<void, StarException> collectRenderEvents(RenderCallback& renderCallback) override;

  [[nodiscard]] std::expected<void, StarException> render(RenderCallback& renderCallback) override;

  [[nodiscard]] std::expected<void, StarException> renderLightSources(RenderCallback& renderCallback) override;

  void setPosition(Vec2F const& pos);

  [[nodiscard]] std::expected<float, StarException> maxHealth() const override;
  float health() const override;
  DamageBarType damageBar() const override;

  [[nodiscard]] std::expected<List<Drawable>, StarException> portrait(PortraitMode mode) const override;
  [[nodiscard]] std::expected<String, StarException> name() const override;
  std::optional<String> statusText() const override;
  bool displayNametag() const override;
  [[nodiscard]] std::expected<Vec3B, StarException> nametagColor() const override;
  [[nodiscard]] std::expected<Vec2F, StarException> nametagOrigin() const override;
  [[nodiscard]] std::expected<String, StarException> nametag() const override;

  bool aggressive() const;

  [[nodiscard]] std::expected<std::optional<LuaValue>, StarException> callScript(String const& func, LuaVariadic<LuaValue> const& args) override;
  [[nodiscard]] std::expected<std::optional<LuaValue>, StarException> evalScript(String const& code) override;

  [[nodiscard]] std::expected<Vec2F, StarException> mouthPosition() const override;
  [[nodiscard]] std::expected<Vec2F, StarException> mouthPosition(bool ignoreAdjustments) const override;
  List<ChatAction> pullPendingChatActions() override;

  bool isInteractive() const override;
  [[nodiscard]] std::expected<InteractAction, StarException> interact(InteractRequest const& request) override;
  RectF interactiveBoundBox() const override;

  std::optional<EntityAnchorState> loungingIn() const override;

  [[nodiscard]] std::expected<List<QuestArcDescriptor>, StarException> offeredQuests() const override;
  [[nodiscard]] std::expected<StringSet, StarException> turnInQuests() const override;
  [[nodiscard]] std::expected<Vec2F, StarException> questIndicatorPosition() const override;

  [[nodiscard]] std::expected<List<LightSource>, StarException> lightSources() const override;

  [[nodiscard]] std::expected<std::optional<ChainableJsonMessageResponse>, StarException> receiveMessage(ConnectionId sendingConnection, String const& message, JsonArray const& args) override;

  [[nodiscard]] std::expected<Vec2F, StarException> armPosition(ToolHand hand, Direction facingDirection, float armAngle, Vec2F offset = {}) const override;
  [[nodiscard]] std::expected<Vec2F, StarException> handOffset(ToolHand hand, Direction facingDirection) const override;
  [[nodiscard]] std::expected<Vec2F, StarException> handPosition(ToolHand hand, Vec2F const& handOffset = {}) const override;
  std::shared_ptr<Item> handItem(ToolHand hand) const override;
  [[nodiscard]] std::expected<Vec2F, StarException> armAdjustment() const override;
  Vec2F velocity() const override;
  [[nodiscard]] std::expected<Vec2F, StarException> aimPosition() const override;
  [[nodiscard]] std::expected<float, StarException> interactRadius() const override;
  Direction facingDirection() const override;
  Direction walkingDirection() const override;
  bool isAdmin() const override;
  Color favoriteColor() const override;
  [[nodiscard]] std::expected<float, StarException> beamGunRadius() const override;
  [[nodiscard]] std::expected<void, StarException> addParticles(List<Particle> const& particles) override;
  [[nodiscard]] std::expected<void, StarException> addSound(String const& sound, float volume = 1.0f, float pitch = 1.0f) override;
  [[nodiscard]] std::expected<bool, StarException> inToolRange() const override;
  [[nodiscard]] std::expected<bool, StarException> inToolRange(Vec2F const& position) const override;
  [[nodiscard]] std::expected<void, StarException> addEphemeralStatusEffects(List<EphemeralStatusEffect> const& statusEffects) override;
  [[nodiscard]] std::expected<ActiveUniqueStatusEffectSummary, StarException> activeUniqueStatusEffectSummary() const override;
  [[nodiscard]] std::expected<float, StarException> powerMultiplier() const override;
  [[nodiscard]] std::expected<bool, StarException> fullEnergy() const override;
  [[nodiscard]] std::expected<float, StarException> energy() const override;
  [[nodiscard]] std::expected<bool, StarException> energyLocked() const override;
  [[nodiscard]] std::expected<bool, StarException> consumeEnergy(float energy) override;
  [[nodiscard]] std::expected<void, StarException> queueUIMessage(String const& message) override;
  [[nodiscard]] std::expected<bool, StarException> instrumentPlaying() override;
  [[nodiscard]] std::expected<void, StarException> instrumentEquipped(String const& instrumentKind) override;
  [[nodiscard]] std::expected<void, StarException> interact(InteractAction const& action) override;
  [[nodiscard]] std::expected<void, StarException> addEffectEmitters(StringSet const& emitters) override;
  [[nodiscard]] std::expected<void, StarException> requestEmote(String const& emote) override;
  ActorMovementController& movementController() override;
  StatusController& statusController() override;
  Songbook& songbook();
  void setCameraFocusEntity(std::optional<EntityId> const& cameraFocusEntity) override;

  [[nodiscard]] std::expected<void, StarException> playEmote(HumanoidEmote emote) override;

  [[nodiscard]] std::expected<List<DamageSource>, StarException> damageSources() const override;

  [[nodiscard]] std::expected<List<PhysicsForceRegion>, StarException> forceRegions() const override;

  using Entity::setUniqueId;

  HumanoidIdentity const& identity() const;
  [[nodiscard]] std::expected<void, StarException> updateIdentity();
  [[nodiscard]] std::expected<void, StarException> setIdentity(HumanoidIdentity identity);

  [[nodiscard]] std::expected<void, StarException> setHumanoidParameter(String key, std::optional<Json> value);
  std::optional<Json> getHumanoidParameter(String key);
  [[nodiscard]] std::expected<void, StarException> setHumanoidParameters(JsonObject parameters);
  JsonObject getHumanoidParameters();

  [[nodiscard]] std::expected<void, StarException> setBodyDirectives(String const& directives);
  [[nodiscard]] std::expected<void, StarException> setEmoteDirectives(String const& directives);

  [[nodiscard]] std::expected<void, StarException> setHairGroup(String const& group);
  [[nodiscard]] std::expected<void, StarException> setHairType(String const& type);
  [[nodiscard]] std::expected<void, StarException> setHairDirectives(String const& directives);

  [[nodiscard]] std::expected<void, StarException> setFacialHairGroup(String const& group);
  [[nodiscard]] std::expected<void, StarException> setFacialHairType(String const& type);
  [[nodiscard]] std::expected<void, StarException> setFacialHairDirectives(String const& directives);

  [[nodiscard]] std::expected<void, StarException> setFacialMaskGroup(String const& group);
  [[nodiscard]] std::expected<void, StarException> setFacialMaskType(String const& type);
  [[nodiscard]] std::expected<void, StarException> setFacialMaskDirectives(String const& directives);

  [[nodiscard]] std::expected<void, StarException> setHair(String const& group, String const& type, String const& directives);
  [[nodiscard]] std::expected<void, StarException> setFacialHair(String const& group, String const& type, String const& directives);
  [[nodiscard]] std::expected<void, StarException> setFacialMask(String const& group, String const& type, String const& directives);

  [[nodiscard]] std::expected<void, StarException> setSpecies(String const& species);
  [[nodiscard]] std::expected<void, StarException> setGender(Gender const& gender);
  [[nodiscard]] std::expected<void, StarException> setPersonality(Personality const& personality);
  [[nodiscard]] std::expected<void, StarException> setImagePath(std::optional<String> const& imagePath);

  [[nodiscard]] std::expected<void, StarException> setFavoriteColor(Color color);
  [[nodiscard]] std::expected<void, StarException> setName(String const& name);
  void setDescription(String const& description);

  [[nodiscard]] std::expected<std::shared_ptr<Humanoid>, StarException> humanoid();
  [[nodiscard]] std::expected<std::shared_ptr<Humanoid>, StarException> humanoid() const;

  bool forceNude() const;

private:
  explicit Npc(Root& rootContext);
  [[nodiscard]] std::expected<List<LightSource>, StarException> presentationLightSources(float interpolation) const;
  [[nodiscard]] std::expected<void, StarException> updateHumanoidDirectives();
  DirectivesGroup m_presentationHumanoidDirectives;
  [[nodiscard]] std::expected<void, StarException> initialize(NpcVariant const& npcVariant);
  [[nodiscard]] std::expected<void, StarException> loadState(Json const& diskStore);

  [[nodiscard]] std::expected<Vec2F, StarException> getAbsolutePosition(Vec2F relativePosition) const;

  [[nodiscard]] std::expected<void, StarException> tickShared(float dt);
  [[nodiscard]] std::expected<LuaCallbacks, StarException> makeNpcCallbacks();

  [[nodiscard]] std::expected<void, StarException> setupNetStates();
  [[nodiscard]] std::expected<void, StarException> getNetStates(bool initial);
  [[nodiscard]] std::expected<void, StarException> setNetStates();

  [[nodiscard]] std::expected<void, StarException> addChatMessage(String const& message, Json const& config, String const& portrait = "");
  void addEmote(HumanoidEmote const& emote);
  [[nodiscard]] std::expected<void, StarException> setDance(std::optional<String> const& danceName);

  [[nodiscard]] std::expected<bool, StarException> setItemSlot(String const& slot, ItemDescriptor itemDescriptor);

  [[nodiscard]] std::expected<bool, StarException> canUseTool() const;

  void disableWornArmor(bool disable);

  [[nodiscard]] std::expected<void, StarException> refreshHumanoidParameters();

  NetElementDynamicGroup<NetHumanoid> m_netHumanoid;
  LuaAnimationScript m_scriptedAnimator;
  NetElementHashMap<String, Json> m_scriptedAnimationParameters;
  NetworkedAnimator::DynamicTarget m_humanoidDynamicTarget;

  NpcVariant m_npcVariant;
  NetElementTopGroup m_netGroup;
  NetElementData<StringList> m_dropPools;

  NetElementData<std::optional<String>> m_uniqueIdNetState;
  NetElementData<EntityDamageTeam> m_teamNetState;

  ClientEntityMode m_clientEntityMode;

  NetElementEnum<Humanoid::State> m_humanoidStateNetState;
  NetElementEnum<HumanoidEmote> m_humanoidEmoteStateNetState;
  NetElementData<std::optional<String>> m_humanoidDanceNetState;

  NetElementData<HumanoidIdentity> m_identityNetState;
  NetElementEvent m_refreshedHumanoidParameters;
  bool m_identityUpdated;

  NetElementData<std::optional<String>> m_deathParticleBurst;

  std::shared_ptr<ActorMovementController> m_movementController;
  std::shared_ptr<StatusController> m_statusController;
  std::shared_ptr<EffectEmitter> m_effectEmitter;

  NetElementBool m_aggressive;

  List<std::shared_ptr<BehaviorState>> m_behaviors;
  mutable LuaActorScript m_scriptComponent;

  List<ChatAction> m_pendingChatActions;
  NetElementEvent m_newChatMessageEvent;
  NetElementString m_chatMessage;
  NetElementString m_chatPortrait;
  NetElementData<Json> m_chatConfig;
  bool m_chatMessageUpdated;

  NetElementData<std::optional<String>> m_statusText;
  NetElementBool m_displayNametag;

  HumanoidEmote m_emoteState;
  GameTimer m_emoteCooldownTimer;
  std::optional<String> m_dance;
  GameTimer m_danceCooldownTimer;
  GameTimer m_blinkCooldownTimer;
  Vec2F m_blinkInterval;

  NetElementBool m_isInteractive;

  NetElementData<List<QuestArcDescriptor>> m_offeredQuests;
  NetElementData<StringSet> m_turnInQuests;

  Vec2F m_questIndicatorOffset;

  std::shared_ptr<ArmorWearer> m_armor;
  std::shared_ptr<ToolUser> m_tools;
  std::shared_ptr<Songbook> m_songbook;

  NetElementBool m_disableWornArmor;

  NetElementFloat m_xAimPosition;
  NetElementFloat m_yAimPosition;

  NetElementBool m_shifting;
  NetElementBool m_damageOnTouch;

  int m_hitDamageNotificationLimiter;
  int m_hitDamageNotificationLimit;
};

}


export module star.npc;

export namespace Star {
  using ::Star::ArmorWearer;
  using ::Star::ToolUser;
  using ::Star::Songbook;
  using ::Star::Item;
  using ::Star::RenderCallback;
  using ::Star::Npc;
  using ::Star::StatusController;
}


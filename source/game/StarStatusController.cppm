module;
import std;
import star.format;
import star.game_timers;
import star.lua;
import star.json;
import star.net_compatibility;
import star.list;
import star.observer_stream;
import star.strong_typedef;
import star.data_stream;
import star.id_map;
import star.either;
import star.game_types;
import star.random;
import star.periodic_function;
import star.ordered_map;
import star.string;
import star.color;
import star.image_processing;
import star.hash;
import star.string_view;
import star.thread;
import star.directives;
import star.asset_path;
import star.bi_map;
import star.audio;
import star.map;
import star.set;

import star.status_types;
import star.damage_types;
import star.world_geometry;
import star.damage;
import star.lua_components;
import star.lua_actor_movement_component;
import star.animated_part_set;
import star.drawable;
import star.animation;
import star.particle;
import star.mixer;
import star.light_source;
import star.networked_animator;
import star.entity_rendering_types;
import star.stat_collection;
import star.status_effect_database;

import star.net_element;
import star.net_element_basic_fields;
import star.net_element_float_fields;
import star.net_element_containers;
import star.net_element_dynamic_group;
import star.net_element_ext;
import star.net_element_group;
import star.net_element_sync_group;

namespace Star {

class Root;
class Entity;
class ActorMovementController;


class StatusController : public NetElement {
public:
  [[nodiscard]] static std::expected<std::shared_ptr<StatusController>, StarException> create(Root& root, Json const& config);

  [[nodiscard]] std::expected<Json, StarException> diskStore() const;
  [[nodiscard]] std::expected<void, StarException> diskLoad(Json const& store);

  Json statusProperty(String const& name, Json const& def = Json()) const;
  void setStatusProperty(String const& name, Json value);

  StringList statNames() const;
  float stat(String const& statName) const;
  // Returns true if the stat is strictly greater than zero
  bool statPositive(String const& statName) const;

  StringList resourceNames() const;
  bool isResource(String const& resourceName) const;
  float resource(String const& resourceName) const;
  // Returns true if the resource is strictly greater than zero
  bool resourcePositive(String const& resourceName) const;

  [[nodiscard]] std::expected<void, StarException> setResource(String const& resourceName, float value);
  [[nodiscard]] std::expected<void, StarException> modifyResource(String const& resourceName, float amount);

  float giveResource(String const& resourceName, float amount);

  [[nodiscard]] std::expected<bool, StarException> consumeResource(String const& resourceName, float amount);
  [[nodiscard]] std::expected<bool, StarException> overConsumeResource(String const& resourceName, float amount);

  [[nodiscard]] std::expected<bool, StarException> resourceLocked(String const& resourceName) const;
  [[nodiscard]] std::expected<void, StarException> setResourceLocked(String const& resourceName, bool locked);

  // Resetting a resource also clears any locked states
  [[nodiscard]] std::expected<void, StarException> resetResource(String const& resourceName);
  [[nodiscard]] std::expected<void, StarException> resetAllResources();

  [[nodiscard]] std::expected<std::optional<float>, StarException> resourceMax(String const& resourceName) const;
  [[nodiscard]] std::expected<std::optional<float>, StarException> resourcePercentage(String const& resourceName) const;
  [[nodiscard]] std::expected<float, StarException> setResourcePercentage(String const& resourceName, float resourcePercentage);
  [[nodiscard]] std::expected<float, StarException> modifyResourcePercentage(String const& resourceName, float resourcePercentage);

  List<PersistentStatusEffect> getPersistentEffects(String const& statEffectCategory) const;
  [[nodiscard]] std::expected<void, StarException> addPersistentEffect(String const& statEffectCategory, PersistentStatusEffect const& persistentEffect);
  [[nodiscard]] std::expected<void, StarException> addPersistentEffects(String const& statEffectCategory, List<PersistentStatusEffect> const& persistentEffects);
  [[nodiscard]] std::expected<void, StarException> setPersistentEffects(String const& statEffectCategory, List<PersistentStatusEffect> const& persistentEffects);
  [[nodiscard]] std::expected<void, StarException> clearPersistentEffects(String const& statEffectCategory);
  [[nodiscard]] std::expected<void, StarException> clearAllPersistentEffects();

  [[nodiscard]] std::expected<void, StarException> addEphemeralEffect(EphemeralStatusEffect const& effect, std::optional<EntityId> sourceEntityId = {});
  [[nodiscard]] std::expected<void, StarException> addEphemeralEffects(List<EphemeralStatusEffect> const& effectList, std::optional<EntityId> sourceEntityId = {});
  // Will have no effect if the unique effect is not applied ephemerally
  [[nodiscard]] std::expected<bool, StarException> removeEphemeralEffect(UniqueStatusEffect const& uniqueEffect);
  [[nodiscard]] std::expected<void, StarException> clearEphemeralEffects();

  bool appliesEnvironmentStatusEffects() const;
  void setAppliesEnvironmentStatusEffects(bool appliesEnvironmentStatusEffects);

  // All unique stat effects, whether applied ephemerally or persistently, and
  // their remaining durations.
  ActiveUniqueStatusEffectSummary activeUniqueStatusEffectSummary() const;

  bool uniqueStatusEffectActive(String const& effectName) const;

  const Directives& primaryDirectives() const;
  void setPrimaryDirectives(Directives const& directives);

  // damage request and notification methods should only be called on the master controller.
  [[nodiscard]] std::expected<List<DamageNotification>, StarException> applyDamageRequest(DamageRequest const& damageRequest);
  void hitOther(EntityId targetEntityId, DamageRequest damageRequest);
  void damagedOther(DamageNotification damageNotification);
  List<DamageNotification> pullSelfDamageNotifications();
  [[nodiscard]] std::expected<void, StarException> applySelfDamageRequest(DamageRequest dr);

  // Pulls recent incoming and outgoing damage notifications.  In order for
  // multiple viewers keep track of notifications and avoid duplicates, the
  // damage notifications are indexed by a monotonically increasing 'step'
  // value.  Every call will return the recent damage notifications, along with
  // another step value to pass into the function on the next call to get
  // damage notifications SINCE the first call.  If since is 0, returns all
  // recent notifications available.
  std::pair<List<DamageNotification>, std::uint64_t> damageTakenSince(std::uint64_t since = 0) const;
  std::pair<List<std::pair<EntityId, DamageRequest>>, std::uint64_t> inflictedHitsSince(std::uint64_t since = 0) const;
  std::pair<List<DamageNotification>, std::uint64_t> inflictedDamageSince(std::uint64_t since = 0) const;

  [[nodiscard]] std::expected<void, StarException> init(Entity& parentEntity, ActorMovementController& movementController);
  [[nodiscard]] std::expected<void, StarException> uninit();

  void initNetVersion(std::optional<std::reference_wrapper<NetElementVersion const>> version = std::nullopt) override;

  [[nodiscard]] std::expected<void, StarException> netStore(DataStream& ds, NetCompatibilityRules rules = {}) const override;
  [[nodiscard]] std::expected<void, StarException> netLoad(DataStream& ds, NetCompatibilityRules rules) override;

  [[nodiscard]] std::expected<void, StarException> enableNetInterpolation(float extrapolationHint = 0.0f) override;
  [[nodiscard]] std::expected<void, StarException> disableNetInterpolation() override;
  [[nodiscard]] std::expected<void, StarException> tickNetInterpolation(float dt) override;

  [[nodiscard]] std::expected<bool, StarException> writeNetDelta(DataStream& ds, std::uint64_t fromVersion, NetCompatibilityRules rules = {}) const override;
  [[nodiscard]] std::expected<void, StarException> readNetDelta(DataStream& ds, float interpolationTime = 0.0f, NetCompatibilityRules rules = {}) override;
  [[nodiscard]] std::expected<void, StarException> blankNetDelta(float interpolationTime) override;

  [[nodiscard]] std::expected<void, StarException> tickMaster(float dt);
  [[nodiscard]] std::expected<void, StarException> tickSlave(float dt);

  const DirectivesGroup& parentDirectives() const;
  [[nodiscard]] std::expected<List<Drawable>, StarException> drawables(float interpolation = 1.0f) const;
  [[nodiscard]] std::expected<List<LightSource>, StarException> lightSources(float interpolation = 1.0f) const;
  [[nodiscard]] std::expected<List<OverheadBar>, StarException> overheadBars();
  bool toolUsageSuppressed() const;

  // new audios and particles will only be generated on the client
  List<std::shared_ptr<AudioInstance>> pullNewAudios();
  List<Particle> pullNewParticles();

  [[nodiscard]] std::expected<std::optional<ChainableJsonMessageResponse>, StarException> receiveMessage(String const& message, bool localMessage, JsonArray const& args = {});

private:
  explicit StatusController(Root& root);
  [[nodiscard]] std::expected<void, StarException> configure(Json const& config);
  Root& m_root;
  using StatScript = LuaStatusScript;

  struct EffectAnimator : public NetElement {
    explicit EffectAnimator(Root& root);
    [[nodiscard]] static std::expected<std::shared_ptr<EffectAnimator>, StarException> create(Root& root, std::optional<String> animationConfig = {});

    void initNetVersion(std::optional<std::reference_wrapper<NetElementVersion const>> version = std::nullopt) override;

    [[nodiscard]] std::expected<void, StarException> netStore(DataStream& ds, NetCompatibilityRules rules = {}) const override;
    [[nodiscard]] std::expected<void, StarException> netLoad(DataStream& ds, NetCompatibilityRules rules) override;

    [[nodiscard]] std::expected<void, StarException> enableNetInterpolation(float extrapolationHint = 0.0f) override;
    [[nodiscard]] std::expected<void, StarException> disableNetInterpolation() override;
    [[nodiscard]] std::expected<void, StarException> tickNetInterpolation(float dt) override;

    [[nodiscard]] std::expected<bool, StarException> writeNetDelta(DataStream& ds, std::uint64_t fromVersion, NetCompatibilityRules rules = {}) const override;
    [[nodiscard]] std::expected<void, StarException> readNetDelta(DataStream& ds, float interpolationTime = 0.0f, NetCompatibilityRules rules = {}) override;
    [[nodiscard]] std::expected<void, StarException> blankNetDelta(float interpolationTime) override;

    Root& m_root;
    std::optional<String> animationConfig;
    std::shared_ptr<NetworkedAnimator> animator;
    NetworkedAnimator::DynamicTarget dynamicTarget;
  };
  using EffectAnimatorGroup = NetElementDynamicGroup<EffectAnimator>;

  struct UniqueEffectMetadata : public NetElementSyncGroup {
    UniqueEffectMetadata();
    [[nodiscard]] static std::expected<std::shared_ptr<UniqueEffectMetadata>, StarException> create(UniqueStatusEffect effect = {}, std::optional<float> duration = {}, std::optional<EntityId> sourceEntityId = {});

    [[nodiscard]] std::expected<void, StarException> netElementsNeedLoad(bool full) override;
    [[nodiscard]] std::expected<void, StarException> netElementsNeedStore() override;

    UniqueStatusEffect effect;
    std::optional<float> duration;
    NetElementFloat durationNetState;
    NetElementFloat maxDuration;

    // If the sourceEntityId is not set here, this implies that the cause of
    // the unique effect was the owning entity.
    NetElementData<std::optional<EntityId>> sourceEntityId;
  };
  using UniqueEffectMetadataGroup = NetElementDynamicGroup<UniqueEffectMetadata>;

  struct PersistentEffectCategory {
    std::optional<StatModifierGroupId> modifierEffectsGroupId;
    List<StatModifier> statModifiers;
    HashSet<UniqueStatusEffect> uniqueEffects;
  };

  struct UniqueEffectInstance {
    explicit UniqueEffectInstance(Root& root) : script(root) {}

    UniqueStatusEffectConfig effectConfig;
    Directives parentDirectives;
    HashSet<StatModifierGroupId> modifierGroups;
    StatScript script;
    UniqueEffectMetadataGroup::ElementId metadataId;
    EffectAnimatorGroup::ElementId animatorId;
    bool toolUsageSuppressed;
  };

  [[nodiscard]] std::expected<void, StarException> updateAnimators(float dt);
  [[nodiscard]] std::expected<void, StarException> updatePersistentUniqueEffects();

  [[nodiscard]] std::expected<float, StarException> defaultUniqueEffectDuration(UniqueStatusEffect const& name) const;
  [[nodiscard]] std::expected<bool, StarException> addUniqueEffect(UniqueStatusEffect const& effect, std::optional<float> duration, std::optional<EntityId> sourceEntityId);
  [[nodiscard]] std::expected<void, StarException> removeUniqueEffect(UniqueStatusEffect const& name);

  [[nodiscard]] std::expected<void, StarException> initPrimaryScript();
  [[nodiscard]] std::expected<void, StarException> uninitPrimaryScript();

  [[nodiscard]] std::expected<void, StarException> initUniqueEffectScript(UniqueEffectInstance& uniqueEffect);
  [[nodiscard]] std::expected<void, StarException> uninitUniqueEffectScript(UniqueEffectInstance& uniqueEffect);

  [[nodiscard]] std::expected<LuaCallbacks, StarException> makeUniqueEffectCallbacks(UniqueEffectInstance& uniqueEffect);

  NetElementGroup m_netGroup;
  std::shared_ptr<StatCollection> m_statCollection;
  NetElementOverride<NetElementHashMap<String, Json>> m_statusProperties;
  NetElementData<DirectivesGroup> m_parentDirectives;
  NetElementBool m_toolUsageSuppressed;

  UniqueEffectMetadataGroup m_uniqueEffectMetadata;
  EffectAnimatorGroup m_effectAnimators;

  std::optional<std::reference_wrapper<Entity>> m_parentEntity;
  std::optional<std::reference_wrapper<ActorMovementController>> m_movementController;

  // Members below are only valid on the master entity

  // there are two magic keys used for this map: 'entities' and 'environment' for StatusEffectEntity
  // and environmentally applied persistent status effects, respectively
  StringMap<PersistentEffectCategory> m_persistentEffects;
  StableHashMap<UniqueStatusEffect, UniqueEffectInstance> m_uniqueEffects;
  float m_minimumLiquidStatusEffectPercentage;
  bool m_appliesEnvironmentStatusEffects;
  bool m_appliesWeatherStatusEffects;
  GameTimer m_environmentStatusEffectUpdateTimer;

  std::optional<String> m_primaryAnimationConfig;
  StatScript m_primaryScript;
  Directives m_primaryDirectives;
  EffectAnimatorGroup::ElementId m_primaryAnimatorId;

  List<DamageNotification> m_pendingSelfDamageNotifications;

  ObserverStream<std::pair<EntityId, DamageRequest>> m_recentHitsGiven;
  ObserverStream<DamageNotification> m_recentDamageGiven;
  ObserverStream<DamageNotification> m_recentDamageTaken;
};

}

export module star.status_controller;

export namespace Star {
  using ::Star::StatusController;
}

module;
import star.game_timers;
import std;
import star.type_identity;
import star.format;
import star.json;
import star.net_compatibility;
import star.byte_array;
import star.id_map;
import star.poly;
import star.casting;
import star.vector;
import star.data_stream;
import star.game_types;
import star.strong_typedef;
import star.bi_map;
import star.list;
import star.rpc_promise;
import star.string;
import star.color;
import star.rect;
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
import star.mobile_entity;
import star.animated_part_set;
import star.drawable;
import star.animation;
import star.particle;
import star.mixer;
import star.networked_animator;
import star.movement_controller;
import star.lua_components;
import star.anchorable_entity;
import star.entity_rendering_types;
import star.lounging_entities;
import star.scripted_entity;
import star.lua_animation_component;

import star.net_element_basic_fields;
import star.net_element_containers;
import star.net_element_system;

namespace Star {
class Root;

struct VehicleExceptionTag {
  static constexpr std::string_view name() { return "VehicleException"; }
};
using VehicleException = StarError<VehicleExceptionTag, StarException>;

class Vehicle : 
  public virtual MobileEntity,
  public virtual LoungeableEntity, 
  public virtual InteractiveEntity, 
  public virtual PhysicsEntity, 
  public virtual ScriptedEntity {
public:
  void const* queryType(TypeKey key) const noexcept override {
    return queryTypeOf<Vehicle, MobileEntity, LoungeableEntity, InteractiveEntity,
        PhysicsEntity, ScriptedEntity>(this, key);
  }
  std::string_view dynamicTypeName() const noexcept override {
    return Star::typeName<Vehicle>();
  }

  [[nodiscard]] static std::expected<std::shared_ptr<Vehicle>, StarException> create(Root& rootContext, Json baseConfig, String path, Json dynamicConfig);

  [[nodiscard]] std::expected<String, StarException> name() const override;

  Json baseConfig() const;
  Json dynamicConfig() const;

  [[nodiscard]] std::expected<Json, StarException> diskStore() const;
  [[nodiscard]] std::expected<void, StarException> diskLoad(Json diskStore);

  EntityType entityType() const override;
  ClientEntityMode clientEntityMode() const override;

  [[nodiscard]] std::expected<List<DamageSource>, StarException> damageSources() const override;
  [[nodiscard]] std::expected<std::optional<HitType>, StarException> queryHit(DamageSource const& source) const override;
  [[nodiscard]] std::expected<std::optional<PolyF>, StarException> hitPoly() const override;

  [[nodiscard]] std::expected<List<DamageNotification>, StarException> applyDamage(DamageRequest const& damage) override;
  [[nodiscard]] std::expected<List<DamageNotification>, StarException> selfDamageNotifications() override;

  [[nodiscard]] std::expected<void, StarException> init(World& world, EntityId entityId, EntityMode mode) override;
  [[nodiscard]] std::expected<void, StarException> uninit() override;

  Vec2F position() const override;
  RectF metaBoundBox() const override;
  RectF collisionArea() const override;
  Vec2F velocity() const;

  [[nodiscard]] std::expected<std::pair<ByteArray, std::uint64_t>, StarException> writeNetState(std::uint64_t fromVersion = 0, NetCompatibilityRules rules = {}) override;
  [[nodiscard]] std::expected<void, StarException> readNetState(ByteArray data, float interpolationTime = 0.0f, NetCompatibilityRules rules = {}) override;

  [[nodiscard]] std::expected<void, StarException> enableInterpolation(float extrapolationHint) override;
  [[nodiscard]] std::expected<void, StarException> disableInterpolation() override;

  [[nodiscard]] std::expected<void, StarException> update(float dt, std::uint64_t currentStep) override;

  std::uint64_t presentationEpoch() const override;

  [[nodiscard]] std::expected<void, StarException> collectRenderEvents(RenderCallback& renderer) override;

  [[nodiscard]] std::expected<void, StarException> render(RenderCallback& renderer) override;

  [[nodiscard]] std::expected<void, StarException> renderLightSources(RenderCallback& renderer) override;

  [[nodiscard]] std::expected<List<LightSource>, StarException> lightSources() const override;

  [[nodiscard]] std::expected<bool, StarException> shouldDestroy() const override;
  [[nodiscard]] std::expected<void, StarException> destroy(std::optional<std::reference_wrapper<RenderCallback>> renderCallback) override;

  [[nodiscard]] std::expected<std::optional<ChainableJsonMessageResponse>, StarException> receiveMessage(ConnectionId sendingConnection, String const& message, JsonArray const& args) override;

  RectF interactiveBoundBox() const override;
  bool isInteractive() const override;
  [[nodiscard]] std::expected<InteractAction, StarException> interact(InteractRequest const& request) override;

  std::size_t anchorCount() const override;
  [[nodiscard]] std::expected<std::shared_ptr<LoungeAnchor const>, StarException> loungeAnchor(std::size_t positionIndex) const override;
  [[nodiscard]] std::expected<void, StarException> loungeControl(std::size_t positionIndex, LoungeControl loungeControl) override;
  [[nodiscard]] std::expected<void, StarException> loungeAim(std::size_t positionIndex, Vec2F const& aimPosition) override;

  [[nodiscard]] std::expected<List<PhysicsForceRegion>, StarException> forceRegions() const override;
  std::size_t movingCollisionCount() const override;
  [[nodiscard]] std::expected<std::optional<PhysicsMovingCollision>, StarException> movingCollision(std::size_t positionIndex) const override;

  [[nodiscard]] std::expected<std::optional<LuaValue>, StarException> callScript(String const& func, LuaVariadic<LuaValue> const& args) override;
  [[nodiscard]] std::expected<std::optional<LuaValue>, StarException> evalScript(String const& code) override;
  
  MovementController& movementController() override;

  void setPosition(Vec2F const& position);

private:
  [[nodiscard]] std::expected<std::size_t, StarException> loungeIndex(String const& name) const;
  Vehicle(Root& rootContext);
  [[nodiscard]] std::expected<void, StarException> initialize(Root& rootContext, Json baseConfig, String path, Json dynamicConfig);

  struct MasterControlState {
    Set<ConnectionId> slavesHeld;
    bool masterHeld;
  };

  struct LoungePositionConfig {
    // The NetworkedAnimator part and part property which should control the
    // lounge position.
    String part;
    String partAnchor;
    std::optional<Vec2F> exitBottomOffset;
    JsonObject armorCosmeticOverrides;
    std::optional<String> cursorOverride;
    std::optional<bool> suppressTools;
    bool cameraFocus;

    NetElementBool enabled;
    NetElementEnum<LoungeOrientation> orientation;
    NetElementData<std::optional<String>> emote;
    NetElementData<std::optional<String>> dance;
    NetElementData<std::optional<String>> directives;
    NetElementData<List<PersistentStatusEffect>> statusEffects;

    Map<LoungeControl, MasterControlState> masterControlState;
    Vec2F masterAimPosition;

    Set<LoungeControl> slaveOldControls;
    Vec2F slaveOldAimPosition;
    Set<LoungeControl> slaveNewControls;
    Vec2F slaveNewAimPosition;
  };

  struct MovingCollisionConfig {
    PhysicsMovingCollision movingCollision;
    std::optional<String> attachToPart;
    NetElementBool enabled;
  };

  struct ForceRegionConfig {
    PhysicsForceRegion forceRegion;
    std::optional<String> attachToPart;
    NetElementBool enabled;
  };

  struct DamageSourceConfig {
    DamageSource damageSource;
    std::optional<String> attachToPart;
    NetElementBool enabled;
  };

  enum class VehicleLayer { Back, Passenger, Front };

  EntityRenderLayer renderLayer(VehicleLayer vehicleLayer) const;

  [[nodiscard]] std::expected<LuaCallbacks, StarException> makeVehicleCallbacks();
  [[nodiscard]] std::expected<Json, StarException> configValue(String const& name, Json def = {}) const;

  String m_typeName;
  Json m_baseConfig;
  String m_path;
  Json m_dynamicConfig;
  RectF m_boundBox;
  float m_slaveControlTimeout = 0.0f;
  bool m_receiveExtraControls;
  OrderedHashMap<String, LoungePositionConfig> m_loungePositions;
  OrderedHashMap<String, MovingCollisionConfig> m_movingCollisions;
  OrderedHashMap<String, ForceRegionConfig> m_forceRegions;

  ClientEntityMode m_clientEntityMode;

  NetElementTopGroup m_netGroup;
  NetElementBool m_interactive;
  std::shared_ptr<MovementController> m_movementController;
  std::shared_ptr<NetworkedAnimator> m_networkedAnimator;
  NetworkedAnimator::DynamicTarget m_networkedAnimatorDynamicTarget;
  LuaStoredEntityScript m_scriptComponent;
  
  LuaAnimationScript m_scriptedAnimator;
  NetElementHashMap<String, Json> m_scriptedAnimationParameters;

  Map<ConnectionId, GameTimer> m_aliveMasterConnections;
  bool m_shouldDestroy = false;
  NetElementData<EntityDamageTeam> m_damageTeam;
  OrderedHashMap<String, DamageSourceConfig> m_damageSources;

  EntityRenderLayer m_baseRenderLayer;
  std::optional<EntityRenderLayer> m_overrideRenderLayer;
  
  GameTimer m_slaveHeartbeatTimer;
};

}

export module star.vehicle;

export namespace Star {
  using ::Star::VehicleExceptionTag;
  using ::Star::VehicleException;
  using ::Star::Vehicle;
}

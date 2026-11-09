module;
import std;
import star.format;
import star.type_identity;
import star.game_timers;
import star.string;
import star.json;
import star.net_compatibility;
import star.byte_array;
import star.id_map;
import star.ordered_map;
import star.poly;
import star.casting;
import star.vector;
import star.data_stream;
import star.game_types;
import star.strong_typedef;
import star.list;
import star.rpc_promise;
import star.lua;
import star.color;
import star.bi_map;
import star.rect;
import star.image_processing;
import star.hash;
import star.string_view;
import star.thread;
import star.directives;
import star.asset_path;

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
import star.scripted_entity;
import star.status_effect_entity;
import star.movement_controller;
import star.animation;
import star.particle;
import star.lua_components;
import star.effect_emitter;

import star.data_stream_devices;
import star.net_element_basic_fields;
import star.net_element_system;

import star.line;
namespace Star {
class Root;

class World;
class RenderCallback;
struct ProjectileConfig;
class AudioInstance;

class Projectile : public virtual MobileEntity, public virtual ScriptedEntity, public virtual PhysicsEntity, public virtual StatusEffectEntity {
public:
  void const* queryType(TypeKey key) const noexcept override {
    return queryTypeOf<Projectile, MobileEntity, ScriptedEntity, PhysicsEntity,
        StatusEffectEntity>(this, key);
  }
  std::string_view dynamicTypeName() const noexcept override {
    return Star::typeName<Projectile>();
  }

  [[nodiscard]] static std::expected<std::shared_ptr<Projectile>, StarException> create(Root& rootContext, std::shared_ptr<ProjectileConfig> const& config, Json const& parameters);
  [[nodiscard]] static std::expected<std::shared_ptr<Projectile>, StarException>
  create(Root& rootContext, std::shared_ptr<ProjectileConfig> const& config, DataStream& netState,
         NetCompatibilityRules rules = {});

  [[nodiscard]] std::expected<ByteArray, StarException> netStore(NetCompatibilityRules rules = {}) const;

  EntityType entityType() const override;

  [[nodiscard]] std::expected<void, StarException> init(World& world, EntityId entityId, EntityMode mode) override;
  [[nodiscard]] std::expected<void, StarException> uninit() override;

  String typeName() const;
  [[nodiscard]] std::expected<String, StarException> name() const override;
  [[nodiscard]] std::expected<String, StarException> description() const override;

  Vec2F position() const override;
  RectF metaBoundBox() const override;

  Vec2F velocity() const;

  bool ephemeral() const override;
  ClientEntityMode clientEntityMode() const override;
  bool masterOnly() const override;

  [[nodiscard]] std::expected<std::pair<ByteArray, std::uint64_t>, StarException> writeNetState(std::uint64_t fromVersion = 0, NetCompatibilityRules rules = {}) override;
  [[nodiscard]] std::expected<void, StarException> readNetState(ByteArray data, float interpolationTime = 0.0f, NetCompatibilityRules rules = {}) override;

  [[nodiscard]] std::expected<void, StarException> enableInterpolation(float extrapolationHint = 0.0f) override;
  [[nodiscard]] std::expected<void, StarException> disableInterpolation() override;

  // If the bullet time to live has run out, or if it has collided, etc this
  // will return true.
  [[nodiscard]] std::expected<bool, StarException> shouldDestroy() const override;
  [[nodiscard]] std::expected<void, StarException> destroy(std::optional<std::reference_wrapper<RenderCallback>> renderCallback) override;

  [[nodiscard]] std::expected<List<DamageSource>, StarException> damageSources() const override;
  [[nodiscard]] std::expected<void, StarException> hitOther(EntityId targetEntityId, DamageRequest const& dr) override;

  [[nodiscard]] std::expected<void, StarException> update(float dt, std::uint64_t currentStep) override;
  std::uint64_t presentationEpoch() const override;
  [[nodiscard]] std::expected<void, StarException> collectRenderEvents(RenderCallback& renderCallback) override;
  [[nodiscard]] std::expected<void, StarException> render(RenderCallback& renderCallback) override;
  [[nodiscard]] std::expected<void, StarException> renderLightSources(RenderCallback& renderCallback) override;

  [[nodiscard]] std::expected<std::optional<ChainableJsonMessageResponse>, StarException> receiveMessage(ConnectionId sendingConnection, String const& message, JsonArray const& args) override;

  [[nodiscard]] std::expected<std::optional<LuaValue>, StarException> callScript(String const& func, LuaVariadic<LuaValue> const& args) override;
  [[nodiscard]] std::expected<std::optional<LuaValue>, StarException> evalScript(String const& code) override;

  String projectileType() const;
  
  [[nodiscard]] std::expected<Json, StarException> configValue(String const& name, Json const& def = Json()) const;

  // InitialPosition, InitialDirection, InitialVelocity, PowerMultiplier, and
  // additional status effects must be set before the projectile is added to
  // the world

  float initialSpeed() const;
  void setInitialSpeed(float speed);

  void setInitialPosition(Vec2F const& position);
  void setInitialDirection(Vec2F const& direction);
  // Overrides internal "speed" parameter
  void setInitialVelocity(Vec2F const& velocity);

  void setReferenceVelocity(std::optional<Vec2F> const& velocity);

  float powerMultiplier() const;
  void setPowerMultiplier(float multiplier);

  // If trackSource is true, then the projectile will (while the entity exists)
  // attempt to track the change in position of the parent entity and move
  // relative to it.
  void setSourceEntity(EntityId source, bool trackSource);
  EntityId sourceEntity() const;

  [[nodiscard]] std::expected<List<PersistentStatusEffect>, StarException> statusEffects() const override;
  PolyF statusEffectArea() const override;

  [[nodiscard]] std::expected<List<PhysicsForceRegion>, StarException> forceRegions() const override;
  std::size_t movingCollisionCount() const override;
  [[nodiscard]] std::expected<std::optional<PhysicsMovingCollision>, StarException> movingCollision(std::size_t positionIndex) const override;

  using Entity::setTeam;
  
  MovementController& movementController() override;

private:
  Projectile(Root& rootContext);
  [[nodiscard]] std::expected<void, StarException> initialize(std::shared_ptr<ProjectileConfig> const& config,
                                                              DataStream& data, NetCompatibilityRules rules);
  [[nodiscard]] std::expected<void, StarException> initialize(std::shared_ptr<ProjectileConfig> const& config, Json const& parameters);

  struct PhysicsForceConfig {
    PhysicsForceRegion forceRegion;
    NetElementBool enabled;
  };

  struct PhysicsCollisionConfig {
    PhysicsMovingCollision movingCollision;
    NetElementBool enabled;
  };

  [[nodiscard]] static std::expected<List<Particle>, StarException> sparkBlock(Root& rootContext, World& world, Vec2I const& position, Vec2F const& damageSource);

  int getFrame() const;
  void setFrame(int frame);
  [[nodiscard]] std::expected<String, StarException> drawableFrame();

  [[nodiscard]] std::expected<void, StarException> processAction(Json const& action);
  [[nodiscard]] std::expected<void, StarException> tickShared(float dt);

  [[nodiscard]] std::expected<void, StarException> setup();

  [[nodiscard]] std::expected<LuaCallbacks, StarException> makeProjectileCallbacks();

  [[nodiscard]] std::expected<void, StarException> collectPendingRenderEvents(RenderCallback& renderCallback);
  int frameAt(float animationTimer, float timeToLive) const;

  std::shared_ptr<ProjectileConfig> m_config;
  Json m_parameters;

  // used when projectiles are fired from a moving entity and should include its velocity
  std::optional<Vec2F> m_referenceVelocity;

  // Individual projectile parameters.  Defaults come from m_config, but can be
  // overridden by parameters.
  float m_acceleration;
  float m_initialSpeed;
  float m_power;
  float m_powerMultiplier;
  Directives m_imageDirectives;
  String m_imageSuffix;
  Json m_damageTeam;
  String m_damageKind;
  DamageType m_damageType;
  std::optional<String> m_damageRepeatGroup;
  std::optional<float> m_damageRepeatTimeout;

  bool m_rayCheckToSource;
  bool m_falldown;
  bool m_hydrophobic;
  bool m_onlyHitTerrain;

  std::optional<String> m_collisionSound;
  String m_persistentAudioFile;
  std::shared_ptr<AudioInstance> m_persistentAudio;

  List<std::tuple<GameTimer, bool, Json>> m_periodicActions;

  NetElementTopGroup m_netGroup;
  std::shared_ptr<MovementController> m_movementController;
  std::shared_ptr<EffectEmitter> m_effectEmitter;
  float m_timeToLive;

  Line2F m_travelLine;
  EntityId m_sourceEntity;
  bool m_trackSourceEntity;
  Vec2F m_lastEntityPosition;

  int m_bounces;

  int m_frame;
  float m_animationTimer;
  float m_animationCycle;
  float m_previousAnimationTimer = 0.0f;
  float m_previousTimeToLive = 0.0f;

  // not quite the same thing as m_collision, used for triggering actionOnCollide
  bool m_wasColliding = false;
  NetElementEvent m_collisionEvent;

  bool m_collision;
  Vec2I m_collisionTile;
  Vec2I m_lastNonCollidingTile;

  mutable LuaEntityScript m_scriptComponent;

  OrderedHashMap<String, PhysicsForceConfig> m_physicsForces;
  OrderedHashMap<String, PhysicsCollisionConfig> m_physicsCollisions;

  List<std::variant<std::shared_ptr<AudioInstance>, Particle, LightSource>> m_pendingRenderables;
  List<LightSource> m_renderLights;
};

}

export module star.projectile;

export namespace Star {
  using ::Star::World;
  using ::Star::RenderCallback;
  using ::Star::ProjectileConfig;
  using ::Star::Projectile;
}

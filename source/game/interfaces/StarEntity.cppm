module;
import std;
import star.type_identity;
import star.string;
import star.list;
import star.format;
import star.json;
import star.net_compatibility;
import star.byte_array;
import star.id_map;
import star.casting;
import star.vector;
import star.data_stream;
import star.game_types;
import star.poly;
import star.strong_typedef;
import star.bi_map;

import star.damage_types;
import star.world_geometry;
import star.status_types;
import star.damage;

import star.light_source;

import star.rect;
import star.algorithm;
namespace Star {

class RenderCallback;
class Root;
class World;
struct DamageNotification;

struct EntityExceptionTag {
  static constexpr std::string_view name() { return "EntityException"; }
};
using EntityException = StarError<EntityExceptionTag, StarException>;

// Specifies how the client should treat an entity created on the client,
// whether it should always be sent to the server and be a slave on the client,
// whether it is allowed to be master on the client, and whether client master
// entities should contribute to client presence.
enum class ClientEntityMode {
  // Always a slave on the client
  ClientSlaveOnly,
  // Can be a master on the client
  ClientMasterAllowed,
  // Can be a master on the client, and when it is contributes to client
  // presence.
  ClientPresenceMaster
};
extern std::expected<EnumMap<ClientEntityMode>, StarException> const ClientEntityModeNames;

// The top-level entity type.  The enum order is intended to be in the order in
// which entities should be updated every tick
enum class EntityType : std::uint8_t {
  Plant,
  Object,
  Vehicle,
  ItemDrop,
  PlantDrop,
  Projectile,
  Stagehand,
  Monster,
  Npc,
  Player
};
extern std::expected<EnumMap<EntityType>, StarException> const EntityTypeNames;

class Entity {
public:
  virtual void const* query_type(TypeKey key) const noexcept { return query_type_of<Entity>(this, key); }
  virtual std::string_view dynamic_type_name() const noexcept { return Star::type_name<Entity>(); }

  virtual ~Entity();

  Root& root() const;

  virtual EntityType entityType() const = 0;

  // Called when an entity is first inserted into a World.  Calling base class
  // init binds the world, entityId, and entityMode.
  [[nodiscard]] virtual std::expected<void, StarException> init(World& world, EntityId entityId, EntityMode mode);

  // Should do whatever steps necessary to take an entity out of a world,
  // default implementation clears the world binding, entityMode, and entityId.
  [[nodiscard]] virtual std::expected<void, StarException> uninit();

  // Write state data that changes over time, and is used to keep slaves in
  // sync.  Can return empty and this is the default.  May be called
  // uninitalized.  Should return the delta to be written to the slave, along
  // with the version to pass into writeDeltaState on the next call.  The first
  // delta written to a slave entity will always be the delta starting with 0.
  [[nodiscard]] virtual std::expected<std::pair<ByteArray, std::uint64_t>, StarException> writeNetState(std::uint64_t fromVersion = 0, NetCompatibilityRules rules = {});
  // Will be called with deltas written by writeDeltaState, including if the
  // delta is empty.  interpolationTime will be provided if interpolation is
  // enabled.
  [[nodiscard]] virtual std::expected<void, StarException> readNetState(ByteArray data, float interpolationTime = 0.0f, NetCompatibilityRules rules = {});

  [[nodiscard]] virtual std::expected<void, StarException> enableInterpolation(float extrapolationHint);
  [[nodiscard]] virtual std::expected<void, StarException> disableInterpolation();

  // Base position of this entity, bound boxes, drawables, and other entity
  // positions are relative to this.
  virtual Vec2F position() const = 0;

  // Largest bounding-box of this entity.  Any damage boxes / drawables / light
  // or sound *sources* must be contained within this bounding box.  Used for
  // all top-level spatial queries.
  virtual RectF metaBoundBox() const = 0;

  // By default returns a null rect, if non-null, it defines the area around
  // this entity where it is likely for the entity to physically collide with
  // collision geometry.
  virtual RectF collisionArea() const;

  // Should this entity allow object / block placement over it, and can the
  // entity immediately be despawned without terribly bad effects?
  virtual bool ephemeral() const;

  // How should this entity be treated if created on the client?  Defaults to
  // ClientSlave.
  virtual ClientEntityMode clientEntityMode() const;
  // Should this entity only exist on the master side?
  virtual bool masterOnly() const;

  [[nodiscard]] virtual std::expected<String, StarException> name() const;
  [[nodiscard]] virtual std::expected<String, StarException> description() const;

  // Gameplay affecting light sources (separate from light sources added during
  // rendering)
  [[nodiscard]] virtual std::expected<List<LightSource>, StarException> lightSources() const;

  // All damage sources for this frame.
  [[nodiscard]] virtual std::expected<List<DamageSource>, StarException> damageSources() const;

  // Return the damage that would result from being hit by the given damage
  // source.  Will be called on master and slave entities.  Culling based on
  // team damage and self damage will be done outside of this query.
  [[nodiscard]] virtual std::expected<std::optional<HitType>, StarException> queryHit(DamageSource const& source) const;

  // Return the polygonal area in which the entity can be hit. Not used for
  // actual hit computation, only for determining more precisely where a
  // hit intersection occurred (e.g. by projectiles)
  [[nodiscard]] virtual std::expected<std::optional<PolyF>, StarException> hitPoly() const;

  // Apply a request to damage this entity. Will only be called on Master
  // entities. DamageRequest might be adjusted based on protection and other
  // effects
  [[nodiscard]] virtual std::expected<List<DamageNotification>, StarException> applyDamage(DamageRequest const& damage);

  // Pull any pending damage notifications applied internally, only called on
  // Master entities.
  [[nodiscard]] virtual std::expected<List<DamageNotification>, StarException> selfDamageNotifications();

  // Called on master entities when a DamageRequest has been generated due to a
  // DamageSource from this entity being applied to another entity.  Will be
  // called on the *causing* entity of the damage.
  [[nodiscard]] virtual std::expected<void, StarException> hitOther(EntityId targetEntityId, DamageRequest const& damageRequest);

  // Called on master entities when this entity has damaged another entity.
  // Only called on the *source entity* of the damage, which may be different
  // than the causing entity.
  [[nodiscard]] virtual std::expected<void, StarException> damagedOther(DamageNotification const& damage);

  // Returning true here indicates that this entity should be removed from the
  // world, default returns false.
  [[nodiscard]] virtual std::expected<bool, StarException> shouldDestroy() const;
  // Will be called once before removing the entity from the World on both
  // master and slave entities. The callback is absent on the server.
  [[nodiscard]] virtual std::expected<void, StarException> destroy(std::optional<std::reference_wrapper<RenderCallback>> renderCallback);

  // Entities can send other entities potentially remote messages and get
  // responses back from them, and should implement this to receive and respond
  // to messages.  If the message is NOT handled, should return Nothing,
  // otherwise should return some Json value.
  // This will only ever be called on master entities.
  [[nodiscard]] virtual std::expected<std::optional<ChainableJsonMessageResponse>, StarException> receiveMessage(ConnectionId sendingConnection, String const& message, JsonArray const& args);

  [[nodiscard]] virtual std::expected<void, StarException> update(float dt, std::uint64_t currentStep);

  // Once-only client presentation effects, collected after each fixed tick even
  // when the entity is outside the display window.
  [[nodiscard]] virtual std::expected<void, StarException> collectRenderEvents(RenderCallback& renderer);

  virtual std::uint64_t presentationEpoch() const;
  void resetPresentation();

  [[nodiscard]] virtual std::expected<void, StarException> render(RenderCallback& renderer);

  [[nodiscard]] virtual std::expected<void, StarException> renderLightSources(RenderCallback& renderer);

  EntityId entityId() const;

  EntityDamageTeam getTeam() const;

  // Returns true if an entity is initialized in a world, and thus has a valid
  // world binding, entity id, and entity mode.
  bool inWorld() const;

  // Reports an error if not currently in a world.
  [[nodiscard]] std::expected<std::reference_wrapper<World>, StarException> world() const;
  // Returns empty if not currently in a world.
  std::optional<std::reference_wrapper<World>> maybeWorld() const;

  // Specifies if the entity is to be saved to disk alongside the sector or
  // despawned.
  bool persistent() const;

  // Entity should keep any sector it is in alive.  Default implementation
  // returns false.
  bool keepAlive() const;

  // If set, then the entity will be discoverable by its unique id and will be
  // indexed in the stored world.  Unique ids must be different across all
  // entities in a single world.
  std::optional<String> uniqueId() const;

  // EntityMode will only be set if the entity is initialized, if the entity is
  // uninitialized then isMaster and isSlave will both return false.
  std::optional<EntityMode> entityMode() const;
  bool isMaster() const;
  bool isSlave() const;

protected:
  explicit Entity(Root& root);
  void capturePresentationRotation(float rotation);
  float samplePresentationRotation(float rotation, float interpolation) const;

  void setPersistent(bool persistent);
  void setKeepAlive(bool keepAlive);
  void setUniqueId(std::optional<String> uniqueId);
  void setTeam(EntityDamageTeam newTeam);

private:
  Root& m_root;
  EntityId m_entityId;
  std::optional<EntityMode> m_entityMode;
  bool m_persistent;
  bool m_keepAlive;
  std::optional<String> m_uniqueId;
  std::optional<std::reference_wrapper<World>> m_world;
  EntityDamageTeam m_team;
  std::uint64_t m_presentationEpoch = 0;
  std::optional<float> m_previousPresentationRotation;
};

template <typename EntityT>
using EntityCallbackOf = std::function<std::expected<void, StarException>(std::shared_ptr<EntityT> const&)>;

template <typename EntityT>
using EntityFilterOf = std::function<std::expected<bool, StarException>(std::shared_ptr<EntityT> const&)>;

using EntityCallback = EntityCallbackOf<Entity>;
using EntityFilter = EntityFilterOf<Entity>;

// Filters based first on dynamic casting to the given type, then optionally on
// the given derived type filter.
template <typename EntityT>
EntityFilter entityTypeFilter(EntityFilterOf<EntityT> filter = {}) {
  return [filter](std::shared_ptr<Entity> const& e) -> std::expected<bool, StarException> {
    if (auto entity = as<EntityT>(e)) {
      if (!filter)
        return true;
      return filter(entity);
    } else {
      return false;
    }
  };
}
}

export module star.entity;

export namespace Star {
  using ::Star::RenderCallback;
  using ::Star::World;
  using ::Star::DamageNotification;
  using ::Star::Entity;
  using ::Star::EntityExceptionTag;
  using ::Star::EntityException;
  using ::Star::ClientEntityMode;
  using ::Star::ClientEntityModeNames;
  using ::Star::EntityType;
  using ::Star::EntityTypeNames;
  using ::Star::EntityCallbackOf;
  using ::Star::EntityFilterOf;
  using ::Star::EntityCallback;
  using ::Star::EntityFilter;
  using ::Star::entityTypeFilter;
}

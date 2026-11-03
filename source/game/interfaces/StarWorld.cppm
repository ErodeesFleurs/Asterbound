module;
import std;
import star.format;
import star.type_identity;
import star.string;
import star.lua;
import star.json;
import star.id_map;
import star.poly;
import star.casting;
import star.vector;
import star.data_stream;
import star.game_types;
import star.liquid_types;
import star.material_types;
import star.strong_typedef;
import star.bi_map;
import star.list;
import star.rpc_promise;


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

import star.line;
import star.rect;
namespace Star {

class World;
class Root;
class TileEntity;
class ScriptedEntity;

typedef std::function<std::expected<void, StarException>(World&)> WorldAction;

class World {
public:
  virtual void const* queryType(TypeKey key) const noexcept { return queryTypeOf<World>(this, key); }
  virtual std::string_view dynamicTypeName() const noexcept { return Star::typeName<World>(); }

  explicit World(Root& root) : m_root(root) {}
  Root& root() const { return m_root; }
  virtual ~World() {}

  // Will remain constant throughout the life of the world.
  virtual ConnectionId connection() const = 0;
  virtual WorldGeometry geometry() const = 0;

  // Update frame counter.  Returns the frame that is *currently* being
  // updated, not the *last* frame, so during the first call to update(), this
  // would return 1
  virtual std::uint64_t currentStep() const = 0;

  // All methods that take int parameters wrap around or clamp so that all int
  // values are valid world indexes.

  virtual MaterialId material(Vec2I const& position, TileLayer layer) const = 0;
  virtual MaterialHue materialHueShift(Vec2I const& position, TileLayer layer) const = 0;
  virtual ModId mod(Vec2I const& position, TileLayer layer) const = 0;
  virtual MaterialHue modHueShift(Vec2I const& position, TileLayer layer) const = 0;
  virtual MaterialColorVariant colorVariant(Vec2I const& position, TileLayer layer) const = 0;
  virtual LiquidLevel liquidLevel(Vec2I const& pos) const = 0;
  [[nodiscard]] virtual std::expected<LiquidLevel, StarException> liquidLevel(RectF const& region) const = 0;

  // Tests a tile modification list and returns the ones that are valid.
  [[nodiscard]] virtual std::expected<TileModificationList, StarException> validTileModifications(TileModificationList const& modificationList, bool allowEntityOverlap) const = 0;
  // Apply a list of tile modifications in the best order to apply as many
  // possible, and returns the modifications that could not be applied.
  [[nodiscard]] virtual std::expected<TileModificationList, StarException> applyTileModifications(TileModificationList const& modificationList, bool allowEntityOverlap) = 0;
  // Swap existing tiles for ones defined in the modification list,
  // and returns the modifications that could not be applied.
  [[nodiscard]] virtual std::expected<TileModificationList, StarException> replaceTiles(TileModificationList const& modificationList, TileDamage const& tileDamage, bool applyDamage = false) = 0;
  // If an applied damage would destroy a tile
  [[nodiscard]] virtual std::expected<bool, StarException> damageWouldDestroy(Vec2I const& pos, TileLayer layer, TileDamage const& tileDamage) const = 0;

  virtual bool isTileProtected(Vec2I const& pos) const = 0;

  virtual std::shared_ptr<Entity> entity(EntityId entityId) const = 0;
  // *If* the entity is initialized immediately and locally, then will use the
  // passed in pointer directly and initialize it, and entity will have a valid
  // id in this world and be ready for use.  This is always the case on the
  // server, but not *always* the case on the client.
  [[nodiscard]] virtual std::expected<void, StarException> addEntity(std::shared_ptr<Entity> const& entity, EntityId entityId = NullEntityId) = 0;

  [[nodiscard]] virtual std::expected<std::shared_ptr<Entity>, StarException> closestEntity(Vec2F const& center, float radius, EntityFilter selector = {}) const = 0;

  [[nodiscard]] virtual std::expected<void, StarException> forAllEntities(EntityCallback entityCallback) const = 0;

  // Query here is a fuzzy query based on metaBoundBox
  [[nodiscard]] virtual std::expected<void, StarException> forEachEntity(RectF const& boundBox, EntityCallback entityCallback) const = 0;
  // Fuzzy metaBoundBox query for intersecting the given line.
  [[nodiscard]] virtual std::expected<void, StarException> forEachEntityLine(Vec2F const& begin, Vec2F const& end, EntityCallback entityCallback) const = 0;
  // Performs action for all entities that occupies the given tile position
  // (only entity types laid out in the tile grid).
  [[nodiscard]] virtual std::expected<void, StarException> forEachEntityAtTile(Vec2I const& pos, EntityCallbackOf<TileEntity> entityCallback) const = 0;

  // Like forEachEntity, but stops scanning when entityFilter returns true, and
  // returns the shared entity pointer found, otherwise returns a null pointer.
  [[nodiscard]] virtual std::expected<std::shared_ptr<Entity>, StarException> findEntity(RectF const& boundBox, EntityFilter entityFilter) const = 0;
  [[nodiscard]] virtual std::expected<std::shared_ptr<Entity>, StarException> findEntityLine(Vec2F const& begin, Vec2F const& end, EntityFilter entityFilter) const = 0;
  [[nodiscard]] virtual std::expected<std::shared_ptr<Entity>, StarException> findEntityAtTile(Vec2I const& pos, EntityFilterOf<TileEntity> entityFilter) const = 0;

  // Is the given tile layer and position occupied by an entity or block?
  [[nodiscard]] virtual std::expected<bool, StarException> tileIsOccupied(Vec2I const& pos, TileLayer layer, bool includeEphemeral = false, bool checkCollision = false) const = 0;

  // Returns the collision kind of a tile.
  [[nodiscard]] virtual std::expected<CollisionKind, StarException> tileCollisionKind(Vec2I const& pos) const = 0;

  // Iterate over the collision block for each tile in the region.  Collision
  // polys for tiles can extend to a maximum of 1 tile outside of the natural
  // tile bounds.
  [[nodiscard]] virtual std::expected<void, StarException> forEachCollisionBlock(RectI const& region, std::function<std::expected<void, StarException>(CollisionBlock const&)> const& iterator) const = 0;

  // Is there some connectable tile / tile based entity in this position?  If
  // tilesOnly is true, only checks to see whether that tile is a connectable
  // material.
  [[nodiscard]] virtual std::expected<bool, StarException> isTileConnectable(Vec2I const& pos, TileLayer layer, bool tilesOnly = false) const = 0;

  // Returns whether or not a given point is inside any colliding tile.  If
  // collisionSet is Dynamic or Static, then does not intersect with platforms.
  [[nodiscard]] virtual std::expected<bool, StarException> pointTileCollision(Vec2F const& point, CollisionSet const& collisionSet = DefaultCollisionSet) const = 0;

  // Returns whether line intersects with any colliding tiles.
  [[nodiscard]] virtual std::expected<bool, StarException> lineTileCollision(Vec2F const& begin, Vec2F const& end, CollisionSet const& collisionSet = DefaultCollisionSet) const = 0;
  [[nodiscard]] virtual std::expected<std::optional<std::pair<Vec2F, Vec2I>>, StarException> lineTileCollisionPoint(Vec2F const& begin, Vec2F const& end, CollisionSet const& collisionSet = DefaultCollisionSet) const = 0;

  // Returns a list of all the collidable tiles along the given line.
  [[nodiscard]] virtual std::expected<List<Vec2I>, StarException> collidingTilesAlongLine(Vec2F const& begin, Vec2F const& end, CollisionSet const& collisionSet = DefaultCollisionSet, int maxSize = -1, bool includeEdges = true) const = 0;

  // Returns whether the given rect contains any colliding tiles.
  [[nodiscard]] virtual std::expected<bool, StarException> rectTileCollision(RectI const& region, CollisionSet const& collisionSet = DefaultCollisionSet) const = 0;

  // Damage multiple tiles, avoiding duplication (objects or plants that occupy
  // more than one tile
  // position are only damaged once)
  [[nodiscard]] virtual std::expected<TileDamageResult, StarException> damageTiles(List<Vec2I> const& tilePositions, TileLayer layer, Vec2F const& sourcePosition, TileDamage const& tileDamage, std::optional<EntityId> sourceEntity = {}) = 0;

  [[nodiscard]] virtual std::expected<std::shared_ptr<InteractiveEntity>, StarException> getInteractiveInRange(Vec2F const& targetPosition, Vec2F const& sourcePosition, float maxRange) const = 0;
  // Can the target entity be reached from the given position within the given radius?
  [[nodiscard]] virtual std::expected<bool, StarException> canReachEntity(Vec2F const& position, float radius, EntityId targetEntity, bool preferInteractive = true) const = 0;
  [[nodiscard]] virtual std::expected<RpcPromise<InteractAction>, StarException> interact(InteractRequest const& request) = 0;

  [[nodiscard]] virtual std::expected<float, StarException> gravity(Vec2F const& pos) const = 0;
  [[nodiscard]] virtual std::expected<float, StarException> windLevel(Vec2F const& pos) const = 0;
  [[nodiscard]] virtual std::expected<float, StarException> lightLevel(Vec2F const& pos) const = 0;
  [[nodiscard]] virtual std::expected<bool, StarException> breathable(Vec2F const& pos) const = 0;
  [[nodiscard]] virtual std::expected<float, StarException> threatLevel() const = 0;
  [[nodiscard]] virtual std::expected<StringList, StarException> environmentStatusEffects(Vec2F const& pos) const = 0;
  [[nodiscard]] virtual std::expected<StringList, StarException> weatherStatusEffects(Vec2F const& pos) const = 0;
  [[nodiscard]] virtual std::expected<bool, StarException> exposedToWeather(Vec2F const& pos) const = 0;
  [[nodiscard]] virtual std::expected<std::optional<String>, StarException> activeWeather(Vec2F const& pos) const = 0;
  [[nodiscard]] virtual std::expected<bool, StarException> isUnderground(Vec2F const& pos) const = 0;
  [[nodiscard]] virtual std::expected<bool, StarException> disableDeathDrops() const = 0;
  [[nodiscard]] virtual std::expected<List<PhysicsForceRegion>, StarException> forceRegions() const = 0;

  // Gets / sets world-wide properties
  [[nodiscard]] virtual std::expected<Json, StarException> getProperty(String const& propertyName, Json const& def = {}) const = 0;
  [[nodiscard]] virtual std::expected<void, StarException> setProperty(String const& propertyName, Json const& property) = 0;

  virtual void timer(float delay, WorldAction worldAction) = 0;
  virtual double epochTime() const = 0;
  virtual std::uint32_t day() const = 0;
  virtual float dayLength() const = 0;
  virtual float timeOfDay() const = 0;

  virtual std::shared_ptr<LuaRoot> luaRoot() = 0;
  [[nodiscard]] virtual std::expected<StringMap<LuaCallbacks>, StarException> luaThreadCallbacks() const = 0;

  // Locate a unique entity, if the target is local, the promise will be
  // finished before being returned.  If the unique entity is not found, the
  // promise will fail.
  [[nodiscard]] virtual std::expected<RpcPromise<Vec2F>, StarException> findUniqueEntity(String const& uniqueEntityId) = 0;

  // Send a message to a local or remote scripted entity.  If the target is
  // local, the promise will be finished before being returned.  Entity id can
  // either be EntityId or a uniqueId.
  [[nodiscard]] virtual std::expected<RpcPromise<Json>, StarException> sendEntityMessage(std::variant<EntityId, String> const& entity, String const& message, JsonArray const& args = {}) = 0;

  // Helper non-virtual methods.

  bool isServer() const;
  bool isClient() const;

  [[nodiscard]] std::expected<List<std::shared_ptr<Entity>>, StarException> entityQuery(RectF const& boundBox, EntityFilter selector = {}) const;
  [[nodiscard]] std::expected<List<std::shared_ptr<Entity>>, StarException> entityLineQuery(Vec2F const& begin, Vec2F const& end, EntityFilter selector = {}) const;

  [[nodiscard]] std::expected<List<std::shared_ptr<TileEntity>>, StarException> entitiesAtTile(Vec2I const& pos, EntityFilter filter = EntityFilter()) const;

  // Find tiles near the given point that are not occupied (according to
  // tileIsOccupied)
  [[nodiscard]] std::expected<List<Vec2I>, StarException> findEmptyTiles(Vec2I pos, unsigned maxDist = 5, std::size_t maxAmount = 1, bool excludeEphemeral = false) const;

  // Do tile modification that only uses a single tile.
  [[nodiscard]] std::expected<bool, StarException> canModifyTile(Vec2I const& pos, TileModification const& modification, bool allowEntityOverlap) const;
  [[nodiscard]] std::expected<bool, StarException> modifyTile(Vec2I const& pos, TileModification const& modification, bool allowEntityOverlap);

  [[nodiscard]] std::expected<TileDamageResult, StarException> damageTile(Vec2I const& tilePosition, TileLayer layer, Vec2F const& sourcePosition, TileDamage const& tileDamage, std::optional<EntityId> sourceEntity = {});

  // Returns closest entity for which lineCollision between the given center
  // position and the entity position returns false.
  [[nodiscard]] std::expected<std::shared_ptr<Entity>, StarException> closestEntityInSight(Vec2F const& center, float radius, CollisionSet const& collisionSet = DefaultCollisionSet, EntityFilter selector = {}) const;

  // Returns whether point collides with any collision geometry.
  [[nodiscard]] std::expected<bool, StarException> pointCollision(Vec2F const& point, CollisionSet const& collisionSet = DefaultCollisionSet) const;

  // Returns first point along line that collides with any collision geometry, along
  // with the normal of the intersected line, if any.
  [[nodiscard]] std::expected<std::optional<std::pair<Vec2F, std::optional<Vec2F>>>, StarException> lineCollision(Line2F const& line, CollisionSet const& collisionSet = DefaultCollisionSet) const;

  // Returns whether poly collides with any collision geometry.
  [[nodiscard]] std::expected<bool, StarException> polyCollision(PolyF const& poly, CollisionSet const& collisionSet = DefaultCollisionSet) const;

  // Helper template methods.  Only queries entities of the given template
  // type, and casts them to the appropriate pointer type.

  template <typename EntityT>
  std::shared_ptr<EntityT> get(EntityId entityId) const;

  template <typename EntityT>
  [[nodiscard]] std::expected<List<std::shared_ptr<EntityT>>, StarException> query(RectF const& boundBox, EntityFilterOf<EntityT> selector = {}) const;

  template <typename EntityT>
  [[nodiscard]] std::expected<std::shared_ptr<EntityT>, StarException> closest(Vec2F const& center, float radius, EntityFilterOf<EntityT> selector = {}) const;

  template <typename EntityT>
  [[nodiscard]] std::expected<std::shared_ptr<EntityT>, StarException> closestInSight(Vec2F const& center, float radius, CollisionSet const& collisionSet, EntityFilterOf<EntityT> selector = {}) const;

  template <typename EntityT>
  [[nodiscard]] std::expected<List<std::shared_ptr<EntityT>>, StarException> lineQuery(Vec2F const& begin, Vec2F const& end, EntityFilterOf<EntityT> selector = {}) const;

  template <typename EntityT>
  [[nodiscard]] std::expected<List<std::shared_ptr<EntityT>>, StarException> atTile(Vec2I const& pos) const;
private:
  Root& m_root;
};

template <typename EntityT>
std::shared_ptr<EntityT> World::get(EntityId entityId) const {
  return as<EntityT>(entity(entityId));
}

template <typename EntityT>
[[nodiscard]] std::expected<List<std::shared_ptr<EntityT>>, StarException> World::query(RectF const& boundBox, EntityFilterOf<EntityT> selector) const {
  List<std::shared_ptr<EntityT>> list;
  auto scanned = forEachEntity(boundBox, [&](std::shared_ptr<Entity> const& entity) -> std::expected<void, StarException> {
    if (auto e = as<EntityT>(entity)) {
      if (selector) {
        auto accepted = selector(e);
        if (!accepted)
          return std::unexpected(std::move(accepted.error()));
        if (!*accepted)
          return {};
      }
      list.append(std::move(e));
    }
    return {};
  });
  if (!scanned)
    return std::unexpected(std::move(scanned.error()));
  return list;
}

template <typename EntityT>
[[nodiscard]] std::expected<std::shared_ptr<EntityT>, StarException> World::closest(Vec2F const& center, float radius, EntityFilterOf<EntityT> selector) const {
  auto found = closestEntity(center, radius, entityTypeFilter<EntityT>(std::move(selector)));
  if (!found)
    return std::unexpected(std::move(found.error()));
  return as<EntityT>(*found);
}

template <typename EntityT>
[[nodiscard]] std::expected<std::shared_ptr<EntityT>, StarException> World::closestInSight(
    Vec2F const& center, float radius, CollisionSet const& collisionSet, EntityFilterOf<EntityT> selector) const {
  auto found = closestEntityInSight(center, radius, collisionSet, entityTypeFilter<EntityT>(std::move(selector)));
  if (!found)
    return std::unexpected(std::move(found.error()));
  return as<EntityT>(*found);
}

template <typename EntityT>
[[nodiscard]] std::expected<List<std::shared_ptr<EntityT>>, StarException> World::lineQuery(
    Vec2F const& begin, Vec2F const& end, EntityFilterOf<EntityT> selector) const {
  List<std::shared_ptr<EntityT>> list;
  auto scanned = forEachEntityLine(begin, end, [&](std::shared_ptr<Entity> const& entity) -> std::expected<void, StarException> {
    if (auto e = as<EntityT>(entity)) {
      if (selector) {
        auto accepted = selector(e);
        if (!accepted)
          return std::unexpected(std::move(accepted.error()));
        if (!*accepted)
          return {};
      }
      list.append(std::move(e));
    }
    return {};
  });
  if (!scanned)
    return std::unexpected(std::move(scanned.error()));
  return list;
}

template <typename EntityT>
[[nodiscard]] std::expected<List<std::shared_ptr<EntityT>>, StarException> World::atTile(Vec2I const& pos) const {
  List<std::shared_ptr<EntityT>> list;
  auto scanned = forEachEntityAtTile(pos, [&](std::shared_ptr<TileEntity> const& entity) -> std::expected<void, StarException> {
    if (auto e = as<EntityT>(entity))
      list.append(std::move(e));
    return {};
  });
  if (!scanned)
    return std::unexpected(std::move(scanned.error()));
  return list;
}

// The caller owns queue extraction and the world lock. A native failure stops
// dispatch, but every keeper in this extracted batch must still be settled.
template <typename Message, typename Receiver>
[[nodiscard]] std::expected<void, StarException> settleWorldMessageBatch(
    List<Message>& messages, Receiver&& receiveMessage) {
  std::optional<StarException> error;
  for (auto& message : messages) {
    if (error) {
      if (auto failed = message.promise.fail("World message handling stopped"); !failed)
        *error = error->withRelated(failed.error(), "World message drain also failed");
      continue;
    }
    auto response = receiveMessage(message.message, message.args);
    std::expected<void, StarException> completion;
    if (!response) {
      error = std::move(response.error());
      completion = message.promise.fail("World message handling failed");
    } else if (!*response) {
      completion = message.promise.fail("Message not handled by world");
    } else if (std::holds_alternative<RpcPromise<Json>>(**response)) {
      completion = message.promise.chain(std::get<RpcPromise<Json>>(**response));
    } else {
      completion = message.promise.fulfill(std::get<Json>(**response));
    }
    if (!completion) {
      if (error)
        *error = error->withRelated(completion.error(), "World message settlement also failed");
      else
        error = std::move(completion.error());
    }
  }
  if (error)
    return std::unexpected(std::move(*error));
  return {};
}
} // namespace Star

export module star.world;

export namespace Star {
  using ::Star::World;
  using ::Star::TileEntity;
  using ::Star::ScriptedEntity;
  using ::Star::WorldAction;
  using ::Star::settleWorldMessageBatch;
}

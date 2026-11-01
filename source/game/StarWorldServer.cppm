module;
import star.type_identity;
import star.game_timers;
import star.json;
import star.xx_hash;
import star.id_map;
import star.list;
import star.bi_map;
import star.casting;
import star.either;
import star.rect;
import star.image;
import star.color;
import star.vector;
import star.thread;
import star.game_types;
import star.material_types;
import star.io_device;
import star.lua;
import star.format;
import star.version;
import star.string;
import star.data_stream;
import star.image_processing;
import star.hash;
import star.string_view;
import star.directives;
import star.asset_path;
import star.random;
import star.weighted_pool;
import star.strong_typedef;
import star.rpc_promise;
import star.set;
import star.worker_pool;

import std;
import star.sector_array_2d;
import star.ordered_map;
import star.net_compatibility;
import star.perlin;
import star.btree;
import star.lru_cache;
import star.data_stream_devices;
import star.btree_database;
import star.ordered_set;
import star.map;

import star.lua_root;
import star.tile_sector_array;
import star.collision_block;
import star.damage_types;
import star.world_geometry;
import star.status_types;
import star.damage;
import star.light_source;
import star.entity;
import star.interaction_types;
import star.item_descriptor;
import star.quest_descriptor;
import star.interactive_entity;
import star.tile_entity;
import star.tile_modification;
import star.force_regions;
import star.world;
import star.liquid_types;
import star.tile_damage;
import star.animation;
import star.particle;
import star.weather_types;
import star.celestial_coordinate;
import star.sky_types;
import star.world_parameters;
import star.celestial_parameters;
import star.world_layout;
import star.collision_generator;
import star.world_tiles;
import star.celestial_types;
import star.chat_types;
import star.uuid;
import star.warping;
import star.wiring;
import star.inspectable_entity;
import star.plant;
import star.plant_database;
import star.biome_placement;
import star.versioning_database;
import star.world_storage;
import star.player_types;
import star.sky_parameters;
import star.system_world;
import star.damage_manager;
import star.net_packets;
import star.cellular_light_array;
import star.cellular_lighting;
import star.cellular_liquid;
import star.world_structure;
import star.lua_components;
import star.drawable;
import star.entity_rendering_types;
import star.sky_render_data;
import star.parallax;
import star.world_render_data;
import star.world_client_state;
import star.interpolation_tracker;
import star.spawn_type_database;
import star.spawner;

import star.byte_array;
namespace Star {

class Root;

class Player;
class WorldTemplate;
class Sky;
class ServerWeather;
struct SkyParameters;
class DamageManager;
class WireProcessor;
class EntityMap;
class WorldStorage;
class FallingBlocksAgent;
class DungeonDefinition;
class TileEntity;
class UniverseSettings;
class UniverseServer;

struct WorldServerExceptionTag {
  static constexpr std::string_view name() { return "WorldServerException"; }
};
using WorldServerException = StarError<WorldServerExceptionTag, StarException>;

// Describes the amount of optional processing that a call to update() in
// WorldServer performs for things like liquid simulation, wiring, sector
// generation etc.
enum class WorldServerFidelity {
  Minimum,
  Low,
  Medium,
  High
};
extern std::expected<EnumMap<WorldServerFidelity>, StarException> const WorldServerFidelityNames;

class WorldServer : public World {
public:
  void const* queryType(TypeKey key) const noexcept override { return queryTypeOf<WorldServer, World>(this, key); }
  std::string_view dynamicTypeName() const noexcept override { return Star::typeName<WorldServer>(); }

  typedef LuaEntityScript ScriptComponent;
  typedef std::function<std::expected<void, StarException>(Json const&)> WorldPropertyListener;

  // Create a new world with the given template, writing new storage file.
  // Failure discards uncommitted storage; only fully initialized worlds escape.
  [[nodiscard]] static std::expected<std::shared_ptr<WorldServer>, StarException> create(Root& root, std::shared_ptr<WorldTemplate> const& worldTemplate, std::shared_ptr<IODevice> storage);
  // Creates the default world template for the given size.
  [[nodiscard]] static std::expected<std::shared_ptr<WorldServer>, StarException> create(Root& root, Vec2U const& size, std::shared_ptr<IODevice> storage);
  // Load an existing world from the given storage files
  [[nodiscard]] static std::expected<std::shared_ptr<WorldServer>, StarException> create(Root& root, std::shared_ptr<IODevice> const& storage);
  // Load an existing world from the given in-memory chunks
  [[nodiscard]] static std::expected<std::shared_ptr<WorldServer>, StarException> create(Root& root, WorldChunks const& chunks);
  ~WorldServer();
  // Final save and teardown; idempotent, including after a reported failure.
  [[nodiscard]] std::expected<void, StarException> close();

  void setWorldId(String worldId);
  String const& worldId() const;

  void setUniverseSettings(std::shared_ptr<UniverseSettings> universeSettings);
  std::shared_ptr<UniverseSettings> universeSettings() const;

  void setPause(bool pause);
  void setReferenceClock(std::shared_ptr<Clock> clock);

  [[nodiscard]] std::expected<void, StarException> initLua(UniverseServer& universe);

  // Give this world a central structure.  If there is a previous central
  // structure it is removed first.  Returns the structure with transformed
  // coordinates.
  [[nodiscard]] std::expected<WorldStructure, StarException> setCentralStructure(WorldStructure centralStructure);
  WorldStructure const& centralStructure() const;
  // If there is an active central structure, it is removed and all unmodified
  // objects and blocks associated with the structure are removed.
  [[nodiscard]] std::expected<void, StarException> removeCentralStructure();

  void setPlayerStart(Vec2F const& startPosition, bool respawnInWorld = false);

  [[nodiscard]] std::expected<bool, StarException> spawnTargetValid(SpawnTarget const& spawnTarget) const;

  // Returns false if the client id already exists, or the spawn target is
  // invalid.
  [[nodiscard]] std::expected<bool, StarException> addClient(ConnectionId clientId, SpawnTarget const& spawnTarget, bool isLocal, bool isAdmin = false, NetCompatibilityRules netRules = {}, ClientSubWorldId subWorldId = MainClientWorldId);

  // Removes client, sends the WorldStopPacket, and returns any pending packets
  // for that client
  [[nodiscard]] std::expected<List<std::shared_ptr<Packet>>, StarException> removeClient(ConnectionId clientId);

  List<ConnectionId> clientIds() const;
  bool hasClient(ConnectionId clientId) const;
  RectF clientWindow(ConnectionId clientId) const;
  // May return null if a Player is not available or if the client id is not
  // valid.
  std::shared_ptr<Player> clientPlayer(ConnectionId clientId) const;
  
  ClientSubWorldId clientSubWorld(ConnectionId clientId) const;

  List<EntityId> players() const;

  [[nodiscard]] std::expected<void, StarException> handleIncomingPackets(ConnectionId clientId, List<std::shared_ptr<Packet>> const& packets);
  [[nodiscard]] std::expected<List<std::shared_ptr<Packet>>, StarException> getOutgoingPackets(ConnectionId clientId);
  bool sendPacket(ConnectionId clientId, std::shared_ptr<Packet> const& packet);

  [[nodiscard]] std::expected<std::optional<ChainableJsonMessageResponse>, StarException> receiveMessage(ConnectionId fromConnection, String const& message, JsonArray const& args);

  [[nodiscard]] std::expected<void, StarException> startFlyingSky(bool enterHyperspace, bool startInWarp, Json settings = {});
  [[nodiscard]] std::expected<void, StarException> stopFlyingSkyAt(SkyParameters const& destination);
  [[nodiscard]] std::expected<void, StarException> setOrbitalSky(SkyParameters const& destination);

  // Defaults to Medium
  WorldServerFidelity fidelity() const;
  [[nodiscard]] std::expected<void, StarException> setFidelity(WorldServerFidelity fidelity);

  bool shouldExpire();
  void setExpiryTime(float expiryTime);
  float expiryTime();

  [[nodiscard]] std::expected<void, StarException> update(float dt);

  ConnectionId connection() const override;
  WorldGeometry geometry() const override;
  std::uint64_t currentStep() const override;
  MaterialId material(Vec2I const& position, TileLayer layer) const override;
  MaterialHue materialHueShift(Vec2I const& position, TileLayer layer) const override;
  ModId mod(Vec2I const& position, TileLayer layer) const override;
  MaterialHue modHueShift(Vec2I const& position, TileLayer layer) const override;
  MaterialColorVariant colorVariant(Vec2I const& position, TileLayer layer) const override;
  LiquidLevel liquidLevel(Vec2I const& pos) const override;
  [[nodiscard]] std::expected<LiquidLevel, StarException> liquidLevel(RectF const& region) const override;

  [[nodiscard]] std::expected<TileModificationList, StarException> validTileModifications(TileModificationList const& modificationList, bool allowEntityOverlap) const override;
  [[nodiscard]] std::expected<TileModificationList, StarException> applyTileModifications(TileModificationList const& modificationList, bool allowEntityOverlap) override;
  [[nodiscard]] std::expected<bool, StarException> replaceTile(Vec2I const& pos, TileModification const& modification, TileDamage const& tileDamage);
  [[nodiscard]] std::expected<TileModificationList, StarException> replaceTiles(TileModificationList const& modificationList, TileDamage const& tileDamage, bool applyDamage = false) override;
  [[nodiscard]] std::expected<bool, StarException> damageWouldDestroy(Vec2I const& pos, TileLayer layer, TileDamage const& tileDamage) const override;
  std::shared_ptr<Entity> entity(EntityId entityId) const override;
  [[nodiscard]] std::expected<void, StarException> addEntity(std::shared_ptr<Entity> const& entity, EntityId entityId = NullEntityId) override;
  [[nodiscard]] std::expected<std::shared_ptr<Entity>, StarException> closestEntity(Vec2F const& center, float radius, EntityFilter selector = EntityFilter()) const override;
  [[nodiscard]] std::expected<void, StarException> forAllEntities(EntityCallback entityCallback) const override;
  [[nodiscard]] std::expected<void, StarException> forEachEntity(RectF const& boundBox, EntityCallback callback) const override;
  [[nodiscard]] std::expected<void, StarException> forEachEntityLine(Vec2F const& begin, Vec2F const& end, EntityCallback callback) const override;
  [[nodiscard]] std::expected<void, StarException> forEachEntityAtTile(Vec2I const& pos, EntityCallbackOf<TileEntity> entityCallback) const override;
  [[nodiscard]] std::expected<std::shared_ptr<Entity>, StarException> findEntity(RectF const& boundBox, EntityFilter entityFilter) const override;
  [[nodiscard]] std::expected<std::shared_ptr<Entity>, StarException> findEntityLine(Vec2F const& begin, Vec2F const& end, EntityFilter entityFilter) const override;
  [[nodiscard]] std::expected<std::shared_ptr<Entity>, StarException> findEntityAtTile(Vec2I const& pos, EntityFilterOf<TileEntity> entityFilter) const override;
  [[nodiscard]] std::expected<bool, StarException> tileIsOccupied(Vec2I const& pos, TileLayer layer, bool includeEphemeral = false, bool checkCollision = false) const override;
  [[nodiscard]] std::expected<CollisionKind, StarException> tileCollisionKind(Vec2I const& pos) const override;
  [[nodiscard]] std::expected<void, StarException> forEachCollisionBlock(RectI const& region, std::function<std::expected<void, StarException>(CollisionBlock const&)> const& iterator) const override;
  [[nodiscard]] std::expected<bool, StarException> isTileConnectable(Vec2I const& pos, TileLayer layer, bool tilesOnly = false) const override;
  [[nodiscard]] std::expected<bool, StarException> pointTileCollision(Vec2F const& point, CollisionSet const& collisionSet = DefaultCollisionSet) const override;
  [[nodiscard]] std::expected<bool, StarException> lineTileCollision(Vec2F const& begin, Vec2F const& end, CollisionSet const& collisionSet = DefaultCollisionSet) const override;
  [[nodiscard]] std::expected<std::optional<std::pair<Vec2F, Vec2I>>, StarException> lineTileCollisionPoint(Vec2F const& begin, Vec2F const& end, CollisionSet const& collisionSet = DefaultCollisionSet) const override;
  [[nodiscard]] std::expected<List<Vec2I>, StarException> collidingTilesAlongLine(Vec2F const& begin, Vec2F const& end, CollisionSet const& collisionSet = DefaultCollisionSet, int maxSize = -1, bool includeEdges = true) const override;
  [[nodiscard]] std::expected<bool, StarException> rectTileCollision(RectI const& region, CollisionSet const& collisionSet = DefaultCollisionSet) const override;
  [[nodiscard]] std::expected<TileDamageResult, StarException> damageTiles(List<Vec2I> const& pos, TileLayer layer, Vec2F const& sourcePosition, TileDamage const& tileDamage, std::optional<EntityId> sourceEntity = {}) override;
  [[nodiscard]] std::expected<std::shared_ptr<InteractiveEntity>, StarException> getInteractiveInRange(Vec2F const& targetPosition, Vec2F const& sourcePosition, float maxRange) const override;
  [[nodiscard]] std::expected<bool, StarException> canReachEntity(Vec2F const& position, float radius, EntityId targetEntity, bool preferInteractive = true) const override;
  [[nodiscard]] std::expected<RpcPromise<InteractAction>, StarException> interact(InteractRequest const& request) override;
  [[nodiscard]] std::expected<float, StarException> gravity(Vec2F const& pos) const override;
  [[nodiscard]] std::expected<float, StarException> windLevel(Vec2F const& pos) const override;
  [[nodiscard]] std::expected<float, StarException> lightLevel(Vec2F const& pos) const override;
  [[nodiscard]] std::expected<bool, StarException> breathable(Vec2F const& pos) const override;
  [[nodiscard]] std::expected<float, StarException> threatLevel() const override;
  [[nodiscard]] std::expected<StringList, StarException> environmentStatusEffects(Vec2F const& pos) const override;
  [[nodiscard]] std::expected<StringList, StarException> weatherStatusEffects(Vec2F const& pos) const override;
  [[nodiscard]] std::expected<bool, StarException> exposedToWeather(Vec2F const& pos) const override;
  [[nodiscard]] std::expected<std::optional<String>, StarException> activeWeather(Vec2F const& pos) const override;
  [[nodiscard]] std::expected<bool, StarException> isUnderground(Vec2F const& pos) const override;
  [[nodiscard]] std::expected<bool, StarException> disableDeathDrops() const override;
  [[nodiscard]] std::expected<List<PhysicsForceRegion>, StarException> forceRegions() const override;
  [[nodiscard]] std::expected<Json, StarException> getProperty(String const& propertyName, Json const& def = Json()) const override;
  [[nodiscard]] std::expected<void, StarException> setProperty(String const& propertyName, Json const& property) override;
  void timer(float delay, WorldAction worldAction) override;
  double epochTime() const override;
  std::uint32_t day() const override;
  float dayLength() const override;
  float timeOfDay() const override;
  std::shared_ptr<LuaRoot> luaRoot() override;
  [[nodiscard]] std::expected<RpcPromise<Vec2F>, StarException> findUniqueEntity(String const& uniqueId) override;
  [[nodiscard]] std::expected<RpcPromise<Json>, StarException> sendEntityMessage(std::variant<EntityId, String> const& entity, String const& message, JsonArray const& args = {}) override;
  bool isTileProtected(Vec2I const& pos) const override;
  
  [[nodiscard]] std::expected<StringMap<LuaCallbacks>, StarException> luaThreadCallbacks() const override;
  
  [[nodiscard]] std::expected<void, StarException> wire(Vec2I const& outputPosition, std::size_t outputIndex, Vec2I const& inputPosition, std::size_t inputIndex);

  bool getTileProtection(DungeonId dungeonId) const;
  [[nodiscard]] std::expected<void, StarException> setTileProtection(DungeonId dungeonId, bool isProtected);
  // sets a provided list of DungeonIds all at once and returns how many were changed
  [[nodiscard]] std::expected<std::size_t, StarException> setTileProtection(List<DungeonId> const& dungeonIds, bool isProtected);
  // used to globally, temporarily disable protection for certain operations
  void setTileProtectionEnabled(bool enabled);

  [[nodiscard]] std::expected<void, StarException> setDungeonGravity(DungeonId dungeonId, std::optional<float> gravity);
  [[nodiscard]] std::expected<void, StarException> setDungeonBreathable(DungeonId dungeonId, std::optional<bool> breathable);

  [[nodiscard]] std::expected<void, StarException> setDungeonId(RectI const& tileRegion, DungeonId dungeonId);

  // Signal a region to load / generate, returns true if it is now fully loaded
  // and generated
  [[nodiscard]] std::expected<bool, StarException> signalRegion(RectI const& region);
  // Immediately generate a given region
  [[nodiscard]] std::expected<void, StarException> generateRegion(RectI const& region);
  // Returns true if a region is fully active without signaling it.
  [[nodiscard]] std::expected<bool, StarException> regionActive(RectI const& region);

  std::shared_ptr<WorldServer::ScriptComponent> scriptContext(String const& contextName);

  List<EntityId> entityIds() const;

  // Queues a microdungeon for placement
  [[nodiscard]] std::expected<RpcPromise<Vec2I>, StarException> enqueuePlacement(List<BiomeItemDistribution> distributions, std::optional<DungeonId> id);

  [[nodiscard]] std::expected<std::reference_wrapper<ServerTile const>, StarException> getServerTile(Vec2I const& position, bool withSignal = false);
  // Borrows a loaded mutable server tile and marks it as needing updates to all
  // clients.
  [[nodiscard]] std::expected<std::optional<std::reference_wrapper<ServerTile>>, StarException> modifyServerTile(Vec2I const& position, bool withSignal = false);

  [[nodiscard]] std::expected<EntityId, StarException> loadUniqueEntity(String const& uniqueId);

  std::shared_ptr<WorldTemplate> worldTemplate() const;
  [[nodiscard]] std::expected<void, StarException> setTemplate(std::shared_ptr<WorldTemplate> newTemplate);
  std::shared_ptr<Sky> sky() const;
  [[nodiscard]] std::expected<void, StarException> modifyLiquid(Vec2I const& pos, LiquidId liquid, float quantity, bool additive = false);
  [[nodiscard]] std::expected<void, StarException> setLiquid(Vec2I const& pos, LiquidId liquid, float level, float pressure);
  [[nodiscard]] std::expected<List<ItemDescriptor>, StarException> destroyBlock(TileLayer layer, Vec2I const& pos, bool genItems, bool destroyModFirst, bool updateNeighbors = true);
  [[nodiscard]] std::expected<void, StarException> removeEntity(EntityId entityId, bool andDie);

  [[nodiscard]] std::expected<void, StarException> updateTileEntityTiles(std::shared_ptr<TileEntity> const& object, bool removing = false, bool checkBreaks = true);

  [[nodiscard]] std::expected<bool, StarException> isVisibleToPlayer(RectF const& region) const;
  [[nodiscard]] std::expected<void, StarException> activateLiquidRegion(RectI const& region);
  [[nodiscard]] std::expected<void, StarException> activateLiquidLocation(Vec2I const& location);

  // if blocks cascade, we'll need to do a break check across all tile entities
  // when the timer next ticks
  void requestGlobalBreakCheck();

  void setSpawningEnabled(bool spawningEnabled);

  void setPropertyListener(String const& propertyName, WorldPropertyListener listener);

  // Write all active sectors to disk without unloading them
  [[nodiscard]] std::expected<void, StarException> sync();
  // Unload all sectors
  [[nodiscard]] std::expected<void, StarException> unloadAll(bool force = false);
  // Copy full world to in memory representation
  [[nodiscard]] std::expected<WorldChunks, StarException> readChunks();

  [[nodiscard]] std::expected<bool, StarException> forceModifyTile(Vec2I const& pos, TileModification const& modification, bool allowEntityOverlap);
  [[nodiscard]] std::expected<TileModificationList, StarException> forceApplyTileModifications(TileModificationList const& modificationList, bool allowEntityOverlap);

  DungeonId dungeonId(Vec2I const& pos) const;

  [[nodiscard]] std::expected<bool, StarException> isPlayerModified(RectI const& region) const;

  [[nodiscard]] std::expected<ItemDescriptor, StarException> collectLiquid(List<Vec2I> const& tilePositions, LiquidId liquidId);

  [[nodiscard]] std::expected<bool, StarException> placeDungeon(String const& dungeonName, Vec2I const& position, std::optional<DungeonId> dungeonId = {}, bool forcePlacement = true);

  [[nodiscard]] std::expected<void, StarException> addBiomeRegion(Vec2I const& position, String const& biomeName, String const& subBlockSelector, int width);
  [[nodiscard]] std::expected<void, StarException> expandBiomeRegion(Vec2I const& position, int newWidth);

  // queue generation of the sectors that will be needed to insert or
  // expand a biome region in order to spread processing over time
  [[nodiscard]] std::expected<bool, StarException> pregenerateAddBiome(Vec2I const& position, int width);
  [[nodiscard]] std::expected<bool, StarException> pregenerateExpandBiome(Vec2I const& position, int newWidth);

  // set the biome at the given position to be the environment biome for the layer
  [[nodiscard]] std::expected<void, StarException> setLayerEnvironmentBiome(Vec2I const& position);

  // for terrestrial worlds only. updates the planet type in the celestial as well as local
  // world parameters along with the primary biome and the weather pool
  [[nodiscard]] std::expected<void, StarException> setPlanetType(String const& planetType, String const& primaryBiomeName);

  // Force the current weather to a specific index from the world's weather pool
  [[nodiscard]] std::expected<void, StarException> setWeatherIndex(std::size_t weatherIndex, bool force = false);
  // Force the current weather to a specific weather type by name
  [[nodiscard]] std::expected<void, StarException> setWeather(String const& weatherName, bool force = false);
  [[nodiscard]] std::expected<void, StarException> setWeather(Vec2F const& position, String const& weatherName, bool force = false);

  // Returns the list of weather names available in this world
  [[nodiscard]] std::expected<StringList, StarException> weatherList() const;
  [[nodiscard]] std::expected<StringList, StarException> weatherList(Vec2F const& position) const;

  // used to notify the universe server that the celestial planet type has changed
  std::optional<std::pair<String, String>> pullNewPlanetType();

private:
  friend struct WorldServerTestAccess;

  enum class UpdateTiming {
    Wiring, Liquid, FallingBlocks, BlockDamage, StorageTick, StorageGenerate, Environment, Count
  };
  enum class ProcessingLimit {
    Liquid, StorageGenerate, Count
  };

  explicit WorldServer(Root& root);
  struct ClientInfo {
    [[nodiscard]] static std::expected<std::shared_ptr<ClientInfo>, StarException> create(Root& root, ConnectionId clientId, InterpolationTracker const trackerInit);
    ClientInfo(ConnectionId clientId, InterpolationTracker const trackerInit);

    [[nodiscard]] std::expected<List<RectI>, StarException> monitoringRegions(std::shared_ptr<EntityMap> const& entityMap) const;

    bool needsDamageNotification(RemoteDamageNotification const& rdn) const;

    ConnectionId clientId;
    ClientSubWorldId subWorldId;
    std::uint64_t skyNetVersion;
    std::uint64_t weatherNetVersion;
    std::optional<String> weatherDomain;
    std::shared_ptr<WorldClientState> clientState;
    bool pendingForward;
    bool started;
    bool local;
    bool admin;

    List<std::shared_ptr<Packet>> outgoingPackets;

    // All slave entities for which the player should be knowledgable about.
    HashMap<EntityId, std::uint64_t> clientSlavesNetVersion;

    // Batch send tile updates
    HashSet<Vec2I> pendingTileUpdates;
    HashSet<Vec2I> pendingLiquidUpdates;
    HashSet<std::pair<Vec2I, TileLayer>> pendingTileDamageUpdates;
    HashSet<ServerTileSectorArray::Sector> pendingSectors;
    HashSet<ServerTileSectorArray::Sector> activeSectors;

    InterpolationTracker interpolationTracker;
  };

  struct TileEntitySpaces {
    List<MaterialSpace> materials;
    List<Vec2I> roots;
  };

  typedef std::function<std::expected<std::reference_wrapper<ServerTile const>, StarException>(Vec2I)> ServerTileGetter;

  [[nodiscard]] std::expected<void, StarException> init(bool firstTime);

  // Parse each field on first actual use, retaining the configured interval as
  // the elapsed tick count when this step matches its interval and offset.
  [[nodiscard]] std::expected<std::optional<unsigned>, StarException> shouldRunThisStep(UpdateTiming timingConfiguration);
  [[nodiscard]] std::expected<std::optional<std::uint64_t>, StarException> processingLimit(ProcessingLimit limitConfiguration);

  [[nodiscard]] std::expected<TileModificationList, StarException> doApplyTileModifications(TileModificationList const& modificationList, bool allowEntityOverlap, bool ignoreTileProtection = false, bool updateNeighbors = true);

  using SectorSnapshotCache = HashMap<ServerTileSectorArray::Sector, std::shared_ptr<TileArrayUpdatePacket>>;

  // Queues pending (step based) updates to the given player
  [[nodiscard]] std::expected<void, StarException> queueUpdatePackets(ConnectionId clientId, bool sendRemoteUpdates,
                                                                      List<RectI> const& monitoringRegions,
                                                                      SectorSnapshotCache& sectorSnapshots);
  [[nodiscard]] std::expected<void, StarException> updateDamage(float dt);

  [[nodiscard]] std::expected<void, StarException> updateDamagedBlocks(float dt);

  // Check for any newly broken entities in this rect
  [[nodiscard]] std::expected<void, StarException> checkEntityBreaks(RectF const& rect);
  // Push modified tile data to each client.
  [[nodiscard]] std::expected<void, StarException> queueTileUpdates(Vec2I const& pos);
  [[nodiscard]] std::expected<void, StarException> queueTileDamageUpdates(Vec2I const& pos, TileLayer layer);
  [[nodiscard]] std::expected<void, StarException> writeNetTile(Vec2I const& pos, NetTile& netTile) const;

  [[nodiscard]] std::expected<void, StarException> dirtyCollision(RectI const& region);
  [[nodiscard]] std::expected<void, StarException> freshenCollision(RectI const& region);

  [[nodiscard]] std::expected<Vec2F, StarException> findPlayerStart(std::optional<Vec2F> firstTry = {});
  [[nodiscard]] std::expected<Vec2F, StarException> findPlayerSpaceStart(float targetX);
  [[nodiscard]] std::expected<void, StarException> readMetadata();
  [[nodiscard]] std::expected<void, StarException> writeMetadata();
  [[nodiscard]] std::expected<float, StarException> gravityFromTile(ServerTile const& tile) const;

  bool isFloatingDungeonWorld() const;

  [[nodiscard]] std::expected<void, StarException> setupForceRegions();
  [[nodiscard]] std::expected<void, StarException> setupWeatherDomains(bool preserveNonSurface = false);
  std::shared_ptr<ServerWeather> weatherForDomain(std::optional<String> const& domain) const;
  [[nodiscard]] std::expected<std::shared_ptr<ServerWeather>, StarException> weatherAt(Vec2F const& position) const;
  std::shared_ptr<ServerWeather> surfaceWeather() const;

  Json m_serverConfig;

  std::shared_ptr<WorldTemplate> m_worldTemplate;
  WorldStructure m_centralStructure;
  Vec2F m_playerStart;
  bool m_adjustPlayerStart = false;
  bool m_respawnInWorld = false;
  JsonObject m_worldProperties;
  StringMap<WorldPropertyListener> m_worldPropertyListeners;

  std::optional<std::pair<String, String>> m_newPlanetType;

  std::shared_ptr<UniverseSettings> m_universeSettings;

  std::shared_ptr<EntityMap> m_entityMap;
  std::shared_ptr<ServerTileSectorArray> m_tileArray;
  ServerTileGetter m_tileGetterFunction;
  std::shared_ptr<WorldStorage> m_worldStorage;
  bool m_closeAttempted = false;
  bool m_initialized = false;
  WorldServerFidelity m_fidelity = WorldServerFidelity::Medium;
  Json m_fidelityConfig;
  // Json objects are immutable; identity detects replacement without comparing
  // the full configuration on every WorldServerThread tick.
  std::optional<std::reference_wrapper<JsonObject const>> m_fidelityConfigIdentity;
  std::array<std::optional<Vec2U>, static_cast<std::size_t>(UpdateTiming::Count)> m_updateTimings;
  std::array<std::optional<std::optional<std::uint64_t>>, static_cast<std::size_t>(ProcessingLimit::Count)> m_processingLimits;

  HashSet<Vec2I> m_damagedBlocks;
  std::shared_ptr<DamageManager> m_damageManager;
  std::shared_ptr<WireProcessor> m_wireProcessor;
  std::shared_ptr<LuaRoot> m_luaRoot;

  StringMap<std::shared_ptr<WorldServer::ScriptComponent>> m_scriptContexts;

  WorldGeometry m_geometry;
  double m_currentTime;
  std::uint64_t m_currentStep;
  mutable CellularLightIntensityCalculator m_lightIntensityCalculator;
  std::shared_ptr<Sky> m_sky;

  StringMap<std::shared_ptr<ServerWeather>> m_weatherDomains;
  std::shared_ptr<ServerWeather> m_emptyWeather;

  std::shared_ptr<Clock> m_referenceClock;

  CollisionGenerator m_collisionGenerator;
  List<CollisionBlock> m_workingCollisionBlocks;

  HashMap<NetCompatibilityRules, HashMap<std::pair<EntityId, std::uint64_t>, std::pair<ByteArray, std::uint64_t>>> m_netStateCache;
  OrderedHashMap<ConnectionId, std::shared_ptr<ClientInfo>> m_clientInfo;

  GameTimer m_entityUpdateTimer;
  GameTimer m_tileEntityBreakCheckTimer;

  std::shared_ptr<LiquidCellEngine<LiquidId>> m_liquidEngine;
  std::shared_ptr<FallingBlocksAgent> m_fallingBlocksAgent;
  std::shared_ptr<Spawner> m_spawner;

  // Keep track of material spaces and roots registered by tile entities to
  // make sure we can cleanly remove them when they change or when the entity
  // is removed / uninitialized
  HashMap<EntityId, TileEntitySpaces> m_tileEntitySpaces;

  List<std::pair<float, WorldAction>> m_timers;

  bool m_needsGlobalBreakCheck = false;

  bool m_generatingDungeon = false;
  HashMap<DungeonId, float> m_dungeonIdGravity;
  HashMap<DungeonId, bool> m_dungeonIdBreathable;
  StableHashSet<DungeonId> m_protectedDungeonIds;
  bool m_tileProtectionEnabled = true;

  HashMap<Uuid, std::pair<ConnectionId, std::variant<std::monostate, ConnectionId, RpcPromiseKeeper<Json>>>> m_entityMessageResponses;
  HashMap<Uuid, std::pair<ConnectionId, RpcPromise<Json>>> m_entityMessagePromises;

  List<PhysicsForceRegion> m_forceRegions;

  String m_worldId;

  GameTimer m_expiryTimer;
  
  StringMap<LuaCallbacks> m_luaThreadCallbacks;
};

}

export module star.world_server;

export namespace Star {
  using ::Star::Player;
  using ::Star::WorldTemplate;
  using ::Star::Sky;
  using ::Star::ServerWeather;
  using ::Star::SkyParameters;
  using ::Star::DamageManager;
  using ::Star::WireProcessor;
  using ::Star::EntityMap;
  using ::Star::WorldStorage;
  using ::Star::FallingBlocksAgent;
  using ::Star::DungeonDefinition;
  using ::Star::WorldServer;
  using ::Star::TileEntity;
  using ::Star::UniverseSettings;
  using ::Star::UniverseServer;
  using ::Star::WorldServerExceptionTag;
  using ::Star::WorldServerException;
  using ::Star::WorldServerFidelity;
  using ::Star::WorldServerFidelityNames;
}

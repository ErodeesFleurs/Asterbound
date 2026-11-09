module;
import star.memory;
import star.xx_hash;
import star.json;
import star.id_map;
import star.set;
import star.btree;
import star.ordered_map;
import star.lru_cache;
import star.data_stream_devices;
import star.thread;
import star.btree_database;
import star.casting;
import star.data_stream;
import star.ordered_set;
import star.list;
import star.bi_map;
import star.game_types;
import star.vector;
import star.string;
import star.map;
import star.io_device;
import star.format;
import star.version;
import star.rpc_promise;
import star.random;
import star.perlin;
import star.weighted_pool;
import star.strong_typedef;
import std;
import star.rect;
import star.worker_pool;

import star.sector_array_2d;
import star.color;
import star.image_processing;
import star.hash;
import star.string_view;
import star.directives;
import star.asset_path;

import star.damage_types;
import star.world_geometry;
import star.status_types;
import star.damage;
import star.light_source;
import star.entity;
import star.collision_block;
import star.liquid_types;
import star.tile_damage;
import star.tile_sector_array;
import star.animation;
import star.particle;
import star.weather_types;
import star.celestial_coordinate;
import star.sky_types;
import star.force_regions;
import star.world_parameters;
import star.celestial_parameters;
import star.world_layout;
import star.collision_generator;
import star.world_tiles;
import star.interaction_types;
import star.item_descriptor;
import star.quest_descriptor;
import star.interactive_entity;
import star.tile_entity;
import star.inspectable_entity;
import star.plant;
import star.plant_database;
import star.biome_placement;
import star.lua_root;
import star.versioning_database;

import star.byte_array;
namespace Star {
class Logger;

struct WorldStorageExceptionTag {
  static constexpr std::string_view name() { return "WorldStorageException"; }
};
using WorldStorageException = StarError<WorldStorageExceptionTag, StarException>;

class EntityMap;
class WorldStorage;

using WorldChunks = HashMap<ByteArray, std::optional<ByteArray>>;

enum class SectorLoadLevel : std::uint8_t {
  None = 0,
  Tiles = 1,
  Entities = 2,

  Loaded = 2
};

enum class SectorGenerationLevel : std::uint8_t {
  None = 0,
  BaseTiles = 1,
  MicroDungeons = 2,
  CaveLiquid = 3,
  Finalize = 4,

  Complete = 4,

  Terraform = 5
};

struct WorldGeneratorFacade {
  using Sector = ServerTileSectorArray::Sector;

  WorldGeneratorFacade() = default;
  virtual ~WorldGeneratorFacade() = default;

  // Should bring a given sector from generationLevel - 1 to generationLevel.
  [[nodiscard]] virtual std::expected<void, StarException> generateSectorLevel(WorldStorage& storage, Sector const& sector, SectorGenerationLevel generationLevel) = 0;

  [[nodiscard]] virtual std::expected<void, StarException> sectorLoadLevelChanged(WorldStorage& storage, Sector const& sector, SectorLoadLevel loadLevel) = 0;

  // Perform terraforming operations (biome reapplication) on the given sector
  [[nodiscard]] virtual std::expected<void, StarException> terraformSector(WorldStorage& storage, Sector const& sector) = 0;

  // Called after an entity is loaded, but before the entity is added to the
  // EntityMap.
  [[nodiscard]] virtual std::expected<void, StarException> initEntity(WorldStorage& storage, EntityId newEntityId, std::shared_ptr<Entity> const& entity) = 0;

  // Called after the entity is removed from the entity map but before it is
  // stored.
  [[nodiscard]] virtual std::expected<void, StarException> destructEntity(WorldStorage& storage, std::shared_ptr<Entity> const& entity) = 0;

  // Should return true if this entity should maintain the sector, false
  // otherwise.
  [[nodiscard]] virtual std::expected<bool, StarException> entityKeepAlive(WorldStorage& storage, std::shared_ptr<Entity> const& entity) const = 0;

  // Should return true if this entity should be stored along with the world,
  // false otherwise.
  [[nodiscard]] virtual std::expected<bool, StarException> entityPersistent(WorldStorage& storage, std::shared_ptr<Entity> const& entity) const = 0;

  // Queues up a microdungeon. Fulfills the rpc promise with the position the
  // microdungeon was placed at
  [[nodiscard]] virtual std::expected<RpcPromise<Vec2I>, StarException> enqueuePlacement(List<BiomeItemDistribution> placements, std::optional<DungeonId> id) = 0;
};

// Handles paging entity and tile data in / out of disk backed storage for
// WorldServer and triggers initial generation.  Ties tile sectors to entity
// sectors, and allows for multiple stage generation of those sectors.  Sector
// generation is done in stages, so that lower generation stages are done in a
// one sector border around the higher generation stages.
//
// WorldStorage is designed so that once constructed, any exceptions triggered
// during loading, unloading, or generation that would result in an
// indeterminate world state cause the underlying database to be rolled back
// and then immediately closed.  The underlying database is committed by
// explicit close() or sync(); destruction never commits.
class Root;
class WorldStorage {
  Root& m_root;
  Root& root() const { return m_root; }
public:
  using Sector = ServerTileSectorArray::Sector;
  using TileArray = ServerTileSectorArray::Array;

  static WorldChunks getWorldChunksUpdate(WorldChunks const& oldChunks, WorldChunks const& newChunks);
  [[nodiscard]] static std::expected<void, StarException> applyWorldChunksUpdateToFile(Logger& logger, String const& file, WorldChunks const& update);
  [[nodiscard]] static std::expected<WorldChunks, StarException> getWorldChunksFromFile(Logger& logger, String const& file);

  // Create a new world of the given size.
  [[nodiscard]] static std::expected<std::shared_ptr<WorldStorage>, StarException> create(Root& root, Vec2U const& worldSize, std::shared_ptr<IODevice> const& device, std::shared_ptr<WorldGeneratorFacade> const& generatorFacade);
  // Read an existing world.
  [[nodiscard]] static std::expected<std::shared_ptr<WorldStorage>, StarException> create(Root& root, std::shared_ptr<IODevice> const& device, std::shared_ptr<WorldGeneratorFacade> const& generatorFacade);
  // Read an in-memory world.
  [[nodiscard]] static std::expected<std::shared_ptr<WorldStorage>, StarException> create(Root& root, WorldChunks const& chunks, std::shared_ptr<WorldGeneratorFacade> const& generatorFacade);
  ~WorldStorage();
  // Unload and save all sectors, reporting persistence errors to the caller.
  [[nodiscard]] std::expected<void, StarException> close();

  [[nodiscard]] std::expected<VersionedJson, StarException> worldMetadata();
  [[nodiscard]] std::expected<void, StarException> setWorldMetadata(VersionedJson const& metadata);

  std::shared_ptr<ServerTileSectorArray> const& tileArray() const;
  std::shared_ptr<EntityMap> const& entityMap() const;

  std::optional<Sector> sectorForPosition(Vec2I const& position) const;
  [[nodiscard]] std::expected<List<Sector>, StarException> sectorsForRegion(RectI const& region) const;
  std::optional<RectI> regionForSector(Sector sector) const;

  SectorLoadLevel sectorLoadLevel(Sector sector) const;
  // Returns the sector generation level if it is currently loaded, nothing
  // otherwise.
  std::optional<SectorGenerationLevel> sectorGenerationLevel(Sector sector) const;
  // Returns true if the sector is both loaded and fully generated.
  bool sectorActive(Sector) const;

  // Fully load the given sector and reset its TTL without triggering any
  // generation.
  [[nodiscard]] std::expected<void, StarException> loadSector(Sector sector);
  // Fully load, reset the TTL, and if necessary, fully generate the given
  // sector.
  [[nodiscard]] std::expected<void, StarException> activateSector(Sector sector);
  // Queue the given sector for activation, if it is not already active.  If
  // the sector is loaded at all, also resets the TTL.
  [[nodiscard]] std::expected<void, StarException> queueSectorActivation(Sector sector);

  // Immediately (synchronously) fully generates the sector, then flags it as requiring
  // terraforming (biome reapplication) which will be handled by the normal generation process
  [[nodiscard]] std::expected<void, StarException> triggerTerraformSector(Sector sector);

  // Queues up a microdungeon. Fulfills the rpc promise with the position the
  // microdungeon was placed at
  [[nodiscard]] std::expected<RpcPromise<Vec2I>, StarException> enqueuePlacement(List<BiomeItemDistribution> placements, std::optional<DungeonId> id);

  // Return the remaining time to live for a sector, if loaded.  A sector's
  // time to live is reset when loaded or generated, and when the time to live
  // reaches zero, the sector is automatically unloaded.
  std::optional<float> sectorTimeToLive(Sector sector) const;
  // Set the given sector's time to live, if it is loaded at all.  Returns
  // false if the sector was not loaded so no action was taken.
  bool setSectorTimeToLive(Sector sector, float newTimeToLive);

  // Returns the position for a given unique entity if it exists in this world,
  // loaded or not.
  [[nodiscard]] std::expected<std::optional<Vec2F>, StarException> findUniqueEntity(String const& uniqueId);

  // If the given unique entity is not loaded, loads its sector and then if the
  // unique entity is found, returns the entity id, otherwise NullEntityId.
  [[nodiscard]] std::expected<EntityId, StarException> loadUniqueEntity(String const& uniqueId);

  // Does any queued generation work, potentially limiting the total number of
  // increases of SectorGenerationLevel by the sectorGenerationLevelLimit, if
  // given.  If sectorOrdering is given, then it will be used to prioritize the
  // queued sectors.
  [[nodiscard]] std::expected<void, StarException> generateQueue(std::optional<std::size_t> sectorGenerationLevelLimit, std::function<bool(Sector, Sector)> sectorOrdering = {});
  // Ticks down the TTL on sectors and generation queue entries, stores old
  // sectors, expires old generation queue entries, and unloads any zombie
  // entities.
  [[nodiscard]] std::expected<void, StarException> tick(float dt, std::optional<std::reference_wrapper<String const>> worldId = std::nullopt);

  // Unload all sectors that can be unloaded (if force is specified, ALWAYS
  // unloads all sectors)
  [[nodiscard]] std::expected<void, StarException> unloadAll(bool force = false);

  // Sync all active sectors without unloading them, and commits the underlying
  // database.
  [[nodiscard]] std::expected<void, StarException> sync();

  // Syncs all active sectors to disk and stores the full content of the world
  // into memory.
  [[nodiscard]] std::expected<WorldChunks, StarException> readChunks();

  // if this is set, all terrain generation is assumed to be handled by dungeon placement
  // and steps such as microdungeons, biome objects and grass mods will be skipped
  bool floatingDungeonWorld() const;
  void setFloatingDungeonWorld(bool floatingDungeonWorld);

private:
  enum class StoreType : std::uint8_t {
    Metadata = 0,
    TileSector = 1,
    EntitySector = 2,
    UniqueIndex = 3,
    SectorUniques = 4
  };

  using SectorAndPosition = std::pair<Sector, Vec2F>;

  struct WorldMetadataStore {
    Vec2U worldSize;
    VersionedJson userMetadata;
  };

  using EntitySectorStore = List<VersionedJson>;
  // Map of uuid to entity's position and sector they were stored in.
  using UniqueIndexStore = HashMap<String, SectorAndPosition>;
  // Set of unique ids that are stored in a given sector
  using SectorUniqueStore = HashSet<String>;

  struct TileSectorStore {
    TileSectorStore();

    // Also store generation level along with tiles, simply because tiles are
    // the first things to be loaded and the last to be stored.
    SectorGenerationLevel generationLevel;

    VersionNumber tileSerializationVersion;
    std::unique_ptr<TileArray> tiles;
  };

  struct SectorMetadata {
    SectorMetadata();

    SectorLoadLevel loadLevel;
    SectorGenerationLevel generationLevel;
    float timeToLive;
  };

  [[nodiscard]] static std::expected<ByteArray, StarException> metadataKey();
  [[nodiscard]] static std::expected<WorldMetadataStore, StarException> readWorldMetadata(ByteArray const& data);
  [[nodiscard]] static std::expected<ByteArray, StarException> writeWorldMetadata(WorldMetadataStore const& metadata);

  [[nodiscard]] static std::expected<ByteArray, StarException> entitySectorKey(Sector const& sector);
  [[nodiscard]] static std::expected<EntitySectorStore, StarException> readEntitySector(ByteArray const& data);
  [[nodiscard]] static std::expected<ByteArray, StarException> writeEntitySector(EntitySectorStore const& store);

  [[nodiscard]] static std::expected<ByteArray, StarException> tileSectorKey(Sector const& sector);
  [[nodiscard]] std::expected<TileSectorStore, StarException> readTileSector(ByteArray const& data);
  [[nodiscard]] static std::expected<ByteArray, StarException> writeTileSector(TileSectorStore const& store);

  [[nodiscard]] static std::expected<ByteArray, StarException> uniqueIndexKey(String const& uniqueId);
  [[nodiscard]] static std::expected<UniqueIndexStore, StarException> readUniqueIndexStore(ByteArray const& data);
  [[nodiscard]] static std::expected<ByteArray, StarException> writeUniqueIndexStore(UniqueIndexStore const& store);

  [[nodiscard]] static std::expected<ByteArray, StarException> sectorUniqueKey(Sector const& sector);
  [[nodiscard]] static std::expected<SectorUniqueStore, StarException> readSectorUniqueStore(ByteArray const& data);
  [[nodiscard]] static std::expected<ByteArray, StarException> writeSectorUniqueStore(SectorUniqueStore const& store);

  [[nodiscard]] static std::expected<void, StarException> openDatabase(BTreeDatabase& db, std::shared_ptr<IODevice> device);

  WorldStorage(Root& root);
  [[nodiscard]] std::expected<void, StarException> initialize();
  StarException invalidate(StarException error, std::string context);

  bool belongsInSector(Sector const& sector, Vec2F const& position) const;

  // Generate a random TTL value in the configured range
  [[nodiscard]] std::expected<float, StarException> randomizedSectorTTL() const;

  // Generate the given sector to the given generation level.  If
  // sectorGenerationLevelLimit is given, stops work as soon as the given
  // number of generation level changes has occurred.  Returns whether the
  // given sector was fully generated, and the total number of generation
  // levels increased.  If any sector's generation level is brought up at all,
  // it will also reset the TTL for that sector.
  [[nodiscard]] std::expected<std::pair<bool, std::size_t>, StarException> generateSectorToLevel(Sector const& sector, SectorGenerationLevel targetGenerationLevel, std::size_t sectorGenerationLevelLimit = NPos);

  // Bring the sector up to the given load level, and all surrounding sectors
  // as appropriate.  If the load level is brought up, also resets the TTL.
  [[nodiscard]] std::expected<void, StarException> loadSectorToLevel(Sector const& sector, SectorLoadLevel targetLoadLevel);

  // Store and unload the given sector to the given level, given the state of
  // the surrounding sectors.  If force is true, will always unload to the
  // given level.
  [[nodiscard]] std::expected<bool, StarException> unloadSectorToLevel(Sector const& sector, SectorLoadLevel targetLoadLevel, bool force = false);

  // Sync this sector to disk without unloading it.
  [[nodiscard]] std::expected<void, StarException> syncSector(Sector const& sector);

  // Returns the sectors within WorldSectorSize of the given sector.  This is
  // *not exactly the same* as the surrounding 9 sectors in a square pattern,
  // because first this does not return invalid sectors, and second, If a world
  // is not evenly divided by the sector size, this may return extra sectors on
  // one side because they are within range.
  [[nodiscard]] std::expected<List<Sector>, StarException> adjacentSectors(Sector const& sector) const;

  // Replace the sector uniques for this sector with the given set
  [[nodiscard]] std::expected<void, StarException> updateSectorUniques(Sector const& sector, UniqueIndexStore const& sectorUniques);
  // Merge the stored sector uniques for this sector with the given set
  [[nodiscard]] std::expected<void, StarException> mergeSectorUniques(Sector const& sector, UniqueIndexStore const& sectorUniques);

  [[nodiscard]] std::expected<std::optional<SectorAndPosition>, StarException> getUniqueIndexEntry(String const& uniqueId);
  [[nodiscard]] std::expected<void, StarException> setUniqueIndexEntry(String const& uniqueId, SectorAndPosition const& sectorAndPosition);
  // Remove the index entry for this unique id, if the index entry found points
  // to the given sector
  [[nodiscard]] std::expected<void, StarException> removeUniqueIndexEntry(String const& uniqueId, Sector const& sector);

  Vec2F m_sectorTimeToLive;
  float m_generationQueueTimeToLive;

  std::shared_ptr<ServerTileSectorArray> m_tileArray;
  std::shared_ptr<EntityMap> m_entityMap;
  std::shared_ptr<WorldGeneratorFacade> m_generatorFacade;

  bool m_floatingDungeonWorld;

  StableHashMap<Sector, SectorMetadata> m_sectorMetadata;
  OrderedHashMap<Sector, float> m_generationQueue;
  BTreeDatabase m_db;
};

}

export module star.world_storage;

export namespace Star {
  using ::Star::WorldStorageExceptionTag;
  using ::Star::WorldStorageException;
  using ::Star::EntityMap;
  using ::Star::WorldGeneratorFacade;
  using ::Star::WorldStorage;
  using ::Star::WorldChunks;
  using ::Star::SectorLoadLevel;
  using ::Star::SectorGenerationLevel;
}

module;
import star.format;
import star.json;
import star.xx_hash;
import star.vector;
import star.set;
import star.map;
import star.random;
import star.game_types;
import star.material_types;
import star.poly;
import star.list;
import star.bi_map;
import std;
import star.rect;
import star.thread;
import star.worker_pool;

import star.sector_array_2d;
import star.color;
import star.image_processing;
import star.hash;
import star.data_stream;
import star.string_view;
import star.directives;
import star.asset_path;
import star.string;
import star.image;
import star.ordered_map;
import star.lru_cache;
import star.id_map;
import star.btree;
import star.data_stream_devices;
import star.btree_database;
import star.casting;
import star.ordered_set;
import star.rpc_promise;
import star.perlin;
import star.weighted_pool;
import star.strong_typedef;

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

import star.item_descriptor;
import star.world_geometry;
import star.dungeon_generator;
import star.damage_types;
import star.status_types;
import star.damage;
import star.light_source;
import star.entity;
import star.interaction_types;
import star.quest_descriptor;
import star.interactive_entity;
import star.tile_entity;
import star.inspectable_entity;
import star.plant;
import star.lua_root;
import star.versioning_database;
import star.world_storage;
import star.cellular_liquid;
import star.plant_database;
import star.biome_placement;

import star.falling_blocks_agent;

namespace Star {
class WorldServer;
class Plant;
class MaterialDatabase;
class LiquidsDatabase;
}

import star.micro_dungeon;
import star.spawn_type_database;
import star.spawner;

namespace Star {

class LiquidWorld : public CellularLiquidWorld<LiquidId> {
public:
  [[nodiscard]] static std::expected<std::shared_ptr<LiquidWorld>, StarException> create(WorldServer& world);

  Vec2I unique_location(Vec2I const& location) const override;
  [[nodiscard]] std::expected<CellularLiquidCell<LiquidId>, StarException> cell(Vec2I const& location) const override;
  [[nodiscard]] std::expected<float, StarException> drain_level(Vec2I const& location) const override;
  [[nodiscard]] std::expected<void, StarException> set_flow(Vec2I const& location, CellularLiquidFlowCell<LiquidId> const& flow) override;
  [[nodiscard]] std::expected<void, StarException> liquid_interaction(Vec2I const& a, LiquidId a_liquid, Vec2I const& b, LiquidId b_liquid) override;
  [[nodiscard]] std::expected<void, StarException> liquid_collision(Vec2I const& pos, LiquidId liquid, Vec2I const& block_pos) override;

private:
  explicit LiquidWorld(WorldServer& world) : m_world_server(world) {}
  WorldServer& m_world_server;
  std::shared_ptr<LiquidsDatabase const> m_liquids_database;
  std::shared_ptr<MaterialDatabase const> m_material_database;
};

class FallingBlocksWorld : public FallingBlocksFacade {
public:
  [[nodiscard]] static std::expected<std::shared_ptr<FallingBlocksWorld>, StarException> create(WorldServer& world);

  [[nodiscard]] std::expected<FallingBehavior, StarException> source_behavior(Vec2I const& pos) override;
  [[nodiscard]] std::expected<bool, StarException> target_open(Vec2I const& pos) override;
  [[nodiscard]] std::expected<BlockMoveResult, BlockMoveError> move_block(Vec2I const& from, Vec2I const& to) override;

private:
  explicit FallingBlocksWorld(WorldServer& world) : m_world_server(world) {}
  WorldServer& m_world_server;
  std::shared_ptr<MaterialDatabase const> m_material_database;
};

class DungeonGeneratorWorld : public DungeonGeneratorWorldFacade {
public:
  DungeonGeneratorWorld(WorldServer& worldServer, bool markForActivation);

  [[nodiscard]] std::expected<void, StarException> markRegion(RectI const& region) override;
  [[nodiscard]] std::expected<void, StarException> markTerrain(PolyF const& region) override;
  [[nodiscard]] std::expected<void, StarException> markSpace(PolyF const& region) override;

  [[nodiscard]] std::expected<void, StarException> setForegroundMaterial(Vec2I const& position, MaterialId material, MaterialHue hueshift, MaterialColorVariant colorVariant) override;
  [[nodiscard]] std::expected<void, StarException> setBackgroundMaterial(Vec2I const& position, MaterialId material, MaterialHue hueshift, MaterialColorVariant colorVariant) override;
  [[nodiscard]] std::expected<void, StarException> setForegroundMod(Vec2I const& position, ModId mod, MaterialHue hueshift) override;
  [[nodiscard]] std::expected<void, StarException> setBackgroundMod(Vec2I const& position, ModId mod, MaterialHue hueshift) override;
  [[nodiscard]] std::expected<void, StarException> placeObject(
      Vec2I const& pos, String const& objectName, Star::Direction direction, Json const& parameters) override;
  [[nodiscard]] std::expected<void, StarException> placeVehicle(Vec2F const& pos, String const& vehicleName, Json const& parameters) override;
  [[nodiscard]] std::expected<void, StarException> placeSurfaceBiomeItems(Vec2I const& pos) override;
  [[nodiscard]] std::expected<void, StarException> placeBiomeTree(Vec2I const& pos) override;
  [[nodiscard]] std::expected<void, StarException> addDrop(Vec2F const& position, ItemDescriptor const& item) override;
  [[nodiscard]] std::expected<void, StarException> spawnNpc(Vec2F const& position, Json const& parameters) override;
  [[nodiscard]] std::expected<void, StarException> spawnStagehand(Vec2F const& position, Json const& definition) override;
  [[nodiscard]] std::expected<void, StarException> setLiquid(Vec2I const& pos, LiquidStore const& liquid) override;
  [[nodiscard]] std::expected<void, StarException> setPlayerStart(Vec2F const& startPosition) override;
  [[nodiscard]] std::expected<void, StarException> connectWireGroup(List<Vec2I> const& wireGroup) override;
  [[nodiscard]] std::expected<void, StarException> setTileProtection(DungeonId dungeonId, bool isProtected) override;
  [[nodiscard]] std::expected<bool, StarException> checkSolid(Vec2I const& position, TileLayer layer) override;
  [[nodiscard]] std::expected<bool, StarException> checkOpen(Vec2I const& position, TileLayer open) override;
  [[nodiscard]] std::expected<bool, StarException> checkOceanLiquid(Vec2I const& position) override;
  [[nodiscard]] std::expected<DungeonId, StarException> getDungeonIdAt(Vec2I const& position) override;
  [[nodiscard]] std::expected<void, StarException> setDungeonIdAt(Vec2I const& position, DungeonId dungeonId) override;
  [[nodiscard]] std::expected<void, StarException> clearTileEntities(RectI const& bounds, Set<Vec2I> const& positions, bool clearAnchoredObjects) override;

  WorldGeometry getWorldGeometry() const override;

private:
  [[nodiscard]] std::expected<void, StarException> placePlant(std::shared_ptr<Plant> const& plant, Vec2I const& position);
  [[nodiscard]] std::expected<void, StarException> place_biome_items(Vec2I const& pos, List<BiomeItemPlacement>& potential_items);

  WorldServer& m_worldServer;
  bool m_markForActivation;
};

class SpawnerWorld : public SpawnerFacade {
public:
  SpawnerWorld(WorldServer& worldServer);

  WorldGeometry geometry() const override;
  List<RectF> clientWindows() const override;
  [[nodiscard]] std::expected<bool, StarException> signalRegion(RectF const& region) const override;
  [[nodiscard]] std::expected<CollisionKind, StarException> collision(Vec2I const& position) const override;
  [[nodiscard]] std::expected<bool, StarException> isFreeSpace(RectF const& area) const override;
  [[nodiscard]] std::expected<bool, StarException> isBackgroundEmpty(Vec2I const& position) const override;
  LiquidLevel liquidLevel(Vec2I const& position) const override;
  [[nodiscard]] std::expected<bool, StarException> spawningProhibited(RectF const& area) const override;
  std::uint64_t spawnSeed() const override;
  [[nodiscard]] std::expected<SpawnProfile, StarException> spawnProfile(Vec2F const& position) const override;
  float dayLevel() const override;
  [[nodiscard]] std::expected<float, StarException> threatLevel() const override;
  [[nodiscard]] std::expected<EntityId, StarException> spawnEntity(std::shared_ptr<Entity> entity) const override;
  [[nodiscard]] std::expected<void, StarException> despawnEntity(EntityId entityId) override;
  std::shared_ptr<Entity> getEntity(EntityId entityId) const override;

private:
  WorldServer& m_worldServer;
};

class WorldGenerator : public WorldGeneratorFacade {
public:
  WorldGenerator(WorldServer& server);

  [[nodiscard]] std::expected<void, StarException> generateSectorLevel(WorldStorage& worldStorage, Sector const& sector, SectorGenerationLevel generationLevel) override;
  [[nodiscard]] std::expected<void, StarException> sectorLoadLevelChanged(WorldStorage& worldStorage, Sector const& sector, SectorLoadLevel loadLevel) override;
  [[nodiscard]] std::expected<void, StarException> terraformSector(WorldStorage& worldStorage, Sector const& sector) override;
  [[nodiscard]] std::expected<void, StarException> initEntity(WorldStorage& worldStorage, EntityId entityId, std::shared_ptr<Entity> const& entity) override;
  [[nodiscard]] std::expected<void, StarException> destructEntity(WorldStorage& worldStorage, std::shared_ptr<Entity> const& entity) override;
  [[nodiscard]] std::expected<bool, StarException> entityKeepAlive(WorldStorage& worldStorage, std::shared_ptr<Entity> const& entity) const override;
  [[nodiscard]] std::expected<bool, StarException> entityPersistent(WorldStorage& worldStorage, std::shared_ptr<Entity> const& entity) const override;
  [[nodiscard]] std::expected<RpcPromise<Vec2I>, StarException> enqueuePlacement(List<BiomeItemDistribution> distributions, std::optional<DungeonId> id) override;

  [[nodiscard]] std::expected<void, StarException> replaceBiomeBlocks(ServerTile& tile);

private:
  struct QueuedPlacement {
    List<BiomeItemDistribution> distributions;
    std::optional<DungeonId> dungeonId;
    RpcPromiseKeeper<Vec2I> promise;
    bool fulfilled;
  };

  [[nodiscard]] std::expected<void, StarException> prepareTiles(WorldStorage& worldStorage, Sector const& sector);
  [[nodiscard]] std::expected<void, StarException> generateMicroDungeons(WorldStorage& worldStorage, Sector const& sector);
  [[nodiscard]] std::expected<void, StarException> generateCaveLiquid(WorldStorage& worldStorage, Sector const& sector);
  [[nodiscard]] std::expected<void, StarException> prepareSector(WorldStorage& worldStorage, Sector const& sector);
  [[nodiscard]] std::expected<void, StarException> prepareSectorBiomeBlocks(WorldStorage& worldStorage, Sector const& sector);

  [[nodiscard]] std::expected<void, StarException> placeBiomeGrass(WorldStorage& worldStorage, ServerTile& tile, Vec2I const& position);

  [[nodiscard]] std::expected<void, StarException> reapply_biome(WorldStorage& world_storage, Sector const& sector);

  [[nodiscard]] std::expected<Set<Vec2I>, StarException> caveLiquidSeeds(WorldStorage& worldStorage, Sector const& sector);
  Map<Vec2I, float> determineLiquidLevel(Set<Vec2I> const& spots, Set<Vec2I> const& filled);
  void levelCluster(Set<Vec2I>& cluster, Set<Vec2I> const& filled, Map<Vec2I, float>& results);

  // Special plant placement routine that does slight terrain adjustments to
  // fit plants.
  [[nodiscard]] std::expected<bool, StarException> placePlant(WorldStorage& worldStorage, std::shared_ptr<Plant> const& plant, Vec2I const& position);

  WorldServer& m_worldServer;
  std::shared_ptr<MicroDungeonFactory> m_microDungeonFactory;
  List<QueuedPlacement> m_queuedPlacements;
};

}

export module star.world_generation;

export namespace Star {
  using ::Star::LiquidWorld;
  using ::Star::FallingBlocksWorld;
  using ::Star::DungeonGeneratorWorld;
  using ::Star::SpawnerWorld;
  using ::Star::WorldGenerator;
}

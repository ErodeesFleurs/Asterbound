module;
import std;
import star.format;
import star.string;
import star.json;
import star.ordered_map;
import star.lru_cache;
import star.bi_map;
import star.random;
import star.perlin;
import star.weighted_pool;
import star.color;
import star.list;
import star.rect;
import star.image_processing;
import star.hash;
import star.data_stream;
import star.string_view;
import star.thread;
import star.directives;
import star.asset_path;
import star.game_types;
import star.liquid_types;
import star.material_types;
import star.vector;
import star.poly;
import star.id_map;
import star.set;
import star.casting;
import star.strong_typedef;

import star.animation;
import star.particle;
import star.weather_types;
import star.celestial_coordinate;
import star.sky_types;
import star.force_regions;
import star.world_parameters;
import star.celestial_parameters;
import star.world_layout;
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
import star.collision_block;
import star.tile_entity;
import star.tile_damage;
import star.inspectable_entity;
import star.plant;
import star.plant_database;
import star.biome_placement;
import star.celestial_types;
import star.sky_parameters;

namespace Star {

class CelestialDatabase;
struct AmbientNoisesDescription;

// Reference object that describes the generation of a single world, and all
// of the world metadata.  Meant to remain static (or relatively static)
// throughout the life of a World.
class Root;
class WorldTemplate {
  Root& m_root;
  Root& root() const { return m_root; }
public:
  struct WeatherDomain {
    String name;
    WeatherPool pool;
    int effectsMinHeight;
  };

  struct WeatherLayer {
    String name;
    std::optional<String> domain;
    int minHeight;
    int maxHeight;
  };

  struct Dungeon {
    String dungeon;
    int baseHeight;
    int baseX;
    int xVariance;
    bool force;
    bool blendWithTerrain;
  };

  struct BlockInfo {
    BiomeIndex blockBiomeIndex = NullBiomeIndex;
    BiomeIndex environmentBiomeIndex = NullBiomeIndex;

    bool biomeTransition = false;

    bool terrain = false;
    bool foregroundCave = false;
    bool backgroundCave = false;

    MaterialId foreground = EmptyMaterialId;
    ModId foregroundMod = NoModId;

    MaterialId background = EmptyMaterialId;
    ModId backgroundMod = NoModId;

    LiquidId caveLiquid = EmptyLiquidId;
    float caveLiquidSeedDensity = 0.0f;

    LiquidId oceanLiquid = EmptyLiquidId;
    int oceanLiquidLevel = 0;

    bool encloseLiquids = false;
    bool fillMicrodungeons = false;

    Json toJson() const;
  };

  struct PotentialBiomeItems {
    // Potential items that would spawn at the given block assuming it is at
    // the surface.
    List<BiomeItemPlacement> surface_biome_items;
    // ... Or on a cave surface.
    List<BiomeItemPlacement> cave_surface_biome_items;
    // ... Or on a cave ceiling.
    List<BiomeItemPlacement> cave_ceiling_biome_items;
    // ... Or on a cave background wall.
    List<BiomeItemPlacement> cave_background_biome_items;
    // ... Or in the ocean
    List<BiomeItemPlacement> ocean_items;
  };

  // Creates a blank world with the given size
  [[nodiscard]] static std::expected<std::shared_ptr<WorldTemplate>, StarException> create(Root& root, Vec2U const& size);
  // Creates a world from the given visitable celestial object.
  [[nodiscard]] static std::expected<std::shared_ptr<WorldTemplate>, StarException> create(Root& root, CelestialCoordinate const& celestialCoordinate, std::shared_ptr<CelestialDatabase> const& celestialDatabase);
  // Creates a world from a bare VisitableWorldParameters structure
  [[nodiscard]] static std::expected<std::shared_ptr<WorldTemplate>, StarException> create(Root& root, std::shared_ptr<VisitableWorldParameters const> const& worldParameters, SkyParameters const& skyParameters, std::uint64_t seed);
  // Load a world template from the given stored data.
  [[nodiscard]] static std::expected<std::shared_ptr<WorldTemplate>, StarException> create(Root& root, Json const& store);

  [[nodiscard]] std::expected<Json, StarException> store() const;

  std::optional<CelestialParameters> const& celestialParameters() const;
  std::shared_ptr<VisitableWorldParameters const> worldParameters() const;
  SkyParameters skyParameters() const;
  std::shared_ptr<WorldLayout> worldLayout() const;

  [[nodiscard]] std::expected<void, StarException> setWorldParameters(std::shared_ptr<VisitableWorldParameters> newParameters);
  void setWorldLayout(std::shared_ptr<WorldLayout> newLayout);
  void setSkyParameters(SkyParameters newParameters);

  std::uint64_t worldSeed() const;
  String worldName() const;

  Vec2U size() const;

  // The average (ish) surface level for this world, off of which terrain
  // generators modify the surface height.
  float surfaceLevel() const;

  // The constant height at which everything below is considered "underground"
  float undergroundLevel() const;

  // returns true if the world is terrestrial and the specified position is within the
  // planet's surface layer
  bool inSurfaceLayer(Vec2I const& position) const;

  // If it is specified, searches the player start search region for an
  // acceptable player start area.  The block returned will be an empty block
  // above a terrain block, in a region of free space.  If no such block can be
  // found or the player start search region is not specified, returns nothing.
  [[nodiscard]] std::expected<std::optional<Vec2I>, StarException> findSensiblePlayerStart() const;

  // Add either a solid region hint or a space region hint for the given
  // polygonal region.  Blending size and weighting is configured in the
  // WorldTemplate config file.
  void addCustomTerrainRegion(PolyF poly);
  void addCustomSpaceRegion(PolyF poly);
  void clearCustomTerrains();

  [[nodiscard]] std::expected<List<RectI>, StarException> previewAddBiomeRegion(Vec2I const& position, int width);
  [[nodiscard]] std::expected<List<RectI>, StarException> previewExpandBiomeRegion(Vec2I const& position, int newWidth);

  [[nodiscard]] std::expected<void, StarException> addBiomeRegion(Vec2I const& position, String const& biomeName, String const& subBlockSelector, int width);
  [[nodiscard]] std::expected<void, StarException> expandBiomeRegion(Vec2I const& position, int newWidth);

  [[nodiscard]] std::expected<List<Dungeon>, StarException> dungeons() const;

  // Is this tile block naturally outside the terrain?
  [[nodiscard]] std::expected<bool, StarException> isOutside(int x, int y) const;
  // Is this integral region of blocks outside the terrain?
  [[nodiscard]] std::expected<bool, StarException> isOutside(RectI const& region) const;

  [[nodiscard]] std::expected<BlockInfo, StarException> blockInfo(int x, int y) const;

  // Partial blockinfo that doesn't use terrain selectors.
  [[nodiscard]] std::expected<BlockInfo, StarException> blockBiomeInfo(int x, int y) const;

  [[nodiscard]] std::expected<BiomeIndex, StarException> blockBiomeIndex(int x, int y) const;
  [[nodiscard]] std::expected<BiomeIndex, StarException> environmentBiomeIndex(int x, int y) const;
  [[nodiscard]] std::expected<std::shared_ptr<Biome const>, StarException> biome(BiomeIndex biomeIndex) const;

  [[nodiscard]] std::expected<std::shared_ptr<Biome const>, StarException> blockBiome(int x, int y) const;
  [[nodiscard]] std::expected<std::shared_ptr<Biome const>, StarException> environmentBiome(int x, int y) const;


  // Material and mod hue shifts for this biome.
  [[nodiscard]] std::expected<MaterialHue, StarException> biomeMaterialHueShift(BiomeIndex biomeIndex, MaterialId material) const;
  [[nodiscard]] std::expected<MaterialHue, StarException> biomeModHueShift(BiomeIndex biomeIndex, ModId mod) const;

  [[nodiscard]] std::expected<std::shared_ptr<AmbientNoisesDescription const>, StarException> ambient_noises(int x, int y) const;
  [[nodiscard]] std::expected<std::shared_ptr<AmbientNoisesDescription const>, StarException> music_track(int x, int y) const;

  StringList environmentStatusEffects(int x, int y) const;
  bool breathable(int x, int y) const;

  WeatherPool weathers() const;

  List<WeatherDomain> const& weatherDomains() const;
  [[nodiscard]] std::expected<std::optional<std::reference_wrapper<WeatherDomain const>>, StarException> weatherDomain(String const& name) const;
  [[nodiscard]] std::expected<std::optional<std::reference_wrapper<WeatherLayer const>>, StarException> weatherLayerAt(Vec2I const& position) const;

  // Return potential items that would spawn at the given block.
  [[nodiscard]] std::expected<void, StarException> add_potential_biome_items(int x, int y, PotentialBiomeItems& items, List<BiomeItemDistribution> const& distributions, BiomePlacementArea area, std::optional<BiomePlacementMode> mode = {}) const;
  [[nodiscard]] std::expected<PotentialBiomeItems, StarException> potential_biome_items_at(int x, int y) const;

  // Return only the potential items that can spawn at the given block.
  [[nodiscard]] std::expected<List<BiomeItemPlacement>, StarException> valid_biome_items(int x, int y, PotentialBiomeItems potential_biome_items) const;

  [[nodiscard]] std::expected<float, StarException> gravity() const;
  float threatLevel() const;

  // For consistently seeding object generation at this position
  std::uint64_t seedFor(int x, int y) const;

private:
  struct CustomTerrainRegion {
    PolyF region;
    RectF regionBounds;
    bool solid;
  };

  WorldTemplate(Root& root);

  [[nodiscard]] std::expected<void, StarException> initialize();
  void install_world_parameters(std::shared_ptr<VisitableWorldParameters const> parameters);
  [[nodiscard]] std::expected<void, StarException> determineWorldName();
  [[nodiscard]] std::expected<void, StarException> setupWeatherDomains();

  [[nodiscard]] std::expected<std::pair<float, float>, StarException> customTerrainWeighting(int x, int y) const;

  // Calculates block info and adds to cache
  [[nodiscard]] std::expected<BlockInfo, StarException> getBlockInfo(std::uint32_t x, std::uint32_t y) const;

  Json m_templateConfig;
  float m_customTerrainBlendSize;
  float m_customTerrainBlendWeight;

  std::optional<CelestialParameters> m_celestialParameters;
  std::shared_ptr<VisitableWorldParameters const> m_worldParameters;
  std::shared_ptr<AmbientNoisesDescription const> m_ambient_override;
  std::shared_ptr<AmbientNoisesDescription const> m_music_override;
  SkyParameters m_skyParameters;
  std::uint64_t m_seed;
  WorldGeometry m_geometry;
  std::shared_ptr<WorldLayout> m_layout;
  String m_worldName;

  List<WeatherDomain> m_weatherDomains;
  List<WeatherLayer> m_weatherLayers;

  List<CustomTerrainRegion> m_customTerrainRegions;

  mutable HashLruCache<Vector<std::uint32_t, 2>, BlockInfo> m_blockCache;
};

}

export module star.world_template;

export namespace Star {
  using ::Star::WorldTemplate;
  using ::Star::CelestialDatabase;
  using ::Star::AmbientNoisesDescription;
}

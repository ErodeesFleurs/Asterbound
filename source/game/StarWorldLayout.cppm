module;
import std;
import star.string;
import star.format;
import star.json;
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
import star.vector;

import star.animation;
import star.particle;
import star.weather_types;
import star.celestial_coordinate;
import star.sky_types;
import star.force_regions;
import star.world_parameters;
import star.celestial_parameters;

namespace Star {

struct Biome;
struct TerrainSelector;

using BiomeIndex = std::uint8_t;
inline constexpr BiomeIndex NullBiomeIndex = 0;

using TerrainSelectorIndex = std::uint32_t;
inline constexpr TerrainSelectorIndex NullTerrainSelectorIndex = 0;

struct WorldRegionLiquids {
  LiquidId caveLiquid;
  float caveLiquidSeedDensity;

  LiquidId oceanLiquid;
  int oceanLiquidLevel;

  bool encloseLiquids;
  bool fillMicrodungeons;
};

struct WorldRegion {
  WorldRegion();
  [[nodiscard]] static std::expected<WorldRegion, StarException> create(Json const& store);

  Json toJson() const;

  TerrainSelectorIndex terrainSelectorIndex;
  TerrainSelectorIndex foregroundCaveSelectorIndex;
  TerrainSelectorIndex backgroundCaveSelectorIndex;

  BiomeIndex blockBiomeIndex;
  BiomeIndex environmentBiomeIndex;

  List<TerrainSelectorIndex> subBlockSelectorIndexes;
  List<TerrainSelectorIndex> foregroundOreSelectorIndexes;
  List<TerrainSelectorIndex> backgroundOreSelectorIndexes;

  WorldRegionLiquids regionLiquids;
};

class Root;
class WorldLayout {
  Root& m_root;
  Root& root() const { return m_root; }
public:
  struct BlockNoise {
    [[nodiscard]] static std::expected<BlockNoise, StarException> build(Json const& config, std::uint64_t seed);

    BlockNoise();
    [[nodiscard]] static std::expected<BlockNoise, StarException> create(Json const& store);

    [[nodiscard]] std::expected<Json, StarException> toJson() const;

    [[nodiscard]] std::expected<Vec2I, StarException> apply(Vec2I const& input, Vec2U const& worldSize) const;

    // Individual noise only applied for horizontal / vertical biome transitions
    PerlinF horizontalNoise;
    PerlinF verticalNoise;

    // 2 dimensional biome noise field for fine grained noise
    PerlinF xNoise;
    PerlinF yNoise;
  };

  struct RegionWeighting {
    float weight;
    int xValue;
    std::reference_wrapper<WorldRegion const> region;
  };

  [[nodiscard]] static std::expected<WorldLayout, StarException> buildTerrestrialLayout(Root& root, TerrestrialWorldParameters const& terrestrialParameters, std::uint64_t seed);
  [[nodiscard]] static std::expected<WorldLayout, StarException> buildAsteroidsLayout(Root& root, AsteroidsWorldParameters const& asteroidParameters, std::uint64_t seed);
  [[nodiscard]] static std::expected<WorldLayout, StarException> buildFloatingDungeonLayout(Root& root, FloatingDungeonWorldParameters const& floatingDungeonParameters, std::uint64_t seed);

  WorldLayout(Root& root);
  [[nodiscard]] static std::expected<std::shared_ptr<WorldLayout>, StarException> create(Root& root, Json const& store);

  [[nodiscard]] std::expected<Json, StarException> toJson() const;

  std::optional<BlockNoise> const& blockNoise() const;
  std::optional<PerlinF> const& blendNoise() const;

  List<RectI> playerStartSearchRegions() const;

  [[nodiscard]] std::expected<std::reference_wrapper<std::shared_ptr<Biome const> const>, StarException> getBiome(BiomeIndex index) const;
  [[nodiscard]] std::expected<std::reference_wrapper<std::shared_ptr<TerrainSelector const> const>, StarException> getTerrainSelector(TerrainSelectorIndex index) const;

  // Will return region weighting in order of greatest to least weighting.
  [[nodiscard]] std::expected<List<RegionWeighting>, StarException> getWeighting(int x, int y) const;

  [[nodiscard]] std::expected<List<RectI>, StarException> previewAddBiomeRegion(Vec2I const& position, int width) const;
  [[nodiscard]] std::expected<List<RectI>, StarException> previewExpandBiomeRegion(Vec2I const& position, int width) const;

  [[nodiscard]] std::expected<void, StarException> addBiomeRegion(TerrestrialWorldParameters const& terrestrialParameters, std::uint64_t seed, Vec2I const& position, String biomeName, String const& subBlockSelector, int width);
  [[nodiscard]] std::expected<void, StarException> expandBiomeRegion(Vec2I const& position, int newWidth);

  // sets the environment biome index for all regions in the current layer
  // to the biome at the specified position, and returns the name of the biome
  [[nodiscard]] std::expected<String, StarException> setLayerEnvironmentBiome(Vec2I const& position);

  std::pair<std::size_t, std::size_t> findLayerAndCell(int x, int y) const;

private:
  struct WorldLayer {
    WorldLayer();

    int yStart;
    Deque<int> boundaries;
    Deque<std::shared_ptr<WorldRegion>> cells;
  };

  struct RegionParams {
    int baseHeight;
    float threatLevel;
    std::optional<String> biomeName;
    std::optional<String> terrainSelector;
    std::optional<String> fgCaveSelector;
    std::optional<String> bgCaveSelector;
    std::optional<String> fgOreSelector;
    std::optional<String> bgOreSelector;
    std::optional<String> subBlockSelector;
    WorldRegionLiquids regionLiquids;
  };

  [[nodiscard]] std::expected<std::pair<WorldLayer, List<RectI>>, StarException> expandRegionInLayer(WorldLayer targetLayer, std::size_t cellIndex, int newWidth) const;

  BiomeIndex registerBiome(std::shared_ptr<Biome const> biome);
  TerrainSelectorIndex registerTerrainSelector(std::shared_ptr<TerrainSelector const> terrainSelector);

  [[nodiscard]] std::expected<WorldRegion, StarException> buildRegion(std::uint64_t seed, RegionParams const& regionParams);
  [[nodiscard]] std::expected<void, StarException> addLayer(std::uint64_t seed, int yStart, RegionParams regionParams);
  [[nodiscard]] std::expected<void, StarException> addLayer(std::uint64_t seed, int yStart, int yBase, String const& primaryBiome,
      RegionParams primaryRegionParams, RegionParams primarySubRegionParams,
      List<RegionParams> secondaryRegions, List<RegionParams> secondarySubRegions,
      Vec2F secondaryRegionSize, Vec2F subRegionSize);
  [[nodiscard]] std::expected<void, StarException> finalize(Color mainSkyColor);

  std::pair<std::size_t, int> findContainingCell(WorldLayer const& layer, int x) const;
  std::pair<std::size_t, int> leftCell(WorldLayer const& layer, std::size_t cellIndex, int x) const;
  std::pair<std::size_t, int> rightCell(WorldLayer const& layer, std::size_t cellIndex, int x) const;

  Vec2U m_worldSize;

  List<std::shared_ptr<Biome const>> m_biomes;
  List<std::shared_ptr<TerrainSelector const>> m_terrainSelectors;

  List<WorldLayer> m_layers;

  float m_regionBlending;
  std::optional<BlockNoise> m_blockNoise;
  std::optional<PerlinF> m_blendNoise;
  List<RectI> m_playerStartSearchRegions;
};


[[nodiscard]] inline std::expected<std::reference_wrapper<std::shared_ptr<Biome const> const>, StarException> WorldLayout::getBiome(BiomeIndex index) const {
  if (index == NullBiomeIndex || index > m_biomes.size())
    return std::unexpected(StarException("WorldLayout::getTerrainSelector called with null or out of range BiomeIndex"));
  return std::cref(m_biomes[index - 1]);
}

[[nodiscard]] inline std::expected<std::reference_wrapper<std::shared_ptr<TerrainSelector const> const>, StarException> WorldLayout::getTerrainSelector(TerrainSelectorIndex index) const {
  if (index == NullBiomeIndex || index > m_terrainSelectors.size())
    return std::unexpected(StarException("WorldLayout::getTerrainSelector called with null or out of range TerrainSelectorIndex"));
  return std::cref(m_terrainSelectors[index - 1]);
}

}

export module star.world_layout;

export namespace Star {
  using ::Star::Biome;
  using ::Star::TerrainSelector;
  using ::Star::WorldRegion;
  using ::Star::WorldLayout;
  using ::Star::BiomeIndex;
  using ::Star::NullBiomeIndex;
  using ::Star::TerrainSelectorIndex;
  using ::Star::NullTerrainSelectorIndex;
  using ::Star::WorldRegionLiquids;
}

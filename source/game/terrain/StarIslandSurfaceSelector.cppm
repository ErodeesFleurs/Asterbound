module;
import std;
import star.format;
import star.json;
import star.string;
import star.json_extra;

import star.ordered_map;
import star.lru_cache;
import star.bi_map;
import star.random;
import star.perlin;
import star.game_types;
import star.math_common;

export module star.terrain_island_surface;
import star.terrain_database;

import star.static_random;
export namespace Star {

struct IslandColumn {
  float topLevel;
  float bottomLevel;
};

struct IslandSurfaceSelector : TerrainSelector {
  static std::string_view const Name;

  [[nodiscard]] static std::expected<std::shared_ptr<IslandSurfaceSelector>, StarException> create(Json const& config, TerrainSelectorParameters const& parameters);
private:
  struct ConstructionKey { explicit ConstructionKey() = default; };
public:
  IslandSurfaceSelector(ConstructionKey, Json const& config, TerrainSelectorParameters const& parameters) : TerrainSelector(Name, config, parameters) {}

  [[nodiscard]] std::expected<float, StarException> get(int x, int y) const override;

  [[nodiscard]] std::expected<IslandColumn, StarException> generateColumn(int x) const;

  mutable HashLruCache<int, IslandColumn> columnCache;

  PerlinF islandHeight;
  PerlinF islandDepth;
  PerlinF islandDecision;

  float islandTaperPoint;
  float islandElevation;

  float layerBaseHeight;
  int worldWidth;
};

}

namespace Star {

std::string_view const IslandSurfaceSelector::Name = "islandSurface";

std::expected<std::shared_ptr<IslandSurfaceSelector>, StarException> IslandSurfaceSelector::create(Json const& config, TerrainSelectorParameters const& parameters) {
  auto result = std::make_shared<IslandSurfaceSelector>(ConstructionKey{}, config, parameters);
  result->layerBaseHeight = parameters.baseHeight;
  result->worldWidth = parameters.worldWidth;
  auto elevation = config.getFloat("islandElevation");
  if (!elevation)
    return std::unexpected(std::move(elevation.error()));
  result->islandElevation = *elevation;
  auto taper = config.getFloat("islandTaperPoint");
  if (!taper)
    return std::unexpected(std::move(taper.error()));
  result->islandTaperPoint = *taper;
  auto readFunction = [&](String const& name) -> std::expected<PerlinF, StarException> {
    auto functionConfig = config.getObject(name);
    if (!functionConfig)
      return std::unexpected(std::move(functionConfig.error()));
    return PerlinF::create(*functionConfig, static_random_u64(parameters.seed, parameters.baseHeight, name));
  };
  auto height = readFunction("islandHeight");
  if (!height)
    return std::unexpected(std::move(height.error()));
  result->islandHeight = std::move(*height);
  auto depth = readFunction("islandDepth");
  if (!depth)
    return std::unexpected(std::move(depth.error()));
  result->islandDepth = std::move(*depth);
  auto decision = readFunction("islandDecision");
  if (!decision)
    return std::unexpected(std::move(decision.error()));
  result->islandDecision = std::move(*decision);
  return result;
}

std::expected<IslandColumn, StarException> IslandSurfaceSelector::generateColumn(int x) const {
  float noiseAngle = 2 * std::numbers::pi * x / worldWidth;
  float noiseX = (std::cos(noiseAngle) * worldWidth) / (2 * std::numbers::pi);
  float noiseY = (std::sin(noiseAngle) * worldWidth) / (2 * std::numbers::pi);

  IslandColumn newCol;
  auto decision = islandDecision.get(noiseX, noiseY);
  if (!decision)
    return std::unexpected(std::move(decision.error()));
  float thisIslandDecision = *decision;
  if (thisIslandDecision > 0) {
    float taperFactor = thisIslandDecision < islandTaperPoint ? std::sin((0.5 * std::numbers::pi * thisIslandDecision) / islandTaperPoint) : 1.0f;
    auto height = islandHeight.get(noiseX, noiseY);
    if (!height)
      return std::unexpected(std::move(height.error()));
    newCol.topLevel = islandElevation + layerBaseHeight + taperFactor * *height;
    auto depth = islandDepth.get(noiseX, noiseY);
    if (!depth)
      return std::unexpected(std::move(depth.error()));
    newCol.bottomLevel = islandElevation + layerBaseHeight - taperFactor * *depth;
  } else {
    newCol.topLevel = layerBaseHeight;
    newCol.bottomLevel = layerBaseHeight;
  }

  return newCol;
}

std::expected<float, StarException> IslandSurfaceSelector::get(int x, int y) const {
  auto col = columnCache.get(x, [this](int x) {
    return IslandSurfaceSelector::generateColumn(x);
  });
  if (!col)
    return std::unexpected(std::move(col.error()));
  return (col->get().topLevel - col->get().bottomLevel) / 2 - std::abs((col->get().topLevel + col->get().bottomLevel) / 2 - y);
}

}

module;
import std;
import star.format;
import star.list;
import star.json;
import star.json_extra;

import star.ordered_map;
import star.lru_cache;
import star.vector;
import star.bi_map;
import star.random;
import star.perlin;
import star.game_types;
import star.logging;
import star.math_common;

export module star.terrain_karst_cave;
import star.terrain_database;

import star.static_random;
export namespace Star {

class KarstCaveSelector : public TerrainSelector {
public:
  static std::string_view const Name;

  [[nodiscard]] static std::expected<std::shared_ptr<KarstCaveSelector>, StarException> create(Json const& config, TerrainSelectorParameters const& parameters);
private:
  struct ConstructionKey { explicit ConstructionKey() = default; };
public:
  KarstCaveSelector(ConstructionKey, Json const& config, TerrainSelectorParameters const& parameters) : TerrainSelector(Name, config, parameters) {}

  [[nodiscard]] std::expected<float, StarException> get(int x, int y) const override;

private:
  struct LayerPerlins {
    PerlinF cave_decision;
    PerlinF layer_height;
    PerlinF cave_height;
    PerlinF cave_floor;
  };

  struct Sector {
    Sector(KarstCaveSelector const& parent, Vec2I sector)
        : parent(parent), sector(sector), values(square(parent.m_sectorSize)) {}
    [[nodiscard]] static std::expected<Sector, StarException> create(KarstCaveSelector const& parent, Vec2I sector);

    float get(int x, int y);

    bool inside(int x, int y);
    void set(int x, int y, float value);

    std::reference_wrapper<KarstCaveSelector const> parent;
    Vec2I sector;
    List<float> values;

    float m_maxValue = 0;
  };

  [[nodiscard]] std::expected<std::reference_wrapper<LayerPerlins const>, StarException> layer_perlins(int y) const;

  int m_sectorSize;
  int m_layerResolution;
  float m_layerDensity;
  int m_bufferHeight;
  float m_caveTaperPoint;

  PerlinF::Parameters cave_decision_parameters;
  PerlinF::Parameters layer_height_parameters;
  PerlinF::Parameters cave_height_parameters;
  PerlinF::Parameters cave_floor_parameters;

  int m_worldWidth;
  std::uint64_t m_seed;

  mutable HashLruCache<int, LayerPerlins> layer_cache;
  mutable HashLruCache<Vec2I, Sector> m_sectorCache;
};

}

namespace Star {

std::string_view const KarstCaveSelector::Name = "karstcave";

std::expected<std::shared_ptr<KarstCaveSelector>, StarException> KarstCaveSelector::create(Json const& config, TerrainSelectorParameters const& parameters) {
  auto result = std::make_shared<KarstCaveSelector>(ConstructionKey{}, config, parameters);
  auto sectorSize = config.get_uint("sectorSize", 64);
  if (!sectorSize)
    return std::unexpected(std::move(sectorSize.error()));
  result->m_sectorSize = *sectorSize;
  auto layerResolution = config.get_int("layerResolution");
  if (!layerResolution)
    return std::unexpected(std::move(layerResolution.error()));
  result->m_layerResolution = *layerResolution;
  auto layerDensity = config.get_float("layerDensity");
  if (!layerDensity)
    return std::unexpected(std::move(layerDensity.error()));
  result->m_layerDensity = *layerDensity;
  auto bufferHeight = config.get_int("bufferHeight");
  if (!bufferHeight)
    return std::unexpected(std::move(bufferHeight.error()));
  result->m_bufferHeight = *bufferHeight;
  auto caveTaperPoint = config.get_float("caveTaperPoint");
  if (!caveTaperPoint)
    return std::unexpected(std::move(caveTaperPoint.error()));
  result->m_caveTaperPoint = *caveTaperPoint;
  auto caveDecision = config.get("caveDecision");
  if (!caveDecision)
    return std::unexpected(std::move(caveDecision.error()));
  auto layerHeight = config.get("layerHeightVariation");
  if (!layerHeight)
    return std::unexpected(std::move(layerHeight.error()));
  auto caveHeight = config.get("caveHeightVariation");
  if (!caveHeight)
    return std::unexpected(std::move(caveHeight.error()));
  auto caveFloor = config.get("caveFloorVariation");
  if (!caveFloor)
    return std::unexpected(std::move(caveFloor.error()));
  result->m_worldWidth = parameters.worldWidth;
  result->m_seed = parameters.seed;
  auto layerCacheSize = config.get_uint("layerPerlinsCacheSize", 16);
  if (!layerCacheSize)
    return std::unexpected(std::move(layerCacheSize.error()));
  result->layer_cache.set_capacity(*layerCacheSize);
  auto sectorCacheSize = config.get_uint("sectorCacheSize", 16);
  if (!sectorCacheSize)
    return std::unexpected(std::move(sectorCacheSize.error()));
  result->m_sectorCache.set_capacity(*sectorCacheSize);

  // Check the selector's own fields first, as before. Only the layer seed
  // changes on a cache miss; the four noise recipes belong to the selector.
  auto decision_parameters = PerlinF::parse_parameters(*caveDecision);
  if (!decision_parameters) return std::unexpected(std::move(decision_parameters.error()));
  result->cave_decision_parameters = *decision_parameters;
  auto layer_parameters = PerlinF::parse_parameters(*layerHeight);
  if (!layer_parameters) return std::unexpected(std::move(layer_parameters.error()));
  result->layer_height_parameters = *layer_parameters;
  auto height_parameters = PerlinF::parse_parameters(*caveHeight);
  if (!height_parameters) return std::unexpected(std::move(height_parameters.error()));
  result->cave_height_parameters = *height_parameters;
  auto floor_parameters = PerlinF::parse_parameters(*caveFloor);
  if (!floor_parameters) return std::unexpected(std::move(floor_parameters.error()));
  result->cave_floor_parameters = *floor_parameters;
  return result;
}

std::expected<float, StarException> KarstCaveSelector::get(int x, int y) const {
  Vec2I key = Vec2I(x - positive_mod(x, m_sectorSize), y - positive_mod(y, m_sectorSize));
  auto sector = m_sectorCache.get(key, [this](Vec2I const& key) {
    return Sector::create(*this, key);
  });
  if (!sector)
    return std::unexpected(std::move(sector.error()));
  return sector->get().get(x, y);
}

std::expected<KarstCaveSelector::Sector, StarException> KarstCaveSelector::Sector::create(KarstCaveSelector const& parent, Vec2I sector) {
  Sector result(parent, sector);


  for (int y = sector[1] - parent.m_bufferHeight; y < sector[1] + parent.m_sectorSize + parent.m_bufferHeight; y++) {
    float layerChance = parent.m_layerDensity * parent.m_layerResolution;
    // determine whether this layer has caves
    if (y % parent.m_layerResolution == 0 && static_random_float(parent.m_seed, y) <= layerChance) {
      auto perlins = parent.layer_perlins(y);
      if (!perlins)
        return std::unexpected(std::move(perlins.error()));
      LayerPerlins const& layer = perlins->get();

      // carve out cave layer
      for (int x = sector[0]; x < sector[0] + parent.m_sectorSize; x++) {
        // use wrapping noise
        float noiseAngle = 2 * std::numbers::pi * x / parent.m_worldWidth;
        float noiseX = (std::cos(noiseAngle) * parent.m_worldWidth) / (2 * std::numbers::pi);
        float noiseY = (std::sin(noiseAngle) * parent.m_worldWidth) / (2 * std::numbers::pi);

        // determine where caves be at
        auto caveDecision = layer.cave_decision.get(noiseX, noiseY);
        if (!caveDecision)
          return std::unexpected(std::move(caveDecision.error()));
        float isThereACaveHere = *caveDecision;
        if (isThereACaveHere > 0) {
          float taperFactor = isThereACaveHere < parent.m_caveTaperPoint
              ? std::sin((0.5 * std::numbers::pi * isThereACaveHere) / parent.m_caveTaperPoint)
              : 1.0f;

          auto layerHeight = layer.layer_height.get(noiseX, noiseY);
          if (!layerHeight)
            return std::unexpected(std::move(layerHeight.error()));
          int baseY = y + *layerHeight;
          auto caveHeight = layer.cave_height.get(noiseX, noiseY);
          if (!caveHeight)
            return std::unexpected(std::move(caveHeight.error()));
          int ceilingY = baseY + *caveHeight * taperFactor;
          auto caveFloor = layer.cave_floor.get(noiseX, noiseY);
          if (!caveFloor)
            return std::unexpected(std::move(caveFloor.error()));
          int floorY = baseY + *caveFloor * taperFactor;
          float halfHeight = std::abs(ceilingY - floorY + 1) / 2.0f;
          float midpointY = (floorY + ceilingY) / 2.0f;

          result.m_maxValue = std::max(result.m_maxValue, halfHeight);

          for (int pointY = floorY; pointY < ceilingY; pointY++)
            if (result.inside(x, pointY))
              result.set(x, pointY, std::max(result.get(x, pointY), halfHeight - std::abs(midpointY - pointY)));
        }
      }
    }
  }
  return result;
}

float KarstCaveSelector::Sector::get(int x, int y) {
  auto val = values[(x - sector[0]) + parent.get().m_sectorSize * (y - sector[1])];
  if (val > 0)
    return val;
  else
    return -m_maxValue;
}

bool KarstCaveSelector::Sector::inside(int x, int y) {
  int x_ = x - sector[0];
  if (x_ < 0 || x_ >= parent.get().m_sectorSize)
    return false;
  int y_ = y - sector[1];
  if (y_ < 0 || y_ >= parent.get().m_sectorSize)
    return false;
  return true;
}

void KarstCaveSelector::Sector::set(int x, int y, float value) {
  values[(x - sector[0]) + parent.get().m_sectorSize * (y - sector[1])] = value;
}

std::expected<std::reference_wrapper<KarstCaveSelector::LayerPerlins const>, StarException> KarstCaveSelector::layer_perlins(int y) const {
  auto result = layer_cache.get(y, [this](int layer) {
    return LayerPerlins{
      .cave_decision = PerlinF(cave_decision_parameters, static_random_u64(layer, m_seed, "CaveDecision")),
      .layer_height = PerlinF(layer_height_parameters, static_random_u64(layer, m_seed, "LayerHeightVariation")),
      .cave_height = PerlinF(cave_height_parameters, static_random_u64(layer, m_seed, "CaveHeightVariation")),
      .cave_floor = PerlinF(cave_floor_parameters, static_random_u64(layer, m_seed, "CaveFloorVariation"))};
  });
  if (!result)
    return std::unexpected(std::move(result.error()));
  return std::cref(result->get());
}

}

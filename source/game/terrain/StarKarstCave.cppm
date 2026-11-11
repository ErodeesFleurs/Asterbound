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
    PerlinF caveDecision;
    PerlinF layerHeightVariation;
    PerlinF caveHeightVariation;
    PerlinF caveFloorVariation;
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

  [[nodiscard]] std::expected<std::reference_wrapper<LayerPerlins const>, StarException> layerPerlins(int y) const;

  int m_sectorSize;
  int m_layerResolution;
  float m_layerDensity;
  int m_bufferHeight;
  float m_caveTaperPoint;

  Json m_caveDecisionPerlinConfig;
  Json m_layerHeightVariationPerlinConfig;
  Json m_caveHeightVariationPerlinConfig;
  Json m_caveFloorVariationPerlinConfig;

  int m_worldWidth;
  std::uint64_t m_seed;

  mutable HashLruCache<int, LayerPerlins> m_layerPerlinsCache;
  mutable HashLruCache<Vec2I, Sector> m_sectorCache;
};

}

namespace Star {

std::string_view const KarstCaveSelector::Name = "karstcave";

std::expected<std::shared_ptr<KarstCaveSelector>, StarException> KarstCaveSelector::create(Json const& config, TerrainSelectorParameters const& parameters) {
  auto result = std::make_shared<KarstCaveSelector>(ConstructionKey{}, config, parameters);
  auto sectorSize = config.getUInt("sectorSize", 64);
  if (!sectorSize)
    return std::unexpected(std::move(sectorSize.error()));
  result->m_sectorSize = *sectorSize;
  auto layerResolution = config.getInt("layerResolution");
  if (!layerResolution)
    return std::unexpected(std::move(layerResolution.error()));
  result->m_layerResolution = *layerResolution;
  auto layerDensity = config.getFloat("layerDensity");
  if (!layerDensity)
    return std::unexpected(std::move(layerDensity.error()));
  result->m_layerDensity = *layerDensity;
  auto bufferHeight = config.getInt("bufferHeight");
  if (!bufferHeight)
    return std::unexpected(std::move(bufferHeight.error()));
  result->m_bufferHeight = *bufferHeight;
  auto caveTaperPoint = config.getFloat("caveTaperPoint");
  if (!caveTaperPoint)
    return std::unexpected(std::move(caveTaperPoint.error()));
  result->m_caveTaperPoint = *caveTaperPoint;
  auto caveDecision = config.get("caveDecision");
  if (!caveDecision)
    return std::unexpected(std::move(caveDecision.error()));
  result->m_caveDecisionPerlinConfig = std::move(*caveDecision);
  auto layerHeight = config.get("layerHeightVariation");
  if (!layerHeight)
    return std::unexpected(std::move(layerHeight.error()));
  result->m_layerHeightVariationPerlinConfig = std::move(*layerHeight);
  auto caveHeight = config.get("caveHeightVariation");
  if (!caveHeight)
    return std::unexpected(std::move(caveHeight.error()));
  result->m_caveHeightVariationPerlinConfig = std::move(*caveHeight);
  auto caveFloor = config.get("caveFloorVariation");
  if (!caveFloor)
    return std::unexpected(std::move(caveFloor.error()));
  result->m_caveFloorVariationPerlinConfig = std::move(*caveFloor);
  result->m_worldWidth = parameters.worldWidth;
  result->m_seed = parameters.seed;
  auto layerCacheSize = config.getUInt("layerPerlinsCacheSize", 16);
  if (!layerCacheSize)
    return std::unexpected(std::move(layerCacheSize.error()));
  result->m_layerPerlinsCache.set_capacity(*layerCacheSize);
  auto sectorCacheSize = config.getUInt("sectorCacheSize", 16);
  if (!sectorCacheSize)
    return std::unexpected(std::move(sectorCacheSize.error()));
  result->m_sectorCache.set_capacity(*sectorCacheSize);
  return result;
}

std::expected<float, StarException> KarstCaveSelector::get(int x, int y) const {
  Vec2I key = Vec2I(x - pmod(x, m_sectorSize), y - pmod(y, m_sectorSize));
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
      auto perlins = parent.layerPerlins(y);
      if (!perlins)
        return std::unexpected(std::move(perlins.error()));
      LayerPerlins const& layerPerlins = perlins->get();

      // carve out cave layer
      for (int x = sector[0]; x < sector[0] + parent.m_sectorSize; x++) {
        // use wrapping noise
        float noiseAngle = 2 * std::numbers::pi * x / parent.m_worldWidth;
        float noiseX = (std::cos(noiseAngle) * parent.m_worldWidth) / (2 * std::numbers::pi);
        float noiseY = (std::sin(noiseAngle) * parent.m_worldWidth) / (2 * std::numbers::pi);

        // determine where caves be at
        auto caveDecision = layerPerlins.caveDecision.get(noiseX, noiseY);
        if (!caveDecision)
          return std::unexpected(std::move(caveDecision.error()));
        float isThereACaveHere = *caveDecision;
        if (isThereACaveHere > 0) {
          float taperFactor = isThereACaveHere < parent.m_caveTaperPoint
              ? std::sin((0.5 * std::numbers::pi * isThereACaveHere) / parent.m_caveTaperPoint)
              : 1.0f;

          auto layerHeight = layerPerlins.layerHeightVariation.get(noiseX, noiseY);
          if (!layerHeight)
            return std::unexpected(std::move(layerHeight.error()));
          int baseY = y + *layerHeight;
          auto caveHeight = layerPerlins.caveHeightVariation.get(noiseX, noiseY);
          if (!caveHeight)
            return std::unexpected(std::move(caveHeight.error()));
          int ceilingY = baseY + *caveHeight * taperFactor;
          auto caveFloor = layerPerlins.caveFloorVariation.get(noiseX, noiseY);
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

std::expected<std::reference_wrapper<KarstCaveSelector::LayerPerlins const>, StarException> KarstCaveSelector::layerPerlins(int y) const {
  auto result = m_layerPerlinsCache.get(y, [this](int y) -> std::expected<LayerPerlins, StarException> {
    auto decision = PerlinF::create(m_caveDecisionPerlinConfig, static_random_u64(y, m_seed, "CaveDecision"));
    if (!decision)
      return std::unexpected(std::move(decision.error()));
    auto layerHeight =
      PerlinF::create(m_layerHeightVariationPerlinConfig, static_random_u64(y, m_seed, "LayerHeightVariation"));
    if (!layerHeight)
      return std::unexpected(std::move(layerHeight.error()));
    auto caveHeight =
      PerlinF::create(m_caveHeightVariationPerlinConfig, static_random_u64(y, m_seed, "CaveHeightVariation"));
    if (!caveHeight)
      return std::unexpected(std::move(caveHeight.error()));
    auto caveFloor =
      PerlinF::create(m_caveFloorVariationPerlinConfig, static_random_u64(y, m_seed, "CaveFloorVariation"));
    if (!caveFloor)
      return std::unexpected(std::move(caveFloor.error()));
    return LayerPerlins{.caveDecision = std::move(*decision),
                        .layerHeightVariation = std::move(*layerHeight),
                        .caveHeightVariation = std::move(*caveHeight),
                        .caveFloorVariation = std::move(*caveFloor)};
  });
  if (!result)
    return std::unexpected(std::move(result.error()));
  return std::cref(result->get());
}

}

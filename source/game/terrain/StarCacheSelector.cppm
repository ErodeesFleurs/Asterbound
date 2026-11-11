module;
import std;
import star.format;
import star.string;
import star.json;

import star.ordered_map;
import star.lru_cache;
import star.vector;

export module star.terrain_cache;
import star.terrain_database;

export namespace Star {

struct CacheSelector : TerrainSelector {
  static std::string_view const Name;

  [[nodiscard]] static std::expected<std::shared_ptr<CacheSelector>, StarException> create(Json const& config, TerrainSelectorParameters const& parameters, TerrainDatabase const& database);
private:
  struct ConstructionKey { explicit ConstructionKey() = default; };
public:
  CacheSelector(ConstructionKey, Json const& config, TerrainSelectorParameters const& parameters) : TerrainSelector(Name, config, parameters) {}

  [[nodiscard]] std::expected<float, StarException> get(int x, int y) const override;

  std::shared_ptr<TerrainSelector const> m_source;
  mutable HashLruCache<Vec2I, float> m_cache;
};

}

namespace Star {

std::string_view const CacheSelector::Name = "cache";

std::expected<std::shared_ptr<CacheSelector>, StarException> CacheSelector::create(
    Json const& config, TerrainSelectorParameters const& parameters, TerrainDatabase const& database) {
  auto result = std::make_shared<CacheSelector>(ConstructionKey{}, config, parameters);
  auto sourceConfig = config.get("source");
  if (!sourceConfig)
    return std::unexpected(std::move(sourceConfig.error()));
  auto sourceType = sourceConfig->getString("type");
  if (!sourceType)
    return std::unexpected(std::move(sourceType.error()));
  auto seedBias = sourceConfig->getUInt("seedBias", 0);
  if (!seedBias)
    return std::unexpected(std::move(seedBias.error()));
  TerrainSelectorParameters sourceParameters = parameters;
  sourceParameters.seed += *seedBias;
  auto source = database.createSelectorType(result->type, *sourceConfig, sourceParameters);
  if (!source)
    return std::unexpected(std::move(source.error()));
  result->m_source = std::move(*source);
  auto cacheSize = config.getUInt("lruCacheSize", 20000);
  if (!cacheSize)
    return std::unexpected(std::move(cacheSize.error()));
  result->m_cache.set_capacity(*cacheSize);
  return result;
}

std::expected<float, StarException> CacheSelector::get(int x, int y) const {
  auto value = m_cache.get(Vec2I(x, y), [this](Vec2I const& key) {
    return m_source->get(key[0], key[1]);
  });
  if (!value)
    return std::unexpected(std::move(value.error()));
  return value->get();
}

}

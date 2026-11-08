module;
import std;
import star.format;
import star.string;
import star.list;
import star.json;

import star.math_common;
import star.interpolation;

export module star.terrain_composition;
import star.terrain_database;

export namespace Star {

struct MaxSelector : TerrainSelector {
  static std::string_view const Name;

  [[nodiscard]] static std::expected<std::shared_ptr<MaxSelector>, StarException> create(Json const& config, TerrainSelectorParameters const& parameters, TerrainDatabase const& database);
private:
  struct ConstructionKey { explicit ConstructionKey() = default; };
public:
  MaxSelector(ConstructionKey, Json const& config, TerrainSelectorParameters const& parameters) : TerrainSelector(Name, config, parameters) {}

  [[nodiscard]] std::expected<float, StarException> get(int x, int y) const override;

  List<std::shared_ptr<TerrainSelector const>> m_sources;
};



struct MinMaxSelector : TerrainSelector {
  static std::string_view const Name;

  [[nodiscard]] static std::expected<std::shared_ptr<MinMaxSelector>, StarException> create(Json const& config, TerrainSelectorParameters const& parameters, TerrainDatabase const& database);
private:
  struct ConstructionKey { explicit ConstructionKey() = default; };
public:
  MinMaxSelector(ConstructionKey, Json const& config, TerrainSelectorParameters const& parameters) : TerrainSelector(Name, config, parameters) {}

  [[nodiscard]] std::expected<float, StarException> get(int x, int y) const override;

  List<std::shared_ptr<TerrainSelector const>> m_sources;
};



struct MixSelector : TerrainSelector {
  static std::string_view const Name;

  [[nodiscard]] static std::expected<std::shared_ptr<MixSelector>, StarException> create(Json const& config, TerrainSelectorParameters const& parameters, TerrainDatabase const& database);
private:
  struct ConstructionKey { explicit ConstructionKey() = default; };
public:
  MixSelector(ConstructionKey, Json const& config, TerrainSelectorParameters const& parameters) : TerrainSelector(Name, config, parameters) {}

  [[nodiscard]] std::expected<float, StarException> get(int x, int y) const override;

  std::shared_ptr<TerrainSelector const> m_mixSource;
  std::shared_ptr<TerrainSelector const> m_aSource;
  std::shared_ptr<TerrainSelector const> m_bSource;
};

}

namespace Star {

std::string_view const MaxSelector::Name = "max";

std::expected<std::shared_ptr<MaxSelector>, StarException> MaxSelector::create(
    Json const& config, TerrainSelectorParameters const& parameters, TerrainDatabase const& database) {
  auto result = std::make_shared<MaxSelector>(ConstructionKey{}, config, parameters);
  auto sources = config.getArray("sources");
  if (!sources)
    return std::unexpected(std::move(sources.error()));
  for (auto const& sourceConfig : *sources) {
    auto sourceType = sourceConfig.getString("type");
    if (!sourceType)
      return std::unexpected(std::move(sourceType.error()));
    auto seedBias = sourceConfig.getUInt("seedBias", 0);
    if (!seedBias)
      return std::unexpected(std::move(seedBias.error()));
    TerrainSelectorParameters sourceParameters = parameters;
    sourceParameters.seed += *seedBias;
    auto source = database.createSelectorType(*sourceType, sourceConfig, sourceParameters);
    if (!source)
      return std::unexpected(std::move(source.error()));
    result->m_sources.push_back(std::move(*source));
  }
  return result;
}

std::expected<float, StarException> MaxSelector::get(int x, int y) const {
  float value = std::numeric_limits<float>::lowest();
  for (auto const& source : m_sources) {
    auto sample = source->get(x, y);
    if (!sample)
      return std::unexpected(std::move(sample.error()));
    value = std::max(value, *sample);
  }
  return value;
}



std::string_view const MinMaxSelector::Name = "minmax";

std::expected<std::shared_ptr<MinMaxSelector>, StarException> MinMaxSelector::create(
    Json const& config, TerrainSelectorParameters const& parameters, TerrainDatabase const& database) {
  auto result = std::make_shared<MinMaxSelector>(ConstructionKey{}, config, parameters);
  auto sources = config.getArray("sources");
  if (!sources)
    return std::unexpected(std::move(sources.error()));
  for (auto const& sourceConfig : *sources) {
    auto sourceType = sourceConfig.getString("type");
    if (!sourceType)
      return std::unexpected(std::move(sourceType.error()));
    auto seedBias = sourceConfig.getUInt("seedBias", 0);
    if (!seedBias)
      return std::unexpected(std::move(seedBias.error()));
    TerrainSelectorParameters sourceParameters = parameters;
    sourceParameters.seed += *seedBias;
    auto source = database.createSelectorType(*sourceType, sourceConfig, sourceParameters);
    if (!source)
      return std::unexpected(std::move(source.error()));
    result->m_sources.push_back(std::move(*source));
  }
  return result;
}

std::expected<float, StarException> MinMaxSelector::get(int x, int y) const {
  float value = 0.0f;
  for (auto const& source : m_sources) {
    auto sample = source->get(x, y);
    if (!sample)
      return std::unexpected(std::move(sample.error()));
    float srcVal = *sample;
    if (value > 0 || srcVal > 0)
      value = std::max(value, srcVal);
    else
      value = std::min(value, srcVal);
  }
  return value;
}



std::string_view const MixSelector::Name = "mix";

std::expected<std::shared_ptr<MixSelector>, StarException> MixSelector::create(
    Json const& config, TerrainSelectorParameters const& parameters, TerrainDatabase const& database) {
  auto result = std::make_shared<MixSelector>(ConstructionKey{}, config, parameters);
  auto readSource = [&](String const& name) -> std::expected<std::shared_ptr<TerrainSelector const>, StarException> {
    auto sourceConfig = config.get(name);
    if (!sourceConfig)
      return std::unexpected(std::move(sourceConfig.error()));
    auto type = sourceConfig->getString("type");
    if (!type)
      return std::unexpected(std::move(type.error()));
    return database.createSelectorType(*type, *sourceConfig, parameters);
  };
  auto mixSource = readSource("mixSource");
  if (!mixSource)
    return std::unexpected(std::move(mixSource.error()));
  result->m_mixSource = std::move(*mixSource);
  auto aSource = readSource("aSource");
  if (!aSource)
    return std::unexpected(std::move(aSource.error()));
  result->m_aSource = std::move(*aSource);
  auto bSource = readSource("bSource");
  if (!bSource)
    return std::unexpected(std::move(bSource.error()));
  result->m_bSource = std::move(*bSource);
  return result;
}

std::expected<float, StarException> MixSelector::get(int x, int y) const {
  auto mix = m_mixSource->get(x, y);
  if (!mix)
    return std::unexpected(std::move(mix.error()));
  auto f = std::clamp(*mix, -1.0f, 1.0f);
  if (f == -1)
    return m_aSource->get(x, y);
  if (f == 1)
    return m_bSource->get(x, y);
  auto a = m_aSource->get(x, y);
  if (!a)
    return std::unexpected(std::move(a.error()));
  auto b = m_bSource->get(x, y);
  if (!b)
    return std::unexpected(std::move(b.error()));
  return lerp(f * 0.5f + 0.5f, *a, *b);
}

}

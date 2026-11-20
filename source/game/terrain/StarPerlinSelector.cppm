module;
import std;
import star.format;
import star.json;
import star.string_view;

import star.bi_map;
import star.random;
import star.perlin;

export module star.terrain_perlin;
import star.terrain_database;

export namespace Star {

struct PerlinSelector : TerrainSelector {
  static std::string_view const Name;

  [[nodiscard]] static std::expected<std::shared_ptr<PerlinSelector>, StarException> create(Json const& config, TerrainSelectorParameters const& parameters);
private:
  struct ConstructionKey { explicit ConstructionKey() = default; };
public:
  PerlinSelector(ConstructionKey, Json const& config, TerrainSelectorParameters const& parameters, PerlinF value, float x, float y)
    : TerrainSelector(Name, config, parameters), function(std::move(value)), xInfluence(x), yInfluence(y) {}

  [[nodiscard]] std::expected<float, StarException> get(int x, int y) const override;

  PerlinF function;

  float xInfluence;
  float yInfluence;
};

}

namespace Star {

std::string_view const PerlinSelector::Name = "perlin";

std::expected<std::shared_ptr<PerlinSelector>, StarException> PerlinSelector::create(Json const& config, TerrainSelectorParameters const& parameters) {
  RandomSource random(parameters.seed);
  auto functionName = config.getString("function");
  if (!functionName)
    return std::unexpected(std::move(functionName.error()));
  auto type = enum_value<PerlinTypeNames>(StringView(*functionName));
  if (!type)
    return std::unexpected(std::move(type.error()));
  auto octaves = config.getFloat("octaves");
  if (!octaves)
    return std::unexpected(std::move(octaves.error()));
  auto freq = config.getFloat("freq");
  if (!freq)
    return std::unexpected(std::move(freq.error()));
  auto amp = config.getFloat("amp");
  if (!amp)
    return std::unexpected(std::move(amp.error()));
  auto bias = config.getFloat("bias", 0.0f);
  if (!bias)
    return std::unexpected(std::move(bias.error()));
  auto alpha = config.getFloat("alpha", 2.0f);
  if (!alpha)
    return std::unexpected(std::move(alpha.error()));
  auto beta = config.getFloat("beta", 2.0f);
  if (!beta)
    return std::unexpected(std::move(beta.error()));
  PerlinF function(*type, *octaves, *freq, *amp, *bias, *alpha, *beta, random.next_u64());
  auto xInfluence = config.getFloat("xInfluence", 1.0f);
  if (!xInfluence)
    return std::unexpected(std::move(xInfluence.error()));
  auto yInfluence = config.getFloat("yInfluence", 1.0f);
  if (!yInfluence)
    return std::unexpected(std::move(yInfluence.error()));
  return std::make_shared<PerlinSelector>(ConstructionKey{}, config, parameters, std::move(function), *xInfluence, *yInfluence);
}

std::expected<float, StarException> PerlinSelector::get(int x, int y) const {
  return function.get(x * xInfluence, y * yInfluence);
}

}

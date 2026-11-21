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
  auto function_name = config.get_string("function");
  if (!function_name)
    return std::unexpected(std::move(function_name.error()));
  auto type = enum_value<perlin_type_names>(StringView(*function_name));
  if (!type)
    return std::unexpected(std::move(type.error()));
  auto octaves = config.get_double("octaves");
  if (!octaves)
    return std::unexpected(std::move(octaves.error()));
  auto freq = config.get_double("freq");
  if (!freq)
    return std::unexpected(std::move(freq.error()));
  auto amp = config.get_double("amp");
  if (!amp)
    return std::unexpected(std::move(amp.error()));
  auto bias = config.get_double("bias", 0.0);
  if (!bias)
    return std::unexpected(std::move(bias.error()));
  auto alpha = config.get_double("alpha", 2.0);
  if (!alpha)
    return std::unexpected(std::move(alpha.error()));
  auto beta = config.get_double("beta", 2.0);
  if (!beta)
    return std::unexpected(std::move(beta.error()));
  auto function = PerlinF::create(*type, *octaves, *freq, *amp, *bias, *alpha, *beta, random.next_u64());
  if (!function)
    return std::unexpected(std::move(function.error()));
  auto xInfluence = config.get_float("xInfluence", 1.0f);
  if (!xInfluence)
    return std::unexpected(std::move(xInfluence.error()));
  auto yInfluence = config.get_float("yInfluence", 1.0f);
  if (!yInfluence)
    return std::unexpected(std::move(yInfluence.error()));
  return std::make_shared<PerlinSelector>(ConstructionKey{}, config, parameters, std::move(*function), *xInfluence, *yInfluence);
}

std::expected<float, StarException> PerlinSelector::get(int x, int y) const {
  return function.get(x * xInfluence, y * yInfluence);
}

}

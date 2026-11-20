module;
import std;
import star.format;
import star.json;

import star.bi_map;
import star.random;
import star.perlin;

export module star.terrain_ridge_blocks;
import star.terrain_database;

export namespace Star {

struct RidgeBlocksSelector : TerrainSelector {
  static std::string_view const Name;

  [[nodiscard]] static std::expected<std::shared_ptr<RidgeBlocksSelector>, StarException> create(Json const& config, TerrainSelectorParameters const& parameters);
private:
  struct ConstructionKey { explicit ConstructionKey() = default; };
public:
  RidgeBlocksSelector(ConstructionKey, Json const& config, TerrainSelectorParameters const& parameters) : TerrainSelector(Name, config, parameters) {}

  [[nodiscard]] std::expected<float, StarException> get(int x, int y) const override;

  float commonality;

  float amplitude;
  float frequency;
  float bias;

  float noiseAmplitude;
  float noiseFrequency;

  PerlinF ridgePerlin1;
  PerlinF ridgePerlin2;
  PerlinF noisePerlin;
};

}

namespace Star {

std::string_view const RidgeBlocksSelector::Name = "ridgeblocks";

std::expected<std::shared_ptr<RidgeBlocksSelector>, StarException> RidgeBlocksSelector::create(Json const& config, TerrainSelectorParameters const& parameters) {
  auto result = std::make_shared<RidgeBlocksSelector>(ConstructionKey{}, config, parameters);
  result->commonality = parameters.commonality;
  auto amplitude = config.get_float("amplitude");
  if (!amplitude)
    return std::unexpected(std::move(amplitude.error()));
  result->amplitude = *amplitude;
  auto frequency = config.get_float("frequency");
  if (!frequency)
    return std::unexpected(std::move(frequency.error()));
  result->frequency = *frequency;
  auto bias = config.get_float("bias");
  if (!bias)
    return std::unexpected(std::move(bias.error()));
  result->bias = *bias;
  auto noiseAmplitude = config.get_float("noiseAmplitude");
  if (!noiseAmplitude)
    return std::unexpected(std::move(noiseAmplitude.error()));
  result->noiseAmplitude = *noiseAmplitude;
  auto noiseFrequency = config.get_float("noiseFrequency");
  if (!noiseFrequency)
    return std::unexpected(std::move(noiseFrequency.error()));
  result->noiseFrequency = *noiseFrequency;
  RandomSource random(parameters.seed);
  result->ridgePerlin1 = PerlinF(PerlinType::RidgedMulti, 2, *frequency, *amplitude, 0, 2.0f, 2.0f, random.next_u64());
  result->ridgePerlin2 = PerlinF(PerlinType::RidgedMulti, 2, *frequency, *amplitude, 0, 2.0f, 2.0f, random.next_u64());
  result->noisePerlin = PerlinF(1, *noiseFrequency, *noiseAmplitude, 0, 1.0f, 2.0f, random.next_u64());
  return result;
}

std::expected<float, StarException> RidgeBlocksSelector::get(int x, int y) const {
  if (commonality <= 0.0f)
    return 0.0f;
  auto xNoise = noisePerlin.get(x, y);
  if (!xNoise)
    return std::unexpected(std::move(xNoise.error()));
  x += *xNoise;
  auto yNoise = noisePerlin.get(y, x);
  if (!yNoise)
    return std::unexpected(std::move(yNoise.error()));
  y += *yNoise;
  auto first = ridgePerlin1.get(x, y);
  if (!first)
    return std::unexpected(std::move(first.error()));
  auto second = ridgePerlin2.get(x, y);
  if (!second)
    return std::unexpected(std::move(second.error()));
  return (*first - *second) * commonality + bias;
}

}

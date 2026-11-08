module;
import std;
import star.format;
import star.string;
import star.string_view;
import star.json;
import star.json_extra;

import star.bi_map;
import star.random;
import star.perlin;
import star.vector;
import star.math_common;

export module star.terrain_transform;
import star.terrain_database;

export namespace Star {

struct RotateSelector : TerrainSelector {
  static std::string_view const Name;

  [[nodiscard]] static std::expected<std::shared_ptr<RotateSelector>, StarException> create(Json const& config, TerrainSelectorParameters const& parameters, TerrainDatabase const& database);
private:
  struct ConstructionKey { explicit ConstructionKey() = default; };
public:
  RotateSelector(ConstructionKey, Json const& config, TerrainSelectorParameters const& parameters) : TerrainSelector(Name, config, parameters) {}

  [[nodiscard]] std::expected<float, StarException> get(int x, int y) const override;

  float rotation;
  Vec2F rotationCenter;

  std::shared_ptr<TerrainSelector const> m_source;
};



struct DisplacementSelector : TerrainSelector {
  static std::string_view const Name;

  [[nodiscard]] static std::expected<std::shared_ptr<DisplacementSelector>, StarException> create(Json const& config, TerrainSelectorParameters const& parameters, TerrainDatabase const& database);
private:
  struct ConstructionKey { explicit ConstructionKey() = default; };
public:
  DisplacementSelector(ConstructionKey, Json const& config, TerrainSelectorParameters const& parameters) : TerrainSelector(Name, config, parameters) {}

  [[nodiscard]] std::expected<float, StarException> get(int x, int y) const override;

  PerlinF xDisplacementFunction;
  PerlinF yDisplacementFunction;

  float xXInfluence;
  float xYInfluence;
  float yXInfluence;
  float yYInfluence;

  bool yClamp;
  Vec2F yClampRange;
  float yClampSmoothing;

  float clampY(float v) const;

  std::shared_ptr<TerrainSelector const> m_source;
};

}

namespace Star {

std::string_view const RotateSelector::Name = "rotate";

std::expected<std::shared_ptr<RotateSelector>, StarException> RotateSelector::create(
    Json const& config, TerrainSelectorParameters const& parameters, TerrainDatabase const& database) {
  auto result = std::make_shared<RotateSelector>(ConstructionKey{}, config, parameters);
  auto rotation = config.getFloat("rotation");
  if (!rotation)
    return std::unexpected(std::move(rotation.error()));
  result->rotation = *rotation;
  result->rotationCenter = Vec2F(parameters.worldWidth / 2, 0);
  auto sourceConfig = config.get("source");
  if (!sourceConfig)
    return std::unexpected(std::move(sourceConfig.error()));
  auto sourceType = sourceConfig->getString("type");
  if (!sourceType)
    return std::unexpected(std::move(sourceType.error()));
  auto source = database.createSelectorType(*sourceType, *sourceConfig, parameters);
  if (!source)
    return std::unexpected(std::move(source.error()));
  result->m_source = std::move(*source);
  return result;
}

std::expected<float, StarException> RotateSelector::get(int x, int y) const {
  auto pos = (Vec2F(x, y) - rotationCenter).rotate(rotation) + rotationCenter;
  return m_source->get(pos[0], pos[1]);
}



std::string_view const DisplacementSelector::Name = "displacement";

std::expected<std::shared_ptr<DisplacementSelector>, StarException> DisplacementSelector::create(
    Json const& config, TerrainSelectorParameters const& parameters, TerrainDatabase const& database) {
  auto result = std::make_shared<DisplacementSelector>(ConstructionKey{}, config, parameters);
  RandomSource random(parameters.seed);
  auto readFunction = [&](char axis) -> std::expected<PerlinF, StarException> {
    auto key = [axis](std::string_view suffix) { return Star::format("{}{}", axis, suffix); };
    auto name = config.getString(key("Type"));
    if (!name)
      return std::unexpected(std::move(name.error()));
    auto type = enumValue<PerlinTypeNames>(StringView(*name));
    if (!type)
      return std::unexpected(std::move(type.error()));
    auto octaves = config.getFloat(key("Octaves"));
    if (!octaves)
      return std::unexpected(std::move(octaves.error()));
    auto freq = config.getFloat(key("Freq"));
    if (!freq)
      return std::unexpected(std::move(freq.error()));
    auto amp = config.getFloat(key("Amp"));
    if (!amp)
      return std::unexpected(std::move(amp.error()));
    auto bias = config.getFloat(key("Bias"), 0.0f);
    if (!bias)
      return std::unexpected(std::move(bias.error()));
    auto alpha = config.getFloat(key("Alpha"), 2.0f);
    if (!alpha)
      return std::unexpected(std::move(alpha.error()));
    auto beta = config.getFloat(key("Beta"), 2.0f);
    if (!beta)
      return std::unexpected(std::move(beta.error()));
    return PerlinF(*type, *octaves, *freq, *amp, *bias, *alpha, *beta, random.randu64());
  };
  auto xFunction = readFunction('x');
  if (!xFunction)
    return std::unexpected(std::move(xFunction.error()));
  result->xDisplacementFunction = std::move(*xFunction);
  auto yFunction = readFunction('y');
  if (!yFunction)
    return std::unexpected(std::move(yFunction.error()));
  result->yDisplacementFunction = std::move(*yFunction);
  auto xXInfluence = config.getFloat("xXInfluence", 1.0f);
  if (!xXInfluence)
    return std::unexpected(std::move(xXInfluence.error()));
  result->xXInfluence = *xXInfluence;
  auto xYInfluence = config.getFloat("xYInfluence", 1.0f);
  if (!xYInfluence)
    return std::unexpected(std::move(xYInfluence.error()));
  result->xYInfluence = *xYInfluence;
  auto yXInfluence = config.getFloat("yXInfluence", 1.0f);
  if (!yXInfluence)
    return std::unexpected(std::move(yXInfluence.error()));
  result->yXInfluence = *yXInfluence;
  auto yYInfluence = config.getFloat("yYInfluence", 1.0f);
  if (!yYInfluence)
    return std::unexpected(std::move(yYInfluence.error()));
  result->yYInfluence = *yYInfluence;
  auto yClamp = config.contains("yClamp");
  if (!yClamp)
    return std::unexpected(std::move(yClamp.error()));
  result->yClamp = *yClamp;
  if (*yClamp) {
    auto rangeConfig = config.get("yClamp");
    if (!rangeConfig)
      return std::unexpected(std::move(rangeConfig.error()));
    auto range = jsonToVec2F(*rangeConfig);
    if (!range)
      return std::unexpected(std::move(range.error()));
    result->yClampRange = *range;
    auto smoothing = config.getFloat("yClampSmoothing", 0);
    if (!smoothing)
      return std::unexpected(std::move(smoothing.error()));
    result->yClampSmoothing = *smoothing;
  }
  auto sourceConfig = config.get("source");
  if (!sourceConfig)
    return std::unexpected(std::move(sourceConfig.error()));
  auto sourceType = sourceConfig->getString("type");
  if (!sourceType)
    return std::unexpected(std::move(sourceType.error()));
  auto source = database.createSelectorType(*sourceType, *sourceConfig, parameters);
  if (!source)
    return std::unexpected(std::move(source.error()));
  result->m_source = std::move(*source);
  return result;
}

std::expected<float, StarException> DisplacementSelector::get(int x, int y) const {
  auto xDisplacement = xDisplacementFunction.get(x * xXInfluence, y * xYInfluence);
  if (!xDisplacement)
    return std::unexpected(std::move(xDisplacement.error()));
  auto yDisplacement = yDisplacementFunction.get(x * yXInfluence, y * yYInfluence);
  if (!yDisplacement)
    return std::unexpected(std::move(yDisplacement.error()));
  auto x_ = x + *xDisplacement;
  auto y_ = y + clampY(*yDisplacement);
  return m_source->get(x_, y_);
}

float DisplacementSelector::clampY(float v) const {
  if (!yClamp)
    return v;
  if (yClampSmoothing == 0)
    return clamp(v, yClampRange[0], yClampRange[1]);

  return 0.2f * (clamp(v - yClampSmoothing, yClampRange[0], yClampRange[1])
      + clamp(v - 0.5f * yClampSmoothing, yClampRange[0], yClampRange[1])
      + clamp(v, yClampRange[0], yClampRange[1])
      + clamp(v + 0.5f * yClampSmoothing, yClampRange[0], yClampRange[1])
      + clamp(v + yClampSmoothing, yClampRange[0], yClampRange[1]));
}

}

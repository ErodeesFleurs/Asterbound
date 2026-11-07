import star.format;
import std;
import star.json;
import star.json_extra;
import star.color;
import star.bi_map;
import star.sky_types;
import star.either;
import star.vector;
import star.celestial_coordinate;
import star.sky_parameters;
import star.sky_render_data;
import star.data_stream_extra;
import star.list;
import star.static_random;
import star.random_point;
import star.string;
import star.data_stream;
import star.image_processing;
import star.hash;
import star.string_view;
import star.thread;
import star.directives;
import star.asset_path;
import star.drawable;

import star.matrix3;
import star.math_common;
namespace Star {

StringList const& SkyRenderData::starTypes() const {
  if (type == SkyType::Warp)
    return hyperStarList;
  else
    return starList;
}

std::expected<List<SkyOrbiter>, StarException> SkyRenderData::backOrbiters(Vec2F const& viewSize) const {
  if (!settings)
    return List<SkyOrbiter>{};
  auto planetScaleResult = settings.queryFloat("satellite.planetScale");
  if (!planetScaleResult) return std::unexpected(std::move(planetScaleResult.error()));
  float planetScale = *planetScaleResult;
  auto moonScaleResult = settings.queryFloat("satellite.moonScale");
  if (!moonScaleResult) return std::unexpected(std::move(moonScaleResult.error()));
  float moonScale = *moonScaleResult;

  List<std::tuple<List<std::pair<String, float>>, Vec2F, float>> orbitingCelestialObjects;

  // Gather up all the CelestialParameters and scales for all the celestial
  // objects to draw in the sky, we should draw the parent planet if we are a
  // satellite, as well as all the other satellites.
  if (skyParameters.nearbyPlanet)
    orbitingCelestialObjects.append(tuple_cat(*skyParameters.nearbyPlanet, std::tie(planetScale)));

  for (auto moon : skyParameters.nearbyMoons)
    orbitingCelestialObjects.append(tuple_cat(moon, std::tie(moonScale)));

  auto area = settings.query("satellite.area");
  if (!area) return std::unexpected(std::move(area.error()));
  auto satelliteAreaResult = jsonToVec2F(*area);
  if (!satelliteAreaResult) return std::unexpected(std::move(satelliteAreaResult.error()));
  Vec2F satelliteArea = *satelliteAreaResult;
  auto planetCenter = Vec2F(viewSize[0] / 2, 0) - worldOffset;
  auto rotMatrix = Mat3F::rotation(worldRotation, planetCenter);

  List<SkyOrbiter> orbiters;

  for (auto const& object : orbitingCelestialObjects) {
    auto const& layers = std::get<0>(object);
    Vec2F pos = std::get<1>(object);
    pos = pos.piecewiseMultiply(satelliteArea);
    pos -= worldOffset;
    pos = rotMatrix.transformVec2(pos);
    for (auto const& l : layers)
      orbiters.append(SkyOrbiter{SkyOrbiterType::Moon, std::get<2>(object) * l.second, 0.0f, l.first, pos});
  }

  return orbiters;
}

std::expected<SkyWorldHorizon, StarException> SkyRenderData::worldHorizon(Vec2F const& viewSize) const {
  if (!settings)
    return SkyWorldHorizon{};

  SkyWorldHorizon worldHorizon;

  if (type == SkyType::Orbital) {
    worldHorizon.center = Vec2F(viewSize[0] / 2, 0) - worldOffset;
    auto scale = settings.queryFloat("planetHorizon.scale");
    if (!scale) return std::unexpected(std::move(scale.error()));
    worldHorizon.scale = *scale;
    worldHorizon.rotation = worldRotation;
    worldHorizon.layers = skyParameters.horizonImages;
  }

  return worldHorizon;
}

std::expected<List<SkyOrbiter>, StarException> SkyRenderData::frontOrbiters(Vec2F const& viewSize) const {
  if (!settings)
    return List<SkyOrbiter>{};

  struct HorizonCloud {
    float startAngle;
    String image;
    float speed;
    float radius;
  };
  List<HorizonCloud> horizonClouds;

  if (skyParameters.horizonClouds) {
    auto count = settings.query("planetHorizon.cloudCount");
    if (!count) return std::unexpected(std::move(count.error()));
    auto countRange = jsonToVec2I(*count);
    if (!countRange) return std::unexpected(std::move(countRange.error()));
    Vec2I cloudCountRange = *countRange;
    auto radius = settings.query("planetHorizon.cloudRadius");
    if (!radius) return std::unexpected(std::move(radius.error()));
    auto radiusRange = jsonToVec2F(*radius);
    if (!radiusRange) return std::unexpected(std::move(radiusRange.error()));
    Vec2F cloudRadiusRange = *radiusRange;
    auto speed = settings.query("planetHorizon.cloudSpeed");
    if (!speed) return std::unexpected(std::move(speed.error()));
    auto speedRange = jsonToVec2F(*speed);
    if (!speedRange) return std::unexpected(std::move(speedRange.error()));
    Vec2F cloudSpeedRange = *speedRange;
    auto clouds = settings.query("planetHorizon.clouds");
    if (!clouds) return std::unexpected(std::move(clouds.error()));
    auto list = jsonToStringList(*clouds);
    if (!list) return std::unexpected(std::move(list.error()));
    StringList cloudList = std::move(*list);

    int numClouds = static_random_i32_range(cloudCountRange[0], cloudCountRange[1], "HorizonCloudCount");
    for (int i = 0; i < numClouds; ++i) {
      auto image = static_random_from(cloudList, i, "Cloud");
      if (!image) return std::unexpected(std::move(image.error()));
      auto angle = static_random_float_range(0, 2 * std::numbers::pi, i, "CloudStartAngle");
      auto speed = static_random_float_range(cloudSpeedRange[0], cloudSpeedRange[1], i, "CloudSpeed");
      auto radius = static_random_float_range(cloudRadiusRange[0], cloudRadiusRange[1], i, "CloudRadius");
      horizonClouds.append({angle, image->get(), speed, radius});
    }
  }

  List<SkyOrbiter> orbiters;
  if (type == SkyType::Atmospheric || type == SkyType::Atmosphereless) {
    String image;
    auto dynamicImage = settings.queryBool("sun.dynamicImage.enabled", false);
    if (!dynamicImage) return std::unexpected(std::move(dynamicImage.error()));
    auto defaultImage = settings.queryString("sun.image");
    if (!defaultImage) return std::unexpected(std::move(defaultImage.error()));
    if (*dynamicImage && !skyParameters.sunType.empty()) {
      auto dynamic = settings.queryString("sun.dynamicImage.images." + skyParameters.sunType, *defaultImage);
      if (!dynamic) return std::unexpected(std::move(dynamic.error()));
      image = std::move(*dynamic);
    } else {
      image = std::move(*defaultImage);
    }
    auto scale = settings.queryFloat("sun.scale", 1.0f);
    if (!scale) return std::unexpected(std::move(scale.error()));
    auto radius = settings.queryFloat("sun.radius");
    if (!radius) return std::unexpected(std::move(radius.error()));
    orbiters.append({SkyOrbiterType::Sun, *scale, 0.0f, image,
        Vec2F::withAngle(orbitAngle, *radius) + viewSize / 2});
  } else if (type == SkyType::Orbital) {
    auto yCenter = settings.queryFloat("planetHorizon.yCenter");
    if (!yCenter) return std::unexpected(std::move(yCenter.error()));
    auto planetCenter = Vec2F(viewSize[0] / 2, 0)
        - Vec2F::withAngle(worldRotation - std::numbers::pi / 2, *yCenter) - worldOffset;
    auto scaleResult = settings.queryFloat("planetHorizon.scale");
    if (!scaleResult) return std::unexpected(std::move(scaleResult.error()));
    float scale = *scaleResult;

    auto rotMatrix = Mat3F::rotation(worldRotation, planetCenter);

    if (skyParameters.horizonClouds) {
      for (auto const& horizonCloud : horizonClouds) {
        Vec2F position = Vec2F::withAngle(horizonCloud.startAngle + orbitAngle * horizonCloud.speed, horizonCloud.radius) + planetCenter;
        position = rotMatrix.transformVec2(position);
        orbiters.append({SkyOrbiterType::HorizonCloud, scale, worldRotation, horizonCloud.image, position});
      }
    }
  }

  return orbiters;
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, SkyRenderData& skyRenderData) {
  SkyRenderData value;
  auto first = ds >> value.settings >> value.skyParameters >> value.type >> value.dayLevel >> value.skyAlpha
      >> value.dayLength >> value.timeOfDay >> value.epochTime >> value.starOffset >> value.starRotation
      >> value.worldOffset >> value.worldRotation >> value.orbitAngle;
  if (!first) return std::unexpected(std::move(first.error()));
  auto frames = ds.readVlqS(value.starFrames);
  if (!frames) return std::unexpected(std::move(frames.error()));
  auto last = ds >> value.starList >> value.hyperStarList >> value.environmentLight >> value.mainSkyColor
      >> value.topRectColor >> value.bottomRectColor >> value.flashColor;
  if (!last) return std::unexpected(std::move(last.error()));
  skyRenderData = std::move(value);
  return std::ref(ds);
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, SkyRenderData const& skyRenderData) {
  auto first = ds << skyRenderData.settings << skyRenderData.skyParameters << skyRenderData.type
      << skyRenderData.dayLevel << skyRenderData.skyAlpha << skyRenderData.dayLength << skyRenderData.timeOfDay
      << skyRenderData.epochTime << skyRenderData.starOffset << skyRenderData.starRotation
      << skyRenderData.worldOffset << skyRenderData.worldRotation << skyRenderData.orbitAngle;
  if (!first) return std::unexpected(std::move(first.error()));
  auto frames = ds.writeVlqS(skyRenderData.starFrames);
  if (!frames) return std::unexpected(std::move(frames.error()));
  auto last = ds << skyRenderData.starList << skyRenderData.hyperStarList << skyRenderData.environmentLight
      << skyRenderData.mainSkyColor << skyRenderData.topRectColor << skyRenderData.bottomRectColor << skyRenderData.flashColor;
  if (!last) return std::unexpected(std::move(last.error()));
  return std::ref(ds);
}

}

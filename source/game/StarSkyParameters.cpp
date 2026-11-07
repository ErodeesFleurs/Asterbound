import star.format;
import std;
import star.string;
import star.json;
import star.json_extra;
import star.ordered_map;
import star.either;
import star.vector;
import star.celestial_coordinate;
import star.game_types;
import star.color;
import star.bi_map;
import star.sky_types;
import star.random;
import star.weighted_pool;
import star.list;
import star.image_processing;
import star.hash;
import star.data_stream;
import star.string_view;
import star.thread;
import star.directives;
import star.asset_path;
import star.animation;
import star.particle;
import star.weather_types;
import star.force_regions;
import star.world_parameters;
import star.celestial_parameters;
import star.celestial_types;
import star.sky_types;
import star.celestial_coordinate;
import star.sky_parameters;
import star.celestial_coordinate;
import star.game_types;
import star.sky_types;
import star.animation;
import star.particle;
import star.weather_types;
import star.force_regions;
import star.world_parameters;
import star.celestial_parameters;
import star.celestial_graphics;
import star.casting;
import star.data_stream_extra;


import star.celestial_database;
import star.root;

import star.static_random;
namespace Star {

SkyParameters::SkyParameters() : seed(), skyColoring(makeRight(Color::Black)), settings(JsonObject()) {}

std::expected<SkyParameters, StarException> SkyParameters::create(Root& root, CelestialCoordinate const& coordinate, std::shared_ptr<CelestialDatabase> const& celestialDatabase) {
  SkyParameters sky;
  if (!coordinate || coordinate.isSystem())
    return sky;
  auto parameters = celestialDatabase->parameters(coordinate);
  if (!parameters) return std::unexpected(std::move(parameters.error()));
  auto params = *parameters;
  if (!params) return sky;
  auto system = coordinate.system();
  if (!system) return std::unexpected(std::move(system.error()));
  auto systemParameters = celestialDatabase->parameters(*system);
  if (!systemParameters) return std::unexpected(std::move(systemParameters.error()));
  auto systemParams = *systemParameters;
  sky.seed = staticRandomU64(params->seed(), "SkySeed");
  auto selfCoordinate = params->coordinate();
  if (selfCoordinate.isSatelliteBody()) {
    auto parent = selfCoordinate.parent();
    if (!parent) return std::unexpected(std::move(parent.error()));
    auto planetResult = celestialDatabase->parameters(*parent);
    if (!planetResult) return std::unexpected(std::move(planetResult.error()));
    if (auto planet = *planetResult) {
      Vec2F pos;
      pos[0] = staticRandomFloat(params->seed(), planet->seed(), "x");
      pos[1] = staticRandomFloat(params->seed(), planet->seed(), "y");
      auto layers = CelestialGraphics::drawWorld(root, *planet, {});
      if (!layers) return std::unexpected(std::move(layers.error()));
      sky.nearbyPlanet = {{std::move(*layers), pos}};
    }
  }
  auto planetCoordinate = selfCoordinate.planet();
  if (!planetCoordinate) return std::unexpected(std::move(planetCoordinate.error()));
  auto children = celestialDatabase->children(*planetCoordinate);
  if (!children) return std::unexpected(std::move(children.error()));
  for (auto satelliteCoordinate : *children) {
    if (satelliteCoordinate != selfCoordinate) {
      auto satelliteResult = celestialDatabase->parameters(satelliteCoordinate);
      if (!satelliteResult) return std::unexpected(std::move(satelliteResult.error()));
      if (auto satellite = *satelliteResult) {
        Vec2F pos;
        pos[0] = staticRandomFloat(params->seed(), satellite->seed(), "x");
        pos[1] = staticRandomFloat(params->seed(), satellite->seed(), "y");
        auto parent = satelliteCoordinate.parent();
        if (!parent) return std::unexpected(std::move(parent.error()));
        auto parentParameters = celestialDatabase->parameters(*parent);
        if (!parentParameters) return std::unexpected(std::move(parentParameters.error()));
        auto layers = CelestialGraphics::drawWorld(root, *satellite, *parentParameters);
        if (!layers) return std::unexpected(std::move(layers.error()));
        sky.nearbyMoons.append({std::move(*layers), pos});
      }
    }
  }
  auto horizon = CelestialGraphics::worldHorizonImages(root, *params);
  if (!horizon) return std::unexpected(std::move(horizon.error()));
  sky.horizonImages = std::move(*horizon);
  sky.readVisitableParameters(params->visitableParameters());
  if (!systemParams)
    return std::unexpected(SkyException("Missing celestial system parameters"));
  auto type = systemParams->getParameter("typeName");
  if (!type) return std::unexpected(std::move(type.error()));
  auto sunType = type->toString();
  if (!sunType) return std::unexpected(std::move(sunType.error()));
  sky.sunType = std::move(*sunType);
  return sky;
}

SkyParameters::SkyParameters(SkyParameters const& oldSkyParameters, std::shared_ptr<VisitableWorldParameters const> newVisitableParameters) : SkyParameters() {
  *this = oldSkyParameters;
  readVisitableParameters(std::move(newVisitableParameters));
}

std::expected<SkyParameters, StarException> SkyParameters::create(Json const& config) {
  SkyParameters sky;
  if (config.isNull()) return sky;
  auto seed = config.getUInt("seed");
  if (!seed) return std::unexpected(std::move(seed.error()));
  sky.seed = *seed;
  auto dayLength = config.optFloat("dayLength");
  if (!dayLength) return std::unexpected(std::move(dayLength.error()));
  sky.dayLength = *dayLength;
  auto extractLayerData = [](Json const& v, std::uint64_t selfSeed) -> std::expected<std::pair<List<std::pair<String, float>>, Vec2F>, StarException> {
    Vec2F pos;
    auto position = v.opt("pos");
    if (!position) return std::unexpected(std::move(position.error()));
    if (*position) {
      auto parsed = jsonToVec2F(**position);
      if (!parsed) return std::unexpected(std::move(parsed.error()));
      pos = *parsed;
    } else {
      auto seed = v.optUInt("seed");
      if (!seed) return std::unexpected(std::move(seed.error()));
      if (*seed) {
        pos[0] = staticRandomFloat(selfSeed, **seed, "x");
        pos[1] = staticRandomFloat(selfSeed, **seed, "y");
      }
    }
    auto layersJson = v.getArray("layers");
    if (!layersJson) return std::unexpected(std::move(layersJson.error()));
    List<std::pair<String, float>> layers;
    for (auto const& layer : *layersJson) {
      auto image = layer.getString("image");
      if (!image) return std::unexpected(std::move(image.error()));
      auto scale = layer.getFloat("scale");
      if (!scale) return std::unexpected(std::move(scale.error()));
      layers.append({std::move(*image), *scale});
    }
    return std::pair{std::move(layers), pos};
  };
  auto planet = config.opt("planet");
  if (!planet) return std::unexpected(std::move(planet.error()));
  if (*planet && **planet) {
    auto layers = extractLayerData(**planet, sky.seed);
    if (!layers) return std::unexpected(std::move(layers.error()));
    sky.nearbyPlanet = std::move(*layers);
  }
  auto satellites = config.optArray("satellites");
  if (!satellites) return std::unexpected(std::move(satellites.error()));
  if (*satellites) {
    for (auto const& satellite : **satellites) {
      auto layers = extractLayerData(satellite, sky.seed);
      if (!layers) return std::unexpected(std::move(layers.error()));
      sky.nearbyMoons.append(std::move(*layers));
    }
  }
  auto horizons = config.optArray("horizonImages");
  if (!horizons) return std::unexpected(std::move(horizons.error()));
  if (*horizons) {
    for (auto const& horizon : **horizons) {
      auto left = horizon.getString("left");
      if (!left) return std::unexpected(std::move(left.error()));
      auto right = horizon.getString("right");
      if (!right) return std::unexpected(std::move(right.error()));
      sky.horizonImages.append({std::move(*left), std::move(*right)});
    }
  }
  auto clouds = config.getBool("horizonClouds", true);
  if (!clouds) return std::unexpected(std::move(clouds.error()));
  sky.horizonClouds = *clouds;
  auto typeName = config.getString("skyType", "barren");
  if (!typeName) return std::unexpected(std::move(typeName.error()));
  if (!SkyTypeNames) return std::unexpected(SkyTypeNames.error());
  auto type = SkyTypeNames->get_left(*typeName);
  if (!type) return std::unexpected(std::move(type.error()));
  sky.skyType = type->get();
  auto colors = config.opt("skyColoring");
  if (!colors) return std::unexpected(std::move(colors.error()));
  if (*colors) {
    auto coloring = SkyColoring::create(**colors);
    if (!coloring) return std::unexpected(std::move(coloring.error()));
    sky.skyColoring.setLeft(std::move(*coloring));
  } else {
    auto light = config.opt("ambientLightLevel");
    if (!light) return std::unexpected(std::move(light.error()));
    if (*light) {
      auto color = jsonToColor(**light);
      if (!color) return std::unexpected(std::move(color.error()));
      sky.skyColoring.setRight(*color);
    } else {
      sky.skyColoring.setRight(Color::Black);
    }
  }
  auto space = config.optFloat("spaceLevel");
  if (!space) return std::unexpected(std::move(space.error()));
  sky.spaceLevel = *space;
  auto surface = config.optFloat("surfaceLevel");
  if (!surface) return std::unexpected(std::move(surface.error()));
  sky.surfaceLevel = *surface;
  auto sun = config.getString("sunType", "");
  if (!sun) return std::unexpected(std::move(sun.error()));
  sky.sunType = std::move(*sun);
  auto settings = config.get("settings", JsonObject());
  if (!settings) return std::unexpected(std::move(settings.error()));
  sky.settings = std::move(*settings);
  return sky;
}

std::expected<Json, StarException> SkyParameters::toJson() const {
  if (!SkyTypeNames) return std::unexpected(SkyTypeNames.error());
  auto typeName = SkyTypeNames->get_right(skyType);
  if (!typeName) return std::unexpected(std::move(typeName.error()));
  auto layerJson = [](std::pair<List<std::pair<String, float>>, Vec2F> const& planet) -> Json {
    JsonArray layers;
    for (auto const& layer : planet.first)
      layers.append(JsonObject{{"image", layer.first}, {"scale", layer.second}});
    return JsonObject{{"layers", std::move(layers)}, {"pos", jsonFromVec2F(planet.second)}};
  };
  Json planet;
  if (nearbyPlanet) planet = layerJson(*nearbyPlanet);
  JsonArray satellites;
  for (auto const& moon : nearbyMoons) satellites.append(layerJson(moon));
  JsonArray horizons;
  for (auto const& horizon : horizonImages)
    horizons.append(JsonObject{{"left", horizon.first}, {"right", horizon.second}});
  Json coloring;
  if (auto value = skyColoring.maybeLeftRef()) coloring = value->get().toJson();
  Json light;
  if (auto value = skyColoring.maybeRightRef()) light = jsonFromColor(value->get());
  return JsonObject{
      {"seed", seed}, {"dayLength", jsonFromOptional<float>(dayLength)},
      {"planet", std::move(planet)}, {"satellites", std::move(satellites)},
      {"horizonImages", std::move(horizons)}, {"horizonClouds", horizonClouds},
      {"skyType", typeName->get()}, {"skyColoring", std::move(coloring)},
      {"ambientLightLevel", std::move(light)}, {"spaceLevel", jsonFromOptional<float>(spaceLevel)},
      {"surfaceLevel", jsonFromOptional<float>(surfaceLevel)}, {"sunType", sunType}, {"settings", settings}};
}

std::expected<void, StarException> SkyParameters::read(DataStream& ds) {
  SkyParameters value;
  auto fields = ds >> value.seed >> value.dayLength >> value.nearbyPlanet >> value.nearbyMoons
      >> value.horizonImages >> value.horizonClouds >> value.skyType >> value.skyColoring
      >> value.spaceLevel >> value.surfaceLevel >> value.sunType;
  if (!fields) return std::unexpected(std::move(fields.error()));
  if (ds.streamCompatibilityVersion() >= 3) {
    auto settings = ds >> value.settings;
    if (!settings) return std::unexpected(std::move(settings.error()));
  }
  *this = std::move(value);
  return {};
}

std::expected<void, StarException> SkyParameters::write(DataStream& ds) const {
  auto fields = ds << seed << dayLength << nearbyPlanet << nearbyMoons << horizonImages << horizonClouds
      << skyType << skyColoring << spaceLevel << surfaceLevel << sunType;
  if (!fields) return std::unexpected(std::move(fields.error()));
  if (ds.streamCompatibilityVersion() >= 3) {
    auto settingsResult = ds << settings;
    if (!settingsResult) return std::unexpected(std::move(settingsResult.error()));
  }
  return {};
}

void SkyParameters::readVisitableParameters(std::shared_ptr<VisitableWorldParameters const> visitableParameters) {
  if (auto terrestrialParameters = as<TerrestrialWorldParameters>(visitableParameters)) {
    dayLength = terrestrialParameters->dayLength;
    if (terrestrialParameters->airless) {
      skyType = SkyType::Atmosphereless;
      horizonClouds = false;
    } else {
      skyType = SkyType::Atmospheric;
      horizonClouds = true;
    }
    skyColoring.setLeft(terrestrialParameters->skyColoring);
    spaceLevel = terrestrialParameters->spaceLayer.layerMinHeight;
    surfaceLevel = terrestrialParameters->atmosphereLayer.layerMinHeight;
  } else {
    skyType = SkyType::Barren;
    horizonClouds = false;

    if (auto asteroidsParameters = as<AsteroidsWorldParameters>(visitableParameters)) {
      skyColoring.setRight(asteroidsParameters->ambientLightLevel);
    } else if (auto floatingDungeonParameters = as<FloatingDungeonWorldParameters>(visitableParameters)) {
      skyColoring.setRight(floatingDungeonParameters->ambientLightLevel);
    } else {
      skyColoring.setRight(Color::Black);
    }
  }
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, SkyParameters& sky) {
  auto result = sky.read(ds);
  if (!result) return std::unexpected(std::move(result.error()));
  return std::ref(ds);
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, SkyParameters const& sky) {
  auto result = sky.write(ds);
  if (!result) return std::unexpected(std::move(result.error()));
  return std::ref(ds);
}

}

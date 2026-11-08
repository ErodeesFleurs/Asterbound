import star.json_extra;
import star.json;
import star.ordered_map;
import star.either;
import star.game_types;
import star.liquid_types;
import star.color;
import star.bi_map;
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
import star.casting;
import star.lexical_cast;
import star.format;
import star.io_device;
import star.ref_ptr;
import star.logging;
import star.listener;
import std;
import star.optional;
import star.string;
import star.version;

import star.celestial_coordinate;
import star.sky_types;
import star.animation;
import star.particle;
import star.weather_types;
import star.force_regions;
import star.world_parameters;
import star.celestial_parameters;
import star.celestial_types;
import star.celestial_graphics;
import star.tile_damage;
import star.plant_database;
import star.parallax;
import star.asset_source;
import star.assets;
import star.root_base;
import star.configuration;
import star.root;

import star.terrain_database;
import star.liquids_database;
import star.celestial_database;
import star.biome_database;


import star.static_random;
namespace Star {

std::expected<List<std::pair<String, float>>, StarException> CelestialGraphics::drawSystemPlanetaryObject(CelestialParameters const& parameters) {
  auto image = parameters.getParameter("smallImage").and_then([](Json const& value) { return value.toString(); });
  if (!image)
    return std::unexpected(std::move(image.error()));
  auto scale = parameters.getParameter("smallImageScale").and_then([](Json const& value) { return value.toFloat(); });
  if (!scale)
    return std::unexpected(std::move(scale.error()));
  return List<std::pair<String, float>>{{std::move(*image), *scale}};
}

std::expected<List<std::pair<String, float>>, StarException> CelestialGraphics::drawSystemCentralBody(CelestialParameters const& parameters) {
  auto image = parameters.getParameter("image").and_then([](Json const& value) { return value.toString(); });
  if (!image)
    return std::unexpected(std::move(image.error()));
  auto scale = parameters.getParameter("imageScale").and_then([](Json const& value) { return value.toFloat(); });
  if (!scale)
    return std::unexpected(std::move(scale.error()));
  return List<std::pair<String, float>>{{std::move(*image), *scale}};
}

std::expected<List<std::pair<String, float>>, StarException> CelestialGraphics::drawWorld(
    Root& root, CelestialParameters const& celestialParameters, std::optional<CelestialParameters> const& overrideShadowParameters) {
  auto assets = root.assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  auto liquidsDatabase = root.liquidsDatabase();
  if (!liquidsDatabase)
    return std::unexpected(std::move(liquidsDatabase.error()));
  auto const& shadowParameters = overrideShadowParameters ? *overrideShadowParameters : celestialParameters;
  auto type = celestialParameters.getParameter("worldType").and_then([](Json const& value) { return value.toString(); });
  if (!type)
    return std::unexpected(std::move(type.error()));
  List<std::pair<String, float>> layers;

  if (*type == "Terrestrial") {
    auto terrestrialParameters = as<TerrestrialWorldParameters>(celestialParameters.visitableParameters());
    if (!terrestrialParameters)
      return layers;
    auto graphics = (*assets)->json("/celestial.config:terrestrialGraphics");
    if (!graphics)
      return std::unexpected(std::move(graphics.error()));
    auto defaults = graphics->get("default");
    if (!defaults)
      return std::unexpected(std::move(defaults.error()));
    auto overrides = graphics->get(terrestrialParameters->typeName, JsonObject());
    if (!overrides)
      return std::unexpected(std::move(overrides.error()));
    auto gfxConfig = jsonMerge(*defaults, *overrides);
    auto liquidImages = gfxConfig.getString("liquidImages", "");
    if (!liquidImages)
      return std::unexpected(std::move(liquidImages.error()));
    auto baseImages = gfxConfig.getString("baseImages", "");
    if (!baseImages)
      return std::unexpected(std::move(baseImages.error()));
    auto shadowImages = gfxConfig.getString("shadowImages", "");
    if (!shadowImages)
      return std::unexpected(std::move(shadowImages.error()));
    auto baseCount = gfxConfig.getInt("baseCount", 0);
    if (!baseCount)
      return std::unexpected(std::move(baseCount.error()));
    auto dynamicsImages = gfxConfig.getString("dynamicsImages", "");
    if (!dynamicsImages)
      return std::unexpected(std::move(dynamicsImages.error()));
    auto imageScale = celestialParameters.getParameter("imageScale", 1.0f).and_then([](Json const& value) { return value.toFloat(); });
    if (!imageScale)
      return std::unexpected(std::move(imageScale.error()));

    if (terrestrialParameters->primarySurfaceLiquid != EmptyLiquidId && !liquidImages->empty()) {
      auto liquidName = (*liquidsDatabase)->liquidName(terrestrialParameters->primarySurfaceLiquid);
      if (!liquidName)
        return std::unexpected(std::move(liquidName.error()));
      auto image = liquidImages->replace("<liquid>", *liquidName);
      if (!image)
        return std::unexpected(std::move(image.error()));
      layers.push_back({std::move(*image), *imageScale});
    } else if (*baseCount > 0) {
      auto image = baseImages->replace("<biome>", terrestrialParameters->primaryBiome)
          .and_then([&](String const& value) { return value.replace("<num>", toString(*baseCount)); });
      if (!image)
        return std::unexpected(std::move(image.error()));
      layers.push_back({strf("{}?hueshift={}", *image, terrestrialParameters->hueShift), *imageScale});
    }
    for (int i = 0; i < *baseCount; ++i) {
      auto baseImage = baseImages->replace("<num>", toString(*baseCount - i));
      if (!baseImage)
        return std::unexpected(std::move(baseImage.error()));
      String hueShiftString, dynamicMaskString;
      if (!dynamicsImages->empty()) {
        auto dynamic = gfxConfig.getArray("dynamicsRange")
            .and_then([&](JsonArray const& value) { return celestialParameters.randomizeParameterRange(value, i); })
            .and_then([](Json const& value) { return value.toInt(); });
        if (!dynamic)
          return std::unexpected(std::move(dynamic.error()));
        auto mask = dynamicsImages->replace("<num>", toString(*dynamic));
        if (!mask)
          return std::unexpected(std::move(mask.error()));
        dynamicMaskString = "?addmask=" + *mask;
      }
      if (terrestrialParameters->hueShift != 0)
        hueShiftString = strf("?hueshift={}", terrestrialParameters->hueShift);
      layers.push_back({*baseImage + hueShiftString + dynamicMaskString, *imageScale});
    }
    if (!shadowImages->empty()) {
      auto number = gfxConfig.getArray("shadowNumber")
          .and_then([&](JsonArray const& value) { return shadowParameters.randomizeParameterRange(value); })
          .and_then([](Json const& value) { return value.toInt(); });
      if (!number)
        return std::unexpected(std::move(number.error()));
      auto shadow = shadowImages->replace("<num>", toString(*number));
      if (!shadow)
        return std::unexpected(std::move(shadow.error()));
      layers.push_back({std::move(*shadow), *imageScale});
    }
  } else if (*type == "Asteroids") {
    auto maskImages = celestialParameters.getParameter("maskImages").and_then([](Json const& value) { return value.toString(); });
    if (!maskImages)
      return std::unexpected(std::move(maskImages.error()));
    auto maskCount = celestialParameters.getParameter("masks").and_then([](Json const& value) { return value.toInt(); });
    if (!maskCount)
      return std::unexpected(std::move(maskCount.error()));
    auto dynamicsImages = celestialParameters.getParameter("dynamicsImages").and_then([](Json const& value) { return value.toString(); });
    if (!dynamicsImages)
      return std::unexpected(std::move(dynamicsImages.error()));
    auto imageScale = celestialParameters.getParameter("imageScale", 1.0f).and_then([](Json const& value) { return value.toFloat(); });
    if (!imageScale)
      return std::unexpected(std::move(imageScale.error()));
    for (int i = 0; i < *maskCount; ++i) {
      auto base = maskImages->replace("<num>", toString(*maskCount - i));
      if (!base)
        return std::unexpected(std::move(base.error()));
      auto number = celestialParameters.randomizeParameterRange("dynamicsRange", i).and_then([](Json const& value) { return value.toInt(); });
      if (!number)
        return std::unexpected(std::move(number.error()));
      auto mask = dynamicsImages->replace("<num>", toString(*number));
      if (!mask)
        return std::unexpected(std::move(mask.error()));
      layers.push_back({strf("{}?addmask={}", *base, *mask), *imageScale});
    }
  } else if (*type == "FloatingDungeon") {
    auto image = celestialParameters.getParameter("image").and_then([](Json const& value) { return value.toString(); });
    if (!image)
      return std::unexpected(std::move(image.error()));
    auto imageScale = celestialParameters.getParameter("imageScale", 1.0f).and_then([](Json const& value) { return value.toFloat(); });
    if (!imageScale)
      return std::unexpected(std::move(imageScale.error()));
    layers.push_back({std::move(*image), *imageScale});
    auto dynamicsImages = celestialParameters.getParameter("dynamicsImages").and_then([](Json const& value) { return value.toString(); });
    if (!dynamicsImages)
      return std::unexpected(std::move(dynamicsImages.error()));
    if (!dynamicsImages->empty()) {
      auto number = celestialParameters.randomizeParameterRange("dynamicsRange").and_then([](Json const& value) { return value.toInt(); });
      if (!number)
        return std::unexpected(std::move(number.error()));
      auto dynamic = dynamicsImages->replace("<num>", toString(*number));
      if (!dynamic)
        return std::unexpected(std::move(dynamic.error()));
      layers.push_back({std::move(*dynamic), *imageScale});
    }
  } else if (*type == "GasGiant") {
    auto gfxConfig = (*assets)->json("/celestial.config:gasGiantGraphics");
    if (!gfxConfig)
      return std::unexpected(std::move(gfxConfig.error()));
    auto baseImage = gfxConfig->getString("baseImage", "");
    if (!baseImage)
      return std::unexpected(std::move(baseImage.error()));
    auto shadowImages = gfxConfig->getString("shadowImages", "");
    if (!shadowImages)
      return std::unexpected(std::move(shadowImages.error()));
    auto dynamicsImages = gfxConfig->getString("dynamicsImages", "");
    if (!dynamicsImages)
      return std::unexpected(std::move(dynamicsImages.error()));
    auto overlayImages = gfxConfig->getString("overlayImages", "");
    if (!overlayImages)
      return std::unexpected(std::move(overlayImages.error()));
    auto overlayCount = gfxConfig->getInt("overlayCount", 0);
    if (!overlayCount)
      return std::unexpected(std::move(overlayCount.error()));
    auto imageScale = celestialParameters.getParameter("imageScale", 1.0f).and_then([](Json const& value) { return value.toFloat(); });
    if (!imageScale)
      return std::unexpected(std::move(imageScale.error()));
    auto hueShift = gfxConfig->getArray("primaryHueShiftRange")
        .and_then([&](JsonArray const& value) { return celestialParameters.randomizeParameterRange(value); })
        .and_then([](Json const& value) { return value.toFloat(); });
    if (!hueShift)
      return std::unexpected(std::move(hueShift.error()));
    if (!baseImage->empty())
      layers.push_back({strf("{}?hueshift={}", *baseImage, *hueShift), *imageScale});
    if (!overlayImages->empty()) {
      for (int i = 0; i < *overlayCount; ++i) {
        auto offset = gfxConfig->getArray("hueShiftOffsetRange")
            .and_then([&](JsonArray const& value) { return celestialParameters.randomizeParameterRange(value); })
            .and_then([](Json const& value) { return value.toFloat(); });
        if (!offset)
          return std::unexpected(std::move(offset.error()));
        *hueShift += *offset;
        auto number = gfxConfig->getArray("dynamicsRange")
            .and_then([&](JsonArray const& value) { return celestialParameters.randomizeParameterRange(value, i); })
            .and_then([](Json const& value) { return value.toInt(); });
        if (!number)
          return std::unexpected(std::move(number.error()));
        auto mask = dynamicsImages->replace("<num>", toString(*number));
        if (!mask)
          return std::unexpected(std::move(mask.error()));
        auto overlay = overlayImages->replace("<num>", toString(i));
        if (!overlay)
          return std::unexpected(std::move(overlay.error()));
        layers.push_back({strf("{}?hueshift={}?addmask={}", *overlay, *hueShift, *mask), *imageScale});
      }
    }
    if (!shadowImages->empty()) {
      auto number = gfxConfig->getArray("shadowNumber")
          .and_then([&](JsonArray const& value) { return shadowParameters.randomizeParameterRange(value); })
          .and_then([](Json const& value) { return value.toInt(); });
      if (!number)
        return std::unexpected(std::move(number.error()));
      auto shadow = shadowImages->replace("<num>", toString(*number));
      if (!shadow)
        return std::unexpected(std::move(shadow.error()));
      layers.push_back({std::move(*shadow), *imageScale});
    }
  }
  return layers;
}

std::expected<List<std::pair<String, String>>, StarException> CelestialGraphics::worldHorizonImages(Root& root, CelestialParameters const& celestialParameters) {
  auto assets = root.assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  auto liquidsDatabase = root.liquidsDatabase();
  if (!liquidsDatabase)
    return std::unexpected(std::move(liquidsDatabase.error()));
  auto getLR = [](String const& base) -> std::expected<std::pair<String, String>, StarException> {
    auto left = base.replace("<selector>", "l");
    if (!left)
      return std::unexpected(std::move(left.error()));
    auto right = base.replace("<selector>", "r");
    if (!right)
      return std::unexpected(std::move(right.error()));
    return std::pair<String, String>(std::move(*left), std::move(*right));
  };
  auto type = celestialParameters.getParameter("worldType").and_then([](Json const& value) { return value.toString(); });
  if (!type)
    return std::unexpected(std::move(type.error()));
  List<std::pair<String, String>> images;
  if (*type == "Terrestrial") {
    auto terrestrialParameters = as<TerrestrialWorldParameters>(celestialParameters.visitableParameters());
    if (!terrestrialParameters)
      return images;
    auto graphics = (*assets)->json("/celestial.config:terrestrialHorizonGraphics");
    if (!graphics)
      return std::unexpected(std::move(graphics.error()));
    auto defaults = graphics->get("default");
    if (!defaults)
      return std::unexpected(std::move(defaults.error()));
    auto overrides = graphics->get(terrestrialParameters->typeName, JsonObject());
    if (!overrides)
      return std::unexpected(std::move(overrides.error()));
    auto gfxConfig = jsonMerge(*defaults, *overrides);
    auto baseImages = gfxConfig.getString("baseImages");
    if (!baseImages)
      return std::unexpected(std::move(baseImages.error()));
    auto atmoTextures = gfxConfig.getString("atmosphereTextures");
    if (!atmoTextures)
      return std::unexpected(std::move(atmoTextures.error()));
    auto shadowTextures = gfxConfig.getString("shadowTextures");
    if (!shadowTextures)
      return std::unexpected(std::move(shadowTextures.error()));
    auto maskTextures = gfxConfig.getString("maskTextures");
    if (!maskTextures)
      return std::unexpected(std::move(maskTextures.error()));
    auto liquidTextures = gfxConfig.getString("liquidTextures");
    if (!liquidTextures)
      return std::unexpected(std::move(liquidTextures.error()));
    auto numMasks = gfxConfig.get("maskRange").and_then([](Json const& value) { return jsonToVec2I(value); });
    if (!numMasks)
      return std::unexpected(std::move(numMasks.error()));
    auto maskPerPlanetRange = gfxConfig.get("maskPerPlanetRange").and_then([](Json const& value) { return jsonToVec2I(value); });
    if (!maskPerPlanetRange)
      return std::unexpected(std::move(maskPerPlanetRange.error()));
    auto biomeHueShift = "?" + imageOperationToString(HueShiftImageOperation::hueShiftDegrees(terrestrialParameters->hueShift));
    if (terrestrialParameters->primarySurfaceLiquid != EmptyLiquidId) {
      RandomSource random(celestialParameters.seed());
      auto count = random.randInt((*maskPerPlanetRange)[0], (*maskPerPlanetRange)[1]);
      if (!count)
        return std::unexpected(std::move(count.error()));
      List<int> masks;
      for (int i = 0; i < *count; ++i) {
        auto mask = random.randInt((*numMasks)[0], (*numMasks)[1]);
        if (!mask)
          return std::unexpected(std::move(mask.error()));
        masks.push_back(*mask);
      }
      auto liquidName = (*liquidsDatabase)->liquidName(terrestrialParameters->primarySurfaceLiquid);
      if (!liquidName)
        return std::unexpected(std::move(liquidName.error()));
      auto liquidBase = liquidTextures->replace("<liquid>", *liquidName).and_then(getLR);
      if (!liquidBase)
        return std::unexpected(std::move(liquidBase.error()));
      images.push_back(std::move(*liquidBase));
      StringList masksLeft, masksRight;
      for (auto mask : masks) {
        auto lr = maskTextures->replace("<mask>", toString(mask)).and_then(getLR);
        if (!lr)
          return std::unexpected(std::move(lr.error()));
        masksLeft.push_back(std::move(lr->first));
        masksRight.push_back(std::move(lr->second));
      }
      String leftMask, rightMask;
      if (!masksLeft.empty())
        leftMask = "?" + imageOperationToString(AlphaMaskImageOperation{AlphaMaskImageOperation::Additive, masksLeft, {0, 0}});
      if (!masksRight.empty())
        rightMask = "?" + imageOperationToString(AlphaMaskImageOperation{AlphaMaskImageOperation::Additive, masksRight, {0, 0}});
      auto base = getLR(*baseImages + biomeHueShift);
      if (!base)
        return std::unexpected(std::move(base.error()));
      images.push_back({base->first + leftMask, base->second + rightMask});
    } else {
      auto base = getLR(*baseImages + biomeHueShift);
      if (!base)
        return std::unexpected(std::move(base.error()));
      images.push_back(std::move(*base));
    }
    auto atmosphere = celestialParameters.getParameter("atmosphere", true).and_then([](Json const& value) { return value.toBool(); });
    if (!atmosphere)
      return std::unexpected(std::move(atmosphere.error()));
    if (*atmosphere) {
      auto atmo = getLR(*atmoTextures);
      if (!atmo)
        return std::unexpected(std::move(atmo.error()));
      images.push_back(std::move(*atmo));
    }
    auto shadow = getLR(*shadowTextures);
    if (!shadow)
      return std::unexpected(std::move(shadow.error()));
    images.push_back(std::move(*shadow));
  } else if (*type == "Asteroids") {
    auto image = (*assets)->json("/celestial.config:asteroidsHorizons")
        .and_then([](Json const& value) { return value.toString(); }).and_then(getLR);
    if (!image)
      return std::unexpected(std::move(image.error()));
    images.push_back(std::move(*image));
  } else if (*type == "FloatingDungeon") {
    auto dungeon = as<FloatingDungeonWorldParameters>(celestialParameters.visitableParameters());
    auto horizons = (*assets)->json("/celestial.config:floatingDungeonHorizons");
    if (!horizons)
      return std::unexpected(std::move(horizons.error()));
    auto contains = horizons->contains(dungeon->primaryDungeon);
    if (!contains)
      return std::unexpected(std::move(contains.error()));
    if (*contains) {
      auto image = horizons->get(dungeon->primaryDungeon)
          .and_then([](Json const& value) { return value.toString(); }).and_then(getLR);
      if (!image)
        return std::unexpected(std::move(image.error()));
      images.push_back(std::move(*image));
    }
  }
  return images;
}

std::expected<int, StarException> CelestialGraphics::worldRadialPosition(Root& root, CelestialParameters const& parameters) {
  if (!parameters.coordinate().isPlanetaryBody() && !parameters.coordinate().isSatelliteBody())
    return 0;
  auto positions = parameters.coordinate().isPlanetaryBody() ? planetRadialPositions(root) : satelliteRadialPositions(root);
  if (!positions)
    return std::unexpected(std::move(positions.error()));
  return static_cast<int>(static_random_u32(parameters.seed(), "RadialNumber") % *positions);
}

std::expected<int, StarException> CelestialGraphics::planetRadialPositions(Root& root) {
  return root.assets().and_then([](auto const& assets) { return assets->json("/celestial.config:planetRadialSlots"); })
      .and_then([](Json const& value) { return value.toInt(); });
}

std::expected<int, StarException> CelestialGraphics::satelliteRadialPositions(Root& root) {
  return root.assets().and_then([](auto const& assets) { return assets->json("/celestial.config:satelliteRadialSlots"); })
      .and_then([](Json const& value) { return value.toInt(); });
}

std::expected<List<std::pair<String, float>>, StarException> CelestialGraphics::drawSystemTwinkle(
    Root& root, std::shared_ptr<CelestialDatabase> celestialDatabase, CelestialCoordinate const& system, double time) {
  auto parameters = celestialDatabase->parameters(system);
  if (!parameters)
    return std::unexpected(std::move(parameters.error()));
  if (!*parameters)
    return List<std::pair<String, float>>();
  auto assets = root.assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  auto count = (*assets)->json("/celestial.config:twinkleFrames").and_then([](Json const& value) { return value.toInt(); });
  if (!count)
    return std::unexpected(std::move(count.error()));
  auto scale = (*assets)->json("/celestial.config:twinkleScale").and_then([](Json const& value) { return value.toFloat(); });
  if (!scale)
    return std::unexpected(std::move(scale.error()));
  auto frames = (**parameters).getParameter("twinkleFrames").and_then([](Json const& value) { return value.toString(); });
  if (!frames)
    return std::unexpected(std::move(frames.error()));
  auto duration = (**parameters).randomizeParameterRange("twinkleTime").and_then([](Json const& value) { return value.toFloat(); });
  if (!duration)
    return std::unexpected(std::move(duration.error()));
  auto background = (**parameters).getParameter("twinkleBackground").and_then([](Json const& value) { return value.toString(); });
  if (!background)
    return std::unexpected(std::move(background.error()));
  double frame = std::fmod<double>(time / *duration, 1.0f) * *count;
  return List<std::pair<String, float>>{{std::move(*background), 1.0f}, {strf("{}:{}", *frames, static_cast<int>(frame)), *scale}};
}

std::expected<List<std::pair<String, float>>, StarException> CelestialGraphics::drawSystemPlanetaryObject(
    std::shared_ptr<CelestialDatabase> celestialDatabase, CelestialCoordinate const& coordinate) {
  auto parameters = celestialDatabase->parameters(coordinate);
  if (!parameters)
    return std::unexpected(std::move(parameters.error()));
  if (*parameters)
    return drawSystemPlanetaryObject(**parameters);
  return List<std::pair<String, float>>();
}

std::expected<List<std::pair<String, float>>, StarException> CelestialGraphics::drawSystemCentralBody(
    std::shared_ptr<CelestialDatabase> celestialDatabase, CelestialCoordinate const& coordinate) {
  auto parameters = celestialDatabase->parameters(coordinate);
  if (!parameters)
    return std::unexpected(std::move(parameters.error()));
  if (*parameters)
    return drawSystemCentralBody(**parameters);
  return List<std::pair<String, float>>();
}

std::expected<List<std::pair<String, float>>, StarException> CelestialGraphics::drawWorld(
    Root& root, std::shared_ptr<CelestialDatabase> celestialDatabase, CelestialCoordinate const& coordinate) {
  auto parameters = celestialDatabase->parameters(coordinate);
  if (!parameters)
    return std::unexpected(std::move(parameters.error()));
  if (!*parameters)
    return List<std::pair<String, float>>();
  if (coordinate.isSatelliteBody()) {
    auto parent = coordinate.parent();
    if (!parent)
      return std::unexpected(std::move(parent.error()));
    auto shadow = celestialDatabase->parameters(*parent);
    if (!shadow)
      return std::unexpected(std::move(shadow.error()));
    return drawWorld(root, **parameters, *shadow);
  }
  return drawWorld(root, **parameters, {});
}

std::expected<List<std::pair<String, String>>, StarException> CelestialGraphics::worldHorizonImages(
    Root& root, std::shared_ptr<CelestialDatabase> celestialDatabase, CelestialCoordinate const& coordinate) {
  auto parameters = celestialDatabase->parameters(coordinate);
  if (!parameters)
    return std::unexpected(std::move(parameters.error()));
  if (*parameters)
    return worldHorizonImages(root, **parameters);
  return List<std::pair<String, String>>();
}

std::expected<int, StarException> CelestialGraphics::worldRadialPosition(
    Root& root, std::shared_ptr<CelestialDatabase> celestialDatabase, CelestialCoordinate const& coordinate) {
  auto parameters = celestialDatabase->parameters(coordinate);
  if (!parameters)
    return std::unexpected(std::move(parameters.error()));
  if (*parameters)
    return worldRadialPosition(root, **parameters);
  return 0;
}

}

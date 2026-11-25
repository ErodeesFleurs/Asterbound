import star.json_extra;
import star.json;
import star.ordered_map;
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
import star.math_common;
namespace Star {

std::expected<List<std::pair<String, float>>, StarException> CelestialGraphics::draw_system_planetary_object(CelestialParameters const& parameters) {
  auto image = parameters.get_parameter("smallImage").and_then([](Json const& value) { return value.to_string(); });
  if (!image)
    return std::unexpected(std::move(image.error()));
  auto scale = parameters.get_parameter("smallImageScale").and_then([](Json const& value) { return value.to_float(); });
  if (!scale)
    return std::unexpected(std::move(scale.error()));
  return List<std::pair<String, float>>{{std::move(*image), *scale}};
}

std::expected<List<std::pair<String, float>>, StarException> CelestialGraphics::draw_system_central_body(CelestialParameters const& parameters) {
  auto image = parameters.get_parameter("image").and_then([](Json const& value) { return value.to_string(); });
  if (!image)
    return std::unexpected(std::move(image.error()));
  auto scale = parameters.get_parameter("imageScale").and_then([](Json const& value) { return value.to_float(); });
  if (!scale)
    return std::unexpected(std::move(scale.error()));
  return List<std::pair<String, float>>{{std::move(*image), *scale}};
}

std::expected<List<std::pair<String, float>>, StarException> CelestialGraphics::draw_world(
    Root& root, CelestialParameters const& celestial_parameters, std::optional<CelestialParameters> const& override_shadow_parameters) {
  auto assets = root.assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  auto liquids_database = root.liquids_database();
  if (!liquids_database)
    return std::unexpected(std::move(liquids_database.error()));
  auto const& shadow_parameters = override_shadow_parameters ? *override_shadow_parameters : celestial_parameters;
  auto type = celestial_parameters.get_parameter("worldType").and_then([](Json const& value) { return value.to_string(); });
  if (!type)
    return std::unexpected(std::move(type.error()));
  List<std::pair<String, float>> layers;

  if (*type == "Terrestrial") {
    auto terrestrial_parameters = as<TerrestrialWorldParameters>(celestial_parameters.visitable_parameters());
    if (!terrestrial_parameters)
      return layers;
    auto graphics = (*assets)->json("/celestial.config:terrestrialGraphics");
    if (!graphics)
      return std::unexpected(std::move(graphics.error()));
    auto defaults = graphics->get("default");
    if (!defaults)
      return std::unexpected(std::move(defaults.error()));
    auto overrides = graphics->get(terrestrial_parameters->typeName, JsonObject());
    if (!overrides)
      return std::unexpected(std::move(overrides.error()));
    auto gfx_config = json_merge(*defaults, *overrides);
    auto liquid_images = gfx_config.get_string("liquidImages", "");
    if (!liquid_images)
      return std::unexpected(std::move(liquid_images.error()));
    auto base_images = gfx_config.get_string("baseImages", "");
    if (!base_images)
      return std::unexpected(std::move(base_images.error()));
    auto shadow_images = gfx_config.get_string("shadowImages", "");
    if (!shadow_images)
      return std::unexpected(std::move(shadow_images.error()));
    auto base_count = gfx_config.get_int("baseCount", 0);
    if (!base_count)
      return std::unexpected(std::move(base_count.error()));
    auto dynamics_images = gfx_config.get_string("dynamicsImages", "");
    if (!dynamics_images)
      return std::unexpected(std::move(dynamics_images.error()));
    auto image_scale = celestial_parameters.get_parameter("imageScale", 1.0f).and_then([](Json const& value) { return value.to_float(); });
    if (!image_scale)
      return std::unexpected(std::move(image_scale.error()));

    if (terrestrial_parameters->primarySurfaceLiquid != EmptyLiquidId && !liquid_images->empty()) {
      auto liquid_name = (*liquids_database)->liquid_name(terrestrial_parameters->primarySurfaceLiquid);
      if (!liquid_name)
        return std::unexpected(std::move(liquid_name.error()));
      auto image = liquid_images->replace("<liquid>", *liquid_name);
      if (!image)
        return std::unexpected(std::move(image.error()));
      layers.push_back({std::move(*image), *image_scale});
    } else if (*base_count > 0) {
      auto image = base_images->replace("<biome>", terrestrial_parameters->primaryBiome)
          .and_then([&](String const& value) { return value.replace("<num>", Star::to_string(*base_count)); });
      if (!image)
        return std::unexpected(std::move(image.error()));
      layers.push_back({Star::format("{}?hueshift={}", *image, terrestrial_parameters->hueShift), *image_scale});
    }
    for (int i = 0; i < *base_count; ++i) {
      auto base_image = base_images->replace("<num>", Star::to_string(*base_count - i));
      if (!base_image)
        return std::unexpected(std::move(base_image.error()));
      String hue_shift_string, dynamic_mask_string;
      if (!dynamics_images->empty()) {
        auto dynamic = gfx_config.get_array("dynamicsRange")
            .and_then([&](JsonArray const& value) { return celestial_parameters.randomize_parameter_range(value, i); })
            .and_then([](Json const& value) { return value.to_int(); });
        if (!dynamic)
          return std::unexpected(std::move(dynamic.error()));
        auto mask = dynamics_images->replace("<num>", Star::to_string(*dynamic));
        if (!mask)
          return std::unexpected(std::move(mask.error()));
        dynamic_mask_string = "?addmask=" + *mask;
      }
      if (terrestrial_parameters->hueShift != 0)
        hue_shift_string = Star::format("?hueshift={}", terrestrial_parameters->hueShift);
      layers.push_back({*base_image + hue_shift_string + dynamic_mask_string, *image_scale});
    }
    if (!shadow_images->empty()) {
      auto number = gfx_config.get_array("shadowNumber")
          .and_then([&](JsonArray const& value) { return shadow_parameters.randomize_parameter_range(value); })
          .and_then([](Json const& value) { return value.to_int(); });
      if (!number)
        return std::unexpected(std::move(number.error()));
      auto shadow = shadow_images->replace("<num>", Star::to_string(*number));
      if (!shadow)
        return std::unexpected(std::move(shadow.error()));
      layers.push_back({std::move(*shadow), *image_scale});
    }
  } else if (*type == "Asteroids") {
    auto mask_images = celestial_parameters.get_parameter("maskImages").and_then([](Json const& value) { return value.to_string(); });
    if (!mask_images)
      return std::unexpected(std::move(mask_images.error()));
    auto mask_count = celestial_parameters.get_parameter("masks").and_then([](Json const& value) { return value.to_int(); });
    if (!mask_count)
      return std::unexpected(std::move(mask_count.error()));
    auto dynamics_images = celestial_parameters.get_parameter("dynamicsImages").and_then([](Json const& value) { return value.to_string(); });
    if (!dynamics_images)
      return std::unexpected(std::move(dynamics_images.error()));
    auto image_scale = celestial_parameters.get_parameter("imageScale", 1.0f).and_then([](Json const& value) { return value.to_float(); });
    if (!image_scale)
      return std::unexpected(std::move(image_scale.error()));
    for (int i = 0; i < *mask_count; ++i) {
      auto base = mask_images->replace("<num>", Star::to_string(*mask_count - i));
      if (!base)
        return std::unexpected(std::move(base.error()));
      auto number = celestial_parameters.randomize_parameter_range("dynamicsRange", i).and_then([](Json const& value) { return value.to_int(); });
      if (!number)
        return std::unexpected(std::move(number.error()));
      auto mask = dynamics_images->replace("<num>", Star::to_string(*number));
      if (!mask)
        return std::unexpected(std::move(mask.error()));
      layers.push_back({Star::format("{}?addmask={}", *base, *mask), *image_scale});
    }
  } else if (*type == "FloatingDungeon") {
    auto image = celestial_parameters.get_parameter("image").and_then([](Json const& value) { return value.to_string(); });
    if (!image)
      return std::unexpected(std::move(image.error()));
    auto image_scale = celestial_parameters.get_parameter("imageScale", 1.0f).and_then([](Json const& value) { return value.to_float(); });
    if (!image_scale)
      return std::unexpected(std::move(image_scale.error()));
    layers.push_back({std::move(*image), *image_scale});
    auto dynamics_images = celestial_parameters.get_parameter("dynamicsImages").and_then([](Json const& value) { return value.to_string(); });
    if (!dynamics_images)
      return std::unexpected(std::move(dynamics_images.error()));
    if (!dynamics_images->empty()) {
      auto number = celestial_parameters.randomize_parameter_range("dynamicsRange").and_then([](Json const& value) { return value.to_int(); });
      if (!number)
        return std::unexpected(std::move(number.error()));
      auto dynamic = dynamics_images->replace("<num>", Star::to_string(*number));
      if (!dynamic)
        return std::unexpected(std::move(dynamic.error()));
      layers.push_back({std::move(*dynamic), *image_scale});
    }
  } else if (*type == "GasGiant") {
    auto gfx_config = (*assets)->json("/celestial.config:gasGiantGraphics");
    if (!gfx_config)
      return std::unexpected(std::move(gfx_config.error()));
    auto base_image = gfx_config->get_string("baseImage", "");
    if (!base_image)
      return std::unexpected(std::move(base_image.error()));
    auto shadow_images = gfx_config->get_string("shadowImages", "");
    if (!shadow_images)
      return std::unexpected(std::move(shadow_images.error()));
    auto dynamics_images = gfx_config->get_string("dynamicsImages", "");
    if (!dynamics_images)
      return std::unexpected(std::move(dynamics_images.error()));
    auto overlay_images = gfx_config->get_string("overlayImages", "");
    if (!overlay_images)
      return std::unexpected(std::move(overlay_images.error()));
    auto overlay_count = gfx_config->get_int("overlayCount", 0);
    if (!overlay_count)
      return std::unexpected(std::move(overlay_count.error()));
    auto image_scale = celestial_parameters.get_parameter("imageScale", 1.0f).and_then([](Json const& value) { return value.to_float(); });
    if (!image_scale)
      return std::unexpected(std::move(image_scale.error()));
    auto hue_shift = gfx_config->get_array("primaryHueShiftRange")
        .and_then([&](JsonArray const& value) { return celestial_parameters.randomize_parameter_range(value); })
        .and_then([](Json const& value) { return value.to_float(); });
    if (!hue_shift)
      return std::unexpected(std::move(hue_shift.error()));
    if (!base_image->empty())
      layers.push_back({Star::format("{}?hueshift={}", *base_image, *hue_shift), *image_scale});
    if (!overlay_images->empty()) {
      for (int i = 0; i < *overlay_count; ++i) {
        auto offset = gfx_config->get_array("hueShiftOffsetRange")
            .and_then([&](JsonArray const& value) { return celestial_parameters.randomize_parameter_range(value); })
            .and_then([](Json const& value) { return value.to_float(); });
        if (!offset)
          return std::unexpected(std::move(offset.error()));
        *hue_shift += *offset;
        auto number = gfx_config->get_array("dynamicsRange")
            .and_then([&](JsonArray const& value) { return celestial_parameters.randomize_parameter_range(value, i); })
            .and_then([](Json const& value) { return value.to_int(); });
        if (!number)
          return std::unexpected(std::move(number.error()));
        auto mask = dynamics_images->replace("<num>", Star::to_string(*number));
        if (!mask)
          return std::unexpected(std::move(mask.error()));
        auto overlay = overlay_images->replace("<num>", Star::to_string(i));
        if (!overlay)
          return std::unexpected(std::move(overlay.error()));
        layers.push_back({Star::format("{}?hueshift={}?addmask={}", *overlay, *hue_shift, *mask), *image_scale});
      }
    }
    if (!shadow_images->empty()) {
      auto number = gfx_config->get_array("shadowNumber")
          .and_then([&](JsonArray const& value) { return shadow_parameters.randomize_parameter_range(value); })
          .and_then([](Json const& value) { return value.to_int(); });
      if (!number)
        return std::unexpected(std::move(number.error()));
      auto shadow = shadow_images->replace("<num>", Star::to_string(*number));
      if (!shadow)
        return std::unexpected(std::move(shadow.error()));
      layers.push_back({std::move(*shadow), *image_scale});
    }
  }
  return layers;
}

std::expected<List<std::pair<String, String>>, StarException> CelestialGraphics::world_horizon_images(Root& root, CelestialParameters const& celestial_parameters) {
  auto assets = root.assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  auto liquids_database = root.liquids_database();
  if (!liquids_database)
    return std::unexpected(std::move(liquids_database.error()));
  auto get_lr = [](String const& base) -> std::expected<std::pair<String, String>, StarException> {
    auto left = base.replace("<selector>", "l");
    if (!left)
      return std::unexpected(std::move(left.error()));
    auto right = base.replace("<selector>", "r");
    if (!right)
      return std::unexpected(std::move(right.error()));
    return std::pair<String, String>(std::move(*left), std::move(*right));
  };
  auto type = celestial_parameters.get_parameter("worldType").and_then([](Json const& value) { return value.to_string(); });
  if (!type)
    return std::unexpected(std::move(type.error()));
  List<std::pair<String, String>> images;
  if (*type == "Terrestrial") {
    auto terrestrial_parameters = as<TerrestrialWorldParameters>(celestial_parameters.visitable_parameters());
    if (!terrestrial_parameters)
      return images;
    auto graphics = (*assets)->json("/celestial.config:terrestrialHorizonGraphics");
    if (!graphics)
      return std::unexpected(std::move(graphics.error()));
    auto defaults = graphics->get("default");
    if (!defaults)
      return std::unexpected(std::move(defaults.error()));
    auto overrides = graphics->get(terrestrial_parameters->typeName, JsonObject());
    if (!overrides)
      return std::unexpected(std::move(overrides.error()));
    auto gfx_config = json_merge(*defaults, *overrides);
    auto base_images = gfx_config.get_string("baseImages");
    if (!base_images)
      return std::unexpected(std::move(base_images.error()));
    auto atmo_textures = gfx_config.get_string("atmosphereTextures");
    if (!atmo_textures)
      return std::unexpected(std::move(atmo_textures.error()));
    auto shadow_textures = gfx_config.get_string("shadowTextures");
    if (!shadow_textures)
      return std::unexpected(std::move(shadow_textures.error()));
    auto mask_textures = gfx_config.get_string("maskTextures");
    if (!mask_textures)
      return std::unexpected(std::move(mask_textures.error()));
    auto liquid_textures = gfx_config.get_string("liquidTextures");
    if (!liquid_textures)
      return std::unexpected(std::move(liquid_textures.error()));
    auto num_masks = gfx_config.get("maskRange").and_then([](Json const& value) { return json_to_vec2i(value); });
    if (!num_masks)
      return std::unexpected(std::move(num_masks.error()));
    auto mask_per_planet_range = gfx_config.get("maskPerPlanetRange").and_then([](Json const& value) { return json_to_vec2i(value); });
    if (!mask_per_planet_range)
      return std::unexpected(std::move(mask_per_planet_range.error()));
    auto biome_hue_shift =
      "?" + image_operation_to_string(HueShiftImageOperation::hue_shift_degrees(terrestrial_parameters->hueShift));
    if (terrestrial_parameters->primarySurfaceLiquid != EmptyLiquidId) {
      RandomSource random(celestial_parameters.seed());
      auto count = random.uniform_int((*mask_per_planet_range)[0], (*mask_per_planet_range)[1]);
      if (!count)
        return std::unexpected(std::move(count.error()));
      List<int> masks;
      for (int i = 0; i < *count; ++i) {
        auto mask = random.uniform_int((*num_masks)[0], (*num_masks)[1]);
        if (!mask)
          return std::unexpected(std::move(mask.error()));
        masks.push_back(*mask);
      }
      auto liquid_name = (*liquids_database)->liquid_name(terrestrial_parameters->primarySurfaceLiquid);
      if (!liquid_name)
        return std::unexpected(std::move(liquid_name.error()));
      auto liquid_base = liquid_textures->replace("<liquid>", *liquid_name).and_then(get_lr);
      if (!liquid_base)
        return std::unexpected(std::move(liquid_base.error()));
      images.push_back(std::move(*liquid_base));
      StringList masks_left, masks_right;
      for (auto mask : masks) {
        auto lr = mask_textures->replace("<mask>", Star::to_string(mask)).and_then(get_lr);
        if (!lr)
          return std::unexpected(std::move(lr.error()));
        masks_left.push_back(std::move(lr->first));
        masks_right.push_back(std::move(lr->second));
      }
      String left_mask, right_mask;
      if (!masks_left.empty())
        left_mask = "?"
          + image_operation_to_string(AlphaMaskImageOperation{AlphaMaskImageOperation::Additive, masks_left, {0, 0}});
      if (!masks_right.empty())
        right_mask = "?"
          + image_operation_to_string(AlphaMaskImageOperation{AlphaMaskImageOperation::Additive, masks_right, {0, 0}});
      auto base = get_lr(*base_images + biome_hue_shift);
      if (!base)
        return std::unexpected(std::move(base.error()));
      images.push_back({base->first + left_mask, base->second + right_mask});
    } else {
      auto base = get_lr(*base_images + biome_hue_shift);
      if (!base)
        return std::unexpected(std::move(base.error()));
      images.push_back(std::move(*base));
    }
    auto atmosphere = celestial_parameters.get_parameter("atmosphere", true).and_then([](Json const& value) { return value.to_bool(); });
    if (!atmosphere)
      return std::unexpected(std::move(atmosphere.error()));
    if (*atmosphere) {
      auto atmo = get_lr(*atmo_textures);
      if (!atmo)
        return std::unexpected(std::move(atmo.error()));
      images.push_back(std::move(*atmo));
    }
    auto shadow = get_lr(*shadow_textures);
    if (!shadow)
      return std::unexpected(std::move(shadow.error()));
    images.push_back(std::move(*shadow));
  } else if (*type == "Asteroids") {
    auto image = (*assets)->json("/celestial.config:asteroidsHorizons")
        .and_then([](Json const& value) { return value.to_string(); }).and_then(get_lr);
    if (!image)
      return std::unexpected(std::move(image.error()));
    images.push_back(std::move(*image));
  } else if (*type == "FloatingDungeon") {
    auto dungeon = as<FloatingDungeonWorldParameters>(celestial_parameters.visitable_parameters());
    auto horizons = (*assets)->json("/celestial.config:floatingDungeonHorizons");
    if (!horizons)
      return std::unexpected(std::move(horizons.error()));
    auto contains = horizons->contains(dungeon->primaryDungeon);
    if (!contains)
      return std::unexpected(std::move(contains.error()));
    if (*contains) {
      auto image = horizons->get(dungeon->primaryDungeon)
          .and_then([](Json const& value) { return value.to_string(); }).and_then(get_lr);
      if (!image)
        return std::unexpected(std::move(image.error()));
      images.push_back(std::move(*image));
    }
  }
  return images;
}

std::expected<int, StarException> CelestialGraphics::world_radial_position(Root& root, CelestialParameters const& parameters) {
  if (!parameters.coordinate().is_planetary_body() && !parameters.coordinate().is_satellite_body())
    return 0;
  auto positions = parameters.coordinate().is_planetary_body() ? planet_radial_positions(root) : satellite_radial_positions(root);
  if (!positions)
    return std::unexpected(std::move(positions.error()));
  return static_cast<int>(static_random_u32(parameters.seed(), "RadialNumber") % *positions);
}

std::expected<int, StarException> CelestialGraphics::planet_radial_positions(Root& root) {
  return root.assets().and_then([](std::shared_ptr<Assets const> const& assets) { return assets->json("/celestial.config:planetRadialSlots"); })
      .and_then([](Json const& value) { return value.to_int(); });
}

std::expected<int, StarException> CelestialGraphics::satellite_radial_positions(Root& root) {
  return root.assets().and_then([](std::shared_ptr<Assets const> const& assets) { return assets->json("/celestial.config:satelliteRadialSlots"); })
      .and_then([](Json const& value) { return value.to_int(); });
}

#pragma float_control(precise, on, push)
std::expected<List<std::pair<String, float>>, StarException> CelestialGraphics::draw_system_twinkle(
    Root& root, CelestialDatabase& celestial_database, CelestialCoordinate const& system, double time) {
  auto parameters = celestial_database.parameters(system);
  if (!parameters)
    return std::unexpected(std::move(parameters.error()));
  if (!*parameters)
    return List<std::pair<String, float>>();
  auto assets = root.assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  auto count = (*assets)->json("/celestial.config:twinkleFrames").and_then([](Json const& value) { return value.to_double(); });
  if (!count)
    return std::unexpected(std::move(count.error()));
  if (!is_finite(*count) || *count < 1 || *count > std::numeric_limits<int>::max() || *count != std::trunc(*count))
    return std::unexpected(StarException("twinkleFrames must be a positive int"));
  auto scale = (*assets)->json("/celestial.config:twinkleScale").and_then([](Json const& value) { return value.to_float(); });
  if (!scale)
    return std::unexpected(std::move(scale.error()));
  auto frames = (**parameters).get_parameter("twinkleFrames").and_then([](Json const& value) { return value.to_string(); });
  if (!frames)
    return std::unexpected(std::move(frames.error()));
  auto duration = (**parameters).randomize_parameter_range("twinkleTime").and_then([](Json const& value) { return value.to_float(); });
  if (!duration)
    return std::unexpected(std::move(duration.error()));
  auto finite = classify_finite(*duration, time);
  if (!finite[0] || *duration <= 0)
    return std::unexpected(StarException("twinkleTime must be finite and positive"));
  if (!finite[1])
    return std::unexpected(StarException("Twinkle animation time is not finite"));
  auto background = (**parameters).get_parameter("twinkleBackground").and_then([](Json const& value) { return value.to_string(); });
  if (!background)
    return std::unexpected(std::move(background.error()));
  double cycles = time / *duration;
  if (!is_finite(cycles))
    return std::unexpected(StarException("Twinkle animation phase is not finite"));
  double frame = std::fmod(cycles, 1.0) * *count;
  return List<std::pair<String, float>>{{std::move(*background), 1.0f}, {Star::format("{}:{}", *frames, static_cast<int>(frame)), *scale}};
}
#pragma float_control(pop)

std::expected<List<std::pair<String, float>>, StarException> CelestialGraphics::draw_system_planetary_object(
    CelestialDatabase& celestial_database, CelestialCoordinate const& coordinate) {
  auto parameters = celestial_database.parameters(coordinate);
  if (!parameters)
    return std::unexpected(std::move(parameters.error()));
  if (*parameters)
    return draw_system_planetary_object(**parameters);
  return List<std::pair<String, float>>();
}

std::expected<List<std::pair<String, float>>, StarException> CelestialGraphics::draw_system_central_body(
    CelestialDatabase& celestial_database, CelestialCoordinate const& coordinate) {
  auto parameters = celestial_database.parameters(coordinate);
  if (!parameters)
    return std::unexpected(std::move(parameters.error()));
  if (*parameters)
    return draw_system_central_body(**parameters);
  return List<std::pair<String, float>>();
}

std::expected<List<std::pair<String, float>>, StarException> CelestialGraphics::draw_world(
    Root& root, CelestialDatabase& celestial_database, CelestialCoordinate const& coordinate) {
  auto parameters = celestial_database.parameters(coordinate);
  if (!parameters)
    return std::unexpected(std::move(parameters.error()));
  if (!*parameters)
    return List<std::pair<String, float>>();
  if (coordinate.is_satellite_body()) {
    auto parent = coordinate.parent();
    if (!parent)
      return std::unexpected(std::move(parent.error()));
    auto shadow = celestial_database.parameters(*parent);
    if (!shadow)
      return std::unexpected(std::move(shadow.error()));
    return draw_world(root, **parameters, *shadow);
  }
  return draw_world(root, **parameters, {});
}

std::expected<List<std::pair<String, String>>, StarException> CelestialGraphics::world_horizon_images(
    Root& root, CelestialDatabase& celestial_database, CelestialCoordinate const& coordinate) {
  auto parameters = celestial_database.parameters(coordinate);
  if (!parameters)
    return std::unexpected(std::move(parameters.error()));
  if (*parameters)
    return world_horizon_images(root, **parameters);
  return List<std::pair<String, String>>();
}

std::expected<int, StarException> CelestialGraphics::world_radial_position(
    Root& root, CelestialDatabase& celestial_database, CelestialCoordinate const& coordinate) {
  auto parameters = celestial_database.parameters(coordinate);
  if (!parameters)
    return std::unexpected(std::move(parameters.error()));
  if (*parameters)
    return world_radial_position(root, **parameters);
  return 0;
}

}

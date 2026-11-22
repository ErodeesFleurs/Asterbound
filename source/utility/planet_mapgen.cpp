#include <cstdio>
import star.json;
import star.output_proxy;
import star.native_runtime;
import star.ordered_map;
import star.vector;
import star.game_types;
import star.material_types;
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
import star.file;
import star.lexical_cast;
import star.image;
import star.string;
import star.ordered_set;
import std;
import star.format;
import star.io_device;
import star.ref_ptr;
import star.logging;
import star.listener;
import star.lru_cache;
import star.perlin;
import star.id_map;
import star.set;
import star.casting;
import star.strong_typedef;

import star.celestial_coordinate;
import star.sky_types;
import star.animation;
import star.particle;
import star.weather_types;
import star.force_regions;
import star.world_parameters;
import star.celestial_parameters;
import star.celestial_types;
import star.option_parser;
import star.version_option_parser;
import star.asset_source;
import star.assets;
import star.root_base;
import star.configuration;
import star.root;
import star.root_loader;
import star.world_layout;
import star.damage_types;
import star.world_geometry;
import star.status_types;
import star.damage;
import star.light_source;
import star.entity;
import star.interaction_types;
import star.item_descriptor;
import star.quest_descriptor;
import star.interactive_entity;
import star.collision_block;
import star.tile_entity;
import star.tile_damage;
import star.inspectable_entity;
import star.plant;
import star.plant_database;
import star.biome_placement;
import star.sky_parameters;
import star.world_template;


import star.celestial_database;

import star.static_random;
using namespace Star;

[[nodiscard]]
static std::expected<void, StarException> run(std::span<char const* const> argv) {
    auto nativeRuntime = NativeRuntime::create();
    if (!nativeRuntime)
      return std::unexpected(std::move(nativeRuntime.error()));
    auto loader = RootLoader::create({{}, {}, {}, LogLevel::Error, false, {}});
    if (!loader)
      return std::unexpected(std::move(loader.error()));
    auto& root_loader = *loader;
    root_loader.set_summary("Generate a WorldTemplate and output the data in it to an image");
    auto coordinate_parameter = root_loader.add_parameter("coordinate", "coordinate", OptionParser::Optional, "coordinate for the celestial world");
    if (!coordinate_parameter)
      return std::unexpected(std::move(coordinate_parameter.error()));
    auto seed_parameter = root_loader.add_parameter("coordseed", "seed", OptionParser::Optional, "seed to use when selecting a random celestial world coordinate");
    if (!seed_parameter)
      return std::unexpected(std::move(seed_parameter.error()));
    auto size_parameter = root_loader.add_parameter("size", "size", OptionParser::Optional, "x,y size of the region to be rendered");
    if (!size_parameter)
      return std::unexpected(std::move(size_parameter.error()));
    auto weighting_switch = root_loader.add_switch("weighting", "Output instead the region weighting at each point");
    if (!weighting_switch)
      return std::unexpected(std::move(weighting_switch.error()));
    auto noise_switch = root_loader.add_switch("weightingblocknoise", "apply layout block noise before outputting weighting");
    if (!noise_switch)
      return std::unexpected(std::move(noise_switch.error()));
    auto transition_switch = root_loader.add_switch("transition", "show biome transition regions");
    if (!transition_switch)
      return std::unexpected(std::move(transition_switch.error()));

    auto initialized = root_loader.command_init_or_die(argv);
    if (!initialized)
      return std::unexpected(std::move(initialized.error()));
    auto& [root, options] = *initialized;
    auto celestialDatabase = CelestialMasterDatabase::create(*root);
    if (!celestialDatabase)
      return std::unexpected(std::move(celestialDatabase.error()));

    std::optional<CelestialCoordinate> coordinate;
    if (auto coordinate_text = options.first_parameter("coordinate")) {
      auto parsed = CelestialCoordinate::create(Json(coordinate_text->get()));
      if (!parsed)
        return std::unexpected(std::move(parsed.error()));
      coordinate = std::move(*parsed);
    } else {
      std::optional<std::uint64_t> seed;
      if (auto seed_text = options.first_parameter("coordseed")) {
        auto parsed = lexical_cast<std::uint64_t>(seed_text->get());
        if (!parsed)
          return std::unexpected(std::move(parsed.error()));
        seed = *parsed;
      }
      auto found = (*celestialDatabase)->findRandomWorld(10, 50, {}, seed);
      if (!found)
        return std::unexpected(std::move(found.error()));
      coordinate = std::move(*found);
    }
    if (!coordinate)
      return std::unexpected(StarException("Could not find world to generate, try again"));

    if (auto written = Star::print("Generating world with coordinate {}\n", *coordinate); !written)
      return std::unexpected(std::move(written.error()));
    auto createdTemplate = WorldTemplate::create(*root, *coordinate, *celestialDatabase);
    if (!createdTemplate)
      return std::unexpected(std::move(createdTemplate.error()));
    auto& worldTemplate = **createdTemplate;
    auto size = worldTemplate.size();

    if (auto size_text = options.first_parameter("size")) {
      auto components = size_text->get().split(",");
      if (!components)
        return std::unexpected(std::move(components.error()));
      auto widthText = components->at(0);
      if (!widthText)
        return std::unexpected(std::move(widthText.error()));
      auto heightText = components->at(1);
      if (!heightText)
        return std::unexpected(std::move(heightText.error()));
      auto width = lexical_cast<unsigned>(widthText->get());
      if (!width)
        return std::unexpected(std::move(width.error()));
      auto height = lexical_cast<unsigned>(heightText->get());
      if (!height)
        return std::unexpected(std::move(height.error()));
      size = Vec2U(*width, *height).piecewise_clamp(Vec2U(0, 0), size);
    } else if (size[0] > 1000) {
      size[0] = 1000;
    }

    if (auto written = Star::print("Generating {} size image for world of type '{}'\n", size, worldTemplate.worldParameters()->typeName); !written)
      return std::unexpected(std::move(written.error()));
    auto outputImage = std::make_shared<Image>(size, PixelFormat::RGB24);

    Color groundColor = Color::rgb(255, 0, 0);
    Color caveColor = Color::rgb(128, 0, 0);
    Color blankColor = Color::rgb(0, 0, 0);

    for (std::size_t x = 0; x < size[0]; ++x) {
      for (std::size_t y = 0; y < size[1]; ++y) {
        if (options.switches.contains("weighting")) {
          auto layout = worldTemplate.worldLayout();
          Color color = Color::Black;
          Vec2I pos(x, y);
          if (options.switches.contains("weightingblocknoise")) {
            if (auto blockNoise = layout->blockNoise()) {
              auto adjusted = blockNoise->apply(pos, size);
              if (!adjusted)
                return std::unexpected(std::move(adjusted.error()));
              pos = *adjusted;
            }
          }
          auto weightings = layout->getWeighting(pos[0], pos[1]);
          if (!weightings)
            return std::unexpected(std::move(weightings.error()));
          for (auto const& weighting : *weightings) {
            Color mixColor = Color::rgb(128, 0, 0);
            mixColor.set_hue(
              static_random_float(reinterpret_cast<std::uintptr_t>(std::addressof(weighting.region.get()))));
            color = Color::rgba_f(color.to_rgba_f() + mixColor.to_rgba_f() * weighting.weight);
          }
          auto written = outputImage->set(x, y, color.to_rgb());
          if (!written)
            return std::unexpected(std::move(written.error()));
        } else if (options.switches.contains("transition")) {
          auto block = worldTemplate.blockInfo(x, y);
          if (!block)
            return std::unexpected(std::move(block.error()));
          auto const& blockInfo = *block;
          if (isRealMaterial(blockInfo.foreground)) {
            Color color = groundColor;
            color.set_hue(blockInfo.biomeTransition ? 0 : 0.5f);
            auto written = outputImage->set(x, y, color.to_rgb());
            if (!written)
              return std::unexpected(std::move(written.error()));
          } else if (isRealMaterial(blockInfo.background)) {
            Color color = caveColor;
            color.set_hue(blockInfo.biomeTransition ? 0 : 0.5f);
            auto written = outputImage->set(x, y, color.to_rgb());
            if (!written)
              return std::unexpected(std::move(written.error()));
          } else {
            auto written = outputImage->set(x, y, blankColor.to_rgb());
            if (!written)
              return std::unexpected(std::move(written.error()));
          }
        } else {
          // Image y = 0 is the top, so reverse it for the world tile
          auto block = worldTemplate.blockInfo(x, y);
          if (!block)
            return std::unexpected(std::move(block.error()));
          auto const& blockInfo = *block;
          if (isRealMaterial(blockInfo.foreground)) {
            Color color = groundColor;
            color.set_hue(static_random_float(blockInfo.foreground));
            color.set_saturation(static_random_float(blockInfo.foregroundMod));
            auto written = outputImage->set(x, y, color.to_rgb());
            if (!written)
              return std::unexpected(std::move(written.error()));
          } else if (isRealMaterial(blockInfo.background)) {
            Color color = caveColor;
            color.set_hue(static_random_float(blockInfo.background));
            color.set_saturation(static_random_float(blockInfo.backgroundMod));
            auto written = outputImage->set(x, y, color.to_rgb());
            if (!written)
              return std::unexpected(std::move(written.error()));
          } else {
            auto written = outputImage->set(x, y, blankColor.to_rgb());
            if (!written)
              return std::unexpected(std::move(written.error()));
          }
        }
      }
    }

    auto outputFile = File::open("mapgen.png", IOMode::Write);
    if (!outputFile)
      return std::unexpected(std::move(outputFile.error()));
    auto written = outputImage->write_png(root->logger(), *outputFile);
    if (!written)
      return std::unexpected(std::move(written.error()));
    return (*outputFile)->close();
}

int main(int argc, char** argv) {
  auto result = run({argv, static_cast<std::size_t>(argc)});
  if (!result) {
    if (auto written = Star::print(stderr, "exception caught: {}\n", Star::output_exception(ExceptionDiagnostic(result.error()), true)); !written)
      return 1;
    return 1;
  }
  return 0;
}

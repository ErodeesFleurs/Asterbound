import star.json;
import star.output_proxy;
import star.native_runtime;
import star.ordered_map;
import star.either;
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
import star.version;
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
static std::expected<void, StarException> run(int argc, char** argv) {
    auto nativeRuntime = NativeRuntime::create();
    if (!nativeRuntime)
      return std::unexpected(std::move(nativeRuntime.error()));
    auto loader = RootLoader::create({{}, {}, {}, LogLevel::Error, false, {}});
    if (!loader)
      return std::unexpected(std::move(loader.error()));
    auto& rootLoader = *loader;
    rootLoader.setSummary("Generate a WorldTemplate and output the data in it to an image");
    auto coordinateParameter = rootLoader.addParameter("coordinate", "coordinate", OptionParser::Optional, "coordinate for the celestial world");
    if (!coordinateParameter)
      return std::unexpected(std::move(coordinateParameter.error()));
    auto seedParameter = rootLoader.addParameter("coordseed", "seed", OptionParser::Optional, "seed to use when selecting a random celestial world coordinate");
    if (!seedParameter)
      return std::unexpected(std::move(seedParameter.error()));
    auto sizeParameter = rootLoader.addParameter("size", "size", OptionParser::Optional, "x,y size of the region to be rendered");
    if (!sizeParameter)
      return std::unexpected(std::move(sizeParameter.error()));
    auto weightingSwitch = rootLoader.addSwitch("weighting", "Output instead the region weighting at each point");
    if (!weightingSwitch)
      return std::unexpected(std::move(weightingSwitch.error()));
    auto noiseSwitch = rootLoader.addSwitch("weightingblocknoise", "apply layout block noise before outputting weighting");
    if (!noiseSwitch)
      return std::unexpected(std::move(noiseSwitch.error()));
    auto transitionSwitch = rootLoader.addSwitch("transition", "show biome transition regions");
    if (!transitionSwitch)
      return std::unexpected(std::move(transitionSwitch.error()));

    auto initialized = rootLoader.commandInitOrDie(argc, argv);
    if (!initialized)
      return std::unexpected(std::move(initialized.error()));
    auto& [root, options] = *initialized;
    auto celestialDatabase = CelestialMasterDatabase::create(*root);
    if (!celestialDatabase)
      return std::unexpected(std::move(celestialDatabase.error()));

    std::optional<CelestialCoordinate> coordinate;
    if (auto coordinateOption = options.parameters.find_ref("coordinate");
        coordinateOption && !coordinateOption->get().empty()) {
      auto parsed = CelestialCoordinate::create(Json(coordinateOption->get()[0]));
      if (!parsed)
        return std::unexpected(std::move(parsed.error()));
      coordinate = std::move(*parsed);
    } else {
      std::optional<std::uint64_t> seed;
      if (auto seedOption = options.parameters.find_ref("coordseed"); seedOption && !seedOption->get().empty()) {
        auto parsed = lexicalCast<std::uint64_t>(seedOption->get()[0]);
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

    coutf("Generating world with coordinate {}\n", *coordinate);
    auto createdTemplate = WorldTemplate::create(*root, *coordinate, *celestialDatabase);
    if (!createdTemplate)
      return std::unexpected(std::move(createdTemplate.error()));
    auto& worldTemplate = **createdTemplate;
    auto size = worldTemplate.size();

    if (auto sizeOption = options.parameters.find_ref("size"); sizeOption && !sizeOption->get().empty()) {
      auto components = sizeOption->get()[0].split(",");
      if (!components)
        return std::unexpected(std::move(components.error()));
      auto widthText = components->at(0);
      if (!widthText)
        return std::unexpected(std::move(widthText.error()));
      auto heightText = components->at(1);
      if (!heightText)
        return std::unexpected(std::move(heightText.error()));
      auto width = lexicalCast<unsigned>(widthText->get());
      if (!width)
        return std::unexpected(std::move(width.error()));
      auto height = lexicalCast<unsigned>(heightText->get());
      if (!height)
        return std::unexpected(std::move(height.error()));
      size = Vec2U(*width, *height).piecewise_clamp(Vec2U(0, 0), size);
    } else if (size[0] > 1000) {
      size[0] = 1000;
    }

    coutf("Generating {} size image for world of type '{}'\n", size, worldTemplate.worldParameters()->typeName);
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
            mixColor.setHue(
              static_random_float(reinterpret_cast<std::uintptr_t>(std::addressof(weighting.region.get()))));
            color = Color::rgbaf(color.toRgbaF() + mixColor.toRgbaF() * weighting.weight);
          }
          auto written = outputImage->set(x, y, color.toRgb());
          if (!written)
            return std::unexpected(std::move(written.error()));
        } else if (options.switches.contains("transition")) {
          auto block = worldTemplate.blockInfo(x, y);
          if (!block)
            return std::unexpected(std::move(block.error()));
          auto const& blockInfo = *block;
          if (isRealMaterial(blockInfo.foreground)) {
            Color color = groundColor;
            color.setHue(blockInfo.biomeTransition ? 0 : 0.5f);
            auto written = outputImage->set(x, y, color.toRgb());
            if (!written)
              return std::unexpected(std::move(written.error()));
          } else if (isRealMaterial(blockInfo.background)) {
            Color color = caveColor;
            color.setHue(blockInfo.biomeTransition ? 0 : 0.5f);
            auto written = outputImage->set(x, y, color.toRgb());
            if (!written)
              return std::unexpected(std::move(written.error()));
          } else {
            auto written = outputImage->set(x, y, blankColor.toRgb());
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
            color.setHue(static_random_float(blockInfo.foreground));
            color.setSaturation(static_random_float(blockInfo.foregroundMod));
            auto written = outputImage->set(x, y, color.toRgb());
            if (!written)
              return std::unexpected(std::move(written.error()));
          } else if (isRealMaterial(blockInfo.background)) {
            Color color = caveColor;
            color.setHue(static_random_float(blockInfo.background));
            color.setSaturation(static_random_float(blockInfo.backgroundMod));
            auto written = outputImage->set(x, y, color.toRgb());
            if (!written)
              return std::unexpected(std::move(written.error()));
          } else {
            auto written = outputImage->set(x, y, blankColor.toRgb());
            if (!written)
              return std::unexpected(std::move(written.error()));
          }
        }
      }
    }

    auto outputFile = File::open("mapgen.png", IOMode::Write);
    if (!outputFile)
      return std::unexpected(std::move(outputFile.error()));
    auto written = outputImage->writePng(root->logger(), *outputFile);
    if (!written)
      return std::unexpected(std::move(written.error()));
    return (*outputFile)->close();
}

int main(int argc, char** argv) {
  auto result = run(argc, argv);
  if (!result) {
    cerrf("exception caught: {}\n", outputException(ExceptionDiagnostic(result.error()), true));
    return 1;
  }
  return 0;
}

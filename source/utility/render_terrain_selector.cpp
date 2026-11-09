#include <cstdio>
import star.json;
import star.output_proxy;
import star.native_runtime;
import star.file;
import star.lexical_cast;
import star.image;
import star.string;
import star.ordered_map;
import star.ordered_set;
import std;
import star.format;
import star.version;
import star.bi_map;
import star.thread;
import star.io_device;
import star.list;
import star.image_processing;
import star.hash;
import star.data_stream;
import star.string_view;
import star.directives;
import star.asset_path;
import star.ref_ptr;
import star.logging;
import star.listener;
import star.random;
import star.color;
import star.multi_array;

import star.option_parser;
import star.version_option_parser;
import star.asset_source;
import star.assets;
import star.root_base;
import star.configuration;
import star.root;
import star.root_loader;

import star.terrain_database;

import star.vector;
import star.math_common;
using namespace Star;

template <typename T>
[[nodiscard]]
static std::expected<T, StarException> numericOption(OptionParser::Options const& options, String const& name, T fallback) {
  auto values = options.parameters.find_ref(name);
  if (!values || values->get().empty())
    return fallback;
  return lexical_cast<T>(values->get()[0]);
}

[[nodiscard]]
static std::expected<void, StarException> run(int argc, char** argv) {
  auto nativeRuntime = NativeRuntime::create();
  if (!nativeRuntime)
    return std::unexpected(std::move(nativeRuntime.error()));
  auto loader = RootLoader::create({{}, {}, {}, LogLevel::Error, false, {}});
  if (!loader)
    return std::unexpected(std::move(loader.error()));
  auto& rootLoader = *loader;
  rootLoader.setSummary("Generate a heatmap image visualizing the output of a given terrain selector");

  auto selectorParameter = rootLoader.addParameter("selector", "selector", OptionParser::Required, "name of the terrain selector to be rendered");
  if (!selectorParameter)
    return std::unexpected(std::move(selectorParameter.error()));
  auto sizeParameter = rootLoader.addParameter("size", "size", OptionParser::Required, "x,y size of the region to be rendered");
  if (!sizeParameter)
    return std::unexpected(std::move(sizeParameter.error()));
  auto seedParameter = rootLoader.addParameter("seed", "seed", OptionParser::Optional, "seed value for the selector");
  if (!seedParameter)
    return std::unexpected(std::move(seedParameter.error()));
  auto commonalityParameter = rootLoader.addParameter("commonality", "commonality", OptionParser::Optional, "commonality value for the selector (default 1)");
  if (!commonalityParameter)
    return std::unexpected(std::move(commonalityParameter.error()));
  auto scaleParameter = rootLoader.addParameter("scale", "scale", OptionParser::Optional, "maximum distance from 0 for color range");
  if (!scaleParameter)
    return std::unexpected(std::move(scaleParameter.error()));
  auto modeParameter = rootLoader.addParameter("mode", "mode", OptionParser::Optional, "color mode: heatmap, terrain");
  if (!modeParameter)
    return std::unexpected(std::move(modeParameter.error()));

  auto initialized = rootLoader.commandInitOrDie(argc, argv);
  if (!initialized)
    return std::unexpected(std::move(initialized.error()));
  auto& [root, options] = *initialized;

  auto sizeValues = options.parameters.get("size");
  if (!sizeValues)
    return std::unexpected(std::move(sizeValues.error()));
  auto sizeText = sizeValues->get().first();
  if (!sizeText)
    return std::unexpected(std::move(sizeText.error()));
  auto components = sizeText->get().split(",");
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
  Vec2U size(*width, *height);

  auto seed = numericOption<std::uint64_t>(options, "seed", root->random().randu64());
  if (!seed)
    return std::unexpected(std::move(seed.error()));
  auto commonality = numericOption<float>(options, "commonality", 1.0f);
  if (!commonality)
    return std::unexpected(std::move(commonality.error()));
  auto requestedScale = numericOption<float>(options, "scale", 1.0f);
  if (!requestedScale)
    return std::unexpected(std::move(requestedScale.error()));
  float scale = *requestedScale;
  auto scaleValues = options.parameters.find_ref("scale");
  bool autoScale = !scaleValues || scaleValues->get().empty();

  String mode = "heatmap";
  if (auto modeValues = options.parameters.find_ref("mode"); modeValues && !modeValues->get().empty())
    mode = modeValues->get()[0];

  auto selectorParameters = TerrainSelectorParameters::create(JsonObject{
      {"worldWidth", size[0]},
      {"baseHeight", size[1] / 2},
      {"seed", *seed},
      {"commonality", *commonality}
    });
  if (!selectorParameters)
    return std::unexpected(std::move(selectorParameters.error()));
  auto terrainDatabase = root->terrainDatabase();
  if (!terrainDatabase)
    return std::unexpected(std::move(terrainDatabase.error()));
  auto selectorValues = options.parameters.get("selector");
  if (!selectorValues)
    return std::unexpected(std::move(selectorValues.error()));
  auto selectorName = selectorValues->get().first();
  if (!selectorName)
    return std::unexpected(std::move(selectorName.error()));
  auto selector = (*terrainDatabase)->createNamedSelector(selectorName->get(), *selectorParameters);
  if (!selector)
    return std::unexpected(std::move(selector.error()));

  MultiArray<float, 2> terrainResult({size[0], size[1]}, 0.0f);
  for (std::size_t x = 0; x < size[0]; ++x) {
    for (std::size_t y = 0; y < size[1]; ++y) {
      auto value = (*selector)->get(x, y);
      if (!value)
        return std::unexpected(std::move(value.error()));
      terrainResult(x, y) = *value;
      if (autoScale)
        scale = std::max(scale, std::abs(*value));
    }
  }

  if (auto written = Star::print("Generating {} size image for selector with scale {}\n", size, scale); !written)
    return std::unexpected(std::move(written.error()));
  auto outputImage = std::make_shared<Image>(size, PixelFormat::RGB24);
  for (std::size_t x = 0; x < size[0]; ++x) {
    for (std::size_t y = 0; y < size[1]; ++y) {
      // Image y = 0 is the top, so reverse it for the world position
      auto value = terrainResult(x, y) / scale;
      if (mode == "heatmap") {
        Color color = Color::rgb(255, 0, 0);
        color.setHue(std::clamp(value / 2 + 0.5f, 0.0f, 1.0f));
        auto written = outputImage->set(x, y, color.toRgb());
        if (!written)
          return std::unexpected(std::move(written.error()));
      } else if (mode == "terrain") {
        auto written = value > 0
            ? outputImage->set(x, y, Vec3B(0, 100 + std::floor(155 * value), std::floor(255 * value)))
            : outputImage->set(x, y, Vec3B(std::floor(255 * -value), 0, 0));
        if (!written)
          return std::unexpected(std::move(written.error()));
      }
    }
  }

  auto outputFile = File::open("terrain.png", IOMode::Write);
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
    if (auto written = Star::print(stderr, "exception caught: {}\n", Star::output_exception(ExceptionDiagnostic(result.error()), true)); !written)
      return 1;
    return 1;
  }
  return 0;
}

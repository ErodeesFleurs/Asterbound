#include <cstdio>
import star.json;
import star.output_proxy;
import star.native_runtime;
import star.file;
import star.lexical_cast;
import star.numeric_option;
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

[[nodiscard]]
static std::expected<void, StarException> run(std::span<char const* const> argv) {
  auto nativeRuntime = NativeRuntime::create();
  if (!nativeRuntime)
    return std::unexpected(std::move(nativeRuntime.error()));
  auto loader = RootLoader::create({{}, {}, {}, LogLevel::Error, false, {}});
  if (!loader)
    return std::unexpected(std::move(loader.error()));
  auto& root_loader = *loader;
  root_loader.set_summary("Generate a heatmap image visualizing the output of a given terrain selector");

  auto selector_parameter = root_loader.add_parameter("selector", "selector", OptionParser::Required, "name of the terrain selector to be rendered");
  if (!selector_parameter)
    return std::unexpected(std::move(selector_parameter.error()));
  auto size_parameter = root_loader.add_parameter("size", "size", OptionParser::Required, "x,y size of the region to be rendered");
  if (!size_parameter)
    return std::unexpected(std::move(size_parameter.error()));
  auto seed_parameter = root_loader.add_parameter("seed", "seed", OptionParser::Optional, "seed value for the selector");
  if (!seed_parameter)
    return std::unexpected(std::move(seed_parameter.error()));
  auto commonality_parameter = root_loader.add_parameter("commonality", "commonality", OptionParser::Optional, "commonality value for the selector (default 1)");
  if (!commonality_parameter)
    return std::unexpected(std::move(commonality_parameter.error()));
  auto scale_parameter = root_loader.add_parameter("scale", "scale", OptionParser::Optional, "maximum distance from 0 for color range");
  if (!scale_parameter)
    return std::unexpected(std::move(scale_parameter.error()));
  auto mode_parameter = root_loader.add_parameter("mode", "mode", OptionParser::Optional, "color mode: heatmap, terrain");
  if (!mode_parameter)
    return std::unexpected(std::move(mode_parameter.error()));

  auto initialized = root_loader.command_init_or_die(argv);
  if (!initialized)
    return std::unexpected(std::move(initialized.error()));
  auto& [root, options] = *initialized;

  auto size_text = options.first_parameter("size");
  if (!size_text)
    return std::unexpected(OptionParserException("Missing required parameter '-size'"));
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
  Vec2U size(*width, *height);

  auto seed = numeric_option<std::uint64_t>(options, "seed", root->random().next_u64(), EmptyParameterList::UseFallback);
  if (!seed)
    return std::unexpected(std::move(seed.error()));
  auto commonality = numeric_option<float>(options, "commonality", 1.0f, EmptyParameterList::UseFallback);
  if (!commonality)
    return std::unexpected(std::move(commonality.error()));
  auto scale_text = options.first_parameter("scale");
  float scale = 1.0f;
  if (scale_text) {
    auto parsed = lexical_cast<float>(scale_text->get());
    if (!parsed)
      return std::unexpected(std::move(parsed.error()));
    scale = *parsed;
  }

  auto mode_text = options.first_parameter("mode");
  std::string_view mode = mode_text ? std::string_view(mode_text->get().utf8()) : "heatmap";

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
  auto selector_name = options.first_parameter("selector");
  if (!selector_name)
    return std::unexpected(OptionParserException("Missing required parameter '-selector'"));
  auto selector = (*terrainDatabase)->createNamedSelector(selector_name->get(), *selectorParameters);
  if (!selector)
    return std::unexpected(std::move(selector.error()));

  MultiArray<float, 2> terrainResult({size[0], size[1]}, 0.0f);
  for (std::size_t x = 0; x < size[0]; ++x) {
    for (std::size_t y = 0; y < size[1]; ++y) {
      auto value = (*selector)->get(x, y);
      if (!value)
        return std::unexpected(std::move(value.error()));
      terrainResult(x, y) = *value;
      if (!scale_text)
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
        color.set_hue(std::clamp(value / 2 + 0.5f, 0.0f, 1.0f));
        auto written = outputImage->set(x, y, color.to_rgb());
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

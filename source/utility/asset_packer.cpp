#include <cstdio>
import star.io_device;
import star.json;
import star.asset_source;
import star.string;
import star.directory_asset_source;
import star.thread;
import star.json_extra;
import star.file;
import star.ordered_map;
import star.ordered_set;
import star.option_parser;
import std;
import star.format;
import star.output_proxy;
import star.version_option_parser;
import star.packed_asset_source;
import star.logging;
import star.native_runtime;

using namespace Star;

[[nodiscard]]
static std::expected<int, StarException> pack_assets(std::span<char const* const> argv) {
  Logger logger;
  auto startTime = std::chrono::steady_clock::now();

  auto parser = VersionOptionParser::create();
  if (!parser)
    return std::unexpected(std::move(parser.error()));
  auto& option_parser = *parser;
  option_parser.set_version_name("Asset Packer");
  option_parser.set_summary("Packs asset folder into a starbound .pak file");
  auto config_parameter = option_parser.add_parameter("c", "configFile", OptionParser::Optional, "JSON file with ignore lists and ordering info");
  if (!config_parameter)
    return std::unexpected(std::move(config_parameter.error()));
  auto server_switch = option_parser.add_switch("s", "Enable server mode");
  if (!server_switch)
    return std::unexpected(std::move(server_switch.error()));
  auto verbose_switch = option_parser.add_switch("v", "Verbose, list each file added");
  if (!verbose_switch)
    return std::unexpected(std::move(verbose_switch.error()));
  option_parser.add_argument("assets folder path", OptionParser::Required, "Path to the assets to be packed");
  option_parser.add_argument("output filename", OptionParser::Required, "Output pak file");

  auto parsed = option_parser.command_parse_or_die(argv);
  if (!parsed)
    return std::unexpected(std::move(parsed.error()));
  auto& opts = *parsed;
  auto assets_argument = opts.arguments.at(0);
  if (!assets_argument)
    return std::unexpected(std::move(assets_argument.error()));
  auto const& assets_folder_path = assets_argument->get();
  auto output_argument = opts.arguments.at(1);
  if (!output_argument)
    return std::unexpected(std::move(output_argument.error()));
  String output_filename = std::move(output_argument->get());

  StringList ignoreFiles;
  StringList extensionOrdering;
  if (auto config_argument = opts.first_parameter("c")) {
    auto const& config_file = config_argument->get();
    auto configFileContents = File::read_file_string(config_file);
    if (!configFileContents) {
      if (!configFileContents.error().is<IOException>())
        return std::unexpected(std::move(configFileContents.error()));
      if (auto written = Star::print(stderr, "Could not open specified configFile: {}\n", config_file); !written)
        return std::unexpected(std::move(written.error()));
      if (auto written = Star::print(stderr, "For the following reason: {}\n", Star::output_exception(ExceptionDiagnostic(configFileContents.error()), false)); !written)
        return std::unexpected(std::move(written.error()));
      return 1;
    }

    auto configFileJson = Json::parse_json(*configFileContents);
    if (!configFileJson) {
      if (!configFileJson.error().is<JsonParsingException>())
        return std::unexpected(std::move(configFileJson.error()));
      if (auto written = Star::print(stderr, "Could not parse the specified configFile: {}\n", config_file); !written)
        return std::unexpected(std::move(written.error()));
      if (auto written = Star::print(stderr, "For the following reason: {}\n", Star::output_exception(ExceptionDiagnostic(configFileJson.error()), false)); !written)
        return std::unexpected(std::move(written.error()));
      return 1;
    }

    auto readConfig = [&]() -> std::expected<void, StarException> {
      auto globalIgnore = configFileJson->get("globalIgnore", JsonArray());
      if (!globalIgnore)
        return std::unexpected(std::move(globalIgnore.error()));
      auto globalPatterns = json_to_string_list(*globalIgnore);
      if (!globalPatterns)
        return std::unexpected(std::move(globalPatterns.error()));
      ignoreFiles = std::move(*globalPatterns);
      if (opts.switches.contains("s")) {
        auto serverIgnore = configFileJson->get("serverIgnore", JsonArray());
        if (!serverIgnore)
          return std::unexpected(std::move(serverIgnore.error()));
        auto serverPatterns = json_to_string_list(*serverIgnore);
        if (!serverPatterns)
          return std::unexpected(std::move(serverPatterns.error()));
        ignoreFiles.append_range(std::move(*serverPatterns));
      }
      auto ordering = configFileJson->get("extensionOrdering", JsonArray());
      if (!ordering)
        return std::unexpected(std::move(ordering.error()));
      auto extensions = json_to_string_list(*ordering);
      if (!extensions)
        return std::unexpected(std::move(extensions.error()));
      extensionOrdering = std::move(*extensions);
      return {};
    };
    auto configRead = readConfig();
    if (!configRead) {
      if (!configRead.error().is<JsonException>())
        return std::unexpected(std::move(configRead.error()));
      if (auto written = Star::print(stderr, "Could not read the asset_packer config file {}\n", config_file); !written)
        return std::unexpected(std::move(written.error()));
      if (auto written = Star::print(stderr, "{}\n", Star::output_exception(ExceptionDiagnostic(configRead.error()), false)); !written)
        return std::unexpected(std::move(written.error()));
      return 1;
    }
  }

  bool verbose = opts.switches.contains("v");
  auto progressCallback = [verbose](std::size_t, std::size_t, String filePath, String assetPath) -> std::expected<void, StarException> {
    if (verbose)
      return Star::print("Adding file '{}' to the target pak as '{}'\n", filePath, assetPath);
    return {};
  };

  auto outputDirectory = File::full_path(File::dir_name(output_filename));
  if (!outputDirectory)
    return std::unexpected(std::move(outputDirectory.error()));
  output_filename = File::relative_to(*outputDirectory, File::base_name(output_filename));
  auto directorySource = DirectoryAssetSource::create(logger, assets_folder_path, ignoreFiles);
  if (!directorySource)
    return std::unexpected(std::move(directorySource.error()));
  auto built = PackedAssetSource::build(**directorySource, output_filename, extensionOrdering, progressCallback);
  if (!built)
    return std::unexpected(std::move(built.error()));

  if (auto written = Star::print("Output packed assets to {} in {}s\n", output_filename, std::chrono::duration<double>(std::chrono::steady_clock::now() - startTime).count()); !written)
    return std::unexpected(std::move(written.error()));
  return 0;
}

int main(int argc, char** argv) {
  auto nativeRuntime = NativeRuntime::create();
  if (!nativeRuntime) {
    if (auto written = Star::print(stderr, "Exception caught: {}\n", Star::output_exception(ExceptionDiagnostic(nativeRuntime.error()), true)); !written)
      return 1;
    return 1;
  }
  auto result = pack_assets({argv, static_cast<std::size_t>(argc)});
  if (!result) {
    if (auto written = Star::print(stderr, "Exception caught: {}\n", Star::output_exception(ExceptionDiagnostic(result.error()), true)); !written)
      return 1;
    return 1;
  }
  return *result;
}

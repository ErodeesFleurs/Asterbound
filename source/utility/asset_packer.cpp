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
  auto start_time = std::chrono::steady_clock::now();

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

  StringList ignore_files;
  StringList extension_ordering;
  if (auto config_argument = opts.first_parameter("c")) {
    auto const& config_file = config_argument->get();
    auto config_file_contents = File::read_file_string(config_file);
    if (!config_file_contents) {
      if (!config_file_contents.error().is<IOException>())
        return std::unexpected(std::move(config_file_contents.error()));
      if (auto written = Star::print(stderr, "Could not open specified configFile: {}\n", config_file); !written)
        return std::unexpected(std::move(written.error()));
      if (auto written = Star::print(stderr, "For the following reason: {}\n", Star::output_exception(ExceptionDiagnostic(config_file_contents.error()), false)); !written)
        return std::unexpected(std::move(written.error()));
      return 1;
    }

    auto config_file_json = Json::parse_json(*config_file_contents);
    if (!config_file_json) {
      if (!config_file_json.error().is<JsonParsingException>())
        return std::unexpected(std::move(config_file_json.error()));
      if (auto written = Star::print(stderr, "Could not parse the specified configFile: {}\n", config_file); !written)
        return std::unexpected(std::move(written.error()));
      if (auto written = Star::print(stderr, "For the following reason: {}\n", Star::output_exception(ExceptionDiagnostic(config_file_json.error()), false)); !written)
        return std::unexpected(std::move(written.error()));
      return 1;
    }

    auto read_config = [&]() -> std::expected<void, StarException> {
      auto global_ignore = config_file_json->get("globalIgnore", JsonArray());
      if (!global_ignore)
        return std::unexpected(std::move(global_ignore.error()));
      auto global_patterns = json_to_string_list(*global_ignore);
      if (!global_patterns)
        return std::unexpected(std::move(global_patterns.error()));
      ignore_files = std::move(*global_patterns);
      if (opts.switches.contains("s")) {
        auto server_ignore = config_file_json->get("serverIgnore", JsonArray());
        if (!server_ignore)
          return std::unexpected(std::move(server_ignore.error()));
        auto server_patterns = json_to_string_list(*server_ignore);
        if (!server_patterns)
          return std::unexpected(std::move(server_patterns.error()));
        ignore_files.append_range(std::move(*server_patterns));
      }
      auto ordering = config_file_json->get("extensionOrdering", JsonArray());
      if (!ordering)
        return std::unexpected(std::move(ordering.error()));
      auto extensions = json_to_string_list(*ordering);
      if (!extensions)
        return std::unexpected(std::move(extensions.error()));
      extension_ordering = std::move(*extensions);
      return {};
    };
    auto config_read = read_config();
    if (!config_read) {
      if (!config_read.error().is<JsonException>())
        return std::unexpected(std::move(config_read.error()));
      if (auto written = Star::print(stderr, "Could not read the asset_packer config file {}\n", config_file); !written)
        return std::unexpected(std::move(written.error()));
      if (auto written = Star::print(stderr, "{}\n", Star::output_exception(ExceptionDiagnostic(config_read.error()), false)); !written)
        return std::unexpected(std::move(written.error()));
      return 1;
    }
  }

  PackedAssetSource::BuildProgressCallback progress_callback;
  if (opts.switches.contains("v"))
    progress_callback = [](std::size_t, std::size_t, String const& file_path, String const& asset_path) {
      return Star::print("Adding file '{}' to the target pak as '{}'\n", file_path, asset_path);
    };

  auto output_directory = File::full_path(File::dir_name(output_filename));
  if (!output_directory)
    return std::unexpected(std::move(output_directory.error()));
  output_filename = File::relative_to(*output_directory, File::base_name(output_filename));
  auto directory_source = DirectoryAssetSource::create(logger, assets_folder_path, ignore_files);
  if (!directory_source)
    return std::unexpected(std::move(directory_source.error()));
  auto built = PackedAssetSource::build(**directory_source, output_filename, extension_ordering, progress_callback);
  if (!built)
    return std::unexpected(std::move(built.error()));

  if (auto written = Star::print("Output packed assets to {} in {}s\n", output_filename, std::chrono::duration<double>(std::chrono::steady_clock::now() - start_time).count()); !written)
    return std::unexpected(std::move(written.error()));
  return 0;
}

int main(int argc, char** argv) {
  auto native_runtime = NativeRuntime::create();
  if (!native_runtime) {
    if (auto written = Star::print(stderr, "Exception caught: {}\n", Star::output_exception(ExceptionDiagnostic(native_runtime.error()), true)); !written)
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

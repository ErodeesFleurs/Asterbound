#include <cstdio>
import star.memory;
import star.io_device;
import star.json;
import star.asset_source;
import star.thread;
import star.json_extra;
import star.file;
import star.string;
import star.ordered_map;
import star.ordered_set;
import star.option_parser;
import std;
import star.format;
import star.output_proxy;
import star.version_option_parser;
import star.packed_asset_source;
import star.native_runtime;

using namespace Star;

[[nodiscard]]
static std::expected<int, StarException> unpack_assets(std::span<char const* const> argv) {
  auto start_time = std::chrono::steady_clock::now();

  auto parser = VersionOptionParser::create();
  if (!parser)
    return std::unexpected(std::move(parser.error()));
  auto& option_parser = *parser;
  option_parser.set_version_name("Asset Unpacker");
  option_parser.set_summary("Unpacks a starbound .pak file into an asset folder");
  auto verbose_switch = option_parser.add_switch("v", "Verbose, list each file extracted");
  if (!verbose_switch)
    return std::unexpected(std::move(verbose_switch.error()));
  option_parser.add_argument("input pak path", OptionParser::Required, "Path to the .pak file to unpack");
  option_parser.add_argument("output folder", OptionParser::Required, "Output folder");

  auto parsed = option_parser.command_parse_or_die(argv);
  if (!parsed)
    return std::unexpected(std::move(parsed.error()));
  auto& opts = *parsed;
  auto input_argument = opts.arguments.at(0);
  if (!input_argument)
    return std::unexpected(std::move(input_argument.error()));
  auto const& input_file = input_argument->get();
  auto output_argument = opts.arguments.at(1);
  if (!output_argument)
    return std::unexpected(std::move(output_argument.error()));
  auto const& output_folder_path = output_argument->get();
  bool verbose = opts.switches.contains("v");

  auto assets_pack = PackedAssetSource::create(input_file);
  if (!assets_pack)
    return std::unexpected(std::move(assets_pack.error()));

  auto output_is_directory = File::is_directory(output_folder_path);
  if (!output_is_directory)
    return std::unexpected(std::move(output_is_directory.error()));
  if (!*output_is_directory) {
    auto made_directory = File::create_directory(output_folder_path);
    if (!made_directory)
      return std::unexpected(std::move(made_directory.error()));
  }
  auto changed_directory = File::change_directory(output_folder_path);
  if (!changed_directory)
    return std::unexpected(std::move(changed_directory.error()));

  auto visited = (*assets_pack)->for_each_path([&](String const& file) -> std::expected<void, StarException> {
    auto extract_entry = [&]() -> std::expected<void, StarException> {
      // The index of a pak is untrusted input, so an entry must not be able to
      // escape the output folder when it is used as a path below.
      bool unsafe_name = file.empty();
      auto components = file.split('/');
      if (!components)
        return std::unexpected(std::move(components.error()));
      for (auto const& component : *components) {
        auto backslash = component.find('\\');
        if (!backslash)
          return std::unexpected(std::move(backslash.error()));
        if (component == ".." || *backslash != npos)
          unsafe_name = true;
      }
      if (unsafe_name) {
        return Star::print(*stderr, "Skipping entry with unsafe path: {}\n", file);
      }

      auto file_data = (*assets_pack)->read(file);
      if (!file_data)
        return std::unexpected(std::move(file_data.error()));
      auto relative_path = "." + file;
      auto relative_dir = File::dir_name(relative_path);
      auto made_directory = File::create_directories(relative_dir);
      if (!made_directory)
        return std::unexpected(std::move(made_directory.error()));
      auto written = File::write_file(*file_data, relative_path);
      if (!written)
        return std::unexpected(std::move(written.error()));
      if (verbose) {
        if (auto written = Star::print("Extracting file '{}' to the output directory as '{}'\n", file, relative_path); !written)
          return std::unexpected(std::move(written.error()));
      }
      return {};
    };
    auto extracted = extract_entry();
    if (!extracted) {
      if (!extracted.error().is<AssetSourceException>())
        return std::unexpected(std::move(extracted.error()));
      if (auto written = Star::print(*stderr, "Could not open file: {}\n", file); !written)
        return std::unexpected(std::move(written.error()));
      if (auto written = Star::print(*stderr, "Reason: {}\n", Star::output_exception(ExceptionDiagnostic(extracted.error()), false)); !written)
        return std::unexpected(std::move(written.error()));
    }
    return {};
  });
  if (!visited)
    return std::unexpected(std::move(visited.error()));

  auto const& metadata = (*assets_pack)->metadata();
  if (!metadata.empty()) {
    auto printed = Json(metadata).print_json(2);
    if (!printed)
      return std::unexpected(std::move(printed.error()));
    auto written = File::write_file(*printed, "_metadata");
    if (!written)
      return std::unexpected(std::move(written.error()));
  }

  if (auto written = Star::print("Unpacked assets to {} in {}s\n", output_folder_path, std::chrono::duration<double>(std::chrono::steady_clock::now() - start_time).count()); !written)
    return std::unexpected(std::move(written.error()));
  return 0;
}

int main(int argc, char** argv) {
  auto native_runtime = NativeRuntime::create();
  if (!native_runtime) {
    if (auto written = Star::print(*stderr, "Exception caught: {}\n", Star::output_exception(ExceptionDiagnostic(native_runtime.error()), true)); !written)
      return 1;
    return 1;
  }
  auto result = unpack_assets({argv, static_cast<std::size_t>(argc)});
  if (!result) {
    if (auto written = Star::print(*stderr, "Exception caught: {}\n", Star::output_exception(ExceptionDiagnostic(result.error()), true)); !written)
      return 1;
    return 1;
  }
  return *result;
}

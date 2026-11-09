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
import star.version;
import star.version_option_parser;
import star.packed_asset_source;
import star.native_runtime;

using namespace Star;

[[nodiscard]]
static std::expected<int, StarException> unpackAssets(int argc, char** argv) {
  auto startTime = std::chrono::steady_clock::now();

  auto parser = VersionOptionParser::create();
  if (!parser)
    return std::unexpected(std::move(parser.error()));
  auto& optParse = *parser;
  optParse.setVersionName("Asset Unpacker");
  optParse.setSummary("Unpacks a starbound .pak file into an asset folder");
  auto verboseSwitch = optParse.addSwitch("v", "Verbose, list each file extracted");
  if (!verboseSwitch)
    return std::unexpected(std::move(verboseSwitch.error()));
  optParse.addArgument("input pak path", OptionParser::Required, "Path to the .pak file to unpack");
  optParse.addArgument("output folder", OptionParser::Required, "Output folder");

  auto parsed = optParse.commandParseOrDie(argc, argv);
  if (!parsed)
    return std::unexpected(std::move(parsed.error()));
  auto& opts = *parsed;
  auto inputArgument = opts.arguments.at(0);
  if (!inputArgument)
    return std::unexpected(std::move(inputArgument.error()));
  String inputFile = inputArgument->get();
  auto outputArgument = opts.arguments.at(1);
  if (!outputArgument)
    return std::unexpected(std::move(outputArgument.error()));
  String outputFolderPath = outputArgument->get();
  bool verbose = opts.switches.contains("v");

  auto assetsPack = PackedAssetSource::create(inputFile);
  if (!assetsPack)
    return std::unexpected(std::move(assetsPack.error()));

  auto outputIsDirectory = File::is_directory(outputFolderPath);
  if (!outputIsDirectory)
    return std::unexpected(std::move(outputIsDirectory.error()));
  if (!*outputIsDirectory) {
    auto madeDirectory = File::create_directory(outputFolderPath);
    if (!madeDirectory)
      return std::unexpected(std::move(madeDirectory.error()));
  }
  auto changedDirectory = File::change_directory(outputFolderPath);
  if (!changedDirectory)
    return std::unexpected(std::move(changedDirectory.error()));

  auto allFiles = (*assetsPack)->assetPaths();
  for (auto const& file : allFiles) {
    auto extractEntry = [&]() -> std::expected<void, StarException> {
      // The index of a pak is untrusted input, so an entry must not be able to
      // escape the output folder when it is used as a path below.
      bool unsafeName = file.empty();
      auto components = file.split('/');
      if (!components)
        return std::unexpected(std::move(components.error()));
      for (auto const& component : *components) {
        auto backslash = component.find('\\');
        if (!backslash)
          return std::unexpected(std::move(backslash.error()));
        if (component == ".." || *backslash != NPos)
          unsafeName = true;
      }
      if (unsafeName) {
        return Star::print(stderr, "Skipping entry with unsafe path: {}\n", file);
      }

      auto fileData = (*assetsPack)->read(file);
      if (!fileData)
        return std::unexpected(std::move(fileData.error()));
      auto relativePath = "." + file;
      auto relativeDir = File::dir_name(relativePath);
      auto madeDirectory = File::create_directories(relativeDir);
      if (!madeDirectory)
        return std::unexpected(std::move(madeDirectory.error()));
      auto written = File::write_file(*fileData, relativePath);
      if (!written)
        return std::unexpected(std::move(written.error()));
      if (verbose) {
        if (auto written = Star::print("Extracting file '{}' to the output directory as '{}'\n", file, relativePath); !written)
          return std::unexpected(std::move(written.error()));
      }
      return {};
    };
    auto extracted = extractEntry();
    if (!extracted) {
      if (!extracted.error().is<AssetSourceException>())
        return std::unexpected(std::move(extracted.error()));
      if (auto written = Star::print(stderr, "Could not open file: {}\n", file); !written)
        return std::unexpected(std::move(written.error()));
      if (auto written = Star::print(stderr, "Reason: {}\n", Star::output_exception(ExceptionDiagnostic(extracted.error()), false)); !written)
        return std::unexpected(std::move(written.error()));
    }
  }

  auto metadata = (*assetsPack)->metadata();
  if (!metadata.empty()) {
    auto printed = Json(std::move(metadata)).printJson(2);
    if (!printed)
      return std::unexpected(std::move(printed.error()));
    auto written = File::write_file(*printed, "_metadata");
    if (!written)
      return std::unexpected(std::move(written.error()));
  }

  if (auto written = Star::print("Unpacked assets to {} in {}s\n", outputFolderPath, std::chrono::duration<double>(std::chrono::steady_clock::now() - startTime).count()); !written)
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
  auto result = unpackAssets(argc, argv);
  if (!result) {
    if (auto written = Star::print(stderr, "Exception caught: {}\n", Star::output_exception(ExceptionDiagnostic(result.error()), true)); !written)
      return 1;
    return 1;
  }
  return *result;
}

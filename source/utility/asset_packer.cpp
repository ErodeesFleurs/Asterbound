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
import star.version;
import star.version_option_parser;
import star.packed_asset_source;
import star.logging;
import star.native_runtime;

using namespace Star;

[[nodiscard]]
static std::expected<int, StarException> packAssets(int argc, char** argv) {
  Logger logger;
  auto startTime = std::chrono::steady_clock::now();

  auto parser = VersionOptionParser::create();
  if (!parser)
    return std::unexpected(std::move(parser.error()));
  auto& optParse = *parser;
  optParse.setVersionName("Asset Packer");
  optParse.setSummary("Packs asset folder into a starbound .pak file");
  auto configParameter = optParse.addParameter("c", "configFile", OptionParser::Optional, "JSON file with ignore lists and ordering info");
  if (!configParameter)
    return std::unexpected(std::move(configParameter.error()));
  auto serverSwitch = optParse.addSwitch("s", "Enable server mode");
  if (!serverSwitch)
    return std::unexpected(std::move(serverSwitch.error()));
  auto verboseSwitch = optParse.addSwitch("v", "Verbose, list each file added");
  if (!verboseSwitch)
    return std::unexpected(std::move(verboseSwitch.error()));
  optParse.addArgument("assets folder path", OptionParser::Required, "Path to the assets to be packed");
  optParse.addArgument("output filename", OptionParser::Required, "Output pak file");

  auto parsed = optParse.commandParseOrDie(argc, argv);
  if (!parsed)
    return std::unexpected(std::move(parsed.error()));
  auto& opts = *parsed;
  auto assetsArgument = opts.arguments.at(0);
  if (!assetsArgument)
    return std::unexpected(std::move(assetsArgument.error()));
  String assetsFolderPath = assetsArgument->get();
  auto outputArgument = opts.arguments.at(1);
  if (!outputArgument)
    return std::unexpected(std::move(outputArgument.error()));
  String outputFilename = outputArgument->get();

  StringList ignoreFiles;
  StringList extensionOrdering;
  if (opts.parameters.contains("c")) {
    auto configValues = opts.parameters.get("c");
    if (!configValues)
      return std::unexpected(std::move(configValues.error()));
    auto configArgument = configValues->get().first();
    if (!configArgument)
      return std::unexpected(std::move(configArgument.error()));
    String configFile = configArgument->get();
    auto configFileContents = File::readFileString(configFile);
    if (!configFileContents) {
      if (!configFileContents.error().is<IOException>())
        return std::unexpected(std::move(configFileContents.error()));
      cerrf("Could not open specified configFile: {}\n", configFile);
      cerrf("For the following reason: {}\n", outputException(ExceptionDiagnostic(configFileContents.error()), false));
      return 1;
    }

    auto configFileJson = Json::parseJson(*configFileContents);
    if (!configFileJson) {
      if (!configFileJson.error().is<JsonParsingException>())
        return std::unexpected(std::move(configFileJson.error()));
      cerrf("Could not parse the specified configFile: {}\n", configFile);
      cerrf("For the following reason: {}\n", outputException(ExceptionDiagnostic(configFileJson.error()), false));
      return 1;
    }

    auto readConfig = [&]() -> std::expected<void, StarException> {
      auto globalIgnore = configFileJson->get("globalIgnore", JsonArray());
      if (!globalIgnore)
        return std::unexpected(std::move(globalIgnore.error()));
      auto globalPatterns = jsonToStringList(*globalIgnore);
      if (!globalPatterns)
        return std::unexpected(std::move(globalPatterns.error()));
      ignoreFiles = std::move(*globalPatterns);
      if (opts.switches.contains("s")) {
        auto serverIgnore = configFileJson->get("serverIgnore", JsonArray());
        if (!serverIgnore)
          return std::unexpected(std::move(serverIgnore.error()));
        auto serverPatterns = jsonToStringList(*serverIgnore);
        if (!serverPatterns)
          return std::unexpected(std::move(serverPatterns.error()));
        ignoreFiles.appendAll(std::move(*serverPatterns));
      }
      auto ordering = configFileJson->get("extensionOrdering", JsonArray());
      if (!ordering)
        return std::unexpected(std::move(ordering.error()));
      auto extensions = jsonToStringList(*ordering);
      if (!extensions)
        return std::unexpected(std::move(extensions.error()));
      extensionOrdering = std::move(*extensions);
      return {};
    };
    auto configRead = readConfig();
    if (!configRead) {
      if (!configRead.error().is<JsonException>())
        return std::unexpected(std::move(configRead.error()));
      cerrf("Could not read the asset_packer config file {}\n", configFile);
      cerrf("{}\n", outputException(ExceptionDiagnostic(configRead.error()), false));
      return 1;
    }
  }

  bool verbose = opts.switches.contains("v");
  auto progressCallback = [verbose](std::size_t, std::size_t, String filePath, String assetPath) {
    if (verbose)
      coutf("Adding file '{}' to the target pak as '{}'\n", filePath, assetPath);
  };

  auto outputDirectory = File::fullPath(File::dirName(outputFilename));
  if (!outputDirectory)
    return std::unexpected(std::move(outputDirectory.error()));
  outputFilename = File::relativeTo(*outputDirectory, File::baseName(outputFilename));
  auto directorySource = DirectoryAssetSource::create(logger, assetsFolderPath, ignoreFiles);
  if (!directorySource)
    return std::unexpected(std::move(directorySource.error()));
  auto built = PackedAssetSource::build(**directorySource, outputFilename, extensionOrdering, progressCallback);
  if (!built)
    return std::unexpected(std::move(built.error()));

  coutf("Output packed assets to {} in {}s\n", outputFilename, std::chrono::duration<double>(std::chrono::steady_clock::now() - startTime).count());
  return 0;
}

int main(int argc, char** argv) {
  auto nativeRuntime = NativeRuntime::create();
  if (!nativeRuntime) {
    cerrf("Exception caught: {}\n", outputException(ExceptionDiagnostic(nativeRuntime.error()), true));
    return 1;
  }
  auto result = packAssets(argc, argv);
  if (!result) {
    cerrf("Exception caught: {}\n", outputException(ExceptionDiagnostic(result.error()), true));
    return 1;
  }
  return *result;
}

import star.json;
import star.json_extra;
import star.string;
import star.ordered_map;
import star.ordered_set;
import std;
import star.file;
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
import star.lexical_cast;

import star.option_parser;
import star.version_option_parser;
import star.asset_source;
import star.assets;
import star.root_base;
import star.configuration;
import star.root;
import star.root_loader;

namespace Star {

std::expected<Json, StarException> const BaseAssetsSettings = Json::parseJson(R"JSON(
    {
      "assetTimeToLive" : 30,
      "referenceImageTimeToLive" : 300,
      "referenceImagePixelBudget" : 16777216,

      // In seconds, audio less than this long will be decompressed in memory.
      "audioDecompressLimit" : 4.0,

      "workerPoolSize" : 2,

      "pathIgnore" : [
        "/\\.",
        "/~",
        "thumbs\\.db$",
        "\\.bak$",
        "\\.tmp$",
        "\\.zip$",
        "\\.orig$",
        "\\.fail$",
        "\\.psd$",
        "\\.tmx$"
      ],

      "digestIgnore" : [
        "\\.ogg$",
        "\\.wav$",
        "\\.abc$"
      ]
    }
  )JSON");

std::expected<Json, StarException> const BaseDefaultConfiguration = Json::parseJson(R"JSON(
    {
      "configurationVersion" : {
        "basic" : 2
      },

      "gameServerPort" : 21025,
)JSON"
#ifdef STAR_SYSTEM_WINDOWS
                                                      R"JSON(
      "gameServerBind" : "*",
      "queryServerBind" : "*",
      "rconServerBind" : "*",
)JSON"
#else
                                                      R"JSON(
      "gameServerBind" : "::",
)JSON"
#endif
R"JSON(
      "serverUsers" : {},
      "allowAnonymousConnections" : true,

      "bannedUuids" : [],
      "bannedIPs" : [],

      "serverName" : "A Starbound Server",
      "maxPlayers" : 8,
      "maxTeamSize" : 4,
      "serverFidelity" : "automatic",

      "checkAssetsDigest" : false,

      "safeScripts" : true,
      "scriptRecursionLimit" : 100,
      "scriptInstructionLimit" : 10000000,
      "scriptProfilingEnabled" : false,
      "scriptInstructionMeasureInterval" : 10000,

      "allowAdminCommands" : true,
      "allowAdminCommandsFromAnyone" : false,
      "anonymousConnectionsAreAdmin" : false,
      "connectionSettings" : {
        "compression" : "Zstd"
      },

      "clientP2PJoinable" : true,
      "clientIPJoinable" : false,

      "clearUniverseFiles" : false,
      "clearPlayerFiles" : false,
      "playerBackupFileCount" : 3,

      "tutorialMessages" : true,

      "interactiveHighlight" : true,

      "monochromeLighting" : false,

      "safe" : {
        "alwaysAllowClipboard" : false,
        "enableImGui" : false,
        "luaHttp" : {
          "enabled" : false,
          "trustedSites" : []
        }
      },

      "crafting" : {
        "filterHaveMaterials" : false
      },

      "inventory" : {
        "pickupToActionBar" : true
      },

      "discord" : {
        "activityDetails" : "<playerName> | <worldName>"
      }
    }
  )JSON");

RootLoader::RootLoader(Defaults defaults) : m_defaults(std::move(defaults)) {}

std::expected<RootLoader, StarException> RootLoader::create(Defaults defaults) {
  RootLoader loader(std::move(defaults));
  auto base = loader.initialize();
  if (!base) return std::unexpected(std::move(base.error()));
  auto boot = loader.addParameter("bootconfig", "bootconfig", Optional,
    "Boot time configuration file, defaults to sbinit.config");
  if (!boot) return std::unexpected(std::move(boot.error()));
  auto logfile = loader.addParameter("logfile", "logfile", Optional,
    strf("Log to the given logfile relative to the root directory, defaults to {}",
      loader.m_defaults.logFile ? *loader.m_defaults.logFile : "no log file"));
  if (!logfile) return std::unexpected(std::move(logfile.error()));
  auto levelName = enumName<LogLevelNames>(loader.m_defaults.logLevel);
  if (!levelName) return std::unexpected(std::move(levelName.error()));
  auto loglevel = loader.addParameter("loglevel", "level", Optional,
    strf("Sets the logging level (debug|info|warn|error), defaults to {}", *levelName));
  if (!loglevel) return std::unexpected(std::move(loglevel.error()));
  auto quiet = loader.addSwitch("quiet", strf("Do not log to stdout, defaults to {}", loader.m_defaults.quiet));
  if (!quiet) return std::unexpected(std::move(quiet.error()));
  auto verbose = loader.addSwitch("verbose", strf("Log to stdout, defaults to {}", !loader.m_defaults.quiet));
  if (!verbose) return std::unexpected(std::move(verbose.error()));
  auto runtime = loader.addSwitch("runtimeconfig",
    strf("Sets the path to the runtime configuration storage file relative to root directory, defauts to {}",
      loader.m_defaults.runtimeConfigFile ? *loader.m_defaults.runtimeConfigFile : "no storage file"));
  if (!runtime) return std::unexpected(std::move(runtime.error()));
  return loader;
}

std::expected<std::pair<Root::Settings, RootLoader::Options>, StarException> RootLoader::parseOrDie(
    StringList const& cmdLineArguments) const {
  auto options = VersionOptionParser::parseOrDie(cmdLineArguments);
  if (!options) return std::unexpected(std::move(options.error()));
  auto settings = rootSettingsForOptions(*options);
  if (!settings) return std::unexpected(std::move(settings.error()));
  return std::pair{std::move(*settings), std::move(*options)};
}

std::expected<std::pair<std::unique_ptr<Root>, RootLoader::Options>, StarException> RootLoader::initOrDie(StringList const& cmdLineArguments) const {
  auto parsed = parseOrDie(cmdLineArguments);
  if (!parsed) return std::unexpected(std::move(parsed.error()));
  auto root = Root::create(std::move(parsed->first));
  if (!root) return std::unexpected(std::move(root.error()));
  return std::pair{std::move(*root), std::move(parsed->second)};
}

std::expected<std::pair<Root::Settings, RootLoader::Options>, StarException> RootLoader::commandParseOrDie(int argc, char** argv) {
  auto options = VersionOptionParser::commandParseOrDie(argc, argv);
  if (!options) return std::unexpected(std::move(options.error()));
  auto settings = rootSettingsForOptions(*options);
  if (!settings) return std::unexpected(std::move(settings.error()));
  return std::pair{std::move(*settings), std::move(*options)};
}

std::expected<std::pair<std::unique_ptr<Root>, RootLoader::Options>, StarException> RootLoader::commandInitOrDie(int argc, char** argv) {
  auto parsed = commandParseOrDie(argc, argv);
  if (!parsed) return std::unexpected(std::move(parsed.error()));
  auto root = Root::create(std::move(parsed->first));
  if (!root) return std::unexpected(std::move(root.error()));
  return std::pair{std::move(*root), std::move(parsed->second)};
}

std::expected<Root::Settings, StarException> RootLoader::rootSettingsForOptions(Options const& options) const {
  auto loadSettings = [&]() -> std::expected<Root::Settings, StarException> {
    if (!BaseAssetsSettings) return std::unexpected(BaseAssetsSettings.error());
    if (!BaseDefaultConfiguration) return std::unexpected(BaseDefaultConfiguration.error());
    String bootConfigFile = options.parameters.value("bootconfig").front_value().value_or("sbinit.config");
    auto contents = File::read_file_string(bootConfigFile);
    if (!contents) return std::unexpected(std::move(contents.error()));
    auto boot = Json::parseJson(*contents);
    if (!boot) return std::unexpected(std::move(boot.error()));
    auto extraAssets = boot->get("assetsSettings", {});
    if (!extraAssets) return std::unexpected(std::move(extraAssets.error()));
    Json assetsSettings = jsonMerge(*BaseAssetsSettings, m_defaults.additionalAssetsSettings, *extraAssets);

    Root::Settings rootSettings;
    auto ttl = assetsSettings.getInt("assetTimeToLive");
    if (!ttl) return std::unexpected(std::move(ttl.error()));
    rootSettings.assetsSettings.assetTimeToLive = *ttl;
    auto referenceTtl = assetsSettings.getFloat("referenceImageTimeToLive");
    if (!referenceTtl)
      return std::unexpected(std::move(referenceTtl.error()));
    rootSettings.assetsSettings.referenceImageTimeToLive = *referenceTtl;
    auto referenceBudget = assetsSettings.getUInt("referenceImagePixelBudget");
    if (!referenceBudget)
      return std::unexpected(std::move(referenceBudget.error()));
    rootSettings.assetsSettings.referenceImagePixelBudget = *referenceBudget;
    auto decompress = assetsSettings.getFloat("audioDecompressLimit");
    if (!decompress) return std::unexpected(std::move(decompress.error()));
    rootSettings.assetsSettings.audioDecompressLimit = *decompress;
    auto poolSize = assetsSettings.getUInt("workerPoolSize");
    if (!poolSize) return std::unexpected(std::move(poolSize.error()));
    rootSettings.assetsSettings.workerPoolSize = *poolSize;
    auto missingImage = assetsSettings.optString("missingImage");
    if (!missingImage) return std::unexpected(std::move(missingImage.error()));
    rootSettings.assetsSettings.missingImage = std::move(*missingImage);
    auto missingAudio = assetsSettings.optString("missingAudio");
    if (!missingAudio) return std::unexpected(std::move(missingAudio.error()));
    rootSettings.assetsSettings.missingAudio = std::move(*missingAudio);
    auto pathIgnore = assetsSettings.get("pathIgnore");
    if (!pathIgnore) return std::unexpected(std::move(pathIgnore.error()));
    auto paths = jsonToStringList(*pathIgnore);
    if (!paths) return std::unexpected(std::move(paths.error()));
    rootSettings.assetsSettings.pathIgnore = std::move(*paths);
    auto digestIgnore = assetsSettings.get("digestIgnore");
    if (!digestIgnore) return std::unexpected(std::move(digestIgnore.error()));
    auto digests = jsonToStringList(*digestIgnore);
    if (!digests) return std::unexpected(std::move(digests.error()));
    rootSettings.assetsSettings.digestIgnore = std::move(*digests);
    auto directories = boot->get("assetDirectories", JsonArray());
    if (!directories) return std::unexpected(std::move(directories.error()));
    auto directoryList = jsonToStringList(*directories);
    if (!directoryList) return std::unexpected(std::move(directoryList.error()));
    rootSettings.assetDirectories = std::move(*directoryList);
    auto sources = boot->get("assetSources", JsonArray());
    if (!sources) return std::unexpected(std::move(sources.error()));
    auto sourceList = jsonToStringList(*sources);
    if (!sourceList) return std::unexpected(std::move(sourceList.error()));
    rootSettings.assetSources = std::move(*sourceList);
    auto extraDefaults = boot->get("defaultConfiguration", {});
    if (!extraDefaults) return std::unexpected(std::move(extraDefaults.error()));
    rootSettings.defaultConfiguration = jsonMerge(*BaseDefaultConfiguration, m_defaults.additionalDefaultConfiguration, *extraDefaults);
    auto storage = boot->getString("storageDirectory");
    if (!storage) return std::unexpected(std::move(storage.error()));
    rootSettings.storageDirectory = std::move(*storage);
    auto logDirectory = boot->optString("logDirectory");
    if (!logDirectory) return std::unexpected(std::move(logDirectory.error()));
    rootSettings.logDirectory = std::move(*logDirectory);
    rootSettings.logFile = options.parameters.value("logfile").front_value();
    if (!rootSettings.logFile)
      rootSettings.logFile = m_defaults.logFile;
    auto backups = boot->getUInt("logFileBackups", 10);
    if (!backups) return std::unexpected(std::move(backups.error()));
    rootSettings.logFileBackups = *backups;
    auto ugc = boot->getBool("includeUGC", true);
    if (!ugc) return std::unexpected(std::move(ugc.error()));
    rootSettings.includeUGC = *ugc;

    if (auto level = options.parameters.value("loglevel").front_value()) {
      auto parsed = enumValue<LogLevelNames>(*level);
      if (!parsed) return std::unexpected(std::move(parsed.error()));
      rootSettings.logLevel = *parsed;
    } else {
      rootSettings.logLevel = m_defaults.logLevel;
    }
    if (options.switches.contains("quiet"))
      rootSettings.quiet = true;
    else if (options.switches.contains("verbose"))
      rootSettings.quiet = false;
    else
      rootSettings.quiet = m_defaults.quiet;
    if (auto runtime = options.parameters.value("runtimeconfig").front_value())
      rootSettings.runtimeConfigFile = *runtime;
    else
      rootSettings.runtimeConfigFile = m_defaults.runtimeConfigFile;
    return rootSettings;
  };
  auto settings = loadSettings();
  if (!settings)
    return std::unexpected(settings.error().withContext("Could not perform initial Root load"));
  return settings;
}

}

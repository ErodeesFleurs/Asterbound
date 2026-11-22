import star.json;
import star.json_extra;
import star.string;
import star.ordered_map;
import star.ordered_set;
import std;
import star.file;
import star.format;
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

std::expected<Json, StarException> const base_assets_settings = Json::parse_json(R"JSON(
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

std::expected<Json, StarException> const base_default_configuration = Json::parse_json(R"JSON(
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
        "allowRemoteAssets" : true,
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
  auto boot = loader.add_parameter("bootconfig", "bootconfig", Optional,
    "Boot time configuration file, defaults to sbinit.config");
  if (!boot) return std::unexpected(std::move(boot.error()));
  auto logfile = loader.add_parameter("logfile", "logfile", Optional,
    Star::format("Log to the given logfile relative to the root directory, defaults to {}",
      loader.m_defaults.log_file ? *loader.m_defaults.log_file : "no log file"));
  if (!logfile) return std::unexpected(std::move(logfile.error()));
  auto level_name = enum_name<log_level_names>(loader.m_defaults.log_level);
  if (!level_name) return std::unexpected(std::move(level_name.error()));
  auto loglevel = loader.add_parameter("loglevel", "level", Optional,
    Star::format("Sets the logging level (debug|info|warn|error), defaults to {}", *level_name));
  if (!loglevel) return std::unexpected(std::move(loglevel.error()));
  auto quiet = loader.add_switch("quiet", Star::format("Do not log to stdout, defaults to {}", loader.m_defaults.quiet));
  if (!quiet) return std::unexpected(std::move(quiet.error()));
  auto verbose = loader.add_switch("verbose", Star::format("Log to stdout, defaults to {}", !loader.m_defaults.quiet));
  if (!verbose) return std::unexpected(std::move(verbose.error()));
  auto runtime = loader.add_switch("runtimeconfig",
    Star::format("Sets the path to the runtime configuration storage file relative to root directory, defauts to {}",
      loader.m_defaults.runtime_config_file ? *loader.m_defaults.runtime_config_file : "no storage file"));
  if (!runtime) return std::unexpected(std::move(runtime.error()));
  return loader;
}

std::expected<std::pair<Root::Settings, RootLoader::Options>, StarException> RootLoader::parse_or_die(
    StringList const& cmd_line_arguments) const {
  auto options = VersionOptionParser::parse_or_die(cmd_line_arguments);
  if (!options) return std::unexpected(std::move(options.error()));
  auto settings = root_settings_for_options(*options);
  if (!settings) return std::unexpected(std::move(settings.error()));
  return std::pair{std::move(*settings), std::move(*options)};
}

std::expected<std::pair<std::unique_ptr<Root>, RootLoader::Options>, StarException> RootLoader::init_or_die(StringList const& cmd_line_arguments) const {
  auto parsed = parse_or_die(cmd_line_arguments);
  if (!parsed) return std::unexpected(std::move(parsed.error()));
  auto root = Root::create(std::move(parsed->first));
  if (!root) return std::unexpected(std::move(root.error()));
  return std::pair{std::move(*root), std::move(parsed->second)};
}

std::expected<std::pair<Root::Settings, RootLoader::Options>, StarException> RootLoader::command_parse_or_die(std::span<char const* const> argv) {
  auto options = VersionOptionParser::command_parse_or_die(argv);
  if (!options) return std::unexpected(std::move(options.error()));
  auto settings = root_settings_for_options(*options);
  if (!settings) return std::unexpected(std::move(settings.error()));
  return std::pair{std::move(*settings), std::move(*options)};
}

std::expected<std::pair<std::unique_ptr<Root>, RootLoader::Options>, StarException> RootLoader::command_init_or_die(std::span<char const* const> argv) {
  auto parsed = command_parse_or_die(argv);
  if (!parsed) return std::unexpected(std::move(parsed.error()));
  auto root = Root::create(std::move(parsed->first));
  if (!root) return std::unexpected(std::move(root.error()));
  return std::pair{std::move(*root), std::move(parsed->second)};
}

std::expected<Root::Settings, StarException> RootLoader::root_settings_for_options(Options const& options) const {
  auto load_settings = [&]() -> std::expected<Root::Settings, StarException> {
    if (!base_assets_settings) return std::unexpected(base_assets_settings.error());
    if (!base_default_configuration) return std::unexpected(base_default_configuration.error());
    auto boot_config_file = options.first_parameter("bootconfig");
    auto contents = boot_config_file ? File::read_file_string(boot_config_file->get()) : File::read_file_string("sbinit.config");
    if (!contents) return std::unexpected(std::move(contents.error()));
    auto boot = Json::parse_json(*contents);
    if (!boot) return std::unexpected(std::move(boot.error()));
    auto extra_assets = boot->get("assetsSettings", {});
    if (!extra_assets) return std::unexpected(std::move(extra_assets.error()));
    Json assets_settings = json_merge(*base_assets_settings, m_defaults.additional_assets_settings, *extra_assets);

    Root::Settings root_settings;
    auto ttl = assets_settings.get_int("assetTimeToLive");
    if (!ttl) return std::unexpected(std::move(ttl.error()));
    root_settings.assetsSettings.asset_time_to_live = *ttl;
    auto reference_ttl = assets_settings.get_float("referenceImageTimeToLive");
    if (!reference_ttl)
      return std::unexpected(std::move(reference_ttl.error()));
    root_settings.assetsSettings.reference_image_time_to_live = *reference_ttl;
    auto reference_budget = assets_settings.get_uint("referenceImagePixelBudget");
    if (!reference_budget)
      return std::unexpected(std::move(reference_budget.error()));
    root_settings.assetsSettings.reference_image_pixel_budget = *reference_budget;
    auto decompress = assets_settings.get_float("audioDecompressLimit");
    if (!decompress) return std::unexpected(std::move(decompress.error()));
    root_settings.assetsSettings.audio_decompress_limit = *decompress;
    auto pool_size = assets_settings.get_uint("workerPoolSize");
    if (!pool_size) return std::unexpected(std::move(pool_size.error()));
    root_settings.assetsSettings.worker_pool_size = *pool_size;
    auto missing_image = assets_settings.opt_string("missingImage");
    if (!missing_image) return std::unexpected(std::move(missing_image.error()));
    root_settings.assetsSettings.missing_image = std::move(*missing_image);
    auto missing_audio = assets_settings.opt_string("missingAudio");
    if (!missing_audio) return std::unexpected(std::move(missing_audio.error()));
    root_settings.assetsSettings.missing_audio = std::move(*missing_audio);
    auto path_ignore = assets_settings.get("pathIgnore");
    if (!path_ignore) return std::unexpected(std::move(path_ignore.error()));
    auto paths = json_to_string_list(*path_ignore);
    if (!paths) return std::unexpected(std::move(paths.error()));
    root_settings.assetsSettings.path_ignore = std::move(*paths);
    auto digest_ignore = assets_settings.get("digestIgnore");
    if (!digest_ignore) return std::unexpected(std::move(digest_ignore.error()));
    auto digests = json_to_string_list(*digest_ignore);
    if (!digests) return std::unexpected(std::move(digests.error()));
    root_settings.assetsSettings.digest_ignore = std::move(*digests);
    auto directories = boot->get("assetDirectories", JsonArray());
    if (!directories) return std::unexpected(std::move(directories.error()));
    auto directory_list = json_to_string_list(*directories);
    if (!directory_list) return std::unexpected(std::move(directory_list.error()));
    root_settings.assetDirectories = std::move(*directory_list);
    auto sources = boot->get("assetSources", JsonArray());
    if (!sources) return std::unexpected(std::move(sources.error()));
    auto source_list = json_to_string_list(*sources);
    if (!source_list) return std::unexpected(std::move(source_list.error()));
    root_settings.assetSources = std::move(*source_list);
    auto extra_defaults = boot->get("defaultConfiguration", {});
    if (!extra_defaults) return std::unexpected(std::move(extra_defaults.error()));
    root_settings.defaultConfiguration = json_merge(*base_default_configuration, m_defaults.additional_default_configuration, *extra_defaults);
    auto storage = boot->get_string("storageDirectory");
    if (!storage) return std::unexpected(std::move(storage.error()));
    root_settings.storageDirectory = std::move(*storage);
    auto log_directory = boot->opt_string("logDirectory");
    if (!log_directory) return std::unexpected(std::move(log_directory.error()));
    root_settings.logDirectory = std::move(*log_directory);
    if (auto logfile = options.first_parameter("logfile"))
      root_settings.logFile = logfile->get();
    else
      root_settings.logFile = m_defaults.log_file;
    auto backups = boot->get_uint("logFileBackups", 10);
    if (!backups) return std::unexpected(std::move(backups.error()));
    root_settings.logFileBackups = *backups;
    auto log_size_limit = boot->get_uint("logFileSizeLimit", root_settings.logFileSizeLimit);
    if (!log_size_limit) return std::unexpected(std::move(log_size_limit.error()));
    root_settings.logFileSizeLimit = *log_size_limit;
    auto ugc = boot->get_bool("includeUGC", true);
    if (!ugc) return std::unexpected(std::move(ugc.error()));
    root_settings.includeUGC = *ugc;

    if (auto level = options.first_parameter("loglevel")) {
      auto parsed = enum_value<log_level_names>(level->get());
      if (!parsed) return std::unexpected(std::move(parsed.error()));
      root_settings.logLevel = *parsed;
    } else {
      root_settings.logLevel = m_defaults.log_level;
    }
    if (options.switches.contains("quiet"))
      root_settings.quiet = true;
    else if (options.switches.contains("verbose"))
      root_settings.quiet = false;
    else
      root_settings.quiet = m_defaults.quiet;
    if (auto runtime = options.first_parameter("runtimeconfig"))
      root_settings.runtimeConfigFile = runtime->get();
    else
      root_settings.runtimeConfigFile = m_defaults.runtime_config_file;
    return root_settings;
  };
  auto settings = load_settings();
  if (!settings)
    return std::unexpected(settings.error().with_context("Could not perform initial Root load"));
  return settings;
}

}

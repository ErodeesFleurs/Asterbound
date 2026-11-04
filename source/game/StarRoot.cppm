module;
import star.json;
import star.ordered_map;
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
import std;
import star.format;
import star.string;
import star.version;

import star.asset_source;
import star.assets;
import star.root_base;
import star.configuration;
import star.random;
import star.game_types;
import star.curve25519;
import star.songbook;
import star.worker_pool;

namespace Star {

class ItemDatabase;
class MaterialDatabase;
class ObjectDatabase;
class PlantDatabase;
class ProjectileDatabase;
class MonsterDatabase;
class NpcDatabase;
class StagehandDatabase;
class VehicleDatabase;
class PlayerFactory;
class EntityFactory;
class TerrainDatabase;
class BiomeDatabase;
class LiquidsDatabase;
class StatusEffectDatabase;
class DamageDatabase;
class ParticleDatabase;
class EffectSourceDatabase;
class FunctionDatabase;
class TreasureDatabase;
class DungeonDefinitions;
class TilesetDatabase;
class StatisticsDatabase;
class EmoteProcessor;
class SpeciesDatabase;
class ImageMetadataDatabase;
class VersioningDatabase;
class QuestTemplateDatabase;
class AiDatabase;
class TechDatabase;
class CodexDatabase;
class BehaviorDatabase;
class TenantDatabase;
class PatternedNameGenerator;
class DanceDatabase;
class SpawnTypeDatabase;
class RadioMessageDatabase;
class CollectionDatabase;

// Instance-owned configuration, assets, factories, and databases.
// Root and its resources are thread safe. Initialization must finish before
// dependent work starts; all borrowers and dependent threads must finish before
// the owning Root is destroyed.
class Root final : public RootBase {
public:
  struct Settings {
    Assets::Settings assetsSettings;

    // Asset sources are scanned for in the given directories, in order.
    StringList assetDirectories;

    // Just raw asset source paths.
    StringList assetSources;

    Json defaultConfiguration;

    // Top-level storage directory under which all game data is saved
    String storageDirectory;

    // Directory to store logs - if not set, uses storage directory and keeps old logs in seperate folder
    std::optional<String> logDirectory;

    // Name of the log file that should be written, if any, relative to the
    // log directory
    std::optional<String> logFile;

    // Number of rotated log file backups
    unsigned logFileBackups;

    // The minimum log level to write to any log sink
    LogLevel logLevel;

    // If true, doesn't write any logging to stdout, only to the log file if
    // given.
    bool quiet;

    // If true, loads UGC from platform services if available. True by default.
    bool includeUGC;

    // If given, will write changed configuration to the given file within the
    // storage directory.
    std::optional<String> runtimeConfigFile;
  };

  // Initializes the starbound root object and does the initial load.  All of
  // the Root members will be just in time loaded as they are accessed, unless
  // fullyLoad is called beforehand.
  [[nodiscard]] static std::expected<std::unique_ptr<Root>, StarException> create(Settings settings);

  Root(Root const&) = delete;
  Root& operator=(Root const&) = delete;

  ~Root() override;

  // Clears existing Root members, allowing them to be loaded fresh from disk.
  [[nodiscard]] std::expected<void, StarException> reload();

  // Reloads with the given mod sources applied on top of the base mod source
  // specified in the settings.  Mods in the base mod source will override mods
  // in the given mod sources
  [[nodiscard]] std::expected<void, StarException> loadMods(StringList modDirectories, bool _reload = true);

  // Ensures all Root members are loaded without waiting for them to be auto
  // loaded.
  [[nodiscard]] std::expected<void, StarException> fullyLoad();

  // Callbacks run synchronously on the reload/hotReload caller, not on the
  // maintenance thread. Retain the subscription; owners of raw-this callbacks
  // must disconnect before tearing down any state used by the callback.
  [[nodiscard]] ListenerSubscription registerReloadListener(std::weak_ptr<Listener> reloadListener);

  [[nodiscard]] std::expected<void, StarException> hotReload();

  // Translates the given path to be relative to the configured storage
  // location.
  String toStoragePath(String const& path) const;

  // All of the Root member accessors are safe to call at any time after Root
  // initialization, if they are not loaded they will load before returning.

  [[nodiscard]] std::expected<std::shared_ptr<Assets const>, StarException> assets() override;
  [[nodiscard]] std::expected<std::shared_ptr<Configuration>, StarException> configuration() override;

  [[nodiscard]] std::expected<std::shared_ptr<ObjectDatabase const>, StarException> objectDatabase();
  [[nodiscard]] std::expected<std::shared_ptr<PlantDatabase const>, StarException> plantDatabase();
  [[nodiscard]] std::expected<std::shared_ptr<ProjectileDatabase const>, StarException> projectileDatabase();
  [[nodiscard]] std::expected<std::shared_ptr<MonsterDatabase const>, StarException> monsterDatabase();
  [[nodiscard]] std::expected<std::shared_ptr<NpcDatabase const>, StarException> npcDatabase();
  [[nodiscard]] std::expected<std::shared_ptr<StagehandDatabase const>, StarException> stagehandDatabase();
  [[nodiscard]] std::expected<std::shared_ptr<VehicleDatabase const>, StarException> vehicleDatabase();
  [[nodiscard]] std::expected<std::shared_ptr<PlayerFactory const>, StarException> playerFactory();
  [[nodiscard]] std::expected<std::shared_ptr<EntityFactory const>, StarException> entityFactory();
  [[nodiscard]] std::expected<std::shared_ptr<PatternedNameGenerator const>, StarException> nameGenerator();
  [[nodiscard]] std::expected<std::shared_ptr<ItemDatabase const>, StarException> itemDatabase();
  [[nodiscard]] std::expected<std::shared_ptr<MaterialDatabase const>, StarException> materialDatabase();
  [[nodiscard]] std::expected<std::shared_ptr<TerrainDatabase const>, StarException> terrainDatabase();
  [[nodiscard]] std::expected<std::shared_ptr<BiomeDatabase const>, StarException> biomeDatabase();
  [[nodiscard]] std::expected<std::shared_ptr<LiquidsDatabase const>, StarException> liquidsDatabase();
  [[nodiscard]] std::expected<std::shared_ptr<StatusEffectDatabase const>, StarException> statusEffectDatabase();
  [[nodiscard]] std::expected<std::shared_ptr<DamageDatabase const>, StarException> damageDatabase();
  [[nodiscard]] std::expected<std::shared_ptr<ParticleDatabase const>, StarException> particleDatabase();
  [[nodiscard]] std::expected<std::shared_ptr<EffectSourceDatabase const>, StarException> effectSourceDatabase();
  [[nodiscard]] std::expected<std::shared_ptr<FunctionDatabase const>, StarException> functionDatabase();
  [[nodiscard]] std::expected<std::shared_ptr<TreasureDatabase const>, StarException> treasureDatabase();
  [[nodiscard]] std::expected<std::shared_ptr<DungeonDefinitions const>, StarException> dungeonDefinitions();
  [[nodiscard]] std::expected<std::shared_ptr<TilesetDatabase const>, StarException> tilesetDatabase();
  [[nodiscard]] std::expected<std::shared_ptr<StatisticsDatabase const>, StarException> statisticsDatabase();
  [[nodiscard]] std::expected<std::shared_ptr<EmoteProcessor const>, StarException> emoteProcessor();
  [[nodiscard]] std::expected<std::shared_ptr<SpeciesDatabase const>, StarException> speciesDatabase();
  [[nodiscard]] std::expected<std::shared_ptr<ImageMetadataDatabase const>, StarException> imageMetadataDatabase();
  [[nodiscard]] std::expected<std::shared_ptr<VersioningDatabase const>, StarException> versioningDatabase();
  [[nodiscard]] std::expected<std::shared_ptr<QuestTemplateDatabase const>, StarException> questTemplateDatabase();
  [[nodiscard]] std::expected<std::shared_ptr<AiDatabase const>, StarException> aiDatabase();
  [[nodiscard]] std::expected<std::shared_ptr<TechDatabase const>, StarException> techDatabase();
  [[nodiscard]] std::expected<std::shared_ptr<CodexDatabase const>, StarException> codexDatabase();
  [[nodiscard]] std::expected<std::shared_ptr<BehaviorDatabase const>, StarException> behaviorDatabase();
  [[nodiscard]] std::expected<std::shared_ptr<TenantDatabase const>, StarException> tenantDatabase();
  [[nodiscard]] std::expected<std::shared_ptr<DanceDatabase const>, StarException> danceDatabase();
  [[nodiscard]] std::expected<std::shared_ptr<SpawnTypeDatabase const>, StarException> spawnTypeDatabase();
  [[nodiscard]] std::expected<std::shared_ptr<RadioMessageDatabase const>, StarException> radioMessageDatabase();
  [[nodiscard]] std::expected<std::shared_ptr<CollectionDatabase const>, StarException> collectionDatabase();

  Settings& settings();

  Logger& logger() { return m_logger; }
  LogMap& logMap() { return m_logMap; }
  SpatialLogger& spatialLogger() { return m_spatialLogger; }
  RandomService& random() { return m_random; }
  // Resource-cache jitter and presentation never consume gameplay randomness.
  RandomService& presentationRandom() { return m_presentationRandom; }
  GameTiming& timing() { return m_timing; }
  Curve25519::KeySet const& identity() const { return m_identity; }
  SongbookSynchronization& songbookSynchronization() { return m_songbookSynchronization; }
  [[nodiscard]] std::expected<std::reference_wrapper<WorkerPool>, StarException> workers();
  [[nodiscard]] std::expected<std::reference_wrapper<bool>, StarException> humanoidHeadRotation();

private:
  explicit Root(Settings settings);
  [[nodiscard]] std::expected<void, StarException> initialize();
  [[nodiscard]] std::expected<StringList, StarException> scanForAssetSources(StringList const& directories, StringList const& manual = {});
  template <typename T, typename... Params>
  [[nodiscard]] std::expected<std::shared_ptr<T>, StarException> loadMember(std::shared_ptr<T>& ptr, std::mutex& mutex, std::string_view name, Params&&... params);
  template <typename T>
  [[nodiscard]] std::expected<std::shared_ptr<T>, StarException> loadMemberFunction(std::shared_ptr<T>& ptr, std::mutex& mutex, std::string_view name, std::function<std::expected<std::shared_ptr<T>, StarException>()> loadFunction);

  // m_configurationMutex must be held when calling.
  [[nodiscard]] std::expected<void, StarException> writeConfig();

  // Services outlive the resource catalogs and all work borrowing them.
  Logger m_logger;
  LogMap m_logMap;
  SpatialLogger m_spatialLogger;
  RandomService m_random;
  RandomService m_presentationRandom{0};
  GameTiming m_timing;
  Curve25519::KeySet m_identity;
  SongbookSynchronization m_songbookSynchronization;
  WorkerPool m_workers;
  std::mutex m_workersStartMutex;
  bool m_workersStarted = false;
  std::mutex m_headRotationMutex;
  bool m_headRotationInitialized = false;
  bool m_humanoidHeadRotation = true;

  Settings m_settings;

  std::mutex m_modsMutex;
  StringList m_modDirectories;

  ListenerGroup m_reloadListeners;

  Json m_lastRuntimeConfig;
  std::optional<String> m_runtimeConfigFile;

  ThreadFunction<void> m_maintenanceThread;
  std::mutex m_maintenanceStopMutex;
  std::condition_variable m_maintenanceStopCondition;
  bool m_stopMaintenanceThread = false;

  std::shared_ptr<Assets> m_assets;
  std::mutex m_assetsMutex;

  std::shared_ptr<Configuration> m_configuration;
  std::mutex m_configurationMutex;

  std::shared_ptr<ObjectDatabase> m_objectDatabase;
  std::mutex m_objectDatabaseMutex;

  std::shared_ptr<PlantDatabase> m_plantDatabase;
  std::mutex m_plantDatabaseMutex;

  std::shared_ptr<ProjectileDatabase> m_projectileDatabase;
  std::mutex m_projectileDatabaseMutex;

  std::shared_ptr<MonsterDatabase> m_monsterDatabase;
  std::mutex m_monsterDatabaseMutex;

  std::shared_ptr<NpcDatabase> m_npcDatabase;
  std::mutex m_npcDatabaseMutex;

  std::shared_ptr<StagehandDatabase> m_stagehandDatabase;
  std::mutex m_stagehandDatabaseMutex;

  std::shared_ptr<VehicleDatabase> m_vehicleDatabase;
  std::mutex m_vehicleDatabaseMutex;

  std::shared_ptr<PlayerFactory> m_playerFactory;
  std::mutex m_playerFactoryMutex;

  std::shared_ptr<EntityFactory> m_entityFactory;
  std::mutex m_entityFactoryMutex;

  std::shared_ptr<PatternedNameGenerator> m_nameGenerator;
  std::mutex m_nameGeneratorMutex;

  std::shared_ptr<ItemDatabase> m_itemDatabase;
  std::mutex m_itemDatabaseMutex;

  std::shared_ptr<MaterialDatabase> m_materialDatabase;
  std::mutex m_materialDatabaseMutex;

  std::shared_ptr<TerrainDatabase> m_terrainDatabase;
  std::mutex m_terrainDatabaseMutex;

  std::shared_ptr<BiomeDatabase> m_biomeDatabase;
  std::mutex m_biomeDatabaseMutex;

  std::shared_ptr<LiquidsDatabase> m_liquidsDatabase;
  std::mutex m_liquidsDatabaseMutex;

  std::shared_ptr<StatusEffectDatabase> m_statusEffectDatabase;
  std::mutex m_statusEffectDatabaseMutex;

  std::shared_ptr<DamageDatabase> m_damageDatabase;
  std::mutex m_damageDatabaseMutex;

  std::shared_ptr<ParticleDatabase> m_particleDatabase;
  std::mutex m_particleDatabaseMutex;

  std::shared_ptr<EffectSourceDatabase> m_effectSourceDatabase;
  std::mutex m_effectSourceDatabaseMutex;

  std::shared_ptr<FunctionDatabase> m_functionDatabase;
  std::mutex m_functionDatabaseMutex;

  std::shared_ptr<TreasureDatabase> m_treasureDatabase;
  std::mutex m_treasureDatabaseMutex;

  std::shared_ptr<DungeonDefinitions> m_dungeonDefinitions;
  std::mutex m_dungeonDefinitionsMutex;

  std::shared_ptr<TilesetDatabase> m_tilesetDatabase;
  std::mutex m_tilesetDatabaseMutex;

  std::shared_ptr<StatisticsDatabase> m_statisticsDatabase;
  std::mutex m_statisticsDatabaseMutex;

  std::shared_ptr<EmoteProcessor> m_emoteProcessor;
  std::mutex m_emoteProcessorMutex;

  std::shared_ptr<SpeciesDatabase> m_speciesDatabase;
  std::mutex m_speciesDatabaseMutex;

  std::shared_ptr<ImageMetadataDatabase> m_imageMetadataDatabase;
  std::mutex m_imageMetadataDatabaseMutex;

  std::shared_ptr<VersioningDatabase> m_versioningDatabase;
  std::mutex m_versioningDatabaseMutex;

  std::shared_ptr<QuestTemplateDatabase> m_questTemplateDatabase;
  std::mutex m_questTemplateDatabaseMutex;

  std::shared_ptr<AiDatabase> m_aiDatabase;
  std::mutex m_aiDatabaseMutex;

  std::shared_ptr<TechDatabase> m_techDatabase;
  std::mutex m_techDatabaseMutex;

  std::shared_ptr<CodexDatabase> m_codexDatabase;
  std::mutex m_codexDatabaseMutex;

  std::shared_ptr<BehaviorDatabase> m_behaviorDatabase;
  std::mutex m_behaviorDatabaseMutex;

  std::shared_ptr<TenantDatabase> m_tenantDatabase;
  std::mutex m_tenantDatabaseMutex;

  std::shared_ptr<DanceDatabase> m_danceDatabase;
  std::mutex m_danceDatabaseMutex;

  std::shared_ptr<SpawnTypeDatabase> m_spawnTypeDatabase;
  std::mutex m_spawnTypeDatabaseMutex;

  std::shared_ptr<RadioMessageDatabase> m_radioMessageDatabase;
  std::mutex m_radioMessageDatabaseMutex;

  std::shared_ptr<CollectionDatabase> m_collectionDatabase;
  std::mutex m_collectionDatabaseMutex;
};

}

export module star.root;

export namespace Star {
  using ::Star::ItemDatabase;
  using ::Star::MaterialDatabase;
  using ::Star::ObjectDatabase;
  using ::Star::PlantDatabase;
  using ::Star::ProjectileDatabase;
  using ::Star::MonsterDatabase;
  using ::Star::NpcDatabase;
  using ::Star::StagehandDatabase;
  using ::Star::VehicleDatabase;
  using ::Star::PlayerFactory;
  using ::Star::EntityFactory;
  using ::Star::TerrainDatabase;
  using ::Star::BiomeDatabase;
  using ::Star::LiquidsDatabase;
  using ::Star::StatusEffectDatabase;
  using ::Star::DamageDatabase;
  using ::Star::ParticleDatabase;
  using ::Star::EffectSourceDatabase;
  using ::Star::FunctionDatabase;
  using ::Star::TreasureDatabase;
  using ::Star::DungeonDefinitions;
  using ::Star::TilesetDatabase;
  using ::Star::StatisticsDatabase;
  using ::Star::EmoteProcessor;
  using ::Star::SpeciesDatabase;
  using ::Star::ImageMetadataDatabase;
  using ::Star::VersioningDatabase;
  using ::Star::QuestTemplateDatabase;
  using ::Star::AiDatabase;
  using ::Star::TechDatabase;
  using ::Star::CodexDatabase;
  using ::Star::BehaviorDatabase;
  using ::Star::TenantDatabase;
  using ::Star::PatternedNameGenerator;
  using ::Star::DanceDatabase;
  using ::Star::SpawnTypeDatabase;
  using ::Star::RadioMessageDatabase;
  using ::Star::CollectionDatabase;
  using ::Star::Root;
}

import star.json;
import star.json_extra;
import star.id_map;
import star.ordered_map;
import star.lru_cache;
import star.thread;
import star.random;
import star.ttl_cache;
import star.casting;
import star.game_types;
import star.strong_typedef;
import star.rpc_promise;
import star.bi_map;
import star.a_star;
import star.data_stream;
import star.list;
import star.image_processing;
import star.hash;
import star.string_view;
import star.directives;
import star.asset_path;
import star.file;
import star.string;
import star.byte_array;
import star.encode;
import std;
import star.format;
import star.output_proxy;
import star.version;
import star.parametric_function;
import star.multi_array_interpolator;
import star.multi_table;
import star.weighted_pool;
import star.logging;
import star.io_device;
import star.json_builder;
import star.set;
import star.color;
import star.ref_ptr;
import star.listener;
import star.lua;
import star.audio;
import star.map;
import star.periodic_function;
import star.periodic;
import star.ordered_set;
import star.either;
import star.image;

import star.lua_root;

import star.damage_types;
import star.world_geometry;
import star.status_types;
import star.damage;
import star.light_source;
import star.entity;
import star.tile_damage;
import star.interaction_types;
import star.celestial_coordinate;
import star.quest_descriptor;
import star.interactive_entity;
import star.tile_entity;
import star.tile_modification;
import star.force_regions;
import star.world;
import star.physics_entity;
import star.movement_controller;
import star.platformer_astar_types;
import star.anchorable_entity;

import star.game_timers;
import star.actor_movement_controller;
import star.drawable;
import star.item;
import star.item_descriptor;
import star.item_recipe;
import star.collision_block;
import star.assets;
import star.root_base;
import star.root;
import star.configuration;
import star.scripted_entity;
import star.status_effect_entity;
import star.animation;
import star.particle;
import star.lua_components;
import star.effect_emitter;
import star.projectile;
import star.mixer;
import star.entity_rendering_types;
import star.entity_rendering;
import star.animated_part_set;
import star.networked_animator;
import star.portrait_entity;
import star.damage_bar_entity;
import star.nametag_entity;
import star.aggressive_entity;
import star.chat_action;
import star.chatty_entity;
import star.lua_animation_component;
import star.lua_actor_movement_component;
import star.actor_entity;
import star.monster_database;
import star.monster;
import star.humanoid;
import star.emote_entity;
import star.lounging_entities;
import star.tool_user_entity;
import star.npc_database;
import star.npc;
import star.wiring;
import star.wire_entity;
import star.inspectable_entity;
import star.object;
import star.plant;
import star.mobile_entity;
import star.uuid;
import star.inventory_types;
import star.ai_types;
import star.player_types;
import star.player;
import star.item_drop;
import star.dungeon_generator;
import star.asset_source;
import star.directory_asset_source;
import star.worker_pool;
import star.vehicle;
import star.player_factory;
import star.entity_factory;
import star.stored_functions;
import star.treasure;
import star.name_generator;

import star.terrain_database;
import star.liquids_database;
import star.vehicle_database;
import star.tileset_database;
import star.species_database;
import star.projectile_database;
import star.ai_database;
import star.behavior_database;
import star.packed_asset_source;
import star.dance_database;
import star.tech_database;
import star.codex_database;
import star.collection_database;
import star.stagehand_database;
import star.emote_processor;
import star.damage_database;
import star.effect_source_database;
import star.particle_database;
import star.radio_message_database;
import star.status_effect_database;
import star.quest_template_database;
import star.spawn_type_database;
import star.tenant_database;
import star.statistics_database;
import star.versioning_database;
import star.item_database;
import star.material_database;
import star.biome_database;
import star.image_metadata_database;
import star.object_database;
import star.plant_database;


import star.plant_drop;

import star.algorithm;
namespace Star {

namespace {
  unsigned const RootMaintenanceSleep = 5000;
  unsigned const RootLoadThreads = 4;
}

Root::Root(Settings settings)
  : m_random(0), m_identity(m_random), m_workers("Root CPU"), m_settings(std::move(settings)) {}

std::expected<std::unique_ptr<Root>, StarException> Root::create(Settings settings) {
  auto root = std::unique_ptr<Root>(new Root(std::move(settings)));
  auto initialized = root->initialize();
  if (!initialized)
    return std::unexpected(std::move(initialized.error()));
  return root;
}

std::expected<void, StarException> Root::initialize() {
  auto random = m_random.init();
  if (!random)
    return std::unexpected(std::move(random.error()));
  if (m_settings.runtimeConfigFile)
    m_runtimeConfigFile = toStoragePath(*m_settings.runtimeConfigFile);

  auto storageExists = File::is_directory(m_settings.storageDirectory);
  if (!storageExists)
    return std::unexpected(std::move(storageExists.error()));
  if (!*storageExists) {
    auto made = File::create_directory(m_settings.storageDirectory);
    if (!made)
      return std::unexpected(std::move(made.error()));
  }

  if (m_settings.logFile) {
    String logFile =
      File::relative_to(m_settings.logDirectory.value_or(m_settings.storageDirectory), *m_settings.logFile);
    String oldLogDirectory = m_settings.logDirectory.value_or(File::relative_to(m_settings.storageDirectory, "logs"));
    auto logDirectoryExists = File::is_directory(oldLogDirectory);
    if (!logDirectoryExists)
      return std::unexpected(std::move(logDirectoryExists.error()));
    if (!*logDirectoryExists) {
      auto made = File::create_directory(oldLogDirectory);
      if (!made)
        return std::unexpected(std::move(made.error()));
    }

    auto backup = File::backup_file_in_sequence(logFile, File::relative_to(oldLogDirectory, *m_settings.logFile),
                                                m_settings.logFileBackups);
    if (!backup)
      return std::unexpected(std::move(backup.error()));
    auto sink = FileLogSink::create(logFile, m_settings.logLevel, true);
    if (!sink)
      return std::unexpected(std::move(sink.error()));
    m_logger.addSink(std::move(*sink));
  }
  m_logger.stdoutSink()->setLevel(m_settings.logLevel);

  if (m_settings.quiet)
    m_logger.removeStdoutSink();

  auto preparing = m_logger.info("Root: Preparing...");
  if (!preparing)
    return std::unexpected(std::move(preparing.error()));

  m_maintenanceStopSource = std::stop_source{};
  auto maintenance = Thread::invoke("Root::maintenanceMain", [this, stopToken = m_maintenanceStopSource.get_token()]() -> std::expected<void, StarException> {
      std::unique_lock locker(m_maintenanceStopMutex);
      while (!stopToken.stop_requested()) {
        m_reloadListeners.clearExpiredListeners();

        {
          std::unique_lock locker(m_objectDatabaseMutex);
          if (std::shared_ptr<ObjectDatabase> objectDb = m_objectDatabase) {
            locker.unlock();
            objectDb->cleanup();
          }
        }
        {
          std::unique_lock locker(m_itemDatabaseMutex);
          if (std::shared_ptr<ItemDatabase> itemDb = m_itemDatabase) {
            locker.unlock();
            itemDb->cleanup();
          }
        }
        {
          std::unique_lock locker(m_monsterDatabaseMutex);
          if (std::shared_ptr<MonsterDatabase> monsterDb = m_monsterDatabase) {
            locker.unlock();
            monsterDb->cleanup();
          }
        }
        {
          std::unique_lock locker(m_assetsMutex);
          if (std::shared_ptr<Assets> assets = m_assets) {
            locker.unlock();
            assets->cleanup();
          }
        }
        {
          std::unique_lock locker(m_tenantDatabaseMutex);
          if (std::shared_ptr<TenantDatabase> tenantDb = m_tenantDatabase) {
            locker.unlock();
            tenantDb->cleanup();
          }
        }
        {
          std::unique_lock locker(m_imageMetadataDatabaseMutex);
          if (std::shared_ptr<ImageMetadataDatabase> imgMetaDb = m_imageMetadataDatabase) {
            locker.unlock();
            imgMetaDb->cleanup();
          }
        }

        auto entropy = m_random.addEntropy();
        if (!entropy)
          return std::unexpected(std::move(entropy.error()));

        {
          std::lock_guard locker(m_configurationMutex);
          auto written = writeConfig();
          if (!written)
            return std::unexpected(std::move(written.error()));
        }

        m_maintenanceStopCondition.wait_for(locker, std::chrono::milliseconds(RootMaintenanceSleep));
      }
      return {};
    });
  if (!maintenance)
    return std::unexpected(std::move(maintenance.error()));
  m_maintenanceThread = std::move(*maintenance);

  return m_logger.info("Root: Done preparing Root.");
}

Root::~Root() {
  if (auto logged = m_logger.info("Root: Shutting down Root"); !logged)
    EmergencyLog::error("Root: Shutdown log failed: {}", outputException(logged.error(), true));

  {
    std::lock_guard locker(m_maintenanceStopMutex);
    m_maintenanceStopSource.request_stop();
    m_maintenanceStopCondition.notify_one();
  }
  if (m_maintenanceThread) {
    if (auto finished = m_maintenanceThread.finish(); !finished)
      EmergencyLog::error("Root: Maintenance failed during shutdown: {}", outputException(finished.error(), true));
  }
  if (auto finished = m_workers.finish(); !finished)
    EmergencyLog::error("Root: Worker shutdown failed: {}", outputException(finished.error(), true));

  m_reloadListeners.clearAllListeners();

  if (auto written = writeConfig(); !written)
    EmergencyLog::error("Root: Could not write configuration during shutdown: {}", outputException(written.error(), true));

}

std::expected<void, StarException> Root::reload() {
  auto logged = m_logger.info("Root: Reloading from disk");
  if (!logged)
    return std::unexpected(std::move(logged.error()));

  {
    // We need to lock all the mutexes to reset everything to cause it to be
    // reloaded, but whenever we lock individual members we should always do it
    // in the same order (well, the same order*ing* not necessarily the same
    // order) to avoid deadlocks.  This means that we need to enumerate the
    // finicky, implicit dependency order that we have due to each member's
    // constructor referencing root recursively.

    // Entity factory depends on all the entity databases and the versioning
    // database.
    std::lock_guard entityFactoryLock(m_entityFactoryMutex);

    // Species database depends on the item database.
    std::lock_guard speciesDatabaseLock(m_speciesDatabaseMutex);

    // Item database depends on object database and codex database
    std::lock_guard itemDatabaseLock(m_itemDatabaseMutex);

    // These databases depend on various things below, but not the item database
    std::lock_guard objectDatabaseLock(m_objectDatabaseMutex);
    std::lock_guard playerFactoryLock(m_playerFactoryMutex);
    std::lock_guard npcDatabaseLock(m_npcDatabaseMutex);
    std::lock_guard stagehandDatabaseLock(m_stagehandDatabaseMutex);
    std::lock_guard vehicleDatabaseLock(m_vehicleDatabaseMutex);
    std::lock_guard monsterDatabaseLock(m_monsterDatabaseMutex);
    std::lock_guard plantDatabaseLock(m_plantDatabaseMutex);
    std::lock_guard projectileDatabaseLock(m_projectileDatabaseMutex);

    // Biome database depends on liquids, materials, and stored function
    // databases.
    std::lock_guard biomeDatabaseLock(m_biomeDatabaseMutex);

    // Dungeon definitions database depends on the material and liquids database
    std::lock_guard dungeonDefinitionsLock(m_dungeonDefinitionsMutex);
    std::lock_guard tilesetDatabaseLock(m_tilesetDatabaseMutex);

    std::lock_guard statisticsDatabaseLock(m_statisticsDatabaseMutex);

    // Liquids database depends on the materials database
    std::lock_guard liquidsDatabaseLock(m_liquidsDatabaseMutex);

    // Material database depends on particle database
    std::lock_guard materialDatabaseLock(m_materialDatabaseMutex);

    // Databases that depend on functions database.
    std::lock_guard damageDatabaseLock(m_damageDatabaseMutex);
    std::lock_guard effectSourceDatabaseLock(m_effectSourceDatabaseMutex);
    std::lock_guard statusEffectDatabaseLock(m_statusEffectDatabaseMutex);
    std::lock_guard treasureDatabaseLock(m_treasureDatabaseMutex);

    // Databases that don't depend on anything other than assets
    std::lock_guard codexDatabaseLock(m_codexDatabaseMutex);
    std::lock_guard behaviorDatabaseMutex(m_behaviorDatabaseMutex);
    std::lock_guard techDatabaseLock(m_techDatabaseMutex);
    std::lock_guard aiDatabaseLock(m_aiDatabaseMutex);
    std::lock_guard questTemplateDatabaseLock(m_questTemplateDatabaseMutex);
    std::lock_guard emoteProcessorLock(m_emoteProcessorMutex);
    std::lock_guard terrainDatabaseLock(m_terrainDatabaseMutex);
    std::lock_guard particleDatabaseLock(m_particleDatabaseMutex);
    std::lock_guard versioningDatabaseLock(m_versioningDatabaseMutex);
    std::lock_guard functionDatabaseLock(m_functionDatabaseMutex);
    std::lock_guard imageMetadataDatabaseLock(m_imageMetadataDatabaseMutex);
    std::lock_guard tenantDatabaseLock(m_tenantDatabaseMutex);
    std::lock_guard nameGeneratorLock(m_nameGeneratorMutex);
    std::lock_guard danceDatabaseLock(m_danceDatabaseMutex);
    std::lock_guard spawnTypeDatabaseLock(m_spawnTypeDatabaseMutex);
    std::lock_guard radioMessageDatabaseLock(m_radioMessageDatabaseMutex);
    std::lock_guard collectionDatabaseLock(m_collectionDatabaseMutex);

    // Configuration and Assets are at the very bottom of the hierarchy.
    std::lock_guard configurationLock(m_configurationMutex);
    std::lock_guard assetsLock(m_assetsMutex);

    auto written = writeConfig();
    if (!written)
      return std::unexpected(std::move(written.error()));

    m_entityFactory.reset();
    m_speciesDatabase.reset();
    m_itemDatabase.reset();
    m_objectDatabase.reset();
    m_playerFactory.reset();
    m_stagehandDatabase.reset();
    m_vehicleDatabase.reset();
    m_npcDatabase.reset();
    m_monsterDatabase.reset();
    m_plantDatabase.reset();
    m_projectileDatabase.reset();
    m_biomeDatabase.reset();
    m_dungeonDefinitions.reset();
    m_tilesetDatabase.reset();
    m_statisticsDatabase.reset();
    m_liquidsDatabase.reset();
    m_materialDatabase.reset();
    m_damageDatabase.reset();
    m_effectSourceDatabase.reset();
    m_statusEffectDatabase.reset();
    m_treasureDatabase.reset();
    m_codexDatabase.reset();
    m_behaviorDatabase.reset();
    m_techDatabase.reset();
    m_aiDatabase.reset();
    m_questTemplateDatabase.reset();
    m_emoteProcessor.reset();
    m_terrainDatabase.reset();
    m_particleDatabase.reset();
    m_versioningDatabase.reset();
    m_functionDatabase.reset();
    m_imageMetadataDatabase.reset();
    m_tenantDatabase.reset();
    m_nameGenerator.reset();
    m_danceDatabase.reset();
    m_spawnTypeDatabase.reset();
    m_radioMessageDatabase.reset();
    m_collectionDatabase.reset();
    m_assets.reset();
    m_configuration.reset();
  }

  return m_reloadListeners.trigger();
}

std::expected<void, StarException> Root::loadMods(StringList modDirectories, bool _reload) {
  // Need to clear mod directories because there was an update for UGC, which have been added already as it assumes an update isn't needed.  
  if (_reload)
    m_modDirectories.clear();

  std::lock_guard locker(m_modsMutex);
  m_modDirectories = std::move(modDirectories);
  
  if (_reload)
    return reload();
  return {};
}

std::expected<void, StarException> Root::fullyLoad() {
  WorkerPool workerPool("Root::fullyLoad");
  auto started = workerPool.start(RootLoadThreads);
  if (!started)
    return std::unexpected(std::move(started.error()));
  List<WorkerPoolHandle> loaders;

  loaders.reserve(40);

  auto add = [&](auto method) {
    loaders.push_back(workerPool.addWork([this, method]() -> std::expected<void, StarException> {
      auto loaded = (this->*method)();
      if (!loaded)
        return std::unexpected(std::move(loaded.error()));
      return {};
    }));
  };
  add(&Root::assets);
  add(&Root::configuration);
  add(&Root::codexDatabase);
  add(&Root::behaviorDatabase);
  add(&Root::techDatabase);
  add(&Root::aiDatabase);
  add(&Root::questTemplateDatabase);
  add(&Root::emoteProcessor);
  add(&Root::terrainDatabase);
  add(&Root::particleDatabase);
  add(&Root::versioningDatabase);
  add(&Root::functionDatabase);
  add(&Root::imageMetadataDatabase);
  add(&Root::tenantDatabase);
  add(&Root::nameGenerator);
  add(&Root::danceDatabase);
  add(&Root::spawnTypeDatabase);
  add(&Root::radioMessageDatabase);
  add(&Root::collectionDatabase);
  add(&Root::statisticsDatabase);
  add(&Root::speciesDatabase);
  add(&Root::projectileDatabase);
  add(&Root::stagehandDatabase);
  add(&Root::damageDatabase);
  add(&Root::effectSourceDatabase);
  add(&Root::statusEffectDatabase);
  add(&Root::treasureDatabase);
  add(&Root::materialDatabase);
  add(&Root::objectDatabase);
  add(&Root::npcDatabase);
  add(&Root::plantDatabase);
  add(&Root::itemDatabase);
  add(&Root::monsterDatabase);
  add(&Root::vehicleDatabase);
  add(&Root::playerFactory);
  add(&Root::entityFactory);
  add(&Root::biomeDatabase);
  add(&Root::liquidsDatabase);
  add(&Root::dungeonDefinitions);
  add(&Root::tilesetDatabase);

  auto startSeconds = std::chrono::time_point_cast<std::chrono::duration<double>>(std::chrono::steady_clock::now());
  std::optional<StarException> failure;
  for (auto& loader : loaders) {
    auto finished = loader.finish();
    if (!finished && !failure)
      failure = std::move(finished.error());
  }
  // Every accepted job borrows this Root. Drain the entire pool even on failure.
  auto finished = workerPool.finish();
  if (!finished && !failure)
    failure = std::move(finished.error());
  if (failure)
    return std::unexpected(std::move(*failure));
  auto logged = m_logger.info("Root: Loaded everything in {} seconds", (std::chrono::time_point_cast<std::chrono::duration<double>>(std::chrono::steady_clock::now()) - startSeconds).count());
  if (!logged)
    return std::unexpected(std::move(logged.error()));

  {
    std::lock_guard locker(m_assetsMutex);
    if (m_assets)
      m_assets->clearCache();
  }
  return {};
}

ListenerSubscription Root::registerReloadListener(std::weak_ptr<Listener> reloadListener) {
  return m_reloadListeners.addListener(std::move(reloadListener));
}

std::expected<void, StarException> Root::hotReload() {
  auto loaded = assets();
  if (!loaded)
    return std::unexpected(std::move(loaded.error()));
  (*loaded)->hotReload();
  return m_reloadListeners.trigger();
}

String Root::toStoragePath(String const& path) const {
  return File::relative_to(m_settings.storageDirectory, File::convert_dir_separators(path));
}

std::expected<std::shared_ptr<Assets const>, StarException> Root::assets() {
  return loadMemberFunction<Assets>(m_assets, m_assetsMutex, "Assets", [this]() -> std::expected<std::shared_ptr<Assets>, StarException> {
      StringList assetDirectories = m_settings.assetDirectories;
      assetDirectories.append_range(m_modDirectories);
      auto assetSources = scanForAssetSources(assetDirectories, m_settings.assetSources);
      if (!assetSources)
        return std::unexpected(std::move(assetSources.error()));
      auto loaded = Assets::create(m_logger, m_random, m_logMap, m_settings.assetsSettings, std::move(*assetSources));
      if (!loaded)
        return std::unexpected(std::move(loaded.error()));
      auto logged = m_logger.info("Assets digest is {}", hexEncode((*loaded)->digest()));
      if (!logged)
        return std::unexpected(std::move(logged.error()));
      return loaded;
    });
}

std::expected<std::shared_ptr<Configuration>, StarException> Root::configuration() {
  return loadMemberFunction<Configuration>(m_configuration, m_configurationMutex, "Configuration",
    [this]() -> std::expected<std::shared_ptr<Configuration>, StarException> {
      Json currentConfig = m_settings.defaultConfiguration;
      if (m_runtimeConfigFile) {
        auto exists = File::is_regular_file(*m_runtimeConfigFile);
        if (!exists)
          return std::unexpected(std::move(exists.error()));
        if (!*exists) {
          auto logged = m_logger.info("Root: no runtime config file, creating new default runtime config");
          if (!logged)
            return std::unexpected(std::move(logged.error()));
        } else {
          auto readConfiguration = [this]() -> std::expected<Json, StarException> {
            auto contents = File::read_file_string(*m_runtimeConfigFile);
            if (!contents)
              return std::unexpected(std::move(contents.error()));
            auto parsed = Json::parseJson(*contents);
            if (!parsed)
              return std::unexpected(std::move(parsed.error()));
            if (!parsed->isType(Json::Type::Object))
              return std::unexpected(ConfigurationException("User config is not of JSON type Object"));
            auto version = parsed->get("configurationVersion", {});
            if (!version)
              return std::unexpected(std::move(version.error()));
            auto defaultVersion = m_settings.defaultConfiguration.get("configurationVersion", {});
            if (!defaultVersion)
              return std::unexpected(std::move(defaultVersion.error()));
            if (*version != *defaultVersion)
              return std::unexpected(ConfigurationException("User config version does not match default config version"));
            auto config = parsed->toObject();
            if (!config)
              return std::unexpected(std::move(config.error()));
            auto defaults = m_settings.defaultConfiguration.objectPtr();
            if (!defaults)
              return std::unexpected(std::move(defaults.error()));
            for (auto const& entry : **defaults) {
              if (!config->contains(entry.first))
                config->insert(entry.first, entry.second);
            }
            return Json(std::move(*config));
          };
          auto loaded = readConfiguration();
          if (loaded) {
            currentConfig = std::move(*loaded);
          } else {
            auto logged = m_logger.warnException(strf("Root: Failed to load user configuration file {}, resetting user config", *m_runtimeConfigFile), ExceptionDiagnostic(loaded.error()));
            if (!logged)
              return std::unexpected(std::move(logged.error()));
            auto backup = File::rename(*m_runtimeConfigFile, *m_runtimeConfigFile + ".old");
            if (!backup)
              return std::unexpected(std::move(backup.error()));
          }
        }
      }
      return std::make_shared<Configuration>(m_settings.defaultConfiguration, std::move(currentConfig));
    });
}

std::expected<std::shared_ptr<ObjectDatabase const>, StarException> Root::objectDatabase() {
  return loadMember(m_objectDatabase, m_objectDatabaseMutex, "ObjectDatabase", *this);
}

std::expected<std::shared_ptr<PlantDatabase const>, StarException> Root::plantDatabase() {
  return loadMember(m_plantDatabase, m_plantDatabaseMutex, "PlantDatabase", *this);
}

std::expected<std::shared_ptr<ProjectileDatabase const>, StarException> Root::projectileDatabase() {
  return loadMember(m_projectileDatabase, m_projectileDatabaseMutex, "ProjectileDatabase", *this);
}

std::expected<std::shared_ptr<MonsterDatabase const>, StarException> Root::monsterDatabase() {
  return loadMember(m_monsterDatabase, m_monsterDatabaseMutex, "MonsterDatabase", *this);
}

std::expected<std::shared_ptr<NpcDatabase const>, StarException> Root::npcDatabase() {
  return loadMember(m_npcDatabase, m_npcDatabaseMutex, "NpcDatabase", *this);
}

std::expected<std::shared_ptr<StagehandDatabase const>, StarException> Root::stagehandDatabase() {
  return loadMember(m_stagehandDatabase, m_stagehandDatabaseMutex, "StagehandDatabase", *this);
}

std::expected<std::shared_ptr<VehicleDatabase const>, StarException> Root::vehicleDatabase() {
  return loadMember(m_vehicleDatabase, m_vehicleDatabaseMutex, "VehicleDatabase", *this);
}

std::expected<std::shared_ptr<PlayerFactory const>, StarException> Root::playerFactory() {
  return loadMember(m_playerFactory, m_playerFactoryMutex, "PlayerFactory", *this);
}

std::expected<std::shared_ptr<EntityFactory const>, StarException> Root::entityFactory() {
  return loadMember(m_entityFactory, m_entityFactoryMutex, "EntityFactory", *this);
}

std::expected<std::shared_ptr<PatternedNameGenerator const>, StarException> Root::nameGenerator() {
  return loadMember(m_nameGenerator, m_nameGeneratorMutex, "NameGenerator", *this);
}

std::expected<std::shared_ptr<ItemDatabase const>, StarException> Root::itemDatabase() {
  return loadMember(m_itemDatabase, m_itemDatabaseMutex, "ItemDatabase", *this);
}

std::expected<std::shared_ptr<MaterialDatabase const>, StarException> Root::materialDatabase() {
  return loadMemberFunction<MaterialDatabase>(m_materialDatabase, m_materialDatabaseMutex, "MaterialDatabase",
    [this]() -> std::expected<std::shared_ptr<MaterialDatabase>, StarException> {
      auto a = assets();
      if (!a) return std::unexpected(std::move(a.error()));
      auto particles = particleDatabase();
      if (!particles) return std::unexpected(std::move(particles.error()));
      auto metadata = imageMetadataDatabase();
      if (!metadata) return std::unexpected(std::move(metadata.error()));
      return MaterialDatabase::create(*a, *particles, **metadata, logger(), random());
    });
}

std::expected<std::shared_ptr<TerrainDatabase const>, StarException> Root::terrainDatabase() {
  return loadMemberFunction<TerrainDatabase>(m_terrainDatabase, m_terrainDatabaseMutex, "TerrainDatabase", [this] {
    return assets().and_then([this](auto const& a) { return TerrainDatabase::create(a, random()); });
  });
}

std::expected<std::shared_ptr<BiomeDatabase const>, StarException> Root::biomeDatabase() {
  return loadMember(m_biomeDatabase, m_biomeDatabaseMutex, "BiomeDatabase", *this);
}

std::expected<std::shared_ptr<LiquidsDatabase const>, StarException> Root::liquidsDatabase() {
  return loadMemberFunction<LiquidsDatabase>(m_liquidsDatabase, m_liquidsDatabaseMutex, "LiquidsDatabase",
    [this]() -> std::expected<std::shared_ptr<LiquidsDatabase>, StarException> {
      auto a = assets();
      if (!a) return std::unexpected(std::move(a.error()));
      auto materials = materialDatabase();
      if (!materials) return std::unexpected(std::move(materials.error()));
      return LiquidsDatabase::create(*a, *materials);
    });
}

std::expected<std::shared_ptr<StatusEffectDatabase const>, StarException> Root::statusEffectDatabase() {
  return loadMemberFunction<StatusEffectDatabase>(m_statusEffectDatabase, m_statusEffectDatabaseMutex, "StatusEffectDatabase", [this] {
    return assets().and_then([](auto const& a) { return StatusEffectDatabase::create(a); });
  });
}

std::expected<std::shared_ptr<DamageDatabase const>, StarException> Root::damageDatabase() {
  return loadMemberFunction<DamageDatabase>(m_damageDatabase, m_damageDatabaseMutex, "DamageDatabase", [this] {
    return assets().and_then([](auto const& a) { return DamageDatabase::create(a); });
  });
}

std::expected<std::shared_ptr<ParticleDatabase const>, StarException> Root::particleDatabase() {
  return loadMember(m_particleDatabase, m_particleDatabaseMutex, "ParticleDatabase", *this);
}

std::expected<std::shared_ptr<EffectSourceDatabase const>, StarException> Root::effectSourceDatabase() {
  return loadMember(m_effectSourceDatabase, m_effectSourceDatabaseMutex, "EffectSourceDatabase", *this);
}

std::expected<std::shared_ptr<FunctionDatabase const>, StarException> Root::functionDatabase() {
  return loadMemberFunction<FunctionDatabase>(m_functionDatabase, m_functionDatabaseMutex, "FunctionDatabase", [this] {
    return assets().and_then([](auto const& a) { return FunctionDatabase::create(a); });
  });
}

std::expected<std::shared_ptr<TreasureDatabase const>, StarException> Root::treasureDatabase() {
  return loadMember(m_treasureDatabase, m_treasureDatabaseMutex, "TreasureDatabase", *this);
}

std::expected<std::shared_ptr<DungeonDefinitions const>, StarException> Root::dungeonDefinitions() {
  return loadMember(m_dungeonDefinitions, m_dungeonDefinitionsMutex, "DungeonDefinitions", *this);
}

std::expected<std::shared_ptr<TilesetDatabase const>, StarException> Root::tilesetDatabase() {
  return loadMember(m_tilesetDatabase, m_tilesetDatabaseMutex, "TilesetDatabase", *this);
}

std::expected<std::shared_ptr<StatisticsDatabase const>, StarException> Root::statisticsDatabase() {
  return loadMember(m_statisticsDatabase, m_statisticsDatabaseMutex, "StatisticsDatabase", *this);
}

std::expected<std::shared_ptr<EmoteProcessor const>, StarException> Root::emoteProcessor() {
  return loadMemberFunction<EmoteProcessor>(m_emoteProcessor, m_emoteProcessorMutex, "EmoteProcessor", [this] {
    return assets().and_then([](auto const& a) { return EmoteProcessor::create(a); });
  });
}

std::expected<std::shared_ptr<SpeciesDatabase const>, StarException> Root::speciesDatabase() {
  return loadMember(m_speciesDatabase, m_speciesDatabaseMutex, "SpeciesDatabase", *this);
}

std::expected<std::shared_ptr<ImageMetadataDatabase const>, StarException> Root::imageMetadataDatabase() {
  return loadMemberFunction<ImageMetadataDatabase>(m_imageMetadataDatabase, m_imageMetadataDatabaseMutex, "ImageMetadataDatabase",
    [this]() -> std::expected<std::shared_ptr<ImageMetadataDatabase>, StarException> {
      return std::make_shared<ImageMetadataDatabase>(*this);
    });
}

std::expected<std::shared_ptr<VersioningDatabase const>, StarException> Root::versioningDatabase() {
  return loadMember(m_versioningDatabase, m_versioningDatabaseMutex, "VersioningDatabase", *this);
}

std::expected<std::shared_ptr<QuestTemplateDatabase const>, StarException> Root::questTemplateDatabase() {
  return loadMemberFunction<QuestTemplateDatabase>(m_questTemplateDatabase, m_questTemplateDatabaseMutex, "QuestTemplateDatabase", [this] {
    return assets().and_then([this](auto const& a) { return QuestTemplateDatabase::create(a, logger()); });
  });
}

std::expected<std::shared_ptr<AiDatabase const>, StarException> Root::aiDatabase() {
  return loadMember(m_aiDatabase, m_aiDatabaseMutex, "AiDatabase", *this);
}

std::expected<std::shared_ptr<TechDatabase const>, StarException> Root::techDatabase() {
  return loadMemberFunction<TechDatabase>(m_techDatabase, m_techDatabaseMutex, "TechDatabase", [this] {
    return assets().and_then([](auto const& a) { return TechDatabase::create(a); });
  });
}

std::expected<std::shared_ptr<CodexDatabase const>, StarException> Root::codexDatabase() {
  return loadMemberFunction<CodexDatabase>(m_codexDatabase, m_codexDatabaseMutex, "CodexDatabase", [this] {
    return assets().and_then([](auto const& a) { return CodexDatabase::create(a); });
  });
}

std::expected<std::shared_ptr<BehaviorDatabase const>, StarException> Root::behaviorDatabase() {
  return loadMemberFunction<BehaviorDatabase>(m_behaviorDatabase, m_behaviorDatabaseMutex, "BehaviorDatabase", [this] {
    return assets().and_then([](auto const& a) { return BehaviorDatabase::create(a); });
  });
}

std::expected<std::shared_ptr<TenantDatabase const>, StarException> Root::tenantDatabase() {
  return loadMember(m_tenantDatabase, m_tenantDatabaseMutex, "TenantDatabase", *this);
}

std::expected<std::shared_ptr<DanceDatabase const>, StarException> Root::danceDatabase() {
  return loadMemberFunction<DanceDatabase>(m_danceDatabase, m_danceDatabaseMutex, "DanceDatabase", [this] {
    return assets().and_then([this](auto const& a) { return DanceDatabase::create(a, logger()); });
  });
}

std::expected<std::shared_ptr<SpawnTypeDatabase const>, StarException> Root::spawnTypeDatabase() {
  return loadMemberFunction<SpawnTypeDatabase>(m_spawnTypeDatabase, m_spawnTypeDatabaseMutex, "SpawnTypeDatabase", [this] {
    return assets().and_then([](auto const& a) { return SpawnTypeDatabase::create(a); });
  });
}

std::expected<std::shared_ptr<RadioMessageDatabase const>, StarException> Root::radioMessageDatabase() {
  return loadMember(m_radioMessageDatabase, m_radioMessageDatabaseMutex, "RadioMessageDatabase", *this);
}

std::expected<std::shared_ptr<CollectionDatabase const>, StarException> Root::collectionDatabase() {
  return loadMember(m_collectionDatabase, m_collectionDatabaseMutex, "CollectionDatabase", *this);
}

Root::Settings& Root::settings() {
  return m_settings;
}

std::expected<StringList, StarException> Root::scanForAssetSources(StringList const& directories, StringList const& manual) {
  struct AssetSource {
    String path;
    std::optional<String> name;
    std::optional<String> version;
    float priority;
    StringList requires_;
    StringList includes;
  };
  List<std::shared_ptr<AssetSource>> assetSources;
  StringMap<std::shared_ptr<AssetSource>> namedSources;

  auto processEntry = [&](String const& sourcePath, bool isDirectory) -> std::expected<bool, StarException> {
    std::shared_ptr<Star::AssetSource> source;
    auto name = File::base_name(sourcePath);
    auto hidden = name.starts_with(".");
    if (!hidden) return std::unexpected(std::move(hidden.error()));
    if (!*hidden) {
      hidden = name.starts_with("_");
      if (!hidden) return std::unexpected(std::move(hidden.error()));
    }
    if (*hidden) {
      auto logged = m_logger.info("Root: Skipping hidden '{}' in asset directory", name);
      if (!logged) return std::unexpected(std::move(logged.error()));
    } else if (isDirectory) {
      auto created = DirectoryAssetSource::create(m_logger, sourcePath);
      if (!created) return std::unexpected(std::move(created.error()));
      source = std::move(*created);
    } else {
      auto packed = sourcePath.ends_with(".pak");
      if (!packed) return std::unexpected(std::move(packed.error()));
      if (*packed) {
        auto created = PackedAssetSource::create(sourcePath);
        if (!created) return std::unexpected(std::move(created.error()));
        source = std::move(*created);
      } else {
        auto logged = m_logger.warn("Root: Unrecognized file in asset directory '{}', skipping", name);
        if (!logged) return std::unexpected(std::move(logged.error()));
      }
    }

    if (!source)
      return false;

    auto metadata = source->metadata();

    auto assetSource = std::make_shared<AssetSource>();
    assetSource->path = sourcePath;
    if (auto value = metadata.maybe("name")) {
      auto parsed = value->toString();
      if (!parsed) return std::unexpected(std::move(parsed.error()));
      assetSource->name = std::move(*parsed);
    }
    if (auto value = metadata.maybe("version")) {
      auto printed = value->printString();
      if (!printed) return std::unexpected(std::move(printed.error()));
      assetSource->version = std::move(*printed);
    }
    auto priority = metadata.value("priority", 0.0f).toFloat();
    if (!priority) return std::unexpected(std::move(priority.error()));
    assetSource->priority = *priority;
    auto required = jsonToStringList(metadata.value("requires", JsonArray{}));
    if (!required) return std::unexpected(std::move(required.error()));
    assetSource->requires_ = std::move(*required);
    auto included = jsonToStringList(metadata.value("includes", JsonArray{}));
    if (!included) return std::unexpected(std::move(included.error()));
    assetSource->includes = std::move(*included);

    if (assetSource->name.value_or(String{}) == "opensb_base" && assetSource->version.value_or(String{}) != OpenStarVersionString) {
      return std::unexpected(AssetSourceException(strf("\n\nOpenStarbound assets version mismatch!\nOpenStarbound v{}, but opensb.pak is v{}\n",
        OpenStarVersionString, assetSource->version.value_or(String{})), false));
    }

    if (assetSource->name) {
      if (auto oldAssetSource = namedSources.value(*assetSource->name)) {
        if (oldAssetSource->priority <= assetSource->priority) {
          auto logged = m_logger.warn("Root: Overriding duplicate asset source '{}' named '{}' with higher or equal priority source '{}",
              oldAssetSource->path, *assetSource->name, assetSource->path);
          if (!logged) return std::unexpected(std::move(logged.error()));
          *oldAssetSource = *assetSource;
        } else {
          auto logged = m_logger.warn("Root: Skipping duplicate asset source '{}' named '{}', previous source '{}' has higher priority",
              assetSource->path, *assetSource->name, oldAssetSource->priority);
          if (!logged) return std::unexpected(std::move(logged.error()));
        }
      } else {
        namedSources[*assetSource->name] = assetSource;
        assetSources.push_back(std::move(assetSource));
      }
    } else {
      assetSources.push_back(std::move(assetSource));
    }

    return true;
  };

  // Scan for assets in each given directory, the first-level ordering of asset
  // sources comes from the scanning order here, and then alphabetically by the
  // file / directory name

  for (auto const& directory : directories) {
    auto directoryExists = File::is_directory(directory);
    if (!directoryExists) return std::unexpected(std::move(directoryExists.error()));
    if (!*directoryExists) {
      auto logged = m_logger.info("Root: Skipping asset directory '{}', directory not found", directory);
      if (!logged) return std::unexpected(std::move(logged.error()));
      continue;
    }

    auto logged = m_logger.info("Root: Scanning for asset sources in directory '{}'", directory);
    if (!logged) return std::unexpected(std::move(logged.error()));
    auto entries = File::directory_entries(directory, true);
    if (!entries) return std::unexpected(std::move(entries.error()));
    entries->sort();
    for (auto& entry : *entries) {
      auto processed = processEntry(File::relative_to(directory, entry.first), entry.second);
      if (!processed) return std::unexpected(std::move(processed.error()));
    }
  }

  // Take in any manual asset source paths

  for (auto& path : manual) {
    auto directory = File::is_directory(path);
    if (!directory) return std::unexpected(std::move(directory.error()));
    auto processed = processEntry(path, *directory);
    if (!processed) return std::unexpected(std::move(processed.error()));
  }

  // Then, order asset sources so that lower priority assets come before higher
  // priority ones

  assetSources.sort([](auto const& a, auto const& b) {
      return 
        a->priority == b->priority ? 
          a->name.value_or(a->path) < b->name.value_or(b->path) : 
          a->priority < b->priority;
    });

  // Finally, sort asset sources so that sources that have dependencies come
  // after their dependencies.

  HashSet<std::shared_ptr<AssetSource>> workingSet;
  OrderedHashSet<std::shared_ptr<AssetSource>> dependencySortedSources;

  std::function<std::expected<void, StarException>(std::shared_ptr<AssetSource>)> dependencySortVisit;
  dependencySortVisit = [&](std::shared_ptr<AssetSource> source) -> std::expected<void, StarException> {
    if (workingSet.contains(source))
      return std::unexpected(AssetSourceException("Asset dependencies form a cycle"));

    if (dependencySortedSources.contains(source))
      return {};

    workingSet.add(source);

    for (auto const& includeName : source->includes) {
      if (auto include = namedSources.find_ref(includeName)) {
        auto visited = dependencySortVisit(include->get());
        if (!visited) return std::unexpected(std::move(visited.error()));
      }
    }

    for (auto const& requirementName : source->requires_) {
      if (auto requirement = namedSources.find_ref(requirementName)) {
        auto visited = dependencySortVisit(requirement->get());
        if (!visited) return std::unexpected(std::move(visited.error()));
      } else {
        return std::unexpected(AssetSourceException(strf("Asset source '{}' is missing dependency '{}'{}", source->name ? *source->name : "<unnamed>", requirementName,
          requirementName != "base" ? "" :
            "\n\nThe base Starbound asset package could not be found, please copy it from another Starbound install!\n"
            "(Locate 'packed.pak' in vanilla Starbound's assets folder, then copy it to OpenStarbound's assets folder.)\n"), false));
      }
    }

    workingSet.remove(source);

    dependencySortedSources.add(std::move(source));
    return {};
  };

  for (auto source : assetSources) {
    auto visited = dependencySortVisit(std::move(source));
    if (!visited) return std::unexpected(std::move(visited.error()));
  }

  StringList sourcePaths;
  for (auto const& source : dependencySortedSources) {
    auto path = File::convert_dir_separators(source->path);
    auto logged = source->name
      ? m_logger.info("Root: Detected asset source named '{}'{} at '{}'", *source->name, source->version ? strf(" version '{}'", *source->version) : "", path)
      : m_logger.info("Root: Detected unnamed asset source at '{}'", path);
    if (!logged) return std::unexpected(std::move(logged.error()));
    sourcePaths.push_back(path);
  }

  return sourcePaths;
}

std::expected<void, StarException> Root::writeConfig() {
  if (m_configuration) {
    auto currentConfig = m_configuration->currentConfiguration();
    if (m_lastRuntimeConfig != currentConfig) {
      if (m_runtimeConfigFile) {
        auto logged = m_logger.info("Root: Writing runtime configuration to '{}'", *m_runtimeConfigFile);
        if (!logged) return std::unexpected(std::move(logged.error()));
        auto contents = m_configuration->printConfiguration();
        if (!contents) return std::unexpected(std::move(contents.error()));
        auto written = File::overwrite_file_with_rename(*contents, *m_runtimeConfigFile);
        if (!written) return std::unexpected(std::move(written.error()));
      }
      m_lastRuntimeConfig = std::move(currentConfig);
    }
  }
  return {};
}

std::expected<std::reference_wrapper<WorkerPool>, StarException> Root::workers() {
  std::lock_guard locker(m_workersStartMutex);
  if (!m_workersStarted) {
    auto started = m_workers.start(std::max(1u, std::thread::hardware_concurrency()));
    if (!started) return std::unexpected(std::move(started.error()));
    m_workersStarted = true;
  }
  return std::ref(m_workers);
}

std::expected<std::reference_wrapper<bool>, StarException> Root::humanoidHeadRotation() {
  std::lock_guard locker(m_headRotationMutex);
  if (!m_headRotationInitialized) {
    auto config = configuration();
    if (!config) return std::unexpected(std::move(config.error()));
    auto setting = (*config)->get("humanoidHeadRotation");
    if (!setting) return std::unexpected(std::move(setting.error()));
    auto value = setting->optBool();
    if (!value) return std::unexpected(std::move(value.error()));
    m_humanoidHeadRotation = value->value_or(true);
    m_headRotationInitialized = true;
  }
  return std::ref(m_humanoidHeadRotation);
}

template <typename T, typename... Params>
[[nodiscard]] std::expected<std::shared_ptr<T>, StarException> Root::loadMember(std::shared_ptr<T>& ptr, std::mutex& mutex, std::string_view name, Params&&... params) {
  return loadMemberFunction<T>(ptr, mutex, name, [&]() {
      return T::create(std::forward<Params>(params)...);
    });
}

template <typename T>
[[nodiscard]] std::expected<std::shared_ptr<T>, StarException> Root::loadMemberFunction(std::shared_ptr<T>& ptr, std::mutex& mutex, std::string_view name, std::function<std::expected<std::shared_ptr<T>, StarException>()> loadFunction) {
  std::lock_guard locker(mutex);
  if (!ptr) {
    auto startSeconds = std::chrono::time_point_cast<std::chrono::duration<double>>(std::chrono::steady_clock::now());
    auto loaded = loadFunction();
    if (!loaded) return std::unexpected(std::move(loaded.error()));
    ptr = std::move(*loaded);
    auto logged = m_logger.info("Root: Loaded {} in {} seconds", name, (std::chrono::time_point_cast<std::chrono::duration<double>>(std::chrono::steady_clock::now()) - startSeconds).count());
    if (!logged) return std::unexpected(std::move(logged.error()));
  }
  return ptr;
}

}

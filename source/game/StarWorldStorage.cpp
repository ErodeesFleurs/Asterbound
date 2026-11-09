import star.memory;
import star.json;
import star.json_extra;
import star.xx_hash;
import star.id_map;
import star.rect;
import star.map;
import star.set;
import star.spatial_hash_2d;
import star.casting;
import star.vector;
import star.game_types;
import star.strong_typedef;
import star.list;
import star.bi_map;
import star.data_stream;
import star.file;
import star.data_stream_extra;
import star.logging;
import star.btree;
import star.ordered_map;
import star.lru_cache;
import star.data_stream_devices;
import star.thread;
import star.btree_database;
import star.ordered_set;
import star.string;
import star.format;
import star.output_proxy;
import star.version;
import star.rpc_promise;
import star.random;
import star.perlin;
import star.weighted_pool;
import std;
import star.worker_pool;

import star.sector_array_2d;
import star.color;
import star.image_processing;
import star.hash;
import star.string_view;
import star.directives;
import star.asset_path;
import star.io_device;
import star.ref_ptr;
import star.listener;

import star.damage_types;
import star.world_geometry;
import star.status_types;
import star.damage;
import star.light_source;
import star.entity;
import star.collision_block;
import star.item_descriptor;
import star.liquid_types;
import star.tile_damage;
import star.tile_sector_array;
import star.animation;
import star.particle;
import star.weather_types;
import star.celestial_coordinate;
import star.sky_types;
import star.force_regions;
import star.world_parameters;
import star.celestial_parameters;
import star.world_layout;
import star.collision_generator;
import star.world_tiles;
import star.interaction_types;
import star.quest_descriptor;
import star.interactive_entity;
import star.tile_entity;
import star.inspectable_entity;
import star.plant;
import star.plant_database;
import star.biome_placement;
import star.lua_root;
import star.world_storage;
import star.asset_source;
import star.assets;

import star.configuration;
import star.root;

import star.liquids_database;
import star.versioning_database;
import star.compression;
import star.material_database;
import star.entity_factory;

import star.entity_map;

import star.byte_array;
import star.algorithm;
namespace Star {

WorldChunks WorldStorage::getWorldChunksUpdate(WorldChunks const& oldChunks, WorldChunks const& newChunks) {
  WorldChunks update;
  for (auto const& p : oldChunks) {
    if (!newChunks.contains(p.first))
      update[p.first] = {};
  }

  for (auto const& p : newChunks) {
    if (oldChunks.value(p.first) != p.second)
      update[p.first] = p.second;
  }
  return update;
}

std::expected<void, StarException> WorldStorage::applyWorldChunksUpdateToFile(Logger& logger, String const& file, WorldChunks const& update) {
  BTreeDatabase db(logger);
  auto operationResult1 = File::open(file, IOMode::ReadWrite);
  if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
  auto operationResult2 = openDatabase(db, (*operationResult1));
  if (!operationResult2) return std::unexpected(std::move(operationResult2.error()));
  for (auto const& pair : update) {
    if (pair.second) {
      auto operationResult1 = db.insert(pair.first, *pair.second);
      if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
    } else {
      auto operationResult1 = db.remove(pair.first);
      if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
    }
  }
  return db.close(true);
}

std::expected<WorldChunks, StarException> WorldStorage::getWorldChunksFromFile(Logger& logger, String const& file) {
  BTreeDatabase db(logger);
  auto operationResult1 = File::open(file, IOMode::Read);
  if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
  auto operationResult2 = openDatabase(db, (*operationResult1));
  if (!operationResult2) return std::unexpected(std::move(operationResult2.error()));
  WorldChunks chunks;
  auto read = db.forAll([&chunks](ByteArray key, ByteArray value) -> std::expected<void, StarException> {
    auto added = chunks.add(std::move(key), std::move(value));
    if (!added) return std::unexpected(std::move(added.error()));
    return {};
  });
  if (!read) return std::unexpected(std::move(read.error()));
  auto closed = db.close(true);
  if (!closed) return std::unexpected(std::move(closed.error()));
  return chunks;
}

std::expected<std::shared_ptr<WorldStorage>, StarException> WorldStorage::create(Root& root, Vec2U const& worldSize, std::shared_ptr<IODevice> const& device, std::shared_ptr<WorldGeneratorFacade> const& generatorFacade) {
  auto result = std::shared_ptr<WorldStorage>(new WorldStorage(root));
  auto initialized = result->initialize();
  if (!initialized) return std::unexpected(std::move(initialized.error()));
  auto& db = result->m_db;
  result->m_tileArray = std::make_shared<ServerTileSectorArray>(worldSize);
  result->m_entityMap = std::make_shared<EntityMap>(worldSize, MinServerEntityId, MaxServerEntityId);
  result->m_generatorFacade = generatorFacade;
  result->m_floatingDungeonWorld = false;
  {
    auto operationResult1 = device->resize(0);
    if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
  }
  {
    auto operationResult1 = openDatabase(db, device);
    if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
  }
  {
    auto metadataRecordKey = metadataKey();
    if (!metadataRecordKey)
      return std::unexpected(std::move(metadataRecordKey.error()));
    auto serializedMetadata =
      writeWorldMetadata(WorldMetadataStore{.worldSize = worldSize, .userMetadata = VersionedJson()});
    if (!serializedMetadata)
      return std::unexpected(std::move(serializedMetadata.error()));
    auto metadataInserted = db.insert((*metadataRecordKey), (*serializedMetadata));
    if (!metadataInserted)
      return std::unexpected(std::move(metadataInserted.error()));
  }
  {
    auto operationResult1 = db.commit();
    if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
  }
  return result;
}

std::expected<std::shared_ptr<WorldStorage>, StarException> WorldStorage::create(Root& root, std::shared_ptr<IODevice> const& device, std::shared_ptr<WorldGeneratorFacade> const& generatorFacade) {
  auto result = std::shared_ptr<WorldStorage>(new WorldStorage(root));
  auto initialized = result->initialize();
  if (!initialized) return std::unexpected(std::move(initialized.error()));
  auto& db = result->m_db;
  result->m_generatorFacade = generatorFacade;
  result->m_floatingDungeonWorld = false;
  {
    auto operationResult1 = openDatabase(db, device);
    if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
  }
  auto autofoundResult1 = metadataKey();
  if (!autofoundResult1) return std::unexpected(std::move(autofoundResult1.error()));
  auto autofoundResult2 = db.find((*autofoundResult1));
  if (!autofoundResult2) return std::unexpected(std::move(autofoundResult2.error()));
  auto found = std::move((*autofoundResult2));
  if (!found) return std::unexpected(WorldStorageException("World storage metadata is missing"));
  auto autometadataResult1 = readWorldMetadata(*found);
  if (!autometadataResult1) return std::unexpected(std::move(autometadataResult1.error()));
  auto metadata = std::move((*autometadataResult1));
  auto worldSize = metadata.worldSize;
  result->m_tileArray = std::make_shared<ServerTileSectorArray>(worldSize);
  result->m_entityMap = std::make_shared<EntityMap>(worldSize, MinServerEntityId, MaxServerEntityId);
  return result;
}

std::expected<std::shared_ptr<WorldStorage>, StarException> WorldStorage::create(Root& root, WorldChunks const& chunks, std::shared_ptr<WorldGeneratorFacade> const& generatorFacade) {
  auto result = std::shared_ptr<WorldStorage>(new WorldStorage(root));
  auto initialized = result->initialize();
  if (!initialized) return std::unexpected(std::move(initialized.error()));
  auto& db = result->m_db;
  result->m_generatorFacade = generatorFacade;
  result->m_floatingDungeonWorld = false;
  {
    auto storage_file = File::ephemeral_file(root.random());
    if (!storage_file)
      return std::unexpected(std::move(storage_file.error()));
    auto database_opened = openDatabase(db, (*storage_file));
    if (!database_opened)
      return std::unexpected(std::move(database_opened.error()));
  }
  for (auto const& pair : chunks) {
    if (pair.second) {
      {
        auto operationResult1 = db.insert(pair.first, *pair.second);
        if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
      }
    }
  }
  auto autofoundResult1 = metadataKey();
  if (!autofoundResult1) return std::unexpected(std::move(autofoundResult1.error()));
  auto autofoundResult2 = db.find((*autofoundResult1));
  if (!autofoundResult2) return std::unexpected(std::move(autofoundResult2.error()));
  auto found = std::move((*autofoundResult2));
  if (!found) return std::unexpected(WorldStorageException("World storage metadata is missing"));
  auto autometadataResult1 = readWorldMetadata(*found);
  if (!autometadataResult1) return std::unexpected(std::move(autometadataResult1.error()));
  auto metadata = std::move((*autometadataResult1));
  auto worldSize = metadata.worldSize;
  result->m_tileArray = std::make_shared<ServerTileSectorArray>(worldSize);
  result->m_entityMap = std::make_shared<EntityMap>(worldSize, MinServerEntityId, MaxServerEntityId);
  return result;
}

WorldStorage::~WorldStorage() = default;

std::expected<void, StarException> WorldStorage::close() {
  if (m_db.isOpen()) {
    auto unloaded = unloadAll(true);
    if (!unloaded) return std::unexpected(std::move(unloaded.error()));
    return m_db.close(true);
  }
  return {};
}

std::expected<VersionedJson, StarException> WorldStorage::worldMetadata() {
  auto autofoundResult1 = metadataKey();
  if (!autofoundResult1) return std::unexpected(std::move(autofoundResult1.error()));
  auto autofoundResult2 = m_db.find((*autofoundResult1));
  if (!autofoundResult2) return std::unexpected(std::move(autofoundResult2.error()));
  auto found = std::move((*autofoundResult2));
  if (!found) return std::unexpected(WorldStorageException("World storage metadata is missing"));
  auto autometadataResult1 = readWorldMetadata(*found);
  if (!autometadataResult1) return std::unexpected(std::move(autometadataResult1.error()));
  auto metadata = std::move((*autometadataResult1));
  return std::move(metadata.userMetadata);
}

std::expected<void, StarException> WorldStorage::setWorldMetadata(VersionedJson const& metadata) {
  auto metadataRecordKey = metadataKey();
  if (!metadataRecordKey)
    return std::unexpected(std::move(metadataRecordKey.error()));
  auto serializedMetadata = writeWorldMetadata({.worldSize = Vec2U(m_tileArray->size()), .userMetadata = metadata});
  if (!serializedMetadata)
    return std::unexpected(std::move(serializedMetadata.error()));
  auto metadataInserted = m_db.insert((*metadataRecordKey), (*serializedMetadata));
  if (!metadataInserted)
    return std::unexpected(std::move(metadataInserted.error()));
  return {};
}

std::shared_ptr<ServerTileSectorArray> const& WorldStorage::tileArray() const {
  return m_tileArray;
}

std::shared_ptr<EntityMap> const& WorldStorage::entityMap() const {
  return m_entityMap;
}

std::optional<WorldStorage::Sector> WorldStorage::sectorForPosition(Vec2I const& position) const {
  auto s = m_tileArray->sectorFor(position);
  if (m_tileArray->sectorValid(s))
    return s;
  return {};
}

std::expected<List<WorldStorage::Sector>, StarException> WorldStorage::sectorsForRegion(RectI const& region) const {
  return m_tileArray->validSectorsFor(region);
}

std::optional<RectI> WorldStorage::regionForSector(Sector sector) const {
  if (m_tileArray->sectorValid(sector))
    return m_tileArray->sectorRegion(sector);
  return {};
}

SectorLoadLevel WorldStorage::sectorLoadLevel(Sector sector) const {
  return m_sectorMetadata.value(sector).loadLevel;
}

std::optional<SectorGenerationLevel> WorldStorage::sectorGenerationLevel(Sector sector) const {
  if (auto p = m_sectorMetadata.find_ref(sector))
    return p->get().generationLevel;
  return {};
}

bool WorldStorage::sectorActive(Sector sector) const {
  if (auto p = m_sectorMetadata.find_ref(sector)) {
    if (p->get().loadLevel == SectorLoadLevel::Loaded && p->get().generationLevel == SectorGenerationLevel::Complete)
      return true;
  }
  return false;
}

std::expected<void, StarException> WorldStorage::loadSector(Sector sector) {
  auto result = [&]() -> std::expected<void, StarException> {
    {
      auto operationResult1 = loadSectorToLevel(sector, SectorLoadLevel::Loaded);
      if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
    }
    auto autottlResult1 = randomizedSectorTTL();
    if (!autottlResult1) return std::unexpected(std::move(autottlResult1.error()));
    auto ttl = std::move((*autottlResult1));
    setSectorTimeToLive(sector, ttl);
    return {};
  }();
  if (!result) return std::unexpected(invalidate(std::move(result.error()), Star::format("Failed to load sector {}", sector)));
  return result;
}

std::expected<void, StarException> WorldStorage::activateSector(Sector sector) {
  auto result = [&]() -> std::expected<void, StarException> {
    {
      auto operationResult1 = generateSectorToLevel(sector, SectorGenerationLevel::Complete);
      if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
    }
    auto autottlResult1 = randomizedSectorTTL();
    if (!autottlResult1) return std::unexpected(std::move(autottlResult1.error()));
    auto ttl = std::move((*autottlResult1));
    setSectorTimeToLive(sector, ttl);
    return {};
  }();
  if (!result) return std::unexpected(invalidate(std::move(result.error()), Star::format("Failed to load sector {}", sector)));
  return result;
}

std::expected<void, StarException> WorldStorage::queueSectorActivation(Sector sector) {
  if (auto p = m_sectorMetadata.find_ref(sector)) {
    auto ptimeToLiveResult1 = randomizedSectorTTL();
    if (!ptimeToLiveResult1) return std::unexpected(std::move(ptimeToLiveResult1.error()));
    p->get().timeToLive = std::move((*ptimeToLiveResult1));
    // Don't bother queueing the sector if it is already fully loaded
    if (p->get().loadLevel == SectorLoadLevel::Loaded && p->get().generationLevel == SectorGenerationLevel::Complete)
      return {};
  }

  auto p = m_generationQueue.insert(sector, m_generationQueueTimeToLive);
  m_generationQueue.move_to_front(p.first);
  return {};
}

std::expected<void, StarException> WorldStorage::triggerTerraformSector(Sector sector) {
  auto result = [&]() -> std::expected<void, StarException> {
    {
      auto operationResult1 = loadSectorToLevel(sector, SectorLoadLevel::Loaded);
      if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
    }
    if (auto p = m_sectorMetadata.find_ref(sector)) {
      if (p->get().generationLevel < SectorGenerationLevel::Complete)
        {
          auto operationResult1 = generateSectorToLevel(sector, SectorGenerationLevel::Complete);
          if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
        }

      p->get().generationLevel = SectorGenerationLevel::Terraform;
    } else {
      return std::unexpected(WorldStorageException(Star::format("Couldn't flag sector {} for terraforming; metadata unavailable", sector)));
    }
    return {};
  }();
  if (!result) return std::unexpected(invalidate(std::move(result.error()), Star::format("Failed to terraform sector {}", sector)));
  return result;
}

std::expected<RpcPromise<Vec2I>, StarException> WorldStorage::enqueuePlacement(List<BiomeItemDistribution> distributions, std::optional<DungeonId> id) {
  return m_generatorFacade->enqueuePlacement(std::move(distributions), id);
}

std::optional<float> WorldStorage::sectorTimeToLive(Sector sector) const {
  if (auto p = m_sectorMetadata.find_ref(sector))
    return p->get().timeToLive;
  return {};
}

bool WorldStorage::setSectorTimeToLive(Sector sector, float newTimeToLive) {
  if (auto p = m_sectorMetadata.find_ref(sector)) {
    p->get().timeToLive = newTimeToLive;
    return true;
  }
  return false;
}

std::expected<std::optional<Vec2F>, StarException> WorldStorage::findUniqueEntity(String const& uniqueId) {
  if (auto entity = m_entityMap->entity(m_entityMap->uniqueEntityId(uniqueId)))
    return entity->position();

  // Only return the unique index entry for the entity IF that stored sector is
  // not loaded, if the stored sector is loaded then the entity ought to have
  // been in the live entity map.
  auto index = getUniqueIndexEntry(uniqueId);
  if (!index) return std::unexpected(std::move(index.error()));
  if (auto const& sectorAndPosition = *index) {
    if (m_sectorMetadata.value(sectorAndPosition->first).loadLevel < SectorLoadLevel::Entities)
      return sectorAndPosition->second;
  }

  return {};
}

std::expected<EntityId, StarException> WorldStorage::loadUniqueEntity(String const& uniqueId) {
  EntityId entityId = m_entityMap->uniqueEntityId(uniqueId);
  if (entityId != NullEntityId)
    return entityId;

  auto index = getUniqueIndexEntry(uniqueId);
  if (!index) return std::unexpected(std::move(index.error()));
  if (auto const& sectorAndPosition = *index) {
    auto loaded = loadSector(sectorAndPosition->first);
    if (!loaded) return std::unexpected(std::move(loaded.error()));
    return m_entityMap->uniqueEntityId(uniqueId);
  }

  return {};
}

std::expected<void, StarException> WorldStorage::generateQueue(std::optional<std::size_t> sectorGenerationLevelLimit, std::function<bool(Sector, Sector)> sectorOrdering) {
  auto result = [&]() -> std::expected<void, StarException> {
    if (sectorOrdering) {
      m_generationQueue.sort([&sectorOrdering](auto const& a, auto const& b) {
          return sectorOrdering(a.first, b.first);
        });
    }

    while (!m_generationQueue.empty()) {
      if (sectorGenerationLevelLimit && *sectorGenerationLevelLimit == 0)
        break;

      auto first = m_generationQueue.first_key();
      if (!first) return std::unexpected(std::move(first.error()));
      auto autopResult1 = generateSectorToLevel(first->get(), SectorGenerationLevel::Complete, sectorGenerationLevelLimit.value_or(NPos));
      if (!autopResult1) return std::unexpected(std::move(autopResult1.error()));
      auto p = std::move((*autopResult1));

      if (p.first)
        {
        auto removed = m_generationQueue.erase_front();
        if (!removed)
          return std::unexpected(std::move(removed.error()));
        }
      if (sectorGenerationLevelLimit)
        *sectorGenerationLevelLimit -= p.second;
    }
    return {};
  }();
  if (!result) return std::unexpected(invalidate(std::move(result.error()), "WorldStorage generation failed while generating from queue"));
  return result;
}

std::expected<void, StarException> WorldStorage::tick(float dt, std::optional<std::reference_wrapper<String const>> worldId) {
  auto result = [&]() -> std::expected<void, StarException> {
    // Tick down generation queue entries, and erase any that are expired.
    eraseWhere(m_generationQueue, [dt](auto& p) {
        p.second -= dt;
        return p.second <= 0.0f;
      });

    // Tick down sector TTL values
    for (auto& p : m_sectorMetadata)
      p.second.timeToLive -= dt;

    // Loop over every loaded sector, figure out whether the sector needs to be
    // unloaded, kept alive by a keep-alive entity, or has any entities that need
    // to be stored because they moved into an entity-unloaded sector (zombies).
    auto autoentityFactoryResult1 = this->root().entityFactory();
    if (!autoentityFactoryResult1) return std::unexpected(std::move(autoentityFactoryResult1.error()));
    auto entityFactory = std::move((*autoentityFactoryResult1));
    unsigned unloaded = 0, skipped = 0;
    for (auto const& p : m_sectorMetadata.pairs()) {
      auto const& sector = p.first;
      auto const& metadata = p.second;

      bool needsUnload = metadata.timeToLive <= 0.0f;

      // If it is not time to unload the sector, then we don't need to scan for
      // keep-alive entities.  If the sector is fully loaded, it can not have any
      // zombie entities.  If both of these are true, there is no work to do.
      if (!needsUnload && metadata.loadLevel == SectorLoadLevel::Entities)
        continue;

      bool keepAlive = false;
      List<std::shared_ptr<Entity>> zombieEntities;
      auto scanned = m_entityMap->forEachEntity(RectF(m_tileArray->sectorRegion(sector)), [&](std::shared_ptr<Entity> const& entity) -> std::expected<void, StarException> {
          if (belongsInSector(sector, entity->position())) {
            if (!keepAlive) {
              auto active = m_generatorFacade->entityKeepAlive(*this, entity);
              if (!active) return std::unexpected(std::move(active.error()));
              if (*active) {
                keepAlive = true;
                return {};
              }
            }
            if (metadata.loadLevel < SectorLoadLevel::Entities)
              zombieEntities.push_back(entity);
          }
          return {};
        });
      if (!scanned) return std::unexpected(std::move(scanned.error()));

      if (keepAlive) {
        auto autottlResult1 = randomizedSectorTTL();
        if (!autottlResult1) return std::unexpected(std::move(autottlResult1.error()));
        auto ttl = std::move((*autottlResult1));
        setSectorTimeToLive(sector, ttl);
      } else if (needsUnload) {
        auto autodidUnloadResult1 = unloadSectorToLevel(sector, SectorLoadLevel::None);
        if (!autodidUnloadResult1) return std::unexpected(std::move(autodidUnloadResult1.error()));
        auto didUnload = std::move((*autodidUnloadResult1));
        (didUnload ? unloaded : skipped)++;
      } else if (!zombieEntities.empty()) {
        List<std::shared_ptr<Entity>> zombiesToStore;
        List<std::shared_ptr<Entity>> zombiesToRemove;
        for (auto const& entity : zombieEntities) {
          auto autopersistentResult1 = m_generatorFacade->entityPersistent(*this, entity);
          if (!autopersistentResult1) return std::unexpected(std::move(autopersistentResult1.error()));
          auto persistent = std::move((*autopersistentResult1));
          if (persistent)
            zombiesToStore.push_back(entity);
          else
            zombiesToRemove.push_back(entity);
        }

        for (auto const& entity : zombiesToRemove) {
          m_entityMap->removeEntity(entity->entityId());
          {
            auto operationResult1 = m_generatorFacade->destructEntity(*this, entity);
            if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
          }
        }

        if (!zombiesToStore.empty()) {
          EntitySectorStore sectorStore;
          auto autofoundResult1 = entitySectorKey(sector);
          if (!autofoundResult1) return std::unexpected(std::move(autofoundResult1.error()));
          auto autofoundResult2 = m_db.find((*autofoundResult1));
          if (!autofoundResult2) return std::unexpected(std::move(autofoundResult2.error()));
          auto found = std::move((*autofoundResult2));
          if (found) {
            auto sectorStoreResult1 = readEntitySector(*found);
            if (!sectorStoreResult1) return std::unexpected(std::move(sectorStoreResult1.error()));
            sectorStore = std::move((*sectorStoreResult1));
          }

          UniqueIndexStore storedUniques;
          for (auto const& entity : zombiesToStore) {
            m_entityMap->removeEntity(entity->entityId());
            {
              auto operationResult1 = m_generatorFacade->destructEntity(*this, entity);
              if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
            }
            if (auto uniqueId = entity->uniqueId()) {
              auto added = storedUniques.add(*uniqueId, {sector, entity->position()});
              if (!added) return std::unexpected(std::move(added.error()));
            }
            auto autostoredResult1 = entityFactory->storeVersionedEntity(entity);
            if (!autostoredResult1) return std::unexpected(std::move(autostoredResult1.error()));
            auto stored = std::move((*autostoredResult1));
            sectorStore.push_back(std::move(stored));
          }
          {
            auto operationResult1 = entitySectorKey(sector);
            if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
            auto operationResult2 = writeEntitySector(sectorStore);
            if (!operationResult2) return std::unexpected(std::move(operationResult2.error()));
            auto operationResult3 = m_db.insert((*operationResult1), (*operationResult2));
            if (!operationResult3) return std::unexpected(std::move(operationResult3.error()));
          }
          {
            auto operationResult1 = mergeSectorUniques(sector, storedUniques);
            if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
          }
        }
      }
    }
    if (worldId) {
      this->root().logMap().set(Star::format("server_{}_storage", worldId->get()),
        Star::format("{} active, {}/{} unloaded ({} held)", m_sectorMetadata.size(), unloaded, skipped + unloaded, skipped));
    }
    return {};
  }();
  if (!result) return std::unexpected(invalidate(std::move(result.error()), "WorldStorage exception during tick"));
  return result;
}

std::expected<void, StarException> WorldStorage::unloadAll(bool force) {
  auto result = [&]() -> std::expected<void, StarException> {
    auto autostorageConfigResult1 = this->root().assets();
    if (!autostorageConfigResult1) return std::unexpected(std::move(autostorageConfigResult1.error()));
    auto autostorageConfigResult2 = (*autostorageConfigResult1)->json("/worldstorage.config");
    if (!autostorageConfigResult2) return std::unexpected(std::move(autostorageConfigResult2.error()));
    auto storageConfig = std::move((*autostorageConfigResult2));
    auto sectors = m_sectorMetadata.keys();

    // Entities can do some strange things during unload, such as repeatedly
    // creating new entities during uninit, or setting their bounding box null
    // or being entirely outside of the world geometry.  This limits the number
    // of tries to completely uninit and store all entities before giving up
    // and just letting some entities not be stored.
    if (m_entityMap) {
      auto unsignedforceUnloadTriesResult1 = storageConfig.getUInt("forceUnloadTries");
      if (!unsignedforceUnloadTriesResult1) return std::unexpected(std::move(unsignedforceUnloadTriesResult1.error()));
      unsigned forceUnloadTries = std::move((*unsignedforceUnloadTriesResult1));
      for (unsigned i = 0; i < forceUnloadTries; ++i) {
        for (auto& sector : sectors)
          {
            auto operationResult1 = unloadSectorToLevel(sector, SectorLoadLevel::Tiles, force);
            if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
          }

        if (!force || m_entityMap->size() == 0)
          break;
      }
    }
    for (auto& sector : sectors)
      {
        auto operationResult1 = unloadSectorToLevel(sector, SectorLoadLevel::None, force);
        if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
      }

    return {};
  }();
  if (!result) return std::unexpected(invalidate(std::move(result.error()), "WorldStorage exception during unload"));
  return result;
}

std::expected<void, StarException> WorldStorage::sync() {
  auto result = [&]() -> std::expected<void, StarException> {
    for (auto const& pair : m_sectorMetadata)
      {
        auto operationResult1 = syncSector(pair.first);
        if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
      }
    {
      auto operationResult1 = m_db.commit();
      if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
    }
    return {};
  }();
  if (!result) return std::unexpected(invalidate(std::move(result.error()), "WorldStorage exception during sync"));
  return result;
}

std::expected<WorldChunks, StarException> WorldStorage::readChunks() {
  auto result = [&]() -> std::expected<WorldChunks, StarException> {
    for (auto const& pair : m_sectorMetadata)
      {
        auto operationResult1 = syncSector(pair.first);
        if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
      }

    WorldChunks chunks;
    auto read = m_db.forAll([&chunks](ByteArray k, ByteArray v) -> std::expected<void, StarException> {
        auto added = chunks.add(std::move(k), std::move(v));
        if (!added) return std::unexpected(std::move(added.error()));
        return {};
      });
    if (!read) return std::unexpected(std::move(read.error()));

    return chunks;

  }();
  if (!result) return std::unexpected(invalidate(std::move(result.error()), "WorldStorage exception during readChunks"));
  return result;
}


bool WorldStorage::floatingDungeonWorld() const {
  return m_floatingDungeonWorld;
}

void WorldStorage::setFloatingDungeonWorld(bool floatingDungeonWorld) {
  m_floatingDungeonWorld = floatingDungeonWorld;
}

WorldStorage::TileSectorStore::TileSectorStore()
  : tileSerializationVersion(ServerTile::CurrentSerializationVersion) {}

WorldStorage::SectorMetadata::SectorMetadata()
  : loadLevel(SectorLoadLevel::None), generationLevel(SectorGenerationLevel::None), timeToLive(0.0f) {}

std::expected<ByteArray, StarException> WorldStorage::metadataKey() {
  DataStreamBuffer metadata(5);
  {
    auto operationResult1 = metadata.write(StoreType::Metadata);
    if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
  }
  return metadata.take_data();
}

std::expected<WorldStorage::WorldMetadataStore, StarException> WorldStorage::readWorldMetadata(ByteArray const& data) {
  auto autouncompressedResult1 = uncompressData(data);
  if (!autouncompressedResult1) return std::unexpected(std::move(autouncompressedResult1.error()));
  auto uncompressed = std::move((*autouncompressedResult1));
  DataStreamBuffer ds(std::move(uncompressed));

  WorldMetadataStore metadata;
  {
    auto operationResult1 = ds.read(metadata.worldSize);
    if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
  }
  {
    auto operationResult1 = ds.read(metadata.userMetadata);
    if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
  }
  {
    auto operationResult1 = VersionedJson::readSubVersioning(ds, metadata.userMetadata);
    if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
  }
  return metadata;
}

std::expected<ByteArray, StarException> WorldStorage::writeWorldMetadata(WorldMetadataStore const& metadata) {
  DataStreamBuffer ds;
  {
    auto operationResult1 = ds.write(metadata.worldSize);
    if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
  }
  {
    auto operationResult1 = ds.write(metadata.userMetadata);
    if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
  }
  {
    auto operationResult1 = VersionedJson::writeSubVersioning(ds, metadata.userMetadata);
    if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
  }
  return compressData(ds.data());
}

std::expected<ByteArray, StarException> WorldStorage::entitySectorKey(Sector const& sector) {
  DataStreamBuffer ds(5);
  {
    auto operationResult1 = ds.write(StoreType::EntitySector);
    if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
  }
  {
    auto operationResult1 = ds.cwrite<std::uint16_t>(sector[0]);
    if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
  }
  {
    auto operationResult1 = ds.cwrite<std::uint16_t>(sector[1]);
    if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
  }
  return ds.take_data();
}

std::expected<WorldStorage::EntitySectorStore, StarException> WorldStorage::readEntitySector(ByteArray const& data) {
  auto autouncompressedResult1 = uncompressData(data);
  if (!autouncompressedResult1) return std::unexpected(std::move(autouncompressedResult1.error()));
  auto uncompressed = std::move((*autouncompressedResult1));
  DataStreamBuffer ds(std::move(uncompressed));
  auto autostoreResult1 = ds.read<EntitySectorStore>();
  if (!autostoreResult1) return std::unexpected(std::move(autostoreResult1.error()));
  auto store = std::move((*autostoreResult1));
  for (auto& entity : store) {
    {
      auto operationResult1 = VersionedJson::readSubVersioning(ds, entity);
      if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
    }
  }
  return store;
}

std::expected<ByteArray, StarException> WorldStorage::writeEntitySector(EntitySectorStore const& store) {
  DataStreamBuffer ds;
  {
    auto operationResult1 = ds.write(store);
    if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
  }
  for (auto& entity : store) {
    {
      auto operationResult1 = VersionedJson::writeSubVersioning(ds, entity);
      if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
    }
  }
  return compressData(ds.data());
}

std::expected<ByteArray, StarException> WorldStorage::tileSectorKey(Sector const& sector) {
  DataStreamBuffer ds(5);
  {
    auto operationResult1 = ds.write(StoreType::TileSector);
    if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
  }
  {
    auto operationResult1 = ds.cwrite<std::uint16_t>(sector[0]);
    if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
  }
  {
    auto operationResult1 = ds.cwrite<std::uint16_t>(sector[1]);
    if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
  }
  return ds.take_data();
}

std::expected<WorldStorage::TileSectorStore, StarException> WorldStorage::readTileSector(ByteArray const& data) {
  auto& root = this->root();
  auto automatDatabaseResult1 = root.materialDatabase();
  if (!automatDatabaseResult1) return std::unexpected(std::move(automatDatabaseResult1.error()));
  auto matDatabase = std::move((*automatDatabaseResult1));
  auto autoliqDatabaseResult1 = root.liquidsDatabase();
  if (!autoliqDatabaseResult1) return std::unexpected(std::move(autoliqDatabaseResult1.error()));
  auto liqDatabase = std::move((*autoliqDatabaseResult1));
  auto autostorageConfigResult1 = root.assets();
  if (!autostorageConfigResult1) return std::unexpected(std::move(autostorageConfigResult1.error()));
  auto autostorageConfigResult2 = (*autostorageConfigResult1)->json("/worldstorage.config");
  if (!autostorageConfigResult2) return std::unexpected(std::move(autostorageConfigResult2.error()));
  auto storageConfig = std::move((*autostorageConfigResult2));

  auto autouncompressedResult1 = uncompressData(data);
  if (!autouncompressedResult1) return std::unexpected(std::move(autouncompressedResult1.error()));
  auto uncompressed = std::move((*autouncompressedResult1));
  DataStreamBuffer ds(std::move(uncompressed));
  TileSectorStore store;
  {
    auto operationResult1 = ds.vuread(store.generationLevel);
    if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
  }
  {
    auto operationResult1 = ds.vuread(store.tileSerializationVersion);
    if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
  }

  store.tiles.reset(new TileArray());
  for (std::size_t y = 0; y < WorldSectorSize; ++y) {
    for (std::size_t x = 0; x < WorldSectorSize; ++x) {
      ServerTile tile;
      {
        auto operationResult1 = tile.read(ds, store.tileSerializationVersion);
        if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
      }

      if (!matDatabase->isValidMaterialId(tile.foreground)) {
        auto tileforegroundResult1 = storageConfig.getUInt("replacementMaterialId");
        if (!tileforegroundResult1) return std::unexpected(std::move(tileforegroundResult1.error()));
        tile.foreground = std::move((*tileforegroundResult1));
      }
      if (!matDatabase->isValidMaterialId(tile.background)) {
        auto tilebackgroundResult1 = storageConfig.getUInt("replacementMaterialId");
        if (!tilebackgroundResult1) return std::unexpected(std::move(tilebackgroundResult1.error()));
        tile.background = std::move((*tilebackgroundResult1));
      }
      if (!matDatabase->isValidModId(tile.foregroundMod)) {
        auto tileforegroundModResult1 = storageConfig.getUInt("replacementModId");
        if (!tileforegroundModResult1) return std::unexpected(std::move(tileforegroundModResult1.error()));
        tile.foregroundMod = std::move((*tileforegroundModResult1));
      }
      if (!matDatabase->isValidModId(tile.backgroundMod)) {
        auto tilebackgroundModResult1 = storageConfig.getUInt("replacementModId");
        if (!tilebackgroundModResult1) return std::unexpected(std::move(tilebackgroundModResult1.error()));
        tile.backgroundMod = std::move((*tilebackgroundModResult1));
      }
      if (!liqDatabase->isValidLiquidId(tile.liquid.liquid)) {
        auto LiquidIdreplacementLiquidResult1 = storageConfig.getUInt("replacementLiquidId");
        if (!LiquidIdreplacementLiquidResult1) return std::unexpected(std::move(LiquidIdreplacementLiquidResult1.error()));
        LiquidId replacementLiquid = std::move((*LiquidIdreplacementLiquidResult1));
        if (replacementLiquid == EmptyLiquidId)
          tile.liquid = LiquidStore();
        else
          tile.liquid.liquid = replacementLiquid;
      }

      (*store.tiles)(x, y) = tile;
    }
  }

  return store;
}

std::expected<ByteArray, StarException> WorldStorage::writeTileSector(TileSectorStore const& store) {
  DataStreamBuffer ds;
  {
    auto operationResult1 = ds.vuwrite(store.generationLevel);
    if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
  }
  {
    auto operationResult1 = ds.vuwrite(store.tileSerializationVersion);
    if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
  }
  if constexpr (Star::DebugEnabled) {
    Star::star_assert_failed((store.tiles));
  }
  for (std::size_t y = 0; y < WorldSectorSize; ++y) {
    for (std::size_t x = 0; x < WorldSectorSize; ++x)
      {
        auto written = (*store.tiles)(x, y).write(ds);
        if (!written) return std::unexpected(std::move(written.error()));
      }
  }
  return compressData(ds.take_data());
}

std::expected<ByteArray, StarException> WorldStorage::uniqueIndexKey(String const& uniqueId) {
  DataStreamBuffer ds(5);
  {
    auto operationResult1 = ds.write(StoreType::UniqueIndex);
    if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
  }
  {
    auto index_written = ds.write(xx_hash32(uniqueId));
    if (!index_written)
      return std::unexpected(std::move(index_written.error()));
  }
  return ds.take_data();
}

std::expected<WorldStorage::UniqueIndexStore, StarException> WorldStorage::readUniqueIndexStore(ByteArray const& data) {
  auto autouncompressedResult1 = uncompressData(data);
  if (!autouncompressedResult1) return std::unexpected(std::move(autouncompressedResult1.error()));
  auto uncompressed = std::move((*autouncompressedResult1));
  return DataStreamBuffer::deserialize_map_container<UniqueIndexStore>(
    uncompressed.span(),
    [](DataStream& ds, String& key, SectorAndPosition& value) -> std::expected<void, StarException> {
      if (auto key_read = ds.read(key); !key_read)
        return std::unexpected(std::move(key_read.error()));
      if (auto x_read = ds.cread<std::uint16_t>(value.first[0]); !x_read)
        return std::unexpected(std::move(x_read.error()));
      if (auto y_read = ds.cread<std::uint16_t>(value.first[1]); !y_read)
        return std::unexpected(std::move(y_read.error()));
      if (auto position_read = ds.read(value.second); !position_read)
        return std::unexpected(std::move(position_read.error()));
      return {};
    });
}

std::expected<ByteArray, StarException> WorldStorage::writeUniqueIndexStore(UniqueIndexStore const& store) {
  auto serialized = DataStreamBuffer::serialize_map_container(
    store, [](DataStream& ds, String const& key, SectorAndPosition const& value) -> std::expected<void, StarException> {
      if (auto key_written = ds.write(key); !key_written)
        return std::unexpected(std::move(key_written.error()));
      if (auto x_written = ds.cwrite<std::uint16_t>(value.first[0]); !x_written)
        return std::unexpected(std::move(x_written.error()));
      if (auto y_written = ds.cwrite<std::uint16_t>(value.first[1]); !y_written)
        return std::unexpected(std::move(y_written.error()));
      if (auto position_written = ds.write(value.second); !position_written)
        return std::unexpected(std::move(position_written.error()));
      return {};
    });
  if (!serialized) return std::unexpected(std::move(serialized.error()));
  return compressData(*serialized);
}

std::expected<ByteArray, StarException> WorldStorage::sectorUniqueKey(Sector const& sector) {
  DataStreamBuffer ds(5);
  {
    auto operationResult1 = ds.write(StoreType::SectorUniques);
    if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
  }
  {
    auto operationResult1 = ds.cwrite<std::uint16_t>(sector[0]);
    if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
  }
  {
    auto operationResult1 = ds.cwrite<std::uint16_t>(sector[1]);
    if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
  }
  return ds.take_data();
}

std::expected<WorldStorage::SectorUniqueStore, StarException> WorldStorage::readSectorUniqueStore(ByteArray const& data) {
  auto autouncompressedResult1 = uncompressData(data);
  if (!autouncompressedResult1) return std::unexpected(std::move(autouncompressedResult1.error()));
  auto uncompressed = std::move((*autouncompressedResult1));
  return DataStreamBuffer::deserialize<SectorUniqueStore>(uncompressed.span());
}

std::expected<ByteArray, StarException> WorldStorage::writeSectorUniqueStore(SectorUniqueStore const& store) {
  auto autoserializedResult1 = DataStreamBuffer::serialize(store);
  if (!autoserializedResult1) return std::unexpected(std::move(autoserializedResult1.error()));
  auto serialized = std::move((*autoserializedResult1));
  return compressData(serialized);
}

std::expected<void, StarException> WorldStorage::openDatabase(BTreeDatabase& db, std::shared_ptr<IODevice> device) {
  {
    auto operationResult1 = db.setContentIdentifier("World4");
    if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
  }
  {
    auto operationResult1 = db.setKeySize(5);
    if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
  }
  {
    auto operationResult1 = db.setIODevice(std::move(device));
    if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
  }
  {
    auto operationResult1 = db.setBlockSize(2048);
    if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
  }
  {
    auto operationResult1 = db.setAutoCommit(false);
    if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
  }
  {
    auto operationResult1 = db.open();
    if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
  }

  if (db.contentIdentifier() != "World4" || db.keySize() != 5)
    return std::unexpected(WorldStorageException::format("World database format is too old or unrecognized!"));
  return {};
}

WorldStorage::WorldStorage(Root& root) : m_root(root), m_db(root.logger()) {}

std::expected<void, StarException> WorldStorage::initialize() {
  auto autostorageConfigResult1 = this->root().assets();
  if (!autostorageConfigResult1) return std::unexpected(std::move(autostorageConfigResult1.error()));
  auto autostorageConfigResult2 = (*autostorageConfigResult1)->json("/worldstorage.config");
  if (!autostorageConfigResult2) return std::unexpected(std::move(autostorageConfigResult2.error()));
  auto storageConfig = std::move((*autostorageConfigResult2));
  auto m_sectorTimeToLiveResult1 = storageConfig.get("sectorTimeToLive");
  if (!m_sectorTimeToLiveResult1) return std::unexpected(std::move(m_sectorTimeToLiveResult1.error()));
  auto m_sectorTimeToLiveResult2 = jsonToVec2F((*m_sectorTimeToLiveResult1));
  if (!m_sectorTimeToLiveResult2) return std::unexpected(std::move(m_sectorTimeToLiveResult2.error()));
  m_sectorTimeToLive = std::move((*m_sectorTimeToLiveResult2));
  auto m_generationQueueTimeToLiveResult1 = storageConfig.getFloat("generationQueueTimeToLive");
  if (!m_generationQueueTimeToLiveResult1) return std::unexpected(std::move(m_generationQueueTimeToLiveResult1.error()));
  m_generationQueueTimeToLive = std::move((*m_generationQueueTimeToLiveResult1));
  return {};
}

StarException WorldStorage::invalidate(StarException error, std::string context) {
  auto rollback = m_db.rollback();
  if (!rollback) {
    error = rollback.error().with_context(Star::format("Rollback failed after {}", error.what()));
  } else {
    auto closed = m_db.close();
    if (!closed) error = closed.error().with_context(Star::format("Close failed after {}", error.what()));
  }
  return WorldStorageException(std::move(context), ExceptionDiagnostic(error));
}

bool WorldStorage::belongsInSector(Sector const& sector, Vec2F const& position) const {
  WorldGeometry geometry(m_tileArray->size());
  return RectF(m_tileArray->sectorRegion(sector)).belongs(geometry.limit(position));
}

std::expected<float, StarException> WorldStorage::randomizedSectorTTL() const {
  return this->root().random().randf(m_sectorTimeToLive[0], m_sectorTimeToLive[1]);
}

std::expected<std::pair<bool, std::size_t>, StarException> WorldStorage::generateSectorToLevel(Sector const& sector, SectorGenerationLevel targetGenerationLevel, std::size_t sectorGenerationLevelLimit) {
  if (!m_tileArray->sectorValid(sector))
    return std::pair<bool, std::size_t>{false, 0};

  {
    auto operationResult1 = loadSectorToLevel(sector, SectorLoadLevel::Loaded);
    if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
  }

  auto& metadata = m_sectorMetadata[sector];

  if (targetGenerationLevel == SectorGenerationLevel::Complete && metadata.generationLevel == SectorGenerationLevel::Terraform) {
    {
      auto operationResult1 = m_generatorFacade->terraformSector(*this, sector);
      if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
    }
    metadata.generationLevel = SectorGenerationLevel::Complete;
    auto metadatatimeToLiveResult1 = randomizedSectorTTL();
    if (!metadatatimeToLiveResult1) return std::unexpected(std::move(metadatatimeToLiveResult1.error()));
    metadata.timeToLive = std::move((*metadatatimeToLiveResult1));
    return std::pair<bool, std::size_t>{true, 1};
  }

  if (metadata.generationLevel >= targetGenerationLevel)
    return std::pair<bool, std::size_t>{true, 0};
  auto metadatatimeToLiveResult1 = randomizedSectorTTL();
  if (!metadatatimeToLiveResult1) return std::unexpected(std::move(metadatatimeToLiveResult1.error()));
  metadata.timeToLive = std::move((*metadatatimeToLiveResult1));

  std::size_t totalGeneratedLevels = 0;
  for (std::uint8_t i = static_cast<std::uint8_t>(metadata.generationLevel) + 1;
       i <= static_cast<std::uint8_t>(targetGenerationLevel); ++i) {
    auto currentGeneration = static_cast<SectorGenerationLevel>(i);
    auto stepDownGeneration = static_cast<SectorGenerationLevel>(i - 1);

    if (stepDownGeneration != SectorGenerationLevel::None) {
      auto autoadjacentResult1 = adjacentSectors(sector);
      if (!autoadjacentResult1) return std::unexpected(std::move(autoadjacentResult1.error()));
      auto adjacent = std::move((*autoadjacentResult1));
      for (auto adjacentSector : adjacent) {
        auto autopResult1 = generateSectorToLevel(adjacentSector, stepDownGeneration, sectorGenerationLevelLimit - totalGeneratedLevels);
        if (!autopResult1) return std::unexpected(std::move(autopResult1.error()));
        auto p = std::move((*autopResult1));
        totalGeneratedLevels += p.second;
        if (!p.first || totalGeneratedLevels >= sectorGenerationLevelLimit)
          return std::pair<bool, std::size_t>{false, totalGeneratedLevels};
      }
    }
    {
      auto operationResult1 = m_generatorFacade->generateSectorLevel(*this, sector, currentGeneration);
      if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
    }
    metadata.generationLevel = currentGeneration;

    ++totalGeneratedLevels;
    if (totalGeneratedLevels >= sectorGenerationLevelLimit)
      return std::pair<bool, std::size_t>{metadata.generationLevel == targetGenerationLevel, totalGeneratedLevels};
  }

  return std::pair<bool, std::size_t>{true, totalGeneratedLevels};
}

std::expected<void, StarException> WorldStorage::loadSectorToLevel(Sector const& sector, SectorLoadLevel targetLoadLevel) {
  if (!m_tileArray->sectorValid(sector)) return {};
  auto autoentityFactoryResult1 = this->root().entityFactory();
  if (!autoentityFactoryResult1) return std::unexpected(std::move(autoentityFactoryResult1.error()));
  auto entityFactory = std::move((*autoentityFactoryResult1));
  auto& metadata = m_sectorMetadata[sector];
  if (metadata.loadLevel >= targetLoadLevel) return {};
  auto metadatatimeToLiveResult1 = randomizedSectorTTL();
  if (!metadatatimeToLiveResult1) return std::unexpected(std::move(metadatatimeToLiveResult1.error()));
  metadata.timeToLive = std::move((*metadatatimeToLiveResult1));
  for (std::uint8_t i = static_cast<std::uint8_t>(metadata.loadLevel) + 1;
       i <= static_cast<std::uint8_t>(targetLoadLevel); ++i) {
    auto currentLoad = static_cast<SectorLoadLevel>(i);
    auto stepDownLoad = static_cast<SectorLoadLevel>(i - 1);
    if (stepDownLoad != SectorLoadLevel::None) {
      auto autoadjacentResult1 = adjacentSectors(sector);
      if (!autoadjacentResult1) return std::unexpected(std::move(autoadjacentResult1.error()));
      auto adjacent = std::move((*autoadjacentResult1));
      for (auto adjacentSector : adjacent) {
        {
          auto operationResult1 = loadSectorToLevel(adjacentSector, stepDownLoad);
          if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
        }
      }
    }
    if (currentLoad == SectorLoadLevel::Tiles) {
      auto autofoundResult1 = tileSectorKey(sector);
      if (!autofoundResult1) return std::unexpected(std::move(autofoundResult1.error()));
      auto autofoundResult2 = m_db.find((*autofoundResult1));
      if (!autofoundResult2) return std::unexpected(std::move(autofoundResult2.error()));
      auto found = std::move((*autofoundResult2));
      if (found) {
        auto autosectorStoreResult1 = readTileSector(*found);
        if (!autosectorStoreResult1) return std::unexpected(std::move(autosectorStoreResult1.error()));
        auto sectorStore = std::move((*autosectorStoreResult1));
        m_tileArray->loadSector(sector, std::move(sectorStore.tiles));
        metadata.generationLevel = sectorStore.generationLevel;
      } else if (!m_tileArray->sectorLoaded(sector)) {
        m_tileArray->loadDefaultSector(sector);
      }
      metadata.loadLevel = currentLoad;
      {
        auto operationResult1 = m_generatorFacade->sectorLoadLevelChanged(*this, sector, currentLoad);
        if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
      }
    } else if (currentLoad == SectorLoadLevel::Entities) {
      List<std::shared_ptr<Entity>> addedEntities;
      auto autofoundResult1 = entitySectorKey(sector);
      if (!autofoundResult1) return std::unexpected(std::move(autofoundResult1.error()));
      auto autofoundResult2 = m_db.find((*autofoundResult1));
      if (!autofoundResult2) return std::unexpected(std::move(autofoundResult2.error()));
      auto found = std::move((*autofoundResult2));
      if (found) {
        auto autosectorStoreResult1 = readEntitySector(*found);
        if (!autosectorStoreResult1) return std::unexpected(std::move(autosectorStoreResult1.error()));
        auto sectorStore = std::move((*autosectorStoreResult1));
        for (auto const& entityStore : sectorStore) {
          auto entity = entityFactory->loadVersionedEntity(entityStore);
          if (entity) {
            addedEntities.push_back(std::move(*entity));
          } else {
            auto logged = this->root().logger().warnException(Star::format("Failed to deserialize entity in sector {}", sector), ExceptionDiagnostic(entity.error()));
            if (!logged) return std::unexpected(std::move(logged.error()));
            if (this->root().logger().loggable(LogLevel::Debug)) {
              auto stored = entityStore.toJson().repr();
              if (!stored) return std::unexpected(std::move(stored.error()));
              if (auto details = this->root().logger().debug("Stored entity: {}", *stored); !details)
                return std::unexpected(std::move(details.error()));
            }
          }
        }
      }
      UniqueIndexStore readUniques;
      for (auto const& entity : addedEntities) {
        {
          auto operationResult1 = m_entityMap->reserveEntityId();
          if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
          auto operationResult2 = m_generatorFacade->initEntity(*this, (*operationResult1), entity);
          if (!operationResult2) return std::unexpected(std::move(operationResult2.error()));
        }
        {
          auto operationResult1 = m_entityMap->addEntity(entity);
          if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
        }
        if (auto uniqueId = entity->uniqueId()) {
          auto added = readUniques.add(*uniqueId, {sector, entity->position()});
          if (!added) return std::unexpected(std::move(added.error()));
        }
      }
      {
        auto operationResult1 = updateSectorUniques(sector, readUniques);
        if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
      }
      metadata.loadLevel = currentLoad;
      {
        auto operationResult1 = m_generatorFacade->sectorLoadLevelChanged(*this, sector, currentLoad);
        if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
      }
    }
  }
  return {};
}

std::expected<bool, StarException> WorldStorage::unloadSectorToLevel(Sector const& sector, SectorLoadLevel targetLoadLevel, bool force) {
  if (!m_tileArray->sectorValid(sector) || targetLoadLevel == SectorLoadLevel::Loaded)
    return true;

  auto& metadata = m_sectorMetadata[sector];
  bool entitiesOverlap = false;
  if (m_entityMap) {
    auto autoentityFactoryResult1 = this->root().entityFactory();
    if (!autoentityFactoryResult1) return std::unexpected(std::move(autoentityFactoryResult1.error()));
    auto entityFactory = std::move((*autoentityFactoryResult1));
    List<std::shared_ptr<Entity>> entitiesToStore;
    List<std::shared_ptr<Entity>> entitiesToRemove;
    auto autosectorEntitiesResult1 = m_entityMap->entityQuery(RectF(m_tileArray->sectorRegion(sector)));
    if (!autosectorEntitiesResult1) return std::unexpected(std::move(autosectorEntitiesResult1.error()));
    auto sectorEntities = std::move((*autosectorEntitiesResult1));

    for (auto& entity : sectorEntities) {
      // Only store / remove entities who belong to this sector.  If an entity
      // overlaps with this sector but does not belong to it, we may not want to
      // completely unload it.
      auto position = entity->position();
      if (!belongsInSector(sector, position)) {
        if (auto entitySector = sectorForPosition(Vec2I(position))) {
          if (auto p = m_sectorMetadata.find_ref(*entitySector))
            entitiesOverlap |= p->get().timeToLive > 0.0f;
        }
        continue;
      }

      auto boolkeepAliveResult1 = m_generatorFacade->entityKeepAlive(*this, entity);
      if (!boolkeepAliveResult1) return std::unexpected(std::move(boolkeepAliveResult1.error()));
      bool keepAlive = std::move((*boolkeepAliveResult1));
      if (keepAlive && !force)
        return false;
      auto autopersistentResult1 = m_generatorFacade->entityPersistent(*this, entity);
      if (!autopersistentResult1) return std::unexpected(std::move(autopersistentResult1.error()));
      auto persistent = std::move((*autopersistentResult1));

      if (persistent)
        entitiesToStore.push_back(std::move(entity));
      else
        entitiesToRemove.push_back(std::move(entity));
    }

    for (auto const& entity : entitiesToRemove) {
      m_entityMap->removeEntity(entity->entityId());
      {
        auto operationResult1 = m_generatorFacade->destructEntity(*this, entity);
        if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
      }
    }

    if (metadata.loadLevel == SectorLoadLevel::Entities || !entitiesToStore.empty()) {
      EntitySectorStore sectorStore;

      // If our current load level indicates that we might have entities that are
      // not loaded, we need to load and merge with them, otherwise we should be
      // overwriting them.
      if (metadata.loadLevel < SectorLoadLevel::Entities) {
        auto key = entitySectorKey(sector);
        if (!key) return std::unexpected(std::move(key.error()));
        auto found = m_db.find(*key);
        if (!found) return std::unexpected(std::move(found.error()));
        if (*found) {
          auto stored = readEntitySector(**found);
          if (!stored) return std::unexpected(std::move(stored.error()));
          sectorStore = std::move(*stored);
        }
      }

      UniqueIndexStore storedUniques;
      for (auto const& entity : entitiesToStore) {
        m_entityMap->removeEntity(entity->entityId());
        {
          auto operationResult1 = m_generatorFacade->destructEntity(*this, entity);
          if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
        }
        auto position = entity->position();
        if (auto uniqueId = entity->uniqueId()) {
          auto added = storedUniques.add(*uniqueId, {sector, position});
          if (!added) return std::unexpected(std::move(added.error()));
        }
        auto autostoredEntityResult1 = entityFactory->storeVersionedEntity(entity);
        if (!autostoredEntityResult1) return std::unexpected(std::move(autostoredEntityResult1.error()));
        auto storedEntity = std::move((*autostoredEntityResult1));
        sectorStore.push_back(std::move(storedEntity));
      }
      {
        auto operationResult1 = entitySectorKey(sector);
        if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
        auto operationResult2 = writeEntitySector(sectorStore);
        if (!operationResult2) return std::unexpected(std::move(operationResult2.error()));
        auto operationResult3 = m_db.insert((*operationResult1), (*operationResult2));
        if (!operationResult3) return std::unexpected(std::move(operationResult3.error()));
      }
      if (metadata.loadLevel < SectorLoadLevel::Entities)
        {
          auto operationResult1 = mergeSectorUniques(sector, storedUniques);
          if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
        }
      else
        {
          auto operationResult1 = updateSectorUniques(sector, storedUniques);
          if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
        }

      if (metadata.loadLevel == SectorLoadLevel::Entities) {
        metadata.loadLevel = SectorLoadLevel::Tiles;
        {
          auto operationResult1 = m_generatorFacade->sectorLoadLevelChanged(*this, sector, SectorLoadLevel::Tiles);
          if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
        }
      }
    }
  }

  if (targetLoadLevel == SectorLoadLevel::None) {
    if (metadata.loadLevel > SectorLoadLevel::None && !entitiesOverlap) {
      TileSectorStore sectorStore;
      sectorStore.tiles = m_tileArray->unloadSector(sector);
      sectorStore.generationLevel = metadata.generationLevel;
      {
        auto operationResult1 = tileSectorKey(sector);
        if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
        auto operationResult2 = writeTileSector(sectorStore);
        if (!operationResult2) return std::unexpected(std::move(operationResult2.error()));
        auto operationResult3 = m_db.insert((*operationResult1), (*operationResult2));
        if (!operationResult3) return std::unexpected(std::move(operationResult3.error()));
      }
      m_sectorMetadata.remove(sector);
      {
        auto operationResult1 = m_generatorFacade->sectorLoadLevelChanged(*this, sector, SectorLoadLevel::None);
        if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
      }
      return true;
    }
    return false;
  }
  return true;
}

std::expected<void, StarException> WorldStorage::syncSector(Sector const& sector) {
  if (!m_tileArray->sectorValid(sector))
    return {};
  auto autoentityFactoryResult1 = this->root().entityFactory();
  if (!autoentityFactoryResult1) return std::unexpected(std::move(autoentityFactoryResult1.error()));
  auto entityFactory = std::move((*autoentityFactoryResult1));
  auto& metadata = m_sectorMetadata[sector];

  // Only sync the levels that we know are loaded.  It is possible that this
  // sector is at load level < Entities but has zombie entities in it,  but
  // storing those without unloading them will lead to duplication.  Zombie
  // entities will be unloaded in update eventually anyway.

  if (metadata.loadLevel >= SectorLoadLevel::Entities) {
    EntitySectorStore sectorStore;
    UniqueIndexStore storedUniques;
    auto autosectorEntitiesResult1 = m_entityMap->entityQuery(RectF(m_tileArray->sectorRegion(sector)));
    if (!autosectorEntitiesResult1) return std::unexpected(std::move(autosectorEntitiesResult1.error()));
    auto sectorEntities = std::move((*autosectorEntitiesResult1));
    for (auto const& entity : sectorEntities) {
      if (!belongsInSector(sector, entity->position()))
        continue;
      auto autopersistentResult1 = m_generatorFacade->entityPersistent(*this, entity);
      if (!autopersistentResult1) return std::unexpected(std::move(autopersistentResult1.error()));
      auto persistent = std::move((*autopersistentResult1));

      if (persistent) {
        if (auto uniqueId = entity->uniqueId()) {
          auto added = storedUniques.add(*uniqueId, {sector, entity->position()});
          if (!added) return std::unexpected(std::move(added.error()));
        }
        auto autostoredEntityResult1 = entityFactory->storeVersionedEntity(entity);
        if (!autostoredEntityResult1) return std::unexpected(std::move(autostoredEntityResult1.error()));
        auto storedEntity = std::move((*autostoredEntityResult1));
        sectorStore.push_back(std::move(storedEntity));
      }
    }
    {
      auto operationResult1 = entitySectorKey(sector);
      if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
      auto operationResult2 = writeEntitySector(sectorStore);
      if (!operationResult2) return std::unexpected(std::move(operationResult2.error()));
      auto operationResult3 = m_db.insert((*operationResult1), (*operationResult2));
      if (!operationResult3) return std::unexpected(std::move(operationResult3.error()));
    }
    {
      auto operationResult1 = updateSectorUniques(sector, storedUniques);
      if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
    }
  }

  if (metadata.loadLevel >= SectorLoadLevel::Tiles) {
    TileSectorStore sectorStore;
    sectorStore.tiles = m_tileArray->copySector(sector);
    sectorStore.generationLevel = metadata.generationLevel;
    {
      auto operationResult1 = tileSectorKey(sector);
      if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
      auto operationResult2 = writeTileSector(sectorStore);
      if (!operationResult2) return std::unexpected(std::move(operationResult2.error()));
      auto operationResult3 = m_db.insert((*operationResult1), (*operationResult2));
      if (!operationResult3) return std::unexpected(std::move(operationResult3.error()));
    }
  }
  return {};
}

std::expected<List<WorldStorage::Sector>, StarException> WorldStorage::adjacentSectors(Sector const& sector) const {
  auto tiles = m_tileArray->sectorRegion(sector);
  return m_tileArray->validSectorsFor(tiles.padded(WorldSectorSize));
}

std::expected<void, StarException> WorldStorage::updateSectorUniques(Sector const& sector, UniqueIndexStore const& sectorUniques) {
  auto autofoundResult1 = sectorUniqueKey(sector);
  if (!autofoundResult1) return std::unexpected(std::move(autofoundResult1.error()));
  auto autofoundResult2 = m_db.find((*autofoundResult1));
  if (!autofoundResult2) return std::unexpected(std::move(autofoundResult2.error()));
  auto found = std::move((*autofoundResult2));
  if (found) {
    auto autooldSectorUniquesResult1 = readSectorUniqueStore(*found);
    if (!autooldSectorUniquesResult1) return std::unexpected(std::move(autooldSectorUniquesResult1.error()));
    auto oldSectorUniques = std::move((*autooldSectorUniquesResult1));
    for (auto const& uniqueId : oldSectorUniques) {
      if (!sectorUniques.contains(uniqueId)) {
        {
          auto operationResult1 = removeUniqueIndexEntry(uniqueId, sector);
          if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
        }
      }
    }
  }
  for (auto const& pair : sectorUniques) {
    {
      auto operationResult1 = setUniqueIndexEntry(pair.first, pair.second);
      if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
    }
  }
  if (sectorUniques.empty()) {
    {
      auto operationResult1 = sectorUniqueKey(sector);
      if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
      auto operationResult2 = m_db.remove((*operationResult1));
      if (!operationResult2) return std::unexpected(std::move(operationResult2.error()));
    }
  } else {
    {
      auto operationResult1 = sectorUniqueKey(sector);
      if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
      auto operationResult2 = writeSectorUniqueStore(HashSet<String>::from(sectorUniques.keys()));
      if (!operationResult2) return std::unexpected(std::move(operationResult2.error()));
      auto operationResult3 = m_db.insert((*operationResult1), (*operationResult2));
      if (!operationResult3) return std::unexpected(std::move(operationResult3.error()));
    }
  }
  return {};
}

std::expected<void, StarException> WorldStorage::mergeSectorUniques(Sector const& sector, UniqueIndexStore const& sectorUniques) {
  SectorUniqueStore sectorUniqueStore;
  auto autofoundResult1 = sectorUniqueKey(sector);
  if (!autofoundResult1) return std::unexpected(std::move(autofoundResult1.error()));
  auto autofoundResult2 = m_db.find((*autofoundResult1));
  if (!autofoundResult2) return std::unexpected(std::move(autofoundResult2.error()));
  auto found = std::move((*autofoundResult2));
  if (found) {
    auto sectorUniqueStoreResult1 = readSectorUniqueStore(*found);
    if (!sectorUniqueStoreResult1) return std::unexpected(std::move(sectorUniqueStoreResult1.error()));
    sectorUniqueStore = std::move((*sectorUniqueStoreResult1));
  }
  for (auto const& pair : sectorUniques) {
    {
      auto operationResult1 = setUniqueIndexEntry(pair.first, pair.second);
      if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
    }
    sectorUniqueStore.add(pair.first);
  }
  if (sectorUniqueStore.empty()) {
    {
      auto operationResult1 = sectorUniqueKey(sector);
      if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
      auto operationResult2 = m_db.remove((*operationResult1));
      if (!operationResult2) return std::unexpected(std::move(operationResult2.error()));
    }
  } else {
    {
      auto operationResult1 = sectorUniqueKey(sector);
      if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
      auto operationResult2 = writeSectorUniqueStore(sectorUniqueStore);
      if (!operationResult2) return std::unexpected(std::move(operationResult2.error()));
      auto operationResult3 = m_db.insert((*operationResult1), (*operationResult2));
      if (!operationResult3) return std::unexpected(std::move(operationResult3.error()));
    }
  }
  return {};
}

std::expected<std::optional<WorldStorage::SectorAndPosition>, StarException> WorldStorage::getUniqueIndexEntry(String const& uniqueId) {
  auto autofoundResult1 = uniqueIndexKey(uniqueId);
  if (!autofoundResult1) return std::unexpected(std::move(autofoundResult1.error()));
  auto autofoundResult2 = m_db.find((*autofoundResult1));
  if (!autofoundResult2) return std::unexpected(std::move(autofoundResult2.error()));
  auto found = std::move((*autofoundResult2));
  if (found) {
    auto autoindexResult1 = readUniqueIndexStore(*found);
    if (!autoindexResult1) return std::unexpected(std::move(autoindexResult1.error()));
    auto index = std::move((*autoindexResult1));
    return index.maybe(uniqueId);
  }
  return {};
}

std::expected<void, StarException> WorldStorage::setUniqueIndexEntry(String const& uniqueId, SectorAndPosition const& sectorAndPosition) {
  UniqueIndexStore uniqueIndex;
  auto autofoundResult1 = uniqueIndexKey(uniqueId);
  if (!autofoundResult1) return std::unexpected(std::move(autofoundResult1.error()));
  auto autofoundResult2 = m_db.find((*autofoundResult1));
  if (!autofoundResult2) return std::unexpected(std::move(autofoundResult2.error()));
  auto found = std::move((*autofoundResult2));
  if (found) {
    auto uniqueIndexResult1 = readUniqueIndexStore(*found);
    if (!uniqueIndexResult1) return std::unexpected(std::move(uniqueIndexResult1.error()));
    uniqueIndex = std::move((*uniqueIndexResult1));
  }
  auto inserted = uniqueIndex.insert(uniqueId, sectorAndPosition);
  if (!inserted.second) {
    if (inserted.first->second == sectorAndPosition) return {};
    inserted.first->second = sectorAndPosition;
  }
  {
    auto operationResult1 = uniqueIndexKey(uniqueId);
    if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
    auto operationResult2 = writeUniqueIndexStore(uniqueIndex);
    if (!operationResult2) return std::unexpected(std::move(operationResult2.error()));
    auto operationResult3 = m_db.insert((*operationResult1), (*operationResult2));
    if (!operationResult3) return std::unexpected(std::move(operationResult3.error()));
  }
  return {};
}

std::expected<void, StarException> WorldStorage::removeUniqueIndexEntry(String const& uniqueId, Sector const& sector) {
  auto autofoundResult1 = uniqueIndexKey(uniqueId);
  if (!autofoundResult1) return std::unexpected(std::move(autofoundResult1.error()));
  auto autofoundResult2 = m_db.find((*autofoundResult1));
  if (!autofoundResult2) return std::unexpected(std::move(autofoundResult2.error()));
  auto found = std::move((*autofoundResult2));
  if (found) {
    auto autouniqueIndexResult1 = readUniqueIndexStore(*found);
    if (!autouniqueIndexResult1) return std::unexpected(std::move(autouniqueIndexResult1.error()));
    auto uniqueIndex = std::move((*autouniqueIndexResult1));
    if (auto position = uniqueIndex.maybe(uniqueId)) {
      if (position->first == sector) {
        uniqueIndex.remove(uniqueId);
        if (uniqueIndex.empty()) {
          {
            auto operationResult1 = uniqueIndexKey(uniqueId);
            if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
            auto operationResult2 = m_db.remove((*operationResult1));
            if (!operationResult2) return std::unexpected(std::move(operationResult2.error()));
          }
        } else {
          {
            auto operationResult1 = uniqueIndexKey(uniqueId);
            if (!operationResult1) return std::unexpected(std::move(operationResult1.error()));
            auto operationResult2 = writeUniqueIndexStore(uniqueIndex);
            if (!operationResult2) return std::unexpected(std::move(operationResult2.error()));
            auto operationResult3 = m_db.insert((*operationResult1), (*operationResult2));
            if (!operationResult3) return std::unexpected(std::move(operationResult3.error()));
          }
        }
      }
    }
  }
  return {};
}

}

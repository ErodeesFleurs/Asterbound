import star.output_proxy;
import star.memory;
import std;
import star.xx_hash;
import star.json;
import star.ordered_map;
import star.bi_map;
import star.data_stream;
import star.thread;
import star.byte_array;
import star.id_map;
import star.set;
import star.btree;
import star.lru_cache;
import star.data_stream_devices;
import star.btree_database;
import star.casting;
import star.ordered_set;
import star.list;
import star.game_types;
import star.rpc_promise;
import star.random;
import star.perlin;
import star.weighted_pool;
import star.strong_typedef;
import star.worker_pool;

import star.sector_array_2d;
import star.color;
import star.image_processing;
import star.hash;
import star.string_view;
import star.directives;
import star.asset_path;
import star.file;
import star.logging;
import star.time;
import star.format;
import star.version;
import star.a_star;
import star.audio;
import star.map;
import star.string;
import star.either;
import star.periodic_function;
import star.io_device;
import star.ref_ptr;
import star.listener;
import star.text;

import star.lua_root;
import star.uuid;
import star.damage_types;
import star.world_geometry;
import star.status_types;
import star.damage;
import star.light_source;
import star.entity;
import star.collision_block;
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
import star.item_descriptor;
import star.quest_descriptor;
import star.interactive_entity;
import star.tile_entity;
import star.inspectable_entity;
import star.plant;
import star.plant_database;
import star.biome_placement;
import star.world_storage;
import star.configuration;
import star.drawable;
import star.animated_part_set;
import star.mixer;
import star.networked_animator;
import star.humanoid;
import star.tile_modification;
import star.world;
import star.physics_entity;
import star.anchorable_entity;
import star.mobile_entity;
import star.actor_entity;
import star.tool_user_entity;
import star.lounging_entities;
import star.chat_action;
import star.chatty_entity;
import star.emote_entity;
import star.portrait_entity;
import star.damage_bar_entity;
import star.nametag_entity;
import star.inventory_types;
import star.movement_controller;
import star.platformer_astar_types;
import star.game_timers;
import star.actor_movement_controller;
import star.ai_types;
import star.entity_rendering_types;
import star.entity_rendering;
import star.player_types;
import star.lua_components;
import star.lua_actor_movement_component;
import star.lua_animation_component;
import star.radio_message_database;
import star.player;
import star.asset_source;
import star.assets;
import star.root_base;
import star.root;

import star.versioning_database;
import star.player_storage;
import star.player_factory;
import star.entity_factory;

namespace Star {

PlayerStorage::PlayerStorage(Root& root) : m_root(root) {}

std::expected<std::shared_ptr<PlayerStorage>, StarException> PlayerStorage::create(Root& root, String const& storageDir) {
  auto storage = std::shared_ptr<PlayerStorage>(new PlayerStorage(root));
  auto initialized = storage->initialize(storageDir);
  if (!initialized)
    return std::unexpected(std::move(initialized.error()));
  storage->m_initialized = true;
  return storage;
}

std::expected<void, StarException> PlayerStorage::initialize(String const& storageDir) {
  m_storageDirectory = storageDir;
  m_backupDirectory = File::relative_to(m_storageDirectory, "backup");
  auto directory = File::is_directory(m_storageDirectory);
  if (!directory)
    return std::unexpected(std::move(directory.error()));
  if (!*directory) {
    auto logged = m_root.logger().info("Creating player storage directory");
    if (!logged)
      return std::unexpected(std::move(logged.error()));
    return File::create_directory(m_storageDirectory);
  }

  auto configuration = m_root.configuration();
  if (!configuration)
    return std::unexpected(std::move(configuration.error()));
  auto clearSetting = (*configuration)->get("clearPlayerFiles");
  if (!clearSetting)
    return std::unexpected(std::move(clearSetting.error()));
  auto clearPlayerFiles = clearSetting->toBool();
  if (!clearPlayerFiles)
    return std::unexpected(std::move(clearPlayerFiles.error()));
  auto files = File::directory_entries(m_storageDirectory);
  if (!files)
    return std::unexpected(std::move(files.error()));
  if (*clearPlayerFiles) {
    auto logged = m_root.logger().info("Clearing all player files");
    if (!logged)
      return std::unexpected(std::move(logged.error()));
    for (auto const& file : *files) {
      if (!file.second) {
        auto removed = File::remove(File::relative_to(m_storageDirectory, file.first));
        if (!removed)
          return std::unexpected(std::move(removed.error()));
      }
    }
  } else {
    auto entityFactory = m_root.entityFactory();
    if (!entityFactory)
      return std::unexpected(std::move(entityFactory.error()));
    for (auto const& file : *files) {
      if (file.second)
        continue;
      auto playerSuffix = file.first.ends_with(".player");
      if (!playerSuffix)
        return std::unexpected(std::move(playerSuffix.error()));
      if (!*playerSuffix)
        continue;

      String filename = File::relative_to(m_storageDirectory, file.first);
      std::optional<std::pair<Uuid, std::reference_wrapper<String const>>> duplicate;
      auto loaded = [&]() -> std::expected<void, StarException> {
        auto json = VersionedJson::read_file(filename);
        if (!json)
          return std::unexpected(std::move(json.error()));
        auto uuidText = json->content.getString("uuid");
        if (!uuidText)
          return std::unexpected(std::move(uuidText.error()));
        auto uuid = Uuid::create(*uuidText);
        if (!uuid)
          return std::unexpected(std::move(uuid.error()));
        auto parts = file.first.rsplit('.', 1);
        if (!parts)
          return std::unexpected(std::move(parts.error()));
        auto basename = parts->at(0);
        if (!basename)
          return std::unexpected(std::move(basename.error()));
        if (m_playerFileNames.insert(*uuid, basename->get())) {
          auto data = (*entityFactory)->loadVersionedJson(*json, EntityType::Player);
          if (!data)
            return std::unexpected(std::move(data.error()));
          m_savedPlayersCache[*uuid] = std::move(*data);
        } else {
          auto existing = m_playerFileNames.get_right(*uuid);
          if (!existing)
            return std::unexpected(std::move(existing.error()));
          duplicate = std::pair{*uuid, *existing};
        }
        return {};
      }();
      if (!loaded) {
        auto logged = m_root.logger().errorException(strf("Error loading player file {}, ignoring", filename), ExceptionDiagnostic(loaded.error()));
        if (!logged)
          return std::unexpected(std::move(logged.error()));
      } else if (duplicate) {
        auto logged = m_root.logger().warn("Duplicate player? Skipping player file {} because it has the same UUID as {}.player ({})",
            file.first, duplicate->second.get(), duplicate->first.hex());
        if (!logged)
          return std::unexpected(std::move(logged.error()));
      }
    }

    // Quarantine malformed saves from the visible cache, retaining their files.
    for (auto it = m_savedPlayersCache.begin(); it != m_savedPlayersCache.end();) {
      auto& entry = *it;
      if (entry.second.isNull()) {
        it = m_savedPlayersCache.erase(it);
        continue;
      }
      auto validated = [&]() -> std::expected<void, StarException> {
        auto entity = (*entityFactory)->diskLoadEntity(EntityType::Player, entry.second);
        if (!entity)
          return std::unexpected(std::move(entity.error()));
        auto player = convert<Player>(*entity);
        if (!player)
          return std::unexpected(std::move(player.error()));
        auto playerUuid = (*player)->uuid();
        if (!playerUuid)
          return std::unexpected(std::move(playerUuid.error()));
        if (*playerUuid != entry.first)
          return std::unexpected(PlayerException(strf("Uuid mismatch in loaded player with filename uuid '{}'", entry.first.hex())));
        return {};
      }();
      if (!validated) {
        auto fileName = uuidFileName(entry.first);
        if (!fileName)
          return std::unexpected(std::move(fileName.error()));
        String uuidHex = entry.first.hex();
        auto logged = m_root.logger().errorException(
            uuidHex == fileName->get() ? strf("Failed to validate player with uuid {}", uuidHex)
                                      : strf("Failed to validate player with uuid {} ({}.player)", uuidHex, fileName->get()),
            ExceptionDiagnostic(validated.error()));
        if (!logged)
          return std::unexpected(std::move(logged.error()));
        it = m_savedPlayersCache.erase(it);
      } else {
        ++it;
      }
    }
  }

  auto metadata = [&]() -> std::expected<void, StarException> {
    auto contents = File::read_file_string(File::relative_to(m_storageDirectory, "metadata"));
    if (!contents)
      return std::unexpected(std::move(contents.error()));
    auto json = Json::parseJson(*contents);
    if (!json)
      return std::unexpected(std::move(json.error()));
    auto object = json->toObject();
    if (!object)
      return std::unexpected(std::move(object.error()));
    m_metadata = std::move(*object);
    auto order = json->get("order", Json());
    if (!order)
      return std::unexpected(std::move(order.error()));
    if (!order->isNull()) {
      auto entries = order->iterateArray();
      if (!entries)
        return std::unexpected(std::move(entries.error()));
      for (auto const& jUuid : *entries) {
        auto text = jUuid.toString();
        if (!text)
          return std::unexpected(std::move(text.error()));
        auto uuid = Uuid::create(*text);
        if (!uuid)
          return std::unexpected(std::move(uuid.error()));
        auto entry = m_savedPlayersCache.find(*uuid);
        if (entry != m_savedPlayersCache.end())
          m_savedPlayersCache.move_to_back(entry);
      }
    }
    m_writtenMetadata = *json;
    return {};
  }();
  if (!metadata)
    return m_root.logger().warnException("Error loading player storage metadata file, resetting", ExceptionDiagnostic(metadata.error()));
  return {};
}

PlayerStorage::~PlayerStorage() {
  if (m_initialized) {
    auto written = writeMetadata();
    if (!written)
      EmergencyLog::error("Error saving player storage metadata: {}",
          outputException(ExceptionDiagnostic(written.error()), false));
  }
}

std::size_t PlayerStorage::playerCount() const {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lockWithDiagnostics(locker);
  return m_savedPlayersCache.size();
}

std::optional<Uuid> PlayerStorage::playerUuidAt(std::size_t index) {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lockWithDiagnostics(locker);
  if (index < m_savedPlayersCache.size())
    return std::next(m_savedPlayersCache.begin(), index)->first;
  else
    return {};
}

std::expected<std::optional<Uuid>, StarException> PlayerStorage::playerUuidByName(String const& name, std::optional<Uuid> except) {
  auto cleanMatch = Text::stripEscapeCodes(name).to_lower();
  if (!cleanMatch)
    return std::unexpected(std::move(cleanMatch.error()));
  std::optional<Uuid> uuid;
  std::unique_lock locker(m_mutex, std::defer_lock);
  lockWithDiagnostics(locker);
  std::size_t longest = std::numeric_limits<std::size_t>::max();
  for (auto& cache : m_savedPlayersCache) {
    if (except && *except == cache.first)
      continue;
    auto playerName = cache.second.optQueryString("identity.name");
    if (!playerName)
      return std::unexpected(std::move(playerName.error()));
    if (!*playerName)
      continue;
    auto cleanName = Text::stripEscapeCodes(**playerName).to_lower();
    if (!cleanName)
      return std::unexpected(std::move(cleanName.error()));
    auto len = cleanName->size();
    if (!len)
      return std::unexpected(std::move(len.error()));
    if (*len < longest && cleanName->utf8().rfind(cleanMatch->utf8()) == 0) {
      longest = *len;
      uuid = cache.first;
    }
  }
  return uuid;
}

std::expected<List<Uuid>, StarException> PlayerStorage::playerUuidListByName(String const& name, std::optional<Uuid> except) {
  auto cleanMatch = Text::stripEscapeCodes(name).to_lower();
  if (!cleanMatch)
    return std::unexpected(std::move(cleanMatch.error()));
  List<Uuid> list;
  std::unique_lock locker(m_mutex, std::defer_lock);
  lockWithDiagnostics(locker);
  for (auto& cache : m_savedPlayersCache) {
    if (except && *except == cache.first)
      continue;
    auto playerName = cache.second.optQueryString("identity.name");
    if (!playerName)
      return std::unexpected(std::move(playerName.error()));
    if (!*playerName)
      continue;
    auto cleanName = Text::stripEscapeCodes(**playerName).to_lower();
    if (!cleanName)
      return std::unexpected(std::move(cleanName.error()));
    if (cleanMatch->empty() || cleanName->utf8().rfind(cleanMatch->utf8()) != NPos)
      list.push_back(cache.first);
  }
  return list;
}


std::expected<Json, StarException> PlayerStorage::savePlayer(std::shared_ptr<Player> const& player) {
  auto entityFactory = m_root.entityFactory();
  if (!entityFactory)
    return std::unexpected(std::move(entityFactory.error()));
  std::unique_lock locker(m_mutex, std::defer_lock);
  lockWithDiagnostics(locker);
  auto playerUuid = player->uuid();
  if (!playerUuid)
    return std::unexpected(std::move(playerUuid.error()));
  auto uuid = *playerUuid;
  auto newPlayerData = player->diskStore();
  if (!newPlayerData)
    return std::unexpected(std::move(newPlayerData.error()));
  auto cached = m_savedPlayersCache.find_ref(uuid);
  if (!cached || cached->get() != *newPlayerData) {
    auto versionedJson = (*entityFactory)->storeVersionedJson(EntityType::Player, *newPlayerData);
    if (!versionedJson)
      return std::unexpected(std::move(versionedJson.error()));
    auto basename = uuidFileName(uuid);
    if (!basename)
      return std::unexpected(std::move(basename.error()));
    auto fileName = strf("{}.player", basename->get());
    auto written = VersionedJson::write_file(*versionedJson, File::relative_to(m_storageDirectory, fileName));
    if (!written)
      return std::unexpected(std::move(written.error()));
    m_savedPlayersCache[uuid] = *newPlayerData;
    auto playerName = player->name();
    if (!playerName)
      return std::unexpected(std::move(playerName.error()));
    auto logged = m_root.logger().debug("Saved player {} to {}", Text::stripEscapeCodes(*playerName), fileName);
    if (!logged)
      return std::unexpected(std::move(logged.error()));
  }
  return std::move(*newPlayerData);
}

std::optional<Json> PlayerStorage::maybeGetPlayerData(Uuid const& uuid) {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lockWithDiagnostics(locker);
  if (auto cache = m_savedPlayersCache.find_ref(uuid))
    return cache->get();
  else
    return {};
}

std::expected<Json, StarException> PlayerStorage::getPlayerData(Uuid const& uuid) {
  auto data = maybeGetPlayerData(uuid);
  if (!data)
    return std::unexpected(PlayerException(strf("No such stored player with uuid '{}'", uuid.hex())));
  return *data;
}

std::expected<std::shared_ptr<Player>, StarException> PlayerStorage::loadPlayer(Uuid const& uuid) {
  auto playerCacheData = getPlayerData(uuid);
  if (!playerCacheData)
    return std::unexpected(std::move(playerCacheData.error()));
  auto entityFactory = m_root.entityFactory();
  if (!entityFactory)
    return std::unexpected(std::move(entityFactory.error()));
  auto loaded = [&]() -> std::expected<std::shared_ptr<Player>, StarException> {
    auto entity = (*entityFactory)->diskLoadEntity(EntityType::Player, *playerCacheData);
    if (!entity)
      return std::unexpected(std::move(entity.error()));
    auto player = convert<Player>(*entity);
    if (!player)
      return std::unexpected(std::move(player.error()));
    auto playerUuid = (*player)->uuid();
    if (!playerUuid)
      return std::unexpected(std::move(playerUuid.error()));
    if (*playerUuid != uuid)
      return std::unexpected(PlayerException(strf("Uuid mismatch in loaded player with filename uuid '{}'", uuid.hex())));
    return std::move(*player);
  }();
  if (loaded)
    return std::move(*loaded);
  auto logged = m_root.logger().errorException("Error loading player file, ignoring", ExceptionDiagnostic(loaded.error()));
  if (!logged)
    return std::unexpected(std::move(logged.error()));
  std::unique_lock locker(m_mutex, std::defer_lock);
  lockWithDiagnostics(locker);
  m_savedPlayersCache.remove(uuid);
  return std::shared_ptr<Player>();
}

std::expected<void, StarException> PlayerStorage::deletePlayer(Uuid const& uuid) {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lockWithDiagnostics(locker);
  if (!m_savedPlayersCache.contains(uuid))
    return std::unexpected(PlayerException(strf("No such stored player with uuid '{}'", uuid.hex())));
  m_savedPlayersCache.remove(uuid);
  auto uuidHex = uuid.hex();
  auto storagePrefix = File::relative_to(m_storageDirectory, uuidHex);
  auto backupPrefix = File::relative_to(m_backupDirectory, uuidHex);
  auto removeIfExists = [](String const& prefix, String const& suffix) -> std::expected<void, StarException> {
    auto exists = File::exists(prefix + suffix);
    if (!exists)
      return std::unexpected(std::move(exists.error()));
    if (*exists)
      return File::remove(prefix + suffix);
    return {};
  };
  for (auto const& suffix : std::initializer_list<std::string_view>{".player", ".shipworld"}) {
    auto removed = removeIfExists(storagePrefix, suffix);
    if (!removed)
      return std::unexpected(std::move(removed.error()));
  }
  auto configuration = m_root.configuration();
  if (!configuration)
    return std::unexpected(std::move(configuration.error()));
  auto setting = (*configuration)->get("playerBackupFileCount");
  if (!setting)
    return std::unexpected(std::move(setting.error()));
  auto playerBackupFileCount = setting->toUInt();
  if (!playerBackupFileCount)
    return std::unexpected(std::move(playerBackupFileCount.error()));
  for (unsigned i = 1, count = static_cast<unsigned>(*playerBackupFileCount); i <= count; ++i) {
    for (auto const& extension : std::initializer_list<std::string_view>{"player", "shipworld"}) {
      auto removed = removeIfExists(backupPrefix, strf(".{}.bak{}", extension, i));
      if (!removed)
        return std::unexpected(std::move(removed.error()));
    }
  }
  return {};
}

std::expected<WorldChunks, StarException> PlayerStorage::loadShipData(Uuid const& uuid) {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lockWithDiagnostics(locker);
  if (!m_savedPlayersCache.contains(uuid))
    return std::unexpected(PlayerException(strf("No such stored player with uuid '{}'", uuid.hex())));
  auto basename = uuidFileName(uuid);
  if (!basename)
    return std::unexpected(std::move(basename.error()));
  String filename = File::relative_to(m_storageDirectory, strf("{}.shipworld", basename->get()));
  auto exists = File::exists(filename);
  if (!exists)
    return std::unexpected(std::move(exists.error()));
  if (*exists) {
    auto chunks = WorldStorage::getWorldChunksFromFile(m_root.logger(), filename);
    if (chunks)
      return std::move(*chunks);
    auto logged = m_root.logger().errorException(strf("Failed to load shipworld file {}, removing", filename), ExceptionDiagnostic(chunks.error()));
    if (!logged)
      return std::unexpected(std::move(logged.error()));
    auto removed = File::remove(filename);
    if (!removed)
      return std::unexpected(std::move(removed.error()));
  }
  return WorldChunks();
}

std::expected<void, StarException> PlayerStorage::applyShipUpdates(Uuid const& uuid, WorldChunks const& updates) {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lockWithDiagnostics(locker);
  if (!m_savedPlayersCache.contains(uuid))
    return std::unexpected(PlayerException(strf("No such stored player with uuid '{}'", uuid.hex())));
  if (updates.empty())
    return {};
  auto basename = uuidFileName(uuid);
  if (!basename)
    return std::unexpected(std::move(basename.error()));
  String filePath = File::relative_to(m_storageDirectory, strf("{}.shipworld", basename->get()));
  return WorldStorage::applyWorldChunksUpdateToFile(m_root.logger(), filePath, updates);
}

std::expected<void, StarException> PlayerStorage::moveToFront(Uuid const& uuid) {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lockWithDiagnostics(locker);
  auto moved = m_savedPlayersCache.move_to_front(uuid);
  if (!moved)
    return std::unexpected(std::move(moved.error()));
  return writeMetadata();
}

std::expected<void, StarException> PlayerStorage::backupCycle(Uuid const& uuid) {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lockWithDiagnostics(locker);
  auto configuration = m_root.configuration();
  if (!configuration)
    return std::unexpected(std::move(configuration.error()));
  auto setting = (*configuration)->get("playerBackupFileCount");
  if (!setting)
    return std::unexpected(std::move(setting.error()));
  auto playerBackupFileCount = setting->toUInt();
  if (!playerBackupFileCount)
    return std::unexpected(std::move(playerBackupFileCount.error()));
  auto fileName = uuidFileName(uuid);
  if (!fileName)
    return std::unexpected(std::move(fileName.error()));
  auto path = [&](String const& dir, String const& extension) {
    return File::relative_to(dir, strf("{}.{}", fileName->get(), extension));
  };
  auto directory = File::is_directory(m_backupDirectory);
  if (!directory)
    return std::unexpected(std::move(directory.error()));
  if (!*directory) {
    auto created = File::create_directory(m_backupDirectory);
    if (!created)
      return std::unexpected(std::move(created.error()));
  }
  for (auto const& extension : std::initializer_list<std::string_view>{"player", "shipworld", "metadata"}) {
    auto backedUp =
      File::backup_file_in_sequence(path(m_storageDirectory, extension), path(m_backupDirectory, extension),
                                    static_cast<unsigned>(*playerBackupFileCount), ".bak");
    if (!backedUp)
      return std::unexpected(std::move(backedUp.error()));
  }
  return {};
}

std::expected<void, StarException> PlayerStorage::setMetadata(String key, Json value) {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lockWithDiagnostics(locker);
  m_metadata[std::move(key)] = std::move(value);
  return writeMetadata();
}

std::expected<Json, StarException> PlayerStorage::getMetadata(String const& key) {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lockWithDiagnostics(locker);
  return Json(m_metadata).get(key, {});
}

std::expected<std::reference_wrapper<String const>, StarException> PlayerStorage::uuidFileName(Uuid const& uuid) {
  if (!m_playerFileNames.contains_left(uuid)) {
    auto added = m_playerFileNames.add(uuid, uuid.hex());
    if (!added)
      return std::unexpected(std::move(added.error()));
  }
  return m_playerFileNames.get_right(uuid);
}

std::expected<void, StarException> PlayerStorage::writeMetadata() {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lockWithDiagnostics(locker);
  JsonArray order;
  for (auto const& p : m_savedPlayersCache)
    order.push_back(p.first.hex());
  m_metadata["order"] = std::move(order);
  Json metadata(m_metadata);
  if (m_writtenMetadata && *m_writtenMetadata == metadata)
    return {};
  auto contents = metadata.printJson(0);
  if (!contents)
    return std::unexpected(std::move(contents.error()));
  String filename = File::relative_to(m_storageDirectory, "metadata");
  auto written = File::overwrite_file_with_rename(*contents, filename);
  if (!written)
    return std::unexpected(std::move(written.error()));
  m_writtenMetadata = std::move(metadata);
  return {};
}

}

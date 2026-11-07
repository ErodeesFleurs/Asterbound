module;
import star.format;
import star.xx_hash;
import star.json;
import star.ordered_map;
import star.data_stream;
import star.bi_map;
import star.id_map;
import star.set;
import star.btree;
import star.lru_cache;
import star.data_stream_devices;
import star.thread;
import star.btree_database;
import star.casting;
import star.ordered_set;
import star.list;
import star.game_types;
import star.string;
import star.version;
import star.rpc_promise;
import star.random;
import star.perlin;
import star.weighted_pool;
import star.strong_typedef;
import std;
import star.worker_pool;

import star.sector_array_2d;
import star.color;
import star.image_processing;
import star.hash;
import star.string_view;
import star.directives;
import star.asset_path;


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
import star.lua_root;
import star.versioning_database;
import star.world_storage;

namespace Star {

class Root;

class Player;

class PlayerStorage {
public:
  [[nodiscard]] static std::expected<std::shared_ptr<PlayerStorage>, StarException> create(Root& root, String const& storageDir);
  ~PlayerStorage();

  std::size_t playerCount() const;
  // Returns nothing if index is out of bounds.
  std::optional<Uuid> playerUuidAt(std::size_t index);
  // Returns nothing if name doesn't match a player.
  [[nodiscard]] std::expected<std::optional<Uuid>, StarException> playerUuidByName(String const& name, std::optional<Uuid> except = {});
  // Returns nothing if name doesn't match a player.
  [[nodiscard]] std::expected<List<Uuid>, StarException> playerUuidListByName(String const& name, std::optional<Uuid> except = {});

  // Also returns the diskStore Json if needed.
  [[nodiscard]] std::expected<Json, StarException> savePlayer(std::shared_ptr<Player> const& player);

  std::optional<Json> maybeGetPlayerData(Uuid const& uuid);
  [[nodiscard]] std::expected<Json, StarException> getPlayerData(Uuid const& uuid);
  [[nodiscard]] std::expected<std::shared_ptr<Player>, StarException> loadPlayer(Uuid const& uuid);
  [[nodiscard]] std::expected<void, StarException> deletePlayer(Uuid const& uuid);

  [[nodiscard]] std::expected<WorldChunks, StarException> loadShipData(Uuid const& uuid);
  [[nodiscard]] std::expected<void, StarException> applyShipUpdates(Uuid const& uuid, WorldChunks const& updates);

  // Move the given player to the top of the player ordering.
  [[nodiscard]] std::expected<void, StarException> moveToFront(Uuid const& uuid);

  // Copy all the player relevant files for this uuid into .bak1 .bak2 etc
  // files for however many backups are configured
  [[nodiscard]] std::expected<void, StarException> backupCycle(Uuid const& uuid);

  // Get / Set PlayerStorage global metadata
  [[nodiscard]] std::expected<void, StarException> setMetadata(String key, Json value);
  [[nodiscard]] std::expected<Json, StarException> getMetadata(String const& key);

private:
  explicit PlayerStorage(Root& root);
  [[nodiscard]] std::expected<void, StarException> initialize(String const& storageDir);
  Root& m_root;
  [[nodiscard]] std::expected<std::reference_wrapper<String const>, StarException> uuidFileName(Uuid const& uuid);
  [[nodiscard]] std::expected<void, StarException> writeMetadata();

  mutable std::recursive_mutex m_mutex;
  bool m_initialized = false;
  String m_storageDirectory;
  String m_backupDirectory;
  OrderedHashMap<Uuid, Json> m_savedPlayersCache;
  BiMap<Uuid, String> m_playerFileNames;
  JsonObject m_metadata;
  // Only acknowledge metadata after a successful load or durable replacement.
  std::optional<Json> m_writtenMetadata;
};

}

export module star.player_storage;

export namespace Star {
using ::Star::PlayerStorage;
}

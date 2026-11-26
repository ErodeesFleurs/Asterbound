module;
import std;
import star.map;
import star.format;
import star.json;
import star.id_map;
import star.game_types;
import star.casting;
import star.strong_typedef;
import star.list;
import star.rpc_promise;
import star.vector;
import star.rect;
import star.bi_map;
import star.a_star;
import star.ordered_map;
import star.lru_cache;
import star.thread;
import star.time;
import star.random;
import star.ttl_cache;
import star.data_stream;
import star.image_processing;
import star.color;
import star.string;
import star.poly;
import star.hash;
import star.string_view;
import star.directives;
import star.asset_path;


import star.world_geometry;
import star.status_types;
import star.damage;
import star.light_source;
import star.entity;
import star.tile_damage;
import star.interaction_types;
import star.item_descriptor;
import star.celestial_coordinate;
import star.quest_descriptor;
import star.interactive_entity;
import star.collision_block;
import star.tile_entity;
import star.tile_modification;
import star.lua_root;
import star.force_regions;
import star.world;
import star.physics_entity;
import star.movement_controller;
import star.platformer_astar_types;
import star.anchorable_entity;

import star.game_timers;
import star.actor_movement_controller;
import star.damage_types;
import star.drawable;
import star.entity_rendering_types;

import star.net_compatibility;
import star.byte_array;
namespace Star {

class Root;
class LuaRoot;
class Rebuilder;
class RandomSource;
class Monster;

struct MonsterExceptionTag {
  static constexpr std::string_view name() { return "MonsterException"; }
};
using MonsterException = StarError<MonsterExceptionTag, StarException>;

struct MonsterVariant {
  String type;
  std::uint64_t seed;
  Json unique_parameters;

  std::optional<String> short_description;
  std::optional<String> description;

  Json animator_config;
  StringMap<String> animator_part_tags;
  float animator_zoom;
  // Is the animator specified Left facing?
  bool reversed;

  // Either is a String which specifies a dropPool, or a map which maps
  // damageSourceKind to the appropriate treasure pool for this monster, with a
  // "default" key as a catch-all.
  Json drop_pool_config;

  // Every parameter specified in each section of the monster configuration is
  // stored here.  The base parameters, size parameters, variation parameters,
  // and part parameters are all merged together into one final configuration.
  Json parameters;

  // Parameters common to all Monsters

  StringList scripts;
  unsigned initial_script_delta;
  StringList animation_scripts;

  RectF meta_bound_box;
  EntityRenderLayer render_layer;
  float scale;

  ActorMovementParameters movement_settings;
  float walk_multiplier;
  float run_multiplier;
  float jump_multiplier;
  float weight_multiplier;
  float health_multiplier;
  float touch_damage_multiplier;

  Json touch_damage_config;
  StringMap<Json> animation_damage_parts;
  Json status_settings;
  Vec2F mouth_offset;
  Vec2F feet_offset;

  String power_level_function;
  String health_level_function;

  ClientEntityMode client_entity_mode;
  bool persistent;

  TeamType damage_team_type;
  std::uint8_t damage_team;

  PolyF self_damage_poly;

  std::optional<String> portrait_icon;

  float damage_received_aggressive_duration;
  float on_damaged_others_aggressive_duration;
  float on_fire_aggressive_duration;

  Vec3B nametag_color;
  std::optional<ColorReplaceMap> color_swap;
};

class MonsterDatabase {
  struct ConstructionKey { explicit ConstructionKey() = default; };

public:
  MonsterDatabase(ConstructionKey, Root& root);

  [[nodiscard]] static std::expected<std::shared_ptr<MonsterDatabase>, StarException> create(Root& root);

  void cleanup();

  StringList monster_types() const;

  [[nodiscard]] std::expected<MonsterVariant, StarException> random_monster(String const& type_name, Json const& unique_parameters = JsonObject()) const;
  [[nodiscard]] std::expected<MonsterVariant, StarException> monster_variant(String const& type_name, std::uint64_t seed, Json const& unique_parameters = JsonObject()) const;

  [[nodiscard]] std::expected<ByteArray, StarException> write_monster_variant(MonsterVariant const& variant, NetCompatibilityRules rules = {}) const;
  [[nodiscard]] std::expected<MonsterVariant, StarException> read_monster_variant(ByteArray const& data, NetCompatibilityRules rules = {}) const;

  [[nodiscard]] std::expected<Json, StarException> write_monster_variant_to_json(MonsterVariant const& variant) const;
  [[nodiscard]] std::expected<MonsterVariant, StarException> read_monster_variant_from_json(Json const& variant) const;

  // If level is 0, then the monster will start with the threat level of
  // whatever world they're spawned in.
  [[nodiscard]] std::expected<std::shared_ptr<Monster>, StarException> create_monster(MonsterVariant variant, std::optional<float> level = {}, Json unique_parameters = {}) const;
  [[nodiscard]] std::expected<std::shared_ptr<Monster>, StarException> disk_load_monster(Json const& disk_store) const;
  [[nodiscard]] std::expected<std::shared_ptr<Monster>, StarException> net_load_monster(ByteArray const& net_store, NetCompatibilityRules rules = {}) const;

  [[nodiscard]] std::expected<List<Drawable>, StarException> monster_portrait(MonsterVariant const& variant) const;

  [[nodiscard]] std::expected<std::pair<String, String>, StarException> skill_info(String const& skill_name) const;
  [[nodiscard]] std::expected<Json, StarException> skill_config_parameter(String const& skill_name, String const& config_parameter_name) const;

  [[nodiscard]] std::expected<ColorReplaceMap, StarException> color_swap(String const& set_name, std::uint64_t seed) const;

  [[nodiscard]] std::expected<Json, StarException> monster_config(String const& type_name) const;

private:
  [[nodiscard]] std::expected<void, StarException> initialize();

  struct MonsterType {
    String type_name;
    std::optional<String> short_description;
    std::optional<String> description;

    StringList categories;
    StringList part_types;

    String animation_config_path;
    String colors;
    bool reversed;

    JsonArray drop_pools;

    Json base_parameters;

    // Additional part-specific parameters which will override any part-specific
    // parameters (such as skills, sounds, etc.) defined in individual .monsterpart files
    Json part_parameter_overrides;

    // Description of all part parameters, and how they are combined and with
    // what defaults.
    Json part_parameter_description;

    Json to_json() const;
  };

  struct MonsterPart {
    String name;
    String category;
    String type;

    String path;
    JsonObject frames;
    Json part_parameters;
  };

  struct MonsterSkill {
    String name;
    String label;
    String image;

    Json config;
    Json parameters;
    Json animation_parameters;
  };

  // Merges part configuration by the method specified in the part parameter
  // config.
  [[nodiscard]] static std::expected<Json, StarException> merge_part_parameters(Json const& part_parameter_description, JsonArray const& parameters);

  // Merges final monster variant parameters together according to the
  // hard-coded variant merge rules (handles things like scripts which are
  // combined rather than overwritten)
  [[nodiscard]] static std::expected<JsonObject, StarException> merge_final_parameters(JsonArray const& parameters);

  // Reads common parameters out of parameters map
  [[nodiscard]] static std::expected<void, StarException> read_common_parameters(MonsterVariant& variant);

  Root& m_root;

  // Maps category name -> part type -> part name -> MonsterPart.  part name ->
  // MonsterPart needs to be be in a predictable order.
  using PartDirectory = StringMap<StringMap<Map<String, MonsterPart>>>;

  [[nodiscard]] std::expected<MonsterVariant, StarException> produce_monster(String const& type_name, std::uint64_t seed, Json const& unique_parameters) const;

  // Selects ordered skills and merges their parameters and animation configs into the variant.
  [[nodiscard]] std::expected<void, StarException> choose_skills(MonsterVariant& variant, RandomSource& random) const;

  StringMap<MonsterType> m_monster_types;
  PartDirectory m_part_directory;
  StringMap<MonsterSkill> m_skills;
  StringMap<List<ColorReplaceMap>> m_color_swaps;

  mutable std::mutex m_cache_mutex;

  std::shared_ptr<Rebuilder> m_rebuilder;

  // Key here is the type name, seed, and unique parameters JSON.
  mutable HashTtlCache<std::tuple<String, std::uint64_t, Json>, MonsterVariant> m_monster_cache;
};

}

export module star.monster_database;

export namespace Star {
  using ::Star::MonsterExceptionTag;
  using ::Star::MonsterException;
  using ::Star::MonsterVariant;
  using ::Star::LuaRoot;
  using ::Star::Rebuilder;
  using ::Star::RandomSource;
  using ::Star::Monster;
  using ::Star::MonsterDatabase;
}

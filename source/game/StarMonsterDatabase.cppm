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

import star.either;
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
  Json uniqueParameters;

  std::optional<String> shortDescription;
  std::optional<String> description;

  Json animatorConfig;
  StringMap<String> animatorPartTags;
  float animatorZoom;
  // Is the animator specified Left facing?
  bool reversed;

  // Either is a String which specifies a dropPool, or a map which maps
  // damageSourceKind to the appropriate treasure pool for this monster, with a
  // "default" key as a catch-all.
  Json dropPoolConfig;

  // Every parameter specified in each section of the monster configuration is
  // stored here.  The base parameters, size parameters, variation parameters,
  // and part parameters are all merged together into one final configuration.
  Json parameters;

  // Parameters common to all Monsters

  StringList scripts;
  unsigned initialScriptDelta;
  StringList animationScripts;

  RectF metaBoundBox;
  EntityRenderLayer renderLayer;
  float scale;

  ActorMovementParameters movementSettings;
  float walkMultiplier;
  float runMultiplier;
  float jumpMultiplier;
  float weightMultiplier;
  float healthMultiplier;
  float touchDamageMultiplier;

  Json touchDamageConfig;
  StringMap<Json> animationDamageParts;
  Json statusSettings;
  Vec2F mouthOffset;
  Vec2F feetOffset;

  String powerLevelFunction;
  String healthLevelFunction;

  ClientEntityMode clientEntityMode;
  bool persistent;

  TeamType damageTeamType;
  std::uint8_t damageTeam;

  PolyF selfDamagePoly;

  std::optional<String> portraitIcon;

  float damageReceivedAggressiveDuration;
  float onDamagedOthersAggressiveDuration;
  float onFireAggressiveDuration;

  Vec3B nametagColor;
  std::optional<ColorReplaceMap> colorSwap;
};

class MonsterDatabase {
public:
  [[nodiscard]] static std::expected<std::shared_ptr<MonsterDatabase>, StarException> create(Root& root);

  void cleanup();

  StringList monsterTypes() const;

  [[nodiscard]] std::expected<MonsterVariant, StarException> randomMonster(String const& typeName, Json const& uniqueParameters = JsonObject()) const;
  [[nodiscard]] std::expected<MonsterVariant, StarException> monsterVariant(String const& typeName, std::uint64_t seed, Json const& uniqueParameters = JsonObject()) const;

  [[nodiscard]] std::expected<ByteArray, StarException> writeMonsterVariant(MonsterVariant const& variant, NetCompatibilityRules rules = {}) const;
  [[nodiscard]] std::expected<MonsterVariant, StarException> readMonsterVariant(ByteArray const& data, NetCompatibilityRules rules = {}) const;

  [[nodiscard]] std::expected<Json, StarException> writeMonsterVariantToJson(MonsterVariant const& mVar) const;
  [[nodiscard]] std::expected<MonsterVariant, StarException> readMonsterVariantFromJson(Json const& variant) const;

  // If level is 0, then the monster will start with the threat level of
  // whatever world they're spawned in.
  [[nodiscard]] std::expected<std::shared_ptr<Monster>, StarException> createMonster(MonsterVariant monsterVariant, std::optional<float> level = {}, Json uniqueParameters = {}) const;
  [[nodiscard]] std::expected<std::shared_ptr<Monster>, StarException> diskLoadMonster(Json const& diskStore) const;
  [[nodiscard]] std::expected<std::shared_ptr<Monster>, StarException> netLoadMonster(ByteArray const& netStore, NetCompatibilityRules rules = {}) const;

  [[nodiscard]] std::expected<List<Drawable>, StarException> monsterPortrait(MonsterVariant const& variant) const;

  [[nodiscard]] std::expected<std::pair<String, String>, StarException> skillInfo(String const& skillName) const;
  [[nodiscard]] std::expected<Json, StarException> skillConfigParameter(String const& skillName, String const& configParameterName) const;

  [[nodiscard]] std::expected<ColorReplaceMap, StarException> colorSwap(String const& setName, std::uint64_t seed) const;

  [[nodiscard]] std::expected<Json, StarException> monsterConfig(String const& typeName) const;

private:
  explicit MonsterDatabase(Root& root);
  [[nodiscard]] std::expected<void, StarException> initialize();

  struct MonsterType {
    String typeName;
    std::optional<String> shortDescription;
    std::optional<String> description;

    StringList categories;
    StringList partTypes;

    String animationConfigPath;
    String colors;
    bool reversed;

    JsonArray dropPools;

    Json baseParameters;

    // Additional part-specific parameters which will override any part-specific
    // parameters (such as skills, sounds, etc.) defined in individual .monsterpart files
    Json partParameterOverrides;

    // Description of all part parameters, and how they are combined and with
    // what defaults.
    Json partParameterDescription;

    Json toJson() const;
  };

  struct MonsterPart {
    String name;
    String category;
    String type;

    String path;
    JsonObject frames;
    Json partParameters;
  };

  struct MonsterSkill {
    String name;
    String label;
    String image;

    Json config;
    Json parameters;
    Json animationParameters;
  };

  // Merges part configuration by the method specified in the part parameter
  // config.
  [[nodiscard]] static std::expected<Json, StarException> mergePartParameters(Json const& partParameterDescription, JsonArray const& parameters);

  // Merges final monster variant parameters together according to the
  // hard-coded variant merge rules (handles things like scripts which are
  // combined rather than overwritten)
  [[nodiscard]] static std::expected<Json, StarException> mergeFinalParameters(JsonArray const& parameters);

  // Reads common parameters out of parameters map
  [[nodiscard]] static std::expected<void, StarException> readCommonParameters(MonsterVariant& monsterVariant);

  Root& m_root;

  // Maps category name -> part type -> part name -> MonsterPart.  part name ->
  // MonsterPart needs to be be in a predictable order.
  using PartDirectory = StringMap<StringMap<Map<String, MonsterPart>>>;

  [[nodiscard]] std::expected<MonsterVariant, StarException> produceMonster(String const& typeName, std::uint64_t seed, Json const& uniqueParameters) const;

  // Given a variant including parameters for baseSkills and specialSkills,
  // returns a variant containing a final 'skills' list of chosen skills, also
  // merges animation configs from skills together.
  [[nodiscard]] std::expected<std::pair<Json, Json>, StarException> chooseSkills(Json const& parameters, Json const& animatorConfig, RandomSource& rand) const;

  StringMap<MonsterType> m_monsterTypes;
  PartDirectory m_partDirectory;
  StringMap<MonsterSkill> m_skills;
  StringMap<List<ColorReplaceMap>> m_colorSwaps;

  mutable std::mutex m_cacheMutex;

  std::shared_ptr<Rebuilder> m_rebuilder;

  // Key here is the type name, seed, and the serialized unique parameters JSON
  mutable HashTtlCache<std::tuple<String, std::uint64_t, Json>, MonsterVariant> m_monsterCache;
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

module;
import std;
import star.format;
import star.json;
import star.string;
import star.random;
import star.periodic_function;
import star.ordered_map;
import star.audio;
import star.thread;
import star.list;
import star.map;
import star.set;
import star.vector;
import star.data_stream;
import star.game_types;
import star.strong_typedef;
import star.id_map;
import star.color;
import star.bi_map;
import star.image_processing;
import star.hash;
import star.string_view;
import star.directives;
import star.asset_path;


import star.drawable;
import star.animated_part_set;
import star.mixer;
import star.light_source;
import star.networked_animator;
import star.humanoid;
import star.damage_types;
import star.status_types;
import star.animation;
import star.particle;
import star.item_descriptor;

import star.entity_splash;

import star.net_compatibility;
import star.byte_array;
namespace Star {

class Root;
class Rebuilder;
class ItemDatabase;
class Item;
class Npc;

struct NpcExceptionTag {
  static constexpr std::string_view name() { return "NpcException"; }
};
using NpcException = StarError<NpcExceptionTag, StarException>;

struct NpcVariant {
  String species;
  String typeName;
  float level;
  std::uint64_t seed;

  Json overrides;

  StringList scripts;
  unsigned initialScriptDelta;
  Json scriptConfig;

  std::optional<String> description;

  HumanoidIdentity humanoid_identity;
  Json humanoid_config;
  bool unique_humanoid_config;
  JsonObject humanoid_parameters;

  Json movementParameters;
  Json statusControllerSettings;
  List<PersistentStatusEffect> innateStatusEffects;
  Json touchDamageConfig;

  StringMap<ItemDescriptor> items;

  StringList dropPools;
  bool disableWornArmor;

  bool persistent;
  bool keepAlive;

  TeamType damageTeamType;
  std::uint8_t damageTeam;

  Vec3B nametagColor;

  EntitySplashConfig splash_config;
};

class NpcDatabase {
public:
  [[nodiscard]] static std::expected<std::shared_ptr<NpcDatabase>, StarException> create(Root& root);

  [[nodiscard]] std::expected<NpcVariant, StarException> generateNpcVariant(String const& species, String const& typeName, float level, std::optional<std::reference_wrapper<ItemDatabase const>> itemCatalog = std::nullopt) const;
  [[nodiscard]] std::expected<NpcVariant, StarException> generateNpcVariant(String const& species, String const& typeName, float level, std::uint64_t seed, Json const& overrides, std::optional<std::reference_wrapper<ItemDatabase const>> itemCatalog = std::nullopt) const;

  [[nodiscard]] std::expected<ByteArray, StarException> writeNpcVariant(NpcVariant const& variant, NetCompatibilityRules rules = {}) const;
  [[nodiscard]] std::expected<NpcVariant, StarException> readNpcVariant(ByteArray const& data, NetCompatibilityRules rules = {}) const;

  [[nodiscard]] std::expected<Json, StarException> writeNpcVariantToJson(NpcVariant const& variant) const;
  [[nodiscard]] std::expected<NpcVariant, StarException> readNpcVariantFromJson(Json const& data) const;

  [[nodiscard]] std::expected<std::shared_ptr<Npc>, StarException> createNpc(NpcVariant const& npcVariant) const;
  [[nodiscard]] std::expected<std::shared_ptr<Npc>, StarException> diskLoadNpc(Json const& diskStore) const;
  [[nodiscard]] std::expected<std::shared_ptr<Npc>, StarException> net_load_npc(ByteArray const& net_store, NetCompatibilityRules rules = {}) const;

  [[nodiscard]] std::expected<List<Drawable>, StarException> npcPortrait(NpcVariant const& npcVariant, PortraitMode mode, std::optional<std::reference_wrapper<ItemDatabase const>> itemCatalog = std::nullopt) const;

  [[nodiscard]] std::expected<Json, StarException> buildConfig(String const& typeName, Json const& overrides = Json()) const;

private:
  explicit NpcDatabase(Root& root);
  [[nodiscard]] std::expected<void, StarException> initialize();

  // Recursively merges maps and lets any non-null merger (including lists)
  // override any base value
  [[nodiscard]] std::expected<Json, StarException> mergeConfigValues(Json const& base, Json const& merger) const;

  Root& m_root;

  std::shared_ptr<Rebuilder> m_rebuilder;

  StringMap<Json> m_npcTypes;
};

}

export module star.npc_database;

export namespace Star {
  using ::Star::NpcExceptionTag;
  using ::Star::NpcException;
  using ::Star::NpcVariant;
  using ::Star::Rebuilder;
  using ::Star::Item;
  using ::Star::Npc;
  using ::Star::NpcDatabase;
}

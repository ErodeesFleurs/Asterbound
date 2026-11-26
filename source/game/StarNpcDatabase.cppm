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
  String type_name;
  float level;
  std::uint64_t seed;

  Json overrides;

  StringList scripts;
  unsigned initial_script_delta;
  Json script_config;

  std::optional<String> description;

  HumanoidIdentity humanoid_identity;
  Json humanoid_config;
  bool unique_humanoid_config;
  JsonObject humanoid_parameters;

  Json movement_parameters;
  Json status_controller_settings;
  List<PersistentStatusEffect> innate_status_effects;
  Json touch_damage_config;

  StringMap<ItemDescriptor> items;

  StringList drop_pools;
  bool disable_worn_armor;

  bool persistent;
  bool keep_alive;

  TeamType damage_team_type;
  std::uint8_t damage_team;

  Vec3B nametag_color;

  EntitySplashConfig splash_config;
};

class NpcDatabase {
  class ConstructionKey {
    friend class NpcDatabase;
    ConstructionKey() = default;
  };

public:
  explicit NpcDatabase(ConstructionKey, Root& root);
  [[nodiscard]] static std::expected<std::shared_ptr<NpcDatabase>, StarException> create(Root& root);

  [[nodiscard]] std::expected<NpcVariant, StarException> generate_npc_variant(String const& species, String const& type_name, float level, std::optional<std::reference_wrapper<ItemDatabase const>> item_catalog = std::nullopt) const;
  [[nodiscard]] std::expected<NpcVariant, StarException> generate_npc_variant(String const& species, String const& type_name, float level, std::uint64_t seed, Json const& overrides, std::optional<std::reference_wrapper<ItemDatabase const>> item_catalog = std::nullopt) const;

  [[nodiscard]] std::expected<ByteArray, StarException> write_npc_variant(NpcVariant const& variant, NetCompatibilityRules rules = {}) const;
  [[nodiscard]] std::expected<NpcVariant, StarException> read_npc_variant(ByteArray const& data, NetCompatibilityRules rules = {}) const;

  [[nodiscard]] std::expected<Json, StarException> write_npc_variant_to_json(NpcVariant const& variant) const;
  [[nodiscard]] std::expected<NpcVariant, StarException> read_npc_variant_from_json(Json const& data) const;

  [[nodiscard]] std::expected<std::shared_ptr<Npc>, StarException> create_npc(NpcVariant variant) const;
  [[nodiscard]] std::expected<std::shared_ptr<Npc>, StarException> disk_load_npc(Json const& disk_store) const;
  [[nodiscard]] std::expected<std::shared_ptr<Npc>, StarException> net_load_npc(ByteArray const& net_store, NetCompatibilityRules rules = {}) const;

  [[nodiscard]] std::expected<List<Drawable>, StarException> npc_portrait(NpcVariant const& npc_variant, PortraitMode mode, std::optional<std::reference_wrapper<ItemDatabase const>> item_catalog = std::nullopt) const;

  [[nodiscard]] std::expected<Json, StarException> build_config(String const& type_name, Json const& overrides = Json()) const;

private:
  [[nodiscard]] std::expected<void, StarException> initialize();

  struct TypeConfig {
    Json config;
    std::expected<String, StarException> base_type;
  };

  Root& m_root;

  std::shared_ptr<Rebuilder> m_rebuilder;

  StringMap<TypeConfig> m_npc_types;
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

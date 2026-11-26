module;
import std;
import star.string;
import star.list;
import star.format;
import star.json;

import star.tile_damage;

namespace Star {

class Assets;
class Root;
class Plant;

struct PlantDatabaseExceptionTag {
  static constexpr std::string_view name() { return "PlantDatabaseException"; }
};
using PlantDatabaseException = StarError<PlantDatabaseExceptionTag, StarException>;

// Configuration for a specific tree variant
struct TreeVariant {
  [[nodiscard]] static std::expected<TreeVariant, StarException> create(Assets const& assets, Json const& json);
  [[nodiscard]] std::expected<Json, StarException> to_json() const;

  String stem_name;
  String foliage_name;

  String stem_directory;
  Json stem_settings;
  float stem_hue_shift = 0;

  String foliage_directory;
  Json foliage_settings;
  float foliage_hue_shift = 0;

  Json descriptions;
  bool ceiling = false;
  bool ephemeral = false;
  Json stem_drop_config;
  Json foliage_drop_config;

  TileDamageParameters tile_damage_parameters;
};

// Configuration for a specific grass variant
struct GrassVariant {
  [[nodiscard]] static std::expected<GrassVariant, StarException> create(Assets const& assets, Json const& json);
  [[nodiscard]] std::expected<Json, StarException> to_json() const;

  String name;

  String directory;
  StringList images;
  float hue_shift = 0;

  Json descriptions;
  bool ceiling = false;
  bool ephemeral = false;
  TileDamageParameters tile_damage_parameters;
};

// Configuration for a specific bush variant
struct BushVariant {
  struct BushShape {
    String image;
    StringList mods;
  };

  [[nodiscard]] static std::expected<BushVariant, StarException> create(Assets const& assets, Json const& json);
  [[nodiscard]] std::expected<Json, StarException> to_json() const;

  String bush_name;
  String mod_name;

  String directory;
  List<BushShape> shapes;

  float base_hue_shift = 0;
  float mod_hue_shift = 0;

  Json descriptions;
  bool ceiling = false;
  bool ephemeral = false;
  TileDamageParameters tile_damage_parameters;
};

class PlantDatabase {
public:
  [[nodiscard]] static std::expected<std::shared_ptr<PlantDatabase>, StarException> create(Root& root);

  [[nodiscard]] std::expected<StringList, StarException> tree_stem_names(bool ceiling = false) const;
  StringList tree_foliage_names() const;
  // Each stem / foliage set has its own patterns of shapes that must match up.
  [[nodiscard]] std::expected<String, StarException> tree_stem_shape(String const& stem_name) const;
  [[nodiscard]] std::expected<String, StarException> tree_foliage_shape(String const& foliage_name) const;
  std::optional<String> tree_stem_directory(String const& stem_name) const;
  std::optional<String> tree_foliage_directory(String const& foliage_name) const;
  // Pass a compatible stem and foliage (as identified by their shape queries).
  [[nodiscard]] std::expected<TreeVariant, StarException> build_tree_variant(
      String const& stem_name, float stem_hue_shift, String const& foliage_name, float foliage_hue_shift) const;
  [[nodiscard]] std::expected<TreeVariant, StarException> build_tree_variant(String const& stem_name, float stem_hue_shift) const;

  [[nodiscard]] std::expected<StringList, StarException> grass_names(bool ceiling = false) const;
  [[nodiscard]] std::expected<GrassVariant, StarException> build_grass_variant(String const& name, float hue_shift) const;

  [[nodiscard]] std::expected<StringList, StarException> bush_names(bool ceiling = false) const;
  [[nodiscard]] std::expected<StringList, StarException> bush_mods(String const& bush_name) const;
  [[nodiscard]] std::expected<BushVariant, StarException> build_bush_variant(
      String const& bush_name, float base_hue_shift, String const& mod_name, float mod_hue_shift) const;

  [[nodiscard]] std::expected<std::shared_ptr<Plant>, StarException> create_plant(TreeVariant const& variant, std::uint64_t seed) const;
  [[nodiscard]] std::expected<std::shared_ptr<Plant>, StarException> create_plant(GrassVariant const& variant, std::uint64_t seed) const;
  [[nodiscard]] std::expected<std::shared_ptr<Plant>, StarException> create_plant(BushVariant const& variant, std::uint64_t seed) const;

  [[nodiscard]] std::expected<Json, StarException> tree_foliage_config(String const& foliage_name) const;
  [[nodiscard]] std::expected<Json, StarException> tree_stem_config(String const& stem_name) const;

private:
  explicit PlantDatabase(Root& root) : m_root(root) {}
  struct Config {
    String directory;
    Json settings;
    Json to_json() const;
  };
  using ConfigMap = StringMap<Config>;
  struct FoliageSelection {
    String const& name;
    Config const& config;
    float hue_shift;
  };

  [[nodiscard]] std::expected<TreeVariant, StarException> resolve_tree_variant(
      String const& stem_name, Config const& stem_config, float stem_hue_shift,
      std::optional<FoliageSelection> foliage) const;

  Root& m_root;

  ConfigMap m_tree_stem_configs;
  ConfigMap m_tree_foliage_configs;
  ConfigMap m_grass_configs;
  ConfigMap m_bush_configs;
};

}

export module star.plant_database;

export namespace Star {
  using ::Star::PlantDatabaseExceptionTag;
  using ::Star::PlantDatabaseException;
  using ::Star::TreeVariant;
  using ::Star::GrassVariant;
  using ::Star::BushVariant;
  using ::Star::Plant;
  using ::Star::PlantDatabase;
}

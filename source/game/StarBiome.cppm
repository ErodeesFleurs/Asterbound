module;
import std;
import star.format;
import star.string;
import star.json;
import star.material_types;
import star.list;
import star.biome_placement;

import star.spawn_type_database;

namespace Star {

struct TreeVariant;
struct AmbientNoisesDescription;
class Parallax;
class Assets;
class Root;

struct BiomePlaceables {
  [[nodiscard]]
  static std::expected<BiomePlaceables, StarException> create(Assets const& assets, Json const& json);

  [[nodiscard]]
  std::expected<Json, StarException> to_json() const;

  // Borrows the first tree in distribution order.
  [[nodiscard]] std::optional<std::reference_wrapper<TreeVariant const>> first_tree() const &;
  std::optional<std::reference_wrapper<TreeVariant const>> first_tree() const && = delete;

  ModId grass_mod = NoModId;
  float grass_mod_density = 0;
  ModId ceiling_grass_mod = NoModId;
  float ceiling_grass_mod_density = 0;

  List<BiomeItemDistribution> item_distributions;
};

struct Biome {
  [[nodiscard]]
  static std::expected<Biome, StarException> create(Root& root, Json const& store);

  [[nodiscard]]
  std::expected<Json, StarException> to_json() const;

  String base_name;
  String description;

  MaterialId main_block = EmptyMaterialId;
  List<MaterialId> sub_blocks;
  // Pairs the ore type with the commonality multiplier.
  List<std::pair<ModId, float>> ores;

  float hue_shift = 0;
  MaterialHue material_hue_shift = 0;

  BiomePlaceables surface_placeables;
  BiomePlaceables underground_placeables;

  SpawnProfile spawn_profile;

  std::shared_ptr<Parallax> parallax;

  std::shared_ptr<AmbientNoisesDescription const> ambient_noises;
  std::shared_ptr<AmbientNoisesDescription const> music_track;
};

}

export module star.biome;

export namespace Star {
  using ::Star::AmbientNoisesDescription;
  using ::Star::Parallax;
  using ::Star::BiomePlaceables;
  using ::Star::Biome;
}

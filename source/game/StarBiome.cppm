module;
import std;
import star.format;
import star.string;
import star.json;
import star.bi_map;
import star.random;
import star.perlin;
import star.weighted_pool;
import star.id_map;
import star.set;
import star.casting;
import star.data_stream;
import star.game_types;
import star.material_types;
import star.list;
import star.image_processing;
import star.hash;
import star.string_view;
import star.thread;
import star.directives;
import star.asset_path;
import star.strong_typedef;


import star.damage_types;
import star.world_geometry;
import star.status_types;
import star.damage;
import star.light_source;
import star.entity;
import star.interaction_types;
import star.item_descriptor;
import star.celestial_coordinate;
import star.quest_descriptor;
import star.interactive_entity;
import star.collision_block;
import star.tile_entity;
import star.tile_damage;
import star.inspectable_entity;
import star.plant;
import star.plant_database;
import star.biome_placement;

import star.spawn_type_database;

namespace Star {

struct AmbientNoisesDescription;
class Parallax;
class Assets;
class Root;

struct BiomePlaceables {
  BiomePlaceables();
  [[nodiscard]]
  static std::expected<BiomePlaceables, StarException> create(Assets const& assets, Json const& json);

  [[nodiscard]]
  std::expected<Json, StarException> toJson() const;

  // If any of the item distributions contain trees, this returns the first
  // tree type.
  std::optional<TreeVariant> firstTreeType() const;

  ModId grassMod;
  float grassModDensity;
  ModId ceilingGrassMod;
  float ceilingGrassModDensity;

  List<BiomeItemDistribution> itemDistributions;
};

struct Biome {
  Biome();
  [[nodiscard]]
  static std::expected<Biome, StarException> create(Root& root, Json const& store);

  [[nodiscard]]
  std::expected<Json, StarException> toJson() const;

  String baseName;
  String description;

  MaterialId mainBlock;
  List<MaterialId> subBlocks;
  // Pairs the ore type with the commonality multiplier.
  List<std::pair<ModId, float>> ores;

  float hueShift;
  MaterialHue materialHueShift;

  BiomePlaceables surfacePlaceables;
  BiomePlaceables undergroundPlaceables;

  SpawnProfile spawnProfile;

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

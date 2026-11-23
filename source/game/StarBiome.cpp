import std;
import star.format;
import star.json;
import star.json_extra;
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
import star.color;

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
import star.inspectable_entity;
import star.plant;
import star.plant_database;
import star.biome_placement;
import star.spawn_type_database;
import star.biome;
import star.tile_damage;
import star.parallax;
import star.ambient;
import star.root;
import star.assets;

import star.algorithm;
namespace Star {

BiomePlaceables::BiomePlaceables() {
  grassMod = NoModId;
  grassModDensity = 0.0f;
  ceilingGrassMod = NoModId;
  ceilingGrassModDensity = 0.0f;
}

std::expected<BiomePlaceables, StarException> BiomePlaceables::create(Assets const& assets, Json const& variant) {
  BiomePlaceables result;
  {
    auto value = variant.get_int("grassMod");
    if (!value) return std::unexpected(std::move(value.error()));
    result.grassMod = std::move(*value);
  }
  {
    auto value = variant.get_float("grassModDensity");
    if (!value) return std::unexpected(std::move(value.error()));
    result.grassModDensity = std::move(*value);
  }
  {
    auto value = variant.get_int("ceilingGrassMod");
    if (!value) return std::unexpected(std::move(value.error()));
    result.ceilingGrassMod = std::move(*value);
  }
  {
    auto value = variant.get_float("ceilingGrassModDensity");
    if (!value) return std::unexpected(std::move(value.error()));
    result.ceilingGrassModDensity = std::move(*value);
  }
  auto items = variant.get_array("itemDistributions");
  if (!items) return std::unexpected(std::move(items.error()));
  for (auto const& item : *items) {
    auto distribution = BiomeItemDistribution::create(assets, item);
    if (!distribution) return std::unexpected(std::move(distribution.error()));
    result.itemDistributions.push_back(std::move(*distribution));
  }
  return result;
}

std::expected<Json, StarException> BiomePlaceables::toJson() const {
  JsonArray distributions;
  for (auto const& distribution : itemDistributions) {
    auto json = distribution.toJson();
    if (!json) return std::unexpected(std::move(json.error()));
    distributions.push_back(std::move(*json));
  }
  return JsonObject{{"grassMod", grassMod}, {"grassModDensity", grassModDensity}, {"ceilingGrassMod", ceilingGrassMod},
      {"ceilingGrassModDensity", ceilingGrassModDensity}, {"itemDistributions", std::move(distributions)}};
}

std::optional<TreeVariant> BiomePlaceables::firstTreeType() const {
  for (auto const& itemDistribution : itemDistributions) {
    for (auto const& biomeItem : itemDistribution.allItems()) {
      if (std::holds_alternative<TreePair>(biomeItem))
        return std::get<TreePair>(biomeItem).first;
    }
  }
  return {};
}

Biome::Biome() {
  mainBlock = EmptyMaterialId;
  hueShift = 0.0f;
  materialHueShift = MaterialHue();
}

std::expected<Biome, StarException> Biome::create(Root& root, Json const& store) {
  Biome result;
  {
    auto value = store.get_string("baseName");
    if (!value) return std::unexpected(std::move(value.error()));
    result.baseName = std::move(*value);
  }
  {
    auto value = store.get_string("description");
    if (!value) return std::unexpected(std::move(value.error()));
    result.description = std::move(*value);
  }
  {
    auto value = store.get_uint("mainBlock");
    if (!value) return std::unexpected(std::move(value.error()));
    result.mainBlock = std::move(*value);
  }
  {
    auto value = store.get_float("hueShift");
    if (!value) return std::unexpected(std::move(value.error()));
    result.hueShift = std::move(*value);
  }
  {
    auto value = store.get_uint("materialHueShift");
    if (!value) return std::unexpected(std::move(value.error()));
    result.materialHueShift = std::move(*value);
  }
  auto blocks = store.get_array("subBlocks");
  if (!blocks) return std::unexpected(std::move(blocks.error()));
  for (auto const& entry : *blocks) {
    auto block = entry.to_uint();
    if (!block) return std::unexpected(std::move(block.error()));
    result.subBlocks.push_back(*block);
  }
  auto ores = store.get_array("ores");
  if (!ores) return std::unexpected(std::move(ores.error()));
  for (auto const& entry : *ores) {
    auto mod = entry.get_uint(0);
    if (!mod) return std::unexpected(std::move(mod.error()));
    auto density = entry.get_float(1);
    if (!density) return std::unexpected(std::move(density.error()));
    result.ores.push_back({static_cast<ModId>(*mod), *density});
  }
  auto assets = root.assets();
  if (!assets) return std::unexpected(std::move(assets.error()));
  auto surfacePlaceablesConfig = store.get("surfacePlaceables");
  if (!surfacePlaceablesConfig) return std::unexpected(std::move(surfacePlaceablesConfig.error()));
  {
    auto value = BiomePlaceables::create(**assets, *surfacePlaceablesConfig);
    if (!value) return std::unexpected(std::move(value.error()));
    result.surfacePlaceables = std::move(*value);
  }
  auto undergroundPlaceablesConfig = store.get("undergroundPlaceables");
  if (!undergroundPlaceablesConfig) return std::unexpected(std::move(undergroundPlaceablesConfig.error()));
  {
    auto value = BiomePlaceables::create(**assets, *undergroundPlaceablesConfig);
    if (!value) return std::unexpected(std::move(value.error()));
    result.undergroundPlaceables = std::move(*value);
  }
  auto spawnConfig = store.opt("spawnProfile");
  if (!spawnConfig) return std::unexpected(std::move(spawnConfig.error()));
  if (*spawnConfig) {
    {
      auto value = SpawnProfile::create(**spawnConfig);
      if (!value) return std::unexpected(std::move(value.error()));
      result.spawnProfile = std::move(*value);
    }
  }
  auto parallaxConfig = store.opt("parallax");
  if (!parallaxConfig) return std::unexpected(std::move(parallaxConfig.error()));
  if (*parallaxConfig) {
    {
      auto value = Parallax::create(root, **parallaxConfig);
      if (!value) return std::unexpected(std::move(value.error()));
      result.parallax = std::move(*value);
    }
  }
  auto ambient_noises_config = store.opt("ambientNoises");
  if (!ambient_noises_config) return std::unexpected(std::move(ambient_noises_config.error()));
  if (*ambient_noises_config) {
    auto description = AmbientNoisesDescription::create(**ambient_noises_config);
    if (!description) return std::unexpected(std::move(description.error()));
    result.ambient_noises = std::make_shared<AmbientNoisesDescription const>(std::move(*description));
  }
  auto music_track_config = store.opt("musicTrack");
  if (!music_track_config) return std::unexpected(std::move(music_track_config.error()));
  if (*music_track_config) {
    auto description = AmbientNoisesDescription::create(**music_track_config);
    if (!description) return std::unexpected(std::move(description.error()));
    result.music_track = std::make_shared<AmbientNoisesDescription const>(std::move(*description));
  }
  return result;
}

std::expected<Json, StarException> Biome::toJson() const {
  auto surface = surfacePlaceables.toJson();
  if (!surface) return std::unexpected(std::move(surface.error()));
  auto underground = undergroundPlaceables.toJson();
  if (!underground) return std::unexpected(std::move(underground.error()));
  Json parallaxStore;
  if (parallax) {
    {
      auto value = parallax->store();
      if (!value) return std::unexpected(std::move(value.error()));
      parallaxStore = std::move(*value);
    }
  }
  return JsonObject{{"baseName", baseName}, {"description", description}, {"mainBlock", mainBlock},
      {"subBlocks", subBlocks.transformed(Construct<Json>())},
      {"ores", ores.transformed([](std::pair<ModId, float> const& p) -> Json { return JsonArray{p.first, p.second}; })},
      {"hueShift", hueShift}, {"materialHueShift", materialHueShift}, {"surfacePlaceables", std::move(*surface)},
      {"undergroundPlaceables", std::move(*underground)}, {"spawnProfile", spawnProfile.toJson()},
      {"parallax", std::move(parallaxStore)}, {"ambientNoises", ambient_noises ? ambient_noises->to_json() : Json()},
      {"musicTrack", music_track ? music_track->to_json() : Json()}};
}

}

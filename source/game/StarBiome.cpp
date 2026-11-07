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
    auto value = variant.getInt("grassMod");
    if (!value) return std::unexpected(std::move(value.error()));
    result.grassMod = std::move(*value);
  }
  {
    auto value = variant.getFloat("grassModDensity");
    if (!value) return std::unexpected(std::move(value.error()));
    result.grassModDensity = std::move(*value);
  }
  {
    auto value = variant.getInt("ceilingGrassMod");
    if (!value) return std::unexpected(std::move(value.error()));
    result.ceilingGrassMod = std::move(*value);
  }
  {
    auto value = variant.getFloat("ceilingGrassModDensity");
    if (!value) return std::unexpected(std::move(value.error()));
    result.ceilingGrassModDensity = std::move(*value);
  }
  auto items = variant.getArray("itemDistributions");
  if (!items) return std::unexpected(std::move(items.error()));
  for (auto const& item : *items) {
    auto distribution = BiomeItemDistribution::create(assets, item);
    if (!distribution) return std::unexpected(std::move(distribution.error()));
    result.itemDistributions.append(std::move(*distribution));
  }
  return result;
}

std::expected<Json, StarException> BiomePlaceables::toJson() const {
  JsonArray distributions;
  for (auto const& distribution : itemDistributions) {
    auto json = distribution.toJson();
    if (!json) return std::unexpected(std::move(json.error()));
    distributions.append(std::move(*json));
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
    auto value = store.getString("baseName");
    if (!value) return std::unexpected(std::move(value.error()));
    result.baseName = std::move(*value);
  }
  {
    auto value = store.getString("description");
    if (!value) return std::unexpected(std::move(value.error()));
    result.description = std::move(*value);
  }
  {
    auto value = store.getUInt("mainBlock");
    if (!value) return std::unexpected(std::move(value.error()));
    result.mainBlock = std::move(*value);
  }
  {
    auto value = store.getFloat("hueShift");
    if (!value) return std::unexpected(std::move(value.error()));
    result.hueShift = std::move(*value);
  }
  {
    auto value = store.getUInt("materialHueShift");
    if (!value) return std::unexpected(std::move(value.error()));
    result.materialHueShift = std::move(*value);
  }
  auto blocks = store.getArray("subBlocks");
  if (!blocks) return std::unexpected(std::move(blocks.error()));
  for (auto const& entry : *blocks) {
    auto block = entry.toUInt();
    if (!block) return std::unexpected(std::move(block.error()));
    result.subBlocks.append(*block);
  }
  auto ores = store.getArray("ores");
  if (!ores) return std::unexpected(std::move(ores.error()));
  for (auto const& entry : *ores) {
    auto mod = entry.getUInt(0);
    if (!mod) return std::unexpected(std::move(mod.error()));
    auto density = entry.getFloat(1);
    if (!density) return std::unexpected(std::move(density.error()));
    result.ores.append({static_cast<ModId>(*mod), *density});
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
  auto ambientNoisesConfig = store.opt("ambientNoises");
  if (!ambientNoisesConfig) return std::unexpected(std::move(ambientNoisesConfig.error()));
  if (*ambientNoisesConfig) {
    auto description = AmbientNoisesDescription::create(**ambientNoisesConfig);
    if (!description) return std::unexpected(std::move(description.error()));
    result.ambientNoises = std::make_shared<AmbientNoisesDescription>(std::move(*description));
  }
  auto musicTrackConfig = store.opt("musicTrack");
  if (!musicTrackConfig) return std::unexpected(std::move(musicTrackConfig.error()));
  if (*musicTrackConfig) {
    auto description = AmbientNoisesDescription::create(**musicTrackConfig);
    if (!description) return std::unexpected(std::move(description.error()));
    result.musicTrack = std::make_shared<AmbientNoisesDescription>(std::move(*description));
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
      {"subBlocks", subBlocks.transformed(construct<Json>())},
      {"ores", ores.transformed([](std::pair<ModId, float> const& p) -> Json { return JsonArray{p.first, p.second}; })},
      {"hueShift", hueShift}, {"materialHueShift", materialHueShift}, {"surfacePlaceables", std::move(*surface)},
      {"undergroundPlaceables", std::move(*underground)}, {"spawnProfile", spawnProfile.toJson()},
      {"parallax", std::move(parallaxStore)}, {"ambientNoises", ambientNoises ? ambientNoises->toJson() : Json()},
      {"musicTrack", musicTrack ? musicTrack->toJson() : Json()}};
}

}

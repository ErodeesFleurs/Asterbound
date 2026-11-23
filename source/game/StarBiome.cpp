import std;
import star.format;
import star.json;
import star.string;
import star.material_types;
import star.list;
import star.biome_placement;
import star.plant_database;
import star.spawn_type_database;
import star.biome;
import star.parallax;
import star.ambient;
import star.root;
import star.assets;

namespace Star {
namespace {

template <std::unsigned_integral T>
std::expected<T, StarException> stored_id(Json const& entry, std::string_view field) {
  // All MaterialId, ModId and MaterialHue values are exactly representable in a double.
  static_assert(std::numeric_limits<T>::digits <= std::numeric_limits<double>::digits);
  auto number = entry.to_double();
  if (!number) return std::unexpected(std::move(number.error()));
  if (!(*number >= 0 && *number <= std::numeric_limits<T>::max()) || std::trunc(*number) != *number)
    return std::unexpected(BiomeException::format("Invalid stored {}: {}", field, *number));
  return static_cast<T>(*number);
}

template <std::unsigned_integral T>
std::expected<T, StarException> stored_field(Json const& store, String const& field) {
  auto entry = store.get(field);
  if (!entry) return std::unexpected(std::move(entry.error()));
  return stored_id<T>(*entry, field.utf8());
}

std::expected<std::shared_ptr<AmbientNoisesDescription const>, StarException>
stored_ambient(Json const& store, String const& key) {
  auto config = store.opt(key);
  if (!config) return std::unexpected(std::move(config.error()));
  if (!*config) return {};
  auto description = AmbientNoisesDescription::create(**config);
  if (!description) return std::unexpected(std::move(description.error()));
  return std::make_shared<AmbientNoisesDescription const>(std::move(*description));
}

}

std::expected<BiomePlaceables, StarException> BiomePlaceables::create(Assets const& assets, Json const& variant) {
  BiomePlaceables result;
  auto grass = stored_field<ModId>(variant, "grassMod");
  if (!grass) return std::unexpected(std::move(grass.error()));
  result.grass_mod = *grass;
  auto density = variant.get_float("grassModDensity");
  if (!density) return std::unexpected(std::move(density.error()));
  result.grass_mod_density = *density;
  auto ceiling = stored_field<ModId>(variant, "ceilingGrassMod");
  if (!ceiling) return std::unexpected(std::move(ceiling.error()));
  result.ceiling_grass_mod = *ceiling;
  auto ceiling_density = variant.get_float("ceilingGrassModDensity");
  if (!ceiling_density) return std::unexpected(std::move(ceiling_density.error()));
  result.ceiling_grass_mod_density = *ceiling_density;
  auto items = variant.get("itemDistributions").and_then(&Json::array_ptr);
  if (!items) return std::unexpected(std::move(items.error()));
  result.item_distributions.reserve((*items)->size());
  for (auto const& item : **items) {
    auto distribution = BiomeItemDistribution::create(assets, item);
    if (!distribution) return std::unexpected(std::move(distribution.error()));
    result.item_distributions.push_back(std::move(*distribution));
  }
  return result;
}

std::expected<Json, StarException> BiomePlaceables::to_json() const {
  JsonArray distributions;
  distributions.reserve(item_distributions.size());
  for (auto const& distribution : item_distributions) {
    auto json = distribution.to_json();
    if (!json) return std::unexpected(std::move(json.error()));
    distributions.push_back(std::move(*json));
  }
  return JsonObject{{"grassMod", grass_mod}, {"grassModDensity", grass_mod_density}, {"ceilingGrassMod", ceiling_grass_mod},
      {"ceilingGrassModDensity", ceiling_grass_mod_density}, {"itemDistributions", std::move(distributions)}};
}

std::optional<std::reference_wrapper<TreeVariant const>> BiomePlaceables::first_tree() const & {
  for (auto const& distribution : item_distributions)
    if (auto tree = distribution.first_tree())
      return tree;
  return {};
}

std::expected<Biome, StarException> Biome::create(Root& root, Json const& store) {
  Biome result;
  {
    auto value = store.get_string("baseName");
    if (!value) return std::unexpected(std::move(value.error()));
    result.base_name = std::move(*value);
  }
  {
    auto value = store.get_string("description");
    if (!value) return std::unexpected(std::move(value.error()));
    result.description = std::move(*value);
  }
  auto main = stored_field<MaterialId>(store, "mainBlock");
  if (!main) return std::unexpected(std::move(main.error()));
  result.main_block = *main;
  auto hue = store.get_float("hueShift");
  if (!hue) return std::unexpected(std::move(hue.error()));
  result.hue_shift = *hue;
  auto material_hue = stored_field<MaterialHue>(store, "materialHueShift");
  if (!material_hue) return std::unexpected(std::move(material_hue.error()));
  result.material_hue_shift = *material_hue;
  auto blocks = store.get("subBlocks").and_then(&Json::array_ptr);
  if (!blocks) return std::unexpected(std::move(blocks.error()));
  result.sub_blocks.reserve((*blocks)->size());
  for (auto const& entry : **blocks) {
    auto block = stored_id<MaterialId>(entry, "subBlocks");
    if (!block) return std::unexpected(std::move(block.error()));
    result.sub_blocks.push_back(*block);
  }
  auto ores = store.get("ores").and_then(&Json::array_ptr);
  if (!ores) return std::unexpected(std::move(ores.error()));
  result.ores.reserve((*ores)->size());
  for (auto const& entry : **ores) {
    auto mod = entry.get(0).and_then([](Json const& id) { return stored_id<ModId>(id, "ores"); });
    if (!mod) return std::unexpected(std::move(mod.error()));
    auto density = entry.get_float(1);
    if (!density) return std::unexpected(std::move(density.error()));
    result.ores.emplace_back(*mod, *density);
  }
  auto assets = root.assets();
  if (!assets) return std::unexpected(std::move(assets.error()));
  auto read_placeables = [&](String const& key) {
    return store.get(key).and_then([&](Json const& config) { return BiomePlaceables::create(**assets, config); });
  };
  auto surface = read_placeables("surfacePlaceables");
  if (!surface) return std::unexpected(std::move(surface.error()));
  result.surface_placeables = std::move(*surface);
  auto underground = read_placeables("undergroundPlaceables");
  if (!underground) return std::unexpected(std::move(underground.error()));
  result.underground_placeables = std::move(*underground);
  auto spawn_config = store.opt("spawnProfile");
  if (!spawn_config) return std::unexpected(std::move(spawn_config.error()));
  if (*spawn_config) {
    auto profile = SpawnProfile::create(**spawn_config);
    if (!profile) return std::unexpected(std::move(profile.error()));
    result.spawn_profile = std::move(*profile);
  }
  auto parallax_config = store.opt("parallax");
  if (!parallax_config) return std::unexpected(std::move(parallax_config.error()));
  if (*parallax_config) {
    auto parsed = Parallax::create(root, **parallax_config);
    if (!parsed) return std::unexpected(std::move(parsed.error()));
    result.parallax = std::move(*parsed);
  }
  auto ambient = stored_ambient(store, "ambientNoises");
  if (!ambient) return std::unexpected(std::move(ambient.error()));
  result.ambient_noises = std::move(*ambient);
  auto music = stored_ambient(store, "musicTrack");
  if (!music) return std::unexpected(std::move(music.error()));
  result.music_track = std::move(*music);
  return result;
}

std::expected<Json, StarException> Biome::to_json() const {
  auto surface = surface_placeables.to_json();
  if (!surface) return std::unexpected(std::move(surface.error()));
  auto underground = underground_placeables.to_json();
  if (!underground) return std::unexpected(std::move(underground.error()));
  Json parallax_store;
  if (parallax) {
    auto value = parallax->store();
    if (!value) return std::unexpected(std::move(value.error()));
    parallax_store = std::move(*value);
  }
  return JsonObject{{"baseName", base_name}, {"description", description}, {"mainBlock", main_block},
      {"subBlocks", sub_blocks.transformed([](MaterialId id) { return Json(id); })},
      {"ores", ores.transformed([](std::pair<ModId, float> const& ore) -> Json { return JsonArray{ore.first, ore.second}; })},
      {"hueShift", hue_shift}, {"materialHueShift", material_hue_shift}, {"surfacePlaceables", std::move(*surface)},
      {"undergroundPlaceables", std::move(*underground)}, {"spawnProfile", spawn_profile.toJson()},
      {"parallax", std::move(parallax_store)}, {"ambientNoises", ambient_noises ? ambient_noises->to_json() : Json()},
      {"musicTrack", music_track ? music_track->to_json() : Json()}};
}

}

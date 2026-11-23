import star.json;
import star.json_extra;
import star.random;
import star.material_types;
import star.list;
import star.asset_path;
import std;
import star.string;
import star.map;
import star.format;
import star.plant_database;
import star.biome_placement;
import star.spawn_type_database;
import star.biome;
import star.weather_types;
import star.sky_types;
import star.assets;
import star.root;
import star.parallax;
import star.stored_functions;
import star.ambient;

import star.biome_database;
import star.material_database;

import star.static_random;
namespace Star {
namespace {

// Missing and explicit null have the same meaning for typed configuration defaults.
std::expected<std::shared_ptr<JsonArray const>, StarException> optional_array(Json const& config, String const& key) {
  auto entry = config.find(key);
  if (!entry) return std::unexpected(std::move(entry.error()));
  if (!*entry || (*entry)->get().is_null()) return {};
  return (*entry)->get().array_ptr();
}

std::expected<std::optional<std::reference_wrapper<Json const>>, StarException> optional_object(Json const& config, String const& key) {
  auto entry = config.find(key);
  if (!entry || !*entry) return entry;
  auto const& value = (*entry)->get();
  if (value.is_null()) return {};
  // Preserve the typed accessor's diagnostic without copying its entire object.
  if (value.type() != Json::Type::Object) return std::unexpected(value.object_ptr().error());
  return entry;
}

std::expected<float, StarException> pick_hue_shift(Json const& config, std::uint64_t seed) {
  auto options = optional_array(config, "hueShiftOptions");
  if (!options) return std::unexpected(std::move(options.error()));
  if (!*options || (*options)->empty()) return 0.0f;
  auto selected = static_random_u32(seed, "BiomeHueShift") % (*options)->size();
  float hue = 0;
  // Invalid unselected options still invalidate the configuration.
  for (std::size_t i = 0; i < (*options)->size(); ++i) {
    auto value = (**options)[i].to_float();
    if (!value) return std::unexpected(std::move(value.error()));
    if (i == selected) hue = *value;
  }
  return hue;
}

template <typename Visitor>
  requires std::invocable<Visitor&, String const&, float>
      && std::same_as<std::invoke_result_t<Visitor&, String const&, float>, std::expected<void, StarException>>
std::expected<void, StarException> visit_ores(Root& root, Json const& config, float threat_level, Visitor&& visit) {
  auto distribution = config.find("ores");
  if (!distribution) return std::unexpected(std::move(distribution.error()));
  if (!*distribution || (*distribution)->get().is_null()) return {};
  auto functions = root.functionDatabase();
  if (!functions) return std::unexpected(std::move(functions.error()));
  auto function = (*functions)->configFunction((*distribution)->get());
  if (!function) return std::unexpected(std::move(function.error()));
  auto entries = (*function)->get(threat_level).and_then(&Json::iterate_array);
  if (!entries) return std::unexpected(std::move(entries.error()));
  for (auto const& entry : *entries) {
    auto density = entry.get_float(1);
    if (!density) return std::unexpected(std::move(density.error()));
    if (*density > 0) {
      auto name = entry.get(0).and_then(&Json::string_ptr);
      if (!name) return std::unexpected(std::move(name.error()));
      auto added = visit(**name, *density);
      if (!added) return added;
    }
  }
  return {};
}

std::expected<BiomePlaceables, StarException> read_placeables(Root& root, MaterialDatabase const& materials,
    Json const& config, std::uint64_t seed, float biome_hue_shift) {
  RandomSource random(seed);
  auto pick_mod = [&](String const& key) -> std::expected<ModId, StarException> {
    auto options = optional_array(config, key);
    if (!options) return std::unexpected(std::move(options.error()));
    if (!*options || (*options)->empty()) return NoModId;
    auto choice = random.sample_ref(**options);
    if (!choice) return std::unexpected(std::move(choice.error()));
    auto name = choice->get().string_ptr();
    if (!name) return std::unexpected(std::move(name.error()));
    return materials.modId(**name);
  };
  BiomePlaceables result;
  auto grass = pick_mod("grassMod");
  if (!grass) return std::unexpected(std::move(grass.error()));
  result.grass_mod = *grass;
  auto density = config.get_float("grassModDensity", 0);
  if (!density) return std::unexpected(std::move(density.error()));
  result.grass_mod_density = *density;
  auto ceiling = pick_mod("ceilingGrassMod");
  if (!ceiling) return std::unexpected(std::move(ceiling.error()));
  result.ceiling_grass_mod = *ceiling;
  auto ceiling_density = config.get_float("ceilingGrassModDensity", 0);
  if (!ceiling_density) return std::unexpected(std::move(ceiling_density.error()));
  result.ceiling_grass_mod_density = *ceiling_density;
  auto items = optional_array(config, "items");
  if (!items) return std::unexpected(std::move(items.error()));
  if (*items) {
    result.item_distributions.reserve((*items)->size());
    for (auto const& item : **items) {
      auto distribution = BiomeItemDistribution::create(root, item, random.next_u64(), biome_hue_shift);
      if (!distribution) return std::unexpected(std::move(distribution.error()));
      result.item_distributions.push_back(std::move(*distribution));
    }
  }
  return result;
}

}

std::expected<std::shared_ptr<BiomeDatabase>, StarException> BiomeDatabase::create(Root& root) {
  auto result = std::make_shared<BiomeDatabase>(ConstructionKey{}, root);
  auto assets = root.assets();
  if (!assets) return std::unexpected(std::move(assets.error()));
  auto scan_files = [&](String const& type, ConfigMap& map) -> std::expected<void, StarException> {
    auto files = (*assets)->scan_extension(type);
    if (!files) return std::unexpected(std::move(files.error()));
    auto queued = (*assets)->queue_jsons(files->get());
    if (!queued) return std::unexpected(std::move(queued.error()));
    for (auto const& file : files->get()) {
      auto const& path = file.string();
      auto read_file = [&]() -> std::expected<void, StarException> {
        auto parameters = (*assets)->json(path);
        if (!parameters)
          return std::unexpected(std::move(parameters.error()));
        if (parameters->is_null())
          return {};
        auto name = parameters->get_string("name");
        if (!name)
          return std::unexpected(std::move(name.error()));
        auto [entry, added] = map.try_emplace(std::move(*name), path, std::move(*parameters));
        if (!added)
          return std::unexpected(BiomeException(Star::format("Duplicate {} generator name '{}'", type, entry->first)));
        return {};
      };
      auto loaded = read_file();
      if (!loaded) return std::unexpected((*assets)->with_asset_context(std::move(loaded.error()), path, "Loading biome or weather"));
    }
    return {};
  };
  auto biomes = scan_files("biome", result->m_biomes);
  if (!biomes) return std::unexpected(std::move(biomes.error()));
  auto weathers = scan_files("weather", result->m_weathers);
  if (!weathers) return std::unexpected(std::move(weathers.error()));
  return result;
}

StringList BiomeDatabase::biome_names() const {
  return m_biomes.keys();
}

std::expected<float, StarException> BiomeDatabase::biome_hue_shift(String const& biome_name, std::uint64_t seed) const {
  auto config = m_biomes.get(biome_name);
  if (!config) return std::unexpected(std::move(config.error()));
  return pick_hue_shift(config->get().parameters, seed);
}

std::expected<bool, StarException> BiomeDatabase::biome_has_weather(String const& biome_name) const {
  auto config = m_biomes.get(biome_name);
  if (!config) return std::unexpected(std::move(config.error()));
  return config->get().parameters.contains("weather");
}

std::expected<WeatherPool, StarException> BiomeDatabase::biome_weathers(String const& biome_name, std::uint64_t seed, float threat_level) const {
  auto config = m_biomes.get(biome_name);
  if (!config) return std::unexpected(std::move(config.error()));
  auto weather = config->get().parameters.find("weather");
  if (!weather) return std::unexpected(std::move(weather.error()));
  if (!*weather) return WeatherPool();
  auto bin = binned_choice_from_json((*weather)->get(), threat_level);
  if (!bin) return std::unexpected(std::move(bin.error()));
  if (bin->is_null()) return WeatherPool();
  auto options = bin->array_ptr();
  if (!options) return std::unexpected(std::move(options.error()));
  if ((*options)->empty()) return WeatherPool();
  auto path = static_random_from(**options, seed, "WeatherPool");
  if (!path) return std::unexpected(std::move(path.error()));
  auto assets = m_root.assets();
  if (!assets) return std::unexpected(std::move(assets.error()));
  auto pool_config = (*assets)->fetch_json(path->get());
  if (!pool_config) return std::unexpected(std::move(pool_config.error()));
  return json_to_weighted_pool<String>(*pool_config);
}

std::expected<bool, StarException> BiomeDatabase::biome_is_airless(String const& biome_name) const {
  auto config = m_biomes.get(biome_name);
  if (!config) return std::unexpected(std::move(config.error()));
  return config->get().parameters.get_bool("airless", false);
}

std::expected<SkyColoring, StarException> BiomeDatabase::biome_sky_coloring(String const& biome_name, std::uint64_t seed) const {
  auto config = m_biomes.get(biome_name);
  if (!config) return std::unexpected(std::move(config.error()));
  auto options = optional_array(config->get().parameters, "skyOptions");
  if (!options) return std::unexpected(std::move(options.error()));
  if (!*options) return SkyColoring();
  auto option = static_random_from(**options, seed, "BiomeSkyOption");
  if (!option) return std::unexpected(std::move(option.error()));
  return SkyColoring::create(option->get());
}

std::expected<String, StarException> BiomeDatabase::biome_friendly_name(String const& biome_name) const {
  auto config = m_biomes.get(biome_name);
  if (!config) return std::unexpected(std::move(config.error()));
  return config->get().parameters.get_string("friendlyName");
}

std::expected<StringList, StarException> BiomeDatabase::biome_status_effects(String const& biome_name) const {
  auto config = m_biomes.get(biome_name);
  if (!config) return std::unexpected(std::move(config.error()));
  auto effects = config->get().parameters.opt("statusEffects");
  if (!effects) return std::unexpected(std::move(effects.error()));
  if (*effects) return json_to_string_list(**effects);
  return StringList();
}

std::expected<StringList, StarException> BiomeDatabase::biome_ores(String const& biome_name, float threat_level) const {
  StringList result;
  auto config = m_biomes.get(biome_name);
  if (!config) return std::unexpected(std::move(config.error()));
  auto parsed = visit_ores(m_root, config->get().parameters, threat_level,
      [&](String const& name, float) -> std::expected<void, StarException> {
        result.push_back(name);
        return {};
      });
  if (!parsed) return std::unexpected(std::move(parsed.error()));
  return result;
}

StringList BiomeDatabase::weather_names() const {
  return m_weathers.keys();
}

std::expected<WeatherType, StarException> BiomeDatabase::weather_type(String const& name) const {
  auto config = m_weathers.find(name);
  if (config == m_weathers.end()) return std::unexpected(BiomeException(Star::format("No such weather type '{}'", name)));
  auto result = WeatherType::create(m_root, config->second.parameters, config->second.path);
  if (!result && result.error().is<MapException>())
    return std::unexpected(BiomeException(Star::format("Required key not found in weather config {}", config->second.path), result.error()));
  return result;
}

std::expected<std::shared_ptr<Biome>, StarException> BiomeDatabase::create_biome(String const& biome_name, std::uint64_t seed, float vertical_mid_point, float threat_level) const {
  auto config = m_biomes.find(biome_name);
  if (config == m_biomes.end()) return std::unexpected(BiomeException(Star::format("No such biome '{}'", biome_name)));
  auto materials = m_root.materialDatabase();
  if (!materials) return std::unexpected(std::move(materials.error()));
  auto parse = [&]() -> std::expected<std::shared_ptr<Biome>, StarException> {
    RandomSource random(seed);
    auto const& parameters = config->second.parameters;
    auto biome = std::make_shared<Biome>();
    auto hue = pick_hue_shift(parameters, seed);
    if (!hue) return std::unexpected(std::move(hue.error()));
    biome->base_name = biome_name;
    {
      auto value = parameters.get_string("description", "");
      if (!value) return std::unexpected(std::move(value.error()));
      biome->description = std::move(*value);
    }
    auto main = parameters.opt_string("mainBlock");
    if (!main) return std::unexpected(std::move(main.error()));
    if (*main) {
      auto value = (*materials)->materialId(**main);
      if (!value) return std::unexpected(std::move(value.error()));
      biome->main_block = *value;
    }
    auto sub_blocks = optional_array(parameters, "subBlocks");
    if (!sub_blocks) return std::unexpected(std::move(sub_blocks.error()));
    if (*sub_blocks) {
      biome->sub_blocks.reserve((*sub_blocks)->size());
      for (auto const& entry : **sub_blocks) {
        auto name = entry.string_ptr();
        if (!name) return std::unexpected(std::move(name.error()));
        auto id = (*materials)->materialId(**name);
        if (!id) return std::unexpected(std::move(id.error()));
        biome->sub_blocks.push_back(*id);
      }
    }
    auto ores = visit_ores(m_root, parameters, threat_level,
        [&](String const& name, float density) -> std::expected<void, StarException> {
          auto id = (*materials)->modId(name);
          if (!id) return std::unexpected(std::move(id.error()));
          biome->ores.emplace_back(*id, density);
          return {};
        });
    if (!ores) return std::unexpected(std::move(ores.error()));
    auto placeables = [&](String const& key) -> std::expected<BiomePlaceables, StarException> {
      auto value = optional_object(parameters, key);
      if (!value) return std::unexpected(std::move(value.error()));
      auto placement_seed = random.next_u64();
      if (!*value) return BiomePlaceables{};
      return read_placeables(m_root, **materials, (*value)->get(), placement_seed, *hue);
    };
    auto surface = placeables("surfacePlaceables");
    if (!surface) return std::unexpected(std::move(surface.error()));
    biome->surface_placeables = std::move(*surface);
    auto underground = placeables("undergroundPlaceables");
    if (!underground) return std::unexpected(std::move(underground.error()));
    biome->underground_placeables = std::move(*underground);
    biome->hue_shift = *hue;
    biome->material_hue_shift = materialHueFromDegrees(*hue);
    auto parallax = parameters.opt_string("parallax");
    if (!parallax) return std::unexpected(std::move(parallax.error()));
    if (*parallax) {
      auto path = AssetPath::relative_to(config->second.path, **parallax);
      auto tree = biome->surface_placeables.first_tree();
      auto value = Parallax::create(m_root, path, seed, vertical_mid_point, *hue,
          tree.transform([](std::reference_wrapper<TreeVariant const> ref) { return ref.get(); }));
      if (!value) return std::unexpected(std::move(value.error()));
      biome->parallax = std::move(*value);
    }
    auto ambient = [&](String const& key) -> std::expected<std::shared_ptr<AmbientNoisesDescription const>, StarException> {
      auto value = optional_object(parameters, key);
      if (!value) return std::unexpected(std::move(value.error()));
      if (!*value) return {};
      auto description = AmbientNoisesDescription::create((*value)->get(), config->second.path);
      if (!description) return std::unexpected(std::move(description.error()));
      return std::make_shared<AmbientNoisesDescription const>(std::move(*description));
    };
    auto music = ambient("musicTrack");
    if (!music) return std::unexpected(std::move(music.error()));
    biome->music_track = std::move(*music);
    auto noises = ambient("ambientNoises");
    if (!noises) return std::unexpected(std::move(noises.error()));
    biome->ambient_noises = std::move(*noises);
    auto spawn_config = optional_object(parameters, "spawnProfile");
    if (!spawn_config) return std::unexpected(std::move(spawn_config.error()));
    if (*spawn_config) {
      auto assets = m_root.assets();
      if (!assets) return std::unexpected(std::move(assets.error()));
      auto value = constructSpawnProfile(**assets, (*spawn_config)->get(), seed);
      if (!value) return std::unexpected(std::move(value.error()));
      biome->spawn_profile = std::move(*value);
    }
    return biome;
  };
  auto result = parse();
  if (!result) return std::unexpected(BiomeException(Star::format("Failed to parse biome: '{}'", biome_name), result.error()));
  return result;
}

std::expected<Json, StarException> BiomeDatabase::biome_config(String const& biome_name) const {
  auto config = m_biomes.get(biome_name);
  if (!config) return std::unexpected(std::move(config.error()));
  return JsonObject{{"config", config->get().parameters}, {"name", biome_name}, {"path", config->get().path}};
}

}

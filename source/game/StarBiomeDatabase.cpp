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
import star.strong_typedef;
import star.color;
import star.image_processing;
import star.hash;
import star.string_view;
import star.thread;
import star.directives;
import star.asset_path;
import star.ordered_map;
import star.io_device;
import star.ref_ptr;
import star.logging;
import star.listener;
import std;
import star.string;
import star.map;
import star.format;
import star.version;
import star.parametric_function;
import star.multi_array_interpolator;
import star.multi_table;

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
import star.animation;
import star.particle;
import star.weather_types;
import star.sky_types;
import star.asset_source;
import star.assets;
import star.root_base;
import star.configuration;
import star.root;
import star.tile_damage;
import star.parallax;
import star.stored_functions;
import star.ambient;

import star.biome_database;
import star.material_database;

import star.static_random;
namespace Star {

std::expected<std::shared_ptr<BiomeDatabase>, StarException> BiomeDatabase::create(Root& root) {
  auto result = std::shared_ptr<BiomeDatabase>(new BiomeDatabase(root));
  auto assets = root.assets();
  if (!assets) return std::unexpected(std::move(assets.error()));
  auto scanFiles = [&](String const& type, ConfigMap& map) -> std::expected<void, StarException> {
    auto files = (*assets)->scanExtension(type);
    if (!files) return std::unexpected(std::move(files.error()));
    auto queued = (*assets)->queueJsons(files->get());
    if (!queued) return std::unexpected(std::move(queued.error()));
    for (auto const& file : files->get()) {
      auto const& path = file.string();
      auto read_file = [&]() -> std::expected<void, StarException> {
        auto parameters = (*assets)->json(path);
        if (!parameters)
          return std::unexpected(std::move(parameters.error()));
        if (parameters->isNull())
          return {};
        auto name = parameters->getString("name");
        if (!name)
          return std::unexpected(std::move(name.error()));
        if (map.contains(*name))
          return std::unexpected(BiomeException(strf("Duplicate {} generator name '{}'", type, *name)));
        map[*name] = {path, *name, std::move(*parameters)};
        return {};
      };
      auto loaded = read_file();
      if (!loaded) return std::unexpected((*assets)->withAssetContext(std::move(loaded.error()), path, "Loading biome or weather"));
    }
    return {};
  };
  auto biomes = scanFiles("biome", result->m_biomes);
  if (!biomes) return std::unexpected(std::move(biomes.error()));
  auto weathers = scanFiles("weather", result->m_weathers);
  if (!weathers) return std::unexpected(std::move(weathers.error()));
  return result;
}

StringList BiomeDatabase::biomeNames() const {
  return m_biomes.keys();
}

std::expected<float, StarException> BiomeDatabase::biomeHueShift(String const& biomeName, std::uint64_t seed) const {
  auto config = m_biomes.get(biomeName);
  if (!config) return std::unexpected(std::move(config.error()));
  auto options = config->get().parameters.get("hueShiftOptions", {});
  if (!options) return std::unexpected(std::move(options.error()));
  return pickHueShiftFromJson(std::move(*options), seed, "BiomeHueShift");
}

std::expected<bool, StarException> BiomeDatabase::biomeHasWeather(String const& biomeName) const {
  auto config = m_biomes.get(biomeName);
  if (!config) return std::unexpected(std::move(config.error()));
  return config->get().parameters.contains("weather");
}

std::expected<WeatherPool, StarException> BiomeDatabase::biomeWeathers(String const& biomeName, std::uint64_t seed, float threatLevel) const {
  auto config = m_biomes.get(biomeName);
  if (!config) return std::unexpected(std::move(config.error()));
  auto weather = config->get().parameters.get("weather", JsonArray{});
  if (!weather) return std::unexpected(std::move(weather.error()));
  auto bin = binnedChoiceFromJson(*weather, threatLevel);
  if (!bin) return std::unexpected(std::move(bin.error()));
  auto weatherList = bin->optArray();
  if (!weatherList) return std::unexpected(std::move(weatherList.error()));
  if (*weatherList && !(*weatherList)->empty()) {
    auto path = static_random_from(**weatherList, seed, "WeatherPool");
    if (!path) return std::unexpected(std::move(path.error()));
    auto assets = m_root.assets();
    if (!assets) return std::unexpected(std::move(assets.error()));
    auto poolConfig = (*assets)->fetchJson(path->get());
    if (!poolConfig) return std::unexpected(std::move(poolConfig.error()));
    return jsonToWeightedPool<String>(*poolConfig);
  }
  return WeatherPool();
}

std::expected<bool, StarException> BiomeDatabase::biomeIsAirless(String const& biomeName) const {
  auto config = m_biomes.get(biomeName);
  if (!config) return std::unexpected(std::move(config.error()));
  return config->get().parameters.getBool("airless", false);
}

std::expected<SkyColoring, StarException> BiomeDatabase::biomeSkyColoring(String const& biomeName, std::uint64_t seed) const {
  SkyColoring result;
  auto config = m_biomes.get(biomeName);
  if (!config) return std::unexpected(std::move(config.error()));
  auto options = config->get().parameters.optArray("skyOptions");
  if (!options) return std::unexpected(std::move(options.error()));
  if (*options) {
    auto option = static_random_from(**options, seed, "BiomeSkyOption");
    if (!option) return std::unexpected(std::move(option.error()));
    {
      auto colorConfig = option->get().get("mainColor");
      if (!colorConfig) return std::unexpected(std::move(colorConfig.error()));
      {
        auto value = jsonToColor(*colorConfig);
        if (!value) return std::unexpected(std::move(value.error()));
        result.mainColor = std::move(*value);
      }
    }
    {
      auto colorConfig = option->get().query("morningColors[0]");
      if (!colorConfig) return std::unexpected(std::move(colorConfig.error()));
      {
        auto value = jsonToColor(*colorConfig);
        if (!value) return std::unexpected(std::move(value.error()));
        result.morningColors.first = std::move(*value);
      }
    }
    {
      auto colorConfig = option->get().query("morningColors[1]");
      if (!colorConfig) return std::unexpected(std::move(colorConfig.error()));
      {
        auto value = jsonToColor(*colorConfig);
        if (!value) return std::unexpected(std::move(value.error()));
        result.morningColors.second = std::move(*value);
      }
    }
    {
      auto colorConfig = option->get().query("dayColors[0]");
      if (!colorConfig) return std::unexpected(std::move(colorConfig.error()));
      {
        auto value = jsonToColor(*colorConfig);
        if (!value) return std::unexpected(std::move(value.error()));
        result.dayColors.first = std::move(*value);
      }
    }
    {
      auto colorConfig = option->get().query("dayColors[1]");
      if (!colorConfig) return std::unexpected(std::move(colorConfig.error()));
      {
        auto value = jsonToColor(*colorConfig);
        if (!value) return std::unexpected(std::move(value.error()));
        result.dayColors.second = std::move(*value);
      }
    }
    {
      auto colorConfig = option->get().query("eveningColors[0]");
      if (!colorConfig) return std::unexpected(std::move(colorConfig.error()));
      {
        auto value = jsonToColor(*colorConfig);
        if (!value) return std::unexpected(std::move(value.error()));
        result.eveningColors.first = std::move(*value);
      }
    }
    {
      auto colorConfig = option->get().query("eveningColors[1]");
      if (!colorConfig) return std::unexpected(std::move(colorConfig.error()));
      {
        auto value = jsonToColor(*colorConfig);
        if (!value) return std::unexpected(std::move(value.error()));
        result.eveningColors.second = std::move(*value);
      }
    }
    {
      auto colorConfig = option->get().query("nightColors[0]");
      if (!colorConfig) return std::unexpected(std::move(colorConfig.error()));
      {
        auto value = jsonToColor(*colorConfig);
        if (!value) return std::unexpected(std::move(value.error()));
        result.nightColors.first = std::move(*value);
      }
    }
    {
      auto colorConfig = option->get().query("nightColors[1]");
      if (!colorConfig) return std::unexpected(std::move(colorConfig.error()));
      {
        auto value = jsonToColor(*colorConfig);
        if (!value) return std::unexpected(std::move(value.error()));
        result.nightColors.second = std::move(*value);
      }
    }
    {
      auto colorConfig = option->get().get("morningLightColor");
      if (!colorConfig) return std::unexpected(std::move(colorConfig.error()));
      {
        auto value = jsonToColor(*colorConfig);
        if (!value) return std::unexpected(std::move(value.error()));
        result.morningLightColor = std::move(*value);
      }
    }
    {
      auto colorConfig = option->get().get("dayLightColor");
      if (!colorConfig) return std::unexpected(std::move(colorConfig.error()));
      {
        auto value = jsonToColor(*colorConfig);
        if (!value) return std::unexpected(std::move(value.error()));
        result.dayLightColor = std::move(*value);
      }
    }
    {
      auto colorConfig = option->get().get("eveningLightColor");
      if (!colorConfig) return std::unexpected(std::move(colorConfig.error()));
      {
        auto value = jsonToColor(*colorConfig);
        if (!value) return std::unexpected(std::move(value.error()));
        result.eveningLightColor = std::move(*value);
      }
    }
    {
      auto colorConfig = option->get().get("nightLightColor");
      if (!colorConfig) return std::unexpected(std::move(colorConfig.error()));
      {
        auto value = jsonToColor(*colorConfig);
        if (!value) return std::unexpected(std::move(value.error()));
        result.nightLightColor = std::move(*value);
      }
    }
  }
  return result;
}

std::expected<String, StarException> BiomeDatabase::biomeFriendlyName(String const& biomeName) const {
  auto config = m_biomes.get(biomeName);
  if (!config) return std::unexpected(std::move(config.error()));
  return config->get().parameters.getString("friendlyName");
}

std::expected<StringList, StarException> BiomeDatabase::biomeStatusEffects(String const& biomeName) const {
  auto config = m_biomes.get(biomeName);
  if (!config) return std::unexpected(std::move(config.error()));
  auto effects = config->get().parameters.opt("statusEffects");
  if (!effects) return std::unexpected(std::move(effects.error()));
  if (*effects) return jsonToStringList(**effects);
  return StringList();
}

std::expected<StringList, StarException> BiomeDatabase::biomeOres(String const& biomeName, float threatLevel) const {
  StringList result;
  auto config = m_biomes.get(biomeName);
  if (!config) return std::unexpected(std::move(config.error()));
  auto distribution = config->get().parameters.get("ores", {});
  if (!distribution) return std::unexpected(std::move(distribution.error()));
  if (!distribution->isNull()) {
    auto database = m_root.functionDatabase();
    if (!database) return std::unexpected(std::move(database.error()));
    auto function = (*database)->configFunction(*distribution);
    if (!function) return std::unexpected(std::move(function.error()));
    auto ores = (*function)->get(threatLevel);
    if (!ores) return std::unexpected(std::move(ores.error()));
    auto entries = ores->iterateArray();
    if (!entries) return std::unexpected(std::move(entries.error()));
    for (auto const& entry : *entries) {
      auto density = entry.getFloat(1);
      if (!density) return std::unexpected(std::move(density.error()));
      if (*density > 0) {
        auto name = entry.getString(0);
        if (!name) return std::unexpected(std::move(name.error()));
        result.push_back(std::move(*name));
      }
    }
  }
  return result;
}

StringList BiomeDatabase::weatherNames() const {
  return m_weathers.keys();
}

std::expected<WeatherType, StarException> BiomeDatabase::weatherType(String const& name) const {
  if (!m_weathers.contains(name)) return std::unexpected(BiomeException(strf("No such weather type '{}'", name)));
  auto config = m_weathers.get(name);
  if (!config) return std::unexpected(std::move(config.error()));
  auto result = WeatherType::create(m_root, config->get().parameters, config->get().path);
  if (!result && result.error().is<MapException>())
    return std::unexpected(BiomeException(strf("Required key not found in weather config {}", config->get().path), result.error()));
  return result;
}

std::expected<std::shared_ptr<Biome>, StarException> BiomeDatabase::createBiome(String const& biomeName, std::uint64_t seed, float verticalMidPoint, float threatLevel) const {
  if (!m_biomes.contains(biomeName)) return std::unexpected(BiomeException(strf("No such biome '{}'", biomeName)));
  auto materials = m_root.materialDatabase();
  if (!materials) return std::unexpected(std::move(materials.error()));
  auto parse = [&]() -> std::expected<std::shared_ptr<Biome>, StarException> {
    RandomSource random(seed);
    auto config = m_biomes.get(biomeName);
    if (!config) return std::unexpected(std::move(config.error()));
    auto const& parameters = config->get().parameters;
    auto biome = std::make_shared<Biome>();
    auto hue = biomeHueShift(biomeName, seed);
    if (!hue) return std::unexpected(std::move(hue.error()));
    biome->baseName = biomeName;
    {
      auto value = parameters.getString("description", "");
      if (!value) return std::unexpected(std::move(value.error()));
      biome->description = std::move(*value);
    }
    auto main = parameters.optString("mainBlock");
    if (!main) return std::unexpected(std::move(main.error()));
    if (*main) {
      {
        auto value = (*materials)->materialId(**main);
        if (!value) return std::unexpected(std::move(value.error()));
        biome->mainBlock = std::move(*value);
      }
    }
    auto subBlocks = parameters.getArray("subBlocks", {});
    if (!subBlocks) return std::unexpected(std::move(subBlocks.error()));
    for (auto const& entry : *subBlocks) {
      auto name = entry.toString();
      if (!name) return std::unexpected(std::move(name.error()));
      auto id = (*materials)->materialId(*name);
      if (!id) return std::unexpected(std::move(id.error()));
      biome->subBlocks.push_back(*id);
    }
    auto oreConfig = parameters.get("ores", {});
    if (!oreConfig) return std::unexpected(std::move(oreConfig.error()));
    {
      auto value = readOres(*oreConfig, threatLevel);
      if (!value) return std::unexpected(std::move(value.error()));
      biome->ores = std::move(*value);
    }
    auto surfacePlaceablesConfig = parameters.getObject("surfacePlaceables", {});
    if (!surfacePlaceablesConfig) return std::unexpected(std::move(surfacePlaceablesConfig.error()));
    {
      auto value = readBiomePlaceables(*surfacePlaceablesConfig, random.randu64(), *hue);
      if (!value) return std::unexpected(std::move(value.error()));
      biome->surfacePlaceables = std::move(*value);
    }
    auto undergroundPlaceablesConfig = parameters.getObject("undergroundPlaceables", {});
    if (!undergroundPlaceablesConfig) return std::unexpected(std::move(undergroundPlaceablesConfig.error()));
    {
      auto value = readBiomePlaceables(*undergroundPlaceablesConfig, random.randu64(), *hue);
      if (!value) return std::unexpected(std::move(value.error()));
      biome->undergroundPlaceables = std::move(*value);
    }
    biome->hueShift = *hue;
    biome->materialHueShift = materialHueFromDegrees(biome->hueShift);
    auto parallax = parameters.optString("parallax");
    if (!parallax) return std::unexpected(std::move(parallax.error()));
    if (*parallax) {
      auto path = AssetPath::relative_to(config->get().path, **parallax);
      {
        auto value = Parallax::create(m_root, path, seed, verticalMidPoint, *hue, biome->surfacePlaceables.firstTreeType());
        if (!value) return std::unexpected(std::move(value.error()));
        biome->parallax = std::move(*value);
      }
    }
    auto musicTrackConfig = parameters.optObject("musicTrack");
    if (!musicTrackConfig) return std::unexpected(std::move(musicTrackConfig.error()));
    if (*musicTrackConfig) {
      auto description = AmbientNoisesDescription::create(**musicTrackConfig, config->get().path);
      if (!description) return std::unexpected(std::move(description.error()));
      biome->musicTrack = std::make_shared<AmbientNoisesDescription>(std::move(*description));
    }
    auto ambientNoisesConfig = parameters.optObject("ambientNoises");
    if (!ambientNoisesConfig) return std::unexpected(std::move(ambientNoisesConfig.error()));
    if (*ambientNoisesConfig) {
      auto description = AmbientNoisesDescription::create(**ambientNoisesConfig, config->get().path);
      if (!description) return std::unexpected(std::move(description.error()));
      biome->ambientNoises = std::make_shared<AmbientNoisesDescription>(std::move(*description));
    }
    auto spawnConfig = parameters.optObject("spawnProfile");
    if (!spawnConfig) return std::unexpected(std::move(spawnConfig.error()));
    if (*spawnConfig) {
      auto assets = m_root.assets();
      if (!assets) return std::unexpected(std::move(assets.error()));
      {
        auto value = constructSpawnProfile(**assets, **spawnConfig, seed);
        if (!value) return std::unexpected(std::move(value.error()));
        biome->spawnProfile = std::move(*value);
      }
    }
    return biome;
  };
  auto result = parse();
  if (!result) return std::unexpected(BiomeException(strf("Failed to parse biome: '{}'", biomeName), result.error()));
  return result;
}

std::expected<Json, StarException> BiomeDatabase::biomeConfig(String const& biomeName) const {
  auto config = m_biomes.get(biomeName);
  if (!config) return std::unexpected(std::move(config.error()));
  return config->get().toJson();
}

std::expected<float, StarException> BiomeDatabase::pickHueShiftFromJson(Json source, std::uint64_t seed, String const& key) {
  if (source.isNull()) return 0;
  auto options = jsonToFloatList(source);
  if (!options) return std::unexpected(std::move(options.error()));
  if (options->empty()) return 0;
  auto t = static_random_u32(seed, key);
  return (*options)[t % options->size()];
}

std::expected<BiomePlaceables, StarException> BiomeDatabase::readBiomePlaceables(Json const& config, std::uint64_t seed, float biomeHueShift) const {
  RandomSource rand(seed);
  BiomePlaceables result;
  {
    auto options = config.optArray("grassMod");
    if (!options) return std::unexpected(std::move(options.error()));
    if (*options && !(*options)->empty()) {
      auto choice = rand.randFrom(**options);
      if (!choice) return std::unexpected(std::move(choice.error()));
      auto name = choice->get().toString();
      if (!name) return std::unexpected(std::move(name.error()));
      auto materials = m_root.materialDatabase();
      if (!materials) return std::unexpected(std::move(materials.error()));
      {
        auto value = (*materials)->modId(*name);
        if (!value) return std::unexpected(std::move(value.error()));
        result.grassMod = std::move(*value);
      }
    }
  }
  {
    auto value = config.getFloat("grassModDensity", 0);
    if (!value) return std::unexpected(std::move(value.error()));
    result.grassModDensity = std::move(*value);
  }
  {
    auto options = config.optArray("ceilingGrassMod");
    if (!options) return std::unexpected(std::move(options.error()));
    if (*options && !(*options)->empty()) {
      auto choice = rand.randFrom(**options);
      if (!choice) return std::unexpected(std::move(choice.error()));
      auto name = choice->get().toString();
      if (!name) return std::unexpected(std::move(name.error()));
      auto materials = m_root.materialDatabase();
      if (!materials) return std::unexpected(std::move(materials.error()));
      {
        auto value = (*materials)->modId(*name);
        if (!value) return std::unexpected(std::move(value.error()));
        result.ceilingGrassMod = std::move(*value);
      }
    }
  }
  {
    auto value = config.getFloat("ceilingGrassModDensity", 0);
    if (!value) return std::unexpected(std::move(value.error()));
    result.ceilingGrassModDensity = std::move(*value);
  }
  auto items = config.getArray("items", {});
  if (!items) return std::unexpected(std::move(items.error()));
  for (auto const& item : *items) {
    auto distribution = BiomeItemDistribution::create(m_root, item, rand.randu64(), biomeHueShift);
    if (!distribution) return std::unexpected(std::move(distribution.error()));
    result.itemDistributions.push_back(std::move(*distribution));
  }
  return result;
}

std::expected<List<std::pair<ModId, float>>, StarException> BiomeDatabase::readOres(Json const& oreDistribution, float threatLevel) const {
  List<std::pair<ModId, float>> result;
  if (!oreDistribution.isNull()) {
    auto functions = m_root.functionDatabase();
    if (!functions) return std::unexpected(std::move(functions.error()));
    auto materials = m_root.materialDatabase();
    if (!materials) return std::unexpected(std::move(materials.error()));
    auto function = (*functions)->configFunction(oreDistribution);
    if (!function) return std::unexpected(std::move(function.error()));
    auto ores = (*function)->get(threatLevel);
    if (!ores) return std::unexpected(std::move(ores.error()));
    auto entries = ores->iterateArray();
    if (!entries) return std::unexpected(std::move(entries.error()));
    for (auto const& entry : *entries) {
      auto density = entry.getFloat(1);
      if (!density) return std::unexpected(std::move(density.error()));
      if (*density > 0) {
        auto name = entry.getString(0);
        if (!name) return std::unexpected(std::move(name.error()));
        auto id = (*materials)->modId(*name);
        if (!id) return std::unexpected(std::move(id.error()));
        result.push_back({*id, *density});
      }
    }
  }
  return result;
}

Json BiomeDatabase::Config::toJson() const {
  return JsonObject{
    {"config", parameters},
    {"name", name},
    {"path", path}
  };
}

}

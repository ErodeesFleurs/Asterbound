import star.json;
import star.json_extra;
import star.thread;
import star.ordered_map;
import star.bi_map;
import star.io_device;
import star.list;
import star.image_processing;
import star.hash;
import star.data_stream;
import star.string_view;
import star.directives;
import star.asset_path;
import star.ref_ptr;
import star.logging;
import star.listener;
import std;
import star.string;
import star.format;
import star.version;
import star.random;

import star.asset_source;
import star.assets;

import star.terrain_database;
import star.terrain_basic;
import star.terrain_composition;
import star.terrain_transform;
import star.terrain_perlin;
import star.terrain_island_surface;
import star.terrain_ridge_blocks;
import star.terrain_karst_cave;
import star.terrain_worm_cave;
import star.terrain_cache;

namespace Star {

TerrainSelectorParameters::TerrainSelectorParameters(RandomService& random) {
  seed = random.randu64();
  worldWidth = 0;
  baseHeight = 0;
  commonality = 1.0f;
}

std::expected<TerrainSelectorParameters, StarException> TerrainSelectorParameters::create(Json const& v) {
  TerrainSelectorParameters result;
  auto worldWidth = v.getUInt("worldWidth");
  if (!worldWidth)
    return std::unexpected(std::move(worldWidth.error()));
  result.worldWidth = *worldWidth;
  auto baseHeight = v.getFloat("baseHeight");
  if (!baseHeight)
    return std::unexpected(std::move(baseHeight.error()));
  result.baseHeight = *baseHeight;
  auto seed = v.getUInt("seed");
  if (!seed)
    return std::unexpected(std::move(seed.error()));
  result.seed = *seed;
  auto commonality = v.getFloat("commonality");
  if (!commonality)
    return std::unexpected(std::move(commonality.error()));
  result.commonality = *commonality;
  return result;
}

Json TerrainSelectorParameters::toJson() const {
  return JsonObject{
      {"worldWidth", worldWidth}, {"baseHeight", baseHeight}, {"seed", seed}, {"commonality", commonality}};
}

TerrainSelectorParameters TerrainSelectorParameters::withSeed(std::uint64_t seed) const {
  TerrainSelectorParameters copy = *this;
  copy.seed = seed;
  return copy;
}

TerrainSelectorParameters TerrainSelectorParameters::withCommonality(float commonality) const {
  TerrainSelectorParameters copy = *this;
  copy.commonality = commonality;
  return copy;
}

TerrainSelector::TerrainSelector(String type, Json config, TerrainSelectorParameters parameters)
  : type(std::move(type)), config(std::move(config)), parameters(std::move(parameters)) {}

TerrainSelector::~TerrainSelector() = default;

TerrainDatabase::TerrainDatabase(ConstructionKey, RandomService& random) : m_random(random) {}

std::expected<std::shared_ptr<TerrainDatabase>, StarException> TerrainDatabase::create(std::shared_ptr<Assets const> const& assets, RandomService& random) {
  auto result = std::make_shared<TerrainDatabase>(ConstructionKey{}, random);

  // The extension determines the selector type; retain scan and seed order.
  auto scanFiles = [&](String const& type, bool composed) -> std::expected<void, StarException> {
    auto files = assets->scanExtension(type);
    if (!files)
      return std::unexpected(std::move(files.error()));
    auto queued = assets->queueJsons(files->get());
    if (!queued)
      return std::unexpected(std::move(queued.error()));
    for (auto const& path : files->get()) {
      auto loaded = [&]() -> std::expected<void, StarException> {
        auto parameters = assets->json(path.string());
        if (!parameters)
          return std::unexpected(std::move(parameters.error()));
        auto name = parameters->getString("name");
        if (!name)
          return std::unexpected(std::move(name.error()));
        auto selectorType = type;
        if (composed) {
          auto configuredType = parameters->getString("type");
          if (!configuredType)
            return std::unexpected(std::move(configuredType.error()));
          selectorType = std::move(*configuredType);
        }
        if (result->m_terrainSelectors.contains(*name))
          return std::unexpected(TerrainException(strf("Duplicate {}terrain generator name '{}'", composed ? "composed " : "", *name)));
        result->m_terrainSelectors[*name] = {std::move(selectorType), std::move(*parameters)};
        return {};
      }();
      if (!loaded)
        return std::unexpected(assets->withAssetContext(std::move(loaded.error()), path.string(), "Loading terrain configuration"));
    }
    return {};
  };
  for (std::string_view type : {KarstCaveSelector::Name, WormCaveSelector::Name, RidgeBlocksSelector::Name}) {
    auto scanned = scanFiles(type, false);
    if (!scanned)
      return std::unexpected(std::move(scanned.error()));
  }
  auto composed = scanFiles("terrain", true);
  if (!composed)
    return std::unexpected(std::move(composed.error()));
  return result;
}

std::expected<std::shared_ptr<TerrainSelector const>, StarException> TerrainDatabase::createNamedSelector(String const& name, TerrainSelectorParameters const& parameters) const {
  if (auto config = m_terrainSelectors.find_ref(name))
    return createSelectorType(config->get().type, config->get().parameters, parameters);
  return std::unexpected(TerrainException(strf("No such terrain selector '{}'", name)));
}

std::expected<std::shared_ptr<TerrainSelector const>, StarException> TerrainDatabase::constantSelector(float value) {
  return createSelectorType(ConstantSelector::Name, JsonObject{{"value", value}}, TerrainSelectorParameters(m_random));
}

Json TerrainDatabase::storeSelector(std::shared_ptr<TerrainSelector const> const& selector) const {
  if (!selector)
    return {};

  return JsonObject{
    {"type", selector->type},
    {"config", selector->config},
    {"parameters", selector->parameters.toJson()}
  };
}

std::expected<std::shared_ptr<TerrainSelector const>, StarException> TerrainDatabase::loadSelector(Json const& store) const {
  if (store.isNull())
    return std::shared_ptr<TerrainSelector const>{};
  auto type = store.getString("type");
  if (!type)
    return std::unexpected(std::move(type.error()));
  auto config = store.get("config");
  if (!config)
    return std::unexpected(std::move(config.error()));
  auto storedParameters = store.get("parameters");
  if (!storedParameters)
    return std::unexpected(std::move(storedParameters.error()));
  auto parameters = TerrainSelectorParameters::create(*storedParameters);
  if (!parameters)
    return std::unexpected(std::move(parameters.error()));
  return createSelectorType(*type, *config, *parameters);
}

std::expected<std::shared_ptr<TerrainSelector const>, StarException> TerrainDatabase::createSelectorType(String const& type, Json const& config, TerrainSelectorParameters const& parameters) const {
  if (type == WormCaveSelector::Name)
    return WormCaveSelector::create(config, parameters);
  else if (type == KarstCaveSelector::Name)
    return KarstCaveSelector::create(config, parameters);
  else if (type == ConstantSelector::Name)
    return ConstantSelector::create(config, parameters);
  else if (type == MaxSelector::Name)
    return MaxSelector::create(config, parameters, *this);
  else if (type == MinMaxSelector::Name)
    return MinMaxSelector::create(config, parameters, *this);
  else if (type == IslandSurfaceSelector::Name)
    return IslandSurfaceSelector::create(config, parameters);
  else if (type == FlatSurfaceSelector::Name)
    return FlatSurfaceSelector::create(config, parameters);
  else if (type == DisplacementSelector::Name)
    return DisplacementSelector::create(config, parameters, *this);
  else if (type == RotateSelector::Name)
    return RotateSelector::create(config, parameters, *this);
  else if (type == MixSelector::Name)
    return MixSelector::create(config, parameters, *this);
  else if (type == PerlinSelector::Name)
    return PerlinSelector::create(config, parameters);
  else if (type == RidgeBlocksSelector::Name)
    return RidgeBlocksSelector::create(config, parameters);
  else if (type == CacheSelector::Name)
    return CacheSelector::create(config, parameters, *this);
  return std::unexpected(TerrainException(strf("Unknown terrain selector type '{}'", type)));
}

}

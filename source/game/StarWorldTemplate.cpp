import star.json;
import star.json_extra;
import star.ordered_map;
import star.lru_cache;
import star.color;
import star.list;
import star.rect;
import star.image_processing;
import star.hash;
import star.data_stream;
import star.string_view;
import star.thread;
import star.directives;
import star.asset_path;
import star.game_types;
import star.material_types;
import star.vector;
import star.poly;
import star.id_map;
import star.set;
import star.casting;
import star.interpolation;
import star.bi_map;
import star.random;
import star.perlin;
import star.weighted_pool;
import star.strong_typedef;
import star.io_device;
import star.ref_ptr;
import star.listener;
import std;
import star.string;
import star.optional;
import star.format;
import star.math_common;
import star.logging;
import star.image;

import star.animation;
import star.particle;
import star.weather_types;
import star.celestial_coordinate;
import star.sky_types;
import star.force_regions;
import star.world_parameters;
import star.celestial_parameters;
import star.world_layout;
import star.damage_types;
import star.world_geometry;
import star.status_types;
import star.damage;
import star.light_source;
import star.entity;
import star.interaction_types;
import star.item_descriptor;
import star.quest_descriptor;
import star.interactive_entity;
import star.collision_block;
import star.tile_entity;
import star.tile_damage;
import star.inspectable_entity;
import star.plant;
import star.celestial_types;
import star.sky_parameters;
import star.world_template;
import star.plant_database;
import star.biome_placement;
import star.spawn_type_database;
import star.biome;
import star.asset_source;
import star.assets;

import star.configuration;
import star.root;
import star.liquid_types;
import star.dungeon_generator;

import star.terrain_database;
import star.biome_database;
import star.celestial_database;
import star.ambient;


import star.static_random;
import star.algorithm;
namespace Star {

std::expected<std::shared_ptr<WorldTemplate>, StarException> WorldTemplate::create(Root& root, Vec2U const& size) {
  auto result = std::shared_ptr<WorldTemplate>(new WorldTemplate(root));
  auto initialized = result->initialize();
  if (!initialized) return std::unexpected(std::move(initialized.error()));
  result->m_geometry = size;
  return result;
}

std::expected<std::shared_ptr<WorldTemplate>, StarException> WorldTemplate::create(Root& root, CelestialCoordinate const& celestialCoordinate, std::shared_ptr<CelestialDatabase> const& celestialDatabase) {
  auto result = std::shared_ptr<WorldTemplate>(new WorldTemplate(root));
  auto initialized = result->initialize();
  if (!initialized) return std::unexpected(std::move(initialized.error()));
  auto parameters = celestialDatabase->parameters(celestialCoordinate);
  if (!parameters) return std::unexpected(std::move(parameters.error()));
  if (!*parameters)
    return std::unexpected(StarException("Celestial parameters for constructing WorldTemplate not found!"));
  result->m_celestialParameters = std::move(*parameters);
  result->install_world_parameters(result->m_celestialParameters->visitableParameters());
  if (!result->m_worldParameters)
    return std::unexpected(StarException("Cannot create WorldTemplate from non-visitable world"));
  auto sky = SkyParameters::create(root, celestialCoordinate, celestialDatabase);
  if (!sky) return std::unexpected(std::move(sky.error()));
  result->m_skyParameters = std::move(*sky);
  result->m_seed = result->m_celestialParameters->seed();
  result->m_geometry = WorldGeometry(result->m_worldParameters->worldSize);
  if (auto parameters = as<TerrestrialWorldParameters>(result->m_worldParameters)) {
    auto layout = WorldLayout::buildTerrestrialLayout(root, *parameters, result->m_seed);
    if (!layout) return std::unexpected(std::move(layout.error()));
    result->m_layout = std::make_shared<WorldLayout>(std::move(*layout));
  } else if (auto parameters = as<AsteroidsWorldParameters>(result->m_worldParameters)) {
    auto layout = WorldLayout::buildAsteroidsLayout(root, *parameters, result->m_seed);
    if (!layout) return std::unexpected(std::move(layout.error()));
    result->m_layout = std::make_shared<WorldLayout>(std::move(*layout));
  } else if (auto parameters = as<FloatingDungeonWorldParameters>(result->m_worldParameters)) {
    auto layout = WorldLayout::buildFloatingDungeonLayout(root, *parameters, result->m_seed);
    if (!layout) return std::unexpected(std::move(layout.error()));
    result->m_layout = std::make_shared<WorldLayout>(std::move(*layout));
  }
  auto name = result->determineWorldName();
  if (!name) return std::unexpected(std::move(name.error()));
  auto weather = result->setupWeatherDomains();
  if (!weather) return std::unexpected(std::move(weather.error()));
  return result;
}

std::expected<std::shared_ptr<WorldTemplate>, StarException> WorldTemplate::create(Root& root, std::shared_ptr<VisitableWorldParameters const> const& worldParameters, SkyParameters const& skyParameters, std::uint64_t seed) {
  auto result = std::shared_ptr<WorldTemplate>(new WorldTemplate(root));
  auto initialized = result->initialize();
  if (!initialized) return std::unexpected(std::move(initialized.error()));
  if (!worldParameters)
    return std::unexpected(StarException("Cannot create WorldTemplate from non-visitable world"));
  result->install_world_parameters(worldParameters);
  result->m_skyParameters = skyParameters;
  result->m_seed = seed;
  result->m_geometry = WorldGeometry(worldParameters->worldSize);
  if (auto parameters = as<TerrestrialWorldParameters>(worldParameters)) {
    auto layout = WorldLayout::buildTerrestrialLayout(root, *parameters, seed);
    if (!layout) return std::unexpected(std::move(layout.error()));
    result->m_layout = std::make_shared<WorldLayout>(std::move(*layout));
  } else if (auto parameters = as<AsteroidsWorldParameters>(worldParameters)) {
    auto layout = WorldLayout::buildAsteroidsLayout(root, *parameters, seed);
    if (!layout) return std::unexpected(std::move(layout.error()));
    result->m_layout = std::make_shared<WorldLayout>(std::move(*layout));
  } else if (auto parameters = as<FloatingDungeonWorldParameters>(worldParameters)) {
    auto layout = WorldLayout::buildFloatingDungeonLayout(root, *parameters, seed);
    if (!layout) return std::unexpected(std::move(layout.error()));
    result->m_layout = std::make_shared<WorldLayout>(std::move(*layout));
  }
  auto name = result->determineWorldName();
  if (!name) return std::unexpected(std::move(name.error()));
  auto weather = result->setupWeatherDomains();
  if (!weather) return std::unexpected(std::move(weather.error()));
  return result;
}

std::expected<std::shared_ptr<WorldTemplate>, StarException> WorldTemplate::create(Root& root, Json const& store) {
  auto result = std::shared_ptr<WorldTemplate>(new WorldTemplate(root));
  auto initialized = result->initialize();
  if (!initialized) return std::unexpected(std::move(initialized.error()));
  auto celestial = store.get("celestialParameters", {});
  if (!celestial) return std::unexpected(std::move(celestial.error()));
  if (!celestial->is_null()) {
    auto parameters = CelestialParameters::create(*celestial);
    if (!parameters) return std::unexpected(std::move(parameters.error()));
    result->m_celestialParameters = std::move(*parameters);
  }
  auto parametersJson = store.get("worldParameters", {});
  if (!parametersJson) return std::unexpected(std::move(parametersJson.error()));
  auto parameters = diskLoadVisitableWorldParameters(*parametersJson);
  if (!parameters) return std::unexpected(std::move(parameters.error()));
  result->install_world_parameters(std::move(*parameters));
  auto skyJson = store.get("skyParameters");
  if (!skyJson) return std::unexpected(std::move(skyJson.error()));
  auto sky = SkyParameters::create(*skyJson);
  if (!sky) return std::unexpected(std::move(sky.error()));
  result->m_skyParameters = std::move(*sky);
  auto seed = store.get_uint("seed");
  if (!seed) return std::unexpected(std::move(seed.error()));
  result->m_seed = *seed;
  auto sizeJson = store.get("size");
  if (!sizeJson) return std::unexpected(std::move(sizeJson.error()));
  auto size = json_to_vec2u(*sizeJson);
  if (!size) return std::unexpected(std::move(size.error()));
  result->m_geometry = WorldGeometry(*size);
  auto regionData = store.opt("regionData");
  if (!regionData) return std::unexpected(std::move(regionData.error()));
  if (*regionData) {
    auto layout = WorldLayout::create(root, **regionData);
    if (!layout) return std::unexpected(std::move(layout.error()));
    result->m_layout = std::move(*layout);
  }
  auto customRegions = store.get_array("customTerrainRegions", JsonArray());
  if (!customRegions) return std::unexpected(std::move(customRegions.error()));
  for (auto const& config : *customRegions) {
    auto regionJson = config.get("region");
    if (!regionJson) return std::unexpected(std::move(regionJson.error()));
    auto region = json_to_polyf(*regionJson);
    if (!region) return std::unexpected(std::move(region.error()));
    auto solid = config.get_bool("solid");
    if (!solid) return std::unexpected(std::move(solid.error()));
    auto bounds = region->bounding_box();
    result->m_customTerrainRegions.push_back({.region = std::move(*region), .regionBounds = bounds, .solid = *solid});
  }
  auto name = result->determineWorldName();
  if (!name) return std::unexpected(std::move(name.error()));
  auto weather = result->setupWeatherDomains();
  if (!weather) return std::unexpected(std::move(weather.error()));
  return result;
}

std::expected<Json, StarException> WorldTemplate::store() const {
  Json celestial;
  if (m_celestialParameters) {
    auto stored = m_celestialParameters->diskStore();
    if (!stored) return std::unexpected(std::move(stored.error()));
    celestial = std::move(*stored);
  }
  auto parameters = diskStoreVisitableWorldParameters(m_worldParameters);
  if (!parameters) return std::unexpected(std::move(parameters.error()));
  auto sky = m_skyParameters.toJson();
  if (!sky) return std::unexpected(std::move(sky.error()));
  Json regionData;
  if (m_layout) {
    auto stored = m_layout->toJson();
    if (!stored) return std::unexpected(std::move(stored.error()));
    regionData = std::move(*stored);
  }
  return JsonObject{
    {"celestialParameters", std::move(celestial)},
    {"worldParameters", std::move(*parameters)},
    {"skyParameters", std::move(*sky)},
    {"seed", m_seed},
    {"size", json_from_vec2u(m_geometry.size())},
    {"regionData", std::move(regionData)},
    {"customTerrainRegions", transform<JsonArray>(m_customTerrainRegions, [](CustomTerrainRegion const& region) {
      return JsonObject{{"region", json_from_polyf(region.region)}, {"solid", region.solid}};
    })}
  };
}

std::optional<CelestialParameters> const& WorldTemplate::celestialParameters() const {
  return m_celestialParameters;
}

std::shared_ptr<VisitableWorldParameters const> WorldTemplate::worldParameters() const {
  return m_worldParameters;
}

SkyParameters WorldTemplate::skyParameters() const {
  return m_skyParameters;
}

std::shared_ptr<WorldLayout> WorldTemplate::worldLayout() const {
  return m_layout;
}

void WorldTemplate::install_world_parameters(std::shared_ptr<VisitableWorldParameters const> parameters) {
  m_worldParameters = std::move(parameters);
  auto floating = as<FloatingDungeonWorldParameters>(m_worldParameters);
  if (!floating) {
    m_ambient_override.reset();
    m_music_override.reset();
    return;
  }

  auto make_override = [](std::optional<String> const& day, std::optional<String> const& night)
      -> std::shared_ptr<AmbientNoisesDescription const> {
    if (!day && !night)
      return {};
    auto description = std::make_shared<AmbientNoisesDescription>();
    if (day)
      description->day_sounds.tracks.push_back(*day);
    if (night)
      description->night_sounds.tracks.push_back(*night);
    return description;
  };
  m_ambient_override = make_override(floating->day_ambient_noises, floating->night_ambient_noises);
  m_music_override = make_override(floating->day_music_track, floating->night_music_track);
}

std::expected<void, StarException> WorldTemplate::setWorldParameters(std::shared_ptr<VisitableWorldParameters> newParameters) {
  install_world_parameters(std::move(newParameters));
  return setupWeatherDomains();
}

void WorldTemplate::setWorldLayout(std::shared_ptr<WorldLayout> newLayout) {
  m_layout = std::exchange(newLayout, {});
  m_blockCache.clear();
}

void WorldTemplate::setSkyParameters(SkyParameters newParameters) {
  // SkyParameters constructs an owning JSON object on reset; preserve the
  // move-before-allocation sequencing rather than constructing it up front.
  m_skyParameters = take(newParameters);
}

std::uint64_t WorldTemplate::worldSeed() const {
  return m_seed;
}

String WorldTemplate::worldName() const {
  return m_worldName;
}

Vec2U WorldTemplate::size() const {
  return m_geometry.size();
}

float WorldTemplate::surfaceLevel() const {
  if (auto terrestrialParameters = as<TerrestrialWorldParameters>(m_worldParameters))
    return terrestrialParameters->surfaceLayer.layerBaseHeight;
  return m_geometry.size()[1] / 2.0f;
}

float WorldTemplate::undergroundLevel() const {
  if (auto terrestrialParameters = as<TerrestrialWorldParameters>(m_worldParameters))
    return terrestrialParameters->surfaceLayer.layerMinHeight;
  else if (auto floatingDungeonParameters = as<FloatingDungeonWorldParameters>(m_worldParameters))
    return floatingDungeonParameters->dungeonUndergroundLevel;
  return 0.0f;
}

bool WorldTemplate::inSurfaceLayer(Vec2I const& position) const {
  if (auto terrestrialParameters = as<TerrestrialWorldParameters>(m_worldParameters)) {
    auto posLayerAndCell = m_layout->findLayerAndCell(position[0], position[1]);
    auto surfaceLayerAndCell = m_layout->findLayerAndCell(position[0], terrestrialParameters->surfaceLayer.layerBaseHeight);
    return posLayerAndCell.first == surfaceLayerAndCell.first;
  }
  return false;
}

std::expected<std::optional<Vec2I>, StarException> WorldTemplate::findSensiblePlayerStart() const {
  if (!m_layout) return {};
  auto playerStartSearchRegions = m_layout->playerStartSearchRegions();
  if (playerStartSearchRegions.empty()) return {};
  auto tries = m_templateConfig.get_int("playerStartSearchTries");
  if (!tries) return std::unexpected(std::move(tries.error()));
  auto radius = m_templateConfig.get_int("playerStartFreeBlocksRadius");
  if (!radius) return std::unexpected(std::move(radius.error()));
  auto height = m_templateConfig.get_int("playerStartFreeBlocksHeight");
  if (!height) return std::unexpected(std::move(height.error()));
  for (int i = 0; i < *tries; ++i) {
    auto searchRegion = this->root().random().sample_ref(playerStartSearchRegions);
    if (!searchRegion) return std::unexpected(std::move(searchRegion.error()));
    auto randomX = this->root().random().uniform_int(searchRegion->get().x_min(), searchRegion->get().x_max());
    if (!randomX) return std::unexpected(std::move(randomX.error()));
    int x = *randomX;
    for (int y = searchRegion->get().y_max() - 1; y >= searchRegion->get().y_min(); --y) {
      auto block = getBlockInfo(x, y);
      if (!block) return std::unexpected(std::move(block.error()));
      if (!block->terrain) continue;
      auto above = getBlockInfo(x, y + 1);
      if (!above) return std::unexpected(std::move(above.error()));
      if (!above->terrain) {
        auto outside = isOutside(RectI(x - *radius, y + 1, x + *radius, y + *height));
        if (!outside) return std::unexpected(std::move(outside.error()));
        if (*outside) return Vec2I(x, y + 1);
      }
    }
  }
  return {};
}

void WorldTemplate::addCustomTerrainRegion(PolyF poly) {
  auto bounds = poly.bounding_box();
  m_customTerrainRegions.push_back({.region = std::move(poly), .regionBounds = bounds, .solid = true});
  m_blockCache.clear();
}

void WorldTemplate::addCustomSpaceRegion(PolyF poly) {
  auto bounds = poly.bounding_box();
  m_customTerrainRegions.push_back({.region = std::move(poly), .regionBounds = bounds, .solid = false});
  m_blockCache.clear();
}

void WorldTemplate::clearCustomTerrains() {
  m_customTerrainRegions.clear();
  m_blockCache.clear();
}

std::expected<List<RectI>, StarException> WorldTemplate::previewAddBiomeRegion(Vec2I const& position, int width) {
  if (auto terrestrialParameters = as<TerrestrialWorldParameters>(m_worldParameters)) {
    auto regionRects = m_layout->previewAddBiomeRegion(position, width);
    if (!regionRects) return std::unexpected(std::move(regionRects.error()));
    regionRects->transform([terrestrialParameters](RectI const& rect) {
      return rect.padded(std::ceil(terrestrialParameters->blendSize));
    });
    return std::move(*regionRects);
  }
  auto logged = this->root().logger().error("Cannot add biome region to non-terrestrial world!");
  if (!logged) return std::unexpected(std::move(logged.error()));
  return List<RectI>();
}

std::expected<List<RectI>, StarException> WorldTemplate::previewExpandBiomeRegion(Vec2I const& position, int newWidth) {
  if (auto terrestrialParameters = as<TerrestrialWorldParameters>(m_worldParameters)) {
    auto regionRects = m_layout->previewExpandBiomeRegion(position, newWidth);
    if (!regionRects) return std::unexpected(std::move(regionRects.error()));
    regionRects->transform([terrestrialParameters](RectI const& rect) {
      return rect.padded(std::ceil(terrestrialParameters->blendSize));
    });
    return std::move(*regionRects);
  }
  auto logged = this->root().logger().error("Cannot expand biome region on non-terrestrial world!");
  if (!logged) return std::unexpected(std::move(logged.error()));
  return List<RectI>();
}

std::expected<void, StarException> WorldTemplate::addBiomeRegion(Vec2I const& position, String const& biomeName, String const& subBlockSelector, int width) {
  if (auto terrestrialParameters = as<TerrestrialWorldParameters>(m_worldParameters)) {
    auto added = m_layout->addBiomeRegion(*terrestrialParameters, m_seed, position, biomeName, subBlockSelector, width);
    if (!added) return std::unexpected(std::move(added.error()));
    m_blockCache.clear();
    return {};
  }
  return this->root().logger().error("Cannot add biome region to non-terrestrial world!");
}

std::expected<void, StarException> WorldTemplate::expandBiomeRegion(Vec2I const& position, int newWidth) {
  if (as<TerrestrialWorldParameters>(m_worldParameters)) {
    auto expanded = m_layout->expandBiomeRegion(position, newWidth);
    if (!expanded) return std::unexpected(std::move(expanded.error()));
    m_blockCache.clear();
    return {};
  }
  return this->root().logger().error("Cannot expand biome region on non-terrestrial world!");
}

std::expected<List<WorldTemplate::Dungeon>, StarException> WorldTemplate::dungeons() const {
  List<Dungeon> dungeonList;

  if (auto floatingDungeonParameters = as<FloatingDungeonWorldParameters>(m_worldParameters)) {
    dungeonList.push_back({.dungeon = floatingDungeonParameters->primaryDungeon,
                           .baseHeight = floatingDungeonParameters->dungeonBaseHeight,
                           .baseX = 0,
                           .xVariance = 0,
                           .force = true,
                           .blendWithTerrain = false});
  } else if (auto terrestrialParameters = as<TerrestrialWorldParameters>(m_worldParameters)) {
    auto addLayerDungeons = [&geometry = m_geometry, seed = m_seed, &dungeonList](TerrestrialWorldParameters::TerrestrialLayer const& layer) -> std::expected<void, StarException> {
      if (layer.dungeons.size() > 0) {
        int dungeonSpacing = std::floor(geometry.width() / layer.dungeons.size());
        auto offset = static_random_u32_range(0, geometry.width(), seed, layer.layerBaseHeight);
        if (!offset)
          return std::unexpected(std::move(offset.error()));
        std::uint32_t dungeonOffset = *offset;
        for (auto const& dungeon : layer.dungeons) {
          dungeonList.push_back({.dungeon = dungeon,
                                 .baseHeight = layer.layerBaseHeight,
                                 .baseX = static_cast<int>(dungeonOffset),
                                 .xVariance = layer.dungeonXVariance,
                                 .force = false,
                                 .blendWithTerrain = true});
          dungeonOffset = (dungeonOffset + dungeonSpacing) % geometry.width();
        }
      }
      return {};
    };

    {
      auto added = addLayerDungeons(terrestrialParameters->spaceLayer);
      if (!added) return std::unexpected(std::move(added.error()));
    }
    {
      auto added = addLayerDungeons(terrestrialParameters->atmosphereLayer);
      if (!added) return std::unexpected(std::move(added.error()));
    }
    {
      auto added = addLayerDungeons(terrestrialParameters->surfaceLayer);
      if (!added) return std::unexpected(std::move(added.error()));
    }
    {
      auto added = addLayerDungeons(terrestrialParameters->subsurfaceLayer);
      if (!added) return std::unexpected(std::move(added.error()));
    }
    for (auto const& ul : terrestrialParameters->undergroundLayers)
      {
      auto added = addLayerDungeons(ul);
      if (!added) return std::unexpected(std::move(added.error()));
    }
    {
      auto added = addLayerDungeons(terrestrialParameters->coreLayer);
      if (!added) return std::unexpected(std::move(added.error()));
    }
  }

  return dungeonList;
}

std::expected<WorldTemplate::BlockInfo, StarException> WorldTemplate::blockInfo(int x, int y) const {
  return getBlockInfo(m_geometry.xwrap(x), y);
}

std::expected<WorldTemplate::BlockInfo, StarException> WorldTemplate::blockBiomeInfo(int x, int y) const {
  BlockInfo blockInfo;
  if (!m_layout) return blockInfo;
  auto flatWeighting = m_layout->getWeighting(x, y);
  if (!flatWeighting) return std::unexpected(std::move(flatWeighting.error()));
  int blendNoiseOffset = 0;
  if (auto const& blendNoise = m_layout->blendNoise()) {
    auto noise = blendNoise->get(x, y);
    if (!noise) return std::unexpected(std::move(noise.error()));
    blendNoiseOffset = static_cast<int>(*noise);
  }
  Vec2I blockPos;
  List<WorldLayout::RegionWeighting> blockWeighting;
  List<WorldLayout::RegionWeighting> transitionWeighting;
  if (auto const& blockNoise = m_layout->blockNoise()) {
    auto position = blockNoise->apply(Vec2I(x, y), m_geometry.size());
    if (!position) return std::unexpected(std::move(position.error()));
    blockPos = *position;
    auto weighting = m_layout->getWeighting(blockPos[0] + blendNoiseOffset, blockPos[1]);
    if (!weighting) return std::unexpected(std::move(weighting.error()));
    blockWeighting = std::move(*weighting);
    weighting = m_layout->getWeighting(blockPos[0], blockPos[1]);
    if (!weighting) return std::unexpected(std::move(weighting.error()));
    transitionWeighting = std::move(*weighting);
  } else {
    blockPos = Vec2I(x, y);
    blockWeighting = *flatWeighting;
    transitionWeighting = *flatWeighting;
  }
  if (flatWeighting->empty() || blockWeighting.empty()) return blockInfo;
  auto const& primaryFlatWeighting = (*flatWeighting)[0];
  auto const& primaryBlockWeighting = blockWeighting[0];
  blockInfo.blockBiomeIndex = primaryBlockWeighting.region.get().blockBiomeIndex;
  blockInfo.environmentBiomeIndex = primaryFlatWeighting.region.get().environmentBiomeIndex;
  auto transition = transitionWeighting.first();
  if (!transition) return std::unexpected(std::move(transition.error()));
  auto threshold = m_templateConfig.get_float("biomeTransitionThreshold", 0);
  if (!threshold) return std::unexpected(std::move(threshold.error()));
  blockInfo.biomeTransition = transition->get().weight < *threshold;
  auto biome_result = biome(blockInfo.blockBiomeIndex);
  if (!biome_result) return std::unexpected(std::move(biome_result.error()));
  if (auto const& block_biome = *biome_result) {
    if (!blockInfo.foregroundCave) {
      blockInfo.foreground = block_biome->main_block;
      blockInfo.background = blockInfo.foreground;
    } else if (!blockInfo.backgroundCave) {
      blockInfo.background = block_biome->main_block;
    }
    if (!primaryBlockWeighting.region.get().subBlockSelectorIndexes.empty()) {
      for (std::size_t i = 0; i < block_biome->sub_blocks.size(); ++i) {
        auto index = primaryBlockWeighting.region.get().subBlockSelectorIndexes.at(i);
        if (!index) return std::unexpected(std::move(index.error()));
        auto selector = m_layout->getTerrainSelector(index->get());
        if (!selector) return std::unexpected(std::move(selector.error()));
        auto value = selector->get()->get(primaryBlockWeighting.xValue - blendNoiseOffset, blockPos[1]);
        if (!value) return std::unexpected(std::move(value.error()));
        if (*value > 0.0f) {
          if (!blockInfo.foregroundCave) {
            blockInfo.foreground = block_biome->sub_blocks[i];
            blockInfo.background = blockInfo.foreground;
          } else if (!blockInfo.backgroundCave) {
            blockInfo.background = block_biome->sub_blocks[i];
          }
          break;
        }
      }
    }
  }
  return blockInfo;
}

std::expected<bool, StarException> WorldTemplate::isOutside(int x, int y) const {
  auto block = getBlockInfo(m_geometry.xwrap(x), y);
  if (!block) return std::unexpected(std::move(block.error()));
  return !block->terrain;
}

std::expected<bool, StarException> WorldTemplate::isOutside(RectI const& region) const {
  for (int x = region.x_min(); x < region.x_max(); ++x) {
    for (int y = region.y_min(); y < region.y_max(); ++y) {
      auto block = getBlockInfo(m_geometry.xwrap(x), y);
      if (!block) return std::unexpected(std::move(block.error()));
      if (block->terrain) return false;
    }
  }
  return true;
}

std::expected<BiomeIndex, StarException> WorldTemplate::blockBiomeIndex(int x, int y) const {
  auto block = getBlockInfo(m_geometry.xwrap(x), y);
  if (!block) return std::unexpected(std::move(block.error()));
  return block->blockBiomeIndex;
}

std::expected<BiomeIndex, StarException> WorldTemplate::environmentBiomeIndex(int x, int y) const {
  auto block = getBlockInfo(m_geometry.xwrap(x), y);
  if (!block) return std::unexpected(std::move(block.error()));
  return block->environmentBiomeIndex;
}

std::expected<std::shared_ptr<Biome const>, StarException> WorldTemplate::biome(BiomeIndex biomeIndex) const {
  if (!m_layout || biomeIndex == NullBiomeIndex) return {};
  auto biome = m_layout->getBiome(biomeIndex);
  if (!biome) return std::unexpected(std::move(biome.error()));
  return biome->get();
}

std::expected<std::shared_ptr<Biome const>, StarException> WorldTemplate::blockBiome(int x, int y) const {
  auto index = blockBiomeIndex(m_geometry.xwrap(x), y);
  if (!index) return std::unexpected(std::move(index.error()));
  return biome(*index);
}

std::expected<std::shared_ptr<Biome const>, StarException> WorldTemplate::environmentBiome(int x, int y) const {
  auto index = environmentBiomeIndex(m_geometry.xwrap(x), y);
  if (!index) return std::unexpected(std::move(index.error()));
  return biome(*index);
}

std::expected<MaterialHue, StarException> WorldTemplate::biomeMaterialHueShift(BiomeIndex biomeIndex, MaterialId material) const {
  auto result = biome(biomeIndex);
  if (!result) return std::unexpected(std::move(result.error()));
  if (*result && material == (*result)->main_block)
    return (*result)->material_hue_shift;
  return MaterialHue();
}

std::expected<MaterialHue, StarException> WorldTemplate::biomeModHueShift(BiomeIndex biomeIndex, ModId mod) const {
  auto result = biome(biomeIndex);
  if (!result) return std::unexpected(std::move(result.error()));
  if (auto const& biome = *result) {
    if (mod == biome->surface_placeables.grass_mod || mod == biome->surface_placeables.ceiling_grass_mod
        || mod == biome->underground_placeables.grass_mod || mod == biome->underground_placeables.ceiling_grass_mod)
      return biome->material_hue_shift;
  }
  return MaterialHue();
}

WeatherPool WorldTemplate::weathers() const {
  if (m_worldParameters)
    return m_worldParameters->weatherPool;
  return {};
}

List<WorldTemplate::WeatherDomain> const& WorldTemplate::weatherDomains() const {
  return m_weatherDomains;
}

std::expected<std::optional<std::reference_wrapper<WorldTemplate::WeatherDomain const>>, StarException> WorldTemplate::weatherDomain(String const& name) const {
  for (auto const& domain : m_weatherDomains) {
    if (domain.name == name)
      return std::cref(domain);
  }
  return std::nullopt;
}

std::expected<std::optional<std::reference_wrapper<WorldTemplate::WeatherLayer const>>, StarException> WorldTemplate::weatherLayerAt(Vec2I const& position) const {
  for (auto const& layer : m_weatherLayers) {
    if (position[1] >= layer.minHeight && position[1] < layer.maxHeight)
      return std::cref(layer);
  }
  return std::nullopt;
}

std::expected<std::shared_ptr<AmbientNoisesDescription const>, StarException> WorldTemplate::ambient_noises(int x, int y) const {
  if (m_ambient_override)
    return m_ambient_override;
  auto biome = environmentBiome(x, y);
  if (!biome) return std::unexpected(std::move(biome.error()));
  if (*biome) return (*biome)->ambient_noises;
  return {};
}

std::expected<std::shared_ptr<AmbientNoisesDescription const>, StarException> WorldTemplate::music_track(int x, int y) const {
  if (m_music_override)
    return m_music_override;
  auto biome = environmentBiome(x, y);
  if (!biome) return std::unexpected(std::move(biome.error()));
  if (*biome) return (*biome)->music_track;
  return {};
}

StringList WorldTemplate::environmentStatusEffects(int, int) const {
  if (m_worldParameters)
    return m_worldParameters->environmentStatusEffects;
  return {};
}

bool WorldTemplate::breathable(int, int) const {
  if (m_worldParameters)
    return !m_worldParameters->airless;
  return true;
}

std::expected<void, StarException> WorldTemplate::add_potential_biome_items(int x, int y, PotentialBiomeItems& items, List<BiomeItemDistribution> const& distributions, BiomePlacementArea area, std::optional<BiomePlacementMode> mode) const {
  auto destination = [&](BiomePlacementMode placement_mode) -> std::optional<std::reference_wrapper<List<BiomeItemPlacement>>> {
    if (area == BiomePlacementArea::Surface) {
      if (placement_mode == BiomePlacementMode::Floor) return items.surface_biome_items;
      if (placement_mode == BiomePlacementMode::Ocean) return items.ocean_items;
    } else if (area == BiomePlacementArea::Underground) {
      if (placement_mode == BiomePlacementMode::Floor) return items.cave_surface_biome_items;
      if (placement_mode == BiomePlacementMode::Ceiling) return items.cave_ceiling_biome_items;
      if (placement_mode == BiomePlacementMode::Background) return items.cave_background_biome_items;
    }
    return std::nullopt;
  };

  for (auto const& distribution : distributions) {
    auto placement_mode = distribution.mode();
    if (mode && placement_mode != *mode) continue;
    auto target = destination(placement_mode);
    if (!target) continue;
    auto placement = distribution.item_to_place(x, y);
    if (!placement) return std::unexpected(std::move(placement.error()));
    if (*placement) target->get().push_back(std::move(**placement));
  }
  return {};
}


std::expected<WorldTemplate::PotentialBiomeItems, StarException> WorldTemplate::potential_biome_items_at(int x, int y) const {
  if (!m_layout || y <= 0 || y >= static_cast<int>(m_geometry.height()) - 1)
    return {};
  x = m_geometry.xwrap(x);

  auto block_biome = [this, x](int sample_y) -> std::expected<std::optional<std::reference_wrapper<Biome const>>, StarException> {
    auto index = blockBiomeIndex(x, sample_y);
    if (!index) return std::unexpected(std::move(index.error()));
    if (*index == NullBiomeIndex) return std::nullopt;
    auto biome = m_layout->getBiome(*index);
    if (!biome) return std::unexpected(std::move(biome.error()));
    return optional_ref(biome->get());
  };

  auto lower_block_biome = block_biome(y - 1);
  if (!lower_block_biome) return std::unexpected(std::move(lower_block_biome.error()));
  auto upper_block_biome = block_biome(y + 1);
  if (!upper_block_biome) return std::unexpected(std::move(upper_block_biome.error()));
  auto this_block_biome = block_biome(y);
  if (!this_block_biome) return std::unexpected(std::move(this_block_biome.error()));

  PotentialBiomeItems potential_biome_items;
  // surface floor, surface ocean
  if (*lower_block_biome) {
    auto added = add_potential_biome_items(x, y, potential_biome_items, (*lower_block_biome)->get().surface_placeables.item_distributions, BiomePlacementArea::Surface, BiomePlacementMode::Floor);
    if (!added) return std::unexpected(std::move(added.error()));
  }
  if (*this_block_biome) {
    auto added = add_potential_biome_items(x, y, potential_biome_items, (*this_block_biome)->get().surface_placeables.item_distributions, BiomePlacementArea::Surface, BiomePlacementMode::Ocean);
    if (!added) return std::unexpected(std::move(added.error()));
  }

  // underground floor, ceiling, background
  if (*lower_block_biome) {
    auto added = add_potential_biome_items(x, y, potential_biome_items, (*lower_block_biome)->get().underground_placeables.item_distributions, BiomePlacementArea::Underground, BiomePlacementMode::Floor);
    if (!added) return std::unexpected(std::move(added.error()));
  }
  if (*upper_block_biome) {
    auto added = add_potential_biome_items(x, y, potential_biome_items, (*upper_block_biome)->get().underground_placeables.item_distributions, BiomePlacementArea::Underground, BiomePlacementMode::Ceiling);
    if (!added) return std::unexpected(std::move(added.error()));
  }
  if (*this_block_biome) {
    auto added = add_potential_biome_items(x, y, potential_biome_items, (*this_block_biome)->get().underground_placeables.item_distributions, BiomePlacementArea::Underground, BiomePlacementMode::Background);
    if (!added) return std::unexpected(std::move(added.error()));
  }

  return potential_biome_items;
}

std::expected<List<BiomeItemPlacement>, StarException> WorldTemplate::valid_biome_items(int x, int y, PotentialBiomeItems potential_biome_items) const {
  if (y <= 0 || y >= static_cast<int>(m_geometry.height()) - 1)
    return {};

  x = m_geometry.xwrap(x);

  auto block = getBlockInfo(x, y);
  if (!block) return std::unexpected(std::move(block.error()));

  if (block->biomeTransition)
    return {};
  List<BiomeItemPlacement> biome_items;

  auto block_above = getBlockInfo(x, y + 1);
  if (!block_above) return std::unexpected(std::move(block_above.error()));
  auto block_below = getBlockInfo(x, y - 1);
  if (!block_below) return std::unexpected(std::move(block_below.error()));

  if (!block_below->biomeTransition && block_below->terrain && !block->terrain && !block_below->foregroundCave)
    biome_items.append_range(std::move(potential_biome_items.surface_biome_items));

  if (!block_below->biomeTransition && block_below->terrain && block->terrain && !block_below->foregroundCave && block->foregroundCave)
    biome_items.append_range(std::move(potential_biome_items.cave_surface_biome_items));

  if (!block_above->biomeTransition && block_above->terrain && block->terrain && !block_above->foregroundCave && block->foregroundCave)
    biome_items.append_range(std::move(potential_biome_items.cave_ceiling_biome_items));

  if (block->terrain && block->foregroundCave && !block->backgroundCave)
    biome_items.append_range(std::move(potential_biome_items.cave_background_biome_items));

  if (block->oceanLiquid != EmptyLiquidId && y == block->oceanLiquidLevel)
    biome_items.append_range(std::move(potential_biome_items.ocean_items));

  return biome_items;
}

std::expected<float, StarException> WorldTemplate::gravity() const {
  if (m_worldParameters)
    return m_worldParameters->gravity;
  return m_templateConfig.get_float("defaultGravity");
}

float WorldTemplate::threatLevel() const {
  if (m_worldParameters)
    return m_worldParameters->threatLevel;
  return 0.0f;
}

std::uint64_t WorldTemplate::seedFor(int x, int y) const {
  return static_random_u64(m_seed, m_geometry.xwrap(x), y, "Block");
}

WorldTemplate::WorldTemplate(Root& root) : m_root(root) {}

std::expected<void, StarException> WorldTemplate::initialize() {
  auto assets = this->root().assets();
  if (!assets) return std::unexpected(std::move(assets.error()));
  auto config = (*assets)->json("/world_template.config");
  if (!config) return std::unexpected(std::move(config.error()));
  m_templateConfig = std::move(*config);
  auto size = m_templateConfig.get_float("customTerrainBlendSize");
  if (!size) return std::unexpected(std::move(size.error()));
  m_customTerrainBlendSize = *size;
  auto weight = m_templateConfig.get_float("customTerrainBlendWeight");
  if (!weight) return std::unexpected(std::move(weight.error()));
  m_customTerrainBlendWeight = *weight;
  auto cacheSize = m_templateConfig.get_int("blockCacheSize");
  if (!cacheSize) return std::unexpected(std::move(cacheSize.error()));
  m_blockCache.set_capacity(*cacheSize);
  m_geometry = Vec2U(2048, 2048);
  m_seed = this->root().random().next_u64();
  return {};
}

std::expected<void, StarException> WorldTemplate::determineWorldName() {
  if (m_celestialParameters) {
    m_worldName = m_celestialParameters->name();
  } else if (auto parameters = as<FloatingDungeonWorldParameters>(m_worldParameters)) {
    auto definitions = this->root().dungeonDefinitions();
    if (!definitions) return std::unexpected(std::move(definitions.error()));
    auto dungeon = (*definitions)->get(parameters->primaryDungeon);
    if (!dungeon) return std::unexpected(std::move(dungeon.error()));
    m_worldName = (*dungeon)->displayName();
  } else {
    m_worldName = "";
  }
  return {};
}

std::expected<void, StarException> WorldTemplate::setupWeatherDomains() {
  m_weatherDomains.clear();
  m_weatherLayers.clear();

  if (!m_worldParameters)
    return {};

  if (auto terrestrial = as<TerrestrialWorldParameters>(m_worldParameters)) {
    struct LayerDefinition {
      String name;
      std::reference_wrapper<TerrestrialWorldParameters::TerrestrialLayer const> layer;
      bool inheritsSurface;
    };

    List<LayerDefinition> definitions;
    definitions.push_back({.name = "core", .layer = std::cref(terrestrial->coreLayer), .inheritsSurface = false});
    for (std::size_t i = terrestrial->undergroundLayers.size(); i != 0; --i)
      definitions.push_back({.name = Star::format("underground{}", i),
                             .layer = std::cref(terrestrial->undergroundLayers[i - 1]),
                             .inheritsSurface = false});
    definitions.push_back(
      {.name = "subsurface", .layer = std::cref(terrestrial->subsurfaceLayer), .inheritsSurface = true});
    definitions.push_back({.name = "surface", .layer = std::cref(terrestrial->surfaceLayer), .inheritsSurface = false});
    definitions.push_back(
      {.name = "atmosphere", .layer = std::cref(terrestrial->atmosphereLayer), .inheritsSurface = true});
    definitions.push_back({.name = "space", .layer = std::cref(terrestrial->spaceLayer), .inheritsSurface = false});

    m_weatherDomains.push_back({.name = "surface",
                                .pool = terrestrial->weatherPool,
                                .effectsMinHeight = terrestrial->surfaceLayer.layerMinHeight});

    auto biome_database = this->root().biome_database();
    if (!biome_database) return std::unexpected(std::move(biome_database.error()));
    for (std::size_t i = 0; i < definitions.size(); ++i) {
      auto const& definition = definitions[i];
      int minHeight = definition.layer.get().layerMinHeight;
      int maxHeight = i + 1 < definitions.size() ? definitions[i + 1].layer.get().layerMinHeight
                                                 : static_cast<int>(m_geometry.height());

      std::optional<String> domain;
      if (definition.name == "surface") {
        domain = String("surface");
      } else {
        auto has_weather = (*biome_database)->biome_has_weather(definition.layer.get().primaryRegion.biome);
        if (!has_weather) return std::unexpected(std::move(has_weather.error()));
        if (*has_weather) {
          domain = definition.name;
          auto pool = (*biome_database)->biome_weathers(definition.layer.get().primaryRegion.biome, m_seed, threatLevel());
          if (!pool) return std::unexpected(std::move(pool.error()));
          m_weatherDomains.push_back(
            {.name = definition.name, .pool = std::move(*pool), .effectsMinHeight = minHeight});
        } else if (definition.inheritsSurface) {
          domain = String("surface");
          m_weatherDomains[0].effectsMinHeight = std::min(m_weatherDomains[0].effectsMinHeight, minHeight);
        }
      }

      m_weatherLayers.push_back(
        {.name = definition.name, .domain = std::exchange(domain, {}), .minHeight = minHeight, .maxHeight = maxHeight});
    }
  } else {
    m_weatherDomains.push_back({.name = "surface",
                                .pool = m_worldParameters->weatherPool,
                                .effectsMinHeight = static_cast<int>(undergroundLevel())});
    m_weatherLayers.push_back({.name = "surface",
                               .domain = std::optional<String>(String("surface")),
                               .minHeight = 0,
                               .maxHeight = static_cast<int>(m_geometry.height())});
  }
  return {};
}

std::expected<std::pair<float, float>, StarException> WorldTemplate::customTerrainWeighting(int x, int y) const {
  float minimumDistance = std::numeric_limits<float>::max();
  float finalSolidWeight = 0.0f;
  float totalWeight = 0.0f;

  for (auto const& region : m_customTerrainRegions) {
    auto contains = m_geometry.rect_contains(region.regionBounds.padded(m_customTerrainBlendSize), Vec2F(x, y));
    if (!contains) return std::unexpected(std::move(contains.error()));
    if (!*contains)
      continue;

    float distance = m_geometry.poly_distance(region.region, Vec2F(x, y));
    if (distance >= m_customTerrainBlendSize)
      continue;

    float weight = 1.0f - distance / m_customTerrainBlendSize;
    totalWeight += weight;
    if (!region.solid)
      weight *= -1.0f;

    finalSolidWeight += weight;
    minimumDistance = std::min(minimumDistance, distance);
  }

  if (minimumDistance > m_customTerrainBlendSize)
    return std::pair(0.0f, 0.0f);

  finalSolidWeight /= totalWeight;

  return std::pair(finalSolidWeight * m_customTerrainBlendWeight, 1.0f - minimumDistance / m_customTerrainBlendSize);
}

std::expected<WorldTemplate::BlockInfo, StarException> WorldTemplate::getBlockInfo(std::uint32_t x, std::uint32_t y) const {
  auto cached = m_blockCache.get(Vector<std::uint32_t, 2>(x, y), [&worldTemplate = *this, x, y](Vector<std::uint32_t, 2>) -> std::expected<BlockInfo, StarException> {
      BlockInfo blockInfo;

      if (!worldTemplate.m_layout)
        return blockInfo;

      // The environment biome is calculated with weighting based on the flat coordinates.
      auto flatWeightingResult = worldTemplate.m_layout->getWeighting(x, y);
      if (!flatWeightingResult) return std::unexpected(std::move(flatWeightingResult.error()));
      auto const& flatWeighting = *flatWeightingResult;

      // The block biome is calculated optionally with higher frequency noise
      // added to prevent straight lines appearing on the boundaries of
      // regions.

      int blendNoiseOffset = 0;
      if (auto const& blendNoise = worldTemplate.m_layout->blendNoise()) {
        auto noise = blendNoise->get(x, y);
        if (!noise) return std::unexpected(std::move(noise.error()));
        blendNoiseOffset = static_cast<int>(*noise);
      }

      Vec2I blockPos;
      List<WorldLayout::RegionWeighting> blockWeighting;
      List<WorldLayout::RegionWeighting> transitionWeighting;
      if (auto const& blockNoise = worldTemplate.m_layout->blockNoise()) {
        auto position = blockNoise->apply(Vec2I(x, y), worldTemplate.m_geometry.size());
        if (!position) return std::unexpected(std::move(position.error()));
        blockPos = *position;
        auto blockWeightingResult = worldTemplate.m_layout->getWeighting(blockPos[0] + blendNoiseOffset, blockPos[1]);
        if (!blockWeightingResult) return std::unexpected(std::move(blockWeightingResult.error()));
        blockWeighting = std::move(*blockWeightingResult);
        auto transitionWeightingResult = worldTemplate.m_layout->getWeighting(blockPos[0], blockPos[1]);
        if (!transitionWeightingResult) return std::unexpected(std::move(transitionWeightingResult.error()));
        transitionWeighting = std::move(*transitionWeightingResult);
      } else {
        blockPos = Vec2I(x, y);
        blockWeighting = flatWeighting;
        transitionWeighting = flatWeighting;
      }

      if (flatWeighting.empty() || blockWeighting.empty())
        return blockInfo;

      auto const& primaryFlatWeighting = flatWeighting[0];
      auto const& primaryBlockWeighting = blockWeighting[0];

      blockInfo.blockBiomeIndex = primaryBlockWeighting.region.get().blockBiomeIndex;
      blockInfo.environmentBiomeIndex = primaryFlatWeighting.region.get().environmentBiomeIndex;

      auto transition = transitionWeighting.first();
      if (!transition) return std::unexpected(std::move(transition.error()));
      auto threshold = worldTemplate.m_templateConfig.get_float("biomeTransitionThreshold", 0);
      if (!threshold) return std::unexpected(std::move(threshold.error()));
      blockInfo.biomeTransition = transition->get().weight < *threshold;

      float terrainSelect = 0.0f;
      float foregroundCaveSelect = 0.0f;
      float backgroundCaveSelect = 0.0f;

      // Terrain weighting uses the flat weighting, and weights each selector
      // to blend among them.
      for (auto const& weighting : flatWeighting) {
        if (weighting.region.get().terrainSelectorIndex != NullTerrainSelectorIndex) {
          auto terrainSelectorResult = worldTemplate.m_layout->getTerrainSelector(weighting.region.get().terrainSelectorIndex);
          if (!terrainSelectorResult) return std::unexpected(std::move(terrainSelectorResult.error()));
          auto const& terrainSelector = terrainSelectorResult->get();
          auto terrainSelectorValue = terrainSelector->get(weighting.xValue, y);
          if (!terrainSelectorValue) return std::unexpected(std::move(terrainSelectorValue.error()));
          float select = *terrainSelectorValue * weighting.weight;
          terrainSelect += select;
        }
      }

      // This is a bit of a cheat. Since customTerrainWeighting is always flat,
      // there are some odd effects that come from linearly interpolating from
      // the generally non-flat terrain sources to flat regions of space.  By
      // using an interpolator that has an exaggerated S curve between the
      // points, this hides some of these effects.
      auto ctweighting = worldTemplate.customTerrainWeighting(x, y);
      if (!ctweighting) return std::unexpected(std::move(ctweighting.error()));
      terrainSelect = quintic2(ctweighting->second, terrainSelect, ctweighting->first);

      if (terrainSelect > 0.0f) {
        blockInfo.terrain = true;

        for (auto const& weighting : flatWeighting) {
          if (weighting.region.get().foregroundCaveSelectorIndex != NullTerrainSelectorIndex) {
            auto foregroundCaveSelectorResult = worldTemplate.m_layout->getTerrainSelector(weighting.region.get().foregroundCaveSelectorIndex);
            if (!foregroundCaveSelectorResult) return std::unexpected(std::move(foregroundCaveSelectorResult.error()));
            auto const& foregroundCaveSelector = foregroundCaveSelectorResult->get();
            auto foregroundCaveSelectorValue = foregroundCaveSelector->get(weighting.xValue, y);
            if (!foregroundCaveSelectorValue) return std::unexpected(std::move(foregroundCaveSelectorValue.error()));
            foregroundCaveSelect += *foregroundCaveSelectorValue * weighting.weight;
          }

          if (weighting.region.get().backgroundCaveSelectorIndex != NullTerrainSelectorIndex) {
            auto backgroundCaveSelectorResult = worldTemplate.m_layout->getTerrainSelector(weighting.region.get().backgroundCaveSelectorIndex);
            if (!backgroundCaveSelectorResult) return std::unexpected(std::move(backgroundCaveSelectorResult.error()));
            auto const& backgroundCaveSelector = backgroundCaveSelectorResult->get();
            auto backgroundCaveSelectorValue = backgroundCaveSelector->get(weighting.xValue, y);
            if (!backgroundCaveSelectorValue) return std::unexpected(std::move(backgroundCaveSelectorValue.error()));
            backgroundCaveSelect += *backgroundCaveSelectorValue * weighting.weight;
          }
        }

        auto surfaceCaveAttenuationDistResult = worldTemplate.m_templateConfig.get_float("surfaceCaveAttenuationDist", 0);
        if (!surfaceCaveAttenuationDistResult) return std::unexpected(std::move(surfaceCaveAttenuationDistResult.error()));
        auto surfaceCaveAttenuationDist = *surfaceCaveAttenuationDistResult;
        if (terrainSelect < surfaceCaveAttenuationDist) {
          auto surfaceCaveAttenuationFactorResult = worldTemplate.m_templateConfig.get_float("surfaceCaveAttenuationFactor", 1);
          if (!surfaceCaveAttenuationFactorResult) return std::unexpected(std::move(surfaceCaveAttenuationFactorResult.error()));
          auto surfaceCaveAttenuationFactor = *surfaceCaveAttenuationFactorResult;
          foregroundCaveSelect -= (surfaceCaveAttenuationDist - terrainSelect) * surfaceCaveAttenuationFactor;
          backgroundCaveSelect -= (surfaceCaveAttenuationDist - terrainSelect) * surfaceCaveAttenuationFactor;
        }
      }

      blockInfo.foregroundCave = foregroundCaveSelect > 0.0f;
      blockInfo.backgroundCave = backgroundCaveSelect > 0.0f;

      auto const& regionLiquids = primaryFlatWeighting.region.get().regionLiquids;
      blockInfo.caveLiquid = regionLiquids.caveLiquid;
      blockInfo.caveLiquidSeedDensity = regionLiquids.caveLiquidSeedDensity;
      blockInfo.oceanLiquid = regionLiquids.oceanLiquid;
      blockInfo.oceanLiquidLevel = regionLiquids.oceanLiquidLevel;
      blockInfo.encloseLiquids = regionLiquids.encloseLiquids;
      blockInfo.fillMicrodungeons = regionLiquids.fillMicrodungeons;

      if (!blockInfo.terrain && blockInfo.encloseLiquids && static_cast<int>(y) < blockInfo.oceanLiquidLevel) {
        blockInfo.terrain = true;
        blockInfo.foregroundCave = true;
      }

      if (blockInfo.terrain) {
        auto biome_result = worldTemplate.biome(blockInfo.blockBiomeIndex);
        if (!biome_result) return std::unexpected(std::move(biome_result.error()));
        if (auto const& block_biome = *biome_result) {
          if (!blockInfo.foregroundCave) {
            blockInfo.foreground = block_biome->main_block;
            blockInfo.background = blockInfo.foreground;
          } else if (!blockInfo.backgroundCave) {
            blockInfo.background = block_biome->main_block;
          }

          // subBlock, foregroundOre, and backgroundOre selectors can be empty
          // if they are not enabled, otherwise they will always have the
          // correct count

          if (!primaryBlockWeighting.region.get().subBlockSelectorIndexes.empty()) {
            for (std::size_t i = 0; i < block_biome->sub_blocks.size(); ++i) {
              auto index = primaryBlockWeighting.region.get().subBlockSelectorIndexes.at(i);
              if (!index) return std::unexpected(std::move(index.error()));
              auto selectorResult = worldTemplate.m_layout->getTerrainSelector(index->get());
              if (!selectorResult) return std::unexpected(std::move(selectorResult.error()));
              auto const& selector = selectorResult->get();
              auto select = selector->get(primaryBlockWeighting.xValue - blendNoiseOffset, blockPos[1]);
              if (!select) return std::unexpected(std::move(select.error()));
              if (*select > 0.0f) {
                if (!blockInfo.foregroundCave) {
                  blockInfo.foreground = block_biome->sub_blocks[i];
                  blockInfo.background = blockInfo.foreground;
                } else if (!blockInfo.backgroundCave) {
                  blockInfo.background = block_biome->sub_blocks[i];
                }

                break;
              }
            }
          }

          if (!blockInfo.foregroundCave && !primaryBlockWeighting.region.get().foregroundOreSelectorIndexes.empty()) {
            for (std::size_t i = 0; i < block_biome->ores.size(); ++i) {
              auto index = primaryBlockWeighting.region.get().foregroundOreSelectorIndexes.at(i);
              if (!index) return std::unexpected(std::move(index.error()));
              auto selectorResult = worldTemplate.m_layout->getTerrainSelector(index->get());
              if (!selectorResult) return std::unexpected(std::move(selectorResult.error()));
              auto const& selector = selectorResult->get();
              auto select = selector->get(x, y);
              if (!select) return std::unexpected(std::move(select.error()));
              if (*select > 0.0f) {
                blockInfo.foregroundMod = block_biome->ores[i].first;
                break;
              }
            }
          }

          if (!blockInfo.backgroundCave && !primaryBlockWeighting.region.get().backgroundOreSelectorIndexes.empty()) {
            for (std::size_t i = 0; i < block_biome->ores.size(); ++i) {
              auto index = primaryBlockWeighting.region.get().backgroundOreSelectorIndexes.at(i);
              if (!index) return std::unexpected(std::move(index.error()));
              auto selectorResult = worldTemplate.m_layout->getTerrainSelector(index->get());
              if (!selectorResult) return std::unexpected(std::move(selectorResult.error()));
              auto const& selector = selectorResult->get();
              auto select = selector->get(x, y);
              if (!select) return std::unexpected(std::move(select.error()));
              if (*select > 0.0f) {
                blockInfo.backgroundMod = block_biome->ores[i].first;
                break;
              }
            }
          }
        }
      }

      return blockInfo;
    });
  if (!cached) return std::unexpected(std::move(cached.error()));
  return cached->get();
}

Json WorldTemplate::BlockInfo::toJson() const {
  return JsonObject({
    {"blockBiomeIndex", blockBiomeIndex},
    {"environmentBiomeIndex", environmentBiomeIndex},
    {"biomeTransition", biomeTransition},
    {"terrain", terrain},
    {"foregroundCave", foregroundCave},
    {"backgroundCave", backgroundCave},
    {"foreground", foreground},
    {"foregroundMod", foregroundMod},
    {"background", background},
    {"backgroundMod", backgroundMod},
    {"caveLiquid", caveLiquid},
    {"caveLiquidSeedDensity", caveLiquidSeedDensity},
    {"oceanLiquid", oceanLiquid},
    {"oceanLiquidLevel", oceanLiquidLevel},
    {"encloseLiquids", encloseLiquids},
    {"fillMicrodungeons", fillMicrodungeons}
  });
}

}

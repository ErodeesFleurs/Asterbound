import star.memory;
import star.json_extra;
import star.json;
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
import star.liquid_types;
import star.bi_map;
import star.random;
import star.perlin;
import star.weighted_pool;
import star.id_map;
import star.set;
import star.casting;
import star.strong_typedef;
import star.ordered_map;
import star.io_device;
import star.ref_ptr;
import star.color;
import star.vector;
import star.logging;
import star.listener;
import std;
import star.string;
import star.format;
import star.version;

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
import star.inspectable_entity;
import star.plant;
import star.plant_database;
import star.biome_placement;
import star.spawn_type_database;
import star.biome;
import star.world_geometry;
import star.asset_source;
import star.assets;
import star.tile_damage;
import star.parallax;

import star.configuration;
import star.root;

import star.terrain_database;
import star.biome_database;


import star.static_random;
import star.math_common;
import star.algorithm;
namespace Star {

WorldRegion::WorldRegion()
  : terrainSelectorIndex(NullTerrainSelectorIndex),
    foregroundCaveSelectorIndex(NullTerrainSelectorIndex),
    backgroundCaveSelectorIndex(NullTerrainSelectorIndex),
    blockBiomeIndex(NullBiomeIndex),
    environmentBiomeIndex(NullBiomeIndex) {}

std::expected<WorldRegion, StarException> WorldRegion::create(Json const& store) {
  WorldRegion result;
  auto resultterrainSelectorIndexResult1 = store.getUInt("terrainSelectorIndex");
  if (!resultterrainSelectorIndexResult1) return std::unexpected(std::move(resultterrainSelectorIndexResult1.error()));
  result.terrainSelectorIndex = std::move((*resultterrainSelectorIndexResult1));
  auto resultforegroundCaveSelectorIndexResult1 = store.getUInt("foregroundCaveSelectorIndex");
  if (!resultforegroundCaveSelectorIndexResult1) return std::unexpected(std::move(resultforegroundCaveSelectorIndexResult1.error()));
  result.foregroundCaveSelectorIndex = std::move((*resultforegroundCaveSelectorIndexResult1));
  auto resultbackgroundCaveSelectorIndexResult1 = store.getUInt("backgroundCaveSelectorIndex");
  if (!resultbackgroundCaveSelectorIndexResult1) return std::unexpected(std::move(resultbackgroundCaveSelectorIndexResult1.error()));
  result.backgroundCaveSelectorIndex = std::move((*resultbackgroundCaveSelectorIndexResult1));
  auto resultblockBiomeIndexResult1 = store.getUInt("blockBiomeIndex");
  if (!resultblockBiomeIndexResult1) return std::unexpected(std::move(resultblockBiomeIndexResult1.error()));
  result.blockBiomeIndex = std::move((*resultblockBiomeIndexResult1));
  auto resultenvironmentBiomeIndexResult1 = store.getUInt("environmentBiomeIndex");
  if (!resultenvironmentBiomeIndexResult1) return std::unexpected(std::move(resultenvironmentBiomeIndexResult1.error()));
  result.environmentBiomeIndex = std::move((*resultenvironmentBiomeIndexResult1));
  auto resultregionLiquidscaveLiquidResult1 = store.getUInt("caveLiquid");
  if (!resultregionLiquidscaveLiquidResult1) return std::unexpected(std::move(resultregionLiquidscaveLiquidResult1.error()));
  result.regionLiquids.caveLiquid = std::move((*resultregionLiquidscaveLiquidResult1));
  auto resultregionLiquidscaveLiquidSeedDensityResult1 = store.getFloat("caveLiquidSeedDensity");
  if (!resultregionLiquidscaveLiquidSeedDensityResult1) return std::unexpected(std::move(resultregionLiquidscaveLiquidSeedDensityResult1.error()));
  result.regionLiquids.caveLiquidSeedDensity = std::move((*resultregionLiquidscaveLiquidSeedDensityResult1));
  auto resultregionLiquidsoceanLiquidResult1 = store.getUInt("oceanLiquid");
  if (!resultregionLiquidsoceanLiquidResult1) return std::unexpected(std::move(resultregionLiquidsoceanLiquidResult1.error()));
  result.regionLiquids.oceanLiquid = std::move((*resultregionLiquidsoceanLiquidResult1));
  auto resultregionLiquidsoceanLiquidLevelResult1 = store.getInt("oceanLiquidLevel");
  if (!resultregionLiquidsoceanLiquidLevelResult1) return std::unexpected(std::move(resultregionLiquidsoceanLiquidLevelResult1.error()));
  result.regionLiquids.oceanLiquidLevel = std::move((*resultregionLiquidsoceanLiquidLevelResult1));
  auto resultregionLiquidsencloseLiquidsResult1 = store.getBool("encloseLiquids");
  if (!resultregionLiquidsencloseLiquidsResult1) return std::unexpected(std::move(resultregionLiquidsencloseLiquidsResult1.error()));
  result.regionLiquids.encloseLiquids = std::move((*resultregionLiquidsencloseLiquidsResult1));
  auto resultregionLiquidsfillMicrodungeonsResult1 = store.getBool("fillMicrodungeons");
  if (!resultregionLiquidsfillMicrodungeonsResult1) return std::unexpected(std::move(resultregionLiquidsfillMicrodungeonsResult1.error()));
  result.regionLiquids.fillMicrodungeons = std::move((*resultregionLiquidsfillMicrodungeonsResult1));
  auto autosubBlockSelectorIndexesResult1 = store.getArray("subBlockSelectorIndexes");
  if (!autosubBlockSelectorIndexesResult1) return std::unexpected(std::move(autosubBlockSelectorIndexesResult1.error()));
  auto subBlockSelectorIndexes = std::move((*autosubBlockSelectorIndexesResult1));
  for (auto const& entry : subBlockSelectorIndexes) {
    auto index = entry.toUInt();
    if (!index) return std::unexpected(std::move(index.error()));
    result.subBlockSelectorIndexes.append(*index);
  }
  auto autoforegroundOreSelectorIndexesResult1 = store.getArray("foregroundOreSelectorIndexes");
  if (!autoforegroundOreSelectorIndexesResult1) return std::unexpected(std::move(autoforegroundOreSelectorIndexesResult1.error()));
  auto foregroundOreSelectorIndexes = std::move((*autoforegroundOreSelectorIndexesResult1));
  for (auto const& entry : foregroundOreSelectorIndexes) {
    auto index = entry.toUInt();
    if (!index) return std::unexpected(std::move(index.error()));
    result.foregroundOreSelectorIndexes.append(*index);
  }
  auto autobackgroundOreSelectorIndexesResult1 = store.getArray("backgroundOreSelectorIndexes");
  if (!autobackgroundOreSelectorIndexesResult1) return std::unexpected(std::move(autobackgroundOreSelectorIndexesResult1.error()));
  auto backgroundOreSelectorIndexes = std::move((*autobackgroundOreSelectorIndexesResult1));
  for (auto const& entry : backgroundOreSelectorIndexes) {
    auto index = entry.toUInt();
    if (!index) return std::unexpected(std::move(index.error()));
    result.backgroundOreSelectorIndexes.append(*index);
  }
  return result;
}

Json WorldRegion::toJson() const {
  return JsonObject{
    {"terrainSelectorIndex", terrainSelectorIndex},
    {"foregroundCaveSelectorIndex", foregroundCaveSelectorIndex},
    {"backgroundCaveSelectorIndex", backgroundCaveSelectorIndex},

    {"blockBiomeIndex", blockBiomeIndex},
    {"environmentBiomeIndex", environmentBiomeIndex},

    {"caveLiquid", regionLiquids.caveLiquid},
    {"caveLiquidSeedDensity", regionLiquids.caveLiquidSeedDensity},

    {"oceanLiquid", regionLiquids.oceanLiquid},
    {"oceanLiquidLevel", regionLiquids.oceanLiquidLevel},

    {"encloseLiquids", regionLiquids.encloseLiquids},
    {"fillMicrodungeons", regionLiquids.fillMicrodungeons},

    {"subBlockSelectorIndexes", subBlockSelectorIndexes.transformed(construct<Json>())},
    {"foregroundOreSelectorIndexes", foregroundOreSelectorIndexes.transformed(construct<Json>())},
    {"backgroundOreSelectorIndexes", backgroundOreSelectorIndexes.transformed(construct<Json>())}
  };
}

std::expected<WorldLayout::BlockNoise, StarException> WorldLayout::BlockNoise::build(Json const& config, std::uint64_t seed) {
  BlockNoise blockNoise;
  auto horizontal_noise = config.get("horizontalNoise");
  if (!horizontal_noise)
    return std::unexpected(std::move(horizontal_noise.error()));
  auto blockNoisehorizontalNoisePerlin =
    PerlinF::create((*horizontal_noise), static_random_u64(seed, "HorizontalNoise"));
  if (!blockNoisehorizontalNoisePerlin) return std::unexpected(std::move(blockNoisehorizontalNoisePerlin.error()));
  blockNoise.horizontalNoise = std::move(*blockNoisehorizontalNoisePerlin);
  auto vertical_noise = config.get("verticalNoise");
  if (!vertical_noise)
    return std::unexpected(std::move(vertical_noise.error()));
  auto blockNoiseverticalNoisePerlin = PerlinF::create((*vertical_noise), static_random_u64(seed, "VerticalNoise"));
  if (!blockNoiseverticalNoisePerlin) return std::unexpected(std::move(blockNoiseverticalNoisePerlin.error()));
  blockNoise.verticalNoise = std::move(*blockNoiseverticalNoisePerlin);
  auto x_noise = config.get("noise");
  if (!x_noise)
    return std::unexpected(std::move(x_noise.error()));
  auto blockNoisexNoisePerlin = PerlinF::create((*x_noise), static_random_u64(seed, "XNoise"));
  if (!blockNoisexNoisePerlin) return std::unexpected(std::move(blockNoisexNoisePerlin.error()));
  blockNoise.xNoise = std::move(*blockNoisexNoisePerlin);
  auto y_noise = config.get("noise");
  if (!y_noise)
    return std::unexpected(std::move(y_noise.error()));
  auto blockNoiseyNoisePerlin = PerlinF::create((*y_noise), static_random_u64(seed, "yNoise"));
  if (!blockNoiseyNoisePerlin) return std::unexpected(std::move(blockNoiseyNoisePerlin.error()));
  blockNoise.yNoise = std::move(*blockNoiseyNoisePerlin);
  return blockNoise;
}

WorldLayout::BlockNoise::BlockNoise() = default;

std::expected<WorldLayout::BlockNoise, StarException> WorldLayout::BlockNoise::create(Json const& store) {
  BlockNoise result;
  auto resulthorizontalNoiseResult1 = store.get("horizontalNoise");
  if (!resulthorizontalNoiseResult1) return std::unexpected(std::move(resulthorizontalNoiseResult1.error()));
  auto resulthorizontalNoisePerlin = PerlinF::create((*resulthorizontalNoiseResult1));
  if (!resulthorizontalNoisePerlin) return std::unexpected(std::move(resulthorizontalNoisePerlin.error()));
  result.horizontalNoise = std::move(*resulthorizontalNoisePerlin);
  auto resultverticalNoiseResult1 = store.get("verticalNoise");
  if (!resultverticalNoiseResult1) return std::unexpected(std::move(resultverticalNoiseResult1.error()));
  auto resultverticalNoisePerlin = PerlinF::create((*resultverticalNoiseResult1));
  if (!resultverticalNoisePerlin) return std::unexpected(std::move(resultverticalNoisePerlin.error()));
  result.verticalNoise = std::move(*resultverticalNoisePerlin);
  auto resultxNoiseResult1 = store.get("xNoise");
  if (!resultxNoiseResult1) return std::unexpected(std::move(resultxNoiseResult1.error()));
  auto resultxNoisePerlin = PerlinF::create((*resultxNoiseResult1));
  if (!resultxNoisePerlin) return std::unexpected(std::move(resultxNoisePerlin.error()));
  result.xNoise = std::move(*resultxNoisePerlin);
  auto resultyNoiseResult1 = store.get("yNoise");
  if (!resultyNoiseResult1) return std::unexpected(std::move(resultyNoiseResult1.error()));
  auto resultyNoisePerlin = PerlinF::create((*resultyNoiseResult1));
  if (!resultyNoisePerlin) return std::unexpected(std::move(resultyNoisePerlin.error()));
  result.yNoise = std::move(*resultyNoisePerlin);
  return result;
}

std::expected<Json, StarException> WorldLayout::BlockNoise::toJson() const {
  auto horizontalNoiseJson = horizontalNoise.toJson();
  if (!horizontalNoiseJson) return std::unexpected(std::move(horizontalNoiseJson.error()));
  auto verticalNoiseJson = verticalNoise.toJson();
  if (!verticalNoiseJson) return std::unexpected(std::move(verticalNoiseJson.error()));
  auto xNoiseJson = xNoise.toJson();
  if (!xNoiseJson) return std::unexpected(std::move(xNoiseJson.error()));
  auto yNoiseJson = yNoise.toJson();
  if (!yNoiseJson) return std::unexpected(std::move(yNoiseJson.error()));
  return JsonObject{{"horizontalNoise", std::move(*horizontalNoiseJson)}, {"verticalNoise", std::move(*verticalNoiseJson)}, {"xNoise", std::move(*xNoiseJson)}, {"yNoise", std::move(*yNoiseJson)}};
}

std::expected<Vec2I, StarException> WorldLayout::BlockNoise::apply(Vec2I const& input, Vec2U const& worldSize) const {
  float angle = (input[0] / static_cast<float>(worldSize[0])) * 2 * std::numbers::pi;
  float xc = std::sin(angle) / (2 * std::numbers::pi) * worldSize[0];
  float zc = std::cos(angle) / (2 * std::numbers::pi) * worldSize[0];

  auto horizontal = horizontalNoise.get(input[1]);
  if (!horizontal) return std::unexpected(std::move(horizontal.error()));
  auto x = xNoise.get(xc, input[1], zc);
  if (!x) return std::unexpected(std::move(x.error()));
  auto vertical = verticalNoise.get(xc, zc);
  if (!vertical) return std::unexpected(std::move(vertical.error()));
  auto y = yNoise.get(xc, input[1], zc);
  if (!y) return std::unexpected(std::move(y.error()));
  Vec2I noisePos = Vec2I(
      std::floor(input[0] + *horizontal + *x),
      std::floor(input[1] + *vertical + *y)
    );
  noisePos[1] = clamp<int>(noisePos[1], 0, worldSize[1]);

  return noisePos;
}

std::expected<WorldLayout, StarException> WorldLayout::buildTerrestrialLayout(Root& root, TerrestrialWorldParameters const& terrestrialParameters, std::uint64_t seed) {
  
  auto autoassetsResult1 = root.assets();
  if (!autoassetsResult1) return std::unexpected(std::move(autoassetsResult1.error()));
  auto assets = std::move((*autoassetsResult1));
  auto autoterrainDatabaseResult1 = root.terrainDatabase();
  if (!autoterrainDatabaseResult1) return std::unexpected(std::move(autoterrainDatabaseResult1.error()));
  auto terrainDatabase = std::move((*autoterrainDatabaseResult1));
  auto autobiomeDatabaseResult1 = root.biomeDatabase();
  if (!autobiomeDatabaseResult1) return std::unexpected(std::move(autobiomeDatabaseResult1.error()));
  auto biomeDatabase = std::move((*autobiomeDatabaseResult1));

  RandomSource randSource(seed);

  WorldLayout layout(root);
  layout.m_worldSize = terrestrialParameters.worldSize;

  auto addLayer =
    [&](TerrestrialWorldParameters::TerrestrialLayer const& terrestrialLayer) -> std::expected<void, StarException> {
    RegionParams primaryRegionParams = {
      .baseHeight = terrestrialLayer.layerBaseHeight,
      .threatLevel = terrestrialParameters.threatLevel,
      .biomeName = terrestrialLayer.primaryRegion.biome,
      .terrainSelector = terrestrialLayer.primaryRegion.blockSelector,
      .fgCaveSelector = terrestrialLayer.primaryRegion.fgCaveSelector,
      .bgCaveSelector = terrestrialLayer.primaryRegion.bgCaveSelector,
      .fgOreSelector = terrestrialLayer.primaryRegion.fgOreSelector,
      .bgOreSelector = terrestrialLayer.primaryRegion.bgOreSelector,
      .subBlockSelector = terrestrialLayer.primaryRegion.subBlockSelector,
      .regionLiquids = {.caveLiquid = terrestrialLayer.primaryRegion.caveLiquid,
                        .caveLiquidSeedDensity = terrestrialLayer.primaryRegion.caveLiquidSeedDensity,
                        .oceanLiquid = terrestrialLayer.primaryRegion.oceanLiquid,
                        .oceanLiquidLevel = terrestrialLayer.primaryRegion.oceanLiquidLevel,
                        .encloseLiquids = terrestrialLayer.primaryRegion.encloseLiquids,
                        .fillMicrodungeons = terrestrialLayer.primaryRegion.fillMicrodungeons}};

    RegionParams primarySubRegionParams = {
      .baseHeight = terrestrialLayer.layerBaseHeight,
      .threatLevel = terrestrialParameters.threatLevel,
      .biomeName = terrestrialLayer.primarySubRegion.biome,
      .terrainSelector = terrestrialLayer.primarySubRegion.blockSelector,
      .fgCaveSelector = terrestrialLayer.primarySubRegion.fgCaveSelector,
      .bgCaveSelector = terrestrialLayer.primarySubRegion.bgCaveSelector,
      .fgOreSelector = terrestrialLayer.primarySubRegion.fgOreSelector,
      .bgOreSelector = terrestrialLayer.primarySubRegion.bgOreSelector,
      .subBlockSelector = terrestrialLayer.primarySubRegion.subBlockSelector,
      .regionLiquids = {.caveLiquid = terrestrialLayer.primarySubRegion.caveLiquid,
                        .caveLiquidSeedDensity = terrestrialLayer.primarySubRegion.caveLiquidSeedDensity,
                        .oceanLiquid = terrestrialLayer.primarySubRegion.oceanLiquid,
                        .oceanLiquidLevel = terrestrialLayer.primarySubRegion.oceanLiquidLevel,
                        .encloseLiquids = terrestrialLayer.primarySubRegion.encloseLiquids,
                        .fillMicrodungeons = terrestrialLayer.primarySubRegion.fillMicrodungeons}};

    List<RegionParams> secondaryRegions;
    for (auto const& secondaryRegion : terrestrialLayer.secondaryRegions) {
      RegionParams secondaryRegionParams = {
        .baseHeight = terrestrialLayer.layerBaseHeight,
        .threatLevel = terrestrialParameters.threatLevel,
        .biomeName = secondaryRegion.biome,
        .terrainSelector = secondaryRegion.blockSelector,
        .fgCaveSelector = secondaryRegion.fgCaveSelector,
        .bgCaveSelector = secondaryRegion.bgCaveSelector,
        .fgOreSelector = secondaryRegion.fgOreSelector,
        .bgOreSelector = secondaryRegion.bgOreSelector,
        .subBlockSelector = secondaryRegion.subBlockSelector,
        .regionLiquids = {.caveLiquid = secondaryRegion.caveLiquid,
                          .caveLiquidSeedDensity = secondaryRegion.caveLiquidSeedDensity,
                          .oceanLiquid = secondaryRegion.oceanLiquid,
                          .oceanLiquidLevel = secondaryRegion.oceanLiquidLevel,
                          .encloseLiquids = secondaryRegion.encloseLiquids,
                          .fillMicrodungeons = secondaryRegion.fillMicrodungeons}};

      secondaryRegions.append(secondaryRegionParams);
    }

    List<RegionParams> secondarySubRegions;
    for (auto const& secondarySubRegion : terrestrialLayer.secondarySubRegions) {
      RegionParams secondarySubRegionParams = {
        .baseHeight = terrestrialLayer.layerBaseHeight,
        .threatLevel = terrestrialParameters.threatLevel,
        .biomeName = secondarySubRegion.biome,
        .terrainSelector = secondarySubRegion.blockSelector,
        .fgCaveSelector = secondarySubRegion.fgCaveSelector,
        .bgCaveSelector = secondarySubRegion.bgCaveSelector,
        .fgOreSelector = secondarySubRegion.fgOreSelector,
        .bgOreSelector = secondarySubRegion.bgOreSelector,
        .subBlockSelector = secondarySubRegion.subBlockSelector,
        .regionLiquids = {.caveLiquid = secondarySubRegion.caveLiquid,
                          .caveLiquidSeedDensity = secondarySubRegion.caveLiquidSeedDensity,
                          .oceanLiquid = secondarySubRegion.oceanLiquid,
                          .oceanLiquidLevel = secondarySubRegion.oceanLiquidLevel,
                          .encloseLiquids = secondarySubRegion.encloseLiquids,
                          .fillMicrodungeons = secondarySubRegion.fillMicrodungeons}};

      secondarySubRegions.append(secondarySubRegionParams);
    }

    return layout.addLayer(seed,
        terrestrialLayer.layerMinHeight,
        terrestrialLayer.layerBaseHeight,
        terrestrialParameters.primaryBiome,
        primaryRegionParams,
        primarySubRegionParams,
        secondaryRegions,
        secondarySubRegions,
        terrestrialLayer.secondaryRegionSizeRange,
        terrestrialLayer.subRegionSizeRange);
  };

  {

    auto added = addLayer(terrestrialParameters.coreLayer);

    if (!added) return std::unexpected(std::move(added.error()));

  }
  for (auto const& undergroundLayer : std::views::reverse(terrestrialParameters.undergroundLayers))
    {
      auto added = addLayer(undergroundLayer);
      if (!added) return std::unexpected(std::move(added.error()));
    }

  {

    auto added = addLayer(terrestrialParameters.subsurfaceLayer);

    if (!added) return std::unexpected(std::move(added.error()));

  }
  {
    auto added = addLayer(terrestrialParameters.surfaceLayer);
    if (!added) return std::unexpected(std::move(added.error()));
  }
  {
    auto added = addLayer(terrestrialParameters.atmosphereLayer);
    if (!added) return std::unexpected(std::move(added.error()));
  }
  {
    auto added = addLayer(terrestrialParameters.spaceLayer);
    if (!added) return std::unexpected(std::move(added.error()));
  }

  layout.m_regionBlending = terrestrialParameters.blendSize;
  if (terrestrialParameters.blockNoiseConfig) {
    auto layoutm_blockNoiseResult1 = BlockNoise::build(terrestrialParameters.blockNoiseConfig, seed);
    if (!layoutm_blockNoiseResult1) return std::unexpected(std::move(layoutm_blockNoiseResult1.error()));
    layout.m_blockNoise = std::move((*layoutm_blockNoiseResult1));
  }
  if (terrestrialParameters.blendNoiseConfig) {
    auto blend_noise = PerlinF::create(terrestrialParameters.blendNoiseConfig, static_random_u64(seed, "BlendNoise"));
    if (!blend_noise)
      return std::unexpected(std::move(blend_noise.error()));
    layout.m_blendNoise = std::move((*blend_noise));
  }

  auto finalized = layout.finalize(terrestrialParameters.skyColoring.mainColor);
  if (!finalized) return std::unexpected(std::move(finalized.error()));

  return layout;
}

std::expected<WorldLayout, StarException> WorldLayout::buildAsteroidsLayout(Root& root, AsteroidsWorldParameters const& asteroidParameters, std::uint64_t seed) {
  auto autoassetsResult1 = root.assets();
  if (!autoassetsResult1) return std::unexpected(std::move(autoassetsResult1.error()));
  auto assets = std::move((*autoassetsResult1));

  RandomSource randSource(seed);

  auto autoasteroidsConfigResult1 = assets->json("/asteroids_worlds.config");
  if (!autoasteroidsConfigResult1) return std::unexpected(std::move(autoasteroidsConfigResult1.error()));
  auto asteroidsConfig = std::move((*autoasteroidsConfigResult1));
  auto autoasteroidTerrainsResult1 = asteroidsConfig.get("terrains");
  if (!autoasteroidTerrainsResult1) return std::unexpected(std::move(autoasteroidTerrainsResult1.error()));
  auto autoasteroidTerrainsResult2 = (*autoasteroidTerrainsResult1).toArray();
  if (!autoasteroidTerrainsResult2) return std::unexpected(std::move(autoasteroidTerrainsResult2.error()));
  auto asteroidTerrains = std::move((*autoasteroidTerrainsResult2));
  auto selectedTerrain = randSource.randFrom(asteroidTerrains);
  if (!selectedTerrain) return std::unexpected(std::move(selectedTerrain.error()));
  auto const& asteroidTerrainConfig = selectedTerrain->get();
  auto autoemptyTerrainConfigResult1 = asteroidsConfig.get("emptyTerrain");
  if (!autoemptyTerrainConfigResult1) return std::unexpected(std::move(autoemptyTerrainConfigResult1.error()));
  auto emptyTerrainConfig = std::move((*autoemptyTerrainConfigResult1));

  WorldLayout layout(root);
  layout.m_worldSize = asteroidParameters.worldSize;

  auto autoasteroidRegionField3Result1 = asteroidTerrainConfig.getString("terrainSelector");
  if (!autoasteroidRegionField3Result1) return std::unexpected(std::move(autoasteroidRegionField3Result1.error()));
  auto asteroidRegionField3 = std::move((*autoasteroidRegionField3Result1));
  auto autoasteroidRegionField4Result1 = asteroidTerrainConfig.getString("caveSelector");
  if (!autoasteroidRegionField4Result1) return std::unexpected(std::move(autoasteroidRegionField4Result1.error()));
  auto asteroidRegionField4 = std::move((*autoasteroidRegionField4Result1));
  auto autoasteroidRegionField5Result1 = asteroidTerrainConfig.getString("bgCaveSelector");
  if (!autoasteroidRegionField5Result1) return std::unexpected(std::move(autoasteroidRegionField5Result1.error()));
  auto asteroidRegionField5 = std::move((*autoasteroidRegionField5Result1));
  auto autoasteroidRegionField6Result1 = asteroidTerrainConfig.getString("oreSelector");
  if (!autoasteroidRegionField6Result1) return std::unexpected(std::move(autoasteroidRegionField6Result1.error()));
  auto asteroidRegionField6 = std::move((*autoasteroidRegionField6Result1));
  auto autoasteroidRegionField7Result1 = asteroidTerrainConfig.getString("oreSelector");
  if (!autoasteroidRegionField7Result1) return std::unexpected(std::move(autoasteroidRegionField7Result1.error()));
  auto asteroidRegionField7 = std::move((*autoasteroidRegionField7Result1));
  auto autoasteroidRegionField8Result1 = asteroidTerrainConfig.getString("subBlockSelector");
  if (!autoasteroidRegionField8Result1) return std::unexpected(std::move(autoasteroidRegionField8Result1.error()));
  auto asteroidRegionField8 = std::move((*autoasteroidRegionField8Result1));
  RegionParams asteroidRegion{.baseHeight = static_cast<int>(asteroidParameters.worldSize[1]) / 2,
                              .threatLevel = asteroidParameters.threatLevel,
                              .biomeName = asteroidParameters.asteroidBiome,
                              .terrainSelector = asteroidRegionField3,
                              .fgCaveSelector = asteroidRegionField4,
                              .bgCaveSelector = asteroidRegionField5,
                              .fgOreSelector = asteroidRegionField6,
                              .bgOreSelector = asteroidRegionField7,
                              .subBlockSelector = asteroidRegionField8,
                              .regionLiquids = {.caveLiquid = EmptyLiquidId,
                                                .caveLiquidSeedDensity = 0.0f,
                                                .oceanLiquid = EmptyLiquidId,
                                                .oceanLiquidLevel = 0,
                                                .encloseLiquids = false,
                                                .fillMicrodungeons = false}};

  auto autoemptyRegionField3Result1 = emptyTerrainConfig.getString("terrainSelector");
  if (!autoemptyRegionField3Result1) return std::unexpected(std::move(autoemptyRegionField3Result1.error()));
  auto emptyRegionField3 = std::move((*autoemptyRegionField3Result1));
  auto autoemptyRegionField4Result1 = emptyTerrainConfig.getString("caveSelector");
  if (!autoemptyRegionField4Result1) return std::unexpected(std::move(autoemptyRegionField4Result1.error()));
  auto emptyRegionField4 = std::move((*autoemptyRegionField4Result1));
  auto autoemptyRegionField5Result1 = emptyTerrainConfig.getString("bgCaveSelector");
  if (!autoemptyRegionField5Result1) return std::unexpected(std::move(autoemptyRegionField5Result1.error()));
  auto emptyRegionField5 = std::move((*autoemptyRegionField5Result1));
  auto autoemptyRegionField6Result1 = emptyTerrainConfig.getString("oreSelector");
  if (!autoemptyRegionField6Result1) return std::unexpected(std::move(autoemptyRegionField6Result1.error()));
  auto emptyRegionField6 = std::move((*autoemptyRegionField6Result1));
  auto autoemptyRegionField7Result1 = emptyTerrainConfig.getString("oreSelector");
  if (!autoemptyRegionField7Result1) return std::unexpected(std::move(autoemptyRegionField7Result1.error()));
  auto emptyRegionField7 = std::move((*autoemptyRegionField7Result1));
  auto autoemptyRegionField8Result1 = emptyTerrainConfig.getString("subBlockSelector");
  if (!autoemptyRegionField8Result1) return std::unexpected(std::move(autoemptyRegionField8Result1.error()));
  auto emptyRegionField8 = std::move((*autoemptyRegionField8Result1));
  RegionParams emptyRegion{.baseHeight = static_cast<int>(asteroidParameters.worldSize[1]) / 2,
                           .threatLevel = asteroidParameters.threatLevel,
                           .biomeName = asteroidParameters.asteroidBiome,
                           .terrainSelector = emptyRegionField3,
                           .fgCaveSelector = emptyRegionField4,
                           .bgCaveSelector = emptyRegionField5,
                           .fgOreSelector = emptyRegionField6,
                           .bgOreSelector = emptyRegionField7,
                           .subBlockSelector = emptyRegionField8,
                           .regionLiquids = {.caveLiquid = EmptyLiquidId,
                                             .caveLiquidSeedDensity = 0.0f,
                                             .oceanLiquid = EmptyLiquidId,
                                             .oceanLiquidLevel = 0,
                                             .encloseLiquids = false,
                                             .fillMicrodungeons = false}};

  {
    auto added = layout.addLayer(seed, 0, emptyRegion);
    if (!added) return std::unexpected(std::move(added.error()));
  }
  {
    auto added = layout.addLayer(seed, asteroidParameters.asteroidBottomLevel, asteroidRegion);
    if (!added) return std::unexpected(std::move(added.error()));
  }
  {
    auto added = layout.addLayer(seed, asteroidParameters.asteroidTopLevel, emptyRegion);
    if (!added) return std::unexpected(std::move(added.error()));
  }

  layout.m_regionBlending = asteroidParameters.blendSize;
  auto noiseJson = asteroidsConfig.opt("blockNoise");
  if (!noiseJson) return std::unexpected(std::move(noiseJson.error()));
  if (*noiseJson) {
    auto layoutm_blockNoiseResult1 = BlockNoise::build(**noiseJson, seed);
    if (!layoutm_blockNoiseResult1) return std::unexpected(std::move(layoutm_blockNoiseResult1.error()));
    layout.m_blockNoise = std::move((*layoutm_blockNoiseResult1));
  }

  layout.m_playerStartSearchRegions.append(RectI(0, asteroidParameters.asteroidBottomLevel, asteroidParameters.worldSize[0], asteroidParameters.asteroidTopLevel));

  auto finalized = layout.finalize(Color::Black);
  if (!finalized) return std::unexpected(std::move(finalized.error()));

  return layout;
}

std::expected<WorldLayout, StarException> WorldLayout::buildFloatingDungeonLayout(Root& root, FloatingDungeonWorldParameters const& floatingDungeonParameters, std::uint64_t seed) {
  auto autoassetsResult1 = root.assets();
  if (!autoassetsResult1) return std::unexpected(std::move(autoassetsResult1.error()));
  auto assets = std::move((*autoassetsResult1));
  auto autobiomeDatabaseResult1 = root.biomeDatabase();
  if (!autobiomeDatabaseResult1) return std::unexpected(std::move(autobiomeDatabaseResult1.error()));
  auto biomeDatabase = std::move((*autobiomeDatabaseResult1));

  RandomSource randSource(seed);

  WorldLayout layout(root);
  layout.m_worldSize = floatingDungeonParameters.worldSize;

  RegionParams biomeRegion{.baseHeight = floatingDungeonParameters.dungeonSurfaceHeight,
                           .threatLevel = floatingDungeonParameters.threatLevel,
                           .biomeName = floatingDungeonParameters.biome,
                           .terrainSelector = {},
                           .fgCaveSelector = {},
                           .bgCaveSelector = {},
                           .fgOreSelector = {},
                           .bgOreSelector = {},
                           .subBlockSelector = {},
                           .regionLiquids = {.caveLiquid = EmptyLiquidId,
                                             .caveLiquidSeedDensity = 0.0f,
                                             .oceanLiquid = EmptyLiquidId,
                                             .oceanLiquidLevel = 0,
                                             .encloseLiquids = false,
                                             .fillMicrodungeons = false}};

  {
    auto added = layout.addLayer(seed, 0, biomeRegion);
    if (!added) return std::unexpected(std::move(added.error()));
  }
  if (floatingDungeonParameters.biome) {
    auto sky = biomeDatabase->biomeSkyColoring(*floatingDungeonParameters.biome, seed);
    if (!sky) return std::unexpected(std::move(sky.error()));
  } else {
    auto finalized = layout.finalize(Color::Black);
    if (!finalized) return std::unexpected(std::move(finalized.error()));
  }

  return layout;
}

WorldLayout::WorldLayout(Root& root) : m_root(root), m_regionBlending(0.0f) {}

std::expected<std::shared_ptr<WorldLayout>, StarException> WorldLayout::create(Root& root, Json const& store) {
  auto result = std::make_shared<WorldLayout>(root);
  auto autoterrainDatabaseResult1 = root.terrainDatabase();
  if (!autoterrainDatabaseResult1) return std::unexpected(std::move(autoterrainDatabaseResult1.error()));
  auto terrainDatabase = std::move((*autoterrainDatabaseResult1));
  auto resultm_worldSizeResult1 = store.get("worldSize");
  if (!resultm_worldSizeResult1) return std::unexpected(std::move(resultm_worldSizeResult1.error()));
  auto resultm_worldSizeResult2 = jsonToVec2U((*resultm_worldSizeResult1));
  if (!resultm_worldSizeResult2) return std::unexpected(std::move(resultm_worldSizeResult2.error()));
  result->m_worldSize = std::move((*resultm_worldSizeResult2));
  auto autobiomesResult1 = store.getArray("biomes");
  if (!autobiomesResult1) return std::unexpected(std::move(autobiomesResult1.error()));
  auto biomes = std::move((*autobiomesResult1));
  for (auto const& value : biomes) {
    auto parsed = Biome::create(root, value);
    if (!parsed) return std::unexpected(std::move(parsed.error()));
    result->m_biomes.append(std::make_shared<Biome>(std::move(*parsed)));
  }
  auto autoterrainSelectorsResult1 = store.getArray("terrainSelectors");
  if (!autoterrainSelectorsResult1) return std::unexpected(std::move(autoterrainSelectorsResult1.error()));
  auto terrainSelectors = std::move((*autoterrainSelectorsResult1));
  for (auto const& value : terrainSelectors) {
    auto parsed = terrainDatabase->loadSelector(value);
    if (!parsed) return std::unexpected(std::move(parsed.error()));
    result->m_terrainSelectors.append(std::move(*parsed));
  }
  auto autolayersResult1 = store.getArray("layers");
  if (!autolayersResult1) return std::unexpected(std::move(autolayersResult1.error()));
  auto layers = std::move((*autolayersResult1));
  for (auto const& value : layers) {
    WorldLayer layer;
    auto layeryStartResult1 = value.getInt("yStart");
    if (!layeryStartResult1) return std::unexpected(std::move(layeryStartResult1.error()));
    layer.yStart = std::move((*layeryStartResult1));
    auto autoboundariesResult1 = value.getArray("boundaries");
    if (!autoboundariesResult1) return std::unexpected(std::move(autoboundariesResult1.error()));
    auto boundaries = std::move((*autoboundariesResult1));
    for (auto const& boundary : boundaries) {
      auto autopositionResult1 = boundary.toInt();
      if (!autopositionResult1) return std::unexpected(std::move(autopositionResult1.error()));
      auto position = std::move((*autopositionResult1));
      layer.boundaries.append(position);
    }
    auto autocellsResult1 = value.getArray("cells");
    if (!autocellsResult1) return std::unexpected(std::move(autocellsResult1.error()));
    auto cells = std::move((*autocellsResult1));
    for (auto const& cell : cells) {
      auto region = WorldRegion::create(cell);
      if (!region) return std::unexpected(std::move(region.error()));
      layer.cells.append(std::make_shared<WorldRegion>(std::move(*region)));
    }
    result->m_layers.append(std::move(layer));
  }
  auto resultm_regionBlendingResult1 = store.getFloat("regionBlending");
  if (!resultm_regionBlendingResult1) return std::unexpected(std::move(resultm_regionBlendingResult1.error()));
  result->m_regionBlending = std::move((*resultm_regionBlendingResult1));
  auto resultm_blockNoiseJson = store.opt("blockNoise");
  if (!resultm_blockNoiseJson) return std::unexpected(std::move(resultm_blockNoiseJson.error()));
  if (*resultm_blockNoiseJson) {
    auto value = BlockNoise::create(**resultm_blockNoiseJson);
    if (!value) return std::unexpected(std::move(value.error()));
    result->m_blockNoise = std::move(*value);
  }
  auto resultm_blendNoiseJson = store.opt("blendNoise");
  if (!resultm_blendNoiseJson) return std::unexpected(std::move(resultm_blendNoiseJson.error()));
  if (*resultm_blendNoiseJson) {
    auto value = PerlinF::create(**resultm_blendNoiseJson);
    if (!value) return std::unexpected(std::move(value.error()));
    result->m_blendNoise = std::move(*value);
  }
  auto autosearchRegionsResult1 = store.getArray("playerStartSearchRegions");
  if (!autosearchRegionsResult1) return std::unexpected(std::move(autosearchRegionsResult1.error()));
  auto searchRegions = std::move((*autosearchRegionsResult1));
  for (auto const& region : searchRegions) {
    auto autorectResult1 = jsonToRectI(region);
    if (!autorectResult1) return std::unexpected(std::move(autorectResult1.error()));
    auto rect = std::move((*autorectResult1));
    result->m_playerStartSearchRegions.append(rect);
  }
  return result;
}

std::expected<Json, StarException> WorldLayout::toJson() const {
  auto autoterrainDatabaseResult1 = this->root().terrainDatabase();
  if (!autoterrainDatabaseResult1) return std::unexpected(std::move(autoterrainDatabaseResult1.error()));
  auto terrainDatabase = std::move((*autoterrainDatabaseResult1));

  JsonArray biomes;
  for (auto const& biome : m_biomes) {
    auto stored = biome->toJson();
    if (!stored) return std::unexpected(std::move(stored.error()));
    biomes.append(std::move(*stored));
  }
  Json blockNoise;
  if (m_blockNoise) {
    auto stored = m_blockNoise->toJson();
    if (!stored) return std::unexpected(std::move(stored.error()));
    blockNoise = std::move(*stored);
  }
  Json blendNoise;
  if (m_blendNoise) {
    auto stored = m_blendNoise->toJson();
    if (!stored) return std::unexpected(std::move(stored.error()));
    blendNoise = std::move(*stored);
  }
  return JsonObject{
    {"worldSize", jsonFromVec2U(m_worldSize)},
    {"biomes", std::move(biomes)},
    {"terrainSelectors", transform<JsonArray>(m_terrainSelectors, [terrainDatabase](auto const& selector) {
        return terrainDatabase->storeSelector(selector);
      })},
    {"layers", m_layers.transformed([](WorldLayer const& layer) -> Json {
        JsonArray boundaries;
        boundaries.reserve(layer.boundaries.size());
        std::ranges::transform(layer.boundaries, std::back_inserter(boundaries), construct<Json>());
        JsonArray cells;
        cells.reserve(layer.cells.size());
        std::ranges::transform(layer.cells, std::back_inserter(cells), std::mem_fn(&WorldRegion::toJson));
        return JsonObject{
          {"yStart", layer.yStart},
          {"boundaries", std::move(boundaries)},
          {"cells", std::move(cells)}
        };
      })},
    {"regionBlending", m_regionBlending},
    {"blockNoise", std::move(blockNoise)},
    {"blendNoise", std::move(blendNoise)},
    {"playerStartSearchRegions", m_playerStartSearchRegions.transformed(jsonFromRectI)}
  };
}

std::optional<WorldLayout::BlockNoise> const& WorldLayout::blockNoise() const {
  return m_blockNoise;
}

std::optional<PerlinF> const& WorldLayout::blendNoise() const {
  return m_blendNoise;
}

List<RectI> WorldLayout::playerStartSearchRegions() const {
  return m_playerStartSearchRegions;
}

std::expected<List<WorldLayout::RegionWeighting>, StarException> WorldLayout::getWeighting(int x, int y) const {
  List<RegionWeighting> weighting;
  WorldGeometry geometry(m_worldSize);

  auto cellWeighting = [&](WorldLayer const& layer, std::size_t cellIndex, int x) -> float {
    int xMin = 0;
    if (cellIndex > 0)
      xMin = layer.boundaries[cellIndex - 1];

    int xMax = m_worldSize[0];
    if (cellIndex < layer.boundaries.size())
      xMax = layer.boundaries[cellIndex];

    if (x > (xMin + xMax) / 2.0f)
      return std::clamp<float>(0.5f - (x - xMax) / m_regionBlending, 0.0f, 1.0f);
    else
      return std::clamp<float>(0.5f - (xMin - x) / m_regionBlending, 0.0f, 1.0f);
  };

  auto addLayerWeighting = [&](WorldLayer const& layer, int x, float weightFactor) {
    if (layer.cells.empty())
      return;

    std::size_t innerCellIndex;
    int innerCellXValue;
    std::tie(innerCellIndex, innerCellXValue) = findContainingCell(layer, x);
    float innerCellWeight = cellWeighting(layer, innerCellIndex, innerCellXValue);

    std::size_t leftCellIndex;
    int leftCellXValue;
    std::tie(leftCellIndex, leftCellXValue) = leftCell(layer, innerCellIndex, innerCellXValue);
    float leftCellWeight = cellWeighting(layer, leftCellIndex, leftCellXValue);

    std::size_t rightCellIndex;
    int rightCellXValue;
    std::tie(rightCellIndex, rightCellXValue) = rightCell(layer, innerCellIndex, innerCellXValue);
    float rightCellWeight = cellWeighting(layer, rightCellIndex, rightCellXValue);

    float totalWeight = innerCellWeight + leftCellWeight + rightCellWeight;
    if (totalWeight <= 0.0f)
      return;

    innerCellWeight *= weightFactor / totalWeight;
    leftCellWeight *= weightFactor / totalWeight;
    rightCellWeight *= weightFactor / totalWeight;

    if (innerCellWeight > 0.0f)
      weighting.append(RegionWeighting{.weight = innerCellWeight,
                                       .xValue = innerCellXValue,
                                       .region = std::cref(*layer.cells[innerCellIndex])});

    if (leftCellWeight > 0.0f)
      weighting.append(RegionWeighting{.weight = leftCellWeight,
                                       .xValue = leftCellXValue,
                                       .region = std::cref(*layer.cells[leftCellIndex])});

    if (rightCellWeight > 0.0f)
      weighting.append(RegionWeighting{.weight = rightCellWeight,
                                       .xValue = rightCellXValue,
                                       .region = std::cref(*layer.cells[rightCellIndex])});
  };

  auto yi = std::lower_bound(m_layers.begin(), m_layers.end(), y, [](WorldLayer const& layer, int y) {
      return layer.yStart < y;
    });

  if (yi == m_layers.end() || yi->yStart != y) {
    if (yi == m_layers.begin())
      return {};
    else
      --yi;
  }

  if (y - yi->yStart < (m_regionBlending / 2)) {
    if (yi == m_layers.begin()) {
      addLayerWeighting(*yi, x, 1.0f);
    } else {
      auto ypi = yi;
      --ypi;

      float yWeight = 0.5f + (y - yi->yStart) / m_regionBlending;
      addLayerWeighting(*yi, x, yWeight);
      addLayerWeighting(*ypi, x, 1.0f - yWeight);
    }
  } else {
    auto yni = yi;
    ++yni;
    if (yni == m_layers.end()) {
      addLayerWeighting(*yi, x, 1.0f);
    } else if (y <= yni->yStart - (m_regionBlending / 2)) {
      addLayerWeighting(*yi, x, 1.0f);
    } else {
      float yWeight = 0.5f - (yni->yStart - y) / m_regionBlending;
      addLayerWeighting(*yi, x, 1.0f - yWeight);
      addLayerWeighting(*yni, x, yWeight);
    }
  }

  // Need to return weighting in order of greatest to least
  std::sort(weighting.begin(), weighting.end(), [](RegionWeighting const& lhs, RegionWeighting const& rhs) {
      return lhs.weight > rhs.weight;
    });

  return weighting;
}

std::expected<List<RectI>, StarException> WorldLayout::previewAddBiomeRegion(Vec2I const& position, int width) const {
  auto layerAndCell = findLayerAndCell(position[0], position[1]);
  auto targetLayer = m_layers[layerAndCell.first];
  auto targetRegion = targetLayer.cells[layerAndCell.second];

  int insertX = position[0] > 0 ? position[0] : 1;

  // need a dummy region to expand
  std::shared_ptr<WorldRegion> dummyRegion;

  targetLayer.boundaries.insertAt(layerAndCell.second, insertX);
  targetLayer.cells.insertAt(layerAndCell.second, dummyRegion);

  targetLayer.boundaries.insertAt(layerAndCell.second, insertX - 1);
  targetLayer.cells.insertAt(layerAndCell.second, targetRegion);

  auto expandResult = expandRegionInLayer(targetLayer, layerAndCell.second + 1, width);

  if (!expandResult) return std::unexpected(std::move(expandResult.error()));

  return std::move(expandResult->second);
}

std::expected<List<RectI>, StarException> WorldLayout::previewExpandBiomeRegion(Vec2I const& position, int width) const {
  auto layerAndCell = findLayerAndCell(position[0], position[1]);
  auto targetLayer = m_layers[layerAndCell.first];

  auto expandResult = expandRegionInLayer(targetLayer, layerAndCell.second, width);

  if (!expandResult) return std::unexpected(std::move(expandResult.error()));

  return std::move(expandResult->second);
}

std::expected<String, StarException> WorldLayout::setLayerEnvironmentBiome(Vec2I const& position) {
  auto layerAndCell = findLayerAndCell(position[0], position[1]);
  auto targetLayer = m_layers[layerAndCell.first];
  auto targetBiomeIndex = targetLayer.cells[layerAndCell.second]->blockBiomeIndex;

  for (std::size_t i = 0; i < targetLayer.cells.size(); ++i)
    targetLayer.cells[i]->environmentBiomeIndex = targetBiomeIndex;

  m_layers[layerAndCell.first] = targetLayer;

  auto biome = getBiome(targetBiomeIndex);
  if (!biome) return std::unexpected(std::move(biome.error()));
  return biome->get()->baseName;
}

std::expected<void, StarException> WorldLayout::addBiomeRegion(
    TerrestrialWorldParameters const& terrestrialParameters,
    std::uint64_t seed,
    Vec2I const& position,
    String biomeName,
    String const& subBlockSelector,
    int width) {

  auto layerAndCell = findLayerAndCell(position[0], position[1]);

  // this->root().logger().info("inserting biome {} into region with layerIndex {} cellIndex {}", biomeName, layerAndCell.first, layerAndCell.second);

  auto targetLayer = m_layers[layerAndCell.first];

  // do this annoying dance to figure out which terrestrial layer we're in, so
  // we can extract the base height
  TerrestrialWorldParameters::TerrestrialLayer terrestrialLayer = terrestrialParameters.coreLayer;
  auto checkLayer = [targetLayer, &terrestrialLayer](TerrestrialWorldParameters::TerrestrialLayer const& layer) {
      if (layer.layerMinHeight == targetLayer.yStart)
        terrestrialLayer = layer;
    };
  for (auto const& undergroundLayer : terrestrialParameters.undergroundLayers)
    checkLayer(undergroundLayer);
  checkLayer(terrestrialParameters.subsurfaceLayer);
  checkLayer(terrestrialParameters.surfaceLayer);
  checkLayer(terrestrialParameters.atmosphereLayer);
  checkLayer(terrestrialParameters.spaceLayer);

  // build a new region using the biomeName and the parameters from the target region
  auto targetRegion = targetLayer.cells[layerAndCell.second];

  WorldRegion newRegion;
  newRegion.terrainSelectorIndex = targetRegion->terrainSelectorIndex;
  newRegion.foregroundCaveSelectorIndex = targetRegion->foregroundCaveSelectorIndex;
  newRegion.backgroundCaveSelectorIndex = targetRegion->backgroundCaveSelectorIndex;
  newRegion.foregroundOreSelectorIndexes = targetRegion->foregroundOreSelectorIndexes;
  newRegion.backgroundOreSelectorIndexes = targetRegion->backgroundOreSelectorIndexes;
  newRegion.regionLiquids = targetRegion->regionLiquids;
  auto autobiomeDatabaseResult1 = this->root().biomeDatabase();
  if (!autobiomeDatabaseResult1) return std::unexpected(std::move(autobiomeDatabaseResult1.error()));
  auto biomeDatabase = std::move((*autobiomeDatabaseResult1));
  auto generated_biome =
    biomeDatabase->createBiome(biomeName, static_random_u64(seed, "BiomeSeed"), terrestrialLayer.layerBaseHeight,
                               terrestrialParameters.threatLevel);
  if (!generated_biome)
    return std::unexpected(std::move(generated_biome.error()));
  auto newBiome = std::move((*generated_biome));

  auto oldBiomeResult = getBiome(targetRegion->blockBiomeIndex);
  if (!oldBiomeResult) return std::unexpected(std::move(oldBiomeResult.error()));
  auto const& oldBiome = oldBiomeResult->get();

  newBiome->ores = oldBiome->ores;

  // build new sub block selectors; this is the only region-level property that needs to be
  // newly constructed for the biome

  TerrainSelectorParameters baseSelectorParameters(this->root().random());
  baseSelectorParameters.worldWidth = m_worldSize[0];
  baseSelectorParameters.baseHeight = terrestrialLayer.layerBaseHeight;
  auto autoterrainDatabaseResult1 = this->root().terrainDatabase();
  if (!autoterrainDatabaseResult1) return std::unexpected(std::move(autoterrainDatabaseResult1.error()));
  auto terrainDatabase = std::move((*autoterrainDatabaseResult1));
  for (std::size_t i = 0; i < newBiome->subBlocks.size(); ++i) {
    auto selector_configuration = terrainDatabase->createNamedSelector(
      subBlockSelector, baseSelectorParameters.withSeed(static_random_u64(seed, i, "subBlocks")));
    if (!selector_configuration)
      return std::unexpected(std::move(selector_configuration.error()));
    auto selector = std::move((*selector_configuration));
    newRegion.subBlockSelectorIndexes.append(registerTerrainSelector(selector));
  }

  newRegion.environmentBiomeIndex = targetRegion->environmentBiomeIndex;
  newRegion.blockBiomeIndex = registerBiome(newBiome);

  std::shared_ptr<WorldRegion> newRegionPtr = std::make_shared<WorldRegion>(newRegion);

  // this->root().logger().info("boundaries before region insertion are {}", targetLayer.boundaries);

  // handle case where insert x position is exactly at world wrap
  int insertX = position[0] > 0 ? position[0] : 1;

  // insert the new region boundary
  targetLayer.boundaries.insertAt(layerAndCell.second, insertX);
  targetLayer.cells.insertAt(layerAndCell.second, newRegionPtr);

  // insert the left side of the (now split) target region
  targetLayer.boundaries.insertAt(layerAndCell.second, insertX - 1);
  targetLayer.cells.insertAt(layerAndCell.second, targetRegion);

  // this->root().logger().info("boundaries after region insertion are {}", targetLayer.boundaries);

  // expand the cell to the desired size
  auto expandResult = expandRegionInLayer(targetLayer, layerAndCell.second + 1, width);
  if (!expandResult) return std::unexpected(std::move(expandResult.error()));

  // update the layer in the template
  m_layers[layerAndCell.first] = std::move(expandResult->first);
  return {};
}

std::expected<void, StarException> WorldLayout::expandBiomeRegion(Vec2I const& position, int newWidth) {
  auto layerAndCell = findLayerAndCell(position[0], position[1]);

  auto targetLayer = m_layers[layerAndCell.first];

  auto expandResult = expandRegionInLayer(targetLayer, layerAndCell.second, newWidth);

  if (!expandResult) return std::unexpected(std::move(expandResult.error()));

  m_layers[layerAndCell.first] = std::move(expandResult->first);
  return {};
}

std::pair<std::size_t, std::size_t> WorldLayout::findLayerAndCell(int x, int y) const {
  // find the target layer
  std::size_t targetLayerIndex{};
  for (std::size_t i = 0; i < m_layers.size(); ++i) {
    if (m_layers[i].yStart < y)
      targetLayerIndex = i;
    else
      break;
  }

  auto targetLayer = m_layers[targetLayerIndex];

  auto targetCell = findContainingCell(targetLayer, x);

  return {targetLayerIndex, targetCell.first};
}

WorldLayout::WorldLayer::WorldLayer() : yStart(0) {}

std::expected<std::pair<WorldLayout::WorldLayer, List<RectI>>, StarException> WorldLayout::expandRegionInLayer(WorldLayout::WorldLayer targetLayer, std::size_t cellIndex, int newWidth) const {
  struct RegionCell {
    int lBound;
    int rBound;
    std::shared_ptr<WorldRegion> region;
  };

  // auto printRegionCells = [](List<RegionCell> const& cells) {
  //   String output = "";
  //   for (auto cell : cells)
  //     output += strf("[{} {}]  ", cell.lBound, cell.rBound);
  //   return output;
  // };

  // this->root().logger().info("expanding region in layer with cellIndex {} newWidth {}", cellIndex, newWidth);

  List<RectI> regionRects;

  if (targetLayer.cells.size() == 1) {
    auto logged = this->root().logger().info("Cannot expand region as it already fills the layer");
    if (!logged) return std::unexpected(std::move(logged.error()));
    return std::pair(std::move(targetLayer), std::move(regionRects));
  }

  // this->root().logger().info("boundaries before expansion are {}", targetLayer.boundaries);

  // TODO: this is a messy way to get the top of the layer, but maybe it's ok
  int layerTop = static_cast<int>(m_worldSize[1]);
  for (std::size_t i = 0; i < m_layers.size(); ++i) {
    if (m_layers[i].yStart == targetLayer.yStart && m_layers.size() > i + 1) {
      layerTop = m_layers[i + 1].yStart;
      break;
    }
  }

  // if the region is going to cover the full layer width, this is much simpler
  if (newWidth == static_cast<int>(m_worldSize[0])) {
    targetLayer.cells = {targetLayer.cells[cellIndex]};
    targetLayer.boundaries = {};

    regionRects.append(RectI{0, targetLayer.yStart, static_cast<int>(m_worldSize[0]), layerTop});
  } else {
    auto targetRegion = targetLayer.cells[cellIndex];

    // convert cells and boundaries into something more tractable
    List<RegionCell> targetCells;
    List<RegionCell> otherCells;

    int lastBoundary = 0;
    std::size_t lastCellIndex = targetLayer.cells.size() - 1;
    for (std::size_t i = 0; i <= lastCellIndex; ++i) {
      int nextBoundary = i == lastCellIndex ? static_cast<int>(m_worldSize[0]) : targetLayer.boundaries[i];
      if (i == cellIndex ||
          (i == 0 && cellIndex == lastCellIndex && targetLayer.cells[i] == targetRegion) ||
          (cellIndex == 0 && i == lastCellIndex && targetLayer.cells[i] == targetRegion))

        targetCells.append(RegionCell{.lBound = lastBoundary, .rBound = nextBoundary, .region = targetLayer.cells[i]});
      else
        otherCells.append(RegionCell{.lBound = lastBoundary, .rBound = nextBoundary, .region = targetLayer.cells[i]});
      lastBoundary = nextBoundary;
    }

    // this->root().logger().info("before expansion:\ntarget cells are: {}\nother cells are: {}", printRegionCells(targetCells), printRegionCells(otherCells));

    if constexpr (Star::DebugEnabled) {
      Star::starAssertFailed((targetCells.size() > 0));
    }
    if constexpr (Star::DebugEnabled) {
      Star::starAssertFailed((targetCells.size() < 3));
    }

    // check the current width to see how much (if any) to expand
    int currentWidth = 0;
    for (auto const& regionCell : targetCells)
      currentWidth += (regionCell.rBound - regionCell.lBound);

    if (currentWidth >= newWidth) {
      auto logged = this->root().logger().info("New cell width ({}) must be greater than current cell width {}!", newWidth, currentWidth);
      if (!logged) return std::unexpected(std::move(logged.error()));
      return std::pair(std::move(targetLayer), std::move(regionRects));
    }

    // expand the leftmost cell to the right and the rightmost cell to the left (they may be the same cell)
    int expandRight = std::ceil(0.5 * (newWidth - currentWidth));
    int expandLeft = std::floor(0.5 * (newWidth - currentWidth));

    // build the rects for the areas NEWLY covered by the region; these don't need to be wrapped because
    // they'll be split when they're consumed
    regionRects.append(RectI{targetCells[0].rBound, targetLayer.yStart, targetCells[0].rBound + expandRight, layerTop});
    regionRects.append(RectI{targetCells[targetCells.size() - 1].lBound - expandLeft, targetLayer.yStart, targetCells[targetCells.size() - 1].lBound, layerTop});

    targetCells[0].rBound += expandRight;
    targetCells[targetCells.size() - 1].lBound -= expandLeft;

    // this->root().logger().info("after expansion:\ntarget cells are: {}\nother cells are:  {}", printRegionCells(targetCells), printRegionCells(otherCells));

    // split any target cells that now cross the world wrap
    List<RegionCell> wrappedTargetCells;
    for (auto const& cell : targetCells) {
      if (cell.lBound < 0) {
        wrappedTargetCells.append(RegionCell{.lBound = 0, .rBound = cell.rBound, .region = cell.region});
        wrappedTargetCells.append(RegionCell{.lBound = static_cast<int>(m_worldSize[0]) + cell.lBound,
                                             .rBound = static_cast<int>(m_worldSize[0]),
                                             .region = cell.region});
      } else if (cell.rBound > static_cast<int>(m_worldSize[0])) {
        wrappedTargetCells.append(
          RegionCell{.lBound = cell.lBound, .rBound = static_cast<int>(m_worldSize[0]), .region = cell.region});
        wrappedTargetCells.append(
          RegionCell{.lBound = 0, .rBound = cell.rBound - static_cast<int>(m_worldSize[0]), .region = cell.region});
      } else {
        wrappedTargetCells.append(cell);
      }
    }

    targetCells = wrappedTargetCells;

    // modify/delete any overlapped cells
    for (auto const& targetCell : targetCells) {
      List<RegionCell> newOtherCells;
      for (auto const& otherCell : otherCells) {
        bool rInside = otherCell.rBound <= targetCell.rBound && otherCell.rBound >= targetCell.lBound;
        bool lInside = otherCell.lBound <= targetCell.rBound && otherCell.lBound >= targetCell.lBound;
        if (rInside && lInside)
          continue;
        else if (rInside)
          newOtherCells.append(
            RegionCell{.lBound = otherCell.lBound, .rBound = targetCell.lBound, .region = otherCell.region});
        else if (lInside)
          newOtherCells.append(
            RegionCell{.lBound = targetCell.rBound, .rBound = otherCell.rBound, .region = otherCell.region});
        else
          newOtherCells.append(otherCell);
      }
      otherCells = newOtherCells;
    }

    // this->root().logger().info("after de-overlapping:\ntarget cells are: {}\nother cells are:  {}", printRegionCells(targetCells), printRegionCells(otherCells));

    // combine lists and sort
    otherCells.appendAll(targetCells);
    otherCells.sort([](RegionCell const& a, RegionCell const& b) { return a.rBound < b.rBound; });

    // convert back into cells and boundaries
    targetLayer.cells.clear();
    targetLayer.boundaries.clear();
    for (std::size_t i = 0; i < otherCells.size(); ++i) {
      targetLayer.cells.append(otherCells[i].region);
      if (i < otherCells.size() - 1)
        targetLayer.boundaries.append(otherCells[i].rBound);
    }
  }

  // this->root().logger().info("boundaries after expansion are {}", targetLayer.boundaries);

  return std::pair(std::move(targetLayer), std::move(regionRects));
}

BiomeIndex WorldLayout::registerBiome(std::shared_ptr<Biome const> biome) {
  std::size_t foundIndex = m_biomes.indexOf(biome);
  if (foundIndex != NPos)
    return foundIndex + 1;

  m_biomes.append(std::move(biome));
  return m_biomes.size();
}

TerrainSelectorIndex WorldLayout::registerTerrainSelector(std::shared_ptr<TerrainSelector const> terrainSelector) {
  std::size_t foundIndex = m_terrainSelectors.indexOf(terrainSelector);
  if (foundIndex != NPos)
    return foundIndex + 1;

  m_terrainSelectors.append(std::move(terrainSelector));
  return m_terrainSelectors.size();
}

std::expected<WorldRegion, StarException> WorldLayout::buildRegion(std::uint64_t seed, RegionParams const& regionParams) {
  auto autoterrainDatabaseResult1 = this->root().terrainDatabase();
  if (!autoterrainDatabaseResult1) return std::unexpected(std::move(autoterrainDatabaseResult1.error()));
  auto terrainDatabase = std::move((*autoterrainDatabaseResult1));
  auto autobiomeDatabaseResult1 = this->root().biomeDatabase();
  if (!autobiomeDatabaseResult1) return std::unexpected(std::move(autobiomeDatabaseResult1.error()));
  auto biomeDatabase = std::move((*autobiomeDatabaseResult1));

  WorldRegion region;

  TerrainSelectorParameters baseSelectorParameters(this->root().random());
  baseSelectorParameters.worldWidth = m_worldSize[0];
  baseSelectorParameters.baseHeight = regionParams.baseHeight;

  TerrainSelectorParameters terrainSelectorParameters =
    baseSelectorParameters.withSeed(static_random_u64(seed, "Terrain"));
  TerrainSelectorParameters foregroundCaveSelectorParameters =
    baseSelectorParameters.withSeed(static_random_u64(seed, "ForegroundCaveSeed"));
  TerrainSelectorParameters backgroundCaveSelectorParameters =
    baseSelectorParameters.withSeed(static_random_u64(seed, "BackgroundCave"));

  if (regionParams.terrainSelector) {
    auto regionterrainSelectorIndexResult1 = terrainDatabase->createNamedSelector(*regionParams.terrainSelector, terrainSelectorParameters);
    if (!regionterrainSelectorIndexResult1) return std::unexpected(std::move(regionterrainSelectorIndexResult1.error()));
    region.terrainSelectorIndex = std::move(registerTerrainSelector((*regionterrainSelectorIndexResult1)));
  }
  if (regionParams.fgCaveSelector) {
    auto regionforegroundCaveSelectorIndexResult1 = terrainDatabase->createNamedSelector(*regionParams.fgCaveSelector, foregroundCaveSelectorParameters);
    if (!regionforegroundCaveSelectorIndexResult1) return std::unexpected(std::move(regionforegroundCaveSelectorIndexResult1.error()));
    region.foregroundCaveSelectorIndex = std::move(registerTerrainSelector((*regionforegroundCaveSelectorIndexResult1)));
  }
  if (regionParams.bgCaveSelector) {
    auto regionbackgroundCaveSelectorIndexResult1 = terrainDatabase->createNamedSelector(*regionParams.bgCaveSelector, backgroundCaveSelectorParameters);
    if (!regionbackgroundCaveSelectorIndexResult1) return std::unexpected(std::move(regionbackgroundCaveSelectorIndexResult1.error()));
    region.backgroundCaveSelectorIndex = std::move(registerTerrainSelector((*regionbackgroundCaveSelectorIndexResult1)));

  }
  if (regionParams.biomeName) {
    auto biome_configuration = biomeDatabase->createBiome(*regionParams.biomeName, static_random_u64(seed, "BiomeSeed"),
                                                          regionParams.baseHeight, regionParams.threatLevel);
    if (!biome_configuration)
      return std::unexpected(std::move(biome_configuration.error()));
    auto biome = std::move((*biome_configuration));

    if (regionParams.subBlockSelector) {
      for (std::size_t i = 0; i < biome->subBlocks.size(); ++i) {
        auto selector_configuration = terrainDatabase->createNamedSelector(
          *regionParams.subBlockSelector, terrainSelectorParameters.withSeed(static_random_u64(seed, i, "subBlocks")));
        if (!selector_configuration)
          return std::unexpected(std::move(selector_configuration.error()));
        auto selector = std::move((*selector_configuration));
        region.subBlockSelectorIndexes.append(registerTerrainSelector(selector));
      }
    }

    for (auto const& [index, ore] : std::views::enumerate(biome->ores)) {
      auto oreSelectorTerrainParameters = terrainSelectorParameters.withCommonality(ore.second);

      if (regionParams.fgOreSelector) {
        auto foreground_selector =
          terrainDatabase->createNamedSelector(*regionParams.fgOreSelector,
                                               oreSelectorTerrainParameters.withSeed(static_random_u64(
                                                 seed, static_cast<std::size_t>(index), "FGOreSelector")));
        if (!foreground_selector)
          return std::unexpected(std::move(foreground_selector.error()));
        auto fgSelector = std::move((*foreground_selector));
        region.foregroundOreSelectorIndexes.append(registerTerrainSelector(fgSelector));
      }

      if (regionParams.bgOreSelector) {
        auto background_selector =
          terrainDatabase->createNamedSelector(*regionParams.bgOreSelector,
                                               oreSelectorTerrainParameters.withSeed(static_random_u64(
                                                 seed, static_cast<std::size_t>(index), "BGOreSelector")));
        if (!background_selector)
          return std::unexpected(std::move(background_selector.error()));
        auto bgSelector = std::move((*background_selector));
        region.backgroundOreSelectorIndexes.append(registerTerrainSelector(bgSelector));
      }
    }

    region.blockBiomeIndex = registerBiome(biome);
    region.environmentBiomeIndex = region.blockBiomeIndex;
  }

  region.regionLiquids = regionParams.regionLiquids;

  return region;
}

std::expected<void, StarException> WorldLayout::addLayer(std::uint64_t seed, int yStart, RegionParams regionParams) {
  WorldLayer layer;
  layer.yStart = yStart;
  auto stdshared_ptrWorldRegionregionResult1 = buildRegion(seed, regionParams);
  if (!stdshared_ptrWorldRegionregionResult1) return std::unexpected(std::move(stdshared_ptrWorldRegionregionResult1.error()));
  std::shared_ptr<WorldRegion> region = std::make_shared<WorldRegion>(std::move(*stdshared_ptrWorldRegionregionResult1));
  layer.cells.append(region);

  m_layers.append(std::move(layer));
  return {};
}

std::expected<void, StarException> WorldLayout::addLayer(std::uint64_t seed, int yStart, int yBase, String const& primaryBiome,
    RegionParams primaryRegionParams, RegionParams primarySubRegionParams,
    List<RegionParams> secondaryRegions, List<RegionParams> secondarySubRegions,
    Vec2F secondaryRegionSize, Vec2F subRegionSize) {
  WorldLayer layer;
  layer.yStart = yStart;

  List<float> relativeRegionSizes;
  float totalRelativeSize = 0.0;
  int mix = 0;
  auto BiomeIndexprimaryEnvironmentBiomeIndexResult1 = buildRegion(seed, primaryRegionParams);
  if (!BiomeIndexprimaryEnvironmentBiomeIndexResult1) return std::unexpected(std::move(BiomeIndexprimaryEnvironmentBiomeIndexResult1.error()));
  BiomeIndex primaryEnvironmentBiomeIndex = std::move((*BiomeIndexprimaryEnvironmentBiomeIndexResult1).environmentBiomeIndex);

  Set<BiomeIndex> spawnBiomeIndexes;

  auto addRegion = [&](RegionParams const& regionParams, RegionParams const& subRegionParams, Vec2F const& regionSizeRange) -> std::expected<void, StarException> {
    auto stdshared_ptrWorldRegionregionResult1 = buildRegion(seed, regionParams);
    if (!stdshared_ptrWorldRegionregionResult1) return std::unexpected(std::move(stdshared_ptrWorldRegionregionResult1.error()));
    std::shared_ptr<WorldRegion> region = std::make_shared<WorldRegion>(std::move(*stdshared_ptrWorldRegionregionResult1));
    auto stdshared_ptrWorldRegionsubRegionResult1 = buildRegion(seed, subRegionParams);
    if (!stdshared_ptrWorldRegionsubRegionResult1) return std::unexpected(std::move(stdshared_ptrWorldRegionsubRegionResult1.error()));
    std::shared_ptr<WorldRegion> subRegion = std::make_shared<WorldRegion>(std::move(*stdshared_ptrWorldRegionsubRegionResult1));
    auto autouseSecondaryBiomeResult1 = this->root().assets();
    if (!autouseSecondaryBiomeResult1) return std::unexpected(std::move(autouseSecondaryBiomeResult1.error()));
    auto autouseSecondaryBiomeResult2 = (*autouseSecondaryBiomeResult1)->json("/terrestrial_worlds.config:useSecondaryEnvironmentBiomeIndex");
    if (!autouseSecondaryBiomeResult2) return std::unexpected(std::move(autouseSecondaryBiomeResult2.error()));
    auto autouseSecondaryBiomeResult3 = (*autouseSecondaryBiomeResult2).toBool();
    if (!autouseSecondaryBiomeResult3) return std::unexpected(std::move(autouseSecondaryBiomeResult3.error()));
    auto useSecondaryBiome = std::move((*autouseSecondaryBiomeResult3));
    if (!useSecondaryBiome)
      region->environmentBiomeIndex = primaryEnvironmentBiomeIndex;
    subRegion->environmentBiomeIndex = region->environmentBiomeIndex;

    if (regionParams.biomeName == primaryBiome)
      spawnBiomeIndexes.add(region->blockBiomeIndex);
    if (subRegionParams.biomeName == primaryBiome)
      spawnBiomeIndexes.add(subRegion->blockBiomeIndex);

    layer.cells.append(region);
    layer.cells.append(subRegion);
    layer.cells.append(region);
    auto region_relative_size = static_random_float_range(regionSizeRange[0], regionSizeRange[1], seed, ++mix, yStart);
    float regionRelativeSize = std::move(region_relative_size);
    auto subregion_relative_size = static_random_float_range(subRegionSize[0], subRegionSize[1], seed, ++mix, yStart);
    float subRegionRelativeSize = std::move(subregion_relative_size);
    totalRelativeSize += regionRelativeSize;

    if (subRegionRelativeSize >= 1.0f)
      return std::unexpected(StarException("Relative size of subRegion must be less than 1.0!"));

    subRegionRelativeSize *= regionRelativeSize;
    regionRelativeSize -= subRegionRelativeSize;

    relativeRegionSizes.append(regionRelativeSize / 2);
    relativeRegionSizes.append(subRegionRelativeSize);
    relativeRegionSizes.append(regionRelativeSize / 2);
    return {};
  };

  // construct list of region cells and relative sizes
  {
    auto added = addRegion(primaryRegionParams, primarySubRegionParams, Vec2F(1, 1));
    if (!added) return std::unexpected(std::move(added.error()));
  }
  for (std::size_t i = 0; i < secondaryRegions.size(); ++i)
    {
      auto added = addRegion(secondaryRegions[i], secondarySubRegions[i], secondaryRegionSize);
      if (!added) return std::unexpected(std::move(added.error()));
    }

  // construct boundaries based on normalized sizes
    auto next_boundary = static_random_i32_range(0, m_worldSize[0] - 1, seed, yStart, "LayerOffset");
    int nextBoundary = std::move(next_boundary);
    layer.boundaries.append(nextBoundary);
    for (std::size_t i = 0; i + 1 < relativeRegionSizes.size(); ++i) {
      int regionSize = static_cast<int>(m_worldSize[0]) * (relativeRegionSizes[i] / totalRelativeSize);
      nextBoundary += regionSize;
      layer.boundaries.append(nextBoundary);
    }

  // wrap cells + boundaries
  for (;;) {
    auto last = layer.boundaries.last();
    if (!last) return std::unexpected(std::move(last.error()));
    if (last->get() <= static_cast<int>(m_worldSize[0]))
      break;
    auto cell = layer.cells.takeLast();
    if (!cell) return std::unexpected(std::move(cell.error()));
    layer.cells.prepend(std::move(*cell));
    auto boundary = layer.boundaries.takeLast();
    if (!boundary) return std::unexpected(std::move(boundary.error()));
    layer.boundaries.prepend(*boundary - m_worldSize[0]);
  }
  auto lastCell = layer.cells.last();
  if (!lastCell) return std::unexpected(std::move(lastCell.error()));
  layer.cells.prepend(lastCell->get());
  auto intyRangeResult1 = this->root().assets();
  if (!intyRangeResult1) return std::unexpected(std::move(intyRangeResult1.error()));
  auto intyRangeResult2 = (*intyRangeResult1)->json("/world_template.config:playerStartSearchYRange");
  if (!intyRangeResult2) return std::unexpected(std::move(intyRangeResult2.error()));
  auto intyRangeResult3 = (*intyRangeResult2).toInt();
  if (!intyRangeResult3) return std::unexpected(std::move(intyRangeResult3.error()));
  int yRange = std::move((*intyRangeResult3));
  int i = 0;
  int lastBoundary = 0;
  for (auto const& region : layer.cells) {
    int nextBoundary = std::move(i < static_cast<int>(layer.boundaries.size()) ? layer.boundaries[i] : m_worldSize[0]);
    if (spawnBiomeIndexes.contains(region->blockBiomeIndex))
      m_playerStartSearchRegions.append(RectI(lastBoundary, std::max(0, yBase - yRange), nextBoundary,
                                              std::min(static_cast<int>(m_worldSize[1]), yBase + yRange)));
    lastBoundary = nextBoundary;
    i++;
  }

  m_layers.append(std::move(layer));
  return {};
}

std::expected<void, StarException> WorldLayout::finalize(Color mainSkyColor) {
  std::sort(m_layers.begin(), m_layers.end(), [](WorldLayer const& a, WorldLayer const& b) { return a.yStart < b.yStart; });

  // Post-process all parallaxes
  for (auto const& biome : m_biomes) {
    if (biome->parallax) {
      auto faded = biome->parallax->fadeToSkyColor(mainSkyColor);
      if (!faded) return std::unexpected(std::move(faded.error()));
    }
  }
  return {};
}

std::pair<std::size_t, int> WorldLayout::findContainingCell(WorldLayer const& layer, int x) const {
  x = WorldGeometry(m_worldSize).xwrap(x);
  auto xi = std::lower_bound(layer.boundaries.begin(), layer.boundaries.end(), x);
  return {xi - layer.boundaries.begin(), x};
}

std::pair<std::size_t, int> WorldLayout::leftCell(WorldLayer const& layer, std::size_t cellIndex, int x) const {
  if (cellIndex == 0)
    return {layer.cells.size() - 1, x + static_cast<int>(m_worldSize[0])};
  else
    return {cellIndex - 1, x};
}

std::pair<std::size_t, int> WorldLayout::rightCell(WorldLayer const& layer, std::size_t cellIndex, int x) const {
  if (cellIndex >= layer.cells.size() - 1)
    return {0, x - static_cast<int>(m_worldSize[0])};
  else
    return {cellIndex + 1, x};
}

}

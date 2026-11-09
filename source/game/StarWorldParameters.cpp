import star.json;
import star.json_extra;
import star.vector;
import star.game_types;
import star.liquid_types;
import star.color;
import star.bi_map;
import star.random;
import star.weighted_pool;
import star.list;
import star.image_processing;
import star.hash;
import star.data_stream;
import star.string_view;
import star.thread;
import star.directives;
import star.asset_path;
import star.data_stream_extra;
import star.data_stream_devices;
import star.ordered_map;
import star.io_device;
import star.ref_ptr;
import star.logging;
import star.listener;
import std;
import star.string;
import star.format;
import star.version;

import star.sky_types;
import star.animation;
import star.particle;
import star.weather_types;
import star.force_regions;
import star.world_parameters;
import star.asset_source;
import star.assets;

import star.configuration;
import star.root;

import star.liquids_database;
import star.biome_database;


import star.byte_array;
import star.static_random;
namespace Star {

extern std::expected<EnumMap<WorldParametersType>, StarException> const WorldParametersTypeNames = EnumMap<WorldParametersType>::create({
    {WorldParametersType::TerrestrialWorldParameters, "TerrestrialWorldParameters"},
    {WorldParametersType::AsteroidsWorldParameters, "AsteroidsWorldParameters"},
    {WorldParametersType::FloatingDungeonWorldParameters, "FloatingDungeonWorldParameters"}});

extern std::expected<EnumMap<BeamUpRule>, StarException> const BeamUpRuleNames = EnumMap<BeamUpRule>::create({
  {BeamUpRule::Nowhere, "Nowhere"},
  {BeamUpRule::Surface, "Surface"},
  {BeamUpRule::Anywhere, "Anywhere"},
  {BeamUpRule::AnywhereWithWarning, "AnywhereWithWarning"}});

extern std::expected<EnumMap<WorldEdgeForceRegionType>, StarException> const WorldEdgeForceRegionTypeNames = EnumMap<WorldEdgeForceRegionType>::create({
  {WorldEdgeForceRegionType::None, "None"},
  {WorldEdgeForceRegionType::Top, "Top"},
  {WorldEdgeForceRegionType::Bottom, "Bottom"},
  {WorldEdgeForceRegionType::TopAndBottom, "TopAndBottom"}});

std::expected<void, StarException> VisitableWorldParameters::load(Json const& store) {
  auto typeNameValue1 = store.getString("typeName", "");
  if (!typeNameValue1) return std::unexpected(std::move(typeNameValue1.error()));
  typeName = std::move(*typeNameValue1);
  auto threatLevelValue1 = store.getFloat("threatLevel");
  if (!threatLevelValue1) return std::unexpected(std::move(threatLevelValue1.error()));
  threatLevel = std::move(*threatLevelValue1);
  auto worldSizeValue1 = store.get("worldSize");
  if (!worldSizeValue1) return std::unexpected(std::move(worldSizeValue1.error()));
  auto worldSizeValue2 = jsonToVec2U((*worldSizeValue1));
  if (!worldSizeValue2) return std::unexpected(std::move(worldSizeValue2.error()));
  worldSize = std::move(*worldSizeValue2);
  auto gravityValue1 = store.getFloat("gravity", 1.0f);
  if (!gravityValue1) return std::unexpected(std::move(gravityValue1.error()));
  gravity = std::move(*gravityValue1);
  auto airlessValue1 = store.getBool("airless", false);
  if (!airlessValue1) return std::unexpected(std::move(airlessValue1.error()));
  airless = std::move(*airlessValue1);
  auto weatherPoolValue1 = store.getArray("weatherPool", JsonArray());
  if (!weatherPoolValue1) return std::unexpected(std::move(weatherPoolValue1.error()));
  auto weatherPoolValue2 = jsonToWeightedPool<String>((*weatherPoolValue1));
  if (!weatherPoolValue2) return std::unexpected(std::move(weatherPoolValue2.error()));
  weatherPool = std::move(*weatherPoolValue2);
  auto environmentStatusEffectsJson = store.opt("environmentStatusEffects");
  if (!environmentStatusEffectsJson) return std::unexpected(std::move(environmentStatusEffectsJson.error()));
  if (*environmentStatusEffectsJson) {
    auto parsed = jsonToStringList(**environmentStatusEffectsJson);
    if (!parsed) return std::unexpected(std::move(parsed.error()));
    environmentStatusEffects = std::move(*parsed);
  } else {
    environmentStatusEffects = StringList{};
  }
  auto overrideTechJson = store.opt("overrideTech");
  if (!overrideTechJson) return std::unexpected(std::move(overrideTechJson.error()));
  if (*overrideTechJson) {
    auto parsed = jsonToStringList(**overrideTechJson);
    if (!parsed) return std::unexpected(std::move(parsed.error()));
    overrideTech = std::move(*parsed);
  } else {
    overrideTech = std::nullopt;
  }
  auto globalDirectivesJson = store.opt("globalDirectives");
  if (!globalDirectivesJson) return std::unexpected(std::move(globalDirectivesJson.error()));
  if (*globalDirectivesJson) {
    auto parsed = jsonToDirectivesList(**globalDirectivesJson);
    if (!parsed) return std::unexpected(std::move(parsed.error()));
    globalDirectives = std::move(*parsed);
  } else {
    globalDirectives = std::nullopt;
  }
  auto beamUpRuleValue1 = store.getString("beamUpRule", "Surface");
  if (!beamUpRuleValue1) return std::unexpected(std::move(beamUpRuleValue1.error()));
  if (!BeamUpRuleNames) return std::unexpected(BeamUpRuleNames.error());
  auto beamUpRuleValue2 = BeamUpRuleNames->get_left((*beamUpRuleValue1));
  if (!beamUpRuleValue2) return std::unexpected(std::move(beamUpRuleValue2.error()));
  beamUpRule = beamUpRuleValue2->get();
  auto disableDeathDropsValue1 = store.getBool("disableDeathDrops", false);
  if (!disableDeathDropsValue1) return std::unexpected(std::move(disableDeathDropsValue1.error()));
  disableDeathDrops = std::move(*disableDeathDropsValue1);
  auto terraformedValue1 = store.getBool("terraformed", false);
  if (!terraformedValue1) return std::unexpected(std::move(terraformedValue1.error()));
  terraformed = std::move(*terraformedValue1);
  auto worldEdgeForceRegionsValue1 = store.getString("worldEdgeForceRegions", "None");
  if (!worldEdgeForceRegionsValue1) return std::unexpected(std::move(worldEdgeForceRegionsValue1.error()));
  if (!WorldEdgeForceRegionTypeNames) return std::unexpected(WorldEdgeForceRegionTypeNames.error());
  auto worldEdgeForceRegionsValue2 = WorldEdgeForceRegionTypeNames->get_left((*worldEdgeForceRegionsValue1));
  if (!worldEdgeForceRegionsValue2) return std::unexpected(std::move(worldEdgeForceRegionsValue2.error()));
  worldEdgeForceRegions = worldEdgeForceRegionsValue2->get();
  return {};
}

std::expected<Json, StarException> VisitableWorldParameters::store() const {
  auto weather = jsonFromWeightedPool<String>(weatherPool);
  if (!weather) return std::unexpected(std::move(weather.error()));
  if (!BeamUpRuleNames) return std::unexpected(BeamUpRuleNames.error());
  auto beam = BeamUpRuleNames->get_right(beamUpRule);
  if (!beam) return std::unexpected(std::move(beam.error()));
  if (!WorldEdgeForceRegionTypeNames) return std::unexpected(WorldEdgeForceRegionTypeNames.error());
  auto edge = WorldEdgeForceRegionTypeNames->get_right(worldEdgeForceRegions);
  if (!edge) return std::unexpected(std::move(edge.error()));
  return JsonObject{{"typeName", typeName},
      {"threatLevel", threatLevel},
      {"worldSize", jsonFromVec2U(worldSize)},
      {"gravity", gravity},
      {"airless", airless},
      {"weatherPool", std::move(*weather)},
      {"environmentStatusEffects", jsonFromStringList(environmentStatusEffects)},
      {"overrideTech", jsonFromOptional(overrideTech.transform(&jsonFromStringList))},
      {"globalDirectives", jsonFromOptional(globalDirectives.transform(&jsonFromDirectivesList))},
      {"beamUpRule", beam->get()},
      {"disableDeathDrops", disableDeathDrops},
      {"terraformed", terraformed},
      {"worldEdgeForceRegions", edge->get()}};
}

std::expected<void, StarException> VisitableWorldParameters::read(DataStream& ds) {
  {

    auto result = ds >> typeName;

    if (!result) return std::unexpected(std::move(result.error()));

  }
  {
    auto result = ds >> threatLevel;
    if (!result) return std::unexpected(std::move(result.error()));
  }
  {
    auto result = ds >> worldSize;
    if (!result) return std::unexpected(std::move(result.error()));
  }
  {
    auto result = ds >> gravity;
    if (!result) return std::unexpected(std::move(result.error()));
  }
  {
    auto result = ds >> airless;
    if (!result) return std::unexpected(std::move(result.error()));
  }
  auto items = ds.read<WeatherPool::ItemsList>();
  if (!items) return std::unexpected(std::move(items.error()));
  weatherPool = WeatherPool(std::move(*items));
  {
    auto result = ds >> environmentStatusEffects;
    if (!result) return std::unexpected(std::move(result.error()));
  }
  {
    auto result = ds >> overrideTech;
    if (!result) return std::unexpected(std::move(result.error()));
  }
  {
    auto result = ds >> globalDirectives;
    if (!result) return std::unexpected(std::move(result.error()));
  }
  {
    auto result = ds >> beamUpRule;
    if (!result) return std::unexpected(std::move(result.error()));
  }
  {
    auto result = ds >> disableDeathDrops;
    if (!result) return std::unexpected(std::move(result.error()));
  }
  {
    auto result = ds >> terraformed;
    if (!result) return std::unexpected(std::move(result.error()));
  }
  {
    auto result = ds >> worldEdgeForceRegions;
    if (!result) return std::unexpected(std::move(result.error()));
  }
  return {};
}

std::expected<void, StarException> VisitableWorldParameters::write(DataStream& ds) const {
  {

    auto result = ds << typeName;

    if (!result) return std::unexpected(std::move(result.error()));

  }
  {
    auto result = ds << threatLevel;
    if (!result) return std::unexpected(std::move(result.error()));
  }
  {
    auto result = ds << worldSize;
    if (!result) return std::unexpected(std::move(result.error()));
  }
  {
    auto result = ds << gravity;
    if (!result) return std::unexpected(std::move(result.error()));
  }
  {
    auto result = ds << airless;
    if (!result) return std::unexpected(std::move(result.error()));
  }
  {
    auto result = ds.write_container<WeatherPool::ItemsList>(weatherPool.items());
    if (!result) return std::unexpected(std::move(result.error()));
  }
  {
    auto result = ds << environmentStatusEffects;
    if (!result) return std::unexpected(std::move(result.error()));
  }
  {
    auto result = ds << overrideTech;
    if (!result) return std::unexpected(std::move(result.error()));
  }
  {
    auto result = ds << globalDirectives;
    if (!result) return std::unexpected(std::move(result.error()));
  }
  {
    auto result = ds << beamUpRule;
    if (!result) return std::unexpected(std::move(result.error()));
  }
  {
    auto result = ds << disableDeathDrops;
    if (!result) return std::unexpected(std::move(result.error()));
  }
  {
    auto result = ds << terraformed;
    if (!result) return std::unexpected(std::move(result.error()));
  }
  {
    auto result = ds << worldEdgeForceRegions;
    if (!result) return std::unexpected(std::move(result.error()));
  }
  return {};
}

std::expected<std::shared_ptr<TerrestrialWorldParameters>, StarException> TerrestrialWorldParameters::create(Json const& store) {
  auto result = std::make_shared<TerrestrialWorldParameters>();
  auto base = result->load(store);
  if (!base) return std::unexpected(std::move(base.error()));
  auto loadTerrestrialRegion = [](Json const& config) -> std::expected<TerrestrialRegion, StarException> {
    TerrestrialRegion region;
    auto biomeValue1 = config.getString("biome");
    if (!biomeValue1) return std::unexpected(std::move(biomeValue1.error()));
    region.biome = std::move(*biomeValue1);
    auto blockSelectorValue1 = config.getString("blockSelector");
    if (!blockSelectorValue1) return std::unexpected(std::move(blockSelectorValue1.error()));
    region.blockSelector = std::move(*blockSelectorValue1);
    auto fgCaveSelectorValue1 = config.getString("fgCaveSelector");
    if (!fgCaveSelectorValue1) return std::unexpected(std::move(fgCaveSelectorValue1.error()));
    region.fgCaveSelector = std::move(*fgCaveSelectorValue1);
    auto bgCaveSelectorValue1 = config.getString("bgCaveSelector");
    if (!bgCaveSelectorValue1) return std::unexpected(std::move(bgCaveSelectorValue1.error()));
    region.bgCaveSelector = std::move(*bgCaveSelectorValue1);
    auto fgOreSelectorValue1 = config.getString("fgOreSelector");
    if (!fgOreSelectorValue1) return std::unexpected(std::move(fgOreSelectorValue1.error()));
    region.fgOreSelector = std::move(*fgOreSelectorValue1);
    auto bgOreSelectorValue1 = config.getString("bgOreSelector");
    if (!bgOreSelectorValue1) return std::unexpected(std::move(bgOreSelectorValue1.error()));
    region.bgOreSelector = std::move(*bgOreSelectorValue1);
    auto subBlockSelectorValue1 = config.getString("subBlockSelector");
    if (!subBlockSelectorValue1) return std::unexpected(std::move(subBlockSelectorValue1.error()));
    region.subBlockSelector = std::move(*subBlockSelectorValue1);
    auto caveLiquidValue1 = config.getUInt("caveLiquid");
    if (!caveLiquidValue1) return std::unexpected(std::move(caveLiquidValue1.error()));
    region.caveLiquid = static_cast<LiquidId>((*caveLiquidValue1));
    auto caveLiquidSeedDensityValue1 = config.getFloat("caveLiquidSeedDensity");
    if (!caveLiquidSeedDensityValue1) return std::unexpected(std::move(caveLiquidSeedDensityValue1.error()));
    region.caveLiquidSeedDensity = std::move(*caveLiquidSeedDensityValue1);
    auto oceanLiquidValue1 = config.getUInt("oceanLiquid");
    if (!oceanLiquidValue1) return std::unexpected(std::move(oceanLiquidValue1.error()));
    region.oceanLiquid = static_cast<LiquidId>((*oceanLiquidValue1));
    auto oceanLiquidLevelValue1 = config.getInt("oceanLiquidLevel");
    if (!oceanLiquidLevelValue1) return std::unexpected(std::move(oceanLiquidLevelValue1.error()));
    region.oceanLiquidLevel = static_cast<int>((*oceanLiquidLevelValue1));
    auto encloseLiquidsValue1 = config.getBool("encloseLiquids");
    if (!encloseLiquidsValue1) return std::unexpected(std::move(encloseLiquidsValue1.error()));
    region.encloseLiquids = std::move(*encloseLiquidsValue1);
    auto fillMicrodungeonsValue1 = config.getBool("fillMicrodungeons");
    if (!fillMicrodungeonsValue1) return std::unexpected(std::move(fillMicrodungeonsValue1.error()));
    region.fillMicrodungeons = std::move(*fillMicrodungeonsValue1);
    return region;
  };

  auto loadTerrestrialLayer = [loadTerrestrialRegion](Json const& config) -> std::expected<TerrestrialLayer, StarException> {
    TerrestrialLayer layer;
    auto layerMinHeightValue1 = config.getInt("layerMinHeight");
    if (!layerMinHeightValue1) return std::unexpected(std::move(layerMinHeightValue1.error()));
    layer.layerMinHeight = static_cast<int>((*layerMinHeightValue1));
    auto layerBaseHeightValue1 = config.getInt("layerBaseHeight");
    if (!layerBaseHeightValue1) return std::unexpected(std::move(layerBaseHeightValue1.error()));
    layer.layerBaseHeight = static_cast<int>((*layerBaseHeightValue1));
    auto dungeonsValue1 = config.get("dungeons");
    if (!dungeonsValue1) return std::unexpected(std::move(dungeonsValue1.error()));
    auto dungeonsValue2 = jsonToStringList((*dungeonsValue1));
    if (!dungeonsValue2) return std::unexpected(std::move(dungeonsValue2.error()));
    layer.dungeons = std::move(*dungeonsValue2);
    auto dungeonXVarianceValue1 = config.getInt("dungeonXVariance");
    if (!dungeonXVarianceValue1) return std::unexpected(std::move(dungeonXVarianceValue1.error()));
    layer.dungeonXVariance = static_cast<int>((*dungeonXVarianceValue1));
    auto primaryRegionValue1 = config.get("primaryRegion");
    if (!primaryRegionValue1) return std::unexpected(std::move(primaryRegionValue1.error()));
    auto primaryRegionValue2 = loadTerrestrialRegion((*primaryRegionValue1));
    if (!primaryRegionValue2) return std::unexpected(std::move(primaryRegionValue2.error()));
    layer.primaryRegion = std::move(*primaryRegionValue2);
    auto primarySubRegionValue1 = config.get("primarySubRegion");
    if (!primarySubRegionValue1) return std::unexpected(std::move(primarySubRegionValue1.error()));
    auto primarySubRegionValue2 = loadTerrestrialRegion((*primarySubRegionValue1));
    if (!primarySubRegionValue2) return std::unexpected(std::move(primarySubRegionValue2.error()));
    layer.primarySubRegion = std::move(*primarySubRegionValue2);
    auto secondaryRegions = config.getArray("secondaryRegions");
    if (!secondaryRegions) return std::unexpected(std::move(secondaryRegions.error()));
    for (auto const& entry : *secondaryRegions) {
      auto region = loadTerrestrialRegion(entry);
      if (!region) return std::unexpected(std::move(region.error()));
      layer.secondaryRegions.push_back(std::move(*region));
    }
    auto secondarySubRegions = config.getArray("secondarySubRegions");
    if (!secondarySubRegions) return std::unexpected(std::move(secondarySubRegions.error()));
    for (auto const& entry : *secondarySubRegions) {
      auto region = loadTerrestrialRegion(entry);
      if (!region) return std::unexpected(std::move(region.error()));
      layer.secondarySubRegions.push_back(std::move(*region));
    }
    auto secondaryRegionSizeRangeValue1 = config.get("secondaryRegionSizeRange");
    if (!secondaryRegionSizeRangeValue1) return std::unexpected(std::move(secondaryRegionSizeRangeValue1.error()));
    auto secondaryRegionSizeRangeValue2 = jsonToVec2F((*secondaryRegionSizeRangeValue1));
    if (!secondaryRegionSizeRangeValue2) return std::unexpected(std::move(secondaryRegionSizeRangeValue2.error()));
    layer.secondaryRegionSizeRange = std::move(*secondaryRegionSizeRangeValue2);
    auto subRegionSizeRangeValue1 = config.get("subRegionSizeRange");
    if (!subRegionSizeRangeValue1) return std::unexpected(std::move(subRegionSizeRangeValue1.error()));
    auto subRegionSizeRangeValue2 = jsonToVec2F((*subRegionSizeRangeValue1));
    if (!subRegionSizeRangeValue2) return std::unexpected(std::move(subRegionSizeRangeValue2.error()));
    layer.subRegionSizeRange = std::move(*subRegionSizeRangeValue2);
    return layer;
  };

  auto primaryBiomeValue1 = store.getString("primaryBiome");
  if (!primaryBiomeValue1) return std::unexpected(std::move(primaryBiomeValue1.error()));
  result->primaryBiome = std::move(*primaryBiomeValue1);
  auto primarySurfaceLiquidValue1 = store.getUInt("surfaceLiquid");
  if (!primarySurfaceLiquidValue1) return std::unexpected(std::move(primarySurfaceLiquidValue1.error()));
  result->primarySurfaceLiquid = std::move(*primarySurfaceLiquidValue1);
  auto sizeNameValue1 = store.getString("sizeName");
  if (!sizeNameValue1) return std::unexpected(std::move(sizeNameValue1.error()));
  result->sizeName = std::move(*sizeNameValue1);
  auto hueShiftValue1 = store.getFloat("hueShift");
  if (!hueShiftValue1) return std::unexpected(std::move(hueShiftValue1.error()));
  result->hueShift = std::move(*hueShiftValue1);
  auto skyColoringValue1 = store.get("skyColoring");
  if (!skyColoringValue1) return std::unexpected(std::move(skyColoringValue1.error()));
  auto skyColoringValue2 = SkyColoring::create((*skyColoringValue1));
  if (!skyColoringValue2) return std::unexpected(std::move(skyColoringValue2.error()));
  result->skyColoring = std::move(*skyColoringValue2);
  auto dayLengthValue1 = store.getFloat("dayLength");
  if (!dayLengthValue1) return std::unexpected(std::move(dayLengthValue1.error()));
  result->dayLength = std::move(*dayLengthValue1);
  auto blockNoiseConfigValue1 = store.get("blockNoise");
  if (!blockNoiseConfigValue1) return std::unexpected(std::move(blockNoiseConfigValue1.error()));
  result->blockNoiseConfig = std::move(*blockNoiseConfigValue1);
  auto blendNoiseConfigValue1 = store.get("blendNoise");
  if (!blendNoiseConfigValue1) return std::unexpected(std::move(blendNoiseConfigValue1.error()));
  result->blendNoiseConfig = std::move(*blendNoiseConfigValue1);
  auto blendSizeValue1 = store.getFloat("blendSize");
  if (!blendSizeValue1) return std::unexpected(std::move(blendSizeValue1.error()));
  result->blendSize = std::move(*blendSizeValue1);
  auto spaceLayerValue1 = store.get("spaceLayer");
  if (!spaceLayerValue1) return std::unexpected(std::move(spaceLayerValue1.error()));
  auto spaceLayerValue2 = loadTerrestrialLayer((*spaceLayerValue1));
  if (!spaceLayerValue2) return std::unexpected(std::move(spaceLayerValue2.error()));
  result->spaceLayer = std::move(*spaceLayerValue2);
  auto atmosphereLayerValue1 = store.get("atmosphereLayer");
  if (!atmosphereLayerValue1) return std::unexpected(std::move(atmosphereLayerValue1.error()));
  auto atmosphereLayerValue2 = loadTerrestrialLayer((*atmosphereLayerValue1));
  if (!atmosphereLayerValue2) return std::unexpected(std::move(atmosphereLayerValue2.error()));
  result->atmosphereLayer = std::move(*atmosphereLayerValue2);
  auto surfaceLayerValue1 = store.get("surfaceLayer");
  if (!surfaceLayerValue1) return std::unexpected(std::move(surfaceLayerValue1.error()));
  auto surfaceLayerValue2 = loadTerrestrialLayer((*surfaceLayerValue1));
  if (!surfaceLayerValue2) return std::unexpected(std::move(surfaceLayerValue2.error()));
  result->surfaceLayer = std::move(*surfaceLayerValue2);
  auto subsurfaceLayerValue1 = store.get("subsurfaceLayer");
  if (!subsurfaceLayerValue1) return std::unexpected(std::move(subsurfaceLayerValue1.error()));
  auto subsurfaceLayerValue2 = loadTerrestrialLayer((*subsurfaceLayerValue1));
  if (!subsurfaceLayerValue2) return std::unexpected(std::move(subsurfaceLayerValue2.error()));
  result->subsurfaceLayer = std::move(*subsurfaceLayerValue2);
  auto layers = store.getArray("undergroundLayers");
  if (!layers) return std::unexpected(std::move(layers.error()));
  for (auto const& entry : *layers) {
    auto layer = loadTerrestrialLayer(entry);
    if (!layer) return std::unexpected(std::move(layer.error()));
    result->undergroundLayers.push_back(std::move(*layer));
  }
  auto coreLayerValue1 = store.get("coreLayer");
  if (!coreLayerValue1) return std::unexpected(std::move(coreLayerValue1.error()));
  auto coreLayerValue2 = loadTerrestrialLayer((*coreLayerValue1));
  if (!coreLayerValue2) return std::unexpected(std::move(coreLayerValue2.error()));
  result->coreLayer = std::move(*coreLayerValue2);
  return result;
}

TerrestrialWorldParameters &TerrestrialWorldParameters::operator=(TerrestrialWorldParameters const& terrestrialWorldParameters) {
  this->primaryBiome = terrestrialWorldParameters.primaryBiome;
  this->primarySurfaceLiquid = terrestrialWorldParameters.primarySurfaceLiquid;
  this->sizeName = terrestrialWorldParameters.sizeName;
  this->hueShift = terrestrialWorldParameters.hueShift;

  this->skyColoring = terrestrialWorldParameters.skyColoring;
  this->dayLength = terrestrialWorldParameters.dayLength;

  this->blockNoiseConfig = terrestrialWorldParameters.blockNoiseConfig;
  this->blendNoiseConfig = terrestrialWorldParameters.blendNoiseConfig;
  this->blendSize = terrestrialWorldParameters.blendSize;

  this->spaceLayer = terrestrialWorldParameters.spaceLayer;
  this->atmosphereLayer = terrestrialWorldParameters.atmosphereLayer;
  this->surfaceLayer = terrestrialWorldParameters.surfaceLayer;
  this->subsurfaceLayer = terrestrialWorldParameters.subsurfaceLayer;
  this->undergroundLayers = terrestrialWorldParameters.undergroundLayers;
  this->coreLayer = terrestrialWorldParameters.coreLayer;
  return *this;
}

WorldParametersType TerrestrialWorldParameters::type() const {
  return WorldParametersType::TerrestrialWorldParameters;
}

std::expected<Json, StarException> TerrestrialWorldParameters::store() const {
  auto storeTerrestrialRegion = [](TerrestrialRegion const& region) -> Json {
    return JsonObject{{"biome", region.biome},
        {"blockSelector", region.blockSelector},
        {"fgCaveSelector", region.fgCaveSelector},
        {"bgCaveSelector", region.bgCaveSelector},
        {"fgOreSelector", region.fgOreSelector},
        {"bgOreSelector", region.bgOreSelector},
        {"subBlockSelector", region.subBlockSelector},
        {"caveLiquid", region.caveLiquid},
        {"caveLiquidSeedDensity", region.caveLiquidSeedDensity},
        {"oceanLiquid", region.oceanLiquid},
        {"oceanLiquidLevel", region.oceanLiquidLevel},
        {"encloseLiquids", region.encloseLiquids},
        {"fillMicrodungeons", region.fillMicrodungeons}};
  };
  auto storeTerrestrialLayer = [storeTerrestrialRegion](TerrestrialLayer const& layer) -> Json {
    return JsonObject{{"layerMinHeight", layer.layerMinHeight},
        {"layerBaseHeight", layer.layerBaseHeight},
        {"dungeons", jsonFromStringList(layer.dungeons)},
        {"dungeonXVariance", layer.dungeonXVariance},
        {"primaryRegion", storeTerrestrialRegion(layer.primaryRegion)},
        {"primarySubRegion", storeTerrestrialRegion(layer.primarySubRegion)},
        {"secondaryRegions", layer.secondaryRegions.transformed(storeTerrestrialRegion)},
        {"secondarySubRegions", layer.secondarySubRegions.transformed(storeTerrestrialRegion)},
        {"secondaryRegionSizeRange", jsonFromVec2F(layer.secondaryRegionSizeRange)},
        {"subRegionSizeRange", jsonFromVec2F(layer.subRegionSizeRange)}};
  };

  auto base = VisitableWorldParameters::store();
  if (!base) return std::unexpected(std::move(base.error()));
  return base->setAll(JsonObject{{"primaryBiome", primaryBiome},
      {"sizeName", sizeName},
      {"hueShift", hueShift},
      {"surfaceLiquid", primarySurfaceLiquid},
      {"skyColoring", skyColoring.toJson()},
      {"dayLength", dayLength},
      {"blockNoise", blockNoiseConfig},
      {"blendNoise", blendNoiseConfig},
      {"blendSize", blendSize},
      {"spaceLayer", storeTerrestrialLayer(spaceLayer)},
      {"atmosphereLayer", storeTerrestrialLayer(atmosphereLayer)},
      {"surfaceLayer", storeTerrestrialLayer(surfaceLayer)},
      {"subsurfaceLayer", storeTerrestrialLayer(subsurfaceLayer)},
      {"undergroundLayers", undergroundLayers.transformed(storeTerrestrialLayer)},
      {"coreLayer", storeTerrestrialLayer(coreLayer)}});
}

[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, TerrestrialWorldParameters::TerrestrialRegion& region) {
  {

    auto result = ds >> region.biome;

    if (!result) return std::unexpected(std::move(result.error()));

  }
  {
    auto result = ds >> region.blockSelector;
    if (!result) return std::unexpected(std::move(result.error()));
  }
  {
    auto result = ds >> region.fgCaveSelector;
    if (!result) return std::unexpected(std::move(result.error()));
  }
  {
    auto result = ds >> region.bgCaveSelector;
    if (!result) return std::unexpected(std::move(result.error()));
  }
  {
    auto result = ds >> region.fgOreSelector;
    if (!result) return std::unexpected(std::move(result.error()));
  }
  {
    auto result = ds >> region.bgOreSelector;
    if (!result) return std::unexpected(std::move(result.error()));
  }
  {
    auto result = ds >> region.subBlockSelector;
    if (!result) return std::unexpected(std::move(result.error()));
  }
  {
    auto result = ds >> region.caveLiquid;
    if (!result) return std::unexpected(std::move(result.error()));
  }
  {
    auto result = ds >> region.caveLiquidSeedDensity;
    if (!result) return std::unexpected(std::move(result.error()));
  }
  {
    auto result = ds >> region.oceanLiquid;
    if (!result) return std::unexpected(std::move(result.error()));
  }
  {
    auto result = ds >> region.oceanLiquidLevel;
    if (!result) return std::unexpected(std::move(result.error()));
  }
  {
    auto result = ds >> region.encloseLiquids;
    if (!result) return std::unexpected(std::move(result.error()));
  }
  {
    auto result = ds >> region.fillMicrodungeons;
    if (!result) return std::unexpected(std::move(result.error()));
  }
  return std::ref(ds);
}

std::expected<void, StarException> TerrestrialWorldParameters::read(DataStream& ds) {
  auto readTerrestrialLayer = [](DataStream& ds, TerrestrialLayer& layer) -> std::expected<void, StarException> {
    {
      auto result = ds >> layer.layerMinHeight;
      if (!result) return std::unexpected(std::move(result.error()));
    }
    {
      auto result = ds >> layer.layerBaseHeight;
      if (!result) return std::unexpected(std::move(result.error()));
    }
    {
      auto result = ds >> layer.dungeons;
      if (!result) return std::unexpected(std::move(result.error()));
    }
    {
      auto result = ds >> layer.dungeonXVariance;
      if (!result) return std::unexpected(std::move(result.error()));
    }
    {
      auto result = ds >> layer.primaryRegion;
      if (!result) return std::unexpected(std::move(result.error()));
    }
    {
      auto result = ds >> layer.primarySubRegion;
      if (!result) return std::unexpected(std::move(result.error()));
    }
    {
      auto result = ds >> layer.secondaryRegions;
      if (!result) return std::unexpected(std::move(result.error()));
    }
    {
      auto result = ds >> layer.secondarySubRegions;
      if (!result) return std::unexpected(std::move(result.error()));
    }
    {
      auto result = ds >> layer.secondaryRegionSizeRange;
      if (!result) return std::unexpected(std::move(result.error()));
    }
    {
      auto result = ds >> layer.subRegionSizeRange;
      if (!result) return std::unexpected(std::move(result.error()));
    }
    return {};
  };

  {

    auto result = VisitableWorldParameters::read(ds);

    if (!result) return std::unexpected(std::move(result.error()));

  }
  {
    auto result = ds >> primaryBiome;
    if (!result) return std::unexpected(std::move(result.error()));
  }
  {
    auto result = ds >> primarySurfaceLiquid;
    if (!result) return std::unexpected(std::move(result.error()));
  }
  {
    auto result = ds >> sizeName;
    if (!result) return std::unexpected(std::move(result.error()));
  }
  {
    auto result = ds >> hueShift;
    if (!result) return std::unexpected(std::move(result.error()));
  }
  {
    auto result = ds >> skyColoring;
    if (!result) return std::unexpected(std::move(result.error()));
  }
  {
    auto result = ds >> dayLength;
    if (!result) return std::unexpected(std::move(result.error()));
  }
  {
    auto result = ds >> blendSize;
    if (!result) return std::unexpected(std::move(result.error()));
  }
  {
    auto result = ds >> blockNoiseConfig;
    if (!result) return std::unexpected(std::move(result.error()));
  }
  {
    auto result = ds >> blendNoiseConfig;
    if (!result) return std::unexpected(std::move(result.error()));
  }
  {
    auto result = readTerrestrialLayer(ds, spaceLayer);
    if (!result) return std::unexpected(std::move(result.error()));
  }
  {
    auto result = readTerrestrialLayer(ds, atmosphereLayer);
    if (!result) return std::unexpected(std::move(result.error()));
  }
  {
    auto result = readTerrestrialLayer(ds, surfaceLayer);
    if (!result) return std::unexpected(std::move(result.error()));
  }
  {
    auto result = readTerrestrialLayer(ds, subsurfaceLayer);
    if (!result) return std::unexpected(std::move(result.error()));
  }
  {
    auto result = ds.read_container(undergroundLayers, readTerrestrialLayer);
    if (!result) return std::unexpected(std::move(result.error()));
  }
  {
    auto result = readTerrestrialLayer(ds, coreLayer);
    if (!result) return std::unexpected(std::move(result.error()));
  }
  return {};
}

[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, TerrestrialWorldParameters::TerrestrialRegion const& region) {
  {

    auto result = ds << region.biome;

    if (!result) return std::unexpected(std::move(result.error()));

  }
  {
    auto result = ds << region.blockSelector;
    if (!result) return std::unexpected(std::move(result.error()));
  }
  {
    auto result = ds << region.fgCaveSelector;
    if (!result) return std::unexpected(std::move(result.error()));
  }
  {
    auto result = ds << region.bgCaveSelector;
    if (!result) return std::unexpected(std::move(result.error()));
  }
  {
    auto result = ds << region.fgOreSelector;
    if (!result) return std::unexpected(std::move(result.error()));
  }
  {
    auto result = ds << region.bgOreSelector;
    if (!result) return std::unexpected(std::move(result.error()));
  }
  {
    auto result = ds << region.subBlockSelector;
    if (!result) return std::unexpected(std::move(result.error()));
  }
  {
    auto result = ds << region.caveLiquid;
    if (!result) return std::unexpected(std::move(result.error()));
  }
  {
    auto result = ds << region.caveLiquidSeedDensity;
    if (!result) return std::unexpected(std::move(result.error()));
  }
  {
    auto result = ds << region.oceanLiquid;
    if (!result) return std::unexpected(std::move(result.error()));
  }
  {
    auto result = ds << region.oceanLiquidLevel;
    if (!result) return std::unexpected(std::move(result.error()));
  }
  {
    auto result = ds << region.encloseLiquids;
    if (!result) return std::unexpected(std::move(result.error()));
  }
  {
    auto result = ds << region.fillMicrodungeons;
    if (!result) return std::unexpected(std::move(result.error()));
  }
  return std::ref(ds);
}

std::expected<void, StarException> TerrestrialWorldParameters::write(DataStream& ds) const {
  auto writeTerrestrialLayer = [](DataStream& ds, TerrestrialLayer const& layer) -> std::expected<void, StarException> {
    {
      auto result = ds << layer.layerMinHeight;
      if (!result) return std::unexpected(std::move(result.error()));
    }
    {
      auto result = ds << layer.layerBaseHeight;
      if (!result) return std::unexpected(std::move(result.error()));
    }
    {
      auto result = ds << layer.dungeons;
      if (!result) return std::unexpected(std::move(result.error()));
    }
    {
      auto result = ds << layer.dungeonXVariance;
      if (!result) return std::unexpected(std::move(result.error()));
    }
    {
      auto result = ds << layer.primaryRegion;
      if (!result) return std::unexpected(std::move(result.error()));
    }
    {
      auto result = ds << layer.primarySubRegion;
      if (!result) return std::unexpected(std::move(result.error()));
    }
    {
      auto result = ds << layer.secondaryRegions;
      if (!result) return std::unexpected(std::move(result.error()));
    }
    {
      auto result = ds << layer.secondarySubRegions;
      if (!result) return std::unexpected(std::move(result.error()));
    }
    {
      auto result = ds << layer.secondaryRegionSizeRange;
      if (!result) return std::unexpected(std::move(result.error()));
    }
    {
      auto result = ds << layer.subRegionSizeRange;
      if (!result) return std::unexpected(std::move(result.error()));
    }
    return {};
  };

  {

    auto result = VisitableWorldParameters::write(ds);

    if (!result) return std::unexpected(std::move(result.error()));

  }
  {
    auto result = ds << primaryBiome;
    if (!result) return std::unexpected(std::move(result.error()));
  }
  {
    auto result = ds << primarySurfaceLiquid;
    if (!result) return std::unexpected(std::move(result.error()));
  }
  {
    auto result = ds << sizeName;
    if (!result) return std::unexpected(std::move(result.error()));
  }
  {
    auto result = ds << hueShift;
    if (!result) return std::unexpected(std::move(result.error()));
  }
  {
    auto result = ds << skyColoring;
    if (!result) return std::unexpected(std::move(result.error()));
  }
  {
    auto result = ds << dayLength;
    if (!result) return std::unexpected(std::move(result.error()));
  }
  {
    auto result = ds << blendSize;
    if (!result) return std::unexpected(std::move(result.error()));
  }
  {
    auto result = ds << blockNoiseConfig;
    if (!result) return std::unexpected(std::move(result.error()));
  }
  {
    auto result = ds << blendNoiseConfig;
    if (!result) return std::unexpected(std::move(result.error()));
  }
  {
    auto result = writeTerrestrialLayer(ds, spaceLayer);
    if (!result) return std::unexpected(std::move(result.error()));
  }
  {
    auto result = writeTerrestrialLayer(ds, atmosphereLayer);
    if (!result) return std::unexpected(std::move(result.error()));
  }
  {
    auto result = writeTerrestrialLayer(ds, surfaceLayer);
    if (!result) return std::unexpected(std::move(result.error()));
  }
  {
    auto result = writeTerrestrialLayer(ds, subsurfaceLayer);
    if (!result) return std::unexpected(std::move(result.error()));
  }
  {
    auto result = ds.write_container(undergroundLayers, writeTerrestrialLayer);
    if (!result) return std::unexpected(std::move(result.error()));
  }
  {
    auto result = writeTerrestrialLayer(ds, coreLayer);
    if (!result) return std::unexpected(std::move(result.error()));
  }
  return {};
}

AsteroidsWorldParameters::AsteroidsWorldParameters() {
  airless = true;
}

std::expected<std::shared_ptr<AsteroidsWorldParameters>, StarException> AsteroidsWorldParameters::create(Json const& store) {
  auto result = std::make_shared<AsteroidsWorldParameters>();
  auto base = result->load(store);
  if (!base) return std::unexpected(std::move(base.error()));
  auto asteroidTopLevelValue1 = store.getInt("asteroidTopLevel");
  if (!asteroidTopLevelValue1) return std::unexpected(std::move(asteroidTopLevelValue1.error()));
  result->asteroidTopLevel = static_cast<int>((*asteroidTopLevelValue1));
  auto asteroidBottomLevelValue1 = store.getInt("asteroidBottomLevel");
  if (!asteroidBottomLevelValue1) return std::unexpected(std::move(asteroidBottomLevelValue1.error()));
  result->asteroidBottomLevel = static_cast<int>((*asteroidBottomLevelValue1));
  auto blendSizeValue1 = store.getFloat("blendSize");
  if (!blendSizeValue1) return std::unexpected(std::move(blendSizeValue1.error()));
  result->blendSize = (*blendSizeValue1);
  auto asteroidBiomeValue1 = store.getString("asteroidBiome");
  if (!asteroidBiomeValue1) return std::unexpected(std::move(asteroidBiomeValue1.error()));
  result->asteroidBiome = (*asteroidBiomeValue1);
  auto ambientLightLevelValue1 = store.get("ambientLightLevel");
  if (!ambientLightLevelValue1) return std::unexpected(std::move(ambientLightLevelValue1.error()));
  auto ambientLightLevelValue2 = jsonToColor((*ambientLightLevelValue1));
  if (!ambientLightLevelValue2) return std::unexpected(std::move(ambientLightLevelValue2.error()));
  result->ambientLightLevel = (*ambientLightLevelValue2);
  return result;
}

WorldParametersType AsteroidsWorldParameters::type() const {
  return WorldParametersType::AsteroidsWorldParameters;
}

std::expected<Json, StarException> AsteroidsWorldParameters::store() const {
  auto base = VisitableWorldParameters::store();
  if (!base) return std::unexpected(std::move(base.error()));
  return base->setAll(JsonObject{{"asteroidTopLevel", asteroidTopLevel},
      {"asteroidBottomLevel", asteroidBottomLevel},
      {"blendSize", blendSize},
      {"asteroidBiome", asteroidBiome},
      {"ambientLightLevel", jsonFromColor(ambientLightLevel)}});
}

std::expected<void, StarException> AsteroidsWorldParameters::read(DataStream& ds) {
  {

    auto result = VisitableWorldParameters::read(ds);

    if (!result) return std::unexpected(std::move(result.error()));

  }
  {
    auto result = ds >> asteroidTopLevel;
    if (!result) return std::unexpected(std::move(result.error()));
  }
  {
    auto result = ds >> asteroidBottomLevel;
    if (!result) return std::unexpected(std::move(result.error()));
  }
  {
    auto result = ds >> blendSize;
    if (!result) return std::unexpected(std::move(result.error()));
  }
  {
    auto result = ds >> asteroidBiome;
    if (!result) return std::unexpected(std::move(result.error()));
  }
  {
    auto result = ds >> ambientLightLevel;
    if (!result) return std::unexpected(std::move(result.error()));
  }
  return {};
}

std::expected<void, StarException> AsteroidsWorldParameters::write(DataStream& ds) const {
  {

    auto result = VisitableWorldParameters::write(ds);

    if (!result) return std::unexpected(std::move(result.error()));

  }
  {
    auto result = ds << asteroidTopLevel;
    if (!result) return std::unexpected(std::move(result.error()));
  }
  {
    auto result = ds << asteroidBottomLevel;
    if (!result) return std::unexpected(std::move(result.error()));
  }
  {
    auto result = ds << blendSize;
    if (!result) return std::unexpected(std::move(result.error()));
  }
  {
    auto result = ds << asteroidBiome;
    if (!result) return std::unexpected(std::move(result.error()));
  }
  {
    auto result = ds << ambientLightLevel;
    if (!result) return std::unexpected(std::move(result.error()));
  }
  return {};
}

std::expected<std::shared_ptr<FloatingDungeonWorldParameters>, StarException> FloatingDungeonWorldParameters::create(Json const& store) {
  auto result = std::make_shared<FloatingDungeonWorldParameters>();
  auto base = result->load(store);
  if (!base) return std::unexpected(std::move(base.error()));
  auto dungeonBaseHeightValue1 = store.getInt("dungeonBaseHeight");
  if (!dungeonBaseHeightValue1) return std::unexpected(std::move(dungeonBaseHeightValue1.error()));
  result->dungeonBaseHeight = static_cast<int>((*dungeonBaseHeightValue1));
  auto dungeonSurfaceHeightValue1 = store.getInt("dungeonSurfaceHeight");
  if (!dungeonSurfaceHeightValue1) return std::unexpected(std::move(dungeonSurfaceHeightValue1.error()));
  result->dungeonSurfaceHeight = static_cast<int>((*dungeonSurfaceHeightValue1));
  auto dungeonUndergroundLevelValue1 = store.getInt("dungeonUndergroundLevel");
  if (!dungeonUndergroundLevelValue1) return std::unexpected(std::move(dungeonUndergroundLevelValue1.error()));
  result->dungeonUndergroundLevel = static_cast<int>((*dungeonUndergroundLevelValue1));
  auto primaryDungeonValue1 = store.getString("primaryDungeon");
  if (!primaryDungeonValue1) return std::unexpected(std::move(primaryDungeonValue1.error()));
  result->primaryDungeon = (*primaryDungeonValue1);
  auto biomeValue1 = store.optString("biome");
  if (!biomeValue1) return std::unexpected(std::move(biomeValue1.error()));
  result->biome = (*biomeValue1);
  auto ambientLightLevelValue1 = store.get("ambientLightLevel");
  if (!ambientLightLevelValue1) return std::unexpected(std::move(ambientLightLevelValue1.error()));
  auto ambientLightLevelValue2 = jsonToColor((*ambientLightLevelValue1));
  if (!ambientLightLevelValue2) return std::unexpected(std::move(ambientLightLevelValue2.error()));
  result->ambientLightLevel = (*ambientLightLevelValue2);
  auto dayMusicTrackValue1 = store.optString("dayMusicTrack");
  if (!dayMusicTrackValue1) return std::unexpected(std::move(dayMusicTrackValue1.error()));
  result->dayMusicTrack = (*dayMusicTrackValue1);
  auto nightMusicTrackValue1 = store.optString("nightMusicTrack");
  if (!nightMusicTrackValue1) return std::unexpected(std::move(nightMusicTrackValue1.error()));
  result->nightMusicTrack = (*nightMusicTrackValue1);
  auto dayAmbientNoisesValue1 = store.optString("dayAmbientNoises");
  if (!dayAmbientNoisesValue1) return std::unexpected(std::move(dayAmbientNoisesValue1.error()));
  result->dayAmbientNoises = (*dayAmbientNoisesValue1);
  auto nightAmbientNoisesValue1 = store.optString("nightAmbientNoises");
  if (!nightAmbientNoisesValue1) return std::unexpected(std::move(nightAmbientNoisesValue1.error()));
  result->nightAmbientNoises = (*nightAmbientNoisesValue1);
  return result;
}

WorldParametersType FloatingDungeonWorldParameters::type() const {
  return WorldParametersType::FloatingDungeonWorldParameters;
}

std::expected<Json, StarException> FloatingDungeonWorldParameters::store() const {
  auto base = VisitableWorldParameters::store();
  if (!base) return std::unexpected(std::move(base.error()));
  return base->setAll(JsonObject{{"dungeonBaseHeight", dungeonBaseHeight},
      {"dungeonSurfaceHeight", dungeonSurfaceHeight},
      {"dungeonUndergroundLevel", dungeonUndergroundLevel},
      {"primaryDungeon", primaryDungeon},
      {"biome", jsonFromOptional(biome)},
      {"ambientLightLevel", jsonFromColor(ambientLightLevel)},
      {"dayMusicTrack", jsonFromOptional(dayMusicTrack)},
      {"nightMusicTrack", jsonFromOptional(nightMusicTrack)},
      {"dayAmbientNoises", jsonFromOptional(dayAmbientNoises)},
      {"nightAmbientNoises", jsonFromOptional(nightAmbientNoises)}});
}

std::expected<void, StarException> FloatingDungeonWorldParameters::read(DataStream& ds) {
  {

    auto result = VisitableWorldParameters::read(ds);

    if (!result) return std::unexpected(std::move(result.error()));

  }
  {
    auto result = ds >> dungeonBaseHeight;
    if (!result) return std::unexpected(std::move(result.error()));
  }
  {
    auto result = ds >> dungeonSurfaceHeight;
    if (!result) return std::unexpected(std::move(result.error()));
  }
  {
    auto result = ds >> dungeonUndergroundLevel;
    if (!result) return std::unexpected(std::move(result.error()));
  }
  {
    auto result = ds >> primaryDungeon;
    if (!result) return std::unexpected(std::move(result.error()));
  }
  {
    auto result = ds >> biome;
    if (!result) return std::unexpected(std::move(result.error()));
  }
  {
    auto result = ds >> ambientLightLevel;
    if (!result) return std::unexpected(std::move(result.error()));
  }
  {
    auto result = ds >> dayMusicTrack;
    if (!result) return std::unexpected(std::move(result.error()));
  }
  {
    auto result = ds >> nightMusicTrack;
    if (!result) return std::unexpected(std::move(result.error()));
  }
  {
    auto result = ds >> dayAmbientNoises;
    if (!result) return std::unexpected(std::move(result.error()));
  }
  {
    auto result = ds >> nightAmbientNoises;
    if (!result) return std::unexpected(std::move(result.error()));
  }
  return {};
}

std::expected<void, StarException> FloatingDungeonWorldParameters::write(DataStream& ds) const {
  {

    auto result = VisitableWorldParameters::write(ds);

    if (!result) return std::unexpected(std::move(result.error()));

  }
  {
    auto result = ds << dungeonBaseHeight;
    if (!result) return std::unexpected(std::move(result.error()));
  }
  {
    auto result = ds << dungeonSurfaceHeight;
    if (!result) return std::unexpected(std::move(result.error()));
  }
  {
    auto result = ds << dungeonUndergroundLevel;
    if (!result) return std::unexpected(std::move(result.error()));
  }
  {
    auto result = ds << primaryDungeon;
    if (!result) return std::unexpected(std::move(result.error()));
  }
  {
    auto result = ds << biome;
    if (!result) return std::unexpected(std::move(result.error()));
  }
  {
    auto result = ds << ambientLightLevel;
    if (!result) return std::unexpected(std::move(result.error()));
  }
  {
    auto result = ds << dayMusicTrack;
    if (!result) return std::unexpected(std::move(result.error()));
  }
  {
    auto result = ds << nightMusicTrack;
    if (!result) return std::unexpected(std::move(result.error()));
  }
  {
    auto result = ds << dayAmbientNoises;
    if (!result) return std::unexpected(std::move(result.error()));
  }
  {
    auto result = ds << nightAmbientNoises;
    if (!result) return std::unexpected(std::move(result.error()));
  }
  return {};
}

std::expected<Json, StarException> diskStoreVisitableWorldParameters(std::shared_ptr<VisitableWorldParameters const> const& parameters) {
  if (!parameters)
    return {};

  auto stored = parameters->store();
  if (!stored) return std::unexpected(std::move(stored.error()));
  if (!WorldParametersTypeNames) return std::unexpected(WorldParametersTypeNames.error());
  auto type = WorldParametersTypeNames->get_right(parameters->type());
  if (!type) return std::unexpected(std::move(type.error()));
  return stored->setAll({{"type", type->get()}});
}

std::expected<std::shared_ptr<VisitableWorldParameters>, StarException> diskLoadVisitableWorldParameters(Json const& store) {
  if (store.isNull())
    return {};

  auto typeName = store.getString("type");
  if (!typeName) return std::unexpected(std::move(typeName.error()));
  if (!WorldParametersTypeNames) return std::unexpected(WorldParametersTypeNames.error());
  auto parsedType = WorldParametersTypeNames->get_left(*typeName);
  if (!parsedType) return std::unexpected(std::move(parsedType.error()));
  auto type = parsedType->get();
  if (type == WorldParametersType::TerrestrialWorldParameters)
    return TerrestrialWorldParameters::create(store).transform([](auto parameters) -> std::shared_ptr<VisitableWorldParameters> { return parameters; });
  else if (type == WorldParametersType::AsteroidsWorldParameters)
    return AsteroidsWorldParameters::create(store).transform([](auto parameters) -> std::shared_ptr<VisitableWorldParameters> { return parameters; });
  else if (type == WorldParametersType::FloatingDungeonWorldParameters)
    return FloatingDungeonWorldParameters::create(store).transform([](auto parameters) -> std::shared_ptr<VisitableWorldParameters> { return parameters; });
  return std::unexpected(StarException("No such WorldParametersType"));
}

std::expected<ByteArray, StarException> netStoreVisitableWorldParameters(std::shared_ptr<VisitableWorldParameters const> const& parameters) {
  if (!parameters)
    return {};

  DataStreamBuffer ds;
  auto type = ds.write(parameters->type());
  if (!type) return std::unexpected(std::move(type.error()));
  auto written = parameters->write(ds);
  if (!written) return std::unexpected(std::move(written.error()));
  return ds.take_data();
}

std::expected<std::shared_ptr<VisitableWorldParameters>, StarException> netLoadVisitableWorldParameters(ByteArray data) {
  if (data.empty())
    return {};

  DataStreamBuffer ds(std::move(data));
  auto parsedType = ds.read<WorldParametersType>();
  if (!parsedType) return std::unexpected(std::move(parsedType.error()));
  auto type = *parsedType;

  std::shared_ptr<VisitableWorldParameters> parameters;
  if (type == WorldParametersType::TerrestrialWorldParameters)
    parameters = std::make_shared<TerrestrialWorldParameters>();
  else if (type == WorldParametersType::AsteroidsWorldParameters)
    parameters = std::make_shared<AsteroidsWorldParameters>();
  else if (type == WorldParametersType::FloatingDungeonWorldParameters)
    parameters = std::make_shared<FloatingDungeonWorldParameters>();
  else
    return std::unexpected(StarException("No such WorldParametersType"));

  auto read = parameters->read(ds);
  if (!read) return std::unexpected(std::move(read.error()));

  return parameters;
}

std::expected<std::shared_ptr<TerrestrialWorldParameters>, StarException> generateTerrestrialWorldParameters(Root& root, String const& typeName, String const& sizeName, std::uint64_t seed) {
  auto autoassetsResult1 = root.assets();
  if (!autoassetsResult1) return std::unexpected(std::move(autoassetsResult1.error()));
  auto assets = std::move((*autoassetsResult1));
  auto autoliquidsDatabaseResult1 = root.liquidsDatabase();
  if (!autoliquidsDatabaseResult1) return std::unexpected(std::move(autoliquidsDatabaseResult1.error()));
  auto liquidsDatabase = std::move((*autoliquidsDatabaseResult1));
  auto autobiomeDatabaseResult1 = root.biomeDatabase();
  if (!autobiomeDatabaseResult1) return std::unexpected(std::move(autobiomeDatabaseResult1.error()));
  auto biomeDatabase = std::move((*autobiomeDatabaseResult1));
  auto autoterrestrialConfigResult1 = assets->json("/terrestrial_worlds.config");
  if (!autoterrestrialConfigResult1) return std::unexpected(std::move(autoterrestrialConfigResult1.error()));
  auto terrestrialConfig = std::move((*autoterrestrialConfigResult1));
  auto autoregionDefaultsResult1 = terrestrialConfig.get("regionDefaults");
  if (!autoregionDefaultsResult1) return std::unexpected(std::move(autoregionDefaultsResult1.error()));
  auto regionDefaults = std::move((*autoregionDefaultsResult1));
  auto autoregionTypesResult1 = terrestrialConfig.get("regionTypes");
  if (!autoregionTypesResult1) return std::unexpected(std::move(autoregionTypesResult1.error()));
  auto regionTypes = std::move((*autoregionTypesResult1));
  auto autobaseConfigResult1 = terrestrialConfig.get("planetDefaults");
  if (!autobaseConfigResult1) return std::unexpected(std::move(autobaseConfigResult1.error()));
  auto baseConfig = std::move((*autobaseConfigResult1));
  auto autosizeConfigResult1 = terrestrialConfig.get("planetSizes");
  if (!autosizeConfigResult1) return std::unexpected(std::move(autosizeConfigResult1.error()));
  auto autosizeConfigResult2 = (*autosizeConfigResult1).get(sizeName);
  if (!autosizeConfigResult2) return std::unexpected(std::move(autosizeConfigResult2.error()));
  auto sizeConfig = std::move((*autosizeConfigResult2));
  auto autotypeConfigResult1 = terrestrialConfig.get("planetTypes");
  if (!autotypeConfigResult1) return std::unexpected(std::move(autotypeConfigResult1.error()));
  auto autotypeConfigResult2 = (*autotypeConfigResult1).get(typeName);
  if (!autotypeConfigResult2) return std::unexpected(std::move(autotypeConfigResult2.error()));
  auto typeConfig = std::move((*autotypeConfigResult2));
  auto config = jsonMerge(baseConfig, sizeConfig, typeConfig);
  auto autogravityRangeResult1 = config.get("gravityRange");
  if (!autogravityRangeResult1) return std::unexpected(std::move(autogravityRangeResult1.error()));
  auto autogravityRangeResult2 = jsonToVec2F((*autogravityRangeResult1));
  if (!autogravityRangeResult2) return std::unexpected(std::move(autogravityRangeResult2.error()));
  auto gravityRange = std::move((*autogravityRangeResult2));
  auto autodayLengthRangeResult1 = config.get("dayLengthRange");
  if (!autodayLengthRangeResult1) return std::unexpected(std::move(autodayLengthRangeResult1.error()));
  auto autodayLengthRangeResult2 = jsonToVec2F((*autodayLengthRangeResult1));
  if (!autodayLengthRangeResult2) return std::unexpected(std::move(autodayLengthRangeResult2.error()));
  auto dayLengthRange = std::move((*autodayLengthRangeResult2));
  auto autothreatLevelRangeResult1 = config.query("threatRange");
  if (!autothreatLevelRangeResult1) return std::unexpected(std::move(autothreatLevelRangeResult1.error()));
  auto autothreatLevelRangeResult2 = jsonToVec2F((*autothreatLevelRangeResult1));
  if (!autothreatLevelRangeResult2) return std::unexpected(std::move(autothreatLevelRangeResult2.error()));
  auto threatLevelRange = std::move((*autothreatLevelRangeResult2));
  auto threatLevel = std::move(static_cast<float>(
    static_random_double(seed, "ThreatLevel") * (threatLevelRange[1] - threatLevelRange[0]) + threatLevelRange[0]));
  auto surfaceBiomeSeed = std::move(static_random_u64(seed, "SurfaceBiomeSeed"));

  auto readRegion = [liquidsDatabase, threatLevel, seed](Json const& regionConfig, String const& layerName, int layerBaseHeight) -> std::expected<TerrestrialWorldParameters::TerrestrialRegion, StarException> {
    auto layerNameSeed = std::string_view(layerName.utf8());
    // These seeds historically hashed C text, stopping at the first NUL byte.
    layerNameSeed = layerNameSeed.substr(0, layerNameSeed.find('\0'));
    TerrestrialWorldParameters::TerrestrialRegion region;
    auto autobiomeChoicesResult1 = regionConfig.get("biome");
    if (!autobiomeChoicesResult1) return std::unexpected(std::move(autobiomeChoicesResult1.error()));
    auto autobiomeChoicesResult2 = binnedChoiceFromJson((*autobiomeChoicesResult1), threatLevel);
    if (!autobiomeChoicesResult2) return std::unexpected(std::move(autobiomeChoicesResult2.error()));
    auto autobiomeChoicesResult3 = jsonToStringList((*autobiomeChoicesResult2));
    if (!autobiomeChoicesResult3) return std::unexpected(std::move(autobiomeChoicesResult3.error()));
    auto biomeChoices = std::move((*autobiomeChoicesResult3));
    auto selected_biome = static_random_value_from(biomeChoices, seed, layerNameSeed);
    region.biome = std::move(selected_biome);
    auto block_selector_choices = regionConfig.getArray("blockSelector");
    if (!block_selector_choices)
      return std::unexpected(std::move(block_selector_choices.error()));
    auto block_selector_choice = static_random_from((*block_selector_choices), seed, "blockSelector", layerNameSeed);
    if (!block_selector_choice)
      return std::unexpected(std::move(block_selector_choice.error()));
    auto block_selector_name = block_selector_choice->get().toString();
    if (!block_selector_name)
      return std::unexpected(std::move(block_selector_name.error()));
    region.blockSelector = std::move((*block_selector_name));
    auto foreground_cave_selector_choices = regionConfig.getArray("fgCaveSelector");
    if (!foreground_cave_selector_choices)
      return std::unexpected(std::move(foreground_cave_selector_choices.error()));
    auto foreground_cave_selector_choice =
      static_random_from((*foreground_cave_selector_choices), seed, "fgCaveSelector", layerNameSeed);
    if (!foreground_cave_selector_choice)
      return std::unexpected(std::move(foreground_cave_selector_choice.error()));
    auto foreground_cave_selector_name = foreground_cave_selector_choice->get().toString();
    if (!foreground_cave_selector_name)
      return std::unexpected(std::move(foreground_cave_selector_name.error()));
    region.fgCaveSelector = std::move((*foreground_cave_selector_name));
    auto background_cave_selector_choices = regionConfig.getArray("bgCaveSelector");
    if (!background_cave_selector_choices)
      return std::unexpected(std::move(background_cave_selector_choices.error()));
    auto background_cave_selector_choice =
      static_random_from((*background_cave_selector_choices), seed, "bgCaveSelector", layerNameSeed);
    if (!background_cave_selector_choice)
      return std::unexpected(std::move(background_cave_selector_choice.error()));
    auto background_cave_selector_name = background_cave_selector_choice->get().toString();
    if (!background_cave_selector_name)
      return std::unexpected(std::move(background_cave_selector_name.error()));
    region.bgCaveSelector = std::move((*background_cave_selector_name));
    auto foreground_ore_selector_choices = regionConfig.getArray("fgOreSelector");
    if (!foreground_ore_selector_choices)
      return std::unexpected(std::move(foreground_ore_selector_choices.error()));
    auto foreground_ore_selector_choice =
      static_random_from((*foreground_ore_selector_choices), seed, "fgOreSelector", layerNameSeed);
    if (!foreground_ore_selector_choice)
      return std::unexpected(std::move(foreground_ore_selector_choice.error()));
    auto foreground_ore_selector_name = foreground_ore_selector_choice->get().toString();
    if (!foreground_ore_selector_name)
      return std::unexpected(std::move(foreground_ore_selector_name.error()));
    region.fgOreSelector = std::move((*foreground_ore_selector_name));
    auto background_ore_selector_choices = regionConfig.getArray("bgOreSelector");
    if (!background_ore_selector_choices)
      return std::unexpected(std::move(background_ore_selector_choices.error()));
    auto background_ore_selector_choice =
      static_random_from((*background_ore_selector_choices), seed, "bgOreSelector", layerNameSeed);
    if (!background_ore_selector_choice)
      return std::unexpected(std::move(background_ore_selector_choice.error()));
    auto background_ore_selector_name = background_ore_selector_choice->get().toString();
    if (!background_ore_selector_name)
      return std::unexpected(std::move(background_ore_selector_name.error()));
    region.bgOreSelector = std::move((*background_ore_selector_name));
    auto sub_block_selector_choices = regionConfig.getArray("subBlockSelector");
    if (!sub_block_selector_choices)
      return std::unexpected(std::move(sub_block_selector_choices.error()));
    auto sub_block_selector_choice =
      static_random_from((*sub_block_selector_choices), seed, "subBlockSelector", layerNameSeed);
    if (!sub_block_selector_choice)
      return std::unexpected(std::move(sub_block_selector_choice.error()));
    auto sub_block_selector_name = sub_block_selector_choice->get().toString();
    if (!sub_block_selector_name)
      return std::unexpected(std::move(sub_block_selector_name.error()));
    region.subBlockSelector = std::move((*sub_block_selector_name));
    auto cave_liquid_choices = regionConfig.getArray("caveLiquid", {});
    if (!cave_liquid_choices)
      return std::unexpected(std::move(cave_liquid_choices.error()));
    auto selected_cave_liquid = static_random_value_from((*cave_liquid_choices), seed, "caveLiquid");
    auto cave_liquid_name = selected_cave_liquid.optString();
    if (!cave_liquid_name)
      return std::unexpected(std::move(cave_liquid_name.error()));
    auto caveLiquid = std::move((*cave_liquid_name));
    if (caveLiquid) {
      auto autocaveLiquidSeedDensityRangeResult1 = regionConfig.get("caveLiquidSeedDensityRange");
      if (!autocaveLiquidSeedDensityRangeResult1) return std::unexpected(std::move(autocaveLiquidSeedDensityRangeResult1.error()));
      auto autocaveLiquidSeedDensityRangeResult2 = jsonToVec2F((*autocaveLiquidSeedDensityRangeResult1));
      if (!autocaveLiquidSeedDensityRangeResult2) return std::unexpected(std::move(autocaveLiquidSeedDensityRangeResult2.error()));
      auto caveLiquidSeedDensityRange = std::move((*autocaveLiquidSeedDensityRangeResult2));
      auto regioncaveLiquidResult1 = liquidsDatabase->liquidId(*caveLiquid);
      if (!regioncaveLiquidResult1) return std::unexpected(std::move(regioncaveLiquidResult1.error()));
      region.caveLiquid = std::move((*regioncaveLiquidResult1));
      auto cave_liquid_density = static_random_float_range(caveLiquidSeedDensityRange[0], caveLiquidSeedDensityRange[1],
                                                           seed, "caveLiquidSeedDensity", layerNameSeed);
      region.caveLiquidSeedDensity = std::move(cave_liquid_density);
    } else {
      region.caveLiquid = EmptyLiquidId;
      region.caveLiquidSeedDensity = 0.0f;
    }
    auto ocean_liquid_choices = regionConfig.getArray("oceanLiquid", {});
    if (!ocean_liquid_choices)
      return std::unexpected(std::move(ocean_liquid_choices.error()));
    auto selected_ocean_liquid = static_random_value_from((*ocean_liquid_choices), seed, "oceanLiquid", layerNameSeed);
    auto ocean_liquid_name = selected_ocean_liquid.optString();
    if (!ocean_liquid_name)
      return std::unexpected(std::move(ocean_liquid_name.error()));
    auto oceanLiquid = std::move((*ocean_liquid_name));
    if (oceanLiquid) {
      auto regionoceanLiquidResult1 = liquidsDatabase->liquidId(*oceanLiquid);
      if (!regionoceanLiquidResult1) return std::unexpected(std::move(regionoceanLiquidResult1.error()));
      region.oceanLiquid = std::move((*regionoceanLiquidResult1));
      auto regionoceanLiquidLevelResult1 = regionConfig.getInt("oceanLevelOffset", 0);
      if (!regionoceanLiquidLevelResult1) return std::unexpected(std::move(regionoceanLiquidLevelResult1.error()));
      region.oceanLiquidLevel = std::move(static_cast<int>((*regionoceanLiquidLevelResult1) + layerBaseHeight));
    } else {
      region.oceanLiquid = EmptyLiquidId;
      region.oceanLiquidLevel = 0;
    }
    auto regionencloseLiquidsResult1 = regionConfig.getBool("encloseLiquids", false);
    if (!regionencloseLiquidsResult1) return std::unexpected(std::move(regionencloseLiquidsResult1.error()));
    region.encloseLiquids = std::move((*regionencloseLiquidsResult1));
    auto regionfillMicrodungeonsResult1 = regionConfig.getBool("fillMicrodungeons", false);
    if (!regionfillMicrodungeonsResult1) return std::unexpected(std::move(regionfillMicrodungeonsResult1.error()));
    region.fillMicrodungeons = std::move((*regionfillMicrodungeonsResult1));
    return region;
  };

  auto readLayer = [readRegion, regionDefaults, regionTypes, seed, config](String const& layerName) -> std::expected<std::optional<TerrestrialWorldParameters::TerrestrialLayer>, StarException> {
    auto autocontainsLayerResult1 = config.get("layers");
    if (!autocontainsLayerResult1) return std::unexpected(std::move(autocontainsLayerResult1.error()));
    auto autocontainsLayerResult2 = (*autocontainsLayerResult1).contains(layerName);
    if (!autocontainsLayerResult2) return std::unexpected(std::move(autocontainsLayerResult2.error()));
    auto containsLayer = std::move((*autocontainsLayerResult2));
    if (!containsLayer) return {};
    auto autolayerConfigResult1 = config.get("layerDefaults");
    if (!autolayerConfigResult1) return std::unexpected(std::move(autolayerConfigResult1.error()));
    auto autolayerConfigResult2 = config.get("layers");
    if (!autolayerConfigResult2) return std::unexpected(std::move(autolayerConfigResult2.error()));
    auto autolayerConfigResult3 = (*autolayerConfigResult2).get(layerName);
    if (!autolayerConfigResult3) return std::unexpected(std::move(autolayerConfigResult3.error()));
    auto layerConfig = jsonMerge((*autolayerConfigResult1), (*autolayerConfigResult3));
    if (!layerConfig) return {};
    auto autoenabledResult1 = layerConfig.getBool("enabled");
    if (!autoenabledResult1) return std::unexpected(std::move(autoenabledResult1.error()));
    auto enabled = std::move((*autoenabledResult1));
    if (!enabled) return {};
    TerrestrialWorldParameters::TerrestrialLayer layer;
    auto layerlayerMinHeightResult1 = layerConfig.getFloat("layerLevel");
    if (!layerlayerMinHeightResult1) return std::unexpected(std::move(layerlayerMinHeightResult1.error()));
    layer.layerMinHeight = std::move(static_cast<int>((*layerlayerMinHeightResult1)));
    auto layerlayerBaseHeightResult1 = layerConfig.getFloat("baseHeight");
    if (!layerlayerBaseHeightResult1) return std::unexpected(std::move(layerlayerBaseHeightResult1.error()));
    layer.layerBaseHeight = std::move(static_cast<int>((*layerlayerBaseHeightResult1)));
    auto autoprimaryRegionListResult1 = layerConfig.getArray("primaryRegion");
    if (!autoprimaryRegionListResult1) return std::unexpected(std::move(autoprimaryRegionListResult1.error()));
    auto primaryRegionList = std::move((*autoprimaryRegionListResult1));
    auto layerNameSeed = std::string_view(layerName.utf8());
    // Match the historical C-text prefix without changing full-String seeds below.
    layerNameSeed = layerNameSeed.substr(0, layerNameSeed.find('\0'));
    auto primary_region_name = static_random_from(primaryRegionList, seed, layerNameSeed, "PrimaryRegionSelection");
    if (!primary_region_name)
      return std::unexpected(std::move(primary_region_name.error()));
    auto primary_region_configuration_name = primary_region_name->get().toString();
    if (!primary_region_configuration_name)
      return std::unexpected(std::move(primary_region_configuration_name.error()));
    auto primaryRegionConfigName = std::move((*primary_region_configuration_name));
    auto JsonprimaryRegionConfigResult1 = regionTypes.get(primaryRegionConfigName);
    if (!JsonprimaryRegionConfigResult1) return std::unexpected(std::move(JsonprimaryRegionConfigResult1.error()));
    Json primaryRegionConfig = jsonMerge(regionDefaults, (*JsonprimaryRegionConfigResult1));
    auto layerprimaryRegionResult1 = readRegion(primaryRegionConfig, layerName, layer.layerBaseHeight);
    if (!layerprimaryRegionResult1) return std::unexpected(std::move(layerprimaryRegionResult1.error()));
    layer.primaryRegion = std::move((*layerprimaryRegionResult1));
    {
      auto autosubRegionListResult1 = primaryRegionConfig.getArray("subRegion");
      if (!autosubRegionListResult1) return std::unexpected(std::move(autosubRegionListResult1.error()));
      auto subRegionList = std::move((*autosubRegionListResult1));
      Json subRegionConfig;
      if (!subRegionList.empty()) {
        auto subregion_choice = static_random_from(subRegionList, seed, layerName, primaryRegionConfigName);
        if (!subregion_choice)
          return std::unexpected(std::move(subregion_choice.error()));
        auto subregion_name = subregion_choice->get().toString();
        if (!subregion_name)
          return std::unexpected(std::move(subregion_name.error()));
        String subRegionName = std::move((*subregion_name));
        auto subRegionConfigResult1 = regionTypes.get(subRegionName);
        if (!subRegionConfigResult1) return std::unexpected(std::move(subRegionConfigResult1.error()));
        subRegionConfig = jsonMerge(regionDefaults, (*subRegionConfigResult1));
      } else {
        subRegionConfig = primaryRegionConfig;
      }
      auto layerprimarySubRegionResult1 = readRegion(subRegionConfig, layerName, layer.layerBaseHeight);
      if (!layerprimarySubRegionResult1) return std::unexpected(std::move(layerprimarySubRegionResult1.error()));
      layer.primarySubRegion = std::move((*layerprimarySubRegionResult1));
    }
    auto Vec2UsecondaryRegionCountRangeResult1 = layerConfig.get("secondaryRegionCount");
    if (!Vec2UsecondaryRegionCountRangeResult1) return std::unexpected(std::move(Vec2UsecondaryRegionCountRangeResult1.error()));
    auto Vec2UsecondaryRegionCountRangeResult2 = jsonToVec2U((*Vec2UsecondaryRegionCountRangeResult1));
    if (!Vec2UsecondaryRegionCountRangeResult2) return std::unexpected(std::move(Vec2UsecondaryRegionCountRangeResult2.error()));
    Vec2U secondaryRegionCountRange = std::move((*Vec2UsecondaryRegionCountRangeResult2));
    auto secondary_region_count =
      static_random_i32_range(static_cast<int>(secondaryRegionCountRange[0]),
                              static_cast<int>(secondaryRegionCountRange[1]), seed, layerName, "SecondaryRegionCount");
    int secondaryRegionCount = std::move(secondary_region_count);
    auto autosecondaryRegionListResult1 = layerConfig.getArray("secondaryRegions");
    if (!autosecondaryRegionListResult1) return std::unexpected(std::move(autosecondaryRegionListResult1.error()));
    auto secondaryRegionList = std::move((*autosecondaryRegionListResult1));
    if (!secondaryRegionList.empty()) {
      static_random_shuffle(secondaryRegionList, seed, layerName, "SecondaryRegionShuffle");
      for (auto const& regionName : secondaryRegionList) {
        if (secondaryRegionCount <= 0) break;
        auto JsonsecondaryRegionConfigResult1 = regionName.toString();
        if (!JsonsecondaryRegionConfigResult1) return std::unexpected(std::move(JsonsecondaryRegionConfigResult1.error()));
        auto JsonsecondaryRegionConfigResult2 = regionTypes.get((*JsonsecondaryRegionConfigResult1));
        if (!JsonsecondaryRegionConfigResult2) return std::unexpected(std::move(JsonsecondaryRegionConfigResult2.error()));
        Json secondaryRegionConfig = jsonMerge(regionDefaults, (*JsonsecondaryRegionConfigResult2));
        auto autoregionResult1 = readRegion(secondaryRegionConfig, layerName, layer.layerBaseHeight);
        if (!autoregionResult1) return std::unexpected(std::move(autoregionResult1.error()));
        auto region = std::move((*autoregionResult1));
        layer.secondaryRegions.push_back(std::move(region));
        auto autosubRegionListResult1 = secondaryRegionConfig.getArray("subRegion");
        if (!autosubRegionListResult1) return std::unexpected(std::move(autosubRegionListResult1.error()));
        auto subRegionList = std::move((*autosubRegionListResult1));
        Json subRegionConfig;
        if (!subRegionList.empty()) {
          auto subregion_choice = regionName.toString();
          if (!subregion_choice)
            return std::unexpected(std::move(subregion_choice.error()));
          auto subregion_name = static_random_from(subRegionList, seed, layerName, (*subregion_choice));
          if (!subregion_name)
            return std::unexpected(std::move(subregion_name.error()));
          auto selected_subregion_name = subregion_name->get().toString();
          if (!selected_subregion_name)
            return std::unexpected(std::move(selected_subregion_name.error()));
          String subRegionName = std::move((*selected_subregion_name));
          auto subRegionConfigResult1 = regionTypes.get(subRegionName);
          if (!subRegionConfigResult1) return std::unexpected(std::move(subRegionConfigResult1.error()));
          subRegionConfig = jsonMerge(regionDefaults, (*subRegionConfigResult1));
        } else {
          subRegionConfig = secondaryRegionConfig;
        }
        auto autosubRegionResult1 = readRegion(subRegionConfig, layerName, layer.layerBaseHeight);
        if (!autosubRegionResult1) return std::unexpected(std::move(autosubRegionResult1.error()));
        auto subRegion = std::move((*autosubRegionResult1));
        layer.secondarySubRegions.push_back(std::move(subRegion));
        --secondaryRegionCount;
      }
    }
    auto layersecondaryRegionSizeRangeResult1 = layerConfig.get("secondaryRegionSize");
    if (!layersecondaryRegionSizeRangeResult1) return std::unexpected(std::move(layersecondaryRegionSizeRangeResult1.error()));
    auto layersecondaryRegionSizeRangeResult2 = jsonToVec2F((*layersecondaryRegionSizeRangeResult1));
    if (!layersecondaryRegionSizeRangeResult2) return std::unexpected(std::move(layersecondaryRegionSizeRangeResult2.error()));
    layer.secondaryRegionSizeRange = std::move((*layersecondaryRegionSizeRangeResult2));
    auto layersubRegionSizeRangeResult1 = layerConfig.get("subRegionSize");
    if (!layersubRegionSizeRangeResult1) return std::unexpected(std::move(layersubRegionSizeRangeResult1.error()));
    auto layersubRegionSizeRangeResult2 = jsonToVec2F((*layersubRegionSizeRangeResult1));
    if (!layersubRegionSizeRangeResult2) return std::unexpected(std::move(layersubRegionSizeRangeResult2.error()));
    layer.subRegionSizeRange = std::move((*layersubRegionSizeRangeResult2));
    auto WeightedPoolStringdungeonPoolResult1 = layerConfig.get("dungeons");
    if (!WeightedPoolStringdungeonPoolResult1) return std::unexpected(std::move(WeightedPoolStringdungeonPoolResult1.error()));
    auto WeightedPoolStringdungeonPoolResult2 = jsonToWeightedPool<String>((*WeightedPoolStringdungeonPoolResult1));
    if (!WeightedPoolStringdungeonPoolResult2) return std::unexpected(std::move(WeightedPoolStringdungeonPoolResult2.error()));
    WeightedPool<String> dungeonPool = std::move((*WeightedPoolStringdungeonPoolResult2));
    Vec2U dungeonCountRange{};
    auto rangeJson = layerConfig.opt("dungeonCountRange");
    if (!rangeJson) return std::unexpected(std::move(rangeJson.error()));
    if (*rangeJson) {
      auto dungeonCountRangeResult1 = jsonToVec2U(**rangeJson);
      if (!dungeonCountRangeResult1) return std::unexpected(std::move(dungeonCountRangeResult1.error()));
      dungeonCountRange = std::move((*dungeonCountRangeResult1));
    }
    auto dungeon_count =
      static_random_u32_range(dungeonCountRange[0], dungeonCountRange[1], seed, layerName, "DungeonCount");
    unsigned dungeonCount = std::move(dungeon_count);
    layer.dungeons.append_range(
      dungeonPool.selectUniques(dungeonCount, static_random_hash64(seed, layerName, "DungeonChoice")));
    auto layerdungeonXVarianceResult1 = layerConfig.getInt("dungeonXVariance", 0);
    if (!layerdungeonXVarianceResult1) return std::unexpected(std::move(layerdungeonXVarianceResult1.error()));
    layer.dungeonXVariance = std::move(static_cast<int>((*layerdungeonXVarianceResult1)));
    return layer;
  };

  auto autosurfaceLayerOptionalResult1 = readLayer("surface");
  if (!autosurfaceLayerOptionalResult1) return std::unexpected(std::move(autosurfaceLayerOptionalResult1.error()));
  auto surfaceLayerOptional = std::move((*autosurfaceLayerOptionalResult1));
  if (!surfaceLayerOptional) return std::unexpected(StarException("Required terrestrial layer surface is missing or disabled"));
  auto surfaceLayer = std::move(*surfaceLayerOptional);
  String primaryBiome = surfaceLayer.primaryRegion.biome;
  auto parameters = std::make_shared<TerrestrialWorldParameters>();
  parameters->threatLevel = std::move(threatLevel);
  parameters->typeName = std::move(typeName);
  auto parametersworldSizeResult1 = config.get("size");
  if (!parametersworldSizeResult1) return std::unexpected(std::move(parametersworldSizeResult1.error()));
  auto parametersworldSizeResult2 = jsonToVec2U((*parametersworldSizeResult1));
  if (!parametersworldSizeResult2) return std::unexpected(std::move(parametersworldSizeResult2.error()));
  parameters->worldSize = std::move((*parametersworldSizeResult2));
  auto gravity_value = static_random_float_range(gravityRange[0], gravityRange[1], seed, "WorldGravity");
  parameters->gravity = std::move(gravity_value);
  auto parametersairlessResult1 = biomeDatabase->biomeIsAirless(primaryBiome);
  if (!parametersairlessResult1) return std::unexpected(std::move(parametersairlessResult1.error()));
  parameters->airless = std::move((*parametersairlessResult1));
  auto parametersenvironmentStatusEffectsResult1 = biomeDatabase->biomeStatusEffects(primaryBiome);
  if (!parametersenvironmentStatusEffectsResult1) return std::unexpected(std::move(parametersenvironmentStatusEffectsResult1.error()));
  parameters->environmentStatusEffects = std::move((*parametersenvironmentStatusEffectsResult1));
  auto parametersoverrideTechJson = config.opt("overrideTech");
  if (!parametersoverrideTechJson) return std::unexpected(std::move(parametersoverrideTechJson.error()));
  if (*parametersoverrideTechJson) {
    auto value = jsonToStringList(**parametersoverrideTechJson);
    if (!value) return std::unexpected(std::move(value.error()));
    parameters->overrideTech = std::move(*value);
  }
  auto parametersglobalDirectivesJson = config.opt("globalDirectives");
  if (!parametersglobalDirectivesJson) return std::unexpected(std::move(parametersglobalDirectivesJson.error()));
  if (*parametersglobalDirectivesJson) {
    auto value = jsonToDirectivesList(**parametersglobalDirectivesJson);
    if (!value) return std::unexpected(std::move(value.error()));
    parameters->globalDirectives = std::move(*value);
  }
  auto beam_rule_name = config.getString("beamUpRule", "Surface");
  if (!beam_rule_name)
    return std::unexpected(std::move(beam_rule_name.error()));
  if (!BeamUpRuleNames) return std::unexpected(BeamUpRuleNames.error());
  auto beam_rule = BeamUpRuleNames->get_left((*beam_rule_name));
  if (!beam_rule)
    return std::unexpected(std::move(beam_rule.error()));
  parameters->beamUpRule = beam_rule->get();
  auto parametersdisableDeathDropsResult1 = config.getBool("disableDeathDrops", false);
  if (!parametersdisableDeathDropsResult1) return std::unexpected(std::move(parametersdisableDeathDropsResult1.error()));
  parameters->disableDeathDrops = std::move((*parametersdisableDeathDropsResult1));
  auto edge_region_name = config.getString("worldEdgeForceRegions", "Top");
  if (!edge_region_name)
    return std::unexpected(std::move(edge_region_name.error()));
  if (!WorldEdgeForceRegionTypeNames) return std::unexpected(WorldEdgeForceRegionTypeNames.error());
  auto edge_region = WorldEdgeForceRegionTypeNames->get_left((*edge_region_name));
  if (!edge_region)
    return std::unexpected(std::move(edge_region.error()));
  parameters->worldEdgeForceRegions = edge_region->get();
  auto parametersweatherPoolResult1 = biomeDatabase->biomeWeathers(primaryBiome, seed, threatLevel);
  if (!parametersweatherPoolResult1) return std::unexpected(std::move(parametersweatherPoolResult1.error()));
  parameters->weatherPool = std::move((*parametersweatherPoolResult1));
  parameters->primaryBiome = std::move(primaryBiome);
  parameters->sizeName = std::move(sizeName);
  auto parametershueShiftResult1 = biomeDatabase->biomeHueShift(parameters->primaryBiome, surfaceBiomeSeed);
  if (!parametershueShiftResult1) return std::unexpected(std::move(parametershueShiftResult1.error()));
  parameters->hueShift = std::move((*parametershueShiftResult1));
  parameters->primarySurfaceLiquid = std::move(surfaceLayer.primaryRegion.oceanLiquid != EmptyLiquidId
      ? surfaceLayer.primaryRegion.oceanLiquid
      : surfaceLayer.primaryRegion.caveLiquid);
  auto parametersskyColoringResult1 = biomeDatabase->biomeSkyColoring(parameters->primaryBiome, seed);
  if (!parametersskyColoringResult1) return std::unexpected(std::move(parametersskyColoringResult1.error()));
  parameters->skyColoring = std::move((*parametersskyColoringResult1));
  auto day_length_value = static_random_float_range(dayLengthRange[0], dayLengthRange[1], seed, "DayLength");
  parameters->dayLength = std::move(day_length_value);
  auto parametersblockNoiseConfigResult1 = config.get("blockNoise");
  if (!parametersblockNoiseConfigResult1) return std::unexpected(std::move(parametersblockNoiseConfigResult1.error()));
  parameters->blockNoiseConfig = std::move((*parametersblockNoiseConfigResult1));
  auto parametersblendNoiseConfigResult1 = config.get("blendNoise");
  if (!parametersblendNoiseConfigResult1) return std::unexpected(std::move(parametersblendNoiseConfigResult1.error()));
  parameters->blendNoiseConfig = std::move((*parametersblendNoiseConfigResult1));
  auto parametersblendSizeResult1 = config.getFloat("blendSize");
  if (!parametersblendSizeResult1) return std::unexpected(std::move(parametersblendSizeResult1.error()));
  parameters->blendSize = std::move((*parametersblendSizeResult1));
  auto autospaceLayerOptionalResult1 = readLayer("space");
  if (!autospaceLayerOptionalResult1) return std::unexpected(std::move(autospaceLayerOptionalResult1.error()));
  auto spaceLayerOptional = std::move((*autospaceLayerOptionalResult1));
  if (!spaceLayerOptional) return std::unexpected(StarException("Required terrestrial layer space is missing or disabled"));
  parameters->spaceLayer = std::move(*spaceLayerOptional);
  auto autoatmosphereLayerOptionalResult1 = readLayer("atmosphere");
  if (!autoatmosphereLayerOptionalResult1) return std::unexpected(std::move(autoatmosphereLayerOptionalResult1.error()));
  auto atmosphereLayerOptional = std::move((*autoatmosphereLayerOptionalResult1));
  if (!atmosphereLayerOptional) return std::unexpected(StarException("Required terrestrial layer atmosphere is missing or disabled"));
  parameters->atmosphereLayer = std::move(*atmosphereLayerOptional);
  parameters->surfaceLayer = std::move(surfaceLayer);
  auto autosubsurfaceLayerOptionalResult1 = readLayer("subsurface");
  if (!autosubsurfaceLayerOptionalResult1) return std::unexpected(std::move(autosubsurfaceLayerOptionalResult1.error()));
  auto subsurfaceLayerOptional = std::move((*autosubsurfaceLayerOptionalResult1));
  if (!subsurfaceLayerOptional) return std::unexpected(StarException("Required terrestrial layer subsurface is missing or disabled"));
  parameters->subsurfaceLayer = std::move(*subsurfaceLayerOptional);
  for (;;) {
    auto underground = readLayer(Star::format("underground{}", parameters->undergroundLayers.size() + 1));
    if (!underground) return std::unexpected(std::move(underground.error()));
    if (!*underground) break;
    parameters->undergroundLayers.push_back(std::move(**underground));
  }
  auto autocoreLayerOptionalResult1 = readLayer("core");
  if (!autocoreLayerOptionalResult1) return std::unexpected(std::move(autocoreLayerOptionalResult1.error()));
  auto coreLayerOptional = std::move((*autocoreLayerOptionalResult1));
  if (!coreLayerOptional) return std::unexpected(StarException("Required terrestrial layer core is missing or disabled"));
  parameters->coreLayer = std::move(*coreLayerOptional);
  return parameters;
}

std::expected<std::shared_ptr<AsteroidsWorldParameters>, StarException> generateAsteroidsWorldParameters(Root& root, std::uint64_t seed) {


  auto autoassetsResult1 = root.assets();
  if (!autoassetsResult1) return std::unexpected(std::move(autoassetsResult1.error()));
  auto assets = std::move((*autoassetsResult1));

  auto parameters = std::make_shared<AsteroidsWorldParameters>();

  auto autoasteroidsConfigResult1 = assets->json("/asteroids_worlds.config");
  if (!autoasteroidsConfigResult1) return std::unexpected(std::move(autoasteroidsConfigResult1.error()));
  auto asteroidsConfig = std::move((*autoasteroidsConfigResult1));
  auto StringbiomeResult1 = asteroidsConfig.getString("biome");
  if (!StringbiomeResult1) return std::unexpected(std::move(StringbiomeResult1.error()));
  String biome = std::move((*StringbiomeResult1));
  auto autogravityRangeResult1 = asteroidsConfig.get("gravityRange");
  if (!autogravityRangeResult1) return std::unexpected(std::move(autogravityRangeResult1.error()));
  auto autogravityRangeResult2 = jsonToVec2F((*autogravityRangeResult1));
  if (!autogravityRangeResult2) return std::unexpected(std::move(autogravityRangeResult2.error()));
  auto gravityRange = std::move((*autogravityRangeResult2));

  auto autothreatLevelRangeResult1 = asteroidsConfig.get("threatRange");
  if (!autothreatLevelRangeResult1) return std::unexpected(std::move(autothreatLevelRangeResult1.error()));
  auto autothreatLevelRangeResult2 = jsonToVec2F((*autothreatLevelRangeResult1));
  if (!autothreatLevelRangeResult2) return std::unexpected(std::move(autothreatLevelRangeResult2.error()));
  auto threatLevelRange = std::move((*autothreatLevelRangeResult2));
  parameters->threatLevel = std::move(static_cast<float>(
    static_random_double(seed, "ThreatLevel") * (threatLevelRange[1] - threatLevelRange[0]) + threatLevelRange[0]));
  parameters->typeName = std::move("asteroids");
  auto parametersworldSizeResult1 = asteroidsConfig.get("worldSize");
  if (!parametersworldSizeResult1) return std::unexpected(std::move(parametersworldSizeResult1.error()));
  auto parametersworldSizeResult2 = jsonToVec2U((*parametersworldSizeResult1));
  if (!parametersworldSizeResult2) return std::unexpected(std::move(parametersworldSizeResult2.error()));
  parameters->worldSize = std::move((*parametersworldSizeResult2));
  auto gravity_value = static_random_float_range(gravityRange[0], gravityRange[1], seed, "WorldGravity");
  parameters->gravity = std::move(gravity_value);
  auto parametersenvironmentStatusEffectsResult1 = asteroidsConfig.getArray("environmentStatusEffects", JsonArray());
  if (!parametersenvironmentStatusEffectsResult1) return std::unexpected(std::move(parametersenvironmentStatusEffectsResult1.error()));
  auto parametersenvironmentStatusEffectsResult2 = jsonToStringList((*parametersenvironmentStatusEffectsResult1));
  if (!parametersenvironmentStatusEffectsResult2) return std::unexpected(std::move(parametersenvironmentStatusEffectsResult2.error()));
  parameters->environmentStatusEffects = std::move((*parametersenvironmentStatusEffectsResult2));
  auto parametersoverrideTechJson = asteroidsConfig.opt("overrideTech");
  if (!parametersoverrideTechJson) return std::unexpected(std::move(parametersoverrideTechJson.error()));
  if (*parametersoverrideTechJson) {
    auto value = jsonToStringList(**parametersoverrideTechJson);
    if (!value) return std::unexpected(std::move(value.error()));
    parameters->overrideTech = std::move(*value);
  }
  auto parametersglobalDirectivesJson = asteroidsConfig.opt("globalDirectives");
  if (!parametersglobalDirectivesJson) return std::unexpected(std::move(parametersglobalDirectivesJson.error()));
  if (*parametersglobalDirectivesJson) {
    auto value = jsonToDirectivesList(**parametersglobalDirectivesJson);
    if (!value) return std::unexpected(std::move(value.error()));
    parameters->globalDirectives = std::move(*value);
  }
  auto beam_rule_name = asteroidsConfig.getString("beamUpRule", "Surface");
  if (!beam_rule_name)
    return std::unexpected(std::move(beam_rule_name.error()));
  if (!BeamUpRuleNames) return std::unexpected(BeamUpRuleNames.error());
  auto beam_rule = BeamUpRuleNames->get_left((*beam_rule_name));
  if (!beam_rule)
    return std::unexpected(std::move(beam_rule.error()));
  parameters->beamUpRule = beam_rule->get();
  auto parametersdisableDeathDropsResult1 = asteroidsConfig.getBool("disableDeathDrops", false);
  if (!parametersdisableDeathDropsResult1) return std::unexpected(std::move(parametersdisableDeathDropsResult1.error()));
  parameters->disableDeathDrops = std::move((*parametersdisableDeathDropsResult1));
  auto edge_region_name = asteroidsConfig.getString("worldEdgeForceRegions", "TopAndBottom");
  if (!edge_region_name)
    return std::unexpected(std::move(edge_region_name.error()));
  if (!WorldEdgeForceRegionTypeNames) return std::unexpected(WorldEdgeForceRegionTypeNames.error());
  auto edge_region = WorldEdgeForceRegionTypeNames->get_left((*edge_region_name));
  if (!edge_region)
    return std::unexpected(std::move(edge_region.error()));
  parameters->worldEdgeForceRegions = edge_region->get();

  auto parametersasteroidTopLevelResult1 = asteroidsConfig.getInt("asteroidsTop");
  if (!parametersasteroidTopLevelResult1) return std::unexpected(std::move(parametersasteroidTopLevelResult1.error()));
  parameters->asteroidTopLevel = std::move(static_cast<int>((*parametersasteroidTopLevelResult1)));
  auto parametersasteroidBottomLevelResult1 = asteroidsConfig.getInt("asteroidsBottom");
  if (!parametersasteroidBottomLevelResult1) return std::unexpected(std::move(parametersasteroidBottomLevelResult1.error()));
  parameters->asteroidBottomLevel = std::move(static_cast<int>((*parametersasteroidBottomLevelResult1)));
  auto parametersblendSizeResult1 = asteroidsConfig.getFloat("blendSize");
  if (!parametersblendSizeResult1) return std::unexpected(std::move(parametersblendSizeResult1.error()));
  parameters->blendSize = std::move((*parametersblendSizeResult1));
  parameters->asteroidBiome = std::move(biome);
  auto parametersambientLightLevelResult1 = asteroidsConfig.get("ambientLightLevel");
  if (!parametersambientLightLevelResult1) return std::unexpected(std::move(parametersambientLightLevelResult1.error()));
  auto parametersambientLightLevelResult2 = jsonToColor((*parametersambientLightLevelResult1));
  if (!parametersambientLightLevelResult2) return std::unexpected(std::move(parametersambientLightLevelResult2.error()));
  parameters->ambientLightLevel = std::move((*parametersambientLightLevelResult2));

  return parameters;
}

std::expected<std::shared_ptr<FloatingDungeonWorldParameters>, StarException> generateFloatingDungeonWorldParameters(Root& root, String const& dungeonWorldName) {


  auto autoassetsResult1 = root.assets();
  if (!autoassetsResult1) return std::unexpected(std::move(autoassetsResult1.error()));
  auto assets = std::move((*autoassetsResult1));

  auto autoworldConfigResult1 = assets->json("/dungeon_worlds.config:" + dungeonWorldName);
  if (!autoworldConfigResult1) return std::unexpected(std::move(autoworldConfigResult1.error()));
  auto worldConfig = std::move((*autoworldConfigResult1));

  auto parameters = std::make_shared<FloatingDungeonWorldParameters>();

  auto parametersthreatLevelResult1 = worldConfig.getFloat("threatLevel", 0);
  if (!parametersthreatLevelResult1) return std::unexpected(std::move(parametersthreatLevelResult1.error()));
  parameters->threatLevel = std::move((*parametersthreatLevelResult1));
  parameters->typeName = std::move(dungeonWorldName);
  auto parametersworldSizeResult1 = worldConfig.get("worldSize");
  if (!parametersworldSizeResult1) return std::unexpected(std::move(parametersworldSizeResult1.error()));
  auto parametersworldSizeResult2 = jsonToVec2U((*parametersworldSizeResult1));
  if (!parametersworldSizeResult2) return std::unexpected(std::move(parametersworldSizeResult2.error()));
  parameters->worldSize = std::move((*parametersworldSizeResult2));
  auto parametersgravityResult1 = worldConfig.getFloat("gravity");
  if (!parametersgravityResult1) return std::unexpected(std::move(parametersgravityResult1.error()));
  parameters->gravity = std::move(*parametersgravityResult1);
  auto parametersairlessResult1 = worldConfig.getBool("airless", false);
  if (!parametersairlessResult1) return std::unexpected(std::move(parametersairlessResult1.error()));
  parameters->airless = std::move((*parametersairlessResult1));
  auto parametersenvironmentStatusEffectsResult1 = worldConfig.getArray("environmentStatusEffects", JsonArray());
  if (!parametersenvironmentStatusEffectsResult1) return std::unexpected(std::move(parametersenvironmentStatusEffectsResult1.error()));
  auto parametersenvironmentStatusEffectsResult2 = jsonToStringList((*parametersenvironmentStatusEffectsResult1));
  if (!parametersenvironmentStatusEffectsResult2) return std::unexpected(std::move(parametersenvironmentStatusEffectsResult2.error()));
  parameters->environmentStatusEffects = std::move((*parametersenvironmentStatusEffectsResult2));
  auto parametersoverrideTechJson = worldConfig.opt("overrideTech");
  if (!parametersoverrideTechJson) return std::unexpected(std::move(parametersoverrideTechJson.error()));
  if (*parametersoverrideTechJson) {
    auto value = jsonToStringList(**parametersoverrideTechJson);
    if (!value) return std::unexpected(std::move(value.error()));
    parameters->overrideTech = std::move(*value);
  }
  auto parametersglobalDirectivesJson = worldConfig.opt("globalDirectives");
  if (!parametersglobalDirectivesJson) return std::unexpected(std::move(parametersglobalDirectivesJson.error()));
  if (*parametersglobalDirectivesJson) {
    auto value = jsonToDirectivesList(**parametersglobalDirectivesJson);
    if (!value) return std::unexpected(std::move(value.error()));
    parameters->globalDirectives = std::move(*value);
  }
  auto weatherPoolConfig = worldConfig.optArray("weatherPool");
  if (!weatherPoolConfig) return std::unexpected(std::move(weatherPoolConfig.error()));
  if (*weatherPoolConfig) {
    auto parametersweatherPoolResult1 = jsonToWeightedPool<String>(**weatherPoolConfig);
    if (!parametersweatherPoolResult1) return std::unexpected(std::move(parametersweatherPoolResult1.error()));
    parameters->weatherPool = std::move((*parametersweatherPoolResult1));
  }
  auto beam_rule_name = worldConfig.getString("beamUpRule", "Surface");
  if (!beam_rule_name)
    return std::unexpected(std::move(beam_rule_name.error()));
  if (!BeamUpRuleNames) return std::unexpected(BeamUpRuleNames.error());
  auto beam_rule = BeamUpRuleNames->get_left((*beam_rule_name));
  if (!beam_rule)
    return std::unexpected(std::move(beam_rule.error()));
  parameters->beamUpRule = beam_rule->get();
  auto parametersdisableDeathDropsResult1 = worldConfig.getBool("disableDeathDrops", false);
  if (!parametersdisableDeathDropsResult1) return std::unexpected(std::move(parametersdisableDeathDropsResult1.error()));
  parameters->disableDeathDrops = std::move((*parametersdisableDeathDropsResult1));
  auto edge_region_name = worldConfig.getString("worldEdgeForceRegions", "Top");
  if (!edge_region_name)
    return std::unexpected(std::move(edge_region_name.error()));
  if (!WorldEdgeForceRegionTypeNames) return std::unexpected(WorldEdgeForceRegionTypeNames.error());
  auto edge_region = WorldEdgeForceRegionTypeNames->get_left((*edge_region_name));
  if (!edge_region)
    return std::unexpected(std::move(edge_region.error()));
  parameters->worldEdgeForceRegions = edge_region->get();

  auto parametersdungeonBaseHeightResult1 = worldConfig.getInt("dungeonBaseHeight");
  if (!parametersdungeonBaseHeightResult1) return std::unexpected(std::move(parametersdungeonBaseHeightResult1.error()));
  parameters->dungeonBaseHeight = std::move(static_cast<int>((*parametersdungeonBaseHeightResult1)));
  auto parametersdungeonSurfaceHeightResult1 = worldConfig.getInt("dungeonSurfaceHeight", parameters->dungeonBaseHeight);
  if (!parametersdungeonSurfaceHeightResult1) return std::unexpected(std::move(parametersdungeonSurfaceHeightResult1.error()));
  parameters->dungeonSurfaceHeight = std::move(static_cast<int>((*parametersdungeonSurfaceHeightResult1)));
  auto parametersdungeonUndergroundLevelResult1 = worldConfig.getInt("dungeonUndergroundLevel", 0);
  if (!parametersdungeonUndergroundLevelResult1) return std::unexpected(std::move(parametersdungeonUndergroundLevelResult1.error()));
  parameters->dungeonUndergroundLevel = std::move(static_cast<int>((*parametersdungeonUndergroundLevelResult1)));
  auto parametersprimaryDungeonResult1 = worldConfig.getString("primaryDungeon");
  if (!parametersprimaryDungeonResult1) return std::unexpected(std::move(parametersprimaryDungeonResult1.error()));
  parameters->primaryDungeon = std::move((*parametersprimaryDungeonResult1));
  auto parametersbiomeResult1 = worldConfig.optString("biome");
  if (!parametersbiomeResult1) return std::unexpected(std::move(parametersbiomeResult1.error()));
  parameters->biome = std::move((*parametersbiomeResult1));
  auto parametersambientLightLevelResult1 = worldConfig.get("ambientLightLevel");
  if (!parametersambientLightLevelResult1) return std::unexpected(std::move(parametersambientLightLevelResult1.error()));
  auto parametersambientLightLevelResult2 = jsonToColor((*parametersambientLightLevelResult1));
  if (!parametersambientLightLevelResult2) return std::unexpected(std::move(parametersambientLightLevelResult2.error()));
  parameters->ambientLightLevel = std::move((*parametersambientLightLevelResult2));
  auto autohasmusicTrackResult1 = worldConfig.contains("musicTrack");
  if (!autohasmusicTrackResult1) return std::unexpected(std::move(autohasmusicTrackResult1.error()));
  auto hasmusicTrack = std::move((*autohasmusicTrackResult1));
  if (hasmusicTrack) {
    auto parametersdayMusicTrackResult1 = worldConfig.optString("musicTrack");
    if (!parametersdayMusicTrackResult1) return std::unexpected(std::move(parametersdayMusicTrackResult1.error()));
    parameters->dayMusicTrack = std::move((*parametersdayMusicTrackResult1));
    auto parametersnightMusicTrackResult1 = worldConfig.optString("musicTrack");
    if (!parametersnightMusicTrackResult1) return std::unexpected(std::move(parametersnightMusicTrackResult1.error()));
    parameters->nightMusicTrack = std::move((*parametersnightMusicTrackResult1));
  } else {
    auto parametersdayMusicTrackResult1 = worldConfig.optString("dayMusicTrack");
    if (!parametersdayMusicTrackResult1) return std::unexpected(std::move(parametersdayMusicTrackResult1.error()));
    parameters->dayMusicTrack = std::move((*parametersdayMusicTrackResult1));
    auto parametersnightMusicTrackResult1 = worldConfig.optString("nightMusicTrack");
    if (!parametersnightMusicTrackResult1) return std::unexpected(std::move(parametersnightMusicTrackResult1.error()));
    parameters->nightMusicTrack = std::move((*parametersnightMusicTrackResult1));
  }
  auto autohasambientNoisesResult1 = worldConfig.contains("ambientNoises");
  if (!autohasambientNoisesResult1) return std::unexpected(std::move(autohasambientNoisesResult1.error()));
  auto hasambientNoises = std::move((*autohasambientNoisesResult1));
  if (hasambientNoises) {
    auto parametersdayAmbientNoisesResult1 = worldConfig.optString("ambientNoises");
    if (!parametersdayAmbientNoisesResult1) return std::unexpected(std::move(parametersdayAmbientNoisesResult1.error()));
    parameters->dayAmbientNoises = std::move((*parametersdayAmbientNoisesResult1));
    auto parametersnightAmbientNoisesResult1 = worldConfig.optString("ambientNoises");
    if (!parametersnightAmbientNoisesResult1) return std::unexpected(std::move(parametersnightAmbientNoisesResult1.error()));
    parameters->nightAmbientNoises = std::move((*parametersnightAmbientNoisesResult1));
  } else {
    auto parametersdayAmbientNoisesResult1 = worldConfig.optString("dayAmbientNoises");
    if (!parametersdayAmbientNoisesResult1) return std::unexpected(std::move(parametersdayAmbientNoisesResult1.error()));
    parameters->dayAmbientNoises = std::move((*parametersdayAmbientNoisesResult1));
    auto parametersnightAmbientNoisesResult1 = worldConfig.optString("nightAmbientNoises");
    if (!parametersnightAmbientNoisesResult1) return std::unexpected(std::move(parametersnightAmbientNoisesResult1.error()));
    parameters->nightAmbientNoises = std::move((*parametersnightAmbientNoisesResult1));
  }

  return parameters;
}

}

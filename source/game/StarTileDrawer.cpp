import star.xx_hash;
import star.json;
import star.json_extra;
import star.id_map;
import star.list;
import star.bi_map;
import star.data_stream;
import star.ordered_map;
import star.lru_cache;
import star.thread;
import star.time;
import star.random;
import star.ttl_cache;
import star.image;
import star.game_types;
import star.material_types;
import star.math_common;
import star.vector;
import star.format;
import star.version;
import star.color;
import star.string;
import star.rect;
import star.image_processing;
import star.hash;
import star.string_view;
import star.directives;
import star.asset_path;
import star.either;
import star.weighted_pool;
import star.casting;
import star.strong_typedef;
import star.lexical_cast;
import std;
import star.set;
import star.worker_pool;

import star.sector_array_2d;
import star.perlin;
import star.io_device;
import star.ref_ptr;
import star.logging;
import star.listener;

import star.collision_block;
import star.item_descriptor;
import star.liquid_types;
import star.tile_damage;
import star.tile_sector_array;
import star.force_regions;
import star.world_parameters;
import star.celestial_parameters;
import star.world_layout;
import star.collision_generator;
import star.world_tiles;
import star.drawable;
import star.entity_rendering_types;
import star.sky_types;
import star.celestial_coordinate;
import star.sky_parameters;
import star.sky_render_data;
import star.plant_database;
import star.parallax;
import star.animation;
import star.particle;
import star.weather_types;
import star.damage_types;
import star.world_geometry;
import star.status_types;
import star.damage;
import star.light_source;
import star.entity;
import star.cellular_light_array;
import star.cellular_lighting;
import star.world_render_data;
import star.material_render_profile;

import star.tile_drawer;
import star.asset_source;
import star.assets;
import star.root_base;
import star.configuration;
import star.root;

import star.liquids_database;
import star.material_database;


import star.static_random;
namespace Star {

RenderTile const TileDrawer::DefaultRenderTile{
    NullMaterialId,
    NoModId,
    NullMaterialId,
    NoModId,
    0,
    0,
    DefaultMaterialColorVariant,
    TileDamageType::Protected,
    0,
    0,
    0,
    DefaultMaterialColorVariant,
    TileDamageType::Protected,
    0,
    EmptyLiquidId,
    0
};

std::expected<std::shared_ptr<TileDrawer>, StarException> TileDrawer::create(Root& root) {
  auto drawer = std::shared_ptr<TileDrawer>(new TileDrawer(root));
  auto initialized = drawer->initializeTileDrawer();
  if (!initialized)
    return std::unexpected(std::move(initialized.error()));
  return drawer;
}

std::expected<void, StarException> TileDrawer::initializeTileDrawer() {
  auto assets = m_root.assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  auto backgroundConfig = (*assets)->json("/rendering.config:backgroundLayerColor");
  if (!backgroundConfig)
    return std::unexpected(std::move(backgroundConfig.error()));
  auto background = jsonToColor(*backgroundConfig);
  if (!background)
    return std::unexpected(std::move(background.error()));
  m_backgroundLayerColor = background->toRgba();
  auto foregroundConfig = (*assets)->json("/rendering.config:foregroundLayerColor");
  if (!foregroundConfig)
    return std::unexpected(std::move(foregroundConfig.error()));
  auto foreground = jsonToColor(*foregroundConfig);
  if (!foreground)
    return std::unexpected(std::move(foreground.error()));
  m_foregroundLayerColor = foreground->toRgba();
  auto liquidConfig = (*assets)->json("/rendering.config:liquidDrawLevels");
  if (!liquidConfig)
    return std::unexpected(std::move(liquidConfig.error()));
  auto liquid = jsonToVec2F(*liquidConfig);
  if (!liquid)
    return std::unexpected(std::move(liquid.error()));
  m_liquidDrawLevels = *liquid;
  return {};
}

std::expected<bool, StarException> TileDrawer::produceTerrainDrawables(Drawables& drawables,
  TerrainLayer terrainLayer, Vec2I const& pos, WorldRenderData const& renderData, float scale, Vec2I offset, std::optional<TerrainLayer> variantLayer) {
  auto materialDatabaseResult = m_root.materialDatabase();
  if (!materialDatabaseResult)
    return std::unexpected(std::move(materialDatabaseResult.error()));
  auto const& materialDatabase = *materialDatabaseResult;
  auto imageMetadataResult = m_root.imageMetadataDatabase();
  if (!imageMetadataResult)
    return std::unexpected(std::move(imageMetadataResult.error()));
  auto const& imageMetadata = *imageMetadataResult;

  RenderTile const& tile = getRenderTile(renderData, pos);

  MaterialId material = EmptyMaterialId;
  MaterialHue materialHue = 0;
  MaterialColorVariant colorVariant = 0;
  ModId mod = NoModId;
  MaterialHue modHue = 0;
  float damageLevel = 0.0f;
  TileDamageType damageType = TileDamageType::Protected;
  Color color;

  bool occlude = false;

  if (terrainLayer == TerrainLayer::Background) {
    material = tile.background;
    materialHue = tile.backgroundHueShift;
    colorVariant = tile.backgroundColorVariant;
    mod = tile.backgroundMod;
    modHue = tile.backgroundModHueShift;
    damageLevel = byteToFloat(tile.backgroundDamageLevel);
    damageType = tile.backgroundDamageType;
    color = Color::rgba(m_backgroundLayerColor);
  } else {
    material = tile.foreground;
    materialHue = tile.foregroundHueShift;
    colorVariant = tile.foregroundColorVariant;
    mod = tile.foregroundMod;
    modHue = tile.foregroundModHueShift;
    damageLevel = byteToFloat(tile.foregroundDamageLevel);
    damageType = tile.foregroundDamageType;
    color = Color::rgba(m_foregroundLayerColor);
  }

  // render non-block colliding things in the midground
  auto collision = materialDatabase->materialCollisionKind(material);
  if (!collision)
    return std::unexpected(std::move(collision.error()));
  bool isBlock = BlockCollisionSet.contains(*collision);
  if ((isBlock && terrainLayer == TerrainLayer::Midground) || (!isBlock && terrainLayer == TerrainLayer::Foreground))
    return false;

  auto getPieceImage = [](std::shared_ptr<MaterialRenderPiece const> const& piece, Box<float, 2> const& box, MaterialHue hue, std::optional<std::reference_wrapper<Directives const>> directives) -> AssetPath {
    String path = (hue == 0)
      ? strf("{}?crop={};{};{};{}", piece->texture, box.xMin(), box.yMin(), box.xMax(), box.yMax())
      : strf("{}?crop={};{};{};{}?hueshift={}", piece->texture, box.xMin(), box.yMin(), box.xMax(), box.yMax(), materialHueToDegrees(hue));

    AssetPath image(path);
    if (directives)
      image.directives += directives->get();

    return image;
  };

  auto materialRenderProfile = materialDatabase->materialRenderProfile(material);
  auto modRenderProfile = materialDatabase->modRenderProfile(mod);

  if (materialRenderProfile) {
    occlude = materialRenderProfile->occludesBehind;
    auto materialColorVariant = materialRenderProfile->colorVariants > 0 ? colorVariant % materialRenderProfile->colorVariants : 0;
    std::uint32_t variance = static_random_u32(renderData.geometry.xwrap(pos[0]) + offset[0], pos[1] + offset[1],
                                               static_cast<int>(variantLayer.value_or(terrainLayer)), "main");
    auto& drawList = drawables[materialZLevel(materialRenderProfile->zLevel, material, materialHue, materialColorVariant)];

    MaterialPieceResultList pieces;
    auto matched = determineMatchingPieces(pieces, std::ref(occlude), materialDatabase, materialRenderProfile->mainMatchList, renderData, pos,
        terrainLayer == TerrainLayer::Background ? TileLayer::Background : TileLayer::Foreground, false);
    if (!matched)
      return std::unexpected(std::move(matched.error()));
    std::optional<std::reference_wrapper<Directives const>> directives;
    if (!materialRenderProfile->colorDirectives.empty()) {
      auto selected = materialRenderProfile->colorDirectives.wrap(materialColorVariant);
      if (!selected)
        return std::unexpected(std::move(selected.error()));
      directives = selected->get();
    }
    for (auto const& piecePair : pieces) {
      auto variant = piecePair.first->variants.find_ref(materialColorVariant);
      if (!variant)
        variant = piecePair.first->variants.find_ref(0);
      if (!variant) continue;
      auto textureCoords = variant->get().wrap(variance);
      if (!textureCoords)
        return std::unexpected(std::move(textureCoords.error()));
      auto image = getPieceImage(piecePair.first, textureCoords->get(), materialHue, directives);
      auto drawable = Drawable::makeImage(*imageMetadata, image, scale, false, piecePair.second * scale + Vec2F(pos), color);
      if (!drawable)
        return std::unexpected(std::move(drawable.error()));
      drawList.emplace_back(std::move(*drawable));
    }
  }

  if (modRenderProfile) {
    auto modColorVariant = modRenderProfile->colorVariants > 0 ? colorVariant % modRenderProfile->colorVariants : 0;
    std::uint32_t variance = static_random_u32(renderData.geometry.xwrap(pos[0]), pos[1],
                                               static_cast<int>(variantLayer.value_or(terrainLayer)), "mod");
    auto& drawList = drawables[modZLevel(modRenderProfile->zLevel, mod, modHue, modColorVariant)];

    MaterialPieceResultList pieces;
    auto matched = determineMatchingPieces(pieces, std::ref(occlude), materialDatabase, modRenderProfile->mainMatchList, renderData, pos,
        terrainLayer == TerrainLayer::Background ? TileLayer::Background : TileLayer::Foreground, true);
    if (!matched)
      return std::unexpected(std::move(matched.error()));
    std::optional<std::reference_wrapper<Directives const>> directives;
    if (!modRenderProfile->colorDirectives.empty()) {
      auto selected = modRenderProfile->colorDirectives.wrap(modColorVariant);
      if (!selected)
        return std::unexpected(std::move(selected.error()));
      directives = selected->get();
    }
    for (auto const& piecePair : pieces) {
      auto variant = piecePair.first->variants.find_ref(modColorVariant);
      if (!variant)
        variant = piecePair.first->variants.find_ref(0);
      if (!variant) continue;
      auto textureCoords = variant->get().wrap(variance);
      if (!textureCoords)
        return std::unexpected(std::move(textureCoords.error()));
      auto image = getPieceImage(piecePair.first, textureCoords->get(), modHue, directives);
      auto drawable = Drawable::makeImage(*imageMetadata, image, scale, false, piecePair.second * scale + Vec2F(pos), color);
      if (!drawable)
        return std::unexpected(std::move(drawable.error()));
      drawList.emplace_back(std::move(*drawable));
    }
  }

  if (materialRenderProfile && damageLevel > 0 && isBlock) {
    auto& drawList = drawables[damageZLevel()];
    auto crackingImage = materialRenderProfile->damageImage(damageLevel, damageType);
    if (!crackingImage)
      return std::unexpected(std::move(crackingImage.error()));
    auto drawable = Drawable::makeImage(*imageMetadata, crackingImage->get().first, scale, false, crackingImage->get().second * scale + Vec2F(pos), color);
    if (!drawable)
      return std::unexpected(std::move(drawable.error()));
    drawList.emplace_back(std::move(*drawable));
  }

  return occlude;
}

WorldRenderData& TileDrawer::renderData() {
  return m_tempRenderData;
}

std::unique_lock<std::mutex> TileDrawer::lockRenderData() {
  return std::unique_lock(m_tempRenderDataMutex);
}

RenderTile const& TileDrawer::getRenderTile(WorldRenderData const& renderData, Vec2I const& worldPos) {
  Vec2I arrayPos = Vec2I(renderData.geometry.pdiff(worldPos[0], renderData.tileMinPosition[0]), worldPos[1] - renderData.tileMinPosition[1]);

  Vec2I size = Vec2I(renderData.tiles.size());
  if (arrayPos[0] >= 0 && arrayPos[1] >= 0 && arrayPos[0] < size[0] && arrayPos[1] < size[1])
    return renderData.tiles(Vec2S(arrayPos));

  return DefaultRenderTile;
}

TileDrawer::QuadZLevel TileDrawer::materialZLevel(std::uint32_t zLevel, MaterialId material, MaterialHue hue, MaterialColorVariant colorVariant) {
  QuadZLevel quadZLevel = 0;
  quadZLevel |= static_cast<std::uint64_t>(colorVariant);
  quadZLevel |= static_cast<std::uint64_t>(hue) << 8;
  quadZLevel |= static_cast<std::uint64_t>(material) << 16;
  quadZLevel |= static_cast<std::uint64_t>(zLevel) << 32;
  return quadZLevel;
}

TileDrawer::QuadZLevel TileDrawer::modZLevel(std::uint32_t zLevel, ModId mod, MaterialHue hue, MaterialColorVariant colorVariant) {
  QuadZLevel quadZLevel = 0;
  quadZLevel |= static_cast<std::uint64_t>(colorVariant);
  quadZLevel |= static_cast<std::uint64_t>(hue) << 8;
  quadZLevel |= static_cast<std::uint64_t>(mod) << 16;
  quadZLevel |= static_cast<std::uint64_t>(zLevel) << 32;
  quadZLevel |= static_cast<std::uint64_t>(1) << 63;
  return quadZLevel;
}

TileDrawer::QuadZLevel TileDrawer::damageZLevel() { return static_cast<std::uint64_t>(-1); }

std::expected<bool, StarException> TileDrawer::determineMatchingPieces(MaterialPieceResultList& resultList, std::optional<std::reference_wrapper<bool>> occlude, std::shared_ptr<MaterialDatabase const> const& materialDb, MaterialRenderMatchList const& matchList,
    WorldRenderData const& renderData, Vec2I const& basePos, TileLayer layer, bool isMod) {
  RenderTile const& tile = getRenderTile(renderData, basePos);

  auto matchSetMatches = [&](std::shared_ptr<MaterialRenderMatch const> const& match) -> std::expected<bool, StarException> {
    if (match->requiredLayer && *match->requiredLayer != layer)
      return false;

    if (match->matchPoints.empty())
      return true;

    bool matchValid = match->matchJoin == MaterialJoinType::All;
    for (auto const& matchPoint : match->matchPoints) {
      auto const& neighborTile = getRenderTile(renderData, basePos + matchPoint.position);

      bool neighborShadowing = false;
      if (layer == TileLayer::Background) {
        if (auto profile = materialDb->materialRenderProfile(neighborTile.foreground))
          neighborShadowing = !profile->foregroundLightTransparent;
      }

      MaterialHue baseHue = layer == TileLayer::Foreground ? tile.foregroundHueShift : tile.backgroundHueShift;
      MaterialHue neighborHue = layer == TileLayer::Foreground ? neighborTile.foregroundHueShift : neighborTile.backgroundHueShift;
      MaterialHue baseModHue = layer == TileLayer::Foreground ? tile.foregroundModHueShift : tile.backgroundModHueShift;
      MaterialHue neighborModHue = layer == TileLayer::Foreground ? neighborTile.foregroundModHueShift : neighborTile.backgroundModHueShift;
      MaterialId baseMaterial = layer == TileLayer::Foreground ? tile.foreground : tile.background;
      MaterialId neighborMaterial = layer == TileLayer::Foreground ? neighborTile.foreground : neighborTile.background;
      ModId baseMod = layer == TileLayer::Foreground ? tile.foregroundMod : tile.backgroundMod;
      ModId neighborMod = layer == TileLayer::Foreground ? neighborTile.foregroundMod : neighborTile.backgroundMod;

      bool rulesValid = matchPoint.rule->join == MaterialJoinType::All;
      for (auto const& ruleEntry : matchPoint.rule->entries) {
        bool valid = true;
        if (isMod) {
          if (std::holds_alternative<MaterialRule::RuleEmpty>(ruleEntry.rule)) {
            valid = neighborMod == NoModId;
          } else if (std::holds_alternative<MaterialRule::RuleConnects>(ruleEntry.rule)) {
            valid = isConnectableMaterial(neighborMaterial);
          } else if (std::holds_alternative<MaterialRule::RuleShadows>(ruleEntry.rule)) {
            valid = neighborShadowing;
          } else if (ruleEntry.rule.index() == 4) {
            valid = neighborMod == baseMod;
            if (std::get<4>(ruleEntry.rule).matchHue)
              valid = valid && baseModHue == neighborModHue;
          } else if (std::holds_alternative<MaterialRule::RuleEqualsId>(ruleEntry.rule)) {
            valid = neighborMod == std::get<MaterialRule::RuleEqualsId>(ruleEntry.rule).id;
          } else if (std::holds_alternative<MaterialRule::RulePropertyEquals>(ruleEntry.rule)) {
            auto const& propertyEquals = std::get<MaterialRule::RulePropertyEquals>(ruleEntry.rule);
            if (auto profile = materialDb->modRenderProfile(neighborMod)) {
              auto property = profile->ruleProperties.get(propertyEquals.propertyName, Json());
              if (!property)
                return std::unexpected(std::move(property.error()));
              valid = *property == propertyEquals.compare;
            } else {
              valid = false;
            }
          }
        } else {
          if (std::holds_alternative<MaterialRule::RuleEmpty>(ruleEntry.rule)) {
            valid = neighborMaterial == EmptyMaterialId;
          } else if (std::holds_alternative<MaterialRule::RuleConnects>(ruleEntry.rule)) {
            valid = isConnectableMaterial(neighborMaterial);
          } else if (std::holds_alternative<MaterialRule::RuleShadows>(ruleEntry.rule)) {
            valid = neighborShadowing;
          } else if (ruleEntry.rule.index() == 4) {
            valid = neighborMaterial == baseMaterial;
            if (std::get<4>(ruleEntry.rule).matchHue)
              valid = valid && baseHue == neighborHue;
          } else if (std::holds_alternative<MaterialRule::RuleEqualsId>(ruleEntry.rule)) {
            valid = neighborMaterial == std::get<MaterialRule::RuleEqualsId>(ruleEntry.rule).id;
          } else if (std::holds_alternative<MaterialRule::RulePropertyEquals>(ruleEntry.rule)) {
            auto const& propertyEquals = std::get<MaterialRule::RulePropertyEquals>(ruleEntry.rule);
            if (auto profile = materialDb->materialRenderProfile(neighborMaterial)) {
              auto property = profile->ruleProperties.get(propertyEquals.propertyName);
              if (!property)
                return std::unexpected(std::move(property.error()));
              valid = *property == propertyEquals.compare;
            } else {
              valid = false;
            }
          }
        }
        if (ruleEntry.inverse)
          valid = !valid;

        if (matchPoint.rule->join == MaterialJoinType::All) {
          rulesValid = valid && rulesValid;
          if (!rulesValid)
            break;
        } else {
          rulesValid = valid || rulesValid;
        }
      }

      if (match->matchJoin == MaterialJoinType::All) {
        matchValid = matchValid && rulesValid;
        if (!matchValid)
          return matchValid;
      } else {
        matchValid = matchValid || rulesValid;
      }
    }
    return matchValid;
  };

  bool subMatchResult = false;
  for (auto const& match : matchList) {
    auto matches = matchSetMatches(match);
    if (!matches)
      return std::unexpected(std::move(matches.error()));
    if (*matches) {
      if (match->occlude && occlude)
        occlude->get() = *match->occlude;

      subMatchResult = true;

      for (auto const& piecePair : match->resultingPieces)
        resultList.push_back({piecePair.first, piecePair.second});

      auto matched = determineMatchingPieces(resultList, occlude, materialDb, match->subMatches, renderData, basePos, layer, isMod);
      if (!matched)
        return std::unexpected(std::move(matched.error()));
      if (*matched && match->haltOnSubMatch)
        break;

      if (match->haltOnMatch)
        break;
    }
  }

  return subMatchResult;
}

}

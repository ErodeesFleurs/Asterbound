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
import star.color;
import star.string;
import star.rect;
import star.image_processing;
import star.hash;
import star.string_view;
import star.directives;
import star.asset_path;
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
    empty_liquid_id,
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
  auto background = json_to_color(*backgroundConfig);
  if (!background)
    return std::unexpected(std::move(background.error()));
  m_backgroundLayerColor = background->to_rgba();
  auto foregroundConfig = (*assets)->json("/rendering.config:foregroundLayerColor");
  if (!foregroundConfig)
    return std::unexpected(std::move(foregroundConfig.error()));
  auto foreground = json_to_color(*foregroundConfig);
  if (!foreground)
    return std::unexpected(std::move(foreground.error()));
  m_foregroundLayerColor = foreground->to_rgba();
  auto liquidConfig = (*assets)->json("/rendering.config:liquidDrawLevels");
  if (!liquidConfig)
    return std::unexpected(std::move(liquidConfig.error()));
  auto liquid = json_to_vec2f(*liquidConfig);
  if (!liquid)
    return std::unexpected(std::move(liquid.error()));
  m_liquidDrawLevels = *liquid;
  return {};
}

std::expected<bool, StarException> TileDrawer::produceTerrainDrawables(Drawables& drawables,
  TerrainLayer terrainLayer, Vec2I const& pos, WorldRenderData const& renderData, float scale, Vec2I offset, std::optional<TerrainLayer> variantLayer) {
  auto material_database_result = m_root.material_database();
  if (!material_database_result)
    return std::unexpected(std::move(material_database_result.error()));
  auto const& material_database = **material_database_result;
  auto image_metadata_result = m_root.image_metadata_database();
  if (!image_metadata_result)
    return std::unexpected(std::move(image_metadata_result.error()));
  auto const& image_metadata = *image_metadata_result;

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
    damageLevel = byte_to_float(tile.backgroundDamageLevel);
    damageType = tile.backgroundDamageType;
    color = Color::rgba(m_backgroundLayerColor);
  } else {
    material = tile.foreground;
    materialHue = tile.foregroundHueShift;
    colorVariant = tile.foregroundColorVariant;
    mod = tile.foregroundMod;
    modHue = tile.foregroundModHueShift;
    damageLevel = byte_to_float(tile.foregroundDamageLevel);
    damageType = tile.foregroundDamageType;
    color = Color::rgba(m_foregroundLayerColor);
  }

  // render non-block colliding things in the midground
  auto collision = material_database.material_collision_kind(material);
  if (!collision)
    return std::unexpected(std::move(collision.error()));
  bool isBlock = BlockCollisionSet.contains(*collision);
  if ((isBlock && terrainLayer == TerrainLayer::Midground) || (!isBlock && terrainLayer == TerrainLayer::Foreground))
    return false;

  auto get_piece_image = [](MaterialRenderPiece const& piece, Box<float, 2> const& box, MaterialHue hue, std::optional<std::reference_wrapper<Directives const>> directives) -> AssetPath {
    String path = (hue == 0)
      ? Star::format("{}?crop={};{};{};{}", piece.texture, box.x_min(), box.y_min(), box.x_max(), box.y_max())
      : Star::format("{}?crop={};{};{};{}?hueshift={}", piece.texture, box.x_min(), box.y_min(), box.x_max(), box.y_max(), materialHueToDegrees(hue));

    AssetPath image(path);
    if (directives)
      image.directives += directives->get();

    return image;
  };

  auto material_profile = material_database.material_render_profile(material);
  auto mod_profile = material_database.mod_render_profile(mod);

  if (material_profile) {
    auto const& profile = material_profile->get();
    occlude = profile.occludesBehind;
    auto material_color_variant = profile.colorVariants > 0 ? colorVariant % profile.colorVariants : 0;
    std::uint32_t variance = static_random_u32(renderData.geometry.xwrap(pos[0]) + offset[0], pos[1] + offset[1],
                                               static_cast<int>(variantLayer.value_or(terrainLayer)), "main");
    auto& drawList = drawables[materialZLevel(profile.zLevel, material, materialHue, material_color_variant)];

    MaterialPieceResultList pieces;
    auto matched = determine_matching_pieces(pieces, std::ref(occlude), material_database, profile.mainMatchList, renderData, pos,
        terrainLayer == TerrainLayer::Background ? TileLayer::Background : TileLayer::Foreground, false);
    if (!matched)
      return std::unexpected(std::move(matched.error()));
    std::optional<std::reference_wrapper<Directives const>> directives;
    if (!profile.colorDirectives.empty()) {
      auto selected = profile.colorDirectives.wrap(material_color_variant);
      if (!selected)
        return std::unexpected(std::move(selected.error()));
      directives = selected->get();
    }
    for (auto const& piece_pair : pieces) {
      auto variant = piece_pair.first.get().variants.find_ref(material_color_variant);
      if (!variant)
        variant = piece_pair.first.get().variants.find_ref(0);
      if (!variant) continue;
      auto textureCoords = variant->get().wrap(variance);
      if (!textureCoords)
        return std::unexpected(std::move(textureCoords.error()));
      auto image = get_piece_image(piece_pair.first.get(), textureCoords->get(), materialHue, directives);
      auto drawable = Drawable::make_image(*image_metadata, image, scale, false, piece_pair.second * scale + Vec2F(pos), color);
      if (!drawable)
        return std::unexpected(std::move(drawable.error()));
      drawList.emplace_back(std::move(*drawable));
    }
  }

  if (mod_profile) {
    auto const& profile = mod_profile->get();
    auto mod_color_variant = profile.colorVariants > 0 ? colorVariant % profile.colorVariants : 0;
    std::uint32_t variance = static_random_u32(renderData.geometry.xwrap(pos[0]), pos[1],
                                               static_cast<int>(variantLayer.value_or(terrainLayer)), "mod");
    auto& drawList = drawables[modZLevel(profile.zLevel, mod, modHue, mod_color_variant)];

    MaterialPieceResultList pieces;
    auto matched = determine_matching_pieces(pieces, std::ref(occlude), material_database, profile.mainMatchList, renderData, pos,
        terrainLayer == TerrainLayer::Background ? TileLayer::Background : TileLayer::Foreground, true);
    if (!matched)
      return std::unexpected(std::move(matched.error()));
    std::optional<std::reference_wrapper<Directives const>> directives;
    if (!profile.colorDirectives.empty()) {
      auto selected = profile.colorDirectives.wrap(mod_color_variant);
      if (!selected)
        return std::unexpected(std::move(selected.error()));
      directives = selected->get();
    }
    for (auto const& piece_pair : pieces) {
      auto variant = piece_pair.first.get().variants.find_ref(mod_color_variant);
      if (!variant)
        variant = piece_pair.first.get().variants.find_ref(0);
      if (!variant) continue;
      auto textureCoords = variant->get().wrap(variance);
      if (!textureCoords)
        return std::unexpected(std::move(textureCoords.error()));
      auto image = get_piece_image(piece_pair.first.get(), textureCoords->get(), modHue, directives);
      auto drawable = Drawable::make_image(*image_metadata, image, scale, false, piece_pair.second * scale + Vec2F(pos), color);
      if (!drawable)
        return std::unexpected(std::move(drawable.error()));
      drawList.emplace_back(std::move(*drawable));
    }
  }

  if (material_profile && damageLevel > 0 && isBlock) {
    auto& drawList = drawables[damageZLevel()];
    auto cracking_image = material_profile->get().damageImage(damageLevel, damageType);
    if (!cracking_image)
      return std::unexpected(std::move(cracking_image.error()));
    auto drawable = Drawable::make_image(*image_metadata, cracking_image->get().first, scale, false, cracking_image->get().second * scale + Vec2F(pos), color);
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
  Vec2I arrayPos = Vec2I(renderData.geometry.pdiff(worldPos[0], renderData.tile_min_position[0]), worldPos[1] - renderData.tile_min_position[1]);

  Vec2I size = Vec2I(renderData.tiles.extents());
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

std::expected<bool, StarException> TileDrawer::determine_matching_pieces(MaterialPieceResultList& result_list, std::optional<std::reference_wrapper<bool>> occlude, MaterialDatabase const& material_database, MaterialRenderMatchList const& match_list,
    WorldRenderData const& renderData, Vec2I const& base_pos, TileLayer layer, bool is_mod) {
  RenderTile const& tile = getRenderTile(renderData, base_pos);

  auto match_set_matches = [&](std::shared_ptr<MaterialRenderMatch const> const& match) -> std::expected<bool, StarException> {
    if (match->requiredLayer && *match->requiredLayer != layer)
      return false;

    if (match->matchPoints.empty())
      return true;

    bool match_valid = match->matchJoin == MaterialJoinType::All;
    for (auto const& match_point : match->matchPoints) {
      auto const& neighbor_tile = getRenderTile(renderData, base_pos + match_point.position);

      bool neighbor_shadowing = false;
      if (layer == TileLayer::Background) {
        if (auto profile = material_database.material_render_profile(neighbor_tile.foreground))
          neighbor_shadowing = !profile->get().foregroundLightTransparent;
      }

      MaterialHue base_hue = layer == TileLayer::Foreground ? tile.foregroundHueShift : tile.backgroundHueShift;
      MaterialHue neighbor_hue = layer == TileLayer::Foreground ? neighbor_tile.foregroundHueShift : neighbor_tile.backgroundHueShift;
      MaterialHue base_mod_hue = layer == TileLayer::Foreground ? tile.foregroundModHueShift : tile.backgroundModHueShift;
      MaterialHue neighbor_mod_hue = layer == TileLayer::Foreground ? neighbor_tile.foregroundModHueShift : neighbor_tile.backgroundModHueShift;
      MaterialId base_material = layer == TileLayer::Foreground ? tile.foreground : tile.background;
      MaterialId neighbor_material = layer == TileLayer::Foreground ? neighbor_tile.foreground : neighbor_tile.background;
      ModId base_mod = layer == TileLayer::Foreground ? tile.foregroundMod : tile.backgroundMod;
      ModId neighbor_mod = layer == TileLayer::Foreground ? neighbor_tile.foregroundMod : neighbor_tile.backgroundMod;

      bool rules_valid = match_point.rule->join == MaterialJoinType::All;
      for (auto const& rule_entry : match_point.rule->entries) {
        bool valid = true;
        if (is_mod) {
          if (std::holds_alternative<MaterialRule::RuleEmpty>(rule_entry.rule)) {
            valid = neighbor_mod == NoModId;
          } else if (std::holds_alternative<MaterialRule::RuleConnects>(rule_entry.rule)) {
            valid = isConnectableMaterial(neighbor_material);
          } else if (std::holds_alternative<MaterialRule::RuleShadows>(rule_entry.rule)) {
            valid = neighbor_shadowing;
          } else if (rule_entry.rule.index() == 4) {
            valid = neighbor_mod == base_mod;
            if (std::get<4>(rule_entry.rule).matchHue)
              valid = valid && base_mod_hue == neighbor_mod_hue;
          } else if (std::holds_alternative<MaterialRule::RuleEqualsId>(rule_entry.rule)) {
            valid = neighbor_mod == std::get<MaterialRule::RuleEqualsId>(rule_entry.rule).id;
          } else if (std::holds_alternative<MaterialRule::RulePropertyEquals>(rule_entry.rule)) {
            auto const& property_equals = std::get<MaterialRule::RulePropertyEquals>(rule_entry.rule);
            if (auto profile = material_database.mod_render_profile(neighbor_mod)) {
              auto property = profile->get().ruleProperties.get(property_equals.propertyName, Json());
              if (!property)
                return std::unexpected(std::move(property.error()));
              valid = *property == property_equals.compare;
            } else {
              valid = false;
            }
          }
        } else {
          if (std::holds_alternative<MaterialRule::RuleEmpty>(rule_entry.rule)) {
            valid = neighbor_material == EmptyMaterialId;
          } else if (std::holds_alternative<MaterialRule::RuleConnects>(rule_entry.rule)) {
            valid = isConnectableMaterial(neighbor_material);
          } else if (std::holds_alternative<MaterialRule::RuleShadows>(rule_entry.rule)) {
            valid = neighbor_shadowing;
          } else if (rule_entry.rule.index() == 4) {
            valid = neighbor_material == base_material;
            if (std::get<4>(rule_entry.rule).matchHue)
              valid = valid && base_hue == neighbor_hue;
          } else if (std::holds_alternative<MaterialRule::RuleEqualsId>(rule_entry.rule)) {
            valid = neighbor_material == std::get<MaterialRule::RuleEqualsId>(rule_entry.rule).id;
          } else if (std::holds_alternative<MaterialRule::RulePropertyEquals>(rule_entry.rule)) {
            auto const& property_equals = std::get<MaterialRule::RulePropertyEquals>(rule_entry.rule);
            if (auto profile = material_database.material_render_profile(neighbor_material)) {
              auto property = profile->get().ruleProperties.get(property_equals.propertyName);
              if (!property)
                return std::unexpected(std::move(property.error()));
              valid = *property == property_equals.compare;
            } else {
              valid = false;
            }
          }
        }
        if (rule_entry.inverse)
          valid = !valid;

        if (match_point.rule->join == MaterialJoinType::All) {
          rules_valid = valid && rules_valid;
          if (!rules_valid)
            break;
        } else {
          rules_valid = valid || rules_valid;
        }
      }

      if (match->matchJoin == MaterialJoinType::All) {
        match_valid = match_valid && rules_valid;
        if (!match_valid)
          return match_valid;
      } else {
        match_valid = match_valid || rules_valid;
      }
    }
    return match_valid;
  };

  bool sub_match_result = false;
  for (auto const& match : match_list) {
    auto matches = match_set_matches(match);
    if (!matches)
      return std::unexpected(std::move(matches.error()));
    if (*matches) {
      if (match->occlude && occlude)
        occlude->get() = *match->occlude;

      sub_match_result = true;

      for (auto const& piece_pair : match->resultingPieces)
        result_list.emplace_back(std::cref(*piece_pair.first), piece_pair.second);

      auto matched = determine_matching_pieces(result_list, occlude, material_database, match->subMatches, renderData, base_pos, layer, is_mod);
      if (!matched)
        return std::unexpected(std::move(matched.error()));
      if (*matched && match->haltOnSubMatch)
        break;

      if (match->haltOnMatch)
        break;
    }
  }

  return sub_match_result;
}

}

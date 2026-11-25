#include <stddef.h>
import std;
import star.json;
import star.json_extra;
import star.xx_hash;
import star.id_map;
import star.list;
import star.map;
import star.bi_map;
import star.data_stream;
import star.casting;
import star.strong_typedef;
import star.lexical_cast;
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
import star.set;
import star.worker_pool;

import star.sector_array_2d;
import star.io_device;
import star.ref_ptr;
import star.logging;
import star.listener;
import star.perlin;

import star.tile_sector_array;
import star.asset_source;
import star.assets;
import star.root_base;
import star.configuration;
import star.root;
import star.force_regions;
import star.world_parameters;
import star.celestial_parameters;
import star.world_layout;
import star.collision_block;
import star.item_descriptor;
import star.damage_types;
import star.world_geometry;
import star.status_types;
import star.damage;
import star.light_source;
import star.entity;
import star.renderer;
import star.tile_painter;
import star.liquid_types;
import star.tile_damage;
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
import star.cellular_light_array;
import star.cellular_lighting;
import star.world_render_data;
import star.material_render_profile;

import star.liquids_database;
import star.material_database;
import star.world_camera;


import star.tile_drawer;

import star.byte_array;
import star.static_random;
import star.matrix3;
namespace Star {

TilePainter::TilePainter(Root& root, std::shared_ptr<Renderer> renderer)
  : TileDrawer(root),
    m_renderer(std::move(renderer)),
    m_textureCache(root.presentationRandom()),
    m_terrainChunkCache(root.presentationRandom()),
    m_liquidChunkCache(root.presentationRandom()) {
  constexpr std::size_t hashWidth = RenderChunkSize + 2 * material_render_profile_max_neighbor_distance;
  m_terrainHashBuffer.resize(hashWidth * hashWidth * offsetof(RenderTile, liquidId));
  m_liquidHashBuffer.resize(hashWidth * hashWidth
                            * (sizeof(RenderTile::liquidId) + sizeof(RenderTile::liquidLevel) + 1));
}

std::expected<std::shared_ptr<TilePainter>, StarException> TilePainter::create(Root& root, std::shared_ptr<Renderer> renderer) {
  auto painter = std::shared_ptr<TilePainter>(new TilePainter(root, std::move(renderer)));
  auto initialized = painter->initialize_tile_drawer();
  if (!initialized)
    return std::unexpected(std::move(initialized.error()));
  auto textureGroup = painter->m_renderer->createTextureGroup(TextureGroupSize::Large);
  if (!textureGroup)
    return std::unexpected(std::move(textureGroup.error()));
  painter->m_textureGroup = std::move(*textureGroup);

  auto assets = root.assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  auto chunkTimeoutJson = (*assets)->json("/rendering.config:chunkCacheTimeout");
  if (!chunkTimeoutJson)
    return std::unexpected(std::move(chunkTimeoutJson.error()));
  auto chunkTimeout = chunkTimeoutJson->to_int();
  if (!chunkTimeout)
    return std::unexpected(std::move(chunkTimeout.error()));
  painter->m_terrainChunkCache.set_time_to_live(std::chrono::milliseconds(*chunkTimeout));
  painter->m_terrainChunkCache.set_time_smear(std::chrono::milliseconds(static_cast<int>(*chunkTimeout / 4)));
  painter->m_liquidChunkCache.set_time_to_live(std::chrono::milliseconds(*chunkTimeout));
  painter->m_liquidChunkCache.set_time_smear(std::chrono::milliseconds(static_cast<int>(*chunkTimeout / 4)));

  auto textureTimeoutJson = (*assets)->json("/rendering.config:textureTimeout");
  if (!textureTimeoutJson)
    return std::unexpected(std::move(textureTimeoutJson.error()));
  auto textureTimeout = textureTimeoutJson->to_int();
  if (!textureTimeout)
    return std::unexpected(std::move(textureTimeout.error()));
  painter->m_textureCache.set_time_to_live(std::chrono::milliseconds(*textureTimeout));

  auto liquids = root.liquids_database();
  if (!liquids)
    return std::unexpected(std::move(liquids.error()));
  for (auto const& liquid : (*liquids)->all_liquid_settings()) {
    auto texture_path = liquid.config.get_string("texture");
    if (!texture_path)
      return std::unexpected(std::move(texture_path.error()));
    auto image = (*assets)->image(*texture_path);
    if (!image)
      return std::unexpected(std::move(image.error()));
    auto texture = painter->m_renderer->createTexture(**image, TextureAddressing::Wrap);
    if (!texture)
      return std::unexpected(std::move(texture.error()));
    auto color_json = liquid.config.get("color");
    if (!color_json)
      return std::unexpected(std::move(color_json.error()));
    auto color = json_to_color(*color_json);
    if (!color)
      return std::unexpected(std::move(color.error()));
    auto bottom_light_json = liquid.config.get("bottomLightMix");
    if (!bottom_light_json)
      return std::unexpected(std::move(bottom_light_json.error()));
    auto bottom_light = json_to_color(*bottom_light_json);
    if (!bottom_light)
      return std::unexpected(std::move(bottom_light.error()));
    auto movement = liquid.config.get_float("textureMovementFactor");
    if (!movement)
      return std::unexpected(std::move(movement.error()));
    auto stored = painter->m_liquids.set(
      liquid.id, LiquidInfo{std::move(*texture), color->to_rgba(), bottom_light->to_rgb_f(), *movement});
    if (!stored)
      return std::unexpected(std::move(stored.error()));
  }
  return painter;
}

std::expected<void, StarException> TilePainter::adjustLighting(WorldRenderData& renderData) const {
  RectI lightRange = RectI::with_size(renderData.light_min_position, Vec2I(renderData.light_map.size()));
  return for_each_render_tile(renderData, lightRange, [&](Vec2I const& pos, RenderTile const& tile) -> std::expected<void, StarException> {
      // Only adjust lighting for tiles with liquid above the draw threshold
      float drawLevel = liquidDrawLevel(byte_to_float(tile.liquidLevel));
      if (drawLevel == 0.0f)
        return {};

      auto lightIndex = Vec2U(pos - renderData.light_min_position);
      auto lightValue = renderData.light_map.get(lightIndex.x(), lightIndex.y());
      if (!lightValue)
        return std::unexpected(std::move(lightValue.error()));

      auto liquid = m_liquids.at(tile.liquidId);
      if (!liquid)
        return std::unexpected(std::move(liquid.error()));
      float darknessLevel = (1.f - (lightValue->sum() / 3.0f)) * drawLevel;
      auto adjusted = lightValue->piecewise_multiply(Vec3F::filled(1.f - darknessLevel)
                                                     + liquid->get().bottomLightMix * darknessLevel);

      return renderData.light_map.set(lightIndex.x(), lightIndex.y(), adjusted);
    });
}

std::expected<void, StarException> TilePainter::setup(WorldCamera const& camera, WorldRenderData& renderData) {
  auto cameraCenter = camera.centerWorldPosition();
  if (m_lastCameraCenter)
    m_cameraPan = renderData.geometry.diff(cameraCenter, *m_lastCameraCenter);
  m_lastCameraCenter = cameraCenter;

  //Kae: Padded by one to fix culling issues with certain tile pieces at chunk borders, such as grass.
  RectI chunkRange = RectI::integral(RectF(camera.worldTileRect().padded(1)).scaled(1.0f / RenderChunkSize));

  std::size_t chunks = chunkRange.volume();
  m_setupTerrainChunks.resize(chunks);
  m_setupLiquidChunks.resize(chunks);

  // Keep the previous pending frame intact on failure, but retain the staging
  // storage between frames instead of allocating two lists on every cache hit.
  auto prepared = [&]() -> std::expected<void, StarException> {
    std::size_t i = 0;
    for (int x = chunkRange.x_min(); x < chunkRange.x_max(); ++x) {
      for (int y = chunkRange.y_min(); y < chunkRange.y_max(); ++y) {
        std::size_t index = i++;
        auto hashes = chunkHashes(renderData, {x, y});
        if (!hashes)
          return std::unexpected(std::move(hashes.error()));
        auto terrain = getTerrainChunk(renderData, {x, y}, hashes->first);
        if (!terrain)
          return std::unexpected(std::move(terrain.error()));
        auto liquid = getLiquidChunk(renderData, {x, y}, hashes->second);
        if (!liquid)
          return std::unexpected(std::move(liquid.error()));
        m_setupTerrainChunks[index] = std::move(*terrain);
        m_setupLiquidChunks[index] = std::move(*liquid);
      }
    }
    m_pendingTerrainChunks.swap(m_setupTerrainChunks);
    m_pendingLiquidChunks.swap(m_setupLiquidChunks);
    return {};
  }();
  m_setupTerrainChunks.clear();
  m_setupLiquidChunks.clear();
  m_setupTextures.clear();
  return prepared;
}

std::expected<void, StarException> TilePainter::renderBackground(WorldCamera const& camera) {
  return renderTerrainChunks(camera, TerrainLayer::Background);
}

std::expected<void, StarException> TilePainter::renderMidground(WorldCamera const& camera) {
  return renderTerrainChunks(camera, TerrainLayer::Midground);
}

std::expected<void, StarException> TilePainter::renderLiquid(WorldCamera const& camera) {
  Mat3F transformation = Mat3F::identity();
  transformation.translate(-Vec2F(camera.worldTileRect().min()));
  transformation.scale(tile_pixels * camera.pixelRatio());
  transformation.translate(camera.tileMinScreen());

  for (auto const& chunk : m_pendingLiquidChunks) {
    for (auto const& p : *chunk) {
      auto rendered = m_renderer->renderBuffer(p.second, transformation);
      if (!rendered)
        return std::unexpected(std::move(rendered.error()));
    }
  }

  return m_renderer->flush();
}

std::expected<void, StarException> TilePainter::renderForeground(WorldCamera const& camera) {
  return renderTerrainChunks(camera, TerrainLayer::Foreground);
}

void TilePainter::cleanup() {
  m_pendingTerrainChunks.clear();
  m_pendingLiquidChunks.clear();

  m_textureCache.cleanup();
  m_terrainChunkCache.cleanup();
  m_liquidChunkCache.cleanup();
}

std::size_t TilePainter::TextureKeyHash::operator()(TextureKey const& key) const {
  if (std::holds_alternative<MaterialPieceTextureKey>(key))
    return hash_of(static_cast<std::uint8_t>(key.index()), std::get<MaterialPieceTextureKey>(key));
  else
    return hash_of(static_cast<std::uint8_t>(key.index()), std::get<AssetTextureKey>(key));
}

std::expected<std::pair<TilePainter::ChunkHash, TilePainter::ChunkHash>, StarException>
TilePainter::chunkHashes(WorldRenderData const& renderData, Vec2I chunkIndex) {
  constexpr std::size_t terrainBytes = offsetof(RenderTile, liquidId);
  constexpr std::size_t liquidBytes = sizeof(RenderTile::liquidId) + sizeof(RenderTile::liquidLevel);
  auto terrainBuffer = m_terrainHashBuffer.span();
  auto liquidBuffer = m_liquidHashBuffer.span();
  std::size_t terrainOffset = 0;
  std::size_t liquidOffset = 0;
  RectI tileRange = RectI::with_size(chunkIndex * RenderChunkSize, Vec2I::filled(RenderChunkSize)).padded(material_render_profile_max_neighbor_distance);
  auto traversed = for_each_render_tile(
    renderData, tileRange, [&](Vec2I const&, RenderTile const& tile) -> std::expected<void, StarException> {
      // Preserve terrain bytes and traversal order exactly. Both fixed-capacity
      // scratch buffers are sized at construction; no per-tile growth is needed.
      std::memcpy(terrainBuffer.subspan(terrainOffset, terrainBytes).data(), &tile, terrainBytes);
      terrainOffset += terrainBytes;
      std::memcpy(liquidBuffer.subspan(liquidOffset, liquidBytes).data(), &tile.liquidId, liquidBytes);
      liquidOffset += liquidBytes;
      // Liquid geometry depends on whether the tile immediately below is empty.
      // Include only that foreground property, not material identity/mod/hue.
      liquidBuffer[liquidOffset++] = tile.foreground == empty_material_id;
      return {};
    });
  if (!traversed)
    return std::unexpected(std::move(traversed.error()));
  return std::pair{xx_hash3(terrainBuffer.first(terrainOffset)), xx_hash3(liquidBuffer.first(liquidOffset))};
}

std::expected<void, StarException> TilePainter::renderTerrainChunks(WorldCamera const& camera, TerrainLayer terrainLayer) {
  Map<QuadZLevel, List<std::shared_ptr<RenderBuffer>>> zOrderBuffers;
  for (auto const& chunk : m_pendingTerrainChunks) {
    if (auto layer = chunk->find_ref(terrainLayer)) {
      for (auto const& pair : layer->get())
        zOrderBuffers[pair.first].push_back(pair.second);
    }
  }

  Mat3F transformation = Mat3F::identity();
  transformation.translate(-Vec2F(camera.worldTileRect().min()));
  transformation.scale(tile_pixels * camera.pixelRatio());
  transformation.translate(camera.tileMinScreen());

  for (auto const& pair : zOrderBuffers) {
    for (auto const& buffer : pair.second) {
      auto rendered = m_renderer->renderBuffer(buffer, transformation);
      if (!rendered)
        return std::unexpected(std::move(rendered.error()));
    }
  }

  return m_renderer->flush();
}

std::expected<std::shared_ptr<TilePainter::TerrainChunk const>, StarException>
TilePainter::getTerrainChunk(WorldRenderData& renderData, Vec2I chunkIndex, ChunkHash hash) {
  std::pair<Vec2I, ChunkHash> chunkKey = {chunkIndex, hash};
  auto cached = m_terrainChunkCache.get(chunkKey, [&](auto const&) -> std::expected<std::shared_ptr<TerrainChunk const>, StarException> {
      auto assets = m_root.assets();
      if (!assets)
        return std::unexpected(std::move(assets.error()));
      auto material_database = m_root.material_database();
      if (!material_database)
        return std::unexpected(std::move(material_database.error()));
      HashMap<TerrainLayer, HashMap<QuadZLevel, List<RenderPrimitive>>> terrainPrimitives;
      // Matching appends to this per-chunk scratch list. Reuse its capacity
      // across tiles/layers; the material catalog owns every borrowed piece.
      MaterialPieceResultList pieces;

      RectI tileRange = RectI::with_size(chunkIndex * RenderChunkSize, Vec2I::filled(RenderChunkSize));
      for (int x = tileRange.x_min(); x < tileRange.x_max(); ++x) {
        for (int y = tileRange.y_min(); y < tileRange.y_max(); ++y) {
          auto foreground = produceTerrainPrimitives(terrainPrimitives[TerrainLayer::Foreground], pieces, TerrainLayer::Foreground, {x, y}, renderData, *assets, **material_database);
          if (!foreground)
            return std::unexpected(std::move(foreground.error()));
          auto midground = produceTerrainPrimitives(terrainPrimitives[TerrainLayer::Midground], pieces, TerrainLayer::Midground, {x, y}, renderData, *assets, **material_database);
          if (!midground)
            return std::unexpected(std::move(midground.error()));
          if (!*foreground && !*midground) {
            auto background = produceTerrainPrimitives(terrainPrimitives[TerrainLayer::Background], pieces, TerrainLayer::Background, {x, y}, renderData, *assets, **material_database);
            if (!background)
              return std::unexpected(std::move(background.error()));
          }
        }
      }

      auto chunk = std::make_shared<TerrainChunk>();

      for (auto&& layerPair : terrainPrimitives) {
        for (auto&& zLevelPair : layerPair.second) {
          auto rb = m_renderer->createRenderBuffer();
          if (!rb)
            return std::unexpected(std::move(rb.error()));
          auto populated = (*rb)->set(zLevelPair.second);
          if (!populated)
            return std::unexpected(std::move(populated.error()));
          (*chunk)[layerPair.first][zLevelPair.first] = std::move(*rb);
        }
      }

      return std::shared_ptr<TerrainChunk const>(std::move(chunk));
    });
  if (!cached)
    return std::unexpected(std::move(cached.error()));
  return cached->get();
}

std::expected<std::shared_ptr<TilePainter::LiquidChunk const>, StarException>
TilePainter::getLiquidChunk(WorldRenderData& renderData, Vec2I chunkIndex, ChunkHash hash) {
  std::pair<Vec2I, ChunkHash> chunkKey = {chunkIndex, hash};
  auto cached = m_liquidChunkCache.get(chunkKey, [&](auto const&) -> std::expected<std::shared_ptr<LiquidChunk const>, StarException> {
      HashMap<LiquidId, List<RenderPrimitive>> liquidPrimitives;

      RectI tileRange = RectI::with_size(chunkIndex * RenderChunkSize, Vec2I::filled(RenderChunkSize));
      for (int x = tileRange.x_min(); x < tileRange.x_max(); ++x) {
        for (int y = tileRange.y_min(); y < tileRange.y_max(); ++y) {
          auto produced = produceLiquidPrimitives(liquidPrimitives, {x, y}, renderData);
          if (!produced)
            return std::unexpected(std::move(produced.error()));
        }
      }

      auto chunk = std::make_shared<LiquidChunk>();

      for (auto&& p : liquidPrimitives) {
        auto rb = m_renderer->createRenderBuffer();
        if (!rb)
          return std::unexpected(std::move(rb.error()));
        auto populated = (*rb)->set(p.second);
        if (!populated)
          return std::unexpected(std::move(populated.error()));
        chunk->set(p.first, std::move(*rb));
      }

      return std::shared_ptr<LiquidChunk const>(std::move(chunk));
    });
  if (!cached)
    return std::unexpected(std::move(cached.error()));
  return cached->get();
}

std::expected<TexturePtr, StarException> TilePainter::setupTexture(std::shared_ptr<Image const> const& image) {
  auto cached = m_setupTextures.find(image);
  if (cached != m_setupTextures.end())
    return cached->second;
  auto texture = m_textureGroup->create(*image);
  if (!texture)
    return std::unexpected(std::move(texture.error()));
  m_setupTextures.insert(image, *texture);
  return std::move(*texture);
}

std::expected<bool, StarException> TilePainter::produceTerrainPrimitives(HashMap<QuadZLevel, List<RenderPrimitive>>& primitives,
    MaterialPieceResultList& pieces, TerrainLayer terrain_layer, Vec2I const& pos, WorldRenderData const& render_data,
    std::shared_ptr<Assets const> const& assets, MaterialDatabase const& material_database) {

  RenderTile const& tile = get_render_tile(render_data, pos);

  MaterialId material = empty_material_id;
  MaterialHue material_hue = 0;
  MaterialColorVariant color_variant = 0;
  ModId mod = no_mod_id;
  MaterialHue mod_hue = 0;
  float damage_level = 0.0f;
  TileDamageType damage_type = TileDamageType::Protected;
  Vec4B color;

  bool occlude = false;

  if (terrain_layer == TerrainLayer::Background) {
    material = tile.background;
    material_hue = tile.backgroundHueShift;
    color_variant = tile.backgroundColorVariant;
    mod = tile.backgroundMod;
    mod_hue = tile.backgroundModHueShift;
    damage_level = byte_to_float(tile.backgroundDamageLevel);
    damage_type = tile.backgroundDamageType;
    color = m_background_layer_color;
  } else {
    material = tile.foreground;
    material_hue = tile.foregroundHueShift;
    color_variant = tile.foregroundColorVariant;
    mod = tile.foregroundMod;
    mod_hue = tile.foregroundModHueShift;
    damage_level = byte_to_float(tile.foregroundDamageLevel);
    damage_type = tile.foregroundDamageType;
    color = m_foreground_layer_color;
  }

  // render non-block colliding things in the midground
  auto collision_kind = material_database.material_collision_kind(material);
  if (!collision_kind)
    return std::unexpected(std::move(collision_kind.error()));
  bool is_block = BlockCollisionSet.contains(*collision_kind);
  if (terrain_layer == (is_block ? TerrainLayer::Midground : TerrainLayer::Foreground))
    return false;

  auto draw_profile = [&](MaterialRenderProfile const& profile, MaterialId id, MaterialHue hue, std::uint32_t variance,
                          bool is_mod) -> std::expected<void, StarException> {
    auto selected_color = profile.world_color_variant(color_variant);
    auto& quads = primitives[is_mod ? mod_z_level(profile.z_level, id, hue, selected_color)
                                    : material_z_level(profile.z_level, id, hue, selected_color)];
    pieces.clear();
    auto matching = determine_matching_pieces(pieces, std::ref(occlude), material_database, profile, render_data, pos,
        terrain_layer == TerrainLayer::Background ? TileLayer::Background : TileLayer::Foreground, is_mod);
    if (!matching)
      return std::unexpected(std::move(matching.error()));
    if (pieces.empty())
      return {};

    // Wrapping an 8-bit color index cannot produce a larger slot.
    auto directive_slot = static_cast<MaterialColorVariant>(profile.color_directive_slot(selected_color));
    for (auto const& [piece_ref, offset] : pieces) {
      auto const& piece = piece_ref.get();
      auto cached = m_textureCache.get(MaterialPieceTextureKey(piece.piece_id, id, hue, directive_slot, is_mod),
          [&](auto const&) -> std::expected<TexturePtr, StarException> {
            AssetPath path = hue == 0 ? piece.texture : Star::format("{}?hueshift={}", piece.texture, material_hue_to_degrees(hue));
            if (!profile.color_directives.empty())
              path.directives += profile.color_directives[directive_slot];
            auto image = assets->image(path);
            if (!image)
              return std::unexpected(std::move(image.error()));
            return setupTexture(*image);
          });
      if (!cached)
        return std::unexpected(std::move(cached.error()));
      auto rectangle = piece.world_rectangle(variance, selected_color);
      if (!rectangle)
        continue;
      auto const& texture_coords = rectangle->get();
      auto world_coords = RectF::with_size(offset / tile_pixels + Vec2F(pos), texture_coords.size() / tile_pixels);
      quads.emplace_back(std::in_place_type_t<RenderQuad>(), cached->get(),
          world_coords.min(), texture_coords.min(),
          Vec2F(world_coords.x_max(), world_coords.y_min()), Vec2F(texture_coords.x_max(), texture_coords.y_min()),
          world_coords.max(), texture_coords.max(),
          Vec2F(world_coords.x_min(), world_coords.y_max()), Vec2F(texture_coords.x_min(), texture_coords.y_max()),
          color, 1.0f);
    }
    return {};
  };

  auto material_profile = material_database.material_render_profile(material);
  if (material_profile) {
    auto const& profile = material_profile->get();
    occlude = profile.occludes_behind;
    auto variance = static_random_u32(render_data.geometry.xwrap(pos[0]), pos[1], static_cast<int>(terrain_layer), "main");
    auto drawn = draw_profile(profile, material, material_hue, variance, false);
    if (!drawn)
      return std::unexpected(std::move(drawn.error()));
  }

  if (auto mod_profile = material_database.mod_render_profile(mod)) {
    auto variance = static_random_u32(render_data.geometry.xwrap(pos[0]), pos[1], static_cast<int>(terrain_layer), "mod");
    auto drawn = draw_profile(mod_profile->get(), mod, mod_hue, variance, true);
    if (!drawn)
      return std::unexpected(std::move(drawn.error()));
  }

  if (material_profile && damage_level > 0 && is_block) {
    auto& quads = primitives[damage_z_level()];
    auto cracking_image = material_profile->get().damage_image(damage_level, damage_type);
    if (!cracking_image)
      return std::unexpected(std::move(cracking_image.error()));

    auto cached = m_textureCache.get(AssetTextureKey(cracking_image->get().first),
        [&](auto const&) -> std::expected<TexturePtr, StarException> {
          auto image = assets->image(cracking_image->get().first);
          if (!image)
            return std::unexpected(std::move(image.error()));
          return setupTexture(*image);
        });
    if (!cached)
      return std::unexpected(std::move(cached.error()));
    TexturePtr texture = cached->get();

    Vec2F texture_size(texture->size());
    RectF texture_coords = RectF::with_size(Vec2F(), texture_size);
    RectF world_coords = RectF::with_size(cracking_image->get().second / tile_pixels + Vec2F(pos), texture_coords.size() / tile_pixels);

    quads.emplace_back(std::in_place_type_t<RenderQuad>(), std::move(texture),
        world_coords.min(), texture_coords.min(),
        Vec2F(world_coords.x_max(), world_coords.y_min()), Vec2F(texture_coords.x_max(), texture_coords.y_min()),
        world_coords.max(), texture_coords.max(),
        Vec2F(world_coords.x_min(), world_coords.y_max()), Vec2F(texture_coords.x_min(), texture_coords.y_max()),
        color, 1.0f);
  }

  return occlude;
}

std::expected<void, StarException> TilePainter::produceLiquidPrimitives(HashMap<LiquidId, List<RenderPrimitive>>& primitives, Vec2I const& pos, WorldRenderData const& renderData) {
  RenderTile const& tile = get_render_tile(renderData, pos);

  float drawLevel = liquidDrawLevel(byte_to_float(tile.liquidLevel));
  if (drawLevel <= 0.0f)
    return {};

  RenderTile const& tileBottom = get_render_tile(renderData, pos - Vec2I(0, 1));
  float bottomDrawLevel = liquidDrawLevel(byte_to_float(tileBottom.liquidLevel));

  RectF worldRect;
  if (tileBottom.foreground == empty_material_id && bottomDrawLevel < 1.0f)
    worldRect = RectF::with_size(Vec2F(pos), Vec2F::filled(1.0f)).expanded(drawLevel);
  else
    worldRect = RectF::with_size(Vec2F(pos), Vec2F(1.0f, drawLevel));

  auto texRect = worldRect.scaled(tile_pixels);

  auto liquid = m_liquids.at(tile.liquidId);
  if (!liquid)
    return std::unexpected(std::move(liquid.error()));
  primitives[tile.liquidId].emplace_back(std::in_place_type_t<RenderQuad>(), liquid->get().texture,
      worldRect.min(), texRect.min(),
      Vec2F(worldRect.x_max(), worldRect.y_min()), Vec2F(texRect.x_max(), texRect.y_min()),
      worldRect.max(), texRect.max(),
      Vec2F(worldRect.x_min(), worldRect.y_max()), Vec2F(texRect.x_min(), texRect.y_max()),
    liquid->get().color, 1.0f);
  return {};
}

float TilePainter::liquidDrawLevel(float liquidLevel) const {
  return std::clamp((liquidLevel - m_liquid_draw_levels[0]) / (m_liquid_draw_levels[1] - m_liquid_draw_levels[0]), 0.0f,
                    1.0f);
}

}

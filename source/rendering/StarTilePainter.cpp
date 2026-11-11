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
  constexpr std::size_t hashWidth = RenderChunkSize + 2 * MaterialRenderProfileMaxNeighborDistance;
  m_terrainHashBuffer.resize(hashWidth * hashWidth * offsetof(RenderTile, liquidId));
  m_liquidHashBuffer.resize(hashWidth * hashWidth
                            * (sizeof(RenderTile::liquidId) + sizeof(RenderTile::liquidLevel) + 1));
}

std::expected<std::shared_ptr<TilePainter>, StarException> TilePainter::create(Root& root, std::shared_ptr<Renderer> renderer) {
  auto painter = std::shared_ptr<TilePainter>(new TilePainter(root, std::move(renderer)));
  auto initialized = painter->initializeTileDrawer();
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
  auto chunkTimeout = chunkTimeoutJson->toInt();
  if (!chunkTimeout)
    return std::unexpected(std::move(chunkTimeout.error()));
  painter->m_terrainChunkCache.set_time_to_live(std::chrono::milliseconds(*chunkTimeout));
  painter->m_terrainChunkCache.set_time_smear(std::chrono::milliseconds(static_cast<int>(*chunkTimeout / 4)));
  painter->m_liquidChunkCache.set_time_to_live(std::chrono::milliseconds(*chunkTimeout));
  painter->m_liquidChunkCache.set_time_smear(std::chrono::milliseconds(static_cast<int>(*chunkTimeout / 4)));

  auto textureTimeoutJson = (*assets)->json("/rendering.config:textureTimeout");
  if (!textureTimeoutJson)
    return std::unexpected(std::move(textureTimeoutJson.error()));
  auto textureTimeout = textureTimeoutJson->toInt();
  if (!textureTimeout)
    return std::unexpected(std::move(textureTimeout.error()));
  painter->m_textureCache.set_time_to_live(std::chrono::milliseconds(*textureTimeout));

  auto liquids = root.liquidsDatabase();
  if (!liquids)
    return std::unexpected(std::move(liquids.error()));
  for (auto const& liquid : (*liquids)->allLiquidSettings()) {
    auto texturePath = liquid->config.getString("texture");
    if (!texturePath)
      return std::unexpected(std::move(texturePath.error()));
    auto image = (*assets)->image(*texturePath);
    if (!image)
      return std::unexpected(std::move(image.error()));
    auto texture = painter->m_renderer->createTexture(**image, TextureAddressing::Wrap);
    if (!texture)
      return std::unexpected(std::move(texture.error()));
    auto colorJson = liquid->config.get("color");
    if (!colorJson)
      return std::unexpected(std::move(colorJson.error()));
    auto color = jsonToColor(*colorJson);
    if (!color)
      return std::unexpected(std::move(color.error()));
    auto bottomLightJson = liquid->config.get("bottomLightMix");
    if (!bottomLightJson)
      return std::unexpected(std::move(bottomLightJson.error()));
    auto bottomLight = jsonToColor(*bottomLightJson);
    if (!bottomLight)
      return std::unexpected(std::move(bottomLight.error()));
    auto movement = liquid->config.getFloat("textureMovementFactor");
    if (!movement)
      return std::unexpected(std::move(movement.error()));
    auto stored = painter->m_liquids.set(
      liquid->id, LiquidInfo{std::move(*texture), color->toRgba(), bottomLight->toRgbF(), *movement});
    if (!stored)
      return std::unexpected(std::move(stored.error()));
  }
  return painter;
}

std::expected<void, StarException> TilePainter::adjustLighting(WorldRenderData& renderData) const {
  RectI lightRange = RectI::withSize(renderData.lightMinPosition, Vec2I(renderData.lightMap.size()));
  return forEachRenderTile(renderData, lightRange, [&](Vec2I const& pos, RenderTile const& tile) -> std::expected<void, StarException> {
      // Only adjust lighting for tiles with liquid above the draw threshold
      float drawLevel = liquidDrawLevel(byteToFloat(tile.liquidLevel));
      if (drawLevel == 0.0f)
        return {};

      auto lightIndex = Vec2U(pos - renderData.lightMinPosition);
      auto lightValue = renderData.lightMap.get(lightIndex.x(), lightIndex.y());
      if (!lightValue)
        return std::unexpected(std::move(lightValue.error()));

      auto liquid = m_liquids.at(tile.liquidId);
      if (!liquid)
        return std::unexpected(std::move(liquid.error()));
      float darknessLevel = (1.f - (lightValue->sum() / 3.0f)) * drawLevel;
      auto adjusted = lightValue->piecewise_multiply(Vec3F::filled(1.f - darknessLevel)
                                                     + liquid->get().bottomLightMix * darknessLevel);

      return renderData.lightMap.set(lightIndex.x(), lightIndex.y(), adjusted);
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
    for (int x = chunkRange.xMin(); x < chunkRange.xMax(); ++x) {
      for (int y = chunkRange.yMin(); y < chunkRange.yMax(); ++y) {
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
  transformation.scale(TilePixels * camera.pixelRatio());
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
  RectI tileRange = RectI::withSize(chunkIndex * RenderChunkSize, Vec2I::filled(RenderChunkSize)).padded(MaterialRenderProfileMaxNeighborDistance);
  auto traversed = forEachRenderTile(
    renderData, tileRange, [&](Vec2I const&, RenderTile const& tile) -> std::expected<void, StarException> {
      // Preserve terrain bytes and traversal order exactly. Both fixed-capacity
      // scratch buffers are sized at construction; no per-tile growth is needed.
      std::memcpy(terrainBuffer.subspan(terrainOffset, terrainBytes).data(), &tile, terrainBytes);
      terrainOffset += terrainBytes;
      std::memcpy(liquidBuffer.subspan(liquidOffset, liquidBytes).data(), &tile.liquidId, liquidBytes);
      liquidOffset += liquidBytes;
      // Liquid geometry depends on whether the tile immediately below is empty.
      // Include only that foreground property, not material identity/mod/hue.
      liquidBuffer[liquidOffset++] = tile.foreground == EmptyMaterialId;
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
  transformation.scale(TilePixels * camera.pixelRatio());
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
      auto materialDatabase = m_root.materialDatabase();
      if (!materialDatabase)
        return std::unexpected(std::move(materialDatabase.error()));
      HashMap<TerrainLayer, HashMap<QuadZLevel, List<RenderPrimitive>>> terrainPrimitives;
      // Matching appends to this per-chunk scratch list. Reuse its capacity
      // across tiles/layers without retaining piece owners beyond this miss.
      MaterialPieceResultList pieces;

      RectI tileRange = RectI::withSize(chunkIndex * RenderChunkSize, Vec2I::filled(RenderChunkSize));
      for (int x = tileRange.xMin(); x < tileRange.xMax(); ++x) {
        for (int y = tileRange.yMin(); y < tileRange.yMax(); ++y) {
          auto foreground = produceTerrainPrimitives(terrainPrimitives[TerrainLayer::Foreground], pieces, TerrainLayer::Foreground, {x, y}, renderData, *assets, *materialDatabase);
          if (!foreground)
            return std::unexpected(std::move(foreground.error()));
          auto midground = produceTerrainPrimitives(terrainPrimitives[TerrainLayer::Midground], pieces, TerrainLayer::Midground, {x, y}, renderData, *assets, *materialDatabase);
          if (!midground)
            return std::unexpected(std::move(midground.error()));
          if (!*foreground && !*midground) {
            auto background = produceTerrainPrimitives(terrainPrimitives[TerrainLayer::Background], pieces, TerrainLayer::Background, {x, y}, renderData, *assets, *materialDatabase);
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

      RectI tileRange = RectI::withSize(chunkIndex * RenderChunkSize, Vec2I::filled(RenderChunkSize));
      for (int x = tileRange.xMin(); x < tileRange.xMax(); ++x) {
        for (int y = tileRange.yMin(); y < tileRange.yMax(); ++y) {
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
    MaterialPieceResultList& pieces, TerrainLayer terrainLayer, Vec2I const& pos, WorldRenderData const& renderData,
    std::shared_ptr<Assets const> const& assets, std::shared_ptr<MaterialDatabase const> const& materialDatabase) {

  RenderTile const& tile = getRenderTile(renderData, pos);

  MaterialId material = EmptyMaterialId;
  MaterialHue materialHue = 0;
  MaterialColorVariant colorVariant = 0;
  ModId mod = NoModId;
  MaterialHue modHue = 0;
  float damageLevel = 0.0f;
  TileDamageType damageType = TileDamageType::Protected;
  Vec4B color;

  bool occlude = false;

  if (terrainLayer == TerrainLayer::Background) {
    material = tile.background;
    materialHue = tile.backgroundHueShift;
    colorVariant = tile.backgroundColorVariant;
    mod = tile.backgroundMod;
    modHue = tile.backgroundModHueShift;
    damageLevel = byteToFloat(tile.backgroundDamageLevel);
    damageType = tile.backgroundDamageType;
    color = m_backgroundLayerColor;
  } else {
    material = tile.foreground;
    materialHue = tile.foregroundHueShift;
    colorVariant = tile.foregroundColorVariant;
    mod = tile.foregroundMod;
    modHue = tile.foregroundModHueShift;
    damageLevel = byteToFloat(tile.foregroundDamageLevel);
    damageType = tile.foregroundDamageType;
    color = m_foregroundLayerColor;
  }

  // render non-block colliding things in the midground
  auto collisionKind = materialDatabase->materialCollisionKind(material);
  if (!collisionKind)
    return std::unexpected(std::move(collisionKind.error()));
  bool isBlock = BlockCollisionSet.contains(*collisionKind);
  if (terrainLayer == (isBlock ? TerrainLayer::Midground : TerrainLayer::Foreground))
    return false;

  auto getPieceTexture = [this, &assets](MaterialId material, std::shared_ptr<MaterialRenderPiece const> const& piece, MaterialHue hue, std::optional<std::reference_wrapper<Directives const>> directives, bool mod) -> std::expected<TexturePtr, StarException> {
    auto cached = m_textureCache.get(MaterialPieceTextureKey(material, piece->pieceId, hue, mod), [&](auto const&) -> std::expected<TexturePtr, StarException> {
        AssetPath texture = (hue == 0) ? piece->texture : Star::format("{}?hueshift={}", piece->texture, materialHueToDegrees(hue));

        if (directives)
          texture.directives += directives->get();

        auto image = assets->image(texture);
        if (!image)
          return std::unexpected(std::move(image.error()));
        return setupTexture(*image);
      });
    if (!cached)
      return std::unexpected(std::move(cached.error()));
    return cached->get();
  };

  auto materialRenderProfile = materialDatabase->materialRenderProfile(material);
  auto modRenderProfile = materialDatabase->modRenderProfile(mod);

  if (materialRenderProfile) {
    occlude = materialRenderProfile->occludesBehind;
    auto materialColorVariant = materialRenderProfile->colorVariants > 0 ? colorVariant % materialRenderProfile->colorVariants : 0;
    std::uint32_t variance = static_random_u32(renderData.geometry.xwrap(pos[0]), pos[1], (int)terrainLayer, "main");
    auto& quadList = primitives[materialZLevel(materialRenderProfile->zLevel, material, materialHue, materialColorVariant)];

    pieces.clear();
    auto matching = determineMatchingPieces(pieces, std::ref(occlude), materialDatabase, materialRenderProfile->mainMatchList, renderData, pos,
        terrainLayer == TerrainLayer::Background ? TileLayer::Background : TileLayer::Foreground, false);
    if (!matching)
      return std::unexpected(std::move(matching.error()));
    std::optional<std::reference_wrapper<Directives const>> directives;
    if (!materialRenderProfile->colorDirectives.empty()) {
      auto selected = materialRenderProfile->colorDirectives.wrap(materialColorVariant);
      if (!selected)
        return std::unexpected(std::move(selected.error()));
      directives = *selected;
    }
    for (auto const& piecePair : pieces) {
      auto texture = getPieceTexture(material, piecePair.first, materialHue, directives, false);
      if (!texture)
        return std::unexpected(std::move(texture.error()));
      auto variant = piecePair.first->variants.find_ref(materialColorVariant);
      if (!variant)
        variant = piecePair.first->variants.find_ref(0);
      if (!variant) continue;
      auto selectedCoords = variant->get().wrap(variance);
      if (!selectedCoords)
        return std::unexpected(std::move(selectedCoords.error()));
      RectF const& textureCoords = selectedCoords->get();
      RectF worldCoords = RectF::withSize(piecePair.second / TilePixels + Vec2F(pos), textureCoords.size() / TilePixels);
      quadList.emplace_back(std::in_place_type_t<RenderQuad>(), std::move(*texture),
          worldCoords  .min(),
          textureCoords.min(),
          Vec2F(  worldCoords.xMax(),   worldCoords.yMin()),
          Vec2F(textureCoords.xMax(), textureCoords.yMin()),
          worldCoords  .max(),
          textureCoords.max(),
          Vec2F(  worldCoords.xMin(),   worldCoords.yMax()),
          Vec2F(textureCoords.xMin(), textureCoords.yMax()),
          color, 1.0f);
    }
  }

  if (modRenderProfile) {
    auto modColorVariant = modRenderProfile->colorVariants > 0 ? colorVariant % modRenderProfile->colorVariants : 0;
    std::uint32_t variance = static_random_u32(renderData.geometry.xwrap(pos[0]), pos[1], (int)terrainLayer, "mod");
    auto& quadList = primitives[modZLevel(modRenderProfile->zLevel, mod, modHue, modColorVariant)];

    pieces.clear();
    auto matching = determineMatchingPieces(pieces, std::ref(occlude), materialDatabase, modRenderProfile->mainMatchList, renderData, pos,
        terrainLayer == TerrainLayer::Background ? TileLayer::Background : TileLayer::Foreground, true);
    if (!matching)
      return std::unexpected(std::move(matching.error()));
    std::optional<std::reference_wrapper<Directives const>> directives;
    if (!modRenderProfile->colorDirectives.empty()) {
      auto selected = modRenderProfile->colorDirectives.wrap(modColorVariant);
      if (!selected)
        return std::unexpected(std::move(selected.error()));
      directives = *selected;
    }
    for (auto const& piecePair : pieces) {
      auto texture = getPieceTexture(mod, piecePair.first, modHue, directives, true);
      if (!texture)
        return std::unexpected(std::move(texture.error()));
      auto variant = piecePair.first->variants.find_ref(modColorVariant);
      if (!variant)
        variant = piecePair.first->variants.find_ref(0);
      if (!variant) continue;
      auto selectedCoords = variant->get().wrap(variance);
      if (!selectedCoords)
        return std::unexpected(std::move(selectedCoords.error()));
      RectF const& textureCoords = selectedCoords->get();
      RectF worldCoords = RectF::withSize(piecePair.second / TilePixels + Vec2F(pos), textureCoords.size() / TilePixels);
      quadList.emplace_back(std::in_place_type_t<RenderQuad>(), std::move(*texture),
          worldCoords.min(), textureCoords.min(),
          Vec2F(worldCoords.xMax(), worldCoords.yMin()), Vec2F(textureCoords.xMax(), textureCoords.yMin()),
          worldCoords.max(), textureCoords.max(),
          Vec2F(worldCoords.xMin(), worldCoords.yMax()), Vec2F(textureCoords.xMin(), textureCoords.yMax()),
        color, 1.0f);
    }
  }

  if (materialRenderProfile && damageLevel > 0 && isBlock) {
    auto& quadList = primitives[damageZLevel()];
    auto crackingImage = materialRenderProfile->damageImage(damageLevel, damageType);
    if (!crackingImage)
      return std::unexpected(std::move(crackingImage.error()));

    auto cached = m_textureCache.get(AssetTextureKey(crackingImage->get().first),
        [&](auto const&) -> std::expected<TexturePtr, StarException> {
          auto image = assets->image(crackingImage->get().first);
          if (!image)
            return std::unexpected(std::move(image.error()));
          return setupTexture(*image);
        });
    if (!cached)
      return std::unexpected(std::move(cached.error()));
    TexturePtr texture = cached->get();

    Vec2F textureSize(texture->size());
    RectF textureCoords = RectF::withSize(Vec2F(), textureSize);
    RectF worldCoords = RectF::withSize(crackingImage->get().second / TilePixels + Vec2F(pos), textureCoords.size() / TilePixels);

    quadList.emplace_back(std::in_place_type_t<RenderQuad>(), std::move(texture),
        worldCoords.min(), textureCoords.min(),
        Vec2F(worldCoords.xMax(), worldCoords.yMin()), Vec2F(textureCoords.xMax(), textureCoords.yMin()),
        worldCoords.max(), textureCoords.max(),
        Vec2F(worldCoords.xMin(), worldCoords.yMax()), Vec2F(textureCoords.xMin(), textureCoords.yMax()),
      color, 1.0f);
  }

  return occlude;
}

std::expected<void, StarException> TilePainter::produceLiquidPrimitives(HashMap<LiquidId, List<RenderPrimitive>>& primitives, Vec2I const& pos, WorldRenderData const& renderData) {
  RenderTile const& tile = getRenderTile(renderData, pos);

  float drawLevel = liquidDrawLevel(byteToFloat(tile.liquidLevel));
  if (drawLevel <= 0.0f)
    return {};

  RenderTile const& tileBottom = getRenderTile(renderData, pos - Vec2I(0, 1));
  float bottomDrawLevel = liquidDrawLevel(byteToFloat(tileBottom.liquidLevel));

  RectF worldRect;
  if (tileBottom.foreground == EmptyMaterialId && bottomDrawLevel < 1.0f)
    worldRect = RectF::withSize(Vec2F(pos), Vec2F::filled(1.0f)).expanded(drawLevel);
  else
    worldRect = RectF::withSize(Vec2F(pos), Vec2F(1.0f, drawLevel));

  auto texRect = worldRect.scaled(TilePixels);

  auto liquid = m_liquids.at(tile.liquidId);
  if (!liquid)
    return std::unexpected(std::move(liquid.error()));
  primitives[tile.liquidId].emplace_back(std::in_place_type_t<RenderQuad>(), liquid->get().texture,
      worldRect.min(), texRect.min(),
      Vec2F(worldRect.xMax(), worldRect.yMin()), Vec2F(texRect.xMax(), texRect.yMin()),
      worldRect.max(), texRect.max(),
      Vec2F(worldRect.xMin(), worldRect.yMax()), Vec2F(texRect.xMin(), texRect.yMax()),
    liquid->get().color, 1.0f);
  return {};
}

float TilePainter::liquidDrawLevel(float liquidLevel) const {
  return std::clamp((liquidLevel - m_liquidDrawLevels[0]) / (m_liquidDrawLevels[1] - m_liquidDrawLevels[0]), 0.0f,
                    1.0f);
}

}

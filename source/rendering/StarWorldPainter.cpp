import std;
import star.format;
import star.xx_hash;
import star.json;
import star.json_extra;
import star.id_map;
import star.game_types;
import star.interpolation;
import star.image;
import star.list;
import star.map;
import star.math_common;
import star.vector;
import star.either;
import star.rect;
import star.image_processing;
import star.hash;
import star.data_stream;
import star.string_view;
import star.thread;
import star.directives;
import star.random;
import star.weighted_pool;
import star.casting;
import star.strong_typedef;
import star.ordered_map;
import star.lru_cache;
import star.time;
import star.ttl_cache;
import star.listener;
import star.bi_map;
import star.perlin;
import star.poly;
import star.random_point;
import star.font;
import star.string;
import star.text;
import star.color;
import star.asset_path;
import star.version;
import star.set;
import star.worker_pool;

import star.sector_array_2d;
import star.io_device;
import star.ref_ptr;
import star.logging;

import star.world_geometry;
import star.collision_block;
import star.liquid_types;
import star.tile_damage;
import star.tile_sector_array;
import star.force_regions;
import star.world_parameters;
import star.celestial_parameters;
import star.world_layout;
import star.collision_generator;
import star.world_tiles;
import star.entity_rendering_types;
import star.sky_types;
import star.celestial_coordinate;
import star.sky_parameters;
import star.sky_render_data;
import star.plant_database;
import star.parallax;
import star.particle;
import star.weather_types;
import star.damage_types;
import star.status_types;
import star.damage;
import star.light_source;
import star.entity;
import star.cellular_light_array;
import star.cellular_lighting;
import star.world_render_data;
import star.material_render_profile;
import star.renderer;
import star.tile_drawer;
import star.tile_painter;
import star.asset_texture_group;
import star.environment_painter;
import star.font_texture_group;
import star.anchor_types;
import star.text_painter;
import star.drawable_painter;
import star.world_camera;
import star.world_painter;
import star.drawable;
import star.asset_source;
import star.assets;
import star.root_base;
import star.root;
import star.configuration;
import star.animation;

namespace Star {

WorldPainter::WorldPainter(Root& root) : m_root(root) {}

std::expected<std::shared_ptr<WorldPainter>, StarException> WorldPainter::create(Root& root) {
  auto painter = std::shared_ptr<WorldPainter>(new WorldPainter(root));
  auto initialized = painter->initialize();
  if (!initialized)
    return std::unexpected(std::move(initialized.error()));
  return painter;
}

std::expected<void, StarException> WorldPainter::initialize() {
  auto assets = m_root.assets();
  if (!assets) return std::unexpected(std::move(assets.error()));
  m_assets = *assets;
  auto configuration = m_root.configuration();
  if (!configuration) return std::unexpected(std::move(configuration.error()));
  auto zoomJson = (*configuration)->get("zoomLevel");
  if (!zoomJson) return std::unexpected(std::move(zoomJson.error()));
  auto zoom = zoomJson->toFloat();
  if (!zoom) return std::unexpected(std::move(zoom.error()));
  m_camera.setScreenSize({800, 600});
  m_camera.setCenterWorldPosition(Vec2F());
  m_camera.setPixelRatio(*zoom);

  auto highlightConfig = m_assets->json("/highlights.config");
  if (!highlightConfig) return std::unexpected(std::move(highlightConfig.error()));
  m_highlightConfig = std::move(*highlightConfig);
  auto directives = m_highlightConfig.get("highlightDirectives");
  if (!directives) return std::unexpected(std::move(directives.error()));
  auto entries = directives->iterateObject();
  if (!entries) return std::unexpected(std::move(entries.error()));
  for (auto const& p : *entries) {
    auto type = enumValue<EntityHighlightEffectTypeNames>(p.first);
    if (!type) return std::unexpected(std::move(type.error()));
    auto underlay = p.second.getString("underlay", "");
    if (!underlay) return std::unexpected(std::move(underlay.error()));
    auto overlay = p.second.getString("overlay", "");
    if (!overlay) return std::unexpected(std::move(overlay.error()));
    m_highlightDirectives.set(*type, {Directives(*underlay), Directives(*overlay)});
  }
  auto loadVector = [this](String const& path) -> std::expected<Vec2F, StarException> {
    auto json = m_assets->json(path);
    if (!json) return std::unexpected(std::move(json.error()));
    return jsonToVec2F(*json);
  };
  auto offset = loadVector("/rendering.config:entityBarOffset");
  if (!offset) return std::unexpected(std::move(offset.error()));
  m_entityBarOffset = *offset;
  auto spacing = loadVector("/rendering.config:entityBarSpacing");
  if (!spacing) return std::unexpected(std::move(spacing.error()));
  m_entityBarSpacing = *spacing;
  auto size = loadVector("/rendering.config:entityBarSize");
  if (!size) return std::unexpected(std::move(size.error()));
  m_entityBarSize = *size;
  auto iconOffset = loadVector("/rendering.config:entityBarIconOffset");
  if (!iconOffset) return std::unexpected(std::move(iconOffset.error()));
  m_entityBarIconOffset = *iconOffset;
  auto preloadJson = m_assets->json("/rendering.config:preloadTextureChance");
  if (!preloadJson) return std::unexpected(std::move(preloadJson.error()));
  auto preload = preloadJson->toFloat();
  if (!preload) return std::unexpected(std::move(preload.error()));
  m_preloadTextureChance = *preload;
  return {};
}

std::expected<void, StarException> WorldPainter::renderInit(std::shared_ptr<Renderer> renderer) {
  auto assets = m_root.assets();
  if (!assets) return std::unexpected(std::move(assets.error()));
  auto textureGroup = renderer->createTextureGroup(TextureGroupSize::Large);
  if (!textureGroup) return std::unexpected(std::move(textureGroup.error()));
  auto textPainter = TextPainter::create(m_root, renderer, *textureGroup);
  if (!textPainter) return std::unexpected(std::move(textPainter.error()));
  auto tilePainter = TilePainter::create(m_root, renderer);
  if (!tilePainter) return std::unexpected(std::move(tilePainter.error()));
  auto environmentPainter = EnvironmentPainter::create(m_root, renderer);
  if (!environmentPainter) return std::unexpected(std::move(environmentPainter.error()));
  auto drawablePainter = std::make_shared<DrawablePainter>(renderer, std::make_shared<AssetTextureGroup>(m_root, *textureGroup));
  m_assets = std::move(*assets);
  m_renderer = std::move(renderer);
  m_textPainter = std::move(*textPainter);
  m_tilePainter = std::move(*tilePainter);
  m_drawablePainter = std::move(drawablePainter);
  m_environmentPainter = std::move(*environmentPainter);
  return {};
}

void WorldPainter::setCameraPosition(WorldGeometry const& geometry, Vec2F const& position) {
  m_camera.setWorldGeometry(geometry);
  m_camera.setCenterWorldPosition(position);
}

WorldCamera& WorldPainter::camera() {
  return m_camera;
}

std::optional<std::reference_wrapper<TileDrawer>> WorldPainter::tileDrawer() const {
  if (m_tilePainter)
    return std::ref(static_cast<TileDrawer&>(*m_tilePainter));
  return std::nullopt;
}

void WorldPainter::update(float dt) {
  m_environmentPainter->update(dt);
}

std::expected<void, StarException> WorldPainter::render(WorldRenderData& renderData, std::function<std::expected<bool, StarException>()> lightWaiter) {
  m_camera.setScreenSize(m_renderer->screenSize());
  auto configuration = m_root.configuration();
  if (!configuration) return std::unexpected(std::move(configuration.error()));
  auto zoomJson = (*configuration)->get("zoomLevel");
  if (!zoomJson) return std::unexpected(std::move(zoomJson.error()));
  auto zoom = zoomJson->toFloat();
  if (!zoom) return std::unexpected(std::move(zoom.error()));
  m_camera.setTargetPixelRatio(*zoom);
  auto assets = m_root.assets();
  if (!assets) return std::unexpected(std::move(assets.error()));
  m_assets = *assets;
  auto imageMetadata = m_root.imageMetadataDatabase();
  if (!imageMetadata) return std::unexpected(std::move(imageMetadata.error()));
  m_imageMetadata = *imageMetadata;
  if (auto result = m_tilePainter->setup(m_camera, renderData); !result)
    return std::unexpected(std::move(result.error()));

  // Stars, Debris Fields, Sky, and Orbiters

  // Use a fixed pixel ratio for certain things.
  float pixelRatioBasis = m_camera.screenSize()[1] / 1080.0f;
  float starAndDebrisRatio = lerp(0.0625f, pixelRatioBasis * 2.0f, m_camera.pixelRatio());
  float orbiterAndPlanetRatio = lerp(0.125f, pixelRatioBasis * 3.0f, m_camera.pixelRatio());

  if (auto result = m_environmentPainter->renderStars(starAndDebrisRatio, Vec2F(m_camera.screenSize()), renderData.skyRenderData); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = m_environmentPainter->renderDebrisFields(starAndDebrisRatio, Vec2F(m_camera.screenSize()), renderData.skyRenderData); !result)
    return std::unexpected(std::move(result.error()));
  if (renderData.skyRenderData.type != SkyType::Atmosphereless) {
    if (auto result = m_environmentPainter->renderBackOrbiters(orbiterAndPlanetRatio, Vec2F(m_camera.screenSize()), renderData.skyRenderData); !result)
      return std::unexpected(std::move(result.error()));
  }
  if (auto result = m_environmentPainter->renderPlanetHorizon(orbiterAndPlanetRatio, Vec2F(m_camera.screenSize()), renderData.skyRenderData); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = m_environmentPainter->renderSky(Vec2F(m_camera.screenSize()), renderData.skyRenderData); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = m_environmentPainter->renderFrontOrbiters(orbiterAndPlanetRatio, Vec2F(m_camera.screenSize()), renderData.skyRenderData); !result)
    return std::unexpected(std::move(result.error()));
  if (renderData.skyRenderData.type == SkyType::Atmosphereless) {
    if (auto result = m_environmentPainter->renderBackOrbiters(orbiterAndPlanetRatio, Vec2F(m_camera.screenSize()), renderData.skyRenderData); !result)
      return std::unexpected(std::move(result.error()));
  }
  if (auto result = m_renderer->flush(); !result)
    return std::unexpected(std::move(result.error()));

  bool lightMapUpdated = false;
  if (lightWaiter) {
    auto waited = lightWaiter();
    if (!waited) return std::unexpected(std::move(waited.error()));
    lightMapUpdated = *waited;
  }
  if (auto result = m_renderer->setEffectParameter("lightMapEnabled", !renderData.isFullbright); !result)
    return std::unexpected(std::move(result.error()));
  if (renderData.isFullbright) {
    if (auto result = m_renderer->setEffectTexture("lightMap", Image::filled(Vec2U(1, 1), {255, 255, 255, 255}, PixelFormat::RGB24)); !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = m_renderer->setEffectParameter("lightMapMultiplier", 1.0f); !result)
      return std::unexpected(std::move(result.error()));
  } else {
    if (lightMapUpdated) {
      if (auto result = adjustLighting(renderData); !result) return std::unexpected(std::move(result.error()));
      if (auto result = m_renderer->setEffectTexture("lightMap", renderData.lightMap); !result)
        return std::unexpected(std::move(result.error()));
    }
    auto multiplierJson = m_assets->json("/rendering.config:lightMapMultiplier");
    if (!multiplierJson) return std::unexpected(std::move(multiplierJson.error()));
    auto multiplier = multiplierJson->toFloat();
    if (!multiplier) return std::unexpected(std::move(multiplier.error()));
    if (auto result = m_renderer->setEffectParameter("lightMapMultiplier", *multiplier); !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = m_renderer->setEffectParameter("lightMapScale", Vec2F::filled(TilePixels * m_camera.pixelRatio())); !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = m_renderer->setEffectParameter("lightMapOffset", m_camera.worldToScreen(Vec2F(renderData.lightMinPosition))); !result)
      return std::unexpected(std::move(result.error()));
  }

  // Parallax layers

  auto parallaxDelta = m_camera.worldGeometry().diff(m_camera.centerWorldPosition(), m_previousCameraCenter);
  if (parallaxDelta.magnitude() > 10)
    m_parallaxWorldPosition = m_camera.centerWorldPosition();
  else
    m_parallaxWorldPosition += parallaxDelta;
  m_previousCameraCenter = m_camera.centerWorldPosition();
  m_parallaxWorldPosition[1] = m_camera.centerWorldPosition()[1];

  if (!renderData.parallaxLayers.empty()) {
    if (auto result = m_environmentPainter->renderParallaxLayers(m_parallaxWorldPosition, m_camera, renderData.parallaxLayers, renderData.skyRenderData); !result)
      return std::unexpected(std::move(result.error()));
  }

  // Main world layers

  // Route references, not moved drawable lists. Display data remains reusable
  // for another presentation of the same logical tick.
  for (auto& layer : m_entityDrawableLayers)
    layer.second.clear();
  for (auto const& ed : renderData.entityDrawables) {
    for (auto const& p : ed.layers)
      if (!p.second.empty())
        m_entityDrawableLayers[p.first].append({ed.highlightEffect, std::cref(p.second)});
  }

  auto entityDrawableIterator = m_entityDrawableLayers.begin();
  auto renderEntitiesUntil = [this, &entityDrawableIterator](std::optional<EntityRenderLayer> until) -> std::expected<void, StarException> {
    while (true) {
      if (entityDrawableIterator == m_entityDrawableLayers.end())
        break;
      if (until && entityDrawableIterator->first >= *until)
        break;
      for (auto& edl : entityDrawableIterator->second) {
        if (auto result = drawEntityLayer(edl.second.get(), edl.first); !result)
          return std::unexpected(std::move(result.error()));
      }
      ++entityDrawableIterator;
    }

    return m_renderer->flush();
  };

  if (auto r = renderEntitiesUntil(RenderLayerBackgroundOverlay); !r) return std::unexpected(std::move(r.error()));
  if (auto r = drawDrawableSet(renderData.backgroundOverlays); !r) return std::unexpected(std::move(r.error()));
  if (auto r = renderEntitiesUntil(RenderLayerBackgroundTile); !r) return std::unexpected(std::move(r.error()));
  if (auto r = m_tilePainter->renderBackground(m_camera); !r) return std::unexpected(std::move(r.error()));
  if (auto r = renderEntitiesUntil(RenderLayerPlatform); !r) return std::unexpected(std::move(r.error()));
  if (auto r = m_tilePainter->renderMidground(m_camera); !r) return std::unexpected(std::move(r.error()));
  if (auto r = renderEntitiesUntil(RenderLayerBackParticle); !r) return std::unexpected(std::move(r.error()));
  if (auto r = renderParticles(renderData, Particle::Layer::Back); !r) return std::unexpected(std::move(r.error()));
  if (auto r = renderEntitiesUntil(RenderLayerLiquid); !r) return std::unexpected(std::move(r.error()));
  if (auto r = m_tilePainter->renderLiquid(m_camera); !r) return std::unexpected(std::move(r.error()));
  if (auto r = renderEntitiesUntil(RenderLayerMiddleParticle); !r) return std::unexpected(std::move(r.error()));
  if (auto r = renderParticles(renderData, Particle::Layer::Middle); !r) return std::unexpected(std::move(r.error()));
  if (auto r = renderEntitiesUntil(RenderLayerForegroundTile); !r) return std::unexpected(std::move(r.error()));
  if (auto r = m_tilePainter->renderForeground(m_camera); !r) return std::unexpected(std::move(r.error()));
  if (auto r = renderEntitiesUntil(RenderLayerForegroundOverlay); !r) return std::unexpected(std::move(r.error()));
  if (auto r = drawDrawableSet(renderData.foregroundOverlays); !r) return std::unexpected(std::move(r.error()));
  if (auto r = renderEntitiesUntil(RenderLayerFrontParticle); !r) return std::unexpected(std::move(r.error()));
  if (auto r = renderParticles(renderData, Particle::Layer::Front); !r) return std::unexpected(std::move(r.error()));
  if (auto r = renderEntitiesUntil(RenderLayerOverlay); !r) return std::unexpected(std::move(r.error()));
  if (auto r = drawDrawableSet(renderData.nametags); !r) return std::unexpected(std::move(r.error()));
  if (auto r = renderBars(renderData); !r) return std::unexpected(std::move(r.error()));
  if (auto r = renderEntitiesUntil({}); !r) return std::unexpected(std::move(r.error()));

  auto dimLevel = std::round(renderData.dimLevel * 255);
  if (dimLevel != 0)
    m_renderer->render(renderFlatRect(RectF::withSize({}, Vec2F(m_camera.screenSize())), Vec4B(renderData.dimColor, dimLevel), 0.0f));

  auto timeoutJson = m_assets->json("/rendering.config:textureTimeout");
  if (!timeoutJson) return std::unexpected(std::move(timeoutJson.error()));
  auto timeout = timeoutJson->toInt();
  if (!timeout) return std::unexpected(std::move(timeout.error()));
  std::int64_t textureTimeout = *timeout;
  m_textPainter->cleanup(textureTimeout);
  m_drawablePainter->cleanup(textureTimeout);
  m_environmentPainter->cleanup(textureTimeout);
  m_tilePainter->cleanup();
  return {};
}

std::expected<void, StarException> WorldPainter::adjustLighting(WorldRenderData& renderData) {
  return m_tilePainter->adjustLighting(renderData);
}

std::expected<void, StarException> WorldPainter::renderParticles(WorldRenderData& renderData, Particle::Layer layer) {
  auto fontSizeJson = m_assets->json("/rendering.config:textParticleFontSize");
  if (!fontSizeJson) return std::unexpected(std::move(fontSizeJson.error()));
  auto fontSize = fontSizeJson->toInt();
  if (!fontSize) return std::unexpected(std::move(fontSize.error()));
  const int textParticleFontSize = *fontSize;
  auto paddingJson = m_assets->json("/rendering.config:particleRenderWindowPadding");
  if (!paddingJson) return std::unexpected(std::move(paddingJson.error()));
  auto padding = paddingJson->toInt();
  if (!padding) return std::unexpected(std::move(padding.error()));
  const RectF particleRenderWindow = RectF::withSize(Vec2F(), Vec2F(m_camera.screenSize())).padded(*padding);

  for (Particle const& particle : renderData.particles) {
    if (layer != particle.layer)
      continue;

    Vec2F position = m_camera.worldToScreen(particle.position);

    if (!particleRenderWindow.contains(position))
      continue;

    Vec2F size = Vec2F::filled(particle.size * m_camera.pixelRatio());

    if (particle.type == Particle::Type::Ember) {
      m_renderer->immediatePrimitives().emplace_back(std::in_place_type_t<RenderQuad>(),
        RectF(position - size / 2, position + size / 2),
        particle.color.toRgba(),
        particle.fullbright ? 0.0f : 1.0f);

    } else if (particle.type == Particle::Type::Streak) {
      // Draw a rotated quad streaking in the direction the particle is coming from.
      // Sadly this looks awful.
      Vec2F dir = particle.velocity.normalized();
      Vec2F sideHalf = dir.rot90() * m_camera.pixelRatio() * particle.size / 2;
      float length = particle.length * m_camera.pixelRatio();
      Vec4B color = particle.color.toRgba();
      float lightMapMultiplier = particle.fullbright ? 0.0f : 1.0f;
      m_renderer->immediatePrimitives().emplace_back(std::in_place_type_t<RenderQuad>(),
        position - sideHalf,
        position + sideHalf,
        position - dir * length + sideHalf,
        position - dir * length - sideHalf,
        color, lightMapMultiplier);

    } else if (particle.type == Particle::Type::Textured || particle.type == Particle::Type::Animated) {
      auto drawableResult = particle.type == Particle::Type::Textured
          ? Drawable::makeImage(*m_imageMetadata, particle.image, 1.0f / TilePixels, true, Vec2F(0, 0))
          : particle.animation->drawable(1.0f / TilePixels);
      if (!drawableResult) return std::unexpected(std::move(drawableResult.error()));
      Drawable drawable = std::move(*drawableResult);

      if (particle.flip && particle.flippable)
        drawable.scale(Vec2F(-1, 1));
      if (drawable.isImage() && particle.type != Particle::Type::Animated) {
        if (auto result = drawable.imagePart().addDirectivesGroup(*m_imageMetadata, particle.directives, true); !result)
          return std::unexpected(std::move(result.error()));
      }
      drawable.fullbright = particle.fullbright;
      drawable.color = particle.color;
      drawable.rotate(particle.rotation);
      drawable.scale(particle.size);
      drawable.translate(particle.position);
      if (auto result = drawDrawable(std::move(drawable)); !result) return std::unexpected(std::move(result.error()));

    } else if (particle.type == Particle::Type::Text) {
      Vec2F position = m_camera.worldToScreen(particle.position);
      int size = std::min(128.0f, std::round((float)textParticleFontSize * m_camera.pixelRatio() * particle.size));
      if (size > 0) {
        m_textPainter->setFontSize(size);
        m_textPainter->setFontColor(particle.color.toRgba());
        if (auto result = m_textPainter->setProcessingDirectives(""); !result) return std::unexpected(std::move(result.error()));
        if (auto result = m_textPainter->setFont(""); !result)
          return std::unexpected(std::move(result.error()));
        if (auto result = m_textPainter->renderText(particle.string, {position, HorizontalAnchor::HMidAnchor, VerticalAnchor::VMidAnchor}); !result)
          return std::unexpected(std::move(result.error()));
      }
    }
  }

  return m_renderer->flush();
}

std::expected<void, StarException> WorldPainter::renderBars(WorldRenderData& renderData) {
  auto offset = m_entityBarOffset;
  for (auto const& bar : renderData.overheadBars) {
    auto position = bar.entityPosition + offset;
    offset += m_entityBarSpacing;
    if (bar.icon) {
      auto iconDrawPosition = position - (m_entityBarSize / 2).round() + m_entityBarIconOffset;
      auto drawable = Drawable::makeImage(*m_imageMetadata, *bar.icon, 1.0f / TilePixels, true, iconDrawPosition);
      if (!drawable) return std::unexpected(std::move(drawable.error()));
      if (auto result = drawDrawable(std::move(*drawable)); !result) return std::unexpected(std::move(result.error()));
    }

    if (!bar.detailOnly) {
      auto fullBar = RectF({}, {m_entityBarSize.x() * bar.percentage, m_entityBarSize.y()});
      auto emptyBar = RectF({m_entityBarSize.x() * bar.percentage, 0.0f}, m_entityBarSize);
      auto fullColor = bar.color;
      auto emptyColor = Color::Black;

      if (auto result = drawDrawable(Drawable::makePoly(PolyF(emptyBar), emptyColor, position)); !result) return std::unexpected(std::move(result.error()));
      if (auto result = drawDrawable(Drawable::makePoly(PolyF(fullBar), fullColor, position)); !result) return std::unexpected(std::move(result.error()));
    }
  }

  return m_renderer->flush();
}

std::expected<void, StarException> WorldPainter::drawEntityLayer(List<Drawable> const& drawables, EntityHighlightEffect highlightEffect) {
  auto maxLevel = m_highlightConfig.getFloat("maxHighlightLevel", 1.0);
  if (!maxLevel) return std::unexpected(std::move(maxLevel.error()));
  highlightEffect.level *= *maxLevel;
  if (auto directives = m_highlightDirectives.maybeRef(highlightEffect.type); directives && highlightEffect.level > 0) {
    auto const& underlayDirectives = directives->get().first;
    auto const& overlayDirectives = directives->get().second;
    auto drawHighlight = [&](Drawable const& drawable,
                             Directives const& highlightDirectives) -> std::expected<void, StarException> {
      auto highlightDrawable = Drawable(drawable);
      highlightDrawable.fullbright = true;
      highlightDrawable.color = Color::rgbaf(1, 1, 1, highlightEffect.level * drawable.color.alphaF());
      if (auto result = highlightDrawable.imagePart().addDirectives(*m_imageMetadata, highlightDirectives, true);
          !result)
        return std::unexpected(std::move(result.error()));
      return drawDrawable(std::move(highlightDrawable));
    };

    // first pass, draw underlay
    if (!underlayDirectives.empty()) {
      for (auto const& d : drawables) {
        if (d.isImage()) {
          if (auto result = drawHighlight(d, underlayDirectives); !result)
            return std::unexpected(std::move(result.error()));
        }
      }
    }

    // second pass, draw main drawables and overlays
    for (auto const& d : drawables) {
      if (auto result = drawDrawable(d); !result) return std::unexpected(std::move(result.error()));
      if (!overlayDirectives.empty() && d.isImage()) {
        if (auto result = drawHighlight(d, overlayDirectives); !result)
          return std::unexpected(std::move(result.error()));
      }
    }
  } else {
    for (auto const& d : drawables) {
      if (auto result = drawDrawable(d); !result) return std::unexpected(std::move(result.error()));
    }
  }
  return {};
}

std::expected<void, StarException> WorldPainter::drawDrawable(Drawable const& drawable) {
  Vec2F position = m_camera.worldToScreen(drawable.position);
  float scale = m_camera.pixelRatio() * TilePixels;

  // draw the drawable if it's on screen
  // if it's not on screen, there's a random chance to pre-load
  // pre-load is not done on every tick because it's expensive to look up images with long paths
  auto bounds = drawable.boundBox(*m_imageMetadata, false);
  if (!bounds) return std::unexpected(std::move(bounds.error()));
  RectF screenBounds((bounds->min() - drawable.position) * scale + position,
      (bounds->max() - drawable.position) * scale + position);
  if (RectF::withSize(Vec2F(), Vec2F(m_camera.screenSize())).intersects(screenBounds))
    return m_drawablePainter->drawDrawableProjected(drawable, position, scale, m_camera.pixelRatio());
  if (drawable.isImage() && m_preloadRandom.randf() < m_preloadTextureChance) {
    auto image = m_assets->tryImage(drawable.imagePart().image);
    if (!image) return std::unexpected(std::move(image.error()));
  }
  return {};
}

std::expected<void, StarException> WorldPainter::drawDrawableSet(List<Drawable> const& drawables) {
  for (Drawable const& drawable : drawables) {
    if (auto result = drawDrawable(drawable); !result) return std::unexpected(std::move(result.error()));
  }
  return m_renderer->flush();
}

}

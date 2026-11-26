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
  m_camera.setScreenSize({800, 600});
  m_camera.setCenterWorldPosition(Vec2F());

  auto highlight_config = m_assets->json("/highlights.config");
  if (!highlight_config) return std::unexpected(std::move(highlight_config.error()));
  m_highlight_config = std::move(*highlight_config);
  auto directives = m_highlight_config.get("highlightDirectives");
  if (!directives) return std::unexpected(std::move(directives.error()));
  auto entries = directives->iterate_object();
  if (!entries) return std::unexpected(std::move(entries.error()));
  for (auto const& p : *entries) {
    auto type = enum_value<entity_highlight_effect_type_names>(p.first);
    if (!type) return std::unexpected(std::move(type.error()));
    auto underlay = p.second.get_string("underlay", "");
    if (!underlay) return std::unexpected(std::move(underlay.error()));
    auto overlay = p.second.get_string("overlay", "");
    if (!overlay) return std::unexpected(std::move(overlay.error()));
    m_highlight_directives.set(*type, {Directives(*underlay), Directives(*overlay)});
  }
  auto loadVector = [this](String const& path) -> std::expected<Vec2F, StarException> {
    auto json = m_assets->json(path);
    if (!json) return std::unexpected(std::move(json.error()));
    return json_to_vec2f(*json);
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
  auto preload = preloadJson->to_float();
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

std::expected<void, StarException> WorldPainter::render(WorldRenderData& render_data, std::function<std::expected<bool, StarException>()> light_waiter) {
  m_camera.setScreenSize(m_renderer->screenSize());
  auto assets = m_root.assets();
  if (!assets) return std::unexpected(std::move(assets.error()));
  m_assets = *assets;
  auto image_metadata = m_root.image_metadata_database();
  if (!image_metadata) return std::unexpected(std::move(image_metadata.error()));
  m_image_metadata = *image_metadata;
  if (auto result = m_tilePainter->setup(m_camera, render_data); !result)
    return std::unexpected(std::move(result.error()));

  // Stars, Debris Fields, Sky, and Orbiters

  // Use a fixed pixel ratio for certain things.
  float pixel_ratio_basis = m_camera.screenSize()[1] / 1080.0f;
  float star_and_debris_ratio = lerp(0.0625f, pixel_ratio_basis * 2.0f, m_camera.pixelRatio());
  float orbiter_and_planet_ratio = lerp(0.125f, pixel_ratio_basis * 3.0f, m_camera.pixelRatio());

  if (auto result = m_environmentPainter->renderStars(star_and_debris_ratio, Vec2F(m_camera.screenSize()), render_data.sky_render_data); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = m_environmentPainter->renderDebrisFields(star_and_debris_ratio, Vec2F(m_camera.screenSize()), render_data.sky_render_data); !result)
    return std::unexpected(std::move(result.error()));
  if (render_data.sky_render_data.type != SkyType::Atmosphereless) {
    if (auto result = m_environmentPainter->renderBackOrbiters(orbiter_and_planet_ratio, Vec2F(m_camera.screenSize()), render_data.sky_render_data); !result)
      return std::unexpected(std::move(result.error()));
  }
  if (auto result = m_environmentPainter->renderPlanetHorizon(orbiter_and_planet_ratio, Vec2F(m_camera.screenSize()), render_data.sky_render_data); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = m_environmentPainter->renderSky(Vec2F(m_camera.screenSize()), render_data.sky_render_data); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = m_environmentPainter->renderFrontOrbiters(orbiter_and_planet_ratio, Vec2F(m_camera.screenSize()), render_data.sky_render_data); !result)
    return std::unexpected(std::move(result.error()));
  if (render_data.sky_render_data.type == SkyType::Atmosphereless) {
    if (auto result = m_environmentPainter->renderBackOrbiters(orbiter_and_planet_ratio, Vec2F(m_camera.screenSize()), render_data.sky_render_data); !result)
      return std::unexpected(std::move(result.error()));
  }
  if (auto result = m_renderer->flush(); !result)
    return std::unexpected(std::move(result.error()));

  bool light_map_updated = false;
  if (light_waiter) {
    auto waited = light_waiter();
    if (!waited) return std::unexpected(std::move(waited.error()));
    light_map_updated = *waited;
  }
  if (auto result = m_renderer->setEffectParameter("lightMapEnabled", !render_data.is_fullbright); !result)
    return std::unexpected(std::move(result.error()));
  if (render_data.is_fullbright) {
    if (auto result = m_renderer->setEffectTexture("lightMap", Image::filled(Vec2U(1, 1), {255, 255, 255, 255}, PixelFormat::RGB24)); !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = m_renderer->setEffectParameter("lightMapMultiplier", 1.0f); !result)
      return std::unexpected(std::move(result.error()));
  } else {
    if (light_map_updated) {
      if (auto result = adjustLighting(render_data); !result) return std::unexpected(std::move(result.error()));
      if (auto result = m_renderer->setEffectTexture("lightMap", render_data.light_map); !result)
        return std::unexpected(std::move(result.error()));
    }
    auto multiplier_json = m_assets->json("/rendering.config:lightMapMultiplier");
    if (!multiplier_json) return std::unexpected(std::move(multiplier_json.error()));
    auto multiplier = multiplier_json->to_float();
    if (!multiplier) return std::unexpected(std::move(multiplier.error()));
    if (auto result = m_renderer->setEffectParameter("lightMapMultiplier", *multiplier); !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = m_renderer->setEffectParameter("lightMapScale", Vec2F::filled(tile_pixels * m_camera.pixelRatio())); !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = m_renderer->setEffectParameter("lightMapOffset", m_camera.worldToScreen(Vec2F(render_data.light_min_position))); !result)
      return std::unexpected(std::move(result.error()));
  }

  // Parallax layers

  auto camera_center = m_camera.centerWorldPosition();
  auto parallax_delta = m_camera.worldGeometry().diff(camera_center, m_previous_camera_center);
  if (parallax_delta.magnitude() > 10)
    m_parallax_world_position = camera_center;
  else
    m_parallax_world_position += parallax_delta;
  m_previous_camera_center = camera_center;
  m_parallax_world_position[1] = camera_center[1];

  if (auto layers = render_data.parallax_layers.samples(); !layers.empty()) {
    if (auto result = m_environmentPainter->render_parallax_layers(m_parallax_world_position, m_camera, layers, render_data.sky_render_data); !result)
      return std::unexpected(std::move(result.error()));
  }

  // Main world layers

  // Route references, not moved drawable lists. Display data remains reusable
  // for another presentation of the same logical tick.
  route_entity_drawables(m_entity_drawable_layers, render_data.entity_drawables);

  auto next_layer = m_entity_drawable_layers.begin();
  auto render_entities_until = [this, &next_layer](std::optional<EntityRenderLayer> until) -> std::expected<void, StarException> {
    while (next_layer != m_entity_drawable_layers.end() && (!until || next_layer->first < *until)) {
      for (auto const& group : next_layer->second) {
        if (auto result = draw_entity_layer(group.drawables.get(), group.highlight_effect); !result)
          return std::unexpected(std::move(result.error()));
      }
      ++next_layer;
    }

    return m_renderer->flush();
  };

  if (auto r = render_entities_until(render_layer_background_overlay); !r) return std::unexpected(std::move(r.error()));
  if (auto r = draw_drawable_set(render_data.background_overlays); !r) return std::unexpected(std::move(r.error()));
  if (auto r = render_entities_until(render_layer_background_tile); !r) return std::unexpected(std::move(r.error()));
  if (auto r = m_tilePainter->renderBackground(m_camera); !r) return std::unexpected(std::move(r.error()));
  if (auto r = render_entities_until(render_layer_platform); !r) return std::unexpected(std::move(r.error()));
  if (auto r = m_tilePainter->renderMidground(m_camera); !r) return std::unexpected(std::move(r.error()));
  if (auto r = render_entities_until(render_layer_back_particle); !r) return std::unexpected(std::move(r.error()));
  if (auto r = renderParticles(render_data, Particle::Layer::Back); !r) return std::unexpected(std::move(r.error()));
  if (auto r = render_entities_until(render_layer_liquid); !r) return std::unexpected(std::move(r.error()));
  if (auto r = m_tilePainter->renderLiquid(m_camera); !r) return std::unexpected(std::move(r.error()));
  if (auto r = render_entities_until(render_layer_middle_particle); !r) return std::unexpected(std::move(r.error()));
  if (auto r = renderParticles(render_data, Particle::Layer::Middle); !r) return std::unexpected(std::move(r.error()));
  if (auto r = render_entities_until(render_layer_foreground_tile); !r) return std::unexpected(std::move(r.error()));
  if (auto r = m_tilePainter->renderForeground(m_camera); !r) return std::unexpected(std::move(r.error()));
  if (auto r = render_entities_until(render_layer_foreground_overlay); !r) return std::unexpected(std::move(r.error()));
  if (auto r = draw_drawable_set(render_data.foreground_overlays); !r) return std::unexpected(std::move(r.error()));
  if (auto r = render_entities_until(render_layer_front_particle); !r) return std::unexpected(std::move(r.error()));
  if (auto r = renderParticles(render_data, Particle::Layer::Front); !r) return std::unexpected(std::move(r.error()));
  if (auto r = render_entities_until(render_layer_overlay); !r) return std::unexpected(std::move(r.error()));
  if (auto r = draw_drawable_set(render_data.nametags); !r) return std::unexpected(std::move(r.error()));
  if (auto r = renderBars(render_data); !r) return std::unexpected(std::move(r.error()));
  if (auto r = render_entities_until({}); !r) return std::unexpected(std::move(r.error()));

  auto dim_level = std::round(render_data.dim_level * 255);
  if (dim_level != 0)
    m_renderer->render(renderFlatRect(RectF::with_size({}, Vec2F(m_camera.screenSize())), Vec4B(render_data.dim_color, dim_level), 0.0f));

  auto timeout_json = m_assets->json("/rendering.config:textureTimeout");
  if (!timeout_json) return std::unexpected(std::move(timeout_json.error()));
  auto timeout = timeout_json->to_int();
  if (!timeout) return std::unexpected(std::move(timeout.error()));
  std::int64_t texture_timeout = *timeout;
  m_textPainter->cleanup(texture_timeout);
  m_drawablePainter->cleanup(texture_timeout);
  m_environmentPainter->cleanup(texture_timeout);
  m_tilePainter->cleanup();
  return {};
}

std::expected<void, StarException> WorldPainter::adjustLighting(WorldRenderData& render_data) {
  return m_tilePainter->adjustLighting(render_data);
}

std::expected<void, StarException> WorldPainter::renderParticles(WorldRenderData& render_data, Particle::Layer layer) {
  auto fontSizeJson = m_assets->json("/rendering.config:textParticleFontSize");
  if (!fontSizeJson) return std::unexpected(std::move(fontSizeJson.error()));
  auto fontSize = fontSizeJson->to_int();
  if (!fontSize) return std::unexpected(std::move(fontSize.error()));
  const int textParticleFontSize = *fontSize;
  auto paddingJson = m_assets->json("/rendering.config:particleRenderWindowPadding");
  if (!paddingJson) return std::unexpected(std::move(paddingJson.error()));
  auto padding = paddingJson->to_int();
  if (!padding) return std::unexpected(std::move(padding.error()));
  const RectF particleRenderWindow = RectF::with_size(Vec2F(), Vec2F(m_camera.screenSize())).padded(*padding);

  for (Particle const& particle : render_data.particles) {
    if (layer != particle.layer)
      continue;

    Vec2F position = m_camera.worldToScreen(particle.position);

    if (!particleRenderWindow.contains(position))
      continue;

    Vec2F size = Vec2F::filled(particle.size * m_camera.pixelRatio());

    if (particle.type == Particle::Type::Ember) {
      m_renderer->immediatePrimitives().emplace_back(std::in_place_type_t<RenderQuad>(),
        RectF(position - size / 2, position + size / 2),
        particle.color.to_rgba(),
        particle.fullbright ? 0.0f : 1.0f);

    } else if (particle.type == Particle::Type::Streak) {
      // Draw a rotated quad streaking in the direction the particle is coming from.
      // Sadly this looks awful.
      Vec2F dir = particle.velocity.normalized();
      Vec2F sideHalf = dir.rot90() * m_camera.pixelRatio() * particle.size / 2;
      float length = particle.length * m_camera.pixelRatio();
      Vec4B color = particle.color.to_rgba();
      float lightMapMultiplier = particle.fullbright ? 0.0f : 1.0f;
      m_renderer->immediatePrimitives().emplace_back(std::in_place_type_t<RenderQuad>(),
        position - sideHalf,
        position + sideHalf,
        position - dir * length + sideHalf,
        position - dir * length - sideHalf,
        color, lightMapMultiplier);

    } else if (particle.type == Particle::Type::Textured || particle.type == Particle::Type::Animated) {
      auto drawableResult = particle.type == Particle::Type::Textured
          ? Drawable::make_image(*m_image_metadata, particle.image, 1.0f / tile_pixels, true, Vec2F(0, 0))
          : particle.animation->drawable(1.0f / tile_pixels);
      if (!drawableResult) return std::unexpected(std::move(drawableResult.error()));
      Drawable drawable = std::move(*drawableResult);

      if (particle.flip && particle.flippable)
        drawable.scale(Vec2F(-1, 1));
      if (drawable.is_image() && particle.type != Particle::Type::Animated) {
        if (auto result = drawable.image_part().add_directives_group(*m_image_metadata, particle.directives, true); !result)
          return std::unexpected(std::move(result.error()));
      }
      drawable.fullbright = particle.fullbright;
      drawable.color = particle.color;
      drawable.rotate(particle.rotation);
      drawable.scale(particle.size);
      drawable.translate(particle.position);
      if (auto result = draw_drawable(drawable); !result) return std::unexpected(std::move(result.error()));

    } else if (particle.type == Particle::Type::Text) {
      Vec2F position = m_camera.worldToScreen(particle.position);
      int size = std::min(128.0f, std::round((float)textParticleFontSize * m_camera.pixelRatio() * particle.size));
      if (size > 0) {
        m_textPainter->setFontSize(size);
        m_textPainter->setFontColor(particle.color.to_rgba());
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

std::expected<void, StarException> WorldPainter::renderBars(WorldRenderData& render_data) {
  auto offset = m_entityBarOffset;
  for (auto const& bar : render_data.overhead_bars) {
    auto position = bar.entity_position + offset;
    offset += m_entityBarSpacing;
    if (bar.icon) {
      auto iconDrawPosition = position - (m_entityBarSize / 2).round() + m_entityBarIconOffset;
      auto drawable = Drawable::make_image(*m_image_metadata, *bar.icon, 1.0f / tile_pixels, true, iconDrawPosition);
      if (!drawable) return std::unexpected(std::move(drawable.error()));
      if (auto result = draw_drawable(*drawable); !result) return std::unexpected(std::move(result.error()));
    }

    if (!bar.detail_only) {
      auto fullBar = RectF({}, {m_entityBarSize.x() * bar.percentage, m_entityBarSize.y()});
      auto emptyBar = RectF({m_entityBarSize.x() * bar.percentage, 0.0f}, m_entityBarSize);
      auto fullColor = bar.color;
      auto emptyColor = Color::palette_black;

      if (auto result = draw_drawable(Drawable::make_poly(PolyF(emptyBar), emptyColor, position)); !result) return std::unexpected(std::move(result.error()));
      if (auto result = draw_drawable(Drawable::make_poly(PolyF(fullBar), fullColor, position)); !result) return std::unexpected(std::move(result.error()));
    }
  }

  return m_renderer->flush();
}

std::expected<void, StarException> WorldPainter::draw_entity_layer(List<Drawable> const& drawables, EntityHighlightEffect highlight_effect) {
  auto max_level = m_highlight_config.get_float("maxHighlightLevel", 1.0);
  if (!max_level) return std::unexpected(std::move(max_level.error()));
  highlight_effect.level *= *max_level;
  if (auto directives = m_highlight_directives.find_ref(highlight_effect.type); directives && highlight_effect.level > 0) {
    auto const& underlay_directives = directives->get().first;
    auto const& overlay_directives = directives->get().second;
    auto draw_highlight = [&](Drawable const& drawable,
                             Directives const& highlight_directives) -> std::expected<void, StarException> {
      auto highlight_drawable = Drawable(drawable);
      highlight_drawable.fullbright = true;
      highlight_drawable.color = Color::rgba_f(1, 1, 1, highlight_effect.level * drawable.color.alpha_f());
      if (auto result = highlight_drawable.image_part().add_directives(*m_image_metadata, highlight_directives, true);
          !result)
        return std::unexpected(std::move(result.error()));
      return draw_drawable(highlight_drawable);
    };

    // first pass, draw underlay
    if (!underlay_directives.empty()) {
      for (auto const& d : drawables) {
        if (d.is_image()) {
          if (auto result = draw_highlight(d, underlay_directives); !result)
            return std::unexpected(std::move(result.error()));
        }
      }
    }

    // second pass, draw main drawables and overlays
    for (auto const& d : drawables) {
      if (auto result = draw_drawable(d); !result) return std::unexpected(std::move(result.error()));
      if (!overlay_directives.empty() && d.is_image()) {
        if (auto result = draw_highlight(d, overlay_directives); !result)
          return std::unexpected(std::move(result.error()));
      }
    }
  } else {
    for (auto const& d : drawables) {
      if (auto result = draw_drawable(d); !result) return std::unexpected(std::move(result.error()));
    }
  }
  return {};
}

std::expected<void, StarException> WorldPainter::draw_drawable(Drawable const& drawable) {
  Vec2F position = m_camera.worldToScreen(drawable.position);
  float pixel_ratio = m_camera.pixelRatio();
  float scale = pixel_ratio * tile_pixels;
  RectF viewport = RectF::with_size({}, Vec2F(m_camera.screenSize()));
  if (drawable.is_line()) {
    m_drawablePainter->draw_projected_line(drawable.line_part(), drawable.color, drawable.fullbright,
        position, scale, pixel_ratio, std::cref(viewport));
    return {};
  }

  auto bounds = drawable.bound_box(*m_image_metadata, false);
  if (!bounds) return std::unexpected(std::move(bounds.error()));
  RectF screen_bounds((bounds->min() - drawable.position) * scale + position,
      (bounds->max() - drawable.position) * scale + position);
  if (viewport.intersects(screen_bounds))
    return m_drawablePainter->draw_drawable_projected(drawable, position, scale, pixel_ratio);
  // Only occasionally pay for preloading a potentially long, off-screen asset path.
  if (drawable.is_image() && m_preloadRandom.uniform_float() < m_preloadTextureChance) {
    auto image = m_assets->try_image(drawable.image_part().image);
    if (!image) return std::unexpected(std::move(image.error()));
  }
  return {};
}

std::expected<void, StarException> WorldPainter::draw_drawable_set(List<Drawable> const& drawables) {
  for (Drawable const& drawable : drawables) {
    if (auto result = draw_drawable(drawable); !result) return std::unexpected(std::move(result.error()));
  }
  return m_renderer->flush();
}

}

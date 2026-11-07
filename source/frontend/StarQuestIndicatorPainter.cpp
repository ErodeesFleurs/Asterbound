import std;
import star.format;
import star.xx_hash;
import star.json;
import star.game_types;
import star.id_map;
import star.image;
import star.version;
import star.either;
import star.random;
import star.weighted_pool;
import star.casting;
import star.strong_typedef;
import star.rpc_promise;
import star.ordered_map;
import star.net_compatibility;
import star.set;
import star.vector;
import star.thread;
import star.worker_pool;

import star.sector_array_2d;
import star.bi_map;
import star.perlin;
import star.btree;
import star.lru_cache;
import star.data_stream_devices;
import star.btree_database;
import star.ordered_set;
import star.io_device;
import star.ref_ptr;
import star.logging;
import star.color;
import star.font;
import star.list;
import star.image_processing;
import star.hash;
import star.data_stream;
import star.string_view;
import star.directives;
import star.string;
import star.text;
import star.asset_path;
import star.listener;
import star.input_event;
import star.audio;
import star.map;
import star.byte_array;

typedef struct ZSTD_CCtx_s ZSTD_CCtx;
typedef struct ZSTD_DCtx_s ZSTD_DCtx;
typedef ZSTD_DCtx ZSTD_DStream;
typedef ZSTD_CCtx ZSTD_CStream;
import star.zstd_compression;

import star.lua_root;
import star.world_geometry;

import star.collision_block;
import star.liquid_types;
import star.tile_damage;
import star.tile_sector_array;
import star.particle;
import star.weather_types;
import star.celestial_coordinate;
import star.sky_types;
import star.force_regions;
import star.world_parameters;
import star.celestial_parameters;
import star.world_layout;
import star.collision_generator;
import star.world_tiles;
import star.item_descriptor;
import star.celestial_types;
import star.chat_types;
import star.uuid;
import star.tile_modification;
import star.damage_types;
import star.status_types;
import star.damage;
import star.light_source;
import star.entity;
import star.interaction_types;
import star.warping;
import star.wiring;
import star.quest_descriptor;
import star.interactive_entity;
import star.tile_entity;
import star.inspectable_entity;
import star.plant;
import star.plant_database;
import star.biome_placement;
import star.versioning_database;
import star.world_storage;
import star.player_types;
import star.sky_parameters;
import star.system_world;
import star.damage_manager;
import star.net_packets;
import star.entity_rendering_types;
import star.sky_render_data;
import star.parallax;
import star.cellular_light_array;
import star.cellular_lighting;
import star.world_render_data;
import star.world_structure;
import star.chat_action;
import star.entity_rendering;
import star.world;
import star.lua_components;
import star.world_client_state;
import star.interpolation_tracker;
import star.ambient;
import star.weather;
import star.game_timers;
import star.world_client;

import star.world_camera;


import star.quest_indicator_painter;
import star.asset_source;
import star.assets;
import star.root_base;
import star.configuration;
import star.root;
import star.application;
import star.statistics_service;
import star.p2p_networking_service;
import star.user_generated_content_service;
import star.desktop_service;
import star.application_controller;
import star.renderer;
import star.font_texture_group;
import star.anchor_types;
import star.text_painter;
import star.drawable;
import star.asset_texture_group;
import star.drawable_painter;
import star.gui_types;
import star.key_bindings;
import star.mixer;
import star.gui_context;
import star.host_address;
import star.ai_types;
import star.socket;
import star.tcp;
import star.net_packet_socket;
import star.universe_connection;
import star.world_client_thread;
import star.universe;
// TODO: make this more thread safe
import star.universe_client;

import star.animation;
import star.quest_manager;

namespace Star {

QuestIndicatorPainter::QuestIndicatorPainter(GuiContext& context, std::shared_ptr<UniverseClient> const& client)
  : m_guiContext(context) {
  m_client = client;
}

[[nodiscard]] std::expected<std::shared_ptr<Animation>, StarException> indicatorAnimation(Root& root, String indicatorPath) {
  auto assets = root.assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  auto config = (*assets)->json(indicatorPath);
  if (!config)
    return std::unexpected(std::move(config.error()));
  auto animation = Animation::create(root, *config, indicatorPath);
  if (!animation)
    return std::unexpected(std::move(animation.error()));
  return std::make_shared<Animation>(std::move(*animation));
}

std::expected<void, StarException> QuestIndicatorPainter::update(float dt, std::shared_ptr<WorldClient> const& world, WorldCamera const& camera) {

  auto& foundIndicators = m_foundIndicators;
  foundIndicators.clear();
  auto entities = world->query<Entity>(camera.worldScreenRect());
  if (!entities)
    return std::unexpected(std::move(entities.error()));
  for (auto const& entity : *entities) {
    auto found = m_client->questManager()->getQuestIndicator(entity);
    if (!found)
      return std::unexpected(std::move(found.error()));
    if (!*found)
      continue;
    auto const& indicator = **found;
    foundIndicators.insert(entity->entityId());

    if (auto currentIndicator = m_indicators.maybeRef(entity->entityId())) {
      currentIndicator->get().worldPosition = indicator.worldPosition;
      if (currentIndicator->get().indicatorName == indicator.indicatorImage) {
        currentIndicator->get().animation->update(dt);
      } else {
        auto animation = indicatorAnimation(m_guiContext.get().root(), indicator.indicatorImage);
        if (!animation)
          return std::unexpected(std::move(animation.error()));
        currentIndicator->get().indicatorName = indicator.indicatorImage;
        currentIndicator->get().animation = std::move(*animation);
      }
    } else {
      auto animation = indicatorAnimation(m_guiContext.get().root(), indicator.indicatorImage);
      if (!animation)
        return std::unexpected(std::move(animation.error()));
      m_indicators[entity->entityId()] = Indicator{.entityId = entity->entityId(),
                                                   .worldPosition = indicator.worldPosition,
                                                   .indicatorName = indicator.indicatorImage,
                                                   .animation = std::move(*animation)};
    }
  }

  for (auto indicator = m_indicators.begin(); indicator != m_indicators.end();) {
    if (!foundIndicators.contains(indicator->first))
      indicator = m_indicators.erase(indicator);
    else
      ++indicator;
  }
  return {};
}

std::expected<Drawable, StarException> QuestIndicatorPainter::Indicator::render(float pixelRatio) const {
  return animation->drawable(pixelRatio);
}

std::expected<void, StarException> QuestIndicatorPainter::render(WorldClient const& world, WorldCamera const& camera, PresentationContext presentation) {
  auto& context = m_guiContext.get();

  for (auto const& indicator : m_indicators | std::views::values) {
    auto entity = world.entity(indicator.entityId);
    if (!entity)
      continue;
    auto drawable = indicator.render(camera.pixelRatio());
    if (!drawable)
      return std::unexpected(std::move(drawable.error()));
    drawable->fullbright = true;
    auto position = indicator.worldPosition + world.geometry().diff(world.renderPosition(indicator.entityId, presentation), entity->position());
    auto drawn = context.drawDrawable(*drawable, camera.worldToScreen(position), 1, Vec4B(255, 255, 255, 255));
    if (!drawn)
      return std::unexpected(std::move(drawn.error()));
  }
  return {};
}

}

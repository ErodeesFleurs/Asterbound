import std;
import star.format;
import star.json;
import star.xx_hash;
import star.json_extra;
import star.id_map;
import star.game_types;
import star.rect;
import star.list;
import star.color;
import star.font;
import star.image_processing;
import star.hash;
import star.data_stream;
import star.string_view;
import star.thread;
import star.directives;
import star.bi_map;
import star.string;
import star.vector;
import star.text;
import star.casting;
import star.strong_typedef;
import star.asset_path;
import star.listener;
import star.input_event;
import star.set;
import star.audio;
import star.map;
import star.image;
import star.math_common;
import star.version;
import star.either;
import star.random;
import star.weighted_pool;
import star.rpc_promise;
import star.ordered_map;
import star.net_compatibility;
import star.worker_pool;

import star.sector_array_2d;
import star.perlin;
import star.btree;
import star.lru_cache;
import star.data_stream_devices;
import star.btree_database;
import star.ordered_set;
import star.io_device;
import star.ref_ptr;
import star.logging;
import star.a_star;
import star.periodic_function;

import star.lua_root;
import star.world_geometry;

import star.collision_block;
import star.liquid_types;
import star.tile_damage;
import star.tile_sector_array;
import star.animation;
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



import star.chat_bubble_separation;
import star.renderer;
import star.font_texture_group;
import star.anchor_types;
import star.asset_source;
import star.assets;
import star.root_base;
import star.configuration;
import star.root;
import star.text_painter;

import star.world_camera;


import star.nameplate_painter;
import star.damage_types;
import star.status_types;
import star.damage;
import star.light_source;
import star.entity;
import star.nametag_entity;
import star.animated_part_set;
import star.networked_animator;
import star.humanoid;
import star.physics_entity;
import star.anchorable_entity;
import star.mobile_entity;
import star.actor_entity;
import star.tool_user_entity;
import star.lounging_entities;
import star.chatty_entity;
import star.emote_entity;
import star.portrait_entity;
import star.damage_bar_entity;
import star.inventory_types;
import star.movement_controller;
import star.platformer_astar_types;
import star.actor_movement_controller;
import star.ai_types;
import star.lua_actor_movement_component;
import star.lua_animation_component;
import star.radio_message_database;
import star.player;
import star.application;
import star.statistics_service;
import star.p2p_networking_service;
import star.user_generated_content_service;
import star.desktop_service;
import star.application_controller;
import star.drawable;
import star.asset_texture_group;
import star.drawable_painter;
import star.gui_types;
import star.key_bindings;
import star.mixer;
import star.gui_context;

namespace Star {
NameplatePainter::NameplatePainter(GuiContext& context) : m_guiContext(context) {}

std::expected<std::unique_ptr<NameplatePainter>, StarException> NameplatePainter::create(GuiContext& context) {
  auto painter = std::unique_ptr<NameplatePainter>(new NameplatePainter(context));
  auto initialized = painter->initialize();
  if (!initialized) return std::unexpected(std::move(initialized.error()));
  return painter;
}

std::expected<void, StarException> NameplatePainter::initialize() {
  auto assets = m_guiContext.get().root().assets();
  if (!assets) return std::unexpected(std::move(assets.error()));
  auto config = (*assets)->json("/interface.config:nametag");
  if (!config) return std::unexpected(std::move(config.error()));
  { auto value = config->getBool("showMasterNames"); if (!value) return std::unexpected(std::move(value.error())); m_showMasterNames = *value; }
  { auto value = config->getFloat("opacityRate"); if (!value) return std::unexpected(std::move(value.error())); m_opacityRate = *value; }
  { auto value = config->queryFloat("inspectOpacityRate", m_opacityRate); if (!value) return std::unexpected(std::move(value.error())); m_inspectOpacityRate = *value; }
  { auto value = config->getFloat("opacityBoost"); if (!value) return std::unexpected(std::move(value.error())); m_opacityBoost = *value; }
  { auto json = config->get("offset"); if (!json) return std::unexpected(std::move(json.error())); auto value = jsonToVec2F(*json); if (!value) return std::unexpected(std::move(value.error())); m_offset = *value; }
  { auto json = config->get("statusOffset"); if (!json) return std::unexpected(std::move(json.error())); auto value = jsonToVec2F(*json); if (!value) return std::unexpected(std::move(value.error())); m_statusOffset = *value; }
  auto textJson = config->get("textStyle");
  if (!textJson) return std::unexpected(std::move(textJson.error()));
  auto text = TextStyle::create(*textJson);
  if (!text) return std::unexpected(std::move(text.error()));
  m_textStyle = std::move(*text);
  auto statusJson = config->get("statusTextStyle", *textJson);
  if (!statusJson) return std::unexpected(std::move(statusJson.error()));
  auto status = TextStyle::create(*statusJson);
  if (!status) return std::unexpected(std::move(status.error()));
  m_statusTextStyle = std::move(*status);
  auto colorJson = config->get("statusColor");
  if (!colorJson) return std::unexpected(std::move(colorJson.error()));
  auto color = jsonToColor(*colorJson);
  if (!color) return std::unexpected(std::move(color.error()));
  m_statusTextStyle.color = color->toRgba();
  auto tween = config->getFloat("tweenFactor");
  if (!tween) return std::unexpected(std::move(tween.error()));
  m_nametags.setTweenFactor(*tween);
  auto threshold = config->getFloat("movementThreshold");
  if (!threshold) return std::unexpected(std::move(threshold.error()));
  m_nametags.setMovementThreshold(*threshold);
  return {};
}

std::expected<void, StarException> NameplatePainter::update(float dt, std::shared_ptr<WorldClient> const& world, WorldCamera const& camera, bool inspectionMode) {
  auto& foundEntities = m_foundEntities;
  foundEntities.clear();
  auto entities = world->query<NametagEntity>(camera.worldScreenRect());
  if (!entities) return std::unexpected(std::move(entities.error()));
  for (auto const& entity : *entities) {
    if ((entity->isMaster() && !m_showMasterNames) || !entity->displayNametag()) continue;
    if (auto player = as<Player>(entity)) {
      if (player->isTeleporting()) continue;
    }
    foundEntities.insert(entity->entityId());
    if (!m_entitiesWithNametags.contains(entity->entityId())) {
      auto name = entity->nametag();
      if (!name) return std::unexpected(std::move(name.error()));
      auto color = entity->nametagColor();
      if (!color) return std::unexpected(std::move(color.error()));
      Nametag nametag = {.name = std::move(*name),
                         .statusText = entity->statusText(),
                         .color = *color,
                         .opacity = 1.0f,
                         .entityId = entity->entityId(),
                         .origin = {}};
      auto bounds = determineBoundBox(Vec2F(), nametag);
      if (!bounds) return std::unexpected(std::move(bounds.error()));
      m_nametags.addBubble(Vec2F(), *bounds, std::move(nametag));
    }
  }
  m_nametags.filter([&foundEntities](BubbleState<Nametag> const&, Nametag const& nametag) { return foundEntities.contains(nametag.entityId); });
  auto updated = m_nametags.forEach([&world, &camera, this, inspectionMode](BubbleState<Nametag>& bubbleState, Nametag& nametag) -> std::expected<void, StarException> {
    if (auto entity = as<NametagEntity>(world->entity(nametag.entityId))) {
      auto origin = entity->nametagOrigin();
      if (!origin) return std::unexpected(std::move(origin.error()));
      nametag.origin = *origin;
      bubbleState.idealDestination = camera.worldToScreen(*origin) + m_offset * camera.pixelRatio();
      auto bounds = determineBoundBox(bubbleState.idealDestination, nametag);
      if (!bounds) return std::unexpected(std::move(bounds.error()));
      bubbleState.boundBox = *bounds;
      nametag.statusText = entity->statusText();
      auto name = entity->nametag();
      if (!name) return std::unexpected(std::move(name.error()));
      nametag.name = std::move(*name);
      auto color = entity->nametagColor();
      if (!color) return std::unexpected(std::move(color.error()));
      nametag.color = *color;
      auto fullyOnScreen = world->geometry().rectContains(camera.worldScreenRect(), entity->position());
      if (!fullyOnScreen) return std::unexpected(std::move(fullyOnScreen.error()));
      if (inspectionMode) nametag.opacity = approach(1.0f, nametag.opacity, m_inspectOpacityRate);
      else if (*fullyOnScreen) nametag.opacity = approach(0.0f, nametag.opacity, m_opacityRate);
      else nametag.opacity = approach(m_opacityBoost, nametag.opacity, m_opacityRate);
    }
    return {};
  });
  if (!updated) return std::unexpected(std::move(updated.error()));
  std::swap(m_entitiesWithNametags, m_foundEntities);
  m_nametags.update(dt);
  return {};
}

std::expected<void, StarException> NameplatePainter::render(WorldClient const& world, WorldCamera const& camera, PresentationContext presentation) {
  auto& context = m_guiContext.get();
  return m_nametags.forEach([&context, &world, &camera, presentation, this](BubbleState<Nametag> const& bubble, Nametag const& nametag) -> std::expected<void, StarException> {
    if (nametag.opacity == 0.0f) return {};
    auto entity = world.entity(nametag.entityId);
    if (!entity)
      return {};
    auto origin = nametag.origin + world.geometry().diff(world.renderPosition(nametag.entityId, presentation), entity->position());
    auto position = camera.worldToScreen(origin) + m_offset * camera.pixelRatio()
        + (bubble.currentPosition - bubble.idealDestination);
    auto style = context.setTextStyle(m_textStyle);
    if (!style) return std::unexpected(std::move(style.error()));
    auto color = Color::rgb(nametag.color);
    color.setAlphaF(nametag.opacity);
    style->get().color = color.toRgba();
    auto name = context.renderText(nametag.name, namePosition(position));
    if (!name) return std::unexpected(std::move(name.error()));
    if (nametag.statusText) {
      auto status = context.setTextStyle(m_statusTextStyle);
      if (!status) return std::unexpected(std::move(status.error()));
      status->get().color[3] *= nametag.opacity;
      auto drawn = context.renderText(*nametag.statusText, statusPosition(position));
      if (!drawn) return std::unexpected(std::move(drawn.error()));
    }
    return context.clearTextStyle();
  });
}

TextPositioning NameplatePainter::namePosition(Vec2F position) const {
  return TextPositioning(position, HorizontalAnchor::HMidAnchor, VerticalAnchor::BottomAnchor);
}

TextPositioning NameplatePainter::statusPosition(Vec2F position) const {
  return TextPositioning(position + m_statusOffset * m_guiContext.get().interfaceScale(), HorizontalAnchor::HMidAnchor, VerticalAnchor::BottomAnchor);
}

std::expected<RectF, StarException> NameplatePainter::determineBoundBox(Vec2F position, Nametag const& nametag) const {
  auto& context = m_guiContext.get();
  auto style = context.setTextStyle(m_textStyle);
  if (!style) return std::unexpected(std::move(style.error()));
  auto bounds = context.determineTextSize(nametag.name, namePosition(position));
  if (!bounds) return std::unexpected(std::move(bounds.error()));
  if (nametag.statusText) {
    auto status = context.setTextStyle(m_statusTextStyle);
    if (!status) return std::unexpected(std::move(status.error()));
    auto statusBounds = context.determineTextSize(*nametag.statusText, statusPosition(position));
    if (!statusBounds) return std::unexpected(std::move(statusBounds.error()));
    bounds->combine(*statusBounds);
  }
  auto cleared = context.clearTextStyle();
  if (!cleared) return std::unexpected(std::move(cleared.error()));
  return *bounds;
}

}

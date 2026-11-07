import std;
import star.format;
import star.xx_hash;
import star.json;
import star.id_map;
import star.vector;
import star.casting;
import star.input_event;
import star.color;
import star.font;
import star.list;
import star.rect;
import star.image_processing;
import star.hash;
import star.data_stream;
import star.string_view;
import star.thread;
import star.directives;
import star.bi_map;
import star.string;
import star.text;
import star.asset_path;
import star.listener;
import star.game_types;
import star.set;
import star.audio;
import star.map;
import star.strong_typedef;
import star.image;
import star.math_common;
import star.version;
import star.either;
import star.random;
import star.weighted_pool;
import star.ordered_map;
import star.lru_cache;
import star.time;
import star.ttl_cache;
import star.perlin;
import star.random_point;
import star.io_device;
import star.ref_ptr;
import star.logging;
import star.rpc_promise;
import star.net_compatibility;
import star.worker_pool;

import star.sector_array_2d;
import star.btree;
import star.data_stream_devices;
import star.btree_database;
import star.ordered_set;
import star.a_star;
import star.periodic_function;
import star.spline;


import star.application;
import star.statistics_service;
import star.p2p_networking_service;
import star.user_generated_content_service;
import star.desktop_service;
import star.application_controller;
import star.renderer;
import star.lua_root;
import star.asset_source;
import star.assets;
import star.root_base;
import star.configuration;
import star.root;
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
import star.widget;
import star.pane;
import star.world_geometry;
import star.wiring;


import star.wire_interface;
import star.widget_parsing;
import star.gui_reader;
import star.tile_sector_array;
import star.force_regions;
import star.world_parameters;
import star.celestial_parameters;
import star.world_layout;
import star.celestial_types;
import star.chat_types;
import star.uuid;
import star.tile_modification;
import star.warping;
import star.inspectable_entity;
import star.plant;
import star.biome_placement;
import star.versioning_database;
import star.world_storage;
import star.player_types;
import star.system_world;
import star.damage_manager;
import star.net_packets;
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
import star.damage_types;
import star.status_types;
import star.damage;
import star.light_source;
import star.entity;
import star.tile_damage;
import star.interaction_types;
import star.item_descriptor;
import star.celestial_coordinate;
import star.quest_descriptor;
import star.interactive_entity;
import star.collision_block;
import star.tile_entity;
import star.wire_entity;
import star.liquid_types;
import star.collision_generator;
import star.world_tiles;
import star.entity_rendering_types;
import star.sky_types;
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
import star.tile_drawer;
import star.tile_painter;
import star.environment_painter;
import star.world_camera;
import star.world_painter;
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
import star.nametag_entity;
import star.inventory_types;
import star.movement_controller;
import star.platformer_astar_types;
import star.actor_movement_controller;
import star.ai_types;
import star.lua_actor_movement_component;
import star.lua_animation_component;
import star.radio_message_database;
import star.player;
import star.item;
import star.non_rotated_drawables_item;
import star.tool_user_item;
import star.beam_item;
import star.status_effect_item;
import star.fireable_item;
import star.swingable_item;
import star.durability_item;
import star.pointable_item;
struct PreviewTile;
import star.preview_tile_tool;
import star.tools;

namespace Star {
WirePane::WirePane(GuiContext& guiContext, std::shared_ptr<WorldClient> worldClient, std::shared_ptr<Player> player, std::shared_ptr<WorldPainter> worldPainter)
  : Pane(guiContext), m_worldClient(std::move(worldClient)), m_player(std::move(player)), m_worldPainter(std::move(worldPainter)), m_connecting(false) {}

std::expected<std::shared_ptr<WirePane>, StarException> WirePane::create(GuiContext& guiContext, std::shared_ptr<WorldClient> worldClient, std::shared_ptr<Player> player, std::shared_ptr<WorldPainter> worldPainter) {
  auto pane = std::shared_ptr<WirePane>(new WirePane(guiContext, std::move(worldClient), std::move(player), std::move(worldPainter)));
  auto initialized = pane->initialize();
  if (!initialized) return std::unexpected(std::move(initialized.error()));
  return pane;
}

std::expected<void, StarException> WirePane::initialize() {
  auto base = initializePane();
  if (!base) return std::unexpected(std::move(base.error()));
  auto assets = context().root().assets();
  if (!assets) return std::unexpected(std::move(assets.error()));
  auto gui = (*assets)->json("/interface/wires/wires.config:gui");
  if (!gui) return std::unexpected(std::move(gui.error()));
  GuiReader reader(context());
  auto constructed = reader.construct(*gui, *this);
  if (!constructed) return std::unexpected(std::move(constructed.error()));
  m_nodeSize = Vec2F(1.8f, 1.8f);
  auto config = (*assets)->json("/player.config:wireConfig");
  if (!config) return std::unexpected(std::move(config.error()));
  {
    auto value = config->getFloat("minWireWidth");
    if (!value) return std::unexpected(std::move(value.error()));
    m_minBeamWidth = *value;
  }
  {
    auto value = config->getFloat("maxWireWidth");
    if (!value) return std::unexpected(std::move(value.error()));
    m_maxBeamWidth = *value;
  }
  {
    auto value = config->getFloat("wireWidthDev", (m_maxBeamWidth - m_minBeamWidth) / 3);
    if (!value) return std::unexpected(std::move(value.error()));
    m_beamWidthDev = *value;
  }
  {
    auto value = config->getFloat("minWireTrans");
    if (!value) return std::unexpected(std::move(value.error()));
    m_minBeamTrans = *value;
  }
  {
    auto value = config->getFloat("maxWireTrans");
    if (!value) return std::unexpected(std::move(value.error()));
    m_maxBeamTrans = *value;
  }
  {
    auto value = config->getFloat("wireTransDev", (m_maxBeamTrans - m_minBeamTrans) / 3);
    if (!value) return std::unexpected(std::move(value.error()));
    m_beamTransDev = *value;
  }
  {
    auto value = config->getFloat("innerBrightnessScale");
    if (!value) return std::unexpected(std::move(value.error()));
    m_innerBrightnessScale = *value;
  }
  {
    auto value = config->getFloat("firstStripeThickness");
    if (!value) return std::unexpected(std::move(value.error()));
    m_firstStripeThickness = *value;
  }
  {
    auto value = config->getFloat("secondStripeThickness");
    if (!value) return std::unexpected(std::move(value.error()));
    m_secondStripeThickness = *value;
  }
  auto titled = setTitle({}, "", "Wire you looking at me like that?");
  if (!titled) return std::unexpected(std::move(titled.error()));
  disableScissoring();
  markAsContainer();
  return {};
}

void WirePane::reset() { m_connecting = false; }

std::expected<void, StarException> WirePane::update(float) {
  if (!active()) return {};
  if (!m_worldClient->inWorld()) { dismiss(); return {}; }
  if (m_connecting) {
    auto entities = m_worldClient->atTile<WireEntity>(m_sourceConnector.entityLocation);
    if (!entities) return std::unexpected(std::move(entities.error()));
    for (auto const& entity : *entities) {
      if (m_sourceConnector.nodeIndex < entity->nodeCount(m_sourceDirection)) return {};
    }
    // Cancel a pending connection when its source node disappears.
    m_connecting = false;
  }
  return {};
}

std::expected<void, StarException> WirePane::renderWire(Vec2F from, Vec2F to, Color baseColor) {
  if (m_worldClient->isTileProtected(Vec2I::floor(from)) || m_worldClient->isTileProtected(Vec2I::floor(to)))
    return {};

  from = m_worldPainter->camera().worldToScreen(from);
  to = m_worldPainter->camera().worldToScreen(to);

  auto rangeRand = [&](float dev, float min, float max) {
    return clamp<float>(context().root().presentationRandom().nrandf(dev, max), min, max);
  };

  float lineThickness = m_worldPainter->camera().pixelRatio() * rangeRand(m_beamWidthDev, m_minBeamWidth, m_maxBeamWidth);
  float beamTransparency = rangeRand(m_beamTransDev, m_minBeamTrans, m_maxBeamTrans);
  baseColor.setAlphaF(baseColor.alphaF() * beamTransparency);
  Color innerStripe = baseColor;
  innerStripe.setValue(1 - (1 - innerStripe.value()) / m_innerBrightnessScale);
  innerStripe.setSaturation(innerStripe.saturation() / m_innerBrightnessScale);
  Color firstStripe = innerStripe;
  innerStripe.setValue(1 - (1 - innerStripe.value()) / m_innerBrightnessScale);
  innerStripe.setSaturation(innerStripe.saturation() / m_innerBrightnessScale);
  Color secondStripe = innerStripe;

  { auto drawn = context().drawLine(from, to, baseColor.toRgba(), lineThickness); if (!drawn) return std::unexpected(std::move(drawn.error())); }
  { auto drawn = context().drawLine(from, to, firstStripe.toRgba(), lineThickness * m_firstStripeThickness); if (!drawn) return std::unexpected(std::move(drawn.error())); }
  { auto drawn = context().drawLine(from, to, secondStripe.toRgba(), lineThickness * m_secondStripeThickness); if (!drawn) return std::unexpected(std::move(drawn.error())); }
  return {};
}

std::expected<void, StarException> WirePane::renderImpl() {
  if (!m_worldClient->inWorld()) return {};
  auto const& camera = m_worldPainter->camera();
  auto entities = m_worldClient->query<WireEntity>(RectF(m_worldClient->clientWindow()));
  if (!entities) return std::unexpected(std::move(entities.error()));
  auto badWire =
    Color::rgbf(0.6f + static_cast<float>(std::sin(Time::monotonicTime() * std::numbers::pi * 2.0)) * 0.4f, 0.0f, 0.0f);
  float phase = 0.5f + 0.5f * std::sin(static_cast<double>(Time::monotonicMilliseconds()) / 100.0);
  auto drawLineColor = Color::Red.mix(Color::White, phase);
  for (auto const& entity : *entities) {
    for (auto direction : {WireDirection::Input, WireDirection::Output}) {
      for (std::size_t i = 0; i < entity->nodeCount(direction); ++i) {
        WireNode node{.direction = direction, .nodeIndex = i};
        auto offset = entity->nodePosition(node);
        if (!offset) return std::unexpected(std::move(offset.error()));
        Vec2I position = entity->tilePosition() + *offset;
        if (m_worldClient->isTileProtected(position)) continue;
        auto icon = entity->nodeIcon(node);
        if (!icon) return std::unexpected(std::move(icon.error()));
        auto size = context().textureSize(*icon);
        if (!size) return std::unexpected(std::move(size.error()));
        auto drawn = context().drawQuad(*icon, camera.worldToScreen(centerOfTile(position) - ((Vec2F(*size) / TilePixels) / 2.0f)), camera.pixelRatio(), Color::White.toRgba());
        if (!drawn) return std::unexpected(std::move(drawn.error()));
      }
    }
  }
  HashSet<std::pair<WireConnection, WireConnection>> visitedConnections;
  for (auto const& entity : *entities) {
    for (auto direction : {WireDirection::Input, WireDirection::Output}) {
      for (std::size_t i = 0; i < entity->nodeCount(direction); ++i) {
        WireNode node{.direction = direction, .nodeIndex = i};
        auto offset = entity->nodePosition(node);
        if (!offset) return std::unexpected(std::move(offset.error()));
        auto connections = entity->connectionsForNode(node);
        if (!connections) return std::unexpected(std::move(connections.error()));
        auto color = entity->nodeColor(node);
        if (!color) return std::unexpected(std::move(color.error()));
        Vec2I tile = entity->tilePosition();
        Vec2I position = tile + *offset;
        Color wire = *color;
        if (direction == WireDirection::Input) wire = wire.mix(Color::Black, 0.8f);
        else {
          auto state = entity->nodeState(node);
          if (!state) return std::unexpected(std::move(state.error()));
          if (!*state) wire = wire.mix(Color::Black, 0.8f);
        }
        for (auto const& connection : *connections) {
          if (direction == WireDirection::Input) wire = color->mix(Color::Black, 0.8f);
          if (direction == WireDirection::Input)
            visitedConnections.add({{.entityLocation = tile, .nodeIndex = i}, connection});
          else if (visitedConnections.add({connection, {.entityLocation = tile, .nodeIndex = i}}))
            continue;
          Vec2I otherPosition = connection.entityLocation;
          auto sources = m_worldClient->atTile<WireEntity>(connection.entityLocation);
          if (!sources) return std::unexpected(std::move(sources.error()));
          if (auto source = sources->get(0)) {
            auto otherDirection = direction == WireDirection::Input ? WireDirection::Output : WireDirection::Input;
            if (connection.nodeIndex < source->nodeCount(otherDirection)) {
              WireNode otherNode{.direction = otherDirection, .nodeIndex = connection.nodeIndex};
              auto otherOffset = source->nodePosition(otherNode);
              if (!otherOffset) return std::unexpected(std::move(otherOffset.error()));
              otherPosition += *otherOffset;
              if (direction == WireDirection::Input) {
                auto otherColor = source->nodeColor(otherNode);
                if (!otherColor) return std::unexpected(std::move(otherColor.error()));
                auto state = source->nodeState(otherNode);
                if (!state) return std::unexpected(std::move(state.error()));
                wire = *otherColor;
                if (!*state) wire = wire.mix(Color::Black, 0.8f);
              }
            } else wire = badWire;
          }
          auto drawn = renderWire(centerOfTile(position), centerOfTile(otherPosition), wire);
          if (!drawn) return std::unexpected(std::move(drawn.error()));
        }
      }
    }
  }
  if (m_connecting) {
    Vec2F aimPos = camera.screenToWorld(Vec2F(m_mousePos) * context().interfaceScale());
    Vec2I sourcePosition = m_sourceConnector.entityLocation;
    auto sources = m_worldClient->atTile<WireEntity>(m_sourceConnector.entityLocation);
    if (!sources) return std::unexpected(std::move(sources.error()));
    if (auto source = sources->get(0)) {
      WireNode node{.direction = m_sourceDirection, .nodeIndex = m_sourceConnector.nodeIndex};
      auto offset = source->nodePosition(node);
      if (!offset) return std::unexpected(std::move(offset.error()));
      auto color = source->nodeColor(node);
      if (!color) return std::unexpected(std::move(color.error()));
      sourcePosition += *offset;
      drawLineColor = color->mix(Color::White, phase);
    }
    return renderWire(centerOfTile(sourcePosition), aimPos, drawLineColor);
  }
  return {};
}

std::expected<bool, StarException> WirePane::sendEvent(InputEvent const& event) {
  if (std::holds_alternative<MouseMoveEvent>(event) || std::holds_alternative<MouseButtonDownEvent>(event)) m_mousePos = *context().mousePosition(event);
  return false;
}

std::expected<WireConnector::SwingResult, StarException> WirePane::swing(WorldGeometry const& geometry, Vec2F pos, FireMode mode) {
  pos = geometry.xwrap(pos);
  if (m_worldClient->isTileProtected(Vec2I(pos))) {
    m_connecting = false;
    return Protected;
  }
  RectF bounds = {pos - Vec2F(16, 16), pos + Vec2F(16, 16)};
  auto entities = m_worldClient->query<WireEntity>(bounds);
  if (!entities) return std::unexpected(std::move(entities.error()));
  std::optional<WireConnection> matchNode;
  WireDirection matchDirection = WireDirection::Output;
  float bestDist = 10000;
  if (mode != FireMode::Primary) m_connecting = false;
  for (auto const& entity : *entities) {
    for (auto direction : {WireDirection::Input, WireDirection::Output}) {
      for (std::size_t i = 0; i < entity->nodeCount(direction); ++i) {
        WireNode node{.direction = direction, .nodeIndex = i};
        auto offset = entity->nodePosition(node);
        if (!offset) return std::unexpected(std::move(offset.error()));
        Vec2F center = centerOfTile(entity->tilePosition() + *offset);
        auto nodeBounds = RectF::withSize(center - m_nodeSize / 2.0f, m_nodeSize);
        auto contains = geometry.rectContains(nodeBounds, pos);
        if (!contains) return std::unexpected(std::move(contains.error()));
        if (!*contains) continue;
        if (mode != FireMode::Primary) {
          auto connections = entity->connectionsForNode(node);
          if (!connections) return std::unexpected(std::move(connections.error()));
          if (connections->empty()) continue;
        }
        float distance = geometry.diff(center, pos).magnitudeSquared();
        if (!matchNode || distance < bestDist) {
          matchNode = WireConnection{.entityLocation = entity->tilePosition(), .nodeIndex = i};
          matchDirection = direction;
          bestDist = distance;
        }
      }
    }
  }
  if (!matchNode) return Nothing;
  if (mode != FireMode::Primary) {
    m_worldClient->disconnectAllWires(matchNode->entityLocation,
                                      WireNode{.direction = matchDirection, .nodeIndex = matchNode->nodeIndex});
    return Connect;
  }
  if (m_connecting) {
    if (m_sourceDirection == matchDirection || m_sourceConnector.entityLocation == matchNode->entityLocation) return Mismatch;
    if (matchDirection == WireDirection::Output) {
      m_worldClient->connectWire(*matchNode, m_sourceConnector);
    } else {
      m_worldClient->connectWire(m_sourceConnector, *matchNode);
    }
  } else {
    m_connecting = true;
    m_sourceDirection = matchDirection;
    m_sourceConnector = *matchNode;
  }
  return Connect;
}

bool WirePane::connecting() { return m_connecting; }

}

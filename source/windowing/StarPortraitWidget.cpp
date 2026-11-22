import std;
import star.json;
import star.json_extra;
import star.list;
import star.rect;
import star.image_processing;
import star.hash;
import star.data_stream;
import star.string_view;
import star.thread;
import star.directives;
import star.asset_path;
import star.vector;
import star.casting;
import star.input_event;
import star.color;
import star.font;
import star.bi_map;
import star.ordered_map;
import star.io_device;
import star.ref_ptr;
import star.logging;
import star.string;
import star.text;
import star.listener;
import star.game_types;
import star.set;
import star.audio;
import star.map;
import star.id_map;
import star.rpc_promise;
import star.strong_typedef;
import star.a_star;
import star.ordered_set;
import star.random;
import star.periodic_function;

import star.application;
import star.statistics_service;
import star.p2p_networking_service;
import star.user_generated_content_service;
import star.desktop_service;
import star.image;
import star.application_controller;
import star.renderer;
import star.lua_root;
import star.asset_source;
import star.assets;
import star.root_base;
import star.configuration;
import star.root;
import star.uuid;
import star.item_descriptor;
import star.animation;
import star.particle;
import star.animated_part_set;
import star.light_source;
import star.networked_animator;
import star.humanoid;
import star.damage_types;
import star.world_geometry;
import star.status_types;
import star.damage;
import star.entity;
import star.interaction_types;
import star.tile_damage;
import star.interactive_entity;
import star.collision_block;
import star.tile_entity;
import star.tile_modification;
import star.force_regions;
import star.world;
import star.physics_entity;
import star.anchorable_entity;
import star.mobile_entity;
import star.actor_entity;
import star.tool_user_entity;
import star.lounging_entities;
import star.chat_action;
import star.chatty_entity;
import star.emote_entity;
import star.portrait_entity;
import star.damage_bar_entity;
import star.nametag_entity;
import star.inspectable_entity;
import star.inventory_types;
import star.movement_controller;
import star.platformer_astar_types;
import star.game_timers;
import star.actor_movement_controller;
import star.celestial_coordinate;
import star.quest_descriptor;
import star.ai_types;
import star.entity_rendering_types;
import star.entity_rendering;
import star.player_types;
import star.lua_components;
import star.lua_actor_movement_component;
import star.lua_animation_component;
import star.radio_message_database;
import star.player;
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
import star.portrait_widget;


import star.image_metadata_database;

import star.format;
namespace Star {

PortraitWidget::PortraitWidget(GuiContext& context, std::shared_ptr<PortraitEntity> entity, PortraitMode mode)
  : Widget(context), m_entity(std::move(entity)), m_portraitMode(mode), m_scale(1), m_renderHumanoid(false), m_iconMode(false) {}

std::expected<std::shared_ptr<PortraitWidget>, StarException> PortraitWidget::create(GuiContext& context, std::shared_ptr<PortraitEntity> entity, PortraitMode mode) {
  auto widget = std::shared_ptr<PortraitWidget>(new PortraitWidget(context, std::move(entity), mode));
  auto result = widget->init();
  if (!result) return std::unexpected(std::move(result.error()));
  return widget;
}

RectI PortraitWidget::getScissorRect() const {
  return noScissor();
}

std::expected<void, StarException> PortraitWidget::renderImpl() {
  auto metadata = context().root().imageMetadataDatabase();
  if (!metadata) return std::unexpected(std::move(metadata.error()));
  Vec2I offset;
  if (m_iconMode) {
    auto imageSize = (*metadata)->imageSize(m_iconImage);
    if (!imageSize) return std::unexpected(std::move(imageSize.error()));
    offset = Vec2I(m_scale * Vec2F(*imageSize) / 2) + m_iconOffset;
    auto result = context().drawInterfaceQuad(m_iconImage, Vec2F(screenPosition()), m_scale);
    if (!result) return std::unexpected(std::move(result.error()));
  }
  if (m_entity) {
    std::shared_ptr<Humanoid> humanoid;
    if (m_renderHumanoid)
      if (auto player = as<Player>(m_entity))
        if (!player->isPermaDead()) humanoid = player->humanoid();
    auto portrait = humanoid ? humanoid->render(false, false) : m_entity->portrait(m_portraitMode);
    if (!portrait) return std::unexpected(std::move(portrait.error()));
    for (auto& drawable : *portrait) {
      drawable.scale(humanoid ? m_scale * 8.0f : m_scale);
      auto result = context().drawInterfaceDrawable(drawable, Vec2F(screenPosition() + offset));
      if (!result) return std::unexpected(std::move(result.error()));
    }
  } else {
    auto const& image = m_portraitMode == PortraitMode::Bust || m_portraitMode == PortraitMode::Head ? m_noEntityImagePart : m_noEntityImageFull;
    auto imageSize = (*metadata)->imageSize(image);
    if (!imageSize) return std::unexpected(std::move(imageSize.error()));
    auto pos = offset - Vec2I(m_scale * Vec2F(*imageSize) * 0.5);
    return context().drawInterfaceQuad(image, Vec2F(screenPosition() + pos), m_scale);
  }
  return {};
}

std::expected<void, StarException> PortraitWidget::init() {
  auto assets = context().root().assets();
  if (!assets) return std::unexpected(std::move(assets.error()));
  auto fullJson = (*assets)->json("/interface.config:portraitNullPlayerImageFull");
  if (!fullJson) return std::unexpected(std::move(fullJson.error()));
  auto full = fullJson->to_string();
  if (!full) return std::unexpected(std::move(full.error()));
  m_noEntityImageFull = AssetPath(*full);
  auto partJson = (*assets)->json("/interface.config:portraitNullPlayerImagePart");
  if (!partJson) return std::unexpected(std::move(partJson.error()));
  auto part = partJson->to_string();
  if (!part) return std::unexpected(std::move(part.error()));
  m_noEntityImagePart = AssetPath(*part);
  auto iconJson = (*assets)->json("/interface.config:portraitIconImage");
  if (!iconJson) return std::unexpected(std::move(iconJson.error()));
  auto icon = iconJson->to_string();
  if (!icon) return std::unexpected(std::move(icon.error()));
  m_iconImage = AssetPath(*icon);
  auto offsetJson = (*assets)->json("/interface.config:portraitIconOffset");
  if (!offsetJson) return std::unexpected(std::move(offsetJson.error()));
  auto offset = json_to_vec2i(*offsetJson);
  if (!offset) return std::unexpected(std::move(offset.error()));
  m_iconOffset = *offset;
  return updateSize();
}

std::expected<void, StarException> PortraitWidget::setEntity(std::shared_ptr<PortraitEntity> entity) {
  m_entity = std::move(entity);
  return updateSize();
}

std::expected<void, StarException> PortraitWidget::setMode(PortraitMode mode) {
  m_portraitMode = mode;
  return updateSize();
}

std::expected<void, StarException> PortraitWidget::setScale(float scale) {
  m_scale = scale;
  return updateSize();
}

std::expected<void, StarException> PortraitWidget::setIconMode() {
  m_iconMode = true;
  return updateSize();
}

void PortraitWidget::setRenderHumanoid(bool renderHumanoid) {
  m_renderHumanoid = renderHumanoid;
}

std::expected<bool, StarException> PortraitWidget::sendEvent(InputEvent const&) {
  return false;
}

std::expected<void, StarException> PortraitWidget::updateSize() {
  auto metadata = context().root().imageMetadataDatabase();
  if (!metadata) return std::unexpected(std::move(metadata.error()));
  if (!m_iconMode && m_entity) {
    auto portrait = m_entity->portrait(m_portraitMode);
    if (!portrait) return std::unexpected(std::move(portrait.error()));
    auto bounds = Drawable::boundBoxAll(**metadata, *portrait, false);
    if (!bounds) return std::unexpected(std::move(bounds.error()));
    setSize(Vec2I((bounds->size() * TilePixels * m_scale).ceil()));
  } else {
    auto const& image = m_iconMode || m_portraitMode == PortraitMode::Bust || m_portraitMode == PortraitMode::Head ? m_iconImage : m_noEntityImageFull;
    auto imageSize = (*metadata)->imageSize(image);
    if (!imageSize) return std::unexpected(std::move(imageSize.error()));
    setSize(Vec2I(*imageSize * m_scale));
  }
  return {};
}

}

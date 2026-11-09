import star.world_lua_bindings;
import std;
import star.format;
import star.xx_hash;
import star.json;
import star.json_extra;
import star.vector;
import star.casting;
import star.input_event;
import star.color;
import star.font;
import star.list;
import star.image_processing;
import star.hash;
import star.data_stream;
import star.string_view;
import star.thread;
import star.directives;
import star.bi_map;
import star.ordered_map;
import star.io_device;
import star.ref_ptr;
import star.logging;
import star.version;
import star.string;
import star.text;
import star.asset_path;
import star.listener;
import star.game_types;
import star.set;
import star.audio;
import star.map;
import star.id_map;
import star.image;
import star.either;
import star.random;
import star.weighted_pool;
import star.strong_typedef;
import star.rpc_promise;
import star.net_compatibility;
import star.worker_pool;

import star.sector_array_2d;
import star.perlin;
import star.btree;
import star.lru_cache;
import star.data_stream_devices;
import star.btree_database;
import star.ordered_set;
import star.a_star;
import star.periodic_function;
import star.byte_array;

typedef struct ZSTD_CCtx_s ZSTD_CCtx;
typedef struct ZSTD_DCtx_s ZSTD_DCtx;
typedef ZSTD_DCtx ZSTD_DStream;
typedef ZSTD_CCtx ZSTD_CStream;
import star.zstd_compression;
import star.lua;



import star.application;
import star.statistics_service;
import star.p2p_networking_service;
import star.user_generated_content_service;
import star.desktop_service;
import star.application_controller;
import star.renderer;
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
import star.lua_components;
import star.widget_parsing;
import star.gui_reader;


import star.base_script_pane;


import star.script_pane;
import star.lua_game_converters;
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
import star.world_geometry;
import star.wiring;
import star.quest_descriptor;
import star.interactive_entity;
import star.tile_entity;
import star.inspectable_entity;
import star.plant;
import star.plant_database;
import star.biome_placement;
import star.lua_root;
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
import star.world_client_state;
import star.interpolation_tracker;
import star.ambient;
import star.weather;
import star.game_timers;
import star.world_client;
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
import star.host_address;
import star.socket;
import star.tcp;
import star.net_packet_socket;
import star.universe_connection;
import star.world_client_thread;
import star.universe;
// TODO: make this more thread safe
import star.universe_client;
import star.widget_lua_bindings;
import star.canvas_widget;
import star.item;
import star.progress_widget;
import star.animation;
import star.item_slot_widget;
import star.item_grid_widget;
import star.image_widget;

import star.player_lua_bindings;
import star.simple_tooltip;
import star.item_tooltip;
import star.config_lua_bindings;
import star.celestial_lua_bindings;
import star.status_controller_lua_bindings;

namespace Star {

ScriptPane::ScriptPane(GuiContext& guiContext, std::shared_ptr<UniverseClient> client, EntityId sourceEntityId)
  : BaseScriptPane(guiContext), m_client(std::move(client)), m_sourceEntityId(sourceEntityId) {}

std::expected<std::shared_ptr<ScriptPane>, StarException> ScriptPane::create(GuiContext& guiContext, std::shared_ptr<UniverseClient> client, Json config, EntityId sourceEntityId) {
  auto pane = std::shared_ptr<ScriptPane>(new ScriptPane(guiContext, std::move(client), sourceEntityId));
  auto initialized = pane->init(std::move(config));
  if (!initialized)
    return std::unexpected(std::move(initialized.error()));
  auto player = LuaBindings::makePlayerCallbacks(guiContext.root(), *pane->m_client->mainPlayer());
  if (!player)
    return std::unexpected(std::move(player.error()));
  auto playerAdded = pane->m_script.addCallbacks("player", std::move(*player));
  if (!playerAdded)
    return std::unexpected(std::move(playerAdded.error()));
  auto status = LuaBindings::makeStatusControllerCallbacks(pane->m_client->mainPlayer()->statusController());
  if (!status)
    return std::unexpected(std::move(status.error()));
  auto statusAdded = pane->m_script.addCallbacks("status", std::move(*status));
  if (!statusAdded)
    return std::unexpected(std::move(statusAdded.error()));
  auto celestial = LuaBindings::makeCelestialCallbacks(guiContext.root(), *pane->m_client);
  if (!celestial)
    return std::unexpected(std::move(celestial.error()));
  auto celestialAdded = pane->m_script.addCallbacks("celestial", std::move(*celestial));
  if (!celestialAdded)
    return std::unexpected(std::move(celestialAdded.error()));
  return pane;
}

std::expected<void, StarException> ScriptPane::displayed() {
  auto world = m_client->worldClient();
  if (world && world->inWorld()) {
    auto configured = m_script.setLuaRoot(world->luaRoot());
    if (!configured)
      return std::unexpected(std::move(configured.error()));
    auto callbacks = LuaBindings::makeWorldCallbacks(context().root(), *world);
    if (!callbacks)
      return std::unexpected(std::move(callbacks.error()));
    auto added = m_script.addCallbacks("world", std::move(*callbacks));
    if (!added)
      return std::unexpected(std::move(added.error()));
  }
  return BaseScriptPane::displayed();
}

std::expected<void, StarException> ScriptPane::dismissed() {
  auto dismissed = BaseScriptPane::dismissed();
  auto removed = m_script.removeCallbacks("world");
  if (!dismissed)
    return std::unexpected(std::move(dismissed.error()));
  if (!removed)
    return std::unexpected(std::move(removed.error()));
  return {};
}

std::expected<void, StarException> ScriptPane::tick(float dt) {
  if (m_sourceEntityId != NullEntityId) {
    auto reachable = m_client->worldClient()->playerCanReachEntity(m_sourceEntityId);
    if (!reachable)
      return std::unexpected(std::move(reachable.error()));
    if (!*reachable)
      dismiss();
  }
  return BaseScriptPane::tick(dt);
}

std::expected<std::shared_ptr<Pane>, StarException> ScriptPane::createTooltip(Vec2I const& screenPosition) {
  auto result = recoverLuaScriptError(m_script.invoke<Json>("createTooltip", screenPosition));
  if (!result)
    return std::unexpected(std::move(result.error()));
  if (*result && !(**result).isNull()) {
    auto const& value = **result;
    if (value.type() == Json::Type::String) {
      auto text = value.toString();
      if (!text)
        return std::unexpected(std::move(text.error()));
      return SimpleTooltipBuilder::buildTooltip(context(), *text);
    }
    auto tooltip = Pane::create(context());
    if (!tooltip)
      return std::unexpected(std::move(tooltip.error()));
    auto constructed = m_reader->construct(value, **tooltip);
    if (!constructed)
      return std::unexpected(std::move(constructed.error()));
    return std::move(*tooltip);
  }
  std::shared_ptr<Item> item;
  if (auto child = getChildAt(screenPosition)) {
    if (auto itemSlot = as<ItemSlotWidget>(child))
      item = itemSlot->item();
    if (auto itemGrid = as<ItemGridWidget>(child)) {
      auto selected = itemGrid->itemAt(screenPosition);
      if (!selected)
        return std::unexpected(std::move(selected.error()));
      item = std::move(*selected);
    }
  }
  if (item)
    return ItemTooltipBuilder::buildItemTooltip(context(), item, m_client->mainPlayer());
  return std::shared_ptr<Pane>{};
}

std::expected<LuaCallbacks, StarException> ScriptPane::makePaneCallbacks() {
  auto callbacks = BaseScriptPane::makePaneCallbacks();
  if (!callbacks)
    return std::unexpected(std::move(callbacks.error()));
  auto registered = callbacks->registerCallback("sourceEntity", [this]() { return m_sourceEntityId; });
  if (!registered)
    return std::unexpected(std::move(registered.error()));
  return std::move(*callbacks);
}

std::expected<bool, StarException> ScriptPane::openWithInventory() const {
  return m_config.getBool("openWithInventory", false);
}

std::expected<bool, StarException> ScriptPane::closeWithInventory() const {
  auto open = openWithInventory();
  if (!open)
    return std::unexpected(std::move(open.error()));
  return m_config.getBool("closeWithInventory", *open);
}

EntityId ScriptPane::sourceEntityId() const {
  return m_sourceEntityId;
}

}

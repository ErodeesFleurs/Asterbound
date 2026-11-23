module;
import std;
import star.json;
import star.json_extra;
import star.xx_hash;
import star.id_map;
import star.lua;
import star.vector;
import star.lua_converters;
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
import star.string;
import star.text;
import star.asset_path;
import star.listener;
import star.game_types;
import star.set;
import star.audio;
import star.map;
import star.strong_typedef;
import star.rpc_promise;
import star.ordered_map;
import star.byte_array;
import star.encode;
import star.bytes;
import star.format;
import star.time;
import star.io_device;
import star.ref_ptr;
import star.logging;
import star.random;
import star.weighted_pool;
import star.ordered_set;
import star.data_stream_devices;

typedef struct ZSTD_CCtx_s ZSTD_CCtx;
typedef struct ZSTD_DCtx_s ZSTD_DCtx;
typedef ZSTD_DCtx ZSTD_DStream;
typedef ZSTD_CCtx ZSTD_CStream;
import star.zstd_compression;
import star.net_compatibility;
import star.worker_pool;

import star.sector_array_2d;
import star.perlin;
import star.btree;
import star.lru_cache;
import star.btree_database;
import star.image;


// Match client include order for SIMD intrinsics used by xxhash.
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
import star.widget_parsing;
import star.gui_reader;
import star.widget_lua_bindings;
import star.lua_game_converters;
import star.inventory_types;
import star.item_descriptor;
import star.damage_types;
import star.world_geometry;
import star.status_types;
import star.damage;
import star.light_source;
import star.entity;
import star.tile_damage;
import star.interaction_types;
import star.celestial_coordinate;
import star.quest_descriptor;
import star.interactive_entity;
import star.collision_block;
import star.tile_entity;
import star.container_entity;
import star.container_interactor;
import star.game_timers;
import star.inventory;
import star.animation;
import star.interface_cursor;
import star.pane_manager;
import star.registered_pane_manager;
import star.main_interface_types;
import star.uuid;
import star.warping;
import star.main_interface;


import star.pane;
import star.item;
import star.lua_components;


import star.base_script_pane;
import star.chat_types;


import star.host_address;
import star.sky_types;
import star.particle;
import star.weather_types;
import star.force_regions;
import star.world_parameters;
import star.celestial_parameters;
import star.ai_types;
import star.socket;
import star.tcp;
import star.liquid_types;
import star.tile_sector_array;
import star.world_layout;
import star.collision_generator;
import star.world_tiles;
import star.celestial_types;
import star.tile_modification;
import star.wiring;
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
import star.net_packet_socket;
import star.universe_connection;
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
import star.world_client;
import star.world_client_thread;
import star.universe;
// TODO: make this more thread safe
import star.universe_client;
import star.lua_root;
import star.cinematic;
import star.portrait_entity;
import star.client_command_processor;

module star.interface_lua_bindings;
import star.client_context;

namespace Star {

std::expected<LuaCallbacks, StarException> LuaBindings::makeInterfaceCallbacks(MainInterface& mainInterface) {
  LuaCallbacks callbacks;
  auto lifetime = mainInterface.callbackStopToken();
  if (auto r = callbacks.register_callback("hudVisible", [&mainInterface, lifetime]() -> std::expected<bool, StarException> {
    if (lifetime.stop_requested()) return std::unexpected(GuiException("Interface is detached"));
    return mainInterface.hudVisible();
  }); !r)
    return std::unexpected(std::move(r.error()));
  if (auto r = callbacks.register_callback("setHudVisible", [&mainInterface, lifetime](bool visible) -> std::expected<void, StarException> {
    if (lifetime.stop_requested()) return std::unexpected(GuiException("Interface is detached"));
    mainInterface.setHudVisible(visible);
    return {};
  }); !r)
    return std::unexpected(std::move(r.error()));
  if (auto r = callbacks.register_callback("returnToTitle", [&mainInterface, lifetime]() -> std::expected<void, StarException> {
    if (lifetime.stop_requested()) return std::unexpected(GuiException("Interface is detached"));
    mainInterface.returnToTitle();
    return {};
  }); !r)
    return std::unexpected(std::move(r.error()));
  if (auto r = callbacks.register_callback("itemTooltip", [&mainInterface, lifetime](LuaUserData const& handle) -> std::expected<LuaUserData, StarException> {
    if (lifetime.stop_requested()) return std::unexpected(GuiException("Interface is detached"));
    auto item = handle.get<std::shared_ptr<Item>>();
    if (!item) return std::unexpected(std::move(item.error()));
    auto tooltip = mainInterface.itemTooltip(item->get());
    if (!tooltip) return std::unexpected(std::move(tooltip.error()));
    return handle.engine().create_userdata(std::move(*tooltip));
  }); !r)
    return std::unexpected(std::move(r.error()));
  if (auto r = callbacks.register_callback("playCinematic", [&mainInterface, lifetime](Json const& config) -> std::expected<void, StarException> {
    if (lifetime.stop_requested()) return std::unexpected(GuiException("Interface is detached"));
    return mainInterface.playCinematic(config);
  }); !r)
    return std::unexpected(std::move(r.error()));
  if (auto r = callbacks.register_callback(
        "bindCanvas",
        [&mainInterface, lifetime](LuaEngine& engine, String const& name, std::optional<bool> ignoreScale)
          -> std::expected<std::optional<LuaCanvas>, StarException> {
          if (lifetime.stop_requested())
            return std::unexpected(GuiException("Interface is detached"));
          auto canvas = mainInterface.fetchCanvas(name, ignoreScale.value_or(false));
          if (!canvas)
            return std::unexpected(std::move(canvas.error()));
          if (*canvas)
            return std::optional<LuaCanvas>(LuaCanvas{.canvas = std::move(*canvas),
                                                      .lifetime = engine.promise_lifetime(),
                                                      .hostLifetime = lifetime,
                                                      .owner = {}});
          return std::optional<LuaCanvas>{};
        });
      !r)
    return std::unexpected(std::move(r.error()));
  if (auto r = callbacks.register_callback("callPane", [&mainInterface, lifetime](String const& id, String const& method, std::optional<JsonArray> arguments) -> std::expected<std::optional<Json>, StarException> {
    if (lifetime.stop_requested()) return std::unexpected(GuiException("Interface is detached"));
    return mainInterface.callPane(id, method, arguments.value_or(JsonArray{}));
  }); !r) return std::unexpected(std::move(r.error()));
  if (auto r = callbacks.register_callback("sendPaneEvent", [&mainInterface, lifetime](String const& id, String const& event, Json payload) -> std::expected<void, StarException> {
    if (lifetime.stop_requested()) return std::unexpected(GuiException("Interface is detached"));
    return mainInterface.sendPaneEvent(id, event, std::move(payload));
  }); !r) return std::unexpected(std::move(r.error()));
  if (auto r = callbacks.register_callback("createPane", [&mainInterface, lifetime](String const& id, Json config, String const& layer) -> std::expected<void, StarException> {
    if (lifetime.stop_requested()) return std::unexpected(GuiException("Interface is detached"));
    return mainInterface.createPane(id, std::move(config), layer);
  }); !r) return std::unexpected(std::move(r.error()));
  if (auto r = callbacks.register_callback("destroyPane", [&mainInterface, lifetime](String const& id) -> std::expected<void, StarException> {
    if (lifetime.stop_requested()) return std::unexpected(GuiException("Interface is detached"));
    return mainInterface.destroyPane(id);
  }); !r) return std::unexpected(std::move(r.error()));
  if (auto r = callbacks.register_callback("session", [&mainInterface, lifetime]() -> std::expected<Json, StarException> {
    if (lifetime.stop_requested()) return std::unexpected(GuiException("Interface is detached"));
    return mainInterface.uiSession();
  }); !r) return std::unexpected(std::move(r.error()));
  if (auto r = callbacks.register_callback("paneIdAvailable", [lifetime](String const& id) -> std::expected<bool, StarException> {
    if (lifetime.stop_requested()) return std::unexpected(GuiException("Interface is detached"));
    return MainInterface::paneIdAvailable(id);
  }); !r) return std::unexpected(std::move(r.error()));
  if (auto r = callbacks.register_callback("paneInputTargets", [&mainInterface, lifetime]() -> std::expected<JsonArray, StarException> {
    if (lifetime.stop_requested()) return std::unexpected(GuiException("Interface is detached"));
    return mainInterface.paneInputTargets();
  }); !r) return std::unexpected(std::move(r.error()));
  if (auto r = callbacks.register_callback("paneLayerOccupied", [&mainInterface, lifetime](String const& layer) -> std::expected<bool, StarException> {
    if (lifetime.stop_requested()) return std::unexpected(GuiException("Interface is detached"));
    return mainInterface.paneLayerOccupied(layer);
  }); !r) return std::unexpected(std::move(r.error()));
  if (auto r = callbacks.register_callback("paneDisplayed", [&mainInterface, lifetime](String const& name) -> std::expected<bool, StarException> {
    if (lifetime.stop_requested()) return std::unexpected(GuiException("Interface is detached"));
    auto pane = mainInterface.findRegisteredPane(name);
    if (!pane) return std::unexpected(std::move(pane.error()));
    return *pane && (*pane)->isDisplayed();
  }); !r) return std::unexpected(std::move(r.error()));
  if (auto r = callbacks.register_callback("paneVisible", [&mainInterface, lifetime](String const& name) -> std::expected<bool, StarException> {
    if (lifetime.stop_requested()) return std::unexpected(GuiException("Interface is detached"));
    auto pane = mainInterface.findRegisteredPane(name);
    if (!pane) return std::unexpected(std::move(pane.error()));
    return *pane && (*pane)->isDisplayed() && (*pane)->visibility();
  }); !r) return std::unexpected(std::move(r.error()));
  if (auto r = callbacks.register_callback("cinematicActive", [&mainInterface, lifetime]() -> std::expected<bool, StarException> {
    if (lifetime.stop_requested()) return std::unexpected(GuiException("Interface is detached"));
    return mainInterface.cinematicActive();
  }); !r) return std::unexpected(std::move(r.error()));
  if (auto r = callbacks.register_callback("cursorWorldPosition", [&mainInterface, lifetime]() -> std::expected<Vec2F, StarException> {
    if (lifetime.stop_requested()) return std::unexpected(GuiException("Interface is detached"));
    return mainInterface.cursorWorldPosition();
  }); !r) return std::unexpected(std::move(r.error()));
  if (auto r = callbacks.register_callback("travelState", [&mainInterface, lifetime]() -> std::expected<Json, StarException> {
    if (lifetime.stop_requested()) return std::unexpected(GuiException("Interface is detached"));
    return mainInterface.travelState();
  }); !r) return std::unexpected(std::move(r.error()));
  if (auto r = callbacks.register_callback("travel", [&mainInterface, lifetime](String const& operation, std::optional<String> member) -> std::expected<bool, StarException> {
    if (lifetime.stop_requested()) return std::unexpected(GuiException("Interface is detached"));
    std::optional<Uuid> target;
    if (member) {
      auto parsed = Uuid::from_hex((*member).utf8());
      if (!parsed) return std::unexpected(std::move(parsed.error()));
      target = *parsed;
    }
    return mainInterface.travel(operation, target);
  }); !r) return std::unexpected(std::move(r.error()));
  if (auto r = callbacks.register_callback("portraitEntity", [&mainInterface, lifetime](LuaEngine& engine, std::optional<EntityId> id) -> std::expected<std::optional<LuaUserData>, StarException> {
    if (lifetime.stop_requested()) return std::unexpected(GuiException("Interface is detached"));
    auto entity = mainInterface.portraitEntity(id);
    if (!entity) return std::optional<LuaUserData>{};
    auto handle = engine.create_userdata(std::move(entity));
    if (!handle) return std::unexpected(std::move(handle.error()));
    return std::optional<LuaUserData>(std::move(*handle));
  }); !r) return std::unexpected(std::move(r.error()));
  if (auto r = callbacks.register_callback("cursorView", [&mainInterface, lifetime]() -> std::expected<Json, StarException> {
    if (lifetime.stop_requested()) return std::unexpected(GuiException("Interface is detached"));
    return mainInterface.cursorView();
  }); !r) return std::unexpected(std::move(r.error()));
  if (auto r = callbacks.register_callback("inventoryChanged", [&mainInterface, lifetime]() -> std::expected<bool, StarException> {
    if (lifetime.stop_requested()) return std::unexpected(GuiException("Interface is detached"));
    return mainInterface.inventoryChanged();
  }); !r) return std::unexpected(std::move(r.error()));
  if (auto r = callbacks.register_callback("togglePane", [&mainInterface, lifetime](String const& id) -> std::expected<void, StarException> {
    if (lifetime.stop_requested()) return std::unexpected(GuiException("Interface is detached"));
    return mainInterface.togglePane(id);
  }); !r) return std::unexpected(std::move(r.error()));
  if (auto r = callbacks.register_callback("bindRegisteredPane", [&mainInterface, lifetime](String const& name) -> std::expected<std::optional<LuaCallbacks>, StarException> {
    if (lifetime.stop_requested()) return std::unexpected(GuiException("Interface is detached"));
    auto pane = mainInterface.findRegisteredPane(name);
    if (!pane) return std::unexpected(std::move(pane.error()));
    if (!*pane) return std::optional<LuaCallbacks>{};
    auto bound = (*pane)->makePaneCallbacks();
    if (!bound) return std::unexpected(std::move(bound.error()));
    return std::optional<LuaCallbacks>(std::move(*bound));
  }); !r) return std::unexpected(std::move(r.error()));
  if (auto r = callbacks.register_callback("displayRegisteredPane", [&mainInterface, lifetime](String const& name) -> std::expected<void, StarException> {
    if (lifetime.stop_requested()) return std::unexpected(GuiException("Interface is detached"));
    return mainInterface.displayRegisteredPane(name);
  }); !r) return std::unexpected(std::move(r.error()));
  if (auto r = callbacks.register_callback("scale", [&mainInterface, lifetime]() -> std::expected<float, StarException> {
    if (lifetime.stop_requested()) return std::unexpected(GuiException("Interface is detached"));
    return mainInterface.context().interfaceScale();
  }); !r)
    return std::unexpected(std::move(r.error()));
  if (auto r = callbacks.register_callback("queueMessage", [&mainInterface, lifetime](String const& message, std::optional<float> cooldown, std::optional<float> spring) -> std::expected<void, StarException> {
    if (lifetime.stop_requested()) return std::unexpected(GuiException("Interface is detached"));
    return mainInterface.queueMessage(message, cooldown, spring.value_or(0));
  }); !r) return std::unexpected(std::move(r.error()));
  return callbacks;
}

std::expected<LuaCallbacks, StarException> LuaBindings::make_chat_callbacks(MainInterface& main_interface, UniverseClient& client) {
  LuaCallbacks callbacks;
  auto lifetime = main_interface.callbackStopToken();
  if (auto r = callbacks.register_callback("send", [&client, lifetime](String const& message, std::optional<String> mode_name, std::optional<bool> speak, std::optional<JsonObject> data) -> std::expected<void, StarException> {
    if (lifetime.stop_requested()) return std::unexpected(GuiException("Interface is detached"));
    ChatSendMode mode = ChatSendMode::Broadcast;
    if (mode_name) {
      auto parsed = enum_value<chat_send_mode_names>(*mode_name);
      if (!parsed) return std::unexpected(std::move(parsed.error()));
      mode = *parsed;
    }
    return client.send_chat(message, mode, speak, std::move(data));
  }); !r) return std::unexpected(std::move(r.error()));
  if (auto r = callbacks.register_callback("parseArguments", [](String const& args) -> std::expected<LuaVariadic<Json>, StarException> {
    auto parsed = Json::parse_sequence(args);
    if (!parsed) return std::unexpected(std::move(parsed.error()));
    auto values = parsed->to_array();
    if (!values) return std::unexpected(std::move(values.error()));
    return LuaVariadic<Json>(std::move(*values));
  }); !r) return std::unexpected(std::move(r.error()));
  if (auto r = callbacks.register_callback("command", [&main_interface, lifetime](String const& command) -> std::expected<StringList, StarException> {
    if (lifetime.stop_requested()) return std::unexpected(GuiException("Interface is detached"));
    return main_interface.executeClientCommand(command);
  }); !r) return std::unexpected(std::move(r.error()));
  if (auto r = callbacks.register_callback("addMessage", [&client, &main_interface, lifetime](String const& text, std::optional<Json> config) -> std::expected<void, StarException> {
    if (lifetime.stop_requested()) return std::unexpected(GuiException("Interface is detached"));
    ChatReceivedMessage message{{MessageContext::Mode::CommandResult}, client.client_context()->connection_id(), "", text};
    bool show_pane = true;
    if (config) {
      auto mode = config->opt_string("mode");
      if (!mode) return std::unexpected(std::move(mode.error()));
      if (*mode) {
        auto parsed = enum_value<message_context_mode_names>(**mode);
        if (!parsed) return std::unexpected(std::move(parsed.error()));
        message.context.mode = *parsed;
      }
      auto channel = config->opt_string("channelName");
      if (!channel) return std::unexpected(std::move(channel.error()));
      if (*channel) message.context.channel_name = std::move(**channel);
      auto portrait = config->opt_string("portrait");
      if (!portrait) return std::unexpected(std::move(portrait.error()));
      if (*portrait) message.portrait = std::move(**portrait);
      auto nickname = config->opt_string("fromNick");
      if (!nickname) return std::unexpected(std::move(nickname.error()));
      if (*nickname) message.from_nick = std::move(**nickname);
      auto from_connection = chat_connection_id(*config, message.from_connection);
      if (!from_connection) return std::unexpected(std::move(from_connection.error()));
      message.from_connection = *from_connection;
      auto show = config->get_bool("showPane", true);
      if (!show) return std::unexpected(std::move(show.error()));
      show_pane = *show;
    }
    return main_interface.add_chat_messages({std::move(message)}, show_pane);
  }); !r) return std::unexpected(std::move(r.error()));
  if (auto r = callbacks.register_callback("input", [&main_interface, lifetime]() -> std::expected<String, StarException> {
    if (lifetime.stop_requested()) return std::unexpected(GuiException("Interface is detached"));
    auto value = main_interface.callUi("chat", "currentChat", {});
    if (!value) return std::unexpected(std::move(value.error()));
    if (!*value) return std::unexpected(GuiException("Chat pane does not export currentChat"));
    return (**value).to_string();
  }); !r) return std::unexpected(std::move(r.error()));
  if (auto r = callbacks.register_callback("setInput", [&main_interface, lifetime](String const& text, std::optional<bool> move_cursor) -> std::expected<bool, StarException> {
    if (lifetime.stop_requested()) return std::unexpected(GuiException("Interface is detached"));
    auto value = main_interface.callUi("chat", "setCurrentChat", {text, move_cursor.value_or(false)});
    if (!value) return std::unexpected(std::move(value.error()));
    if (!*value) return std::unexpected(GuiException("Chat pane does not export setCurrentChat"));
    return (**value).to_bool();
  }); !r) return std::unexpected(std::move(r.error()));
  if (auto r = callbacks.register_callback("clear", [&main_interface, lifetime](std::optional<std::size_t> count) -> std::expected<void, StarException> {
    if (lifetime.stop_requested()) return std::unexpected(GuiException("Interface is detached"));
    auto value = main_interface.callUi("chat", "clear", {count ? Json(*count) : Json()});
    if (!value) return std::unexpected(std::move(value.error()));
    return {};
  }); !r) return std::unexpected(std::move(r.error()));
  return callbacks;
}

}

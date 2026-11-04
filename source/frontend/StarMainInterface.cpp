import star.json_extra;
import std;
import star.team_client;
import star.optional;
import star.json;
import star.xx_hash;
import star.algorithm;
import star.id_map;
import star.list;
import star.rect;
import star.image_processing;
import star.hash;
import star.data_stream;
import star.string_view;
import star.thread;
import star.directives;
import star.asset_path;
import star.ordered_map;
import star.either;
import star.vector;
import star.poly;
import star.game_types;
import star.color;
import star.bi_map;
import star.random;
import star.weighted_pool;
import star.logging;
import star.output_proxy;
import star.lexical_cast;
import star.casting;
import star.input_event;
import star.font;
import star.input;
import star.string;
import star.text;
import star.listener;
import star.set;
import star.audio;
import star.map;
import star.interpolation;
import star.image;
import star.math_common;
import star.lru_cache;
import star.time;
import star.ttl_cache;
import star.perlin;
import star.random_point;
import star.rpc_promise;
import star.byte_array;
import star.encode;
import star.bytes;
import star.format;
import star.strong_typedef;
import star.ordered_set;
import star.io_device;
import star.ref_ptr;
import star.data_stream_devices;

typedef struct ZSTD_CCtx_s ZSTD_CCtx;
typedef struct ZSTD_DCtx_s ZSTD_DCtx;
typedef ZSTD_DCtx ZSTD_DStream;
typedef ZSTD_CCtx ZSTD_CStream;
import star.zstd_compression;
import star.net_compatibility;
import star.worker_pool;

import star.sector_array_2d;
import star.btree;
import star.btree_database;
import star.a_star;
import star.periodic_function;
import star.lua;

import star.lua_root;
import star.lua_components;
import star.lua_actor_movement_component;
import star.lua_animation_component;
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
import star.root;
import star.host_address;
import star.socket;
import star.tcp;
import star.tile_sector_array;
import star.world_layout;
import star.tile_modification;
import star.inspectable_entity;
import star.plant;
import star.biome_placement;
import star.versioning_database;
import star.world_storage;
import star.player_types;
import star.system_world;
import star.damage_manager;
import star.net_packets;
import star.net_packet_socket;
import star.universe_connection;
import star.world_structure;
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
import star.celestial_coordinate;
import star.sky_types;
import star.animation;
import star.particle;
import star.weather_types;
import star.force_regions;
import star.world_parameters;
import star.celestial_parameters;
import star.celestial_types;
import star.damage_types;
import star.status_types;
import star.damage;
import star.light_source;
import star.entity;
import star.tile_damage;
import star.interaction_types;
import star.interactive_entity;
import star.collision_block;
import star.tile_entity;
import star.container_entity;
import star.inventory;
import star.interface_cursor;
import star.main_interface;

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



import star.container_interactor;
import star.widget_parsing;
import star.gui_reader;


import star.container_interface;

import star.liquid_types;
import star.collision_generator;
import star.world_tiles;
import star.entity_rendering_types;
import star.sky_parameters;
import star.sky_render_data;
import star.plant_database;
import star.parallax;
import star.cellular_light_array;
import star.cellular_lighting;
import star.world_render_data;
import star.material_render_profile;
import star.tile_drawer;
import star.tile_painter;
import star.environment_painter;
import star.world_camera;
import star.world_painter;
import star.item_descriptor;
import star.item_recipe;


import star.crafting_interface;



import star.merchant_interface;

import star.uuid;





import star.songbook;









import star.confirmation_dialog;



import star.join_request_dialog;
import star.game_timers;
import star.pane_manager;
import star.shell_parser;
import star.cinematic;
import star.client_command_processor;




import star.base_script_pane;
import star.chat_types;



import star.configuration;

import star.registered_pane_manager;


import star.main_interface_types;


import star.options_menu;

import star.inventory_types;






import star.world_geometry;
import star.wiring;


import star.wire_interface;




import star.progress_widget;
import star.label_widget;








import star.canvas_widget;
import star.item_slot_widget;
import star.button_group;
import star.button_widget;
import star.animated_part_set;
import star.networked_animator;
import star.humanoid;
import star.physics_entity;
import star.anchorable_entity;
import star.mobile_entity;
import star.actor_entity;
import star.lounging_entities;
import star.chatty_entity;
import star.emote_entity;
import star.portrait_entity;
import star.nametag_entity;
import star.movement_controller;
import star.platformer_astar_types;
import star.actor_movement_controller;
import star.player;
import star.player_log;
import star.aggressive_entity;
import star.scripted_entity;
import star.monster_database;
import star.effect_emitter;
import star.monster;
import star.item_drop;
import star.item;
import star.item_tooltip;

import star.quest_descriptor;
import star.ai_types;
import star.warping;



import star.tech_database;



import star.ai_interface;
import star.fireable_item;
import star.tool_user_entity;


import star.player_universe_map;




import star.teleport_dialog;




import star.chat_bubble_separation;



import star.nameplate_painter;




import star.quest_indicator_painter;






import star.script_pane;
import star.warp_target_entity;
import star.world_template;


import star.radio_message_database;










import star.chat_action;






import star.chat_bubble_manager;
import star.npc_database;
import star.npc;



import star.char_selection;

import star.inspection_tool;
import star.active_item;
import star.http_trust_dialog;
import star.image_metadata_database;
import star.celestial_database;


import star.player_inventory;
import star.client_context;
import star.player_storage;

import star.quest_manager;
import star.interface_lua_bindings;
import star.world_lua_bindings;
import star.player_lua_bindings;
import star.status_controller_lua_bindings;
import star.celestial_lua_bindings;
import star.team_client_lua_bindings;
import star.songbook_lua_bindings;

namespace Star {

namespace {

template <typename T>
std::expected<void, StarException> installPane(MainInterfacePaneManager& paneManager, MainInterfacePanes id,
                                               PaneLayer layer, std::expected<std::shared_ptr<T>, StarException> pane,
                                               std::shared_ptr<T>& destination,
                                               PaneManager::DismissCallback dismissed = {}) {
  if (!pane)
    return std::unexpected(std::move(pane.error()));
  if (auto registered = paneManager.registerPane(id, layer, *pane, std::move(dismissed)); !registered)
    return std::unexpected(std::move(registered.error()));
  destination = std::move(*pane);
  return {};
}
template <typename ScriptHost>
std::expected<void, StarException> bindUiCapabilities(ScriptHost& host, std::shared_ptr<LuaRoot> const& root,
    Map<String, LuaCallbacks> const& capabilities) {
  for (auto const& entry : capabilities) {
    if (auto removed = host.removeCallbacks(entry.first); !removed)
      return std::unexpected(std::move(removed.error()));
    if (auto added = host.addCallbacks(entry.first, entry.second); !added)
      return std::unexpected(std::move(added.error()));
  }
  return host.setLuaRoot(root);
}


}// namespace


MainInterface::MainInterface(GuiContext& guiContext, std::shared_ptr<UniverseClient> client, WorldPainter& painter, std::shared_ptr<Cinematic> cinematicOverlay)
  : m_guiContext(guiContext)
  , m_client(std::move(client))
  , m_worldPainter(painter)
  , m_cinematicOverlay(std::move(cinematicOverlay))
  , m_paneManager(guiContext)
  , m_containerInteractor(std::make_shared<ContainerInteractor>()) {}

std::expected<std::unique_ptr<MainInterface>, StarException> MainInterface::create(GuiContext& guiContext, std::shared_ptr<UniverseClient> client, std::shared_ptr<WorldPainter> painter, std::shared_ptr<Cinematic> cinematicOverlay) {
  auto interface = std::unique_ptr<MainInterface>(new MainInterface(guiContext, std::move(client), *painter, std::move(cinematicOverlay)));
  auto initialized = interface->initialize(std::move(painter));
  if (!initialized)
    return std::unexpected(std::move(initialized.error()));
  return interface;
}

std::expected<void, StarException> MainInterface::initialize(std::shared_ptr<WorldPainter> painter) {
  auto config = MainInterfaceConfig::loadFromAssets(context());
  if (!config)
    return std::unexpected(std::move(config.error()));
  m_config = std::move(*config);
  auto cursor = InterfaceCursor::create(context());
  if (!cursor)
    return std::unexpected(std::move(cursor.error()));
  m_cursor = std::move(*cursor);
  if (auto initialized = m_paneManager.initialize(); !initialized)
    return std::unexpected(std::move(initialized.error()));

  GuiReader itemSlotReader(context());
  auto slot = itemSlotReader.makeSingle("cursorItemSlot", m_config->cursorItemSlot);
  if (!slot)
    return std::unexpected(std::move(slot.error()));
  auto cursorItem = convert<ItemSlotWidget>(*slot);
  if (!cursorItem)
    return std::unexpected(std::move(cursorItem.error()));
  m_cursorItem = std::move(*cursorItem);

  m_debugSpatialClearTimer = GameTimer(m_config->debugSpatialClearTime);
  m_debugMapClearTimer = GameTimer(m_config->debugMapClearTime);

  if (auto initialized = initMainBarPanes(); !initialized)
    return std::unexpected(std::move(initialized.error()));
  if (auto initialized = initScriptAndHudPanes(std::move(painter)); !initialized)
    return std::unexpected(std::move(initialized.error()));
  if (auto initialized = initCharacterSwap(); !initialized)
    return std::unexpected(std::move(initialized.error()));
  return initWorldOverlays();
}

std::expected<void, StarException> MainInterface::handleInventoryDismissed() {
  if (auto player = m_client->mainPlayer()) {
    if (auto cleared = player->clearSwap(); !cleared)
      return std::unexpected(std::move(cleared.error()));
  }
  if (m_containerPane) {
    m_containerPane->dismiss();
    m_containerPane.reset();
    if (auto closed = m_containerInteractor->closeContainer(); !closed)
      return std::unexpected(std::move(closed.error()));
  }
  for (EntityId id : m_interactionScriptPanes.keyView()) {
    auto pane = as<ScriptPane>(m_interactionScriptPanes[id]);
    if (m_paneManager.isDisplayed(pane)) {
      auto withInventory = pane->openWithInventory();
      if (!withInventory)
        return std::unexpected(std::move(withInventory.error()));
      if (*withInventory)
        pane->dismiss();
    }
  }
  return {};
}

std::expected<void, StarException> MainInterface::initMainBarPanes() {
  auto inventoryDismissed = [this](std::shared_ptr<Pane> const&) { return handleInventoryDismissed(); };
  if (auto result = installPane(m_paneManager, MainInterfacePanes::Inventory, PaneLayer::Window,
                                InventoryPane::create(context(), *this, m_client->mainPlayer(), m_containerInteractor),
                                m_inventoryWindow, inventoryDismissed);
      !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = installPane(m_paneManager, MainInterfacePanes::CraftingPlain, PaneLayer::Window,
                                CraftingPane::create(context(), m_client->worldClient(), m_client->mainPlayer(),
                                                     JsonObject{{"filter", JsonArray{"plain"}}},
                                                     m_client->mainPlayer()->entityId()),
                                m_plainCraftingWindow);
      !result)
    return std::unexpected(std::move(result.error()));
  auto ai = AiInterface::create(context(), m_client, m_cinematicOverlay);
  if (!ai)
    return std::unexpected(std::move(ai.error()));
  if (auto result = m_paneManager.registerPane(MainInterfacePanes::Ai, PaneLayer::Window, *ai); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = installPane(m_paneManager, MainInterfacePanes::Options, PaneLayer::ModalWindow,
                                OptionsMenu::create(context(), m_paneManager, m_client), m_optionsMenu);
      !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = installPane(m_paneManager, MainInterfacePanes::Confirmation, PaneLayer::ModalWindow,
                                ConfirmationDialog::create(context()), m_confirmationDialog);
      !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = initHttpTrustDialog(); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = installPane(m_paneManager, MainInterfacePanes::JoinRequest, PaneLayer::ModalWindow,
                                JoinRequestDialog::create(context()), m_joinRequestDialog);
      !result)
    return std::unexpected(std::move(result.error()));
  return {};
}

std::expected<void, StarException> MainInterface::initScriptAndHudPanes(std::shared_ptr<WorldPainter> painter) {
  auto assets = context().root().assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  auto mmUpgrade = (*assets)->json("/interface.config:mainBar.mmUpgrade");
  if (!mmUpgrade)
    return std::unexpected(std::move(mmUpgrade.error()));
  auto mmUpgradeConfig = mmUpgrade->getString("scriptPane", "/interface/scripted/mmupgrade/mmupgradegui.config");
  if (!mmUpgradeConfig)
    return std::unexpected(std::move(mmUpgradeConfig.error()));
  if (auto result = installPane(m_paneManager, MainInterfacePanes::MmUpgrade, PaneLayer::Window,
                                ScriptPane::create(context(), m_client, *mmUpgradeConfig), m_mmUpgrade);
      !result)
    return std::unexpected(std::move(result.error()));
  auto collections = (*assets)->json("/interface.config:mainBar.collections");
  if (!collections)
    return std::unexpected(std::move(collections.error()));
  auto collectionConfig = collections->getString("scriptPane", "/interface/scripted/collections/collectionsgui.config");
  if (!collectionConfig)
    return std::unexpected(std::move(collectionConfig.error()));
  if (auto result = installPane(m_paneManager, MainInterfacePanes::Collections, PaneLayer::Window,
                                ScriptPane::create(context(), m_client, *collectionConfig), m_collections);
      !result)
    return std::unexpected(std::move(result.error()));
  m_clientCommandProcessor = std::make_shared<ClientCommandProcessor>(context(), m_client, m_cinematicOverlay, m_paneManager, m_config->macroCommands);
  if (auto result = initUiController(); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result =
        installPane(m_paneManager, MainInterfacePanes::WireInterface, PaneLayer::World,
                    WirePane::create(context(), m_client->worldClient(), m_client->mainPlayer(), std::move(painter)),
                    m_wireInterface);
      !result)
    return std::unexpected(std::move(result.error()));
  m_client->mainPlayer()->setWireConnector(std::ref(*m_wireInterface));

  return {};
}

std::expected<void, StarException> MainInterface::initCharacterSwap() {
  auto selection = CharSelectionPane::create(context(), m_client->playerStorage(), []() -> std::expected<void, StarException> { return {}; },
    [this, lifetime = callbackStopToken()](std::shared_ptr<Player> mainPlayer) -> std::expected<void, StarException> {
      if (lifetime.stop_requested())
        return std::unexpected(GuiException("Main interface is detached"));
      auto playerUuid = mainPlayer->uuid();
      if (!playerUuid)
        return std::unexpected(std::move(playerUuid.error()));
      if (auto result = m_client->switchPlayer(*playerUuid); !result)
        return std::unexpected(std::move(result.error()));
      auto configuration = context().root().configuration();
      if (!configuration)
        return std::unexpected(std::move(configuration.error()));
      auto moveToFrontConfig = (*configuration)->get("characterSwapMovesToFront", false);
      if (!moveToFrontConfig)
        return std::unexpected(std::move(moveToFrontConfig.error()));
      auto moveToFront = moveToFrontConfig->toBool();
      if (!moveToFront)
        return std::unexpected(std::move(moveToFront.error()));
      if (*moveToFront) {
        if (auto result = m_client->playerStorage()->moveToFront(*playerUuid); !result)
          return std::unexpected(std::move(result.error()));
      }
      auto dismissAfterSwapConfig = (*configuration)->get("characterSwapDismisses", false);
      if (!dismissAfterSwapConfig)
        return std::unexpected(std::move(dismissAfterSwapConfig.error()));
      auto dismissAfterSwap = dismissAfterSwapConfig->toBool();
      if (!dismissAfterSwap)
        return std::unexpected(std::move(dismissAfterSwap.error()));
      if (*dismissAfterSwap)
        return m_paneManager.dismissRegisteredPane(MainInterfacePanes::CharacterSwap).transform([](bool) {});
      return {};
    }, [](Uuid) -> std::expected<void, StarException> { return {}; });
  if (!selection)
    return std::unexpected(std::move(selection.error()));
  auto charSelectionMenu = std::move(*selection);
  if (auto result = charSelectionMenu->setReadOnly(true); !result)
    return std::unexpected(std::move(result.error()));
  charSelectionMenu->setAnchor(PaneAnchor::Center);
  charSelectionMenu->unlockPosition();
  auto backgrounds = charSelectionMenu->getBG();
  backgrounds.header = std::move(backgrounds.body);
  if (auto result = charSelectionMenu->setBG(backgrounds); !result)
    return std::unexpected(std::move(result.error()));
  auto dismissLabel = charSelectionMenu->requireChild("toggleDismissLabel");
  if (!dismissLabel)
    return std::unexpected(std::move(dismissLabel.error()));
  if (auto result = (*dismissLabel)->setVisibility(true); !result)
    return std::unexpected(std::move(result.error()));
  auto toggleDismiss = charSelectionMenu->requireChild<ButtonWidget>("toggleDismissCheckbox");
  if (!toggleDismiss)
    return std::unexpected(std::move(toggleDismiss.error()));
  auto configuration = context().root().configuration();
  if (!configuration)
    return std::unexpected(std::move(configuration.error()));
  auto dismissAfterSwapConfig = (*configuration)->get("characterSwapDismisses", false);
  if (!dismissAfterSwapConfig)
    return std::unexpected(std::move(dismissAfterSwapConfig.error()));
  auto dismissAfterSwap = dismissAfterSwapConfig->toBool();
  if (!dismissAfterSwap)
    return std::unexpected(std::move(dismissAfterSwap.error()));
  (*toggleDismiss)->setChecked(*dismissAfterSwap);
  if (auto result = (*toggleDismiss)->setVisibility(true); !result)
    return std::unexpected(std::move(result.error()));
  return m_paneManager.registerPane(MainInterfacePanes::CharacterSwap, PaneLayer::Window, charSelectionMenu);
}

std::expected<void, StarException> MainInterface::initWorldOverlays() {
  auto nameplates = NameplatePainter::create(context());
  if (!nameplates)
    return std::unexpected(std::move(nameplates.error()));
  m_nameplatePainter = std::move(*nameplates);
  m_questIndicatorPainter = std::make_unique<QuestIndicatorPainter>(context(), m_client);
  auto bubbles = ChatBubbleManager::create(context());
  if (!bubbles)
    return std::unexpected(std::move(bubbles.error()));
  m_chatBubbleManager = std::move(*bubbles);
  return {};
}

std::expected<LuaCallbacks, StarException> MainInterface::makeClientCallbacks() {
  LuaCallbacks callbacks;
  auto storage = m_client->playerStorage();
  auto lifetime = callbackStopToken();
  if (auto added = callbacks.registerCallback("storage",
      [storage, lifetime](String const& key) -> std::expected<LuaCallbacks, StarException> {
        if (lifetime.stop_requested()) return std::unexpected(GuiException("Interface is detached"));
        if (key.empty() || key == "order")
          return std::unexpected(GuiException("Storage must name a non-reserved metadata namespace"));
        LuaCallbacks scoped;
        if (auto added = scoped.registerCallback("get", [storage, key, lifetime](std::optional<Json> fallback) -> std::expected<Json, StarException> {
              if (lifetime.stop_requested()) return std::unexpected(GuiException("Interface is detached"));
              auto value = storage->getMetadata(key);
              if (!value) return std::unexpected(std::move(value.error()));
              return value->isNull() ? fallback.value_or(Json()) : *value;
            }); !added)
          return std::unexpected(std::move(added.error()));
        if (auto added = scoped.registerCallback("set", [storage, key, lifetime](Json value) -> std::expected<void, StarException> {
              if (lifetime.stop_requested()) return std::unexpected(GuiException("Interface is detached"));
              return storage->setMetadata(key, std::move(value));
            }); !added)
          return std::unexpected(std::move(added.error()));
        return scoped;
      }); !added)
    return std::unexpected(std::move(added.error()));
  return callbacks;
}

Json MainInterface::uiSession() const {
  return JsonObject{{"hasPlayer", static_cast<bool>(m_uiPlayer)}, {"inWorld", m_uiInWorld},
      {"playerGeneration", m_uiPlayerGeneration}, {"worldGeneration", m_uiWorldGeneration}};
}

std::expected<std::shared_ptr<Pane>, StarException> MainInterface::itemTooltip(std::shared_ptr<Item> const& item) {
  return ItemTooltipBuilder::buildItemTooltip(context(), item, m_uiPlayer);
}

std::expected<void, StarException> MainInterface::playCinematic(Json const& config) {
  return m_cinematicOverlay->load(config);
}

bool MainInterface::cinematicActive() const {
  return !m_cinematicOverlay->completed();
}

std::expected<void, StarException> MainInterface::initUiController() {
  m_uiController = std::make_unique<LuaUpdateScript>(context().root());
  // Session capability replacement is an integration boundary, not a policy
  // installed independently on every pane. LuaBaseComponent owns script reload.
  if (auto scripts = m_uiController->setScript("/interface/opensb/ui/controller.lua"); !scripts)
    return std::unexpected(std::move(scripts.error()));
  return synchronizeUiSession();
}


std::expected<void, StarException> MainInterface::synchronizeUiSession() {
  if (!m_uiController) return {};
  auto player = m_client->mainPlayer();
  auto songbook = player ? player->songbook() : std::shared_ptr<Songbook>{};
  auto world = m_client->worldClient();
  auto team = m_client->teamClient();
  auto luaRoot = m_client->luaRoot();
  auto worldId = m_client->clientContext() ? m_client->clientContext()->playerWorldId() : WorldId{};
  bool inWorld = world && world->inWorld();
  auto generation = luaRoot->generation();
  // /swap disk-loads another character into the existing Player allocation.
  // Keep identity separately from the borrowed object, whose UUID can mutate.
  std::optional<Uuid> playerUuid;
  if (player) {
    auto uuid = player->uuid();
    if (!uuid) return std::unexpected(std::move(uuid.error()));
    playerUuid = *uuid;
  }
  bool playerChanged = player != m_uiPlayer || playerUuid != m_uiPlayerUuid;
  if (!playerChanged && songbook == m_uiSongbook && world == m_uiWorld && team == m_uiTeam && luaRoot == m_uiRoot
      && generation == m_uiRootGeneration && worldId == m_uiWorldId && inWorld == m_uiInWorld)
    return {};

  bool worldChanged = world != m_uiWorld || worldId != m_uiWorldId || inWorld != m_uiInWorld;
  Json next = JsonObject{{"hasPlayer", static_cast<bool>(player)}, {"inWorld", inWorld},
      {"playerGeneration", m_uiPlayerGeneration + (playerChanged ? 1 : 0)},
      {"worldGeneration", m_uiWorldGeneration + (worldChanged ? 1 : 0)}};
  if (m_uiController->initialized()) {
    auto suspended = m_uiController->invoke<Json>("suspend", next);
    if (!suspended) return std::unexpected(std::move(suspended.error()));
  }
  // Uninit every borrowing context while the old targets are still retained.
  // Script hooks may change the registry; retain snapshot ownership.
  for (auto const& instance : m_paneInstances.values()) {
    if (auto stopped = instance.first->stopScripts(); !stopped)
      return std::unexpected(std::move(stopped.error()));
  }
  if (auto stopped = m_uiController->uninit(); !stopped)
    return std::unexpected(std::move(stopped.error()));
  m_uiCapabilities.clear();
  m_uiPlayer = std::move(player);
  m_uiPlayerUuid = std::move(playerUuid);
  // Player::diskLoad can replace this object in-place. Keep the old service
  // alive until every borrowing context above has retired its callback scope.
  m_uiSongbook = std::move(songbook);
  m_uiWorld = std::move(world);
  m_uiTeam = std::move(team);
  m_uiRoot = std::move(luaRoot);
  m_uiWorldId = std::move(worldId);
  m_uiRootGeneration = generation;
  m_uiInWorld = inWorld;
  if (worldChanged) ++m_uiWorldGeneration;
  if (playerChanged) ++m_uiPlayerGeneration;

  // Root common services remain inherited, but borrowed session services are
  // always explicit. An unavailable target must shadow any previous session's
  // root table, not accidentally inherit callbacks into a released target.
  m_uiCapabilities.set("player", LuaCallbacks{});
  m_uiCapabilities.set("status", LuaCallbacks{});
  m_uiCapabilities.set("world", LuaCallbacks{});
  m_uiCapabilities.set("team", LuaCallbacks{});
  m_uiCapabilities.set("songbook", LuaCallbacks{});
  if (m_uiPlayer) {
    auto callbacks = LuaBindings::makePlayerCallbacks(context().root(), *m_uiPlayer);
    if (!callbacks) return std::unexpected(std::move(callbacks.error()));
    m_uiCapabilities.set("player", std::move(*callbacks));
    auto status = LuaBindings::makeStatusControllerCallbacks(m_uiPlayer->statusController());
    if (!status) return std::unexpected(std::move(status.error()));
    m_uiCapabilities.set("status", std::move(*status));
  }
  if (m_uiSongbook) {
    auto callbacks = LuaBindings::makeSongbookCallbacks(*m_uiSongbook);
    if (!callbacks) return std::unexpected(std::move(callbacks.error()));
    m_uiCapabilities.set("songbook", std::move(*callbacks));
  }
  if (m_uiInWorld) {
    auto callbacks = LuaBindings::makeWorldCallbacks(context().root(), *m_uiWorld);
    if (!callbacks) return std::unexpected(std::move(callbacks.error()));
    m_uiCapabilities.set("world", std::move(*callbacks));
  }
  if (m_uiTeam) {
    auto callbacks = LuaBindings::makeTeamClientCallbacks(*m_uiTeam);
    if (!callbacks) return std::unexpected(std::move(callbacks.error()));
    m_uiCapabilities.set("team", std::move(*callbacks));
  }
  auto celestial = LuaBindings::makeCelestialCallbacks(context().root(), *m_client);
  if (!celestial) return std::unexpected(std::move(celestial.error()));
  m_uiCapabilities.set("celestial", std::move(*celestial));
  auto chat = LuaBindings::makeChatCallbacks(*this, *m_client);
  if (!chat) return std::unexpected(std::move(chat.error()));
  m_uiCapabilities.set("chat", std::move(*chat));
  auto interfaceCallbacks = LuaBindings::makeInterfaceCallbacks(*this);
  if (!interfaceCallbacks) return std::unexpected(std::move(interfaceCallbacks.error()));
  m_uiCapabilities.set("interface", std::move(*interfaceCallbacks));
  auto client = makeClientCallbacks();
  if (!client) return std::unexpected(std::move(client.error()));
  m_uiCapabilities.set("client", std::move(*client));
  for (auto const& entry : m_paneInstances) {
    if (auto bound = bindUiCapabilities(*entry.second.first, m_uiRoot, m_uiCapabilities); !bound)
      return std::unexpected(std::move(bound.error()));
  }
  if (auto bound = bindUiCapabilities(*m_uiController, m_uiRoot, m_uiCapabilities); !bound)
    return std::unexpected(std::move(bound.error()));
  auto initialized = m_uiController->init();
  if (!initialized) return std::unexpected(std::move(initialized.error()));
  for (auto const& instance : m_paneInstances.values()) {
    if (m_paneManager.isDisplayed(instance.first)) {
      if (auto resumed = instance.first->resumeScripts(); !resumed)
        return std::unexpected(std::move(resumed.error()));
    }
  }
  return {};
}

std::expected<bool, StarException> MainInterface::paneIdAvailable(String const& id) {
  if (id.empty() || id == "$ui") return false;
  if (!MainInterfacePanesNames) return std::unexpected(MainInterfacePanesNames.error());
  return !MainInterfacePanesNames->getLeft(id).has_value();
}

std::expected<JsonArray, StarException> MainInterface::paneInputTargets() const {
  auto targets = m_paneManager.inputTargetPanes();
  if (!targets) return std::unexpected(std::move(targets.error()));
  JsonArray ids;
  for (auto const& pane : *targets) {
    for (auto const& entry : m_paneInstances) {
      if (entry.second.first == pane) {
        ids.append(entry.first);
        break;
      }
    }
  }
  return ids;
}

std::expected<bool, StarException> MainInterface::paneLayerOccupied(String const& layer) const {
  if (!PaneLayerNames) return std::unexpected(PaneLayerNames.error());
  auto parsed = PaneLayerNames->getLeft(layer);
  if (!parsed) return std::unexpected(std::move(parsed.error()));
  return static_cast<bool>(m_paneManager.topPane({parsed->get()}));
}

std::expected<void, StarException> MainInterface::createPane(String const& id, Json config, String const& layer) {
  auto available = paneIdAvailable(id);
  if (!available) return std::unexpected(std::move(available.error()));
  if (!*available || m_paneInstances.contains(id))
    return std::unexpected(GuiException(strf("Pane id '{}' is already registered", id)));
  if (!PaneLayerNames) return std::unexpected(PaneLayerNames.error());
  auto parsed = PaneLayerNames->getLeft(layer);
  if (!parsed) return std::unexpected(std::move(parsed.error()));
  auto pane = BaseScriptPane::create(context(), std::move(config));
  if (!pane) return std::unexpected(std::move(pane.error()));
  if (auto bound = bindUiCapabilities(**pane, m_uiRoot, m_uiCapabilities); !bound)
    return std::unexpected(std::move(bound.error()));
  m_paneInstances.set(id, {*pane, parsed->get()});
  return {};
}

std::expected<void, StarException> MainInterface::destroyPane(String const& id) {
  auto instance = m_paneInstances.maybeRef(id);
  if (!instance) return {};
  auto pane = instance->get().first;
  if (m_paneManager.isDisplayed(pane)) {
    if (auto dismissed = m_paneManager.dismissPane(pane); !dismissed)
      return std::unexpected(std::move(dismissed.error()));
  } else if (auto dismissed = pane->dismissed(); !dismissed) {
    return std::unexpected(std::move(dismissed.error()));
  }
  m_paneInstances.remove(id);
  return {};
}

std::expected<std::shared_ptr<Pane>, StarException> MainInterface::findRegisteredPane(String const& id) {
  if (auto instance = m_paneInstances.maybeRef(id))
    return std::shared_ptr<Pane>(instance->get().first);
  if (!MainInterfacePanesNames) return std::unexpected(MainInterfacePanesNames.error());
  auto type = MainInterfacePanesNames->getLeft(id);
  if (!type) return std::shared_ptr<Pane>{};
  auto pane = m_paneManager.maybeRegisteredPane(type->get());
  if (!pane) return std::unexpected(std::move(pane.error()));
  return *pane;
}

std::expected<void, StarException> MainInterface::displayRegisteredPane(String const& id) {
  if (auto instance = m_paneInstances.maybeRef(id)) {
    auto [pane, layer] = instance->get();
    if (m_paneManager.isDisplayed(pane)) {
      if (auto shown = pane->show(); !shown)
        return std::unexpected(std::move(shown.error()));
      return pane->resumeScripts();
    }
    return m_paneManager.displayPane(layer, pane);
  }
  if (!MainInterfacePanesNames) return std::unexpected(MainInterfacePanesNames.error());
  auto type = MainInterfacePanesNames->getLeft(id);
  if (!type) {
    if (!m_uiController) return std::unexpected(std::move(type.error()));
    auto opened = m_uiController->invoke<Json>("open", id);
    if (!opened) return std::unexpected(std::move(opened.error()));
    return {};
  }
  auto displayed = m_paneManager.displayRegisteredPane(type->get());
  if (!displayed) return std::unexpected(std::move(displayed.error()));
  return {};
}

std::expected<void, StarException> MainInterface::togglePane(String const& id) {
  if (!MainInterfacePanesNames) return std::unexpected(MainInterfacePanesNames.error());
  auto type = MainInterfacePanesNames->getLeft(id);
  if (type) {
    if (type->get() == MainInterfacePanes::CraftingPlain)
      return togglePlainCraftingWindow();
    auto toggled = m_paneManager.toggleRegisteredPane(type->get());
    if (!toggled) return std::unexpected(std::move(toggled.error()));
  } else {
    auto toggled = m_uiController->invoke<Json>("toggle", id);
    if (!toggled) return std::unexpected(std::move(toggled.error()));
  }
  return {};
}

std::expected<std::optional<Json>, StarException> MainInterface::callPane(String const& id, String const& method, JsonArray const& arguments) {
  auto pane = findRegisteredPane(id);
  if (!pane) return std::unexpected(std::move(pane.error()));
  auto scripted = as<BaseScriptPane>(*pane);
  if (!scripted)
    return std::unexpected(GuiException(strf("Pane '{}' does not expose Lua methods", id)));
  return scripted->call(method, arguments);
}

std::expected<std::optional<Json>, StarException> MainInterface::callUi(String const& id, String const& method, JsonArray const& arguments) {
  if (!m_uiController) return std::unexpected(GuiException("UI controller is not initialized"));
  return m_uiController->invoke<Json>("dispatch", id, method, arguments);
}

std::expected<void, StarException> MainInterface::sendPaneEvent(String const& id, String const& event, Json payload) {
  if (!m_uiController) return std::unexpected(GuiException("UI controller is not initialized"));
  auto queued = m_uiController->invoke<Json>("enqueue", id, event, std::move(payload));
  if (!queued) return std::unexpected(std::move(queued.error()));
  return {};
}

std::expected<void, StarException> MainInterface::addChatMessages(List<ChatReceivedMessage> const& messages, bool showPane) {
  if (messages.empty()) return {};
  JsonArray payload;
  payload.reserve(messages.size());
  for (auto const& message : messages) {
    auto logged = message.fromNick.empty()
      ? context().root().logger().info("Chat: {}", message.text)
      : context().root().logger().info("Chat: <{}> {}", message.fromNick, message.text);
    if (!logged) return std::unexpected(std::move(logged.error()));
    auto json = message.toJson();
    if (!json) return std::unexpected(std::move(json.error()));
    payload.append(std::move(*json));
  }
  return sendPaneEvent("chat", "messages", JsonObject{{"messages", std::move(payload)}, {"showPane", showPane}});
}

MainInterface::~MainInterface() {
  auto dismissed = close();
  if (!dismissed)
    EmergencyLog::error("MainInterface: failed to dismiss panes during teardown: {}", dismissed.error().what());
}

std::expected<void, StarException> MainInterface::close() {
  auto stopped = m_uiController ? m_uiController->uninit("shutdown") : std::expected<void, StarException>{};
  auto dismissed = m_paneManager.dismissAllPanes();
  (void)m_callbackStopSource.request_stop();
  if (auto player = m_client->mainPlayer())
    player->setWireConnector(std::nullopt);
  if (m_inventoryWindow)
    m_inventoryWindow->detachInterface();
  if (m_optionsMenu)
    m_optionsMenu->detachInterface();
  for (auto const& weakDialog : m_teleportDialogs)
    if (auto dialog = weakDialog.lock())
      dialog->detachInterface();
  if (m_clientCommandProcessor)
    m_clientCommandProcessor->detachInterface();
  if (!stopped) return std::unexpected(std::move(stopped.error()));
  return dismissed;
}

MainInterface::RunningState MainInterface::currentState() const {
  return m_state;
}

void MainInterface::returnToTitle() {
  m_state = ReturnToTitle;
}

MainInterfacePaneManager& MainInterface::paneManager() {
  return m_paneManager;
}

GuiContext& MainInterface::context() const {
  return m_guiContext.get();
}

std::stop_token MainInterface::callbackStopToken() const {
  return m_callbackStopSource.get_token();
}

std::expected<bool, StarException> MainInterface::pauseRequested() const {
  auto paused = m_uiController->invoke<bool>("pauseRequested");
  if (!paused)
    return std::unexpected(std::move(paused.error()));
  return paused->value_or(false);
}

std::expected<Json, StarException> MainInterface::travelState() const {
  auto up = m_client->canBeamUp();
  if (!up) return std::unexpected(std::move(up.error()));
  auto down = m_client->canBeamDown();
  if (!down) return std::unexpected(std::move(down.error()));
  auto deploy = m_client->canBeamDown(true);
  if (!deploy) return std::unexpected(std::move(deploy.error()));
  auto teamShip = m_client->canBeamToTeamShip();
  if (!teamShip) return std::unexpected(std::move(teamShip.error()));
  return JsonObject{{"beamUp", *up}, {"beamDown", *down}, {"deploy", *deploy}, {"teamShip", *teamShip}};
}

std::expected<bool, StarException> MainInterface::travel(String const& operation, std::optional<Uuid> const& member) {
  if (operation == "teamShip") {
    if (!member) return false;
    auto allowed = m_client->canBeamToTeamShip();
    if (!allowed) return std::unexpected(std::move(allowed.error()));
    if (!*allowed) return false;
    bool reachable = false;
    for (auto const& entry : m_client->teamClient()->members())
      if (entry.uuid == *member && entry.warpMode != WarpMode::None) {
        reachable = true;
        break;
      }
    if (!reachable) return false;
    auto warped = warpTo(WarpToWorld{ClientShipWorldId(*member), {}});
    if (!warped) return std::unexpected(std::move(warped.error()));
    return true;
  }
  if (operation != "beamUp" && operation != "beamDown" && operation != "deploy")
    return std::unexpected(GuiException(strf("Unknown travel operation '{}'", operation)));
  auto allowed = operation == "beamUp" ? m_client->canBeamUp() : m_client->canBeamDown(operation == "deploy");
  if (!allowed) return std::unexpected(std::move(allowed.error()));
  if (!*allowed) return false;
  auto warped = operation == "beamUp" ? warpToOwnShip() : warpToOrbitedWorld(operation == "deploy");
  if (!warped) return std::unexpected(std::move(warped.error()));
  return true;
}

std::shared_ptr<PortraitEntity> MainInterface::portraitEntity(std::optional<EntityId> id) const {
  std::shared_ptr<Entity> entity;
  if (id) entity = m_client->worldClient()->entity(*id);
  else entity = m_client->mainPlayer();
  return as<PortraitEntity>(entity);
}

Json MainInterface::cursorView() const {
  return JsonObject{{"position", jsonFromVec2F(m_cursorScreenPos)},
      {"roundedPosition", jsonFromVec2I(m_cursorScreenIPos)},
      {"size", jsonFromVec2I(m_cursor->size())}, {"scale", m_cursor->scale(interfaceScale())}};
}

bool MainInterface::inventoryChanged() const {
  return m_inventoryWindow->containsNewItems();
}

std::expected<void, StarException> MainInterface::openCraftingWindow(Json const& config, EntityId sourceEntityId) {
  if (m_craftingWindow && m_paneManager.isDisplayed(m_craftingWindow)) {
    if (auto dismissed = m_paneManager.dismissPane(m_craftingWindow); !dismissed)
      return std::unexpected(std::move(dismissed.error()));
    if (sourceEntityId != NullEntityId && m_craftingWindow->sourceEntityId() == sourceEntityId) {
      m_craftingWindow.reset();
      return {};
    }
  }
  auto window = CraftingPane::create(context(), m_client->worldClient(), m_client->mainPlayer(), config, sourceEntityId);
  if (!window)
    return std::unexpected(std::move(window.error()));
  m_craftingWindow = std::move(*window);
  auto displayed = m_paneManager.displayPane(PaneLayer::Window, m_craftingWindow, [this](std::shared_ptr<Pane> const&) -> std::expected<void, StarException> {
    if (auto player = m_client->mainPlayer())
      return player->clearSwap();
    return {};
  });
  if (!displayed)
    return std::unexpected(std::move(displayed.error()));
  return {};
}

std::expected<void, StarException> MainInterface::openMerchantWindow(Json const& config, EntityId sourceEntityId) {
  if (m_merchantWindow && m_paneManager.isDisplayed(m_merchantWindow)) {
    if (auto dismissed = m_paneManager.dismissPane(m_merchantWindow); !dismissed)
      return std::unexpected(std::move(dismissed.error()));
    if (sourceEntityId != NullEntityId && m_merchantWindow->sourceEntityId() == sourceEntityId) {
      m_merchantWindow.reset();
      return {};
    }
  }
  auto openWithInventory = config.getBool("openWithInventory", true);
  if (!openWithInventory)
    return std::unexpected(std::move(openWithInventory.error()));
  auto inventoryDisplayed = m_paneManager.registeredPaneIsDisplayed(MainInterfacePanes::Inventory);
  if (!inventoryDisplayed)
    return std::unexpected(std::move(inventoryDisplayed.error()));
  auto closeWithInventory = config.getBool("closeWithInventory", !*inventoryDisplayed);
  if (!closeWithInventory)
    return std::unexpected(std::move(closeWithInventory.error()));
  auto window = MerchantPane::create(context(), m_client->worldClient(), m_client->mainPlayer(), config, sourceEntityId);
  if (!window)
    return std::unexpected(std::move(window.error()));
  m_merchantWindow = std::move(*window);
  auto displayed = m_paneManager.displayPane(PaneLayer::Window, m_merchantWindow, [this, close = *closeWithInventory](std::shared_ptr<Pane> const&) -> std::expected<void, StarException> {
    if (close) {
      auto dismissed = m_paneManager.dismissRegisteredPane(MainInterfacePanes::Inventory);
      if (!dismissed)
        return std::unexpected(std::move(dismissed.error()));
    }
    return {};
  });
  if (!displayed)
    return std::unexpected(std::move(displayed.error()));
  if (*openWithInventory) {
    auto opened = m_paneManager.displayRegisteredPane(MainInterfacePanes::Inventory);
    if (!opened)
      return std::unexpected(std::move(opened.error()));
  }
  auto inventory = m_paneManager.registeredPane(MainInterfacePanes::Inventory);
  if (!inventory)
    return std::unexpected(std::move(inventory.error()));
  auto assets = context().root().assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  auto gapConfig = (*assets)->json("/interface.config:bringAdjacentWindowGap");
  if (!gapConfig)
    return std::unexpected(std::move(gapConfig.error()));
  auto gap = gapConfig->toFloat();
  if (!gap)
    return std::unexpected(std::move(gap.error()));
  return m_paneManager.bringPaneAdjacent(*inventory, m_merchantWindow, *gap);
}

std::expected<void, StarException> MainInterface::togglePlainCraftingWindow() {
  auto toggled = m_paneManager.toggleRegisteredPane(MainInterfacePanes::CraftingPlain);
  if (!toggled)
    return std::unexpected(std::move(toggled.error()));
  if (m_craftingWindow && m_craftingWindow->isDisplayed() && m_craftingWindow != m_plainCraftingWindow) {
    auto dismissed = m_paneManager.dismissPane(m_craftingWindow);
    if (!dismissed)
      return std::unexpected(std::move(dismissed.error()));
  }
  m_craftingWindow = m_plainCraftingWindow;
  return {};
}

bool MainInterface::windowsOpen() const {
  return (bool)m_paneManager.topPane({PaneLayer::Window});
}

std::shared_ptr<MerchantPane> MainInterface::activeMerchantPane() const {
  if (m_paneManager.isDisplayed(m_merchantWindow))
    return m_merchantWindow;
  else
    return {};
}

std::expected<bool, StarException> MainInterface::handleInputEvent(InputEvent const& event) {
  auto player = m_client->mainPlayer();
  auto& root = context().root();
  if (std::holds_alternative<MouseMoveEvent>(event)) {
    auto const& mouseMove = std::get<MouseMoveEvent>(event);
    m_cursorScreenIPos = Vec2I::round(m_cursorScreenPos = mouseMove.mousePosition);
  }
  if (auto synchronized = synchronizeUiSession(); !synchronized)
    return std::unexpected(std::move(synchronized.error()));
  auto dispatchUiInput = [&](Json const& data, std::shared_ptr<Pane> const& submitTarget) -> std::expected<bool, StarException> {
    auto before = m_uiController->invoke<Json>("flush");
    if (!before) return std::unexpected(std::move(before.error()));
    bool userSubmit = false;
    if (submitTarget) {
      auto captured = m_paneManager.keyboardCapturedPane();
      if (!captured) return std::unexpected(std::move(captured.error()));
      auto textInput = m_paneManager.keyboardCapturedForTextInput();
      if (!textInput) return std::unexpected(std::move(textInput.error()));
      userSubmit = *captured == submitTarget && *textInput;
    }
    std::expected<std::optional<bool>, StarException> consumed;
    {
      struct CommandInputScope {
        bool& enabled;
        bool previous;
        ~CommandInputScope() { enabled = previous; }
      } commandScope{m_commandInput, std::exchange(m_commandInput, userSubmit)};
      std::optional<Input::ClipboardUnlock> clipboardUnlock;
      if (userSubmit) {
        auto input = context().input();
        if (!input) return std::unexpected(std::move(input.error()));
        clipboardUnlock.emplace(input->get().unlockClipboard());
      }
      consumed = m_uiController->invoke<bool>("input", data);
    }
    if (!consumed) return std::unexpected(std::move(consumed.error()));
    auto after = m_uiController->invoke<Json>("flush");
    if (!after) return std::unexpected(std::move(after.error()));
    return consumed->value_or(false);
  };
  std::optional<Json> pendingUiInput;
  if (std::holds_alternative<KeyDownEvent>(event) || std::holds_alternative<KeyUpEvent>(event)) {
    bool down = std::holds_alternative<KeyDownEvent>(event);
    auto key = down ? std::get<KeyDownEvent>(event).key : std::get<KeyUpEvent>(event).key;
    if (!InterfaceActionNames) return std::unexpected(InterfaceActionNames.error());
    JsonArray actions;
    for (auto action : context().actions(event)) {
      auto name = InterfaceActionNames->getRight(action);
      if (!name) return std::unexpected(std::move(name.error()));
      actions.append(name->get());
    }
    auto captured = m_paneManager.keyboardCapturedPane();
    if (!captured) return std::unexpected(std::move(captured.error()));
    String capturedId;
    if (*captured) {
      for (auto const& entry : m_paneInstances) {
        if (entry.second.first == *captured) {
          capturedId = entry.first;
          break;
        }
      }
    }
    Json input = JsonObject{{"type", down ? Json("keyDown") : Json("keyUp")}, {"key", static_cast<int>(key)},
        {"keyboardCaptured", static_cast<bool>(*captured)}, {"capturedPane", capturedId}, {"actions", std::move(actions)}};
    // Keep PaneManager's native close/layer/focus priority. Focused Lua panes
    // receive semantic actions before their editor; uncaptured actions run only
    // after higher native panes have declined the event.
    bool closingWindow = down && context().actions(event).contains(InterfaceAction::GuiClose)
        && m_paneManager.topPane({PaneLayer::Window, PaneLayer::ModalWindow});
    if (down && (!*captured || closingWindow)) {
      pendingUiInput = std::move(input);
    } else {
      auto textInput = m_paneManager.keyboardCapturedForTextInput();
      if (!textInput) return std::unexpected(std::move(textInput.error()));
      bool userSubmit = down && !capturedId.empty() && *textInput
          && context().actions(event).contains(InterfaceAction::ChatSendLine);
      auto consumed = dispatchUiInput(input, userSubmit ? *captured : std::shared_ptr<Pane>{});
      if (!consumed) return std::unexpected(std::move(consumed.error()));
      if (*consumed) return true;
    }
  }

  auto paneConsumed = m_paneManager.sendInputEvent(event);
  if (!paneConsumed)
    return std::unexpected(std::move(paneConsumed.error()));
  if (*paneConsumed && !std::holds_alternative<MouseButtonUpEvent>(event) && !std::holds_alternative<KeyUpEvent>(event))
    return true;
  if (pendingUiInput) {
    auto consumed = dispatchUiInput(*pendingUiInput, {});
    if (!consumed) return std::unexpected(std::move(consumed.error()));
    if (*consumed) return true;
  }

  if (std::holds_alternative<KeyDownEvent>(event)) {
    auto capturedWidget = m_paneManager.keyboardCapturedWidget();
    if (!capturedWidget)
      return std::unexpected(std::move(capturedWidget.error()));
    if (!*capturedWidget) {
      for (auto action : context().actions(event)) {
        auto handled = [&, action]() -> std::expected<void, StarException> {
          switch (action) {
            case InterfaceAction::GuiShifting:
              context().setShiftHeld(true);
              return {};
            case InterfaceAction::InterfaceHideHud:
              m_disableHud = !m_disableHud;
              return {};
            case InterfaceAction::InterfaceRepeatCommand: {
              if (m_lastCommand.empty()) return {};
              auto input = context().input();
              if (!input) return std::unexpected(std::move(input.error()));
              auto clipboardUnlock = input->get().unlockClipboard();
              auto results = executeClientCommand(m_lastCommand);
              if (!results) return std::unexpected(std::move(results.error()));
              for (auto const& result : *results) {
                if (auto added = addChatMessages({{{MessageContext::CommandResult}, ServerConnectionId, "", result}}, true); !added)
                  return std::unexpected(std::move(added.error()));
              }
              return {};
            }
            case InterfaceAction::InterfaceToggleFullscreen:
              return m_optionsMenu->toggleFullscreen();
            case InterfaceAction::InterfaceReload:
              if (auto reloaded = root.reload(); !reloaded)
                return std::unexpected(std::move(reloaded.error()));
              return root.fullyLoad();
            case InterfaceAction::InterfaceCrafting:
              return togglePlainCraftingWindow();
            case InterfaceAction::InterfaceInventory: {
              auto toggled = m_paneManager.toggleRegisteredPane(MainInterfacePanes::Inventory);
              if (!toggled)
                return std::unexpected(std::move(toggled.error()));
              return {};
            }
            default:
              return {};
          }
        }();
        if (!handled)
          return std::unexpected(std::move(handled.error()));
      }
      }
    return false;
  } else if (std::holds_alternative<KeyUpEvent>(event)) {
    auto const& keyUp = std::get<KeyUpEvent>(event);
    if (context().actionsForKey(keyUp.key).contains(InterfaceAction::GuiShifting))
      context().setShiftHeld(false);
    return false;
  } else if (std::holds_alternative<MouseButtonDownEvent>(event)) {
    auto const& mouseDown = std::get<MouseButtonDownEvent>(event);
    auto button = mouseDown.mouseButton;
    if (button == MouseButton::Left)
      player->beginPrimaryFire();
    if (button == MouseButton::Right)
      player->beginAltFire();
    if (button == MouseButton::Middle)
      player->beginTrigger();
  } else if (std::holds_alternative<MouseButtonUpEvent>(event)) {
    auto const& mouseUp = std::get<MouseButtonUpEvent>(event);
    if (mouseUp.mouseButton == MouseButton::Left)
      player->endPrimaryFire();
    if (mouseUp.mouseButton == MouseButton::Right)
      player->endAltFire();
    if (mouseUp.mouseButton == MouseButton::Middle)
      player->endTrigger();
  }
  bool captured = false;
  for (auto& pair : m_canvases) {
    auto consumed = pair.second->sendEvent(event);
    if (!consumed)
      return std::unexpected(std::move(consumed.error()));
    captured |= *consumed;
  }
  return captured;
}

std::expected<bool, StarException> MainInterface::inputFocus() const {
  auto widget = m_paneManager.keyboardCapturedWidget();
  if (!widget)
    return std::unexpected(std::move(widget.error()));
  return static_cast<bool>(*widget);
}

std::expected<bool, StarException> MainInterface::textInputActive() const {
  return m_paneManager.keyboardCapturedForTextInput();
}

std::expected<void, StarException> MainInterface::handleInteractAction(InteractAction interactAction) {
  auto assets = context().root().assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  auto world = m_client->worldClient();

  if (interactAction.type == InteractActionType::OpenContainer) {
    if (m_containerPane && m_containerInteractor->openContainerId() == interactAction.entityId)
      return m_paneManager.dismissPane(m_containerPane);
    if (m_containerPane) {
      if (auto dismissed = m_paneManager.dismissPane(m_containerPane); !dismissed)
        return std::unexpected(std::move(dismissed.error()));
    }
    auto entity = world->get<ContainerEntity>(interactAction.entityId);
    if (!entity)
      return {};
    auto inventoryDisplayed = m_paneManager.registeredPaneIsDisplayed(MainInterfacePanes::Inventory);
    if (!inventoryDisplayed)
      return std::unexpected(std::move(inventoryDisplayed.error()));
    auto opened = m_containerInteractor->openContainer(entity);
    if (!opened)
      return std::unexpected(std::move(opened.error()));
    auto containerPane = ContainerPane::create(context(), world, m_client->mainPlayer(), m_containerInteractor);
    if (!containerPane) {
      auto closed = m_containerInteractor->closeContainer();
      if (!closed)
        return std::unexpected(containerPane.error().withRelated(closed.error(), "Closing container after pane construction failure also failed"));
      return std::unexpected(std::move(containerPane.error()));
    }
    auto inventory = m_paneManager.displayRegisteredPane(MainInterfacePanes::Inventory);
    if (!inventory) {
      auto closed = m_containerInteractor->closeContainer();
      if (!closed)
        return std::unexpected(inventory.error().withRelated(closed.error(), "Closing container after inventory display failure also failed"));
      return std::unexpected(std::move(inventory.error()));
    }
    m_containerPane = std::move(*containerPane);
    auto displayed = m_paneManager.displayPane(PaneLayer::Window, m_containerPane, [this, closeWithInventory = !*inventoryDisplayed](std::shared_ptr<Pane> const&) -> std::expected<void, StarException> {
      if (closeWithInventory) {
        auto dismissed = m_paneManager.dismissRegisteredPane(MainInterfacePanes::Inventory);
        if (!dismissed)
          return std::unexpected(std::move(dismissed.error()));
      } else {
        auto closed = m_containerInteractor->closeContainer();
        m_containerPane.reset();
        if (!closed)
          return std::unexpected(std::move(closed.error()));
      }
      return {};
    });
    if (!displayed) {
      auto closed = m_containerInteractor->closeContainer();
      m_containerPane.reset();
      if (!closed)
        return std::unexpected(displayed.error().withRelated(closed.error(), "Closing container after pane display failure also failed"));
      return std::unexpected(std::move(displayed.error()));
    }
    auto inventoryPane = m_paneManager.registeredPane(MainInterfacePanes::Inventory);
    if (!inventoryPane)
      return std::unexpected(std::move(inventoryPane.error()));
    auto gapConfig = (*assets)->json("/interface.config:bringAdjacentWindowGap");
    if (!gapConfig)
      return std::unexpected(std::move(gapConfig.error()));
    auto gap = gapConfig->toFloat();
    if (!gap)
      return std::unexpected(std::move(gap.error()));
    return m_paneManager.bringPaneAdjacent(*inventoryPane, m_containerPane, *gap);
  } else if (interactAction.type == InteractActionType::SitDown) {
    auto index = interactAction.data.toUInt();
    if (!index)
      return std::unexpected(std::move(index.error()));
    auto lounged = m_client->mainPlayer()->lounge(interactAction.entityId, *index);
    if (!lounged)
      return std::unexpected(std::move(lounged.error()));
  } else if (interactAction.type == InteractActionType::OpenCraftingInterface || interactAction.type == InteractActionType::OpenNpcCraftingInterface) {
    if (interactAction.entityId != NullEntityId && !world->entity(interactAction.entityId))
      return {};
    return openCraftingWindow(interactAction.data, interactAction.entityId);
  } else if (interactAction.type == InteractActionType::OpenSongbookInterface) {
    return sendPaneEvent("$ui", "open", JsonObject{{"id", "songbook"}});
  } else if (interactAction.type == InteractActionType::OpenMerchantInterface) {
    if (interactAction.entityId != NullEntityId && !world->entity(interactAction.entityId))
      return {};
    return openMerchantWindow(interactAction.data, interactAction.entityId);
  } else if (interactAction.type == InteractActionType::OpenAiInterface) {
    auto pane = m_paneManager.registeredPane<AiInterface>(MainInterfacePanes::Ai);
    if (!pane)
      return std::unexpected(std::move(pane.error()));
    (*pane)->setSourceEntityId(interactAction.entityId);
    auto displayed = m_paneManager.displayRegisteredPane(MainInterfacePanes::Ai);
    if (!displayed)
      return std::unexpected(std::move(displayed.error()));
  } else if (interactAction.type == InteractActionType::OpenTeleportDialog) {
    if (m_teleportDialog)
      m_teleportDialog->dismiss();
    auto teleportAllowed = m_client->canTeleport();
    if (!teleportAllowed)
      return std::unexpected(std::move(teleportAllowed.error()));
    if (!*teleportAllowed)
      return {};
    TeleportBookmark currentLocation;
    auto config = (*assets)->fetchJson(interactAction.data);
    if (!config)
      return std::unexpected(std::move(config.error()));
    auto canBookmark = config->getBool("canBookmark", false);
    if (!canBookmark)
      return std::unexpected(std::move(canBookmark.error()));
    auto canTeleport = config->getBool("canTeleport", true);
    if (!canTeleport)
      return std::unexpected(std::move(canTeleport.error()));
    if (*canBookmark) {
      if (auto entity = world->entity(interactAction.entityId)) {
        if (auto uniqueEntityId = entity->uniqueId()) {
          auto worldTemplate = world->currentTemplate();
          String icon, planetName;
          auto playerWorld = m_client->playerWorld();
          if (std::holds_alternative<ClientShipWorldId>(playerWorld)) {
            icon = "ship";
            planetName = "Player Ship";
          } else if (std::holds_alternative<CelestialWorldId>(playerWorld)
              || std::holds_alternative<InstanceWorldId>(playerWorld)
              || std::holds_alternative<CustomWorldId>(playerWorld)
              || std::holds_alternative<ClientCustomWorldId>(playerWorld)) {
            icon = worldTemplate->worldParameters()->typeName;
            planetName = worldTemplate->worldName();
          } else {
            icon = "default";
            planetName = "???";
          }
          auto name = config->getString("bookmarkName", "");
          if (!name)
            return std::unexpected(std::move(name.error()));
          currentLocation = TeleportBookmark{{playerWorld, SpawnTargetUniqueEntity(*uniqueEntityId)}, planetName, *name, icon};
          auto bookmarks = m_client->mainPlayer()->universeMap()->teleportBookmarks();
          if (!bookmarks)
            return std::unexpected(std::move(bookmarks.error()));
          if (!bookmarks->contains(currentLocation) || !*canTeleport) {
            auto bookmark = currentLocation.toJson();
            if (!bookmark)
              return std::unexpected(std::move(bookmark.error()));
            return sendPaneEvent("bookmarkEditor", "edit", JsonObject{
                {"bookmark", std::move(*bookmark)}, {"sourceEntityId", interactAction.entityId},
                {"sourceUniqueId", *uniqueEntityId}});
          }
        }
      }
    }
    if (*canTeleport) {
      auto sendEvent = [this, lifetime = m_callbackStopSource.get_token(), session = uiSession()](
          String const& id, String const& event, Json payload) -> std::expected<void, StarException> {
        if (lifetime.stop_requested())
          return std::unexpected(GuiException("Interface is detached"));
        if (session != uiSession())
          return {};
        return sendPaneEvent(id, event, std::move(payload));
      };
      auto dialog = TeleportDialog::create(context(), m_client, std::move(sendEvent), interactAction.data, interactAction.entityId, currentLocation);
      if (!dialog)
        return std::unexpected(std::move(dialog.error()));
      m_teleportDialog = std::move(*dialog);
      std::erase_if(m_teleportDialogs, [](auto const& dialog) { return dialog.expired(); });
      m_teleportDialogs.emplace_back(m_teleportDialog);
      auto displayed = m_paneManager.displayPane(PaneLayer::ModalWindow, m_teleportDialog);
      if (!displayed)
        return std::unexpected(std::move(displayed.error()));
    }
  } else if (interactAction.type == InteractActionType::ShowPopup) {
    auto message = interactAction.data.getString("message");
    if (!message)
      return std::unexpected(std::move(message.error()));
    auto title = interactAction.data.getString("title", "");
    if (!title)
      return std::unexpected(std::move(title.error()));
    auto subtitle = interactAction.data.getString("subtitle", "");
    if (!subtitle)
      return std::unexpected(std::move(subtitle.error()));
    auto sound = interactAction.data.optString("sound");
    if (!sound)
      return std::unexpected(std::move(sound.error()));
    JsonObject payload{{"message", std::move(*message)}, {"title", std::move(*title)}, {"subtitle", std::move(*subtitle)}};
    if (*sound)
      payload.set("sound", std::move(**sound));
    return sendPaneEvent("popup", "show", std::move(payload));
  } else if (interactAction.type == InteractActionType::ScriptPane) {
    auto sourceEntity = interactAction.entityId;
    if (sourceEntity != NullEntityId && m_interactionScriptPanes.contains(sourceEntity) && m_paneManager.isDisplayed(m_interactionScriptPanes[sourceEntity])) {
      if (auto dismissed = m_paneManager.dismissPane(m_interactionScriptPanes[sourceEntity]); !dismissed)
        return std::unexpected(std::move(dismissed.error()));
    }
    auto pane = ScriptPane::create(context(), m_client, interactAction.data, sourceEntity);
    if (!pane)
      return std::unexpected(std::move(pane.error()));
    return displayScriptPane(*pane, sourceEntity);
  } else if (interactAction.type == InteractActionType::Message) {
    auto type = interactAction.data.getString("messageType");
    if (!type)
      return std::unexpected(std::move(type.error()));
    auto arguments = interactAction.data.getArray("messageArgs");
    if (!arguments)
      return std::unexpected(std::move(arguments.error()));
    auto received = m_client->mainPlayer()->receiveMessage(connectionForEntity(interactAction.entityId), *type, *arguments);
    if (!received)
      return std::unexpected(std::move(received.error()));
  }
  return {};
}

std::expected<void, StarException> MainInterface::preUpdate(float) {
  auto player = m_client->mainPlayer();
  if (!m_client->paused())
    player->aim(cursorWorldPosition());
  if (m_paneManager.topPane({PaneLayer::Window, PaneLayer::ModalWindow}))
    return player->setBusyState(PlayerBusyState::Menu);
  auto focused = inputFocus();
  if (!focused)
    return std::unexpected(std::move(focused.error()));
  return player->setBusyState(*focused ? PlayerBusyState::Chatting : PlayerBusyState::None);
}

std::expected<void, StarException> MainInterface::update(float dt) {
  m_debugSpatialClearDue |= m_debugSpatialClearTimer.wrapTick(dt);
  m_debugMapClearDue |= m_debugMapClearTimer.wrapTick(dt);
  if (auto synchronized = synchronizeUiSession(); !synchronized)
    return std::unexpected(std::move(synchronized.error()));

  for (auto const& message : m_client->mainPlayer()->pullQueuedMessages()) {
    auto queued = queueMessage(message);
    if (!queued)
      return std::unexpected(std::move(queued.error()));
  }
  for (auto const& drop : m_client->mainPlayer()->pullQueuedItemDrops()) {
    auto queued = queueItemPickupText(drop);
    if (!queued)
      return std::unexpected(std::move(queued.error()));
  }
  auto controlled = m_uiController->update<Json>(dt);
  if (!controlled) return std::unexpected(std::move(controlled.error()));
  auto panesUpdated = m_paneManager.update(dt);
  if (!panesUpdated)
    return std::unexpected(std::move(panesUpdated.error()));
  auto cursorUpdated = m_cursor->update(dt);
  if (!cursorUpdated)
    return std::unexpected(std::move(cursorUpdated.error()));


  auto player = m_client->mainPlayer();
  if (player->wireToolInUse()) {
    auto displayed = m_paneManager.displayRegisteredPane(MainInterfacePanes::WireInterface);
    if (!displayed)
      return std::unexpected(std::move(displayed.error()));
    player->setWireConnector(std::ref(*m_wireInterface));
  } else {
    auto dismissed = m_paneManager.dismissRegisteredPane(MainInterfacePanes::WireInterface);
    if (!dismissed)
      return std::unexpected(std::move(dismissed.error()));
  }

  // update inventory pane items, to know if item slots changed
  auto inventoryUpdated = m_inventoryWindow->updateItems();
  if (!inventoryUpdated)
    return std::unexpected(std::move(inventoryUpdated.error()));



  if (!m_cinematicOverlay || m_cinematicOverlay->completed()) {
    auto cinematicState = m_client->mainPlayer()->setInCinematic(false);
    if (!cinematicState)
      return std::unexpected(std::move(cinematicState.error()));
  } else {
    auto cinematicState = m_client->mainPlayer()->setInCinematic(true);
    if (!cinematicState)
      return std::unexpected(std::move(cinematicState.error()));
  }


  auto chatAdded = addChatMessages(m_client->pullChatMessages(), true);
  if (!chatAdded)
    return std::unexpected(std::move(chatAdded.error()));
  auto commandResults = m_clientCommandProcessor->updatePromises();
  if (!commandResults)
    return std::unexpected(std::move(commandResults.error()));
  for (auto const& result : *commandResults) {
    auto added = addChatMessages({{{MessageContext::CommandResult}, ServerConnectionId, "", result}}, true);
    if (!added)
      return std::unexpected(std::move(added.error()));
  }

  if (auto worldClient = m_client->worldClient()) {
    if (worldClient->inWorld()) {
      auto pendingCinematic = m_client->mainPlayer()->pullPendingCinematic();
      if (!pendingCinematic)
        return std::unexpected(std::move(pendingCinematic.error()));
      if (auto& cinematic = *pendingCinematic) {
        if (*cinematic) {
          auto assets = context().root().assets();
          if (!assets)
            return std::unexpected(std::move(assets.error()));
          Json definition;
          auto fetched = (*assets)->fetchJson(std::move(*cinematic));
          if (!fetched) {
            auto logged = context().root().logger().warn("MainInterface: failed to fetch cinematic: {}", fetched.error().what());
            if (!logged)
              return std::unexpected(std::move(logged.error()));
          } else {
            definition = std::move(*fetched);
          }
          if (definition.isType(Json::Type::Object)) {
            auto loaded = m_cinematicOverlay->load(definition);
            if (!loaded) {
              auto logged = context().root().logger().warn("MainInterface: rejected malformed cinematic ({}), stopping overlay", loaded.error().what());
              if (!logged)
                return std::unexpected(std::move(logged.error()));
              m_cinematicOverlay->stop();
            }
          } else {
            auto logged = context().root().logger().warn("MainInterface: ignoring non-object cinematic of type {}", (int)definition.type());
            if (!logged)
              return std::unexpected(std::move(logged.error()));
            m_cinematicOverlay->stop();
          }
        } else {
          m_cinematicOverlay->stop();
        }
      }
    }
  }

  if (!m_confirmationDialog->isDisplayed()) {
    auto pendingConfirmation = m_client->mainPlayer()->pullPendingConfirmation();
    if (!pendingConfirmation)
      return std::unexpected(std::move(pendingConfirmation.error()));
    if (auto& confirmation = *pendingConfirmation) {
      auto displayed = m_paneManager.displayRegisteredPane(MainInterfacePanes::Confirmation);
      if (!displayed)
        return std::unexpected(std::move(displayed.error()));
      auto confirmed = m_confirmationDialog->displayConfirmation(confirmation->first, std::move(confirmation->second));
      if (!confirmed)
        return std::unexpected(std::move(confirmed.error()));
    }
  } else {
    auto confirmationSource = m_confirmationDialog->sourceEntityId();
    if (confirmationSource) {
      auto reachable = m_client->worldClient()->playerCanReachEntity(*confirmationSource);
      if (!reachable)
        return std::unexpected(std::move(reachable.error()));
      if (!*reachable)
        m_confirmationDialog->dismiss();
    }
  }

  if (!m_joinRequestDialog->isDisplayed()) {
    if (auto req = m_queuedJoinRequests.maybeTakeLast()) {
      auto displayed = m_paneManager.displayRegisteredPane(MainInterfacePanes::JoinRequest);
      if (!displayed)
        return std::unexpected(std::move(displayed.error()));
      auto userName = std::move(req->first);
      auto requested = m_joinRequestDialog->displayRequest(userName, [req = std::move(*req)](P2PJoinRequestReply reply) mutable -> std::expected<void, StarException> {
          return req.second.fulfill(reply);
        });
      if (!requested)
        return std::unexpected(std::move(requested.error()));
    }
  }

  for (EntityId id : m_interactionScriptPanes.keys()) {
    if (!m_paneManager.isDisplayed(m_interactionScriptPanes[id]))
      m_interactionScriptPanes.remove(id);
  }



  auto containerResults = m_containerInteractor->pullContainerResults();
  if (!containerResults)
    return std::unexpected(std::move(containerResults.error()));
  for (auto& containerResult : *containerResults) {
    bool handled = false;
    if (m_containerPane) {
      auto given = m_containerPane->giveContainerResult(containerResult);
      if (!given)
        return std::unexpected(std::move(given.error()));
      handled = *given;
    }
    if (!handled) {
      auto given = m_inventoryWindow->giveContainerResult(containerResult);
      if (!given)
        return std::unexpected(std::move(given.error()));
      if (!*given) {
        for (auto const& item : containerResult) {
          if (m_containerInteractor->containerOpen()) {
            auto remaining = m_client->mainPlayer()->pickupItems(item);
            if (!remaining)
              return std::unexpected(std::move(remaining.error()));
            auto returned = m_containerInteractor->addToContainer(*remaining);
            if (!returned)
              return std::unexpected(std::move(returned.error()));
          } else {
            auto givenToPlayer = m_client->mainPlayer()->giveItem(item);
            if (!givenToPlayer)
              return std::unexpected(std::move(givenToPlayer.error()));
          }
        }
      }
    }
  }

  auto cursorState = updateCursor();
  if (!cursorState)
    return std::unexpected(std::move(cursorState.error()));

  auto nameplatesUpdated = m_nameplatePainter->update(dt, m_client->worldClient(), m_worldPainter.get().camera(), m_client->worldClient()->interactiveHighlightMode());
  if (!nameplatesUpdated)
    return std::unexpected(std::move(nameplatesUpdated.error()));
  auto indicatorsUpdated = m_questIndicatorPainter->update(dt, m_client->worldClient(), m_worldPainter.get().camera());
  if (!indicatorsUpdated)
    return std::unexpected(std::move(indicatorsUpdated.error()));

  auto cameraSet = m_chatBubbleManager->setCamera(m_worldPainter.get().camera());
  if (!cameraSet)
    return std::unexpected(std::move(cameraSet.error()));
  if (auto worldClient = m_client->worldClient()) {
    auto chatActions = worldClient->pullPendingChatActions();
    if (!chatActions)
      return std::unexpected(std::move(chatActions.error()));

    for (auto& action : *chatActions) {
      if (std::holds_alternative<PortraitChatAction>(action)) {
        PortraitChatAction& portraitAction = std::get<PortraitChatAction>(action);

        String name;
        if (auto npc = as<Npc>(worldClient->entity(portraitAction.entity))) {
          auto npcName = npc->name();
          if (!npcName)
            return std::unexpected(std::move(npcName.error()));
          name = std::move(*npcName);
        }
        auto portrait = portraitAction.portrait.replace("<frame>", "0");
        if (!portrait)
          return std::unexpected(std::move(portrait.error()));

        ChatReceivedMessage message = {
          {MessageContext::World},
          ServerConnectionId,
          Text::stripEscapeCodes(name),
          Text::stripEscapeCodes(portraitAction.text),
          Text::stripEscapeCodes(*portrait)};
        auto added = addChatMessages({message}, false);
        if (!added)
          return std::unexpected(std::move(added.error()));
      } else if (std::holds_alternative<SayChatAction>(action)) {
        SayChatAction& sayAction = std::get<SayChatAction>(action);

        if (sayAction.config) {
          auto message = sayAction.config.opt("message");
          if (!message)
            return std::unexpected(std::move(message.error()));
          if (*message) {
            auto received = ChatReceivedMessage::create(**message);
            if (!received)
              return std::unexpected(std::move(received.error()));
            auto showPane = sayAction.config.getBool("showPane", false);
            if (!showPane)
              return std::unexpected(std::move(showPane.error()));
            auto added = addChatMessages({std::move(*received)}, *showPane);
            if (!added)
              return std::unexpected(std::move(added.error()));
          }
        }
      }
    }

    auto actionsAdded = m_chatBubbleManager->addChatActions(std::move(*chatActions));
    if (!actionsAdded)
      return std::unexpected(std::move(actionsAdded.error()));
    auto bubblesUpdated = m_chatBubbleManager->update(dt, worldClient);
    if (!bubblesUpdated)
      return std::unexpected(std::move(bubblesUpdated.error()));
  }

  if (auto container = m_client->worldClient()->get<ContainerEntity>(m_containerInteractor->openContainerId())) {
    auto reachable = m_client->worldClient()->playerCanReachEntity(container->entityId());
    if (!reachable)
      return std::unexpected(std::move(reachable.error()));
    if (!*reachable || !container->isInteractive()) {
      auto closed = m_containerInteractor->closeContainer();
      if (!closed)
        return std::unexpected(std::move(closed.error()));
    }
  }

  for (auto& pair : m_canvases) {
    pair.second->setPosition(Vec2I());
    auto size = pair.second->ignoreInterfaceScale() ? context().windowSize() : context().windowInterfaceSize();
    if (!size)
      return std::unexpected(std::move(size.error()));
    pair.second->setSize(Vec2I(*size));
    auto updated = pair.second->update(dt);
    if (!updated)
      return std::unexpected(std::move(updated.error()));
  }
  return {};
}

std::expected<void, StarException> MainInterface::renderInWorldElements(float interpolation) {
  if (m_disableHud)
    return {};
  if (auto cleared = context().clearTextStyle(); !cleared)
    return std::unexpected(std::move(cleared.error()));
  auto world = m_client->worldClient();
  if (!world)
    return {};
  auto const& camera = m_worldPainter.get().camera();
  if (auto rendered = m_questIndicatorPainter->render(*world, camera, interpolation); !rendered)
    return std::unexpected(std::move(rendered.error()));
  if (auto rendered = m_nameplatePainter->render(*world, camera, interpolation); !rendered)
    return std::unexpected(std::move(rendered.error()));
  return m_chatBubbleManager->render(*world, camera, interpolation);
}

std::expected<void, StarException> MainInterface::render() {
  if (m_disableHud)
    return {};
  if (auto cleared = context().clearTextStyle(); !cleared)
    return std::unexpected(std::move(cleared.error()));
  if (auto result = m_paneManager.renderLayers(PaneLayer::Background, PaneLayer::Background); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = renderDebug(); !result)
    return std::unexpected(std::move(result.error()));
  auto size = context().windowSize();
  if (!size)
    return std::unexpected(std::move(size.error()));
  RectI screenRect = RectI::withSize(Vec2I(), Vec2I(*size));
  for (auto& pair : m_canvases) {
    pair.second->setPosition(Vec2I());
    auto canvasSize = pair.second->ignoreInterfaceScale() ? context().windowSize() : context().windowInterfaceSize();
    if (!canvasSize)
      return std::unexpected(std::move(canvasSize.error()));
    pair.second->setSize(Vec2I(*canvasSize));
    auto rendered = pair.second->render(screenRect);
    if (!rendered)
      return std::unexpected(std::move(rendered.error()));
  }
  if (auto rendered = renderWindows(); !rendered)
    return std::unexpected(std::move(rendered.error()));
  return renderCursor();
}

Vec2F MainInterface::cursorWorldPosition() const {
  return m_worldPainter.get().camera().screenToWorld(m_cursorScreenPos);
}

bool MainInterface::isDebugDisplayed() {
  return m_clientCommandProcessor->debugDisplayEnabled();
}

std::expected<StringList, StarException> MainInterface::executeClientCommand(String const& command) {
  struct CommandInputScope {
    bool& enabled;
    bool previous;
    ~CommandInputScope() { enabled = previous; }
  } commandScope{m_commandInput, std::exchange(m_commandInput, false)};
  if (commandScope.previous)
    m_lastCommand = command;
  return m_clientCommandProcessor->handleCommand(command);
}

std::expected<void, StarException> MainInterface::queueMessage(String const& message, std::optional<float> cooldown, float spring) {
  JsonObject payload{{"text", message}, {"spring", spring}};
  if (cooldown)
    payload["cooldown"] = *cooldown;
  return sendPaneEvent("Notifications", "message", std::move(payload));
}

std::expected<void, StarException> MainInterface::queueMessage(String const& message) {
  return queueMessage(message, {}, 0.0f);
}

std::expected<void, StarException> MainInterface::queueJoinRequest(std::pair<String, RpcPromiseKeeper<P2PJoinRequestReply>> request) {
  m_queuedJoinRequests.push_back(std::move(request));
  return {};
}

std::expected<void, StarException> MainInterface::queueItemPickupText(std::shared_ptr<Item> const& item) {
  return sendPaneEvent("Notifications", "pickup", JsonObject{
      {"descriptor", item->descriptor().singular().toJson()},
      {"name", item->friendlyName()}, {"count", item->count()}});
}

bool MainInterface::fixedCamera() const {
  return m_clientCommandProcessor->fixedCameraEnabled();
}

bool MainInterface::hudVisible() const {
  return !m_disableHud;
}

void MainInterface::setHudVisible(bool visible) {
  m_disableHud = !visible;
}

std::expected<void, StarException> MainInterface::warpToOrbitedWorld(bool deploy) {
  auto allowed = m_client->canBeamDown(deploy);
  if (!allowed)
    return std::unexpected(std::move(allowed.error()));
  if (*allowed)
    return deploy ? m_client->warpPlayer(WarpAlias::OrbitedWorld, true, "deploy", true)
                  : m_client->warpPlayer(WarpAlias::OrbitedWorld, true, "beam");
  auto played = context().playAudio("/sfx/interface/clickon_error.ogg");
  if (!played)
    return std::unexpected(std::move(played.error()));
  return {};
}

std::expected<void, StarException> MainInterface::warpToOwnShip() {
  auto allowed = m_client->canBeamUp();
  if (!allowed)
    return std::unexpected(std::move(allowed.error()));
  if (*allowed)
    return warpTo(WarpAlias::OwnShip);
  auto played = context().playAudio("/sfx/interface/clickon_error.ogg");
  if (!played)
    return std::unexpected(std::move(played.error()));
  return {};
}

std::expected<void, StarException> MainInterface::warpTo(WarpAction const& warpAction) {
  if (m_client->beamUpRule() == BeamUpRule::AnywhereWithWarning) {
    if (m_confirmationDialog->isDisplayed())
      m_confirmationDialog->dismiss();
    auto displayed = m_paneManager.displayRegisteredPane(MainInterfacePanes::Confirmation);
    if (!displayed)
      return std::unexpected(std::move(displayed.error()));
    return m_confirmationDialog->displayConfirmation("/interface/windowconfig/beamupconfirmation.config",
      [this, warpAction, lifetime = callbackStopToken()](Widget&) -> std::expected<void, StarException> {
        if (lifetime.stop_requested())
          return std::unexpected(GuiException("Main interface is detached"));
        return m_client->warpPlayer(warpAction, true, "beam");
      },
      [](Widget&) -> std::expected<void, StarException> { return {}; });
  }
  return m_client->warpPlayer(warpAction, true, "beam");
}

std::expected<std::shared_ptr<CanvasWidget>, StarException> MainInterface::fetchCanvas(String const& canvasName, bool ignoreInterfaceScale) {
  if (auto existing = m_canvases.maybeRef(canvasName)) {
    existing->get()->setIgnoreInterfaceScale(ignoreInterfaceScale);
    return existing->get();
  }
  auto created = CanvasWidget::create(context());
  if (!created)
    return std::unexpected(std::move(created.error()));
  auto size = ignoreInterfaceScale ? context().windowSize() : context().windowInterfaceSize();
  if (!size)
    return std::unexpected(std::move(size.error()));
  auto canvas = std::move(*created);
  canvas->setPosition(Vec2I());
  canvas->setSize(Vec2I(*size));
  canvas->setIgnoreInterfaceScale(ignoreInterfaceScale);
  m_canvases.emplace(canvasName, canvas);
  return canvas;
}

std::shared_ptr<ClientCommandProcessor> MainInterface::commandProcessor() const {
  return m_clientCommandProcessor;
}

// For when the player swaps characters. We need to completely reload ScriptPanes,
// because a lot of ScriptPanes do not expect the character to suddenly change and may break or spill data over.
std::expected<void, StarException> MainInterface::takeScriptPanes(List<ScriptPaneInfo>& out) {
  return m_paneManager.dismissWhere([&](std::shared_ptr<Pane> const& pane) -> std::expected<bool, StarException> {
    if (auto scriptPane = as<ScriptPane>(pane)) {
      if (scriptPane->isDismissed())
        return false;
      auto sourceEntityId = scriptPane->sourceEntityId();
      m_interactionScriptPanes.remove(sourceEntityId);
      auto& info = out.emplaceAppend();
      info.scriptPane = scriptPane;
      info.config = scriptPane->rawConfig();
      info.sourceEntityId = sourceEntityId;
      info.visible = scriptPane->visibility();
      info.position = scriptPane->relativePosition();
      return true;
    }
    return false;
  });
}

std::expected<void, StarException> MainInterface::reviveScriptPanes(List<ScriptPaneInfo>& panes) {
  for (auto& info : panes) {
    auto created = ScriptPane::create(context(), m_client, info.config, info.sourceEntityId);
    if (!created)
      return std::unexpected(std::move(created.error()));
    auto pane = std::move(*created);
    if (auto visible = pane->setVisibility(info.visible); !visible)
      return std::unexpected(std::move(visible.error()));
    if (info.scriptPane == m_mmUpgrade || info.scriptPane == m_collections) {
      auto id = info.scriptPane == m_mmUpgrade ? MainInterfacePanes::MmUpgrade : MainInterfacePanes::Collections;
      auto removed = m_paneManager.deregisterPane(id);
      if (!removed)
        return std::unexpected(std::move(removed.error()));
      if (auto registered = m_paneManager.registerPane(id, PaneLayer::Window, pane); !registered)
        return std::unexpected(std::move(registered.error()));
      if (id == MainInterfacePanes::MmUpgrade)
        m_mmUpgrade = pane;
      else
        m_collections = pane;
    }
    if (auto displayed = displayScriptPane(pane, info.sourceEntityId); !displayed)
      return std::unexpected(std::move(displayed.error()));
    pane->setPosition(info.position);
    info.scriptPane = std::move(pane);
  }
  return {};
}


float MainInterface::interfaceScale() const {
  return context().interfaceScale();
}

std::expected<unsigned, StarException> MainInterface::windowHeight() const {
  return context().windowHeight();
}

std::expected<unsigned, StarException> MainInterface::windowWidth() const {
  return context().windowWidth();
}


std::expected<void, StarException> MainInterface::renderWindows() {
  return m_paneManager.renderLayers(PaneLayer::Tooltip, PaneLayer::World);
}

std::expected<void, StarException> MainInterface::renderDebug() {
  if (!isDebugDisplayed()) {
    context().root().spatialLogger().clear();
    m_debugTextRect = RectF::null();
    context().root().logMap().clear();
    context().root().spatialLogger().setObserved(false);
    return {};
  }
  context().root().spatialLogger().setObserved(true);
  auto drawDebug = [&]() -> std::expected<void, StarException> {
    if (m_clientCommandProcessor->debugHudEnabled()) {
      if (auto r = context().setTextStyle(m_config->debugTextStyle); !r) return std::unexpected(std::move(r.error()));
      if (auto r = context().setLineSpacing(0.5f); !r) return std::unexpected(std::move(r.error()));
      bool clearMap = std::exchange(m_debugMapClearDue, false);
      auto logMapValues = context().root().logMap().getValues();
      if (clearMap) context().root().logMap().clear();
      auto height = windowHeight();
      if (!height) return std::unexpected(std::move(height.error()));
      List<String> formatted;
      formatted.reserve(logMapValues.size());
      int counter = 0;
      for (auto const& pair : logMapValues) {
        TextPositioning positioning = {Vec2F(m_config->debugOffset[0], *height - m_config->debugOffset[1] - m_config->textStyle.fontSize * interfaceScale() * counter++)};
        String& text = formatted.emplace_back(strf("{}^lightgray;:^green,set; {}", pair.first, pair.second));
        auto size = context().determineTextSize(text, positioning);
        if (!size) return std::unexpected(std::move(size.error()));
        m_debugTextRect.combine(size->padded(m_config->debugBackgroundPad));
      }
      if (!m_debugTextRect.isNull()) {
        auto renderer = context().renderer();
        if (!renderer) return std::unexpected(std::move(renderer.error()));
        RenderQuad& quad = std::get<RenderQuad>(renderer->get().immediatePrimitives()
            .emplace_back(std::in_place_type_t<RenderQuad>(), m_debugTextRect, m_config->debugBackgroundColor.toRgba(), 0.0f));
        quad.b.color[3] = quad.c.color[3] = 0;
      }
      m_debugTextRect = RectF::null();
      for (std::size_t index = 0; index != formatted.size(); ++index) {
        TextPositioning positioning = {Vec2F(m_config->debugOffset[0], *height - m_config->debugOffset[1] - m_config->textStyle.fontSize * interfaceScale() * index)};
        if (auto r = context().renderText(formatted[index], positioning); !r) return std::unexpected(std::move(r.error()));
      }
    }
    auto const& camera = m_worldPainter.get().camera();
    bool clearSpatial = std::exchange(m_debugSpatialClearDue, false);
    for (auto const& line : context().root().spatialLogger().getLines("world", clearSpatial)) {
      Vec2F begin = camera.worldToScreen(line.begin);
      Vec2F end = camera.worldGeometry().diff(line.end, line.begin) * camera.pixelRatio() * TilePixels + begin;
      if (auto r = context().drawLine(begin, end, line.color, 1); !r) return std::unexpected(std::move(r.error()));
    }
    for (auto const& line : context().root().spatialLogger().getLines("screen", clearSpatial)) {
      if (auto r = context().drawLine(Vec2F(line.begin), Vec2F(line.end), line.color, 1); !r) return std::unexpected(std::move(r.error()));
    }
    auto drawPoint = [&](Vec2F position, Vec4B const& color) -> std::expected<void, StarException> {
      if (auto r = context().drawLine(position + Vec2F(-2, -2), position + Vec2F(-2, 2), color, 1); !r) return std::unexpected(std::move(r.error()));
      if (auto r = context().drawLine(position + Vec2F(-2, 2), position + Vec2F(2, 2), color, 1); !r) return std::unexpected(std::move(r.error()));
      if (auto r = context().drawLine(position + Vec2F(2, 2), position + Vec2F(2, -2), color, 1); !r) return std::unexpected(std::move(r.error()));
      return context().drawLine(position + Vec2F(2, -2), position + Vec2F(-2, -2), color, 1);
    };
    for (auto const& point : context().root().spatialLogger().getPoints("world", clearSpatial)) {
      if (auto r = drawPoint(camera.worldToScreen(point.position), point.color); !r) return std::unexpected(std::move(r.error()));
    }
    for (auto const& point : context().root().spatialLogger().getPoints("screen", clearSpatial)) {
      if (auto r = drawPoint(point.position, point.color); !r) return std::unexpected(std::move(r.error()));
    }
    if (auto r = context().setTextStyle(m_config->debugTextStyle); !r) return std::unexpected(std::move(r.error()));
    for (auto const& logText : context().root().spatialLogger().getText("world", clearSpatial)) {
      if (auto r = context().setFontColor(logText.color); !r) return std::unexpected(std::move(r.error()));
      if (auto r = context().renderText(logText.text, camera.worldToScreen(logText.position)); !r)
        return std::unexpected(std::move(r.error()));
    }
    for (auto const& logText : context().root().spatialLogger().getText("screen", clearSpatial)) {
      if (auto r = context().setFontColor(logText.color); !r) return std::unexpected(std::move(r.error()));
      if (auto r = context().renderText(logText.text, logText.position); !r)
        return std::unexpected(std::move(r.error()));
    }
    return {};
  };
  auto drawn = drawDebug();
  auto cleared = context().clearTextStyle();
  if (!drawn && !cleared)
    return std::unexpected(drawn.error().withRelated(cleared.error(), "Debug text cleanup failed"));
  if (!drawn) return std::unexpected(std::move(drawn.error()));
  return cleared;
}

std::expected<void, StarException> MainInterface::updateCursor() {
  auto actionOverride = m_uiController->invoke<std::optional<String>>("cursorOverride", m_cursorScreenIPos / interfaceScale());
  if (!actionOverride) return std::unexpected(std::move(actionOverride.error()));
  std::optional<String> cursorOverride = actionOverride->value_or(std::optional<String>{});

  if (!cursorOverride) {
    if (auto pane = m_paneManager.getPaneAt(m_cursorScreenIPos / interfaceScale())) {
      auto override = pane->cursorOverride(m_cursorScreenIPos / interfaceScale());
      if (!override) return std::unexpected(std::move(override.error()));
      cursorOverride = std::move(*override);
    } else {
      auto player = m_client->mainPlayer();
      if (auto anchorState = m_client->mainPlayer()->loungingIn()) {
        if (auto loungeable = m_client->worldClient()->get<LoungeableEntity>(anchorState->entityId)) {
          auto loungeAnchor = loungeable->loungeAnchor(anchorState->positionIndex);
          if (!loungeAnchor) return std::unexpected(std::move(loungeAnchor.error()));
          if (*loungeAnchor) cursorOverride = (*loungeAnchor)->cursorOverride;
        }
      }
      if (!cursorOverride) {
        for (auto item : {player->primaryHandItem(), player->altHandItem()}) {
          if (auto activeItem = as<ActiveItem>(item)) {
            if (auto cursor = activeItem->cursor()) {
              cursorOverride = cursor;
              break;
            }
          } else if (auto inspectionTool = as<InspectionTool>(item)) {
            cursorOverride = String("/cursors/inspect.cursor");
            break;
          }
        }
      }
    }
  }

  if (cursorOverride)
    return m_cursor->setCursor(*cursorOverride);
  return m_cursor->resetCursor();
}

std::expected<void, StarException> MainInterface::renderCursor() {
  // A cinematic hides both hardware and drawn cursors.
  if (m_cinematicOverlay && !m_cinematicOverlay->completed()) {
    context().applicationController()->setCursorVisible(false);
    return {};
  }
  Vec2I cursorPos = m_cursorScreenIPos;
  Vec2I cursorSize = m_cursor->size();
  Vec2I cursorOffset = m_cursor->offset();
  float cursorScale = m_cursor->scale(interfaceScale());
  auto cursorDrawable = m_cursor->drawable();
  if (!cursorDrawable) return std::unexpected(std::move(cursorDrawable.error()));
  cursorPos[0] -= cursorOffset[0] * cursorScale;
  cursorPos[1] -= (cursorSize[1] - cursorOffset[1]) * cursorScale;
  auto hardwareCursor = context().trySetCursor(*cursorDrawable, cursorOffset, cursorScale);
  if (!hardwareCursor) return std::unexpected(std::move(hardwareCursor.error()));
  if (!*hardwareCursor) {
    if (auto r = context().drawDrawable(*cursorDrawable, Vec2F(cursorPos), cursorScale); !r)
      return std::unexpected(std::move(r.error()));
  }
  m_cursorItem->setPosition(Vec2I::round(m_cursorScreenPos / interfaceScale() + Vec2F(m_config->inventoryItemMouseOffset)));
  m_cursorItem->setItem(m_client->mainPlayer()->inventory()->swapSlotItem());
  auto drawItem = [&]() -> std::expected<void, StarException> {
    auto width = windowWidth();
    if (!width) return std::unexpected(std::move(width.error()));
    auto height = windowHeight();
    if (!height) return std::unexpected(std::move(height.error()));
    return m_cursorItem->render(RectI::withSize({}, {(int)*width, (int)*height}));
  };
  auto drawn = drawItem();
  auto reset = context().resetInterfaceScissorRect();
  if (!drawn && !reset)
    return std::unexpected(drawn.error().withRelated(reset.error(), "Cursor item scissor cleanup failed"));
  if (!drawn) return std::unexpected(std::move(drawn.error()));
  return reset;
}


std::expected<void, StarException> MainInterface::initHttpTrustDialog() {
  auto dialog = HttpTrustDialog::create(context());
  if (!dialog)
    return std::unexpected(std::move(dialog.error()));
  return m_paneManager.registerPane(MainInterfacePanes::HttpTrustDialog, PaneLayer::ModalWindow, *dialog);
}

std::expected<void, StarException> MainInterface::displayScriptPane(std::shared_ptr<ScriptPane>& scriptPane, EntityId sourceEntity) {
  if (sourceEntity != NullEntityId)
    m_interactionScriptPanes[sourceEntity] = scriptPane;
  PaneLayer layer = PaneLayer::Window;
  auto layerName = scriptPane->config().optString("paneLayer");
  if (!layerName)
    return std::unexpected(std::move(layerName.error()));
  if (*layerName) {
    if (!PaneLayerNames)
      return std::unexpected(PaneLayerNames.error());
    auto parsed = PaneLayerNames->getLeft(**layerName);
    if (!parsed)
      return std::unexpected(std::move(parsed.error()));
    layer = parsed->get();
  }
  auto withInventory = scriptPane->openWithInventory();
  if (!withInventory)
    return std::unexpected(std::move(withInventory.error()));
  if (*withInventory) {
    auto close = scriptPane->closeWithInventory();
    if (!close)
      return std::unexpected(std::move(close.error()));
    auto inventoryPane = m_paneManager.registeredPane(MainInterfacePanes::Inventory);
    if (!inventoryPane)
      return std::unexpected(std::move(inventoryPane.error()));
    auto assets = context().root().assets();
    if (!assets)
      return std::unexpected(std::move(assets.error()));
    auto gapConfig = (*assets)->json("/interface.config:bringAdjacentWindowGap");
    if (!gapConfig)
      return std::unexpected(std::move(gapConfig.error()));
    auto gap = gapConfig->toFloat();
    if (!gap)
      return std::unexpected(std::move(gap.error()));
    auto displayed = m_paneManager.displayPane(layer, scriptPane, [this, close = *close](std::shared_ptr<Pane> const&) -> std::expected<void, StarException> {
      if (close) {
        auto dismissed = m_paneManager.dismissRegisteredPane(MainInterfacePanes::Inventory);
        if (!dismissed)
          return std::unexpected(std::move(dismissed.error()));
      }
      return {};
    });
    if (!displayed)
      return std::unexpected(std::move(displayed.error()));
    auto inventoryDisplayed = m_paneManager.displayRegisteredPane(MainInterfacePanes::Inventory);
    if (!inventoryDisplayed)
      return std::unexpected(std::move(inventoryDisplayed.error()));
    if (auto positioned = m_paneManager.bringPaneAdjacent(*inventoryPane, scriptPane, *gap); !positioned)
      return std::unexpected(std::move(positioned.error()));
  } else {
    auto displayed = m_paneManager.displayPane(layer, scriptPane);
    if (!displayed)
      return std::unexpected(std::move(displayed.error()));
  }
  return {};
}

}

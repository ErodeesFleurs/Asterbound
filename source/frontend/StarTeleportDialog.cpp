import std;
import star.memory;
import star.xx_hash;
import star.json;
import star.ordered_map;
import star.either;
import star.game_types;
import star.color;
import star.bi_map;
import star.random;
import star.weighted_pool;
import star.list;
import star.image_processing;
import star.hash;
import star.data_stream;
import star.string_view;
import star.thread;
import star.directives;
import star.asset_path;
import star.casting;
import star.input_event;
import star.font;
import star.io_device;
import star.ref_ptr;
import star.logging;
import star.format;
import star.version;
import star.string;
import star.text;
import star.listener;
import star.set;
import star.audio;
import star.map;
import star.strong_typedef;
import star.id_map;
import star.image;
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
import star.byte_array;

typedef struct ZSTD_CCtx_s ZSTD_CCtx;
typedef struct ZSTD_DCtx_s ZSTD_DCtx;
typedef ZSTD_DCtx ZSTD_DStream;
typedef ZSTD_CCtx ZSTD_CStream;
import star.zstd_compression;
import star.a_star;
import star.periodic_function;

import star.lua_root;
import star.celestial_coordinate;
import star.sky_types;
import star.animation;
import star.particle;
import star.weather_types;
import star.force_regions;
import star.world_parameters;
import star.celestial_parameters;
import star.celestial_types;

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
import star.uuid;
import star.warping;

import star.sky_parameters;
import star.system_world;
import star.player_universe_map;




import star.teleport_dialog;
import star.collision_block;
import star.liquid_types;
import star.tile_damage;
import star.tile_sector_array;
import star.world_layout;
import star.collision_generator;
import star.world_tiles;
import star.item_descriptor;
import star.chat_types;
import star.tile_modification;
import star.damage_types;
import star.status_types;
import star.damage;
import star.light_source;
import star.entity;
import star.interaction_types;
import star.world_geometry;
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
import star.world_client;
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
import star.lua_actor_movement_component;
import star.lua_animation_component;
import star.radio_message_database;
import star.player;
import star.widget_parsing;
import star.gui_reader;
import star.game_timers;
import star.button_group;
import star.button_widget;
import star.image_widget;
import star.label_widget;
import star.list_widget;


import star.celestial_database;

import star.client_context;
import star.team_client;

import star.quest_manager;

import star.algorithm;
namespace Star {

namespace {
[[nodiscard]] std::expected<void, StarException> setupTeleportEntry(Widget& entry, String const& name, String const& planet, std::optional<String> const& icon) {
  auto nameWidget = entry.fetchChild<LabelWidget>("name");
  if (!nameWidget) return std::unexpected(std::move(nameWidget.error()));
  auto nameText = (*nameWidget)->setText(name);
  if (!nameText) return std::unexpected(std::move(nameText.error()));
  auto planetWidget = entry.fetchChild<LabelWidget>("planetName");
  if (!planetWidget) return std::unexpected(std::move(planetWidget.error()));
  auto planetText = (*planetWidget)->setText(planet);
  if (!planetText) return std::unexpected(std::move(planetText.error()));
  if (icon) {
    auto iconWidget = entry.fetchChild<ImageWidget>("icon");
    if (!iconWidget) return std::unexpected(std::move(iconWidget.error()));
    auto image = (*iconWidget)->setImage(*icon);
    if (!image) return std::unexpected(std::move(image.error()));
  }
  auto editButton = entry.fetchChild<ButtonWidget>("editButton");
  if (!editButton) return std::unexpected(std::move(editButton.error()));
  return (*editButton)->hide();
}

[[nodiscard]] std::expected<void, StarException> setupBookmarkEntry(Widget& entry, TeleportBookmark const& bookmark) {
  auto nameWidget = entry.fetchChild<LabelWidget>("name");
  if (!nameWidget) return std::unexpected(std::move(nameWidget.error()));
  auto name = (*nameWidget)->setText(bookmark.bookmarkName);
  if (!name) return std::unexpected(std::move(name.error()));
  auto planetNameWidget = entry.fetchChild<LabelWidget>("planetName");
  if (!planetNameWidget) return std::unexpected(std::move(planetNameWidget.error()));
  auto planetName = (*planetNameWidget)->setText(bookmark.targetName);
  if (!planetName) return std::unexpected(std::move(planetName.error()));
  auto iconWidget = entry.fetchChild<ImageWidget>("icon");
  if (!iconWidget) return std::unexpected(std::move(iconWidget.error()));
  auto icon = (*iconWidget)->setImage(strf("/interface/bookmarks/icons/{}.png", bookmark.icon));
  if (!icon) return std::unexpected(std::move(icon.error()));
  auto editButton = entry.fetchChild<ButtonWidget>("editButton");
  if (!editButton) return std::unexpected(std::move(editButton.error()));
  auto data = bookmark.toJson();
  if (!data) return std::unexpected(std::move(data.error()));
  (*editButton)->setData(std::move(*data));
  return {};
}
}

TeleportDialog::TeleportDialog(GuiContext& guiContext, std::shared_ptr<UniverseClient> client, SendPaneEvent sendPaneEvent, EntityId sourceEntityId, TeleportBookmark currentLocation)
  : Pane(guiContext), m_sourceEntityId(sourceEntityId), m_client(std::move(client)), m_sendPaneEvent(std::move(sendPaneEvent)), m_currentLocation(std::move(currentLocation)) {}

std::expected<std::shared_ptr<TeleportDialog>, StarException> TeleportDialog::create(GuiContext& guiContext, std::shared_ptr<UniverseClient> client, SendPaneEvent sendPaneEvent, Json config, EntityId sourceEntityId, TeleportBookmark currentLocation) {
  auto pane = std::shared_ptr<TeleportDialog>(new TeleportDialog(guiContext, std::move(client), std::move(sendPaneEvent), sourceEntityId, std::move(currentLocation)));
  auto base = pane->initializePane();
  if (!base) return std::unexpected(std::move(base.error()));
  auto initialized = pane->initialize(std::move(config));
  if (!initialized) return std::unexpected(std::move(initialized.error()));
  return pane;
}

std::expected<void, StarException> TeleportDialog::initialize(Json config) {
  auto assets = context().root().assets();
  if (!assets) return std::unexpected(std::move(assets.error()));
  GuiReader reader(context());
  reader.registerCallback("dismiss", [this](Widget&) -> std::expected<void, StarException> { dismiss(); return {}; });
  reader.registerCallback("teleport", [this](Widget&) { return teleport(); });
  reader.registerCallback("selectDestination", [this](Widget&) { return selectDestination(); });
  auto layout = (*assets)->json("/interface/windowconfig/teleportdialog.config:paneLayout");
  if (!layout) return std::unexpected(std::move(layout.error()));
  auto constructed = reader.construct(*layout, *this);
  if (!constructed) return std::unexpected(std::move(constructed.error()));
  auto fetched = (*assets)->fetchJson(config);
  if (!fetched) return std::unexpected(std::move(fetched.error()));
  config = std::move(*fetched);
  auto destList = fetchChild<ListWidget>("bookmarkList.bookmarkItemList");
  if (!destList) return std::unexpected(std::move(destList.error()));
  (*destList)->registerMemberCallback("editBookmark", [this](Widget& button) { return editBookmark(button); });
  auto destinations = config.getArray("destinations", JsonArray());
  if (!destinations) return std::unexpected(std::move(destinations.error()));
  for (auto const& dest : *destinations) {
    auto prerequisite = dest.optString("prerequisiteQuest");
    if (!prerequisite) return std::unexpected(std::move(prerequisite.error()));
    if (*prerequisite && !m_client->mainPlayer()->questManager()->hasCompleted(**prerequisite)) continue;
    auto warpText = dest.getString("warpAction");
    if (!warpText) return std::unexpected(std::move(warpText.error()));
    auto warpAction = parseWarpAction(*warpText);
    if (!warpAction) return std::unexpected(std::move(warpAction.error()));
    auto deploy = dest.getBool("deploy", false);
    if (!deploy) return std::unexpected(std::move(deploy.error()));
    if (std::holds_alternative<WarpAlias>(*warpAction) && std::get<WarpAlias>(*warpAction) == WarpAlias::OrbitedWorld) {
      auto canBeamDown = m_client->canBeamDown(*deploy);
      if (!canBeamDown) return std::unexpected(std::move(canBeamDown.error()));
      if (!*canBeamDown) continue;
    }
    auto entry = (*destList)->addItem();
    if (!entry) return std::unexpected(std::move(entry.error()));
    auto name = dest.getString("name");
    if (!name) return std::unexpected(std::move(name.error()));
    auto planet = dest.getString("planetName", "");
    if (!planet) return std::unexpected(std::move(planet.error()));
    auto iconName = dest.optString("icon");
    if (!iconName) return std::unexpected(std::move(iconName.error()));
    std::optional<String> icon;
    if (*iconName) icon = strf("/interface/bookmarks/icons/{}.png", **iconName);
    auto setup = setupTeleportEntry(**entry, *name, *planet, icon);
    if (!setup) return std::unexpected(std::move(setup.error()));
    auto mission = dest.getBool("mission", false);
    if (!mission) return std::unexpected(std::move(mission.error()));
    if (*mission) {
      // if the warpaction is for an instance world, set the uuid to the team uuid
      if (std::holds_alternative<WarpToWorld>(*warpAction)) {
        auto const& warpToWorld = std::get<WarpToWorld>(*warpAction);
        if (std::holds_alternative<InstanceWorldId>(warpToWorld.world)) {
          auto const& worldId = std::get<InstanceWorldId>(warpToWorld.world);
          *warpAction = WarpToWorld(InstanceWorldId(worldId.instance, m_client->teamUuid(), worldId.level), warpToWorld.target);
        }
      }
    }
    m_destinations.append({std::move(*warpAction), *deploy});
  }
  auto beamLabelConfig = (*assets)->json("/interface/windowconfig/teleportdialog.config:beamPartyMemberLabel");
  if (!beamLabelConfig) return std::unexpected(std::move(beamLabelConfig.error()));
  auto beamPartyMember = beamLabelConfig->toString();
  if (!beamPartyMember) return std::unexpected(std::move(beamPartyMember.error()));
  auto deployLabelConfig = (*assets)->json("/interface/windowconfig/teleportdialog.config:deployPartyMemberLabel");
  if (!deployLabelConfig) return std::unexpected(std::move(deployLabelConfig.error()));
  auto deployPartyMember = deployLabelConfig->toString();
  if (!deployPartyMember) return std::unexpected(std::move(deployPartyMember.error()));
  auto beamIconConfig = (*assets)->json("/interface/windowconfig/teleportdialog.config:beamPartyMemberIcon");
  if (!beamIconConfig) return std::unexpected(std::move(beamIconConfig.error()));
  auto beamPartyMemberIcon = beamIconConfig->toString();
  if (!beamPartyMemberIcon) return std::unexpected(std::move(beamPartyMemberIcon.error()));
  auto deployIconConfig = (*assets)->json("/interface/windowconfig/teleportdialog.config:deployPartyMemberIcon");
  if (!deployIconConfig) return std::unexpected(std::move(deployIconConfig.error()));
  auto deployPartyMemberIcon = deployIconConfig->toString();
  if (!deployPartyMemberIcon) return std::unexpected(std::move(deployPartyMemberIcon.error()));
  auto includeParty = config.getBool("includePartyMembers", false);
  if (!includeParty) return std::unexpected(std::move(includeParty.error()));
  if (*includeParty) {
    for (auto const& member : m_client->teamClient()->members()) {
      if (member.uuid == m_client->clientContext()->playerUuid() || member.warpMode == WarpMode::None) continue;
      auto entry = (*destList)->addItem();
      if (!entry) return std::unexpected(std::move(entry.error()));
      bool deploy = member.warpMode == WarpMode::DeployOnly;
      auto setup = setupTeleportEntry(**entry, member.name, deploy ? *deployPartyMember : *beamPartyMember, deploy ? *deployPartyMemberIcon : *beamPartyMemberIcon);
      if (!setup) return std::unexpected(std::move(setup.error()));
      m_destinations.append({WarpToPlayer(member.uuid), deploy});
    }
  }
  auto includeBookmarks = config.getBool("includePlayerBookmarks", false);
  if (!includeBookmarks) return std::unexpected(std::move(includeBookmarks.error()));
  if (*includeBookmarks) {
    auto bookmarks = m_client->mainPlayer()->universeMap()->teleportBookmarks();
    if (!bookmarks) return std::unexpected(std::move(bookmarks.error()));
    auto sorted = sortByComputedValue(*bookmarks, [](TeleportBookmark const& bookmark) { return bookmark.bookmarkName.toLower(); });
    if (!sorted) return std::unexpected(std::move(sorted.error()));
    for (auto const& bookmark : *bookmarks) {
      auto entry = (*destList)->addItem();
      if (!entry) return std::unexpected(std::move(entry.error()));
      auto setup = setupBookmarkEntry(**entry, bookmark);
      if (!setup) return std::unexpected(std::move(setup.error()));
      if (bookmark == m_currentLocation) {
        auto disabled = (*destList)->setEnabled((*destList)->itemPosition(*entry), false);
        if (!disabled) return std::unexpected(std::move(disabled.error()));
        auto editButton = (*entry)->fetchChild<ButtonWidget>("editButton");
        if (!editButton) return std::unexpected(std::move(editButton.error()));
        (*editButton)->setEnabled(false);
      }
      m_destinations.append({WarpToWorld(bookmark.target.first, bookmark.target.second), false});
    }
  }
  return selectDestination();
}

std::expected<void, StarException> TeleportDialog::tick(float) {
  auto reachable = m_client->worldClient()->playerCanReachEntity(m_sourceEntityId);
  if (!reachable) return std::unexpected(std::move(reachable.error()));
  if (!*reachable) dismiss();
  return {};
}

std::expected<void, StarException> TeleportDialog::selectDestination() {
  auto destList = fetchChild<ListWidget>("bookmarkList.bookmarkItemList");
  if (!destList) return std::unexpected(std::move(destList.error()));
  auto button = fetchChild<ButtonWidget>("btnTeleport");
  if (!button) return std::unexpected(std::move(button.error()));
  (*button)->setEnabled((*destList)->selectedItem() != NPos);
  return {};
}

std::expected<void, StarException> TeleportDialog::teleport() {
  auto destList = fetchChild<ListWidget>("bookmarkList.bookmarkItemList");
  if (!destList) return std::unexpected(std::move(destList.error()));
  if ((*destList)->selectedItem() != NPos) {
    auto destination = m_destinations.at((*destList)->selectedItem());
    if (!destination) return std::unexpected(std::move(destination.error()));
    auto& selected = destination->get();
    auto text = printWarpAction(selected.first);
    if (!text) return std::unexpected(std::move(text.error()));
    auto message = m_client->worldClient()->sendEntityMessage(m_sourceEntityId, "onTeleport", {*text});
    if (!message) return std::unexpected(std::move(message.error()));
    bool deploy = selected.second;
    bool beam = std::holds_alternative<WarpAlias>(selected.first) && std::get<WarpAlias>(selected.first) == WarpAlias::OrbitedWorld;
    auto action = std::exchange(selected, {}).first;
    auto warped = deploy ? m_client->warpPlayer(action, true, "deploy", true) : m_client->warpPlayer(action, true, beam ? String("beam") : String("default"));
    if (!warped) return std::unexpected(std::move(warped.error()));
    dismiss();
  }
  return {};
}

std::expected<void, StarException> TeleportDialog::editBookmark(Widget& button) {
  if (!m_sendPaneEvent) return std::unexpected(StarException("Teleport dialog is detached from its interface"));
  // The clicked row owns its canonical target identity. Re-reading and sorting
  // the live bookmark set can map an old row to a different bookmark.
  JsonObject payload{{"bookmark", button.data()}, {"sourceEntityId", m_sourceEntityId}};
  if (auto source = m_client->worldClient()->entity(m_sourceEntityId)) {
    if (auto uniqueId = source->uniqueId())
      payload["sourceUniqueId"] = *uniqueId;
  } else {
    dismiss();
    return {};
  }
  auto sent = m_sendPaneEvent("bookmarkEditor", "edit", std::move(payload));
  if (!sent) return std::unexpected(std::move(sent.error()));
  dismiss();
  return {};
}

}

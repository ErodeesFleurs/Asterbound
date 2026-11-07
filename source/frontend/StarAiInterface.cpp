import star.json_extra;
import std;
import star.format;
import star.memory;
import star.json;
import star.xx_hash;
import star.id_map;
import star.ordered_map;
import star.lru_cache;
import star.thread;
import star.time;
import star.random;
import star.ttl_cache;
import star.casting;
import star.game_types;
import star.rpc_promise;
import star.vector;
import star.bi_map;
import star.a_star;
import star.data_stream;
import star.string;
import star.color;
import star.list;
import star.image_processing;
import star.hash;
import star.string_view;
import star.directives;
import star.asset_path;
import star.ordered_set;
import star.strong_typedef;
import star.input_event;
import star.font;
import star.text;
import star.listener;
import star.set;
import star.audio;
import star.map;
import star.lexical_cast;
import star.json_rpc;
import star.either;
import star.io_device;
import star.ref_ptr;
import star.logging;
import star.version;
import star.periodic_function;
import star.weighted_pool;
import star.byte_array;
import star.data_stream_devices;

typedef struct ZSTD_CCtx_s ZSTD_CCtx;
typedef struct ZSTD_DCtx_s ZSTD_DCtx;
typedef ZSTD_DCtx ZSTD_DStream;
typedef ZSTD_CCtx ZSTD_CStream;
import star.zstd_compression;
import star.net_compatibility;
import star.math_common;
import star.worker_pool;

import star.sector_array_2d;
import star.perlin;
import star.btree;
import star.btree_database;
import star.image;

import star.lua_root;

import star.lua_components;
import star.lua_actor_movement_component;
import star.lua_animation_component;
import star.damage_types;
import star.world_geometry;
import star.status_types;
import star.damage;
import star.light_source;
import star.entity;
import star.tile_damage;
import star.interaction_types;
import star.interactive_entity;
import star.collision_block;
import star.tile_entity;
import star.tile_modification;
import star.force_regions;
import star.world;
import star.physics_entity;
import star.movement_controller;
import star.platformer_astar_types;
import star.anchorable_entity;

import star.game_timers;
import star.actor_movement_controller;
import star.item;
import star.item_descriptor;
import star.item_database;
import star.item_recipe;
import star.drawable;
import star.uuid;
import star.player_companions;

import star.celestial_coordinate;
import star.quest_descriptor;
import star.ai_types;
import star.warping;
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
import star.asset_texture_group;
import star.drawable_painter;
import star.gui_types;
import star.key_bindings;
import star.mixer;
import star.gui_context;
import star.widget;
import star.pane;

import star.pane_manager;
import star.registered_pane_manager;


import star.main_interface_types;
import star.animation;
import star.tech_database;



import star.ai_interface;
import star.container_entity;
import star.particle;
import star.animated_part_set;
import star.networked_animator;
import star.humanoid;
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
import star.entity_rendering_types;
import star.entity_rendering;
import star.player_types;
import star.radio_message_database;
import star.player;
import star.host_address;
import star.sky_types;
import star.weather_types;
import star.world_parameters;
import star.celestial_parameters;
import star.chat_types;
import star.socket;
import star.tcp;
import star.liquid_types;
import star.tile_sector_array;
import star.world_layout;
import star.collision_generator;
import star.world_tiles;
import star.celestial_types;
import star.wiring;
import star.plant;
import star.plant_database;
import star.biome_placement;
import star.versioning_database;
import star.world_storage;
import star.sky_parameters;
import star.system_world;
import star.damage_manager;
import star.net_packets;
import star.net_packet_socket;
import star.universe_connection;
import star.sky_render_data;
import star.parallax;
import star.cellular_light_array;
import star.cellular_lighting;
import star.world_render_data;
import star.world_structure;
import star.world_client_state;
import star.interpolation_tracker;
import star.ambient;
import star.weather;
import star.world_client;
import star.world_client_thread;
import star.universe;
// TODO: make this more thread safe
import star.universe_client;
import star.canvas_widget;
import star.label_widget;
import star.image_widget;
import star.image_stretch_widget;
import star.widget_parsing;
import star.gui_reader;
import star.list_widget;
import star.button_group;
import star.button_widget;
import star.tab_set;
import star.progress_widget;
import star.item_slot_widget;
import star.stack_widget;
import star.cinematic;

import star.ai_database;


import star.client_context;

namespace Star {

AiInterface::AiInterface(GuiContext& guiContext, std::shared_ptr<UniverseClient> client, std::shared_ptr<Cinematic> cinematic, Animation staticAnimation, Animation scanlineAnimation, Animation faceAnimation)
  : Pane(guiContext), m_currentPage(AiPages::StatusPage), m_client(std::move(client)), m_cinematic(std::move(cinematic)),
    m_sourceEntityId(NullEntityId), m_staticAnimation(std::move(staticAnimation)), m_scanlineAnimation(std::move(scanlineAnimation)),
    m_faceAnimation("", std::move(faceAnimation)), m_textLength(0), m_textMaxLength(0) {}

std::expected<std::shared_ptr<AiInterface>, StarException> AiInterface::create(GuiContext& guiContext, std::shared_ptr<UniverseClient> client, std::shared_ptr<Cinematic> cinematic) {
  auto database = guiContext.root().aiDatabase();
  if (!database)
    return std::unexpected(std::move(database.error()));
  auto staticAnimation = (*database)->staticAnimation(client->mainPlayer()->shipSpecies());
  if (!staticAnimation)
    return std::unexpected(std::move(staticAnimation.error()));
  auto faceAnimation = Animation::create(guiContext.root());
  if (!faceAnimation)
    return std::unexpected(std::move(faceAnimation.error()));
  auto pane = std::shared_ptr<AiInterface>(new AiInterface(guiContext, std::move(client), std::move(cinematic),
      std::move(*staticAnimation), (*database)->scanlineAnimation(), std::move(*faceAnimation)));
  pane->m_aiDatabase = std::move(*database);
  auto initialized = pane->initializePane();
  if (!initialized)
    return std::unexpected(std::move(initialized.error()));
  initialized = pane->initialize();
  if (!initialized)
    return std::unexpected(std::move(initialized.error()));
  return pane;
}

std::expected<void, StarException> AiInterface::initialize() {
  auto assetsResult = context().root().assets();
  if (!assetsResult)
    return std::unexpected(std::move(assetsResult.error()));
  auto assets = *assetsResult;
  auto config = assets->json("/interface/ai/ai.config");
  if (!config)
    return std::unexpected(std::move(config.error()));
  GuiReader reader(context());
  reader.registerCallback("close", [this](Widget&) -> std::expected<void, StarException> { dismiss(); return {}; });
  reader.registerCallback("missionItemList", [this](Widget&) { return selectMission(); });
  reader.registerCallback("startMission", [this](Widget&) { return startMission(); });
  reader.registerCallback("crewItemList", [this](Widget&) { return selectRecruit(); });
  reader.registerCallback("dismissRecruit", [this](Widget&) { return dismissRecruit(); });
  reader.registerCallback("showMissions", [this](Widget&) { return showMissions(); });
  reader.registerCallback("showCrew", [this](Widget&) { return showCrew(); });
  reader.registerCallback("goBack", [this](Widget&) { return goBack(); });
  auto layout = config->get("guiConfig");
  if (!layout)
    return std::unexpected(std::move(layout.error()));
  auto constructed = reader.construct(*layout, *this);
  if (!constructed)
    return std::unexpected(std::move(constructed.error()));

  auto mainStack = fetchChild<StackWidget>("mainStack");
  if (!mainStack) return std::unexpected(std::move(mainStack.error()));
  m_mainStack = std::move(*mainStack);
  auto missionStack = findChild<StackWidget>("missionStack");
  if (!missionStack) return std::unexpected(std::move(missionStack.error()));
  m_missionStack = std::move(*missionStack);
  auto crewStack = findChild<StackWidget>("crewStack");
  if (!crewStack) return std::unexpected(std::move(crewStack.error()));
  m_crewStack = std::move(*crewStack);
  auto leftPadding = config->getInt("breadcrumbLeftPadding");
  if (!leftPadding) return std::unexpected(std::move(leftPadding.error()));
  m_breadcrumbLeftPadding = *leftPadding;
  auto rightPadding = config->getInt("breadcrumbRightPadding");
  if (!rightPadding) return std::unexpected(std::move(rightPadding.error()));
  m_breadcrumbRightPadding = *rightPadding;
  auto homeBg = fetchChild<ImageStretchWidget>("homeBreadcrumbBg");
  if (!homeBg) return std::unexpected(std::move(homeBg.error()));
  m_homeBreadcrumbBackground = std::move(*homeBg);
  auto pageBg = fetchChild<ImageStretchWidget>("pageBreadcrumbBg");
  if (!pageBg) return std::unexpected(std::move(pageBg.error()));
  m_pageBreadcrumbBackground = std::move(*pageBg);
  auto itemBg = fetchChild<ImageStretchWidget>("itemBreadcrumbBg");
  if (!itemBg) return std::unexpected(std::move(itemBg.error()));
  m_itemBreadcrumbBackground = std::move(*itemBg);
  auto home = fetchChild<LabelWidget>("homeBreadcrumb");
  if (!home) return std::unexpected(std::move(home.error()));
  m_homeBreadcrumbWidget = std::move(*home);
  auto page = fetchChild<LabelWidget>("pageBreadcrumb");
  if (!page) return std::unexpected(std::move(page.error()));
  m_pageBreadcrumbWidget = std::move(*page);
  auto item = fetchChild<LabelWidget>("itemBreadcrumb");
  if (!item) return std::unexpected(std::move(item.error()));
  m_itemBreadcrumbWidget = std::move(*item);
  auto canvas = findChild<CanvasWidget>("aiFaceCanvas");
  if (!canvas) return std::unexpected(std::move(canvas.error()));
  m_aiFaceCanvasWidget = std::move(*canvas);
  auto missions = findChild<ListWidget>("missionItemList");
  if (!missions) return std::unexpected(std::move(missions.error()));
  m_missionListWidget = std::move(*missions);
  auto crew = findChild<ListWidget>("crewItemList");
  if (!crew) return std::unexpected(std::move(crew.error()));
  m_crewListWidget = std::move(*crew);
  auto showMissions = findChild<ButtonWidget>("showMissions");
  if (!showMissions) return std::unexpected(std::move(showMissions.error()));
  m_showMissionsButton = std::move(*showMissions);
  auto showCrew = findChild<ButtonWidget>("showCrew");
  if (!showCrew) return std::unexpected(std::move(showCrew.error()));
  m_showCrewButton = std::move(*showCrew);
  auto back = findChild<ButtonWidget>("backButton");
  if (!back) return std::unexpected(std::move(back.error()));
  m_backButton = std::move(*back);
  auto missionName = findChild<LabelWidget>("missionName");
  if (!missionName) return std::unexpected(std::move(missionName.error()));
  m_missionNameLabel = std::move(*missionName);
  auto missionIcon = findChild<ImageWidget>("missionIcon");
  if (!missionIcon) return std::unexpected(std::move(missionIcon.error()));
  m_missionIcon = std::move(*missionIcon);
  auto start = findChild<ButtonWidget>("startMission");
  if (!start) return std::unexpected(std::move(start.error()));
  m_startMissionButton = std::move(*start);
  auto recruitName = findChild<LabelWidget>("recruitName");
  if (!recruitName) return std::unexpected(std::move(recruitName.error()));
  m_recruitNameLabel = std::move(*recruitName);
  auto recruitIcon = findChild<ImageWidget>("recruitIcon");
  if (!recruitIcon) return std::unexpected(std::move(recruitIcon.error()));
  m_recruitIcon = std::move(*recruitIcon);
  auto dismissRecruit = findChild<ButtonWidget>("dismissRecruit");
  if (!dismissRecruit) return std::unexpected(std::move(dismissRecruit.error()));
  m_dismissRecruitButton = std::move(*dismissRecruit);
  m_species = m_client->mainPlayer()->shipSpecies();
  auto missionBreadcrumb = config->getString("missionBreadcrumbText");
  if (!missionBreadcrumb) return std::unexpected(std::move(missionBreadcrumb.error()));
  m_missionBreadcrumbText = std::move(*missionBreadcrumb);
  auto missionDeploy = config->getString("missionDeployText");
  if (!missionDeploy) return std::unexpected(std::move(missionDeploy.error()));
  m_missionDeployText = std::move(*missionDeploy);
  auto crewBreadcrumb = config->getString("crewBreadcrumbText");
  if (!crewBreadcrumb) return std::unexpected(std::move(crewBreadcrumb.error()));
  m_crewBreadcrumbText = std::move(*crewBreadcrumb);
  auto recruitNameText = config->getString("defaultRecruitName");
  if (!recruitNameText) return std::unexpected(std::move(recruitNameText.error()));
  m_defaultRecruitName = std::move(*recruitNameText);
  auto description = config->getString("defaultRecruitDescription");
  if (!description) return std::unexpected(std::move(description.error()));
  m_defaultRecruitDescription = std::move(*description);
  return {};
}

std::expected<void, StarException> AiInterface::update(float dt) {
  if (!m_client->playerOnOwnShip())
    dismiss();
  auto result = Pane::update(dt);
  if (!result) return std::unexpected(std::move(result.error()));
  result = m_showCrewButton->setVisibility(m_currentPage == AiPages::StatusPage);
  if (!result) return std::unexpected(std::move(result.error()));
  result = m_showMissionsButton->setVisibility(m_currentPage == AiPages::StatusPage);
  if (!result) return std::unexpected(std::move(result.error()));
  result = m_backButton->setVisibility(m_currentPage != AiPages::StatusPage);
  if (!result) return std::unexpected(std::move(result.error()));
  m_staticAnimation.update(dt);
  m_scanlineAnimation.update(dt);

  if (m_currentSpeech) {
    m_textLength += m_currentSpeech->speedModifier * m_aiDatabase->charactersPerSecond() * dt;
    result = m_currentTextWidget->setText(m_currentSpeech->text);
    if (!result) return std::unexpected(std::move(result.error()));
    result = m_currentTextWidget->setTextCharLimit(std::min(m_textMaxLength, std::floor(m_textLength)));
    if (!result) return std::unexpected(std::move(result.error()));
    result = setFaceAnimation(m_textLength < m_textMaxLength ? m_currentSpeech->animation : "idle");
    if (!result) return std::unexpected(std::move(result.error()));
    if (m_textLength < m_textMaxLength) {
      if (!m_chatterSound || m_chatterSound->finished()) {
        auto assets = context().root().assets();
        if (!assets) return std::unexpected(std::move(assets.error()));
        auto soundConfig = (*assets)->json("/interface/ai/ai.config:chatterSound");
        if (!soundConfig) return std::unexpected(std::move(soundConfig.error()));
        auto soundName = soundConfig->toString();
        if (!soundName) return std::unexpected(std::move(soundName.error()));
        auto sound = (*assets)->audio(*soundName);
        if (!sound) return std::unexpected(std::move(sound.error()));
        auto chatter = AudioInstance::create(**sound);
        if (!chatter) return std::unexpected(std::move(chatter.error()));
        m_chatterSound = std::move(*chatter);
        m_chatterSound->setLoops(-1);
        auto played = context().playAudio(m_chatterSound);
        if (!played)
          return std::unexpected(std::move(played.error()));
      }
    } else if (m_chatterSound) {
      m_chatterSound->stop();
    }
    m_faceAnimation.second.update(dt * m_currentSpeech->speedModifier);
  } else {
    result = setFaceAnimation("idle");
    if (!result) return std::unexpected(std::move(result.error()));
    m_faceAnimation.second.update(dt);
    if (m_chatterSound)
      m_chatterSound->stop();
  }

  auto const& aiState = m_client->mainPlayer()->aiState();
  if (!std::ranges::equal(aiState.availableMissions, m_availableMissions)
      || !std::ranges::equal(aiState.completedMissions, m_completedMissions)) {
    m_availableMissions = aiState.availableMissions.values();
    m_completedMissions = aiState.completedMissions.values();
    result = populateMissions();
    if (!result) return std::unexpected(std::move(result.error()));
  }
  auto crew = m_client->mainPlayer()->companions()->getCompanions("crew");
  if (crew != m_crew) {
    m_crew = crew;
    result = populateCrew();
    if (!result) return std::unexpected(std::move(result.error()));
  }
  m_aiFaceCanvasWidget->clear();
  auto face = m_faceAnimation.second.drawable(1.0f);
  if (!face) return std::unexpected(std::move(face.error()));
  m_aiFaceCanvasWidget->drawDrawable(std::move(*face), Vec2F(0, 0));
  auto staticDrawable = m_staticAnimation.drawable(1.0f);
  if (!staticDrawable) return std::unexpected(std::move(staticDrawable.error()));
  m_aiFaceCanvasWidget->drawDrawable(std::move(*staticDrawable), Vec2F(0, 0));
  auto scanline = m_scanlineAnimation.drawable(1.0f);
  if (!scanline) return std::unexpected(std::move(scanline.error()));
  m_aiFaceCanvasWidget->drawDrawable(std::move(*scanline), Vec2F(0, 0));
  return {};
}

std::expected<void, StarException> AiInterface::updateBreadcrumbs() {
  // Home breadcrumb
  auto width = m_homeBreadcrumbWidget->size()[0];
  width += m_breadcrumbLeftPadding + m_breadcrumbRightPadding;
  m_homeBreadcrumbBackground->setSize(Vec2I(width, m_homeBreadcrumbBackground->size()[1]));

  // Middle breadcrumb
  if (m_currentPage != AiPages::StatusPage) {
    Vec2I pagePosition = m_homeBreadcrumbBackground->position() + Vec2I(width, 0);
    m_pageBreadcrumbWidget->setPosition(Vec2I(pagePosition[0] + m_breadcrumbLeftPadding, m_homeBreadcrumbWidget->position()[1]));
    m_pageBreadcrumbBackground->setPosition(pagePosition - Vec2I(m_breadcrumbRightPadding, 0));
    width = m_pageBreadcrumbWidget->size()[0] + (2 * m_breadcrumbRightPadding) + m_breadcrumbLeftPadding; // Add right padding twice because of the overlap to the left
    m_pageBreadcrumbBackground->setSize(Vec2I(width, m_pageBreadcrumbBackground->size()[1]));

    // End breadcrumb
    if (m_currentPage == AiPages::MissionPage || m_currentPage == AiPages::CrewPage) {
      Vec2I itemPosition = m_pageBreadcrumbBackground->position() + Vec2I(width, 0);
      m_itemBreadcrumbWidget->setPosition(Vec2I(itemPosition[0] + m_breadcrumbLeftPadding, m_homeBreadcrumbWidget->position()[1]));
      m_itemBreadcrumbBackground->setPosition(itemPosition - Vec2I(m_breadcrumbRightPadding, 0));
      width = m_itemBreadcrumbWidget->size()[0] + (2 * m_breadcrumbRightPadding) + m_breadcrumbLeftPadding;
      m_itemBreadcrumbBackground->setSize(Vec2I(width, m_itemBreadcrumbBackground->size()[1]));
    }
  }

  // Visibility
  auto result = m_pageBreadcrumbBackground->setVisibility(m_currentPage != AiPages::StatusPage);
  if (!result) return std::unexpected(std::move(result.error()));
  result = m_pageBreadcrumbWidget->setVisibility(m_currentPage != AiPages::StatusPage);
  if (!result) return std::unexpected(std::move(result.error()));
  result = m_itemBreadcrumbBackground->setVisibility(m_currentPage == AiPages::MissionPage || m_currentPage == AiPages::CrewPage);
  if (!result) return std::unexpected(std::move(result.error()));
  return m_itemBreadcrumbWidget->setVisibility(m_currentPage == AiPages::MissionPage || m_currentPage == AiPages::CrewPage);
}

std::expected<void, StarException> AiInterface::displayed() {
  if (!m_client->playerOnOwnShip())
    return {};
  auto result = Pane::displayed();
  if (!result) return std::unexpected(std::move(result.error()));
  return showStatus();
}

std::expected<void, StarException> AiInterface::dismissed() {
  if (m_chatterSound)
    m_chatterSound->stop();
  m_selectedMission = {};
  m_selectedRecruit = {};
  return Pane::dismissed();
}

void AiInterface::setSourceEntityId(EntityId sourceEntityId) {
  m_sourceEntityId = sourceEntityId;
}

std::expected<void, StarException> AiInterface::showStatus() {
  auto speech = m_aiDatabase->shipStatus(m_client->clientContext()->shipUpgrades().shipLevel);
  if (!speech) return std::unexpected(std::move(speech.error()));
  m_currentPage = AiPages::StatusPage;
  auto result = setCurrentSpeech("shipStatusText", std::move(*speech));
  if (!result) return std::unexpected(std::move(result.error()));
  result = m_mainStack->showPage(0);
  if (!result) return std::unexpected(std::move(result.error()));
  return updateBreadcrumbs();
}

std::expected<void, StarException> AiInterface::showMissions() {
  m_currentPage = AiPages::MissionList;
  m_currentSpeech = {};
  auto result = m_pageBreadcrumbWidget->setText(m_missionBreadcrumbText);
  if (!result) return std::unexpected(std::move(result.error()));
  result = populateMissions();
  if (!result) return std::unexpected(std::move(result.error()));
  result = m_missionListWidget->clearSelected();
  if (!result) return std::unexpected(std::move(result.error()));
  result = m_mainStack->showPage(1);
  if (!result) return std::unexpected(std::move(result.error()));
  result = m_missionStack->showPage(0);
  if (!result) return std::unexpected(std::move(result.error()));
  if (m_availableMissions.empty() && m_completedMissions.empty()) {
    result = m_missionStack->showPage(0);
    if (!result) return std::unexpected(std::move(result.error()));
    result = setCurrentSpeech("noMissionsText", m_aiDatabase->noMissionsSpeech());
  } else {
    result = m_missionStack->showPage(1);
  }
  if (!result) return std::unexpected(std::move(result.error()));
  return updateBreadcrumbs();
}

std::expected<void, StarException> AiInterface::selectMission() {
  std::size_t selectedItem = m_missionListWidget->selectedItem();
  m_selectedMission = {};
  if (selectedItem < m_availableMissions.size())
    m_selectedMission = m_availableMissions[selectedItem];
  else if (selectedItem < m_availableMissions.size() + m_completedMissions.size())
    m_selectedMission = m_completedMissions[selectedItem - m_availableMissions.size()];
  if (!m_selectedMission)
    return {};
  auto mission = m_aiDatabase->mission(*m_selectedMission);
  if (!mission) return std::unexpected(std::move(mission.error()));
  auto missionText = mission->speciesText.value(m_species, mission->speciesText.value("default"));
  m_currentPage = AiPages::MissionPage;
  auto result = setCurrentSpeech("missionText", missionText.selectSpeech);
  if (!result) return std::unexpected(std::move(result.error()));
  result = m_missionNameLabel->setText(missionText.buttonText);
  if (!result) return std::unexpected(std::move(result.error()));
  result = m_missionIcon->setImage(mission->icon);
  if (!result) return std::unexpected(std::move(result.error()));
  result = m_itemBreadcrumbWidget->setText(m_missionDeployText);
  if (!result) return std::unexpected(std::move(result.error()));
  result = m_missionStack->showPage(2);
  if (!result) return std::unexpected(std::move(result.error()));
  return updateBreadcrumbs();
}

std::expected<void, StarException> AiInterface::showCrew() {
  m_currentPage = AiPages::CrewList;
  m_currentSpeech = {};
  auto result = m_pageBreadcrumbWidget->setText(m_crewBreadcrumbText);
  if (!result) return std::unexpected(std::move(result.error()));
  result = populateMissions();
  if (!result) return std::unexpected(std::move(result.error()));
  result = m_crewListWidget->clearSelected();
  if (!result) return std::unexpected(std::move(result.error()));
  result = m_mainStack->showPage(2);
  if (!result) return std::unexpected(std::move(result.error()));
  if (m_crew.empty()) {
    result = m_crewStack->showPage(0);
    if (!result) return std::unexpected(std::move(result.error()));
    result = setCurrentSpeech("noCrewText", m_aiDatabase->noCrewSpeech());
  } else {
    result = m_crewStack->showPage(1);
  }
  if (!result) return std::unexpected(std::move(result.error()));
  return updateBreadcrumbs();
}

std::expected<void, StarException> AiInterface::selectRecruit() {
  std::size_t selectedItem = m_crewListWidget->selectedItem();
  m_selectedRecruit = selectedItem < m_crew.size() ? m_crew[selectedItem] : nullptr;
  if (!m_selectedRecruit)
    return {};
  m_currentPage = AiPages::CrewPage;
  auto description = m_selectedRecruit->description();
  if (!description) return std::unexpected(std::move(description.error()));
  auto name = m_selectedRecruit->name();
  if (!name) return std::unexpected(std::move(name.error()));
  auto speech = description->value_or(m_defaultRecruitDescription);
  auto result = setCurrentSpeech(
    "recruitText", {.animation = m_aiDatabase->defaultAnimation(), .text = speech, .speedModifier = 1.0f});
  if (!result) return std::unexpected(std::move(result.error()));
  result = m_recruitNameLabel->setText(name->value_or(m_defaultRecruitName));
  if (!result) return std::unexpected(std::move(result.error()));
  result = m_recruitIcon->setDrawables(m_selectedRecruit->portrait());
  if (!result) return std::unexpected(std::move(result.error()));
  result = m_itemBreadcrumbWidget->setText(name->value_or(m_defaultRecruitName));
  if (!result) return std::unexpected(std::move(result.error()));
  result = m_crewStack->showPage(2);
  if (!result) return std::unexpected(std::move(result.error()));
  return updateBreadcrumbs();
}

std::expected<void, StarException> AiInterface::populateMissions() {
  auto result = m_missionListWidget->clear();
  if (!result) return std::unexpected(std::move(result.error()));
  auto addMission = [this](String const& missionName, bool completed) -> std::expected<void, StarException> {
    auto widget = m_missionListWidget->addItem();
    if (!widget) return std::unexpected(std::move(widget.error()));
    auto label = (*widget)->fetchChild<LabelWidget>("itemName");
    if (!label) return std::unexpected(std::move(label.error()));
    auto icon = (*widget)->fetchChild<ImageWidget>("itemIcon");
    if (!icon) return std::unexpected(std::move(icon.error()));
    auto mission = m_aiDatabase->mission(missionName);
    if (!mission) return std::unexpected(std::move(mission.error()));
    auto image = (*icon)->setImage(mission->icon);
    if (!image) return std::unexpected(std::move(image.error()));
    auto missionText = mission->speciesText.value(m_species, mission->speciesText.value("default"));
    return (*label)->setText(completed ? missionText.repeatButtonText : missionText.buttonText);
  };
  for (auto const& missionName : m_availableMissions) {
    result = addMission(missionName, false);
    if (!result) return std::unexpected(std::move(result.error()));
  }
  for (auto const& missionName : m_completedMissions) {
    result = addMission(missionName, true);
    if (!result) return std::unexpected(std::move(result.error()));
  }
  return m_missionListWidget->setSelected(NPos);
}

std::expected<void, StarException> AiInterface::startMission() {
  if (!m_selectedMission)
    return {};
  auto mission = m_aiDatabase->mission(*m_selectedMission);
  if (!mission) return std::unexpected(std::move(mission.error()));
  auto result = m_client->warpPlayer(
      WarpToWorld{InstanceWorldId(mission->missionUniqueWorld, m_client->teamUuid()), {}},
      true, mission->warpAnimation.value_or("default"), mission->warpDeploy.value_or(false));
  if (!result) return std::unexpected(std::move(result.error()));
  dismiss();
  return {};
}

std::expected<void, StarException> AiInterface::populateCrew() {
  auto result = m_crewListWidget->clear();
  if (!result) return std::unexpected(std::move(result.error()));
  for (auto const& recruit : m_crew) {
    auto widget = m_crewListWidget->addItem();
    if (!widget) return std::unexpected(std::move(widget.error()));
    auto label = (*widget)->fetchChild<LabelWidget>("itemName");
    if (!label) return std::unexpected(std::move(label.error()));
    auto icon = (*widget)->fetchChild<ImageWidget>("itemIcon");
    if (!icon) return std::unexpected(std::move(icon.error()));
    result = (*icon)->setDrawables(recruit->portrait());
    if (!result) return std::unexpected(std::move(result.error()));
    auto name = recruit->name();
    if (!name) return std::unexpected(std::move(name.error()));
    result = (*label)->setText(name->value_or(m_defaultRecruitName));
    if (!result) return std::unexpected(std::move(result.error()));
  }
  return m_crewListWidget->setSelected(NPos);
}

std::expected<void, StarException> AiInterface::dismissRecruit() {
  if (!m_selectedRecruit)
    return {};
  auto podUuid = m_selectedRecruit->podUuid();
  if (!podUuid) return std::unexpected(std::move(podUuid.error()));
  auto result = m_client->mainPlayer()->companions()->dismissCompanion("crew", *podUuid);
  if (!result) return std::unexpected(std::move(result.error()));
  return m_crewStack->showPage(1);
}

std::expected<void, StarException> AiInterface::goBack() {
  std::expected<void, StarException> result;
  if (m_currentPage == AiPages::MissionPage)
    result = showMissions();
  else if (m_currentPage == AiPages::CrewPage)
    result = showCrew();
  else if (m_currentPage == AiPages::MissionList || m_currentPage == AiPages::CrewList)
    result = showStatus();
  if (!result) return std::unexpected(std::move(result.error()));
  return updateBreadcrumbs();
}

std::expected<void, StarException> AiInterface::setFaceAnimation(String const& name) {
  if (m_faceAnimation.first != name) {
    auto animation = m_aiDatabase->animation(m_species, name);
    if (!animation) return std::unexpected(std::move(animation.error()));
    m_faceAnimation = {name, std::move(*animation)};
  }
  return {};
}

std::expected<void, StarException> AiInterface::setCurrentSpeech(String const& textWidget, AiSpeech speech) {
  auto widget = findChild<LabelWidget>(textWidget);
  if (!widget) return std::unexpected(std::move(widget.error()));
  auto length = Text::stripEscapeCodes(speech.text).size();
  if (!length) return std::unexpected(std::move(length.error()));
  auto result = (*widget)->setText("");
  if (!result) return std::unexpected(std::move(result.error()));
  m_currentSpeech = std::move(speech);
  m_textLength = 0;
  m_textMaxLength = *length;
  m_currentTextWidget = std::move(*widget);
  return {};
}

}

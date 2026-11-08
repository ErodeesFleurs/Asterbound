import std;
import star.format;
import star.json;
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
import star.rpc_promise;
import star.strong_typedef;
import star.a_star;
import star.ordered_set;
import star.either;
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


import star.char_selection;
import star.widget_parsing;
import star.gui_reader;
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
import star.button_group;
import star.button_widget;
import star.portrait_widget;
import star.label_widget;
import star.large_char_plate_widget;
import star.text_box_widget;

import star.player_storage;

namespace Star {

CharSelectionPane::CharSelectionPane(GuiContext& guiContext, std::shared_ptr<PlayerStorage> playerStorage,
    CreateCharCallback createCallback, SelectCharacterCallback selectCallback, DeleteCharacterCallback deleteCallback)
  : Pane(guiContext), m_playerStorage(std::move(playerStorage)), m_downScroll(0),
    m_createCallback(std::move(createCallback)), m_selectCallback(std::move(selectCallback)),
    m_deleteCallback(std::move(deleteCallback)) {}

std::expected<std::shared_ptr<CharSelectionPane>, StarException> CharSelectionPane::create(
    GuiContext& guiContext, std::shared_ptr<PlayerStorage> playerStorage, CreateCharCallback createCallback,
    SelectCharacterCallback selectCallback, DeleteCharacterCallback deleteCallback) {
  if (!createCallback || !selectCallback || !deleteCallback)
    return std::unexpected(GuiException("Character selection requires create, select and delete callbacks"));
  auto pane = std::shared_ptr<CharSelectionPane>(new CharSelectionPane(guiContext, std::move(playerStorage),
      std::move(createCallback), std::move(selectCallback), std::move(deleteCallback)));
  if (auto result = pane->initializePane(); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = pane->init(); !result)
    return std::unexpected(std::move(result.error()));
  return pane;
}

std::expected<void, StarException> CharSelectionPane::init() {
  GuiReader guiReader(context());
  guiReader.registerCallback("playerUpButton", [this](Widget&) { return shiftCharacters(-1); });
  guiReader.registerCallback("playerDownButton", [this](Widget&) { return shiftCharacters(1); });
  guiReader.registerCallback("charSelector1", [this](Widget&) { return selectCharacter(0); });
  guiReader.registerCallback("charSelector2", [this](Widget&) { return selectCharacter(1); });
  guiReader.registerCallback("charSelector3", [this](Widget&) { return selectCharacter(2); });
  guiReader.registerCallback("charSelector4", [this](Widget&) { return selectCharacter(3); });
  guiReader.registerCallback("createCharButton", [this](Widget&) { return m_createCallback(); });
  guiReader.registerCallback("searchCharacter", [this](Widget& obj) -> std::expected<void, StarException> {
    m_downScroll = 0;
    auto textBox = rconvert<TextBoxWidget>(obj);
    if (!textBox)
      return std::unexpected(std::move(textBox.error()));
    auto trimmed = textBox->get().getText().trim();
    if (!trimmed)
      return std::unexpected(std::move(trimmed.error()));
    auto lowered = trimmed->to_lower();
    if (!lowered)
      return std::unexpected(std::move(lowered.error()));
    m_search = std::move(*lowered);
    return updateCharacterPlates();
  });
  guiReader.registerCallback("clearSearch", [this](Widget&) -> std::expected<void, StarException> {
    auto search = requireChild<TextBoxWidget>("searchCharacter");
    if (!search)
      return std::unexpected(std::move(search.error()));
    if (auto result = (*search)->setText(""); !result)
      return std::unexpected(std::move(result.error()));
    return {};
  });
  guiReader.registerCallback("toggleDismissCheckbox", [this](Widget& widget) -> std::expected<void, StarException> {
    auto configuration = context().root().configuration();
    if (!configuration)
      return std::unexpected(std::move(configuration.error()));
    auto button = rconvert<ButtonWidget>(widget);
    if (!button)
      return std::unexpected(std::move(button.error()));
    return (*configuration)->set("characterSwapDismisses", button->get().isChecked());
  });
  auto assets = context().root().assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  auto config = (*assets)->json("/interface/windowconfig/charselection.config");
  if (!config)
    return std::unexpected(std::move(config.error()));
  return guiReader.construct(*config, *this);
}

std::expected<bool, StarException> CharSelectionPane::sendEvent(InputEvent const& event) {
  if (m_visible) {
    if (std::holds_alternative<MouseWheelEvent>(event)) {
      if (inMember(*context().mousePosition(event))) {
        if (std::get<MouseWheelEvent>(event).mouseWheel == MouseWheel::Down) {
          if (auto result = shiftCharacters(1); !result)
            return std::unexpected(std::move(result.error()));
        } else if (std::get<MouseWheelEvent>(event).mouseWheel == MouseWheel::Up) {
          if (auto result = shiftCharacters(-1); !result)
            return std::unexpected(std::move(result.error()));
        }
        return true;
      }
    }
  }
  return Pane::sendEvent(event);
}

std::expected<void, StarException> CharSelectionPane::show() {
  if (auto result = Pane::show(); !result)
    return std::unexpected(std::move(result.error()));
  m_downScroll = 0;
  auto search = requireChild<TextBoxWidget>("searchCharacter");
  if (!search)
    return std::unexpected(std::move(search.error()));
  if (auto result = (*search)->setText(""); !result)
    return std::unexpected(std::move(result.error()));
  return updateCharacterPlates();
}

std::expected<void, StarException> CharSelectionPane::shiftCharacters(int shift) {
  m_downScroll = std::max<int>(std::min<int>(m_downScroll + shift, m_filteredList.size() - 3), 0);
  return updateCharacterPlates();
}

std::expected<void, StarException> CharSelectionPane::selectCharacter(unsigned buttonIndex) {
  if (m_downScroll + buttonIndex < m_filteredList.size()) {
    auto playerUuid = m_filteredList[m_downScroll + buttonIndex];
    auto player = m_playerStorage->loadPlayer(playerUuid);
    if (!player)
      return std::unexpected(std::move(player.error()));
    if (!*player)
      return std::unexpected(GuiException("Selected character could not be loaded"));
    if ((*player)->isPermaDead() && !(*player)->isAdmin()) {
      auto assets = context().root().assets();
      if (!assets)
        return std::unexpected(std::move(assets.error()));
      auto soundsConfig = (*assets)->json("/interface.config:buttonClickFailSound");
      if (!soundsConfig)
        return std::unexpected(std::move(soundsConfig.error()));
      auto sounds = soundsConfig->toArray();
      if (!sounds)
        return std::unexpected(std::move(sounds.error()));
      auto sound = context().root().random().randValueFrom(*sounds, "").toString();
      if (!sound)
        return std::unexpected(std::move(sound.error()));
      if (!sound->empty()) {
        if (auto result = context().playAudio(*sound); !result)
          return std::unexpected(std::move(result.error()));
      }
      return {};
    }
    return m_selectCallback(*player);
  }
  return m_createCallback();
}

std::expected<void, StarException> CharSelectionPane::updateCharacterPlates() {
  auto updatePlayerLine = [this](String const& name, unsigned scrollPosition) -> std::expected<void, StarException> {
    auto filtered = m_playerStorage->playerUuidListByName(m_search);
    if (!filtered)
      return std::unexpected(std::move(filtered.error()));
    m_filteredList = std::move(*filtered);
    auto selector = requireChild<LargeCharPlateWidget>(name);
    if (!selector)
      return std::unexpected(std::move(selector.error()));
    if (!m_filteredList.empty() && scrollPosition < m_filteredList.size()) {
      auto playerUuid = m_filteredList[scrollPosition];
      auto player = m_playerStorage->loadPlayer(playerUuid);
      if (!player)
        return std::unexpected(std::move(player.error()));
      if (*player) {
        if (auto result = (*selector)->show(); !result)
          return std::unexpected(std::move(result.error()));
        (*player)->humanoid()->setFacingDirection(Direction::Right);
        if (auto result = (*selector)->setPlayer(*player); !result)
          return std::unexpected(std::move(result.error()));
        if (!m_readOnly) {
          if (auto result = (*selector)->enableDelete([this, playerUuid](Widget&) { return m_deleteCallback(playerUuid); }); !result)
            return std::unexpected(std::move(result.error()));
        }
        return {};
      }
    }
    if (auto result = (*selector)->setPlayer({}); !result)
      return std::unexpected(std::move(result.error()));
    (*selector)->disableDelete();
    if (m_readOnly)
      return (*selector)->hide();
    return {};
  };
  for (unsigned i = 0; i < 4; ++i) {
    if (auto result = updatePlayerLine(strf("charSelector{}", i + 1), m_downScroll + i); !result)
      return std::unexpected(std::move(result.error()));
  }
  auto up = requireChild("playerUpButton");
  if (!up)
    return std::unexpected(std::move(up.error()));
  if (auto result = (*up)->setVisibility(m_downScroll > 0); !result)
    return std::unexpected(std::move(result.error()));
  auto down = requireChild("playerDownButton");
  if (!down)
    return std::unexpected(std::move(down.error()));
  return (*down)->setVisibility(m_downScroll < m_filteredList.size() - 3);
}

std::expected<void, StarException> CharSelectionPane::setReadOnly(bool readOnly) {
  auto button = findChild("createCharButton");
  if (!button)
    return std::unexpected(std::move(button.error()));
  if (!*button)
    return std::unexpected(GuiException("Character selection create button was not found"));
  return (*button)->setVisibility(!(m_readOnly = readOnly));
}

}

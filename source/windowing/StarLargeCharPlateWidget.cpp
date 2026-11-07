import std;
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
import star.button_group;
import star.button_widget;
import star.portrait_widget;
import star.label_widget;
import star.large_char_plate_widget;

import star.format;
namespace Star {

LargeCharPlateWidget::LargeCharPlateWidget(GuiContext& context) : ButtonWidget(context), m_portraitScale(0) {}

std::expected<std::shared_ptr<LargeCharPlateWidget>, StarException> LargeCharPlateWidget::create(GuiContext& context, WidgetCallbackFunc mainCallback, std::shared_ptr<Player> player) {
  auto widget = std::shared_ptr<LargeCharPlateWidget>(new LargeCharPlateWidget(context));
  auto base = widget->initialize();
  if (!base) return std::unexpected(std::move(base.error()));
  widget->m_player = std::move(player);
  widget->setSize(widget->ButtonWidget::size());
  auto assets = context.root().assets();
  if (!assets) return std::unexpected(std::move(assets.error()));
  auto config = (*assets)->json("/interface.config:largeCharPlate");
  if (!config) return std::unexpected(std::move(config.error()));
  widget->m_config = *config;
  auto backing = config->getString("backingImage");
  if (!backing) return std::unexpected(std::move(backing.error()));
  widget->setCallback(std::move(mainCallback));
  auto images = widget->setImages(*backing);
  if (!images) return std::unexpected(std::move(images.error()));
  widget->m_playerPlate = std::move(*backing);
  auto hover = config->getString("playerHover");
  if (!hover) return std::unexpected(std::move(hover.error()));
  widget->m_playerPlateHover = std::move(*hover);
  auto noPlayer = config->getString("noPlayer");
  if (!noPlayer) return std::unexpected(std::move(noPlayer.error()));
  widget->m_noPlayerPlate = std::move(*noPlayer);
  auto noHover = config->getString("noPlayerHover");
  if (!noHover) return std::unexpected(std::move(noHover.error()));
  widget->m_noPlayerPlateHover = std::move(*noHover);
  auto portraitOffsetJson = config->get("portraitOffset");
  if (!portraitOffsetJson) return std::unexpected(std::move(portraitOffsetJson.error()));
  auto portraitOffset = jsonToVec2I(*portraitOffsetJson);
  if (!portraitOffset) return std::unexpected(std::move(portraitOffset.error()));
  widget->m_portraitOffset = *portraitOffset;
  auto scale = config->getFloat("portraitScale");
  if (!scale) return std::unexpected(std::move(scale.error()));
  widget->m_portraitScale = *scale;
  auto portrait = PortraitWidget::create(context);
  if (!portrait) return std::unexpected(std::move(portrait.error()));
  widget->m_portrait = *portrait;
  auto scaled = (*portrait)->setScale(*scale);
  if (!scaled) return std::unexpected(std::move(scaled.error()));
  (*portrait)->setPosition(*portraitOffset);
  (*portrait)->setRenderHumanoid(true);
  auto portraitChild = widget->addChild("portrait", *portrait);
  if (!portraitChild) return std::unexpected(std::move(portraitChild.error()));

  auto regularJson = config->get("textColor");
  if (!regularJson) return std::unexpected(std::move(regularJson.error()));
  auto regular = jsonToVec3B(*regularJson);
  if (!regular) return std::unexpected(std::move(regular.error()));
  widget->m_regularTextColor = Color::rgb(*regular);
  auto disabledJson = config->get("textColorDisabled");
  if (!disabledJson) return std::unexpected(std::move(disabledJson.error()));
  auto disabled = jsonToVec3B(*disabledJson);
  if (!disabled) return std::unexpected(std::move(disabled.error()));
  widget->m_disabledTextColor = Color::rgb(*disabled);
  auto createText = config->getString("noPlayerText");
  if (!createText) return std::unexpected(std::move(createText.error()));
  widget->m_createCharText = std::move(*createText);
  auto createColorJson = config->get("noPlayerTextColor");
  if (!createColorJson) return std::unexpected(std::move(createColorJson.error()));
  auto createColor = jsonToVec3B(*createColorJson);
  if (!createColor) return std::unexpected(std::move(createColor.error()));
  widget->m_createCharTextColor = Color::rgb(*createColor);

  if (!HorizontalAnchorNames) return std::unexpected(HorizontalAnchorNames.error());
  if (!VerticalAnchorNames) return std::unexpected(VerticalAnchorNames.error());
  auto makeLabel = [&](String const& childName, String text, String const& offsetKey, String const& hKey, String const& hDefault, String const& vKey, Vec2I& offset) -> std::expected<std::shared_ptr<LabelWidget>, StarException> {
    auto offsetJson = config->get(offsetKey);
    if (!offsetJson) return std::unexpected(std::move(offsetJson.error()));
    auto parsedOffset = jsonToVec2I(*offsetJson);
    if (!parsedOffset) return std::unexpected(std::move(parsedOffset.error()));
    offset = *parsedOffset;
    auto hName = config->getString(hKey, hDefault);
    if (!hName) return std::unexpected(std::move(hName.error()));
    auto h = HorizontalAnchorNames->get_left(*hName);
    if (!h) return std::unexpected(std::move(h.error()));
    auto vName = config->getString(vKey, "bottom");
    if (!vName) return std::unexpected(std::move(vName.error()));
    auto v = VerticalAnchorNames->get_left(*vName);
    if (!v) return std::unexpected(std::move(v.error()));
    auto label = LabelWidget::create(context, std::move(text), Color::White, h->get(), v->get());
    if (!label) return std::unexpected(std::move(label.error()));
    (*label)->setPosition(offset);
    auto child = widget->addChild(childName, *label);
    if (!child) return std::unexpected(std::move(child.error()));
    return *label;
  };
  auto modeText = config->getString("modeText");
  if (!modeText) return std::unexpected(std::move(modeText.error()));
  auto modeName = makeLabel("modeName", std::move(*modeText), "modeNameOffset", "modeNameHAnchor", "mid", "modeNameVAnchor", widget->m_modeNameOffset);
  if (!modeName) return std::unexpected(std::move(modeName.error()));
  widget->m_modeName = *modeName;
  auto mode = makeLabel("mode", {}, "modeOffset", "modeHAnchor", "left", "modeVAnchor", widget->m_modeOffset);
  if (!mode) return std::unexpected(std::move(mode.error()));
  widget->m_mode = *mode;
  auto name = makeLabel("player", {}, "playerNameOffset", "playerNameHAnchor", "mid", "playerNameVAnchor", widget->m_playerNameOffset);
  if (!name) return std::unexpected(std::move(name.error()));
  widget->m_playerName = *name;
  (*name)->setColor(widget->m_createCharTextColor);
  return widget;
}

std::expected<void, StarException> LargeCharPlateWidget::renderImpl() {
  Vec2I pressedOffset = isPressed() ? ButtonWidget::pressedOffset() : Vec2I(0, 0);

  m_portrait->setPosition(m_portraitOffset + pressedOffset);
  m_mode->setPosition(m_modeOffset + pressedOffset);
  m_modeName->setPosition(m_modeNameOffset + pressedOffset);
  m_playerName->setPosition(m_playerNameOffset + pressedOffset);
  if (m_delete) {
    m_delete->setPosition(m_deleteOffset + pressedOffset);
  }

  if (m_player) {
    auto images = ButtonWidget::setImages(m_playerPlate, m_playerPlateHover);
    if (!images) return std::unexpected(std::move(images.error()));
    auto rendered = ButtonWidget::renderImpl();
    if (!rendered) return std::unexpected(std::move(rendered.error()));
    m_modeName->setColor(m_regularTextColor);
    m_playerName->setColor(m_regularTextColor);
    auto name = m_player->name();
    if (!name) return std::unexpected(std::move(name.error()));
    return m_playerName->setText(*name);
  } else {
    auto images = ButtonWidget::setImages(m_noPlayerPlate, m_noPlayerPlateHover);
    if (!images) return std::unexpected(std::move(images.error()));
    ButtonWidget::enable();
    auto rendered = ButtonWidget::renderImpl();
    if (!rendered) return std::unexpected(std::move(rendered.error()));
    m_modeName->setColor(m_disabledTextColor);
    m_playerName->setColor(m_createCharTextColor);
    return m_playerName->setText(m_createCharText);
  }
}

std::expected<void, StarException> LargeCharPlateWidget::mouseOut() {
  if (m_delete) {
    auto result = m_delete->mouseOut();
    if (!result) return std::unexpected(std::move(result.error()));
  }
  return ButtonWidget::mouseOut();
}

std::expected<void, StarException> LargeCharPlateWidget::setPlayer(std::shared_ptr<Player> player) {
  m_player = std::move(player);
  auto portrait = m_portrait->setEntity(m_player);
  if (!portrait) return std::unexpected(std::move(portrait.error()));
  String name = m_createCharText;
  if (m_player) {
    auto playerName = m_player->name();
    if (!playerName) return std::unexpected(std::move(playerName.error()));
    name = std::move(*playerName);
  }
  auto text = m_playerName->setText(std::move(name));
  if (!text) return std::unexpected(std::move(text.error()));
  auto assets = context().root().assets();
  if (!assets) return std::unexpected(std::move(assets.error()));
  auto modesJson = (*assets)->json("/interface.config:modeTypeTextAndColor");
  if (!modesJson) return std::unexpected(std::move(modesJson.error()));
  auto modes = modesJson->toArray();
  if (!modes) return std::unexpected(std::move(modes.error()));
  auto modeJson = modes->at(m_player ? 1 + static_cast<int>(m_player->modeType()) : 0);
  if (!modeJson) return std::unexpected(std::move(modeJson.error()));
  auto mode = modeJson->get().toArray();
  if (!mode) return std::unexpected(std::move(mode.error()));
  auto modeTextJson = mode->at(0);
  if (!modeTextJson) return std::unexpected(std::move(modeTextJson.error()));
  auto modeText = modeTextJson->get().toString();
  if (!modeText) return std::unexpected(std::move(modeText.error()));
  auto modeColorJson = mode->at(1);
  if (!modeColorJson) return std::unexpected(std::move(modeColorJson.error()));
  auto modeColor = jsonToVec3B(modeColorJson->get());
  if (!modeColor) return std::unexpected(std::move(modeColor.error()));
  auto result = m_mode->setText(*modeText);
  if (!result) return std::unexpected(std::move(result.error()));
  m_mode->setColor(Color::rgb(*modeColor));
  return {};
}

std::expected<void, StarException> LargeCharPlateWidget::enableDelete(WidgetCallbackFunc const& callback) {
  disableDelete();
  auto config = m_config.get("trashButton");
  if (!config) return std::unexpected(std::move(config.error()));
  auto base = config->getString("baseImage");
  if (!base) return std::unexpected(std::move(base.error()));
  auto hover = config->getString("hoverImage");
  if (!hover) return std::unexpected(std::move(hover.error()));
  auto pressed = config->getString("pressedImage");
  if (!pressed) return std::unexpected(std::move(pressed.error()));
  auto disabled = config->getString("disabledImage");
  if (!disabled) return std::unexpected(std::move(disabled.error()));
  auto offsetJson = config->get("offset");
  if (!offsetJson) return std::unexpected(std::move(offsetJson.error()));
  auto offset = jsonToVec2I(*offsetJson);
  if (!offset) return std::unexpected(std::move(offset.error()));
  auto button = ButtonWidget::create(context(), callback, *base, *hover, *pressed, *disabled);
  if (!button) return std::unexpected(std::move(button.error()));
  auto child = addChild("trashButton", *button);
  if (!child) return std::unexpected(std::move(child.error()));
  m_delete = std::move(*button);
  m_delete->setPosition(*offset);
  m_deleteOffset = *offset;
  return {};
}

void LargeCharPlateWidget::disableDelete() {
  if (m_delete) {
    removeChild(*m_delete);
  }

  m_delete = {};
}

std::expected<bool, StarException> LargeCharPlateWidget::sendEvent(InputEvent const& event) {
  if (std::holds_alternative<MouseMoveEvent>(event) && m_delete) {
    auto result = m_delete->inMember(*m_context.mousePosition(event)) ? m_delete->mouseOver() : m_delete->mouseOut();
    if (!result) return std::unexpected(std::move(result.error()));
  }
  auto handled = Widget::sendEvent(event);
  if (!handled) return std::unexpected(std::move(handled.error()));
  if (*handled) return true;
  return ButtonWidget::sendEvent(event);
}

std::expected<void, StarException> LargeCharPlateWidget::update(float dt) {
  auto result = ButtonWidget::update(dt);
  if (!result) return std::unexpected(std::move(result.error()));
  if (!m_player) return {};
  auto animate = m_config.getBool("animatePortrait", true);
  if (!animate) return std::unexpected(std::move(animate.error()));
  if (!*animate) return {};
  auto humanoid = m_player->humanoid();
  if (m_delete && m_delete->isHovered()) {
    result = humanoid->setEmoteState(HumanoidEmote::Sad);
    if (!result) return std::unexpected(std::move(result.error()));
    result = humanoid->setState(Humanoid::Run);
    if (!result) return std::unexpected(std::move(result.error()));
  } else {
    result = humanoid->setEmoteState(HumanoidEmote::Idle);
    if (!result) return std::unexpected(std::move(result.error()));
    result = humanoid->setState(isHovered() ? Humanoid::Walk : Humanoid::Idle);
    if (!result) return std::unexpected(std::move(result.error()));
  }
  return humanoid->animate(dt, {});
}

}

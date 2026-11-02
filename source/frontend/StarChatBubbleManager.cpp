import std;
import star.format;
import star.xx_hash;
import star.json;
import star.json_extra;
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
import star.game_types;
import star.color;
import star.font;
import star.bi_map;
import star.string;
import star.vector;
import star.text;
import star.format;
import star.version;
import star.casting;
import star.strong_typedef;
import star.listener;
import star.input_event;
import star.set;
import star.audio;
import star.map;
import star.ordered_map;
import star.io_device;
import star.ref_ptr;
import star.logging;
import star.image;
import star.math_common;
import star.either;
import star.random;
import star.weighted_pool;
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

import star.world_geometry;

import star.chat_action;
import star.renderer;
import star.font_texture_group;
import star.anchor_types;
import star.asset_source;
import star.assets;
import star.root_base;
import star.root;
import star.text_painter;



import star.chat_bubble_separation;

import star.world_camera;


import star.chat_bubble_manager;
import star.configuration;
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
import star.chatty_entity;
import star.asset_texture_group;
import star.application;
import star.statistics_service;
import star.p2p_networking_service;
import star.user_generated_content_service;
import star.desktop_service;
import star.application_controller;
import star.drawable;
import star.drawable_painter;
import star.gui_types;
import star.key_bindings;
import star.mixer;
import star.gui_context;

import star.stored_functions;

import star.image_metadata_database;

import star.algorithm;
namespace Star {
ChatBubbleManager::ChatBubbleManager(GuiContext& context)
  : m_guiContext(context), m_textTemplate(Vec2F()), m_portraitTextTemplate(Vec2F()) {}

std::expected<std::unique_ptr<ChatBubbleManager>, StarException> ChatBubbleManager::create(GuiContext& context) {
  auto manager = std::unique_ptr<ChatBubbleManager>(new ChatBubbleManager(context));
  auto initialized = manager->initialize();
  if (!initialized) return std::unexpected(std::move(initialized.error()));
  return manager;
}

std::expected<void, StarException> ChatBubbleManager::initialize() {
  auto& context = m_guiContext.get();
  auto assets = context.root().assets();
  if (!assets) return std::unexpected(std::move(assets.error()));
  auto config = (*assets)->json("/interface/windowconfig/chatbubbles.config");
  if (!config) return std::unexpected(std::move(config.error()));
  m_cachedInterfaceScale = context.interfaceScale();
  auto colorJson = config->get("textColor");
  if (!colorJson) return std::unexpected(std::move(colorJson.error()));
  auto color = jsonToColor(*colorJson);
  if (!color) return std::unexpected(std::move(color.error()));
  m_textStyle.color = color->toRgba();
  auto styleJson = config->get("textStyle");
  if (!styleJson) return std::unexpected(std::move(styleJson.error()));
  auto style = m_textStyle.loadJson(*styleJson);
  if (!style) return std::unexpected(std::move(style.error()));
  {
    auto json = config->get("textPadding");
    if (!json) return std::unexpected(std::move(json.error()));
    auto value = jsonToVec2F(*json);
    if (!value) return std::unexpected(std::move(value.error()));
    m_textPadding = *value;
  }
  {
    auto json = config->get("bubbleOffset");
    if (!json) return std::unexpected(std::move(json.error()));
    auto value = jsonToVec2F(*json);
    if (!value) return std::unexpected(std::move(value.error()));
    m_bubbleOffset = *value;
  }
  {
    auto json = config->get("portraitMorePosition");
    if (!json) return std::unexpected(std::move(json.error()));
    auto value = jsonToVec2I(*json);
    if (!value) return std::unexpected(std::move(value.error()));
    m_portraitMorePosition = *value;
  }
  {
    auto json = config->get("portraitBackgroundSize");
    if (!json) return std::unexpected(std::move(json.error()));
    auto value = jsonToVec2I(*json);
    if (!value) return std::unexpected(std::move(value.error()));
    m_portraitBackgroundSize = *value;
  }
  {
    auto json = config->get("portraitPosition");
    if (!json) return std::unexpected(std::move(json.error()));
    auto value = jsonToVec2I(*json);
    if (!value) return std::unexpected(std::move(value.error()));
    m_portraitPosition = *value;
  }
  {
    auto json = config->get("portraitSize");
    if (!json) return std::unexpected(std::move(json.error()));
    auto value = jsonToVec2I(*json);
    if (!value) return std::unexpected(std::move(value.error()));
    m_portraitSize = *value;
  }
  {
    auto json = config->get("portraitTextPosition");
    if (!json) return std::unexpected(std::move(json.error()));
    auto value = jsonToVec2I(*json);
    if (!value) return std::unexpected(std::move(value.error()));
    m_portraitTextPosition = *value;
  }
  {
    auto value = config->getFloat("partSize");
    if (!value) return std::unexpected(std::move(value.error()));
    m_partSize = *value;
  }
  {
    auto value = config->getFloat("maxAge");
    if (!value) return std::unexpected(std::move(value.error()));
    m_maxAge = std::move(*value);
  }
  {
    auto value = config->getFloat("portraitMaxAge");
    if (!value) return std::unexpected(std::move(value.error()));
    m_portraitMaxAge = std::move(*value);
  }
  {
    auto value = config->getFloat("interBubbleMargin");
    if (!value) return std::unexpected(std::move(value.error()));
    m_interBubbleMargin = std::move(*value);
  }
  {
    auto value = config->getInt("maxMessagePerEntity");
    if (!value) return std::unexpected(std::move(value.error()));
    m_maxMessagePerEntity = std::move(*value);
  }
  {
    auto value = config->getString("portraitBackgroundImage");
    if (!value) return std::unexpected(std::move(value.error()));
    m_portraitBackgroundImage = std::move(*value);
  }
  {
    auto value = config->getString("portraitMoreImage");
    if (!value) return std::unexpected(std::move(value.error()));
    m_portraitMoreImage = std::move(*value);
  }
  {
    auto value = config->getUInt("portraitTextWidth");
    if (!value) return std::unexpected(std::move(value.error()));
    m_portraitTextWidth = std::move(*value);
  }
  {
    auto value = config->getFloat("portraitChatterFramerate");
    if (!value) return std::unexpected(std::move(value.error()));
    m_portraitChatterFramerate = std::move(*value);
  }
  {
    auto value = config->getFloat("portraitChatterDuration");
    if (!value) return std::unexpected(std::move(value.error()));
    m_portraitChatterDuration = std::move(*value);
  }
  {
    auto value = config->getFloat("furthestTextDistance");
    if (!value) return std::unexpected(std::move(value.error()));
    m_furthestVisibleTextDistance = std::move(*value);
  }
  auto wrap = config->getUInt("textWrapWidth");
  if (!wrap) return std::unexpected(std::move(wrap.error()));
  m_textTemplate = TextPositioning{Vec2F(), HorizontalAnchor::HMidAnchor, VerticalAnchor::TopAnchor, *wrap};
  m_portraitTextTemplate = TextPositioning{Vec2F(m_portraitTextPosition), HorizontalAnchor::LeftAnchor,
                                           VerticalAnchor::TopAnchor, m_portraitTextWidth};
  {
    auto value = config->getFloat("tweenFactor");
    if (!value) return std::unexpected(std::move(value.error()));
    m_bubbles.setTweenFactor(*value);
  }
  {
    auto value = config->getFloat("movementThreshold");
    if (!value) return std::unexpected(std::move(value.error()));
    m_bubbles.setMovementThreshold(*value);
  }
  auto functions = context.root().functionDatabase();
  if (!functions) return std::unexpected(std::move(functions.error()));
  {
    auto name = config->getString("textFadeFunction");
    if (!name) return std::unexpected(std::move(name.error()));
    auto value = (*functions)->function(*name);
    if (!value) return std::unexpected(std::move(value.error()));
    m_textFadeFunction = std::move(*value);
  }
  {
    auto name = config->getString("bubbleFadeFunction");
    if (!name) return std::unexpected(std::move(name.error()));
    auto value = (*functions)->function(*name);
    if (!value) return std::unexpected(std::move(value.error()));
    m_bubbleFadeFunction = std::move(*value);
  }
  return {};
}

std::expected<void, StarException> ChatBubbleManager::setCamera(WorldCamera const& camera) {
  float oldPixelRatio = m_camera.pixelRatio();
  float oldInterfaceScale = m_cachedInterfaceScale;
  m_camera = camera;
  m_cachedInterfaceScale = m_guiContext.get().interfaceScale();
  if (m_camera.pixelRatio() != oldPixelRatio || m_cachedInterfaceScale != oldInterfaceScale) {
    List<ChatAction> actions;
    auto collected = m_bubbles.forEach([&actions](BubbleState<Bubble> const& state, Bubble& bubble) -> std::expected<void, StarException> {
        actions.append(SayChatAction{bubble.entity, bubble.text, state.idealDestination, bubble.config});
        return {};
      });
    if (!collected) return std::unexpected(std::move(collected.error()));
    m_bubbles.clear();
    for (auto& portraitBubble : m_portraitBubbles)
      actions.append(PortraitChatAction{
          portraitBubble.entity,
          portraitBubble.portrait,
          portraitBubble.text,
          portraitBubble.position,
          portraitBubble.config
        });
    m_portraitBubbles.clear();
    return addChatActions(std::move(actions), true);
  }
  return {};
}

std::expected<void, StarException> ChatBubbleManager::update(float dt, std::shared_ptr<WorldClient> world) {
  auto updated = m_bubbles.forEach([this, dt, &world](BubbleState<Bubble>& bubbleState, Bubble& bubble) -> std::expected<void, StarException> {
    bubble.age += dt;
    if (auto entity = world->get<ChattyEntity>(bubble.entity)) {
      auto onscreen = m_camera.worldGeometry().rectIntersectsRect(m_camera.worldScreenRect(), entity->metaBoundBox().translated(entity->position()));
      if (!onscreen) return std::unexpected(std::move(onscreen.error()));
      bubble.onscreen = *onscreen;
      auto mouth = entity->mouthPosition();
      if (!mouth) return std::unexpected(std::move(mouth.error()));
      bubbleState.idealDestination = m_camera.worldToScreen(*mouth + m_bubbleOffset);
    }
    return {};
  });
  if (!updated) return std::unexpected(std::move(updated.error()));

  for (auto& portraitBubble : m_portraitBubbles) {
    portraitBubble.age += dt;
    if (auto entity = world->entity(portraitBubble.entity)) {
      auto onscreen = m_camera.worldGeometry().rectIntersectsRect(m_camera.worldScreenRect(), entity->metaBoundBox().translated(entity->position()));
      if (!onscreen) return std::unexpected(std::move(onscreen.error()));
      portraitBubble.onscreen = *onscreen;
      if (auto chatter = as<ChattyEntity>(entity)) {
        auto mouth = chatter->mouthPosition();
        if (!mouth) return std::unexpected(std::move(mouth.error()));
        portraitBubble.position = *mouth;
      } else
        portraitBubble.position = entity->position();
    }
  }

  auto& count = m_entityBubbleCounts;
  count.clear();
  filter(m_portraitBubbles, [&](PortraitBubble const& portraitBubble) -> bool {
      count[portraitBubble.entity] += m_maxMessagePerEntity;
      if (count[portraitBubble.entity] > m_maxMessagePerEntity)
        return false;
      if (world->get<ChattyEntity>(portraitBubble.entity))
        return portraitBubble.age < m_portraitMaxAge;
      return false;
    });

  m_bubbles.filter([&](BubbleState<Bubble> const&, Bubble const& bubble) -> bool {
      if (++count[bubble.entity] > m_maxMessagePerEntity)
        return false;
      if (world->get<ChattyEntity>(bubble.entity))
        return bubble.age < m_maxAge;
      return false;
    });

  m_bubbles.update(dt);
  return {};
}

std::expected<std::uint8_t, StarException> ChatBubbleManager::calcDistanceFadeAlpha(Vec2F bubbleScreenPosition, std::shared_ptr<StoredFunction> fadeFunction) const {
  // first calculate bubble position as a factor, distance from center to edge
  // of screen (0.0-1.0)
  float halfScreenwidth = m_camera.screenSize()[0] * 0.5f;
  float distanceFactor = (std::fabsf(bubbleScreenPosition[0] - halfScreenwidth)) / halfScreenwidth;

  // that distance factor is divided by the max allowable distance
  // to re-space the distance as a 0 - 1 over the max allowable distance
  distanceFactor = std::clamp(distanceFactor / m_furthestVisibleTextDistance, 0.0f, 1.0f);

  auto evaluated = fadeFunction->evaluate(distanceFactor);
  if (!evaluated) return std::unexpected(std::move(evaluated.error()));
  return std::clamp((int)*evaluated, 0, 255);
}
std::expected<void, StarException> ChatBubbleManager::render() {
  if (m_bubbles.empty() && m_portraitBubbles.empty())
    return {};
  auto configuration = m_guiContext.get().root().configuration();
  if (!configuration) return std::unexpected(std::move(configuration.error()));
  auto setting = (*configuration)->get("speechBubbles");
  if (!setting) return std::unexpected(std::move(setting.error()));
  auto enabled = setting->toBool();
  if (!enabled) return std::unexpected(std::move(enabled.error()));
  if (!*enabled) return {};

  float pixelRatio = m_guiContext.get().interfaceScale();

  auto rendered = m_bubbles.forEach([this, pixelRatio](BubbleState<Bubble> const& state, Bubble& bubble) -> std::expected<void, StarException> {
      if (bubble.onscreen) {
        auto fade = calcDistanceFadeAlpha(state.currentPosition, m_bubbleFadeFunction);
        if (!fade) return std::unexpected(std::move(fade.error()));
        int alpha = *fade;
        if (alpha) {
          for (auto const& bubbleImage : bubble.backgroundImages)
            { auto drawn = drawBubbleImage(state.currentPosition, bubbleImage, pixelRatio, alpha); if (!drawn) return std::unexpected(std::move(drawn.error())); }
          for (auto const& bubbleText : bubble.bubbleText)
            { auto drawn = drawBubbleText(state.currentPosition, bubbleText, pixelRatio, alpha, false); if (!drawn) return std::unexpected(std::move(drawn.error())); }
        }
      }
      return {};
    });
  if (!rendered) return std::unexpected(std::move(rendered.error()));

  for (auto portraitBubble : m_portraitBubbles) {
    if (portraitBubble.onscreen) {
      Vec2F screenPos = m_camera.worldToScreen(portraitBubble.position + m_bubbleOffset);
      int frame = 0;
      if (portraitBubble.age <= m_portraitChatterDuration)
        frame = int((portraitBubble.age / m_portraitChatterFramerate) * 2) % 2;
      // 255 here because portrait bubbles are always full opacity
      for (auto const& bubbleImage : portraitBubble.backgroundImages) {
        auto frameImage = std::get<0>(bubbleImage).replace("<frame>", toString(frame));
        if (!frameImage) return std::unexpected(std::move(frameImage.error()));
        auto drawn = drawBubbleImage(screenPos, std::make_tuple(std::move(*frameImage), std::get<1>(bubbleImage)), pixelRatio, 255);
        if (!drawn) return std::unexpected(std::move(drawn.error()));
      }
      // 255 here because portrait bubbles are always full opacity
      for (auto const& bubbleText : portraitBubble.bubbleText)
        { auto drawn = drawBubbleText(screenPos, bubbleText, pixelRatio, 255, true); if (!drawn) return std::unexpected(std::move(drawn.error())); }
    }
  }
  return {};
}

std::expected<void, StarException> ChatBubbleManager::addChatActions(List<ChatAction> chatActions, bool silent) {
  float partSize = m_partSize;
  float pixelRatio = m_guiContext.get().interfaceScale();

  for (auto const& action : chatActions) {
    Json config = JsonObject{};
    Vec2F position;

    if (std::holds_alternative<SayChatAction>(action)) {
      auto const& sayAction = std::get<SayChatAction>(action);
      auto actionConfig = sayAction.config.optObject();
      if (!actionConfig) return std::unexpected(std::move(actionConfig.error()));
      config = actionConfig->value_or(JsonObject{});
      position = sayAction.position;

      TextStyle textStyle = m_textStyle;
      auto fontSize = config.getUInt("fontSize", textStyle.fontSize);
      if (!fontSize)
        return std::unexpected(std::move(fontSize.error()));
      textStyle.fontSize = *fontSize;
      auto jColor = config.opt("color");
      if (!jColor)
        return std::unexpected(std::move(jColor.error()));
      if (*jColor) {
        auto color = jsonToColor(**jColor);
        if (!color)
          return std::unexpected(std::move(color.error()));
        textStyle.color = color->toRgba();
      }
      auto styleJson = config.get("style", Json());
      if (!styleJson)
        return std::unexpected(std::move(styleJson.error()));
      auto loadedStyle = textStyle.loadJson(*styleJson);
      if (!loadedStyle)
        return std::unexpected(std::move(loadedStyle.error()));
      // Size the bubble using the same final style and wrap width as drawing.
      auto styled = m_guiContext.get().setTextStyle(textStyle, pixelRatio);
      if (!styled) return std::unexpected(std::move(styled.error()));
      TextPositioning tp = scaledPositioning(m_textTemplate, pixelRatio);
      auto result = m_guiContext.get().determineTextSize(sayAction.text, tp);
      if (!result) return std::unexpected(std::move(result.error()));
      float textWidth = result->width() / pixelRatio + m_textPadding[0];
      float textHeight = result->height() / pixelRatio + m_textPadding[1];

      Vec2I innerTiles = Vec2I::ceil(Vec2F((textWidth + 4) / partSize, (textHeight + 4) / partSize));
      if (innerTiles[0] % 2 == 0)
        innerTiles[0] += 1;
      if (innerTiles[0] < 3)
        innerTiles[0] = 3;
      int middleIdx = (innerTiles[0] - 1) / 2;

      List<BubbleImage> backgroundImages;
      auto border = config.getBool("drawBorder", true);
      if (!border) return std::unexpected(std::move(border.error()));
      if (*border) {
        for (int y = 0; y < innerTiles[1]; y++) {
          for (int x = 0; x < innerTiles[0]; x++) {
            auto partPosition = [partSize](int x, int y) {
              return Vec2F(x * partSize, y * partSize);
            };
            if (y == 0) {
              if (x == 0) {
                backgroundImages.append(BubbleImage{"/interface/chatbubbles/cornerBottomLeft.png", partPosition(x, y)});
              } else if (x == innerTiles[0] - 1) {
                backgroundImages.append(BubbleImage{"/interface/chatbubbles/cornerBottomRight.png", partPosition(x, y)});
              } else {
                if (middleIdx == x)
                  backgroundImages.append(BubbleImage{"/interface/chatbubbles/point.png", partPosition(x, y - 1)});
                else
                  backgroundImages.append(BubbleImage{"/interface/chatbubbles/sideDown.png", partPosition(x, y)});
              }
            } else if (y == innerTiles[1] - 1) {
              if (x == 0)
                backgroundImages.append(BubbleImage{"/interface/chatbubbles/cornerTopLeft.png", partPosition(x, y)});
              else if (x == innerTiles[0] - 1)
                backgroundImages.append(BubbleImage{"/interface/chatbubbles/cornerTopRight.png", partPosition(x, y)});
              else
                backgroundImages.append(BubbleImage{"/interface/chatbubbles/sideUp.png", partPosition(x, y)});
            } else {
              if (x == 0)
                backgroundImages.append(BubbleImage{"/interface/chatbubbles/sideLeft.png", partPosition(x, y)});
              else if (x == innerTiles[0] - 1)
                backgroundImages.append(BubbleImage{"/interface/chatbubbles/sideRight.png", partPosition(x, y)});
              else
                backgroundImages.append(BubbleImage{"/interface/chatbubbles/center.png", partPosition(x, y)});
            }
          }
        }
      }

      float textMultiLineShift = textHeight;
      float horizontalCenter = partSize * innerTiles[0] * 0.5f;
      float verticalShift = (partSize * innerTiles[1] - textMultiLineShift) * 0.5f + textMultiLineShift - 1.0f;
      Vec2F position = Vec2F(horizontalCenter, verticalShift);
      List<BubbleText> bubbleTexts;

      bubbleTexts.append(std::make_tuple(sayAction.text, textStyle, true, position));

      for (auto& backgroundImage : backgroundImages)
        std::get<1>(backgroundImage) += Vec2F(-horizontalCenter, partSize);
      for (auto& bubbleText : bubbleTexts)
        std::get<3>(bubbleText) += Vec2F(-horizontalCenter, partSize);

      auto pos = m_camera.worldToScreen(sayAction.position + m_bubbleOffset);
      RectF boundBox = RectF::null();
      for (auto const& image : backgroundImages) {
        auto bounds = bubbleImageRect(pos, image, pixelRatio);
        if (!bounds) return std::unexpected(std::move(bounds.error()));
        boundBox.combine(*bounds);
      }
      Bubble bubble = {sayAction.entity, sayAction.text, sayAction.config, 0, std::move(backgroundImages), std::move(bubbleTexts), false};
      List<BubbleState<Bubble>> oldBubbles = m_bubbles.filtered([&sayAction](BubbleState<Bubble> const&, Bubble const& bubble) {
          return bubble.entity == sayAction.entity;
        });
      m_bubbles.filter([&sayAction](BubbleState<Bubble> const&, Bubble const& bubble) { return bubble.entity != sayAction.entity; });
      m_bubbles.addBubble(pos, boundBox, std::move(bubble), m_interBubbleMargin * pixelRatio);
      oldBubbles.sort([](BubbleState<Bubble> const& a, BubbleState<Bubble> const& b) { return a.contents.age < b.contents.age; });
      for (auto& bubble : oldBubbles.slice(0, m_maxMessagePerEntity - 1))
        m_bubbles.addBubble(bubble.idealDestination, bubble.boundBox, bubble.contents, 0);

    } else if (std::holds_alternative<PortraitChatAction>(action)) {
      auto const& portraitAction = std::get<PortraitChatAction>(action);
      auto actionConfig = portraitAction.config.optObject();
      if (!actionConfig) return std::unexpected(std::move(actionConfig.error()));
      config = actionConfig->value_or(JsonObject{});
      position = portraitAction.position;

      List<BubbleImage> backgroundImages;
      backgroundImages.append(std::make_tuple(m_portraitBackgroundImage, Vec2F()));
      auto more = config.getBool("drawMoreIndicator", false);
      if (!more) return std::unexpected(std::move(more.error()));
      if (*more)
        backgroundImages.append(std::make_tuple(m_portraitMoreImage, Vec2F(m_portraitMorePosition)));
      backgroundImages.append(std::make_tuple(portraitAction.portrait, Vec2F(m_portraitPosition)));
      List<BubbleText> bubbleTexts;
      bubbleTexts.append(std::make_tuple(portraitAction.text, m_textStyle, false, Vec2F(m_portraitTextPosition)));

      for (auto& backgroundImage : backgroundImages)
        std::get<1>(backgroundImage) += Vec2F(-m_portraitBackgroundSize[0] / 2, 0);
      for (auto& bubbleText : bubbleTexts)
        std::get<3>(bubbleText) += Vec2F(-m_portraitBackgroundSize[0] / 2, 0);

      m_portraitBubbles.prepend({
          portraitAction.entity,
          portraitAction.portrait,
          portraitAction.text,
          portraitAction.position,
          portraitAction.config,
          0,
          std::move(backgroundImages),
          std::move(bubbleTexts),
          false
        });
    }

    if (!silent) {
      auto sound = config.optString("sound");
      if (!sound) return std::unexpected(std::move(sound.error()));
      if (*sound) {
        auto assets = m_guiContext.get().root().assets();
        if (!assets)
          return std::unexpected(std::move(assets.error()));
        auto audio = (*assets)->audio(**sound);
        if (!audio) return std::unexpected(std::move(audio.error()));
        auto instance = AudioInstance::create(**audio);
        if (!instance) return std::unexpected(std::move(instance.error()));
        auto audioInstance = std::move(*instance);
        audioInstance->setPosition(position);
        auto volume = config.getFloat("volume", 1.0f);
        if (!volume) return std::unexpected(std::move(volume.error()));
        auto pitch = config.getFloat("pitch", 1.0f);
        if (!pitch) return std::unexpected(std::move(pitch.error()));
        audioInstance->setVolume(*volume);
        audioInstance->setPitchMultiplier(*pitch);
        auto played = m_guiContext.get().playAudio(audioInstance);
        if (!played) return std::unexpected(std::move(played.error()));
      }
    }
  }
  return {};
}

std::expected<RectF, StarException> ChatBubbleManager::bubbleImageRect(Vec2F screenPos, BubbleImage const& image, float pixelRatio) {
  auto metadata = m_guiContext.get().root().imageMetadataDatabase();
  if (!metadata) return std::unexpected(std::move(metadata.error()));
  auto size = (*metadata)->imageSize(std::get<0>(image));
  if (!size) return std::unexpected(std::move(size.error()));
  return RectF::withSize(screenPos + std::get<1>(image) * pixelRatio, Vec2F(*size) * pixelRatio);
}

std::expected<void, StarException> ChatBubbleManager::drawBubbleImage(Vec2F screenPos, BubbleImage const& image, float pixelRatio, int alpha) {
  return m_guiContext.get().drawQuad(std::get<0>(image), screenPos + std::get<1>(image) * pixelRatio, pixelRatio, {255, 255, 255, alpha});
}

std::expected<void, StarException> ChatBubbleManager::drawBubbleText(Vec2F screenPos, BubbleText const& text, float pixelRatio, int alpha, bool isPortrait) {
  TextStyle style = std::get<1>(text);
  style.color[3] *= ((float)alpha / 255.f);
  auto styled = m_guiContext.get().setTextStyle(style, pixelRatio);
  if (!styled) return std::unexpected(std::move(styled.error()));
  TextPositioning tp = isPortrait ? m_portraitTextTemplate : m_textTemplate;
  tp.pos = screenPos + std::get<3>(text) * pixelRatio;
  tp = scaledPositioning(tp, pixelRatio);
  auto rendered = m_guiContext.get().renderText(std::get<0>(text), tp);
  if (!rendered) return std::unexpected(std::move(rendered.error()));
  return {};
}

TextPositioning ChatBubbleManager::scaledPositioning(TextPositioning positioning, float pixelRatio) const {
  positioning.wrapWidth =
    positioning.wrapWidth.transform([pixelRatio](unsigned width) { return static_cast<unsigned>(width * pixelRatio); });
  return positioning;
}
}

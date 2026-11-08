import std;
import star.optional;
import star.json;
import star.xx_hash;
import star.json_extra;
import star.thread;
import star.time;
import star.color;
import star.font;
import star.list;
import star.rect;
import star.image_processing;
import star.hash;
import star.data_stream;
import star.string_view;
import star.directives;
import star.bi_map;
import star.string;
import star.vector;
import star.text;
import star.asset_path;
import star.listener;
import star.input_event;
import star.game_types;
import star.set;
import star.audio;
import star.map;
import star.lua;
import star.ordered_map;
import star.io_device;
import star.ref_ptr;
import star.logging;
import star.format;
import star.version;
import star.id_map;
import star.image;
import star.math_common;
import star.either;
import star.random;
import star.weighted_pool;
import star.casting;
import star.strong_typedef;
import star.interpolation;
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

import star.asset_source;
import star.assets;
import star.root_base;
import star.configuration;
import star.root;
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
import star.lua_components;
import star.world_client_state;
import star.interpolation_tracker;
import star.ambient;
import star.weather;
import star.game_timers;
import star.world_client;
import star.application;
import star.statistics_service;
import star.p2p_networking_service;
import star.user_generated_content_service;
import star.desktop_service;
import star.application_controller;
import star.renderer;
import star.cinematic;
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
import star.image_metadata_database;
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

import star.algorithm;
namespace Star {

const float vWidth = 960.0f;
const float vHeight = 540.0f;

std::expected<std::shared_ptr<Cinematic>, StarException> Cinematic::create(GuiContext& context) {
  return std::shared_ptr<Cinematic>(new Cinematic(context));
}

Cinematic::Cinematic(GuiContext& context) : m_guiContext(context) {
  m_completable = false;
  m_suppressInput = false;
}

std::expected<void, StarException> Cinematic::load(Json const& definition) {
  stop();
  Cinematic pending(m_guiContext.get());
  auto loaded = pending.loadDefinition(definition);
  if (!loaded)
    return std::unexpected(std::move(loaded.error()));
  pending.m_player = m_player;
  *this = std::move(pending);
  return {};
}

std::expected<void, StarException> Cinematic::loadDefinition(Json const& definition) {
  auto timeSkips = definition.getArray("timeSkips", JsonArray());
  if (!timeSkips)
    return std::unexpected(std::move(timeSkips.error()));
  for (auto const& timeSkipDefinition : *timeSkips) {
    auto available = timeSkipDefinition.getFloat("available");
    if (!available)
      return std::unexpected(std::move(available.error()));
    auto skipTo = timeSkipDefinition.getFloat("skipTo");
    if (!skipTo)
      return std::unexpected(std::move(skipTo.error()));
    m_timeSkips.push_back(TimeSkip{.availableTime = *available, .skipToTime = *skipTo});
  }
  std::sort(m_timeSkips.begin(), m_timeSkips.end(), [](TimeSkip const& a, TimeSkip const& b) {
    return a.availableTime < b.availableTime;
  });

  auto cameras = definition.getArray("camera", JsonArray());
  if (!cameras)
    return std::unexpected(std::move(cameras.error()));
  for (auto const& cameraDefinition : *cameras) {
    auto timecode = cameraDefinition.getFloat("timecode");
    if (!timecode)
      return std::unexpected(std::move(timecode.error()));
    auto zoom = cameraDefinition.getFloat("zoom", 1.0f);
    if (!zoom)
      return std::unexpected(std::move(zoom.error()));
    CameraKeyFrame keyFrame{.timecode = *timecode, .zoom = *zoom, .pan = {}};
    auto hasPan = cameraDefinition.contains("pan");
    if (!hasPan)
      return std::unexpected(std::move(hasPan.error()));
    if (*hasPan) {
      auto panJson = cameraDefinition.get("pan");
      if (!panJson)
        return std::unexpected(std::move(panJson.error()));
      auto pan = jsonToVec2F(*panJson);
      if (!pan)
        return std::unexpected(std::move(pan.error()));
      keyFrame.pan = *pan;
    }
    m_cameraKeyFrames.push_back(keyFrame);
  }

  auto panels = definition.getArray("panels");
  if (!panels)
    return std::unexpected(std::move(panels.error()));
  for (auto const& panelDefinition : *panels) {
    auto panel = std::make_shared<Panel>();
    auto useCamera = panelDefinition.getBool("useCamera", true);
    if (!useCamera)
      return std::unexpected(std::move(useCamera.error()));
    panel->useCamera = *useCamera;
    auto drawables = panelDefinition.getArray("drawables", {});
    if (!drawables)
      return std::unexpected(std::move(drawables.error()));
    panel->drawables = std::move(*drawables);
    auto animationFrames = panelDefinition.getInt("animationFrames", std::numeric_limits<int>::max());
    if (!animationFrames)
      return std::unexpected(std::move(animationFrames.error()));
    panel->animationFrames = *animationFrames;
    auto text = panelDefinition.getString("text", "");
    if (!text)
      return std::unexpected(std::move(text.error()));
    panel->text = std::move(*text);
    auto textPositionJson = panelDefinition.get("textPosition", JsonObject());
    if (!textPositionJson)
      return std::unexpected(std::move(textPositionJson.error()));
    auto textPosition = TextPositioning::create(*textPositionJson);
    if (!textPosition)
      return std::unexpected(std::move(textPosition.error()));
    panel->textPosition = std::move(*textPosition);
    auto textStyleJson = panelDefinition.get("textStyle", Json());
    if (!textStyleJson)
      return std::unexpected(std::move(textStyleJson.error()));
    auto textStyle = TextStyle::create(*textStyleJson);
    if (!textStyle)
      return std::unexpected(std::move(textStyle.error()));
    panel->textStyle = std::move(*textStyle);
    auto fontColorJson = panelDefinition.opt("fontColor");
    if (!fontColorJson)
      return std::unexpected(std::move(fontColorJson.error()));
    if (*fontColorJson) {
      auto color = jsonToVec4B(**fontColorJson);
      if (!color)
        return std::unexpected(std::move(color.error()));
      panel->textStyle.color = *color;
    }
    auto fontSize = panelDefinition.getUInt("fontSize", panel->textStyle.fontSize);
    if (!fontSize)
      return std::unexpected(std::move(fontSize.error()));
    panel->textStyle.fontSize = *fontSize;
    auto avatar = panelDefinition.getString("avatar", "");
    if (!avatar)
      return std::unexpected(std::move(avatar.error()));
    panel->avatar = std::move(*avatar);
    auto startTime = panelDefinition.getFloat("startTime", 0);
    if (!startTime)
      return std::unexpected(std::move(startTime.error()));
    panel->startTime = *startTime;
    auto endTime = panelDefinition.getFloat("endTime", 0);
    if (!endTime)
      return std::unexpected(std::move(endTime.error()));
    panel->endTime = *endTime;
    auto loopTime = panelDefinition.getFloat("loopTime", 0);
    if (!loopTime)
      return std::unexpected(std::move(loopTime.error()));
    panel->loopTime = *loopTime;
    auto keyframes = panelDefinition.get("keyframes").and_then([](Json value) { return value.iterateArray(); });
    if (!keyframes)
      return std::unexpected(std::move(keyframes.error()));
    for (auto const& keyframeDefinition : *keyframes) {
      KeyFrame keyframe;
      auto timecode = keyframeDefinition.getFloat("timecode");
      if (!timecode)
        return std::unexpected(std::move(timecode.error()));
      keyframe.timecode = *timecode;
      auto assignIfPresent = [&](String const& name, auto& target, auto convert) -> std::expected<void, StarException> {
        auto field = keyframeDefinition.find(name);
        if (!field)
          return std::unexpected(std::move(field.error()));
        if (*field) {
          auto value = std::invoke(convert, (*field)->get());
          if (!value)
            return std::unexpected(std::move(value.error()));
          target = std::move(*value);
        }
        return {};
      };
      auto decoded = assignIfPresent("complete", keyframe.complete, &Json::toBool);
      if (!decoded)
        return std::unexpected(std::move(decoded.error()));
      decoded = assignIfPresent("zoom", keyframe.zoom, &Json::toFloat);
      if (!decoded)
        return std::unexpected(std::move(decoded.error()));
      decoded = assignIfPresent("alpha", keyframe.alpha, &Json::toFloat);
      if (!decoded)
        return std::unexpected(std::move(decoded.error()));
      decoded = assignIfPresent("position", keyframe.position, jsonToVec2F);
      if (!decoded)
        return std::unexpected(std::move(decoded.error()));
      decoded = assignIfPresent("frame", keyframe.frame, &Json::toFloat);
      if (!decoded)
        return std::unexpected(std::move(decoded.error()));
      decoded = assignIfPresent("textPercentage", keyframe.textPercentage, &Json::toFloat);
      if (!decoded)
        return std::unexpected(std::move(decoded.error()));
      panel->keyFrames.push_back(std::move(keyframe));
      float endTimecode = panel->endTime > 0 ? std::min(panel->endTime, panel->startTime + *timecode) : panel->startTime + *timecode;
      m_completionTime = std::max(m_completionTime, endTimecode);
    }
    m_panels.push_back(std::move(panel));
  }

  auto audio = definition.getArray("audio");
  if (!audio)
    return std::unexpected(std::move(audio.error()));
  for (auto const& audioDefinition : *audio) {
    AudioCue cue;
    auto timecode = audioDefinition.getFloat("timecode");
    if (!timecode)
      return std::unexpected(std::move(timecode.error()));
    cue.timecode = *timecode;
    auto loops = audioDefinition.getInt("loops", 0);
    if (!loops)
      return std::unexpected(std::move(loops.error()));
    cue.loops = *loops;
    auto endTimecode = audioDefinition.getFloat("endTimecode", 0);
    if (!endTimecode)
      return std::unexpected(std::move(endTimecode.error()));
    cue.endTimecode = *endTimecode;
    auto resource = audioDefinition.getString("resource");
    if (!resource)
      return std::unexpected(std::move(resource.error()));
    cue.resource = std::move(*resource);
    m_audioCues.push_back(std::move(cue));
    m_completionTime = std::max(m_completionTime, *timecode);
    m_completionTime = std::max(m_completionTime, *endTimecode);
  }
  m_activeAudio.resize(m_audioCues.size());

  auto hasOffset = definition.contains("offset");
  if (!hasOffset)
    return std::unexpected(std::move(hasOffset.error()));
  if (*hasOffset) {
    auto offsetJson = definition.get("offset");
    if (!offsetJson)
      return std::unexpected(std::move(offsetJson.error()));
    auto offset = jsonToVec2F(*offsetJson);
    if (!offset)
      return std::unexpected(std::move(offset.error()));
    m_offset = *offset;
  }
  auto hasBackgroundColor = definition.contains("backgroundColor");
  if (!hasBackgroundColor)
    return std::unexpected(std::move(hasBackgroundColor.error()));
  if (*hasBackgroundColor) {
    auto backgroundColorJson = definition.get("backgroundColor");
    if (!backgroundColorJson)
      return std::unexpected(std::move(backgroundColorJson.error()));
    auto color = jsonToVec4B(*backgroundColorJson);
    if (!color)
      return std::unexpected(std::move(color.error()));
    m_backgroundColor = *color;
  }
  auto backgroundFadeTime = definition.getFloat("backgroundFadeTime", 0);
  if (!backgroundFadeTime)
    return std::unexpected(std::move(backgroundFadeTime.error()));
  m_backgroundFadeTime = *backgroundFadeTime;
  m_completionTime += 2 * m_backgroundFadeTime;

  auto scissor = definition.getBool("scissor", true);
  if (!scissor)
    return std::unexpected(std::move(scissor.error()));
  m_scissor = *scissor;
  auto letterbox = definition.getBool("letterbox", true);
  if (!letterbox)
    return std::unexpected(std::move(letterbox.error()));
  m_letterbox = *letterbox;
  auto skippable = definition.getBool("skippable", true);
  if (!skippable)
    return std::unexpected(std::move(skippable.error()));
  m_skippable = *skippable;
  auto suppressInput = definition.getBool("suppressInput", true);
  if (!suppressInput)
    return std::unexpected(std::move(suppressInput.error()));
  m_suppressInput = *suppressInput;
  auto muteSfx = definition.getBool("muteSfx", false);
  if (!muteSfx)
    return std::unexpected(std::move(muteSfx.error()));
  m_muteSfx = *muteSfx;
  auto muteMusic = definition.getBool("muteMusic", false);
  if (!muteMusic)
    return std::unexpected(std::move(muteMusic.error()));
  m_muteMusic = *muteMusic;

  m_timer.reset();
  m_timer.start();
  return {};
}

void Cinematic::setPlayer(std::shared_ptr<Player> player) { m_player = std::move(player); }

void Cinematic::update(float) {
  m_currentTimeSkip = {};
  for (auto timeSkip : m_timeSkips) {
    if (currentTimecode() >= timeSkip.availableTime && currentTimecode() < timeSkip.skipToTime)
      m_currentTimeSkip = timeSkip;
  }
}

bool Cinematic::completed() const {
  for (std::size_t i = 0; i < m_audioCues.size(); ++i) {
    if (m_activeAudio[i] && !m_activeAudio[i]->finished())
      return false;
  }

  return m_timer.time() >= m_completionTime;
}

bool Cinematic::completable() const {
  return m_completable;
}

std::expected<void, StarException> Cinematic::render() {
  if (completed())
    return {};

  auto& guiContext = m_guiContext.get();
  auto mixer = guiContext.mixer();
  auto rendererResult = guiContext.renderer();
  if (!rendererResult)
    return std::unexpected(std::move(rendererResult.error()));
  auto& renderer = rendererResult->get();
  auto textPainterResult = guiContext.textPainter();
  if (!textPainterResult)
    return std::unexpected(std::move(textPainterResult.error()));
  auto& textPainter = textPainterResult->get();

  m_windowSize = Vec2F(renderer.screenSize());
  Vec2F screenWindowSize = Vec2F(vWidth * m_drawableScale, vHeight * m_drawableScale);
  m_drawableScale = std::min(m_windowSize[0] / vWidth, m_windowSize[1] / vHeight);
  m_drawableTranslation = Vec2F(0.5f * (m_windowSize[0] - vWidth * m_drawableScale), 0.5f * (m_windowSize[1] - vHeight * m_drawableScale));
  m_scissorRect = RectI::withSize(Vec2I::floor((m_windowSize / 2) - (screenWindowSize / 2)), Vec2I::ceil(screenWindowSize));

  updateCamera(m_timer.time());

  float fadeFactor = 1.0;
  if (m_backgroundFadeTime > 0) {
    if (m_timer.time() < m_backgroundFadeTime)
      fadeFactor = m_timer.time() / m_backgroundFadeTime;
    else if (m_completionTime - m_timer.time() < m_backgroundFadeTime)
      fadeFactor = std::max<float>(0.0f, m_completionTime - m_timer.time()) / m_backgroundFadeTime;
  }

  if (m_backgroundColor) {
    Vec4B backgroundColor = *m_backgroundColor;
    backgroundColor[3] *= fadeFactor;
    renderer.render(renderFlatRect(RectF::withSize(Vec2F(0, 0), m_windowSize), backgroundColor, 0.0f));
  }

  if (m_letterbox && !m_backgroundColor) {
    Vec4B letterboxColor = Vec4B(0, 0, 0, 255 * fadeFactor);
    if (m_windowSize[0] / vWidth > m_windowSize[1] / vHeight) {
      renderer.render(renderFlatRect(RectF(0, 0, m_scissorRect.xMin(), m_windowSize[1]), letterboxColor, 0.0f));
      renderer.render(renderFlatRect(RectF(m_scissorRect.xMax(), 0, m_windowSize[0], m_windowSize[1]), letterboxColor, 0.0f));
    } else {
      renderer.render(renderFlatRect(RectF(0, 0, m_windowSize[0], m_scissorRect.yMin()), letterboxColor, 0.0f));
      renderer.render(renderFlatRect(RectF(0, m_scissorRect.yMax(), m_windowSize[0], m_windowSize[1]), letterboxColor, 0.0f));
    }
  }

  if (fadeFactor < 1.0f)
    return {};

  if (m_scissor) {
    auto scissor = renderer.setScissorRect(m_scissorRect);
    if (!scissor)
      return std::unexpected(std::move(scissor.error()));
  }

  auto renderPanels = [&]() -> std::expected<void, StarException> {
    String playerSpecies = "";
    if (m_player)
      playerSpecies = m_player->species();

    for (auto const& panel : m_panels) {
      float drawableScale = m_drawableScale;
      Vec2F drawableTranslation = m_drawableTranslation;
      if (panel->useCamera) {
        drawableScale *= m_cameraZoom;
        drawableTranslation += m_cameraPan * drawableScale;
      }

      auto values = determinePanelValues(panel, currentTimecode());
      if (values.completable)
        m_completable = true;
      if (!values.alpha)
        continue;
      auto frame = Star::to_string(static_cast<int>(values.frame) % panel->animationFrames);
      auto alphaColor = Color::rgbaf(1.0f, 1.0f, 1.0f, values.alpha);
      for (auto const& d : panel->drawables) {
        auto metadata = m_guiContext.get().root().imageMetadataDatabase();
        if (!metadata)
          return std::unexpected(std::move(metadata.error()));
        auto image = d.getString("image");
        if (!image)
          return std::unexpected(std::move(image.error()));
        auto definition = d.set("image", image->replace_tags(StringMap<String>{{"species", playerSpecies}, {"frame", frame}}));
        if (!definition)
          return std::unexpected(std::move(definition.error()));
        auto drawable = Drawable::create(**metadata, *definition);
        if (!drawable)
          return std::unexpected(std::move(drawable.error()));
        drawable->translate(m_offset);
        drawable->scale(values.zoom);
        drawable->translate(values.position);
        drawable->color *= alphaColor;
        auto drawn = drawDrawable(*drawable, drawableScale, drawableTranslation);
        if (!drawn)
          return std::unexpected(std::move(drawn.error()));
      }
      if (!panel->avatar.empty() && m_player) {
        auto portraitMode = enumValue<PortraitModeNames>(panel->avatar);
        if (!portraitMode)
          return std::unexpected(std::move(portraitMode.error()));
        auto portrait = m_player->portrait(*portraitMode);
        if (!portrait)
          return std::unexpected(std::move(portrait.error()));
        for (auto& drawable : *portrait) {
          drawable.translate(m_offset);
          drawable.scale(values.zoom);
          drawable.translate(values.position);
          drawable.color *= alphaColor;
          auto drawn = drawDrawable(drawable, drawableScale, drawableTranslation);
          if (!drawn)
            return std::unexpected(std::move(drawn.error()));
        }
      }
      if (!panel->text.empty()) {
        auto style = textPainter.setTextStyle(panel->textStyle);
        if (!style)
          return std::unexpected(std::move(style.error()));
        textPainter.setFontSize(std::floor(panel->textStyle.fontSize * drawableScale));
        auto fontColor = panel->textStyle.color;
        fontColor[3] *= values.alpha;
        textPainter.setFontColor(fontColor);
        Vec2F position = (m_offset + values.position + Vec2F(panel->textPosition.pos)) * drawableScale + drawableTranslation;
        TextPositioning tp(position, panel->textPosition.hAnchor, panel->textPosition.vAnchor, {}, {});
        if (panel->textPosition.wrapWidth)
          tp.wrapWidth = std::floor(*panel->textPosition.wrapWidth * drawableScale);
        if (values.textPercentage < 1.0) {
          auto length = panel->text.length();
          if (!length)
            return std::unexpected(std::move(length.error()));
          tp.charLimit = std::floor(*length * values.textPercentage);
        }
        auto text = textPainter.renderText(panel->text, tp);
        if (!text)
          return std::unexpected(std::move(text.error()));
      }
    }
    return {};
  };
  auto panelsRendered = renderPanels();

  if (m_scissor) {
    auto resetScissor = renderer.setScissorRect({});
    if (!resetScissor) {
      if (!panelsRendered)
        return std::unexpected(panelsRendered.error().with_context(
            Star::format("Cinematic scissor restoration failed: {}",
                Star::print_exception(ExceptionDiagnostic(resetScissor.error()), true))));
      return std::unexpected(std::move(resetScissor.error()));
    }
  }
  if (!panelsRendered)
    return std::unexpected(std::move(panelsRendered.error()));

  for (std::size_t i = 0; i < m_audioCues.size(); ++i) {
    if (m_audioCues[i].endTimecode > 0 && m_audioCues[i].endTimecode <= currentTimecode()) {
      if (!m_activeAudio[i])
        continue;
      m_activeAudio[i]->stop();
    } else if (m_audioCues[i].timecode <= currentTimecode()) {
      if (m_activeAudio[i])
        continue;
      auto assets = m_guiContext.get().root().assets();
      if (!assets)
        return std::unexpected(std::move(assets.error()));
      auto audio = (*assets)->audio(m_audioCues[i].resource);
      if (!audio)
        return std::unexpected(std::move(audio.error()));
      auto audioInstance = AudioInstance::create(**audio);
      if (!audioInstance)
        return std::unexpected(std::move(audioInstance.error()));
      (*audioInstance)->setLoops(m_audioCues[i].loops);
      (*audioInstance)->setMixerGroup(MixerGroup::Cinematic);
      auto played = mixer->play(*audioInstance);
      if (!played)
        return std::unexpected(std::move(played.error()));
      m_activeAudio[i] = std::move(*audioInstance);
    }
  }
  return {};
}

std::expected<void, StarException> Cinematic::drawDrawable(Drawable const& drawable, float drawableScale, Vec2F const& drawableTranslation) {
  auto& guiContext = m_guiContext.get();
  auto renderer = guiContext.renderer();
  if (!renderer)
    return std::unexpected(std::move(renderer.error()));
  auto textureGroup = guiContext.assetTextureGroup();
  if (!textureGroup)
    return std::unexpected(std::move(textureGroup.error()));

  auto& primitives = renderer->get().immediatePrimitives();

  if (drawable.isImage()) {
    auto const& imagePart = drawable.imagePart();
    auto texture = textureGroup->get().loadTexture(imagePart.image);
    if (!texture)
      return std::unexpected(std::move(texture.error()));
    auto size = Vec2F((*texture)->size());

    RectF imageRect(Vec2F(), size);

    Vec2F screenTranslation = drawable.position * drawableScale + drawableTranslation;

    Vec2F lowerLeft =
        imagePart.transformation.transformVec2(Vec2F(imageRect.xMin(), imageRect.yMin())) * drawableScale
        + screenTranslation;
    Vec2F lowerRight =
        imagePart.transformation.transformVec2(Vec2F(imageRect.xMax(), imageRect.yMin())) * drawableScale
        + screenTranslation;
    Vec2F upperRight =
        imagePart.transformation.transformVec2(Vec2F(imageRect.xMax(), imageRect.yMax())) * drawableScale
        + screenTranslation;
    Vec2F upperLeft =
        imagePart.transformation.transformVec2(Vec2F(imageRect.xMin(), imageRect.yMax())) * drawableScale
        + screenTranslation;

    Vec4B drawableColor = drawable.color.toRgba();

    primitives.emplace_back(std::in_place_type_t<RenderQuad>(), std::move(*texture),
        lowerLeft,  Vec2F{0, 0},
        lowerRight, Vec2F{size[0], 0},
        upperRight, Vec2F{size[0], size[1]},
        upperLeft,  Vec2F{0, size[1]},
        drawableColor, 0.0f);
  } else if (!std::holds_alternative<std::monostate>(drawable.part)) {
    return std::unexpected(StarException("Cinematic drawable must be an image"));
  }
  return {};
}

void Cinematic::updateCamera(float timecode) {
  float startZoom = 1.0f;
  float startZoomTimecode = -1;
  float endZoom = startZoom;
  float endZoomTimecode = startZoomTimecode;

  Vec2F startPan = {0, 0};
  float startPanTimecode = -1;
  Vec2F endPan = startPan;
  float endPanTimecode = startPanTimecode;

  for (auto keyframe : m_cameraKeyFrames) {
    if (keyframe.timecode <= timecode) {
      startZoom = keyframe.zoom;
      startZoomTimecode = keyframe.timecode;
    }
    if (endZoomTimecode < timecode) {
      endZoom = keyframe.zoom;
      endZoomTimecode = keyframe.timecode;
    }

    if (keyframe.timecode <= timecode) {
      startPan = keyframe.pan;
      startPanTimecode = keyframe.timecode;
    }
    if (endPanTimecode < timecode) {
      endPan = keyframe.pan;
      endPanTimecode = keyframe.timecode;
    }
  }

  if (startZoom == endZoom)
    m_cameraZoom = startZoom;
  else if (timecode <= startZoomTimecode)
    m_cameraZoom = startZoom;
  else if (timecode >= endZoomTimecode)
    m_cameraZoom = endZoom;
  else
    m_cameraZoom = lerp((timecode - startZoomTimecode) / (endZoomTimecode - startZoomTimecode), startZoom, endZoom);

  if (startPan == endPan)
    m_cameraPan = startPan;
  else if (timecode <= startPanTimecode)
    m_cameraPan = startPan;
  else if (timecode >= endPanTimecode)
    m_cameraPan = endPan;
  else
    m_cameraPan = lerp((timecode - startPanTimecode) / (endPanTimecode - startPanTimecode), startPan, endPan);
}

float Cinematic::currentTimecode() const {
  return std::min(static_cast<float>(m_timer.time()) - m_backgroundFadeTime,
                  m_completionTime - 2 * m_backgroundFadeTime);
}

Cinematic::PanelValues Cinematic::determinePanelValues(std::shared_ptr<Panel> panel, float timecode) {
  if (panel->endTime != 0) {
    if (timecode > panel->endTime) {
      Cinematic::PanelValues result{};
      result.alpha = 0;
      return result;
    }
  }

  if (panel->startTime != 0) {
    if (timecode < panel->startTime) {
      Cinematic::PanelValues result{};
      result.alpha = 0;
      return result;
    } else {
      timecode -= panel->startTime;
    }
  }

  if (panel->loopTime != 0) {
    timecode = std::fmod(timecode, panel->loopTime);
  }

  float startZoom = 0;
  float startZoomTimecode = -1;
  float endZoom = startZoom;
  float endZoomTimecode = startZoomTimecode;

  float startAlpha = 0;
  float startAlphaTimecode = -1;
  float endAlpha = startAlpha;
  float endAlphaTimecode = startAlphaTimecode;

  Vec2F startPosition = {};
  float startPositionTimecode = -1;
  Vec2F endPosition = startPosition;
  float endPositionTimecode = startPositionTimecode;

  float startFrame = 0;
  float startFrameTimecode = -1;
  float endFrame = startFrame;
  float endFrameTimecode = startFrameTimecode;

  float startTextPercentage = 1;
  float startTextPercentageTimecode = -1;
  float endTextPercentage = 1;
  float endTextPercentageTimecode = -1;

  bool completable = false;

  for (auto const& keyframe : panel->keyFrames) {
    if (keyframe.complete && *keyframe.complete && keyframe.timecode <= timecode)
      completable = true;

    if (keyframe.zoom) {
      float zoom = *keyframe.zoom;
      if (keyframe.timecode <= timecode) {
        startZoom = zoom;
        startZoomTimecode = keyframe.timecode;
      }
      if (endZoomTimecode < timecode) {
        endZoom = zoom;
        endZoomTimecode = keyframe.timecode;
      }
    }

    if (keyframe.alpha) {
      float alpha = *keyframe.alpha;
      if (keyframe.timecode <= timecode) {
        startAlpha = alpha;
        startAlphaTimecode = keyframe.timecode;
      }
      if (endAlphaTimecode < timecode) {
        endAlpha = alpha;
        endAlphaTimecode = keyframe.timecode;
      }
    }

    if (keyframe.position) {
      Vec2F position = *keyframe.position;
      if (keyframe.timecode <= timecode) {
        startPosition = position;
        startPositionTimecode = keyframe.timecode;
      }
      if (endPositionTimecode < timecode) {
        endPosition = position;
        endPositionTimecode = keyframe.timecode;
      }
    }

    if (keyframe.frame) {
      float frame = *keyframe.frame;
      if (keyframe.timecode <= timecode) {
        startFrame = frame;
        startFrameTimecode = keyframe.timecode;
      }
      if (endFrameTimecode < timecode) {
        endFrame = frame;
        endFrameTimecode = keyframe.timecode;
      }
    }

    if (keyframe.textPercentage) {
      float textPercentage = *keyframe.textPercentage;
      if (keyframe.timecode <= timecode) {
        startTextPercentage = textPercentage;
        startTextPercentageTimecode = keyframe.timecode;
      }
      if (endTextPercentageTimecode < timecode) {
        endTextPercentage = textPercentage;
        endTextPercentageTimecode = keyframe.timecode;
      }
    }
  }

  Cinematic::PanelValues result;

  result.completable = completable;

  if (startZoom == endZoom)
    result.zoom = startZoom;
  else if (timecode <= startZoomTimecode)
    result.zoom = startZoom;
  else if (timecode >= endZoomTimecode)
    result.zoom = endZoom;
  else
    result.zoom = lerp((timecode - startZoomTimecode) / (endZoomTimecode - startZoomTimecode), startZoom, endZoom);

  if (startAlpha == endAlpha)
    result.alpha = startAlpha;
  else if (timecode <= startAlphaTimecode)
    result.alpha = startAlpha;
  else if (timecode >= endAlphaTimecode)
    result.alpha = endAlpha;
  else
    result.alpha = lerp((timecode - startAlphaTimecode) / (endAlphaTimecode - startAlphaTimecode), startAlpha, endAlpha);

  if (startPosition == endPosition)
    result.position = startPosition;
  else if (timecode <= startPositionTimecode)
    result.position = startPosition;
  else if (timecode >= endPositionTimecode)
    result.position = endPosition;
  else
    result.position = lerp((timecode - startPositionTimecode) / (endPositionTimecode - startPositionTimecode), startPosition, endPosition);

  if (startFrame == endFrame)
    result.frame = startFrame;
  else if (timecode <= startFrameTimecode)
    result.frame = startFrame;
  else if (timecode >= endFrameTimecode)
    result.frame = endFrame;
  else
    result.frame = lerp((timecode - startFrameTimecode) / (endFrameTimecode - startFrameTimecode), startFrame, endFrame);

  if (startTextPercentage == endTextPercentage)
    result.textPercentage = startTextPercentage;
  else if (timecode <= startTextPercentageTimecode)
    result.textPercentage = startTextPercentage;
  else if (timecode >= endTextPercentageTimecode)
    result.textPercentage = endTextPercentage;
  else
    result.textPercentage =
        lerp((timecode - startTextPercentageTimecode) / (endTextPercentageTimecode - startTextPercentageTimecode),
            startTextPercentage,
            endTextPercentage);

  return result;
}

void Cinematic::setTime(float timecode) {
  m_timer.setTime(timecode + m_backgroundFadeTime);
}

void Cinematic::stop() {
  m_timeSkips.clear();
  m_cameraKeyFrames.clear();
  m_panels.clear();
  m_completionTime = 0;
  m_timer.stop();
  m_timer.reset();
  for (std::size_t i = 0; i < m_audioCues.size(); ++i) {
    if (m_activeAudio[i])
      m_activeAudio[i]->stop();
  }
  m_audioCues.clear();
  m_activeAudio.clear();
  m_completable = false;
  m_suppressInput = false;
}

std::expected<bool, StarException> Cinematic::handleInputEvent(InputEvent const& event) {
  if (completed())
    return false;
  if (std::holds_alternative<MouseButtonUpEvent>(event) || std::holds_alternative<KeyUpEvent>(event))
    return false;
  if (std::holds_alternative<KeyDownEvent>(event)) {
    if (m_currentTimeSkip) {
      auto timeSkip = takeOptional(m_currentTimeSkip);
      if (!timeSkip)
        return std::unexpected(std::move(timeSkip.error()));
      setTime(timeSkip->skipToTime);
      return true;
    } else if (m_skippable && m_guiContext.get().actions(event).contains(InterfaceAction::CinematicSkip)) {
      stop();
      return true;
    }
  }
  return m_suppressInput;
}

bool Cinematic::suppressInput() const {
  return m_suppressInput && !completed();
}

bool Cinematic::muteSfx() const {
  return m_muteSfx && !completed();
}

bool Cinematic::muteMusic() const {
  return m_muteMusic && !completed();
}

}

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
import star.id_map;
import star.image;
import star.math_common;
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

struct CinematicContentErrorTag {
  static constexpr std::string_view name() { return "CinematicContentError"; }
};
using CinematicContentError = StarError<CinematicContentErrorTag, StarException>;

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

std::expected<void, StarException> Cinematic::rejectContent(StarException const& error) {
  stop();
  return m_guiContext.get().root().logger().warn("Skipping malformed cinematic: {}", error.what());
}

std::expected<void, StarException> Cinematic::loadOptional(std::expected<Json, StarException> definition) {
  auto loaded = definition.and_then([this](Json const& config) { return load(config); });
  if (!loaded)
    return rejectContent(loaded.error());
  return {};
}

std::expected<void, StarException> Cinematic::loadDefinition(Json const& definition) {
  auto timeSkips = definition.get_array("timeSkips", JsonArray());
  if (!timeSkips)
    return std::unexpected(std::move(timeSkips.error()));
  for (auto const& timeSkipDefinition : *timeSkips) {
    auto available = timeSkipDefinition.get_float("available");
    if (!available)
      return std::unexpected(std::move(available.error()));
    auto skipTo = timeSkipDefinition.get_float("skipTo");
    if (!skipTo)
      return std::unexpected(std::move(skipTo.error()));
    m_timeSkips.push_back(TimeSkip{.availableTime = *available, .skipToTime = *skipTo});
  }
  std::sort(m_timeSkips.begin(), m_timeSkips.end(), [](TimeSkip const& a, TimeSkip const& b) {
    return a.availableTime < b.availableTime;
  });

  auto cameras = definition.get_array("camera", JsonArray());
  if (!cameras)
    return std::unexpected(std::move(cameras.error()));
  for (auto const& cameraDefinition : *cameras) {
    auto timecode = cameraDefinition.get_float("timecode");
    if (!timecode)
      return std::unexpected(std::move(timecode.error()));
    auto zoom = cameraDefinition.get_float("zoom", 1.0f);
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
      auto pan = json_to_vec2f(*panJson);
      if (!pan)
        return std::unexpected(std::move(pan.error()));
      keyFrame.pan = *pan;
    }
    m_cameraKeyFrames.push_back(keyFrame);
  }

  auto panels = definition.get_array("panels");
  if (!panels)
    return std::unexpected(std::move(panels.error()));
  for (auto const& panelDefinition : *panels) {
    auto panel = std::make_shared<Panel>();
    auto useCamera = panelDefinition.get_bool("useCamera", true);
    if (!useCamera)
      return std::unexpected(std::move(useCamera.error()));
    panel->useCamera = *useCamera;
    auto drawables = panelDefinition.get_array("drawables", {});
    if (!drawables)
      return std::unexpected(std::move(drawables.error()));
    panel->drawables = std::move(*drawables);
    auto animationFrames = panelDefinition.get_int("animationFrames", std::numeric_limits<int>::max());
    if (!animationFrames)
      return std::unexpected(std::move(animationFrames.error()));
    if (*animationFrames <= 0 || *animationFrames > std::numeric_limits<int>::max())
      return std::unexpected(StarException("Cinematic animationFrames must be a positive int"));
    panel->animationFrames = *animationFrames;
    auto text = panelDefinition.get_string("text", "");
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
      auto color = json_to_vec4b(**fontColorJson);
      if (!color)
        return std::unexpected(std::move(color.error()));
      panel->textStyle.color = *color;
    }
    auto fontSize = panelDefinition.get_uint("fontSize", panel->textStyle.font_size);
    if (!fontSize)
      return std::unexpected(std::move(fontSize.error()));
    panel->textStyle.font_size = *fontSize;
    auto avatar = panelDefinition.get_string("avatar", "");
    if (!avatar)
      return std::unexpected(std::move(avatar.error()));
    panel->avatar = std::move(*avatar);
    auto startTime = panelDefinition.get_float("startTime", 0);
    if (!startTime)
      return std::unexpected(std::move(startTime.error()));
    panel->startTime = *startTime;
    auto endTime = panelDefinition.get_float("endTime", 0);
    if (!endTime)
      return std::unexpected(std::move(endTime.error()));
    panel->endTime = *endTime;
    auto loopTime = panelDefinition.get_float("loopTime", 0);
    if (!loopTime)
      return std::unexpected(std::move(loopTime.error()));
    panel->loopTime = *loopTime;
    auto keyframes = panelDefinition.get("keyframes").and_then([](Json value) { return value.iterate_array(); });
    if (!keyframes)
      return std::unexpected(std::move(keyframes.error()));
    for (auto const& keyframeDefinition : *keyframes) {
      KeyFrame keyframe;
      auto timecode = keyframeDefinition.get_float("timecode");
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
      auto decoded = assignIfPresent("complete", keyframe.complete, &Json::to_bool);
      if (!decoded)
        return std::unexpected(std::move(decoded.error()));
      decoded = assignIfPresent("zoom", keyframe.zoom, &Json::to_float);
      if (!decoded)
        return std::unexpected(std::move(decoded.error()));
      decoded = assignIfPresent("alpha", keyframe.alpha, &Json::to_float);
      if (!decoded)
        return std::unexpected(std::move(decoded.error()));
      decoded = assignIfPresent("position", keyframe.position, json_to_vec2f);
      if (!decoded)
        return std::unexpected(std::move(decoded.error()));
      decoded = assignIfPresent("frame", keyframe.frame, &Json::to_float);
      if (!decoded)
        return std::unexpected(std::move(decoded.error()));
      decoded = assignIfPresent("textPercentage", keyframe.textPercentage, &Json::to_float);
      if (!decoded)
        return std::unexpected(std::move(decoded.error()));
      panel->keyFrames.push_back(std::move(keyframe));
      float endTimecode = panel->endTime > 0 ? std::min(panel->endTime, panel->startTime + *timecode) : panel->startTime + *timecode;
      m_completionTime = std::max(m_completionTime, endTimecode);
    }
    m_panels.push_back(std::move(panel));
  }

  auto audio = definition.get_array("audio");
  if (!audio)
    return std::unexpected(std::move(audio.error()));
  for (auto const& audioDefinition : *audio) {
    AudioCue cue;
    auto timecode = audioDefinition.get_float("timecode");
    if (!timecode)
      return std::unexpected(std::move(timecode.error()));
    cue.timecode = *timecode;
    auto loops = audioDefinition.get_int("loops", 0);
    if (!loops)
      return std::unexpected(std::move(loops.error()));
    cue.loops = *loops;
    auto endTimecode = audioDefinition.get_float("endTimecode", 0);
    if (!endTimecode)
      return std::unexpected(std::move(endTimecode.error()));
    cue.endTimecode = *endTimecode;
    auto resource = audioDefinition.get_string("resource");
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
    auto offset = json_to_vec2f(*offsetJson);
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
    auto color = json_to_vec4b(*backgroundColorJson);
    if (!color)
      return std::unexpected(std::move(color.error()));
    m_backgroundColor = *color;
  }
  auto backgroundFadeTime = definition.get_float("backgroundFadeTime", 0);
  if (!backgroundFadeTime)
    return std::unexpected(std::move(backgroundFadeTime.error()));
  m_backgroundFadeTime = *backgroundFadeTime;
  m_completionTime += 2 * m_backgroundFadeTime;

  auto scissor = definition.get_bool("scissor", true);
  if (!scissor)
    return std::unexpected(std::move(scissor.error()));
  m_scissor = *scissor;
  auto letterbox = definition.get_bool("letterbox", true);
  if (!letterbox)
    return std::unexpected(std::move(letterbox.error()));
  m_letterbox = *letterbox;
  auto skippable = definition.get_bool("skippable", true);
  if (!skippable)
    return std::unexpected(std::move(skippable.error()));
  m_skippable = *skippable;
  auto suppressInput = definition.get_bool("suppressInput", true);
  if (!suppressInput)
    return std::unexpected(std::move(suppressInput.error()));
  m_suppressInput = *suppressInput;
  auto muteSfx = definition.get_bool("muteSfx", false);
  if (!muteSfx)
    return std::unexpected(std::move(muteSfx.error()));
  m_muteSfx = *muteSfx;
  auto muteMusic = definition.get_bool("muteMusic", false);
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
  m_scissorRect = RectI::with_size(Vec2I::floor((m_windowSize / 2) - (screenWindowSize / 2)), Vec2I::ceil(screenWindowSize));

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
    renderer.render(renderFlatRect(RectF::with_size(Vec2F(0, 0), m_windowSize), backgroundColor, 0.0f));
  }

  if (m_letterbox && !m_backgroundColor) {
    Vec4B letterboxColor = Vec4B(0, 0, 0, 255 * fadeFactor);
    if (m_windowSize[0] / vWidth > m_windowSize[1] / vHeight) {
      renderer.render(renderFlatRect(RectF(0, 0, m_scissorRect.x_min(), m_windowSize[1]), letterboxColor, 0.0f));
      renderer.render(renderFlatRect(RectF(m_scissorRect.x_max(), 0, m_windowSize[0], m_windowSize[1]), letterboxColor, 0.0f));
    } else {
      renderer.render(renderFlatRect(RectF(0, 0, m_windowSize[0], m_scissorRect.y_min()), letterboxColor, 0.0f));
      renderer.render(renderFlatRect(RectF(0, m_scissorRect.y_max(), m_windowSize[0], m_windowSize[1]), letterboxColor, 0.0f));
    }
  }

  if (fadeFactor < 1.0f)
    return {};

  std::optional<RectI> previousScissor;
  if (m_scissor) {
    previousScissor = renderer.scissorRect();
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
      if (!(values.frame >= std::numeric_limits<int>::min() && static_cast<double>(values.frame) <= std::numeric_limits<int>::max()))
        return std::unexpected(CinematicContentError("Cinematic frame is outside the integer range"));
      if (!(values.alpha >= 0 && values.alpha <= 1 && values.textPercentage >= 0 && values.textPercentage <= 1))
        return std::unexpected(CinematicContentError("Invalid cinematic panel alpha or textPercentage"));
      auto frame = Star::to_string(static_cast<int>(values.frame) % panel->animationFrames);
      auto alphaColor = Color::rgba_f(1.0f, 1.0f, 1.0f, values.alpha);
      for (auto const& d : panel->drawables) {
        auto metadata = m_guiContext.get().root().imageMetadataDatabase();
        if (!metadata)
          return std::unexpected(std::move(metadata.error()));
        auto image = d.get_string("image");
        if (!image)
          return std::unexpected(CinematicContentError("Invalid panel image", std::move(image.error())));
        auto definition = d.set("image", image->replace_tags(StringMap<String>{{"species", playerSpecies}, {"frame", frame}}));
        if (!definition)
          return std::unexpected(CinematicContentError("Invalid panel image definition", std::move(definition.error())));
        auto drawable = Drawable::create(**metadata, *definition);
        if (!drawable)
          return std::unexpected(CinematicContentError("Invalid panel drawable", std::move(drawable.error())));
        drawable->translate(m_offset);
        drawable->scale(values.zoom);
        drawable->translate(values.position);
        drawable->color *= alphaColor;
        auto drawn = drawDrawable(*drawable, drawableScale, drawableTranslation);
        if (!drawn)
          return std::unexpected(std::move(drawn.error()));
      }
      if (!panel->avatar.empty() && m_player) {
        auto portraitMode = enum_value<portrait_mode_names>(panel->avatar);
        if (!portraitMode)
          return std::unexpected(CinematicContentError("Invalid panel avatar", std::move(portraitMode.error())));
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
        textPainter.setFontSize(std::floor(panel->textStyle.font_size * drawableScale));
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
            return std::unexpected(CinematicContentError("Invalid panel text", std::move(length.error())));
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
    auto resetScissor = renderer.setScissorRect(previousScissor);
    if (!resetScissor) {
      if (!panelsRendered)
        return std::unexpected(resetScissor.error().with_related(panelsRendered.error(), "Drawing cinematic panels also failed"));
      return std::unexpected(std::move(resetScissor.error()));
    }
  }
  if (!panelsRendered) {
    if (panelsRendered.error().is<CinematicContentError>())
      return rejectContent(panelsRendered.error());
    return std::unexpected(std::move(panelsRendered.error()));
  }

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
        return rejectContent(audio.error());
      auto audioInstance = AudioInstance::create(**audio);
      if (!audioInstance)
        return rejectContent(audioInstance.error());
      (*audioInstance)->set_loops(m_audioCues[i].loops);
      (*audioInstance)->set_mixer_group(MixerGroup::Cinematic);
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

  if (drawable.is_image()) {
    auto const& imagePart = drawable.image_part();
    auto texture = textureGroup->get().loadTexture(imagePart.image);
    if (!texture) {
      if (texture.error().is<AssetException>())
        return std::unexpected(CinematicContentError("Invalid panel texture asset", std::move(texture.error())));
      return std::unexpected(std::move(texture.error()));
    }
    auto size = Vec2F((*texture)->size());

    RectF imageRect(Vec2F(), size);

    Vec2F screenTranslation = drawable.position * drawableScale + drawableTranslation;

    Vec2F lowerLeft =
        imagePart.transformation.transform_point(Vec2F(imageRect.x_min(), imageRect.y_min())) * drawableScale
        + screenTranslation;
    Vec2F lowerRight =
        imagePart.transformation.transform_point(Vec2F(imageRect.x_max(), imageRect.y_min())) * drawableScale
        + screenTranslation;
    Vec2F upperRight =
        imagePart.transformation.transform_point(Vec2F(imageRect.x_max(), imageRect.y_max())) * drawableScale
        + screenTranslation;
    Vec2F upperLeft =
        imagePart.transformation.transform_point(Vec2F(imageRect.x_min(), imageRect.y_max())) * drawableScale
        + screenTranslation;

    Vec4B drawableColor = drawable.color.to_rgba();

    primitives.emplace_back(std::in_place_type_t<RenderQuad>(), std::move(*texture),
        lowerLeft,  Vec2F{0, 0},
        lowerRight, Vec2F{size[0], 0},
        upperRight, Vec2F{size[0], size[1]},
        upperLeft,  Vec2F{0, size[1]},
        drawableColor, 0.0f);
  } else if (!std::holds_alternative<std::monostate>(drawable.part)) {
    return std::unexpected(CinematicContentError("Cinematic drawable must be an image"));
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
  m_timer.set_time(timecode + m_backgroundFadeTime);
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
      auto timeSkip = take_optional(m_currentTimeSkip);
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

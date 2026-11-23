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
ChatBubbleManager::ChatBubbleManager(ConstructionKey, GuiContext& context)
  : m_gui_context(context), m_text_template(Vec2F()), m_portrait_text_template(Vec2F()) {}

std::expected<std::unique_ptr<ChatBubbleManager>, StarException> ChatBubbleManager::create(GuiContext& context) {
  auto manager = std::make_unique<ChatBubbleManager>(ConstructionKey{}, context);
  auto initialized = manager->initialize();
  if (!initialized) return std::unexpected(std::move(initialized.error()));
  return manager;
}

std::expected<void, StarException> ChatBubbleManager::initialize() {
  auto& context = m_gui_context.get();
  auto assets = context.root().assets();
  if (!assets) return std::unexpected(std::move(assets.error()));
  auto config = (*assets)->json("/interface/windowconfig/chatbubbles.config");
  if (!config) return std::unexpected(std::move(config.error()));
  m_cached_interface_scale = context.interfaceScale();
  auto color_json = config->get("textColor");
  if (!color_json) return std::unexpected(std::move(color_json.error()));
  auto color = json_to_color(*color_json);
  if (!color) return std::unexpected(std::move(color.error()));
  m_text_style.color = color->to_rgba();
  auto style_json = config->get("textStyle");
  if (!style_json) return std::unexpected(std::move(style_json.error()));
  auto style = m_text_style.load_json(*style_json);
  if (!style) return std::unexpected(std::move(style.error()));
  {
    auto json = config->get("textPadding");
    if (!json) return std::unexpected(std::move(json.error()));
    auto value = json_to_vec2f(*json);
    if (!value) return std::unexpected(std::move(value.error()));
    m_text_padding = *value;
  }
  {
    auto json = config->get("bubbleOffset");
    if (!json) return std::unexpected(std::move(json.error()));
    auto value = json_to_vec2f(*json);
    if (!value) return std::unexpected(std::move(value.error()));
    m_bubble_offset = *value;
  }
  {
    auto json = config->get("portraitMorePosition");
    if (!json) return std::unexpected(std::move(json.error()));
    auto value = json_to_vec2i(*json);
    if (!value) return std::unexpected(std::move(value.error()));
    m_portrait_more_position = *value;
  }
  {
    auto json = config->get("portraitBackgroundSize");
    if (!json) return std::unexpected(std::move(json.error()));
    auto value = json_to_vec2i(*json);
    if (!value) return std::unexpected(std::move(value.error()));
    m_portrait_background_size = *value;
  }
  {
    auto json = config->get("portraitPosition");
    if (!json) return std::unexpected(std::move(json.error()));
    auto value = json_to_vec2i(*json);
    if (!value) return std::unexpected(std::move(value.error()));
    m_portrait_position = *value;
  }
  {
    auto json = config->get("portraitSize");
    if (!json) return std::unexpected(std::move(json.error()));
    auto value = json_to_vec2i(*json);
    if (!value) return std::unexpected(std::move(value.error()));
    m_portrait_size = *value;
  }
  {
    auto json = config->get("portraitTextPosition");
    if (!json) return std::unexpected(std::move(json.error()));
    auto value = json_to_vec2i(*json);
    if (!value) return std::unexpected(std::move(value.error()));
    m_portrait_text_position = *value;
  }
  {
    auto value = config->get_float("partSize");
    if (!value) return std::unexpected(std::move(value.error()));
    m_part_size = *value;
  }
  {
    auto value = config->get_float("maxAge");
    if (!value) return std::unexpected(std::move(value.error()));
    m_max_age = std::move(*value);
  }
  {
    auto value = config->get_float("portraitMaxAge");
    if (!value) return std::unexpected(std::move(value.error()));
    m_portrait_max_age = std::move(*value);
  }
  {
    auto value = config->get_float("interBubbleMargin");
    if (!value) return std::unexpected(std::move(value.error()));
    m_inter_bubble_margin = std::move(*value);
  }
  {
    auto value = config->get_int("maxMessagePerEntity");
    if (!value) return std::unexpected(std::move(value.error()));
    m_max_message_per_entity = std::move(*value);
  }
  {
    auto value = config->get_string("portraitBackgroundImage");
    if (!value) return std::unexpected(std::move(value.error()));
    m_portrait_background_image = std::move(*value);
  }
  {
    auto value = config->get_string("portraitMoreImage");
    if (!value) return std::unexpected(std::move(value.error()));
    m_portrait_more_image = std::move(*value);
  }
  {
    auto value = config->get_uint("portraitTextWidth");
    if (!value) return std::unexpected(std::move(value.error()));
    m_portrait_text_width = std::move(*value);
  }
  {
    auto value = config->get_float("portraitChatterFramerate");
    if (!value) return std::unexpected(std::move(value.error()));
    m_portrait_chatter_framerate = std::move(*value);
  }
  {
    auto value = config->get_float("portraitChatterDuration");
    if (!value) return std::unexpected(std::move(value.error()));
    m_portrait_chatter_duration = std::move(*value);
  }
  {
    auto value = config->get_float("furthestTextDistance");
    if (!value) return std::unexpected(std::move(value.error()));
    m_furthest_visible_text_distance = std::move(*value);
  }
  auto wrap = config->get_uint("textWrapWidth");
  if (!wrap) return std::unexpected(std::move(wrap.error()));
  m_text_template = TextPositioning{Vec2F(), HorizontalAnchor::HMidAnchor, VerticalAnchor::TopAnchor, *wrap};
  m_portrait_text_template = TextPositioning{Vec2F(m_portrait_text_position), HorizontalAnchor::LeftAnchor,
                                           VerticalAnchor::TopAnchor, m_portrait_text_width};
  {
    auto value = config->get_float("tweenFactor");
    if (!value) return std::unexpected(std::move(value.error()));
    m_bubbles.set_tween_factor(*value);
  }
  {
    auto value = config->get_float("movementThreshold");
    if (!value) return std::unexpected(std::move(value.error()));
    m_bubbles.set_movement_threshold(*value);
  }
  auto functions = context.root().functionDatabase();
  if (!functions) return std::unexpected(std::move(functions.error()));
  {
    auto name = config->get_string("textFadeFunction");
    if (!name) return std::unexpected(std::move(name.error()));
    auto value = (*functions)->function(*name);
    if (!value) return std::unexpected(std::move(value.error()));
    m_text_fade_function = std::move(*value);
  }
  {
    auto name = config->get_string("bubbleFadeFunction");
    if (!name) return std::unexpected(std::move(name.error()));
    auto value = (*functions)->function(*name);
    if (!value) return std::unexpected(std::move(value.error()));
    m_bubble_fade_function = std::move(*value);
  }
  return {};
}

std::expected<void, StarException> ChatBubbleManager::set_camera(WorldCamera const& camera) {
  float old_pixel_ratio = m_camera.pixelRatio();
  float old_interface_scale = m_cached_interface_scale;
  m_camera = camera;
  m_cached_interface_scale = m_gui_context.get().interfaceScale();
  if (m_camera.pixelRatio() != old_pixel_ratio || m_cached_interface_scale != old_interface_scale) {
    List<ChatAction> actions;
    auto collected = m_bubbles.for_each(
      [&actions](BubbleState<Bubble> const& state, Bubble const& bubble) -> std::expected<void, StarException> {
        actions.push_back(SayChatAction{bubble.entity, bubble.text, state.ideal_destination, bubble.config});
        return {};
      });
    if (!collected) return std::unexpected(std::move(collected.error()));
    m_bubbles.clear();
    for (auto& portrait_bubble : m_portrait_bubbles)
      actions.push_back(PortraitChatAction{portrait_bubble.entity, portrait_bubble.portrait, portrait_bubble.text,
                                           portrait_bubble.position, portrait_bubble.config});
    m_portrait_bubbles.clear();
    return add_chat_actions(std::move(actions), true);
  }
  return {};
}

std::expected<void, StarException> ChatBubbleManager::update(float dt, std::shared_ptr<WorldClient> const& world) {
  if (m_world.lock() != world) {
    // Actions queued for the new world have not bound an entity yet.
    m_bubbles.filter([](BubbleState<Bubble> const&, Bubble const& bubble) { return !bubble.instance; });
    m_portrait_bubbles.filter([](PortraitBubble const& bubble) { return !bubble.instance; });
    m_world = world;
  }
  auto updated = m_bubbles.update_destinations([this, dt, &world](BubbleState<Bubble>& bubble_state, Bubble& bubble) -> std::expected<void, StarException> {
    bubble.age += dt;
    if (auto entity = world->get<ChattyEntity>(bubble.entity)) {
      if (bubble.instance && bubble.instance->lock() != entity)
        return {};
      if (!bubble.instance)
        bubble.instance = entity;
      if (bubble.presentation_epoch != entity->presentationEpoch()) {
        bubble_state.reset_presentation();
        bubble.presentation_epoch = entity->presentationEpoch();
      }
      auto onscreen = m_camera.worldGeometry().rect_intersects_rect(m_camera.worldScreenRect(), entity->metaBoundBox().translated(entity->position()));
      if (!onscreen) return std::unexpected(std::move(onscreen.error()));
      bubble.onscreen = *onscreen;
      auto mouth = entity->mouth_position();
      if (!mouth) return std::unexpected(std::move(mouth.error()));
      bubble_state.ideal_destination = m_camera.worldToScreen(*mouth + m_bubble_offset);
    }
    return {};
  });
  if (!updated) return std::unexpected(std::move(updated.error()));

  for (auto& portrait_bubble : m_portrait_bubbles) {
    portrait_bubble.age += dt;
    if (auto entity = world->entity(portrait_bubble.entity)) {
      if (portrait_bubble.instance && portrait_bubble.instance->lock() != entity)
        continue;
      if (!portrait_bubble.instance)
        portrait_bubble.instance = entity;
      auto onscreen = m_camera.worldGeometry().rect_intersects_rect(m_camera.worldScreenRect(), entity->metaBoundBox().translated(entity->position()));
      if (!onscreen) return std::unexpected(std::move(onscreen.error()));
      portrait_bubble.onscreen = *onscreen;
      if (auto chatter = as<ChattyEntity>(entity)) {
        auto mouth = chatter->mouth_position();
        if (!mouth) return std::unexpected(std::move(mouth.error()));
        portrait_bubble.position = *mouth;
      } else
        portrait_bubble.position = entity->position();
    }
  }

  auto& count = m_entity_bubble_counts;
  count.clear();
  m_portrait_bubbles.filter([&](PortraitBubble const& portrait_bubble) -> bool {
      if ((count[portrait_bubble.entity] += m_max_message_per_entity) > m_max_message_per_entity)
        return false;
      if (auto entity = world->get<ChattyEntity>(portrait_bubble.entity))
        return portrait_bubble.instance && portrait_bubble.instance->lock() == entity
            && portrait_bubble.age < m_portrait_max_age;
      return false;
    });

  m_bubbles.filter([&](BubbleState<Bubble> const&, Bubble const& bubble) -> bool {
      if (++count[bubble.entity] > m_max_message_per_entity)
        return false;
      if (auto entity = world->get<ChattyEntity>(bubble.entity))
        return bubble.instance && bubble.instance->lock() == entity && bubble.age < m_max_age;
      return false;
    });

  m_bubbles.update(dt);
  return {};
}

std::expected<std::uint8_t, StarException> ChatBubbleManager::calc_distance_fade_alpha(Vec2F bubble_screen_position, StoredFunction const& fade_function, WorldCamera const& camera) const {
  // first calculate bubble position as a factor, distance from center to edge
  // of screen (0.0-1.0)
  float half_screen_width = camera.screenSize()[0] * 0.5f;
  float distance_factor = (std::fabsf(bubble_screen_position[0] - half_screen_width)) / half_screen_width;

  // that distance factor is divided by the max allowable distance
  // to re-space the distance as a 0 - 1 over the max allowable distance
  distance_factor = std::clamp(distance_factor / m_furthest_visible_text_distance, 0.0f, 1.0f);

  auto evaluated = fade_function.evaluate(distance_factor);
  if (!evaluated) return std::unexpected(std::move(evaluated.error()));
  return std::clamp(static_cast<int>(*evaluated), 0, 255);
}
std::expected<void, StarException> ChatBubbleManager::render(WorldClient const& world, WorldCamera const& camera, PresentationContext presentation) {
  if (m_world.lock().get() != &world)
    return {};
  if (m_presentation_epoch != presentation.epoch()) {
    m_bubbles.reset_presentation();
    m_presentation_epoch = presentation.epoch();
  }
  if (m_bubbles.empty() && m_portrait_bubbles.empty())
    return {};
  auto configuration = m_gui_context.get().root().configuration();
  if (!configuration) return std::unexpected(std::move(configuration.error()));
  auto setting = (*configuration)->get("speechBubbles");
  if (!setting) return std::unexpected(std::move(setting.error()));
  auto enabled = setting->to_bool();
  if (!enabled) return std::unexpected(std::move(enabled.error()));
  if (!*enabled) return {};

  float pixel_ratio = m_gui_context.get().interfaceScale();

  auto rendered = m_bubbles.for_each([this, &world, &camera, presentation, pixel_ratio](BubbleState<Bubble> const& state, Bubble const& bubble) -> std::expected<void, StarException> {
      if (bubble.onscreen) {
        auto entity = world.entity(bubble.entity);
        if (!entity || !bubble.instance || bubble.instance->lock() != entity)
          return {};
        auto origin = m_camera.screenToWorld(state.ideal_destination);
        origin += world.geometry().diff(world.renderPosition(bubble.entity, presentation), entity->position());
        auto screen_position = camera.worldToScreen(origin)
            + state.sample_offset(bubble.presentation_epoch == entity->presentationEpoch() ? presentation.interpolation() : 1.0f);
        auto fade = calc_distance_fade_alpha(screen_position, *m_bubble_fade_function, camera);
        if (!fade) return std::unexpected(std::move(fade.error()));
        int alpha = *fade;
        if (alpha) {
          for (auto const& bubble_image : bubble.background_images)
            { auto drawn = draw_bubble_image(screen_position, bubble_image, pixel_ratio, alpha); if (!drawn) return std::unexpected(std::move(drawn.error())); }
          for (auto const& bubble_text : bubble.bubble_text)
            { auto drawn = draw_bubble_text(screen_position, bubble_text, pixel_ratio, alpha, false); if (!drawn) return std::unexpected(std::move(drawn.error())); }
        }
      }
      return {};
    });
  if (!rendered) return std::unexpected(std::move(rendered.error()));

  for (auto const& portrait_bubble : m_portrait_bubbles) {
    auto entity = world.entity(portrait_bubble.entity);
    if (!entity || !portrait_bubble.instance || portrait_bubble.instance->lock() != entity)
      continue;
    if (portrait_bubble.onscreen) {
      auto position = portrait_bubble.position;
      position += world.geometry().diff(world.renderPosition(portrait_bubble.entity, presentation), entity->position());
      Vec2F screen_pos = camera.worldToScreen(position + m_bubble_offset);
      int frame = 0;
      if (portrait_bubble.age <= m_portrait_chatter_duration)
        frame = static_cast<int>((portrait_bubble.age / m_portrait_chatter_framerate) * 2) % 2;
      // 255 here because portrait bubbles are always full opacity
      for (auto const& bubble_image : portrait_bubble.background_images) {
        auto frame_image = std::get<0>(bubble_image).replace("<frame>", Star::to_string(frame));
        if (!frame_image) return std::unexpected(std::move(frame_image.error()));
        auto drawn = draw_bubble_image(screen_pos, std::make_tuple(std::move(*frame_image), std::get<1>(bubble_image)), pixel_ratio, 255);
        if (!drawn) return std::unexpected(std::move(drawn.error()));
      }
      // 255 here because portrait bubbles are always full opacity
      for (auto const& bubble_text : portrait_bubble.bubble_text)
        { auto drawn = draw_bubble_text(screen_pos, bubble_text, pixel_ratio, 255, true); if (!drawn) return std::unexpected(std::move(drawn.error())); }
    }
  }
  return {};
}

std::expected<void, StarException> ChatBubbleManager::add_chat_actions(List<ChatAction> chat_actions, bool silent) {
  float part_size = m_part_size;
  float pixel_ratio = m_gui_context.get().interfaceScale();

  for (auto& action : chat_actions) {
    Json config = JsonObject{};
    Vec2F position;

    if (std::holds_alternative<SayChatAction>(action)) {
      auto& say_action = std::get<SayChatAction>(action);
      auto action_config = say_action.config.opt_object();
      if (!action_config) return std::unexpected(std::move(action_config.error()));
      config = std::move(*action_config).value_or(JsonObject{});
      position = say_action.position;

      TextStyle text_style = m_text_style;
      auto font_size = config.get_uint("fontSize", text_style.font_size);
      if (!font_size)
        return std::unexpected(std::move(font_size.error()));
      text_style.font_size = *font_size;
      auto color_json = config.opt("color");
      if (!color_json)
        return std::unexpected(std::move(color_json.error()));
      if (*color_json) {
        auto color = json_to_color(**color_json);
        if (!color)
          return std::unexpected(std::move(color.error()));
        text_style.color = color->to_rgba();
      }
      auto style_json = config.get("style", Json());
      if (!style_json)
        return std::unexpected(std::move(style_json.error()));
      auto loaded_style = text_style.load_json(*style_json);
      if (!loaded_style)
        return std::unexpected(std::move(loaded_style.error()));
      // Size the bubble using the same final style and wrap width as drawing.
      auto styled = m_gui_context.get().setTextStyle(text_style, pixel_ratio);
      if (!styled) return std::unexpected(std::move(styled.error()));
      TextPositioning tp = scaled_positioning(m_text_template, pixel_ratio);
      auto result = m_gui_context.get().determineTextSize(say_action.text, tp);
      if (!result) return std::unexpected(std::move(result.error()));
      float text_width = result->width() / pixel_ratio + m_text_padding[0];
      float text_height = result->height() / pixel_ratio + m_text_padding[1];

      Vec2I inner_tiles = Vec2I::ceil(Vec2F((text_width + 4) / part_size, (text_height + 4) / part_size));
      if (inner_tiles[0] % 2 == 0)
        inner_tiles[0] += 1;
      if (inner_tiles[0] < 3)
        inner_tiles[0] = 3;
      int middle_idx = (inner_tiles[0] - 1) / 2;

      List<BubbleImage> background_images;
      auto border = config.get_bool("drawBorder", true);
      if (!border) return std::unexpected(std::move(border.error()));
      if (*border) {
        for (int y = 0; y < inner_tiles[1]; y++) {
          for (int x = 0; x < inner_tiles[0]; x++) {
            auto part_position = [part_size](int x, int y) {
              return Vec2F(x * part_size, y * part_size);
            };
            if (y == 0) {
              if (x == 0) {
                background_images.push_back(
                  BubbleImage{"/interface/chatbubbles/cornerBottomLeft.png", part_position(x, y)});
              } else if (x == inner_tiles[0] - 1) {
                background_images.push_back(
                  BubbleImage{"/interface/chatbubbles/cornerBottomRight.png", part_position(x, y)});
              } else {
                if (middle_idx == x)
                  background_images.push_back(BubbleImage{"/interface/chatbubbles/point.png", part_position(x, y - 1)});
                else
                  background_images.push_back(BubbleImage{"/interface/chatbubbles/sideDown.png", part_position(x, y)});
              }
            } else if (y == inner_tiles[1] - 1) {
              if (x == 0)
                background_images.push_back(BubbleImage{"/interface/chatbubbles/cornerTopLeft.png", part_position(x, y)});
              else if (x == inner_tiles[0] - 1)
                background_images.push_back(
                  BubbleImage{"/interface/chatbubbles/cornerTopRight.png", part_position(x, y)});
              else
                background_images.push_back(BubbleImage{"/interface/chatbubbles/sideUp.png", part_position(x, y)});
            } else {
              if (x == 0)
                background_images.push_back(BubbleImage{"/interface/chatbubbles/sideLeft.png", part_position(x, y)});
              else if (x == inner_tiles[0] - 1)
                background_images.push_back(BubbleImage{"/interface/chatbubbles/sideRight.png", part_position(x, y)});
              else
                background_images.push_back(BubbleImage{"/interface/chatbubbles/center.png", part_position(x, y)});
            }
          }
        }
      }

      float text_multi_line_shift = text_height;
      float horizontal_center = part_size * inner_tiles[0] * 0.5f;
      float vertical_shift = (part_size * inner_tiles[1] - text_multi_line_shift) * 0.5f + text_multi_line_shift - 1.0f;
      Vec2F position = Vec2F(horizontal_center, vertical_shift);
      List<BubbleText> bubble_texts;

      bubble_texts.push_back(std::make_tuple(say_action.text, text_style, true, position));

      for (auto& background_image : background_images)
        std::get<1>(background_image) += Vec2F(-horizontal_center, part_size);
      for (auto& bubble_text : bubble_texts)
        std::get<3>(bubble_text) += Vec2F(-horizontal_center, part_size);

      auto pos = m_camera.worldToScreen(say_action.position + m_bubble_offset);
      RectF bound_box = RectF::null();
      for (auto const& image : background_images) {
        auto bounds = bubble_image_rect(pos, image, pixel_ratio);
        if (!bounds) return std::unexpected(std::move(bounds.error()));
        bound_box.combine(*bounds);
      }
      Bubble bubble = {.entity = say_action.entity,
                       .text = std::move(say_action.text),
                       .config = std::move(say_action.config),
                       .age = 0,
                       .background_images = std::move(background_images),
                       .bubble_text = std::move(bubble_texts),
                       .onscreen = false,
                       .instance = {}};
      struct PreviousBubble {
        Vec2F destination;
        RectF bounds;
        Bubble contents;
      };
      List<PreviousBubble> old_bubbles;
      m_bubbles.filter([&](BubbleState<Bubble> const& state, Bubble& previous) {
        if (previous.entity != say_action.entity)
          return true;
        if (m_max_message_per_entity != 1)
          old_bubbles.push_back({state.ideal_destination, state.bound_box, std::move(previous)});
        return false;
      });
      m_bubbles.add_bubble(pos, bound_box, std::move(bubble), m_inter_bubble_margin * pixel_ratio);
      // A nonpositive limit historically used a negative slice stop, relative to the end.
      auto stop = static_cast<std::int64_t>(m_max_message_per_entity) - 1;
      auto size = old_bubbles.size();
      auto keep = stop < 0 ? size - std::min(size, static_cast<std::size_t>(-stop))
                           : std::min(size, static_cast<std::size_t>(stop));
      if (keep != 0) {
        old_bubbles.sort([](PreviousBubble const& a, PreviousBubble const& b) { return a.contents.age < b.contents.age; });
        for (auto& previous : std::span(old_bubbles).first(keep))
          m_bubbles.add_bubble(previous.destination, previous.bounds, std::move(previous.contents), 0);
      }

    } else if (std::holds_alternative<PortraitChatAction>(action)) {
      auto& portrait_action = std::get<PortraitChatAction>(action);
      auto action_config = portrait_action.config.opt_object();
      if (!action_config) return std::unexpected(std::move(action_config.error()));
      config = std::move(*action_config).value_or(JsonObject{});
      position = portrait_action.position;

      List<BubbleImage> background_images;
      background_images.push_back(std::make_tuple(m_portrait_background_image, Vec2F()));
      auto more = config.get_bool("drawMoreIndicator", false);
      if (!more) return std::unexpected(std::move(more.error()));
      if (*more)
        background_images.push_back(std::make_tuple(m_portrait_more_image, Vec2F(m_portrait_more_position)));
      background_images.push_back(std::make_tuple(portrait_action.portrait, Vec2F(m_portrait_position)));
      List<BubbleText> bubble_texts;
      bubble_texts.push_back(std::make_tuple(portrait_action.text, m_text_style, false, Vec2F(m_portrait_text_position)));

      for (auto& background_image : background_images)
        std::get<1>(background_image) += Vec2F(-m_portrait_background_size[0] / 2, 0);
      for (auto& bubble_text : bubble_texts)
        std::get<3>(bubble_text) += Vec2F(-m_portrait_background_size[0] / 2, 0);

      m_portrait_bubbles.push_front({.entity = portrait_action.entity,
                                    .portrait = std::move(portrait_action.portrait),
                                    .text = std::move(portrait_action.text),
                                    .position = portrait_action.position,
                                    .config = std::move(portrait_action.config),
                                    .age = 0,
                                    .background_images = std::move(background_images),
                                    .bubble_text = std::move(bubble_texts),
                                    .onscreen = false,
                                    .instance = {}});
    }

    if (!silent) {
      auto sound = config.opt_string("sound");
      if (!sound) return std::unexpected(std::move(sound.error()));
      if (*sound) {
        auto assets = m_gui_context.get().root().assets();
        if (!assets)
          return std::unexpected(std::move(assets.error()));
        auto audio = (*assets)->audio(**sound);
        if (!audio) return std::unexpected(std::move(audio.error()));
        auto instance = AudioInstance::create(**audio);
        if (!instance) return std::unexpected(std::move(instance.error()));
        auto audio_instance = std::move(*instance);
        audio_instance->set_position(position);
        auto volume = config.get_float("volume", 1.0f);
        if (!volume) return std::unexpected(std::move(volume.error()));
        auto pitch = config.get_float("pitch", 1.0f);
        if (!pitch) return std::unexpected(std::move(pitch.error()));
        audio_instance->set_volume(*volume);
        audio_instance->set_pitch_multiplier(*pitch);
        auto played = m_gui_context.get().playAudio(audio_instance);
        if (!played) return std::unexpected(std::move(played.error()));
      }
    }
  }
  return {};
}

std::expected<RectF, StarException> ChatBubbleManager::bubble_image_rect(Vec2F screen_pos, BubbleImage const& image, float pixel_ratio) {
  auto metadata = m_gui_context.get().root().imageMetadataDatabase();
  if (!metadata) return std::unexpected(std::move(metadata.error()));
  auto size = (*metadata)->imageSize(std::get<0>(image));
  if (!size) return std::unexpected(std::move(size.error()));
  return RectF::with_size(screen_pos + std::get<1>(image) * pixel_ratio, Vec2F(*size) * pixel_ratio);
}

std::expected<void, StarException> ChatBubbleManager::draw_bubble_image(Vec2F screen_pos, BubbleImage const& image, float pixel_ratio, int alpha) {
  return m_gui_context.get().drawQuad(std::get<0>(image), screen_pos + std::get<1>(image) * pixel_ratio, pixel_ratio, {255, 255, 255, alpha});
}

std::expected<void, StarException> ChatBubbleManager::draw_bubble_text(Vec2F screen_pos, BubbleText const& text, float pixel_ratio, int alpha, bool is_portrait) {
  TextStyle style = std::get<1>(text);
  style.color[3] *= (static_cast<float>(alpha) / 255.f);
  auto styled = m_gui_context.get().setTextStyle(style, pixel_ratio);
  if (!styled) return std::unexpected(std::move(styled.error()));
  TextPositioning tp = is_portrait ? m_portrait_text_template : m_text_template;
  tp.pos = screen_pos + std::get<3>(text) * pixel_ratio;
  tp = scaled_positioning(tp, pixel_ratio);
  auto rendered = m_gui_context.get().renderText(std::get<0>(text), tp);
  if (!rendered) return std::unexpected(std::move(rendered.error()));
  return {};
}

TextPositioning ChatBubbleManager::scaled_positioning(TextPositioning positioning, float pixel_ratio) const {
  positioning.wrapWidth =
    positioning.wrapWidth.transform([pixel_ratio](unsigned width) { return static_cast<unsigned>(width * pixel_ratio); });
  return positioning;
}
}

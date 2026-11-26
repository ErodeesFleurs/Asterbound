import star.json;
import star.json_extra;
import star.id_map;
import star.casting;
import star.vector;
import star.data_stream;
import star.game_types;
import star.message_types;
import star.poly;
import star.strong_typedef;
import star.bi_map;
import star.color;
import star.list;
import star.image_processing;
import star.hash;
import star.string_view;
import star.thread;
import star.directives;
import star.asset_path;
import star.rpc_promise;
import star.a_star;
import std;
import star.format;
import star.lua;
import star.data_stream_extra;
import star.set;
import star.string;
import star.random;
import star.periodic_function;
import star.ordered_map;
import star.audio;
import star.map;
import star.io_device;
import star.ref_ptr;
import star.logging;
import star.listener;
import star.ordered_set;


import star.lua_root;
import star.lua_components;
import star.lua_actor_movement_component;
import star.lua_animation_component;
import star.drawable;
import star.item;
import star.damage_types;
import star.world_geometry;
import star.status_types;
import star.damage;
import star.light_source;
import star.entity;
import star.animation;
import star.particle;
import star.interaction_types;
import star.tile_damage;
import star.item_descriptor;
import star.celestial_coordinate;
import star.quest_descriptor;
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
import star.mobile_entity;
import star.actor_entity;
import star.tool_user_entity;
import star.tool_user_item;
import star.animated_part_set;
import star.mixer;
import star.networked_animator;
import star.durability_item;
import star.asset_source;
import star.assets;
import star.root_base;
import star.configuration;
import star.root;
import star.uuid;
import star.humanoid;
import star.lounging_entities;
import star.chat_action;
import star.chatty_entity;
import star.portrait_entity;
import star.damage_bar_entity;
import star.nametag_entity;
import star.inspectable_entity;
import star.inventory_types;
import star.ai_types;
import star.entity_rendering_types;
import star.entity_rendering;
import star.player_types;
import star.radio_message_database;
import star.player;
import star.emote_entity;

import star.active_item;
import star.networked_animator_lua_bindings;
import star.item_lua_bindings;
import star.status_controller_lua_bindings;
import star.player_lua_bindings;
import star.entity_lua_bindings;
import star.scripted_animator_lua_bindings;
import star.config_lua_bindings;

import star.line;
import star.math_common;
import star.algorithm;
namespace Star {
namespace {
std::expected<Vec2F, StarException> active_hand_position(ToolUserEntity const& owner, ToolHand hand, Direction facing,
                                                   float angle, Vec2F const& offset) {
  auto hand_offset = owner.hand_offset(hand, facing);
  if (!hand_offset)
    return std::unexpected(std::move(hand_offset.error()));
  return owner.arm_position(hand, facing, angle, offset + *hand_offset);
}
}


std::expected<std::shared_ptr<ActiveItem>, StarException> ActiveItem::create(Root& root, Json const& config, String const& directory, Json const& parameters) {
  auto item = std::shared_ptr<ActiveItem>(new ActiveItem(root));
  auto initialized = item->initialize(config, directory, parameters);
  if (!initialized)
    return std::unexpected(std::move(initialized.error()));
  auto assets = root.assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  auto animationValue = item->instance_value("animation");
  if (!animationValue)
    return std::unexpected(std::move(animationValue.error()));
  auto animationConfig = (*assets)->fetch_json(*animationValue, directory);
  if (!animationConfig)
    return std::unexpected(std::move(animationConfig.error()));
  auto custom = item->instance_value("animationCustom");
  if (!custom)
    return std::unexpected(std::move(custom.error()));
  if (*custom)
    animationConfig = json_merge(*animationConfig, *custom);
  auto animator = NetworkedAnimator::create(root, *animationConfig, directory);
  if (!animator)
    return std::unexpected(std::move(animator.error()));
  item->m_itemAnimator = std::move(*animator);
  auto parts = item->instance_value("animationParts", JsonObject()).and_then([](Json const& value) {
    return value.iterate_object();
  });
  if (!parts)
    return std::unexpected(std::move(parts.error()));
  for (auto const& pair : *parts) {
    auto part = pair.second.to_string();
    if (!part)
      return std::unexpected(std::move(part.error()));
    item->m_itemAnimator->set_part_tag(pair.first, "partImage", *part);
  }
  auto animationParameters = config.get_object("scriptedAnimationParameters", {});
  if (!animationParameters)
    return std::unexpected(std::move(animationParameters.error()));
  item->m_scriptedAnimationParameters.reset(std::move(*animationParameters));
  for (NetElement& element : std::initializer_list<std::reference_wrapper<NetElement>>{
      *item->m_itemAnimator,
      item->m_holdingItem, item->m_backArmFrame, item->m_frontArmFrame,
      item->m_twoHandedGrip, item->m_recoil, item->m_outsideOfHand,
      item->m_armAngle, item->m_facingDirection, item->m_damageSources,
      item->m_itemDamageSources, item->m_shieldPolys, item->m_itemShieldPolys,
      item->m_force_regions, item->m_item_force_regions}) {
    auto added = item->add_net_element(element);
    if (!added)
      return std::unexpected(std::move(added.error()));
  }
  auto parametersAdded = item->add_net_element(item->m_scriptedAnimationParameters, false);
  if (!parametersAdded)
    return std::unexpected(std::move(parametersAdded.error()));
  item->m_holdingItem.set(true);
  item->m_armAngle.set_fixed_point_base(0.01f);
  return item;
}

std::expected<std::shared_ptr<Item>, StarException> ActiveItem::clone() const {
  return create(root(), config(), directory(), parameters()).transform([](std::shared_ptr<ActiveItem> item) -> std::shared_ptr<Item> { return item; });
}

std::expected<void, StarException> ActiveItem::init(ToolUserEntity& owner, ToolHand hand) {
  auto initialized = ToolUserItem::init(owner, hand);
  if (!initialized)
    return std::unexpected(std::move(initialized.error()));
  auto mode = entityMode();
  if (!mode)
    return std::unexpected(std::move(mode.error()));
  if (*mode == EntityMode::Master) {
    auto scripts = instance_value("scripts").and_then(json_to_string_list);
    if (!scripts)
      return std::unexpected(std::move(scripts.error()));
    auto configured = m_script.set_scripts(scripts->transformed(
      [directory = directory()](String const& script) { return AssetPath::relative_to(directory, script); }));
    if (!configured)
      return std::unexpected(std::move(configured.error()));
    auto delta = instance_value("scriptDelta", 1).and_then([](Json const& value) { return value.to_uint(); });
    if (!delta)
      return std::unexpected(std::move(delta.error()));
    m_script.set_update_delta(*delta);
    m_twoHandedGrip.set(two_handed());
    auto previousStorage = instance_value("scriptStorage");
    if (!previousStorage)
      return std::unexpected(std::move(previousStorage.error()));
    if (*previousStorage) {
      auto stored = previousStorage->to_object().and_then([&](JsonObject storage) { return m_script.set_script_storage(std::move(storage)); });
      if (!stored)
        return std::unexpected(std::move(stored.error()));
    }
    auto activeCallbacks = makeActiveItemCallbacks().and_then([&](LuaCallbacks callbacks) { return m_script.add_callbacks("activeItem", std::move(callbacks)); });
    if (!activeCallbacks)
      return std::unexpected(std::move(activeCallbacks.error()));
    auto item_callbacks = LuaBindings::make_item_callbacks(*this).and_then([&](LuaCallbacks callbacks) { return m_script.add_callbacks("item", std::move(callbacks)); });
    if (!item_callbacks)
      return std::unexpected(std::move(item_callbacks.error()));
    auto configCallbacks =
      LuaBindings::makeConfigCallbacks([this](String const& name,
                                              Json const& defaultValue) -> std::expected<Json, StarException> {
        return instance_value(name, defaultValue);
      }).and_then([&](LuaCallbacks callbacks) { return m_script.add_callbacks("config", std::move(callbacks)); });
    if (!configCallbacks)
      return std::unexpected(std::move(configCallbacks.error()));
    auto animatorCallbacks = LuaBindings::make_networked_animator_callbacks(*m_itemAnimator).and_then([&](LuaCallbacks callbacks) { return m_script.add_callbacks("animator", std::move(callbacks)); });
    if (!animatorCallbacks)
      return std::unexpected(std::move(animatorCallbacks.error()));
    auto statusCallbacks = LuaBindings::makeStatusControllerCallbacks(owner.status_controller()).and_then([&](LuaCallbacks callbacks) { return m_script.add_callbacks("status", std::move(callbacks)); });
    if (!statusCallbacks)
      return std::unexpected(std::move(statusCallbacks.error()));
    auto movementCallbacks = m_script.add_actor_movement_callbacks(owner.movement_controller());
    if (!movementCallbacks)
      return std::unexpected(std::move(movementCallbacks.error()));
    if (auto player = as_ref<Player>(owner)) {
      auto playerCallbacks = LuaBindings::makePlayerCallbacks(root(), player->get()).and_then([&](LuaCallbacks callbacks) { return m_script.add_callbacks("player", std::move(callbacks)); });
      if (!playerCallbacks)
        return std::unexpected(std::move(playerCallbacks.error()));
    }
    auto entityCallbacks = LuaBindings::makeEntityCallbacks(owner).and_then([&](LuaCallbacks callbacks) { return m_script.add_callbacks("entity", std::move(callbacks)); });
    if (!entityCallbacks)
      return std::unexpected(std::move(entityCallbacks.error()));
    auto itemWorld = world();
    if (!itemWorld)
      return std::unexpected(std::move(itemWorld.error()));
    auto scriptInitialized = recover_lua_script_error(m_script.init(itemWorld->get()));
    if (!scriptInitialized)
      return std::unexpected(std::move(scriptInitialized.error()));
    m_currentFireMode = FireMode::None;
  }
  auto itemWorld = world();
  if (!itemWorld)
    return std::unexpected(std::move(itemWorld.error()));
  if (itemWorld->get().isClient()) {
    auto animationScripts = instance_value("animationScripts");
    if (!animationScripts)
      return std::unexpected(std::move(animationScripts.error()));
    if (*animationScripts) {
      auto scripts = json_to_string_list(*animationScripts);
      if (!scripts)
        return std::unexpected(std::move(scripts.error()));
      auto configured = m_scriptedAnimator.set_scripts(scripts->transformed(
        [directory = directory()](String const& script) { return AssetPath::relative_to(directory, script); }));
      if (!configured)
        return std::unexpected(std::move(configured.error()));
      auto delta = instance_value("animationDelta", 1).and_then([](Json const& value) { return value.to_uint(); });
      if (!delta)
        return std::unexpected(std::move(delta.error()));
      m_scriptedAnimator.set_update_delta(*delta);
      auto animationCallbacks = LuaBindings::make_scripted_animator_callbacks(*m_itemAnimator,
          [this](String const& name, Json const& defaultValue) -> Json { return m_scriptedAnimationParameters.value(name, defaultValue); })
          .and_then([&](LuaCallbacks callbacks) { return m_scriptedAnimator.add_callbacks("animationConfig", std::move(callbacks)); });
      if (!animationCallbacks)
        return std::unexpected(std::move(animationCallbacks.error()));
      auto activeCallbacks = makeScriptedAnimationCallbacks().and_then([&](LuaCallbacks callbacks) { return m_scriptedAnimator.add_callbacks("activeItemAnimation", std::move(callbacks)); });
      if (!activeCallbacks)
        return std::unexpected(std::move(activeCallbacks.error()));
      auto configCallbacks =
        LuaBindings::makeConfigCallbacks([this](String const& name,
                                                Json const& defaultValue) -> std::expected<Json, StarException> {
          return instance_value(name, defaultValue);
        }).and_then([&](LuaCallbacks callbacks) {
          return m_scriptedAnimator.add_callbacks("config", std::move(callbacks));
        });
      if (!configCallbacks)
        return std::unexpected(std::move(configCallbacks.error()));
      auto animationInitialized = recover_lua_script_error(m_scriptedAnimator.init(itemWorld->get()));
      if (!animationInitialized)
        return std::unexpected(std::move(animationInitialized.error()));
    }
  }
  return {};
}

std::expected<void, StarException> ActiveItem::uninit() {
  std::optional<StarException> error;
  auto remember = [&](auto result) {
    if (!result) {
      if (error)
        *error = error->with_context(result.error().what());
      else
        error = std::move(result.error());
    }
  };
  auto mode = entityMode();
  if (!mode) {
    remember(std::move(mode));
  } else if (*mode == EntityMode::Master) {
    remember(recover_lua_script_error(m_script.uninit()));
    for (auto const& name : std::initializer_list<std::string_view>{"activeItem", "item", "config", "animator", "status"})
      remember(m_script.remove_callbacks(name));
    remember(m_script.remove_actor_movement_callbacks());
    for (auto const& name : std::initializer_list<std::string_view>{"player", "entity"})
      remember(m_script.remove_callbacks(name));
  }
  auto itemWorld = world();
  if (!itemWorld) {
    remember(std::move(itemWorld));
  } else if (itemWorld->get().isClient()) {
    auto animationScripts = instance_value("animationScripts");
    if (!animationScripts) {
      remember(std::move(animationScripts));
    } else if (*animationScripts) {
      remember(recover_lua_script_error(m_scriptedAnimator.uninit()));
      for (auto const& name : std::initializer_list<std::string_view>{"animationConfig", "activeItemAnimation", "config"})
        remember(m_scriptedAnimator.remove_callbacks(name));
    }
  }
  m_itemAnimatorDynamicTarget.stop_audio();
  remember(ToolUserItem::uninit());
  m_activeAudio.clear();
  if (error)
    return std::unexpected(std::move(*error));
  return {};
}

std::expected<void, StarException> ActiveItem::update(float dt, FireMode fireMode, bool shifting, HashSet<MoveControlType> const& moves) {
  StringMap<bool> moveMap;
  for (auto move : moves) {
    auto name = enum_name<move_control_type_names>(move);
    if (!name)
      return std::unexpected(std::move(name.error()));
    moveMap[String(*name)] = true;
  }
  auto mode = entityMode();
  if (!mode)
    return std::unexpected(std::move(mode.error()));
  if (*mode == EntityMode::Master) {
    auto fireModeName = enum_name<fire_mode_names>(fireMode);
    if (!fireModeName)
      return std::unexpected(std::move(fireModeName.error()));
    if (fireMode != m_currentFireMode) {
      m_currentFireMode = fireMode;
      if (fireMode != FireMode::None) {
        auto activated = recover_lua_script_error(m_script.invoke("activate", *fireModeName, shifting, moveMap));
        if (!activated)
          return std::unexpected(std::move(activated.error()));
      }
    }
    auto updated = recover_lua_script_error(m_script.update(m_script.update_dt(dt), *fireModeName, shifting, moveMap));
    if (!updated)
      return std::unexpected(std::move(updated.error()));
    auto retain = instance_value("retainScriptStorageInItem", false).and_then([](Json const& value) { return value.to_bool(); });
    if (!retain)
      return std::unexpected(std::move(retain.error()));
    if (*retain) {
      auto storage = m_script.get_script_storage();
      if (!storage)
        return std::unexpected(std::move(storage.error()));
      auto stored = set_instance_value("scriptStorage", *storage);
      if (!stored)
        return std::unexpected(std::move(stored.error()));
    }
  }
  auto itemWorld = world();
  if (!itemWorld)
    return std::unexpected(std::move(itemWorld.error()));
  bool isClient = itemWorld->get().isClient();
  auto animated = m_itemAnimator->update(dt, isClient ? std::optional{std::ref(m_itemAnimatorDynamicTarget)} : std::nullopt);
  if (!animated)
    return std::unexpected(std::move(animated.error()));
  if (isClient) {
    auto updated = recover_lua_script_error(m_scriptedAnimator.update(m_scriptedAnimator.update_dt(dt)));
    if (!updated)
      return std::unexpected(std::move(updated.error()));
  }
  for (auto it = m_activeAudio.begin(); it != m_activeAudio.end();) {
    auto itemOwner = owner();
    if (!itemOwner)
      return std::unexpected(std::move(itemOwner.error()));
    auto position = handPosition(it->second);
    if (!position)
      return std::unexpected(std::move(position.error()));
    it->first->set_position(itemOwner->get().position() + *position);
    if (it->first->finished())
      it = m_activeAudio.erase(it);
    else
      ++it;
  }
  auto shields = shieldPolys();
  if (!shields)
    return std::unexpected(std::move(shields.error()));
  for (auto shieldPoly : *shields) {
    auto itemOwner = owner();
    if (!itemOwner)
      return std::unexpected(std::move(itemOwner.error()));
    shieldPoly.translate(itemOwner->get().position());
    if (isClient)
      root().spatialLogger().log_poly("world", shieldPoly, {255, 255, 0, 255});
  }
  if (isClient && root().spatialLogger().observed()) {
    auto forces = force_regions();
    if (!forces)
      return std::unexpected(std::move(forces.error()));
    auto& logger = root().spatialLogger();
    for (auto const& force : *forces) {
      std::visit([&]<physics_force_region Region>(Region const& region) {
        if constexpr (std::same_as<Region, RadialForceRegion>) {
          logger.log_point("world", region.center, {155, 0, 255, 255});
        } else {
          logger.log_poly("world", region.region, {155, 0, 255, 255});
          if constexpr (std::same_as<Region, GradientForceRegion>)
            logger.log_line("world", region.gradient, {155, 0, 255, 255});
        }
      }, force);
    }
  }
  return {};
}

std::expected<List<DamageSource>, StarException> ActiveItem::damageSources() const {
  List<DamageSource> damageSources = m_damageSources.get();
  for (auto ds : m_itemDamageSources.get()) {
    if (std::holds_alternative<PolyF>(ds.damage_area)) {
      auto& poly = std::get<PolyF>(ds.damage_area);
      poly.rotate(m_armAngle.get());
      auto itemOwner = owner();
      if (!itemOwner)
        return std::unexpected(std::move(itemOwner.error()));
      if (itemOwner->get().facing_direction() == Direction::Left)
        poly.flip_horizontal(0.0f);
      auto position = handPosition(Vec2F());
      if (!position)
        return std::unexpected(std::move(position.error()));
      poly.translate(*position);
    } else if (std::holds_alternative<Line2F>(ds.damage_area)) {
      auto& line = std::get<Line2F>(ds.damage_area);
      line.rotate(m_armAngle.get());
      auto itemOwner = owner();
      if (!itemOwner)
        return std::unexpected(std::move(itemOwner.error()));
      if (itemOwner->get().facing_direction() == Direction::Left)
        line.flip_horizontal(0.0f);
      auto position = handPosition(Vec2F());
      if (!position)
        return std::unexpected(std::move(position.error()));
      line.translate(*position);
    }
    damageSources.push_back(std::move(ds));
  }
  return damageSources;
}

std::expected<List<PolyF>, StarException> ActiveItem::shieldPolys() const {
  List<PolyF> shieldPolys = m_shieldPolys.get();
  for (auto sp : m_itemShieldPolys.get()) {
    sp.rotate(m_armAngle.get());
    auto itemOwner = owner();
    if (!itemOwner)
      return std::unexpected(std::move(itemOwner.error()));
    if (itemOwner->get().facing_direction() == Direction::Left)
      sp.flip_horizontal(0.0f);
    auto position = handPosition(Vec2F());
    if (!position)
      return std::unexpected(std::move(position.error()));
    sp.translate(*position);
    shieldPolys.push_back(std::move(sp));
  }
  return shieldPolys;
}

std::expected<List<PhysicsForceRegion>, StarException> ActiveItem::force_regions() const {
  auto const& local = m_item_force_regions.get();
  auto const& world = m_force_regions.get();
  if (local.empty())
    return world;

  auto item_owner = owner();
  if (!item_owner)
    return std::unexpected(std::move(item_owner.error()));
  auto item_hand = hand();
  if (!item_hand)
    return std::unexpected(std::move(item_hand.error()));
  auto const& actor = item_owner->get();
  auto facing = actor.facing_direction();
  float angle = m_armAngle.get();
  auto hand_offset = active_hand_position(actor, *item_hand, facing, angle, {});
  if (!hand_offset)
    return std::unexpected(std::move(hand_offset.error()));
  auto origin = actor.position() + *hand_offset;
  bool mirror = facing == Direction::Left;
  float cosine = std::cos(angle), sine = std::sin(angle);
  auto transform = [&](Vec2F point) {
    float x = point[0] * cosine - point[1] * sine;
    return origin + Vec2F(mirror ? -x : x, point[0] * sine + point[1] * cosine);
  };

  List<PhysicsForceRegion> forces;
  forces.reserve(world.size() + local.size());
  forces.append_range(world);
  for (auto const& value : local) {
    auto& force = forces.emplace_back(value);
    std::visit([&]<physics_force_region Region>(Region& region) {
      if constexpr (std::same_as<Region, RadialForceRegion>) {
        region.center = transform(region.center);
      } else {
        for (auto& vertex : region.region)
          vertex = transform(vertex);
        if (mirror)
          std::ranges::reverse(region.region);
        if constexpr (std::same_as<Region, GradientForceRegion>) {
          region.gradient.min() = transform(region.gradient.min());
          region.gradient.max() = transform(region.gradient.max());
        } else {
          static_assert(std::same_as<Region, DirectionalForceRegion>);
        }
      }
    }, force);
  }
  return forces;
}

bool ActiveItem::holdingItem() const {
  return m_holdingItem.get();
}

std::optional<String> ActiveItem::backArmFrame() const {
  return m_backArmFrame.get();
}

std::optional<String> ActiveItem::frontArmFrame() const {
  return m_frontArmFrame.get();
}

bool ActiveItem::twoHandedGrip() const {
  return m_twoHandedGrip.get();
}

bool ActiveItem::recoil() const {
  return m_recoil.get();
}

bool ActiveItem::outsideOfHand() const {
  return m_outsideOfHand.get();
}

float ActiveItem::armAngle() const {
  return m_armAngle.get();
}

std::optional<Direction> ActiveItem::facingDirection() const {
  return m_facingDirection.get();
}

std::expected<List<Drawable>, StarException> ActiveItem::handDrawables(float interpolation) const {
  auto sample = m_itemAnimator->sample(interpolation);
  if (m_itemAnimator->const_parts().empty()) {
    auto drawables = Item::icon_drawables();
    if (!drawables)
      return std::unexpected(std::move(drawables.error()));
    Drawable::scale_all(*drawables, 1.0f / tile_pixels);
    return drawables;
  } else {
    return sample.drawables();
  }
}

std::expected<List<std::pair<Drawable, std::optional<EntityRenderLayer>>>, StarException> ActiveItem::entityDrawables() const {
  return m_scriptedAnimator.drawables();
}

std::expected<void, StarException> ActiveItem::append_light_sources(List<LightSource>& lights, float interpolation) const {
  auto first_light = lights.size();
  auto rollback = make_scope_exit([&] { lights.resize(first_light); });
  auto sample = m_itemAnimator->sample(interpolation);
  if (auto result = sample.append_light_sources(lights); !result)
    return std::unexpected(std::move(result.error()));

  // Like audio, only the item animator's output starts in hand space.
  for (auto i = first_light; i < lights.size(); ++i) {
    auto& light = lights[i];
    auto item_owner = owner();
    if (!item_owner)
      return std::unexpected(std::move(item_owner.error()));
    auto offset = handPosition(light.position);
    if (!offset)
      return std::unexpected(std::move(offset.error()));
    light.position = item_owner->get().position() + *offset;
    light.beam_angle += m_armAngle.get();
    if (item_owner->get().facing_direction() == Direction::Left) {
      if (light.beam_angle > 0)
        light.beam_angle = std::numbers::pi / 2 + normalize_angle(std::numbers::pi / 2 - light.beam_angle);
      else
        light.beam_angle = -std::numbers::pi / 2 - normalize_angle(light.beam_angle + std::numbers::pi / 2);
    }
  }
  lights.append_range(m_scriptedAnimator.light_sources());
  rollback.release();
  return {};
}

std::expected<List<std::shared_ptr<AudioInstance>>, StarException> ActiveItem::pullNewAudios() {
  // Because the item animator is in hand-space, and Humanoid does all the
  // translation *and flipping*, we cannot use NetworkedAnimator's built-in
  // functionality to rotate and flip, and instead must do it manually.  We do
  // not call animatorTarget.setPosition, and keep track of running audio
  // ourselves.  It would be easier if (0, 0) for the NetworkedAnimator was,
  // say, the shoulder and un-rotated, but it gets a bit weird with Humanoid
  // modifications.
  List<std::shared_ptr<AudioInstance>> result;
  for (auto& audio : m_itemAnimatorDynamicTarget.pull_new_audios()) {
    m_activeAudio[audio] = *audio->position();
    auto itemOwner = owner();
    if (!itemOwner)
      return std::unexpected(std::move(itemOwner.error()));
    auto position = handPosition(*audio->position());
    if (!position)
      return std::unexpected(std::move(position.error()));
    audio->set_position(itemOwner->get().position() + *position);
    result.push_back(std::move(audio));
  }
  result.append_range(m_scriptedAnimator.pull_new_audios());
  return result;
}

std::expected<List<Particle>, StarException> ActiveItem::pullNewParticles() {
  // Same as pullNewAudios, we translate, rotate, and flip ourselves
  List<Particle> result;
  for (auto& particle : m_itemAnimatorDynamicTarget.pull_new_particles()) {
    auto itemOwner = owner();
    if (!itemOwner)
      return std::unexpected(std::move(itemOwner.error()));
    auto position = handPosition(particle.position);
    if (!position)
      return std::unexpected(std::move(position.error()));
    particle.position = itemOwner->get().position() + *position;
    particle.velocity = particle.velocity.rotate(m_armAngle.get());
    if (itemOwner->get().facing_direction() == Direction::Left) {
      particle.velocity[0] *= -1;
      particle.flip = !particle.flip;
    }
    result.push_back(std::move(particle));
  }
  result.append_range(m_scriptedAnimator.pull_new_particles());
  return result;
}

std::optional<String> ActiveItem::cursor() const {
  return m_cursor;
}

MessageResponseResult ActiveItem::receive_message(String const& message, bool local_message, JsonArray const& args) {
  return recover_lua_script_error(m_script.handle_message(message, local_message, args));
}

std::expected<float, StarException> ActiveItem::durabilityStatus() {
  auto durability = instance_value("durability").and_then([](Json const& value) { return value.opt_float(); });
  if (!durability)
    return std::unexpected(std::move(durability.error()));
  if (*durability) {
    auto durabilityHit = instance_value("durabilityHit").and_then([](Json const& value) { return value.opt_float(); });
    if (!durabilityHit)
      return std::unexpected(std::move(durabilityHit.error()));
    return durabilityHit->value_or(**durability) / **durability;
  }
  return 1.0f;
}

std::expected<Vec2F, StarException> ActiveItem::armPosition(Vec2F const& offset) const {
  auto itemOwner = owner();
  if (!itemOwner)
    return std::unexpected(std::move(itemOwner.error()));
  auto itemHand = hand();
  if (!itemHand)
    return std::unexpected(std::move(itemHand.error()));
  return itemOwner->get().arm_position(*itemHand, itemOwner->get().facing_direction(), m_armAngle.get(), offset);
}

std::expected<Vec2F, StarException> ActiveItem::handPosition(Vec2F const& offset) const {
  auto item_owner = owner();
  if (!item_owner)
    return std::unexpected(std::move(item_owner.error()));
  auto item_hand = hand();
  if (!item_hand)
    return std::unexpected(std::move(item_hand.error()));
  return active_hand_position(item_owner->get(), *item_hand, item_owner->get().facing_direction(), m_armAngle.get(), offset);
}

std::expected<LuaCallbacks, StarException> ActiveItem::makeActiveItemCallbacks() {
  LuaCallbacks callbacks;
  if (auto added = callbacks.register_callback("ownerEntityId", [this]() {
      return owner().transform([](std::reference_wrapper<ToolUserEntity> owner) { return owner.get().entityId(); });
    }); !added)
    return std::unexpected(std::move(added.error()));
  if (auto added = callbacks.register_callback("ownerTeam", [this]() {
      return owner().and_then([](std::reference_wrapper<ToolUserEntity> owner) { return owner.get().getTeam().to_json(); });
    }); !added)
    return std::unexpected(std::move(added.error()));
  if (auto added = callbacks.register_callback("ownerAimPosition", [this]() {
      return owner().and_then([](std::reference_wrapper<ToolUserEntity> owner) { return owner.get().aim_position(); });
    }); !added)
    return std::unexpected(std::move(added.error()));
  if (auto added = callbacks.register_callback("ownerPowerMultiplier", [this]() {
      return owner().and_then([](std::reference_wrapper<ToolUserEntity> owner) { return owner.get().power_multiplier(); });
    }); !added)
    return std::unexpected(std::move(added.error()));
  if (auto added = callbacks.register_callback(
        "fireMode",
        [this]() -> std::expected<String, StarException> {
          return enum_name<fire_mode_names>(m_currentFireMode).transform([](std::string_view name) {
            return String(name);
          });
        });
      !added)
    return std::unexpected(std::move(added.error()));
  if (auto added = callbacks.register_callback("hand", [this]() -> std::expected<String, StarException> {
      auto itemHand = hand();
      if (!itemHand)
        return std::unexpected(std::move(itemHand.error()));
      return enum_name<tool_hand_names>(*itemHand).transform([](std::string_view name) { return String(name); });
    }); !added)
    return std::unexpected(std::move(added.error()));
  if (auto added = callbacks.register_callback("handPosition", [this](std::optional<Vec2F> offset) {
      return handPosition(offset.value_or(Vec2F{}));
    }); !added)
    return std::unexpected(std::move(added.error()));

  // Aim a barrel with a vertical offset from the hand at the given target.
  if (auto added = callbacks.register_callback("aimAngleAndDirection", [this](float aim_vertical_offset, Vec2F target_position) -> std::expected<LuaTupleReturn<float, int>, StarException> {
      auto item_owner = owner();
      if (!item_owner)
        return std::unexpected(std::move(item_owner.error()));
      auto item_hand = hand();
      if (!item_hand)
        return std::unexpected(std::move(item_hand.error()));
      auto rotation_center = item_owner->get().arm_position(*item_hand, item_owner->get().facing_direction(), 0.0f, Vec2F());
      if (!rotation_center)
        return std::unexpected(std::move(rotation_center.error()));
      Vec2F hand_rotation_center = *rotation_center;
      Vec2F owner_position = item_owner->get().position();
      auto item_world = item_owner->get().world();
      if (!item_world)
        return std::unexpected(std::move(item_world.error()));
      Vec2F to_target = item_world->get().geometry().diff(target_position, owner_position);
      // Keep the target beyond the hand's rotation center to avoid rapid flips.
      auto dir = numerical_direction(item_owner->get().facing_direction());
      auto target_x = to_target[0] * dir;
      auto center_x = hand_rotation_center[0] * dir;
      if (center_x >= 0 && target_x >= 0 && target_x < center_x)
        to_target[0] = hand_rotation_center[0] + (0.125 * dir);
      to_target -= hand_rotation_center;
      float to_target_dist = to_target.magnitude();
      // Clamp targets inside the barrel-offset circle to retain continuity.
      float angle_adjust = -std::asin(std::clamp(aim_vertical_offset / to_target_dist, -1.0f, 1.0f));
      auto angle_side = get_angle_side(to_target.angle());
      return lua_tuple_return(angle_side.first + angle_adjust, numerical_direction(angle_side.second));
    }); !added)
    return std::unexpected(std::move(added.error()));
  if (auto added = callbacks.register_callback("aimAngle", [this](float aim_vertical_offset, Vec2F target_position) -> std::expected<float, StarException> {
      auto item_owner = owner();
      if (!item_owner)
        return std::unexpected(std::move(item_owner.error()));
      auto item_hand = hand();
      if (!item_hand)
        return std::unexpected(std::move(item_hand.error()));
      auto rotation_center = item_owner->get().arm_position(*item_hand, item_owner->get().facing_direction(), 0.0f, Vec2F());
      if (!rotation_center)
        return std::unexpected(std::move(rotation_center.error()));
      Vec2F hand_rotation_center = *rotation_center;
      Vec2F owner_position = item_owner->get().position();
      auto item_world = item_owner->get().world();
      if (!item_world)
        return std::unexpected(std::move(item_world.error()));
      Vec2F to_target = item_world->get().geometry().diff(target_position, owner_position + hand_rotation_center);
      float to_target_dist = to_target.magnitude();
      float angle_adjust = -std::asin(std::clamp(aim_vertical_offset / to_target_dist, -1.0f, 1.0f));
      return to_target.angle() + angle_adjust;
    }); !added)
    return std::unexpected(std::move(added.error()));
  if (auto added = callbacks.register_callback("setHoldingItem", [this](bool value) { m_holdingItem.set(value); }); !added)
    return std::unexpected(std::move(added.error()));
  if (auto added = callbacks.register_callback("setBackArmFrame", [this](std::optional<String> value) { m_backArmFrame.set(value); }); !added)
    return std::unexpected(std::move(added.error()));
  if (auto added = callbacks.register_callback("setFrontArmFrame", [this](std::optional<String> value) { m_frontArmFrame.set(value); }); !added)
    return std::unexpected(std::move(added.error()));
  if (auto added = callbacks.register_callback("setTwoHandedGrip", [this](bool value) { m_twoHandedGrip.set(value); }); !added)
    return std::unexpected(std::move(added.error()));
  if (auto added = callbacks.register_callback("setRecoil", [this](bool value) { m_recoil.set(value); }); !added)
    return std::unexpected(std::move(added.error()));
  if (auto added = callbacks.register_callback("setOutsideOfHand", [this](bool value) { m_outsideOfHand.set(value); }); !added)
    return std::unexpected(std::move(added.error()));
  if (auto added = callbacks.register_callback("setArmAngle", [this](float value) { m_armAngle.set(value); }); !added)
    return std::unexpected(std::move(added.error()));
  if (auto added = callbacks.register_callback("setFacingDirection", [this](float value) { m_facingDirection.set(direction_of(value)); }); !added)
    return std::unexpected(std::move(added.error()));
  if (auto added = callbacks.register_callback("setDamageSources", [this](std::optional<JsonArray> const& values) -> std::expected<void, StarException> {
      List<DamageSource> sources;
      if (values) {
        for (auto const& value : *values) {
          auto source = DamageSource::create(value);
          if (!source)
            return std::unexpected(std::move(source.error()));
          sources.push_back(std::move(*source));
        }
      }
      m_damageSources.set(std::move(sources));
      return {};
    }); !added)
    return std::unexpected(std::move(added.error()));
  if (auto added = callbacks.register_callback("setItemDamageSources", [this](std::optional<JsonArray> const& values) -> std::expected<void, StarException> {
      List<DamageSource> sources;
      if (values) {
        for (auto const& value : *values) {
          auto source = DamageSource::create(value);
          if (!source)
            return std::unexpected(std::move(source.error()));
          sources.push_back(std::move(*source));
        }
      }
      m_itemDamageSources.set(std::move(sources));
      return {};
    }); !added)
    return std::unexpected(std::move(added.error()));
  if (auto added = callbacks.register_callback("setShieldPolys", [this](std::optional<List<PolyF>> const& value) { m_shieldPolys.set(value.value_or(List<PolyF>{})); }); !added)
    return std::unexpected(std::move(added.error()));
  if (auto added = callbacks.register_callback("setItemShieldPolys", [this](std::optional<List<PolyF>> const& value) { m_itemShieldPolys.set(value.value_or(List<PolyF>{})); }); !added)
    return std::unexpected(std::move(added.error()));
  if (auto added = callbacks.register_callback("setForceRegions", [this](std::optional<JsonArray> const& values) -> std::expected<void, StarException> {
      List<PhysicsForceRegion> regions;
      if (values) {
        for (auto const& value : *values) {
          auto region = json_to_physics_force_region(value);
          if (!region)
            return std::unexpected(std::move(region.error()));
          regions.push_back(std::move(*region));
        }
      }
      m_force_regions.set(std::move(regions));
      return {};
    }); !added)
    return std::unexpected(std::move(added.error()));
  if (auto added = callbacks.register_callback("setItemForceRegions", [this](std::optional<JsonArray> const& values) -> std::expected<void, StarException> {
      List<PhysicsForceRegion> regions;
      if (values) {
        for (auto const& value : *values) {
          auto region = json_to_physics_force_region(value);
          if (!region)
            return std::unexpected(std::move(region.error()));
          regions.push_back(std::move(*region));
        }
      }
      m_item_force_regions.set(std::move(regions));
      return {};
    }); !added)
    return std::unexpected(std::move(added.error()));
  if (auto added = callbacks.register_callback("setCursor", [this](std::optional<String> value) { m_cursor = std::move(value); }); !added)
    return std::unexpected(std::move(added.error()));
  if (auto added = callbacks.register_callback("setScriptedAnimationParameter", [this](String name, Json value) { m_scriptedAnimationParameters.set(std::move(name), std::move(value)); }); !added)
    return std::unexpected(std::move(added.error()));

  auto loadIcon = [this](Json const& icon) -> std::expected<List<Drawable>, StarException> {
    List<Drawable> drawables;
    if (icon.type() == Json::Type::Array) {
      auto configs = icon.to_array();
      if (!configs)
        return std::unexpected(std::move(configs.error()));
      for (auto config : *configs) {
        auto image = config.opt_string("image");
        if (!image)
          return std::unexpected(std::move(image.error()));
        auto metadata = root().image_metadata_database();
        if (!metadata)
          return std::unexpected(std::move(metadata.error()));
        if (*image) {
          auto updated = config.set("image", AssetPath::relative_to(directory(), **image));
          if (!updated)
            return std::unexpected(std::move(updated.error()));
          config = std::move(*updated);
        }
        auto drawable = Drawable::create(**metadata, config);
        if (!drawable)
          return std::unexpected(std::move(drawable.error()));
        drawables.push_back(std::move(*drawable));
      }
    } else {
      auto image = icon.to_string();
      if (!image)
        return std::unexpected(std::move(image.error()));
      auto metadata = root().image_metadata_database();
      if (!metadata)
        return std::unexpected(std::move(metadata.error()));
      auto drawable = Drawable::make_image(**metadata, AssetPath::relative_to(directory(), *image), 1.0f, true, Vec2F());
      if (!drawable)
        return std::unexpected(std::move(drawable.error()));
      drawables.push_back(std::move(*drawable));
    }
    return drawables;
  };
  if (auto added = callbacks.register_callback("setInventoryIcon", [this, loadIcon](Json icon) -> std::expected<void, StarException> {
      auto stored = set_instance_value("inventoryIcon", icon);
      if (!stored)
        return std::unexpected(std::move(stored.error()));
      return loadIcon(icon).and_then([&](List<Drawable> drawables) { return set_icon_drawables(std::move(drawables)); });
    }); !added)
    return std::unexpected(std::move(added.error()));
  if (auto added = callbacks.register_callback("setSecondaryIcon", [this, loadIcon](Json icon) -> std::expected<void, StarException> {
      auto stored = set_instance_value("secondaryIcon", icon);
      if (!stored)
        return std::unexpected(std::move(stored.error()));
      if (icon.type() == Json::Type::Array || icon.type() == Json::Type::String)
        return loadIcon(icon).and_then([&](List<Drawable> drawables) { return set_secondary_icon_drawables(std::move(drawables)); });
      return set_secondary_icon_drawables(std::nullopt);
    }); !added)
    return std::unexpected(std::move(added.error()));
  if (auto added = callbacks.register_callback("setInstanceValue", [this](String name, Json value) { return set_instance_value(std::move(name), std::move(value)); }); !added)
    return std::unexpected(std::move(added.error()));
  if (auto added = callbacks.register_callback("callOtherHandScript", [this](String const& function, LuaVariadic<LuaValue> const& args) -> std::expected<LuaValue, StarException> {
      auto itemOwner = owner();
      if (!itemOwner)
        return std::unexpected(std::move(itemOwner.error()));
      auto itemHand = hand();
      if (!itemHand)
        return std::unexpected(std::move(itemHand.error()));
      if (auto otherHandItem = itemOwner->get().hand_item(*itemHand == ToolHand::Primary ? ToolHand::Alt : ToolHand::Primary)) {
        if (auto otherActiveItem = as<ActiveItem>(otherHandItem))
          return recover_lua_script_error(otherActiveItem->m_script.invoke(function, args)).transform([](std::optional<LuaValue> value) {
            return std::move(value).value_or(LuaValue{});
          });
      }
      return LuaValue{};
    }); !added)
    return std::unexpected(std::move(added.error()));
  if (auto added = callbacks.register_callback("interact", [this](String const& type, Json config_data, std::optional<EntityId> source_entity_id) -> std::expected<void, StarException> {
      auto item_owner = owner();
      if (!item_owner)
        return std::unexpected(std::move(item_owner.error()));
      auto action = InteractAction::create(type, source_entity_id.value_or(null_entity_id), std::move(config_data));
      if (!action)
        return std::unexpected(std::move(action.error()));
      if (!source_entity_id && action->type == InteractActionType::Message)
        action->entity_id = item_owner->get().entityId();
      return item_owner->get().interact(std::move(*action));
    }); !added)
    return std::unexpected(std::move(added.error()));
  if (auto added = callbacks.register_callback("emote", [this](String const& name) -> std::expected<void, StarException> {
      if (!humanoid_emote_names)
        return std::unexpected(humanoid_emote_names.error());
      auto emote = humanoid_emote_names->get_left(name);
      if (!emote)
        return std::unexpected(std::move(emote.error()));
      auto itemOwner = owner();
      if (!itemOwner)
        return std::unexpected(std::move(itemOwner.error()));
      if (auto entity = as_ref<EmoteEntity>(itemOwner->get()))
        return entity->get().play_emote(emote->get());
      return {};
    }); !added)
    return std::unexpected(std::move(added.error()));
  if (auto added = callbacks.register_callback("setCameraFocusEntity", [this](std::optional<EntityId> const& entity) -> std::expected<void, StarException> {
      auto itemOwner = owner();
      if (!itemOwner)
        return std::unexpected(std::move(itemOwner.error()));
      itemOwner->get().set_camera_focus_entity(entity);
      return {};
    }); !added)
    return std::unexpected(std::move(added.error()));
  if (auto added = callbacks.register_callback("setDescription", [this](String const& value) -> std::expected<void, StarException> {
      auto stored = set_instance_value("description", value);
      if (!stored)
        return std::unexpected(std::move(stored.error()));
      set_description(value);
      return {};
    }); !added)
    return std::unexpected(std::move(added.error()));
  if (auto added = callbacks.register_callback("setShortDescription", [this](String const& value) -> std::expected<void, StarException> {
      auto stored = set_instance_value("shortdescription", value);
      if (!stored)
        return std::unexpected(std::move(stored.error()));
      set_short_description(value);
      return {};
    }); !added)
    return std::unexpected(std::move(added.error()));
  return callbacks;
}

std::expected<LuaCallbacks, StarException> ActiveItem::makeScriptedAnimationCallbacks() {
  LuaCallbacks callbacks;
  if (auto added = callbacks.register_callback("ownerPosition", [this]() {
      return owner().transform([](std::reference_wrapper<ToolUserEntity> owner) { return owner.get().position(); });
    }); !added)
    return std::unexpected(std::move(added.error()));
  if (auto added = callbacks.register_callback("ownerAimPosition", [this]() {
      return owner().and_then([](std::reference_wrapper<ToolUserEntity> owner) { return owner.get().aim_position(); });
    }); !added)
    return std::unexpected(std::move(added.error()));
  if (auto added = callbacks.register_callback("ownerArmAngle", [this]() { return m_armAngle.get(); }); !added)
    return std::unexpected(std::move(added.error()));
  if (auto added = callbacks.register_callback("ownerFacingDirection", [this]() {
      return owner().transform([](std::reference_wrapper<ToolUserEntity> owner) { return numerical_direction(owner.get().facing_direction()); });
    }); !added)
    return std::unexpected(std::move(added.error()));
  if (auto added = callbacks.register_callback("handPosition", [this](std::optional<Vec2F> offset) { return handPosition(offset.value_or(Vec2F{})); }); !added)
    return std::unexpected(std::move(added.error()));
  return callbacks;
}

}

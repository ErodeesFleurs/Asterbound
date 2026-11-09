import star.json;
import star.json_extra;
import star.id_map;
import star.casting;
import star.vector;
import star.data_stream;
import star.game_types;
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
import star.version;
import star.ordered_set;
import star.either;


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

std::expected<std::shared_ptr<ActiveItem>, StarException> ActiveItem::create(Root& root, Json const& config, String const& directory, Json const& parameters) {
  auto item = std::shared_ptr<ActiveItem>(new ActiveItem(root));
  auto initialized = item->initialize(config, directory, parameters);
  if (!initialized)
    return std::unexpected(std::move(initialized.error()));
  auto assets = root.assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  auto animationValue = item->instanceValue("animation");
  if (!animationValue)
    return std::unexpected(std::move(animationValue.error()));
  auto animationConfig = (*assets)->fetchJson(*animationValue, directory);
  if (!animationConfig)
    return std::unexpected(std::move(animationConfig.error()));
  auto custom = item->instanceValue("animationCustom");
  if (!custom)
    return std::unexpected(std::move(custom.error()));
  if (*custom)
    animationConfig = jsonMerge(*animationConfig, *custom);
  auto animator = NetworkedAnimator::create(root, *animationConfig, directory);
  if (!animator)
    return std::unexpected(std::move(animator.error()));
  item->m_itemAnimator = std::move(*animator);
  auto parts = item->instanceValue("animationParts", JsonObject()).and_then([](Json const& value) { return value.iterateObject(); });
  if (!parts)
    return std::unexpected(std::move(parts.error()));
  for (auto const& pair : *parts) {
    auto part = pair.second.toString();
    if (!part)
      return std::unexpected(std::move(part.error()));
    item->m_itemAnimator->setPartTag(pair.first, "partImage", *part);
  }
  auto animationParameters = config.getObject("scriptedAnimationParameters", {});
  if (!animationParameters)
    return std::unexpected(std::move(animationParameters.error()));
  item->m_scriptedAnimationParameters.reset(std::move(*animationParameters));
  for (NetElement& element : std::initializer_list<std::reference_wrapper<NetElement>>{
      *item->m_itemAnimator,
      item->m_holdingItem, item->m_backArmFrame, item->m_frontArmFrame,
      item->m_twoHandedGrip, item->m_recoil, item->m_outsideOfHand,
      item->m_armAngle, item->m_facingDirection, item->m_damageSources,
      item->m_itemDamageSources, item->m_shieldPolys, item->m_itemShieldPolys,
      item->m_forceRegions, item->m_itemForceRegions}) {
    auto added = item->addNetElement(element);
    if (!added)
      return std::unexpected(std::move(added.error()));
  }
  auto parametersAdded = item->addNetElement(item->m_scriptedAnimationParameters, false);
  if (!parametersAdded)
    return std::unexpected(std::move(parametersAdded.error()));
  item->m_holdingItem.set(true);
  item->m_armAngle.setFixedPointBase(0.01f);
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
    auto scripts = instanceValue("scripts").and_then(jsonToStringList);
    if (!scripts)
      return std::unexpected(std::move(scripts.error()));
    auto configured = m_script.setScripts(scripts->transformed(
      [directory = directory()](String const& script) { return AssetPath::relative_to(directory, script); }));
    if (!configured)
      return std::unexpected(std::move(configured.error()));
    auto delta = instanceValue("scriptDelta", 1).and_then([](Json const& value) { return value.toUInt(); });
    if (!delta)
      return std::unexpected(std::move(delta.error()));
    m_script.setUpdateDelta(*delta);
    m_twoHandedGrip.set(twoHanded());
    auto previousStorage = instanceValue("scriptStorage");
    if (!previousStorage)
      return std::unexpected(std::move(previousStorage.error()));
    if (*previousStorage) {
      auto stored = previousStorage->toObject().and_then([&](JsonObject storage) { return m_script.setScriptStorage(std::move(storage)); });
      if (!stored)
        return std::unexpected(std::move(stored.error()));
    }
    auto activeCallbacks = makeActiveItemCallbacks().and_then([&](LuaCallbacks callbacks) { return m_script.addCallbacks("activeItem", std::move(callbacks)); });
    if (!activeCallbacks)
      return std::unexpected(std::move(activeCallbacks.error()));
    auto itemCallbacks = LuaBindings::makeItemCallbacks(*this).and_then([&](LuaCallbacks callbacks) { return m_script.addCallbacks("item", std::move(callbacks)); });
    if (!itemCallbacks)
      return std::unexpected(std::move(itemCallbacks.error()));
    auto configCallbacks =
      LuaBindings::makeConfigCallbacks([this](String const& name,
                                              Json const& defaultValue) -> std::expected<Json, StarException> {
        return instanceValue(name, defaultValue);
      }).and_then([&](LuaCallbacks callbacks) { return m_script.addCallbacks("config", std::move(callbacks)); });
    if (!configCallbacks)
      return std::unexpected(std::move(configCallbacks.error()));
    auto animatorCallbacks = LuaBindings::makeNetworkedAnimatorCallbacks(*m_itemAnimator).and_then([&](LuaCallbacks callbacks) { return m_script.addCallbacks("animator", std::move(callbacks)); });
    if (!animatorCallbacks)
      return std::unexpected(std::move(animatorCallbacks.error()));
    auto statusCallbacks = LuaBindings::makeStatusControllerCallbacks(owner.statusController()).and_then([&](LuaCallbacks callbacks) { return m_script.addCallbacks("status", std::move(callbacks)); });
    if (!statusCallbacks)
      return std::unexpected(std::move(statusCallbacks.error()));
    auto movementCallbacks = m_script.addActorMovementCallbacks(owner.movementController());
    if (!movementCallbacks)
      return std::unexpected(std::move(movementCallbacks.error()));
    if (auto player = asRef<Player>(owner)) {
      auto playerCallbacks = LuaBindings::makePlayerCallbacks(root(), player->get()).and_then([&](LuaCallbacks callbacks) { return m_script.addCallbacks("player", std::move(callbacks)); });
      if (!playerCallbacks)
        return std::unexpected(std::move(playerCallbacks.error()));
    }
    auto entityCallbacks = LuaBindings::makeEntityCallbacks(owner).and_then([&](LuaCallbacks callbacks) { return m_script.addCallbacks("entity", std::move(callbacks)); });
    if (!entityCallbacks)
      return std::unexpected(std::move(entityCallbacks.error()));
    auto itemWorld = world();
    if (!itemWorld)
      return std::unexpected(std::move(itemWorld.error()));
    auto scriptInitialized = recoverLuaScriptError(m_script.init(itemWorld->get()));
    if (!scriptInitialized)
      return std::unexpected(std::move(scriptInitialized.error()));
    m_currentFireMode = FireMode::None;
  }
  auto itemWorld = world();
  if (!itemWorld)
    return std::unexpected(std::move(itemWorld.error()));
  if (itemWorld->get().isClient()) {
    auto animationScripts = instanceValue("animationScripts");
    if (!animationScripts)
      return std::unexpected(std::move(animationScripts.error()));
    if (*animationScripts) {
      auto scripts = jsonToStringList(*animationScripts);
      if (!scripts)
        return std::unexpected(std::move(scripts.error()));
      auto configured = m_scriptedAnimator.setScripts(scripts->transformed(
        [directory = directory()](String const& script) { return AssetPath::relative_to(directory, script); }));
      if (!configured)
        return std::unexpected(std::move(configured.error()));
      auto delta = instanceValue("animationDelta", 1).and_then([](Json const& value) { return value.toUInt(); });
      if (!delta)
        return std::unexpected(std::move(delta.error()));
      m_scriptedAnimator.setUpdateDelta(*delta);
      auto animationCallbacks = LuaBindings::makeScriptedAnimatorCallbacks(*m_itemAnimator,
          [this](String const& name, Json const& defaultValue) -> Json { return m_scriptedAnimationParameters.value(name, defaultValue); })
          .and_then([&](LuaCallbacks callbacks) { return m_scriptedAnimator.addCallbacks("animationConfig", std::move(callbacks)); });
      if (!animationCallbacks)
        return std::unexpected(std::move(animationCallbacks.error()));
      auto activeCallbacks = makeScriptedAnimationCallbacks().and_then([&](LuaCallbacks callbacks) { return m_scriptedAnimator.addCallbacks("activeItemAnimation", std::move(callbacks)); });
      if (!activeCallbacks)
        return std::unexpected(std::move(activeCallbacks.error()));
      auto configCallbacks =
        LuaBindings::makeConfigCallbacks([this](String const& name,
                                                Json const& defaultValue) -> std::expected<Json, StarException> {
          return instanceValue(name, defaultValue);
        }).and_then([&](LuaCallbacks callbacks) {
          return m_scriptedAnimator.addCallbacks("config", std::move(callbacks));
        });
      if (!configCallbacks)
        return std::unexpected(std::move(configCallbacks.error()));
      auto animationInitialized = recoverLuaScriptError(m_scriptedAnimator.init(itemWorld->get()));
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
    remember(recoverLuaScriptError(m_script.uninit()));
    for (auto const& name : std::initializer_list<std::string_view>{"activeItem", "item", "config", "animator", "status"})
      remember(m_script.removeCallbacks(name));
    remember(m_script.removeActorMovementCallbacks());
    for (auto const& name : std::initializer_list<std::string_view>{"player", "entity"})
      remember(m_script.removeCallbacks(name));
  }
  auto itemWorld = world();
  if (!itemWorld) {
    remember(std::move(itemWorld));
  } else if (itemWorld->get().isClient()) {
    auto animationScripts = instanceValue("animationScripts");
    if (!animationScripts) {
      remember(std::move(animationScripts));
    } else if (*animationScripts) {
      remember(recoverLuaScriptError(m_scriptedAnimator.uninit()));
      for (auto const& name : std::initializer_list<std::string_view>{"animationConfig", "activeItemAnimation", "config"})
        remember(m_scriptedAnimator.removeCallbacks(name));
    }
  }
  m_itemAnimatorDynamicTarget.stopAudio();
  remember(ToolUserItem::uninit());
  m_activeAudio.clear();
  if (error)
    return std::unexpected(std::move(*error));
  return {};
}

std::expected<void, StarException> ActiveItem::update(float dt, FireMode fireMode, bool shifting, HashSet<MoveControlType> const& moves) {
  StringMap<bool> moveMap;
  for (auto move : moves) {
    auto name = enumName<MoveControlTypeNames>(move);
    if (!name)
      return std::unexpected(std::move(name.error()));
    moveMap[String(*name)] = true;
  }
  auto mode = entityMode();
  if (!mode)
    return std::unexpected(std::move(mode.error()));
  if (*mode == EntityMode::Master) {
    auto fireModeName = enumName<FireModeNames>(fireMode);
    if (!fireModeName)
      return std::unexpected(std::move(fireModeName.error()));
    if (fireMode != m_currentFireMode) {
      m_currentFireMode = fireMode;
      if (fireMode != FireMode::None) {
        auto activated = recoverLuaScriptError(m_script.invoke("activate", *fireModeName, shifting, moveMap));
        if (!activated)
          return std::unexpected(std::move(activated.error()));
      }
    }
    auto updated = recoverLuaScriptError(m_script.update(m_script.updateDt(dt), *fireModeName, shifting, moveMap));
    if (!updated)
      return std::unexpected(std::move(updated.error()));
    auto retain = instanceValue("retainScriptStorageInItem", false).and_then([](Json const& value) { return value.toBool(); });
    if (!retain)
      return std::unexpected(std::move(retain.error()));
    if (*retain) {
      auto storage = m_script.getScriptStorage();
      if (!storage)
        return std::unexpected(std::move(storage.error()));
      auto stored = setInstanceValue("scriptStorage", *storage);
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
    auto updated = recoverLuaScriptError(m_scriptedAnimator.update(m_scriptedAnimator.updateDt(dt)));
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
    it->first->setPosition(itemOwner->get().position() + *position);
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
      root().spatialLogger().logPoly("world", shieldPoly, {255, 255, 0, 255});
  }
  if (isClient) {
    auto forces = forceRegions();
    if (!forces)
      return std::unexpected(std::move(forces.error()));
    for (auto forceRegion : *forces) {
      if (std::holds_alternative<DirectionalForceRegion>(forceRegion))
        root().spatialLogger().logPoly("world", std::get<DirectionalForceRegion>(forceRegion).region, {155, 0, 255, 255});
      else if (std::holds_alternative<RadialForceRegion>(forceRegion))
        root().spatialLogger().logPoint("world", std::get<RadialForceRegion>(forceRegion).center, {155, 0, 255, 255});
    }
  }
  return {};
}

std::expected<List<DamageSource>, StarException> ActiveItem::damageSources() const {
  List<DamageSource> damageSources = m_damageSources.get();
  for (auto ds : m_itemDamageSources.get()) {
    if (std::holds_alternative<PolyF>(ds.damageArea)) {
      auto& poly = std::get<PolyF>(ds.damageArea);
      poly.rotate(m_armAngle.get());
      auto itemOwner = owner();
      if (!itemOwner)
        return std::unexpected(std::move(itemOwner.error()));
      if (itemOwner->get().facingDirection() == Direction::Left)
        poly.flipHorizontal(0.0f);
      auto position = handPosition(Vec2F());
      if (!position)
        return std::unexpected(std::move(position.error()));
      poly.translate(*position);
    } else if (std::holds_alternative<Line2F>(ds.damageArea)) {
      auto& line = std::get<Line2F>(ds.damageArea);
      line.rotate(m_armAngle.get());
      auto itemOwner = owner();
      if (!itemOwner)
        return std::unexpected(std::move(itemOwner.error()));
      if (itemOwner->get().facingDirection() == Direction::Left)
        line.flipHorizontal(0.0f);
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
    if (itemOwner->get().facingDirection() == Direction::Left)
      sp.flipHorizontal(0.0f);
    auto position = handPosition(Vec2F());
    if (!position)
      return std::unexpected(std::move(position.error()));
    sp.translate(*position);
    shieldPolys.push_back(std::move(sp));
  }
  return shieldPolys;
}

std::expected<List<PhysicsForceRegion>, StarException> ActiveItem::forceRegions() const {
  List<PhysicsForceRegion> forceRegions = m_forceRegions.get();
  for (auto fr : m_itemForceRegions.get()) {
    if (std::holds_alternative<DirectionalForceRegion>(fr)) {
      auto& dfr = std::get<DirectionalForceRegion>(fr);
      dfr.region.rotate(m_armAngle.get());
      auto itemOwner = owner();
      if (!itemOwner)
        return std::unexpected(std::move(itemOwner.error()));
      if (itemOwner->get().facingDirection() == Direction::Left)
        dfr.region.flipHorizontal(0.0f);
      auto position = handPosition(Vec2F());
      if (!position)
        return std::unexpected(std::move(position.error()));
      dfr.region.translate(itemOwner->get().position() + *position);
    } else if (std::holds_alternative<RadialForceRegion>(fr)) {
      auto& rfr = std::get<RadialForceRegion>(fr);
      rfr.center = rfr.center.rotate(m_armAngle.get());
      auto itemOwner = owner();
      if (!itemOwner)
        return std::unexpected(std::move(itemOwner.error()));
      if (itemOwner->get().facingDirection() == Direction::Left)
        rfr.center[0] *= -1;
      auto position = handPosition(Vec2F());
      if (!position)
        return std::unexpected(std::move(position.error()));
      rfr.center += itemOwner->get().position() + *position;
    }
    forceRegions.push_back(std::move(fr));
  }
  return forceRegions;
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
  if (m_itemAnimator->constParts().empty()) {
    auto drawables = Item::iconDrawables();
    if (!drawables)
      return std::unexpected(std::move(drawables.error()));
    Drawable::scaleAll(*drawables, 1.0f / TilePixels);
    return drawables;
  } else {
    return sample.drawables();
  }
}

std::expected<List<std::pair<Drawable, std::optional<EntityRenderLayer>>>, StarException> ActiveItem::entityDrawables() const {
  return m_scriptedAnimator.drawables();
}

std::expected<List<LightSource>, StarException> ActiveItem::lights(float interpolation) const {
  auto sample = m_itemAnimator->sample(interpolation);
  // Same as pullNewAudios, we translate and flip ourselves.
  List<LightSource> result;
  auto lights = sample.lightSources();
  if (!lights)
    return std::unexpected(std::move(lights.error()));
  for (auto& light : *lights) {
    auto itemOwner = owner();
    if (!itemOwner)
      return std::unexpected(std::move(itemOwner.error()));
    auto position = handPosition(light.position);
    if (!position)
      return std::unexpected(std::move(position.error()));
    light.position = itemOwner->get().position() + *position;
    light.beamAngle += m_armAngle.get();
    if (itemOwner->get().facingDirection() == Direction::Left) {
      if (light.beamAngle > 0)
        light.beamAngle = std::numbers::pi / 2 + constrainAngle(std::numbers::pi / 2 - light.beamAngle);
      else
        light.beamAngle = -std::numbers::pi / 2 - constrainAngle(light.beamAngle + std::numbers::pi / 2);
    }
    result.push_back(std::move(light));
  }
  result.append_range(m_scriptedAnimator.lightSources());
  return result;
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
  for (auto& audio : m_itemAnimatorDynamicTarget.pullNewAudios()) {
    m_activeAudio[audio] = *audio->position();
    auto itemOwner = owner();
    if (!itemOwner)
      return std::unexpected(std::move(itemOwner.error()));
    auto position = handPosition(*audio->position());
    if (!position)
      return std::unexpected(std::move(position.error()));
    audio->setPosition(itemOwner->get().position() + *position);
    result.push_back(std::move(audio));
  }
  result.append_range(m_scriptedAnimator.pullNewAudios());
  return result;
}

std::expected<List<Particle>, StarException> ActiveItem::pullNewParticles() {
  // Same as pullNewAudios, we translate, rotate, and flip ourselves
  List<Particle> result;
  for (auto& particle : m_itemAnimatorDynamicTarget.pullNewParticles()) {
    auto itemOwner = owner();
    if (!itemOwner)
      return std::unexpected(std::move(itemOwner.error()));
    auto position = handPosition(particle.position);
    if (!position)
      return std::unexpected(std::move(position.error()));
    particle.position = itemOwner->get().position() + *position;
    particle.velocity = particle.velocity.rotate(m_armAngle.get());
    if (itemOwner->get().facingDirection() == Direction::Left) {
      particle.velocity[0] *= -1;
      particle.flip = !particle.flip;
    }
    result.push_back(std::move(particle));
  }
  result.append_range(m_scriptedAnimator.pullNewParticles());
  return result;
}

std::optional<String> ActiveItem::cursor() const {
  return m_cursor;
}

std::expected<std::optional<ChainableJsonMessageResponse>, StarException> ActiveItem::receiveMessage(String const& message, bool localMessage, JsonArray const& args) {
  return recoverLuaScriptError(m_script.handleMessage(message, localMessage, args));
}

std::expected<float, StarException> ActiveItem::durabilityStatus() {
  auto durability = instanceValue("durability").and_then([](Json const& value) { return value.optFloat(); });
  if (!durability)
    return std::unexpected(std::move(durability.error()));
  if (*durability) {
    auto durabilityHit = instanceValue("durabilityHit").and_then([](Json const& value) { return value.optFloat(); });
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
  return itemOwner->get().armPosition(*itemHand, itemOwner->get().facingDirection(), m_armAngle.get(), offset);
}

std::expected<Vec2F, StarException> ActiveItem::handPosition(Vec2F const& offset) const {
  auto itemOwner = owner();
  if (!itemOwner)
    return std::unexpected(std::move(itemOwner.error()));
  auto itemHand = hand();
  if (!itemHand)
    return std::unexpected(std::move(itemHand.error()));
  auto handOffset = itemOwner->get().handOffset(*itemHand, itemOwner->get().facingDirection());
  if (!handOffset)
    return std::unexpected(std::move(handOffset.error()));
  return armPosition(offset + *handOffset);
}

std::expected<LuaCallbacks, StarException> ActiveItem::makeActiveItemCallbacks() {
  LuaCallbacks callbacks;
  if (auto added = callbacks.registerCallback("ownerEntityId", [this]() {
      return owner().transform([](std::reference_wrapper<ToolUserEntity> owner) { return owner.get().entityId(); });
    }); !added)
    return std::unexpected(std::move(added.error()));
  if (auto added = callbacks.registerCallback("ownerTeam", [this]() {
      return owner().and_then([](std::reference_wrapper<ToolUserEntity> owner) { return owner.get().getTeam().toJson(); });
    }); !added)
    return std::unexpected(std::move(added.error()));
  if (auto added = callbacks.registerCallback("ownerAimPosition", [this]() {
      return owner().and_then([](std::reference_wrapper<ToolUserEntity> owner) { return owner.get().aimPosition(); });
    }); !added)
    return std::unexpected(std::move(added.error()));
  if (auto added = callbacks.registerCallback("ownerPowerMultiplier", [this]() {
      return owner().and_then([](std::reference_wrapper<ToolUserEntity> owner) { return owner.get().powerMultiplier(); });
    }); !added)
    return std::unexpected(std::move(added.error()));
  if (auto added = callbacks.registerCallback(
        "fireMode",
        [this]() -> std::expected<String, StarException> {
          return enumName<FireModeNames>(m_currentFireMode).transform([](std::string_view name) {
            return String(name);
          });
        });
      !added)
    return std::unexpected(std::move(added.error()));
  if (auto added = callbacks.registerCallback("hand", [this]() -> std::expected<String, StarException> {
      auto itemHand = hand();
      if (!itemHand)
        return std::unexpected(std::move(itemHand.error()));
      return enumName<ToolHandNames>(*itemHand).transform([](std::string_view name) { return String(name); });
    }); !added)
    return std::unexpected(std::move(added.error()));
  if (auto added = callbacks.registerCallback("handPosition", [this](std::optional<Vec2F> offset) {
      return handPosition(offset.value_or(Vec2F{}));
    }); !added)
    return std::unexpected(std::move(added.error()));

  // Aim a barrel with a vertical offset from the hand at the given target.
  if (auto added = callbacks.registerCallback("aimAngleAndDirection", [this](float aimVerticalOffset, Vec2F targetPosition) -> std::expected<LuaTupleReturn<float, int>, StarException> {
      auto itemOwner = owner();
      if (!itemOwner)
        return std::unexpected(std::move(itemOwner.error()));
      auto itemHand = hand();
      if (!itemHand)
        return std::unexpected(std::move(itemHand.error()));
      auto rotationCenter = itemOwner->get().armPosition(*itemHand, itemOwner->get().facingDirection(), 0.0f, Vec2F());
      if (!rotationCenter)
        return std::unexpected(std::move(rotationCenter.error()));
      Vec2F handRotationCenter = *rotationCenter;
      Vec2F ownerPosition = itemOwner->get().position();
      auto itemWorld = itemOwner->get().world();
      if (!itemWorld)
        return std::unexpected(std::move(itemWorld.error()));
      Vec2F toTarget = itemWorld->get().geometry().diff(targetPosition, ownerPosition);
      // Keep the target beyond the hand's rotation center to avoid rapid flips.
      auto dir = numericalDirection(itemOwner->get().facingDirection());
      auto targetX = toTarget[0] * dir;
      auto centerX = handRotationCenter[0] * dir;
      if (centerX >= 0 && targetX >= 0 && targetX < centerX)
        toTarget[0] = handRotationCenter[0] + (0.125 * dir);
      toTarget -= handRotationCenter;
      float toTargetDist = toTarget.magnitude();
      // Clamp targets inside the barrel-offset circle to retain continuity.
      float angleAdjust = -std::asin(std::clamp(aimVerticalOffset / toTargetDist, -1.0f, 1.0f));
      auto angleSide = getAngleSide(toTarget.angle());
      return luaTupleReturn(angleSide.first + angleAdjust, numericalDirection(angleSide.second));
    }); !added)
    return std::unexpected(std::move(added.error()));
  if (auto added = callbacks.registerCallback("aimAngle", [this](float aimVerticalOffset, Vec2F targetPosition) -> std::expected<float, StarException> {
      auto itemOwner = owner();
      if (!itemOwner)
        return std::unexpected(std::move(itemOwner.error()));
      auto itemHand = hand();
      if (!itemHand)
        return std::unexpected(std::move(itemHand.error()));
      auto rotationCenter = itemOwner->get().armPosition(*itemHand, itemOwner->get().facingDirection(), 0.0f, Vec2F());
      if (!rotationCenter)
        return std::unexpected(std::move(rotationCenter.error()));
      Vec2F handRotationCenter = *rotationCenter;
      Vec2F ownerPosition = itemOwner->get().position();
      auto itemWorld = itemOwner->get().world();
      if (!itemWorld)
        return std::unexpected(std::move(itemWorld.error()));
      Vec2F toTarget = itemWorld->get().geometry().diff(targetPosition, ownerPosition + handRotationCenter);
      float toTargetDist = toTarget.magnitude();
      float angleAdjust = -std::asin(std::clamp(aimVerticalOffset / toTargetDist, -1.0f, 1.0f));
      return toTarget.angle() + angleAdjust;
    }); !added)
    return std::unexpected(std::move(added.error()));
  if (auto added = callbacks.registerCallback("setHoldingItem", [this](bool value) { m_holdingItem.set(value); }); !added)
    return std::unexpected(std::move(added.error()));
  if (auto added = callbacks.registerCallback("setBackArmFrame", [this](std::optional<String> value) { m_backArmFrame.set(value); }); !added)
    return std::unexpected(std::move(added.error()));
  if (auto added = callbacks.registerCallback("setFrontArmFrame", [this](std::optional<String> value) { m_frontArmFrame.set(value); }); !added)
    return std::unexpected(std::move(added.error()));
  if (auto added = callbacks.registerCallback("setTwoHandedGrip", [this](bool value) { m_twoHandedGrip.set(value); }); !added)
    return std::unexpected(std::move(added.error()));
  if (auto added = callbacks.registerCallback("setRecoil", [this](bool value) { m_recoil.set(value); }); !added)
    return std::unexpected(std::move(added.error()));
  if (auto added = callbacks.registerCallback("setOutsideOfHand", [this](bool value) { m_outsideOfHand.set(value); }); !added)
    return std::unexpected(std::move(added.error()));
  if (auto added = callbacks.registerCallback("setArmAngle", [this](float value) { m_armAngle.set(value); }); !added)
    return std::unexpected(std::move(added.error()));
  if (auto added = callbacks.registerCallback("setFacingDirection", [this](float value) { m_facingDirection.set(directionOf(value)); }); !added)
    return std::unexpected(std::move(added.error()));
  if (auto added = callbacks.registerCallback("setDamageSources", [this](std::optional<JsonArray> const& values) -> std::expected<void, StarException> {
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
  if (auto added = callbacks.registerCallback("setItemDamageSources", [this](std::optional<JsonArray> const& values) -> std::expected<void, StarException> {
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
  if (auto added = callbacks.registerCallback("setShieldPolys", [this](std::optional<List<PolyF>> const& value) { m_shieldPolys.set(value.value_or(List<PolyF>{})); }); !added)
    return std::unexpected(std::move(added.error()));
  if (auto added = callbacks.registerCallback("setItemShieldPolys", [this](std::optional<List<PolyF>> const& value) { m_itemShieldPolys.set(value.value_or(List<PolyF>{})); }); !added)
    return std::unexpected(std::move(added.error()));
  if (auto added = callbacks.registerCallback("setForceRegions", [this](std::optional<JsonArray> const& values) -> std::expected<void, StarException> {
      List<PhysicsForceRegion> regions;
      if (values) {
        for (auto const& value : *values) {
          auto region = jsonToPhysicsForceRegion(value);
          if (!region)
            return std::unexpected(std::move(region.error()));
          regions.push_back(std::move(*region));
        }
      }
      m_forceRegions.set(std::move(regions));
      return {};
    }); !added)
    return std::unexpected(std::move(added.error()));
  if (auto added = callbacks.registerCallback("setItemForceRegions", [this](std::optional<JsonArray> const& values) -> std::expected<void, StarException> {
      List<PhysicsForceRegion> regions;
      if (values) {
        for (auto const& value : *values) {
          auto region = jsonToPhysicsForceRegion(value);
          if (!region)
            return std::unexpected(std::move(region.error()));
          regions.push_back(std::move(*region));
        }
      }
      m_itemForceRegions.set(std::move(regions));
      return {};
    }); !added)
    return std::unexpected(std::move(added.error()));
  if (auto added = callbacks.registerCallback("setCursor", [this](std::optional<String> value) { m_cursor = std::move(value); }); !added)
    return std::unexpected(std::move(added.error()));
  if (auto added = callbacks.registerCallback("setScriptedAnimationParameter", [this](String name, Json value) { m_scriptedAnimationParameters.set(std::move(name), std::move(value)); }); !added)
    return std::unexpected(std::move(added.error()));

  auto loadIcon = [this](Json const& icon) -> std::expected<List<Drawable>, StarException> {
    List<Drawable> drawables;
    if (icon.type() == Json::Type::Array) {
      auto configs = icon.toArray();
      if (!configs)
        return std::unexpected(std::move(configs.error()));
      for (auto config : *configs) {
        auto image = config.optString("image");
        if (!image)
          return std::unexpected(std::move(image.error()));
        auto metadata = root().imageMetadataDatabase();
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
      auto image = icon.toString();
      if (!image)
        return std::unexpected(std::move(image.error()));
      auto metadata = root().imageMetadataDatabase();
      if (!metadata)
        return std::unexpected(std::move(metadata.error()));
      auto drawable = Drawable::makeImage(**metadata, AssetPath::relative_to(directory(), *image), 1.0f, true, Vec2F());
      if (!drawable)
        return std::unexpected(std::move(drawable.error()));
      drawables.push_back(std::move(*drawable));
    }
    return drawables;
  };
  if (auto added = callbacks.registerCallback("setInventoryIcon", [this, loadIcon](Json icon) -> std::expected<void, StarException> {
      auto stored = setInstanceValue("inventoryIcon", icon);
      if (!stored)
        return std::unexpected(std::move(stored.error()));
      return loadIcon(icon).and_then([&](List<Drawable> drawables) { return setIconDrawables(std::move(drawables)); });
    }); !added)
    return std::unexpected(std::move(added.error()));
  if (auto added = callbacks.registerCallback("setSecondaryIcon", [this, loadIcon](Json icon) -> std::expected<void, StarException> {
      auto stored = setInstanceValue("secondaryIcon", icon);
      if (!stored)
        return std::unexpected(std::move(stored.error()));
      if (icon.type() == Json::Type::Array || icon.type() == Json::Type::String)
        return loadIcon(icon).and_then([&](List<Drawable> drawables) { return setSecondaryIconDrawables(std::move(drawables)); });
      return setSecondaryIconDrawables(std::nullopt);
    }); !added)
    return std::unexpected(std::move(added.error()));
  if (auto added = callbacks.registerCallback("setInstanceValue", [this](String name, Json value) { return setInstanceValue(std::move(name), std::move(value)); }); !added)
    return std::unexpected(std::move(added.error()));
  if (auto added = callbacks.registerCallback("callOtherHandScript", [this](String const& function, LuaVariadic<LuaValue> const& args) -> std::expected<LuaValue, StarException> {
      auto itemOwner = owner();
      if (!itemOwner)
        return std::unexpected(std::move(itemOwner.error()));
      auto itemHand = hand();
      if (!itemHand)
        return std::unexpected(std::move(itemHand.error()));
      if (auto otherHandItem = itemOwner->get().handItem(*itemHand == ToolHand::Primary ? ToolHand::Alt : ToolHand::Primary)) {
        if (auto otherActiveItem = as<ActiveItem>(otherHandItem))
          return recoverLuaScriptError(otherActiveItem->m_script.invoke(function, args)).transform([](std::optional<LuaValue> value) {
            return std::move(value).value_or(LuaValue{});
          });
      }
      return LuaValue{};
    }); !added)
    return std::unexpected(std::move(added.error()));
  if (auto added = callbacks.registerCallback("interact", [this](String const& type, Json const& configData, std::optional<EntityId> const& sourceEntityId) -> std::expected<void, StarException> {
      auto itemOwner = owner();
      if (!itemOwner)
        return std::unexpected(std::move(itemOwner.error()));
      auto action = InteractAction::create(type, sourceEntityId.value_or(NullEntityId), configData);
      if (!action)
        return std::unexpected(std::move(action.error()));
      return itemOwner->get().interact(*action);
    }); !added)
    return std::unexpected(std::move(added.error()));
  if (auto added = callbacks.registerCallback("emote", [this](String const& name) -> std::expected<void, StarException> {
      if (!HumanoidEmoteNames)
        return std::unexpected(HumanoidEmoteNames.error());
      auto emote = HumanoidEmoteNames->get_left(name);
      if (!emote)
        return std::unexpected(std::move(emote.error()));
      auto itemOwner = owner();
      if (!itemOwner)
        return std::unexpected(std::move(itemOwner.error()));
      if (auto entity = asRef<EmoteEntity>(itemOwner->get()))
        return entity->get().playEmote(emote->get());
      return {};
    }); !added)
    return std::unexpected(std::move(added.error()));
  if (auto added = callbacks.registerCallback("setCameraFocusEntity", [this](std::optional<EntityId> const& entity) -> std::expected<void, StarException> {
      auto itemOwner = owner();
      if (!itemOwner)
        return std::unexpected(std::move(itemOwner.error()));
      itemOwner->get().setCameraFocusEntity(entity);
      return {};
    }); !added)
    return std::unexpected(std::move(added.error()));
  if (auto added = callbacks.registerCallback("setDescription", [this](String const& value) -> std::expected<void, StarException> {
      auto stored = setInstanceValue("description", value);
      if (!stored)
        return std::unexpected(std::move(stored.error()));
      setDescription(value);
      return {};
    }); !added)
    return std::unexpected(std::move(added.error()));
  if (auto added = callbacks.registerCallback("setShortDescription", [this](String const& value) -> std::expected<void, StarException> {
      auto stored = setInstanceValue("shortdescription", value);
      if (!stored)
        return std::unexpected(std::move(stored.error()));
      setShortDescription(value);
      return {};
    }); !added)
    return std::unexpected(std::move(added.error()));
  return callbacks;
}

std::expected<LuaCallbacks, StarException> ActiveItem::makeScriptedAnimationCallbacks() {
  LuaCallbacks callbacks;
  if (auto added = callbacks.registerCallback("ownerPosition", [this]() {
      return owner().transform([](std::reference_wrapper<ToolUserEntity> owner) { return owner.get().position(); });
    }); !added)
    return std::unexpected(std::move(added.error()));
  if (auto added = callbacks.registerCallback("ownerAimPosition", [this]() {
      return owner().and_then([](std::reference_wrapper<ToolUserEntity> owner) { return owner.get().aimPosition(); });
    }); !added)
    return std::unexpected(std::move(added.error()));
  if (auto added = callbacks.registerCallback("ownerArmAngle", [this]() { return m_armAngle.get(); }); !added)
    return std::unexpected(std::move(added.error()));
  if (auto added = callbacks.registerCallback("ownerFacingDirection", [this]() {
      return owner().transform([](std::reference_wrapper<ToolUserEntity> owner) { return numericalDirection(owner.get().facingDirection()); });
    }); !added)
    return std::unexpected(std::move(added.error()));
  if (auto added = callbacks.registerCallback("handPosition", [this](std::optional<Vec2F> offset) { return handPosition(offset.value_or(Vec2F{})); }); !added)
    return std::unexpected(std::move(added.error()));
  return callbacks;
}

}

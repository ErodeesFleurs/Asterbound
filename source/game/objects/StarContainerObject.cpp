import star.output_proxy;
import star.json_extra;
import star.json;
import star.id_map;
import star.ordered_map;
import star.lru_cache;
import star.thread;
import star.time;
import star.random;
import star.ttl_cache;
import star.casting;
import star.game_types;
import star.strong_typedef;
import star.rpc_promise;
import star.bi_map;
import star.a_star;
import star.data_stream;
import star.lexical_cast;
import star.weighted_pool;
import star.interpolation;
import star.parametric_function;
import star.logging;
import star.audio;
import star.list;
import star.map;
import star.set;
import star.color;
import star.string;
import star.image_processing;
import star.hash;
import star.string_view;
import star.directives;
import star.asset_path;
import star.math_common;
import star.periodic;
import star.periodic_function;
import star.lua;
import star.io_device;
import star.ref_ptr;
import star.listener;
import std;
import star.format;
import star.version;

import star.lua_root;
import star.lua_components;
import star.lua_animation_component;

import star.damage_types;
import star.world_geometry;
import star.status_types;
import star.damage;
import star.entity;
import star.tile_damage;
import star.interaction_types;
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
import star.item;
import star.item_descriptor;
import star.item_database;
import star.item_recipe;
import star.status_effect_entity;
import star.scripted_entity;
import star.chat_action;
import star.chatty_entity;
import star.wiring;
import star.wire_entity;
import star.inspectable_entity;
import star.animated_part_set;
import star.networked_animator;
import star.object;
import star.container_entity;
import star.container_object;
import star.asset_source;
import star.assets;
import star.root_base;
import star.configuration;
import star.root;
import star.mobile_entity;
import star.item_drop;
import star.mixer;
import star.drawable;
import star.entity_rendering_types;
import star.animation;
import star.particle;

import star.light_source;
import star.entity_rendering;
import star.augment_item;
import star.treasure;


import star.object_database;

import star.item_bag;

import star.data_stream_devices;

import star.algorithm;
namespace Star {

ContainerObject::ContainerObject(Root& rootContext) : Entity(rootContext), Object(rootContext), m_initialized(false), m_count(0), m_currentState(0), m_animationFrameCooldown(0), m_autoCloseCooldown(0), m_itemsUpdated(true), m_runUpdatedCallback(true) {}

std::expected<std::shared_ptr<ContainerObject>, StarException> ContainerObject::create(Root& rootContext, std::shared_ptr<ObjectConfig const> config, Json const& parameters) {
  auto object = std::shared_ptr<ContainerObject>(new ContainerObject(rootContext));
  auto operation1 = object->initialize(std::move(config), parameters);
  if (!operation1)
    return std::unexpected(std::move(operation1.error()));
  object->m_opened.set(0);
  object->m_crafting.set(false);
  object->m_craftingProgress.set(0);
  auto slotConfigResult = object->configValue("slotCount");
  if (!slotConfigResult)
    return std::unexpected(std::move(slotConfigResult.error()));
  auto slotConfig = std::move(*slotConfigResult);
  auto slotsResult = slotConfig.toInt();
  if (!slotsResult)
    return std::unexpected(std::move(slotsResult.error()));
  auto slots = std::move(*slotsResult);
  object->m_items = std::make_shared<ItemBag>(rootContext, slots);
  auto operation2 = object->m_netGroup.addNetElement(object->m_opened);
  if (!operation2)
    return std::unexpected(std::move(operation2.error()));
  auto operation3 = object->m_netGroup.addNetElement(object->m_crafting);
  if (!operation3)
    return std::unexpected(std::move(operation3.error()));
  auto operation4 = object->m_netGroup.addNetElement(object->m_craftingProgress);
  if (!operation4)
    return std::unexpected(std::move(operation4.error()));
  auto operation5 = object->m_netGroup.addNetElement(object->m_itemsNetState);
  if (!operation5)
    return std::unexpected(std::move(operation5.error()));
  object->m_craftingProgress.setInterpolator(lerp<float, float>);
  return object;
}

std::expected<void, StarException> ContainerObject::init(World& world, EntityId entityId, EntityMode mode) {
  if (mode == EntityMode::Master)
    m_interactive.set(true);
  auto operation1 = Object::init(world, entityId, mode);
  if (!operation1)
    return std::unexpected(std::move(operation1.error()));
  auto initialized = [&]() -> std::expected<void, StarException> {
    if (mode == EntityMode::Master && !m_initialized) {
      m_initialized = true;
      auto levelResult = world.threatLevel();
      if (!levelResult)
        return std::unexpected(std::move(levelResult.error()));
      auto level = std::move(*levelResult);
      auto seedConfigResult = configValue("treasureSeed", root().random().randu64());
      if (!seedConfigResult)
        return std::unexpected(std::move(seedConfigResult.error()));
      auto seedConfig = std::move(*seedConfigResult);
      auto seedResult = seedConfig.toUInt();
      if (!seedResult)
        return std::unexpected(std::move(seedResult.error()));
      auto seed = std::move(*seedResult);
      auto levelConfigResult = configValue("level", level);
      if (!levelConfigResult)
        return std::unexpected(std::move(levelConfigResult.error()));
      auto levelConfig = std::move(*levelConfigResult);
      auto objectLevelResult = levelConfig.toFloat();
      if (!objectLevelResult)
        return std::unexpected(std::move(objectLevelResult.error()));
      auto objectLevel = std::move(*objectLevelResult);
      level = objectLevel;
      auto adjustmentConfigResult = configValue("levelAdjustment", 0);
      if (!adjustmentConfigResult)
        return std::unexpected(std::move(adjustmentConfigResult.error()));
      auto adjustmentConfig = std::move(*adjustmentConfigResult);
      auto adjustmentResult = adjustmentConfig.toFloat();
      if (!adjustmentResult)
        return std::unexpected(std::move(adjustmentResult.error()));
      auto adjustment = std::move(*adjustmentResult);
      level += adjustment;
      auto initialItemsResult = configValue("initialItems");
      if (!initialItemsResult)
        return std::unexpected(std::move(initialItemsResult.error()));
      auto initialItems = std::move(*initialItemsResult);
      if (!initialItems.isNull()) {
        auto itemsConfigResult = configValue("initialItems");
        if (!itemsConfigResult)
          return std::unexpected(std::move(itemsConfigResult.error()));
        auto itemsConfig = std::move(*itemsConfigResult);
        auto itemsResult = itemsConfig.iterateArray();
        if (!itemsResult)
          return std::unexpected(std::move(itemsResult.error()));
        auto items = std::move(*itemsResult);
        for (auto const& spec : items) {
          auto itemDatabaseResult = root().itemDatabase();
          if (!itemDatabaseResult)
            return std::unexpected(std::move(itemDatabaseResult.error()));
          auto itemDatabase = std::move(*itemDatabaseResult);
          auto descriptorResult = ItemDescriptor::create(spec);
          if (!descriptorResult)
            return std::unexpected(std::move(descriptorResult.error()));
          auto descriptor = std::move(*descriptorResult);
          auto itemResult = itemDatabase->item(descriptor, level, ++seed);
          if (!itemResult)
            return std::unexpected(std::move(itemResult.error()));
          auto item = std::move(*itemResult);
          auto operation2 = m_items->addItems(std::move(item));
          if (!operation2)
            return std::unexpected(std::move(operation2.error()));
        }
      }
      auto poolConfigResult = configValue("treasurePools");
      if (!poolConfigResult)
        return std::unexpected(std::move(poolConfigResult.error()));
      auto poolConfig = std::move(*poolConfigResult);
      if (!poolConfig.isNull()) {
        auto poolsConfigResult = configValue("treasurePools");
        if (!poolsConfigResult)
          return std::unexpected(std::move(poolsConfigResult.error()));
        auto poolsConfig = std::move(*poolsConfigResult);
        auto poolsResult = poolsConfig.toArray();
        if (!poolsResult)
          return std::unexpected(std::move(poolsResult.error()));
        auto pools = std::move(*poolsResult);
        auto poolValue = root().random().randValueFrom(pools);
        auto poolResult = poolValue.toString();
        if (!poolResult)
          return std::unexpected(std::move(poolResult.error()));
        auto pool = std::move(*poolResult);
        auto treasureResult = root().treasureDatabase();
        if (!treasureResult)
          return std::unexpected(std::move(treasureResult.error()));
        auto treasure = std::move(*treasureResult);
        auto operation3 = treasure->fillWithTreasure(m_items, pool, level, ++seed);
        if (!operation3)
          return std::unexpected(std::move(operation3.error()));
      }
      itemsUpdated();
    }
    return {};
  }();
  if (!initialized) {
    auto cleaned = Object::uninit();
    if (!cleaned)
      return std::unexpected(initialized.error().with_context(Star::format("Container initialization cleanup failed: {}", Star::output_exception(cleaned.error(), false))));
    return std::unexpected(std::move(initialized.error()));
  }
  return {};
}

std::expected<void, StarException> ContainerObject::update(float dt, std::uint64_t currentStep) {
  auto operation1 = Object::update(dt, currentStep);
  if (!operation1)
    return std::unexpected(std::move(operation1.error()));
  if (isMaster()) {
    auto currentWorldResult = world();
    if (!currentWorldResult)
      return std::unexpected(std::move(currentWorldResult.error()));
    auto currentWorld = std::move(*currentWorldResult);
    for (auto const& item : std::exchange(m_lostItems, {})) {
      auto dropResult = ItemDrop::createRandomizedDrop(root(), item, position());
      if (!dropResult)
        return std::unexpected(std::move(dropResult.error()));
      auto drop = std::move(*dropResult);
      auto operation2 = currentWorld.get().addEntity(drop);
      if (!operation2)
        return std::unexpected(std::move(operation2.error()));
    }
    if (m_crafting.get()) {
      auto operation3 = tickCrafting(dt);
      if (!operation3)
        return std::unexpected(std::move(operation3.error()));
    }
    if (m_autoCloseCooldown > 0) {
      m_autoCloseCooldown -= 1;
      if (m_autoCloseCooldown <= 0) {
        --m_count;
        if (m_count <= 0) {
          m_count = 0;
          m_opened.set(0);
        } else {
          auto cooldownConfigResult = configValue("autoCloseCooldown");
          if (!cooldownConfigResult)
            return std::unexpected(std::move(cooldownConfigResult.error()));
          auto cooldownConfig = std::move(*cooldownConfigResult);
          auto cooldownResult = cooldownConfig.toInt();
          if (!cooldownResult)
            return std::unexpected(std::move(cooldownResult.error()));
          auto cooldown = std::move(*cooldownResult);
          m_autoCloseCooldown = cooldown;
        }
      }
    }
    m_ageItemsTimer.update(currentWorld.get().epochTime());
    auto ageConfigResult = configValue("ageItemsEvery", 10);
    if (!ageConfigResult)
      return std::unexpected(std::move(ageConfigResult.error()));
    auto ageConfig = std::move(*ageConfigResult);
    auto ageIntervalResult = ageConfig.toDouble();
    if (!ageIntervalResult)
      return std::unexpected(std::move(ageIntervalResult.error()));
    auto ageInterval = std::move(*ageIntervalResult);
    if (m_ageItemsTimer.elapsedTime() > ageInterval) {
      auto multiplierConfigResult = configValue("itemAgeMultiplier", 1.0f);
      if (!multiplierConfigResult)
        return std::unexpected(std::move(multiplierConfigResult.error()));
      auto multiplierConfig = std::move(*multiplierConfigResult);
      auto multiplierResult = multiplierConfig.toDouble();
      if (!multiplierResult)
        return std::unexpected(std::move(multiplierResult.error()));
      auto multiplier = std::move(*multiplierResult);
      double elapsed = m_ageItemsTimer.elapsedTime() * multiplier;
      for (auto& item : m_items->items()) {
        auto databaseResult = root().itemDatabase();
        if (!databaseResult)
          return std::unexpected(std::move(databaseResult.error()));
        auto database = std::move(*databaseResult);
        auto agedResult = database->ageItem(item, elapsed);
        if (!agedResult)
          return std::unexpected(std::move(agedResult.error()));
        auto aged = std::move(*agedResult);
        if (aged)
          itemsUpdated();
      }
      m_ageItemsTimer.setElapsedTime(0.0);
    }
    if (std::exchange(m_runUpdatedCallback, false)) {
      auto operation4 = m_scriptComponent.invoke("containerCallback");
      if (!operation4)
        return std::unexpected(std::move(operation4.error()));
    }
  } else {
    setImageKey("key", Star::to_string(m_currentState));
    setImageKey("state", m_crafting.get() ? String("crafting") : String("idle"));
    m_animationFrameCooldown -= 1;
  }
  return {};
}

std::expected<void, StarException> ContainerObject::collectRenderEvents(RenderCallback& renderCallback) {
  auto assetsResult = root().assets();
  if (!assetsResult)
    return std::unexpected(std::move(assetsResult.error()));
  auto assets = std::move(*assetsResult);
  if (m_animationFrameCooldown <= 0) {
    if (m_opened.get() != m_currentState) {
      if (m_currentState == 0) {
        auto openConfigResult = configValue("openSounds");
        if (!openConfigResult)
          return std::unexpected(std::move(openConfigResult.error()));
        auto openConfig = std::move(*openConfigResult);
        if (!openConfig.isNull()) {
          auto soundsConfigResult = configValue("openSounds");
          if (!soundsConfigResult)
            return std::unexpected(std::move(soundsConfigResult.error()));
          auto soundsConfig = std::move(*soundsConfigResult);
          auto soundsResult = soundsConfig.toArray();
          if (!soundsResult)
            return std::unexpected(std::move(soundsResult.error()));
          auto sounds = std::move(*soundsResult);
          auto soundValue = root().random().randValueFrom(sounds);
          auto soundNameResult = soundValue.toString();
          if (!soundNameResult)
            return std::unexpected(std::move(soundNameResult.error()));
          auto soundName = std::move(*soundNameResult);
          auto audioResult = assets->audio(soundName);
          if (!audioResult)
            return std::unexpected(std::move(audioResult.error()));
          auto audio = std::move(*audioResult);
          auto instanceResult = AudioInstance::create(*audio);
          if (!instanceResult)
            return std::unexpected(std::move(instanceResult.error()));
          auto instance = std::move(*instanceResult);
          instance->setPosition(position());
          instance->setRangeMultiplier(config()->soundEffectRangeMultiplier);
          auto operation1 = renderCallback.addAudio(std::move(instance));
          if (!operation1)
            return std::unexpected(std::move(operation1.error()));
        }
      }
      auto openFrameConfigResult = configValue("openFrameIndex", 2);
      if (!openFrameConfigResult)
        return std::unexpected(std::move(openFrameConfigResult.error()));
      auto openFrameConfig = std::move(*openFrameConfigResult);
      auto openFrameResult = openFrameConfig.toInt();
      if (!openFrameResult)
        return std::unexpected(std::move(openFrameResult.error()));
      auto openFrame = std::move(*openFrameResult);
      if (m_currentState == openFrame) {
        auto closeConfigResult = configValue("closeSounds");
        if (!closeConfigResult)
          return std::unexpected(std::move(closeConfigResult.error()));
        auto closeConfig = std::move(*closeConfigResult);
        if (!closeConfig.isNull()) {
          auto soundsConfigResult = configValue("closeSounds");
          if (!soundsConfigResult)
            return std::unexpected(std::move(soundsConfigResult.error()));
          auto soundsConfig = std::move(*soundsConfigResult);
          auto soundsResult = soundsConfig.toArray();
          if (!soundsResult)
            return std::unexpected(std::move(soundsResult.error()));
          auto sounds = std::move(*soundsResult);
          auto soundValue = root().random().randValueFrom(sounds);
          auto soundNameResult = soundValue.toString();
          if (!soundNameResult)
            return std::unexpected(std::move(soundNameResult.error()));
          auto soundName = std::move(*soundNameResult);
          auto audioResult = assets->audio(soundName);
          if (!audioResult)
            return std::unexpected(std::move(audioResult.error()));
          auto audio = std::move(*audioResult);
          auto instanceResult = AudioInstance::create(*audio);
          if (!instanceResult)
            return std::unexpected(std::move(instanceResult.error()));
          auto instance = std::move(*instanceResult);
          instance->setPosition(position());
          instance->setRangeMultiplier(config()->soundEffectRangeMultiplier);
          auto operation2 = renderCallback.addAudio(std::move(instance));
          if (!operation2)
            return std::unexpected(std::move(operation2.error()));
        }
      }
      if (m_opened.get() < m_currentState)
        m_currentState -= 1;
      else
        m_currentState += 1;
      auto frameConfigResult = configValue("frameCooldown");
      if (!frameConfigResult)
        return std::unexpected(std::move(frameConfigResult.error()));
      auto frameConfig = std::move(*frameConfigResult);
      auto cooldownResult = frameConfig.toInt();
      if (!cooldownResult)
        return std::unexpected(std::move(cooldownResult.error()));
      auto cooldown = std::move(*cooldownResult);
      m_animationFrameCooldown = cooldown;
    } else {
      m_animationFrameCooldown = 0;
    }
  }
  return Object::collectRenderEvents(renderCallback);
}

std::expected<void, StarException> ContainerObject::destroy(std::optional<std::reference_wrapper<RenderCallback>> renderCallback) {
  auto operation1 = Object::destroy(renderCallback);
  if (!operation1)
    return std::unexpected(std::move(operation1.error()));
  if (isMaster()) {
    auto currentWorldResult = world();
    if (!currentWorldResult)
      return std::unexpected(std::move(currentWorldResult.error()));
    auto currentWorld = std::move(*currentWorldResult);
    for (auto const& item : m_items->items()) {
      auto dropResult = ItemDrop::createRandomizedDrop(root(), item, position());
      if (!dropResult)
        return std::unexpected(std::move(dropResult.error()));
      auto drop = std::move(*dropResult);
      auto operation2 = currentWorld.get().addEntity(drop);
      if (!operation2)
        return std::unexpected(std::move(operation2.error()));
    }
  }
  return {};
}

std::expected<std::optional<ChainableJsonMessageResponse>, StarException> ContainerObject::receiveMessage(ConnectionId sendingConnection, String const& message, JsonArray const& args) {
  auto databaseResult = root().itemDatabase();
  if (!databaseResult)
    return std::unexpected(std::move(databaseResult.error()));
  auto database = std::move(*databaseResult);
  auto matches0Result = message.equals_ignore_case("startCrafting");
  if (!matches0Result)
    return std::unexpected(std::move(matches0Result.error()));
  auto matches0 = std::move(*matches0Result);
  if (matches0) {
    auto operation1 = startCrafting();
    if (!operation1)
      return std::unexpected(std::move(operation1.error()));
    return ChainableJsonMessageResponse(Json());
  }
  auto matches1Result = message.equals_ignore_case("stopCrafting");
  if (!matches1Result)
    return std::unexpected(std::move(matches1Result.error()));
  auto matches1 = std::move(*matches1Result);
  if (matches1) {
    auto operation2 = stopCrafting();
    if (!operation2)
      return std::unexpected(std::move(operation2.error()));
    return ChainableJsonMessageResponse(Json());
  }
  auto matches2Result = message.equals_ignore_case("burnContainerContents");
  if (!matches2Result)
    return std::unexpected(std::move(matches2Result.error()));
  auto matches2 = std::move(*matches2Result);
  if (matches2) {
    auto operation3 = burnContainerContents();
    if (!operation3)
      return std::unexpected(std::move(operation3.error()));
    return ChainableJsonMessageResponse(Json());
  }
  auto matches3Result = message.equals_ignore_case("addItems");
  if (!matches3Result)
    return std::unexpected(std::move(matches3Result.error()));
  auto matches3 = std::move(*matches3Result);
  if (matches3) {
    auto itemArgResult = args.at(0);
    if (!itemArgResult)
      return std::unexpected(std::move(itemArgResult.error()));
    auto itemArg = std::move(*itemArgResult);
    auto itemResult = database->fromJson(itemArg.get());
    if (!itemResult)
      return std::unexpected(std::move(itemResult.error()));
    auto item = std::move(*itemResult);
    auto resultResult = doAddItems(item);
    if (!resultResult)
      return std::unexpected(std::move(resultResult.error()));
    auto result = std::move(*resultResult);
    return ChainableJsonMessageResponse(itemSafeDescriptor(result).toJson());
  }
  auto matches4Result = message.equals_ignore_case("putItems");
  if (!matches4Result)
    return std::unexpected(std::move(matches4Result.error()));
  auto matches4 = std::move(*matches4Result);
  if (matches4) {
    auto slotArgResult = args.at(0);
    if (!slotArgResult)
      return std::unexpected(std::move(slotArgResult.error()));
    auto slotArg = std::move(*slotArgResult);
    auto slotResult = slotArg.get().toUInt();
    if (!slotResult)
      return std::unexpected(std::move(slotResult.error()));
    auto slot = std::move(*slotResult);
    auto itemArgResult = args.at(1);
    if (!itemArgResult)
      return std::unexpected(std::move(itemArgResult.error()));
    auto itemArg = std::move(*itemArgResult);
    auto itemResult = database->fromJson(itemArg.get());
    if (!itemResult)
      return std::unexpected(std::move(itemResult.error()));
    auto item = std::move(*itemResult);
    auto resultResult = doPutItems(slot, item);
    if (!resultResult)
      return std::unexpected(std::move(resultResult.error()));
    auto result = std::move(*resultResult);
    return ChainableJsonMessageResponse(itemSafeDescriptor(result).toJson());
  }
  auto matches5Result = message.equals_ignore_case("takeItems");
  if (!matches5Result)
    return std::unexpected(std::move(matches5Result.error()));
  auto matches5 = std::move(*matches5Result);
  if (matches5) {
    auto slotArgResult = args.at(0);
    if (!slotArgResult)
      return std::unexpected(std::move(slotArgResult.error()));
    auto slotArg = std::move(*slotArgResult);
    auto slotResult = slotArg.get().toUInt();
    if (!slotResult)
      return std::unexpected(std::move(slotResult.error()));
    auto slot = std::move(*slotResult);
    auto countArgResult = args.at(1);
    if (!countArgResult)
      return std::unexpected(std::move(countArgResult.error()));
    auto countArg = std::move(*countArgResult);
    auto countResult = countArg.get().toUInt();
    if (!countResult)
      return std::unexpected(std::move(countResult.error()));
    auto count = std::move(*countResult);
    auto resultResult = doTakeItems(slot, count);
    if (!resultResult)
      return std::unexpected(std::move(resultResult.error()));
    auto result = std::move(*resultResult);
    return ChainableJsonMessageResponse(itemSafeDescriptor(result).toJson());
  }
  auto matches6Result = message.equals_ignore_case("swapItems");
  if (!matches6Result)
    return std::unexpected(std::move(matches6Result.error()));
  auto matches6 = std::move(*matches6Result);
  if (matches6) {
    auto slotArgResult = args.at(0);
    if (!slotArgResult)
      return std::unexpected(std::move(slotArgResult.error()));
    auto slotArg = std::move(*slotArgResult);
    auto slotResult = slotArg.get().toUInt();
    if (!slotResult)
      return std::unexpected(std::move(slotResult.error()));
    auto slot = std::move(*slotResult);
    auto itemArgResult = args.at(1);
    if (!itemArgResult)
      return std::unexpected(std::move(itemArgResult.error()));
    auto itemArg = std::move(*itemArgResult);
    auto itemResult = database->fromJson(itemArg.get());
    if (!itemResult)
      return std::unexpected(std::move(itemResult.error()));
    auto item = std::move(*itemResult);
    auto combineArg = args.get(2);
    auto combineResult = combineArg.optBool();
    if (!combineResult)
      return std::unexpected(std::move(combineResult.error()));
    auto combine = std::move(*combineResult);
    auto resultResult = doSwapItems(slot, item, combine.value_or(true));
    if (!resultResult)
      return std::unexpected(std::move(resultResult.error()));
    auto result = std::move(*resultResult);
    return ChainableJsonMessageResponse(itemSafeDescriptor(result).toJson());
  }
  auto matches7Result = message.equals_ignore_case("applyAugment");
  if (!matches7Result)
    return std::unexpected(std::move(matches7Result.error()));
  auto matches7 = std::move(*matches7Result);
  if (matches7) {
    auto slotArgResult = args.at(0);
    if (!slotArgResult)
      return std::unexpected(std::move(slotArgResult.error()));
    auto slotArg = std::move(*slotArgResult);
    auto slotResult = slotArg.get().toUInt();
    if (!slotResult)
      return std::unexpected(std::move(slotResult.error()));
    auto slot = std::move(*slotResult);
    auto itemArgResult = args.at(1);
    if (!itemArgResult)
      return std::unexpected(std::move(itemArgResult.error()));
    auto itemArg = std::move(*itemArgResult);
    auto itemResult = database->fromJson(itemArg.get());
    if (!itemResult)
      return std::unexpected(std::move(itemResult.error()));
    auto item = std::move(*itemResult);
    auto resultResult = doApplyAugment(slot, item);
    if (!resultResult)
      return std::unexpected(std::move(resultResult.error()));
    auto result = std::move(*resultResult);
    return ChainableJsonMessageResponse(itemSafeDescriptor(result).toJson());
  }
  auto matches8Result = message.equals_ignore_case("consumeItems");
  if (!matches8Result)
    return std::unexpected(std::move(matches8Result.error()));
  auto matches8 = std::move(*matches8Result);
  if (matches8) {
    auto descriptorArgResult = args.at(0);
    if (!descriptorArgResult)
      return std::unexpected(std::move(descriptorArgResult.error()));
    auto descriptorArg = std::move(*descriptorArgResult);
    auto descriptorResult = ItemDescriptor::create(descriptorArg.get());
    if (!descriptorResult)
      return std::unexpected(std::move(descriptorResult.error()));
    auto descriptor = std::move(*descriptorResult);
    auto resultResult = doConsumeItems(descriptor);
    if (!resultResult)
      return std::unexpected(std::move(resultResult.error()));
    auto result = std::move(*resultResult);
    return ChainableJsonMessageResponse(Json(result));
  }
  auto matches9Result = message.equals_ignore_case("consumeItemsAt");
  if (!matches9Result)
    return std::unexpected(std::move(matches9Result.error()));
  auto matches9 = std::move(*matches9Result);
  if (matches9) {
    auto slotArgResult = args.at(0);
    if (!slotArgResult)
      return std::unexpected(std::move(slotArgResult.error()));
    auto slotArg = std::move(*slotArgResult);
    auto slotResult = slotArg.get().toUInt();
    if (!slotResult)
      return std::unexpected(std::move(slotResult.error()));
    auto slot = std::move(*slotResult);
    auto countArgResult = args.at(1);
    if (!countArgResult)
      return std::unexpected(std::move(countArgResult.error()));
    auto countArg = std::move(*countArgResult);
    auto countResult = countArg.get().toUInt();
    if (!countResult)
      return std::unexpected(std::move(countResult.error()));
    auto count = std::move(*countResult);
    auto resultResult = doConsumeItems(slot, count);
    if (!resultResult)
      return std::unexpected(std::move(resultResult.error()));
    auto result = std::move(*resultResult);
    return ChainableJsonMessageResponse(Json(result));
  }
  auto matches10Result = message.equals_ignore_case("clearContainer");
  if (!matches10Result)
    return std::unexpected(std::move(matches10Result.error()));
  auto matches10 = std::move(*matches10Result);
  if (matches10) {
    auto itemsResult = doClearContainer();
    if (!itemsResult)
      return std::unexpected(std::move(itemsResult.error()));
    auto items = std::move(*itemsResult);
    JsonArray result;
    for (auto const& item : items)
      result.push_back(itemSafeDescriptor(item).toJson());
    return ChainableJsonMessageResponse(std::move(result));
  }
  return Object::receiveMessage(sendingConnection, message, args);
}

std::expected<InteractAction, StarException> ContainerObject::interact(InteractRequest const&) {
  return InteractAction(InteractActionType::OpenContainer, entityId(), Json());
}

std::expected<Json, StarException> ContainerObject::containerGuiConfig() const {
  auto assetsResult = root().assets();
  if (!assetsResult)
    return std::unexpected(std::move(assetsResult.error()));
  auto assets = std::move(*assetsResult);
  auto configResult = configValue("uiConfig");
  if (!configResult)
    return std::unexpected(std::move(configResult.error()));
  auto config = std::move(*configResult);
  auto pathResult = config.toString();
  if (!pathResult)
    return std::unexpected(std::move(pathResult.error()));
  auto path = std::move(*pathResult);
  auto resolvedResult = path.replace("<slots>", Star::to_string(m_items->size()));
  if (!resolvedResult)
    return std::unexpected(std::move(resolvedResult.error()));
  auto resolved = std::move(*resolvedResult);
  return assets->json(resolved);
}

std::expected<String, StarException> ContainerObject::containerDescription() const {
  return Object::shortDescription();
}

std::expected<String, StarException> ContainerObject::containerSubTitle() const {
  auto assetsResult = root().assets();
  if (!assetsResult)
    return std::unexpected(std::move(assetsResult.error()));
  auto assets = std::move(*assetsResult);
  auto categoriesResult = assets->json("/items/categories.config:labels");
  if (!categoriesResult)
    return std::unexpected(std::move(categoriesResult.error()));
  auto categories = std::move(*categoriesResult);
  return categories.getString(Object::category(), Object::category());
}

std::expected<ItemDescriptor, StarException> ContainerObject::iconItem() const {
  auto configResult = configValue("hasWindowIcon", true);
  if (!configResult)
    return std::unexpected(std::move(configResult.error()));
  auto config = std::move(*configResult);
  auto hasIconResult = config.toBool();
  if (!hasIconResult)
    return std::unexpected(std::move(hasIconResult.error()));
  auto hasIcon = std::move(*hasIconResult);
  if (hasIcon) {
    auto objectNameResult = name();
    if (!objectNameResult)
      return std::unexpected(std::move(objectNameResult.error()));
    auto objectName = std::move(*objectNameResult);
    return ItemDescriptor::create(objectName, 1);
  }
  return ItemDescriptor();
}

std::shared_ptr<ItemBag const> ContainerObject::itemBag() const {
  return m_items;
}

std::expected<void, StarException> ContainerObject::containerOpen() {
  auto frameConfigResult = configValue("openFrameIndex", 2);
  if (!frameConfigResult)
    return std::unexpected(std::move(frameConfigResult.error()));
  auto frameConfig = std::move(*frameConfigResult);
  auto frameResult = frameConfig.toInt();
  if (!frameResult)
    return std::unexpected(std::move(frameResult.error()));
  auto frame = std::move(*frameResult);
  m_opened.set(frame);
  m_count++;
  auto cooldownConfigResult = configValue("autoCloseCooldown");
  if (!cooldownConfigResult)
    return std::unexpected(std::move(cooldownConfigResult.error()));
  auto cooldownConfig = std::move(*cooldownConfigResult);
  auto cooldownResult = cooldownConfig.toInt();
  if (!cooldownResult)
    return std::unexpected(std::move(cooldownResult.error()));
  auto cooldown = std::move(*cooldownResult);
  m_autoCloseCooldown = cooldown;
  return {};
}

std::expected<void, StarException> ContainerObject::containerClose() {
  --m_count;
  if (m_count <= 0) {
    m_count = 0;
    m_opened.set(0);
  }
  return {};
}

std::expected<RpcPromise<std::shared_ptr<Item>>, StarException> ContainerObject::addItems(std::shared_ptr<Item> const& items) {
  if (isSlave()) {
    auto currentWorldResult = world();
    if (!currentWorldResult)
      return std::unexpected(std::move(currentWorldResult.error()));
    auto currentWorld = std::move(*currentWorldResult);
    auto promiseResult = currentWorld.get().sendEntityMessage(entityId(), "addItems", {itemSafeDescriptor(items).toJson()});
    if (!promiseResult)
      return std::unexpected(std::move(promiseResult.error()));
    auto promise = std::move(*promiseResult);
    return promise.wrap([owner = std::ref(root())](Json res) -> std::expected<std::shared_ptr<Item>, StarException> {
  auto databaseResult = owner.get().itemDatabase();
  if (!databaseResult)
    return std::unexpected(std::move(databaseResult.error()));
  auto database = std::move(*databaseResult);
  auto descriptorResult = ItemDescriptor::create(res);
  if (!descriptorResult)
    return std::unexpected(std::move(descriptorResult.error()));
  auto descriptor = std::move(*descriptorResult);
  return database->item(descriptor);
});
  }
  auto resultResult = doAddItems(items);
  if (!resultResult)
    return std::unexpected(std::move(resultResult.error()));
  auto result = std::move(*resultResult);
  return RpcPromise<std::shared_ptr<Item>>::createFulfilled(std::move(result));
}

std::expected<RpcPromise<std::shared_ptr<Item>>, StarException> ContainerObject::putItems(std::size_t pos, std::shared_ptr<Item> const& items) {
  if (isSlave()) {
    auto currentWorldResult = world();
    if (!currentWorldResult)
      return std::unexpected(std::move(currentWorldResult.error()));
    auto currentWorld = std::move(*currentWorldResult);
    auto promiseResult = currentWorld.get().sendEntityMessage(entityId(), "putItems", {itemSafeDescriptor(items).toJson()});
    if (!promiseResult)
      return std::unexpected(std::move(promiseResult.error()));
    auto promise = std::move(*promiseResult);
    return promise.wrap([owner = std::ref(root())](Json res) -> std::expected<std::shared_ptr<Item>, StarException> {
  auto databaseResult = owner.get().itemDatabase();
  if (!databaseResult)
    return std::unexpected(std::move(databaseResult.error()));
  auto database = std::move(*databaseResult);
  auto descriptorResult = ItemDescriptor::create(res);
  if (!descriptorResult)
    return std::unexpected(std::move(descriptorResult.error()));
  auto descriptor = std::move(*descriptorResult);
  return database->item(descriptor);
});
  }
  auto resultResult = doPutItems(pos, items);
  if (!resultResult)
    return std::unexpected(std::move(resultResult.error()));
  auto result = std::move(*resultResult);
  return RpcPromise<std::shared_ptr<Item>>::createFulfilled(std::move(result));
}

std::expected<RpcPromise<std::shared_ptr<Item>>, StarException> ContainerObject::takeItems(std::size_t slot, std::size_t count) {
  if (isSlave()) {
    auto currentWorldResult = world();
    if (!currentWorldResult)
      return std::unexpected(std::move(currentWorldResult.error()));
    auto currentWorld = std::move(*currentWorldResult);
    auto promiseResult = currentWorld.get().sendEntityMessage(entityId(), "takeItems", {slot, count});
    if (!promiseResult)
      return std::unexpected(std::move(promiseResult.error()));
    auto promise = std::move(*promiseResult);
    return promise.wrap([owner = std::ref(root())](Json res) -> std::expected<std::shared_ptr<Item>, StarException> {
  auto databaseResult = owner.get().itemDatabase();
  if (!databaseResult)
    return std::unexpected(std::move(databaseResult.error()));
  auto database = std::move(*databaseResult);
  auto descriptorResult = ItemDescriptor::create(res);
  if (!descriptorResult)
    return std::unexpected(std::move(descriptorResult.error()));
  auto descriptor = std::move(*descriptorResult);
  return database->item(descriptor);
});
  }
  auto resultResult = doTakeItems(slot, count);
  if (!resultResult)
    return std::unexpected(std::move(resultResult.error()));
  auto result = std::move(*resultResult);
  return RpcPromise<std::shared_ptr<Item>>::createFulfilled(std::move(result));
}

std::expected<RpcPromise<std::shared_ptr<Item>>, StarException> ContainerObject::swapItems(std::size_t slot, std::shared_ptr<Item> const& items, bool tryCombine) {
  if (isSlave()) {
    auto currentWorldResult = world();
    if (!currentWorldResult)
      return std::unexpected(std::move(currentWorldResult.error()));
    auto currentWorld = std::move(*currentWorldResult);
    auto promiseResult = currentWorld.get().sendEntityMessage(entityId(), "swapItems", {slot, itemSafeDescriptor(items).toJson(), tryCombine});
    if (!promiseResult)
      return std::unexpected(std::move(promiseResult.error()));
    auto promise = std::move(*promiseResult);
    return promise.wrap([owner = std::ref(root())](Json res) -> std::expected<std::shared_ptr<Item>, StarException> {
  auto databaseResult = owner.get().itemDatabase();
  if (!databaseResult)
    return std::unexpected(std::move(databaseResult.error()));
  auto database = std::move(*databaseResult);
  auto descriptorResult = ItemDescriptor::create(res);
  if (!descriptorResult)
    return std::unexpected(std::move(descriptorResult.error()));
  auto descriptor = std::move(*descriptorResult);
  return database->item(descriptor);
});
  }
  auto resultResult = doSwapItems(slot, items, tryCombine);
  if (!resultResult)
    return std::unexpected(std::move(resultResult.error()));
  auto result = std::move(*resultResult);
  return RpcPromise<std::shared_ptr<Item>>::createFulfilled(std::move(result));
}

std::expected<RpcPromise<std::shared_ptr<Item>>, StarException> ContainerObject::applyAugment(std::size_t slot, std::shared_ptr<Item> const& augment) {
  if (isSlave()) {
    auto currentWorldResult = world();
    if (!currentWorldResult)
      return std::unexpected(std::move(currentWorldResult.error()));
    auto currentWorld = std::move(*currentWorldResult);
    auto promiseResult = currentWorld.get().sendEntityMessage(entityId(), "applyAugment", {slot, itemSafeDescriptor(augment).toJson()});
    if (!promiseResult)
      return std::unexpected(std::move(promiseResult.error()));
    auto promise = std::move(*promiseResult);
    return promise.wrap([owner = std::ref(root())](Json res) -> std::expected<std::shared_ptr<Item>, StarException> {
  auto databaseResult = owner.get().itemDatabase();
  if (!databaseResult)
    return std::unexpected(std::move(databaseResult.error()));
  auto database = std::move(*databaseResult);
  auto descriptorResult = ItemDescriptor::create(res);
  if (!descriptorResult)
    return std::unexpected(std::move(descriptorResult.error()));
  auto descriptor = std::move(*descriptorResult);
  return database->item(descriptor);
});
  }
  auto resultResult = doApplyAugment(slot, augment);
  if (!resultResult)
    return std::unexpected(std::move(resultResult.error()));
  auto result = std::move(*resultResult);
  return RpcPromise<std::shared_ptr<Item>>::createFulfilled(std::move(result));
}

std::expected<RpcPromise<bool>, StarException> ContainerObject::consumeItems(ItemDescriptor const& descriptor) {
  if (isSlave()) {
    auto currentWorldResult = world();
    if (!currentWorldResult)
      return std::unexpected(std::move(currentWorldResult.error()));
    auto currentWorld = std::move(*currentWorldResult);
    auto promiseResult = currentWorld.get().sendEntityMessage(entityId(), "consumeItems", {descriptor.toJson()});
    if (!promiseResult)
      return std::unexpected(std::move(promiseResult.error()));
    auto promise = std::move(*promiseResult);
    return promise.wrap([](Json res) { return res.toBool(); });
  }
  auto resultResult = doConsumeItems(descriptor);
  if (!resultResult)
    return std::unexpected(std::move(resultResult.error()));
  auto result = std::move(*resultResult);
  return RpcPromise<bool>::createFulfilled(result);
}

std::expected<RpcPromise<bool>, StarException> ContainerObject::consumeItems(std::size_t pos, std::size_t count) {
  if (isSlave()) {
    auto currentWorldResult = world();
    if (!currentWorldResult)
      return std::unexpected(std::move(currentWorldResult.error()));
    auto currentWorld = std::move(*currentWorldResult);
    auto promiseResult = currentWorld.get().sendEntityMessage(entityId(), "consumeItemsAt", {pos, count});
    if (!promiseResult)
      return std::unexpected(std::move(promiseResult.error()));
    auto promise = std::move(*promiseResult);
    return promise.wrap([](Json res) { return res.toBool(); });
  }
  auto resultResult = doConsumeItems(pos, count);
  if (!resultResult)
    return std::unexpected(std::move(resultResult.error()));
  auto result = std::move(*resultResult);
  return RpcPromise<bool>::createFulfilled(result);
}

std::expected<RpcPromise<List<std::shared_ptr<Item>>>, StarException> ContainerObject::clearContainer() {
  if (isSlave()) {
    auto currentWorldResult = world();
    if (!currentWorldResult)
      return std::unexpected(std::move(currentWorldResult.error()));
    auto currentWorld = std::move(*currentWorldResult);
    auto promiseResult = currentWorld.get().sendEntityMessage(entityId(), "clearContainer", {});
    if (!promiseResult)
      return std::unexpected(std::move(promiseResult.error()));
    auto promise = std::move(*promiseResult);
    return promise.wrap([owner = std::ref(root())](Json res) -> std::expected<List<std::shared_ptr<Item>>, StarException> {
  auto databaseResult = owner.get().itemDatabase();
  if (!databaseResult)
    return std::unexpected(std::move(databaseResult.error()));
  auto database = std::move(*databaseResult);
  auto entriesResult = res.toArray();
  if (!entriesResult)
    return std::unexpected(std::move(entriesResult.error()));
  auto entries = std::move(*entriesResult);
  List<std::shared_ptr<Item>> items;
  for (auto const& entry : entries) {
    auto descriptorResult = ItemDescriptor::create(entry);
    if (!descriptorResult)
      return std::unexpected(std::move(descriptorResult.error()));
    auto descriptor = std::move(*descriptorResult);
    auto itemResult = database->item(descriptor);
    if (!itemResult)
      return std::unexpected(std::move(itemResult.error()));
    auto item = std::move(*itemResult);
    items.push_back(std::move(item));
  }
  return items;
});
  }
  auto resultResult = doClearContainer();
  if (!resultResult)
    return std::unexpected(std::move(resultResult.error()));
  auto result = std::move(*resultResult);
  return RpcPromise<List<std::shared_ptr<Item>>>::createFulfilled(std::move(result));
}

bool ContainerObject::isCrafting() const {
  return m_crafting.get();
}

std::expected<void, StarException> ContainerObject::startCrafting() {
  if (isSlave()) {
    auto currentWorldResult = world();
    if (!currentWorldResult)
      return std::unexpected(std::move(currentWorldResult.error()));
    auto currentWorld = std::move(*currentWorldResult);
    auto operation1 = currentWorld.get().sendEntityMessage(entityId(), "startCrafting");
    if (!operation1)
      return std::unexpected(std::move(operation1.error()));
  } else {
    if (m_crafting.get())
      return {};
    auto inputItems = m_items->items();
    auto operation2 = inputItems.pop_back_checked();
    if (!operation2)
      return std::unexpected(std::move(operation2.error()));
    auto recipeResult = recipeForMaterials(inputItems);
    if (!recipeResult)
      return std::unexpected(std::move(recipeResult.error()));
    auto recipe = std::move(*recipeResult);
    m_goalRecipe = std::move(recipe);
    m_crafting.set(true);
    itemsUpdated();
    // tickCrafting validates the selected recipe.
  }
  return {};
}

std::expected<void, StarException> ContainerObject::stopCrafting() {
  if (isSlave()) {
    auto currentWorldResult = world();
    if (!currentWorldResult)
      return std::unexpected(std::move(currentWorldResult.error()));
    auto currentWorld = std::move(*currentWorldResult);
    auto operation1 = currentWorld.get().sendEntityMessage(entityId(), "stopCrafting");
    if (!operation1)
      return std::unexpected(std::move(operation1.error()));
  } else {
    if (!m_crafting.get())
      return {};
    m_crafting.set(false);
    m_craftingProgress.set(0);
    m_goalRecipe = ItemRecipe();
  }
  return {};
}

float ContainerObject::craftingProgress() const {
  if (!isCrafting())
    return 1;
  return std::clamp(m_craftingProgress.get(), 0.0f, 1.0f);
}

std::expected<void, StarException> ContainerObject::burnContainerContents() {
  auto currentWorldResult = world();
  if (!currentWorldResult)
    return std::unexpected(std::move(currentWorldResult.error()));
  auto currentWorld = std::move(*currentWorldResult);
  if (isSlave()) {
    auto operation1 = currentWorld.get().sendEntityMessage(entityId(), "burnContainerContents");
    if (!operation1)
      return std::unexpected(std::move(operation1.error()));
  } else {
    auto operation2 = stopCrafting();
    if (!operation2)
      return std::unexpected(std::move(operation2.error()));
    auto fuelConfigResult = currentWorld.get().getProperty("ship.fuel", 0);
    if (!fuelConfigResult)
      return std::unexpected(std::move(fuelConfigResult.error()));
    auto fuelConfig = std::move(*fuelConfigResult);
    auto levelResult = fuelConfig.toUInt();
    if (!levelResult)
      return std::unexpected(std::move(levelResult.error()));
    auto level = std::move(*levelResult);
    auto maxConfigResult = currentWorld.get().getProperty("ship.maxFuel", 0);
    if (!maxConfigResult)
      return std::unexpected(std::move(maxConfigResult.error()));
    auto maxConfig = std::move(*maxConfigResult);
    auto maxLevelResult = maxConfig.toUInt();
    if (!maxLevelResult)
      return std::unexpected(std::move(maxLevelResult.error()));
    auto maxLevel = std::move(*maxLevelResult);
    for (auto& item : m_items->items()) {
      if (level > maxLevel)
        level = maxLevel;
      if (maxLevel == level)
        break;
      auto leftToFill = maxLevel - level;
      if (item) {
        auto itemFuelConfigResult = item->instanceValue("fuelAmount", 0);
        if (!itemFuelConfigResult)
          return std::unexpected(std::move(itemFuelConfigResult.error()));
        auto itemFuelConfig = std::move(*itemFuelConfigResult);
        auto fuelSingleResult = itemFuelConfig.toUInt();
        if (!fuelSingleResult)
          return std::unexpected(std::move(fuelSingleResult.error()));
        auto fuelSingle = std::move(*fuelSingleResult);
        if (fuelSingle > 0) {
          auto count = std::min<std::uint64_t>((leftToFill + fuelSingle - 1) / fuelSingle, item->count());
          level = std::min(maxLevel, level + fuelSingle * count);
          auto consumed = item->consume(count);
          if constexpr (Star::DebugEnabled) {
            Star::star_assert_failed((consumed));
          }
          ((void)(consumed));
        }
      }
    }
    itemsUpdated();
    auto operation3 = currentWorld.get().setProperty("ship.fuel", level);
    if (!operation3)
      return std::unexpected(std::move(operation3.error()));
  }
  return {};
}

std::expected<void, StarException> ContainerObject::getNetStates(bool initial) {
  auto operation1 = Object::getNetStates(initial);
  if (!operation1)
    return std::unexpected(std::move(operation1.error()));
  if (m_itemsNetState.pullUpdated()) {
    DataStreamBuffer ds(m_itemsNetState.get());
    auto operation2 = m_items->read(ds);
    if (!operation2)
      return std::unexpected(std::move(operation2.error()));
    itemsUpdated();
  }
  return {};
}

std::expected<void, StarException> ContainerObject::setNetStates() {
  auto operation1 = Object::setNetStates();
  if (!operation1)
    return std::unexpected(std::move(operation1.error()));
  if (std::exchange(m_itemsUpdated, false)) {
    DataStreamBuffer ds;
    auto operation2 = m_items->write(ds);
    if (!operation2)
      return std::unexpected(std::move(operation2.error()));
    m_itemsNetState.set(ds.takeData());
  }
  return {};
}

std::expected<void, StarException> ContainerObject::readStoredData(Json const& diskStore) {
  auto operation1 = Object::readStoredData(diskStore);
  if (!operation1)
    return std::unexpected(std::move(operation1.error()));
  auto openedResult = diskStore.getInt("opened");
  if (!openedResult)
    return std::unexpected(std::move(openedResult.error()));
  auto opened = std::move(*openedResult);
  m_opened.set(opened);
  auto currentStateResult = diskStore.getInt("currentState");
  if (!currentStateResult)
    return std::unexpected(std::move(currentStateResult.error()));
  auto currentState = std::move(*currentStateResult);
  m_currentState = currentState;
  auto craftingResult = diskStore.getBool("crafting");
  if (!craftingResult)
    return std::unexpected(std::move(craftingResult.error()));
  auto crafting = std::move(*craftingResult);
  m_crafting.set(crafting);
  auto progressResult = diskStore.getFloat("craftingProgress");
  if (!progressResult)
    return std::unexpected(std::move(progressResult.error()));
  auto progress = std::move(*progressResult);
  m_craftingProgress.set(progress);
  auto initializedResult = diskStore.getBool("initialized");
  if (!initializedResult)
    return std::unexpected(std::move(initializedResult.error()));
  auto initialized = std::move(*initializedResult);
  m_initialized = initialized;
  auto itemsConfigResult = diskStore.get("items");
  if (!itemsConfigResult)
    return std::unexpected(std::move(itemsConfigResult.error()));
  auto itemsConfig = std::move(*itemsConfigResult);
  auto itemsResult = ItemBag::loadStore(root(), itemsConfig);
  if (!itemsResult)
    return std::unexpected(std::move(itemsResult.error()));
  auto items = std::move(*itemsResult);
  m_items = std::make_shared<ItemBag>(std::move(items));
  auto timerConfigResult = diskStore.get("ageItemsTimer");
  if (!timerConfigResult)
    return std::unexpected(std::move(timerConfigResult.error()));
  auto timerConfig = std::move(*timerConfigResult);
  auto timerResult = EpochTimer::create(timerConfig);
  if (!timerResult)
    return std::unexpected(std::move(timerResult.error()));
  auto timer = std::move(*timerResult);
  m_ageItemsTimer = std::move(timer);
  auto slotsConfigResult = configValue("slotCount");
  if (!slotsConfigResult)
    return std::unexpected(std::move(slotsConfigResult.error()));
  auto slotsConfig = std::move(*slotsConfigResult);
  auto slotsResult = slotsConfig.toUInt();
  if (!slotsResult)
    return std::unexpected(std::move(slotsResult.error()));
  auto slots = std::move(*slotsResult);
  auto overflowResult = m_items->resize(slots);
  if (!overflowResult)
    return std::unexpected(std::move(overflowResult.error()));
  auto overflow = std::move(*overflowResult);
  m_lostItems.append_range(std::move(overflow));
  return {};
}

std::expected<Json, StarException> ContainerObject::writeStoredData() const {
  auto storedResult = Object::writeStoredData();
  if (!storedResult)
    return std::unexpected(std::move(storedResult.error()));
  auto stored = std::move(*storedResult);
  auto itemsResult = m_items->diskStore();
  if (!itemsResult)
    return std::unexpected(std::move(itemsResult.error()));
  auto items = std::move(*itemsResult);
  return stored.setAll({{"opened", m_opened.get()}, {"currentState", m_currentState}, {"crafting", m_crafting.get()}, {"craftingProgress", m_craftingProgress.get()}, {"initialized", m_initialized}, {"items", std::move(items)}, {"ageItemsTimer", m_ageItemsTimer.toJson()}});
}

std::expected<ItemRecipe, StarException> ContainerObject::recipeForMaterials(List<std::shared_ptr<Item>> const& inputItems) {
  auto databaseResult = root().itemDatabase();
  if (!databaseResult)
    return std::unexpected(std::move(databaseResult.error()));
  auto database = std::move(*databaseResult);
  auto recipeGroupResult = configValue("recipeGroup");
  if (!recipeGroupResult)
    return std::unexpected(std::move(recipeGroupResult.error()));
  auto recipeGroup = std::move(*recipeGroupResult);
  if (!recipeGroup.isNull()) {
    auto groupResult = recipeGroup.toString();
    if (!groupResult)
      return std::unexpected(std::move(groupResult.error()));
    auto group = std::move(*groupResult);
    return database->getPreciseRecipeForMaterials(group, inputItems, {});
  }
  auto descriptors = inputItems
      | std::views::filter([](std::shared_ptr<Item> const& item) { return bool(item); })
      | std::views::transform([](std::shared_ptr<Item> const& item) { return item->descriptor().toJson(); });
  JsonArray materials;
  materials.reserve(inputItems.size());
  std::ranges::copy(descriptors, std::back_inserter(materials));
  auto resultResult = m_scriptComponent.invoke<Json>("craftingRecipe", materials);
  if (!resultResult)
    return std::unexpected(std::move(resultResult.error()));
  auto result = std::move(*resultResult);
  if (!result || result->isNull())
    return ItemRecipe();
  return database->parseRecipe(*result);
}

std::expected<void, StarException> ContainerObject::tickCrafting(float dt) {
  if (!m_crafting.get())
    return {};
  auto inputItems = m_items->items();
  auto operation1 = inputItems.pop_back_checked();
  if (!operation1)
    return std::unexpected(std::move(operation1.error()));
  auto recipeResult = recipeForMaterials(inputItems);
  if (!recipeResult)
    return std::unexpected(std::move(recipeResult.error()));
  auto recipe = std::move(*recipeResult);
  bool craftingFail = recipe.isNull() || m_goalRecipe != recipe;
  auto targetItemResult = std::as_const(*m_items).at(m_items->size() - 1);
  if (!targetItemResult)
    return std::unexpected(std::move(targetItemResult.error()));
  auto targetItem = std::move(*targetItemResult);
  if (targetItem) {
    if (!targetItem->matches(m_goalRecipe.output, true))
      craftingFail = true;
    else if (targetItem->count() + m_goalRecipe.output.count() > targetItem->maxStack())
      craftingFail = true;
  }
  if (craftingFail) {
    m_crafting.set(false);
    m_craftingProgress.set(0);
    m_goalRecipe = ItemRecipe();
    return {};
  }
  if (m_goalRecipe.duration > 0)
    m_craftingProgress.set(m_craftingProgress.get() + dt / m_goalRecipe.duration);
  else
    m_craftingProgress.set(1.0f);
  if (m_craftingProgress.get() >= 1.0f) {
    m_craftingProgress.set(0);
    for (auto const& input : m_goalRecipe.inputs) {
      bool consumed = m_items->consumeItems(input);
      ((void)(consumed));
      if constexpr (Star::DebugEnabled) {
        Star::star_assert_failed((consumed));
      }
    }
    auto databaseResult = root().itemDatabase();
    if (!databaseResult)
      return std::unexpected(std::move(databaseResult.error()));
    auto database = std::move(*databaseResult);
    auto outputResult = database->item(m_goalRecipe.output);
    if (!outputResult)
      return std::unexpected(std::move(outputResult.error()));
    auto output = std::move(*outputResult);
    auto overflowResult = m_items->putItems(m_items->size() - 1, std::move(output));
    if (!overflowResult)
      return std::unexpected(std::move(overflowResult.error()));
    auto overflow = std::move(*overflowResult);
    if (overflow) {
      auto currentWorldResult = world();
      if (!currentWorldResult)
        return std::unexpected(std::move(currentWorldResult.error()));
      auto currentWorld = std::move(*currentWorldResult);
      auto dropResult = ItemDrop::createRandomizedDrop(root(), overflow, position());
      if (!dropResult)
        return std::unexpected(std::move(dropResult.error()));
      auto drop = std::move(*dropResult);
      auto operation2 = currentWorld.get().addEntity(drop);
      if (!operation2)
        return std::unexpected(std::move(operation2.error()));
    }
    itemsUpdated();
  }
  return {};
}

std::expected<std::shared_ptr<Item>, StarException> ContainerObject::doAddItems(std::shared_ptr<Item> const& items) {
  itemsUpdated();
  return m_items->addItems(items);
}

std::expected<std::shared_ptr<Item>, StarException> ContainerObject::doPutItems(std::size_t slot, std::shared_ptr<Item> const& items) {
  itemsUpdated();
  return m_items->putItems(slot, items);
}

std::expected<std::shared_ptr<Item>, StarException> ContainerObject::doTakeItems(std::size_t slot, std::size_t count) {
  itemsUpdated();
  return m_items->takeItems(slot, count);
}

std::expected<std::shared_ptr<Item>, StarException> ContainerObject::doSwapItems(std::size_t slot, std::shared_ptr<Item> const& items, bool tryCombine) {
  itemsUpdated();
  return m_items->swapItems(slot, items, tryCombine);
}

std::expected<std::shared_ptr<Item>, StarException> ContainerObject::doApplyAugment(std::size_t slot, std::shared_ptr<Item> const& item) {
  itemsUpdated();
  if (auto augment = item ? asRef<AugmentItem>(*item) : std::nullopt) {
    auto slotItemResult = m_items->at(slot);
    if (!slotItemResult)
      return std::unexpected(std::move(slotItemResult.error()));
    auto slotItem = std::move(*slotItemResult);
    if (slotItem.get()) {
      auto resultResult = augment->get().applyTo(slotItem.get());
      if (!resultResult)
        return std::unexpected(std::move(resultResult.error()));
      auto result = std::move(*resultResult);
      auto operation1 = m_items->setItem(slot, std::move(result));
      if (!operation1)
        return std::unexpected(std::move(operation1.error()));
    }
  }
  return item;
}

std::expected<bool, StarException> ContainerObject::doConsumeItems(ItemDescriptor const& descriptor) {
  if (m_items->consumeItems(descriptor)) {
    itemsUpdated();
    return true;
  }
  return false;
}

std::expected<bool, StarException> ContainerObject::doConsumeItems(std::size_t slot, std::size_t count) {
  auto consumedResult = m_items->consumeItems(slot, count);
  if (!consumedResult)
    return std::unexpected(std::move(consumedResult.error()));
  auto consumed = std::move(*consumedResult);
  if (consumed) {
    itemsUpdated();
    return true;
  }
  return false;
}

std::expected<List<std::shared_ptr<Item>>, StarException> ContainerObject::doClearContainer() {
  auto operation1 = stopCrafting();
  if (!operation1)
    return std::unexpected(std::move(operation1.error()));
  auto result = m_items->takeAll();
  m_items->clearItems();
  itemsUpdated();
  return result;
}

void ContainerObject::itemsUpdated() {
  m_itemsUpdated = true;
  m_runUpdatedCallback = true;
}

}

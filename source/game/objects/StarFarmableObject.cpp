import star.output_proxy;
import star.json;
import star.xx_hash;
import star.json_extra;
import star.list;
import star.bi_map;
import star.data_stream;
import star.id_map;
import star.math_common;
import star.random;
import star.periodic;
import star.periodic_function;
import star.casting;
import star.strong_typedef;
import star.set;
import star.color;
import star.lua;
import star.vector;
import star.audio;
import star.thread;
import star.map;
import star.string;
import star.image_processing;
import star.hash;
import star.string_view;
import star.directives;
import star.asset_path;
import star.ordered_map;
import star.lexical_cast;
import star.io_device;
import star.ref_ptr;
import star.listener;
import std;
import star.format;
import star.image;
import star.rpc_promise;
import star.worker_pool;

import std;
import star.sector_array_2d;
import star.net_compatibility;
import star.perlin;
import star.btree;
import star.lru_cache;
import star.data_stream_devices;
import star.btree_database;
import star.ordered_set;
import star.game_types;
import star.material_types;
import star.weighted_pool;
import star.parametric_function;
import star.logging;

import star.collision_block;
import star.item_descriptor;
import star.lua_components;
import star.lua_animation_component;
import star.world_geometry;
import star.status_types;
import star.damage;
import star.entity;
import star.tile_damage;
import star.interaction_types;
import star.celestial_coordinate;
import star.quest_descriptor;
import star.interactive_entity;
import star.tile_entity;
import star.status_effect_entity;
import star.scripted_entity;
import star.chat_action;
import star.chatty_entity;
import star.wiring;
import star.wire_entity;
import star.inspectable_entity;
import star.animated_part_set;
import star.drawable;
import star.animation;
import star.particle;
import star.mixer;
import star.light_source;
import star.networked_animator;
import star.damage_types;
import star.entity_rendering_types;
import star.entity_rendering;
import star.object;
import star.asset_source;
import star.assets;
import star.root_base;
import star.configuration;
import star.root;
import star.plant;
import star.lua_root;
import star.tile_sector_array;
import star.tile_modification;
import star.force_regions;
import star.world;
import star.liquid_types;
import star.weather_types;
import star.sky_types;
import star.world_parameters;
import star.celestial_parameters;
import star.world_layout;
import star.collision_generator;
import star.world_tiles;
import star.celestial_types;
import star.chat_types;
import star.uuid;
import star.warping;
import star.biome_placement;
import star.versioning_database;
import star.world_storage;
import star.player_types;
import star.sky_parameters;
import star.system_world;
import star.damage_manager;
import star.net_packets;
import star.cellular_light_array;
import star.cellular_lighting;
import star.cellular_liquid;
import star.world_structure;
import star.sky_render_data;
import star.parallax;
import star.world_render_data;
import star.world_client_state;
import star.interpolation_tracker;
import star.spawn_type_database;
import star.spawner;
import star.world_server;
import star.physics_entity;
import star.movement_controller;
import star.mobile_entity;
import star.game_timers;
import star.item_drop;
import star.treasure;

import star.farmable_object;
import star.plant_database;
import star.object_database;
import star.material_database;


namespace Star {

FarmableObject::FarmableObject(Root& rootContext) : Entity(rootContext), Object(rootContext), m_stage(0), m_stageAlt(-1), m_stageEnterTime(0), m_nextStageTime(0), m_min_immersion(0), m_max_immersion(2), m_consumeSoilMoisture(true), m_finalStage(false) {}

std::expected<std::shared_ptr<FarmableObject>, StarException> FarmableObject::create(Root& rootContext, std::shared_ptr<ObjectConfig const> config, Json const& parameters) {
  auto object = std::shared_ptr<FarmableObject>(new FarmableObject(rootContext));
  auto operation1 = object->initialize(std::move(config), parameters);
  if (!operation1)
    return std::unexpected(std::move(operation1.error()));
  auto stagesConfigResult = object->configValue("stages", JsonArray{JsonObject()});
  if (!stagesConfigResult)
    return std::unexpected(std::move(stagesConfigResult.error()));
  auto stagesConfig = std::move(*stagesConfigResult);
  auto stagesResult = stagesConfig.to_array();
  if (!stagesResult)
    return std::unexpected(std::move(stagesResult.error()));
  auto stages = std::move(*stagesResult);
  object->m_stages = std::move(stages);
  auto startingStageConfigResult = object->configValue("startingStage", 0);
  if (!startingStageConfigResult)
    return std::unexpected(std::move(startingStageConfigResult.error()));
  auto startingStageConfig = std::move(*startingStageConfigResult);
  auto startingStageResult = startingStageConfig.to_int();
  if (!startingStageResult)
    return std::unexpected(std::move(startingStageResult.error()));
  auto startingStage = std::move(*startingStageResult);
  object->m_stage = std::move(startingStage);
  auto min_immersion = object->configValue("minImmersion", 0).and_then(&Json::to_float);
  if (!min_immersion)
    return std::unexpected(std::move(min_immersion.error()));
  auto max_immersion = object->configValue("maxImmersion", 2).and_then(&Json::to_float);
  if (!max_immersion)
    return std::unexpected(std::move(max_immersion.error()));
  if (!is_finite(*min_immersion) || !is_finite(*max_immersion) || *min_immersion > *max_immersion)
    return std::unexpected(ObjectException("Expected finite minImmersion <= maxImmersion"));
  object->m_min_immersion = *min_immersion;
  object->m_max_immersion = *max_immersion;
  auto consumeSoilMoistureConfigResult = object->configValue("consumeSoilMoisture", true);
  if (!consumeSoilMoistureConfigResult)
    return std::unexpected(std::move(consumeSoilMoistureConfigResult.error()));
  auto consumeSoilMoistureConfig = std::move(*consumeSoilMoistureConfigResult);
  auto consumeSoilMoistureResult = consumeSoilMoistureConfig.to_bool();
  if (!consumeSoilMoistureResult)
    return std::unexpected(std::move(consumeSoilMoistureResult.error()));
  auto consumeSoilMoisture = std::move(*consumeSoilMoistureResult);
  object->m_consumeSoilMoisture = std::move(consumeSoilMoisture);
  auto assets = rootContext.assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  auto farming = (*assets)->json("/farming.config");
  if (!farming)
    return std::unexpected(std::move(farming.error()));
  auto window_size = farming->get_float("immersionWindow");
  if (!window_size)
    return std::unexpected(std::move(window_size.error()));
  auto count = farming->get_double("immersionResolution");
  if (!count)
    return std::unexpected(std::move(count.error()));
  if (!is_finite(*count) || *count < 1 || *count >= std::ldexp(1.0, std::numeric_limits<std::size_t>::digits)
      || *count != std::trunc(*count))
    return std::unexpected(ObjectException("Invalid immersionResolution: expected a positive integer sample count"));
  auto immersion = SlidingWindow::create(*window_size, static_cast<std::size_t>(*count), std::midpoint(*min_immersion, *max_immersion));
  if (!immersion)
    return std::unexpected(std::move(immersion.error()));
  object->m_immersion.emplace(std::move(*immersion));
  return object;
}

std::expected<void, StarException> FarmableObject::update(float dt, std::uint64_t currentStep) {
  auto operation2 = Object::update(dt, currentStep);
  if (!operation2)
    return std::unexpected(std::move(operation2.error()));
  if (isMaster()) {
    auto currentWorldResult = world();
    if (!currentWorldResult)
      return std::unexpected(std::move(currentWorldResult.error()));
    auto currentWorld = std::move(*currentWorldResult);
    if (m_nextStageTime == 0) {
      m_nextStageTime = currentWorld.get().epochTime();
      auto operation3 = enterStage(m_stage);
      if (!operation3)
        return std::unexpected(std::move(operation3.error()));
    }
    while (!m_finalStage && currentWorld.get().epochTime() >= m_nextStageTime) {
      int lastStage = m_stage;
      auto operation4 = enterStage(m_stage + 1);
      if (!operation4)
        return std::unexpected(std::move(operation4.error()));
      if (m_stage == lastStage)
        break;
    }
    auto sampled = m_immersion->update(dt, [this] { return liquidFillLevel(); });
    if (!sampled)
      return std::unexpected(std::move(sampled.error()));
    auto average = m_immersion->average();
    if (average > m_max_immersion || average < m_min_immersion) {
      auto operation6 = breakObject(false);
      if (!operation6)
        return std::unexpected(std::move(operation6.error()));
    }
  }
  return {};
}

std::expected<bool, StarException> FarmableObject::damageTiles(List<Vec2I> const& position, Vec2F const& sourcePosition, TileDamage const& tileDamage) {
  if (tileDamage.type != TileDamageType::Beamish && tileDamage.type != TileDamageType::Blockish && tileDamage.type != TileDamageType::Plantish)
    return Object::damageTiles(position, sourcePosition, tileDamage);
  auto harvestedResult = harvest();
  if (!harvestedResult)
    return std::unexpected(std::move(harvestedResult.error()));
  auto harvested = std::move(*harvestedResult);
  if (!harvested)
    return Object::damageTiles(position, sourcePosition, tileDamage);
  return false;
}

std::expected<InteractAction, StarException> FarmableObject::interact(InteractRequest const&) {
  auto operation7 = harvest();
  if (!operation7)
    return std::unexpected(std::move(operation7.error()));
  return InteractAction();
}

std::expected<bool, StarException> FarmableObject::harvest() {
  if (!isMaster())
    return false;
  auto stage = m_stages.get(m_stage);
  auto harvestableResult = stage.contains("harvestPool");
  if (!harvestableResult)
    return std::unexpected(std::move(harvestableResult.error()));
  auto harvestable = std::move(*harvestableResult);
  if (!harvestable)
    return false;
  auto currentWorldResult = world();
  if (!currentWorldResult)
    return std::unexpected(std::move(currentWorldResult.error()));
  auto currentWorld = std::move(*currentWorldResult);
  auto dropped = [&]() -> std::expected<void, StarException> {
    auto treasureResult = root().treasureDatabase();
    if (!treasureResult)
      return std::unexpected(std::move(treasureResult.error()));
    auto treasure = std::move(*treasureResult);
    auto poolResult = stage.get_string("harvestPool");
    if (!poolResult)
      return std::unexpected(std::move(poolResult.error()));
    auto pool = std::move(*poolResult);
    auto levelResult = currentWorld.get().threatLevel();
    if (!levelResult)
      return std::unexpected(std::move(levelResult.error()));
    auto level = std::move(*levelResult);
    auto itemsResult = treasure->createTreasure(pool, level);
    if (!itemsResult)
      return std::unexpected(std::move(itemsResult.error()));
    auto items = std::move(*itemsResult);
    for (auto const& item : items) {
      auto drop_result = ItemDrop::create_randomized_drop(root(), item, position());
      if (!drop_result)
        return std::unexpected(std::move(drop_result.error()));
      auto drop = std::move(*drop_result);
      auto operation8 = currentWorld.get().addEntity(drop);
      if (!operation8)
        return std::unexpected(std::move(operation8.error()));
    }
    return {};
  }();
  if (!dropped) {
    auto objectNameResult = name();
    if (!objectNameResult)
      return std::unexpected(std::move(objectNameResult.error()));
    auto objectName = std::move(*objectNameResult);
    auto operation9 =
      root().logger().warn_exception(Star::format("Failed to create treasure for farmable object '{}'", objectName),
                                     ExceptionDiagnostic(dropped.error()));
    if (!operation9)
      return std::unexpected(std::move(operation9.error()));
  }
  auto resetResult = stage.contains("resetToStage");
  if (!resetResult)
    return std::unexpected(std::move(resetResult.error()));
  auto reset = std::move(*resetResult);
  if (reset) {
    m_nextStageTime = currentWorld.get().epochTime();
    auto resetStageResult = stage.get_int("resetToStage");
    if (!resetStageResult)
      return std::unexpected(std::move(resetStageResult.error()));
    auto resetStage = std::move(*resetStageResult);
    auto operation10 = enterStage(resetStage);
    if (!operation10)
      return std::unexpected(std::move(operation10.error()));
  } else {
    auto operation11 = breakObject(true);
    if (!operation11)
      return std::unexpected(std::move(operation11.error()));
  }
  return true;
}

int FarmableObject::stage() const {
  return m_stage;
}

std::expected<void, StarException> FarmableObject::enterStage(int newStage) {
  if (m_stages.empty())
    return std::unexpected(ObjectException("Farmable object has no stages"));
  newStage = clamp<int>(newStage, 0, m_stages.size() - 1);
  auto currentWorldResult = world();
  if (!currentWorldResult)
    return std::unexpected(std::move(currentWorldResult.error()));
  auto currentWorld = std::move(*currentWorldResult);
  if (m_consumeSoilMoisture && newStage > m_stage) {
    if (auto orientation = currentOrientation()) {
      auto assetsResult = root().assets();
      if (!assetsResult)
        return std::unexpected(std::move(assetsResult.error()));
      auto assets = std::move(*assetsResult);
      auto materialsResult = root().materialDatabase();
      if (!materialsResult)
        return std::unexpected(std::move(materialsResult.error()));
      auto materials = std::move(*materialsResult);
      auto wetToDryModsResult = assets->json("/farming.config:wetToDryMods");
      if (!wetToDryModsResult)
        return std::unexpected(std::move(wetToDryModsResult.error()));
      auto wetToDryMods = std::move(*wetToDryModsResult);
      for (auto const& anchor : orientation->anchors) {
        auto pos = tilePosition() + anchor.position;
        auto modNameResult = materials->modName(currentWorld.get().mod(pos, anchor.layer));
        if (!modNameResult)
          return std::unexpected(std::move(modNameResult.error()));
        auto modName = std::move(*modNameResult);
        auto newModResult = wetToDryMods.opt_string(modName.get());
        if (!newModResult)
          return std::unexpected(std::move(newModResult.error()));
        auto newMod = std::move(*newModResult);
        if (newMod) {
          auto modResult = materials->modId(*newMod);
          if (!modResult)
            return std::unexpected(std::move(modResult.error()));
          auto mod = std::move(*modResult);
          auto operation12 = currentWorld.get().modifyTile(
            pos, PlaceMod{.layer = anchor.layer, .mod = mod, .modHueShift = MaterialHue()}, true);
          if (!operation12)
            return std::unexpected(std::move(operation12.error()));
        } else {
          auto stage = m_stages.get(m_stage);
          auto durationConfigResult = stage.get("duration", JsonArray{0, 0});
          if (!durationConfigResult)
            return std::unexpected(std::move(durationConfigResult.error()));
          auto durationConfig = std::move(*durationConfigResult);
          auto durationResult = json_to_vec2f(durationConfig);
          if (!durationResult)
            return std::unexpected(std::move(durationResult.error()));
          auto duration = std::move(*durationResult);
          auto intervalResult = root().random().uniform_float(duration[0], duration[1]);
          if (!intervalResult)
            return std::unexpected(std::move(intervalResult.error()));
          auto interval = std::move(*intervalResult);
          m_nextStageTime = currentWorld.get().epochTime() + interval;
          return {};
        }
      }
    }
  }
  auto nextStage = m_stages.get(newStage);
  auto treeResult = nextStage.get_bool("tree", false);
  if (!treeResult)
    return std::unexpected(std::move(treeResult.error()));
  auto tree = std::move(*treeResult);
  if (tree) {
    auto stemNameConfigResult = configValue("stemName");
    if (!stemNameConfigResult)
      return std::unexpected(std::move(stemNameConfigResult.error()));
    auto stemNameConfig = std::move(*stemNameConfigResult);
    auto stemNameResult = stemNameConfig.to_string();
    if (!stemNameResult)
      return std::unexpected(std::move(stemNameResult.error()));
    auto stemName = std::move(*stemNameResult);
    auto stemHueShiftConfigResult = configValue("stemHueShift", 0);
    if (!stemHueShiftConfigResult)
      return std::unexpected(std::move(stemHueShiftConfigResult.error()));
    auto stemHueShiftConfig = std::move(*stemHueShiftConfigResult);
    auto stemHueShiftResult = stemHueShiftConfig.to_float();
    if (!stemHueShiftResult)
      return std::unexpected(std::move(stemHueShiftResult.error()));
    auto stemHueShift = std::move(*stemHueShiftResult);
    auto foliageNameConfigResult = configValue("foliageName", "");
    if (!foliageNameConfigResult)
      return std::unexpected(std::move(foliageNameConfigResult.error()));
    auto foliageNameConfig = std::move(*foliageNameConfigResult);
    auto foliageNameResult = foliageNameConfig.to_string();
    if (!foliageNameResult)
      return std::unexpected(std::move(foliageNameResult.error()));
    auto foliageName = std::move(*foliageNameResult);
    auto foliageHueShiftConfigResult = configValue("foliageHueShift", 0);
    if (!foliageHueShiftConfigResult)
      return std::unexpected(std::move(foliageHueShiftConfigResult.error()));
    auto foliageHueShiftConfig = std::move(*foliageHueShiftConfigResult);
    auto foliageHueShiftResult = foliageHueShiftConfig.to_float();
    if (!foliageHueShiftResult)
      return std::unexpected(std::move(foliageHueShiftResult.error()));
    auto foliageHueShift = std::move(*foliageHueShiftResult);
    Vec2I plantPosition = tilePosition();
    auto plantsResult = root().plantDatabase();
    if (!plantsResult)
      return std::unexpected(std::move(plantsResult.error()));
    auto plants = std::move(*plantsResult);
    auto variantResult = foliageName.empty() ? plants->buildTreeVariant(stemName, stemHueShift) : plants->buildTreeVariant(stemName, stemHueShift, foliageName, foliageHueShift);
    if (!variantResult)
      return std::unexpected(std::move(variantResult.error()));
    auto plantResult = plants->createPlant(*variantResult, root().random().next_i64());
    if (!plantResult)
      return std::unexpected(std::move(plantResult.error()));
    auto plant = std::move(*plantResult);
    auto operation13 = plant->setTilePosition(plantPosition);
    if (!operation13)
      return std::unexpected(std::move(operation13.error()));
    auto occupiedResult = anySpacesOccupied(plant->spaces());
    if (!occupiedResult)
      return std::unexpected(std::move(occupiedResult.error()));
    auto occupied = std::move(*occupiedResult);
    bool rooted = false;
    if (!occupied) {
      auto rootsOccupiedResult = allSpacesOccupied(plant->roots());
      if (!rootsOccupiedResult)
        return std::unexpected(std::move(rootsOccupiedResult.error()));
      auto rootsOccupied = std::move(*rootsOccupiedResult);
      rooted = rootsOccupied;
    }
    if (occupied || !rooted) {
      newStage = 0;
    } else {
      currentWorld.get().timer(2.f / 60.f, [plant](World& world) -> std::expected<void, StarException> { return world.addEntity(plant); });
      m_finalStage = true;
      return breakObject(true);
    }
  }
  auto stage = m_stages.get(newStage);
  if (newStage == static_cast<int>(m_stages.size()) - 1) {
    m_finalStage = true;
  } else {
    m_finalStage = false;
    m_stageEnterTime = m_nextStageTime;
    auto durationConfigResult = stage.get("duration", JsonArray{0, 0});
    if (!durationConfigResult)
      return std::unexpected(std::move(durationConfigResult.error()));
    auto durationConfig = std::move(*durationConfigResult);
    auto durationResult = json_to_vec2f(durationConfig);
    if (!durationResult)
      return std::unexpected(std::move(durationResult.error()));
    auto duration = std::move(*durationResult);
    auto intervalResult = root().random().uniform_float(duration[0], duration[1]);
    if (!intervalResult)
      return std::unexpected(std::move(intervalResult.error()));
    auto interval = std::move(*intervalResult);
    m_nextStageTime += interval;
  }
  auto harvestableResult = stage.contains("harvestPool");
  if (!harvestableResult)
    return std::unexpected(std::move(harvestableResult.error()));
  auto harvestable = std::move(*harvestableResult);
  m_interactive.set(harvestable);
  // Keep the variant when adjacent stages have equal alternative counts.
  bool chooseAlt = m_stageAlt == -1;
  if (!chooseAlt) {
    auto nextAltsResult = stage.get_int("alts", 1);
    if (!nextAltsResult)
      return std::unexpected(std::move(nextAltsResult.error()));
    auto nextAlts = std::move(*nextAltsResult);
    auto previousStage = m_stages.get(m_stage);
    auto previousAltsResult = previousStage.get_int("alts", 1);
    if (!previousAltsResult)
      return std::unexpected(std::move(previousAltsResult.error()));
    auto previousAlts = std::move(*previousAltsResult);
    chooseAlt = nextAlts != previousAlts;
  }
  if (chooseAlt) {
    auto altsResult = stage.get_int("alts", 1);
    if (!altsResult)
      return std::unexpected(std::move(altsResult.error()));
    auto alts = std::move(*altsResult);
    auto alt = root().random().uniform_int(alts - 1);
    if (!alt)
      return std::unexpected(std::move(alt.error()));
    m_stageAlt = *alt;
  }
  m_stage = newStage;
  setImageKey("stage", Star::to_string(m_stage));
  setImageKey("alt", Star::to_string(m_stageAlt));
  return {};
}

std::expected<void, StarException> FarmableObject::readStoredData(Json const& diskStore) {
  auto operation14 = Object::readStoredData(diskStore);
  if (!operation14)
    return std::unexpected(std::move(operation14.error()));
  auto stageResult = diskStore.get_int("stage");
  if (!stageResult)
    return std::unexpected(std::move(stageResult.error()));
  auto stage = std::move(*stageResult);
  m_stage = stage;
  auto stageAltResult = diskStore.get_int("stageAlt");
  if (!stageAltResult)
    return std::unexpected(std::move(stageAltResult.error()));
  auto stageAlt = std::move(*stageAltResult);
  m_stageAlt = stageAlt;
  auto stageEnterTimeResult = diskStore.get_double("stageEnterTime");
  if (!stageEnterTimeResult)
    return std::unexpected(std::move(stageEnterTimeResult.error()));
  auto stageEnterTime = std::move(*stageEnterTimeResult);
  m_stageEnterTime = stageEnterTime;
  auto nextStageTimeResult = diskStore.get_double("nextStageTime");
  if (!nextStageTimeResult)
    return std::unexpected(std::move(nextStageTimeResult.error()));
  auto nextStageTime = std::move(*nextStageTimeResult);
  m_nextStageTime = nextStageTime;
  m_finalStage = (m_stage == static_cast<int>(m_stages.size()) - 1);
  setImageKey("stage", Star::to_string(m_stage));
  setImageKey("alt", Star::to_string(m_stageAlt));
  return {};
}

std::expected<Json, StarException> FarmableObject::writeStoredData() const {
  auto storedResult = Object::writeStoredData();
  if (!storedResult)
    return std::unexpected(std::move(storedResult.error()));
  auto stored = std::move(*storedResult);
  return stored.set_all({{"stage", m_stage},
                         {"stageAlt", m_stageAlt},
                         {"stageEnterTime", m_stageEnterTime},
                         {"nextStageTime", m_nextStageTime}});
}

}

#include <assert.h>
import star.output_proxy;
import star.memory;
import star.json_extra;
import star.json;
import star.id_map;
import star.poly;
import star.list;
import star.bi_map;
import star.data_stream_extra;
import star.casting;
import star.strong_typedef;
import star.rpc_promise;
import star.lexical_cast;
import star.logging;
import star.game_types;
import star.random;
import star.weighted_pool;
import star.interpolation;
import star.parametric_function;
import star.data_stream;
import star.audio;
import star.thread;
import star.map;
import star.set;
import star.vector;
import star.color;
import star.string;
import star.rect;
import star.image_processing;
import star.hash;
import star.string_view;
import star.directives;
import star.asset_path;
import star.math_common;
import star.periodic;
import star.periodic_function;
import star.lua;
import star.ordered_map;
import star.io_device;
import star.ref_ptr;
import star.listener;
import std;
import star.format;
import star.version;

import star.collision_block;
import star.lua_components;
import star.lua_animation_component;
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
import star.tile_entity;
import star.tile_modification;
import star.lua_root;
import star.force_regions;
import star.world;
import star.asset_source;
import star.assets;
import star.root_base;
import star.configuration;
import star.root;
import star.physics_entity;
import star.movement_controller;
import star.mobile_entity;
import star.game_timers;
import star.item_drop;
import star.item_descriptor;
import star.mixer;
import star.drawable;
import star.entity_rendering_types;
import star.animation;
import star.particle;

import star.light_source;
import star.entity_rendering;
import star.lua_game_converters;
import star.treasure;

import star.networked_animator_lua_bindings;
import star.entity_lua_bindings;
import star.scripted_animator_lua_bindings;
import star.config_lua_bindings;
import star.particle_database;
import star.object_database;
import star.material_database;



import star.data_stream_devices;

import star.net_compatibility;
import star.byte_array;
import star.algorithm;
namespace Star {

Object::Object(Root& rootContext) : Entity(rootContext), m_scriptComponent(rootContext), m_scriptedAnimator(rootContext), m_animationTimer(0), m_currentFrame(-1), m_broken(false), m_unbreakable(false), m_orientationIndex(NPos), m_animationCenterLine(0), m_clientEntityMode(ClientEntityMode::ClientSlaveOnly) {}

std::expected<std::shared_ptr<Object>, StarException> Object::create(Root& rootContext, std::shared_ptr<ObjectConfig const> config, Json const& parameters) {
  auto object = std::shared_ptr<Object>(new Object(rootContext));
  auto operation1 = object->initialize(std::move(config), parameters);
  if (!operation1)
    return std::unexpected(std::move(operation1.error()));
  return object;
}

std::expected<void, StarException> Object::initialize(std::shared_ptr<ObjectConfig const> config, Json const& parameters) {
  m_config = std::move(config);
  if (!parameters.isNull()) {
    auto objectParametersResult = parameters.toObject();
    if (!objectParametersResult)
      return std::unexpected(std::move(objectParametersResult.error()));
    auto objectParameters = std::move(*objectParametersResult);
    m_parameters.reset(std::move(objectParameters));
  }
  auto jOrientations = m_parameters.maybeRef("customOrientations");
  if (jOrientations && jOrientations->get().isType(Json::Type::Array)) {
    auto baseResult = m_config->config.getArray("orientations");
    if (!baseResult)
      return std::unexpected(std::move(baseResult.error()));
    auto base = std::move(*baseResult);
    auto orientationsResult = jOrientations->get().toArray();
    if (!orientationsResult)
      return std::unexpected(std::move(orientationsResult.error()));
    auto orientations = std::move(*orientationsResult);
    for (std::size_t i = 0; i != orientations.size(); ++i) {
      auto mergedOrientation = jsonMergeNulling(base.get(i), orientations.get(i));
      base.set(i, std::move(mergedOrientation));
    }
    auto customOrientationsResult = ObjectDatabase::parseOrientations(root(), m_config->path, base, m_config->config);
    if (!customOrientationsResult)
      return std::unexpected(std::move(customOrientationsResult.error()));
    auto customOrientations = std::move(*customOrientationsResult);
    m_orientations = std::move(customOrientations);
  }
  m_lightFlickering = m_config->lightFlickering;
  auto tileDamageStatus = EntityTileDamageStatus::create();
  if (!tileDamageStatus)
    return std::unexpected(std::move(tileDamageStatus.error()));
  m_tileDamageStatus = std::move(*tileDamageStatus);
  auto actionResult = configValue("interactAction", Json());
  if (!actionResult)
    return std::unexpected(std::move(actionResult.error()));
  auto action = std::move(*actionResult);
  m_interactive.set(!action.isNull());
  m_unbreakable = m_config->unbreakable;
  if (!m_unbreakable) {
    auto unbreakableConfigResult = configValue("unbreakable", false);
    if (!unbreakableConfigResult)
      return std::unexpected(std::move(unbreakableConfigResult.error()));
    auto unbreakableConfig = std::move(*unbreakableConfigResult);
    auto unbreakableResult = unbreakableConfig.toBool();
    if (!unbreakableResult)
      return std::unexpected(std::move(unbreakableResult.error()));
    auto unbreakable = std::move(*unbreakableResult);
    m_unbreakable = unbreakable;
  }
  m_direction.set(Direction::Left);
  m_health.set(m_config->health);
  if (m_config->animationConfig) {
    auto animatorResult = NetworkedAnimator::create(root(), m_config->animationConfig, m_config->path);
    if (!animatorResult)
      return std::unexpected(std::move(animatorResult.error()));
    auto animator = std::move(*animatorResult);
    m_networkedAnimator = std::move(animator);
  } else {
    auto animator = NetworkedAnimator::create(root());
    if (!animator)
      return std::unexpected(std::move(animator.error()));
    m_networkedAnimator = std::move(*animator);
  }
  if (m_config->damageTeam.type != TeamType::Null)
    setTeam(m_config->damageTeam);
  else
    setTeam(EntityDamageTeam(TeamType::Environment));
  auto inputNodesResult = configValue("inputNodes", JsonArray());
  if (!inputNodesResult)
    return std::unexpected(std::move(inputNodesResult.error()));
  auto inputNodes = std::move(*inputNodesResult);
  auto inputNodeConfigsResult = configValue("inputNodesConfig", JsonArray());
  if (!inputNodeConfigsResult)
    return std::unexpected(std::move(inputNodeConfigsResult.error()));
  auto inputNodeConfigs = std::move(*inputNodeConfigsResult);
  auto inputCount = inputNodes.size();
  if (!inputCount)
    return std::unexpected(std::move(inputCount.error()));
  for (std::size_t i = 0; i != *inputCount; ++i) {
    auto positionResult = inputNodes.get(i);
    if (!positionResult)
      return std::unexpected(std::move(positionResult.error()));
    auto position = std::move(*positionResult);
    auto nodeConfigResult = inputNodeConfigs.get(i, JsonObject());
    if (!nodeConfigResult)
      return std::unexpected(std::move(nodeConfigResult.error()));
    auto nodeConfig = std::move(*nodeConfigResult);
    auto nodeResult = InputNode::create(position, nodeConfig);
    if (!nodeResult)
      return std::unexpected(std::move(nodeResult.error()));
    auto node = std::move(*nodeResult);
    m_inputNodes.append(std::move(node));
  }
  auto outputNodesResult = configValue("outputNodes", JsonArray());
  if (!outputNodesResult)
    return std::unexpected(std::move(outputNodesResult.error()));
  auto outputNodes = std::move(*outputNodesResult);
  auto outputNodeConfigsResult = configValue("outputNodesConfig", JsonArray());
  if (!outputNodeConfigsResult)
    return std::unexpected(std::move(outputNodeConfigsResult.error()));
  auto outputNodeConfigs = std::move(*outputNodeConfigsResult);
  auto outputCount = outputNodes.size();
  if (!outputCount)
    return std::unexpected(std::move(outputCount.error()));
  for (std::size_t i = 0; i != *outputCount; ++i) {
    auto positionResult = outputNodes.get(i);
    if (!positionResult)
      return std::unexpected(std::move(positionResult.error()));
    auto position = std::move(*positionResult);
    auto nodeConfigResult = outputNodeConfigs.get(i, JsonObject());
    if (!nodeConfigResult)
      return std::unexpected(std::move(nodeConfigResult.error()));
    auto nodeConfig = std::move(*nodeConfigResult);
    auto nodeResult = OutputNode::create(position, nodeConfig);
    if (!nodeResult)
      return std::unexpected(std::move(nodeResult.error()));
    auto node = std::move(*nodeResult);
    m_outputNodes.append(std::move(node));
  }
  auto offeredConfigResult = configValue("offeredQuests", JsonArray());
  if (!offeredConfigResult)
    return std::unexpected(std::move(offeredConfigResult.error()));
  auto offeredConfig = std::move(*offeredConfigResult);
  auto offeredResult = offeredConfig.toArray();
  if (!offeredResult)
    return std::unexpected(std::move(offeredResult.error()));
  auto offered = std::move(*offeredResult);
  List<QuestArcDescriptor> quests;
  for (auto const& entry : offered) {
    auto questResult = QuestArcDescriptor::fromJson(root(), entry);
    if (!questResult)
      return std::unexpected(std::move(questResult.error()));
    auto quest = std::move(*questResult);
    quests.append(std::move(quest));
  }
  m_offeredQuests.set(std::move(quests));
  auto turnInConfigResult = configValue("turnInQuests", JsonArray());
  if (!turnInConfigResult)
    return std::unexpected(std::move(turnInConfigResult.error()));
  auto turnInConfig = std::move(*turnInConfigResult);
  auto turnInResult = jsonToStringSet(turnInConfig);
  if (!turnInResult)
    return std::unexpected(std::move(turnInResult.error()));
  auto turnIn = std::move(*turnInResult);
  m_turnInQuests.set(std::move(turnIn));
  if (!m_offeredQuests.get().empty() || !m_turnInQuests.get().empty())
    m_interactive.set(true);
  auto colorConfigResult = configValue("color", "default");
  if (!colorConfigResult)
    return std::unexpected(std::move(colorConfigResult.error()));
  auto colorConfig = std::move(*colorConfigResult);
  auto colorResult = colorConfig.toString();
  if (!colorResult)
    return std::unexpected(std::move(colorResult.error()));
  auto color = std::move(*colorResult);
  m_imageKeys.set("color", color.takeUtf8());
  auto keysConfigResult = configValue("imageKeys", JsonObject());
  if (!keysConfigResult)
    return std::unexpected(std::move(keysConfigResult.error()));
  auto keysConfig = std::move(*keysConfigResult);
  auto keysResult = keysConfig.toObject();
  if (!keysResult)
    return std::unexpected(std::move(keysResult.error()));
  auto keys = std::move(*keysResult);
  for (auto const& pair : keys) {
    auto valueResult = pair.second.toString();
    if (!valueResult)
      return std::unexpected(std::move(valueResult.error()));
    auto value = std::move(*valueResult);
    m_imageKeys.set(pair.first, value);
  }
  auto uniqueIdConfigResult = configValue("uniqueId");
  if (!uniqueIdConfigResult)
    return std::unexpected(std::move(uniqueIdConfigResult.error()));
  auto uniqueIdConfig = std::move(*uniqueIdConfigResult);
  auto uniqueIdResult = uniqueIdConfig.optString();
  if (!uniqueIdResult)
    return std::unexpected(std::move(uniqueIdResult.error()));
  auto uniqueId = std::move(*uniqueIdResult);
  setUniqueId(std::move(uniqueId));
  auto operation2 = m_netGroup.addNetElement(m_parameters);
  if (!operation2)
    return std::unexpected(std::move(operation2.error()));
  auto operation3 = m_netGroup.addNetElement(m_uniqueIdNetState);
  if (!operation3)
    return std::unexpected(std::move(operation3.error()));
  auto operation4 = m_netGroup.addNetElement(m_interactive);
  if (!operation4)
    return std::unexpected(std::move(operation4.error()));
  auto operation5 = m_netGroup.addNetElement(m_materialSpaces);
  if (!operation5)
    return std::unexpected(std::move(operation5.error()));
  auto operation6 = m_netGroup.addNetElement(m_xTilePosition);
  if (!operation6)
    return std::unexpected(std::move(operation6.error()));
  auto operation7 = m_netGroup.addNetElement(m_yTilePosition);
  if (!operation7)
    return std::unexpected(std::move(operation7.error()));
  auto operation8 = m_netGroup.addNetElement(m_direction);
  if (!operation8)
    return std::unexpected(std::move(operation8.error()));
  auto operation9 = m_netGroup.addNetElement(m_health);
  if (!operation9)
    return std::unexpected(std::move(operation9.error()));
  auto operation10 = m_netGroup.addNetElement(m_orientationIndexNetState);
  if (!operation10)
    return std::unexpected(std::move(operation10.error()));
  auto operation11 = m_netGroup.addNetElement(m_netImageKeys);
  if (!operation11)
    return std::unexpected(std::move(operation11.error()));
  auto operation12 = m_netGroup.addNetElement(m_soundEffectEnabled);
  if (!operation12)
    return std::unexpected(std::move(operation12.error()));
  auto operation13 = m_netGroup.addNetElement(m_lightSourceColor);
  if (!operation13)
    return std::unexpected(std::move(operation13.error()));
  auto operation14 = m_netGroup.addNetElement(m_newChatMessageEvent);
  if (!operation14)
    return std::unexpected(std::move(operation14.error()));
  auto operation15 = m_netGroup.addNetElement(m_chatMessage);
  if (!operation15)
    return std::unexpected(std::move(operation15.error()));
  auto operation16 = m_netGroup.addNetElement(m_chatPortrait);
  if (!operation16)
    return std::unexpected(std::move(operation16.error()));
  auto operation17 = m_netGroup.addNetElement(m_chatConfig);
  if (!operation17)
    return std::unexpected(std::move(operation17.error()));
  for (auto& node : m_inputNodes) {
    auto operation18 = m_netGroup.addNetElement(node.connections);
    if (!operation18)
      return std::unexpected(std::move(operation18.error()));
    auto operation19 = m_netGroup.addNetElement(node.state);
    if (!operation19)
      return std::unexpected(std::move(operation19.error()));
  }
  for (auto& node : m_outputNodes) {
    auto operation20 = m_netGroup.addNetElement(node.connections);
    if (!operation20)
      return std::unexpected(std::move(operation20.error()));
    auto operation21 = m_netGroup.addNetElement(node.state);
    if (!operation21)
      return std::unexpected(std::move(operation21.error()));
  }
  auto operation22 = m_netGroup.addNetElement(m_offeredQuests);
  if (!operation22)
    return std::unexpected(std::move(operation22.error()));
  auto operation23 = m_netGroup.addNetElement(m_turnInQuests);
  if (!operation23)
    return std::unexpected(std::move(operation23.error()));
  auto operation24 = m_netGroup.addNetElement(m_damageSources);
  if (!operation24)
    return std::unexpected(std::move(operation24.error()));
  // Scripted animation parameters deliberately do not interpolate.
  auto operation25 = m_netGroup.addNetElement(m_scriptedAnimationParameters, false);
  if (!operation25)
    return std::unexpected(std::move(operation25.error()));
  auto operation26 = m_netGroup.addNetElement(*m_tileDamageStatus);
  if (!operation26)
    return std::unexpected(std::move(operation26.error()));
  auto operation27 = m_netGroup.addNetElement(*m_networkedAnimator);
  if (!operation27)
    return std::unexpected(std::move(operation27.error()));
  m_netGroup.setNeedsLoadCallback(std::bind(&Object::getNetStates, std::ref(*this), std::placeholders::_1));
  m_netGroup.setNeedsStoreCallback(std::bind(&Object::setNetStates, std::ref(*this)));
  auto modeConfigResult = configValue("clientEntityMode", "ClientSlaveOnly");
  if (!modeConfigResult)
    return std::unexpected(std::move(modeConfigResult.error()));
  auto modeConfig = std::move(*modeConfigResult);
  auto modeNameResult = modeConfig.toString();
  if (!modeNameResult)
    return std::unexpected(std::move(modeNameResult.error()));
  auto modeName = std::move(*modeNameResult);
  if (!ClientEntityModeNames)
    return std::unexpected(ClientEntityModeNames.error());
  auto modeResult = ClientEntityModeNames->getLeft(modeName);
  if (!modeResult)
    return std::unexpected(std::move(modeResult.error()));
  auto mode = std::move(*modeResult);
  m_clientEntityMode = mode.get();
  return {};
}

std::expected<Json, StarException> Object::diskStore() const {
  auto storedResult = writeStoredData();
  if (!storedResult)
    return std::unexpected(std::move(storedResult.error()));
  auto stored = std::move(*storedResult);
  return stored.setAll({{"name", m_config->name}, {"parameters", m_parameters.baseMap()}});
}

std::expected<ByteArray, StarException> Object::netStore(NetCompatibilityRules rules) {
  DataStreamBuffer ds;
  ds.setStreamCompatibilityVersion(rules);
  auto operation28 = ds.write(m_config->name);
  if (!operation28)
    return std::unexpected(std::move(operation28.error()));
  auto operation29 = ds.write<Json>(m_parameters.baseMap());
  if (!operation29)
    return std::unexpected(std::move(operation29.error()));
  return ds.takeData();
}

EntityType Object::entityType() const {
  return EntityType::Object;
}

ClientEntityMode Object::clientEntityMode() const {
  return m_clientEntityMode;
}

std::expected<void, StarException> Object::init(World& world, EntityId entityId, EntityMode mode) {
  auto operation1 = Entity::init(world, entityId, mode);
  if (!operation1)
    return std::unexpected(std::move(operation1.error()));
  auto initialized = [&]() -> std::expected<void, StarException> {
    if (m_orientationIndex == NPos) {
      auto operation2 = updateOrientation();
      if (!operation2)
        return std::unexpected(std::move(operation2.error()));
    } else if (auto orientation = currentOrientation()) {
      if (orientation->directionAffinity)
        m_direction.set(*orientation->directionAffinity);
      m_materialSpaces.set(orientation->materialSpaces);
    }
    m_orientationDrawablesCache.reset();
    // Preserve the legacy color/directive split, including its byte offsets.
    auto colorConfigResult = configValue("color", "default");
    if (!colorConfigResult)
      return std::unexpected(std::move(colorConfigResult.error()));
    auto colorConfig = std::move(*colorConfigResult);
    auto colorResult = colorConfig.toString();
    if (!colorResult)
      return std::unexpected(std::move(colorResult.error()));
    auto color = std::move(*colorResult);
    auto colorName = color.takeUtf8();
    auto colorEnd = colorName.find('?');
    if (colorEnd != NPos) {
      std::size_t suffixBegin = colorName.rfind('?');
      String colorDirectives;
      std::string colorSuffix = suffixBegin == NPos ? "" : colorName.substr(suffixBegin);
      if (colorSuffix.empty() && colorSuffix.rfind("?replace", 0) != 0)
        colorDirectives = colorName.substr(colorEnd);
      else
        colorDirectives = colorName.substr(colorEnd, suffixBegin - colorEnd);
      m_colorSuffix = std::move(colorSuffix);
      m_colorDirectives = std::move(colorDirectives);
    } else {
      m_colorDirectives = "";
      m_colorSuffix = "";
    }
    m_directives = "";
    auto directivesResult = configValue("");
    if (!directivesResult)
      return std::unexpected(std::move(directivesResult.error()));
    auto directives = std::move(*directivesResult);
    if (directives.isType(Json::Type::String)) {
      auto directiveStringResult = directives.toString();
      if (!directiveStringResult)
        return std::unexpected(std::move(directiveStringResult.error()));
      auto directiveString = std::move(*directiveStringResult);
      m_directives.parse(std::move(directiveString));
    }
    if (isMaster()) {
      setImageKey("color", colorName);
      auto keysConfigResult = configValue("imageKeys", JsonObject());
      if (!keysConfigResult)
        return std::unexpected(std::move(keysConfigResult.error()));
      auto keysConfig = std::move(*keysConfigResult);
      auto keysResult = keysConfig.toObject();
      if (!keysResult)
        return std::unexpected(std::move(keysResult.error()));
      auto keys = std::move(*keysResult);
      for (auto const& pair : keys) {
        auto valueResult = pair.second.toString();
        if (!valueResult)
          return std::unexpected(std::move(valueResult.error()));
        auto value = std::move(*valueResult);
        setImageKey(pair.first, value);
      }
      if (auto light = m_config->lightColors.maybeRef(colorName)) {
        m_lightSourceColor.set(light->get());
      } else {
        m_lightSourceColor.set(Color::Clear);
      }
      m_soundEffectEnabled.set(true);
      m_liquidCheckTimer = GameTimer(m_config->liquidCheckInterval);
      m_liquidCheckTimer.setDone();
      auto keepAliveConfigResult = configValue("keepAlive", false);
      if (!keepAliveConfigResult)
        return std::unexpected(std::move(keepAliveConfigResult.error()));
      auto keepAliveConfig = std::move(*keepAliveConfigResult);
      auto keepAliveResult = keepAliveConfig.toBool();
      if (!keepAliveResult)
        return std::unexpected(std::move(keepAliveResult.error()));
      auto keepAlive = std::move(*keepAliveResult);
      setKeepAlive(keepAlive);
      auto scriptsConfigResult = configValue("scripts", JsonArray());
      if (!scriptsConfigResult)
        return std::unexpected(std::move(scriptsConfigResult.error()));
      auto scriptsConfig = std::move(*scriptsConfigResult);
      if (scriptsConfig.isType(Json::Type::Array)) {
        auto scriptsResult = jsonToStringList(scriptsConfig);
        if (!scriptsResult)
          return std::unexpected(std::move(scriptsResult.error()));
        auto scripts = std::move(*scriptsResult);
        std::ranges::for_each(scripts, [&](String& script) { script = AssetPath::relativeTo(m_config->path, script); });
        auto operation3 = m_scriptComponent.setScripts(std::move(scripts));
        if (!operation3)
          return std::unexpected(std::move(operation3.error()));
      } else {
        auto operation4 = m_scriptComponent.setScripts(m_config->scripts);
        if (!operation4)
          return std::unexpected(std::move(operation4.error()));
      }
      auto deltaConfigResult = configValue("scriptDelta", 5);
      if (!deltaConfigResult)
        return std::unexpected(std::move(deltaConfigResult.error()));
      auto deltaConfig = std::move(*deltaConfigResult);
      auto deltaResult = deltaConfig.toInt();
      if (!deltaResult)
        return std::unexpected(std::move(deltaResult.error()));
      auto delta = std::move(*deltaResult);
      m_scriptComponent.setUpdateDelta(delta);
      auto objectCallbacksResult = makeObjectCallbacks();
      if (!objectCallbacksResult)
        return std::unexpected(std::move(objectCallbacksResult.error()));
      auto objectCallbacks = std::move(*objectCallbacksResult);
      auto operation5 = m_scriptComponent.addCallbacks("object", std::move(objectCallbacks));
      if (!operation5)
        return std::unexpected(std::move(operation5.error()));
      auto configCallbacksResult = LuaBindings::makeConfigCallbacks(std::bind(&Object::configValue, std::ref(*this), std::placeholders::_1, std::placeholders::_2));
      if (!configCallbacksResult)
        return std::unexpected(std::move(configCallbacksResult.error()));
      auto configCallbacks = std::move(*configCallbacksResult);
      auto operation6 = m_scriptComponent.addCallbacks("config", std::move(configCallbacks));
      if (!operation6)
        return std::unexpected(std::move(operation6.error()));
      auto entityCallbacksResult = LuaBindings::makeEntityCallbacks(*this);
      if (!entityCallbacksResult)
        return std::unexpected(std::move(entityCallbacksResult.error()));
      auto entityCallbacks = std::move(*entityCallbacksResult);
      auto operation7 = m_scriptComponent.addCallbacks("entity", std::move(entityCallbacks));
      if (!operation7)
        return std::unexpected(std::move(operation7.error()));
      auto animatorCallbacksResult = LuaBindings::makeNetworkedAnimatorCallbacks(*m_networkedAnimator);
      if (!animatorCallbacksResult)
        return std::unexpected(std::move(animatorCallbacksResult.error()));
      auto animatorCallbacks = std::move(*animatorCallbacksResult);
      auto operation8 = m_scriptComponent.addCallbacks("animator", std::move(animatorCallbacks));
      if (!operation8)
        return std::unexpected(std::move(operation8.error()));
      auto operation9 = m_scriptComponent.init(world);
      if (!operation9)
        return std::unexpected(std::move(operation9.error()));
    }
    if (world.isClient()) {
      auto operation10 = m_scriptedAnimator.setScripts(m_config->animationScripts);
      if (!operation10)
        return std::unexpected(std::move(operation10.error()));
      auto animationCallbacksResult = LuaBindings::makeScriptedAnimatorCallbacks(*m_networkedAnimator, [this](String const& name, Json const& def) -> std::expected<Json, StarException> { return m_scriptedAnimationParameters.value(name, def); });
      if (!animationCallbacksResult)
        return std::unexpected(std::move(animationCallbacksResult.error()));
      auto animationCallbacks = std::move(*animationCallbacksResult);
      auto operation11 = m_scriptedAnimator.addCallbacks("animationConfig", std::move(animationCallbacks));
      if (!operation11)
        return std::unexpected(std::move(operation11.error()));
      auto objectAnimatorCallbacksResult = makeAnimatorObjectCallbacks();
      if (!objectAnimatorCallbacksResult)
        return std::unexpected(std::move(objectAnimatorCallbacksResult.error()));
      auto objectAnimatorCallbacks = std::move(*objectAnimatorCallbacksResult);
      auto operation12 = m_scriptedAnimator.addCallbacks("objectAnimator", std::move(objectAnimatorCallbacks));
      if (!operation12)
        return std::unexpected(std::move(operation12.error()));
      auto configCallbacksResult = LuaBindings::makeConfigCallbacks(std::bind(&Object::configValue, std::ref(*this), std::placeholders::_1, std::placeholders::_2));
      if (!configCallbacksResult)
        return std::unexpected(std::move(configCallbacksResult.error()));
      auto configCallbacks = std::move(*configCallbacksResult);
      auto operation13 = m_scriptedAnimator.addCallbacks("config", std::move(configCallbacks));
      if (!operation13)
        return std::unexpected(std::move(operation13.error()));
      auto entityCallbacksResult = LuaBindings::makeEntityCallbacks(*this);
      if (!entityCallbacksResult)
        return std::unexpected(std::move(entityCallbacksResult.error()));
      auto entityCallbacks = std::move(*entityCallbacksResult);
      auto operation14 = m_scriptedAnimator.addCallbacks("entity", std::move(entityCallbacks));
      if (!operation14)
        return std::unexpected(std::move(operation14.error()));
      auto operation15 = m_scriptedAnimator.init(world);
      if (!operation15)
        return std::unexpected(std::move(operation15.error()));
    }
    m_xTilePosition.set(world.geometry().xwrap((int)m_xTilePosition.get()));
    auto partsConfigResult = configValue("animationParts", JsonObject());
    if (!partsConfigResult)
      return std::unexpected(std::move(partsConfigResult.error()));
    auto partsConfig = std::move(*partsConfigResult);
    auto partsResult = partsConfig.toObject();
    if (!partsResult)
      return std::unexpected(std::move(partsResult.error()));
    auto parts = std::move(*partsResult);
    for (auto const& pair : parts) {
      auto imageResult = pair.second.toString();
      if (!imageResult)
        return std::unexpected(std::move(imageResult.error()));
      auto image = std::move(*imageResult);
      m_networkedAnimator->setPartTag(pair.first, "partImage", image);
    }
    auto animationPositionConfigResult = configValue("animationPosition", JsonArray{0, 0});
    if (!animationPositionConfigResult)
      return std::unexpected(std::move(animationPositionConfigResult.error()));
    auto animationPositionConfig = std::move(*animationPositionConfigResult);
    auto animationPositionResult = jsonToVec2F(animationPositionConfig);
    if (!animationPositionResult)
      return std::unexpected(std::move(animationPositionResult.error()));
    auto animationPosition = std::move(*animationPositionResult);
    m_animationPosition = animationPosition / TilePixels;
    m_networkedAnimator->setFlipped(false);
    auto imageMetadataResult = root().imageMetadataDatabase();
    if (!imageMetadataResult)
      return std::unexpected(std::move(imageMetadataResult.error()));
    auto imageMetadata = std::move(*imageMetadataResult);
    auto drawablesResult = m_networkedAnimator->drawables();
    if (!drawablesResult)
      return std::unexpected(std::move(drawablesResult.error()));
    auto drawables = std::move(*drawablesResult);
    auto boundsResult = Drawable::boundBoxAll(*imageMetadata, drawables, false);
    if (!boundsResult)
      return std::unexpected(std::move(boundsResult.error()));
    auto bounds = std::move(*boundsResult);
    auto centerConfigResult = configValue("animationCenterLine", bounds.center()[0]);
    if (!centerConfigResult)
      return std::unexpected(std::move(centerConfigResult.error()));
    auto centerConfig = std::move(*centerConfigResult);
    auto centerResult = centerConfig.toFloat();
    if (!centerResult)
      return std::unexpected(std::move(centerResult.error()));
    auto center = std::move(*centerResult);
    m_animationCenterLine = center;
    m_networkedAnimator->setFlipped(direction() == Direction::Left, m_animationCenterLine);
    // Do not animate initial state transitions unless explicitly requested.
    auto finishResult = configValue("forceFinishAnimationsInInit", true);
    if (!finishResult)
      return std::unexpected(std::move(finishResult.error()));
    auto finish = std::move(*finishResult);
    if (finish != false) {
      auto operation16 = m_networkedAnimator->finishAnimations();
      if (!operation16)
        return std::unexpected(std::move(operation16.error()));
    }
    return {};
  }();
  if (!initialized) {
    auto cleaned = uninit();
    if (!cleaned)
      return std::unexpected(initialized.error().withContext(strf("Object initialization cleanup failed: {}", outputException(cleaned.error(), false))));
    return std::unexpected(std::move(initialized.error()));
  }
  return {};
}

std::expected<void, StarException> Object::uninit() {
  std::optional<StarException> error;
  auto remember = [&](auto result) { if (!result && !error) error = std::move(result.error()); };
  if (isMaster()) {
    remember(m_scriptComponent.uninit());
    remember(m_scriptComponent.removeCallbacks("object"));
    remember(m_scriptComponent.removeCallbacks("config"));
    remember(m_scriptComponent.removeCallbacks("entity"));
    remember(m_scriptComponent.removeCallbacks("animator"));
  }
  if (auto attachedWorld = maybeWorld(); attachedWorld && attachedWorld->get().isClient()) {
    remember(m_scriptedAnimator.uninit());
    remember(m_scriptedAnimator.removeCallbacks("animationConfig"));
    remember(m_scriptedAnimator.removeCallbacks("objectAnimator"));
    remember(m_scriptedAnimator.removeCallbacks("config"));
    remember(m_scriptedAnimator.removeCallbacks("entity"));
  }
  if (m_soundEffect)
    m_soundEffect->stop();
  remember(Entity::uninit());
  if (error)
    return std::unexpected(std::move(*error));
  return {};
}

std::expected<List<LightSource>, StarException> Object::lightSources() const {
  return lightSources(nullptr);
}

std::expected<List<LightSource>, StarException> Object::lightSources(NetworkedAnimator::Sample const* sample) const {
  auto lightsResult = sample ? sample->lightSources(position() + m_animationPosition)
                            : m_networkedAnimator->lightSources(position() + m_animationPosition);
  if (!lightsResult)
    return std::unexpected(std::move(lightsResult.error()));
  auto lights = std::move(*lightsResult);
  auto orientation = currentOrientation();
  if (!m_lightSourceColor.get().isClear() && orientation) {
    Color color = m_lightSourceColor.get();
    if (m_lightFlickering)
      color.setValue(std::clamp(color.value() * m_lightFlickering->value(SinWeightOperator<float>()), 0.0f, 1.0f));

    LightSource lightSource;
    lightSource.position = position() + centerOfTile(orientation->lightPosition);
    lightSource.color = color.toRgbF();
    lightSource.type = m_config->lightType;
    lightSource.pointBeam = m_config->pointBeam;
    lightSource.beamAngle = orientation->beamAngle;
    lightSource.beamAmbience = m_config->beamAmbience;

    lights.append(lightSource);
  }

  return lights;
}

Vec2F Object::position() const {
  return Vec2F(m_xTilePosition.get(), m_yTilePosition.get());
}

RectF Object::metaBoundBox() const {
  if (auto orientation = currentOrientation()) {
    // default metaboundbox extends the bounding box of the orientation's
    // spaces by one block
    return orientation->metaBoundBox.value_or(RectF(Vec2F(orientation->boundBox.min()) - Vec2F(1, 1), Vec2F(orientation->boundBox.max()) + Vec2F(2, 2)));
  } else {
    return RectF(-1, -1, 1, 1);
  }
}

std::expected<std::pair<ByteArray, std::uint64_t>, StarException> Object::writeNetState(std::uint64_t fromVersion, NetCompatibilityRules rules) {
  return m_netGroup.writeNetState(fromVersion, rules);
}

std::expected<void, StarException> Object::readNetState(ByteArray data, float interpolationTime, NetCompatibilityRules rules) {
  return m_netGroup.readNetState(std::move(data), interpolationTime, rules);
}

Vec2I Object::tilePosition() const {
  return Vec2I(m_xTilePosition.get(), m_yTilePosition.get());
}

std::expected<void, StarException> Object::setTilePosition(Vec2I const& pos) {
  if (m_xTilePosition.get() != pos[0] || m_yTilePosition.get() != pos[1]) {
    m_xTilePosition.set(pos[0]);
    m_yTilePosition.set(pos[1]);
    if (inWorld())
      return updateOrientation();
  }
  return {};
}

Direction Object::direction() const {
  return m_direction.get();
}

void Object::setDirection(Direction direction) {
  m_direction.set(direction);
}

std::expected<void, StarException> Object::updateOrientation() {
  auto currentWorldResult = world();
  if (!currentWorldResult)
    return std::unexpected(std::move(currentWorldResult.error()));
  auto& currentWorld = currentWorldResult->get();
  auto indexResult = m_config->findValidOrientation(currentWorld, tilePosition(), m_direction.get());
  if (!indexResult)
    return std::unexpected(std::move(indexResult.error()));
  auto index = std::move(*indexResult);
  auto operation1 = setOrientationIndex(index);
  if (!operation1)
    return std::unexpected(std::move(operation1.error()));
  if (auto orientation = currentOrientation()) {
    if (orientation->directionAffinity)
      m_direction.set(*orientation->directionAffinity);
    m_materialSpaces.set(orientation->materialSpaces);
  }
  resetEmissionTimers();
  return {};
}

List<Vec2I> Object::anchorPositions() const {
  if (auto orientation = currentOrientation()) {
    List<Vec2I> positions;
    for (auto anchor : orientation->anchors)
      positions.append(anchor.position + tilePosition());
    return positions;
  } else {
    return {};
  }
}

List<Vec2I> Object::spaces() const {
  if (auto orientation = currentOrientation())
    return orientation->spaces;
  else
    return {};
}

List<MaterialSpace> Object::materialSpaces() const {
  return m_materialSpaces.get();
}

List<Vec2I> Object::roots() const {
  if (m_config->rooting) {
    if (auto orientation = currentOrientation()) {
      List<Vec2I> res;
      for (auto anchor : orientation->anchors)
        res.append(anchor.position);
      return res;
    }
  }
  return {};
}

std::expected<void, StarException> Object::update(float dt, std::uint64_t) {
  if (!inWorld())
    return {};
  auto currentWorldResult = world();
  if (!currentWorldResult)
    return std::unexpected(std::move(currentWorldResult.error()));
  auto& currentWorld = currentWorldResult->get();
  if (isMaster()) {
    m_tileDamageStatus->recover(m_config->tileDamageParameters, dt);
    if (m_liquidCheckTimer.wrapTick(root().timing().timestep)) {
      auto operation1 = checkLiquidBroken();
      if (!operation1)
        return std::unexpected(std::move(operation1.error()));
    }
    if (auto orientation = currentOrientation()) {
      auto frame = clamp<int>(std::floor(m_animationTimer / orientation->animationCycle * orientation->frames), 0, orientation->frames - 1);
      if (m_currentFrame != frame) {
        m_currentFrame = frame;
        setImageKey("frame", toString(frame));
      }
      m_animationTimer = std::fmod(m_animationTimer + dt, orientation->animationCycle);
    }
    auto operation2 = m_networkedAnimator->update(dt, std::nullopt);
    if (!operation2)
      return std::unexpected(std::move(operation2.error()));
    m_networkedAnimator->setFlipped(direction() == Direction::Left, m_animationCenterLine);
    auto operation3 = m_scriptComponent.update(m_scriptComponent.updateDt(dt));
    if (!operation3)
      return std::unexpected(std::move(operation3.error()));
  } else {
    auto operation4 = m_networkedAnimator->update(dt, std::ref(m_networkedAnimatorDynamicTarget));
    if (!operation4)
      return std::unexpected(std::move(operation4.error()));
    m_networkedAnimatorDynamicTarget.updatePosition(position() + m_animationPosition);
  }
  if (m_lightFlickering)
    m_lightFlickering->update(dt);
  for (auto& timer : m_emissionTimers)
    timer.tick(dt);
  if (currentWorld.isClient()) {
    auto operation5 = m_scriptedAnimator.update();
    if (!operation5)
      return std::unexpected(std::move(operation5.error()));
  }
  return {};
}

std::expected<void, StarException> Object::collectRenderEvents(RenderCallback& renderCallback) {
  for (auto const& pair : m_imageKeys)
    m_networkedAnimator->setGlobalTag(pair.first, pair.second);
  auto shake = computeDamageShake();
  if (!shake)
    return std::unexpected(std::move(shake.error()));
  m_presentationDamageShake = *shake;
  auto operation1 = collectParticles(renderCallback);
  if (!operation1)
    return std::unexpected(std::move(operation1.error()));
  auto operation2 = collectSounds(renderCallback);
  if (!operation2)
    return std::unexpected(std::move(operation2.error()));
  auto operation3 = renderCallback.addAudios(m_networkedAnimatorDynamicTarget.pullNewAudios());
  if (!operation3)
    return std::unexpected(std::move(operation3.error()));
  auto operation4 = renderCallback.addParticles(m_networkedAnimatorDynamicTarget.pullNewParticles());
  if (!operation4)
    return std::unexpected(std::move(operation4.error()));
  auto operation8 = renderCallback.addParticles(m_scriptedAnimator.pullNewParticles());
  if (!operation8)
    return std::unexpected(std::move(operation8.error()));
  auto operation9 = renderCallback.addAudios(m_scriptedAnimator.pullNewAudios());
  if (!operation9)
    return std::unexpected(std::move(operation9.error()));
  return {};
}

std::expected<void, StarException> Object::render(RenderCallback& renderCallback) {
  auto sample = m_networkedAnimator->sample(renderCallback.presentation().interpolation());
  if (!m_networkedAnimator->constParts().empty()) {
    auto shakeResult = damageShake();
    if (!shakeResult)
      return std::unexpected(std::move(shakeResult.error()));
    auto shake = std::move(*shakeResult);
    auto drawablesResult = sample.drawables(position() + m_animationPosition + shake);
    if (!drawablesResult)
      return std::unexpected(std::move(drawablesResult.error()));
    auto drawables = std::move(*drawablesResult);
    auto operation5 = renderCallback.addDrawables(drawables, renderLayer());
    if (!operation5)
      return std::unexpected(std::move(operation5.error()));
  } else if (m_orientationIndex != NPos) {
    auto drawablesResult = orientationDrawables(m_orientationIndex);
    if (!drawablesResult)
      return std::unexpected(std::move(drawablesResult.error()));
    auto drawables = std::move(*drawablesResult);
    auto operation6 = renderCallback.addDrawables(drawables, renderLayer(), position());
    if (!operation6)
      return std::unexpected(std::move(operation6.error()));
  }
  auto const& drawables = m_scriptedAnimator.drawables();
  for (auto const& pair : drawables) {
    auto operation7 = renderCallback.addDrawable(pair.first, pair.second.value_or(renderLayer()));
    if (!operation7)
      return std::unexpected(std::move(operation7.error()));
  }
  return {};
}

std::expected<void, StarException> Object::renderLightSources(RenderCallback& renderCallback) {
  auto sample = m_networkedAnimator->sample(renderCallback.presentation().interpolation());
  auto lightsResult = lightSources(&sample);
  if (!lightsResult)
    return std::unexpected(std::move(lightsResult.error()));
  if (auto added = renderCallback.addLightSources(std::move(*lightsResult)); !added)
    return std::unexpected(std::move(added.error()));
  auto const& lights = m_scriptedAnimator.lightSources();
  return renderCallback.addLightSources(lights);
}

std::expected<bool, StarException> Object::damageTiles(List<Vec2I> const&, Vec2F const&, TileDamage const& tileDamage) {
  if (m_unbreakable)
    return false;
  auto operation1 = m_tileDamageStatus->damage(m_config->tileDamageParameters, tileDamage);
  if (!operation1)
    return std::unexpected(std::move(operation1.error()));
  if (m_tileDamageStatus->dead())
    m_broken = true;
  return m_broken;
}

std::expected<bool, StarException> Object::canBeDamaged() const {
  return !m_unbreakable;
}

std::expected<bool, StarException> Object::checkBroken() {
  if (!m_broken && !m_unbreakable) {
    if (auto orientation = currentOrientation()) {
      auto currentWorldResult = world();
      if (!currentWorldResult)
        return std::unexpected(std::move(currentWorldResult.error()));
      auto& currentWorld = currentWorldResult->get();
      auto validResult = orientation->anchorsValid(currentWorld, tilePosition());
      if (!validResult)
        return std::unexpected(std::move(validResult.error()));
      auto valid = std::move(*validResult);
      if (!valid)
        m_broken = true;
    } else {
      m_broken = true;
    }
  }
  return m_broken;
}

std::expected<bool, StarException> Object::shouldDestroy() const {
  return m_broken || (m_health.get() <= 0);
}

std::expected<void, StarException> Object::destroy(std::optional<std::reference_wrapper<RenderCallback>> renderCallback) {
  bool doSmash = m_health.get() <= 0;
  if (!doSmash) {
    auto smashConfigResult = configValue("smashOnBreak", m_config->smashOnBreak);
    if (!smashConfigResult)
      return std::unexpected(std::move(smashConfigResult.error()));
    auto smashConfig = std::move(*smashConfigResult);
    auto smashResult = smashConfig.toBool();
    if (!smashResult)
      return std::unexpected(std::move(smashResult.error()));
    auto smash = std::move(*smashResult);
    doSmash = smash;
  }
  if (isMaster()) {
    auto operation1 = m_scriptComponent.invoke("die", m_health.get() <= 0);
    if (!operation1)
      return std::unexpected(std::move(operation1.error()));
    auto dropped = [&]() -> std::expected<void, StarException> {
      auto currentWorldResult = world();
      if (!currentWorldResult)
        return std::unexpected(std::move(currentWorldResult.error()));
      auto& currentWorld = currentWorldResult->get();
      auto dropItem = [&](auto const& item) -> std::expected<void, StarException> {
        auto dropResult = ItemDrop::createRandomizedDrop(root(), item, position());
        if (!dropResult)
          return std::unexpected(std::move(dropResult.error()));
        auto drop = std::move(*dropResult);
        auto operation2 = currentWorld.addEntity(std::move(drop));
        if (!operation2)
          return std::unexpected(std::move(operation2.error()));
        return {};
      };
      if (doSmash) {
        auto poolConfigResult = configValue("smashDropPool", "");
        if (!poolConfigResult)
          return std::unexpected(std::move(poolConfigResult.error()));
        auto poolConfig = std::move(*poolConfigResult);
        auto poolResult = poolConfig.toString();
        if (!poolResult)
          return std::unexpected(std::move(poolResult.error()));
        auto pool = std::move(*poolResult);
        if (!pool.empty()) {
          auto treasureResult = root().treasureDatabase();
          if (!treasureResult)
            return std::unexpected(std::move(treasureResult.error()));
          auto treasure = std::move(*treasureResult);
          auto levelResult = currentWorld.threatLevel();
          if (!levelResult)
            return std::unexpected(std::move(levelResult.error()));
          auto level = std::move(*levelResult);
          auto itemsResult = treasure->createTreasure(pool, level);
          if (!itemsResult)
            return std::unexpected(std::move(itemsResult.error()));
          auto items = std::move(*itemsResult);
          for (auto const& item : items) {
            auto operation3 = dropItem(item);
            if (!operation3)
              return std::unexpected(std::move(operation3.error()));
          }
        } else if (!m_config->smashDropOptions.empty()) {
          auto optionsResult = root().random().randFrom(m_config->smashDropOptions);
          if (!optionsResult)
            return std::unexpected(std::move(optionsResult.error()));
          auto const& options = optionsResult->get();
          for (auto const& item : options) {
            auto operation4 = dropItem(item);
            if (!operation4)
              return std::unexpected(std::move(operation4.error()));
          }
        }
      } else {
        auto poolConfigResult = configValue("breakDropPool", "");
        if (!poolConfigResult)
          return std::unexpected(std::move(poolConfigResult.error()));
        auto poolConfig = std::move(*poolConfigResult);
        auto poolResult = poolConfig.toString();
        if (!poolResult)
          return std::unexpected(std::move(poolResult.error()));
        auto pool = std::move(*poolResult);
        if (!pool.empty()) {
          auto treasureResult = root().treasureDatabase();
          if (!treasureResult)
            return std::unexpected(std::move(treasureResult.error()));
          auto treasure = std::move(*treasureResult);
          auto levelResult = currentWorld.threatLevel();
          if (!levelResult)
            return std::unexpected(std::move(levelResult.error()));
          auto level = std::move(*levelResult);
          auto itemsResult = treasure->createTreasure(pool, level);
          if (!itemsResult)
            return std::unexpected(std::move(itemsResult.error()));
          auto items = std::move(*itemsResult);
          for (auto const& item : items) {
            auto operation5 = dropItem(item);
            if (!operation5)
              return std::unexpected(std::move(operation5.error()));
          }
        } else if (!m_config->breakDropOptions.empty()) {
          auto optionsResult = root().random().randFrom(m_config->breakDropOptions);
          if (!optionsResult)
            return std::unexpected(std::move(optionsResult.error()));
          auto const& options = optionsResult->get();
          for (auto const& item : options) {
            auto operation6 = dropItem(item);
            if (!operation6)
              return std::unexpected(std::move(operation6.error()));
          }
        } else if (m_config->hasObjectItem) {
          auto itemResult = ItemDescriptor::create(m_config->name, 1);
          if (!itemResult)
            return std::unexpected(std::move(itemResult.error()));
          auto item = std::move(*itemResult);
          auto retainConfigResult = configValue("retainObjectParametersInItem", m_config->retainObjectParametersInItem);
          if (!retainConfigResult)
            return std::unexpected(std::move(retainConfigResult.error()));
          auto retainConfig = std::move(*retainConfigResult);
          auto retainResult = retainConfig.optBool();
          if (!retainResult)
            return std::unexpected(std::move(retainResult.error()));
          auto retain = std::move(*retainResult);
          if (retain.value_or(false)) {
            auto parameters = m_parameters.baseMap();
            parameters.remove("owner");
            auto storageResult = m_scriptComponent.getScriptStorage();
            if (!storageResult)
              return std::unexpected(std::move(storageResult.error()));
            auto storage = std::move(*storageResult);
            parameters["scriptStorage"] = std::move(storage);
            auto retained = item.applyParameters(parameters);
            if (!retained)
              return std::unexpected(std::move(retained.error()));
            item = std::move(*retained);
          }
          auto operation7 = dropItem(item);
          if (!operation7)
            return std::unexpected(std::move(operation7.error()));
        }
      }
      return {};
    }();
    if (!dropped) {
      auto operation8 = root().logger().warnException(strf("Failed to create treasure for object '{}'", m_config->name), ExceptionDiagnostic(dropped.error()));
      if (!operation8)
        return std::unexpected(std::move(operation8.error()));
    }
  }
  if (renderCallback && doSmash && !m_config->smashSoundOptions.empty()) {
    auto assetsResult = root().assets();
    if (!assetsResult)
      return std::unexpected(std::move(assetsResult.error()));
    auto assets = std::move(*assetsResult);
    auto soundResult = root().random().randFrom(m_config->smashSoundOptions);
    if (!soundResult)
      return std::unexpected(std::move(soundResult.error()));
    auto const& sound = soundResult->get();
    auto audioResult = assets->audio(sound);
    if (!audioResult)
      return std::unexpected(std::move(audioResult.error()));
    auto audio = std::move(*audioResult);
    auto instanceResult = AudioInstance::create(*audio);
    if (!instanceResult)
      return std::unexpected(std::move(instanceResult.error()));
    auto instance = std::move(*instanceResult);
    auto operation9 = renderCallback->get().addAudios({std::move(instance)}, position());
    if (!operation9)
      return std::unexpected(std::move(operation9.error()));
  }
  if (renderCallback && doSmash && !m_config->smashParticles.empty()) {
    auto particlesDatabaseResult = root().particleDatabase();
    if (!particlesDatabaseResult)
      return std::unexpected(std::move(particlesDatabaseResult.error()));
    auto particlesDatabase = std::move(*particlesDatabaseResult);
    List<Particle> particles;
    for (auto const& config : m_config->smashParticles) {
      auto particleConfigResult = config.get("particle");
      if (!particleConfigResult)
        return std::unexpected(std::move(particleConfigResult.error()));
      auto particleConfig = std::move(*particleConfigResult);
      auto creatorResult = particlesDatabase->particleCreator(particleConfig, m_config->path);
      if (!creatorResult)
        return std::unexpected(std::move(creatorResult.error()));
      auto creator = std::move(*creatorResult);
      auto countResult = config.getUInt("count", 1);
      if (!countResult)
        return std::unexpected(std::move(countResult.error()));
      auto count = std::move(*countResult);
      auto offsetConfigResult = config.get("offset", JsonArray{0, 0});
      if (!offsetConfigResult)
        return std::unexpected(std::move(offsetConfigResult.error()));
      auto offsetConfig = std::move(*offsetConfigResult);
      auto offsetResult = jsonToVec2F(offsetConfig);
      if (!offsetResult)
        return std::unexpected(std::move(offsetResult.error()));
      auto offset = std::move(*offsetResult);
      auto flipResult = config.getBool("flip", false);
      if (!flipResult)
        return std::unexpected(std::move(flipResult.error()));
      auto flip = std::move(*flipResult);
      for (unsigned i = 0; i < count; ++i) {
        Particle particle = creator();
        particle.position += offset;
        if (flip)
          particle.flip = !particle.flip;
        if (m_direction.get() == Direction::Left) {
          particle.position[0] *= -1;
          particle.velocity[0] *= -1;
          particle.flip = !particle.flip;
        }
        particle.translate(position() + volume().center());
        particles.append(std::move(particle));
      }
    }
    auto operation10 = renderCallback->get().addParticles(particles);
    if (!operation10)
      return std::unexpected(std::move(operation10.error()));
  }
  if (m_soundEffect)
    m_soundEffect->stop(1.0f);
  return {};
}

std::expected<String, StarException> Object::name() const {
  return m_config->name;
}

std::expected<String, StarException> Object::shortDescription() const {
  auto valueResult = configValue("shortdescription", m_config->name);
  if (!valueResult)
    return std::unexpected(std::move(valueResult.error()));
  auto value = std::move(*valueResult);
  return value.toString();
}

std::expected<String, StarException> Object::description() const {
  auto fallbackResult = shortDescription();
  if (!fallbackResult)
    return std::unexpected(std::move(fallbackResult.error()));
  auto fallback = std::move(*fallbackResult);
  auto valueResult = configValue("description", fallback);
  if (!valueResult)
    return std::unexpected(std::move(valueResult.error()));
  auto value = std::move(*valueResult);
  return value.toString();
}

bool Object::inspectable() const {
  return m_config->scannable;
}

std::expected<std::optional<String>, StarException> Object::inspectionLogName() const {
  auto valueResult = configValue("inspectionLogName");
  if (!valueResult)
    return std::unexpected(std::move(valueResult.error()));
  auto value = std::move(*valueResult);
  auto nameResult = value.optString();
  if (!nameResult)
    return std::unexpected(std::move(nameResult.error()));
  auto name = std::move(*nameResult);
  return name.value_or(m_config->name);
}

std::expected<std::optional<String>, StarException> Object::inspectionDescription(String const& species) const {
  auto generalConfigResult = configValue("inspectionDescription");
  if (!generalConfigResult)
    return std::unexpected(std::move(generalConfigResult.error()));
  auto generalConfig = std::move(*generalConfigResult);
  auto generalResult = generalConfig.optString();
  if (!generalResult)
    return std::unexpected(std::move(generalResult.error()));
  auto general = std::move(*generalResult);
  auto specificConfigResult = configValue(strf("{}Description", species));
  if (!specificConfigResult)
    return std::unexpected(std::move(specificConfigResult.error()));
  auto specificConfig = std::move(*specificConfigResult);
  auto specificResult = specificConfig.optString();
  if (!specificResult)
    return std::unexpected(std::move(specificResult.error()));
  auto specific = std::move(*specificResult);
  auto fallbackResult = description();
  if (!fallbackResult)
    return std::unexpected(std::move(fallbackResult.error()));
  auto fallback = std::move(*fallbackResult);
  return (general ? general : specific).value_or(fallback);
}

String Object::category() const {
  return m_config->category;
}

std::shared_ptr<ObjectOrientation> Object::currentOrientation() const {
  if (m_orientationIndex != NPos)
    return getOrientations()[m_orientationIndex];
  else
    return {};
}

std::expected<List<Drawable>, StarException> Object::cursorHintDrawables() const {
  auto placementConfigResult = configValue("placementImage");
  if (!placementConfigResult)
    return std::unexpected(std::move(placementConfigResult.error()));
  auto placementConfig = std::move(*placementConfigResult);
  if (placementConfig) {
    auto placementImageResult = placementConfig.toString();
    if (!placementImageResult)
      return std::unexpected(std::move(placementImageResult.error()));
    auto placementImage = std::move(*placementImageResult);
    if (m_direction.get() == Direction::Left)
      placementImage += "?flipx";
    auto metadataResult = root().imageMetadataDatabase();
    if (!metadataResult)
      return std::unexpected(std::move(metadataResult.error()));
    auto metadata = std::move(*metadataResult);
    auto positionConfigResult = configValue("placementImagePosition", jsonFromVec2F(Vec2F()));
    if (!positionConfigResult)
      return std::unexpected(std::move(positionConfigResult.error()));
    auto positionConfig = std::move(*positionConfigResult);
    auto imagePositionResult = jsonToVec2F(positionConfig);
    if (!imagePositionResult)
      return std::unexpected(std::move(imagePositionResult.error()));
    auto imagePosition = std::move(*imagePositionResult);
    auto drawableResult = Drawable::makeImage(*metadata, AssetPath::relativeTo(m_config->path, placementImage), 1.0 / TilePixels, false, imagePosition / TilePixels);
    if (!drawableResult)
      return std::unexpected(std::move(drawableResult.error()));
    auto drawable = std::move(*drawableResult);
    return List<Drawable>{std::move(drawable)};
  }
  if (m_orientationIndex != NPos)
    return orientationDrawables(m_orientationIndex);
  List<Drawable> result;
  auto const& orientations = getOrientations();
  for (std::size_t i = 0; i < orientations.size(); ++i) {
    if (orientations[i]->directionAffinity && *orientations[i]->directionAffinity == m_direction.get()) {
      auto drawablesResult = orientationDrawables(i);
      if (!drawablesResult)
        return std::unexpected(std::move(drawablesResult.error()));
      auto drawables = std::move(*drawablesResult);
      result = std::move(drawables);
      break;
    }
  }
  if (result.empty())
    return orientationDrawables(0);
  return result;
}

std::expected<void, StarException> Object::getNetStates(bool initial) {
  setUniqueId(m_uniqueIdNetState.get());
  if (m_orientationIndex != m_orientationIndexNetState.get()) {
    auto operation1 = setOrientationIndex(m_orientationIndexNetState.get());
    if (!operation1)
      return std::unexpected(std::move(operation1.error()));
  }
  if (m_newChatMessageEvent.pullOccurred() && !initial) {
    auto mouthResult = mouthPosition();
    if (!mouthResult)
      return std::unexpected(std::move(mouthResult.error()));
    auto mouth = std::move(*mouthResult);
    if (m_chatPortrait.get().empty())
      m_pendingChatActions.append(SayChatAction{entityId(), m_chatMessage.get(), mouth});
    else
      m_pendingChatActions.append(PortraitChatAction{entityId(), m_chatPortrait.get(), m_chatMessage.get(), mouth, m_chatConfig.get()});
  }
  if (m_netImageKeys.pullUpdated()) {
    m_imageKeys.merge(m_netImageKeys.baseMap(), true);
    m_orientationDrawablesCache.reset();
  }
  return {};
}

std::expected<void, StarException> Object::setNetStates() {
  m_uniqueIdNetState.set(uniqueId());
  m_orientationIndexNetState.set(m_orientationIndex);
  return {};
}

std::expected<List<QuestArcDescriptor>, StarException> Object::offeredQuests() const {
  return m_offeredQuests.get();
}

std::expected<StringSet, StarException> Object::turnInQuests() const {
  return m_turnInQuests.get();
}

std::expected<Vec2F, StarException> Object::questIndicatorPosition() const {
  if (auto orientation = currentOrientation()) {
    auto pos = position() + Vec2F(orientation->boundBox.center()[0], orientation->boundBox.max()[1] + 2.5);
    if (!(orientation->boundBox.size()[0] % 2))
      pos[0] += 0.5;
    auto configPositionResult = configValue("questIndicatorPosition", Json());
    if (!configPositionResult)
      return std::unexpected(std::move(configPositionResult.error()));
    auto configPosition = std::move(*configPositionResult);
    if (configPosition) {
      auto offsetResult = jsonToVec2F(configPosition);
      if (!offsetResult)
        return std::unexpected(std::move(offsetResult.error()));
      auto offset = std::move(*offsetResult);
      if (m_direction.get() == Direction::Left)
        offset[0] = -offset[0];
      pos += offset;
    }
    return pos;
  }
  return position();
}

std::expected<std::optional<ChainableJsonMessageResponse>, StarException> Object::receiveMessage(ConnectionId sendingConnection, String const& message, JsonArray const& args) {
  auto currentWorldResult = world();
  if (!currentWorldResult)
    return std::unexpected(std::move(currentWorldResult.error()));
  auto& currentWorld = currentWorldResult->get();
  return m_scriptComponent.handleMessage(message, sendingConnection == currentWorld.connection(), args);
}

std::expected<Json, StarException> Object::configValue(String const& name, Json const& def) const {
  if (auto orientation = currentOrientation())
    return jsonMergeQueryDef(name, def, m_config->config, orientation->config, m_parameters.baseMap());
  else
    return jsonMergeQueryDef(name, def, m_config->config, m_parameters.baseMap());
}

std::shared_ptr<ObjectConfig const> Object::config() const {
  return m_config;
}

std::expected<void, StarException> Object::readStoredData(Json const& diskStore) {
  auto idResult = diskStore.optString("uniqueId");
  if (!idResult)
    return std::unexpected(std::move(idResult.error()));
  auto id = std::move(*idResult);
  setUniqueId(std::move(id));
  auto positionConfigResult = diskStore.get("tilePosition");
  if (!positionConfigResult)
    return std::unexpected(std::move(positionConfigResult.error()));
  auto positionConfig = std::move(*positionConfigResult);
  auto positionResult = jsonToVec2I(positionConfig);
  if (!positionResult)
    return std::unexpected(std::move(positionResult.error()));
  auto position = std::move(*positionResult);
  auto operation1 = setTilePosition(position);
  if (!operation1)
    return std::unexpected(std::move(operation1.error()));
  auto orientationConfigResult = diskStore.get("orientationIndex");
  if (!orientationConfigResult)
    return std::unexpected(std::move(orientationConfigResult.error()));
  auto orientationConfig = std::move(*orientationConfigResult);
  auto indexResult = jsonToSize(orientationConfig);
  if (!indexResult)
    return std::unexpected(std::move(indexResult.error()));
  auto index = std::move(*indexResult);
  auto operation2 = setOrientationIndex(index);
  if (!operation2)
    return std::unexpected(std::move(operation2.error()));
  auto directionResult = diskStore.getString("direction");
  if (!directionResult)
    return std::unexpected(std::move(directionResult.error()));
  auto direction = std::move(*directionResult);
  auto directionValueResult = enumValue<DirectionNames>(direction);
  if (!directionValueResult)
    return std::unexpected(std::move(directionValueResult.error()));
  auto directionValue = std::move(*directionValueResult);
  m_direction.set(directionValue);
  auto actionResult = configValue("interactAction", Json());
  if (!actionResult)
    return std::unexpected(std::move(actionResult.error()));
  auto action = std::move(*actionResult);
  auto interactiveResult = diskStore.getBool("interactive", !action.isNull());
  if (!interactiveResult)
    return std::unexpected(std::move(interactiveResult.error()));
  auto interactive = std::move(*interactiveResult);
  m_interactive.set(interactive);
  auto storageResult = diskStore.getObject("scriptStorage", JsonObject());
  if (!storageResult)
    return std::unexpected(std::move(storageResult.error()));
  auto storage = std::move(*storageResult);
  auto operation3 = m_scriptComponent.setScriptStorage(std::move(storage));
  if (!operation3)
    return std::unexpected(std::move(operation3.error()));
  auto inputNodesResult = diskStore.getArray("inputWireNodes");
  if (!inputNodesResult)
    return std::unexpected(std::move(inputNodesResult.error()));
  auto inputNodes = std::move(*inputNodesResult);
  for (std::size_t i = 0; i < m_inputNodes.size() && i < inputNodes.size(); ++i) {
    auto& node = m_inputNodes[i];
    List<WireConnection> connections;
    auto storedConnectionsResult = inputNodes[i].getArray("connections");
    if (!storedConnectionsResult)
      return std::unexpected(std::move(storedConnectionsResult.error()));
    auto storedConnections = std::move(*storedConnectionsResult);
    for (auto const& connection : storedConnections) {
      auto locationConfigResult = connection.get(0);
      if (!locationConfigResult)
        return std::unexpected(std::move(locationConfigResult.error()));
      auto locationConfig = std::move(*locationConfigResult);
      auto locationResult = jsonToVec2I(locationConfig);
      if (!locationResult)
        return std::unexpected(std::move(locationResult.error()));
      auto location = std::move(*locationResult);
      auto nodeIndexResult = connection.getUInt(1);
      if (!nodeIndexResult)
        return std::unexpected(std::move(nodeIndexResult.error()));
      auto nodeIndex = std::move(*nodeIndexResult);
      connections.append(WireConnection{location, (std::size_t)nodeIndex});
    }
    node.connections.set(std::move(connections));
    auto stateResult = inputNodes[i].getBool("state");
    if (!stateResult)
      return std::unexpected(std::move(stateResult.error()));
    auto state = std::move(*stateResult);
    node.state.set(state);
  }
  auto outputNodesResult = diskStore.getArray("outputWireNodes");
  if (!outputNodesResult)
    return std::unexpected(std::move(outputNodesResult.error()));
  auto outputNodes = std::move(*outputNodesResult);
  for (std::size_t i = 0; i < m_outputNodes.size() && i < outputNodes.size(); ++i) {
    auto& node = m_outputNodes[i];
    List<WireConnection> connections;
    auto storedConnectionsResult = outputNodes[i].getArray("connections");
    if (!storedConnectionsResult)
      return std::unexpected(std::move(storedConnectionsResult.error()));
    auto storedConnections = std::move(*storedConnectionsResult);
    for (auto const& connection : storedConnections) {
      auto locationConfigResult = connection.get(0);
      if (!locationConfigResult)
        return std::unexpected(std::move(locationConfigResult.error()));
      auto locationConfig = std::move(*locationConfigResult);
      auto locationResult = jsonToVec2I(locationConfig);
      if (!locationResult)
        return std::unexpected(std::move(locationResult.error()));
      auto location = std::move(*locationResult);
      auto nodeIndexResult = connection.getUInt(1);
      if (!nodeIndexResult)
        return std::unexpected(std::move(nodeIndexResult.error()));
      auto nodeIndex = std::move(*nodeIndexResult);
      connections.append(WireConnection{location, (std::size_t)nodeIndex});
    }
    node.connections.set(std::move(connections));
    auto stateResult = outputNodes[i].getBool("state");
    if (!stateResult)
      return std::unexpected(std::move(stateResult.error()));
    auto state = std::move(*stateResult);
    node.state.set(state);
  }
  return {};
}

std::expected<Json, StarException> Object::writeStoredData() const {
  JsonArray inputNodes;
  for (std::size_t i = 0; i < m_inputNodes.size(); ++i) {
    auto const& in = m_inputNodes[i];
    JsonArray connections;
    for (auto const& node : in.connections.get())
      connections.append(JsonArray{jsonFromVec2I(node.entityLocation), node.nodeIndex});

    inputNodes.append(JsonObject{
        {"connections", std::move(connections)},
        {"state", in.state.get()}
      });
  }

  JsonArray outputNodes;
  for (std::size_t i = 0; i < m_outputNodes.size(); ++i) {
    auto const& in = m_outputNodes[i];
    JsonArray connections;
    for (auto const& node : in.connections.get())
      connections.append(JsonArray{jsonFromVec2I(node.entityLocation), node.nodeIndex});

    outputNodes.append(JsonObject{
        {"connections", std::move(connections)},
        {"state", in.state.get()}
      });
  }

  auto direction = enumName<DirectionNames>(m_direction.get());
  if (!direction)
    return std::unexpected(std::move(direction.error()));
  auto storage = m_scriptComponent.getScriptStorage();
  if (!storage)
    return std::unexpected(std::move(storage.error()));
  return JsonObject{{"uniqueId", jsonFromOptional(uniqueId())},
                    {"tilePosition", jsonFromVec2I(tilePosition())},
                    {"orientationIndex", jsonFromSize(m_orientationIndex)},
                    {"direction", String(*direction)},
                    {"scriptStorage", std::move(*storage)},
                    {"interactive", m_interactive.get()},
                    {"inputWireNodes", std::move(inputNodes)},
                    {"outputWireNodes", std::move(outputNodes)}};
}

std::expected<void, StarException> Object::breakObject(bool smash) {
  m_broken = true;
  if (smash)
    m_health.set(0.0f);
  return {};
}

std::size_t Object::nodeCount(WireDirection direction) const {
  if (direction == WireDirection::Input)
    return m_inputNodes.size();
  else
    return m_outputNodes.size();
}

std::expected<Vec2I, StarException> Object::nodePosition(WireNode wireNode) const {
  if (wireNode.direction == WireDirection::Input) {
    auto nodeResult = m_inputNodes.at(wireNode.nodeIndex);
    if (!nodeResult)
      return std::unexpected(std::move(nodeResult.error()));
    auto const& node = nodeResult->get();
    return node.position;
  }
  auto nodeResult = m_outputNodes.at(wireNode.nodeIndex);
  if (!nodeResult)
    return std::unexpected(std::move(nodeResult.error()));
  auto const& node = nodeResult->get();
  return node.position;
}

std::expected<List<WireConnection>, StarException> Object::connectionsForNode(WireNode wireNode) const {
  if (wireNode.direction == WireDirection::Input) {
    auto nodeResult = m_inputNodes.at(wireNode.nodeIndex);
    if (!nodeResult)
      return std::unexpected(std::move(nodeResult.error()));
    auto const& node = nodeResult->get();
    return node.connections.get();
  }
  auto nodeResult = m_outputNodes.at(wireNode.nodeIndex);
  if (!nodeResult)
    return std::unexpected(std::move(nodeResult.error()));
  auto const& node = nodeResult->get();
  return node.connections.get();
}

std::expected<bool, StarException> Object::nodeState(WireNode wireNode) const {
  if (wireNode.direction == WireDirection::Input) {
    auto nodeResult = m_inputNodes.at(wireNode.nodeIndex);
    if (!nodeResult)
      return std::unexpected(std::move(nodeResult.error()));
    auto const& node = nodeResult->get();
    return node.state.get();
  }
  auto nodeResult = m_outputNodes.at(wireNode.nodeIndex);
  if (!nodeResult)
    return std::unexpected(std::move(nodeResult.error()));
  auto const& node = nodeResult->get();
  return node.state.get();
}
std::expected<String, StarException> Object::nodeIcon(WireNode wireNode) const {
  if (wireNode.direction == WireDirection::Input) {
    auto nodeResult = m_inputNodes.at(wireNode.nodeIndex);
    if (!nodeResult)
      return std::unexpected(std::move(nodeResult.error()));
    auto const& node = nodeResult->get();
    return node.icon;
  }
  auto nodeResult = m_outputNodes.at(wireNode.nodeIndex);
  if (!nodeResult)
    return std::unexpected(std::move(nodeResult.error()));
  auto const& node = nodeResult->get();
  return node.icon;
}

std::expected<Color, StarException> Object::nodeColor(WireNode wireNode) const {
  if (wireNode.direction == WireDirection::Input) {
    auto nodeResult = m_inputNodes.at(wireNode.nodeIndex);
    if (!nodeResult)
      return std::unexpected(std::move(nodeResult.error()));
    auto const& node = nodeResult->get();
    return node.color;
  }
  auto nodeResult = m_outputNodes.at(wireNode.nodeIndex);
  if (!nodeResult)
    return std::unexpected(std::move(nodeResult.error()));
  auto const& node = nodeResult->get();
  return node.color;
}

std::expected<void, StarException> Object::addNodeConnection(WireNode wireNode, WireConnection nodeConnection) {
  if (wireNode.direction == WireDirection::Input) {
    if (m_inputNodes.empty()) {
      auto operation1 = root().logger().info("Tried to add wire connection to input node on object with no input nodes");
      if (!operation1)
        return std::unexpected(std::move(operation1.error()));
      return {};
    }
    auto nodeResult = m_inputNodes.at(wireNode.nodeIndex);
    if (!nodeResult)
      return std::unexpected(std::move(nodeResult.error()));
    auto& node = nodeResult->get();
    node.connections.update([&](auto& list) {
      if (list.contains(nodeConnection))
        return false;
      list.append(nodeConnection);
      return true;
    });
  } else {
    if (m_outputNodes.empty()) {
      auto operation2 = root().logger().info("Tried to add wire connection to output node on object with no output nodes");
      if (!operation2)
        return std::unexpected(std::move(operation2.error()));
      return {};
    }
    auto nodeResult = m_outputNodes.at(wireNode.nodeIndex);
    if (!nodeResult)
      return std::unexpected(std::move(nodeResult.error()));
    auto& node = nodeResult->get();
    node.connections.update([&](auto& list) {
      if (list.contains(nodeConnection))
        return false;
      list.append(nodeConnection);
      return true;
    });
  }
  auto operation3 = m_scriptComponent.invoke("onNodeConnectionChange");
  if (!operation3)
    return std::unexpected(std::move(operation3.error()));
  return {};
}

std::expected<void, StarException> Object::removeNodeConnection(WireNode wireNode, WireConnection nodeConnection) {
  if (wireNode.direction == WireDirection::Input) {
    auto nodeResult = m_inputNodes.at(wireNode.nodeIndex);
    if (!nodeResult)
      return std::unexpected(std::move(nodeResult.error()));
    auto& node = nodeResult->get();
    node.connections.update([&](auto& list) {
      return list.remove(nodeConnection);
    });
  } else {
    auto nodeResult = m_outputNodes.at(wireNode.nodeIndex);
    if (!nodeResult)
      return std::unexpected(std::move(nodeResult.error()));
    auto& node = nodeResult->get();
    node.connections.update([&](auto& list) {
      return list.remove(nodeConnection);
    });
  }
  auto operation1 = m_scriptComponent.invoke("onNodeConnectionChange");
  if (!operation1)
    return std::unexpected(std::move(operation1.error()));
  return {};
}

std::expected<void, StarException> Object::evaluate(WireCoordinator& coordinator) {
  for (std::size_t i = 0; i < m_inputNodes.size(); ++i) {
    auto& node = m_inputNodes[i];
    bool nextState = false;
    for (auto const& connection : node.connections.get()) {
      auto stateResult = coordinator.readInputConnection(connection);
      if (!stateResult)
        return std::unexpected(std::move(stateResult.error()));
      auto state = std::move(*stateResult);
      nextState |= state;
    }
    if (node.state.get() != nextState) {
      node.state.set(nextState);
      auto operation1 = m_scriptComponent.invoke("onInputNodeChange", JsonObject{{"node", i}, {"level", nextState}});
      if (!operation1)
        return std::unexpected(std::move(operation1.error()));
    }
  }
  return {};
}

void Object::setImageKey(String const& name, String const& value) {
  if (!isSlave())
    m_netImageKeys.set(name, value);

  if (auto p = m_imageKeys.maybeRef(name)) {
    if (p->get() != value) {
      p->get() = value;
      m_orientationDrawablesCache.reset();
    }
  } else {
    m_imageKeys.set(name, value);
    m_orientationDrawablesCache.reset();
  }
}

void Object::resetEmissionTimers() {
  m_emissionTimers.clear();
  if (auto orientation = currentOrientation())
    for (std::size_t i = 0; i < orientation->particleEmitters.size(); i++)
      m_emissionTimers.append(GameTimer());
}

std::size_t Object::orientationIndex() const {
  return m_orientationIndex;
}

std::expected<void, StarException> Object::setOrientationIndex(std::size_t orientationIndex) {
  if (orientationIndex != NPos && orientationIndex >= getOrientations().size())
    return std::unexpected(ObjectException::format("Invalid orientation {} for object '{}'", orientationIndex, m_config->name));
  m_orientationIndex = orientationIndex;
  return {};
}

PolyF Object::volume() const {
  if (auto orientation = currentOrientation()) {
    RectF box = RectF(orientation->boundBox);
    box.max()[0]++;
    box.max()[1]++;
    return PolyF(box);
  } else {
    return PolyF(RectF(0, 0, 1, 1));
  }
}

std::expected<float, StarException> Object::liquidFillLevel() const {
  if (auto orientation = currentOrientation())
    return spacesLiquidFillLevel(orientation->spaces);

  return 0;
}

bool Object::biomePlaced() const {
  return m_config->biomePlaced;
}

std::expected<LuaCallbacks, StarException> Object::makeObjectCallbacks() {
  LuaCallbacks callbacks;
  auto operation1 = callbacks.registerCallback("name", [this]() {
  return name();
});
  if (!operation1)
    return std::unexpected(std::move(operation1.error()));
  auto operation2 = callbacks.registerCallback("direction", [this]() {
  return numericalDirection(direction());
});
  if (!operation2)
    return std::unexpected(std::move(operation2.error()));
  auto operation3 = callbacks.registerCallback("position", [this]() {
  return position();
});
  if (!operation3)
    return std::unexpected(std::move(operation3.error()));
  auto operation4 = callbacks.registerCallback("uniqueId", [this]() {
  return uniqueId();
});
  if (!operation4)
    return std::unexpected(std::move(operation4.error()));
  auto operation5 = callbacks.registerCallback("boundBox", [this]() {
  return metaBoundBox().translated(position());
});
  if (!operation5)
    return std::unexpected(std::move(operation5.error()));
  auto operation6 = callbacks.registerCallback("spaces", [this]() {
  return spaces();
});
  if (!operation6)
    return std::unexpected(std::move(operation6.error()));
  auto operation7 = callbacks.registerCallback("getLightColor", [this]() {
  return m_lightSourceColor.get();
});
  if (!operation7)
    return std::unexpected(std::move(operation7.error()));
  auto operation8 = callbacks.registerCallback("inputNodeCount", [this]() {
  return m_inputNodes.size();
});
  if (!operation8)
    return std::unexpected(std::move(operation8.error()));
  auto operation9 = callbacks.registerCallback("outputNodeCount", [this]() {
  return m_outputNodes.size();
});
  if (!operation9)
    return std::unexpected(std::move(operation9.error()));
  auto operation10 = callbacks.registerCallback("health", [this]() {
  return m_health.get();
});
  if (!operation10)
    return std::unexpected(std::move(operation10.error()));
  auto operation11 = callbacks.registerCallback("setInteractive", [this](bool interactive) {
  m_interactive.set(interactive);
});
  if (!operation11)
    return std::unexpected(std::move(operation11.error()));
  auto operation12 = callbacks.registerCallback("setUniqueId", [this](std::optional<String> id) {
  setUniqueId(std::move(id));
});
  if (!operation12)
    return std::unexpected(std::move(operation12.error()));
  auto operation13 = callbacks.registerCallback("setProcessingDirectives", [this](String const& directives) {
  m_networkedAnimator->setProcessingDirectives(directives);
});
  if (!operation13)
    return std::unexpected(std::move(operation13.error()));
  auto operation14 = callbacks.registerCallback("setSoundEffectEnabled", [this](bool enabled) {
  m_soundEffectEnabled.set(enabled);
});
  if (!operation14)
    return std::unexpected(std::move(operation14.error()));
  auto operation15 = callbacks.registerCallback("setLightColor", [this](Color const& color) {
  m_lightSourceColor.set(color);
});
  if (!operation15)
    return std::unexpected(std::move(operation15.error()));
  auto operation16 = callbacks.registerCallback("setTurnInQuests", [this](std::optional<StringList> const& quests) {
  m_turnInQuests.set(StringSet::from(quests.value_or(StringList{})));
});
  if (!operation16)
    return std::unexpected(std::move(operation16.error()));
  auto operation17 = callbacks.registerCallback("setConfigParameter", [this](String key, Json value) {
  m_parameters.set(std::move(key), std::move(value));
});
  if (!operation17)
    return std::unexpected(std::move(operation17.error()));
  auto operation18 = callbacks.registerCallback("setAnimationParameter", [this](String key, Json value) {
  m_scriptedAnimationParameters.set(std::move(key), std::move(value));
});
  if (!operation18)
    return std::unexpected(std::move(operation18.error()));
  auto operation19 = callbacks.registerCallback("setHealth", [this](float health) {
  m_health.set(health);
});
  if (!operation19)
    return std::unexpected(std::move(operation19.error()));
  auto operation20 = callbacks.registerCallback("smash", [this](std::optional<bool> smash) {
  return breakObject(smash.value_or(false));
});
  if (!operation20)
    return std::unexpected(std::move(operation20.error()));
  auto operation21 = callbacks.registerCallback("level", [this]() -> std::expected<Json, StarException> {
  auto currentWorldResult = world();
  if (!currentWorldResult)
    return std::unexpected(std::move(currentWorldResult.error()));
  auto& currentWorld = currentWorldResult->get();
  auto levelResult = currentWorld.threatLevel();
  if (!levelResult)
    return std::unexpected(std::move(levelResult.error()));
  auto level = std::move(*levelResult);
  return configValue("level", level);
});
  if (!operation21)
    return std::unexpected(std::move(operation21.error()));
  auto operation22 = callbacks.registerCallback("toAbsolutePosition", [this](Vec2F const& p) {
  return p + position();
});
  if (!operation22)
    return std::unexpected(std::move(operation22.error()));
  auto operation23 = callbacks.registerCallback("say", [this](String line, std::optional<StringMap<String>> const& tags, Json const& config) -> std::expected<bool, StarException> {
  if (tags)
    line = line.replaceTags(*tags, false);
  if (!line.empty()) {
    auto operation1 = addChatMessage(line, config);
    if (!operation1)
      return std::unexpected(std::move(operation1.error()));
    return true;
  }
  return false;
});
  if (!operation23)
    return std::unexpected(std::move(operation23.error()));
  auto operation24 = callbacks.registerCallback("sayPortrait", [this](String line, String portrait, std::optional<StringMap<String>> const& tags, Json const& config) -> std::expected<bool, StarException> {
  if (tags)
    line = line.replaceTags(*tags, false);
  if (!line.empty()) {
    auto operation1 = addChatMessage(line, config, portrait);
    if (!operation1)
      return std::unexpected(std::move(operation1.error()));
    return true;
  }
  return false;
});
  if (!operation24)
    return std::unexpected(std::move(operation24.error()));
  auto operation25 = callbacks.registerCallback("isTouching", [this](EntityId entityId) -> std::expected<bool, StarException> {
  auto currentWorldResult = world();
  if (!currentWorldResult)
    return std::unexpected(std::move(currentWorldResult.error()));
  auto& currentWorld = currentWorldResult->get();
  if (auto entity = currentWorld.entity(entityId))
    return !entity->collisionArea().overlap(volume().boundBox()).isEmpty();
  return false;
});
  if (!operation25)
    return std::unexpected(std::move(operation25.error()));
  auto operation26 = callbacks.registerCallback("getInputNodePosition", [this](std::size_t i) {
  return nodePosition(WireNode{WireDirection::Input, i});
});
  if (!operation26)
    return std::unexpected(std::move(operation26.error()));
  auto operation27 = callbacks.registerCallback("getInputNodeLevel", [this](std::size_t i) {
  return nodeState(WireNode{WireDirection::Input, i});
});
  if (!operation27)
    return std::unexpected(std::move(operation27.error()));
  auto operation28 = callbacks.registerCallback("isInputNodeConnected", [this](std::size_t i) -> std::expected<bool, StarException> {
  auto nodeResult = m_inputNodes.at(i);
  if (!nodeResult)
    return std::unexpected(std::move(nodeResult.error()));
  auto const& node = nodeResult->get();
  return !node.connections.get().empty();
});
  if (!operation28)
    return std::unexpected(std::move(operation28.error()));
  auto operation29 = callbacks.registerCallback("getInputNodeIds", [this](LuaEngine& engine, std::size_t i) -> std::expected<LuaTable, StarException> {
  auto resultResult = engine.createTable();
  if (!resultResult)
    return std::unexpected(std::move(resultResult.error()));
  auto result = std::move(*resultResult);
  auto nodeResult = m_inputNodes.at(i);
  if (!nodeResult)
    return std::unexpected(std::move(nodeResult.error()));
  auto const& node = nodeResult->get();
  auto currentWorldResult = world();
  if (!currentWorldResult)
    return std::unexpected(std::move(currentWorldResult.error()));
  auto& currentWorld = currentWorldResult->get();
  for (auto const& connection : node.connections.get()) {
    auto entitiesResult = currentWorld.atTile<WireEntity>(connection.entityLocation);
    if (!entitiesResult)
      return std::unexpected(std::move(entitiesResult.error()));
    auto entities = std::move(*entitiesResult);
    for (auto const& entity : entities) {
      auto operation1 = result.set(entity->entityId(), connection.nodeIndex);
      if (!operation1)
        return std::unexpected(std::move(operation1.error()));
    }
  }
  return result;
});
  if (!operation29)
    return std::unexpected(std::move(operation29.error()));
  auto operation30 = callbacks.registerCallback("getOutputNodePosition", [this](std::size_t i) {
  return nodePosition(WireNode{WireDirection::Output, i});
});
  if (!operation30)
    return std::unexpected(std::move(operation30.error()));
  auto operation31 = callbacks.registerCallback("getOutputNodeLevel", [this](std::size_t i) {
  return nodeState(WireNode{WireDirection::Output, i});
});
  if (!operation31)
    return std::unexpected(std::move(operation31.error()));
  auto operation32 = callbacks.registerCallback("isOutputNodeConnected", [this](std::size_t i) -> std::expected<bool, StarException> {
  auto nodeResult = m_outputNodes.at(i);
  if (!nodeResult)
    return std::unexpected(std::move(nodeResult.error()));
  auto const& node = nodeResult->get();
  return !node.connections.get().empty();
});
  if (!operation32)
    return std::unexpected(std::move(operation32.error()));
  auto operation33 = callbacks.registerCallback("getOutputNodeIds", [this](LuaEngine& engine, std::size_t i) -> std::expected<LuaTable, StarException> {
  auto resultResult = engine.createTable();
  if (!resultResult)
    return std::unexpected(std::move(resultResult.error()));
  auto result = std::move(*resultResult);
  auto nodeResult = m_outputNodes.at(i);
  if (!nodeResult)
    return std::unexpected(std::move(nodeResult.error()));
  auto const& node = nodeResult->get();
  auto currentWorldResult = world();
  if (!currentWorldResult)
    return std::unexpected(std::move(currentWorldResult.error()));
  auto& currentWorld = currentWorldResult->get();
  for (auto const& connection : node.connections.get()) {
    auto entitiesResult = currentWorld.atTile<WireEntity>(connection.entityLocation);
    if (!entitiesResult)
      return std::unexpected(std::move(entitiesResult.error()));
    auto entities = std::move(*entitiesResult);
    for (auto const& entity : entities) {
      auto operation1 = result.set(entity->entityId(), connection.nodeIndex);
      if (!operation1)
        return std::unexpected(std::move(operation1.error()));
    }
  }
  return result;
});
  if (!operation33)
    return std::unexpected(std::move(operation33.error()));
  auto operation34 = callbacks.registerCallback("setOutputNodeLevel", [this](std::size_t i, bool level) -> std::expected<void, StarException> {
  auto nodeResult = m_outputNodes.at(i);
  if (!nodeResult)
    return std::unexpected(std::move(nodeResult.error()));
  auto& node = nodeResult->get();
  node.state.set(level);
  return {};
});
  if (!operation34)
    return std::unexpected(std::move(operation34.error()));
  auto operation35 = callbacks.registerCallback("setAllOutputNodes", [this](bool level) {
  for (auto& node : m_outputNodes)
    node.state.set(level);
});
  if (!operation35)
    return std::unexpected(std::move(operation35.error()));
  auto operation36 = callbacks.registerCallback("setOfferedQuests", [this](std::optional<JsonArray> const& offeredQuests) -> std::expected<void, StarException> {
  List<QuestArcDescriptor> quests;
  for (auto const& entry : offeredQuests.value_or(JsonArray{})) {
    auto questResult = QuestArcDescriptor::fromJson(root(), entry);
    if (!questResult)
      return std::unexpected(std::move(questResult.error()));
    auto quest = std::move(*questResult);
    quests.append(std::move(quest));
  }
  m_offeredQuests.set(std::move(quests));
  return {};
});
  if (!operation36)
    return std::unexpected(std::move(operation36.error()));
  auto operation37 = callbacks.registerCallback("setMaterialSpaces", [this](std::optional<JsonArray> const& newSpaces) -> std::expected<void, StarException> {
  List<MaterialSpace> spaces;
  auto materialsResult = root().materialDatabase();
  if (!materialsResult)
    return std::unexpected(std::move(materialsResult.error()));
  auto materials = std::move(*materialsResult);
  for (auto const& space : newSpaces.value_or(JsonArray{})) {
    auto positionConfigResult = space.get(0);
    if (!positionConfigResult)
      return std::unexpected(std::move(positionConfigResult.error()));
    auto positionConfig = std::move(*positionConfigResult);
    auto positionResult = jsonToVec2I(positionConfig);
    if (!positionResult)
      return std::unexpected(std::move(positionResult.error()));
    auto position = std::move(*positionResult);
    auto nameResult = space.getString(1);
    if (!nameResult)
      return std::unexpected(std::move(nameResult.error()));
    auto name = std::move(*nameResult);
    auto materialResult = materials->materialId(name);
    if (!materialResult)
      return std::unexpected(std::move(materialResult.error()));
    auto material = std::move(*materialResult);
    spaces.append({position, material});
  }
  m_materialSpaces.set(std::move(spaces));
  return {};
});
  if (!operation37)
    return std::unexpected(std::move(operation37.error()));
  auto operation38 = callbacks.registerCallback("setDamageSources", [this](std::optional<JsonArray> damageSources) -> std::expected<void, StarException> {
  List<DamageSource> sources;
  for (auto const& entry : damageSources.value_or(JsonArray{})) {
    auto sourceResult = DamageSource::create(entry);
    if (!sourceResult)
      return std::unexpected(std::move(sourceResult.error()));
    auto source = std::move(*sourceResult);
    sources.append(std::move(source));
  }
  m_damageSources.set(std::move(sources));
  return {};
});
  if (!operation38)
    return std::unexpected(std::move(operation38.error()));
  return callbacks;
}

std::expected<LuaCallbacks, StarException> Object::makeAnimatorObjectCallbacks() {
  LuaCallbacks callbacks;
  auto operation1 = callbacks.registerCallback("getParameter", [this](String const& name, Json const& def) {
  return configValue(name, def);
});
  if (!operation1)
    return std::unexpected(std::move(operation1.error()));
  auto operation2 = callbacks.registerCallback("direction", [this]() {
  return numericalDirection(direction());
});
  if (!operation2)
    return std::unexpected(std::move(operation2.error()));
  auto operation3 = callbacks.registerCallback("position", [this]() {
  return position();
});
  if (!operation3)
    return std::unexpected(std::move(operation3.error()));
  return callbacks;
}

std::expected<List<DamageSource>, StarException> Object::damageSources() const {
  auto sources = m_damageSources.get();
  if (auto orientation = currentOrientation()) {
    auto touchDamageConfig = jsonMerge(m_config->touchDamageConfig, orientation->touchDamageConfig);
    if (!touchDamageConfig.isNull()) {
      auto sourceResult = DamageSource::create(touchDamageConfig);
      if (!sourceResult)
        return std::unexpected(std::move(sourceResult.error()));
      auto source = std::move(*sourceResult);
      source.sourceEntityId = entityId();
      source.team = getTeam();
      sources.append(std::move(source));
    }
  }
  return sources;
}

std::expected<List<PersistentStatusEffect>, StarException> Object::statusEffects() const {
  return m_config->statusEffects;
}

PolyF Object::statusEffectArea() const {
  if (auto orientation = currentOrientation()) {
    if (orientation->statusEffectArea)
      return *orientation->statusEffectArea;
  }
  return volume();
}

std::expected<std::optional<HitType>, StarException> Object::queryHit(DamageSource const& source) const {
  if (!m_config->smashable || !inWorld() || m_health.get() <= 0.0f || m_unbreakable)
    return std::nullopt;
  auto currentWorldResult = world();
  if (!currentWorldResult)
    return std::unexpected(std::move(currentWorldResult.error()));
  auto& currentWorld = currentWorldResult->get();
  auto poly = volume();
  poly.translate(position());
  auto intersectsResult = source.intersectsWithPoly(currentWorld.geometry(), poly);
  if (!intersectsResult)
    return std::unexpected(std::move(intersectsResult.error()));
  auto intersects = std::move(*intersectsResult);
  if (intersects)
    return HitType::Hit;
  return std::nullopt;
}

std::expected<std::optional<PolyF>, StarException> Object::hitPoly() const {
  auto poly = volume();
  poly.translate(position());
  return poly;
}

std::expected<List<DamageNotification>, StarException> Object::applyDamage(DamageRequest const& damage) {
  if (!m_config->smashable || !inWorld() || m_health.get() <= 0.0f)
    return List<DamageNotification>();
  auto notificationsResult = m_scriptComponent.invoke<List<DamageNotification>>("applyDamageRequest", damage);
  if (!notificationsResult)
    return std::unexpected(std::move(notificationsResult.error()));
  auto notifications = std::move(*notificationsResult);
  if (notifications) {
    float totalDamage = 0.0f;
    for (auto const& notification : *notifications)
      totalDamage += notification.healthLost;
    float dmg = std::min(m_health.get(), totalDamage);
    m_health.set(m_health.get() - dmg);
    return std::move(*notifications);
  }
  float dmg = std::min(m_health.get(), damage.damage);
  m_health.set(m_health.get() - dmg);
  return List<DamageNotification>{{damage.sourceEntityId, entityId(), position(), damage.damage, dmg, m_health.get() <= 0 ? HitType::Kill : HitType::Hit, damage.damageSourceKind, m_config->damageMaterialKind}};
}

RectF Object::interactiveBoundBox() const {
  if (auto orientation = currentOrientation()) {
    auto rect = RectF(orientation->boundBox);
    rect.setMax(Vec2F(orientation->boundBox.xMax() + 1, orientation->boundBox.yMax() + 1));
    return rect;
  } else {
    return RectF::null();
  }
}

bool Object::isInteractive() const {
  return m_interactive.get();
}

std::expected<InteractAction, StarException> Object::interact(InteractRequest const& request) {
  auto currentWorldResult = world();
  if (!currentWorldResult)
    return std::unexpected(std::move(currentWorldResult.error()));
  auto& currentWorld = currentWorldResult->get();
  Vec2F diff = currentWorld.geometry().diff(request.sourcePosition, position());
  auto resultResult = m_scriptComponent.invoke<Json>("onInteraction", JsonObject{{"source", JsonArray{diff[0], diff[1]}}, {"sourceId", request.sourceId}});
  if (!resultResult)
    return std::unexpected(std::move(resultResult.error()));
  auto result = std::move(*resultResult);
  if (result) {
    if (result->isNull())
      return InteractAction();
    if (result->isType(Json::Type::String)) {
      auto typeResult = result->toString();
      if (!typeResult)
        return std::unexpected(std::move(typeResult.error()));
      auto type = std::move(*typeResult);
      return InteractAction::create(type, entityId(), Json());
    }
    auto typeResult = result->getString(0);
    if (!typeResult)
      return std::unexpected(std::move(typeResult.error()));
    auto type = std::move(*typeResult);
    auto dataResult = result->get(1);
    if (!dataResult)
      return std::unexpected(std::move(dataResult.error()));
    auto data = std::move(*dataResult);
    return InteractAction::create(type, entityId(), std::move(data));
  }
  auto actionResult = configValue("interactAction", Json());
  if (!actionResult)
    return std::unexpected(std::move(actionResult.error()));
  auto action = std::move(*actionResult);
  if (!action.isNull()) {
    auto typeResult = action.toString();
    if (!typeResult)
      return std::unexpected(std::move(typeResult.error()));
    auto type = std::move(*typeResult);
    auto dataResult = configValue("interactData", Json());
    if (!dataResult)
      return std::unexpected(std::move(dataResult.error()));
    auto data = std::move(*dataResult);
    return InteractAction::create(type, entityId(), std::move(data));
  }
  return InteractAction();
}

List<Vec2I> Object::interactiveSpaces() const {
  if (auto orientation = currentOrientation()) {
    if (auto iSpaces = orientation->interactiveSpaces)
      return *iSpaces;
  }
  return spaces();
}

std::expected<std::optional<LuaValue>, StarException> Object::callScript(String const& func, LuaVariadic<LuaValue> const& args) {
  return m_scriptComponent.invoke(func, args);
}

std::expected<std::optional<LuaValue>, StarException> Object::evalScript(String const& code) {
  return m_scriptComponent.eval(code);
}

std::expected<Vec2F, StarException> Object::mouthPosition() const {
  if (auto orientation = currentOrientation()) {
    auto pos = position() + Vec2F(orientation->boundBox.center()[0], orientation->boundBox.max()[1]);
    if (!(orientation->boundBox.size()[0] % 2))
      pos[0] += 0.5;
    auto configPositionResult = configValue("mouthPosition", Json());
    if (!configPositionResult)
      return std::unexpected(std::move(configPositionResult.error()));
    auto configPosition = std::move(*configPositionResult);
    if (configPosition) {
      auto offsetResult = jsonToVec2F(configPosition);
      if (!offsetResult)
        return std::unexpected(std::move(offsetResult.error()));
      auto offset = std::move(*offsetResult);
      if (m_direction.get() == Direction::Left)
        offset[0] = -offset[0];
      pos += offset;
    }
    return pos;
  }
  return position();
}

std::expected<Vec2F, StarException> Object::mouthPosition(bool) const {
  return mouthPosition();
}

List<ChatAction> Object::pullPendingChatActions() {
  return std::move(m_pendingChatActions);
}

std::expected<void, StarException> Object::addChatMessage(String const& message, Json const& config, String const& portrait) {
  assert(!isSlave());
  auto mouthResult = mouthPosition();
  if (!mouthResult)
    return std::unexpected(std::move(mouthResult.error()));
  auto mouth = std::move(*mouthResult);
  m_chatMessage.set(message);
  m_chatPortrait.set(portrait);
  m_chatConfig.set(config);
  m_newChatMessageEvent.trigger();
  if (portrait.empty())
    m_pendingChatActions.append(SayChatAction{entityId(), message, mouth});
  else
    m_pendingChatActions.append(PortraitChatAction{entityId(), portrait, message, mouth});
  return {};
}

std::expected<List<Drawable>, StarException> Object::orientationDrawables(std::size_t orientationIndex) const {
  if (orientationIndex == NPos)
    return List<Drawable>();
  auto orientationEntryResult = getOrientations().at(orientationIndex);
  if (!orientationEntryResult)
    return std::unexpected(std::move(orientationEntryResult.error()));
  auto const& orientation = orientationEntryResult->get();
  if (!m_orientationDrawablesCache || orientationIndex != m_orientationDrawablesCache->first) {
    auto metadataResult = root().imageMetadataDatabase();
    if (!metadataResult)
      return std::unexpected(std::move(metadataResult.error()));
    auto metadata = std::move(*metadataResult);
    List<Drawable> generated;
    for (auto const& layer : orientation->imageLayers) {
      Drawable drawable = layer;
      auto& imagePart = drawable.imagePart();
      imagePart.image.directives.clear();
      String imagePath = AssetPath::join(imagePart.image);
      if ((m_colorDirectives || !m_colorSuffix.empty()) && m_imageKeys.contains("color")) {
        String& color = m_imageKeys.find("color")->second;
        auto colorEndResult = color.find('?');
        if (!colorEndResult)
          return std::unexpected(std::move(colorEndResult.error()));
        auto colorEnd = std::move(*colorEndResult);
        auto baseColorResult = color.substr(0, colorEnd);
        if (!baseColorResult)
          return std::unexpected(std::move(baseColorResult.error()));
        auto baseColor = std::move(*baseColorResult);
        String backup = std::move(color);
        color = std::move(baseColor);
        auto const& image = imagePath.utf8();
        std::size_t suffix = NPos;
        if (!m_colorSuffix.empty() && (suffix = image.rfind("<color>")) != NPos)
          imagePart.image = String(image.substr(0, (suffix += 7))).replaceTags(m_imageKeys, true, "default");
        else
          imagePart.image = imagePath.replaceTags(m_imageKeys, true, "default");
        color = std::move(backup);
        imagePart.image.directives = layer.imagePart().image.directives;
        if (m_colorDirectives) {
          auto operation1 = imagePart.addDirectives(*metadata, m_colorDirectives);
          if (!operation1)
            return std::unexpected(std::move(operation1.error()));
        }
        if (suffix != NPos) {
          auto operation2 = imagePart.addDirectives(*metadata, m_colorSuffix + String(image.substr(suffix)).replaceTags(m_imageKeys, true, "default"));
          if (!operation2)
            return std::unexpected(std::move(operation2.error()));
        }
      } else {
        imagePart.image = imagePath.replaceTags(m_imageKeys, true, "default");
        imagePart.image.directives = layer.imagePart().image.directives;
      }
      auto operation3 = imagePart.addDirectives(*metadata, m_directives);
      if (!operation3)
        return std::unexpected(std::move(operation3.error()));
      if (orientation->flipImages) {
        auto boundsResult = drawable.boundBox(*metadata, false);
        if (!boundsResult)
          return std::unexpected(std::move(boundsResult.error()));
        auto bounds = std::move(*boundsResult);
        drawable.scale(Vec2F(-1, 1), bounds.center() - drawable.position);
      }
      generated.append(std::move(drawable));
    }
    m_orientationDrawablesCache = std::make_pair(orientationIndex, std::move(generated));
  }
  auto drawables = m_orientationDrawablesCache->second;
  auto shakeResult = damageShake();
  if (!shakeResult)
    return std::unexpected(std::move(shakeResult.error()));
  auto shake = std::move(*shakeResult);
  Drawable::translateAll(drawables, orientation->imagePosition + shake);
  return drawables;
}

EntityRenderLayer Object::renderLayer() const {
  if (auto orientation = currentOrientation())
    return orientation->renderLayer;
  else
    return RenderLayerObject;
}

std::expected<void, StarException> Object::collectParticles(RenderCallback& renderCallback) {
  if (!inWorld())
    return {};
  if (auto orientation = currentOrientation()) {
    if (m_emissionTimers.size() != orientation->particleEmitters.size())
      resetEmissionTimers();
    for (std::size_t i = 0; i < orientation->particleEmitters.size(); ++i) {
      auto const& emitter = orientation->particleEmitters[i];
      if (emitter.particleEmissionRate <= 0.0f)
        continue;
      auto& timer = m_emissionTimers[i];
      if (timer.ready()) {
        auto particle = emitter.particle;
        particle.applyVariance(root(), emitter.particleVariance);
        if (emitter.placeInSpaces) {
          auto spaceResult = root().random().randFrom(orientation->spaces);
          if (!spaceResult)
            return std::unexpected(std::move(spaceResult.error()));
          auto const& space = spaceResult->get();
          particle.translate(Vec2F(space) + Vec2F(0.5, 0.5));
        }
        particle.translate(position());
        auto operation1 = renderCallback.addParticle(std::move(particle));
        if (!operation1)
          return std::unexpected(std::move(operation1.error()));
        auto varianceResult = root().random().randf(-emitter.particleEmissionRateVariance, emitter.particleEmissionRateVariance);
        if (!varianceResult)
          return std::unexpected(std::move(varianceResult.error()));
        auto variance = std::move(*varianceResult);
        timer = GameTimer(1.0f / (emitter.particleEmissionRate + variance));
      }
    }
  }
  return {};
}

std::expected<void, StarException> Object::collectSounds(RenderCallback& renderCallback) {
  if (m_soundEffectEnabled.get()) {
    if (!m_config->soundEffect.empty() && (!m_soundEffect || m_soundEffect->finished())) {
      auto assetsResult = root().assets();
      if (!assetsResult)
        return std::unexpected(std::move(assetsResult.error()));
      auto assets = std::move(*assetsResult);
      auto audioResult = assets->audio(m_config->soundEffect);
      if (!audioResult)
        return std::unexpected(std::move(audioResult.error()));
      auto audio = std::move(*audioResult);
      auto soundResult = AudioInstance::create(*audio);
      if (!soundResult)
        return std::unexpected(std::move(soundResult.error()));
      auto sound = std::move(*soundResult);
      sound->setLoops(-1);
      auto durationResult = sound->totalTime();
      if (!durationResult)
        return std::unexpected(std::move(durationResult.error()));
      auto duration = std::move(*durationResult);
      auto operation1 = sound->seekTime(root().random().randf() * duration);
      if (!operation1)
        return std::unexpected(std::move(operation1.error()));
      sound->setRangeMultiplier(m_config->soundEffectRangeMultiplier);
      sound->setPosition(metaBoundBox().center() + position());
      sound->setVolume(0);
      sound->setVolume(1, 1.0);
      auto operation2 = renderCallback.addAudio(sound);
      if (!operation2)
        return std::unexpected(std::move(operation2.error()));
      m_soundEffect = std::move(sound);
    }
  } else if (m_soundEffect) {
    m_soundEffect->stop();
  }
  return {};
}

List<std::shared_ptr<ObjectOrientation>> const& Object::getOrientations() const {
  return m_orientations ? *m_orientations : m_config->orientations;
}

std::expected<Vec2F, StarException> Object::damageShake() const {
  return m_presentationDamageShake;
}

std::expected<Vec2F, StarException> Object::computeDamageShake() const {
  if (m_tileDamageStatus->damaged() && !m_tileDamageStatus->damageProtected()) {
    auto xResult = root().random().randf(-1, 1);
    if (!xResult)
      return std::unexpected(std::move(xResult.error()));
    auto x = std::move(*xResult);
    auto yResult = root().random().randf(-1, 1);
    if (!yResult)
      return std::unexpected(std::move(yResult.error()));
    auto y = std::move(*yResult);
    return Vec2F(x, y) * m_tileDamageStatus->damageEffectPercentage() * m_config->damageShakeMagnitude;
  }
  return Vec2F();
}

std::expected<void, StarException> Object::checkLiquidBroken() {
  if (m_config->minimumLiquidLevel || m_config->maximumLiquidLevel) {
    auto levelResult = liquidFillLevel();
    if (!levelResult)
      return std::unexpected(std::move(levelResult.error()));
    auto level = std::move(*levelResult);
    if (m_config->minimumLiquidLevel && level < *m_config->minimumLiquidLevel)
      m_broken = true;
    if (m_config->maximumLiquidLevel && level > *m_config->maximumLiquidLevel)
      m_broken = true;
  }
  return {};
}

std::expected<Object::OutputNode, StarException> Object::OutputNode::create(Json const& positionConfig, Json const& config) {
  OutputNode node;
  auto positionResult = jsonToVec2I(positionConfig);
  if (!positionResult)
    return std::unexpected(std::move(positionResult.error()));
  auto position = std::move(*positionResult);
  node.position = position;
  auto colorConfigResult = config.get("color", jsonFromColor(Color::Red));
  if (!colorConfigResult)
    return std::unexpected(std::move(colorConfigResult.error()));
  auto colorConfig = std::move(*colorConfigResult);
  auto colorResult = jsonToColor(colorConfig);
  if (!colorResult)
    return std::unexpected(std::move(colorResult.error()));
  auto color = std::move(*colorResult);
  node.color = color;
  auto iconResult = config.getString("icon", "/interface/wires/outbound.png");
  if (!iconResult)
    return std::unexpected(std::move(iconResult.error()));
  auto icon = std::move(*iconResult);
  node.icon = std::move(icon);
  return node;
}

std::expected<Object::InputNode, StarException> Object::InputNode::create(Json const& positionConfig, Json const& config) {
  InputNode node;
  auto positionResult = jsonToVec2I(positionConfig);
  if (!positionResult)
    return std::unexpected(std::move(positionResult.error()));
  auto position = std::move(*positionResult);
  node.position = position;
  auto colorConfigResult = config.get("color", jsonFromColor(Color::Red));
  if (!colorConfigResult)
    return std::unexpected(std::move(colorConfigResult.error()));
  auto colorConfig = std::move(*colorConfigResult);
  auto colorResult = jsonToColor(colorConfig);
  if (!colorResult)
    return std::unexpected(std::move(colorResult.error()));
  auto color = std::move(*colorResult);
  node.color = color;
  auto iconResult = config.getString("icon", "/interface/wires/inbound.png");
  if (!iconResult)
    return std::unexpected(std::move(iconResult.error()));
  auto icon = std::move(*iconResult);
  node.icon = std::move(icon);
  return node;
}

}// namespace Star

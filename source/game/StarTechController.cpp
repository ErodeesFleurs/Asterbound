import star.format;
import star.json;
import star.json_extra;
import star.id_map;
import star.list;
import star.image_processing;
import star.hash;
import star.data_stream;
import star.string_view;
import star.thread;
import star.directives;
import star.data_stream_extra;
import star.casting;
import star.vector;
import star.game_types;
import star.strong_typedef;
import star.bi_map;
import star.lua;
import star.string;
import star.color;
import star.asset_path;
import star.rpc_promise;
import star.logging;
import star.interpolation;
import star.random;
import star.periodic_function;
import star.ordered_map;
import star.audio;
import star.map;
import star.set;
import star.observer_stream;
import star.either;
import star.a_star;
import star.ordered_set;
import star.io_device;
import star.ref_ptr;
import star.listener;
import std;
import star.version;

import star.animated_part_set;
import star.animation;
import star.particle;
import star.mixer;
import star.networked_animator;
import star.lua_components;
import star.lua_actor_movement_component;

import star.tech_database;

import star.tech_controller;
import star.stat_collection;
import star.status_effect_database;
import star.status_controller;
import star.lua_game_converters;
import star.uuid;
import star.humanoid;
import star.physics_entity;
import star.mobile_entity;
import star.actor_entity;
import star.tool_user_entity;
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
import star.ai_types;
import star.entity_rendering;
import star.player_types;
import star.lua_animation_component;
import star.radio_message_database;
import star.player;
import star.asset_source;
import star.assets;
import star.root_base;
import star.configuration;
import star.root;
import star.damage_types;
import star.world_geometry;
import star.status_types;
import star.damage;
import star.light_source;
import star.entity;
import star.scripted_entity;
import star.drawable;
import star.anchorable_entity;
import star.entity_rendering_types;
import star.lounging_entities;
import star.tile_damage;
import star.interaction_types;
import star.item_descriptor;
import star.celestial_coordinate;
import star.quest_descriptor;
import star.interactive_entity;
import star.collision_block;
import star.tile_entity;
import star.tile_modification;
import star.lua_root;
import star.force_regions;
import star.world;

import star.player_lua_bindings;
import star.entity_lua_bindings;
import star.networked_animator_lua_bindings;
import star.config_lua_bindings;
import star.status_controller_lua_bindings;

import star.net_element;

import star.net_compatibility;
import star.algorithm;
namespace Star {

std::expected<EnumMap<TechController::ParentState>, StarException> const TechController::ParentStateNames = EnumMap<TechController::ParentState>::create({
  {TechController::ParentState::Stand, "Stand"},
  {TechController::ParentState::Fly, "Fly"},
  {TechController::ParentState::Fall, "Fall"},
  {TechController::ParentState::Sit, "Sit"},
  {TechController::ParentState::Lay, "Lay"},
  {TechController::ParentState::Duck, "Duck"},
  {TechController::ParentState::Walk, "Walk"},
  {TechController::ParentState::Run, "Run"},
  {TechController::ParentState::Swim, "Swim"},
  {TechController::ParentState::SwimIdle, "SwimIdle"}
});

TechController::TechController(Root& root) : m_root(root), m_techAnimators([&root] { return TechAnimator::create(root); }) {

  m_moveRun = false;
  m_movePrimaryFire = false;
  m_moveAltFire = false;
  m_moveUp = false;
  m_moveDown = false;
  m_moveLeft = false;
  m_moveRight = false;
  m_moveJump = false;
  m_moveSpecial1 = false;
  m_moveSpecial2 = false;
  m_moveSpecial3 = false;


  m_xParentOffset.setFixedPointBase(0.003125);
  m_yParentOffset.setFixedPointBase(0.003125);
  m_xParentOffset.setInterpolator(lerp<float, float>);
  m_yParentOffset.setInterpolator(lerp<float, float>);
}

std::expected<std::shared_ptr<TechController>, StarException> TechController::create(Root& root) {
  auto controller = std::shared_ptr<TechController>(new TechController(root));
  for (std::reference_wrapper<NetElement> element : {std::ref<NetElement>(controller->m_techAnimators),
      std::ref<NetElement>(controller->m_parentState), std::ref<NetElement>(controller->m_parentDirectives),
      std::ref<NetElement>(controller->m_xParentOffset), std::ref<NetElement>(controller->m_yParentOffset),
      std::ref<NetElement>(controller->m_parentHidden), std::ref<NetElement>(controller->m_toolUsageSuppressed)}) {
    auto added = controller->addNetElement(element.get());
    if (!added)
      return std::unexpected(std::move(added.error()));
  }
  return controller;
}

std::expected<Json, StarException> TechController::diskStore() {
  JsonArray modules;
  if (m_overriddenTech) {
    for (auto const& name : *m_overriddenTech)
      modules.push_back(JsonObject{{"module", name}, {"scriptData", JsonObject()}});
  } else {
    for (auto const& module : m_techModules) {
      auto storage = module.scriptComponent.getScriptStorage();
      if (!storage)
        return std::unexpected(std::move(storage.error()));
      modules.push_back(JsonObject{{"module", module.config.name}, {"scriptData", std::move(*storage)}});
    }
  }
  return JsonObject{{"techModules", std::move(modules)}};
}

std::expected<void, StarException> TechController::diskLoad(Json const& store) {
  auto modules = store.getArray("techModules", {});
  if (!modules)
    return std::unexpected(std::move(modules.error()));
  List<std::tuple<String, JsonObject>> moduleInits;
  for (auto const& module : *modules) {
    auto name = module.getString("module");
    if (!name)
      return std::unexpected(std::move(name.error()));
    auto data = module.getObject("scriptData", {});
    if (!data)
      return std::unexpected(std::move(data.error()));
    moduleInits.push_back(std::make_tuple(std::move(*name), std::move(*data)));
  }
  return setupTechModules(moduleInits);
}

std::expected<void, StarException> TechController::init(Entity& parentEntity, ActorMovementController& movementController, StatusController& statusController) {
  m_parentEntity = std::ref(parentEntity);
  m_movementController = std::ref(movementController);
  m_statusController = std::ref(statusController);

  m_moveRun = false;
  m_movePrimaryFire = false;
  m_moveAltFire = false;
  resetMoves();

  if (m_parentEntity->get().isMaster())
    return initializeModules();
  return {};
}

std::expected<void, StarException> TechController::uninit() {
  std::optional<StarException> error;
  for (auto& module : m_techModules) {
    auto unloaded = unloadModule(module);
    if (!unloaded) {
      if (error)
        *error = error->with_context(unloaded.error().what());
      else
        error = std::move(unloaded.error());
    }
  }
  m_parentEntity.reset();
  m_movementController.reset();
  m_statusController.reset();
  if (error)
    return std::unexpected(std::move(*error));
  return {};
}

std::expected<void, StarException> TechController::setLoadedTech(StringList const& techModules, bool forceLoad) {
  if (forceLoad || loadedTech() != techModules) {
    auto setup = setupTechModules(techModules.transformed([](String const& module) { return std::make_tuple(module, JsonObject()); }));
    if (!setup)
      return std::unexpected(std::move(setup.error()));
    if (m_parentEntity)
      return initializeModules();
  }
  return {};
}

StringList TechController::loadedTech() const {
  return transform<StringList>(m_techModules, [](TechModule const& tm) { return tm.config.name; });
}

std::expected<void, StarException> TechController::reloadTech() {
  return setLoadedTech(loadedTech(), true);
}

bool TechController::techOverridden() const { return static_cast<bool>(m_overriddenTech); }

std::expected<void, StarException> TechController::setOverrideTech(StringList const& techModules) {
  if (!m_overriddenTech)
    m_overriddenTech = loadedTech();
  return setLoadedTech(techModules, true);
}

std::expected<void, StarException> TechController::clearOverrideTech() {
  if (m_overriddenTech) {
    auto loaded = setLoadedTech(*m_overriddenTech, true);
    if (!loaded)
      return std::unexpected(std::move(loaded.error()));
    m_overriddenTech = {};
  }
  return {};
}

void TechController::setShouldRun(bool shouldRun) {
  m_moveRun = shouldRun;
}

void TechController::beginPrimaryFire() {
  m_movePrimaryFire = true;
}

void TechController::beginAltFire() {
  m_moveAltFire = true;
}

void TechController::endPrimaryFire() {
  m_movePrimaryFire = false;
}

void TechController::endAltFire() {
  m_moveAltFire = false;
}

void TechController::moveUp() {
  m_moveUp = true;
}

void TechController::moveDown() {
  m_moveDown = true;
}

void TechController::moveLeft() {
  m_moveLeft = true;
}

void TechController::moveRight() {
  m_moveRight = true;
}

void TechController::jump() {
  m_moveJump = true;
}

void TechController::special(int specialKey) {
  if (specialKey == 1)
    m_moveSpecial1 = true;
  if (specialKey == 2)
    m_moveSpecial2 = true;
  if (specialKey == 3)
    m_moveSpecial3 = true;
}

void TechController::setAimPosition(Vec2F const& aimPosition) {
  m_aimPosition = aimPosition;
}

std::expected<void, StarException> TechController::tickMaster(float dt) {
  // if there's no gravity, fly instead of walking
  if (m_movementController->get().zeroG()) {
    if (m_moveRight || m_moveLeft || m_moveUp || m_moveDown) {
      Vec2F flightControl = {0, 0};

      if (m_moveRight)
        flightControl[0]++;
      if (m_moveLeft)
        flightControl[0]--;
      if (m_moveUp)
        flightControl[1]++;
      if (m_moveDown)
        flightControl[1]--;

      m_movementController->get().controlFly(flightControl);
    }
  } else {
    if (m_moveLeft != m_moveRight)
      m_movementController->get().controlMove(m_moveLeft ? Direction::Left : Direction::Right, m_moveRun);

    if (m_moveJump && !m_moveDown)
      m_movementController->get().controlJump();

    if (m_movementController->get().onGround() && m_moveDown && !m_moveJump)
      m_movementController->get().controlCrouch();
    else if (m_moveDown)
      m_movementController->get().controlDown();
  }

  for (auto& module : m_techModules) {
    JsonObject moves = {
      {"run", m_moveRun},
      {"up", m_moveUp},
      {"down", m_moveDown},
      {"left", m_moveLeft},
      {"right", m_moveRight},
      {"jump", m_moveJump},
      {"primaryFire", m_movePrimaryFire},
      {"altFire", m_moveAltFire},
      {"special1", m_moveSpecial1},
      {"special2", m_moveSpecial2},
      {"special3", m_moveSpecial3}
    };

    auto updated = recoverLuaScriptError(module.scriptComponent.update(JsonObject{{"moves", moves}, {"dt", module.scriptComponent.updateDt(dt)}}));
    if (!updated)
      return std::unexpected(std::move(updated.error()));
  }

  resetMoves();
  return updateAnimators(dt);
}

std::expected<void, StarException> TechController::tickSlave(float dt) {
  resetMoves();
  return updateAnimators(dt);
}

std::optional<TechController::ParentState> TechController::parentState() const {
  return m_parentState.get();
}

DirectivesGroup const& TechController::parentDirectives() const {
  return m_parentDirectives.get();
}

Vec2F TechController::parentOffset() const {
  return {m_xParentOffset.get(), m_yParentOffset.get()};
}

bool TechController::toolUsageSuppressed() const {
  return m_toolUsageSuppressed.get();
}

bool TechController::parentHidden() const {
  return m_parentHidden.get();
}

std::expected<List<Drawable>, StarException> TechController::backDrawables(float interpolation) {
  List<Drawable> drawables;

  for (auto const& animator : m_techAnimators.netElements()) {
    if (animator->isVisible()) {
      auto sample = animator->animator->sample(interpolation);
      auto pieces = sample.drawablesWithZLevel(m_movementController->get().position());
      if (!pieces)
        return std::unexpected(std::move(pieces.error()));
      for (auto& piece : *pieces) {
        if (piece.second < 0.0f)
          drawables.push_back(std::move(piece.first));
      }
    }
  }

  return drawables;
}

std::expected<List<Drawable>, StarException> TechController::frontDrawables(float interpolation) {
  List<Drawable> drawables;

  for (auto const& animator : m_techAnimators.netElements()) {
    if (animator->isVisible()) {
      auto sample = animator->animator->sample(interpolation);
      auto pieces = sample.drawablesWithZLevel(m_movementController->get().position());
      if (!pieces)
        return std::unexpected(std::move(pieces.error()));
      for (auto& piece : *pieces) {
        if (piece.second >= 0.0f)
          drawables.push_back(std::move(piece.first));
      }
    }
  }

  return drawables;
}

std::expected<List<LightSource>, StarException> TechController::lightSources(float interpolation) const {
  List<LightSource> lightSources;

  for (auto const& animator : m_techAnimators.netElements()) {
    if (animator->isVisible()) {
      auto sample = animator->animator->sample(interpolation);
      auto lights = sample.lightSources(m_movementController->get().position());
      if (!lights)
        return std::unexpected(std::move(lights.error()));
      lightSources.append_range(std::move(*lights));
    }
  }

  return lightSources;
}

List<std::shared_ptr<AudioInstance>> TechController::pullNewAudios() {
  List<std::shared_ptr<AudioInstance>> newAudios;

  for (auto const& animator : m_techAnimators.netElements()) {
    if (animator->isVisible())
      newAudios.append_range(animator->dynamicTarget.pullNewAudios());
    else
      animator->dynamicTarget.pullNewAudios();
  }

  return newAudios;
}

List<Particle> TechController::pullNewParticles() {
  List<Particle> newParticles;

  for (auto const& animator : m_techAnimators.netElements()) {
    if (animator->isVisible())
      newParticles.append_range(animator->dynamicTarget.pullNewParticles());
    else
      animator->dynamicTarget.pullNewParticles();
  }

  return newParticles;
}

std::expected<std::optional<ChainableJsonMessageResponse>, StarException> TechController::receiveMessage(String const& message, bool localMessage, JsonArray const& args) {
  for (auto& module : m_techModules) {
    auto response = recoverLuaScriptError(module.scriptComponent.handleMessage(message, localMessage, args));
    if (!response)
      return std::unexpected(std::move(response.error()));
    if (*response)
      return std::move(*response);
  }
  return std::optional<ChainableJsonMessageResponse>();
}

TechController::TechAnimator::TechAnimator(Root& root) : m_root(root) {}

std::expected<std::shared_ptr<TechController::TechAnimator>, StarException> TechController::TechAnimator::create(Root& root, std::optional<String> config) {
  auto result = std::make_shared<TechAnimator>(root);
  auto animator = config ? NetworkedAnimator::create(root, *config) : NetworkedAnimator::create(root);
  if (!animator)
    return std::unexpected(std::move(animator.error()));
  result->animationConfig = std::move(config);
  result->animator = std::move(*animator);
  if (auto added = result->netGroup.addNetElement(*result->animator); !added)
    return std::unexpected(std::move(added.error()));
  if (auto added = result->netGroup.addNetElement(result->visible); !added)
    return std::unexpected(std::move(added.error()));
  return result;
}

void TechController::TechAnimator::initNetVersion(std::optional<std::reference_wrapper<NetElementVersion const>> version) {
  netGroup.initNetVersion(version);
}

std::expected<void, StarException> TechController::TechAnimator::netStore(DataStream& ds, NetCompatibilityRules rules) const {
  if (!checkWithRules(rules))
    return {};
  auto written = ds << animationConfig;
  if (!written)
    return std::unexpected(std::move(written.error()));
  return netGroup.netStore(ds, rules);
}

std::expected<void, StarException> TechController::TechAnimator::netLoad(DataStream& ds, NetCompatibilityRules rules) {
  if (!checkWithRules(rules))
    return {};
  std::optional<String> config;
  auto read = ds >> config;
  if (!read)
    return std::unexpected(std::move(read.error()));
  auto created = config ? NetworkedAnimator::create(m_root, *config) : NetworkedAnimator::create(m_root);
  if (!created)
    return std::unexpected(std::move(created.error()));
  netGroup.clearNetElements();
  animationConfig = std::move(config);
  animator = std::move(*created);
  if (auto added = netGroup.addNetElement(*animator); !added)
    return std::unexpected(std::move(added.error()));
  if (auto added = netGroup.addNetElement(visible); !added)
    return std::unexpected(std::move(added.error()));
  return netGroup.netLoad(ds, rules);
}

std::expected<void, StarException> TechController::TechAnimator::enableNetInterpolation(float extrapolationHint) {
  return netGroup.enableNetInterpolation(extrapolationHint);
}

std::expected<void, StarException> TechController::TechAnimator::disableNetInterpolation() {
  return netGroup.disableNetInterpolation();
}

std::expected<void, StarException> TechController::TechAnimator::tickNetInterpolation(float dt) {
  return netGroup.tickNetInterpolation(dt);
}

std::expected<bool, StarException> TechController::TechAnimator::writeNetDelta(DataStream& ds, std::uint64_t fromVersion, NetCompatibilityRules rules) const {
  return netGroup.writeNetDelta(ds, fromVersion, rules);
}

std::expected<void, StarException> TechController::TechAnimator::readNetDelta(DataStream& ds, float interpolationTime, NetCompatibilityRules rules) {
  return netGroup.readNetDelta(ds, interpolationTime, rules);
}

std::expected<void, StarException> TechController::TechAnimator::blankNetDelta(float interpolationTime) {
  return netGroup.blankNetDelta(interpolationTime);
}

void TechController::TechAnimator::setVisible(bool visible) {
  this->visible.set(visible);
  if (!visible)
    dynamicTarget.stopAudio();
}

bool TechController::TechAnimator::isVisible() const {
  return visible.get();
}

std::expected<void, StarException> TechController::setupTechModules(List<std::tuple<String, JsonObject>> const& moduleInits) {
  std::optional<StarException> unloadError;
  for (auto& module : m_techModules) {
    auto unloaded = unloadModule(module);
    if (!unloaded) {
      if (unloadError)
        *unloadError = unloadError->with_context(unloaded.error().what());
      else
        unloadError = std::move(unloaded.error());
    }
  }
  if (unloadError)
    return std::unexpected(std::move(*unloadError));
  m_techModules.clear();
  m_techAnimators.clearNetElements();
  auto techDatabase = m_root.techDatabase();
  if (!techDatabase)
    return std::unexpected(std::move(techDatabase.error()));
  for (auto const& moduleInit : moduleInits) {
    if (!(*techDatabase)->contains(std::get<0>(moduleInit))) {
      auto logged = m_root.logger().warn("Tech module '{}' not found in tech database", std::get<0>(moduleInit));
      if (!logged)
        return std::unexpected(std::move(logged.error()));
      continue;
    }
    auto config = (*techDatabase)->tech(std::get<0>(moduleInit));
    if (!config)
      return std::unexpected(std::move(config.error()));
    auto visible = config->parameters.getBool("visible", true);
    if (!visible)
      return std::unexpected(std::move(visible.error()));
    auto moduleAnimator = TechAnimator::create(m_root, config->animationConfig);
    if (!moduleAnimator)
      return std::unexpected(std::move(moduleAnimator.error()));
    auto parts = config->parameters.getObject("animationParts", JsonObject());
    if (!parts)
      return std::unexpected(std::move(parts.error()));
    for (auto const& entry : *parts) {
      auto image = entry.second.toString();
      if (!image)
        return std::unexpected(std::move(image.error()));
      (*moduleAnimator)->animator->setPartTag(entry.first, "partImage", std::move(*image));
    }
    auto& module = m_techModules.emplace_back(m_root);
    module.config = std::move(*config);
    module.visible = *visible;
    module.toolUsageSuppressed = false;
    if (auto installed = module.scriptComponent.setScripts(module.config.scripts); !installed)
      return std::unexpected(std::move(installed.error()));
    if (auto stored = module.scriptComponent.setScriptStorage(std::get<1>(moduleInit)); !stored)
      return std::unexpected(std::move(stored.error()));
    auto added = m_techAnimators.addNetElement(std::move(*moduleAnimator));
    if (!added)
      return std::unexpected(std::move(added.error()));
    module.animatorId = *added;
  }
  return {};
}

std::expected<void, StarException> TechController::unloadModule(TechModule& module) {
  std::optional<StarException> error;
  auto remember = [&](auto result) {
    if (!result) {
      if (error)
        *error = error->with_context(result.error().what());
      else
        error = std::move(result.error());
    }
  };
  remember(recoverLuaScriptError(module.scriptComponent.uninit()));
  for (auto const& name : std::initializer_list<std::string_view>{"tech", "config", "entity", "animator", "status", "player"})
    remember(module.scriptComponent.removeCallbacks(name));
  remember(module.scriptComponent.removeActorMovementCallbacks());
  if (error)
    return std::unexpected(std::move(*error));
  return {};
}

std::expected<void, StarException> TechController::initializeModules() {
  auto initialized = [&]() -> std::expected<void, StarException> {
    for (auto& module : m_techModules) {
      auto add = [&](String name, std::expected<LuaCallbacks, StarException> callbacks) -> std::expected<void, StarException> {
        if (!callbacks)
          return std::unexpected(std::move(callbacks.error()));
        return module.scriptComponent.addCallbacks(std::move(name), std::move(*callbacks));
      };
      if (auto result = add("tech", makeTechCallbacks(module)); !result)
        return std::unexpected(std::move(result.error()));
      if (auto result = add("config", LuaBindings::makeConfigCallbacks(
          [&module](String const& name, Json const& def) { return module.config.parameters.query(name, def); })); !result)
        return std::unexpected(std::move(result.error()));
      if (auto result = add("entity", LuaBindings::makeEntityCallbacks(m_parentEntity->get())); !result)
        return std::unexpected(std::move(result.error()));
      auto animator = m_techAnimators.getNetElement(module.animatorId);
      if (!animator)
        return std::unexpected(std::move(animator.error()));
      if (auto result = add("animator", LuaBindings::makeNetworkedAnimatorCallbacks(*(*animator)->animator)); !result)
        return std::unexpected(std::move(result.error()));
      if (auto result = add("status", LuaBindings::makeStatusControllerCallbacks(m_statusController->get())); !result)
        return std::unexpected(std::move(result.error()));
      if (auto player = as_ref<Player>(m_parentEntity->get())) {
        if (auto result = add("player", LuaBindings::makePlayerCallbacks(m_root, player->get())); !result)
          return std::unexpected(std::move(result.error()));
      }
      if (auto added = module.scriptComponent.addActorMovementCallbacks(m_movementController->get()); !added)
        return std::unexpected(std::move(added.error()));
      auto world = m_parentEntity->get().world();
      if (!world)
        return std::unexpected(std::move(world.error()));
      if (auto result = recoverLuaScriptError(module.scriptComponent.init(world->get())); !result)
        return std::unexpected(std::move(result.error()));
    }
    return {};
  }();
  if (!initialized) {
    auto error = std::move(initialized.error());
    for (auto& module : m_techModules) {
      auto unloaded = unloadModule(module);
      if (!unloaded)
        error = error.with_context(unloaded.error().what());
    }
    return std::unexpected(std::move(error));
  }
  return {};
}

void TechController::resetMoves() {
  m_moveRun = false;
  m_moveUp = false;
  m_moveDown = false;
  m_moveLeft = false;
  m_moveRight = false;
  m_moveJump = false;
  m_moveSpecial1 = false;
  m_moveSpecial2 = false;
  m_moveSpecial3 = false;
}

std::expected<void, StarException> TechController::updateAnimators(float dt) {
  for (auto const& module : m_techModules) {
    auto animator = m_techAnimators.getNetElement(module.animatorId);
    if (!animator)
      return std::unexpected(std::move(animator.error()));
    (*animator)->setVisible(module.visible);
  }
  auto world = m_parentEntity->get().world();
  if (!world)
    return std::unexpected(std::move(world.error()));
  for (auto const& animator : m_techAnimators.netElements()) {
    auto dynamicTarget = world->get().isServer() || !animator->isVisible()
        ? std::optional<std::reference_wrapper<NetworkedAnimator::DynamicTarget>>{} : std::optional{std::ref(animator->dynamicTarget)};
    auto updated = animator->animator->update(dt, dynamicTarget);
    if (!updated)
      return std::unexpected(std::move(updated.error()));
    if (dynamicTarget)
      dynamicTarget->get().updatePosition(m_movementController->get().position());
  }
  return {};
}

std::expected<LuaCallbacks, StarException> TechController::makeTechCallbacks(TechModule& techModule) {
  LuaCallbacks callbacks;
  if (auto result = callbacks.registerCallback("aimPosition", [this]() { return m_aimPosition; }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.registerCallback("setVisible", [&techModule](bool visible) {
      techModule.visible = visible;
    }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.registerCallback("setParentState",
      [this](std::optional<String> const& state) -> std::expected<void, StarException> {
        if (state) {
          if (!ParentStateNames)
            return std::unexpected(ParentStateNames.error());
          auto value = ParentStateNames->get_left(*state);
          if (!value)
            return std::unexpected(std::move(value.error()));
          m_parentState.set(value->get());
        } else {
          m_parentState.set({});
        }
        return {};
      }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.registerCallback("setParentDirectives", [this, &techModule](std::optional<String> const& directives) {
      techModule.parentDirectives = directives.value_or(String{});
      DirectivesGroup combined;
      for (auto const& module : m_techModules)
        combined.append(module.parentDirectives);
      m_parentDirectives.set(std::move(combined));
    }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.registerCallback("setParentHidden", [this](bool hidden) { m_parentHidden.set(hidden); }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.registerCallback("setParentOffset", [this](Vec2F const& offset) {
      m_xParentOffset.set(offset[0]);
      m_yParentOffset.set(offset[1]);
    }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.registerCallback("parentLounging",
                                               [this]() {
                                                 if (auto entity = as_ref<LoungingEntity>(m_parentEntity->get()))
                                                   return entity->get().loungingIn().has_value();
                                                 return false;
                                               });
      !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.registerCallback("setToolUsageSuppressed", [this, &techModule](bool suppressed) {
      if (techModule.toolUsageSuppressed == suppressed)
        return;
      techModule.toolUsageSuppressed = suppressed;
      bool anySuppressed = false;
      for (auto const& module : m_techModules)
        anySuppressed |= module.toolUsageSuppressed;
      m_toolUsageSuppressed.set(anySuppressed);
    }); !result)
    return std::unexpected(std::move(result.error()));
  return callbacks;
}

}

import star.output_proxy;
import star.json;
import star.json_extra;
import star.id_map;
import star.poly;
import star.casting;
import star.vector;
import star.strong_typedef;
import star.list;
import star.rpc_promise;
import star.logging;
import star.data_stream;
import star.bi_map;
import star.game_types;
import star.random;
import star.weighted_pool;
import star.parametric_function;
import star.multi_array_interpolator;
import star.multi_table;
import star.thread;
import star.time;
import star.ordered_map;
import star.lru_cache;
import star.ttl_cache;
import star.audio;
import star.map;
import star.set;
import star.color;
import star.string;
import star.rect;
import star.image_processing;
import star.hash;
import star.string_view;
import star.directives;
import star.asset_path;
import star.a_star;
import star.lua;
import star.periodic_function;
import star.io_device;
import star.ref_ptr;
import star.listener;
import std;
import star.format;
import star.version;
import star.observer_stream;
import star.either;

import star.monster;
import star.data_stream_extra;
import star.mixer;
import star.drawable;
import star.entity_rendering_types;
import star.animation;
import star.particle;
import star.entity_rendering;
import star.physics_entity;
import star.movement_controller;
import star.platformer_astar_types;
import star.anchorable_entity;
import star.game_timers;
import star.actor_movement_controller;
import star.animated_part_set;
import star.networked_animator;
import star.portrait_entity;
import star.damage_bar_entity;
import star.nametag_entity;
import star.aggressive_entity;
import star.scripted_entity;
import star.chat_action;
import star.chatty_entity;
import star.lua_components;
import star.lua_converters;
import star.lua_game_converters;
import star.lua_animation_component;
import star.lua_actor_movement_component;
import star.mobile_entity;
import star.actor_entity;
import star.effect_emitter;
import star.damage_types;
import star.world_geometry;
import star.status_types;
import star.damage;
import star.light_source;
import star.entity;
import star.tile_damage;
import star.interaction_types;
import star.celestial_coordinate;
import star.quest_descriptor;
import star.interactive_entity;
import star.collision_block;
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
import star.item_descriptor;
import star.item_drop;
import star.stat_collection;
import star.status_effect_database;
import star.status_controller;
import star.treasure;
import star.stored_functions;

import star.networked_animator_lua_bindings;
import star.entity_lua_bindings;
import star.behavior_lua_bindings;
import star.scripted_animator_lua_bindings;
import star.config_lua_bindings;
import star.status_controller_lua_bindings;

import star.monster_database;
import star.behavior_state;

import star.net_compatibility;
import star.byte_array;
import star.line;
import star.algorithm;
namespace Star {

Monster::Monster(Root& rootContext) : Entity(rootContext), m_scriptComponent(rootContext), m_scriptedAnimator(rootContext) {}

std::expected<std::shared_ptr<Monster>, StarException> Monster::create(Root& rootContext, MonsterVariant const& monsterVariant, std::optional<float> level) {
  auto creature = std::shared_ptr<Monster>(new Monster(rootContext));
  auto initialized = creature->initialize(monsterVariant, level);
  if (!initialized)
    return std::unexpected(std::move(initialized.error()));
  return creature;
}

std::expected<void, StarException> Monster::initialize(MonsterVariant const& monsterVariant, std::optional<float> level) {
  m_monsterLevel = level;

  m_damageOnTouch = false;
  m_aggressive = false;

  m_knockedOut = false;
  m_knockoutTimer = 0.0;

  m_dropPool = monsterVariant.dropPoolConfig;

  m_monsterVariant = monsterVariant;

  auto result1 = this->root().assets();
if (!result1) return std::unexpected(std::move(result1.error()));
auto result2 = std::move(*result1)->json("/quests/quests.config:defaultIndicatorOffset");
if (!result2) return std::unexpected(std::move(result2.error()));
auto result3 = jsonToVec2F(std::move(*result2));
if (!result3) return std::unexpected(std::move(result3.error()));
m_questIndicatorOffset = std::move(*result3);

  setTeam(EntityDamageTeam(m_monsterVariant.damageTeamType, m_monsterVariant.damageTeam));

  auto result4 = NetworkedAnimator::create(this->root(), m_monsterVariant.animatorConfig);
if (!result4) return std::unexpected(std::move(result4.error()));
m_networkedAnimator = std::move(*result4);
  auto result5 = EffectEmitter::create(this->root());
if (!result5) return std::unexpected(std::move(result5.error()));
m_effectEmitter = std::move(*result5);
  for (auto const& pair : m_monsterVariant.animatorPartTags)
    m_networkedAnimator->setPartTag(pair.first, "partImage", pair.second);
  m_networkedAnimator->setZoom(m_monsterVariant.animatorZoom);
  auto result6 = this->root().monsterDatabase();
if (!result6) return std::unexpected(std::move(result6.error()));
auto result7 = m_monsterVariant.parameters.getString("colors", "default");
if (!result7) return std::unexpected(std::move(result7.error()));
auto result8 = std::move(*result6)->colorSwap(std::move(*result7), m_monsterVariant.seed);
if (!result8) return std::unexpected(std::move(result8.error()));
auto colorSwap = m_monsterVariant.colorSwap.value_or(std::move(*result8));
  if (!colorSwap.empty())
    m_networkedAnimator->setProcessingDirectives(imageOperationToString(ColorReplaceImageOperation{colorSwap}));

  auto result9 = StatusController::create(this->root(), m_monsterVariant.statusSettings);
if (!result9) return std::unexpected(std::move(result9.error()));
m_statusController = std::move(*result9);

  auto scriptsConfig = m_monsterVariant.parameters.optArray("scripts");
  if (!scriptsConfig) return std::unexpected(std::move(scriptsConfig.error()));
  if (*scriptsConfig) {
    auto scripts = jsonToStringList(**scriptsConfig);
    if (!scripts) return std::unexpected(std::move(scripts.error()));
    auto result10 = m_scriptComponent.setScripts(*scripts);
if (!result10) return std::unexpected(std::move(result10.error()));

  } else {
    auto result11 = m_scriptComponent.setScripts(m_monsterVariant.scripts);
if (!result11) return std::unexpected(std::move(result11.error()));

  }
  m_scriptComponent.setUpdateDelta(m_monsterVariant.initialScriptDelta);

  auto result12 = ActorMovementParameters::sensibleDefaults(this->root());
if (!result12) return std::unexpected(std::move(result12.error()));
auto movementParameters = std::move(*result12).merge(monsterVariant.movementSettings);
  if (movementParameters.standingPoly)
    movementParameters.standingPoly->scale(m_monsterVariant.animatorZoom);
  if (movementParameters.crouchingPoly)
    movementParameters.crouchingPoly->scale(m_monsterVariant.animatorZoom);
  *movementParameters.walkSpeed *= monsterVariant.walkMultiplier;
  *movementParameters.runSpeed *= monsterVariant.runMultiplier;
  *movementParameters.airJumpProfile.jumpSpeed *= monsterVariant.jumpMultiplier;
  *movementParameters.liquidJumpProfile.jumpSpeed *= monsterVariant.jumpMultiplier;
  *movementParameters.mass *= monsterVariant.weightMultiplier;
  if (!movementParameters.physicsEffectCategories)
    movementParameters.physicsEffectCategories = StringSet{"monster"};
  auto result13 = ActorMovementController::create(this->root(), movementParameters);
if (!result13) return std::unexpected(std::move(result13.error()));
m_movementController = std::move(*result13);

  setPersistent(m_monsterVariant.persistent);

  auto result14 = setupNetStates();
if (!result14) return std::unexpected(std::move(result14.error()));

  auto result15 = setNetStates();
if (!result15) return std::unexpected(std::move(result15.error()));
  return {};
}

std::expected<std::shared_ptr<Monster>, StarException> Monster::create(Root& rootContext, Json const& diskStore) {
  auto database = rootContext.monsterDatabase();
  if (!database) return std::unexpected(std::move(database.error()));
  auto encoded = diskStore.get("monsterVariant");
  if (!encoded) return std::unexpected(std::move(encoded.error()));
  auto variant = (*database)->readMonsterVariantFromJson(*encoded);
  if (!variant) return std::unexpected(std::move(variant.error()));
  auto creature = create(rootContext, *variant, {});
  if (!creature) return std::unexpected(std::move(creature.error()));
  auto loaded = (*creature)->loadState(diskStore);
  if (!loaded) return std::unexpected(std::move(loaded.error()));
  return creature;
}

std::expected<void, StarException> Monster::loadState(Json const& diskStore) {
  auto result16 = diskStore.optFloat("monsterLevel");
if (!result16) return std::unexpected(std::move(result16.error()));
m_monsterLevel = std::move(*result16);
  auto result17 = diskStore.get("movementState");
if (!result17) return std::unexpected(std::move(result17.error()));
auto result18 = m_movementController->loadState(std::move(*result17));
if (!result18) return std::unexpected(std::move(result18.error()));

  auto result19 = diskStore.get("statusController");
if (!result19) return std::unexpected(std::move(result19.error()));
auto result20 = m_statusController->diskLoad(std::move(*result19));
if (!result20) return std::unexpected(std::move(result20.error()));

  auto result21 = diskStore.getBool("damageOnTouch");
if (!result21) return std::unexpected(std::move(result21.error()));
m_damageOnTouch = std::move(*result21);
  auto result22 = diskStore.getBool("aggressive");
if (!result22) return std::unexpected(std::move(result22.error()));
m_aggressive = std::move(*result22);
  auto result23 = diskStore.getString("deathParticleBurst");
if (!result23) return std::unexpected(std::move(result23.error()));
m_deathParticleBurst = std::move(*result23);
  auto result24 = diskStore.getString("deathSound");
if (!result24) return std::unexpected(std::move(result24.error()));
m_deathSound = std::move(*result24);
  auto result25 = diskStore.getString("activeSkillName");
if (!result25) return std::unexpected(std::move(result25.error()));
m_activeSkillName = std::move(*result25);
  auto result26 = diskStore.get("dropPool");
if (!result26) return std::unexpected(std::move(result26.error()));
m_dropPool = std::move(*result26);
  auto result27 = diskStore.get("effectEmitter");
if (!result27) return std::unexpected(std::move(result27.error()));
auto result28 = m_effectEmitter->fromJson(std::move(*result27));
if (!result28) return std::unexpected(std::move(result28.error()));

  auto result29 = diskStore.getObject("scriptStorage");
if (!result29) return std::unexpected(std::move(result29.error()));
auto result30 = m_scriptComponent.setScriptStorage(std::move(*result29));
if (!result30) return std::unexpected(std::move(result30.error()));


  auto result31 = diskStore.optString("uniqueId");
if (!result31) return std::unexpected(std::move(result31.error()));
setUniqueId(std::move(*result31));
  {
  auto result32 = diskStore.contains("team");
  if (!result32) return std::unexpected(std::move(result32.error()));
  if (std::move(*result32))
      {
    auto result33 = diskStore.get("team");
    if (!result33) return std::unexpected(std::move(result33.error()));
    auto result34 = EntityDamageTeam::create(std::move(*result33));
    if (!result34) return std::unexpected(std::move(result34.error()));
    setTeam(std::move(*result34));
  }
}
  return {};
}

std::expected<Json, StarException> Monster::diskStore() const {
  auto result35 = m_movementController->storeState();
if (!result35) return std::unexpected(std::move(result35.error()));
auto result36 = m_statusController->diskStore();
if (!result36) return std::unexpected(std::move(result36.error()));
auto result37 = m_effectEmitter->toJson();
if (!result37) return std::unexpected(std::move(result37.error()));
auto result38 = this->root().monsterDatabase();
if (!result38) return std::unexpected(std::move(result38.error()));
auto result39 = std::move(*result38)->writeMonsterVariantToJson(m_monsterVariant);
if (!result39) return std::unexpected(std::move(result39.error()));
auto result40 = m_scriptComponent.getScriptStorage();
if (!result40) return std::unexpected(std::move(result40.error()));
auto result41 = getTeam().toJson();
if (!result41) return std::unexpected(std::move(result41.error()));
return JsonObject{
    {"monsterLevel", jsonFromOptional(m_monsterLevel)},
    {"movementState", std::move(*result35)},
    {"statusController", std::move(*result36)},
    {"damageOnTouch", m_damageOnTouch},
    {"aggressive", aggressive()},
    {"deathParticleBurst", m_deathParticleBurst},
    {"deathSound", m_deathSound},
    {"activeSkillName", m_activeSkillName},
    {"dropPool", m_dropPool},
    {"effectEmitter", std::move(*result37)},
    {"monsterVariant", std::move(*result39)},
    {"scriptStorage", std::move(*result40)},
    {"uniqueId", jsonFromOptional(uniqueId())},
    {"team", std::move(*result41)}
  };
}

std::expected<ByteArray, StarException> Monster::netStore(NetCompatibilityRules rules) {
  auto result42 = this->root().monsterDatabase();
if (!result42) return std::unexpected(std::move(result42.error()));
return std::move(*result42)->writeMonsterVariant(m_monsterVariant, rules);
}

EntityType Monster::entityType() const {
  return EntityType::Monster;
}

ClientEntityMode Monster::clientEntityMode() const {
  return m_monsterVariant.clientEntityMode;
}

std::expected<void, StarException> Monster::init(World& world, EntityId entityId, EntityMode mode) {
  auto result43 = Entity::init(world, entityId, mode);
if (!result43) return std::unexpected(std::move(result43.error()));


  m_movementController->init(world);
  m_movementController->setIgnorePhysicsEntities({entityId});
  auto result44 = m_statusController->init(*this, *m_movementController);
if (!result44) return std::unexpected(std::move(result44.error()));


  if (!m_monsterLevel)
    {
  auto result45 = world.threatLevel();
  if (!result45) return std::unexpected(std::move(result45.error()));
  m_monsterLevel = std::move(*result45);
}

  if (isMaster()) {
    auto result46 = this->root().functionDatabase();
if (!result46) return std::unexpected(std::move(result46.error()));
auto functionDatabase = std::move(*result46);
    auto result47 = functionDatabase->function(m_monsterVariant.healthLevelFunction);
if (!result47) return std::unexpected(std::move(result47.error()));
auto result48 = monsterLevel();
if (!result48) return std::unexpected(std::move(result48.error()));
auto result49 = std::move(*result47)->evaluate(std::move(*result48));
if (!result49) return std::unexpected(std::move(result49.error()));
float healthMultiplier = m_monsterVariant.healthMultiplier * std::move(*result49);
    auto result50 = m_statusController->setPersistentEffects("innate", {StatModifier(StatBaseMultiplier{"maxHealth", healthMultiplier})});
if (!result50) return std::unexpected(std::move(result50.error()));


    auto result51 = makeMonsterCallbacks();
if (!result51) return std::unexpected(std::move(result51.error()));
auto result52 = m_scriptComponent.addCallbacks("monster", std::move(*result51));
if (!result52) return std::unexpected(std::move(result52.error()));

    auto result53 = LuaBindings::makeConfigCallbacks([this](String const& name, Json const& def) {
        return m_monsterVariant.parameters.query(name, def);
      });
if (!result53) return std::unexpected(std::move(result53.error()));
auto result54 = m_scriptComponent.addCallbacks("config", std::move(*result53));
if (!result54) return std::unexpected(std::move(result54.error()));

    auto result55 = LuaBindings::makeEntityCallbacks(*this);
if (!result55) return std::unexpected(std::move(result55.error()));
auto result56 = m_scriptComponent.addCallbacks("entity", std::move(*result55));
if (!result56) return std::unexpected(std::move(result56.error()));

    auto result57 = LuaBindings::makeNetworkedAnimatorCallbacks(*m_networkedAnimator);
if (!result57) return std::unexpected(std::move(result57.error()));
auto result58 = m_scriptComponent.addCallbacks("animator", std::move(*result57));
if (!result58) return std::unexpected(std::move(result58.error()));

    auto result59 = LuaBindings::makeStatusControllerCallbacks(*m_statusController);
if (!result59) return std::unexpected(std::move(result59.error()));
auto result60 = m_scriptComponent.addCallbacks("status", std::move(*result59));
if (!result60) return std::unexpected(std::move(result60.error()));

    auto result61 = LuaBindings::makeBehaviorCallbacks(this->root(), m_behaviors);
if (!result61) return std::unexpected(std::move(result61.error()));
auto result62 = m_scriptComponent.addCallbacks("behavior", std::move(*result61));
if (!result62) return std::unexpected(std::move(result62.error()));

    auto result63 = m_scriptComponent.addActorMovementCallbacks(*m_movementController);
if (!result63) return std::unexpected(std::move(result63.error()));

    auto result64 = m_scriptComponent.init(world);
if (!result64) return std::unexpected(std::move(result64.error()));

  }

  if (world.isClient()) {
    auto result65 = m_scriptedAnimator.setScripts(m_monsterVariant.animationScripts);
if (!result65) return std::unexpected(std::move(result65.error()));


    auto result66 = LuaBindings::makeScriptedAnimatorCallbacks(*m_networkedAnimator,
      [this](String const& name, Json const& defaultValue) -> Json {
        return m_scriptedAnimationParameters.value(name, defaultValue);
      });
if (!result66) return std::unexpected(std::move(result66.error()));
auto result67 = m_scriptedAnimator.addCallbacks("animationConfig", std::move(*result66));
if (!result67) return std::unexpected(std::move(result67.error()));

    auto result68 = LuaBindings::makeConfigCallbacks([this](String const& name, Json const& def) {
        return m_monsterVariant.parameters.query(name, def);
      });
if (!result68) return std::unexpected(std::move(result68.error()));
auto result69 = m_scriptedAnimator.addCallbacks("config", std::move(*result68));
if (!result69) return std::unexpected(std::move(result69.error()));

    auto result70 = LuaBindings::makeEntityCallbacks(*this);
if (!result70) return std::unexpected(std::move(result70.error()));
auto result71 = m_scriptedAnimator.addCallbacks("entity", std::move(*result70));
if (!result71) return std::unexpected(std::move(result71.error()));

    auto result72 = m_scriptedAnimator.init(world);
if (!result72) return std::unexpected(std::move(result72.error()));

  }

  setPosition(position());
  return {};
}

std::expected<void, StarException> Monster::uninit() {
  if (isMaster()) {
    auto result73 = m_scriptComponent.uninit();
if (!result73) return std::unexpected(std::move(result73.error()));

    auto result74 = m_scriptComponent.removeCallbacks("monster");
if (!result74) return std::unexpected(std::move(result74.error()));

    auto result75 = m_scriptComponent.removeCallbacks("config");
if (!result75) return std::unexpected(std::move(result75.error()));

    auto result76 = m_scriptComponent.removeCallbacks("entity");
if (!result76) return std::unexpected(std::move(result76.error()));

    auto result77 = m_scriptComponent.removeCallbacks("animator");
if (!result77) return std::unexpected(std::move(result77.error()));

    auto result78 = m_scriptComponent.removeCallbacks("status");
if (!result78) return std::unexpected(std::move(result78.error()));

    auto result79 = m_scriptComponent.removeActorMovementCallbacks();
if (!result79) return std::unexpected(std::move(result79.error()));

  }
  {
  auto result80 = world();
  if (!result80) return std::unexpected(std::move(result80.error()));
  if (result80->get().isClient()) {
      auto result81 = m_scriptedAnimator.removeCallbacks("animationConfig");
  if (!result81) return std::unexpected(std::move(result81.error()));

      auto result82 = m_scriptedAnimator.removeCallbacks("config");
  if (!result82) return std::unexpected(std::move(result82.error()));

      auto result83 = m_scriptedAnimator.removeCallbacks("entity");
  if (!result83) return std::unexpected(std::move(result83.error()));

    }
}
  auto result84 = m_statusController->uninit();
if (!result84) return std::unexpected(std::move(result84.error()));

  m_movementController->uninit();
  auto result85 = Entity::uninit();
if (!result85) return std::unexpected(std::move(result85.error()));

  return {};
}

std::expected<Vec2F, StarException> Monster::mouthOffset() const {
  auto result86 = getAbsolutePosition(m_monsterVariant.mouthOffset);
if (!result86) return std::unexpected(std::move(result86.error()));
return std::move(*result86) - position();
}

std::expected<Vec2F, StarException> Monster::feetOffset() const {
  auto result87 = getAbsolutePosition(m_monsterVariant.feetOffset);
if (!result87) return std::unexpected(std::move(result87.error()));
return std::move(*result87) - position();
}

Vec2F Monster::position() const {
  return m_movementController->position();
}

RectF Monster::metaBoundBox() const {
  return m_monsterVariant.metaBoundBox;
}

RectF Monster::collisionArea() const {
  return m_movementController->collisionPoly().boundBox();
}

Vec2F Monster::velocity() const {
  return m_movementController->velocity();
}

std::expected<std::pair<ByteArray, std::uint64_t>, StarException> Monster::writeNetState(std::uint64_t fromVersion, NetCompatibilityRules rules) {
  return m_netGroup.writeNetState(fromVersion, rules);
}

std::expected<void, StarException> Monster::readNetState(ByteArray data, float interpolationTime, NetCompatibilityRules rules) {
  auto result88 = m_netGroup.readNetState(data, interpolationTime, rules);
if (!result88) return std::unexpected(std::move(result88.error()));

  return {};
}

std::expected<void, StarException> Monster::enableInterpolation(float extrapolationHint) {
  auto result89 = m_netGroup.enableNetInterpolation(extrapolationHint);
if (!result89) return std::unexpected(std::move(result89.error()));

  return {};
}

std::expected<void, StarException> Monster::disableInterpolation() {
  auto result90 = m_netGroup.disableNetInterpolation();
if (!result90) return std::unexpected(std::move(result90.error()));

  return {};
}

std::expected<String, StarException> Monster::name() const {
  auto const& name = m_name.get();
  return (name ? name : m_monsterVariant.shortDescription).value_or("");
}

std::expected<String, StarException> Monster::description() const {
  return m_monsterVariant.description.value_or("Some indescribable horror");
}

std::expected<std::optional<HitType>, StarException> Monster::queryHit(DamageSource const& source) const {
  if (!inWorld() || m_knockedOut || m_statusController->statPositive("invulnerable"))
    return {};

  auto body = hitPoly();
  if (!body) return std::unexpected(std::move(body.error()));
  if (!*body) return std::unexpected(MonsterException("Monster has no hit polygon"));
  {
  auto result91 = world();
  if (!result91) return std::unexpected(std::move(result91.error()));
  auto result92 = source.intersectsWithPoly(result91->get().geometry(), **body);
  if (!result92) return std::unexpected(std::move(result92.error()));
  if (std::move(*result92))
      return HitType::Hit;
}

  return {};
}

std::expected<std::optional<PolyF>, StarException> Monster::hitPoly() const {
  PolyF hitBody = m_monsterVariant.selfDamagePoly;
  hitBody.rotate(m_movementController->rotation());
  hitBody.translate(position());
  return hitBody;
}

std::expected<List<DamageNotification>, StarException> Monster::applyDamage(DamageRequest const& damage) {
  if (!inWorld())
    return {};

  auto result93 = m_statusController->applyDamageRequest(damage);
if (!result93) return std::unexpected(std::move(result93.error()));
auto notifications = std::move(*result93);

  float totalDamage = 0.0f;
  for (auto const& notification : notifications)
    totalDamage += notification.healthLost;

  if (totalDamage > 0.0f) {
    auto result94 = m_scriptComponent.invoke("damage", JsonObject{
        {"sourceId", damage.sourceEntityId},
        {"damage", totalDamage},
        {"sourceDamage", damage.damage},
        {"sourceKind", damage.damageSourceKind}
      });
if (!result94) return std::unexpected(std::move(result94.error()));

  }

  if (!m_statusController->resourcePositive("health"))
    m_deathDamageSourceKinds.add(damage.damageSourceKind);

  return notifications;
}

std::expected<List<DamageNotification>, StarException> Monster::selfDamageNotifications() {
  return m_statusController->pullSelfDamageNotifications();
}

std::expected<List<DamageSource>, StarException> Monster::damageSources() const {
  List<DamageSource> damageSources = m_damageSources.get();

  auto result95 = this->root().functionDatabase();
if (!result95) return std::unexpected(std::move(result95.error()));
auto result96 = std::move(*result95)->function(m_monsterVariant.powerLevelFunction);
if (!result96) return std::unexpected(std::move(result96.error()));
auto result97 = monsterLevel();
if (!result97) return std::unexpected(std::move(result97.error()));
auto result98 = std::move(*result96)->evaluate(std::move(*result97));
if (!result98) return std::unexpected(std::move(result98.error()));
float levelPowerMultiplier = std::move(*result98);
  if (m_damageOnTouch && !m_monsterVariant.touchDamageConfig.isNull()) {
    auto result99 = DamageSource::create(m_monsterVariant.touchDamageConfig);
if (!result99) return std::unexpected(std::move(result99.error()));
auto damageSource = std::move(*result99);
    if (std::holds_alternative<PolyF>(damageSource.damageArea))
      std::get<PolyF>(damageSource.damageArea).rotate(m_movementController->rotation());
    damageSource.damage *= m_monsterVariant.touchDamageMultiplier * levelPowerMultiplier * m_statusController->stat("powerMultiplier");
    damageSource.sourceEntityId = entityId();
    damageSource.team = getTeam();
    damageSources.push_back(damageSource);
  }

  auto animationDamageParts = m_monsterVariant.animationDamageParts;
  for (auto const& pair : m_monsterVariant.animationDamageParts) {
    if (!m_animationDamageParts.get().contains(pair.first))
      continue;

    auto result100 = pair.second.getString("anchorPart");
if (!result100) return std::unexpected(std::move(result100.error()));
String anchorPart = std::move(*result100);
    auto result101 = pair.second.get("damageSource");
if (!result101) return std::unexpected(std::move(result101.error()));
auto result102 = DamageSource::create(std::move(*result101));
if (!result102) return std::unexpected(std::move(result102.error()));
DamageSource ds = std::move(*result102);
    ds.damage *= levelPowerMultiplier * m_statusController->stat("powerMultiplier");
    auto result104 = std::visit([this,&anchorPart](auto& poly) -> std::expected<void, StarException> {
      if constexpr (!std::is_same_v<std::remove_cvref_t<decltype(poly)>, std::monostate>) {
        auto result103 = m_networkedAnimator->partTransformation(anchorPart);
if (!result103) return std::unexpected(std::move(result103.error()));
poly.transform(std::move(*result103));
        if (m_networkedAnimator->flipped())
          poly.flipHorizontal(m_networkedAnimator->flippedRelativeCenterLine());
      }
      return {};
    }, ds.damageArea);
if (!result104) return std::unexpected(std::move(result104.error()));

    if (std::holds_alternative<Vec2F>(ds.knockback)) {
      Vec2F knockback = std::get<Vec2F>(ds.knockback);
      auto result106 = m_networkedAnimator->partTransformation(anchorPart);
if (!result106) return std::unexpected(std::move(result106.error()));
knockback = std::move(*result106).transformVec2(knockback);
      if (m_networkedAnimator->flipped())
        knockback = Vec2F(-knockback[0], knockback[1]);
      ds.knockback = knockback;
    }

    List<DamageSource> partSources;
    if (std::holds_alternative<Line2F>(ds.damageArea)) {
      {
  auto result107 = pair.second.getBool("checkLineCollision", false);
  if (!result107) return std::unexpected(std::move(result107.error()));
  if (std::move(*result107)) {
          Line2F worldLine = std::get<Line2F>(ds.damageArea).translated(position());
          float length = worldLine.length();

          auto result108 = pair.second.getInt("bounces", 0);
  if (!result108) return std::unexpected(std::move(result108.error()));
  auto bounces = std::move(*result108);
          while (true) {
    auto result109 = world();
    if (!result109) return std::unexpected(std::move(result109.error()));
    auto result110 = result109->get().lineTileCollisionPoint(worldLine.min(), worldLine.max());
    if (!result110) return std::unexpected(std::move(result110.error()));
    auto collision =  std::move(*result110);
    if (!collision) break;
    worldLine = Line2F(worldLine.min(), collision.value_or(std::pair<Vec2F, Vec2I>{}).first);
              ds.damageArea = worldLine.translated(-position());
              length = length - worldLine.length();

              if (--bounces >= 0 && length > 0.0f) {
                partSources.push_back(ds);
                ds = DamageSource(ds);
                Vec2F dir = worldLine.direction();
                Vec2F normal = Vec2F((*collision).second);
                Vec2F reflection = dir - (2 * dir.piecewise_multiply(normal).sum() * normal);
                if (std::holds_alternative<Vec2F>(ds.knockback))
                  ds.knockback = std::get<Vec2F>(ds.knockback).rotate(reflection.angle_between(worldLine.direction()));

                worldLine.min() = (*collision).first;
                worldLine.max() = worldLine.min() + (reflection * length);
                ds.damageArea = worldLine.translated(-position());
              } else {
                break;
              }
  }
  partSources.push_back(ds);
        }
}
    } else {
      partSources.push_back(ds);
    }
    damageSources.append_range(partSources);
  }

  return damageSources;
}

std::expected<bool, StarException> Monster::shouldDie() {
  auto result = m_scriptComponent.invoke<bool>("shouldDie");
  if (!result)
    return std::unexpected(std::move(result.error()));
  if (*result)
    return **result;
  return !m_statusController->resourcePositive("health") || m_scriptComponent.error();
}

std::expected<void, StarException> Monster::knockout() {
  m_knockedOut = true;
  auto result113 = m_monsterVariant.parameters.getFloat("knockoutTime", 1.0f);
if (!result113) return std::unexpected(std::move(result113.error()));
m_knockoutTimer = std::move(*result113);

  m_damageOnTouch = false;

  auto result114 = m_monsterVariant.parameters.getString("knockoutEffect");
if (!result114) return std::unexpected(std::move(result114.error()));
String knockoutEffect = std::move(*result114);
  if (!knockoutEffect.empty())
    {
  auto result115 = m_networkedAnimator->setEffectEnabled(knockoutEffect, true);
  if (!result115) return std::unexpected(std::move(result115.error()));

}

  auto result116 = m_monsterVariant.parameters.getObject("knockoutAnimationStates", JsonObject());
if (!result116) return std::unexpected(std::move(result116.error()));
auto knockoutAnimationStates = std::move(*result116);
for (auto const& pair : knockoutAnimationStates) {
  auto result117 = pair.second.toString();
  if (!result117) return std::unexpected(std::move(result117.error()));
  auto result118 = m_networkedAnimator->setState(pair.first, std::move(*result117));
  if (!result118) return std::unexpected(std::move(result118.error()));
}
  return {};
}

std::expected<bool, StarException> Monster::shouldDestroy() const {
  return m_knockedOut && m_knockoutTimer <= 0;
}

std::expected<void, StarException> Monster::destroy(std::optional<std::reference_wrapper<RenderCallback>> renderCallback) {
  auto result119 = m_scriptComponent.invoke("die");
if (!result119) return std::unexpected(std::move(result119.error()));


  if (isMaster() && !m_dropPool.isNull()) {
    auto result120 = this->root().treasureDatabase();
if (!result120) return std::unexpected(std::move(result120.error()));
auto treasureDatabase = std::move(*result120);

    String treasurePool;
    if (m_dropPool.isType(Json::Type::String)) {
      auto result121 = m_dropPool.toString();
if (!result121) return std::unexpected(std::move(result121.error()));
treasurePool = std::move(*result121);
    } else {
      // Check to see whether any of the damage types that were used to cause
      // death are in the damage pool map, if so spawn treasure from that,
      // otherwise set the treasure pool to the "default" entry.

      for (auto const& damageSourceKind : m_deathDamageSourceKinds) {
        {
  auto result122 = m_dropPool.contains(damageSourceKind);
  if (!result122) return std::unexpected(std::move(result122.error()));
  if (std::move(*result122))
            {
    auto result123 = m_dropPool.getString(damageSourceKind);
    if (!result123) return std::unexpected(std::move(result123.error()));
    treasurePool = std::move(*result123);
  }
}
      }

      if (treasurePool.empty())
        {
  auto result124 = m_dropPool.getString("default");
  if (!result124) return std::unexpected(std::move(result124.error()));
  treasurePool = std::move(*result124);
}
    }

    auto dropped = [&]() -> std::expected<void, StarException> {
      {
  auto result125 = monsterLevel();
  if (!result125) return std::unexpected(std::move(result125.error()));
  auto result126 = treasureDatabase->createTreasure(treasurePool, std::move(*result125));
  if (!result126) return std::unexpected(std::move(result126.error()));
  for (auto const& treasureItem : std::move(*result126))
          {
    auto result127 = world();
    if (!result127) return std::unexpected(std::move(result127.error()));
    auto result128 = ItemDrop::createRandomizedDrop(this->root(), treasureItem, position());
    if (!result128) return std::unexpected(std::move(result128.error()));
    auto result129 = result127->get().addEntity(std::move(*result128));
    if (!result129) return std::unexpected(std::move(result129.error()));

  }
}
      return {};
    }();
    if (!dropped) {
      auto const& e = dropped.error();
      auto result130 = this->root().logger().warnException(strf("Failed to create treasure for monster '{}'", m_monsterVariant.type), ExceptionDiagnostic(e));
if (!result130) return std::unexpected(std::move(result130.error()));
    
    }
  }

  if (renderCallback) {
    if (!m_deathParticleBurst.empty())
      {
  auto result131 = m_networkedAnimator->burstParticleEmitter(m_deathParticleBurst);
  if (!result131) return std::unexpected(std::move(result131.error()));

}

    if (!m_deathSound.empty())
      {
  auto result132 = m_networkedAnimator->playSound(m_deathSound);
  if (!result132) return std::unexpected(std::move(result132.error()));

}

    auto result133 = m_networkedAnimator->update(0.0, std::ref(m_networkedAnimatorDynamicTarget));
if (!result133) return std::unexpected(std::move(result133.error()));


    auto result134 = renderCallback->get().addAudios(m_networkedAnimatorDynamicTarget.pullNewAudios());
if (!result134) return std::unexpected(std::move(result134.error()));
    auto result135 = renderCallback->get().addParticles(m_networkedAnimatorDynamicTarget.pullNewParticles());
if (!result135) return std::unexpected(std::move(result135.error()));
    auto result136 = renderCallback->get().addParticles(m_statusController->pullNewParticles());
if (!result136) return std::unexpected(std::move(result136.error()));
  }

  m_deathDamageSourceKinds.clear();

  if (isMaster())
    {
  auto result137 = setNetStates();
  if (!result137) return std::unexpected(std::move(result137.error()));

}
  return {};
}

std::expected<List<LightSource>, StarException> Monster::lightSources() const {
  auto result138 = m_networkedAnimator->lightSources(position());
if (!result138) return std::unexpected(std::move(result138.error()));
auto lightSources = std::move(*result138);
  auto result139 = m_statusController->lightSources();
if (!result139) return std::unexpected(std::move(result139.error()));
lightSources.append_range(std::move(*result139));
return lightSources;
}

std::expected<void, StarException> Monster::hitOther(EntityId targetEntityId, DamageRequest const& damageRequest) {
  if (inWorld() && isMaster())
    m_statusController->hitOther(targetEntityId, damageRequest);
  return {};
}

std::expected<void, StarException> Monster::damagedOther(DamageNotification const& damage) {
  if (inWorld() && isMaster())
    m_statusController->damagedOther(damage);
  return {};
}

std::expected<void, StarException> Monster::update(float dt, std::uint64_t) {
  if (!inWorld())
    return {};

  m_movementController->setTimestep(dt);

  if (isMaster()) {
    m_networkedAnimator->setFlipped((m_movementController->facingDirection() == Direction::Left) != m_monsterVariant.reversed);

    if (m_knockedOut) {
      m_knockoutTimer -= dt;
    } else {
      if (m_scriptComponent.updateReady())
        m_physicsForces.set({});
      auto result140 = m_scriptComponent.update(m_scriptComponent.updateDt(dt));
if (!result140) return std::unexpected(std::move(result140.error()));


      {
  auto result141 = shouldDie();
  if (!result141) return std::unexpected(std::move(result141.error()));
  if (std::move(*result141))
          {
    auto result142 = knockout();
    if (!result142) return std::unexpected(std::move(result142.error()));

  }
}
    }

    auto result143 = m_movementController->tickMaster(dt);
if (!result143) return std::unexpected(std::move(result143.error()));


    auto result144 = m_statusController->tickMaster(dt);
if (!result144) return std::unexpected(std::move(result144.error()));

    auto result145 = updateStatus(dt);
if (!result145) return std::unexpected(std::move(result145.error()));

  } else {
    auto result146 = m_netGroup.tickNetInterpolation(dt);
if (!result146) return std::unexpected(std::move(result146.error()));


    auto result147 = m_statusController->tickSlave(dt);
if (!result147) return std::unexpected(std::move(result147.error()));

    auto result148 = updateStatus(dt);
if (!result148) return std::unexpected(std::move(result148.error()));


    auto result149 = m_movementController->tickSlave(dt);
if (!result149) return std::unexpected(std::move(result149.error()));

  }

  {
  auto result150 = world();
  if (!result150) return std::unexpected(std::move(result150.error()));
  if (result150->get().isServer()) {
      auto result151 = m_networkedAnimator->update(dt, std::nullopt);
  if (!result151) return std::unexpected(std::move(result151.error()));

    } else {
      auto result152 = m_networkedAnimator->update(dt, std::ref(m_networkedAnimatorDynamicTarget));
  if (!result152) return std::unexpected(std::move(result152.error()));

      m_networkedAnimatorDynamicTarget.updatePosition(position());

      auto result153 = m_scriptedAnimator.update();
  if (!result153) return std::unexpected(std::move(result153.error()));


      this->root().spatialLogger().logPoly("world", m_movementController->collisionBody(), { 255, 0, 0, 255 });
    }
}
  return {};
}

std::uint64_t Monster::presentationEpoch() const {
  return Entity::presentationEpoch() + m_movementController->presentationEpoch();
}

std::expected<void, StarException> Monster::collectRenderEvents(RenderCallback& renderCallback) {
  auto result158 = renderCallback.addAudios(m_networkedAnimatorDynamicTarget.pullNewAudios());
if (!result158) return std::unexpected(std::move(result158.error()));
  auto result159 = renderCallback.addParticles(m_networkedAnimatorDynamicTarget.pullNewParticles());
if (!result159) return std::unexpected(std::move(result159.error()));
  auto result162 = renderCallback.addParticles(m_statusController->pullNewParticles());
if (!result162) return std::unexpected(std::move(result162.error()));
  auto result163 = renderCallback.addAudios(m_statusController->pullNewAudios());
if (!result163) return std::unexpected(std::move(result163.error()));

  auto result164 = m_effectEmitter->collectRenderEvents(renderCallback);
if (!result164) return std::unexpected(std::move(result164.error()));
  auto result166 = renderCallback.addAudios(m_scriptedAnimator.pullNewAudios());
if (!result166) return std::unexpected(std::move(result166.error()));
  auto result167 = renderCallback.addParticles(m_scriptedAnimator.pullNewParticles());
if (!result167) return std::unexpected(std::move(result167.error()));
  return {};
}

std::expected<void, StarException> Monster::render(RenderCallback& renderCallback) {
  auto sample = m_networkedAnimator->sample(renderCallback.presentation().interpolation());
  {
  auto result154 = sample.drawables(position());
  if (!result154) return std::unexpected(std::move(result154.error()));
  for (auto& drawable : std::move(*result154)) {
      if (drawable.isImage())
        {
    auto result155 = this->root().imageMetadataDatabase();
    if (!result155) return std::unexpected(std::move(result155.error()));
    auto result156 = drawable.imagePart().addDirectivesGroup(*std::move(*result155), m_statusController->parentDirectives(), true);
    if (!result156) return std::unexpected(std::move(result156.error()));

  }
      auto result157 = renderCallback.addDrawable(std::move(drawable), m_monsterVariant.renderLayer);
  if (!result157) return std::unexpected(std::move(result157.error()));
    }
}


  auto result160 = m_statusController->drawables(renderCallback.presentation().interpolation());
if (!result160) return std::unexpected(std::move(result160.error()));
auto result161 = renderCallback.addDrawables(std::move(*result160), m_monsterVariant.renderLayer);
if (!result161) return std::unexpected(std::move(result161.error()));

for (auto const& drawablePair : m_scriptedAnimator.drawables()) {
  auto result165 = renderCallback.addDrawable(drawablePair.first, drawablePair.second.value_or(m_monsterVariant.renderLayer));
  if (!result165) return std::unexpected(std::move(result165.error()));
}
  return {};
}

std::expected<void, StarException> Monster::renderLightSources(RenderCallback& renderCallback) {
  auto sample = m_networkedAnimator->sample(renderCallback.presentation().interpolation());
  auto result168 = sample.lightSources(position());
if (!result168) return std::unexpected(std::move(result168.error()));
auto result169 = renderCallback.addLightSources(std::move(*result168));
if (!result169) return std::unexpected(std::move(result169.error()));
  auto result170 = m_statusController->lightSources(renderCallback.presentation().interpolation());
if (!result170) return std::unexpected(std::move(result170.error()));
auto result171 = renderCallback.addLightSources(std::move(*result170));
if (!result171) return std::unexpected(std::move(result171.error()));
  auto result172 = renderCallback.addLightSources(m_scriptedAnimator.lightSources());
if (!result172) return std::unexpected(std::move(result172.error()));
  return {};
}

void Monster::setPosition(Vec2F const& pos) {
  resetPresentation();
  m_movementController->setPosition(pos);
}

std::expected<std::optional<ChainableJsonMessageResponse>, StarException> Monster::receiveMessage(ConnectionId sendingConnection, String const& message, JsonArray const& args) {
  auto result173 = world();
if (!result173) return std::unexpected(std::move(result173.error()));
auto result174 = m_scriptComponent.handleMessage(message, result173->get().connection() == sendingConnection, args);
if (!result174) return std::unexpected(std::move(result174.error()));
std::optional<ChainableJsonMessageResponse> result = std::move(*result174);
  if (!result)
    {
  auto result175 = world();
  if (!result175) return std::unexpected(std::move(result175.error()));
  auto result176 = m_statusController->receiveMessage(message, result175->get().connection() == sendingConnection, args);
  if (!result176) return std::unexpected(std::move(result176.error()));
  result = std::move(*result176);
}
  return result;
}

std::expected<float, StarException> Monster::maxHealth() const {
  auto maximum = m_statusController->resourceMax("health");
  if (!maximum) return std::unexpected(std::move(maximum.error()));
  if (!*maximum) return std::unexpected(MonsterException("Health resource has no maximum"));
  return **maximum;
}

float Monster::health() const {
  return m_statusController->resource("health");
}

DamageBarType Monster::damageBar() const {
  return m_damageBar.get();
}

std::expected<Vec2F, StarException> Monster::getAbsolutePosition(Vec2F relativePosition) const {
  if (m_movementController->facingDirection() == Direction::Left)
    relativePosition[0] *= -1;
  if (m_movementController->rotation() != 0)
    relativePosition = relativePosition.rotate(m_movementController->rotation());
  return m_movementController->position() + relativePosition;
}

std::expected<void, StarException> Monster::updateStatus(float dt) {
  m_effectEmitter->setSourcePosition("normal", position());
  auto result177 = mouthOffset();
if (!result177) return std::unexpected(std::move(result177.error()));
m_effectEmitter->setSourcePosition("mouth", position() + std::move(*result177));
  auto result178 = feetOffset();
if (!result178) return std::unexpected(std::move(result178.error()));
m_effectEmitter->setSourcePosition("feet", position() + std::move(*result178));
  m_effectEmitter->setDirection(m_movementController->facingDirection());
  auto result179 = m_effectEmitter->tick(dt, *entityMode());
if (!result179) return std::unexpected(std::move(result179.error()));

  return {};
}

std::expected<LuaCallbacks, StarException> Monster::makeMonsterCallbacks() {
  LuaCallbacks callbacks;

  auto result180 = callbacks.registerCallback("type", [this]() {
      return m_monsterVariant.type;
    });
if (!result180) return std::unexpected(std::move(result180.error()));

  auto result181 = callbacks.registerCallback("seed", [this]() {
      return toString(m_monsterVariant.seed);
    });
if (!result181) return std::unexpected(std::move(result181.error()));

  auto result182 = callbacks.registerCallback("uniqueParameters", [this]() {
      return m_monsterVariant.uniqueParameters;
    });
if (!result182) return std::unexpected(std::move(result182.error()));

  auto result183 = callbacks.registerCallback("level", [this]() {
      return monsterLevel();
    });
if (!result183) return std::unexpected(std::move(result183.error()));

  auto result184 = callbacks.registerCallback("setDamageOnTouch", [this](bool arg1) {
      m_damageOnTouch = arg1;
    });
if (!result184) return std::unexpected(std::move(result184.error()));

  auto result185 = callbacks.registerCallback("setDamageSources", [this](std::optional<JsonArray> const& damageSources) -> std::expected<void, StarException> {
      List<DamageSource> parsed;
      for (auto const& source : damageSources.value_or(JsonArray{})) {
        auto damage = DamageSource::create(source);
        if (!damage) return std::unexpected(std::move(damage.error()));
        parsed.push_back(std::move(*damage));
      }
      m_damageSources.set(std::move(parsed));
      return {};
    });
if (!result185) return std::unexpected(std::move(result185.error()));

  auto result186 = callbacks.registerCallback("setDamageParts", [this](StringSet const& parts) {
      m_animationDamageParts.set(parts);
    });
if (!result186) return std::unexpected(std::move(result186.error()));

  auto result187 = callbacks.registerCallback("setAggressive", [this](bool arg1) {
      m_aggressive = arg1;
    });
if (!result187) return std::unexpected(std::move(result187.error()));

  auto result188 = callbacks.registerCallback("setActiveSkillName", [this](std::optional<String> const& activeSkillName) {
      m_activeSkillName = activeSkillName.value_or(String{});
    });
if (!result188) return std::unexpected(std::move(result188.error()));

  auto result189 = callbacks.registerCallback("setDropPool", [this](Json dropPool) {
      m_dropPool = std::move(dropPool);
    });
if (!result189) return std::unexpected(std::move(result189.error()));

  auto result190 = callbacks.registerCallback("toAbsolutePosition", [this](Vec2F const& p) {
      return getAbsolutePosition(p);
    });
if (!result190) return std::unexpected(std::move(result190.error()));

  auto result191 = callbacks.registerCallback("mouthPosition", [this]() {
      return mouthPosition();
    });
if (!result191) return std::unexpected(std::move(result191.error()));

  // This callback is registered here rather than in
  // makeActorMovementControllerCallbacks
  // because it requires access to world
  auto result193 = callbacks.registerCallback("flyTo", [this](Vec2F const& arg1) -> std::expected<void, StarException> {
      auto result192 = world();
if (!result192) return std::unexpected(std::move(result192.error()));
m_movementController->controlFly(result192->get().geometry().diff(arg1, position()));
      return {};
});
if (!result193) return std::unexpected(std::move(result193.error()));

  auto result194 = callbacks.registerCallback("setDeathParticleBurst", [this](std::optional<String> const& arg1) {
      m_deathParticleBurst = arg1.value_or(String{});
    });
if (!result194) return std::unexpected(std::move(result194.error()));

  auto result195 = callbacks.registerCallback("setDeathSound", [this](std::optional<String> const& arg1) {
      m_deathSound = arg1.value_or(String{});
    });
if (!result195) return std::unexpected(std::move(result195.error()));

  auto result196 = callbacks.registerCallback("setPhysicsForces", [this](JsonArray const& forces) -> std::expected<void, StarException> {
      List<PhysicsForceRegion> parsed;
      for (auto const& force : forces) {
        auto region = jsonToPhysicsForceRegion(force);
        if (!region) return std::unexpected(std::move(region.error()));
        parsed.push_back(std::move(*region));
      }
      m_physicsForces.set(std::move(parsed));
      return {};
    });
if (!result196) return std::unexpected(std::move(result196.error()));

  auto result197 = callbacks.registerCallback("setName", [this](String const& name) {
      m_name.set(name);
    });
if (!result197) return std::unexpected(std::move(result197.error()));
  auto result198 = callbacks.registerCallback("setDisplayNametag", [this](bool display) {
      m_displayNametag.set(display);
    });
if (!result198) return std::unexpected(std::move(result198.error()));

  auto result200 = callbacks.registerCallback("say", [this](String line, std::optional<StringMap<String>> const& tags) -> std::expected<bool, StarException> {
      if (tags)
        line = line.replace_tags(*tags, false);

      if (!line.empty()) {
        auto result199 = addChatMessage(line);
if (!result199) return std::unexpected(std::move(result199.error()));

        return true;
      }

      return false;
    });
if (!result200) return std::unexpected(std::move(result200.error()));

  auto result202 = callbacks.registerCallback("sayPortrait", [this](String line, String portrait, std::optional<StringMap<String>> const& tags) -> std::expected<bool, StarException> {
      if (tags)
        line = line.replace_tags(*tags, false);

      if (!line.empty()) {
        auto result201 = addChatMessage(line, portrait);
if (!result201) return std::unexpected(std::move(result201.error()));

        return true;
      }

      return false;
    });
if (!result202) return std::unexpected(std::move(result202.error()));

  auto result204 = callbacks.registerCallback("setDamageTeam", [this](Json const& team) -> std::expected<void, StarException> {
      auto result203 = EntityDamageTeam::create(team);
if (!result203) return std::unexpected(std::move(result203.error()));
setTeam(std::move(*result203));
      return {};
});
if (!result204) return std::unexpected(std::move(result204.error()));

auto result205 = callbacks.registerCallback(
  "setUniqueId", [this](std::optional<String> uniqueId) { setUniqueId(std::move(uniqueId)); });
if (!result205) return std::unexpected(std::move(result205.error()));

  auto result207 = callbacks.registerCallback("setDamageBar", [this](String const& damageBarType) -> std::expected<void, StarException> {
      if (!DamageBarTypeNames) return std::unexpected(DamageBarTypeNames.error());
      auto result206 = DamageBarTypeNames->get_left(damageBarType);
      if (!result206)
        return std::unexpected(std::move(result206.error()));
      m_damageBar.set(result206->get());
      return {};
});
if (!result207) return std::unexpected(std::move(result207.error()));

  auto result208 = callbacks.registerCallback("setInteractive", [this](bool interactive) {
      m_interactive.set(interactive);
    });
if (!result208) return std::unexpected(std::move(result208.error()));

  auto result209 = callbacks.registerCallback("setAnimationParameter", [this](String name, Json value) {
      m_scriptedAnimationParameters.set(std::move(name), std::move(value));
    });
if (!result209) return std::unexpected(std::move(result209.error()));

  return callbacks;
}

std::expected<void, StarException> Monster::addChatMessage(String const& message, String const& portrait) {
  m_chatMessage.set(message);
  m_chatPortrait.set(portrait);
  m_newChatMessageEvent.trigger();
  if (portrait.empty())
    {
  auto result210 = mouthPosition();
  if (!result210) return std::unexpected(std::move(result210.error()));
  m_pendingChatActions.push_back(SayChatAction{entityId(), message, std::move(*result210)});
}
  else
    {
  auto result211 = mouthPosition();
  if (!result211) return std::unexpected(std::move(result211.error()));
  m_pendingChatActions.push_back(PortraitChatAction{entityId(), portrait, message, std::move(*result211)});
}
  return {};
}

std::expected<void, StarException> Monster::setupNetStates() {
  auto result212 = m_netGroup.addNetElement(m_uniqueIdNetState);
if (!result212) return std::unexpected(std::move(result212.error()));

  auto result213 = m_netGroup.addNetElement(m_teamNetState);
if (!result213) return std::unexpected(std::move(result213.error()));

  auto result214 = m_netGroup.addNetElement(m_monsterLevelNetState);
if (!result214) return std::unexpected(std::move(result214.error()));

  auto result215 = m_netGroup.addNetElement(m_damageOnTouchNetState);
if (!result215) return std::unexpected(std::move(result215.error()));

  auto result216 = m_netGroup.addNetElement(m_damageSources);
if (!result216) return std::unexpected(std::move(result216.error()));

  auto result217 = m_netGroup.addNetElement(m_aggressiveNetState);
if (!result217) return std::unexpected(std::move(result217.error()));

  auto result218 = m_netGroup.addNetElement(m_knockedOutNetState);
if (!result218) return std::unexpected(std::move(result218.error()));

  auto result219 = m_netGroup.addNetElement(m_deathParticleBurstNetState);
if (!result219) return std::unexpected(std::move(result219.error()));

  auto result220 = m_netGroup.addNetElement(m_deathSoundNetState);
if (!result220) return std::unexpected(std::move(result220.error()));

  auto result221 = m_netGroup.addNetElement(m_activeSkillNameNetState);
if (!result221) return std::unexpected(std::move(result221.error()));

  auto result222 = m_netGroup.addNetElement(m_name);
if (!result222) return std::unexpected(std::move(result222.error()));

  auto result223 = m_netGroup.addNetElement(m_displayNametag);
if (!result223) return std::unexpected(std::move(result223.error()));

  auto result224 = m_netGroup.addNetElement(m_dropPoolNetState);
if (!result224) return std::unexpected(std::move(result224.error()));

  auto result225 = m_netGroup.addNetElement(m_physicsForces);
if (!result225) return std::unexpected(std::move(result225.error()));


  auto result226 = m_netGroup.addNetElement(*m_networkedAnimator);
if (!result226) return std::unexpected(std::move(result226.error()));

  auto result227 = m_netGroup.addNetElement(*m_movementController);
if (!result227) return std::unexpected(std::move(result227.error()));

  auto result228 = m_netGroup.addNetElement(*m_statusController);
if (!result228) return std::unexpected(std::move(result228.error()));

  auto result229 = m_netGroup.addNetElement(*m_effectEmitter);
if (!result229) return std::unexpected(std::move(result229.error()));


  auto result230 = m_netGroup.addNetElement(m_newChatMessageEvent);
if (!result230) return std::unexpected(std::move(result230.error()));

  auto result231 = m_netGroup.addNetElement(m_chatMessage);
if (!result231) return std::unexpected(std::move(result231.error()));

  auto result232 = m_netGroup.addNetElement(m_chatPortrait);
if (!result232) return std::unexpected(std::move(result232.error()));


  auto result233 = m_netGroup.addNetElement(m_damageBar);
if (!result233) return std::unexpected(std::move(result233.error()));

  auto result234 = m_netGroup.addNetElement(m_interactive);
if (!result234) return std::unexpected(std::move(result234.error()));


    // don't interpolate scripted animation parameters or animationdamageparts
  auto result235 = m_netGroup.addNetElement(m_animationDamageParts, false);
if (!result235) return std::unexpected(std::move(result235.error()));

  auto result236 = m_netGroup.addNetElement(m_scriptedAnimationParameters, false);
if (!result236) return std::unexpected(std::move(result236.error()));


  m_netGroup.setNeedsLoadCallback([this](bool initial) { return getNetStates(initial); });
  m_netGroup.setNeedsStoreCallback([this] { return setNetStates(); });
  return {};
}

std::expected<void, StarException> Monster::setNetStates() {
  m_uniqueIdNetState.set(uniqueId());
  m_teamNetState.set(getTeam());
  m_monsterLevelNetState.set(m_monsterLevel);
  m_damageOnTouchNetState.set(m_damageOnTouch);
  m_aggressiveNetState.set(aggressive());
  m_knockedOutNetState.set(m_knockedOut);
  m_deathParticleBurstNetState.set(m_deathParticleBurst);
  m_deathSoundNetState.set(m_deathSound);
  m_activeSkillNameNetState.set(m_activeSkillName);
  m_dropPoolNetState.set(m_dropPool);
  return {};
}

std::expected<void, StarException> Monster::getNetStates(bool initial) {
  setUniqueId(m_uniqueIdNetState.get());
  setTeam(m_teamNetState.get());
  m_monsterLevel = m_monsterLevelNetState.get();
  m_damageOnTouch = m_damageOnTouchNetState.get();
  m_aggressive = m_aggressiveNetState.get();
  m_knockedOut = m_knockedOutNetState.get();
  if (m_deathParticleBurstNetState.pullUpdated())
    m_deathParticleBurst = m_deathParticleBurstNetState.get();
  if (m_deathSoundNetState.pullUpdated())
    m_deathSound = m_deathSoundNetState.get();
  if (m_activeSkillNameNetState.pullUpdated())
    m_activeSkillName = m_activeSkillNameNetState.get();
  if (m_dropPoolNetState.pullUpdated())
    m_dropPool = m_dropPoolNetState.get();

  if (m_newChatMessageEvent.pullOccurred() && !initial) {
    if (m_chatPortrait.get().empty())
      {
  auto result237 = mouthPosition();
  if (!result237) return std::unexpected(std::move(result237.error()));
  m_pendingChatActions.push_back(SayChatAction{entityId(), m_chatMessage.get(), std::move(*result237)});
}
    else
      {
  auto result238 = mouthPosition();
  if (!result238) return std::unexpected(std::move(result238.error()));
  m_pendingChatActions.push_back(
    PortraitChatAction{entityId(), m_chatPortrait.get(), m_chatMessage.get(), std::move(*result238)});
}
  }
  return {};
}

std::expected<float, StarException> Monster::monsterLevel() const {
  if (!m_monsterLevel) return std::unexpected(MonsterException("Monster level is not initialized"));
  return *m_monsterLevel;
}

std::expected<Monster::SkillInfo, StarException> Monster::activeSkillInfo() const {
  SkillInfo skillInfo;

  if (!m_activeSkillName.empty()) {
    auto result239 = this->root().monsterDatabase();
if (!result239) return std::unexpected(std::move(result239.error()));
auto monsterDatabase = std::move(*result239);
    auto result240 = monsterDatabase->skillInfo(m_activeSkillName);
if (!result240) return std::unexpected(std::move(result240.error()));
auto monsterSkillInfo = std::move(*result240);
    skillInfo.label = monsterSkillInfo.first;
    skillInfo.image = monsterSkillInfo.second;
  }

  return skillInfo;
}

std::expected<List<Drawable>, StarException> Monster::portrait(PortraitMode) const {
  if (m_monsterVariant.portraitIcon) {
    auto result241 = this->root().imageMetadataDatabase();
if (!result241) return std::unexpected(std::move(result241.error()));
auto result242 = Drawable::makeImage(*std::move(*result241), *m_monsterVariant.portraitIcon, 1.0f, true, Vec2F());
if (!result242) return std::unexpected(std::move(result242.error()));
return List<Drawable>{std::move(*result242)};
  } else {
    auto result243 = NetworkedAnimator::create(*m_networkedAnimator);
if (!result243) return std::unexpected(std::move(result243.error()));
auto animator = std::move(*result243);
    animator->setFlipped(!m_monsterVariant.reversed);
    auto result244 = animator->drawables();
if (!result244) return std::unexpected(std::move(result244.error()));
auto drawables = std::move(*result244);
    Drawable::scaleAll(drawables, TilePixels);
    return drawables;
  }
}

String Monster::typeName() const {
  return m_monsterVariant.type;
}

MonsterVariant Monster::monsterVariant() const {
  return m_monsterVariant;
}

std::optional<String> Monster::statusText() const {
  return {};
}

bool Monster::displayNametag() const {
  return m_displayNametag.get();
}

std::expected<Vec3B, StarException> Monster::nametagColor() const {
  return m_monsterVariant.nametagColor;
}

std::expected<Vec2F, StarException> Monster::nametagOrigin() const {
  return mouthPosition(false);
}

std::expected<String, StarException> Monster::nametag() const {
  return name();
}

bool Monster::aggressive() const {
  return m_aggressive;
}

std::expected<std::optional<LuaValue>, StarException> Monster::callScript(String const& func, LuaVariadic<LuaValue> const& args) {
  return m_scriptComponent.invoke(func, args);
}

std::expected<std::optional<LuaValue>, StarException> Monster::evalScript(String const& code) {
  return m_scriptComponent.eval(code);
}

std::expected<Vec2F, StarException> Monster::mouthPosition() const {
  auto result245 = mouthOffset();
if (!result245) return std::unexpected(std::move(result245.error()));
return std::move(*result245) + position();
}

std::expected<Vec2F, StarException> Monster::mouthPosition(bool) const {
  return mouthPosition();
}

List<ChatAction> Monster::pullPendingChatActions() {
  return std::move(m_pendingChatActions);
}

std::expected<List<PhysicsForceRegion>, StarException> Monster::forceRegions() const {
  return m_physicsForces.get();
}

std::expected<InteractAction, StarException> Monster::interact(InteractRequest const& request) {
  auto result246 = m_scriptComponent.invoke<Json>("interact", JsonObject{{"sourceId", request.sourceId}, {"sourcePosition", jsonFromVec2F(request.sourcePosition)}});
if (!result246) return std::unexpected(std::move(result246.error()));
auto result = std::move(*result246).value_or(Json{});

  if (result.isNull())
    return {};

  if (result.isType(Json::Type::String))
    {
  auto result247 = result.toString();
  if (!result247) return std::unexpected(std::move(result247.error()));
  return InteractAction::create(std::move(*result247), entityId(), {});
}

  auto result248 = result.getString(0);
if (!result248) return std::unexpected(std::move(result248.error()));
auto result249 = result.get(1);
if (!result249) return std::unexpected(std::move(result249.error()));
return InteractAction::create(std::move(*result248), entityId(), std::move(*result249));
}

bool Monster::isInteractive() const {
  return m_interactive.get();
}

std::expected<Vec2F, StarException> Monster::questIndicatorPosition() const {
  Vec2F pos = position() + m_questIndicatorOffset;
  pos[1] += collisionArea().yMax();
  return pos;
}

ActorMovementController& Monster::movementController() {
  return *m_movementController;
}

StatusController& Monster::statusController() {
  return *m_statusController;
}

}

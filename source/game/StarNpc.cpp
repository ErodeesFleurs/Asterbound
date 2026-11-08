import star.output_proxy;
import star.inventory_types;
#include <assert.h>
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
import star.data_stream;
import star.bi_map;
import star.game_types;
import star.data_stream_extra;
import star.strong_typedef;
import star.rpc_promise;
import star.logging;
import star.weighted_pool;
import star.interpolation;
import star.parametric_function;
import star.string;
import star.byte_array;
import star.encode;
import star.audio;
import star.list;
import star.map;
import star.set;
import star.vector;
import star.color;
import star.poly;
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

import star.npc;
import star.lua_root;
import star.item;
import star.item_database;
import star.item_descriptor;
import star.item_recipe;
import star.animated_part_set;
import star.networked_animator;
import star.humanoid;
import star.portrait_entity;
import star.damage_bar_entity;
import star.nametag_entity;
import star.scripted_entity;
import star.chat_action;
import star.chatty_entity;
import star.emote_entity;
import star.lounging_entities;
import star.lua_actor_movement_component;
import star.lua_animation_component;
import star.npc_database;
import star.effect_emitter;
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
import star.asset_source;
import star.assets;
import star.root_base;
import star.configuration;
import star.root;
import star.item_drop;
import star.mixer;
import star.drawable;
import star.entity_rendering_types;
import star.animation;
import star.particle;

import star.light_source;
import star.entity_rendering;
import star.effect_source_item;
import star.previewable_item;
import star.swingable_item;
import star.armors;
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
import star.status_effect_item;
import star.lua_components;
import star.lua_converters;
import star.lua_game_converters;
import star.fireable_item;
import star.stat_collection;
import star.status_effect_database;
import star.status_controller;
import star.treasure;

import star.species_database;
import star.networked_animator_lua_bindings;
import star.entity_lua_bindings;
import star.songbook_lua_bindings;
import star.behavior_lua_bindings;
import star.scripted_animator_lua_bindings;
import star.config_lua_bindings;
import star.status_controller_lua_bindings;
import star.dance_database;
import star.emote_processor;



import star.songbook;

import star.armor_wearer;
import star.tool_user;
import star.entity_splash;

import star.net_compatibility;
import star.static_random;
import star.math_common;
import star.algorithm;
namespace Star {

std::expected<std::shared_ptr<Npc>, StarException> Npc::create(Root& rootContext, ByteArray const& netStore, NetCompatibilityRules rules) {
  auto database = rootContext.npcDatabase();
  if (!database) return std::unexpected(std::move(database.error()));
  auto variant = (*database)->readNpcVariant(netStore, rules);
  if (!variant) return std::unexpected(std::move(variant.error()));
  return create(rootContext, *variant);
}

Npc::Npc(Root& rootContext) : Entity(rootContext), m_netHumanoid([&rootContext] { return NetHumanoid::create(rootContext); }), m_scriptedAnimator(rootContext), m_scriptComponent(rootContext) {}

std::expected<std::shared_ptr<Npc>, StarException> Npc::create(Root& rootContext, NpcVariant const& npcVariant) {
  auto creature = std::shared_ptr<Npc>(new Npc(rootContext));
  auto initialized = creature->initialize(npcVariant);
  if (!initialized)
    return std::unexpected(std::move(initialized.error()));
  return creature;
}

std::expected<void, StarException> Npc::initialize(NpcVariant const& npcVariant) {

  auto result1 = NetHumanoid::create(this->root(), npcVariant.humanoidIdentity, npcVariant.humanoidParameters, npcVariant.uniqueHumanoidConfig ? npcVariant.humanoidConfig : Json());
if (!result1) return std::unexpected(std::move(result1.error()));
auto result2 = m_netHumanoid.addNetElement(std::move(*result1));
if (!result2) return std::unexpected(std::move(result2.error()));

  m_disableWornArmor.set(npcVariant.disableWornArmor);

  m_emoteState = HumanoidEmote::Idle;
  m_chatMessageUpdated = false;

  m_statusText.set({});
  m_displayNametag.set(false);

  auto result3 = this->root().assets();
if (!result3) return std::unexpected(std::move(result3.error()));
auto assets = std::move(*result3);

  auto result4 = assets->json("/npcs/npc.config:emoteCooldown");
if (!result4) return std::unexpected(std::move(result4.error()));
auto result5 = std::move(*result4).toFloat();
if (!result5) return std::unexpected(std::move(result5.error()));
m_emoteCooldownTimer = GameTimer(std::move(*result5));
  m_danceCooldownTimer = GameTimer(0.0f);
  auto result6 = assets->json("/npcs/npc.config:blinkInterval");
if (!result6) return std::unexpected(std::move(result6.error()));
auto result7 = jsonToVec2F(std::move(*result6));
if (!result7) return std::unexpected(std::move(result7.error()));
m_blinkInterval = std::move(*result7);

  auto result8 = assets->json("/quests/quests.config:defaultIndicatorOffset");
if (!result8) return std::unexpected(std::move(result8.error()));
auto result9 = jsonToVec2F(std::move(*result8));
if (!result9) return std::unexpected(std::move(result9.error()));
m_questIndicatorOffset = std::move(*result9);

  if (npcVariant.overrides)
    {
  auto result10 = npcVariant.overrides.getString("clientEntityMode", "ClientSlaveOnly");
  if (!result10) return std::unexpected(std::move(result10.error()));
  if (!ClientEntityModeNames) return std::unexpected(ClientEntityModeNames.error());
  auto result11 = ClientEntityModeNames->get_left(std::move(*result10));
  if (!result11) return std::unexpected(std::move(result11.error()));
  m_clientEntityMode = result11->get();
}

  m_isInteractive.set(false);

  m_shifting.set(false);
  m_damageOnTouch.set(false);

  m_dropPools.set(npcVariant.dropPools);
  m_npcVariant = npcVariant;

  setTeam(EntityDamageTeam(m_npcVariant.damageTeamType, m_npcVariant.damageTeam));

  auto result12 = m_scriptComponent.setScripts(m_npcVariant.scripts);
if (!result12) return std::unexpected(std::move(result12.error()));

  m_scriptComponent.setUpdateDelta(m_npcVariant.initialScriptDelta);
  auto result13 = humanoid();
if (!result13) return std::unexpected(std::move(result13.error()));
auto result14 = ActorMovementParameters::create(jsonMerge(std::move(*result13)->defaultMovementParameters(), m_npcVariant.movementParameters));
if (!result14) return std::unexpected(std::move(result14.error()));
auto movementParameters = std::move(*result14);
  if (!movementParameters.physicsEffectCategories)
    movementParameters.physicsEffectCategories = StringSet({"npc"});
  auto result15 = ActorMovementController::create(this->root(), movementParameters);
if (!result15) return std::unexpected(std::move(result15.error()));
m_movementController = std::move(*result15);
  m_identityUpdated = false;
  auto result16 = humanoid();
if (!result16) return std::unexpected(std::move(result16.error()));
m_deathParticleBurst.set(std::move(*result16)->defaultDeathParticles());

  auto result17 = StatusController::create(this->root(), m_npcVariant.statusControllerSettings);
if (!result17) return std::unexpected(std::move(result17.error()));
m_statusController = std::move(*result17);
  auto result18 = m_statusController->setPersistentEffects("innate", m_npcVariant.innateStatusEffects);
if (!result18) return std::unexpected(std::move(result18.error()));

  auto result19 = this->root().speciesDatabase();
if (!result19) return std::unexpected(std::move(result19.error()));
auto result21 = (*result19)->species(species());
if (!result21) return std::unexpected(std::move(result21.error()));
auto speciesDefinition = std::move(*result21);
  auto result22 = m_statusController->setPersistentEffects("species", speciesDefinition->statusEffects());
if (!result22) return std::unexpected(std::move(result22.error()));

  m_statusController->setStatusProperty("species", species());
  if (!m_statusController->statusProperty("effectDirectives"))
    m_statusController->setStatusProperty("effectDirectives", speciesDefinition->effectDirectives());

auto result25 = Songbook::create(this->root(), species());
if (!result25) return std::unexpected(std::move(result25.error()));
m_songbook = std::move(*result25);

  auto result26 = EffectEmitter::create(this->root());
if (!result26) return std::unexpected(std::move(result26.error()));
m_effectEmitter = std::move(*result26);

  m_hitDamageNotificationLimiter = 0;
  auto result27 = assets->json("/npcs/npc.config:hitDamageNotificationLimit");
if (!result27) return std::unexpected(std::move(result27.error()));
auto result28 = std::move(*result27).toInt();
if (!result28) return std::unexpected(std::move(result28.error()));
m_hitDamageNotificationLimit = std::move(*result28);

  m_blinkCooldownTimer = GameTimer();

  auto result29 = ArmorWearer::create(this->root());
if (!result29) return std::unexpected(std::move(result29.error()));
m_armor = std::move(*result29);
  auto result30 = ToolUser::create(this->root());
if (!result30) return std::unexpected(std::move(result30.error()));
m_tools = std::move(*result30);

  m_aggressive.set(false);

  setPersistent(m_npcVariant.persistent);
  setKeepAlive(m_npcVariant.keepAlive);

  auto result31 = setupNetStates();
if (!result31) return std::unexpected(std::move(result31.error()));
  return {};
}

std::expected<std::shared_ptr<Npc>, StarException> Npc::create(Root& rootContext, NpcVariant const& npcVariant, Json const& diskStore) {
  auto creature = create(rootContext, npcVariant);
  if (!creature) return std::unexpected(std::move(creature.error()));
  auto loaded = (*creature)->loadState(diskStore);
  if (!loaded) return std::unexpected(std::move(loaded.error()));
  return creature;
}

std::expected<void, StarException> Npc::loadState(Json const& diskStore) {
  auto result32 = diskStore.get("movementController");
if (!result32) return std::unexpected(std::move(result32.error()));
auto result33 = m_movementController->loadState(std::move(*result32));
if (!result33) return std::unexpected(std::move(result33.error()));

  auto result34 = diskStore.get("statusController");
if (!result34) return std::unexpected(std::move(result34.error()));
auto result35 = m_statusController->diskLoad(std::move(*result34));
if (!result35) return std::unexpected(std::move(result35.error()));

  auto result36 = diskStore.get("aimPosition");
if (!result36) return std::unexpected(std::move(result36.error()));
auto result37 = jsonToVec2F(std::move(*result36));
if (!result37) return std::unexpected(std::move(result37.error()));
auto aimPosition = std::move(*result37);
  m_xAimPosition.set(aimPosition[0]);
  m_yAimPosition.set(aimPosition[1]);
  auto result38 = humanoid();
if (!result38) return std::unexpected(std::move(result38.error()));
auto result39 = diskStore.getString("humanoidState");
if (!result39) return std::unexpected(std::move(result39.error()));
if (!Humanoid::StateNames) return std::unexpected(Humanoid::StateNames.error());
auto result40 = Humanoid::StateNames->get_left(std::move(*result39));
if (!result40) return std::unexpected(std::move(result40.error()));
auto result41 = std::move(*result38)->setState(result40->get());
if (!result41) return std::unexpected(std::move(result41.error()));

  auto result42 = humanoid();
if (!result42) return std::unexpected(std::move(result42.error()));
auto result43 = diskStore.getString("humanoidEmoteState");
if (!result43) return std::unexpected(std::move(result43.error()));
if (!HumanoidEmoteNames) return std::unexpected(HumanoidEmoteNames.error());
auto result44 = HumanoidEmoteNames->get_left(std::move(*result43));
if (!result44) return std::unexpected(std::move(result44.error()));
auto result45 = std::move(*result42)->setEmoteState(result44->get());
if (!result45) return std::unexpected(std::move(result45.error()));

  auto result46 = diskStore.getBool("isInteractive");
if (!result46) return std::unexpected(std::move(result46.error()));
m_isInteractive.set(std::move(*result46));
  auto result47 = diskStore.getBool("shifting");
if (!result47) return std::unexpected(std::move(result47.error()));
m_shifting.set(std::move(*result47));
  auto result48 = diskStore.getBool("damageOnTouch", false);
if (!result48) return std::unexpected(std::move(result48.error()));
m_damageOnTouch.set(std::move(*result48));

  auto result49 = diskStore.get("effectEmitter");
if (!result49) return std::unexpected(std::move(result49.error()));
auto result50 = m_effectEmitter->fromJson(std::move(*result49));
if (!result50) return std::unexpected(std::move(result50.error()));


  auto result51 = diskStore.get("armor");
if (!result51) return std::unexpected(std::move(result51.error()));
auto result52 = m_armor->diskLoad(std::move(*result51));
if (!result52) return std::unexpected(std::move(result52.error()));
  auto result53 = diskStore.get("tools");
if (!result53) return std::unexpected(std::move(result53.error()));
auto result54 = m_tools->diskLoad(std::move(*result53));
if (!result54) return std::unexpected(std::move(result54.error()));


  auto result55 = diskStore.getBool("disableWornArmor");
if (!result55) return std::unexpected(std::move(result55.error()));
m_disableWornArmor.set(std::move(*result55));

  auto result56 = diskStore.getObject("scriptStorage");
if (!result56) return std::unexpected(std::move(result56.error()));
auto result57 = m_scriptComponent.setScriptStorage(std::move(*result56));
if (!result57) return std::unexpected(std::move(result57.error()));


  auto result58 = diskStore.optString("uniqueId");
if (!result58) return std::unexpected(std::move(result58.error()));
setUniqueId(std::move(*result58));
  {
  auto result59 = diskStore.contains("team");
  if (!result59) return std::unexpected(std::move(result59.error()));
  if (std::move(*result59))
      {
    auto result60 = diskStore.get("team");
    if (!result60) return std::unexpected(std::move(result60.error()));
    auto result61 = EntityDamageTeam::create(std::move(*result60));
    if (!result61) return std::unexpected(std::move(result61.error()));
    setTeam(std::move(*result61));
  }
}

  auto result62 = diskStore.optString("deathParticleBurst");
if (!result62) return std::unexpected(std::move(result62.error()));
m_deathParticleBurst.set(std::move(*result62));

  auto result63 = diskStore.getArray("dropPools");
if (!result63) return std::unexpected(std::move(result63.error()));
auto result64 = jsonToStringList(std::move(*result63));
if (!result64) return std::unexpected(std::move(result64.error()));
m_dropPools.set(std::move(*result64));

  m_blinkCooldownTimer = GameTimer();

  auto result65 = diskStore.getBool("aggressive");
if (!result65) return std::unexpected(std::move(result65.error()));
m_aggressive.set(std::move(*result65));
  return {};
}

std::expected<Json, StarException> Npc::diskStore() const {
  auto result66 = this->root().npcDatabase();
if (!result66) return std::unexpected(std::move(result66.error()));
auto result67 = std::move(*result66)->writeNpcVariantToJson(m_npcVariant);
if (!result67) return std::unexpected(std::move(result67.error()));
auto result68 = m_movementController->storeState();
if (!result68) return std::unexpected(std::move(result68.error()));
auto result69 = m_statusController->diskStore();
if (!result69) return std::unexpected(std::move(result69.error()));
auto result70 = m_armor->diskStore();
if (!result70) return std::unexpected(std::move(result70.error()));
auto result71 = m_tools->diskStore();
if (!result71) return std::unexpected(std::move(result71.error()));
auto result72 = humanoid();
if (!result72) return std::unexpected(std::move(result72.error()));
if (!Humanoid::StateNames) return std::unexpected(Humanoid::StateNames.error());
auto result73 = Humanoid::StateNames->get_right(std::move(*result72)->state());
if (!result73) return std::unexpected(std::move(result73.error()));
auto result74 = humanoid();
if (!result74) return std::unexpected(std::move(result74.error()));
if (!HumanoidEmoteNames) return std::unexpected(HumanoidEmoteNames.error());
auto result75 = HumanoidEmoteNames->get_right(std::move(*result74)->emoteState());
if (!result75) return std::unexpected(std::move(result75.error()));
auto result76 = m_effectEmitter->toJson();
if (!result76) return std::unexpected(std::move(result76.error()));
auto result77 = m_scriptComponent.getScriptStorage();
if (!result77) return std::unexpected(std::move(result77.error()));
auto result78 = getTeam().toJson();
if (!result78) return std::unexpected(std::move(result78.error()));
return JsonObject{
    {"npcVariant", std::move(*result67)},
    {"movementController", std::move(*result68)},
    {"statusController", std::move(*result69)},
    {"armor", std::move(*result70)},
    {"tools", std::move(*result71)},
    {"aimPosition", jsonFromVec2F({m_xAimPosition.get(), m_yAimPosition.get()})},
    {"humanoidState", result73->get()},
    {"humanoidEmoteState", result75->get()},
    {"isInteractive", m_isInteractive.get()},
    {"shifting", m_shifting.get()},
    {"damageOnTouch", m_damageOnTouch.get()},
    {"effectEmitter", std::move(*result76)},
    {"disableWornArmor", m_disableWornArmor.get()},
    {"scriptStorage", std::move(*result77)},
    {"uniqueId", jsonFromOptional(uniqueId())},
    {"team", std::move(*result78)},
    {"deathParticleBurst", jsonFromOptional(m_deathParticleBurst.get())},
    {"dropPools", m_dropPools.get().transformed(construct<Json>())},
    {"aggressive", m_aggressive.get()}
  };
}

std::expected<ByteArray, StarException> Npc::netStore(NetCompatibilityRules rules) {
  auto result79 = this->root().npcDatabase();
if (!result79) return std::unexpected(std::move(result79.error()));
return std::move(*result79)->writeNpcVariant(m_npcVariant, rules);
}

EntityType Npc::entityType() const {
  return EntityType::Npc;
}

ClientEntityMode Npc::clientEntityMode() const {
  return m_clientEntityMode;
}

std::expected<void, StarException> Npc::init(World& world, EntityId entityId, EntityMode mode) {
  auto result80 = Entity::init(world, entityId, mode);
if (!result80) return std::unexpected(std::move(result80.error()));

  m_movementController->init(world);
  m_movementController->setIgnorePhysicsEntities({entityId});
  auto result81 = m_statusController->init(*this, *m_movementController);
if (!result81) return std::unexpected(std::move(result81.error()));

  auto result82 = m_tools->init(*this);
if (!result82) return std::unexpected(std::move(result82.error()));


  auto result83 = humanoid();
if (!result83) return std::unexpected(std::move(result83.error()));
auto result84 = m_armor->setupHumanoid(*std::move(*result83), forceNude());
if (!result84) return std::unexpected(std::move(result84.error()));


  if (isMaster()) {
    auto result85 = m_movementController->resetAnchorState();
if (!result85) return std::unexpected(std::move(result85.error()));


    auto result86 = this->root().itemDatabase();
if (!result86) return std::unexpected(std::move(result86.error()));
auto itemDatabase = std::move(*result86);
    for (auto const& item : m_npcVariant.items)
      {
  auto result87 = setItemSlot(item.first, item.second);
  if (!result87) return std::unexpected(std::move(result87.error()));

}
    auto result88 = makeNpcCallbacks();
if (!result88) return std::unexpected(std::move(result88.error()));
auto result89 = m_scriptComponent.addCallbacks("npc", std::move(*result88));
if (!result89) return std::unexpected(std::move(result89.error()));

    auto result90 = LuaBindings::makeConfigCallbacks([this](String const& name, Json const& def)
            { return m_npcVariant.scriptConfig.query(name, def); });
if (!result90) return std::unexpected(std::move(result90.error()));
auto result91 = m_scriptComponent.addCallbacks("config",
        std::move(*result90));
if (!result91) return std::unexpected(std::move(result91.error()));

    auto result92 = LuaBindings::makeEntityCallbacks(*this);
if (!result92) return std::unexpected(std::move(result92.error()));
auto result93 = m_scriptComponent.addCallbacks("entity", std::move(*result92));
if (!result93) return std::unexpected(std::move(result93.error()));

    auto result94 = LuaBindings::makeStatusControllerCallbacks(*m_statusController);
if (!result94) return std::unexpected(std::move(result94.error()));
auto result95 = m_scriptComponent.addCallbacks("status", std::move(*result94));
if (!result95) return std::unexpected(std::move(result95.error()));

    auto result96 = LuaBindings::makeBehaviorCallbacks(this->root(), m_behaviors);
if (!result96) return std::unexpected(std::move(result96.error()));
auto result97 = m_scriptComponent.addCallbacks("behavior", std::move(*result96));
if (!result97) return std::unexpected(std::move(result97.error()));

    auto result98 = LuaBindings::makeSongbookCallbacks(*m_songbook);
if (!result98) return std::unexpected(std::move(result98.error()));
auto result99 = m_scriptComponent.addCallbacks("songbook", std::move(*result98));
if (!result99) return std::unexpected(std::move(result99.error()));

    auto result100 = humanoid();
if (!result100) return std::unexpected(std::move(result100.error()));
auto animator = (*result100)->networkedAnimator();
auto result101 = LuaBindings::makeNetworkedAnimatorCallbacks(animator->get());
if (!result101) return std::unexpected(std::move(result101.error()));
auto result102 = m_scriptComponent.addCallbacks("animator", std::move(*result101));
if (!result102) return std::unexpected(std::move(result102.error()));

    auto result103 = m_scriptComponent.addActorMovementCallbacks(*m_movementController);
if (!result103) return std::unexpected(std::move(result103.error()));

    auto result104 = m_scriptComponent.init(world);
if (!result104) return std::unexpected(std::move(result104.error()));

  }
  if (world.isClient()) {
    auto result105 = humanoid();
if (!result105) return std::unexpected(std::move(result105.error()));
auto result106 = m_scriptedAnimator.setScripts(std::move(*result105)->animationScripts());
if (!result106) return std::unexpected(std::move(result106.error()));

    auto result107 = humanoid();
if (!result107) return std::unexpected(std::move(result107.error()));
auto animator = (*result107)->networkedAnimator();
auto result108 = LuaBindings::makeScriptedAnimatorCallbacks(animator->get(),
      [this](String const& name, Json const& defaultValue) -> Json {
        return m_scriptedAnimationParameters.value(name, defaultValue);
      });
if (!result108) return std::unexpected(std::move(result108.error()));
auto result109 = m_scriptedAnimator.addCallbacks("animationConfig", std::move(*result108));
if (!result109) return std::unexpected(std::move(result109.error()));

    auto result110 = LuaBindings::makeConfigCallbacks([this](String const& name, Json const& def)
            { return m_npcVariant.scriptConfig.query(name, def); });
if (!result110) return std::unexpected(std::move(result110.error()));
auto result111 = m_scriptedAnimator.addCallbacks("config",
        std::move(*result110));
if (!result111) return std::unexpected(std::move(result111.error()));

    auto result112 = LuaBindings::makeEntityCallbacks(*this);
if (!result112) return std::unexpected(std::move(result112.error()));
auto result113 = m_scriptedAnimator.addCallbacks("entity", std::move(*result112));
if (!result113) return std::unexpected(std::move(result113.error()));

    auto result114 = m_scriptedAnimator.init(world);
if (!result114) return std::unexpected(std::move(result114.error()));

  }

  if (auto result = updateHumanoidDirectives(); !result)
    return std::unexpected(std::move(result.error()));
  auto pose = humanoid();
  if (!pose)
    return std::unexpected(std::move(pose.error()));
  return (*pose)->prepareAnimatorPose();
}

std::expected<void, StarException> Npc::uninit() {
  if (isMaster()) {
    auto result115 = m_movementController->resetAnchorState();
if (!result115) return std::unexpected(std::move(result115.error()));

    auto result116 = m_scriptComponent.uninit();
if (!result116) return std::unexpected(std::move(result116.error()));

    auto result117 = m_scriptComponent.removeCallbacks("npc");
if (!result117) return std::unexpected(std::move(result117.error()));

    auto result118 = m_scriptComponent.removeCallbacks("config");
if (!result118) return std::unexpected(std::move(result118.error()));

    auto result119 = m_scriptComponent.removeCallbacks("entity");
if (!result119) return std::unexpected(std::move(result119.error()));

    auto result120 = m_scriptComponent.removeCallbacks("status");
if (!result120) return std::unexpected(std::move(result120.error()));

    auto result121 = m_scriptComponent.removeCallbacks("behavior");
if (!result121) return std::unexpected(std::move(result121.error()));

    auto result122 = m_scriptComponent.removeCallbacks("songbook");
if (!result122) return std::unexpected(std::move(result122.error()));

    auto result123 = m_scriptComponent.removeCallbacks("animator");
if (!result123) return std::unexpected(std::move(result123.error()));

    auto result124 = m_scriptComponent.removeActorMovementCallbacks();
if (!result124) return std::unexpected(std::move(result124.error()));

  }
  {
  auto result125 = world();
  if (!result125) return std::unexpected(std::move(result125.error()));
  if (result125->get().isClient()) {
      auto result126 = m_scriptedAnimator.uninit();
  if (!result126) return std::unexpected(std::move(result126.error()));

      auto result127 = m_scriptedAnimator.removeCallbacks("animationConfig");
  if (!result127) return std::unexpected(std::move(result127.error()));

      auto result128 = m_scriptedAnimator.removeCallbacks("config");
  if (!result128) return std::unexpected(std::move(result128.error()));

      auto result129 = m_scriptedAnimator.removeCallbacks("entity");
  if (!result129) return std::unexpected(std::move(result129.error()));

    }
}
  auto result130 = m_tools->uninit();
if (!result130) return std::unexpected(std::move(result130.error()));

  auto result131 = m_statusController->uninit();
if (!result131) return std::unexpected(std::move(result131.error()));

  m_movementController->uninit();
  auto result132 = Entity::uninit();
if (!result132) return std::unexpected(std::move(result132.error()));

  return {};
}

std::expected<void, StarException> Npc::enableInterpolation(float extrapolationHint) {
  auto result133 = m_netGroup.enableNetInterpolation(extrapolationHint);
if (!result133) return std::unexpected(std::move(result133.error()));

  return {};
}

std::expected<void, StarException> Npc::disableInterpolation() {
  auto result134 = m_netGroup.disableNetInterpolation();
if (!result134) return std::unexpected(std::move(result134.error()));

  return {};
}

Vec2F Npc::position() const {
  return m_movementController->position();
}

RectF Npc::metaBoundBox() const {
  return RectF(-4, -4, 4, 4);
}

std::expected<Vec2F, StarException> Npc::mouthOffset(bool ignoreAdjustments) const {
  auto result135 = humanoid();
if (!result135) return std::unexpected(std::move(result135.error()));
auto result136 = std::move(*result135)->mouthOffset(ignoreAdjustments);
if (!result136) return std::unexpected(std::move(result136.error()));
auto result137 = humanoid();
if (!result137) return std::unexpected(std::move(result137.error()));
auto result138 = humanoid();
if (!result138) return std::unexpected(std::move(result138.error()));
auto result139 = std::move(*result138)->mouthOffset(ignoreAdjustments);
if (!result139) return std::unexpected(std::move(result139.error()));
return Vec2F{std::move(*result136)[0] * numericalDirection(std::move(*result137)->facingDirection()),
      std::move(*result139)[1]};
}

std::expected<Vec2F, StarException> Npc::feetOffset() const {
  auto result140 = humanoid();
if (!result140) return std::unexpected(std::move(result140.error()));
auto result141 = std::move(*result140)->feetOffset();
if (!result141) return std::unexpected(std::move(result141.error()));
auto result142 = humanoid();
if (!result142) return std::unexpected(std::move(result142.error()));
auto result143 = humanoid();
if (!result143) return std::unexpected(std::move(result143.error()));
auto result144 = std::move(*result143)->feetOffset();
if (!result144) return std::unexpected(std::move(result144.error()));
return Vec2F{std::move(*result141)[0] * numericalDirection(std::move(*result142)->facingDirection()), std::move(*result144)[1]};
}

std::expected<Vec2F, StarException> Npc::headArmorOffset() const {
  auto result145 = humanoid();
if (!result145) return std::unexpected(std::move(result145.error()));
auto result146 = std::move(*result145)->headArmorOffset();
if (!result146) return std::unexpected(std::move(result146.error()));
auto result147 = humanoid();
if (!result147) return std::unexpected(std::move(result147.error()));
auto result148 = humanoid();
if (!result148) return std::unexpected(std::move(result148.error()));
auto result149 = std::move(*result148)->headArmorOffset();
if (!result149) return std::unexpected(std::move(result149.error()));
return Vec2F{std::move(*result146)[0] * numericalDirection(std::move(*result147)->facingDirection()), std::move(*result149)[1]};
}

std::expected<Vec2F, StarException> Npc::chestArmorOffset() const {
  auto result150 = humanoid();
if (!result150) return std::unexpected(std::move(result150.error()));
auto result151 = std::move(*result150)->chestArmorOffset();
if (!result151) return std::unexpected(std::move(result151.error()));
auto result152 = humanoid();
if (!result152) return std::unexpected(std::move(result152.error()));
auto result153 = humanoid();
if (!result153) return std::unexpected(std::move(result153.error()));
auto result154 = std::move(*result153)->chestArmorOffset();
if (!result154) return std::unexpected(std::move(result154.error()));
return Vec2F{std::move(*result151)[0] * numericalDirection(std::move(*result152)->facingDirection()), std::move(*result154)[1]};
}

std::expected<Vec2F, StarException> Npc::backArmorOffset() const {
  auto result155 = humanoid();
if (!result155) return std::unexpected(std::move(result155.error()));
auto result156 = std::move(*result155)->backArmorOffset();
if (!result156) return std::unexpected(std::move(result156.error()));
auto result157 = humanoid();
if (!result157) return std::unexpected(std::move(result157.error()));
auto result158 = humanoid();
if (!result158) return std::unexpected(std::move(result158.error()));
auto result159 = std::move(*result158)->backArmorOffset();
if (!result159) return std::unexpected(std::move(result159.error()));
return Vec2F{std::move(*result156)[0] * numericalDirection(std::move(*result157)->facingDirection()), std::move(*result159)[1]};
}

std::expected<Vec2F, StarException> Npc::legsArmorOffset() const {
  auto result160 = humanoid();
if (!result160) return std::unexpected(std::move(result160.error()));
auto result161 = std::move(*result160)->legsArmorOffset();
if (!result161) return std::unexpected(std::move(result161.error()));
auto result162 = humanoid();
if (!result162) return std::unexpected(std::move(result162.error()));
auto result163 = humanoid();
if (!result163) return std::unexpected(std::move(result163.error()));
auto result164 = std::move(*result163)->legsArmorOffset();
if (!result164) return std::unexpected(std::move(result164.error()));
return Vec2F{std::move(*result161)[0] * numericalDirection(std::move(*result162)->facingDirection()), std::move(*result164)[1]};
}

RectF Npc::collisionArea() const {
  return m_movementController->collisionPoly().boundBox();
}

std::expected<std::pair<ByteArray, std::uint64_t>, StarException> Npc::writeNetState(std::uint64_t fromVersion, NetCompatibilityRules rules) {
  // client-side npcs error nearby vanilla NPC scripts because callScriptedEntity
  // for now, scrungle the collision poly to avoid their queries. hacky :(
  {
  bool condition166 = static_cast<bool>(m_npcVariant.overrides);
  if (condition166) {
    auto result165 = m_npcVariant.overrides.getBool("overrideNetPoly", false);
    if (!result165) return std::unexpected(std::move(result165.error()));
    condition166 = static_cast<bool>(std::move(*result165));
  }
  if (condition166) {
      if (auto mode = entityMode()) {
        if (*mode == EntityMode::Master && connectionForEntity(entityId()) != ServerConnectionId) {
          PolyF poly = m_movementController->collisionPoly();
          m_movementController->setCollisionPoly({ { 0.0f, -3.402823466e+38F }});
          auto result = m_netGroup.writeNetState(fromVersion, rules);
          m_movementController->setCollisionPoly(poly);
          return result;
        }
      }
    }
}

  return m_netGroup.writeNetState(fromVersion, rules);
}

std::expected<void, StarException> Npc::readNetState(ByteArray data, float interpolationTime, NetCompatibilityRules rules) {
  auto result167 = m_netGroup.readNetState(data, interpolationTime, rules);
if (!result167) return std::unexpected(std::move(result167.error()));

  return {};
}

std::expected<String, StarException> Npc::description() const {
  return m_npcVariant.description.value_or("Some funny looking person");
}

String Npc::species() const {
  return m_npcVariant.humanoidIdentity.species;
}

Gender Npc::gender() const {
  return m_npcVariant.humanoidIdentity.gender;
}

String Npc::npcType() const {
  return m_npcVariant.typeName;
}

std::expected<Json, StarException> Npc::scriptConfigParameter(String const& parameterName, Json const& defaultValue) const {
  return m_npcVariant.scriptConfig.query(parameterName, defaultValue);
}

std::expected<std::optional<HitType>, StarException> Npc::queryHit(DamageSource const& source) const {
  if (!inWorld() || !m_statusController->resourcePositive("health") || m_statusController->statPositive("invulnerable"))
    return {};

  {
  auto result168 = m_tools->queryShieldHit(source);
  if (!result168) return std::unexpected(std::move(result168.error()));
  if (std::move(*result168))
      return HitType::ShieldHit;
}

  {
  auto result169 = world();
  if (!result169) return std::unexpected(std::move(result169.error()));
  auto result170 = source.intersectsWithPoly(result169->get().geometry(), m_movementController->collisionBody());
  if (!result170) return std::unexpected(std::move(result170.error()));
  if (std::move(*result170))
      return HitType::Hit;
}

  return {};
}

std::expected<std::optional<PolyF>, StarException> Npc::hitPoly() const {
  return m_movementController->collisionBody();
}

std::expected<List<DamageNotification>, StarException> Npc::applyDamage(DamageRequest const& damage) {
  if (!inWorld())
    return {};

  auto result171 = m_statusController->applyDamageRequest(damage);
if (!result171) return std::unexpected(std::move(result171.error()));
auto notifications = std::move(*result171);

  float totalDamage = 0.0f;
  for (auto const& notification : notifications)
    totalDamage += notification.healthLost;

  if (totalDamage > 0 && m_hitDamageNotificationLimiter < m_hitDamageNotificationLimit) {
    auto result172 = m_scriptComponent.invoke("damage", JsonObject{
        {"sourceId", damage.sourceEntityId},
        {"damage", totalDamage},
        {"sourceDamage", damage.damage},
        {"sourceKind", damage.damageSourceKind}
      });
if (!result172) return std::unexpected(std::move(result172.error()));

    m_hitDamageNotificationLimiter++;
  }

  return notifications;
}

std::expected<List<DamageNotification>, StarException> Npc::selfDamageNotifications() {
  return m_statusController->pullSelfDamageNotifications();
}

std::expected<bool, StarException> Npc::shouldDestroy() const {
  auto result = m_scriptComponent.invoke<bool>("shouldDie");
  if (!result)
    return std::unexpected(std::move(result.error()));
  if (*result)
    return **result;
  return !m_statusController->resourcePositive("health") || m_scriptComponent.error();
}

std::expected<void, StarException> Npc::destroy(std::optional<std::reference_wrapper<RenderCallback>> renderCallback) {
  auto result174 = m_scriptComponent.invoke("die");
if (!result174) return std::unexpected(std::move(result174.error()));


  if (isMaster() && !m_dropPools.get().empty()) {
    auto result175 = this->root().treasureDatabase();
if (!result175) return std::unexpected(std::move(result175.error()));
auto treasureDatabase = std::move(*result175);
    auto dropped = [&]() -> std::expected<void, StarException> {
      {
        auto result176 = static_random_from(m_dropPools.get(), m_npcVariant.seed);
        if (!result176)
          return std::unexpected(std::move(result176.error()));
        auto result177 = treasureDatabase->createTreasure(result176->get(), m_npcVariant.level);
        if (!result177)
          return std::unexpected(std::move(result177.error()));
        for (auto const& treasureItem : std::move(*result177)) {
          auto result178 = world();
          if (!result178)
            return std::unexpected(std::move(result178.error()));
          auto result179 = ItemDrop::createRandomizedDrop(this->root(), treasureItem, position());
          if (!result179)
            return std::unexpected(std::move(result179.error()));
          auto result180 = result178->get().addEntity(std::move(*result179));
          if (!result180)
            return std::unexpected(std::move(result180.error()));
        }
}
      return {};
    }();
    if (!dropped) {
      auto const& e = dropped.error();
      auto result181 = this->root().logger().warnException(strf("Failed to create treasure for NPC '{}'", npcType()), ExceptionDiagnostic(e));
if (!result181) return std::unexpected(std::move(result181.error()));
    
    }
  }

  if (renderCallback && m_deathParticleBurst.get())
    {
  auto result182 = humanoid();
  if (!result182) return std::unexpected(std::move(result182.error()));
  auto result183 = std::move(*result182)->particles(*m_deathParticleBurst.get());
  if (!result183) return std::unexpected(std::move(result183.error()));
  auto result184 = renderCallback->get().addParticles(std::move(*result183), position());
  if (!result184) return std::unexpected(std::move(result184.error()));
}

  m_songbook->stop();
  return {};
}

std::expected<void, StarException> Npc::damagedOther(DamageNotification const& damage) {
  if (inWorld() && isMaster())
    m_statusController->damagedOther(damage);
  return {};
}

std::expected<void, StarException> Npc::update(float dt, std::uint64_t) {
  if (!inWorld())
    return {};

  m_movementController->setTimestep(dt);

  if (isMaster()) {
    auto result185 = m_scriptComponent.update(m_scriptComponent.updateDt(dt));
if (!result185) return std::unexpected(std::move(result185.error()));


    {
  auto result186 = inConflictingLoungeAnchor();
  if (!result186) return std::unexpected(std::move(result186.error()));
  if (std::move(*result186))
        {
    auto result187 = m_movementController->resetAnchorState();
    if (!result187) return std::unexpected(std::move(result187.error()));

  }
}

    auto anchor = m_movementController->entityAnchor();
    if (auto loungeAnchor = anchor ? asRef<LoungeAnchor>(*anchor) : std::nullopt) {
      if (loungeAnchor->get().emote)
        {
  auto result188 = requestEmote(*loungeAnchor->get().emote);
  if (!result188) return std::unexpected(std::move(result188.error()));

}
      if (loungeAnchor->get().dance)
        {
  auto result189 = setDance(loungeAnchor->get().dance);
  if (!result189) return std::unexpected(std::move(result189.error()));

}
      auto result190 = m_statusController->setPersistentEffects("lounging", loungeAnchor->get().statusEffects);
if (!result190) return std::unexpected(std::move(result190.error()));

      m_effectEmitter->addEffectSources("normal", loungeAnchor->get().effectEmitters);
      switch (loungeAnchor->get().orientation) {
        case LoungeOrientation::Sit:
          {
  auto result191 = humanoid();
  if (!result191) return std::unexpected(std::move(result191.error()));
  auto result192 = std::move(*result191)->setState(Humanoid::Sit);
  if (!result192) return std::unexpected(std::move(result192.error()));

}
          break;
        case LoungeOrientation::Lay:
          {
  auto result193 = humanoid();
  if (!result193) return std::unexpected(std::move(result193.error()));
  auto result194 = std::move(*result193)->setState(Humanoid::Lay);
  if (!result194) return std::unexpected(std::move(result194.error()));

}
          break;
        case LoungeOrientation::Stand:
          {
  auto result195 = humanoid();
  if (!result195) return std::unexpected(std::move(result195.error()));
  auto result196 = std::move(*result195)->setState(Humanoid::Idle);
  if (!result196) return std::unexpected(std::move(result196.error()));

} // currently the same as "standard"
          // idle, but this is lounging idle
          break;
        default:
          {
  auto result197 = humanoid();
  if (!result197) return std::unexpected(std::move(result197.error()));
  auto result198 = std::move(*result197)->setState(Humanoid::Idle);
  if (!result198) return std::unexpected(std::move(result198.error()));

}
      }
    } else {
      auto result199 = m_statusController->setPersistentEffects("lounging", {});
if (!result199) return std::unexpected(std::move(result199.error()));

    }

    auto result200 = m_armor->effects(*m_effectEmitter);
if (!result200) return std::unexpected(std::move(result200.error()));

    auto result201 = m_tools->effects(*m_effectEmitter);
if (!result201) return std::unexpected(std::move(result201.error()));


    if (auto result = synchronizeEquipmentStatusEffects(*m_armor, *m_tools, *m_statusController, m_disableWornArmor.get()); !result)
      return std::unexpected(std::move(result.error()));


    auto result206 = m_movementController->tickMaster(dt);
if (!result206) return std::unexpected(std::move(result206.error()));

    auto result207 = m_statusController->tickMaster(dt);
if (!result207) return std::unexpected(std::move(result207.error()));


    auto result208 = tickShared(dt);
if (!result208) return std::unexpected(std::move(result208.error()));


    if (!is<LoungeAnchor>(m_movementController->entityAnchor())) {
      if (m_movementController->groundMovement()) {
        if (m_movementController->running())
          {
  auto result209 = humanoid();
  if (!result209) return std::unexpected(std::move(result209.error()));
  auto result210 = std::move(*result209)->setState(Humanoid::Run);
  if (!result210) return std::unexpected(std::move(result210.error()));

}
        else if (m_movementController->walking())
          {
  auto result211 = humanoid();
  if (!result211) return std::unexpected(std::move(result211.error()));
  auto result212 = std::move(*result211)->setState(Humanoid::Walk);
  if (!result212) return std::unexpected(std::move(result212.error()));

}
        else if (m_movementController->crouching())
          {
  auto result213 = humanoid();
  if (!result213) return std::unexpected(std::move(result213.error()));
  auto result214 = std::move(*result213)->setState(Humanoid::Duck);
  if (!result214) return std::unexpected(std::move(result214.error()));

}
        else
          {
  auto result215 = humanoid();
  if (!result215) return std::unexpected(std::move(result215.error()));
  auto result216 = std::move(*result215)->setState(Humanoid::Idle);
  if (!result216) return std::unexpected(std::move(result216.error()));

}
      } else if (m_movementController->liquidMovement()) {
        if (std::abs(m_movementController->xVelocity()) > 0)
          {
  auto result217 = humanoid();
  if (!result217) return std::unexpected(std::move(result217.error()));
  auto result218 = std::move(*result217)->setState(Humanoid::Swim);
  if (!result218) return std::unexpected(std::move(result218.error()));

}
        else
          {
  auto result219 = humanoid();
  if (!result219) return std::unexpected(std::move(result219.error()));
  auto result220 = std::move(*result219)->setState(Humanoid::SwimIdle);
  if (!result220) return std::unexpected(std::move(result220.error()));

}
      } else {
        if (m_movementController->yVelocity() > 0)
          {
  auto result221 = humanoid();
  if (!result221) return std::unexpected(std::move(result221.error()));
  auto result222 = std::move(*result221)->setState(Humanoid::Jump);
  if (!result222) return std::unexpected(std::move(result222.error()));

}
        else
          {
  auto result223 = humanoid();
  if (!result223) return std::unexpected(std::move(result223.error()));
  auto result224 = std::move(*result223)->setState(Humanoid::Fall);
  if (!result224) return std::unexpected(std::move(result224.error()));

}
      }
    }

    if (m_emoteCooldownTimer.tick(dt))
      m_emoteState = HumanoidEmote::Idle;
    if (m_danceCooldownTimer.tick(dt))
      m_dance = {};

    if (m_chatMessageUpdated) {
      auto result225 = this->root().emoteProcessor();
if (!result225) return std::unexpected(std::move(result225.error()));
auto result226 = std::move(*result225)->detectEmotes(m_chatMessage.get());
if (!result226) return std::unexpected(std::move(result226.error()));
auto state = std::move(*result226);
      if (state != HumanoidEmote::Idle)
        addEmote(state);
      m_chatMessageUpdated = false;
    }

    if (m_blinkCooldownTimer.tick(dt)) {
      auto result227 = this->root().random().randf(m_blinkInterval[0], m_blinkInterval[1]);
if (!result227) return std::unexpected(std::move(result227.error()));
m_blinkCooldownTimer = GameTimer(std::move(*result227));
      if (m_emoteState == HumanoidEmote::Idle)
        addEmote(HumanoidEmote::Blink);
    }

    auto result228 = humanoid();
if (!result228) return std::unexpected(std::move(result228.error()));
auto result229 = std::move(*result228)->setEmoteState(m_emoteState);
if (!result229) return std::unexpected(std::move(result229.error()));

    auto result230 = humanoid();
if (!result230) return std::unexpected(std::move(result230.error()));
auto result231 = std::move(*result230)->setDance(m_dance);
if (!result231) return std::unexpected(std::move(result231.error()));


  } else {
    auto result232 = m_netGroup.tickNetInterpolation(dt);
if (!result232) return std::unexpected(std::move(result232.error()));

    auto result233 = m_movementController->tickSlave(dt);
if (!result233) return std::unexpected(std::move(result233.error()));

    auto result234 = m_statusController->tickSlave(dt);
if (!result234) return std::unexpected(std::move(result234.error()));


    auto result235 = tickShared(dt);
if (!result235) return std::unexpected(std::move(result235.error()));

  }

  {
  auto result236 = world();
  if (!result236) return std::unexpected(std::move(result236.error()));
  if (result236->get().isClient())
      this->root().spatialLogger().logPoly("world", m_movementController->collisionBody(), {0, 255, 0, 255});
}
  return {};
}

std::uint64_t Npc::presentationEpoch() const {
  return Entity::presentationEpoch() + m_movementController->presentationEpoch();
}

std::expected<void, StarException> Npc::collectRenderEvents(RenderCallback& renderCallback) {
  auto result246 = renderCallback.addParticles(m_humanoidDynamicTarget.pullNewParticles());
if (!result246) return std::unexpected(std::move(result246.error()));
  auto result247 = renderCallback.addAudios(m_humanoidDynamicTarget.pullNewAudios());
if (!result247) return std::unexpected(std::move(result247.error()));
  auto result250 = renderCallback.addParticles(m_statusController->pullNewParticles());
if (!result250) return std::unexpected(std::move(result250.error()));
  auto result251 = renderCallback.addAudios(m_statusController->pullNewAudios());
if (!result251) return std::unexpected(std::move(result251.error()));

  auto result252 = world();
if (!result252) return std::unexpected(std::move(result252.error()));
auto result253 = m_npcVariant.splashConfig.doSplash(position(), m_movementController->velocity(), result252->get());
if (!result253) return std::unexpected(std::move(result253.error()));
auto result254 = renderCallback.addParticles(std::move(*result253));
if (!result254) return std::unexpected(std::move(result254.error()));
  auto result261 = m_effectEmitter->collectRenderEvents(renderCallback);
if (!result261) return std::unexpected(std::move(result261.error()));

  auto result262 = m_songbook->collectRenderEvents(renderCallback);
if (!result262) return std::unexpected(std::move(result262.error()));
  return m_tools->collectRenderEvents(renderCallback);
}

std::expected<void, StarException> Npc::updateHumanoidDirectives() {
  DirectivesGroup directives;
  Vec2F scale = Vec2F::filled(1.f);
  for (auto const& entry : m_statusController->parentDirectives().list()) {
    auto extracted = Humanoid::extractScaleFromDirectives(entry);
    if (!extracted)
      return std::unexpected(std::move(extracted.error()));
    scale = scale.piecewiseMultiply(extracted->first);
    directives.append(extracted->second);
  }
  auto pose = humanoid();
  if (!pose)
    return std::unexpected(std::move(pose.error()));
  (*pose)->setScale(scale);
  m_presentationHumanoidDirectives = std::move(directives);
  return {};
}

std::expected<void, StarException> Npc::render(RenderCallback& renderCallback) {
  EntityRenderLayer renderLayer = RenderLayerNpc;
  auto anchor = m_movementController->entityAnchor();
  if (auto loungeAnchor = anchor ? asRef<LoungeAnchor>(*anchor) : std::nullopt)
    renderLayer = loungeAnchor->get().loungeRenderLayer;

  auto result237 = humanoid();
if (!result237) return std::unexpected(std::move(result237.error()));
auto result238 = m_tools->setupHumanoidHandItemDrawables(*std::move(*result237), renderCallback.presentation().interpolation());
if (!result238) return std::unexpected(std::move(result238.error()));


  auto const& humanoidDirectives = m_presentationHumanoidDirectives;

  {
  auto result241 = humanoid();
  if (!result241) return std::unexpected(std::move(result241.error()));
  auto result242 = std::move(*result241)->render(true, true, renderCallback.presentation().interpolation());
  if (!result242) return std::unexpected(std::move(result242.error()));
  for (auto& drawable : std::move(*result242)) {
      drawable.translate(position());
      if (drawable.isImage())
        {
    auto result243 = this->root().imageMetadataDatabase();
    if (!result243) return std::unexpected(std::move(result243.error()));
    auto result244 = drawable.imagePart().addDirectivesGroup(*std::move(*result243), humanoidDirectives, true);
    if (!result244) return std::unexpected(std::move(result244.error()));

  }
      auto result245 = renderCallback.addDrawable(std::move(drawable), renderLayer);
  if (!result245) return std::unexpected(std::move(result245.error()));
    }
}


  auto result248 = m_statusController->drawables(renderCallback.presentation().interpolation());
if (!result248) return std::unexpected(std::move(result248.error()));
auto result249 = renderCallback.addDrawables(std::move(*result248), renderLayer);
if (!result249) return std::unexpected(std::move(result249.error()));

  auto result255 = inToolRange();
if (!result255) return std::unexpected(std::move(result255.error()));
auto result256 = m_tools->render(renderCallback, std::move(*result255), m_shifting.get(), renderLayer);
if (!result256) return std::unexpected(std::move(result256.error()));


  auto result257 = aimPosition();
if (!result257) return std::unexpected(std::move(result257.error()));
auto result258 = inToolRange();
if (!result258) return std::unexpected(std::move(result258.error()));
auto result259 = m_tools->renderObjectPreviews(std::move(*result257), walkingDirection(), std::move(*result258), favoriteColor());
if (!result259) return std::unexpected(std::move(result259.error()));
auto result260 = renderCallback.addDrawables(std::move(*result259), renderLayer);
if (!result260) return std::unexpected(std::move(result260.error()));


  return {};
}

std::expected<void, StarException> Npc::renderLightSources(RenderCallback& renderCallback) {
  auto result263 = presentationLightSources(renderCallback.presentation().interpolation());
if (!result263) return std::unexpected(std::move(result263.error()));
auto result264 = renderCallback.addLightSources(std::move(*result263));
if (!result264) return std::unexpected(std::move(result264.error()));
  return {};
}

void Npc::setPosition(Vec2F const& pos) {
  resetPresentation();
  m_movementController->setPosition(pos);
}

std::expected<float, StarException> Npc::maxHealth() const {
  auto maximum = m_statusController->resourceMax("health");
  if (!maximum) return std::unexpected(std::move(maximum.error()));
  if (!*maximum) return std::unexpected(NpcException("Health resource has no maximum"));
  return **maximum;
}

float Npc::health() const {
  return m_statusController->resource("health");
}

DamageBarType Npc::damageBar() const {
  return DamageBarType::Default;
}

std::expected<List<Drawable>, StarException> Npc::portrait(PortraitMode mode) const {
  auto result265 = humanoid();
if (!result265) return std::unexpected(std::move(result265.error()));
return std::move(*result265)->renderPortrait(mode);
}

std::expected<String, StarException> Npc::name() const {
  return m_npcVariant.humanoidIdentity.name;
}

std::optional<String> Npc::statusText() const {
  return m_statusText.get();
}

bool Npc::displayNametag() const {
  return m_displayNametag.get();
}

std::expected<Vec3B, StarException> Npc::nametagColor() const {
  return m_npcVariant.nametagColor;
}

std::expected<Vec2F, StarException> Npc::nametagOrigin() const {
  return mouthPosition(false);
}

std::expected<String, StarException> Npc::nametag() const {
  return name();
}

bool Npc::aggressive() const {
  return m_aggressive.get();
}

std::expected<std::optional<LuaValue>, StarException> Npc::callScript(String const& func, LuaVariadic<LuaValue> const& args) {
  return m_scriptComponent.invoke(func, args);
}

std::expected<std::optional<LuaValue>, StarException> Npc::evalScript(String const& code) {
  return m_scriptComponent.eval(code);
}

std::expected<Vec2F, StarException> Npc::getAbsolutePosition(Vec2F relativePosition) const {
  {
  auto result266 = humanoid();
  if (!result266) return std::unexpected(std::move(result266.error()));
  if (std::move(*result266)->facingDirection() == Direction::Left)
      relativePosition[0] *= -1;
}
  return m_movementController->position() + relativePosition;
}

std::expected<void, StarException> Npc::tickShared(float dt) {
  if (m_hitDamageNotificationLimiter)
    m_hitDamageNotificationLimiter--;

  auto result267 = world();
if (!result267) return std::unexpected(std::move(result267.error()));
auto result268 = m_songbook->update(*entityMode(), result267->get());
if (!result268) return std::unexpected(std::move(result268.error()));


  m_effectEmitter->setSourcePosition("normal", position());
  auto result269 = mouthOffset(true);
if (!result269) return std::unexpected(std::move(result269.error()));
m_effectEmitter->setSourcePosition("mouth", position() + std::move(*result269));
  auto result270 = feetOffset();
if (!result270) return std::unexpected(std::move(result270.error()));
m_effectEmitter->setSourcePosition("feet", position() + std::move(*result270));
  auto result271 = headArmorOffset();
if (!result271) return std::unexpected(std::move(result271.error()));
m_effectEmitter->setSourcePosition("headArmor", std::move(*result271) + position());
  auto result272 = chestArmorOffset();
if (!result272) return std::unexpected(std::move(result272.error()));
m_effectEmitter->setSourcePosition("chestArmor", std::move(*result272) + position());
  auto result273 = legsArmorOffset();
if (!result273) return std::unexpected(std::move(result273.error()));
m_effectEmitter->setSourcePosition("legsArmor", std::move(*result273) + position());
  auto result274 = backArmorOffset();
if (!result274) return std::unexpected(std::move(result274.error()));
m_effectEmitter->setSourcePosition("backArmor", std::move(*result274) + position());

  auto result275 = humanoid();
if (!result275) return std::unexpected(std::move(result275.error()));
m_effectEmitter->setDirection(std::move(*result275)->facingDirection());
  auto result276 = m_effectEmitter->tick(dt, *entityMode());
if (!result276) return std::unexpected(std::move(result276.error()));


  auto result277 = humanoid();
if (!result277) return std::unexpected(std::move(result277.error()));
auto result278 = std::move(*result277)->setMovingBackwards(m_movementController->movingDirection() != m_movementController->facingDirection());
if (!result278) return std::unexpected(std::move(result278.error()));

  auto result279 = humanoid();
if (!result279) return std::unexpected(std::move(result279.error()));
std::move(*result279)->setFacingDirection(m_movementController->facingDirection());
  auto result280 = humanoid();
if (!result280) return std::unexpected(std::move(result280.error()));
std::move(*result280)->setRotation(m_movementController->rotation());

  ActorMovementModifiers firingModifiers;
  auto mainItem = handItem(ToolHand::Primary);
  if (auto fireableMain = mainItem ? asRef<FireableItem>(*mainItem) : std::nullopt) {
    if (fireableMain->get().firing()) {
      if (fireableMain->get().stopWhileFiring())
        firingModifiers.movementSuppressed = true;
      else if (fireableMain->get().walkWhileFiring())
        firingModifiers.runningSuppressed = true;
    }
  }

  auto altItem = handItem(ToolHand::Alt);
  if (auto fireableAlt = altItem ? asRef<FireableItem>(*altItem) : std::nullopt) {
    if (fireableAlt->get().firing()) {
      if (fireableAlt->get().stopWhileFiring())
        firingModifiers.movementSuppressed = true;
      else if (fireableAlt->get().walkWhileFiring())
        firingModifiers.runningSuppressed = true;
    }
  }

  auto result281 = humanoid();
if (!result281) return std::unexpected(std::move(result281.error()));
auto result282 = m_armor->setupHumanoid(*std::move(*result281), forceNude());
if (!result282) return std::unexpected(std::move(result282.error()));


  auto result283 = canUseTool();
if (!result283) return std::unexpected(std::move(result283.error()));
m_tools->suppressItems(!std::move(*result283));
  auto result284 = m_tools->tick(dt, m_shifting.get(), {});
if (!result284) return std::unexpected(std::move(result284.error()));


  {
  auto result285 = humanoid();
  if (!result285) return std::unexpected(std::move(result285.error()));
  auto result286 = aimPosition();
  if (!result286) return std::unexpected(std::move(result286.error()));
  auto result287 = m_tools->setupHumanoidHandItems(*std::move(*result285), position(), std::move(*result286));
  if (!result287) return std::unexpected(std::move(result287.error()));
  if (auto overrideDirection = std::move(*result287))
      m_movementController->controlFace(*overrideDirection);
}

  if (auto result = updateHumanoidDirectives(); !result)
    return std::unexpected(std::move(result.error()));
  {
  auto result288 = world();
  if (!result288) return std::unexpected(std::move(result288.error()));
  if (result288->get().isClient()) {
      auto result289 = humanoid();
  if (!result289) return std::unexpected(std::move(result289.error()));
  auto result290 = std::move(*result289)->animate(dt, std::ref(m_humanoidDynamicTarget));
  if (!result290) return std::unexpected(std::move(result290.error()));

      m_humanoidDynamicTarget.updatePosition(position());
    } else {
      auto result291 = humanoid();
  if (!result291) return std::unexpected(std::move(result291.error()));
  auto result292 = std::move(*result291)->animate(dt, {});
  if (!result292) return std::unexpected(std::move(result292.error()));

    }
}
  auto result293 = m_scriptedAnimator.update();
if (!result293) return std::unexpected(std::move(result293.error()));

  return {};
}

std::expected<LuaCallbacks, StarException> Npc::makeNpcCallbacks() {
  LuaCallbacks callbacks;

  auto result294 = callbacks.registerCallback("toAbsolutePosition", [this](Vec2F const& p) { return getAbsolutePosition(p); });
if (!result294) return std::unexpected(std::move(result294.error()));

  auto result295 = callbacks.registerCallback("species", [this]() { return m_npcVariant.species; });
if (!result295) return std::unexpected(std::move(result295.error()));

auto result298 = callbacks.registerCallback("gender", [this]() -> std::expected<String, StarException> {
  auto result296 = humanoid();
  if (!result296)
    return std::unexpected(std::move(result296.error()));
  auto result297 = enumName<GenderNames>(std::move(*result296)->identity().gender);
  if (!result297)
    return std::unexpected(std::move(result297.error()));
  return String(*result297);
});
if (!result298) return std::unexpected(std::move(result298.error()));

  auto result300 = callbacks.registerCallback("humanoidIdentity", [this]() -> std::expected<Json, StarException> { auto result299 = humanoid();
if (!result299) return std::unexpected(std::move(result299.error()));
return std::move(*result299)->identity().toJson(); });
if (!result300) return std::unexpected(std::move(result300.error()));
  auto result303 = callbacks.registerCallback("setHumanoidIdentity", [this](Json const& id) -> std::expected<void, StarException> { auto result301 = HumanoidIdentity::create(id);
if (!result301) return std::unexpected(std::move(result301.error()));
auto result302 = setIdentity(std::move(*result301));
if (!result302) return std::unexpected(std::move(result302.error()));
   return {};
});
if (!result303) return std::unexpected(std::move(result303.error()));
auto result305 = callbacks.registerCallback(
  "setHumanoidParameter", [this](String key, std::optional<Json> value) -> std::expected<void, StarException> {
    auto result304 = setHumanoidParameter(std::move(key), std::move(value));
    if (!result304)
      return std::unexpected(std::move(result304.error()));
    return {};
  });
if (!result305) return std::unexpected(std::move(result305.error()));
auto result306 = callbacks.registerCallback(
  "getHumanoidParameter", [this](String key) -> std::optional<Json> { return getHumanoidParameter(std::move(key)); });
if (!result306) return std::unexpected(std::move(result306.error()));
auto result308 = callbacks.registerCallback("setHumanoidParameters",
                                            [this](JsonObject parameters) -> std::expected<void, StarException> {
                                              auto result307 = setHumanoidParameters(std::move(parameters));
                                              if (!result307)
                                                return std::unexpected(std::move(result307.error()));
                                              return {};
                                            });
if (!result308) return std::unexpected(std::move(result308.error()));
  auto result309 = callbacks.registerCallback("getHumanoidParameters", [this]() -> JsonObject { return getHumanoidParameters(); });
if (!result309) return std::unexpected(std::move(result309.error()));
  auto result311 = callbacks.registerCallback("refreshHumanoidParameters", [this]() -> std::expected<void, StarException> { auto result310 = refreshHumanoidParameters();
if (!result310) return std::unexpected(std::move(result310.error()));
   return {};
});
if (!result311) return std::unexpected(std::move(result311.error()));
  auto result313 = callbacks.registerCallback("humanoidConfig", [this](bool withOverrides) -> std::expected<Json, StarException> { auto result312 = humanoid();
if (!result312) return std::unexpected(std::move(result312.error()));
return std::move(*result312)->humanoidConfig(withOverrides); });
if (!result313) return std::unexpected(std::move(result313.error()));

  auto result314 = callbacks.registerCallback(   "bodyDirectives", [this]()   { return identity().bodyDirectives;      });
if (!result314) return std::unexpected(std::move(result314.error()));
  auto result316 = callbacks.registerCallback("setBodyDirectives", [this](String const& str) -> std::expected<void, StarException> { auto result315 = setBodyDirectives(str);
if (!result315) return std::unexpected(std::move(result315.error()));
   return {};
});
if (!result316) return std::unexpected(std::move(result316.error()));

  auto result317 = callbacks.registerCallback(   "emoteDirectives", [this]()   { return identity().emoteDirectives;      });
if (!result317) return std::unexpected(std::move(result317.error()));
  auto result319 = callbacks.registerCallback("setEmoteDirectives", [this](String const& str) -> std::expected<void, StarException> { auto result318 = setEmoteDirectives(str);
if (!result318) return std::unexpected(std::move(result318.error()));
   return {};
});
if (!result319) return std::unexpected(std::move(result319.error()));

  auto result320 = callbacks.registerCallback(   "hairGroup",      [this]()   { return identity().hairGroup;      });
if (!result320) return std::unexpected(std::move(result320.error()));
  auto result322 = callbacks.registerCallback("setHairGroup",      [this](String const& str) -> std::expected<void, StarException> { auto result321 = setHairGroup(str);
if (!result321) return std::unexpected(std::move(result321.error()));
   return {};
});
if (!result322) return std::unexpected(std::move(result322.error()));
  auto result323 = callbacks.registerCallback(   "hairType",       [this]()   { return identity().hairType;      });
if (!result323) return std::unexpected(std::move(result323.error()));
  auto result325 = callbacks.registerCallback("setHairType",       [this](String const& str) -> std::expected<void, StarException> { auto result324 = setHairType(str);
if (!result324) return std::unexpected(std::move(result324.error()));
   return {};
});
if (!result325) return std::unexpected(std::move(result325.error()));
  auto result326 = callbacks.registerCallback(   "hairDirectives", [this]()   { return identity().hairDirectives;     });
if (!result326) return std::unexpected(std::move(result326.error()));
  auto result328 = callbacks.registerCallback("setHairDirectives", [this](String const& str) -> std::expected<void, StarException> { auto result327 = setHairDirectives(str);
if (!result327) return std::unexpected(std::move(result327.error()));
   return {};
});
if (!result328) return std::unexpected(std::move(result328.error()));

  auto result329 = callbacks.registerCallback(   "facialHairGroup",      [this]()   { return identity().facialHairGroup;      });
if (!result329) return std::unexpected(std::move(result329.error()));
  auto result331 = callbacks.registerCallback("setFacialHairGroup",      [this](String const& str) -> std::expected<void, StarException> { auto result330 = setFacialHairGroup(str);
if (!result330) return std::unexpected(std::move(result330.error()));
   return {};
});
if (!result331) return std::unexpected(std::move(result331.error()));
  auto result332 = callbacks.registerCallback(   "facialHairType",       [this]()   { return identity().facialHairType;      });
if (!result332) return std::unexpected(std::move(result332.error()));
  auto result334 = callbacks.registerCallback("setFacialHairType",       [this](String const& str) -> std::expected<void, StarException> { auto result333 = setFacialHairType(str);
if (!result333) return std::unexpected(std::move(result333.error()));
   return {};
});
if (!result334) return std::unexpected(std::move(result334.error()));
  auto result335 = callbacks.registerCallback(   "facialHairDirectives", [this]()   { return identity().facialHairDirectives;      });
if (!result335) return std::unexpected(std::move(result335.error()));
  auto result337 = callbacks.registerCallback("setFacialHairDirectives", [this](String const& str) -> std::expected<void, StarException> { auto result336 = setFacialHairDirectives(str);
if (!result336) return std::unexpected(std::move(result336.error()));
   return {};
});
if (!result337) return std::unexpected(std::move(result337.error()));

  auto result338 = callbacks.registerCallback("hair", [this]() {
    HumanoidIdentity const& humanoidIdentity = identity();
    return luaTupleReturn(humanoidIdentity.hairGroup, humanoidIdentity.hairType, humanoidIdentity.hairDirectives);
  });
if (!result338) return std::unexpected(std::move(result338.error()));

  auto result339 = callbacks.registerCallback("facialHair", [this]() {
    HumanoidIdentity const& humanoidIdentity = identity();
    return luaTupleReturn(humanoidIdentity.facialHairGroup, humanoidIdentity.facialHairType, humanoidIdentity.facialHairDirectives);
  });
if (!result339) return std::unexpected(std::move(result339.error()));

  auto result340 = callbacks.registerCallback("facialMask", [this]() {
    HumanoidIdentity const& humanoidIdentity = identity();
    return luaTupleReturn(humanoidIdentity.facialMaskGroup, humanoidIdentity.facialMaskType, humanoidIdentity.facialMaskDirectives);
  });
if (!result340) return std::unexpected(std::move(result340.error()));

  auto result345 = callbacks.registerCallback("setFacialHair", [this](std::optional<String> const& group, std::optional<String> const& type, std::optional<String> const& directives) -> std::expected<void, StarException> {
    if (group && type && directives)
      {
  auto result341 = setFacialHair(*group, *type, *directives);
  if (!result341) return std::unexpected(std::move(result341.error()));

}
    else {
      if (group)      {
  auto result342 = setFacialHairGroup(*group);
  if (!result342) return std::unexpected(std::move(result342.error()));

}
      if (type)       {
  auto result343 = setFacialHairType(*type);
  if (!result343) return std::unexpected(std::move(result343.error()));

}
      if (directives) {
  auto result344 = setFacialHairDirectives(*directives);
  if (!result344) return std::unexpected(std::move(result344.error()));

}
    }
    return {};
});
if (!result345) return std::unexpected(std::move(result345.error()));

  auto result350 = callbacks.registerCallback("setFacialMask", [this](std::optional<String> const& group, std::optional<String> const& type, std::optional<String> const& directives) -> std::expected<void, StarException> {
    if (group && type && directives)
      {
  auto result346 = setFacialMask(*group, *type, *directives);
  if (!result346) return std::unexpected(std::move(result346.error()));

}
    else {
      if (group)      {
  auto result347 = setFacialMaskGroup(*group);
  if (!result347) return std::unexpected(std::move(result347.error()));

}
      if (type)       {
  auto result348 = setFacialMaskType(*type);
  if (!result348) return std::unexpected(std::move(result348.error()));

}
      if (directives) {
  auto result349 = setFacialMaskDirectives(*directives);
  if (!result349) return std::unexpected(std::move(result349.error()));

}
    }
    return {};
});
if (!result350) return std::unexpected(std::move(result350.error()));

  auto result355 = callbacks.registerCallback("setHair", [this](std::optional<String> const& group, std::optional<String> const& type, std::optional<String> const& directives) -> std::expected<void, StarException> {
    if (group && type && directives)
      {
  auto result351 = setHair(*group, *type, *directives);
  if (!result351) return std::unexpected(std::move(result351.error()));

}
    else {
      if (group)      {
  auto result352 = setHairGroup(*group);
  if (!result352) return std::unexpected(std::move(result352.error()));

}
      if (type)       {
  auto result353 = setHairType(*type);
  if (!result353) return std::unexpected(std::move(result353.error()));

}
      if (directives) {
  auto result354 = setHairDirectives(*directives);
  if (!result354) return std::unexpected(std::move(result354.error()));

}
    }
    return {};
});
if (!result355) return std::unexpected(std::move(result355.error()));

  auto result356 = callbacks.registerCallback(   "description", [this]()                          { return description(); });
if (!result356) return std::unexpected(std::move(result356.error()));
  auto result357 = callbacks.registerCallback("setDescription", [this](String const& description) { setDescription(description); });
if (!result357) return std::unexpected(std::move(result357.error()));

  auto result358 = callbacks.registerCallback(   "name", [this]()                   { return name(); });
if (!result358) return std::unexpected(std::move(result358.error()));
  auto result360 = callbacks.registerCallback("setName", [this](String const& name) -> std::expected<void, StarException> { auto result359 = setName(name);
if (!result359) return std::unexpected(std::move(result359.error()));
   return {};
});
if (!result360) return std::unexpected(std::move(result360.error()));

  auto result362 = callbacks.registerCallback("setSpecies", [this](String const& species) -> std::expected<void, StarException> { auto result361 = setSpecies(species);
if (!result361) return std::unexpected(std::move(result361.error()));
   return {};
});
if (!result362) return std::unexpected(std::move(result362.error()));

  auto result363 = callbacks.registerCallback(   "imagePath", [this]()                        { return identity().imagePath;    });
if (!result363) return std::unexpected(std::move(result363.error()));
  auto result365 = callbacks.registerCallback("setImagePath", [this](std::optional<String> const& imagePath) -> std::expected<void, StarException> { auto result364 = setImagePath(imagePath);
if (!result364) return std::unexpected(std::move(result364.error()));
   return {};
});
if (!result365) return std::unexpected(std::move(result365.error()));

auto result368 =
  callbacks.registerCallback("setGender", [this](String const& gender) -> std::expected<void, StarException> {
    auto result366 = enumValue<GenderNames>(gender);
    if (!result366)
      return std::unexpected(std::move(result366.error()));
    auto result367 = setGender(*result366);
    if (!result367)
      return std::unexpected(std::move(result367.error()));
    return {};
  });
if (!result368) return std::unexpected(std::move(result368.error()));

  auto result369 = callbacks.registerCallback(   "personality", [this]() { return jsonFromPersonality(identity().personality); });
if (!result369) return std::unexpected(std::move(result369.error()));
  auto result372 = callbacks.registerCallback("setPersonality", [this](Json const& personalityConfig) -> std::expected<void, StarException> {
    Personality const& oldPersonality = identity().personality;
    Personality newPersonality = oldPersonality;
    auto result370 = parsePersonality(newPersonality, personalityConfig);
if (!result370) return std::unexpected(std::move(result370.error()));

    auto result371 = setPersonality(newPersonality);
if (!result371) return std::unexpected(std::move(result371.error()));

    return {};
});
if (!result372) return std::unexpected(std::move(result372.error()));

  auto result373 = callbacks.registerCallback(   "favoriteColor", [this]()            { return favoriteColor();  });
if (!result373) return std::unexpected(std::move(result373.error()));
  auto result375 = callbacks.registerCallback("setFavoriteColor", [this](Color color) -> std::expected<void, StarException> { auto result374 = setFavoriteColor(color);
if (!result374) return std::unexpected(std::move(result374.error()));
   return {};
});
if (!result375) return std::unexpected(std::move(result375.error()));


  auto result376 = callbacks.registerCallback("npcType", [this]() { return npcType(); });
if (!result376) return std::unexpected(std::move(result376.error()));

  auto result377 = callbacks.registerCallback("seed", [this]() { return m_npcVariant.seed; });
if (!result377) return std::unexpected(std::move(result377.error()));

  auto result378 = callbacks.registerCallback("level", [this]() { return m_npcVariant.level; });
if (!result378) return std::unexpected(std::move(result378.error()));

  auto result379 = callbacks.registerCallback("dropPools", [this]() { return m_dropPools.get(); });
if (!result379) return std::unexpected(std::move(result379.error()));

  auto result380 = callbacks.registerCallback("setDropPools", [this](StringList const& dropPools) { m_dropPools.set(dropPools); });
if (!result380) return std::unexpected(std::move(result380.error()));

  auto result381 = callbacks.registerCallback("energy", [this]() { return m_statusController->resource("energy"); });
if (!result381) return std::unexpected(std::move(result381.error()));

  auto result382 = callbacks.registerCallback("maxEnergy", [this]() { return m_statusController->resourceMax("energy"); });
if (!result382) return std::unexpected(std::move(result382.error()));

  auto result384 = callbacks.registerCallback("say", [this](String line, std::optional<StringMap<String>> const& tags, Json const& config) -> std::expected<bool, StarException> {
      if (tags)
        line = line.replaceTags(*tags, false);

      if (!line.empty()) {
        auto result383 = addChatMessage(line, config);
if (!result383) return std::unexpected(std::move(result383.error()));

        return true;
      }

      return false;
    });
if (!result384) return std::unexpected(std::move(result384.error()));

  auto result386 = callbacks.registerCallback("sayPortrait", [this](String line, String portrait, std::optional<StringMap<String>> const& tags, Json const& config) -> std::expected<bool, StarException> {
      if (tags)
        line = line.replaceTags(*tags, false);

      if (!line.empty()) {
        auto result385 = addChatMessage(line, config, portrait);
if (!result385) return std::unexpected(std::move(result385.error()));

        return true;
      }

      return false;
    });
if (!result386) return std::unexpected(std::move(result386.error()));

  auto result388 = callbacks.registerCallback("emote", [this](String const& arg1) -> std::expected<void, StarException> { if (!HumanoidEmoteNames) return std::unexpected(HumanoidEmoteNames.error());
    auto result387 = HumanoidEmoteNames->get_left(arg1);
    if (!result387)
      return std::unexpected(std::move(result387.error()));
    addEmote(result387->get());
    return {};
});
if (!result388) return std::unexpected(std::move(result388.error()));

  auto result390 = callbacks.registerCallback("dance", [this](std::optional<String> const& danceName) -> std::expected<void, StarException> { auto result389 = setDance(danceName);
if (!result389) return std::unexpected(std::move(result389.error()));
   return {};
});
if (!result390) return std::unexpected(std::move(result390.error()));

  auto result391 = callbacks.registerCallback("setInteractive", [this](bool interactive) { m_isInteractive.set(interactive); });
if (!result391) return std::unexpected(std::move(result391.error()));

  auto result399 = callbacks.registerCallback("setLounging", [this](EntityId loungeableEntityId, std::optional<std::size_t> maybeAnchorIndex) -> std::expected<bool, StarException> {
      std::size_t anchorIndex = maybeAnchorIndex.value_or(0);
      auto result392 = world();
if (!result392) return std::unexpected(std::move(result392.error()));
      auto loungeableEntity = result392->get().get<LoungeableEntity>(loungeableEntityId);
      {
  bool condition395 = static_cast<bool>(!loungeableEntity || anchorIndex >= loungeableEntity->anchorCount());
  if (!condition395) {
    auto result394 = loungeableEntity->entitiesLoungingIn(anchorIndex);
    if (!result394) return std::unexpected(std::move(result394.error()));
    condition395 = static_cast<bool>(!std::move(*result394).empty());
  }
  bool condition397 = static_cast<bool>(condition395);
  if (!condition397) {
    auto result396 = loungeableEntity->loungeAnchor(anchorIndex);
    if (!result396) return std::unexpected(std::move(result396.error()));
    condition397 = static_cast<bool>(!std::move(*result396));
  }
  if (condition397)
          return false;
}

      auto result398 = m_movementController->setAnchorState({loungeableEntityId, anchorIndex});
if (!result398) return std::unexpected(std::move(result398.error()));

      return true;
    });
if (!result399) return std::unexpected(std::move(result399.error()));

  auto result401 = callbacks.registerCallback("resetLounging", [this]() -> std::expected<void, StarException> { auto result400 = m_movementController->resetAnchorState();
if (!result400) return std::unexpected(std::move(result400.error()));
   return {};
});
if (!result401) return std::unexpected(std::move(result401.error()));

  auto result402 = callbacks.registerCallback("isLounging", [this]() { return is<LoungeAnchor>(m_movementController->entityAnchor()); });
if (!result402) return std::unexpected(std::move(result402.error()));

  auto result403 = callbacks.registerCallback("loungingIn", [this]() -> std::optional<EntityId> {
      auto loungingState = loungingIn();
      if (loungingState)
        return loungingState.value_or(EntityAnchorState{}).entityId;
      else
        return {};
    });
if (!result403) return std::unexpected(std::move(result403.error()));

  auto result404 = callbacks.registerCallback("setOfferedQuests", [this](std::optional<JsonArray> const& offeredQuests) -> std::expected<void, StarException> {
      List<QuestArcDescriptor> parsed;
      for (auto const& quest : offeredQuests.value_or(JsonArray{})) {
        auto arc = QuestArcDescriptor::fromJson(root(), quest);
        if (!arc) return std::unexpected(std::move(arc.error()));
        parsed.push_back(std::move(*arc));
      }
      m_offeredQuests.set(std::move(parsed));
      return {};
    });
if (!result404) return std::unexpected(std::move(result404.error()));

  auto result405 = callbacks.registerCallback("setTurnInQuests", [this](std::optional<StringList> const& turnInQuests) {
      m_turnInQuests.set(StringSet::from(turnInQuests.value_or(StringList{})));
    });
if (!result405) return std::unexpected(std::move(result405.error()));

  auto result407 = callbacks.registerCallback("setItemSlot", [this](String const& slot, Json const& itemDescriptor) -> std::expected<Json, StarException> {
      auto result406 = ItemDescriptor::create(itemDescriptor);
if (!result406) return std::unexpected(std::move(result406.error()));
return setItemSlot(slot, std::move(*result406));
    });
if (!result407) return std::unexpected(std::move(result407.error()));

  auto result412 = callbacks.registerCallback("getItemSlot", [this](String const& entry) -> std::expected<Json, StarException> {
      {
  if (!EquipmentSlotNames) return std::unexpected(EquipmentSlotNames.error());
  auto result408 = EquipmentSlotNames->find_left_ref(entry);
  if (!result408) return std::unexpected(std::move(result408.error()));
  if (auto equipmentSlot = std::move(*result408)) {
    return m_armor->itemDescriptor(static_cast<std::uint8_t>(equipmentSlot->get())).toJson();
        } else {
    auto result409 = entry.equalsIgnoreCase("primary");
    if (!result409) return std::unexpected(std::move(result409.error()));
    if (std::move(*result409))
            return m_tools->primaryHandItemDescriptor().toJson();
          else {
      auto result410 = entry.equalsIgnoreCase("alt");
      if (!result410) return std::unexpected(std::move(result410.error()));
      if (std::move(*result410))
              return m_tools->altHandItemDescriptor().toJson();
            else if (m_npcVariant.items.contains(entry))
              {
        auto result411 = m_npcVariant.items.get(entry);
        if (!result411) return std::unexpected(std::move(result411.error()));
        return result411->get().toJson();
      }
    }
  }
}

      return {};
    });
if (!result412) return std::unexpected(std::move(result412.error()));

  auto result413 = callbacks.registerCallback("disableWornArmor", [this](bool disable) { m_disableWornArmor.set(disable); });
if (!result413) return std::unexpected(std::move(result413.error()));

  auto result414 = callbacks.registerCallback("beginPrimaryFire", [this]() { m_tools->beginPrimaryFire(); });
if (!result414) return std::unexpected(std::move(result414.error()));
  auto result415 = callbacks.registerCallback("beginAltFire", [this]() { m_tools->beginAltFire(); });
if (!result415) return std::unexpected(std::move(result415.error()));
  auto result416 = callbacks.registerCallback("endPrimaryFire", [this]() { m_tools->endPrimaryFire(); });
if (!result416) return std::unexpected(std::move(result416.error()));
  auto result417 = callbacks.registerCallback("endAltFire", [this]() { m_tools->endAltFire(); });
if (!result417) return std::unexpected(std::move(result417.error()));
  auto result418 = callbacks.registerCallback("setShifting", [this](bool shifting) { m_shifting.set(shifting); });
if (!result418) return std::unexpected(std::move(result418.error()));
  auto result419 = callbacks.registerCallback("setDamageOnTouch", [this](bool damageOnTouch) { m_damageOnTouch.set(damageOnTouch); });
if (!result419) return std::unexpected(std::move(result419.error()));

  auto result421 = callbacks.registerCallback("aimPosition", [this]() -> std::expected<Json, StarException> { auto result420 = aimPosition();
if (!result420) return std::unexpected(std::move(result420.error()));
return jsonFromVec2F(std::move(*result420)); });
if (!result421) return std::unexpected(std::move(result421.error()));

  auto result423 = callbacks.registerCallback("setAimPosition", [this](Vec2F const& pos) -> std::expected<void, StarException> {
      auto result422 = world();
if (!result422) return std::unexpected(std::move(result422.error()));
auto aimPosition = result422->get().geometry().diff(pos, position());
      m_xAimPosition.set(aimPosition[0]);
      m_yAimPosition.set(aimPosition[1]);
      return {};
});
if (!result423) return std::unexpected(std::move(result423.error()));

  auto result424 = callbacks.registerCallback("setDeathParticleBurst", [this](std::optional<String> const& deathParticleBurst) {
      m_deathParticleBurst.set(deathParticleBurst);
    });
if (!result424) return std::unexpected(std::move(result424.error()));

  auto result425 = callbacks.registerCallback("setStatusText", [this](std::optional<String> const& status) { m_statusText.set(status); });
if (!result425) return std::unexpected(std::move(result425.error()));
  auto result426 = callbacks.registerCallback("setDisplayNametag", [this](bool display) { m_displayNametag.set(display); });
if (!result426) return std::unexpected(std::move(result426.error()));

  auto result427 = callbacks.registerCallback("setPersistent", [this](bool persistent) { setPersistent(persistent); });
if (!result427) return std::unexpected(std::move(result427.error()));

  auto result428 = callbacks.registerCallback("setKeepAlive", [this](bool keepAlive) { setKeepAlive(keepAlive); });
if (!result428) return std::unexpected(std::move(result428.error()));

  auto result430 = callbacks.registerCallback("setDamageTeam", [this](Json const& team) -> std::expected<void, StarException> { auto result429 = EntityDamageTeam::create(team);
if (!result429) return std::unexpected(std::move(result429.error()));
setTeam(std::move(*result429));   return {};
});
if (!result430) return std::unexpected(std::move(result430.error()));

  auto result431 = callbacks.registerCallback("setAggressive", [this](bool aggressive) { m_aggressive.set(aggressive); });
if (!result431) return std::unexpected(std::move(result431.error()));

auto result432 = callbacks.registerCallback(
  "setUniqueId", [this](std::optional<String> uniqueId) { setUniqueId(std::move(uniqueId)); });
if (!result432) return std::unexpected(std::move(result432.error()));

  auto result433 = callbacks.registerCallback("setAnimationParameter", [this](String name, Json value) {
    m_scriptedAnimationParameters.set(std::move(name), std::move(value));
  });
if (!result433) return std::unexpected(std::move(result433.error()));

  return callbacks;
}

std::expected<void, StarException> Npc::setupNetStates() {
  auto result434 = m_netGroup.addNetElement(m_xAimPosition);
if (!result434) return std::unexpected(std::move(result434.error()));

  auto result435 = m_netGroup.addNetElement(m_yAimPosition);
if (!result435) return std::unexpected(std::move(result435.error()));


  m_xAimPosition.setFixedPointBase(0.0625);
  m_yAimPosition.setFixedPointBase(0.0625);
  m_xAimPosition.setInterpolator(lerp<float, float>);
  m_yAimPosition.setInterpolator(lerp<float, float>);

  auto result436 = m_netGroup.addNetElement(m_uniqueIdNetState);
if (!result436) return std::unexpected(std::move(result436.error()));

  auto result437 = m_netGroup.addNetElement(m_teamNetState);
if (!result437) return std::unexpected(std::move(result437.error()));

  auto result438 = m_netGroup.addNetElement(m_humanoidStateNetState);
if (!result438) return std::unexpected(std::move(result438.error()));

  auto result439 = m_netGroup.addNetElement(m_humanoidEmoteStateNetState);
if (!result439) return std::unexpected(std::move(result439.error()));

  auto result440 = m_netGroup.addNetElement(m_humanoidDanceNetState);
if (!result440) return std::unexpected(std::move(result440.error()));


  auto result441 = m_netGroup.addNetElement(m_newChatMessageEvent);
if (!result441) return std::unexpected(std::move(result441.error()));

  auto result442 = m_netGroup.addNetElement(m_chatMessage);
if (!result442) return std::unexpected(std::move(result442.error()));

  auto result443 = m_netGroup.addNetElement(m_chatPortrait);
if (!result443) return std::unexpected(std::move(result443.error()));

  auto result444 = m_netGroup.addNetElement(m_chatConfig);
if (!result444) return std::unexpected(std::move(result444.error()));


  auto result445 = m_netGroup.addNetElement(m_statusText);
if (!result445) return std::unexpected(std::move(result445.error()));

  auto result446 = m_netGroup.addNetElement(m_displayNametag);
if (!result446) return std::unexpected(std::move(result446.error()));


  auto result447 = m_netGroup.addNetElement(m_isInteractive);
if (!result447) return std::unexpected(std::move(result447.error()));


  auto result448 = m_netGroup.addNetElement(m_offeredQuests);
if (!result448) return std::unexpected(std::move(result448.error()));

  auto result449 = m_netGroup.addNetElement(m_turnInQuests);
if (!result449) return std::unexpected(std::move(result449.error()));


  auto result450 = m_netGroup.addNetElement(m_shifting);
if (!result450) return std::unexpected(std::move(result450.error()));

  auto result451 = m_netGroup.addNetElement(m_damageOnTouch);
if (!result451) return std::unexpected(std::move(result451.error()));


  auto result452 = m_netGroup.addNetElement(m_disableWornArmor);
if (!result452) return std::unexpected(std::move(result452.error()));


  auto result453 = m_netGroup.addNetElement(m_deathParticleBurst);
if (!result453) return std::unexpected(std::move(result453.error()));


  auto result454 = m_netGroup.addNetElement(m_dropPools);
if (!result454) return std::unexpected(std::move(result454.error()));

  auto result455 = m_netGroup.addNetElement(m_aggressive);
if (!result455) return std::unexpected(std::move(result455.error()));


  auto result456 = m_netGroup.addNetElement(*m_movementController);
if (!result456) return std::unexpected(std::move(result456.error()));

  auto result457 = m_netGroup.addNetElement(*m_effectEmitter);
if (!result457) return std::unexpected(std::move(result457.error()));

  auto result458 = m_netGroup.addNetElement(*m_statusController);
if (!result458) return std::unexpected(std::move(result458.error()));

  auto result459 = m_netGroup.addNetElement(*m_armor);
if (!result459) return std::unexpected(std::move(result459.error()));

  auto result460 = m_netGroup.addNetElement(*m_tools);
if (!result460) return std::unexpected(std::move(result460.error()));

  m_songbook->setCompatibilityVersion(6);
  auto result461 = m_netGroup.addNetElement(*m_songbook);
if (!result461) return std::unexpected(std::move(result461.error()));


  m_identityNetState.setCompatibilityVersion(10);
  auto result462 = m_netGroup.addNetElement(m_identityNetState);
if (!result462) return std::unexpected(std::move(result462.error()));

  m_refreshedHumanoidParameters.setCompatibilityVersion(10);
  auto result463 = m_netGroup.addNetElement(m_refreshedHumanoidParameters);
if (!result463) return std::unexpected(std::move(result463.error()));


  m_netHumanoid.setCompatibilityVersion(10);
  auto result464 = m_netGroup.addNetElement(m_netHumanoid);
if (!result464) return std::unexpected(std::move(result464.error()));


  m_scriptedAnimationParameters.setCompatibilityVersion(10);
  auto result465 = m_netGroup.addNetElement(m_scriptedAnimationParameters);
if (!result465) return std::unexpected(std::move(result465.error()));


  m_netGroup.setNeedsStoreCallback([this] { return setNetStates(); });
  m_netGroup.setNeedsLoadCallback([this](bool initial) { return getNetStates(initial); });
  return {};
}

std::expected<void, StarException> Npc::setNetStates() {
  m_uniqueIdNetState.set(uniqueId());
  m_teamNetState.set(getTeam());
  auto result466 = humanoid();
if (!result466) return std::unexpected(std::move(result466.error()));
m_humanoidStateNetState.set(std::move(*result466)->state());
  auto result467 = humanoid();
if (!result467) return std::unexpected(std::move(result467.error()));
m_humanoidEmoteStateNetState.set(std::move(*result467)->emoteState());
  auto result468 = humanoid();
if (!result468) return std::unexpected(std::move(result468.error()));
m_humanoidDanceNetState.set(std::move(*result468)->dance());

  if (m_identityUpdated) {
    m_identityNetState.push(m_npcVariant.humanoidIdentity);
    m_identityUpdated = false;
  }
  return {};
}

std::expected<void, StarException> Npc::getNetStates(bool initial) {
  setUniqueId(m_uniqueIdNetState.get());
  setTeam(m_teamNetState.get());
  auto result469 = humanoid();
if (!result469) return std::unexpected(std::move(result469.error()));
auto result470 = std::move(*result469)->setState(m_humanoidStateNetState.get());
if (!result470) return std::unexpected(std::move(result470.error()));

  auto result471 = humanoid();
if (!result471) return std::unexpected(std::move(result471.error()));
auto result472 = std::move(*result471)->setEmoteState(m_humanoidEmoteStateNetState.get());
if (!result472) return std::unexpected(std::move(result472.error()));

  auto result473 = humanoid();
if (!result473) return std::unexpected(std::move(result473.error()));
auto result474 = std::move(*result473)->setDance(m_humanoidDanceNetState.get());
if (!result474) return std::unexpected(std::move(result474.error()));


  if ((m_identityNetState.pullUpdated()) && !initial) {
    auto newIdentity = m_identityNetState.get();
    if ((m_npcVariant.humanoidIdentity.species == newIdentity.species) && (m_npcVariant.humanoidIdentity.imagePath == newIdentity.imagePath)) {
      auto result475 = humanoid();
if (!result475) return std::unexpected(std::move(result475.error()));
auto result476 = std::move(*result475)->setIdentity(newIdentity);
if (!result476) return std::unexpected(std::move(result476.error()));

    }
    m_npcVariant.humanoidIdentity = newIdentity;
  }
  if (m_refreshedHumanoidParameters.pullOccurred() && !initial) {
    auto result477 = refreshHumanoidParameters();
if (!result477) return std::unexpected(std::move(result477.error()));

  }

  if (m_newChatMessageEvent.pullOccurred() && !initial) {
    m_chatMessageUpdated = true;
    if (m_chatPortrait.get().empty())
      {
  auto result478 = mouthPosition();
  if (!result478) return std::unexpected(std::move(result478.error()));
  m_pendingChatActions.push_back(
    SayChatAction{entityId(), m_chatMessage.get(), std::move(*result478), m_chatConfig.get()});
}
    else
      {
  auto result479 = mouthPosition();
  if (!result479) return std::unexpected(std::move(result479.error()));
  m_pendingChatActions.push_back(PortraitChatAction{entityId(), m_chatPortrait.get(), m_chatMessage.get(),
                                                    std::move(*result479), m_chatConfig.get()});
}
  }
  return {};
}

std::expected<Vec2F, StarException> Npc::mouthPosition() const {
  auto result480 = mouthOffset(true);
if (!result480) return std::unexpected(std::move(result480.error()));
return std::move(*result480) + position();
}

std::expected<Vec2F, StarException> Npc::mouthPosition(bool ignoreAdjustments) const {
  auto result481 = mouthOffset(ignoreAdjustments);
if (!result481) return std::unexpected(std::move(result481.error()));
return std::move(*result481) + position();
}

List<ChatAction> Npc::pullPendingChatActions() {
  return std::move(m_pendingChatActions);
}

std::expected<void, StarException> Npc::addChatMessage(String const& message, Json const& config, String const& portrait) {
  assert(!isSlave());
  m_chatMessage.set(message);
  m_chatPortrait.set(portrait);
  m_chatConfig.set(config);
  m_chatMessageUpdated = true;
  m_newChatMessageEvent.trigger();
  if (portrait.empty())
    {
  auto result482 = mouthPosition();
  if (!result482) return std::unexpected(std::move(result482.error()));
  m_pendingChatActions.push_back(SayChatAction{entityId(), message, std::move(*result482), config});
}
  else
    {
  auto result483 = mouthPosition();
  if (!result483) return std::unexpected(std::move(result483.error()));
  m_pendingChatActions.push_back(PortraitChatAction{entityId(), portrait, message, std::move(*result483), config});
}
  return {};
}

void Npc::addEmote(HumanoidEmote const& emote) {
  assert(!isSlave());
  m_emoteState = emote;
  m_emoteCooldownTimer.reset();
}

std::expected<void, StarException> Npc::setDance(std::optional<String> const& danceName) {
  assert(!isSlave());
  m_dance = danceName;

  if (danceName.has_value()) {
    auto result484 = this->root().danceDatabase();
if (!result484) return std::unexpected(std::move(result484.error()));
auto danceDatabase = std::move(*result484);
    auto result485 = danceDatabase->getDance(*danceName);
if (!result485) return std::unexpected(std::move(result485.error()));
std::shared_ptr<Dance> dance = std::move(*result485);
    m_danceCooldownTimer = GameTimer(dance->duration);
  }
  return {};
}

bool Npc::isInteractive() const {
  return m_isInteractive.get();
}

std::expected<InteractAction, StarException> Npc::interact(InteractRequest const& request) {
  auto result486 = m_scriptComponent.invoke<Json>("interact",
      JsonObject{{"sourceId", request.sourceId}, {"sourcePosition", jsonFromVec2F(request.sourcePosition)}});
if (!result486) return std::unexpected(std::move(result486.error()));
auto result = std::move(*result486).value_or(Json{});

  if (result.isNull())
    return {};

  if (result.isType(Json::Type::String))
    {
  auto result487 = result.toString();
  if (!result487) return std::unexpected(std::move(result487.error()));
  return InteractAction::create(std::move(*result487), entityId(), Json());
}

  auto result488 = result.getString(0);
if (!result488) return std::unexpected(std::move(result488.error()));
auto result489 = result.get(1);
if (!result489) return std::unexpected(std::move(result489.error()));
return InteractAction::create(std::move(*result488), entityId(), std::move(*result489));
}

RectF Npc::interactiveBoundBox() const {
  return m_movementController->collisionPoly().boundBox();
}

std::optional<EntityAnchorState> Npc::loungingIn() const {
  if (is<LoungeAnchor>(m_movementController->entityAnchor()))
    return m_movementController->anchorState();
  return {};
}

std::expected<List<QuestArcDescriptor>, StarException> Npc::offeredQuests() const {
  return m_offeredQuests.get();
}

std::expected<StringSet, StarException> Npc::turnInQuests() const {
  return m_turnInQuests.get();
}

std::expected<Vec2F, StarException> Npc::questIndicatorPosition() const {
  Vec2F pos = position() + m_questIndicatorOffset;
  pos[1] += interactiveBoundBox().yMax();
  return pos;
}

std::expected<bool, StarException> Npc::setItemSlot(String const& slot, ItemDescriptor itemDescriptor) {
  auto result490 = this->root().itemDatabase();
if (!result490) return std::unexpected(std::move(result490.error()));
auto result491 = std::move(*result490)->item(std::move(itemDescriptor), m_npcVariant.level, m_npcVariant.seed);
if (!result491) return std::unexpected(std::move(result491.error()));
auto item = std::move(*result491);

  {
  if (!EquipmentSlotNames) return std::unexpected(EquipmentSlotNames.error());
  auto result492 = EquipmentSlotNames->find_left_ref(slot);
  if (!result492) return std::unexpected(std::move(result492.error()));
  if (auto equipmentSlot = std::move(*result492)) {
    m_armor->setItem(static_cast<std::uint8_t>(equipmentSlot->get()), as<ArmorItem>(item));
    } else {
    auto result493 = slot.equalsIgnoreCase("primary");
    if (!result493) return std::unexpected(std::move(result493.error()));
    if (std::move(*result493))
        {
      auto result494 = m_tools->setItems(item, m_tools->altHandItem());
      if (!result494) return std::unexpected(std::move(result494.error()));

    }
      else {
      auto result495 = slot.equalsIgnoreCase("alt");
      if (!result495) return std::unexpected(std::move(result495.error()));
      if (std::move(*result495))
          {
        auto result496 = m_tools->setItems(m_tools->primaryHandItem(), item);
        if (!result496) return std::unexpected(std::move(result496.error()));

      }
        else
          return false;
    }
  }
}

  return true;
}

std::expected<bool, StarException> Npc::canUseTool() const {
  auto result497 = shouldDestroy();
if (!result497) return std::unexpected(std::move(result497.error()));
bool canUse = !std::move(*result497) && !m_statusController->toolUsageSuppressed();
  if (canUse) {
    auto anchor = m_movementController->entityAnchor();
    if (auto loungeAnchor = anchor ? asRef<LoungeAnchor>(*anchor) : std::nullopt)
      if (loungeAnchor->get().suppressTools.value_or(loungeAnchor->get().controllable))
        return false;
  }
  return canUse;
}

void Npc::disableWornArmor(bool disable) {
  m_disableWornArmor.set(disable);
}

std::expected<List<LightSource>, StarException> Npc::lightSources() const {
  return presentationLightSources(1.0f);
}

std::expected<List<LightSource>, StarException> Npc::presentationLightSources(float interpolation) const {
  List<LightSource> lights;
  auto result498 = m_tools->lightSources(interpolation);
if (!result498) return std::unexpected(std::move(result498.error()));
lights.append_range(std::move(*result498));
auto result499 = m_statusController->lightSources(interpolation);
if (!result499) return std::unexpected(std::move(result499.error()));
lights.append_range(std::move(*result499));
auto result500 = humanoid();
if (!result500) return std::unexpected(std::move(result500.error()));
auto animator = (*result500)->networkedAnimator();
auto humanoidSample = animator->get().sample(interpolation);
auto prepared = (*result500)->prepareAnimatorPose(humanoidSample);
if (!prepared) return std::unexpected(std::move(prepared.error()));
auto result501 = humanoidSample.lightSources();
if (!result501) return std::unexpected(std::move(result501.error()));
lights.append_range(std::move(*result501));
return lights;
}

std::expected<std::optional<ChainableJsonMessageResponse>, StarException> Npc::receiveMessage(ConnectionId sendingConnection, String const& message, JsonArray const& args) {
  auto result502 = world();
if (!result502) return std::unexpected(std::move(result502.error()));
auto result503 = m_scriptComponent.handleMessage(message, result502->get().connection() == sendingConnection, args);
if (!result503) return std::unexpected(std::move(result503.error()));
std::optional<ChainableJsonMessageResponse> result = std::move(*result503);
  if (!result)
    {
  auto result504 = world();
  if (!result504) return std::unexpected(std::move(result504.error()));
  auto result505 = m_statusController->receiveMessage(message, result504->get().connection() == sendingConnection, args);
  if (!result505) return std::unexpected(std::move(result505.error()));
  result = std::move(*result505);
}
  return result;
}

std::expected<Vec2F, StarException> Npc::armPosition(ToolHand hand, Direction facingDirection, float armAngle, Vec2F offset) const {
  auto result506 = humanoid();
if (!result506) return std::unexpected(std::move(result506.error()));
return m_tools->armPosition(*std::move(*result506), hand, facingDirection, armAngle, offset);
}

std::expected<Vec2F, StarException> Npc::handOffset(ToolHand hand, Direction facingDirection) const {
  auto result507 = humanoid();
if (!result507) return std::unexpected(std::move(result507.error()));
return m_tools->handOffset(*std::move(*result507), hand, facingDirection);
}

std::expected<Vec2F, StarException> Npc::handPosition(ToolHand hand, Vec2F const& handOffset) const {
  auto result508 = humanoid();
if (!result508) return std::unexpected(std::move(result508.error()));
return m_tools->handPosition(hand, *std::move(*result508), handOffset);
}

std::shared_ptr<Item> Npc::handItem(ToolHand hand) const {
  if (hand == ToolHand::Primary)
    return m_tools->primaryHandItem();
  return m_tools->altHandItem();
}

std::expected<Vec2F, StarException> Npc::armAdjustment() const {
  auto result509 = humanoid();
if (!result509) return std::unexpected(std::move(result509.error()));
return std::move(*result509)->armAdjustment();
}

Vec2F Npc::velocity() const {
  return m_movementController->velocity();
}

std::expected<Vec2F, StarException> Npc::aimPosition() const {
  auto result510 = world();
if (!result510) return std::unexpected(std::move(result510.error()));
return result510->get().geometry().xwrap(Vec2F(m_xAimPosition.get(), m_yAimPosition.get()) + position());
}

std::expected<float, StarException> Npc::interactRadius() const {
  return 9999;
}

Direction Npc::facingDirection() const {
  return m_movementController->facingDirection();
}

Direction Npc::walkingDirection() const {
  return m_movementController->movingDirection();
}

bool Npc::isAdmin() const {
  return false;
}

Color Npc::favoriteColor() const {
  return Color::White;
}

std::expected<float, StarException> Npc::beamGunRadius() const {
  return m_tools->beamGunRadius();
}

std::expected<void, StarException> Npc::addParticles(List<Particle> const&) {  return {};
}

std::expected<void, StarException> Npc::addSound(String const&, float, float) {  return {};
}

std::expected<bool, StarException> Npc::inToolRange() const {
  return true;
}

std::expected<bool, StarException> Npc::inToolRange(Vec2F const&) const {
  return true;
}

std::expected<void, StarException> Npc::addEphemeralStatusEffects(List<EphemeralStatusEffect> const& statusEffects) {
  auto result511 = m_statusController->addEphemeralEffects(statusEffects);
if (!result511) return std::unexpected(std::move(result511.error()));

  return {};
}

std::expected<ActiveUniqueStatusEffectSummary, StarException> Npc::activeUniqueStatusEffectSummary() const {
  return m_statusController->activeUniqueStatusEffectSummary();
}

std::expected<float, StarException> Npc::powerMultiplier() const {
  return m_statusController->stat("powerMultiplier");
}

std::expected<bool, StarException> Npc::fullEnergy() const {
  auto percentage = m_statusController->resourcePercentage("energy");
  if (!percentage) return std::unexpected(std::move(percentage.error()));
  if (!*percentage) return std::unexpected(NpcException("Energy resource has no maximum"));
  return **percentage >= 1.0f;
}

std::expected<float, StarException> Npc::energy() const {
  return m_statusController->resource("energy");
}

std::expected<bool, StarException> Npc::energyLocked() const {
  return m_statusController->resourceLocked("energy");
}

std::expected<bool, StarException> Npc::consumeEnergy(float energy) {
  return m_statusController->overConsumeResource("energy", energy);
}

std::expected<void, StarException> Npc::queueUIMessage(String const&) {  return {};
}

std::expected<bool, StarException> Npc::instrumentPlaying() {
  return m_songbook->instrumentPlaying();
}

std::expected<void, StarException> Npc::instrumentEquipped(String const& instrumentKind) {
  {
  auto result512 = canUseTool();
  if (!result512) return std::unexpected(std::move(result512.error()));
  if (std::move(*result512))
      {
    auto result513 = mouthPosition();
    if (!result513) return std::unexpected(std::move(result513.error()));
    m_songbook->keepAlive(instrumentKind, std::move(*result513));
  }
}
  return {};
}

std::expected<void, StarException> Npc::interact(InteractAction const&) {  return {};
}

std::expected<void, StarException> Npc::addEffectEmitters(StringSet const& emitters) {
  m_effectEmitter->addEffectSources("normal", emitters);
  return {};
}

std::expected<void, StarException> Npc::requestEmote(String const& emote) {
  if (!emote.empty()) {
    if (!HumanoidEmoteNames) return std::unexpected(HumanoidEmoteNames.error());
    auto result514 = HumanoidEmoteNames->get_left(emote);
    if (!result514)
      return std::unexpected(std::move(result514.error()));
    auto state = result514->get();
    if (state != HumanoidEmote::Idle && (m_emoteState == HumanoidEmote::Idle || m_emoteState == HumanoidEmote::Blink))
      addEmote(state);
  }
  return {};
}

ActorMovementController& Npc::movementController() {
  return *m_movementController;
}

StatusController& Npc::statusController() {
  return *m_statusController;
}

Songbook& Npc::songbook() {
  return *m_songbook;
}

void Npc::setCameraFocusEntity(std::optional<EntityId> const&) {
  // players only
}

std::expected<void, StarException> Npc::playEmote(HumanoidEmote emote) {
  addEmote(emote);
  return {};
}

std::expected<List<DamageSource>, StarException> Npc::damageSources() const {
  auto result515 = m_tools->damageSources();
if (!result515) return std::unexpected(std::move(result515.error()));
auto damageSources = std::move(*result515);

  if (m_damageOnTouch.get() && !m_npcVariant.touchDamageConfig.isNull()) {
    Json config = m_npcVariant.touchDamageConfig;
    {
  auto result516 = config.contains("poly");
  if (!result516) return std::unexpected(std::move(result516.error()));
  bool condition518 = static_cast<bool>(!std::move(*result516));
  if (condition518) {
    auto result517 = config.contains("line");
    if (!result517) return std::unexpected(std::move(result517.error()));
    condition518 = static_cast<bool>(!std::move(*result517));
  }
  if (condition518) {
        auto result519 = config.set("poly", jsonFromPolyF(m_movementController->collisionPoly()));
  if (!result519) return std::unexpected(std::move(result519.error()));
  config = std::move(*result519);
      }
}
    auto result520 = DamageSource::create(config);
if (!result520) return std::unexpected(std::move(result520.error()));
auto damageSource = std::move(*result520);
    if (std::holds_alternative<PolyF>(damageSource.damageArea))
      std::get<PolyF>(damageSource.damageArea).rotate(m_movementController->rotation());
    damageSource.damage *= m_statusController->stat("powerMultiplier");
    damageSources.push_back(damageSource);
  }

  for (auto& damageSource : damageSources) {
    damageSource.sourceEntityId = entityId();
    damageSource.team = getTeam();
  }

  return damageSources;
}

std::expected<List<PhysicsForceRegion>, StarException> Npc::forceRegions() const {
  return m_tools->forceRegions();
}


HumanoidIdentity const& Npc::identity() const {
  return m_npcVariant.humanoidIdentity;
}

std::expected<void, StarException> Npc::updateIdentity() {
  m_identityUpdated = true;
  auto result521 = humanoid();
if (!result521) return std::unexpected(std::move(result521.error()));
auto oldIdentity = std::move(*result521)->identity();
  if ((m_npcVariant.humanoidIdentity.species != oldIdentity.species) || (m_npcVariant.humanoidIdentity.imagePath != oldIdentity.imagePath)) {
    auto result522 = refreshHumanoidParameters();
if (!result522) return std::unexpected(std::move(result522.error()));

  } else {
    auto result523 = humanoid();
if (!result523) return std::unexpected(std::move(result523.error()));
auto result524 = std::move(*result523)->setIdentity(m_npcVariant.humanoidIdentity);
if (!result524) return std::unexpected(std::move(result524.error()));

  }
  return {};
}

std::expected<void, StarException> Npc::setIdentity(HumanoidIdentity identity) {
  m_npcVariant.humanoidIdentity = std::move(identity);
  auto result525 = updateIdentity();
if (!result525) return std::unexpected(std::move(result525.error()));

  return {};
}

std::expected<void, StarException> Npc::setHumanoidParameter(String key, std::optional<Json> value) {
  if (value.has_value())
    m_npcVariant.humanoidParameters.set(key, value.value_or(Json{}));
  else
    m_npcVariant.humanoidParameters.erase(key);

  auto elements = m_netHumanoid.netElements();
  auto result526 = elements.last();
if (!result526) return std::unexpected(std::move(result526.error()));
auto result527 = result526->get()->setHumanoidParameters(m_npcVariant.humanoidParameters);
if (!result527) return std::unexpected(std::move(result527.error()));

  return {};
}

std::optional<Json> Npc::getHumanoidParameter(String key) {
  return m_npcVariant.humanoidParameters.maybe(key);
}

std::expected<void, StarException> Npc::setHumanoidParameters(JsonObject parameters) {
  m_npcVariant.humanoidParameters = std::move(parameters);

  auto elements = m_netHumanoid.netElements();
  auto result528 = elements.last();
if (!result528) return std::unexpected(std::move(result528.error()));
auto result529 = result528->get()->setHumanoidParameters(m_npcVariant.humanoidParameters);
if (!result529) return std::unexpected(std::move(result529.error()));

  return {};
}


JsonObject Npc::getHumanoidParameters() {
  return m_npcVariant.humanoidParameters;
}

std::expected<void, StarException> Npc::setBodyDirectives(String const& directives)
{ m_npcVariant.humanoidIdentity.bodyDirectives = directives; auto result530 = updateIdentity();
if (!result530) return std::unexpected(std::move(result530.error()));
   return {};
}

std::expected<void, StarException> Npc::setEmoteDirectives(String const& directives)
{ m_npcVariant.humanoidIdentity.emoteDirectives = directives; auto result531 = updateIdentity();
if (!result531) return std::unexpected(std::move(result531.error()));
   return {};
}

std::expected<void, StarException> Npc::setHairGroup(String const& group)
{ m_npcVariant.humanoidIdentity.hairGroup = group; auto result532 = updateIdentity();
if (!result532) return std::unexpected(std::move(result532.error()));
   return {};
}

std::expected<void, StarException> Npc::setHairType(String const& type)
{ m_npcVariant.humanoidIdentity.hairType = type; auto result533 = updateIdentity();
if (!result533) return std::unexpected(std::move(result533.error()));
   return {};
}

std::expected<void, StarException> Npc::setHairDirectives(String const& directives)
{ m_npcVariant.humanoidIdentity.hairDirectives = directives; auto result534 = updateIdentity();
if (!result534) return std::unexpected(std::move(result534.error()));
   return {};
}

std::expected<void, StarException> Npc::setFacialHairGroup(String const& group)
{ m_npcVariant.humanoidIdentity.facialHairGroup = group; auto result535 = updateIdentity();
if (!result535) return std::unexpected(std::move(result535.error()));
   return {};
}

std::expected<void, StarException> Npc::setFacialHairType(String const& type)
{ m_npcVariant.humanoidIdentity.facialHairType = type; auto result536 = updateIdentity();
if (!result536) return std::unexpected(std::move(result536.error()));
   return {};
}

std::expected<void, StarException> Npc::setFacialHairDirectives(String const& directives)
{ m_npcVariant.humanoidIdentity.facialHairDirectives = directives; auto result537 = updateIdentity();
if (!result537) return std::unexpected(std::move(result537.error()));
   return {};
}

std::expected<void, StarException> Npc::setFacialMaskGroup(String const& group)
{ m_npcVariant.humanoidIdentity.facialMaskGroup = group; auto result538 = updateIdentity();
if (!result538) return std::unexpected(std::move(result538.error()));
   return {};
}

std::expected<void, StarException> Npc::setFacialMaskType(String const& type)
{ m_npcVariant.humanoidIdentity.facialMaskType = type; auto result539 = updateIdentity();
if (!result539) return std::unexpected(std::move(result539.error()));
   return {};
}

std::expected<void, StarException> Npc::setFacialMaskDirectives(String const& directives)
{ m_npcVariant.humanoidIdentity.facialMaskDirectives = directives; auto result540 = updateIdentity();
if (!result540) return std::unexpected(std::move(result540.error()));
   return {};
}

std::expected<void, StarException> Npc::setHair(String const& group, String const& type, String const& directives) {
  m_npcVariant.humanoidIdentity.hairGroup = group;
  m_npcVariant.humanoidIdentity.hairType = type;
  m_npcVariant.humanoidIdentity.hairDirectives = directives;
  auto result541 = updateIdentity();
if (!result541) return std::unexpected(std::move(result541.error()));

  return {};
}

std::expected<void, StarException> Npc::setFacialHair(String const& group, String const& type, String const& directives) {
  m_npcVariant.humanoidIdentity.facialHairGroup = group;
  m_npcVariant.humanoidIdentity.facialHairType = type;
  m_npcVariant.humanoidIdentity.facialHairDirectives = directives;
  auto result542 = updateIdentity();
if (!result542) return std::unexpected(std::move(result542.error()));

  return {};
}

std::expected<void, StarException> Npc::setFacialMask(String const& group, String const& type, String const& directives) {
  m_npcVariant.humanoidIdentity.facialMaskGroup = group;
  m_npcVariant.humanoidIdentity.facialMaskType = type;
  m_npcVariant.humanoidIdentity.facialMaskDirectives = directives;
  auto result543 = updateIdentity();
if (!result543) return std::unexpected(std::move(result543.error()));

  return {};
}

std::expected<void, StarException> Npc::setSpecies(String const& species) {
  m_npcVariant.humanoidIdentity.species = species;
  auto result544 = updateIdentity();
if (!result544) return std::unexpected(std::move(result544.error()));

  return {};
}

std::expected<void, StarException> Npc::setGender(Gender const& gender) {
  m_npcVariant.humanoidIdentity.gender = gender;
  auto result545 = updateIdentity();
if (!result545) return std::unexpected(std::move(result545.error()));

  return {};
}

std::expected<void, StarException> Npc::setPersonality(Personality const& personality) {
  m_npcVariant.humanoidIdentity.personality = personality;
  auto result546 = updateIdentity();
if (!result546) return std::unexpected(std::move(result546.error()));

  return {};
}

std::expected<void, StarException> Npc::setImagePath(std::optional<String> const& imagePath) {
  m_npcVariant.humanoidIdentity.imagePath = imagePath;
  auto result547 = updateIdentity();
if (!result547) return std::unexpected(std::move(result547.error()));

  return {};
}

std::expected<void, StarException> Npc::setFavoriteColor(Color color) {
  m_npcVariant.humanoidIdentity.color = color.toRgba();
  auto result548 = updateIdentity();
if (!result548) return std::unexpected(std::move(result548.error()));

  return {};
}

std::expected<void, StarException> Npc::setName(String const& name) {
  m_npcVariant.humanoidIdentity.name = name;
  auto result549 = updateIdentity();
if (!result549) return std::unexpected(std::move(result549.error()));

  return {};
}

void Npc::setDescription(String const& description) {
  m_npcVariant.description = description;
}

std::expected<std::shared_ptr<Humanoid>, StarException> Npc::humanoid() {
  auto elements = m_netHumanoid.netElements();
  auto last = elements.last();
  if (!last) return std::unexpected(std::move(last.error()));
  return last->get()->humanoid();
}
std::expected<std::shared_ptr<Humanoid>, StarException> Npc::humanoid() const {
  auto elements = m_netHumanoid.netElements();
  auto last = elements.last();
  if (!last) return std::unexpected(std::move(last.error()));
  return last->get()->humanoid();
}

std::expected<void, StarException> Npc::refreshHumanoidParameters() {
  auto result550 = this->root().speciesDatabase();
if (!result550) return std::unexpected(std::move(result550.error()));
auto speciesDatabase = std::move(*result550);
  auto result551 = speciesDatabase->species(m_npcVariant.humanoidIdentity.species);
if (!result551) return std::unexpected(std::move(result551.error()));
auto speciesDef = std::move(*result551);

  if (isMaster()) {
    m_refreshedHumanoidParameters.trigger();
    m_scriptedAnimationParameters.clear();
    m_netHumanoid.clearNetElements();
    auto result552 = NetHumanoid::create(this->root(), m_npcVariant.humanoidIdentity, m_npcVariant.humanoidParameters, m_npcVariant.uniqueHumanoidConfig ? m_npcVariant.humanoidConfig : Json());
if (!result552) return std::unexpected(std::move(result552.error()));
auto result553 = m_netHumanoid.addNetElement(std::move(*result552));
if (!result553) return std::unexpected(std::move(result553.error()));

    auto result554 = humanoid();
if (!result554) return std::unexpected(std::move(result554.error()));
m_deathParticleBurst.set(std::move(*result554)->defaultDeathParticles());
  }else {
    auto elements = m_netHumanoid.netElements();
    auto result555 = elements.last();
if (!result555) return std::unexpected(std::move(result555.error()));
m_npcVariant.humanoidParameters = result555->get()->humanoidParameters();
  }

  auto result556 = m_armor->diskStore();
if (!result556) return std::unexpected(std::move(result556.error()));
auto armor = std::move(*result556);
  m_armor->reset();
  auto result557 = m_armor->diskLoad(armor);
if (!result557) return std::unexpected(std::move(result557.error()));
  auto result558 = humanoid();
if (!result558) return std::unexpected(std::move(result558.error()));
auto result559 = m_armor->setupHumanoid(*std::move(*result558), forceNude());
if (!result559) return std::unexpected(std::move(result559.error()));


  auto result560 = humanoid();
if (!result560) return std::unexpected(std::move(result560.error()));
auto result561 = ActorMovementParameters::create(jsonMerge(std::move(*result560)->defaultMovementParameters(), m_npcVariant.movementParameters));
if (!result561) return std::unexpected(std::move(result561.error()));
auto result562 = m_movementController->resetBaseParameters(std::move(*result561));
if (!result562) return std::unexpected(std::move(result562.error()));


  if (inWorld()) {
    if (isMaster()) {
      if (m_scriptComponent.initialized()) {
        auto result563 = m_scriptComponent.removeCallbacks("animator");
if (!result563) return std::unexpected(std::move(result563.error()));

        auto result564 = humanoid();
if (!result564) return std::unexpected(std::move(result564.error()));
auto animator = (*result564)->networkedAnimator();
auto result565 = LuaBindings::makeNetworkedAnimatorCallbacks(animator->get());
if (!result565) return std::unexpected(std::move(result565.error()));
auto result566 = m_scriptComponent.addCallbacks("animator", std::move(*result565));
if (!result566) return std::unexpected(std::move(result566.error()));

        auto result567 = m_scriptComponent.invoke("refreshHumanoidParameters");
if (!result567) return std::unexpected(std::move(result567.error()));

      }
    }
    {
  auto result568 = world();
  if (!result568) return std::unexpected(std::move(result568.error()));
  if (result568->get().isClient() && m_scriptedAnimator.initialized()) {
        auto result569 = m_scriptedAnimator.uninit();
  if (!result569) return std::unexpected(std::move(result569.error()));

        auto result570 = m_scriptedAnimator.removeCallbacks("animationConfig");
  if (!result570) return std::unexpected(std::move(result570.error()));

        auto result571 = m_scriptedAnimator.removeCallbacks("config");
  if (!result571) return std::unexpected(std::move(result571.error()));

        auto result572 = m_scriptedAnimator.removeCallbacks("entity");
  if (!result572) return std::unexpected(std::move(result572.error()));


        auto result573 = humanoid();
  if (!result573) return std::unexpected(std::move(result573.error()));
  auto result574 = m_scriptedAnimator.setScripts(std::move(*result573)->animationScripts());
  if (!result574) return std::unexpected(std::move(result574.error()));

        auto result575 = humanoid();
  if (!result575) return std::unexpected(std::move(result575.error()));
  auto animator = (*result575)->networkedAnimator();
  auto result576 = LuaBindings::makeScriptedAnimatorCallbacks(animator->get(),
          [this](String const& name, Json const& defaultValue) -> Json {
            return m_scriptedAnimationParameters.value(name, defaultValue);
          });
  if (!result576) return std::unexpected(std::move(result576.error()));
  auto result577 = m_scriptedAnimator.addCallbacks("animationConfig", std::move(*result576));
  if (!result577) return std::unexpected(std::move(result577.error()));

        auto result578 = LuaBindings::makeConfigCallbacks([this](String const& name, Json const& def)
                { return m_npcVariant.scriptConfig.query(name, def); });
  if (!result578) return std::unexpected(std::move(result578.error()));
  auto result579 = m_scriptedAnimator.addCallbacks("config",
            std::move(*result578));
  if (!result579) return std::unexpected(std::move(result579.error()));

        auto result580 = LuaBindings::makeEntityCallbacks(*this);
  if (!result580) return std::unexpected(std::move(result580.error()));
  auto result581 = m_scriptedAnimator.addCallbacks("entity", std::move(*result580));
  if (!result581) return std::unexpected(std::move(result581.error()));

        auto result582 = world();
  if (!result582) return std::unexpected(std::move(result582.error()));
  auto result583 = m_scriptedAnimator.init(result582->get());
  if (!result583) return std::unexpected(std::move(result583.error()));

      }
}
  }
  return {};
}

bool Npc::forceNude() const {
  return m_statusController->statPositive("nude");
}

}

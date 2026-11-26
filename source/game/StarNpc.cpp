import star.inventory_types;
import star.json_extra;
import star.json;
import star.random;
import star.casting;
import star.bi_map;
import star.game_types;
import star.message_types;
import star.logging;
import star.interpolation;
import star.string;
import star.byte_array;
import star.list;
import star.vector;
import star.color;
import star.poly;
import star.rect;
import star.directives;
import star.lua;
import std;
import star.format;

import star.npc;
import star.item;
import star.item_database;
import star.item_descriptor;
import star.networked_animator;
import star.humanoid;
import star.damage_bar_entity;
import star.chat_action;
import star.lounging_entities;
import star.npc_database;
import star.effect_emitter;
import star.damage_types;
import star.world_geometry;
import star.status_types;
import star.damage;
import star.entity;
import star.interaction_types;
import star.quest_descriptor;
import star.force_regions;
import star.world;
import star.assets;
import star.root;
import star.item_drop;
import star.drawable;
import star.entity_rendering_types;
import star.particle;

import star.light_source;
import star.entity_rendering;
import star.armors;
import star.anchorable_entity;
import star.game_timers;
import star.actor_movement_controller;
import star.lua_components;
import star.lua_converters;
import star.lua_game_converters;
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
import star.emote_processor;

import star.songbook;
import star.armor_wearer;
import star.tool_user;
import star.entity_splash;

import star.net_compatibility;
import star.static_random;
import star.algorithm;

namespace Star {

std::expected<std::shared_ptr<Npc>, StarException> Npc::create(Root& root_context, ByteArray const& net_store, NetCompatibilityRules rules) {
  auto database = root_context.npc_database();
  if (!database) return std::unexpected(std::move(database.error()));
  auto variant = (*database)->read_npc_variant(net_store, rules);
  if (!variant) return std::unexpected(std::move(variant.error()));
  return create(root_context, std::move(*variant));
}

Npc::Npc(ConstructionKey, Root& root_context, NpcVariant&& npc_variant)
  : Entity(root_context), m_net_humanoid([&root_context] { return NetHumanoid::create(root_context); }),
    m_scripted_animator(root_context), m_npc_variant(std::move(npc_variant)), m_script_component(root_context) {}

std::expected<std::shared_ptr<Npc>, StarException> Npc::create(Root& root_context, NpcVariant npc_variant) {
  auto creature = std::make_shared<Npc>(ConstructionKey{}, root_context, std::move(npc_variant));
  auto initialized = creature->initialize();
  if (!initialized)
    return std::unexpected(std::move(initialized.error()));
  return creature;
}

std::expected<void, StarException> Npc::initialize() {
  auto const& npc_variant = m_npc_variant;
  auto wrapper = NetHumanoid::create(root(), npc_variant.humanoid_identity, npc_variant.humanoid_parameters,
      npc_variant.unique_humanoid_config ? npc_variant.humanoid_config : Json());
  if (!wrapper)
    return std::unexpected(std::move(wrapper.error()));
  m_applied_humanoid = (*wrapper)->humanoid();
  if (auto added = m_net_humanoid.add_net_element(std::move(*wrapper)); !added)
    return std::unexpected(std::move(added.error()));

  m_disable_worn_armor.set(npc_variant.disable_worn_armor);

  m_emote_state = HumanoidEmote::Idle;
  m_chat_message_updated = false;

  m_status_text.set({});
  m_display_nametag.set(false);

  auto result3 = this->root().assets();
if (!result3) return std::unexpected(std::move(result3.error()));
auto assets = std::move(*result3);

  auto result4 = assets->json("/npcs/npc.config:emoteCooldown");
if (!result4) return std::unexpected(std::move(result4.error()));
auto result5 = std::move(*result4).to_float();
if (!result5) return std::unexpected(std::move(result5.error()));
m_emote_cooldown_timer = GameTimer(std::move(*result5));
  auto result6 = assets->json("/npcs/npc.config:blinkInterval");
if (!result6) return std::unexpected(std::move(result6.error()));
auto result7 = json_to_vec2f(std::move(*result6));
if (!result7) return std::unexpected(std::move(result7.error()));
m_blink_interval = std::move(*result7);

  auto result8 = assets->json("/quests/quests.config:defaultIndicatorOffset");
if (!result8) return std::unexpected(std::move(result8.error()));
auto result9 = json_to_vec2f(std::move(*result8));
if (!result9) return std::unexpected(std::move(result9.error()));
m_quest_indicator_offset = std::move(*result9);

  if (npc_variant.overrides)
    {
  auto result10 = npc_variant.overrides.get_string("clientEntityMode", "ClientSlaveOnly");
  if (!result10) return std::unexpected(std::move(result10.error()));
  if (!ClientEntityModeNames) return std::unexpected(ClientEntityModeNames.error());
  auto result11 = ClientEntityModeNames->get_left(std::move(*result10));
  if (!result11) return std::unexpected(std::move(result11.error()));
  m_client_entity_mode = result11->get();
}

  m_is_interactive.set(false);

  m_shifting.set(false);
  m_damage_on_touch.set(false);

  m_drop_pools.set(npc_variant.drop_pools);

  setTeam(EntityDamageTeam(m_npc_variant.damage_team_type, m_npc_variant.damage_team));

  auto result12 = m_script_component.set_scripts(m_npc_variant.scripts);
if (!result12) return std::unexpected(std::move(result12.error()));

  m_script_component.set_update_delta(m_npc_variant.initial_script_delta);
  auto result13 = humanoid();
if (!result13) return std::unexpected(std::move(result13.error()));
auto result14 = ActorMovementParameters::create(json_merge(std::move(*result13)->default_movement_parameters(), m_npc_variant.movement_parameters));
if (!result14) return std::unexpected(std::move(result14.error()));
auto movement_parameters = std::move(*result14);
  if (!movement_parameters.physics_effect_categories)
    movement_parameters.physics_effect_categories = StringSet({"npc"});
  auto result15 = ActorMovementController::create(this->root(), movement_parameters);
if (!result15) return std::unexpected(std::move(result15.error()));
m_movement_controller = std::move(*result15);
  auto result16 = humanoid();
if (!result16) return std::unexpected(std::move(result16.error()));
m_death_particle_burst.set(std::move(*result16)->default_death_particles());

  auto result17 = StatusController::create(this->root(), m_npc_variant.status_controller_settings);
if (!result17) return std::unexpected(std::move(result17.error()));
m_status_controller = std::move(*result17);
  auto result18 = m_status_controller->setPersistentEffects("innate", m_npc_variant.innate_status_effects);
if (!result18) return std::unexpected(std::move(result18.error()));

  auto result19 = this->root().speciesDatabase();
if (!result19) return std::unexpected(std::move(result19.error()));
auto result21 = (*result19)->species(species());
if (!result21) return std::unexpected(std::move(result21.error()));
auto species_definition = std::move(*result21);
  auto result22 = m_status_controller->setPersistentEffects("species", species_definition->statusEffects());
if (!result22) return std::unexpected(std::move(result22.error()));

  m_status_controller->setStatusProperty("species", species());
  if (!m_status_controller->statusProperty("effectDirectives"))
    m_status_controller->setStatusProperty("effectDirectives", species_definition->effectDirectives());

auto result25 = Songbook::create(this->root(), species());
if (!result25) return std::unexpected(std::move(result25.error()));
m_songbook = std::move(*result25);

  auto result26 = EffectEmitter::create(this->root());
if (!result26) return std::unexpected(std::move(result26.error()));
m_effect_emitter = std::move(*result26);

  m_hit_damage_notification_limiter = 0;
  auto result27 = assets->json("/npcs/npc.config:hitDamageNotificationLimit");
if (!result27) return std::unexpected(std::move(result27.error()));
auto result28 = std::move(*result27).to_int();
if (!result28) return std::unexpected(std::move(result28.error()));
m_hit_damage_notification_limit = std::move(*result28);

  m_blink_cooldown_timer = GameTimer();

  auto result29 = ArmorWearer::create(this->root());
if (!result29) return std::unexpected(std::move(result29.error()));
m_armor = std::move(*result29);
  auto result30 = ToolUser::create(this->root());
if (!result30) return std::unexpected(std::move(result30.error()));
m_tools = std::move(*result30);

  m_aggressive.set(false);

  setPersistent(m_npc_variant.persistent);
  setKeepAlive(m_npc_variant.keep_alive);

  auto result31 = setup_net_states();
if (!result31) return std::unexpected(std::move(result31.error()));
  return {};
}

std::expected<std::shared_ptr<Npc>, StarException> Npc::create(Root& root_context, NpcVariant npc_variant, Json const& disk_store) {
  auto creature = create(root_context, std::move(npc_variant));
  if (!creature) return std::unexpected(std::move(creature.error()));
  auto loaded = (*creature)->load_state(disk_store);
  if (!loaded) return std::unexpected(std::move(loaded.error()));
  return creature;
}

std::expected<void, StarException> Npc::load_state(Json const& disk_store) {
  auto result32 = disk_store.get("movementController");
if (!result32) return std::unexpected(std::move(result32.error()));
auto result33 = m_movement_controller->load_state(std::move(*result32));
if (!result33) return std::unexpected(std::move(result33.error()));

  auto result34 = disk_store.get("statusController");
if (!result34) return std::unexpected(std::move(result34.error()));
auto result35 = m_status_controller->diskLoad(std::move(*result34));
if (!result35) return std::unexpected(std::move(result35.error()));

  auto result36 = disk_store.get("aimPosition");
if (!result36) return std::unexpected(std::move(result36.error()));
auto result37 = json_to_vec2f(std::move(*result36));
if (!result37) return std::unexpected(std::move(result37.error()));
auto aim_position = std::move(*result37);
  m_x_aim_position.set(aim_position[0]);
  m_y_aim_position.set(aim_position[1]);
  auto result38 = humanoid();
if (!result38) return std::unexpected(std::move(result38.error()));
auto result39 = disk_store.get_string("humanoidState");
if (!result39) return std::unexpected(std::move(result39.error()));
if (!Humanoid::state_names) return std::unexpected(Humanoid::state_names.error());
auto result40 = Humanoid::state_names->get_left(std::move(*result39));
if (!result40) return std::unexpected(std::move(result40.error()));
auto result41 = std::move(*result38)->set_state(result40->get());
if (!result41) return std::unexpected(std::move(result41.error()));

  auto result42 = humanoid();
if (!result42) return std::unexpected(std::move(result42.error()));
auto result43 = disk_store.get_string("humanoidEmoteState");
if (!result43) return std::unexpected(std::move(result43.error()));
if (!humanoid_emote_names) return std::unexpected(humanoid_emote_names.error());
auto result44 = humanoid_emote_names->get_left(std::move(*result43));
if (!result44) return std::unexpected(std::move(result44.error()));
auto result45 = std::move(*result42)->set_emote_state(result44->get());
if (!result45) return std::unexpected(std::move(result45.error()));

  auto result46 = disk_store.get_bool("isInteractive");
if (!result46) return std::unexpected(std::move(result46.error()));
m_is_interactive.set(std::move(*result46));
  auto result47 = disk_store.get_bool("shifting");
if (!result47) return std::unexpected(std::move(result47.error()));
m_shifting.set(std::move(*result47));
  auto result48 = disk_store.get_bool("damageOnTouch", false);
if (!result48) return std::unexpected(std::move(result48.error()));
m_damage_on_touch.set(std::move(*result48));

  auto result49 = disk_store.get("effectEmitter");
if (!result49) return std::unexpected(std::move(result49.error()));
auto result50 = m_effect_emitter->from_json(std::move(*result49));
if (!result50) return std::unexpected(std::move(result50.error()));


  auto result51 = disk_store.get("armor");
if (!result51) return std::unexpected(std::move(result51.error()));
auto result52 = m_armor->disk_load(std::move(*result51));
if (!result52) return std::unexpected(std::move(result52.error()));
  auto result53 = disk_store.get("tools");
if (!result53) return std::unexpected(std::move(result53.error()));
auto result54 = m_tools->diskLoad(std::move(*result53));
if (!result54) return std::unexpected(std::move(result54.error()));


  auto result55 = disk_store.get_bool("disableWornArmor");
if (!result55) return std::unexpected(std::move(result55.error()));
m_disable_worn_armor.set(std::move(*result55));

  auto result56 = disk_store.get_object("scriptStorage");
if (!result56) return std::unexpected(std::move(result56.error()));
auto result57 = m_script_component.set_script_storage(std::move(*result56));
if (!result57) return std::unexpected(std::move(result57.error()));


  auto result58 = disk_store.opt_string("uniqueId");
if (!result58) return std::unexpected(std::move(result58.error()));
set_unique_id(std::move(*result58));
  {
  auto result59 = disk_store.contains("team");
  if (!result59) return std::unexpected(std::move(result59.error()));
  if (std::move(*result59))
      {
    auto result60 = disk_store.get("team");
    if (!result60) return std::unexpected(std::move(result60.error()));
    auto result61 = EntityDamageTeam::create(std::move(*result60));
    if (!result61) return std::unexpected(std::move(result61.error()));
    setTeam(std::move(*result61));
  }
}

  auto result62 = disk_store.opt_string("deathParticleBurst");
if (!result62) return std::unexpected(std::move(result62.error()));
m_death_particle_burst.set(std::move(*result62));

  auto result63 = disk_store.get_array("dropPools");
if (!result63) return std::unexpected(std::move(result63.error()));
auto result64 = json_to_string_list(std::move(*result63));
if (!result64) return std::unexpected(std::move(result64.error()));
m_drop_pools.set(std::move(*result64));

  m_blink_cooldown_timer = GameTimer();

  auto result65 = disk_store.get_bool("aggressive");
if (!result65) return std::unexpected(std::move(result65.error()));
m_aggressive.set(std::move(*result65));
  return {};
}

std::expected<Json, StarException> Npc::disk_store() const {
  auto result66 = this->root().npc_database();
if (!result66) return std::unexpected(std::move(result66.error()));
auto result67 = std::move(*result66)->write_npc_variant_to_json(m_npc_variant);
if (!result67) return std::unexpected(std::move(result67.error()));
auto result68 = m_movement_controller->store_state();
if (!result68) return std::unexpected(std::move(result68.error()));
auto result69 = m_status_controller->diskStore();
if (!result69) return std::unexpected(std::move(result69.error()));
auto result70 = m_armor->disk_store();
if (!result70) return std::unexpected(std::move(result70.error()));
auto result71 = m_tools->diskStore();
if (!result71) return std::unexpected(std::move(result71.error()));
auto result72 = humanoid();
if (!result72) return std::unexpected(std::move(result72.error()));
if (!Humanoid::state_names) return std::unexpected(Humanoid::state_names.error());
auto result73 = Humanoid::state_names->get_right(std::move(*result72)->state());
if (!result73) return std::unexpected(std::move(result73.error()));
auto result74 = humanoid();
if (!result74) return std::unexpected(std::move(result74.error()));
if (!humanoid_emote_names) return std::unexpected(humanoid_emote_names.error());
auto result75 = humanoid_emote_names->get_right(std::move(*result74)->emote_state());
if (!result75) return std::unexpected(std::move(result75.error()));
auto result76 = m_effect_emitter->to_json();
if (!result76) return std::unexpected(std::move(result76.error()));
auto result77 = m_script_component.get_script_storage();
if (!result77) return std::unexpected(std::move(result77.error()));
auto result78 = getTeam().to_json();
if (!result78) return std::unexpected(std::move(result78.error()));
return JsonObject{
    {"npcVariant", std::move(*result67)},
    {"movementController", std::move(*result68)},
    {"statusController", std::move(*result69)},
    {"armor", std::move(*result70)},
    {"tools", std::move(*result71)},
    {"aimPosition", json_from_vec2f({m_x_aim_position.get(), m_y_aim_position.get()})},
    {"humanoidState", result73->get()},
    {"humanoidEmoteState", result75->get()},
    {"isInteractive", m_is_interactive.get()},
    {"shifting", m_shifting.get()},
    {"damageOnTouch", m_damage_on_touch.get()},
    {"effectEmitter", std::move(*result76)},
    {"disableWornArmor", m_disable_worn_armor.get()},
    {"scriptStorage", std::move(*result77)},
    {"uniqueId", json_from_optional(uniqueId())},
    {"team", std::move(*result78)},
    {"deathParticleBurst", json_from_optional(m_death_particle_burst.get())},
    {"dropPools", m_drop_pools.get().transformed(Construct<Json>())},
    {"aggressive", m_aggressive.get()}
  };
}

std::expected<ByteArray, StarException> Npc::net_store(NetCompatibilityRules rules) {
  auto result79 = this->root().npc_database();
if (!result79) return std::unexpected(std::move(result79.error()));
return std::move(*result79)->write_npc_variant(m_npc_variant, rules);
}

EntityType Npc::entity_type() const {
  return EntityType::Npc;
}

ClientEntityMode Npc::client_entity_mode() const {
  return m_client_entity_mode;
}

std::expected<void, StarException> Npc::init(World& world, EntityId entity_id, EntityMode mode) {
  auto result80 = Entity::init(world, entity_id, mode);
if (!result80) return std::unexpected(std::move(result80.error()));

  m_movement_controller->init(world);
  m_movement_controller->set_ignore_physics_entities({entity_id});
  auto result81 = m_status_controller->init(*this, *m_movement_controller);
if (!result81) return std::unexpected(std::move(result81.error()));

  auto result82 = m_tools->init(*this);
if (!result82) return std::unexpected(std::move(result82.error()));


  auto result83 = humanoid();
if (!result83) return std::unexpected(std::move(result83.error()));
auto result84 = m_armor->setup_humanoid(*std::move(*result83), force_nude());
if (!result84) return std::unexpected(std::move(result84.error()));


  if (isMaster()) {
    auto result85 = m_movement_controller->reset_anchor_state();
if (!result85) return std::unexpected(std::move(result85.error()));


    auto result86 = this->root().itemDatabase();
if (!result86) return std::unexpected(std::move(result86.error()));
auto item_database = std::move(*result86);
    for (auto const& item : m_npc_variant.items)
      {
  auto result87 = set_item_slot(item.first, item.second);
  if (!result87) return std::unexpected(std::move(result87.error()));

}
    auto result88 = make_npc_callbacks();
if (!result88) return std::unexpected(std::move(result88.error()));
auto result89 = m_script_component.add_callbacks("npc", std::move(*result88));
if (!result89) return std::unexpected(std::move(result89.error()));

    auto result90 = LuaBindings::makeConfigCallbacks([this](String const& name, Json const& def)
            { return m_npc_variant.script_config.query(name, def); });
if (!result90) return std::unexpected(std::move(result90.error()));
auto result91 = m_script_component.add_callbacks("config",
        std::move(*result90));
if (!result91) return std::unexpected(std::move(result91.error()));

    auto result92 = LuaBindings::makeEntityCallbacks(*this);
if (!result92) return std::unexpected(std::move(result92.error()));
auto result93 = m_script_component.add_callbacks("entity", std::move(*result92));
if (!result93) return std::unexpected(std::move(result93.error()));

    auto result94 = LuaBindings::makeStatusControllerCallbacks(*m_status_controller);
if (!result94) return std::unexpected(std::move(result94.error()));
auto result95 = m_script_component.add_callbacks("status", std::move(*result94));
if (!result95) return std::unexpected(std::move(result95.error()));

    auto result96 = LuaBindings::make_behavior_callbacks(this->root(), m_behaviors);
if (!result96) return std::unexpected(std::move(result96.error()));
auto result97 = m_script_component.add_callbacks("behavior", std::move(*result96));
if (!result97) return std::unexpected(std::move(result97.error()));

    auto result98 = LuaBindings::makeSongbookCallbacks(*m_songbook);
if (!result98) return std::unexpected(std::move(result98.error()));
auto result99 = m_script_component.add_callbacks("songbook", std::move(*result98));
if (!result99) return std::unexpected(std::move(result99.error()));

    if (auto bound = bind_actor_animator(); !bound)
      return std::unexpected(std::move(bound.error()));

    auto result103 = m_script_component.add_actor_movement_callbacks(*m_movement_controller);
if (!result103) return std::unexpected(std::move(result103.error()));

    auto result104 = recover_lua_script_error(m_script_component.init(world));
if (!result104) return std::unexpected(std::move(result104.error()));

  }
  if (world.isClient()) {
    if (auto bound = bind_presentation_animator(); !bound)
      return std::unexpected(std::move(bound.error()));

    auto result110 = LuaBindings::makeConfigCallbacks([this](String const& name, Json const& def)
            { return m_npc_variant.script_config.query(name, def); });
if (!result110) return std::unexpected(std::move(result110.error()));
auto result111 = m_scripted_animator.add_callbacks("config",
        std::move(*result110));
if (!result111) return std::unexpected(std::move(result111.error()));

    auto result112 = LuaBindings::makeEntityCallbacks(*this);
if (!result112) return std::unexpected(std::move(result112.error()));
auto result113 = m_scripted_animator.add_callbacks("entity", std::move(*result112));
if (!result113) return std::unexpected(std::move(result113.error()));

    auto result114 = recover_lua_script_error(m_scripted_animator.init(world));
if (!result114) return std::unexpected(std::move(result114.error()));

  }

  if (auto result = update_humanoid_directives(); !result)
    return std::unexpected(std::move(result.error()));
  auto pose = humanoid();
  if (!pose)
    return std::unexpected(std::move(pose.error()));
  return (*pose)->prepare_animator_pose();
}

std::expected<void, StarException> Npc::uninit() {
  if (isMaster()) {
    auto result115 = m_movement_controller->reset_anchor_state();
if (!result115) return std::unexpected(std::move(result115.error()));

    auto result116 = recover_lua_script_error(m_script_component.uninit());
if (!result116) return std::unexpected(std::move(result116.error()));

    auto result117 = m_script_component.remove_callbacks("npc");
if (!result117) return std::unexpected(std::move(result117.error()));

    auto result118 = m_script_component.remove_callbacks("config");
if (!result118) return std::unexpected(std::move(result118.error()));

    auto result119 = m_script_component.remove_callbacks("entity");
if (!result119) return std::unexpected(std::move(result119.error()));

    auto result120 = m_script_component.remove_callbacks("status");
if (!result120) return std::unexpected(std::move(result120.error()));

    auto result121 = m_script_component.remove_callbacks("behavior");
if (!result121) return std::unexpected(std::move(result121.error()));

    auto result122 = m_script_component.remove_callbacks("songbook");
if (!result122) return std::unexpected(std::move(result122.error()));

    auto result123 = m_script_component.remove_callbacks("animator");
if (!result123) return std::unexpected(std::move(result123.error()));
    if (!m_script_component.initialized())
      m_actor_callback_humanoid.reset();

    auto result124 = m_script_component.remove_actor_movement_callbacks();
if (!result124) return std::unexpected(std::move(result124.error()));

  }
  {
  auto result125 = world();
  if (!result125) return std::unexpected(std::move(result125.error()));
  if (result125->get().isClient()) {
      auto result126 = recover_lua_script_error(m_scripted_animator.uninit());
  if (!result126) return std::unexpected(std::move(result126.error()));

      auto result127 = m_scripted_animator.remove_callbacks("animationConfig");
  if (!result127) return std::unexpected(std::move(result127.error()));
      if (!m_scripted_animator.initialized())
        m_presentation_callback_humanoid.reset();

      auto result128 = m_scripted_animator.remove_callbacks("config");
  if (!result128) return std::unexpected(std::move(result128.error()));

      auto result129 = m_scripted_animator.remove_callbacks("entity");
  if (!result129) return std::unexpected(std::move(result129.error()));

    }
}
  auto result130 = m_tools->uninit();
if (!result130) return std::unexpected(std::move(result130.error()));

  auto result131 = m_status_controller->uninit();
if (!result131) return std::unexpected(std::move(result131.error()));

  m_movement_controller->uninit();
  auto result132 = Entity::uninit();
if (!result132) return std::unexpected(std::move(result132.error()));

  return {};
}

std::expected<void, StarException> Npc::enable_interpolation(float extrapolation_hint) {
  auto result133 = m_net_group.enable_net_interpolation(extrapolation_hint);
if (!result133) return std::unexpected(std::move(result133.error()));

  return {};
}

std::expected<void, StarException> Npc::disable_interpolation() {
  auto result134 = m_net_group.disable_net_interpolation();
if (!result134) return std::unexpected(std::move(result134.error()));

  return {};
}

Vec2F Npc::position() const {
  return m_movement_controller->position();
}

RectF Npc::meta_bound_box() const {
  return RectF(-4, -4, 4, 4);
}

std::expected<Vec2F, StarException> Npc::mouth_offset(bool ignore_adjustments) const {
  auto body = humanoid();
  if (!body) return std::unexpected(std::move(body.error()));
  auto offset = (*body)->mouth_offset(ignore_adjustments);
  if (!offset) return std::unexpected(std::move(offset.error()));
  (*offset)[0] *= numerical_direction((*body)->facing_direction());
  return offset;
}

std::expected<Vec2F, StarException> Npc::feet_offset() const {
  auto body = humanoid();
  if (!body) return std::unexpected(std::move(body.error()));
  auto offset = (*body)->feet_offset();
  if (!offset) return std::unexpected(std::move(offset.error()));
  (*offset)[0] *= numerical_direction((*body)->facing_direction());
  return offset;
}

std::expected<Vec2F, StarException> Npc::head_armor_offset() const {
  auto body = humanoid();
  if (!body) return std::unexpected(std::move(body.error()));
  auto offset = (*body)->head_armor_offset();
  if (!offset) return std::unexpected(std::move(offset.error()));
  (*offset)[0] *= numerical_direction((*body)->facing_direction());
  return offset;
}

std::expected<Vec2F, StarException> Npc::chest_armor_offset() const {
  auto body = humanoid();
  if (!body) return std::unexpected(std::move(body.error()));
  auto offset = (*body)->chest_armor_offset();
  if (!offset) return std::unexpected(std::move(offset.error()));
  (*offset)[0] *= numerical_direction((*body)->facing_direction());
  return offset;
}

std::expected<Vec2F, StarException> Npc::back_armor_offset() const {
  auto body = humanoid();
  if (!body) return std::unexpected(std::move(body.error()));
  auto offset = (*body)->back_armor_offset();
  if (!offset) return std::unexpected(std::move(offset.error()));
  (*offset)[0] *= numerical_direction((*body)->facing_direction());
  return offset;
}

std::expected<Vec2F, StarException> Npc::legs_armor_offset() const {
  auto body = humanoid();
  if (!body) return std::unexpected(std::move(body.error()));
  auto offset = (*body)->legs_armor_offset();
  if (!offset) return std::unexpected(std::move(offset.error()));
  (*offset)[0] *= numerical_direction((*body)->facing_direction());
  return offset;
}

RectF Npc::collision_area() const {
  return m_movement_controller->collision_poly().bounding_box();
}

std::expected<std::pair<ByteArray, std::uint64_t>, StarException> Npc::write_net_state(std::uint64_t from_version, NetCompatibilityRules rules) {
  // client-side npcs error nearby vanilla NPC scripts because callScriptedEntity
  // for now, scrungle the collision poly to avoid their queries. hacky :(
  {
  bool condition166 = static_cast<bool>(m_npc_variant.overrides);
  if (condition166) {
    auto result165 = m_npc_variant.overrides.get_bool("overrideNetPoly", false);
    if (!result165) return std::unexpected(std::move(result165.error()));
    condition166 = static_cast<bool>(std::move(*result165));
  }
  if (condition166) {
      if (auto mode = entityMode()) {
        if (auto owner = connection_for_entity(entityId()); *mode == EntityMode::Master && owner && *owner != server_connection_id) {
          PolyF poly = m_movement_controller->collision_poly();
          m_movement_controller->set_collision_poly({ { 0.0f, -3.402823466e+38F }});
          auto result = m_net_group.write_net_state(from_version, rules);
          m_movement_controller->set_collision_poly(poly);
          return result;
        }
      }
    }
}

  return m_net_group.write_net_state(from_version, rules);
}

std::expected<void, StarException> Npc::read_net_state(ByteArray const& data, float interpolation_time, NetCompatibilityRules rules) {
  return m_net_group.read_net_state(data, interpolation_time, rules);
}

std::expected<String, StarException> Npc::description() const {
  return m_npc_variant.description.value_or("Some funny looking person");
}

String Npc::species() const {
  return m_npc_variant.humanoid_identity.species;
}

Gender Npc::gender() const {
  return m_npc_variant.humanoid_identity.gender;
}

String Npc::npc_type() const {
  return m_npc_variant.type_name;
}

std::expected<Json, StarException> Npc::script_config_parameter(String const& parameter_name, Json const& default_value) const {
  return m_npc_variant.script_config.query(parameter_name, default_value);
}

std::expected<std::optional<HitType>, StarException> Npc::query_hit(DamageSource const& source) const {
  if (!inWorld() || !m_status_controller->resourcePositive("health") || m_status_controller->statPositive("invulnerable"))
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
  auto result170 = source.intersects_with_poly(result169->get().geometry(), m_movement_controller->collision_body());
  if (!result170) return std::unexpected(std::move(result170.error()));
  if (std::move(*result170))
      return HitType::Hit;
}

  return {};
}

std::expected<std::optional<PolyF>, StarException> Npc::hit_poly() const {
  return m_movement_controller->collision_body();
}

std::expected<List<DamageNotification>, StarException> Npc::apply_damage(DamageRequest const& damage) {
  if (!inWorld())
    return {};

  auto result171 = m_status_controller->apply_damage_request(damage);
if (!result171) return std::unexpected(std::move(result171.error()));
auto notifications = std::move(*result171);

  float total_damage = 0.0f;
  for (auto const& notification : notifications)
    total_damage += notification.health_lost;

  if (total_damage > 0 && m_hit_damage_notification_limiter < m_hit_damage_notification_limit) {
    auto result172 = recover_lua_script_error(m_script_component.invoke("damage", JsonObject{
        {"sourceId", damage.source_entity_id},
        {"damage", total_damage},
        {"sourceDamage", damage.damage},
        {"sourceKind", damage.damage_source_kind}
      }));
if (!result172) return std::unexpected(std::move(result172.error()));

    m_hit_damage_notification_limiter++;
  }

  return notifications;
}

std::expected<List<DamageNotification>, StarException> Npc::self_damage_notifications() {
  return m_status_controller->pull_self_damage_notifications();
}

std::expected<bool, StarException> Npc::should_destroy() const {
  auto result = recover_lua_script_error(m_script_component.invoke<bool>("shouldDie"));
  if (!result)
    return std::unexpected(std::move(result.error()));
  if (*result)
    return **result;
  return !m_status_controller->resourcePositive("health") || m_script_component.error();
}

std::expected<void, StarException> Npc::destroy(std::optional<std::reference_wrapper<RenderCallback>> render_callback) {
  auto result174 = recover_lua_script_error(m_script_component.invoke("die"));
if (!result174) return std::unexpected(std::move(result174.error()));


  if (isMaster() && !m_drop_pools.get().empty()) {
    auto result175 = this->root().treasureDatabase();
if (!result175) return std::unexpected(std::move(result175.error()));
auto treasure_database = std::move(*result175);
    auto dropped = [&]() -> std::expected<void, StarException> {
      {
        auto result176 = static_random_from(m_drop_pools.get(), m_npc_variant.seed);
        if (!result176)
          return std::unexpected(std::move(result176.error()));
        auto result177 = treasure_database->createTreasure(result176->get(), m_npc_variant.level);
        if (!result177)
          return std::unexpected(std::move(result177.error()));
        for (auto const& treasure_item : std::move(*result177)) {
          auto result178 = world();
          if (!result178)
            return std::unexpected(std::move(result178.error()));
          auto result179 = ItemDrop::create_randomized_drop(this->root(), treasure_item, position());
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
      auto result181 = this->root().logger().warn_exception(
        Star::format("Failed to create treasure for NPC '{}'", npc_type()), ExceptionDiagnostic(e));
      if (!result181)
        return std::unexpected(std::move(result181.error()));
    }
  }

  if (render_callback && m_death_particle_burst.get())
    {
  auto result182 = humanoid();
  if (!result182) return std::unexpected(std::move(result182.error()));
  auto result183 = std::move(*result182)->particles(*m_death_particle_burst.get());
  if (!result183) return std::unexpected(std::move(result183.error()));
  auto result184 = render_callback->get().add_particles(std::move(*result183), position());
  if (!result184) return std::unexpected(std::move(result184.error()));
}

  m_songbook->stop();
  return {};
}

std::expected<void, StarException> Npc::damaged_other(DamageNotification const& damage) {
  if (inWorld() && isMaster())
    m_status_controller->damaged_other(damage);
  return {};
}

std::expected<void, StarException> Npc::update(float dt, std::uint64_t) {
  if (!inWorld())
    return {};

  m_movement_controller->set_timestep(dt);

  if (isMaster()) {
    if (auto updated = recover_lua_script_error(m_script_component.update(m_script_component.update_dt(dt))); !updated)
      return std::unexpected(std::move(updated.error()));

    auto conflict = inConflictingLoungeAnchor();
    if (!conflict)
      return std::unexpected(std::move(conflict.error()));
    if (*conflict) {
      if (auto reset = m_movement_controller->reset_anchor_state(); !reset)
        return std::unexpected(std::move(reset.error()));
    }

    auto anchor = m_movement_controller->entity_anchor();
    if (auto lounge_anchor = anchor ? as_ref<LoungeAnchor>(*anchor) : std::nullopt) {
      auto const& lounge = lounge_anchor->get();
      if (lounge.emote) {
        if (auto emote = request_emote(*lounge.emote); !emote)
          return emote;
      }
      if (lounge.dance) {
        if (auto dance = set_dance(lounge.dance); !dance)
          return dance;
      }
      if (auto effects = m_status_controller->setPersistentEffects("lounging", lounge.statusEffects); !effects)
        return effects;
      m_effect_emitter->add_effect_sources("normal", lounge.effectEmitters);

      auto state = Humanoid::Idle;
      switch (lounge.orientation) {
        case LoungeOrientation::Sit: state = Humanoid::Sit; break;
        case LoungeOrientation::Lay: state = Humanoid::Lay; break;
        default: break;
      }
      auto pose = humanoid();
      if (!pose)
        return std::unexpected(std::move(pose.error()));
      if (auto applied = (*pose)->set_state(state); !applied)
        return applied;
    } else if (auto effects = m_status_controller->setPersistentEffects("lounging", {}); !effects) {
      return effects;
    }

    if (auto effects = m_armor->effects(*m_effect_emitter); !effects)
      return std::unexpected(std::move(effects.error()));
    if (auto effects = m_tools->effects(*m_effect_emitter); !effects)
      return std::unexpected(std::move(effects.error()));
    if (auto synced = synchronize_equipment_status_effects(*m_armor, *m_tools, *m_status_controller, m_disable_worn_armor.get()); !synced)
      return std::unexpected(std::move(synced.error()));

    if (auto moved = m_movement_controller->tick_master(dt); !moved)
      return std::unexpected(std::move(moved.error()));
    if (auto updated = m_status_controller->tickMaster(dt); !updated)
      return std::unexpected(std::move(updated.error()));
    if (auto updated = tick_shared(dt); !updated)
      return updated;

    // Movement can release the anchor; recheck before applying locomotion.
    if (!is<LoungeAnchor>(m_movement_controller->entity_anchor())) {
      auto const& movement = *m_movement_controller;
      auto state = Humanoid::Idle;
      if (movement.ground_movement()) {
        if (movement.running())
          state = Humanoid::Run;
        else if (movement.walking())
          state = Humanoid::Walk;
        else if (movement.crouching())
          state = Humanoid::Duck;
      } else if (movement.liquid_movement()) {
        state = std::abs(movement.x_velocity()) > 0 ? Humanoid::Swim : Humanoid::SwimIdle;
      } else {
        state = movement.y_velocity() > 0 ? Humanoid::Jump : Humanoid::Fall;
      }
      auto pose = humanoid();
      if (!pose)
        return std::unexpected(std::move(pose.error()));
      if (auto applied = (*pose)->set_state(state); !applied)
        return applied;
    }

    if (m_emote_cooldown_timer.tick(dt))
      m_emote_state = HumanoidEmote::Idle;
    m_dance.tick(dt);

    if (m_chat_message_updated) {
      auto state = EmoteProcessor::detect_chat_emote(root(), m_chat_message.get());
      if (!state) return std::unexpected(std::move(state.error()));
      if (*state != HumanoidEmote::Idle) {
        if (auto emote = add_emote(*state); !emote)
          return emote;
      }
      m_chat_message_updated = false;
    }

    if (m_blink_cooldown_timer.tick(dt)) {
      auto interval = root().random().uniform_float(m_blink_interval[0], m_blink_interval[1]);
      if (!interval)
        return std::unexpected(std::move(interval.error()));
      m_blink_cooldown_timer = GameTimer(*interval);
      if (m_emote_state == HumanoidEmote::Idle) {
        if (auto emote = add_emote(HumanoidEmote::Blink); !emote)
          return emote;
      }
    }

    auto pose = humanoid();
    if (!pose)
      return std::unexpected(std::move(pose.error()));
    if (auto applied = (*pose)->set_emote_state(m_emote_state); !applied)
      return applied;
    if (auto applied = (*pose)->set_dance(m_dance.name); !applied)
      return applied;
  } else {
    if (auto interpolated = m_net_group.tick_net_interpolation(dt); !interpolated)
      return std::unexpected(std::move(interpolated.error()));
    if (auto moved = m_movement_controller->tick_slave(dt); !moved)
      return std::unexpected(std::move(moved.error()));
    if (auto updated = m_status_controller->tickSlave(dt); !updated)
      return std::unexpected(std::move(updated.error()));
    if (auto updated = tick_shared(dt); !updated)
      return updated;
  }

  auto current_world = world();
  if (!current_world)
    return std::unexpected(std::move(current_world.error()));
  if (current_world->get().isClient())
    root().spatialLogger().log_poly("world", m_movement_controller->collision_body(), {0, 255, 0, 255});
  return {};
}

std::uint64_t Npc::presentation_epoch() const {
  return Entity::presentation_epoch() + m_movement_controller->presentation_epoch();
}

std::expected<void, StarException> Npc::collect_render_events(RenderCallback& render_callback) {
  auto result246 = render_callback.add_particles(m_humanoid_dynamic_target.pull_new_particles());
if (!result246) return std::unexpected(std::move(result246.error()));
  auto result247 = render_callback.add_audios(m_humanoid_dynamic_target.pull_new_audios());
if (!result247) return std::unexpected(std::move(result247.error()));
  auto result250 = render_callback.add_particles(m_status_controller->pullNewParticles());
if (!result250) return std::unexpected(std::move(result250.error()));
  auto result251 = render_callback.add_audios(m_status_controller->pullNewAudios());
if (!result251) return std::unexpected(std::move(result251.error()));

  auto current_world = world();
  if (!current_world) return std::unexpected(std::move(current_world.error()));
  auto splash = m_npc_variant.splash_config.generate_particles(position(), m_movement_controller->velocity(), current_world->get());
  if (!splash) return std::unexpected(std::move(splash.error()));
  if (auto submitted = render_callback.add_particles(std::move(*splash)); !submitted)
    return std::unexpected(std::move(submitted.error()));
  auto result261 = m_effect_emitter->collect_render_events(render_callback);
if (!result261) return std::unexpected(std::move(result261.error()));

  auto result262 = m_songbook->collectRenderEvents(render_callback);
if (!result262) return std::unexpected(std::move(result262.error()));
  return m_tools->collectRenderEvents(render_callback);
}

std::expected<void, StarException> Npc::update_humanoid_directives() {
  DirectivesGroup directives;
  Vec2F scale = Vec2F::filled(1.f);
  for (auto const& entry : m_status_controller->parentDirectives().list()) {
    auto extracted = Humanoid::extract_scale_from_directives(entry);
    if (!extracted)
      return std::unexpected(std::move(extracted.error()));
    scale = scale.piecewise_multiply(extracted->first);
    directives.append(extracted->second);
  }
  auto pose = humanoid();
  if (!pose)
    return std::unexpected(std::move(pose.error()));
  (*pose)->set_scale(scale);
  m_presentation_humanoid_directives = std::move(directives);
  return {};
}

std::expected<void, StarException> Npc::render(RenderCallback& render_callback) {
  auto render_layer = render_layer_npc;
  auto anchor = m_movement_controller->entity_anchor();
  if (auto lounge_anchor = anchor ? as_ref<LoungeAnchor>(*anchor) : std::nullopt)
    render_layer = lounge_anchor->get().loungeRenderLayer;

  auto interpolation = render_callback.presentation().interpolation();
  auto pose = humanoid();
  if (!pose)
    return std::unexpected(std::move(pose.error()));
  if (auto setup = m_tools->setupHumanoidHandItemDrawables(**pose, interpolation); !setup)
    return setup;
  auto drawables = (*pose)->render(true, true, interpolation);
  if (!drawables)
    return std::unexpected(std::move(drawables.error()));

  auto origin = position();
  auto const& directives = m_presentation_humanoid_directives;
  std::shared_ptr<ImageMetadataDatabase const> metadata;
  for (auto& drawable : *drawables) {
    drawable.translate(origin);
    if (!directives.empty() && drawable.is_image()) {
      if (!metadata) {
        auto database = root().image_metadata_database();
        if (!database)
          return std::unexpected(std::move(database.error()));
        metadata = std::move(*database);
      }
      if (auto applied = drawable.image_part().add_directives_group(*metadata, directives, true); !applied)
        return std::unexpected(std::move(applied.error()));
    }
    if (auto submitted = render_callback.add_drawable(std::move(drawable), render_layer); !submitted)
      return submitted;
  }

  auto status_drawables = m_status_controller->drawables(interpolation);
  if (!status_drawables)
    return std::unexpected(std::move(status_drawables.error()));
  if (auto submitted = render_callback.add_drawables(std::move(*status_drawables), render_layer); !submitted)
    return submitted;

  auto tool_in_range = in_tool_range();
  if (!tool_in_range)
    return std::unexpected(std::move(tool_in_range.error()));
  if (auto rendered = m_tools->render(render_callback, *tool_in_range, m_shifting.get(), render_layer); !rendered)
    return rendered;

  auto aim = aim_position();
  if (!aim)
    return std::unexpected(std::move(aim.error()));
  auto preview_in_range = in_tool_range();
  if (!preview_in_range)
    return std::unexpected(std::move(preview_in_range.error()));
  auto previews = m_tools->renderObjectPreviews(*aim, walking_direction(), *preview_in_range, favorite_color());
  if (!previews)
    return std::unexpected(std::move(previews.error()));
  return render_callback.add_drawables(std::move(*previews), render_layer);
}

std::expected<void, StarException> Npc::render_light_sources(RenderCallback& render_callback) {
  auto lights = presentation_light_sources(render_callback.presentation().interpolation());
  if (!lights)
    return std::unexpected(std::move(lights.error()));
  return render_callback.add_light_sources(*lights);
}

void Npc::set_position(Vec2F const& pos) {
  resetPresentation();
  m_movement_controller->set_position(pos);
}

std::expected<float, StarException> Npc::max_health() const {
  auto maximum = m_status_controller->resourceMax("health");
  if (!maximum) return std::unexpected(std::move(maximum.error()));
  if (!*maximum) return std::unexpected(NpcException("Health resource has no maximum"));
  return **maximum;
}

float Npc::health() const {
  return m_status_controller->resource("health");
}

DamageBarType Npc::damage_bar() const {
  return DamageBarType::Default;
}

std::expected<List<Drawable>, StarException> Npc::portrait(PortraitMode mode) const {
  auto result265 = humanoid();
if (!result265) return std::unexpected(std::move(result265.error()));
return std::move(*result265)->render_portrait(mode);
}

std::expected<String, StarException> Npc::name() const {
  return m_npc_variant.humanoid_identity.name;
}

std::optional<String> Npc::status_text() const {
  return m_status_text.get();
}

bool Npc::display_nametag() const {
  return m_display_nametag.get();
}

std::expected<Vec3B, StarException> Npc::nametag_color() const {
  return m_npc_variant.nametag_color;
}

std::expected<Vec2F, StarException> Npc::nametag_origin() const {
  return mouth_position(false);
}

std::expected<String, StarException> Npc::nametag() const {
  return name();
}

bool Npc::aggressive() const {
  return m_aggressive.get();
}

std::expected<std::optional<LuaValue>, StarException> Npc::call_script(String const& func, LuaVariadic<LuaValue> const& args) {
  return recover_lua_script_error(m_script_component.invoke(func, args));
}

std::expected<std::optional<LuaValue>, StarException> Npc::eval_script(String const& code) {
  return recover_lua_script_error(m_script_component.eval(code));
}

std::expected<Vec2F, StarException> Npc::get_absolute_position(Vec2F relative_position) const {
  {
  auto result266 = humanoid();
  if (!result266) return std::unexpected(std::move(result266.error()));
  if (std::move(*result266)->facing_direction() == Direction::Left)
      relative_position[0] *= -1;
}
  return m_movement_controller->position() + relative_position;
}

std::expected<void, StarException> Npc::tick_shared(float dt) {
  if (m_hit_damage_notification_limiter)
    m_hit_damage_notification_limiter--;

  auto result267 = world();
if (!result267) return std::unexpected(std::move(result267.error()));
auto result268 = m_songbook->update(*entityMode(), result267->get());
if (!result268) return std::unexpected(std::move(result268.error()));


  m_effect_emitter->set_source_position("normal", position());
  auto result269 = mouth_offset(true);
if (!result269) return std::unexpected(std::move(result269.error()));
m_effect_emitter->set_source_position("mouth", position() + std::move(*result269));
  auto result270 = feet_offset();
if (!result270) return std::unexpected(std::move(result270.error()));
m_effect_emitter->set_source_position("feet", position() + std::move(*result270));
  auto result271 = head_armor_offset();
if (!result271) return std::unexpected(std::move(result271.error()));
m_effect_emitter->set_source_position("headArmor", std::move(*result271) + position());
  auto result272 = chest_armor_offset();
if (!result272) return std::unexpected(std::move(result272.error()));
m_effect_emitter->set_source_position("chestArmor", std::move(*result272) + position());
  auto result273 = legs_armor_offset();
if (!result273) return std::unexpected(std::move(result273.error()));
m_effect_emitter->set_source_position("legsArmor", std::move(*result273) + position());
  auto result274 = back_armor_offset();
if (!result274) return std::unexpected(std::move(result274.error()));
m_effect_emitter->set_source_position("backArmor", std::move(*result274) + position());

  auto result275 = humanoid();
if (!result275) return std::unexpected(std::move(result275.error()));
m_effect_emitter->set_direction(std::move(*result275)->facing_direction());
  auto result276 = m_effect_emitter->tick(dt, *entityMode());
if (!result276) return std::unexpected(std::move(result276.error()));


  auto result277 = humanoid();
if (!result277) return std::unexpected(std::move(result277.error()));
auto result278 = std::move(*result277)->set_moving_backwards(m_movement_controller->moving_direction() != m_movement_controller->facing_direction());
if (!result278) return std::unexpected(std::move(result278.error()));

  auto result279 = humanoid();
if (!result279) return std::unexpected(std::move(result279.error()));
std::move(*result279)->set_facing_direction(m_movement_controller->facing_direction());
  auto result280 = humanoid();
if (!result280) return std::unexpected(std::move(result280.error()));
std::move(*result280)->set_rotation(m_movement_controller->rotation());

  auto result281 = humanoid();
if (!result281) return std::unexpected(std::move(result281.error()));
auto result282 = m_armor->setup_humanoid(*std::move(*result281), force_nude());
if (!result282) return std::unexpected(std::move(result282.error()));


  auto result283 = can_use_tool();
if (!result283) return std::unexpected(std::move(result283.error()));
m_tools->suppressItems(!std::move(*result283));
  auto result284 = m_tools->tick(dt, m_shifting.get(), {});
if (!result284) return std::unexpected(std::move(result284.error()));


  {
  auto result285 = humanoid();
  if (!result285) return std::unexpected(std::move(result285.error()));
  auto result286 = aim_position();
  if (!result286) return std::unexpected(std::move(result286.error()));
  auto result287 = m_tools->setupHumanoidHandItems(*std::move(*result285), position(), std::move(*result286));
  if (!result287) return std::unexpected(std::move(result287.error()));
  if (auto override_direction = std::move(*result287))
      m_movement_controller->control_face(*override_direction);
}

  if (auto result = update_humanoid_directives(); !result)
    return std::unexpected(std::move(result.error()));
  {
  auto result288 = world();
  if (!result288) return std::unexpected(std::move(result288.error()));
  if (result288->get().isClient()) {
      auto result289 = humanoid();
  if (!result289) return std::unexpected(std::move(result289.error()));
  auto result290 = std::move(*result289)->animate(dt, std::ref(m_humanoid_dynamic_target));
  if (!result290) return std::unexpected(std::move(result290.error()));

      m_humanoid_dynamic_target.update_position(position());
    } else {
      auto result291 = humanoid();
  if (!result291) return std::unexpected(std::move(result291.error()));
  auto result292 = std::move(*result291)->animate(dt, {});
  if (!result292) return std::unexpected(std::move(result292.error()));

    }
}
  auto result293 = recover_lua_script_error(m_scripted_animator.update());
if (!result293) return std::unexpected(std::move(result293.error()));

  return {};
}

std::expected<LuaCallbacks, StarException> Npc::make_npc_callbacks() {
  LuaCallbacks callbacks;

  auto result294 = callbacks.register_callback("toAbsolutePosition", [this](Vec2F const& p) { return get_absolute_position(p); });
if (!result294) return std::unexpected(std::move(result294.error()));

  auto result295 = callbacks.register_callback("species", [this]() { return m_npc_variant.species; });
if (!result295) return std::unexpected(std::move(result295.error()));

auto result298 = callbacks.register_callback("gender", [this]() -> std::expected<String, StarException> {
  auto result296 = humanoid();
  if (!result296)
    return std::unexpected(std::move(result296.error()));
  auto result297 = enum_name<gender_names>(std::move(*result296)->identity().gender);
  if (!result297)
    return std::unexpected(std::move(result297.error()));
  return String(*result297);
});
if (!result298) return std::unexpected(std::move(result298.error()));

  auto result300 = callbacks.register_callback("humanoidIdentity", [this]() -> std::expected<Json, StarException> { auto result299 = humanoid();
if (!result299) return std::unexpected(std::move(result299.error()));
return std::move(*result299)->identity().to_json(); });
if (!result300) return std::unexpected(std::move(result300.error()));
  auto result303 = callbacks.register_callback("setHumanoidIdentity", [this](Json const& id) -> std::expected<void, StarException> { auto result301 = HumanoidIdentity::create(id);
if (!result301) return std::unexpected(std::move(result301.error()));
auto result302 = set_identity(std::move(*result301));
if (!result302) return std::unexpected(std::move(result302.error()));
   return {};
});
if (!result303) return std::unexpected(std::move(result303.error()));
auto result305 = callbacks.register_callback(
  "setHumanoidParameter", [this](String key, std::optional<Json> value) -> std::expected<void, StarException> {
    auto result304 = set_humanoid_parameter(std::move(key), std::move(value));
    if (!result304)
      return std::unexpected(std::move(result304.error()));
    return {};
  });
if (!result305) return std::unexpected(std::move(result305.error()));
auto result306 = callbacks.register_callback(
  "getHumanoidParameter", [this](String key) -> std::optional<Json> { return get_humanoid_parameter(std::move(key)); });
if (!result306) return std::unexpected(std::move(result306.error()));
auto result308 = callbacks.register_callback("setHumanoidParameters",
                                            [this](JsonObject parameters) -> std::expected<void, StarException> {
                                              auto result307 = set_humanoid_parameters(std::move(parameters));
                                              if (!result307)
                                                return std::unexpected(std::move(result307.error()));
                                              return {};
                                            });
if (!result308) return std::unexpected(std::move(result308.error()));
  auto result309 = callbacks.register_callback("getHumanoidParameters", [this]() -> JsonObject { return get_humanoid_parameters(); });
if (!result309) return std::unexpected(std::move(result309.error()));
  auto result311 = callbacks.register_callback("refreshHumanoidParameters", [this]() -> std::expected<void, StarException> { auto result310 = refresh_humanoid_parameters();
if (!result310) return std::unexpected(std::move(result310.error()));
   return {};
});
if (!result311) return std::unexpected(std::move(result311.error()));
  auto result313 = callbacks.register_callback("humanoidConfig", [this](bool with_overrides) -> std::expected<Json, StarException> { auto result312 = humanoid();
if (!result312) return std::unexpected(std::move(result312.error()));
return std::move(*result312)->humanoid_config(with_overrides); });
if (!result313) return std::unexpected(std::move(result313.error()));

  auto result314 = callbacks.register_callback(   "bodyDirectives", [this]()   { return identity().body_directives;      });
if (!result314) return std::unexpected(std::move(result314.error()));
  auto result316 = callbacks.register_callback("setBodyDirectives", [this](String const& str) -> std::expected<void, StarException> { auto result315 = set_body_directives(str);
if (!result315) return std::unexpected(std::move(result315.error()));
   return {};
});
if (!result316) return std::unexpected(std::move(result316.error()));

  auto result317 = callbacks.register_callback(   "emoteDirectives", [this]()   { return identity().emote_directives;      });
if (!result317) return std::unexpected(std::move(result317.error()));
  auto result319 = callbacks.register_callback("setEmoteDirectives", [this](String const& str) -> std::expected<void, StarException> { auto result318 = set_emote_directives(str);
if (!result318) return std::unexpected(std::move(result318.error()));
   return {};
});
if (!result319) return std::unexpected(std::move(result319.error()));

  auto result320 = callbacks.register_callback(   "hairGroup",      [this]()   { return identity().hair_group;      });
if (!result320) return std::unexpected(std::move(result320.error()));
  auto result322 = callbacks.register_callback("setHairGroup",      [this](String const& str) -> std::expected<void, StarException> { auto result321 = set_hair_group(str);
if (!result321) return std::unexpected(std::move(result321.error()));
   return {};
});
if (!result322) return std::unexpected(std::move(result322.error()));
  auto result323 = callbacks.register_callback(   "hairType",       [this]()   { return identity().hair_type;      });
if (!result323) return std::unexpected(std::move(result323.error()));
  auto result325 = callbacks.register_callback("setHairType",       [this](String const& str) -> std::expected<void, StarException> { auto result324 = set_hair_type(str);
if (!result324) return std::unexpected(std::move(result324.error()));
   return {};
});
if (!result325) return std::unexpected(std::move(result325.error()));
  auto result326 = callbacks.register_callback(   "hairDirectives", [this]()   { return identity().hair_directives;     });
if (!result326) return std::unexpected(std::move(result326.error()));
  auto result328 = callbacks.register_callback("setHairDirectives", [this](String const& str) -> std::expected<void, StarException> { auto result327 = set_hair_directives(str);
if (!result327) return std::unexpected(std::move(result327.error()));
   return {};
});
if (!result328) return std::unexpected(std::move(result328.error()));

  auto result329 = callbacks.register_callback(   "facialHairGroup",      [this]()   { return identity().facial_hair_group;      });
if (!result329) return std::unexpected(std::move(result329.error()));
  auto result331 = callbacks.register_callback("setFacialHairGroup",      [this](String const& str) -> std::expected<void, StarException> { auto result330 = set_facial_hair_group(str);
if (!result330) return std::unexpected(std::move(result330.error()));
   return {};
});
if (!result331) return std::unexpected(std::move(result331.error()));
  auto result332 = callbacks.register_callback(   "facialHairType",       [this]()   { return identity().facial_hair_type;      });
if (!result332) return std::unexpected(std::move(result332.error()));
  auto result334 = callbacks.register_callback("setFacialHairType",       [this](String const& str) -> std::expected<void, StarException> { auto result333 = set_facial_hair_type(str);
if (!result333) return std::unexpected(std::move(result333.error()));
   return {};
});
if (!result334) return std::unexpected(std::move(result334.error()));
  auto result335 = callbacks.register_callback(   "facialHairDirectives", [this]()   { return identity().facial_hair_directives;      });
if (!result335) return std::unexpected(std::move(result335.error()));
  auto result337 = callbacks.register_callback("setFacialHairDirectives", [this](String const& str) -> std::expected<void, StarException> { auto result336 = set_facial_hair_directives(str);
if (!result336) return std::unexpected(std::move(result336.error()));
   return {};
});
if (!result337) return std::unexpected(std::move(result337.error()));

  auto result338 = callbacks.register_callback("hair", [this]() {
    HumanoidIdentity const& humanoid_identity = identity();
    return lua_tuple_return(humanoid_identity.hair_group, humanoid_identity.hair_type, humanoid_identity.hair_directives);
  });
if (!result338) return std::unexpected(std::move(result338.error()));

  auto result339 = callbacks.register_callback("facialHair", [this]() {
    HumanoidIdentity const& humanoid_identity = identity();
    return lua_tuple_return(humanoid_identity.facial_hair_group, humanoid_identity.facial_hair_type, humanoid_identity.facial_hair_directives);
  });
if (!result339) return std::unexpected(std::move(result339.error()));

  auto result340 = callbacks.register_callback("facialMask", [this]() {
    HumanoidIdentity const& humanoid_identity = identity();
    return lua_tuple_return(humanoid_identity.facial_mask_group, humanoid_identity.facial_mask_type, humanoid_identity.facial_mask_directives);
  });
if (!result340) return std::unexpected(std::move(result340.error()));

  auto result345 = callbacks.register_callback("setFacialHair", [this](std::optional<String> const& group, std::optional<String> const& type, std::optional<String> const& directives) -> std::expected<void, StarException> {
    if (group && type && directives)
      {
  auto result341 = set_facial_hair(*group, *type, *directives);
  if (!result341) return std::unexpected(std::move(result341.error()));

}
    else {
      if (group)      {
  auto result342 = set_facial_hair_group(*group);
  if (!result342) return std::unexpected(std::move(result342.error()));

}
      if (type)       {
  auto result343 = set_facial_hair_type(*type);
  if (!result343) return std::unexpected(std::move(result343.error()));

}
      if (directives) {
  auto result344 = set_facial_hair_directives(*directives);
  if (!result344) return std::unexpected(std::move(result344.error()));

}
    }
    return {};
});
if (!result345) return std::unexpected(std::move(result345.error()));

  auto result350 = callbacks.register_callback("setFacialMask", [this](std::optional<String> const& group, std::optional<String> const& type, std::optional<String> const& directives) -> std::expected<void, StarException> {
    if (group && type && directives)
      {
  auto result346 = set_facial_mask(*group, *type, *directives);
  if (!result346) return std::unexpected(std::move(result346.error()));

}
    else {
      if (group)      {
  auto result347 = set_facial_mask_group(*group);
  if (!result347) return std::unexpected(std::move(result347.error()));

}
      if (type)       {
  auto result348 = set_facial_mask_type(*type);
  if (!result348) return std::unexpected(std::move(result348.error()));

}
      if (directives) {
  auto result349 = set_facial_mask_directives(*directives);
  if (!result349) return std::unexpected(std::move(result349.error()));

}
    }
    return {};
});
if (!result350) return std::unexpected(std::move(result350.error()));

  auto result355 = callbacks.register_callback("setHair", [this](std::optional<String> const& group, std::optional<String> const& type, std::optional<String> const& directives) -> std::expected<void, StarException> {
    if (group && type && directives)
      {
  auto result351 = set_hair(*group, *type, *directives);
  if (!result351) return std::unexpected(std::move(result351.error()));

}
    else {
      if (group)      {
  auto result352 = set_hair_group(*group);
  if (!result352) return std::unexpected(std::move(result352.error()));

}
      if (type)       {
  auto result353 = set_hair_type(*type);
  if (!result353) return std::unexpected(std::move(result353.error()));

}
      if (directives) {
  auto result354 = set_hair_directives(*directives);
  if (!result354) return std::unexpected(std::move(result354.error()));

}
    }
    return {};
});
if (!result355) return std::unexpected(std::move(result355.error()));

  auto result356 = callbacks.register_callback(   "description", [this]()                          { return description(); });
if (!result356) return std::unexpected(std::move(result356.error()));
  auto result357 = callbacks.register_callback("setDescription", [this](String const& description) { set_description(description); });
if (!result357) return std::unexpected(std::move(result357.error()));

  auto result358 = callbacks.register_callback(   "name", [this]()                   { return name(); });
if (!result358) return std::unexpected(std::move(result358.error()));
  auto result360 = callbacks.register_callback("setName", [this](String const& name) -> std::expected<void, StarException> { auto result359 = set_name(name);
if (!result359) return std::unexpected(std::move(result359.error()));
   return {};
});
if (!result360) return std::unexpected(std::move(result360.error()));

  auto result362 = callbacks.register_callback("setSpecies", [this](String const& species) -> std::expected<void, StarException> { auto result361 = set_species(species);
if (!result361) return std::unexpected(std::move(result361.error()));
   return {};
});
if (!result362) return std::unexpected(std::move(result362.error()));

  auto result363 = callbacks.register_callback(   "imagePath", [this]()                        { return identity().image_path;    });
if (!result363) return std::unexpected(std::move(result363.error()));
  auto result365 = callbacks.register_callback("setImagePath", [this](std::optional<String> const& image_path) -> std::expected<void, StarException> { auto result364 = set_image_path(image_path);
if (!result364) return std::unexpected(std::move(result364.error()));
   return {};
});
if (!result365) return std::unexpected(std::move(result365.error()));

auto result368 =
  callbacks.register_callback("setGender", [this](String const& gender) -> std::expected<void, StarException> {
    auto result366 = enum_value<gender_names>(gender);
    if (!result366)
      return std::unexpected(std::move(result366.error()));
    auto result367 = set_gender(*result366);
    if (!result367)
      return std::unexpected(std::move(result367.error()));
    return {};
  });
if (!result368) return std::unexpected(std::move(result368.error()));

  auto result369 = callbacks.register_callback(   "personality", [this]() { return json_from_personality(identity().personality); });
if (!result369) return std::unexpected(std::move(result369.error()));
  auto result372 = callbacks.register_callback("setPersonality", [this](Json const& personality_config) -> std::expected<void, StarException> {
    Personality new_personality = identity().personality;
    auto result370 = parse_personality(new_personality, personality_config);
if (!result370) return std::unexpected(std::move(result370.error()));

    auto result371 = set_personality(new_personality);
if (!result371) return std::unexpected(std::move(result371.error()));

    return {};
});
if (!result372) return std::unexpected(std::move(result372.error()));

  auto result373 = callbacks.register_callback(   "favoriteColor", [this]()            { return favorite_color();  });
if (!result373) return std::unexpected(std::move(result373.error()));
  auto result375 = callbacks.register_callback("setFavoriteColor", [this](Color color) -> std::expected<void, StarException> { auto result374 = set_favorite_color(color);
if (!result374) return std::unexpected(std::move(result374.error()));
   return {};
});
if (!result375) return std::unexpected(std::move(result375.error()));


  auto result376 = callbacks.register_callback("npcType", [this]() { return npc_type(); });
if (!result376) return std::unexpected(std::move(result376.error()));

  auto result377 = callbacks.register_callback("seed", [this]() { return m_npc_variant.seed; });
if (!result377) return std::unexpected(std::move(result377.error()));

  auto result378 = callbacks.register_callback("level", [this]() { return m_npc_variant.level; });
if (!result378) return std::unexpected(std::move(result378.error()));

  auto result379 = callbacks.register_callback("dropPools", [this]() { return m_drop_pools.get(); });
if (!result379) return std::unexpected(std::move(result379.error()));

  auto result380 = callbacks.register_callback("setDropPools", [this](StringList const& drop_pools) { m_drop_pools.set(drop_pools); });
if (!result380) return std::unexpected(std::move(result380.error()));

  auto result381 = callbacks.register_callback("energy", [this]() { return m_status_controller->resource("energy"); });
if (!result381) return std::unexpected(std::move(result381.error()));

  auto result382 = callbacks.register_callback("maxEnergy", [this]() { return m_status_controller->resourceMax("energy"); });
if (!result382) return std::unexpected(std::move(result382.error()));

  auto result384 = callbacks.register_callback("say", [this](String line, std::optional<StringMap<String>> const& tags, Json const& config) -> std::expected<bool, StarException> {
      if (tags)
        line = line.replace_tags(*tags, false);

      if (!line.empty()) {
        auto result383 = add_chat_message(line, config);
if (!result383) return std::unexpected(std::move(result383.error()));

        return true;
      }

      return false;
    });
if (!result384) return std::unexpected(std::move(result384.error()));

  auto result386 = callbacks.register_callback("sayPortrait", [this](String line, String portrait, std::optional<StringMap<String>> const& tags, Json const& config) -> std::expected<bool, StarException> {
      if (tags)
        line = line.replace_tags(*tags, false);

      if (!line.empty()) {
        auto result385 = add_chat_message(line, config, portrait);
if (!result385) return std::unexpected(std::move(result385.error()));

        return true;
      }

      return false;
    });
if (!result386) return std::unexpected(std::move(result386.error()));

  auto result388 = callbacks.register_callback("emote", [this](String const& arg1) -> std::expected<void, StarException> { if (!humanoid_emote_names) return std::unexpected(humanoid_emote_names.error());
    auto result387 = humanoid_emote_names->get_left(arg1);
    if (!result387)
      return std::unexpected(std::move(result387.error()));
    return add_emote(result387->get());
});
if (!result388) return std::unexpected(std::move(result388.error()));

  auto result390 = callbacks.register_callback("dance", [this](std::optional<String> const& name) { return set_dance(name); });
if (!result390) return std::unexpected(std::move(result390.error()));

  auto result391 = callbacks.register_callback("setInteractive", [this](bool interactive) { m_is_interactive.set(interactive); });
if (!result391) return std::unexpected(std::move(result391.error()));

  auto result399 = callbacks.register_callback("setLounging", [this](EntityId loungeable_entity_id, std::optional<std::size_t> maybe_anchor_index) -> std::expected<bool, StarException> {
      std::size_t anchor_index = maybe_anchor_index.value_or(0);
      auto result392 = world();
if (!result392) return std::unexpected(std::move(result392.error()));
      auto loungeable_entity = result392->get().get<LoungeableEntity>(loungeable_entity_id);
      {
  bool condition395 = static_cast<bool>(!loungeable_entity || anchor_index >= loungeable_entity->anchorCount());
  if (!condition395) {
    auto result394 = loungeable_entity->entitiesLoungingIn(anchor_index);
    if (!result394) return std::unexpected(std::move(result394.error()));
    condition395 = static_cast<bool>(!std::move(*result394).empty());
  }
  bool condition397 = static_cast<bool>(condition395);
  if (!condition397) {
    auto result396 = loungeable_entity->loungeAnchor(anchor_index);
    if (!result396) return std::unexpected(std::move(result396.error()));
    condition397 = static_cast<bool>(!std::move(*result396));
  }
  if (condition397)
          return false;
}

      auto result398 = m_movement_controller->set_anchor_state({loungeable_entity_id, anchor_index});
if (!result398) return std::unexpected(std::move(result398.error()));

      return true;
    });
if (!result399) return std::unexpected(std::move(result399.error()));

  auto result401 = callbacks.register_callback("resetLounging", [this]() -> std::expected<void, StarException> { auto result400 = m_movement_controller->reset_anchor_state();
if (!result400) return std::unexpected(std::move(result400.error()));
   return {};
});
if (!result401) return std::unexpected(std::move(result401.error()));

  auto result402 = callbacks.register_callback("isLounging", [this]() { return is<LoungeAnchor>(m_movement_controller->entity_anchor()); });
if (!result402) return std::unexpected(std::move(result402.error()));

  auto result403 = callbacks.register_callback("loungingIn", [this]() -> std::optional<EntityId> {
      auto lounging_state = lounging_in();
      if (lounging_state)
        return lounging_state.value_or(EntityAnchorState{}).entityId;
      else
        return {};
    });
if (!result403) return std::unexpected(std::move(result403.error()));

  auto result404 = callbacks.register_callback("setOfferedQuests", [this](std::optional<JsonArray> const& offered_quests) -> std::expected<void, StarException> {
      List<QuestArcDescriptor> parsed;
      for (auto const& quest : offered_quests.value_or(JsonArray{})) {
        auto arc = QuestArcDescriptor::fromJson(root(), quest);
        if (!arc) return std::unexpected(std::move(arc.error()));
        parsed.push_back(std::move(*arc));
      }
      m_offered_quests.set(std::move(parsed));
      return {};
    });
if (!result404) return std::unexpected(std::move(result404.error()));

  auto result405 = callbacks.register_callback("setTurnInQuests", [this](std::optional<StringList> const& turn_in_quests) {
      m_turn_in_quests.set(StringSet::from(turn_in_quests.value_or(StringList{})));
    });
if (!result405) return std::unexpected(std::move(result405.error()));

  auto result407 = callbacks.register_callback("setItemSlot", [this](String const& slot, Json const& item_descriptor) -> std::expected<Json, StarException> {
      auto result406 = ItemDescriptor::create(item_descriptor);
if (!result406) return std::unexpected(std::move(result406.error()));
return set_item_slot(slot, std::move(*result406));
    });
if (!result407) return std::unexpected(std::move(result407.error()));

  auto result412 = callbacks.register_callback("getItemSlot", [this](String const& entry) -> std::expected<Json, StarException> {
      {
  auto result408 = maybe_enum_value<equipment_slot_names>(entry);
  if (!result408) return std::unexpected(std::move(result408.error()));
  if (auto equipment_slot = std::move(*result408)) {
    return m_armor->item_descriptor(static_cast<std::uint8_t>(*equipment_slot)).to_json();
        } else {
    auto result409 = entry.equals_ignore_case("primary");
    if (!result409) return std::unexpected(std::move(result409.error()));
    if (std::move(*result409))
            return m_tools->primaryHandItemDescriptor().to_json();
          else {
      auto result410 = entry.equals_ignore_case("alt");
      if (!result410) return std::unexpected(std::move(result410.error()));
      if (std::move(*result410))
              return m_tools->altHandItemDescriptor().to_json();
            else if (m_npc_variant.items.contains(entry))
              {
        auto result411 = m_npc_variant.items.get(entry);
        if (!result411) return std::unexpected(std::move(result411.error()));
        return result411->get().to_json();
      }
    }
  }
}

      return {};
    });
if (!result412) return std::unexpected(std::move(result412.error()));

  auto result413 = callbacks.register_callback("disableWornArmor", [this](bool disable) { m_disable_worn_armor.set(disable); });
if (!result413) return std::unexpected(std::move(result413.error()));

  auto result414 = callbacks.register_callback("beginPrimaryFire", [this]() { m_tools->beginPrimaryFire(); });
if (!result414) return std::unexpected(std::move(result414.error()));
  auto result415 = callbacks.register_callback("beginAltFire", [this]() { m_tools->beginAltFire(); });
if (!result415) return std::unexpected(std::move(result415.error()));
  auto result416 = callbacks.register_callback("endPrimaryFire", [this]() { m_tools->endPrimaryFire(); });
if (!result416) return std::unexpected(std::move(result416.error()));
  auto result417 = callbacks.register_callback("endAltFire", [this]() { m_tools->endAltFire(); });
if (!result417) return std::unexpected(std::move(result417.error()));
  auto result418 = callbacks.register_callback("setShifting", [this](bool shifting) { m_shifting.set(shifting); });
if (!result418) return std::unexpected(std::move(result418.error()));
  auto result419 = callbacks.register_callback("setDamageOnTouch", [this](bool damage_on_touch) { m_damage_on_touch.set(damage_on_touch); });
if (!result419) return std::unexpected(std::move(result419.error()));

  auto result421 = callbacks.register_callback("aimPosition", [this]() -> std::expected<Json, StarException> { auto result420 = aim_position();
if (!result420) return std::unexpected(std::move(result420.error()));
return json_from_vec2f(std::move(*result420)); });
if (!result421) return std::unexpected(std::move(result421.error()));

  auto result423 = callbacks.register_callback("setAimPosition", [this](Vec2F const& pos) -> std::expected<void, StarException> {
      auto result422 = world();
if (!result422) return std::unexpected(std::move(result422.error()));
auto aim_position = result422->get().geometry().diff(pos, position());
      m_x_aim_position.set(aim_position[0]);
      m_y_aim_position.set(aim_position[1]);
      return {};
});
if (!result423) return std::unexpected(std::move(result423.error()));

  auto result424 = callbacks.register_callback("setDeathParticleBurst", [this](std::optional<String> const& death_particle_burst) {
      m_death_particle_burst.set(death_particle_burst);
    });
if (!result424) return std::unexpected(std::move(result424.error()));

  auto result425 = callbacks.register_callback("setStatusText", [this](std::optional<String> const& status) { m_status_text.set(status); });
if (!result425) return std::unexpected(std::move(result425.error()));
  auto result426 = callbacks.register_callback("setDisplayNametag", [this](bool display) { m_display_nametag.set(display); });
if (!result426) return std::unexpected(std::move(result426.error()));

  auto result427 = callbacks.register_callback("setPersistent", [this](bool persistent) { setPersistent(persistent); });
if (!result427) return std::unexpected(std::move(result427.error()));

  auto result428 = callbacks.register_callback("setKeepAlive", [this](bool keep_alive) { setKeepAlive(keep_alive); });
if (!result428) return std::unexpected(std::move(result428.error()));

  auto result430 = callbacks.register_callback("setDamageTeam", [this](Json const& team) -> std::expected<void, StarException> { auto result429 = EntityDamageTeam::create(team);
if (!result429) return std::unexpected(std::move(result429.error()));
setTeam(std::move(*result429));   return {};
});
if (!result430) return std::unexpected(std::move(result430.error()));

  auto result431 = callbacks.register_callback("setAggressive", [this](bool aggressive) { m_aggressive.set(aggressive); });
if (!result431) return std::unexpected(std::move(result431.error()));

auto result432 = callbacks.register_callback(
  "setUniqueId", [this](std::optional<String> uniqueId) { set_unique_id(std::move(uniqueId)); });
if (!result432) return std::unexpected(std::move(result432.error()));

  auto result433 = callbacks.register_callback("setAnimationParameter", [this](String name, Json value) {
    m_scripted_animation_parameters.set(std::move(name), std::move(value));
  });
if (!result433) return std::unexpected(std::move(result433.error()));

  return callbacks;
}

std::expected<void, StarException> Npc::setup_net_states() {
  auto result434 = m_net_group.add_net_element(m_x_aim_position);
if (!result434) return std::unexpected(std::move(result434.error()));

  auto result435 = m_net_group.add_net_element(m_y_aim_position);
if (!result435) return std::unexpected(std::move(result435.error()));


  m_x_aim_position.set_fixed_point_base(0.0625);
  m_y_aim_position.set_fixed_point_base(0.0625);
  m_x_aim_position.set_interpolator(lerp<float, float>);
  m_y_aim_position.set_interpolator(lerp<float, float>);

  auto result436 = m_net_group.add_net_element(m_unique_id_net_state);
if (!result436) return std::unexpected(std::move(result436.error()));

  auto result437 = m_net_group.add_net_element(m_team_net_state);
if (!result437) return std::unexpected(std::move(result437.error()));

  auto result438 = m_net_group.add_net_element(m_humanoid_state_net_state);
if (!result438) return std::unexpected(std::move(result438.error()));

  auto result439 = m_net_group.add_net_element(m_humanoid_emote_state_net_state);
if (!result439) return std::unexpected(std::move(result439.error()));

  auto result440 = m_net_group.add_net_element(m_humanoid_dance_net_state);
if (!result440) return std::unexpected(std::move(result440.error()));


  auto result441 = m_net_group.add_net_element(m_new_chat_message_event);
if (!result441) return std::unexpected(std::move(result441.error()));

  auto result442 = m_net_group.add_net_element(m_chat_message);
if (!result442) return std::unexpected(std::move(result442.error()));

  auto result443 = m_net_group.add_net_element(m_chat_portrait);
if (!result443) return std::unexpected(std::move(result443.error()));

  auto result444 = m_net_group.add_net_element(m_chat_config);
if (!result444) return std::unexpected(std::move(result444.error()));


  auto result445 = m_net_group.add_net_element(m_status_text);
if (!result445) return std::unexpected(std::move(result445.error()));

  auto result446 = m_net_group.add_net_element(m_display_nametag);
if (!result446) return std::unexpected(std::move(result446.error()));


  auto result447 = m_net_group.add_net_element(m_is_interactive);
if (!result447) return std::unexpected(std::move(result447.error()));


  auto result448 = m_net_group.add_net_element(m_offered_quests);
if (!result448) return std::unexpected(std::move(result448.error()));

  auto result449 = m_net_group.add_net_element(m_turn_in_quests);
if (!result449) return std::unexpected(std::move(result449.error()));


  auto result450 = m_net_group.add_net_element(m_shifting);
if (!result450) return std::unexpected(std::move(result450.error()));

  auto result451 = m_net_group.add_net_element(m_damage_on_touch);
if (!result451) return std::unexpected(std::move(result451.error()));


  auto result452 = m_net_group.add_net_element(m_disable_worn_armor);
if (!result452) return std::unexpected(std::move(result452.error()));


  auto result453 = m_net_group.add_net_element(m_death_particle_burst);
if (!result453) return std::unexpected(std::move(result453.error()));


  auto result454 = m_net_group.add_net_element(m_drop_pools);
if (!result454) return std::unexpected(std::move(result454.error()));

  auto result455 = m_net_group.add_net_element(m_aggressive);
if (!result455) return std::unexpected(std::move(result455.error()));


  auto result456 = m_net_group.add_net_element(*m_movement_controller);
if (!result456) return std::unexpected(std::move(result456.error()));

  auto result457 = m_net_group.add_net_element(*m_effect_emitter);
if (!result457) return std::unexpected(std::move(result457.error()));

  auto result458 = m_net_group.add_net_element(*m_status_controller);
if (!result458) return std::unexpected(std::move(result458.error()));

  auto result459 = m_net_group.add_net_element(*m_armor);
if (!result459) return std::unexpected(std::move(result459.error()));

  auto result460 = m_net_group.add_net_element(*m_tools);
if (!result460) return std::unexpected(std::move(result460.error()));

  m_songbook->set_compatibility_version(6);
  auto result461 = m_net_group.add_net_element(*m_songbook);
if (!result461) return std::unexpected(std::move(result461.error()));


  m_identity_net_state.set_compatibility_version(10);
  auto result462 = m_net_group.add_net_element(m_identity_net_state);
if (!result462) return std::unexpected(std::move(result462.error()));

  m_refreshed_humanoid_parameters.set_compatibility_version(10);
  auto result463 = m_net_group.add_net_element(m_refreshed_humanoid_parameters);
if (!result463) return std::unexpected(std::move(result463.error()));


  m_net_humanoid.set_compatibility_version(10);
  auto result464 = m_net_group.add_net_element(m_net_humanoid);
if (!result464) return std::unexpected(std::move(result464.error()));


  m_scripted_animation_parameters.set_compatibility_version(10);
  auto result465 = m_net_group.add_net_element(m_scripted_animation_parameters);
if (!result465) return std::unexpected(std::move(result465.error()));


  m_net_group.set_needs_store_callback([this] { return set_net_states(); });
  m_net_group.set_needs_load_callback([this](bool initial) { return get_net_states(initial); });
  return {};
}

std::expected<void, StarException> Npc::set_net_states() {
  m_unique_id_net_state.set(uniqueId());
  m_team_net_state.set(getTeam());
  auto body = humanoid();
  if (!body)
    return std::unexpected(std::move(body.error()));
  m_humanoid_state_net_state.set((*body)->state());
  m_humanoid_emote_state_net_state.set((*body)->emote_state());
  m_humanoid_dance_net_state.set((*body)->dance());

  if (m_identity_updated) {
    m_identity_net_state.push(m_npc_variant.humanoid_identity);
    m_identity_updated = false;
  }
  return {};
}

std::expected<void, StarException> Npc::get_net_states(bool initial) {
  set_unique_id(m_unique_id_net_state.get());
  setTeam(m_team_net_state.get());
  bool replaced;
  {
    auto body = humanoid();
    if (!body)
      return std::unexpected(std::move(body.error()));
    if (auto state = (*body)->set_state(m_humanoid_state_net_state.get()); !state)
      return std::unexpected(std::move(state.error()));
    if (auto emote = (*body)->set_emote_state(m_humanoid_emote_state_net_state.get()); !emote)
      return std::unexpected(std::move(emote.error()));
    if (auto dance = (*body)->set_dance(m_humanoid_dance_net_state.get()); !dance)
      return std::unexpected(std::move(dance.error()));

    // The dynamic group switches immediately; the identity and refresh fields
    // may arrive later through interpolation.
    replaced = m_applied_humanoid.owner_before(*body) || body->owner_before(m_applied_humanoid);
    if (replaced) {
      if (auto accepted = accept_replicated_humanoid(**body); !accepted)
        return std::unexpected(std::move(accepted.error()));
      m_applied_humanoid = *body;
    }
    bool refreshed = m_refreshed_humanoid_parameters.pull_occurred();
    if (m_identity_net_state.pull_updated() && !initial && !replaced && !refreshed) {
      auto const& next = m_identity_net_state.get();
      auto const& previous = (*body)->identity();
      if (next.species == previous.species && next.image_path == previous.image_path) {
        if (auto applied = (*body)->set_identity(next); !applied)
          return std::unexpected(std::move(applied.error()));
        m_npc_variant.humanoid_identity = next;
      }
    }
  }
  if (replaced) {
    if (auto restarted = restart_presentation_animator(); !restarted)
      return std::unexpected(std::move(restarted.error()));
  }

  if (m_new_chat_message_event.pull_occurred() && !initial) {
    m_chat_message_updated = true;
    if (m_chat_portrait.get().empty())
      {
  auto result478 = mouth_position();
  if (!result478) return std::unexpected(std::move(result478.error()));
  m_pending_chat_actions.push_back(
    SayChatAction{entityId(), m_chat_message.get(), std::move(*result478), m_chat_config.get()});
}
    else
      {
  auto result479 = mouth_position();
  if (!result479) return std::unexpected(std::move(result479.error()));
  m_pending_chat_actions.push_back(PortraitChatAction{entityId(), m_chat_portrait.get(), m_chat_message.get(),
                                                    std::move(*result479), m_chat_config.get()});
}
  }
  return {};
}

std::expected<Vec2F, StarException> Npc::mouth_position() const {
  auto result480 = mouth_offset(true);
if (!result480) return std::unexpected(std::move(result480.error()));
return std::move(*result480) + position();
}

std::expected<Vec2F, StarException> Npc::mouth_position(bool ignore_adjustments) const {
  auto result481 = mouth_offset(ignore_adjustments);
if (!result481) return std::unexpected(std::move(result481.error()));
return std::move(*result481) + position();
}

List<ChatAction> Npc::pull_pending_chat_actions() {
  return std::move(m_pending_chat_actions);
}

std::expected<void, StarException> Npc::add_chat_message(String const& message, Json const& config, String const& portrait) {
  if (isSlave())
    return std::unexpected(StarException::format("Cannot add a chat message on a slave NPC"));
  m_chat_message.set(message);
  m_chat_portrait.set(portrait);
  m_chat_config.set(config);
  m_chat_message_updated = true;
  m_new_chat_message_event.trigger();
  if (portrait.empty())
    {
  auto result482 = mouth_position();
  if (!result482) return std::unexpected(std::move(result482.error()));
  m_pending_chat_actions.push_back(SayChatAction{entityId(), message, std::move(*result482), config});
}
  else
    {
  auto result483 = mouth_position();
  if (!result483) return std::unexpected(std::move(result483.error()));
  m_pending_chat_actions.push_back(PortraitChatAction{entityId(), portrait, message, std::move(*result483), config});
}
  return {};
}

std::expected<void, StarException> Npc::add_emote(HumanoidEmote emote) {
  if (isSlave())
    return std::unexpected(StarException::format("Cannot add an emote on a slave NPC"));
  m_emote_state = emote;
  m_emote_cooldown_timer.reset();
  return {};
}

std::expected<void, StarException> Npc::set_dance(std::optional<String> const& dance_name) {
  if (isSlave())
    return std::unexpected(StarException::format("Cannot set a dance on a slave NPC"));
  return m_dance.set(root(), dance_name);
}

bool Npc::is_interactive() const {
  return m_is_interactive.get();
}

std::expected<InteractAction, StarException> Npc::interact(InteractRequest const& request) {
  auto result = recover_lua_script_error(m_script_component.invoke<Json>("interact",
      JsonObject{{"sourceId", request.source_id}, {"sourcePosition", json_from_vec2f(request.source_position)}}));
  if (!result)
    return std::unexpected(std::move(result.error()));
  if (!*result || (*result)->is_null())
    return {};
  return interact_action_from_script(**result, entityId());
}

RectF Npc::interactive_bound_box() const {
  return m_movement_controller->collision_poly().bounding_box();
}

std::optional<EntityAnchorState> Npc::lounging_in() const {
  if (is<LoungeAnchor>(m_movement_controller->entity_anchor()))
    return m_movement_controller->anchor_state();
  return {};
}

std::expected<List<QuestArcDescriptor>, StarException> Npc::offered_quests() const {
  return m_offered_quests.get();
}

std::expected<StringSet, StarException> Npc::turn_in_quests() const {
  return m_turn_in_quests.get();
}

std::expected<Vec2F, StarException> Npc::quest_indicator_position() const {
  Vec2F pos = position() + m_quest_indicator_offset;
  pos[1] += interactive_bound_box().y_max();
  return pos;
}

std::expected<bool, StarException> Npc::set_item_slot(String const& slot, ItemDescriptor item_descriptor) {
  auto result490 = this->root().itemDatabase();
if (!result490) return std::unexpected(std::move(result490.error()));
auto result491 = std::move(*result490)->item(std::move(item_descriptor), m_npc_variant.level, m_npc_variant.seed);
if (!result491) return std::unexpected(std::move(result491.error()));
auto item = std::move(*result491);

  {
  auto result492 = maybe_enum_value<equipment_slot_names>(slot);
  if (!result492) return std::unexpected(std::move(result492.error()));
  if (auto equipment_slot = std::move(*result492)) {
    m_armor->set_item(static_cast<std::uint8_t>(*equipment_slot), as<ArmorItem>(item));
    } else {
    auto result493 = slot.equals_ignore_case("primary");
    if (!result493) return std::unexpected(std::move(result493.error()));
    if (std::move(*result493))
        {
      auto result494 = m_tools->setItems(item, m_tools->altHandItem());
      if (!result494) return std::unexpected(std::move(result494.error()));

    }
      else {
      auto result495 = slot.equals_ignore_case("alt");
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

std::expected<bool, StarException> Npc::can_use_tool() const {
  auto result497 = should_destroy();
if (!result497) return std::unexpected(std::move(result497.error()));
bool can_use = !std::move(*result497) && !m_status_controller->toolUsageSuppressed();
  if (can_use) {
    auto anchor = m_movement_controller->entity_anchor();
    if (auto lounge_anchor = anchor ? as_ref<LoungeAnchor>(*anchor) : std::nullopt)
      if (lounge_anchor->get().suppressTools.value_or(lounge_anchor->get().controllable))
        return false;
  }
  return can_use;
}

void Npc::disable_worn_armor(bool disable) {
  m_disable_worn_armor.set(disable);
}

std::expected<List<LightSource>, StarException> Npc::light_sources() const {
  return presentation_light_sources(1.0f);
}

std::expected<List<LightSource>, StarException> Npc::presentation_light_sources(float interpolation) const {
  List<LightSource> lights;
  if (auto result = m_tools->append_light_sources(lights, interpolation); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = m_status_controller->append_light_sources(lights, interpolation); !result)
    return std::unexpected(std::move(result.error()));

  auto actor = humanoid();
  if (!actor)
    return std::unexpected(std::move(actor.error()));
  auto sample = (*actor)->networked_animator()->get().sample(interpolation);
  if (auto prepared = (*actor)->prepare_animator_pose(sample); !prepared)
    return std::unexpected(std::move(prepared.error()));
  if (auto result = sample.append_light_sources(lights); !result)
    return std::unexpected(std::move(result.error()));
  return lights;
}

MessageResponseResult Npc::receive_message(ConnectionId sending_connection, String const& message, JsonArray const& args) {
  auto current_world = world();
  if (!current_world)
    return std::unexpected(std::move(current_world.error()));
  auto response = recover_lua_script_error(m_script_component.handle_message(message, current_world->get().connection() == sending_connection, args));
  if (!response || *response)
    return response;
  // The script may have changed world membership before declining the message.
  auto fallback_world = world();
  if (!fallback_world)
    return std::unexpected(std::move(fallback_world.error()));
  return m_status_controller->receive_message(message, fallback_world->get().connection() == sending_connection, args);
}

std::expected<Vec2F, StarException> Npc::arm_position(ToolHand hand, Direction facing_direction, float arm_angle, Vec2F offset) const {
  auto body = humanoid();
  if (!body) return std::unexpected(std::move(body.error()));
  return (*body)->arm_position(hand, facing_direction, arm_angle, offset);
}

std::expected<Vec2F, StarException> Npc::hand_offset(ToolHand hand, Direction facing_direction) const {
  auto body = humanoid();
  if (!body) return std::unexpected(std::move(body.error()));
  return (*body)->hand_offset(hand, facing_direction);
}

std::expected<Vec2F, StarException> Npc::hand_position(ToolHand hand, Vec2F const& hand_offset) const {
  auto body = humanoid();
  if (!body) return std::unexpected(std::move(body.error()));
  return (*body)->hand_position(hand, hand_offset);
}

std::shared_ptr<Item> Npc::hand_item(ToolHand hand) const {
  if (hand == ToolHand::Primary)
    return m_tools->primaryHandItem();
  return m_tools->altHandItem();
}

std::expected<Vec2F, StarException> Npc::arm_adjustment() const {
  auto result509 = humanoid();
if (!result509) return std::unexpected(std::move(result509.error()));
return std::move(*result509)->arm_adjustment();
}

Vec2F Npc::velocity() const {
  return m_movement_controller->velocity();
}

std::expected<Vec2F, StarException> Npc::aim_position() const {
  auto result510 = world();
if (!result510) return std::unexpected(std::move(result510.error()));
return result510->get().geometry().xwrap(Vec2F(m_x_aim_position.get(), m_y_aim_position.get()) + position());
}

std::expected<float, StarException> Npc::interact_radius() const {
  return 9999;
}

Direction Npc::facing_direction() const {
  return m_movement_controller->facing_direction();
}

Direction Npc::walking_direction() const {
  return m_movement_controller->moving_direction();
}

bool Npc::is_admin() const {
  return false;
}

Color Npc::favorite_color() const {
  return Color::palette_white;
}

std::expected<float, StarException> Npc::beam_gun_radius() const {
  return m_tools->beamGunRadius();
}

std::expected<void, StarException> Npc::add_particles(List<Particle> const&) {  return {};
}

std::expected<void, StarException> Npc::add_sound(String const&, float, float) {  return {};
}

std::expected<bool, StarException> Npc::in_tool_range() const {
  return true;
}

std::expected<bool, StarException> Npc::in_tool_range(Vec2F const&) const {
  return true;
}

std::expected<void, StarException> Npc::add_ephemeral_status_effects(List<EphemeralStatusEffect> const& status_effects) {
  auto result511 = m_status_controller->addEphemeralEffects(status_effects);
if (!result511) return std::unexpected(std::move(result511.error()));

  return {};
}

std::expected<ActiveUniqueStatusEffectSummary, StarException> Npc::active_unique_status_effect_summary() const {
  return m_status_controller->activeUniqueStatusEffectSummary();
}

std::expected<float, StarException> Npc::power_multiplier() const {
  return m_status_controller->stat("powerMultiplier");
}

std::expected<bool, StarException> Npc::full_energy() const {
  auto percentage = m_status_controller->resourcePercentage("energy");
  if (!percentage) return std::unexpected(std::move(percentage.error()));
  if (!*percentage) return std::unexpected(NpcException("Energy resource has no maximum"));
  return **percentage >= 1.0f;
}

std::expected<float, StarException> Npc::energy() const {
  return m_status_controller->resource("energy");
}

std::expected<bool, StarException> Npc::energy_locked() const {
  return m_status_controller->resourceLocked("energy");
}

std::expected<bool, StarException> Npc::consume_energy(float energy) {
  return m_status_controller->overConsumeResource("energy", energy);
}

std::expected<void, StarException> Npc::queue_ui_message(String const&) {  return {};
}

std::expected<bool, StarException> Npc::instrument_playing() {
  return m_songbook->instrumentPlaying();
}

std::expected<void, StarException> Npc::instrument_equipped(String const& instrument_kind) {
  {
  auto result512 = can_use_tool();
  if (!result512) return std::unexpected(std::move(result512.error()));
  if (std::move(*result512))
      {
    auto result513 = mouth_position();
    if (!result513) return std::unexpected(std::move(result513.error()));
    m_songbook->keepAlive(instrument_kind, std::move(*result513));
  }
}
  return {};
}

std::expected<void, StarException> Npc::interact(InteractAction) {
  return {};
}

std::expected<void, StarException> Npc::add_effect_emitters(StringSet const& emitters) {
  m_effect_emitter->add_effect_sources("normal", emitters);
  return {};
}

std::expected<void, StarException> Npc::request_emote(String const& emote) {
  if (!emote.empty()) {
    if (!humanoid_emote_names) return std::unexpected(humanoid_emote_names.error());
    auto result514 = humanoid_emote_names->get_left(emote);
    if (!result514)
      return std::unexpected(std::move(result514.error()));
    auto state = result514->get();
    if (state != HumanoidEmote::Idle && (m_emote_state == HumanoidEmote::Idle || m_emote_state == HumanoidEmote::Blink))
      return add_emote(state);
  }
  return {};
}

ActorMovementController& Npc::movement_controller() {
  return *m_movement_controller;
}

StatusController& Npc::status_controller() {
  return *m_status_controller;
}

Songbook& Npc::songbook() {
  return *m_songbook;
}

void Npc::set_camera_focus_entity(std::optional<EntityId> const&) {
  // players only
}

std::expected<void, StarException> Npc::play_emote(HumanoidEmote emote) {
  return add_emote(emote);
}

std::expected<List<DamageSource>, StarException> Npc::damage_sources() const {
  auto sources = m_tools->damageSources();
  if (!sources)
    return std::unexpected(std::move(sources.error()));

  auto const& config = m_npc_variant.touch_damage_config;
  if (m_damage_on_touch.get() && !config.is_null()) {
    if (!m_touch_damage_template) {
      auto poly = config.contains("poly");
      if (!poly)
        return std::unexpected(std::move(poly.error()));
      bool uses_collision_poly = false;
      if (!*poly) {
        auto line = config.contains("line");
        if (!line)
          return std::unexpected(std::move(line.error()));
        uses_collision_poly = !*line;
      }
      // Preserve precedence over damageArea without freezing the current shape.
      auto source = [&]() -> std::expected<DamageSource, StarException> {
        if (!uses_collision_poly)
          return DamageSource::create(config);
        auto template_config = config.set("poly", JsonArray{});
        if (!template_config)
          return std::unexpected(std::move(template_config.error()));
        return DamageSource::create(*template_config);
      }();
      if (!source)
        return std::unexpected(std::move(source.error()));
      m_touch_damage_template.emplace(TouchDamageTemplate{std::move(*source), uses_collision_poly});
    }

    auto& touch = sources->emplace_back(m_touch_damage_template->source);
    if (m_touch_damage_template->uses_collision_poly)
      touch.damage_area = fix_inside_out_poly(m_movement_controller->collision_poly());
    if (std::holds_alternative<PolyF>(touch.damage_area))
      std::get<PolyF>(touch.damage_area).rotate(m_movement_controller->rotation());
    touch.damage *= m_status_controller->stat("powerMultiplier");
  }

  if (!sources->empty()) {
    auto id = entityId();
    auto team = getTeam();
    for (auto& source : *sources) {
      source.source_entity_id = id;
      source.team = team;
    }
  }
  return sources;
}

std::expected<List<PhysicsForceRegion>, StarException> Npc::force_regions() const {
  return m_tools->force_regions();
}


HumanoidIdentity const& Npc::identity() const {
  return m_npc_variant.humanoid_identity;
}

std::expected<void, StarException> Npc::set_identity(HumanoidIdentity identity) {
  auto const& previous = m_npc_variant.humanoid_identity;
  if (identity.species != previous.species || identity.image_path != previous.image_path)
    return replace_humanoid(std::move(identity));

  auto body = humanoid();
  if (!body)
    return std::unexpected(std::move(body.error()));
  if (auto applied = (*body)->set_identity(identity); !applied)
    return std::unexpected(std::move(applied.error()));
  m_npc_variant.humanoid_identity = std::move(identity);
  m_identity_updated = true;
  return {};
}

std::expected<void, StarException> Npc::set_humanoid_parameter(String key, std::optional<Json> value) {
  auto parameters = m_npc_variant.humanoid_parameters;
  if (value)
    parameters.set(std::move(key), std::move(*value));
  else
    parameters.erase(key);
  return set_humanoid_parameters(std::move(parameters));
}

std::optional<Json> Npc::get_humanoid_parameter(String key) {
  return m_npc_variant.humanoid_parameters.maybe(key);
}

std::expected<void, StarException> Npc::set_humanoid_parameters(JsonObject parameters) {
  auto element = m_net_humanoid.last_net_element();
  if (!element)
    return std::unexpected(std::move(element.error()));
  auto applied = element->get().set_humanoid_parameters(parameters);
  if (!applied)
    return std::unexpected(std::move(applied.error()));
  m_npc_variant.humanoid_parameters = std::move(parameters);
  return {};
}

JsonObject Npc::get_humanoid_parameters() {
  return m_npc_variant.humanoid_parameters;
}

std::expected<void, StarException> Npc::set_body_directives(String const& directives) {
  auto candidate = identity();
  candidate.body_directives = directives;
  return set_identity(std::move(candidate));
}

std::expected<void, StarException> Npc::set_emote_directives(String const& directives) {
  auto candidate = identity();
  candidate.emote_directives = directives;
  return set_identity(std::move(candidate));
}

std::expected<void, StarException> Npc::set_hair_group(String const& group) {
  auto candidate = identity();
  candidate.hair_group = group;
  return set_identity(std::move(candidate));
}

std::expected<void, StarException> Npc::set_hair_type(String const& type) {
  auto candidate = identity();
  candidate.hair_type = type;
  return set_identity(std::move(candidate));
}

std::expected<void, StarException> Npc::set_hair_directives(String const& directives) {
  auto candidate = identity();
  candidate.hair_directives = directives;
  return set_identity(std::move(candidate));
}

std::expected<void, StarException> Npc::set_facial_hair_group(String const& group) {
  auto candidate = identity();
  candidate.facial_hair_group = group;
  return set_identity(std::move(candidate));
}

std::expected<void, StarException> Npc::set_facial_hair_type(String const& type) {
  auto candidate = identity();
  candidate.facial_hair_type = type;
  return set_identity(std::move(candidate));
}

std::expected<void, StarException> Npc::set_facial_hair_directives(String const& directives) {
  auto candidate = identity();
  candidate.facial_hair_directives = directives;
  return set_identity(std::move(candidate));
}

std::expected<void, StarException> Npc::set_facial_mask_group(String const& group) {
  auto candidate = identity();
  candidate.facial_mask_group = group;
  return set_identity(std::move(candidate));
}

std::expected<void, StarException> Npc::set_facial_mask_type(String const& type) {
  auto candidate = identity();
  candidate.facial_mask_type = type;
  return set_identity(std::move(candidate));
}

std::expected<void, StarException> Npc::set_facial_mask_directives(String const& directives) {
  auto candidate = identity();
  candidate.facial_mask_directives = directives;
  return set_identity(std::move(candidate));
}

std::expected<void, StarException> Npc::set_hair(String const& group, String const& type, String const& directives) {
  auto candidate = identity();
  candidate.hair_group = group;
  candidate.hair_type = type;
  candidate.hair_directives = directives;
  return set_identity(std::move(candidate));
}

std::expected<void, StarException> Npc::set_facial_hair(String const& group, String const& type, String const& directives) {
  auto candidate = identity();
  candidate.facial_hair_group = group;
  candidate.facial_hair_type = type;
  candidate.facial_hair_directives = directives;
  return set_identity(std::move(candidate));
}

std::expected<void, StarException> Npc::set_facial_mask(String const& group, String const& type, String const& directives) {
  auto candidate = identity();
  candidate.facial_mask_group = group;
  candidate.facial_mask_type = type;
  candidate.facial_mask_directives = directives;
  return set_identity(std::move(candidate));
}

std::expected<void, StarException> Npc::set_species(String const& species) {
  auto candidate = identity();
  candidate.species = species;
  return set_identity(std::move(candidate));
}

std::expected<void, StarException> Npc::set_gender(Gender const& gender) {
  auto candidate = identity();
  candidate.gender = gender;
  return set_identity(std::move(candidate));
}

std::expected<void, StarException> Npc::set_personality(Personality const& personality) {
  auto candidate = identity();
  candidate.personality = personality;
  return set_identity(std::move(candidate));
}

std::expected<void, StarException> Npc::set_image_path(std::optional<String> const& image_path) {
  auto candidate = identity();
  candidate.image_path = image_path;
  return set_identity(std::move(candidate));
}

std::expected<void, StarException> Npc::set_favorite_color(Color color) {
  auto candidate = identity();
  candidate.color = color.to_rgba();
  return set_identity(std::move(candidate));
}

std::expected<void, StarException> Npc::set_name(String const& name) {
  auto candidate = identity();
  candidate.name = name;
  return set_identity(std::move(candidate));
}

void Npc::set_description(String const& description) {
  m_npc_variant.description = description;
}

std::expected<std::shared_ptr<Humanoid>, StarException> Npc::humanoid() const {
  auto element = m_net_humanoid.last_net_element();
  if (!element)
    return std::unexpected(std::move(element.error()));
  return element->get().humanoid();
}

std::expected<void, StarException> Npc::refresh_humanoid_parameters() {
  return replace_humanoid();
}

std::expected<void, StarException> Npc::replace_humanoid(std::optional<HumanoidIdentity> identity) {
  if (isSlave())
    return std::unexpected(StarException("Cannot replace a slave NPC humanoid"));

  auto candidate = NetHumanoid::create(root(), identity ? *identity : m_npc_variant.humanoid_identity,
      m_npc_variant.humanoid_parameters, m_npc_variant.unique_humanoid_config ? m_npc_variant.humanoid_config : Json());
  if (!candidate)
    return std::unexpected(std::move(candidate.error()));
  {
    auto body = (*candidate)->humanoid();
    auto armor = m_armor->prepare_humanoid_replacement(*body, force_nude());
    if (!armor)
      return std::unexpected(std::move(armor.error()));
    auto movement = ActorMovementParameters::create(json_merge(body->default_movement_parameters(), m_npc_variant.movement_parameters));
    if (!movement)
      return std::unexpected(std::move(movement.error()));
    if (auto installed = m_net_humanoid.replace_net_elements(std::move(*candidate), [&] {
          return m_movement_controller->reset_base_parameters(*movement);
        }); !installed)
      return std::unexpected(std::move(installed.error()));

    m_armor->commit_humanoid_replacement(*armor);
    m_applied_humanoid = body;
    m_death_particle_burst.set(body->default_death_particles());
  }
  if (identity) {
    m_npc_variant.humanoid_identity = std::move(*identity);
    m_identity_updated = true;
  }
  m_scripted_animation_parameters.clear();
  m_refreshed_humanoid_parameters.trigger();

  // The new native state is published before Lua runs. A hook may change it
  // again, and a hook failure cannot roll back arbitrary Lua side effects.
  if (m_script_component.initialized()) {
    if (auto removed = m_script_component.remove_callbacks("animator"); !removed)
      return std::unexpected(std::move(removed.error()));
    if (auto bound = bind_actor_animator(); !bound)
      return std::unexpected(std::move(bound.error()));
    if (auto invoked = recover_lua_script_error(m_script_component.invoke("refreshHumanoidParameters")); !invoked)
      return std::unexpected(std::move(invoked.error()));
  }
  return restart_presentation_animator();
}

std::expected<void, StarException> Npc::accept_replicated_humanoid(Humanoid& body) {
  auto armor = m_armor->prepare_humanoid_replacement(body, force_nude());
  if (!armor)
    return std::unexpected(std::move(armor.error()));
  auto movement = ActorMovementParameters::create(json_merge(body.default_movement_parameters(), m_npc_variant.movement_parameters));
  if (!movement)
    return std::unexpected(std::move(movement.error()));
  auto element = m_net_humanoid.last_net_element();
  if (!element)
    return std::unexpected(std::move(element.error()));
  if (auto applied = m_movement_controller->reset_base_parameters(*movement); !applied)
    return std::unexpected(std::move(applied.error()));

  m_armor->commit_humanoid_replacement(*armor);
  m_npc_variant.humanoid_identity = body.identity();
  m_npc_variant.humanoid_parameters = element->get().humanoid_parameters();
  return {};
}

std::expected<void, StarException> Npc::bind_actor_animator() {
  auto body = humanoid();
  if (!body)
    return std::unexpected(std::move(body.error()));
  auto callbacks = LuaBindings::make_networked_animator_callbacks((*body)->networked_animator()->get());
  if (!callbacks)
    return std::unexpected(std::move(callbacks.error()));
  if (auto bound = m_script_component.add_callbacks("animator", std::move(*callbacks)); !bound)
    return std::unexpected(std::move(bound.error()));
  m_actor_callback_humanoid = std::move(*body);
  return {};
}

std::expected<void, StarException> Npc::bind_presentation_animator() {
  auto body = humanoid();
  if (!body)
    return std::unexpected(std::move(body.error()));
  if (auto scripts = m_scripted_animator.set_scripts((*body)->animation_scripts()); !scripts)
    return std::unexpected(std::move(scripts.error()));
  auto callbacks = LuaBindings::make_scripted_animator_callbacks((*body)->networked_animator()->get(),
      [this](String const& name, Json const& default_value) {
        return m_scripted_animation_parameters.value(name, default_value);
      });
  if (!callbacks)
    return std::unexpected(std::move(callbacks.error()));
  if (auto bound = m_scripted_animator.add_callbacks("animationConfig", std::move(*callbacks)); !bound)
    return std::unexpected(std::move(bound.error()));
  m_presentation_callback_humanoid = std::move(*body);
  return {};
}

std::expected<void, StarException> Npc::restart_presentation_animator() {
  if (!m_scripted_animator.initialized() ||
      (!m_applied_humanoid.owner_before(m_presentation_callback_humanoid) &&
       !m_presentation_callback_humanoid.owner_before(m_applied_humanoid)))
    return {};

  if (auto stopped = recover_lua_script_error(m_scripted_animator.uninit()); !stopped)
    return std::unexpected(std::move(stopped.error()));
  if (auto removed = m_scripted_animator.remove_callbacks("animationConfig"); !removed)
    return std::unexpected(std::move(removed.error()));
  if (auto bound = bind_presentation_animator(); !bound)
    return std::unexpected(std::move(bound.error()));
  auto current_world = world();
  if (!current_world)
    return std::unexpected(std::move(current_world.error()));
  return recover_lua_script_error(m_scripted_animator.init(current_world->get()));
}

bool Npc::force_nude() const {
  return m_status_controller->statPositive("nude");
}

}

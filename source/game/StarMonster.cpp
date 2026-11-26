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
import star.message_types;
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
import star.image_metadata_database;
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
import star.matrix3;
import star.small_vector;
namespace Star {

Monster::Monster(ConstructionKey, Root& root_context, MonsterVariant&& variant)
    : Entity(root_context), m_monster_variant(std::move(variant)),
      m_script_component(root_context), m_scripted_animator(root_context) {}

std::expected<std::shared_ptr<Monster>, StarException> Monster::create(
    Root& root_context, MonsterVariant&& variant, std::optional<float> level) {
  auto monster = std::make_shared<Monster>(ConstructionKey{}, root_context, std::move(variant));
  auto initialized = monster->initialize(level);
  if (!initialized)
    return std::unexpected(std::move(initialized.error()));
  return monster;
}

std::expected<void, StarException> Monster::initialize(std::optional<float> level) {
  auto const& variant = m_monster_variant;
  m_monster_level = level;

  m_damage_on_touch = false;
  m_aggressive = false;

  m_knocked_out = false;
  m_knockout_timer = 0.0;

  m_drop_pool = variant.drop_pool_config;

  auto assets = root().assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  auto indicator_config = (*assets)->json("/quests/quests.config:defaultIndicatorOffset");
  if (!indicator_config)
    return std::unexpected(std::move(indicator_config.error()));
  auto indicator_offset = json_to_vec2f(*indicator_config);
  if (!indicator_offset)
    return std::unexpected(std::move(indicator_offset.error()));
  m_quest_indicator_offset = *indicator_offset;

  setTeam(EntityDamageTeam(variant.damage_team_type, variant.damage_team));

  auto animator = NetworkedAnimator::create(root(), variant.animator_config);
  if (!animator)
    return std::unexpected(std::move(animator.error()));
  m_networked_animator = std::move(*animator);
  auto emitter = EffectEmitter::create(root());
  if (!emitter)
    return std::unexpected(std::move(emitter.error()));
  m_effect_emitter = std::move(*emitter);
  auto database = root().monster_database();
  if (!database)
    return std::unexpected(std::move(database.error()));
  if (auto applied = (*database)->apply_appearance(variant, *m_networked_animator); !applied)
    return std::unexpected(std::move(applied.error()));

  auto status = StatusController::create(root(), variant.status_settings);
  if (!status)
    return std::unexpected(std::move(status.error()));
  m_status_controller = std::move(*status);

  auto script_config = variant.parameters.opt_array("scripts");
  if (!script_config)
    return std::unexpected(std::move(script_config.error()));
  if (*script_config) {
    auto scripts = json_to_string_list(std::move(**script_config));
    if (!scripts)
      return std::unexpected(std::move(scripts.error()));
    if (auto set = m_script_component.set_scripts(*scripts); !set)
      return std::unexpected(std::move(set.error()));
  } else if (auto set = m_script_component.set_scripts(variant.scripts); !set) {
    return std::unexpected(std::move(set.error()));
  }
  m_script_component.set_update_delta(variant.initial_script_delta);

  auto defaults = ActorMovementParameters::sensible_defaults(root());
  if (!defaults)
    return std::unexpected(std::move(defaults.error()));
  auto movement_parameters = std::move(*defaults).merge(variant.movement_settings);
  if (movement_parameters.standing_poly)
    movement_parameters.standing_poly->scale(variant.scale);
  if (movement_parameters.crouching_poly)
    movement_parameters.crouching_poly->scale(variant.scale);
  *movement_parameters.walk_speed *= variant.walk_multiplier;
  *movement_parameters.run_speed *= variant.run_multiplier;
  *movement_parameters.air_jump_profile.jump_speed *= variant.jump_multiplier;
  *movement_parameters.liquid_jump_profile.jump_speed *= variant.jump_multiplier;
  *movement_parameters.mass *= variant.weight_multiplier;
  if (!movement_parameters.physics_effect_categories)
    movement_parameters.physics_effect_categories = StringSet{"monster"};
  auto movement = ActorMovementController::create(root(), movement_parameters);
  if (!movement)
    return std::unexpected(std::move(movement.error()));
  m_movement_controller = std::move(*movement);

  setPersistent(variant.persistent);

  if (auto setup = setupNetStates(); !setup)
    return std::unexpected(std::move(setup.error()));
  return setNetStates();
}

std::expected<std::shared_ptr<Monster>, StarException> Monster::create(Root& root_context, Json const& disk_store) {
  auto database = root_context.monster_database();
  if (!database) return std::unexpected(std::move(database.error()));
  auto encoded = disk_store.get("monsterVariant");
  if (!encoded) return std::unexpected(std::move(encoded.error()));
  auto variant = (*database)->read_monster_variant_from_json(*encoded);
  if (!variant) return std::unexpected(std::move(variant.error()));
  auto creature = create(root_context, std::move(*variant), {});
  if (!creature) return std::unexpected(std::move(creature.error()));
  auto loaded = (*creature)->loadState(disk_store);
  if (!loaded) return std::unexpected(std::move(loaded.error()));
  return creature;
}

std::expected<void, StarException> Monster::loadState(Json const& diskStore) {
  auto result16 = diskStore.opt_float("monsterLevel");
if (!result16) return std::unexpected(std::move(result16.error()));
m_monster_level = std::move(*result16);
  auto result17 = diskStore.get("movementState");
if (!result17) return std::unexpected(std::move(result17.error()));
auto result18 = m_movement_controller->load_state(std::move(*result17));
if (!result18) return std::unexpected(std::move(result18.error()));

  auto result19 = diskStore.get("statusController");
if (!result19) return std::unexpected(std::move(result19.error()));
auto result20 = m_status_controller->diskLoad(std::move(*result19));
if (!result20) return std::unexpected(std::move(result20.error()));

  auto result21 = diskStore.get_bool("damageOnTouch");
if (!result21) return std::unexpected(std::move(result21.error()));
m_damage_on_touch = std::move(*result21);
  auto result22 = diskStore.get_bool("aggressive");
if (!result22) return std::unexpected(std::move(result22.error()));
m_aggressive = std::move(*result22);
  auto result23 = diskStore.get_string("deathParticleBurst");
if (!result23) return std::unexpected(std::move(result23.error()));
m_death_particle_burst = std::move(*result23);
  auto result24 = diskStore.get_string("deathSound");
if (!result24) return std::unexpected(std::move(result24.error()));
m_death_sound = std::move(*result24);
  auto result25 = diskStore.get_string("activeSkillName");
if (!result25) return std::unexpected(std::move(result25.error()));
m_active_skill_name = std::move(*result25);
  auto result26 = diskStore.get("dropPool");
if (!result26) return std::unexpected(std::move(result26.error()));
m_drop_pool = std::move(*result26);
  auto result27 = diskStore.get("effectEmitter");
if (!result27) return std::unexpected(std::move(result27.error()));
auto result28 = m_effect_emitter->from_json(std::move(*result27));
if (!result28) return std::unexpected(std::move(result28.error()));

  auto result29 = diskStore.get_object("scriptStorage");
if (!result29) return std::unexpected(std::move(result29.error()));
auto result30 = m_script_component.set_script_storage(std::move(*result29));
if (!result30) return std::unexpected(std::move(result30.error()));


  auto result31 = diskStore.opt_string("uniqueId");
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
  auto result35 = m_movement_controller->store_state();
if (!result35) return std::unexpected(std::move(result35.error()));
auto result36 = m_status_controller->diskStore();
if (!result36) return std::unexpected(std::move(result36.error()));
auto result37 = m_effect_emitter->to_json();
if (!result37) return std::unexpected(std::move(result37.error()));
auto result38 = this->root().monster_database();
if (!result38) return std::unexpected(std::move(result38.error()));
auto result39 = (*result38)->write_monster_variant_to_json(m_monster_variant);
if (!result39) return std::unexpected(std::move(result39.error()));
auto result40 = m_script_component.get_script_storage();
if (!result40) return std::unexpected(std::move(result40.error()));
auto result41 = getTeam().to_json();
if (!result41) return std::unexpected(std::move(result41.error()));
return JsonObject{
    {"monsterLevel", json_from_optional(m_monster_level)},
    {"movementState", std::move(*result35)},
    {"statusController", std::move(*result36)},
    {"damageOnTouch", m_damage_on_touch},
    {"aggressive", aggressive()},
    {"deathParticleBurst", m_death_particle_burst},
    {"deathSound", m_death_sound},
    {"activeSkillName", m_active_skill_name},
    {"dropPool", m_drop_pool},
    {"effectEmitter", std::move(*result37)},
    {"monsterVariant", std::move(*result39)},
    {"scriptStorage", std::move(*result40)},
    {"uniqueId", json_from_optional(uniqueId())},
    {"team", std::move(*result41)}
  };
}

std::expected<ByteArray, StarException> Monster::net_store(NetCompatibilityRules rules) {
  auto result42 = this->root().monster_database();
if (!result42) return std::unexpected(std::move(result42.error()));
return (*result42)->write_monster_variant(m_monster_variant, rules);
}

EntityType Monster::entity_type() const {
  return EntityType::Monster;
}

ClientEntityMode Monster::client_entity_mode() const {
  return m_monster_variant.client_entity_mode;
}

std::expected<void, StarException> Monster::init(World& world, EntityId entityId, EntityMode mode) {
  auto result43 = Entity::init(world, entityId, mode);
if (!result43) return std::unexpected(std::move(result43.error()));


  m_movement_controller->init(world);
  m_movement_controller->setIgnorePhysicsEntities({entityId});
  auto result44 = m_status_controller->init(*this, *m_movement_controller);
if (!result44) return std::unexpected(std::move(result44.error()));


  if (!m_monster_level)
    {
  auto result45 = world.threatLevel();
  if (!result45) return std::unexpected(std::move(result45.error()));
  m_monster_level = std::move(*result45);
}

  if (isMaster()) {
    auto result46 = this->root().functionDatabase();
if (!result46) return std::unexpected(std::move(result46.error()));
auto functionDatabase = std::move(*result46);
    auto result47 = functionDatabase->function(m_monster_variant.health_level_function);
if (!result47) return std::unexpected(std::move(result47.error()));
auto result48 = monster_level();
if (!result48) return std::unexpected(std::move(result48.error()));
auto result49 = std::move(*result47)->evaluate(std::move(*result48));
if (!result49) return std::unexpected(std::move(result49.error()));
float healthMultiplier = m_monster_variant.health_multiplier * std::move(*result49);
    auto result50 = m_status_controller->setPersistentEffects("innate", {StatModifier(StatBaseMultiplier{"maxHealth", healthMultiplier})});
if (!result50) return std::unexpected(std::move(result50.error()));


    auto result51 = makeMonsterCallbacks();
if (!result51) return std::unexpected(std::move(result51.error()));
auto result52 = m_script_component.add_callbacks("monster", std::move(*result51));
if (!result52) return std::unexpected(std::move(result52.error()));

    auto result53 = LuaBindings::makeConfigCallbacks([this](String const& name, Json const& def) {
        return m_monster_variant.parameters.query(name, def);
      });
if (!result53) return std::unexpected(std::move(result53.error()));
auto result54 = m_script_component.add_callbacks("config", std::move(*result53));
if (!result54) return std::unexpected(std::move(result54.error()));

    auto result55 = LuaBindings::makeEntityCallbacks(*this);
if (!result55) return std::unexpected(std::move(result55.error()));
auto result56 = m_script_component.add_callbacks("entity", std::move(*result55));
if (!result56) return std::unexpected(std::move(result56.error()));

    auto result57 = LuaBindings::makeNetworkedAnimatorCallbacks(*m_networked_animator);
if (!result57) return std::unexpected(std::move(result57.error()));
auto result58 = m_script_component.add_callbacks("animator", std::move(*result57));
if (!result58) return std::unexpected(std::move(result58.error()));

    auto result59 = LuaBindings::makeStatusControllerCallbacks(*m_status_controller);
if (!result59) return std::unexpected(std::move(result59.error()));
auto result60 = m_script_component.add_callbacks("status", std::move(*result59));
if (!result60) return std::unexpected(std::move(result60.error()));

    auto result61 = LuaBindings::make_behavior_callbacks(this->root(), m_behaviors);
if (!result61) return std::unexpected(std::move(result61.error()));
auto result62 = m_script_component.add_callbacks("behavior", std::move(*result61));
if (!result62) return std::unexpected(std::move(result62.error()));

    auto result63 = m_script_component.add_actor_movement_callbacks(*m_movement_controller);
if (!result63) return std::unexpected(std::move(result63.error()));

    auto result64 = recover_lua_script_error(m_script_component.init(world));
if (!result64) return std::unexpected(std::move(result64.error()));

  }

  if (world.isClient()) {
    auto result65 = m_scripted_animator.set_scripts(m_monster_variant.animation_scripts);
if (!result65) return std::unexpected(std::move(result65.error()));


    auto result66 = LuaBindings::makeScriptedAnimatorCallbacks(*m_networked_animator,
      [this](String const& name, Json const& defaultValue) -> Json {
        return m_scripted_animation_parameters.value(name, defaultValue);
      });
if (!result66) return std::unexpected(std::move(result66.error()));
auto result67 = m_scripted_animator.add_callbacks("animationConfig", std::move(*result66));
if (!result67) return std::unexpected(std::move(result67.error()));

    auto result68 = LuaBindings::makeConfigCallbacks([this](String const& name, Json const& def) {
        return m_monster_variant.parameters.query(name, def);
      });
if (!result68) return std::unexpected(std::move(result68.error()));
auto result69 = m_scripted_animator.add_callbacks("config", std::move(*result68));
if (!result69) return std::unexpected(std::move(result69.error()));

    auto result70 = LuaBindings::makeEntityCallbacks(*this);
if (!result70) return std::unexpected(std::move(result70.error()));
auto result71 = m_scripted_animator.add_callbacks("entity", std::move(*result70));
if (!result71) return std::unexpected(std::move(result71.error()));

    auto result72 = recover_lua_script_error(m_scripted_animator.init(world));
if (!result72) return std::unexpected(std::move(result72.error()));

  }

  setPosition(position());
  return {};
}

std::expected<void, StarException> Monster::uninit() {
  if (isMaster()) {
    auto result73 = recover_lua_script_error(m_script_component.uninit());
if (!result73) return std::unexpected(std::move(result73.error()));

    auto result74 = m_script_component.remove_callbacks("monster");
if (!result74) return std::unexpected(std::move(result74.error()));

    auto result75 = m_script_component.remove_callbacks("config");
if (!result75) return std::unexpected(std::move(result75.error()));

    auto result76 = m_script_component.remove_callbacks("entity");
if (!result76) return std::unexpected(std::move(result76.error()));

    auto result77 = m_script_component.remove_callbacks("animator");
if (!result77) return std::unexpected(std::move(result77.error()));

    auto result78 = m_script_component.remove_callbacks("status");
if (!result78) return std::unexpected(std::move(result78.error()));

    auto result79 = m_script_component.remove_actor_movement_callbacks();
if (!result79) return std::unexpected(std::move(result79.error()));

  }
  {
  auto result80 = world();
  if (!result80) return std::unexpected(std::move(result80.error()));
  if (result80->get().isClient()) {
      auto result81 = m_scripted_animator.remove_callbacks("animationConfig");
  if (!result81) return std::unexpected(std::move(result81.error()));

      auto result82 = m_scripted_animator.remove_callbacks("config");
  if (!result82) return std::unexpected(std::move(result82.error()));

      auto result83 = m_scripted_animator.remove_callbacks("entity");
  if (!result83) return std::unexpected(std::move(result83.error()));

    }
}
  auto result84 = m_status_controller->uninit();
if (!result84) return std::unexpected(std::move(result84.error()));

  m_movement_controller->uninit();
  auto result85 = Entity::uninit();
if (!result85) return std::unexpected(std::move(result85.error()));

  return {};
}

Vec2F Monster::mouth_offset() const {
  return absolute_position(m_monster_variant.mouth_offset) - position();
}

Vec2F Monster::feet_offset() const {
  return absolute_position(m_monster_variant.feet_offset) - position();
}

Vec2F Monster::position() const {
  return m_movement_controller->position();
}

RectF Monster::meta_bound_box() const {
  return m_monster_variant.meta_bound_box;
}

RectF Monster::collision_area() const {
  return m_movement_controller->collisionPoly().bounding_box();
}

Vec2F Monster::velocity() const {
  return m_movement_controller->velocity();
}

std::expected<std::pair<ByteArray, std::uint64_t>, StarException> Monster::write_net_state(std::uint64_t fromVersion, NetCompatibilityRules rules) {
  return m_net_group.write_net_state(fromVersion, rules);
}

std::expected<void, StarException> Monster::read_net_state(ByteArray data, float interpolationTime, NetCompatibilityRules rules) {
  return m_net_group.read_net_state(std::move(data), interpolationTime, rules);
}

std::expected<void, StarException> Monster::enable_interpolation(float extrapolation_hint) {
  auto result89 = m_net_group.enable_net_interpolation(extrapolation_hint);
if (!result89) return std::unexpected(std::move(result89.error()));

  return {};
}

std::expected<void, StarException> Monster::disable_interpolation() {
  auto result90 = m_net_group.disable_net_interpolation();
if (!result90) return std::unexpected(std::move(result90.error()));

  return {};
}

std::expected<String, StarException> Monster::name() const {
  auto const& name = m_name.get();
  return (name ? name : m_monster_variant.short_description).value_or("");
}

std::expected<String, StarException> Monster::description() const {
  return m_monster_variant.description.value_or("Some indescribable horror");
}

std::expected<std::optional<HitType>, StarException> Monster::queryHit(DamageSource const& source) const {
  if (!inWorld() || m_knocked_out || m_status_controller->statPositive("invulnerable"))
    return {};

  auto body = hitPoly();
  if (!body) return std::unexpected(std::move(body.error()));
  if (!*body) return std::unexpected(MonsterException("Monster has no hit polygon"));
  {
  auto result91 = world();
  if (!result91) return std::unexpected(std::move(result91.error()));
  auto result92 = source.intersects_with_poly(result91->get().geometry(), **body);
  if (!result92) return std::unexpected(std::move(result92.error()));
  if (std::move(*result92))
      return HitType::Hit;
}

  return {};
}

std::expected<std::optional<PolyF>, StarException> Monster::hitPoly() const {
  PolyF hitBody = m_monster_variant.self_damage_poly;
  hitBody.rotate(m_movement_controller->rotation());
  hitBody.translate(position());
  return hitBody;
}

std::expected<List<DamageNotification>, StarException> Monster::applyDamage(DamageRequest const& damage) {
  if (!inWorld())
    return {};

  auto result93 = m_status_controller->apply_damage_request(damage);
if (!result93) return std::unexpected(std::move(result93.error()));
auto notifications = std::move(*result93);

  float totalDamage = 0.0f;
  for (auto const& notification : notifications)
    totalDamage += notification.health_lost;

  if (totalDamage > 0.0f) {
    auto result94 = recover_lua_script_error(m_script_component.invoke("damage", JsonObject{
        {"sourceId", damage.source_entity_id},
        {"damage", totalDamage},
        {"sourceDamage", damage.damage},
        {"sourceKind", damage.damage_source_kind}
      }));
if (!result94) return std::unexpected(std::move(result94.error()));

  }

  if (!m_status_controller->resourcePositive("health"))
    m_death_damage_source_kinds.add(damage.damage_source_kind);

  return notifications;
}

std::expected<List<DamageNotification>, StarException> Monster::selfDamageNotifications() {
  return m_status_controller->pull_self_damage_notifications();
}

std::expected<float, StarException> Monster::damage_level_power() const {
  auto functions = root().functionDatabase();
  if (!functions)
    return std::unexpected(std::move(functions.error()));

  // A root reload may replace the catalog. Compare owners without keeping the
  // old database alive, and only cache a successfully evaluated exact level.
  if (m_monster_level
      && m_damage_level_power.level_bits == std::bit_cast<std::uint32_t>(*m_monster_level)
      && !m_damage_level_power.functions.owner_before(*functions)
      && !functions->owner_before(m_damage_level_power.functions))
    return m_damage_level_power.multiplier;

  auto function = (*functions)->function(m_monster_variant.power_level_function);
  if (!function)
    return std::unexpected(std::move(function.error()));
  auto level = monster_level();
  if (!level)
    return std::unexpected(std::move(level.error()));
  auto multiplier = (*function)->evaluate(*level);
  if (!multiplier)
    return std::unexpected(std::move(multiplier.error()));

  m_damage_level_power = {*functions, std::bit_cast<std::uint32_t>(*level), static_cast<float>(*multiplier)};
  return m_damage_level_power.multiplier;
}

std::expected<List<DamageSource>, StarException> Monster::damage_sources() const {
  auto level_power = damage_level_power();
  if (!level_power)
    return std::unexpected(std::move(level_power.error()));

  auto const& scripted_sources = m_damage_sources.get();
  auto const& active_parts = m_animation_damage_parts.get();
  bool damage_on_touch = m_damage_on_touch && !m_monster_variant.touch_damage_config.is_null();
  if (!damage_on_touch && active_parts.empty())
    return scripted_sources;

  List<DamageSource> sources;
  if (damage_on_touch)
    sources.reserve(scripted_sources.size() + 1);
  sources.append_range(scripted_sources);

  std::optional<float> status_power;
  auto status_multiplier = [&] {
    if (!status_power)
      status_power = m_status_controller->stat("powerMultiplier");
    return *status_power;
  };

  if (damage_on_touch) {
    if (!m_touch_damage_source_template) {
      auto source = DamageSource::create(m_monster_variant.touch_damage_config);
      if (!source)
        return std::unexpected(std::move(source.error()));
      m_touch_damage_source_template.emplace(std::move(*source));
    }
    auto& touch = sources.emplace_back(*m_touch_damage_source_template);
    if (std::holds_alternative<PolyF>(touch.damage_area))
      std::get<PolyF>(touch.damage_area).rotate(m_movement_controller->rotation());
    touch.damage *= m_monster_variant.touch_damage_multiplier * *level_power * status_multiplier();
    touch.source_entity_id = entityId();
    touch.team = getTeam();
  }

  if (active_parts.empty())
    return sources;

  // Animations can change between queries. Within this query, sources anchored
  // to the same part (including their knockback) use one current transform.
  using AnchorTransform = std::pair<std::reference_wrapper<String const>, Mat3F>;
  SmallVector<AnchorTransform, 4> transforms;
  auto anchor_transform = [&](String const& anchor) -> std::expected<std::reference_wrapper<Mat3F const>, StarException> {
    auto found = std::ranges::find_if(transforms, [&](AnchorTransform const& entry) { return entry.first.get() == anchor; });
    if (found != transforms.end())
      return std::cref(found->second);
    auto transform = m_networked_animator->partTransformation(anchor);
    if (!transform)
      return std::unexpected(std::move(transform.error()));
    transforms.emplace_back(std::cref(anchor), std::move(*transform));
    return std::cref(transforms[transforms.size() - 1].second);
  };

  bool flipped = m_networked_animator->flipped();
  float flip_center = flipped ? m_networked_animator->flippedRelativeCenterLine() : 0.0f;
  auto const& part_configs = m_monster_variant.animation_damage_parts;
  std::size_t next_index = 0;
  for (auto const& [name, config] : part_configs) {
    auto index = next_index++;
    if (!active_parts.contains(name))
      continue;

    if (m_animation_damage_source_templates.empty())
      m_animation_damage_source_templates.resize(part_configs.size());
    auto& part = m_animation_damage_source_templates[index];
    if (!part) {
      auto anchor = config.get_string("anchorPart");
      if (!anchor)
        return std::unexpected(std::move(anchor.error()));
      auto damage_config = config.get("damageSource");
      if (!damage_config)
        return std::unexpected(std::move(damage_config.error()));
      auto source = DamageSource::create(*damage_config);
      if (!source)
        return std::unexpected(std::move(source.error()));
      part.emplace(AnimationDamageSourceTemplate{std::move(*anchor), std::move(*source), std::nullopt});
    }

    auto const& source = part->source;
    bool directional = std::holds_alternative<Vec2F>(source.knockback);
    std::optional<std::reference_wrapper<Mat3F const>> transform;
    if (!std::holds_alternative<std::monostate>(source.damage_area) || directional) {
      auto resolved = anchor_transform(part->anchor_part);
      if (!resolved)
        return std::unexpected(std::move(resolved.error()));
      transform = *resolved;
    }

    bool line = std::holds_alternative<Line2F>(source.damage_area);
    if (line) {
      // Resolve the live anchor before parsing line settings, even when the
      // flag is false and the line will not emit any segments.
      if (!part->line_collision) {
        auto enabled = config.get_bool("checkLineCollision", false);
        if (!enabled)
          return std::unexpected(std::move(enabled.error()));
        std::int64_t bounces = 0;
        if (*enabled) {
          auto count = config.get_int("bounces", 0);
          if (!count)
            return std::unexpected(std::move(count.error()));
          bounces = *count;
        }
        part->line_collision.emplace(AnimationDamageSourceTemplate::LineCollision{*enabled, bounces});
      }
      if (!part->line_collision->enabled)
        continue;
    }

    std::optional<Vec2F> knockback;
    if (directional) {
      knockback = transform->get().transform_point(std::get<Vec2F>(source.knockback));
      if (flipped)
        (*knockback)[0] = -(*knockback)[0];
    }
    float part_power = *level_power * status_multiplier();
    auto append_source = [&]() -> DamageSource& {
      auto& output = sources.emplace_back(source);
      output.damage *= part_power;
      if (knockback)
        output.knockback = *knockback;
      return output;
    };

    if (!line) {
      auto& output = append_source();
      if (std::holds_alternative<PolyF>(output.damage_area)) {
        auto& poly = std::get<PolyF>(output.damage_area);
        poly.transform(transform->get());
        if (flipped)
          poly.flip_horizontal(flip_center);
      }
      continue;
    }

    auto local_line = std::get<Line2F>(source.damage_area).transformed(transform->get());
    if (flipped)
      local_line.flip_horizontal(flip_center);

    auto current_world = world();
    if (!current_world)
      return std::unexpected(std::move(current_world.error()));
    auto origin = position();
    auto world_line = local_line.translated(origin);
    auto bounces = part->line_collision->bounces;
    float remaining = bounces > 0 ? world_line.length() : 0.0f;
    bool reflected = false;
    while (true) {
      auto collision = current_world->get().lineTileCollisionPoint(world_line.min(), world_line.max());
      if (!collision)
        return std::unexpected(std::move(collision.error()));
      if (*collision)
        world_line.max() = (**collision).first;
      // The first unobstructed segment retains its exact local coordinates.
      append_source().damage_area = *collision || reflected ? world_line.translated(-origin) : local_line;
      if (!*collision || bounces <= 0)
        break;

      remaining -= world_line.length();
      if (!(remaining > 0.0f))
        break;
      --bounces;
      Vec2F direction = world_line.direction();
      Vec2F normal((**collision).second);
      Vec2F reflection = direction - (2 * direction.piecewise_multiply(normal).sum() * normal);
      if (knockback) {
        auto incoming = world_line.direction();
        // Rounded normalization can perturb acos(-1) on exact reversal.
        auto angle = incoming != Vec2F{} && reflection == -incoming
            ? std::numbers::pi_v<float> : reflection.angle_between(incoming);
        *knockback = knockback->rotate(angle);
      }

      world_line.min() = (**collision).first;
      world_line.max() = world_line.min() + reflection * remaining;
      reflected = true;
    }
  }
  return sources;
}

std::expected<bool, StarException> Monster::should_die() {
  auto result = recover_lua_script_error(m_script_component.invoke<bool>("shouldDie"));
  if (!result)
    return std::unexpected(std::move(result.error()));
  if (*result)
    return **result;
  return !m_status_controller->resourcePositive("health") || m_script_component.error();
}

std::expected<void, StarException> Monster::knockout() {
  m_knocked_out = true;
  auto result113 = m_monster_variant.parameters.get_float("knockoutTime", 1.0f);
if (!result113) return std::unexpected(std::move(result113.error()));
m_knockout_timer = std::move(*result113);

  m_damage_on_touch = false;

  auto result114 = m_monster_variant.parameters.get_string("knockoutEffect");
if (!result114) return std::unexpected(std::move(result114.error()));
String knockoutEffect = std::move(*result114);
  if (!knockoutEffect.empty())
    {
  auto result115 = m_networked_animator->setEffectEnabled(knockoutEffect, true);
  if (!result115) return std::unexpected(std::move(result115.error()));

}

  auto result116 = m_monster_variant.parameters.get_object("knockoutAnimationStates", JsonObject());
if (!result116) return std::unexpected(std::move(result116.error()));
auto knockoutAnimationStates = std::move(*result116);
for (auto const& pair : knockoutAnimationStates) {
  auto result117 = pair.second.to_string();
  if (!result117) return std::unexpected(std::move(result117.error()));
  auto result118 = m_networked_animator->setState(pair.first, std::move(*result117));
  if (!result118) return std::unexpected(std::move(result118.error()));
}
  return {};
}

std::expected<bool, StarException> Monster::should_destroy() const {
  return m_knocked_out && m_knockout_timer <= 0;
}

std::expected<void, StarException> Monster::destroy(std::optional<std::reference_wrapper<RenderCallback>> render_callback) {
  auto result119 = recover_lua_script_error(m_script_component.invoke("die"));
if (!result119) return std::unexpected(std::move(result119.error()));


  if (isMaster() && !m_drop_pool.is_null()) {
    auto result120 = this->root().treasureDatabase();
if (!result120) return std::unexpected(std::move(result120.error()));
auto treasureDatabase = std::move(*result120);

    String treasurePool;
    if (m_drop_pool.is_type(Json::Type::String)) {
      auto result121 = m_drop_pool.to_string();
if (!result121) return std::unexpected(std::move(result121.error()));
treasurePool = std::move(*result121);
    } else {
      // Check to see whether any of the damage types that were used to cause
      // death are in the damage pool map, if so spawn treasure from that,
      // otherwise set the treasure pool to the "default" entry.

      for (auto const& damageSourceKind : m_death_damage_source_kinds) {
        {
  auto result122 = m_drop_pool.contains(damageSourceKind);
  if (!result122) return std::unexpected(std::move(result122.error()));
  if (std::move(*result122))
            {
    auto result123 = m_drop_pool.get_string(damageSourceKind);
    if (!result123) return std::unexpected(std::move(result123.error()));
    treasurePool = std::move(*result123);
  }
}
      }

      if (treasurePool.empty())
        {
  auto result124 = m_drop_pool.get_string("default");
  if (!result124) return std::unexpected(std::move(result124.error()));
  treasurePool = std::move(*result124);
}
    }

    auto dropped = [&]() -> std::expected<void, StarException> {
      {
  auto result125 = monster_level();
  if (!result125) return std::unexpected(std::move(result125.error()));
  auto result126 = treasureDatabase->createTreasure(treasurePool, std::move(*result125));
  if (!result126) return std::unexpected(std::move(result126.error()));
  for (auto const& treasure_item : std::move(*result126))
          {
    auto result127 = world();
    if (!result127) return std::unexpected(std::move(result127.error()));
    auto result128 = ItemDrop::create_randomized_drop(this->root(), treasure_item, position());
    if (!result128) return std::unexpected(std::move(result128.error()));
    auto result129 = result127->get().addEntity(std::move(*result128));
    if (!result129) return std::unexpected(std::move(result129.error()));

  }
}
      return {};
    }();
    if (!dropped) {
      auto const& e = dropped.error();
      auto result130 = this->root().logger().warn_exception(
        Star::format("Failed to create treasure for monster '{}'", m_monster_variant.type), ExceptionDiagnostic(e));
      if (!result130)
        return std::unexpected(std::move(result130.error()));
    }
  }

  if (render_callback) {
    if (!m_death_particle_burst.empty())
      {
  auto result131 = m_networked_animator->burstParticleEmitter(m_death_particle_burst);
  if (!result131) return std::unexpected(std::move(result131.error()));

}

    if (!m_death_sound.empty())
      {
  auto result132 = m_networked_animator->playSound(m_death_sound);
  if (!result132) return std::unexpected(std::move(result132.error()));

}

    auto result133 = m_networked_animator->update(0.0, std::ref(m_networked_animator_dynamic_target));
if (!result133) return std::unexpected(std::move(result133.error()));


    auto result134 = render_callback->get().add_audios(m_networked_animator_dynamic_target.pullNewAudios());
if (!result134) return std::unexpected(std::move(result134.error()));
    auto result135 = render_callback->get().add_particles(m_networked_animator_dynamic_target.pullNewParticles());
if (!result135) return std::unexpected(std::move(result135.error()));
    auto result136 = render_callback->get().add_particles(m_status_controller->pullNewParticles());
if (!result136) return std::unexpected(std::move(result136.error()));
  }

  m_death_damage_source_kinds.clear();

  if (isMaster())
    {
  auto result137 = setNetStates();
  if (!result137) return std::unexpected(std::move(result137.error()));

}
  return {};
}

std::expected<List<LightSource>, StarException> Monster::light_sources() const {
  auto lights = m_networked_animator->light_sources(position());
  if (!lights)
    return std::unexpected(std::move(lights.error()));
  if (auto result = m_status_controller->append_light_sources(*lights); !result)
    return std::unexpected(std::move(result.error()));
  return lights;
}

std::expected<void, StarException> Monster::hitOther(EntityId targetEntityId, DamageRequest const& damageRequest) {
  if (inWorld() && isMaster())
    m_status_controller->hit_other(targetEntityId, damageRequest);
  return {};
}

std::expected<void, StarException> Monster::damagedOther(DamageNotification const& damage) {
  if (inWorld() && isMaster())
    m_status_controller->damaged_other(damage);
  return {};
}

std::expected<void, StarException> Monster::update(float dt, std::uint64_t) {
  if (!inWorld())
    return {};

  m_movement_controller->setTimestep(dt);

  if (isMaster()) {
    m_networked_animator->setFlipped((m_movement_controller->facing_direction() == Direction::Left) != m_monster_variant.reversed);

    if (m_knocked_out) {
      m_knockout_timer -= dt;
    } else {
      if (m_script_component.update_ready())
        m_physics_forces.set({});
      auto script = recover_lua_script_error(m_script_component.update(m_script_component.update_dt(dt)));
      if (!script) return std::unexpected(std::move(script.error()));
      auto dying = should_die();
      if (!dying) return std::unexpected(std::move(dying.error()));
      if (*dying) {
        if (auto result = knockout(); !result) return result;
      }
    }

    if (auto result = m_movement_controller->tick_master(dt); !result) return result;
    if (auto result = m_status_controller->tickMaster(dt); !result) return result;
    if (auto result = update_status(dt); !result) return result;
  } else {
    if (auto result = m_net_group.tick_net_interpolation(dt); !result) return result;
    if (auto result = m_status_controller->tickSlave(dt); !result) return result;
    if (auto result = update_status(dt); !result) return result;
    if (auto result = m_movement_controller->tick_slave(dt); !result) return result;
  }

  auto current_world = world();
  if (!current_world) return std::unexpected(std::move(current_world.error()));
  if (current_world->get().isServer())
    return m_networked_animator->update(dt, std::nullopt);

  if (auto result = m_networked_animator->update(dt, std::ref(m_networked_animator_dynamic_target)); !result) return result;
  m_networked_animator_dynamic_target.updatePosition(position());
  auto animation = recover_lua_script_error(m_scripted_animator.update());
  if (!animation) return std::unexpected(std::move(animation.error()));
  auto& spatial_log = root().spatialLogger();
  if (spatial_log.observed())
    spatial_log.log_poly("world", m_movement_controller->collisionBody(), {255, 0, 0, 255});
  return {};
}

std::uint64_t Monster::presentation_epoch() const {
  return Entity::presentation_epoch() + m_movement_controller->presentation_epoch();
}

std::expected<void, StarException> Monster::collectRenderEvents(RenderCallback& render_callback) {
  auto result158 = render_callback.add_audios(m_networked_animator_dynamic_target.pullNewAudios());
if (!result158) return std::unexpected(std::move(result158.error()));
  auto result159 = render_callback.add_particles(m_networked_animator_dynamic_target.pullNewParticles());
if (!result159) return std::unexpected(std::move(result159.error()));
  auto result162 = render_callback.add_particles(m_status_controller->pullNewParticles());
if (!result162) return std::unexpected(std::move(result162.error()));
  auto result163 = render_callback.add_audios(m_status_controller->pullNewAudios());
if (!result163) return std::unexpected(std::move(result163.error()));

  auto result164 = m_effect_emitter->collect_render_events(render_callback);
if (!result164) return std::unexpected(std::move(result164.error()));
  auto result166 = render_callback.add_audios(m_scripted_animator.pull_new_audios());
if (!result166) return std::unexpected(std::move(result166.error()));
  auto result167 = render_callback.add_particles(m_scripted_animator.pull_new_particles());
if (!result167) return std::unexpected(std::move(result167.error()));
  return {};
}

std::expected<void, StarException> Monster::render(RenderCallback& render_callback) {
  auto interpolation = render_callback.presentation().interpolation();
  auto sample = m_networked_animator->sample(interpolation);
  auto parts = sample.drawables(position());
  if (!parts) return std::unexpected(std::move(parts.error()));
  auto layer = m_monster_variant.render_layer;
  auto const& directives = m_status_controller->parentDirectives();
  std::shared_ptr<ImageMetadataDatabase const> metadata;
  for (auto& drawable : *parts) {
    if (!directives.empty() && drawable.is_image()) {
      if (!metadata) {
        auto loaded = root().image_metadata_database();
        if (!loaded) return std::unexpected(std::move(loaded.error()));
        metadata = std::move(*loaded);
      }
      auto applied = drawable.image_part().add_directives_group(*metadata, directives, true);
      if (!applied) return std::unexpected(std::move(applied.error()));
    }
    if (auto result = render_callback.add_drawable(std::move(drawable), layer); !result) return result;
  }

  auto effects = m_status_controller->drawables(interpolation);
  if (!effects) return std::unexpected(std::move(effects.error()));
  if (auto result = render_callback.add_drawables(std::move(*effects), layer); !result) return result;
  for (auto const& [drawable, override_layer] : m_scripted_animator.drawables()) {
    if (auto result = render_callback.add_drawable(drawable, override_layer.value_or(layer)); !result) return result;
  }
  return {};
}

std::expected<void, StarException> Monster::render_light_sources(RenderCallback& render_callback) {
  auto sample = m_networked_animator->sample(render_callback.presentation().interpolation());
  auto lights = sample.light_sources(position());
  if (!lights)
    return std::unexpected(std::move(lights.error()));
  if (auto added = render_callback.add_light_sources(*lights); !added)
    return std::unexpected(std::move(added.error()));
  lights->clear();
  if (auto result = m_status_controller->append_light_sources(*lights, render_callback.presentation().interpolation()); !result)
    return std::unexpected(std::move(result.error()));
  if (auto added = render_callback.add_light_sources(*lights); !added)
    return std::unexpected(std::move(added.error()));
  return render_callback.add_light_sources(m_scripted_animator.light_sources());
}

void Monster::setPosition(Vec2F const& pos) {
  resetPresentation();
  m_movement_controller->setPosition(pos);
}

MessageResponseResult Monster::receive_message(ConnectionId sending_connection, String const& message, JsonArray const& args) {
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

std::expected<float, StarException> Monster::maxHealth() const {
  auto maximum = m_status_controller->resourceMax("health");
  if (!maximum) return std::unexpected(std::move(maximum.error()));
  if (!*maximum) return std::unexpected(MonsterException("Health resource has no maximum"));
  return **maximum;
}

float Monster::health() const {
  return m_status_controller->resource("health");
}

DamageBarType Monster::damageBar() const {
  return m_damage_bar.get();
}

Vec2F Monster::absolute_position(Vec2F relative_position) const {
  if (m_movement_controller->facing_direction() == Direction::Left)
    relative_position[0] *= -1;
  if (auto rotation = m_movement_controller->rotation(); rotation != 0)
    relative_position = relative_position.rotate(rotation);
  return position() + relative_position;
}

std::expected<void, StarException> Monster::update_status(float dt) {
  auto origin = position();
  m_effect_emitter->set_source_position("normal", origin);
  m_effect_emitter->set_source_position("mouth", origin + mouth_offset());
  m_effect_emitter->set_source_position("feet", origin + feet_offset());
  m_effect_emitter->set_direction(m_movement_controller->facing_direction());
  return m_effect_emitter->tick(dt, *entityMode());
}

std::expected<LuaCallbacks, StarException> Monster::makeMonsterCallbacks() {
  LuaCallbacks callbacks;

  auto result180 = callbacks.register_callback("type", [this]() {
      return m_monster_variant.type;
    });
if (!result180) return std::unexpected(std::move(result180.error()));

  auto result181 = callbacks.register_callback("seed", [this]() {
      return Star::to_string(m_monster_variant.seed);
    });
if (!result181) return std::unexpected(std::move(result181.error()));

  auto result182 = callbacks.register_callback("uniqueParameters", [this]() {
      return m_monster_variant.unique_parameters;
    });
if (!result182) return std::unexpected(std::move(result182.error()));

  auto result183 = callbacks.register_callback("level", [this]() {
      return monster_level();
    });
if (!result183) return std::unexpected(std::move(result183.error()));

  auto result184 = callbacks.register_callback("setDamageOnTouch", [this](bool arg1) {
      m_damage_on_touch = arg1;
    });
if (!result184) return std::unexpected(std::move(result184.error()));

  auto result185 = callbacks.register_callback("setDamageSources", [this](std::optional<JsonArray> const& damageSources) -> std::expected<void, StarException> {
      List<DamageSource> parsed;
      for (auto const& source : damageSources.value_or(JsonArray{})) {
        auto damage = DamageSource::create(source);
        if (!damage) return std::unexpected(std::move(damage.error()));
        parsed.push_back(std::move(*damage));
      }
      m_damage_sources.set(std::move(parsed));
      return {};
    });
if (!result185) return std::unexpected(std::move(result185.error()));

  auto result186 = callbacks.register_callback("setDamageParts", [this](StringSet const& parts) {
      m_animation_damage_parts.set(parts);
    });
if (!result186) return std::unexpected(std::move(result186.error()));

  auto result187 = callbacks.register_callback("setAggressive", [this](bool arg1) {
      m_aggressive = arg1;
    });
if (!result187) return std::unexpected(std::move(result187.error()));

  auto result188 = callbacks.register_callback("setActiveSkillName", [this](std::optional<String> const& activeSkillName) {
      m_active_skill_name = activeSkillName.value_or(String{});
    });
if (!result188) return std::unexpected(std::move(result188.error()));

  auto result189 = callbacks.register_callback("setDropPool", [this](Json dropPool) {
      m_drop_pool = std::move(dropPool);
    });
if (!result189) return std::unexpected(std::move(result189.error()));

  auto result190 = callbacks.register_callback("toAbsolutePosition", [this](Vec2F const& p) {
      return absolute_position(p);
    });
if (!result190) return std::unexpected(std::move(result190.error()));

  auto result191 = callbacks.register_callback("mouthPosition", [this]() {
      return mouth_position();
    });
if (!result191) return std::unexpected(std::move(result191.error()));

  // This callback is registered here rather than in
  // makeActorMovementControllerCallbacks
  // because it requires access to world
  auto result193 = callbacks.register_callback("flyTo", [this](Vec2F const& arg1) -> std::expected<void, StarException> {
      auto result192 = world();
if (!result192) return std::unexpected(std::move(result192.error()));
m_movement_controller->control_fly(result192->get().geometry().diff(arg1, position()));
      return {};
});
if (!result193) return std::unexpected(std::move(result193.error()));

  auto result194 = callbacks.register_callback("setDeathParticleBurst", [this](std::optional<String> const& arg1) {
      m_death_particle_burst = arg1.value_or(String{});
    });
if (!result194) return std::unexpected(std::move(result194.error()));

  auto result195 = callbacks.register_callback("setDeathSound", [this](std::optional<String> const& arg1) {
      m_death_sound = arg1.value_or(String{});
    });
if (!result195) return std::unexpected(std::move(result195.error()));

  auto result196 = callbacks.register_callback("setPhysicsForces", [this](JsonArray const& forces) -> std::expected<void, StarException> {
      List<PhysicsForceRegion> parsed;
      for (auto const& force : forces) {
        auto region = json_to_physics_force_region(force);
        if (!region) return std::unexpected(std::move(region.error()));
        parsed.push_back(std::move(*region));
      }
      m_physics_forces.set(std::move(parsed));
      return {};
    });
if (!result196) return std::unexpected(std::move(result196.error()));

  auto result197 = callbacks.register_callback("setName", [this](String const& name) {
      m_name.set(name);
    });
if (!result197) return std::unexpected(std::move(result197.error()));
  auto result198 = callbacks.register_callback("setDisplayNametag", [this](bool display) {
      m_display_nametag.set(display);
    });
if (!result198) return std::unexpected(std::move(result198.error()));

  auto result200 = callbacks.register_callback("say", [this](String line, std::optional<StringMap<String>> const& tags) -> std::expected<bool, StarException> {
      if (tags)
        line = line.replace_tags(*tags, false);

      if (!line.empty()) {
        auto result199 = add_chat_message(line);
if (!result199) return std::unexpected(std::move(result199.error()));

        return true;
      }

      return false;
    });
if (!result200) return std::unexpected(std::move(result200.error()));

  auto result202 = callbacks.register_callback("sayPortrait", [this](String line, String portrait, std::optional<StringMap<String>> const& tags) -> std::expected<bool, StarException> {
      if (tags)
        line = line.replace_tags(*tags, false);

      if (!line.empty()) {
        auto result201 = add_chat_message(line, portrait);
if (!result201) return std::unexpected(std::move(result201.error()));

        return true;
      }

      return false;
    });
if (!result202) return std::unexpected(std::move(result202.error()));

  auto result204 = callbacks.register_callback("setDamageTeam", [this](Json const& team) -> std::expected<void, StarException> {
      auto result203 = EntityDamageTeam::create(team);
if (!result203) return std::unexpected(std::move(result203.error()));
setTeam(std::move(*result203));
      return {};
});
if (!result204) return std::unexpected(std::move(result204.error()));

auto result205 = callbacks.register_callback(
  "setUniqueId", [this](std::optional<String> uniqueId) { setUniqueId(std::move(uniqueId)); });
if (!result205) return std::unexpected(std::move(result205.error()));

  auto result207 = callbacks.register_callback("setDamageBar", [this](String const& damageBarType) -> std::expected<void, StarException> {
      if (!DamageBarTypeNames) return std::unexpected(DamageBarTypeNames.error());
      auto result206 = DamageBarTypeNames->get_left(damageBarType);
      if (!result206)
        return std::unexpected(std::move(result206.error()));
      m_damage_bar.set(result206->get());
      return {};
});
if (!result207) return std::unexpected(std::move(result207.error()));

  auto result208 = callbacks.register_callback("setInteractive", [this](bool interactive) {
      m_interactive.set(interactive);
    });
if (!result208) return std::unexpected(std::move(result208.error()));

  auto result209 = callbacks.register_callback("setAnimationParameter", [this](String name, Json value) {
      m_scripted_animation_parameters.set(std::move(name), std::move(value));
    });
if (!result209) return std::unexpected(std::move(result209.error()));

  return callbacks;
}

std::expected<void, StarException> Monster::add_chat_message(String const& message, String const& portrait) {
  m_chat_message.set(message);
  m_chat_portrait.set(portrait);
  m_new_chat_message_event.trigger();
  if (portrait.empty())
    {
  auto result210 = mouth_position();
  if (!result210) return std::unexpected(std::move(result210.error()));
  m_pending_chat_actions.push_back(SayChatAction{entityId(), message, std::move(*result210)});
}
  else
    {
  auto result211 = mouth_position();
  if (!result211) return std::unexpected(std::move(result211.error()));
  m_pending_chat_actions.push_back(PortraitChatAction{entityId(), portrait, message, std::move(*result211)});
}
  return {};
}

std::expected<void, StarException> Monster::setupNetStates() {
  auto result212 = m_net_group.add_net_element(m_unique_id_net_state);
if (!result212) return std::unexpected(std::move(result212.error()));

  auto result213 = m_net_group.add_net_element(m_team_net_state);
if (!result213) return std::unexpected(std::move(result213.error()));

  auto result214 = m_net_group.add_net_element(m_monster_level_net_state);
if (!result214) return std::unexpected(std::move(result214.error()));

  auto result215 = m_net_group.add_net_element(m_damage_on_touch_net_state);
if (!result215) return std::unexpected(std::move(result215.error()));

  auto result216 = m_net_group.add_net_element(m_damage_sources);
if (!result216) return std::unexpected(std::move(result216.error()));

  auto result217 = m_net_group.add_net_element(m_aggressive_net_state);
if (!result217) return std::unexpected(std::move(result217.error()));

  auto result218 = m_net_group.add_net_element(m_knocked_out_net_state);
if (!result218) return std::unexpected(std::move(result218.error()));

  auto result219 = m_net_group.add_net_element(m_death_particle_burst_net_state);
if (!result219) return std::unexpected(std::move(result219.error()));

  auto result220 = m_net_group.add_net_element(m_death_sound_net_state);
if (!result220) return std::unexpected(std::move(result220.error()));

  auto result221 = m_net_group.add_net_element(m_active_skill_name_net_state);
if (!result221) return std::unexpected(std::move(result221.error()));

  auto result222 = m_net_group.add_net_element(m_name);
if (!result222) return std::unexpected(std::move(result222.error()));

  auto result223 = m_net_group.add_net_element(m_display_nametag);
if (!result223) return std::unexpected(std::move(result223.error()));

  auto result224 = m_net_group.add_net_element(m_drop_pool_net_state);
if (!result224) return std::unexpected(std::move(result224.error()));

  auto result225 = m_net_group.add_net_element(m_physics_forces);
if (!result225) return std::unexpected(std::move(result225.error()));


  auto result226 = m_net_group.add_net_element(*m_networked_animator);
if (!result226) return std::unexpected(std::move(result226.error()));

  auto result227 = m_net_group.add_net_element(*m_movement_controller);
if (!result227) return std::unexpected(std::move(result227.error()));

  auto result228 = m_net_group.add_net_element(*m_status_controller);
if (!result228) return std::unexpected(std::move(result228.error()));

  auto result229 = m_net_group.add_net_element(*m_effect_emitter);
if (!result229) return std::unexpected(std::move(result229.error()));


  auto result230 = m_net_group.add_net_element(m_new_chat_message_event);
if (!result230) return std::unexpected(std::move(result230.error()));

  auto result231 = m_net_group.add_net_element(m_chat_message);
if (!result231) return std::unexpected(std::move(result231.error()));

  auto result232 = m_net_group.add_net_element(m_chat_portrait);
if (!result232) return std::unexpected(std::move(result232.error()));


  auto result233 = m_net_group.add_net_element(m_damage_bar);
if (!result233) return std::unexpected(std::move(result233.error()));

  auto result234 = m_net_group.add_net_element(m_interactive);
if (!result234) return std::unexpected(std::move(result234.error()));


    // don't interpolate scripted animation parameters or animationdamageparts
  auto result235 = m_net_group.add_net_element(m_animation_damage_parts, false);
if (!result235) return std::unexpected(std::move(result235.error()));

  auto result236 = m_net_group.add_net_element(m_scripted_animation_parameters, false);
if (!result236) return std::unexpected(std::move(result236.error()));


  m_net_group.set_needs_load_callback([this](bool initial) { return getNetStates(initial); });
  m_net_group.set_needs_store_callback([this] { return setNetStates(); });
  return {};
}

std::expected<void, StarException> Monster::setNetStates() {
  m_unique_id_net_state.set(uniqueId());
  m_team_net_state.set(getTeam());
  m_monster_level_net_state.set(m_monster_level);
  m_damage_on_touch_net_state.set(m_damage_on_touch);
  m_aggressive_net_state.set(aggressive());
  m_knocked_out_net_state.set(m_knocked_out);
  m_death_particle_burst_net_state.set(m_death_particle_burst);
  m_death_sound_net_state.set(m_death_sound);
  m_active_skill_name_net_state.set(m_active_skill_name);
  m_drop_pool_net_state.set(m_drop_pool);
  return {};
}

std::expected<void, StarException> Monster::getNetStates(bool initial) {
  setUniqueId(m_unique_id_net_state.get());
  setTeam(m_team_net_state.get());
  m_monster_level = m_monster_level_net_state.get();
  m_damage_on_touch = m_damage_on_touch_net_state.get();
  m_aggressive = m_aggressive_net_state.get();
  m_knocked_out = m_knocked_out_net_state.get();
  if (m_death_particle_burst_net_state.pull_updated())
    m_death_particle_burst = m_death_particle_burst_net_state.get();
  if (m_death_sound_net_state.pull_updated())
    m_death_sound = m_death_sound_net_state.get();
  if (m_active_skill_name_net_state.pull_updated())
    m_active_skill_name = m_active_skill_name_net_state.get();
  if (m_drop_pool_net_state.pull_updated())
    m_drop_pool = m_drop_pool_net_state.get();

  if (m_new_chat_message_event.pull_occurred() && !initial) {
    if (m_chat_portrait.get().empty())
      {
  auto result237 = mouth_position();
  if (!result237) return std::unexpected(std::move(result237.error()));
  m_pending_chat_actions.push_back(SayChatAction{entityId(), m_chat_message.get(), std::move(*result237)});
}
    else
      {
  auto result238 = mouth_position();
  if (!result238) return std::unexpected(std::move(result238.error()));
  m_pending_chat_actions.push_back(
    PortraitChatAction{entityId(), m_chat_portrait.get(), m_chat_message.get(), std::move(*result238)});
}
  }
  return {};
}

std::expected<float, StarException> Monster::monster_level() const {
  if (!m_monster_level) return std::unexpected(MonsterException("Monster level is not initialized"));
  return *m_monster_level;
}

std::expected<Monster::SkillInfo, StarException> Monster::activeSkillInfo() const {
  if (m_active_skill_name.empty())
    return SkillInfo{};
  auto database = root().monster_database();
  if (!database)
    return std::unexpected(std::move(database.error()));
  auto skill = (*database)->skill_info(m_active_skill_name);
  if (!skill)
    return std::unexpected(std::move(skill.error()));
  return SkillInfo{std::move(skill->first), std::move(skill->second)};
}

std::expected<List<Drawable>, StarException> Monster::portrait(PortraitMode) const {
  if (m_monster_variant.portrait_icon) {
    auto result241 = this->root().image_metadata_database();
if (!result241) return std::unexpected(std::move(result241.error()));
auto result242 = Drawable::make_image(*std::move(*result241), *m_monster_variant.portrait_icon, 1.0f, true, Vec2F());
if (!result242) return std::unexpected(std::move(result242.error()));
return List<Drawable>{std::move(*result242)};
  } else {
    auto result243 = NetworkedAnimator::create(*m_networked_animator);
if (!result243) return std::unexpected(std::move(result243.error()));
auto animator = std::move(*result243);
    animator->setFlipped(!m_monster_variant.reversed);
    auto result244 = animator->drawables();
if (!result244) return std::unexpected(std::move(result244.error()));
auto drawables = std::move(*result244);
    Drawable::scale_all(drawables, tile_pixels);
    return drawables;
  }
}

String Monster::type_name() const {
  return m_monster_variant.type;
}

MonsterVariant Monster::monster_variant() const {
  return m_monster_variant;
}

std::optional<String> Monster::statusText() const {
  return {};
}

bool Monster::displayNametag() const {
  return m_display_nametag.get();
}

std::expected<Vec3B, StarException> Monster::nametagColor() const {
  return m_monster_variant.nametag_color;
}

std::expected<Vec2F, StarException> Monster::nametagOrigin() const {
  return mouth_position(false);
}

std::expected<String, StarException> Monster::nametag() const {
  return name();
}

bool Monster::aggressive() const {
  return m_aggressive;
}

std::expected<std::optional<LuaValue>, StarException> Monster::call_script(String const& func, LuaVariadic<LuaValue> const& args) {
  return recover_lua_script_error(m_script_component.invoke(func, args));
}

std::expected<std::optional<LuaValue>, StarException> Monster::eval_script(String const& code) {
  return recover_lua_script_error(m_script_component.eval(code));
}

std::expected<Vec2F, StarException> Monster::mouth_position() const {
  return mouth_offset() + position();
}

std::expected<Vec2F, StarException> Monster::mouth_position(bool) const {
  return mouth_position();
}

List<ChatAction> Monster::pull_pending_chat_actions() {
  return std::move(m_pending_chat_actions);
}

std::expected<List<PhysicsForceRegion>, StarException> Monster::force_regions() const {
  return m_physics_forces.get();
}

std::expected<InteractAction, StarException> Monster::interact(InteractRequest const& request) {
  auto result = recover_lua_script_error(m_script_component.invoke<Json>("interact",
      JsonObject{{"sourceId", request.source_id}, {"sourcePosition", json_from_vec2f(request.source_position)}}));
  if (!result)
    return std::unexpected(std::move(result.error()));
  if (!*result || (*result)->is_null())
    return {};
  return interact_action_from_script(**result, entityId());
}

bool Monster::isInteractive() const {
  return m_interactive.get();
}

std::expected<Vec2F, StarException> Monster::questIndicatorPosition() const {
  Vec2F pos = position() + m_quest_indicator_offset;
  pos[1] += collision_area().y_max();
  return pos;
}

ActorMovementController& Monster::movement_controller() {
  return *m_movement_controller;
}

StatusController& Monster::statusController() {
  return *m_status_controller;
}

}

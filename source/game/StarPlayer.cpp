#include <assert.h>
import star.json;
import star.xx_hash;
import star.json_extra;
import star.data_stream_extra;
import star.id_map;
import star.ordered_map;
import star.lru_cache;
import star.thread;
import star.time;
import star.random;
import star.ttl_cache;
import star.casting;
import star.poly;
import star.list;
import star.bi_map;
import star.data_stream;
import star.game_types;
import star.message_types;
import star.json_rpc;
import star.strong_typedef;
import star.vector;
import star.string;
import star.byte_array;
import star.encode;
import star.rect;
import star.image_processing;
import star.hash;
import star.string_view;
import star.directives;
import star.color;
import star.asset_path;
import star.rpc_promise;
import star.set;
import star.optional;
import star.a_star;
import star.ordered_set;
import star.audio;
import star.map;
import star.either;
import star.interpolation;
import star.periodic_function;
import star.matrix3;
import star.io_device;
import star.ref_ptr;
import star.logging;
import star.listener;
import star.format;
import star.output_proxy;
import star.spline;
import star.lua;
import star.weighted_pool;
import star.data_stream_devices;

import std;
using ZSTD_CCtx = struct ZSTD_CCtx_s;
using ZSTD_DCtx = struct ZSTD_DCtx_s;
using ZSTD_DStream = ZSTD_DCtx;
using ZSTD_CStream = ZSTD_CCtx;
import star.zstd_compression;
import star.net_compatibility;
import star.math_common;
import star.worker_pool;

import star.sector_array_2d;
import star.perlin;
import star.btree;
import star.btree_database;
import star.image;

import star.utility_lua_bindings;
import star.lua_root;
import star.lua_actor_movement_component;
import star.lua_components;
import star.lua_animation_component;
import star.collision_block;
import star.item_descriptor;
import star.drawable;
import star.item;
import star.item_recipe;
import star.player_deployment;
import star.celestial_coordinate;
import star.quest_descriptor;
import star.uuid;
import star.warping;
import star.quests;
import star.quest_manager;
import star.animated_part_set;
import star.mixer;
import star.networked_animator;
import star.humanoid;
import star.physics_entity;
import star.anchorable_entity;
import star.mobile_entity;
import star.actor_entity;
import star.tool_user_entity;
import star.lounging_entities;
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
import star.entity_rendering_types;
import star.entity_rendering;
import star.player_types;
import star.player;
import star.asset_source;
import star.assets;
import star.root_base;
import star.configuration;
import star.root;
import star.non_rotated_drawables_item;
import star.tool_user_item;
import star.beam_item;
import star.status_effect_item;
import star.fireable_item;
import star.swingable_item;
import star.durability_item;
import star.pointable_item;
struct PreviewTile;
import star.preview_tile_tool;
import star.tools;
import star.scripted_entity;
import star.item_drop;
import star.effect_source_item;
import star.previewable_item;
import star.armors;

import star.tech_database;
import star.animation;
import star.particle;
import star.damage_types;
import star.world_geometry;
import star.status_types;
import star.damage;
import star.light_source;
import star.entity;
import star.tile_damage;
import star.interaction_types;
import star.interactive_entity;
import star.tile_entity;
import star.tile_modification;
import star.force_regions;
import star.world;
import star.stat_collection;
import star.status_effect_database;
import star.status_controller;
import star.player_blueprints;
import star.player_universe_map;
import star.player_codexes;
import star.player_tech;
import star.player_log;
import star.host_address;
import star.sky_types;
import star.weather_types;
import star.world_parameters;
import star.celestial_parameters;
import star.chat_types;
import star.socket;
import star.tcp;
import star.p2p_networking_service;
import star.liquid_types;
import star.tile_sector_array;
import star.world_layout;
import star.collision_generator;
import star.world_tiles;
import star.celestial_types;
import star.wiring;
import star.plant;
import star.plant_database;
import star.biome_placement;
import star.versioning_database;
import star.world_storage;
import star.sky_parameters;
import star.system_world;
import star.damage_manager;
import star.net_packets;
import star.net_packet_socket;
import star.universe_connection;
import star.sky_render_data;
import star.parallax;
import star.cellular_light_array;
import star.cellular_lighting;
import star.world_render_data;
import star.world_structure;
import star.world_client_state;
import star.interpolation_tracker;
import star.ambient;
import star.weather;
import star.world_client;
import star.world_client_thread;
import star.universe;
// TODO: make this more thread safe
import star.universe_client;

import star.species_database;
import star.ai_database;
import star.player_lua_bindings;
import star.entity_lua_bindings;
import star.networked_animator_lua_bindings;
import star.inspection_tool;
import star.songbook_lua_bindings;
import star.scripted_animator_lua_bindings;
import star.celestial_lua_bindings;
import star.status_controller_lua_bindings;
import star.dance_database;
import star.collection_database;
import star.emote_processor;
import star.radio_message_database;
import star.material_database;
import star.item_database;


import star.player_inventory;
import star.client_context;
import star.item_bag;
import star.player_companions;
import star.statistics;
import star.player_factory;

import star.effect_emitter;

import star.songbook;

import star.armor_wearer;
import star.tool_user;
import star.tech_controller;
import star.entity_splash;

import star.algorithm;
namespace Star {

namespace {

std::expected<void, StarException> apply_lounge_cosmetic_overrides(
    Root& root, JsonObject const& overrides, ArmorWearer& armor) {
  std::shared_ptr<ItemDatabase const> item_database;
  auto apply = [&]<std::derived_from<ArmorItem> Armor>(
      std::string_view key, EquipmentSlot slot) -> std::expected<void, StarException> {
    auto entry = overrides.find(key);
    if (entry == overrides.end())
      return {};
    if (!item_database) {
      auto acquired = root.itemDatabase();
      if (!acquired)
        return std::unexpected(std::move(acquired.error()));
      item_database = std::move(*acquired);
    }
    auto descriptor = ItemDescriptor::create(entry->second);
    if (!descriptor)
      return std::unexpected(std::move(descriptor.error()));
    auto item = item_database->item(std::move(*descriptor));
    if (!item)
      return std::unexpected(std::move(item.error()));
    if (PlayerInventory::item_allowed_as_equipment(*item, slot))
      armor.set_item(static_cast<std::uint8_t>(slot), as<Armor>(std::move(*item)));
    return {};
  };

  if (auto result = apply.operator()<HeadArmor>("head", EquipmentSlot::HeadCosmetic); !result)
    return result;
  if (auto result = apply.operator()<ChestArmor>("chest", EquipmentSlot::ChestCosmetic); !result)
    return result;
  if (auto result = apply.operator()<LegsArmor>("legs", EquipmentSlot::LegsCosmetic); !result)
    return result;
  return apply.operator()<BackArmor>("back", EquipmentSlot::BackCosmetic);
}

}

std::expected<EnumMap<Player::State>, StarException> const Player::state_names = EnumMap<State>::create({
  {Player::State::Idle, "idle"},
  {Player::State::Walk, "walk"},
  {Player::State::Run, "run"},
  {Player::State::Jump, "jump"},
  {Player::State::Fall, "fall"},
  {Player::State::Swim, "swim"},
  {Player::State::SwimIdle, "swimIdle"},
  {Player::State::TeleportIn, "teleportIn"},
  {Player::State::TeleportOut, "teleportOut"},
  {Player::State::Crouch, "crouch"},
  {Player::State::Lounge, "lounge"}
});

std::expected<std::shared_ptr<Player>, StarException> Player::create(Root& root, std::shared_ptr<PlayerConfig> config) {
  return create(root, std::move(config), Uuid(root.random()));
}

Player::Player(Root& root_owner) : Entity(root_owner), m_net_humanoid([&root_owner] { return NetHumanoid::create(root_owner); }), m_scripted_animator(root_owner) {}

Player::~Player() {
  if (m_inventory)
    m_inventory->setPlayer(std::nullopt);
  if (m_quest_manager)
    m_quest_manager->detachPlayer();
}

std::expected<std::shared_ptr<Player>, StarException> Player::create(Root& root, std::shared_ptr<PlayerConfig> config, Uuid uuid) {
  auto player = std::shared_ptr<Player>(new Player(root));
  auto initialized = player->initialize(std::move(config), std::move(uuid));
  if (!initialized)
    return std::unexpected(std::move(initialized.error()));
  return player;
}

std::expected<void, StarException> Player::initialize(std::shared_ptr<PlayerConfig> config, Uuid uuid) {
  auto assets_result = root().assets();
  if (!assets_result)
    return std::unexpected(std::move(assets_result.error()));
  auto assets = std::move(*assets_result);

  m_config = std::move(config);
  m_client.reset();

  m_state = State::Idle;
  m_emote_state = HumanoidEmote::Idle;

  m_footstep_timer = 0.0f;
  m_teleport_timer = 0.0f;
  m_teleport_animation_type = "default";

  m_shifting = false;

  m_aim_position = Vec2F();

  set_unique_id(uuid.hex());
  m_identity = m_config->default_identity;
  m_identity_updated = true;

  auto quest_manager_result = QuestManager::create(root(), *this);
  if (!quest_manager_result)
    return std::unexpected(std::move(quest_manager_result.error()));
  auto quest_manager = std::move(*quest_manager_result);
  m_quest_manager = std::move(quest_manager);
  auto tools_result = ToolUser::create(root());
  if (!tools_result)
    return std::unexpected(std::move(tools_result.error()));
  auto tools = std::move(*tools_result);
  m_tools = std::move(tools);
  auto armor_result = ArmorWearer::create(root());
  if (!armor_result)
    return std::unexpected(std::move(armor_result.error()));
  auto armor = std::move(*armor_result);
  m_armor = std::move(armor);
  m_companions = std::make_shared<PlayerCompanions>(root(), m_config->companions_config);

  for (auto&& p : m_config->generic_script_contexts) {
    auto script_component = std::make_shared<GenericScriptComponent>(root());
    if (auto result = script_component->set_script(p.second); !result)
      return std::unexpected(std::move(result.error()));
    m_generic_script_contexts.set(p.first, script_component);
  }

  // all of these are defaults and won't include the correct humanoid config for the species
  if (auto result = install_humanoid(); !result)
    return std::unexpected(std::move(result.error()));
  auto movement_parameters_result = ActorMovementParameters::create(json_merge(humanoid()->default_movement_parameters(), humanoid()->player_movement_parameters().value_or(m_config->movement_parameters)));
  if (!movement_parameters_result)
    return std::unexpected(std::move(movement_parameters_result.error()));
  auto movement_parameters = std::move(*movement_parameters_result);
  if (!movement_parameters.physics_effect_categories)
    movement_parameters.physics_effect_categories = StringSet({"player"});
  auto movement_controller_result = ActorMovementController::create(root(), movement_parameters);
  if (!movement_controller_result)
    return std::unexpected(std::move(movement_controller_result.error()));
  auto movement_controller = std::move(*movement_controller_result);
  m_movement_controller = std::move(movement_controller);
  auto zero_g_movement_parameters_result = ActorMovementParameters::create(m_config->zero_g_movement_parameters);
  if (!zero_g_movement_parameters_result)
    return std::unexpected(std::move(zero_g_movement_parameters_result.error()));
  auto zero_g_movement_parameters = std::move(*zero_g_movement_parameters_result);
  m_zero_g_movement_parameters = std::move(zero_g_movement_parameters);

  auto tech_controller_result = TechController::create(root());
  if (!tech_controller_result)
    return std::unexpected(std::move(tech_controller_result.error()));
  auto tech_controller = std::move(*tech_controller_result);
  m_tech_controller = std::move(tech_controller);
  auto status_controller_result = StatusController::create(root(), m_config->status_controller_settings);
  if (!status_controller_result)
    return std::unexpected(std::move(status_controller_result.error()));
  auto status_controller = std::move(*status_controller_result);
  m_status_controller = std::move(status_controller);
  m_deployment = std::make_shared<PlayerDeployment>(root(), m_config->deployment_config);

  auto inventory_result = PlayerInventory::create(root());
  if (!inventory_result)
    return std::unexpected(std::move(inventory_result.error()));
  auto inventory = std::move(*inventory_result);
  m_inventory = std::move(inventory);
  m_inventory->setPlayer(std::ref(*this));

  m_blueprints = std::make_shared<PlayerBlueprints>();
  m_universe_map = std::make_shared<PlayerUniverseMap>();
  m_codexes = std::make_shared<PlayerCodexes>(root());
  m_techs = std::make_shared<PlayerTech>(root());
  m_log = std::make_shared<PlayerLog>();

  if (auto result = set_mode_type(PlayerMode::Casual); !result)
    return std::unexpected(std::move(result.error()));

  setTeam(EntityDamageTeam(TeamType::Friendly));

  auto footstep_volume_variance_json_result = assets->json("/sfx.config:footstepVolumeVariance");
  if (!footstep_volume_variance_json_result)
    return std::unexpected(std::move(footstep_volume_variance_json_result.error()));
  auto footstep_volume_variance_json = std::move(*footstep_volume_variance_json_result);
  auto footstep_volume_variance_result = footstep_volume_variance_json.to_float();
  if (!footstep_volume_variance_result)
    return std::unexpected(std::move(footstep_volume_variance_result.error()));
  auto footstep_volume_variance = std::move(*footstep_volume_variance_result);
  m_footstep_volume_variance = footstep_volume_variance;
  auto landing_volume_json_result = assets->json("/sfx.config:landingVolume");
  if (!landing_volume_json_result)
    return std::unexpected(std::move(landing_volume_json_result.error()));
  auto landing_volume_json = std::move(*landing_volume_json_result);
  auto landing_volume_result = landing_volume_json.to_float();
  if (!landing_volume_result)
    return std::unexpected(std::move(landing_volume_result.error()));
  auto landing_volume = std::move(*landing_volume_result);
  m_landing_volume = landing_volume;

  auto effects_config_result = assets->fetch_json(m_config->effects_animator);
  if (!effects_config_result)
    return std::unexpected(std::move(effects_config_result.error()));
  auto effects_config = std::move(*effects_config_result);
  auto effects_animator_result = NetworkedAnimator::create(root(), effects_config);
  if (!effects_animator_result)
    return std::unexpected(std::move(effects_animator_result.error()));
  auto effects_animator = std::move(*effects_animator_result);
  m_effects_animator = std::move(effects_animator);
  auto emitter_result = EffectEmitter::create(root());
  if (!emitter_result)
    return std::unexpected(std::move(emitter_result.error()));
  auto emitter = std::move(*emitter_result);
  m_effect_emitter = std::move(emitter);

  auto interact_radius_json_result = assets->json("/player.config:interactRadius");
  if (!interact_radius_json_result)
    return std::unexpected(std::move(interact_radius_json_result.error()));
  auto interact_radius_json = std::move(*interact_radius_json_result);
  auto interact_radius_result = interact_radius_json.to_float();
  if (!interact_radius_result)
    return std::unexpected(std::move(interact_radius_result.error()));
  auto interact_radius = std::move(*interact_radius_result);
  m_interact_radius = interact_radius;

  auto walk_into_interact_bias_json_result = assets->json("/player.config:walkIntoInteractBias");
  if (!walk_into_interact_bias_json_result)
    return std::unexpected(std::move(walk_into_interact_bias_json_result.error()));
  auto walk_into_interact_bias_json = std::move(*walk_into_interact_bias_json_result);
  auto walk_into_interact_bias_result = json_to_vec2f(walk_into_interact_bias_json);
  if (!walk_into_interact_bias_result)
    return std::unexpected(std::move(walk_into_interact_bias_result.error()));
  auto walk_into_interact_bias = std::move(*walk_into_interact_bias_result);
  m_walk_into_interact_bias = walk_into_interact_bias;

  if (m_landing_volume <= 1)
    m_landing_volume = 6;

  m_is_admin = false;

  auto emote_cooldown_json_result = assets->json("/player.config:emoteCooldown");
  if (!emote_cooldown_json_result)
    return std::unexpected(std::move(emote_cooldown_json_result.error()));
  auto emote_cooldown_json = std::move(*emote_cooldown_json_result);
  auto emote_cooldown_result = emote_cooldown_json.to_float();
  if (!emote_cooldown_result)
    return std::unexpected(std::move(emote_cooldown_result.error()));
  auto emote_cooldown = std::move(*emote_cooldown_result);
  m_emote_cooldown = emote_cooldown;
  auto blink_interval_json_result = assets->json("/player.config:blinkInterval");
  if (!blink_interval_json_result)
    return std::unexpected(std::move(blink_interval_json_result.error()));
  auto blink_interval_json = std::move(*blink_interval_json_result);
  auto blink_interval_result = json_to_vec2f(blink_interval_json);
  if (!blink_interval_result)
    return std::unexpected(std::move(blink_interval_result.error()));
  auto blink_interval = std::move(*blink_interval_result);
  m_blink_interval = blink_interval;

  m_emote_cooldown_timer = GameTimer(m_emote_cooldown);
  m_blink_cooldown_timer = GameTimer(0);

  m_chat_message_changed = false;
  m_chat_message_updated = false;

  m_interrupt_radio_message = false;

  auto songbook_result = Songbook::create(root(), species());
  if (!songbook_result)
    return std::unexpected(std::move(songbook_result.error()));
  auto songbook = std::move(*songbook_result);
  m_songbook = std::move(songbook);

  m_last_damaged_other_timer = 0;
  m_last_damaged_target = null_entity_id;

  auto age_items_every_json_result = assets->json("/player.config:ageItemsEvery");
  if (!age_items_every_json_result)
    return std::unexpected(std::move(age_items_every_json_result.error()));
  auto age_items_every_json = std::move(*age_items_every_json_result);
  auto age_items_every_result = age_items_every_json.to_float();
  if (!age_items_every_result)
    return std::unexpected(std::move(age_items_every_result.error()));
  auto age_items_every = std::move(*age_items_every_result);
  m_age_items_timer = GameTimer(age_items_every);

  if (auto result = refresh_equipment(); !result)
    return std::unexpected(std::move(result.error()));

  auto food_low_threshold_json_result = assets->json("/player.config:foodLowThreshold");
  if (!food_low_threshold_json_result)
    return std::unexpected(std::move(food_low_threshold_json_result.error()));
  auto food_low_threshold_json = std::move(*food_low_threshold_json_result);
  auto food_low_threshold_result = food_low_threshold_json.to_float();
  if (!food_low_threshold_result)
    return std::unexpected(std::move(food_low_threshold_result.error()));
  auto food_low_threshold = std::move(*food_low_threshold_result);
  m_food_low_threshold = food_low_threshold;
  auto food_low_status_effects_json_result = assets->json("/player.config:foodLowStatusEffects");
  if (!food_low_status_effects_json_result)
    return std::unexpected(std::move(food_low_status_effects_json_result.error()));
  auto food_low_status_effects_json = std::move(*food_low_status_effects_json_result);
  auto food_low_status_effects_array_result = food_low_status_effects_json.to_array();
  if (!food_low_status_effects_array_result)
    return std::unexpected(std::move(food_low_status_effects_array_result.error()));
  auto food_low_status_effects_array = std::move(*food_low_status_effects_array_result);
  for (auto const& entry : food_low_status_effects_array) {
    auto effect_result = jsonToPersistentStatusEffect(entry);
    if (!effect_result)
      return std::unexpected(std::move(effect_result.error()));
    auto effect = std::move(*effect_result);
    m_food_low_status_effects.push_back(std::move(effect));
  }
  auto food_empty_status_effects_json_result = assets->json("/player.config:foodEmptyStatusEffects");
  if (!food_empty_status_effects_json_result)
    return std::unexpected(std::move(food_empty_status_effects_json_result.error()));
  auto food_empty_status_effects_json = std::move(*food_empty_status_effects_json_result);
  auto food_empty_status_effects_array_result = food_empty_status_effects_json.to_array();
  if (!food_empty_status_effects_array_result)
    return std::unexpected(std::move(food_empty_status_effects_array_result.error()));
  auto food_empty_status_effects_array = std::move(*food_empty_status_effects_array_result);
  for (auto const& entry : food_empty_status_effects_array) {
    auto effect_result = jsonToPersistentStatusEffect(entry);
    if (!effect_result)
      return std::unexpected(std::move(effect_result.error()));
    auto effect = std::move(*effect_result);
    m_food_empty_status_effects.push_back(std::move(effect));
  }

  auto in_cinematic_status_effects_json_result = assets->json("/player.config:inCinematicStatusEffects");
  if (!in_cinematic_status_effects_json_result)
    return std::unexpected(std::move(in_cinematic_status_effects_json_result.error()));
  auto in_cinematic_status_effects_json = std::move(*in_cinematic_status_effects_json_result);
  auto in_cinematic_status_effects_array_result = in_cinematic_status_effects_json.to_array();
  if (!in_cinematic_status_effects_array_result)
    return std::unexpected(std::move(in_cinematic_status_effects_array_result.error()));
  auto in_cinematic_status_effects_array = std::move(*in_cinematic_status_effects_array_result);
  for (auto const& entry : in_cinematic_status_effects_array) {
    auto effect_result = jsonToPersistentStatusEffect(entry);
    if (!effect_result)
      return std::unexpected(std::move(effect_result.error()));
    auto effect = std::move(*effect_result);
    m_in_cinematic_status_effects.push_back(std::move(effect));
  }

  if (auto result = synchronize_equipment_status_effects(*m_armor, *m_tools, *m_status_controller); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = m_status_controller->resetAllResources(); !result)
    return std::unexpected(std::move(result.error()));

  m_landing_noise_pending = false;
  m_footstep_pending = false;

  setKeepAlive(true);

  {
    if (auto result = m_net_group.add_net_element(m_state_net_state); !result)
      return std::unexpected(std::move(result.error()));
  }
  {
    if (auto result = m_net_group.add_net_element(m_shifting_net_state); !result)
      return std::unexpected(std::move(result.error()));
  }
  {
    if (auto result = m_net_group.add_net_element(m_x_aim_position_net_state); !result)
      return std::unexpected(std::move(result.error()));
  }
  {
    if (auto result = m_net_group.add_net_element(m_y_aim_position_net_state); !result)
      return std::unexpected(std::move(result.error()));
  }
  {
    if (auto result = m_net_group.add_net_element(m_identity_net_state); !result)
      return std::unexpected(std::move(result.error()));
  }
  {
    if (auto result = m_net_group.add_net_element(m_team_net_state); !result)
      return std::unexpected(std::move(result.error()));
  }
  {
    if (auto result = m_net_group.add_net_element(m_landed_net_state); !result)
      return std::unexpected(std::move(result.error()));
  }
  {
    if (auto result = m_net_group.add_net_element(m_chat_message_net_state); !result)
      return std::unexpected(std::move(result.error()));
  }
  {
    if (auto result = m_net_group.add_net_element(m_new_chat_message_net_state); !result)
      return std::unexpected(std::move(result.error()));
  }
  {
    if (auto result = m_net_group.add_net_element(m_emote_net_state); !result)
      return std::unexpected(std::move(result.error()));
  }

  m_x_aim_position_net_state.set_fixed_point_base(0.003125);
  m_y_aim_position_net_state.set_fixed_point_base(0.003125);
  m_y_aim_position_net_state.set_interpolator(lerp<float, float>);

  {
    if (auto result = m_net_group.add_net_element(*m_inventory); !result)
      return std::unexpected(std::move(result.error()));
  }
  {
    if (auto result = m_net_group.add_net_element(*m_tools); !result)
      return std::unexpected(std::move(result.error()));
  }
  {
    if (auto result = m_net_group.add_net_element(*m_armor); !result)
      return std::unexpected(std::move(result.error()));
  }
  {
    if (auto result = m_net_group.add_net_element(*m_songbook); !result)
      return std::unexpected(std::move(result.error()));
  }
  {
    if (auto result = m_net_group.add_net_element(*m_movement_controller); !result)
      return std::unexpected(std::move(result.error()));
  }
  {
    if (auto result = m_net_group.add_net_element(*m_effect_emitter); !result)
      return std::unexpected(std::move(result.error()));
  }
  {
    if (auto result = m_net_group.add_net_element(*m_effects_animator); !result)
      return std::unexpected(std::move(result.error()));
  }
  {
    if (auto result = m_net_group.add_net_element(*m_status_controller); !result)
      return std::unexpected(std::move(result.error()));
  }
  {
    if (auto result = m_net_group.add_net_element(*m_tech_controller); !result)
      return std::unexpected(std::move(result.error()));
  }

  m_net_humanoid.set_compatibility_version(10);
  {
    if (auto result = m_net_group.add_net_element(m_net_humanoid); !result)
      return std::unexpected(std::move(result.error()));
  }
  m_refreshed_humanoid_parameters.set_compatibility_version(10);
  {
    if (auto result = m_net_group.add_net_element(m_refreshed_humanoid_parameters); !result)
      return std::unexpected(std::move(result.error()));
  }

  m_scripted_animation_parameters.set_compatibility_version(10);
  {
    if (auto result = m_net_group.add_net_element(m_scripted_animation_parameters); !result)
      return std::unexpected(std::move(result.error()));
  }

  m_death_particle_burst.set_compatibility_version(10);
  {
    if (auto result = m_net_group.add_net_element(m_death_particle_burst); !result)
      return std::unexpected(std::move(result.error()));
  }

  m_humanoid_dance_net_state.set_compatibility_version(13);
  {
    if (auto result = m_net_group.add_net_element(m_humanoid_dance_net_state); !result)
      return std::unexpected(std::move(result.error()));
  }

  m_net_group.set_needs_load_callback([this](bool initial) { return get_net_states(initial); });
  m_net_group.set_needs_store_callback([this]() { return set_net_states(); });
  return {};
}

std::expected<std::shared_ptr<Player>, StarException> Player::create(Root& root_owner, std::shared_ptr<PlayerConfig> config, ByteArray const& net_store, NetCompatibilityRules rules) {
  auto player = create(root_owner, std::move(config));
  if (!player)
    return std::unexpected(std::move(player.error()));
  auto loaded = (*player)->net_load(net_store, rules);
  if (!loaded)
    return std::unexpected(std::move(loaded.error()));
  return player;
}

std::expected<void, StarException> Player::net_load(ByteArray const& net_store, NetCompatibilityRules rules) {
  DataStreamReader ds(net_store.span());
  ds.set_stream_compatibility_version(rules);
  auto id_result = ds.read<String>();
  if (!id_result)
    return std::unexpected(std::move(id_result.error()));
  auto id = std::move(*id_result);
  set_unique_id(std::move(id));
  if (auto result = ds.read(m_description); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = ds.read(m_mode_type); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = ds.read(m_identity); !result)
    return std::unexpected(std::move(result.error()));
  if (rules.version() >= 10) {
    if (auto result = ds.read(m_humanoid_parameters); !result)
      return std::unexpected(std::move(result.error()));
  }
  if (auto result = install_humanoid(); !result)
    return std::unexpected(std::move(result.error()));
  auto movement_parameters_result = ActorMovementParameters::create(json_merge(humanoid()->default_movement_parameters(), humanoid()->player_movement_parameters().value_or(m_config->movement_parameters)));
  if (!movement_parameters_result)
    return std::unexpected(std::move(movement_parameters_result.error()));
  auto movement_parameters = std::move(*movement_parameters_result);
  if (auto result = m_movement_controller->reset_base_parameters(movement_parameters); !result)
    return std::unexpected(std::move(result.error()));
  m_death_particle_burst.set(humanoid()->default_death_particles());
  return {};
}

std::expected<void, StarException> Player::install_humanoid() {
  auto current_result = NetHumanoid::create(root(), m_identity, m_humanoid_parameters, Json());
  if (!current_result)
    return std::unexpected(std::move(current_result.error()));
  auto current = std::move(*current_result);
  m_net_humanoid.clear_net_elements();
  if (auto result = m_net_humanoid.add_net_element(current); !result)
    return std::unexpected(std::move(result.error()));
  m_current_net_humanoid = std::move(current);
  return {};
}


std::expected<std::shared_ptr<Player>, StarException> Player::create(Root& root_owner, std::shared_ptr<PlayerConfig> config, Json const& disk_store) {
  auto player = create(root_owner, std::move(config));
  if (!player)
    return std::unexpected(std::move(player.error()));
  auto loaded = (*player)->disk_load(disk_store);
  if (!loaded)
    return std::unexpected(std::move(loaded.error()));
  return player;
}

std::expected<void, StarException> Player::disk_load(Json const& disk_store) {
  auto id_result = disk_store.get_string("uuid");
  if (!id_result)
    return std::unexpected(std::move(id_result.error()));
  auto id = std::move(*id_result);
  set_unique_id(std::move(id));
  auto description_result = disk_store.get_string("description");
  if (!description_result)
    return std::unexpected(std::move(description_result.error()));
  auto description = std::move(*description_result);
  m_description = std::move(description);
  auto mode_name_result = disk_store.get_string("modeType");
  if (!mode_name_result)
    return std::unexpected(std::move(mode_name_result.error()));
  auto mode_name = std::move(*mode_name_result);
  if (!PlayerModeNames)
    return std::unexpected(PlayerModeNames.error());
  auto mode_result = PlayerModeNames->get_left(mode_name);
  if (!mode_result)
    return std::unexpected(std::move(mode_result.error()));
  auto const& mode = mode_result->get();
  if (auto result = set_mode_type(mode); !result)
    return std::unexpected(std::move(result.error()));
  auto ship_result = disk_store.get("shipUpgrades");
  if (!ship_result)
    return std::unexpected(std::move(ship_result.error()));
  auto ship = std::move(*ship_result);
  auto ship_upgrades_result = ShipUpgrades::create(ship);
  if (!ship_upgrades_result)
    return std::unexpected(std::move(ship_upgrades_result.error()));
  auto ship_upgrades = std::move(*ship_upgrades_result);
  m_ship_upgrades = std::move(ship_upgrades);
  auto blueprints_json_result = disk_store.get("blueprints");
  if (!blueprints_json_result)
    return std::unexpected(std::move(blueprints_json_result.error()));
  auto blueprints_json = std::move(*blueprints_json_result);
  auto blueprints_result = PlayerBlueprints::create(blueprints_json);
  if (!blueprints_result)
    return std::unexpected(std::move(blueprints_result.error()));
  auto blueprints = std::move(*blueprints_result);
  m_blueprints = std::make_shared<PlayerBlueprints>(std::move(blueprints));
  auto universe_map_json_result = disk_store.get("universeMap");
  if (!universe_map_json_result)
    return std::unexpected(std::move(universe_map_json_result.error()));
  auto universe_map_json = std::move(*universe_map_json_result);
  auto universe_map_result = PlayerUniverseMap::create(universe_map_json);
  if (!universe_map_result)
    return std::unexpected(std::move(universe_map_result.error()));
  auto universe_map = std::move(*universe_map_result);
  m_universe_map = std::make_shared<PlayerUniverseMap>(std::move(universe_map));
  if (m_client_context)
    m_universe_map->setServerUuid(m_client_context->server_uuid());
  auto codexes_json = disk_store.get("codexes");
  if (!codexes_json)
    return std::unexpected(std::move(codexes_json.error()));
  auto codexes = PlayerCodexes::create(root(), *codexes_json);
  if (!codexes)
    return std::unexpected(std::move(codexes.error()));
  m_codexes = std::move(*codexes);
  auto techs_json_result = disk_store.get("techs");
  if (!techs_json_result)
    return std::unexpected(std::move(techs_json_result.error()));
  auto techs_json = std::move(*techs_json_result);
  auto techs_result = PlayerTech::create(root(), techs_json);
  if (!techs_result)
    return std::unexpected(std::move(techs_result.error()));
  auto techs = std::move(*techs_result);
  m_techs = std::move(techs);
  auto identity_json_result = disk_store.get("identity");
  if (!identity_json_result)
    return std::unexpected(std::move(identity_json_result.error()));
  auto identity_json = std::move(*identity_json_result);
  auto identity_result = HumanoidIdentity::create(identity_json);
  if (!identity_result)
    return std::unexpected(std::move(identity_result.error()));
  auto identity = std::move(*identity_result);
  m_identity = std::move(identity);
  m_identity_updated = true;
  auto team_json_result = disk_store.get("team");
  if (!team_json_result)
    return std::unexpected(std::move(team_json_result.error()));
  auto team_json = std::move(*team_json_result);
  auto team_result = EntityDamageTeam::create(team_json);
  if (!team_result)
    return std::unexpected(std::move(team_result.error()));
  auto team = std::move(*team_result);
  setTeam(std::move(team));
  m_state = State::Idle;
  auto inventory_result = disk_store.get("inventory");
  if (!inventory_result)
    return std::unexpected(std::move(inventory_result.error()));
  auto inventory = std::move(*inventory_result);
  if (auto result = m_inventory->load(inventory); !result)
    return std::unexpected(std::move(result.error()));
  auto movement_result = disk_store.get("movementController");
  if (!movement_result)
    return std::unexpected(std::move(movement_result.error()));
  auto movement = std::move(*movement_result);
  if (auto result = m_movement_controller->load_state(movement); !result)
    return std::unexpected(std::move(result.error()));
  auto tech_controller_result = disk_store.get("techController");
  if (!tech_controller_result)
    return std::unexpected(std::move(tech_controller_result.error()));
  auto tech_controller = std::move(*tech_controller_result);
  if (auto result = m_tech_controller->disk_load(tech_controller); !result)
    return std::unexpected(std::move(result.error()));
  auto status_controller_result = disk_store.get("statusController");
  if (!status_controller_result)
    return std::unexpected(std::move(status_controller_result.error()));
  auto status_controller = std::move(*status_controller_result);
  if (auto result = m_status_controller->diskLoad(status_controller); !result)
    return std::unexpected(std::move(result.error()));
  auto log_json_result = disk_store.get("log");
  if (!log_json_result)
    return std::unexpected(std::move(log_json_result.error()));
  auto log_json = std::move(*log_json_result);
  auto log_result = PlayerLog::create(log_json);
  if (!log_result)
    return std::unexpected(std::move(log_result.error()));
  auto log = std::move(*log_result);
  m_log = std::make_shared<PlayerLog>(std::move(log));
  auto species_database_result = root().speciesDatabase();
  if (!species_database_result)
    return std::unexpected(std::move(species_database_result.error()));
  auto species_database = std::move(*species_database_result);
  auto species_def_result = species_database->species(m_identity.species);
  if (!species_def_result)
    return std::unexpected(std::move(species_def_result.error()));
  auto species_def = std::move(*species_def_result);
  auto ship_species_result = disk_store.get_string("shipSpecies", m_identity.species);
  if (!ship_species_result)
    return std::unexpected(std::move(ship_species_result.error()));
  auto ship_species = std::move(*ship_species_result);
  m_ship_species = std::move(ship_species);
  auto quests_result = disk_store.get("quests", JsonObject{});
  if (!quests_result)
    return std::unexpected(std::move(quests_result.error()));
  auto quests = std::move(*quests_result);
  if (auto result = m_quest_manager->diskLoad(quests); !result)
    return std::unexpected(std::move(result.error()));
  auto companions_result = disk_store.get("companions", JsonObject{});
  if (!companions_result)
    return std::unexpected(std::move(companions_result.error()));
  auto companions = std::move(*companions_result);
  if (auto result = m_companions->diskLoad(companions); !result)
    return std::unexpected(std::move(result.error()));
  auto deployment_result = disk_store.get("deployment", JsonObject{});
  if (!deployment_result)
    return std::unexpected(std::move(deployment_result.error()));
  auto deployment = std::move(*deployment_result);
  if (auto result = m_deployment->diskLoad(deployment); !result)
    return std::unexpected(std::move(result.error()));
  auto parameters_result = disk_store.get_object("humanoidParameters", JsonObject());
  if (!parameters_result)
    return std::unexpected(std::move(parameters_result.error()));
  auto parameters = std::move(*parameters_result);
  m_humanoid_parameters = std::move(parameters);
  if (auto result = install_humanoid(); !result)
    return std::unexpected(std::move(result.error()));
  auto movement_parameters_result = ActorMovementParameters::create(json_merge(humanoid()->default_movement_parameters(), humanoid()->player_movement_parameters().value_or(m_config->movement_parameters)));
  if (!movement_parameters_result)
    return std::unexpected(std::move(movement_parameters_result.error()));
  auto movement_parameters = std::move(*movement_parameters_result);
  if (auto result = m_movement_controller->reset_base_parameters(movement_parameters); !result)
    return std::unexpected(std::move(result.error()));
  m_effects_animator->set_global_tag("effectDirectives", species_def->effectDirectives());
  m_death_particle_burst.set(humanoid()->default_death_particles());
  auto properties_result = disk_store.get_object("genericProperties");
  if (!properties_result)
    return std::unexpected(std::move(properties_result.error()));
  auto properties = std::move(*properties_result);
  m_generic_properties = std::move(properties);
  m_armor->reset();
  if (auto result = refresh_armor(); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = set_net_armor_secrets(true); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = m_codexes->learn_initial(species()); !result)
    return std::unexpected(std::move(result.error()));
  auto ai_json = disk_store.find("aiState");
  if (!ai_json)
    return std::unexpected(std::move(ai_json.error()));
  if (*ai_json) {
    auto state = AiState::create((*ai_json)->get());
    if (!state)
      return std::unexpected(std::move(state.error()));
    m_ai_state = std::move(*state);
  } else {
    m_ai_state = {};
  }
  for (auto&& script : m_generic_script_contexts) {
    if (auto result = script.second->set_script_storage({}); !result)
      return std::unexpected(std::move(result.error()));
  }
  auto storage_json_result = disk_store.get("genericScriptStorage", JsonObject{});
  if (!storage_json_result)
    return std::unexpected(std::move(storage_json_result.error()));
  auto storage_json = std::move(*storage_json_result);
  auto storage_result = storage_json.to_object();
  if (!storage_result)
    return std::unexpected(std::move(storage_result.error()));
  auto storage = std::move(*storage_result);
  for (auto&& p : storage) {
    if (auto script = m_generic_script_contexts.maybe(p.first).value_or(std::shared_ptr<GenericScriptComponent>{})) {
      auto object_result = p.second.to_object();
      if (!object_result)
        return std::unexpected(std::move(object_result.error()));
      auto object = std::move(*object_result);
      if (auto result = script->set_script_storage(std::move(object)); !result)
        return std::unexpected(std::move(result.error()));
    }
  }
  for (auto const& descriptor : m_config->default_blueprints)
    m_blueprints->add(descriptor);
  for (auto const& descriptor : species_def->defaultBlueprints())
    m_blueprints->add(descriptor);
  if (m_identity.gender == Gender::Male && m_description == "This gal seems to have nothing to say for herself.")
    m_description = "This guy seems to have nothing to say for himself.";
  else if (m_identity.gender == Gender::Female && m_description == "This guy seems to have nothing to say for himself.")
    m_description = "This gal seems to have nothing to say for herself.";
  return {};
}

std::shared_ptr<ClientContext> Player::client_context() const {
  return m_client_context;
}

void Player::set_client_context(std::shared_ptr<ClientContext> client_context) {
  m_client_context = std::move(client_context);
  if (m_client_context && m_universe_map)
    m_universe_map->setServerUuid(m_client_context->server_uuid());
}

std::shared_ptr<Statistics> Player::statistics() const {
  return m_statistics;
}

void Player::set_statistics(std::shared_ptr<Statistics> statistics) { m_statistics = std::move(statistics); }

void Player::set_universe_client(std::optional<std::reference_wrapper<UniverseClient>> client) {
  m_client = client;
  m_quest_manager->setUniverseClient(client);
}

std::optional<std::reference_wrapper<UniverseClient>> Player::universe_client() const {
  return m_client;
}

EntityType Player::entity_type() const {
  return EntityType::Player;
}

ClientEntityMode Player::client_entity_mode() const {
  return ClientEntityMode::ClientPresenceMaster;
}

std::expected<void, StarException> Player::init(World& world, EntityId entity_id, EntityMode mode) {
  m_hidden_loop_audios.clear();
  m_presentation_bars.clear();
  m_control_movement_state.reset();
  m_movement_animation_state.reset();
  m_movement_controller->set_network_moving_direction({});
  if (auto result = Entity::init(world, entity_id, mode); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = m_tools->init(*this); !result)
    return std::unexpected(std::move(result.error()));
  m_movement_controller->init(world);
  m_movement_controller->set_ignore_physics_entities({entity_id});
  if (auto result = m_status_controller->init(*this, *m_movement_controller); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = m_tech_controller->init(*this, *m_movement_controller, *m_status_controller); !result)
    return std::unexpected(std::move(result.error()));
  auto species_database_result = root().speciesDatabase();
  if (!species_database_result)
    return std::unexpected(std::move(species_database_result.error()));
  auto species_database = std::move(*species_database_result);
  auto species_definition_result = species_database->species(m_identity.species);
  if (!species_definition_result)
    return std::unexpected(std::move(species_definition_result.error()));
  auto species_definition = std::move(*species_definition_result);
  if (mode == EntityMode::Master) {
    m_scripted_animation_parameters.clear();
    m_movement_controller->set_rotation(0);
    m_status_controller->setStatusProperty("ouchNoise", species_definition->ouchNoise(m_identity.gender));
    m_emote_state = HumanoidEmote::Idle;
    if (auto result = m_quest_manager->init(*this, world); !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = m_companions->init(*this, world); !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = m_deployment->init(*this, world); !result)
      return std::unexpected(std::move(result.error()));
    m_mission_radio_messages.clear();
    if (auto result = m_status_controller->setPersistentEffects("species", species_definition->statusEffects()); !result)
      return std::unexpected(std::move(result.error()));
    for (auto&& p : m_generic_script_contexts) {
      if (auto result = p.second->add_actor_movement_callbacks(*m_movement_controller); !result)
        return std::unexpected(std::move(result.error()));
      auto player_callbacks_result = LuaBindings::make_player_callbacks(root(), *this);
      if (!player_callbacks_result)
        return std::unexpected(std::move(player_callbacks_result.error()));
      auto player_callbacks = std::move(*player_callbacks_result);
      if (auto result = p.second->add_callbacks("player", std::move(player_callbacks)); !result)
        return std::unexpected(std::move(result.error()));
      auto entity_callbacks_result = LuaBindings::makeEntityCallbacks(*this);
      if (!entity_callbacks_result)
        return std::unexpected(std::move(entity_callbacks_result.error()));
      auto entity_callbacks = std::move(*entity_callbacks_result);
      if (auto result = p.second->add_callbacks("entity", std::move(entity_callbacks)); !result)
        return std::unexpected(std::move(result.error()));
      auto status_callbacks_result = LuaBindings::makeStatusControllerCallbacks(*m_status_controller);
      if (!status_callbacks_result)
        return std::unexpected(std::move(status_callbacks_result.error()));
      auto status_callbacks = std::move(*status_callbacks_result);
      if (auto result = p.second->add_callbacks("status", std::move(status_callbacks)); !result)
        return std::unexpected(std::move(result.error()));
      auto songbook_callbacks_result = LuaBindings::makeSongbookCallbacks(*m_songbook);
      if (!songbook_callbacks_result)
        return std::unexpected(std::move(songbook_callbacks_result.error()));
      auto songbook_callbacks = std::move(*songbook_callbacks_result);
      if (auto result = p.second->add_callbacks("songbook", std::move(songbook_callbacks)); !result)
        return std::unexpected(std::move(result.error()));
      auto animator_callbacks_result = LuaBindings::make_networked_animator_callbacks(humanoid()->networked_animator()->get());
      if (!animator_callbacks_result)
        return std::unexpected(std::move(animator_callbacks_result.error()));
      auto animator_callbacks = std::move(*animator_callbacks_result);
      if (auto result = p.second->add_callbacks("animator", std::move(animator_callbacks)); !result)
        return std::unexpected(std::move(result.error()));
      if (m_client) {
        auto celestial_callbacks_result = LuaBindings::make_celestial_callbacks(root(), m_client->get());
        if (!celestial_callbacks_result)
          return std::unexpected(std::move(celestial_callbacks_result.error()));
        auto celestial_callbacks = std::move(*celestial_callbacks_result);
        if (auto result = p.second->add_callbacks("celestial", std::move(celestial_callbacks)); !result)
          return std::unexpected(std::move(result.error()));
      }
      if (auto result = p.second->init(world); !result)
        return std::unexpected(std::move(result.error()));
    }
    for (auto& p : m_inventory->pullOverflow()) {
      auto drop_result = ItemDrop::create_randomized_drop(root(), p, m_movement_controller->position(), true);
      if (!drop_result)
        return std::unexpected(std::move(drop_result.error()));
      auto drop = std::move(*drop_result);
      if (auto result = world.addEntity(std::move(drop)); !result)
        return std::unexpected(std::move(result.error()));
    }
    if (auto result = set_net_armor_secrets(); !result)
      return std::unexpected(std::move(result.error()));
  }
  if (world.isClient()) {
    if (auto result = m_scripted_animator.set_scripts(humanoid()->animation_scripts()); !result)
      return std::unexpected(std::move(result.error()));
    auto animation_callbacks_result = LuaBindings::make_scripted_animator_callbacks(humanoid()->networked_animator()->get(),
      [&parameters = m_scripted_animation_parameters](String const& name, Json const& default_value) -> std::expected<Json, StarException> {
        return parameters.value(name, default_value);
      });
    if (!animation_callbacks_result)
      return std::unexpected(std::move(animation_callbacks_result.error()));
    auto animation_callbacks = std::move(*animation_callbacks_result);
    if (auto result = m_scripted_animator.add_callbacks("animationConfig", std::move(animation_callbacks)); !result)
      return std::unexpected(std::move(result.error()));
    auto entity_callbacks_result = LuaBindings::makeEntityCallbacks(*this);
    if (!entity_callbacks_result)
      return std::unexpected(std::move(entity_callbacks_result.error()));
    auto entity_callbacks = std::move(*entity_callbacks_result);
    if (auto result = m_scripted_animator.add_callbacks("entity", std::move(entity_callbacks)); !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = recover_lua_script_error(m_scripted_animator.init(world)); !result)
      return std::unexpected(std::move(result.error()));
  }
  m_x_aim_position_net_state.set_interpolator(world.geometry().x_lerp_function());
  if (auto result = refresh_equipment(); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = update_humanoid_directives(); !result)
    return std::unexpected(std::move(result.error()));
  return humanoid()->prepare_animator_pose();
}

std::expected<void, StarException> Player::uninit() {
  for (auto const& audio : m_hidden_loop_audios)
    audio->stop();
  m_hidden_loop_audios.clear();
  m_presentation_bars.clear();
  std::optional<StarException> error;
  auto remember = [&error](auto result) {
    if (!result) {
      if (!error)
        error = std::move(result.error());
      else
        error = error->with_related(result.error(), "Additional error while uninitializing player");
    }
  };
  remember(m_tech_controller->uninit());
  m_movement_controller->uninit();
  set_wire_connector(std::nullopt);
  remember(m_tools->uninit());
  remember(m_status_controller->uninit());
  if (isMaster()) {
    remember(m_quest_manager->uninit());
    remember(m_companions->uninit());
    remember(m_deployment->uninit());
    for (auto&& p : m_generic_script_contexts) {
      remember(p.second->uninit());
      for (auto group : std::array<std::string_view, 7>{"animator", "entity", "player", "mcontroller", "status", "songbook", "world"})
        remember(p.second->remove_callbacks(group));
      if (m_client)
        remember(p.second->remove_callbacks("celestial"));
    }
  }
  if (auto world = maybeWorld(); world && world->get().isClient()) {
    remember(recover_lua_script_error(m_scripted_animator.uninit()));
    remember(m_scripted_animator.remove_callbacks("animationConfig"));
    remember(m_scripted_animator.remove_callbacks("entity"));
  }
  m_control_movement_state.reset();
  m_movement_animation_state.reset();
  m_movement_controller->set_network_moving_direction({});
  m_pending_alt_music.reset();
  remember(Entity::uninit());
  if (error)
    return std::unexpected(std::move(*error));
  return {};
}

std::expected<void, StarException> Player::update_humanoid_directives() {
  DirectivesGroup directives;
  Vec2F scale = Vec2F::filled(1.f);
  for (auto const& group : {std::cref(m_tech_controller->parentDirectives()), std::cref(m_status_controller->parentDirectives())}) {
    for (auto const& entry : group.get().list()) {
      auto extracted = Humanoid::extract_scale_from_directives(entry);
      if (!extracted)
        return std::unexpected(std::move(extracted.error()));
      scale = scale.piecewise_multiply(extracted->first);
      directives.append(extracted->second);
    }
  }
  humanoid()->set_scale(scale);
  m_presentation_humanoid_directives = std::move(directives);
  return {};
}

std::expected<List<Drawable>, StarException> Player::drawables(float interpolation) const {
  List<Drawable> drawables;
  if (!is_teleporting()) {
    auto back_result = m_tech_controller->backDrawables(interpolation);
    if (!back_result)
      return std::unexpected(std::move(back_result.error()));
    auto back = std::move(*back_result);
    drawables.append_range(std::move(back));
    if (!m_tech_controller->parentHidden()) {
      if (auto result = m_tools->setupHumanoidHandItemDrawables(*humanoid(), interpolation); !result)
        return std::unexpected(std::move(result.error()));
      auto const& directives = m_presentation_humanoid_directives;
      auto rendered_result = humanoid()->render(true, true, interpolation);
      if (!rendered_result)
        return std::unexpected(std::move(rendered_result.error()));
      auto rendered = std::move(*rendered_result);
      for (auto& drawable : rendered) {
        drawable.translate(position() + m_tech_controller->parentOffset());
        if (drawable.is_image()) {
          auto metadata_result = root().image_metadata_database();
          if (!metadata_result)
            return std::unexpected(std::move(metadata_result.error()));
          auto metadata = std::move(*metadata_result);
          if (auto result = drawable.image_part().add_directives_group(*metadata, directives, true); !result)
            return std::unexpected(std::move(result.error()));
          if (auto anchor = as<LoungeAnchor>(m_movement_controller->entity_anchor())) {
            if (auto const& extra = anchor->directives) {
              if (auto result = drawable.image_part().add_directives(*metadata, *extra, true); !result)
                return std::unexpected(std::move(result.error()));
            }
          }
        }
        drawables.push_back(std::move(drawable));
      }
    }
    auto front_result = m_tech_controller->frontDrawables(interpolation);
    if (!front_result)
      return std::unexpected(std::move(front_result.error()));
    auto front = std::move(*front_result);
    drawables.append_range(std::move(front));
    auto status_result = m_status_controller->drawables(interpolation);
    if (!status_result)
      return std::unexpected(std::move(status_result.error()));
    auto status = std::move(*status_result);
    drawables.append_range(std::move(status));
    auto range_result = in_tool_range();
    if (!range_result)
      return std::unexpected(std::move(range_result.error()));
    auto range = std::move(*range_result);
    auto previews_result = m_tools->render_object_previews(m_aim_position, walking_direction(), range, favorite_color());
    if (!previews_result)
      return std::unexpected(std::move(previews_result.error()));
    auto previews = std::move(*previews_result);
    drawables.append_range(std::move(previews));
  }
  auto effects_sample = m_effects_animator->sample(interpolation);
  auto effects_result = effects_sample.drawables(position());
  if (!effects_result)
    return std::unexpected(std::move(effects_result.error()));
  auto effects = std::move(*effects_result);
  drawables.append_range(std::move(effects));
  return drawables;
}


std::expected<List<Particle>, StarException> Player::particles() {
  auto current_world = world();
  if (!current_world)
    return std::unexpected(std::move(current_world.error()));
  auto generated = m_config->splash_config.generate_particles(position(), m_movement_controller->velocity(), current_world->get());
  if (!generated)
    return std::unexpected(std::move(generated.error()));
  auto particles = std::move(*generated);
  particles.append_range(std::exchange(m_callback_particles, {}));
  particles.append_range(m_humanoid_dynamic_target.pull_new_particles());
  particles.append_range(m_tech_controller->pullNewParticles());
  particles.append_range(m_status_controller->pullNewParticles());
  return particles;
}

std::expected<void, StarException> Player::add_particles(List<Particle> const& particles) {
  m_callback_particles.append_range(particles);
  return {};
}

std::expected<void, StarException> Player::add_sound(String const& sound, float volume, float pitch) {
  m_callback_sounds.emplace_back(sound, volume, pitch);
  return {};
}

std::expected<void, StarException> Player::add_ephemeral_status_effects(List<EphemeralStatusEffect> const& status_effects) {
  if (isSlave())
    return std::unexpected(PlayerException("Adding status effects to an entity can only be done directly on the master entity."));
  return m_status_controller->addEphemeralEffects(status_effects);
}

std::expected<ActiveUniqueStatusEffectSummary, StarException> Player::active_unique_status_effect_summary() const {
  return m_status_controller->activeUniqueStatusEffectSummary();
}

std::expected<float, StarException> Player::power_multiplier() const {
  return m_status_controller->stat("powerMultiplier");
}

bool Player::is_dead() const {
  return !m_status_controller->resourcePositive("health");
}

std::expected<void, StarException> Player::kill() {
  return m_status_controller->setResource("health", 0);
}

bool Player::wire_tool_in_use() const { return static_cast<bool>(as<WireTool>(m_tools->primaryHandItem())); }

void Player::set_wire_connector(std::optional<std::reference_wrapper<WireConnector>> wire_connector) const {
  if (auto wire_tool = as<WireTool>(m_tools->primaryHandItem()))
    wire_tool->setConnector(wire_connector);
}

std::expected<List<Drawable>, StarException> Player::portrait(PortraitMode mode) const {
  if (is_perma_dead())
    return humanoid()->render_skull();
  if (invisible())
    return List<Drawable>{};
  if (!inWorld()) {
    if (auto result = refresh_humanoid(); !result)
      return std::unexpected(std::move(result.error()));
  }
  return humanoid()->render_portrait(mode);
}

bool Player::underwater() const {
  if (!inWorld())
    return false;
  return maybeWorld()->get().liquidLevel(Vec2I(position() + m_config->underwater_sensor)).level >= m_config->underwater_min_water_level;
}

std::expected<List<LightSource>, StarException> Player::light_sources() const {
  return presentation_light_sources(1.0f);
}

std::expected<List<LightSource>, StarException> Player::presentation_light_sources(float interpolation) const {
  List<LightSource> lights;
  if (auto result = m_tools->append_light_sources(lights, interpolation); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = m_status_controller->append_light_sources(lights, interpolation); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = m_tech_controller->append_light_sources(lights, interpolation); !result)
    return std::unexpected(std::move(result.error()));

  auto actor = humanoid();
  auto sample = actor->networked_animator()->get().sample(interpolation);
  if (auto prepared = actor->prepare_animator_pose(sample); !prepared)
    return std::unexpected(std::move(prepared.error()));
  if (auto result = sample.append_light_sources(lights); !result)
    return std::unexpected(std::move(result.error()));
  return lights;
}

RectF Player::meta_bound_box() const {
  return m_config->meta_bound_box;
}

std::expected<std::optional<HitType>, StarException> Player::query_hit(DamageSource const& source) const {
  if (!inWorld() || is_dead() || m_is_admin || is_teleporting() || m_status_controller->statPositive("invulnerable"))
    return std::optional<HitType>{};
  auto shield_result = m_tools->queryShieldHit(source);
  if (!shield_result)
    return std::unexpected(std::move(shield_result.error()));
  auto shield = std::move(*shield_result);
  if (shield)
    return std::optional<HitType>{HitType::ShieldHit};
  auto current_world_result = world();
  if (!current_world_result)
    return std::unexpected(std::move(current_world_result.error()));
  auto& current_world = current_world_result->get();
  auto intersects_result = source.intersects_with_poly(current_world.geometry(), m_movement_controller->collision_body());
  if (!intersects_result)
    return std::unexpected(std::move(intersects_result.error()));
  auto intersects = std::move(*intersects_result);
  if (intersects)
    return std::optional<HitType>{HitType::Hit};
  return std::optional<HitType>{};
}

std::expected<std::optional<PolyF>, StarException> Player::hit_poly() const {
  return m_movement_controller->collision_body();
}

std::expected<List<DamageNotification>, StarException> Player::apply_damage(DamageRequest const& request) {
  if (!inWorld() || is_dead() || m_is_admin)
    return {};

  return m_status_controller->apply_damage_request(request);
}

std::expected<List<DamageNotification>, StarException> Player::self_damage_notifications() {
  return m_status_controller->pull_self_damage_notifications();
}

std::expected<void, StarException> Player::hit_other(EntityId target_entity_id, DamageRequest const& damage_request) {
  if (!isMaster())
    return {};
  m_status_controller->hit_other(target_entity_id, damage_request);
  auto current_world_result = world();
  if (!current_world_result)
    return std::unexpected(std::move(current_world_result.error()));
  auto& current_world = current_world_result->get();
  if (as<DamageBarEntity>(current_world.entity(target_entity_id))) {
    m_last_damaged_other_timer = 0;
    m_last_damaged_target = target_entity_id;
  }
  return {};
}

std::expected<void, StarException> Player::damaged_other(DamageNotification const& damage) {
  if (isMaster())
    m_status_controller->damaged_other(damage);
  return {};
}

std::expected<List<DamageSource>, StarException> Player::damage_sources() const {
  return m_damage_sources;
}

std::expected<bool, StarException> Player::should_destroy() const {
  return is_dead();
}

std::expected<void, StarException> Player::destroy(std::optional<std::reference_wrapper<RenderCallback>> render_callback) {
  m_state = State::Idle;
  m_emote_state = HumanoidEmote::Idle;
  if (render_callback && m_death_particle_burst.get()) {
    auto const& burst = *m_death_particle_burst.get();
    auto particles = humanoid()->particles(burst);
    if (!particles) {
      (void)root().logger().warn("Skipping player death particles '{}': {}", burst, particles.error().what());
    } else if (auto added = render_callback->get().add_particles(std::move(*particles), position()); !added) {
      (void)root().logger().warn("Skipping player death particles '{}': {}", burst, added.error().what());
    }
  }
  if (isMaster()) {
    m_log->addDeathCount(1);
    auto current_world_result = world();
    if (!current_world_result)
      return std::unexpected(std::move(current_world_result.error()));
    auto& current_world = current_world_result->get();
    auto disabled_result = current_world.disableDeathDrops();
    if (!disabled_result)
      return std::unexpected(std::move(disabled_result.error()));
    auto disabled = std::move(*disabled_result);
    if (!disabled) {
      auto const& drop_types = m_mode_config.deathDropItemTypes;
      if (auto drop_string = drop_types.maybe_left_ref()) {
        if (drop_string->get() == "all") {
          if (auto result = drop_everything(); !result)
            return std::unexpected(std::move(result.error()));
        }
      } else {
        Set<ItemType> types;
        for (auto const& name : drop_types.right_ref()) {
          auto type_result = enum_value<item_type_names>(name);
          if (!type_result)
            return std::unexpected(std::move(type_result.error()));
          auto const& type = *type_result;
          types.add(type);
        }
        auto database_result = root().itemDatabase();
        if (!database_result)
          return std::unexpected(std::move(database_result.error()));
        auto database = std::move(*database_result);
        if (auto result = drop_selected_items([types = std::move(types), database](std::shared_ptr<Item> item) -> std::expected<bool, StarException> {
          auto type_result = database->item_type(item->name());
          if (!type_result)
            return std::unexpected(std::move(type_result.error()));
          auto type = std::move(*type_result);
          return types.contains(type);
        }); !result)
          return std::unexpected(std::move(result.error()));
      }
    }
  }
  m_songbook->stop();
  return {};
}

std::optional<EntityAnchorState> Player::lounging_in() const {
  if (is<LoungeAnchor>(m_movement_controller->entity_anchor()))
    return m_movement_controller->anchor_state();
  return {};
}

std::expected<bool, StarException> Player::lounge(EntityId loungeable_entity_id, std::size_t anchor_index) {
  if (!can_use_tool())
    return false;
  auto current_world_result = world();
  if (!current_world_result)
    return std::unexpected(std::move(current_world_result.error()));
  auto& current_world = current_world_result->get();
  auto loungeable_entity = current_world.get<LoungeableEntity>(loungeable_entity_id);
  if (!loungeable_entity || anchor_index >= loungeable_entity->anchorCount())
    return false;
  auto loungers_result = loungeable_entity->entitiesLoungingIn(anchor_index);
  if (!loungers_result)
    return std::unexpected(std::move(loungers_result.error()));
  auto loungers = std::move(*loungers_result);
  if (!loungers.empty())
    return false;
  auto anchor_result = loungeable_entity->loungeAnchor(anchor_index);
  if (!anchor_result)
    return std::unexpected(std::move(anchor_result.error()));
  auto anchor = std::move(*anchor_result);
  if (!anchor)
    return false;
  m_state = State::Lounge;
  if (auto result = m_movement_controller->set_anchor_state({loungeable_entity_id, anchor_index}); !result)
    return std::unexpected(std::move(result.error()));
  return true;
}

std::expected<void, StarException> Player::stop_lounging() {
  if (lounging_in()) {
    {
      if (auto result = m_movement_controller->reset_anchor_state(); !result)
        return std::unexpected(std::move(result.error()));
    }
    m_state = State::Idle;
    {
      if (auto result = m_status_controller->setPersistentEffects("lounging", {}); !result)
        return std::unexpected(std::move(result.error()));
    }
  }
  return {};
}

Vec2F Player::position() const {
  return m_movement_controller->position();
}

Vec2F Player::velocity() const {
  return m_movement_controller->velocity();
}

std::expected<Vec2F, StarException> Player::mouth_offset(bool ignore_adjustments) const {
  auto offset_result = humanoid()->mouth_offset(ignore_adjustments);
  if (!offset_result)
    return std::unexpected(std::move(offset_result.error()));
  auto offset = std::move(*offset_result);
  return Vec2F(offset[0] * numerical_direction(facing_direction()), offset[1]);
}

std::expected<Vec2F, StarException> Player::feet_offset() const {
  auto offset_result = humanoid()->feet_offset();
  if (!offset_result)
    return std::unexpected(std::move(offset_result.error()));
  auto offset = std::move(*offset_result);
  return Vec2F(offset[0] * numerical_direction(facing_direction()), offset[1]);
}

std::expected<Vec2F, StarException> Player::head_armor_offset() const {
  auto offset_result = humanoid()->head_armor_offset();
  if (!offset_result)
    return std::unexpected(std::move(offset_result.error()));
  auto offset = std::move(*offset_result);
  return Vec2F(offset[0] * numerical_direction(facing_direction()), offset[1]);
}

std::expected<Vec2F, StarException> Player::chest_armor_offset() const {
  auto offset_result = humanoid()->chest_armor_offset();
  if (!offset_result)
    return std::unexpected(std::move(offset_result.error()));
  auto offset = std::move(*offset_result);
  return Vec2F(offset[0] * numerical_direction(facing_direction()), offset[1]);
}

std::expected<Vec2F, StarException> Player::back_armor_offset() const {
  auto offset_result = humanoid()->back_armor_offset();
  if (!offset_result)
    return std::unexpected(std::move(offset_result.error()));
  auto offset = std::move(*offset_result);
  return Vec2F(offset[0] * numerical_direction(facing_direction()), offset[1]);
}

std::expected<Vec2F, StarException> Player::legs_armor_offset() const {
  auto offset_result = humanoid()->legs_armor_offset();
  if (!offset_result)
    return std::unexpected(std::move(offset_result.error()));
  auto offset = std::move(*offset_result);
  return Vec2F(offset[0] * numerical_direction(facing_direction()), offset[1]);
}

std::expected<Vec2F, StarException> Player::mouth_position() const {
  return mouth_position(true);
}

std::expected<Vec2F, StarException> Player::mouth_position(bool ignore_adjustments) const {
  auto offset_result = mouth_offset(ignore_adjustments);
  if (!offset_result)
    return std::unexpected(std::move(offset_result.error()));
  auto offset = std::move(*offset_result);
  return position() + offset;
}

RectF Player::collision_area() const {
  return m_movement_controller->collision_poly().bounding_box();
}

std::expected<void, StarException> Player::revive(Vec2F const& foot_position) {
  if (!is_dead())
    return {};
  m_state = State::Idle;
  m_emote_state = HumanoidEmote::Idle;
  if (auto result = synchronize_equipment_status_effects(*m_armor, *m_tools, *m_status_controller); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = m_status_controller->resetAllResources(); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = m_status_controller->clearEphemeralEffects(); !result)
    return std::unexpected(std::move(result.error()));
  end_primary_fire();
  end_alt_fire();
  end_trigger();
  m_effect_emitter->reset();
  auto offset_result = feet_offset();
  if (!offset_result)
    return std::unexpected(std::move(offset_result.error()));
  auto offset = std::move(*offset_result);
  resetPresentation();
  m_movement_controller->set_position(foot_position - offset);
  m_movement_controller->set_velocity(Vec2F());
  if (auto result = m_tech_controller->reload_tech(); !result)
    return std::unexpected(std::move(result.error()));
  float money_cost = m_inventory->currency("money") * m_mode_config.reviveCostPercentile;
  m_inventory->consumeCurrency(
    "money", std::min(static_cast<std::uint64_t>(std::round(money_cost)), m_inventory->currency("money")));
  return {};
}

bool Player::shifting() const {
  return m_shifting;
}

void Player::set_shifting(bool shifting) {
  m_shifting = shifting;
}

std::expected<void, StarException> Player::special(int special_key) {
  auto lounge_anchor = as<LoungeAnchor>(m_movement_controller->entity_anchor());
  if (lounge_anchor && lounge_anchor->controllable) {
    auto anchor_state = m_movement_controller->anchor_state();
    if (!anchor_state)
      return std::unexpected(PlayerException("Player lounge anchor has no anchor state"));
    {
      auto current_world_result = world();
      if (!current_world_result)
        return std::unexpected(std::move(current_world_result.error()));
      auto& current_world = current_world_result->get();
      if (auto loungeable_entity = current_world.get<LoungeableEntity>(anchor_state->entityId)) {
        if (special_key == 1)
          return loungeable_entity->loungeControl(anchor_state->positionIndex, LoungeControl::Special1);
        if (special_key == 2)
          return loungeable_entity->loungeControl(anchor_state->positionIndex, LoungeControl::Special2);
        if (special_key == 3)
          return loungeable_entity->loungeControl(anchor_state->positionIndex, LoungeControl::Special3);
        return {};
      }
    }
  }
  m_tech_controller->special(special_key);
  return {};
}

void Player::set_move_vector(Vec2F const& vec) {
  m_move_vector = vec;
}

void Player::move_left() {
  m_pending_moves.add(MoveControlType::Left);
}

void Player::move_right() {
  m_pending_moves.add(MoveControlType::Right);
}

void Player::move_up() {
  m_pending_moves.add(MoveControlType::Up);
}

void Player::move_down() {
  m_pending_moves.add(MoveControlType::Down);
}

void Player::jump() {
  m_pending_moves.add(MoveControlType::Jump);
}

std::expected<void, StarException> Player::drop_item() {
  auto current_world = maybeWorld();
  if (!current_world || !can_use_tool())
    return {};
  Vec2F throw_direction = current_world->get().geometry().diff(m_aim_position, position());
  auto primary_slot_result = m_inventory->primary_held_slot();
  if (!primary_slot_result)
    return std::unexpected(std::move(primary_slot_result.error()));
  auto primary_slot = std::move(*primary_slot_result);
  auto secondary_slot_result = m_inventory->secondary_held_slot();
  if (!secondary_slot_result)
    return std::unexpected(std::move(secondary_slot_result.error()));
  auto secondary_slot = std::move(*secondary_slot_result);
  for (auto const& throw_slot : {primary_slot, secondary_slot}) {
    if (throw_slot) {
      auto drop_result = m_inventory->takeSlot(*throw_slot);
      if (!drop_result)
        return std::unexpected(std::move(drop_result.error()));
      auto drop = std::move(*drop_result);
      if (drop) {
        auto entity_result = ItemDrop::throw_drop(root(), drop, position(), velocity(), throw_direction);
        if (!entity_result)
          return std::unexpected(std::move(entity_result.error()));
        auto entity = std::move(*entity_result);
        return current_world->get().addEntity(std::move(entity));
      }
    }
  }
  return {};
}

std::optional<Player::PresentationMessage> Player::presentation_message(String const& message) {
  if (message == "playCinematic")
    return PresentationMessage::Cinematic;
  if (message == "playAltMusic")
    return PresentationMessage::PlayMusic;
  if (message == "stopAltMusic")
    return PresentationMessage::StopMusic;
  return {};
}

std::expected<std::optional<String>, StarException> Player::process_presentation_message(
    Root& root, PresentationMessage message, JsonArray const& args, std::optional<std::reference_wrapper<Player>> recipient) {
  std::shared_ptr<Assets const> assets;
  auto check_path = [&](String const& path) -> std::expected<std::optional<String>, StarException> {
    auto locator = AssetPath::split(path);
    if (locator.base_path.empty() || !locator.directives.empty() || (locator.sub_path && message != PresentationMessage::Cinematic))
      return Star::format("Invalid presentation asset path '{}'", path);
    if (!assets) {
      auto loaded = root.assets();
      if (!loaded)
        return std::unexpected(std::move(loaded.error()));
      assets = std::move(*loaded);
    }
    // asset_exists only validates/folds the supplied UTF-8 key and probes the index.
    auto exists = assets->asset_exists(locator.base_path);
    if (!exists)
      return String(exists.error().what());
    if (!*exists)
      return Star::format("Unknown presentation asset '{}'", path);
    return {};
  };

  if (message == PresentationMessage::Cinematic) {
    if (args.empty() || !(args[0].is_type(Json::Type::Object) || args[0].is_type(Json::Type::String)))
      return String("playCinematic requires an object or an asset path");
    bool unique = false;
    if (args.size() > 1) {
      auto value = args[1].to_bool();
      if (!value)
        return String("playCinematic unique must be a boolean");
      unique = *value;
    }
    if (args[0].is_type(Json::Type::String)) {
      auto path = args[0].string_ptr();
      if (!path)
        return std::unexpected(std::move(path.error()));
      auto checked = check_path(**path);
      if (!checked || *checked)
        return checked;
    }
    if (recipient) {
      auto queued = recipient->get().set_pending_cinematic(args[0], unique);
      if (!queued)
        return std::unexpected(std::move(queued.error()));
    }
    return {};
  }

  AltMusicRequest request;
  bool play = message == PresentationMessage::PlayMusic;
  std::size_t fade_index = play ? 1 : 0;
  if (args.size() > fade_index) {
    auto fade = args[fade_index].to_double();
    if (!fade || !is_finite(*fade) || *fade < 0 || *fade > std::numeric_limits<float>::max())
      return String("Music fade time must be a finite nonnegative duration");
    request.fade_time = static_cast<float>(*fade);
  }
  if (play) {
    if (args.empty())
      return String("playAltMusic requires a track or playlist");
    if (args.size() > 2) {
      auto loops = args[2].to_int();
      if (!loops || !std::in_range<int>(*loops))
        return String("playAltMusic loops must fit in an integer");
      request.loops = static_cast<int>(*loops);
    }
    if (recipient)
      request.tracks.emplace();
    auto track = [&](Json const& value) -> std::expected<std::optional<String>, StarException> {
      auto path = value.string_ptr();
      if (!path)
        return String("playAltMusic tracks must be asset paths");
      auto checked = check_path(**path);
      if (checked && !*checked && recipient)
        request.tracks->push_back(**path);
      return checked;
    };
    if (args[0].is_type(Json::Type::Array)) {
      auto tracks = args[0].array_ptr();
      if (!tracks)
        return std::unexpected(std::move(tracks.error()));
      if (recipient)
        request.tracks->reserve((*tracks)->size());
      for (auto const& value : **tracks) {
        auto checked = track(value);
        if (!checked || *checked)
          return checked;
      }
    } else {
      auto checked = track(args[0]);
      if (!checked || *checked)
        return checked;
    }
  }
  if (recipient)
    recipient->get().m_pending_alt_music = std::move(request);
  return {};
}

MessageResponseResult Player::receive_message(ConnectionId from_connection, String const& message, JsonArray const& args) {
  auto current_world_result = world();
  if (!current_world_result)
    return std::unexpected(std::move(current_world_result.error()));
  auto& current_world = current_world_result->get();
  bool local_message = from_connection == current_world.connection();
  if (message == "queueRadioMessage" && !args.empty()) {
    float delay = 0;
    if (args.size() > 1 && args[1].can_convert(Json::Type::Float)) {
      auto value_result = args[1].to_float();
      if (!value_result)
        return std::unexpected(std::move(value_result.error()));
      auto value = std::move(*value_result);
      delay = value;
    }
    auto checked = process_radio_message(args[0], delay);
    if (!checked)
      return std::unexpected(std::move(checked.error()));
    if (*checked)
      return ChainableJsonMessageResponse(RpcPromise<Json>::make_failed(std::move(**checked)));
    return ChainableJsonMessageResponse(Json{});
  } else if (message == "warp") {
    std::optional<String> animation;
    if (args.size() > 1) {
      auto value_result = args[1].to_string();
      if (!value_result)
        return std::unexpected(std::move(value_result.error()));
      auto value = std::move(*value_result);
      animation = std::move(value);
    }
    bool deploy = false;
    if (args.size() > 2) {
      auto value_result = args[2].to_bool();
      if (!value_result)
        return std::unexpected(std::move(value_result.error()));
      auto value = std::move(*value_result);
      deploy = value;
    }
    auto action_result = args.get(0).to_string();
    if (!action_result)
      return std::unexpected(std::move(action_result.error()));
    auto action = std::move(*action_result);
    set_pending_warp(action, animation, deploy);
  } else if (message == "interruptRadioMessage") {
    m_interrupt_radio_message = true;
  } else if (auto presentation = presentation_message(message)) {
    auto checked = process_presentation_message(root(), *presentation, args, std::ref(*this));
    if (!checked)
      return std::unexpected(std::move(checked.error()));
    if (*checked)
      return ChainableJsonMessageResponse(RpcPromise<Json>::make_failed(std::move(**checked)));
    return ChainableJsonMessageResponse(Json{});
  } else if (message == "recordEvent") {
    auto event_json_result = args.at(0);
    if (!event_json_result)
      return std::unexpected(std::move(event_json_result.error()));
    auto const& event_json = event_json_result->get();
    auto fields_result = args.at(1);
    if (!fields_result)
      return std::unexpected(std::move(fields_result.error()));
    auto const& fields = fields_result->get();
    auto event_result = event_json.to_string();
    if (!event_result)
      return std::unexpected(std::move(event_result.error()));
    auto event = std::move(*event_result);
    statistics()->recordEvent(event, fields);
  } else if (message == "addCollectable") {
    auto collection_result = args.get(0).to_string();
    if (!collection_result)
      return std::unexpected(std::move(collection_result.error()));
    auto collection = std::move(*collection_result);
    auto collectable_result = args.get(1).to_string();
    if (!collectable_result)
      return std::unexpected(std::move(collectable_result.error()));
    auto collectable = std::move(*collectable_result);
    auto database_result = root().collection_database();
    if (!database_result)
      return std::unexpected(std::move(database_result.error()));
    auto database = std::move(*database_result);
    if (database->contains(collection, collectable)) {
      if (auto result = add_collectable(collection, collectable); !result)
        return std::unexpected(std::move(result.error()));
    }
  } else {
    if (auto response = m_tools->receive_message(message, local_message, args); !response || *response)
      return response;
    if (auto response = m_status_controller->receive_message(message, local_message, args); !response || *response)
      return response;
    if (auto response = m_companions->receive_message(message, local_message, args); !response || *response)
      return response;
    if (auto response = m_deployment->receive_message(message, local_message, args); !response || *response)
      return response;
    if (auto response = m_tech_controller->receive_message(message, local_message, args); !response || *response)
      return response;
    if (auto response = m_quest_manager->receive_message(message, local_message, args); !response || *response)
      return response;
    for (auto const& entry : m_generic_script_contexts) {
      auto response = entry.second->handle_message(message, local_message, args);
      if (!response || *response)
        return response;
    }
  }
  return std::nullopt;
}

std::expected<void, StarException> Player::update(float dt, std::uint64_t) {
  auto current_world_result = world();
  if (!current_world_result)
    return std::unexpected(std::move(current_world_result.error()));
  auto& current_world = current_world_result->get();
  m_movement_controller->set_timestep(dt);

  if (isMaster()) {
    // Requests from postUpdate, messages before this tick, or a prior world
    // must not outlive the generic update that actually refreshes them.
    m_control_movement_state.reset();
    if (m_emote_cooldown_timer.tick(dt))
      m_emote_state = HumanoidEmote::Idle;
    m_dance.tick(dt);

    if (m_chat_message_updated) {
      auto state = EmoteProcessor::detect_chat_emote(root(), m_chat_message);
      if (!state) return std::unexpected(std::move(state.error()));
      if (*state != HumanoidEmote::Idle)
        add_emote(*state);
      m_chat_message_updated = false;
    }

    if (m_blink_cooldown_timer.tick(dt)) {
      auto blink_interval_result = root().random().uniform_float(m_blink_interval[0], m_blink_interval[1]);
      if (!blink_interval_result)
        return std::unexpected(std::move(blink_interval_result.error()));
      auto blink_interval = std::move(*blink_interval_result);
      m_blink_cooldown_timer = GameTimer(blink_interval);
      auto lounge_anchor = as<LoungeAnchor>(m_movement_controller->entity_anchor());
      if (m_emote_state == HumanoidEmote::Idle && (!lounge_anchor || !lounge_anchor->emote))
        add_emote(HumanoidEmote::Blink);
    }

    m_last_damaged_other_timer += dt;

    if (m_movement_controller->zero_g())
      m_movement_controller->control_parameters(m_zero_g_movement_parameters);

    if (is_teleporting()) {
      m_teleport_timer -= dt;
      if (m_teleport_timer <= 0 && m_state == State::TeleportIn) {
        m_state = State::Idle;
        {
          if (auto result = m_effects_animator->burst_particle_emitter(m_teleport_animation_type + "Burst"); !result)
            return std::unexpected(std::move(result.error()));
        }
      }
    }

    if (!is_teleporting()) {
      {
        if (auto result = process_controls(); !result)
          return std::unexpected(std::move(result.error()));
      }

      {
        if (auto result = m_quest_manager->update(dt); !result)
          return std::unexpected(std::move(result.error()));
      }
      {
        if (auto result = m_companions->update(dt); !result)
          return std::unexpected(std::move(result.error()));
      }
      {
        if (auto result = m_deployment->update(dt); !result)
          return std::unexpected(std::move(result.error()));
      }

      // Keep the existing input cutoff. A failed eligible edge retries before
      // a newer press queued by a callback after this point.
      if (!m_use_edge_ready)
        m_use_edge_ready = std::exchange(m_use_edge_pending, false);

      {
        if (auto result = m_inventory->cleanup(); !result)
          return std::unexpected(std::move(result.error()));
      }
      {
        if (auto result = refresh_equipment(); !result)
          return std::unexpected(std::move(result.error()));
      }

      auto conflicting_anchor_result = inConflictingLoungeAnchor();
      if (!conflicting_anchor_result)
        return std::unexpected(std::move(conflicting_anchor_result.error()));
      auto conflicting_anchor = std::move(*conflicting_anchor_result);
      if (conflicting_anchor)
        {
          if (auto result = m_movement_controller->reset_anchor_state(); !result)
            return std::unexpected(std::move(result.error()));
        }

      if (m_state == State::Lounge) {
        if (auto lounge_anchor = as<LoungeAnchor>(m_movement_controller->entity_anchor())) {
          {
            if (auto result = m_status_controller->setPersistentEffects("lounging", lounge_anchor->status_effects); !result)
              return std::unexpected(std::move(result.error()));
          }
          {
            if (auto result = add_effect_emitters(lounge_anchor->effect_emitters); !result)
              return std::unexpected(std::move(result.error()));
          }
          if (lounge_anchor->emote)
            {
              if (auto result = request_emote(*lounge_anchor->emote); !result)
                return std::unexpected(std::move(result.error()));
            }

          // Equipment refresh has already restored inventory cosmetics for this tick.
          auto const& overrides = lounge_anchor->armor_cosmetic_overrides;
          if (!overrides.empty()) {
            if (auto result = apply_lounge_cosmetic_overrides(root(), overrides, *m_armor); !result)
              return std::unexpected(std::move(result.error()));
          }
        } else {
          m_state = State::Idle;
          {
            if (auto result = m_movement_controller->reset_anchor_state(); !result)
              return std::unexpected(std::move(result.error()));
          }
        }
      } else {
        {
          if (auto result = m_movement_controller->reset_anchor_state(); !result)
            return std::unexpected(std::move(result.error()));
        }
        {
          if (auto result = m_status_controller->setPersistentEffects("lounging", {}); !result)
            return std::unexpected(std::move(result.error()));
        }
      }

      if (!force_nude())
        {
          if (auto result = m_armor->effects(*m_effect_emitter); !result)
            return std::unexpected(std::move(result.error()));
        }

      {
        if (auto result = m_tools->effects(*m_effect_emitter); !result)
          return std::unexpected(std::move(result.error()));
      }

      auto aim_relative = current_world.geometry().diff(m_aim_position, position()); // dumb, but due to how things are ordered
      {
        if (auto result = m_movement_controller->tick_master(dt); !result)
          return std::unexpected(std::move(result.error()));
      }
      m_aim_position = position() + aim_relative;                               // it's gonna have to be like this for now

      {
        if (auto result = m_tech_controller->tickMaster(dt); !result)
          return std::unexpected(std::move(result.error()));
      }

      for (auto&& p : m_generic_script_contexts) {
        if (auto result = p.second->update(p.second->update_dt(dt)); !result)
          return std::unexpected(std::move(result.error()));
      }

      if (m_use_edge_ready) {
        auto anchor = as<LoungeAnchor>(m_movement_controller->entity_anchor());
        bool tool_available = can_use_tool();
        if (anchor && (!tool_available || anchor->controllable))
          {
            if (auto result = m_movement_controller->reset_anchor_state(); !result)
              return std::unexpected(std::move(result.error()));
          }
        else if (tool_available) {
          auto target = best_interaction_entity(true);
          if (!target)
            return std::unexpected(std::move(target.error()));
          if (*target) {
            if (auto result = dispatch_use_interaction(*target); !result)
              return std::unexpected(std::move(result.error()));
          }
        }
        m_use_edge_ready = false;
      }

      if (auto result = synchronize_equipment_status_effects(*m_armor, *m_tools, *m_status_controller); !result)
        return std::unexpected(std::move(result.error()));

      if (auto result = m_tech_controller->sync_equipped_tech(m_techs->equipped_techs()); !result)
        return std::unexpected(std::move(result.error()));

      if (!is_dead())
        {
          if (auto result = m_status_controller->tickMaster(dt); !result)
            return std::unexpected(std::move(result.error()));
        }

      if (!m_mode_config.hunger)
        {
          if (auto result = m_status_controller->resetResource("food"); !result)
            return std::unexpected(std::move(result.error()));
        }

      if (!m_status_controller->resourcePositive("food")) {
        if (auto result = m_status_controller->setPersistentEffects("hunger", m_food_empty_status_effects); !result)
          return std::unexpected(std::move(result.error()));
      } else {
        auto food_result = m_status_controller->resourcePercentage("food");
        if (!food_result)
          return std::unexpected(std::move(food_result.error()));
        auto food = std::move(*food_result);
        if (food.value_or(0.0f) <= m_food_low_threshold) {
          if (auto result = m_status_controller->setPersistentEffects("hunger", m_food_low_status_effects); !result)
            return std::unexpected(std::move(result.error()));
        } else {
          if (auto result = m_status_controller->setPersistentEffects("hunger", {}); !result)
            return std::unexpected(std::move(result.error()));
        }
      }

      auto retained = m_delayed_radio_messages.begin();
      auto const end = m_delayed_radio_messages.end();
      for (auto entry = retained; entry != end; ++entry) {
        auto& [timer, message] = *entry;
        if (timer.tick(dt)) {
          enqueue_radio_message(std::move(message));
        } else {
          if (retained != entry)
            *retained = std::move(*entry);
          ++retained;
        }
      }
      if (retained != end)
        m_delayed_radio_messages.erase(retained, end);
    }

    if (m_is_admin) {
      {
        if (auto result = m_status_controller->resetResource("health"); !result)
          return std::unexpected(std::move(result.error()));
      }
      {
        if (auto result = m_status_controller->resetResource("energy"); !result)
          return std::unexpected(std::move(result.error()));
      }
      {
        if (auto result = m_status_controller->resetResource("food"); !result)
          return std::unexpected(std::move(result.error()));
      }
      {
        if (auto result = m_status_controller->resetResource("breath"); !result)
          return std::unexpected(std::move(result.error()));
      }
    }

    m_log->addPlayTime(root().timing().time_step);

    if (m_age_items_timer.wrap_tick(dt)) {
      auto item_database_result = root().itemDatabase();
  if (!item_database_result)
    return std::unexpected(std::move(item_database_result.error()));
  auto item_database = std::move(*item_database_result);
      if (auto result = m_inventory->for_each_item([&](InventorySlot const&, std::shared_ptr<Item>& item) -> std::expected<void, StarException> {
        if (auto result = item_database->age_item(item, m_age_items_timer.duration); !result)
          return std::unexpected(std::move(result.error()));
        return {};
      }); !result)
        return std::unexpected(std::move(result.error()));
    }

    for (auto& tool : {m_tools->primaryHandItem(), m_tools->altHandItem()}) {
      if (auto inspection_tool = as<InspectionTool>(tool)) {
        for (auto& ir : inspection_tool->pullInspectionResults()) {
          if (ir.objectName) {
            if (!ir.entityId)
              return std::unexpected(PlayerException("Scanned object has no entity id"));
            if (auto result = m_quest_manager->receive_message("objectScanned", true, {*ir.objectName, *ir.entityId}); !result)
              return std::unexpected(std::move(result.error()));
            m_log->addScannedObject(*ir.objectName);
          }

          if (auto chat = add_chat_message(ir.message, JsonObject{
            {"message", JsonObject{
              {"context", JsonObject{{"mode", "RadioMessage"}}},
              {"fromConnection", current_world.connection()},
              {"text", ir.message}
            }}
          }); !chat)
            return std::unexpected(std::move(chat.error()));
        }
      }
    }

    auto interesting_objects = m_quest_manager->interesting_objects();
    if (!interesting_objects)
      return std::unexpected(std::move(interesting_objects.error()));
    m_interesting_objects = std::move(*interesting_objects);

  } else {
    {
      if (auto result = m_net_group.tick_net_interpolation(dt); !result)
        return std::unexpected(std::move(result.error()));
    }
    {
      if (auto result = m_movement_controller->tick_slave(dt); !result)
        return std::unexpected(std::move(result.error()));
    }
    {
      if (auto result = m_tech_controller->tickSlave(dt); !result)
        return std::unexpected(std::move(result.error()));
    }
    {
      if (auto result = m_status_controller->tickSlave(dt); !result)
        return std::unexpected(std::move(result.error()));
    }
  }

  update_movement_animation_state();

  humanoid()->set_rotation(m_movement_controller->rotation());

  bool suppressed_items = !can_use_tool();

  auto lounge_anchor = as<LoungeAnchor>(m_movement_controller->entity_anchor());
  if (lounge_anchor && lounge_anchor->dance) {
    if (auto result = humanoid()->set_dance(*lounge_anchor->dance); !result)
      return std::unexpected(std::move(result.error()));
  } else if (m_dance.name) {
    if (auto result = humanoid()->set_dance(m_dance.name); !result)
      return std::unexpected(std::move(result.error()));
  } else {
    bool clear_dance = !suppressed_items && (m_tools->primaryHandItem() || m_tools->altHandItem());
    if (!clear_dance) {
      auto cyclic_or_ended = humanoid()->dance_cyclic_or_ended();
      if (!cyclic_or_ended)
        return std::unexpected(std::move(cyclic_or_ended.error()));
      clear_dance = *cyclic_or_ended || m_movement_controller->running();
    }
    if (clear_dance) {
      if (auto result = humanoid()->set_dance({}); !result)
        return std::unexpected(std::move(result.error()));
    }
  }
  bool is_client = current_world.isClient();

  m_tools->suppressItems(suppressed_items);
  {
    if (auto result = m_tools->tick(dt, m_shifting, m_pending_moves); !result)
      return std::unexpected(std::move(result.error()));
  }

  Direction facing_direction = m_movement_controller->facing_direction();

  auto override_facing_direction_result = m_tools->setupHumanoidHandItems(*humanoid(), position(), m_aim_position);
  if (!override_facing_direction_result)
    return std::unexpected(std::move(override_facing_direction_result.error()));
  auto override_facing_direction = std::move(*override_facing_direction_result);
  if (!lounging_in() && override_facing_direction)
    m_movement_controller->control_face(facing_direction = *override_facing_direction);

  humanoid()->set_facing_direction(facing_direction);
  Direction moving_direction = m_movement_controller->moving_direction();
  if (m_movement_animation_state) {
    auto tangent = Vec2F::with_angle(m_movement_controller->rotation());
    auto velocity = m_movement_controller->velocity();
    float tangent_velocity = velocity.x() * tangent.x() + velocity.y() * tangent.y();
    moving_direction = tangent_velocity == 0.f ? facing_direction : tangent_velocity < 0.f ? Direction::Left : Direction::Right;
  }
  if (isMaster())
    m_movement_controller->set_network_moving_direction(m_movement_animation_state ? std::optional(moving_direction) : std::nullopt);
  if (auto result = humanoid()->set_moving_backwards(facing_direction != moving_direction); !result)
    return std::unexpected(std::move(result.error()));

  {
    if (auto result = refresh_humanoid(); !result)
      return std::unexpected(std::move(result.error()));
  }

  auto scale = Mat3F::scaling(Vec2F(facing_direction == Direction::Right ? 1.f : -1.f, 1.f));
  {
    if (auto result = m_effects_animator->set_transformation_group("flip", scale); !result)
      return std::unexpected(std::move(result.error()));
  }

  if (m_state == State::Walk || m_state == State::Run) {
    if ((m_footstep_timer += dt) > m_config->footstep_timing) {
      m_footstep_pending = true;
      m_footstep_timer = 0.0;
    }
  }

  if (is_client) {
    {
      if (auto result = m_effects_animator->update(dt, std::ref(m_effects_animator_dynamic_target)); !result)
        return std::unexpected(std::move(result.error()));
    }
    m_effects_animator_dynamic_target.update_position(position() + m_tech_controller->parentOffset());
  } else {
    {
      if (auto result = m_effects_animator->update(dt, std::nullopt); !result)
        return std::unexpected(std::move(result.error()));
    }
  }

  if (!is_teleporting())
    {
      if (auto result = process_state_changes(dt); !result)
        return std::unexpected(std::move(result.error()));
    }

  auto damage_sources_result = m_tools->damageSources();
  if (!damage_sources_result)
    return std::unexpected(std::move(damage_sources_result.error()));
  auto damage_sources = std::move(*damage_sources_result);
  m_damage_sources = std::move(damage_sources);
  for (auto& damage_source : m_damage_sources) {
    damage_source.source_entity_id = entityId();
    damage_source.team = getTeam();
  }

  {
    if (auto result = m_songbook->update(*entityMode(), current_world); !result)
      return std::unexpected(std::move(result.error()));
  }

  m_effect_emitter->set_source_position("normal", position());
  auto mouth_offset_result = mouth_offset(true);
  if (!mouth_offset_result)
    return std::unexpected(std::move(mouth_offset_result.error()));
  auto mouth_offset = std::move(*mouth_offset_result);
  m_effect_emitter->set_source_position("mouth", mouth_offset + position());
  auto feet_offset_result = feet_offset();
  if (!feet_offset_result)
    return std::unexpected(std::move(feet_offset_result.error()));
  auto feet_offset = std::move(*feet_offset_result);
  m_effect_emitter->set_source_position("feet", feet_offset + position());
  auto head_armor_offset_result = head_armor_offset();
  if (!head_armor_offset_result)
    return std::unexpected(std::move(head_armor_offset_result.error()));
  auto head_armor_offset = std::move(*head_armor_offset_result);
  m_effect_emitter->set_source_position("headArmor", head_armor_offset + position());
  auto chest_armor_offset_result = chest_armor_offset();
  if (!chest_armor_offset_result)
    return std::unexpected(std::move(chest_armor_offset_result.error()));
  auto chest_armor_offset = std::move(*chest_armor_offset_result);
  m_effect_emitter->set_source_position("chestArmor", chest_armor_offset + position());
  auto legs_armor_offset_result = legs_armor_offset();
  if (!legs_armor_offset_result)
    return std::unexpected(std::move(legs_armor_offset_result.error()));
  auto legs_armor_offset = std::move(*legs_armor_offset_result);
  m_effect_emitter->set_source_position("legsArmor", legs_armor_offset + position());
  auto back_armor_offset_result = back_armor_offset();
  if (!back_armor_offset_result)
    return std::unexpected(std::move(back_armor_offset_result.error()));
  auto back_armor_offset = std::move(*back_armor_offset_result);
  m_effect_emitter->set_source_position("backArmor", back_armor_offset + position());

  auto primary_offset_result = hand_position(ToolHand::Primary);
  if (!primary_offset_result)
    return std::unexpected(std::move(primary_offset_result.error()));
  auto primary_offset = std::move(*primary_offset_result);
  m_effect_emitter->set_source_position("primary", primary_offset + position());
  auto alt_offset_result = hand_position(ToolHand::Alt);
  if (!alt_offset_result)
    return std::unexpected(std::move(alt_offset_result.error()));
  auto alt_offset = std::move(*alt_offset_result);
  m_effect_emitter->set_source_position("alt", alt_offset + position());

  m_effect_emitter->set_direction(facing_direction);

  {
    if (auto result = m_effect_emitter->tick(dt, *entityMode()); !result)
      return std::unexpected(std::move(result.error()));
  }

  if (is_client) {
    bool calculate_head_rotation = isMaster();
    if (!calculate_head_rotation) {
      auto head_rotation_property_result = get_secret_property("humanoid.headRotation");
      if (!head_rotation_property_result)
        return std::unexpected(std::move(head_rotation_property_result.error()));
      auto head_rotation_property = std::move(*head_rotation_property_result);
      if (head_rotation_property.is_type(Json::Type::Float)) {
        auto rotation_result = head_rotation_property.to_float();
        if (!rotation_result)
          return std::unexpected(std::move(rotation_result.error()));
        auto rotation = std::move(*rotation_result);
        humanoid()->set_head_rotation(rotation);
      } else
        calculate_head_rotation = true;
    }
    if (calculate_head_rotation) { // master or not an OpenStarbound player
      float head_rotation = 0.f;
      auto head_rotation_enabled = root().humanoidHeadRotation();
      if (!head_rotation_enabled)
        return std::unexpected(std::move(head_rotation_enabled.error()));
      if (head_rotation_enabled->get() && (humanoid()->hand_holding_item(ToolHand::Primary) || humanoid()->hand_holding_item(ToolHand::Alt) || humanoid()->dance())) {
        auto primary = m_tools->primaryHandItem();
        auto alt = m_tools->altHandItem();
        String const disable_flag = "disableHeadRotation";
        auto status_flag = m_status_controller->statusProperty(disable_flag);
        bool disabled = false;
        if (status_flag.is_type(Json::Type::Bool)) {
          auto flag_result = status_flag.to_bool();
          if (!flag_result)
            return std::unexpected(std::move(flag_result.error()));
          auto flag = std::move(*flag_result);
          disabled = flag;
        }
        if (!disabled && primary) {
          auto flag_result = primary->instance_value(disable_flag);
          if (!flag_result)
            return std::unexpected(std::move(flag_result.error()));
          auto flag = std::move(*flag_result);
          disabled = bool(flag);
        }
        if (!disabled && alt) {
          auto flag_result = alt->instance_value(disable_flag);
          if (!flag_result)
            return std::unexpected(std::move(flag_result.error()));
          auto flag = std::move(*flag_result);
          disabled = bool(flag);
        }
        if (!disabled) {
          auto mouth_result = mouth_position();
          if (!mouth_result)
            return std::unexpected(std::move(mouth_result.error()));
          auto mouth = std::move(*mouth_result);
          auto diff = current_world.geometry().diff(m_aim_position, mouth);
          diff.set_x(std::fabsf(diff.x()));
          head_rotation = diff.angle() * .25f * numerical_direction(humanoid()->facing_direction());
        }
      }
      humanoid()->set_head_rotation(head_rotation);
      if (isMaster())
        {
          if (auto result = set_secret_property("humanoid.headRotation", head_rotation); !result)
            return std::unexpected(std::move(result.error()));
        }
    }
  }
  
  if (isMaster()) {
    for (auto&& p : m_generic_script_contexts) {
      if (auto result = p.second->invoke("postUpdate"); !result)
        return std::unexpected(std::move(result.error()));
    }
  }

  m_pending_moves.clear();

  if (is_client)
    root().spatialLogger().log_poly("world", m_movement_controller->collision_body(),
                                    isMaster() ? Color::palette_orange.to_rgba() : Color::palette_yellow.to_rgba());
  return {};
}

float Player::time_since_last_gave_damage() const {
  return m_last_damaged_other_timer;
}

EntityId Player::last_damaged_target() const {
  return m_last_damaged_target;
}

std::uint64_t Player::presentation_epoch() const {
  return Entity::presentation_epoch() + m_movement_controller->presentation_epoch();
}

std::expected<void, StarException> Player::collect_render_events(RenderCallback& callback) {
  NullRenderCallback discard(m_hidden_loop_audios);
  bool hidden = invisible();
  RenderCallback& render_callback = hidden ? static_cast<RenderCallback&>(discard) : callback;
  if (!hidden && !is_teleporting()) {
    auto bars = m_status_controller->overhead_bars();
    if (!bars)
      return std::unexpected(std::move(bars.error()));
    m_presentation_bars = std::move(*bars);
  } else {
    m_presentation_bars.clear();
  }
  if (!hidden) {
    for (auto& audio : std::exchange(m_hidden_loop_audios, {})) {
      if (!audio->finished()) {
        if (auto result = callback.add_audio(std::move(audio)); !result)
          return std::unexpected(std::move(result.error()));
      }
    }
  }
  Vec2I sensor = Vec2I((m_config->footstep_sensor + m_movement_controller->position()).floor());
  auto sound_result = get_footstep_sound(sensor);
  if (!sound_result)
    return std::unexpected(std::move(sound_result.error()));
  auto sound = std::move(*sound_result);
  if (!sound.empty() && !m_tech_controller->parentState() && !m_tech_controller->parentHidden()) {
    if (m_landing_noise_pending || m_footstep_pending) {
    auto assets_result = root().assets();
    if (!assets_result)
      return std::unexpected(std::move(assets_result.error()));
    auto assets = std::move(*assets_result);
    auto audio_result = assets->audio(sound);
    if (!audio_result)
      return std::unexpected(std::move(audio_result.error()));
    auto audio = std::move(*audio_result);
    if (m_landing_noise_pending) {
      auto noise_result = AudioInstance::create(*audio);
      if (!noise_result)
        return std::unexpected(std::move(noise_result.error()));
      auto noise = std::move(*noise_result);
      auto offset_result = feet_offset();
      if (!offset_result)
        return std::unexpected(std::move(offset_result.error()));
      auto offset = std::move(*offset_result);
      noise->set_position(position() + offset);
      noise->set_volume(m_landing_volume);
      if (auto result = render_callback.add_audio(std::move(noise)); !result)
        return std::unexpected(std::move(result.error()));
    }
    if (m_footstep_pending) {
      auto noise_result = AudioInstance::create(*audio);
      if (!noise_result)
        return std::unexpected(std::move(noise_result.error()));
      auto noise = std::move(*noise_result);
      auto offset_result = feet_offset();
      if (!offset_result)
        return std::unexpected(std::move(offset_result.error()));
      auto offset = std::move(*offset_result);
      noise->set_position(position() + offset);
      auto variance_result = root().random().uniform_float(0, m_footstep_volume_variance);
      if (!variance_result)
        return std::unexpected(std::move(variance_result.error()));
      auto variance = std::move(*variance_result);
      noise->set_volume(1 - variance);
      if (auto result = render_callback.add_audio(std::move(noise)); !result)
        return std::unexpected(std::move(result.error()));
    }
    }
  } else {
    m_footstep_timer = m_config->footstep_timing;
  }
  m_footstep_pending = false;
  m_landing_noise_pending = false;
  if (auto result = render_callback.add_audios(m_effects_animator_dynamic_target.pull_new_audios()); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = render_callback.add_particles(m_effects_animator_dynamic_target.pull_new_particles()); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = render_callback.add_audios(m_tech_controller->pullNewAudios()); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = render_callback.add_audios(m_status_controller->pullNewAudios()); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = render_callback.add_audios(m_humanoid_dynamic_target.pull_new_audios()); !result)
    return std::unexpected(std::move(result.error()));
  for (auto const& pending : std::exchange(m_callback_sounds, {})) {
    auto assets_result = root().assets();
    if (!assets_result)
      return std::unexpected(std::move(assets_result.error()));
    auto assets = std::move(*assets_result);
    auto sample_result = assets->audio(std::get<0>(pending));
    if (!sample_result)
      return std::unexpected(std::move(sample_result.error()));
    auto sample = std::move(*sample_result);
    auto audio_instance = AudioInstance::create(*sample);
    if (!audio_instance)
      return std::unexpected(std::move(audio_instance.error()));
    auto audio = std::move(*audio_instance);
    audio->set_volume(std::get<1>(pending));
    audio->set_pitch_multiplier(std::get<2>(pending));
    audio->set_position(position());
    if (auto result = render_callback.add_audio(std::move(audio)); !result)
      return std::unexpected(std::move(result.error()));
  }
  auto emitted_result = particles();
  if (!emitted_result)
    return std::unexpected(std::move(emitted_result.error()));
  if (auto result = render_callback.add_particles(std::move(*emitted_result)); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = m_tools->collectRenderEvents(render_callback); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = m_effect_emitter->collect_render_events(render_callback); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = m_songbook->collectRenderEvents(render_callback); !result)
    return std::unexpected(std::move(result.error()));
  if (isMaster())
    return m_deployment->collectRenderEvents(render_callback, position());
  return {};
}

std::expected<void, StarException> Player::render(RenderCallback& render_callback) {
  if (invisible())
    return {};
  auto lounge_anchor = as<LoungeAnchor>(m_movement_controller->entity_anchor());
  EntityRenderLayer layer = render_layer_player;
  auto override_layer_result = get_secret_property("overrideRenderLayer");
  if (!override_layer_result)
    return std::unexpected(std::move(override_layer_result.error()));
  auto override_layer = std::move(*override_layer_result);
  if (override_layer.can_convert(Json::Type::Int)) {
    auto value_result = override_layer.to_uint();
    if (!value_result)
      return std::unexpected(std::move(value_result.error()));
    layer = *value_result;
  }
  if (lounge_anchor)
    layer = lounge_anchor->loungeRenderLayer;
  auto rendered_result = drawables(render_callback.presentation().interpolation());
  if (!rendered_result)
    return std::unexpected(std::move(rendered_result.error()));
  if (auto result = render_callback.add_drawables(std::move(*rendered_result), layer); !result)
    return std::unexpected(std::move(result.error()));
  if (!is_teleporting()) {
    if (auto result = render_callback.add_overhead_bars(m_presentation_bars, position()); !result)
      return std::unexpected(std::move(result.error()));
  }
  auto range_result = in_tool_range();
  if (!range_result)
    return std::unexpected(std::move(range_result.error()));
  auto range = std::move(*range_result);
  if (auto result = m_tools->render(render_callback, range, m_shifting, layer); !result)
    return std::unexpected(std::move(result.error()));
  if (isMaster())
    return m_deployment->render(render_callback, position());
  return {};
}

std::expected<void, StarException> Player::render_light_sources(RenderCallback& render_callback) {
  auto lights = presentation_light_sources(render_callback.presentation().interpolation());
  if (!lights)
    return std::unexpected(std::move(lights.error()));
  if (auto result = render_callback.add_light_sources(*lights); !result)
    return std::unexpected(std::move(result.error()));
  return m_deployment->render_light_sources(render_callback);
}

std::expected<void, StarException> Player::set_render_layer(std::optional<EntityRenderLayer> layer) {
  return set_secret_property("overrideRenderLayer", layer ? Json(*layer) : Json());
}

Json Player::get_generic_property(String const& name, Json const& default_value) const {
  return m_generic_properties.value(name, default_value);
}

void Player::set_generic_property(String const& name, Json const& value) {
  if (value.is_null())
    m_generic_properties.erase(name);
  else
    m_generic_properties.set(name, value);
}

std::shared_ptr<PlayerInventory> Player::inventory() const {
  return m_inventory;
}

std::expected<std::uint64_t, StarException> Player::items_can_hold(std::shared_ptr<Item> const& items) const {
  return m_inventory->itemsCanFit(items);
}

std::expected<std::shared_ptr<Item>, StarException> Player::pickup_items(std::shared_ptr<Item> const& items, bool silent) {
  if (is_dead() || !items)
    return items;
  auto capacity_result = m_inventory->itemsCanFit(items);
  if (!capacity_result)
    return std::unexpected(std::move(capacity_result.error()));
  auto capacity = std::move(*capacity_result);
  if (capacity == 0)
    return items;
  if (auto result = trigger_pickup_events(items); !result)
    return std::unexpected(std::move(result.error()));
  if (!silent) {
    auto sound_result = items->pickup_sound();
    if (!sound_result)
      return std::unexpected(std::move(sound_result.error()));
    auto sound = std::move(*sound_result);
    if (!sound.empty()) {
      if (auto result = m_effects_animator->set_sound_pool("pickup", {sound}); !result)
        return std::unexpected(std::move(result.error()));
      float pitch = 1.f - (static_cast<float>(items->count()) / static_cast<float>(items->max_stack())) * 0.5f;
      auto variance_result = root().random().uniform_float(0.8f, 1.2f);
      if (!variance_result)
        return std::unexpected(std::move(variance_result.error()));
      auto variance = std::move(*variance_result);
      if (auto result = m_effects_animator->set_sound_pitch_multiplier("pickup", std::clamp(pitch * variance, 0.f, 2.f));
          !result)
        return std::unexpected(std::move(result.error()));
      if (auto result = m_effects_animator->play_sound("pickup"); !result)
        return std::unexpected(std::move(result.error()));
    }
    auto database_result = root().itemDatabase();
    if (!database_result)
      return std::unexpected(std::move(database_result.error()));
    auto database = std::move(*database_result);
    auto shared_result = database->item_shared(items->descriptor());
    if (!shared_result)
      return std::unexpected(std::move(shared_result.error()));
    auto shared = std::move(*shared_result);
    queue_item_pickup_message(shared);
  }
  return m_inventory->addItems(items);
}

std::expected<void, StarException> Player::give_item(std::shared_ptr<Item> const& item) {
  auto spill_result = pickup_items(item, false);
  if (!spill_result)
    return std::unexpected(std::move(spill_result.error()));
  auto spill = std::move(*spill_result);
  if (spill) {
    auto current_world_result = world();
    if (!current_world_result)
      return std::unexpected(std::move(current_world_result.error()));
    auto& current_world = current_world_result->get();
    auto drop_result = ItemDrop::create_randomized_drop(root(), spill->descriptor(), position());
    if (!drop_result)
      return std::unexpected(std::move(drop_result.error()));
    auto drop = std::move(*drop_result);
    return current_world.addEntity(std::move(drop));
  }
  return {};
}

std::expected<void, StarException> Player::trigger_pickup_events(std::shared_ptr<Item> const& source) {
  // Blueprint and collection lookups can re-enter item builders; hold the pickup item across them.
  auto const item = source;
  if (!item)
    return {};
  for (auto const& blueprint : item->learn_blueprints_on_pickup()) {
    if (auto result = add_blueprint(blueprint); !result)
      return std::unexpected(std::move(result.error()));
  }
  for (auto const& collectable : item->collectables_on_pickup()) {
    if (auto result = add_collectable(collectable.first, collectable.second); !result)
      return std::unexpected(std::move(result.error()));
  }
  auto messages_result = item->instance_value("radioMessagesOnPickup", JsonArray());
  if (!messages_result)
    return std::unexpected(std::move(messages_result.error()));
  auto messages = std::move(*messages_result);
  auto message_array_result = messages.to_array();
  if (!message_array_result)
    return std::unexpected(std::move(message_array_result.error()));
  auto message_array = std::move(*message_array_result);
  for (auto const& message : message_array) {
    if (message.is_type(Json::Type::Array)) {
      auto parts_result = message.to_array();
      if (!parts_result)
        return std::unexpected(std::move(parts_result.error()));
      auto parts = std::move(*parts_result);
      if (parts.size() >= 2 && parts[1].can_convert(Json::Type::Float)) {
        auto delay_result = parts[1].to_float();
        if (!delay_result)
          return std::unexpected(std::move(delay_result.error()));
        auto delay = std::move(*delay_result);
        if (auto result = queue_radio_message(parts[0], delay); !result)
          return std::unexpected(std::move(result.error()));
      }
    } else {
      if (auto result = queue_radio_message(message); !result)
        return std::unexpected(std::move(result.error()));
    }
  }
  auto cinematic_result = item->instance_value("cinematicOnPickup", Json());
  if (!cinematic_result)
    return std::unexpected(std::move(cinematic_result.error()));
  auto cinematic = std::move(*cinematic_result);
  if (cinematic) {
    if (auto result = set_pending_cinematic(cinematic, true); !result)
      return std::unexpected(std::move(result.error()));
  }
  auto quests_result = item->pickup_quest_templates();
  if (!quests_result)
    return std::unexpected(std::move(quests_result.error()));
  auto quests = std::move(*quests_result);
  for (auto const& arc : quests) {
    auto can_start_result = m_quest_manager->canStart(arc);
    if (!can_start_result)
      return std::unexpected(std::move(can_start_result.error()));
    auto can_start = std::move(*can_start_result);
    if (can_start) {
      auto quest_result = Quest::create(root(), arc, 0, *this);
      if (!quest_result)
        return std::unexpected(std::move(quest_result.error()));
      auto quest = std::move(*quest_result);
      if (auto result = m_quest_manager->offer(quest); !result)
        return std::unexpected(std::move(result.error()));
    }
  }
  auto consume_result = item->instance_value("consumeOnPickup", Json());
  if (!consume_result)
    return std::unexpected(std::move(consume_result.error()));
  auto consume = std::move(*consume_result);
  if (consume) {
    auto enabled_result = consume.to_bool();
    if (!enabled_result)
      return std::unexpected(std::move(enabled_result.error()));
    auto enabled = std::move(*enabled_result);
    if (enabled)
      item->consume(item->count());
  }
  auto category_result = item->instance_value("eventCategory", item->category());
  if (!category_result)
    return std::unexpected(std::move(category_result.error()));
  auto category = std::move(*category_result);
  statistics()->recordEvent("item", JsonObject{{"itemName", item->name()}, {"count", item->count()}, {"category", std::move(category)}});
  return {};
}

std::shared_ptr<Item> Player::essential_item(EssentialItem essential_item) const {
  return m_inventory->essentialItem(essential_item);
}

bool Player::has_item(ItemDescriptor const& descriptor, bool exact_match) const {
  return m_inventory->hasItem(descriptor, exact_match);
}

std::uint64_t Player::has_count_of_item(ItemDescriptor const& descriptor, bool exact_match) const {
  return m_inventory->hasCountOfItem(descriptor, exact_match);
}

std::expected<ItemDescriptor, StarException> Player::take_item(ItemDescriptor const& descriptor, bool consume_partial, bool exact_match) {
  return m_inventory->takeItems(descriptor, consume_partial, exact_match);
}

std::expected<void, StarException> Player::give_item(ItemDescriptor const& descriptor) {
  auto database_result = root().itemDatabase();
  if (!database_result)
    return std::unexpected(std::move(database_result.error()));
  auto database = std::move(*database_result);
  auto item_result = database->item(descriptor);
  if (!item_result)
    return std::unexpected(std::move(item_result.error()));
  auto item = std::move(*item_result);
  return give_item(item);
}

std::expected<void, StarException> Player::clear_swap() {
  auto cleared_result = m_inventory->clearSwap();
  if (!cleared_result)
    return std::unexpected(std::move(cleared_result.error()));
  auto cleared = std::move(*cleared_result);
  if (!cleared) {
    if (auto current_world = maybeWorld()) {
      auto item_result = m_inventory->takeSlot(SwapSlot());
      if (!item_result)
        return std::unexpected(std::move(item_result.error()));
      auto item = std::move(*item_result);
      auto drop_result = ItemDrop::create_randomized_drop(root(), item, position());
      if (!drop_result)
        return std::unexpected(std::move(drop_result.error()));
      auto drop = std::move(*drop_result);
      if (auto result = current_world->get().addEntity(std::move(drop)); !result)
        return std::unexpected(std::move(result.error()));
    }
  }
  end_primary_fire();
  end_alt_fire();
  end_trigger();
  return {};
}

std::expected<void, StarException> Player::refresh_items() {
  if (isSlave())
    return {};
  auto primary_result = m_inventory->primary_held_item();
  if (!primary_result)
    return std::unexpected(std::move(primary_result.error()));
  auto primary = std::move(*primary_result);
  auto secondary_result = m_inventory->secondary_held_item();
  if (!secondary_result)
    return std::unexpected(std::move(secondary_result.error()));
  auto secondary = std::move(*secondary_result);
  return m_tools->setItems(std::move(primary), std::move(secondary));
}

std::expected<void, StarException> Player::refresh_armor() {
  if (isSlave())
    return {};

  bool should_set_armor_secrets = m_client_context && m_client_context->net_compatibility_rules().version() < 9;
  for (std::uint8_t i = 0; i != 20; ++i) {
    auto slot = static_cast<EquipmentSlot>(i);
    auto item = m_inventory->equipment(slot);
    bool visible = m_inventory->equipmentVisibility(slot);
    if (m_armor->set_item(i, item, visible)) {
      if (slot >= EquipmentSlot::Cosmetic1 && should_set_armor_secrets)
        {
          if (auto result = set_net_armor_secret(slot, item, visible); !result)
            return std::unexpected(std::move(result.error()));
        }
    }
  }
  return {};
}

std::expected<void, StarException> Player::refresh_humanoid() const {
  auto refreshed = [&]() -> std::expected<void, StarException> {
    auto changed_result = m_armor->setup_humanoid(*humanoid(), force_nude());
    if (!changed_result)
      return std::unexpected(std::move(changed_result.error()));
    auto changed = std::move(*changed_result);
    if (changed) {
      auto parameters_result = ActorMovementParameters::create(json_merge(humanoid()->default_movement_parameters(), humanoid()->player_movement_parameters().value_or(m_config->movement_parameters)));
      if (!parameters_result)
        return std::unexpected(std::move(parameters_result.error()));
      auto parameters = std::move(*parameters_result);
      return m_movement_controller->reset_base_parameters(parameters);
    }
    return {};
  }();
  if (!refreshed && isMaster())
    return std::unexpected(std::move(refreshed.error()));
  return {};
}

std::expected<void, StarException> Player::refresh_equipment() {
  {
    if (auto result = refresh_armor(); !result)
      return std::unexpected(std::move(result.error()));
  }
  {
    if (auto result = refresh_items(); !result)
      return std::unexpected(std::move(result.error()));
  }
  return {};
}

std::shared_ptr<PlayerBlueprints> Player::blueprints() const {
  return m_blueprints;
}

std::expected<bool, StarException> Player::add_blueprint(ItemDescriptor const& descriptor, bool show_failure) {
  if (descriptor.is_null())
    return false;
  bool const known = m_blueprints->isKnown(descriptor);
  if (known && !show_failure)
    return false;
  auto database_result = root().itemDatabase();
  if (!database_result)
    return std::unexpected(std::move(database_result.error()));
  auto database = std::move(*database_result);
  auto item_result = database->item(descriptor);
  if (!item_result)
    return std::unexpected(std::move(item_result.error()));
  auto item = std::move(*item_result);
  auto assets_result = root().assets();
  if (!assets_result)
    return std::unexpected(std::move(assets_result.error()));
  auto assets = std::move(*assets_result);
  if (!known) {
    m_blueprints->add(descriptor);
    auto message_result = assets->json("/player.config:blueprintUnlock");
    if (!message_result)
      return std::unexpected(std::move(message_result.error()));
    auto message = std::move(*message_result);
    auto text_result = message.to_string();
    if (!text_result)
      return std::unexpected(std::move(text_result.error()));
    auto text = std::move(*text_result);
    auto formatted_result = text.replace("<ItemName>", item->friendly_name());
    if (!formatted_result)
      return std::unexpected(std::move(formatted_result.error()));
    auto formatted = std::move(*formatted_result);
    if (auto result = queue_ui_message(formatted); !result)
      return std::unexpected(std::move(result.error()));
    return true;
  }
  if (show_failure) {
    auto message_result = assets->json("/player.config:blueprintAlreadyKnown");
    if (!message_result)
      return std::unexpected(std::move(message_result.error()));
    auto message = std::move(*message_result);
    auto text_result = message.to_string();
    if (!text_result)
      return std::unexpected(std::move(text_result.error()));
    auto text = std::move(*text_result);
    auto formatted_result = text.replace("<ItemName>", item->friendly_name());
    if (!formatted_result)
      return std::unexpected(std::move(formatted_result.error()));
    auto formatted = std::move(*formatted_result);
    if (auto result = queue_ui_message(formatted); !result)
      return std::unexpected(std::move(result.error()));
  }
  return false;
}

bool Player::blueprint_known(ItemDescriptor const& descriptor) const {
  if (descriptor.is_null())
    return false;

  return m_blueprints->isKnown(descriptor);
}

std::expected<bool, StarException> Player::add_collectable(String const& collection_name, String const& collectable_name) {
  if (m_log->has_collectable(collection_name, collectable_name))
    return false;
  auto database = root().collection_database();
  if (!database) return std::unexpected(std::move(database.error()));
  auto collection = (*database)->collection(collection_name);
  if (!collection) return std::unexpected(std::move(collection.error()));
  auto entry = (*database)->collectable(collection_name, collectable_name);
  if (!entry) return std::unexpected(std::move(entry.error()));
  auto assets = root().assets();
  if (!assets) return std::unexpected(std::move(assets.error()));
  auto message = (*assets)->json("/player.config:collectableUnlock")
      .and_then([](Json const& value) { return value.to_string(); })
      .and_then([&](String const& text) { return text.replace("<collectable>", entry->title); })
      .and_then([&](String const& text) { return text.replace("<collection>", collection->title); });
  if (!message) return std::unexpected(std::move(message.error()));
  if (!m_log->add_collectable(collection_name, collectable_name))
    return false;
  if (auto queued = queue_ui_message(*message); !queued) return std::unexpected(std::move(queued.error()));
  return true;
}

std::shared_ptr<PlayerUniverseMap> Player::universe_map() const {
  return m_universe_map;
}

std::shared_ptr<PlayerCodexes> const& Player::codexes() const& {
  return m_codexes;
}

std::shared_ptr<PlayerTech> Player::techs() const {
  return m_techs;
}

std::expected<void, StarException> Player::override_tech(std::optional<StringList> const& tech_modules) {
  if (tech_modules)
    {
      if (auto result = m_tech_controller->set_override_tech(*tech_modules); !result)
        return std::unexpected(std::move(result.error()));
    }
  else
    {
      if (auto result = m_tech_controller->clear_override_tech(); !result)
        return std::unexpected(std::move(result.error()));
    }
  return {};
}

bool Player::tech_overridden() const {
  return m_tech_controller->tech_overridden();
}

std::shared_ptr<PlayerCompanions> Player::companions() const {
  return m_companions;
}

std::shared_ptr<PlayerLog> Player::log() const {
  return m_log;
}

std::expected<std::shared_ptr<InteractiveEntity>, StarException> Player::best_interaction_entity(bool include_nearby) {
  if (!inWorld())
    return std::shared_ptr<InteractiveEntity>{};
  auto current_world_result = world();
  if (!current_world_result)
    return std::unexpected(std::move(current_world_result.error()));
  auto& current_world = current_world_result->get();
  auto entity_result = current_world.getInteractiveInRange(m_aim_position, is_admin() ? m_aim_position : position(), m_interact_radius);
  if (!entity_result)
    return std::unexpected(std::move(entity_result.error()));
  auto entity = std::move(*entity_result);
  if (!entity && include_nearby) {
    Vec2F bias = m_walk_into_interact_bias;
    if (facing_direction() == Direction::Left)
      bias[0] *= -1;
    auto nearby_result = current_world.getInteractiveInRange(position() + bias, position(), m_interact_radius);
    if (!nearby_result)
      return std::unexpected(std::move(nearby_result.error()));
    auto nearby = std::move(*nearby_result);
    entity = std::move(nearby);
  }
  if (!entity)
    return std::shared_ptr<InteractiveEntity>{};
  if (is_admin())
    return entity;
  auto reachable_result = current_world.canReachEntity(position(), m_interact_radius, entity->entityId());
  if (!reachable_result)
    return std::unexpected(std::move(reachable_result.error()));
  auto reachable = std::move(*reachable_result);
  return reachable ? entity : std::shared_ptr<InteractiveEntity>{};
}

std::expected<void, StarException> Player::dispatch_use_interaction(std::shared_ptr<InteractiveEntity> const& entity) {
  bool quest_intercepted = false;
  for (auto const& quest : m_quest_manager->list_active_quests()) {
    // A quest can act even if its interaction hook declines or returns an error.
    m_use_edge_ready = false;
    auto intercepted = quest->interact_with_entity(entity->entityId());
    if (!intercepted)
      return std::unexpected(std::move(intercepted.error()));
    if (*intercepted)
      quest_intercepted = true;
  }
  if (quest_intercepted)
    return {};
  bool any_turned_in = false;
  auto turn_ins = entity->turn_in_quests();
  if (!turn_ins)
    return std::unexpected(std::move(turn_ins.error()));
  for (auto const& quest_id : *turn_ins) {
    if (m_quest_manager->canTurnIn(quest_id)) {
      auto quest = m_quest_manager->getQuest(quest_id);
      if (!quest)
        return std::unexpected(std::move(quest.error()));
      if (auto result = (*quest)->set_entity_parameter("questReceiver", entity); !result)
        return std::unexpected(std::move(result.error()));
      m_use_edge_ready = false;
      if (auto result = (*quest)->complete(); !result)
        return std::unexpected(std::move(result.error()));
      any_turned_in = true;
    }
  }
  if (any_turned_in)
    return {};
  auto offers = entity->offered_quests();
  if (!offers)
    return std::unexpected(std::move(offers.error()));
  for (auto const& arc : *offers) {
    auto can_start = m_quest_manager->canStart(arc);
    if (!can_start)
      return std::unexpected(std::move(can_start.error()));
    if (*can_start) {
      auto quest = Quest::create(root(), arc, 0, *this);
      if (!quest)
        return std::unexpected(std::move(quest.error()));
      (*quest)->setWorldId(client_context()->player_world_id());
      (*quest)->setServerUuid(client_context()->server_uuid());
      if (auto result = (*quest)->set_entity_parameter("questGiver", entity); !result)
        return std::unexpected(std::move(result.error()));
      m_use_edge_ready = false;
      return m_quest_manager->offer(*quest);
    }
  }
  auto current_world = world();
  if (!current_world)
    return std::unexpected(std::move(current_world.error()));
  // The world may deliver synchronously and return a handler error afterward.
  m_use_edge_ready = false;
  auto action = current_world->get().interact(InteractRequest{entityId(), position(), entity->entityId(), m_aim_position});
  if (!action)
    return std::unexpected(std::move(action.error()));
  queue_interact_action(std::move(*action));
  return {};
}

void Player::aim(Vec2F const& position) {
  m_tech_controller->setAimPosition(position);
  m_aim_position = position;
}

std::expected<Vec2F, StarException> Player::aim_position() const {
  return m_aim_position;
}

std::expected<Vec2F, StarException> Player::arm_position(ToolHand hand, Direction facing_direction, float arm_angle, Vec2F offset) const {
  return humanoid()->arm_position(hand, facing_direction, arm_angle, offset);
}

std::expected<Vec2F, StarException> Player::hand_offset(ToolHand hand, Direction facing_direction) const {
  return humanoid()->hand_offset(hand, facing_direction);
}

std::expected<Vec2F, StarException> Player::hand_position(ToolHand hand, Vec2F const& hand_offset) const {
  return humanoid()->hand_position(hand, hand_offset);
}

std::shared_ptr<Item> Player::hand_item(ToolHand hand) const {
  if (hand == ToolHand::Primary)
    return m_tools->primaryHandItem();
  else
    return m_tools->altHandItem();
}

std::expected<Vec2F, StarException> Player::arm_adjustment() const {
  return humanoid()->arm_adjustment();
}

void Player::set_camera_focus_entity(std::optional<EntityId> const& camera_focus_entity) {
  m_camera_focus_entity = camera_focus_entity;
}

std::expected<void, StarException> Player::play_emote(HumanoidEmote emote) {
  add_emote(emote);
  return {};
}

bool Player::can_use_tool() const {
  bool can_use = !is_dead() && !is_teleporting() && !m_tech_controller->toolUsageSuppressed() && !m_status_controller->toolUsageSuppressed();
  if (can_use) {
    if (auto lounge_anchor = as<LoungeAnchor>(m_movement_controller->entity_anchor()))
      if (lounge_anchor->suppressTools.value_or(lounge_anchor->controllable))
        return false;
  }
  return can_use;
}

void Player::begin_primary_fire() {
  m_tech_controller->beginPrimaryFire();
  m_tools->beginPrimaryFire();
}

void Player::begin_alt_fire() {
  m_tech_controller->beginAltFire();
  m_tools->beginAltFire();
}

void Player::end_primary_fire() {
  m_tech_controller->endPrimaryFire();
  m_tools->endPrimaryFire();
}

void Player::end_alt_fire() {
  m_tech_controller->endAltFire();
  m_tools->endAltFire();
}

void Player::begin_trigger() {
  if (!m_use_down)
    m_use_edge_pending = true;
  m_use_down = true;
}

void Player::end_trigger() {
  m_use_down = false;
}

std::expected<float, StarException> Player::tool_radius() const {
  auto radius_result = m_tools->toolRadius();
  if (!radius_result)
    return std::unexpected(std::move(radius_result.error()));
  auto radius = std::move(*radius_result);
  if (radius)
    return *radius;
  return m_interact_radius;
}

std::expected<float, StarException> Player::interact_radius() const {
  return m_interact_radius;
}

void Player::set_interact_radius(float interact_radius) {
  m_interact_radius = interact_radius;
}

void Player::queue_interact_action(RpcPromise<InteractAction> action) {
  m_pending_interact_actions.push_back(std::move(action));
}

std::expected<List<InteractAction>, StarException> Player::pull_interact_actions() {
  auto& queue = m_pending_interact_actions;
  auto const count = queue.size();
  auto first_finished = count;
  List<std::size_t> pending_after_gap;
  List<InteractAction> actions;
  for (std::size_t i = 0; i < count; ++i) {
    auto status = queue[i].status();
    if (!status)
      return std::unexpected(std::move(status.error()));
    if (*status == RpcPromiseStatus::Pending) {
      if (first_finished != count)
        pending_after_gap.push_back(i);
      continue;
    }
    if (first_finished == count)
      first_finished = i;
    if (*status == RpcPromiseStatus::Succeeded) {
      // Success is terminal. Read the shared value without consuming it for other observers.
      auto action = queue[i].result();
      if (!action)
        return std::unexpected(std::move(action.error()));
      actions.push_back(std::move(**action));
    }
  }

  // Nothing is removed until every query succeeded. Keep the pending prefix in place.
  if (first_finished != count) {
    auto write = first_finished;
    for (auto read : pending_after_gap)
      queue[write++] = std::move(queue[read]);
    queue.erase(queue.begin() + write, queue.begin() + count);
  }
  return actions;
}

std::uint64_t Player::currency(String const& currency_type) const {
  return m_inventory->currency(currency_type);
}

float Player::health() const {
  return m_status_controller->resource("health");
}

std::expected<float, StarException> Player::max_health() const {
  auto value_result = m_status_controller->resourceMax("health");
  if (!value_result)
    return std::unexpected(std::move(value_result.error()));
  auto value = std::move(*value_result);
  if (!value)
    return std::unexpected(PlayerException("Missing required player resource 'health'"));
  return *value;
}

DamageBarType Player::damage_bar() const {
  return DamageBarType::Default;
}

std::expected<float, StarException> Player::health_percentage() const {
  auto value_result = m_status_controller->resourcePercentage("health");
  if (!value_result)
    return std::unexpected(std::move(value_result.error()));
  auto value = std::move(*value_result);
  if (!value)
    return std::unexpected(PlayerException("Missing required player resource 'health'"));
  return *value;
}

std::expected<float, StarException> Player::energy() const {
  return m_status_controller->resource("energy");
}

std::expected<float, StarException> Player::max_energy() const {
  auto value_result = m_status_controller->resourceMax("energy");
  if (!value_result)
    return std::unexpected(std::move(value_result.error()));
  auto value = std::move(*value_result);
  if (!value)
    return std::unexpected(PlayerException("Missing required player resource 'energy'"));
  return *value;
}

std::expected<float, StarException> Player::energy_percentage() const {
  auto value_result = m_status_controller->resourcePercentage("energy");
  if (!value_result)
    return std::unexpected(std::move(value_result.error()));
  auto value = std::move(*value_result);
  if (!value)
    return std::unexpected(PlayerException("Missing required player resource 'energy'"));
  return *value;
}

std::expected<float, StarException> Player::energy_regen_block_percent() const {
  auto value_result = m_status_controller->resourcePercentage("energyRegenBlock");
  if (!value_result)
    return std::unexpected(std::move(value_result.error()));
  auto value = std::move(*value_result);
  if (!value)
    return std::unexpected(PlayerException("Missing required player resource 'energyRegenBlock'"));
  return *value;
}

std::expected<bool, StarException> Player::full_energy() const {
  auto maximum_result = max_energy();
  if (!maximum_result)
    return std::unexpected(std::move(maximum_result.error()));
  auto maximum = std::move(*maximum_result);
  return m_status_controller->resource("energy") >= maximum;
}

std::expected<bool, StarException> Player::energy_locked() const {
  return m_status_controller->resourceLocked("energy");
}

std::expected<bool, StarException> Player::consume_energy(float energy) {
  if (m_is_admin)
    return true;
  return m_status_controller->overConsumeResource("energy", energy);
}

std::expected<float, StarException> Player::food_percentage() const {
  auto value_result = m_status_controller->resourcePercentage("food");
  if (!value_result)
    return std::unexpected(std::move(value_result.error()));
  auto value = std::move(*value_result);
  if (!value)
    return std::unexpected(PlayerException("Missing required player resource 'food'"));
  return *value;
}

float Player::breath() const {
  return m_status_controller->resource("breath");
}

std::expected<float, StarException> Player::max_breath() const {
  auto value_result = m_status_controller->resourceMax("breath");
  if (!value_result)
    return std::unexpected(std::move(value_result.error()));
  auto value = std::move(*value_result);
  if (!value)
    return std::unexpected(PlayerException("Missing required player resource 'breath'"));
  return *value;
}

float Player::protection() const {
  return m_status_controller->stat("protection");
}

bool Player::force_nude() const {
  return m_status_controller->statPositive("nude");
}

std::expected<String, StarException> Player::description() const {
  return m_description;
}

void Player::set_description(String const& description) {
  m_description = description;
}

Direction Player::walking_direction() const {
  return m_movement_controller->moving_direction();
}

Direction Player::facing_direction() const {
  return m_movement_controller->facing_direction();
}

std::expected<std::pair<ByteArray, std::uint64_t>, StarException> Player::write_net_state(std::uint64_t from_version, NetCompatibilityRules rules) {
  return m_net_group.write_net_state(from_version, rules);
}

std::expected<void, StarException> Player::read_net_state(ByteArray const& data, float interpolation_time, NetCompatibilityRules rules) {
  return m_net_group.read_net_state(data, interpolation_time, rules);
}

std::expected<void, StarException> Player::enable_interpolation(float) {
  return m_net_group.enable_net_interpolation();
}

std::expected<void, StarException> Player::disable_interpolation() {
  return m_net_group.disable_net_interpolation();
}

std::expected<void, StarException> Player::process_controls() {
  auto current_world_result = world();
  if (!current_world_result)
    return std::unexpected(std::move(current_world_result.error()));
  auto& current_world = current_world_result->get();
  bool run = !m_shifting && !m_status_controller->statPositive("encumberance");

  bool use_move_vector = m_move_vector.x() != 0.0f;
  if (use_move_vector) {
    for (auto move : m_pending_moves) {
      if (move == MoveControlType::Left || move == MoveControlType::Right) {
        use_move_vector = false;
        break;
      }
    }
  }

  if (use_move_vector) {
    m_pending_moves.insert(m_move_vector.x() < 0.0f ? MoveControlType::Left : MoveControlType::Right);
    m_movement_controller->set_move_speed_multiplier(std::clamp(std::abs(m_move_vector.x()), 0.0f, 1.0f));
  }
  else
    m_movement_controller->set_move_speed_multiplier(1.0f);

  if (auto fireable_main = as<FireableItem>(m_tools->primaryHandItem())) {
    if (fireable_main->inUse() && fireable_main->walkWhileFiring())
      run = false;
  }

  if (auto fireable_alt = as<FireableItem>(m_tools->altHandItem())) {
    if (fireable_alt->inUse() && fireable_alt->walkWhileFiring())
      run = false;
  }

  bool move = true;

  if (auto fireable_main = as<FireableItem>(m_tools->primaryHandItem())) {
    if (fireable_main->inUse() && fireable_main->stopWhileFiring())
      move = false;
  }

  if (auto fireable_alt = as<FireableItem>(m_tools->altHandItem())) {
    if (fireable_alt->inUse() && fireable_alt->stopWhileFiring())
      move = false;
  }

  auto lounge_anchor = as<LoungeAnchor>(m_movement_controller->entity_anchor());
  if (lounge_anchor && lounge_anchor->controllable) {
    auto anchor_state = m_movement_controller->anchor_state();
    if (!anchor_state)
      return std::unexpected(PlayerException("Player lounge anchor has no anchor state"));
    if (auto loungeable_entity = current_world.get<LoungeableEntity>(anchor_state->entityId)) {
      for (auto move : m_pending_moves) {
        if (move == MoveControlType::Up)
          {
            if (auto result = loungeable_entity->loungeControl(anchor_state->positionIndex, LoungeControl::Up); !result)
              return std::unexpected(std::move(result.error()));
          }
        else if (move == MoveControlType::Down)
          {
            if (auto result = loungeable_entity->loungeControl(anchor_state->positionIndex, LoungeControl::Down); !result)
              return std::unexpected(std::move(result.error()));
          }
        else if (move == MoveControlType::Left)
          {
            if (auto result = loungeable_entity->loungeControl(anchor_state->positionIndex, LoungeControl::Left); !result)
              return std::unexpected(std::move(result.error()));
          }
        else if (move == MoveControlType::Right)
          {
            if (auto result = loungeable_entity->loungeControl(anchor_state->positionIndex, LoungeControl::Right); !result)
              return std::unexpected(std::move(result.error()));
          }
        else if (move == MoveControlType::Jump)
          {
            if (auto result = loungeable_entity->loungeControl(anchor_state->positionIndex, LoungeControl::Jump); !result)
              return std::unexpected(std::move(result.error()));
          }
      }
      if (m_tools->firingPrimary())
        {
          if (auto result = loungeable_entity->loungeControl(anchor_state->positionIndex, LoungeControl::PrimaryFire); !result)
            return std::unexpected(std::move(result.error()));
        }
      if (m_tools->firingAlt())
        {
          if (auto result = loungeable_entity->loungeControl(anchor_state->positionIndex, LoungeControl::AltFire); !result)
            return std::unexpected(std::move(result.error()));
        }
      if (m_shifting)
        {
          if (auto result = loungeable_entity->loungeControl(anchor_state->positionIndex, LoungeControl::Walk); !result)
            return std::unexpected(std::move(result.error()));
        }
      {
        if (auto result = loungeable_entity->loungeAim(anchor_state->positionIndex, m_aim_position); !result)
          return std::unexpected(std::move(result.error()));
      }
    }
    move = false;
  }

  m_tech_controller->setShouldRun(run);

  if (move) {
    for (auto move : m_pending_moves) {
      switch (move) {
        case MoveControlType::Right:
          m_tech_controller->moveRight();
          break;
        case MoveControlType::Left:
          m_tech_controller->moveLeft();
          break;
        case MoveControlType::Up:
          m_tech_controller->moveUp();
          break;
        case MoveControlType::Down:
          m_tech_controller->moveDown();
          break;
        case MoveControlType::Jump:
          m_tech_controller->jump();
          break;
      }
    }
  }

  if (m_state == State::Lounge && !m_pending_moves.empty() && move)
    {
      if (auto result = stop_lounging(); !result)
        return std::unexpected(std::move(result.error()));
    }
  return {};
}

std::expected<void, StarException> Player::control_movement_state(std::optional<String> const& state) {
  if (!state) {
    m_control_movement_state.reset();
    return {};
  }
  Humanoid::State parsed;
  if (*state == "idle")
    parsed = Humanoid::Idle;
  else if (*state == "walk")
    parsed = Humanoid::Walk;
  else if (*state == "run")
    parsed = Humanoid::Run;
  else if (*state == "jump")
    parsed = Humanoid::Jump;
  else if (*state == "fall")
    parsed = Humanoid::Fall;
  else if (*state == "duck")
    parsed = Humanoid::Duck;
  else
    return std::unexpected(PlayerException("Movement state must be idle, walk, run, jump, fall, duck, or nil"));
  if (inWorld() && isMaster())
    m_control_movement_state = parsed;
  return {};
}

void Player::update_movement_animation_state() {
  m_movement_animation_state = isMaster() ? m_control_movement_state : std::nullopt;
  if (is_dead() || is_teleporting() || m_state == State::Lounge
      || m_movement_controller->entity_anchor() || m_tech_controller->parentState())
    m_movement_animation_state.reset();
}

std::expected<void, StarException> Player::process_state_changes(float dt) {
  auto current_world_result = world();
  if (!current_world_result)
    return std::unexpected(std::move(current_world_result.error()));
  auto& current_world = current_world_result->get();
  if (isMaster()) {

    // Set the current player state based on what movement controller tells us
    // we're doing and do some state transition logic
    State old_state = m_state;

    if (m_movement_controller->zero_g()) {
      if (m_movement_controller->flying())
        m_state = State::Swim;
      else if (m_state != State::Lounge)
        m_state = State::SwimIdle;
    } else if (m_movement_controller->ground_movement()) {
      if (m_movement_controller->running()) {
        m_state = State::Run;
      } else if (m_movement_controller->walking()) {
        m_state = State::Walk;
      } else if (m_movement_controller->crouching()) {
        m_state = State::Crouch;
      } else {
        if (m_state != State::Lounge)
          m_state = State::Idle;
      }
    } else if (m_movement_controller->liquid_movement()) {
      if (m_movement_controller->jumping()) {
        m_state = State::Swim;
      } else {
        if (m_state != State::Lounge)
          m_state = State::SwimIdle;
      }
    } else {
      if (m_movement_controller->jumping()) {
        m_state = State::Jump;
      } else {
        if (m_movement_controller->falling()) {
          m_state = State::Fall;
        }
        if (m_movement_controller->velocity()[1] > 0) {
          if (m_state != State::Lounge)
            m_state = State::Jump;
        }
      }
    }

    if (m_move_vector.x() != 0.0f && (m_state == State::Run))
        m_state = std::abs(m_move_vector.x()) > 0.5f ? State::Run : State::Walk;

    if (m_state == State::Jump && (old_state == State::Idle || old_state == State::Run || old_state == State::Walk || old_state == State::Crouch))
      {
        if (auto result = m_effects_animator->burst_particle_emitter("jump"); !result)
          return std::unexpected(std::move(result.error()));
      }

    if (!m_movement_controller->is_null_colliding()) {
      if (old_state == State::Fall && old_state != m_state && m_state != State::Swim && m_state != State::SwimIdle
          && m_state != State::Jump) {
        {
          if (auto result = m_effects_animator->burst_particle_emitter("landing"); !result)
            return std::unexpected(std::move(result.error()));
        }
        m_landed_net_state.trigger();
        m_landing_noise_pending = true;
      }
    }
  }
  if (auto result = update_humanoid_directives(); !result)
    return std::unexpected(std::move(result.error()));
  if (current_world.isClient()) {
    {
      if (auto result = humanoid()->animate(dt, std::ref(m_humanoid_dynamic_target)); !result)
        return std::unexpected(std::move(result.error()));
    }
    m_humanoid_dynamic_target.update_position(position() + (m_tech_controller->parentOffset()));
  } else {
    {
      if (auto result = humanoid()->animate(dt, {}); !result)
        return std::unexpected(std::move(result.error()));
    }
  }
  if (auto result = recover_lua_script_error(m_scripted_animator.update()); !result)
    return std::unexpected(std::move(result.error()));

  if (auto tech_state = m_tech_controller->parentState()) {
    if (tech_state == TechController::ParentState::Stand) {
      {
        if (auto result = humanoid()->set_state(Humanoid::Idle); !result)
          return std::unexpected(std::move(result.error()));
      }
    } else if (tech_state == TechController::ParentState::Fly) {
      {
        if (auto result = humanoid()->set_state(Humanoid::Jump); !result)
          return std::unexpected(std::move(result.error()));
      }
    } else if (tech_state == TechController::ParentState::Fall) {
      {
        if (auto result = humanoid()->set_state(Humanoid::Fall); !result)
          return std::unexpected(std::move(result.error()));
      }
    } else if (tech_state == TechController::ParentState::Sit) {
      {
        if (auto result = humanoid()->set_state(Humanoid::Sit); !result)
          return std::unexpected(std::move(result.error()));
      }
    } else if (tech_state == TechController::ParentState::Lay) {
      {
        if (auto result = humanoid()->set_state(Humanoid::Lay); !result)
          return std::unexpected(std::move(result.error()));
      }
    } else if (tech_state == TechController::ParentState::Duck) {
      {
        if (auto result = humanoid()->set_state(Humanoid::Duck); !result)
          return std::unexpected(std::move(result.error()));
      }
    } else if (tech_state == TechController::ParentState::Walk) {
      {
        if (auto result = humanoid()->set_state(Humanoid::Walk); !result)
          return std::unexpected(std::move(result.error()));
      }
    } else if (tech_state == TechController::ParentState::Run) {
      {
        if (auto result = humanoid()->set_state(Humanoid::Run); !result)
          return std::unexpected(std::move(result.error()));
      }
    } else if (tech_state == TechController::ParentState::Swim) {
      {
        if (auto result = humanoid()->set_state(Humanoid::Swim); !result)
          return std::unexpected(std::move(result.error()));
      }
    } else if (tech_state == TechController::ParentState::SwimIdle) {
      {
        if (auto result = humanoid()->set_state(Humanoid::SwimIdle); !result)
          return std::unexpected(std::move(result.error()));
      }
    }
  } else if (m_movement_animation_state) {
    if (auto result = humanoid()->set_state(*m_movement_animation_state); !result)
      return std::unexpected(std::move(result.error()));
  } else {
    auto lounge_anchor = as<LoungeAnchor>(m_movement_controller->entity_anchor());
    if (m_state == State::Idle) {
      {
        if (auto result = humanoid()->set_state(Humanoid::Idle); !result)
          return std::unexpected(std::move(result.error()));
      }
    } else if (m_state == State::Walk) {
      {
        if (auto result = humanoid()->set_state(Humanoid::Walk); !result)
          return std::unexpected(std::move(result.error()));
      }
    } else if (m_state == State::Run) {
      {
        if (auto result = humanoid()->set_state(Humanoid::Run); !result)
          return std::unexpected(std::move(result.error()));
      }
    } else if (m_state == State::Jump) {
      {
        if (auto result = humanoid()->set_state(Humanoid::Jump); !result)
          return std::unexpected(std::move(result.error()));
      }
    } else if (m_state == State::Fall) {
      {
        if (auto result = humanoid()->set_state(Humanoid::Fall); !result)
          return std::unexpected(std::move(result.error()));
      }
    } else if (m_state == State::Swim) {
      {
        if (auto result = humanoid()->set_state(Humanoid::Swim); !result)
          return std::unexpected(std::move(result.error()));
      }
    } else if (m_state == State::SwimIdle) {
      {
        if (auto result = humanoid()->set_state(Humanoid::SwimIdle); !result)
          return std::unexpected(std::move(result.error()));
      }
    } else if (m_state == State::Crouch) {
      {
        if (auto result = humanoid()->set_state(Humanoid::Duck); !result)
          return std::unexpected(std::move(result.error()));
      }
    } else if (m_state == State::Lounge && lounge_anchor && lounge_anchor->orientation == LoungeOrientation::Sit) {
      {
        if (auto result = humanoid()->set_state(Humanoid::Sit); !result)
          return std::unexpected(std::move(result.error()));
      }
    } else if (m_state == State::Lounge && lounge_anchor && lounge_anchor->orientation == LoungeOrientation::Lay) {
      {
        if (auto result = humanoid()->set_state(Humanoid::Lay); !result)
          return std::unexpected(std::move(result.error()));
      }
    } else if (m_state == State::Lounge && lounge_anchor && lounge_anchor->orientation == LoungeOrientation::Stand) {
      {
        if (auto result = humanoid()->set_state(Humanoid::Idle); !result)
          return std::unexpected(std::move(result.error()));
      }
    }
  }

  {

    if (auto result = humanoid()->set_emote_state(m_emote_state); !result)

      return std::unexpected(std::move(result.error()));

  }
  return humanoid()->prepare_animator_pose();
}

std::expected<String, StarException> Player::get_footstep_sound(Vec2I const& sensor) const {
  auto database_result = root().material_database();
  if (!database_result)
    return std::unexpected(std::move(database_result.error()));
  auto const& database = **database_result;
  auto current_world_result = world();
  if (!current_world_result)
    return std::unexpected(std::move(current_world_result.error()));
  auto& current_world = current_world_result->get();
  String fallback = database.default_footstep_sound();
  List<Vec2I> scan_order{{0, 0}, {0, -1}, {-1, 0}, {1, 0}, {-1, -1}, {1, -1}};
  for (auto const& sub_sensor : scan_order) {
    auto sound_result = database.footstep_sound(current_world.material(sensor + sub_sensor, TileLayer::Foreground), current_world.mod(sensor + sub_sensor, TileLayer::Foreground));
    if (!sound_result)
      return std::unexpected(std::move(sound_result.error()));
    auto sound = std::move(*sound_result);
    if (!sound.empty() && sound != fallback)
      return sound;
  }
  return fallback;
}

std::expected<bool, StarException> Player::in_interaction_range() const {
  return in_interaction_range(center_of_tile(m_aim_position));
}

std::expected<bool, StarException> Player::in_interaction_range(Vec2F aim_pos) const {
  if (is_admin())
    return true;
  auto current_world_result = world();
  if (!current_world_result)
    return std::unexpected(std::move(current_world_result.error()));
  auto& current_world = current_world_result->get();
  return current_world.geometry().diff(aim_pos, position()).magnitude() < m_interact_radius;
}

std::expected<bool, StarException> Player::in_tool_range() const {
  return in_tool_range(center_of_tile(m_aim_position));
}

std::expected<bool, StarException> Player::in_tool_range(Vec2F const& aim_pos) const {
  if (is_admin())
    return true;
  auto current_world_result = world();
  if (!current_world_result)
    return std::unexpected(std::move(current_world_result.error()));
  auto& current_world = current_world_result->get();
  auto radius_result = tool_radius();
  if (!radius_result)
    return std::unexpected(std::move(radius_result.error()));
  auto radius = std::move(*radius_result);
  return current_world.geometry().diff(aim_pos, position()).magnitude() < radius;
}

std::expected<void, StarException> Player::get_net_states(bool initial) {
  auto elements = m_net_humanoid.net_elements();
  auto elements_last_result = elements.last();
  if (!elements_last_result)
    return std::unexpected(std::move(elements_last_result.error()));
  auto elements_last = std::move(elements_last_result->get());
  m_current_net_humanoid = std::move(elements_last);
  m_state = static_cast<State>(m_state_net_state.get());
  m_shifting = m_shifting_net_state.get();
  m_aim_position[0] = m_x_aim_position_net_state.get();
  m_aim_position[1] = m_y_aim_position_net_state.get();
  if (m_identity_net_state.pull_updated() && !initial) {
    auto new_identity = m_identity_net_state.get();
    if (m_identity.species == new_identity.species && m_identity.image_path == new_identity.image_path) {
      if (auto result = humanoid()->set_identity(new_identity); !result)
        return std::unexpected(std::move(result.error()));
    }
    m_identity = std::move(new_identity);
  }
  if (m_refreshed_humanoid_parameters.pull_occurred() && !initial) {
    if (auto result = refresh_humanoid_parameters(); !result)
      return std::unexpected(std::move(result.error()));
  }
  setTeam(m_team_net_state.get());
  if (m_landed_net_state.pull_occurred() && !initial)
    m_landing_noise_pending = true;
  if (m_new_chat_message_net_state.pull_occurred() && !initial) {
    m_chat_message = m_chat_message_net_state.get();
    m_chat_message_updated = true;
    m_pending_chat_actions.push_back(SayChatAction{entityId(), m_chat_message, m_movement_controller->position()});
  }
  if (!humanoid_emote_names)
    return std::unexpected(humanoid_emote_names.error());
  auto emote_result = humanoid_emote_names->get_left(m_emote_net_state.get());
  if (!emote_result)
    return std::unexpected(std::move(emote_result.error()));
  auto const& emote = emote_result->get();
  m_emote_state = emote;
  m_dance.name = m_humanoid_dance_net_state.get();
  return apply_net_armor_secrets();
}

std::expected<void, StarException> Player::set_net_states() {
  // The legacy player-state field controls remote presentation, not the local
  // movement controller or native movement effects. Keep its type and slot.
  // Teleport/lounge/death can start after update (for example in postUpdate).
  // They must not be hidden by a presentation request already resolved above.
  if (is_dead() || is_teleporting() || m_state == State::Lounge
      || m_movement_controller->entity_anchor() || m_tech_controller->parentState()) {
    m_movement_animation_state.reset();
    m_movement_controller->set_network_moving_direction({});
  }
  State state = m_state;
  if (m_movement_animation_state) {
    switch (*m_movement_animation_state) {
      case Humanoid::Idle: state = State::Idle; break;
      case Humanoid::Walk: state = State::Walk; break;
      case Humanoid::Run: state = State::Run; break;
      case Humanoid::Jump: state = State::Jump; break;
      case Humanoid::Fall: state = State::Fall; break;
      case Humanoid::Duck: state = State::Crouch; break;
      default: break;
    }
  }
  m_state_net_state.set(static_cast<unsigned>(state));
  m_shifting_net_state.set(m_shifting);
  m_x_aim_position_net_state.set(m_aim_position[0]);
  m_y_aim_position_net_state.set(m_aim_position[1]);
  if (m_identity_updated) {
    m_identity_net_state.push(m_identity);
    m_identity_updated = false;
  }
  m_team_net_state.set(getTeam());
  if (m_chat_message_changed) {
    m_chat_message_changed = false;
    m_chat_message_net_state.push(m_chat_message);
    m_new_chat_message_net_state.trigger();
  }
  if (!humanoid_emote_names)
    return std::unexpected(humanoid_emote_names.error());
  auto emote_result = humanoid_emote_names->get_right(m_emote_state);
  if (!emote_result)
    return std::unexpected(std::move(emote_result.error()));
  auto const& emote = emote_result->get();
  m_emote_net_state.set(emote);
  m_humanoid_dance_net_state.set(m_dance.name);
  return {};
}

std::expected<void, StarException> Player::set_net_armor_secret(EquipmentSlot slot, std::shared_ptr<ArmorItem> const& armor, bool visible) {
  auto slot_name_result = enum_name<equipment_slot_names>(slot);
  if (!slot_name_result)
    return std::unexpected(std::move(slot_name_result.error()));
  auto const& slot_name = *slot_name_result;
  ItemDescriptor descriptor = visible ? item_safe_descriptor(armor) : ItemDescriptor();
  auto versioning_result = root().versioningDatabase();
  if (!versioning_result)
    return std::unexpected(std::move(versioning_result.error()));
  auto versioning = std::move(*versioning_result);
  auto store_result = descriptor.disk_store(*versioning);
  if (!store_result)
    return std::unexpected(std::move(store_result.error()));
  auto store = std::move(*store_result);
  if (auto result = set_secret_property(Star::format("armorWearer.{}.data", slot_name), store); !result)
    return std::unexpected(std::move(result.error()));
  if (m_armor_secret_net_versions.empty()) {
    if (auto result = set_secret_property("armorWearer.replicating", true); !result)
      return std::unexpected(std::move(result.error()));
  }
  return set_secret_property(Star::format("armorWearer.{}.version", slot_name), ++m_armor_secret_net_versions[slot]);
}

std::expected<void, StarException> Player::set_net_armor_secrets(bool include_empty) {
  if (m_client_context && m_client_context->net_compatibility_rules().version() < 9) {
    for (std::uint8_t i = 0; i != 12; ++i) {
      auto slot = EquipmentSlot(static_cast<std::uint8_t>(EquipmentSlot::Cosmetic1) + i);
      auto stored_result = m_inventory->itemsAt(slot);
      if (!stored_result)
        return std::unexpected(std::move(stored_result.error()));
      auto stored = std::move(*stored_result);
      auto item = as<ArmorItem>(stored);
      bool visible = m_inventory->equipmentVisibility(slot);
      if ((item && visible) || include_empty) {
        if (auto result = set_net_armor_secret(slot, item, visible); !result)
          return std::unexpected(std::move(result.error()));
      }
    }
  }
  return {};
}

std::expected<void, StarException> Player::apply_net_armor_secrets() {
  if (!isSlave() || !get_secret_property_ref("armorWearer.replicating"))
    return {};
  auto database = root().itemDatabase();
  if (!database)
    return std::unexpected(std::move(database.error()));
  for (std::uint8_t i = 0; i != 12; ++i) {
    auto slot = EquipmentSlot(static_cast<std::uint8_t>(EquipmentSlot::Cosmetic1) + i);
    auto name = enum_name<equipment_slot_names>(slot);
    if (!name)
      return std::unexpected(std::move(name.error()));
    auto version = get_secret_property(Star::format("armorWearer.{}.version", *name), 0);
    if (!version)
      return std::unexpected(std::move(version.error()));
    if (!version->is_type(Json::Type::Int))
      continue;
    auto revision = version->to_uint();
    if (!revision)
      return std::unexpected(std::move(revision.error()));
    if (*revision <= m_armor_secret_net_versions.value(slot, 0))
      continue;
    std::shared_ptr<ArmorItem> item;
    if (auto data = get_secret_property_ref(Star::format("armorWearer.{}.data", *name))) {
      DataStreamReader reader{std::span<char const>(data->get().utf8())};
      auto store = reader.read<Json>();
      if (!store)
        return std::unexpected(std::move(store.error()));
      if (auto loaded = (*database)->disk_load(*store, item); !loaded)
        return std::unexpected(std::move(loaded.error()));
    }
    auto applied = m_inventory->setItem(slot, item);
    if (!applied)
      return std::unexpected(std::move(applied.error()));
    if (!*applied)
      return std::unexpected(ItemException("Could not apply replicated cosmetic armor"));
    m_armor->set_cosmetic_item(i, item);
    m_armor_secret_net_versions[slot] = *revision;
  }
  return {};
}

void Player::set_admin(bool is_admin) {
  m_is_admin = is_admin;
}

bool Player::is_admin() const {
  return m_is_admin;
}

std::expected<void, StarException> Player::set_favorite_color(Color color) {
  m_identity.color = color.to_rgba();
  return update_identity();
}

Color Player::favorite_color() const {
  return Color::rgba(m_identity.color);
}

bool Player::is_teleporting() const {
  return (m_state == State::TeleportIn) || (m_state == State::TeleportOut);
}

bool Player::is_teleporting_out() const {
  return inWorld() && (m_state == State::TeleportOut) && m_teleport_timer >= 0.0f;
}

std::expected<bool, StarException> Player::can_deploy() {
  return m_deployment->canDeploy();
}

void Player::deploy_abort(String const& animation_type) {
  m_teleport_animation_type = animation_type;
  m_deployment->setDeploying(false);
}

bool Player::is_deploying() const {
  return m_deployment->isDeploying();
}

bool Player::is_deployed() const {
  return m_deployment->isDeployed();
}

std::expected<void, StarException> Player::set_busy_state(PlayerBusyState busy_state) {
  if (!PlayerBusyStateNames)
    return std::unexpected(PlayerBusyStateNames.error());
  auto name_result = PlayerBusyStateNames->get_right(busy_state);
  if (!name_result)
    return std::unexpected(std::move(name_result.error()));
  auto const& name = name_result->get();
  if (auto result = m_effects_animator->set_state("busy", name); !result)
    return std::unexpected(std::move(result.error()));
  return {};
}

std::expected<void, StarException> Player::teleport_out(String const& animation_type, bool deploy) {
  resetPresentation();
  m_state = State::TeleportOut;
  m_teleport_animation_type = animation_type;
  {
    if (auto result = m_effects_animator->set_state("teleport", m_teleport_animation_type + "Out"); !result)
      return std::unexpected(std::move(result.error()));
  }
  m_deployment->setDeploying(deploy);
  {
    if (auto result = m_deployment->teleportOut(); !result)
      return std::unexpected(std::move(result.error()));
  }
  m_teleport_timer = deploy ? m_config->deploy_out_time : m_config->teleport_out_time;
  return {};
}

std::expected<void, StarException> Player::teleport_in() {
  resetPresentation();
  m_state = State::TeleportIn;
  if (auto result = m_effects_animator->set_state("teleport", m_teleport_animation_type + "In"); !result)
    return std::unexpected(std::move(result.error()));
  m_teleport_timer = m_deployment->isDeployed() ? m_config->deploy_in_time : m_config->teleport_in_time;
  auto assets_result = root().assets();
  if (!assets_result)
    return std::unexpected(std::move(assets_result.error()));
  auto assets = std::move(*assets_result);
  auto json_result = assets->json("/player.config:teleportInStatusEffects");
  if (!json_result)
    return std::unexpected(std::move(json_result.error()));
  auto json = std::move(*json_result);
  auto array_result = json.to_array();
  if (!array_result)
    return std::unexpected(std::move(array_result.error()));
  auto array = std::move(*array_result);
  List<EphemeralStatusEffect> effects;
  for (auto const& entry : array) {
    auto effect_result = jsonToEphemeralStatusEffect(entry);
    if (!effect_result)
      return std::unexpected(std::move(effect_result.error()));
    auto effect = std::move(*effect_result);
    effects.push_back(std::move(effect));
  }
  return m_status_controller->addEphemeralEffects(effects);
}

std::expected<void, StarException> Player::teleport_abort() {
  resetPresentation();
  m_state = State::TeleportIn;
  {
    if (auto result = m_effects_animator->set_state("teleport", "abort"); !result)
      return std::unexpected(std::move(result.error()));
  }
  m_deployment->setDeploying(m_deployment->isDeployed());
  m_teleport_timer = m_config->teleport_in_time;
  return {};
}

std::expected<void, StarException> Player::move_to(Vec2F const& foot_position) {
  auto offset_result = feet_offset();
  if (!offset_result)
    return std::unexpected(std::move(offset_result.error()));
  auto offset = std::move(*offset_result);
  resetPresentation();
  m_movement_controller->set_position(foot_position - offset);
  m_movement_controller->set_velocity(Vec2F());
  return {};
}

std::shared_ptr<Item> Player::primary_hand_item() const {
  return m_tools->primaryHandItem();
}

std::shared_ptr<Item> Player::alt_hand_item() const {
  return m_tools->altHandItem();
}

std::expected<Uuid, StarException> Player::uuid() const {
  auto id = uniqueId();
  if (!id)
    return std::unexpected(PlayerException("Player has no unique id"));
  return Uuid::from_hex((*id).utf8());
}

PlayerMode Player::mode_type() const {
  return m_mode_type;
}

std::expected<void, StarException> Player::set_mode_type(PlayerMode mode) {
  m_mode_type = mode;
  auto assets_result = root().assets();
  if (!assets_result)
    return std::unexpected(std::move(assets_result.error()));
  auto assets = std::move(*assets_result);
  auto config_result = assets->json("/playermodes.config");
  if (!config_result)
    return std::unexpected(std::move(config_result.error()));
  auto config = std::move(*config_result);
  if (!PlayerModeNames)
    return std::unexpected(PlayerModeNames.error());
  auto name_result = PlayerModeNames->get_right(mode);
  if (!name_result)
    return std::unexpected(std::move(name_result.error()));
  auto const& name = name_result->get();
  auto mode_json_result = config.get(name);
  if (!mode_json_result)
    return std::unexpected(std::move(mode_json_result.error()));
  auto mode_json = std::move(*mode_json_result);
  auto mode_config_result = PlayerModeConfig::create(mode_json);
  if (!mode_config_result)
    return std::unexpected(std::move(mode_config_result.error()));
  auto mode_config = std::move(*mode_config_result);
  m_mode_config = std::move(mode_config);
  return {};
}

PlayerModeConfig const& Player::mode_config() const & {
  return m_mode_config;
}

ShipUpgrades const& Player::ship_upgrades() const & {
  return m_ship_upgrades;
}

void Player::set_ship_upgrades(ShipUpgrades const& ship_upgrades) {
  if (m_ship_upgrades != ship_upgrades)
    m_ship_upgrades = ship_upgrades;
}

std::expected<void, StarException> Player::apply_ship_upgrades(Json const& upgrades) {
  auto player_uuid = uuid();
  if (!player_uuid)
    return std::unexpected(std::move(player_uuid.error()));
  if (m_client_context->player_uuid() == *player_uuid) {
    if (auto result = m_client_context->rpc_interface()->invoke_remote("ship.applyShipUpgrades", upgrades); !result)
      return std::unexpected(std::move(result.error()));
  } else {
    if (auto result = m_ship_upgrades.apply(upgrades); !result)
      return std::unexpected(std::move(result.error()));
  }
  return {};
}

void Player::set_ship_species(String species) {
  m_ship_species = std::move(species);
}

String Player::ship_species() const {
  return m_ship_species;
}

std::expected<String, StarException> Player::name() const {
  return m_identity.name;
}

std::expected<void, StarException> Player::set_name(String const& name) {
  m_identity.name = name;
  return update_identity();
}

std::optional<String> Player::status_text() const {
  return {};
}

bool Player::display_nametag() const {
  return true;
}

std::expected<Vec3B, StarException> Player::nametag_color() const {
  auto assets_result = root().assets();
  if (!assets_result)
    return std::unexpected(std::move(assets_result.error()));
  auto assets = std::move(*assets_result);
  auto color_result = assets->json("/player.config:nametagColor");
  if (!color_result)
    return std::unexpected(std::move(color_result.error()));
  auto color = std::move(*color_result);
  return json_to_vec3b(color);
}

std::expected<Vec2F, StarException> Player::nametag_origin() const {
  return mouth_position(false);
}

std::expected<String, StarException> Player::nametag() const {
  auto value_result = get_secret_property("nametag");
  if (!value_result)
    return std::unexpected(std::move(value_result.error()));
  auto value = std::move(*value_result);
  if (value.is_type(Json::Type::String))
    return value.to_string();
  return m_identity.name;
}

std::expected<void, StarException> Player::set_nametag(std::optional<String> nametag) {
  return set_secret_property("nametag", nametag ? Json(*nametag) : Json());
}

std::expected<void, StarException> Player::update_identity() {
  m_identity_updated = true;
  auto const& old_identity = humanoid()->identity();
  if (m_identity.species != old_identity.species || m_identity.image_path != old_identity.image_path)
    return refresh_humanoid_parameters();
  return humanoid()->set_identity(m_identity);
}

std::expected<void, StarException> Player::set_humanoid_parameter(String key, std::optional<Json> value) {
  auto parameters = m_humanoid_parameters;
  if (value)
    parameters.set(std::move(key), std::move(*value));
  else
    parameters.erase(key);
  return set_humanoid_parameters(std::move(parameters));
}

std::optional<Json> Player::get_humanoid_parameter(String key) {
  return m_humanoid_parameters.maybe(key);
}

std::expected<void, StarException> Player::set_humanoid_parameters(JsonObject parameters) {
  auto applied = m_current_net_humanoid->set_humanoid_parameters(parameters);
  if (!applied)
    return std::unexpected(std::move(applied.error()));
  m_humanoid_parameters = std::move(parameters);
  return {};
}

JsonObject Player::get_humanoid_parameters() {
  return m_humanoid_parameters;
}

std::expected<void, StarException> Player::set_body_directives(String const& directives)
{ m_identity.body_directives = directives; return update_identity(); }

std::expected<void, StarException> Player::set_emote_directives(String const& directives)
{ m_identity.emote_directives = directives; return update_identity(); }

std::expected<void, StarException> Player::set_hair_group(String const& group)
{ m_identity.hair_group = group; return update_identity(); }

std::expected<void, StarException> Player::set_hair_type(String const& type)
{ m_identity.hair_type = type; return update_identity(); }

std::expected<void, StarException> Player::set_hair_directives(String const& directives)
{ m_identity.hair_directives = directives; return update_identity(); }

std::expected<void, StarException> Player::set_facial_hair_group(String const& group)
{ m_identity.facial_hair_group = group; return update_identity(); }

std::expected<void, StarException> Player::set_facial_hair_type(String const& type)
{ m_identity.facial_hair_type = type; return update_identity(); }

std::expected<void, StarException> Player::set_facial_hair_directives(String const& directives)
{ m_identity.facial_hair_directives = directives; return update_identity(); }

std::expected<void, StarException> Player::set_facial_mask_group(String const& group)
{ m_identity.facial_mask_group = group; return update_identity(); }

std::expected<void, StarException> Player::set_facial_mask_type(String const& type)
{ m_identity.facial_mask_type = type; return update_identity(); }

std::expected<void, StarException> Player::set_facial_mask_directives(String const& directives)
{ m_identity.facial_mask_directives = directives; return update_identity(); }

std::expected<void, StarException> Player::set_hair(String const& group, String const& type, String const& directives) {
  m_identity.hair_group = group;
  m_identity.hair_type = type;
  m_identity.hair_directives = directives;
  return update_identity();
}

std::expected<void, StarException> Player::set_facial_hair(String const& group, String const& type, String const& directives) {
  m_identity.facial_hair_group = group;
  m_identity.facial_hair_type = type;
  m_identity.facial_hair_directives = directives;
  return update_identity();
}

std::expected<void, StarException> Player::set_facial_mask(String const& group, String const& type, String const& directives) {
  m_identity.facial_mask_group = group;
  m_identity.facial_mask_type = type;
  m_identity.facial_mask_directives = directives;
  return update_identity();
}

std::expected<void, StarException> Player::set_species(String const& species) {
  auto database_result = root().speciesDatabase();
  if (!database_result)
    return std::unexpected(std::move(database_result.error()));
  auto database = std::move(*database_result);
  if (auto result = database->species(species); !result)
    return std::unexpected(std::move(result.error()));
  m_identity.species = species;
  return update_identity();
}

Gender Player::gender() const {
  return m_identity.gender;
}

std::expected<void, StarException> Player::set_gender(Gender const& gender) {
  m_identity.gender = gender;
  return update_identity();
}

String Player::species() const {
  return m_identity.species;
}

std::expected<void, StarException> Player::set_personality(Personality const& personality) {
  m_identity.personality = personality;
  return update_identity();
}

std::expected<void, StarException> Player::set_image_path(std::optional<String> const& image_path) {
  m_identity.image_path = image_path;
  return update_identity();
}

std::shared_ptr<Humanoid> Player::humanoid() {
  return m_current_net_humanoid->humanoid();
}
std::shared_ptr<Humanoid> Player::humanoid() const {
  return m_current_net_humanoid->humanoid();
}

HumanoidIdentity const& Player::identity() const {
  return m_identity;
}

std::expected<void, StarException> Player::set_identity(HumanoidIdentity identity) {
  m_identity = std::move(identity);
  return update_identity();
}

List<String> Player::pull_queued_messages() { return std::exchange(m_queued_messages, {}); }

List<std::shared_ptr<Item>> Player::pull_queued_item_drops() { return std::exchange(m_queued_item_pickups, {}); }

std::expected<void, StarException> Player::queue_ui_message(String const& message) {
  if (!isSlave())
    m_queued_messages.push_back(message);
  return {};
}

void Player::queue_item_pickup_message(std::shared_ptr<Item> const& item) {
  if (!isSlave())
    m_queued_item_pickups.push_back(item);
}

std::expected<void, StarException> Player::add_chat_message(String const& message, Json const& config) {
  if constexpr (Star::debug_enabled) {
    Star::star_assert_failed((!isSlave()));
  }
  m_chat_message = message;
  m_chat_message_updated = true;
  m_chat_message_changed = true;
  auto mouth_result = mouth_position();
  if (!mouth_result)
    return std::unexpected(std::move(mouth_result.error()));
  auto mouth = std::move(*mouth_result);
  m_pending_chat_actions.push_back(SayChatAction{entityId(), message, mouth, config});
  return {};
}

void Player::add_emote(HumanoidEmote const& emote, std::optional<float> emote_cooldown) {
  if constexpr (Star::debug_enabled) {
    Star::star_assert_failed((!isSlave()));
  }
  m_emote_state = emote;
  m_emote_cooldown_timer = GameTimer(emote_cooldown.value_or(m_emote_cooldown));
}
std::expected<void, StarException> Player::set_dance(std::optional<String> const& dance_name) {
  if constexpr (Star::debug_enabled)
    Star::star_assert_failed(!isSlave());
  assert(!isSlave());
  return m_dance.set(root(), dance_name);
}

std::pair<HumanoidEmote, float> Player::current_emote() const {
  return std::make_pair(m_emote_state, m_emote_cooldown_timer.remaining);
}

Player::State Player::current_state() const {
  return m_state;
}

List<ChatAction> Player::pull_pending_chat_actions() { return std::exchange(m_pending_chat_actions, {}); }

std::expected<std::optional<String>, StarException> Player::inspection_log_name() const {
  auto identifier = uniqueId();
  if (identifier) {
    auto hash = xx_hash3_128(std::span<char const>(identifier->utf8()));
    auto bytes = std::bit_cast<std::array<char, sizeof(hash)>>(hash);
    return String("Player #") + hex_encode(std::span<char const>(bytes));
  }
  return identifier;
}

std::expected<std::optional<String>, StarException> Player::inspection_description(String const&) const {
  return m_description;
}

std::expected<float, StarException> Player::beam_gun_radius() const {
  return m_tools->beamGunRadius();
}

std::expected<bool, StarException> Player::instrument_playing() {
  return m_songbook->instrumentPlaying();
}

std::expected<void, StarException> Player::instrument_equipped(String const& instrument_kind) {
  if (can_use_tool()) {
    auto mouth_result = mouth_position();
    if (!mouth_result)
      return std::unexpected(std::move(mouth_result.error()));
    auto mouth = std::move(*mouth_result);
    m_songbook->keepAlive(instrument_kind, mouth);
  }
  return {};
}

std::expected<void, StarException> Player::interact(InteractAction action) {
  if constexpr (Star::debug_enabled) {
    Star::star_assert_failed((!isSlave()));
  }
  queue_interact_action(RpcPromise<InteractAction>::make_fulfilled(std::move(action)));
  return {};
}

std::expected<void, StarException> Player::add_effect_emitters(StringSet const& emitters) {
  if constexpr (Star::debug_enabled) {
    Star::star_assert_failed((!isSlave()));
  }
  m_effect_emitter->add_effect_sources("normal", emitters);
  return {};
}

std::expected<void, StarException> Player::request_emote(String const& emote) {
  if (!humanoid_emote_names)
    return std::unexpected(humanoid_emote_names.error());
  auto state_result = humanoid_emote_names->get_left(emote);
  if (!state_result)
    return std::unexpected(std::move(state_result.error()));
  auto const& state = state_result->get();
  if (state != HumanoidEmote::Idle && (m_emote_state == state || m_emote_state == HumanoidEmote::Idle || m_emote_state == HumanoidEmote::Blink))
    add_emote(state, {});
  return {};
}

ActorMovementController& Player::movement_controller() {
  return *m_movement_controller;
}

StatusController& Player::status_controller() {
  return *m_status_controller;
}

std::expected<List<PhysicsForceRegion>, StarException> Player::force_regions() const {
  return m_tools->force_regions();
}


std::shared_ptr<StatusController> Player::status_controller_ptr() {
  return m_status_controller;
}

std::shared_ptr<ActorMovementController> Player::movement_controller_ptr() {
  return m_movement_controller;
}

std::shared_ptr<PlayerConfig> Player::config() {
  return m_config;
}

std::shared_ptr<Songbook> Player::songbook() const {
  return m_songbook;
}

std::shared_ptr<QuestManager> Player::quest_manager() const {
  return m_quest_manager;
}

std::expected<Json, StarException> Player::disk_store() {
  JsonObject storage;
  for (auto&& script : m_generic_script_contexts) {
    auto entry_result = script.second->get_script_storage();
    if (!entry_result)
      return std::unexpected(std::move(entry_result.error()));
    auto entry = std::move(*entry_result);
    if (!entry.empty())
      storage[script.first] = std::move(entry);
  }
  auto id = uniqueId();
  if (!id)
    return std::unexpected(PlayerException("Player has no unique id"));
  if (!PlayerModeNames)
    return std::unexpected(PlayerModeNames.error());
  auto mode_result = PlayerModeNames->get_right(m_mode_type);
  if (!mode_result)
    return std::unexpected(std::move(mode_result.error()));
  auto const& mode = mode_result->get();
  auto universe_map_result = m_universe_map->toJson();
  if (!universe_map_result)
    return std::unexpected(std::move(universe_map_result.error()));
  auto universe_map = std::move(*universe_map_result);
  auto techs_result = m_techs->toJson();
  if (!techs_result)
    return std::unexpected(std::move(techs_result.error()));
  auto techs = std::move(*techs_result);
  auto identity_result = m_identity.to_json();
  if (!identity_result)
    return std::unexpected(std::move(identity_result.error()));
  auto identity = std::move(*identity_result);
  auto team_result = getTeam().to_json();
  if (!team_result)
    return std::unexpected(std::move(team_result.error()));
  auto team = std::move(*team_result);
  auto inventory_result = m_inventory->store();
  if (!inventory_result)
    return std::unexpected(std::move(inventory_result.error()));
  auto inventory = std::move(*inventory_result);
  auto movement_result = m_movement_controller->store_state();
  if (!movement_result)
    return std::unexpected(std::move(movement_result.error()));
  auto movement = std::move(*movement_result);
  auto tech_controller_result = m_tech_controller->disk_store();
  if (!tech_controller_result)
    return std::unexpected(std::move(tech_controller_result.error()));
  auto tech_controller = std::move(*tech_controller_result);
  auto status_controller_result = m_status_controller->diskStore();
  if (!status_controller_result)
    return std::unexpected(std::move(status_controller_result.error()));
  auto status_controller = std::move(*status_controller_result);
  auto quests_result = m_quest_manager->diskStore();
  if (!quests_result)
    return std::unexpected(std::move(quests_result.error()));
  auto quests = std::move(*quests_result);
  auto companions_result = m_companions->diskStore();
  if (!companions_result)
    return std::unexpected(std::move(companions_result.error()));
  auto companions = std::move(*companions_result);
  auto deployment_result = m_deployment->diskStore();
  if (!deployment_result)
    return std::unexpected(std::move(deployment_result.error()));
  auto deployment = std::move(*deployment_result);
  return JsonObject{
    {"uuid", *id}, {"description", m_description}, {"modeType", mode},
    {"shipUpgrades", m_ship_upgrades.to_json()}, {"shipSpecies", !m_ship_species.empty() ? m_ship_species : m_identity.species},
    {"blueprints", m_blueprints->toJson()}, {"universeMap", std::move(universe_map)}, {"codexes", m_codexes->to_json()},
    {"techs", std::move(techs)}, {"identity", std::move(identity)}, {"team", std::move(team)},
    {"inventory", std::move(inventory)}, {"movementController", std::move(movement)},
    {"techController", std::move(tech_controller)}, {"statusController", std::move(status_controller)},
    {"log", m_log->toJson()}, {"aiState", m_ai_state.to_json()}, {"quests", std::move(quests)},
    {"companions", std::move(companions)}, {"deployment", std::move(deployment)},
    {"genericProperties", m_generic_properties}, {"genericScriptStorage", std::move(storage)}, {"humanoidParameters", m_humanoid_parameters}
  };
}

std::expected<ByteArray, StarException> Player::net_store(NetCompatibilityRules rules) {
  DataStreamBuffer ds;
  ds.set_stream_compatibility_version(rules);
  auto id = uniqueId();
  if (!id)
    return std::unexpected(PlayerException("Player has no unique id"));
  if (auto result = ds.write(*id); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = ds.write(m_description); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = ds.write(m_mode_type); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = ds.write(m_identity); !result)
    return std::unexpected(std::move(result.error()));
  if (rules.version() >= 10) {
    if (auto result = ds.write(m_humanoid_parameters); !result)
      return std::unexpected(std::move(result.error()));
  }
  return ds.take_data();
}

std::expected<void, StarException> Player::finalize_creation() {
  m_blueprints = std::make_shared<PlayerBlueprints>();
  m_techs = std::make_shared<PlayerTech>(root());
  auto database_result = root().itemDatabase();
  if (!database_result)
    return std::unexpected(std::move(database_result.error()));
  auto database = std::move(*database_result);
  for (auto const& descriptor : m_config->default_items) {
    auto item_result = database->item(descriptor);
    if (!item_result)
      return std::unexpected(std::move(item_result.error()));
    auto item = std::move(*item_result);
    if (auto result = m_inventory->addItems(item); !result)
      return std::unexpected(std::move(result.error()));
  }
  auto species_database_result = root().speciesDatabase();
  if (!species_database_result)
    return std::unexpected(std::move(species_database_result.error()));
  auto species_database = std::move(*species_database_result);
  auto species_definition_result = species_database->species(m_identity.species);
  if (!species_definition_result)
    return std::unexpected(std::move(species_definition_result.error()));
  auto species_definition = std::move(*species_definition_result);
  for (auto const& descriptor : species_definition->defaultItems()) {
    auto item_result = database->item(descriptor);
    if (!item_result)
      return std::unexpected(std::move(item_result.error()));
    auto item = std::move(*item_result);
    if (auto result = m_inventory->addItems(item); !result)
      return std::unexpected(std::move(result.error()));
  }
  for (auto const& descriptor : m_config->default_blueprints)
    m_blueprints->add(descriptor);
  for (auto const& descriptor : species_definition->defaultBlueprints())
    m_blueprints->add(descriptor);
  if (auto result = refresh_equipment(); !result)
    return std::unexpected(std::move(result.error()));
  m_state = State::Idle;
  m_emote_state = HumanoidEmote::Idle;
  if (auto result = synchronize_equipment_status_effects(*m_armor, *m_tools, *m_status_controller); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = m_status_controller->resetAllResources(); !result)
    return std::unexpected(std::move(result.error()));
  m_effect_emitter->reset();
  m_description = Star::format("This {} seems to have nothing to say for {}self.", m_identity.gender == Gender::Male ? "guy" : "gal", m_identity.gender == Gender::Male ? "him" : "her");
  return {};
}

bool Player::invisible() const {
  return m_status_controller->statPositive("invisible");
}

std::expected<void, StarException> Player::animate_portrait(float dt) {
  if (auto result = humanoid()->animate(dt, {}); !result)
    return std::unexpected(std::move(result.error()));
  if (m_emote_cooldown_timer.tick(dt))
    m_emote_state = HumanoidEmote::Idle;
  return humanoid()->set_emote_state(m_emote_state);
}

std::expected<bool, StarException> Player::is_outside() {
  if (!inWorld())
    return false;
  auto current_world_result = world();
  if (!current_world_result)
    return std::unexpected(std::move(current_world_result.error()));
  auto& current_world = current_world_result->get();
  auto underground_result = current_world.isUnderground(position());
  if (!underground_result)
    return std::unexpected(std::move(underground_result.error()));
  auto underground = std::move(*underground_result);
  if (underground)
    return false;
  auto mouth_result = mouth_position();
  if (!mouth_result)
    return std::unexpected(std::move(mouth_result.error()));
  auto mouth = std::move(*mouth_result);
  auto occupied_result = current_world.tileIsOccupied(Vec2I::floor(mouth), TileLayer::Background);
  if (!occupied_result)
    return std::unexpected(std::move(occupied_result.error()));
  auto occupied = std::move(*occupied_result);
  return !occupied;
}

std::expected<void, StarException> Player::drop_selected_items(std::function<std::expected<bool, StarException>(std::shared_ptr<Item>)> filter) {
  auto current_world = maybeWorld();
  if (!current_world)
    return {};
  return m_inventory->for_each_item([&](InventorySlot const&, std::shared_ptr<Item>& item) -> std::expected<void, StarException> {
    if (!item)
      return {};
    if (filter) {
      auto selected_result = filter(item);
      if (!selected_result)
        return std::unexpected(std::move(selected_result.error()));
      auto selected = std::move(*selected_result);
      if (!selected)
        return {};
    }
    auto angle_result = root().random().uniform_float(-std::numbers::pi, std::numbers::pi);
    if (!angle_result)
      return std::unexpected(std::move(angle_result.error()));
    auto angle = std::move(*angle_result);
    auto drop_result =
      ItemDrop::throw_drop(root(), std::exchange(item, {}), position(), velocity(), Vec2F::with_angle(angle), true);
    if (!drop_result)
      return std::unexpected(std::move(drop_result.error()));
    auto drop = std::move(*drop_result);
    return current_world->get().addEntity(std::move(drop));
  });
}

std::expected<void, StarException> Player::drop_everything() {
  return drop_selected_items({});
}

bool Player::is_perma_dead() const {
  if (!is_dead())
    return false;
  return m_mode_config.permadeath;
}

bool Player::interrupt_radio_message() {
  return std::exchange(m_interrupt_radio_message, false);
}

std::expected<std::optional<RadioMessage>, StarException> Player::pull_pending_radio_message() {
  if (m_pending_radio_messages.empty())
    return std::optional<RadioMessage>{};
  auto& first = m_pending_radio_messages.front();
  if (first.unique)
    m_log->add_radio_message(first.message_id);
  std::expected<std::optional<RadioMessage>, StarException> message(std::in_place, std::in_place, std::move(first));
  m_pending_radio_messages.pop_front();
  return message;
}

std::expected<void, StarException> Player::queue_radio_message(Json const& message_config, float delay) {
  auto checked = process_radio_message(message_config, delay);
  if (!checked)
    return std::unexpected(std::move(checked.error()));
  if (*checked)
    (void)root().logger().warn("Couldn't queue radio message: {}", **checked);
  return {};
}

std::expected<std::optional<String>, StarException> Player::process_radio_message(Json const& config, float delay) {
  auto database = root().radio_message_database();
  if (!database)
    return std::unexpected(std::move(database.error()));
  auto decoded = (*database)->create_radio_message(config);
  if (!decoded)
    return Star::format("invalid radio message: {}", decoded.error().what());
  // Keep the decoded tree alive until the selected payload has been queued.
  // Assigning a parent from one of its own descendants would invalidate the source.
  auto selected = std::ref(*decoded);
  for (;;) {
    auto variant = selected.get().species_ai_message.find_ref(m_ship_species);
    if (!variant)
      variant = selected.get().species_message.find_ref(m_identity.species);
    if (!variant)
      break;
    selected = *variant;
  }
  auto& message = selected.get();
  if (message.type == RadioMessageType::Tutorial) {
    auto configuration = root().configuration();
    if (!configuration)
      return std::unexpected(std::move(configuration.error()));
    auto enabled = (*configuration)->get("tutorialMessages").and_then([](Json const& value) { return value.to_bool(); });
    if (!enabled)
      return std::unexpected(std::move(enabled.error()));
    if (!*enabled)
      return {};
  }
  if (!message.portrait_image.empty()) {
    auto first = message.portrait_image[0];
    if (!first)
      return std::unexpected(std::move(first.error()));
    if (*first != '/') {
      auto ai = root().ai_database();
      if (!ai)
        return std::unexpected(std::move(ai.error()));
      auto portrait = (*ai)->portrait_image(m_ship_species, message.portrait_image);
      if (!portrait)
        return std::unexpected(std::move(portrait.error()));
      message.portrait_image = std::move(*portrait);
    }
  }
  if (m_log->has_radio_message(message.message_id))
    return {};
  // A mission accepts one request per world, including when the first is delayed.
  if (message.type == RadioMessageType::Mission && !m_mission_radio_messages.add(message.message_id))
    return {};
  for (auto const& pending : m_pending_radio_messages) {
    if (pending.message_id == message.message_id)
      return {};
  }
  for (auto& [timer, delayed] : m_delayed_radio_messages) {
    if (delayed.message_id == message.message_id) {
      if (delay == 0)
        timer.set_done();
      return {};
    }
  }
  if (delay > 0)
    m_delayed_radio_messages.emplace_back(GameTimer(delay), std::move(message));
  else
    enqueue_radio_message(std::move(message));
  return {};
}

void Player::enqueue_radio_message(RadioMessage&& message) {
  if (message.important) {
    m_interrupt_radio_message = true;
    m_pending_radio_messages.push_front(std::move(message));
  } else {
    m_pending_radio_messages.push_back(std::move(message));
  }
}

std::expected<std::optional<Json>, StarException> Player::pull_pending_cinematic() {
  if (m_pending_cinematic && m_pending_cinematic->is_type(Json::Type::String)) {
    auto name_result = m_pending_cinematic->to_string();
    if (!name_result)
      return std::unexpected(std::move(name_result.error()));
    auto name = std::move(*name_result);
    m_log->addCinematic(name);
  }
  return std::exchange(m_pending_cinematic, {});
}

std::expected<void, StarException> Player::set_pending_cinematic(Json const& cinematic, bool unique) {
  if (unique && cinematic.is_type(Json::Type::String)) {
    auto name_result = cinematic.to_string();
    if (!name_result)
      return std::unexpected(std::move(name_result.error()));
    auto name = std::move(*name_result);
    if (m_log->cinematics().contains(name))
      return {};
  }
  m_pending_cinematic = cinematic;
  return {};
}

std::expected<void, StarException> Player::set_in_cinematic(bool in_cinematic) {
  if (in_cinematic)
    {
      if (auto result = m_status_controller->setPersistentEffects("cinematic", m_in_cinematic_status_effects); !result)
        return std::unexpected(std::move(result.error()));
    }
  else
    {
      if (auto result = m_status_controller->setPersistentEffects("cinematic", {}); !result)
        return std::unexpected(std::move(result.error()));
    }
  return {};
}

std::optional<Player::AltMusicRequest> Player::pull_pending_alt_music() {
  return std::exchange(m_pending_alt_music, {});
}

std::optional<PlayerWarpRequest> Player::pull_pending_warp() { return std::exchange(m_pending_warp, {}); }

void Player::set_pending_warp(String const& action, std::optional<String> const& animation, bool deploy) {
  m_pending_warp = PlayerWarpRequest{action, animation, deploy};
}

std::expected<std::optional<std::pair<Json, RpcPromiseKeeper<Json>>>, StarException> Player::pull_pending_confirmation() {
  if (m_pending_confirmations.empty())
    return std::optional<std::pair<Json, RpcPromiseKeeper<Json>>>{};
  auto confirmation_result = m_pending_confirmations.take_front();
  if (!confirmation_result)
    return std::unexpected(std::move(confirmation_result.error()));
  auto confirmation = std::move(*confirmation_result);
  return std::optional<std::pair<Json, RpcPromiseKeeper<Json>>>{std::move(confirmation)};
}

void Player::queue_confirmation(Json const& dialog_config, RpcPromiseKeeper<Json> const& result_promise) {
  m_pending_confirmations.push_back(std::make_pair(dialog_config, result_promise));
}

AiState const& Player::ai_state() const {
  return m_ai_state;
}

AiState& Player::ai_state() {
  return m_ai_state;
}

bool Player::inspecting() const {
  return is<InspectionTool>(m_tools->primaryHandItem()) || is<InspectionTool>(m_tools->altHandItem());
}

std::expected<EntityHighlightEffect, StarException> Player::inspection_highlight(std::shared_ptr<InspectableEntity> const& inspectable_entity) const {
  auto tool = as<InspectionTool>(m_tools->primaryHandItem());
  if (!tool)
    tool = as<InspectionTool>(m_tools->altHandItem());
  EntityHighlightEffect result;
  if (!tool)
    return result;
  auto name_result = inspectable_entity->inspection_log_name();
  if (!name_result)
    return std::unexpected(std::move(name_result.error()));
  auto name = std::move(*name_result);
  if (!name)
    return result;
  auto level_result = tool->inspectionHighlightLevel(inspectable_entity);
  if (!level_result)
    return std::unexpected(std::move(level_result.error()));
  auto level = std::move(*level_result);
  result.level = level;
  if (result.level > 0) {
    if (m_interesting_objects.contains(*name))
      result.type = EntityHighlightEffectType::Interesting;
    else if (m_log->scannedObjects().contains(*name))
      result.type = EntityHighlightEffectType::Inspected;
    else
      result.type = EntityHighlightEffectType::Inspectable;
  }
  return result;
}

std::expected<Vec2F, StarException> Player::camera_position() {
  if (auto current_world = maybeWorld()) {
    if (auto anchor = as<LoungeAnchor>(m_movement_controller->entity_anchor())) {
      if (anchor->cameraFocus) {
        auto state = m_movement_controller->anchor_state();
        if (!state)
          return std::unexpected(PlayerException("Player lounge anchor has no anchor state"));
        if (auto entity = current_world->get().entity(state->entityId))
          return entity->position();
      }
    }
    if (m_camera_focus_entity) {
      if (auto entity = current_world->get().entity(*m_camera_focus_entity))
        return entity->position();
      m_camera_focus_entity = {};
    }
  }
  return position();
}

std::shared_ptr<NetworkedAnimator> Player::effects_animator() {
  return m_effects_animator;
}

const String secret_property_prefix = std::string("\0JsonProperty\0", sizeof("\0JsonProperty\0") - 1);

std::optional<StringView> Player::get_secret_property_view(String const& name) const {
  if (auto tag = m_effects_animator->global_tag(secret_property_prefix + name)) {
    auto const& view = tag->get().utf8();
    DataStreamReader buffer{std::span<char const>(view)};
    auto type = buffer.read<std::uint8_t>();
    if (!type || static_cast<Json::Type>(*type - 1) != Json::Type::String)
      return {};
    auto len = buffer.read_vlq_u();
    if (!len)
      return {};
    auto pos = buffer.pos();
    if (!pos)
      return {};
    if (*pos <= buffer.size() && *len == buffer.size() - *pos)
      return StringView(std::string_view(view).substr(*pos, *len));
  }
  return {};
}

std::optional<std::reference_wrapper<String const>> Player::get_secret_property_ref(String const& name) const {
  return m_effects_animator->global_tag(secret_property_prefix + name);
}

std::expected<Json, StarException> Player::get_secret_property(String const& name, Json default_value) const {
  if (auto tag = m_effects_animator->global_tag(secret_property_prefix + name)) {
    DataStreamReader buffer{std::span<char const>(tag->get().utf8())};
    auto value = buffer.read<Json>();
    if (value)
      return std::move(*value);
    if (auto result = root().logger().error_exception(
          Star::format("Exception reading secret player property '{}'", name), ExceptionDiagnostic(value.error()));
        !result)
      return std::unexpected(std::move(result.error()));
  }
  return default_value;
}

std::expected<void, StarException> Player::set_secret_property(String const& name, Json const& value) {
  if (value) {
    DataStreamBuffer ds;
    if (auto result = ds.write(value); !result)
      return std::unexpected(std::move(result.error()));
    auto const& data = ds.data();
    m_effects_animator->set_global_tag(secret_property_prefix + name, String(std::string_view(data.span().data(), data.size())));
  } else {
    m_effects_animator->remove_global_tag(secret_property_prefix + name);
  }
  return {};
}

std::expected<void, StarException> Player::refresh_humanoid_parameters() {
  auto database_result = root().speciesDatabase();
  if (!database_result)
    return std::unexpected(std::move(database_result.error()));
  auto database = std::move(*database_result);
  auto definition_result = database->species(m_identity.species);
  if (!definition_result)
    return std::unexpected(std::move(definition_result.error()));
  auto definition = std::move(*definition_result);
  if (isMaster() || !inWorld()) {
    m_refreshed_humanoid_parameters.trigger();
    if (auto result = install_humanoid(); !result)
      return std::unexpected(std::move(result.error()));
    m_effects_animator->set_global_tag("effectDirectives", definition->effectDirectives());
    m_death_particle_burst.set(humanoid()->default_death_particles());
    m_status_controller->setStatusProperty("ouchNoise", definition->ouchNoise(m_identity.gender));
    m_scripted_animation_parameters.clear();
  } else {
    m_humanoid_parameters = m_current_net_humanoid->humanoid_parameters();
  }
  auto armor_result = m_armor->disk_store();
  if (!armor_result)
    return std::unexpected(std::move(armor_result.error()));
  auto armor = std::move(*armor_result);
  m_armor->reset();
  if (auto result = m_armor->disk_load(armor); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = m_armor->setup_humanoid(*humanoid(), force_nude()); !result)
    return std::unexpected(std::move(result.error()));
  auto parameters_result = ActorMovementParameters::create(json_merge(humanoid()->default_movement_parameters(), humanoid()->player_movement_parameters().value_or(m_config->movement_parameters)));
  if (!parameters_result)
    return std::unexpected(std::move(parameters_result.error()));
  auto parameters = std::move(*parameters_result);
  if (auto result = m_movement_controller->reset_base_parameters(parameters); !result)
    return std::unexpected(std::move(result.error()));
  if (inWorld()) {
    if (isMaster()) {
      for (auto&& script : m_generic_script_contexts) {
        if (script.second->initialized()) {
          if (auto result = script.second->remove_callbacks("animator"); !result)
            return std::unexpected(std::move(result.error()));
          auto callbacks_result = LuaBindings::make_networked_animator_callbacks(humanoid()->networked_animator()->get());
          if (!callbacks_result)
            return std::unexpected(std::move(callbacks_result.error()));
          auto callbacks = std::move(*callbacks_result);
          if (auto result = script.second->add_callbacks("animator", std::move(callbacks)); !result)
            return std::unexpected(std::move(result.error()));
          if (auto result = script.second->invoke("refreshHumanoidParameters"); !result)
            return std::unexpected(std::move(result.error()));
        }
      }
    }
    auto current_world_result = world();
    if (!current_world_result)
      return std::unexpected(std::move(current_world_result.error()));
    auto& current_world = current_world_result->get();
    if (current_world.isClient() && m_scripted_animator.initialized()) {
      if (auto result = recover_lua_script_error(m_scripted_animator.uninit()); !result)
        return std::unexpected(std::move(result.error()));
      if (auto result = m_scripted_animator.remove_callbacks("animationConfig"); !result)
        return std::unexpected(std::move(result.error()));
      if (auto result = m_scripted_animator.remove_callbacks("entity"); !result)
        return std::unexpected(std::move(result.error()));
      if (auto result = m_scripted_animator.set_scripts(humanoid()->animation_scripts()); !result)
        return std::unexpected(std::move(result.error()));
      auto animation_callbacks_result = LuaBindings::make_scripted_animator_callbacks(humanoid()->networked_animator()->get(),
        [&parameters = m_scripted_animation_parameters](String const& name, Json const& default_value) -> std::expected<Json, StarException> {
          return parameters.value(name, default_value);
        });
      if (!animation_callbacks_result)
        return std::unexpected(std::move(animation_callbacks_result.error()));
      auto animation_callbacks = std::move(*animation_callbacks_result);
      if (auto result = m_scripted_animator.add_callbacks("animationConfig", std::move(animation_callbacks)); !result)
        return std::unexpected(std::move(result.error()));
      auto entity_callbacks_result = LuaBindings::makeEntityCallbacks(*this);
      if (!entity_callbacks_result)
        return std::unexpected(std::move(entity_callbacks_result.error()));
      auto entity_callbacks = std::move(*entity_callbacks_result);
      if (auto result = m_scripted_animator.add_callbacks("entity", std::move(entity_callbacks)); !result)
        return std::unexpected(std::move(result.error()));
      if (auto result = recover_lua_script_error(m_scripted_animator.init(current_world)); !result)
        return std::unexpected(std::move(result.error()));
    }
  }
  if (auto result = update_humanoid_directives(); !result)
    return std::unexpected(std::move(result.error()));
  return humanoid()->prepare_animator_pose();
}

std::expected<void, StarException> Player::set_animation_parameter(String name, Json value) {
  m_scripted_animation_parameters.set(std::move(name), std::move(value));
  return {};
}

}

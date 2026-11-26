import star.json;
import star.json_extra;
import star.data_stream_extra;
import star.color;
import star.bi_map;
import star.list;
import star.image_processing;
import star.hash;
import star.data_stream;
import star.string_view;
import star.thread;
import star.directives;
import star.asset_path;
import star.id_map;
import star.casting;
import star.rpc_promise;
import star.strong_typedef;
import star.game_types;
import star.a_star;
import star.ordered_set;
import star.audio;
import star.map;
import star.set;
import star.string;
import star.random;
import star.periodic_function;
import star.ordered_map;
import star.io_device;
import star.ref_ptr;
import star.logging;
import star.listener;
import std;
import star.format;

import star.lua_root;
import star.animation;
import star.particle;
import star.uuid;
import star.item_descriptor;
import star.drawable;
import star.animated_part_set;
import star.mixer;
import star.light_source;
import star.networked_animator;
import star.humanoid;
import star.damage_types;
import star.world_geometry;
import star.status_types;
import star.damage;
import star.entity;
import star.interaction_types;
import star.tile_damage;
import star.interactive_entity;
import star.collision_block;
import star.tile_entity;
import star.tile_modification;
import star.force_regions;
import star.world;
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
import star.celestial_coordinate;
import star.quest_descriptor;
import star.ai_types;
import star.entity_rendering_types;
import star.entity_rendering;
import star.player_types;
import star.lua_components;
import star.lua_actor_movement_component;
import star.lua_animation_component;
import star.radio_message_database;
import star.player;
import star.asset_source;
import star.assets;
import star.root_base;
import star.configuration;
import star.root;
import star.utility_lua_bindings;
import star.rebuilder;
import star.player_factory;

import star.entity_splash;

import star.net_compatibility;
import star.byte_array;
namespace Star {

std::expected<std::shared_ptr<PlayerConfig>, StarException> PlayerConfig::create(Root& root, JsonObject const& cfg) {
  Json json(cfg);
  auto timing_config = json.get("humanoidTiming", {});
  if (!timing_config)
    return std::unexpected(std::move(timing_config.error()));
  auto timing = Humanoid::HumanoidTiming::create(root, *timing_config);
  if (!timing)
    return std::unexpected(std::move(timing.error()));
  auto config = std::shared_ptr<PlayerConfig>(new PlayerConfig(std::move(*timing)));
  auto identity_config = json.get("defaultHumanoidIdentity", {});
  if (!identity_config)
    return std::unexpected(std::move(identity_config.error()));
  auto identity = HumanoidIdentity::create(*identity_config);
  if (!identity)
    return std::unexpected(std::move(identity.error()));
  config->default_identity = std::move(*identity);
  auto items = json.get_array("defaultItems", JsonArray());
  if (!items)
    return std::unexpected(std::move(items.error()));
  for (auto const& item_json : *items) {
    auto item = ItemDescriptor::create(item_json);
    if (!item)
      return std::unexpected(std::move(item.error()));
    config->default_items.push_back(std::move(*item));
  }
  auto blueprint_config = json.get("defaultBlueprints", JsonObject());
  if (!blueprint_config)
    return std::unexpected(std::move(blueprint_config.error()));
  auto blueprints = blueprint_config->get_array("tier1", JsonArray());
  if (!blueprints)
    return std::unexpected(std::move(blueprints.error()));
  for (auto const& item_json : *blueprints) {
    auto item = ItemDescriptor::create(item_json);
    if (!item)
      return std::unexpected(std::move(item.error()));
    config->default_blueprints.push_back(std::move(*item));
  }
  auto bounds_config = json.get("metaBoundBox");
  if (!bounds_config)
    return std::unexpected(std::move(bounds_config.error()));
  auto bounds = json_to_rectf(*bounds_config);
  if (!bounds)
    return std::unexpected(std::move(bounds.error()));
  config->meta_bound_box = *bounds;
  for (auto const& field : {
      std::pair{std::string_view{"movementParameters"}, std::ref(config->movement_parameters)},
      std::pair{std::string_view{"zeroGMovementParameters"}, std::ref(config->zero_g_movement_parameters)},
      std::pair{std::string_view{"statusControllerSettings"}, std::ref(config->status_controller_settings)},
      std::pair{std::string_view{"companionsConfig"}, std::ref(config->companions_config)},
      std::pair{std::string_view{"deploymentConfig"}, std::ref(config->deployment_config)}}) {
    auto value = json.get(field.first);
    if (!value)
      return std::unexpected(std::move(value.error()));
    field.second.get() = std::move(*value);
  }
  for (auto const& field : {
      std::pair{std::string_view{"footstepTiming"}, std::ref(config->footstep_timing)},
      std::pair{std::string_view{"underwaterMinWaterLevel"}, std::ref(config->underwater_min_water_level)},
      std::pair{std::string_view{"teleportInTime"}, std::ref(config->teleport_in_time)},
      std::pair{std::string_view{"teleportOutTime"}, std::ref(config->teleport_out_time)},
      std::pair{std::string_view{"deployInTime"}, std::ref(config->deploy_in_time)},
      std::pair{std::string_view{"deployOutTime"}, std::ref(config->deploy_out_time)}}) {
    auto value = json.get_float(field.first);
    if (!value)
      return std::unexpected(std::move(value.error()));
    field.second.get() = *value;
  }
  for (auto const& field : {
      std::pair{std::string_view{"footstepSensor"}, std::ref(config->footstep_sensor)},
      std::pair{std::string_view{"underwaterSensor"}, std::ref(config->underwater_sensor)}}) {
    auto value = json.get(field.first);
    if (!value)
      return std::unexpected(std::move(value.error()));
    auto vector = json_to_vec2f(*value);
    if (!vector)
      return std::unexpected(std::move(vector.error()));
    field.second.get() = *vector;
  }
  auto splash_config = json.get("splashConfig");
  if (!splash_config)
    return std::unexpected(std::move(splash_config.error()));
  auto splash = EntitySplashConfig::create(root, *splash_config);
  if (!splash)
    return std::unexpected(std::move(splash.error()));
  config->splash_config = std::move(*splash);
  for (auto const& field : {
      std::pair{std::string_view{"effectsAnimator"}, std::ref(config->effects_animator)},
      std::pair{std::string_view{"bodyMaterialKind"}, std::ref(config->body_material_kind)}}) {
    auto value = json.get_string(field.first);
    if (!value)
      return std::unexpected(std::move(value.error()));
    field.second.get() = std::move(*value);
  }
  auto context_config = json.get("genericScriptContexts");
  if (!context_config)
    return std::unexpected(std::move(context_config.error()));
  auto contexts = context_config->opt_object();
  if (!contexts)
    return std::unexpected(std::move(contexts.error()));
  if (*contexts) {
    for (auto const& entry : **contexts) {
      auto name = entry.second.to_string();
      if (!name)
        return std::unexpected(std::move(name.error()));
      config->generic_script_contexts[entry.first] = std::move(*name);
    }
  }
  return config;
}

PlayerFactory::PlayerFactory(Root& root) : m_root(root) {}

std::expected<std::shared_ptr<PlayerFactory>, StarException> PlayerFactory::create(Root& root) {
  auto factory = std::shared_ptr<PlayerFactory>(new PlayerFactory(root));
  auto rebuilder = Rebuilder::create(root, "player");
  if (!rebuilder)
    return std::unexpected(std::move(rebuilder.error()));
  factory->m_rebuilder = std::move(*rebuilder);
  auto assets = root.assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  auto configJson = (*assets)->json("/player.config");
  if (!configJson)
    return std::unexpected(std::move(configJson.error()));
  auto object = configJson->to_object();
  if (!object)
    return std::unexpected(std::move(object.error()));
  auto config = PlayerConfig::create(root, *object);
  if (!config)
    return std::unexpected(std::move(config.error()));
  factory->m_config = std::move(*config);
  return factory;
}

std::expected<std::shared_ptr<Player>, StarException> PlayerFactory::create() const {
  return Player::create(m_root, m_config);
}

std::expected<std::shared_ptr<Player>, StarException> PlayerFactory::diskLoadPlayer(Json const& diskStore) const {
  auto player = Player::create(m_root, m_config, diskStore);
  if (player)
    return std::move(*player);
  auto lastError = std::move(player.error());
  auto rebuilt = m_rebuilder->rebuild(diskStore, lastError,
      [&](Json const& store) -> std::expected<void, StarException> {
    player = Player::create(m_root, m_config, store);
    if (!player) {
      lastError = player.error();
      return std::unexpected(lastError);
    }
    return {};
  });
  if (!rebuilt)
    return std::unexpected(std::move(rebuilt.error()));
  if (!*rebuilt)
    return std::unexpected(std::move(lastError));
  // Rebuilder reports success only after a successful attempt.
  if (!player)
    return std::unexpected(std::move(player.error()));
  return std::move(*player);
}

std::expected<std::shared_ptr<Player>, StarException> PlayerFactory::net_load_player(ByteArray const& net_store, NetCompatibilityRules rules) const {
  return Player::create(m_root, m_config, net_store, rules);
}

}

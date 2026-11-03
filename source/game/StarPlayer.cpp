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
import star.version;
import star.spline;
import star.lua;
import star.observer_stream;
import star.weighted_pool;
import star.data_stream_devices;

import std;
typedef struct ZSTD_CCtx_s ZSTD_CCtx;
typedef struct ZSTD_DCtx_s ZSTD_DCtx;
typedef ZSTD_DCtx ZSTD_DStream;
typedef ZSTD_CCtx ZSTD_CStream;
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

template <typename Armor, typename SetItem>
std::expected<void, StarException> applyLoungeCosmeticOverride(
    JsonObject const& overrides, String const& key, EquipmentSlot slot, ItemDatabase const& itemDatabase,
    ArmorWearer& armor, SetItem&& setItem) {
  auto overrideJson = overrides.maybe(key);
  if (!overrideJson)
    return {};
  auto descriptor = ItemDescriptor::create(*overrideJson);
  if (!descriptor)
    return std::unexpected(std::move(descriptor.error()));
  auto item = itemDatabase.item(std::move(*descriptor));
  if (!item)
    return std::unexpected(std::move(item.error()));
  if (PlayerInventory::itemAllowedAsEquipment(*item, slot))
    setItem(armor, as<Armor>(*item));
  return {};
}

}

std::expected<EnumMap<Player::State>, StarException> const Player::StateNames = EnumMap<State>::create({
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

Player::Player(Root& rootOwner) : Entity(rootOwner), m_netHumanoid([&rootOwner] { return NetHumanoid::create(rootOwner); }), m_scriptedAnimator(rootOwner) {}

Player::~Player() {
  if (m_inventory)
    m_inventory->setPlayer(std::nullopt);
  if (m_questManager)
    m_questManager->detachPlayer();
}

std::expected<std::shared_ptr<Player>, StarException> Player::create(Root& root, std::shared_ptr<PlayerConfig> config, Uuid uuid) {
  auto player = std::shared_ptr<Player>(new Player(root));
  auto initialized = player->initialize(std::move(config), std::move(uuid));
  if (!initialized)
    return std::unexpected(std::move(initialized.error()));
  return player;
}

std::expected<void, StarException> Player::initialize(std::shared_ptr<PlayerConfig> config, Uuid uuid) {
  auto assetsResult = root().assets();
  if (!assetsResult)
    return std::unexpected(std::move(assetsResult.error()));
  auto assets = std::move(*assetsResult);

  m_config = std::move(config);
  m_client.reset();

  m_state = State::Idle;
  m_emoteState = HumanoidEmote::Idle;

  m_footstepTimer = 0.0f;
  m_teleportTimer = 0.0f;
  m_teleportAnimationType = "default";

  m_shifting = false;

  m_aimPosition = Vec2F();

  setUniqueId(uuid.hex());
  m_identity = m_config->defaultIdentity;
  m_identityUpdated = true;

  auto questManagerResult = QuestManager::create(root(), *this);
  if (!questManagerResult)
    return std::unexpected(std::move(questManagerResult.error()));
  auto questManager = std::move(*questManagerResult);
  m_questManager = std::move(questManager);
  auto toolsResult = ToolUser::create(root());
  if (!toolsResult)
    return std::unexpected(std::move(toolsResult.error()));
  auto tools = std::move(*toolsResult);
  m_tools = std::move(tools);
  auto armorResult = ArmorWearer::create(root());
  if (!armorResult)
    return std::unexpected(std::move(armorResult.error()));
  auto armor = std::move(*armorResult);
  m_armor = std::move(armor);
  m_companions = std::make_shared<PlayerCompanions>(root(), m_config->companionsConfig);

  for (auto& p : m_config->genericScriptContexts) {
    auto scriptComponent = std::make_shared<GenericScriptComponent>(root());
    if (auto result = scriptComponent->setScript(p.second); !result)
      return std::unexpected(std::move(result.error()));
    m_genericScriptContexts.set(p.first, scriptComponent);
  }

  // all of these are defaults and won't include the correct humanoid config for the species
  if (auto result = installHumanoid(); !result)
    return std::unexpected(std::move(result.error()));
  auto movementParametersResult = ActorMovementParameters::create(jsonMerge(humanoid()->defaultMovementParameters(), humanoid()->playerMovementParameters().value_or(m_config->movementParameters)));
  if (!movementParametersResult)
    return std::unexpected(std::move(movementParametersResult.error()));
  auto movementParameters = std::move(*movementParametersResult);
  if (!movementParameters.physicsEffectCategories)
    movementParameters.physicsEffectCategories = StringSet({"player"});
  auto movementControllerResult = ActorMovementController::create(root(), movementParameters);
  if (!movementControllerResult)
    return std::unexpected(std::move(movementControllerResult.error()));
  auto movementController = std::move(*movementControllerResult);
  m_movementController = std::move(movementController);
  auto zeroGMovementParametersResult = ActorMovementParameters::create(m_config->zeroGMovementParameters);
  if (!zeroGMovementParametersResult)
    return std::unexpected(std::move(zeroGMovementParametersResult.error()));
  auto zeroGMovementParameters = std::move(*zeroGMovementParametersResult);
  m_zeroGMovementParameters = std::move(zeroGMovementParameters);

  auto techControllerResult = TechController::create(root());
  if (!techControllerResult)
    return std::unexpected(std::move(techControllerResult.error()));
  auto techController = std::move(*techControllerResult);
  m_techController = std::move(techController);
  auto statusControllerResult = StatusController::create(root(), m_config->statusControllerSettings);
  if (!statusControllerResult)
    return std::unexpected(std::move(statusControllerResult.error()));
  auto statusController = std::move(*statusControllerResult);
  m_statusController = std::move(statusController);
  m_deployment = std::make_shared<PlayerDeployment>(root(), m_config->deploymentConfig);

  auto inventoryResult = PlayerInventory::create(root());
  if (!inventoryResult)
    return std::unexpected(std::move(inventoryResult.error()));
  auto inventory = std::move(*inventoryResult);
  m_inventory = std::move(inventory);
  m_inventory->setPlayer(std::ref(*this));

  m_blueprints = std::make_shared<PlayerBlueprints>();
  m_universeMap = std::make_shared<PlayerUniverseMap>();
  m_codexes = std::make_shared<PlayerCodexes>(root());
  m_techs = std::make_shared<PlayerTech>(root());
  m_log = std::make_shared<PlayerLog>();

  if (auto result = setModeType(PlayerMode::Casual); !result)
    return std::unexpected(std::move(result.error()));

  m_useDown = false;
  m_edgeTriggeredUse = false;
  setTeam(EntityDamageTeam(TeamType::Friendly));

  auto footstepVolumeVarianceJsonResult = assets->json("/sfx.config:footstepVolumeVariance");
  if (!footstepVolumeVarianceJsonResult)
    return std::unexpected(std::move(footstepVolumeVarianceJsonResult.error()));
  auto footstepVolumeVarianceJson = std::move(*footstepVolumeVarianceJsonResult);
  auto footstepVolumeVarianceResult = footstepVolumeVarianceJson.toFloat();
  if (!footstepVolumeVarianceResult)
    return std::unexpected(std::move(footstepVolumeVarianceResult.error()));
  auto footstepVolumeVariance = std::move(*footstepVolumeVarianceResult);
  m_footstepVolumeVariance = footstepVolumeVariance;
  auto landingVolumeJsonResult = assets->json("/sfx.config:landingVolume");
  if (!landingVolumeJsonResult)
    return std::unexpected(std::move(landingVolumeJsonResult.error()));
  auto landingVolumeJson = std::move(*landingVolumeJsonResult);
  auto landingVolumeResult = landingVolumeJson.toFloat();
  if (!landingVolumeResult)
    return std::unexpected(std::move(landingVolumeResult.error()));
  auto landingVolume = std::move(*landingVolumeResult);
  m_landingVolume = landingVolume;

  auto effectsConfigResult = assets->fetchJson(m_config->effectsAnimator);
  if (!effectsConfigResult)
    return std::unexpected(std::move(effectsConfigResult.error()));
  auto effectsConfig = std::move(*effectsConfigResult);
  auto effectsAnimatorResult = NetworkedAnimator::create(root(), effectsConfig);
  if (!effectsAnimatorResult)
    return std::unexpected(std::move(effectsAnimatorResult.error()));
  auto effectsAnimator = std::move(*effectsAnimatorResult);
  m_effectsAnimator = std::move(effectsAnimator);
  auto emitterResult = EffectEmitter::create(root());
  if (!emitterResult)
    return std::unexpected(std::move(emitterResult.error()));
  auto emitter = std::move(*emitterResult);
  m_effectEmitter = std::move(emitter);

  auto interactRadiusJsonResult = assets->json("/player.config:interactRadius");
  if (!interactRadiusJsonResult)
    return std::unexpected(std::move(interactRadiusJsonResult.error()));
  auto interactRadiusJson = std::move(*interactRadiusJsonResult);
  auto interactRadiusResult = interactRadiusJson.toFloat();
  if (!interactRadiusResult)
    return std::unexpected(std::move(interactRadiusResult.error()));
  auto interactRadius = std::move(*interactRadiusResult);
  m_interactRadius = interactRadius;

  auto walkIntoInteractBiasJsonResult = assets->json("/player.config:walkIntoInteractBias");
  if (!walkIntoInteractBiasJsonResult)
    return std::unexpected(std::move(walkIntoInteractBiasJsonResult.error()));
  auto walkIntoInteractBiasJson = std::move(*walkIntoInteractBiasJsonResult);
  auto walkIntoInteractBiasResult = jsonToVec2F(walkIntoInteractBiasJson);
  if (!walkIntoInteractBiasResult)
    return std::unexpected(std::move(walkIntoInteractBiasResult.error()));
  auto walkIntoInteractBias = std::move(*walkIntoInteractBiasResult);
  m_walkIntoInteractBias = walkIntoInteractBias;

  if (m_landingVolume <= 1)
    m_landingVolume = 6;

  m_isAdmin = false;

  auto emoteCooldownJsonResult = assets->json("/player.config:emoteCooldown");
  if (!emoteCooldownJsonResult)
    return std::unexpected(std::move(emoteCooldownJsonResult.error()));
  auto emoteCooldownJson = std::move(*emoteCooldownJsonResult);
  auto emoteCooldownResult = emoteCooldownJson.toFloat();
  if (!emoteCooldownResult)
    return std::unexpected(std::move(emoteCooldownResult.error()));
  auto emoteCooldown = std::move(*emoteCooldownResult);
  m_emoteCooldown = emoteCooldown;
  auto blinkIntervalJsonResult = assets->json("/player.config:blinkInterval");
  if (!blinkIntervalJsonResult)
    return std::unexpected(std::move(blinkIntervalJsonResult.error()));
  auto blinkIntervalJson = std::move(*blinkIntervalJsonResult);
  auto blinkIntervalResult = jsonToVec2F(blinkIntervalJson);
  if (!blinkIntervalResult)
    return std::unexpected(std::move(blinkIntervalResult.error()));
  auto blinkInterval = std::move(*blinkIntervalResult);
  m_blinkInterval = blinkInterval;

  m_emoteCooldownTimer = GameTimer(m_emoteCooldown);
  m_blinkCooldownTimer = GameTimer(0);

  m_chatMessageChanged = false;
  m_chatMessageUpdated = false;

  m_interruptRadioMessage = false;

  auto songbookResult = Songbook::create(root(), species());
  if (!songbookResult)
    return std::unexpected(std::move(songbookResult.error()));
  auto songbook = std::move(*songbookResult);
  m_songbook = std::move(songbook);

  m_lastDamagedOtherTimer = 0;
  m_lastDamagedTarget = NullEntityId;

  auto ageItemsEveryJsonResult = assets->json("/player.config:ageItemsEvery");
  if (!ageItemsEveryJsonResult)
    return std::unexpected(std::move(ageItemsEveryJsonResult.error()));
  auto ageItemsEveryJson = std::move(*ageItemsEveryJsonResult);
  auto ageItemsEveryResult = ageItemsEveryJson.toFloat();
  if (!ageItemsEveryResult)
    return std::unexpected(std::move(ageItemsEveryResult.error()));
  auto ageItemsEvery = std::move(*ageItemsEveryResult);
  m_ageItemsTimer = GameTimer(ageItemsEvery);

  if (auto result = refreshEquipment(); !result)
    return std::unexpected(std::move(result.error()));

  auto foodLowThresholdJsonResult = assets->json("/player.config:foodLowThreshold");
  if (!foodLowThresholdJsonResult)
    return std::unexpected(std::move(foodLowThresholdJsonResult.error()));
  auto foodLowThresholdJson = std::move(*foodLowThresholdJsonResult);
  auto foodLowThresholdResult = foodLowThresholdJson.toFloat();
  if (!foodLowThresholdResult)
    return std::unexpected(std::move(foodLowThresholdResult.error()));
  auto foodLowThreshold = std::move(*foodLowThresholdResult);
  m_foodLowThreshold = foodLowThreshold;
  auto foodLowStatusEffectsJsonResult = assets->json("/player.config:foodLowStatusEffects");
  if (!foodLowStatusEffectsJsonResult)
    return std::unexpected(std::move(foodLowStatusEffectsJsonResult.error()));
  auto foodLowStatusEffectsJson = std::move(*foodLowStatusEffectsJsonResult);
  auto foodLowStatusEffectsArrayResult = foodLowStatusEffectsJson.toArray();
  if (!foodLowStatusEffectsArrayResult)
    return std::unexpected(std::move(foodLowStatusEffectsArrayResult.error()));
  auto foodLowStatusEffectsArray = std::move(*foodLowStatusEffectsArrayResult);
  for (auto const& entry : foodLowStatusEffectsArray) {
    auto effectResult = jsonToPersistentStatusEffect(entry);
    if (!effectResult)
      return std::unexpected(std::move(effectResult.error()));
    auto effect = std::move(*effectResult);
    m_foodLowStatusEffects.append(std::move(effect));
  }
  auto foodEmptyStatusEffectsJsonResult = assets->json("/player.config:foodEmptyStatusEffects");
  if (!foodEmptyStatusEffectsJsonResult)
    return std::unexpected(std::move(foodEmptyStatusEffectsJsonResult.error()));
  auto foodEmptyStatusEffectsJson = std::move(*foodEmptyStatusEffectsJsonResult);
  auto foodEmptyStatusEffectsArrayResult = foodEmptyStatusEffectsJson.toArray();
  if (!foodEmptyStatusEffectsArrayResult)
    return std::unexpected(std::move(foodEmptyStatusEffectsArrayResult.error()));
  auto foodEmptyStatusEffectsArray = std::move(*foodEmptyStatusEffectsArrayResult);
  for (auto const& entry : foodEmptyStatusEffectsArray) {
    auto effectResult = jsonToPersistentStatusEffect(entry);
    if (!effectResult)
      return std::unexpected(std::move(effectResult.error()));
    auto effect = std::move(*effectResult);
    m_foodEmptyStatusEffects.append(std::move(effect));
  }

  auto inCinematicStatusEffectsJsonResult = assets->json("/player.config:inCinematicStatusEffects");
  if (!inCinematicStatusEffectsJsonResult)
    return std::unexpected(std::move(inCinematicStatusEffectsJsonResult.error()));
  auto inCinematicStatusEffectsJson = std::move(*inCinematicStatusEffectsJsonResult);
  auto inCinematicStatusEffectsArrayResult = inCinematicStatusEffectsJson.toArray();
  if (!inCinematicStatusEffectsArrayResult)
    return std::unexpected(std::move(inCinematicStatusEffectsArrayResult.error()));
  auto inCinematicStatusEffectsArray = std::move(*inCinematicStatusEffectsArrayResult);
  for (auto const& entry : inCinematicStatusEffectsArray) {
    auto effectResult = jsonToPersistentStatusEffect(entry);
    if (!effectResult)
      return std::unexpected(std::move(effectResult.error()));
    auto effect = std::move(*effectResult);
    m_inCinematicStatusEffects.append(std::move(effect));
  }

  if (auto result = synchronizeEquipmentStatusEffects(*m_armor, *m_tools, *m_statusController); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = m_statusController->resetAllResources(); !result)
    return std::unexpected(std::move(result.error()));

  m_landingNoisePending = false;
  m_footstepPending = false;

  setKeepAlive(true);

  {
    if (auto result = m_netGroup.addNetElement(m_stateNetState); !result)
      return std::unexpected(std::move(result.error()));
  }
  {
    if (auto result = m_netGroup.addNetElement(m_shiftingNetState); !result)
      return std::unexpected(std::move(result.error()));
  }
  {
    if (auto result = m_netGroup.addNetElement(m_xAimPositionNetState); !result)
      return std::unexpected(std::move(result.error()));
  }
  {
    if (auto result = m_netGroup.addNetElement(m_yAimPositionNetState); !result)
      return std::unexpected(std::move(result.error()));
  }
  {
    if (auto result = m_netGroup.addNetElement(m_identityNetState); !result)
      return std::unexpected(std::move(result.error()));
  }
  {
    if (auto result = m_netGroup.addNetElement(m_teamNetState); !result)
      return std::unexpected(std::move(result.error()));
  }
  {
    if (auto result = m_netGroup.addNetElement(m_landedNetState); !result)
      return std::unexpected(std::move(result.error()));
  }
  {
    if (auto result = m_netGroup.addNetElement(m_chatMessageNetState); !result)
      return std::unexpected(std::move(result.error()));
  }
  {
    if (auto result = m_netGroup.addNetElement(m_newChatMessageNetState); !result)
      return std::unexpected(std::move(result.error()));
  }
  {
    if (auto result = m_netGroup.addNetElement(m_emoteNetState); !result)
      return std::unexpected(std::move(result.error()));
  }

  m_xAimPositionNetState.setFixedPointBase(0.003125);
  m_yAimPositionNetState.setFixedPointBase(0.003125);
  m_yAimPositionNetState.setInterpolator(lerp<float, float>);

  {
    if (auto result = m_netGroup.addNetElement(*m_inventory); !result)
      return std::unexpected(std::move(result.error()));
  }
  {
    if (auto result = m_netGroup.addNetElement(*m_tools); !result)
      return std::unexpected(std::move(result.error()));
  }
  {
    if (auto result = m_netGroup.addNetElement(*m_armor); !result)
      return std::unexpected(std::move(result.error()));
  }
  {
    if (auto result = m_netGroup.addNetElement(*m_songbook); !result)
      return std::unexpected(std::move(result.error()));
  }
  {
    if (auto result = m_netGroup.addNetElement(*m_movementController); !result)
      return std::unexpected(std::move(result.error()));
  }
  {
    if (auto result = m_netGroup.addNetElement(*m_effectEmitter); !result)
      return std::unexpected(std::move(result.error()));
  }
  {
    if (auto result = m_netGroup.addNetElement(*m_effectsAnimator); !result)
      return std::unexpected(std::move(result.error()));
  }
  {
    if (auto result = m_netGroup.addNetElement(*m_statusController); !result)
      return std::unexpected(std::move(result.error()));
  }
  {
    if (auto result = m_netGroup.addNetElement(*m_techController); !result)
      return std::unexpected(std::move(result.error()));
  }

  m_netHumanoid.setCompatibilityVersion(10);
  {
    if (auto result = m_netGroup.addNetElement(m_netHumanoid); !result)
      return std::unexpected(std::move(result.error()));
  }
  m_refreshedHumanoidParameters.setCompatibilityVersion(10);
  {
    if (auto result = m_netGroup.addNetElement(m_refreshedHumanoidParameters); !result)
      return std::unexpected(std::move(result.error()));
  }

  m_scriptedAnimationParameters.setCompatibilityVersion(10);
  {
    if (auto result = m_netGroup.addNetElement(m_scriptedAnimationParameters); !result)
      return std::unexpected(std::move(result.error()));
  }

  m_deathParticleBurst.setCompatibilityVersion(10);
  {
    if (auto result = m_netGroup.addNetElement(m_deathParticleBurst); !result)
      return std::unexpected(std::move(result.error()));
  }

  m_humanoidDanceNetState.setCompatibilityVersion(13);
  {
    if (auto result = m_netGroup.addNetElement(m_humanoidDanceNetState); !result)
      return std::unexpected(std::move(result.error()));
  }

  m_netGroup.setNeedsLoadCallback(std::bind(&Player::getNetStates, std::ref(*this), std::placeholders::_1));
  m_netGroup.setNeedsStoreCallback(std::bind(&Player::setNetStates, std::ref(*this)));
  return {};
}

std::expected<std::shared_ptr<Player>, StarException> Player::create(Root& rootOwner, std::shared_ptr<PlayerConfig> config, ByteArray const& netStore, NetCompatibilityRules rules) {
  auto player = create(rootOwner, std::move(config));
  if (!player)
    return std::unexpected(std::move(player.error()));
  auto loaded = (*player)->netLoad(netStore, rules);
  if (!loaded)
    return std::unexpected(std::move(loaded.error()));
  return player;
}

std::expected<void, StarException> Player::netLoad(ByteArray const& netStore, NetCompatibilityRules rules) {
  DataStreamBuffer ds(netStore);
  ds.setStreamCompatibilityVersion(rules);
  auto idResult = ds.read<String>();
  if (!idResult)
    return std::unexpected(std::move(idResult.error()));
  auto id = std::move(*idResult);
  setUniqueId(std::move(id));
  if (auto result = ds.read(m_description); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = ds.read(m_modeType); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = ds.read(m_identity); !result)
    return std::unexpected(std::move(result.error()));
  if (rules.version() >= 10) {
    if (auto result = ds.read(m_humanoidParameters); !result)
      return std::unexpected(std::move(result.error()));
  }
  if (auto result = installHumanoid(); !result)
    return std::unexpected(std::move(result.error()));
  auto movementParametersResult = ActorMovementParameters::create(jsonMerge(humanoid()->defaultMovementParameters(), humanoid()->playerMovementParameters().value_or(m_config->movementParameters)));
  if (!movementParametersResult)
    return std::unexpected(std::move(movementParametersResult.error()));
  auto movementParameters = std::move(*movementParametersResult);
  if (auto result = m_movementController->resetBaseParameters(movementParameters); !result)
    return std::unexpected(std::move(result.error()));
  m_deathParticleBurst.set(humanoid()->defaultDeathParticles());
  return {};
}

std::expected<void, StarException> Player::installHumanoid() {
  auto currentResult = NetHumanoid::create(root(), m_identity, m_humanoidParameters, Json());
  if (!currentResult)
    return std::unexpected(std::move(currentResult.error()));
  auto current = std::move(*currentResult);
  m_netHumanoid.clearNetElements();
  if (auto result = m_netHumanoid.addNetElement(current); !result)
    return std::unexpected(std::move(result.error()));
  m_currentNetHumanoid = std::move(current);
  return {};
}


std::expected<std::shared_ptr<Player>, StarException> Player::create(Root& rootOwner, std::shared_ptr<PlayerConfig> config, Json const& diskStore) {
  auto player = create(rootOwner, std::move(config));
  if (!player)
    return std::unexpected(std::move(player.error()));
  auto loaded = (*player)->diskLoad(diskStore);
  if (!loaded)
    return std::unexpected(std::move(loaded.error()));
  return player;
}

std::expected<void, StarException> Player::diskLoad(Json const& diskStore) {
  auto idResult = diskStore.getString("uuid");
  if (!idResult)
    return std::unexpected(std::move(idResult.error()));
  auto id = std::move(*idResult);
  setUniqueId(std::move(id));
  auto descriptionResult = diskStore.getString("description");
  if (!descriptionResult)
    return std::unexpected(std::move(descriptionResult.error()));
  auto description = std::move(*descriptionResult);
  m_description = std::move(description);
  auto modeNameResult = diskStore.getString("modeType");
  if (!modeNameResult)
    return std::unexpected(std::move(modeNameResult.error()));
  auto modeName = std::move(*modeNameResult);
  if (!PlayerModeNames)
    return std::unexpected(PlayerModeNames.error());
  auto modeResult = PlayerModeNames->getLeft(modeName);
  if (!modeResult)
    return std::unexpected(std::move(modeResult.error()));
  auto const& mode = modeResult->get();
  if (auto result = setModeType(mode); !result)
    return std::unexpected(std::move(result.error()));
  auto shipResult = diskStore.get("shipUpgrades");
  if (!shipResult)
    return std::unexpected(std::move(shipResult.error()));
  auto ship = std::move(*shipResult);
  auto shipUpgradesResult = ShipUpgrades::create(ship);
  if (!shipUpgradesResult)
    return std::unexpected(std::move(shipUpgradesResult.error()));
  auto shipUpgrades = std::move(*shipUpgradesResult);
  m_shipUpgrades = std::move(shipUpgrades);
  auto blueprintsJsonResult = diskStore.get("blueprints");
  if (!blueprintsJsonResult)
    return std::unexpected(std::move(blueprintsJsonResult.error()));
  auto blueprintsJson = std::move(*blueprintsJsonResult);
  auto blueprintsResult = PlayerBlueprints::create(blueprintsJson);
  if (!blueprintsResult)
    return std::unexpected(std::move(blueprintsResult.error()));
  auto blueprints = std::move(*blueprintsResult);
  m_blueprints = std::make_shared<PlayerBlueprints>(std::move(blueprints));
  auto universeMapJsonResult = diskStore.get("universeMap");
  if (!universeMapJsonResult)
    return std::unexpected(std::move(universeMapJsonResult.error()));
  auto universeMapJson = std::move(*universeMapJsonResult);
  auto universeMapResult = PlayerUniverseMap::create(universeMapJson);
  if (!universeMapResult)
    return std::unexpected(std::move(universeMapResult.error()));
  auto universeMap = std::move(*universeMapResult);
  m_universeMap = std::make_shared<PlayerUniverseMap>(std::move(universeMap));
  if (m_clientContext)
    m_universeMap->setServerUuid(m_clientContext->serverUuid());
  auto codexesJsonResult = diskStore.get("codexes");
  if (!codexesJsonResult)
    return std::unexpected(std::move(codexesJsonResult.error()));
  auto codexesJson = std::move(*codexesJsonResult);
  auto codexesResult = PlayerCodexes::create(root(), codexesJson);
  if (!codexesResult)
    return std::unexpected(std::move(codexesResult.error()));
  auto codexes = std::move(*codexesResult);
  m_codexes = std::move(codexes);
  auto techsJsonResult = diskStore.get("techs");
  if (!techsJsonResult)
    return std::unexpected(std::move(techsJsonResult.error()));
  auto techsJson = std::move(*techsJsonResult);
  auto techsResult = PlayerTech::create(root(), techsJson);
  if (!techsResult)
    return std::unexpected(std::move(techsResult.error()));
  auto techs = std::move(*techsResult);
  m_techs = std::move(techs);
  auto identityJsonResult = diskStore.get("identity");
  if (!identityJsonResult)
    return std::unexpected(std::move(identityJsonResult.error()));
  auto identityJson = std::move(*identityJsonResult);
  auto identityResult = HumanoidIdentity::create(identityJson);
  if (!identityResult)
    return std::unexpected(std::move(identityResult.error()));
  auto identity = std::move(*identityResult);
  m_identity = std::move(identity);
  m_identityUpdated = true;
  auto teamJsonResult = diskStore.get("team");
  if (!teamJsonResult)
    return std::unexpected(std::move(teamJsonResult.error()));
  auto teamJson = std::move(*teamJsonResult);
  auto teamResult = EntityDamageTeam::create(teamJson);
  if (!teamResult)
    return std::unexpected(std::move(teamResult.error()));
  auto team = std::move(*teamResult);
  setTeam(std::move(team));
  m_state = State::Idle;
  auto inventoryResult = diskStore.get("inventory");
  if (!inventoryResult)
    return std::unexpected(std::move(inventoryResult.error()));
  auto inventory = std::move(*inventoryResult);
  if (auto result = m_inventory->load(inventory); !result)
    return std::unexpected(std::move(result.error()));
  auto movementResult = diskStore.get("movementController");
  if (!movementResult)
    return std::unexpected(std::move(movementResult.error()));
  auto movement = std::move(*movementResult);
  if (auto result = m_movementController->loadState(movement); !result)
    return std::unexpected(std::move(result.error()));
  auto techControllerResult = diskStore.get("techController");
  if (!techControllerResult)
    return std::unexpected(std::move(techControllerResult.error()));
  auto techController = std::move(*techControllerResult);
  if (auto result = m_techController->diskLoad(techController); !result)
    return std::unexpected(std::move(result.error()));
  auto statusControllerResult = diskStore.get("statusController");
  if (!statusControllerResult)
    return std::unexpected(std::move(statusControllerResult.error()));
  auto statusController = std::move(*statusControllerResult);
  if (auto result = m_statusController->diskLoad(statusController); !result)
    return std::unexpected(std::move(result.error()));
  auto logJsonResult = diskStore.get("log");
  if (!logJsonResult)
    return std::unexpected(std::move(logJsonResult.error()));
  auto logJson = std::move(*logJsonResult);
  auto logResult = PlayerLog::create(logJson);
  if (!logResult)
    return std::unexpected(std::move(logResult.error()));
  auto log = std::move(*logResult);
  m_log = std::make_shared<PlayerLog>(std::move(log));
  auto speciesDatabaseResult = root().speciesDatabase();
  if (!speciesDatabaseResult)
    return std::unexpected(std::move(speciesDatabaseResult.error()));
  auto speciesDatabase = std::move(*speciesDatabaseResult);
  auto speciesDefResult = speciesDatabase->species(m_identity.species);
  if (!speciesDefResult)
    return std::unexpected(std::move(speciesDefResult.error()));
  auto speciesDef = std::move(*speciesDefResult);
  auto shipSpeciesResult = diskStore.getString("shipSpecies", m_identity.species);
  if (!shipSpeciesResult)
    return std::unexpected(std::move(shipSpeciesResult.error()));
  auto shipSpecies = std::move(*shipSpeciesResult);
  m_shipSpecies = std::move(shipSpecies);
  auto questsResult = diskStore.get("quests", JsonObject{});
  if (!questsResult)
    return std::unexpected(std::move(questsResult.error()));
  auto quests = std::move(*questsResult);
  if (auto result = m_questManager->diskLoad(quests); !result)
    return std::unexpected(std::move(result.error()));
  auto companionsResult = diskStore.get("companions", JsonObject{});
  if (!companionsResult)
    return std::unexpected(std::move(companionsResult.error()));
  auto companions = std::move(*companionsResult);
  if (auto result = m_companions->diskLoad(companions); !result)
    return std::unexpected(std::move(result.error()));
  auto deploymentResult = diskStore.get("deployment", JsonObject{});
  if (!deploymentResult)
    return std::unexpected(std::move(deploymentResult.error()));
  auto deployment = std::move(*deploymentResult);
  if (auto result = m_deployment->diskLoad(deployment); !result)
    return std::unexpected(std::move(result.error()));
  auto parametersResult = diskStore.getObject("humanoidParameters", JsonObject());
  if (!parametersResult)
    return std::unexpected(std::move(parametersResult.error()));
  auto parameters = std::move(*parametersResult);
  m_humanoidParameters = std::move(parameters);
  if (auto result = installHumanoid(); !result)
    return std::unexpected(std::move(result.error()));
  auto movementParametersResult = ActorMovementParameters::create(jsonMerge(humanoid()->defaultMovementParameters(), humanoid()->playerMovementParameters().value_or(m_config->movementParameters)));
  if (!movementParametersResult)
    return std::unexpected(std::move(movementParametersResult.error()));
  auto movementParameters = std::move(*movementParametersResult);
  if (auto result = m_movementController->resetBaseParameters(movementParameters); !result)
    return std::unexpected(std::move(result.error()));
  m_effectsAnimator->setGlobalTag("effectDirectives", speciesDef->effectDirectives());
  m_deathParticleBurst.set(humanoid()->defaultDeathParticles());
  auto propertiesResult = diskStore.getObject("genericProperties");
  if (!propertiesResult)
    return std::unexpected(std::move(propertiesResult.error()));
  auto properties = std::move(*propertiesResult);
  m_genericProperties = std::move(properties);
  m_armor->reset();
  if (auto result = refreshArmor(); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = setNetArmorSecrets(true); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = m_codexes->learnInitialCodexes(species()); !result)
    return std::unexpected(std::move(result.error()));
  auto aiJsonResult = diskStore.get("aiState", JsonObject{});
  if (!aiJsonResult)
    return std::unexpected(std::move(aiJsonResult.error()));
  auto aiJson = std::move(*aiJsonResult);
  auto aiStateResult = AiState::create(aiJson);
  if (!aiStateResult)
    return std::unexpected(std::move(aiStateResult.error()));
  auto aiState = std::move(*aiStateResult);
  m_aiState = std::move(aiState);
  for (auto& script : m_genericScriptContexts)
    {
      if (auto result = script.second->setScriptStorage({}); !result)
        return std::unexpected(std::move(result.error()));
    }
  auto storageJsonResult = diskStore.get("genericScriptStorage", JsonObject{});
  if (!storageJsonResult)
    return std::unexpected(std::move(storageJsonResult.error()));
  auto storageJson = std::move(*storageJsonResult);
  auto storageResult = storageJson.toObject();
  if (!storageResult)
    return std::unexpected(std::move(storageResult.error()));
  auto storage = std::move(*storageResult);
  for (auto& p : storage) {
    if (auto script = m_genericScriptContexts.maybe(p.first).value_or(std::shared_ptr<GenericScriptComponent>{})) {
      auto objectResult = p.second.toObject();
      if (!objectResult)
        return std::unexpected(std::move(objectResult.error()));
      auto object = std::move(*objectResult);
      if (auto result = script->setScriptStorage(std::move(object)); !result)
        return std::unexpected(std::move(result.error()));
    }
  }
  for (auto const& descriptor : m_config->defaultBlueprints)
    m_blueprints->add(descriptor);
  for (auto const& descriptor : speciesDef->defaultBlueprints())
    m_blueprints->add(descriptor);
  if (m_identity.gender == Gender::Male && m_description == "This gal seems to have nothing to say for herself.")
    m_description = "This guy seems to have nothing to say for himself.";
  else if (m_identity.gender == Gender::Female && m_description == "This guy seems to have nothing to say for himself.")
    m_description = "This gal seems to have nothing to say for herself.";
  return {};
}

std::shared_ptr<ClientContext> Player::clientContext() const {
  return m_clientContext;
}

void Player::setClientContext(std::shared_ptr<ClientContext> clientContext) {
  m_clientContext = std::move(clientContext);
  if (m_clientContext && m_universeMap)
    m_universeMap->setServerUuid(m_clientContext->serverUuid());
}

std::shared_ptr<Statistics> Player::statistics() const {
  return m_statistics;
}

void Player::setStatistics(std::shared_ptr<Statistics> statistics) {
  m_statistics = statistics;
}

void Player::setUniverseClient(std::optional<std::reference_wrapper<UniverseClient>> client) {
  m_client = client;
  m_questManager->setUniverseClient(client);
}

std::optional<std::reference_wrapper<UniverseClient>> Player::universeClient() const {
  return m_client;
}

EntityType Player::entityType() const {
  return EntityType::Player;
}

ClientEntityMode Player::clientEntityMode() const {
  return ClientEntityMode::ClientPresenceMaster;
}

std::expected<void, StarException> Player::init(World& world, EntityId entityId, EntityMode mode) {
  m_controlMovementState.reset();
  m_movementAnimationState.reset();
  m_movementController->setNetworkMovingDirection({});
  if (auto result = Entity::init(world, entityId, mode); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = m_tools->init(*this); !result)
    return std::unexpected(std::move(result.error()));
  m_movementController->init(world);
  m_movementController->setIgnorePhysicsEntities({entityId});
  if (auto result = m_statusController->init(*this, *m_movementController); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = m_techController->init(*this, *m_movementController, *m_statusController); !result)
    return std::unexpected(std::move(result.error()));
  auto speciesDatabaseResult = root().speciesDatabase();
  if (!speciesDatabaseResult)
    return std::unexpected(std::move(speciesDatabaseResult.error()));
  auto speciesDatabase = std::move(*speciesDatabaseResult);
  auto speciesDefinitionResult = speciesDatabase->species(m_identity.species);
  if (!speciesDefinitionResult)
    return std::unexpected(std::move(speciesDefinitionResult.error()));
  auto speciesDefinition = std::move(*speciesDefinitionResult);
  if (mode == EntityMode::Master) {
    m_scriptedAnimationParameters.clear();
    m_movementController->setRotation(0);
    m_statusController->setStatusProperty("ouchNoise", speciesDefinition->ouchNoise(m_identity.gender));
    m_emoteState = HumanoidEmote::Idle;
    if (auto result = m_questManager->init(*this, world); !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = m_companions->init(*this, world); !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = m_deployment->init(*this, world); !result)
      return std::unexpected(std::move(result.error()));
    m_missionRadioMessages.clear();
    if (auto result = m_statusController->setPersistentEffects("species", speciesDefinition->statusEffects()); !result)
      return std::unexpected(std::move(result.error()));
    for (auto& p : m_genericScriptContexts) {
      if (auto result = p.second->addActorMovementCallbacks(*m_movementController); !result)
        return std::unexpected(std::move(result.error()));
      auto playerCallbacksResult = LuaBindings::makePlayerCallbacks(root(), *this);
      if (!playerCallbacksResult)
        return std::unexpected(std::move(playerCallbacksResult.error()));
      auto playerCallbacks = std::move(*playerCallbacksResult);
      if (auto result = p.second->addCallbacks("player", std::move(playerCallbacks)); !result)
        return std::unexpected(std::move(result.error()));
      auto entityCallbacksResult = LuaBindings::makeEntityCallbacks(*this);
      if (!entityCallbacksResult)
        return std::unexpected(std::move(entityCallbacksResult.error()));
      auto entityCallbacks = std::move(*entityCallbacksResult);
      if (auto result = p.second->addCallbacks("entity", std::move(entityCallbacks)); !result)
        return std::unexpected(std::move(result.error()));
      auto statusCallbacksResult = LuaBindings::makeStatusControllerCallbacks(*m_statusController);
      if (!statusCallbacksResult)
        return std::unexpected(std::move(statusCallbacksResult.error()));
      auto statusCallbacks = std::move(*statusCallbacksResult);
      if (auto result = p.second->addCallbacks("status", std::move(statusCallbacks)); !result)
        return std::unexpected(std::move(result.error()));
      auto songbookCallbacksResult = LuaBindings::makeSongbookCallbacks(*m_songbook);
      if (!songbookCallbacksResult)
        return std::unexpected(std::move(songbookCallbacksResult.error()));
      auto songbookCallbacks = std::move(*songbookCallbacksResult);
      if (auto result = p.second->addCallbacks("songbook", std::move(songbookCallbacks)); !result)
        return std::unexpected(std::move(result.error()));
      auto animatorCallbacksResult = LuaBindings::makeNetworkedAnimatorCallbacks(humanoid()->networkedAnimator()->get());
      if (!animatorCallbacksResult)
        return std::unexpected(std::move(animatorCallbacksResult.error()));
      auto animatorCallbacks = std::move(*animatorCallbacksResult);
      if (auto result = p.second->addCallbacks("animator", std::move(animatorCallbacks)); !result)
        return std::unexpected(std::move(result.error()));
      if (m_client) {
        auto celestialCallbacksResult = LuaBindings::makeCelestialCallbacks(root(), m_client->get());
        if (!celestialCallbacksResult)
          return std::unexpected(std::move(celestialCallbacksResult.error()));
        auto celestialCallbacks = std::move(*celestialCallbacksResult);
        if (auto result = p.second->addCallbacks("celestial", std::move(celestialCallbacks)); !result)
          return std::unexpected(std::move(result.error()));
      }
      if (auto result = p.second->init(world); !result)
        return std::unexpected(std::move(result.error()));
    }
    for (auto& p : m_inventory->pullOverflow()) {
      auto dropResult = ItemDrop::createRandomizedDrop(root(), p, m_movementController->position(), true);
      if (!dropResult)
        return std::unexpected(std::move(dropResult.error()));
      auto drop = std::move(*dropResult);
      if (auto result = world.addEntity(std::move(drop)); !result)
        return std::unexpected(std::move(result.error()));
    }
    if (auto result = setNetArmorSecrets(); !result)
      return std::unexpected(std::move(result.error()));
  }
  if (world.isClient()) {
    if (auto result = m_scriptedAnimator.setScripts(humanoid()->animationScripts()); !result)
      return std::unexpected(std::move(result.error()));
    auto animationCallbacksResult = LuaBindings::makeScriptedAnimatorCallbacks(humanoid()->networkedAnimator()->get(),
      [&parameters = m_scriptedAnimationParameters](String const& name, Json const& defaultValue) -> std::expected<Json, StarException> {
        return parameters.value(name, defaultValue);
      });
    if (!animationCallbacksResult)
      return std::unexpected(std::move(animationCallbacksResult.error()));
    auto animationCallbacks = std::move(*animationCallbacksResult);
    if (auto result = m_scriptedAnimator.addCallbacks("animationConfig", std::move(animationCallbacks)); !result)
      return std::unexpected(std::move(result.error()));
    auto entityCallbacksResult = LuaBindings::makeEntityCallbacks(*this);
    if (!entityCallbacksResult)
      return std::unexpected(std::move(entityCallbacksResult.error()));
    auto entityCallbacks = std::move(*entityCallbacksResult);
    if (auto result = m_scriptedAnimator.addCallbacks("entity", std::move(entityCallbacks)); !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = m_scriptedAnimator.init(world); !result)
      return std::unexpected(std::move(result.error()));
  }
  m_xAimPositionNetState.setInterpolator(world.geometry().xLerpFunction());
  return refreshEquipment();
}

std::expected<void, StarException> Player::uninit() {
  std::optional<StarException> error;
  auto remember = [&error](auto result) {
    if (!result) {
      if (!error)
        error = std::move(result.error());
      else
        error = error->withContext(strf("Additional error while uninitializing player: {}", outputException(ExceptionDiagnostic(result.error()), true)));
    }
  };
  remember(m_techController->uninit());
  m_movementController->uninit();
  setWireConnector(std::nullopt);
  remember(m_tools->uninit());
  remember(m_statusController->uninit());
  if (isMaster()) {
    remember(m_questManager->uninit());
    remember(m_companions->uninit());
    remember(m_deployment->uninit());
    for (auto& p : m_genericScriptContexts) {
      remember(p.second->uninit());
      for (auto group : std::array<std::string_view, 7>{"animator", "entity", "player", "mcontroller", "status", "songbook", "world"})
        remember(p.second->removeCallbacks(group));
      if (m_client)
        remember(p.second->removeCallbacks("celestial"));
    }
  }
  if (auto world = maybeWorld(); world && world->get().isClient()) {
    remember(m_scriptedAnimator.uninit());
    remember(m_scriptedAnimator.removeCallbacks("animationConfig"));
    remember(m_scriptedAnimator.removeCallbacks("entity"));
  }
  m_controlMovementState.reset();
  m_movementAnimationState.reset();
  m_movementController->setNetworkMovingDirection({});
  remember(Entity::uninit());
  if (error)
    return std::unexpected(std::move(*error));
  return {};
}

std::expected<List<Drawable>, StarException> Player::drawables() const {
  List<Drawable> drawables;
  if (!isTeleporting()) {
    auto backResult = m_techController->backDrawables();
    if (!backResult)
      return std::unexpected(std::move(backResult.error()));
    auto back = std::move(*backResult);
    drawables.appendAll(std::move(back));
    if (!m_techController->parentHidden()) {
      if (auto result = m_tools->setupHumanoidHandItemDrawables(*humanoid()); !result)
        return std::unexpected(std::move(result.error()));
      DirectivesGroup directives;
      Vec2F scale = Vec2F::filled(1.f);
      auto extractScale = [&](List<Directives> const& list) -> std::expected<void, StarException> {
        for (auto const& entry : list) {
          auto resultResult = Humanoid::extractScaleFromDirectives(entry);
          if (!resultResult)
            return std::unexpected(std::move(resultResult.error()));
          auto result = std::move(*resultResult);
          scale = scale.piecewiseMultiply(result.first);
          directives.append(result.second);
        }
        return {};
      };
      if (auto result = extractScale(m_techController->parentDirectives().list()); !result)
        return std::unexpected(std::move(result.error()));
      if (auto result = extractScale(m_statusController->parentDirectives().list()); !result)
        return std::unexpected(std::move(result.error()));
      humanoid()->setScale(scale);
      auto renderedResult = humanoid()->render();
      if (!renderedResult)
        return std::unexpected(std::move(renderedResult.error()));
      auto rendered = std::move(*renderedResult);
      for (auto& drawable : rendered) {
        drawable.translate(position() + m_techController->parentOffset());
        if (drawable.isImage()) {
          auto metadataResult = root().imageMetadataDatabase();
          if (!metadataResult)
            return std::unexpected(std::move(metadataResult.error()));
          auto metadata = std::move(*metadataResult);
          if (auto result = drawable.imagePart().addDirectivesGroup(*metadata, directives, true); !result)
            return std::unexpected(std::move(result.error()));
          if (auto anchor = as<LoungeAnchor>(m_movementController->entityAnchor())) {
            if (auto const& extra = anchor->directives) {
              if (auto result = drawable.imagePart().addDirectives(*metadata, *extra, true); !result)
                return std::unexpected(std::move(result.error()));
            }
          }
        }
        drawables.append(std::move(drawable));
      }
    }
    auto frontResult = m_techController->frontDrawables();
    if (!frontResult)
      return std::unexpected(std::move(frontResult.error()));
    auto front = std::move(*frontResult);
    drawables.appendAll(std::move(front));
    auto statusResult = m_statusController->drawables();
    if (!statusResult)
      return std::unexpected(std::move(statusResult.error()));
    auto status = std::move(*statusResult);
    drawables.appendAll(std::move(status));
    auto rangeResult = inToolRange();
    if (!rangeResult)
      return std::unexpected(std::move(rangeResult.error()));
    auto range = std::move(*rangeResult);
    auto previewsResult = m_tools->renderObjectPreviews(m_aimPosition, walkingDirection(), range, favoriteColor());
    if (!previewsResult)
      return std::unexpected(std::move(previewsResult.error()));
    auto previews = std::move(*previewsResult);
    drawables.appendAll(std::move(previews));
  }
  auto effectsResult = m_effectsAnimator->drawables(position());
  if (!effectsResult)
    return std::unexpected(std::move(effectsResult.error()));
  auto effects = std::move(*effectsResult);
  drawables.appendAll(std::move(effects));
  return drawables;
}

std::expected<List<OverheadBar>, StarException> Player::bars() const {
  return m_statusController->overheadBars();
}

std::expected<List<Particle>, StarException> Player::particles() {
  List<Particle> particles;
  auto currentWorldResult = world();
  if (!currentWorldResult)
    return std::unexpected(std::move(currentWorldResult.error()));
  auto& currentWorld = currentWorldResult->get();
  auto splashResult = m_config->splashConfig.doSplash(position(), m_movementController->velocity(), currentWorld);
  if (!splashResult)
    return std::unexpected(std::move(splashResult.error()));
  auto splash = std::move(*splashResult);
  particles.appendAll(std::move(splash));
  particles.appendAll(take(m_callbackParticles));
  particles.appendAll(m_humanoidDynamicTarget.pullNewParticles());
  particles.appendAll(m_techController->pullNewParticles());
  particles.appendAll(m_statusController->pullNewParticles());
  return particles;
}

std::expected<void, StarException> Player::addParticles(List<Particle> const& particles) {
  m_callbackParticles.appendAll(particles);
  return {};
}

std::expected<void, StarException> Player::addSound(String const& sound, float volume, float pitch) {
  m_callbackSounds.emplaceAppend(sound, volume, pitch);
  return {};
}

std::expected<void, StarException> Player::addEphemeralStatusEffects(List<EphemeralStatusEffect> const& statusEffects) {
  if (isSlave())
    return std::unexpected(PlayerException("Adding status effects to an entity can only be done directly on the master entity."));
  return m_statusController->addEphemeralEffects(statusEffects);
}

std::expected<ActiveUniqueStatusEffectSummary, StarException> Player::activeUniqueStatusEffectSummary() const {
  return m_statusController->activeUniqueStatusEffectSummary();
}

std::expected<float, StarException> Player::powerMultiplier() const {
  return m_statusController->stat("powerMultiplier");
}

bool Player::isDead() const {
  return !m_statusController->resourcePositive("health");
}

std::expected<void, StarException> Player::kill() {
  return m_statusController->setResource("health", 0);
}

bool Player::wireToolInUse() const {
  return (bool)as<WireTool>(m_tools->primaryHandItem());
}

void Player::setWireConnector(std::optional<std::reference_wrapper<WireConnector>> wireConnector) const {
  if (auto wireTool = as<WireTool>(m_tools->primaryHandItem()))
    wireTool->setConnector(wireConnector);
}

std::expected<List<Drawable>, StarException> Player::portrait(PortraitMode mode) const {
  if (isPermaDead())
    return humanoid()->renderSkull();
  if (invisible())
    return List<Drawable>{};
  if (!inWorld()) {
    if (auto result = refreshHumanoid(); !result)
      return std::unexpected(std::move(result.error()));
  }
  return humanoid()->renderPortrait(mode);
}

bool Player::underwater() const {
  if (!inWorld())
    return false;
  return maybeWorld()->get().liquidLevel(Vec2I(position() + m_config->underwaterSensor)).level >= m_config->underwaterMinWaterLevel;
}

std::expected<List<LightSource>, StarException> Player::lightSources() const {
  List<LightSource> lights;
  auto toolsResult = m_tools->lightSources();
  if (!toolsResult)
    return std::unexpected(std::move(toolsResult.error()));
  auto tools = std::move(*toolsResult);
  lights.appendAll(std::move(tools));
  auto statusResult = m_statusController->lightSources();
  if (!statusResult)
    return std::unexpected(std::move(statusResult.error()));
  auto status = std::move(*statusResult);
  lights.appendAll(std::move(status));
  auto techResult = m_techController->lightSources();
  if (!techResult)
    return std::unexpected(std::move(techResult.error()));
  auto tech = std::move(*techResult);
  lights.appendAll(std::move(tech));
  auto humanoidLightsResult = humanoid()->networkedAnimator()->get().lightSources();
  if (!humanoidLightsResult)
    return std::unexpected(std::move(humanoidLightsResult.error()));
  auto humanoidLights = std::move(*humanoidLightsResult);
  lights.appendAll(std::move(humanoidLights));
  return lights;
}

RectF Player::metaBoundBox() const {
  return m_config->metaBoundBox;
}

std::expected<std::optional<HitType>, StarException> Player::queryHit(DamageSource const& source) const {
  if (!inWorld() || isDead() || m_isAdmin || isTeleporting() || m_statusController->statPositive("invulnerable"))
    return std::optional<HitType>{};
  auto shieldResult = m_tools->queryShieldHit(source);
  if (!shieldResult)
    return std::unexpected(std::move(shieldResult.error()));
  auto shield = std::move(*shieldResult);
  if (shield)
    return std::optional<HitType>{HitType::ShieldHit};
  auto currentWorldResult = world();
  if (!currentWorldResult)
    return std::unexpected(std::move(currentWorldResult.error()));
  auto& currentWorld = currentWorldResult->get();
  auto intersectsResult = source.intersectsWithPoly(currentWorld.geometry(), m_movementController->collisionBody());
  if (!intersectsResult)
    return std::unexpected(std::move(intersectsResult.error()));
  auto intersects = std::move(*intersectsResult);
  if (intersects)
    return std::optional<HitType>{HitType::Hit};
  return std::optional<HitType>{};
}

std::expected<std::optional<PolyF>, StarException> Player::hitPoly() const {
  return m_movementController->collisionBody();
}

std::expected<List<DamageNotification>, StarException> Player::applyDamage(DamageRequest const& request) {
  if (!inWorld() || isDead() || m_isAdmin)
    return {};

  return m_statusController->applyDamageRequest(request);
}

std::expected<List<DamageNotification>, StarException> Player::selfDamageNotifications() {
  return m_statusController->pullSelfDamageNotifications();
}

std::expected<void, StarException> Player::hitOther(EntityId targetEntityId, DamageRequest const& damageRequest) {
  if (!isMaster())
    return {};
  m_statusController->hitOther(targetEntityId, damageRequest);
  auto currentWorldResult = world();
  if (!currentWorldResult)
    return std::unexpected(std::move(currentWorldResult.error()));
  auto& currentWorld = currentWorldResult->get();
  if (as<DamageBarEntity>(currentWorld.entity(targetEntityId))) {
    m_lastDamagedOtherTimer = 0;
    m_lastDamagedTarget = targetEntityId;
  }
  return {};
}

std::expected<void, StarException> Player::damagedOther(DamageNotification const& damage) {
  if (isMaster())
    m_statusController->damagedOther(damage);
  return {};
}

std::expected<List<DamageSource>, StarException> Player::damageSources() const {
  return m_damageSources;
}

std::expected<bool, StarException> Player::shouldDestroy() const {
  return isDead();
}

std::expected<void, StarException> Player::destroy(std::optional<std::reference_wrapper<RenderCallback>> renderCallback) {
  m_state = State::Idle;
  m_emoteState = HumanoidEmote::Idle;
  if (renderCallback && m_deathParticleBurst.get()) {
    auto particlesResult = humanoid()->particles(*m_deathParticleBurst.get());
    if (!particlesResult)
      return std::unexpected(std::move(particlesResult.error()));
    auto particles = std::move(*particlesResult);
    if (auto result = renderCallback->get().addParticles(std::move(particles), position()); !result)
      return std::unexpected(std::move(result.error()));
  }
  if (isMaster()) {
    m_log->addDeathCount(1);
    auto currentWorldResult = world();
    if (!currentWorldResult)
      return std::unexpected(std::move(currentWorldResult.error()));
    auto& currentWorld = currentWorldResult->get();
    auto disabledResult = currentWorld.disableDeathDrops();
    if (!disabledResult)
      return std::unexpected(std::move(disabledResult.error()));
    auto disabled = std::move(*disabledResult);
    if (!disabled) {
      auto config = modeConfig();
      if (auto dropString = config.deathDropItemTypes.maybeLeft()) {
        if (*dropString == "all") {
          if (auto result = dropEverything(); !result)
            return std::unexpected(std::move(result.error()));
        }
      } else {
        auto namesResult = config.deathDropItemTypes.right();
        if (!namesResult)
          return std::unexpected(std::move(namesResult.error()));
        auto const& names = namesResult->get();
        if (!ItemTypeNames)
          return std::unexpected(ItemTypeNames.error());
        Set<ItemType> types;
        for (auto const& name : names) {
          auto typeResult = ItemTypeNames->getLeft(name);
          if (!typeResult)
            return std::unexpected(std::move(typeResult.error()));
          auto const& type = typeResult->get();
          types.add(type);
        }
        auto databaseResult = root().itemDatabase();
        if (!databaseResult)
          return std::unexpected(std::move(databaseResult.error()));
        auto database = std::move(*databaseResult);
        if (auto result = dropSelectedItems([types = std::move(types), database](std::shared_ptr<Item> item) -> std::expected<bool, StarException> {
          auto typeResult = database->itemType(item->name());
          if (!typeResult)
            return std::unexpected(std::move(typeResult.error()));
          auto type = std::move(*typeResult);
          return types.contains(type);
        }); !result)
          return std::unexpected(std::move(result.error()));
      }
    }
  }
  m_songbook->stop();
  return {};
}

std::optional<EntityAnchorState> Player::loungingIn() const {
  if (is<LoungeAnchor>(m_movementController->entityAnchor()))
    return m_movementController->anchorState();
  return {};
}

std::expected<bool, StarException> Player::lounge(EntityId loungeableEntityId, std::size_t anchorIndex) {
  if (!canUseTool())
    return false;
  auto currentWorldResult = world();
  if (!currentWorldResult)
    return std::unexpected(std::move(currentWorldResult.error()));
  auto& currentWorld = currentWorldResult->get();
  auto loungeableEntity = currentWorld.get<LoungeableEntity>(loungeableEntityId);
  if (!loungeableEntity || anchorIndex >= loungeableEntity->anchorCount())
    return false;
  auto loungersResult = loungeableEntity->entitiesLoungingIn(anchorIndex);
  if (!loungersResult)
    return std::unexpected(std::move(loungersResult.error()));
  auto loungers = std::move(*loungersResult);
  if (!loungers.empty())
    return false;
  auto anchorResult = loungeableEntity->loungeAnchor(anchorIndex);
  if (!anchorResult)
    return std::unexpected(std::move(anchorResult.error()));
  auto anchor = std::move(*anchorResult);
  if (!anchor)
    return false;
  m_state = State::Lounge;
  if (auto result = m_movementController->setAnchorState({loungeableEntityId, anchorIndex}); !result)
    return std::unexpected(std::move(result.error()));
  return true;
}

std::expected<void, StarException> Player::stopLounging() {
  if (loungingIn()) {
    {
      if (auto result = m_movementController->resetAnchorState(); !result)
        return std::unexpected(std::move(result.error()));
    }
    m_state = State::Idle;
    {
      if (auto result = m_statusController->setPersistentEffects("lounging", {}); !result)
        return std::unexpected(std::move(result.error()));
    }
  }
  return {};
}

Vec2F Player::position() const {
  return m_movementController->position();
}

Vec2F Player::velocity() const {
  return m_movementController->velocity();
}

std::expected<Vec2F, StarException> Player::mouthOffset(bool ignoreAdjustments) const {
  auto offsetResult = humanoid()->mouthOffset(ignoreAdjustments);
  if (!offsetResult)
    return std::unexpected(std::move(offsetResult.error()));
  auto offset = std::move(*offsetResult);
  return Vec2F(offset[0] * numericalDirection(facingDirection()), offset[1]);
}

std::expected<Vec2F, StarException> Player::feetOffset() const {
  auto offsetResult = humanoid()->feetOffset();
  if (!offsetResult)
    return std::unexpected(std::move(offsetResult.error()));
  auto offset = std::move(*offsetResult);
  return Vec2F(offset[0] * numericalDirection(facingDirection()), offset[1]);
}

std::expected<Vec2F, StarException> Player::headArmorOffset() const {
  auto offsetResult = humanoid()->headArmorOffset();
  if (!offsetResult)
    return std::unexpected(std::move(offsetResult.error()));
  auto offset = std::move(*offsetResult);
  return Vec2F(offset[0] * numericalDirection(facingDirection()), offset[1]);
}

std::expected<Vec2F, StarException> Player::chestArmorOffset() const {
  auto offsetResult = humanoid()->chestArmorOffset();
  if (!offsetResult)
    return std::unexpected(std::move(offsetResult.error()));
  auto offset = std::move(*offsetResult);
  return Vec2F(offset[0] * numericalDirection(facingDirection()), offset[1]);
}

std::expected<Vec2F, StarException> Player::backArmorOffset() const {
  auto offsetResult = humanoid()->backArmorOffset();
  if (!offsetResult)
    return std::unexpected(std::move(offsetResult.error()));
  auto offset = std::move(*offsetResult);
  return Vec2F(offset[0] * numericalDirection(facingDirection()), offset[1]);
}

std::expected<Vec2F, StarException> Player::legsArmorOffset() const {
  auto offsetResult = humanoid()->legsArmorOffset();
  if (!offsetResult)
    return std::unexpected(std::move(offsetResult.error()));
  auto offset = std::move(*offsetResult);
  return Vec2F(offset[0] * numericalDirection(facingDirection()), offset[1]);
}

std::expected<Vec2F, StarException> Player::mouthPosition() const {
  return mouthPosition(true);
}

std::expected<Vec2F, StarException> Player::mouthPosition(bool ignoreAdjustments) const {
  auto offsetResult = mouthOffset(ignoreAdjustments);
  if (!offsetResult)
    return std::unexpected(std::move(offsetResult.error()));
  auto offset = std::move(*offsetResult);
  return position() + offset;
}

RectF Player::collisionArea() const {
  return m_movementController->collisionPoly().boundBox();
}

std::expected<void, StarException> Player::revive(Vec2F const& footPosition) {
  if (!isDead())
    return {};
  m_state = State::Idle;
  m_emoteState = HumanoidEmote::Idle;
  if (auto result = synchronizeEquipmentStatusEffects(*m_armor, *m_tools, *m_statusController); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = m_statusController->resetAllResources(); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = m_statusController->clearEphemeralEffects(); !result)
    return std::unexpected(std::move(result.error()));
  endPrimaryFire();
  endAltFire();
  endTrigger();
  m_effectEmitter->reset();
  auto offsetResult = feetOffset();
  if (!offsetResult)
    return std::unexpected(std::move(offsetResult.error()));
  auto offset = std::move(*offsetResult);
  m_movementController->setPosition(footPosition - offset);
  m_movementController->setVelocity(Vec2F());
  if (auto result = m_techController->reloadTech(); !result)
    return std::unexpected(std::move(result.error()));
  float moneyCost = m_inventory->currency("money") * modeConfig().reviveCostPercentile;
  m_inventory->consumeCurrency("money", std::min((std::uint64_t)std::round(moneyCost), m_inventory->currency("money")));
  return {};
}

bool Player::shifting() const {
  return m_shifting;
}

void Player::setShifting(bool shifting) {
  m_shifting = shifting;
}

std::expected<void, StarException> Player::special(int specialKey) {
  auto loungeAnchor = as<LoungeAnchor>(m_movementController->entityAnchor());
  if (loungeAnchor && loungeAnchor->controllable) {
    auto anchorState = m_movementController->anchorState();
    if (!anchorState)
      return std::unexpected(PlayerException("Player lounge anchor has no anchor state"));
    {
      auto currentWorldResult = world();
      if (!currentWorldResult)
        return std::unexpected(std::move(currentWorldResult.error()));
      auto& currentWorld = currentWorldResult->get();
      if (auto loungeableEntity = currentWorld.get<LoungeableEntity>(anchorState->entityId)) {
        if (specialKey == 1)
          return loungeableEntity->loungeControl(anchorState->positionIndex, LoungeControl::Special1);
        if (specialKey == 2)
          return loungeableEntity->loungeControl(anchorState->positionIndex, LoungeControl::Special2);
        if (specialKey == 3)
          return loungeableEntity->loungeControl(anchorState->positionIndex, LoungeControl::Special3);
        return {};
      }
    }
  }
  m_techController->special(specialKey);
  return {};
}

void Player::setMoveVector(Vec2F const& vec) {
  m_moveVector = vec;
}

void Player::moveLeft() {
  m_pendingMoves.add(MoveControlType::Left);
}

void Player::moveRight() {
  m_pendingMoves.add(MoveControlType::Right);
}

void Player::moveUp() {
  m_pendingMoves.add(MoveControlType::Up);
}

void Player::moveDown() {
  m_pendingMoves.add(MoveControlType::Down);
}

void Player::jump() {
  m_pendingMoves.add(MoveControlType::Jump);
}

std::expected<void, StarException> Player::dropItem() {
  auto currentWorld = maybeWorld();
  if (!currentWorld || !canUseTool())
    return {};
  Vec2F throwDirection = currentWorld->get().geometry().diff(m_aimPosition, position());
  auto primarySlotResult = m_inventory->primaryHeldSlot();
  if (!primarySlotResult)
    return std::unexpected(std::move(primarySlotResult.error()));
  auto primarySlot = std::move(*primarySlotResult);
  auto secondarySlotResult = m_inventory->secondaryHeldSlot();
  if (!secondarySlotResult)
    return std::unexpected(std::move(secondarySlotResult.error()));
  auto secondarySlot = std::move(*secondarySlotResult);
  for (auto const& throwSlot : {primarySlot, secondarySlot}) {
    if (throwSlot) {
      auto dropResult = m_inventory->takeSlot(*throwSlot);
      if (!dropResult)
        return std::unexpected(std::move(dropResult.error()));
      auto drop = std::move(*dropResult);
      if (drop) {
        auto entityResult = ItemDrop::throwDrop(root(), drop, position(), velocity(), throwDirection);
        if (!entityResult)
          return std::unexpected(std::move(entityResult.error()));
        auto entity = std::move(*entityResult);
        return currentWorld->get().addEntity(std::move(entity));
      }
    }
  }
  return {};
}

std::expected<std::optional<ChainableJsonMessageResponse>, StarException> Player::receiveMessage(ConnectionId fromConnection, String const& message, JsonArray const& args) {
  auto currentWorldResult = world();
  if (!currentWorldResult)
    return std::unexpected(std::move(currentWorldResult.error()));
  auto& currentWorld = currentWorldResult->get();
  bool localMessage = fromConnection == currentWorld.connection();
  if (message == "queueRadioMessage" && !args.empty()) {
    float delay = 0;
    if (args.size() > 1 && args[1].canConvert(Json::Type::Float)) {
      auto valueResult = args[1].toFloat();
      if (!valueResult)
        return std::unexpected(std::move(valueResult.error()));
      auto value = std::move(*valueResult);
      delay = value;
    }
    if (auto result = queueRadioMessage(args[0], delay); !result)
      return std::unexpected(std::move(result.error()));
  } else if (message == "warp") {
    std::optional<String> animation;
    if (args.size() > 1) {
      auto valueResult = args[1].toString();
      if (!valueResult)
        return std::unexpected(std::move(valueResult.error()));
      auto value = std::move(*valueResult);
      animation = std::move(value);
    }
    bool deploy = false;
    if (args.size() > 2) {
      auto valueResult = args[2].toBool();
      if (!valueResult)
        return std::unexpected(std::move(valueResult.error()));
      auto value = std::move(*valueResult);
      deploy = value;
    }
    auto actionResult = args.get(0).toString();
    if (!actionResult)
      return std::unexpected(std::move(actionResult.error()));
    auto action = std::move(*actionResult);
    setPendingWarp(action, animation, deploy);
  } else if (message == "interruptRadioMessage") {
    m_interruptRadioMessage = true;
  } else if (message == "playCinematic" && !args.empty()) {
    bool unique = false;
    if (args.size() > 1) {
      auto valueResult = args[1].toBool();
      if (!valueResult)
        return std::unexpected(std::move(valueResult.error()));
      auto value = std::move(*valueResult);
      unique = value;
    }
    if (auto result = setPendingCinematic(args[0], unique); !result)
      return std::unexpected(std::move(result.error()));
  } else if (message == "playAltMusic" && !args.empty()) {
    float fadeTime = 0;
    if (args.size() > 1) {
      auto valueResult = args[1].toFloat();
      if (!valueResult)
        return std::unexpected(std::move(valueResult.error()));
      auto value = std::move(*valueResult);
      fadeTime = value;
    }
    int loops = -1;
    if (args.size() > 2) {
      auto valueResult = args[2].toInt();
      if (!valueResult)
        return std::unexpected(std::move(valueResult.error()));
      auto value = std::move(*valueResult);
      loops = value;
    }
    StringList tracks;
    if (args[0].canConvert(Json::Type::Array)) {
      auto arrayResult = args[0].toArray();
      if (!arrayResult)
        return std::unexpected(std::move(arrayResult.error()));
      auto array = std::move(*arrayResult);
      auto valuesResult = jsonToStringList(array);
      if (!valuesResult)
        return std::unexpected(std::move(valuesResult.error()));
      auto values = std::move(*valuesResult);
      tracks = std::move(values);
    }
    m_pendingAltMusic = std::pair<std::optional<std::pair<StringList, int>>, float>(std::make_pair(std::move(tracks), loops), fadeTime);
  } else if (message == "stopAltMusic") {
    float fadeTime = 0;
    if (!args.empty()) {
      auto valueResult = args[0].toFloat();
      if (!valueResult)
        return std::unexpected(std::move(valueResult.error()));
      auto value = std::move(*valueResult);
      fadeTime = value;
    }
    m_pendingAltMusic = std::pair<std::optional<std::pair<StringList, int>>, float>({}, fadeTime);
  } else if (message == "recordEvent") {
    auto eventJsonResult = args.at(0);
    if (!eventJsonResult)
      return std::unexpected(std::move(eventJsonResult.error()));
    auto const& eventJson = eventJsonResult->get();
    auto fieldsResult = args.at(1);
    if (!fieldsResult)
      return std::unexpected(std::move(fieldsResult.error()));
    auto const& fields = fieldsResult->get();
    auto eventResult = eventJson.toString();
    if (!eventResult)
      return std::unexpected(std::move(eventResult.error()));
    auto event = std::move(*eventResult);
    statistics()->recordEvent(event, fields);
  } else if (message == "addCollectable") {
    auto collectionResult = args.get(0).toString();
    if (!collectionResult)
      return std::unexpected(std::move(collectionResult.error()));
    auto collection = std::move(*collectionResult);
    auto collectableResult = args.get(1).toString();
    if (!collectableResult)
      return std::unexpected(std::move(collectableResult.error()));
    auto collectable = std::move(*collectableResult);
    auto databaseResult = root().collectionDatabase();
    if (!databaseResult)
      return std::unexpected(std::move(databaseResult.error()));
    auto database = std::move(*databaseResult);
    if (database->hasCollectable(collection, collectable)) {
      if (auto result = addCollectable(collection, collectable); !result)
        return std::unexpected(std::move(result.error()));
    }
  } else {
    {
      auto responseResult = m_tools->receiveMessage(message, localMessage, args);
      if (!responseResult)
        return std::unexpected(std::move(responseResult.error()));
      auto response = std::move(*responseResult);
      if (response)
        return response;
    }
    {
      auto responseResult = m_statusController->receiveMessage(message, localMessage, args);
      if (!responseResult)
        return std::unexpected(std::move(responseResult.error()));
      auto response = std::move(*responseResult);
      if (response)
        return response;
    }
    {
      auto responseResult = m_companions->receiveMessage(message, localMessage, args);
      if (!responseResult)
        return std::unexpected(std::move(responseResult.error()));
      auto response = std::move(*responseResult);
      if (response)
        return response;
    }
    {
      auto responseResult = m_deployment->receiveMessage(message, localMessage, args);
      if (!responseResult)
        return std::unexpected(std::move(responseResult.error()));
      auto response = std::move(*responseResult);
      if (response)
        return response;
    }
    {
      auto responseResult = m_techController->receiveMessage(message, localMessage, args);
      if (!responseResult)
        return std::unexpected(std::move(responseResult.error()));
      auto response = std::move(*responseResult);
      if (response)
        return response;
    }
    {
      auto responseResult = m_questManager->receiveMessage(message, localMessage, args);
      if (!responseResult)
        return std::unexpected(std::move(responseResult.error()));
      auto response = std::move(*responseResult);
      if (response)
        return response;
    }
    for (auto& script : m_genericScriptContexts) {
      auto responseResult = script.second->handleMessage(message, localMessage, args);
      if (!responseResult)
        return std::unexpected(std::move(responseResult.error()));
      auto response = std::move(*responseResult);
      if (response)
        return response;
    }
  }
  return std::optional<ChainableJsonMessageResponse>{};
}

std::expected<void, StarException> Player::update(float dt, std::uint64_t) {
  auto currentWorldResult = world();
  if (!currentWorldResult)
    return std::unexpected(std::move(currentWorldResult.error()));
  auto& currentWorld = currentWorldResult->get();
  m_movementController->setTimestep(dt);

  if (isMaster()) {
    // Requests from postUpdate, messages before this tick, or a prior world
    // must not outlive the generic update that actually refreshes them.
    m_controlMovementState.reset();
    if (m_emoteCooldownTimer.tick(dt))
      m_emoteState = HumanoidEmote::Idle;
    if (m_danceCooldownTimer.tick(dt))
      m_dance = {};

    if (m_chatMessageUpdated) {
      auto emoteProcessorResult = root().emoteProcessor();
      if (!emoteProcessorResult)
        return std::unexpected(std::move(emoteProcessorResult.error()));
      auto emoteProcessor = std::move(*emoteProcessorResult);
      auto stateResult = emoteProcessor->detectEmotes(m_chatMessage);
      if (!stateResult)
        return std::unexpected(std::move(stateResult.error()));
      auto state = std::move(*stateResult);
      if (state != HumanoidEmote::Idle)
        addEmote(state);
      m_chatMessageUpdated = false;
    }

    if (m_blinkCooldownTimer.tick(dt)) {
      auto blinkIntervalResult = root().random().randf(m_blinkInterval[0], m_blinkInterval[1]);
      if (!blinkIntervalResult)
        return std::unexpected(std::move(blinkIntervalResult.error()));
      auto blinkInterval = std::move(*blinkIntervalResult);
      m_blinkCooldownTimer = GameTimer(blinkInterval);
      auto loungeAnchor = as<LoungeAnchor>(m_movementController->entityAnchor());
      if (m_emoteState == HumanoidEmote::Idle && (!loungeAnchor || !loungeAnchor->emote))
        addEmote(HumanoidEmote::Blink);
    }

    m_lastDamagedOtherTimer += dt;

    if (m_movementController->zeroG())
      m_movementController->controlParameters(m_zeroGMovementParameters);

    if (isTeleporting()) {
      m_teleportTimer -= dt;
      if (m_teleportTimer <= 0 && m_state == State::TeleportIn) {
        m_state = State::Idle;
        {
          if (auto result = m_effectsAnimator->burstParticleEmitter(m_teleportAnimationType + "Burst"); !result)
            return std::unexpected(std::move(result.error()));
        }
      }
    }

    if (!isTeleporting()) {
      {
        if (auto result = processControls(); !result)
          return std::unexpected(std::move(result.error()));
      }

      {
        if (auto result = m_questManager->update(dt); !result)
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

      bool edgeTriggeredUse = take(m_edgeTriggeredUse);

      {
        if (auto result = m_inventory->cleanup(); !result)
          return std::unexpected(std::move(result.error()));
      }
      {
        if (auto result = refreshEquipment(); !result)
          return std::unexpected(std::move(result.error()));
      }

      auto conflictingAnchorResult = inConflictingLoungeAnchor();
      if (!conflictingAnchorResult)
        return std::unexpected(std::move(conflictingAnchorResult.error()));
      auto conflictingAnchor = std::move(*conflictingAnchorResult);
      if (conflictingAnchor)
        {
          if (auto result = m_movementController->resetAnchorState(); !result)
            return std::unexpected(std::move(result.error()));
        }

      if (m_state == State::Lounge) {
        if (auto loungeAnchor = as<LoungeAnchor>(m_movementController->entityAnchor())) {
          {
            if (auto result = m_statusController->setPersistentEffects("lounging", loungeAnchor->statusEffects); !result)
              return std::unexpected(std::move(result.error()));
          }
          {
            if (auto result = addEffectEmitters(loungeAnchor->effectEmitters); !result)
              return std::unexpected(std::move(result.error()));
          }
          if (loungeAnchor->emote)
            {
              if (auto result = requestEmote(*loungeAnchor->emote); !result)
                return std::unexpected(std::move(result.error()));
            }

          auto itemDatabaseResult = root().itemDatabase();
          if (!itemDatabaseResult)
            return std::unexpected(std::move(itemDatabaseResult.error()));
          auto itemDatabase = std::move(*itemDatabaseResult);
          auto const& overrides = loungeAnchor->armorCosmeticOverrides;
          if (auto result = applyLoungeCosmeticOverride<HeadArmor>(overrides, "head", EquipmentSlot::HeadCosmetic,
                  *itemDatabase, *m_armor, [](ArmorWearer& armor, std::shared_ptr<HeadArmor> item) { armor.setHeadCosmeticItem(std::move(item)); }); !result)
            return std::unexpected(std::move(result.error()));
          if (auto result = applyLoungeCosmeticOverride<ChestArmor>(overrides, "chest", EquipmentSlot::ChestCosmetic,
                  *itemDatabase, *m_armor, [](ArmorWearer& armor, std::shared_ptr<ChestArmor> item) { armor.setChestCosmeticItem(std::move(item)); }); !result)
            return std::unexpected(std::move(result.error()));
          if (auto result = applyLoungeCosmeticOverride<LegsArmor>(overrides, "legs", EquipmentSlot::LegsCosmetic,
                  *itemDatabase, *m_armor, [](ArmorWearer& armor, std::shared_ptr<LegsArmor> item) { armor.setLegsCosmeticItem(std::move(item)); }); !result)
            return std::unexpected(std::move(result.error()));
          if (auto result = applyLoungeCosmeticOverride<BackArmor>(overrides, "back", EquipmentSlot::BackCosmetic,
                  *itemDatabase, *m_armor, [](ArmorWearer& armor, std::shared_ptr<BackArmor> item) { armor.setBackCosmeticItem(std::move(item)); }); !result)
            return std::unexpected(std::move(result.error()));
        } else {
          m_state = State::Idle;
          {
            if (auto result = m_movementController->resetAnchorState(); !result)
              return std::unexpected(std::move(result.error()));
          }
        }
      } else {
        {
          if (auto result = m_movementController->resetAnchorState(); !result)
            return std::unexpected(std::move(result.error()));
        }
        {
          if (auto result = m_statusController->setPersistentEffects("lounging", {}); !result)
            return std::unexpected(std::move(result.error()));
        }
      }

      if (!forceNude())
        {
          if (auto result = m_armor->effects(*m_effectEmitter); !result)
            return std::unexpected(std::move(result.error()));
        }

      {
        if (auto result = m_tools->effects(*m_effectEmitter); !result)
          return std::unexpected(std::move(result.error()));
      }

      auto aimRelative = currentWorld.geometry().diff(m_aimPosition, position()); // dumb, but due to how things are ordered
      {
        if (auto result = m_movementController->tickMaster(dt); !result)
          return std::unexpected(std::move(result.error()));
      }
      m_aimPosition = position() + aimRelative;                               // it's gonna have to be like this for now

      {
        if (auto result = m_techController->tickMaster(dt); !result)
          return std::unexpected(std::move(result.error()));
      }

      for (auto& p : m_genericScriptContexts) {
        if (auto result = p.second->update(p.second->updateDt(dt)); !result)
          return std::unexpected(std::move(result.error()));
      }

      if (edgeTriggeredUse) {
        auto anchor = as<LoungeAnchor>(m_movementController->entityAnchor());
        bool useTool = canUseTool();
        if (anchor && (!useTool || anchor->controllable))
          {
            if (auto result = m_movementController->resetAnchorState(); !result)
              return std::unexpected(std::move(result.error()));
          }
        else if (useTool) {
          auto ieResult = bestInteractionEntity(true);
          if (!ieResult)
            return std::unexpected(std::move(ieResult.error()));
          auto ie = std::move(*ieResult);
          if (ie)
            {
              if (auto result = interactWithEntity(ie); !result)
                return std::unexpected(std::move(result.error()));
            }
        }
      }

      if (auto result = synchronizeEquipmentStatusEffects(*m_armor, *m_tools, *m_statusController); !result)
        return std::unexpected(std::move(result.error()));

      if (!m_techController->techOverridden())
        {
          if (auto result = m_techController->setLoadedTech(m_techs->equippedTechs().values()); !result)
            return std::unexpected(std::move(result.error()));
        }

      if (!isDead())
        {
          if (auto result = m_statusController->tickMaster(dt); !result)
            return std::unexpected(std::move(result.error()));
        }

      if (!modeConfig().hunger)
        {
          if (auto result = m_statusController->resetResource("food"); !result)
            return std::unexpected(std::move(result.error()));
        }

      if (!m_statusController->resourcePositive("food")) {
        if (auto result = m_statusController->setPersistentEffects("hunger", m_foodEmptyStatusEffects); !result)
          return std::unexpected(std::move(result.error()));
      } else {
        auto foodResult = m_statusController->resourcePercentage("food");
        if (!foodResult)
          return std::unexpected(std::move(foodResult.error()));
        auto food = std::move(*foodResult);
        if (food.value_or(0.0f) <= m_foodLowThreshold) {
          if (auto result = m_statusController->setPersistentEffects("hunger", m_foodLowStatusEffects); !result)
            return std::unexpected(std::move(result.error()));
        } else {
          if (auto result = m_statusController->setPersistentEffects("hunger", {}); !result)
            return std::unexpected(std::move(result.error()));
        }
      }

      for (auto& pair : m_delayedRadioMessages) {
        if (pair.first.tick(dt))
          {
            if (auto result = queueRadioMessage(pair.second); !result)
              return std::unexpected(std::move(result.error()));
          }
      }
      m_delayedRadioMessages.filter([](std::pair<GameTimer, RadioMessage>& pair) { return !pair.first.ready(); });
    }

    if (m_isAdmin) {
      {
        if (auto result = m_statusController->resetResource("health"); !result)
          return std::unexpected(std::move(result.error()));
      }
      {
        if (auto result = m_statusController->resetResource("energy"); !result)
          return std::unexpected(std::move(result.error()));
      }
      {
        if (auto result = m_statusController->resetResource("food"); !result)
          return std::unexpected(std::move(result.error()));
      }
      {
        if (auto result = m_statusController->resetResource("breath"); !result)
          return std::unexpected(std::move(result.error()));
      }
    }

    m_log->addPlayTime(root().timing().timestep);

    if (m_ageItemsTimer.wrapTick(dt)) {
      auto itemDatabaseResult = root().itemDatabase();
  if (!itemDatabaseResult)
    return std::unexpected(std::move(itemDatabaseResult.error()));
  auto itemDatabase = std::move(*itemDatabaseResult);
      if (auto result = m_inventory->forEveryItem([&](InventorySlot const&, std::shared_ptr<Item>& item) -> std::expected<void, StarException> {
        if (auto result = itemDatabase->ageItem(item, m_ageItemsTimer.time); !result)
          return std::unexpected(std::move(result.error()));
        return {};
      }); !result)
        return std::unexpected(std::move(result.error()));
    }

    for (auto& tool : {m_tools->primaryHandItem(), m_tools->altHandItem()}) {
      if (auto inspectionTool = as<InspectionTool>(tool)) {
        for (auto& ir : inspectionTool->pullInspectionResults()) {
          if (ir.objectName) {
            if (!ir.entityId)
              return std::unexpected(PlayerException("Scanned object has no entity id"));
            if (auto result = m_questManager->receiveMessage("objectScanned", true, {*ir.objectName, *ir.entityId}); !result)
              return std::unexpected(std::move(result.error()));
            m_log->addScannedObject(*ir.objectName);
          }

          if (auto chat = addChatMessage(ir.message, JsonObject{
            {"message", JsonObject{
              {"context", JsonObject{{"mode", "RadioMessage"}}},
              {"fromConnection", currentWorld.connection()},
              {"text", ir.message}
            }}
          }); !chat)
            return std::unexpected(std::move(chat.error()));
        }
      }
    }

    auto interestingObjectsResult = m_questManager->interestingObjects();
    if (!interestingObjectsResult)
      return std::unexpected(std::move(interestingObjectsResult.error()));
    auto interestingObjects = std::move(*interestingObjectsResult);
    m_interestingObjects = std::move(interestingObjects);

  } else {
    {
      if (auto result = m_netGroup.tickNetInterpolation(dt); !result)
        return std::unexpected(std::move(result.error()));
    }
    {
      if (auto result = m_movementController->tickSlave(dt); !result)
        return std::unexpected(std::move(result.error()));
    }
    {
      if (auto result = m_techController->tickSlave(dt); !result)
        return std::unexpected(std::move(result.error()));
    }
    {
      if (auto result = m_statusController->tickSlave(dt); !result)
        return std::unexpected(std::move(result.error()));
    }
  }

  updateMovementAnimationState();

  humanoid()->setRotation(m_movementController->rotation());

  bool suppressedItems = !canUseTool();

  auto loungeAnchor = as<LoungeAnchor>(m_movementController->entityAnchor());
  if (loungeAnchor && loungeAnchor->dance) {
    if (auto result = humanoid()->setDance(*loungeAnchor->dance); !result)
      return std::unexpected(std::move(result.error()));
  } else if (m_dance) {
    if (auto result = humanoid()->setDance(m_dance); !result)
      return std::unexpected(std::move(result.error()));
  } else {
    bool clearDance = !suppressedItems && (m_tools->primaryHandItem() || m_tools->altHandItem());
    if (!clearDance) {
      auto cyclicOrEndedResult = humanoid()->danceCyclicOrEnded();
      if (!cyclicOrEndedResult)
        return std::unexpected(std::move(cyclicOrEndedResult.error()));
      auto cyclicOrEnded = std::move(*cyclicOrEndedResult);
      clearDance = cyclicOrEnded || m_movementController->running();
    }
    if (clearDance) {
      if (auto result = humanoid()->setDance({}); !result)
        return std::unexpected(std::move(result.error()));
    }
  }
  bool isClient = currentWorld.isClient();

  m_tools->suppressItems(suppressedItems);
  {
    if (auto result = m_tools->tick(dt, m_shifting, m_pendingMoves); !result)
      return std::unexpected(std::move(result.error()));
  }

  Direction facingDirection = m_movementController->facingDirection();

  auto overrideFacingDirectionResult = m_tools->setupHumanoidHandItems(*humanoid(), position(), m_aimPosition);
  if (!overrideFacingDirectionResult)
    return std::unexpected(std::move(overrideFacingDirectionResult.error()));
  auto overrideFacingDirection = std::move(*overrideFacingDirectionResult);
  if (!loungingIn() && overrideFacingDirection)
    m_movementController->controlFace(facingDirection = *overrideFacingDirection);

  humanoid()->setFacingDirection(facingDirection);
  Direction movingDirection = m_movementController->movingDirection();
  if (m_movementAnimationState) {
    auto tangent = Vec2F::withAngle(m_movementController->rotation());
    auto velocity = m_movementController->velocity();
    float tangentVelocity = velocity.x() * tangent.x() + velocity.y() * tangent.y();
    movingDirection = tangentVelocity == 0.f ? facingDirection : tangentVelocity < 0.f ? Direction::Left : Direction::Right;
  }
  if (isMaster())
    m_movementController->setNetworkMovingDirection(m_movementAnimationState ? std::optional(movingDirection) : std::nullopt);
  if (auto result = humanoid()->setMovingBackwards(facingDirection != movingDirection); !result)
    return std::unexpected(std::move(result.error()));

  {
    if (auto result = refreshHumanoid(); !result)
      return std::unexpected(std::move(result.error()));
  }

  auto scale = Mat3F::scaling(Vec2F(facingDirection == Direction::Right ? 1.f : -1.f, 1.f));
  {
    if (auto result = m_effectsAnimator->setTransformationGroup("flip", scale); !result)
      return std::unexpected(std::move(result.error()));
  }

  if (m_state == State::Walk || m_state == State::Run) {
    if ((m_footstepTimer += dt) > m_config->footstepTiming) {
      m_footstepPending = true;
      m_footstepTimer = 0.0;
    }
  }

  if (isClient) {
    {
      if (auto result = m_effectsAnimator->update(dt, std::ref(m_effectsAnimatorDynamicTarget)); !result)
        return std::unexpected(std::move(result.error()));
    }
    m_effectsAnimatorDynamicTarget.updatePosition(position() + m_techController->parentOffset());
  } else {
    {
      if (auto result = m_effectsAnimator->update(dt, std::nullopt); !result)
        return std::unexpected(std::move(result.error()));
    }
  }

  if (!isTeleporting())
    {
      if (auto result = processStateChanges(dt); !result)
        return std::unexpected(std::move(result.error()));
    }

  auto damageSourcesResult = m_tools->damageSources();
  if (!damageSourcesResult)
    return std::unexpected(std::move(damageSourcesResult.error()));
  auto damageSources = std::move(*damageSourcesResult);
  m_damageSources = std::move(damageSources);
  for (auto& damageSource : m_damageSources) {
    damageSource.sourceEntityId = entityId();
    damageSource.team = getTeam();
  }

  {
    if (auto result = m_songbook->update(*entityMode(), currentWorld); !result)
      return std::unexpected(std::move(result.error()));
  }

  m_effectEmitter->setSourcePosition("normal", position());
  auto mouthOffsetResult = mouthOffset(true);
  if (!mouthOffsetResult)
    return std::unexpected(std::move(mouthOffsetResult.error()));
  auto mouthOffset = std::move(*mouthOffsetResult);
  m_effectEmitter->setSourcePosition("mouth", mouthOffset + position());
  auto feetOffsetResult = feetOffset();
  if (!feetOffsetResult)
    return std::unexpected(std::move(feetOffsetResult.error()));
  auto feetOffset = std::move(*feetOffsetResult);
  m_effectEmitter->setSourcePosition("feet", feetOffset + position());
  auto headArmorOffsetResult = headArmorOffset();
  if (!headArmorOffsetResult)
    return std::unexpected(std::move(headArmorOffsetResult.error()));
  auto headArmorOffset = std::move(*headArmorOffsetResult);
  m_effectEmitter->setSourcePosition("headArmor", headArmorOffset + position());
  auto chestArmorOffsetResult = chestArmorOffset();
  if (!chestArmorOffsetResult)
    return std::unexpected(std::move(chestArmorOffsetResult.error()));
  auto chestArmorOffset = std::move(*chestArmorOffsetResult);
  m_effectEmitter->setSourcePosition("chestArmor", chestArmorOffset + position());
  auto legsArmorOffsetResult = legsArmorOffset();
  if (!legsArmorOffsetResult)
    return std::unexpected(std::move(legsArmorOffsetResult.error()));
  auto legsArmorOffset = std::move(*legsArmorOffsetResult);
  m_effectEmitter->setSourcePosition("legsArmor", legsArmorOffset + position());
  auto backArmorOffsetResult = backArmorOffset();
  if (!backArmorOffsetResult)
    return std::unexpected(std::move(backArmorOffsetResult.error()));
  auto backArmorOffset = std::move(*backArmorOffsetResult);
  m_effectEmitter->setSourcePosition("backArmor", backArmorOffset + position());

  auto primaryOffsetResult = handPosition(ToolHand::Primary);
  if (!primaryOffsetResult)
    return std::unexpected(std::move(primaryOffsetResult.error()));
  auto primaryOffset = std::move(*primaryOffsetResult);
  m_effectEmitter->setSourcePosition("primary", primaryOffset + position());
  auto altOffsetResult = handPosition(ToolHand::Alt);
  if (!altOffsetResult)
    return std::unexpected(std::move(altOffsetResult.error()));
  auto altOffset = std::move(*altOffsetResult);
  m_effectEmitter->setSourcePosition("alt", altOffset + position());

  m_effectEmitter->setDirection(facingDirection);

  {
    if (auto result = m_effectEmitter->tick(dt, *entityMode()); !result)
      return std::unexpected(std::move(result.error()));
  }

  if (isClient) {
    bool calculateHeadRotation = isMaster();
    if (!calculateHeadRotation) {
      auto headRotationPropertyResult = getSecretProperty("humanoid.headRotation");
      if (!headRotationPropertyResult)
        return std::unexpected(std::move(headRotationPropertyResult.error()));
      auto headRotationProperty = std::move(*headRotationPropertyResult);
      if (headRotationProperty.isType(Json::Type::Float)) {
        auto rotationResult = headRotationProperty.toFloat();
        if (!rotationResult)
          return std::unexpected(std::move(rotationResult.error()));
        auto rotation = std::move(*rotationResult);
        humanoid()->setHeadRotation(rotation);
      } else
        calculateHeadRotation = true;
    }
    if (calculateHeadRotation) { // master or not an OpenStarbound player
      float headRotation = 0.f;
      auto headRotationEnabled = root().humanoidHeadRotation();
      if (!headRotationEnabled)
        return std::unexpected(std::move(headRotationEnabled.error()));
      if (headRotationEnabled->get() && (humanoid()->handHoldingItem(ToolHand::Primary) || humanoid()->handHoldingItem(ToolHand::Alt) || humanoid()->dance())) {
        auto primary = m_tools->primaryHandItem();
        auto alt = m_tools->altHandItem();
        String const disableFlag = "disableHeadRotation";
        auto statusFlag = m_statusController->statusProperty(disableFlag);
        bool disabled = false;
        if (statusFlag.isType(Json::Type::Bool)) {
          auto flagResult = statusFlag.toBool();
          if (!flagResult)
            return std::unexpected(std::move(flagResult.error()));
          auto flag = std::move(*flagResult);
          disabled = flag;
        }
        if (!disabled && primary) {
          auto flagResult = primary->instanceValue(disableFlag);
          if (!flagResult)
            return std::unexpected(std::move(flagResult.error()));
          auto flag = std::move(*flagResult);
          disabled = bool(flag);
        }
        if (!disabled && alt) {
          auto flagResult = alt->instanceValue(disableFlag);
          if (!flagResult)
            return std::unexpected(std::move(flagResult.error()));
          auto flag = std::move(*flagResult);
          disabled = bool(flag);
        }
        if (!disabled) {
          auto mouthResult = mouthPosition();
          if (!mouthResult)
            return std::unexpected(std::move(mouthResult.error()));
          auto mouth = std::move(*mouthResult);
          auto diff = currentWorld.geometry().diff(m_aimPosition, mouth);
          diff.setX(std::fabsf(diff.x()));
          headRotation = diff.angle() * .25f * numericalDirection(humanoid()->facingDirection());
        }
      }
      humanoid()->setHeadRotation(headRotation);
      if (isMaster())
        {
          if (auto result = setSecretProperty("humanoid.headRotation", headRotation); !result)
            return std::unexpected(std::move(result.error()));
        }
    }
  }
  
  if (isMaster()) {
    for (auto& p : m_genericScriptContexts) {
      if (auto result = p.second->invoke("postUpdate"); !result)
        return std::unexpected(std::move(result.error()));
    }
  }

  m_pendingMoves.clear();

  if (isClient)
    root().spatialLogger().logPoly("world", m_movementController->collisionBody(), isMaster() ? Color::Orange.toRgba() : Color::Yellow.toRgba());
  return {};
}

float Player::timeSinceLastGaveDamage() const {
  return m_lastDamagedOtherTimer;
}

EntityId Player::lastDamagedTarget() const {
  return m_lastDamagedTarget;
}

std::expected<void, StarException> Player::render(RenderCallback& renderCallback) {
  if (invisible()) {
    m_techController->pullNewAudios();
    m_techController->pullNewParticles();
    m_statusController->pullNewAudios();
    m_statusController->pullNewParticles();
    m_humanoidDynamicTarget.pullNewAudios();
    m_humanoidDynamicTarget.pullNewParticles();
    return {};
  }
  Vec2I sensor = Vec2I((m_config->footstepSensor + m_movementController->position()).floor());
  auto soundResult = getFootstepSound(sensor);
  if (!soundResult)
    return std::unexpected(std::move(soundResult.error()));
  auto sound = std::move(*soundResult);
  if (!sound.empty() && !m_techController->parentState() && !m_techController->parentHidden()) {
    auto assetsResult = root().assets();
    if (!assetsResult)
      return std::unexpected(std::move(assetsResult.error()));
    auto assets = std::move(*assetsResult);
    auto audioResult = assets->audio(sound);
    if (!audioResult)
      return std::unexpected(std::move(audioResult.error()));
    auto audio = std::move(*audioResult);
    if (m_landingNoisePending) {
      auto noiseResult = AudioInstance::create(*audio);
      if (!noiseResult)
        return std::unexpected(std::move(noiseResult.error()));
      auto noise = std::move(*noiseResult);
      auto offsetResult = feetOffset();
      if (!offsetResult)
        return std::unexpected(std::move(offsetResult.error()));
      auto offset = std::move(*offsetResult);
      noise->setPosition(position() + offset);
      noise->setVolume(m_landingVolume);
      if (auto result = renderCallback.addAudio(std::move(noise)); !result)
        return std::unexpected(std::move(result.error()));
    }
    if (m_footstepPending) {
      auto noiseResult = AudioInstance::create(*audio);
      if (!noiseResult)
        return std::unexpected(std::move(noiseResult.error()));
      auto noise = std::move(*noiseResult);
      auto offsetResult = feetOffset();
      if (!offsetResult)
        return std::unexpected(std::move(offsetResult.error()));
      auto offset = std::move(*offsetResult);
      noise->setPosition(position() + offset);
      auto varianceResult = root().random().randf(0, m_footstepVolumeVariance);
      if (!varianceResult)
        return std::unexpected(std::move(varianceResult.error()));
      auto variance = std::move(*varianceResult);
      noise->setVolume(1 - variance);
      if (auto result = renderCallback.addAudio(std::move(noise)); !result)
        return std::unexpected(std::move(result.error()));
    }
  } else {
    m_footstepTimer = m_config->footstepTiming;
  }
  m_footstepPending = false;
  m_landingNoisePending = false;
  if (auto result = renderCallback.addAudios(m_effectsAnimatorDynamicTarget.pullNewAudios()); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = renderCallback.addParticles(m_effectsAnimatorDynamicTarget.pullNewParticles()); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = renderCallback.addAudios(m_techController->pullNewAudios()); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = renderCallback.addAudios(m_statusController->pullNewAudios()); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = renderCallback.addAudios(m_humanoidDynamicTarget.pullNewAudios()); !result)
    return std::unexpected(std::move(result.error()));
  for (auto const& pending : take(m_callbackSounds)) {
    auto assetsResult = root().assets();
    if (!assetsResult)
      return std::unexpected(std::move(assetsResult.error()));
    auto assets = std::move(*assetsResult);
    auto sampleResult = assets->audio(std::get<0>(pending));
    if (!sampleResult)
      return std::unexpected(std::move(sampleResult.error()));
    auto sample = std::move(*sampleResult);
    auto audioInstance = AudioInstance::create(*sample);
    if (!audioInstance)
      return std::unexpected(std::move(audioInstance.error()));
    auto audio = std::move(*audioInstance);
    audio->setVolume(std::get<1>(pending));
    audio->setPitchMultiplier(std::get<2>(pending));
    audio->setPosition(position());
    if (auto result = renderCallback.addAudio(std::move(audio)); !result)
      return std::unexpected(std::move(result.error()));
  }
  auto loungeAnchor = as<LoungeAnchor>(m_movementController->entityAnchor());
  EntityRenderLayer layer = RenderLayerPlayer;
  auto overrideLayerResult = getSecretProperty("overrideRenderLayer");
  if (!overrideLayerResult)
    return std::unexpected(std::move(overrideLayerResult.error()));
  auto overrideLayer = std::move(*overrideLayerResult);
  if (overrideLayer.canConvert(Json::Type::Int)) {
    auto valueResult = overrideLayer.toUInt();
    if (!valueResult)
      return std::unexpected(std::move(valueResult.error()));
    auto value = std::move(*valueResult);
    layer = value;
  }
  if (loungeAnchor)
    layer = loungeAnchor->loungeRenderLayer;
  auto renderedResult = drawables();
  if (!renderedResult)
    return std::unexpected(std::move(renderedResult.error()));
  auto rendered = std::move(*renderedResult);
  if (auto result = renderCallback.addDrawables(std::move(rendered), layer); !result)
    return std::unexpected(std::move(result.error()));
  if (!isTeleporting()) {
    auto overheadResult = bars();
    if (!overheadResult)
      return std::unexpected(std::move(overheadResult.error()));
    auto overhead = std::move(*overheadResult);
    if (auto result = renderCallback.addOverheadBars(std::move(overhead), position()); !result)
      return std::unexpected(std::move(result.error()));
  }
  auto emittedResult = particles();
  if (!emittedResult)
    return std::unexpected(std::move(emittedResult.error()));
  auto emitted = std::move(*emittedResult);
  if (auto result = renderCallback.addParticles(std::move(emitted)); !result)
    return std::unexpected(std::move(result.error()));
  auto rangeResult = inToolRange();
  if (!rangeResult)
    return std::unexpected(std::move(rangeResult.error()));
  auto range = std::move(*rangeResult);
  if (auto result = m_tools->render(renderCallback, range, m_shifting, layer); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = m_effectEmitter->render(renderCallback); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = m_songbook->render(renderCallback); !result)
    return std::unexpected(std::move(result.error()));
  if (isMaster())
    return m_deployment->render(renderCallback, position());
  return {};
}

std::expected<void, StarException> Player::renderLightSources(RenderCallback& renderCallback) {
  auto lightsResult = lightSources();
  if (!lightsResult)
    return std::unexpected(std::move(lightsResult.error()));
  auto lights = std::move(*lightsResult);
  if (auto result = renderCallback.addLightSources(std::move(lights)); !result)
    return std::unexpected(std::move(result.error()));
  return m_deployment->renderLightSources(renderCallback);
}

std::expected<void, StarException> Player::setRenderLayer(std::optional<EntityRenderLayer> layer) {
  return setSecretProperty("overrideRenderLayer", layer ? Json(*layer) : Json());
}

Json Player::getGenericProperty(String const& name, Json const& defaultValue) const {
  return m_genericProperties.value(name, defaultValue);
}

void Player::setGenericProperty(String const& name, Json const& value) {
  if (value.isNull())
    m_genericProperties.erase(name);
  else
    m_genericProperties.set(name, value);
}

std::shared_ptr<PlayerInventory> Player::inventory() const {
  return m_inventory;
}

std::expected<std::uint64_t, StarException> Player::itemsCanHold(std::shared_ptr<Item> const& items) const {
  return m_inventory->itemsCanFit(items);
}

std::expected<std::shared_ptr<Item>, StarException> Player::pickupItems(std::shared_ptr<Item> const& items, bool silent) {
  if (isDead() || !items)
    return items;
  auto capacityResult = m_inventory->itemsCanFit(items);
  if (!capacityResult)
    return std::unexpected(std::move(capacityResult.error()));
  auto capacity = std::move(*capacityResult);
  if (capacity == 0)
    return items;
  if (auto result = triggerPickupEvents(items); !result)
    return std::unexpected(std::move(result.error()));
  if (!silent) {
    auto soundResult = items->pickupSound();
    if (!soundResult)
      return std::unexpected(std::move(soundResult.error()));
    auto sound = std::move(*soundResult);
    if (!sound.empty()) {
      if (auto result = m_effectsAnimator->setSoundPool("pickup", {sound}); !result)
        return std::unexpected(std::move(result.error()));
      float pitch = 1.f - ((float)items->count() / (float)items->maxStack()) * 0.5f;
      auto varianceResult = root().random().randf(0.8f, 1.2f);
      if (!varianceResult)
        return std::unexpected(std::move(varianceResult.error()));
      auto variance = std::move(*varianceResult);
      if (auto result = m_effectsAnimator->setSoundPitchMultiplier("pickup", std::clamp(pitch * variance, 0.f, 2.f));
          !result)
        return std::unexpected(std::move(result.error()));
      if (auto result = m_effectsAnimator->playSound("pickup"); !result)
        return std::unexpected(std::move(result.error()));
    }
    auto databaseResult = root().itemDatabase();
    if (!databaseResult)
      return std::unexpected(std::move(databaseResult.error()));
    auto database = std::move(*databaseResult);
    auto sharedResult = database->itemShared(items->descriptor());
    if (!sharedResult)
      return std::unexpected(std::move(sharedResult.error()));
    auto shared = std::move(*sharedResult);
    queueItemPickupMessage(shared);
  }
  return m_inventory->addItems(items);
}

std::expected<void, StarException> Player::giveItem(std::shared_ptr<Item> const& item) {
  auto spillResult = pickupItems(item, false);
  if (!spillResult)
    return std::unexpected(std::move(spillResult.error()));
  auto spill = std::move(*spillResult);
  if (spill) {
    auto currentWorldResult = world();
    if (!currentWorldResult)
      return std::unexpected(std::move(currentWorldResult.error()));
    auto& currentWorld = currentWorldResult->get();
    auto dropResult = ItemDrop::createRandomizedDrop(root(), spill->descriptor(), position());
    if (!dropResult)
      return std::unexpected(std::move(dropResult.error()));
    auto drop = std::move(*dropResult);
    return currentWorld.addEntity(std::move(drop));
  }
  return {};
}

std::expected<void, StarException> Player::triggerPickupEvents(std::shared_ptr<Item> const& item) {
  if (!item)
    return {};
  for (auto const& blueprint : item->learnBlueprintsOnPickup()) {
    if (auto result = addBlueprint(blueprint); !result)
      return std::unexpected(std::move(result.error()));
  }
  for (auto const& collectable : item->collectablesOnPickup()) {
    if (auto result = addCollectable(collectable.first, collectable.second); !result)
      return std::unexpected(std::move(result.error()));
  }
  auto messagesResult = item->instanceValue("radioMessagesOnPickup", JsonArray());
  if (!messagesResult)
    return std::unexpected(std::move(messagesResult.error()));
  auto messages = std::move(*messagesResult);
  auto messageArrayResult = messages.toArray();
  if (!messageArrayResult)
    return std::unexpected(std::move(messageArrayResult.error()));
  auto messageArray = std::move(*messageArrayResult);
  for (auto const& message : messageArray) {
    if (message.isType(Json::Type::Array)) {
      auto partsResult = message.toArray();
      if (!partsResult)
        return std::unexpected(std::move(partsResult.error()));
      auto parts = std::move(*partsResult);
      if (parts.size() >= 2 && parts[1].canConvert(Json::Type::Float)) {
        auto delayResult = parts[1].toFloat();
        if (!delayResult)
          return std::unexpected(std::move(delayResult.error()));
        auto delay = std::move(*delayResult);
        if (auto result = queueRadioMessage(parts[0], delay); !result)
          return std::unexpected(std::move(result.error()));
      }
    } else {
      if (auto result = queueRadioMessage(message); !result)
        return std::unexpected(std::move(result.error()));
    }
  }
  auto cinematicResult = item->instanceValue("cinematicOnPickup", Json());
  if (!cinematicResult)
    return std::unexpected(std::move(cinematicResult.error()));
  auto cinematic = std::move(*cinematicResult);
  if (cinematic) {
    if (auto result = setPendingCinematic(cinematic, true); !result)
      return std::unexpected(std::move(result.error()));
  }
  auto questsResult = item->pickupQuestTemplates();
  if (!questsResult)
    return std::unexpected(std::move(questsResult.error()));
  auto quests = std::move(*questsResult);
  for (auto const& arc : quests) {
    auto canStartResult = m_questManager->canStart(arc);
    if (!canStartResult)
      return std::unexpected(std::move(canStartResult.error()));
    auto canStart = std::move(*canStartResult);
    if (canStart) {
      auto questResult = Quest::create(root(), arc, 0, *this);
      if (!questResult)
        return std::unexpected(std::move(questResult.error()));
      auto quest = std::move(*questResult);
      if (auto result = m_questManager->offer(quest); !result)
        return std::unexpected(std::move(result.error()));
    }
  }
  auto consumeResult = item->instanceValue("consumeOnPickup", Json());
  if (!consumeResult)
    return std::unexpected(std::move(consumeResult.error()));
  auto consume = std::move(*consumeResult);
  if (consume) {
    auto enabledResult = consume.toBool();
    if (!enabledResult)
      return std::unexpected(std::move(enabledResult.error()));
    auto enabled = std::move(*enabledResult);
    if (enabled)
      item->consume(item->count());
  }
  auto categoryResult = item->instanceValue("eventCategory", item->category());
  if (!categoryResult)
    return std::unexpected(std::move(categoryResult.error()));
  auto category = std::move(*categoryResult);
  statistics()->recordEvent("item", JsonObject{{"itemName", item->name()}, {"count", item->count()}, {"category", std::move(category)}});
  return {};
}

std::shared_ptr<Item> Player::essentialItem(EssentialItem essentialItem) const {
  return m_inventory->essentialItem(essentialItem);
}

bool Player::hasItem(ItemDescriptor const& descriptor, bool exactMatch) const {
  return m_inventory->hasItem(descriptor, exactMatch);
}

std::uint64_t Player::hasCountOfItem(ItemDescriptor const& descriptor, bool exactMatch) const {
  return m_inventory->hasCountOfItem(descriptor, exactMatch);
}

std::expected<ItemDescriptor, StarException> Player::takeItem(ItemDescriptor const& descriptor, bool consumePartial, bool exactMatch) {
  return m_inventory->takeItems(descriptor, consumePartial, exactMatch);
}

std::expected<void, StarException> Player::giveItem(ItemDescriptor const& descriptor) {
  auto databaseResult = root().itemDatabase();
  if (!databaseResult)
    return std::unexpected(std::move(databaseResult.error()));
  auto database = std::move(*databaseResult);
  auto itemResult = database->item(descriptor);
  if (!itemResult)
    return std::unexpected(std::move(itemResult.error()));
  auto item = std::move(*itemResult);
  return giveItem(item);
}

std::expected<void, StarException> Player::clearSwap() {
  auto clearedResult = m_inventory->clearSwap();
  if (!clearedResult)
    return std::unexpected(std::move(clearedResult.error()));
  auto cleared = std::move(*clearedResult);
  if (!cleared) {
    if (auto currentWorld = maybeWorld()) {
      auto itemResult = m_inventory->takeSlot(SwapSlot());
      if (!itemResult)
        return std::unexpected(std::move(itemResult.error()));
      auto item = std::move(*itemResult);
      auto dropResult = ItemDrop::createRandomizedDrop(root(), item, position());
      if (!dropResult)
        return std::unexpected(std::move(dropResult.error()));
      auto drop = std::move(*dropResult);
      if (auto result = currentWorld->get().addEntity(std::move(drop)); !result)
        return std::unexpected(std::move(result.error()));
    }
  }
  endPrimaryFire();
  endAltFire();
  endTrigger();
  return {};
}

std::expected<void, StarException> Player::refreshItems() {
  if (isSlave())
    return {};
  auto primaryResult = m_inventory->primaryHeldItem();
  if (!primaryResult)
    return std::unexpected(std::move(primaryResult.error()));
  auto primary = std::move(*primaryResult);
  auto secondaryResult = m_inventory->secondaryHeldItem();
  if (!secondaryResult)
    return std::unexpected(std::move(secondaryResult.error()));
  auto secondary = std::move(*secondaryResult);
  return m_tools->setItems(std::move(primary), std::move(secondary));
}

std::expected<void, StarException> Player::refreshArmor() {
  if (isSlave())
    return {};

  bool shouldSetArmorSecrets = m_clientContext && m_clientContext->netCompatibilityRules().version() < 9;
  for (std::uint8_t i = 0; i != 20; ++i) {
    auto slot = (EquipmentSlot)i;
    auto item = m_inventory->equipment(slot);
    bool visible = m_inventory->equipmentVisibility(slot);
    if (m_armor->setItem(i, item, visible)) {
      if (slot >= EquipmentSlot::Cosmetic1 && shouldSetArmorSecrets)
        {
          if (auto result = setNetArmorSecret(slot, item, visible); !result)
            return std::unexpected(std::move(result.error()));
        }
    }
  }
  return {};
}

std::expected<void, StarException> Player::refreshHumanoid() const {
  auto refreshed = [&]() -> std::expected<void, StarException> {
    auto changedResult = m_armor->setupHumanoid(*humanoid(), forceNude());
    if (!changedResult)
      return std::unexpected(std::move(changedResult.error()));
    auto changed = std::move(*changedResult);
    if (changed) {
      auto parametersResult = ActorMovementParameters::create(jsonMerge(humanoid()->defaultMovementParameters(), humanoid()->playerMovementParameters().value_or(m_config->movementParameters)));
      if (!parametersResult)
        return std::unexpected(std::move(parametersResult.error()));
      auto parameters = std::move(*parametersResult);
      return m_movementController->resetBaseParameters(parameters);
    }
    return {};
  }();
  if (!refreshed && isMaster())
    return std::unexpected(std::move(refreshed.error()));
  return {};
}

std::expected<void, StarException> Player::refreshEquipment() {
  {
    if (auto result = refreshArmor(); !result)
      return std::unexpected(std::move(result.error()));
  }
  {
    if (auto result = refreshItems(); !result)
      return std::unexpected(std::move(result.error()));
  }
  return {};
}

std::shared_ptr<PlayerBlueprints> Player::blueprints() const {
  return m_blueprints;
}

std::expected<bool, StarException> Player::addBlueprint(ItemDescriptor const& descriptor, bool showFailure) {
  if (descriptor.isNull())
    return false;
  bool const known = m_blueprints->isKnown(descriptor);
  if (known && !showFailure)
    return false;
  auto databaseResult = root().itemDatabase();
  if (!databaseResult)
    return std::unexpected(std::move(databaseResult.error()));
  auto database = std::move(*databaseResult);
  auto itemResult = database->item(descriptor);
  if (!itemResult)
    return std::unexpected(std::move(itemResult.error()));
  auto item = std::move(*itemResult);
  auto assetsResult = root().assets();
  if (!assetsResult)
    return std::unexpected(std::move(assetsResult.error()));
  auto assets = std::move(*assetsResult);
  if (!known) {
    m_blueprints->add(descriptor);
    auto messageResult = assets->json("/player.config:blueprintUnlock");
    if (!messageResult)
      return std::unexpected(std::move(messageResult.error()));
    auto message = std::move(*messageResult);
    auto textResult = message.toString();
    if (!textResult)
      return std::unexpected(std::move(textResult.error()));
    auto text = std::move(*textResult);
    auto formattedResult = text.replace("<ItemName>", item->friendlyName());
    if (!formattedResult)
      return std::unexpected(std::move(formattedResult.error()));
    auto formatted = std::move(*formattedResult);
    if (auto result = queueUIMessage(formatted); !result)
      return std::unexpected(std::move(result.error()));
    return true;
  }
  if (showFailure) {
    auto messageResult = assets->json("/player.config:blueprintAlreadyKnown");
    if (!messageResult)
      return std::unexpected(std::move(messageResult.error()));
    auto message = std::move(*messageResult);
    auto textResult = message.toString();
    if (!textResult)
      return std::unexpected(std::move(textResult.error()));
    auto text = std::move(*textResult);
    auto formattedResult = text.replace("<ItemName>", item->friendlyName());
    if (!formattedResult)
      return std::unexpected(std::move(formattedResult.error()));
    auto formatted = std::move(*formattedResult);
    if (auto result = queueUIMessage(formatted); !result)
      return std::unexpected(std::move(result.error()));
  }
  return false;
}

bool Player::blueprintKnown(ItemDescriptor const& descriptor) const {
  if (descriptor.isNull())
    return false;

  return m_blueprints->isKnown(descriptor);
}

std::expected<bool, StarException> Player::addCollectable(String const& collectionName, String const& collectableName) {
  if (!m_log->addCollectable(collectionName, collectableName))
    return false;
  auto databaseResult = root().collectionDatabase();
  if (!databaseResult)
    return std::unexpected(std::move(databaseResult.error()));
  auto database = std::move(*databaseResult);
  auto collectionResult = database->collection(collectionName);
  if (!collectionResult)
    return std::unexpected(std::move(collectionResult.error()));
  auto collection = std::move(*collectionResult);
  auto collectableResult = database->collectable(collectionName, collectableName);
  if (!collectableResult)
    return std::unexpected(std::move(collectableResult.error()));
  auto collectable = std::move(*collectableResult);
  auto assetsResult = root().assets();
  if (!assetsResult)
    return std::unexpected(std::move(assetsResult.error()));
  auto assets = std::move(*assetsResult);
  auto messageResult = assets->json("/player.config:collectableUnlock");
  if (!messageResult)
    return std::unexpected(std::move(messageResult.error()));
  auto message = std::move(*messageResult);
  auto textResult = message.toString();
  if (!textResult)
    return std::unexpected(std::move(textResult.error()));
  auto text = std::move(*textResult);
  auto collectableTextResult = text.replace("<collectable>", collectable.title);
  if (!collectableTextResult)
    return std::unexpected(std::move(collectableTextResult.error()));
  auto collectableText = std::move(*collectableTextResult);
  auto formattedResult = collectableText.replace("<collection>", collection.title);
  if (!formattedResult)
    return std::unexpected(std::move(formattedResult.error()));
  auto formatted = std::move(*formattedResult);
  if (auto result = queueUIMessage(formatted); !result)
    return std::unexpected(std::move(result.error()));
  return true;
}

std::shared_ptr<PlayerUniverseMap> Player::universeMap() const {
  return m_universeMap;
}

std::shared_ptr<PlayerCodexes> Player::codexes() const {
  return m_codexes;
}

std::shared_ptr<PlayerTech> Player::techs() const {
  return m_techs;
}

std::expected<void, StarException> Player::overrideTech(std::optional<StringList> const& techModules) {
  if (techModules)
    {
      if (auto result = m_techController->setOverrideTech(*techModules); !result)
        return std::unexpected(std::move(result.error()));
    }
  else
    {
      if (auto result = m_techController->clearOverrideTech(); !result)
        return std::unexpected(std::move(result.error()));
    }
  return {};
}

bool Player::techOverridden() const {
  return m_techController->techOverridden();
}

std::shared_ptr<PlayerCompanions> Player::companions() const {
  return m_companions;
}

std::shared_ptr<PlayerLog> Player::log() const {
  return m_log;
}

std::expected<std::shared_ptr<InteractiveEntity>, StarException> Player::bestInteractionEntity(bool includeNearby) {
  if (!inWorld())
    return std::shared_ptr<InteractiveEntity>{};
  auto currentWorldResult = world();
  if (!currentWorldResult)
    return std::unexpected(std::move(currentWorldResult.error()));
  auto& currentWorld = currentWorldResult->get();
  auto entityResult = currentWorld.getInteractiveInRange(m_aimPosition, isAdmin() ? m_aimPosition : position(), m_interactRadius);
  if (!entityResult)
    return std::unexpected(std::move(entityResult.error()));
  auto entity = std::move(*entityResult);
  if (!entity && includeNearby) {
    Vec2F bias = m_walkIntoInteractBias;
    if (facingDirection() == Direction::Left)
      bias[0] *= -1;
    auto nearbyResult = currentWorld.getInteractiveInRange(position() + bias, position(), m_interactRadius);
    if (!nearbyResult)
      return std::unexpected(std::move(nearbyResult.error()));
    auto nearby = std::move(*nearbyResult);
    entity = std::move(nearby);
  }
  if (!entity)
    return std::shared_ptr<InteractiveEntity>{};
  if (isAdmin())
    return entity;
  auto reachableResult = currentWorld.canReachEntity(position(), m_interactRadius, entity->entityId());
  if (!reachableResult)
    return std::unexpected(std::move(reachableResult.error()));
  auto reachable = std::move(*reachableResult);
  return reachable ? entity : std::shared_ptr<InteractiveEntity>{};
}

std::expected<void, StarException> Player::interactWithEntity(std::shared_ptr<InteractiveEntity> entity) {
  bool questIntercepted = false;
  for (auto const& quest : m_questManager->listActiveQuests()) {
    auto interceptedResult = quest->interactWithEntity(entity->entityId());
    if (!interceptedResult)
      return std::unexpected(std::move(interceptedResult.error()));
    auto intercepted = std::move(*interceptedResult);
    if (intercepted)
      questIntercepted = true;
  }
  if (questIntercepted)
    return {};
  bool anyTurnedIn = false;
  auto turnInsResult = entity->turnInQuests();
  if (!turnInsResult)
    return std::unexpected(std::move(turnInsResult.error()));
  auto turnIns = std::move(*turnInsResult);
  for (auto const& questId : turnIns) {
    if (m_questManager->canTurnIn(questId)) {
      auto questResult = m_questManager->getQuest(questId);
      if (!questResult)
        return std::unexpected(std::move(questResult.error()));
      auto quest = std::move(*questResult);
      if (auto result = quest->setEntityParameter("questReceiver", entity); !result)
        return std::unexpected(std::move(result.error()));
      if (auto result = quest->complete(); !result)
        return std::unexpected(std::move(result.error()));
      anyTurnedIn = true;
    }
  }
  if (anyTurnedIn)
    return {};
  auto offersResult = entity->offeredQuests();
  if (!offersResult)
    return std::unexpected(std::move(offersResult.error()));
  auto offers = std::move(*offersResult);
  for (auto const& arc : offers) {
    auto canStartResult = m_questManager->canStart(arc);
    if (!canStartResult)
      return std::unexpected(std::move(canStartResult.error()));
    auto canStart = std::move(*canStartResult);
    if (canStart) {
      auto questResult = Quest::create(root(), arc, 0, *this);
      if (!questResult)
        return std::unexpected(std::move(questResult.error()));
      auto quest = std::move(*questResult);
      quest->setWorldId(clientContext()->playerWorldId());
      quest->setServerUuid(clientContext()->serverUuid());
      if (auto result = quest->setEntityParameter("questGiver", entity); !result)
        return std::unexpected(std::move(result.error()));
      return m_questManager->offer(quest);
    }
  }
  auto currentWorldResult = world();
  if (!currentWorldResult)
    return std::unexpected(std::move(currentWorldResult.error()));
  auto& currentWorld = currentWorldResult->get();
  auto actionResult = currentWorld.interact(InteractRequest{entityId(), position(), entity->entityId(), m_aimPosition});
  if (!actionResult)
    return std::unexpected(std::move(actionResult.error()));
  auto action = std::move(*actionResult);
  m_pendingInteractActions.append(std::move(action));
  return {};
}

void Player::aim(Vec2F const& position) {
  m_techController->setAimPosition(position);
  m_aimPosition = position;
}

std::expected<Vec2F, StarException> Player::aimPosition() const {
  return m_aimPosition;
}

std::expected<Vec2F, StarException> Player::armPosition(ToolHand hand, Direction facingDirection, float armAngle, Vec2F offset) const {
  return m_tools->armPosition(*humanoid(), hand, facingDirection, armAngle, offset);
}

std::expected<Vec2F, StarException> Player::handOffset(ToolHand hand, Direction facingDirection) const {
  return m_tools->handOffset(*humanoid(), hand, facingDirection);
}

std::expected<Vec2F, StarException> Player::handPosition(ToolHand hand, Vec2F const& handOffset) const {
  return m_tools->handPosition(hand, *humanoid(), handOffset);
}

std::shared_ptr<Item> Player::handItem(ToolHand hand) const {
  if (hand == ToolHand::Primary)
    return m_tools->primaryHandItem();
  else
    return m_tools->altHandItem();
}

std::expected<Vec2F, StarException> Player::armAdjustment() const {
  return humanoid()->armAdjustment();
}

void Player::setCameraFocusEntity(std::optional<EntityId> const& cameraFocusEntity) {
  m_cameraFocusEntity = cameraFocusEntity;
}

std::expected<void, StarException> Player::playEmote(HumanoidEmote emote) {
  addEmote(emote);
  return {};
}

bool Player::canUseTool() const {
  bool canUse = !isDead() && !isTeleporting() && !m_techController->toolUsageSuppressed() && !m_statusController->toolUsageSuppressed();
  if (canUse) {
    if (auto loungeAnchor = as<LoungeAnchor>(m_movementController->entityAnchor()))
      if (loungeAnchor->suppressTools.value_or(loungeAnchor->controllable))
        return false;
  }
  return canUse;
}

void Player::beginPrimaryFire() {
  m_techController->beginPrimaryFire();
  m_tools->beginPrimaryFire();
}

void Player::beginAltFire() {
  m_techController->beginAltFire();
  m_tools->beginAltFire();
}

void Player::endPrimaryFire() {
  m_techController->endPrimaryFire();
  m_tools->endPrimaryFire();
}

void Player::endAltFire() {
  m_techController->endAltFire();
  m_tools->endAltFire();
}

void Player::beginTrigger() {
  if (!m_useDown)
    m_edgeTriggeredUse = true;
  m_useDown = true;
}

void Player::endTrigger() {
  m_useDown = false;
}

std::expected<float, StarException> Player::toolRadius() const {
  auto radiusResult = m_tools->toolRadius();
  if (!radiusResult)
    return std::unexpected(std::move(radiusResult.error()));
  auto radius = std::move(*radiusResult);
  if (radius)
    return *radius;
  return m_interactRadius;
}

std::expected<float, StarException> Player::interactRadius() const {
  return m_interactRadius;
}

void Player::setInteractRadius(float interactRadius) {
  m_interactRadius = interactRadius;
}

std::expected<List<InteractAction>, StarException> Player::pullInteractActions() {
  List<InteractAction> results;
  for (auto it = m_pendingInteractActions.begin(); it != m_pendingInteractActions.end();) {
    auto responseResult = it->result();
    if (!responseResult)
      return std::unexpected(std::move(responseResult.error()));
    auto response = std::move(*responseResult);
    if (response)
      results.append(std::move(*response));
    auto finishedResult = it->finished();
    if (!finishedResult)
      return std::unexpected(std::move(finishedResult.error()));
    auto finished = std::move(*finishedResult);
    if (finished)
      it = m_pendingInteractActions.erase(it);
    else
      ++it;
  }
  return results;
}

std::uint64_t Player::currency(String const& currencyType) const {
  return m_inventory->currency(currencyType);
}

float Player::health() const {
  return m_statusController->resource("health");
}

std::expected<float, StarException> Player::maxHealth() const {
  auto valueResult = m_statusController->resourceMax("health");
  if (!valueResult)
    return std::unexpected(std::move(valueResult.error()));
  auto value = std::move(*valueResult);
  if (!value)
    return std::unexpected(PlayerException("Missing required player resource 'health'"));
  return *value;
}

DamageBarType Player::damageBar() const {
  return DamageBarType::Default;
}

std::expected<float, StarException> Player::healthPercentage() const {
  auto valueResult = m_statusController->resourcePercentage("health");
  if (!valueResult)
    return std::unexpected(std::move(valueResult.error()));
  auto value = std::move(*valueResult);
  if (!value)
    return std::unexpected(PlayerException("Missing required player resource 'health'"));
  return *value;
}

std::expected<float, StarException> Player::energy() const {
  return m_statusController->resource("energy");
}

std::expected<float, StarException> Player::maxEnergy() const {
  auto valueResult = m_statusController->resourceMax("energy");
  if (!valueResult)
    return std::unexpected(std::move(valueResult.error()));
  auto value = std::move(*valueResult);
  if (!value)
    return std::unexpected(PlayerException("Missing required player resource 'energy'"));
  return *value;
}

std::expected<float, StarException> Player::energyPercentage() const {
  auto valueResult = m_statusController->resourcePercentage("energy");
  if (!valueResult)
    return std::unexpected(std::move(valueResult.error()));
  auto value = std::move(*valueResult);
  if (!value)
    return std::unexpected(PlayerException("Missing required player resource 'energy'"));
  return *value;
}

std::expected<float, StarException> Player::energyRegenBlockPercent() const {
  auto valueResult = m_statusController->resourcePercentage("energyRegenBlock");
  if (!valueResult)
    return std::unexpected(std::move(valueResult.error()));
  auto value = std::move(*valueResult);
  if (!value)
    return std::unexpected(PlayerException("Missing required player resource 'energyRegenBlock'"));
  return *value;
}

std::expected<bool, StarException> Player::fullEnergy() const {
  auto maximumResult = maxEnergy();
  if (!maximumResult)
    return std::unexpected(std::move(maximumResult.error()));
  auto maximum = std::move(*maximumResult);
  return m_statusController->resource("energy") >= maximum;
}

std::expected<bool, StarException> Player::energyLocked() const {
  return m_statusController->resourceLocked("energy");
}

std::expected<bool, StarException> Player::consumeEnergy(float energy) {
  if (m_isAdmin)
    return true;
  return m_statusController->overConsumeResource("energy", energy);
}

std::expected<float, StarException> Player::foodPercentage() const {
  auto valueResult = m_statusController->resourcePercentage("food");
  if (!valueResult)
    return std::unexpected(std::move(valueResult.error()));
  auto value = std::move(*valueResult);
  if (!value)
    return std::unexpected(PlayerException("Missing required player resource 'food'"));
  return *value;
}

float Player::breath() const {
  return m_statusController->resource("breath");
}

std::expected<float, StarException> Player::maxBreath() const {
  auto valueResult = m_statusController->resourceMax("breath");
  if (!valueResult)
    return std::unexpected(std::move(valueResult.error()));
  auto value = std::move(*valueResult);
  if (!value)
    return std::unexpected(PlayerException("Missing required player resource 'breath'"));
  return *value;
}

float Player::protection() const {
  return m_statusController->stat("protection");
}

bool Player::forceNude() const {
  return m_statusController->statPositive("nude");
}

std::expected<String, StarException> Player::description() const {
  return m_description;
}

void Player::setDescription(String const& description) {
  m_description = description;
}

Direction Player::walkingDirection() const {
  return m_movementController->movingDirection();
}

Direction Player::facingDirection() const {
  return m_movementController->facingDirection();
}

std::expected<std::pair<ByteArray, std::uint64_t>, StarException> Player::writeNetState(std::uint64_t fromVersion, NetCompatibilityRules rules) {
  return m_netGroup.writeNetState(fromVersion, rules);
}

std::expected<void, StarException> Player::readNetState(ByteArray data, float interpolationTime, NetCompatibilityRules rules) {
  return m_netGroup.readNetState(data, interpolationTime, rules);
}

std::expected<void, StarException> Player::enableInterpolation(float) {
  return m_netGroup.enableNetInterpolation();
}

std::expected<void, StarException> Player::disableInterpolation() {
  return m_netGroup.disableNetInterpolation();
}

std::expected<void, StarException> Player::processControls() {
  auto currentWorldResult = world();
  if (!currentWorldResult)
    return std::unexpected(std::move(currentWorldResult.error()));
  auto& currentWorld = currentWorldResult->get();
  bool run = !m_shifting && !m_statusController->statPositive("encumberance");

  bool useMoveVector = m_moveVector.x() != 0.0f;
  if (useMoveVector) {
    for (auto move : m_pendingMoves) {
      if (move == MoveControlType::Left || move == MoveControlType::Right) {
        useMoveVector = false;
        break;
      }
    }
  }

  if (useMoveVector) {
    m_pendingMoves.insert(m_moveVector.x() < 0.0f ? MoveControlType::Left : MoveControlType::Right);
    m_movementController->setMoveSpeedMultiplier(std::clamp(std::abs(m_moveVector.x()), 0.0f, 1.0f));
  }
  else
    m_movementController->setMoveSpeedMultiplier(1.0f);

  if (auto fireableMain = as<FireableItem>(m_tools->primaryHandItem())) {
    if (fireableMain->inUse() && fireableMain->walkWhileFiring())
      run = false;
  }

  if (auto fireableAlt = as<FireableItem>(m_tools->altHandItem())) {
    if (fireableAlt->inUse() && fireableAlt->walkWhileFiring())
      run = false;
  }

  bool move = true;

  if (auto fireableMain = as<FireableItem>(m_tools->primaryHandItem())) {
    if (fireableMain->inUse() && fireableMain->stopWhileFiring())
      move = false;
  }

  if (auto fireableAlt = as<FireableItem>(m_tools->altHandItem())) {
    if (fireableAlt->inUse() && fireableAlt->stopWhileFiring())
      move = false;
  }

  auto loungeAnchor = as<LoungeAnchor>(m_movementController->entityAnchor());
  if (loungeAnchor && loungeAnchor->controllable) {
    auto anchorState = m_movementController->anchorState();
    if (!anchorState)
      return std::unexpected(PlayerException("Player lounge anchor has no anchor state"));
    if (auto loungeableEntity = currentWorld.get<LoungeableEntity>(anchorState->entityId)) {
      for (auto move : m_pendingMoves) {
        if (move == MoveControlType::Up)
          {
            if (auto result = loungeableEntity->loungeControl(anchorState->positionIndex, LoungeControl::Up); !result)
              return std::unexpected(std::move(result.error()));
          }
        else if (move == MoveControlType::Down)
          {
            if (auto result = loungeableEntity->loungeControl(anchorState->positionIndex, LoungeControl::Down); !result)
              return std::unexpected(std::move(result.error()));
          }
        else if (move == MoveControlType::Left)
          {
            if (auto result = loungeableEntity->loungeControl(anchorState->positionIndex, LoungeControl::Left); !result)
              return std::unexpected(std::move(result.error()));
          }
        else if (move == MoveControlType::Right)
          {
            if (auto result = loungeableEntity->loungeControl(anchorState->positionIndex, LoungeControl::Right); !result)
              return std::unexpected(std::move(result.error()));
          }
        else if (move == MoveControlType::Jump)
          {
            if (auto result = loungeableEntity->loungeControl(anchorState->positionIndex, LoungeControl::Jump); !result)
              return std::unexpected(std::move(result.error()));
          }
      }
      if (m_tools->firingPrimary())
        {
          if (auto result = loungeableEntity->loungeControl(anchorState->positionIndex, LoungeControl::PrimaryFire); !result)
            return std::unexpected(std::move(result.error()));
        }
      if (m_tools->firingAlt())
        {
          if (auto result = loungeableEntity->loungeControl(anchorState->positionIndex, LoungeControl::AltFire); !result)
            return std::unexpected(std::move(result.error()));
        }
      if (m_shifting)
        {
          if (auto result = loungeableEntity->loungeControl(anchorState->positionIndex, LoungeControl::Walk); !result)
            return std::unexpected(std::move(result.error()));
        }
      {
        if (auto result = loungeableEntity->loungeAim(anchorState->positionIndex, m_aimPosition); !result)
          return std::unexpected(std::move(result.error()));
      }
    }
    move = false;
  }

  m_techController->setShouldRun(run);

  if (move) {
    for (auto move : m_pendingMoves) {
      switch (move) {
        case MoveControlType::Right:
          m_techController->moveRight();
          break;
        case MoveControlType::Left:
          m_techController->moveLeft();
          break;
        case MoveControlType::Up:
          m_techController->moveUp();
          break;
        case MoveControlType::Down:
          m_techController->moveDown();
          break;
        case MoveControlType::Jump:
          m_techController->jump();
          break;
      }
    }
  }

  if (m_state == State::Lounge && !m_pendingMoves.empty() && move)
    {
      if (auto result = stopLounging(); !result)
        return std::unexpected(std::move(result.error()));
    }
  return {};
}

std::expected<void, StarException> Player::controlMovementState(std::optional<String> const& state) {
  if (!state) {
    m_controlMovementState.reset();
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
  else
    return std::unexpected(PlayerException("Movement state must be idle, walk, run, jump, fall, or nil"));
  if (inWorld() && isMaster())
    m_controlMovementState = parsed;
  return {};
}

void Player::updateMovementAnimationState() {
  m_movementAnimationState = isMaster() ? m_controlMovementState : std::nullopt;
  if (isDead() || isTeleporting() || m_state == State::Lounge
      || m_movementController->entityAnchor() || m_techController->parentState())
    m_movementAnimationState.reset();
}

std::expected<void, StarException> Player::processStateChanges(float dt) {
  auto currentWorldResult = world();
  if (!currentWorldResult)
    return std::unexpected(std::move(currentWorldResult.error()));
  auto& currentWorld = currentWorldResult->get();
  if (isMaster()) {

    // Set the current player state based on what movement controller tells us
    // we're doing and do some state transition logic
    State oldState = m_state;

    if (m_movementController->zeroG()) {
      if (m_movementController->flying())
        m_state = State::Swim;
      else if (m_state != State::Lounge)
        m_state = State::SwimIdle;
    } else if (m_movementController->groundMovement()) {
      if (m_movementController->running()) {
        m_state = State::Run;
      } else if (m_movementController->walking()) {
        m_state = State::Walk;
      } else if (m_movementController->crouching()) {
        m_state = State::Crouch;
      } else {
        if (m_state != State::Lounge)
          m_state = State::Idle;
      }
    } else if (m_movementController->liquidMovement()) {
      if (m_movementController->jumping()) {
        m_state = State::Swim;
      } else {
        if (m_state != State::Lounge)
          m_state = State::SwimIdle;
      }
    } else {
      if (m_movementController->jumping()) {
        m_state = State::Jump;
      } else {
        if (m_movementController->falling()) {
          m_state = State::Fall;
        }
        if (m_movementController->velocity()[1] > 0) {
          if (m_state != State::Lounge)
            m_state = State::Jump;
        }
      }
    }

    if (m_moveVector.x() != 0.0f && (m_state == State::Run))
        m_state = std::abs(m_moveVector.x()) > 0.5f ? State::Run : State::Walk;

    if (m_state == State::Jump && (oldState == State::Idle || oldState == State::Run || oldState == State::Walk || oldState == State::Crouch))
      {
        if (auto result = m_effectsAnimator->burstParticleEmitter("jump"); !result)
          return std::unexpected(std::move(result.error()));
      }

    if (!m_movementController->isNullColliding()) {
      if (oldState == State::Fall && oldState != m_state && m_state != State::Swim && m_state != State::SwimIdle
          && m_state != State::Jump) {
        {
          if (auto result = m_effectsAnimator->burstParticleEmitter("landing"); !result)
            return std::unexpected(std::move(result.error()));
        }
        m_landedNetState.trigger();
        m_landingNoisePending = true;
      }
    }
  }
  if (currentWorld.isClient()) {
    {
      if (auto result = humanoid()->animate(dt, std::ref(m_humanoidDynamicTarget)); !result)
        return std::unexpected(std::move(result.error()));
    }
    m_humanoidDynamicTarget.updatePosition(position() + (m_techController->parentOffset()));
  } else {
    {
      if (auto result = humanoid()->animate(dt, {}); !result)
        return std::unexpected(std::move(result.error()));
    }
  }
  if (auto result = m_scriptedAnimator.update(); !result)
    return std::unexpected(std::move(result.error()));

  if (auto techState = m_techController->parentState()) {
    if (techState == TechController::ParentState::Stand) {
      {
        if (auto result = humanoid()->setState(Humanoid::Idle); !result)
          return std::unexpected(std::move(result.error()));
      }
    } else if (techState == TechController::ParentState::Fly) {
      {
        if (auto result = humanoid()->setState(Humanoid::Jump); !result)
          return std::unexpected(std::move(result.error()));
      }
    } else if (techState == TechController::ParentState::Fall) {
      {
        if (auto result = humanoid()->setState(Humanoid::Fall); !result)
          return std::unexpected(std::move(result.error()));
      }
    } else if (techState == TechController::ParentState::Sit) {
      {
        if (auto result = humanoid()->setState(Humanoid::Sit); !result)
          return std::unexpected(std::move(result.error()));
      }
    } else if (techState == TechController::ParentState::Lay) {
      {
        if (auto result = humanoid()->setState(Humanoid::Lay); !result)
          return std::unexpected(std::move(result.error()));
      }
    } else if (techState == TechController::ParentState::Duck) {
      {
        if (auto result = humanoid()->setState(Humanoid::Duck); !result)
          return std::unexpected(std::move(result.error()));
      }
    } else if (techState == TechController::ParentState::Walk) {
      {
        if (auto result = humanoid()->setState(Humanoid::Walk); !result)
          return std::unexpected(std::move(result.error()));
      }
    } else if (techState == TechController::ParentState::Run) {
      {
        if (auto result = humanoid()->setState(Humanoid::Run); !result)
          return std::unexpected(std::move(result.error()));
      }
    } else if (techState == TechController::ParentState::Swim) {
      {
        if (auto result = humanoid()->setState(Humanoid::Swim); !result)
          return std::unexpected(std::move(result.error()));
      }
    } else if (techState == TechController::ParentState::SwimIdle) {
      {
        if (auto result = humanoid()->setState(Humanoid::SwimIdle); !result)
          return std::unexpected(std::move(result.error()));
      }
    }
  } else if (m_movementAnimationState) {
    if (auto result = humanoid()->setState(*m_movementAnimationState); !result)
      return std::unexpected(std::move(result.error()));
  } else {
    auto loungeAnchor = as<LoungeAnchor>(m_movementController->entityAnchor());
    if (m_state == State::Idle) {
      {
        if (auto result = humanoid()->setState(Humanoid::Idle); !result)
          return std::unexpected(std::move(result.error()));
      }
    } else if (m_state == State::Walk) {
      {
        if (auto result = humanoid()->setState(Humanoid::Walk); !result)
          return std::unexpected(std::move(result.error()));
      }
    } else if (m_state == State::Run) {
      {
        if (auto result = humanoid()->setState(Humanoid::Run); !result)
          return std::unexpected(std::move(result.error()));
      }
    } else if (m_state == State::Jump) {
      {
        if (auto result = humanoid()->setState(Humanoid::Jump); !result)
          return std::unexpected(std::move(result.error()));
      }
    } else if (m_state == State::Fall) {
      {
        if (auto result = humanoid()->setState(Humanoid::Fall); !result)
          return std::unexpected(std::move(result.error()));
      }
    } else if (m_state == State::Swim) {
      {
        if (auto result = humanoid()->setState(Humanoid::Swim); !result)
          return std::unexpected(std::move(result.error()));
      }
    } else if (m_state == State::SwimIdle) {
      {
        if (auto result = humanoid()->setState(Humanoid::SwimIdle); !result)
          return std::unexpected(std::move(result.error()));
      }
    } else if (m_state == State::Crouch) {
      {
        if (auto result = humanoid()->setState(Humanoid::Duck); !result)
          return std::unexpected(std::move(result.error()));
      }
    } else if (m_state == State::Lounge && loungeAnchor && loungeAnchor->orientation == LoungeOrientation::Sit) {
      {
        if (auto result = humanoid()->setState(Humanoid::Sit); !result)
          return std::unexpected(std::move(result.error()));
      }
    } else if (m_state == State::Lounge && loungeAnchor && loungeAnchor->orientation == LoungeOrientation::Lay) {
      {
        if (auto result = humanoid()->setState(Humanoid::Lay); !result)
          return std::unexpected(std::move(result.error()));
      }
    } else if (m_state == State::Lounge && loungeAnchor && loungeAnchor->orientation == LoungeOrientation::Stand) {
      {
        if (auto result = humanoid()->setState(Humanoid::Idle); !result)
          return std::unexpected(std::move(result.error()));
      }
    }
  }

  {

    if (auto result = humanoid()->setEmoteState(m_emoteState); !result)

      return std::unexpected(std::move(result.error()));

  }
  return {};
}

std::expected<String, StarException> Player::getFootstepSound(Vec2I const& sensor) const {
  auto databaseResult = root().materialDatabase();
  if (!databaseResult)
    return std::unexpected(std::move(databaseResult.error()));
  auto database = std::move(*databaseResult);
  auto currentWorldResult = world();
  if (!currentWorldResult)
    return std::unexpected(std::move(currentWorldResult.error()));
  auto& currentWorld = currentWorldResult->get();
  String fallback = database->defaultFootstepSound();
  List<Vec2I> scanOrder{{0, 0}, {0, -1}, {-1, 0}, {1, 0}, {-1, -1}, {1, -1}};
  for (auto const& subSensor : scanOrder) {
    auto soundResult = database->footstepSound(currentWorld.material(sensor + subSensor, TileLayer::Foreground), currentWorld.mod(sensor + subSensor, TileLayer::Foreground));
    if (!soundResult)
      return std::unexpected(std::move(soundResult.error()));
    auto sound = std::move(*soundResult);
    if (!sound.empty() && sound != fallback)
      return sound;
  }
  return fallback;
}

std::expected<bool, StarException> Player::inInteractionRange() const {
  return inInteractionRange(centerOfTile(m_aimPosition));
}

std::expected<bool, StarException> Player::inInteractionRange(Vec2F aimPos) const {
  if (isAdmin())
    return true;
  auto currentWorldResult = world();
  if (!currentWorldResult)
    return std::unexpected(std::move(currentWorldResult.error()));
  auto& currentWorld = currentWorldResult->get();
  return currentWorld.geometry().diff(aimPos, position()).magnitude() < m_interactRadius;
}

std::expected<bool, StarException> Player::inToolRange() const {
  return inToolRange(centerOfTile(m_aimPosition));
}

std::expected<bool, StarException> Player::inToolRange(Vec2F const& aimPos) const {
  if (isAdmin())
    return true;
  auto currentWorldResult = world();
  if (!currentWorldResult)
    return std::unexpected(std::move(currentWorldResult.error()));
  auto& currentWorld = currentWorldResult->get();
  auto radiusResult = toolRadius();
  if (!radiusResult)
    return std::unexpected(std::move(radiusResult.error()));
  auto radius = std::move(*radiusResult);
  return currentWorld.geometry().diff(aimPos, position()).magnitude() < radius;
}

std::expected<void, StarException> Player::getNetStates(bool initial) {
  auto elements = m_netHumanoid.netElements();
  auto elementsLastResult = elements.last();
  if (!elementsLastResult)
    return std::unexpected(std::move(elementsLastResult.error()));
  auto elementsLast = std::move(elementsLastResult->get());
  m_currentNetHumanoid = std::move(elementsLast);
  m_state = (State)m_stateNetState.get();
  m_shifting = m_shiftingNetState.get();
  m_aimPosition[0] = m_xAimPositionNetState.get();
  m_aimPosition[1] = m_yAimPositionNetState.get();
  if (m_identityNetState.pullUpdated() && !initial) {
    auto newIdentity = m_identityNetState.get();
    if (m_identity.species == newIdentity.species && m_identity.imagePath == newIdentity.imagePath) {
      if (auto result = humanoid()->setIdentity(newIdentity); !result)
        return std::unexpected(std::move(result.error()));
    }
    m_identity = std::move(newIdentity);
  }
  if (m_refreshedHumanoidParameters.pullOccurred() && !initial) {
    if (auto result = refreshHumanoidParameters(); !result)
      return std::unexpected(std::move(result.error()));
  }
  setTeam(m_teamNetState.get());
  if (m_landedNetState.pullOccurred() && !initial)
    m_landingNoisePending = true;
  if (m_newChatMessageNetState.pullOccurred() && !initial) {
    m_chatMessage = m_chatMessageNetState.get();
    m_chatMessageUpdated = true;
    m_pendingChatActions.append(SayChatAction{entityId(), m_chatMessage, m_movementController->position()});
  }
  if (!HumanoidEmoteNames)
    return std::unexpected(HumanoidEmoteNames.error());
  auto emoteResult = HumanoidEmoteNames->getLeft(m_emoteNetState.get());
  if (!emoteResult)
    return std::unexpected(std::move(emoteResult.error()));
  auto const& emote = emoteResult->get();
  m_emoteState = emote;
  m_dance = m_humanoidDanceNetState.get();
  return getNetArmorSecrets();
}

std::expected<void, StarException> Player::setNetStates() {
  // The legacy player-state field controls remote presentation, not the local
  // movement controller or native movement effects. Keep its type and slot.
  // Teleport/lounge/death can start after update (for example in postUpdate).
  // They must not be hidden by a presentation request already resolved above.
  if (isDead() || isTeleporting() || m_state == State::Lounge
      || m_movementController->entityAnchor() || m_techController->parentState()) {
    m_movementAnimationState.reset();
    m_movementController->setNetworkMovingDirection({});
  }
  State state = m_state;
  if (m_movementAnimationState) {
    switch (*m_movementAnimationState) {
      case Humanoid::Idle: state = State::Idle; break;
      case Humanoid::Walk: state = State::Walk; break;
      case Humanoid::Run: state = State::Run; break;
      case Humanoid::Jump: state = State::Jump; break;
      case Humanoid::Fall: state = State::Fall; break;
      default: break;
    }
  }
  m_stateNetState.set((unsigned)state);
  m_shiftingNetState.set(m_shifting);
  m_xAimPositionNetState.set(m_aimPosition[0]);
  m_yAimPositionNetState.set(m_aimPosition[1]);
  if (m_identityUpdated) {
    m_identityNetState.push(m_identity);
    m_identityUpdated = false;
  }
  m_teamNetState.set(getTeam());
  if (m_chatMessageChanged) {
    m_chatMessageChanged = false;
    m_chatMessageNetState.push(m_chatMessage);
    m_newChatMessageNetState.trigger();
  }
  if (!HumanoidEmoteNames)
    return std::unexpected(HumanoidEmoteNames.error());
  auto emoteResult = HumanoidEmoteNames->getRight(m_emoteState);
  if (!emoteResult)
    return std::unexpected(std::move(emoteResult.error()));
  auto const& emote = emoteResult->get();
  m_emoteNetState.set(emote);
  m_humanoidDanceNetState.set(m_dance);
  return {};
}

std::expected<void, StarException> Player::setNetArmorSecret(EquipmentSlot slot, std::shared_ptr<ArmorItem> const& armor, bool visible) {
  if (!EquipmentSlotNames)
    return std::unexpected(EquipmentSlotNames.error());
  auto slotNameResult = EquipmentSlotNames->getRight(slot);
  if (!slotNameResult)
    return std::unexpected(std::move(slotNameResult.error()));
  auto const& slotName = slotNameResult->get();
  ItemDescriptor descriptor = visible ? itemSafeDescriptor(armor) : ItemDescriptor();
  auto versioningResult = root().versioningDatabase();
  if (!versioningResult)
    return std::unexpected(std::move(versioningResult.error()));
  auto versioning = std::move(*versioningResult);
  auto storeResult = descriptor.diskStore(*versioning);
  if (!storeResult)
    return std::unexpected(std::move(storeResult.error()));
  auto store = std::move(*storeResult);
  if (auto result = setSecretProperty(strf("armorWearer.{}.data", slotName), store); !result)
    return std::unexpected(std::move(result.error()));
  if (m_armorSecretNetVersions.empty()) {
    if (auto result = setSecretProperty("armorWearer.replicating", true); !result)
      return std::unexpected(std::move(result.error()));
  }
  return setSecretProperty(strf("armorWearer.{}.version", slotName), ++m_armorSecretNetVersions[slot]);
}

std::expected<void, StarException> Player::setNetArmorSecrets(bool includeEmpty) {
  if (m_clientContext && m_clientContext->netCompatibilityRules().version() < 9) {
    for (std::uint8_t i = 0; i != 12; ++i) {
      auto slot = EquipmentSlot((std::uint8_t)EquipmentSlot::Cosmetic1 + i);
      auto storedResult = m_inventory->itemsAt(slot);
      if (!storedResult)
        return std::unexpected(std::move(storedResult.error()));
      auto stored = std::move(*storedResult);
      auto item = as<ArmorItem>(stored);
      bool visible = m_inventory->equipmentVisibility(slot);
      if ((item && visible) || includeEmpty) {
        if (auto result = setNetArmorSecret(slot, item, visible); !result)
          return std::unexpected(std::move(result.error()));
      }
    }
  }
  return {};
}

std::expected<void, StarException> Player::getNetArmorSecrets() {
  if (isSlave() && getSecretPropertyRef("armorWearer.replicating")) {
    auto databaseResult = root().itemDatabase();
    if (!databaseResult)
      return std::unexpected(std::move(databaseResult.error()));
    auto database = std::move(*databaseResult);
    if (!EquipmentSlotNames)
      return std::unexpected(EquipmentSlotNames.error());
    for (std::uint8_t i = 0; i != 12; ++i) {
      auto slot = EquipmentSlot((std::uint8_t)EquipmentSlot::Cosmetic1 + i);
      auto slotNameResult = EquipmentSlotNames->getRight(slot);
      if (!slotNameResult)
        return std::unexpected(std::move(slotNameResult.error()));
      auto const& slotName = slotNameResult->get();
      auto& currentVersion = m_armorSecretNetVersions[slot];
      std::uint64_t newVersion = 0;
      auto versionResult = getSecretProperty(strf("armorWearer.{}.version", slotName), 0);
      if (!versionResult)
        return std::unexpected(std::move(versionResult.error()));
      auto version = std::move(*versionResult);
      if (version.isType(Json::Type::Int)) {
        auto valueResult = version.toUInt();
        if (!valueResult)
          return std::unexpected(std::move(valueResult.error()));
        auto value = std::move(*valueResult);
        newVersion = value;
      }
      if (newVersion > currentVersion) {
        currentVersion = newVersion;
        std::shared_ptr<ArmorItem> item;
        auto storeResult = getSecretProperty(strf("armorWearer.{}.data", slotName));
        if (!storeResult)
          return std::unexpected(std::move(storeResult.error()));
        auto store = std::move(*storeResult);
        if (auto result = database->diskLoad(store, item); !result)
          return std::unexpected(std::move(result.error()));
        if (auto result = m_inventory->setItem(slot, item); !result)
          return std::unexpected(std::move(result.error()));
        m_armor->setCosmeticItem(i, item);
      }
    }
  }
  return {};
}

void Player::setAdmin(bool isAdmin) {
  m_isAdmin = isAdmin;
}

bool Player::isAdmin() const {
  return m_isAdmin;
}

std::expected<void, StarException> Player::setFavoriteColor(Color color) {
  m_identity.color = color.toRgba();
  return updateIdentity();
}

Color Player::favoriteColor() const {
  return Color::rgba(m_identity.color);
}

bool Player::isTeleporting() const {
  return (m_state == State::TeleportIn) || (m_state == State::TeleportOut);
}

bool Player::isTeleportingOut() const {
  return inWorld() && (m_state == State::TeleportOut) && m_teleportTimer >= 0.0f;
}

std::expected<bool, StarException> Player::canDeploy() {
  return m_deployment->canDeploy();
}

void Player::deployAbort(String const& animationType) {
  m_teleportAnimationType = animationType;
  m_deployment->setDeploying(false);
}

bool Player::isDeploying() const {
  return m_deployment->isDeploying();
}

bool Player::isDeployed() const {
  return m_deployment->isDeployed();
}

std::expected<void, StarException> Player::setBusyState(PlayerBusyState busyState) {
  if (!PlayerBusyStateNames)
    return std::unexpected(PlayerBusyStateNames.error());
  auto nameResult = PlayerBusyStateNames->getRight(busyState);
  if (!nameResult)
    return std::unexpected(std::move(nameResult.error()));
  auto const& name = nameResult->get();
  if (auto result = m_effectsAnimator->setState("busy", name); !result)
    return std::unexpected(std::move(result.error()));
  return {};
}

std::expected<void, StarException> Player::teleportOut(String const& animationType, bool deploy) {
  m_state = State::TeleportOut;
  m_teleportAnimationType = animationType;
  {
    if (auto result = m_effectsAnimator->setState("teleport", m_teleportAnimationType + "Out"); !result)
      return std::unexpected(std::move(result.error()));
  }
  m_deployment->setDeploying(deploy);
  {
    if (auto result = m_deployment->teleportOut(); !result)
      return std::unexpected(std::move(result.error()));
  }
  m_teleportTimer = deploy ? m_config->deployOutTime : m_config->teleportOutTime;
  return {};
}

std::expected<void, StarException> Player::teleportIn() {
  m_state = State::TeleportIn;
  if (auto result = m_effectsAnimator->setState("teleport", m_teleportAnimationType + "In"); !result)
    return std::unexpected(std::move(result.error()));
  m_teleportTimer = m_deployment->isDeployed() ? m_config->deployInTime : m_config->teleportInTime;
  auto assetsResult = root().assets();
  if (!assetsResult)
    return std::unexpected(std::move(assetsResult.error()));
  auto assets = std::move(*assetsResult);
  auto jsonResult = assets->json("/player.config:teleportInStatusEffects");
  if (!jsonResult)
    return std::unexpected(std::move(jsonResult.error()));
  auto json = std::move(*jsonResult);
  auto arrayResult = json.toArray();
  if (!arrayResult)
    return std::unexpected(std::move(arrayResult.error()));
  auto array = std::move(*arrayResult);
  List<EphemeralStatusEffect> effects;
  for (auto const& entry : array) {
    auto effectResult = jsonToEphemeralStatusEffect(entry);
    if (!effectResult)
      return std::unexpected(std::move(effectResult.error()));
    auto effect = std::move(*effectResult);
    effects.append(std::move(effect));
  }
  return m_statusController->addEphemeralEffects(effects);
}

std::expected<void, StarException> Player::teleportAbort() {
  m_state = State::TeleportIn;
  {
    if (auto result = m_effectsAnimator->setState("teleport", "abort"); !result)
      return std::unexpected(std::move(result.error()));
  }
  m_deployment->setDeploying(m_deployment->isDeployed());
  m_teleportTimer = m_config->teleportInTime;
  return {};
}

std::expected<void, StarException> Player::moveTo(Vec2F const& footPosition) {
  auto offsetResult = feetOffset();
  if (!offsetResult)
    return std::unexpected(std::move(offsetResult.error()));
  auto offset = std::move(*offsetResult);
  m_movementController->setPosition(footPosition - offset);
  m_movementController->setVelocity(Vec2F());
  return {};
}

std::shared_ptr<Item> Player::primaryHandItem() const {
  return m_tools->primaryHandItem();
}

std::shared_ptr<Item> Player::altHandItem() const {
  return m_tools->altHandItem();
}

std::expected<Uuid, StarException> Player::uuid() const {
  auto id = uniqueId();
  if (!id)
    return std::unexpected(PlayerException("Player has no unique id"));
  return Uuid::create(*id);
}

PlayerMode Player::modeType() const {
  return m_modeType;
}

std::expected<void, StarException> Player::setModeType(PlayerMode mode) {
  m_modeType = mode;
  auto assetsResult = root().assets();
  if (!assetsResult)
    return std::unexpected(std::move(assetsResult.error()));
  auto assets = std::move(*assetsResult);
  auto configResult = assets->json("/playermodes.config");
  if (!configResult)
    return std::unexpected(std::move(configResult.error()));
  auto config = std::move(*configResult);
  if (!PlayerModeNames)
    return std::unexpected(PlayerModeNames.error());
  auto nameResult = PlayerModeNames->getRight(mode);
  if (!nameResult)
    return std::unexpected(std::move(nameResult.error()));
  auto const& name = nameResult->get();
  auto modeJsonResult = config.get(name);
  if (!modeJsonResult)
    return std::unexpected(std::move(modeJsonResult.error()));
  auto modeJson = std::move(*modeJsonResult);
  auto modeConfigResult = PlayerModeConfig::create(modeJson);
  if (!modeConfigResult)
    return std::unexpected(std::move(modeConfigResult.error()));
  auto modeConfig = std::move(*modeConfigResult);
  m_modeConfig = std::move(modeConfig);
  return {};
}

PlayerModeConfig Player::modeConfig() const {
  return m_modeConfig;
}

ShipUpgrades Player::shipUpgrades() {
  return m_shipUpgrades;
}

void Player::setShipUpgrades(ShipUpgrades shipUpgrades) {
  m_shipUpgrades = std::move(shipUpgrades);
}

std::expected<void, StarException> Player::applyShipUpgrades(Json const& upgrades) {
  auto playerUuidResult = uuid();
  if (!playerUuidResult)
    return std::unexpected(std::move(playerUuidResult.error()));
  auto playerUuid = std::move(*playerUuidResult);
  if (m_clientContext->playerUuid() == playerUuid) {
    if (auto result = m_clientContext->rpcInterface()->invokeRemote("ship.applyShipUpgrades", upgrades); !result)
      return std::unexpected(std::move(result.error()));
  } else {
    if (auto result = m_shipUpgrades.apply(upgrades); !result)
      return std::unexpected(std::move(result.error()));
  }
  return {};
}

void Player::setShipSpecies(String species) {
  m_shipSpecies = std::move(species);
}

String Player::shipSpecies() const {
  return m_shipSpecies;
}

std::expected<String, StarException> Player::name() const {
  return m_identity.name;
}

std::expected<void, StarException> Player::setName(String const& name) {
  m_identity.name = name;
  return updateIdentity();
}

std::optional<String> Player::statusText() const {
  return {};
}

bool Player::displayNametag() const {
  return true;
}

std::expected<Vec3B, StarException> Player::nametagColor() const {
  auto assetsResult = root().assets();
  if (!assetsResult)
    return std::unexpected(std::move(assetsResult.error()));
  auto assets = std::move(*assetsResult);
  auto colorResult = assets->json("/player.config:nametagColor");
  if (!colorResult)
    return std::unexpected(std::move(colorResult.error()));
  auto color = std::move(*colorResult);
  return jsonToVec3B(color);
}

std::expected<Vec2F, StarException> Player::nametagOrigin() const {
  return mouthPosition(false);
}

std::expected<String, StarException> Player::nametag() const {
  auto valueResult = getSecretProperty("nametag");
  if (!valueResult)
    return std::unexpected(std::move(valueResult.error()));
  auto value = std::move(*valueResult);
  if (value.isType(Json::Type::String))
    return value.toString();
  return m_identity.name;
}

std::expected<void, StarException> Player::setNametag(std::optional<String> nametag) {
  return setSecretProperty("nametag", nametag ? Json(*nametag) : Json());
}

std::expected<void, StarException> Player::updateIdentity() {
  m_identityUpdated = true;
  auto const& oldIdentity = humanoid()->identity();
  if (m_identity.species != oldIdentity.species || m_identity.imagePath != oldIdentity.imagePath)
    return refreshHumanoidParameters();
  return humanoid()->setIdentity(m_identity);
}

std::expected<void, StarException> Player::setHumanoidParameter(String key, std::optional<Json> value) {
  if (value)
    m_humanoidParameters.set(key, *value);
  else
    m_humanoidParameters.erase(key);
  return m_currentNetHumanoid->setHumanoidParameters(m_humanoidParameters);
}

std::optional<Json> Player::getHumanoidParameter(String key) {
  return m_humanoidParameters.maybe(key);
}

std::expected<void, StarException> Player::setHumanoidParameters(JsonObject parameters) {
  m_humanoidParameters = std::move(parameters);
  return m_currentNetHumanoid->setHumanoidParameters(m_humanoidParameters);
}

JsonObject Player::getHumanoidParameters() {
  return m_humanoidParameters;
}

std::expected<void, StarException> Player::setBodyDirectives(String const& directives)
{ m_identity.bodyDirectives = directives; return updateIdentity(); }

std::expected<void, StarException> Player::setEmoteDirectives(String const& directives)
{ m_identity.emoteDirectives = directives; return updateIdentity(); }

std::expected<void, StarException> Player::setHairGroup(String const& group)
{ m_identity.hairGroup = group; return updateIdentity(); }

std::expected<void, StarException> Player::setHairType(String const& type)
{ m_identity.hairType = type; return updateIdentity(); }

std::expected<void, StarException> Player::setHairDirectives(String const& directives)
{ m_identity.hairDirectives = directives; return updateIdentity(); }

std::expected<void, StarException> Player::setFacialHairGroup(String const& group)
{ m_identity.facialHairGroup = group; return updateIdentity(); }

std::expected<void, StarException> Player::setFacialHairType(String const& type)
{ m_identity.facialHairType = type; return updateIdentity(); }

std::expected<void, StarException> Player::setFacialHairDirectives(String const& directives)
{ m_identity.facialHairDirectives = directives; return updateIdentity(); }

std::expected<void, StarException> Player::setFacialMaskGroup(String const& group)
{ m_identity.facialMaskGroup = group; return updateIdentity(); }

std::expected<void, StarException> Player::setFacialMaskType(String const& type)
{ m_identity.facialMaskType = type; return updateIdentity(); }

std::expected<void, StarException> Player::setFacialMaskDirectives(String const& directives)
{ m_identity.facialMaskDirectives = directives; return updateIdentity(); }

std::expected<void, StarException> Player::setHair(String const& group, String const& type, String const& directives) {
  m_identity.hairGroup = group;
  m_identity.hairType = type;
  m_identity.hairDirectives = directives;
  return updateIdentity();
}

std::expected<void, StarException> Player::setFacialHair(String const& group, String const& type, String const& directives) {
  m_identity.facialHairGroup = group;
  m_identity.facialHairType = type;
  m_identity.facialHairDirectives = directives;
  return updateIdentity();
}

std::expected<void, StarException> Player::setFacialMask(String const& group, String const& type, String const& directives) {
  m_identity.facialMaskGroup = group;
  m_identity.facialMaskType = type;
  m_identity.facialMaskDirectives = directives;
  return updateIdentity();
}

std::expected<void, StarException> Player::setSpecies(String const& species) {
  auto databaseResult = root().speciesDatabase();
  if (!databaseResult)
    return std::unexpected(std::move(databaseResult.error()));
  auto database = std::move(*databaseResult);
  if (auto result = database->species(species); !result)
    return std::unexpected(std::move(result.error()));
  m_identity.species = species;
  return updateIdentity();
}

Gender Player::gender() const {
  return m_identity.gender;
}

std::expected<void, StarException> Player::setGender(Gender const& gender) {
  m_identity.gender = gender;
  return updateIdentity();
}

String Player::species() const {
  return m_identity.species;
}

std::expected<void, StarException> Player::setPersonality(Personality const& personality) {
  m_identity.personality = personality;
  return updateIdentity();
}

std::expected<void, StarException> Player::setImagePath(std::optional<String> const& imagePath) {
  m_identity.imagePath = imagePath;
  return updateIdentity();
}

std::shared_ptr<Humanoid> Player::humanoid() {
  return m_currentNetHumanoid->humanoid();
}
std::shared_ptr<Humanoid> Player::humanoid() const {
  return m_currentNetHumanoid->humanoid();
}

HumanoidIdentity const& Player::identity() const {
  return m_identity;
}

std::expected<void, StarException> Player::setIdentity(HumanoidIdentity identity) {
  m_identity = std::move(identity);
  return updateIdentity();
}

List<String> Player::pullQueuedMessages() {
  return take(m_queuedMessages);
}

List<std::shared_ptr<Item>> Player::pullQueuedItemDrops() {
  return take(m_queuedItemPickups);
}

std::expected<void, StarException> Player::queueUIMessage(String const& message) {
  if (!isSlave())
    m_queuedMessages.append(message);
  return {};
}

void Player::queueItemPickupMessage(std::shared_ptr<Item> const& item) {
  if (!isSlave())
    m_queuedItemPickups.append(item);
}

std::expected<void, StarException> Player::addChatMessage(String const& message, Json const& config) {
  if constexpr (Star::DebugEnabled) {
    Star::starAssertFailed((!isSlave()));
  }
  m_chatMessage = message;
  m_chatMessageUpdated = true;
  m_chatMessageChanged = true;
  auto mouthResult = mouthPosition();
  if (!mouthResult)
    return std::unexpected(std::move(mouthResult.error()));
  auto mouth = std::move(*mouthResult);
  m_pendingChatActions.append(SayChatAction{entityId(), message, mouth, config});
  return {};
}

void Player::addEmote(HumanoidEmote const& emote, std::optional<float> emoteCooldown) {
  if constexpr (Star::DebugEnabled) {
    Star::starAssertFailed((!isSlave()));
  }
  m_emoteState = emote;
  m_emoteCooldownTimer = GameTimer(emoteCooldown.value_or(m_emoteCooldown));
}
std::expected<void, StarException> Player::setDance(std::optional<String> const& danceName) {
  if constexpr (Star::DebugEnabled) {
    Star::starAssertFailed((!isSlave()));
  }
  assert(!isSlave());
  m_dance = danceName;
  if (danceName) {
    auto databaseResult = root().danceDatabase();
    if (!databaseResult)
      return std::unexpected(std::move(databaseResult.error()));
    auto database = std::move(*databaseResult);
    auto danceResult = database->getDance(*danceName);
    if (!danceResult)
      return std::unexpected(std::move(danceResult.error()));
    auto dance = std::move(*danceResult);
    m_danceCooldownTimer = GameTimer(dance->duration);
  }
  return {};
}

std::pair<HumanoidEmote, float> Player::currentEmote() const {
  return std::make_pair(m_emoteState, m_emoteCooldownTimer.timer);
}

Player::State Player::currentState() const {
  return m_state;
}

List<ChatAction> Player::pullPendingChatActions() {
  return take(m_pendingChatActions);
}

std::expected<std::optional<String>, StarException> Player::inspectionLogName() const {
  auto identifier = uniqueId();
  if (identifier) {
    auto hash = xxHash3_128(std::span<char const>(identifier->utf8()));
    return String("Player #") + hexEncode(std::span(reinterpret_cast<char const*>(&hash), sizeof(hash)));
  }
  return identifier;
}

std::expected<std::optional<String>, StarException> Player::inspectionDescription(String const&) const {
  return m_description;
}

std::expected<float, StarException> Player::beamGunRadius() const {
  return m_tools->beamGunRadius();
}

std::expected<bool, StarException> Player::instrumentPlaying() {
  return m_songbook->instrumentPlaying();
}

std::expected<void, StarException> Player::instrumentEquipped(String const& instrumentKind) {
  if (canUseTool()) {
    auto mouthResult = mouthPosition();
    if (!mouthResult)
      return std::unexpected(std::move(mouthResult.error()));
    auto mouth = std::move(*mouthResult);
    m_songbook->keepAlive(instrumentKind, mouth);
  }
  return {};
}

std::expected<void, StarException> Player::interact(InteractAction const& action) {
  if constexpr (Star::DebugEnabled) {
    Star::starAssertFailed((!isSlave()));
  }
  m_pendingInteractActions.append(RpcPromise<InteractAction>::createFulfilled(action));
  return {};
}

std::expected<void, StarException> Player::addEffectEmitters(StringSet const& emitters) {
  if constexpr (Star::DebugEnabled) {
    Star::starAssertFailed((!isSlave()));
  }
  m_effectEmitter->addEffectSources("normal", emitters);
  return {};
}

std::expected<void, StarException> Player::requestEmote(String const& emote) {
  if (!HumanoidEmoteNames)
    return std::unexpected(HumanoidEmoteNames.error());
  auto stateResult = HumanoidEmoteNames->getLeft(emote);
  if (!stateResult)
    return std::unexpected(std::move(stateResult.error()));
  auto const& state = stateResult->get();
  if (state != HumanoidEmote::Idle && (m_emoteState == state || m_emoteState == HumanoidEmote::Idle || m_emoteState == HumanoidEmote::Blink))
    addEmote(state, {});
  return {};
}

ActorMovementController& Player::movementController() {
  return *m_movementController;
}

StatusController& Player::statusController() {
  return *m_statusController;
}

std::expected<List<PhysicsForceRegion>, StarException> Player::forceRegions() const {
  return m_tools->forceRegions();
}


std::shared_ptr<StatusController> Player::statusControllerPtr() {
  return m_statusController;
}

std::shared_ptr<ActorMovementController> Player::movementControllerPtr() {
  return m_movementController;
}

std::shared_ptr<PlayerConfig> Player::config() {
  return m_config;
}

std::shared_ptr<Songbook> Player::songbook() const {
  return m_songbook;
}

std::shared_ptr<QuestManager> Player::questManager() const {
  return m_questManager;
}

std::expected<Json, StarException> Player::diskStore() {
  JsonObject storage;
  for (auto& script : m_genericScriptContexts) {
    auto entryResult = script.second->getScriptStorage();
    if (!entryResult)
      return std::unexpected(std::move(entryResult.error()));
    auto entry = std::move(*entryResult);
    if (!entry.empty())
      storage[script.first] = std::move(entry);
  }
  auto id = uniqueId();
  if (!id)
    return std::unexpected(PlayerException("Player has no unique id"));
  if (!PlayerModeNames)
    return std::unexpected(PlayerModeNames.error());
  auto modeResult = PlayerModeNames->getRight(m_modeType);
  if (!modeResult)
    return std::unexpected(std::move(modeResult.error()));
  auto const& mode = modeResult->get();
  auto universeMapResult = m_universeMap->toJson();
  if (!universeMapResult)
    return std::unexpected(std::move(universeMapResult.error()));
  auto universeMap = std::move(*universeMapResult);
  auto techsResult = m_techs->toJson();
  if (!techsResult)
    return std::unexpected(std::move(techsResult.error()));
  auto techs = std::move(*techsResult);
  auto identityResult = m_identity.toJson();
  if (!identityResult)
    return std::unexpected(std::move(identityResult.error()));
  auto identity = std::move(*identityResult);
  auto teamResult = getTeam().toJson();
  if (!teamResult)
    return std::unexpected(std::move(teamResult.error()));
  auto team = std::move(*teamResult);
  auto inventoryResult = m_inventory->store();
  if (!inventoryResult)
    return std::unexpected(std::move(inventoryResult.error()));
  auto inventory = std::move(*inventoryResult);
  auto movementResult = m_movementController->storeState();
  if (!movementResult)
    return std::unexpected(std::move(movementResult.error()));
  auto movement = std::move(*movementResult);
  auto techControllerResult = m_techController->diskStore();
  if (!techControllerResult)
    return std::unexpected(std::move(techControllerResult.error()));
  auto techController = std::move(*techControllerResult);
  auto statusControllerResult = m_statusController->diskStore();
  if (!statusControllerResult)
    return std::unexpected(std::move(statusControllerResult.error()));
  auto statusController = std::move(*statusControllerResult);
  auto questsResult = m_questManager->diskStore();
  if (!questsResult)
    return std::unexpected(std::move(questsResult.error()));
  auto quests = std::move(*questsResult);
  auto companionsResult = m_companions->diskStore();
  if (!companionsResult)
    return std::unexpected(std::move(companionsResult.error()));
  auto companions = std::move(*companionsResult);
  auto deploymentResult = m_deployment->diskStore();
  if (!deploymentResult)
    return std::unexpected(std::move(deploymentResult.error()));
  auto deployment = std::move(*deploymentResult);
  return JsonObject{
    {"uuid", *id}, {"description", m_description}, {"modeType", mode},
    {"shipUpgrades", m_shipUpgrades.toJson()}, {"shipSpecies", !m_shipSpecies.empty() ? m_shipSpecies : m_identity.species},
    {"blueprints", m_blueprints->toJson()}, {"universeMap", std::move(universeMap)}, {"codexes", m_codexes->toJson()},
    {"techs", std::move(techs)}, {"identity", std::move(identity)}, {"team", std::move(team)},
    {"inventory", std::move(inventory)}, {"movementController", std::move(movement)},
    {"techController", std::move(techController)}, {"statusController", std::move(statusController)},
    {"log", m_log->toJson()}, {"aiState", m_aiState.toJson()}, {"quests", std::move(quests)},
    {"companions", std::move(companions)}, {"deployment", std::move(deployment)},
    {"genericProperties", m_genericProperties}, {"genericScriptStorage", std::move(storage)}, {"humanoidParameters", m_humanoidParameters}
  };
}

std::expected<ByteArray, StarException> Player::netStore(NetCompatibilityRules rules) {
  DataStreamBuffer ds;
  ds.setStreamCompatibilityVersion(rules);
  auto id = uniqueId();
  if (!id)
    return std::unexpected(PlayerException("Player has no unique id"));
  if (auto result = ds.write(*id); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = ds.write(m_description); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = ds.write(m_modeType); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = ds.write(m_identity); !result)
    return std::unexpected(std::move(result.error()));
  if (rules.version() >= 10) {
    if (auto result = ds.write(m_humanoidParameters); !result)
      return std::unexpected(std::move(result.error()));
  }
  return ds.takeData();
}

std::expected<void, StarException> Player::finalizeCreation() {
  m_blueprints = std::make_shared<PlayerBlueprints>();
  m_techs = std::make_shared<PlayerTech>(root());
  auto databaseResult = root().itemDatabase();
  if (!databaseResult)
    return std::unexpected(std::move(databaseResult.error()));
  auto database = std::move(*databaseResult);
  for (auto const& descriptor : m_config->defaultItems) {
    auto itemResult = database->item(descriptor);
    if (!itemResult)
      return std::unexpected(std::move(itemResult.error()));
    auto item = std::move(*itemResult);
    if (auto result = m_inventory->addItems(item); !result)
      return std::unexpected(std::move(result.error()));
  }
  auto speciesDatabaseResult = root().speciesDatabase();
  if (!speciesDatabaseResult)
    return std::unexpected(std::move(speciesDatabaseResult.error()));
  auto speciesDatabase = std::move(*speciesDatabaseResult);
  auto speciesDefinitionResult = speciesDatabase->species(m_identity.species);
  if (!speciesDefinitionResult)
    return std::unexpected(std::move(speciesDefinitionResult.error()));
  auto speciesDefinition = std::move(*speciesDefinitionResult);
  for (auto const& descriptor : speciesDefinition->defaultItems()) {
    auto itemResult = database->item(descriptor);
    if (!itemResult)
      return std::unexpected(std::move(itemResult.error()));
    auto item = std::move(*itemResult);
    if (auto result = m_inventory->addItems(item); !result)
      return std::unexpected(std::move(result.error()));
  }
  for (auto const& descriptor : m_config->defaultBlueprints)
    m_blueprints->add(descriptor);
  for (auto const& descriptor : speciesDefinition->defaultBlueprints())
    m_blueprints->add(descriptor);
  if (auto result = refreshEquipment(); !result)
    return std::unexpected(std::move(result.error()));
  m_state = State::Idle;
  m_emoteState = HumanoidEmote::Idle;
  if (auto result = synchronizeEquipmentStatusEffects(*m_armor, *m_tools, *m_statusController); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = m_statusController->resetAllResources(); !result)
    return std::unexpected(std::move(result.error()));
  m_effectEmitter->reset();
  m_description = strf("This {} seems to have nothing to say for {}self.", m_identity.gender == Gender::Male ? "guy" : "gal", m_identity.gender == Gender::Male ? "him" : "her");
  return {};
}

bool Player::invisible() const {
  return m_statusController->statPositive("invisible");
}

std::expected<void, StarException> Player::animatePortrait(float dt) {
  if (auto result = humanoid()->animate(dt, {}); !result)
    return std::unexpected(std::move(result.error()));
  if (m_emoteCooldownTimer.tick(dt))
    m_emoteState = HumanoidEmote::Idle;
  return humanoid()->setEmoteState(m_emoteState);
}

std::expected<bool, StarException> Player::isOutside() {
  if (!inWorld())
    return false;
  auto currentWorldResult = world();
  if (!currentWorldResult)
    return std::unexpected(std::move(currentWorldResult.error()));
  auto& currentWorld = currentWorldResult->get();
  auto undergroundResult = currentWorld.isUnderground(position());
  if (!undergroundResult)
    return std::unexpected(std::move(undergroundResult.error()));
  auto underground = std::move(*undergroundResult);
  if (underground)
    return false;
  auto mouthResult = mouthPosition();
  if (!mouthResult)
    return std::unexpected(std::move(mouthResult.error()));
  auto mouth = std::move(*mouthResult);
  auto occupiedResult = currentWorld.tileIsOccupied(Vec2I::floor(mouth), TileLayer::Background);
  if (!occupiedResult)
    return std::unexpected(std::move(occupiedResult.error()));
  auto occupied = std::move(*occupiedResult);
  return !occupied;
}

std::expected<void, StarException> Player::dropSelectedItems(std::function<std::expected<bool, StarException>(std::shared_ptr<Item>)> filter) {
  auto currentWorld = maybeWorld();
  if (!currentWorld)
    return {};
  return m_inventory->forEveryItem([&](InventorySlot const&, std::shared_ptr<Item>& item) -> std::expected<void, StarException> {
    if (!item)
      return {};
    if (filter) {
      auto selectedResult = filter(item);
      if (!selectedResult)
        return std::unexpected(std::move(selectedResult.error()));
      auto selected = std::move(*selectedResult);
      if (!selected)
        return {};
    }
    auto angleResult = root().random().randf(-std::numbers::pi, std::numbers::pi);
    if (!angleResult)
      return std::unexpected(std::move(angleResult.error()));
    auto angle = std::move(*angleResult);
    auto dropResult = ItemDrop::throwDrop(root(), take(item), position(), velocity(), Vec2F::withAngle(angle), true);
    if (!dropResult)
      return std::unexpected(std::move(dropResult.error()));
    auto drop = std::move(*dropResult);
    return currentWorld->get().addEntity(std::move(drop));
  });
}

std::expected<void, StarException> Player::dropEverything() {
  return dropSelectedItems({});
}

bool Player::isPermaDead() const {
  if (!isDead())
    return false;
  return modeConfig().permadeath;
}

bool Player::interruptRadioMessage() {
  if (m_interruptRadioMessage) {
    m_interruptRadioMessage = false;
    return true;
  }
  return false;
}

std::expected<std::optional<RadioMessage>, StarException> Player::pullPendingRadioMessage() {
  if (m_pendingRadioMessages.empty())
    return std::optional<RadioMessage>{};
  auto const& first = m_pendingRadioMessages[0];
  if (first.unique)
    m_log->addRadioMessage(first.messageId);
  auto messageResult = m_pendingRadioMessages.takeFirst();
  if (!messageResult)
    return std::unexpected(std::move(messageResult.error()));
  auto message = std::move(*messageResult);
  return std::optional<RadioMessage>{std::move(message)};
}

std::expected<void, StarException> Player::queueRadioMessage(Json const& messageConfig, float delay) {
  auto decoded = [&]() -> std::expected<std::optional<RadioMessage>, StarException> {
    auto databaseResult = root().radioMessageDatabase();
    if (!databaseResult)
      return std::unexpected(std::move(databaseResult.error()));
    auto database = std::move(*databaseResult);
    auto messageResult = database->createRadioMessage(messageConfig);
    if (!messageResult)
      return std::unexpected(std::move(messageResult.error()));
    auto message = std::move(*messageResult);
    while (message.speciesAiMessage.contains(shipSpecies()) || message.speciesMessage.contains(species()))
      message = message.speciesAiMessage.value(shipSpecies(), message.speciesMessage.value(species()));
    if (message.type == RadioMessageType::Tutorial) {
      auto configurationResult = root().configuration();
      if (!configurationResult)
        return std::unexpected(std::move(configurationResult.error()));
      auto configuration = std::move(*configurationResult);
      auto tutorialResult = configuration->get("tutorialMessages");
      if (!tutorialResult)
        return std::unexpected(std::move(tutorialResult.error()));
      auto tutorial = std::move(*tutorialResult);
      auto enabledResult = tutorial.toBool();
      if (!enabledResult)
        return std::unexpected(std::move(enabledResult.error()));
      auto enabled = std::move(*enabledResult);
      if (!enabled)
        return std::optional<RadioMessage>{};
    }
    if (!message.portraitImage.empty()) {
      auto firstResult = message.portraitImage[0];
      if (!firstResult)
        return std::unexpected(std::move(firstResult.error()));
      auto first = std::move(*firstResult);
      if (first != '/') {
        auto aiResult = root().aiDatabase();
        if (!aiResult)
          return std::unexpected(std::move(aiResult.error()));
        auto ai = std::move(*aiResult);
        auto portraitResult = ai->portraitImage(shipSpecies(), message.portraitImage);
        if (!portraitResult)
          return std::unexpected(std::move(portraitResult.error()));
        auto portrait = std::move(*portraitResult);
        message.portraitImage = std::move(portrait);
      }
    }
    return std::optional<RadioMessage>{std::move(message)};
  }();
  if (!decoded) {
    if (!decoded.error().is<RadioMessageDatabaseException>())
      return std::unexpected(std::move(decoded.error()));
    auto descriptionResult = messageConfig.repr();
    if (!descriptionResult)
      return std::unexpected(std::move(descriptionResult.error()));
    auto description = std::move(*descriptionResult);
    if (auto result = root().logger().error("Couldn't queue radio message '{}': {}", description, decoded.error().what()); !result)
      return std::unexpected(std::move(result.error()));
    return {};
  }
  if (!*decoded)
    return {};
  auto message = std::move(**decoded);
  if (m_log->radioMessages().contains(message.messageId))
    return {};
  if (message.type == RadioMessageType::Mission) {
    if (m_missionRadioMessages.contains(message.messageId))
      return {};
    m_missionRadioMessages.add(message.messageId);
  }
  for (auto const& pending : m_pendingRadioMessages) {
    if (pending.messageId == message.messageId)
      return {};
  }
  for (auto& delayed : m_delayedRadioMessages) {
    if (delayed.second.messageId == message.messageId) {
      if (delay == 0)
        delayed.first.setDone();
      return {};
    }
  }
  if (delay > 0) {
    m_delayedRadioMessages.append(std::pair<GameTimer, RadioMessage>{GameTimer(delay), std::move(message)});
    return {};
  }
  return queueRadioMessage(std::move(message));
}

std::expected<void, StarException> Player::queueRadioMessage(RadioMessage message) {
  if (message.important) {
    m_interruptRadioMessage = true;
    m_pendingRadioMessages.prepend(message);
  } else {
    m_pendingRadioMessages.append(message);
  }
  return {};
}

std::expected<std::optional<Json>, StarException> Player::pullPendingCinematic() {
  if (m_pendingCinematic && m_pendingCinematic->isType(Json::Type::String)) {
    auto nameResult = m_pendingCinematic->toString();
    if (!nameResult)
      return std::unexpected(std::move(nameResult.error()));
    auto name = std::move(*nameResult);
    m_log->addCinematic(name);
  }
  return take(m_pendingCinematic);
}

std::expected<void, StarException> Player::setPendingCinematic(Json const& cinematic, bool unique) {
  if (unique && cinematic.isType(Json::Type::String)) {
    auto nameResult = cinematic.toString();
    if (!nameResult)
      return std::unexpected(std::move(nameResult.error()));
    auto name = std::move(*nameResult);
    if (m_log->cinematics().contains(name))
      return {};
  }
  m_pendingCinematic = cinematic;
  return {};
}

std::expected<void, StarException> Player::setInCinematic(bool inCinematic) {
  if (inCinematic)
    {
      if (auto result = m_statusController->setPersistentEffects("cinematic", m_inCinematicStatusEffects); !result)
        return std::unexpected(std::move(result.error()));
    }
  else
    {
      if (auto result = m_statusController->setPersistentEffects("cinematic", {}); !result)
        return std::unexpected(std::move(result.error()));
    }
  return {};
}

std::optional<std::pair<std::optional<std::pair<StringList, int>>, float>> Player::pullPendingAltMusic() {
  return take(m_pendingAltMusic);
}

std::optional<PlayerWarpRequest> Player::pullPendingWarp() {
  return take(m_pendingWarp);
}

void Player::setPendingWarp(String const& action, std::optional<String> const& animation, bool deploy) {
  m_pendingWarp = PlayerWarpRequest{action, animation, deploy};
}

std::expected<std::optional<std::pair<Json, RpcPromiseKeeper<Json>>>, StarException> Player::pullPendingConfirmation() {
  if (m_pendingConfirmations.empty())
    return std::optional<std::pair<Json, RpcPromiseKeeper<Json>>>{};
  auto confirmationResult = m_pendingConfirmations.takeFirst();
  if (!confirmationResult)
    return std::unexpected(std::move(confirmationResult.error()));
  auto confirmation = std::move(*confirmationResult);
  return std::optional<std::pair<Json, RpcPromiseKeeper<Json>>>{std::move(confirmation)};
}

void Player::queueConfirmation(Json const& dialogConfig, RpcPromiseKeeper<Json> const& resultPromise) {
  m_pendingConfirmations.append(std::make_pair(dialogConfig, resultPromise));
}

AiState const& Player::aiState() const {
  return m_aiState;
}

AiState& Player::aiState() {
  return m_aiState;
}

bool Player::inspecting() const {
  return is<InspectionTool>(m_tools->primaryHandItem()) || is<InspectionTool>(m_tools->altHandItem());
}

std::expected<EntityHighlightEffect, StarException> Player::inspectionHighlight(std::shared_ptr<InspectableEntity> const& inspectableEntity) const {
  auto tool = as<InspectionTool>(m_tools->primaryHandItem());
  if (!tool)
    tool = as<InspectionTool>(m_tools->altHandItem());
  EntityHighlightEffect result;
  if (!tool)
    return result;
  auto nameResult = inspectableEntity->inspectionLogName();
  if (!nameResult)
    return std::unexpected(std::move(nameResult.error()));
  auto name = std::move(*nameResult);
  if (!name)
    return result;
  auto levelResult = tool->inspectionHighlightLevel(inspectableEntity);
  if (!levelResult)
    return std::unexpected(std::move(levelResult.error()));
  auto level = std::move(*levelResult);
  result.level = level;
  if (result.level > 0) {
    if (m_interestingObjects.contains(*name))
      result.type = EntityHighlightEffectType::Interesting;
    else if (m_log->scannedObjects().contains(*name))
      result.type = EntityHighlightEffectType::Inspected;
    else
      result.type = EntityHighlightEffectType::Inspectable;
  }
  return result;
}

std::expected<Vec2F, StarException> Player::cameraPosition() {
  if (auto currentWorld = maybeWorld()) {
    if (auto anchor = as<LoungeAnchor>(m_movementController->entityAnchor())) {
      if (anchor->cameraFocus) {
        auto state = m_movementController->anchorState();
        if (!state)
          return std::unexpected(PlayerException("Player lounge anchor has no anchor state"));
        if (auto entity = currentWorld->get().entity(state->entityId))
          return entity->position();
      }
    }
    if (m_cameraFocusEntity) {
      if (auto entity = currentWorld->get().entity(*m_cameraFocusEntity))
        return entity->position();
      m_cameraFocusEntity = {};
    }
  }
  return position();
}

std::shared_ptr<NetworkedAnimator> Player::effectsAnimator() {
  return m_effectsAnimator;
}

const String secretProprefix = std::string("\0JsonProperty\0", sizeof("\0JsonProperty\0") - 1);

std::optional<StringView> Player::getSecretPropertyView(String const& name) const {
  if (auto tag = m_effectsAnimator->globalTag(secretProprefix + name)) {
    auto const& view = tag->get().utf8();
    DataStreamExternalBuffer buffer{std::span<char const>(view)};
    auto type = buffer.read<std::uint8_t>();
    if (!type || (Json::Type)(*type - 1) != Json::Type::String)
      return {};
    auto len = buffer.readVlqU();
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

std::optional<std::reference_wrapper<String const>> Player::getSecretPropertyRef(String const& name) const {
  return m_effectsAnimator->globalTag(secretProprefix + name);
}

std::expected<Json, StarException> Player::getSecretProperty(String const& name, Json defaultValue) const {
  if (auto tag = m_effectsAnimator->globalTag(secretProprefix + name)) {
    DataStreamExternalBuffer buffer{std::span<char const>(tag->get().utf8())};
    auto value = buffer.read<Json>();
    if (value)
      return std::move(*value);
    if (auto result = root().logger().error("Exception reading secret player property '{}': {}", name, value.error().what()); !result)
      return std::unexpected(std::move(result.error()));
  }
  return defaultValue;
}

std::expected<void, StarException> Player::setSecretProperty(String const& name, Json const& value) {
  if (value) {
    DataStreamBuffer ds;
    if (auto result = ds.write(value); !result)
      return std::unexpected(std::move(result.error()));
    auto const& data = ds.data();
    m_effectsAnimator->setGlobalTag(secretProprefix + name, String(std::string_view(data.span().data(), data.size())));
  } else {
    m_effectsAnimator->removeGlobalTag(secretProprefix + name);
  }
  return {};
}

std::expected<void, StarException> Player::refreshHumanoidParameters() {
  auto databaseResult = root().speciesDatabase();
  if (!databaseResult)
    return std::unexpected(std::move(databaseResult.error()));
  auto database = std::move(*databaseResult);
  auto definitionResult = database->species(m_identity.species);
  if (!definitionResult)
    return std::unexpected(std::move(definitionResult.error()));
  auto definition = std::move(*definitionResult);
  if (isMaster() || !inWorld()) {
    m_refreshedHumanoidParameters.trigger();
    if (auto result = installHumanoid(); !result)
      return std::unexpected(std::move(result.error()));
    m_effectsAnimator->setGlobalTag("effectDirectives", definition->effectDirectives());
    m_deathParticleBurst.set(humanoid()->defaultDeathParticles());
    m_statusController->setStatusProperty("ouchNoise", definition->ouchNoise(m_identity.gender));
    m_scriptedAnimationParameters.clear();
  } else {
    m_humanoidParameters = m_currentNetHumanoid->humanoidParameters();
  }
  auto armorResult = m_armor->diskStore();
  if (!armorResult)
    return std::unexpected(std::move(armorResult.error()));
  auto armor = std::move(*armorResult);
  m_armor->reset();
  if (auto result = m_armor->diskLoad(armor); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = m_armor->setupHumanoid(*humanoid(), forceNude()); !result)
    return std::unexpected(std::move(result.error()));
  auto parametersResult = ActorMovementParameters::create(jsonMerge(humanoid()->defaultMovementParameters(), humanoid()->playerMovementParameters().value_or(m_config->movementParameters)));
  if (!parametersResult)
    return std::unexpected(std::move(parametersResult.error()));
  auto parameters = std::move(*parametersResult);
  if (auto result = m_movementController->resetBaseParameters(parameters); !result)
    return std::unexpected(std::move(result.error()));
  if (inWorld()) {
    if (isMaster()) {
      for (auto& script : m_genericScriptContexts) {
        if (script.second->initialized()) {
          if (auto result = script.second->removeCallbacks("animator"); !result)
            return std::unexpected(std::move(result.error()));
          auto callbacksResult = LuaBindings::makeNetworkedAnimatorCallbacks(humanoid()->networkedAnimator()->get());
          if (!callbacksResult)
            return std::unexpected(std::move(callbacksResult.error()));
          auto callbacks = std::move(*callbacksResult);
          if (auto result = script.second->addCallbacks("animator", std::move(callbacks)); !result)
            return std::unexpected(std::move(result.error()));
          if (auto result = script.second->invoke("refreshHumanoidParameters"); !result)
            return std::unexpected(std::move(result.error()));
        }
      }
    }
    auto currentWorldResult = world();
    if (!currentWorldResult)
      return std::unexpected(std::move(currentWorldResult.error()));
    auto& currentWorld = currentWorldResult->get();
    if (currentWorld.isClient() && m_scriptedAnimator.initialized()) {
      if (auto result = m_scriptedAnimator.uninit(); !result)
        return std::unexpected(std::move(result.error()));
      if (auto result = m_scriptedAnimator.removeCallbacks("animationConfig"); !result)
        return std::unexpected(std::move(result.error()));
      if (auto result = m_scriptedAnimator.removeCallbacks("entity"); !result)
        return std::unexpected(std::move(result.error()));
      if (auto result = m_scriptedAnimator.setScripts(humanoid()->animationScripts()); !result)
        return std::unexpected(std::move(result.error()));
      auto animationCallbacksResult = LuaBindings::makeScriptedAnimatorCallbacks(humanoid()->networkedAnimator()->get(),
        [&parameters = m_scriptedAnimationParameters](String const& name, Json const& defaultValue) -> std::expected<Json, StarException> {
          return parameters.value(name, defaultValue);
        });
      if (!animationCallbacksResult)
        return std::unexpected(std::move(animationCallbacksResult.error()));
      auto animationCallbacks = std::move(*animationCallbacksResult);
      if (auto result = m_scriptedAnimator.addCallbacks("animationConfig", std::move(animationCallbacks)); !result)
        return std::unexpected(std::move(result.error()));
      auto entityCallbacksResult = LuaBindings::makeEntityCallbacks(*this);
      if (!entityCallbacksResult)
        return std::unexpected(std::move(entityCallbacksResult.error()));
      auto entityCallbacks = std::move(*entityCallbacksResult);
      if (auto result = m_scriptedAnimator.addCallbacks("entity", std::move(entityCallbacks)); !result)
        return std::unexpected(std::move(result.error()));
      if (auto result = m_scriptedAnimator.init(currentWorld); !result)
        return std::unexpected(std::move(result.error()));
    }
  }
  return {};
}

std::expected<void, StarException> Player::setAnimationParameter(String name, Json value) {
  m_scriptedAnimationParameters.set(std::move(name), std::move(value));
  return {};
}

}

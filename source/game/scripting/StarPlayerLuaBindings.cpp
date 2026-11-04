module;
import star.json_extra;
import star.json;
import star.xx_hash;
import star.ordered_map;
import star.lru_cache;
import star.thread;
import star.time;
import star.random;
import star.ttl_cache;
import star.casting;
import star.set;
import star.string;
import star.color;
import star.list;
import star.image_processing;
import star.hash;
import star.data_stream;
import star.string_view;
import star.directives;
import star.asset_path;
import star.bi_map;
import star.game_types;
import star.lua;
import star.io_device;
import star.ref_ptr;
import star.logging;
import star.listener;
import std;
import star.format;
import star.version;
import star.id_map;
import star.rpc_promise;
import star.a_star;
import star.ordered_set;
import star.audio;
import star.map;
import star.either;
import star.periodic_function;
import star.strong_typedef;
import star.vector;
import star.weighted_pool;
import star.byte_array;
import star.data_stream_devices;

import std;
typedef struct ZSTD_CCtx_s ZSTD_CCtx;
typedef struct ZSTD_DCtx_s ZSTD_DCtx;
typedef ZSTD_DCtx ZSTD_DStream;
typedef ZSTD_CCtx ZSTD_CStream;
import star.zstd_compression;
import star.net_compatibility;
import star.worker_pool;

import star.sector_array_2d;
import star.perlin;
import star.btree;
import star.btree_database;
import star.image;


import star.lua_root;
import star.lua_actor_movement_component;
import star.drawable;
import star.item;
import star.item_descriptor;
import star.item_database;
import star.item_recipe;
import star.asset_source;
import star.assets;
import star.root_base;
import star.configuration;
import star.root;
import star.animation;
import star.particle;
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
import star.ai_types;
import star.entity_rendering_types;
import star.entity_rendering;
import star.player_types;
import star.lua_components;
import star.lua_animation_component;
import star.radio_message_database;
import star.player;
import star.player_tech;
import star.player_log;
import star.uuid;
import star.celestial_coordinate;
import star.warping;
import star.player_universe_map;
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
import star.player_codexes;
import star.quest_descriptor;

module star.player_lua_bindings;
import star.codex;
import star.networked_animator_lua_bindings;
import star.tech_database;


import star.client_context;
import star.player_inventory;
import star.statistics;
import star.item_bag;
import star.team_client;

import star.quest_manager;
import star.quests;

namespace Star {

std::expected<LuaCallbacks, StarException> LuaBindings::makePlayerCallbacks(Root& root, Player& player) {
  LuaCallbacks callbacks;

  if (auto registered = callbacks.registerCallback("inWorld", [&player] { return player.inWorld(); }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback("save", [&player]() -> std::expected<Json, StarException> {
    return player.diskStore();
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback("load", [&player](Json const& data) -> std::expected<void, StarException> {
    auto saved = player.diskStore();
    if (!saved) return std::unexpected(std::move(saved.error()));
    auto loaded = player.diskLoad(data);
    if (loaded) return {};
    auto rollback = player.diskLoad(*saved);
    if (!rollback)
      return std::unexpected(loaded.error().withContext(strf("Player load failed; restoring saved player state also failed: {}", printException(ExceptionDiagnostic(rollback.error()), true))));
    return std::unexpected(std::move(loaded.error()));
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback("effectsAnimator", [&player]() -> std::expected<LuaCallbacks, StarException> {
    return LuaBindings::makeNetworkedAnimatorCallbacks(*player.effectsAnimator());
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback("teamMembers", [&player]() -> std::expected<std::optional<JsonArray>, StarException> {
    if (auto client = player.universeClient()) {
      return client->get().teamClient()->members().transformed([](TeamClient::Member const& member) -> Json {
        return JsonObject{
          {"name", member.name},
          {"uuid", member.uuid.hex()},
          {"entity", member.entity},
          {"healthPercentage", member.healthPercentage},
          {"energyPercentage", member.energyPercentage}};
      });
    }
    return {};
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback(   "humanoidIdentity", [&player]() -> std::expected<Json, StarException> {
    return player.humanoid()->identity().toJson();
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback("setHumanoidIdentity", [&player](Json const& id) -> std::expected<void, StarException> {
    auto identity = HumanoidIdentity::create(id);
    if (!identity) return std::unexpected(std::move(identity.error()));
    return player.setIdentity(std::move(*identity));
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback("setHumanoidParameter", [&player](String key, std::optional<Json> value) -> std::expected<void, StarException> {
    return player.setHumanoidParameter(key, value);
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback("getHumanoidParameter", [&player](String key) -> std::expected<std::optional<Json>, StarException> {
    return player.getHumanoidParameter(key);
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback("setHumanoidParameters", [&player](JsonObject parameters) -> std::expected<void, StarException> {
    return player.setHumanoidParameters(parameters);
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback("getHumanoidParameters", [&player]() -> std::expected<JsonObject, StarException> {
    return player.getHumanoidParameters();
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback("refreshHumanoidParameters", [&player]() -> std::expected<void, StarException> {
    return player.refreshHumanoidParameters();
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback("humanoidConfig", [&player](bool withOverrides) -> std::expected<Json, StarException> {
    return player.humanoid()->humanoidConfig(withOverrides);
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback("controlMovementState", [&player](std::optional<String> const& state) -> std::expected<void, StarException> {
    return player.controlMovementState(state);
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback(   "bodyDirectives", [&player]() -> std::expected<String, StarException> {
    return player.identity().bodyDirectives.string();
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback("setBodyDirectives", [&player](String const& str) -> std::expected<void, StarException> {
    return player.setBodyDirectives(str);
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback(   "emoteDirectives", [&player]() -> std::expected<String, StarException> {
    return player.identity().emoteDirectives.string();
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback("setEmoteDirectives", [&player](String const& str) -> std::expected<void, StarException> {
    return player.setEmoteDirectives(str);
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback(   "hairGroup",      [&player]() -> std::expected<String, StarException> {
    return player.identity().hairGroup;
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback("setHairGroup",      [&player](String const& str) -> std::expected<void, StarException> {
    return player.setHairGroup(str);
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback(   "hairType",       [&player]() -> std::expected<String, StarException> {
    return player.identity().hairType;
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback("setHairType",       [&player](String const& str) -> std::expected<void, StarException> {
    return player.setHairType(str);
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback(   "hairDirectives", [&player]() -> std::expected<String, StarException> {
    return player.identity().hairDirectives.string();
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback("setHairDirectives", [&player](String const& str) -> std::expected<void, StarException> {
    return player.setHairDirectives(str);
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback(   "facialHairGroup",      [&player]() -> std::expected<String, StarException> {
    return player.identity().facialHairGroup;
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback("setFacialHairGroup",      [&player](String const& str) -> std::expected<void, StarException> {
    return player.setFacialHairGroup(str);
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback(   "facialHairType",       [&player]() -> std::expected<String, StarException> {
    return player.identity().facialHairType;
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback("setFacialHairType",       [&player](String const& str) -> std::expected<void, StarException> {
    return player.setFacialHairType(str);
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback(   "facialHairDirectives", [&player]() -> std::expected<String, StarException> {
    return player.identity().facialHairDirectives.string();
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback("setFacialHairDirectives", [&player](String const& str) -> std::expected<void, StarException> {
    return player.setFacialHairDirectives(str);
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback(   "facialMaskGroup",      [&player]() -> std::expected<String, StarException> {
    return player.identity().facialMaskGroup;
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback("setFacialMaskGroup",      [&player](String const& str) -> std::expected<void, StarException> {
    return player.setFacialMaskGroup(str);
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback(   "facialMaskType",       [&player]() -> std::expected<String, StarException> {
    return player.identity().facialMaskType;
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback("setFacialMaskType",       [&player](String const& str) -> std::expected<void, StarException> {
    return player.setFacialMaskType(str);
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback(   "facialMaskDirectives", [&player]() -> std::expected<String, StarException> {
    return player.identity().facialMaskDirectives.string();
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback("setFacialMaskDirectives", [&player](String const& str) -> std::expected<void, StarException> {
    return player.setFacialMaskDirectives(str);
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback("hair", [&player]() -> std::expected<decltype(luaTupleReturn(String(), String(), String())), StarException> {
    HumanoidIdentity const& identity = player.identity();
    return luaTupleReturn(identity.hairGroup, identity.hairType, identity.hairDirectives.string());
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback("facialHair", [&player]() -> std::expected<decltype(luaTupleReturn(String(), String(), String())), StarException> {
    HumanoidIdentity const& identity = player.identity();
    return luaTupleReturn(identity.facialHairGroup, identity.facialHairType, identity.facialHairDirectives.string());
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback("facialMask", [&player]() -> std::expected<decltype(luaTupleReturn(String(), String(), String())), StarException> {
    HumanoidIdentity const& identity = player.identity();
    return luaTupleReturn(identity.facialMaskGroup, identity.facialMaskType, identity.facialMaskDirectives.string());
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback("setFacialHair", [&player](std::optional<String> const& group, std::optional<String> const& type, std::optional<String> const& directives) -> std::expected<void, StarException> {
    if (group && type && directives)
      return player.setFacialHair(*group, *type, *directives);
    if (group) { auto changed = player.setFacialHairGroup(*group);
    if (!changed) return std::unexpected(std::move(changed.error()));
 }
    if (type) { auto changed = player.setFacialHairType(*type);
    if (!changed) return std::unexpected(std::move(changed.error()));
 }
    if (directives) { auto changed = player.setFacialHairDirectives(*directives);
    if (!changed) return std::unexpected(std::move(changed.error()));
 }
    return {};
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback("setFacialMask", [&player](std::optional<String> const& group, std::optional<String> const& type, std::optional<String> const& directives) -> std::expected<void, StarException> {
    if (group && type && directives)
      return player.setFacialMask(*group, *type, *directives);
    if (group) { auto changed = player.setFacialMaskGroup(*group);
    if (!changed) return std::unexpected(std::move(changed.error()));
 }
    if (type) { auto changed = player.setFacialMaskType(*type);
    if (!changed) return std::unexpected(std::move(changed.error()));
 }
    if (directives) { auto changed = player.setFacialMaskDirectives(*directives);
    if (!changed) return std::unexpected(std::move(changed.error()));
 }
    return {};
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback("setHair", [&player](std::optional<String> const& group, std::optional<String> const& type, std::optional<String> const& directives) -> std::expected<void, StarException> {
    if (group && type && directives)
      return player.setHair(*group, *type, *directives);
    if (group) { auto changed = player.setHairGroup(*group);
    if (!changed) return std::unexpected(std::move(changed.error()));
 }
    if (type) { auto changed = player.setHairType(*type);
    if (!changed) return std::unexpected(std::move(changed.error()));
 }
    if (directives) { auto changed = player.setHairDirectives(*directives);
    if (!changed) return std::unexpected(std::move(changed.error()));
 }
    return {};
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback(   "description", [&player]() -> std::expected<String, StarException> {
    return player.description();
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback("setDescription", [&player](String const& description) -> std::expected<void, StarException> {
    player.setDescription(description);
    return {};
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback(   "name", [&player]() -> std::expected<String, StarException> {
    return player.name();
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback("setName", [&player](String const& name) -> std::expected<void, StarException> {
    return player.setName(name);
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback(   "nametag", [&player]() -> std::expected<String, StarException> {
    return player.nametag();
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback("setNametag", [&player](std::optional<String> const& nametag) -> std::expected<void, StarException> {
    return player.setNametag(nametag);
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback(   "species", [&player]() -> std::expected<String, StarException> {
    return player.species();
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback("setSpecies", [&player](String const& species) -> std::expected<void, StarException> {
    return player.setSpecies(species);
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback(   "imagePath", [&player]() -> std::expected<std::optional<String>, StarException> {
    return player.identity().imagePath;
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback("setImagePath", [&player](std::optional<String> const& imagePath) -> std::expected<void, StarException> {
    return player.setImagePath(imagePath);
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback("gender",
                                                   [&player]() -> std::expected<String, StarException> {
                                                     auto value = enumName<GenderNames>(player.gender());
                                                     if (!value)
                                                       return std::unexpected(std::move(value.error()));
                                                     return String(*value);
                                                   });
      !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered =
        callbacks.registerCallback("setGender",
                                   [&player](String const& gender) -> std::expected<void, StarException> {
                                     auto value = enumValue<GenderNames>(gender);
                                     if (!value)
                                       return std::unexpected(std::move(value.error()));
                                     return player.setGender(*value);
                                   });
      !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback(   "personality", [&player]() -> std::expected<Json, StarException> {
    return jsonFromPersonality(player.identity().personality);
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback("setPersonality", [&player](Json const& personalityConfig) -> std::expected<void, StarException> {
    Personality personality = player.identity().personality;
    auto parsed = parsePersonality(personality, personalityConfig);
    if (!parsed) return std::unexpected(std::move(parsed.error()));
    return player.setPersonality(personality);
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback(   "favoriteColor", [&player]() -> std::expected<Color, StarException> {
    return player.favoriteColor();
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback("setFavoriteColor", [&player](Color color) -> std::expected<void, StarException> {
    return player.setFavoriteColor(color);
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback(   "mode", [&player]() -> std::expected<String, StarException> {
    if (!PlayerModeNames) return std::unexpected(PlayerModeNames.error());
    auto value = PlayerModeNames->getRight(player.modeType());
    if (!value) return std::unexpected(std::move(value.error()));
    return value->get();
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback("hungerEnabled", [&player]() {
    return player.modeConfig().hunger;
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback("lastDamagedTarget", [&player]() -> std::optional<EntityId> {
    auto id = player.lastDamagedTarget();
    return id == NullEntityId ? std::nullopt : std::optional<EntityId>(id);
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback("timeSinceLastGaveDamage", [&player]() {
    return player.timeSinceLastGaveDamage();
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback("position", [&player]() {
    return player.position();
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback("setMode", [&player](String const& modeName) -> std::expected<void, StarException> {
    if (!PlayerModeNames) return std::unexpected(PlayerModeNames.error());
    auto value = PlayerModeNames->getLeft(modeName);
    if (!value) return std::unexpected(std::move(value.error()));
    return player.setModeType(value->get());
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback(   "interactRadius", [&player]() -> std::expected<float, StarException> {
    return player.interactRadius();
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback("setInteractRadius", [&player](float radius) -> std::expected<void, StarException> {
    player.setInteractRadius(radius);
    return {};
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  
  if (auto registered = callbacks.registerCallback("setRenderLayer", [&player](std::optional<String> const& layer) -> std::expected<void, StarException> {
    if (layer) {
      auto parsed = parseRenderLayer(*layer);
    if (!parsed) return std::unexpected(std::move(parsed.error()));
    return player.setRenderLayer(*parsed);
    }
    return player.setRenderLayer({});
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback("actionBarGroup", [&player]() -> std::expected<decltype(luaTupleReturn(int(), std::uint8_t())), StarException> {
    return luaTupleReturn(player.inventory()->customBarGroup() + 1, player.inventory()->customBarGroups());
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback("setActionBarGroup", [&player](int group) -> std::expected<void, StarException> {
    player.inventory()->setCustomBarGroup((group - 1) % (unsigned)player.inventory()->customBarGroups());
    return {};
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback("selectedActionBarSlot", [&player](LuaEngine& engine) -> std::expected<std::optional<LuaValue>, StarException> {
    auto location = player.inventory()->selectedActionBarLocation();
    if (std::holds_alternative<CustomBarIndex>(location)) {
      auto value = engine.luaFrom<CustomBarIndex>(std::get<CustomBarIndex>(location) + 1);
    if (!value) return std::unexpected(std::move(value.error()));
    return std::optional<LuaValue>(std::move(*value));
    }
    if (std::holds_alternative<EssentialItem>(location)) {
      if (!EssentialItemNames) return std::unexpected(EssentialItemNames.error());
    auto name = EssentialItemNames->getRight(std::get<EssentialItem>(location));
    if (!name) return std::unexpected(std::move(name.error()));
    auto value = engine.luaFrom<String>(name->get());
    if (!value) return std::unexpected(std::move(value.error()));
    return std::optional<LuaValue>(std::move(*value));
    }
    return {};
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback("setSelectedActionBarSlot", [&player](std::variant<std::monostate, int, String> const& slot) -> std::expected<void, StarException> {
    auto inventory = player.inventory();
    if (std::holds_alternative<std::monostate>(slot)) {
      inventory->selectActionBarLocation({});
    } else if (std::holds_alternative<int>(slot)) {
      CustomBarIndex wrapped = (std::get<int>(slot) - 1) % (unsigned)inventory->customBarIndexes();
      inventory->selectActionBarLocation(SelectedActionBarLocation(wrapped));
    } else {
      if (!EssentialItemNames) return std::unexpected(EssentialItemNames.error());
    auto item = EssentialItemNames->getLeft(std::get<String>(slot));
    if (!item) return std::unexpected(std::move(item.error()));
    inventory->selectActionBarLocation(SelectedActionBarLocation(item->get()));
    }
    return {};
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback("actionBarSlotLink", [&player](int slot, String const& handName) -> std::expected<std::optional<InventorySlot>, StarException> {
    auto inventory = player.inventory();
    CustomBarIndex wrapped = (slot - 1) % (unsigned)inventory->customBarIndexes();
    if (handName == "primary") return inventory->customBarPrimarySlot(wrapped);
    if (handName == "alt") return inventory->customBarSecondarySlot(wrapped);
    return std::unexpected(StarException::format("Unknown tool hand {}", handName));
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback("setActionBarSlotLink", [&player](int slot, String const& handName, std::optional<InventorySlot> inventorySlot) -> std::expected<void, StarException> {
    auto inventory = player.inventory();
    CustomBarIndex wrapped = (slot - 1) % (unsigned)inventory->customBarIndexes();
    if (inventorySlot && !inventory->slotValid(*inventorySlot)) inventorySlot.reset();
    if (handName == "primary") return inventory->setCustomBarPrimarySlot(wrapped, inventorySlot);
    if (handName == "alt") return inventory->setCustomBarSecondarySlot(wrapped, inventorySlot);
    return std::unexpected(StarException::format("Unknown tool hand {}", handName));
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  struct ActionBarViewCache {
    PlayerInventory const* inventory = nullptr;
    List<std::shared_ptr<Item>> items;
    List<unsigned> flags;
    SelectedActionBarLocation selection;
    unsigned group = 0;
    std::uint64_t revision = 0;
  };
  if (auto registered = callbacks.registerCallback("actionBarView",
      [&player, cache = ActionBarViewCache{}](LuaEngine& engine, std::optional<std::uint64_t> knownRevision) mutable -> std::expected<std::optional<LuaTable>, StarException> {
    auto inventory = player.inventory();
    auto indexes = inventory->customBarIndexes();
    std::size_t size = indexes * 2 + EssentialItemCount + 1;
    bool changed = cache.inventory != inventory.get() || cache.items.size() != size
      || cache.selection != inventory->selectedActionBarLocation() || cache.group != inventory->customBarGroup();
    cache.inventory = inventory.get();
    cache.items.resize(size);
    cache.flags.resize(size);
    auto remember = [&](std::size_t i, std::shared_ptr<Item> item) {
      unsigned flags = item ? (item->twoHanded() ? 1u : 0u) | (item->hasSecondaryDrawables() ? 2u : 0u) : 0u;
      changed = changed || cache.items[i] != item || cache.flags[i] != flags;
      cache.items[i] = std::move(item);
      cache.flags[i] = flags;
    };
    for (CustomBarIndex i = 0; i < indexes; ++i) {
      auto primary = inventory->customBarPrimarySlot(i);
      if (!primary) return std::unexpected(std::move(primary.error()));
      auto secondary = inventory->customBarSecondarySlot(i);
      if (!secondary) return std::unexpected(std::move(secondary.error()));
      auto first = *primary ? inventory->itemsAt(**primary) : std::expected<std::shared_ptr<Item>, StarException>(std::shared_ptr<Item>{});
      if (!first) return std::unexpected(std::move(first.error()));
      auto second = *secondary ? inventory->itemsAt(**secondary) : std::expected<std::shared_ptr<Item>, StarException>(std::shared_ptr<Item>{});
      if (!second) return std::unexpected(std::move(second.error()));
      remember(i * 2, std::move(*first));
      remember(i * 2 + 1, std::move(*second));
    }
    for (unsigned i = 0; i < EssentialItemCount; ++i)
      remember(indexes * 2 + i, inventory->essentialItem(static_cast<EssentialItem>(i)));
    remember(size - 1, inventory->swapSlotItem());
    cache.selection = inventory->selectedActionBarLocation();
    cache.group = inventory->customBarGroup();
    // Context rebinding must not reuse a token still held by a session pane.
    static std::atomic<std::uint64_t> nextRevision{0};
    if (changed || !cache.revision) cache.revision = nextRevision.fetch_add(1, std::memory_order_relaxed) + 1;
    if (knownRevision && *knownRevision == cache.revision) return {};
    auto view = engine.createTable(0, 7);
    if (!view) return std::unexpected(std::move(view.error()));
    auto put = [&](std::string_view name, auto value) { return view->set(name, std::move(value)); };
    if (auto r = put("revision", cache.revision); !r) return std::unexpected(std::move(r.error()));
    if (auto r = put("indexes", indexes); !r) return std::unexpected(std::move(r.error()));
    if (auto r = put("groups", inventory->customBarGroups()); !r) return std::unexpected(std::move(r.error()));
    if (auto r = put("group", cache.group + 1); !r) return std::unexpected(std::move(r.error()));
    if (auto index = std::get_if<CustomBarIndex>(&cache.selection)) {
      if (auto r = put("selected", *index + 1); !r) return std::unexpected(std::move(r.error()));
    } else if (auto essential = std::get_if<EssentialItem>(&cache.selection)) {
      if (!EssentialItemNames) return std::unexpected(EssentialItemNames.error());
      auto name = EssentialItemNames->getRight(*essential);
      if (!name) return std::unexpected(std::move(name.error()));
      if (auto r = put("selected", name->get()); !r) return std::unexpected(std::move(r.error()));
    }
    auto items = engine.createTable(static_cast<int>(size), 0);
    if (!items) return std::unexpected(std::move(items.error()));
    for (std::size_t i = 0; i < size; ++i) {
      auto entry = engine.createTable(0, 4);
      if (!entry) return std::unexpected(std::move(entry.error()));
      if (auto const& item = cache.items[i]) {
        auto handle = engine.createUserData(item);
        if (!handle) return std::unexpected(std::move(handle.error()));
        if (auto r = entry->set("handle", std::move(*handle)); !r) return std::unexpected(std::move(r.error()));
        std::size_t identity = 0;
        while (cache.items[identity] != item) ++identity;
        if (auto r = entry->set("identity", identity + 1); !r) return std::unexpected(std::move(r.error()));
      }
      if (auto r = entry->set("twoHanded", (cache.flags[i] & 1) != 0); !r) return std::unexpected(std::move(r.error()));
      if (auto r = entry->set("secondaryDrawables", (cache.flags[i] & 2) != 0); !r) return std::unexpected(std::move(r.error()));
      if (auto r = items->set(i + 1, std::move(*entry)); !r) return std::unexpected(std::move(r.error()));
    }
    if (auto r = put("items", std::move(*items)); !r) return std::unexpected(std::move(r.error()));
    return std::optional<LuaTable>(std::move(*view));
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback("linkActionBarSwap", [&player](unsigned slot, bool primary) -> std::expected<void, StarException> {
    if (!slot || slot > player.inventory()->customBarIndexes())
      return std::unexpected(StarException("Action bar slot out of range"));
    return player.inventory()->linkCustomBarSwap(static_cast<CustomBarIndex>(slot - 1), primary);
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback("clearActionBarLink", [&player](unsigned slot, bool primary) -> std::expected<void, StarException> {
    if (!slot || slot > player.inventory()->customBarIndexes())
      return std::unexpected(StarException("Action bar slot out of range"));
    return player.inventory()->clearCustomBarLink(static_cast<CustomBarIndex>(slot - 1), primary);
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback("itemBagSize", [&player](String const& bagName) -> std::expected<std::optional<unsigned>, StarException> {
    if (auto bag = player.inventory()->bagContents(bagName))
      return (unsigned)bag->size();
    else
      return {};
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback("itemAllowedInBag", [&root, &player](String const& bagName, Json const& item) -> std::expected<bool, StarException> {
    auto inventory = player.inventory();
    if (!inventory->bagContents(bagName)) return false;
    auto database = root.itemDatabase();
    if (!database) return std::unexpected(std::move(database.error()));
    auto descriptor = ItemDescriptor::create(item);
    if (!descriptor) return std::unexpected(std::move(descriptor.error()));
    auto created = (*database)->item(*descriptor);
    if (!created) return std::unexpected(std::move(created.error()));
    return inventory->itemAllowedInBag(*created, bagName);
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback("item", [&player](InventorySlot const& slot) -> std::expected<std::optional<Json>, StarException> {
    if (!player.inventory()->slotValid(slot)) return {};
    auto item = player.inventory()->itemsAt(slot);
    if (!item) return std::unexpected(std::move(item.error()));
    if (!*item) return {};
    return itemSafeDescriptor(*item).toJson();
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback("setItem", [&root, &player](InventorySlot const& slot, Json const& item) -> std::expected<void, StarException> {
    if (!player.inventory()->slotValid(slot)) return {};
    auto database = root.itemDatabase();
    if (!database) return std::unexpected(std::move(database.error()));
    auto descriptor = ItemDescriptor::create(item);
    if (!descriptor) return std::unexpected(std::move(descriptor.error()));
    auto created = (*database)->item(*descriptor);
    if (!created) return std::unexpected(std::move(created.error()));
    auto changed = player.inventory()->setItem(slot, *created);
    if (!changed) return std::unexpected(std::move(changed.error()));
    return {};
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback(
        "setDamageTeam",
        [&player](String const& typeName,
                  std::optional<std::uint16_t> teamNumber) -> std::expected<void, StarException> {
          auto type = enumValue<TeamTypeNames>(typeName);
          if (!type)
            return std::unexpected(std::move(type.error()));
          player.setTeam(EntityDamageTeam(*type, teamNumber.value_or(0)));
          return {};
        });
      !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback("say", [&player](String const& message) -> std::expected<void, StarException> {
    return player.addChatMessage(message);
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback("emote", [&player](String const& emote, std::optional<float> cooldown) -> std::expected<void, StarException> {
    if (!HumanoidEmoteNames) return std::unexpected(HumanoidEmoteNames.error());
    auto value = HumanoidEmoteNames->getLeft(emote);
    if (!value) return std::unexpected(std::move(value.error()));
    player.addEmote(value->get(), cooldown);
    return {};
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback("currentEmote", [&player]() -> std::expected<decltype(luaTupleReturn(String(), float())), StarException> {
    auto emote = player.currentEmote();
    if (!HumanoidEmoteNames) return std::unexpected(HumanoidEmoteNames.error());
    auto name = HumanoidEmoteNames->getRight(emote.first);
    if (!name) return std::unexpected(std::move(name.error()));
    return luaTupleReturn(name->get(), emote.second);
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback("dance", [&player](std::optional<String> const& dance) -> std::expected<void, StarException> {
    return player.setDance(dance);
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback("currentState", [&player]() -> std::expected<String, StarException> {
    if (!Player::StateNames) return std::unexpected(Player::StateNames.error());
    auto value = Player::StateNames->getRight(player.currentState());
    if (!value) return std::unexpected(std::move(value.error()));
    return value->get();
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback("aimPosition", [&player]() -> std::expected<Vec2F, StarException> {
    return player.aimPosition();
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback("id",       [&player]() -> std::expected<EntityId, StarException> {
    return player.entityId();
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback("uniqueId", [&player]() -> std::expected<std::optional<String>, StarException> {
    return player.uniqueId();
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback("isAdmin",  [&player]() -> std::expected<bool, StarException> {
    return player.isAdmin();
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback("interact", [&player](String const& type, Json const& configData, std::optional<EntityId> const& sourceEntityId) -> std::expected<void, StarException> {
    auto action = InteractAction::create(type, sourceEntityId.value_or(NullEntityId), configData);
    if (!action) return std::unexpected(std::move(action.error()));
    return player.interact(*action);
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback("shipUpgrades", [&player]() -> std::expected<Json, StarException> {
    return player.shipUpgrades().toJson();
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback("upgradeShip", [&player](Json const& upgrades) -> std::expected<void, StarException> {
    return player.applyShipUpgrades(upgrades);
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback("setUniverseFlag", [&player](String const& flagName) -> std::expected<void, StarException> {
    auto invoked = player.clientContext()->rpcInterface()->invokeRemote("universe.setFlag", flagName);
    if (!invoked) return std::unexpected(std::move(invoked.error()));
    return {};
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback("giveBlueprint", [&player](Json const& item) -> std::expected<void, StarException> {
    auto descriptor = ItemDescriptor::create(item);
    if (!descriptor) return std::unexpected(std::move(descriptor.error()));
    auto given = player.addBlueprint(*descriptor);
    if (!given) return std::unexpected(std::move(given.error()));
    return {};
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback("blueprintKnown", [&player](Json const& item) -> std::expected<bool, StarException> {
    auto descriptor = ItemDescriptor::create(item);
    if (!descriptor) return std::unexpected(std::move(descriptor.error()));
    return player.blueprintKnown(*descriptor);
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback("availableRecipes", [&root, &player](std::optional<StringSet> const& filter) -> std::expected<JsonArray, StarException> {
    auto database = root.itemDatabase();
    if (!database) return std::unexpected(std::move(database.error()));
    auto inventory = player.inventory();
    auto items = inventory->availableItems();
    if (!items) return std::unexpected(std::move(items.error()));
    auto recipes = (*database)->recipesFromBagContents(*items, inventory->availableCurrencies(), filter.value_or(StringSet{}));
    JsonArray result;
    result.reserve(recipes.size());
    for (auto const& recipe : recipes) {
      auto json = recipe.toJson();
    if (!json) return std::unexpected(std::move(json.error()));
    result.append(std::move(*json));
    }
    return result;
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback("makeTechAvailable", [&player](String const& tech) -> std::expected<void, StarException> {
    player.techs()->makeAvailable(tech);
    return {};
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback("makeTechUnavailable", [&player](String const& tech) -> std::expected<void, StarException> {
    player.techs()->makeUnavailable(tech);
    return {};
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback("enableTech", [&player](String const& tech) -> std::expected<void, StarException> {
    return player.techs()->enable(tech);
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback("equipTech", [&player](String const& tech) -> std::expected<void, StarException> {
    return player.techs()->equip(tech);
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback("unequipTech", [&player](String const& tech) -> std::expected<void, StarException> {
    player.techs()->unequip(tech);
    return {};
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback("availableTechs", [&player]() -> std::expected<StringSet, StarException> {
    return player.techs()->availableTechs();
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback("enabledTechs", [&player]() -> std::expected<StringSet, StarException> {
    return player.techs()->enabledTechs();
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback("equippedTech", [&player](String typeName) -> std::expected<std::optional<String>, StarException> {
    if (!TechTypeNames) return std::unexpected(TechTypeNames.error());
    auto techType = TechTypeNames->getLeft(typeName);
    if (!techType) return std::unexpected(std::move(techType.error()));
    return player.techs()->equippedTechs().maybe(techType->get());
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback("currency", [&player](String const& currencyType) -> std::expected<std::uint64_t, StarException> {
    return player.currency(currencyType);
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback("addCurrency", [&player](String const& currencyType, std::uint64_t amount) -> std::expected<void, StarException> {
    return player.inventory()->addCurrency(currencyType, amount);
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback("consumeCurrency", [&player](String const& currencyType, std::uint64_t amount) -> std::expected<bool, StarException> {
    return player.inventory()->consumeCurrency(currencyType, amount);
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback("cleanupItems", [&player]() -> std::expected<void, StarException> {
    return player.inventory()->cleanup();
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback("giveItem", [&player](Json const& item) -> std::expected<void, StarException> {
    auto descriptor = ItemDescriptor::create(item);
    if (!descriptor) return std::unexpected(std::move(descriptor.error()));
    return player.giveItem(*descriptor);
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback("giveEssentialItem", [&root, &player](String const& slotName, Json const& item) -> std::expected<void, StarException> {
    if (!EssentialItemNames) return std::unexpected(EssentialItemNames.error());
    auto slot = EssentialItemNames->getLeft(slotName);
    if (!slot) return std::unexpected(std::move(slot.error()));
    auto database = root.itemDatabase();
    if (!database) return std::unexpected(std::move(database.error()));
    auto descriptor = ItemDescriptor::create(item);
    if (!descriptor) return std::unexpected(std::move(descriptor.error()));
    auto created = (*database)->item(*descriptor);
    if (!created) return std::unexpected(std::move(created.error()));
    player.inventory()->setEssentialItem(slot->get(), *created);
    return {};
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback("essentialItem", [&player](String const& slotName) -> std::expected<Json, StarException> {
    if (!EssentialItemNames) return std::unexpected(EssentialItemNames.error());
    auto slot = EssentialItemNames->getLeft(slotName);
    if (!slot) return std::unexpected(std::move(slot.error()));
    return itemSafeDescriptor(player.inventory()->essentialItem(slot->get())).toJson();
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback("removeEssentialItem", [&player](String const& slotName) -> std::expected<void, StarException> {
    if (!EssentialItemNames) return std::unexpected(EssentialItemNames.error());
    auto slot = EssentialItemNames->getLeft(slotName);
    if (!slot) return std::unexpected(std::move(slot.error()));
    player.inventory()->setEssentialItem(slot->get(), {});
    return {};
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback("setEquippedItem", [&root, &player](String const& slotName, Json const& item) -> std::expected<void, StarException> {
    if (!EquipmentSlotNames) return std::unexpected(EquipmentSlotNames.error());
    auto slot = EquipmentSlotNames->getLeft(slotName);
    if (!slot) return std::unexpected(std::move(slot.error()));
    auto database = root.itemDatabase();
    if (!database) return std::unexpected(std::move(database.error()));
    auto descriptor = ItemDescriptor::create(item);
    if (!descriptor) return std::unexpected(std::move(descriptor.error()));
    auto created = (*database)->item(*descriptor);
    if (!created) return std::unexpected(std::move(created.error()));
    auto changed = player.inventory()->setItem(InventorySlot(slot->get()), *created);
    if (!changed) return std::unexpected(std::move(changed.error()));
    return {};
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback("equippedItem", [&player](String const& slotName) -> std::expected<Json, StarException> {
    if (!EquipmentSlotNames) return std::unexpected(EquipmentSlotNames.error());
    auto slot = EquipmentSlotNames->getLeft(slotName);
    if (!slot) return std::unexpected(std::move(slot.error()));
    auto item = player.inventory()->itemsAt(InventorySlot(slot->get()));
    if (!item) return std::unexpected(std::move(item.error()));
    if (*item) return (*item)->descriptor().toJson();
    return Json();
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback("armorVisible", [&player](String const& slotName) -> std::expected<std::optional<bool>, StarException> {
    if (!EquipmentSlotNames) return std::unexpected(EquipmentSlotNames.error());
    auto slot = EquipmentSlotNames->maybeLeftRef(slotName);
    if (!slot) return std::unexpected(std::move(slot.error()));
    if (*slot)
      return player.inventory()->equipmentVisibility((*slot)->get());
    return {};
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback("setArmorVisible", [&player](String const& slotName, bool visible) -> std::expected<bool, StarException> {
    if (!EquipmentSlotNames) return std::unexpected(EquipmentSlotNames.error());
    auto slot = EquipmentSlotNames->maybeLeftRef(slotName);
    if (!slot) return std::unexpected(std::move(slot.error()));
    if (*slot) {
      player.inventory()->setEquipmentVisibility((*slot)->get(), visible);
      return true;
    }
    return false;
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback("hasItem", [&player](Json const& item, std::optional<bool> exactMatch) -> std::expected<bool, StarException> {
    auto descriptor = ItemDescriptor::create(item);
    if (!descriptor) return std::unexpected(std::move(descriptor.error()));
    return player.hasItem(*descriptor, exactMatch.value_or(false));
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback("hasCountOfItem", [&player](Json const& item, std::optional<bool> exactMatch) -> std::expected<std::uint64_t, StarException> {
    auto descriptor = ItemDescriptor::create(item);
    if (!descriptor) return std::unexpected(std::move(descriptor.error()));
    return player.hasCountOfItem(*descriptor, exactMatch.value_or(false));
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback("consumeItem", [&player](Json const& item, std::optional<bool> consumePartial, std::optional<bool> exactMatch) -> std::expected<Json, StarException> {
    auto descriptor = ItemDescriptor::create(item);
    if (!descriptor) return std::unexpected(std::move(descriptor.error()));
    auto taken = player.takeItem(*descriptor, consumePartial.value_or(false), exactMatch.value_or(false));
    if (!taken) return std::unexpected(std::move(taken.error()));
    return taken->toJson();
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback("inventoryTags", [&player]() -> std::expected<StringMap<std::size_t>, StarException> {
    auto items = player.inventory()->allItems();
    if (!items) return std::unexpected(std::move(items.error()));
    StringMap<std::size_t> counts;
    for (auto const& item : *items) {
      auto tags = item->itemTags();
    if (!tags) return std::unexpected(std::move(tags.error()));
    for (auto const& tag : *tags) ++counts[tag];
    }
    return counts;
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback("itemsWithTag", [&player](String const& tag) -> std::expected<JsonArray, StarException> {
    auto inventory = player.inventory()->allItems();
    if (!inventory) return std::unexpected(std::move(inventory.error()));
    JsonArray items;
    for (auto const& item : *inventory) {
      auto tags = item->itemTags();
    if (!tags) return std::unexpected(std::move(tags.error()));
    for (auto const& itemTag : *tags) {
        if (itemTag == tag) items.append(item->descriptor().toJson());
      }
    }
    return items;
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback("consumeTaggedItem", [&player](String const& itemTag, std::uint64_t count) -> std::expected<void, StarException> {
    auto items = player.inventory()->allItems();
    if (!items) return std::unexpected(std::move(items.error()));
    for (auto const& item : *items) {
      auto matches = item->hasItemTag(itemTag);
    if (!matches) return std::unexpected(std::move(matches.error()));
    if (*matches) {
        std::uint64_t takeCount = std::min(item->count(), count);
        auto taken = player.takeItem(item->descriptor().singular().multiply(takeCount));
    if (!taken) return std::unexpected(std::move(taken.error()));
    count -= takeCount;
        if (count == 0) break;
      }
    }
    return {};
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback("hasItemWithParameter", [&player](String const& parameterName, Json const& parameterValue) -> std::expected<bool, StarException> {
    auto items = player.inventory()->allItems();
    if (!items) return std::unexpected(std::move(items.error()));
    for (auto const& item : *items) {
      auto value = item->instanceValue(parameterName, Json());
    if (!value) return std::unexpected(std::move(value.error()));
    if (*value == parameterValue) return true;
    }
    return false;
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback("consumeItemWithParameter", [&player](String const& parameterName, Json const& parameterValue, std::uint64_t count) -> std::expected<void, StarException> {
    auto items = player.inventory()->allItems();
    if (!items) return std::unexpected(std::move(items.error()));
    for (auto const& item : *items) {
      auto matches = item->instanceValue(parameterName, Json());
    if (!matches) return std::unexpected(std::move(matches.error()));
    if (*matches == parameterValue) {
        std::uint64_t takeCount = std::min(item->count(), count);
        auto taken = player.takeItem(item->descriptor().singular().multiply(takeCount));
    if (!taken) return std::unexpected(std::move(taken.error()));
    count -= takeCount;
        if (count == 0) break;
      }
    }
    return {};
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback("getItemWithParameter", [&player](String const& parameterName, Json const& parameterValue) -> std::expected<Json, StarException> {
    auto items = player.inventory()->allItems();
    if (!items) return std::unexpected(std::move(items.error()));
    for (auto const& item : *items) {
      auto value = item->instanceValue(parameterName, Json());
    if (!value) return std::unexpected(std::move(value.error()));
    if (*value == parameterValue) return item->descriptor().toJson();
    }
    return Json();
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback("primaryHandItem", [&player]() -> std::expected<std::optional<Json>, StarException> {
    if (!player.primaryHandItem())
        return {};
      return player.primaryHandItem()->descriptor().toJson();
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback("altHandItem", [&player]() -> std::expected<std::optional<Json>, StarException> {
    if (!player.altHandItem())
        return {};
      return player.altHandItem()->descriptor().toJson();
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback("primaryHandItemTags", [&player]() -> std::expected<StringSet, StarException> {
    if (!player.primaryHandItem())
        return {};
      return player.primaryHandItem()->itemTags();
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback("altHandItemTags", [&player]() -> std::expected<StringSet, StarException> {
    if (!player.altHandItem())
        return {};
      return player.altHandItem()->itemTags();
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback("swapSlotItem", [&player]() -> std::expected<std::optional<Json>, StarException> {
    if (!player.inventory()->swapSlotItem())
        return {};
      return player.inventory()->swapSlotItem()->descriptor().toJson();
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback("setSwapSlotItem", [&root, &player](Json const& item) -> std::expected<void, StarException> {
    auto database = root.itemDatabase();
    if (!database) return std::unexpected(std::move(database.error()));
    auto descriptor = ItemDescriptor::create(item);
    if (!descriptor) return std::unexpected(std::move(descriptor.error()));
    auto created = (*database)->item(*descriptor);
    if (!created) return std::unexpected(std::move(created.error()));
    return player.inventory()->setSwapSlotItem(*created);
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback("canStartQuest",
      [&root, &player](Json const& quest) -> std::expected<bool, StarException> {
    auto arc = QuestArcDescriptor::fromJson(root, quest);
    if (!arc) return std::unexpected(std::move(arc.error()));
    return player.questManager()->canStart(*arc);
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback("startQuest", [&root, &player](Json const& quest, std::optional<String> const& serverUuid, std::optional<String> const& worldId) -> std::expected<String, StarException> {
    auto arc = QuestArcDescriptor::fromJson(root, quest);
    if (!arc) return std::unexpected(std::move(arc.error()));
    auto created = Quest::create(root, *arc, 0, player);
    if (!created) return std::unexpected(std::move(created.error()));
    auto followUp = std::move(*created);
    if (serverUuid) {
      auto uuid = Uuid::create(*serverUuid);
    if (!uuid) return std::unexpected(std::move(uuid.error()));
    followUp->setServerUuid(*uuid);
    }
    if (worldId) {
      auto id = parseWorldId(*worldId);
    if (!id) return std::unexpected(std::move(id.error()));
    followUp->setWorldId(*id);
    }
    auto offered = player.questManager()->offer(followUp);
    if (!offered) return std::unexpected(std::move(offered.error()));
    return followUp->questId();
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback("questIds", [&player]() -> std::expected<StringList, StarException> {
    return player.questManager()->quests().keys();
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback("serverQuestIds", [&player]() -> std::expected<StringList, StarException> {
    return player.questManager()->serverQuests().keys();
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback("quest", [&player](String const& questId) -> std::expected<Json, StarException> {
    if (!player.questManager()->hasQuest(questId)) return {};
    auto quest = player.questManager()->getQuest(questId);
    if (!quest) return std::unexpected(std::move(quest.error()));
    return (*quest)->diskStore();
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback("questPortrait", [&player](String const& questId, String const& portraitName) -> std::expected<std::optional<List<Drawable>>, StarException> {
    if (!player.questManager()->hasQuest(questId)) return {};
    auto quest = player.questManager()->getQuest(questId);
    if (!quest) return std::unexpected(std::move(quest.error()));
    return (*quest)->portrait(portraitName);
  }); !registered)
    return std::unexpected(std::move(registered.error()));


  if (auto registered = callbacks.registerCallback("questState", [&player](String const& questId) -> std::expected<std::optional<String>, StarException> {
    if (!player.questManager()->hasQuest(questId)) return {};
    auto quest = player.questManager()->getQuest(questId);
    if (!quest) return std::unexpected(std::move(quest.error()));
    if (!QuestStateNames) return std::unexpected(QuestStateNames.error());
    auto state = QuestStateNames->getRight((*quest)->state());
    if (!state) return std::unexpected(std::move(state.error()));
    return state->get();
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback("questObjectives", [&player](String const& questId) -> std::expected<std::optional<JsonArray>, StarException> {
    auto quest = player.questManager()->getQuest(questId);
    if (!quest) return std::unexpected(std::move(quest.error()));
    return (*quest)->objectiveList();
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback("callQuest", [&player](String const& questId, String const& func, LuaVariadic<LuaValue> const& args) -> std::expected<std::optional<LuaValue>, StarException> {
    if (!player.questManager()->hasQuest(questId)) return {};
    auto quest = player.questManager()->getQuest(questId);
    if (!quest) return std::unexpected(std::move(quest.error()));
    return (*quest)->callScript(func, args);
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback("hasQuest", [&player](String const& questId) -> std::expected<bool, StarException> {
    return player.questManager()->hasQuest(questId);
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback("hasAcceptedQuest", [&player](String const& questId) -> std::expected<bool, StarException> {
    return player.questManager()->hasAcceptedQuest(questId);
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback("hasActiveQuest", [&player](String const& questId) -> std::expected<bool, StarException> {
    return player.questManager()->isActive(questId);
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback("hasCompletedQuest", [&player](String const& questId) -> std::expected<bool, StarException> {
    return player.questManager()->hasCompleted(questId);
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback("trackedQuestId", [&player]() -> std::expected<std::optional<String>, StarException> {
    return player.questManager()->trackedQuestId();
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback("setTrackedQuest", [&player](String const& questId) -> std::expected<void, StarException> {
    if (!player.questManager()->isCurrent(questId))
      player.questManager()->setAsTracked(questId);
    else
      player.questManager()->setAsTracked({});
    return {};
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback("canTurnInQuest", [&player](String const& questId) -> std::expected<bool, StarException> {
    return player.questManager()->canTurnIn(questId);
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback("currentQuestId", [&player]() -> std::expected<std::optional<String>, StarException> {
    return player.questManager()->currentQuestId();
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback("currentQuest", [&player]() -> std::expected<Json, StarException> {
    auto maybeQuest = player.questManager()->currentQuest();
    if (maybeQuest) {
      return (*maybeQuest)->diskStore();
    }
    return {};
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback("currentQuestView", [&player]() -> Json {
    auto current = player.questManager()->currentQuest();
    if (!current)
      return {};
    auto const& quest = **current;
    JsonObject view;
    view.reserve(4);
    view.set("id", quest.questId());
    if (auto const& objectives = quest.objectiveList())
      view.set("objectives", *objectives);
    if (auto progress = quest.progress())
      view.set("progress", *progress);
    if (auto direction = quest.compassDirection())
      view.set("compassDirection", *direction);
    return view;
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback("pendingQuestDialog", [&player](String const& kind) {
    return player.questManager()->pendingDialog(kind);
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback("questDialogValid", [&player](std::uint64_t token) {
    return player.questManager()->dialogValid(token);
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback("acknowledgeQuestDialog", [&player](std::uint64_t token) {
    return player.questManager()->acknowledgeDialog(token);
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback("questDialogCinema", [&player](std::uint64_t token) {
    return player.questManager()->dialogCinema(token);
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback("finishQuestDialog", [&player](std::uint64_t token, String const& decision) {
    return player.questManager()->finishDialog(token, decision);
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback("questLogEntries", [&player](bool completed) -> Json {
    auto manager = player.questManager();
    auto quests = completed ? manager->listCompletedQuests() : manager->listActiveQuests();
    auto playerWorld = player.clientContext() ? player.clientContext()->playerWorldId() : WorldId{};
    JsonArray entries;
    entries.reserve(quests.size());
    for (auto const& quest : quests) {
      auto world = quest->worldId();
      entries.emplace_back(JsonObject{
          {"id", quest->questId()}, {"title", quest->title()}, {"main", quest->mainQuest()},
          {"unread", quest->unread()}, {"tracked", manager->isCurrent(quest->questId())},
          {"currentWorld", world && playerWorld == *world},
          {"portraitRevision", quest->portraitRevision()}});
    }
    JsonObject result{{"entries", std::move(entries)}};
    if (auto current = manager->currentQuest()) result.set("current", (*current)->questId());
    return result;
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback("questLogDetails", [&player](String const& id,
      std::optional<std::uint64_t> knownRevision, std::optional<std::uint64_t> knownPortraitRevision,
      std::optional<std::uint64_t> knownRewardsRevision) -> std::expected<Json, StarException> {
    auto manager = player.questManager();
    if (!manager->hasQuest(id)) return Json{};
    auto found = manager->getQuest(id);
    if (!found) return std::unexpected(std::move(found.error()));
    auto const& quest = **found;
    bool active = manager->isActive(id);
    bool abandonable = false;
    if (active) {
      auto allowed = quest.canBeAbandoned();
      if (!allowed) return std::unexpected(std::move(allowed.error()));
      abandonable = *allowed;
    }
    JsonObject result{{"active", active}, {"tracked", manager->isCurrent(id)},
        {"canAbandon", abandonable}, {"revision", quest.presentationRevision()},
        {"portraitRevision", quest.portraitRevision()}, {"rewardsRevision", quest.rewardsRevision()}};
    if (!knownRevision || *knownRevision != quest.presentationRevision()) {
      result.set("title", quest.title());
      result.set("text", quest.text());
    }
    if (!knownPortraitRevision || *knownPortraitRevision != quest.portraitRevision()) {
      result.set("portraitTitle", "");
      if (auto portrait = quest.portrait("Objective")) {
        auto json = jsonFromList(*portrait, [](Drawable const& drawable) { return drawable.toJson(); });
        if (!json) return std::unexpected(std::move(json.error()));
        result.set("portrait", std::move(*json));
        result.set("portraitTitle", quest.portraitTitle("Objective").value_or(""));
      }
    }
    if (!knownRewardsRevision || *knownRewardsRevision != quest.rewardsRevision()) {
      JsonArray rewards;
      rewards.reserve(quest.rewards().size());
      for (auto const& reward : quest.rewards()) rewards.emplace_back(reward->descriptor().toJson());
      result.set("rewards", std::move(rewards));
    }
    return result;
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback("markQuestRead", [&player](String const& id) {
    player.questManager()->markAsRead(id);
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback("abandonQuest", [&player](String const& id) -> std::expected<bool, StarException> {
    auto manager = player.questManager();
    if (!manager->isActive(id)) return false;
    auto quest = manager->getQuest(id);
    if (!quest) return std::unexpected(std::move(quest.error()));
    auto allowed = (*quest)->canBeAbandoned();
    if (!allowed) return std::unexpected(std::move(allowed.error()));
    if (!*allowed) return false;
    auto abandoned = (*quest)->abandon();
    if (!abandoned) return std::unexpected(std::move(abandoned.error()));
    return true;
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback("currentQuestWorld", [&player]() -> std::expected<std::optional<String>, StarException> {
    if (auto quest = player.questManager()->currentQuest()) {
      if (auto worldId = (*quest)->worldId()) {
        auto id = printWorldId(*worldId);
    if (!id) return std::unexpected(std::move(id.error()));
    return std::move(*id);
      }
    }
    return {};
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback("questWorlds", [&player]() -> std::expected<List<std::pair<String, bool>>, StarException> {
    List<std::pair<String, bool>> result;
    auto current = player.questManager()->currentQuest();
    for (auto const& quest : player.questManager()->listActiveQuests()) {
      if (auto worldId = quest->worldId()) {
        auto id = printWorldId(*worldId);
    if (!id) return std::unexpected(std::move(id.error()));
    bool selected = current && (*current)->questId() == quest->questId();
        result.append({std::move(*id), selected});
      }
    }
    return result;
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback("currentQuestLocation", [&player]() -> std::expected<Json, StarException> {
    if (auto quest = player.questManager()->currentQuest()) {
      if (auto location = (*quest)->location()) {
        return JsonObject{{"system", jsonFromVec3I(location->first)}, {"location", jsonFromSystemLocation(location->second)}};
      }
    }
    return Json();
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback("questLocations", [&player]() -> std::expected<List<std::pair<Json, bool>>, StarException> {
    List<std::pair<Json, bool>> result;
    auto current = player.questManager()->currentQuest();
    for (auto const& quest : player.questManager()->listActiveQuests()) {
      if (auto location = quest->location()) {
        Json locationJson = JsonObject{{"system", jsonFromVec3I(location->first)}, {"location", jsonFromSystemLocation(location->second)}};
        bool selected = current && (*current)->questId() == quest->questId();
        result.append({std::move(locationJson), selected});
      }
    }
    return result;
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback("enableMission", [&player](String const& mission) -> std::expected<void, StarException> {
    AiState& aiState = player.aiState();
      if (!aiState.completedMissions.contains(mission))
        aiState.availableMissions.add(mission);
    return {};
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback("completeMission", [&player](String const& mission) -> std::expected<void, StarException> {
    AiState& aiState = player.aiState();
      aiState.availableMissions.remove(mission);
      aiState.completedMissions.add(mission);
    return {};
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback("hasCompletedMission", [&player](String const& mission) -> std::expected<bool, StarException> {
    return player.aiState().completedMissions.contains(mission);
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback("radioMessage", [&player](Json const& messageConfig, std::optional<float> const& delay) -> std::expected<void, StarException> {
    return player.queueRadioMessage(messageConfig, delay.value_or(0));
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback("interruptRadioMessage", [&player]() {
    return player.interruptRadioMessage();
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback("pullPendingRadioMessage", [&player]() -> std::expected<std::optional<Json>, StarException> {
    auto pending = player.pullPendingRadioMessage();
    if (!pending) return std::unexpected(std::move(pending.error()));
    if (!*pending) return std::optional<Json>{};
    auto const& message = **pending;
    return std::optional<Json>{JsonObject{
        {"messageId", message.messageId}, {"text", message.text},
        {"senderName", message.senderName}, {"portraitImage", message.portraitImage},
        {"portraitFrames", message.portraitFrames}, {"portraitSpeed", message.portraitSpeed},
        {"textSpeed", message.textSpeed}, {"persistTime", message.persistTime},
        {"chatterSound", message.chatterSound}}};
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback("worldId", [&player]() -> std::expected<String, StarException> {
    return printWorldId(player.clientContext()->playerWorldId());
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback("serverUuid", [&player]() -> std::expected<String, StarException> {
    return player.clientContext()->serverUuid().hex();
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback("ownShipWorldId", [&player]() -> std::expected<String, StarException> {
    auto uuid = player.uuid();
    if (!uuid) return std::unexpected(std::move(uuid.error()));
    return printWorldId(ClientShipWorldId(*uuid));
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback("lounge", [&player](EntityId entityId, std::optional<std::size_t> anchorIndex) -> std::expected<bool, StarException> {
    return player.lounge(entityId, anchorIndex.value_or(0));
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback("isLounging", [&player]() -> std::expected<bool, StarException> {
    return (bool)player.loungingIn();
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback("loungingIn", [&player]() -> std::expected<std::optional<EntityId>, StarException> {
    if (auto anchorState = player.loungingIn())
        return anchorState->entityId;
      return {};
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback("stopLounging", [&player]() -> std::expected<void, StarException> {
    return player.stopLounging();
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback("playTime", [&player]() -> std::expected<double, StarException> {
    return player.log()->playTime();
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback("introComplete", [&player]() -> std::expected<bool, StarException> {
    return player.log()->introComplete();
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback("setIntroComplete", [&player](bool complete) -> std::expected<void, StarException> {
    player.log()->setIntroComplete(complete);
    return {};
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback("warp", [&player](String action, std::optional<String> animation, std::optional<bool> deploy) -> std::expected<void, StarException> {
    player.setPendingWarp(action, animation, deploy.value_or(false));
    return {};
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback("canDeploy", [&player]() -> std::expected<bool, StarException> {
    return player.canDeploy();
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback("isDeployed", [&player]() -> std::expected<bool, StarException> {
    return player.isDeployed();
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback("confirm", [&player](Json dialogConfig) -> std::expected<RpcPromise<Json>, StarException> {
    auto pair = RpcPromise<Json>::createPair();
      player.queueConfirmation(dialogConfig, pair.second);
      return pair.first;
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback("playCinematic", [&player](Json const& cinematic, std::optional<bool> unique) -> std::expected<void, StarException> {
    return player.setPendingCinematic(cinematic, unique.value_or(false));
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback("recordEvent", [&player](String const& eventName, Json const& fields) -> std::expected<void, StarException> {
    player.statistics()->recordEvent(eventName, fields);
    return {};
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback("worldHasOrbitBookmark", [&player](Json const& coords) -> std::expected<bool, StarException> {
    auto coordinate = CelestialCoordinate::create(coords);
    if (!coordinate) return std::unexpected(std::move(coordinate.error()));
    auto bookmark = player.universeMap()->worldBookmark(*coordinate);
    if (!bookmark) return std::unexpected(std::move(bookmark.error()));
    return bookmark->has_value();
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback("orbitBookmarks", [&player]() -> std::expected<List<std::pair<Vec3I, Json>>, StarException> {
    auto bookmarks = player.universeMap()->orbitBookmarks();
    if (!bookmarks) return std::unexpected(std::move(bookmarks.error()));
    List<std::pair<Vec3I, Json>> result;
    for (auto const& bookmark : *bookmarks) {
      auto json = bookmark.second.toJson();
    if (!json) return std::unexpected(std::move(json.error()));
    result.append({bookmark.first, std::move(*json)});
    }
    return result;
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback("systemBookmarks", [&player](Json const& coords) -> std::expected<List<Json>, StarException> {
    auto coordinate = CelestialCoordinate::create(coords);
    if (!coordinate) return std::unexpected(std::move(coordinate.error()));
    auto bookmarks = player.universeMap()->systemBookmarks(*coordinate);
    if (!bookmarks) return std::unexpected(std::move(bookmarks.error()));
    List<Json> result;
    for (auto const& bookmark : *bookmarks) {
      auto json = bookmark.toJson();
    if (!json) return std::unexpected(std::move(json.error()));
    result.append(std::move(*json));
    }
    return result;
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback("addOrbitBookmark", [&player](Json const& system, Json const& bookmarkConfig) -> std::expected<bool, StarException> {
    auto coordinate = CelestialCoordinate::create(system);
    if (!coordinate) return std::unexpected(std::move(coordinate.error()));
    auto bookmark = OrbitBookmark::fromJson(bookmarkConfig);
    if (!bookmark) return std::unexpected(std::move(bookmark.error()));
    return player.universeMap()->addOrbitBookmark(*coordinate, *bookmark);
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback("removeOrbitBookmark", [&player](Json const& system, Json const& bookmarkConfig) -> std::expected<bool, StarException> {
    auto coordinate = CelestialCoordinate::create(system);
    if (!coordinate) return std::unexpected(std::move(coordinate.error()));
    auto bookmark = OrbitBookmark::fromJson(bookmarkConfig);
    if (!bookmark) return std::unexpected(std::move(bookmark.error()));
    return player.universeMap()->removeOrbitBookmark(*coordinate, *bookmark);
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback("teleportBookmarks", [&player]() -> std::expected<List<Json>, StarException> {
    auto bookmarks = player.universeMap()->teleportBookmarks();
    if (!bookmarks) return std::unexpected(std::move(bookmarks.error()));
    List<Json> result;
    for (auto const& bookmark : *bookmarks) {
      auto json = bookmark.toJson();
    if (!json) return std::unexpected(std::move(json.error()));
    result.append(std::move(*json));
    }
    return result;
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback("addTeleportBookmark", [&player](Json const& bookmarkConfig) -> std::expected<bool, StarException> {
    auto bookmark = TeleportBookmark::fromJson(bookmarkConfig);
    if (!bookmark) return std::unexpected(std::move(bookmark.error()));
    return player.universeMap()->addTeleportBookmark(*bookmark);
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback("removeTeleportBookmark", [&player](Json const& bookmarkConfig) -> std::expected<bool, StarException> {
    auto bookmark = TeleportBookmark::fromJson(bookmarkConfig);
    if (!bookmark) return std::unexpected(std::move(bookmark.error()));
    return player.universeMap()->removeTeleportBookmark(*bookmark);
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback("isMapped", [&player](Json const& coords) -> std::expected<bool, StarException> {
    auto coordinate = CelestialCoordinate::create(coords);
    if (!coordinate) return std::unexpected(std::move(coordinate.error()));
    return player.universeMap()->isMapped(*coordinate);
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback("mappedObjects", [&player](Json const& coords) -> std::expected<Json, StarException> {
    auto coordinate = CelestialCoordinate::create(coords);
    if (!coordinate) return std::unexpected(std::move(coordinate.error()));
    auto objects = player.universeMap()->mappedObjects(*coordinate);
    if (!objects) return std::unexpected(std::move(objects.error()));
    JsonObject json;
    for (auto const& [uuid, mapped] : *objects) {
      Json orbit;
      if (mapped.orbit)
        orbit = mapped.orbit->toJson();
      json.set(uuid.hex(), JsonObject{{"typeName", mapped.typeName}, {"orbit", std::move(orbit)}, {"parameters", mapped.parameters}});
    }
    return json;
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback("collectables", [&player](String const& collection) -> std::expected<StringSet, StarException> {
    return player.log()->collectables(collection);
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback("getProperty", [&player](String const& name, std::optional<Json> const& defaultValue) -> std::expected<Json, StarException> {
    return player.getGenericProperty(name, defaultValue.value_or(Json()));
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback("setProperty", [&player](String const& name, Json const& value) -> std::expected<void, StarException> {
    player.setGenericProperty(name, value);
    return {};
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback("addScannedObject", [&player](String const& objectName) -> std::expected<bool, StarException> {
    return player.log()->addScannedObject(objectName);
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback("removeScannedObject", [&player](String const& objectName) -> std::expected<void, StarException> {
    player.log()->removeScannedObject(objectName);
    return {};
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  // codex bindings
  if (auto registered = callbacks.registerCallback("isCodexKnown", [&player](String const& codexId) -> std::expected<bool, StarException> {
    return player.codexes()->codexKnown(codexId);
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback("isCodexRead", [&player](String const& codexId) -> std::expected<bool, StarException> {
    return player.codexes()->codexRead(codexId);
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback("markCodexRead", [&player](String const& codexId) -> std::expected<bool, StarException> {
    return player.codexes()->markCodexRead(codexId);
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback("markCodexUnread", [&player](String const& codexId) -> std::expected<bool, StarException> {
    return player.codexes()->markCodexUnread(codexId);
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback("learnCodex", [&player](String const& codexId, std::optional<bool> markRead) -> std::expected<void, StarException> {
    auto learned = player.codexes()->learnCodex(codexId, markRead.value_or(false));
    if (!learned) return std::unexpected(std::move(learned.error()));
    return {};
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback("getCodexes", [&player]() -> std::expected<Json, StarException> {
    return player.codexes()->toJson();
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback("getCodex", [&player](String const& id) -> std::expected<Json, StarException> {
    auto codex = player.codexes()->codex(id);
    if (!codex) return Json();
    JsonArray pages;
    auto count = codex->pageCount();
    pages.reserve(count);
    for (std::size_t index = 0; index < count; ++index)
      pages.append(codex->page(index));
    return JsonObject{{"id", codex->id()}, {"species", codex->species()},
        {"title", codex->title()}, {"description", codex->description()},
        {"icon", codex->icon()}, {"contentPages", std::move(pages)}};
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback("getCodexList", [&player]() -> std::expected<JsonArray, StarException> {
    JsonArray result;
    auto entries = player.codexes()->codexes();
    result.reserve(entries.size());
    for (auto const& entry : entries)
      result.append(JsonObject{{"id", entry.first->id()}, {"read", entry.second}});
    return result;
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback("getNewCodexId", [&player]() -> std::expected<std::optional<String>, StarException> {
    if (auto codex = player.codexes()->firstNewCodex())
      return codex->id();
    return {};
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback("getNewCodex", [&player]() -> std::expected<std::optional<String>, StarException> {
    auto codexPtr = player.codexes()->firstNewCodex();
    if (codexPtr)
      return codexPtr->title();

    return {};
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback("setAnimationParameter", [&player](String name, Json value) -> std::expected<void, StarException> {
    return player.setAnimationParameter(name, value);
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback("setCameraFocusEntity", [&player](std::optional<EntityId> const& entityId) -> std::expected<void, StarException> {
    player.setCameraFocusEntity(entityId);
    return {};
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  if (auto registered = callbacks.registerCallback("headRotation", [&player]() -> std::expected<Json, StarException> {
    return player.getSecretProperty("humanoid.headRotation");
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  // more accurate than mcontroller.facingDirection
  if (auto registered = callbacks.registerCallback("facingDirection", [&player]() -> std::expected<int, StarException> {
    return numericalDirection(player.humanoid()->facingDirection());
  }); !registered)
    return std::unexpected(std::move(registered.error()));

  return callbacks;
}

}

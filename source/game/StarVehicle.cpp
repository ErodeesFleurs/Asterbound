import star.json;
import star.json_extra;
import star.id_map;
import star.casting;
import star.strong_typedef;
import star.rpc_promise;
import star.lua;
import star.random;
import star.periodic_function;
import star.ordered_map;
import star.matrix3;
import star.rect;
import star.io_device;
import star.ref_ptr;
import star.logging;
import star.thread;
import star.listener;
import std;
import star.format;
import star.data_stream_extra;
import star.audio;
import star.list;
import star.map;
import star.set;
import star.vector;
import star.bi_map;
import star.color;
import star.string;
import star.data_stream;
import star.poly;
import star.image_processing;
import star.hash;
import star.string_view;
import star.directives;
import star.asset_path;
import star.game_types;
import star.message_types;
import star.a_star;
import star.ordered_set;

import star.damage_types;
import star.world_geometry;
import star.status_types;
import star.damage;
import star.entity;
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
import star.physics_entity;
import star.mobile_entity;
import star.animated_part_set;
import star.networked_animator;
import star.movement_controller;
import star.lua_components;
import star.anchorable_entity;
import star.lounging_entities;
import star.scripted_entity;
import star.lua_animation_component;
import star.vehicle;
import star.asset_source;
import star.assets;
import star.root_base;
import star.configuration;
import star.root;
import star.lua_game_converters;
import star.mixer;
import star.drawable;
import star.entity_rendering_types;
import star.animation;
import star.particle;

import star.light_source;
import star.entity_rendering;
import star.uuid;
import star.humanoid;
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
import star.platformer_astar_types;
import star.game_timers;
import star.actor_movement_controller;
import star.ai_types;
import star.player_types;
import star.lua_actor_movement_component;
import star.radio_message_database;
import star.player;

import star.networked_animator_lua_bindings;
import star.entity_lua_bindings;
import star.scripted_animator_lua_bindings;
import star.config_lua_bindings;
import star.movement_controller_lua_bindings;

import star.net_compatibility;
import star.byte_array;
import star.math_common;
import star.algorithm;
namespace Star {

Vehicle::Vehicle(Root& rootContext) : Entity(rootContext), m_scriptComponent(rootContext), m_scriptedAnimator(rootContext) {}


std::expected<std::shared_ptr<Vehicle>, StarException> Vehicle::create(Root& rootContext, Json baseConfig, String path, Json dynamicConfig) {
  struct MakeShared final : Vehicle {
    explicit MakeShared(Root& root) : Entity(root), Vehicle(root) {}
  };
  auto entity = std::make_shared<MakeShared>(rootContext);

  if (auto result = entity->initialize(rootContext, std::move(baseConfig), std::move(path), std::move(dynamicConfig)); !result) return std::unexpected(std::move(result.error()));
  return entity;
}

std::expected<void, StarException> Vehicle::initialize(Root& rootContext, Json baseConfig, String path, Json dynamicConfig) {
  m_baseConfig = std::move(baseConfig);
  m_path = std::move(path);
  m_dynamicConfig = std::move(dynamicConfig);
  auto result1006 = MovementController::create(rootContext);
if (!result1006) return std::unexpected(std::move(result1006.error()));
m_movement_controller = std::move(*result1006);

  auto result1007 = m_baseConfig.get_string("name");
if (!result1007) return std::unexpected(std::move(result1007.error()));
m_typeName = std::move(*result1007);

  auto result1008 = configValue("persistent", false);
if (!result1008) return std::unexpected(std::move(result1008.error()));
auto result1009 = std::move(*result1008).to_bool();
if (!result1009) return std::unexpected(std::move(result1009.error()));
setPersistent(std::move(*result1009));
  auto result1010 = configValue("clientEntityMode", "ClientSlaveOnly");
if (!result1010) return std::unexpected(std::move(result1010.error()));
auto result1011 = std::move(*result1010).to_string();
if (!result1011) return std::unexpected(std::move(result1011.error()));
if (!ClientEntityModeNames) return std::unexpected(ClientEntityModeNames.error());
auto result1012 = ClientEntityModeNames->get_left(std::move(*result1011));
if (!result1012) return std::unexpected(std::move(result1012.error()));
m_client_entity_mode = result1012->get();

  auto result1013 = configValue("script");
if (!result1013) return std::unexpected(std::move(result1013.error()));
auto result1014 = std::move(*result1013).to_string();
if (!result1014) return std::unexpected(std::move(result1014.error()));
auto result1015 = m_scriptComponent.set_script(AssetPath::relative_to(m_path, std::move(*result1014)));
if (!result1015) return std::unexpected(std::move(result1015.error()));
  auto result1016 = configValue("scriptDelta", 1);
if (!result1016) return std::unexpected(std::move(result1016.error()));
auto result1017 = std::move(*result1016).to_uint();
if (!result1017) return std::unexpected(std::move(result1017.error()));
m_scriptComponent.set_update_delta(std::move(*result1017));
  auto result1018 = configValue("boundBox");
if (!result1018) return std::unexpected(std::move(result1018.error()));
auto result1019 = json_to_rectf(std::move(*result1018));
if (!result1019) return std::unexpected(std::move(result1019.error()));
m_boundBox = std::move(*result1019);
  auto result1020 = configValue("slaveControlTimeout");
if (!result1020) return std::unexpected(std::move(result1020.error()));
auto result1021 = std::move(*result1020).to_float();
if (!result1021) return std::unexpected(std::move(result1021.error()));
m_slave_control_timeout = std::move(*result1021);
  auto result1022 = configValue("receiveExtraControls", false);
if (!result1022) return std::unexpected(std::move(result1022.error()));
auto result1023 = std::move(*result1022).to_bool();
if (!result1023) return std::unexpected(std::move(result1023.error()));
m_receiveExtraControls = std::move(*result1023);
  auto result1024 = configValue("slaveControlHeartbeat");
if (!result1024) return std::unexpected(std::move(result1024.error()));
auto result1025 = std::move(*result1024).to_float();
if (!result1025) return std::unexpected(std::move(result1025.error()));
m_slave_heartbeat_timer = GameTimer(std::move(*result1025));
  auto result1026 = configValue("damageTeam");
if (!result1026) return std::unexpected(std::move(result1026.error()));
auto teamConfig = std::move(*result1026);
  if (!teamConfig.is_null()) {
auto result1027 = EntityDamageTeam::create(teamConfig);
if (!result1027) return std::unexpected(std::move(result1027.error()));
m_damageTeam.set(std::move(*result1027));
}
  else m_damageTeam.set(EntityDamageTeam{});
  auto result1028 = configValue("interactive", true);
if (!result1028) return std::unexpected(std::move(result1028.error()));
auto result1029 = std::move(*result1028).to_bool();
if (!result1029) return std::unexpected(std::move(result1029.error()));
m_interactive.set(std::move(*result1029));
  auto result1030 = configValue("baseRenderLayer","Vehicle");
if (!result1030) return std::unexpected(std::move(result1030.error()));
auto result1031 = std::move(*result1030).to_string();
if (!result1031) return std::unexpected(std::move(result1031.error()));
auto result1032 = parse_render_layer(*result1031);
if (!result1032) return std::unexpected(std::move(result1032.error()));
m_base_render_layer = std::move(*result1032);
  auto result1033 = configValue("overrideRenderLayer");
if (!result1033) return std::unexpected(std::move(result1033.error()));
if (!std::move(*result1033).is_null()) {
    auto result1034 = configValue("overrideRenderLayer");
if (!result1034) return std::unexpected(std::move(result1034.error()));
auto result1035 = std::move(*result1034).to_string();
if (!result1035) return std::unexpected(std::move(result1035.error()));
auto result1036 = parse_render_layer(*result1035);
if (!result1036) return std::unexpected(std::move(result1036.error()));
m_override_render_layer = std::move(*result1036);
  }

  auto result1037 = configValue("animationScript");
if (!result1037) return std::unexpected(std::move(result1037.error()));
auto result1038 = std::move(*result1037).opt_string();
if (!result1038) return std::unexpected(std::move(result1038.error()));
if (auto animationScript = std::move(*result1038))
    {
auto result1039 = m_scriptedAnimator.set_script(*animationScript);
if (!result1039) return std::unexpected(std::move(result1039.error()));
}

  auto result1040 = configValue("loungePositions", JsonObject());
if (!result1040) return std::unexpected(std::move(result1040.error()));
auto result1041 = std::move(*result1040).iterate_object();
if (!result1041) return std::unexpected(std::move(result1041.error()));
for (auto const& pair : std::move(*result1041)) {
    auto& loungePosition = m_loungePositions[pair.first];
    auto result1042 = pair.second.get_string("part");
if (!result1042) return std::unexpected(std::move(result1042.error()));
loungePosition.part = std::move(*result1042);
    auto result1043 = pair.second.get_string("partAnchor");
if (!result1043) return std::unexpected(std::move(result1043.error()));
loungePosition.partAnchor = std::move(*result1043);
    auto result1044 = pair.second.get("exitBottomOffset",Json());
if (!result1044) return std::unexpected(std::move(result1044.error()));
auto result1045 = json_to_optional<Vec2F>(std::move(*result1044),json_to_vec2f);
if (!result1045) return std::unexpected(std::move(result1045.error()));
loungePosition.exitBottomOffset = std::move(*result1045);
    auto result1046 = pair.second.get_object("armorCosmeticOverrides", JsonObject());
if (!result1046) return std::unexpected(std::move(result1046.error()));
loungePosition.armor_cosmetic_overrides = std::move(*result1046);
    auto result1047 = pair.second.opt_string("cursorOverride");
if (!result1047) return std::unexpected(std::move(result1047.error()));
loungePosition.cursorOverride = std::move(*result1047);
    auto result1048 = pair.second.get_bool("cameraFocus", false);
if (!result1048) return std::unexpected(std::move(result1048.error()));
loungePosition.cameraFocus = std::move(*result1048);
    auto result1049 = pair.second.get_bool("enabled", true);
if (!result1049) return std::unexpected(std::move(result1049.error()));
loungePosition.enabled.set(std::move(*result1049));
    auto result1050 = pair.second.opt_string("orientation");
if (!result1050) return std::unexpected(std::move(result1050.error()));
if (auto orientation = std::move(*result1050))
      {
if (!LoungeOrientationNames) return std::unexpected(LoungeOrientationNames.error());
auto result1051 = LoungeOrientationNames->get_left(*orientation);
if (!result1051) return std::unexpected(std::move(result1051.error()));
loungePosition.orientation.set(result1051->get());
}
    auto result1052 = pair.second.opt_string("emote");
if (!result1052) return std::unexpected(std::move(result1052.error()));
loungePosition.emote.set(std::move(*result1052));
    auto result1053 = pair.second.opt_string("dance");
if (!result1053) return std::unexpected(std::move(result1053.error()));
loungePosition.dance.set(std::move(*result1053));
    auto result1054 = pair.second.opt_string("directives");
if (!result1054) return std::unexpected(std::move(result1054.error()));
loungePosition.directives.set(std::move(*result1054));
    auto result1055 = pair.second.get("statusEffects", JsonArray{});
if (!result1055) return std::unexpected(std::move(result1055.error()));
auto result1056 = json_to_list<PersistentStatusEffect>(std::move(*result1055),jsonToPersistentStatusEffect);
if (!result1056) return std::unexpected(std::move(result1056.error()));
loungePosition.status_effects.set(std::move(*result1056));
    auto result1057 = pair.second.opt_bool("suppressTools");
if (!result1057) return std::unexpected(std::move(result1057.error()));
loungePosition.suppressTools = std::move(*result1057);
  }

  auto result1058 = configValue("physicsCollisions", JsonObject());
if (!result1058) return std::unexpected(std::move(result1058.error()));
auto result1059 = std::move(*result1058).iterate_object();
if (!result1059) return std::unexpected(std::move(result1059.error()));
for (auto const& pair : std::move(*result1059)) {
    auto& collisionConfig = m_movingCollisions[pair.first];
    auto result1060 = PhysicsMovingCollision::fromJson(pair.second);
if (!result1060) return std::unexpected(std::move(result1060.error()));
collisionConfig.movingCollision = std::move(*result1060);
    auto result1061 = pair.second.opt_string("attachToPart");
if (!result1061) return std::unexpected(std::move(result1061.error()));
collisionConfig.attachToPart = std::move(*result1061);
    auto result1062 = pair.second.get_bool("enabled", true);
if (!result1062) return std::unexpected(std::move(result1062.error()));
collisionConfig.enabled.set(std::move(*result1062));
  }

  auto result1063 = configValue("physicsForces", JsonObject());
if (!result1063) return std::unexpected(std::move(result1063.error()));
auto result1064 = std::move(*result1063).iterate_object();
if (!result1064) return std::unexpected(std::move(result1064.error()));
for (auto const& pair : std::move(*result1064)) {
    auto& force_region_config = m_force_regions[pair.first];
    auto result1065 = json_to_physics_force_region(pair.second);
if (!result1065) return std::unexpected(std::move(result1065.error()));
force_region_config.force_region = std::move(*result1065);
    auto result1066 = pair.second.opt_string("attachToPart");
if (!result1066) return std::unexpected(std::move(result1066.error()));
force_region_config.attach_to_part = std::move(*result1066);
    auto result1067 = pair.second.get_bool("enabled", true);
if (!result1067) return std::unexpected(std::move(result1067.error()));
force_region_config.enabled.set(std::move(*result1067));
  }

  auto result1068 = configValue("damageSources", JsonObject());
if (!result1068) return std::unexpected(std::move(result1068.error()));
auto result1069 = std::move(*result1068).iterate_object();
if (!result1069) return std::unexpected(std::move(result1069.error()));
for (auto const& pair : std::move(*result1069)) {
    auto& damageSourceConfig = m_damageSources[pair.first];
    auto result1070 = DamageSource::create(pair.second);
if (!result1070) return std::unexpected(std::move(result1070.error()));
damageSourceConfig.damageSource = std::move(*result1070);
    auto result1071 = pair.second.opt_string("attachToPart");
if (!result1071) return std::unexpected(std::move(result1071.error()));
damageSourceConfig.attachToPart = std::move(*result1071);
    auto result1072 = pair.second.get_bool("enabled", true);
if (!result1072) return std::unexpected(std::move(result1072.error()));
damageSourceConfig.enabled.set(std::move(*result1072));
  }

  auto result1073 = this->root().assets();
if (!result1073) return std::unexpected(std::move(result1073.error()));
auto assets = std::move(*result1073);
  auto result1074 = configValue("animation");
if (!result1074) return std::unexpected(std::move(result1074.error()));
auto result1075 = assets->fetch_json(std::move(*result1074), m_path);
if (!result1075) return std::unexpected(std::move(result1075.error()));
auto animationConfig = std::move(*result1075);
  auto result1076 = configValue("animationCustom");
if (!result1076) return std::unexpected(std::move(result1076.error()));
if (auto customConfig = std::move(*result1076))
    animationConfig = json_merge(animationConfig, customConfig);

  auto result1078 = NetworkedAnimator::create(this->root(), animationConfig, m_path);
if (!result1078) return std::unexpected(std::move(result1078.error()));
m_networkedAnimator = std::move(*result1078);

  auto result1079 = configValue("animationGlobalTags", JsonObject());
if (!result1079) return std::unexpected(std::move(result1079.error()));
auto result1080 = std::move(*result1079).iterate_object();
if (!result1080) return std::unexpected(std::move(result1080.error()));
for (auto const& p : std::move(*result1080))
    {
auto result1081 = p.second.to_string();
if (!result1081) return std::unexpected(std::move(result1081.error()));
m_networkedAnimator->set_global_tag(p.first, std::move(*result1081));
}
  auto result1082 = configValue("animationPartTags", JsonObject());
if (!result1082) return std::unexpected(std::move(result1082.error()));
auto result1083 = std::move(*result1082).iterate_object();
if (!result1083) return std::unexpected(std::move(result1083.error()));
for (auto const& partPair : std::move(*result1083)) {
  auto result1084 = partPair.second.iterate_object();
  if (!result1084)
    return std::unexpected(std::move(result1084.error()));
  for (auto const& tagPair : std::move(*result1084)) {
    auto result1085 = tagPair.second.to_string();
    if (!result1085)
      return std::unexpected(std::move(result1085.error()));
    m_networkedAnimator->set_part_tag(partPair.first, tagPair.first, std::move(*result1085));
  }
  }

  auto result1086 = configValue("movementSettings");
if (!result1086) return std::unexpected(std::move(result1086.error()));
auto result1087 = MovementParameters::create(std::move(*result1086));
if (!result1087) return std::unexpected(std::move(result1087.error()));
auto movement_parameters = std::move(*result1087);
  if (!movement_parameters.physics_effect_categories)
    movement_parameters.physics_effect_categories = StringSet({"vehicle"});
  auto result1088 = m_movement_controller->reset_parameters(movement_parameters);
if (!result1088) return std::unexpected(std::move(result1088.error()));

  auto result1089 = m_netGroup.add_net_element(m_interactive);
if (!result1089) return std::unexpected(std::move(result1089.error()));
  auto result1090 = m_netGroup.add_net_element(*m_movement_controller);
if (!result1090) return std::unexpected(std::move(result1090.error()));
  auto result1091 = m_netGroup.add_net_element(*m_networkedAnimator);
if (!result1091) return std::unexpected(std::move(result1091.error()));
  auto result1092 = m_netGroup.add_net_element(m_damageTeam);
if (!result1092) return std::unexpected(std::move(result1092.error()));

m_loungePositions.sort_by_key();
for (auto& p : m_loungePositions) {
  auto result1093 = m_netGroup.add_net_element(p.second.enabled);
  if (!result1093)
    return std::unexpected(std::move(result1093.error()));
  auto result1094 = m_netGroup.add_net_element(p.second.orientation);
  if (!result1094)
    return std::unexpected(std::move(result1094.error()));
  auto result1095 = m_netGroup.add_net_element(p.second.emote);
  if (!result1095)
    return std::unexpected(std::move(result1095.error()));
  auto result1096 = m_netGroup.add_net_element(p.second.dance);
  if (!result1096)
    return std::unexpected(std::move(result1096.error()));
  auto result1097 = m_netGroup.add_net_element(p.second.directives);
  if (!result1097)
    return std::unexpected(std::move(result1097.error()));
  auto result1098 = m_netGroup.add_net_element(p.second.status_effects);
  if (!result1098)
    return std::unexpected(std::move(result1098.error()));
}

m_movingCollisions.sort_by_key();
for (auto& p : m_movingCollisions) {
  auto result1099 = m_netGroup.add_net_element(p.second.enabled);
  if (!result1099)
    return std::unexpected(std::move(result1099.error()));
}

m_force_regions.sort_by_key();
for (auto& p : m_force_regions) {
  auto result1100 = m_netGroup.add_net_element(p.second.enabled);
  if (!result1100)
    return std::unexpected(std::move(result1100.error()));
}

m_damageSources.sort_by_key();
for (auto& p : m_damageSources) {
  auto result1101 = m_netGroup.add_net_element(p.second.enabled);
  if (!result1101)
    return std::unexpected(std::move(result1101.error()));
}

  // don't interpolate scripted animation parameters
  auto result1102 = m_netGroup.add_net_element(m_scriptedAnimationParameters, false);
if (!result1102) return std::unexpected(std::move(result1102.error()));
  return {};
}

std::expected<String, StarException> Vehicle::name() const {
  return m_typeName;
}

Json Vehicle::baseConfig() const {
  return m_baseConfig;
}

Json Vehicle::dynamicConfig() const {
  return m_dynamicConfig;
}

std::expected<Json, StarException> Vehicle::diskStore() const {
  auto result1273 = m_damageTeam.get().to_json();
if (!result1273) return std::unexpected(std::move(result1273.error()));
auto result1274 = m_scriptComponent.get_script_storage();
if (!result1274) return std::unexpected(std::move(result1274.error()));
return JsonObject{
    {"movement", m_movement_controller->store_state()},
    {"damageTeam", std::move(*result1273)},
    {"persistent", persistent()},
    {"scriptStorage", std::move(*result1274)}
  };
}

std::expected<void, StarException> Vehicle::diskLoad(Json diskStore) {
  auto result1266 = diskStore.get("movement");
if (!result1266) return std::unexpected(std::move(result1266.error()));
auto result1267 = m_movement_controller->load_state(std::move(*result1266));
if (!result1267) return std::unexpected(std::move(result1267.error()));
  auto result1268 = diskStore.get("damageTeam");
if (!result1268) return std::unexpected(std::move(result1268.error()));
auto result1269 = EntityDamageTeam::create(std::move(*result1268));
if (!result1269) return std::unexpected(std::move(result1269.error()));
m_damageTeam.set(std::move(*result1269));
  auto result1270 = diskStore.get_bool("persistent");
if (!result1270) return std::unexpected(std::move(result1270.error()));
setPersistent(std::move(*result1270));
  auto result1271 = diskStore.get_object("scriptStorage");
if (!result1271) return std::unexpected(std::move(result1271.error()));
auto result1272 = m_scriptComponent.set_script_storage(std::move(*result1271));
if (!result1272) return std::unexpected(std::move(result1272.error()));
  return {};
}

EntityType Vehicle::entity_type() const {
  return EntityType::Vehicle;
}

ClientEntityMode Vehicle::client_entity_mode() const {
  return m_client_entity_mode;
}

std::expected<std::optional<HitType>, StarException> Vehicle::query_hit(DamageSource const& source) const {
  auto result1264 = world();
if (!result1264) return std::unexpected(std::move(result1264.error()));
auto result1265 = source.intersects_with_poly(result1264->get().geometry(), m_movement_controller->collision_body());
if (!result1265) return std::unexpected(std::move(result1265.error()));
if (std::move(*result1265))
    return HitType::Hit;

  return {};
}

std::expected<std::optional<PolyF>, StarException> Vehicle::hit_poly() const {
  return m_movement_controller->collision_body();
}

std::expected<List<DamageNotification>, StarException> Vehicle::apply_damage(DamageRequest const& damage) {
  if (!inWorld())
    return {};

  auto result1262 = recover_lua_script_error(m_scriptComponent.invoke<List<DamageNotification>>("applyDamage", damage));
if (!result1262) return std::unexpected(std::move(result1262.error()));
  return std::move(*result1262).value_or(List<DamageNotification>{});
}

std::expected<List<DamageNotification>, StarException> Vehicle::self_damage_notifications() {
  auto result1260 = recover_lua_script_error(m_scriptComponent.invoke<List<DamageNotification>>("selfDamageNotifications"));
if (!result1260) return std::unexpected(std::move(result1260.error()));
  return std::move(*result1260).value_or(List<DamageNotification>{});
}

std::expected<void, StarException> Vehicle::init(World& world, EntityId entityId, EntityMode mode) {
  auto result1241 = Entity::init(world, entityId, mode);
if (!result1241) return std::unexpected(std::move(result1241.error()));
  m_movement_controller->init(world);
  m_movement_controller->set_ignore_physics_entities({entityId});
  if (isMaster()) {
    auto result1242 = makeVehicleCallbacks();
if (!result1242) return std::unexpected(std::move(result1242.error()));
auto result1243 = m_scriptComponent.add_callbacks("vehicle", std::move(*result1242));
if (!result1243) return std::unexpected(std::move(result1243.error()));
auto result1244 =
  LuaBindings::makeConfigCallbacks([this](String const& name, Json const& def) { return configValue(name, def); });
if (!result1244) return std::unexpected(std::move(result1244.error()));
auto result1245 = m_scriptComponent.add_callbacks(
        "config", std::move(*result1244));
if (!result1245) return std::unexpected(std::move(result1245.error()));
    auto result1246 = LuaBindings::makeEntityCallbacks(*this);
if (!result1246) return std::unexpected(std::move(result1246.error()));
auto result1247 = m_scriptComponent.add_callbacks("entity", std::move(*result1246));
if (!result1247) return std::unexpected(std::move(result1247.error()));
    auto result1248 = LuaBindings::make_movement_controller_callbacks(*m_movement_controller);
if (!result1248) return std::unexpected(std::move(result1248.error()));
auto result1249 = m_scriptComponent.add_callbacks("mcontroller", std::move(*result1248));
if (!result1249) return std::unexpected(std::move(result1249.error()));
    auto result1250 = LuaBindings::make_networked_animator_callbacks(*m_networkedAnimator);
if (!result1250) return std::unexpected(std::move(result1250.error()));
auto result1251 = m_scriptComponent.add_callbacks("animator", std::move(*result1250));
if (!result1251) return std::unexpected(std::move(result1251.error()));
    auto result1252 = recover_lua_script_error(m_scriptComponent.init(world));
if (!result1252) return std::unexpected(std::move(result1252.error()));
  } else {
    m_slave_heartbeat_timer.reset();
  }

  if (world.isClient()) {
    auto result1253 = LuaBindings::make_scripted_animator_callbacks(*m_networkedAnimator,
      [this](String const& name, Json const& defaultValue) -> std::expected<Json, StarException> {
        return m_scriptedAnimationParameters.value(name, defaultValue);
      });
if (!result1253) return std::unexpected(std::move(result1253.error()));
auto result1254 = m_scriptedAnimator.add_callbacks("animationConfig", std::move(*result1253));
if (!result1254) return std::unexpected(std::move(result1254.error()));
    auto result1255 = LuaBindings::makeConfigCallbacks([this](String const& name, Json const& def) {
        return configValue(name, def);
      });
if (!result1255) return std::unexpected(std::move(result1255.error()));
auto result1256 = m_scriptedAnimator.add_callbacks("config", std::move(*result1255));
if (!result1256) return std::unexpected(std::move(result1256.error()));
    auto result1257 = LuaBindings::makeEntityCallbacks(*this);
if (!result1257) return std::unexpected(std::move(result1257.error()));
auto result1258 = m_scriptedAnimator.add_callbacks("entity", std::move(*result1257));
if (!result1258) return std::unexpected(std::move(result1258.error()));

    auto result1259 = recover_lua_script_error(m_scriptedAnimator.init(world));
if (!result1259) return std::unexpected(std::move(result1259.error()));
  }
  return {};
}

std::expected<void, StarException> Vehicle::uninit() {
  auto result1230 = recover_lua_script_error(m_scriptComponent.uninit());
if (!result1230) return std::unexpected(std::move(result1230.error()));
  auto result1231 = m_scriptComponent.remove_callbacks("vehicle");
if (!result1231) return std::unexpected(std::move(result1231.error()));
  auto result1232 = m_scriptComponent.remove_callbacks("config");
if (!result1232) return std::unexpected(std::move(result1232.error()));
  auto result1233 = m_scriptComponent.remove_callbacks("entity");
if (!result1233) return std::unexpected(std::move(result1233.error()));
  auto result1234 = m_scriptComponent.remove_callbacks("mcontroller");
if (!result1234) return std::unexpected(std::move(result1234.error()));
  auto result1235 = m_scriptComponent.remove_callbacks("animator");
if (!result1235) return std::unexpected(std::move(result1235.error()));
  m_movement_controller->uninit();

  auto result1236 = world();
if (!result1236) return std::unexpected(std::move(result1236.error()));
if (result1236->get().isClient()) {
    auto result1237 = m_scriptedAnimator.remove_callbacks("animationConfig");
if (!result1237) return std::unexpected(std::move(result1237.error()));
    auto result1238 = m_scriptedAnimator.remove_callbacks("config");
if (!result1238) return std::unexpected(std::move(result1238.error()));
    auto result1239 = m_scriptedAnimator.remove_callbacks("entity");
if (!result1239) return std::unexpected(std::move(result1239.error()));
  }

  auto result1240 = Entity::uninit();
if (!result1240) return std::unexpected(std::move(result1240.error()));
  return {};
}

Vec2F Vehicle::position() const {
  return m_movement_controller->position();
}

RectF Vehicle::meta_bound_box() const {
  return m_boundBox;
}

RectF Vehicle::collision_area() const {
  return m_movement_controller->collision_poly().bounding_box();
}

Vec2F Vehicle::velocity() const {
  return m_movement_controller->velocity();
}

std::expected<std::pair<ByteArray, std::uint64_t>, StarException> Vehicle::write_net_state(std::uint64_t fromVersion, NetCompatibilityRules rules) {
  auto result1229 = m_netGroup.write_net_state(fromVersion, rules);
if (!result1229) return std::unexpected(std::move(result1229.error()));
return std::move(*result1229);
}

std::expected<void, StarException> Vehicle::read_net_state(ByteArray const& data, float interpolation_time, NetCompatibilityRules rules) {
  return m_netGroup.read_net_state(data, interpolation_time, rules);
}

std::expected<void, StarException> Vehicle::enable_interpolation(float extrapolation_hint) {
  auto result1227 = m_netGroup.enable_net_interpolation(extrapolation_hint);
if (!result1227) return std::unexpected(std::move(result1227.error()));
  return {};
}

std::expected<void, StarException> Vehicle::disable_interpolation() {
  auto result1226 = m_netGroup.disable_net_interpolation();
if (!result1226) return std::unexpected(std::move(result1226.error()));
  return {};
}

std::expected<void, StarException> Vehicle::update(float dt, std::uint64_t) {
  setTeam(m_damageTeam.get());

  auto result1200 = world();
if (!result1200) return std::unexpected(std::move(result1200.error()));
if (result1200->get().isClient()) {
    auto result1201 = m_networkedAnimator->update(dt, std::ref(m_networkedAnimatorDynamicTarget));
if (!result1201) return std::unexpected(std::move(result1201.error()));
    m_networkedAnimatorDynamicTarget.update_position(position());
  } else {
    auto result1203 = m_networkedAnimator->update(dt, std::nullopt);
if (!result1203) return std::unexpected(std::move(result1203.error()));
  }

  if (isMaster()) {
    auto result1204 = m_movement_controller->tick_master(dt);
if (!result1204) return std::unexpected(std::move(result1204.error()));
    auto result1205 = recover_lua_script_error(m_scriptComponent.update(m_scriptComponent.update_dt(dt)));
if (!result1205) return std::unexpected(std::move(result1205.error()));

    Star::erase_if(m_alive_master_connections, [this](decltype(m_alive_master_connections)::value_type& connection) {
      return connection.second.tick(root().timing().time_step);
    });

    for (auto& loungePositionPair : m_loungePositions) {
      for (auto& p : loungePositionPair.second.masterControlState) {
        p.second.masterHeld = false;
        filter(p.second.slavesHeld, [this](ConnectionId id) {
            return m_alive_master_connections.contains(id);
          });
      }
    }
  } else {
    auto result1206 = m_netGroup.tick_net_interpolation(dt);
if (!result1206) return std::unexpected(std::move(result1206.error()));

    auto result1207 = m_movement_controller->tick_slave(dt);
if (!result1207) return std::unexpected(std::move(result1207.error()));

    bool heartbeat = m_slave_heartbeat_timer.wrap_tick(root().timing().time_step);

    for (auto& p : m_loungePositions) {
      if (heartbeat) {
        JsonArray allControlsHeld;
        for (LoungeControl control : p.second.slaveNewControls) {
          if (control > LoungeControl::Special3 && !m_receiveExtraControls)
            continue;
          if (!LoungeControlNames) return std::unexpected(LoungeControlNames.error());
          auto result1208 = LoungeControlNames->get_right(control);
          if (!result1208)
            return std::unexpected(std::move(result1208.error()));
          allControlsHeld.push_back(result1208->get());
        }
        auto result1209 = world();
if (!result1209) return std::unexpected(std::move(result1209.error()));
auto result1210 = loungeIndex(p.first);
if (!result1210) return std::unexpected(std::move(result1210.error()));
auto result1211 = result1209->get().send_entity_message(entityId(), "control_all", {std::move(*result1210), std::move(allControlsHeld)});
if (!result1211) return std::unexpected(std::move(result1211.error()));
      } else {
        for (auto control : p.second.slaveNewControls.difference(p.second.slaveOldControls)) {
          if (control > LoungeControl::Special3 && !m_receiveExtraControls)
            continue;
          auto result1212 = world();
if (!result1212) return std::unexpected(std::move(result1212.error()));
auto result1213 = loungeIndex(p.first);
if (!result1213) return std::unexpected(std::move(result1213.error()));
if (!LoungeControlNames) return std::unexpected(LoungeControlNames.error());
auto result1214 = LoungeControlNames->get_right(control);
if (!result1214) return std::unexpected(std::move(result1214.error()));
auto result1215 = result1212->get().send_entity_message(entityId(), "control_on", {std::move(*result1213), result1214->get()});
if (!result1215) return std::unexpected(std::move(result1215.error()));
        }
        for (auto control : p.second.slaveOldControls.difference(p.second.slaveNewControls)) {
          if (control > LoungeControl::Special3 && !m_receiveExtraControls)
            continue;
          auto result1216 = world();
if (!result1216) return std::unexpected(std::move(result1216.error()));
auto result1217 = loungeIndex(p.first);
if (!result1217) return std::unexpected(std::move(result1217.error()));
if (!LoungeControlNames) return std::unexpected(LoungeControlNames.error());
auto result1218 = LoungeControlNames->get_right(control);
if (!result1218) return std::unexpected(std::move(result1218.error()));
auto result1219 = result1216->get().send_entity_message(entityId(), "control_off", {std::move(*result1217), result1218->get()});
if (!result1219) return std::unexpected(std::move(result1219.error()));
        }
      }

      if (p.second.slaveOldAimPosition != p.second.slaveNewAimPosition)
        {
auto result1220 = world();
if (!result1220) return std::unexpected(std::move(result1220.error()));
auto result1221 = loungeIndex(p.first);
if (!result1221) return std::unexpected(std::move(result1221.error()));
auto result1222 = result1220->get().send_entity_message(entityId(), "aim", {std::move(*result1221), p.second.slaveNewAimPosition[0], p.second.slaveNewAimPosition[1]});
if (!result1222) return std::unexpected(std::move(result1222.error()));
}

p.second.slaveOldControls = std::exchange(p.second.slaveNewControls, {});
p.second.slaveOldAimPosition = p.second.slaveNewAimPosition;
    }
  }

  auto result1223 = world();
if (!result1223) return std::unexpected(std::move(result1223.error()));
if (result1223->get().isClient())
    {
auto result1224 = recover_lua_script_error(m_scriptedAnimator.update());
if (!result1224) return std::unexpected(std::move(result1224.error()));
}

  auto result1225 = world();
  if (!result1225) return std::unexpected(std::move(result1225.error()));
  if (result1225->get().isClient()) {
    this->root().spatialLogger().log_poly("world", m_movement_controller->collision_body(), {255, 255, 0, 255});
  }
  return {};
}

std::uint64_t Vehicle::presentation_epoch() const {
  return Entity::presentation_epoch() + m_movement_controller->presentation_epoch();
}

std::expected<void, StarException> Vehicle::collect_render_events(RenderCallback& renderer) {
  auto&& result1190 = m_networkedAnimatorDynamicTarget.pull_new_audios();
auto result1191 = renderer.add_audios(std::move(result1190));
if (!result1191) return std::unexpected(std::move(result1191.error()));
  auto&& result1192 = m_networkedAnimatorDynamicTarget.pull_new_particles();
auto result1193 = renderer.add_particles(std::move(result1192));
if (!result1193) return std::unexpected(std::move(result1193.error()));
  auto&& result1196 = m_scriptedAnimator.pull_new_audios();
auto result1197 = renderer.add_audios(std::move(result1196));
if (!result1197) return std::unexpected(std::move(result1197.error()));
  auto&& result1198 = m_scriptedAnimator.pull_new_particles();
auto result1199 = renderer.add_particles(std::move(result1198));
if (!result1199) return std::unexpected(std::move(result1199.error()));
  return {};
}

std::expected<void, StarException> Vehicle::render(RenderCallback& renderer) {
  auto sample = m_networkedAnimator->sample(renderer.presentation().interpolation());
  auto result1187 = sample.drawables_with_z_level(position());
if (!result1187) return std::unexpected(std::move(result1187.error()));
for (auto& drawable : std::move(*result1187)) {
    if (drawable.second < 0.0f)
      {
auto result1188 = renderer.add_drawable(std::move(drawable.first), render_layer(VehicleLayer::Back));
if (!result1188) return std::unexpected(std::move(result1188.error()));
}
    else
      {
auto result1189 = renderer.add_drawable(std::move(drawable.first), render_layer(VehicleLayer::Front));
if (!result1189) return std::unexpected(std::move(result1189.error()));
}
  }


  auto&& result1194 = m_scriptedAnimator.drawables();
for (auto const& drawable_pair : result1194)
    {
auto result1195 = renderer.add_drawable(drawable_pair.first, drawable_pair.second.value_or(render_layer(VehicleLayer::Front)));
if (!result1195) return std::unexpected(std::move(result1195.error()));
}
  return {};
}

std::expected<void, StarException> Vehicle::render_light_sources(RenderCallback& renderer) {
  auto sample = m_networkedAnimator->sample(renderer.presentation().interpolation());
  auto lights = sample.light_sources(position());
  if (!lights)
    return std::unexpected(std::move(lights.error()));
  if (auto added = renderer.add_light_sources(*lights); !added)
    return std::unexpected(std::move(added.error()));
  return renderer.add_light_sources(m_scriptedAnimator.light_sources());
}

std::expected<List<LightSource>, StarException> Vehicle::light_sources() const {
  return m_networkedAnimator->light_sources(position());
}

std::expected<bool, StarException> Vehicle::should_destroy() const {
  return m_shouldDestroy;
}

std::expected<void, StarException> Vehicle::destroy(std::optional<std::reference_wrapper<RenderCallback>> render_callback) {
  if (render_callback) {
    auto result1177 = m_networkedAnimator->update(0.0, std::ref(m_networkedAnimatorDynamicTarget));
if (!result1177) return std::unexpected(std::move(result1177.error()));

    auto&& result1178 = m_networkedAnimatorDynamicTarget.pull_new_audios();
auto result1179 = render_callback->get().add_audios(std::move(result1178));
if (!result1179) return std::unexpected(std::move(result1179.error()));
    auto&& result1180 = m_networkedAnimatorDynamicTarget.pull_new_particles();
auto result1181 = render_callback->get().add_particles(std::move(result1180));
if (!result1181) return std::unexpected(std::move(result1181.error()));
  }
  return {};
}

MessageResponseResult Vehicle::receive_message(ConnectionId connection_id, String const& message, JsonArray const& args) {
  m_alive_master_connections[connection_id] = GameTimer(m_slave_control_timeout);
  auto result1145 = message.equals_ignore_case("control_on");
if (!result1145) return std::unexpected(std::move(result1145.error()));
if (std::move(*result1145)) {
    auto result1146 = args.at(0);
if (!result1146) return std::unexpected(std::move(result1146.error()));
auto result1147 = result1146->get().to_uint();
if (!result1147) return std::unexpected(std::move(result1147.error()));
auto result1148 = m_loungePositions.value_at(std::move(*result1147));
if (!result1148) return std::unexpected(std::move(result1148.error()));
auto& loungePosition = result1148->get();
    auto result1149 = args.at(1);
if (!result1149) return std::unexpected(std::move(result1149.error()));
auto result1150 = result1149->get().to_string();
if (!result1150) return std::unexpected(std::move(result1150.error()));
if (!LoungeControlNames) return std::unexpected(LoungeControlNames.error());
auto result1151 = LoungeControlNames->get_left(std::move(*result1150));
if (!result1151) return std::unexpected(std::move(result1151.error()));
loungePosition.masterControlState[result1151->get()].slavesHeld.add(connection_id);
    return ChainableJsonMessageResponse(Json());
  } else {
auto result1152 = message.equals_ignore_case("control_off");
if (!result1152) return std::unexpected(std::move(result1152.error()));
if (std::move(*result1152)) {
    auto result1153 = args.at(0);
if (!result1153) return std::unexpected(std::move(result1153.error()));
auto result1154 = result1153->get().to_uint();
if (!result1154) return std::unexpected(std::move(result1154.error()));
auto result1155 = m_loungePositions.value_at(std::move(*result1154));
if (!result1155) return std::unexpected(std::move(result1155.error()));
auto& loungePosition = result1155->get();
    auto result1156 = args.at(1);
if (!result1156) return std::unexpected(std::move(result1156.error()));
auto result1157 = result1156->get().to_string();
if (!result1157) return std::unexpected(std::move(result1157.error()));
if (!LoungeControlNames) return std::unexpected(LoungeControlNames.error());
auto result1158 = LoungeControlNames->get_left(std::move(*result1157));
if (!result1158) return std::unexpected(std::move(result1158.error()));
loungePosition.masterControlState[result1158->get()].slavesHeld.remove(connection_id);
    return ChainableJsonMessageResponse(Json());
  } else {
auto result1159 = message.equals_ignore_case("control_all");
if (!result1159) return std::unexpected(std::move(result1159.error()));
if (std::move(*result1159)) {
    auto result1160 = args.at(0);
if (!result1160) return std::unexpected(std::move(result1160.error()));
auto result1161 = result1160->get().to_uint();
if (!result1161) return std::unexpected(std::move(result1161.error()));
auto result1162 = m_loungePositions.value_at(std::move(*result1161));
if (!result1162) return std::unexpected(std::move(result1162.error()));
auto& loungePosition = result1162->get();
    Set<LoungeControl> allControlsHeld;
    auto result1163 = args.at(1);
if (!result1163) return std::unexpected(std::move(result1163.error()));
auto result1164 = result1163->get().iterate_array();
if (!result1164) return std::unexpected(std::move(result1164.error()));
for (auto const& s : std::move(*result1164))
      {
auto result1165 = s.to_string();
if (!result1165) return std::unexpected(std::move(result1165.error()));
if (!LoungeControlNames) return std::unexpected(LoungeControlNames.error());
auto result1166 = LoungeControlNames->get_left(std::move(*result1165));
if (!result1166) return std::unexpected(std::move(result1166.error()));
allControlsHeld.add(result1166->get());
}
    for (auto& p : loungePosition.masterControlState) {
      if (allControlsHeld.contains(p.first))
        p.second.slavesHeld.add(connection_id);
      else
        p.second.slavesHeld.remove(connection_id);
    }
    return ChainableJsonMessageResponse(Json());
  } else {
auto result1167 = message.equals_ignore_case("aim");
if (!result1167) return std::unexpected(std::move(result1167.error()));
if (std::move(*result1167)) {
    auto result1168 = args.at(0);
if (!result1168) return std::unexpected(std::move(result1168.error()));
auto result1169 = result1168->get().to_uint();
if (!result1169) return std::unexpected(std::move(result1169.error()));
auto result1170 = m_loungePositions.value_at(std::move(*result1169));
if (!result1170) return std::unexpected(std::move(result1170.error()));
auto& loungePosition = result1170->get();
    auto result1171 = args.at(1);
if (!result1171) return std::unexpected(std::move(result1171.error()));
auto result1172 = result1171->get().to_float();
if (!result1172) return std::unexpected(std::move(result1172.error()));
auto result1173 = args.at(2);
if (!result1173) return std::unexpected(std::move(result1173.error()));
auto result1174 = result1173->get().to_float();
if (!result1174) return std::unexpected(std::move(result1174.error()));
loungePosition.masterAimPosition = {std::move(*result1172), std::move(*result1174)};
    return ChainableJsonMessageResponse(Json());
  } else {
    auto result1175 = world();
if (!result1175) return std::unexpected(std::move(result1175.error()));
auto result1176 = recover_lua_script_error(m_scriptComponent.handle_message(message, connection_id == result1175->get().connection(), args));
if (!result1176) return std::unexpected(std::move(result1176.error()));
return std::move(*result1176);
  }
}
}
}
}

RectF Vehicle::interactive_bound_box() const {
  return collision_area();
}

bool Vehicle::is_interactive() const {
  return m_interactive.get();
}

std::expected<InteractAction, StarException> Vehicle::interact(InteractRequest const& request) {
  auto result = recover_lua_script_error(m_scriptComponent.invoke<Json>("onInteraction", JsonObject{
      {"sourceId", request.source_id}, {"sourcePosition", json_from_vec2f(request.source_position)},
      {"interactPosition", json_from_vec2f(request.interact_position)}}));
  if (!result)
    return std::unexpected(std::move(result.error()));
  if (*result && !(**result).is_null())
    return interact_action_from_script(**result, entityId());

  auto distance_squared = [&](LoungePositionConfig const& lounge) -> std::expected<float, StarException> {
    auto anchor = m_networkedAnimator->part_point(lounge.part, lounge.partAnchor);
    if (!anchor)
      return std::unexpected(std::move(anchor.error()));
    if (!*anchor)
      return std::unexpected(VehicleException(Star::format("Missing lounge anchor '{}' on part '{}'", lounge.partAnchor, lounge.part)));
    return vmag_squared(**anchor + position() - request.interact_position);
  };
  std::optional<std::size_t> index;
  std::optional<float> best_distance;
  for (std::size_t i = 0; i < m_loungePositions.size(); ++i) {
    auto candidate = m_loungePositions.value_at(i);
    if (!candidate)
      return std::unexpected(std::move(candidate.error()));
    if (!candidate->get().enabled.get())
      continue;
    if (!index) {
      index = i;
      continue;
    }
    auto distance = distance_squared(candidate->get());
    if (!distance)
      return std::unexpected(std::move(distance.error()));
    if (!best_distance) {
      auto selected = m_loungePositions.value_at(*index);
      if (!selected)
        return std::unexpected(std::move(selected.error()));
      auto selected_distance = distance_squared(selected->get());
      if (!selected_distance)
        return std::unexpected(std::move(selected_distance.error()));
      best_distance = *selected_distance;
    }
    if (*distance < *best_distance) {
      index = i;
      best_distance = *distance;
    }
  }

  if (index)
    return InteractAction(InteractActionType::SitDown, entityId(), *index);

  return InteractAction();
}

std::size_t Vehicle::anchorCount() const {
  return m_loungePositions.size();
}

std::expected<std::shared_ptr<LoungeAnchor const>, StarException> Vehicle::loungeAnchor(std::size_t positionIndex) const {
  auto result1130 = m_loungePositions.value_at(positionIndex);
  if (!result1130)
    return std::unexpected(std::move(result1130.error()));
  auto const& positionConfig = result1130->get();
  if (!positionConfig.enabled.get())
    return {};

  auto result1131 = m_networkedAnimator->final_part_transformation(positionConfig.part);
if (!result1131) return std::unexpected(std::move(result1131.error()));
Mat3F partTransformation = std::move(*result1131);
  auto result1132 = m_networkedAnimator->part_property(positionConfig.part, positionConfig.partAnchor);
if (!result1132) return std::unexpected(std::move(result1132.error()));
auto result1133 = json_to_vec2f(std::move(*result1132));
if (!result1133) return std::unexpected(std::move(result1133.error()));
Vec2F partAnchor = std::move(*result1133);

  auto loungePosition = std::make_shared<LoungeAnchor>();
  loungePosition->position = partTransformation.transform_point(partAnchor) + position();
  if (positionConfig.exitBottomOffset)
    loungePosition->exitBottomPosition = partTransformation.transform_point(partAnchor + positionConfig.exitBottomOffset.value_or(Vec2F{})) + position();
  loungePosition->direction = partTransformation.determinant() > 0 ? Direction::Right : Direction::Left;
  loungePosition->angle = partTransformation.transform_angle(0.0f);
  if (loungePosition->direction == Direction::Left)
    loungePosition->angle += std::numbers::pi;
  loungePosition->controllable = true;
  loungePosition->loungeRenderLayer = render_layer(VehicleLayer::Passenger);
  loungePosition->orientation = positionConfig.orientation.get();
  loungePosition->emote = positionConfig.emote.get();
  loungePosition->dance = positionConfig.dance.get();
  loungePosition->directives = positionConfig.directives.get();
  loungePosition->status_effects = positionConfig.status_effects.get();
  loungePosition->armor_cosmetic_overrides = positionConfig.armor_cosmetic_overrides;
  loungePosition->cursorOverride = positionConfig.cursorOverride;
  loungePosition->cameraFocus = positionConfig.cameraFocus;
  loungePosition->suppressTools = positionConfig.suppressTools;
  return loungePosition;
}

std::expected<void, StarException> Vehicle::loungeControl(std::size_t index, LoungeControl loungeControl) {
  auto result1129 = m_loungePositions.value_at(index);
  if (!result1129)
    return std::unexpected(std::move(result1129.error()));
  auto& loungePosition = result1129->get();
  if (isSlave())
    loungePosition.slaveNewControls.add(loungeControl);
  else
    loungePosition.masterControlState[loungeControl].masterHeld = true;
  return {};
}

std::expected<void, StarException> Vehicle::loungeAim(std::size_t index, Vec2F const& aimPosition) {
  auto result1128 = m_loungePositions.value_at(index);
  if (!result1128)
    return std::unexpected(std::move(result1128.error()));
  auto& loungePosition = result1128->get();
  if (isSlave())
    loungePosition.slaveNewAimPosition = aimPosition;
  else
    loungePosition.masterAimPosition = aimPosition;
  return {};
}

std::expected<List<PhysicsForceRegion>, StarException> Vehicle::force_regions() const {
  List<PhysicsForceRegion> forces;
  for (auto const& [name, config] : m_force_regions) {
    if (!config.enabled.get())
      continue;
    Vec2F translation = position();
    if (config.attach_to_part) {
      auto transform = m_networkedAnimator->final_part_transformation(*config.attach_to_part);
      if (!transform)
        return std::unexpected(std::move(transform.error()));
      translation += transform->transform_point(Vec2F{});
    }
    auto& force = forces.emplace_back(config.force_region);
    std::visit([&](physics_force_region auto& region) { region.translate(translation); }, force);
  }
  return forces;
}

std::expected<List<DamageSource>, StarException> Vehicle::damage_sources() const {
  List<DamageSource> sources;
  for (auto const& p : m_damageSources) {
    if (p.second.enabled.get()) {
      DamageSource damageSource = p.second.damageSource;

      if (p.second.attachToPart) {
        auto result1126 = m_networkedAnimator->final_part_transformation(p.second.attachToPart.value());
if (!result1126) return std::unexpected(std::move(result1126.error()));
Mat3F partTransformation = std::move(*result1126);
        std::visit([partTransformation](auto& da) {
          if constexpr (!std::is_same_v<std::remove_cvref_t<decltype(da)>, std::monostate>)
            da.transform(partTransformation);
        }, damageSource.damage_area);
      }

      damageSource.team = m_damageTeam.get();
      damageSource.source_entity_id = entityId();

      sources.push_back(std::move(damageSource));
    }
  }
  return sources;
}

std::size_t Vehicle::movingCollisionCount() const {
  return m_movingCollisions.size();
}

std::expected<std::optional<PhysicsMovingCollision>, StarException> Vehicle::movingCollision(std::size_t positionIndex) const {
  auto result1124 = m_movingCollisions.value_at(positionIndex);
  if (!result1124)
    return std::unexpected(std::move(result1124.error()));
  auto const& collisionConfig = result1124->get();
  if (!collisionConfig.enabled.get())
    return {};

  PhysicsMovingCollision collision = collisionConfig.movingCollision;

  if (collisionConfig.attachToPart) {
    auto result1125 = m_networkedAnimator->final_part_transformation(*collisionConfig.attachToPart);
if (!result1125) return std::unexpected(std::move(result1125.error()));
Mat3F partTransformation = std::move(*result1125);

    Vec2F localTranslation = partTransformation.transform_point(Vec2F());
    collision.position += localTranslation;

    Mat3F localTransform = Mat3F::translation(-localTranslation) * partTransformation;
    collision.collision.transform(localTransform);
  }

  collision.position += position();

  return collision;
}

std::expected<std::optional<LuaValue>, StarException> Vehicle::call_script(String const& func, LuaVariadic<LuaValue> const& args) {
  auto result1123 = recover_lua_script_error(m_scriptComponent.invoke(func, args));
if (!result1123) return std::unexpected(std::move(result1123.error()));
return std::move(*result1123);
}

std::expected<std::optional<LuaValue>, StarException> Vehicle::eval_script(String const& code) {
  auto result1122 = recover_lua_script_error(m_scriptComponent.eval(code));
if (!result1122) return std::unexpected(std::move(result1122.error()));
return std::move(*result1122);
}

MovementController& Vehicle::movement_controller() {
  return *m_movement_controller;
}

void Vehicle::setPosition(Vec2F const& position) {
  resetPresentation();
  m_movement_controller->set_position(position);
}

EntityRenderLayer Vehicle::render_layer(VehicleLayer vehicle_layer) const {
  // Z-offset based on entity id, so vehicles don't overlap strangely.
  return m_override_render_layer
    ? (*m_override_render_layer + static_cast<unsigned>(vehicle_layer))
    : (m_base_render_layer
       + vehicle_layer_offset(entityId(), static_cast<EntityRenderLayer>(vehicle_layer)));
}

std::expected<LuaCallbacks, StarException> Vehicle::makeVehicleCallbacks() {
  LuaCallbacks callbacks;

  auto result1104 = callbacks.register_callback("controlHeld", [this](String const& loungeName, String const& controlName)  -> std::expected<bool, StarException> {
      auto result1004 = m_loungePositions.get(loungeName);
if (!result1004) return std::unexpected(std::move(result1004.error()));
if (!LoungeControlNames) return std::unexpected(LoungeControlNames.error());
auto result1005 = LoungeControlNames->get_left(controlName);
if (!result1005) return std::unexpected(std::move(result1005.error()));
auto const& mc = result1004->get().masterControlState[result1005->get()];
      return mc.masterHeld || !mc.slavesHeld.empty();
    });
if (!result1104) return std::unexpected(std::move(result1104.error()));

  auto result1105 = callbacks.register_callback("shiftingHeld", [this](String const& loungeName)  -> std::expected<bool, StarException> {
      auto result1000 = m_loungePositions.get(loungeName);
if (!result1000) return std::unexpected(std::move(result1000.error()));
auto const& mc = result1000->get().masterControlState[LoungeControl::Walk];
      if (mc.masterHeld || !mc.slavesHeld.empty())
        return true;
      else {
        auto result1001 = loungeIndex(loungeName);
if (!result1001) return std::unexpected(std::move(result1001.error()));
auto result1002 = entitiesLoungingIn(std::move(*result1001));
if (!result1002) return std::unexpected(std::move(result1002.error()));
for (EntityId entity : std::move(*result1002)) {
          auto result1003 = world();
if (!result1003) return std::unexpected(std::move(result1003.error()));
if (auto player = result1003->get().get<Player>(entity))
            return player->shifting();
        }
      }
      return false;
    });
if (!result1105) return std::unexpected(std::move(result1105.error()));

  auto result1106 = callbacks.register_callback( "aimPosition", [this](String const& loungeName)  -> std::expected<Vec2F, StarException> {
      auto result999 = m_loungePositions.get(loungeName);
if (!result999) return std::unexpected(std::move(result999.error()));
return result999->get().masterAimPosition;
    });
if (!result1106) return std::unexpected(std::move(result1106.error()));

  auto result1107 = callbacks.register_callback("entityLoungingIn", [this](String const& name) -> std::expected<LuaValue, StarException> {
      auto result996 = loungeIndex(name);
if (!result996) return std::unexpected(std::move(result996.error()));
auto result997 = entitiesLoungingIn(std::move(*result996));
if (!result997) return std::unexpected(std::move(result997.error()));
auto entitiesIn = std::move(*result997);
      if (entitiesIn.empty())
        return lua_nil;
      auto result998 = entitiesIn.first();
if (!result998) return std::unexpected(std::move(result998.error()));
return LuaInt(*result998);
    });
if (!result1107) return std::unexpected(std::move(result1107.error()));

  auto result1108 = callbacks.register_callback("setLoungeEnabled", [this](String const& name, bool enabled)  -> std::expected<void, StarException> {
      auto result995 = m_loungePositions.get(name);
if (!result995) return std::unexpected(std::move(result995.error()));
result995->get().enabled.set(enabled);
      return {};
});
if (!result1108) return std::unexpected(std::move(result1108.error()));

  auto result1109 = callbacks.register_callback("setLoungeOrientation", [this](String const& name, String const& orientation)  -> std::expected<void, StarException> {
      auto result993 = m_loungePositions.get(name);
if (!result993) return std::unexpected(std::move(result993.error()));
if (!LoungeOrientationNames) return std::unexpected(LoungeOrientationNames.error());
auto result994 = LoungeOrientationNames->get_left(orientation);
if (!result994) return std::unexpected(std::move(result994.error()));
result993->get().orientation.set(result994->get());
      return {};
});
if (!result1109) return std::unexpected(std::move(result1109.error()));

  auto result1110 = callbacks.register_callback("setLoungeEmote", [this](String const& name, std::optional<String> emote)  -> std::expected<void, StarException> {
      auto result992 = m_loungePositions.get(name);
if (!result992) return std::unexpected(std::move(result992.error()));
result992->get().emote.set(std::move(emote));
      return {};
});
if (!result1110) return std::unexpected(std::move(result1110.error()));

  auto result1111 = callbacks.register_callback("setLoungeDance", [this](String const& name, std::optional<String> dance)  -> std::expected<void, StarException> {
      auto result991 = m_loungePositions.get(name);
if (!result991) return std::unexpected(std::move(result991.error()));
result991->get().dance.set(std::move(dance));
      return {};
});
if (!result1111) return std::unexpected(std::move(result1111.error()));

  auto result1112 = callbacks.register_callback("setLoungeDirectives", [this](String const& name, std::optional<String> directives)  -> std::expected<void, StarException> {
      auto result990 = m_loungePositions.get(name);
if (!result990) return std::unexpected(std::move(result990.error()));
result990->get().directives.set(std::move(directives));
      return {};
});
if (!result1112) return std::unexpected(std::move(result1112.error()));

  auto result1113 = callbacks.register_callback("setLoungeStatusEffects", [this](String const& name, JsonArray const& status_effects)  -> std::expected<void, StarException> {
      auto result988 = m_loungePositions.get(name);
if (!result988) return std::unexpected(std::move(result988.error()));
auto result989 = json_to_list<PersistentStatusEffect>(Json(status_effects),jsonToPersistentStatusEffect);
if (!result989) return std::unexpected(std::move(result989.error()));
result988->get().status_effects.set(std::move(*result989));
      return {};
});
if (!result1113) return std::unexpected(std::move(result1113.error()));

  auto result1114 = callbacks.register_callback("setPersistent", [this](bool persistent) {
      setPersistent(persistent);
    });
if (!result1114) return std::unexpected(std::move(result1114.error()));

  auto result1115 = callbacks.register_callback("setInteractive", [this](bool interactive) {
      m_interactive.set(interactive);
    });
if (!result1115) return std::unexpected(std::move(result1115.error()));

  auto result1116 = callbacks.register_callback("setDamageTeam", [this](Json damageTeam)  -> std::expected<void, StarException> {
      auto result987 = EntityDamageTeam::create(damageTeam);
if (!result987) return std::unexpected(std::move(result987.error()));
m_damageTeam.set(std::move(*result987));
      return {};
});
if (!result1116) return std::unexpected(std::move(result1116.error()));

  auto result1117 = callbacks.register_callback("setDamageSourceEnabled", [this](String const& name, bool enabled)  -> std::expected<void, StarException> {
      auto result986 = m_damageSources.get(name);
if (!result986) return std::unexpected(std::move(result986.error()));
result986->get().enabled.set(enabled);
      return {};
});
if (!result1117) return std::unexpected(std::move(result1117.error()));

  auto result1118 = callbacks.register_callback("setMovingCollisionEnabled", [this](String const& name, bool enabled)  -> std::expected<void, StarException> {
      auto result985 = m_movingCollisions.get(name);
if (!result985) return std::unexpected(std::move(result985.error()));
result985->get().enabled.set(enabled);
      return {};
});
if (!result1118) return std::unexpected(std::move(result1118.error()));

  auto result1119 = callbacks.register_callback("setForceRegionEnabled", [this](String const& name, bool enabled)  -> std::expected<void, StarException> {
      auto result984 = m_force_regions.get(name);
if (!result984) return std::unexpected(std::move(result984.error()));
result984->get().enabled.set(enabled);
      return {};
});
if (!result1119) return std::unexpected(std::move(result1119.error()));

  auto result1120 = callbacks.register_callback("destroy", [this]() {
      m_shouldDestroy = true;
    });
if (!result1120) return std::unexpected(std::move(result1120.error()));

  auto result1121 = callbacks.register_callback("setAnimationParameter", [this](String name, Json value) {
      m_scriptedAnimationParameters.set(std::move(name), std::move(value));
    });
if (!result1121) return std::unexpected(std::move(result1121.error()));

  return callbacks;
}

std::expected<Json, StarException> Vehicle::configValue(String const& name, Json def) const {
  auto result1103 = json_merge_query_def(name, std::move(def), m_baseConfig, m_dynamicConfig);
if (!result1103) return std::unexpected(std::move(result1103.error()));
return std::move(*result1103);
}

std::expected<std::size_t, StarException> Vehicle::loungeIndex(String const& name) const {
  auto index = m_loungePositions.index_of(name);
  if (!index) return std::unexpected(VehicleException(Star::format("No such lounge position '{}'",name)));
  return *index;
}

}

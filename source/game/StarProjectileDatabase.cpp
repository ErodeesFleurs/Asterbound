import star.json;
import star.json_extra;
import star.set;
import star.thread;
import star.color;
import star.string;
import star.data_stream;
import star.rect;
import star.list;
import star.image_processing;
import star.hash;
import star.string_view;
import star.directives;
import star.asset_path;
import star.game_types;
import star.id_map;
import star.ordered_map;
import star.casting;
import star.strong_typedef;
import star.rpc_promise;
import star.lua;
import star.bi_map;
import star.io_device;
import star.ref_ptr;
import star.logging;
import star.listener;
import std;
import star.format;
import star.output_proxy;
import star.version;

import star.drawable;
import star.entity_rendering_types;
import star.damage_types;
import star.world_geometry;
import star.status_types;
import star.damage;
import star.light_source;
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
import star.scripted_entity;
import star.status_effect_entity;
import star.movement_controller;
import star.animation;
import star.particle;
import star.lua_components;
import star.effect_emitter;
import star.projectile;
import star.asset_source;
import star.assets;
import star.root_base;
import star.configuration;
import star.root;

import star.projectile_database;

import star.data_stream_devices;

import star.net_compatibility;
import star.byte_array;
namespace Star {

std::expected<std::shared_ptr<ProjectileDatabase>, StarException> ProjectileDatabase::create(Root& root) {
  struct MakeShared final : ProjectileDatabase {
    explicit MakeShared(Root& root) : ProjectileDatabase(root) {}
  };
  auto database = std::make_shared<MakeShared>(root);
  auto assets = root.assets();
  if (!assets) return std::unexpected(std::move(assets.error()));
  auto files = (*assets)->scanExtension("projectile");
  if (!files) return std::unexpected(std::move(files.error()));
  if (auto queued = (*assets)->queueJsons(files->get()); !queued) return std::unexpected(std::move(queued.error()));
  for (auto const& scannedFile : files->get()) {
    String file = scannedFile.string();
    auto config = database->readConfig(file);
    if (!config) {
      if (auto logged = root.logger().errorException(strf("Could not read projectile '{}'", file), ExceptionDiagnostic(config.error())); !logged)
        return std::unexpected(std::move(logged.error()));
      continue;
    }
    if (database->m_configs.contains((*config)->typeName)) {
      auto error = (*assets)->withAssetContext(ProjectileDatabaseException::format("Duplicate projectile asset typeName {}. configfile {}", (*config)->typeName, file), file, "Loading projectile");
      if (auto logged = root.logger().errorException("Error loading projectile", ExceptionDiagnostic(error)); !logged)
        return std::unexpected(std::move(logged.error()));
    } else database->m_configs[(*config)->typeName] = std::move(*config);
  }
  return database;
}

StringList ProjectileDatabase::allProjectileTypes() const {
  return m_configs.keys();
}

bool ProjectileDatabase::isProjectile(String const& projectileName) const {
  return m_configs.contains(projectileName);
}

std::expected<Json, StarException> ProjectileDatabase::projectileConfig(String const& type) const {
  auto config = m_configs.find_ref(type);
  if (!config) return std::unexpected(ProjectileDatabaseException(strf("Unknown projectile with typeName {}.", type)));
  return config->get()->config;
}

std::expected<std::shared_ptr<Projectile>, StarException> ProjectileDatabase::createProjectile(String const& type, Json const& parameters) const {
  auto config = m_configs.find_ref(type);
  if (!config) return std::unexpected(ProjectileDatabaseException(strf("Unknown projectile with typeName {}.", type)));
  return Projectile::create(m_root, config->get(), parameters);
}

std::expected<String, StarException> ProjectileDatabase::damageKindImage(String const& type) const {
  auto config = m_configs.find_ref(type);
  if (!config) return std::unexpected(ProjectileDatabaseException(strf("Unknown projectile with typeName {}.", type)));
  return config->get()->damageKindImage;
}

std::expected<float, StarException> ProjectileDatabase::gravityMultiplier(String const& type) const {
  auto config = m_configs.find_ref(type);
  if (!config) return std::unexpected(ProjectileDatabaseException(strf("Unknown projectile with typeName {}.", type)));
  return config->get()->movementSettings.getFloat("gravityMultiplier", 1);
}

std::expected<std::shared_ptr<Projectile>, StarException> ProjectileDatabase::netLoadProjectile(ByteArray const& netStore, NetCompatibilityRules rules) const {
  DataStreamBuffer ds(netStore);
  ds.setStreamCompatibilityVersion(rules);
  auto typeName = ds.read<String>();
  if (!typeName) return std::unexpected(std::move(typeName.error()));
  auto config = m_configs.find_ref(*typeName);
  if (!config) return std::unexpected(ProjectileDatabaseException(strf("Unknown projectile with typeName {}.", *typeName)));
  return Projectile::create(m_root, config->get(), ds, rules);
}

std::expected<std::shared_ptr<ProjectileConfig>, StarException> ProjectileDatabase::readConfig(String const& path) {
  auto result74 = m_root.assets();
if (!result74) return std::unexpected(std::move(result74.error()));
auto assets = std::move(*result74);

  auto loaded = [&]() -> std::expected<std::shared_ptr<ProjectileConfig>, StarException> {
  auto result75 = assets->json(path);
if (!result75) return std::unexpected(std::move(result75.error()));
Json config = std::move(*result75);

  auto projectileConfig = std::make_shared<ProjectileConfig>();
  projectileConfig->config = config;
  auto result76 = config.getString("projectileName");
if (!result76) return std::unexpected(std::move(result76.error()));
projectileConfig->typeName = std::move(*result76);
  projectileConfig->directory = AssetPath::directory(path);

  auto result77 = config.getString("description", "");
if (!result77) return std::unexpected(std::move(result77.error()));
projectileConfig->description = std::move(*result77);

  auto boundBox = config.opt("boundBox");
  if (!boundBox) return std::unexpected(std::move(boundBox.error()));
  if (*boundBox) {
    auto parsed = jsonToRectF(**boundBox);
    if (!parsed) return std::unexpected(std::move(parsed.error()));
    projectileConfig->boundBox = *parsed;
  } else projectileConfig->boundBox = RectF{-5,-5,5,5};

  auto result78 = config.getString("physics", "default");
if (!result78) return std::unexpected(std::move(result78.error()));
auto physicsType = std::move(*result78);
  auto result79 = config.getObject("movementSettings", JsonObject());
if (!result79) return std::unexpected(std::move(result79.error()));
JsonObject movementSettings = std::move(*result79);
  auto physicsPath = strf("/projectiles/physics.config:{}", physicsType);
  auto result80 = assets->json(physicsPath);
if (!result80) return std::unexpected(assets->withAssetContext(std::move(result80.error()), physicsPath, "Loading projectile physics"));
  projectileConfig->movementSettings = jsonMerge(std::move(*result80), movementSettings);

  auto result82 = config.getFloat("speed", 50);
if (!result82) return std::unexpected(std::move(result82.error()));
projectileConfig->initialSpeed = std::move(*result82);
  auto result83 = config.getFloat("acceleration", 0);
if (!result83) return std::unexpected(std::move(result83.error()));
projectileConfig->acceleration = std::move(*result83);
  auto result84 = config.getFloat("power", 1);
if (!result84) return std::unexpected(std::move(result84.error()));
projectileConfig->power = std::move(*result84);
  auto result85 = config.contains("damagePoly");
if (!result85) return std::unexpected(std::move(result85.error()));
if (std::move(*result85)) {
    auto result86 = config.get("damagePoly");
if (!result86) return std::unexpected(std::move(result86.error()));
auto result87 = jsonToPolyF(std::move(*result86));
if (!result87) return std::unexpected(std::move(result87.error()));
projectileConfig->damagePoly = std::move(*result87);
    projectileConfig->damagePoly.scale(1.0f / TilePixels);
  }
  auto result88 = config.getBool("piercing", false);
if (!result88) return std::unexpected(std::move(result88.error()));
projectileConfig->piercing = std::move(*result88);
  auto result89 = config.getBool("falldown", false);
if (!result89) return std::unexpected(std::move(result89.error()));
projectileConfig->falldown = std::move(*result89);
  auto result90 = config.getInt("bounces", 0);
if (!result90) return std::unexpected(std::move(result90.error()));
projectileConfig->bounces = std::move(*result90);

  auto result91 = config.getArray("actionOnCollide", {});
if (!result91) return std::unexpected(std::move(result91.error()));
projectileConfig->actionOnCollide = std::move(*result91);
  auto result92 = config.getArray("actionOnReap", {});
if (!result92) return std::unexpected(std::move(result92.error()));
projectileConfig->actionOnReap = std::move(*result92);
  auto result93 = config.getArray("actionOnHit", {});
if (!result93) return std::unexpected(std::move(result93.error()));
projectileConfig->actionOnHit = std::move(*result93);
  auto result94 = config.getArray("actionOnTimeout", {});
if (!result94) return std::unexpected(std::move(result94.error()));
projectileConfig->actionOnTimeout = std::move(*result94);

  auto result95 = config.getArray("periodicActions", {});
if (!result95) return std::unexpected(std::move(result95.error()));
for (auto const& c : std::move(*result95))
    {
auto result96 = c.getFloat("time");
if (!result96) return std::unexpected(std::move(result96.error()));
auto result97 = c.getBool("repeat", true);
if (!result97) return std::unexpected(std::move(result97.error()));
projectileConfig->periodicActions.append(std::make_tuple(std::move(*result96), std::move(*result97), c));
}

  auto result98 = config.getString("image");
if (!result98) return std::unexpected(std::move(result98.error()));
projectileConfig->image = AssetPath::relative_to(path, std::move(*result98));
auto result99 = config.getUInt("frameNumber", 1);
if (!result99) return std::unexpected(std::move(result99.error()));
projectileConfig->frameNumber = std::move(*result99);
  auto result100 = config.getFloat("animationCycle", 1.0);
if (!result100) return std::unexpected(std::move(result100.error()));
projectileConfig->animationCycle = std::move(*result100);
  auto result101 = config.getBool("animationLoops", true);
if (!result101) return std::unexpected(std::move(result101.error()));
projectileConfig->animationLoops = std::move(*result101);
  auto result102 = config.getUInt("windupFrames", 0);
if (!result102) return std::unexpected(std::move(result102.error()));
projectileConfig->windupFrames = std::move(*result102);
  auto result103 = config.getBool("intangibleWindup", false);
if (!result103) return std::unexpected(std::move(result103.error()));
projectileConfig->intangibleWindup = std::move(*result103);
  auto result104 = config.getUInt("winddownFrames", 0);
if (!result104) return std::unexpected(std::move(result104.error()));
projectileConfig->winddownFrames = std::move(*result104);
  auto result105 = config.getBool("intangibleWinddown", false);
if (!result105) return std::unexpected(std::move(result105.error()));
projectileConfig->intangibleWinddown = std::move(*result105);
  auto result106 = config.getBool("flippable", false);
if (!result106) return std::unexpected(std::move(result106.error()));
projectileConfig->flippable = std::move(*result106);
  auto result107 = config.getBool("orientationLocked", false);
if (!result107) return std::unexpected(std::move(result107.error()));
projectileConfig->orientationLocked = std::move(*result107);

  auto result108 = config.getBool("fullbright", false);
if (!result108) return std::unexpected(std::move(result108.error()));
projectileConfig->fullbright = std::move(*result108);
  auto result109 = config.getString("renderLayer", "Projectile");
if (!result109) return std::unexpected(std::move(result109.error()));
auto result110 = parseRenderLayer(std::move(*result109));
if (!result110) return std::unexpected(std::move(result110.error()));
projectileConfig->renderLayer = std::move(*result110);

  auto result111 = config.get("lightColor", JsonArray{0, 0, 0});
if (!result111) return std::unexpected(std::move(result111.error()));
auto result112 = jsonToColor(std::move(*result111));
if (!result112) return std::unexpected(std::move(result112.error()));
projectileConfig->lightColor = std::move(*result112);
  auto result113 = config.get("lightPosition", JsonArray{0, 0});
if (!result113) return std::unexpected(std::move(result113.error()));
auto result114 = jsonToVec2F(std::move(*result113));
if (!result114) return std::unexpected(std::move(result114.error()));
projectileConfig->lightPosition = std::move(*result114);
  auto result115 = config.optString("lightType");
if (!result115) return std::unexpected(std::move(result115.error()));
if (auto lightType = std::move(*result115))
    {
  auto result116 = enumValue<LightTypeNames>(*lightType);
  if (!result116)
    return std::unexpected(std::move(result116.error()));
  projectileConfig->lightType = *result116;
}
  else
    {
auto result117 = config.getBool("pointLight", false);
if (!result117) return std::unexpected(std::move(result117.error()));
projectileConfig->lightType = static_cast<LightType>(std::move(*result117));
}

  auto result118 = config.getString("persistentAudio", "");
if (!result118) return std::unexpected(std::move(result118.error()));
projectileConfig->persistentAudio = std::move(*result118);

  // Initialize timeToLive after animationCycle so we can have the default be
  // based on animationCycle
  if (!projectileConfig->animationLoops)
    {
auto result119 = config.getFloat("timeToLive", projectileConfig->animationCycle);
if (!result119) return std::unexpected(std::move(result119.error()));
projectileConfig->timeToLive = std::move(*result119);
}
  else
    {
auto result120 = config.getFloat("timeToLive", 5.0);
if (!result120) return std::unexpected(std::move(result120.error()));
projectileConfig->timeToLive = std::move(*result120);
}

  auto result121 = config.getString("damageKindImage", "");
if (!result121) return std::unexpected(std::move(result121.error()));
projectileConfig->damageKindImage = std::move(*result121);

  auto result122 = config.getString("damageKind", "");
if (!result122) return std::unexpected(std::move(result122.error()));
projectileConfig->damageKind = std::move(*result122);
  auto result123 = config.getString("damageType", "damage");
if (!result123) return std::unexpected(std::move(result123.error()));
projectileConfig->damageType = std::move(*result123);
  auto result124 = config.get("damageTeam", Json());
if (!result124) return std::unexpected(std::move(result124.error()));
projectileConfig->damageTeam = std::move(*result124);
  auto result125 = config.optString("damageRepeatGroup");
if (!result125) return std::unexpected(std::move(result125.error()));
projectileConfig->damageRepeatGroup = std::move(*result125);
  auto result126 = config.optFloat("damageRepeatTimeout");
if (!result126) return std::unexpected(std::move(result126.error()));
projectileConfig->damageRepeatTimeout = std::move(*result126);

  if (!projectileConfig->damageKindImage.empty())
    projectileConfig->damageKindImage =
      AssetPath::relative_to(projectileConfig->directory, projectileConfig->damageKindImage);

  auto result127 = config.getArray("statusEffects", {});
if (!result127) return std::unexpected(std::move(result127.error()));
for (auto const& effect : std::move(*result127)) {
auto result128 = jsonToEphemeralStatusEffect(effect);
if (!result128) return std::unexpected(std::move(result128.error()));
projectileConfig->statusEffects.append(std::move(*result128));
}

  auto result129 = config.getArray("emitters", JsonArray());
if (!result129) return std::unexpected(std::move(result129.error()));
auto result130 = jsonToStringSet(std::move(*result129));
if (!result130) return std::unexpected(std::move(result130.error()));
projectileConfig->emitters = std::move(*result130);

  auto result131 = config.getBool("hydrophobic", false);
if (!result131) return std::unexpected(std::move(result131.error()));
projectileConfig->hydrophobic = std::move(*result131);

  auto result132 = config.getBool("rayCheckToSource", false);
if (!result132) return std::unexpected(std::move(result132.error()));
projectileConfig->rayCheckToSource = std::move(*result132);
  auto result133 = config.getFloat("knockback", 0.0f);
if (!result133) return std::unexpected(std::move(result133.error()));
projectileConfig->knockback = std::move(*result133);
  auto result134 = config.getBool("knockbackDirectional", false);
if (!result134) return std::unexpected(std::move(result134.error()));
projectileConfig->knockbackDirectional = std::move(*result134);

  auto result135 = config.getBool("onlyHitTerrain", false);
if (!result135) return std::unexpected(std::move(result135.error()));
projectileConfig->onlyHitTerrain = std::move(*result135);

  auto result136 = config.getString("clientEntityMode", "ClientMasterAllowed");
if (!result136) return std::unexpected(std::move(result136.error()));
if (!ClientEntityModeNames) return std::unexpected(ClientEntityModeNames.error());
auto result137 = ClientEntityModeNames->get_left(std::move(*result136));
if (!result137) return std::unexpected(std::move(result137.error()));
projectileConfig->clientEntityMode = result137->get();
  auto result138 = config.getBool("masterOnly", false);
if (!result138) return std::unexpected(std::move(result138.error()));
projectileConfig->masterOnly = std::move(*result138);

  auto result139 = config.get("scripts", JsonArray());
if (!result139) return std::unexpected(std::move(result139.error()));
auto result140 = jsonToStringList(std::move(*result139));
if (!result140) return std::unexpected(std::move(result140.error()));
std::ranges::for_each(*result140, [&](String& script) { script = AssetPath::relative_to(path, script); });
projectileConfig->scripts = std::move(*result140);

  auto result141 = config.get("physicsForces", JsonObject());
if (!result141) return std::unexpected(std::move(result141.error()));
projectileConfig->physicsForces = std::move(*result141);
  auto result142 = config.get("physicsCollisions", JsonObject());
if (!result142) return std::unexpected(std::move(result142.error()));
projectileConfig->physicsCollisions = std::move(*result142);

  auto result143 = config.getArray("persistentStatusEffects", JsonArray());
if (!result143) return std::unexpected(std::move(result143.error()));
for (auto const& effect : std::move(*result143)) {
auto result144 = jsonToPersistentStatusEffect(effect);
if (!result144) return std::unexpected(std::move(result144.error()));
projectileConfig->persistentStatusEffects.append(std::move(*result144));
}
  auto result145 = config.get("statusEffectArea", JsonArray());
if (!result145) return std::unexpected(std::move(result145.error()));
auto result146 = jsonToPolyF(std::move(*result145));
if (!result146) return std::unexpected(std::move(result146.error()));
projectileConfig->statusEffectArea = std::move(*result146);

  return projectileConfig;
  }();
  if (!loaded)
    return std::unexpected(assets->withAssetContext(std::move(loaded.error()), path, "Loading projectile"));
  return loaded;
}

}

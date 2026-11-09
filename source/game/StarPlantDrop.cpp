import star.json;
import star.id_map;
import star.list;
import star.rect;
import star.image_processing;
import star.hash;
import star.data_stream;
import star.string_view;
import star.thread;
import star.directives;
import star.asset_path;
import star.casting;
import star.strong_typedef;
import star.rpc_promise;
import star.data_stream_extra;
import star.audio;
import star.map;
import star.set;
import star.vector;
import star.bi_map;
import star.color;
import star.string;
import star.poly;
import star.game_types;
import star.random;
import star.a_star;
import star.ordered_set;
import star.either;
import star.periodic_function;
import star.ordered_map;
import star.io_device;
import star.ref_ptr;
import star.logging;
import star.listener;
import std;
import star.format;
import star.version;
import star.lua;

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
import star.movement_controller;
import star.mobile_entity;
import star.inspectable_entity;
import star.plant;

import star.plant_drop;
import star.uuid;
import star.animated_part_set;
import star.networked_animator;
import star.humanoid;
import star.anchorable_entity;
import star.actor_entity;
import star.tool_user_entity;
import star.lounging_entities;
import star.chat_action;
import star.chatty_entity;
import star.emote_entity;
import star.portrait_entity;
import star.damage_bar_entity;
import star.nametag_entity;
import star.inventory_types;
import star.platformer_astar_types;
import star.game_timers;
import star.actor_movement_controller;
import star.ai_types;
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
import star.scripted_entity;
import star.item_drop;
import star.mixer;
import star.drawable;
import star.entity_rendering_types;
import star.animation;
import star.particle;

import star.light_source;
import star.entity_rendering;

import star.particle_database;
import star.image_metadata_database;


import star.data_stream_devices;

import star.net_compatibility;
import star.byte_array;
import star.math_common;
namespace Star {

PlantDrop::PlantDrop(Root& rootContext) : Entity(rootContext) {}


PlantDrop::PlantDropPiece::PlantDropPiece() {
  image = "";
  offset = {};
  segmentIdx = 0;
  kind = Plant::PlantPieceKind::None;
  flip = false;
}

std::expected<std::shared_ptr<PlantDrop>, StarException> PlantDrop::create(Root& rootContext, List<Plant::PlantPiece> pieces, Vec2F const& position, Vec2F const& strikeVector, String const& description,
    bool upsideDown, Json stemConfig, Json foliageConfig, Json saplingConfig, bool master, float random) {
  struct MakeShared final : PlantDrop {
    explicit MakeShared(Root& root) : Entity(root), PlantDrop(root) {}
  };
  auto entity = std::make_shared<MakeShared>(rootContext);
  auto controller = MovementController::create(rootContext);
  if (!controller) return std::unexpected(std::move(controller.error()));
  entity->m_movementController = std::move(*controller);
  if (auto result = entity->initialize(std::move(pieces), position, strikeVector, description, upsideDown, std::move(stemConfig), std::move(foliageConfig), std::move(saplingConfig), master, random); !result) return std::unexpected(std::move(result.error()));
  return entity;
}

std::expected<void, StarException> PlantDrop::initialize(List<Plant::PlantPiece> pieces, Vec2F const& position, Vec2F const& strikeVector, String const& description,
    bool upsideDown, Json stemConfig, Json foliageConfig, Json saplingConfig, bool master, float random) {
  auto result361 = m_netGroup.addNetElement(*m_movementController);
if (!result361) return std::unexpected(std::move(result361.error()));
  auto result362 = m_netGroup.addNetElement(m_spawnedDrops);
if (!result362) return std::unexpected(std::move(result362.error()));

  m_stemConfig = std::move(stemConfig);
  if (m_stemConfig.isNull())
    m_stemConfig = JsonObject();
  m_foliageConfig = std::move(foliageConfig);
  if (m_foliageConfig.isNull())
    m_foliageConfig = JsonObject();
  m_saplingConfig = std::move(saplingConfig);

  m_master = master;
  m_firstTick = true;
  m_spawnedDrops.set(false);
  m_spawnedDropEffects = false;
  m_time = 5000;
  m_description = description;
  m_movementController->setPosition(position);
  if (!upsideDown) {
    m_rotationRate = std::copysign(0.00001f, -strikeVector[0] + random);
    m_rotationFallThreshold = std::numbers::pi / (3 + random);
    m_rotationCap = std::numbers::pi - m_rotationFallThreshold;
  } else {
    m_rotationRate = 0;
    m_rotationFallThreshold = 0;
    m_rotationCap = 0;
  }

  bool structuralFound = false;
  RectF stemBounds = RectF::null();
  RectF fullBounds = RectF::null();

  // note structuralSegment is only available in the constructor
  for (auto& piece : pieces) {
    for (auto& pos : piece.spaces) {
      fullBounds.combine(Vec2F(pos));
      fullBounds.combine(Vec2F(pos) + Vec2F(1, 1));
      if (piece.structuralSegment) {
        structuralFound = true;
        stemBounds.combine(Vec2F(pos));
        stemBounds.combine(Vec2F(pos) + Vec2F(1, 1));
      }
    }
    PlantDropPiece pdp;
    pdp.image = piece.image;
    pdp.offset = piece.offset;
    pdp.segmentIdx = piece.segmentIdx;
    pdp.kind = piece.kind;
    pdp.flip = piece.flip;
    m_pieces.push_back(pdp);
  }

  if (fullBounds.isNull())
    fullBounds = RectF(position, position);
  if (stemBounds.isNull())
    stemBounds = RectF(position, position);

  m_boundingBox = fullBounds;
  if (structuralFound)
    m_collisionRect = stemBounds;
  else
    m_collisionRect = fullBounds;
  return {};
}

std::expected<std::shared_ptr<PlantDrop>, StarException> PlantDrop::create(Root& rootContext, ByteArray const& netStore, NetCompatibilityRules rules) {
  struct MakeShared final : PlantDrop {
    explicit MakeShared(Root& root) : Entity(root), PlantDrop(root) {}
  };
  auto entity = std::make_shared<MakeShared>(rootContext);
  auto controller = MovementController::create(rootContext);
  if (!controller) return std::unexpected(std::move(controller.error()));
  entity->m_movementController = std::move(*controller);
  if (auto result = entity->initialize(netStore, rules); !result) return std::unexpected(std::move(result.error()));
  return entity;
}

std::expected<void, StarException> PlantDrop::initialize(ByteArray const& netStore, NetCompatibilityRules rules) {
  auto result349 = m_netGroup.addNetElement(*m_movementController);
if (!result349) return std::unexpected(std::move(result349.error()));
  auto result350 = m_netGroup.addNetElement(m_spawnedDrops);
if (!result350) return std::unexpected(std::move(result350.error()));

DataStreamReader ds(netStore.span());
ds.set_stream_compatibility_version(rules);
auto stream351 = (ds >> m_time);
if (!stream351) return std::unexpected(std::move(stream351.error()));
  auto stream352 = (ds >> m_master);
if (!stream352) return std::unexpected(std::move(stream352.error()));
  auto stream353 = (ds >> m_description);
if (!stream353) return std::unexpected(std::move(stream353.error()));
  auto stream354 = (ds >> m_boundingBox);
if (!stream354) return std::unexpected(std::move(stream354.error()));
  auto stream355 = (ds >> m_collisionRect);
if (!stream355) return std::unexpected(std::move(stream355.error()));
  auto stream356 = (ds >> m_rotationRate);
if (!stream356) return std::unexpected(std::move(stream356.error()));
auto result357 =
  ds.read_container(m_pieces, [](DataStream& ds, PlantDropPiece& piece) -> std::expected<void, StarException> {
    auto result275 = ds.read(piece.image);
    if (!result275)
      return std::unexpected(std::move(result275.error()));
    auto result276 = ds.read(piece.offset[0]);
    if (!result276)
      return std::unexpected(std::move(result276.error()));
    auto result277 = ds.read(piece.offset[1]);
    if (!result277)
      return std::unexpected(std::move(result277.error()));
    auto result278 = ds.read(piece.flip);
    if (!result278)
      return std::unexpected(std::move(result278.error()));
    auto result279 = ds.read(piece.kind);
    if (!result279)
      return std::unexpected(std::move(result279.error()));
    return {};
  });
if (!result357) return std::unexpected(std::move(result357.error()));
  auto stream358 = (ds >> m_stemConfig);
if (!stream358) return std::unexpected(std::move(stream358.error()));
  auto stream359 = (ds >> m_foliageConfig);
if (!stream359) return std::unexpected(std::move(stream359.error()));
  auto stream360 = (ds >> m_saplingConfig);
if (!stream360) return std::unexpected(std::move(stream360.error()));

  m_firstTick = true;
  m_spawnedDropEffects = true;
  return {};
}

std::expected<ByteArray, StarException> PlantDrop::netStore(NetCompatibilityRules) {
  DataStreamBuffer ds;
  auto stream339 = (ds << m_time);
if (!stream339) return std::unexpected(std::move(stream339.error()));
  auto stream340 = (ds << m_master);
if (!stream340) return std::unexpected(std::move(stream340.error()));
  auto stream341 = (ds << m_description);
if (!stream341) return std::unexpected(std::move(stream341.error()));
  auto stream342 = (ds << m_boundingBox);
if (!stream342) return std::unexpected(std::move(stream342.error()));
  auto stream343 = (ds << m_collisionRect);
if (!stream343) return std::unexpected(std::move(stream343.error()));
  auto stream344 = (ds << m_rotationRate);
if (!stream344) return std::unexpected(std::move(stream344.error()));
auto result345 =
  ds.write_container(m_pieces, [](DataStream& ds, PlantDropPiece const& piece) -> std::expected<void, StarException> {
    auto result270 = ds.write(piece.image);
    if (!result270)
      return std::unexpected(std::move(result270.error()));
    auto result271 = ds.write(piece.offset[0]);
    if (!result271)
      return std::unexpected(std::move(result271.error()));
    auto result272 = ds.write(piece.offset[1]);
    if (!result272)
      return std::unexpected(std::move(result272.error()));
    auto result273 = ds.write(piece.flip);
    if (!result273)
      return std::unexpected(std::move(result273.error()));
    auto result274 = ds.write(piece.kind);
    if (!result274)
      return std::unexpected(std::move(result274.error()));
    return {};
  });
if (!result345) return std::unexpected(std::move(result345.error()));
  auto stream346 = (ds << m_stemConfig);
if (!stream346) return std::unexpected(std::move(stream346.error()));
  auto stream347 = (ds << m_foliageConfig);
if (!stream347) return std::unexpected(std::move(stream347.error()));
  auto stream348 = (ds << m_saplingConfig);
if (!stream348) return std::unexpected(std::move(stream348.error()));

return ds.take_data();
}

EntityType PlantDrop::entityType() const {
  return EntityType::PlantDrop;
}

std::expected<void, StarException> PlantDrop::init(World& world, EntityId entityId, EntityMode mode) {
  auto result337 = Entity::init(world, entityId, mode);
if (!result337) return std::unexpected(std::move(result337.error()));
  m_movementController->init(world);

  PolyF collisionPoly = PolyF(RectF::withCenter(m_collisionRect.center(), m_collisionRect.size() / 2.0f));
  MovementParameters parameters;
  parameters.collisionPoly = collisionPoly;
  parameters.ignorePlatformCollision = true;
  parameters.gravityMultiplier = 0.2f;
  parameters.physicsEffectCategories = StringSet({"plantdrop"});
  auto result338 = m_movementController->applyParameters(parameters);
if (!result338) return std::unexpected(std::move(result338.error()));
  return {};
}

std::expected<void, StarException> PlantDrop::uninit() {
  auto result336 = Entity::uninit();
if (!result336) return std::unexpected(std::move(result336.error()));
  m_movementController->uninit();
  return {};
}

std::expected<String, StarException> PlantDrop::description() const {
  return m_description;
}

Vec2F PlantDrop::position() const {
  return m_movementController->position();
}

RectF PlantDrop::metaBoundBox() const {
  return m_boundingBox;
}

RectF PlantDrop::collisionRect() const {
  PolyF shape = PolyF(m_collisionRect);
  shape.rotate(m_movementController->rotation());
  return shape.boundBox();
}

std::expected<void, StarException> PlantDrop::update(float dt, std::uint64_t) {
  capturePresentationRotation(m_movementController->rotation());
  m_time -= dt;

  m_movementController->setTimestep(dt);
  if (isMaster()) {
    if (m_spawnedDropEffects && !m_spawnedDrops.get())
      m_spawnedDropEffects = false; // false positive assumption over already having done the effect
    // to avoid effects for newly joining players.
    if (m_spawnedDrops.get())
      m_firstTick = false;

    // think up a better curve then sin
    auto result307 = world();
if (!result307) return std::unexpected(std::move(result307.error()));
auto result308 = result307->get().gravity(position());
if (!result308) return std::unexpected(std::move(result308.error()));
auto rotationAcceleration = 0.01f * std::move(*result308) * std::copysign(1.0f, m_rotationRate) * dt;
    if (std::abs(m_movementController->rotation()) > m_rotationCap)
      m_rotationRate -= rotationAcceleration;
    else if (std::fabs(m_movementController->rotation()) < m_rotationFallThreshold)
      m_rotationRate += rotationAcceleration;

    m_presentationRotationTravel = m_rotationRate * dt;
    m_movementController->rotate(m_rotationRate);

    PolyF collisionPoly = PolyF(RectF::withCenter(m_collisionRect.center(), m_collisionRect.size() / 2.0f));

    if (m_time > 0) {
      MovementParameters parameters;
      parameters.collisionPoly = collisionPoly;
      parameters.gravityEnabled = std::fabs(m_movementController->rotation()) >= m_rotationFallThreshold;
      auto result309 = m_movementController->applyParameters(parameters);
if (!result309) return std::unexpected(std::move(result309.error()));

      auto result310 = m_movementController->tickMaster(dt);
if (!result310) return std::unexpected(std::move(result310.error()));
      if (m_movementController->onGround())
        m_time = 0;
    }

    auto result311 = this->root().imageMetadataDatabase();
if (!result311) return std::unexpected(std::move(result311.error()));
auto imgMetadata = std::move(*result311);

    bool condition314 = static_cast<bool>(m_time <= 0);
if (!condition314) {
auto result312 = world();
if (!result312) return std::unexpected(std::move(result312.error()));
auto result313 = result312->get().gravity(position());
if (!result313) return std::unexpected(std::move(result313.error()));
condition314 = static_cast<bool>(std::move(*result313) == 0);
}
if ((condition314) && !m_spawnedDrops.get()) {
      m_spawnedDrops.set(true);
      for (auto& plantPiece : m_pieces) {
        JsonArray dropOptions;
        if (plantPiece.kind == Plant::PlantPieceKind::Stem)
          {
auto result315 = m_stemConfig.getArray("drops", {});
if (!result315) return std::unexpected(std::move(result315.error()));
dropOptions = std::move(*result315);
}
        if (plantPiece.kind == Plant::PlantPieceKind::Foliage)
          {
auto result316 = m_foliageConfig.getArray("drops", {});
if (!result316) return std::unexpected(std::move(result316.error()));
dropOptions = std::move(*result316);
}
        if (dropOptions.size()) {
          auto result317 = this->root().random().randFrom(dropOptions);
if (!result317) return std::unexpected(std::move(result317.error()));
auto result318 = result317->get().toArray();
if (!result318) return std::unexpected(std::move(result318.error()));
auto option = std::move(*result318);
for (auto const& drop : option) {
  auto result319 = imgMetadata->imageSize(plantPiece.image);
  if (!result319)
    return std::unexpected(std::move(result319.error()));
  auto size = std::move(*result319);
  auto result320 = this->root().random().randf(-0.2f, 0.2f);
  if (!result320)
    return std::unexpected(std::move(result320.error()));
  auto result321 = this->root().random().randf(-0.2f, 0.2f);
  if (!result321)
    return std::unexpected(std::move(result321.error()));
  Vec2F pos = Vec2F(plantPiece.offset + Vec2F(size) * .5f / TilePixels).rotate(m_movementController->rotation())
    + Vec2F(std::move(*result320), std::move(*result321));
  auto result322 = drop.getString("item");
  if (!result322)
    return std::unexpected(std::move(result322.error()));
  if (std::move(*result322) == "sapling") {
    auto result323 = world();
    if (!result323)
      return std::unexpected(std::move(result323.error()));
    auto result324 = drop.getInt("count", 1);
    if (!result324)
      return std::unexpected(std::move(result324.error()));
    auto result325 =
      ItemDescriptor::create("sapling", static_cast<std::size_t>(std::move(*result324)), m_saplingConfig);
    if (!result325)
      return std::unexpected(std::move(result325.error()));
    auto result326 = ItemDrop::createRandomizedDrop(this->root(), std::move(*result325), position() + pos);
    if (!result326)
      return std::unexpected(std::move(result326.error()));
    auto result327 = result323->get().addEntity(std::move(*result326));
    if (!result327)
      return std::unexpected(std::move(result327.error()));
  } else {
    auto result328 = world();
    if (!result328)
      return std::unexpected(std::move(result328.error()));
    auto result329 = drop.getString("item");
    if (!result329)
      return std::unexpected(std::move(result329.error()));
    auto result330 = drop.getInt("count", 1);
    if (!result330)
      return std::unexpected(std::move(result330.error()));
    auto result331 = ItemDescriptor::create(std::move(*result329), static_cast<std::size_t>(std::move(*result330)));
    if (!result331)
      return std::unexpected(std::move(result331.error()));
    auto result332 = ItemDrop::createRandomizedDrop(this->root(), std::move(*result331), position() + pos);
    if (!result332)
      return std::unexpected(std::move(result332.error()));
    auto result333 = result328->get().addEntity(std::move(*result332));
    if (!result333)
      return std::unexpected(std::move(result333.error()));
  }
}
        }
      }
    }
  } else {
    auto result334 = m_netGroup.tickNetInterpolation(dt);
if (!result334) return std::unexpected(std::move(result334.error()));

    if (m_spawnedDropEffects && !m_spawnedDrops.get())
      m_spawnedDropEffects = false; // false positive assumption over already having done the effect
    // to avoid effects for newly joining players.
    if (m_spawnedDrops.get())
      m_firstTick = false;

    auto result335 = m_movementController->tickSlave(dt);
if (!result335) return std::unexpected(std::move(result335.error()));
  }
  return {};
}


std::expected<void, StarException> PlantDrop::particleForPlantPart(PlantDropPiece const& piece, String const& mode, Json const& mainConfig, RenderCallback& renderCallback) {
  auto result295 = mainConfig.get("particles", JsonObject());
if (!result295) return std::unexpected(std::move(result295.error()));
auto result296 = std::move(*result295).get(mode, JsonObject());
if (!result296) return std::unexpected(std::move(result296.error()));
Json particleConfig = std::move(*result296);
  auto result297 = particleConfig.getArray("options", {});
if (!result297) return std::unexpected(std::move(result297.error()));
JsonArray particleOptions = std::move(*result297);
  if (!particleOptions.size())
    return {};

  Particle particle;

  auto result298 = this->root().imageMetadataDatabase();
if (!result298) return std::unexpected(std::move(result298.error()));
auto imgMetadata = std::move(*result298);

  auto result299 = imgMetadata->imageSize(piece.image);
if (!result299) return std::unexpected(std::move(result299.error()));
Vec2F imageSize = Vec2F(std::move(*result299)) / TilePixels;
  auto result300 = particleConfig.getFloat("density", 1);
if (!result300) return std::unexpected(std::move(result300.error()));
float density = (imageSize.x() * imageSize.y()) / std::move(*result300);

  auto result301 = imgMetadata->imageSpaces(piece.image, piece.offset * TilePixels, Plant::PlantScanThreshold, piece.flip);
if (!result301) return std::unexpected(std::move(result301.error()));
auto spaces = Set<Vec2I>::from(std::move(*result301));
  if (spaces.empty())
    return {};

  while (density > 0) {
    Vec2F particlePos = piece.offset + Vec2F(imageSize) / 2.0f + Vec2F(this->root().random().nrandf(imageSize.x() / 8.0f, 0), this->root().random().nrandf(imageSize.y() / 8.0f, 0));

    if (!spaces.contains(Vec2I(particlePos.floor())))
      continue;

    auto config = this->root().random().randValueFrom(particleOptions, {});

    auto result302 = this->root().particleDatabase();
if (!result302) return std::unexpected(std::move(result302.error()));
auto result303 = std::move(*result302)->particle(config);
if (!result303) return std::unexpected(std::move(result303.error()));
particle = std::move(*result303);
    auto result304 = mainConfig.getFloat("hueshift", 0);
if (!result304) return std::unexpected(std::move(result304.error()));
particle.color.hueShift(std::move(*result304) / 360.0f);
    for (Directives const& directives : piece.image.directives.list())
      particle.directives.append(directives);

    density--;

    particle.position = position() + particlePos.rotate(m_movementController->rotation());

    auto result305 = renderCallback.addParticle(particle);
if (!result305) return std::unexpected(std::move(result305.error()));
  }
  return {};
}

std::uint64_t PlantDrop::presentationEpoch() const {
  return Entity::presentationEpoch() + m_movementController->presentationEpoch();
}

std::expected<void, StarException> PlantDrop::collectRenderEvents(RenderCallback& renderCallback) {
  auto result284 = this->root().assets();
if (!result284) return std::unexpected(std::move(result284.error()));
auto assets = std::move(*result284);

  if (m_firstTick) {
    m_firstTick = false;
    // smoke, particles

    if (m_master) {
      auto playBreakSound = [&self = *this, &assets, &renderCallback](Json const& config)  -> std::expected<void, StarException> {
        auto result263 = config.get("sounds", JsonObject());
if (!result263) return std::unexpected(std::move(result263.error()));
auto result264 = std::move(*result263).getArray("breakTree", JsonArray());
if (!result264) return std::unexpected(std::move(result264.error()));
JsonArray breakTreeOptions = std::move(*result264);
        if (breakTreeOptions.size()) {
          auto result265 = self.root().random().randFrom(breakTreeOptions);
if (!result265) return std::unexpected(std::move(result265.error()));
auto sound = result265->get();
          auto result266 = sound.getString("file");
if (!result266) return std::unexpected(std::move(result266.error()));
auto result267 = assets->audio(std::move(*result266));
if (!result267) return std::unexpected(std::move(result267.error()));
auto result1279 = AudioInstance::create(*std::move(*result267));
if (!result1279) return std::unexpected(std::move(result1279.error()));
auto audioInstance = std::move(*result1279);
          audioInstance->setPosition(self.collisionRect().center() + self.position());
          auto result268 = sound.getFloat("volume", 1.0f);
if (!result268) return std::unexpected(std::move(result268.error()));
audioInstance->setVolume(std::move(*result268));
          auto result269 = renderCallback.addAudio(std::move(audioInstance));
if (!result269) return std::unexpected(std::move(result269.error()));
        }
        return {};
};
      auto result285 = playBreakSound(m_stemConfig);
if (!result285) return std::unexpected(std::move(result285.error()));
      auto result286 = playBreakSound(m_foliageConfig);
if (!result286) return std::unexpected(std::move(result286.error()));
    }

    for (auto const& plantPiece : m_pieces) {
      if (plantPiece.kind == Plant::PlantPieceKind::Stem)
        {
auto result287 = particleForPlantPart(plantPiece, "breakTree", m_stemConfig, renderCallback);
if (!result287) return std::unexpected(std::move(result287.error()));
}
      if (plantPiece.kind == Plant::PlantPieceKind::Foliage)
        {
auto result288 = particleForPlantPart(plantPiece, "breakTree", m_foliageConfig, renderCallback);
if (!result288) return std::unexpected(std::move(result288.error()));
}
    }
  }

  if (m_spawnedDrops.get() && !m_spawnedDropEffects) {
    m_spawnedDropEffects = true;
    // smoke, particles

    auto playHitSound = [&self = *this, &assets, &renderCallback](Json const& config)  -> std::expected<void, StarException> {
      auto result256 = config.get("sounds", JsonObject());
if (!result256) return std::unexpected(std::move(result256.error()));
auto result257 = std::move(*result256).getArray("hitGround", JsonArray());
if (!result257) return std::unexpected(std::move(result257.error()));
JsonArray hitGroundOptions = std::move(*result257);
      if (hitGroundOptions.size()) {
        auto result258 = self.root().random().randFrom(hitGroundOptions);
if (!result258) return std::unexpected(std::move(result258.error()));
auto sound = result258->get();
        auto result259 = sound.getString("file");
if (!result259) return std::unexpected(std::move(result259.error()));
auto result260 = assets->audio(std::move(*result259));
if (!result260) return std::unexpected(std::move(result260.error()));
auto result1278 = AudioInstance::create(*std::move(*result260));
if (!result1278) return std::unexpected(std::move(result1278.error()));
auto audioInstance = std::move(*result1278);
        audioInstance->setPosition(self.collisionRect().center() + self.position());
        auto result261 = sound.getFloat("volume", 1.0f);
if (!result261) return std::unexpected(std::move(result261.error()));
audioInstance->setVolume(std::move(*result261));
        auto result262 = renderCallback.addAudio(std::move(audioInstance));
if (!result262) return std::unexpected(std::move(result262.error()));
      }
      return {};
};
    auto result289 = playHitSound(m_stemConfig);
if (!result289) return std::unexpected(std::move(result289.error()));
    auto result290 = playHitSound(m_foliageConfig);
if (!result290) return std::unexpected(std::move(result290.error()));

    for (auto const& plantPiece : m_pieces) {
      if (plantPiece.kind == Plant::PlantPieceKind::Stem)
        {
auto result291 = particleForPlantPart(plantPiece, "hitGround", m_stemConfig, renderCallback);
if (!result291) return std::unexpected(std::move(result291.error()));
}
      if (plantPiece.kind == Plant::PlantPieceKind::Foliage)
        {
auto result292 = particleForPlantPart(plantPiece, "hitGround", m_foliageConfig, renderCallback);
if (!result292) return std::unexpected(std::move(result292.error()));
}
    }
  }

  return {};
}

std::expected<void, StarException> PlantDrop::render(RenderCallback& renderCallback) {
  float alpha = renderCallback.presentation().interpolation();
  float rotation = isMaster()
    ? m_movementController->rotation() - m_presentationRotationTravel * (1.0f - alpha)
    : samplePresentationRotation(m_movementController->rotation(), alpha);
  if (m_time > 0 && !m_spawnedDrops.get()) {
    for (auto const& plantPiece : m_pieces) {
      auto result293 = this->root().imageMetadataDatabase();
if (!result293) return std::unexpected(std::move(result293.error()));
auto result1284 = Drawable::makeImage(*std::move(*result293), plantPiece.image, 1.0f / TilePixels, false, plantPiece.offset);
if (!result1284) return std::unexpected(std::move(result1284.error()));
auto drawable = std::move(*result1284);
      if (plantPiece.flip)
        drawable.scale(Vec2F(-1, 1));
      drawable.rotate(rotation);
      drawable.translate(position());
      auto result294 = renderCallback.addDrawable(std::move(drawable), RenderLayerPlantDrop);
if (!result294) return std::unexpected(std::move(result294.error()));
    }
  }
  return {};
}

MovementController& PlantDrop::movementController() {
  return *m_movementController;
}

std::expected<std::pair<ByteArray, std::uint64_t>, StarException> PlantDrop::writeNetState(std::uint64_t fromVersion, NetCompatibilityRules rules) {
  auto result283 = m_netGroup.writeNetState(fromVersion, rules);
if (!result283) return std::unexpected(std::move(result283.error()));
return std::move(*result283);
}

std::expected<void, StarException> PlantDrop::readNetState(ByteArray data, float interpolationTime, NetCompatibilityRules rules) {
  auto result282 = m_netGroup.readNetState(data, interpolationTime, rules);
if (!result282) return std::unexpected(std::move(result282.error()));
  return {};
}

std::expected<void, StarException> PlantDrop::enableInterpolation(float extrapolationHint) {
  auto result281 = m_netGroup.enableNetInterpolation(extrapolationHint);
if (!result281) return std::unexpected(std::move(result281.error()));
  return {};
}

std::expected<void, StarException> PlantDrop::disableInterpolation() {
  auto result280 = m_netGroup.disableNetInterpolation();
if (!result280) return std::unexpected(std::move(result280.error()));
  return {};
}

std::expected<bool, StarException> PlantDrop::shouldDestroy() const {
  return m_time <= 0.0f;
}

}

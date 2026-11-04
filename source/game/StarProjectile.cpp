import star.memory;
import star.json;
import star.json_extra;
import star.id_map;
import star.poly;
import star.list;
import star.bi_map;
import star.data_stream;
import star.casting;
import star.vector;
import star.game_types;
import star.liquid_types;
import star.material_types;
import star.strong_typedef;
import star.rpc_promise;
import star.logging;
import star.data_stream_extra;
import star.parametric_function;
import star.multi_array_interpolator;
import star.multi_table;
import star.ordered_map;
import star.lua;
import star.color;
import star.rect;
import star.image_processing;
import star.hash;
import star.string_view;
import star.thread;
import star.directives;
import star.asset_path;
import star.io_device;
import star.ref_ptr;
import star.listener;
import std;
import star.format;
import star.version;
import star.lru_cache;
import star.time;
import star.random;
import star.ttl_cache;
import star.audio;
import star.map;
import star.set;
import star.string;
import star.a_star;
import star.periodic_function;

import star.collision_block;
import star.item_descriptor;
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
import star.mixer;
import star.drawable;
import star.entity_rendering_types;
import star.entity_rendering;
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
import star.chat_action;
import star.chatty_entity;
import star.lua_animation_component;
import star.lua_actor_movement_component;
import star.actor_entity;
import star.monster_database;
import star.monster;
import star.item_drop;
import star.stored_functions;

import star.liquids_database;
import star.projectile_database;
import star.config_lua_bindings;
import star.entity_lua_bindings;
import star.movement_controller_lua_bindings;
import star.particle_database;
import star.material_database;


import star.data_stream_devices;

import star.net_compatibility;
import star.byte_array;
import star.line;
import star.math_common;
namespace Star {

Projectile::Projectile(Root& rootContext) : Entity(rootContext), m_scriptComponent(rootContext) {}


std::expected<std::shared_ptr<Projectile>, StarException> Projectile::create(Root& rootContext, std::shared_ptr<ProjectileConfig> const& config, Json const& parameters) {
  struct MakeShared final : Projectile {
    explicit MakeShared(Root& root) : Entity(root), Projectile(root) {}
  };
  auto entity = std::make_shared<MakeShared>(rootContext);

  if (auto result = entity->initialize(config, parameters); !result) return std::unexpected(std::move(result.error()));
  return entity;
}

std::expected<void, StarException> Projectile::initialize(std::shared_ptr<ProjectileConfig> const& config, Json const& parameters) {
  m_config = config;
  m_parameters = parameters;

  auto result667 = setup();
if (!result667) return std::unexpected(std::move(result667.error()));
  return {};
}

std::expected<std::shared_ptr<Projectile>, StarException> Projectile::create(Root& rootContext, std::shared_ptr<ProjectileConfig> const& config, DataStreamBuffer& data, NetCompatibilityRules rules) {
  struct MakeShared final : Projectile {
    explicit MakeShared(Root& root) : Entity(root), Projectile(root) {}
  };
  auto entity = std::make_shared<MakeShared>(rootContext);

  if (auto result = entity->initialize(config, data, rules); !result) return std::unexpected(std::move(result.error()));
  return entity;
}

std::expected<void, StarException> Projectile::initialize(std::shared_ptr<ProjectileConfig> const& config, DataStreamBuffer& data, NetCompatibilityRules) {
  m_config = config;
  auto result660 = data.read(m_parameters);
if (!result660) return std::unexpected(std::move(result660.error()));
  auto result661 = setup();
if (!result661) return std::unexpected(std::move(result661.error()));

  auto result662 = data.readVlqI();
if (!result662) return std::unexpected(std::move(result662.error()));
EntityId sourceEntity = std::move(*result662);
  auto result663 = data.read<bool>();
if (!result663) return std::unexpected(std::move(result663.error()));
bool trackSourceEntity = std::move(*result663);
  setSourceEntity(sourceEntity, trackSourceEntity);

  auto result664 = data.read(m_initialSpeed);
if (!result664) return std::unexpected(std::move(result664.error()));
  auto result665 = data.read(m_powerMultiplier);
if (!result665) return std::unexpected(std::move(result665.error()));
  auto result666 = data.read<EntityDamageTeam>();
if (!result666) return std::unexpected(std::move(result666.error()));
setTeam(std::move(*result666));
  return {};
}

std::expected<ByteArray, StarException> Projectile::netStore(NetCompatibilityRules rules) const {
  DataStreamBuffer ds;
  ds.setStreamCompatibilityVersion(rules);

  auto result977 = ds.write(m_config->typeName);
if (!result977) return std::unexpected(std::move(result977.error()));
  auto result978 = ds.write(m_parameters);
if (!result978) return std::unexpected(std::move(result978.error()));

  auto result979 = ds.viwrite(m_sourceEntity);
if (!result979) return std::unexpected(std::move(result979.error()));
  auto result980 = ds.write(m_trackSourceEntity);
if (!result980) return std::unexpected(std::move(result980.error()));

  auto result981 = ds.write(m_initialSpeed);
if (!result981) return std::unexpected(std::move(result981.error()));
  auto result982 = ds.write(m_powerMultiplier);
if (!result982) return std::unexpected(std::move(result982.error()));
  auto result983 = ds.write(getTeam());
if (!result983) return std::unexpected(std::move(result983.error()));

  return ds.takeData();
}

EntityType Projectile::entityType() const {
  return EntityType::Projectile;
}

std::expected<Json, StarException> Projectile::configValue(String const& name, Json const& def) const {
  auto result975 = m_config->config.query(name, def);
if (!result975) return std::unexpected(std::move(result975.error()));
auto result976 = m_parameters.query(name, std::move(*result975));
if (!result976) return std::unexpected(std::move(result976.error()));
return std::move(*result976);
}

std::expected<void, StarException> Projectile::init(World& world, EntityId entityId, EntityMode mode) {
  auto result948 = Entity::init(world, entityId, mode);
if (!result948) return std::unexpected(std::move(result948.error()));
  m_movementController->init(world);
  m_movementController->setIgnorePhysicsEntities({entityId});

  auto result949 = m_parameters.getFloat("timeToLive", m_config->timeToLive);
if (!result949) return std::unexpected(std::move(result949.error()));
m_timeToLive = std::move(*result949);
  setSourceEntity(m_sourceEntity, m_trackSourceEntity);

  m_periodicActions.clear();
  auto result950 = m_parameters.contains("periodicActions");
if (!result950) return std::unexpected(std::move(result950.error()));
if (std::move(*result950)) {
    auto result951 = m_parameters.getArray("periodicActions", {});
if (!result951) return std::unexpected(std::move(result951.error()));
for (auto const& c : std::move(*result951))
      {
auto result952 = c.getFloat("time");
if (!result952) return std::unexpected(std::move(result952.error()));
auto result953 = c.getBool("repeat", true);
if (!result953) return std::unexpected(std::move(result953.error()));
m_periodicActions.append(std::make_tuple(GameTimer(std::move(*result952)), std::move(*result953), c));
}
  } else {
    for (auto const& [time, repeat, action] : m_config->periodicActions)
      m_periodicActions.append(std::make_tuple(GameTimer(time), repeat, action));
  }

  if (isMaster()) {
    auto result957 = m_parameters.get("scripts", Json());
if (!result957) return std::unexpected(std::move(result957.error()));
auto result958 = jsonToOptional<StringList>(std::move(*result957), jsonToStringList);
if (!result958) return std::unexpected(std::move(result958.error()));
auto parameterScripts = std::move(*result958);
    auto scripts = parameterScripts ? *parameterScripts : m_config->scripts;
    if (!scripts.empty()) {
      auto result959 = m_scriptComponent.setScripts(scripts);
if (!result959) return std::unexpected(std::move(result959.error()));
      auto result960 = m_config->config.getUInt("scriptDelta", 1);
if (!result960) return std::unexpected(std::move(result960.error()));
auto result961 = m_parameters.getUInt("scriptDelta", std::move(*result960));
if (!result961) return std::unexpected(std::move(result961.error()));
m_scriptComponent.setUpdateDelta(std::move(*result961));

      auto result962 = makeProjectileCallbacks();
if (!result962) return std::unexpected(std::move(result962.error()));
auto result963 = m_scriptComponent.addCallbacks("projectile", std::move(*result962));
if (!result963) return std::unexpected(std::move(result963.error()));
      auto result964 = LuaBindings::makeConfigCallbacks(std::bind(&Projectile::configValue, std::cref(*this), std::placeholders::_1, std::placeholders::_2));
if (!result964) return std::unexpected(std::move(result964.error()));
auto result965 = m_scriptComponent.addCallbacks("config", std::move(*result964));
if (!result965) return std::unexpected(std::move(result965.error()));
      auto result966 = LuaBindings::makeEntityCallbacks(*this);
if (!result966) return std::unexpected(std::move(result966.error()));
auto result967 = m_scriptComponent.addCallbacks("entity", std::move(*result966));
if (!result967) return std::unexpected(std::move(result967.error()));
      auto result968 = LuaBindings::makeMovementControllerCallbacks(*m_movementController);
if (!result968) return std::unexpected(std::move(result968.error()));
auto result969 = m_scriptComponent.addCallbacks("mcontroller", std::move(*result968));
if (!result969) return std::unexpected(std::move(result969.error()));
      auto result970 = m_scriptComponent.init(world);
if (!result970) return std::unexpected(std::move(result970.error()));
    }
  }
  m_travelLine = Line2F(position(), position());

  auto result971 = m_parameters.get("referenceVelocity", Json());
if (!result971) return std::unexpected(std::move(result971.error()));
auto referenceVelocity = std::move(*result971);
  if (!referenceVelocity.isNull()) {
auto result972 = jsonToVec2F(referenceVelocity);
if (!result972) return std::unexpected(std::move(result972.error()));
setReferenceVelocity(std::move(*result972));
}

  if (world.isClient() && !m_persistentAudioFile.empty()) {
    auto result973 = this->root().assets();
if (!result973) return std::unexpected(std::move(result973.error()));
auto result974 = std::move(*result973)->audio(m_persistentAudioFile);
if (!result974) return std::unexpected(std::move(result974.error()));
auto result1276 = AudioInstance::create(*std::move(*result974));
if (!result1276) return std::unexpected(std::move(result1276.error()));
m_persistentAudio = std::move(*result1276);
    m_persistentAudio->setLoops(-1);
    m_persistentAudio->setPosition(position());
    m_pendingRenderables.append(m_persistentAudio);
  }
  return {};
}

std::expected<void, StarException> Projectile::uninit() {
  if (m_persistentAudio)
    m_persistentAudio->stop();
  m_movementController->uninit();
  if (isMaster()) {
    auto result940 = configValue("scripts", Json());
if (!result940) return std::unexpected(std::move(result940.error()));
auto result941 = jsonToOptional<StringList>(std::move(*result940), jsonToStringList);
if (!result941) return std::unexpected(std::move(result941.error()));
auto scripts = std::move(*result941);
    if (scripts && !(*scripts).empty()) {
      auto result942 = m_scriptComponent.uninit();
if (!result942) return std::unexpected(std::move(result942.error()));
      auto result943 = m_scriptComponent.removeCallbacks("projectile");
if (!result943) return std::unexpected(std::move(result943.error()));
      auto result944 = m_scriptComponent.removeCallbacks("config");
if (!result944) return std::unexpected(std::move(result944.error()));
      auto result945 = m_scriptComponent.removeCallbacks("entity");
if (!result945) return std::unexpected(std::move(result945.error()));
      auto result946 = m_scriptComponent.removeCallbacks("mcontroller");
if (!result946) return std::unexpected(std::move(result946.error()));
    }
  }
  auto result947 = Entity::uninit();
if (!result947) return std::unexpected(std::move(result947.error()));
  return {};
}

String Projectile::typeName() const {
  return m_config->typeName;
}

std::expected<String, StarException> Projectile::name() const {
  return m_config->typeName;
}

std::expected<String, StarException> Projectile::description() const {
  return m_config->description;
}

Vec2F Projectile::position() const {
  return m_movementController->position();
}

RectF Projectile::metaBoundBox() const {
  return m_config->boundBox;
}

Vec2F Projectile::velocity() const {
  return m_movementController->velocity();
}

std::expected<std::pair<ByteArray, std::uint64_t>, StarException> Projectile::writeNetState(std::uint64_t fromVersion, NetCompatibilityRules rules) {
  auto result939 = m_netGroup.writeNetState(fromVersion, rules);
if (!result939) return std::unexpected(std::move(result939.error()));
return std::move(*result939);
}

std::expected<void, StarException> Projectile::readNetState(ByteArray data, float interpolationTime, NetCompatibilityRules rules) {
  auto result938 = m_netGroup.readNetState(data, interpolationTime, rules);
if (!result938) return std::unexpected(std::move(result938.error()));
  return {};
}

std::expected<void, StarException> Projectile::enableInterpolation(float extrapolationHint) {
  auto result937 = m_netGroup.enableNetInterpolation(extrapolationHint);
if (!result937) return std::unexpected(std::move(result937.error()));
  return {};
}

std::expected<void, StarException> Projectile::disableInterpolation() {
  auto result936 = m_netGroup.disableNetInterpolation();
if (!result936) return std::unexpected(std::move(result936.error()));
  return {};
}

std::expected<bool, StarException> Projectile::shouldDestroy() const {
  auto result935 = m_scriptComponent.invoke<bool>("shouldDestroy");
if (!result935) return std::unexpected(std::move(result935.error()));
if (auto res = std::move(*result935))
    return *res;
  return m_timeToLive <= 0.0f;
}

std::expected<void, StarException> Projectile::destroy(std::optional<std::reference_wrapper<RenderCallback>> renderCallback) {
  auto result927 = m_parameters.getArray("actionOnReap", m_config->actionOnReap);
if (!result927) return std::unexpected(std::move(result927.error()));
for (auto const& action : std::move(*result927))
    {
auto result928 = processAction(action);
if (!result928) return std::unexpected(std::move(result928.error()));
}

  if (m_collision) {
    auto result929 = m_parameters.getArray("actionOnHit", m_config->actionOnHit);
if (!result929) return std::unexpected(std::move(result929.error()));
for (auto const& action : std::move(*result929))
      {
auto result930 = processAction(action);
if (!result930) return std::unexpected(std::move(result930.error()));
}
  } else {
    auto result931 = m_parameters.getArray("actionOnTimeout", m_config->actionOnTimeout);
if (!result931) return std::unexpected(std::move(result931.error()));
for (auto const& action : std::move(*result931))
      {
auto result932 = processAction(action);
if (!result932) return std::unexpected(std::move(result932.error()));
}
  }

  if (renderCallback)
    {
auto result933 = collectPendingRenderEvents(renderCallback->get());
if (!result933) return std::unexpected(std::move(result933.error()));
}

  auto result934 = m_scriptComponent.invoke("destroy");
if (!result934) return std::unexpected(std::move(result934.error()));
  return {};
}

std::expected<List<DamageSource>, StarException> Projectile::damageSources() const {
  if (m_onlyHitTerrain)
    return {};

  float time_per_frame = m_animationCycle / m_config->frameNumber;
  if ((m_config->intangibleWindup && m_animationTimer < time_per_frame * m_config->windupFrames)
      || (m_config->intangibleWinddown && m_timeToLive < time_per_frame * m_config->winddownFrames))
    return {};

  EntityDamageTeam sourceTeam = getTeam();

  auto statusEffects = m_config->statusEffects;
  auto result921 = m_parameters.getArray("statusEffects", {});
if (!result921) return std::unexpected(std::move(result921.error()));
for (auto const& effect : std::move(*result921)) {
auto result922 = jsonToEphemeralStatusEffect(effect);
if (!result922) return std::unexpected(std::move(result922.error()));
statusEffects.append(std::move(*result922));
}

  auto result923 = m_parameters.getFloat("knockback", m_config->knockback);
if (!result923) return std::unexpected(std::move(result923.error()));
float knockbackMagnitude = std::move(*result923);

  DamageSource::Knockback knockback;
  auto result924 = m_parameters.getBool("knockbackDirectional", m_config->knockbackDirectional);
if (!result924) return std::unexpected(std::move(result924.error()));
if (std::move(*result924))
    knockback = Vec2F::withAngle(m_movementController->rotation()) * knockbackMagnitude;
  else
    knockback = knockbackMagnitude;

  List<DamageSource> res;
  auto addDamageSource = [&, &self = *this](DamageSource::DamageArea damageArea) {
    res.append(DamageSource(self.m_damageType, damageArea, self.m_power * self.m_powerMultiplier, true, self.m_sourceEntity, sourceTeam,
        self.m_damageRepeatGroup, self.m_damageRepeatTimeout, self.m_damageKind, statusEffects, knockback, self.m_rayCheckToSource));
  };

  auto result925 = world();
if (!result925) return std::unexpected(std::move(result925.error()));
  Vec2F positionDelta = result925->get().geometry().diff(m_travelLine.min(), m_travelLine.max());
  static float const MinimumDamageLineDelta = 0.1f;
  bool useDamageLine = positionDelta.magnitudeSquared() >= square(MinimumDamageLineDelta);
  if (useDamageLine)
    addDamageSource(Line2F(positionDelta, Vec2F()));

  if (!m_config->damagePoly.isNull()) {
    PolyF damagePoly = m_config->damagePoly;
    if (m_config->flippable) {
      auto angleSide = getAngleSide(m_movementController->rotation(), true);
      if (angleSide.second == Direction::Left)
        damagePoly.flipHorizontal(0);
      damagePoly.rotate(angleSide.first);
    } else {
      damagePoly.rotate(m_movementController->rotation());
    }
    addDamageSource(damagePoly);
  } else if (!useDamageLine) {
    addDamageSource(PolyF(RectF::withCenter(Vec2F(), Vec2F::filled(MinimumDamageLineDelta))));
  }

  return res;
}

std::expected<void, StarException> Projectile::hitOther(EntityId entity, DamageRequest const&) {
  auto result916 = m_parameters.getBool("piercing", m_config->piercing);
if (!result916) return std::unexpected(std::move(result916.error()));
if (!std::move(*result916)) {
    auto result917 = world();
if (!result917) return std::unexpected(std::move(result917.error()));
auto victimEntity = result917->get().entity(entity);
    if (!victimEntity || (victimEntity->getTeam().type != TeamType::Passive && victimEntity->getTeam().type != TeamType::Environment)) {
      if (victimEntity) {
        auto result918 = victimEntity->hitPoly();
if (!result918) return std::unexpected(std::move(result918.error()));
if (auto hitPoly = std::move(*result918)) {
          auto result919 = world();
if (!result919) return std::unexpected(std::move(result919.error()));
auto geometry = result919->get().geometry();
          Vec2F checkVec = m_movementController->velocity().normalized() * 5;
          Vec2F nearMin = geometry.nearestTo(hitPoly->center(), m_movementController->position() - checkVec);
          if (auto intersection = hitPoly->lineIntersection(Line2F(nearMin, nearMin + checkVec * 2)))
            m_movementController->setPosition(intersection->point);
        }
      }
      m_movementController->setVelocity({0, 0});
      m_collision = true;
      m_timeToLive = 0.0f;
    }
  }
  auto result920 = m_scriptComponent.invoke("hit", entity);
if (!result920) return std::unexpected(std::move(result920.error()));
  return {};
}

std::expected<void, StarException> Projectile::update(float dt, std::uint64_t) {
  capturePresentationRotation(m_movementController->rotation());
  m_previousAnimationTimer = m_animationTimer;
  m_previousTimeToLive = m_timeToLive;
  m_movementController->setTimestep(dt);

  if (isMaster()) {
    m_timeToLive -= dt;
    if (m_timeToLive < 0)
      m_timeToLive = 0;

    m_effectEmitter->addEffectSources("normal", m_config->emitters);

    if (m_referenceVelocity)
      m_movementController->setVelocity(m_movementController->velocity() - *m_referenceVelocity);

    auto result905 = m_scriptComponent.update(m_scriptComponent.updateDt(dt));
if (!result905) return std::unexpected(std::move(result905.error()));
    m_movementController->accelerate(m_movementController->velocity().normalized() * m_acceleration);

    if (m_referenceVelocity)
      m_movementController->setVelocity(m_movementController->velocity() + *m_referenceVelocity);

    auto result906 = m_movementController->tickMaster(dt);
if (!result906) return std::unexpected(std::move(result906.error()));
    m_travelLine.min() = m_travelLine.max();
    m_travelLine.max() = m_movementController->position();

    auto result907 = tickShared(dt);
if (!result907) return std::unexpected(std::move(result907.error()));

    if (m_trackSourceEntity) {
      auto result908 = world();
if (!result908) return std::unexpected(std::move(result908.error()));
if (auto sourceEntity = result908->get().entity(m_sourceEntity)) {
        Vec2F newEntityPosition = sourceEntity->position();
        m_movementController->translate(newEntityPosition - m_lastEntityPosition);
        m_lastEntityPosition = newEntityPosition;
      } else {
        m_trackSourceEntity = false;
      }
    }

    if (m_movementController->atWorldLimit())
      m_timeToLive = 0.0f;

    if ((m_movementController->isColliding() || m_movementController->stickingDirection()) && !m_movementController->isNullColliding()) {
      if (!m_wasColliding)
        m_collisionEvent.trigger();
      m_wasColliding = true;
    } else {
      m_wasColliding = false;
    }

    if (m_movementController->isColliding()) {
      if (m_movementController->isNullColliding()) {
        // Don't trigger collision action, just silently die if we collide with a
        // null block.
        m_timeToLive = 0.0f;
      } else if (m_bounces != 0) {
        auto result909 = m_scriptComponent.invoke("bounce");
if (!result909) return std::unexpected(std::move(result909.error()));
        if (m_bounces > 0)
          --m_bounces;
      } else if (m_falldown && !(m_movementController->onGround() || m_movementController->isCollisionStuck() || m_movementController->stickingDirection())) {
        // Wait til this projectile actually hits the ground before dying

      } else if (!m_movementController->stickingDirection()) {
        m_collision = true;
        m_timeToLive = 0.0f;
        // Move slightly less than one tile unit in the direction that the projectile
        // has most recently moved to find the collision tile.  This is *not* perfect by any means.
        m_collisionTile = Vec2I::floor(m_movementController->position() + m_travelLine.direction() * 0.9f);

        m_lastNonCollidingTile = Vec2I::floor(m_movementController->position());
        for (float i = 0; i < 1.51f; i += 0.5f) {
          auto pos = Vec2I::floor(m_movementController->position() + m_travelLine.direction() * -i);
          auto result910 = world();
if (!result910) return std::unexpected(std::move(result910.error()));
if (result910->get().material(pos, TileLayer::Foreground) == EmptyMaterialId) {
            m_lastNonCollidingTile = pos;
            break;
          }
        }
      }
    }

    if (!m_collision && m_hydrophobic) {
      auto result911 = world();
if (!result911) return std::unexpected(std::move(result911.error()));
auto liquid = result911->get().liquidLevel(Vec2I::floor(position()));
      if (liquid.level > 0.5f) {
        m_collision = true;
        m_timeToLive = 0.0f;
        m_collisionTile = Vec2I::floor(position());
        m_lastNonCollidingTile = m_collisionTile;
      }
    }
  } else {
    auto result912 = m_netGroup.tickNetInterpolation(dt);
if (!result912) return std::unexpected(std::move(result912.error()));
    auto result913 = m_movementController->tickSlave(dt);
if (!result913) return std::unexpected(std::move(result913.error()));
    m_travelLine.min() = m_travelLine.max();
    m_travelLine.max() = m_movementController->position();

    m_timeToLive -= dt;

    auto result914 = tickShared(dt);
if (!result914) return std::unexpected(std::move(result914.error()));
  }

  auto result915 = world();
if (!result915) return std::unexpected(std::move(result915.error()));
if (result915->get().isClient())
    this->root().spatialLogger().logPoly("world", m_movementController->collisionBody(), Color::Red.toRgba());
  return {};
}

std::uint64_t Projectile::presentationEpoch() const {
  return Entity::presentationEpoch() + m_movementController->presentationEpoch();
}

std::expected<void, StarException> Projectile::collectRenderEvents(RenderCallback& renderCallback) {
  auto result900 = collectPendingRenderEvents(renderCallback);
if (!result900) return std::unexpected(std::move(result900.error()));

  if (m_persistentAudio)
    m_persistentAudio->setPosition(position());

  auto result901 = m_effectEmitter->collectRenderEvents(renderCallback);
if (!result901) return std::unexpected(std::move(result901.error()));

  return {};
}

std::expected<void, StarException> Projectile::render(RenderCallback& renderCallback) {
  float rotation = samplePresentationRotation(m_movementController->rotation(), renderCallback.presentation().interpolation());
  float alpha = renderCallback.presentation().interpolation();
  int frame = m_frame;
  if (frame == getFrame())
    frame = frameAt(std::lerp(m_previousAnimationTimer, m_animationTimer, alpha), std::lerp(m_previousTimeToLive, m_timeToLive, alpha));
  String image = strf("{}:{}{}", m_config->image, frame, m_imageSuffix);
  auto result902 = this->root().imageMetadataDatabase();
if (!result902) return std::unexpected(std::move(result902.error()));
auto result1280 = Drawable::makeImage(*std::move(*result902), image, 1.0f / TilePixels, true, Vec2F());
if (!result1280) return std::unexpected(std::move(result1280.error()));
Drawable drawable = std::move(*result1280);
  auto result903 = this->root().imageMetadataDatabase();
if (!result903) return std::unexpected(std::move(result903.error()));
  auto directivesResult = drawable.imagePart().addDirectives(**result903, m_imageDirectives, true);
  if (!directivesResult)
    return std::unexpected(std::move(directivesResult.error()));
  if (m_config->flippable) {
    auto angleSide = getAngleSide(rotation, true);
    if (angleSide.second == Direction::Left)
      drawable.scale(Vec2F(-1, 1));
    drawable.rotate(angleSide.first);
  } else {
    drawable.rotate(rotation);
  }
  drawable.fullbright = m_config->fullbright;
  drawable.translate(position());
  auto result904 = renderCallback.addDrawable(std::move(drawable), m_config->renderLayer);
if (!result904) return std::unexpected(std::move(result904.error()));
  return {};
}

std::expected<void, StarException> Projectile::renderLightSources(RenderCallback& renderCallback) {
  auto lights = renderCallback.addLightSources(m_renderLights);
  if (!lights)
    return std::unexpected(std::move(lights.error()));
  auto result899 = renderCallback.addLightSource({position(), m_config->lightColor.toRgbF(), m_config->lightType, 0.0f, 0.0f, 0.0f});
if (!result899) return std::unexpected(std::move(result899.error()));
  return {};
}

std::expected<std::optional<ChainableJsonMessageResponse>, StarException> Projectile::receiveMessage(ConnectionId sendingConnection, String const& message, JsonArray const& args) {
  auto result896 = world();
if (!result896) return std::unexpected(std::move(result896.error()));
auto result897 = m_scriptComponent.handleMessage(message, sendingConnection == result896->get().connection(), args);
if (!result897) return std::unexpected(std::move(result897.error()));
return std::move(*result897);
}

std::expected<std::optional<LuaValue>, StarException> Projectile::callScript(String const& func, LuaVariadic<LuaValue> const& args) {
  auto result895 = m_scriptComponent.invoke(func, args);
if (!result895) return std::unexpected(std::move(result895.error()));
return std::move(*result895);
}

std::expected<std::optional<LuaValue>, StarException> Projectile::evalScript(String const& code) {
  auto result894 = m_scriptComponent.eval(code);
if (!result894) return std::unexpected(std::move(result894.error()));
return std::move(*result894);
}

String Projectile::projectileType() const {
  return m_config->typeName;
}

void Projectile::setReferenceVelocity(std::optional<Vec2F> const& velocity) {
  m_movementController->setVelocity(m_movementController->velocity() - m_referenceVelocity.value_or(Vec2F{}));
  m_referenceVelocity = velocity;
  m_movementController->setVelocity(m_movementController->velocity() + velocity.value_or(Vec2F{}));
  m_effectEmitter->setBaseVelocity(velocity.value_or(Vec2F{}));
}

float Projectile::initialSpeed() const {
  return m_initialSpeed;
}

void Projectile::setInitialSpeed(float speed) {
  m_initialSpeed = speed;
}

void Projectile::setInitialPosition(Vec2F const& position) {
  resetPresentation();
  m_movementController->setPosition(position);
}

void Projectile::setInitialDirection(Vec2F const& direction) {
  m_movementController->setVelocity(vnorm(direction) * m_initialSpeed + m_referenceVelocity.value_or(Vec2F{}));
  m_movementController->setRotation(direction.angle());
}

void Projectile::setInitialVelocity(Vec2F const& velocity) {
  m_movementController->setVelocity(velocity + m_referenceVelocity.value_or(Vec2F{}));
  m_movementController->setRotation(velocity.angle());
}

void Projectile::setSourceEntity(EntityId source, bool trackSource) {
  m_sourceEntity = source;
  m_trackSourceEntity = trackSource;
  if (auto world = maybeWorld()) {
    if (auto sourceEntity = world->get().entity(source)) {
      m_lastEntityPosition = sourceEntity->position();
      if (!m_damageTeam)
        setTeam(sourceEntity->getTeam());
    } else {
      m_sourceEntity = NullEntityId;
      m_trackSourceEntity = false;
    }
  }
}

float Projectile::powerMultiplier() const {
  return m_powerMultiplier;
}

void Projectile::setPowerMultiplier(float powerMultiplier) {
  m_powerMultiplier = powerMultiplier;
}

EntityId Projectile::sourceEntity() const {
  return m_sourceEntity;
}

std::expected<List<PersistentStatusEffect>, StarException> Projectile::statusEffects() const {
  return m_config->persistentStatusEffects;
}

PolyF Projectile::statusEffectArea() const {
  return m_config->statusEffectArea;
}

std::expected<List<PhysicsForceRegion>, StarException> Projectile::forceRegions() const {
  List<PhysicsForceRegion> forces;
  for (auto const& p : m_physicsForces) {
    if (p.second.enabled.get()) {
      PhysicsForceRegion forceRegion = p.second.forceRegion;
      std::visit([pos = position()](auto& fr) { fr.translate(pos); }, forceRegion);
      forces.append(std::move(forceRegion));
    }
  }
  return forces;
}

std::size_t Projectile::movingCollisionCount() const {
  return m_physicsCollisions.size();
}

std::expected<std::optional<PhysicsMovingCollision>, StarException> Projectile::movingCollision(std::size_t positionIndex) const {
  auto result893 = m_physicsCollisions.valueAt(positionIndex);
if (!result893) return std::unexpected(std::move(result893.error()));
auto const& mc = result893->get();
  if (!mc.enabled.get())
    return {};
  PhysicsMovingCollision collision = mc.movingCollision;
  collision.translate(position());
  return collision;
}

MovementController& Projectile::movementController() {
  return *m_movementController;
}

std::expected<List<Particle>, StarException> Projectile::sparkBlock(Root& rootContext, World& world, Vec2I const& position, Vec2F const& damageSource) {
  auto& root = rootContext;
  auto result885 = root.assets();
if (!result885) return std::unexpected(std::move(result885.error()));
auto assets = std::move(*result885);
  auto result886 = root.materialDatabase();
if (!result886) return std::unexpected(std::move(result886.error()));
auto materialDatabase = std::move(*result886);

  auto result887 = assets->json("/client.config:blockDamageParticle");
if (!result887) return std::unexpected(std::move(result887.error()));
auto result888 = Particle::create(rootContext, std::move(*result887));
if (!result888) return std::unexpected(std::move(result888.error()));
auto blockDamageParticle = std::move(*result888);
  auto result889 = assets->json("/client.config:blockDamageParticleVariance");
if (!result889) return std::unexpected(std::move(result889.error()));
auto result890 = Particle::create(rootContext, std::move(*result889));
if (!result890) return std::unexpected(std::move(result890.error()));
auto blockDamageVariance = std::move(*result890);

  List<Particle> result;
  for (auto layer : {TileLayer::Background, TileLayer::Foreground}) {
    auto material = world.material(position, layer);
    auto hueShift = world.materialHueShift(position, layer);
    if (isRealMaterial(material)) {
      auto particle = blockDamageParticle;
      particle.position += centerOfTile(position);
      particle.velocity = particle.velocity.magnitude() * vnorm(world.geometry().diff(damageSource, particle.position));
      auto result892 = materialDatabase->materialParticleColor(material, hueShift);
if (!result892) return std::unexpected(std::move(result892.error()));
particle.color = std::move(*result892);
      particle.applyVariance(rootContext, blockDamageVariance);

      particle.approach += Vec2F(0.0f, 5.0f);
      particle.velocity += Vec2F(rootContext.random().randf() - 0.5f, 5.0f + rootContext.random().randf());
      particle.velocity += 10.0f * Vec2F(1.0f - 2.0f * rootContext.random().randf(), 1.0f - 2.0f * rootContext.random().randf());
      particle.finalVelocity = 0.5f * (particle.finalVelocity + Vec2F(rootContext.random().randf() - 0.5f, -20.0f + rootContext.random().randf()));
      particle.trail = true;

      result.append(std::move(particle));
    }
  }
  return result;
}

int Projectile::getFrame() const {
  return frameAt(m_animationTimer, m_timeToLive);
}

int Projectile::frameAt(float animationTimer, float timeToLive) const {
  float time_per_frame = m_animationCycle / m_config->frameNumber;

  if (m_config->animationLoops) {
    if (animationTimer < time_per_frame * m_config->windupFrames) {
      return std::floor(animationTimer / time_per_frame);
    } else if (timeToLive < time_per_frame * m_config->winddownFrames) {
      return m_config->windupFrames + m_config->frameNumber
          + clamp<int>((time_per_frame * m_config->winddownFrames - timeToLive) / time_per_frame, 0, m_config->winddownFrames - 1);
    } else {
      float time_within_cycle = std::fmod(animationTimer, m_animationCycle);
      return m_config->windupFrames + std::floor(time_within_cycle / time_per_frame);
    }
  } else {
    return clamp<int>(animationTimer / time_per_frame, 0, m_config->frameNumber - 1);
  }
}

void Projectile::setFrame(int frame) {
  m_frame = frame;
}

std::expected<String, StarException> Projectile::drawableFrame() {
  String str = strf("{}:{}{}", m_config->image, m_frame, m_imageSuffix);
  return m_imageDirectives.addToString(str);
}

bool Projectile::ephemeral() const {
  return true;
}

ClientEntityMode Projectile::clientEntityMode() const {
  return m_config->clientEntityMode;
}

bool Projectile::masterOnly() const {
  return m_config->masterOnly;
}

std::expected<void, StarException> Projectile::processAction(Json const& action) {
  Json parameters;
  String command;

  if (action.type() == Json::Type::Object) {
    parameters = action;
    auto result729 = parameters.getString("action");
if (!result729) return std::unexpected(std::move(result729.error()));
auto result730 = std::move(*result729).toLower();
if (!result730) return std::unexpected(std::move(result730.error()));
command = std::move(*result730);
  } else {
    auto result731 = action.toString();
if (!result731) return std::unexpected(std::move(result731.error()));
auto result732 = std::move(*result731).toLower();
if (!result732) return std::unexpected(std::move(result732.error()));
command = std::move(*result732);
  }

  auto doWithDelay = [&self = *this](int stepsDelay, WorldAction function)  -> std::expected<void, StarException> {
    if (stepsDelay == 0)
      {
auto result657 = self.world();
if (!result657) return std::unexpected(std::move(result657.error()));
auto result658 = function(result657->get());
if (!result658) return std::unexpected(std::move(result658.error()));
}
    else
      {
auto result659 = self.world();
if (!result659) return std::unexpected(std::move(result659.error()));
result659->get().timer((float)stepsDelay / 60.f, function);
}
    return {};
};

  if (command == "tile") {
    if (isSlave())
      return {};

    auto result733 = this->root().materialDatabase();
if (!result733) return std::unexpected(std::move(result733.error()));
auto materialDatabase = std::move(*result733);
    List<MaterialId> tileDrops;
    unsigned totalDrops = 0;
    auto result734 = parameters.getArray("materials");
if (!result734) return std::unexpected(std::move(result734.error()));
for (auto sets : std::move(*result734)) {
      auto result735 = sets.getUInt("quantity", 1);
if (!result735) return std::unexpected(std::move(result735.error()));
unsigned numDrops = std::move(*result735);
      auto result736 = sets.getString("kind");
if (!result736) return std::unexpected(std::move(result736.error()));
auto result737 = materialDatabase->materialId(std::move(*result736));
if (!result737) return std::unexpected(std::move(result737.error()));
auto mat = std::move(*result737);
      for (unsigned i = 0; i < numDrops; i++)
        tileDrops.push_back(mat);
      totalDrops += numDrops;
    }

    auto result738 = world();
if (!result738) return std::unexpected(std::move(result738.error()));
auto result739 = parameters.getInt("radius", 2);
if (!result739) return std::unexpected(std::move(result739.error()));
auto result740 = result738->get().findEmptyTiles(m_lastNonCollidingTile, std::move(*result739), totalDrops);
if (!result740) return std::unexpected(std::move(result740.error()));
List<Vec2I> openSpaces = std::move(*result740);
    if (openSpaces.size() < totalDrops)
      {
auto result741 = this->root().logger().debug("Couldn't find a place for all the tile drops. {} drops requested, {} spaces found.", totalDrops, openSpaces.size());
if (!result741) return std::unexpected(std::move(result741.error()));
}

    auto result742 = parameters.getBool("allowEntityOverlap", true);
if (!result742) return std::unexpected(std::move(result742.error()));
bool allowEntityOverlap = std::move(*result742);

    this->root().random().shuffle(tileDrops);
    for (auto& tile : zip(openSpaces, tileDrops)) {
      auto result743 = world();
if (!result743) return std::unexpected(std::move(result743.error()));
auto result746 = result743->get().modifyTile(std::get<0>(tile), PlaceMaterial{TileLayer::Foreground, std::get<1>(tile), MaterialHue()}, allowEntityOverlap);
if (!result746) return std::unexpected(std::move(result746.error()));
if (!std::move(*result746)) {
auto result748 = materialDatabase->materialItemDrop(std::get<1>(tile));
if (!result748) return std::unexpected(std::move(result748.error()));
auto result750 = ItemDrop::createRandomizedDrop(this->root(), std::move(*result748), Vec2F(std::get<0>(tile)));
if (!result750) return std::unexpected(std::move(result750.error()));
auto itemDrop = std::move(*result750);
        auto result751 = world();
if (!result751) return std::unexpected(std::move(result751.error()));
auto result752 = result751->get().addEntity(itemDrop);
if (!result752) return std::unexpected(std::move(result752.error()));
      }
    }

  } else if (command == "applysurfacemod") {
    if (isSlave())
      return {};

    auto result753 = this->root().materialDatabase();
if (!result753) return std::unexpected(std::move(result753.error()));
auto materialDatabase = std::move(*result753);
    std::optional<ModId> previousMod;
    auto result754 = parameters.optString("previousMod");
if (!result754) return std::unexpected(std::move(result754.error()));
if (auto previousName = std::move(*result754)) {
auto result755 = materialDatabase->modId(*previousName);
if (!result755) return std::unexpected(std::move(result755.error()));
previousMod = std::move(*result755);
}
    auto result756 = parameters.getString("newMod");
if (!result756) return std::unexpected(std::move(result756.error()));
auto result757 = materialDatabase->modId(std::move(*result756));
if (!result757) return std::unexpected(std::move(result757.error()));
ModId newMod = std::move(*result757);
    auto result758 = parameters.getInt("radius", 0);
if (!result758) return std::unexpected(std::move(result758.error()));
int radius = std::move(*result758);
    auto result759 = parameters.getFloat("chance", 1.0f);
if (!result759) return std::unexpected(std::move(result759.error()));
float chance = std::move(*result759);

    Set<TileLayer> layers;
    auto result760 = parameters.getBool("foreground", true);
if (!result760) return std::unexpected(std::move(result760.error()));
if (std::move(*result760))
      layers.add(TileLayer::Foreground);
    auto result761 = parameters.getBool("background", false);
if (!result761) return std::unexpected(std::move(result761.error()));
if (std::move(*result761))
      layers.add(TileLayer::Background);

    for (auto layer : layers) {
      // Go in vertical lines for each column, stop at the first non-emppty
      // material in each column.
      for (int x = m_collisionTile[0] - radius; x <= m_collisionTile[0] + radius; ++x) {
        auto result762 = world();
if (!result762) return std::unexpected(std::move(result762.error()));
if (result762->get().material({x, m_collisionTile[1] + radius + 1}, layer) == EmptyMaterialId) {
          for (int y = m_collisionTile[1] + radius; y >= m_collisionTile[1] - radius; --y) {
            auto result763 = world();
if (!result763) return std::unexpected(std::move(result763.error()));
auto mat = result763->get().material({x, y}, layer);
            if (this->root().random().randf() <= chance) {
              if (isRealMaterial(mat)) {
                auto result764 = world();
if (!result764) return std::unexpected(std::move(result764.error()));
auto mod = result764->get().mod({x, y}, layer);
                if (!previousMod || *previousMod == mod)
                  {
auto result765 = world();
if (!result765) return std::unexpected(std::move(result765.error()));
auto result766 = result765->get().modifyTile({x, y}, PlaceMod{layer, newMod, {}}, true);
if (!result766) return std::unexpected(std::move(result766.error()));
}
              }
            }
            if (mat != EmptyMaterialId)
              break;
          }
        }
      }
    }

  } else if (command == "liquid") {
    if (isSlave())
      return {};

    auto result767 = parameters.getFloat("quantity", 1.0f);
if (!result767) return std::unexpected(std::move(result767.error()));
float waterAmount = std::move(*result767);
    auto result768 = this->root().liquidsDatabase();
if (!result768) return std::unexpected(std::move(result768.error()));
auto result769 = parameters.getString("liquid");
if (!result769) return std::unexpected(std::move(result769.error()));
auto result770 = std::move(*result768)->liquidId(std::move(*result769));
if (!result770) return std::unexpected(std::move(result770.error()));
LiquidId liquid = std::move(*result770);
    auto result771 = world();
if (!result771) return std::unexpected(std::move(result771.error()));
auto result772 = parameters.getInt("radius", 5);
if (!result772) return std::unexpected(std::move(result772.error()));
auto result773 = result771->get().findEmptyTiles(m_lastNonCollidingTile, std::move(*result772), 50);
if (!result773) return std::unexpected(std::move(result773.error()));
auto empty = std::move(*result773);
    for (Vec2I pos : empty) {
      auto result774 = world();
if (!result774) return std::unexpected(std::move(result774.error()));
auto result775 = result774->get().lineTileCollision(Vec2F(pos), Vec2F(m_lastNonCollidingTile));
if (!result775) return std::unexpected(std::move(result775.error()));
if (std::move(*result775))
        continue;

      auto result776 = world();
if (!result776) return std::unexpected(std::move(result776.error()));
auto liquidLevel = result776->get().liquidLevel(pos);
      if (liquidLevel.liquid == EmptyLiquidId || liquidLevel.liquid == liquid) {
        auto result777 = world();
if (!result777) return std::unexpected(std::move(result777.error()));
auto result778 = result777->get().modifyTile(pos, PlaceLiquid{liquid, waterAmount}, true);
if (!result778) return std::unexpected(std::move(result778.error()));
        break;
      }
    }

  } else if (command == "projectile") {
    if (isSlave())
      return {};

    auto result779 = parameters.getString("type");
if (!result779) return std::unexpected(std::move(result779.error()));
String type = std::move(*result779);
    auto result780 = parameters.get("config", JsonObject());
if (!result780) return std::unexpected(std::move(result780.error()));
auto projectileParameters = std::move(*result780);
    auto result781 = projectileParameters.contains("damageTeam");
if (!result781) return std::unexpected(std::move(result781.error()));
if (!std::move(*result781)) {
      if (m_damageTeam)
        {
auto result782 = projectileParameters.set("damageTeam", m_damageTeam);
if (!result782) return std::unexpected(std::move(result782.error()));
projectileParameters = std::move(*result782);
}
    }
    auto result783 = parameters.contains("inheritDamageFactor");
if (!result783) return std::unexpected(std::move(result783.error()));
bool condition785 = static_cast<bool>(std::move(*result783));
if (condition785) {
auto result784 = projectileParameters.contains("power");
if (!result784) return std::unexpected(std::move(result784.error()));
condition785 = static_cast<bool>(!std::move(*result784));
}
if (condition785)
      {
auto result786 = parameters.getFloat("inheritDamageFactor");
if (!result786) return std::unexpected(std::move(result786.error()));
auto result787 = projectileParameters.set("power", m_power * std::move(*result786));
if (!result787) return std::unexpected(std::move(result787.error()));
projectileParameters = std::move(*result787);
}
    auto result788 = parameters.contains("inheritSpeedFactor");
if (!result788) return std::unexpected(std::move(result788.error()));
if (std::move(*result788))
      {
auto result789 = parameters.getFloat("inheritSpeedFactor");
if (!result789) return std::unexpected(std::move(result789.error()));
auto result790 = projectileParameters.set("speed", (m_movementController->velocity() - m_referenceVelocity.value_or(Vec2F{})).magnitude() * std::move(*result789));
if (!result790) return std::unexpected(std::move(result790.error()));
projectileParameters = std::move(*result790);
}

    auto result791 = this->root().projectileDatabase();
if (!result791) return std::unexpected(std::move(result791.error()));
auto result792 = std::move(*result791)->createProjectile(type, projectileParameters);
if (!result792) return std::unexpected(std::move(result792.error()));
auto projectile = std::move(*result792);
    Vec2F offset;
    auto result793 = parameters.contains("offset");
if (!result793) return std::unexpected(std::move(result793.error()));
if (std::move(*result793)) {
      auto result794 = parameters.getArray("offset", {0.0f, 0.0f});
if (!result794) return std::unexpected(std::move(result794.error()));
auto result795 = jsonToVec2F(std::move(*result794));
if (!result795) return std::unexpected(std::move(result795.error()));
offset = std::move(*result795);
    } else {
auto result796 = parameters.contains("offsetRange");
if (!result796) return std::unexpected(std::move(result796.error()));
if (std::move(*result796)) {
      auto result797 = parameters.get("offsetRange");
if (!result797) return std::unexpected(std::move(result797.error()));
auto result798 = jsonToVec4F(std::move(*result797));
if (!result798) return std::unexpected(std::move(result798.error()));
auto offsetRange = std::move(*result798);
      auto result799 = this->root().random().randf(offsetRange[0], offsetRange[2]);
if (!result799) return std::unexpected(std::move(result799.error()));
auto result800 = this->root().random().randf(offsetRange[1], offsetRange[3]);
if (!result800) return std::unexpected(std::move(result800.error()));
offset = Vec2F(std::move(*result799), std::move(*result800));
    }
}
    if (m_referenceVelocity)
      projectile->setReferenceVelocity(m_referenceVelocity);
    projectile->setInitialPosition(position() + offset);
    auto result801 = parameters.contains("direction");
if (!result801) return std::unexpected(std::move(result801.error()));
if (std::move(*result801)) {
      auto result802 = parameters.get("direction");
if (!result802) return std::unexpected(std::move(result802.error()));
auto result803 = jsonToVec2F(std::move(*result802));
if (!result803) return std::unexpected(std::move(result803.error()));
projectile->setInitialDirection(std::move(*result803));
    } else {
      float angle = m_movementController->rotation();
      float angleAdjust = 0;
      auto result804 = parameters.contains("angle");
if (!result804) return std::unexpected(std::move(result804.error()));
if (std::move(*result804))
        {
auto result805 = parameters.getFloat("angle");
if (!result805) return std::unexpected(std::move(result805.error()));
angle = std::move(*result805) * std::numbers::pi / 180.0f;
}
      auto result806 = parameters.contains("fuzzAngle");
if (!result806) return std::unexpected(std::move(result806.error()));
if (std::move(*result806))
        {
auto result807 = this->root().random().randf(-1, 1);
if (!result807) return std::unexpected(std::move(result807.error()));
auto result808 = parameters.getFloat("fuzzAngle");
if (!result808) return std::unexpected(std::move(result808.error()));
angleAdjust += std::move(*result807) * std::move(*result808) * std::numbers::pi / 180.0f;
}
      auto result809 = parameters.contains("angleAdjust");
if (!result809) return std::unexpected(std::move(result809.error()));
if (std::move(*result809))
        {
auto result810 = parameters.getFloat("angleAdjust");
if (!result810) return std::unexpected(std::move(result810.error()));
angleAdjust += std::move(*result810) * std::numbers::pi / 180.0f;
}
      auto result811 = parameters.contains("autoFlipAdjust");
if (!result811) return std::unexpected(std::move(result811.error()));
bool condition813 = static_cast<bool>(std::move(*result811));
if (condition813) {
auto result812 = parameters.getBool("autoFlipAdjust");
if (!result812) return std::unexpected(std::move(result812.error()));
condition813 = static_cast<bool>(std::move(*result812));
}
if (condition813) {
        if (Vec2F::withAngle(m_movementController->rotation())[0] < 0)
          angleAdjust = -angleAdjust;
      }
      auto result814 = parameters.contains("autoFlipAngle");
if (!result814) return std::unexpected(std::move(result814.error()));
bool condition816 = static_cast<bool>(std::move(*result814));
if (condition816) {
auto result815 = parameters.getBool("autoFlipAngle");
if (!result815) return std::unexpected(std::move(result815.error()));
condition816 = static_cast<bool>(std::move(*result815));
}
if (condition816) {
        if (Vec2F::withAngle(m_movementController->rotation())[0] < 0)
          angle = -angle;
      }
      angle += angleAdjust;
      projectile->setInitialDirection(Vec2F::withAngle(angle, 1.0f));
    }
    projectile->setSourceEntity(m_sourceEntity, false);
    projectile->setPowerMultiplier(m_powerMultiplier);
    
    // if the entity no longer exists and no explicit damage team is set, inherit damage team
    bool condition818 = static_cast<bool>(!projectile->m_damageTeam);
if (condition818) {
auto result817 = world();
if (!result817) return std::unexpected(std::move(result817.error()));
condition818 = static_cast<bool>(!result817->get().entity(m_sourceEntity));
}
if (condition818)
      projectile->setTeam(getTeam());

    auto result819 = parameters.getUInt("delaySteps", 0);
if (!result819) return std::unexpected(std::move(result819.error()));
auto result820 = doWithDelay(std::move(*result819), [projectile](World& world)  -> std::expected<void, StarException> { auto result656 = world.addEntity(projectile);
if (!result656) return std::unexpected(std::move(result656.error()));   return {};
});
if (!result820) return std::unexpected(std::move(result820.error()));

  } else if (command == "spark") {
    auto result821 = world();
if (!result821) return std::unexpected(std::move(result821.error()));
if (!result821->get().isClient())
      return {};

    auto result822 = world();
if (!result822) return std::unexpected(std::move(result822.error()));
auto collisionMaterial = result822->get().material(m_collisionTile, TileLayer::Foreground);
    if (!m_collision || collisionMaterial == EmptyMaterialId)
      return {};

    auto result823 = world();
if (!result823) return std::unexpected(std::move(result823.error()));
auto result824 = sparkBlock(this->root(), result823->get(), m_collisionTile, position());
if (!result824) return std::unexpected(std::move(result824.error()));
for (auto& particle : std::move(*result824)) {
      // enable trails and such
      particle.approach += Vec2F(0.0f, 5.0f);
      particle.velocity += Vec2F(this->root().random().randf() - 0.5f, 5.0f + this->root().random().randf());
      particle.velocity += 10.0f * Vec2F(1.0f - 2.0f * this->root().random().randf(), 1.0f - 2.0f * this->root().random().randf());
      particle.finalVelocity = 0.5f * (particle.finalVelocity + Vec2F(this->root().random().randf() - 0.5f, -20.0f + this->root().random().randf()));
      particle.trail = true;

      m_pendingRenderables.append(std::move(particle));
    }

  } else if (command == "particle") {
    auto result825 = world();
if (!result825) return std::unexpected(std::move(result825.error()));
if (!result825->get().isClient())
      return {};

    auto result826 = this->root().particleDatabase();
if (!result826) return std::unexpected(std::move(result826.error()));
auto result827 = parameters.get("specification");
if (!result827) return std::unexpected(std::move(result827.error()));
auto result828 = std::move(*result826)->particle(std::move(*result827));
if (!result828) return std::unexpected(std::move(result828.error()));
Particle particle = std::move(*result828);
    particle.position = particle.position.rotate(m_movementController->rotation());
    auto result829 = parameters.getBool("rotate", false);
if (!result829) return std::unexpected(std::move(result829.error()));
if (std::move(*result829)) {
      particle.rotation = m_movementController->rotation();
      particle.velocity = particle.velocity.rotate(m_movementController->rotation());
    }
    particle.translate(position());
    particle.velocity += m_referenceVelocity.value_or(Vec2F{});
    m_pendingRenderables.append(std::move(particle));

  } else if (command == "explosion") {
    if (isSlave())
      return {};

    auto result830 = parameters.getFloat("foregroundRadius");
if (!result830) return std::unexpected(std::move(result830.error()));
float foregroundRadius = std::move(*result830);
    auto result831 = parameters.getFloat("backgroundRadius");
if (!result831) return std::unexpected(std::move(result831.error()));
float backgroundRadius = std::move(*result831);
    auto result832 = parameters.getFloat("explosiveDamageAmount");
if (!result832) return std::unexpected(std::move(result832.error()));
float explosiveDamageAmount = std::move(*result832);
    auto result833 = parameters.getString("tileDamageType", "explosive");
if (!result833) return std::unexpected(std::move(result833.error()));
if (!TileDamageTypeNames) return std::unexpected(TileDamageTypeNames.error());
auto result834 = TileDamageTypeNames->getLeft(std::move(*result833));
if (!result834) return std::unexpected(std::move(result834.error()));
auto damageType = result834->get();
    auto result835 = parameters.getUInt("harvestLevel", 0);
if (!result835) return std::unexpected(std::move(result835.error()));
unsigned harvestLevel = std::move(*result835);
    Vec2F explosionPosition = position();

    auto result836 = parameters.getUInt("delaySteps", 0);
if (!result836) return std::unexpected(std::move(result836.error()));
auto result837 = doWithDelay(std::move(*result836),
        [foregroundRadius, backgroundRadius, explosionPosition, damageType,
            explosiveDamageAmount, harvestLevel, sourceEntityId = sourceEntity()](World& world)  -> std::expected<void, StarException> {
        auto result654 = world.damageTiles(tileAreaBrush(foregroundRadius, explosionPosition, false),
            TileLayer::Foreground,
            explosionPosition,
            {damageType, explosiveDamageAmount, harvestLevel},
            sourceEntityId);
if (!result654) return std::unexpected(std::move(result654.error()));
        auto result655 = world.damageTiles(tileAreaBrush(backgroundRadius, explosionPosition, false),
            TileLayer::Background,
            explosionPosition,
            {damageType, explosiveDamageAmount, harvestLevel},
            sourceEntityId);
if (!result655) return std::unexpected(std::move(result655.error()));
        return {};
});
if (!result837) return std::unexpected(std::move(result837.error()));

  } else if (command == "spawnmonster") {
    if (isMaster()) {
      auto result838 = parameters.getString("type");
if (!result838) return std::unexpected(std::move(result838.error()));
String const type = std::move(*result838);

      auto result839 = parameters.getObject("arguments", JsonObject{});
if (!result839) return std::unexpected(std::move(result839.error()));
JsonObject arguments = std::move(*result839);

      auto result840 = m_parameters.getFloat("level", 0.0f);
if (!result840) return std::unexpected(std::move(result840.error()));
auto result841 = parameters.getFloat("level", std::move(*result840));
if (!result841) return std::unexpected(std::move(result841.error()));
float level = std::move(*result841);

      auto result842 = this->root().monsterDatabase();
if (!result842) return std::unexpected(std::move(result842.error()));
auto monsterDatabase = std::move(*result842);
      auto result843 = monsterDatabase->randomMonster(type, arguments);
if (!result843) return std::unexpected(std::move(result843.error()));
auto result844 = monsterDatabase->createMonster(std::move(*result843), level);
if (!result844) return std::unexpected(std::move(result844.error()));
auto monster = std::move(*result844);

      auto spawnPosition = position();
      auto result845 = parameters.contains("offset");
if (!result845) return std::unexpected(std::move(result845.error()));
if (std::move(*result845))
        {
auto result846 = parameters.get("offset");
if (!result846) return std::unexpected(std::move(result846.error()));
auto result847 = jsonToVec2F(std::move(*result846));
if (!result847) return std::unexpected(std::move(result847.error()));
spawnPosition += std::move(*result847);
}
      monster->setPosition(spawnPosition);
      auto result848 = world();
if (!result848) return std::unexpected(std::move(result848.error()));
auto result849 = result848->get().addEntity(monster);
if (!result849) return std::unexpected(std::move(result849.error()));
    }

    auto result850 = world();
if (!result850) return std::unexpected(std::move(result850.error()));
bool condition852 = static_cast<bool>(result850->get().isClient());
if (condition852) {
auto result851 = parameters.contains("particle");
if (!result851) return std::unexpected(std::move(result851.error()));
condition852 = static_cast<bool>(std::move(*result851));
}
if (condition852) {
      auto result853 = parameters.getObject("particle");
if (!result853) return std::unexpected(std::move(result853.error()));
auto result854 = Particle::create(root(), std::move(*result853));
if (!result854) return std::unexpected(std::move(result854.error()));
auto particle = std::move(*result854);
      particle.translate(position());
      particle.velocity += m_referenceVelocity.value_or(Vec2F{});
      m_pendingRenderables.append(std::move(particle));
    }

  } else if (command == "item") {
    if (isSlave())
      return {};

    auto result855 = parameters.getString("name");
if (!result855) return std::unexpected(std::move(result855.error()));
String const name = std::move(*result855);
    auto result856 = parameters.getInt("count", 1);
if (!result856) return std::unexpected(std::move(result856.error()));
std::size_t count = std::move(*result856);
    auto result857 = parameters.getObject("data", JsonObject{});
if (!result857) return std::unexpected(std::move(result857.error()));
JsonObject data = std::move(*result857);

    auto result858 = ItemDescriptor::create(name, count, data);
if (!result858) return std::unexpected(std::move(result858.error()));
auto result859 = ItemDrop::createRandomizedDrop(this->root(), std::move(*result858), position());
if (!result859) return std::unexpected(std::move(result859.error()));
auto itemDrop = std::move(*result859);
    auto result860 = world();
if (!result860) return std::unexpected(std::move(result860.error()));
auto result861 = result860->get().addEntity(itemDrop);
if (!result861) return std::unexpected(std::move(result861.error()));

  } else if (command == "sound") {
    auto result862 = world();
if (!result862) return std::unexpected(std::move(result862.error()));
if (!result862->get().isClient())
      return {};

    auto result863 = this->root().assets();
if (!result863) return std::unexpected(std::move(result863.error()));
auto result864 = parameters.getArray("options");
if (!result864) return std::unexpected(std::move(result864.error()));
auto result865 = this->root().random().randValueFrom(std::move(*result864)).toString();
if (!result865) return std::unexpected(std::move(result865.error()));
auto result866 = std::move(*result863)->audio(std::move(*result865));
if (!result866) return std::unexpected(std::move(result866.error()));
auto result1275 = AudioInstance::create(*std::move(*result866));
if (!result1275) return std::unexpected(std::move(result1275.error()));
std::shared_ptr<AudioInstance> sound = std::move(*result1275);
    sound->setPosition(position());
    auto result867 = parameters.getFloat("volume", 1.0f);
if (!result867) return std::unexpected(std::move(result867.error()));
sound->setVolume(std::move(*result867));
    auto result868 = parameters.getFloat("pitch", 1.0f);
if (!result868) return std::unexpected(std::move(result868.error()));
sound->setPitchMultiplier(std::move(*result868));
    m_pendingRenderables.append(std::move(sound));

  } else if (command == "light") {
    auto result869 = world();
if (!result869) return std::unexpected(std::move(result869.error()));
if (!result869->get().isClient())
      return {};

    auto result870 = parameters.get("color");
if (!result870) return std::unexpected(std::move(result870.error()));
auto result871 = jsonToColor(std::move(*result870));
if (!result871) return std::unexpected(std::move(result871.error()));
auto result872 = parameters.getBool("pointLight", true);
if (!result872) return std::unexpected(std::move(result872.error()));
m_pendingRenderables.append(LightSource{
        position(),
        std::move(*result871).toRgbF(),
        (LightType)std::move(*result872),
        0.0f,
        0.0f,
        0.0f
      });

  } else if (command == "option") {
    auto result873 = parameters.getArray("options");
if (!result873) return std::unexpected(std::move(result873.error()));
JsonArray options = std::move(*result873);
    if (options.size())
      {
auto result874 = this->root().random().randFrom(options);
if (!result874) return std::unexpected(std::move(result874.error()));
auto result875 = processAction(result874->get());
if (!result875) return std::unexpected(std::move(result875.error()));
}

  } else if (command == "actions") {
    auto result876 = parameters.getArray("list");
if (!result876) return std::unexpected(std::move(result876.error()));
JsonArray list = std::move(*result876);
    for (auto const& action : list)
      {
auto result877 = processAction(action);
if (!result877) return std::unexpected(std::move(result877.error()));
}

  } else if (command == "loop") {
    auto result878 = parameters.getInt("count");
if (!result878) return std::unexpected(std::move(result878.error()));
int count = std::move(*result878);
    auto result879 = parameters.getArray("body");
if (!result879) return std::unexpected(std::move(result879.error()));
JsonArray body = std::move(*result879);
    while (count > 0) {
      for (auto const& action : body)
        {
auto result880 = processAction(action);
if (!result880) return std::unexpected(std::move(result880.error()));
}
      count--;
    }

  } else if (command == "config") {
    auto result881 = this->root().assets();
if (!result881) return std::unexpected(std::move(result881.error()));
auto result882 = parameters.getString("file");
if (!result882) return std::unexpected(std::move(result882.error()));
auto result883 = std::move(*result881)->json(std::move(*result882));
if (!result883) return std::unexpected(std::move(result883.error()));
auto result884 = processAction(std::move(*result883));
if (!result884) return std::unexpected(std::move(result884.error()));

  } else {
    return std::unexpected(StarException(strf("Unknown projectile reap command {}", command)));
  }
  return {};
}

std::expected<void, StarException> Projectile::tickShared(float dt) {
  if (!m_config->orientationLocked && !m_movementController->stickingDirection()) {
    auto apparentVelocity = m_movementController->velocity() - m_referenceVelocity.value_or(Vec2F{});
    if (apparentVelocity != Vec2F())
      m_movementController->setRotation(apparentVelocity.angle());
  }

  m_animationTimer += dt;
  setFrame(getFrame());

  m_effectEmitter->setSourcePosition("normal", position());
  m_effectEmitter->setDirection(getAngleSide(m_movementController->rotation(), true).second);
  auto result719 = m_effectEmitter->tick(dt, *entityMode());
if (!result719) return std::unexpected(std::move(result719.error()));

  if (m_collisionEvent.pullOccurred()) {
    auto result720 = m_parameters.getArray("actionOnCollide", m_config->actionOnCollide);
if (!result720) return std::unexpected(std::move(result720.error()));
for (auto const& action : std::move(*result720))
      {
auto result721 = processAction(action);
if (!result721) return std::unexpected(std::move(result721.error()));
}
  }

  for (auto periodicActionIt = m_periodicActions.begin(); periodicActionIt != m_periodicActions.end();) {
    auto& periodicAction = *periodicActionIt;
    if (std::get<1>(periodicAction)) {
      if (std::get<0>(periodicAction).wrapTick(root().timing().timestep)) {
        auto actionResult = processAction(std::get<2>(periodicAction));
        if (!actionResult)
          return std::unexpected(std::move(actionResult.error()));
      }
    } else {
      if (std::get<0>(periodicAction).tick(dt)) {
        auto actionResult = processAction(std::get<2>(periodicAction));
        if (!actionResult)
          return std::unexpected(std::move(actionResult.error()));
        periodicActionIt = m_periodicActions.erase(periodicActionIt);
        continue;
      }
    }
    ++periodicActionIt;
  }
  return {};
}

std::expected<void, StarException> Projectile::setup() {
  auto result681 = m_parameters.optString("uniqueId");
if (!result681) return std::unexpected(std::move(result681.error()));
if (auto uniqueId = std::move(*result681))
    setUniqueId(*uniqueId);

  auto result682 = m_parameters.getFloat("acceleration", m_config->acceleration);
if (!result682) return std::unexpected(std::move(result682.error()));
m_acceleration = std::move(*result682);
  auto result683 = m_parameters.getFloat("power", m_config->power);
if (!result683) return std::unexpected(std::move(result683.error()));
m_power = std::move(*result683);
  auto result684 = m_parameters.getFloat("powerMultiplier", 1.0f);
if (!result684) return std::unexpected(std::move(result684.error()));
m_powerMultiplier = std::move(*result684);
  { // it is possible to shove a frame name in processing. I hope nobody actually does this but account for it...
    auto result685 = m_parameters.getString("processing", "");
if (!result685) return std::unexpected(std::move(result685.error()));
String processing = std::move(*result685);
    auto begin = processing.utf8().find_first_of('?');
    if (begin == NPos) {
      m_imageDirectives = "";
      m_imageSuffix = std::move(processing);
    }
    else if (begin == 0) {
      m_imageDirectives = std::move(processing);
      m_imageSuffix = "";
    }
    else {
      m_imageDirectives = (String)processing.utf8().substr(begin);
      m_imageSuffix = processing.utf8().substr(0, begin);
    }
  }
  auto result686 = m_parameters.getString("persistentAudio", m_config->persistentAudio);
if (!result686) return std::unexpected(std::move(result686.error()));
m_persistentAudioFile = std::move(*result686);

  auto result687 = m_parameters.getString("damageKind", m_config->damageKind);
if (!result687) return std::unexpected(std::move(result687.error()));
m_damageKind = std::move(*result687);
  auto result688 = m_parameters.getString("damageType", m_config->damageType);
if (!result688) return std::unexpected(std::move(result688.error()));
auto result689 = enumValue<DamageTypeNames>(std::move(*result688));
if (!result689) return std::unexpected(std::move(result689.error()));
m_damageType = *result689;
auto result690 = m_parameters.getBool("rayCheckToSource", m_config->rayCheckToSource);
if (!result690) return std::unexpected(std::move(result690.error()));
m_rayCheckToSource = std::move(*result690);

  auto result691 = m_parameters.get("damageTeam", m_config->damageTeam);
if (!result691) return std::unexpected(std::move(result691.error()));
if (auto damageTeam = std::move(*result691)) {
    m_damageTeam = damageTeam;
    auto result692 = EntityDamageTeam::create(damageTeam);
if (!result692) return std::unexpected(std::move(result692.error()));
setTeam(std::move(*result692));
  }
  auto result693 = m_parameters.optString("damageRepeatGroup");
if (!result693) return std::unexpected(std::move(result693.error()));
auto damageRepeatGroup = std::move(*result693);
  auto const& defaultDamageRepeatGroup = m_config->damageRepeatGroup;
  m_damageRepeatGroup = damageRepeatGroup ? damageRepeatGroup : defaultDamageRepeatGroup;
  auto result694 = m_parameters.optFloat("damageRepeatTimeout");
if (!result694) return std::unexpected(std::move(result694.error()));
auto damageRepeatTimeout = std::move(*result694);
  auto const& defaultDamageRepeatTimeout = m_config->damageRepeatTimeout;
  m_damageRepeatTimeout = damageRepeatTimeout ? damageRepeatTimeout : defaultDamageRepeatTimeout;

  auto result695 = m_parameters.getBool("falldown", m_config->falldown);
if (!result695) return std::unexpected(std::move(result695.error()));
m_falldown = std::move(*result695);

  auto result696 = m_parameters.getBool("hydrophobic", m_config->hydrophobic);
if (!result696) return std::unexpected(std::move(result696.error()));
m_hydrophobic = std::move(*result696);
  auto result697 = m_parameters.getBool("onlyHitTerrain", m_config->onlyHitTerrain);
if (!result697) return std::unexpected(std::move(result697.error()));
m_onlyHitTerrain = std::move(*result697);

  auto result698 = m_parameters.get("movementSettings", Json());
if (!result698) return std::unexpected(std::move(result698.error()));
  auto movementSettings = jsonMerge(m_config->movementSettings, *result698);
  auto result700 = movementSettings.contains("physicsEffectCategories");
if (!result700) return std::unexpected(std::move(result700.error()));
if (!std::move(*result700))
    {
auto result701 = movementSettings.set("physicsEffectCategories", JsonArray{"projectile"});
if (!result701) return std::unexpected(std::move(result701.error()));
movementSettings = std::move(*result701);
}
  auto result702 = MovementParameters::create(movementSettings);
if (!result702) return std::unexpected(std::move(result702.error()));
auto result703 = MovementController::create(this->root(), std::move(*result702));
if (!result703) return std::unexpected(std::move(result703.error()));
m_movementController = std::move(*result703);

  auto result704 = EffectEmitter::create(this->root());
if (!result704) return std::unexpected(std::move(result704.error()));
m_effectEmitter = std::move(*result704);

  auto result705 = m_parameters.getFloat("speed", m_config->initialSpeed);
if (!result705) return std::unexpected(std::move(result705.error()));
m_initialSpeed = std::move(*result705);
  m_sourceEntity = NullEntityId;
  m_trackSourceEntity = false;
  auto result706 = m_parameters.getInt("bounces", m_config->bounces);
if (!result706) return std::unexpected(std::move(result706.error()));
m_bounces = std::move(*result706);

  m_frame = 0;

  m_animationTimer = 0.0f;
  auto result707 = m_parameters.getFloat("animationCycle", m_config->animationCycle);
if (!result707) return std::unexpected(std::move(result707.error()));
m_animationCycle = std::move(*result707);
  m_collision = false;

  auto result708 = m_config->physicsForces.iterateObject();
if (!result708) return std::unexpected(std::move(result708.error()));
for (auto const& p : std::move(*result708)) {
    auto& forceConfig = m_physicsForces[p.first];

    auto result709 = jsonToPhysicsForceRegion(p.second);
if (!result709) return std::unexpected(std::move(result709.error()));
forceConfig.forceRegion = std::move(*result709);
    auto result710 = p.second.getBool("enabled", true);
if (!result710) return std::unexpected(std::move(result710.error()));
forceConfig.enabled.set(std::move(*result710));
  }

  auto result711 = m_config->physicsCollisions.iterateObject();
if (!result711) return std::unexpected(std::move(result711.error()));
for (auto const& p : std::move(*result711)) {
    auto& forceConfig = m_physicsCollisions[p.first];

    auto result712 = PhysicsMovingCollision::fromJson(p.second);
if (!result712) return std::unexpected(std::move(result712.error()));
forceConfig.movingCollision = std::move(*result712);
    auto result713 = p.second.getBool("enabled", true);
if (!result713) return std::unexpected(std::move(result713.error()));
forceConfig.enabled.set(std::move(*result713));
  }

  m_physicsForces.sortByKey();
  for (auto& p : m_physicsForces)
    {
auto result714 = m_netGroup.addNetElement(p.second.enabled);
if (!result714) return std::unexpected(std::move(result714.error()));
}

  m_physicsCollisions.sortByKey();
  for (auto& p : m_physicsCollisions)
    {
auto result715 = m_netGroup.addNetElement(p.second.enabled);
if (!result715) return std::unexpected(std::move(result715.error()));
}

  auto result716 = m_netGroup.addNetElement(m_collisionEvent);
if (!result716) return std::unexpected(std::move(result716.error()));
  auto result717 = m_netGroup.addNetElement(*m_movementController);
if (!result717) return std::unexpected(std::move(result717.error()));
  auto result718 = m_netGroup.addNetElement(*m_effectEmitter);
if (!result718) return std::unexpected(std::move(result718.error()));
  return {};
}

std::expected<LuaCallbacks, StarException> Projectile::makeProjectileCallbacks() {
  LuaCallbacks callbacks;
  auto result670 = callbacks.registerCallback("getParameter", [&self = *this](String const& name, Json const& def) {
      return self.configValue(name,def);
    });
if (!result670) return std::unexpected(std::move(result670.error()));
  auto result671 = callbacks.registerCallback("die", [&self = *this]() { self.m_timeToLive = 0.0f; });
if (!result671) return std::unexpected(std::move(result671.error()));
  auto result672 = callbacks.registerCallback("sourceEntity", [&self = *this]() -> std::optional<EntityId> {
      if (self.m_sourceEntity == NullEntityId)
        return {};
      else
        return self.m_sourceEntity;
    });
if (!result672) return std::unexpected(std::move(result672.error()));
  auto result673 = callbacks.registerCallback("powerMultiplier", [&self = *this]() { return self.powerMultiplier(); });
if (!result673) return std::unexpected(std::move(result673.error()));
  auto result674 = callbacks.registerCallback("timeToLive", [&self = *this]() { return self.m_timeToLive; });
if (!result674) return std::unexpected(std::move(result674.error()));
  auto result675 = callbacks.registerCallback("setTimeToLive", [&self = *this](float const& timeToLive) { return self.m_timeToLive = timeToLive; });
if (!result675) return std::unexpected(std::move(result675.error()));
  auto result676 = callbacks.registerCallback("collision", [&self = *this]() { return self.m_collision; });
if (!result676) return std::unexpected(std::move(result676.error()));
  auto result677 = callbacks.registerCallback("processAction", [&self = *this](Json const& action)  -> std::expected<void, StarException> { auto result653 = self.processAction(action);
if (!result653) return std::unexpected(std::move(result653.error()));   return {};
});
if (!result677) return std::unexpected(std::move(result677.error()));
  auto result678 = callbacks.registerCallback("power", [&self = *this]() { return self.m_power; });
if (!result678) return std::unexpected(std::move(result678.error()));
  auto result679 = callbacks.registerCallback("setPower", [&self = *this](float const& power) { self.m_power = power; });
if (!result679) return std::unexpected(std::move(result679.error()));
  auto result680 = callbacks.registerCallback("setReferenceVelocity", [&self = *this](std::optional<Vec2F> const& referenceVelocity) { self.setReferenceVelocity(referenceVelocity); });
if (!result680) return std::unexpected(std::move(result680.error()));
  return callbacks;
}

std::expected<void, StarException> Projectile::collectPendingRenderEvents(RenderCallback& renderCallback) {
  m_renderLights.clear();
  for (auto const& renderable : m_pendingRenderables) {
    if (std::holds_alternative<std::shared_ptr<AudioInstance>>(renderable))
      {
auto result668 = renderCallback.addAudio(std::get<std::shared_ptr<AudioInstance>>(renderable));
if (!result668) return std::unexpected(std::move(result668.error()));
}
    else if (std::holds_alternative<Particle>(renderable))
      {
auto result669 = renderCallback.addParticle(std::get<Particle>(renderable));
if (!result669) return std::unexpected(std::move(result669.error()));
}
    else if (auto light = std::get_if<LightSource>(&renderable))
      m_renderLights.append(*light);
  }
  m_pendingRenderables.clear();
  return {};
}

}

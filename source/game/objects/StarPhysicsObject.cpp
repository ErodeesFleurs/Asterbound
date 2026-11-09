import star.format;
import star.output_proxy;
import star.json;
import star.json_extra;
import star.id_map;
import star.casting;
import star.vector;
import star.data_stream;
import star.game_types;
import star.strong_typedef;
import star.bi_map;
import star.list;
import star.interpolation;
import star.lua_converters;
import star.random;
import star.periodic;
import star.periodic_function;
import star.set;
import star.color;
import star.lua;
import star.audio;
import star.thread;
import star.map;
import star.string;
import star.rect;
import star.image_processing;
import star.hash;
import star.string_view;
import star.directives;
import star.asset_path;
import star.ordered_map;
import star.io_device;
import star.ref_ptr;
import star.logging;
import star.listener;
import std;
import star.version;


import star.lua_root;
import star.lua_components;
import star.lua_animation_component;
import star.tile_damage;
import star.interaction_types;
import star.item_descriptor;
import star.celestial_coordinate;
import star.quest_descriptor;
import star.interactive_entity;
import star.tile_entity;
import star.status_effect_entity;
import star.scripted_entity;
import star.chat_action;
import star.chatty_entity;
import star.wiring;
import star.wire_entity;
import star.inspectable_entity;
import star.animated_part_set;
import star.drawable;
import star.animation;
import star.particle;
import star.mixer;
import star.networked_animator;
import star.entity_rendering_types;
import star.entity_rendering;
import star.object;
import star.damage_types;
import star.world_geometry;
import star.status_types;
import star.damage;
import star.light_source;
import star.entity;
import star.collision_block;
import star.force_regions;
import star.physics_entity;
import star.asset_source;
import star.assets;
import star.root_base;
import star.configuration;
import star.root;

import star.physics_object;
import star.object_database;


import star.algorithm;
namespace Star {

PhysicsObject::PhysicsObject(Root& rootContext) : Entity(rootContext), Object(rootContext) {}

std::expected<std::shared_ptr<PhysicsObject>, StarException> PhysicsObject::create(Root& rootContext, std::shared_ptr<ObjectConfig const> config, Json const& parameters) {
  auto object = std::shared_ptr<PhysicsObject>(new PhysicsObject(rootContext));
  auto operation1 = object->initialize(std::move(config), parameters);
  if (!operation1)
    return std::unexpected(std::move(operation1.error()));
  auto forcesConfigResult = object->configValue("physicsForces", JsonObject());
  if (!forcesConfigResult)
    return std::unexpected(std::move(forcesConfigResult.error()));
  auto forcesConfig = std::move(*forcesConfigResult);
  auto forcesResult = forcesConfig.toObject();
  if (!forcesResult)
    return std::unexpected(std::move(forcesResult.error()));
  auto forces = std::move(*forcesResult);
  for (auto const& pair : forces) {
    auto& force = object->m_physicsForces[pair.first];
    auto regionResult = jsonToPhysicsForceRegion(pair.second);
    if (!regionResult)
      return std::unexpected(std::move(regionResult.error()));
    auto region = std::move(*regionResult);
    force.forceRegion = std::move(region);
    auto enabledResult = pair.second.getBool("enabled", true);
    if (!enabledResult)
      return std::unexpected(std::move(enabledResult.error()));
    auto enabled = std::move(*enabledResult);
    force.enabled.set(enabled);
  }
  auto collisionsConfigResult = object->configValue("physicsCollisions", JsonObject());
  if (!collisionsConfigResult)
    return std::unexpected(std::move(collisionsConfigResult.error()));
  auto collisionsConfig = std::move(*collisionsConfigResult);
  auto collisionsResult = collisionsConfig.toObject();
  if (!collisionsResult)
    return std::unexpected(std::move(collisionsResult.error()));
  auto collisions = std::move(*collisionsResult);
  for (auto const& pair : collisions) {
    auto& collision = object->m_physicsCollisions[pair.first];
    auto movingCollisionResult = PhysicsMovingCollision::fromJson(pair.second);
    if (!movingCollisionResult)
      return std::unexpected(std::move(movingCollisionResult.error()));
    auto movingCollision = std::move(*movingCollisionResult);
    collision.movingCollision = std::move(movingCollision);
    collision.xPosition.set(std::exchange(collision.movingCollision.position[0], 0.0f));
    collision.yPosition.set(std::exchange(collision.movingCollision.position[1], 0.0f));
    auto enabledResult = pair.second.getBool("enabled", true);
    if (!enabledResult)
      return std::unexpected(std::move(enabledResult.error()));
    auto enabled = std::move(*enabledResult);
    collision.enabled.set(enabled);
  }
  object->m_physicsForces.sort_by_key();
  for (auto& pair : object->m_physicsForces) {
    auto operation2 = object->m_netGroup.addNetElement(pair.second.enabled);
    if (!operation2)
      return std::unexpected(std::move(operation2.error()));
  }
  object->m_physicsCollisions.sort_by_key();
  for (auto& pair : object->m_physicsCollisions) {
    auto operation3 = object->m_netGroup.addNetElement(pair.second.xPosition);
    if (!operation3)
      return std::unexpected(std::move(operation3.error()));
    auto operation4 = object->m_netGroup.addNetElement(pair.second.yPosition);
    if (!operation4)
      return std::unexpected(std::move(operation4.error()));
    pair.second.xPosition.setInterpolator(lerp<float, float>);
    pair.second.yPosition.setInterpolator(lerp<float, float>);
    auto operation5 = object->m_netGroup.addNetElement(pair.second.enabled);
    if (!operation5)
      return std::unexpected(std::move(operation5.error()));
  }
  return object;
}

std::expected<void, StarException> PhysicsObject::enableInterpolation(float extrapolationHint) {
  return m_netGroup.enableNetInterpolation(extrapolationHint);
}

std::expected<void, StarException> PhysicsObject::disableInterpolation() {
  return m_netGroup.disableNetInterpolation();
}

std::expected<void, StarException> PhysicsObject::init(World& world, EntityId entityId, EntityMode mode) {
  if (mode == EntityMode::Master) {
    LuaCallbacks callbacks;
    auto operation6 = callbacks.registerCallback("setForceEnabled", [this](String const& force, bool enabled) -> std::expected<void, StarException> {
  auto configResult = m_physicsForces.get(force);
  if (!configResult)
    return std::unexpected(std::move(configResult.error()));
  auto& config = configResult->get();
  config.enabled.set(enabled);
  return {};
});
    if (!operation6)
      return std::unexpected(std::move(operation6.error()));
    auto operation7 = callbacks.registerCallback("setCollisionPosition", [this](String const& collision, Vec2F const& pos) -> std::expected<void, StarException> {
  auto configResult = m_physicsCollisions.get(collision);
  if (!configResult)
    return std::unexpected(std::move(configResult.error()));
  auto& config = configResult->get();
  config.xPosition.set(pos[0]);
  config.yPosition.set(pos[1]);
  return {};
});
    if (!operation7)
      return std::unexpected(std::move(operation7.error()));
    auto operation8 = callbacks.registerCallback("setCollisionEnabled", [this](String const& collision, bool enabled) -> std::expected<void, StarException> {
  auto configResult = m_physicsCollisions.get(collision);
  if (!configResult)
    return std::unexpected(std::move(configResult.error()));
  auto& config = configResult->get();
  config.enabled.set(enabled);
  return {};
});
    if (!operation8)
      return std::unexpected(std::move(operation8.error()));
    auto operation9 = m_scriptComponent.addCallbacks("physics", std::move(callbacks));
    if (!operation9)
      return std::unexpected(std::move(operation9.error()));
  }
  auto initialized = Object::init(world, entityId, mode);
  if (!initialized) {
    auto removed = m_scriptComponent.removeCallbacks("physics");
    if (!removed)
      return std::unexpected(initialized.error().with_context(Star::format("Physics callback cleanup failed: {}", Star::output_exception(removed.error(), false))));
    return std::unexpected(std::move(initialized.error()));
  }
  m_metaBoundBox = Object::metaBoundBox();
  for (auto const& pair : m_physicsForces)
    m_metaBoundBox.combine(std::visit([](auto const& force) { return force.boundBox(); }, pair.second.forceRegion));
  return {};
}

std::expected<void, StarException> PhysicsObject::uninit() {
  auto callbacks = m_scriptComponent.removeCallbacks("physics");
  auto uninitialized = Object::uninit();
  if (!callbacks)
    return std::unexpected(std::move(callbacks.error()));
  return uninitialized;
}

std::expected<void, StarException> PhysicsObject::update(float dt, std::uint64_t currentStep) {
  auto operation10 = Object::update(dt, currentStep);
  if (!operation10)
    return std::unexpected(std::move(operation10.error()));
  if (isSlave())
    return m_netGroup.tickNetInterpolation(dt);
  return {};
}

RectF PhysicsObject::metaBoundBox() const {
  return m_metaBoundBox;
}

std::expected<List<PhysicsForceRegion>, StarException> PhysicsObject::forceRegions() const {
  List<PhysicsForceRegion> forces;
  for (auto const& p : m_physicsForces) {
    if (p.second.enabled.get()) {
      PhysicsForceRegion forceRegion = p.second.forceRegion;
      std::visit([pos = position()](auto& fr) { fr.translate(pos); }, forceRegion);
      forces.push_back(std::move(forceRegion));
    }
  }
  return forces;
}

std::size_t PhysicsObject::movingCollisionCount() const {
  return m_physicsCollisions.size();
}

std::expected<std::optional<PhysicsMovingCollision>, StarException> PhysicsObject::movingCollision(std::size_t positionIndex) const {
  auto config = m_physicsCollisions.value_at(positionIndex);
  if (!config)
    return std::unexpected(std::move(config.error()));
  if (!config->get().enabled.get())
    return std::nullopt;
  PhysicsMovingCollision collision = config->get().movingCollision;
  collision.translate(position() + Vec2F(config->get().xPosition.get(), config->get().yPosition.get()));
  return collision;
}

}

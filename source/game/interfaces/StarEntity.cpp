import std;
import star.string;
import star.list;
import star.format;
import star.json;
import star.id_map;
import star.casting;
import star.data_stream;
import star.game_types;
import star.poly;
import star.strong_typedef;
import star.bi_map;
import star.net_compatibility;
import star.damage_types;
import star.world_geometry;
import star.status_types;
import star.damage;
import star.light_source;
import star.entity;


import star.byte_array;
import star.rect;
namespace Star {

class Root;

extern std::expected<EnumMap<ClientEntityMode>, StarException> const ClientEntityModeNames = EnumMap<ClientEntityMode>::create({
  {ClientEntityMode::ClientSlaveOnly, "ClientSlaveOnly"},
  {ClientEntityMode::ClientMasterAllowed, "ClientMasterAllowed"},
  {ClientEntityMode::ClientPresenceMaster, "ClientPresenceMaster"}
});

extern std::expected<EnumMap<EntityType>, StarException> const EntityTypeNames = EnumMap<EntityType>::create({
  {EntityType::Plant, "plant"},
  {EntityType::Object, "object"},
  {EntityType::Vehicle, "vehicle"},
  {EntityType::ItemDrop, "itemDrop"},
  {EntityType::PlantDrop, "plantDrop"},
  {EntityType::Projectile, "projectile"},
  {EntityType::Stagehand, "stagehand"},
  {EntityType::Monster, "monster"},
  {EntityType::Npc, "npc"},
  {EntityType::Player, "player"}
});

Entity::~Entity() = default;

std::expected<void, StarException> Entity::init(World& world, EntityId entityId, EntityMode mode) {
  if (entityId == NullEntityId)
    return std::unexpected(EntityException("Entity::init called with null entity id"));
  if (m_world)
    return std::unexpected(EntityException("Entity::init called when already initialized"));

  resetPresentation();
  m_world = std::ref(world);
  m_entityMode = mode;
  m_entityId = entityId;
  return {};
}

std::expected<void, StarException> Entity::uninit() {
  resetPresentation();
  m_world.reset();
  m_entityMode = {};
  m_entityId = NullEntityId;
  return {};
}

std::expected<std::pair<ByteArray, std::uint64_t>, StarException> Entity::writeNetState(std::uint64_t, NetCompatibilityRules) {
  return std::pair<ByteArray, std::uint64_t>{ByteArray(), 0};
}

std::expected<void, StarException> Entity::readNetState(ByteArray, float, NetCompatibilityRules) { return {}; }

std::expected<void, StarException> Entity::enableInterpolation(float) { return {}; }

std::expected<void, StarException> Entity::disableInterpolation() { return {}; }

RectF Entity::collisionArea() const {
  return RectF::null();
}

bool Entity::ephemeral() const {
  return false;
}

ClientEntityMode Entity::clientEntityMode() const {
  return ClientEntityMode::ClientSlaveOnly;
}

bool Entity::masterOnly() const {
  return false;
}

std::expected<String, StarException> Entity::name() const {
  return "";
}

std::expected<String, StarException> Entity::description() const {
  return "";
}

std::expected<List<LightSource>, StarException> Entity::lightSources() const {
  return List<LightSource>{};
}

std::expected<List<DamageSource>, StarException> Entity::damageSources() const {
  return List<DamageSource>{};
}

std::expected<void, StarException> Entity::hitOther(EntityId, DamageRequest const&) { return {}; }

std::expected<void, StarException> Entity::damagedOther(DamageNotification const&) { return {}; }

std::expected<std::optional<HitType>, StarException> Entity::queryHit(DamageSource const&) const {
  return {};
}

std::expected<std::optional<PolyF>, StarException> Entity::hitPoly() const {
  return {};
}

std::expected<List<DamageNotification>, StarException> Entity::applyDamage(DamageRequest const&) {
  return List<DamageNotification>{};
}

std::expected<List<DamageNotification>, StarException> Entity::selfDamageNotifications() {
  return List<DamageNotification>{};
}

std::expected<bool, StarException> Entity::shouldDestroy() const {
  return false;
}

std::expected<void, StarException> Entity::destroy(std::optional<std::reference_wrapper<RenderCallback>>) { return {}; }

std::expected<std::optional<ChainableJsonMessageResponse>, StarException> Entity::receiveMessage(ConnectionId, String const&, JsonArray const&) {
  return {};
}

std::expected<void, StarException> Entity::update(float, std::uint64_t) { return {}; }

std::expected<void, StarException> Entity::collectRenderEvents(RenderCallback&) { return {}; }

std::uint64_t Entity::presentationEpoch() const { return m_presentationEpoch; }

void Entity::resetPresentation() {
  ++m_presentationEpoch;
  m_previousPresentationRotation.reset();
}

void Entity::capturePresentationRotation(float rotation) {
  m_previousPresentationRotation = rotation;
}

float Entity::samplePresentationRotation(float rotation, float interpolation) const {
  if (!m_previousPresentationRotation)
    return rotation;
  float difference = std::remainder(*m_previousPresentationRotation - rotation, 2.0f * std::numbers::pi_v<float>);
  return rotation + difference * (1.0f - std::clamp(interpolation, 0.0f, 1.0f));
}

std::expected<void, StarException> Entity::render(RenderCallback&) { return {}; }

std::expected<void, StarException> Entity::renderLightSources(RenderCallback&) { return {}; }

EntityId Entity::entityId() const {
  return m_entityId;
}

EntityDamageTeam Entity::getTeam() const {
  return m_team;
}

bool Entity::inWorld() const {
  if (m_world) {
    if constexpr (Star::DebugEnabled) {
      Star::star_assert_failed((m_world && m_entityId != NullEntityId && m_entityMode));
    }
    return true;
  } else {
    if constexpr (Star::DebugEnabled) {
      Star::star_assert_failed((!m_world && m_entityId == NullEntityId && !m_entityMode));
    }
    return false;
  }
}

std::expected<std::reference_wrapper<World>, StarException> Entity::world() const {
  if (!m_world)
    return std::unexpected(EntityException("world() called while uninitialized"));

  return *m_world;
}

std::optional<std::reference_wrapper<World>> Entity::maybeWorld() const {
  return m_world;
}

bool Entity::persistent() const {
  return m_persistent;
}

bool Entity::keepAlive() const {
  return m_keepAlive;
}

std::optional<String> Entity::uniqueId() const {
  return m_uniqueId;
}

std::optional<EntityMode> Entity::entityMode() const {
  return m_entityMode;
}

bool Entity::isMaster() const {
  return m_entityMode == EntityMode::Master;
}

bool Entity::isSlave() const {
  return m_entityMode == EntityMode::Slave;
}

Root& Entity::root() const {
  return m_root;
}

Entity::Entity(Root& root) : m_root(root) {
  m_world.reset();
  m_entityId = NullEntityId;
  m_persistent = false;
  m_keepAlive = false;
}

void Entity::setPersistent(bool persistent) {
  m_persistent = persistent;
}

void Entity::setKeepAlive(bool keepAlive) {
  m_keepAlive = keepAlive;
}

void Entity::setUniqueId(std::optional<String> uniqueId) { m_uniqueId = std::move(uniqueId); }

void Entity::setTeam(EntityDamageTeam newTeam) {
  m_team = newTeam;
}

}

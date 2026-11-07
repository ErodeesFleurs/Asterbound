import std;
import star.format;
import star.set;
import star.json;
import star.id_map;
import star.casting;
import star.data_stream;
import star.game_types;
import star.poly;
import star.strong_typedef;
import star.bi_map;
import star.color;
import star.list;
import star.image_processing;
import star.hash;
import star.string_view;
import star.thread;
import star.directives;
import star.asset_path;
import star.rpc_promise;
import star.a_star;
import star.lua_root;
import star.damage_types;
import star.world_geometry;
import star.status_types;
import star.damage;
import star.light_source;
import star.entity;
import star.animation;
import star.particle;
import star.interaction_types;
import star.tile_damage;
import star.item_descriptor;
import star.celestial_coordinate;
import star.quest_descriptor;
import star.interactive_entity;
import star.collision_block;
import star.tile_entity;
import star.tile_modification;
import star.force_regions;
import star.world;
import star.physics_entity;
import star.movement_controller;
import star.platformer_astar_types;
import star.anchorable_entity;
import star.game_timers;
import star.actor_movement_controller;
import star.mobile_entity;
import star.actor_entity;
import star.tool_user_entity;
import star.tool_user_item;

namespace Star {

ToolUserItem::ToolUserItem() = default;

std::expected<void, StarException> ToolUserItem::init(ToolUserEntity& owner, ToolHand hand) {
  m_owner = std::ref(owner);
  m_hand = hand;
  return {};
}

std::expected<void, StarException> ToolUserItem::uninit() {
  m_owner.reset();
  m_hand = {};
  return {};
}

std::expected<void, StarException> ToolUserItem::update(float, FireMode, bool, HashSet<MoveControlType> const&) { return {}; }

bool ToolUserItem::initialized() const { return static_cast<bool>(m_owner); }

std::expected<std::reference_wrapper<ToolUserEntity>, StarException> ToolUserItem::owner() const {
  if (!m_owner)
    return std::unexpected(ToolUserItemException("Not initialized in ToolUserItem::owner"));
  return *m_owner;
}

std::expected<EntityMode, StarException> ToolUserItem::entityMode() const {
  if (!m_owner)
    return std::unexpected(ToolUserItemException("Not initialized in ToolUserItem::entityMode"));
  auto mode = m_owner->get().entityMode();
  if (!mode)
    return std::unexpected(ToolUserItemException("Owner is uninitialized in ToolUserItem::entityMode"));
  return *mode;
}

std::expected<ToolHand, StarException> ToolUserItem::hand() const {
  if (!m_owner || !m_hand)
    return std::unexpected(ToolUserItemException("Not initialized in ToolUserItem::hand"));
  return *m_hand;
}

std::expected<std::reference_wrapper<World>, StarException> ToolUserItem::world() const {
  if (!m_owner)
    return std::unexpected(ToolUserItemException("Not initialized in ToolUserItem::world"));
  return m_owner->get().world();
}

std::expected<List<DamageSource>, StarException> ToolUserItem::damageSources() const {
  return List<DamageSource>{};
}

std::expected<List<PolyF>, StarException> ToolUserItem::shieldPolys() const {
  return List<PolyF>{};
}

std::expected<List<PhysicsForceRegion>, StarException> ToolUserItem::forceRegions() const {
  return List<PhysicsForceRegion>{};
}

}

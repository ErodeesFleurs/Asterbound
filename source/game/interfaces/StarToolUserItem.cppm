module;
import std;
import star.type_identity;
import star.set;
import star.format;
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
import star.lua_root;

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

namespace Star {

struct ToolUserItemExceptionTag {
  static constexpr std::string_view name() { return "ToolUserItemException"; }
};
using ToolUserItemException = StarError<ToolUserItemExceptionTag, StarException>;

// FIXME: You know what another name for an item that a tool user uses is?  A
// Tool.  Three words when one will do, rename.
class ToolUserItem {
public:
  virtual void const* query_type(TypeKey key) const noexcept { return query_type_of<ToolUserItem>(this, key); }
  virtual std::string_view dynamic_type_name() const noexcept { return Star::type_name<ToolUserItem>(); }

  ToolUserItem();
  virtual ~ToolUserItem() = default;

  // Owner must be initialized when a ToolUserItem is initialized, and the item
  // must be uninitialized before its owner is uninitialized.
  [[nodiscard]]
  virtual std::expected<void, StarException> init(ToolUserEntity& owner, ToolHand hand);
  [[nodiscard]]
  virtual std::expected<void, StarException> uninit();

  // Default implementation does nothing
  [[nodiscard]]
  virtual std::expected<void, StarException> update(float dt, FireMode fireMode, bool shifting, HashSet<MoveControlType> const& moves);

  // Default implementations return empty list
  [[nodiscard]]
  virtual std::expected<List<DamageSource>, StarException> damageSources() const;
  [[nodiscard]]
  virtual std::expected<List<PolyF>, StarException> shieldPolys() const;
  [[nodiscard]]
  virtual std::expected<List<PhysicsForceRegion>, StarException> forceRegions() const;

  bool initialized() const;

  // Accessors report an error when initialized() is false.
  [[nodiscard]]
  std::expected<std::reference_wrapper<ToolUserEntity>, StarException> owner() const;
  [[nodiscard]]
  std::expected<EntityMode, StarException> entityMode() const;
  [[nodiscard]]
  std::expected<ToolHand, StarException> hand() const;
  [[nodiscard]]
  std::expected<std::reference_wrapper<World>, StarException> world() const;

private:
  std::optional<std::reference_wrapper<ToolUserEntity>> m_owner;
  std::optional<ToolHand> m_hand;
};

}

export module star.tool_user_item;

export namespace Star {
  using ::Star::ToolUserItemExceptionTag;
  using ::Star::ToolUserItemException;
  using ::Star::ToolUserItem;
}

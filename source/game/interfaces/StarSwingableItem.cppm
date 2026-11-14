module;
import std;
import star.format;
import star.type_identity;
import star.string;
import star.json;
import star.id_map;
import star.casting;
import star.data_stream;
import star.game_types;
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

import star.tool_user_item;

import star.status_effect_item;
import star.lua_components;

import star.fireable_item;

namespace Star {

class SwingableItem : public FireableItem {
public:
  void const* query_type(TypeKey key) const noexcept override {
    return query_type_of<SwingableItem, FireableItem>(this, key);
  }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<SwingableItem>(); }

  SwingableItem();
  ~SwingableItem() override = default;

  // These can be different
  // Default implementation is the same though
  [[nodiscard]]
  virtual std::expected<float, StarException> getAngleDir(float aimAngle, Direction facingDirection);
  [[nodiscard]]
  virtual std::expected<float, StarException> getAngle(float aimAngle);
  [[nodiscard]]
  virtual std::expected<float, StarException> getItemAngle(float aimAngle);
  virtual String getArmFrame();

  [[nodiscard]]
  virtual std::expected<List<Drawable>, StarException> drawables() const = 0;

  [[nodiscard]]
  std::expected<void, StarException> initializeSwingable(Json const& params);
  [[nodiscard]]
  std::expected<void, StarException> setParams(Json const& params);

protected:
  float m_swingStart;
  float m_swingFinish;
  float m_swingAimFactor;
  std::optional<float> m_coolingDownAngle;
};

}

export module star.swingable_item;

export namespace Star {
  using ::Star::SwingableItem;
}

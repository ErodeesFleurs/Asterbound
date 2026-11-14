module;
import std;
import star.format;
import star.type_identity;
import star.json;
import star.id_map;
import star.strong_typedef;
import star.data_stream;
import star.vector;
import star.game_types;
import star.casting;
import star.bi_map;
import star.list;

import star.uuid;
import star.celestial_coordinate;
import star.warping;

import star.damage_types;
import star.world_geometry;
import star.status_types;
import star.damage;

import star.light_source;

import star.entity;
import star.tile_damage;

import star.interaction_types;

import star.item_descriptor;
import star.quest_descriptor;

import star.interactive_entity;
import star.collision_block;

import star.tile_entity;

namespace Star {

class WarpTargetEntity : public virtual TileEntity {
public:
  void const* query_type(TypeKey key) const noexcept override {
    return query_type_of<WarpTargetEntity, TileEntity>(this, key);
  }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<WarpTargetEntity>(); }

  // Foot position for things teleporting onto this entity, relative to root
  // position.
  [[nodiscard]] virtual std::expected<Vec2F, StarException> footPosition() const = 0;
};

}

export module star.warp_target_entity;

export namespace Star {
  using ::Star::WarpTargetEntity;
}

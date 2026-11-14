module;
import std;
import star.format;
import star.type_identity;
import star.string;
import star.json;
import star.id_map;
import star.color;
import star.game_types;
import star.data_stream;
import star.casting;
import star.vector;
import star.strong_typedef;
import star.bi_map;
import star.list;

import star.world_geometry;
import star.wiring;

import star.damage_types;
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

namespace Star {

class WireEntity : public virtual TileEntity {
public:
  void const* query_type(TypeKey key) const noexcept override {
    return query_type_of<WireEntity, TileEntity>(this, key);
  }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<WireEntity>(); }

  ~WireEntity() override = default;

  virtual std::size_t nodeCount(WireDirection direction) const = 0;
  [[nodiscard]] virtual std::expected<Vec2I, StarException> nodePosition(WireNode wireNode) const = 0;
  [[nodiscard]] virtual std::expected<List<WireConnection>, StarException> connectionsForNode(WireNode wireNode) const = 0;
  [[nodiscard]] virtual std::expected<bool, StarException> nodeState(WireNode wireNode) const = 0;

  [[nodiscard]] virtual std::expected<Color, StarException> nodeColor(WireNode wireNode) const = 0;
  [[nodiscard]] virtual std::expected<String, StarException> nodeIcon(WireNode wireNode) const = 0;

  [[nodiscard]] virtual std::expected<void, StarException> addNodeConnection(WireNode wireNode, WireConnection nodeConnection) = 0;
  [[nodiscard]] virtual std::expected<void, StarException> removeNodeConnection(WireNode wireNode, WireConnection nodeConnection) = 0;

  [[nodiscard]] virtual std::expected<void, StarException> evaluate(WireCoordinator& coordinator) = 0;
};

}

export module star.wire_entity;

export namespace Star {
  using ::Star::WireEntity;
}

module;
import std;
import star.format;
import star.type_identity;
import star.json;
import star.id_map;
import star.casting;
import star.vector;
import star.data_stream;
import star.game_types;
import star.strong_typedef;
import star.bi_map;

import star.damage_types;
import star.world_geometry;
import star.status_types;
import star.damage;

import star.light_source;

import star.entity;

namespace Star {

struct EntityAnchor {
  virtual ~EntityAnchor() = default;
  virtual void query_type(TypeQuery& query) const noexcept { query_type_of<EntityAnchor>(*this, query); }
  virtual std::string_view dynamic_type_name() const noexcept { return Star::type_name<EntityAnchor>(); }

  Vec2F position;
  // If set, the entity should place the bottom center of its collision poly on
  // the given position at exit
  std::optional<Vec2F> exitBottomPosition;
  Direction direction;
  float angle;
};

struct EntityAnchorState {
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, EntityAnchorState& anchorState);
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, EntityAnchorState const& anchorState);
  EntityId entityId;
  std::size_t positionIndex;

  bool operator==(EntityAnchorState const& eas) const;
};

[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, EntityAnchorState& anchorState);
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, EntityAnchorState const& anchorState);

class AnchorableEntity : public virtual Entity {
public:
  void query_type(TypeQuery& query) const noexcept override {
    query_type_of<AnchorableEntity, Entity>(*this, query);
  }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<AnchorableEntity>(); }

  virtual std::size_t anchorCount() const = 0;
  [[nodiscard]] virtual std::expected<std::shared_ptr<EntityAnchor const>, StarException> anchor(std::size_t anchorPositionIndex) const = 0;
};

}

export module star.anchorable_entity;

export namespace Star {
  using ::Star::EntityAnchor;
  using ::Star::EntityAnchorState;
  using ::Star::AnchorableEntity;
}

module;
import std;
import star.format;
import star.type_identity;
import star.string;
import star.json;
import star.id_map;
import star.casting;
import star.vector;
import star.data_stream;
import star.game_types;
import star.material_types;
import star.strong_typedef;
import star.bi_map;
import star.list;

import star.damage_types;
import star.world_geometry;
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

namespace Star {

struct MaterialSpace {
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, MaterialSpace const& materialSpace);
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, MaterialSpace& materialSpace);
  MaterialSpace();
  MaterialSpace(Vec2I space, MaterialId material);

  bool operator==(MaterialSpace const& rhs) const;

  Vec2I space;
  MaterialId material;
};

[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, MaterialSpace const& materialSpace);
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, MaterialSpace& materialSpace);

// Entities that derive from TileEntity are those that can be placed in the
// tile grid, and occupy tile spaces, possibly affecting collision.
class TileEntity : public virtual InteractiveEntity {
public:
  void const* query_type(TypeKey key) const noexcept override {
    return query_type_of<TileEntity, InteractiveEntity>(this, key);
  }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<TileEntity>(); }

  TileEntity();

  // position() here is simply the tilePosition (but Vec2F)
  Vec2F position() const override;

  // The base tile position of this object.
  virtual Vec2I tilePosition() const = 0;
  [[nodiscard]] virtual std::expected<void, StarException> setTilePosition(Vec2I const& pos) = 0;

  // TileEntities occupy the given spaces in tile space.  This is relative to
  // the current base position, and may include negative positions.  A 1x1
  // object would occupy just (0, 0).
  virtual List<Vec2I> spaces() const;

  // Blocks that should be marked as "root", so that they are non-destroyable
  // until this entity is destroyable.  Should be outside of spaces(), and
  // after placement should remain static for the lifetime of the entity.
  virtual List<Vec2I> roots() const;

  // TileEntities may register some of their occupied spaces with metamaterials
  // to generate collidable regions
  virtual List<MaterialSpace> materialSpaces() const;
  
  // Returns whether the entity was destroyed
  [[nodiscard]] virtual std::expected<bool, StarException> damageTiles(List<Vec2I> const& positions, Vec2F const& sourcePosition, TileDamage const& tileDamage);
  [[nodiscard]] virtual std::expected<bool, StarException> canBeDamaged() const;

  // Forces the tile entity to do an immediate check if it has been invalidly
  // placed in some way.  The tile entity may do this check on its own, but
  // less often.
  [[nodiscard]] virtual std::expected<bool, StarException> checkBroken() = 0;

  // If the entity accepts interaction through right clicking, by default,
  // returns false.
  bool isInteractive() const override;
  // By default, does nothing.  Will be called only on the server.
  [[nodiscard]] std::expected<InteractAction, StarException> interact(InteractRequest const& request) override;
  // Specific subset spaces that are interactive, by default, just returns
  // spaces()
  virtual List<Vec2I> interactiveSpaces() const;

  [[nodiscard]] std::expected<List<QuestArcDescriptor>, StarException> offeredQuests() const override;
  [[nodiscard]] std::expected<StringSet, StarException> turnInQuests() const override;
  [[nodiscard]] std::expected<Vec2F, StarException> questIndicatorPosition() const override;

protected:
  // Checks whether any of a given spaces list (relative to current tile
  // position) is occupied by a real material.  (Does not include tile
  // entities).
  [[nodiscard]] std::expected<bool, StarException> anySpacesOccupied(List<Vec2I> const& relativeSpaces) const;

  // Checks that *all* spaces are occupied by a real material.
  [[nodiscard]] std::expected<bool, StarException> allSpacesOccupied(List<Vec2I> const& relativeSpaces) const;

  [[nodiscard]] std::expected<float, StarException> spacesLiquidFillLevel(List<Vec2I> const& relativeSpaces) const;
};

inline MaterialSpace::MaterialSpace()
  : material(NullMaterialId) {}

inline MaterialSpace::MaterialSpace(Vec2I space, MaterialId material)
  : space(space), material(material) {}

inline bool MaterialSpace::operator==(MaterialSpace const& rhs) const {
  return space         == rhs.space
      && material      == rhs.material;
}

}

export module star.tile_entity;

export namespace Star {
  using ::Star::TileEntity;
  using ::Star::MaterialSpace;
}

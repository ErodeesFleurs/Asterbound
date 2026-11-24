module;

import star.list;
import star.format;
import star.collision_block;
import star.multi_array;

import std;

import star.vector;
import star.rect;
namespace Star {

// Turns cell geometry into "smoothed" polygonal geometry.  Used by World to
// generate ramps and slopes based on tiles.
class CollisionGenerator {
public:
  // The maximum number of spaces away from a block that can influence the
  // collision geometry of a given block.
  static constexpr int block_influence_radius = 2;

  // Only marching masks 13 and 14 can emit two solids. Both require the
  // current tile to be solid (bit 8). A platform can instead have at most
  // one marching polygon and two platform segments at its generation space.
  static constexpr std::size_t maximum_collisions_per_space = 3;

  // Callback function to tell what kind of collision geometry is in a cell.
  // Will be called up to block_influence_radius outside of the given query
  // region.
  using CollisionKindAccessor = std::function<std::expected<CollisionKind, StarException>(int, int)>;

  void init(CollisionKindAccessor accessor);

  // Get collision geometry for the given block region.
  [[nodiscard]] std::expected<List<CollisionBlock>, StarException> get_blocks(RectI const& region) const;

private:
  void append_platforms(List<CollisionBlock>& output, RectI const& region) const;
  void append_marching_squares(List<CollisionBlock>& output, RectI const& region) const;

  [[nodiscard]] std::expected<void, StarException> populate_collision_buffer(RectI const& region) const;
  CollisionKind collision_kind(int x, int y) const;

  CollisionKindAccessor m_accessor;

  mutable Vec2I m_buffer_corner;
  mutable MultiArray<CollisionKind, 2> m_collision_buffer;
};

}

export module star.collision_generator;

export namespace Star {
  using ::Star::CollisionGenerator;
}

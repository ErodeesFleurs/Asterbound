module;

import star.list;
import star.format;
import star.bi_map;
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
  static int const BlockInfluenceRadius = 2;

  // The Maximum number of blocks that will be generated for a single tile
  // space.
  static std::size_t const MaximumCollisionsPerSpace = 4;

  // Callback function to tell what kind of collision geometry is in a cell.
  // Will be called up to BlockInfluenceRadius outside of the given query
  // region.
  using CollisionKindAccessor = std::function<std::expected<CollisionKind, StarException>(int, int)>;

  void init(CollisionKindAccessor accessor);

  // Get collision geometry for the given block region.
  [[nodiscard]] std::expected<List<CollisionBlock>, StarException> getBlocks(RectI const& region) const;

private:
  void getBlocksPlatforms(List<CollisionBlock>& output, RectI const& region, CollisionKind kind) const;
  void getBlocksMarchingSquares(List<CollisionBlock>& output, RectI const& region, CollisionKind kind) const;

  [[nodiscard]] std::expected<void, StarException> populateCollisionBuffer(RectI const& region) const;
  CollisionKind collisionKind(int x, int y) const;

  CollisionKindAccessor m_accessor;

  mutable Vec2I m_collisionBufferCorner;
  mutable MultiArray<CollisionKind, 2> m_collisionBuffer;
};

}

export module star.collision_generator;

export namespace Star {
  using ::Star::CollisionGenerator;
}

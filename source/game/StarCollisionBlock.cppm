module;

import star.poly;
import star.format;
import std;
import star.list;
import star.bi_map;

import star.vector;
import star.rect;
import star.algorithm;
namespace Star {

enum class CollisionKind : std::uint8_t {
  // Special collision block that is used for unloaded / un-generated tiles.
  // Collides the same as "Block", but does not tile with it.
  Null,
  None,
  Platform,
  Dynamic,
  Slippery,
  Block
};

enum class TileCollisionOverride : std::uint8_t {
  None,
  Empty,
  Platform,
  Block
};

inline CollisionKind collisionKindFromOverride(TileCollisionOverride const& over) {
  switch (over) {
    case TileCollisionOverride::Empty:
      return CollisionKind::None;
    case TileCollisionOverride::Platform:
      return CollisionKind::Platform;
    case TileCollisionOverride::Block:
      return CollisionKind::Block;
    default:
      return CollisionKind::Null;
  }
}

class CollisionSet {
public:
  CollisionSet();
  CollisionSet(std::initializer_list<CollisionKind> kinds);

  void insert(CollisionKind kind);
  void remove(CollisionKind kind);
  bool contains(CollisionKind kind) const;

private:
  static std::uint8_t kindBit(CollisionKind kind);

  std::uint8_t m_kinds;
};

// The default CollisionSet consists of Null, Slippery, Dynamic and Block
extern CollisionSet const DefaultCollisionSet;

// Defines what can be "blocks" e.g. for tile rendering: Block and Slippery
extern CollisionSet const BlockCollisionSet;

inline constexpr auto TileCollisionOverrideNames = sortedEnumNames(
  std::to_array<std::pair<TileCollisionOverride, std::string_view>>({{TileCollisionOverride::None, "None"},
                                                                     {TileCollisionOverride::Empty, "Empty"},
                                                                     {TileCollisionOverride::Platform, "Platform"},
                                                                     {TileCollisionOverride::Block, "Block"}}));

inline constexpr auto CollisionKindNames =
  sortedEnumNames(std::to_array<std::pair<CollisionKind, std::string_view>>({{CollisionKind::Null, "Null"},
                                                                             {CollisionKind::None, "None"},
                                                                             {CollisionKind::Platform, "Platform"},
                                                                             {CollisionKind::Dynamic, "Dynamic"},
                                                                             {CollisionKind::Slippery, "Slippery"},
                                                                             {CollisionKind::Block, "Block"}}));

bool isColliding(CollisionKind kind, CollisionSet const& collisionSet);
bool isSolidColliding(CollisionKind kind);

// Returns the highest priority collision kind, where Block > Slippery >
// Dynamic > Platform > None > Null
CollisionKind maxCollision(CollisionKind first, CollisionKind second);

struct CollisionBlock {
  // Make a null collision block for the given space.
  static CollisionBlock nullBlock(Vec2I const& space);

  CollisionKind kind;
  Vec2I space;
  PolyF poly;
  RectF polyBounds;
};

inline CollisionSet::CollisionSet()
  : m_kinds(0) {}

inline CollisionSet::CollisionSet(std::initializer_list<CollisionKind> kinds)
  : CollisionSet() {
  for (auto kind : kinds) {
    insert(kind);
  }
}

inline void CollisionSet::insert(CollisionKind kind) {
  m_kinds = m_kinds | kindBit(kind);
}

inline void CollisionSet::remove(CollisionKind kind) {
  m_kinds = m_kinds & ~kindBit(kind);
}

inline bool CollisionSet::contains(CollisionKind kind) const {
  return m_kinds & kindBit(kind);
}

inline std::uint8_t CollisionSet::kindBit(CollisionKind kind) { return 1 << (static_cast<std::uint8_t>(kind) + 1); }

inline bool isColliding(CollisionKind kind, CollisionSet const& collisionSet) {
  return collisionSet.contains(kind);
}

inline bool isSolidColliding(CollisionKind kind) {
  return isColliding(kind, DefaultCollisionSet);
}

inline CollisionKind maxCollision(CollisionKind first, CollisionKind second) {
  return std::max(first, second);
}

inline CollisionBlock CollisionBlock::nullBlock(Vec2I const& space) {
  CollisionBlock block;
  block.kind = CollisionKind::Null;
  block.space = space;
  block.poly = {
    Vec2F(space) + Vec2F(0, 0),
    Vec2F(space) + Vec2F(1, 0),
    Vec2F(space) + Vec2F(1, 1),
    Vec2F(space) + Vec2F(0, 1)
  };
  block.polyBounds = RectF::withSize(Vec2F(space), Vec2F(1, 1));
  return block;
}

}

export module star.collision_block;

export namespace Star {
  using ::Star::CollisionKind;
  using ::Star::TileCollisionOverride;
  using ::Star::collisionKindFromOverride;
  using ::Star::CollisionSet;
  using ::Star::DefaultCollisionSet;
  using ::Star::BlockCollisionSet;
  using ::Star::TileCollisionOverrideNames;
  using ::Star::CollisionKindNames;
  using ::Star::isColliding;
  using ::Star::isSolidColliding;
  using ::Star::maxCollision;
  using ::Star::CollisionBlock;
}

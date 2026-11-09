import star.format;
import std;
import star.list;
import star.bi_map;
import star.collision_block;

namespace Star {

extern CollisionSet const DefaultCollisionSet({CollisionKind::Null, CollisionKind::Slippery, CollisionKind::Dynamic, CollisionKind::Block});
extern CollisionSet const BlockCollisionSet({CollisionKind::Block, CollisionKind::Slippery});


}

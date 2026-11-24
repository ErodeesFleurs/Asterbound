import std;
import star.format;
import star.json;
import star.id_map;
import star.poly;
import star.casting;
import star.vector;
import star.data_stream;
import star.game_types;
import star.strong_typedef;
import star.bi_map;
import star.list;
import star.rpc_promise;
import star.lua;
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
import star.tile_entity;
import star.tile_modification;
import star.lua_root;
import star.force_regions;
import star.world;
import star.scripted_entity;

import star.line;
import star.rect;
import star.math_common;
namespace Star {

bool World::isServer() const {
  return connection() == server_connection_id;
}

bool World::isClient() const {
  return !isServer();
}

std::expected<List<std::shared_ptr<Entity>>, StarException> World::entityQuery(RectF const& boundBox, EntityFilter selector) const {
  List<std::shared_ptr<Entity>> list;
  auto scanned = forEachEntity(boundBox, [&](std::shared_ptr<Entity> const& entity) -> std::expected<void, StarException> {
    if (selector) {
      auto accepted = selector(entity);
      if (!accepted)
        return std::unexpected(std::move(accepted.error()));
      if (!*accepted)
        return {};
    }
    list.push_back(entity);
    return {};
  });
  if (!scanned)
    return std::unexpected(std::move(scanned.error()));
  return list;
}

std::expected<List<std::shared_ptr<Entity>>, StarException> World::entityLineQuery(Vec2F const& begin, Vec2F const& end, EntityFilter selector) const {
  List<std::shared_ptr<Entity>> list;
  auto scanned = forEachEntityLine(begin, end, [&](std::shared_ptr<Entity> const& entity) -> std::expected<void, StarException> {
    if (selector) {
      auto accepted = selector(entity);
      if (!accepted)
        return std::unexpected(std::move(accepted.error()));
      if (!*accepted)
        return {};
    }
    list.push_back(entity);
    return {};
  });
  if (!scanned)
    return std::unexpected(std::move(scanned.error()));
  return list;
}

std::expected<List<std::shared_ptr<TileEntity>>, StarException> World::entitiesAtTile(Vec2I const& pos, EntityFilter selector) const {
  List<std::shared_ptr<TileEntity>> list;
  auto scanned = forEachEntityAtTile(pos, [&](std::shared_ptr<TileEntity> const& entity) -> std::expected<void, StarException> {
    if (selector) {
      auto accepted = selector(entity);
      if (!accepted)
        return std::unexpected(std::move(accepted.error()));
      if (!*accepted)
        return {};
    }
    list.push_back(entity);
    return {};
  });
  if (!scanned)
    return std::unexpected(std::move(scanned.error()));
  return list;
}

std::expected<List<Vec2I>, StarException> World::findEmptyTiles(Vec2I pos, unsigned maxDist, std::size_t maxAmount, bool excludeEphemeral) const {
  List<Vec2I> res;
  auto occupied = tileIsOccupied(pos, TileLayer::Foreground, excludeEphemeral);
  if (!occupied)
    return std::unexpected(std::move(occupied.error()));
  if (!*occupied)
    res.push_back(pos);

  if (res.size() >= maxAmount)
    return res;

  // searches manhattan distance counterclockwise from right
  for (int distance = 1; distance <= static_cast<int>(maxDist); distance++) {
    const int totalSpots = 4 * distance;
    int xDiff = distance;
    int yDiff = 0;
    int dx = -1;
    int dy = 1;
    for (int i = 0; i < totalSpots; i++) {
      auto nextOccupied = tileIsOccupied(pos + Vec2I(xDiff, yDiff), TileLayer::Foreground);
      if (!nextOccupied)
        return std::unexpected(std::move(nextOccupied.error()));
      if (!*nextOccupied) {
        res.push_back(pos + Vec2I(xDiff, yDiff));
        if (res.size() >= maxAmount) {
          return res;
        }
      }
      xDiff += dx;
      yDiff += dy;
      if (std::abs(xDiff) == distance)
        dx *= -1;
      if (std::abs(yDiff) == distance)
        dy *= -1;
    }
  }

  return res;
}

std::expected<bool, StarException> World::canModifyTile(Vec2I const& pos, TileModification const& modification, bool allowEntityOverlap) const {
  auto valid = validTileModifications({{pos, modification}}, allowEntityOverlap);
  if (!valid)
    return std::unexpected(std::move(valid.error()));
  return !valid->empty();
}

std::expected<bool, StarException> World::modifyTile(Vec2I const& pos, TileModification const& modification, bool allowEntityOverlap) {
  auto remaining = applyTileModifications({{pos, modification}}, allowEntityOverlap);
  if (!remaining)
    return std::unexpected(std::move(remaining.error()));
  return remaining->empty();
}

std::expected<TileDamageResult, StarException> World::damageTile(Vec2I const& tilePosition, TileLayer layer, Vec2F const& sourcePosition, TileDamage const& tileDamage, std::optional<EntityId> sourceEntity) {
  return damageTiles({tilePosition}, layer, sourcePosition, tileDamage, sourceEntity);
}

std::expected<std::shared_ptr<Entity>, StarException> World::closestEntityInSight(Vec2F const& center, float radius, CollisionSet const& collisionSet, EntityFilter selector) const {
  return closestEntity(center, radius,
                       [this, center, collisionSet, selector = std::move(selector)](
                         std::shared_ptr<Entity> const& entity) -> std::expected<bool, StarException> {
                         if (selector) {
                           auto accepted = selector(entity);
                           if (!accepted)
                             return std::unexpected(std::move(accepted.error()));
                           if (!*accepted)
                             return false;
                         }
                         auto collision = lineTileCollision(center, entity->position(), collisionSet);
                         if (!collision)
                           return std::unexpected(std::move(collision.error()));
                         return !*collision;
                       });
}

std::expected<bool, StarException> World::point_collision(Vec2F const& point, CollisionSet const& collision_set) const {
  return collision_blocks_satisfy(RectI::with_center(Vec2I(point), {3, 3}),
      [&](CollisionBlock const& block) -> std::expected<bool, StarException> {
        return collision_set.contains(block.kind) && block.poly.contains(point);
      });
}

std::expected<std::optional<std::pair<Vec2F, std::optional<Vec2F>>>, StarException> World::line_collision(Line2F const& line, CollisionSet const& collision_set) const {
  auto const& geometry = this->geometry();
  auto delta = line.diff();
  std::optional<float> closest;
  std::optional<Vec2F> normal;
  auto region = RectI::integral(RectF::bound_box_of(line.min(), line.max()).padded(1));
  auto scanned = for_each_collision_block(region, [&](CollisionBlock const& block) -> std::expected<void, StarException> {
    if (!block.poly.empty() && collision_set.contains(block.kind)) {
      auto start = geometry.nearest_to(block.poly.center(), line.min());
      auto hit = block.poly.line_intersection(Line2F(start, start + delta));
      if (hit && (!closest || hit->along < *closest)) {
        closest = hit->along;
        normal = hit->intersected_side.transform([&](std::size_t side) { return block.poly.normal(side); });
      }
    }
    return {};
  });
  if (!scanned)
    return std::unexpected(std::move(scanned.error()));
  if (closest)
    return std::make_pair(line.eval(*closest), normal);
  return {};
}

std::expected<bool, StarException> World::poly_collision(PolyF const& poly, CollisionSet const& collision_set) const {
  auto const& geometry = this->geometry();
  auto poly_center = poly.center();
  PolyF translated;
  return collision_blocks_satisfy(RectI::integral(poly.bounding_box()).padded(1),
      [&](CollisionBlock const& block) -> std::expected<bool, StarException> {
        if (!collision_set.contains(block.kind))
          return false;
        auto center = block.poly.center();
        auto offset = geometry.nearest_to(poly_center, center) - center;
        if (offset == Vec2F())
          return poly.intersects(block.poly);
        translated = block.poly;
        translated.translate(offset);
        return poly.intersects(translated);
      });
}

}

module;
import star.json;
import star.image;
import star.data_stream;
import star.bi_map;
import star.vector;
import star.game_types;
import star.random;
import star.set;
import star.thread;
import star.ordered_map;
import star.lru_cache;
import star.rect;
import star.string;
import star.io_device;
import star.list;
import star.image_processing;
import star.hash;
import star.string_view;
import star.directives;
import star.asset_path;
import star.ref_ptr;
import star.listener;
import std;
import star.format;
import star.version;
import star.logging;


import star.item_descriptor;
import star.world_geometry;
import star.dungeon_generator;
import star.asset_source;
import star.assets;
import star.configuration;
import star.root;

module star.micro_dungeon;

import star.algorithm;
namespace Star {

// Placed on server so it can keep a cacheing system which allows for a quick
// scan if a piece fits.

MicroDungeonFactory::MicroDungeonFactory(Root& root) : m_root(root) {
  m_generating = false;

  m_placementshifts.push_back(0);
  for (int i = 1; i < 4; ++i)
    m_placementshifts.push_back(i);
  for (int i = 1; i < 4; ++i)
    m_placementshifts.push_back(-i);
}

std::expected<std::optional<std::pair<List<RectI>, Set<Vec2I>>>, StarException> MicroDungeonFactory::generate(RectI const& bounds,
    String const& dungeonName,
    Vec2I const& position,
    std::uint64_t seed,
    float threatLevel,
    std::shared_ptr<DungeonGeneratorWorldFacade> facade,
    bool forcePlacement) {
  Dungeon::DungeonGeneratorWriter writer(m_root, facade, {}, {});
  if (m_generating)
    return std::unexpected(DungeonException("Not reentrant."));
  m_generating = true;
  auto generatingGuard = finally([this]() { m_generating = false; });
  auto dungeonGenerator = DungeonGenerator::create(m_root, dungeonName, seed, threatLevel, BiomeMicroDungeonId);
  if (!dungeonGenerator) return std::unexpected(std::move(dungeonGenerator.error()));

  auto generate = [&]() -> std::expected<std::optional<std::pair<List<RectI>, Set<Vec2I>>>, StarException> {
    auto anchorPart = (*dungeonGenerator)->pickAnchor();
    if (!anchorPart) return std::unexpected(std::move(anchorPart.error()));
    if (!*anchorPart) {
      auto logged = m_root.logger().debug("No valid anchor piece found for microdungeon at {}, skipping", position);
      if (!logged) return std::unexpected(std::move(logged.error()));
      return {};
    }
    if (forcePlacement) {
      auto built = (*dungeonGenerator)->buildDungeon(*anchorPart, position - (*anchorPart)->anchorPoint(), writer, forcePlacement);
      if (!built) return std::unexpected(std::move(built.error()));
      return std::optional<std::pair<List<RectI>, Set<Vec2I>>>(std::move(*built));
    }
    for (int dy : m_placementshifts) {
      auto pos = position - (*anchorPart)->anchorPoint() + Vec2I{0, dy};
      if (!bounds.contains(pos) || !bounds.contains(pos + Vec2I{(*anchorPart)->size()} - Vec2I{1, 1}))
        continue;
      bool collision = false;
      auto visited = (*anchorPart)->forEachTile([&](Vec2I tilePos, Dungeon::Tile const& tile) -> std::expected<bool, StarException> {
        if (tile.usesPlaces()) {
          auto dungeonId = facade->getDungeonIdAt(pos + tilePos);
          if (!dungeonId) return std::unexpected(std::move(dungeonId.error()));
          if (*dungeonId != NoDungeonId) {
            collision = true;
            return true;
          }
        }
        return false;
      });
      if (!visited) return std::unexpected(std::move(visited.error()));
      if (!collision) {
        auto canPlace = (*anchorPart)->canPlace(pos, writer);
        if (!canPlace) return std::unexpected(std::move(canPlace.error()));
        if (*canPlace) {
          auto built = (*dungeonGenerator)->buildDungeon(*anchorPart, pos, writer, forcePlacement);
          if (!built) return std::unexpected(std::move(built.error()));
          return std::optional<std::pair<List<RectI>, Set<Vec2I>>>(std::move(*built));
        }
      }
    }
    return {};
  };
  auto result = generate();
  if (!result)
    return std::unexpected(DungeonException(Star::format("Error generating microdungeon named '{}'", (*dungeonGenerator)->definition()->name()), ExceptionDiagnostic(result.error())));
  return result;
}

}

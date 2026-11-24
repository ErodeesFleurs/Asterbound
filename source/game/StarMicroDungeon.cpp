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

MicroDungeonFactory::MicroDungeonFactory(Root& root) : m_root(root) {}

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
  auto generatingGuard = make_scope_exit([this]() { m_generating = false; });
  auto dungeonGenerator = DungeonGenerator::create(m_root, dungeonName, seed, threatLevel, biome_micro_dungeon_id);
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
    static constexpr std::array placement_shifts{0, 1, 2, 3, -1, -2, -3};
    for (int dy : placement_shifts) {
      auto pos = position - (*anchorPart)->anchorPoint() + Vec2I{0, dy};
      if (!bounds.contains(pos) || !bounds.contains(pos + Vec2I{(*anchorPart)->size()} - Vec2I{1, 1}))
        continue;
      bool collision = false;
      for (auto offset : (*anchorPart)->occupied_offsets()) {
        auto dungeon_id = facade->getDungeonIdAt(pos + offset);
        if (!dungeon_id) return std::unexpected(std::move(dungeon_id.error()));
        if (*dungeon_id != no_dungeon_id) {
          collision = true;
          break;
        }
      }
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

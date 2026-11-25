module;

import std;
import star.algorithm;
import star.dungeon_generator;
import star.format;
import star.game_types;
import star.logging;
import star.rect;
import star.root;
import star.string;
import star.vector;

module star.micro_dungeon;

namespace Star {

MicroDungeonFactory::MicroDungeonFactory(Root& root) : m_root(root) {}

std::expected<std::optional<DungeonBuildResult>, StarException> MicroDungeonFactory::generate(RectI const& bounds,
    String const& dungeon_name,
    Vec2I const& position,
    std::uint64_t seed,
    float threat_level,
    DungeonGeneratorWorldFacade& facade,
    bool force_placement) {
  if (m_generating)
    return std::unexpected(DungeonException("Not reentrant."));
  m_generating = true;
  auto generating_guard = make_scope_exit([&] { m_generating = false; });
  auto generator = DungeonGenerator::create(m_root, dungeon_name, seed, threat_level, biome_micro_dungeon_id);
  if (!generator) return std::unexpected(std::move(generator.error()));

  auto generate = [&]() -> std::expected<std::optional<DungeonBuildResult>, StarException> {
    auto picked = generator->pick_anchor();
    if (!picked) return std::unexpected(std::move(picked.error()));
    if (!*picked) {
      auto logged = m_root.logger().debug("No valid anchor piece found for microdungeon at {}, skipping", position);
      if (!logged) return std::unexpected(std::move(logged.error()));
      return {};
    }

    auto const& anchor = (*picked)->get();
    using WidePosition = Vector<std::int64_t, 2>;
    auto const anchor_base = WidePosition(position) - WidePosition(anchor.anchorPoint());
    Dungeon::DungeonGeneratorWriter writer(m_root, facade, {}, {});
    std::optional<Vec2I> placement;
    if (force_placement) {
      if (!std::in_range<int>(anchor_base[0]) || !std::in_range<int>(anchor_base[1]))
        return std::unexpected(DungeonException("Dungeon anchor position is outside the coordinate range"));
      placement.emplace(anchor_base);
    } else {
      static constexpr std::array placement_shifts{0, 1, 2, 3, -1, -2, -3};
      Rect<std::int64_t> const wide_bounds(bounds);
      auto const anchor_last_tile = anchor_base + WidePosition(anchor.size()) - WidePosition{1, 1};
      for (int dy : placement_shifts) {
        WidePosition const shift{0, dy};
        auto const shifted = anchor_base + shift;
        if (!wide_bounds.contains(shifted) || !wide_bounds.contains(anchor_last_tile + shift))
          continue;
        Vec2I const pos(shifted);
        bool collision = false;
        for (auto offset : anchor.occupied_offsets()) {
          auto const occupied = shifted + WidePosition(offset);
          if (!std::in_range<int>(occupied[0]) || !std::in_range<int>(occupied[1]))
            return std::unexpected(DungeonException("Dungeon tile position is outside the coordinate range"));
          auto dungeon_id = facade.getDungeonIdAt(Vec2I(occupied));
          if (!dungeon_id) return std::unexpected(std::move(dungeon_id.error()));
          if (*dungeon_id != no_dungeon_id) {
            collision = true;
            break;
          }
        }
        if (collision)
          continue;
        auto can_place = anchor.canPlace(pos, writer);
        if (!can_place) return std::unexpected(std::move(can_place.error()));
        if (*can_place) {
          placement = pos;
          break;
        }
      }
    }
    if (!placement)
      return {};
    auto built = generator->build_dungeon(anchor, *placement, writer, force_placement);
    if (!built) return std::unexpected(std::move(built.error()));
    return std::optional<DungeonBuildResult>(std::move(*built));
  };
  auto result = generate();
  if (!result)
    return std::unexpected(DungeonException(Star::format("Error generating microdungeon named '{}'", generator->definition().name()), ExceptionDiagnostic(result.error())));
  return result;
}

}

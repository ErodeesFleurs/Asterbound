module;

import std;
import star.dungeon_generator;
import star.format;
import star.rect;
import star.string;
import star.vector;

namespace Star {
class Root;
}

export module star.micro_dungeon;

export namespace Star {

class MicroDungeonFactory {
  Root& m_root;
public:
  explicit MicroDungeonFactory(Root& root);

  // A successful empty optional means no placement; errors retain the dungeon
  // generation context and never become a skipped placement.
  [[nodiscard]] std::expected<std::optional<DungeonBuildResult>, StarException> generate(RectI const& bounds,
      String const& dungeon_name,
      Vec2I const& position,
      std::uint64_t seed,
      float threat_level,
      DungeonGeneratorWorldFacade& facade,
      bool force_placement = false);

private:
  bool m_generating = false;
};

}

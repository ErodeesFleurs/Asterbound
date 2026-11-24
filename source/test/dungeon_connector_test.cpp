import std;
import star.format;
import star.string;
import star.vector;
import star.list;
import star.map;
import star.root;
import star.game_test;
import star.dungeon_generator;

#include "gtest/gtest.h"

using namespace Star;

namespace {

const GameTestRegistration connector_candidates("DungeonConnectorTest", "PrisonBucketsMatchOrderedFullScan", [](GameTest& fixture) {
  auto definitions = fixture.root().dungeonDefinitions();
  ASSERT_TRUE(definitions) << definitions.error().what();
  auto definition = (*definitions)->get("humanprison");
  ASSERT_TRUE(definition) << definition.error().what();

  struct Entry {
    std::reference_wrapper<Dungeon::Connector const> connection;
    bool forward_only;
  };
  std::vector<Entry> full_scan;
  for (auto const& [name, part] : (*definition)->parts()) {
    SCOPED_TRACE(name.utf8());
    std::size_t index = 0;
    auto visited = part->forEachTile([&](Vec2I pos, Dungeon::Tile const& tile) -> std::expected<bool, StarException> {
      if (!tile.connector) return false;
      if (index >= part->connections().size()) {
        ADD_FAILURE() << "Tile has no corresponding part connector";
        return true;
      }
      auto const& connection = *part->connections()[index++];
      EXPECT_EQ(pos, connection.offset());
      EXPECT_EQ(tile.connector->value, connection.value());
      full_scan.push_back({std::cref(connection), tile.connector->forwardOnly});
      return false;
    });
    ASSERT_TRUE(visited) << visited.error().what();
    ASSERT_EQ(index, part->connections().size());
  }

  bool saw_multiple_matches = false;
  bool saw_direction_mismatch = false;
  for (auto source : full_scan) {
    auto const& connector = source.connection.get();
    SCOPED_TRACE(connector.part().name().utf8());
    auto indexed = (*definition)->find_connectable_parts(connector);
    ASSERT_TRUE(indexed) << indexed.error().what();
    auto source_step = connector.positionAdjustment();
    std::size_t matched = 0;
    for (auto entry : full_scan) {
      auto const& candidate = entry.connection.get();
      if (candidate.part().doesNotConnectTo(connector.part()) || entry.forward_only || candidate.value() != connector.value()) continue;
      auto candidate_step = candidate.positionAdjustment();
      if (source_step != Vec2I(0, 0) && candidate_step != Vec2I(0, 0) && candidate_step != -source_step) {
        saw_direction_mismatch = true;
        continue;
      }
      ASSERT_LT(matched, indexed->size());
      EXPECT_EQ(&(*indexed)[matched++].get(), &candidate);
    }
    EXPECT_EQ(matched, indexed->size());
    saw_multiple_matches |= matched > 1;
  }
  EXPECT_TRUE(saw_multiple_matches);
  EXPECT_TRUE(saw_direction_mismatch);
});

}

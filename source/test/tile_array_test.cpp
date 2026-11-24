import star.rect;
import star.list;
import star.multi_array;
import star.set;
import star.vector;
import star.thread;
import star.worker_pool;

import std;
import star.format;
import star.sector_array_2d;

import star.worker_pool;
import star.tile_sector_array;
import star.assets;
import star.data_stream;
import star.data_stream_devices;
import star.json;
import star.string;
import star.tile_damage;
import star.collision_block;
import star.collision_generator;
import star.poly;
import star.world_geometry;
import star.world_impl;
import star.world_tiles;

#include "gtest/gtest.h"

import star.array;
import star.game_test;
import star.root;
using namespace Star;

namespace {

const GameTestRegistration tile_damage_union("TileDamageTest", "WeightedUnionDistinguishesMissingAndZero", [](GameTest& fixture) {
  auto assets = fixture.root().assets();
  ASSERT_TRUE(assets);
  auto left = TileDamageParameters::create(**assets, JsonObject{
      {"totalHealth", 4}, {"damageRecovery", 1}, {"harvestLevel", 6}, {"maximumEffectTime", 1},
      {"damageFactors", JsonObject{{"blockish", 2}, {"explosive", 1.5}, {"beamish", 3}, {"protected", 0}}}});
  auto right = TileDamageParameters::create(**assets, JsonObject{
      {"totalHealth", 12}, {"damageRecovery", 2}, {"harvestLevel", 3}, {"maximumEffectTime", 2},
      {"damageFactors", JsonObject{{"blockish", 6}, {"fire", 3}, {"beamish", 0}}}});
  ASSERT_TRUE(left);
  ASSERT_TRUE(right);
  Json const expected = JsonObject{
      {"totalHealth", 16}, {"damageRecovery", 3}, {"requiredHarvestLevel", 6}, {"maximumEffectTime", 2},
      {"damageFactors", JsonObject{{"blockish", 4}, {"explosive", 1.5}, {"beamish", 0}, {"protected", 0}, {"fire", 3}}}};
  for (bool reverse : {false, true}) {
    auto merged = reverse ? right->sum(*left) : left->sum(*right);
    ASSERT_TRUE(merged);
    auto json = merged->to_json();
    ASSERT_TRUE(json);
    EXPECT_EQ(*json, expected);
    EXPECT_FLOAT_EQ(merged->damage_fraction({TileDamageType::Blockish, 1}), .25f);
    EXPECT_FLOAT_EQ(merged->damage_fraction({TileDamageType::Plantish, 10}), 0);
  }
});

const GameTestRegistration tile_damage_health("TileDamageTest", "ZeroHealthAndExplicitImmunity", [](GameTest& fixture) {
  auto assets = fixture.root().assets();
  ASSERT_TRUE(assets);
  Json config = JsonObject{{"damageRecovery", 1}, {"damageFactors", JsonObject{{"fire", 2}, {"blockish", 0}, {"protected", 2}}}};
  auto zero = TileDamageParameters::create(**assets, config, 0);
  ASSERT_TRUE(zero);
  TileDamageStatus tile;
  auto entity = EntityTileDamageStatus::create();
  ASSERT_TRUE(entity);
  (*entity)->reset();
  for (TileDamage damage : {TileDamage{TileDamageType::Blockish, 10}, {TileDamageType::Fire, 0}, {TileDamageType::Plantish, 10}}) {
    tile.damage(*zero, {}, damage);
    (*entity)->damage(*zero, damage);
    EXPECT_EQ(tile.damage_percentage(), 0);
    EXPECT_EQ((*entity)->damage_percentage(), 0);
  }
  TileDamage hit(TileDamageType::Protected, 1);
  tile.damage(*zero, {}, hit);
  (*entity)->damage(*zero, hit);
  EXPECT_EQ(tile.damage_percentage(), 1);
  EXPECT_EQ((*entity)->damage_percentage(), 1);
  tile.recover(*zero, 0);
  (*entity)->recover(*zero, 0);
  EXPECT_EQ(tile.damage_percentage(), 1);
  EXPECT_EQ((*entity)->damage_percentage(), 1);
  tile.recover(*zero, 1);
  (*entity)->recover(*zero, 1);
  EXPECT_TRUE(tile.healthy());
  EXPECT_TRUE((*entity)->healthy());
  auto combined = zero->sum(*zero);
  ASSERT_TRUE(combined);
  hit.type = TileDamageType::Fire;
  tile.damage(*combined, {}, hit);
  (*entity)->damage(*combined, hit);
  EXPECT_TRUE(tile.dead());
  EXPECT_TRUE((*entity)->dead());

  auto positive = TileDamageParameters::create(**assets, JsonObject{
      {"damageRecovery", 0}, {"totalHealth", 4}, {"damageFactors", JsonObject{{"fire", 4}, {"blockish", 4}}}});
  ASSERT_TRUE(positive);
  for (bool reverse : {false, true}) {
    auto mixed = reverse ? zero->sum(*positive) : positive->sum(*zero);
    ASSERT_TRUE(mixed);
    EXPECT_EQ(mixed->damage_fraction(hit), 1);
    EXPECT_EQ(mixed->damage_fraction({TileDamageType::Blockish, 1}), 0);
  }
  EXPECT_FALSE(TileDamageParameters::create(**assets, config, -1));
  auto maximum = TileDamageParameters::create(**assets, config, std::numeric_limits<float>::max());
  ASSERT_TRUE(maximum);
  EXPECT_FALSE(maximum->sum(*maximum));
});

const GameTestRegistration tile_damage_formats("TileDamageTest", "HarvestConfigPersistedJsonAndLegacyWire", [](GameTest& fixture) {
  auto assets = fixture.root().assets();
  ASSERT_TRUE(assets);
  Json config = JsonObject{{"damageRecovery", 1}, {"totalHealth", 4}, {"damageFactors", JsonObject{{"fire", 2}}},
                           {"harvestLevel", 7}, {"requiredHarvestLevel", 3}};
  auto from_config = TileDamageParameters::create(**assets, config);
  ASSERT_TRUE(from_config);
  EXPECT_EQ(from_config->required_harvest_level(), 7u);
  auto persisted = from_config->to_json();
  ASSERT_TRUE(persisted);
  EXPECT_EQ(*persisted, (JsonObject{{"damageRecovery", 1}, {"totalHealth", 4}, {"damageFactors", JsonObject{{"fire", 2}}},
                                    {"requiredHarvestLevel", 7}, {"maximumEffectTime", 1.5}}));
  auto restored = TileDamageParameters::create(**assets, *persisted);
  ASSERT_TRUE(restored);
  EXPECT_EQ(restored->required_harvest_level(), 7u);
  auto overridden = TileDamageParameters::create(**assets, *persisted, {}, 0u);
  ASSERT_TRUE(overridden);
  EXPECT_EQ(overridden->required_harvest_level(), 0u);

  // Map count, enum byte, factor, recovery, unsigned harvest level, effect time, total health.
  constexpr std::array<char, 22> bytes{1, 5, 0x40, 0, 0, 0, 0x3f, '\x80', 0, 0, 0, 0, 0, 7,
                                       0x3f, '\xc0', 0, 0, 0x40, '\x80', 0, 0};
  DataStreamBuffer encoded;
  ASSERT_TRUE(encoded.write(*restored));
  EXPECT_TRUE(std::ranges::equal(encoded.span(), bytes));
  DataStreamExternalBuffer stream(bytes);
  TileDamageParameters decoded;
  ASSERT_TRUE(stream.read(decoded));
  EXPECT_EQ(stream.remaining(), 0u);
  auto decoded_json = decoded.to_json();
  ASSERT_TRUE(decoded_json);
  EXPECT_EQ(*decoded_json, *persisted);
});

TEST(CollisionGenerator, RejectsUnrepresentableHaloBeforeReadingAnyTile) {
  constexpr int low = std::numeric_limits<int>::min();
  constexpr int high = std::numeric_limits<int>::max();
  CollisionGenerator generator;
  unsigned reads = 0;
  generator.init([&](int, int) { ++reads; return CollisionKind::None; });

  for (auto const& region : {
         RectI({low + 1, 0}, {low + 2, 1}), RectI({0, high - 2}, {1, high - 1}),
         RectI({low + 2, 0}, {0, 1}), RectI({0, -2}, {1, high - 2})})
    EXPECT_FALSE(generator.get_blocks(region));
  EXPECT_EQ(reads, 0u);

  auto blocks = generator.get_blocks(RectI({low + 2, high - 3}, {low + 3, high - 2}));
  ASSERT_TRUE(blocks);
  EXPECT_TRUE(blocks->empty());
  EXPECT_EQ(reads, 25u);
}

TEST(CollisionGenerator, AllPlatformNeighborhoodsPreserveDirectedSegmentsAndCapacity) {
  // Legacy directed segment sequences. Bits name the eight neighbors in
  // row-major order from bottom left, skipping the platform at the center.
  // x = rising then falling, R/F = slope followed by its upward extension.
  static constexpr std::string_view sequences =
    "-r--ff------ffff-r-r-r-r--------"
    "ffffffff----ffff-r-r-r-r--------"
    "-r--ff------ff---r---r----------"
    "-f--FF------ff---r---r----------"
    "rrrrrrrr----ffff-r-r-r-r--------"
    "-r-rfxfx----ffff-r-r-r-r--------"
    "-R--rR------ff---r---r----------"
    "-R--Fx-x----ff---r---r----------";
  static_assert(sequences.size() == 256);
  static_assert(CollisionGenerator::maximum_collisions_per_space == 3);
  // The negative x also checks that segment origins are not normalized.
  Vec2I const space(-7, 11);
  unsigned mask = 0;
  CollisionGenerator generator;
  generator.init([&](int x, int y) {
    int dx = x - space[0], dy = y - space[1];
    if (dx < -1 || dx > 1 || dy < -1 || dy > 1)
      return CollisionKind::None;
    int index = (dy + 1) * 3 + dx + 1;
    if (index == 4)
      return CollisionKind::Platform;
    return mask & (1u << (index < 4 ? index : index - 1)) ? CollisionKind::Platform : CollisionKind::Block;
  });
  std::size_t largest = 0;
  for (; mask < sequences.size(); ++mask) {
    SCOPED_TRACE(mask);
    auto blocks = generator.get_blocks(RectI::with_size(space, {1, 1}));
    ASSERT_TRUE(blocks);
    largest = std::max(largest, blocks->size());
    ASSERT_LE(blocks->size(), CollisionGenerator::maximum_collisions_per_space);
    // A non-platform at the upper-left, upper-right or lower-right marching
    // corner contributes exactly one solid polygon, before any segments.
    constexpr unsigned surrounding_corners = (1u << 6) | (1u << 7) | (1u << 4);
    bool has_solid = (mask & surrounding_corners) != surrounding_corners;
    char sequence = sequences[mask];
    bool has_second = sequence == 'x' || sequence == 'R' || sequence == 'F';
    ASSERT_EQ(blocks->size(), std::size_t(has_solid + 1 + has_second));
    if (has_solid)
      EXPECT_EQ((*blocks)[0].kind, CollisionKind::Block);
    for (auto const& block : *blocks) {
      EXPECT_EQ(block.space, space);
      EXPECT_EQ(block.poly_bounds, block.poly.bounding_box());
    }
    auto check_segment = [&](std::size_t index, Vec2F start, Vec2F end) {
      auto const& block = (*blocks)[index];
      EXPECT_EQ(block.kind, CollisionKind::Platform);
      ASSERT_EQ(block.poly.sides(), 2u);
      EXPECT_EQ(block.poly.vertices()[0], Vec2F(space) + start);
      EXPECT_EQ(block.poly.vertices()[1], Vec2F(space) + end);
    };
    if (sequence == '-')
      check_segment(has_solid, {0, 1}, {1, 1});
    else if (sequence == 'f' || sequence == 'F')
      check_segment(has_solid, {1, 0}, {0, 1});
    else
      check_segment(has_solid, {0, 0}, {1, 1});
    if (sequence == 'x')
      check_segment(has_solid + 1, {1, 0}, {0, 1});
    else if (sequence == 'R')
      check_segment(has_solid + 1, {1, 1}, {2, 2});
    else if (sequence == 'F')
      check_segment(has_solid + 1, {0, 1}, {-1, 2});
  }
  EXPECT_EQ(largest, CollisionGenerator::maximum_collisions_per_space);
}

TEST(CollisionGenerator, MarchingMasksAndSpecialCornersPreserveOrderedShapes) {
  // Each digit is a half-tile vertex, clockwise from the bottom-left
  // corner; 8 is the center. A space separates the two hollow-corner parts.
  static constexpr std::array<std::string_view, 20> sequences = {
    "", "123", "345", "1245", "756", "123567", "7346", "12467",
    "017", "0237", "013457", "02457", "0156", "02356", "01346", "0246",
    "5678", "0187", "0238 0856", "0186 6834"
  };
  static constexpr std::array<unsigned, 4> special_masks = {4, 8, 13, 14};
  std::array<Vec2I, 4> const corners = {Vec2I(0, 1), Vec2I(1, 1), Vec2I(1, 0), Vec2I(0, 0)};
  std::array<CollisionKind, 4> const kinds = {
    CollisionKind::Slippery, CollisionKind::Dynamic, CollisionKind::Block, CollisionKind::Dynamic
  };
  std::array<Vec2F, 9> const offsets = {
    Vec2F(.5f, .5f), Vec2F(.5f, 1), Vec2F(.5f, 1.5f), Vec2F(1, 1.5f), Vec2F(1.5f, 1.5f),
    Vec2F(1.5f, 1), Vec2F(1.5f, .5f), Vec2F(1, .5f), Vec2F(1, 1)
  };
  Vec2I const space(5, -7);
  for (unsigned shape = 0; shape < sequences.size(); ++shape) {
    SCOPED_TRACE(shape);
    unsigned mask = shape < 16 ? shape : special_masks[shape - 16];
    CollisionKind expected_kind = CollisionKind::None;
    for (unsigned i = 0; i < corners.size(); ++i)
      if (mask & (1u << i))
        expected_kind = std::max(expected_kind, kinds[i]);
    CollisionGenerator generator;
    generator.init([&](int x, int y) {
      auto delta = Vec2I(x, y) - space;
      for (unsigned i = 0; i < corners.size(); ++i)
        if (delta == corners[i])
          return (mask & (1u << i)) ? kinds[i] : CollisionKind::None;
      if ((shape == 16 && delta == Vec2I(2, 0)) || (shape == 17 && delta == Vec2I(-1, 0)) ||
          (shape == 18 && (delta == Vec2I(0, 2) || delta == Vec2I(2, 0))) ||
          (shape == 19 && (delta == Vec2I(1, 2) || delta == Vec2I(-1, 0))))
        return CollisionKind::Block;
      return CollisionKind::None;
    });
    auto blocks = generator.get_blocks(RectI::with_size(space, {1, 1}));
    ASSERT_TRUE(blocks);
    ASSERT_EQ(blocks->size(), shape == 0 ? 0u : shape >= 18 ? 2u : 1u);
    auto sequence = sequences[shape];
    for (auto const& block : *blocks) {
      EXPECT_EQ(block.space, space);
      EXPECT_EQ(block.kind, expected_kind);
      EXPECT_EQ(block.poly_bounds, block.poly.bounding_box());
      auto end = sequence.find(' ');
      auto vertices = sequence.substr(0, end);
      ASSERT_EQ(block.poly.sides(), vertices.size());
      for (std::size_t i = 0; i < vertices.size(); ++i)
        EXPECT_EQ(block.poly.vertices()[i], Vec2F(space) + offsets[vertices[i] - '0']);
      sequence.remove_prefix(vertices.size() + (end != sequence.npos));
    }
    EXPECT_TRUE(sequence.empty());
  }
}

using VisitGrid = SectorArray2D<int, 2>;

template <typename Grid, typename Function>
concept visits_columns = requires(Grid& grid, Function&& function) {
  grid.eval_columns(0, 0, 1, 1, std::forward<Function>(function));
};

using WriteColumn = decltype([](std::size_t, std::size_t, std::optional<std::span<int>>, std::size_t)
    -> std::expected<bool, StarException> { return true; });
static_assert(visits_columns<VisitGrid, WriteColumn> && !visits_columns<VisitGrid const, WriteColumn>);
static_assert(!visits_columns<VisitGrid, decltype([](auto...) { return true; })>);

struct ReadColumns {
  ReadColumns() = default;
  ReadColumns(ReadColumns const&) = delete;
  std::vector<std::array<int, 3>> visits;

  std::expected<bool, StarException> operator()(std::size_t x, std::size_t y,
      std::optional<std::span<int const>> column, std::size_t count) & {
    visits.push_back({int(x), int(y), column ? int(count) : -int(count)});
    if (column) {
      EXPECT_EQ(column->size(), count);
      for (std::size_t i = 0; i < column->size(); ++i)
        EXPECT_EQ((*column)[i], int(x * 10 + y + i));
    }
    return true;
  }
};

static_assert(WorldImpl::collision_tile_source<ClientTileSectorArray>);
static_assert(WorldImpl::collision_tile_source<ServerTileSectorArray>);
static_assert(!WorldImpl::collision_tile_source<TileSectorArray<int, 2>>);

const GameTestRegistration wrapped_line_collisions("TileSectorArrayTest", "WrappedLineCollisions", [](GameTest&) {
  WorldGeometry geometry(16, 16);
  ClientTileSectorArray tiles(geometry.size());
  tiles.load_default_sector({0, 0});
  CollisionSet const solids{CollisionKind::Block};
  for (Vec2I position : {Vec2I(0, 4), Vec2I(1, 4), Vec2I(6, 6), Vec2I(5, 6), Vec2I(4, 6), Vec2I(3, 6), Vec2I(5, 7)}) {
    auto tile = tiles.modify_tile(position);
    ASSERT_TRUE(tile);
    tile->get().collision = CollisionKind::Block;
  }

  // Tile lookup wraps at the seam; the returned grid coordinates stay in traversal space.
  Vec2F begin(14.5f, 4.5f), end(2.5f, 4.5f);
  auto hits = WorldImpl::colliding_tiles_along_line(geometry, tiles, begin, end, solids, 8, true);
  ASSERT_TRUE(hits);
  EXPECT_EQ(*hits, (List<Vec2I>{{16, 4}, {17, 4}}));
  auto forward = WorldImpl::line_tile_collision_point(geometry, tiles, begin, end, solids);
  ASSERT_TRUE(forward && *forward);
  EXPECT_EQ((*forward)->second, Vec2I(1, 0));
  EXPECT_FLOAT_EQ((*forward)->first[0], 16.0f - 0.0001f);
  EXPECT_FLOAT_EQ((*forward)->first[1], 4.5f);
  auto reverse = WorldImpl::line_tile_collision_point(geometry, tiles, end, begin, solids);
  ASSERT_TRUE(reverse && *reverse);
  EXPECT_EQ((*reverse)->second, Vec2I(-1, 0));
  EXPECT_FLOAT_EQ((*reverse)->first[0], 2.0f + 0.0001f);
  EXPECT_FLOAT_EQ((*reverse)->first[1], 4.5f);

  // Only the floored endpoint collides, even though the segment approaches its corner from above.
  EXPECT_EQ(WorldImpl::line_tile_collision(geometry, tiles, {4.5f, 7.5f}, {5, 7}, solids), true);
  EXPECT_EQ(WorldImpl::line_tile_collision(geometry, tiles, {4.5f, 7.5f}, {4.75f, 7.25f}, solids), false);

  // Travelling left from x=6 only touches tile 6 at the starting boundary.
  // Filtering before limiting would incorrectly admit tile 3 as a replacement.
  begin = {6, 6.5f};
  end = {2.5f, 6.5f};
  auto candidates = WorldImpl::colliding_tiles_along_line(geometry, tiles, begin, end, solids, 3, true);
  ASSERT_TRUE(candidates);
  EXPECT_EQ(*candidates, (List<Vec2I>{{6, 6}, {5, 6}, {4, 6}}));
  auto retained = WorldImpl::colliding_tiles_along_line(geometry, tiles, begin, end, solids, 3, false);
  ASSERT_TRUE(retained);
  EXPECT_EQ(*retained, (List<Vec2I>{{5, 6}, {4, 6}}));
  auto glancing_only = WorldImpl::colliding_tiles_along_line(geometry, tiles, begin, end, solids, 1, false);
  ASSERT_TRUE(glancing_only);
  EXPECT_TRUE(glancing_only->empty());
  auto no_candidates = WorldImpl::colliding_tiles_along_line(geometry, tiles, begin, end, solids, 0, false);
  ASSERT_TRUE(no_candidates);
  EXPECT_TRUE(no_candidates->empty());
});

const GameTestRegistration sector_traversal("TileSectorArrayTest", "SectorTraversal", [](GameTest&) {
  VisitGrid grid(2, 2);
  grid.load_sector({0, 0}, std::make_unique<VisitGrid::Array>());
  grid.load_sector({1, 1}, std::make_unique<VisitGrid::Array>());
  std::vector<int> visited;
  auto filled = grid.eval(0, 1, 4, 3, [&](std::size_t x, std::size_t y, auto element) {
    int value = int(x * 10 + y);
    if (element)
      element->get() = value;
    visited.push_back(element ? value : -value);
    return std::expected<bool, StarException>(true);
  }, true);
  ASSERT_EQ(filled, true);
  EXPECT_EQ(visited, (std::vector<int>{1, 11, -2, -3, -12, -13, -21, -31, 22, 23, 32, 33}));

  auto const& reader = grid;
  ReadColumns columns;
  ASSERT_EQ(reader.eval_columns(1, 1, 3, 3, std::move(columns), true), true);
  EXPECT_EQ(columns.visits, (std::vector<std::array<int, 3>>{
    {1, 1, 1}, {1, 2, -2}, {2, 1, -1}, {3, 1, -1}, {2, 2, 2}, {3, 2, 2}}));
  columns.visits.clear();
  constexpr auto limit = std::numeric_limits<std::size_t>::max();
  EXPECT_EQ(reader.eval_columns(limit, limit, limit, 0, columns, true), true);
  EXPECT_FALSE(reader.eval_columns(0, 1, 4, limit, columns, true));
  EXPECT_TRUE(columns.visits.empty());

  ASSERT_EQ(grid.eval_columns(0, 1, 4, 3, [](std::size_t, std::size_t, auto column, std::size_t) {
    for (auto& value : *column)
      ++value;
    return std::expected<bool, StarException>(true);
  }), true);

  visited.clear();
  auto stopped = reader.eval(0, 1, 4, 3, [&](std::size_t, std::size_t, std::optional<std::reference_wrapper<int const>> element)
      -> std::expected<bool, StarException> {
    if (!element)
      return std::unexpected(StarException("Unexpected unloaded cell"));
    visited.push_back(element->get());
    return element->get() != 24;
  });
  EXPECT_EQ(stopped, false);
  EXPECT_EQ(visited, (std::vector<int>{2, 12, 23, 24}));
});

const GameTestRegistration sector_parallel_drain("TileSectorArrayTest", "SectorParallelDrain", [](GameTest&) {
  VisitGrid const grid(4, 1);
  WorkerPool workers("sector-drain");
  ASSERT_TRUE(workers.start(2));
  ASSERT_EQ(grid.eval_columns_parallel(workers, 0, 0, 1, 1, [&](std::size_t, std::size_t,
      std::optional<std::span<int const>>, std::size_t) -> std::expected<bool, StarException> {
    return workers.is_worker_thread();
  }, true), true);

  auto nested = workers.add_producer<unsigned>([&]() -> std::expected<unsigned, StarException> {
    unsigned calls = 0;
    auto result = grid.eval_columns_parallel(workers, 0, 0, 8, 1, [&](std::size_t, std::size_t,
        std::optional<std::span<int const>>, std::size_t) -> std::expected<bool, StarException> {
      ++calls;
      return false;
    }, true);
    if (!result)
      return std::unexpected(std::move(result.error()));
    return *result ? 0 : calls;
  });
  auto serial_stop = nested.get();
  ASSERT_TRUE(serial_stop);
  EXPECT_EQ(serial_stop->get(), 1u);

  std::array<unsigned, 4> visits{};
  auto stopped = grid.eval_columns_parallel(workers, 0, 0, 8, 1,
      [&](std::size_t x, std::size_t, std::optional<std::span<int const>>, std::size_t) -> std::expected<bool, StarException> {
    ++visits[x / 2];
    return x != 0;
  }, true);
  EXPECT_EQ(stopped, false);
  EXPECT_EQ(visits, (std::array<unsigned, 4>{1, 2, 2, 2}));

  // Two workers: strip 1 waits while strip 2 fails and strip 3 enters. The
  // earlier submitted error must win, and the final borrowed strip must drain.
  std::binary_semaphore last_entered(0);
  std::latch release_first(1);
  std::latch release_last(1);
  StarException first_error("first submitted error");
  auto run = std::async(std::launch::async, [&] {
    return grid.eval_columns_parallel(workers, 0, 0, 8, 1, [&](std::size_t x, std::size_t,
        std::optional<std::span<int const>>, std::size_t) -> std::expected<bool, StarException> {
      if (x == 0)
        return false;
      if (x == 2) {
        release_first.wait();
        return std::unexpected(first_error);
      }
      if (x == 4)
        return std::unexpected(StarException("later submitted error"));
      if (x == 6) {
        last_entered.release();
        release_last.wait();
      }
      return true;
    }, true);
  });
  bool entered = last_entered.try_acquire_for(std::chrono::seconds(5));
  EXPECT_TRUE(entered);
  release_first.count_down();
  if (entered) {
    // The other worker can only process this sentinel after strip 1 exits.
    auto sentinel = workers.add_work([]() -> std::expected<void, StarException> { return {}; });
    EXPECT_EQ(sentinel.wait(5000), true);
    EXPECT_EQ(run.wait_for(std::chrono::milliseconds(20)), std::future_status::timeout);
  }
  release_last.count_down();
  auto failed = run.get();
  ASSERT_TRUE(workers.finish());
  ASSERT_FALSE(failed);
  EXPECT_EQ(failed.error().what(), first_error.what());
});

}

const GameTestRegistration tileSectorArrayAll("TileSectorArrayTest", "All", [](GameTest& fixture) {
  typedef TileSectorArray<int, 32> TileArray;
  TileArray tileSectorArray({100, 100}, -1);

  EXPECT_TRUE(tileSectorArray.sector_valid(TileArray::Sector(1, 1)));
  EXPECT_TRUE(tileSectorArray.sector_valid(TileArray::Sector(3, 3)));
  EXPECT_FALSE(tileSectorArray.sector_valid(TileArray::Sector(4, 4)));

  auto validSectors = tileSectorArray.valid_sectors_for(RectI(0, -32, 64, 32));
  ASSERT_TRUE(validSectors);
  EXPECT_EQ(*validSectors, List<TileArray::Sector>({{0, 0}, {1, 0}}));

  EXPECT_TRUE(TileArray::Sector(0, 0) == tileSectorArray.sector_for({0, 0}));
  EXPECT_TRUE(TileArray::Sector(3, 1) == tileSectorArray.sector_for({-1, 33}));
  EXPECT_TRUE(-1 == tileSectorArray.tile({-1, -1}));
  EXPECT_FALSE(tileSectorArray.modify_tile({-1, -1}));
  EXPECT_TRUE(RectI(32, 32, 64, 64) == tileSectorArray.sector_region({1, 1}));
  EXPECT_TRUE(TileArray::Sector(0, 3) == tileSectorArray.adjacent_sector({3, 3}, {1, 0}));

  tileSectorArray.load_sector({0, 0}, std::make_unique<TileArray::Array>(1));
  tileSectorArray.load_sector({1, 0}, std::make_unique<TileArray::Array>(1));
  tileSectorArray.load_sector({2, 0}, std::make_unique<TileArray::Array>(1));
  tileSectorArray.load_sector({3, 0}, std::make_unique<TileArray::Array>(1));
  tileSectorArray.load_sector({0, 1}, std::make_unique<TileArray::Array>(2));
  tileSectorArray.load_sector({1, 1}, std::make_unique<TileArray::Array>(2));
  tileSectorArray.load_sector({2, 1}, std::make_unique<TileArray::Array>(2));
  tileSectorArray.load_sector({3, 1}, std::make_unique<TileArray::Array>(2));

  Set<Vec2I> found;
  ASSERT_TRUE(tileSectorArray.tile_each(RectI(-2, 0, 3, 1),
      [&found](Vec2I const& pos, int tile) -> std::expected<void, StarException> {
        found.add(pos);
        EXPECT_TRUE(pos[0] >= -2 && pos[0] < 3);
        EXPECT_TRUE(pos[1] == 0);
        EXPECT_EQ(1, tile);
        return {};
      }));
  EXPECT_TRUE(found.contains(Vec2I(0, 0)));
  EXPECT_TRUE(found.contains(Vec2I(-1, 0)));
  EXPECT_TRUE(found.contains(Vec2I(-2, 0)));
  EXPECT_TRUE(found.contains(Vec2I(1, 0)));

  ASSERT_TRUE(tileSectorArray.tile_each(RectI(-10, 0, -1, 1),
      [](Vec2I const& pos, int tile) -> std::expected<void, StarException> {
        EXPECT_TRUE(pos[0] >= -10 && pos[0] < -1);
        EXPECT_EQ(1, tile);
        return {};
      }));

  ASSERT_TRUE(tileSectorArray.tile_each(RectI(-10, -1, -1, 0),
      [](Vec2I const& pos, int tile) -> std::expected<void, StarException> {
        EXPECT_TRUE(pos[0] >= -10 && pos[0] < -1);
        EXPECT_TRUE(pos[1] == -1);
        EXPECT_EQ(-1, tile);
        return {};
      }));

  found.clear();
  ASSERT_TRUE(tileSectorArray.tile_each(RectI(110, 101, 120, 102),
      [&found](Vec2I const& pos, int tile) -> std::expected<void, StarException> {
        found.add(pos);
        EXPECT_TRUE(pos[0] >= 110 && pos[0] < 120);
        EXPECT_TRUE(pos[1] == 101);
        EXPECT_EQ(-1, tile);
        return {};
      }));
  EXPECT_TRUE(found.contains(Vec2I(110, 101)));
  EXPECT_TRUE(found.contains(Vec2I(119, 101)));

  auto workers = fixture.root().workers();
  ASSERT_TRUE(workers);
  auto res1 = tileSectorArray.tile_each_result(workers->get(), RectI(110, 110, 120, 120),
      [](Vec2I const& pos, int tile) -> std::expected<int, StarException> {
        return (pos[0] >= 110 && pos[0] < 120 && pos[1] >= 110 && pos[1] < 120 && tile == -1) ? 1 : 0;
      });
  ASSERT_TRUE(res1);
  MultiArray<int, 2> res1comp({10, 10}, 1);

  EXPECT_TRUE(res1->extents() == res1comp.extents());
  res1->for_each([](Array2S const&, int elem) { EXPECT_TRUE(elem == 1); });

  auto res2 = tileSectorArray.tile_each_result(workers->get(), RectI(32, 32, 64, 64),
      [](Vec2I const& pos, int tile) -> std::expected<int, StarException> {
        return (pos[0] >= 32 && pos[0] < 64 && pos[1] >= 32 && pos[1] < 64 && tile == 2) ? 1 : 0;
      });
  ASSERT_TRUE(res2);
  MultiArray<int, 2> res2comp({32, 32}, 1);

  EXPECT_TRUE(res2->extents() == res2comp.extents());
  res2->for_each([](Array2S const&, int elem) { EXPECT_TRUE(elem == 1); });

  auto res3 = tileSectorArray.tile_each_result(workers->get(), RectI(-10, -10, 1, 1),
      [](Vec2I const& pos, int tile) -> std::expected<int, StarException> {
        if (pos[1] < 0)
          return tile == -1;
        else
          return tile == 1;
      });
  ASSERT_TRUE(res3);
  MultiArray<int, 2> res3comp({11, 11}, 1);

  EXPECT_TRUE(res3->extents() == res3comp.extents());
  res3->for_each([](Array2S const&, int elem) { EXPECT_TRUE(elem == 1); });
});

namespace {

template <typename Grid>
concept can_copy_sector = requires(Grid const& grid) { grid.copy_sector({0, 0}); };

template <typename Grid>
concept can_borrow_sector = requires(Grid&& grid) { std::forward<Grid>(grid).sector({0, 0}); };

template <typename Grid>
concept can_borrow_cell = requires(Grid&& grid) { std::forward<Grid>(grid).get(0, 0); };

template <typename Block>
concept can_index_sector = requires(Block&& block) { std::forward<Block>(block)(0, 0); };

struct AssignOnlyTile {
  int value = 0;
  AssignOnlyTile() = default;
  AssignOnlyTile(AssignOnlyTile const&) = delete;
  AssignOnlyTile& operator=(AssignOnlyTile const&) = default;
};

struct CopyOnlyTile {
  int value = 0;
  CopyOnlyTile() = default;
  CopyOnlyTile(CopyOnlyTile const&) = default;
  CopyOnlyTile& operator=(CopyOnlyTile const&) = delete;
};

using IntGrid = SectorArray2D<int, 2>;
using MoveGrid = SectorArray2D<std::unique_ptr<int>, 2>;
using AssignGrid = SectorArray2D<AssignOnlyTile, 2>;
using CopyGrid = SectorArray2D<CopyOnlyTile, 2>;

static_assert(valid_sector_size<1> && !valid_sector_size<0>
    && !valid_sector_size<std::numeric_limits<std::size_t>::max()>);
static_assert(can_copy_sector<IntGrid> && can_copy_sector<CopyGrid>
    && !can_copy_sector<MoveGrid> && !can_copy_sector<AssignGrid>);
static_assert(std::constructible_from<AssignGrid::Array, AssignOnlyTile const&>
    && !std::constructible_from<CopyGrid::Array, CopyOnlyTile const&>);
static_assert(can_borrow_sector<IntGrid&> && can_borrow_sector<IntGrid const&>
    && !can_borrow_sector<IntGrid> && !can_borrow_sector<IntGrid const>);
static_assert(can_borrow_cell<IntGrid&> && can_borrow_cell<IntGrid const&>
    && !can_borrow_cell<IntGrid> && !can_borrow_cell<IntGrid const>);
static_assert(can_index_sector<IntGrid::Array&> && can_index_sector<IntGrid::Array const&>
    && !can_index_sector<IntGrid::Array> && !can_index_sector<IntGrid::Array const>);
static_assert(std::same_as<decltype(std::declval<IntGrid const&>().sector({})),
    std::optional<std::reference_wrapper<IntGrid::Array const>>>);
static_assert(std::same_as<decltype(std::declval<IntGrid const&>().get(0, 0)),
    std::optional<std::reference_wrapper<int const>>>);

const GameTestRegistration sector_ranges("TileSectorArrayTest", "SectorRanges", [](GameTest&) {
  IntGrid grid(3, 2);
  EXPECT_EQ(grid.width(), 6u);
  EXPECT_EQ(grid.height(), 4u);
  auto clipped_edges = grid.sector_range(1, 1, 5, 2);
  ASSERT_TRUE(clipped_edges);
  EXPECT_EQ(clipped_edges->min, Vec2S(0, 0));
  EXPECT_EQ(clipped_edges->max, Vec2S(3, 2));
  auto exact = grid.sector_range(2, 0, 2, 2);
  ASSERT_TRUE(exact);
  EXPECT_EQ(exact->min, Vec2S(1, 0));
  EXPECT_EQ(exact->max, Vec2S(2, 1));

  constexpr auto limit = std::numeric_limits<std::size_t>::max();
  for (auto empty : {grid.sector_range(1, 1, 0, 2), grid.sector_range(1, 1, 3, 0),
           grid.sector_range(limit, limit, limit, 0)}) {
    ASSERT_TRUE(empty);
    EXPECT_EQ(empty->min, empty->max);
  }
  for (auto invalid : {grid.sector_range(5, 3, 2, 1), grid.sector_range(0, 4, 1, 1),
           grid.sector_range(1, 1, limit, 1), grid.sector_range(limit, limit, 2, 2)}) {
    ASSERT_FALSE(invalid);
    EXPECT_TRUE(invalid.error().is<OutOfRangeException>());
  }
});

const GameTestRegistration sector_ownership("TileSectorArrayTest", "SectorOwnership", [](GameTest&) {
  IntGrid grid(3, 2);
  auto block = std::make_unique<IntGrid::Array>();
  EXPECT_EQ(block->elements, (std::array<int, 4>{}));
  (*block)(1, 0) = 7;
  EXPECT_EQ(block->elements, (std::array<int, 4>{0, 0, 7, 0}));
  grid.load_sector({1, 0}, std::move(block));
  auto copy = std::as_const(grid).copy_sector({1, 0});
  ASSERT_TRUE(copy);
  auto cell = grid.get(3, 0);
  ASSERT_TRUE(cell);
  cell->get() = 9;
  EXPECT_EQ((*copy)(1, 0), 7);
  EXPECT_FALSE(grid.get(6, 0));
  EXPECT_FALSE(grid.sector({3, 0}));
  EXPECT_FALSE(grid.is_sector_loaded({3, 0}));
  EXPECT_FALSE(grid.copy_sector({0, 0}));

  grid.load_sector({1, 0}, std::make_unique<IntGrid::Array>(13));
  EXPECT_EQ(grid.loaded_sector_count(), 1u);
  ASSERT_TRUE(grid.get(3, 0));
  EXPECT_EQ(grid.get(3, 0)->get(), 13);
  grid.load_sector({0, 1}, std::make_unique<IntGrid::Array>(21));
  auto snapshot = grid.loaded_sectors();
  ASSERT_EQ(snapshot.size(), 2u);
  EXPECT_EQ(std::ranges::count(snapshot, Vec2S(1, 0)), 1);
  EXPECT_EQ(std::ranges::count(snapshot, Vec2S(0, 1)), 1);
  for (auto const& id : snapshot) {
    ASSERT_TRUE(grid.is_sector_loaded(id));
    auto taken = grid.take_sector(id);
    ASSERT_TRUE(taken);
    EXPECT_EQ((*taken)(0, 0), id[0] ? 13 : 21);
    EXPECT_FALSE(grid.is_sector_loaded(id));
    EXPECT_FALSE(grid.take_sector(id));
  }
  EXPECT_EQ(grid.loaded_sector_count(), 0u);
  grid.load_sector({0, 0}, std::move(copy));
  grid.load_sector({0, 0}, {});
  EXPECT_FALSE(grid.is_sector_loaded({0, 0}));
  EXPECT_TRUE(grid.loaded_sectors().empty());
  grid.load_sector({0, 0}, std::make_unique<IntGrid::Array>());
  grid.discard_sector({0, 0});
  EXPECT_TRUE(grid.loaded_sectors().empty());
  grid.load_sector({1, 1}, std::make_unique<IntGrid::Array>());
  grid.init(2, 2);
  EXPECT_FALSE(grid.is_sector_loaded({1, 1}));
  EXPECT_TRUE(grid.loaded_sectors().empty());
});

const GameTestRegistration sector_element_contracts("TileSectorArrayTest", "SectorElementContracts", [](GameTest&) {
  AssignOnlyTile fill;
  fill.value = 17;
  AssignGrid::Array filled(fill);
  for (auto const& tile : filled.elements)
    EXPECT_EQ(tile.value, 17);

  CopyGrid copy_grid(1, 1);
  auto copyable = std::make_unique<CopyGrid::Array>();
  (*copyable)(1, 1).value = 31;
  copy_grid.load_sector({0, 0}, std::move(copyable));
  auto copy = std::as_const(copy_grid).copy_sector({0, 0});
  ASSERT_TRUE(copy);
  EXPECT_EQ((*copy)(1, 1).value, 31);

  MoveGrid move_grid(1, 1);
  auto movable = std::make_unique<MoveGrid::Array>();
  (*movable)(1, 0) = std::make_unique<int>(47);
  move_grid.load_sector({0, 0}, std::move(movable));
  auto taken = move_grid.take_sector({0, 0});
  ASSERT_TRUE(taken);
  ASSERT_TRUE((*taken)(1, 0));
  EXPECT_EQ(*(*taken)(1, 0), 47);
});

}

namespace {

// Every index is an actual int&, but all indices refer to the same cell.
struct AliasingTileTarget {
  std::size_t height = 0;
  int value = 0;
  std::expected<void, StarException> set_size(Vec2S const& shape) {
    height = shape[1];
    value = 0;
    return {};
  }
  std::size_t extent(std::size_t) const { return height; }
  int& operator[](std::size_t) { return value; }
  int& operator()(std::size_t, std::size_t) { return value; }
};

const GameTestRegistration tile_output_storage("TileSectorArrayTest", "ProxyAndAliasedOutput", [](GameTest& fixture) {
  auto pool = fixture.root().workers();
  ASSERT_TRUE(pool);
  auto& workers = pool->get();
  TileSectorArray<int, 2> tiles({5, 3}, -1);
  tiles.load_sector({0, 0}, std::make_unique<TileSectorArray<int, 2>::Array>(17));
  auto caller = std::this_thread::get_id();

  // More than two wraps, with both unbacked rows and unloaded sectors.
  RectI region(-7, -1, 8, 4);
  auto bits = tiles.tile_each_result(workers, region,
      [&](Vec2I const& pos, int tile) -> std::expected<bool, StarException> {
        if (std::this_thread::get_id() != caller) return std::unexpected(StarException("packed output visited off caller thread"));
        if (tile != tiles.tile(pos)) return std::unexpected(StarException("unexpected wrapped source tile"));
        return (pos[0] + pos[1]) % 2 == 0;
      });
  ASSERT_TRUE(bits);
  EXPECT_EQ(bits->extents(), (Array2S{15, 5}));
  for (int x = region.x_min(); x < region.x_max(); ++x) {
    for (int y = region.y_min(); y < region.y_max(); ++y)
      EXPECT_EQ(static_cast<bool>((*bits)(x - region.x_min(), y - region.y_min())), (x + y) % 2 == 0);
  }

  AliasingTileTarget target;
  auto written = tiles.tile_each_to(workers, target, region,
      [&](int& output, Vec2I const&, int) -> std::expected<void, StarException> {
        if (std::this_thread::get_id() != caller) return std::unexpected(StarException("aliased output visited off caller thread"));
        ++output;
        return {};
      });
  ASSERT_TRUE(written);
  EXPECT_EQ(target.value, 75);

  // Interiors use one const copy per split, whereas default rows advance the
  // original callback, even if the in-world source is also the default tile.
  struct SequenceWriter {
    std::vector<int>& calls;
    mutable int next = 0;
    std::expected<void, StarException> operator()(int&, Vec2I const&, int) const {
      calls.push_back(++next);
      return {};
    }
  };
  std::vector<int> calls;
  SequenceWriter sequence{calls};
  ASSERT_TRUE(tiles.tile_each_to(workers, target, RectI(4, -1, 6, 1), sequence));
  EXPECT_EQ(calls, (std::vector<int>{1, 1, 2, 2}));
  EXPECT_EQ(sequence.next, 2);

  auto const& read_only = tiles;
  std::vector<Vec2I> columns;
  auto scanned = read_only.tile_each_columns(RectI(-1, -1, 2, 3),
      [&](Vec2I const& pos, std::span<int const> column) -> std::expected<void, StarException> {
        EXPECT_EQ(column.size(), 2u);
        EXPECT_EQ(column.front(), 17);
        columns.push_back(pos);
        return {};
      });
  ASSERT_TRUE(scanned);
  EXPECT_EQ(columns, (std::vector<Vec2I>{{0, 0}, {1, 0}}));
});

const GameTestRegistration tile_geometry_extremes("TileSectorArrayTest", "EmptyWorldAndExtremeWrap", [](GameTest& fixture) {
  auto pool = fixture.root().workers();
  ASSERT_TRUE(pool);
  TileSectorArray<int, 2> empty({0, 4}, -7);
  EXPECT_FALSE(empty.tile_loaded({1, 1}));
  EXPECT_FALSE(empty.modify_tile({1, 1}));
  EXPECT_EQ(empty.tile({1, 1}), -7);
  auto sectors = empty.valid_sectors_for(RectI(-2, -2, 2, 2));
  ASSERT_TRUE(sectors);
  EXPECT_TRUE(sectors->empty());
  std::size_t reads = 0;
  auto visited = empty.tile_each(RectI(-2, -2, 2, 2), [&](Vec2I const&, int tile) -> std::expected<void, StarException> {
    EXPECT_EQ(tile, -7);
    ++reads;
    return {};
  });
  ASSERT_TRUE(visited);
  EXPECT_EQ(reads, 16u);
  auto defaults = empty.tile_each_result(pool->get(), RectI(2, -1, 4, 2),
      [](Vec2I const& pos, int tile) -> std::expected<int, StarException> { return tile + pos[0] * 10 + pos[1]; });
  ASSERT_TRUE(defaults);
  for (int x = 0; x < 2; ++x) {
    for (int y = 0; y < 3; ++y)
      EXPECT_EQ((*defaults)(x, y), -7 + (x + 2) * 10 + y - 1);
  }

  TileSectorArray<int, 2> tiles({5, 2}, -1);
  tiles.load_sector({1, 0}, std::make_unique<TileSectorArray<int, 2>::Array>(9));
  int low = std::numeric_limits<int>::min(), high = std::numeric_limits<int>::max();
  for (int x : {low, high - 3}) {
    auto values = tiles.tile_each_result(pool->get(), RectI(x, 0, x + 3, 1),
        [](Vec2I const&, int tile) -> std::expected<int, StarException> { return tile; });
    ASSERT_TRUE(values);
    for (int i = 0; i < 3; ++i) {
      int canonical_x = ((x + i) % 5 + 5) % 5;
      EXPECT_EQ((*values)(i, 0), canonical_x == 2 || canonical_x == 3 ? 9 : -1);
    }
  }
  auto no_rows = tiles.valid_sectors_for(RectI(low, 3, high, 4));
  ASSERT_TRUE(no_rows);
  EXPECT_TRUE(no_rows->empty());
  EXPECT_FALSE(tiles.tile_satisfies({high, 0}, 1, [](Vec2I const&, int) -> std::expected<bool, StarException> { return false; }));
});

const GameTestRegistration wrapped_cache_rebuild("TileSectorArrayTest", "WrappedCollisionCacheRebuild", [](GameTest&) {
  TileSectorArray<ServerTile, 4> tiles({12, 4});
  tiles.load_default_sector({0, 0});
  tiles.load_default_sector({2, 0});
  ASSERT_TRUE(tiles.tile_eval({0, 0, 12, 4}, [](Vec2I const&, ServerTile& tile) -> std::expected<void, StarException> {
    tile.update_collision(CollisionKind::Platform);
    return {};
  }));
  bool fail = false;
  int samples = 0;
  CollisionGenerator generator;
  generator.init([&](int x, int y) -> std::expected<CollisionKind, StarException> {
    ++samples;
    if (fail) return std::unexpected(StarException("collision sample failed"));
    return tiles.tile({x, y}).getCollision();
  });

  auto expect_flat = [&](int x) {
    auto const& cache = tiles.tile({x, 1}).collision_cache;
    ASSERT_EQ(cache.size(), 1u);
    auto const& block = cache.front();
    EXPECT_EQ(block.kind, CollisionKind::Platform);
    EXPECT_EQ(block.space, Vec2I(x, 1));
    ASSERT_EQ(block.poly.sides(), 2u);
    EXPECT_EQ(block.poly.vertices()[0], Vec2F(x, 2));
    EXPECT_EQ(block.poly.vertices()[1], Vec2F(x + 1, 2));
    EXPECT_EQ(block.poly_bounds, block.poly.bounding_box());
  };
  ASSERT_TRUE(WorldImpl::refresh_collision_cache(tiles, {-1, 1, 1, 2}, generator));
  expect_flat(-1);
  expect_flat(0);

  WorldImpl::invalidate_collision_cache(tiles, {-1, 1, 1, 2});
  fail = true;
  EXPECT_FALSE(WorldImpl::refresh_collision_cache(tiles, {-1, 1, 1, 2}, generator));
  EXPECT_TRUE(tiles.tile({-1, 1}).collision_cache_dirty);
  expect_flat(-1); // Failure cannot publish empty or partially generated geometry.
  fail = false;
  RectI const multi_wrap(-20, 1, 28, 2);
  ASSERT_TRUE(WorldImpl::refresh_collision_cache(tiles, multi_wrap, generator));
  for (int x : {0, 1, 2, 3, 8, 9, 10, 11}) expect_flat(x);

  samples = 0;
  ASSERT_TRUE(WorldImpl::refresh_collision_cache(tiles, multi_wrap, generator));
  EXPECT_EQ(samples, 0);
  tiles.modify_tile({0, 1})->get().collision_cache_dirty = true;
  tiles.modify_tile({11, 1})->get().collision_cache_dirty = true;
  ASSERT_TRUE(WorldImpl::refresh_collision_cache(tiles, multi_wrap, generator));
  EXPECT_EQ(samples, 2 * 5 * 5); // Two one-tile rebuilds with their full halos.

  int visits = 0;
  auto any = WorldImpl::collision_blocks_satisfy(tiles, {4, 1, 8, 2}, generator,
      [](Vec2I const&, ServerTile const& tile) { return tile.getCollision(); },
      [&](CollisionBlock const& block) -> std::expected<bool, StarException> {
        ++visits;
        EXPECT_EQ(block.kind, CollisionKind::Null);
        EXPECT_EQ(block.space, Vec2I(4, 1));
        return true;
      });
  ASSERT_TRUE(any);
  EXPECT_TRUE(*any);
  EXPECT_EQ(visits, 1);

  tiles.unload_sector({0, 0});
  WorldImpl::invalidate_collision_cache(tiles, tiles.sector_region({0, 0}));
  EXPECT_TRUE(tiles.tile({11, 1}).collision_cache_dirty);
  EXPECT_TRUE(tiles.tile({10, 1}).collision_cache_dirty);
  EXPECT_FALSE(tiles.tile({9, 1}).collision_cache_dirty);

  WorldImpl::invalidate_collision_cache(tiles, {std::numeric_limits<int>::min(), std::numeric_limits<int>::min(),
      std::numeric_limits<int>::max(), std::numeric_limits<int>::max()});
  EXPECT_TRUE(tiles.tile({9, 1}).collision_cache_dirty);
});

// Use real fixed-capacity storage with a smaller limit to force publication
// failure. Production tiles can hold every legal single-coordinate result.
struct SingleBlockTile : ServerTile {
  StaticList<CollisionBlock, 1> collision_cache;
};

const GameTestRegistration cache_refill_retry("TileSectorArrayTest", "CollisionRefillFailureRetriesWholeWindow", [](GameTest&) {
  TileSectorArray<SingleBlockTile, 4> tiles({4, 4});
  tiles.load_default_sector({0, 0});
  CollisionGenerator generator;
  generator.init([&](int x, int y) -> std::expected<CollisionKind, StarException> {
    return tiles.tile({x, y}).getCollision();
  });
  tiles.modify_tile({1, 1})->get().update_collision(CollisionKind::Platform);
  RectI const row(0, 1, 3, 2);
  ASSERT_TRUE(WorldImpl::refresh_collision_cache(tiles, row, generator));
  ASSERT_FALSE(tiles.tile({1, 1}).collision_cache_dirty);
  for (Vec2I pos : {Vec2I(0, 0), Vec2I(0, 2), Vec2I(2, 0), Vec2I(2, 2)})
    tiles.modify_tile(pos)->get().update_collision(CollisionKind::Platform);
  tiles.modify_tile({0, 1})->get().collision_cache_dirty = true;
  tiles.modify_tile({2, 1})->get().collision_cache_dirty = true;
  ASSERT_FALSE(WorldImpl::refresh_collision_cache(tiles, row, generator));
  for (int x = 0; x < 3; ++x) EXPECT_TRUE(tiles.tile({x, 1}).collision_cache_dirty);
  tiles.modify_tile({0, 2})->get().update_collision(CollisionKind::None);
  ASSERT_TRUE(WorldImpl::refresh_collision_cache(tiles, row, generator));
  for (int x = 0; x < 3; ++x) EXPECT_FALSE(tiles.tile({x, 1}).collision_cache_dirty);
  ASSERT_EQ(tiles.tile({1, 1}).collision_cache.size(), 1u);
  EXPECT_EQ(tiles.tile({1, 1}).collision_cache.front().poly.vertices()[0], Vec2F(1, 1));
});

const GameTestRegistration effective_tile_collision("TileSectorArrayTest", "ObjectCollisionInvalidation", [](GameTest&) {
  ServerTile tile;
  ASSERT_TRUE(tile.update_collision(CollisionKind::Block));
  tile.collision_cache_dirty = false;
  EXPECT_FALSE(tile.update_object_collision(CollisionKind::Platform));
  EXPECT_FALSE(tile.collision_cache_dirty);
  EXPECT_TRUE(tile.update_collision(CollisionKind::None));
  EXPECT_EQ(tile.getCollision(), CollisionKind::Platform);
  EXPECT_TRUE(tile.collision_cache_dirty);
  ASSERT_TRUE(tile.update_object_collision(CollisionKind::Dynamic));
  tile.collision_cache_dirty = false;
  EXPECT_FALSE(tile.update_collision(CollisionKind::Block));
  EXPECT_FALSE(tile.collision_cache_dirty);
  EXPECT_TRUE(tile.update_object_collision(CollisionKind::None));
  EXPECT_EQ(tile.getCollision(), CollisionKind::Block);
});

}

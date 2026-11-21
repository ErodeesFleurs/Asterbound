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

#include "gtest/gtest.h"

import star.array;
import star.game_test;
import star.root;
using namespace Star;

namespace {

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
  EXPECT_STREQ(failed.error().what(), first_error.what());
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

}

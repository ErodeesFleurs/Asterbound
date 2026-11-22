#include "gtest/gtest.h"

import std;
import star.format;
import star.list;
import star.rect;
import star.spatial_hash_2d;
import star.vector;

using namespace Star;

namespace {

using Grid = SpatialHash2D<int, float, int>;

struct Payload {
  explicit Payload(int id) : id(id) {}
  Payload(Payload&&) = default;
  Payload& operator=(Payload&&) = default;
  int id;
};

using PayloadGrid = SpatialHash2D<int, float, Payload>;

struct PayloadVisitor {
  PayloadVisitor() = default;
  PayloadVisitor(PayloadVisitor&&) = default;
  void operator()(Payload const& payload) & { sum += payload.id; }
  int sum = 0;
};

template <typename T>
concept copies_query_values = requires(T const& grid, typename T::Rect const& rect) { grid.query_values(rect); };

template <typename T>
concept borrows_temporary = requires(T&& grid) { std::move(grid).find_ref(1); };

static_assert(!std::is_copy_constructible_v<Grid> && !std::is_copy_assignable_v<Grid>);
static_assert(std::is_move_constructible_v<PayloadGrid>);
static_assert(copies_query_values<Grid> && !copies_query_values<PayloadGrid>);
static_assert(!borrows_temporary<Grid> && !borrows_temporary<Grid const>);

TEST(SpatialHash2DTest, ClosedRectanglesMatchBruteForceAcrossSectorEdges) {
  auto grid = Grid::create(4);
  ASSERT_TRUE(grid);
  std::vector<std::vector<RectF>> const boxes{
    {{-8, -8, -4, -4}}, {{-4, -4, 0, 0}}, {{0, 0, 4, 4}}, {{4, 4, 4, 4}},
    {{-4, -8, -4, 8}}, {{-8, 0, 8, 0}}, {{-8.5f, -0.5f, -8.25f, 0.5f}},
    {{-8, -2, -7, 2}, {7, -2, 8, 2}, {-8, -2, -7, 2}}, {RectF::null()}, {}};
  for (int id = 0; id < static_cast<int>(boxes.size()); ++id)
    ASSERT_TRUE(grid->set(id, boxes[id], id));
  ASSERT_EQ(grid->size(), boxes.size());

  for (float size : {4.f, 1.f, 2.5f}) {
    ASSERT_TRUE(grid->set_sector_size(size));
    for (float x : {-8.5f, -8.f, -4.25f, -4.f, -0.125f, 0.f, 3.99f, 4.f, 8.f}) {
      for (float y : {-8.f, -4.f, -0.25f, 0.f, 4.f, 8.f}) {
        SCOPED_TRACE(Star::format("size {} x {} y {}", size, x, y));
        for (RectF const& probe : {RectF(x, y, x, y), RectF(x, y, x, y + 4),
                                    RectF(x, y, x + 4, y), RectF(x, y, x + 1, y + 1)}) {
          std::array queries{probe, RectF::null(), probe};
          std::vector<int> expected;
          for (int id = 0; id < static_cast<int>(boxes.size()); ++id)
            if (std::ranges::any_of(boxes[id], [&](RectF const& stored) {
                  return !stored.is_null() && stored.intersects(probe);
                }))
              expected.push_back(id);
          auto actual = grid->query_values(queries);
          ASSERT_TRUE(actual) << actual.error().what();
          std::ranges::sort(*actual);
          EXPECT_TRUE(std::ranges::equal(*actual, expected));
        }
      }
    }
  }
  auto both = grid->query_values(std::array{RectF(-8, -2, -7, 2), RectF(7, -2, 8, 2)});
  ASSERT_TRUE(both);
  EXPECT_EQ(std::ranges::count(*both, 7), 1);
}

TEST(SpatialHash2DTest, InvalidGeometryAndReindexLeaveBothIndexesAndValueIntact) {
  using NarrowGrid = SpatialHash2D<int, float, int, std::int8_t>;
  auto runtime_float = [](std::uint32_t bits) {
    volatile std::uint32_t input = bits;
    return std::bit_cast<float>(static_cast<std::uint32_t>(input));
  };
  for (auto bits : {0u, 0x80000000u, 0xbf800000u, 0x7fc00001u, 0x7f800000u, 0xff800000u}) {
    SCOPED_TRACE(Star::format("create sector size bits {:08x}", bits));
    EXPECT_FALSE(NarrowGrid::create(runtime_float(bits)));
  }
  auto grid = NarrowGrid::create(2);
  ASSERT_TRUE(grid);
  RectF const original(200, 2, 202, 4);
  ASSERT_TRUE(grid->set(1, original, 11));
  auto unchanged = [&] {
    ASSERT_EQ(grid->size(), 1u);
    auto hits = grid->query_values(original);
    ASSERT_TRUE(hits) << hits.error().what();
    ASSERT_EQ(hits->size(), 1u);
    EXPECT_EQ((*hits)[0], 11);
    auto miss = grid->query_values(RectF(0, 0, 0, 0));
    ASSERT_TRUE(miss);
    EXPECT_TRUE(miss->empty());
    auto const& stored = grid->entries().begin()->second.rects;
    ASSERT_EQ(stored.size(), 1u);
    EXPECT_EQ(stored[0], original);
  };
  auto const nan = runtime_float(0x7fc00001u);
  auto const inf = runtime_float(0x7f800000u);
  auto const neg_inf = runtime_float(0xff800000u);
  for (auto bad : {RectF(1, 0, 0, 0), RectF(0, 1, 1, 0), RectF(nan, 0, 1, 1),
                   RectF(0, 0, inf, 1), RectF(0, neg_inf, 1, 1), RectF(0, 0, 1, nan),
                   RectF(256, 0, 256, 0), RectF(-257, 0, -257, 0)}) {
    SCOPED_TRACE(Star::format("rect endpoint bits {:08x} {:08x} {:08x} {:08x}",
        std::bit_cast<std::uint32_t>(bad.x_min()), std::bit_cast<std::uint32_t>(bad.y_min()),
        std::bit_cast<std::uint32_t>(bad.x_max()), std::bit_cast<std::uint32_t>(bad.y_max())));
    std::array rects{RectF(0, 0, 0, 0), bad};
    EXPECT_FALSE(grid->set(1, rects));
    EXPECT_FALSE(grid->set(1, rects, 99));
    EXPECT_FALSE(grid->set(2, rects, 22));
    int calls = 0;
    EXPECT_FALSE(grid->for_each(std::array{original, bad}, [&](int) { ++calls; }));
    EXPECT_EQ(calls, 0);
    unchanged();
  }
  for (auto bits : {0u, 0xc0000000u, 0x7fc00001u, 0x7f800000u, 0xff800000u, 0x3f800000u}) {
    SCOPED_TRACE(Star::format("reindex sector size bits {:08x}", bits));
    EXPECT_FALSE(grid->set_sector_size(runtime_float(bits)));
    unchanged();
  }
  EXPECT_FALSE(grid->set(9, Vec2F(0, 0)));
  ASSERT_TRUE(grid->set(1, grid->entries().begin()->second.rects));
  unchanged();
  std::array moved{RectF(-8, -4, -4, 0), RectF(4, 0, 8, 4)};
  ASSERT_TRUE(grid->set(1, moved));
  // Force a real change from a subrange of the very storage being replaced.
  auto const& rects = grid->entries().begin()->second.rects;
  ASSERT_TRUE(grid->set(1, std::ranges::subrange(rects.begin() + 1, rects.end()), 22));
  auto hit = grid->query_values(moved[1]);
  ASSERT_TRUE(hit);
  ASSERT_EQ(hit->size(), 1u);
  EXPECT_EQ((*hit)[0], 22);
  auto miss = grid->query_values(moved[0]);
  ASSERT_TRUE(miss);
  EXPECT_TRUE(miss->empty());
}

TEST(SpatialHash2DTest, IntegerLimitsHugeSparseQueriesAndOversizedEntryTransitions) {
  using IntegerGrid = SpatialHash2D<int, int, int>;
  auto grid = IntegerGrid::create(1);
  ASSERT_TRUE(grid);
  constexpr int low = std::numeric_limits<int>::min(), high = std::numeric_limits<int>::max();
  RectI const all(low, low, high, high);
  ASSERT_TRUE(grid->set(1, Vec2I(low, high), 1));
  ASSERT_TRUE(grid->set(2, Vec2I(high, low), 2));
  ASSERT_TRUE(grid->set(3, all, 3));
  auto check = [&](RectI query, std::initializer_list<int> expected) {
    auto hits = grid->query_values(query);
    ASSERT_TRUE(hits) << hits.error().what();
    std::ranges::sort(*hits);
    EXPECT_TRUE(std::ranges::equal(*hits, expected));
  };
  check(all, {1, 2, 3});
  check(RectI(low, high, low, high), {1, 3});
  check(RectI(high, low, high, low), {2, 3});
  check(RectI(-1, -1, 1, 1), {3});
  ASSERT_TRUE(grid->set_sector_size(high));
  check(RectI(high, low, high, low), {2, 3});
  ASSERT_TRUE(grid->set_sector_size(1));
  ASSERT_TRUE(grid->set(3, Vec2I(0, 0)));
  check(RectI(low, high, low, high), {1});
  check(RectI(0, 0, 0, 0), {3});
  ASSERT_TRUE(grid->set(3, all));
  EXPECT_EQ(grid->remove(3), std::optional<int>(3));
  check(all, {1, 2});
}

TEST(SpatialHash2DTest, FloatingSectorConversionChecksExclusiveUpperLimit) {
  auto grid = Grid::create(1);
  ASSERT_TRUE(grid);
  // INT_MAX rounds up to 2^31 as a float. Comparing with float(INT_MAX)
  // would accept that invalid sector and then cast it to a signed int.
  constexpr float limit = 0x1p31f;
  constexpr float last = 0x1.fffffep30f;
  ASSERT_TRUE(grid->set(1, Vec2F(last, -limit), 1));
  EXPECT_FALSE(grid->set(2, Vec2F(limit, 0), 2));
  EXPECT_FALSE(grid->set(2, Vec2F(0, -0x1.000002p31f), 2));
  EXPECT_FALSE(grid->set_sector_size(std::numeric_limits<float>::min()));
  auto last_hit = grid->query_values(RectF(last, -limit, last, -limit));
  ASSERT_TRUE(last_hit);
  ASSERT_EQ(last_hit->size(), 1u);
  EXPECT_EQ((*last_hit)[0], 1);
}

TEST(SpatialHash2DTest, MoveOnlyPayloadAndLvalueVisitorSurviveOwnerTransfer) {
  auto created = PayloadGrid::create(2);
  ASSERT_TRUE(created);
  RectF box(0, 0, 2, 2);
  ASSERT_TRUE(created->set(1, box, Payload(3)));
  ASSERT_TRUE(created->set(2, box, Payload(5)));
  PayloadGrid grid(std::move(*created));
  PayloadVisitor visitor;
  ASSERT_TRUE(grid.for_each(box, visitor));
  EXPECT_EQ(visitor.sum, 8);
  auto removed = grid.remove(2);
  ASSERT_TRUE(removed);
  EXPECT_EQ(removed->id, 5);
  ASSERT_TRUE(grid.for_each(box, visitor));
  EXPECT_EQ(visitor.sum, 11);
}

TEST(SpatialHash2DTest, SnapshotMembershipSurvivesNestedMutationsAndFirstVisitorError) {
  auto grid = Grid::create(2);
  ASSERT_TRUE(grid);
  RectF const here(0, 0, 4, 4), away(20, 20, 22, 22);
  for (int id : {1, 2, 3})
    ASSERT_TRUE(grid->set(id, here, id));
  std::vector<std::reference_wrapper<Grid::Entry const>> identities;
  for (auto const& item : grid->entries())
    identities.emplace_back(item.second);
  std::ranges::sort(identities, std::less{}, [](auto entry) { return std::addressof(entry.get()); });
  std::vector<int> expected_order;
  for (auto entry : identities)
    expected_order.push_back(entry.get().value);
  std::vector<int> single_cell_order;
  ASSERT_TRUE(grid->for_each(RectF(0, 0, 0, 0), [&](int id) { single_cell_order.push_back(id); }));
  EXPECT_EQ(single_cell_order, expected_order);
  std::vector<int> outer;
  auto walked = grid->for_each(std::array{here, here}, [&](int id) -> std::expected<void, StarException> {
    outer.push_back(id);
    if (outer.size() == 1) {
      for (int key : {1, 2, 3}) {
        auto moved = grid->set(key, away);
        if (!moved) return moved;
      }
      auto added = grid->set(4, here, 4);
      if (!added) return added;
      auto resized = grid->set_sector_size(1);
      if (!resized) return resized;
      std::vector<int> inner;
      auto nested = grid->for_each(here, [&](int value) { inner.push_back(value); });
      if (!nested) return nested;
      EXPECT_EQ(inner, std::vector<int>{4});
    }
    return {};
  });
  ASSERT_TRUE(walked) << walked.error().what();
  EXPECT_EQ(outer, expected_order);
  int calls = 0;
  auto failed = grid->for_each(away, [&](int) -> std::expected<void, StarException> {
    if (++calls == 2) return std::unexpected(OutOfRangeException("visitor stopped"));
    return {};
  });
  ASSERT_FALSE(failed);
  EXPECT_TRUE(failed.error().is<OutOfRangeException>());
  EXPECT_EQ(calls, 2);
}

} // namespace

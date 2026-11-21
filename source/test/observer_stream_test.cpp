import std;
import star.format;
import star.list;
import star.observer_stream;

#include "gtest/gtest.h"

using namespace Star;

TEST(ObserverStreamTest, ZeroLimitAdvancesWithoutRetainingEvents) {
  ObserverStream<int> stream;
  stream.push_back(10);
  stream.tick(100);
  stream.push_back(20);
  auto [values, cursor] = stream.query();
  EXPECT_TRUE(values.empty());
  EXPECT_EQ(cursor, 102u);
  EXPECT_TRUE(stream.query(100).first.empty());
  stream.set_history_limit(1);
  stream.push_back(30);
  EXPECT_EQ(stream.query(102).first, (List<int>{30}));
  EXPECT_TRUE(stream.query(103).first.empty());
}

TEST(ObserverStreamTest, LimitChangesPruneOnlyStepsStrictlyBeforeCutoff) {
  ObserverStream<int> stream(5);
  stream.push_back(11);
  stream.tick(2);
  stream.push_back(22);
  stream.push_back(33);
  EXPECT_EQ(stream.query().first, (List<int>{11, 22, 33}));
  stream.set_history_limit(2);
  EXPECT_EQ(stream.query().first, (List<int>{22, 33}));
  stream.tick(0);
  EXPECT_EQ(stream.query().second, 5u);
  stream.tick();
  EXPECT_EQ(stream.query().first, (List<int>{33}));
  stream.set_history_limit(0);
  stream.tick(50);
  EXPECT_TRUE(stream.query().first.empty());
  stream.set_history_limit(1);
  EXPECT_TRUE(stream.query().first.empty());
}

TEST(ObserverStreamTest, IndependentReadersKeepOwningSnapshotsAndRecoverStaleCursors) {
  ObserverStream<int> stream(3);
  int input = 17;
  stream.push_back(input);
  auto first = stream.query();
  input = 18;
  stream.push_back(input);
  auto second = stream.query();
  EXPECT_EQ(stream.query(first.second).first, (List<int>{18}));
  EXPECT_TRUE(stream.query(second.second).first.empty());
  stream.reset();
  stream.push_back(99);
  auto stale = stream.query(second.second);
  EXPECT_TRUE(stale.first.empty());
  EXPECT_EQ(stale.second, 1u);
  stream.push_back(100);
  EXPECT_EQ(stream.query(stale.second).first, (List<int>{100}));
  EXPECT_EQ(first.first, (List<int>{17}));
  EXPECT_EQ(second.first, (List<int>{17, 18}));
}

TEST(ObserverStreamTest, ProjectionBorrowsSuffixAndStopsAtFirstError) {
  ObserverStream<std::unique_ptr<int>> stream(4);
  for (int i : {10, 20, 30, 40})
    stream.push_back(std::make_unique<int>(i));
  List<int> visited;
  auto rejected = stream.transform_since<int>(1, [&](std::unique_ptr<int> const& entry) -> std::expected<int, StarException> {
    visited.push_back(*entry);
    if (*entry == 30)
      return std::unexpected(OutOfRangeException("projection rejected entry", false));
    return *entry + 1;
  });
  ASSERT_FALSE(rejected.has_value());
  EXPECT_TRUE(rejected.error().is<OutOfRangeException>());
  EXPECT_EQ(visited, (List<int>{20, 30}));
  auto project = [](std::unique_ptr<int> const& entry) -> std::expected<int, StarException> { return *entry + 1; };
  auto retry = stream.transform_since<int>(1, project);
  ASSERT_TRUE(retry.has_value());
  EXPECT_EQ(retry->first, (List<int>{21, 31, 41}));
  EXPECT_EQ(retry->second, 4u);
  auto empty = stream.transform_since<int>(retry->second, project);
  ASSERT_TRUE(empty.has_value());
  EXPECT_TRUE(empty->first.empty());
  EXPECT_EQ(empty->second, retry->second);
}

TEST(ObserverStreamTest, QuerySnapshotsRemainIndependentAfterEviction) {
  ObserverStream<int> stream(2);
  stream.push_back(10);
  stream.push_back(20);
  auto snapshot = stream.query();
  stream.push_back(30);
  auto current = stream.query();
  ASSERT_EQ(current.first.size(), 2u);
  current.first[0] = -1;
  EXPECT_EQ(stream.query().first, (List<int>{20, 30}));
  EXPECT_EQ(snapshot.first, (List<int>{10, 20}));
  EXPECT_EQ(snapshot.second, 2u);
}

TEST(ObserverStreamTest, LastRepresentableCursorIsValidButCannotAdvance) {
  constexpr auto last = std::numeric_limits<std::uint64_t>::max();
  ObserverStream<int> stream(1);
  stream.tick(last - 1);
  stream.push_back(77);
  stream.tick(0);
  auto snapshot = stream.query(last - 1);
  EXPECT_EQ(snapshot.first, (List<int>{77}));
  EXPECT_EQ(snapshot.second, last);
  EXPECT_TRUE(stream.query(last).first.empty());
  EXPECT_DEATH(stream.tick(), ".*");
  EXPECT_DEATH(stream.push_back(88), ".*");
}

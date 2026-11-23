import std;
import star.chat_bubble_separation;
import star.format;
import star.rect;
import star.vector;

#include "gtest/gtest.h"

using namespace Star;

namespace {
void insert(BubbleSeparator<int>& bubbles, int id, RectF bounds) {
  bubbles.add_bubble(bounds.center(), bounds, id);
}

std::optional<RectF> box_for(BubbleSeparator<int> const& bubbles, int id) {
  std::optional<RectF> result;
  EXPECT_TRUE(bubbles.for_each([&](BubbleState<int> const& bubble, int value) -> std::expected<void, StarException> {
    if (value == id)
      result = bubble.separated_box;
    return {};
  }));
  return result;
}
}

TEST(ChatBubbleSeparation, UnsortedConsecutiveInsertsFindHiddenContainingBox) {
  BubbleSeparator<int> bubbles;
  insert(bubbles, 1, {100, 20, 110, 30});
  insert(bubbles, 2, {-80, 20, -70, 30});
  insert(bubbles, 3, {-1000, 0, 1000, 10});
  insert(bubbles, 4, {-20, 20, -10, 30});
  insert(bubbles, 5, {20, 20, 30, 30});
  // Both neighboring endpoints belong to non-overlapping narrow boxes.
  // There is deliberately no bulk update before either insertion.
  insert(bubbles, 6, {0, 2, 10, 8});
  EXPECT_EQ(box_for(bubbles, 6), (RectF{0, 10, 10, 16}));
  insert(bubbles, 7, {0, 2, 10, 8});
  EXPECT_EQ(box_for(bubbles, 7), (RectF{0, 16, 10, 22}));
}

TEST(ChatBubbleSeparation, BulkQueriesReadOnlyOldGeometryAndPublishNewEdges) {
  BubbleSeparator<int> bubbles;
  insert(bubbles, 1, {0, -4, 20, 6}); // First in Y order; moves before the others solve.
  insert(bubbles, 2, {40, 0, 50, 6});
  insert(bubbles, 3, {100, 0, 110, 6});
  ASSERT_TRUE(bubbles.update_destinations([](BubbleState<int>& bubble, int id) -> std::expected<void, StarException> {
    switch (id) {
      case 1: bubble.ideal_destination = {90, 5}; break;
      case 2: bubble.ideal_destination = {9, 3}; break;
      case 3: bubble.ideal_destination = {89, 3}; break;
    }
    return {};
  }));
  EXPECT_EQ(box_for(bubbles, 1), (RectF{80, 0, 100, 10}));
  EXPECT_EQ(box_for(bubbles, 2), (RectF{4, 6, 14, 12}));
  EXPECT_EQ(box_for(bubbles, 3), (RectF{84, 0, 94, 6}));
  insert(bubbles, 4, {83, 1, 85, 3});
  EXPECT_EQ(box_for(bubbles, 4), (RectF{83, 10, 85, 12}));
  insert(bubbles, 5, {0, 0, 2, 2});
  EXPECT_EQ(box_for(bubbles, 5), (RectF{0, 0, 2, 2}));
}

TEST(ChatBubbleSeparation, CoincidentPreviousBoxesExcludeOnlyOneSelf) {
  BubbleSeparator<int> bubbles;
  insert(bubbles, 1, {0, 0, 10, 10});
  insert(bubbles, 2, {20, 0, 30, 10});
  auto converge = [](BubbleState<int>& bubble, int) -> std::expected<void, StarException> {
    bubble.ideal_destination = {105, 5};
    return {};
  };
  ASSERT_TRUE(bubbles.update_destinations(converge));
  EXPECT_EQ(box_for(bubbles, 1), (RectF{100, 0, 110, 10}));
  EXPECT_EQ(box_for(bubbles, 2), box_for(bubbles, 1));
  ASSERT_TRUE(bubbles.update_destinations(converge));
  EXPECT_EQ(box_for(bubbles, 1), (RectF{100, 10, 110, 20}));
  EXPECT_EQ(box_for(bubbles, 2), box_for(bubbles, 1));
  bubbles.filter([](BubbleState<int> const&, int id) { return id == 2; });
  insert(bubbles, 3, {101, 11, 103, 13});
  EXPECT_EQ(box_for(bubbles, 3), (RectF{101, 20, 103, 22}));
}

TEST(ChatBubbleSeparation, TickBorrowsMoveOnlyVisitorAndPayload) {
  using Value = std::unique_ptr<int>;
  BubbleSeparator<Value> bubbles;
  bubbles.add_bubble({5, 5}, {0, 0, 10, 10}, std::make_unique<int>(3));
  auto visit = [increment = std::make_unique<int>(2)](BubbleState<Value>&, Value& value) -> std::expected<void, StarException> {
    *value += *increment;
    return {};
  };
  ASSERT_TRUE(bubbles.update_destinations(visit));
  ASSERT_TRUE(bubbles.update_destinations(visit));
  ASSERT_TRUE(bubbles.for_each([](BubbleState<Value> const&, Value const& value) -> std::expected<void, StarException> {
    EXPECT_EQ(*value, 7);
    return {};
  }));
}

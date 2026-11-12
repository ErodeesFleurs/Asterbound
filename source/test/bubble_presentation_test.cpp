import std;
import star.format;
import star.chat_bubble_separation;
import star.rect;
import star.vector;

#include "gtest/gtest.h"

using namespace Star;

namespace {
std::expected<void, StarException> solve(BubbleSeparator<int>& separator) {
  return separator.updateDestinations([](BubbleState<int>&, int&) -> std::expected<void, StarException> { return {}; });
}

std::optional<BubbleState<int>> bubble(BubbleSeparator<int> const& separator, int id) {
  std::optional<BubbleState<int>> result;
  auto visited = separator.forEach([&](BubbleState<int> const& state, int value) -> std::expected<void, StarException> {
    if (value == id)
      result = state;
    return {};
  });
  EXPECT_TRUE(visited);
  return result;
}
}

TEST(BubblePresentationTest, DisplayVisitsSampleOffsetsWithoutSolvingOrTweening) {
  BubbleSeparator<int> separator(0.5f);
  separator.addBubble({5, 5}, RectF{0, 0, 10, 10}, 1);
  ASSERT_TRUE(solve(separator));
  separator.update(1.0f / 60.0f);
  separator.addBubble({5, 5}, RectF{0, 0, 10, 10}, 2);
  ASSERT_TRUE(solve(separator));
  separator.update(1.0f / 60.0f);
  separator.filter([](BubbleState<int> const&, int id) { return id != 1; });
  ASSERT_TRUE(solve(separator));
  separator.update(1.0f / 60.0f);
  auto before = bubble(separator, 2);
  ASSERT_TRUE(before);
  EXPECT_NE(before->previousOffset, before->currentOffset);
  for (float alpha : {0.0f, 0.25f, 0.75f, 1.0f, 0.25f}) {
    auto rendered = separator.forEach([&](BubbleState<int> const& state, int) -> std::expected<void, StarException> {
      EXPECT_EQ(state.sampleOffset(alpha), before->previousOffset + alpha * (before->currentOffset - before->previousOffset));
      return {};
    });
    ASSERT_TRUE(rendered);
    auto after = bubble(separator, 2);
    ASSERT_TRUE(after);
    EXPECT_EQ(after->previousOffset, before->previousOffset);
    EXPECT_EQ(after->currentOffset, before->currentOffset);
    EXPECT_EQ(after->seperatedOffset, before->seperatedOffset);
    EXPECT_EQ(after->separatedBox, before->separatedBox);
    EXPECT_EQ(after->currentDestination, before->currentDestination);
  }
  EXPECT_EQ(before->sampleOffset(-1), before->previousOffset);
  EXPECT_EQ(before->sampleOffset(2), before->currentOffset);
  separator.update(1.0f / 60.0f);
  auto next = bubble(separator, 2);
  ASSERT_TRUE(next);
  EXPECT_EQ(next->previousOffset, before->currentOffset);
}

TEST(BubblePresentationTest, BirthResetRemovalAndReusedContentsDoNotBlendHistories) {
  BubbleSeparator<int> separator(0.5f);
  separator.addBubble({5, 5}, RectF{0, 0, 10, 10}, 1);
  ASSERT_TRUE(solve(separator));
  separator.update(1.0f / 60.0f);
  separator.addBubble({5, 5}, RectF{0, 0, 10, 10}, 2);
  auto born = bubble(separator, 2);
  ASSERT_TRUE(born);
  EXPECT_EQ(born->sampleOffset(0), born->currentOffset);
  ASSERT_TRUE(solve(separator));
  separator.update(1.0f / 60.0f);
  born = bubble(separator, 2);
  ASSERT_TRUE(born);
  EXPECT_EQ(born->sampleOffset(0), born->currentOffset);
  separator.filter([](BubbleState<int> const&, int id) { return id == 2; });
  ASSERT_TRUE(solve(separator));
  separator.update(1.0f / 60.0f);
  auto moving = bubble(separator, 2);
  ASSERT_TRUE(moving);
  EXPECT_NE(moving->sampleOffset(0), moving->sampleOffset(1));
  separator.resetPresentation();
  auto reset = bubble(separator, 2);
  ASSERT_TRUE(reset);
  for (float alpha : {0.0f, 0.25f, 0.75f, 1.0f})
    EXPECT_EQ(reset->sampleOffset(alpha), reset->currentOffset);
  ASSERT_TRUE(solve(separator));
  separator.update(1.0f / 60.0f);
  reset = bubble(separator, 2);
  ASSERT_TRUE(reset);
  EXPECT_EQ(reset->sampleOffset(0), reset->sampleOffset(1));
  separator.filter([](BubbleState<int> const&, int) { return false; });
  EXPECT_TRUE(separator.empty());
  EXPECT_FALSE(bubble(separator, 2));
  separator.addBubble({100, 100}, RectF{95, 95, 105, 105}, 2);
  auto reused = bubble(separator, 2);
  ASSERT_TRUE(reused);
  EXPECT_EQ(reused->sampleOffset(0), Vec2F());
  EXPECT_EQ(reused->sampleOffset(1), Vec2F());
  separator.clear();
  EXPECT_TRUE(separator.empty());
  separator.addBubble({5, 5}, RectF{0, 0, 10, 10}, 2);
  reused = bubble(separator, 2);
  ASSERT_TRUE(reused);
  EXPECT_EQ(reused->currentOffset, Vec2F());
}

import std;
import star.format;
import star.chat_bubble_separation;
import star.rect;
import star.vector;

#include "gtest/gtest.h"

using namespace Star;

namespace {
std::expected<void, StarException> solve(BubbleSeparator<int>& separator) {
  return separator.update_destinations([](BubbleState<int>&, int&) -> std::expected<void, StarException> { return {}; });
}

std::optional<BubbleState<int>> bubble(BubbleSeparator<int> const& separator, int id) {
  std::optional<BubbleState<int>> result;
  auto visited = separator.for_each([&](BubbleState<int> const& state, int value) -> std::expected<void, StarException> {
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
  separator.add_bubble({5, 5}, RectF{0, 0, 10, 10}, 1);
  ASSERT_TRUE(solve(separator));
  separator.update(1.0f / 60.0f);
  separator.add_bubble({5, 5}, RectF{0, 0, 10, 10}, 2);
  ASSERT_TRUE(solve(separator));
  separator.update(1.0f / 60.0f);
  separator.filter([](BubbleState<int> const&, int id) { return id != 1; });
  ASSERT_TRUE(solve(separator));
  separator.update(1.0f / 60.0f);
  auto before = bubble(separator, 2);
  ASSERT_TRUE(before);
  EXPECT_NE(before->previous_offset, before->current_offset);
  for (float alpha : {0.0f, 0.25f, 0.75f, 1.0f, 0.25f}) {
    auto rendered = separator.for_each([&](BubbleState<int> const& state, int) -> std::expected<void, StarException> {
      EXPECT_EQ(state.sample_offset(alpha), before->previous_offset + alpha * (before->current_offset - before->previous_offset));
      return {};
    });
    ASSERT_TRUE(rendered);
    auto after = bubble(separator, 2);
    ASSERT_TRUE(after);
    EXPECT_EQ(after->previous_offset, before->previous_offset);
    EXPECT_EQ(after->current_offset, before->current_offset);
    EXPECT_EQ(after->separated_offset, before->separated_offset);
    EXPECT_EQ(after->separated_box, before->separated_box);
    EXPECT_EQ(after->current_destination, before->current_destination);
  }
  EXPECT_EQ(before->sample_offset(-1), before->previous_offset);
  EXPECT_EQ(before->sample_offset(2), before->current_offset);
  separator.update(1.0f / 60.0f);
  auto next = bubble(separator, 2);
  ASSERT_TRUE(next);
  EXPECT_EQ(next->previous_offset, before->current_offset);
}

TEST(BubblePresentationTest, BirthResetRemovalAndReusedContentsDoNotBlendHistories) {
  BubbleSeparator<int> separator(0.5f);
  separator.add_bubble({5, 5}, RectF{0, 0, 10, 10}, 1);
  ASSERT_TRUE(solve(separator));
  separator.update(1.0f / 60.0f);
  separator.add_bubble({5, 5}, RectF{0, 0, 10, 10}, 2);
  auto born = bubble(separator, 2);
  ASSERT_TRUE(born);
  EXPECT_EQ(born->sample_offset(0), born->current_offset);
  ASSERT_TRUE(solve(separator));
  separator.update(1.0f / 60.0f);
  born = bubble(separator, 2);
  ASSERT_TRUE(born);
  EXPECT_EQ(born->sample_offset(0), born->current_offset);
  separator.filter([](BubbleState<int> const&, int id) { return id == 2; });
  ASSERT_TRUE(solve(separator));
  separator.update(1.0f / 60.0f);
  auto moving = bubble(separator, 2);
  ASSERT_TRUE(moving);
  EXPECT_NE(moving->sample_offset(0), moving->sample_offset(1));
  separator.reset_presentation();
  auto reset = bubble(separator, 2);
  ASSERT_TRUE(reset);
  for (float alpha : {0.0f, 0.25f, 0.75f, 1.0f})
    EXPECT_EQ(reset->sample_offset(alpha), reset->current_offset);
  ASSERT_TRUE(solve(separator));
  separator.update(1.0f / 60.0f);
  reset = bubble(separator, 2);
  ASSERT_TRUE(reset);
  EXPECT_EQ(reset->sample_offset(0), reset->sample_offset(1));
  separator.filter([](BubbleState<int> const&, int) { return false; });
  EXPECT_TRUE(separator.empty());
  EXPECT_FALSE(bubble(separator, 2));
  separator.add_bubble({100, 100}, RectF{95, 95, 105, 105}, 2);
  auto reused = bubble(separator, 2);
  ASSERT_TRUE(reused);
  EXPECT_EQ(reused->sample_offset(0), Vec2F());
  EXPECT_EQ(reused->sample_offset(1), Vec2F());
  separator.clear();
  EXPECT_TRUE(separator.empty());
  separator.add_bubble({5, 5}, RectF{0, 0, 10, 10}, 2);
  reused = bubble(separator, 2);
  ASSERT_TRUE(reused);
  EXPECT_EQ(reused->current_offset, Vec2F());
}

import std;
import star.ai_types;
import star.format;
import star.json;
import star.string;
#include "gtest/gtest.h"

using namespace Star;

TEST(AiStateTest, RoundTripKeepsFirstOccurrenceOrderInEachList) {
  auto state = AiState::create(JsonObject{
      {"availableMissions", JsonArray{"z", "a", "z", "m"}},
      {"completedMissions", JsonArray{"m", "z", "m", "a"}}});
  ASSERT_TRUE(state) << state.error().what();
  Json expected = JsonObject{{"availableMissions", JsonArray{"z", "a", "m"}},
      {"completedMissions", JsonArray{"m", "z", "a"}}};
  EXPECT_EQ(state->to_json(), expected);
}

TEST(AiStateTest, MissingListsDefaultToEmptyIndependently) {
  Json empty = JsonObject{{"availableMissions", JsonArray{}}, {"completedMissions", JsonArray{}}};
  auto state = AiState::create(JsonObject{});
  ASSERT_TRUE(state);
  EXPECT_EQ(state->to_json(), empty);
  for (auto const& key : {String("availableMissions"), String("completedMissions")}) {
    auto saved = AiState::create(JsonObject{{key, JsonArray{"x"}}});
    ASSERT_TRUE(saved);
    auto expected = empty.set(key, JsonArray{"x"});
    ASSERT_TRUE(expected);
    EXPECT_EQ(saved->to_json(), *expected);
    EXPECT_FALSE(AiState::create(JsonObject{{key, Json{}}}));
    EXPECT_FALSE(AiState::create(JsonObject{{key, JsonObject{}}}));
    EXPECT_FALSE(AiState::create(JsonObject{{key, JsonArray{"x", 1}}}));
  }
  EXPECT_FALSE(AiState::create(JsonArray{}));
}

TEST(AiStateTest, ReportsFirstInvalidEntryBeforeReadingCompletedMissions) {
  auto state = AiState::create(JsonObject{
      {"availableMissions", JsonArray{"ok", false, 2}}, {"completedMissions", JsonObject{}}});
  auto first_error = Json(false).to_string();
  ASSERT_FALSE(state);
  ASSERT_FALSE(first_error);
  EXPECT_TRUE(state.error().is<JsonException>());
  EXPECT_EQ(state.error().what(), first_error.error().what());
}

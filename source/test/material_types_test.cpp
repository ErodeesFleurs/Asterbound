import std;
import star.game_test;
import star.image_metadata_database;
import star.json;
import star.map;
import star.material_render_profile;
import star.material_types;
import star.root;
import star.string;
import star.unicode;

#include "gtest/gtest.h"

using namespace Star;

TEST(MaterialHueTest, FiniteDegreesTruncateAndWrap) {
  struct HueCase {
    float degrees;
    unsigned hue;
  };
  HueCase const cases[] = {
    {0.0f, 0}, {45.0f, 31}, {-45.0f, 225},
    {360.0f, 0}, {-360.0f, 0}, {405.0f, 31}, {-405.0f, 225},
    {359.0f, 254}, {-359.0f, 2}, {0.5f, 0}, {-0.5f, 0}
  };
  for (auto const& test : cases)
    EXPECT_EQ(static_cast<unsigned>(materialHueFromDegrees(test.degrees)), test.hue) << test.degrees;
}

namespace {

const GameTestRegistration
  materialRuleJoinNames("MaterialRenderProfile", "RuleJoinNamesPreserveDefaultsCaseAndErrors", [](GameTest& fixture) {
    auto metadata = fixture.root().imageMetadataDatabase();
    ASSERT_TRUE(metadata) << metadata.error().what();
    auto makeSpec = [](Json rule) -> Json {
      return JsonObject{{"representativePiece", ""},
                        {"rules", JsonObject{{"test", std::move(rule)}}},
                        {"pieces", JsonObject{}},
                        {"matches", JsonArray{JsonArray{"main", JsonArray{}}}}};
    };
    for (auto const& [rule, expected] : {std::pair{Json(JsonObject{}), MaterialJoinType::All},
                                         std::pair{Json(JsonObject{{"join", "aLl"}}), MaterialJoinType::All},
                                         std::pair{Json(JsonObject{{"join", "aNy"}}), MaterialJoinType::Any}}) {
      auto profile = parseMaterialRenderProfile(**metadata, makeSpec(rule));
      ASSERT_TRUE(profile) << profile.error().what();
      auto parsed = profile->rules.get("test");
      ASSERT_TRUE(parsed) << parsed.error().what();
      EXPECT_EQ(parsed->get()->join, expected);
    }
    for (String const& name : {String("unknown"), String("日本語"), String(std::string_view("All\0", 4))}) {
      auto profile = parseMaterialRenderProfile(**metadata, makeSpec(JsonObject{{"join", name}}));
      ASSERT_FALSE(profile);
      EXPECT_TRUE(profile.error().is<MapException>());
    }
    auto invalid =
      parseMaterialRenderProfile(**metadata, makeSpec(JsonObject{{"join", String(std::string_view("All\xff", 4))}}));
    ASSERT_FALSE(invalid);
    EXPECT_TRUE(invalid.error().is<UnicodeException>());
  });

}// namespace

import std;
import star.format;
import star.list;
import star.map;
import star.string;
import star.json;
import star.renderer;

#include "gtest/gtest.h"

using namespace Star;

TEST(RenderPreferences, RejectsMalformedFieldsIndependentlyAndRemembersThem) {
  RenderPreferences prefs;
  JsonObject config{{"renderOutput", "hdr10"}, {"renderExposure", 2}, {"hdrPaperWhite", 300}, {"hdrPeakLuminance", 800},
                    {"renderFpsLimit", 119.875}, {"vsync", false}, {"hdr", false}};
  auto initial = prefs.observe(config);
  ASSERT_TRUE(initial && initial->empty());
  RenderOutputSettings expected{RenderOutputMode::Hdr10, 2, 300, 800};
  EXPECT_EQ(prefs.output, expected);
  EXPECT_EQ(prefs.fpsLimit, 119.875);

  config.set("hdrPeakLuminance", 299);
  config.set("renderFpsLimit", std::bit_cast<double>(std::uint64_t{0x7ff8000000000001}));
  config.set("hdr", "no");
  config.set("vsync", true);
  Json snapshot(config);
  auto rejected = prefs.observe(snapshot);
  ASSERT_TRUE(rejected);
  EXPECT_EQ(rejected->size(), 3u);
  EXPECT_EQ(prefs.output, expected);
  EXPECT_EQ(prefs.fpsLimit, 119.875);
  EXPECT_FALSE(prefs.hdr);
  EXPECT_TRUE(prefs.vsync);
  auto unchanged = prefs.observe(snapshot);
  ASSERT_TRUE(unchanged && unchanged->empty());
  config.set("unrelated", true);
  auto unrelated = prefs.observe(config);
  ASSERT_TRUE(unrelated && unrelated->empty());

  config.set("hdrPeakLuminance", 600);
  config.set("renderFpsLimit", 60);
  config.set("hdr", true);
  config.set("vsync", 1);
  auto corrected = prefs.observe(config);
  ASSERT_TRUE(corrected);
  EXPECT_EQ(corrected->size(), 1u);
  expected.peakNits = 600;
  EXPECT_EQ(prefs.output, expected);
  EXPECT_EQ(prefs.fpsLimit, 60);
  EXPECT_TRUE(prefs.hdr);
  EXPECT_TRUE(prefs.vsync);
}

TEST(RenderPreferences, InvalidColdConfigurationUsesDefaultsWithoutMaskingInvalidRoot) {
  RenderPreferences prefs;
  Json config(JsonObject{{"renderOutput", "unsupported"}, {"renderFpsLimit", -1}, {"vsync", JsonArray{}}, {"hdr", 1}});
  auto result = prefs.observe(config);
  ASSERT_TRUE(result);
  EXPECT_EQ(result->size(), 4u);
  EXPECT_EQ(prefs.output, RenderOutputSettings{});
  EXPECT_EQ(prefs.fpsLimit, 0);
  EXPECT_TRUE(prefs.vsync);
  EXPECT_TRUE(prefs.hdr);
  EXPECT_FALSE(prefs.observe(JsonArray{}));
}

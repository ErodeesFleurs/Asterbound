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

TEST(RenderPreferences, DisplayAndCameraFieldsKeepIndependentLastGoodValues) {
  RenderPreferences prefs;
  JsonObject fields{{"renderOutput", "legacy"}, {"renderExposure", 0}, {"hdrPaperWhite", 203}, {"hdrPeakLuminance", 1000},
                    {"vsync", true}, {"renderFpsLimit", 0}, {"antiAliasing", true}, {"limitTextureAtlasSize", true},
                    {"useMultiTexturing", false}, {"interfaceScale", 1.5}, {"zoomLevel", 2.75}, {"cameraSpeedFactor", 0.5}};
  auto initial = prefs.observe(fields);
  ASSERT_TRUE(initial && initial->empty());
  fields.set("limitTextureAtlasSize", JsonObject{});
  fields.set("useMultiTexturing", "true");
  fields.set("antiAliasing", 0);
  fields.set("interfaceScale", -0.5);
  fields.set("zoomLevel", std::numeric_limits<double>::max());
  fields.set("cameraSpeedFactor", 0);
  fields.set("hardwareCursor", false);
  auto rejected = prefs.observe(fields);
  ASSERT_TRUE(rejected);
  EXPECT_EQ(rejected->size(), 6u);
  EXPECT_TRUE(prefs.antiAliasing);
  EXPECT_TRUE(prefs.limitTextureAtlasSize);
  EXPECT_FALSE(prefs.useMultiTexturing);
  EXPECT_FALSE(prefs.hardwareCursor);
  EXPECT_EQ(prefs.interfaceScale, 1.5f);
  EXPECT_EQ(prefs.zoomLevel, 2.75f);
  EXPECT_EQ(prefs.cameraSpeedFactor, 0.5f);
  fields.set("interfaceScale", 0);
  fields.set("zoomLevel", 4);
  auto corrected = prefs.observe(fields);
  ASSERT_TRUE(corrected && corrected->empty());
  EXPECT_EQ(prefs.interfaceScale, 0);
  EXPECT_EQ(prefs.zoomLevel, 4);
}

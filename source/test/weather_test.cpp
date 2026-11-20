import std;
import star.json;
import star.random;
import star.weighted_pool;
import star.color;
import star.bi_map;
import star.list;
import star.image_processing;
import star.hash;
import star.data_stream;
import star.string_view;
import star.string;
import star.thread;
import star.directives;
import star.asset_path;
import star.animation;
import star.particle;
import star.weather_types;
import star.world_geometry;
import star.vector;
import star.tile_damage;
import star.plant_database;
import star.parallax;
import star.game_test;
import star.root;

#include "gtest/gtest.h"

import star.weather;

using namespace Star;

namespace {

static Json weatherConfig() {
  return JsonObject{
    {"name", "testWeather"},
    {"particles", JsonArray()},
    {"projectiles", JsonArray()},
    {"duration", JsonArray{40, 100}}};
}

}// namespace

const GameTestRegistration weatherNoParallax("WeatherTest", "NoParallax", [](GameTest& fixture) {
  auto weather = WeatherType::create(fixture.root(), weatherConfig());
  ASSERT_TRUE(weather);

  EXPECT_FALSE(weather->parallax);
  auto stored = weather->toJson();
  ASSERT_TRUE(stored);
  auto parallax = stored->get("parallax");
  ASSERT_TRUE(parallax);
  EXPECT_TRUE(parallax->is_null());
  auto roundTrip = WeatherType::create(fixture.root(), *stored);
  ASSERT_TRUE(roundTrip);
  EXPECT_FALSE(roundTrip->parallax);
});

const GameTestRegistration weatherEmptyParallax("WeatherTest", "EmptyParallax", [](GameTest& fixture) {
  auto config = weatherConfig().set("parallax", "");
  ASSERT_TRUE(config);
  auto weather = WeatherType::create(fixture.root(), *config);
  ASSERT_TRUE(weather);

  EXPECT_FALSE(weather->parallax);
});

const GameTestRegistration weatherShorthandParallax("WeatherTest", "ShorthandParallax", [](GameTest& fixture) {
  auto config = weatherConfig().set("parallax", "rain");
  ASSERT_TRUE(config);
  auto weather = WeatherType::create(fixture.root(), *config);
  ASSERT_TRUE(weather);

  ASSERT_TRUE(weather->parallax);
  EXPECT_EQ(*weather->parallax, "/parallax/weather/rain.parallax");

  auto stored = weather->toJson();
  ASSERT_TRUE(stored);
  auto roundTrip = WeatherType::create(fixture.root(), *stored);
  ASSERT_TRUE(roundTrip);
  ASSERT_TRUE(roundTrip->parallax);
  EXPECT_EQ(*roundTrip->parallax, "/parallax/weather/rain.parallax");
});

const GameTestRegistration weatherAbsoluteParallax("WeatherTest", "AbsoluteParallax", [](GameTest& fixture) {
  auto config = weatherConfig().set("parallax", "/custom/weather/storm.parallax");
  ASSERT_TRUE(config);
  auto weather = WeatherType::create(fixture.root(), *config);
  ASSERT_TRUE(weather);

  ASSERT_TRUE(weather->parallax);
  EXPECT_EQ(*weather->parallax, "/custom/weather/storm.parallax");
});

const GameTestRegistration weatherCompleteStateRestoresAfterLocalClear("WeatherTest", "CompleteStateRestoresAfterLocalClear", [](GameTest& fixture) {
  WeatherPool pool;
  ASSERT_TRUE(pool.insert(1.0, String("rain")));
  WorldGeometry geometry(Vec2U(1000, 1000));

  auto server = ServerWeather::create(fixture.root());
  ASSERT_TRUE(server);
  (*server)->setup(pool, 0, geometry, {});
  ASSERT_TRUE((*server)->setWeather("rain", true));
  auto update = (*server)->writeUpdate(0);
  ASSERT_TRUE(update);

  auto client = ClientWeather::create(fixture.root());
  ASSERT_TRUE(client);
  (*client)->setup(geometry, {});
  (*client)->setVisibleRegion({0, 100, 100, 200});
  ASSERT_TRUE((*client)->readUpdate(update->first, {}));
  ASSERT_TRUE((*client)->update(0));
  EXPECT_FALSE((*client)->statusEffects().empty());

  ASSERT_TRUE((*client)->clear());
  EXPECT_TRUE((*client)->statusEffects().empty());

  ASSERT_TRUE((*client)->readUpdate(update->first, {}));
  (*client)->setVisibleRegion({0, 100, 100, 200});
  ASSERT_TRUE((*client)->update(0));
  EXPECT_FALSE((*client)->statusEffects().empty());
});

const GameTestRegistration parallaxWindSettingsDefaultOff("ParallaxTest", "WindSettingsDefaultOff", [](GameTest&) {
  ParallaxLayer layer;
  auto restored = ParallaxLayer::create(layer.store());
  ASSERT_TRUE(restored);

  EXPECT_FALSE(restored->followsWind);
  EXPECT_FALSE(restored->windSpeedMultiplier);
});

const GameTestRegistration parallaxWindSettingsRoundTrip("ParallaxTest", "WindSettingsRoundTrip", [](GameTest&) {
  ParallaxLayer layer;
  layer.followsWind = true;
  layer.windSpeedMultiplier = 0.25f;
  auto restored = ParallaxLayer::create(layer.store());
  ASSERT_TRUE(restored);

  EXPECT_TRUE(restored->followsWind);
  ASSERT_TRUE(restored->windSpeedMultiplier);
  EXPECT_FLOAT_EQ(*restored->windSpeedMultiplier, 0.25f);
});

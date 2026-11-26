import std;
import star.algorithm;
import star.buffer;
import star.byte_array;
import star.data_stream_devices;
import star.file;
import star.format;
import star.image;
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

std::expected<std::unique_ptr<Root>, StarException> parallax_fixture_root(GameTest& fixture, String const& directory,
    String const& overlay) {
  auto preload = File::write_file(String("[]"), File::relative_to(overlay, "preload.config"));
  if (!preload) return std::unexpected(std::move(preload.error()));
  auto settings = fixture.rootSettings();
  settings.assetDirectories.clear();
  settings.assetSources = {overlay};
  settings.storageDirectory = File::relative_to(directory, "storage");
  settings.runtimeConfigFile = {};
  settings.logFile = {};
  settings.includeUGC = false;
  settings.mix_random_entropy = false;
  return Root::create(std::move(settings));
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

  // Narrow the viewport while exposing strips above and below the old one.
  // Rain must not be emitted across the old viewport's wider x range.
  (void)(*client)->pullNewParticles();
  (*client)->setVisibleRegion({40, 50, 60, 250});
  ASSERT_TRUE((*client)->update(0));
  auto particles = (*client)->pullNewParticles();
  ASSERT_FALSE(particles.empty());
  for (auto const& particle : particles) {
    EXPECT_GE(particle.position[0], 40);
    EXPECT_LE(particle.position[0], 60);
  }

  ASSERT_TRUE((*client)->clear());
  EXPECT_TRUE((*client)->statusEffects().empty());

  ASSERT_TRUE((*client)->readUpdate(update->first, {}));
  (*client)->setVisibleRegion({0, 100, 100, 200});
  ASSERT_TRUE((*client)->update(0));
  EXPECT_FALSE((*client)->statusEffects().empty());
});

const GameTestRegistration parallax_wind_settings_default_off("ParallaxTest", "WindSettingsDefaultOff", [](GameTest&) {
  ParallaxLayer layer;
  auto restored = ParallaxLayer::create(layer.store());
  ASSERT_TRUE(restored);

  EXPECT_FALSE(restored->follows_wind);
  EXPECT_FALSE(restored->wind_speed_multiplier);
});

const GameTestRegistration parallax_wind_settings_round_trip("ParallaxTest", "WindSettingsRoundTrip", [](GameTest&) {
  ParallaxLayer layer;
  layer.follows_wind = true;
  layer.wind_speed_multiplier = 0.25f;
  auto restored = ParallaxLayer::create(layer.store());
  ASSERT_TRUE(restored);

  EXPECT_TRUE(restored->follows_wind);
  ASSERT_TRUE(restored->wind_speed_multiplier);
  EXPECT_FLOAT_EQ(*restored->wind_speed_multiplier, 0.25f);
});

const GameTestRegistration parallax_fixed_seed_generation("ParallaxTest", "FixedSeedGeneration", [](GameTest& fixture) {
  auto directory = File::temporary_directory(fixture.root().random());
  ASSERT_TRUE(directory);
  auto cleanup = make_scope_exit([&] { EXPECT_TRUE(File::remove_all(*directory)); });
  auto overlay = File::relative_to(*directory, "assets");
  auto base = File::relative_to(overlay, "parallax/images/fixture/varied/base");
  ASSERT_TRUE(File::create_directories(base));
  auto buffer = std::make_shared<Buffer>();
  ASSERT_TRUE(Image(13, 7, PixelFormat::RGBA32).write_png(fixture.root().logger(), buffer));
  ASSERT_TRUE(File::write_file(buffer->take_data(), File::relative_to(base, "1.png")));
  ASSERT_TRUE(File::write_file(String(R"({
    "verticalOrigin": 3,
    "layers": [
      {"kind":"frequency-skip", "frequency":0.5},
      {"kind":"fixture/plain", "frequency":0.7, "parallax":2, "noRandomOffset":true,
        "minSpeed":0, "maxSpeed":8, "directives":"flipx"},
      {"kind":"foliage/no-tree"},
      {"kind":"fixture/varied", "frequency":0.01, "parallax":[1,3], "baseCount":3, "modCount":2,
        "offset":[2,5], "minSpeed":[0,0], "maxSpeed":[8,4], "directives":"?flipy",
        "frameNumber":3, "frameOffset":-1, "animationCycle":3, "repeatY":true, "tileLimitTop":6, "fadePercent":0.125},
      {"kind":"fixture/far", "parallax":[-2,10], "noRandomOffset":true, "nohueshift":true}
    ]
  })"), File::relative_to(overlay, "fixed.parallax")));
  auto root = parallax_fixture_root(fixture, *directory, overlay);
  ASSERT_TRUE(root) << root.error().what();
  auto generated = Parallax::create(**root, "/fixed.parallax", 42, 4.0f, 15.0f);
  ASSERT_TRUE(generated) << generated.error().what();

  auto const& layers = (*generated)->layers();
  ASSERT_EQ(layers.size(), 3u);
  auto const& far = layers[0];
  auto const& plain = layers[1];
  auto const& varied = layers[2];
  EXPECT_EQ(far.textures, (StringList{"/parallax/images/fixture/far/base/1.png"}));
  EXPECT_EQ(far.parallax_value, Vec2F(-2, 10));
  EXPECT_EQ(far.z_level, 8.0f);
  EXPECT_TRUE(far.directives.empty());
  EXPECT_EQ(plain.textures, (StringList{"/parallax/images/fixture/plain/base/1.png"}));
  EXPECT_EQ(plain.parallax_value, Vec2F(2, 2));
  EXPECT_EQ(plain.z_level, 4.0f);
  EXPECT_EQ(plain.vertical_origin, 7.0f);
  EXPECT_EQ(plain.parallax_offset, Vec2F(0, 0));
  EXPECT_FLOAT_EQ(plain.speed[0], 3.62977314f);
  EXPECT_EQ(plain.speed[1], 0.0f);
  EXPECT_EQ(plain.directives.string(), "flipx?hueshift=15");
  EXPECT_EQ(varied.textures, (StringList{"/parallax/images/fixture/varied/base/1.png", "/parallax/images/fixture/varied/mod/2.png"}));
  EXPECT_EQ(varied.parallax_value, Vec2F(1, 3));
  EXPECT_EQ(varied.z_level, plain.z_level);
  EXPECT_EQ(varied.parallax_offset, Vec2F(6, 5));
  EXPECT_FLOAT_EQ(varied.speed[0], 2.52928948f);
  EXPECT_FLOAT_EQ(varied.speed[1], 2.16429925f);
  EXPECT_EQ(varied.directives.string(), "?flipy?hueshift=15");
  EXPECT_EQ(varied.tile_limit_top, 6.0f);
  EXPECT_EQ(varied.frame_number, 3u);
  EXPECT_EQ(varied.frame_offset, -1);
  EXPECT_EQ(varied.animation_cycle, 3.0f);

  ASSERT_TRUE((*generated)->fade_to_sky_color(Color::rgba(0x11, 0x22, 0x33, 0x44)));
  EXPECT_EQ(plain.directives.string(), "flipx?hueshift=15");
  EXPECT_TRUE(varied.directives.string().utf8().starts_with("?flipy?hueshift=15?fade=112233="));

  auto stored = (*generated)->store();
  ASSERT_TRUE(stored);
  auto restored = Parallax::create(**root, *stored);
  ASSERT_TRUE(restored) << restored.error().what();
  auto round_trip = (*restored)->store();
  ASSERT_TRUE(round_trip);
  EXPECT_EQ(*round_trip, *stored);
});

const GameTestRegistration parallax_json_numeric_bounds("ParallaxTest", "JsonNumericBounds", [](GameTest&) {
  ParallaxLayer layer;
  layer.textures = {"/legacy.png"};
  layer.parallax_value = {-2.0f, 3.0f};
  layer.speed = {4.0f, -1.5f};
  layer.frame_number = 3;
  layer.frame_offset = -1;
  layer.animation_cycle = 3.0f;
  layer.alpha = 0.125f;
  auto store = layer.store();
  EXPECT_EQ(store.contains("alpha"), false);

  auto restored = ParallaxLayer::create(store);
  ASSERT_TRUE(restored);
  EXPECT_EQ(restored->store(), store);
  EXPECT_EQ(restored->alpha, 1.0f);
  EXPECT_EQ(restored->animation_frame(-0.5), 2u);
  EXPECT_EQ(restored->animation_frame(0.0), 2u);
  EXPECT_EQ(restored->animation_frame(1.0), 0u);
  EXPECT_EQ(restored->animation_frame(2.0), 1u);
  EXPECT_EQ(restored->animation_frame(3.0), 2u);
  EXPECT_EQ(restored->animation_frame(1.0e300), restored->animation_frame(std::fmod(1.0e300, 3.0)));
  restored->frame_offset = std::numeric_limits<int>::min();
  EXPECT_EQ(restored->animation_frame(0.0), 1u);
  restored->frame_offset = std::numeric_limits<int>::max();
  EXPECT_EQ(restored->animation_frame(2.0), 0u);

  auto legacy_zero = store.set("frameNumber", 0);
  ASSERT_TRUE(legacy_zero);
  auto static_layer = ParallaxLayer::create(*legacy_zero);
  ASSERT_TRUE(static_layer);
  EXPECT_EQ(static_layer->store(), *legacy_zero);
  EXPECT_EQ(static_layer->animation_frame(1.0), 0u);

  auto beyond_int = std::int64_t(std::numeric_limits<int>::max()) + 1;
  std::initializer_list<std::pair<String, Json>> invalid{
      {"frameNumber", -1}, {"frameNumber", beyond_int}, {"frameOffset", beyond_int},
      {"animationCycle", 0}, {"animationCycle", -1}, {"animationCycle", 0x1p-149}, {"animationCycle", 1e39},
      {"parallaxValue", JsonArray{0, 1}}, {"parallaxValue", JsonArray{1, 1e39}}, {"zLevel", 1e39},
      {"verticalOrigin", 1e39}, {"parallaxOffset", JsonArray{0, 1e39}},
      {"speedY", 1e39}, {"tileLimitTop", 1e39}, {"windSpeedMultiplier", 1e39}};
  for (auto const& [key, value] : invalid) {
    auto malformed = store.set(key, value);
    ASSERT_TRUE(malformed);
    auto loaded = ParallaxLayer::create(*malformed);
    ASSERT_FALSE(loaded) << key.utf8();
  }
});

const GameTestRegistration parallax_asset_numeric_bounds("ParallaxTest", "AssetNumericBoundsWithLayerContext", [](GameTest& fixture) {
  auto directory = File::temporary_directory(fixture.root().random());
  ASSERT_TRUE(directory);
  auto cleanup = make_scope_exit([&] { EXPECT_TRUE(File::remove_all(*directory)); });
  auto overlay = File::relative_to(*directory, "assets");
  ASSERT_TRUE(File::create_directory(overlay));
  auto beyond_int = std::int64_t(std::numeric_limits<int>::max()) + 1;
  Json settings = JsonObject{{"kind", "bad-layer"}, {"parallax", 2}, {"noRandomOffset", true}, {"frameNumber", 3}};
  std::initializer_list<std::pair<String, Json>> invalid{
      {"baseCount", 0}, {"baseCount", beyond_int}, {"modCount", -1}, {"modCount", beyond_int},
      {"frameNumber", -1}, {"frameNumber", 0}, {"frameNumber", beyond_int}, {"frameOffset", beyond_int},
      {"animationCycle", 0}, {"animationCycle", 0x1p-149}, {"animationCycle", 1e39},
      {"parallax", 0}, {"parallax", JsonArray{1, 1e39}}, {"frequency", 1e39}};
  std::size_t index = 0;
  for (auto const& [key, value] : invalid) {
    auto malformed = settings.set(key, value);
    ASSERT_TRUE(malformed);
    auto contents = Json(JsonObject{{"layers", JsonArray{*malformed}}}).repr();
    ASSERT_TRUE(contents);
    auto filename = String(Star::format("invalid-{}.parallax", index++));
    ASSERT_TRUE(File::write_file(*contents, File::relative_to(overlay, filename)));
  }
  auto root = parallax_fixture_root(fixture, *directory, overlay);
  ASSERT_TRUE(root) << root.error().what();
  index = 0;
  for (auto const& [key, value] : invalid) {
    auto path = String(Star::format("/invalid-{}.parallax", index++));
    auto loaded = Parallax::create(**root, path, 42, 0.0f, 0.0f);
    ASSERT_FALSE(loaded) << key.utf8();
    auto const& message = loaded.error().what();
    EXPECT_NE(message.find(path.utf8()), std::string::npos) << message;
    EXPECT_NE(message.find("bad-layer"), std::string::npos) << message;
    EXPECT_NE(message.find(key.utf8()), std::string::npos) << message;
  }
});

const GameTestRegistration parallax_layer_legacy_wire("ParallaxTest", "LegacyLayerWireIsAtomic", [](GameTest&) {
  // Big-endian legacy bytes: includes alpha, but not frame count, offset, or cycle.
  static constexpr char wire[] =
      "\x02\x04" "base" "\x03" "mod" "\x06" "?flipx"
      "\x3f\x00\x00\x00\x40\x00\x00\x00\xc0\x80\x00\x00"
      "\x01\x00\x01\x40\xf0\x00\x00\x00"
      "\x41\x40\x00\x00\xc0\x00\x00\x00\x40\x40\x00\x00\xbe\x80\x00\x00"
      "\x03" "day" "\x40\xc8\x00\x00\xbf\x80\x00\x00"
      "\x01\x01\x3e\x80\x00\x00\x01\x00\x3e\x00\x00\x00";
  auto bytes = std::span(wire).first(sizeof(wire) - 1);
  ParallaxLayer expected;
  expected.textures = {"base", "mod"};
  expected.directives = String("?flipx");
  expected.alpha = 0.5f;
  expected.parallax_value = {2, -4};
  expected.repeat = {1, 0};
  expected.tile_limit_top = 7.5f;
  expected.vertical_origin = 12;
  expected.z_level = -2;
  expected.parallax_offset = {3, -0.25f};
  expected.time_of_day_correlation = "day";
  expected.speed = {6.25f, -1};
  expected.follows_wind = true;
  expected.wind_speed_multiplier = 0.25f;
  expected.unlit = true;
  expected.fade_percent = 0.125f;

  ParallaxLayer destination;
  destination.textures = {"retained"};
  destination.alpha = 0.75f;
  destination.tile_limit_bottom = -3.0f;
  destination.frame_number = 12;
  destination.frame_offset = -20;
  destination.animation_cycle = 8.0f;
  auto retained = destination.store();
  for (std::size_t n = 0; n < bytes.size(); ++n) {
    auto decoded = DataStreamBuffer::deserialize(destination, bytes.first(n));
    ASSERT_FALSE(decoded) << n;
    EXPECT_EQ(destination.store(), retained) << n;
    EXPECT_EQ(destination.alpha, 0.75f) << n;
  }
  ASSERT_TRUE(DataStreamBuffer::deserialize(destination, bytes));
  EXPECT_EQ(destination.store(), expected.store());
  EXPECT_EQ(destination.alpha, expected.alpha);

  expected.frame_number = 7;
  expected.frame_offset = -2;
  expected.animation_cycle = 6.0f;
  auto encoded = DataStreamBuffer::serialize(expected);
  ASSERT_TRUE(encoded);
  EXPECT_EQ(*encoded, ByteArray(bytes));
});

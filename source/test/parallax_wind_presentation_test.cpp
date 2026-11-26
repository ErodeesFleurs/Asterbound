import std;
import star.algorithm;
import star.assets;
import star.byte_array;
import star.client_context;
import star.directives;
import star.entity_rendering_types;
import star.file;
import star.format;
import star.game_test;
import star.game_types;
import star.input;
import star.input_lua_bindings;
import star.json;
import star.lua;
import star.net_packets;
import star.parallax;
import star.player;
import star.player_factory;
import star.random;
import star.root;
import star.sky;
import star.sky_parameters;
import star.string;
import star.universe_client;
import star.uuid;
import star.weather;
import star.weather_types;
import star.world_client;
import star.vector;
import star.world_geometry;
import star.world_render_data;
import star.world_template;

#include "gtest/gtest.h"

using namespace Star;

TEST(ParallaxWindPresentationTest, DirectionMagnitudeAndSignedTimeSampleWithoutIntegrating) {
  ParallaxWindTime wind;
  wind.begin_update(10.0);
  wind.update(10.25, 4.0f);
  for (float alpha : {0.0f, 0.25f, 0.75f, 1.0f, 0.25f}) {
    EXPECT_EQ(wind.sample(alpha), Vec3D(0.25, 1.0, 1.0) * static_cast<double>(alpha));
    EXPECT_EQ(wind.current, Vec3D(0.25, 1.0, 1.0));
    EXPECT_EQ(wind.previous, Vec3D());
    EXPECT_EQ(wind.last_epoch, 10.25);
  }
  wind.begin_update(10.25);
  wind.update(10.5, -2.0f);
  EXPECT_EQ(wind.sample(0), Vec3D(0.25, 1.0, 1.0));
  EXPECT_EQ(wind.sample(0.5f), Vec3D(0.125, 1.25, 0.75));
  EXPECT_EQ(wind.sample(1), Vec3D(0.0, 1.5, 0.5));
  EXPECT_EQ(wind.sample(-1), wind.previous);
  EXPECT_EQ(wind.sample(2), wind.current);
}

TEST(ParallaxWindPresentationTest, ClockCorrectionResetAndReplacementDoNotMixDomains) {
  ParallaxWindTime wind;
  wind.begin_update(59.75);
  wind.update(60.25, 2.0f);
  // Day boundaries do not wrap the absolute integration clock.
  EXPECT_EQ(wind.sample(0.5f), Vec3D(0.25, 0.5, 0.5));
  wind.reset_presentation();
  EXPECT_EQ(wind.sample(0), wind.sample(1));
  // A corrected clock becomes the next tick's baseline, not elapsed wind time.
  wind.begin_update(10000.0);
  wind.update(10000.25, -4.0f);
  EXPECT_EQ(wind.previous, Vec3D(0.5, 1.0, 1.0));
  EXPECT_EQ(wind.current, Vec3D(0.25, 2.0, 0.0));
  wind.begin_update(10000.25);
  wind.update(1.0, 100.0f);
  EXPECT_EQ(wind.sample(0), wind.sample(1));
  EXPECT_EQ(wind.current, Vec3D(0.25, 2.0, 0.0));
  wind.begin_update(1.0);
  wind.update(1.25, 0.0f);
  // Retain the existing zero-wind direction convention.
  EXPECT_EQ(wind.current, Vec3D(0.0, 2.0, 0.0));
  wind = {};
  for (float alpha : {0.0f, 0.25f, 0.75f, 1.0f})
    EXPECT_EQ(wind.sample(alpha), Vec3D());
}

namespace {

std::vector<String> sample_textures(WorldRenderData const& frame) {
  std::vector<String> textures;
  for (auto const& sample : frame.parallax_layers.samples())
    textures.push_back(sample.layer.get().textures.front());
  return textures;
}

const GameTestRegistration weather_retry_and_transition_snapshots(
    "ParallaxSnapshotTest", "WeatherRetryAndTransitionSnapshots", [](GameTest& fixture) {
  auto directory = File::temporary_directory(fixture.root().random());
  ASSERT_TRUE(directory) << directory.error().what();
  auto cleanup = make_scope_exit([&] { EXPECT_TRUE(File::remove_all(*directory)); });
  auto overlay = File::relative_to(*directory, "assets");
  ASSERT_TRUE(File::create_directory(overlay));
  auto write_asset = [&](String const& name, String const& contents) {
    return File::write_file(contents, File::relative_to(overlay, name));
  };
  ASSERT_TRUE(write_asset("client.config.patch", R"({"parallaxFadeTime":1.0})"));
  ASSERT_TRUE(write_asset("weather.config.patch", R"({"weatherWarmupTime":4.0,"weatherCooldownTime":4.0})"));
  ASSERT_TRUE(write_asset("snapshot-a.weather",
      R"({"name":"snapshot-a","duration":[40,40],"maximumWind":4,"parallax":"/snapshot-a.parallax"})"));
  ASSERT_TRUE(write_asset("snapshot-b.weather",
      R"({"name":"snapshot-b","duration":[40,40],"parallax":"/snapshot-b.parallax"})"));
  ASSERT_TRUE(write_asset("snapshot-missing.weather",
      R"({"name":"snapshot-missing","duration":[40,40],"parallax":"/snapshot-missing.parallax"})"));
  ASSERT_TRUE(write_asset("snapshot-a.parallax", R"({"layers":[{"kind":"snapshot-a","parallax":1,
      "offset":[10,20],"noRandomOffset":true,"nohueshift":true,"fadePercent":0.25,
      "followsWind":true,"windSpeedMultiplier":2,"minSpeed":[4,3],"maxSpeed":[4,3]}]})"));
  ASSERT_TRUE(write_asset("snapshot-b.parallax", R"({"layers":[{"kind":"invalid-b","parallax":"invalid",
      "noRandomOffset":true}]})"));
  ASSERT_TRUE(write_asset("snapshot-current.parallax", R"({"layers":[
      {"kind":"snapshot-current-first","parallax":1,"noRandomOffset":true},
      {"kind":"snapshot-current-high","parallax":2,"noRandomOffset":true},
      {"kind":"snapshot-current-last","parallax":1,"noRandomOffset":true}]})"));
  ASSERT_TRUE(write_asset("snapshot-next.parallax", R"({"layers":[
      {"kind":"snapshot-next","parallax":1,"noRandomOffset":true}]})"));

  auto settings = fixture.rootSettings();
  settings.assetSources.push_back(overlay);
  settings.storageDirectory = File::relative_to(*directory, "storage");
  settings.runtimeConfigFile = {};
  settings.logFile = {};
  settings.includeUGC = false;
  settings.mix_random_entropy = false;
  auto isolated = Root::create(std::move(settings));
  ASSERT_TRUE(isolated) << isolated.error().what();
  auto& root = **isolated;

  // Pump the real rendering client directly; no background server or GUI is
  // necessary to exercise its WorldStart, weather packets, ticks and snapshots.
  auto input = Input::create(root);
  ASSERT_TRUE(input) << input.error().what();
  auto universe = UniverseClient::create(root, {}, {}, File::relative_to(*directory, "client"));
  ASSERT_TRUE(universe) << universe.error().what();
  auto input_callbacks = LuaBindings::make_input_callbacks(**input);
  ASSERT_TRUE(input_callbacks) << input_callbacks.error().what();
  auto installed = (*universe)->setLuaCallbacks("input", *input_callbacks);
  ASSERT_TRUE(installed) << installed.error().what();
  auto players = root.playerFactory();
  ASSERT_TRUE(players) << players.error().what();
  auto player = (*players)->create();
  ASSERT_TRUE(player) << player.error().what();
  ASSERT_TRUE((*player)->finalize_creation());
  auto uuid = (*player)->uuid();
  ASSERT_TRUE(uuid) << uuid.error().what();
  auto context = ClientContext::create(root.logger(), Uuid::nil(), *uuid, min_client_connection_id, {});
  ASSERT_TRUE(context) << context.error().what();
  (*player)->set_client_context(*context);
  auto client = WorldClient::create(root, *player, (*universe)->luaRoot(), **universe);
  ASSERT_TRUE(client) << client.error().what();
  auto& world = **client;
  auto close = make_scope_exit([&] { EXPECT_TRUE(world.close()); });

  Vec2U const world_size(128, 128);
  auto world_template = WorldTemplate::create(root, world_size);
  ASSERT_TRUE(world_template) << world_template.error().what();
  auto template_data = (*world_template)->store();
  ASSERT_TRUE(template_data) << template_data.error().what();
  auto sky = Sky::create(root, (*world_template)->skyParameters(), false);
  ASSERT_TRUE(sky) << sky.error().what();
  (*sky)->setEpochTime(34.0);
  auto sky_update = (*sky)->writeUpdate();
  ASSERT_TRUE(sky_update) << sky_update.error().what();
  auto start = std::make_shared<WorldStartPacket>();
  start->client_id = min_client_connection_id;
  start->template_data = std::move(*template_data);
  start->sky_data = std::move(sky_update->first);
  start->player_start = start->player_respawn = {64, 64};
  start->respawn_in_world = true;
  start->world_properties = JsonObject{};
  auto started = world.handleIncomingPackets({start});
  ASSERT_TRUE(started) << started.error().what();
  world.setClientWindow({60, 60, 68, 68});
  world.setFullBright(true);

  auto current = Parallax::create(root, "/snapshot-current.parallax", 17, (*world_template)->surfaceLevel(), 0.0f);
  ASSERT_TRUE(current) << current.error().what();
  auto next = Parallax::create(root, "/snapshot-next.parallax", 29, (*world_template)->surfaceLevel(), 0.0f);
  ASSERT_TRUE(next) << next.error().what();
  world.set_parallax(*current);
  WeatherPool pool;
  for (auto const& name : std::array<String, 3>{"snapshot-a", "snapshot-b", "snapshot-missing"})
    ASSERT_TRUE(pool.insert(1.0, name));
  auto weather = ServerWeather::create(root);
  ASSERT_TRUE(weather) << weather.error().what();
  (*weather)->setup(std::move(pool), 0, WorldGeometry(world_size), {});
  auto change_weather = [&](String const& name, bool warmup) -> std::expected<void, StarException> {
    if (auto changed = (*weather)->setWeather(name, !warmup); !changed)
      return changed;
    if (warmup) {
      if (auto warmed = (*weather)->update(1.0); !warmed)
        return warmed;
    }
    auto packet = (*weather)->writeUpdate(0);
    if (!packet)
      return std::unexpected(std::move(packet.error()));
    if (auto received = world.handleIncomingPackets({std::make_shared<EnvironmentUpdatePacket>(ByteArray{}, std::move(packet->first))}); !received)
      return received;
    return world.update(0.125f);
  };
  auto active_a = change_weather("snapshot-a", true);
  ASSERT_TRUE(active_a) << active_a.error().what();
  ASSERT_FLOAT_EQ((*weather)->weatherIntensity(), 0.25f);

  auto texture = [](String const& kind) { return "/parallax/images/" + kind + "/base/1.png"; };
  std::vector<String> expected_a = {
    texture("snapshot-current-high"), texture("snapshot-current-first"), texture("snapshot-current-last"), texture("snapshot-a")};
  WorldRenderData frame;
  auto render = [&](WorldRenderData& output, float alpha = 1.0f) {
    return world.render(output, 0, std::nullopt, PresentationContext(alpha));
  };
  for (float alpha : {0.0f, 0.5f, 1.0f, 0.25f, 0.5f}) {
    auto rendered = render(frame, alpha);
    ASSERT_TRUE(rendered) << rendered.error().what();
    ASSERT_EQ(sample_textures(frame), expected_a);
    auto const& sample = frame.parallax_layers.samples().back();
    EXPECT_FLOAT_EQ(sample.alpha, 0.25f);
    EXPECT_EQ(sample.speed, Vec2F(0, 3));
    EXPECT_EQ(sample.layer.get().speed, Vec2F(4, 3));
    EXPECT_EQ(sample.layer.get().parallax_offset, Vec2F(10, 20));
    EXPECT_NEAR(sample.parallax_offset[0],
        10.0 - 4.0 * (0.125 * (*weather)->wind() * alpha * 2.0) / world.dayLength(), 1e-6);
    EXPECT_FLOAT_EQ(sample.parallax_offset[1], 20);
    EXPECT_FALSE(sample.layer.get().directives.empty());
  }

  world.set_parallax(*next);
  auto faded = world.update(0.25f);
  ASSERT_TRUE(faded) << faded.error().what();
  std::vector<String> const expected_transition = {
    expected_a[0], expected_a[1], expected_a[2], texture("snapshot-next"), expected_a[3]};
  WorldRenderData archived_frame;
  auto transition = render(archived_frame);
  ASSERT_TRUE(transition) << transition.error().what();
  ASSERT_EQ(sample_textures(archived_frame), expected_transition);
  auto archived_samples = archived_frame.parallax_layers.samples();
  EXPECT_FLOAT_EQ(archived_samples[2].alpha, 1.0f);
  EXPECT_FLOAT_EQ(archived_samples[3].alpha, 0.5f);
  EXPECT_FLOAT_EQ(archived_samples[4].alpha, 0.25f);

  auto active_b = change_weather("snapshot-b", false);
  ASSERT_TRUE(active_b) << active_b.error().what();
  for (int attempt = 0; attempt < 2; ++attempt) {
    auto broken = render(frame);
    ASSERT_FALSE(broken) << "Malformed weather B was incorrectly cached as a successful empty overlay";
    EXPECT_TRUE(broken.error().is<JsonException>()) << broken.error().what();
    EXPECT_TRUE(frame.parallax_layers.samples().empty());
  }
  ASSERT_TRUE(write_asset("snapshot-b.parallax", R"({"verticalOrigin":8,"layers":[
      {"kind":"snapshot-b","parallax":1,"noRandomOffset":true}]})"));
  ASSERT_TRUE(write_asset("snapshot-a.parallax", R"({"layers":[
      {"kind":"unexpected-a-reload","parallax":1,"noRandomOffset":true}]})"));
  auto assets = root.assets();
  ASSERT_TRUE(assets) << assets.error().what();
  (*assets)->hot_reload();

  // A failed B neither consumes B's key nor discards the previously built A.
  auto back_to_a = change_weather("snapshot-a", true);
  ASSERT_TRUE(back_to_a) << back_to_a.error().what();
  ASSERT_TRUE(render(frame));
  EXPECT_EQ(sample_textures(frame), expected_transition);
  auto back_to_b = change_weather("snapshot-b", false);
  ASSERT_TRUE(back_to_b) << back_to_b.error().what();
  auto retried = render(frame);
  ASSERT_TRUE(retried) << retried.error().what();
  std::vector<String> const expected_b = {
    expected_a[0], texture("snapshot-b"), expected_a[1], expected_a[2], texture("snapshot-next")};
  ASSERT_EQ(sample_textures(frame), expected_b);
  auto const retried_samples = frame.parallax_layers.samples();
  EXPECT_FLOAT_EQ(retried_samples[1].alpha, 1.0f);
  EXPECT_FLOAT_EQ(retried_samples[2].alpha, 0.75f);
  EXPECT_FLOAT_EQ(retried_samples[4].alpha, 1.0f);

  auto missing = change_weather("snapshot-missing", false);
  ASSERT_TRUE(missing) << missing.error().what();
  auto absent = render(frame);
  ASSERT_TRUE(absent) << absent.error().what();
  std::vector<String> const expected_missing = {expected_a[0], expected_a[1], expected_a[2], texture("snapshot-next")};
  EXPECT_EQ(sample_textures(frame), expected_missing);

  // A previously published frame keeps its sources even after the world drops
  // them, including the weather overlay no longer present in the active cache.
  current->reset();
  next->reset();
  frame.parallax_layers.clear();
  ASSERT_TRUE(world.handleIncomingPackets({std::make_shared<WorldStopPacket>("fixture exit")}));
  ASSERT_TRUE(render(frame));
  EXPECT_TRUE(frame.parallax_layers.samples().empty());
  EXPECT_EQ(sample_textures(archived_frame), expected_transition);
});

}

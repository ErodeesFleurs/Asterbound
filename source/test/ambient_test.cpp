import std;
import star.string;
import star.list;
import star.json;
import star.assets;
import star.mixer;
import star.root;
import star.ambient;
import star.game_test;

#include "gtest/gtest.h"

using namespace Star;

TEST(AmbientConfigTest, TrackConfiguration) {
  Json config = JsonObject{{"day", JsonObject{{"tracks", JsonArray{"bird.ogg", "/music/rain.ogg", "bird.ogg"}}}},
                           {"night", JsonObject{}}, {"loops", 0}};
  auto parsed = AmbientNoisesDescription::create(config, "/biomes/test.biome");
  ASSERT_TRUE(parsed);
  EXPECT_EQ(parsed->day_sounds.tracks, (StringList{"/biomes/bird.ogg", "/music/rain.ogg", "/biomes/bird.ogg"}));
  EXPECT_TRUE(parsed->night_sounds.tracks.empty());
  EXPECT_EQ(parsed->track_loops, 0);
  auto stored = parsed->to_json();
  auto restored = AmbientNoisesDescription::create(stored);
  ASSERT_TRUE(restored);
  EXPECT_EQ(restored->to_json(), stored);
  auto defaults = AmbientNoisesDescription::create(JsonObject{});
  ASSERT_TRUE(defaults);
  EXPECT_EQ(defaults->track_loops, -1);
  EXPECT_FALSE(AmbientTrackGroup::create(JsonObject{{"tracks", Json{}}}));
  EXPECT_FALSE(AmbientTrackGroup::create(JsonObject{{"tracks", JsonArray{"valid", 42}}}));
}

const GameTestRegistration ambient_voice_lifecycle("AmbientTest", "VoiceLifecycle", [](GameTest& fixture) {
  auto& root = fixture.root();
  auto assets = root.assets();
  ASSERT_TRUE(assets);
  StringList tracks{"/sfx/interface/voice_on.ogg"};
  // Synchronous preload: manager selection deliberately uses the nonblocking asset lookup.
  ASSERT_TRUE((*assets)->audio(tracks.front()));
  auto description = std::make_shared<AmbientNoisesDescription const>(AmbientTrackGroup(tracks), AmbientTrackGroup{}, 0);
  Mixer mixer(root.logger(), 8192, 1);
  mixer.set_speed(0); // Advance fades without touching EOF on the short test clip.
  AmbientManager manager(root);
  manager.set_track_switch_grace(100);
  auto first = manager.update_ambient(description, true);
  ASSERT_TRUE(first && *first);
  EXPECT_EQ((*first)->loops(), 0);
  ASSERT_TRUE(mixer.play(*first));
  auto stable = manager.update_ambient(description, true);
  ASSERT_TRUE(stable);
  EXPECT_FALSE(*stable);
  auto night = manager.update_ambient(description, false);
  ASSERT_TRUE(night);
  EXPECT_FALSE(*night);
  auto day = manager.update_ambient(description, true);
  ASSERT_TRUE(day);
  EXPECT_FALSE(*day); // The matching day voice survived the grace period.
  auto weather = manager.update_weather(tracks);
  ASSERT_TRUE(weather && *weather);
  EXPECT_EQ((*weather)->loops(), -1);
  ASSERT_TRUE(mixer.play(*weather));
  manager.stop_ambient(0.5f);
  auto silent = manager.update_ambient({}, true);
  ASSERT_TRUE(silent);
  EXPECT_FALSE(*silent);
  auto stable_weather = manager.update_weather(tracks);
  ASSERT_TRUE(stable_weather);
  EXPECT_FALSE(*stable_weather);
  std::array<std::int16_t, 2048> samples{};
  ASSERT_TRUE(mixer.read(samples, samples.size()));
  EXPECT_FALSE((*first)->finished());
  ASSERT_TRUE(mixer.read(samples, samples.size()));
  EXPECT_TRUE((*first)->finished());
  EXPECT_FALSE((*weather)->finished());
  auto restarted = manager.update_ambient(description, true);
  ASSERT_TRUE(restarted && *restarted);
  EXPECT_NE(*first, *restarted);
  ASSERT_TRUE(mixer.play(*restarted));
  manager.cancel_all();
  ASSERT_TRUE(mixer.read(samples, samples.size()));
  EXPECT_TRUE((*restarted)->finished());
  EXPECT_TRUE((*weather)->finished());
  auto clear_weather = manager.update_weather({});
  ASSERT_TRUE(clear_weather);
  EXPECT_FALSE(*clear_weather);
});

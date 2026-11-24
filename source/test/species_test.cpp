import star.json;
import star.string;
import star.ordered_map;
import star.bi_map;
import star.thread;
import star.io_device;
import star.list;
import star.image_processing;
import star.hash;
import star.data_stream;
import star.string_view;
import star.directives;
import star.asset_path;
import star.ref_ptr;
import star.logging;
import star.listener;
import std;
import star.format;

import star.asset_source;
import star.assets;
import star.root_base;
import star.configuration;
import star.root;

#include "gtest/gtest.h"

import star.humanoid;
import star.species_database;
import star.game_test;

using namespace Star;

const GameTestRegistration speciesGenderCoverage("SpeciesTest", "NameGenAndOuchNoisesMustCoverEveryGender", [](GameTest& fixture) {
  auto assets = fixture.root().assets();
  ASSERT_TRUE(assets);
  auto speciesFiles = (*assets)->scan_extension("species");
  ASSERT_TRUE(speciesFiles);
  ASSERT_FALSE(speciesFiles->get().empty());

  auto config = (*assets)->json(speciesFiles->get().begin()->string());
  ASSERT_TRUE(config);
  auto valid = SpeciesDefinition::create(fixture.root(), *config);
  ASSERT_TRUE(valid);

  // Both lists are indexed by Gender, so a species has to provide an entry for
  // every gender.  A shorter list used to be read with operator[], which is out
  // of bounds (upstream #599: an empty ouchNoises array was an access
  // violation).
  for (auto key : {String("nameGen"), String("ouchNoises")}) {
    auto emptyConfig = config->set(key, JsonArray());
    ASSERT_TRUE(emptyConfig);
    auto empty = SpeciesDefinition::create(fixture.root(), *emptyConfig);
    ASSERT_FALSE(empty);
    EXPECT_TRUE(empty.error().is<StarException>());
    auto entries = config->get(key);
    ASSERT_TRUE(entries);
    auto first = entries->get(0);
    ASSERT_TRUE(first);
    auto shortConfig = config->set(key, JsonArray{*first});
    ASSERT_TRUE(shortConfig);
    auto shortened = SpeciesDefinition::create(fixture.root(), *shortConfig);
    ASSERT_FALSE(shortened);
    EXPECT_TRUE(shortened.error().is<StarException>());
  }
});

const GameTestRegistration emote_timing_contract("HumanoidTimingTest", "EmoteDomainAndFrameBoundaries", [](GameTest& fixture) {
  JsonArray cycles(emote_size, Json(2.0));
  JsonArray frames(emote_size, Json(4));
  frames[static_cast<std::size_t>(HumanoidEmote::Idle)] = 1;
  frames[static_cast<std::size_t>(HumanoidEmote::Blink)] = std::numeric_limits<int>::max();
  auto load = [&] {
    return Humanoid::HumanoidTiming::create(fixture.root(), JsonObject{{"emoteCycle", cycles}, {"emoteFrames", frames}});
  };
  auto timing = load();
  ASSERT_TRUE(timing) << timing.error().what();
  auto const longest = std::numeric_limits<float>::max();
  EXPECT_EQ(timing->emote_state_seq(longest, HumanoidEmote::Idle), 1);
  EXPECT_EQ(timing->emote_state_seq(longest, HumanoidEmote::Blink), std::numeric_limits<int>::max());
  EXPECT_EQ(timing->emote_state_seq(1.999f, HumanoidEmote::Blabbering), 4);
  EXPECT_EQ(timing->emote_state_seq(2.0f, HumanoidEmote::Blabbering), 1);
  EXPECT_EQ(timing->emote_state_seq(2.5f, HumanoidEmote::Blabbering), 2);
  EXPECT_EQ(timing->emote_state_seq(2.0f, HumanoidEmote::Happy), 4);
  EXPECT_EQ(timing->emote_state_seq(-2.0f, HumanoidEmote::Happy), 1);

  constexpr double nan = std::numeric_limits<double>::quiet_NaN();
  constexpr double infinity = std::numeric_limits<double>::infinity();
  constexpr auto slot = static_cast<std::size_t>(HumanoidEmote::Sleep);
  for (double invalid : {0.0, -1.0, infinity, nan, 1e300, 1e-300}) {
    cycles[slot] = invalid;
    auto rejected = load();
    ASSERT_FALSE(rejected);
    EXPECT_NE(std::string_view(rejected.error().what()).find("Sleep"), std::string_view::npos);
  }
  cycles[slot] = 2.0;
  for (double invalid : {0.0, -1.0, 1.5, 2147483648.0, 4294967297.0, infinity, nan}) {
    frames[slot] = invalid;
    auto rejected = load();
    ASSERT_FALSE(rejected);
    EXPECT_NE(std::string_view(rejected.error().what()).find("Sleep"), std::string_view::npos);
  }
});

TEST(HumanoidEmoteTest, WireAndAssetLayout) {
  struct ExpectedDescription {
    std::string_view name;
    std::string_view frame;
    bool cyclic;
  };
  constexpr ExpectedDescription expected[] = {
    {"Idle", "idle", false}, {"Blabbering", "blabber", true}, {"Shouting", "shout", true},
    {"Happy", "happy", false}, {"Sad", "sad", true}, {"NEUTRAL", "neutral", false},
    {"Laugh", "laugh", true}, {"Annoyed", "annoyed", false}, {"Oh", "oh", false},
    {"OOOH", "oooh", false}, {"Blink", "blink", false}, {"Wink", "wink", false},
    {"Eat", "eat", true}, {"Sleep", "sleep", true}
  };
  static_assert(std::size(expected) == emote_size);
  ASSERT_TRUE(humanoid_emote_names);
  for (std::size_t i = 0; i < emote_size; ++i) {
    auto emote = static_cast<HumanoidEmote>(i);
    auto name = humanoid_emote_names->get_right(emote);
    ASSERT_TRUE(name);
    EXPECT_EQ(name->get().utf8(), expected[i].name);
    EXPECT_EQ(emote_descriptions[i].frame, expected[i].frame);
    EXPECT_EQ(Humanoid::HumanoidTiming::cyclic_emote_state(emote), expected[i].cyclic);
  }
}

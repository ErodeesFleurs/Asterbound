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
import star.algorithm;
import star.file;
import star.name_generator;
import star.random;

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

const GameTestRegistration generator_contract("NameGeneratorTest", "RulesFilteringAndSeededMarkov", [](GameTest& fixture) {
  auto directory = File::temporary_directory(fixture.root().random());
  ASSERT_TRUE(directory) << directory.error().what();
  auto cleanup = make_scope_exit([&] { EXPECT_TRUE(File::remove_all(*directory)); });
  auto const overlay = File::relative_to(*directory, "assets");
  ASSERT_TRUE(File::create_directories(File::relative_to(overlay, "names")));
  auto write = [&](String const& name, String const& contents) {
    auto result = File::write_file(contents, File::relative_to(overlay, name));
    ASSERT_TRUE(result) << result.error().what();
  };
  ASSERT_NO_FATAL_FAILURE(write("preload.config", "[]"));
  ASSERT_NO_FATAL_FAILURE(write("names/profanityfilter.config", R"(["onq"])"));
  ASSERT_NO_FATAL_FAILURE(write("weighted.namesource", R"({"name":"weighted","prefixSize":1,"endSize":2,"sourceNames":["Ax猫","Ax狐","Ax狐","鹿y虎"]})"));
  ASSERT_NO_FATAL_FAILURE(write("unavailable.namesource", R"({"name":"unavailable","prefixSize":3,"endSize":2,"sourceNames":["猫狐"]})"));
  ASSERT_NO_FATAL_FAILURE(write("cycle.namesource", R"({"name":"cycle","sourceNames":["aaaa"]})"));
  ASSERT_NO_FATAL_FAILURE(write("markov.config", R"([{"mode":"markov","source":"weighted","targetLength":[1,3]}])"));
  ASSERT_NO_FATAL_FAILURE(write("no-start.config", R"([{"mode":"markov","source":"unavailable","targetLength":[2,3]}])"));
  ASSERT_NO_FATAL_FAILURE(write("cycle.config", R"([{"mode":"markov","source":"cycle","targetLength":[5000,5000]}])"));
  ASSERT_NO_FATAL_FAILURE(write("nested.config", R"([[{"mode":"serie"},"prefix ",[{"mode":"serie","titleCase":true},"mIXed-case"]],[{"mode":"invalid"}]])"));
  ASSERT_NO_FATAL_FAILURE(write("clean.config", R"([{"mode":"serie"},"xxbad-xxbady"])"));
  ASSERT_NO_FATAL_FAILURE(write("filtered.config", R"([{"mode":"serie"},"ok-ébad"])"));
  ASSERT_NO_FATAL_FAILURE(write("filtered-prefix.config", R"([{"mode":"serie"},"ok BADger"])"));
  ASSERT_NO_FATAL_FAILURE(write("empty.config", R"([{"mode":"serie"}])"));
  ASSERT_NO_FATAL_FAILURE(write("separators.config", R"([{"mode":"serie"}," -  - "])"));
  ASSERT_NO_FATAL_FAILURE(write("exhausted.config", R"(["bad",""])"));
  JsonArray attempts(128, Json("bad"));
  attempts.back() = "clean";
  auto retry_rules = Json(attempts).repr();
  ASSERT_TRUE(retry_rules) << retry_rules.error().what();
  ASSERT_NO_FATAL_FAILURE(write("retry.config", *retry_rules));

  auto settings = fixture.rootSettings();
  settings.assetDirectories.clear();
  settings.assetSources = {overlay};
  settings.storageDirectory = File::relative_to(*directory, "storage");
  settings.logDirectory = {};
  settings.logFile = {};
  settings.runtimeConfigFile = {};
  settings.includeUGC = false;
  auto root = Root::create(settings);
  ASSERT_TRUE(root) << root.error().what();
  auto generator = PatternedNameGenerator::create(**root);
  ASSERT_TRUE(generator) << generator.error().what();

  for (std::uint64_t seed = 0; seed < 64; ++seed) {
    RandomSource oracle(seed);
    auto target = oracle.uniform_uint(1, 3);
    ASSERT_TRUE(target);
    auto const start = oracle.uniform_uint(3);
    auto const max_successor = start == 3 ? 0 : 2;
    (void)oracle.uniform_uint(max_successor);
    auto const ending = oracle.uniform_uint(max_successor);
    RandomSource random(seed);
    auto name = (*generator)->generate_name("/markov.config", random);
    ASSERT_TRUE(name) << name.error().what();
    EXPECT_EQ(name->utf8(), start == 3 ? "鹿y虎" : ending == 0 ? "ax猫" : "ax狐");
    EXPECT_EQ(random.next_u64(), oracle.next_u64());
  }

  bool used_good_branch = false;
  bool used_bad_branch = false;
  for (std::uint64_t seed = 0; seed < 16; ++seed) {
    RandomSource oracle(seed);
    auto const selected = oracle.uniform_uint(1);
    RandomSource random(seed);
    auto name = (*generator)->generate_name("/nested.config", random);
    if (selected == 0) {
      ASSERT_TRUE(name) << name.error().what();
      EXPECT_EQ(name->utf8(), "prefix Mixed-Case");
      used_good_branch = true;
    } else {
      EXPECT_FALSE(name);
      used_bad_branch = true;
    }
    EXPECT_EQ(random.next_u64(), oracle.next_u64());
  }
  EXPECT_TRUE(used_good_branch && used_bad_branch);

  auto clean = (*generator)->generate_name("/clean.config", 0);
  ASSERT_TRUE(clean) << clean.error().what();
  EXPECT_EQ(clean->utf8(), "xxbad-xxbady");
  for (auto const& rule : {String("/filtered.config"), String("/filtered-prefix.config"), String("/empty.config")}) {
    SCOPED_TRACE(rule.utf8());
    EXPECT_FALSE((*generator)->generate_name(rule, 0));
  }
  auto no_start = (*generator)->generate_name("/no-start.config", 0);
  ASSERT_FALSE(no_start);
  EXPECT_NE(std::string_view(no_start.error().what()).find("Empty container"), std::string_view::npos);
  RandomSource cycle_random(0);
  auto cycle = (*generator)->generate_name("/cycle.config", cycle_random);
  ASSERT_FALSE(cycle);
  EXPECT_NE(std::string_view(cycle.error().what()).find("Markov walk limit"), std::string_view::npos);
  RandomSource cycle_oracle(0);
  for (int draw = 0; draw < 4098; ++draw)
    (void)cycle_oracle.uniform_uint(0);
  EXPECT_EQ(cycle_random.next_u64(), cycle_oracle.next_u64());
  RandomSource exhausted_random(17);
  EXPECT_FALSE((*generator)->generate_name("/exhausted.config", exhausted_random));
  RandomSource exhausted_oracle(17);
  for (int draw = 0; draw < 100; ++draw)
    (void)exhausted_oracle.uniform_uint(1);
  EXPECT_EQ(exhausted_random.next_u64(), exhausted_oracle.next_u64());

  // Select a seed whose first clean alternative lands on the last legal attempt.
  bool found_last_attempt = false;
  for (std::uint64_t seed = 0; seed < 10000; ++seed) {
    RandomSource oracle(seed);
    std::size_t first_clean = 0;
    while (first_clean < 100 && oracle.uniform_uint(127) != 127)
      ++first_clean;
    if (first_clean != 99)
      continue;
    RandomSource random(seed);
    auto name = (*generator)->generate_name("/retry.config", random);
    ASSERT_TRUE(name) << name.error().what();
    EXPECT_EQ(name->utf8(), "clean");
    EXPECT_EQ(random.next_u64(), oracle.next_u64());
    found_last_attempt = true;
    break;
  }
  EXPECT_TRUE(found_last_attempt) << "No seed selected the clean alternative on attempt 100";

  auto const empty_word_overlay = File::relative_to(*directory, "empty-word-assets");
  ASSERT_TRUE(File::create_directories(File::relative_to(empty_word_overlay, "names")));
  ASSERT_TRUE(File::write_file(String(R"([""])"), File::relative_to(empty_word_overlay, "names/profanityfilter.config")));
  settings.assetSources.push_back(empty_word_overlay);
  settings.storageDirectory = File::relative_to(*directory, "empty-word-storage");
  auto empty_word_root = Root::create(std::move(settings));
  ASSERT_TRUE(empty_word_root) << empty_word_root.error().what();
  auto empty_word_generator = PatternedNameGenerator::create(**empty_word_root);
  ASSERT_TRUE(empty_word_generator) << empty_word_generator.error().what();
  EXPECT_FALSE((*empty_word_generator)->generate_name("/clean.config", 0));
  auto separators = (*empty_word_generator)->generate_name("/separators.config", 0);
  ASSERT_TRUE(separators) << separators.error().what();
  EXPECT_EQ(separators->utf8(), " -  - ");
});

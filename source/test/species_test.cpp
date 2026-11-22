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

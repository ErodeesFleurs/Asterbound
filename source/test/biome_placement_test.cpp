#include "gtest/gtest.h"

import std;
import star.assets;
import star.bi_map;
import star.biome;
import star.biome_placement;
import star.format;
import star.game_test;
import star.json;
import star.map;
import star.object_database;
import star.root;
import star.string;
import star.unicode;
import star.vector;

using namespace Star;

namespace {

Json distribution_config(String mode, String type, String const& object_name) {
  return JsonObject{
      {"mode", std::move(mode)}, {"type", "object"}, {"variants", 3}, {"priority", 0.375},
      {"objectSets", JsonArray{JsonObject{
          {"parameters", JsonObject{{"direction", "left"}}},
          {"pool", JsonArray{JsonArray{1.0, object_name}}}}}},
      {"distribution", JsonObject{{"type", std::move(type)}, {"blockProbability", 0.75},
          {"modulus", 5}, {"modulusVariance", 0.5}, {"densityOffset", 2.0}}}};
}

const GameTestRegistration storage_round_trip("BiomePlacement", "ConfigAndStoragePreserveModesSeedsAndPlacement", [](GameTest& fixture) {
  auto assets = fixture.root().assets();
  ASSERT_TRUE(assets) << assets.error().what();
  auto database = fixture.root().object_database();
  ASSERT_TRUE(database) << database.error().what();
  auto names = (*database)->all_objects();
  ASSERT_FALSE(names.empty());
  constexpr std::uint64_t seed = 0xfedcba9876543210ULL;
  std::array<std::pair<std::string_view, BiomePlacementMode>, 4> modes{{
      {"FlOoR", BiomePlacementMode::Floor}, {"CeIlInG", BiomePlacementMode::Ceiling},
      {"BaCkGrOuNd", BiomePlacementMode::Background}, {"OcEaN", BiomePlacementMode::Ocean}}};
  for (auto const& [mode, expected_mode] : modes) {
    for (std::string_view type : {"RaNdOm", "PeRiOdIc"}) {
      SCOPED_TRACE(mode);
      SCOPED_TRACE(type);
      auto config = distribution_config(mode, type, names[0]);
      auto created = BiomeItemDistribution::create(fixture.root(), config, seed);
      ASSERT_TRUE(created) << created.error().what();
      EXPECT_EQ(created->mode(), expected_mode);
      auto stored = created->to_json();
      ASSERT_TRUE(stored) << stored.error().what();
      auto canonical_mode = stored->get_string("mode");
      ASSERT_TRUE(canonical_mode) << canonical_mode.error().what();
      auto lowered_mode = String(mode).to_lower();
      ASSERT_TRUE(lowered_mode) << lowered_mode.error().what();
      EXPECT_EQ(*canonical_mode, *lowered_mode);
      auto canonical_type = stored->get_string("distribution");
      ASSERT_TRUE(canonical_type) << canonical_type.error().what();
      auto lowered_type = String(type).to_lower();
      ASSERT_TRUE(lowered_type) << lowered_type.error().what();
      EXPECT_EQ(*canonical_type, *lowered_type);
      auto repeated = BiomeItemDistribution::create(fixture.root(), config, seed);
      ASSERT_TRUE(repeated) << repeated.error().what();
      auto repeated_store = repeated->to_json();
      ASSERT_TRUE(repeated_store) << repeated_store.error().what();
      EXPECT_EQ(*repeated_store, *stored);

      auto mixed_mode_store = stored->set("mode", mode);
      ASSERT_TRUE(mixed_mode_store) << mixed_mode_store.error().what();
      auto mixed_store = mixed_mode_store->set("distribution", type);
      ASSERT_TRUE(mixed_store) << mixed_store.error().what();
      auto restored = BiomeItemDistribution::create(**assets, *mixed_store);
      ASSERT_TRUE(restored) << restored.error().what();
      EXPECT_EQ(restored->mode(), expected_mode);
      auto restored_store = restored->to_json();
      ASSERT_TRUE(restored_store) << restored_store.error().what();
      EXPECT_EQ(*restored_store, *stored);
      EXPECT_EQ(created->size(), 3u);
      EXPECT_EQ(restored->size(), 3u);
      unsigned placements = 0;
      for (int x = -32; x <= 32; ++x) {
        auto original_item = created->item_to_place(x, 17);
        ASSERT_TRUE(original_item) << original_item.error().what();
        auto restored_item = restored->item_to_place(x, 17);
        ASSERT_TRUE(restored_item) << restored_item.error().what();
        ASSERT_EQ(original_item->has_value(), restored_item->has_value());
        if (!*original_item)
          continue;
        ++placements;
        EXPECT_EQ((**original_item).position, (**restored_item).position);
        EXPECT_EQ((**original_item).priority, (**restored_item).priority);
        auto original_json = variant_from_biome_item((**original_item).item);
        ASSERT_TRUE(original_json) << original_json.error().what();
        auto restored_json = variant_from_biome_item((**restored_item).item);
        ASSERT_TRUE(restored_json) << restored_json.error().what();
        EXPECT_EQ(*original_json, *restored_json);
      }
      EXPECT_GT(placements, 0u);
    }
  }
});

const GameTestRegistration micro_names("BiomePlacement", "RejectEmptyMicroDungeonSelections", [](GameTest& fixture) {
  auto assets = fixture.root().assets();
  if (!fixture.check(bool(assets), "load biome assets")) return;
  for (Json names : std::array<Json, 2>{JsonArray{}, JsonArray{"valid", ""}}) {
    Json config = JsonObject{{"mode", "floor"}, {"type", "microdungeon"}, {"variants", 1}, {"microdungeons", names},
        {"distribution", JsonObject{{"type", "random"}, {"blockProbability", 1}}}};
    fixture.check(!BiomeItemDistribution::create(fixture.root(), config, 42), "reject unselectable names in config");
    fixture.check(!variant_to_biome_item(**assets, JsonArray{"microDungeon", names}), "reject unselectable names in storage");
  }
});

const GameTestRegistration empty_random("BiomePlacement", "EmptyRandomPoolDoesNotPlaceDefaultGrass", [](GameTest& fixture) {
  auto& root = fixture.root();
  auto assets = root.assets();
  ASSERT_TRUE(assets);
  Json config = JsonObject{{"mode", "floor"}, {"type", "object"}, {"variants", 0},
      {"distribution", JsonObject{{"type", "random"}, {"blockProbability", 1}}}};
  auto distribution = BiomeItemDistribution::create(root, config, 42);
  ASSERT_TRUE(distribution);
  auto item = distribution->item_at(0, 0);
  ASSERT_TRUE(item);
  EXPECT_FALSE(*item);
  auto stored = distribution->to_json();
  ASSERT_TRUE(stored);
  // Old worlds can contain arbitrary octaves in the unused density function.
  auto legacy = stored->set_path("densityFunction.octaves", -1);
  ASSERT_TRUE(legacy);
  auto restored = BiomeItemDistribution::create(**assets, *legacy);
  ASSERT_TRUE(restored);
  auto placement = restored->item_to_place(0, 0);
  ASSERT_TRUE(placement);
  EXPECT_FALSE(*placement);
  for (auto count : std::array<Json, 3>{-1, std::int64_t{1} << 31, 0.5}) {
    auto invalid = config.set("variants", count);
    ASSERT_TRUE(invalid);
    EXPECT_FALSE(BiomeItemDistribution::create(root, *invalid, 42));
  }
});

const GameTestRegistration periodic_limits("BiomePlacement", "PeriodicCoordinateLimits", [](GameTest& fixture) {
  auto assets = fixture.root().assets();
  ASSERT_TRUE(assets);
  auto config = distribution_config("floor", "periodic", "unused");
  auto original = BiomeItemDistribution::create(fixture.root(), config, 42);
  ASSERT_TRUE(original);
  auto stored = original->to_json();
  ASSERT_TRUE(stored);
  auto sample = [&](int x, int offset, int modulus) -> std::expected<bool, StarException> {
    auto adjusted = stored->set("modulus", modulus);
    if (!adjusted) return std::unexpected(std::move(adjusted.error()));
    adjusted = adjusted->set("modulusOffset", offset);
    if (!adjusted) return std::unexpected(std::move(adjusted.error()));
    for (std::string_view field : {"densityFunction", "modulusDistortion"}) {
      auto noise = adjusted->get(field);
      if (!noise) return std::unexpected(std::move(noise.error()));
      noise = noise->set("frequency", 0);
      if (!noise) return std::unexpected(std::move(noise.error()));
      noise = noise->set("bias", field == "densityFunction" ? 1 : 0);
      if (!noise) return std::unexpected(std::move(noise.error()));
      adjusted = adjusted->set(field, std::move(*noise));
      if (!adjusted) return std::unexpected(std::move(adjusted.error()));
    }
    auto distribution = BiomeItemDistribution::create(**assets, *adjusted);
    if (!distribution) return std::unexpected(std::move(distribution.error()));
    auto item = distribution->item_at(x, 0);
    if (!item) return std::unexpected(std::move(item.error()));
    return item->has_value();
  };
  EXPECT_EQ(sample(0, 0, 1), true);
  EXPECT_FALSE(sample(1, std::numeric_limits<int>::max(), 1).has_value());
  EXPECT_FALSE(sample(0, std::numeric_limits<int>::max(), 1).has_value());
  EXPECT_EQ(sample(0, std::numeric_limits<int>::min(), -1), true);
});

const GameTestRegistration stored_ids("BiomePlacement", "StoredIdsRejectTruncationAndRetainSentinels", [](GameTest& fixture) {
  Biome biome;
  auto store = biome.to_json();
  ASSERT_TRUE(store);
  for (std::string_view field : {"mainBlock", "materialHueShift", "subBlocks", "ores"}) {
    auto value = [&](Json number) -> Json {
      if (field == "subBlocks") return JsonArray{number};
      if (field == "ores") return JsonArray{JsonArray{number, 1}};
      return number;
    };
    int maximum = field == "materialHueShift" ? 255 : 65535;
    auto allowed = store->set(field, value(maximum));
    ASSERT_TRUE(allowed);
    auto restored = Biome::create(fixture.root(), *allowed);
    ASSERT_TRUE(restored) << restored.error().what();
    auto saved = restored->to_json();
    ASSERT_TRUE(saved);
    EXPECT_EQ(saved->get(field), value(maximum));
    for (auto invalid : std::array<Json, 3>{-1, maximum + 1, 0.5}) {
      auto bad = store->set(field, value(invalid));
      ASSERT_TRUE(bad);
      EXPECT_FALSE(Biome::create(fixture.root(), *bad));
    }
  }
  auto assets = fixture.root().assets();
  ASSERT_TRUE(assets);
  auto placeables = biome.surface_placeables.to_json();
  ASSERT_TRUE(placeables);
  for (std::string_view field : {"grassMod", "ceilingGrassMod"}) {
    auto overflow = placeables->set(field, 65536);
    ASSERT_TRUE(overflow);
    EXPECT_FALSE(BiomePlaceables::create(**assets, *overflow));
  }
});

const GameTestRegistration rejected_names("BiomePlacement", "FactoriesPreserveNameErrorsAndFirstError", [](GameTest& fixture) {
  auto assets = fixture.root().assets();
  ASSERT_TRUE(assets) << assets.error().what();
  for (auto bad : {String("unknown"), String("日本語"), String(std::string_view("floor\0", 6))}) {
    auto config_mode = BiomeItemDistribution::create(fixture.root(), JsonObject{{"mode", bad}}, 42);
    ASSERT_FALSE(config_mode);
    EXPECT_TRUE(config_mode.error().is<MapException>());
    auto store_mode = BiomeItemDistribution::create(**assets, JsonObject{{"mode", bad}});
    ASSERT_FALSE(store_mode);
    EXPECT_TRUE(store_mode.error().is<MapException>());
    auto config_type = BiomeItemDistribution::create(fixture.root(),
        JsonObject{{"distribution", JsonObject{{"type", bad}}}}, 42);
    ASSERT_FALSE(config_type);
    EXPECT_TRUE(config_type.error().is<MapException>());
    auto store_type = BiomeItemDistribution::create(**assets,
        JsonObject{{"mode", "floor"}, {"distribution", bad}});
    ASSERT_FALSE(store_type);
    EXPECT_TRUE(store_type.error().is<MapException>());
  }
  String invalid(std::string_view("floor\xff", 6));
  auto config_mode = BiomeItemDistribution::create(fixture.root(), JsonObject{{"mode", invalid}}, 42);
  ASSERT_FALSE(config_mode);
  EXPECT_TRUE(config_mode.error().is<UnicodeException>());
  auto store_type = BiomeItemDistribution::create(**assets,
      JsonObject{{"mode", "floor"}, {"distribution", invalid}});
  ASSERT_FALSE(store_type);
  EXPECT_TRUE(store_type.error().is<UnicodeException>());
});

}

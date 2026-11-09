#include "gtest/gtest.h"

import std;
import star.assets;
import star.bi_map;
import star.biome_placement;
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

Json distributionConfig(String mode, String type, String const& objectName) {
  return JsonObject{
      {"mode", std::move(mode)}, {"type", "object"}, {"variants", 3}, {"priority", 0.375},
      {"objectSets", JsonArray{JsonObject{
          {"parameters", JsonObject{{"direction", "left"}}},
          {"pool", JsonArray{JsonArray{1.0, objectName}}}}}},
      {"distribution", JsonObject{{"type", std::move(type)}, {"blockProbability", 0.75},
          {"modulus", 5}, {"modulusVariance", 0.5}, {"densityOffset", 2.0}}}};
}

const GameTestRegistration storageRoundTrip("BiomePlacement", "ConfigAndStoragePreserveModesSeedsAndPlacement", [](GameTest& fixture) {
  auto assets = fixture.root().assets();
  ASSERT_TRUE(assets) << assets.error().what();
  auto database = fixture.root().objectDatabase();
  ASSERT_TRUE(database) << database.error().what();
  auto names = (*database)->allObjects();
  ASSERT_FALSE(names.empty());
  constexpr std::uint64_t seed = 0xfedcba9876543210ULL;
  std::array<std::pair<std::string_view, BiomePlacementMode>, 4> modes{{
      {"FlOoR", BiomePlacementMode::Floor}, {"CeIlInG", BiomePlacementMode::Ceiling},
      {"BaCkGrOuNd", BiomePlacementMode::Background}, {"OcEaN", BiomePlacementMode::Ocean}}};
  for (auto const& [mode, expectedMode] : modes) {
    for (std::string_view type : {"RaNdOm", "PeRiOdIc"}) {
      SCOPED_TRACE(mode);
      SCOPED_TRACE(type);
      auto config = distributionConfig(mode, type, names[0]);
      auto created = BiomeItemDistribution::create(fixture.root(), config, seed);
      ASSERT_TRUE(created) << created.error().what();
      EXPECT_EQ(created->mode(), expectedMode);
      auto stored = created->toJson();
      ASSERT_TRUE(stored) << stored.error().what();
      auto canonicalMode = stored->getString("mode");
      ASSERT_TRUE(canonicalMode) << canonicalMode.error().what();
      auto loweredMode = String(mode).to_lower();
      ASSERT_TRUE(loweredMode) << loweredMode.error().what();
      EXPECT_EQ(*canonicalMode, *loweredMode);
      auto canonicalType = stored->getString("distribution");
      ASSERT_TRUE(canonicalType) << canonicalType.error().what();
      auto loweredType = String(type).to_lower();
      ASSERT_TRUE(loweredType) << loweredType.error().what();
      EXPECT_EQ(*canonicalType, *loweredType);
      auto repeated = BiomeItemDistribution::create(fixture.root(), config, seed);
      ASSERT_TRUE(repeated) << repeated.error().what();
      auto repeatedStore = repeated->toJson();
      ASSERT_TRUE(repeatedStore) << repeatedStore.error().what();
      EXPECT_EQ(*repeatedStore, *stored);

      auto mixedModeStore = stored->set("mode", mode);
      ASSERT_TRUE(mixedModeStore) << mixedModeStore.error().what();
      auto mixedStore = mixedModeStore->set("distribution", type);
      ASSERT_TRUE(mixedStore) << mixedStore.error().what();
      auto restored = BiomeItemDistribution::create(**assets, *mixedStore);
      ASSERT_TRUE(restored) << restored.error().what();
      EXPECT_EQ(restored->mode(), expectedMode);
      auto restoredStore = restored->toJson();
      ASSERT_TRUE(restoredStore) << restoredStore.error().what();
      EXPECT_EQ(*restoredStore, *stored);
      EXPECT_EQ(created->allItems().size(), 3u);
      EXPECT_EQ(restored->allItems().size(), 3u);
      unsigned placements = 0;
      for (int x = -32; x <= 32; ++x) {
        auto originalItem = created->itemToPlace(x, 17);
        ASSERT_TRUE(originalItem) << originalItem.error().what();
        auto restoredItem = restored->itemToPlace(x, 17);
        ASSERT_TRUE(restoredItem) << restoredItem.error().what();
        ASSERT_EQ(originalItem->has_value(), restoredItem->has_value());
        if (!*originalItem)
          continue;
        ++placements;
        EXPECT_EQ((**originalItem).position, (**restoredItem).position);
        EXPECT_EQ((**originalItem).priority, (**restoredItem).priority);
        auto originalJson = variantFromBiomeItem((**originalItem).item);
        ASSERT_TRUE(originalJson) << originalJson.error().what();
        auto restoredJson = variantFromBiomeItem((**restoredItem).item);
        ASSERT_TRUE(restoredJson) << restoredJson.error().what();
        EXPECT_EQ(*originalJson, *restoredJson);
      }
      EXPECT_GT(placements, 0u);
    }
  }
});

const GameTestRegistration rejectedNames("BiomePlacement", "FactoriesPreserveNameErrorsAndFirstError", [](GameTest& fixture) {
  auto assets = fixture.root().assets();
  ASSERT_TRUE(assets) << assets.error().what();
  for (auto bad : {String("unknown"), String("日本語"), String(std::string_view("floor\0", 6))}) {
    auto configMode = BiomeItemDistribution::create(fixture.root(), JsonObject{{"mode", bad}}, 42);
    ASSERT_FALSE(configMode);
    EXPECT_TRUE(configMode.error().is<MapException>());
    auto storeMode = BiomeItemDistribution::create(**assets, JsonObject{{"mode", bad}});
    ASSERT_FALSE(storeMode);
    EXPECT_TRUE(storeMode.error().is<MapException>());
    auto configType = BiomeItemDistribution::create(fixture.root(),
        JsonObject{{"distribution", JsonObject{{"type", bad}}}}, 42);
    ASSERT_FALSE(configType);
    EXPECT_TRUE(configType.error().is<MapException>());
    auto storeType = BiomeItemDistribution::create(**assets,
        JsonObject{{"mode", "floor"}, {"distribution", bad}});
    ASSERT_FALSE(storeType);
    EXPECT_TRUE(storeType.error().is<MapException>());
  }
  String invalid(std::string_view("floor\xff", 6));
  auto configMode = BiomeItemDistribution::create(fixture.root(), JsonObject{{"mode", invalid}}, 42);
  ASSERT_FALSE(configMode);
  EXPECT_TRUE(configMode.error().is<UnicodeException>());
  auto storeType = BiomeItemDistribution::create(**assets,
      JsonObject{{"mode", "floor"}, {"distribution", invalid}});
  ASSERT_FALSE(storeType);
  EXPECT_TRUE(storeType.error().is<UnicodeException>());
});

}

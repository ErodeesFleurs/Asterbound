#include "gtest/gtest.h"

import std;
import star.algorithm;
import star.assets;
import star.damage_database;
import star.file;
import star.format;
import star.game_test;
import star.logging;
import star.root;
import star.string;

using namespace Star;

namespace {

// Only these assets participate in each database load; the shared game Root
// supplies runtime services, not damage definitions from the installed game.
std::expected<std::shared_ptr<DamageDatabase>, StarException>
damageFixture(GameTest& fixture, String const& directory, std::initializer_list<std::pair<String, String>> files) {
  auto damageDirectory = File::makeDirectoryRecursive(File::relativeTo(directory, "damage"));
  if (!damageDirectory)
    return std::unexpected(std::move(damageDirectory.error()));
  auto preload = File::writeFile(String("[]"), File::relativeTo(directory, "preload.config"));
  if (!preload)
    return std::unexpected(std::move(preload.error()));
  auto elements = File::writeFile(String(R"({
    "default":{"resistanceStat":"defaultResistance","damageNumberParticles":{}},
    "ElementOnly":{"resistanceStat":"elementResistance","damageNumberParticles":{}}
  })"),
                                  File::relativeTo(directory, "damage/elementaltypes.config"));
  if (!elements)
    return std::unexpected(std::move(elements.error()));
  for (auto const& [path, contents] : files) {
    auto written = File::writeFile(contents, File::relativeTo(directory, path));
    if (!written)
      return std::unexpected(std::move(written.error()));
  }
  Assets::Settings settings{};
  settings.workerPoolSize = 1;
  settings.assetTimeToLive = 10;
  Logger logger;
  auto assets = Assets::create(logger, fixture.root().random(), fixture.root().logMap(), settings, {directory});
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  return DamageDatabase::create(*assets);
}

}// namespace

const GameTestRegistration
  damageLookup("DamageDatabaseTest", "CaseInsensitiveKindsAndIndependentElementalTypes", [](GameTest& fixture) {
    auto directory = File::temporaryDirectory(fixture.root().random());
    ASSERT_TRUE(directory);
    auto cleanup = finally([&] { EXPECT_TRUE(File::removeDirectoryRecursive(*directory)); });
    auto database = damageFixture(fixture, *directory,
                                  {{"damage/default.damage", R"({"kind":"DeFaUlT"})"},
                                   {"damage/mixed.damage", R"({"kind":"MiXeD","elementalType":"ElementOnly"})"},
                                   {"damage/kindonly.damage", R"({"kind":"KindOnly"})"}});
    ASSERT_TRUE(database) << database.error().what();
    auto mixed = (*database)->damageKind("MiXeD");
    ASSERT_TRUE(mixed);
    EXPECT_EQ(mixed->get().name, "MiXeD");
    EXPECT_EQ(mixed->get().elementalType, "ElementOnly");
    for (String const& query : {String("mixed"), String("MIXED"), String("mIxEd")}) {
      auto found = (*database)->damageKind(query);
      ASSERT_TRUE(found) << found.error().what();
      EXPECT_EQ(&found->get(), &mixed->get());
    }
    auto defaultKind = (*database)->damageKind("");
    auto namedDefault = (*database)->damageKind("DEFAULT");
    ASSERT_TRUE(defaultKind);
    ASSERT_TRUE(namedDefault);
    EXPECT_EQ(&defaultKind->get(), &namedDefault->get());
    EXPECT_EQ(defaultKind->get().name, "DeFaUlT");
    EXPECT_FALSE((*database)->damageKind("unknown"));

    // This type has no damage kind with the same name. Conversely, a damage kind
    // is not an elemental type, and must get the domain's unknown-type error.
    auto element = (*database)->elementalType("ElementOnly");
    ASSERT_TRUE(element) << element.error().what();
    EXPECT_EQ(element->get().resistanceStat, "elementResistance");
    auto kindOnly = (*database)->elementalType("KindOnly");
    ASSERT_FALSE(kindOnly);
    EXPECT_NE(std::string_view(kindOnly.error().what()).find("Unknown elemental type"), std::string_view::npos);
    EXPECT_FALSE((*database)->elementalType("elementonly"));
  });

const GameTestRegistration
  damageDuplicates("DamageDatabaseTest", "RejectsExactAndCaseOnlyKindDuplicates", [](GameTest& fixture) {
    auto directory = File::temporaryDirectory(fixture.root().random());
    ASSERT_TRUE(directory);
    auto cleanup = finally([&] { EXPECT_TRUE(File::removeDirectoryRecursive(*directory)); });
    for (String const& duplicate : {String("MiXeD"), String("mIxEd")}) {
      SCOPED_TRACE(duplicate.utf8());
      auto caseDirectory = File::relativeTo(*directory, duplicate);
      auto database = damageFixture(fixture, caseDirectory,
                                    {{"damage/first.damage", R"({"kind":"MiXeD"})"},
                                     {"damage/second.damage", strf(R"({{"kind":"{}"}})", duplicate)}});
      ASSERT_FALSE(database);
      auto message = std::string_view(database.error().what());
      EXPECT_NE(message.find("Duplicate damage kind Name"), std::string_view::npos);
      EXPECT_NE(message.find(".damage"), std::string_view::npos);
    }
  });

const GameTestRegistration
  damageUndefinedElement("DamageDatabaseTest", "RejectsUndefinedElementalTypeDuringLoad", [](GameTest& fixture) {
    auto directory = File::temporaryDirectory(fixture.root().random());
    ASSERT_TRUE(directory);
    auto cleanup = finally([&] { EXPECT_TRUE(File::removeDirectoryRecursive(*directory)); });
    auto database =
      damageFixture(fixture, *directory,
                    {{"damage/invalid.damage", R"({"kind":"MissingElementKind","elementalType":"MissingElement"})"}});
    ASSERT_FALSE(database);
    auto message = std::string_view(database.error().what());
    EXPECT_NE(message.find("Undefined elemental type MissingElement"), std::string_view::npos);
    EXPECT_NE(message.find("MissingElementKind"), std::string_view::npos);
  });

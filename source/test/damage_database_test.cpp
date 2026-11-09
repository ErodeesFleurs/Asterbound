#include "gtest/gtest.h"

import std;
import star.algorithm;
import star.assets;
import star.damage_database;
import star.damage_types;
import star.damage;
import star.lua;
import star.lua_game_converters;
import star.unicode;
import star.file;
import star.format;
import star.game_test;
import star.json;
import star.logging;
import star.map;
import star.root;
import star.string;

using namespace Star;

namespace {

// Only these assets participate in each database load; the shared game Root
// supplies runtime services, not damage definitions from the installed game.
std::expected<std::shared_ptr<DamageDatabase>, StarException>
damageFixture(GameTest& fixture, String const& directory, std::initializer_list<std::pair<String, String>> files) {
  auto damageDirectory = File::create_directories(File::relative_to(directory, "damage"));
  if (!damageDirectory)
    return std::unexpected(std::move(damageDirectory.error()));
  auto preload = File::write_file(String("[]"), File::relative_to(directory, "preload.config"));
  if (!preload)
    return std::unexpected(std::move(preload.error()));
  auto elements = File::write_file(String(R"({
    "default":{"resistanceStat":"defaultResistance","damageNumberParticles":{}},
    "ElementOnly":{"resistanceStat":"elementResistance","damageNumberParticles":{}}
  })"),
                                   File::relative_to(directory, "damage/elementaltypes.config"));
  if (!elements)
    return std::unexpected(std::move(elements.error()));
  for (auto const& [path, contents] : files) {
    auto written = File::write_file(contents, File::relative_to(directory, path));
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
    auto directory = File::temporary_directory(fixture.root().random());
    ASSERT_TRUE(directory);
    auto cleanup = finally([&] { EXPECT_TRUE(File::remove_all(*directory)); });
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
    auto directory = File::temporary_directory(fixture.root().random());
    ASSERT_TRUE(directory);
    auto cleanup = finally([&] { EXPECT_TRUE(File::remove_all(*directory)); });
    for (String const& duplicate : {String("MiXeD"), String("mIxEd")}) {
      SCOPED_TRACE(duplicate.utf8());
      auto caseDirectory = File::relative_to(*directory, duplicate);
      auto database = damageFixture(fixture, caseDirectory,
                                    {{"damage/first.damage", R"({"kind":"MiXeD"})"},
                                     {"damage/second.damage", Star::format(R"({{"kind":"{}"}})", duplicate)}});
      ASSERT_FALSE(database);
      auto message = std::string_view(database.error().what());
      EXPECT_NE(message.find("Duplicate damage kind Name"), std::string_view::npos);
      EXPECT_NE(message.find(".damage"), std::string_view::npos);
    }
  });

const GameTestRegistration
  damageUndefinedElement("DamageDatabaseTest", "RejectsUndefinedElementalTypeDuringLoad", [](GameTest& fixture) {
    auto directory = File::temporary_directory(fixture.root().random());
    ASSERT_TRUE(directory);
    auto cleanup = finally([&] { EXPECT_TRUE(File::remove_all(*directory)); });
    auto database =
      damageFixture(fixture, *directory,
                    {{"damage/invalid.damage", R"({"kind":"MissingElementKind","elementalType":"MissingElement"})"}});
    ASSERT_FALSE(database);
    auto message = std::string_view(database.error().what());
    EXPECT_NE(message.find("Undefined elemental type MissingElement"), std::string_view::npos);
    EXPECT_NE(message.find("MissingElementKind"), std::string_view::npos);
  });

const GameTestRegistration
  damageNestedEffects("DamageDatabaseTest", "NestedEffectsPreservePayloadsAndNullDefaults", [](GameTest& fixture) {
    auto directory = File::temporary_directory(fixture.root().random());
    ASSERT_TRUE(directory);
    auto cleanup = finally([&] { EXPECT_TRUE(File::remove_all(*directory)); });
    auto database = damageFixture(fixture, *directory,
                                  {{"damage/elementaltypes.config", R"({
        "default":{"resistanceStat":"health","damageNumberParticles":{"hIt":"normal","kIlL":"finish"}}
      })"},
                                   {"damage/nested.damage", R"({
        "kind":"Nested","effects":{
          "flesh":{"hIt":{"sounds":["impact"],"particles":{"type":"spark"}},"kIlL":{}},
          "metal":{"sTrOnGhIt":{"sounds":null,"particles":null}}
        }
      })"},
                                   {"damage/missing.damage", R"({"kind":"Missing"})"},
                                   {"damage/null.damage", R"({"kind":"Null","effects":null})"}});
    ASSERT_TRUE(database) << database.error().what();
    auto elemental = (*database)->elementalType("default");
    ASSERT_TRUE(elemental);
    auto numberParticle = elemental->get().damageNumberParticles.get(HitType::Kill);
    ASSERT_TRUE(numberParticle);
    EXPECT_EQ(numberParticle->get(), "finish");
    auto nested = (*database)->damageKind("nested");
    ASSERT_TRUE(nested);
    auto flesh = nested->get().effects.get("flesh");
    ASSERT_TRUE(flesh);
    auto hit = flesh->get().get(HitType::Hit);
    ASSERT_TRUE(hit);
    EXPECT_EQ(hit->get().sounds, Json(JsonArray{"impact"}));
    EXPECT_EQ(hit->get().particles, Json(JsonObject{{"type", "spark"}}));
    auto kill = flesh->get().get(HitType::Kill);
    ASSERT_TRUE(kill);
    EXPECT_EQ(kill->get().sounds, Json(JsonArray{}));
    EXPECT_EQ(kill->get().particles, Json(JsonArray{}));
    auto metal = nested->get().effects.get("metal");
    ASSERT_TRUE(metal);
    auto strong = metal->get().get(HitType::StrongHit);
    ASSERT_TRUE(strong);
    EXPECT_TRUE(strong->get().sounds.isNull());
    EXPECT_TRUE(strong->get().particles.isNull());
    for (String const& name : {String("Missing"), String("Null")}) {
      auto empty = (*database)->damageKind(name);
      ASSERT_TRUE(empty);
      EXPECT_TRUE(empty->get().effects.empty());
    }
  });

const GameTestRegistration
  damageNestedFailures("DamageDatabaseTest", "NestedTypesAndHitNamesRetainFirstErrorOrder", [](GameTest& fixture) {
    auto directory = File::temporary_directory(fixture.root().random());
    ASSERT_TRUE(directory);
    auto cleanup = finally([&] { EXPECT_TRUE(File::remove_all(*directory)); });
    struct Failure {
      String elements;
      String damage;
      bool mapError;
    };
    Failure const cases[] = {
      // Resistance is read before the unknown particle name.
      {R"({"default":{"resistanceStat":3,"damageNumberParticles":{"UnknownHit":3}}})", R"({"kind":"Invalid"})", false},
      // Particle names are resolved before particle strings or damage files.
      {R"({"default":{"resistanceStat":"health","damageNumberParticles":{"UnknownHit":3}}})", R"({"kind":3})", true},
      {R"({"default":{"resistanceStat":"health","damageNumberParticles":[]}})",
       R"({"kind":"Invalid","elementalType":"unknown"})", false},
      // The effects object and each material's hit object precede elemental lookup.
      {"", R"({"kind":"Invalid","effects":[],"elementalType":"unknown"})", false},
      {"", R"({"kind":"Invalid","effects":{"flesh":[]},"elementalType":"unknown"})", false},
      // A hit payload is queried for sounds/particles before resolving its name.
      {"", R"({"kind":"Invalid","effects":{"flesh":{"UnknownHit":3}}})", false},
      {"", R"({"kind":"Invalid","effects":{"flesh":{"Hit":null}}})", false},
      {"", R"({"kind":"Invalid","effects":{"flesh":{"UnknownHit":{}}},"elementalType":"unknown"})", true}};
    unsigned index = 0;
    for (auto const& failure : cases) {
      SCOPED_TRACE(index);
      auto caseDirectory = File::relative_to(*directory, Star::format("case{}", index++));
      auto database = failure.elements.empty()
        ? damageFixture(fixture, caseDirectory, {{"damage/invalid.damage", failure.damage}})
        : damageFixture(
            fixture, caseDirectory,
            {{"damage/elementaltypes.config", failure.elements}, {"damage/invalid.damage", failure.damage}});
      ASSERT_FALSE(database);
      EXPECT_EQ(database.error().is<MapException>(), failure.mapError);
      EXPECT_EQ(database.error().is<JsonException>(), !failure.mapError);
    }
  });

const GameTestRegistration
  damageEnumJsonBoundaries("DamageTypesTest", "JsonDefaultsCanonicalNamesAndFirstError", [](GameTest&) {
    for (Json const& input : {Json(JsonObject{{"damage", 3}}),
                              Json(JsonObject{{"damage", 3}, {"hitType", Json()}, {"damageType", Json()}})}) {
      auto request = DamageRequest::create(input);
      ASSERT_TRUE(request);
      EXPECT_EQ(request->hitType, HitType::Hit);
      EXPECT_EQ(request->damageType, DamageType::Damage);
      auto stored = request->toJson();
      ASSERT_TRUE(stored);
      EXPECT_EQ(stored->getString("hitType"), "Hit");
      EXPECT_EQ(stored->getString("damageType"), "Damage");
    }
    auto mixedCase =
      DamageRequest::create(JsonObject{{"damage", 3}, {"hitType", "sTrOnGhIt"}, {"damageType", "iGnOrEsDeF"}});
    ASSERT_TRUE(mixedCase);
    EXPECT_EQ(mixedCase->hitType, HitType::StrongHit);
    EXPECT_EQ(mixedCase->damageType, DamageType::IgnoresDef);
    auto stored = mixedCase->toJson();
    ASSERT_TRUE(stored);
    EXPECT_EQ(stored->getString("hitType"), "StrongHit");
    EXPECT_EQ(stored->getString("damageType"), "IgnoresDef");

    // An unknown first enum wins over a later malformed UTF-8 name or missing damage.
    for (String const& name : {String("unknown"), String("日本語"), String(std::string_view("Hit\0", 4))}) {
      auto rejected =
        DamageRequest::create(JsonObject{{"hitType", name}, {"damageType", String(std::string_view("Damage\xff", 7))}});
      ASSERT_FALSE(rejected);
      EXPECT_TRUE(rejected.error().is<MapException>());
    }
    auto malformed =
      DamageRequest::create(JsonObject{{"hitType", String(std::string_view("Hit\xff", 4))}, {"damageType", "unknown"}});
    ASSERT_FALSE(malformed);
    EXPECT_TRUE(malformed.error().is<UnicodeException>());
    auto team = EntityDamageTeam::create(JsonObject{{"type", "FrIeNdLy"}});
    ASSERT_TRUE(team);
    EXPECT_EQ(team->type, TeamType::Friendly);
    EXPECT_EQ(team->team, 0);
    EXPECT_EQ(team->toJson(), Json(JsonObject{{"type", "friendly"}, {"team", 0}}));
  });

const GameTestRegistration
  damageEnumLuaBoundaries("DamageTypesTest", "LuaOptionalLookupAndCanonicalRoundTrip", [](GameTest&) {
    auto engine = LuaEngine::create();
    ASSERT_TRUE(engine);
    auto context = (*engine)->createContext();
    ASSERT_TRUE(context);
    auto defaults = context->eval<DamageRequest>("return {damage = 3}");
    ASSERT_TRUE(defaults);
    EXPECT_EQ(defaults->hitType, HitType::Hit);
    EXPECT_EQ(defaults->damageType, DamageType::Damage);
    auto request =
      context->eval<DamageRequest>("return {damage = 3, hitType = 'sTrOnGhIt', damageType = 'iGnOrEsDeF'}");
    ASSERT_TRUE(request);
    ASSERT_TRUE(context->setPath("request", *request));
    auto rendered = context->eval<bool>("return request.hitType == 'StrongHit' and request.damageType == 'IgnoresDef'");
    ASSERT_TRUE(rendered);
    EXPECT_TRUE(*rendered);

    for (String const& name : {String("unknown"), String("日本語"), String(std::string_view("Hit\0", 4))}) {
      ASSERT_TRUE(context->setPath("name", name));
      auto value = context->eval<LuaValue>("return {damage = 3, hitType = name, damageType = string.char(255)}");
      ASSERT_TRUE(value);
      auto rejected = (*engine)->luaMaybeTo<DamageRequest>(*value);
      ASSERT_TRUE(rejected);
      EXPECT_FALSE(*rejected);
    }
    auto malformed =
      context->eval<LuaValue>("return {damage = 3, hitType = 'Hit' .. string.char(255), damageType = 'unknown'}");
    ASSERT_TRUE(malformed);
    auto rejected = (*engine)->luaMaybeTo<DamageRequest>(*malformed);
    ASSERT_FALSE(rejected);
    EXPECT_TRUE(rejected.error().is<UnicodeException>());

    DamageNotification notification;
    notification.hitType = HitType::ShieldHit;
    ASSERT_TRUE(context->setPath("notification", notification));
    auto roundTrip = context->eval<DamageNotification>("return notification");
    ASSERT_TRUE(roundTrip);
    EXPECT_EQ(roundTrip->hitType, HitType::ShieldHit);
    auto unknown = context->eval<LuaValue>("notification.hitType = 'unknown'; return notification");
    ASSERT_TRUE(unknown);
    auto noNotification = (*engine)->luaMaybeTo<DamageNotification>(*unknown);
    ASSERT_TRUE(noNotification);
    EXPECT_FALSE(*noNotification);
  });

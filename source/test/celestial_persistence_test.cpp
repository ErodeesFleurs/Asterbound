#include "gtest/gtest.h"
#if defined(STAR_SYSTEM_LINUX) && GTEST_HAS_DEATH_TEST
#include <signal.h>
#include <sys/resource.h>
#endif

import std;
import star.algorithm;
import star.celestial_coordinate;
import star.celestial_database;
import star.celestial_parameters;
import star.celestial_types;
import star.either;
import star.file;
import star.format;
import star.game_test;
import star.json;
import star.list;
import star.map;
import star.string;
import star.root;
import star.vector;

using namespace Star;

namespace {
const GameTestRegistration persistence("CelestialDatabase", "PublishesPersistenceOnlyWithBackingStorage", [](GameTest& fixture) {
  auto& root = fixture.root();
  Vec2I const chunk{0, 0};
  CelestialCoordinate const coordinate(Vec3I(1, 1, 1));
  auto directory = File::temporary_directory(root.random());
  ASSERT_TRUE(directory) << directory.error().what();
  auto cleanup = make_scope_exit([&] { EXPECT_TRUE(File::remove_all(*directory)); });
  auto path = File::relative_to(*directory, "universe.chunks");
  auto check = [&](CelestialMasterDatabase& database, bool persisted) {
    auto response = database.respond_to_request(make_left(chunk));
    ASSERT_TRUE(response) << response.error().what();
    ASSERT_TRUE(response->is_left());
    EXPECT_EQ(response->left_ref().persistent, persisted);
  };
  {
    auto memory = CelestialMasterDatabase::create(root);
    ASSERT_TRUE(memory) << memory.error().what();
    ASSERT_TRUE((*memory)->mark_persistent(coordinate));
    check(**memory, false);
  }
  {
    auto disk = CelestialMasterDatabase::create(root, path);
    ASSERT_TRUE(disk) << disk.error().what();
    auto close = make_scope_exit([&] { EXPECT_TRUE((*disk)->close()); });
    for (int pass = 0; pass != 2; ++pass) {
      auto marked = (*disk)->mark_persistent(coordinate);
      ASSERT_TRUE(marked) << marked.error().what();
      check(**disk, true);
    }
  }
  auto reopened = CelestialMasterDatabase::create(root, path);
  ASSERT_TRUE(reopened) << reopened.error().what();
  auto close = make_scope_exit([&] { EXPECT_TRUE((*reopened)->close()); });
  check(**reopened, true);
});

#if defined(STAR_SYSTEM_LINUX) && GTEST_HAS_DEATH_TEST
const GameTestRegistration retry("CelestialDatabase", "RetriesPersistenceAfterFileGrowthFails", [](GameTest& fixture) {
  auto old_style = testing::GTEST_FLAG(death_test_style);
  testing::GTEST_FLAG(death_test_style) = "threadsafe";
  auto restore_style = make_scope_exit([&] { testing::GTEST_FLAG(death_test_style) = old_style; });
  // Re-exec: neither SIGXFSZ nor the soft file size limit affects the test runner.
  auto exercise = [&] {
    auto& root = fixture.root();
    auto directory = File::temporary_directory(root.random());
    ASSERT_TRUE(directory);
    auto cleanup = make_scope_exit([&] { EXPECT_TRUE(File::remove_all(*directory)); });
    auto disk = CelestialMasterDatabase::create(root, File::relative_to(*directory, "retry.chunks"));
    ASSERT_TRUE(disk) << disk.error().what();
    auto close = make_scope_exit([&] { EXPECT_TRUE((*disk)->close()); });
    Vec2I const chunk{0, 0};
    CelestialCoordinate const coord(Vec3I(1, 1, 1));
    auto before = (*disk)->respond_to_request(make_left(chunk));
    ASSERT_TRUE(before);
    if (before->left_ref().persistent)
      GTEST_SKIP() << "Asset configuration persists all generated chunks";
    std::optional<CelestialCoordinate> planet;
    for (auto const& [location, star] : before->left_ref().system_parameters) {
      auto children = (*disk)->children(CelestialCoordinate(location));
      ASSERT_TRUE(children);
      if (!children->empty()) {
        planet = children->front();
        break;
      }
    }
    ASSERT_TRUE(planet);
    auto old_name = (*disk)->name(*planet);
    ASSERT_TRUE(old_name && *old_name);
    auto replacement = CelestialParameters::create(root, *planet, 1, "Updated", JsonObject{});
    ASSERT_TRUE(replacement);
    struct rlimit original{};
    ASSERT_EQ(::getrlimit(RLIMIT_FSIZE, &original), 0);
    auto limited = original;
    limited.rlim_cur = 1;
    ASSERT_NE(::signal(SIGXFSZ, SIG_IGN), SIG_ERR);
    ASSERT_EQ(::setrlimit(RLIMIT_FSIZE, &limited), 0);
    auto failed = (*disk)->mark_persistent(coord);
    auto failed_update = (*disk)->update_parameters(*planet, *replacement);
    ASSERT_EQ(::setrlimit(RLIMIT_FSIZE, &original), 0);
    ASSERT_FALSE(failed);
    EXPECT_FALSE(failed_update);
    auto retained_name = (*disk)->name(*planet);
    ASSERT_TRUE(retained_name);
    EXPECT_EQ(*retained_name, *old_name);
    auto unchanged = (*disk)->respond_to_request(make_left(chunk));
    ASSERT_TRUE(unchanged);
    EXPECT_FALSE(unchanged->left_ref().persistent);
    auto saved = (*disk)->mark_persistent(coord);
    ASSERT_TRUE(saved) << saved.error().what();
    auto committed = (*disk)->respond_to_request(make_left(chunk));
    ASSERT_TRUE(committed);
    EXPECT_TRUE(committed->left_ref().persistent);
    ASSERT_TRUE((*disk)->update_parameters(*planet, *replacement));
    auto updated_name = (*disk)->name(*planet);
    ASSERT_TRUE(updated_name);
    EXPECT_EQ(*updated_name, std::optional<String>("Updated"));
  };
  EXPECT_EXIT((exercise(), std::_Exit(testing::Test::HasFailure() ? 1 : 0)), testing::ExitedWithCode(0), "");
});
#endif
}

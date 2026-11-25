#include "gtest/gtest.h"
#if defined(STAR_SYSTEM_LINUX) && GTEST_HAS_DEATH_TEST
#include <signal.h>
#include <sys/resource.h>
#endif

import std;
import star.algorithm;
import star.casting;
import star.btree_database;
import star.byte_array;
import star.celestial_coordinate;
import star.celestial_database;
import star.celestial_parameters;
import star.celestial_types;
import star.either;
import star.data_stream_devices;
import star.data_stream_extra;
import star.file;
import star.format;
import star.game_test;
import star.io_device;
import star.json;
import star.list;
import star.map;
import star.logging;
import star.rect;
import star.string;
import star.root;
import star.test_universe;
import star.universe_client;
import star.universe_server;
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

const GameTestRegistration replace_incompatible("CelestialDatabase", "ReplacesIncompatibleStorageOnlyOnce", [](GameTest& fixture) {
  auto& root = fixture.root();
  for (auto const& identifier : std::array{String("Celestial1"), String("Celestial2")}) {
    SCOPED_TRACE(identifier.utf8());
    auto directory = File::temporary_directory(root.random());
    ASSERT_TRUE(directory);
    auto cleanup = make_scope_exit([&] { EXPECT_TRUE(File::remove_all(*directory)); });
    auto path = File::relative_to(*directory, "universe.chunks");
    ByteArray const key(4, 'k'), value(8, 'v');
    {
      BTreeDatabase old(root.logger(), identifier, 4);
      auto file = File::open(path, IOMode::ReadWrite);
      ASSERT_TRUE(file);
      ASSERT_TRUE(old.set_io_device(std::move(*file)));
      ASSERT_TRUE(old.open());
      ASSERT_TRUE(old.insert(key, value));
      ASSERT_TRUE(old.close(true));
    }
    {
      auto database = CelestialMasterDatabase::create(root, path);
      ASSERT_TRUE(database) << database.error().what();
      auto close = make_scope_exit([&] { EXPECT_TRUE((*database)->close()); });
      ASSERT_TRUE((*database)->mark_persistent(CelestialCoordinate(Vec3I(1, 1, 1))));
    }
    auto entries = File::directory_entries(*directory);
    ASSERT_TRUE(entries);
    ASSERT_EQ(entries->size(), 2u);
    auto const& backup_name = (*entries)[(*entries)[0].first == "universe.chunks" ? 1 : 0].first;
    ASSERT_TRUE(backup_name.utf8().ends_with(".fail"));
    {
      BTreeDatabase backup(root.logger());
      auto file = File::open(File::relative_to(*directory, backup_name), IOMode::Read);
      ASSERT_TRUE(file);
      ASSERT_TRUE(backup.set_io_device(std::move(*file)));
      ASSERT_TRUE(backup.open());
      EXPECT_EQ(backup.content_identifier(), identifier);
      EXPECT_EQ(backup.key_size(), 4u);
      auto saved = backup.find(key);
      ASSERT_TRUE(saved && *saved);
      EXPECT_EQ(**saved, value);
      ASSERT_TRUE(backup.close(true));
    }
    {
      BTreeDatabase replacement(root.logger());
      auto file = File::open(path, IOMode::Read);
      ASSERT_TRUE(file);
      ASSERT_TRUE(replacement.set_io_device(std::move(*file)));
      ASSERT_TRUE(replacement.open());
      EXPECT_EQ(replacement.content_identifier(), "Celestial2");
      EXPECT_EQ(replacement.key_size(), 32u);
      ASSERT_TRUE(replacement.close(true));
    }
    auto reopened = CelestialMasterDatabase::create(root, path);
    ASSERT_TRUE(reopened) << reopened.error().what();
    auto close = make_scope_exit([&] { EXPECT_TRUE((*reopened)->close()); });
    auto response = (*reopened)->respond_to_request(make_left(Vec2I(0, 0)));
    ASSERT_TRUE(response && response->is_left());
    EXPECT_TRUE(response->left_ref().persistent);
    auto final_entries = File::directory_entries(*directory);
    ASSERT_TRUE(final_entries);
    EXPECT_EQ(final_entries->size(), 2u);
    auto backup_exists = File::exists(File::relative_to(*directory, backup_name));
    ASSERT_TRUE(backup_exists);
    EXPECT_TRUE(*backup_exists);
  }
});

const GameTestRegistration mixed_queue("CelestialDatabase", "ServerDeliversMapDataAfterOneChunkFails", [](GameTest& fixture) {
  auto directory = File::temporary_directory(fixture.root().random());
  ASSERT_TRUE(directory) << directory.error().what();
  auto cleanup = make_scope_exit([&] { EXPECT_TRUE(File::remove_all(*directory)); });
  auto overlay = File::relative_to(*directory, "assets");
  ASSERT_TRUE(File::create_directory(overlay));
  // A client retry must not mask a lost successful response from the first batch.
  ASSERT_TRUE(File::write_file(String(R"([{"op":"replace","path":"/requestTimeout","value":300}])"),
      File::relative_to(overlay, "celestial.config.patch")));
  auto settings = fixture.rootSettings();
  settings.assetSources.push_back(overlay);
  settings.storageDirectory = File::relative_to(*directory, "storage");
  settings.runtimeConfigFile = {};
  settings.logDirectory = {};
  settings.logFile = {};
  settings.includeUGC = false;
  settings.defaultConfiguration = settings.defaultConfiguration.set("clearUniverseFiles", false).value();
  auto root = Root::create(std::move(settings));
  ASSERT_TRUE(root) << root.error().what();

  // Only this record in the test-owned save is unreadable. All requests,
  // including the failing one, are ordinary native client map scans.
  Vec2I const bad_index(7, -11);
  auto chunks = File::relative_to(*directory, "damaged.chunks");
  {
    BTreeSha256Database saved((*root)->logger());
    ASSERT_TRUE(saved.set_content_identifier("Celestial2"));
    auto file = File::open(chunks, IOMode::ReadWrite);
    ASSERT_TRUE(file) << file.error().what();
    ASSERT_TRUE(saved.set_io_device(std::move(*file)));
    ASSERT_TRUE(saved.open());
    auto key = DataStreamBuffer::serialize(bad_index);
    ASSERT_TRUE(key) << key.error().what();
    ASSERT_TRUE(saved.insert(*key, ByteArray(2, 'x')));
    ASSERT_TRUE(saved.close(true));
  }
  auto universe = TestUniverse::create(**root, {100, 100}, false, chunks);
  ASSERT_TRUE(universe) << universe.error().what();
  auto close = make_scope_exit([&] { EXPECT_TRUE((*universe)->close()); });
  struct Diagnostics : LogSink {
    std::atomic<unsigned> dropped = 0;
    std::expected<void, StarException> log(std::string_view message, LogLevel level) override {
      if (message.starts_with("UniverseServer: Dropping celestial request for client ")) {
        EXPECT_EQ(level, LogLevel::Warn);
        ++dropped;
      } else if (level == LogLevel::Error) {
        ADD_FAILURE() << message;
      }
      return {};
    }
  };
  auto diagnostics = std::make_shared<Diagnostics>();
  diagnostics->set_level(LogLevel::Warn);
  (*root)->logger().add_sink(diagnostics);

  auto master = as<CelestialMasterDatabase>((*universe)->server()->celestial_database());
  ASSERT_TRUE(master);
  auto slave = (*universe)->client()->celestial_database();
  int size = master->base_information().chunk_size;
  ASSERT_GT(size, 0);
  ASSERT_LE(size, std::numeric_limits<int>::max() / 11);
  Vec2I origin = bad_index * size;
  RectI batch = RectI::with_size(origin, {3 * size, size});
  RectI good = RectI::with_size(origin + Vec2I(size, 0), {2 * size, size});
  ASSERT_FALSE(slave->scan_region_fully_loaded(batch).value());
  ASSERT_FALSE(slave->scan_region_fully_loaded(good).value());
  auto wait_for = [&](auto&& ready) -> testing::AssertionResult {
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(60);
    do {
      auto updated = (*universe)->update();
      if (!updated)
        return testing::AssertionFailure() << updated.error().what();
      if (ready())
        return testing::AssertionSuccess();
    } while (!testing::Test::HasFailure() && std::chrono::steady_clock::now() < deadline);
    return testing::AssertionFailure() << "Celestial queue did not deliver the expected result";
  };
  ASSERT_TRUE(wait_for([&] { return diagnostics->dropped.load() && slave->scan_region_fully_loaded(good).value(); }));
  auto systems = slave->scan_systems(good).value();
  ASSERT_FALSE(systems.empty());
  EXPECT_TRUE(std::ranges::is_permutation(systems, master->scan_systems(good).value()));

  // The first packet contained summaries only. A later, separate request
  // must still bring real orbital data back through the same player's queue.
  auto const& system = systems.front();
  ASSERT_FALSE(slave->has_children(system).value().has_value());
  ASSERT_TRUE(wait_for([&] { return slave->has_children(system).value().has_value(); }));
  EXPECT_EQ(slave->child_orbits(system).value(), master->child_orbits(system).value());
  EXPECT_EQ(diagnostics->dropped.load(), 1u);
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

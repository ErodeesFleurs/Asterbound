import std;
import star.algorithm;
import star.assets;
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
import star.rect;
import star.root;
import star.string;
import star.vector;

#include "gtest/gtest.h"

using namespace Star;

namespace {
CelestialBaseInformation base_info(int chunk_size = 4) {
  return {3, 2, chunk_size, {-100, 100}, {0, 10}, false};
}

void expect_system_request(CelestialSlaveDatabase& database, Vec3I const& location) {
  auto requests = database.pull_requests();
  ASSERT_EQ(requests.size(), 1u);
  ASSERT_TRUE(requests[0].is_right());
  EXPECT_EQ(requests[0].right_ref(), location);
}

const GameTestRegistration in_flight_requests("CelestialDatabase", "SystemRequestIsDeduplicatedUntilResponse", [](GameTest& fixture) {
  auto assets = fixture.root().assets();
  ASSERT_TRUE(assets) << assets.error().what();
  auto database = CelestialSlaveDatabase::create(*assets, fixture.root().random(), base_info());
  ASSERT_TRUE(database) << database.error().what();
  auto& slave = **database;
  Vec3I location{1, -1, 7};
  CelestialCoordinate system(location);
  CelestialChunk chunk;
  chunk.chunk_index = {0, -1};
  ASSERT_TRUE(slave.push_responses({make_left(chunk)}));

  ASSERT_TRUE(slave.signal_system(system));
  ASSERT_NO_FATAL_FAILURE(expect_system_request(slave, location));
  auto orbits = slave.child_orbits(system);
  ASSERT_TRUE(orbits);
  EXPECT_TRUE(orbits->empty());
  ASSERT_TRUE(slave.signal_system(system));
  EXPECT_TRUE(slave.pull_requests().empty());

  ASSERT_TRUE(slave.push_responses({make_right(CelestialSystemObjects{location, {}})}));
  auto children = slave.has_children(system);
  ASSERT_TRUE(children);
  EXPECT_EQ(*children, std::optional<bool>(false));
  EXPECT_TRUE(slave.pull_requests().empty());
  // A fresh summary lacks details. The previous response cleared its pending timer.
  ASSERT_TRUE(slave.push_responses({make_left(std::move(chunk))}));
  ASSERT_TRUE(slave.signal_system(system));
  ASSERT_NO_FATAL_FAILURE(expect_system_request(slave, location));
});

const GameTestRegistration timeout_reregistration("CelestialDatabase", "ExpiredSystemRequestCanBeRegisteredAgain", [](GameTest& fixture) {
  auto& root = fixture.root();
  auto directory = File::temporary_directory(root.random());
  ASSERT_TRUE(directory) << directory.error().what();
  auto cleanup = make_scope_exit([&] { EXPECT_TRUE(File::remove_all(*directory)); });
  ASSERT_TRUE(File::write_file(String("[]"), File::relative_to(*directory, "preload.config")));
  ASSERT_TRUE(File::write_file(String(R"({"requestTimeout":0})"), File::relative_to(*directory, "celestial.config")));
  Assets::Settings settings{};
  settings.worker_pool_size = 1;
  auto assets = Assets::create(root.logger(), root.random(), root.logMap(), settings, {*directory});
  ASSERT_TRUE(assets) << assets.error().what();
  auto database = CelestialSlaveDatabase::create(*assets, root.random(), base_info());
  ASSERT_TRUE(database) << database.error().what();
  auto& slave = **database;
  Vec3I location{1, -1, 7};
  CelestialChunk chunk;
  chunk.chunk_index = {0, -1};
  ASSERT_TRUE(slave.push_responses({make_left(std::move(chunk))}));
  ASSERT_TRUE(slave.signal_system(CelestialCoordinate(location)));
  ASSERT_NO_FATAL_FAILURE(expect_system_request(slave, location));
  EXPECT_TRUE(slave.pull_requests().empty()); // Reap the zero-duration in-flight timer.
  ASSERT_TRUE(slave.signal_system(CelestialCoordinate(location)));
  ASSERT_NO_FATAL_FAILURE(expect_system_request(slave, location));
});

const GameTestRegistration region_prefetch("CelestialDatabase", "EachScanReturnsCachedDataAndRequestsEveryMissingChunk", [](GameTest& fixture) {
  auto assets = fixture.root().assets();
  ASSERT_TRUE(assets) << assets.error().what();
  Vec3I included{-3, -3, 1};
  RectI region(-3, -3, 3, 3);
  for (int scan = 0; scan < 4; ++scan) {
    SCOPED_TRACE(scan);
    auto database = CelestialSlaveDatabase::create(*assets, fixture.root().random(), base_info());
    ASSERT_TRUE(database) << database.error().what();
    auto& slave = **database;
    CelestialChunk cached;
    cached.chunk_index = {-1, -1};
    cached.system_parameters[included] = CelestialParameters();
    cached.constellations = {{{{-3, -3}, {-1, -1}}}};
    ASSERT_TRUE(slave.push_responses({make_left(std::move(cached))}));
    if (scan == 0) {
      auto loaded = slave.scan_region_fully_loaded(region);
      ASSERT_TRUE(loaded);
      EXPECT_FALSE(*loaded);
    } else if (scan == 1) {
      auto systems = slave.scan_systems(region);
      ASSERT_TRUE(systems);
      EXPECT_EQ(*systems, List<CelestialCoordinate>{CelestialCoordinate(included)});
    } else if (scan == 2) {
      auto lines = slave.scan_constellation_lines(region);
      ASSERT_TRUE(lines);
      ASSERT_EQ(lines->size(), 1u);
      EXPECT_EQ((*lines)[0].first, Vec2I(-3, -3));
      EXPECT_EQ((*lines)[0].second, Vec2I(-1, -1));
    } else {
      ASSERT_TRUE(slave.signal_region(region));
    }
    std::set<std::pair<int, int>> pending;
    auto requests = slave.pull_requests();
    ASSERT_EQ(requests.size(), 3u);
    for (auto const& request : requests) {
      ASSERT_TRUE(request.is_left());
      auto index = request.left_ref();
      pending.emplace(index[0], index[1]);
      CelestialChunk chunk;
      chunk.chunk_index = index;
      if (index == Vec2I(0, 0)) {
        chunk.system_parameters[Vec3I(3, 1, 2)] = CelestialParameters();
        chunk.system_parameters[Vec3I(1, 3, 2)] = CelestialParameters();
      }
      ASSERT_TRUE(slave.push_responses({make_left(std::move(chunk))}));
    }
    EXPECT_EQ(pending, (std::set<std::pair<int, int>>{{-1, 0}, {0, -1}, {0, 0}}));
    auto loaded = slave.scan_region_fully_loaded(region);
    ASSERT_TRUE(loaded);
    EXPECT_TRUE(*loaded);
    auto systems = slave.scan_systems(region);
    ASSERT_TRUE(systems);
    std::set<CelestialCoordinate> expected{CelestialCoordinate(included), CelestialCoordinate(Vec3I(3, 1, 2)), CelestialCoordinate(Vec3I(1, 3, 2))};
    EXPECT_EQ((std::set<CelestialCoordinate>(systems->begin(), systems->end())), expected);
    EXPECT_TRUE(slave.pull_requests().empty());
  }
});

const GameTestRegistration extreme_geometry("CelestialDatabase", "ChunkGeometryIsHalfOpenAndUsesSafeFloorDivision", [](GameTest& fixture) {
  auto assets = fixture.root().assets();
  ASSERT_TRUE(assets) << assets.error().what();
  constexpr int low = std::numeric_limits<int>::min(), high = std::numeric_limits<int>::max();
  auto database = CelestialSlaveDatabase::create(*assets, fixture.root().random(), base_info(3));
  ASSERT_TRUE(database) << database.error().what();
  auto& slave = **database;
  ASSERT_TRUE(slave.signal_region(RectI(low, -3, low + 1, 0)));
  ASSERT_TRUE(slave.signal_region(RectI(0, 0, 0, high)));
  ASSERT_TRUE(slave.signal_system(CelestialCoordinate(Vec3I(high, low, 1))));
  auto requests = slave.pull_requests();
  ASSERT_EQ(requests.size(), 2u);
  std::set<std::pair<int, int>> indexes;
  for (auto const& request : requests) {
    ASSERT_TRUE(request.is_left());
    auto index = request.left_ref();
    indexes.emplace(index[0], index[1]);
  }
  EXPECT_EQ(indexes, (std::set<std::pair<int, int>>{{-715827883, -1}, {715827882, -715827883}}));

  auto master = CelestialMasterDatabase::create(fixture.root());
  ASSERT_TRUE(master) << master.error().what();
  EXPECT_FALSE((*master)->respond_to_request(make_left(Vec2I(high, high))));
  ASSERT_TRUE((*master)->close());
});

const GameTestRegistration invalid_geometry("CelestialDatabase", "RejectsInvalidGeometryOnBothSides", [](GameTest& fixture) {
  auto assets = fixture.root().assets();
  ASSERT_TRUE(assets) << assets.error().what();
  auto config = (*assets)->json("/celestial.config");
  ASSERT_TRUE(config) << config.error().what();
  auto directory = File::temporary_directory(fixture.root().random());
  ASSERT_TRUE(directory) << directory.error().what();
  auto cleanup = make_scope_exit([&] { EXPECT_TRUE(File::remove_all(*directory)); });
  auto overlay = File::relative_to(*directory, "assets");
  ASSERT_TRUE(File::create_directory(overlay));
  ASSERT_TRUE(File::write_file(String("[]"), File::relative_to(overlay, "preload.config")));
  for (auto [size, z] : std::array{std::pair{0, Vec2I(0, 1)}, std::pair{-1, Vec2I(0, 1)},
                                  std::pair{4, Vec2I(3, 3)}, std::pair{4, Vec2I(8, 3)}}) {
    auto info = base_info(size);
    info.z_coord_range = z;
    auto slave = CelestialSlaveDatabase::create(*assets, fixture.root().random(), info);
    EXPECT_FALSE(slave);
    auto const field = size <= 0 ? String("chunkSize") : String("zCoordRange");
    auto changed = config->set(field, size <= 0 ? Json(size) : Json(JsonArray{z[0], z[1]}));
    ASSERT_TRUE(changed);
    auto text = changed->repr();
    ASSERT_TRUE(text);
    ASSERT_TRUE(File::write_file(*text, File::relative_to(overlay, "celestial.config")));
    auto settings = fixture.rootSettings();
    settings.assetDirectories.clear();
    settings.assetSources = {overlay};
    settings.storageDirectory = File::relative_to(*directory, "storage");
    settings.runtimeConfigFile = {};
    settings.logDirectory = {};
    settings.logFile = {};
    settings.includeUGC = false;
    settings.assetsSettings.worker_pool_size = 1;
    auto root = Root::create(std::move(settings));
    ASSERT_TRUE(root) << root.error().what();
    auto master = CelestialMasterDatabase::create(**root);
    ASSERT_FALSE(master);
    EXPECT_NE(std::string_view(master.error().what()).find(field.utf8()), std::string_view::npos) << master.error().what();
  }
});
}

import std;
import star.algorithm;
import star.assets;
import star.cellular_liquid;
import star.dungeon_generator;
import star.map;
import star.format;
import star.file;
import star.game_test;
import star.hash;
import star.liquids_database;
import star.json;
import star.logging;
import star.rect;
import star.root;
import star.vector;
import star.string;
import star.liquid_types;
#include "gtest/gtest.h"

using namespace Star;

TEST(DungeonLiquidTest, PressureTracksHighestConnectedCellOfSameLiquid) {
  Map<Vec2I, LiquidStore> liquids;
  auto add = [&](int x, int y, LiquidId id) {
    liquids[{x, y}] = LiquidStore::filled(id, 0.5f);
  };
  add(0, -2, 1);
  add(1, -2, 1);
  add(1, -1, 1);
  liquids[{1, 0}] = LiquidStore::endless(1, 9.0f);
  add(0, -1, 2); // Touches water; its pressure must be independent.
  add(2, 1, 1);  // Diagonal contact is not connected.
  ASSERT_TRUE(Dungeon::pressurize_liquids(liquids));
  auto cell = [&](int x, int y) -> LiquidStore const& { return liquids[{x, y}]; };
  EXPECT_EQ(cell(0, -2).pressure, 3.0f);
  EXPECT_EQ(cell(1, -2).pressure, 3.0f);
  EXPECT_EQ(cell(1, -1).pressure, 2.0f);
  EXPECT_EQ(cell(1, 0).pressure, 1.0f);
  EXPECT_TRUE(cell(1, 0).source);
  EXPECT_EQ(cell(0, -1).pressure, 1.0f);
  EXPECT_EQ(cell(2, 1).pressure, 1.0f);
  EXPECT_EQ(cell(0, -2).level, 0.5f);
}

TEST(LiquidStoreTest, OverfullLevelSaturatesNetworkUpdates) {
  auto store = LiquidStore::filled(1, 0.5f);
  auto update = store.update(1, 1.1f, 3.0f);
  ASSERT_TRUE(update);
  EXPECT_EQ(update->liquid, 1);
  EXPECT_EQ(update->level, 255);
  EXPECT_EQ(store.netUpdate().level, 255);
  EXPECT_FLOAT_EQ(store.level, 1.1f);
  EXPECT_FLOAT_EQ(store.pressure, 3.0f);

  // Both overfull levels have the same wire value; only physics changes.
  EXPECT_FALSE(store.update(1, 1.3f, 4.0f));
  EXPECT_FLOAT_EQ(store.level, 1.3f);
  EXPECT_FLOAT_EQ(store.pressure, 4.0f);
}

namespace {
enum class TestLiquidId : std::uint8_t { Water, Lava };
struct CustomLiquidId {
  explicit CustomLiquidId(unsigned value) : value(value) {}
  bool operator==(CustomLiquidId const&) const = default;
  unsigned value;
};
}

template <>
struct Star::ValueHash<CustomLiquidId> {
  std::size_t operator()(CustomLiquidId const& id) const noexcept { return id.value; }
};

namespace {
struct UnhashableLiquidId {
  bool operator==(UnhashableLiquidId const&) const = default;
};

template <typename Id>
concept accepts_liquid_id = requires { typename LiquidCellEngine<Id>; };

static_assert(accepts_liquid_id<LiquidId>);
static_assert(accepts_liquid_id<TestLiquidId>);
static_assert(accepts_liquid_id<CustomLiquidId>);
static_assert(!accepts_liquid_id<UnhashableLiquidId>);

const GameTestRegistration wrapped_wall_coordinates("LiquidEngine", "WrappedWallKeepsBothCollisionCoordinates", [](GameTest&) {
  struct WrappedWorld final : CellularLiquidWorld<TestLiquidId> {
    std::set<std::pair<int, int>> collisions;

    Vec2I unique_location(Vec2I const& position) const override {
      return {position[0] & 1, position[1]};
    }
    std::expected<CellularLiquidCell<TestLiquidId>, StarException> cell(Vec2I const& position) const override {
      if (position == Vec2I(0, 0))
        return CellularLiquidSourceCell<TestLiquidId>{TestLiquidId::Water, 1.0f};
      return CellularLiquidCollisionCell{};
    }
    std::expected<void, StarException> set_flow(Vec2I const&, CellularLiquidFlowCell<TestLiquidId> const&) override {
      return std::unexpected(StarException("Cannot write flow into a source or wall"));
    }
    std::expected<void, StarException> liquid_collision(Vec2I const&, TestLiquidId, Vec2I const& wall) override {
      collisions.emplace(wall[0], wall[1]);
      return {};
    }
  };
  auto world = std::make_shared<WrappedWorld>();
  LiquidCellEngine<TestLiquidId> engine(43, {}, world);
  engine.visit_location({0, 0});
  ASSERT_TRUE(engine.update());
  ASSERT_TRUE(engine.update());
  std::set<std::pair<int, int>> const expected{{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
  EXPECT_EQ(world->collisions, expected);
});

struct LiquidCallbackFailureTag {
  static constexpr std::string_view name() { return "LiquidCallbackFailure"; }
};
using LiquidCallbackFailure = StarError<LiquidCallbackFailureTag, StarException>;
struct LiquidReadbackFailureTag {
  static constexpr std::string_view name() { return "LiquidReadbackFailure"; }
};
using LiquidReadbackFailure = StarError<LiquidReadbackFailureTag, StarException>;

std::expected<std::shared_ptr<LiquidsDatabase>, StarException>
liquids_fixture(GameTest& fixture, String const& directory, Json const& config) {
  auto created = File::create_directories(directory);
  if (!created)
    return std::unexpected(std::move(created.error()));
  auto preload = File::write_file(String("[]"), File::relative_to(directory, "preload.config"));
  if (!preload)
    return std::unexpected(std::move(preload.error()));
  auto contents = config.print_json();
  if (!contents)
    return std::unexpected(std::move(contents.error()));
  auto written = File::write_file(*contents, File::relative_to(directory, "liquids.config"));
  if (!written)
    return std::unexpected(std::move(written.error()));
  Assets::Settings settings{};
  settings.worker_pool_size = 1;
  settings.asset_time_to_live = 10;
  Logger logger;
  auto assets = Assets::create(logger, fixture.root().random(), fixture.root().logMap(), settings, {directory});
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  auto materials = fixture.root().materialDatabase();
  if (!materials)
    return std::unexpected(std::move(materials.error()));
  return LiquidsDatabase::create(*assets, *materials);
}

const GameTestRegistration
  liquid_engine_config("LiquidsDatabaseTest", "RequiredFloatFieldsUseAssetConfiguration", [](GameTest& fixture) {
    auto directory = File::temporary_directory(fixture.root().random());
    ASSERT_TRUE(directory);
    auto cleanup = make_scope_exit([&] { EXPECT_TRUE(File::remove_all(*directory)); });
    Json engine = JsonObject{{"lateralMoveFactor", 1},
                             {"spreadOverfillUpFactor", 2},
                             {"spreadOverfillLateralFactor", 3},
                             {"spreadOverfillDownFactor", 4},
                             {"pressureEqualizeFactor", 5},
                             {"pressureMoveFactor", 6},
                             {"maximumPressureLevelImbalance", 7},
                             {"minimumLivenPressureChange", 8},
                             {"minimumLivenLevelChange", 9},
                             {"minimumLiquidLevel", 10},
                             {"interactTransformationLevel", 11}};
    auto valid = liquids_fixture(fixture, File::relative_to(*directory, "valid"),
                                JsonObject{{"liquidEngineParameters", engine}, {"backgroundDrain", 12}});
    ASSERT_TRUE(valid) << valid.error().what();
    auto parameters = (*valid)->liquid_engine_parameters();
    EXPECT_FLOAT_EQ(parameters.lateral_move_factor, 1);
    EXPECT_FLOAT_EQ(parameters.spread_overfill_up_factor, 2);
    EXPECT_FLOAT_EQ(parameters.spread_overfill_lateral_factor, 3);
    EXPECT_FLOAT_EQ(parameters.spread_overfill_down_factor, 4);
    EXPECT_FLOAT_EQ(parameters.pressure_equalize_factor, 5);
    EXPECT_FLOAT_EQ(parameters.pressure_move_factor, 6);
    EXPECT_FLOAT_EQ(parameters.maximum_pressure_level_imbalance, 7);
    EXPECT_FLOAT_EQ(parameters.minimum_liven_pressure_change, 8);
    EXPECT_FLOAT_EQ(parameters.minimum_liven_level_change, 9);
    EXPECT_FLOAT_EQ(parameters.minimum_liquid_level, 10);
    EXPECT_FLOAT_EQ(parameters.interact_transformation_level, 11);
    EXPECT_FLOAT_EQ((*valid)->backgroundDrain(), 12);

    constexpr std::string_view fields[] = {"lateralMoveFactor",
                                           "spreadOverfillUpFactor",
                                           "spreadOverfillLateralFactor",
                                           "spreadOverfillDownFactor",
                                           "pressureEqualizeFactor",
                                           "pressureMoveFactor",
                                           "maximumPressureLevelImbalance",
                                           "minimumLivenPressureChange",
                                           "minimumLivenLevelChange",
                                           "minimumLiquidLevel",
                                           "interactTransformationLevel"};
    unsigned index = 0;
    for (auto name : fields) {
      SCOPED_TRACE(name);
      for (bool missing : {true, false}) {
        SCOPED_TRACE(missing);
        auto changed = missing ? engine.erase(name) : engine.set(name, "not a float");
        ASSERT_TRUE(changed);
        auto loaded = liquids_fixture(fixture, File::relative_to(*directory, Star::format("case{}", index++)),
                                     JsonObject{{"liquidEngineParameters", *changed}, {"backgroundDrain", 12}});
        ASSERT_FALSE(loaded);
        EXPECT_TRUE(loaded.error().is<JsonException>());
      }
    }

    for (bool missing : {true, false}) {
      Json config = missing ? Json(JsonObject{{"liquidEngineParameters", engine}})
                            : Json(JsonObject{{"liquidEngineParameters", engine}, {"backgroundDrain", false}});
      auto outer = liquids_fixture(
        fixture, File::relative_to(*directory, missing ? String("outerMissing") : String("outerType")), config);
      ASSERT_FALSE(outer);
      EXPECT_TRUE(outer.error().is<JsonException>());
    }
  });

template <int basin_width, int basin_height>
class SizedLiquidBasin : public CellularLiquidWorld<LiquidId> {
public:
  static constexpr int width = basin_width;
  static constexpr int height = basin_height;
  std::array<CellularLiquidFlowCell<LiquidId>, width * height> cells{};
  mutable bool fail_next_read = false;

  Vec2I unique_location(Vec2I const& position) const override {
    return {(position[0] % width + width) % width, position[1]};
  }

  std::expected<CellularLiquidCell<LiquidId>, StarException> cell(Vec2I const& position) const override {
    if (std::exchange(fail_next_read, false))
      return std::unexpected(StarException("Transient basin read failure"));
    auto point = unique_location(position);
    if (point[1] < 0 || point[1] >= height)
      return CellularLiquidCollisionCell{};
    return cells[point[1] * width + point[0]];
  }

  std::expected<void, StarException> set_flow(Vec2I const& position, CellularLiquidFlowCell<LiquidId> const& flow) override {
    auto point = unique_location(position);
    if (point[1] < 0 || point[1] >= height)
      return std::unexpected(StarException("Liquid flow attempted to overwrite basin boundary"));
    cells[point[1] * width + point[0]] = flow;
    return {};
  }

  float mass() const {
    float total = 0;
    for (auto const& cell : cells)
      total += cell.level;
    return total;
  }
};
using LiquidBasin = SizedLiquidBasin<5, 3>;

const GameTestRegistration liquid_borrow_recovery("LiquidEngine", "WrappedFlowConservesMassAfterReadFailure", [](GameTest& fixture) {
  auto database = fixture.root().liquidsDatabase();
  ASSERT_TRUE(database) << database.error().what();
  auto water = (*database)->liquidId("water");
  ASSERT_TRUE(water) << water.error().what();
  auto parameters = (*database)->liquid_engine_parameters();
  // A closed basin has no drain or sub-threshold evaporation.
  parameters.minimum_liquid_level = 0;
  auto basin = std::make_shared<LiquidBasin>();
  basin->cells[2 * LiquidBasin::width] = {*water, 2.0f, 0.0f};
  LiquidCellEngine<LiquidId> engine(43, parameters, basin);
  engine.visit_location({0, 2});
  ASSERT_TRUE(engine.update());
  basin->fail_next_read = true;
  auto failed = engine.update();
  ASSERT_FALSE(failed);
  EXPECT_FLOAT_EQ(basin->mass(), 2.0f);

  engine.visit_location({0, 2});
  for (unsigned tick = 0; tick < 64; ++tick) {
    auto updated = engine.update();
    ASSERT_TRUE(updated) << updated.error().what();
    EXPECT_NEAR(basin->mass(), 2.0f, 0.0001f);
  }
  float bottom_mass = 0;
  for (int x = 0; x < LiquidBasin::width; ++x)
    bottom_mass += basin->cells[x].level;
  EXPECT_GT(bottom_mass, 0.5f);
  EXPECT_GT(basin->cells[LiquidBasin::width - 1].level, 0.01f);
});

class CallbackLiquidBasin : public CellularLiquidWorld<LiquidId> {
public:
  LiquidId water;
  LiquidId lava;
  std::optional<char> failure;
  std::vector<char> calls;
  unsigned source_writes = 0;
  std::array<CellularLiquidFlowCell<LiquidId>, 2> flows;
  std::vector<Vec2I> committed_writes;
  std::function<void()> interaction;

  CallbackLiquidBasin(LiquidId water, LiquidId lava)
    : water(water), lava(lava), flows{{{water, 0.5f, 0}, {lava, 0.5f, 0}}} {}

  std::expected<CellularLiquidCell<LiquidId>, StarException> cell(Vec2I const& p) const override {
    if (p == Vec2I(0, 0))
      return CellularLiquidSourceCell<LiquidId>{water, 0};
    if (p[1] == 0 && p[0] >= 1 && p[0] <= 2)
      return flows[p[0] - 1];
    return CellularLiquidCollisionCell{};
  }

  std::expected<void, StarException> record(char kind) {
    calls.push_back(kind);
    if (failure == kind && (kind != 'w' || !committed_writes.empty()))
      return std::unexpected(LiquidCallbackFailure(Star::format("Injected liquid callback {}", kind)));
    return {};
  }

  std::expected<void, StarException> set_flow(Vec2I const& p,
      CellularLiquidFlowCell<LiquidId> const& flow) override {
    if (p == Vec2I(0, 0)) {
      ++source_writes;
      return std::unexpected(StarException("Source cell was written as flow"));
    }
    auto recorded = record('w');
    if (!recorded)
      return recorded;
    flows[p[0] - 1] = flow;
    committed_writes.push_back(p);
    return {};
  }

  std::expected<void, StarException> liquid_interaction(Vec2I const&, LiquidId, Vec2I const&, LiquidId) override {
    auto recorded = record('i');
    if (recorded && interaction)
      interaction();
    return recorded;
  }

  std::expected<void, StarException> liquid_collision(Vec2I const&, LiquidId, Vec2I const&) override {
    return record('c');
  }
};

const GameTestRegistration liquid_callback_recovery(
    "LiquidEngine", "PhaseStorageSurvivesSourceAndCallbackFailures", [](GameTest& fixture) {
  auto database = fixture.root().liquidsDatabase();
  ASSERT_TRUE(database) << database.error().what();
  auto water = (*database)->liquidId("water");
  ASSERT_TRUE(water) << water.error().what();
  auto lava = (*database)->liquidId("lava");
  ASSERT_TRUE(lava) << lava.error().what();
  for (char failure : {'w', 'i', 'c'}) {
    SCOPED_TRACE(failure);
    auto basin = std::make_shared<CallbackLiquidBasin>(*water, *lava);
    LiquidCellEngine<LiquidId> engine(43, {}, basin);
    engine.visit_region(RectI(0, 0, 3, 1));
    ASSERT_TRUE(engine.update());
    basin->failure = failure;
    auto failed = engine.update();
    ASSERT_FALSE(failed);
    EXPECT_TRUE(failed.error().is<LiquidCallbackFailure>());
    ASSERT_FALSE(basin->calls.empty());
    EXPECT_EQ(basin->calls.back(), failure);
    if (failure == 'w') {
      EXPECT_EQ(basin->calls.size(), 2u);
      EXPECT_EQ(basin->committed_writes.size(), 1u);
      EXPECT_FLOAT_EQ(basin->flows[basin->committed_writes.front()[0] - 1].level, 0.5f);
      auto committed = basin->committed_writes.front()[0] - 1;
      EXPECT_FLOAT_EQ(basin->flows[committed].pressure, 0.5f);
      EXPECT_FLOAT_EQ(basin->flows[1 - committed].pressure, 0.0f);
    } else {
      EXPECT_EQ(std::count(basin->calls.begin(), basin->calls.end(), failure), 1);
      EXPECT_EQ(std::count(basin->calls.begin(), basin->calls.end(), 'w'), 2);
      if (failure == 'i')
        EXPECT_EQ(std::count(basin->calls.begin(), basin->calls.end(), 'c'), 0);
    }
    basin->failure.reset();
    // Exercise repeated fresh activation/simulation phases after all three
    // early returns, with source-state neighbors borrowing arena-backed cells.
    for (unsigned tick = 0; tick < 128; ++tick) {
      basin->calls.clear();
      engine.visit_region(RectI(0, 0, 3, 1));
      ASSERT_TRUE(engine.update());
    }
    EXPECT_EQ(basin->source_writes, 0u);
  }
  auto basin = std::make_shared<CallbackLiquidBasin>(*water, *lava);
  LiquidCellEngine<LiquidId> engine(43, {}, basin);
  engine.visit_region(RectI(0, 0, 3, 1));
  ASSERT_TRUE(engine.update());
  basin->interaction = [&] {
    basin->flows[1].liquid = *water;
    engine.visit_location({2, 0});
  };
  ASSERT_TRUE(engine.update());
  // The pressure correction also revisits x=1 and its source neighbor x=0.
  EXPECT_EQ(engine.active_cells(*water), 3u);
  EXPECT_EQ(engine.active_cells(*lava), 0u);
  EXPECT_EQ(basin->source_writes, 0u);

  auto emitting = std::make_shared<CallbackLiquidBasin>(*water, *lava);
  LiquidCellEngineParameters source_parameters{};
  source_parameters.lateral_move_factor = 0.5f;
  LiquidCellEngine<LiquidId> source_engine(43, source_parameters, emitting);
  source_engine.visit_region(RectI(0, 0, 3, 1));
  ASSERT_TRUE(source_engine.update());
  ASSERT_TRUE(source_engine.update());
  EXPECT_FLOAT_EQ(emitting->flows[0].level, 0.75f);
  EXPECT_FLOAT_EQ(emitting->flows[1].level, 0.5f);
  EXPECT_EQ(emitting->source_writes, 0u);
});

class BudgetLiquidBasin : public CellularLiquidWorld<LiquidId> {
public:
  static constexpr int count = 16;
  std::array<CellularLiquidFlowCell<LiquidId>, count> cells{};
  mutable bool fail_next_read = false;
  mutable std::optional<unsigned> reads_before_failure;

  Vec2I unique_location(Vec2I const& position) const override {
    return {(position[0] % (count * 2) + count * 2) % (count * 2), position[1]};
  }

  std::expected<CellularLiquidCell<LiquidId>, StarException> cell(Vec2I const& position) const override {
    if (std::exchange(fail_next_read, false))
      return std::unexpected(StarException("Transient budget basin read failure"));
    if (reads_before_failure) {
      if (*reads_before_failure == 0) {
        reads_before_failure.reset();
        return std::unexpected(StarException("Delayed budget basin read failure"));
      }
      --*reads_before_failure;
    }
    auto point = unique_location(position);
    if (point[1] != 0 || point[0] % 2 != 0)
      return CellularLiquidCollisionCell{};
    return cells[point[0] / 2];
  }

  std::expected<void, StarException> set_flow(Vec2I const& position,
                                             CellularLiquidFlowCell<LiquidId> const& flow) override {
    auto point = unique_location(position);
    if (point[1] != 0 || point[0] % 2 != 0)
      return std::unexpected(StarException("Budget basin boundary overwritten"));
    cells[point[0] / 2] = flow;
    return {};
  }
};

const GameTestRegistration
  liquid_budget_ordering("LiquidEngine", "UnlimitedCellsAfterBudgetAndOverlappingRegions", [](GameTest& fixture) {
    auto database = fixture.root().liquidsDatabase();
    ASSERT_TRUE(database) << database.error().what();
    auto water = (*database)->liquidId("water");
    ASSERT_TRUE(water) << water.error().what();
    auto lava = (*database)->liquidId("lava");
    ASSERT_TRUE(lava) << lava.error().what();
    // Isolated cells have no flow or pressure changes, so processing a cell
    // retires it. Active selection is observable without solver instrumentation.
    LiquidCellEngineParameters parameters{};
    auto basin = std::make_shared<BudgetLiquidBasin>();
    LiquidCellEngine<LiquidId> engine(43, parameters, basin);
    engine.set_processing_limit(0);
    for (int i = 0; i < BudgetLiquidBasin::count; ++i) {
      basin->cells[i] = {i % 2 ? *lava : *water, 0.5f, 0.5f};
      // Publish separately to establish a deterministic order in each liquid's
      // activation queue, independent of the visited-location hash set.
      engine.visit_location({i * 2, 0});
      ASSERT_TRUE(engine.update());
    }
    ASSERT_EQ(engine.active_cells(), basin->cells.size());
    engine.set_processing_limit(2);
    // Engine membership uses Rect::contains (inclusive max), not belongs.
    // End at odd x=29 so x=28 is unlimited but x=30 remains ordinary.
    engine.set_no_processing_limit_regions({RectI(20, 0, 28, 1), RectI(24, 0, 29, 1)});
    basin->fail_next_read = true;
    auto failed = engine.update();
    ASSERT_FALSE(failed);
    ASSERT_EQ(engine.active_cells(), basin->cells.size());
    ASSERT_TRUE(engine.update());
    for (int i = 0; i < BudgetLiquidBasin::count; ++i) {
      bool unlimited = i >= 10 && i <= 14;
      EXPECT_EQ(engine.is_active({i * 2, 0}), !(i < 4 || unlimited)) << i;
      EXPECT_FLOAT_EQ(basin->cells[i].level, 0.5f);
      EXPECT_FLOAT_EQ(basin->cells[i].pressure, 0.5f);
      EXPECT_EQ(basin->cells[i].liquid, i % 2 ? *lava : *water);
    }
    EXPECT_EQ(engine.active_cells(), 7u);

    // Keep the same windows while retired cells become active again. The newly
    // unlimited cell must run with a zero ordinary budget; the newly ordinary
    // cell must stay behind the previously queued ordinary cells.
    engine.set_processing_limit(0);
    engine.visit_location({24, 0});
    engine.visit_location({0, 0});
    ASSERT_TRUE(engine.update());
    EXPECT_TRUE(engine.is_active({24, 0}));
    EXPECT_TRUE(engine.is_active({0, 0}));
    EXPECT_EQ(engine.active_cells(), 9u);
    basin->fail_next_read = true;
    ASSERT_FALSE(engine.update());
    EXPECT_EQ(engine.active_cells(), 9u);
    ASSERT_TRUE(engine.update());
    EXPECT_FALSE(engine.is_active({24, 0}));
    EXPECT_TRUE(engine.is_active({0, 0}));
    EXPECT_EQ(engine.active_cells(), 8u);
    engine.set_processing_limit(1);
    ASSERT_TRUE(engine.update());
    EXPECT_FALSE(engine.is_active({8, 0}));
    EXPECT_FALSE(engine.is_active({10, 0}));
    EXPECT_TRUE(engine.is_active({0, 0}));
    EXPECT_EQ(engine.active_cells(), 6u);

    // Replacing the monitored windows must expose former unlimited cells to the
    // ordinary budget and include newly monitored cells without stale indexing.
    engine.set_processing_limit(0);
    engine.set_no_processing_limit_regions({RectI(30, 0, 32, 1)});
    ASSERT_TRUE(engine.update());
    EXPECT_FALSE(engine.is_active({30, 0}));
    EXPECT_TRUE(engine.is_active({12, 0}));
    EXPECT_TRUE(engine.is_active({18, 0}));
    EXPECT_EQ(engine.active_cells(), 5u);
    engine.set_processing_limit({});
    engine.set_liquid_tick_delta(*lava, 0);
    ASSERT_TRUE(engine.update());
    EXPECT_EQ(engine.active_cells(*water), 0u);
    EXPECT_EQ(engine.active_cells(*lava), 2u);
    engine.set_liquid_tick_delta(*lava, 1);
    ASSERT_TRUE(engine.update());
    EXPECT_EQ(engine.active_cells(), 0u);
  });

const GameTestRegistration
  liquid_partial_read_recovery("LiquidEngine", "LateReadFailurePreservesRetirementAndBudgetOrder", [](GameTest& fixture) {
    auto database = fixture.root().liquidsDatabase();
    ASSERT_TRUE(database) << database.error().what();
    auto water = (*database)->liquidId("water");
    ASSERT_TRUE(water) << water.error().what();
    // Fail inside the ordinary prefix, then at two different positions in the
    // unlimited suffix after the ordinary budget has already been exhausted.
    for (unsigned successful_reads : {1u, 3u, 5u}) {
      SCOPED_TRACE(successful_reads);
      LiquidCellEngineParameters parameters{};
      auto basin = std::make_shared<BudgetLiquidBasin>();
      LiquidCellEngine<LiquidId> engine(43, parameters, basin);
      engine.set_processing_limit(0);
      for (int i = 0; i < BudgetLiquidBasin::count; ++i) {
        basin->cells[i] = {*water, 0.5f, 0.5f};
        engine.visit_location({i * 2, 0});
        ASSERT_TRUE(engine.update());
      }
      engine.set_processing_limit(2);
      engine.set_no_processing_limit_regions({RectI(20, 0, 28, 1), RectI(24, 0, 29, 1)});
      basin->reads_before_failure = successful_reads;
      auto failed = engine.update();
      ASSERT_FALSE(failed);
      // The environment fails during selected-cell reads, before finalization.
      // Successful selections have already retired; the failed cell and all
      // later selections must remain active for recovery.
      constexpr std::array<int, 7> selected = {0, 1, 10, 11, 12, 13, 14};
      for (int i = 0; i < BudgetLiquidBasin::count; ++i) {
        bool retired =
          std::find(selected.begin(), selected.begin() + successful_reads, i) != selected.begin() + successful_reads;
        EXPECT_EQ(engine.is_active({i * 2, 0}), !retired) << i;
      }
      auto expect_conserved_basin = [&] {
        float mass = 0;
        for (auto const& cell : basin->cells) {
          EXPECT_EQ(cell.liquid, *water);
          EXPECT_FLOAT_EQ(cell.level, 0.5f);
          EXPECT_FLOAT_EQ(cell.pressure, 0.5f);
          mass += cell.level;
        }
        EXPECT_FLOAT_EQ(mass, BudgetLiquidBasin::count * 0.5f);
      };
      expect_conserved_basin();

      auto recovered = engine.update();
      ASSERT_TRUE(recovered) << recovered.error().what();
      // Recovery gets a fresh budget, not a replay of the failed tick. One
      // ordinary cell retired before the first fault, two before the others.
      int ordinary_retired = successful_reads == 1 ? 3 : 4;
      for (int i = 0; i < BudgetLiquidBasin::count; ++i) {
        bool retired = i < ordinary_retired || (i >= 10 && i <= 14);
        EXPECT_EQ(engine.is_active({i * 2, 0}), !retired) << i;
      }
      expect_conserved_basin();

      engine.set_processing_limit(0);
      ASSERT_TRUE(engine.update());
      EXPECT_TRUE(engine.is_active({ordinary_retired * 2, 0}));
      engine.set_processing_limit(1);
      ASSERT_TRUE(engine.update());
      EXPECT_FALSE(engine.is_active({ordinary_retired * 2, 0}));
      EXPECT_TRUE(engine.is_active({(ordinary_retired + 1) * 2, 0}));
      expect_conserved_basin();
    }
  });
const GameTestRegistration liquid_cache_moves(
    "LiquidEngine", "WorkingCacheSurvivesMovesAndRepeatedPhaseFailures", [](GameTest& fixture) {
      auto database = fixture.root().liquidsDatabase();
      ASSERT_TRUE(database) << database.error().what();
      auto water = (*database)->liquidId("water");
      ASSERT_TRUE(water) << water.error().what();
      auto parameters = (*database)->liquid_engine_parameters();
      parameters.minimum_liquid_level = 0;
      auto basin = std::make_shared<LiquidBasin>();
      basin->cells[0] = {*water, 2.0f, 0.0f};
      LiquidCellEngine<LiquidId> original(43, parameters, basin);
      original.visit_region(RectI(0, 0, LiquidBasin::width, LiquidBasin::height));
      ASSERT_TRUE(original.update());
      auto moved = std::move(original);
      LiquidCellEngine<LiquidId> engine(44, parameters, basin);
      engine.visit_location({0, 0});
      ASSERT_TRUE(engine.update());
      engine = std::move(moved);
      for (unsigned tick = 0; tick < 4096; ++tick) {
        if (tick % 31 == 0) {
          engine.visit_location({0, 0});
          basin->fail_next_read = true;
          EXPECT_FALSE(engine.update());
          // A second consecutive failure must discard the first phase too.
          basin->fail_next_read = true;
          EXPECT_FALSE(engine.update());
        }
        engine.visit_region(RectI(0, 0, LiquidBasin::width, LiquidBasin::height));
        auto updated = engine.update();
        ASSERT_TRUE(updated) << updated.error().what();
        EXPECT_NEAR(basin->mass(), 2.0f, 0.0001f);
      }
    });

const GameTestRegistration liquid_readback_scales(
    "LiquidEngine", "ReadbackScaleChangesRecoverFromLateReadFailure", [](GameTest& fixture) {
      auto database = fixture.root().liquidsDatabase();
      ASSERT_TRUE(database) << database.error().what();
      auto water = (*database)->liquidId("water");
      ASSERT_TRUE(water) << water.error().what();
      struct Basin : SizedLiquidBasin<8194, 1> {
        mutable std::optional<unsigned> reads_before_failure;
        mutable std::uint64_t trace = 1469598103934665603ULL;
        void mix(std::uint32_t value) const { trace = (trace ^ value) * 1099511628211ULL; }
        std::expected<CellularLiquidCell<LiquidId>, StarException> cell(Vec2I const& p) const override {
          mix(p[0]);
          mix(p[1]);
          if (reads_before_failure) {
            if (*reads_before_failure == 0) {
              reads_before_failure.reset();
              return std::unexpected(LiquidReadbackFailure("Late scalable readback failure"));
            }
            --*reads_before_failure;
          }
          return SizedLiquidBasin<8194, 1>::cell(p);
        }
        std::expected<void, StarException> set_flow(
            Vec2I const& p, CellularLiquidFlowCell<LiquidId> const& flow) override {
          mix(p[0]);
          mix(p[1]);
          mix(std::bit_cast<std::uint32_t>(flow.level));
          mix(std::bit_cast<std::uint32_t>(flow.pressure));
          return SizedLiquidBasin<8194, 1>::set_flow(p, flow);
        }
      };
      auto basin = std::make_shared<Basin>();
      for (auto& cell : basin->cells)
        cell = {*water, 0.5f, 0.5f};
      LiquidCellEngine<LiquidId> engine(43, {}, basin);
      for (unsigned visits : {512u, 2048u, 8192u, 2048u}) {
        SCOPED_TRACE(visits);
        engine.set_processing_limit(0);
        engine.visit_region(RectI(0, 0, visits, 1));
        if (visits >= 2048) {
          basin->reads_before_failure = 20;
          auto failed = engine.update();
          ASSERT_FALSE(failed);
          EXPECT_TRUE(failed.error().is<LiquidReadbackFailure>());
          engine.visit_region(RectI(0, 0, visits, 1));
        }
        ASSERT_TRUE(engine.update());
        EXPECT_EQ(engine.active_cells(), visits + 2);
        engine.set_processing_limit({});
        ASSERT_TRUE(engine.update());
        EXPECT_EQ(engine.active_cells(), 0u);
        for (auto const& cell : basin->cells) {
          EXPECT_EQ(cell.liquid, *water);
          EXPECT_FLOAT_EQ(cell.level, 0.5f);
          EXPECT_FLOAT_EQ(cell.pressure, 0.5f);
        }
      }
      testing::Test::RecordProperty("read_write_trace", std::to_string(basin->trace));
    });
}

import std;
import star.algorithm;
import star.assets;
import star.cellular_liquid;
import star.format;
import star.file;
import star.game_test;
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
std::expected<std::shared_ptr<LiquidsDatabase>, StarException>
liquidsFixture(GameTest& fixture, String const& directory, Json const& config) {
  auto created = File::makeDirectoryRecursive(directory);
  if (!created)
    return std::unexpected(std::move(created.error()));
  auto preload = File::writeFile(String("[]"), File::relativeTo(directory, "preload.config"));
  if (!preload)
    return std::unexpected(std::move(preload.error()));
  auto contents = config.printJson();
  if (!contents)
    return std::unexpected(std::move(contents.error()));
  auto written = File::writeFile(*contents, File::relativeTo(directory, "liquids.config"));
  if (!written)
    return std::unexpected(std::move(written.error()));
  Assets::Settings settings{};
  settings.workerPoolSize = 1;
  settings.assetTimeToLive = 10;
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
  liquidEngineConfig("LiquidsDatabaseTest", "RequiredFloatFieldsUseAssetConfiguration", [](GameTest& fixture) {
    auto directory = File::temporaryDirectory(fixture.root().random());
    ASSERT_TRUE(directory);
    auto cleanup = finally([&] { EXPECT_TRUE(File::removeDirectoryRecursive(*directory)); });
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
    auto valid = liquidsFixture(fixture, File::relativeTo(*directory, "valid"),
                                JsonObject{{"liquidEngineParameters", engine}, {"backgroundDrain", 12}});
    ASSERT_TRUE(valid) << valid.error().what();
    auto parameters = (*valid)->liquidEngineParameters();
    EXPECT_FLOAT_EQ(parameters.lateralMoveFactor, 1);
    EXPECT_FLOAT_EQ(parameters.spreadOverfillUpFactor, 2);
    EXPECT_FLOAT_EQ(parameters.spreadOverfillLateralFactor, 3);
    EXPECT_FLOAT_EQ(parameters.spreadOverfillDownFactor, 4);
    EXPECT_FLOAT_EQ(parameters.pressureEqualizeFactor, 5);
    EXPECT_FLOAT_EQ(parameters.pressureMoveFactor, 6);
    EXPECT_FLOAT_EQ(parameters.maximumPressureLevelImbalance, 7);
    EXPECT_FLOAT_EQ(parameters.minimumLivenPressureChange, 8);
    EXPECT_FLOAT_EQ(parameters.minimumLivenLevelChange, 9);
    EXPECT_FLOAT_EQ(parameters.minimumLiquidLevel, 10);
    EXPECT_FLOAT_EQ(parameters.interactTransformationLevel, 11);
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
        auto changed = missing ? engine.eraseKey(name) : engine.set(name, "not a float");
        ASSERT_TRUE(changed);
        auto loaded = liquidsFixture(fixture, File::relativeTo(*directory, strf("case{}", index++)),
                                     JsonObject{{"liquidEngineParameters", *changed}, {"backgroundDrain", 12}});
        ASSERT_FALSE(loaded);
        EXPECT_TRUE(loaded.error().is<JsonException>());
      }
    }

    for (bool missing : {true, false}) {
      Json config = missing ? Json(JsonObject{{"liquidEngineParameters", engine}})
                            : Json(JsonObject{{"liquidEngineParameters", engine}, {"backgroundDrain", false}});
      auto outer = liquidsFixture(
        fixture, File::relativeTo(*directory, missing ? String("outerMissing") : String("outerType")), config);
      ASSERT_FALSE(outer);
      EXPECT_TRUE(outer.error().is<JsonException>());
    }
  });

template <int BasinWidth, int BasinHeight>
class SizedLiquidBasin : public CellularLiquidWorld<LiquidId> {
public:
  static constexpr int Width = BasinWidth;
  static constexpr int Height = BasinHeight;
  std::array<CellularLiquidFlowCell<LiquidId>, Width * Height> cells{};
  mutable bool failNextRead = false;

  Vec2I uniqueLocation(Vec2I const& position) const override {
    return {(position[0] % Width + Width) % Width, position[1]};
  }

  std::expected<CellularLiquidCell<LiquidId>, StarException> cell(Vec2I const& position) const override {
    if (std::exchange(failNextRead, false))
      return std::unexpected(StarException("Transient basin read failure"));
    auto point = uniqueLocation(position);
    if (point[1] < 0 || point[1] >= Height)
      return CellularLiquidCollisionCell{};
    return cells[point[1] * Width + point[0]];
  }

  std::expected<void, StarException> setFlow(Vec2I const& position, CellularLiquidFlowCell<LiquidId> const& flow) override {
    auto point = uniqueLocation(position);
    if (point[1] < 0 || point[1] >= Height)
      return std::unexpected(StarException("Liquid flow attempted to overwrite basin boundary"));
    cells[point[1] * Width + point[0]] = flow;
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

const GameTestRegistration liquidBorrowRecovery("LiquidEngine", "WrappedFlowConservesMassAfterReadFailure", [](GameTest& fixture) {
  auto database = fixture.root().liquidsDatabase();
  ASSERT_TRUE(database) << database.error().what();
  auto water = (*database)->liquidId("water");
  ASSERT_TRUE(water) << water.error().what();
  auto parameters = (*database)->liquidEngineParameters();
  // A closed basin has no drain or sub-threshold evaporation.
  parameters.minimumLiquidLevel = 0;
  auto basin = std::make_shared<LiquidBasin>();
  basin->cells[2 * LiquidBasin::Width] = {*water, 2.0f, 0.0f};
  LiquidCellEngine<LiquidId> engine(43, parameters, basin);
  engine.visitLocation({0, 2});
  ASSERT_TRUE(engine.update());
  basin->failNextRead = true;
  auto failed = engine.update();
  ASSERT_FALSE(failed);
  EXPECT_FLOAT_EQ(basin->mass(), 2.0f);

  engine.visitLocation({0, 2});
  for (unsigned tick = 0; tick < 64; ++tick) {
    auto updated = engine.update();
    ASSERT_TRUE(updated) << updated.error().what();
    EXPECT_NEAR(basin->mass(), 2.0f, 0.0001f);
  }
  float bottomMass = 0;
  for (int x = 0; x < LiquidBasin::Width; ++x)
    bottomMass += basin->cells[x].level;
  EXPECT_GT(bottomMass, 0.5f);
  EXPECT_GT(basin->cells[LiquidBasin::Width - 1].level, 0.01f);
});

class CallbackLiquidBasin : public CellularLiquidWorld<LiquidId> {
public:
  LiquidId water;
  LiquidId lava;
  std::optional<char> failure;
  std::vector<char> calls;
  unsigned sourceWrites = 0;
  std::array<CellularLiquidFlowCell<LiquidId>, 2> flows;
  std::vector<Vec2I> committedWrites;
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
    if (failure == kind && (kind != 'w' || !committedWrites.empty()))
      return std::unexpected(StarException(strf("Injected liquid callback {}", kind)));
    return {};
  }

  std::expected<void, StarException> setFlow(Vec2I const& p,
      CellularLiquidFlowCell<LiquidId> const& flow) override {
    if (p == Vec2I(0, 0)) {
      ++sourceWrites;
      return std::unexpected(StarException("Source cell was written as flow"));
    }
    auto recorded = record('w');
    if (!recorded)
      return recorded;
    flows[p[0] - 1] = flow;
    committedWrites.push_back(p);
    return {};
  }

  std::expected<void, StarException> liquidInteraction(Vec2I const&, LiquidId, Vec2I const&, LiquidId) override {
    auto recorded = record('i');
    if (recorded && interaction)
      interaction();
    return recorded;
  }

  std::expected<void, StarException> liquidCollision(Vec2I const&, LiquidId, Vec2I const&) override {
    return record('c');
  }
};

const GameTestRegistration liquidCallbackRecovery(
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
    engine.visitRegion(RectI(0, 0, 3, 1));
    ASSERT_TRUE(engine.update());
    basin->failure = failure;
    auto failed = engine.update();
    ASSERT_FALSE(failed);
    auto expectedError = StarException(strf("Injected liquid callback {}", failure));
    EXPECT_STREQ(failed.error().what(), expectedError.what());
    ASSERT_FALSE(basin->calls.empty());
    EXPECT_EQ(basin->calls.back(), failure);
    if (failure == 'w') {
      EXPECT_EQ(basin->calls.size(), 2u);
      EXPECT_EQ(basin->committedWrites.size(), 1u);
      EXPECT_FLOAT_EQ(basin->flows[basin->committedWrites.front()[0] - 1].level, 0.5f);
      auto committed = basin->committedWrites.front()[0] - 1;
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
      engine.visitRegion(RectI(0, 0, 3, 1));
      ASSERT_TRUE(engine.update());
    }
    EXPECT_EQ(basin->sourceWrites, 0u);
  }
  auto basin = std::make_shared<CallbackLiquidBasin>(*water, *lava);
  LiquidCellEngine<LiquidId> engine(43, {}, basin);
  engine.visitRegion(RectI(0, 0, 3, 1));
  ASSERT_TRUE(engine.update());
  basin->interaction = [&] {
    basin->flows[1].liquid = *water;
    engine.visitLocation({2, 0});
  };
  ASSERT_TRUE(engine.update());
  // The pressure correction also revisits x=1 and its source neighbor x=0.
  EXPECT_EQ(engine.activeCells(*water), 3u);
  EXPECT_EQ(engine.activeCells(*lava), 0u);
  EXPECT_EQ(basin->sourceWrites, 0u);

  auto emitting = std::make_shared<CallbackLiquidBasin>(*water, *lava);
  LiquidCellEngineParameters sourceParameters{};
  sourceParameters.lateralMoveFactor = 0.5f;
  LiquidCellEngine<LiquidId> sourceEngine(43, sourceParameters, emitting);
  sourceEngine.visitRegion(RectI(0, 0, 3, 1));
  ASSERT_TRUE(sourceEngine.update());
  ASSERT_TRUE(sourceEngine.update());
  EXPECT_FLOAT_EQ(emitting->flows[0].level, 0.75f);
  EXPECT_FLOAT_EQ(emitting->flows[1].level, 0.5f);
  EXPECT_EQ(emitting->sourceWrites, 0u);
});

class BudgetLiquidBasin : public CellularLiquidWorld<LiquidId> {
public:
  static constexpr int Count = 16;
  std::array<CellularLiquidFlowCell<LiquidId>, Count> cells{};
  mutable bool failNextRead = false;
  mutable std::optional<unsigned> readsBeforeFailure;

  Vec2I uniqueLocation(Vec2I const& position) const override {
    return {(position[0] % (Count * 2) + Count * 2) % (Count * 2), position[1]};
  }

  std::expected<CellularLiquidCell<LiquidId>, StarException> cell(Vec2I const& position) const override {
    if (std::exchange(failNextRead, false))
      return std::unexpected(StarException("Transient budget basin read failure"));
    if (readsBeforeFailure) {
      if (*readsBeforeFailure == 0) {
        readsBeforeFailure.reset();
        return std::unexpected(StarException("Delayed budget basin read failure"));
      }
      --*readsBeforeFailure;
    }
    auto point = uniqueLocation(position);
    if (point[1] != 0 || point[0] % 2 != 0)
      return CellularLiquidCollisionCell{};
    return cells[point[0] / 2];
  }

  std::expected<void, StarException> setFlow(Vec2I const& position,
                                             CellularLiquidFlowCell<LiquidId> const& flow) override {
    auto point = uniqueLocation(position);
    if (point[1] != 0 || point[0] % 2 != 0)
      return std::unexpected(StarException("Budget basin boundary overwritten"));
    cells[point[0] / 2] = flow;
    return {};
  }
};

const GameTestRegistration
  liquidBudgetOrdering("LiquidEngine", "UnlimitedCellsAfterBudgetAndOverlappingRegions", [](GameTest& fixture) {
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
    engine.setProcessingLimit(0);
    for (int i = 0; i < BudgetLiquidBasin::Count; ++i) {
      basin->cells[i] = {i % 2 ? *lava : *water, 0.5f, 0.5f};
      // Publish separately to establish a deterministic order in each liquid's
      // activation queue, independent of the visited-location hash set.
      engine.visitLocation({i * 2, 0});
      ASSERT_TRUE(engine.update());
    }
    ASSERT_EQ(engine.activeCells(), BudgetLiquidBasin::Count);
    engine.setProcessingLimit(2);
    // Engine membership uses Rect::contains (inclusive max), not belongs.
    // End at odd x=29 so x=28 is unlimited but x=30 remains ordinary.
    engine.setNoProcessingLimitRegions({RectI(20, 0, 28, 1), RectI(24, 0, 29, 1)});
    basin->failNextRead = true;
    auto failed = engine.update();
    ASSERT_FALSE(failed);
    ASSERT_EQ(engine.activeCells(), BudgetLiquidBasin::Count);
    ASSERT_TRUE(engine.update());
    for (int i = 0; i < BudgetLiquidBasin::Count; ++i) {
      bool unlimited = i >= 10 && i <= 14;
      EXPECT_EQ(engine.isActive({i * 2, 0}), !(i < 4 || unlimited)) << i;
      EXPECT_FLOAT_EQ(basin->cells[i].level, 0.5f);
      EXPECT_FLOAT_EQ(basin->cells[i].pressure, 0.5f);
      EXPECT_EQ(basin->cells[i].liquid, i % 2 ? *lava : *water);
    }
    EXPECT_EQ(engine.activeCells(), 7u);

    // Keep the same windows while retired cells become active again. The newly
    // unlimited cell must run with a zero ordinary budget; the newly ordinary
    // cell must stay behind the previously queued ordinary cells.
    engine.setProcessingLimit(0);
    engine.visitLocation({24, 0});
    engine.visitLocation({0, 0});
    ASSERT_TRUE(engine.update());
    EXPECT_TRUE(engine.isActive({24, 0}));
    EXPECT_TRUE(engine.isActive({0, 0}));
    EXPECT_EQ(engine.activeCells(), 9u);
    basin->failNextRead = true;
    ASSERT_FALSE(engine.update());
    EXPECT_EQ(engine.activeCells(), 9u);
    ASSERT_TRUE(engine.update());
    EXPECT_FALSE(engine.isActive({24, 0}));
    EXPECT_TRUE(engine.isActive({0, 0}));
    EXPECT_EQ(engine.activeCells(), 8u);
    engine.setProcessingLimit(1);
    ASSERT_TRUE(engine.update());
    EXPECT_FALSE(engine.isActive({8, 0}));
    EXPECT_FALSE(engine.isActive({10, 0}));
    EXPECT_TRUE(engine.isActive({0, 0}));
    EXPECT_EQ(engine.activeCells(), 6u);

    // Replacing the monitored windows must expose former unlimited cells to the
    // ordinary budget and include newly monitored cells without stale indexing.
    engine.setProcessingLimit(0);
    engine.setNoProcessingLimitRegions({RectI(30, 0, 32, 1)});
    ASSERT_TRUE(engine.update());
    EXPECT_FALSE(engine.isActive({30, 0}));
    EXPECT_TRUE(engine.isActive({12, 0}));
    EXPECT_TRUE(engine.isActive({18, 0}));
    EXPECT_EQ(engine.activeCells(), 5u);
    engine.setProcessingLimit({});
    engine.setLiquidTickDelta(*lava, 0);
    ASSERT_TRUE(engine.update());
    EXPECT_EQ(engine.activeCells(*water), 0u);
    EXPECT_EQ(engine.activeCells(*lava), 2u);
    engine.setLiquidTickDelta(*lava, 1);
    ASSERT_TRUE(engine.update());
    EXPECT_EQ(engine.activeCells(), 0u);
  });

const GameTestRegistration
  liquidPartialReadRecovery("LiquidEngine", "LateReadFailurePreservesRetirementAndBudgetOrder", [](GameTest& fixture) {
    auto database = fixture.root().liquidsDatabase();
    ASSERT_TRUE(database) << database.error().what();
    auto water = (*database)->liquidId("water");
    ASSERT_TRUE(water) << water.error().what();
    // Fail inside the ordinary prefix, then at two different positions in the
    // unlimited suffix after the ordinary budget has already been exhausted.
    for (unsigned successfulReads : {1u, 3u, 5u}) {
      SCOPED_TRACE(successfulReads);
      LiquidCellEngineParameters parameters{};
      auto basin = std::make_shared<BudgetLiquidBasin>();
      LiquidCellEngine<LiquidId> engine(43, parameters, basin);
      engine.setProcessingLimit(0);
      for (int i = 0; i < BudgetLiquidBasin::Count; ++i) {
        basin->cells[i] = {*water, 0.5f, 0.5f};
        engine.visitLocation({i * 2, 0});
        ASSERT_TRUE(engine.update());
      }
      engine.setProcessingLimit(2);
      engine.setNoProcessingLimitRegions({RectI(20, 0, 28, 1), RectI(24, 0, 29, 1)});
      basin->readsBeforeFailure = successfulReads;
      auto failed = engine.update();
      ASSERT_FALSE(failed);
      // The environment fails during selected-cell reads, before finalization.
      // Successful selections have already retired; the failed cell and all
      // later selections must remain active for recovery.
      constexpr std::array<int, 7> selected = {0, 1, 10, 11, 12, 13, 14};
      for (int i = 0; i < BudgetLiquidBasin::Count; ++i) {
        bool retired =
          std::find(selected.begin(), selected.begin() + successfulReads, i) != selected.begin() + successfulReads;
        EXPECT_EQ(engine.isActive({i * 2, 0}), !retired) << i;
      }
      auto expectConservedBasin = [&] {
        float mass = 0;
        for (auto const& cell : basin->cells) {
          EXPECT_EQ(cell.liquid, *water);
          EXPECT_FLOAT_EQ(cell.level, 0.5f);
          EXPECT_FLOAT_EQ(cell.pressure, 0.5f);
          mass += cell.level;
        }
        EXPECT_FLOAT_EQ(mass, BudgetLiquidBasin::Count * 0.5f);
      };
      expectConservedBasin();

      auto recovered = engine.update();
      ASSERT_TRUE(recovered) << recovered.error().what();
      // Recovery gets a fresh budget, not a replay of the failed tick. One
      // ordinary cell retired before the first fault, two before the others.
      int ordinaryRetired = successfulReads == 1 ? 3 : 4;
      for (int i = 0; i < BudgetLiquidBasin::Count; ++i) {
        bool retired = i < ordinaryRetired || (i >= 10 && i <= 14);
        EXPECT_EQ(engine.isActive({i * 2, 0}), !retired) << i;
      }
      expectConservedBasin();

      engine.setProcessingLimit(0);
      ASSERT_TRUE(engine.update());
      EXPECT_TRUE(engine.isActive({ordinaryRetired * 2, 0}));
      engine.setProcessingLimit(1);
      ASSERT_TRUE(engine.update());
      EXPECT_FALSE(engine.isActive({ordinaryRetired * 2, 0}));
      EXPECT_TRUE(engine.isActive({(ordinaryRetired + 1) * 2, 0}));
      expectConservedBasin();
    }
  });
const GameTestRegistration liquidCacheMoves(
    "LiquidEngine", "WorkingCacheSurvivesMovesAndRepeatedPhaseFailures", [](GameTest& fixture) {
      auto database = fixture.root().liquidsDatabase();
      ASSERT_TRUE(database) << database.error().what();
      auto water = (*database)->liquidId("water");
      ASSERT_TRUE(water) << water.error().what();
      auto parameters = (*database)->liquidEngineParameters();
      parameters.minimumLiquidLevel = 0;
      auto basin = std::make_shared<LiquidBasin>();
      basin->cells[0] = {*water, 2.0f, 0.0f};
      LiquidCellEngine<LiquidId> original(43, parameters, basin);
      original.visitRegion(RectI(0, 0, LiquidBasin::Width, LiquidBasin::Height));
      ASSERT_TRUE(original.update());
      auto moved = std::move(original);
      LiquidCellEngine<LiquidId> engine(44, parameters, basin);
      engine.visitLocation({0, 0});
      ASSERT_TRUE(engine.update());
      engine = std::move(moved);
      for (unsigned tick = 0; tick < 4096; ++tick) {
        if (tick % 31 == 0) {
          engine.visitLocation({0, 0});
          basin->failNextRead = true;
          EXPECT_FALSE(engine.update());
          // A second consecutive failure must discard the first phase too.
          basin->failNextRead = true;
          EXPECT_FALSE(engine.update());
        }
        engine.visitRegion(RectI(0, 0, LiquidBasin::Width, LiquidBasin::Height));
        auto updated = engine.update();
        ASSERT_TRUE(updated) << updated.error().what();
        EXPECT_NEAR(basin->mass(), 2.0f, 0.0001f);
      }
    });

const GameTestRegistration liquidReadbackScales(
    "LiquidEngine", "ReadbackScaleChangesRecoverFromLateReadFailure", [](GameTest& fixture) {
      auto database = fixture.root().liquidsDatabase();
      ASSERT_TRUE(database) << database.error().what();
      auto water = (*database)->liquidId("water");
      ASSERT_TRUE(water) << water.error().what();
      struct Basin : SizedLiquidBasin<8194, 1> {
        mutable std::optional<unsigned> readsBeforeFailure;
        mutable std::uint64_t trace = 1469598103934665603ULL;
        void mix(std::uint32_t value) const { trace = (trace ^ value) * 1099511628211ULL; }
        std::expected<CellularLiquidCell<LiquidId>, StarException> cell(Vec2I const& p) const override {
          mix(p[0]);
          mix(p[1]);
          if (readsBeforeFailure) {
            if (*readsBeforeFailure == 0) {
              readsBeforeFailure.reset();
              return std::unexpected(StarException("Late scalable readback failure"));
            }
            --*readsBeforeFailure;
          }
          return SizedLiquidBasin<8194, 1>::cell(p);
        }
        std::expected<void, StarException> setFlow(
            Vec2I const& p, CellularLiquidFlowCell<LiquidId> const& flow) override {
          mix(p[0]);
          mix(p[1]);
          mix(std::bit_cast<std::uint32_t>(flow.level));
          mix(std::bit_cast<std::uint32_t>(flow.pressure));
          return SizedLiquidBasin<8194, 1>::setFlow(p, flow);
        }
      };
      auto basin = std::make_shared<Basin>();
      for (auto& cell : basin->cells)
        cell = {*water, 0.5f, 0.5f};
      LiquidCellEngine<LiquidId> engine(43, {}, basin);
      for (unsigned visits : {512u, 2048u, 8192u, 2048u}) {
        SCOPED_TRACE(visits);
        engine.setProcessingLimit(0);
        engine.visitRegion(RectI(0, 0, visits, 1));
        if (visits >= 2048) {
          basin->readsBeforeFailure = 20;
          auto failed = engine.update();
          ASSERT_FALSE(failed);
          auto expectedError = StarException("Late scalable readback failure");
          EXPECT_STREQ(failed.error().what(), expectedError.what());
          engine.visitRegion(RectI(0, 0, visits, 1));
        }
        ASSERT_TRUE(engine.update());
        EXPECT_EQ(engine.activeCells(), visits + 2);
        engine.setProcessingLimit({});
        ASSERT_TRUE(engine.update());
        EXPECT_EQ(engine.activeCells(), 0u);
        for (auto const& cell : basin->cells) {
          EXPECT_EQ(cell.liquid, *water);
          EXPECT_FLOAT_EQ(cell.level, 0.5f);
          EXPECT_FLOAT_EQ(cell.pressure, 0.5f);
        }
      }
      testing::Test::RecordProperty("read_write_trace", std::to_string(basin->trace));
    });
}

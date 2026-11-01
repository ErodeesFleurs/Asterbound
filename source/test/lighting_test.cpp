#include "gtest/gtest.h"

import std;
import star.assets;
import star.cellular_light_array;
import star.cellular_lighting;
import star.format;
import star.game_test;
import star.json;
import star.liquid_types;
import star.liquids_database;
import star.material_database;
import star.material_types;
import star.rect;
import star.root;
import star.string;
import star.tile_sector_array;
import star.vector;
import star.worker_pool;
import star.world_client;
import star.world_tiles;

using namespace Star;

namespace {

template <typename Traits>
void checkPointObstacleSnapshotRefresh() {
  using Array = CellularLightArray<Traits>;
  typename Traits::Value value;
  if constexpr (std::same_as<Traits, ScalarLightTraits>)
    value = 1.0f;
  else
    value = Vec3F(1.0f, 0.8f, 0.6f);
  Array reused;
  Array reference;
  for (auto* array : {&reused, &reference}) {
    array->setParameters(0, 20, 1, 20, 1, 0, false);
    array->begin(20, 20);
    array->addPointLight({{4.5f, 10.5f}, value, 0, 0, 0, false});
  }
  auto& retainedCell = reused.cell(10, 10);
  reused.calculate(1, 1, 19, 19);
  auto unobstructed = Traits::maxIntensity(reused.getLight(15, 10));
  ASSERT_GT(unobstructed, 0.0f);
  for (bool obstacle : {true, false, true}) {
    for (std::size_t i = 0; i < 400; ++i) {
      reused.cellAtIndex(i).light = {};
      reference.cellAtIndex(i).light = {};
    }
    // Mutate through a reference obtained before the previous calculation,
    // bypassing setObstacle. A cache updated only by setters would be stale.
    retainedCell.obstacle = obstacle;
    reference.setObstacle(10, 10, obstacle);
    reused.calculate(1, 1, 19, 19);
    reference.calculate(1, 1, 19, 19);
    for (std::size_t i = 0; i < 400; ++i)
      EXPECT_EQ(std::memcmp(&reused.cellAtIndex(i).light, &reference.cellAtIndex(i).light,
          sizeof(typename Traits::Value)), 0);
    auto brightness = Traits::maxIntensity(reused.getLight(15, 10));
    if (obstacle)
      EXPECT_LT(brightness, unobstructed);
    else
      EXPECT_EQ(brightness, unobstructed);
  }
  reused.begin(8, 12);
  reused.addPointLight({{2.5f, 4.5f}, value, 0, 0, 0, false});
  reused.cellAtIndex(3 * 12 + 5).obstacle = true;
  reused.calculate(1, 1, 7, 11);
  // The horizontal ray misses (3,5), while the ray to (6,5) crosses it
  // with weight 1/4. Distance alone differs by less than 0.01 here, so an
  // old 20-cell column stride cannot satisfy this attenuation difference.
  EXPECT_GT(Traits::maxIntensity(reused.getLight(6, 4))
      - Traits::maxIntensity(reused.getLight(6, 5)), 0.1f);
}

template <typename Traits>
void checkPointRayOrientations() {
  typename Traits::Value value;
  if constexpr (std::same_as<Traits, ScalarLightTraits>)
    value = 1.0f;
  else
    value = Vec3F(1.0f, 0.8f, 0.6f);
  for (auto [obstacle, obstacleRange] :
      {std::pair{false, 2.0f}, std::pair{true, 2.0f}, std::pair{true, 0.25f}}) {
    CellularLightArray<Traits> array;
    array.setParameters(0, 20, 1, 20, obstacleRange, 0, false);
    array.begin(24, 24);
    array.addPointLight({{10.5f, 10.5f}, value, 0, 0, 0, false});
    array.setObstacle(10, 10, obstacle);
    array.calculate(1, 1, 23, 23);
    for (auto target : {Vec2I(15, 10), Vec2I(5, 10), Vec2I(10, 15), Vec2I(10, 5),
        Vec2I(15, 15), Vec2I(5, 5), Vec2I(5, 15), Vec2I(15, 5),
        Vec2I(17, 12), Vec2I(3, 8), Vec2I(12, 17), Vec2I(8, 3)}) {
      auto delta = Vec2F(target - Vec2I(10, 10));
      float distance = delta.magnitude();
      float expected = 1.0f - distance / 20.0f;
      if (obstacle) {
        // An isolated source-center obstacle covers exactly half a ray cell.
        // Circularization scales that half by the dominant direction axis.
        float dominant = std::max(std::fabs(delta[0] / distance), std::fabs(delta[1] / distance));
        expected -= 0.5f / obstacleRange / dominant;
      }
      expected = std::max(expected, 0.0f);
      EXPECT_NEAR(Traits::maxIntensity(array.getLight(target[0], target[1])), expected, 0.000001f)
          << target[0] << "," << target[1] << " obstacle=" << obstacle << " range=" << obstacleRange;
    }
  }
  for (bool vertical : {false, true}) {
    CellularLightArray<Traits> array;
    array.setParameters(0, 20, 1, 20, 2, 0, false);
    array.begin(24, 24);
    array.addPointLight({vertical ? Vec2F(10.5f, 10.0f) : Vec2F(10.0f, 10.5f),
        value, 0, 0, 0, false});
    array.setObstacle(10, 10, true);
    array.setObstacle(vertical ? 10 : 9, vertical ? 9 : 10, true);
    array.calculate(1, 1, 23, 23);
    // Corrected source coordinates are 9.5, an endpoint-rounding tie.
    // Both directions cross one full obstacle cell at distance 5.5.
    for (int coordinate : {4, 15})
      EXPECT_NEAR(Traits::maxIntensity(array.getLight(vertical ? 10 : coordinate,
          vertical ? coordinate : 10)), 0.225f, 0.000001f);
  }
}

void addBoundaryLights(CellularLightingCalculator& calculator, RectI const& query) {
  Vec2F origin(query.min());
  calculator.addSpreadLight(origin + Vec2F{0.25f, 1.75f}, {0.8f, 0.3f, 0.1f});
  calculator.addPointLight(origin + Vec2F{-0.25f, 8.5f}, {0.1f, 0.6f, 0.9f}, 1.3f, 0.7f, 0.15f);
  calculator.addSpreadLight(origin + Vec2F{17.5f, 13.25f}, Vec3F{0.2f, 0.4f, 0.7f} * 0.85f);
  calculator.addPointLight(origin + Vec2F{17.5f, 13.25f}, {0.2f, 0.4f, 0.7f}, 0.0f, 0.0f, 1.0f, true);
}

void gatherTileOracle(CellularLightingCalculator& calculator, ClientTileSectorArray const& tiles,
                      MaterialDatabase const& materials, LiquidsDatabase const& liquids, Vec3F const& environment,
                      float underground) {
  auto region = calculator.calculationRegion();
  for (int x = region.xMin(); x < region.xMax(); ++x) {
    for (int y = region.yMin(); y < region.yMax(); ++y) {
      Vec2I position{x, y};
      if (y < 0 || y >= static_cast<int>(tiles.size()[1]) || !tiles.tileLoaded(position))
        continue;
      auto const& tile = tiles.tile(position);
      Vec3F emission;
      if (tile.foreground != EmptyMaterialId || tile.foregroundMod != NoModId)
        emission += materials.radiantLight(tile.foreground, tile.foregroundMod);
      if (tile.liquid.liquid != EmptyLiquidId && tile.liquid.level != 0.0f)
        emission += liquids.radiantLight(tile.liquid);
      if (tile.foregroundLightTransparent) {
        if (tile.background != EmptyMaterialId || tile.backgroundMod != NoModId)
          emission += materials.radiantLight(tile.background, tile.backgroundMod);
        if (tile.backgroundLightTransparent && y > underground)
          emission += environment;
      }
      calculator.setCellIndex(calculator.baseIndexFor(position), emission, !tile.foregroundLightTransparent);
    }
  }
}

void checkWideLightingSnapshot(GameTest& fixture, RectI const& query) {
  auto materials = fixture.root().materialDatabase();
  ASSERT_TRUE(materials);
  auto liquids = fixture.root().liquidsDatabase();
  ASSERT_TRUE(liquids);
  auto assets = fixture.root().assets();
  ASSERT_TRUE(assets);
  auto parameters = (*assets)->json("/lighting.config:lighting");
  ASSERT_TRUE(parameters);
  auto workers = fixture.root().workers();
  ASSERT_TRUE(workers);
  WorkerPool caller("Lighting wide snapshot caller");

  std::optional<MaterialId> radiantMaterial;
  auto names = (*materials)->materialNames();
  std::ranges::sort(names);
  for (auto const& name : names) {
    auto id = (*materials)->materialId(name);
    ASSERT_TRUE(id);
    if ((*materials)->radiantLight(*id, NoModId).max() > 0.0f) {
      radiantMaterial = *id;
      break;
    }
  }
  ASSERT_TRUE(radiantMaterial);
  std::optional<LiquidId> radiantLiquid;
  auto liquidNames = (*liquids)->liquidNames();
  std::ranges::sort(liquidNames);
  for (auto const& name : liquidNames) {
    auto id = (*liquids)->liquidId(name);
    ASSERT_TRUE(id);
    if ((*liquids)->radiantLight(LiquidLevel{*id, 1.0f}).max() > 0.0f) {
      radiantLiquid = *id;
      break;
    }
  }
  ASSERT_TRUE(radiantLiquid);

  ClientTileSectorArray tiles({64, 64});
  tiles.loadDefaultSector({0, 0});
  tiles.loadDefaultSector({1, 0});
  tiles.loadDefaultSector({0, 1});
  // Sector (1, 1) stays unloaded in every repeated world image.
  ASSERT_TRUE(
    tiles.tileEval(RectI(0, 0, 64, 64), [&](Vec2I const& pos, ClientTile& tile) -> std::expected<void, StarException> {
      int key = pos[0] * 11 + pos[1] * 23;
      tile.foreground = key % 17 == 0 ? *radiantMaterial : EmptyMaterialId;
      tile.foregroundMod = NoModId;
      tile.background = key % 19 == 0 ? *radiantMaterial : EmptyMaterialId;
      tile.backgroundMod = NoModId;
      tile.foregroundLightTransparent = pos[0] % 13 != 0 || pos[1] % 7 == 0;
      tile.backgroundLightTransparent = key % 5 != 0;
      tile.liquid = key % 7 == 0 ? LiquidLevel{*radiantLiquid, (key % 5) * 0.25f} : LiquidLevel();
      return {};
    }));

  Vec3F environment{0.13f, 0.23f, 0.37f};
  for (bool monochrome : {false, true}) {
    CellularLightingCalculator oracle(monochrome);
    ASSERT_TRUE(oracle.setParameters(*parameters));
    oracle.begin(query);
    ASSERT_GE(oracle.calculationRegion().width(), static_cast<int>(tiles.size()[0]));
    gatherTileOracle(oracle, tiles, **materials, **liquids, environment, 13.0f);
    Lightmap expected;
    ASSERT_TRUE(oracle.calculate(expected));
    std::vector<float> snapshot(expected.data().begin(), expected.data().end());
    ASSERT_TRUE(std::ranges::any_of(snapshot, [](float value) { return value > 0.0f; }));
    for (bool parallel : {false, true}) {
      SCOPED_TRACE(std::format("monochrome={} parallel={}", monochrome, parallel));
      CellularLightingCalculator actual(monochrome);
      ASSERT_TRUE(actual.setParameters(*parameters));
      actual.begin(query);
      ASSERT_TRUE(gatherClientLightingTiles(actual, tiles, parallel ? workers->get() : caller, **materials, **liquids,
                                            environment, 13.0f));
      Lightmap output;
      ASSERT_TRUE(actual.calculate(output));
      ASSERT_EQ(output.size(), Vec2U(query.size()));
      ASSERT_EQ(output.data().size(), snapshot.size());
      ASSERT_EQ(std::memcmp(output.data().data(), snapshot.data(), output.data().size_bytes()), 0);
    }
  }
  ASSERT_TRUE(caller.finish());
}

}// namespace
TEST(CellularLightArray, PointObstacleSnapshotRefreshesMutableCellsAndStride) {
  checkPointObstacleSnapshotRefresh<ScalarLightTraits>();
  checkPointObstacleSnapshotRefresh<ColoredLightTraits>();
}
TEST(CellularLightArray, PointRaysMatchGeometricAttenuationInEveryOrientation) {
  checkPointRayOrientations<ScalarLightTraits>();
  checkPointRayOrientations<ColoredLightTraits>();
}


const GameTestRegistration
  clientLightingGatherMatchesTileSnapshot("ClientLighting", "GatherMatchesTileSnapshot", [](GameTest& fixture) {
    auto materials = fixture.root().materialDatabase();
    ASSERT_TRUE(materials);
    auto liquids = fixture.root().liquidsDatabase();
    ASSERT_TRUE(liquids);
    auto assets = fixture.root().assets();
    ASSERT_TRUE(assets);
    auto parameters = (*assets)->json("/lighting.config:lighting");
    ASSERT_TRUE(parameters);
    auto workers = fixture.root().workers();
    ASSERT_TRUE(workers);
    WorkerPool caller("Lighting serial regression");

    auto names = (*materials)->materialNames();
    std::ranges::sort(names);
    std::optional<MaterialId> radiantMaterial;
    for (auto const& name : names) {
      auto id = (*materials)->materialId(name);
      ASSERT_TRUE(id);
      if ((*materials)->radiantLight(*id, NoModId).max() > 0.0f) {
        radiantMaterial = *id;
        break;
      }
    }
    ASSERT_TRUE(radiantMaterial);
    auto liquidNames = (*liquids)->liquidNames();
    std::ranges::sort(liquidNames);
    std::optional<LiquidId> radiantLiquid;
    for (auto const& name : liquidNames) {
      auto id = (*liquids)->liquidId(name);
      ASSERT_TRUE(id);
      if ((*liquids)->radiantLight(LiquidLevel{*id, 1.0f}).max() > 0.0f) {
        radiantLiquid = *id;
        break;
      }
    }
    ASSERT_TRUE(radiantLiquid);
    std::optional<ModId> radiantMod;
    auto modNames = (*materials)->modNames();
    std::ranges::sort(modNames);
    for (auto const& name : modNames) {
      auto id = (*materials)->modId(name);
      ASSERT_TRUE(id);
      if ((*materials)->radiantLight(EmptyMaterialId, *id).max() > 0.0f) {
        radiantMod = *id;
        break;
      }
    }
    ASSERT_TRUE(radiantMod);

    // Column traversal represents at most one wrap. The real configured border
    // must fit within the world; a whole-world query intentionally collapses its
    // x coordinates and is not an unwrapped calculator snapshot.
    CellularLightingCalculator regionProbe;
    ASSERT_TRUE(regionProbe.setParameters(*parameters));
    regionProbe.begin(RectI(-8, -4, 48, 40));
    unsigned worldWidth = static_cast<unsigned>(regionProbe.calculationRegion().width()) + 32;
    ClientTileSectorArray tiles({worldWidth, 96});
    auto sectors = tiles.validSectorsFor(RectI(0, 0, static_cast<int>(worldWidth), 96));
    ASSERT_TRUE(sectors);
    for (auto const& sector : *sectors) {
      if (sector != ClientTileSectorArray::Sector{1, 1})
        tiles.loadDefaultSector(sector);
    }
    ASSERT_TRUE(tiles.tileEval(RectI(0, 0, static_cast<int>(worldWidth), 96),
                               [&](Vec2I const& pos, ClientTile& tile) -> std::expected<void, StarException> {
                                 int key = pos[0] * 11 + pos[1] * 23;
                                 tile.foreground = key % 17 == 0 ? *radiantMaterial : EmptyMaterialId;
                                 tile.foregroundMod = key % 29 == 0 ? *radiantMod : NoModId;
                                 tile.background = key % 19 == 0 ? *radiantMaterial : EmptyMaterialId;
                                 tile.backgroundMod = key % 31 == 0 ? *radiantMod : NoModId;
                                 tile.foregroundLightTransparent = pos[0] % 13 != 0 || pos[1] % 7 == 0;
                                 tile.backgroundLightTransparent = key % 5 != 0;
                                 tile.liquid =
                                   key % 7 == 0 ? LiquidLevel{*radiantLiquid, (key % 5) * 0.25f} : LiquidLevel();
                                 return {};
                               }));

    Vec3F environment{0.13f, 0.23f, 0.37f};
    for (bool monochrome : {false, true}) {
      for (RectI query : {RectI(-8, -4, 48, 40),
                          RectI(static_cast<int>(worldWidth) - 36, 67, static_cast<int>(worldWidth) + 8, 101)}) {
        for (bool additive : {false, true}) {
          auto config = parameters->set("pointAdditive", additive);
          ASSERT_TRUE(config);
          CellularLightingCalculator oracle(monochrome);
          ASSERT_TRUE(oracle.setParameters(*config));
          oracle.begin(query);
          ASSERT_LT(oracle.calculationRegion().width(), static_cast<int>(worldWidth));
          gatherTileOracle(oracle, tiles, **materials, **liquids, environment, 13.0f);
          addBoundaryLights(oracle, query);
          Lightmap expected;
          ASSERT_TRUE(oracle.calculate(expected));
          std::vector<float> snapshot(expected.data().begin(), expected.data().end());
          for (bool parallel : {false, true}) {
            CellularLightingCalculator actual(monochrome);
            ASSERT_TRUE(actual.setParameters(*config));
            actual.begin(query);
            ASSERT_TRUE(gatherClientLightingTiles(actual, tiles, parallel ? workers->get() : caller, **materials,
                                                  **liquids, environment, 13.0f));
            addBoundaryLights(actual, query);
            Lightmap output;
            ASSERT_TRUE(actual.calculate(output));
            EXPECT_EQ(output.size(), Vec2U(query.size()));
            ASSERT_EQ(output.data().size(), snapshot.size());
            EXPECT_EQ(std::memcmp(output.data().data(), snapshot.data(), output.data().size_bytes()), 0);
          }
        }
      }
    }
    ASSERT_TRUE(caller.finish());
  });

const GameTestRegistration
  clientLightingBeginClearsPreviousSnapshot("ClientLighting", "BeginClearsPreviousSnapshot", [](GameTest& fixture) {
    auto assets = fixture.root().assets();
    ASSERT_TRUE(assets);
    auto parameters = (*assets)->json("/lighting.config:lighting");
    ASSERT_TRUE(parameters);
    CellularLightingCalculator calculator;
    ASSERT_TRUE(calculator.setParameters(*parameters));
    RectI query(10, 20, 27, 33);
    for (bool monochrome : {false, true, false}) {
      ASSERT_TRUE(calculator.setMonochrome(monochrome));
      calculator.begin(query);
      auto region = calculator.calculationRegion();
      for (int x = region.xMin(); x < region.xMax(); ++x) {
        for (int y = region.yMin(); y < region.yMax(); ++y)
          calculator.setCellIndex(calculator.baseIndexFor({x, y}), {0.3f, 0.5f, 0.7f}, (x + y) % 3 == 0);
      }
      addBoundaryLights(calculator, query);
      Lightmap lit;
      ASSERT_TRUE(calculator.calculate(lit));
      EXPECT_TRUE(std::ranges::any_of(lit.data(), [](float value) { return value > 0.0f; }));
      calculator.begin(query);
      Lightmap dark;
      ASSERT_TRUE(calculator.calculate(dark));
      EXPECT_EQ(dark.size(), Vec2U(query.size()));
      EXPECT_TRUE(std::ranges::all_of(dark.data(), [](float value) { return value == 0.0f; }));
    }
  });

const GameTestRegistration clientLightingWideAcrossSeam("ClientLightingWideSnapshot", "AcrossSeam",
                                                        [](GameTest& fixture) {
                                                          checkWideLightingSnapshot(fixture, RectI(-19, -5, 45, 47));
                                                        });

const GameTestRegistration
  clientLightingWideNegativeOrigin("ClientLightingWideSnapshot", "SeveralWrapsNegativeOrigin", [](GameTest& fixture) {
    checkWideLightingSnapshot(fixture, RectI(-173, 11, 97, 67));
  });

const GameTestRegistration
  clientLightingWidePositiveOrigin("ClientLightingWideSnapshot", "SeveralWrapsPositiveOrigin", [](GameTest& fixture) {
    checkWideLightingSnapshot(fixture, RectI(139, 31, 299, 91));
  });

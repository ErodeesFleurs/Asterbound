#include "gtest/gtest.h"

import std;
import star.assets;
import star.cellular_light_array;
import star.cellular_lighting;
import star.format;
import star.game_test;
import star.image;
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

template <typename T>
concept has_lightmap_span = requires(T&& map) { std::forward<T>(map).span(); };

static_assert(std::same_as<decltype(std::declval<Lightmap&>().span()), std::span<float>>);
static_assert(std::same_as<decltype(std::declval<Lightmap const&>().span()), std::span<float const>>);
static_assert(!has_lightmap_span<Lightmap> && !has_lightmap_span<Lightmap const>);
static_assert(std::is_convertible_v<Lightmap&, ImageView> && std::is_convertible_v<Lightmap const&, ImageView>);
static_assert(!std::is_convertible_v<Lightmap&&, ImageView> && !std::is_convertible_v<Lightmap const&&, ImageView>);

template <light_traits Traits>
void check_point_obstacle_snapshot_refresh() {
  using Array = CellularLightArray<Traits>;
  typename Traits::Value value;
  if constexpr (std::same_as<Traits, ScalarLightTraits>)
    value = 1.0f;
  else
    value = Vec3F(1.0f, 0.8f, 0.6f);
  Array reused;
  Array reference;
  auto parameters = LightParameters::create(0, 20, 1, 20, 1, 0, false);
  ASSERT_TRUE(parameters);
  for (Array& array : {std::ref(reused), std::ref(reference)}) {
    array.set_parameters(*parameters);
    ASSERT_TRUE(array.begin(20, 20));
    array.add_point_light({{4.5f, 10.5f}, value, 0, 0, 0, false});
  }
  auto& retained_cell = reused.cell(10, 10);
  reused.calculate(1, 1, 19, 19);
  auto unobstructed = Traits::max_intensity(reused.get_light(15, 10));
  ASSERT_GT(unobstructed, 0.0f);
  for (bool obstacle : {true, false, true}) {
    for (std::size_t i = 0; i < 400; ++i) {
      reused.cell_at_index(i).light = {};
      reference.cell_at_index(i).light = {};
    }
    // Mutate through a reference obtained before the previous calculation,
    // bypassing set_obstacle. A cache updated only by setters would be stale.
    retained_cell.obstacle = obstacle;
    reference.set_obstacle(10, 10, obstacle);
    reused.calculate(1, 1, 19, 19);
    reference.calculate(1, 1, 19, 19);
    using Bits = std::array<std::byte, sizeof(typename Traits::Value)>;
    for (std::size_t i = 0; i < 400; ++i)
      EXPECT_EQ(std::bit_cast<Bits>(reused.cell_at_index(i).light), std::bit_cast<Bits>(reference.cell_at_index(i).light));
    auto brightness = Traits::max_intensity(reused.get_light(15, 10));
    if (obstacle)
      EXPECT_LT(brightness, unobstructed);
    else
      EXPECT_EQ(brightness, unobstructed);
  }
  ASSERT_TRUE(reused.begin(8, 12));
  reused.add_point_light({{2.5f, 4.5f}, value, 0, 0, 0, false});
  reused.cell_at_index(3 * 12 + 5).obstacle = true;
  reused.calculate(1, 1, 7, 11);
  // The horizontal ray misses (3,5), while the ray to (6,5) crosses it
  // with weight 1/4. Distance alone differs by less than 0.01 here, so an
  // old 20-cell column stride cannot satisfy this attenuation difference.
  EXPECT_GT(Traits::max_intensity(reused.get_light(6, 4))
      - Traits::max_intensity(reused.get_light(6, 5)), 0.1f);
}

template <light_traits Traits>
void check_point_ray_orientations() {
  typename Traits::Value value;
  if constexpr (std::same_as<Traits, ScalarLightTraits>)
    value = 1.0f;
  else
    value = Vec3F(1.0f, 0.8f, 0.6f);
  for (auto [obstacle, obstacle_range] :
      {std::pair{false, 2.0f}, std::pair{true, 2.0f}, std::pair{true, 0.25f}}) {
    CellularLightArray<Traits> array;
    auto parameters = LightParameters::create(0, 20, 1, 20, obstacle_range, 0, false);
    ASSERT_TRUE(parameters);
    array.set_parameters(*parameters);
    ASSERT_TRUE(array.begin(24, 24));
    array.add_point_light({{10.5f, 10.5f}, value, 0, 0, 0, false});
    array.set_obstacle(10, 10, obstacle);
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
        expected -= 0.5f / obstacle_range / dominant;
      }
      expected = std::max(expected, 0.0f);
      EXPECT_NEAR(Traits::max_intensity(array.get_light(target[0], target[1])), expected, 0.000001f)
          << target[0] << "," << target[1] << " obstacle=" << obstacle << " range=" << obstacle_range;
    }
  }
  for (bool vertical : {false, true}) {
    CellularLightArray<Traits> array;
    auto parameters = LightParameters::create(0, 20, 1, 20, 2, 0, false);
    ASSERT_TRUE(parameters);
    array.set_parameters(*parameters);
    ASSERT_TRUE(array.begin(24, 24));
    array.add_point_light({vertical ? Vec2F(10.5f, 10.0f) : Vec2F(10.0f, 10.5f),
        value, 0, 0, 0, false});
    array.set_obstacle(10, 10, true);
    array.set_obstacle(vertical ? 10 : 9, vertical ? 9 : 10, true);
    array.calculate(1, 1, 23, 23);
    // Corrected source coordinates are 9.5, an endpoint-rounding tie.
    // Both directions cross one full obstacle cell at distance 5.5.
    for (int coordinate : {4, 15})
      EXPECT_NEAR(Traits::max_intensity(array.get_light(vertical ? 10 : coordinate,
          vertical ? coordinate : 10)), 0.225f, 0.000001f);
  }
}

void addBoundaryLights(CellularLightingCalculator& calculator, RectI const& query) {
  Vec2F origin(query.min());
  calculator.add_spread_light(origin + Vec2F{0.25f, 1.75f}, {0.8f, 0.3f, 0.1f});
  calculator.add_point_light(origin + Vec2F{-0.25f, 8.5f}, {0.1f, 0.6f, 0.9f}, 1.3f, 0.7f, 0.15f);
  calculator.add_spread_light(origin + Vec2F{17.5f, 13.25f}, Vec3F{0.2f, 0.4f, 0.7f} * 0.85f);
  calculator.add_point_light(origin + Vec2F{17.5f, 13.25f}, {0.2f, 0.4f, 0.7f}, 0.0f, 0.0f, 1.0f, true);
}

void gatherTileOracle(CellularLightingCalculator& calculator, ClientTileSectorArray const& tiles,
                      MaterialDatabase const& materials, LiquidsDatabase const& liquids, Vec3F const& environment,
                      float underground) {
  auto region = calculator.calculation_region();
  for (int x = region.x_min(); x < region.x_max(); ++x) {
    for (int y = region.y_min(); y < region.y_max(); ++y) {
      Vec2I position{x, y};
      if (y < 0 || y >= static_cast<int>(tiles.size()[1]) || !tiles.tile_loaded(position))
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
      calculator.set_cell_index(calculator.base_index_for(position), emission, !tile.foregroundLightTransparent);
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
  tiles.load_default_sector({0, 0});
  tiles.load_default_sector({1, 0});
  tiles.load_default_sector({0, 1});
  // Sector (1, 1) stays unloaded in every repeated world image.
  ASSERT_TRUE(
    tiles.tile_eval(RectI(0, 0, 64, 64), [&](Vec2I const& pos, ClientTile& tile) -> std::expected<void, StarException> {
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
    ASSERT_TRUE(oracle.set_parameters(*parameters));
    ASSERT_TRUE(oracle.begin(query));
    ASSERT_GE(oracle.calculation_region().width(), static_cast<int>(tiles.size()[0]));
    gatherTileOracle(oracle, tiles, **materials, **liquids, environment, 13.0f);
    Lightmap expected;
    ASSERT_TRUE(oracle.calculate(expected));
    std::vector<float> snapshot(std::from_range, expected.span());
    ASSERT_TRUE(std::ranges::any_of(snapshot, [](float value) { return value > 0.0f; }));
    for (bool parallel : {false, true}) {
      SCOPED_TRACE(std::format("monochrome={} parallel={}", monochrome, parallel));
      CellularLightingCalculator actual(monochrome);
      ASSERT_TRUE(actual.set_parameters(*parameters));
      ASSERT_TRUE(actual.begin(query));
      ASSERT_TRUE(gatherClientLightingTiles(actual, tiles, parallel ? workers->get() : caller, **materials, **liquids,
                                            environment, 13.0f));
      Lightmap output;
      ASSERT_TRUE(actual.calculate(output));
      ASSERT_EQ(output.size(), Vec2U(query.size()));
      ASSERT_EQ(output.span().size(), snapshot.size());
      ASSERT_EQ(std::memcmp(output.span().data(), snapshot.data(), output.span().size_bytes()), 0);
    }
  }
  ASSERT_TRUE(caller.finish());
}

}// namespace
TEST(CellularLightArray, PointObstacleSnapshotRefreshesMutableCellsAndStride) {
  check_point_obstacle_snapshot_refresh<ScalarLightTraits>();
  check_point_obstacle_snapshot_refresh<ColoredLightTraits>();
}
TEST(CellularLightArray, PointRaysMatchGeometricAttenuationInEveryOrientation) {
  check_point_ray_orientations<ScalarLightTraits>();
  check_point_ray_orientations<ColoredLightTraits>();
}

TEST(CellularLightArray, TinyGridsSeedAndLightWithoutASpreadSweep) {
  ScalarCellularLightArray array;
  auto parameters = LightParameters::create(2, 4, 1, 8, 1, 0, true);
  ASSERT_TRUE(parameters);
  array.set_parameters(*parameters);
  for (Vec2S size : {Vec2S(1, 1), Vec2S(1, 8), Vec2S(8, 1), Vec2S(2, 2)}) {
    ASSERT_TRUE(array.begin(size[0], size[1]));
    array.add_spread_light({{0.5f, 0.5f}, 0.75f});
    array.add_point_light({{0, 0}, 1.0f, 0, 0, 0, false});
    array.calculate(0, 0, std::numeric_limits<std::size_t>::max(), std::numeric_limits<std::size_t>::max());
    EXPECT_NEAR(array.get_light(0, 0), 1.75f - std::sqrt(0.5f) / 8.0f, 1e-6f);
  }
}

TEST(CellularLightArray, ReshapeClearsCellsAndSourcesAndRejectsInvalidSizesAtomically) {
  ColoredCellularLightArray array;
  auto parameters = LightParameters::create(0, 4, 1, 8, 1, 0, false);
  ASSERT_TRUE(parameters);
  array.set_parameters(*parameters);
  for (Vec2S size : {Vec2S(2, 8), Vec2S(8, 2), Vec2S(2, 3), Vec2S(4, 4)}) {
    ASSERT_TRUE(array.begin(size[0], size[1]));
    array.calculate(0, 0, size[0], size[1]);
    for (std::size_t i = 0; i < size[0] * size[1]; ++i) {
      EXPECT_EQ(array.cell_at_index(i).light, Vec3F());
      EXPECT_FALSE(array.cell_at_index(i).obstacle);
      array.cell_at_index(i) = {Vec3F::filled(static_cast<float>(i + 1)), true};
    }
    EXPECT_EQ(array.get_light(1, 0), Vec3F::filled(static_cast<float>(size[1] + 1)));
    array.add_spread_light({{0.5f, 0.5f}, Vec3F::filled(20)});
    array.add_point_light({{0.5f, 0.5f}, Vec3F::filled(10), 0, 0, 0, false});
    EXPECT_FALSE(array.begin(0, 3));
    EXPECT_FALSE(array.begin(std::numeric_limits<std::size_t>::max(), 2));
    auto int_max = static_cast<std::size_t>(std::numeric_limits<int>::max());
    EXPECT_FALSE(array.begin(int_max, int_max));
    EXPECT_TRUE(array.get_obstacle(0, 0));
    array.calculate(0, 0, 1, 1);
    EXPECT_EQ(array.get_light(0, 0), Vec3F::filled(30));
  }
}

TEST(CellularLightArray, PreseedingClipsTheFootprintBeforeIntegerConversion) {
  ScalarCellularLightArray array;
  auto parameters = LightParameters::create(0, 4, 1, 8, 1, 0, false);
  ASSERT_TRUE(parameters);
  array.set_parameters(*parameters);
  ASSERT_TRUE(array.begin(1, 1));
  for (std::size_t axis = 0; axis != 2; ++axis) {
    for (auto bits : {0x7f7fffffu, 0xff7fffffu, 0x7fc00001u, 0x7f800000u}) {
      // Inject an actual runtime payload into the queued aggregate. Passing
      // constant NaN as a float argument is not reliable with -fno-honor-nans.
      volatile std::uint32_t input = bits;
      ScalarCellularLightArray::SpreadLight source{{0, 0}, 10.0f};
      source.position[axis] = std::bit_cast<float>(static_cast<std::uint32_t>(input));
      ASSERT_EQ(std::bit_cast<std::uint32_t>(source.position[axis]), bits);
      array.add_spread_light(source);
    }
  }
  array.calculate(0, 0, 1, 1);
  EXPECT_EQ(array.get_light(0, 0), 0.0f);
  array.add_spread_light({{-0.25f, 0.5f}, 1.0f});
  array.calculate(0, 0, 1, 1);
  EXPECT_EQ(array.get_light(0, 0), 1.0f - 0.75f / 4.0f);
}

TEST(CellularLightArray, ColoredSpreadRetainsChannelRatiosDestinationMaximaAndNegativeCandidates) {
  ColoredCellularLightArray array;
  auto parameters = LightParameters::create(1, 4, 2, 8, 1, 0, false);
  ASSERT_TRUE(parameters);
  array.set_parameters(*parameters);
  // In a 3x3 grid only the center is a source in either sweep.
  // Use a non-unit peak so proportional normalization is observable. These
  // channels avoid diagonal subtraction ties under native reciprocal estimates.
  Vec3F color{16.0f, 0.5f, -0.25f};
  Vec3F existing{15.875f, -0.5f, -0.75f};
  for (bool obstacle : {false, true}) {
    ASSERT_TRUE(array.begin(3, 3));
    for (std::size_t i = 0; i < 9; ++i)
      array.cell_at_index(i).light = existing;
    array.cell(1, 1) = {color, obstacle};
    array.calculate(0, 0, 3, 3);
    for (int x = 0; x < 3; ++x) {
      for (int y = 0; y < 3; ++y) {
        if (x == 1 && y == 1)
          continue;
        float drop = obstacle ? 0.5f : 0.25f;
        if (x != 1 && y != 1)
          drop = static_cast<float>(drop * std::numbers::sqrt2);
        drop /= color[0];
        auto light = array.get_light(x, y);
        for (std::size_t channel = 0; channel < 3; ++channel)
          EXPECT_EQ(light[channel], std::max(existing[channel], color[channel] - color[channel] * drop));
      }
    }
  }
}


TEST(CellularLightingParameters, InvalidConfigurationAndBoundsPreservePreviousState) {
  Json valid = JsonObject{{"spreadPasses", 0}, {"spreadMaxAir", 4}, {"spreadMaxObstacle", 1},
      {"pointMaxAir", 6}, {"pointMaxObstacle", 2}, {"pointObstacleBoost", 0}, {"brightnessLimit", 1}};
  CellularLightingCalculator color;
  CellularLightIntensityCalculator intensity;
  ASSERT_TRUE(color.set_parameters(valid));
  ASSERT_TRUE(intensity.set_parameters(valid));
  RectI query(1, 2, 4, 5);
  ASSERT_TRUE(color.begin(query));
  ASSERT_TRUE(intensity.begin({1.5f, 2.5f}));
  auto color_region = color.calculation_region();
  auto intensity_region = intensity.calculation_region();
  auto reject = [&](String const& key, Json value) {
    SCOPED_TRACE(key.utf8());
    auto invalid = valid.set(key, std::move(value));
    ASSERT_TRUE(invalid);
    EXPECT_FALSE(color.set_parameters(*invalid));
    EXPECT_FALSE(intensity.set_parameters(*invalid));
  };
  reject("spreadPasses", -1);
  reject("spreadPasses", 0.5);
  reject("spreadPasses", 0x1p32);
  reject("spreadMaxAir", 0);
  reject("spreadMaxObstacle", -1);
  reject("pointMaxAir", 0x1p31);
  reject("pointMaxObstacle", std::numeric_limits<double>::infinity());
  // Supply a runtime IEEE payload even in builds that assume finite FP operands.
  volatile std::uint64_t nan_bits = 0x7ff8000000000001ull;
  Json nan = std::bit_cast<double>(std::uint64_t(nan_bits));
  auto payload = nan.to_double();
  ASSERT_TRUE(payload);
  ASSERT_EQ(std::bit_cast<std::uint64_t>(*payload), std::uint64_t(nan_bits));
  reject("pointObstacleBoost", std::move(nan));
  auto bad_limit = valid.set("brightnessLimit", -1);
  ASSERT_TRUE(bad_limit);
  EXPECT_FALSE(color.set_parameters(*bad_limit));
  ASSERT_TRUE(color.set_monochrome(true));
  ASSERT_TRUE(color.begin(query));
  EXPECT_EQ(color.calculation_region(), color_region);
  constexpr int lo = std::numeric_limits<int>::min(), hi = std::numeric_limits<int>::max();
  EXPECT_FALSE(color.begin(RectI(lo, 0, lo + 1, 1)));
  EXPECT_FALSE(color.begin(RectI(0, hi - 1, 1, hi)));
  EXPECT_FALSE(color.begin(RectI(lo + 16, 0, hi - 16, 1)));
  EXPECT_EQ(color.calculation_region(), color_region);
  EXPECT_FALSE(intensity.begin({0x1p31f, 0}));
  EXPECT_FALSE(intensity.begin({std::numeric_limits<float>::quiet_NaN(), 0}));
  EXPECT_EQ(intensity.calculation_region(), intensity_region);
}

TEST(Lightmap, ConstViewBorrowsOnlyLogicalRgbBytes) {
  Lightmap map(8, 3);
  Lightmap smaller(2, 1);
  map = smaller;
  auto const& owner = map;
  ImageView view = owner;
  EXPECT_EQ(view.size, Vec2U(2, 1));
  EXPECT_EQ(view.format, PixelFormat::RGB_F);
  std::array const channels{1.0f, -0.0f, 0.25f, 2.0f, -1.0f, 0.5f};
  ASSERT_EQ(map.span().size(), channels.size());
  std::ranges::copy(channels, map.span().begin());
  EXPECT_TRUE(std::ranges::equal(owner.span(), channels));
  EXPECT_TRUE(std::ranges::equal(std::as_bytes(view.data), std::as_bytes(std::span(channels))));
}

TEST(Lightmap, CopyMovePreservesEveryRgbComponent) {
  Lightmap original(7, 5);
  ASSERT_TRUE(std::ranges::all_of(original.span(), [](float value) { return value == 0.0f; }));
  for (unsigned y = 0; y < original.height(); ++y)
    for (unsigned x = 0; x < original.width(); ++x) {
      float value = 1.0f + y * original.width() + x;
      ASSERT_TRUE(original.set(x, y, Vec3F(value, value + 0.25f, value + 0.5f)));
    }
  std::vector<float> snapshot(std::from_range, original.span());
  Lightmap copy(original);
  EXPECT_TRUE(std::ranges::equal(copy.span(), snapshot));
  auto const& alias = original;
  original = alias;
  EXPECT_TRUE(std::ranges::equal(original.span(), snapshot));
  Lightmap assigned(1, 2);
  assigned = original;
  EXPECT_EQ(assigned.size(), original.size());
  EXPECT_TRUE(std::ranges::equal(assigned.span(), snapshot));
  ASSERT_TRUE(original.set(6, 4, Vec3F::filled(-1.0f)));
  EXPECT_EQ(*copy.get(6, 4), Vec3F(35.0f, 35.25f, 35.5f));

  Lightmap moved(std::move(copy));
  EXPECT_TRUE(copy.empty());
  EXPECT_EQ(copy.size(), Vec2U());
  EXPECT_TRUE(copy.span().empty());
  EXPECT_TRUE(std::ranges::equal(moved.span(), snapshot));
  auto& move_alias = moved;
  moved = std::move(move_alias);
  EXPECT_TRUE(std::ranges::equal(moved.span(), snapshot));
  assigned = std::move(moved);
  EXPECT_TRUE(moved.empty());
  EXPECT_TRUE(moved.span().empty());
  EXPECT_TRUE(std::ranges::equal(assigned.span(), snapshot));

  Lightmap small(2, 1);
  ASSERT_TRUE(small.set(0, 0, Vec3F(0.1f, 0.2f, 0.3f)));
  ASSERT_TRUE(small.set(1, 0, Vec3F(0.4f, 0.5f, 0.6f)));
  assigned = small;
  EXPECT_EQ(assigned.size(), small.size());
  EXPECT_TRUE(std::ranges::equal(assigned.span(), small.span()));
  assigned = Lightmap(0, 9);
  EXPECT_EQ(assigned.size(), Vec2U(0, 9));
  EXPECT_TRUE(assigned.empty());
  EXPECT_TRUE(assigned.span().empty());
  assigned = small;
  Lightmap empty;
  assigned = empty;
  EXPECT_EQ(assigned.size(), Vec2U());
  EXPECT_TRUE(assigned.span().empty());
  EXPECT_FALSE(assigned.get(0, 0));
  EXPECT_FALSE(assigned.set(0, 0, 1.0f));
  EXPECT_FALSE(assigned.add(0, 0, Vec3F::filled(1.0f)));
  EXPECT_FALSE(small.get(2, 0));
  EXPECT_FALSE(small.set(0, 1, Vec3F::filled(1.0f)));
  EXPECT_FALSE(small.add(2, 0, Vec3F::filled(1.0f)));
}

TEST(ClientLightingResult, RotatesOwnedMapsAndConsumesOnlyFreshResults) {
  ClientLightingResult completed;
  Lightmap pending(3, 2);
  std::ranges::fill(pending.span(), 1.25f);
  Lightmap displayed(2, 2);
  std::ranges::fill(displayed.span(), -3.0f);
  Vec2I position(-10, -20);
  EXPECT_FALSE(completed.take(displayed, position));
  EXPECT_EQ(position, Vec2I(-10, -20));
  EXPECT_TRUE(std::ranges::all_of(displayed.span(), [](float value) { return value == -3.0f; }));

  completed.publish(pending, {10, 20});
  EXPECT_TRUE(pending.empty());
  EXPECT_TRUE(std::ranges::all_of(displayed.span(), [](float value) { return value == -3.0f; }));
  ASSERT_TRUE(completed.take(displayed, position));
  EXPECT_EQ(position, Vec2I(10, 20));
  EXPECT_EQ(displayed.size(), Vec2U(3, 2));
  EXPECT_TRUE(std::ranges::all_of(displayed.span(), [](float value) { return value == 1.25f; }));
  EXPECT_FALSE(completed.take(displayed, position));
  EXPECT_EQ(position, Vec2I(10, 20));
  EXPECT_TRUE(std::ranges::all_of(displayed.span(), [](float value) { return value == 1.25f; }));

  pending = Lightmap(4, 3);
  std::ranges::fill(pending.span(), 2.5f);
  completed.publish(pending, {30, 40});
  EXPECT_EQ(pending.size(), Vec2U(2, 2));
  EXPECT_TRUE(std::ranges::all_of(pending.span(), [](float value) { return value == -3.0f; }));
  // Reuse the returned storage while the consumer still owns the first result.
  std::ranges::fill(pending.span(), 7.5f);
  EXPECT_TRUE(std::ranges::all_of(displayed.span(), [](float value) { return value == 1.25f; }));
  completed.publish(pending, {50, 60});
  ASSERT_TRUE(completed.take(displayed, position));
  EXPECT_EQ(position, Vec2I(50, 60));
  EXPECT_EQ(displayed.size(), Vec2U(2, 2));
  EXPECT_TRUE(std::ranges::all_of(displayed.span(), [](float value) { return value == 7.5f; }));
  EXPECT_EQ(pending.size(), Vec2U(4, 3));
  EXPECT_TRUE(std::ranges::all_of(pending.span(), [](float value) { return value == 2.5f; }));

  completed.publish(pending, {70, 80});
  completed.reset();
  EXPECT_FALSE(completed.take(displayed, position));
  EXPECT_EQ(position, Vec2I(50, 60));
  EXPECT_TRUE(std::ranges::all_of(displayed.span(), [](float value) { return value == 7.5f; }));
  Lightmap empty;
  completed.publish(empty, {90, 100});
  ASSERT_TRUE(completed.take(displayed, position));
  EXPECT_EQ(position, Vec2I(90, 100));
  EXPECT_TRUE(displayed.empty());
  EXPECT_TRUE(displayed.span().empty());
  EXPECT_FALSE(completed.take(displayed, position));
}

const GameTestRegistration
  clientLightingReusedOutput("ClientLighting", "ReusedOutputMatchesFreshCalculations", [](GameTest& fixture) {
    auto assets = fixture.root().assets();
    ASSERT_TRUE(assets);
    auto parameters = (*assets)->json("/lighting.config:lighting");
    ASSERT_TRUE(parameters);
    CellularLightingCalculator reused;
    Lightmap output;
    unsigned iteration = 0;
    for (bool monochrome : {false, true, false}) {
      ASSERT_TRUE(reused.set_monochrome(monochrome));
      ASSERT_TRUE(reused.set_parameters(*parameters));
      for (auto size : {Vec2I(17, 13), Vec2I(31, 19), Vec2I(4, 3), Vec2I(0, 0), Vec2I(17, 13)}) {
        RectI query(Vec2I(10, 20), Vec2I(10, 20) + size);
        CellularLightingCalculator fresh(monochrome);
        ASSERT_TRUE(fresh.set_parameters(*parameters));
        auto populate = [&](CellularLightingCalculator& calculator) {
          ASSERT_TRUE(calculator.begin(query));
          auto region = calculator.calculation_region();
          for (int x = region.x_min(); x < region.x_max(); ++x)
            for (int y = region.y_min(); y < region.y_max(); ++y) {
              float value = ((x * 7 + y * 11 + iteration) % 17) * 0.125f;
              calculator.set_cell_index(calculator.base_index_for({x, y}),
                  Vec3F(value, value * 0.6f, value * 0.2f), (x + y) % 3 == 0);
            }
          addBoundaryLights(calculator, query);
        };
        populate(fresh);
        populate(reused);
        Lightmap expected;
        ASSERT_TRUE(fresh.calculate(expected));
        std::ranges::fill(output.span(), -100.0f);
        ASSERT_TRUE(reused.calculate(output));
        EXPECT_EQ(output.size(), Vec2U(size));
        EXPECT_TRUE(std::ranges::equal(output.span(), expected.span()));
        ++iteration;
      }
    }
  });

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
    ASSERT_TRUE(regionProbe.set_parameters(*parameters));
    ASSERT_TRUE(regionProbe.begin(RectI(-8, -4, 48, 40)));
    unsigned worldWidth = static_cast<unsigned>(regionProbe.calculation_region().width()) + 32;
    ClientTileSectorArray tiles({worldWidth, 96});
    auto sectors = tiles.valid_sectors_for(RectI(0, 0, static_cast<int>(worldWidth), 96));
    ASSERT_TRUE(sectors);
    for (auto const& sector : *sectors) {
      if (sector != ClientTileSectorArray::Sector{1, 1})
        tiles.load_default_sector(sector);
    }
    ASSERT_TRUE(tiles.tile_eval(RectI(0, 0, static_cast<int>(worldWidth), 96),
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
          ASSERT_TRUE(oracle.set_parameters(*config));
          ASSERT_TRUE(oracle.begin(query));
          ASSERT_LT(oracle.calculation_region().width(), static_cast<int>(worldWidth));
          gatherTileOracle(oracle, tiles, **materials, **liquids, environment, 13.0f);
          addBoundaryLights(oracle, query);
          Lightmap expected;
          ASSERT_TRUE(oracle.calculate(expected));
          std::vector<float> snapshot(std::from_range, expected.span());
          for (bool parallel : {false, true}) {
            CellularLightingCalculator actual(monochrome);
            ASSERT_TRUE(actual.set_parameters(*config));
            ASSERT_TRUE(actual.begin(query));
            ASSERT_TRUE(gatherClientLightingTiles(actual, tiles, parallel ? workers->get() : caller, **materials,
                                                  **liquids, environment, 13.0f));
            addBoundaryLights(actual, query);
            Lightmap output;
            ASSERT_TRUE(actual.calculate(output));
            EXPECT_EQ(output.size(), Vec2U(query.size()));
            ASSERT_EQ(output.span().size(), snapshot.size());
            EXPECT_EQ(std::memcmp(output.span().data(), snapshot.data(), output.span().size_bytes()), 0);
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
    ASSERT_TRUE(calculator.set_parameters(*parameters));
    RectI query(10, 20, 27, 33);
    for (bool monochrome : {false, true, false}) {
      ASSERT_TRUE(calculator.set_monochrome(monochrome));
      ASSERT_TRUE(calculator.begin(query));
      auto region = calculator.calculation_region();
      for (int x = region.x_min(); x < region.x_max(); ++x) {
        for (int y = region.y_min(); y < region.y_max(); ++y)
          calculator.set_cell_index(calculator.base_index_for({x, y}), {0.3f, 0.5f, 0.7f}, (x + y) % 3 == 0);
      }
      addBoundaryLights(calculator, query);
      Lightmap lit;
      ASSERT_TRUE(calculator.calculate(lit));
      EXPECT_TRUE(std::ranges::any_of(lit.span(), [](float value) { return value > 0.0f; }));
      ASSERT_TRUE(calculator.begin(query));
      Lightmap dark;
      ASSERT_TRUE(calculator.calculate(dark));
      EXPECT_EQ(dark.size(), Vec2U(query.size()));
      EXPECT_TRUE(std::ranges::all_of(dark.span(), [](float value) { return value == 0.0f; }));
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

namespace {

template <typename Traits>
void check_boundary_obstacle_coverage() {
  typename Traits::Value value;
  if constexpr (std::same_as<Traits, ScalarLightTraits>)
    value = 1.0f;
  else
    value = Vec3F(1.0f, 0.5f, 0.25f);
  struct Ray {
    Vec2F source;
    Vec2I target;
    float covered_cells; // Wu coverage before circularization.
  };
  Ray const rays[]{
      {{0, 3.5f}, {2, 3}, 2.5f}, {{7, 3.5f}, {4, 3}, 2.5f},
      {{3.5f, 0}, {3, 2}, 2.5f}, {{3.5f, 7}, {3, 4}, 2.5f},
      {{0, 0}, {2, 2}, 2.5f}, {{7, 0}, {4, 2}, 2.5f},
      {{0, 7}, {2, 4}, 2.5f}, {{7, 7}, {4, 4}, 2.5f},
      // Exterior minor neighbors carry nonzero weight: the in-grid
      // contributions are 0.25 + 0.75 + 0.5, not two whole cells.
      {{3.5f, 0}, {5, 0}, 1.5f}, {{0, 3.5f}, {0, 5}, 1.5f},
      // At the far edge the last sample's zero-weight neighbor is outside.
      {{0, 7}, {2, 7}, 2.5f}, {{7, 0}, {7, 2}, 2.5f},
  };
  // The shorter obstacle range saturates the ray before all samples contribute.
  for (float obstacle_range : {16.0f, 0.25f}) {
    auto params = LightParameters::create(0, 32, 16, 32, obstacle_range, 0, false);
    ASSERT_TRUE(params);
    CellularLightArray<Traits> array;
    array.set_parameters(*params);
    for (auto const& ray : rays) {
      ASSERT_TRUE(array.begin(8, 8));
      for (std::size_t x = 0; x < 8; ++x)
        for (std::size_t y = 0; y < 8; ++y)
          array.set_obstacle(x, y, true);
      array.add_point_light({ray.source, value, 0, 0, 0, false});
      auto x = ray.target[0];
      auto y = ray.target[1];
      array.calculate(x, y, x + 1, y + 1);
      Vec2F delta = Vec2F(x + 0.5f, y + 0.5f) - ray.source;
      float distance = delta.magnitude();
      float major_distance = std::max(std::fabs(delta[0]), std::fabs(delta[1]));
      float expected = 1.0f - distance / 32.0f - ray.covered_cells / obstacle_range * distance / major_distance;
      EXPECT_NEAR(Traits::max_intensity(array.get_light(x, y)), std::max(0.0f, expected), 0.000001f)
          << "source=" << ray.source[0] << ',' << ray.source[1] << " target=" << x << ',' << y
          << " obstacle range=" << obstacle_range;
    }
  }
}

template <typename Traits>
void check_point_center_and_beam() {
  typename Traits::Value value;
  typename Traits::Value ambient;
  if constexpr (std::same_as<Traits, ScalarLightTraits>) {
    value = 0.75f;
    ambient = 0.125f;
  } else {
    value = Vec3F(0.75f, 0.5f, 0.25f);
    ambient = Vec3F::filled(0.125f);
  }
  for (bool additive : {false, true}) {
    auto params = LightParameters::create(0, 4, 4, 4, 4, 0, additive);
    ASSERT_TRUE(params);
    for (bool as_spread : {false, true}) {
      CellularLightArray<Traits> array;
      array.set_parameters(*params);
      ASSERT_TRUE(array.begin(8, 8));
      array.set_light(3, 3, ambient);
      array.set_light(2, 3, ambient);
      array.add_point_light({{3.5f, 3.5f}, value, 0.0001f, 0, 0, as_spread});
      array.calculate(2, 3, 4, 4);
      EXPECT_EQ(array.get_light(3, 3), value + ambient);
      // One cell behind the beam. Only RGB enables this exact cutoff value.
      float beam_drop = std::same_as<Traits, ScalarLightTraits> ? 0.0f : 0.0002f;
      float contribution = 0.75f - (0.25f + beam_drop);
      float expected = additive ? 0.125f + contribution * (as_spread ? 0.15f : 1.0f) : contribution;
      EXPECT_NEAR(Traits::max_intensity(array.get_light(2, 3)), expected, 0.000001f);
    }
  }
}

} // namespace

TEST(CellularLightArray, BoundaryPointRaysIgnoreOnlyExteriorCoverage) {
  check_boundary_obstacle_coverage<ScalarLightTraits>();
  check_boundary_obstacle_coverage<ColoredLightTraits>();
}

TEST(CellularLightArray, ExactCenterAlwaysAddsAndBeamThresholdDependsOnTraits) {
  check_point_center_and_beam<ScalarLightTraits>();
  check_point_center_and_beam<ColoredLightTraits>();
}

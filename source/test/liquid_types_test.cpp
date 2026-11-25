import std;
import star.algorithm;
import star.assets;
import star.cellular_liquid;
import star.dungeon_generator;
import star.map;
import star.either;
import star.format;
import star.file;
import star.game_test;
import star.hash;
import star.liquids_database;
import star.json;
import star.logging;
import star.image_metadata_database;
import star.material_database;
import star.rect;
import star.root;
import star.vector;
import star.string;
import star.liquid_types;
import star.color;
import star.entity_splash;
import star.list;
import star.particle;
import star.particle_database;
import star.random;
import star.world_server;
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

const GameTestRegistration liquid_default_values("LiquidStoreTest", "DefaultsAndFactoriesPreserveEmptyAndSourceState", [](GameTest& fixture) {
  LiquidLevel level;
  fixture.check(level.liquid == empty_liquid_id && level.level == 0.0f, "default liquid level is empty");
  LiquidNetUpdate net;
  level = net.liquid_level();
  fixture.check(net.liquid == empty_liquid_id && net.level == 0
      && level.liquid == empty_liquid_id && level.level == 0.0f, "default network update decodes as empty");

  for (auto const& store : {LiquidStore{}, LiquidStore::filled(empty_liquid_id, 0.5f, 4.0f),
                            LiquidStore::endless(empty_liquid_id, 4.0f)}) {
    fixture.check(store.liquid == empty_liquid_id && store.level == 0.0f && store.pressure == 0.0f && !store.source,
                  "default and empty factories produce an ordinary empty store");
  }
  auto store = LiquidStore::filled(7, 0.5f);
  fixture.check(store.liquid == 7 && store.level == 0.5f && store.pressure == 0.5f && !store.source,
                "filled cells default pressure to level");
  store = LiquidStore::endless(7, 4.0f);
  fixture.check(store.liquid == 7 && store.level == 1.0f && store.pressure == 4.0f && store.source,
                "endless cells start full at the requested pressure");
});

const GameTestRegistration liquid_drain_values("LiquidStoreTest", "DrainsDoNotCreateLiquidOrAffectSources", [](GameTest& fixture) {
  constexpr auto nan = std::numeric_limits<float>::quiet_NaN();
  constexpr auto infinity = std::numeric_limits<float>::infinity();
  auto store = LiquidStore::filled(7, 0.75f, 3.0f);
  LiquidLevel level = store;
  for (auto amount : {0.0f, -0.25f, nan, infinity}) {
    level.drain(amount);
    store.drain(amount);
    fixture.check(level.liquid == 7 && level.level == 0.75f && store.liquid == 7
        && store.level == 0.75f && store.pressure == 3.0f, "nonpositive and nonfinite drains change nothing");
  }

  level.drain(0.25f);
  store.drain(0.25f);
  fixture.check(level.level == 0.5f && store.level == 0.5f && store.pressure == 3.0f,
                "partial drains preserve pressure");
  level.drain(2.0f);
  store.drain(2.0f);
  fixture.check(level.liquid == empty_liquid_id && level.level == 0.0f && store.liquid == empty_liquid_id
      && store.level == 0.0f && store.pressure == 0.0f, "overdrain clears liquid and pressure without going below zero");
  store.drain(1.0f);
  fixture.check(store.level == 0.0f, "draining an empty tile leaves it empty");

  auto source = LiquidStore::endless(7, 3.0f);
  source.drain(2.0f);
  fixture.check(source.liquid == 7 && source.level == 1.0f && source.pressure == 3.0f && source.source,
                "draining a source leaves it untouched");
});

const GameTestRegistration liquid_wire_dirtiness("LiquidStoreTest", "OnlyWireChangesAreDirtyWhilePhysicsKeepsFullPrecision", [](GameTest& fixture) {
  auto store = LiquidStore::filled(1, 0.5f);
  fixture.check(store.net_update().level == 127, "wire levels truncate rather than round");
  fixture.check(!store.update(1, 0.5f, 2.0f) && store.pressure == 2.0f, "pressure alone does not dirty the wire");
  fixture.check(!store.update(1, 0.5001f, 1.0f) && store.level == 0.5001f && store.pressure == 1.0f,
                "within-byte changes update only physics");
  fixture.check(store.update(1, 0.502f, 1.0f) && store.net_update().level == 128, "crossing a byte boundary dirties the wire");
  fixture.check(store.update(2, 0.502f, 1.0f), "a different ID dirties the wire with the same level");
  auto decoded = store.net_update().liquid_level();
  fixture.check(decoded.liquid == 2 && decoded.level == 128.0f / 255.0f, "decoding retains the ID and restores the byte fraction");

  fixture.check(store.update(2, 1.1f, 3.0f) && store.net_update().level == 255 && store.level == 1.1f,
                "overfull levels saturate the wire but not physics");
  fixture.check(!store.update(2, 1.3f, 4.0f) && store.level == 1.3f && store.pressure == 4.0f,
                "a second overfull level leaves the saturated wire unchanged");
});

const GameTestRegistration liquid_source_updates("LiquidStoreTest", "SourcesAreMonotonicAndAnExhaustedFlowClearsItsIdentity", [](GameTest& fixture) {
  auto source = LiquidStore::endless(7, 3.0f);
  fixture.check(!source.update(8, 2.0f, 4.0f) && source.liquid == 7 && source.level == 1.0f && source.pressure == 3.0f,
                "a different ID cannot overwrite a source");
  fixture.check(!source.update(7, 0.25f, 2.0f) && source.level == 1.0f && source.pressure == 3.0f,
                "a source does not lose level or pressure");
  fixture.check(!source.update(7, 0.25f, 4.0f) && source.level == 1.0f && source.pressure == 4.0f,
                "a source can gain pressure without changing level");
  fixture.check(!source.update(7, 1.25f, 2.0f) && source.level == 1.25f && source.pressure == 4.0f && source.source,
                "a source can gain level without losing pressure or dirtying a saturated byte");

  auto flow = LiquidStore::filled(7, 0.001f, 3.0f);
  fixture.check(flow.net_update().level == 0, "a small positive level can encode to zero");
  fixture.check(flow.update(7, -0.25f, 4.0f) && flow.liquid == empty_liquid_id
      && flow.level == -0.25f && flow.pressure == 0.0f && flow.net_update().level == 0,
      "an exhausted flow changes ID and clears pressure without rewriting the supplied finite level");
});

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
Json splash_config() {
  return JsonObject{{"splashSpeedMin", 4}, {"splashMinWaterLevel", 0.5f},
                    {"splashBottomSensor", JsonArray{-0.5f, 0}}, {"splashTopSensor", JsonArray{0, 0.5f}},
                    {"numSplashParticles", 3}, {"splashYVelocityFactor", 2},
                    {"splashParticle", JsonObject{{"type", "text"}, {"text", "splash"},
                                                 {"velocity", JsonArray{3, -9}}, {"timeToLive", 7}}},
                    {"splashParticleVariance", JsonObject{{"position", JsonArray{2, 1}},
                                                         {"velocity", JsonArray{1, 2}}, {"timeToLive", 3}}}};
}

const GameTestRegistration splash_config_errors("EntitySplashTest", "RejectsMalformedConfig", [](GameTest& fixture) {
  auto config = splash_config();
  for (auto const& [key, value] : JsonObject{{"splashSpeedMin", false}, {"splashMinWaterLevel", "bad"},
                                            {"splashBottomSensor", JsonArray{1}}, {"splashTopSensor", false},
                                            {"splashYVelocityFactor", "bad"}, {"splashParticle", false},
                                            {"splashParticleVariance", JsonObject{{"type", "unknown"}}}}) {
    auto changed = config.set(key, value);
    ASSERT_TRUE(changed);
    EXPECT_FALSE(EntitySplashConfig::create(fixture.root(), *changed)) << key.utf8();
  }
  for (std::int64_t count : std::array<std::int64_t, 2>{-1, std::int64_t{std::numeric_limits<int>::max()} + 1}) {
    auto changed = config.set("numSplashParticles", count);
    ASSERT_TRUE(changed);
    EXPECT_FALSE(EntitySplashConfig::create(fixture.root(), *changed)) << count;
  }
});

const GameTestRegistration splash_world_cases("EntitySplashTest", "WorldSensorsParticleOrderAndRandomStream", [](GameTest& fixture) {
  auto& root = fixture.root();
  auto file = File::ephemeral_file(root.random());
  ASSERT_TRUE(file);
  auto created = WorldServer::create(root, Vec2U(2048, 2048), *file);
  ASSERT_TRUE(created) << created.error().what();
  auto& world = **created;
  auto close = make_scope_exit([&] { EXPECT_TRUE(world.close()); });
  Vec2F position(1024.25f, 1024.75f);
  Vec2I bottom(1023, 1024), top(1024, 1025);
  ASSERT_TRUE(world.generateRegion(RectI::with_size(bottom, Vec2I(4, 4))));
  auto database = root.liquids_database();
  ASSERT_TRUE(database);
  auto water = (*database)->liquid_id("water");
  auto lava = (*database)->liquid_id("lava");
  ASSERT_TRUE(water && lava);
  auto config = EntitySplashConfig::create(root, splash_config());
  ASSERT_TRUE(config) << config.error().what();
  ASSERT_TRUE(world.setLiquid(bottom, *water, 0.75f, 1));
  ASSERT_TRUE(world.setLiquid(top, *lava, 0.25f, 1));
  constexpr std::uint64_t seed = 31097;
  auto expect_empty = [&](float speed) {
    root.random().seed(seed);
    auto particles = config->generate_particles(position, {99, speed}, world);
    ASSERT_TRUE(particles) << particles.error().what();
    EXPECT_TRUE(particles->empty());
    EXPECT_EQ(root.random().next_u32(), RandomService(seed).next_u32());
  };
  expect_empty(3.99f);
  config->splash_min_water_level = 0.51f;
  expect_empty(-4);
  config->splash_min_water_level = 0.5f;
  auto expect_batch = [&](float speed, LiquidId id) {
    root.random().seed(seed);
    auto particles = config->generate_particles(position, {99, speed}, world);
    ASSERT_TRUE(particles) << particles.error().what();
    ASSERT_EQ(particles->size(), 3u);
    auto next_draw = root.random().next_u32();
    RandomService reference(seed);
    for (unsigned i = 0; i < 3 * 11; ++i)
      reference.uniform_float();
    EXPECT_EQ(next_draw, reference.next_u32());
    root.random().seed(seed);
    auto settings = (*database)->liquid_settings(id);
    ASSERT_TRUE(settings);
    for (auto const& particle : *particles) {
      auto expected = config->splash_particle;
      expected.position = position;
      expected.velocity = {3, std::fabs(speed) * 2};
      expected.color = Color::rgba(settings->get().liquid_color);
      expected.applyVariance(root, config->splash_particle_variance);
      auto expected_json = expected.toJson();
      auto actual_json = particle.toJson();
      ASSERT_TRUE(expected_json && actual_json);
      EXPECT_EQ(*actual_json, *expected_json);
    }
  };
  expect_batch(4, *water);
  expect_batch(-4, *water);
  config->splash_top_sensor = config->splash_bottom_sensor;
  expect_empty(4);
  config->splash_min_water_level = 0;
  auto zero_variance = Particle::create(root, JsonObject{});
  ASSERT_TRUE(zero_variance);
  config->splash_particle_variance = std::move(*zero_variance);
  expect_batch(4, *water);
  config->num_splash_particles = 0;
  expect_empty(4);
  config->num_splash_particles = 3;
  ASSERT_TRUE(world.setLiquid(bottom, empty_liquid_id, 0, 0));
  expect_empty(4);
  config->splash_top_sensor = {0, 0.5f};
  config->splash_min_water_level = -0.25f;
  expect_batch(4, *lava);
  auto unknown = std::numeric_limits<LiquidId>::max();
  ASSERT_FALSE((*database)->is_valid_liquid_id(unknown));
  ASSERT_TRUE(world.setLiquid(bottom, unknown, 1, 1));
  auto failed = config->generate_particles(position, {0, 4}, world);
  EXPECT_FALSE(failed);
});

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

std::expected<std::shared_ptr<Assets>, StarException> open_liquid_fixture_assets(
    GameTest& fixture, String const& directory, Logger& logger) {
  auto preload = File::write_file(String("[]"), File::relative_to(directory, "preload.config"));
  if (!preload)
    return std::unexpected(std::move(preload.error()));
  Assets::Settings settings{};
  settings.worker_pool_size = 1;
  settings.asset_time_to_live = 10;
  return Assets::create(logger, fixture.root().random(), fixture.root().logMap(), settings, {directory});
}

const GameTestRegistration liquid_bucket_config("LiquidConfigTest", "RequiresFinitePositiveBucketSize", [](GameTest& fixture) {
  auto directory = File::temporary_directory(fixture.root().random());
  if (!fixture.check(bool(directory), "create bucket config fixture directory")) return;
  auto cleanup = make_scope_exit([&] { fixture.check(bool(File::remove_all(*directory)), "remove bucket config fixture"); });
  auto items = File::relative_to(*directory, "items");
  if (!fixture.check(bool(File::create_directories(items)), "create bucket config asset directory")) return;
  auto path = File::relative_to(items, "defaultParameters.config");
  Logger logger;
  for (auto const& [value, valid] : std::array<std::pair<std::string_view, bool>, 7>{{
      {"0.75", true}, {"0", false}, {"-1", false}, {"1e-100", false}, {"1e100", false},
      {"1e99999", false}, {R"("invalid")", false}}}) {
    if (!fixture.check(bool(File::write_file(
        String(R"({"liquidItems":{"bucketSize":)") + String(value) + "}}", path)), "write bucket size config")) return;
    auto assets = open_liquid_fixture_assets(fixture, *directory, logger);
    if (!fixture.check(bool(assets), assets ? "open bucket fixture assets" : assets.error().what())) return;
    auto loaded = load_liquid_bucket_size(**assets);
    if (valid)
      fixture.check(loaded && *loaded == 0.75f, "preserve a valid fractional bucket size");
    else
      fixture.check(!loaded, Star::format("reject bucket size {}", value));
  }
});

const GameTestRegistration liquid_material_config("LiquidConfigTest", "ConsumptionIsNonnegativeAndFitsFloat", [](GameTest& fixture) {
  auto& root = fixture.root();
  auto directory = File::temporary_directory(root.random());
  if (!fixture.check(bool(directory), "create material reaction fixture directory")) return;
  auto cleanup = make_scope_exit([&] { fixture.check(bool(File::remove_all(*directory)), "remove material reaction fixture"); });
  auto tiles = File::relative_to(*directory, "tiles");
  if (!fixture.check(bool(File::create_directories(tiles)), "create damage config asset directory") ||
      !fixture.check(bool(File::write_file(String("[]"), File::relative_to(*directory, "metamaterials.config"))),
                     "write empty metamaterial list") ||
      !fixture.check(bool(File::write_file(
          String(R"({"defaultFootstepSound":"/fixture.ogg"})"), File::relative_to(*directory, "client.config"))),
          "write default footstep configuration") ||
      !fixture.check(bool(File::write_file(
          String(R"({"damageFactors":{},"damageRecovery":0})"), File::relative_to(tiles, "defaultDamage.config"))),
          "write material damage defaults")) return;
  auto particles = root.particleDatabase();
  auto image_metadata = root.image_metadata_database();
  if (!fixture.check(particles && image_metadata, "load material catalog dependencies")) return;
  auto with_interactions = [](String const& header, std::string_view second) {
    return header + R"(,"liquidInteractions":[{"liquidId":1},{"liquidId":2,"consumeLiquid":)"
        + String(second) + R"(},{"liquidId":3,"consumeLiquid":0.25}]})";
  };
  Logger logger;
  for (auto const& [material, mod] : std::array<std::pair<std::string_view, std::string_view>, 5>{{
      {"0", "0"}, {"-0.25", "0"}, {"1e100", "0"}, {"0", "-0.25"}, {"0", "1e99999"}}}) {
    if (!fixture.check(bool(File::write_file(
        with_interactions(R"({"materialId":1,"materialName":"fixture","category":"fixture")", material),
        File::relative_to(*directory, "fixture.material"))), "write material reactions") ||
        !fixture.check(bool(File::write_file(
        with_interactions(R"({"modId":1,"modName":"fixture")", mod),
        File::relative_to(*directory, "fixture.matmod"))), "write material mod reactions")) return;
    auto assets = open_liquid_fixture_assets(fixture, *directory, logger);
    if (!fixture.check(bool(assets), assets ? "open material fixture assets" : assets.error().what())) return;
    auto database = MaterialDatabase::create(*assets, *particles, **image_metadata, logger, root.random());
    if (material == "0" && mod == "0") {
      if (!fixture.check(bool(database), database ? "load valid material reactions" : database.error().what())) return;
      for (LiquidId id : {1, 2, 3}) {
        auto material_rule = (*database)->liquidMaterialInteraction(id, 1);
        auto mod_rule = (*database)->liquidModInteraction(id, 1);
        float expected = id == 3 ? 0.25f : 0.0f;
        fixture.check(material_rule && material_rule->consumeLiquid == expected, "preserve valid material consumption");
        fixture.check(mod_rule && mod_rule->consumeLiquid == expected, "preserve valid mod consumption");
      }
    } else {
      fixture.check(!database && database.error().is<MaterialException>(),
                    Star::format("reject reaction amounts material={}, mod={}", material, mod));
    }
  }
});

std::expected<std::shared_ptr<LiquidsDatabase>, StarException> liquids_fixture(
    GameTest& fixture, String const& directory, Json const& config,
    std::initializer_list<std::pair<String, Json>> liquids = {}) {
  auto created = File::create_directories(directory);
  if (!created)
    return std::unexpected(std::move(created.error()));
  auto contents = config.print_json();
  if (!contents)
    return std::unexpected(std::move(contents.error()));
  auto written = File::write_file(*contents, File::relative_to(directory, "liquids.config"));
  if (!written)
    return std::unexpected(std::move(written.error()));
  for (auto const& [filename, liquid] : liquids) {
    auto contents = liquid.print_json();
    if (!contents)
      return std::unexpected(std::move(contents.error()));
    auto written = File::write_file(*contents, File::relative_to(directory, filename));
    if (!written)
      return std::unexpected(std::move(written.error()));
  }
  Logger logger;
  auto assets = open_liquid_fixture_assets(fixture, directory, logger);
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  auto materials = fixture.root().materialDatabase();
  if (!materials)
    return std::unexpected(std::move(materials.error()));
  return LiquidsDatabase::create(**assets, **materials);
}

Json liquid_engine_config() {
  return JsonObject{{"lateralMoveFactor", 1},
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
}

const GameTestRegistration
  liquid_engine_fields("LiquidsDatabaseTest", "RequiredFloatFieldsUseAssetConfiguration", [](GameTest& fixture) {
    auto directory = File::temporary_directory(fixture.root().random());
    if (!fixture.check(bool(directory), "create isolated liquid test directory"))
      return;
    auto cleanup = make_scope_exit([&] { fixture.check(bool(File::remove_all(*directory)), "remove liquid test directory"); });
    auto engine = liquid_engine_config();
    auto valid = liquids_fixture(fixture, File::relative_to(*directory, "valid"),
                                JsonObject{{"liquidEngineParameters", engine}, {"backgroundDrain", 12}});
    if (!fixture.check(bool(valid), valid ? "load liquid engine fields" : valid.error().what()))
      return;
    auto parameters = (*valid)->liquid_engine_parameters();
    fixture.check(parameters.lateral_move_factor == 1, "read lateralMoveFactor");
    fixture.check(parameters.spread_overfill_up_factor == 2, "read spreadOverfillUpFactor");
    fixture.check(parameters.spread_overfill_lateral_factor == 3, "read spreadOverfillLateralFactor");
    fixture.check(parameters.spread_overfill_down_factor == 4, "read spreadOverfillDownFactor");
    fixture.check(parameters.pressure_equalize_factor == 5, "read pressureEqualizeFactor");
    fixture.check(parameters.pressure_move_factor == 6, "read pressureMoveFactor");
    fixture.check(parameters.maximum_pressure_level_imbalance == 7, "read maximumPressureLevelImbalance");
    fixture.check(parameters.minimum_liven_pressure_change == 8, "read minimumLivenPressureChange");
    fixture.check(parameters.minimum_liven_level_change == 9, "read minimumLivenLevelChange");
    fixture.check(parameters.minimum_liquid_level == 10, "read minimumLiquidLevel");
    fixture.check(parameters.interact_transformation_level == 11, "read interactTransformationLevel");
    fixture.check((*valid)->background_drain() == 12, "read backgroundDrain");

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
      auto field = std::string(name);
      for (bool missing : {true, false}) {
        auto changed = missing ? engine.erase(name) : engine.set(name, "not a float");
        if (!fixture.check(bool(changed), field))
          return;
        auto loaded = liquids_fixture(fixture, File::relative_to(*directory, Star::format("case{}", index++)),
                                     JsonObject{{"liquidEngineParameters", *changed}, {"backgroundDrain", 12}});
        fixture.check(!loaded && loaded.error().is<JsonException>(),
                      (missing ? "missing " : "wrong type for ") + field);
      }
    }

    for (bool missing : {true, false}) {
      Json config = missing ? Json(JsonObject{{"liquidEngineParameters", engine}})
                            : Json(JsonObject{{"liquidEngineParameters", engine}, {"backgroundDrain", false}});
      auto outer = liquids_fixture(
        fixture, File::relative_to(*directory, missing ? String("outerMissing") : String("outerType")), config);
      fixture.check(!outer && outer.error().is<JsonException>(),
                    missing ? "missing backgroundDrain" : "wrong type for backgroundDrain");
    }
  });

JsonObject liquid_asset(std::int64_t id, String name, std::int64_t tick_delta = 1, JsonArray interactions = {}) {
  return JsonObject{{"liquidId", id},
                    {"name", std::move(name)},
                    {"tickDelta", tick_delta},
                    {"color", JsonArray{0, 0, 0, 255}},
                    {"interactions", std::move(interactions)}};
}

const GameTestRegistration liquid_catalog_identity(
    "LiquidsDatabaseTest", "RejectsDuplicateAndReservedIdentities", [](GameTest& fixture) {
  auto directory = File::temporary_directory(fixture.root().random());
  if (!fixture.check(bool(directory), "create isolated liquid test directory"))
    return;
  auto cleanup = make_scope_exit([&] { fixture.check(bool(File::remove_all(*directory)), "remove liquid test directory"); });
  Json config = JsonObject{{"liquidEngineParameters", liquid_engine_config()}, {"backgroundDrain", 0}};
  auto expect_rejected = [&](String const& reason, Json first, Json second) {
    auto catalog = liquids_fixture(fixture, File::relative_to(*directory, reason), config,
                                   {{"a.liquid", std::move(first)}, {"b.liquid", std::move(second)}});
    fixture.check(!catalog && catalog.error().is<LiquidException>(), reason.utf8());
  };
  expect_rejected("duplicate-id", liquid_asset(1, "first"), liquid_asset(1, "second"));
  expect_rejected("duplicate-name", liquid_asset(1, "first"), liquid_asset(2, "first"));
  expect_rejected("duplicate-pair", liquid_asset(1, "first"), liquid_asset(1, "first"));
  expect_rejected("reserved-id", liquid_asset(1, "first"), liquid_asset(0, "notempty"));
  expect_rejected("reserved-name", liquid_asset(1, "first"), liquid_asset(2, "empty"));
});

const GameTestRegistration liquid_catalog_widths(
    "LiquidsDatabaseTest", "RejectsOutOfRangeLiquidIdsAndTickDelta", [](GameTest& fixture) {
  auto directory = File::temporary_directory(fixture.root().random());
  if (!fixture.check(bool(directory), "create isolated liquid test directory"))
    return;
  auto cleanup = make_scope_exit([&] { fixture.check(bool(File::remove_all(*directory)), "remove liquid test directory"); });
  Json config = JsonObject{{"liquidEngineParameters", liquid_engine_config()}, {"backgroundDrain", 0}};
  auto expect_rejected = [&](String const& reason, Json liquid) {
    auto catalog = liquids_fixture(fixture, File::relative_to(*directory, reason), config,
                                   {{"test.liquid", std::move(liquid)}});
    fixture.check(!catalog && catalog.error().is<LiquidException>(), reason.utf8());
  };
  expect_rejected("liquid-id-overflow", liquid_asset(256, "test"));
  expect_rejected("interaction-target-overflow",
                  liquid_asset(1, "test", 1, {JsonObject{{"liquid", 256}, {"liquidResult", 1}}}));
  expect_rejected("interaction-result-overflow",
                  liquid_asset(1, "test", 1, {JsonObject{{"liquid", 1}, {"liquidResult", 256}}}));
  expect_rejected("negative-interaction-target",
                  liquid_asset(1, "test", 1, {JsonObject{{"liquid", -1}, {"liquidResult", 1}}}));
  expect_rejected("negative-interaction-result",
                  liquid_asset(1, "test", 1, {JsonObject{{"liquid", 1}, {"liquidResult", -1}}}));
  constexpr auto overflow = std::int64_t{std::numeric_limits<unsigned>::max()} + 1;
  expect_rejected("tick-delta-overflow", liquid_asset(1, "test", overflow));
});

const GameTestRegistration liquid_catalog_lookup(
    "LiquidsDatabaseTest", "SparseLookupPreservesInteractionsAndLazyDescriptions", [](GameTest& fixture) {
  auto directory = File::temporary_directory(fixture.root().random());
  if (!fixture.check(bool(directory), "create isolated liquid test directory"))
    return;
  auto cleanup = make_scope_exit([&] { fixture.check(bool(File::remove_all(*directory)), "remove liquid test directory"); });
  auto materials = fixture.root().materialDatabase();
  if (!fixture.check(bool(materials), materials ? "load materials" : materials.error().what()))
    return;
  auto dirt = (*materials)->materialId("dirt");
  if (!fixture.check(bool(dirt), dirt ? "resolve dirt material" : dirt.error().what()))
    return;

  auto source = liquid_asset(17, "source", 0, {
    JsonObject{{"liquid", 255}, {"liquidResult", 2}},
    JsonObject{{"liquid", 91}, {"liquidResult", 2}},
    JsonObject{{"liquid", 91}, {"liquidResult", 255}},
    JsonObject{{"liquid", 2}, {"materialResult", "dirt"}, {"liquidResult", 255}},
    JsonObject{{"liquid", 92}, {"liquidResult", 93}},
    JsonObject{{"liquid", 0}, {"liquidResult", 0}},
  });
  source["description"] = "Default description";
  source["humanDescription"] = "Human description";
  source["avianDescription"] = false;
  Json config = JsonObject{{"liquidEngineParameters", liquid_engine_config()}, {"backgroundDrain", 0}};
  auto loaded = liquids_fixture(fixture, *directory, config,
      {{"a-source.liquid", std::move(source)},
       {"b-high.liquid", liquid_asset(255, "high", 1, {JsonObject{{"liquid", 17}, {"liquidResult", 17}}})},
       {"z-low.liquid", liquid_asset(2, "low")}});
  if (!fixture.check(bool(loaded), loaded ? "load sparse catalog" : loaded.error().what()))
    return;
  auto const& catalog = **loaded;

  auto settings = catalog.all_liquid_settings();
  static_assert(std::same_as<std::ranges::range_reference_t<decltype(settings)>, LiquidSettings const&>);
  std::vector<LiquidId> ids;
  for (auto const& entry : settings)
    ids.push_back(entry.id);
  fixture.check(ids == std::vector<LiquidId>{2, 17, 255}, "settings contain only registered IDs in ascending order");
  fixture.check(catalog.liquid_id("source") == LiquidId{17} && catalog.liquid_name(255) == String("high"),
                "look up registered IDs and names");
  fixture.check(catalog.is_valid_liquid_id(empty_liquid_id) && catalog.is_liquid_name("empty")
                  && catalog.liquid_id("empty") == empty_liquid_id
                  && catalog.liquid_name(empty_liquid_id) == String("empty")
                  && !catalog.liquid_settings(empty_liquid_id),
                "empty is a reserved name and ID, not a catalog entry");
  fixture.check(!catalog.is_valid_liquid_id(91) && !catalog.liquid_settings(91),
                "an unregistered interaction target does not fill a catalog hole");
  auto entry = catalog.liquid_settings(17);
  fixture.check(entry && entry->get().tick_delta == 0, "zero tickDelta disables scheduling");

  auto forward = catalog.interact(17, 255);
  auto backwards = catalog.interact(255, 17);
  // Mutual references guarantee a forward reference regardless of asset scan order.
  fixture.check(forward && forward->maybe_right() == LiquidId{2}
                  && backwards && backwards->maybe_right() == LiquidId{17},
                "resolve forward target and result IDs");
  auto repeated = catalog.interact(17, 91);
  fixture.check(repeated && repeated->maybe_right() == LiquidId{255}, "last interaction for a target wins");
  auto material = catalog.interact(17, 2);
  fixture.check(material && material->maybe_left() == *dirt, "materialResult takes precedence over liquidResult");
  fixture.check(!catalog.interact(2, 17), "interactions do not install a reverse rule");
  auto unregistered = catalog.interact(17, 92);
  fixture.check(unregistered && unregistered->maybe_right() == LiquidId{93},
                "in-range targets and results need not be registered");
  auto empty = catalog.interact(17, empty_liquid_id);
  fixture.check(empty && empty->maybe_right() == empty_liquid_id, "interaction IDs may be zero");

  fixture.check(catalog.liquid_description(17, "human") == String("Human description"), "use species description");
  fixture.check(catalog.liquid_description(17, "floran") == String("Default description")
                  && catalog.liquid_description(17) == String("Default description"),
                "fall back to generic description when species has no override");
  auto malformed = catalog.liquid_description(17, "avian");
  fixture.check(!malformed && malformed.error().is<JsonException>(),
                "malformed species description fails only when requested");
  fixture.check(catalog.liquid_description(2, "avian") == String()
                  && catalog.liquid_description(empty_liquid_id) == String(),
                "missing descriptions and empty liquid yield empty text");
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
  auto database = fixture.root().liquids_database();
  ASSERT_TRUE(database) << database.error().what();
  auto water = (*database)->liquid_id("water");
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
  auto database = fixture.root().liquids_database();
  ASSERT_TRUE(database) << database.error().what();
  auto water = (*database)->liquid_id("water");
  ASSERT_TRUE(water) << water.error().what();
  auto lava = (*database)->liquid_id("lava");
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
    auto database = fixture.root().liquids_database();
    ASSERT_TRUE(database) << database.error().what();
    auto water = (*database)->liquid_id("water");
    ASSERT_TRUE(water) << water.error().what();
    auto lava = (*database)->liquid_id("lava");
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
    auto database = fixture.root().liquids_database();
    ASSERT_TRUE(database) << database.error().what();
    auto water = (*database)->liquid_id("water");
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
      auto database = fixture.root().liquids_database();
      ASSERT_TRUE(database) << database.error().what();
      auto water = (*database)->liquid_id("water");
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
      auto database = fixture.root().liquids_database();
      ASSERT_TRUE(database) << database.error().what();
      auto water = (*database)->liquid_id("water");
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

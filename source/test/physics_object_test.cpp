#include "gtest/gtest.h"

import std;
import star.actor_movement_controller;
import star.algorithm;
import star.assets;
import star.byte_array;
import star.data_stream;
import star.data_stream_devices;
import star.data_stream_extra;
import star.entity;
import star.entity_map;
import star.farmable_object;
import star.file;
import star.force_regions;
import star.format;
import star.game_test;
import star.game_timers;
import star.game_types;
import star.interactive_entity;
import star.json;
import star.line;
import star.list;
import star.movement_controller;
import star.collision_block;
import star.physics_entity;
import star.object_database;
import star.physics_object;
import star.platformer_astar_types;
import star.poly;
import star.rect;
import star.root;
import star.tile_entity;
import star.string;
import star.vector;
import star.world_server;

using namespace Star;

namespace {

static_assert(physics_force_region<DirectionalForceRegion const&> && physics_force_region<RadialForceRegion>
    && physics_force_region<GradientForceRegion> && !physics_force_region<PhysicsForceRegion>);

TEST(ForceRegions, JsonPrecedenceAndFilters) {
  JsonObject config{{"type", "dIrEcTiOnAlFoRcErEgIoN"}, {"rectRegion", JsonArray{1, 2, 5, 6}},
      {"xTargetVelocity", 0}, {"yTargetVelocity", Json()}, {"controlForce", -4}, {"categoryBlacklist", Json()}};
  auto parsed = json_to_physics_force_region(config);
  ASSERT_TRUE(parsed) << parsed.error().what();
  auto const& directional = std::get<DirectionalForceRegion>(*parsed);
  EXPECT_EQ(directional.bounding_box(), RectF(1, 2, 5, 6));
  EXPECT_EQ(directional.x_target_velocity, 0);
  EXPECT_FALSE(directional.y_target_velocity);
  EXPECT_EQ(directional.control_force, -4);
  EXPECT_TRUE(directional.category_filter.check({}));
  EXPECT_TRUE(directional.category_filter.check({"actor"}));

  config["type"] = "GradientForceRegion";
  config["gradient"] = JsonArray{JsonArray{0, 0}, JsonArray{0, 1}};
  config["baseTargetVelocity"] = -5;
  config["baseControlForce"] = 6;
  config["polyRegion"] = JsonArray{JsonArray{-4, -3}, JsonArray{0, -3}, JsonArray{-4, 0}};
  config["categoryWhitelist"] = JsonArray{"actor"};
  parsed = json_to_physics_force_region(config);
  ASSERT_TRUE(parsed) << parsed.error().what();
  auto const& gradient = std::get<GradientForceRegion>(*parsed);
  EXPECT_EQ(gradient.bounding_box(), RectF(-4, -3, 0, 0));
  EXPECT_TRUE(gradient.category_filter.check({"other", "actor"}));
  EXPECT_FALSE(gradient.category_filter.check({}));
  config["categoryBlacklist"] = JsonArray{"actor"};
  auto conflict = json_to_physics_force_region(config);
  ASSERT_FALSE(conflict);
  EXPECT_NE(std::string_view(conflict.error().what()).find("Cannot specify both"), std::string_view::npos);
  config["categoryWhitelist"] = Json();
  auto blacklist = json_to_physics_category_filter(config);
  ASSERT_TRUE(blacklist);
  EXPECT_FALSE(blacklist->check({"actor"}));
  EXPECT_TRUE(blacklist->check({"other"}));

  // Explicit null is an invalid polygon, not a request to fall back to the valid rectangle.
  config["polyRegion"] = Json();
  EXPECT_FALSE(json_to_physics_force_region(config));
  config["type"] = "other";
  auto unknown = json_to_physics_force_region(config);
  ASSERT_FALSE(unknown);
  EXPECT_NE(std::string_view(unknown.error().what()).find("No such physics force region type 'other'"), std::string_view::npos);
}

TEST(ForceRegions, JsonGeometryDomain) {
  JsonObject radial{{"outerRadius", 4}, {"innerRadius", 0}, {"targetRadialVelocity", -3}, {"controlForce", 2}};
  auto parsed = RadialForceRegion::from_json(radial);
  ASSERT_TRUE(parsed);
  EXPECT_EQ(parsed->bounding_box(), RectF(-4, -4, 4, 4));
  radial["innerRadius"] = 4;
  EXPECT_TRUE(RadialForceRegion::from_json(radial));
  radial["innerRadius"] = -1;
  EXPECT_FALSE(RadialForceRegion::from_json(radial));
  radial["innerRadius"] = 5;
  EXPECT_FALSE(RadialForceRegion::from_json(radial));
  radial["innerRadius"] = radial["outerRadius"] = 0;
  EXPECT_TRUE(RadialForceRegion::from_json(radial));
  radial["outerRadius"] = 1e100;
  EXPECT_FALSE(RadialForceRegion::from_json(radial));
  radial["outerRadius"] = 2;
  radial["center"] = JsonArray{1e100, 0};
  EXPECT_FALSE(RadialForceRegion::from_json(radial));

  JsonObject gradient{{"rectRegion", JsonArray{0, 0, 2, 3}}, {"gradient", JsonArray{JsonArray{1, 2}, JsonArray{1, 2}}},
      {"baseTargetVelocity", -1}, {"baseControlForce", 3}};
  EXPECT_FALSE(GradientForceRegion::from_json(gradient));
  gradient["gradient"] = JsonArray{JsonArray{0, 0}, JsonArray{1e30, 0}};
  EXPECT_FALSE(GradientForceRegion::from_json(gradient)); // squared length overflows float
  gradient["gradient"] = JsonArray{JsonArray{0, 0}, JsonArray{1, 0}};
  gradient["baseControlForce"] = 1e100;
  EXPECT_FALSE(GradientForceRegion::from_json(gradient));
}

TEST(ForceRegions, LegacyWireLayout) {
  PolyF polygon(RectF(-1, -2, 3, 4));
  auto filter = PhysicsCategoryFilter::whitelist({"actor"});
  auto check = [](PhysicsForceRegion const& region, DataStreamBuffer const& legacy) {
    auto bytes = DataStreamBuffer::serialize(region);
    ASSERT_TRUE(bytes);
    EXPECT_EQ(*bytes, legacy.data());
    auto decoded = DataStreamBuffer::deserialize<PhysicsForceRegion>(legacy.span());
    ASSERT_TRUE(decoded) << decoded.error().what();
    EXPECT_EQ(*decoded, region);
  };
  DataStreamBuffer directional;
  ASSERT_TRUE(directional << std::uint8_t{0} << polygon << false << true << -3.f << 2.f
      << std::uint32_t{0} << StringSet{"actor"});
  check(DirectionalForceRegion{polygon, {}, -3.f, 2.f, filter}, directional);
  DataStreamBuffer radial;
  ASSERT_TRUE(radial << std::uint8_t{1} << Vec2F(2, 4) << 5.f << 5.f << -1.f << 2.f
      << std::uint32_t{1} << StringSet{});
  check(RadialForceRegion{{2, 4}, 5, 5, -1, 2, {}}, radial);
  DataStreamBuffer gradient;
  ASSERT_TRUE(gradient << std::uint8_t{2} << polygon << Vec2F(1, 2) << Vec2F(-3, 4) << -6.f << 7.f
      << std::uint32_t{0} << StringSet{"actor"});
  check(GradientForceRegion{polygon, Line2F({1, 2}, {-3, 4}), -6, 7, filter}, gradient);
}

TEST(ForceRegions, InvalidWireDoesNotReplaceValues) {
  RadialForceRegion original{{2, 3}, 4, 1, -6, 5, PhysicsCategoryFilter::whitelist({"actor"})};
  auto radial = original;
  auto check_radii = [&](std::uint32_t outer, float inner) {
    DataStreamBuffer wire;
    ASSERT_TRUE(wire << Vec2F() << outer << inner << 1.f << 2.f << std::uint32_t{1} << StringSet{});
    auto result = DataStreamBuffer::deserialize(radial, wire.span());
    ASSERT_FALSE(result);
    EXPECT_TRUE(result.error().is<DataStreamException>());
    EXPECT_EQ(radial, original);
  };
  check_radii(0x7fc00001u, 0.f); // NaN payload, independent of compiler floating-point assumptions
  check_radii(0x7f800000u, 0.f);
  check_radii(std::bit_cast<std::uint32_t>(2.f), 3.f);
  check_radii(std::bit_cast<std::uint32_t>(2.f), -1.f);
  DataStreamBuffer bad_gradient;
  PolyF polygon(RectF(0, 0, 2, 3));
  ASSERT_TRUE(bad_gradient << polygon << Vec2F(1, 2) << Vec2F(1, 2) << 3.f << 4.f << std::uint32_t{1} << StringSet{});
  GradientForceRegion gradient{polygon, Line2F({3, 0}, {0, 4}), 3, 4, {}};
  auto previous = gradient;
  EXPECT_FALSE(DataStreamBuffer::deserialize(gradient, bad_gradient.span()));
  EXPECT_EQ(gradient, previous);
  DataStreamBuffer bad_filter;
  ASSERT_TRUE(bad_filter << std::uint32_t{2} << StringSet{});
  auto filter = original.category_filter;
  EXPECT_FALSE(DataStreamBuffer::deserialize(filter, bad_filter.span()));
  EXPECT_EQ(filter, original.category_filter);
}

std::shared_ptr<ObjectConfig> unanchored_physics_config() {
  auto config = std::make_shared<ObjectConfig>();
  config->name = "physics-bounds-test";
  config->path = "/";
  config->config = JsonObject();
  config->health = 1;
  auto orientation = std::make_shared<ObjectOrientation>();
  orientation->config = JsonObject();
  orientation->boundBox = RectI(0, 0, 0, 0);
  orientation->meta_bound_box = RectF(-1, -1, 1, 1);
  config->orientations.push_back(std::move(orientation));
  return config;
}

TEST(SlidingWindow, RejectsInvalidConfiguration) {
  static_assert(!std::default_initializable<SlidingWindow>);
  auto empty = SlidingWindow::create(1, 0, 0);
  ASSERT_FALSE(empty);
  EXPECT_NE(std::string_view(empty.error().what()).find("immersionResolution"), std::string_view::npos);
  EXPECT_FALSE(SlidingWindow::create(1, std::numeric_limits<std::size_t>::max(), 0));
  EXPECT_FALSE(SlidingWindow::create(-1, 1, 0));
  EXPECT_FALSE(SlidingWindow::create(0, 1, 0));
  constexpr float infinite_window = std::numeric_limits<float>::infinity();
  constexpr float nan_sample = std::numeric_limits<float>::quiet_NaN();
  EXPECT_FALSE(SlidingWindow::create(infinite_window, 1, 0));
  EXPECT_FALSE(SlidingWindow::create(1, 1, nan_sample));
}

TEST(SlidingWindow, FailedSampleConsumesPeriodWithoutReplacingSlot) {
  auto window = SlidingWindow::create(3, 3, -1);
  ASSERT_TRUE(window);
  int calls = 0;
  auto fail = [&]() -> std::expected<float, StarException> {
    ++calls;
    return std::unexpected(StarException("liquid query failed"));
  };
  ASSERT_TRUE(window->update(0.75f, fail));
  EXPECT_EQ(calls, 0);
  EXPECT_FALSE(window->update(0.25f, fail));
  ASSERT_TRUE(window->update(0.75f, fail));
  EXPECT_EQ(calls, 1);
  EXPECT_EQ(window->average(), -1);

  // Slots 1, 2, 0 receive the samples. Physical-order float addition loses
  // the unit in (1 + 2^24) - 2^24; chronological or double sums retain it.
  for (float value : {0x1p24f, -0x1p24f, 1.f}) {
    ASSERT_TRUE(window->update(10.f, [value, &calls]() -> std::expected<float, StarException> {
      ++calls;
      return value;
    }));
  }
  EXPECT_EQ(calls, 4);
  EXPECT_EQ(window->average(), 0);
}

const GameTestRegistration farm_immersion("FarmableObject", "ImmersionSampleThreshold", [](GameTest& fixture) {
  auto& root = fixture.root();
  auto assets = root.assets();
  ASSERT_TRUE(assets);
  auto farming = (*assets)->json("/farming.config");
  ASSERT_TRUE(farming);
  auto duration = farming->get_float("immersionWindow");
  auto count = farming->get_uint("immersionResolution");
  ASSERT_TRUE(duration && count && *count);
  float period = *duration / *count;

  auto storage = File::ephemeral_file(root.random());
  ASSERT_TRUE(storage);
  auto world = WorldServer::create(root, Vec2U(128, 128), *storage);
  ASSERT_TRUE(world) << world.error().what();
  auto close = make_scope_exit([&] { EXPECT_TRUE((*world)->close()); });
  auto config = unanchored_physics_config();
  config->orientations.front()->spaces = {Vec2I(0, 0)};
  config->orientations.front()->frames = 1;
  config->orientations.front()->animationCycle = 1;
  EXPECT_FALSE(FarmableObject::create(root, config, JsonObject{{"minImmersion", 1}, {"maxImmersion", 0}}));
  for (int threshold : {0, 1}) {
    auto crop = FarmableObject::create(root, config, JsonObject{{"minImmersion", threshold}, {"maxImmersion", threshold}});
    ASSERT_TRUE(crop) << crop.error().what();
    ASSERT_TRUE((*crop)->setTilePosition({64, 64}));
    ASSERT_TRUE((*crop)->init(**world, 1, EntityMode::Master));
    auto release = make_scope_exit([&] { EXPECT_TRUE((*crop)->uninit()); });
    ASSERT_TRUE((*crop)->update(period / 2, 1));
    auto before = (*crop)->should_destroy();
    ASSERT_TRUE(before);
    EXPECT_FALSE(*before);
    ASSERT_TRUE((*crop)->update(period / 2, 2));
    auto after = (*crop)->should_destroy();
    ASSERT_TRUE(after);
    EXPECT_EQ(*after, threshold != 0); // Dry soil: exactly at zero survives, strictly below one dies.
  }
});

template <typename T>
concept typed_map_query = requires(EntityMap const& map, RectF const& bounds) { map.query<T>(bounds); };
static_assert(typed_map_query<PhysicsEntity> && !typed_map_query<int>);

const GameTestRegistration typed_tile_queries("EntityMap", "TypedLinesAndMutableWrappedTiles", [](GameTest& fixture) {
  auto storage = File::ephemeral_file(fixture.root().random());
  ASSERT_TRUE(storage);
  Vec2U size(128, 128);
  auto world = WorldServer::create(fixture.root(), size, *storage);
  ASSERT_TRUE(world) << world.error().what();
  auto close = make_scope_exit([&] { EXPECT_TRUE((*world)->close()); });
  auto config = unanchored_physics_config();
  config->orientations.front()->spaces = {Vec2I(0, 0), Vec2I(0, 0)};
  auto object = PhysicsObject::create(fixture.root(), config, JsonObject{{"physicsForces", JsonObject()}});
  ASSERT_TRUE(object) << object.error().what();
  Vec2I origin(127, 64);
  ASSERT_TRUE((*object)->setTilePosition(origin));
  EntityMap entities(size, 1, 10);
  ASSERT_TRUE((*object)->init(**world, 1, EntityMode::Master));
  auto release = make_scope_exit([&] { entities.remove_entity(1); EXPECT_TRUE((*object)->uninit()); });
  ASSERT_TRUE(entities.add_entity(*object));
  int calls = 0;
  auto line = entities.line_query<PhysicsEntity>({126, 64}, {127, 64}, [&](auto const& physics) {
    ++calls;
    return physics == *object;
  });
  ASSERT_TRUE(line) << line.error().what();
  ASSERT_EQ(line->size(), 1u);
  EXPECT_EQ(calls, 1);
  EXPECT_EQ(line->front(), *object);

  Vec2I wrapped(std::numeric_limits<int>::max(), 64);
  auto tiles = entities.at_tile<PhysicsEntity>(wrapped);
  ASSERT_TRUE(tiles) << tiles.error().what();
  EXPECT_EQ(tiles->size(), 2u);
  auto occupied = entities.tile_is_occupied(wrapped, true);
  ASSERT_TRUE(occupied);
  EXPECT_TRUE(*occupied);
  calls = 0;
  auto found = entities.find_entity_at_tile(wrapped, [&](auto const&) { return ++calls == 2; });
  ASSERT_TRUE(found);
  EXPECT_EQ(*found, *object);
  calls = 0;
  ASSERT_TRUE(entities.for_each_entity_at_tile(wrapped, [&](auto const& tile) {
    ++calls;
    return tile->setTilePosition({126, 64});
  }));
  EXPECT_EQ(calls, 1);
});

const GameTestRegistration preferred_orientation("ObjectDatabase", "PreferredOrientationFallback", [](GameTest& fixture) {
  auto storage = File::ephemeral_file(fixture.root().random());
  ASSERT_TRUE(storage);
  auto world = WorldServer::create(fixture.root(), Vec2U(128, 128), *storage);
  ASSERT_TRUE(world) << world.error().what();
  auto cleanup = make_scope_exit([&] { EXPECT_TRUE((*world)->close()); });
  ObjectConfig config{};
  auto neutral = std::make_shared<ObjectOrientation>();
  auto preferred = std::make_shared<ObjectOrientation>();
  preferred->direction_affinity = Direction::Right;
  config.orientations = {neutral, preferred};
  Vec2I position(64, 64);
  EXPECT_EQ(config.find_valid_orientation(**world, position), 0u);
  EXPECT_EQ(config.find_valid_orientation(**world, position, Direction::Right), 1u);
  // An empty world's background cannot satisfy the preferred candidate's anchor.
  ObjectOrientation::Anchor anchor{};
  anchor.layer = TileLayer::Background;
  preferred->anchors.push_back(anchor);
  EXPECT_EQ(config.find_valid_orientation(**world, position, Direction::Right), 0u);
});

const GameTestRegistration legacy_orientation_order("ObjectDatabase", "LegacyOrientationExpansion", [](GameTest& fixture) {
  auto parse = [&](Json const& entries) {
    return ObjectDatabase::parseOrientations(fixture.root(), "/test.object", entries, JsonObject());
  };
  auto entries = Json::parse(R"([
    {"imageLayers":[]},
    {"imageLayers":[],"dualImage":null,"leftImage":"ignored"},
    {"imageLayers":[],"leftImage":"left"},
    {"imageLayers":[],"leftImage":"a","rightImage":"b"},
    {"imageLayers":[],"spaceScan":0,"imageKeys":{"color":"custom"}}
  ])");
  ASSERT_TRUE(entries);
  auto orientations = parse(*entries);
  ASSERT_TRUE(orientations) << orientations.error().what();
  JsonArray actual;
  for (auto const& orientation : *orientations)
    actual.emplace_back(orientation->config);
  auto expected = Json::parse(R"([
    {"imageLayers":[]},
    {"imageLayers":[],"dualImage":null,"leftImage":"ignored","image":null,"flipImages":true,"direction":"left"},
    {"imageLayers":[],"dualImage":null,"leftImage":"ignored","image":null,"direction":"right"},
    {"imageLayers":[],"leftImage":"left","image":"left","direction":"left"},
    {"imageLayers":[],"leftImage":"left","image":null,"direction":"right"},
    {"imageLayers":[],"leftImage":"a","rightImage":"b","image":"a","direction":"left"},
    {"imageLayers":[],"leftImage":"a","rightImage":"b","image":"b","direction":"right"},
    {"imageLayers":[],"spaceScan":0,"imageKeys":{"color":"custom"}}
  ])");
  ASSERT_TRUE(expected);
  EXPECT_EQ(Json(std::move(actual)), *expected);

  // The later non-object must fail before the earlier missing image is parsed.
  auto malformed = parse(JsonArray{JsonObject(), Json()});
  ASSERT_FALSE(malformed);
  EXPECT_NE(std::string_view(malformed.error().what()).find("Improper conversion to JsonObject"), std::string_view::npos);
});

const GameTestRegistration
  local_force_bounds("PhysicsObject", "LocalBoundsAndWorldForcesAtLargeCoordinates", [](GameTest& fixture) {
    // The empty world is sparse: no sectors or terrain need to be generated to
    // initialize an anchor-free object and index its physics bounds.
    Vec2U worldSize(32768, 32768);
    auto storage = File::ephemeral_file(fixture.root().random());
    ASSERT_TRUE(storage) << storage.error().what();
    auto created = WorldServer::create(fixture.root(), worldSize, *storage);
    ASSERT_TRUE(created) << created.error().what();
    auto world = *created;
    auto closeWorld = make_scope_exit([&] { EXPECT_TRUE(world->close()); });
    EntityMap entities(worldSize, 1, 100);

    // A minimal configuration is input to the real PhysicsObject implementation,
    // not a substitute Entity or World. An anchor-free orientation gives the
    // non-force bounds a known extent without requiring terrain.
    auto config = unanchored_physics_config();

    auto forceConfig = Json::parse(R"({
    "directional": {
      "type":"DirectionalForceRegion", "rectRegion":[-12,-4,-8,2],
      "xTargetVelocity":3, "controlForce":10
    },
    "gradient": {
      "type":"GradientForceRegion", "polyRegion":[[7,-9],[13,-9],[10,-5]],
      "gradient":[[7,-9],[13,-5]], "baseTargetVelocity":4, "baseControlForce":20
    },
    "radial": {
      "type":"RadialForceRegion", "center":[3,4], "outerRadius":4,
      "innerRadius":1, "targetRadialVelocity":5, "controlForce":30
    }
  })");
    ASSERT_TRUE(forceConfig) << forceConfig.error().what();
    RectF expected_bounds(-12, -9, 13, 8);
    Vec2I farPosition(EntityMap::maximum_entity_bound_box + 1024, EntityMap::maximum_entity_bound_box + 2048);
    ASSERT_LT(farPosition[0] + 13, static_cast<int>(worldSize[0]));
    ASSERT_LT(farPosition[1] + 6, static_cast<int>(worldSize[1]));

    for (bool enabled : {false, true}) {
      SCOPED_TRACE(enabled ? "enabled" : "disabled");
      auto forces = forceConfig->to_object();
      ASSERT_TRUE(forces) << forces.error().what();
      for (auto&& pair : *forces) {
        auto changed = pair.second.set("enabled", enabled);
        ASSERT_TRUE(changed) << changed.error().what();
        pair.second = std::move(*changed);
      }
      auto objectResult =
        PhysicsObject::create(fixture.root(), config, JsonObject{{"physicsForces", std::move(*forces)}});
      ASSERT_TRUE(objectResult) << objectResult.error().what();
      auto object = *objectResult;
      std::optional<RectF> firstBounds;

      // Exercise ordinary, large-coordinate and horizontally wrapped bounds.
      int const world_width = static_cast<int>(worldSize[0]);
      for (auto const& tilePosition : {Vec2I(512, 256), farPosition, Vec2I(0, 256),
                                       Vec2I(world_width - 13, 256), Vec2I(12, 256)}) {
        SCOPED_TRACE(Star::format("position {}", tilePosition));
        ASSERT_TRUE(object->setTilePosition(tilePosition));
        auto id = entities.reserve_entity_id();
        ASSERT_TRUE(id) << id.error().what();
        auto initialized = object->init(*world, *id, EntityMode::Master);
        ASSERT_TRUE(initialized) << initialized.error().what();
        auto uninitObject = make_scope_exit([&] {
          entities.remove_entity(*id);
          EXPECT_TRUE(object->uninit());
        });
        Vec2F position(tilePosition);
        EXPECT_EQ(object->position(), position);
        EXPECT_EQ(object->meta_bound_box(), expected_bounds);
        if (firstBounds)
          EXPECT_EQ(object->meta_bound_box(), *firstBounds);
        else
          firstBounds = object->meta_bound_box();
        EXPECT_LT(object->meta_bound_box().width(), EntityMap::maximum_entity_bound_box);
        EXPECT_LT(object->meta_bound_box().height(), EntityMap::maximum_entity_bound_box);

        auto output = object->force_regions();
        ASSERT_TRUE(output) << output.error().what();
        if (enabled) {
          ASSERT_EQ(output->size(), 3u);
          ASSERT_TRUE(std::holds_alternative<DirectionalForceRegion>((*output)[0]));
          ASSERT_TRUE(std::holds_alternative<GradientForceRegion>((*output)[1]));
          ASSERT_TRUE(std::holds_alternative<RadialForceRegion>((*output)[2]));
          auto const& directional = std::get<DirectionalForceRegion>((*output)[0]);
          auto const& gradient = std::get<GradientForceRegion>((*output)[1]);
          auto const& radial = std::get<RadialForceRegion>((*output)[2]);
          EXPECT_EQ(directional.bounding_box(), RectF(-12, -4, -8, 2).translated(position));
          EXPECT_EQ(gradient.bounding_box(), RectF(7, -9, 13, -5).translated(position));
          EXPECT_EQ(gradient.gradient.min(), position + Vec2F(7, -9));
          EXPECT_EQ(gradient.gradient.max(), position + Vec2F(13, -5));
          EXPECT_EQ(radial.center, position + Vec2F(3, 4));
          EXPECT_EQ(radial.bounding_box(), RectF(-1, 0, 7, 8).translated(position));
        } else {
          EXPECT_TRUE(output->empty());
        }

        object->set_unique_id(String("indexed-object"));
        auto added = entities.add_entity(object);
        ASSERT_TRUE(added) << added.error().what();
        auto expectQuery = [&](Vec2F localPoint, bool present) {
          auto found = entities.entity_query(RectF::with_size(position + localPoint, Vec2F(0.25f, 0.25f)));
          ASSERT_TRUE(found) << found.error().what();
          EXPECT_EQ(found->size(), present ? 1u : 0u);
          if (present && !found->empty())
            EXPECT_EQ((*found)[0].get(), object.get());
        };
        // Force-only areas must be indexed even while forces are disabled. The
        // old translated meta box missed the negative-side area and produced a
        // false hit far outside the local bounds at the first position.
        expectQuery(Vec2F(-11, -3), true);
        expectQuery(Vec2F(10, -8), true);
        expectQuery(Vec2F(3, 4), true);
        expectQuery(Vec2F(32, 32), false);
        expectQuery(-position, false);
        if (tilePosition[0] == 0) {
          float const width = static_cast<float>(worldSize[0]);
          float const y = position[1];
          for (auto const& query : {RectF(width - 11, y, width - 10, y + 1),
                                    RectF(9, y, 10, y + 1), RectF(-11, y, 10, y + 1)}) {
            auto hits = entities.entity_query(query);
            ASSERT_TRUE(hits) << hits.error().what();
            ASSERT_EQ(hits->size(), 1u);
            EXPECT_EQ((*hits)[0], object);
          }
          int filter_calls = 0;
          auto rejected = entities.entity_query(RectF(-11, y, 10, y + 1),
              [&](std::shared_ptr<Entity> const&) -> std::expected<bool, StarException> {
                ++filter_calls;
                return std::unexpected(EntityMapException("seam filter"));
              });
          ASSERT_FALSE(rejected);
          EXPECT_TRUE(rejected.error().is<EntityMapException>());
          EXPECT_EQ(filter_calls, 1);
          auto other = PhysicsObject::create(fixture.root(), config, JsonObject{{"physicsForces", JsonObject()}});
          ASSERT_TRUE(other) << other.error().what();
          ASSERT_TRUE((*other)->setTilePosition(Vec2I(128, 256)));
          auto other_id = entities.reserve_entity_id();
          ASSERT_TRUE(other_id);
          ASSERT_TRUE((*other)->init(*world, *other_id, EntityMode::Master));
          auto release_other = make_scope_exit([&] {
            entities.remove_entity(*other_id);
            EXPECT_TRUE((*other)->uninit());
          });
          (*other)->set_unique_id(String("second-object"));
          ASSERT_TRUE(entities.add_entity(*other));
          ASSERT_TRUE((*other)->setTilePosition(Vec2I(512, 256)));
          (*other)->set_unique_id(String("indexed-object"));
          auto conflict = entities.update_all_entities();
          ASSERT_FALSE(conflict);
          EXPECT_TRUE(conflict.error().is<EntityMapException>());
          EXPECT_EQ(entities.unique_entity_id("indexed-object"), *id);
          EXPECT_EQ(entities.unique_entity_id("second-object"), *other_id);
          auto old_hits = entities.entity_query(RectF(128, 256, 128, 256));
          ASSERT_TRUE(old_hits);
          ASSERT_EQ(old_hits->size(), 1u);
          EXPECT_EQ((*old_hits)[0], *other);
          auto new_hits = entities.entity_query(RectF(512, 256, 512, 256));
          ASSERT_TRUE(new_hits);
          EXPECT_TRUE(new_hits->empty());
          (*other)->set_unique_id(String("second-object"));
          ASSERT_TRUE(entities.update_all_entities());
          new_hits = entities.entity_query(RectF(512, 256, 512, 256));
          ASSERT_TRUE(new_hits);
          ASSERT_EQ(new_hits->size(), 1u);
          EXPECT_EQ((*new_hits)[0], *other);
        }
        if (tilePosition[0] == 12 || tilePosition[0] == world_width - 13) {
          // Exact periodic contact, not a box straddling the seam.
          bool const at_left = tilePosition[0] == 12;
          float const seam = at_left ? static_cast<float>(world_width) : 0.f;
          Vec2F const edge(seam, position[1]);
          Vec2F const outside(seam + (at_left ? -1.f : 1.f), position[1]);
          auto point_hits = entities.entity_query(RectF(edge, edge));
          ASSERT_TRUE(point_hits) << point_hits.error().what();
          ASSERT_EQ(point_hits->size(), 1u);
          EXPECT_EQ((*point_hits)[0], object);
          auto line_hits = entities.entity_line_query(outside, edge);
          ASSERT_TRUE(line_hits) << line_hits.error().what();
          ASSERT_EQ(line_hits->size(), 1u);
          EXPECT_EQ((*line_hits)[0], object);
          auto diagonal_miss = entities.entity_line_query(outside, edge + Vec2F(0, 20));
          ASSERT_TRUE(diagonal_miss) << diagonal_miss.error().what();
          EXPECT_TRUE(diagonal_miss->empty());
        }
        ASSERT_TRUE(entities.update_all_entities());
        expectQuery(Vec2F(-11, -3), true);
        expectQuery(Vec2F(32, 32), false);
      }
    }
  });

const GameTestRegistration entity_id_range("EntityMap", "InclusiveIdReservation", [](GameTest& fixture) {
  Vec2U world_size(128, 128);
  auto storage = File::ephemeral_file(fixture.root().random());
  ASSERT_TRUE(storage);
  auto world = WorldServer::create(fixture.root(), world_size, *storage);
  ASSERT_TRUE(world) << world.error().what();
  auto close_world = make_scope_exit([&] { EXPECT_TRUE((*world)->close()); });
  EntityMap pair(world_size, 7, 8);
  auto cleanup = make_scope_exit([&] {
    for (auto id : pair.entity_ids())
      EXPECT_TRUE(pair.remove_entity(id)->uninit());
  });

  auto config = unanchored_physics_config();
  // A collision with a foreign ID must fall back into the local range.
  // The foreign entity must not consume either of its two slots.
  std::array<std::pair<EntityId, EntityId>, 3> requests{{{42, 42}, {42, 7}, {null_entity_id, 8}}};
  for (auto [request, expected_id] : requests) {
    auto id = pair.reserve_entity_id(request);
    ASSERT_TRUE(id) << id.error().what();
    ASSERT_EQ(*id, expected_id);
    auto object = PhysicsObject::create(fixture.root(), config, JsonObject());
    ASSERT_TRUE(object) << object.error().what();
    ASSERT_TRUE((*object)->setTilePosition({64, 64}));
    ASSERT_TRUE((*object)->init(**world, *id, EntityMode::Master));
    ASSERT_TRUE(pair.add_entity(*object));
  }
  auto exhausted = pair.reserve_entity_id();
  ASSERT_FALSE(exhausted);
  EXPECT_TRUE(exhausted.error().is<EntityMapException>());
  auto conflict = pair.maybe_reserve_entity_id(7);
  ASSERT_TRUE(conflict);
  EXPECT_FALSE(*conflict);
  auto external = pair.reserve_entity_id(43);
  ASSERT_TRUE(external);
  EXPECT_EQ(*external, 43);
  auto released = pair.remove_entity(8);
  ASSERT_TRUE(released);
  EXPECT_TRUE(released->uninit());
  auto last_slot = pair.reserve_entity_id(7);
  ASSERT_TRUE(last_slot);
  EXPECT_EQ(*last_slot, 8);

  EntityMap singleton(world_size, 7, 7);
  auto only_id = singleton.reserve_entity_id();
  ASSERT_TRUE(only_id);
  EXPECT_EQ(*only_id, 7);
  ASSERT_TRUE(singleton.add_entity(pair.entity(7)));
  EXPECT_FALSE(singleton.reserve_entity_id());
  ASSERT_TRUE(singleton.remove_entity(7));
  only_id = singleton.reserve_entity_id();
  ASSERT_TRUE(only_id);
  EXPECT_EQ(*only_id, 7);
});

const GameTestRegistration entity_query_geometry("EntityMap", "InteractionRadiusAndWrappedOrdering", [](GameTest& fixture) {
  Vec2U world_size(128, 128);
  auto storage = File::ephemeral_file(fixture.root().random());
  ASSERT_TRUE(storage);
  auto world = WorldServer::create(fixture.root(), world_size, *storage);
  ASSERT_TRUE(world) << world.error().what();
  auto close_world = make_scope_exit([&] { EXPECT_TRUE((*world)->close()); });
  EntityMap entities(world_size, 1, 2);
  auto cleanup = make_scope_exit([&] {
    for (auto id : entities.entity_ids())
      EXPECT_TRUE(entities.remove_entity(id)->uninit());
  });
  auto add = [&](EntityId id, int x, RectF bounds, int interaction_x) {
    auto config = unanchored_physics_config();
    config->orientations.front()->meta_bound_box = bounds;
    config->orientations.front()->interactiveSpaces = List<Vec2I>{{interaction_x, 0}};
    auto object = PhysicsObject::create(fixture.root(), config, JsonObject{{"interactAction", "OpenCraftingInterface"}});
    ASSERT_TRUE(object) << object.error().what();
    ASSERT_TRUE((*object)->setTilePosition({x, 64}));
    ASSERT_TRUE((*object)->init(**world, id, EntityMode::Master));
    ASSERT_TRUE(entities.add_entity(*object));
  };
  auto expect_near = [&](float x, EntityId wanted) {
    auto found = entities.interactive_entity_near({x, 64.5f}, 4.f);
    ASSERT_TRUE(found) << found.error().what();
    EXPECT_EQ(*found ? (*found)->entityId() : null_entity_id, wanted);
  };
  ASSERT_NO_FATAL_FAILURE(add(1, 127, RectF(0, 0, 2, 1), 0));
  // Across the seam, the tile center is at -0.5 and its bounds end at 1.
  expect_near(3.25f, 1);
  expect_near(3.5f, 1);
  expect_near(3.75f, null_entity_id);

  ASSERT_NO_FATAL_FAILURE(add(2, 2, RectF(-3, 0, 1, 1), -3));
  // Both objects offer the same interaction tile. Moving the cursor between
  // their distinct box centers must flip the tie-break, in either visit order.
  expect_near(0.f, 1);
  expect_near(1.f, 2);
  auto ordered = entities.entities_at({0.f, 64.5f});
  ASSERT_TRUE(ordered) << ordered.error().what();
  ASSERT_EQ(ordered->size(), 2u);
  EXPECT_EQ((*ordered)[0]->entityId(), 1);
  EXPECT_EQ((*ordered)[1]->entityId(), 2);
});

const GameTestRegistration moving_collision_filter("MovementController", "MovingPlatformAcrossSeam", [](GameTest& fixture) {
  auto storage = File::ephemeral_file(fixture.root().random());
  ASSERT_TRUE(storage) << storage.error().what();
  auto created = WorldServer::create(fixture.root(), Vec2U(2048, 2048), *storage);
  ASSERT_TRUE(created) << created.error().what();
  auto world = *created;
  auto close_world = make_scope_exit([&] { EXPECT_TRUE(world->close()); });
  // Indices follow key order: reject 0 by category, accept 1, stop before 2.
  auto shapes = Json::parse(R"({
    "a":{"collision":[[-1,0],[1,0],[1,1],[-1,1]],"categoryWhitelist":["vehicle"]},
    "b":{"collision":[[-1,0],[1,0],[1,1],[-1,1]],"collisionKind":"platform","categoryWhitelist":["actor"]},
    "c":{"collision":[[-1,0],[1,0],[1,1],[-1,1]]}
  })");
  ASSERT_TRUE(shapes) << shapes.error().what();
  auto object = PhysicsObject::create(fixture.root(), unanchored_physics_config(), JsonObject{{"physicsCollisions", *shapes}});
  ASSERT_TRUE(object) << object.error().what();
  ASSERT_TRUE((*object)->setTilePosition({0, 256}));
  auto added = world->addEntity(*object);
  ASSERT_TRUE(added) << added.error().what();
  auto remove = make_scope_exit([&] { EXPECT_TRUE(world->removeEntity((*object)->entityId(), false)); });
  MovementParameters parameters;
  parameters.physics_effect_categories = StringSet{"actor"};
  auto controller = MovementController::create(fixture.root(), parameters);
  ASSERT_TRUE(controller) << controller.error().what();
  (*controller)->init(*world);
  auto release = make_scope_exit([&] { (*controller)->uninit(); });
  std::size_t visits = 0;
  PolyF retained;
  auto scan = (*controller)->for_each_moving_collision(RectF(2047.25f, 256, 2047.75f, 257),
      [&](MovingCollisionId id, CollisionKind kind, PolyF&& poly, RectF const& bounds) -> std::expected<bool, StarException> {
        ++visits;
        EXPECT_EQ(id.physics_entity_id, (*object)->entityId());
        EXPECT_EQ(id.collision_index, 1u);
        EXPECT_EQ(kind, CollisionKind::Platform);
        EXPECT_EQ(bounds, RectF(2047, 256, 2049, 257));
        retained = std::move(poly);
        return false;
      });
  ASSERT_TRUE(scan) << scan.error().what();
  EXPECT_EQ(visits, 1u);
  EXPECT_EQ(retained, (PolyF{{2047,256},{2049,256},{2049,257},{2047,257}}));
});

// Observe the production solver's protected result without making it public game API.
class ForceTestController : public MovementController {
public:
  explicit ForceTestController(Root& root) : MovementController(root) {}
  using MovementController::initialize;
  using MovementController::applied_force_region;
  using MovementController::update_force_regions;
  using MovementController::surface_velocity;
};

const GameTestRegistration moving_platform_detachment("MovementController", "MovingPlatformLaunchMomentum", [](GameTest& fixture) {
  auto& root = fixture.root();
  auto storage = File::ephemeral_file(root.random());
  if (!fixture.check(bool(storage), "create ephemeral platform world storage")) return;
  auto world = WorldServer::create(root, Vec2U(2048, 2048), *storage);
  if (!fixture.check(bool(world), "create platform world")) return;
  auto close = make_scope_exit([&] { fixture.check(bool((*world)->close()), "close platform world"); });
  if (!fixture.check(bool((*world)->generateRegion(RectI(1008, 1008, 1040, 1040))), "generate platform region")) return;
  if (!fixture.check(bool((*world)->setDungeonGravity(no_dungeon_id, 60)), "set deck gravity")) return;
  auto shapes = Json::parse(R"({"deck":{"collision":[[-2,0],[2,0],[2,1],[-2,1]]}})");
  if (!fixture.check(bool(shapes), "parse platform shape")) return;
  auto deck = PhysicsObject::create(root, unanchored_physics_config(), JsonObject{{"physicsCollisions", *shapes}});
  if (!fixture.check(bool(deck) && bool((*deck)->setTilePosition({1024, 1024})) && bool((*world)->addEntity(*deck)), "place deck")) return;
  auto remove = make_scope_exit([&] { fixture.check(bool((*world)->removeEntity((*deck)->entityId(), false)), "remove deck"); });

  ForceTestController rider(root);
  MovementParameters parameters;
  parameters.collision_poly = PolyF(RectF(-0.25f, -0.25f, 0.25f, 0.25f));
  parameters.friction_enabled = false;
  parameters.gravity_enabled = true;
  parameters.gravity_multiplier = 1;
  parameters.air_buoyancy = 1;
  parameters.liquid_buoyancy = 1;
  parameters.enable_surface_slope_correction = true;
  parameters.speed_limit = 1000;
  parameters.max_movement_per_step = 0;
  parameters.bounce_factor = 0;
  if (!fixture.check(bool(rider.initialize(parameters)), "initialize rider")) return;
  rider.init(**world);
  auto release = make_scope_exit([&] { rider.uninit(); });
  rider.set_position({1024, 1025.375f});
  rider.set_velocity({0, -30});
  constexpr float dt = 1.0f / 60;
  if (!fixture.check(bool(rider.tick_master(dt)) && rider.y_position() == 1025.25f, "land on moving deck")) return;
  if (!fixture.check(bool((*deck)->setTilePosition({1025, 1024})), "shift supporting deck")) return;
  rider.set_velocity({0, 30});
  if (!fixture.check(bool(rider.tick_master(dt)), "launch from translated deck")) return;
  fixture.check(rider.position() == Vec2F(1025, 1025.75f) && rider.velocity() == Vec2F(1 / dt, 30),
                "launch inherits deck motion exactly once");
  fixture.check(rider.surface_velocity() == Vec2F{}, "detached rider has no supporting surface velocity");

  auto next_deck = PhysicsObject::create(root, unanchored_physics_config(), JsonObject{{"physicsCollisions", *shapes}});
  if (!fixture.check(bool(next_deck) && bool((*next_deck)->setTilePosition({1031, 1024})) &&
                     bool((*world)->addEntity(*next_deck)), "place second deck")) return;
  auto remove_next = make_scope_exit([&] { fixture.check(bool((*world)->removeEntity((*next_deck)->entityId(), false)), "remove second deck"); });
  rider.set_position({1025, 1025.375f});
  rider.set_velocity({0, -30});
  if (!fixture.check(bool(rider.tick_master(dt)) && rider.y_position() == 1025.25f, "land back on first deck")) return;
  if (!fixture.check(bool((*deck)->setTilePosition({1026, 1024})), "translate first deck again")) return;
  rider.set_velocity({300, -7.5f});
  if (!fixture.check(bool(rider.tick_master(dt)), "cross directly to second deck")) return;
  if (!fixture.check(rider.position() == Vec2F(1031, 1025.25f),
        std::format("land on second deck, got ({}, {})", rider.x_position(), rider.y_position()))) return;
  if (!fixture.check(rider.surface_velocity() == Vec2F{}, "second deck must not inherit first deck velocity")) return;
  rider.set_velocity({0, -7.5f});
  if (!fixture.check(bool(rider.tick_master(dt)), "stay on second deck")) return;
  if (!fixture.check(rider.position() == Vec2F(1031, 1025.25f),
        std::format("stay on second deck, got ({}, {})", rider.x_position(), rider.y_position()))) return;
  auto velocity_before_pause = rider.velocity();
  if (!fixture.check(bool((*next_deck)->setTilePosition({1032, 1024})), "translate deck while paused")) return;
  if (!fixture.check(bool(rider.tick_master(0)), "synchronize attached rider at zero timestep")) return;
  fixture.check(rider.position() == Vec2F(1032, 1025.25f) && rider.velocity() == velocity_before_pause,
                std::format("paused deck: pos ({}, {}), velocity ({}, {})", rider.x_position(), rider.y_position(), rider.x_velocity(), rider.y_velocity()));
});

const GameTestRegistration force_narrow_phase("MovementController", "ForceRegionNarrowPhaseAndOrder", [](GameTest& fixture) {
  auto storage = File::ephemeral_file(fixture.root().random());
  ASSERT_TRUE(storage);
  auto world = WorldServer::create(fixture.root(), Vec2U(2048, 2048), *storage);
  ASSERT_TRUE(world) << world.error().what();
  auto close = make_scope_exit([&] { EXPECT_TRUE((*world)->close()); });
  auto config = Json::parse(R"({
    "ring":{"type":"RadialForceRegion","innerRadius":4,"outerRadius":8,"targetRadialVelocity":10,"controlForce":8,"categoryWhitelist":["ring"]},
    "cone":{"type":"RadialForceRegion","innerRadius":0,"outerRadius":8,"targetRadialVelocity":10,"controlForce":8,"categoryWhitelist":["cone"]},
    "ramp":{"type":"RadialForceRegion","innerRadius":8,"outerRadius":8,"targetRadialVelocity":10,"controlForce":8,"categoryWhitelist":["ramp"]},
    "triangle":{"type":"DirectionalForceRegion","polyRegion":[[-4,-4],[4,-4],[-4,4]],"controlForce":8,"xTargetVelocity":10,"categoryWhitelist":["triangle"]},
    "collapsed":{"type":"GradientForceRegion","rectRegion":[-4,-4,4,4],"gradient":[[0,0],[0,0.000001]],"baseTargetVelocity":8,"baseControlForce":8,"categoryWhitelist":["collapsed"]},
    "half":{"type":"DirectionalForceRegion","rectRegion":[-4,-4,2,4],"controlForce":8,"xTargetVelocity":10,"categoryWhitelist":["half"]},
    "a_gradient":{"type":"GradientForceRegion","rectRegion":[-4,-4,4,4],"gradient":[[0,0],[4,0]],"baseTargetVelocity":4,"baseControlForce":4,"categoryWhitelist":["gradient","ordered"]},
    "b_direct":{"type":"DirectionalForceRegion","rectRegion":[-4,-4,4,4],"controlForce":1,"xTargetVelocity":-2,"categoryWhitelist":["ordered"]},
    "brake":{"type":"DirectionalForceRegion","rectRegion":[-4,-4,4,4],"controlForce":2,"xTargetVelocity":0,"categoryWhitelist":["brake"]},
    "inactive":{"type":"DirectionalForceRegion","rectRegion":[-4,-4,4,4],"controlForce":0,"xTargetVelocity":8,"categoryWhitelist":["inactive"]},
    "no_axes":{"type":"DirectionalForceRegion","rectRegion":[-4,-4,4,4],"controlForce":8,"categoryWhitelist":["no_axes"]}
  })");
  ASSERT_TRUE(config) << config.error().what();
  auto object = PhysicsObject::create(fixture.root(), unanchored_physics_config(), JsonObject{{"physicsForces", *config}});
  ASSERT_TRUE(object) << object.error().what();
  ASSERT_TRUE((*object)->setTilePosition({0, 256}));
  ASSERT_TRUE((*world)->addEntity(*object));
  auto remove = make_scope_exit([&] { EXPECT_TRUE((*world)->removeEntity((*object)->entityId(), false)); });

  MovementParameters parameters;
  parameters.mass = 1;
  parameters.collision_poly = PolyF(RectF(-0.25f, -0.25f, 0.25f, 0.25f));
  ForceTestController controller(fixture.root());
  ASSERT_TRUE(controller.initialize(parameters));
  controller.init(**world);
  auto release = make_scope_exit([&] { controller.uninit(); });
  controller.set_timestep(1);
  auto sample = [&](String category, Vec2F offset, float initial, float expected, bool applied) {
    SCOPED_TRACE(Star::format("{} at {} from {}", category, offset, initial));
    MovementParameters patch;
    patch.physics_effect_categories = StringSet{std::move(category)};
    ASSERT_TRUE(controller.apply_parameters(patch));
    controller.set_position(Vec2F(0, 256) + offset);
    controller.set_velocity({initial, 0});
    auto update = controller.update_force_regions();
    ASSERT_TRUE(update) << update.error().what();
    EXPECT_EQ(controller.applied_force_region(), applied);
    EXPECT_NEAR(controller.x_velocity(), expected, 0.00001f);
    EXPECT_NEAR(controller.y_velocity(), 0, 0.00001f);
  };
  sample("ring", {-6, 0}, 0, -4, true); // Beyond the old half-width, across the world seam.
  sample("ring", {6, 6}, 0, 0, false); // In the bounding square, outside the circle.
  sample("ring", {8, 0}, 0, 0, false);
  sample("ring", {0, 0}, 0, 0, false);
  sample("cone", {2, 0}, 0, 6, true);
  sample("ramp", {2, 0}, 0, 2, true);
  sample("triangle", {3, 3}, 0, 0, false); // Overlapping rectangles, disjoint polygons.
  sample("half", {2, 0}, 0, 4, true); // Exactly half of the body's area is inside the field.
  sample("collapsed", {2, 0}, 0, 0, false); // A valid local gradient collapses after world translation.
  sample("gradient", {4, 0}, 0, 0, false); // Nonzero overlap but the gradient has faded.
  sample("gradient", {-2, 0}, 0, 6, true);
  sample("ordered", {2, 0}, 0, 1, true); // Gradient brings x to 2 before the later opposing force.
  sample("brake", {1, 0}, 5, 3, true); // An explicit zero target brakes; it is not a missing target.
  sample("brake", {1, 0}, 0, 0, true); // Already at target, but still under active control.
  sample("inactive", {1, 0}, 1, 1, false);
  sample("no_axes", {1, 0}, 1, 1, false);
  sample("rejected", {1, 0}, 1, 1, false);
  for (auto const& degenerate : {PolyF(Vec2F{}), PolyF{}}) {
    MovementParameters body;
    body.collision_poly = degenerate;
    ASSERT_TRUE(controller.apply_parameters(body));
    sample("ordered", {2, 0}, 0, 0, false);
    sample("ring", {2, 0}, 0, 4, true);
  }
});

const GameTestRegistration force_path_reset("MovementController", "ForceRegionResetsOnlyAnAffectedPath", [](GameTest& fixture) {
  auto storage = File::ephemeral_file(fixture.root().random());
  ASSERT_TRUE(storage);
  auto world = WorldServer::create(fixture.root(), Vec2U(2048, 2048), *storage);
  ASSERT_TRUE(world) << world.error().what();
  auto close = make_scope_exit([&] { EXPECT_TRUE((*world)->close()); });
  ASSERT_TRUE((*world)->generateRegion(RectI(1008, 1008, 1040, 1040)));
  auto config = Json::parse(R"({"field":{"type":"RadialForceRegion","innerRadius":4,"outerRadius":8,"targetRadialVelocity":0,"controlForce":8}})");
  ASSERT_TRUE(config);
  auto object = PhysicsObject::create(fixture.root(), unanchored_physics_config(), JsonObject{{"physicsForces", *config}});
  ASSERT_TRUE(object) << object.error().what();
  ASSERT_TRUE((*object)->setTilePosition({1024, 1024}));
  ASSERT_TRUE((*world)->addEntity(*object));
  auto remove = make_scope_exit([&] { EXPECT_TRUE((*world)->removeEntity((*object)->entityId(), false)); });
  ActorMovementParameters parameters;
  parameters.gravity_enabled = false;
  parameters.fly_speed = 8;
  parameters.path_explore_rate = 1000;
  parameters.standing_poly = PolyF(RectF(-0.25f, -0.25f, 0.25f, 0.25f));
  parameters.crouching_poly = parameters.standing_poly;
  PlatformerAStar::Parameters search;
  search.must_end_on_ground = false;
  for (bool inside : {false, true}) {
    auto actor = ActorMovementController::create(fixture.root(), parameters);
    ASSERT_TRUE(actor) << actor.error().what();
    (*actor)->init(**world);
    auto release = make_scope_exit([&] { (*actor)->uninit(); });
    Vec2F start(1030, inside ? 1024.25f : 1030.25f);
    (*actor)->set_position(start);
    ASSERT_TRUE((*actor)->tick_master(1.f / 60));
    auto request = (*actor)->control_path_move(start + Vec2F(3, 0), false, search);
    ASSERT_TRUE(request) << request.error().what();
    ASSERT_FALSE(request->has_value());
    ASSERT_FALSE((*actor)->pathfinding()); // The path was found and is ready to move.
    ASSERT_TRUE((*actor)->tick_master(1.f / 60));
    EXPECT_EQ((*actor)->pathfinding(), inside); // Only a narrow-phase hit discards that path.
  }
});

}// namespace

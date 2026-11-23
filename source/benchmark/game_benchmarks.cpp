#include <cstdio>
#include <benchmark/benchmark.h>

import std;
import star.chat_processor;
import star.chat_types;
import star.message_types;
import star.rpc_promise;
import star.celestial_database;
import star.celestial_coordinate;
import star.celestial_parameters;
import star.celestial_types;
import star.biome;
import star.biome_placement;
import star.plant_database;
import star.behavior_database;
import star.behavior_state;
import star.assets;
import star.asset_path;
import star.file;
import star.format;
import star.json;
import star.ai_types;
import star.animation;
import star.drawable;
import star.ambient;
import star.world_template;
import star.sky_parameters;
import star.list;
import star.logging;
import star.lua;
import star.lua_converters;
import star.lua_root;
import star.native_runtime;
import star.benchmark_runner;
import star.object_database;
import star.projectile_database;
import star.root;
import star.root_loader;
import star.warping;
import star.string;
import star.world_layout;
import star.world_parameters;
import star.algorithm;
import star.actor_movement_controller;
import star.collision_block;
import star.game_types;
import star.material_database;
import star.material_types;
import star.platformer_astar;
import star.platformer_astar_types;
import star.rect;
import star.tile_modification;
import star.vector;
import star.world_server;
import star.world_tiles;

using namespace Star;

void register_player_storage_benchmarks(Star::Root& root, bool& failed);

namespace {

struct BenchmarkSession {
  Root& root;
  bool failed = false;

  void fail(benchmark::State& state, std::string_view message) {
    failed = true;
    state.SkipWithError(message.data());
  }

  void fail(benchmark::State& state, StarException const& error) {
    fail(state, error.what());
  }
};

namespace PlatformerBenchmarks {

struct Fixture {
  std::shared_ptr<WorldServer> world;
  ActorMovementParameters movement;
  PlatformerAStar::Parameters parameters;
  Vec2F start;
  unsigned steps;
  unsigned budget;
  bool flying;
  MaterialId floor_material;
};

void fingerprint(std::uint64_t& value, std::uint64_t word) {
  for (unsigned shift = 0; shift != 64; shift += 8) {
    value ^= (word >> shift) & 255;
    value *= 1099511628211ULL;
  }
}

void fingerprint_float(std::uint64_t& value, float number) { fingerprint(value, std::bit_cast<std::uint32_t>(number)); }

void fingerprint_vector(std::uint64_t& value, Vec2F const& vector) {
  fingerprint_float(value, vector[0]);
  fingerprint_float(value, vector[1]);
}

void fingerprint_node(std::uint64_t& value, PlatformerAStar::Node const& node) {
  fingerprint_vector(value, node.position);
  fingerprint(value, node.velocity.has_value());
  if (node.velocity)
    fingerprint_vector(value, *node.velocity);
}

std::expected<Fixture, StarException> prepare_fixture(Root& root, std::shared_ptr<WorldServer> world, bool flying,
                                                      unsigned steps, unsigned budget) {
  world->setSpawningEnabled(false);
  world->setTileProtectionEnabled(false);
  auto generated = world->generateRegion(RectI::with_size(Vec2I(1016, 1016), Vec2I(56, 24)));
  if (!generated)
    return std::unexpected(std::move(generated.error()));
  for (auto dungeon : {no_dungeon_id, construction_dungeon_id}) {
    auto gravity = world->setDungeonGravity(dungeon, flying ? 0.0f : 60.0f);
    if (!gravity)
      return std::unexpected(std::move(gravity.error()));
  }
  MaterialId floor_material = EmptyMaterialId;
  if (!flying) {
    auto materials = root.materialDatabase();
    if (!materials)
      return std::unexpected(std::move(materials.error()));
    auto dirt = (*materials)->materialId("dirt");
    if (!dirt)
      return std::unexpected(std::move(dirt.error()));
    floor_material = *dirt;
    for (int x = 1018; x < 1066; ++x) {
      Vec2I position(x, 1024);
      auto tile = world->modifyServerTile(position, true);
      if (!tile)
        return std::unexpected(std::move(tile.error()));
      if (!*tile)
        return std::unexpected(StarException("Platformer benchmark floor tile is unavailable"));
      (*tile)->get().background = StructureMaterialId;
      auto placed = world->forceModifyTile(
        position, PlaceMaterial{TileLayer::Foreground, floor_material, MaterialHue(0), TileCollisionOverride::Block},
        true);
      if (!placed)
        return std::unexpected(std::move(placed.error()));
      if (!*placed)
        return std::unexpected(StarException("Platformer benchmark floor placement was rejected"));
    }
  }
  ActorMovementParameters movement;
  movement.mass = 1.0f;
  movement.gravity_enabled = true;
  movement.gravity_multiplier = 1.0f;
  movement.air_buoyancy = 0.0f;
  movement.minimum_liquid_percentage = 0.5f;
  movement.walk_speed = 4.0f;
  movement.run_speed = 8.0f;
  movement.air_jump_profile.jump_speed.reset();
  PlatformerAStar::Parameters parameters;
  parameters.bound_box = RectF(-0.25f, -0.25f, 0.25f, 0.25f);
  parameters.standing_bound_box = parameters.bound_box;
  parameters.dropping_bound_box = parameters.bound_box;
  parameters.must_end_on_ground = !flying;
  parameters.max_distance = 48.0f;
  parameters.max_nodes_to_search = 512;
  return Fixture{std::move(world), std::move(movement), std::move(parameters), Vec2F(1024.0f, 1025.25f), steps, budget,
                 flying,           floor_material};
}

std::expected<void, StarException> validate_world(Fixture const& fixture) {
  // Independent terrain checks, not PathFinder's own collision predicates.
  CollisionSet collisions{CollisionKind::Null, CollisionKind::Block, CollisionKind::Slippery, CollisionKind::Platform,
                          CollisionKind::Dynamic};
  auto blocked = fixture.world->rectTileCollision(RectI(1018, 1025, 1066, 1032), collisions);
  if (!blocked)
    return std::unexpected(std::move(blocked.error()));
  if (*blocked)
    return std::unexpected(StarException("Platformer benchmark requires a completely clear air corridor"));
  auto liquid = fixture.world->liquidLevel(RectF(1018.0f, 1025.0f, 1066.0f, 1032.0f));
  if (!liquid)
    return std::unexpected(std::move(liquid.error()));
  if (liquid->level != 0.0f)
    return std::unexpected(StarException("Platformer benchmark requires a dry air corridor"));
  for (int x = 1018; x < 1066; ++x) {
    auto floor = fixture.world->rectTileCollision(RectI(x, 1024, x + 1, 1025), collisions);
    if (!floor)
      return std::unexpected(std::move(floor.error()));
    if (*floor == fixture.flying)
      return std::unexpected(StarException("Platformer benchmark floor presence differs from its oracle"));
    auto gravity = fixture.world->gravity(Vec2F(static_cast<float>(x), fixture.start[1]));
    if (!gravity)
      return std::unexpected(std::move(gravity.error()));
    if (*gravity != (fixture.flying ? 0.0f : 60.0f))
      return std::unexpected(StarException("Platformer benchmark world gravity differs from its oracle"));
  }
  return {};
}

std::uint64_t input_fingerprint(Fixture const& fixture) {
  std::uint64_t value = 14695981039346656037ULL;
  // Fixed setup/config is part of the input fingerprint, not the ephemeral
  // filename or WorldServer RNG. Terrain is checked independently pre/post.
  for (unsigned char byte :
       std::string_view("platformer-v1;world=2048x2048;region=1016,1016,56,24;floor=1018:1066,1024;"
                        "air=1018,1025,1066,1032;mass=1;gravityEnabled=1;gravityMultiplier=1;airBuoyancy=0;"
                        "minimumLiquidPercentage=.5;walkSpeed=4;runSpeed=8;jumpSpeed=none;"
                        "allBoxes=-.25,-.25,.25,.25;maxDistance=48;maxNodesToSearch=512;otherSearchOptions=default"))
    fingerprint(value, byte);
  fingerprint_vector(value, fixture.start);
  fingerprint(value, fixture.steps);
  fingerprint(value, fixture.budget);
  fingerprint(value, fixture.flying);
  fingerprint(value, fixture.floor_material);
  fingerprint_float(value, fixture.flying ? 0.0f : 60.0f);
  return value;
}

std::expected<void, StarException> run_search(Fixture const& fixture, bool check_output, std::uint64_t& output) {
  auto target = fixture.start + Vec2F(static_cast<float>(fixture.steps), 0.0f);
  auto finder =
    PlatformerAStar::PathFinder::create(*fixture.world, fixture.start, target, fixture.movement, fixture.parameters);
  if (!finder)
    return std::unexpected(std::move(finder.error()));
  for (unsigned call = 0; call < 514; ++call) {
    auto status = (*finder)->explore(fixture.budget ? std::optional<unsigned>(fixture.budget) : std::nullopt);
    if (!status)
      return std::unexpected(std::move(status.error()));
    if (!status->has_value())
      continue;
    if (!**status)
      return std::unexpected(StarException("Platformer benchmark failed to find its unobstructed route"));
    auto const& path = (*finder)->result();
    benchmark::DoNotOptimize(path);
    if (!check_output)
      return {};
    if (!path || path->size() != fixture.steps)
      return std::unexpected(StarException("Platformer path size differs from independent straight-line oracle"));
    fingerprint(output, **status);
    fingerprint(output, path.has_value());
    fingerprint(output, path->size());
    PlatformerAStar::Node previous{fixture.start, {}};
    for (unsigned index = 0; index < fixture.steps; ++index) {
      auto const& edge = (*path)[index];
      PlatformerAStar::Node expected_target{fixture.start + Vec2F(static_cast<float>(index + 1), 0.0f), {}};
      auto expected_action = fixture.flying ? PlatformerAStar::Action::Fly : PlatformerAStar::Action::Walk;
      if (edge.source != previous || edge.target != expected_target || edge.action != expected_action
          || edge.cost != 1.0f || edge.jump_velocity != Vec2F(0.0f, 0.0f))
        return std::unexpected(StarException("Platformer complete edge differs from independent straight-line oracle"));
      fingerprint_float(output, edge.cost);
      fingerprint(output, static_cast<std::uint64_t>(edge.action));
      fingerprint_vector(output, edge.jump_velocity);
      fingerprint_node(output, edge.source);
      fingerprint_node(output, edge.target);
      previous = edge.target;
    }
    if (previous.position != target || previous.velocity)
      return std::unexpected(StarException("Platformer endpoint differs from independent straight-line oracle"));
    return {};
  }
  return std::unexpected(StarException("Platformer benchmark exceeded its bounded exploration calls"));
}

void benchmark_platformer(benchmark::State& state, BenchmarkSession& session, bool flying, unsigned steps,
                          unsigned budget) {
  auto storage = File::ephemeral_file(session.root.random());
  if (!storage) {
    session.fail(state, storage.error());
    return;
  }
  auto world = WorldServer::create(session.root, Vec2U(2048, 2048), *storage);
  if (!world) {
    session.fail(state, world.error());
    return;
  }
  auto close_world = make_scope_exit([&] {
    if (auto closed = (*world)->close(); !closed)
      session.fail(state, closed.error());
  });
  auto fixture = prepare_fixture(session.root, *world, flying, steps, budget);
  if (!fixture) {
    session.fail(state, fixture.error());
    return;
  }
  auto terrain = validate_world(*fixture);
  if (!terrain) {
    session.fail(state, terrain.error());
    return;
  }
  auto input = input_fingerprint(*fixture);
  std::uint64_t output = 14695981039346656037ULL;
  if (auto checked = run_search(*fixture, true, output); !checked) {
    session.fail(state, checked.error());
    return;
  }
  state.SetLabel(std::format("real WorldServer; steps={}; budget={} (0=unlimited); input={:016x}; output={:016x}",
                             steps, budget, input, output));
  // World/file/terrain/parameters and complete pre/post checks are untimed.
  // Each timed iteration creates one real PathFinder, explores to the first
  // terminal answer and destroys the finder and its owned complete result.
  // Axis-aligned dry routes have an independent exact unit-edge oracle; no
  // platformer neighbor/arc solver is duplicated in this benchmark.
  for (auto _ : state) {
    (void)_;
    std::uint64_t unused_output = 0;
    if (auto result = run_search(*fixture, false, unused_output); !result) {
      session.fail(state, result.error());
      return;
    }
  }
  std::uint64_t checked_output = 14695981039346656037ULL;
  if (auto checked = run_search(*fixture, true, checked_output); !checked)
    session.fail(state, checked.error());
  else if (checked_output != output || input_fingerprint(*fixture) != input)
    session.fail(state, "Platformer fixture or complete output fingerprint changed after timing");
  if (auto checked = validate_world(*fixture); !checked)
    session.fail(state, checked.error());
  state.SetItemsProcessed(state.iterations());
}

}// namespace PlatformerBenchmarks

std::expected<void, StarException> requirePrivateStorage(String const& storage) {
  // A benchmark bootconfig must point at private storage, never a save tree.
  for (std::string_view name : {"player", "universe"}) {
    auto exists = File::exists(File::relative_to(storage, name));
    if (!exists)
      return std::unexpected(std::move(exists.error()));
    if (*exists)
      return std::unexpected(StarException("Game benchmarks require private bootconfig storage without player/universe directories"));
  }
  return {};
}

struct ObjectFixture {
  std::shared_ptr<ObjectDatabase> database;
  std::vector<String> names;
  std::vector<std::shared_ptr<ObjectConfig>> configs;
  std::string label;
};

std::expected<ObjectFixture, StarException> prepareObjects(Root& root) {
  auto database = ObjectDatabase::create(root);
  if (!database)
    return std::unexpected(std::move(database.error()));
  auto catalogNames = (*database)->allObjects();
  std::vector<String> names(catalogNames.begin(), catalogNames.end());
  std::ranges::sort(names);
  if (names.empty())
    return std::unexpected(StarException("Real asset object catalog is empty"));
  ObjectFixture fixture{std::move(*database), {}, {}, {}};
  std::array<std::size_t, 2> selected{};
  for (auto const& name : names) {
    auto config = fixture.database->getConfig(name);
    if (!config)
      return std::unexpected(std::move(config.error()));
    if (!*config || (*config)->name != name || (*config)->path.empty() || (*config)->orientations.empty())
      return std::unexpected(StarException("Real object config failed identity/orientation validation"));
    bool scripted = !(*config)->scripts.empty() || !(*config)->animationScripts.empty();
    if (selected[scripted] == 4)
      continue;
    ++selected[scripted];
    auto jsonName = (*config)->config.get_string("objectName");
    if (!jsonName)
      return std::unexpected(std::move(jsonName.error()));
    if (*jsonName != name)
      return std::unexpected(StarException("Object config JSON disagrees with catalog identity"));
    auto cached = fixture.database->getConfig(name);
    if (!cached)
      return std::unexpected(std::move(cached.error()));
    if (cached->get() != config->get())
      return std::unexpected(StarException("Object catalog did not retain the warmed config"));
    fixture.names.push_back(name);
    fixture.configs.push_back(std::move(*config));
    if (!fixture.label.empty())
      fixture.label += ",";
    fixture.label += name.utf8();
    if (selected[0] == 4 && selected[1] == 4)
      break;
  }
  fixture.label += std::format("; plain={} scripted={}", selected[0], selected[1]);
  return fixture;
}

void objectColdParse(benchmark::State& state, BenchmarkSession& session) {
  // Validation warms the shared Assets JSON/image caches. Each measured batch
  // has a fresh ObjectDatabase config cache, not cold disk or cold Assets.
  auto fixture = prepareObjects(session.root);
  if (!fixture) {
    session.fail(state, fixture.error());
    return;
  }
  state.SetLabel(fixture->label);
  for (auto _ : state) {
    (void)_;
    state.PauseTiming();
    auto database = ObjectDatabase::create(session.root);
    if (!database) {
      session.fail(state, database.error());
      break;
    }
    state.ResumeTiming();
    bool parsed = true;
    for (auto const& name : fixture->names) {
      auto config = (*database)->getConfig(name);
      if (!config) {
        session.fail(state, config.error());
        parsed = false;
        break;
      }
      benchmark::DoNotOptimize(config->get());
    }
    if (!parsed)
      break;
    state.PauseTiming();
    database->reset();
    state.ResumeTiming();
  }
  state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(fixture->names.size()));
}

void objectWarmLookup(benchmark::State& state, BenchmarkSession& session) {
  auto fixture = prepareObjects(session.root);
  if (!fixture) {
    session.fail(state, fixture.error());
    return;
  }
  state.SetLabel(fixture->label);
  for (auto _ : state) {
    (void)_;
    for (auto const& name : fixture->names) {
      auto config = fixture->database->getConfig(name);
      if (!config) {
        session.fail(state, config.error());
        return;
      }
      benchmark::DoNotOptimize(config->get());
    }
  }
  state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(fixture->names.size()));
}

void projectileColdCatalogParse(benchmark::State& state, BenchmarkSession& session) {
  // readConfig is private and construction eagerly parses the entire catalog.
  // Measure the honest public factory, not projectileConfig's cached JSON copy.
  auto reference = ProjectileDatabase::create(session.root);
  if (!reference) {
    session.fail(state, reference.error());
    return;
  }
  auto names = (*reference)->allProjectileTypes();
  std::ranges::sort(names);
  if (names.empty()) {
    session.fail(state, "Real asset projectile catalog is empty");
    return;
  }
  auto assets = session.root.assets();
  if (!assets) {
    session.fail(state, assets.error());
    return;
  }
  auto files = (*assets)->scan_extension("projectile");
  if (!files) {
    session.fail(state, files.error());
    return;
  }
  if (files->get().size() != names.size()) {
    session.fail(state, "Projectile factory skipped or duplicated a real asset input");
    return;
  }
  for (auto const& file : files->get()) {
    auto input = (*assets)->json(file.string());
    if (!input) {
      session.fail(state, input.error());
      return;
    }
    auto name = input->get_string("projectileName");
    if (!name) {
      session.fail(state, name.error());
      return;
    }
    auto parsed = (*reference)->projectileConfig(*name);
    if (!parsed) {
      session.fail(state, parsed.error());
      return;
    }
    if (*parsed != *input) {
      session.fail(state, "Projectile factory changed or discarded a real asset input");
      return;
    }
  }
  state.SetLabel("entire catalog; warm Assets JSON cache");
  for (auto _ : state) {
    (void)_;
    auto database = ProjectileDatabase::create(session.root);
    if (!database) {
      session.fail(state, database.error());
      break;
    }
    benchmark::DoNotOptimize(database->get());
    state.PauseTiming();
    auto actualNames = (*database)->allProjectileTypes();
    std::ranges::sort(actualNames);
    if (actualNames != names) {
      session.fail(state, "Fresh projectile catalog changed during benchmark");
      break;
    }
    database->reset();
    state.ResumeTiming();
  }
  state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(names.size()));
}

std::expected<String, StarException> chooseWorldKey(Json const& config, String const& group, String const& preferred) {
  auto entries = config.get_object(group);
  if (!entries)
    return std::unexpected(std::move(entries.error()));
  auto keys = entries->keys();
  if (keys.empty())
    return std::unexpected(StarException("Real terrestrial world catalog is empty"));
  std::ranges::sort(keys);
  for (auto const& key : keys) {
    if (key == preferred)
      return key;
  }
  return keys[0];
}

void worldLayoutToJson(benchmark::State& state, BenchmarkSession& session) {
  auto assets = session.root.assets();
  if (!assets) {
    session.fail(state, assets.error());
    return;
  }
  auto catalog = (*assets)->json("/terrestrial_worlds.config");
  if (!catalog) {
    session.fail(state, catalog.error());
    return;
  }
  auto type = chooseWorldKey(*catalog, "planetTypes", "garden");
  auto size = chooseWorldKey(*catalog, "planetSizes", "small");
  if (!type || !size) {
    session.fail(state, !type ? type.error() : size.error());
    return;
  }
  constexpr std::uint64_t seed = 0x4f534242454e4348ULL;
  auto parameters = generateTerrestrialWorldParameters(session.root, *type, *size, seed);
  if (!parameters) {
    session.fail(state, parameters.error());
    return;
  }
  // Generate only an in-memory layout. No WorldServer, save file or universe
  // is opened, and generation/round-trip checks are outside the timed loop.
  auto layout = WorldLayout::buildTerrestrialLayout(session.root, **parameters, seed);
  if (!layout) {
    session.fail(state, layout.error());
    return;
  }
  auto reference = layout->toJson();
  if (!reference) {
    session.fail(state, reference.error());
    return;
  }
  auto loaded = WorldLayout::create(session.root, *reference);
  if (!loaded) {
    session.fail(state, loaded.error());
    return;
  }
  auto roundTrip = (*loaded)->toJson();
  if (!roundTrip) {
    session.fail(state, roundTrip.error());
    return;
  }
  // Biomes contain unordered spawn/plant sets; their serialized array order
  // need not survive loading. Compare geometry, indices and noise, not that
  // incidental set iteration order. create() above validates biome payloads.
  for (std::string_view field : {"worldSize", "terrainSelectors", "layers", "regionBlending",
      "blockNoise", "blendNoise", "playerStartSearchRegions"}) {
    auto original = reference->get(field);
    auto restored = roundTrip->get(field);
    if (!original || !restored || *original != *restored) {
      session.fail(state, "WorldLayout geometry/selectors/noise changed after loading");
      return;
    }
  }
  state.SetLabel(Star::format("{}/{}; seed={}; toJson only, no text encoding", *type, *size, seed));
  for (auto _ : state) {
    (void)_;
    auto stored = layout->toJson();
    if (!stored) {
      session.fail(state, stored.error());
      break;
    }
    benchmark::DoNotOptimize(*stored);
  }
  state.SetItemsProcessed(state.iterations());
}

String nestedProfileProgram() {
  std::string source;
  for (unsigned branch = 0; branch != 8; ++branch) {
    for (unsigned leaf = 0; leaf != 8; ++leaf) {
      source += std::format("function leaf_{}_{}()\n  local sum = 0\n  for i = 1, 128 do sum = sum + i end\n  return sum\nend\n", branch, leaf);
    }
    source += std::format("function branch_{}()\n  local sum = 0\n", branch);
    for (unsigned leaf = 0; leaf != 8; ++leaf)
      source += std::format("  sum = sum + leaf_{}_{}()\n", branch, leaf);
    source += "  return sum\nend\n";
  }
  source += "function benchmarkEntry()\n  local sum = 0\n";
  for (unsigned branch = 0; branch != 8; ++branch)
    source += std::format("  sum = sum + branch_{}()\n", branch);
  source += "  return sum\nend\n";
  return String(std::move(source));
}

std::expected<std::shared_ptr<LuaRoot>, StarException> prepareProfile(Root& root, String const& program) {
  auto lua = LuaRoot::create(root);
  if (!lua)
    return std::unexpected(std::move(lua.error()));
  auto& engine = (*lua)->lua_engine();
  engine.set_profiling_enabled(true);
  engine.set_instruction_measure_interval(100);
  auto executed = [&]() -> std::expected<void, StarException> {
    auto context = (*lua)->create_context();
    if (!context)
      return std::unexpected(std::move(context.error()));
    if (auto loaded = context->load(program, "game_benchmarks_nested_profile"); !loaded)
      return std::unexpected(std::move(loaded.error()));
    auto result = context->eval<int>("benchmarkEntry()");
    if (!result)
      return std::unexpected(std::move(result.error()));
    if (*result != 64 * (128 * 129 / 2))
      return std::unexpected(StarException("Nested profile Lua computation returned incorrect result"));
    return {};
  }();
  if (!executed) {
    // Finish partial engines explicitly too; the caller cleans any output.
    if (auto stopped = (*lua)->shutdown(); !stopped)
      return std::unexpected(std::move(stopped.error()));
    return std::unexpected(std::move(executed.error()));
  }
  return std::move(*lua);
}

struct ProfileShape {
  std::size_t nodes = 0;
  std::size_t depth = 0;
  std::array<bool, 64> leaves{};
};

std::expected<void, StarException> inspectProfile(Json const& entries, std::size_t depth, ProfileShape& shape) {
  auto array = entries.iterate_array();
  if (!array)
    return std::unexpected(std::move(array.error()));
  for (auto const& entry : *array) {
    auto name = entry.get_string("function");
    auto source = entry.get_string("source");
    auto self = entry.get_int("self");
    auto total = entry.get_int("total");
    auto calls = entry.get("calls");
    if (!name) return std::unexpected(std::move(name.error()));
    if (!source) return std::unexpected(std::move(source.error()));
    if (!self) return std::unexpected(std::move(self.error()));
    if (!total) return std::unexpected(std::move(total.error()));
    if (!calls) return std::unexpected(std::move(calls.error()));
    if (source->empty() || *self < 0 || *total < *self)
      return std::unexpected(StarException("Lua profile contains invalid timing/source data"));
    ++shape.nodes;
    shape.depth = std::max(shape.depth, depth);
    for (unsigned branch = 0; branch != 8; ++branch) {
      for (unsigned leaf = 0; leaf != 8; ++leaf) {
        if (*name == Star::format("leaf_{}_{}", branch, leaf))
          shape.leaves[branch * 8 + leaf] = true;
      }
    }
    if (auto inspected = inspectProfile(*calls, depth + 1, shape); !inspected)
      return std::unexpected(std::move(inspected.error()));
  }
  return {};
}

std::expected<void, StarException> requireEmptyProfileDirectory(String const& directory) {
  auto link = File::is_symlink(directory);
  if (!link)
    return std::unexpected(std::move(link.error()));
  if (*link)
    return std::unexpected(StarException("Benchmark Lua profile directory must not be a symlink"));
  auto exists = File::exists(directory);
  if (!exists)
    return std::unexpected(std::move(exists.error()));
  if (!*exists) {
    if (auto made = File::create_directory(directory); !made)
      return std::unexpected(std::move(made.error()));
  }
  auto entries = File::directory_entries(directory);
  if (!entries)
    return std::unexpected(std::move(entries.error()));
  if (!entries->empty())
    return std::unexpected(StarException("Lua export benchmark requires an empty storage/lua directory in private bootconfig storage"));
  return {};
}

std::expected<void, StarException> consumeProfiles(String const& directory, bool validate) {
  auto entries = File::directory_entries(directory);
  if (!entries)
    return std::unexpected(std::move(entries.error()));
  std::optional<StarException> error;
  std::size_t profiles = 0;
  for (auto const& [name, isDirectory] : *entries) {
    // The directory was empty before this owned run. Never recurse or delete
    // unrelated files, even if another process unexpectedly writes here.
    if (isDirectory || !name.ends_with(".luaprofile")) {
      if (!error)
        error = StarException("Unexpected entry in private benchmark Lua directory");
      continue;
    }
    ++profiles;
    auto path = File::relative_to(directory, name);
    if (validate) {
      auto checked = [&]() -> std::expected<void, StarException> {
        auto text = File::read_file_string(path);
        if (!text)
          return std::unexpected(std::move(text.error()));
        auto json = Json::parse_json(*text);
        if (!json)
          return std::unexpected(std::move(json.error()));
        ProfileShape shape;
        if (auto inspected = inspectProfile(*json, 1, shape); !inspected)
          return std::unexpected(std::move(inspected.error()));
        if (shape.depth < 3 || shape.nodes < 73 || !std::ranges::all_of(shape.leaves, [](bool present) { return present; }))
          return std::unexpected(StarException("LuaRoot did not export the expected complete nested profile"));
        benchmark::DoNotOptimize(shape.nodes);
        return {};
      }();
      if (!checked && !error)
        error = std::move(checked.error());
    }
    // Cleanup is outside timing and still runs when validation fails.
    if (auto removed = File::remove(path); !removed && !error)
      error = std::move(removed.error());
  }
  if (error)
    return std::unexpected(std::move(*error));
  if (validate && profiles != 1)
    return std::unexpected(StarException("LuaRoot shutdown must export exactly one profile per iteration"));
  return {};
}

void luaNestedProfileExport(benchmark::State& state, BenchmarkSession& session) {
  auto directory = session.root.toStoragePath("lua");
  if (auto empty = requireEmptyProfileDirectory(directory); !empty) {
    session.fail(state, empty.error());
    return;
  }
  auto program = nestedProfileProgram();
  auto validate = [&]() -> std::expected<void, StarException> {
    auto lua = prepareProfile(session.root, program);
    if (!lua)
      return std::unexpected(std::move(lua.error()));
    return (*lua)->shutdown();
  }();
  auto consumed = consumeProfiles(directory, static_cast<bool>(validate));
  if (!validate || !consumed) {
    session.fail(state, !validate ? validate.error() : consumed.error());
    return;
  }
  state.SetLabel("64 leaves / 8 branches; shutdown + nested JSON + file export; Lua/setup/cleanup untimed");
  for (auto _ : state) {
    (void)_;
    state.PauseTiming();
    auto lua = prepareProfile(session.root, program);
    if (!lua) {
      session.fail(state, lua.error());
      if (auto cleaned = consumeProfiles(directory, false); !cleaned)
        session.fail(state, cleaned.error());
      break;
    }
    state.ResumeTiming();
    auto stopped = (*lua)->shutdown();
    benchmark::DoNotOptimize(stopped.has_value());
    state.PauseTiming();
    lua->reset();
    auto exported = consumeProfiles(directory, static_cast<bool>(stopped));
    if (!stopped || !exported) {
      session.fail(state, !stopped ? stopped.error() : exported.error());
      break;
    }
    state.ResumeTiming();
  }
  state.SetItemsProcessed(state.iterations());
}

}

int main(int argc, char** argv) {
  auto nativeRuntime = NativeRuntime::create();
  if (!nativeRuntime) {
    if (!Star::print(*stderr, "{}\n", nativeRuntime.error().what()))
      return 1;
    return 1;
  }
  // Recognized Google Benchmark flags are removed first. Remaining -bootconfig
  // and RootLoader flags retain their normal parser and error handling.
  Star::BenchmarkLifetime benchmark_lifetime(argc, argv);
  auto loader = RootLoader::create({{}, {}, {}, LogLevel::Error, true, {}});
  if (!loader) {
    if (!Star::print(*stderr, "{}\n", loader.error().what()))
      return 1;
    return 1;
  }
  auto parsed = loader->command_parse_or_die(Star::native_arguments(argc, argv));
  if (!parsed) {
    if (!Star::print(*stderr, "{}\n", parsed.error().what()))
      return 1;
    return 1;
  }
  auto& settings = parsed->first;
  if (auto storage = requirePrivateStorage(settings.storageDirectory); !storage) {
    if (!Star::print(*stderr, "{}\n", storage.error().what()))
      return 1;
    return 1;
  }
  // Never persist configuration or rotate logs, even if extra RootLoader flags
  // requested them. Writes are owned transient Lua profiles/ephemeral worlds.
  settings.runtimeConfigFile.reset();
  settings.logFile.reset();
  auto root = Root::create(std::move(settings));
  if (!root) {
    if (!Star::print(*stderr, "{}\n", root.error().what()))
      return 1;
    return 1;
  }
  BenchmarkSession session{**root};
  for (std::string_view scenario : {"BroadcastDrain32", "ChannelChurn32", "PollPending64"}) {
    benchmark::RegisterBenchmark(std::string("Game/Chat/").append(scenario).c_str(), [&, scenario](benchmark::State& state) {
      ChatProcessor processor;
      for (ConnectionId id = 1; id <= 32; ++id) {
        auto added = processor.connect_client(id);
        if (!added) return session.fail(state, added.error());
      }
      for (ConnectionId id = 1; id <= 32; ++id) {
        auto drained = processor.pull_pending_messages(id);
        if (!drained) return session.fail(state, drained.error());
      }
      if (scenario == "PollPending64") {
        processor.set_command_handler([](ConnectionId, String, String) -> std::expected<ServerCommandResult, StarException> {
          return RpcPromise<String>::make_pair().first;
        });
        for (unsigned i = 0; i != 64; ++i) {
          auto sent = processor.broadcast(1, "/pending");
          if (!sent) return session.fail(state, sent.error());
        }
        for (auto _ : state) {
          auto result = processor.update_promises();
          if (!result) return session.fail(state, result.error());
        }
      } else if (scenario == "ChannelChurn32") {
        std::array<String, 32> channels;
        for (unsigned i = 0; i != channels.size(); ++i) channels[i] = Star::format("channel-{}", i);
        for (auto _ : state) {
          for (auto const& channel : channels) processor.join_channel(1, channel);
          for (auto const& channel : channels) processor.leave_channel(1, channel);
        }
      } else {
        String const text("The quick brown fox jumps over the lazy dog.");
        for (auto _ : state) {
          auto sent = processor.broadcast(1, text);
          if (!sent) return session.fail(state, sent.error());
          for (ConnectionId id = 1; id <= 32; ++id) {
            auto result = processor.pull_pending_messages(id);
            if (!result) return session.fail(state, result.error());
            benchmark::DoNotOptimize(*result);
          }
        }
      }
    });
  }
  for (std::string_view scenario : {"ColdFourChunks", "HotSatelliteName", "LoadedSlaveScan"}) {
    benchmark::RegisterBenchmark(std::string("Game/Celestial/").append(scenario).c_str(), [&, scenario](benchmark::State& state) {
      auto master = CelestialMasterDatabase::create(session.root);
      if (!master) return session.fail(state, master.error());
      auto base = (*master)->base_information();
      int const side = base.chunk_size;
      RectI region(Vec2I(-side, -side), Vec2I(side, side));
      auto systems = (*master)->scan_systems(region);
      if (!systems) return session.fail(state, systems.error());
      std::optional<CelestialCoordinate> moon;
      std::uint64_t fingerprint = 14695981039346656037ull;
      auto mix = [&](std::uint64_t v) { fingerprint = (fingerprint ^ v) * 1099511628211ull; };
      auto record = [&](CelestialCoordinate const& coord) -> std::expected<void, StarException> {
        auto p = (*master)->parameters(coord);
        if (!p) return std::unexpected(std::move(p.error()));
        if (!*p) return std::unexpected(StarException("Missing generated body"));
        mix((**p).seed());
        for (auto const& s : {coord.id(), (**p).name()}) {
          for (unsigned char c : s.utf8()) mix(c);
          mix(256);
        }
        return {};
      };
      for (auto const& system : *systems) {
        if (auto r = record(system); !r) return session.fail(state, r.error());
        auto planets = (*master)->children(system);
        if (!planets) return session.fail(state, planets.error());
        for (auto const& planet : *planets) {
          if (auto r = record(planet); !r) return session.fail(state, r.error());
          auto satellites = (*master)->children(planet);
          if (!satellites) return session.fail(state, satellites.error());
          for (auto const& satellite : *satellites) {
            if (!moon) moon = satellite;
            if (auto r = record(satellite); !r) return session.fail(state, r.error());
          }
        }
      }
      if (!moon) return session.fail(state, "Expected a moon in the fixed four-chunk fixture");
      auto lines = (*master)->scan_constellation_lines(region);
      if (!lines) return session.fail(state, lines.error());
      for (auto const& [from, to] : *lines) {
        mix(from[0]); mix(from[1]); mix(to[0]); mix(to[1]);
      }
      state.SetLabel(std::to_string(fingerprint));
      if (scenario == "ColdFourChunks") {
        for (auto _ : state) {
          state.PauseTiming();
          auto fresh = CelestialMasterDatabase::create(session.root);
          if (!fresh) { state.ResumeTiming(); return session.fail(state, fresh.error()); }
          state.ResumeTiming();
          auto found = (*fresh)->scan_systems(region);
          if (!found) return session.fail(state, found.error());
          benchmark::DoNotOptimize(*found);
          state.PauseTiming();
          fresh->reset();
          state.ResumeTiming();
        }
      } else if (scenario == "HotSatelliteName") {
        for (auto _ : state) {
          auto name = (*master)->name(*moon);
          if (!name) return session.fail(state, name.error());
          benchmark::DoNotOptimize(*name);
        }
      } else {
        auto assets = session.root.assets();
        if (!assets) return session.fail(state, assets.error());
        auto slave = CelestialSlaveDatabase::create(*assets, session.root.random(), base);
        if (!slave) return session.fail(state, slave.error());
        auto signaled = (*slave)->signal_region(region);
        if (!signaled) return session.fail(state, signaled.error());
        List<CelestialResponse> responses;
        for (auto const& request : (*slave)->pull_requests()) {
          auto response = (*master)->respond_to_request(request);
          if (!response) return session.fail(state, response.error());
          responses.push_back(std::move(*response));
        }
        auto pushed = (*slave)->push_responses(std::move(responses));
        if (!pushed) return session.fail(state, pushed.error());
        auto loaded = (*slave)->scan_systems(region);
        if (!loaded) return session.fail(state, loaded.error());
        if (*loaded != *systems) return session.fail(state, "Slave and master system scans differ");
        for (auto _ : state) {
          auto found = (*slave)->scan_systems(region);
          if (!found) return session.fail(state, found.error());
          benchmark::DoNotOptimize(*found);
        }
      }
    });
  }
  for (std::string_view scenario : {"FirstTree", "Build16", "Sample128", "RandomSample128"}) {
    benchmark::RegisterBenchmark(std::string("Game/Biome/").append(scenario).c_str(), [&, scenario](benchmark::State& state) {
      auto plants = session.root.plantDatabase();
      if (!plants) return session.fail(state, plants.error());
      auto stems = (*plants)->treeStemNames();
      if (!stems) return session.fail(state, stems.error());
      auto leaves = (*plants)->treeFoliageNames();
      std::ranges::sort(*stems);
      std::ranges::sort(leaves);
      if (stems->empty()) return session.fail(state, "No tree stems");
      JsonArray stem_names, leaf_names{""};
      for (auto const& stem : *stems | std::views::take(8)) stem_names.emplace_back(stem);
      for (auto const& leaf : leaves | std::views::take(8)) leaf_names.emplace_back(leaf);
      bool const random = scenario == "RandomSample128";
      Json config = JsonObject{{"type", "tree"}, {"variants", 16}, {"treeStemList", std::move(stem_names)},
          {"treeFoliageList", std::move(leaf_names)}, {"treeStemHueShiftMax", 90}, {"treeFoliageHueShiftMax", 60},
          {"distribution", random ? JsonObject{{"type", "random"}, {"blockProbability", 0.375}}
                                  : JsonObject{{"type", "periodic"}, {"modulus", 3}, {"modulusVariance", 0.5}}}};
      constexpr std::uint64_t seed = 0xfedcba9876543210ULL;
      auto built = BiomeItemDistribution::create(session.root, config, seed);
      if (!built) return session.fail(state, built.error());
      BiomePlaceables placeables;
      placeables.item_distributions.push_back(std::move(*built));
      auto const& distribution = placeables.item_distributions.front();
      std::uint64_t hash = 14695981039346656037ull;
      auto mix = [&](std::uint32_t value) { hash = (hash ^ value) * 1099511628211ull; };
      auto mix_tree = [&](TreeVariant const& tree) {
        for (auto const& name : {std::cref(tree.stemName), std::cref(tree.foliageName)}) {
          for (unsigned char c : name.get().utf8()) mix(c);
          mix(256);
        }
        mix(std::bit_cast<std::uint32_t>(tree.stemHueShift));
        mix(std::bit_cast<std::uint32_t>(tree.foliageHueShift));
      };
      auto store = distribution.to_json();
      if (!store) return session.fail(state, store.error());
      auto candidates = store->get_array(random ? String("randomItems") : String("weightedItems"));
      if (!candidates) return session.fail(state, candidates.error());
      auto assets = session.root.assets();
      if (!assets) return session.fail(state, assets.error());
      for (auto const& entry : *candidates) {
        auto payload = random ? std::expected<Json, StarException>(entry) : entry.get(0);
        if (!payload) return session.fail(state, payload.error());
        auto item = variant_to_biome_item(**assets, *payload);
        if (!item) return session.fail(state, item.error());
        auto const& trees = std::get<TreePair>(*item);
        mix_tree(trees.first);
        mix_tree(trees.second);
      }
      for (int x = -64; x < 64; ++x) {
        auto item = distribution.item_at(x, 17);
        if (!item) return session.fail(state, item.error());
        mix(item->has_value());
        if (*item) mix_tree(std::get<TreePair>((*item)->get()).first);
      }
      state.SetLabel(std::to_string(hash));
      for (auto _ : state) {
        if (scenario == "FirstTree") {
          auto first = placeables.first_tree();
          benchmark::DoNotOptimize(first);
        } else if (scenario == "Build16") {
          auto result = BiomeItemDistribution::create(session.root, config, seed);
          if (!result) return session.fail(state, result.error());
          benchmark::DoNotOptimize(*result);
        } else {
          for (int x = -64; x < 64; ++x) {
            auto result = distribution.item_at(x, 17);
            if (!result) return session.fail(state, result.error());
            benchmark::DoNotOptimize(*result);
          }
        }
      }
    });
  }
  for (std::string_view scenario : {"WideEarlyExit", "Notify32", "Resume"}) {
    benchmark::RegisterBenchmark(std::string("Game/Behavior/").append(scenario).c_str(), [&, scenario](benchmark::State& state) {
      auto lua = LuaRoot::create(session.root);
      if (!lua) return session.fail(state, lua.error());
      auto context = (*lua)->create_context();
      if (!context) return session.fail(state, context.error());
      std::string_view program = scenario == "Resume" ? "function probe() while true do coroutine.yield(nil) end end"
          : scenario == "Notify32" ? "function probe() return true end" : "function probe() return false end";
      auto loaded = context->load(String(program), "behavior_probe");
      if (!loaded) return session.fail(state, loaded.error());
      auto env = context->eval<LuaTable>("_ENV");
      if (!env) return session.fail(state, env.error());
      List<std::shared_ptr<BehaviorNode const>> children;
      for (unsigned i = 0; i != 32; ++i) {
        StringMap<NodeParameter> parameters;
        parameters.set("input", {NodeParameterType::Number, String("source")});
        children.push_back(std::make_shared<BehaviorNode>(ActionNode("probe", std::move(parameters), {})));
      }
      auto tree = std::make_shared<BehaviorTree>();
      tree->root = std::make_shared<BehaviorNode>(CompositeNode(SequenceNode(std::move(children))));
      tree->functions.add("probe");
      auto behavior = BehaviorState::create(std::move(tree), *env);
      if (!behavior) return session.fail(state, behavior.error());
      auto warm = (*behavior)->run(1.f / 60);
      if (!warm) return session.fail(state, warm.error());
      auto expected = scenario == "Resume" ? NodeStatus::Running : scenario == "Notify32" ? NodeStatus::Success : NodeStatus::Failure;
      if (*warm != expected) return session.fail(state, "Unexpected behavior node status");
      auto board = (*behavior)->blackboard().lock();
      String const key = "source";
      for (auto _ : state) {
        if (scenario == "Notify32") {
          auto set = board->set(NodeParameterType::Number, key, LuaInt{42});
          if (!set) return session.fail(state, set.error());
        } else {
          auto status = (*behavior)->run(1.f / 60);
          if (!status) return session.fail(state, status.error());
          if (*status != expected) return session.fail(state, "Behavior node status changed");
        }
      }
    });
  }
  for (bool changing : {false, true}) {
    benchmark::RegisterBenchmark(changing ? "Game/Sprite/AdvanceFrame" : "Game/Sprite/SameFrame", [&, changing](benchmark::State& state) {
      auto animation = Animation::create(session.root, JsonObject{{"mode", "LoopForever"}, {"frameNumber", 8}, {"frames", "/sprite.png:<index>"}});
      if (!animation) return session.fail(state, animation.error());
      for (auto _ : state) {
        animation->update(changing ? 0.125f : 0.0f);
        benchmark::DoNotOptimize(*animation);
      }
    });
  }
  for (bool processed : {false, true}) {
    benchmark::RegisterBenchmark(processed ? "Game/Sprite/CenteredBorder" : "Game/Sprite/TaggedDrawable", [&, processed](benchmark::State& state) {
      auto animation = Animation::create(session.root, JsonObject{{"frames", "/<image>.png?multiply=ffffff0<index>"}, {"centered", processed}, {"processing", processed ? Json("?border=1;ffffffff") : Json("")}});
      if (!animation) return session.fail(state, animation.error());
      animation->set_tag("image", "assetmissing");
      auto warm = animation->drawable(1.0f);
      if (!warm) return session.fail(state, warm.error());
      for (auto _ : state) {
        auto frame = animation->drawable(1.0f);
        if (!frame) return session.fail(state, frame.error());
        benchmark::DoNotOptimize(*frame);
      }
    });
  }
  benchmark::RegisterBenchmark("Game/Ambient/StablePlayback16", [&](benchmark::State& state) {
    AmbientManager manager(session.root);
    auto description = std::make_shared<AmbientNoisesDescription>();
    for (int i = 0; i < 16; ++i)
      description->day_sounds.tracks.push_back("/sfx/interface/voice_on.ogg");
    auto assets = session.root.assets();
    if (!assets) return session.fail(state, assets.error());
    auto loaded = (*assets)->audio("/sfx/interface/voice_on.ogg");
    if (!loaded) return session.fail(state, loaded.error());
    auto initial = manager.update_ambient(description, true);
    if (!initial || !*initial) return session.fail(state, "Ambient fixture did not start");
    for (auto _ : state) {
      auto next = manager.update_ambient(description, true);
      if (!next || *next) return session.fail(state, "Stable ambient unexpectedly changed");
      benchmark::DoNotOptimize(next);
    }
  });
  benchmark::RegisterBenchmark("Game/Ambient/NoWeather", [&](benchmark::State& state) {
    AmbientManager manager(session.root);
    for (auto _ : state) {
      auto next = manager.update_weather({});
      if (!next || *next) return session.fail(state, "Unexpected weather track");
      benchmark::DoNotOptimize(next);
    }
  });
  benchmark::RegisterBenchmark("Game/Ambient/FloatingOverrides", [&](benchmark::State& state) {
    auto parameters = std::make_shared<FloatingDungeonWorldParameters>();
    parameters->worldSize = {256, 256};
    parameters->day_ambient_noises = "/sfx/interface/voice_on.ogg";
    parameters->night_music_track = "/music/arctic-constellation1.ogg";
    auto world = WorldTemplate::create(session.root, parameters->worldSize);
    if (!world) return session.fail(state, world.error());
    auto configured = (*world)->setWorldParameters(parameters);
    if (!configured) return session.fail(state, configured.error());
    for (auto _ : state) {
      auto ambient = (*world)->ambient_noises(128, 128);
      auto music = (*world)->music_track(128, 128);
      if (!ambient || !*ambient || !music || !*music) return session.fail(state, "Missing floating override");
      benchmark::DoNotOptimize(ambient);
      benchmark::DoNotOptimize(music);
    }
  });
  benchmark::RegisterBenchmark("Game/AiState/ReadMissionLists", [](benchmark::State& state) {
    JsonArray missions;
    for (unsigned i = 0; i < 64; ++i)
      missions.emplace_back(Star::format("mission_with_nontrivial_name_{}", i));
    Json config = JsonObject{{"availableMissions", missions}, {"completedMissions", std::move(missions)}};
    for (auto _ : state) {
      auto decoded = AiState::create(config);
      if (!decoded) {
        state.SkipWithError(decoded.error().what());
        break;
      }
      benchmark::DoNotOptimize(decoded);
    }
  });
  benchmark::RegisterBenchmark("Warp/LongEntityTargetToJson", [](benchmark::State& state) {
    SpawnTarget const target{SpawnTargetUniqueEntity(String(std::string(512, 'x')))};
    for (auto _ : state) {
      auto json = spawn_target_to_json(target);
      benchmark::DoNotOptimize(json);
    }
  });
  benchmark::RegisterBenchmark("ObjectDatabase/ColdConfigParse_WarmAssets", [&](benchmark::State& state) {
    objectColdParse(state, session);
  })->Iterations(8)->UseRealTime()->Unit(benchmark::kMicrosecond);
  benchmark::RegisterBenchmark("ObjectDatabase/WarmConfigLookup", [&](benchmark::State& state) {
    objectWarmLookup(state, session);
  })->MinTime(0.1)->UseRealTime();
  benchmark::RegisterBenchmark("ProjectileDatabase/ColdEntireCatalogParse_WarmAssets", [&](benchmark::State& state) {
    projectileColdCatalogParse(state, session);
  })->Iterations(3)->UseRealTime()->Unit(benchmark::kMillisecond);
  benchmark::RegisterBenchmark("WorldLayout/GeneratedTerrestrialToJson", [&](benchmark::State& state) {
    worldLayoutToJson(state, session);
  })->Iterations(64)->UseRealTime()->Unit(benchmark::kMicrosecond);
  benchmark::RegisterBenchmark("LuaRoot/NestedProfileShutdownAndExport", [&](benchmark::State& state) {
    luaNestedProfileExport(state, session);
  })->Iterations(8)->UseRealTime()->Unit(benchmark::kMicrosecond);
  benchmark::RegisterBenchmark(
    "Game/AStar/flat_walk8",
    [&](benchmark::State& state) { PlatformerBenchmarks::benchmark_platformer(state, session, false, 8, 0); })
    ->MinTime(0.1)
    ->UseRealTime()
    ->Unit(benchmark::kMicrosecond);
  benchmark::RegisterBenchmark(
    "Game/AStar/flat_walk32_budget4",
    [&](benchmark::State& state) { PlatformerBenchmarks::benchmark_platformer(state, session, false, 32, 4); })
    ->MinTime(0.1)
    ->UseRealTime()
    ->Unit(benchmark::kMicrosecond);
  benchmark::RegisterBenchmark(
    "Game/AStar/fly8",
    [&](benchmark::State& state) { PlatformerBenchmarks::benchmark_platformer(state, session, true, 8, 0); })
    ->MinTime(0.1)
    ->UseRealTime()
    ->Unit(benchmark::kMicrosecond);
  benchmark::RegisterBenchmark(
    "Game/AStar/fly32_budget4",
    [&](benchmark::State& state) { PlatformerBenchmarks::benchmark_platformer(state, session, true, 32, 4); })
    ->MinTime(0.1)
    ->UseRealTime()
    ->Unit(benchmark::kMicrosecond);
  register_player_storage_benchmarks(**root, session.failed);
  auto result = Star::run_registered_benchmarks(true, false);
  // Remove callbacks borrowing session/Root before those owners leave scope.
  benchmark::ClearRegisteredBenchmarks();
  return session.failed || result != 0 ? 1 : 0;
}

#include <cstdio>
#include <benchmark/benchmark.h>

import std;
import star.assets;
import star.asset_path;
import star.file;
import star.format;
import star.json;
import star.list;
import star.logging;
import star.lua;
import star.lua_converters;
import star.lua_root;
import star.native_runtime;
import star.object_database;
import star.projectile_database;
import star.root;
import star.root_loader;
import star.string;
import star.world_layout;
import star.world_parameters;

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

struct BenchmarkLifetime {
  ~BenchmarkLifetime() {
    benchmark::ClearRegisteredBenchmarks();
    benchmark::Shutdown();
  }
};

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
    auto jsonName = (*config)->config.getString("objectName");
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
  auto files = (*assets)->scanExtension("projectile");
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
    auto name = input->getString("projectileName");
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
  auto entries = config.getObject(group);
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
  auto& engine = (*lua)->luaEngine();
  engine.setProfilingEnabled(true);
  engine.setInstructionMeasureInterval(100);
  auto executed = [&]() -> std::expected<void, StarException> {
    auto context = (*lua)->createContext();
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
  auto array = entries.iterateArray();
  if (!array)
    return std::unexpected(std::move(array.error()));
  for (auto const& entry : *array) {
    auto name = entry.getString("function");
    auto source = entry.getString("source");
    auto self = entry.getInt("self");
    auto total = entry.getInt("total");
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
        auto json = Json::parseJson(*text);
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
    if (!Star::print(stderr, "{}\n", nativeRuntime.error().what()))
      return 1;
    return 1;
  }
  // Recognized Google Benchmark flags are removed first. Remaining -bootconfig
  // and RootLoader flags retain their normal parser and error handling.
  benchmark::Initialize(&argc, argv);
  BenchmarkLifetime benchmarkLifetime;
  auto loader = RootLoader::create({{}, {}, {}, LogLevel::Error, true, {}});
  if (!loader) {
    if (!Star::print(stderr, "{}\n", loader.error().what()))
      return 1;
    return 1;
  }
  auto parsed = loader->commandParseOrDie(argc, argv);
  if (!parsed) {
    if (!Star::print(stderr, "{}\n", parsed.error().what()))
      return 1;
    return 1;
  }
  auto& settings = parsed->first;
  if (auto storage = requirePrivateStorage(settings.storageDirectory); !storage) {
    if (!Star::print(stderr, "{}\n", storage.error().what()))
      return 1;
    return 1;
  }
  // Never persist configuration or rotate logs, even if extra RootLoader flags
  // requested them. The only benchmark writes are owned transient Lua profiles.
  settings.runtimeConfigFile.reset();
  settings.logFile.reset();
  auto root = Root::create(std::move(settings));
  if (!root) {
    if (!Star::print(stderr, "{}\n", root.error().what()))
      return 1;
    return 1;
  }
  BenchmarkSession session{**root};
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
  register_player_storage_benchmarks(**root, session.failed);
  auto matched = benchmark::RunSpecifiedBenchmarks();
  // Remove callbacks borrowing session/Root before those owners leave scope.
  benchmark::ClearRegisteredBenchmarks();
  return session.failed || matched == 0 ? 1 : 0;
}

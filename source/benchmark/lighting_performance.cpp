#include <benchmark/benchmark.h>

import std;
import star.assets;
import star.cellular_lighting;
import star.format;
import star.json;
import star.liquid_types;
import star.liquids_database;
import star.material_database;
import star.material_types;
import star.multi_array;
import star.performance_benchmarks;
import star.rect;
import star.root;
import star.string;
import star.tile_sector_array;
import star.vector;
import star.worker_pool;
import star.world_client;
import star.world_tiles;

namespace Star {
namespace {

struct MaterialSample {
  MaterialId id;
  bool foregroundTransparent;
  bool backgroundTransparent;
};

struct LightingFixture {
  std::shared_ptr<MaterialDatabase const> materials;
  std::shared_ptr<LiquidsDatabase const> liquids;
  Json parameters;
  ClientTileSectorArray tiles{{384, 192}};
  WorkerPool callerWorkers{"Lighting benchmark caller"};
  Vec3F environment{0.19f, 0.23f, 0.31f};
  float underground = 42.0f;
};

std::expected<std::shared_ptr<LightingFixture>, StarException> prepareLighting(Root& root) {
  auto materials = root.material_database();
  if (!materials)
    return std::unexpected(std::move(materials.error()));
  auto liquids = root.liquids_database();
  if (!liquids)
    return std::unexpected(std::move(liquids.error()));
  auto assets = root.assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  auto parameters = (*assets)->json("/lighting.config:lighting");
  if (!parameters)
    return std::unexpected(std::move(parameters.error()));
  auto additive = parameters->set("pointAdditive", true);
  if (!additive)
    return std::unexpected(std::move(additive.error()));

  auto fixture = std::make_shared<LightingFixture>();
  fixture->materials = std::move(*materials);
  fixture->liquids = std::move(*liquids);
  fixture->parameters = std::move(*additive);
  std::vector<MaterialSample> samples;
  std::vector<MaterialSample> radiantSamples;
  auto names = fixture->materials->material_names();
  std::ranges::sort(names);
  for (auto const& name : names) {
    auto id = fixture->materials->material_id(name);
    if (!id)
      return std::unexpected(std::move(id.error()));
    if (!isRealMaterial(*id))
      continue;
    auto foreground = fixture->materials->foreground_light_transparent(*id);
    if (!foreground)
      return std::unexpected(std::move(foreground.error()));
    auto background = fixture->materials->background_light_transparent(*id);
    if (!background)
      return std::unexpected(std::move(background.error()));
    MaterialSample sample{*id, *foreground, *background};
    samples.push_back(sample);
    if (fixture->materials->radiant_light(*id, NoModId).max() > 0.0f)
      radiantSamples.push_back(sample);
  }
  std::vector<ModId> mods{NoModId};
  auto mod_names = fixture->materials->mod_names();
  std::ranges::sort(mod_names);
  for (auto const& name : mod_names) {
    auto id = fixture->materials->mod_id(name);
    if (!id)
      return std::unexpected(std::move(id.error()));
    mods.push_back(*id);
  }
  std::vector<LiquidId> liquid_ids;
  auto liquid_names = fixture->liquids->liquid_names();
  std::ranges::sort(liquid_names);
  for (auto const& name : liquid_names) {
    auto id = fixture->liquids->liquid_id(name);
    if (!id)
      return std::unexpected(std::move(id.error()));
    if (*id != empty_liquid_id)
      liquid_ids.push_back(*id);
  }
  if (samples.empty() || radiantSamples.empty() || liquid_ids.empty())
    return std::unexpected(StarException("Lighting benchmarks need real materials, radiant materials and liquids"));

  auto sectors = fixture->tiles.valid_sectors_for(RectI(0, 0, 384, 192));
  if (!sectors)
    return std::unexpected(std::move(sectors.error()));
  for (auto const& sector : *sectors) {
    // Keep genuine unloaded-sector gaps, including near the wrapping boundary.
    if ((sector[0] * 3 + sector[1]) % 11 != 0)
      fixture->tiles.load_default_sector(sector);
  }
  auto filled = fixture->tiles.tile_eval(
    RectI(0, 0, 384, 192), [&](Vec2I const& pos, ClientTile& tile) -> std::expected<void, StarException> {
      unsigned key = static_cast<unsigned>(pos[0] * 73 + pos[1] * 151);
      auto const& foreground =
        key % 17 == 0 ? radiantSamples[(key / 17) % radiantSamples.size()] : samples[key % samples.size()];
      auto const& background = samples[(key / 7) % samples.size()];
      tile.foreground = key % 5 < 2 ? foreground.id : EmptyMaterialId;
      tile.foregroundMod = key % 7 == 0 ? mods[(key / 7) % mods.size()] : NoModId;
      tile.background = key % 3 == 0 ? background.id : EmptyMaterialId;
      tile.backgroundMod = key % 13 == 0 ? mods[(key / 13) % mods.size()] : NoModId;
      tile.foregroundLightTransparent = tile.foreground == EmptyMaterialId || foreground.foregroundTransparent;
      tile.backgroundLightTransparent = tile.background == EmptyMaterialId || background.backgroundTransparent;
      // Dynamic object collision can make otherwise transparent foreground opaque.
      if (pos[0] % 31 == 0 && pos[1] % 13 != 0)
        tile.foregroundLightTransparent = false;
      tile.foregroundHueShift = static_cast<MaterialHue>(key);
      tile.backgroundHueShift = static_cast<MaterialHue>(key / 3);
      tile.liquid =
        key % 9 == 0 ? LiquidLevel(liquid_ids[(key / 9) % liquid_ids.size()], (key % 5) * 0.25f) : LiquidLevel();
      return {};
    });
  if (!filled)
    return std::unexpected(std::move(filled.error()));
  return fixture;
}

// Independent, serial tile-level oracle. In particular unloaded and vertically
// out-of-world cells retain begin()'s zero values rather than gathering defaults.
std::expected<void, StarException> gatherReference(LightingFixture const& fixture,
                                                   CellularLightingCalculator& calculator) {
  auto region = calculator.calculation_region();
  for (int x = region.x_min(); x < region.x_max(); ++x) {
    for (int y = region.y_min(); y < region.y_max(); ++y) {
      Vec2I position{x, y};
      if (y < 0 || y >= static_cast<int>(fixture.tiles.size()[1]) || !fixture.tiles.tile_loaded(position))
        continue;
      auto const& tile = fixture.tiles.tile(position);
      Vec3F light;
      if (tile.foreground != EmptyMaterialId || tile.foregroundMod != NoModId)
        light += fixture.materials->radiant_light(tile.foreground, tile.foregroundMod);
      if (tile.liquid.liquid != empty_liquid_id && tile.liquid.level != 0.0f)
        light += fixture.liquids->radiant_light(tile.liquid);
      if (tile.foregroundLightTransparent) {
        if (tile.background != EmptyMaterialId || tile.backgroundMod != NoModId)
          light += fixture.materials->radiant_light(tile.background, tile.backgroundMod);
        if (tile.backgroundLightTransparent && y > fixture.underground)
          light += fixture.environment;
      }
      calculator.set_cell_index(calculator.base_index_for(position), light, !tile.foregroundLightTransparent);
    }
  }
  return {};
}

void addFixtureLights(CellularLightingCalculator& calculator, RectI const& query, int sourceKind) {
  // Fractional positions, beams, hybrid lights and sources just outside the
  // query exercise the production border/obstacle paths as well as interiors.
  for (unsigned i = 0; i < 20; ++i) {
    Vec2F position{query.x_min() - 2.25f + (i * 37 % (query.width() + 4)),
                   query.y_min() - 1.5f + (i * 23 % (query.height() + 4))};
    Vec3F color{0.21f + (i % 3) * 0.31f, 0.17f + (i % 5) * 0.14f, 0.11f + (i % 7) * 0.11f};
    if (sourceKind == 0 || (sourceKind == 2 && i % 3 == 0))
      calculator.add_spread_light(position, color);
    else if (sourceKind == 2 && i % 3 == 2) {
      calculator.add_spread_light(position, color * 0.85f);
      calculator.add_point_light(position, color, 0.7f, i * 0.37f, 0.15f, true);
    } else
      calculator.add_point_light(position, color, i % 2 == 0 ? 0.0f : 1.4f, i * 0.37f, 0.1f);
  }
}

bool sameLightmap(Lightmap const& output, std::vector<float> const& reference, RectI const& query) {
  auto channels = output.span();
  return output.size() == Vec2U(query.size()) && channels.size() == reference.size()
    && std::memcmp(channels.data(), reference.data(), channels.size_bytes()) == 0;
}

std::uint64_t lightDigest(std::span<float const> values) {
  std::uint64_t digest = 14695981039346656037ull;
  for (float value : values) {
    for (auto byte : std::bit_cast<std::array<unsigned char, sizeof(float)>>(value)) {
      digest ^= byte;
      digest *= 1099511628211ull;
    }
  }
  return digest;
}

enum class LightingWork { Gather, Propagate, GatherAndPropagate };

void lightingBenchmark(benchmark::State& state, PerformanceBenchmarkSession& session, LightingFixture& fixture,
                       bool monochrome, bool edge, int sourceKind, bool caller, LightingWork work) {
  auto rootWorkers = session.root.workers();
  if (!rootWorkers) {
    session.fail(state, rootWorkers.error());
    return;
  }
  WorkerPool& workers = caller ? fixture.callerWorkers : rootWorkers->get();
  RectI query = RectI::with_size(edge ? Vec2I{-12, -6} : Vec2I{73, 51},
                                Vec2I{static_cast<int>(state.range(0)), static_cast<int>(state.range(1))});
  CellularLightingCalculator calculator(monochrome);
  auto configured = calculator.set_parameters(fixture.parameters);
  if (!configured) {
    session.fail(state, configured.error());
    return;
  }
  if (auto begun = calculator.begin(query); !begun) {
    session.fail(state, begun.error());
    return;
  }
  auto referenceGathered = gatherReference(fixture, calculator);
  if (!referenceGathered) {
    session.fail(state, referenceGathered.error());
    return;
  }
  addFixtureLights(calculator, query, sourceKind);
  Lightmap output;
  auto referenceCalculated = calculator.calculate(output);
  if (!referenceCalculated) {
    session.fail(state, referenceCalculated.error());
    return;
  }
  std::vector<float> reference(std::from_range, output.span());
  state.SetLabel(std::format("snapshot={:016x}; loaded={}; workers={}", lightDigest(reference),
                             fixture.tiles.loaded_sector_count(), workers.worker_count()));
  auto gather = [&]() {
    return gatherClientLightingTiles(calculator, fixture.tiles, workers, *fixture.materials, *fixture.liquids,
                                     fixture.environment, fixture.underground);
  };
  auto calculationRegion = calculator.calculation_region();
  state.counters["gather_cells"] = calculationRegion.width() * calculationRegion.height();
  for (auto _ : state) {
    (void)_;
    state.PauseTiming();
    if (auto begun = calculator.begin(query); !begun) {
      session.fail(state, begun.error());
      return;
    }
    if (work == LightingWork::Propagate) {
      auto gathered = gather();
      if (!gathered) {
        session.fail(state, gathered.error());
        break;
      }
      addFixtureLights(calculator, query, sourceKind);
    }
    state.ResumeTiming();
    if (work != LightingWork::Propagate) {
      auto gathered = gather();
      if (!gathered) {
        session.fail(state, gathered.error());
        break;
      }
    }
    if (work == LightingWork::Gather)
      state.PauseTiming();
    if (work != LightingWork::Propagate)
      addFixtureLights(calculator, query, sourceKind);
    auto calculated = calculator.calculate(output);
    if (!calculated) {
      session.fail(state, calculated.error());
      break;
    }
    if (work != LightingWork::Gather)
      state.PauseTiming();
    if (!sameLightmap(output, reference, query)) {
      session.fail(state, "Lighting output differs from the serial tile snapshot");
      break;
    }
    benchmark::DoNotOptimize(output.span().data());
    state.ResumeTiming();
  }
  state.SetItemsProcessed(state.iterations() * query.width() * query.height());
}

// Isolate the traversal used by the render-data projection, without measuring
// lighting propagation, GPU work or allocation of a fresh viewport each frame.
void sector_projection(benchmark::State& state, PerformanceBenchmarkSession& session, LightingFixture const& fixture,
                       WorkerPool& workers, RectI const& query) {
  MultiArray<MaterialId, 2> output(Vec2S(query.size()));
  auto project = [&] {
    return fixture.tiles.tile_each_to(workers, output, query,
      [](MaterialId& dest, Vec2I const&, ClientTile const& tile) -> std::expected<void, StarException> {
        dest = tile.foreground;
        return {};
      });
  };
  auto first = project();
  if (!first) {
    session.fail(state, first.error());
    return;
  }
  for (int x = query.x_min(); x < query.x_max(); ++x) {
    for (int y = query.y_min(); y < query.y_max(); ++y) {
      if (output(x - query.x_min(), y - query.y_min()) != fixture.tiles.tile({x, y}).foreground) {
        session.fail(state, "Sector projection differs from point lookup");
        return;
      }
    }
  }
  state.counters["workers"] = workers.worker_count();
  for (auto _ : state) {
    auto result = project();
    if (!result) {
      session.fail(state, result.error());
      break;
    }
    benchmark::ClobberMemory();
  }
  state.SetItemsProcessed(state.iterations() * query.width() * query.height());
}

}// namespace

std::expected<void, StarException> registerLightingPerformanceBenchmarks(PerformanceBenchmarkSession& session) {
  auto fixture = prepareLighting(session.root);
  if (!fixture)
    return std::unexpected(std::move(fixture.error()));
  auto root_workers = session.root.workers();
  if (!root_workers)
    return std::unexpected(std::move(root_workers.error()));
  for (auto const& [name, query] : std::array{
         std::pair{"SingleSector", RectI(36, 4, 60, 28)},
         std::pair{"CrossSector", RectI(52, 20, 76, 44)},
         std::pair{"Sparse", RectI(0, 0, 48, 32)},  // Sector (0, 0) is unloaded.
         std::pair{"LargeWrap", RectI(-24, -6, 296, 164)}}) {
    for (bool caller : {true, false}) {
      auto& workers = caller ? (*fixture)->callerWorkers : root_workers->get();
      auto label = std::format("Sector/Project/{}/{}", name, caller ? "Caller" : "Workers");
      benchmark::RegisterBenchmark(label.c_str(), [&, fixture = *fixture, query](benchmark::State& state) {
        sector_projection(state, session, *fixture, workers, query);
      })->UseRealTime();
    }
  }
  for (bool monochrome : {false, true}) {
    for (bool edge : {false, true}) {
      for (bool caller : {false, true}) {
        auto name = std::format("Lighting/Gather/{}/{}/{}", monochrome ? "Monochrome" : "Colored",
                                edge ? "WrapEdge" : "Interior", caller ? "Caller" : "Workers");
        benchmark::RegisterBenchmark(name.c_str(),
                                     [&, fixture = *fixture, monochrome, edge, caller](benchmark::State& state) {
                                       lightingBenchmark(state, session, *fixture, monochrome, edge, 2, caller,
                                                         LightingWork::Gather);
                                     })
          ->Args({32, 24})
          ->Args({160, 96})
          ->UseRealTime();
      }
      for (int sourceKind : {0, 1, 2}) {
        for (LightingWork work : {LightingWork::Propagate, LightingWork::GatherAndPropagate}) {
          auto name =
            std::format("Lighting/{}/{}/{}/{}", work == LightingWork::Propagate ? "Propagate" : "GatherAndPropagate",
                        monochrome ? "Monochrome" : "Colored", edge ? "WrapEdge" : "Interior",
                        sourceKind == 0     ? "Spread"
                          : sourceKind == 1 ? "Point"
                                            : "Mixed");
          benchmark::RegisterBenchmark(
            name.c_str(),
            [&, fixture = *fixture, monochrome, edge, sourceKind, work](benchmark::State& state) {
              lightingBenchmark(state, session, *fixture, monochrome, edge, sourceKind, false, work);
            })
            ->Args({160, 96})
            ->UseRealTime();
        }
      }
    }
  }
  return {};
}

}// namespace Star

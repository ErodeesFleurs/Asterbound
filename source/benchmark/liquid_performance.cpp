#include <benchmark/benchmark.h>

import std;
import star.cellular_liquid;
import star.format;
import star.liquid_types;
import star.liquids_database;
import star.list;
import star.performance_benchmarks;
import star.rect;
import star.root;
import star.vector;

namespace Star {
namespace {

struct LiquidScenario {
  std::string_view name;
  int width;
  int height;
  std::optional<unsigned> budget;
  bool monitored;
  bool multiple_liquids;
  bool read_recovery;
  bool steady_windows = false;
};

// This is the engine's real environment boundary: a finite, horizontally wrapped
// basin with impermeable floor/ceiling, not a replacement for the liquid solver.
class PerformanceLiquidBasin : public CellularLiquidWorld<LiquidId> {
public:
  PerformanceLiquidBasin(int width, int height) : width(width), height(height), cells(width * height) {}

  Vec2I unique_location(Vec2I const& position) const override {
    return {(position[0] % width + width) % width, position[1]};
  }

  std::expected<CellularLiquidCell<LiquidId>, StarException> cell(Vec2I const& position) const override {
    if (std::exchange(fail_next_read, false))
      return std::unexpected(StarException("Transient performance basin read failure"));
    auto point = unique_location(position);
    if (point[1] < 0 || point[1] >= height)
      return CellularLiquidCollisionCell{};
    return cells[point[1] * width + point[0]];
  }

  std::expected<void, StarException> set_flow(Vec2I const& position,
                                             CellularLiquidFlowCell<LiquidId> const& flow) override {
    auto point = unique_location(position);
    if (point[1] < 0 || point[1] >= height)
      return std::unexpected(StarException("Liquid benchmark wrote outside basin"));
    mix(point[0]);
    mix(point[1]);
    cells[point[1] * width + point[0]] = flow;
    return {};
  }

  std::expected<void, StarException> liquid_interaction(Vec2I const& a, LiquidId a_liquid, Vec2I const& b,
                                                       LiquidId b_liquid) override {
    mix(a[0]);
    mix(a[1]);
    mix(a_liquid);
    mix(b[0]);
    mix(b[1]);
    mix(b_liquid);
    return {};
  }

  std::expected<void, StarException> liquid_collision(Vec2I const& position, LiquidId liquid,
                                                     Vec2I const& collision) override {
    mix(position[0]);
    mix(position[1]);
    mix(liquid);
    mix(collision[0]);
    mix(collision[1]);
    return {};
  }

  void mix(std::uint32_t value) { trace = (trace ^ value) * 1099511628211ULL; }

  double mass() const {
    double result = 0;
    for (auto const& cell : cells)
      result += cell.level;
    return result;
  }

  int width;
  int height;
  std::vector<CellularLiquidFlowCell<LiquidId>> cells;
  mutable bool fail_next_read = false;
  std::uint64_t trace = 1469598103934665603ULL;
};

struct LiquidRun {
  LiquidRun(LiquidScenario scenario, LiquidCellEngineParameters parameters, LiquidId water, LiquidId lava)
      : basin(std::make_shared<PerformanceLiquidBasin>(scenario.width, scenario.height)),
        engine(0x43bd159, parameters, basin) {
    for (int y = 0; y < scenario.height; ++y) {
      for (int x = 0; x < scenario.width; ++x) {
        auto liquid = scenario.multiple_liquids && x >= scenario.width / 2 ? lava : water;
        float level = 0.75f + static_cast<float>((x * 13 + y * 7) % 9) / 32.0f;
        basin->cells[y * scenario.width + x] = {liquid, level, static_cast<float>(scenario.height - y)};
      }
    }
    engine.visit_region(RectI(0, 0, scenario.width, scenario.height));
  }

  std::expected<void, StarException> prepare(LiquidScenario scenario, LiquidId lava) {
    // First tick publishes visited cells into the real active-cell queue.
    auto activated = engine.update();
    if (!activated)
      return std::unexpected(std::move(activated.error()));
    engine.set_processing_limit(scenario.budget);
    if (scenario.monitored) {
      // Overlapping windows, separated windows and a window at the wrapped edge.
      engine.set_no_processing_limit_regions({RectI(4, 2, 12, 6), RectI(8, 3, 16, 7),
                                          RectI(scenario.width / 2, 8, scenario.width / 2 + 8, 12),
                                          RectI(scenario.width - 4, 0, scenario.width, 4)});
    }
    if (scenario.multiple_liquids)
      engine.set_liquid_tick_delta(lava, 2);
    if (scenario.steady_windows) {
      auto warmed = engine.update();
      if (!warmed)
        return std::unexpected(std::move(warmed.error()));
    }
    if (scenario.read_recovery) {
      basin->fail_next_read = true;
      auto failed = engine.update();
      if (failed)
        return std::unexpected(StarException("Liquid benchmark did not expose injected read failure"));
    }
    basin->trace = 1469598103934665603ULL;
    return {};
  }

  std::uint64_t digest() const {
    std::uint64_t hash = basin->trace;
    auto mix = [&](std::uint32_t value) { hash = (hash ^ value) * 1099511628211ULL; };
    for (int y = 0; y < basin->height; ++y) {
      for (int x = 0; x < basin->width; ++x) {
        auto const& cell = basin->cells[y * basin->width + x];
        mix(cell.liquid ? static_cast<std::uint32_t>(*cell.liquid) + 1 : 0);
        mix(std::bit_cast<std::uint32_t>(cell.level));
        mix(std::bit_cast<std::uint32_t>(cell.pressure));
        mix(engine.is_active({x, y}));
      }
    }
    return hash;
  }

  std::shared_ptr<PerformanceLiquidBasin> basin;
  LiquidCellEngine<LiquidId> engine;
};

}// namespace

std::expected<void, StarException> register_liquid_performance_benchmarks(PerformanceBenchmarkSession& session) {
  auto database = session.root.liquidsDatabase();
  if (!database)
    return std::unexpected(std::move(database.error()));
  auto water = (*database)->liquidId("water");
  if (!water)
    return std::unexpected(std::move(water.error()));
  auto lava = (*database)->liquidId("lava");
  if (!lava)
    return std::unexpected(std::move(lava.error()));
  auto parameters = (*database)->liquid_engine_parameters();
  // Closed-basin mass conservation includes arbitrarily small residual levels.
  parameters.minimum_liquid_level = 0;
  for (auto scenario : {LiquidScenario{"Liquid/LowBudget/16384", 256, 64, 8, false, false, false},
                        LiquidScenario{"Liquid/LowBudget/65536", 1024, 64, 8, false, false, false},
                        LiquidScenario{"Liquid/OverlapRegions/65536", 1024, 64, 8, true, false, false},
                        LiquidScenario{"Liquid/SteadyOverlapRegions/65536", 1024, 64, 8, true, false, false, true},
                        LiquidScenario{"Liquid/MultipleLiquids/32768", 512, 64, 8, true, true, false},
                        LiquidScenario{"Liquid/ReadRecovery/16384", 256, 64, 8, true, false, true},
                        LiquidScenario{"Liquid/FullPressure/4096", 128, 32, {}, false, false, false}}) {
    LiquidRun reference(scenario, parameters, *water, *lava);
    auto prepared = reference.prepare(scenario, *lava);
    if (!prepared)
      return std::unexpected(std::move(prepared.error()));
    auto initial_mass = reference.basin->mass();
    auto initial_active = reference.engine.active_cells();
    auto updated = reference.engine.update();
    if (!updated)
      return std::unexpected(std::move(updated.error()));
    auto expected_digest = reference.digest();
    auto expected_active = reference.engine.active_cells();
    if (std::abs(reference.basin->mass() - initial_mass) > initial_mass * 0.000001)
      return std::unexpected(StarException("Liquid benchmark reference did not conserve mass"));

    benchmark::RegisterBenchmark(
      scenario.name.data(),
      [&session, scenario, parameters, water = *water, lava = *lava, initial_mass, initial_active, expected_digest,
       expected_active](benchmark::State& state) {
        for (auto _ : state) {
          state.PauseTiming();
          {
            LiquidRun run(scenario, parameters, water, lava);
            auto prepared = run.prepare(scenario, lava);
            if (!prepared) {
              session.fail(state, prepared.error());
              break;
            }
            state.ResumeTiming();
            auto updated = run.engine.update();
            benchmark::DoNotOptimize(updated.has_value());
            state.PauseTiming();
            if (!updated) {
              session.fail(state, updated.error());
              break;
            }
            if (run.digest() != expected_digest || run.engine.active_cells() != expected_active
                || std::abs(run.basin->mass() - initial_mass) > initial_mass * 0.000001) {
              session.fail(state, "Liquid benchmark changed selected cells, callback order, output or mass");
              break;
            }
          }
          state.ResumeTiming();
        }
        // Exact split counters survive JSON's double representation and permit
        // comparison of production state, active selection and callback order A/B.
        state.counters["digest_hi"] = static_cast<std::uint32_t>(expected_digest >> 32);
        state.counters["digest_lo"] = static_cast<std::uint32_t>(expected_digest);
        state.counters["active_after"] = expected_active;
        state.counters["active_before"] = initial_active;
        state.counters["mass_before"] = initial_mass;
      });
  }
  return {};
}

}// namespace Star

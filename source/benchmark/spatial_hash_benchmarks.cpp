#include <benchmark/benchmark.h>

import std;
import star.format;
import star.rect;
import star.spatial_hash_2d;

namespace {

using Index = Star::SpatialHash2D<int, float, int>;
using Rect = Index::Rect;
constexpr int count = 256;
constexpr int probes = 64;

Rect bounds(int id, float offset) {
  float x = static_cast<float>((id % 32 - 16) * 32) + offset;
  float y = static_cast<float>((id / 32 - 4) * 32) + 3.5f;
  return {x, y, x + 3.0f, y + 4.0f};
}

bool populate(benchmark::State& state, Index& index) {
  for (int id = 0; id != count; ++id) {
    std::array rects{bounds(id, 2.5f)};
    auto inserted = index.set(id, rects, id);
    if (!inserted) {
      state.SkipWithError(inserted.error().what());
      return false;
    }
  }
  return true;
}

void movement(benchmark::State& state, float displacement) {
  Index index(std::integral_constant<int, 16>{});
  if (!populate(state, index)) return;
  std::array<std::array<Rect, 1>, count> original, moved;
  for (int id = 0; id != count; ++id) {
    original[id] = {bounds(id, 2.5f)};
    moved[id] = {bounds(id, 2.5f + displacement)};
  }
  bool forward = false;
  for (auto _ : state) {
    (void)_;
    forward = !forward;
    auto const& positions = forward ? moved : original;
    for (int id = 0; id != count; ++id) {
      auto changed = index.set(id, positions[id]);
      if (!changed) { state.SkipWithError(changed.error().what()); return; }
    }
    benchmark::ClobberMemory();
  }
  auto const& positions = forward ? moved : original;
  for (int id = 0; id != count; ++id) {
    auto found = index.query_values(positions[id]);
    if (!found || found->size() != 1 || found->front() != id) {
      state.SkipWithError("movement did not preserve query membership"); return;
    }
  }
  state.SetItemsProcessed(state.iterations() * count);
}

enum class Shape { Point, Boundary, Local, Sparse, Broad, Split };

void query(benchmark::State& state, Shape shape) {
  Index index(std::integral_constant<int, 16>{});
  if (!populate(state, index)) return;
  std::array<std::array<Rect, 2>, probes> regions;
  for (int i = 0; i != probes; ++i) {
    int id = (i * 73) % count;
    auto r = bounds(id, 2.5f);
    float x = r.x_min(), y = r.y_min();
    if (shape == Shape::Boundary) {
      // Store boxes ending exactly on a sector boundary; query their closed right edge.
      r = {x, y, x + 13.5f, y + 4.0f};
      auto updated = index.set(id, std::array{r});
      if (!updated) { state.SkipWithError(updated.error().what()); return; }
    }
    switch (shape) {
      case Shape::Point: regions[i][0] = {x + 1, y + 1, x + 1, y + 1}; break;
      case Shape::Boundary: regions[i][0] = {r.x_max(), y + 1, r.x_max(), y + 1}; break;
      case Shape::Local: regions[i][0] = {x - 2, y - 2, x + 4, y + 5}; break;
      case Shape::Sparse: regions[i][0] = {x + 2048, y, x + 3072, y + 512}; break;
      case Shape::Broad: regions[i][0] = {x - 512, y - 128, x + 512, y + 128}; break;
      case Shape::Split: regions[i][0] = r; break;
    }
    regions[i][1] = bounds((id + 17) % count, 2.5f);
  }
  auto width = shape == Shape::Split ? 2u : 1u;
  std::uint64_t expected_hits = 0, expected_sum = 0;
  for (auto const& region : regions) {
    for (auto const& [key, entry] : index.entries()) {
      for (auto const& area : std::span(region).first(width)) {
        if (entry.rects.front().intersects(area)) {
          ++expected_hits;
          expected_sum += key;
          break;
        }
      }
    }
  }
  std::uint64_t hits = 0, sum = 0;
  for (auto _ : state) {
    (void)_;
    hits = sum = 0;
    for (auto const& region : regions) {
      auto found = index.for_each(std::span(region).first(width), [&](int value) -> std::expected<void, Star::StarException> {
        ++hits;
        sum += value;
        return {};
      });
      if (!found) { state.SkipWithError(found.error().what()); return; }
    }
    benchmark::DoNotOptimize(hits);
    benchmark::DoNotOptimize(sum);
  }
  state.counters["hits_per_batch"] = static_cast<double>(hits);
  if (expected_hits != hits || expected_sum != sum)
    state.SkipWithError("query differs from inclusive brute-force membership");
  state.SetItemsProcessed(state.iterations() * probes);
}

} // namespace

void register_spatial_hash_benchmarks() {
  benchmark::RegisterBenchmark("SpatialHash/Move/SameSector", [](benchmark::State& s) { movement(s, 1); });
  benchmark::RegisterBenchmark("SpatialHash/Move/CrossSector", [](benchmark::State& s) { movement(s, 16); });
  for (auto [shape, name] : {std::pair{Shape::Point, "SpatialHash/Query/Point"},
                             std::pair{Shape::Boundary, "SpatialHash/Query/ExactEdge"},
                             std::pair{Shape::Local, "SpatialHash/Query/Local"},
                             std::pair{Shape::Sparse, "SpatialHash/Query/Sparse"},
                             std::pair{Shape::Broad, "SpatialHash/Query/Broad"},
                             std::pair{Shape::Split, "SpatialHash/Query/Split"}})
    benchmark::RegisterBenchmark(name, [shape](benchmark::State& s) { query(s, shape); });
}

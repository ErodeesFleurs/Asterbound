#include <benchmark/benchmark.h>

import std;
import star.spline;
import star.vector;

namespace {
using Star::CSplineF;
using Star::Vec2F;

// An open, curved cubic with a nonzero endpoint chord. Alternate the near
// control each frame so no result can legitimately be reused across frames.
void set_frame(CSplineF& curve, unsigned frame) {
  curve[0] = Vec2F(0, 0);
  curve[1] = Vec2F(1, frame == 0 ? 8 : 7);
  curve[2] = Vec2F(6, -1);
  curve[3] = Vec2F(10, 3);
}

#pragma float_control(precise, on, push)
std::array<double, 2> reference_point(double t, unsigned frame) {
  double const s = 1 - t;
  return {3*s*s*t + 18*s*t*t + 10*t*t*t,
      3*s*s*t*(frame == 0 ? 8 : 7) - 3*s*t*t + 3*t*t*t};
}

// Simpson integration of the analytic derivative, independent of the
// production chord sum, interpolation and inverse. Used only outside timing.
double reference_length(double t, unsigned frame) {
  auto speed = [frame](double u) {
    double const s = 1 - u, y = frame == 0 ? 8 : 7;
    return 3 * std::hypot(s*s + 10*s*u + 4*u*u, y*s*s - 2*(1+y)*s*u + 4*u*u);
  };
  constexpr int intervals = 1024;
  double sum = speed(0) + speed(t);
  for (int i = 1; i < intervals; ++i)
    sum += (i % 2 ? 4 : 2) * speed(t * i / intervals);
  return sum * t / (3 * intervals);
}
#pragma float_control(pop)

void point_batch(benchmark::State& state) {
  CSplineF curve;
  set_frame(curve, 0);
  std::array<Vec2F, 64> points;
  for (auto _ : state) {
    benchmark::ClobberMemory();
    for (unsigned i = 0; i < points.size(); ++i)
      points[i] = curve.point_at(static_cast<float>(i) / 64);
    benchmark::DoNotOptimize(points);
  }
  double error = 0;
  for (unsigned i = 0; i < points.size(); ++i) {
    auto ref = reference_point(static_cast<double>(i) / 64, 0);
    error = std::max(error, std::hypot(points[i][0] - ref[0], points[i][1] - ref[1]));
  }
  state.counters["max_point_error"] = error;
  state.SetItemsProcessed(64 * state.iterations());
}

void changing_frame(benchmark::State& state) {
  CSplineF curve;
  Star::SplineArcLengthTable<float, 100> table;
  std::array<float, 2> lengths;
  std::array<Vec2F, 32> points;
  for (auto _ : state) {
    for (unsigned frame = 0; frame < lengths.size(); ++frame) {
      set_frame(curve, frame);
      benchmark::ClobberMemory();
      curve.sample_arc_lengths(table);
      lengths[frame] = table.total_length();
      benchmark::DoNotOptimize(table);
      // Equal visual sample count, even though the old chord length would
      // choose fewer segments. This isolates geometry, not render throughput.
      for (unsigned i = 0; i < points.size(); ++i)
        points[i] = curve.point_at(static_cast<float>(i) / 32);
      benchmark::DoNotOptimize(points);
    }
    benchmark::DoNotOptimize(lengths);
  }
  state.counters["frame0_length"] = lengths[0];
  state.counters["frame0_reference"] = reference_length(1, 0);
  state.counters["max_length_error"] = std::max(std::abs(lengths[0] - reference_length(1, 0)),
      std::abs(lengths[1] - reference_length(1, 1)));
  state.SetItemsProcessed(2 * state.iterations());
}

void inverse_batch(benchmark::State& state) {
  CSplineF curve;
  Star::SplineArcLengthTable<float, 100> table;
  constexpr unsigned count = 16;
  std::array<std::array<float, count>, 2> parameters;
  for (auto _ : state) {
    for (unsigned frame = 0; frame < parameters.size(); ++frame) {
      set_frame(curve, frame);
      benchmark::ClobberMemory();
      curve.sample_arc_lengths(table);
      std::size_t cursor = 0;
      for (unsigned i = 0; i < count; ++i) {
        float u = static_cast<float>(i + 1) / (count + 1);
        parameters[frame][i] = table.parameter_at_length_fraction(u, cursor);
      }
    }
    benchmark::DoNotOptimize(parameters);
  }
  double error = 0;
  for (unsigned frame = 0; frame < parameters.size(); ++frame) {
    double total = reference_length(1, frame);
    for (unsigned i = 0; i < count; ++i) {
      float u = static_cast<float>(i + 1) / (count + 1);
      error = std::max(error, std::abs(reference_length(parameters[frame][i], frame) - u * total));
    }
  }
  state.counters["frame0_t_at_8_of_17"] = parameters[0][7];
  state.counters["max_inverse_length_error"] = error;
  state.SetItemsProcessed(2 * count * state.iterations());
}
} // namespace

void register_spline_benchmarks() {
  benchmark::RegisterBenchmark("Spline/PointBatch64", point_batch);
  benchmark::RegisterBenchmark("Spline/ChangingFrameSamples32", changing_frame);
  benchmark::RegisterBenchmark("Spline/ChangingFrameInverseBatch16", inverse_batch);
}

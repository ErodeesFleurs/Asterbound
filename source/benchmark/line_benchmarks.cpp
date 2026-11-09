#include <benchmark/benchmark.h>
#include <cstdio>

import std;
import star.format;
import star.line;
import star.native_runtime;
import star.vector;

namespace {
template <typename T>
concept line_benchmark_real = std::same_as<T, float> || std::same_as<T, double>;

constexpr std::size_t batch_size = 64;
enum class intersection_family { nonparallel_finite, nonparallel_infinite, parallel_mixed, collinear_mixed };

struct intersection_fixture {
  std::array<double, 8> endpoints;
  std::array<double, 2> point;
  double t;
  bool intersects;
  bool coincides;
  bool glances;
};

// Hand-solved geometries: all coordinates, parameters and transformed results
// are exact binary fractions. These are not a second intersection algorithm.
constexpr std::array<intersection_fixture, 8> nonparallel_fixtures{{
  {{0, 0, 8, 0, 2, -4, 2, 4}, {2, 0}, 0.25, true, false, false},
  {{0, 0, 8, 0, 12, -4, 12, 4}, {12, 0}, 1.5, false, false, false},
  {{0, 0, 8, 0, 8, 0, 8, 4}, {8, 0}, 1, true, false, true},
  {{0, 0, 8, 0, 0, -4, 0, 4}, {0, 0}, 0, true, false, true},
  {{0, 0, 8, 8, 0, 8, 8, 0}, {4, 4}, 0.5, true, false, false},
  {{8, 0, 0, 8, 0, 0, 8, 8}, {4, 4}, 0.5, true, false, false},
  {{0, 0, 8, 0, 4, 4, 4, 8}, {4, 0}, 0.5, false, false, false},
  {{0, 0, 0, 8, -4, 4, 4, 4}, {0, 4}, 0.5, true, false, false},
}};
constexpr std::array<intersection_fixture, 4> parallel_fixtures{{
  {{0, 0, 8, 0, 0, 4, 8, 4}, {0, 0}, 0, false, false, false},
  {{0, 0, 0, 8, 4, 0, 4, 8}, {0, 0}, 0, false, false, false},
  {{0, 0, 8, 8, 0, 4, 8, 12}, {0, 0}, 0, false, false, false},
  {{0, 8, 8, 0, 0, 12, 8, 4}, {0, 0}, 0, false, false, false},
}};
constexpr std::array<intersection_fixture, 8> collinear_fixtures{{
  {{0, 0, 8, 0, 4, 0, 12, 0}, {4, 0}, 0.5, true, true, true},
  {{0, 0, 8, 0, 12, 0, 16, 0}, {0, 0}, 1.5, false, true, false},
  {{0, 0, 8, 0, -4, 0, 4, 0}, {0, 0}, 0, true, true, true},
  {{0, 0, 8, 0, -8, 0, -4, 0}, {0, 0}, -0.5, false, true, false},
  {{0, 0, 0, 8, 0, 4, 0, 12}, {0, 4}, 0.5, true, true, true},
  {{0, 0, 0, 8, 0, 12, 0, 16}, {0, 0}, 1.5, false, true, false},
  {{0, 0, 8, 8, 4, 4, 12, 12}, {4, 4}, 0.5, true, true, true},
  {{0, 0, 8, 8, 12, 12, 16, 16}, {0, 0}, 1.5, false, true, false},
}};

struct distance_fixture {
  std::array<double, 6> coordinates;
  double finite;
  double infinite;
};
constexpr std::array<distance_fixture, 8> distance_fixtures{{
  {{0, 0, 8, 0, 4, 3}, 3, 3},
  {{0, 0, 8, 0, -4, 3}, 5, 3},
  {{0, 0, 8, 0, 12, -3}, 5, 3},
  {{0, 0, 0, 8, 3, 4}, 3, 3},
  {{0, 0, 0, 8, -3, -4}, 5, 3},
  {{0, 0, 0, 8, 3, 12}, 5, 3},
  {{0, 0, 6, 8, -4, 3}, 5, 5},
  {{0, 0, 6, 8, -1, 7}, 5, 5},
}};

struct fingerprint {
  std::uint64_t value = 14695981039346656037ull;

  void add(bool bit) { value = (value ^ static_cast<unsigned char>(bit)) * 1099511628211ull; }

  template <line_benchmark_real T> void add(T number) {
    using bits_type = std::conditional_t<sizeof(T) == 4, std::uint32_t, std::uint64_t>;
    // Signed zero is not distinguished by the mathematical result contract.
    auto bits = std::bit_cast<bits_type>(number == T(0) ? T(0) : number);
    for (std::size_t i = 0; i < sizeof(T); ++i) {
      value = (value ^ static_cast<unsigned char>(bits & 255)) * 1099511628211ull;
      bits >>= 8;
    }
  }
};

template <line_benchmark_real T> struct expected_intersection {
  std::array<T, 2> point;
  T t;
  bool intersects;
  bool coincides;
  bool glances;
};

template <line_benchmark_real T> void add_result(fingerprint& hash, expected_intersection<T> const& result) {
  hash.add(result.intersects);
  hash.add(result.point[0]);
  hash.add(result.point[1]);
  hash.add(result.t);
  hash.add(result.coincides);
  hash.add(result.glances);
}

bool require(benchmark::State& state, bool condition) {
  if (!condition)
    state.SkipWithError("complete result differs from independently solved line fixtures");
  return condition;
}

void label(benchmark::State& state, fingerprint input, fingerprint output, std::string_view detail) {
  state.SetLabel(std::format("fixture-fnv1a={:016x}; complete-result-fnv1a={:016x}; batch={}; {}", input.value,
                             output.value, batch_size, detail));
}

template <line_benchmark_real T> T scale(std::size_t index) { return T(1u << (index % 4)) * T(0.5); }

template <line_benchmark_real T> T translation(std::size_t index, std::size_t axis) {
  return T(static_cast<int>((index / 4 + axis * 3) % 11) - 5) * T(0.25);
}

template <intersection_family Family> constexpr auto const& fixtures() {
  if constexpr (Family == intersection_family::parallel_mixed)
    return parallel_fixtures;
  else if constexpr (Family == intersection_family::collinear_mixed)
    return collinear_fixtures;
  else
    return nonparallel_fixtures;
}

template <line_benchmark_real T, intersection_family Family, bool Full>
void measure_intersection(benchmark::State& state) {
  using line_type = Star::Line<T, 2>;
  using vector_type = Star::Vector<T, 2>;
  using result_type = std::conditional_t<Full, typename line_type::IntersectResult, bool>;
  std::array<line_type, batch_size> first;
  std::array<line_type, batch_size> second;
  std::array<bool, batch_size> infinite;
  std::array<expected_intersection<T>, batch_size> expected;
  std::array<result_type, batch_size> output;
  fingerprint input_hash;
  auto const& table = fixtures<Family>();
  for (std::size_t i = 0; i < batch_size; ++i) {
    auto const& fixture = table[i % table.size()];
    std::array<T, 8> coordinates;
    for (std::size_t j = 0; j < coordinates.size(); ++j) {
      coordinates[j] = T(fixture.endpoints[j]) * scale<T>(i) + translation<T>(i, j % 2);
      input_hash.add(coordinates[j]);
    }
    first[i] = line_type(vector_type(coordinates[0], coordinates[1]), vector_type(coordinates[2], coordinates[3]));
    second[i] = line_type(vector_type(coordinates[4], coordinates[5]), vector_type(coordinates[6], coordinates[7]));
    if constexpr (Family == intersection_family::nonparallel_finite)
      infinite[i] = false;
    else if constexpr (Family == intersection_family::nonparallel_infinite)
      infinite[i] = true;
    else
      infinite[i] = (i / table.size()) % 2 != 0;
    input_hash.add(infinite[i]);
    auto& result = expected[i];
    result = {{T(fixture.point[0]) * scale<T>(i) + translation<T>(i, 0),
               T(fixture.point[1]) * scale<T>(i) + translation<T>(i, 1)},
              T(fixture.t),
              fixture.intersects,
              fixture.coincides,
              fixture.glances};
    if constexpr (Family == intersection_family::nonparallel_infinite) {
      result.intersects = true;
      result.glances = false;
    } else if constexpr (Family == intersection_family::parallel_mixed) {
      result.point = {T(0), T(0)};
      result.t = std::numeric_limits<T>::max();
    } else if constexpr (Family == intersection_family::collinear_mixed) {
      if (infinite[i]) {
        result.intersects = true;
        result.glances = true;
        result.point.fill(-std::numeric_limits<T>::max());
      } else if (!result.intersects) {
        // The default-constructed point is zero for finite collinear misses.
        result.point = {T(0), T(0)};
      }
    }
  }
  auto evaluate = [&] {
    for (std::size_t i = 0; i < batch_size; ++i) {
      if constexpr (Full)
        output[i] = first[i].intersection(second[i], infinite[i]);
      else
        output[i] = first[i].intersects(second[i], infinite[i]);
    }
  };
  auto validate = [&] {
    for (std::size_t i = 0; i < batch_size; ++i) {
      auto const& want = expected[i];
      if constexpr (Full) {
        auto const& actual = output[i];
        if (actual.intersects != want.intersects || actual.point[0] != want.point[0] || actual.point[1] != want.point[1]
            || actual.t != want.t || actual.coincides != want.coincides || actual.glances != want.glances)
          return require(state, false);
      } else if (output[i] != want.intersects) {
        return require(state, false);
      }
    }
    return true;
  };
  evaluate();
  if (!validate())
    return;
  fingerprint result_hash;
  for (std::size_t i = 0; i < batch_size; ++i) {
    if constexpr (Full) {
      auto const& result = output[i];
      add_result(result_hash,
                 expected_intersection<T>{{result.point[0], result.point[1]},
                                          result.t,
                                          result.intersects,
                                          result.coincides,
                                          result.glances});
    } else {
      result_hash.add(output[i]);
    }
  }
  label(state, input_hash, result_hash,
        Full ? "all result fields checked; exact oracle" : "boolean result checked; exact oracle");
  for (auto _ : state) {
    (void)_;
    benchmark::DoNotOptimize(first.data());
    benchmark::DoNotOptimize(second.data());
    benchmark::DoNotOptimize(infinite.data());
    benchmark::ClobberMemory();
    evaluate();
    benchmark::DoNotOptimize(output.data());
    benchmark::ClobberMemory();
  }
  if (!validate())
    return;
  state.SetItemsProcessed(state.iterations() * batch_size);
}

template <line_benchmark_real T, bool Infinite> void measure_distance(benchmark::State& state) {
  using vector_type = Star::Vector<T, 2>;
  std::array<Star::Line<T, 2>, batch_size> lines;
  std::array<vector_type, batch_size> points;
  std::array<T, batch_size> expected;
  std::array<T, batch_size> output;
  fingerprint input_hash;
  for (std::size_t i = 0; i < batch_size; ++i) {
    auto const& fixture = distance_fixtures[i % distance_fixtures.size()];
    std::array<T, 6> coordinates;
    for (std::size_t j = 0; j < coordinates.size(); ++j) {
      coordinates[j] = T(fixture.coordinates[j]) * scale<T>(i) + translation<T>(i, j % 2);
      input_hash.add(coordinates[j]);
    }
    input_hash.add(Infinite);
    lines[i] =
      Star::Line<T, 2>(vector_type(coordinates[0], coordinates[1]), vector_type(coordinates[2], coordinates[3]));
    points[i] = vector_type(coordinates[4], coordinates[5]);
    expected[i] = T(Infinite ? fixture.infinite : fixture.finite) * scale<T>(i);
  }
  auto evaluate = [&] {
    for (std::size_t i = 0; i < batch_size; ++i)
      output[i] = lines[i].distance_to(points[i], Infinite);
  };
  auto validate = [&] { return require(state, output == expected); };
  evaluate();
  if (!validate())
    return;
  fingerprint result_hash;
  for (auto result : output)
    result_hash.add(result);
  label(state, input_hash, result_hash, "nonzero lines; exact 3-4-5 distance oracle");
  for (auto _ : state) {
    (void)_;
    benchmark::DoNotOptimize(lines.data());
    benchmark::DoNotOptimize(points.data());
    benchmark::ClobberMemory();
    evaluate();
    benchmark::DoNotOptimize(output.data());
    benchmark::ClobberMemory();
  }
  if (!validate())
    return;
  state.SetItemsProcessed(state.iterations() * batch_size);
}

template <line_benchmark_real T, intersection_family Family>
void register_intersection(std::string_view type, std::string_view geometry) {
  benchmark::RegisterBenchmark(std::format("Line/{}/Intersection/{}", type, geometry).c_str(),
                               measure_intersection<T, Family, true>);
  benchmark::RegisterBenchmark(std::format("Line/{}/Intersects/{}", type, geometry).c_str(),
                               measure_intersection<T, Family, false>);
}

template <line_benchmark_real T> void register_family(std::string_view type) {
  register_intersection<T, intersection_family::nonparallel_finite>(type, "NonparallelFinite");
  register_intersection<T, intersection_family::nonparallel_infinite>(type, "NonparallelInfinite");
  register_intersection<T, intersection_family::parallel_mixed>(type, "ParallelFiniteInfinite");
  register_intersection<T, intersection_family::collinear_mixed>(type, "PositiveCollinearFiniteInfinite");
  benchmark::RegisterBenchmark(std::format("Line/{}/Distance/Finite", type).c_str(), measure_distance<T, false>);
  benchmark::RegisterBenchmark(std::format("Line/{}/Distance/Infinite", type).c_str(), measure_distance<T, true>);
}

class checked_reporter final : public benchmark::ConsoleReporter {
public:
  bool failed = false;
  void ReportRuns(std::vector<Run> const& reports) override {
    for (auto const& report : reports)
      failed |= report.skipped == benchmark::internal::SkippedWithError;
    ConsoleReporter::ReportRuns(reports);
  }
};
}// namespace

int main(int argc, char** argv) {
  auto runtime = Star::NativeRuntime::create();
  if (!runtime) {
    (void)Star::print(stderr, "{}\n", runtime.error().what());
    return 1;
  }
  register_family<float>("Float");
  register_family<double>("Double");
  benchmark::Initialize(&argc, argv);
  if (benchmark::ReportUnrecognizedArguments(argc, argv)) {
    benchmark::Shutdown();
    return 1;
  }
  checked_reporter reporter;
  auto matched = benchmark::RunSpecifiedBenchmarks(&reporter);
  benchmark::Shutdown();
  return reporter.failed || matched == 0 ? 1 : 0;
}

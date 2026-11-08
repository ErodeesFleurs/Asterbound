#include <benchmark/benchmark.h>

import std;
import star.native_runtime;
import star.vector;

namespace {

constexpr std::size_t batch_size = 64;
enum class operation {
  add,
  compound_add,
  dot,
  magnitude,
  normalized,
  projection,
  transform,
  absolute,
  rotate,
  fly_repeated,
  fly_cached
};

template <std::floating_point T, std::size_t N, operation Kind>
using result_type = std::conditional_t<Kind == operation::dot || Kind == operation::magnitude, T, Star::Vector<T, N>>;

template <std::floating_point T, std::size_t N, operation Kind>
result_type<T, N, Kind> evaluate(Star::Vector<T, N> const& a, Star::Vector<T, N> const& b) {
  if constexpr (Kind == operation::add) {
    return a + b;
  } else if constexpr (Kind == operation::compound_add) {
    auto result = a;
    result += b;
    return result;
  } else if constexpr (Kind == operation::dot) {
    return a * b;
  } else if constexpr (Kind == operation::magnitude) {
    return a.magnitude();
  } else if constexpr (Kind == operation::normalized) {
    return a.normalized();
  } else if constexpr (Kind == operation::projection) {
    return a.project_onto(b);
  } else if constexpr (Kind == operation::transform) {
    return a.transformed([](T const& value) { return value * T(1.25) + T(0.5); });
  } else if constexpr (Kind == operation::absolute) {
    return a.abs();
  } else if constexpr (Kind == operation::rotate) {
    return a.rotate(T(0.375));
  } else if constexpr (Kind == operation::fly_repeated) {
    auto angle_factor = b.normalized() * a.normalized();
    auto speed_along_angle = angle_factor * b.magnitude();
    return a.normalized() * std::fmin(T(32), speed_along_angle + T(3.25) / T(1.75) * T(0.016));
  } else {
    auto direction = a.normalized();
    auto angle_factor = b.normalized() * direction;
    auto speed_along_angle = angle_factor * b.magnitude();
    return direction * std::fmin(T(32), speed_along_angle + T(3.25) / T(1.75) * T(0.016));
  }
}

template <std::floating_point T, std::size_t N, operation Kind>
std::array<T, N> expected_value(Star::Vector<T, N> const& a, Star::Vector<T, N> const& b) {
  T dot = 0;
  T a_squared = 0;
  T b_squared = 0;
  for (std::size_t j = 0; j < N; ++j) {
    dot += a[j] * b[j];
    a_squared += a[j] * a[j];
    b_squared += b[j] * b[j];
  }
  auto length = std::sqrt(a_squared);
  std::array<T, N> result{};
  for (std::size_t j = 0; j < N; ++j) {
    if constexpr (Kind == operation::add || Kind == operation::compound_add)
      result[j] = a[j] + b[j];
    else if constexpr (Kind == operation::dot)
      result[j] = dot;
    else if constexpr (Kind == operation::magnitude)
      result[j] = length;
    else if constexpr (Kind == operation::normalized)
      result[j] = a[j] / length;
    else if constexpr (Kind == operation::projection)
      result[j] = (dot * b[j]) / b_squared;
    else if constexpr (Kind == operation::transform)
      result[j] = a[j] * T(1.25) + T(0.5);
    else if constexpr (Kind == operation::absolute)
      result[j] = std::fabs(a[j]);
    else if constexpr (Kind == operation::rotate)
      result[j] = j == 0 ? a[0] * std::cos(T(0.375)) - a[1] * std::sin(T(0.375))
                         : a[0] * std::sin(T(0.375)) + a[1] * std::cos(T(0.375));
    else
      result[j] = a[j] / length * std::fmin(T(32), dot / length + T(3.25) / T(1.75) * T(0.016));
  }
  return result;
}

template <std::floating_point T> bool near(T actual, T expected) {
  return std::fabs(actual - expected)
    <= T(32) * std::numeric_limits<T>::epsilon() * std::max(T(1), std::fabs(expected));
}

template <std::floating_point T, std::size_t N, operation Kind> void measure(benchmark::State& state) {
  using vector_type = Star::Vector<T, N>;
  std::array<vector_type, batch_size> a;
  std::array<vector_type, batch_size> b;
  std::array<std::array<T, N>, batch_size> expected;
  for (std::size_t i = 0; i < batch_size; ++i) {
    for (std::size_t j = 0; j < N; ++j) {
      a[i][j] = T((i % 13 + 1) * (j + 1)) * (i % 3 ? T(0.125) : T(-0.125));
      b[i][j] = T((i % 7 + 2) * (N - j)) * T(0.25);
    }
    expected[i] = expected_value<T, N, Kind>(a[i], b[i]);
  }
  std::array<result_type<T, N, Kind>, batch_size> output;
  auto validate = [&] {
    for (std::size_t i = 0; i < batch_size; ++i) {
      auto result = evaluate<T, N, Kind>(a[i], b[i]);
      if constexpr (Kind == operation::dot || Kind == operation::magnitude) {
        if (!near(result, expected[i][0]))
          return false;
      } else {
        for (std::size_t j = 0; j < N; ++j)
          if (!near(result[j], expected[i][j]))
            return false;
      }
    }
    return true;
  };
  if (!validate()) {
    state.SkipWithError("Vector result differs from independently evaluated scalar fixture");
    return;
  }
  for (auto _ : state) {
    (void)_;
    benchmark::ClobberMemory();
    for (std::size_t i = 0; i < batch_size; ++i)
      output[i] = evaluate<T, N, Kind>(a[i], b[i]);
    benchmark::DoNotOptimize(output.data());
    benchmark::ClobberMemory();
  }
  if (!validate())
    state.SkipWithError("Vector result differs after timed execution");
  state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(batch_size));
  state.SetLabel("64 public vector operations and result writes; fixtures/checks untimed");
}

template <std::floating_point T, std::size_t N, operation Kind> void register_case(char const* type, char const* name) {
  auto full_name = std::format("Vector/{}/{}/{}D", type, name, N);
  benchmark::RegisterBenchmark(full_name.c_str(), &measure<T, N, Kind>);
}

template <std::floating_point T, std::size_t N> void register_family(char const* type) {
  register_case<T, N, operation::add>(type, "AddBatch64");
  register_case<T, N, operation::compound_add>(type, "CompoundAddBatch64");
  register_case<T, N, operation::dot>(type, "DotBatch64");
  register_case<T, N, operation::magnitude>(type, "MagnitudeBatch64");
  register_case<T, N, operation::normalized>(type, "NormalizedBatch64");
  register_case<T, N, operation::projection>(type, "ProjectionBatch64");
  register_case<T, N, operation::transform>(type, "TransformBatch64");
  register_case<T, N, operation::absolute>(type, "AbsBatch64");
  if constexpr (N == 2) {
    register_case<T, N, operation::rotate>(type, "RotateBatch64");
    register_case<T, N, operation::fly_repeated>(type, "FlyArithmeticRepeatedBatch64");
    register_case<T, N, operation::fly_cached>(type, "FlyArithmeticCachedBatch64");
  }
}

class checked_reporter : public benchmark::ConsoleReporter {
public:
  void ReportRuns(std::vector<Run> const& reports) override {
    for (auto const& run : reports)
      failed |= run.skipped == benchmark::internal::SkippedWithError;
    benchmark::ConsoleReporter::ReportRuns(reports);
  }
  bool failed = false;
};

}// namespace

int main(int argc, char** argv) {
  auto runtime = Star::NativeRuntime::create();
  if (!runtime) {
    std::cerr << runtime.error().what() << '\n';
    return 1;
  }
  register_family<float, 2>("Float");
  register_family<float, 3>("Float");
  register_family<float, 4>("Float");
  register_family<double, 2>("Double");
  register_family<double, 3>("Double");
  register_family<double, 4>("Double");
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

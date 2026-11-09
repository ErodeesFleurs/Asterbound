#include <cstdio>
#include <benchmark/benchmark.h>

import std;
import star.format;
import star.image;
import star.image_processing;
import star.native_runtime;
import star.string;
import star.vector;

namespace {

constexpr std::size_t batch_size = 64;
enum class operation {
  add,
  compound_add,
  subtract,
  piecewise_multiply,
  scalar_multiply,
  componentwise_chain,
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
  } else if constexpr (Kind == operation::subtract) {
    return a - b;
  } else if constexpr (Kind == operation::piecewise_multiply) {
    return a.piecewise_multiply(b);
  } else if constexpr (Kind == operation::scalar_multiply) {
    return a * T(1.25);
  } else if constexpr (Kind == operation::componentwise_chain) {
    return (a + b).piecewise_multiply(b) - a * T(0.5);
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
    else if constexpr (Kind == operation::subtract)
      result[j] = a[j] - b[j];
    else if constexpr (Kind == operation::piecewise_multiply)
      result[j] = a[j] * b[j];
    else if constexpr (Kind == operation::scalar_multiply)
      result[j] = a[j] * T(1.25);
    else if constexpr (Kind == operation::componentwise_chain)
      result[j] = (a[j] + b[j]) * b[j] - a[j] * T(0.5);
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
  for (std::size_t i = 0; i < batch_size; ++i)
    output[i] = evaluate<T, N, Kind>(a[i], b[i]);
  auto validate = [&] {
    for (std::size_t i = 0; i < batch_size; ++i) {
      auto const& result = output[i];
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
  // Escape input storage so each iteration's memory barrier retains its loads.
  benchmark::DoNotOptimize(a.data());
  benchmark::DoNotOptimize(b.data());
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
  register_case<T, N, operation::subtract>(type, "SubtractBatch64");
  register_case<T, N, operation::piecewise_multiply>(type, "PiecewiseMultiplyBatch64");
  register_case<T, N, operation::scalar_multiply>(type, "ScalarMultiplyBatch64");
  register_case<T, N, operation::componentwise_chain>(type, "ComponentwiseChainBatch64");
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

void measure_image_blend(benchmark::State& state, Star::BlendImageOperation::BlendMode mode) {
  auto side = static_cast<unsigned>(state.range(0));
  auto source_count = static_cast<std::size_t>(state.range(1));
  Star::Image original(side, side);
  for (std::size_t i = 0; i < original.data().size(); ++i)
    original.data()[i] = static_cast<std::uint8_t>((i * 29 + 17) % 256);
  std::array<Star::Image, 3> sources;
  std::array<Star::String, 3> names{"first", "second", "third"};
  Star::BlendImageOperation blend{mode, {}, {}};
  for (std::size_t i = 0; i < source_count; ++i) {
    sources[i] = Star::Image(side, side);
    for (std::size_t j = 0; j < sources[i].data().size(); ++j)
      sources[i].data()[j] = static_cast<std::uint8_t>((j * 37 + (i + 1) * 53) % 256);
    blend.blendImages.push_back(names[i]);
  }
  Star::ImageOperation operation = std::move(blend);
  Star::ImageReferenceCallback references =
    [&](Star::String const& name)
      -> std::expected<std::optional<std::reference_wrapper<Star::Image const>>, Star::StarException> {
    for (std::size_t i = 0; i < source_count; ++i)
      if (name == names[i])
        return std::optional{std::cref(sources[i])};
    return std::nullopt;
  };
  std::vector<std::uint8_t> expected(original.data().size());
  for (std::size_t i = 0; i < expected.size(); ++i) {
    float value = original.data()[i] / 255.0f;
    for (std::size_t j = 0; j < source_count; ++j) {
      float other = sources[j].data()[i] / 255.0f;
      value = mode == Star::BlendImageOperation::Multiply ? value * other : 1.0f - (1.0f - value) * (1.0f - other);
    }
    expected[i] = static_cast<std::uint8_t>(std::clamp(value, 0.0f, 1.0f) * 255.0f);
  }
  Star::Image output = original;
  auto validate = [&] {
    return std::ranges::equal(output.data(), expected);
  };
  auto initial = Star::processImageOperation(operation, output, references);
  if (!initial) {
    state.SkipWithError(initial.error().what());
    return;
  }
  if (!validate()) {
    state.SkipWithError("Image blend differs from independent scalar byte fixture");
    return;
  }
  for (auto _ : state) {
    (void)_;
    state.PauseTiming();
    std::ranges::copy(original.data(), output.data().begin());
    state.ResumeTiming();
    auto result = Star::processImageOperation(operation, output, references);
    benchmark::DoNotOptimize(output.data().data());
    if (!result) {
      state.SkipWithError(result.error().what());
      return;
    }
  }
  if (!validate())
    state.SkipWithError("Image blend differs after timed execution");
  state.SetItemsProcessed(state.iterations() * side * side);
  state.SetLabel("one public image blend; input reset and scalar byte checks untimed");
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
    if (!Star::print(stderr, "{}\n", runtime.error().what()))
      return 1;
    return 1;
  }
  register_family<float, 2>("Float");
  register_family<float, 3>("Float");
  register_family<float, 4>("Float");
  register_family<double, 2>("Double");
  register_family<double, 3>("Double");
  register_family<double, 4>("Double");
  for (auto mode : {Star::BlendImageOperation::Multiply, Star::BlendImageOperation::Screen}) {
    auto name = mode == Star::BlendImageOperation::Multiply ? "Vector/ImageBlend/Multiply" : "Vector/ImageBlend/Screen";
    auto registration = benchmark::RegisterBenchmark(name, [mode](benchmark::State& state) {
      measure_image_blend(state, mode);
    });
    registration->Args({64, 1})->Args({64, 3})->Args({256, 1})->Args({256, 3});
  }
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

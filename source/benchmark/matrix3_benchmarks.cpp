#include <cstdio>
#include <benchmark/benchmark.h>

import std;
import star.format;
import star.math_common;
import star.matrix3;
import star.native_runtime;
import star.vector;

namespace {

constexpr std::size_t matrix_batch_size = 64;
using ReferenceMatrix = std::array<long double, 9>;

template <typename T>
concept bench_scalar = std::same_as<T, float> || std::same_as<T, double>;

template <typename Kernel, typename T>
concept matrix_kernel = requires(Kernel const& kernel, Star::Matrix3<T> const& a, Star::Matrix3<T> const& b) {
  { kernel(a, b) } -> std::same_as<Star::Matrix3<T>>;
};

template <typename Kernel, typename T>
concept inverse_kernel = std::invocable<Kernel const&, Star::Matrix3<T> const&>
    && requires(std::invoke_result_t<Kernel const&, Star::Matrix3<T> const&> const& result) {
      { static_cast<bool>(result) } -> std::same_as<bool>;
      { *result } -> std::same_as<Star::Matrix3<T> const&>;
    };

enum class MatrixOperation {
  add, subtract, scalar_multiply, scalar_divide, multiply, self_alias,
  translate, scale, rotate, affine_chain
};

enum class VectorOperation { point, direction, angle };

template <bench_scalar T>
ReferenceMatrix fixture_values(std::size_t index, bool second = false) {
  auto v = static_cast<long double>(index % 17) / 64;
  ReferenceMatrix values = second
      ? ReferenceMatrix{1.5L, -0.375L, 0.75L, 0.25L, 2 + v, -0.5L, -0.125L, 0.25L, 1.75L}
      : ReferenceMatrix{2 + v, 0.25L, -0.5L, -0.375L, 1.75L, 0.625L, 0.125L, -0.1875L, 1.5L + v};
  if (index == 0)
    values = second ? ReferenceMatrix{1, 2, 0, 3, 1, 2, 2, 0, 1}
                    : ReferenceMatrix{2, 1, 3, 0, 3, 1, 1, 0, 4};
  // References start from the exact public input values, not matrix operations.
  for (auto& value : values)
    value = static_cast<T>(value);
  return values;
}

template <bench_scalar T>
Star::Matrix3<T> make_matrix(ReferenceMatrix const& values) {
  return Star::Matrix3<T>(T(values[0]), T(values[1]), T(values[2]),
      T(values[3]), T(values[4]), T(values[5]), T(values[6]), T(values[7]), T(values[8]));
}

ReferenceMatrix reference_product(ReferenceMatrix const& a, ReferenceMatrix const& b) {
  ReferenceMatrix result{};
  for (std::size_t row = 0; row < 3; ++row)
    for (std::size_t column = 0; column < 3; ++column)
      for (std::size_t inner = 0; inner < 3; ++inner)
        result[row * 3 + column] += a[row * 3 + inner] * b[inner * 3 + column];
  return result;
}

ReferenceMatrix reference_matrix(MatrixOperation operation, ReferenceMatrix const& a, ReferenceMatrix const& b) {
  auto result = a;
  if (operation == MatrixOperation::add || operation == MatrixOperation::subtract
      || operation == MatrixOperation::scalar_multiply || operation == MatrixOperation::scalar_divide) {
    for (std::size_t i = 0; i < 9; ++i) {
      if (operation == MatrixOperation::add)
        result[i] += b[i];
      else if (operation == MatrixOperation::subtract)
        result[i] -= b[i];
      else if (operation == MatrixOperation::scalar_multiply)
        result[i] *= 1.25L;
      else
        result[i] /= 1.25L;
    }
    return result;
  }
  if (operation == MatrixOperation::multiply || operation == MatrixOperation::self_alias)
    return reference_product(a, operation == MatrixOperation::self_alias ? a : b);

  ReferenceMatrix translation{1, 0, 1.5L, 0, 1, -2.25L, 0, 0, 1};
  ReferenceMatrix scaling{1.125L, 0, -0.15625L, 0, 0.875L, -0.09375L, 0, 0, 1};
  auto cosine = std::cos(0.375L);
  auto sine = std::sin(0.375L);
  ReferenceMatrix rotation{cosine, -sine, 1.25L - 1.25L * cosine - 0.75L * sine,
      sine, cosine, -0.75L - 1.25L * sine + 0.75L * cosine, 0, 0, 1};
  if (operation == MatrixOperation::translate)
    return reference_product(translation, a);
  if (operation == MatrixOperation::scale)
    return reference_product(scaling, a);
  if (operation == MatrixOperation::rotate)
    return reference_product(rotation, a);
  return reference_product(rotation, reference_product(scaling, reference_product(translation, a)));
}

ReferenceMatrix reference_inverse(ReferenceMatrix const& input) {
  // Closed-form long-double cofactors are independent of production's pivoted
  // elimination algorithm. Fixtures are fixed, nonsingular and well-conditioned.
  auto [a, b, c, d, e, f, g, h, i] = input;
  auto determinant = a * (e * i - f * h) - b * (d * i - f * g) + c * (d * h - e * g);
  ReferenceMatrix result{
      e * i - f * h, c * h - b * i, b * f - c * e,
      f * g - d * i, a * i - c * g, c * d - a * f,
      d * h - e * g, b * g - a * h, a * e - b * d};
  for (auto& value : result)
    value /= determinant;
  return result;
}

template <bench_scalar T>
bool near(T actual, long double expected) {
  return Star::isFinite(actual) && std::fabs(static_cast<long double>(actual) - expected)
      <= 64 * std::numeric_limits<T>::epsilon() * std::max(1.0L, std::fabs(expected));
}

template <bench_scalar T>
bool validate_matrix(Star::Matrix3<T> const& actual, ReferenceMatrix const& expected) {
  for (std::size_t row = 0; row < 3; ++row)
    for (std::size_t column = 0; column < 3; ++column)
      if (!near(actual[row][column], expected[row * 3 + column]))
        return false;
  return true;
}

template <bench_scalar T, typename Kernel>
  requires matrix_kernel<Kernel, T>
void measure_matrix(benchmark::State& state, MatrixOperation operation, Kernel const& kernel) {
  std::array<Star::Matrix3<T>, matrix_batch_size> a, b, output;
  std::array<ReferenceMatrix, matrix_batch_size> expected;
  for (std::size_t i = 0; i < matrix_batch_size; ++i) {
    auto a_values = fixture_values<T>(i);
    auto b_values = fixture_values<T>(i, true);
    a[i] = make_matrix<T>(a_values);
    b[i] = make_matrix<T>(b_values);
    expected[i] = reference_matrix(operation, a_values, b_values);
    output[i] = kernel(a[i], b[i]);
  }
  auto validate = [&] {
    for (std::size_t i = 0; i < matrix_batch_size; ++i)
      if (!validate_matrix(output[i], expected[i]))
        return false;
    return true;
  };
  if (!validate()) {
    state.SkipWithError("Matrix3 complete result differs from independent long-double fixture");
    return;
  }
  benchmark::DoNotOptimize(a.data());
  benchmark::DoNotOptimize(b.data());
  for (auto _ : state) {
    (void)_;
    benchmark::ClobberMemory();
    for (std::size_t i = 0; i < matrix_batch_size; ++i)
      output[i] = kernel(a[i], b[i]);
    benchmark::DoNotOptimize(output.data());
    benchmark::ClobberMemory();
  }
  if (!validate())
    state.SkipWithError("Matrix3 complete result differs after timed execution");
  state.SetItemsProcessed(state.iterations() * matrix_batch_size);
  state.SetLabel("64 results; public operation(s), local value copies/destruction and output writes timed; fixed storage/setup/full9 oracle untimed; general third row; chain=3 transforms/result");
}

template <bench_scalar T, VectorOperation Operation>
void measure_vector(benchmark::State& state) {
  auto count = static_cast<std::size_t>(state.range(0));
  using Vec2 = Star::Vector<T, 2>;
  auto values = fixture_values<T>(0);
  auto matrix = make_matrix<T>(values);
  std::vector<Vec2> inputs(count), output(count);
  std::vector<T> angles(count), angle_output(count);
  std::vector<std::array<long double, 2>> expected(count);
  for (std::size_t i = 0; i < count; ++i) {
    inputs[i] = Vec2(T(static_cast<int>(i % 19) - 9) / T(8), T(i % 11 + 1) / T(16));
    angles[i] = T(static_cast<int>(i % 23) - 11) / T(8);
    long double x = inputs[i][0];
    long double y = inputs[i][1];
    if constexpr (Operation == VectorOperation::angle) {
      x = std::cos(static_cast<long double>(angles[i]));
      y = std::sin(static_cast<long double>(angles[i]));
    }
    expected[i] = {values[0] * x + values[1] * y, values[3] * x + values[4] * y};
    if constexpr (Operation == VectorOperation::point) {
      expected[i][0] += values[2];
      expected[i][1] += values[5];
    }
    if constexpr (Operation == VectorOperation::angle)
      expected[i][0] = std::atan2(expected[i][1], expected[i][0]);
  }
  auto execute = [&] {
    for (std::size_t i = 0; i < count; ++i) {
      if constexpr (Operation == VectorOperation::point)
        output[i] = matrix.transform_point(inputs[i]);
      else if constexpr (Operation == VectorOperation::direction)
        output[i] = matrix.transform_direction(inputs[i]);
      else
        angle_output[i] = matrix.transform_angle(angles[i]);
    }
  };
  auto validate = [&] {
    for (std::size_t i = 0; i < count; ++i) {
      if constexpr (Operation == VectorOperation::angle) {
        if (!near(angle_output[i], expected[i][0]))
          return false;
      } else if (!near(output[i][0], expected[i][0]) || !near(output[i][1], expected[i][1])) {
        return false;
      }
    }
    return true;
  };
  execute();
  if (!validate()) {
    state.SkipWithError("Matrix3 vector/angle result differs from independent scalar fixture");
    return;
  }
  benchmark::DoNotOptimize(matrix);
  benchmark::DoNotOptimize(inputs.data());
  benchmark::DoNotOptimize(angles.data());
  for (auto _ : state) {
    (void)_;
    benchmark::ClobberMemory();
    execute();
    if constexpr (Operation == VectorOperation::angle)
      benchmark::DoNotOptimize(angle_output.data());
    else
      benchmark::DoNotOptimize(output.data());
    benchmark::ClobberMemory();
  }
  if (!validate())
    state.SkipWithError("Matrix3 vector/angle result differs after timed execution");
  state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(count));
  state.SetLabel(std::format("{} ordered per-point public calls + result writes timed; vector allocation/setup/destruction and full scalar oracle untimed; {}",
      count, Operation == VectorOperation::direction ? "after-only: old API absent"
          : Operation == VectorOperation::angle && std::same_as<T, double> ? "after-only: old angle API narrows to float"
          : "same-type comparable"));
}

template <bench_scalar T>
void measure_copy_to(benchmark::State& state) {
  std::array<Star::Matrix3<T>, matrix_batch_size> matrices;
  std::array<std::array<T, 11>, matrix_batch_size> output;
  std::array<ReferenceMatrix, matrix_batch_size> expected;
  for (std::size_t i = 0; i < matrix_batch_size; ++i) {
    expected[i] = fixture_values<T>(i);
    matrices[i] = make_matrix<T>(expected[i]);
    output[i].fill(T(37.5));
    output[i].front() = T(123.5);
    output[i].back() = T(-321.25);
  }
  auto execute = [&] {
    for (std::size_t i = 0; i < matrix_batch_size; ++i)
      matrices[i].copy_to(std::span<T, 11>(output[i]).template subspan<1, 9>());
  };
  auto validate = [&] {
    for (std::size_t i = 0; i < matrix_batch_size; ++i) {
      if (output[i].front() != T(123.5) || output[i].back() != T(-321.25))
        return false;
      for (std::size_t j = 0; j < 9; ++j)
        if (output[i][j + 1] != T(expected[i][j]))
          return false;
    }
    return true;
  };
  execute();
  if (!validate()) {
    state.SkipWithError("Matrix3 row-major upload buffer differs in nine elements or guard sentinels");
    return;
  }
  benchmark::DoNotOptimize(matrices.data());
  for (auto _ : state) {
    (void)_;
    benchmark::ClobberMemory();
    execute();
    benchmark::DoNotOptimize(output.data());
    benchmark::ClobberMemory();
  }
  if (!validate())
    state.SkipWithError("Matrix3 row-major upload buffer differs after timed execution");
  state.SetItemsProcessed(state.iterations() * matrix_batch_size);
  state.SetLabel("64 full9 row-major span copies timed; fixed storage/setup/full9+sentinel checks untimed; CPU packing only, not GL upload/GPU/frame time");
}

template <bench_scalar T, typename Kernel>
  requires inverse_kernel<Kernel, T>
void measure_inverse(benchmark::State& state, Kernel const& kernel) {
  using Result = std::invoke_result_t<Kernel const&, Star::Matrix3<T> const&>;
  std::array<Star::Matrix3<T>, matrix_batch_size> matrices;
  std::array<Result, matrix_batch_size> output;
  std::array<ReferenceMatrix, matrix_batch_size> expected;
  for (std::size_t i = 0; i < matrix_batch_size; ++i) {
    auto values = fixture_values<T>(i);
    matrices[i] = make_matrix<T>(values);
    expected[i] = reference_inverse(values);
    output[i] = kernel(matrices[i]);
  }
  auto validate = [&] {
    for (std::size_t i = 0; i < matrix_batch_size; ++i)
      if (!output[i] || !validate_matrix(*output[i], expected[i]))
        return false;
    return true;
  };
  if (!validate()) {
    state.SkipWithError("Matrix3 checked inverse failed or differs from independent closed-form cofactor oracle");
    return;
  }
  benchmark::DoNotOptimize(matrices.data());
  for (auto _ : state) {
    (void)_;
    benchmark::ClobberMemory();
    for (std::size_t i = 0; i < matrix_batch_size; ++i)
      output[i] = kernel(matrices[i]);
    benchmark::DoNotOptimize(output.data());
    benchmark::ClobberMemory();
  }
  if (!validate())
    state.SkipWithError("Matrix3 checked inverse differs after timed execution");
  state.SetItemsProcessed(state.iterations() * matrix_batch_size);
  state.SetLabel("64 checked successful inverses + result replacement/destruction/writes timed; fixed storage/setup/full9 cofactor oracle untimed; after-only: original inverse invalid, not comparable");
}

template <bench_scalar T, typename Kernel>
  requires matrix_kernel<Kernel, T>
void register_matrix(std::string_view type, std::string_view name, MatrixOperation operation, Kernel kernel) {
  auto full_name = std::format("Matrix3/{}/{}/Batch64", type, name);
  benchmark::RegisterBenchmark(full_name.c_str(), [operation, kernel](benchmark::State& state) {
    measure_matrix<T>(state, operation, kernel);
  });
}

template <bench_scalar T>
void register_family(std::string_view type) {
  using Matrix = Star::Matrix3<T>;
  using Vec2 = Star::Vector<T, 2>;
  register_matrix<T>(type, "Add", MatrixOperation::add, [](Matrix const& a, Matrix const& b) { return a + b; });
  register_matrix<T>(type, "Subtract", MatrixOperation::subtract, [](Matrix const& a, Matrix const& b) { return a - b; });
  register_matrix<T>(type, "ScalarMultiply", MatrixOperation::scalar_multiply, [](Matrix const& a, Matrix const&) { return a * T(1.25); });
  register_matrix<T>(type, "ScalarDivide", MatrixOperation::scalar_divide, [](Matrix const& a, Matrix const&) { return a / T(1.25); });
  register_matrix<T>(type, "Matmul", MatrixOperation::multiply, [](Matrix const& a, Matrix const& b) { return a * b; });
  register_matrix<T>(type, "SelfAliasMultiplyAssign", MatrixOperation::self_alias, [](Matrix const& a, Matrix const&) {
    auto result = a;
    result *= result;
    return result;
  });
  register_matrix<T>(type, "Translate", MatrixOperation::translate, [](Matrix const& a, Matrix const&) {
    auto result = a;
    result.translate(Vec2(T(1.5), T(-2.25)));
    return result;
  });
  register_matrix<T>(type, "Scale", MatrixOperation::scale, [](Matrix const& a, Matrix const&) {
    auto result = a;
    result.scale(Vec2(T(1.125), T(0.875)), Vec2(T(1.25), T(-0.75)));
    return result;
  });
  register_matrix<T>(type, "Rotate", MatrixOperation::rotate, [](Matrix const& a, Matrix const&) {
    auto result = a;
    result.rotate(T(0.375), Vec2(T(1.25), T(-0.75)));
    return result;
  });
  register_matrix<T>(type, "AffineChain", MatrixOperation::affine_chain, [](Matrix const& a, Matrix const&) {
    auto result = a;
    result.translate(Vec2(T(1.5), T(-2.25)));
    result.scale(Vec2(T(1.125), T(0.875)), Vec2(T(1.25), T(-0.75)));
    result.rotate(T(0.375), Vec2(T(1.25), T(-0.75)));
    return result;
  });
  auto point_name = std::format("Matrix3/{}/Point", type);
  benchmark::RegisterBenchmark(point_name.c_str(), &measure_vector<T, VectorOperation::point>)->Arg(4)->Arg(64)->Arg(1024);
  auto direction_name = std::format("Matrix3/{}/DirectionAfterOnly", type);
  benchmark::RegisterBenchmark(direction_name.c_str(), &measure_vector<T, VectorOperation::direction>)->Arg(64);
  auto angle_name = std::format("Matrix3/{}/{}", type,
      std::same_as<T, double> ? "AngleAfterOnly" : "Angle");
  benchmark::RegisterBenchmark(angle_name.c_str(), &measure_vector<T, VectorOperation::angle>)->Arg(64);
  auto copy_name = std::format("Matrix3/{}/CopyTo/Batch64", type);
  benchmark::RegisterBenchmark(copy_name.c_str(), &measure_copy_to<T>);
  auto inverse_name = std::format("Matrix3/{}/InverseAfterOnly/Batch64", type);
  benchmark::RegisterBenchmark(inverse_name.c_str(), [](benchmark::State& state) {
    measure_inverse<T>(state, [](Matrix const& matrix) { return matrix.inverse(); });
  });
}

class CheckedReporter : public benchmark::ConsoleReporter {
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
  register_family<float>("Float");
  register_family<double>("Double");
  benchmark::Initialize(&argc, argv);
  if (benchmark::ReportUnrecognizedArguments(argc, argv)) {
    benchmark::Shutdown();
    return 1;
  }
  CheckedReporter reporter;
  auto matched = benchmark::RunSpecifiedBenchmarks(&reporter);
  benchmark::Shutdown();
  return reporter.failed || matched == 0 ? 1 : 0;
}

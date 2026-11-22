#include <cstdio>
#include <benchmark/benchmark.h>

import std;
import star.format;
import star.math_common;
import star.benchmark_runner;

namespace {

enum class Operation { Fraction, Modulo, Wrap, Normalize, Difference, Approach, Near };

struct Fingerprints {
  std::uint64_t fixture = 14695981039346656037ull;
  std::uint64_t output = 14695981039346656037ull;

  static void add(std::uint64_t& hash, std::uint64_t value) {
    hash = (hash ^ value) * 1099511628211ull;
  }

  template <std::floating_point T>
  static std::uint64_t bits(T value) {
    if constexpr (std::same_as<T, float>)
      return std::bit_cast<std::uint32_t>(value);
    else {
      static_assert(std::same_as<T, double>);
      return std::bit_cast<std::uint64_t>(value);
    }
  }

  std::string label(std::string_view description) const {
    return std::format("fixture={:016x}; output={:016x}; {}", fixture, output, description);
  }
};

template <std::floating_point T>
struct Input {
  T value;
  T other;
  T period;
};

template <std::floating_point T>
std::array<Input<T>, 64> make_input() {
  constexpr std::array<long double, 16> values{
      -1024.75L, -32.5L, -12.25L, -6.5L, -1.75L, -1.0L, -0.125L, -0.0L,
      0.0L, 0.125L, 0.5L, 1.0L, 1.75L, 6.5L, 32.25L, 1024.75L};
  std::array<Input<T>, 64> result;
  for (std::size_t i = 0; i < result.size(); ++i)
    result[i] = {static_cast<T>(values[i % values.size()]),
                 static_cast<T>(values[(i * 7 + 3) % values.size()]),
                 static_cast<T>(i % 4 == 3 ? 0 : i % 4 == 0 ? 8 : i % 4 == 1 ? 10 : 1024)};
  return result;
}

template <Operation Op, std::floating_point T>
T evaluate(Input<T> const& input) {
  if constexpr (Op == Operation::Fraction)
    return Star::fractional_part(input.value);
  else if constexpr (Op == Operation::Modulo)
    return Star::positive_mod(input.value, input.period);
  else if constexpr (Op == Operation::Wrap)
    return Star::wrap_difference(input.value, input.other, input.period);
  else if constexpr (Op == Operation::Normalize)
    return Star::normalize_angle(input.value);
  else if constexpr (Op == Operation::Difference)
    return Star::angle_difference(input.value, input.other);
  else if constexpr (Op == Operation::Approach)
    return Star::approach_angle(input.other, input.value, T(0.125));
  else
    return static_cast<T>(Star::near_equal(input.value, input.other, 2))
        + T(2) * static_cast<T>(Star::near_zero(input.value));
}

// Independent extended-precision oracle stays outside the project's fast-math
// contract. No timed operation calls this code. Changed angle endpoint/huge
// fractional domains belong to correctness tests, not old/new speed ratios.
#pragma float_control(precise, on, push)
long double reference_mod(long double value, long double period) {
  if (period == 0)
    return value;
  auto result = std::fmod(value, period);
  return result < 0 ? result + period : result;
}

long double reference_angle(long double value) {
  constexpr auto pi = 3.141592653589793238462643383279502884L;
  auto result = std::remainder(value, 2 * pi);
  return result >= pi ? result - 2 * pi : result;
}

template <Operation Op, std::floating_point T>
bool validate(Input<T> const& input, T actual) {
  long double value = input.value, other = input.other, period = input.period;
  long double expected;
  if constexpr (Op == Operation::Fraction)
    expected = value - std::floor(value);
  else if constexpr (Op == Operation::Modulo)
    expected = reference_mod(value, period);
  else if constexpr (Op == Operation::Wrap) {
    expected = reference_mod(value, period) - reference_mod(other, period);
    if (expected > period / 2)
      expected -= period;
    else if (expected < -period / 2)
      expected += period;
  } else if constexpr (Op == Operation::Normalize)
    expected = reference_angle(value);
  else if constexpr (Op == Operation::Difference)
    expected = reference_angle(other - value);
  else if constexpr (Op == Operation::Approach) {
    auto delta = reference_angle(other - value);
    expected = reference_angle(value + std::clamp(delta, -0.125L, 0.125L));
  } else {
    bool equal = std::abs(value - other) <= std::numeric_limits<T>::epsilon()
        * std::max(std::abs(value), std::abs(other)) * 2;
    bool zero = std::abs(value) <= static_cast<long double>(std::numeric_limits<T>::min()) * 2;
    expected = static_cast<int>(equal) + 2 * static_cast<int>(zero);
  }
  // The old angle implementation loses input precision while adding pi. This
  // shared ordinary-domain fixture allows that bounded error, not endpoint
  // interval violations or long-double downcasts.
  auto tolerance = static_cast<long double>(std::numeric_limits<T>::epsilon())
      * (Op == Operation::Normalize || Op == Operation::Difference || Op == Operation::Approach ? 32 : 8);
  return std::abs(static_cast<long double>(actual) - expected) <= tolerance;
}
#pragma float_control(pop)
template <Operation Op, std::floating_point T>
void numeric(benchmark::State& state) {
  auto input = make_input<T>();
  if constexpr (Op == Operation::Near) {
    constexpr auto minimum = std::numeric_limits<T>::min();
    constexpr auto epsilon = std::numeric_limits<T>::epsilon();
    input[0] = {T(1), T(1) + epsilon, T(0)};
    input[1] = {T(1), T(1) + T(4) * epsilon, T(0)};
    input[2] = {minimum, T(0), T(0)};
    input[3] = {T(3) * minimum, T(0), T(0)};
    input[4] = {-minimum, T(0), T(0)};
  }
  if constexpr (Op == Operation::Normalize || Op == Operation::Difference || Op == Operation::Approach)
    for (auto& item : input) {
      item.value /= T(128);
      item.other /= T(128);
    }
  if constexpr (Op == Operation::Wrap) {
    input[0] = {T(4), T(0), T(8)};
    input[1] = {T(0), T(4), T(8)};
  }
  auto count = static_cast<std::size_t>(state.range(0));
  std::array<T, 64> output{};
  for (auto const& item : input)
    if (!validate<Op>(item, evaluate<Op>(item))) {
      state.SkipWithError("complete independent numeric output differs");
      return;
    }
  for (auto _ : state) {
    (void)_;
    benchmark::ClobberMemory();
    for (std::size_t i = 0; i < count; ++i)
      output[i] = evaluate<Op>(input[i]);
    benchmark::DoNotOptimize(output);
    benchmark::ClobberMemory();
  }
  Fingerprints fingerprints;
  for (std::size_t i = 0; i < count; ++i) {
    if (!validate<Op>(input[i], output[i])) {
      state.SkipWithError("actual timed numeric output differs from independent oracle");
      return;
    }
    Fingerprints::add(fingerprints.fixture, Fingerprints::bits(input[i].value));
    Fingerprints::add(fingerprints.fixture, Fingerprints::bits(input[i].other));
    Fingerprints::add(fingerprints.fixture, Fingerprints::bits(input[i].period));
    Fingerprints::add(fingerprints.output, Fingerprints::bits(output[i]));
  }
  state.SetLabel(fingerprints.label(
      Op == Operation::Normalize || Op == Operation::Difference || Op == Operation::Approach
      ? "one result/item; independent numerical oracle; angle output bits may differ by version"
      : Op == Operation::Near ? "two predicates/item; normal-threshold and epsilon boundaries"
                             : "one scalar result/item; shared finite valid domain"));
  state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(count));
}

struct IntegerOutput {
  std::int64_t modulo;
  std::int64_t difference;
};

IntegerOutput reference_integer(std::int64_t value) {
  while (value < 0)
    value += 1024;
  while (value >= 1024)
    value -= 1024;
  return {value, value - 512};
}

void integer_wrap(benchmark::State& state) {
  std::array<std::int64_t, 64> input;
  std::array<IntegerOutput, 64> output{};
  for (std::size_t i = 0; i < input.size(); ++i)
    input[i] = static_cast<std::int64_t>(i) * 137 - 4096;
  for (auto value : input) {
    auto expected = reference_integer(value);
    if (Star::positive_mod(value, std::int64_t(1024)) != expected.modulo
        || Star::wrap_difference(value, std::int64_t(512), std::int64_t(1024)) != expected.difference) {
      state.SkipWithError("complete independent integer modulo/wrap output differs");
      return;
    }
  }
  auto count = static_cast<std::size_t>(state.range(0));
  for (auto _ : state) {
    (void)_;
    benchmark::ClobberMemory();
    for (std::size_t i = 0; i < count; ++i)
      output[i] = {Star::positive_mod(input[i], std::int64_t(1024)),
                   Star::wrap_difference(input[i], std::int64_t(512), std::int64_t(1024))};
    benchmark::DoNotOptimize(output);
    benchmark::ClobberMemory();
  }
  Fingerprints fingerprints;
  for (std::size_t i = 0; i < count; ++i) {
    auto expected = reference_integer(input[i]);
    if (output[i].modulo != expected.modulo || output[i].difference != expected.difference) {
      state.SkipWithError("actual timed integer modulo/wrap fields differ from independent oracle");
      return;
    }
    Fingerprints::add(fingerprints.fixture, static_cast<std::uint64_t>(input[i]));
    Fingerprints::add(fingerprints.output, static_cast<std::uint64_t>(output[i].modulo));
    Fingerprints::add(fingerprints.output, static_cast<std::uint64_t>(output[i].difference));
  }
  state.SetLabel(fingerprints.label("two integer results/item; signed half-period ties"));
  state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(count));
}

struct ByteOutput {
  std::uint8_t encoded;
  std::uint8_t saturated;
  float decoded;
};

bool validate_byte(float value, std::uint8_t byte, ByteOutput const& actual) {
  auto shifted = value * 2 - 0.5f;
  auto expected = static_cast<unsigned>(value * 256) * 255 / 256;
  auto saturated = static_cast<unsigned>(std::clamp(shifted, 0.0f, 1.0f) * 256) * 255 / 256;
  return actual.encoded == expected && actual.saturated == saturated
      && std::abs(static_cast<double>(actual.decoded) - static_cast<double>(byte) / 255) <= 1e-7;
}

void byte_quantization(benchmark::State& state) {
  std::array<float, 64> input;
  std::array<std::uint8_t, 64> bytes;
  std::array<ByteOutput, 64> output{};
  for (std::size_t i = 0; i < input.size(); ++i) {
    input[i] = i == 0 ? 0.5f : i == 1 ? 0.0f : i == 2 ? 1.0f
        : static_cast<float>((i * 17) % 257) / 256;
    bytes[i] = static_cast<std::uint8_t>(i * 4);
    ByteOutput actual{Star::float_to_byte(input[i]), Star::float_to_byte(input[i] * 2 - 0.5f, true),
                      Star::byte_to_float(bytes[i])};
    if (!validate_byte(input[i], bytes[i], actual)) {
      state.SkipWithError("complete independent byte quantization output differs");
      return;
    }
  }
  auto count = static_cast<std::size_t>(state.range(0));
  for (auto _ : state) {
    (void)_;
    benchmark::ClobberMemory();
    for (std::size_t i = 0; i < count; ++i)
      output[i] = {Star::float_to_byte(input[i]), Star::float_to_byte(input[i] * 2 - 0.5f, true),
                   Star::byte_to_float(bytes[i])};
    benchmark::DoNotOptimize(output);
    benchmark::ClobberMemory();
  }
  Fingerprints fingerprints;
  for (std::size_t i = 0; i < count; ++i) {
    if (!validate_byte(input[i], bytes[i], output[i])) {
      state.SkipWithError("actual timed byte output fields differ from independent oracle");
      return;
    }
    Fingerprints::add(fingerprints.fixture, Fingerprints::bits(input[i]));
    Fingerprints::add(fingerprints.fixture, bytes[i]);
    Fingerprints::add(fingerprints.output, output[i].encoded);
    Fingerprints::add(fingerprints.output, output[i].saturated);
    Fingerprints::add(fingerprints.output, Fingerprints::bits(output[i].decoded));
  }
  state.SetLabel(fingerprints.label("three results/item: truncation, saturation, normalized byte"));
  state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(count));
}

template <Operation Op, std::floating_point T>
void register_numeric(std::string_view type, std::string_view operation) {
  auto name = std::format("MathCommon/{}/{}", type, operation);
  benchmark::RegisterBenchmark(name.c_str(), numeric<Op, T>)->Arg(1)->Arg(64);
}

template <std::floating_point T>
void register_type(std::string_view type) {
  register_numeric<Operation::Fraction, T>(type, "FractionalPart");
  register_numeric<Operation::Modulo, T>(type, "PositiveMod");
  register_numeric<Operation::Wrap, T>(type, "WrapDifference");
  register_numeric<Operation::Normalize, T>(type, "NormalizeAngle");
  register_numeric<Operation::Difference, T>(type, "AngleDifference");
  register_numeric<Operation::Approach, T>(type, "ApproachAngle");
  register_numeric<Operation::Near, T>(type, "NearEqualAndZero");
}

}// namespace

void register_spline_benchmarks();

// The native main/Google Benchmark ABI is the only pointer boundary.
int main(int argc, char** argv) {
  return Star::run_benchmarks(argc, argv, [](int) {
    register_type<float>("Float");
    register_type<double>("Double");
    register_spline_benchmarks();
    benchmark::RegisterBenchmark("MathCommon/Int64/ModuloAndWrap", integer_wrap)->Arg(1)->Arg(64);
    benchmark::RegisterBenchmark("MathCommon/Byte/Quantization", byte_quantization)->Arg(1)->Arg(64);
    return 0;
  });
}

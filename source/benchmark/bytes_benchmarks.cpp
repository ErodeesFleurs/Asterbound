#include <benchmark/benchmark.h>
#include <cstdio>

import std;
import star.bytes;
import star.format;
import star.native_runtime;

namespace {
using Star::ByteOrder;
constexpr std::size_t batch_size = 64;

template <class T>
concept scalar_value = (std::unsigned_integral<T> && std::has_unique_object_representations_v<T>)
  || (std::floating_point<T> && (sizeof(T) == 4 || sizeof(T) == 8));

enum class scalar_operation { to_big, to_little, to_mixed };
enum class raw_operation { reverse, reverse_copy, to_mixed, to_mixed_copy };

ByteOrder mixed_order(std::size_t index) {
  constexpr std::array orders{ByteOrder::BigEndian, ByteOrder::NoConversion, ByteOrder::LittleEndian};
  return orders[index % orders.size()];
}

bool needs_reverse(ByteOrder order) {
  // Independent oracle: no API under test, including the native-order helper.
  return order != ByteOrder::NoConversion
    && ((order == ByteOrder::LittleEndian) != (std::endian::native == std::endian::little));
}

std::uint64_t fingerprint(std::span<std::byte const> bytes) {
  std::uint64_t hash = 14695981039346656037ull;
  for (auto byte : bytes)
    hash = (hash ^ std::to_integer<unsigned char>(byte)) * 1099511628211ull;
  return hash;
}

bool require(benchmark::State& state, bool condition) {
  if (!condition)
    state.SkipWithError("complete result differs from independent byte oracle");
  return condition;
}

void label(benchmark::State& state, std::span<std::byte const> input, std::span<std::byte const> output,
           std::string_view detail) {
  state.SetLabel(std::format("fixture-fnv1a={:016x}; complete-result-fnv1a={:016x}; result-bytes={}; batch={}; {}",
                             fingerprint(input), fingerprint(output), output.size(), batch_size, detail));
}

void account(benchmark::State& state, std::size_t bytes_per_item) {
  state.SetItemsProcessed(state.iterations() * batch_size);
  state.SetBytesProcessed(state.iterations() * batch_size * bytes_per_item);
}

template <scalar_value T, scalar_operation Operation> T convert_scalar(T value, ByteOrder order) {
  if constexpr (Operation == scalar_operation::to_big)
    return Star::to_big_endian(value);
  else if constexpr (Operation == scalar_operation::to_little)
    return Star::to_little_endian(value);
  else
    return Star::to_byte_order(order, value);
}

template <scalar_value T> T scalar_fixture(std::size_t index) {
  using bits_type =
    std::conditional_t<sizeof(T) == 2, std::uint16_t, std::conditional_t<sizeof(T) == 4, std::uint32_t, std::uint64_t>>;
  auto bits = static_cast<bits_type>((0x9e3779b97f4a7c15ull * (index + 1)) ^ (index << 9));
  if constexpr (std::floating_point<T>) {
    // Include signed zeros, infinities, subnormals, quiet/signaling NaNs with
    // nontrivial payloads. Comparisons below inspect bytes, never FP equality.
    if constexpr (sizeof(T) == 4) {
      constexpr std::array<std::uint32_t, 8> special{0, 0x80000000u, 0x7f800000u, 0xff800000u,
                                                     1, 0x80000001u, 0x7fc12345u, 0x7f812345u};
      if (index < special.size())
        bits = special[index];
    } else {
      constexpr std::array<std::uint64_t, 8> special{
        0, 0x8000000000000000ull, 0x7ff0000000000000ull, 0xfff0000000000000ull,
        1, 0x8000000000000001ull, 0x7ff8123456789abcull, 0x7ff0123456789abcull};
      if (index < special.size())
        bits = special[index];
    }
  } else if (index == 0)
    bits = 0;
  return std::bit_cast<T>(bits);
}

template <scalar_value T, scalar_operation Operation> void measure_scalar(benchmark::State& state) {
  std::array<T, batch_size> input;
  std::array<T, batch_size> output;
  std::array<ByteOrder, batch_size> orders;
  std::array<std::byte, sizeof(input)> expected;
  for (std::size_t i = 0; i != batch_size; ++i) {
    input[i] = scalar_fixture<T>(i);
    if constexpr (Operation == scalar_operation::to_big)
      orders[i] = ByteOrder::BigEndian;
    else if constexpr (Operation == scalar_operation::to_little)
      orders[i] = ByteOrder::LittleEndian;
    else
      orders[i] = mixed_order(i);
    auto bytes = std::as_bytes(std::span<T const>(&input[i], 1));
    for (std::size_t j = 0; j != sizeof(T); ++j)
      expected[i * sizeof(T) + j] = bytes[needs_reverse(orders[i]) ? sizeof(T) - 1 - j : j];
  }
  auto evaluate = [&] {
    for (std::size_t i = 0; i != batch_size; ++i)
      output[i] = convert_scalar<T, Operation>(input[i], orders[i]);
  };
  auto validate = [&] { return require(state, std::ranges::equal(std::as_bytes(std::span(output)), expected)); };
  evaluate();
  if (!validate())
    return;
  label(state, std::as_bytes(std::span(input)), expected, "scalar conversion; bit-preserving oracle");
  for (auto _ : state) {
    (void)_;
    benchmark::DoNotOptimize(input.data());
    benchmark::DoNotOptimize(orders.data());
    benchmark::ClobberMemory();
    evaluate();
    benchmark::DoNotOptimize(output.data());
    benchmark::ClobberMemory();
  }
  if (!validate())
    return;
  account(state, sizeof(T));
}

template <raw_operation Operation> void convert_raw(ByteOrder order, void* dest, void const* src, std::size_t size) {
  if constexpr (Operation == raw_operation::reverse)
    Star::reverse_bytes(dest, size);
  else if constexpr (Operation == raw_operation::reverse_copy)
    Star::reverse_bytes_copy(dest, src, size);
  else if constexpr (Operation == raw_operation::to_mixed)
    Star::to_byte_order(order, dest, size);
  else
    Star::to_byte_order(order, dest, src, size);
}

template <std::size_t Size, raw_operation Operation> void measure_raw(benchmark::State& state) {
  constexpr bool copying = Operation == raw_operation::reverse_copy || Operation == raw_operation::to_mixed_copy;
  constexpr bool reversing = Operation == raw_operation::reverse || Operation == raw_operation::reverse_copy;
  // Every lane starts at +1 from an eight-byte-aligned stride. Both guards and
  // all payload bytes participate in the untimed oracle and fingerprints.
  constexpr std::size_t stride = ((Size + 2 + 7) / 8) * 8;
  alignas(8) std::array<std::byte, stride * batch_size> input;
  alignas(8) std::array<std::byte, stride * batch_size> output;
  std::array<ByteOrder, batch_size> orders;
  for (std::size_t i = 0; i != input.size(); ++i)
    input[i] = std::byte((i * 37 + i / 7 + 11) & 255);
  for (std::size_t i = 0; i != batch_size; ++i)
    orders[i] = mixed_order(i);
  auto initial = input;
  if constexpr (copying)
    initial.fill(std::byte{0xa5});
  output = initial;
  auto oracle = [&](bool odd) {
    auto expected = initial;
    for (std::size_t i = 0; i != batch_size; ++i) {
      bool reverse = (reversing || needs_reverse(orders[i])) && (copying || odd);
      for (std::size_t j = 0; j != Size; ++j)
        expected[i * stride + 1 + j] = input[i * stride + 1 + (reverse ? Size - 1 - j : j)];
    }
    return expected;
  };
  auto evaluate = [&] {
    for (std::size_t i = 0; i != batch_size; ++i)
      convert_raw<Operation>(orders[i], output.data() + i * stride + 1, input.data() + i * stride + 1, Size);
  };
  evaluate();
  auto expected = oracle(true);
  if (!require(state, output == expected))
    return;
  label(state, input, expected,
        copying ? "misaligned copy; guards included; allocation untimed"
                : "misaligned in-place; repeated involution; guards included; allocation untimed");
  output = initial;
  for (auto _ : state) {
    (void)_;
    benchmark::DoNotOptimize(input.data());
    benchmark::DoNotOptimize(orders.data());
    benchmark::ClobberMemory();
    evaluate();
    benchmark::DoNotOptimize(output.data());
    benchmark::ClobberMemory();
  }
  if (!require(state, output == oracle((state.iterations() & 1) != 0)))
    return;
  account(state, Size);
}

template <scalar_value T> void register_scalar(std::string_view name) {
  benchmark::RegisterBenchmark(std::format("Bytes/Scalar/{}/ToBig", name).c_str(),
                               measure_scalar<T, scalar_operation::to_big>);
  benchmark::RegisterBenchmark(std::format("Bytes/Scalar/{}/ToLittle", name).c_str(),
                               measure_scalar<T, scalar_operation::to_little>);
  benchmark::RegisterBenchmark(std::format("Bytes/Scalar/{}/ToMixed", name).c_str(),
                               measure_scalar<T, scalar_operation::to_mixed>);
}

template <std::size_t Size> void register_reverse() {
  benchmark::RegisterBenchmark(std::format("Bytes/Raw/ReverseInPlace/{}", Size).c_str(),
                               measure_raw<Size, raw_operation::reverse>);
  benchmark::RegisterBenchmark(std::format("Bytes/Raw/ReverseCopy/{}", Size).c_str(),
                               measure_raw<Size, raw_operation::reverse_copy>);
}

template <std::size_t Size> void register_mixed() {
  benchmark::RegisterBenchmark(std::format("Bytes/Raw/ToMixedInPlace/{}", Size).c_str(),
                               measure_raw<Size, raw_operation::to_mixed>);
  benchmark::RegisterBenchmark(std::format("Bytes/Raw/ToMixedCopy/{}", Size).c_str(),
                               measure_raw<Size, raw_operation::to_mixed_copy>);
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
  register_scalar<std::uint16_t>("U16");
  register_scalar<std::uint32_t>("U32");
  register_scalar<std::uint64_t>("U64");
  register_scalar<float>("F32");
  register_scalar<double>("F64");
  register_reverse<0>();
  register_reverse<2>();
  register_reverse<4>();
  register_reverse<8>();
  register_reverse<3>();
  register_reverse<17>();
  register_reverse<256>();
  register_reverse<4096>();
  register_mixed<17>();
  register_mixed<256>();
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

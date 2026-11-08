#include <cstdio>
#include <benchmark/benchmark.h>
#include <zlib-ng.h>
#include <zstd.h>

import std;
import star.byte_array;
import star.compression;
import star.encode;
import star.format;
import star.native_runtime;
import star.string;
import star.zstd_compression;

namespace {
using Star::ByteArray;
using Star::StarException;
constexpr std::size_t batch_size = 32;
constexpr std::size_t codec_batch_size = 4;

std::uint64_t fingerprint(std::span<char const> bytes) {
  std::uint64_t hash = 14695981039346656037ull;
  for (unsigned char byte : bytes) hash = (hash ^ byte) * 1099511628211ull;
  return hash;
}
std::string fixture(std::size_t size) {
  std::string bytes(size, '\0');
  for (std::size_t i = 0; i != size; ++i) bytes[i] = char((i * 37 + i / 7 + 11) & 255);
  return bytes;
}
bool require(benchmark::State& state, bool condition, char const* message) {
  if (!condition) state.SkipWithError(message);
  return condition;
}
template <class Result>
concept checked_result = requires(Result const& result) {
  { static_cast<bool>(result) } -> std::same_as<bool>;
  { result.error().what() } -> std::convertible_to<char const*>;
};
template <checked_result Result>
bool checked(benchmark::State& state, Result const& result) {
  if (result) return true;
  state.SkipWithError(result.error().what());
  return false;
}
bool equal_bytes(std::span<char const> actual, std::string_view expected) {
  return actual.size() == expected.size() && std::equal(actual.begin(), actual.end(), expected.begin());
}
void label(benchmark::State& state, std::span<char const> input, std::span<char const> output, std::size_t batch) {
  state.SetLabel(std::format("fixture-fnv1a={:016x}; complete-result-fnv1a={:016x}; result-bytes={}; batch={}; ownership/destruction timed",
      fingerprint(input), fingerprint(output), output.size(), batch));
}
void account(benchmark::State& state, std::size_t bytes, std::size_t batch) {
  state.SetItemsProcessed(state.iterations() * batch);
  state.SetBytesProcessed(state.iterations() * batch * bytes);
}

enum class operation { append, append_reserved, push_back, push_back_reserved, terminated_text,
  insert, combine_extend, compare_equal, compare_unequal, owning_substr, borrowed_subspan };

template <operation Operation>
void measure_bytes(benchmark::State& state) {
  auto bytes = fixture(static_cast<std::size_t>(state.range(0)));
  auto input = ByteArray::from_string(bytes);
  auto other = ByteArray::from_string(bytes);
  if constexpr (Operation == operation::compare_unequal) other[other.size() - 1] ^= 1;
  if constexpr (Operation == operation::combine_extend) other.remove_suffix(other.size() / 3);
  auto expected = bytes;
  if constexpr (Operation == operation::terminated_text) expected += '\0';
  if constexpr (Operation == operation::insert) expected.insert(expected.size() / 2, 1, '\x5a');
  if constexpr (Operation == operation::combine_extend)
    for (std::size_t i = 0; i != other.size(); ++i) expected[i] = char(static_cast<unsigned char>(bytes[i]) ^ static_cast<unsigned char>(bytes[i]));
  if constexpr (Operation == operation::owning_substr || Operation == operation::borrowed_subspan)
    expected = bytes.substr(bytes.size() / 4, bytes.size() / 2);

  auto evaluate = [&]() {
    if constexpr (Operation == operation::terminated_text) return ByteArray::from_string_with_null(bytes);
    else if constexpr (Operation == operation::owning_substr) return input.substr(bytes.size() / 4, bytes.size() / 2);
    else if constexpr (Operation == operation::borrowed_subspan) return input.span().subspan(bytes.size() / 4, bytes.size() / 2);
    else if constexpr (Operation == operation::compare_equal || Operation == operation::compare_unequal) return input == other;
    else if constexpr (Operation == operation::combine_extend)
      return input.combine([](char a, char b) { return char(static_cast<unsigned char>(a) ^ static_cast<unsigned char>(b)); }, other, true);
    else if constexpr (Operation == operation::insert) {
      ByteArray output(input);
      output.insert(output.size() / 2, '\x5a');
      return output;
    } else {
      ByteArray output;
      if constexpr (Operation == operation::append_reserved || Operation == operation::push_back_reserved) output.reserve(bytes.size());
      if constexpr (Operation == operation::push_back || Operation == operation::push_back_reserved) {
        for (char byte : bytes) output.push_back(byte);
      } else {
        for (std::size_t offset = 0; offset < bytes.size(); offset += 13)
          output.append(std::span<char const>(bytes.data() + offset, std::min(std::size_t(13), bytes.size() - offset)));
      }
      return output;
    }
  };
  {
    auto validation = evaluate();
    if constexpr (Operation == operation::compare_equal || Operation == operation::compare_unequal) {
      constexpr bool expected_comparison = Operation == operation::compare_equal;
      if (!require(state, validation == expected_comparison, "comparison differs from independent fixture")) return;
      expected.assign(1, char(expected_comparison));
      // Both complete comparison operands belong to the fixture fingerprint.
      bytes.append(other.data(), other.size());
    } else if constexpr (Operation == operation::borrowed_subspan) {
      if (!require(state, equal_bytes(validation, expected), "complete borrowed result differs")) return;
    } else if (!require(state, equal_bytes(validation.span(), expected), "complete owning result differs")) return;
  }
  label(state, std::span<char const>(bytes), std::span<char const>(expected), batch_size);
  for (auto _ : state) {
    (void)_;
    for (std::size_t i = 0; i != batch_size; ++i) {
      benchmark::ClobberMemory();
      auto output = evaluate();
      if constexpr (Operation == operation::compare_equal || Operation == operation::compare_unequal) benchmark::DoNotOptimize(output);
      else if constexpr (Operation == operation::borrowed_subspan) { benchmark::DoNotOptimize(output.data()); benchmark::DoNotOptimize(output.size()); }
      else { benchmark::DoNotOptimize(output.data()); benchmark::DoNotOptimize(output.size()); }
      benchmark::ClobberMemory();
    }
  }
  account(state, input.size(), batch_size);
}

// Independent fixture encoders: never manufacture expected decoded bytes with the API under test.
std::string encode_hex(std::string_view bytes) {
  constexpr std::string_view digits = "0123456789abcdef";
  std::string output;
  for (unsigned char byte : bytes) { output += digits[byte >> 4]; output += digits[byte & 15]; }
  return output;
}
std::string encode_base64(std::string_view bytes) {
  constexpr std::string_view alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string output;
  for (std::size_t i = 0; i < bytes.size(); i += 3) {
    auto remaining = bytes.size() - i;
    auto word = unsigned(static_cast<unsigned char>(bytes[i])) << 16;
    if (remaining > 1) word |= unsigned(static_cast<unsigned char>(bytes[i + 1])) << 8;
    if (remaining > 2) word |= unsigned(static_cast<unsigned char>(bytes[i + 2]));
    output += alphabet[(word >> 18) & 63]; output += alphabet[(word >> 12) & 63];
    output += remaining > 1 ? alphabet[(word >> 6) & 63] : '=';
    output += remaining > 2 ? alphabet[word & 63] : '=';
  }
  return output;
}
void measure_decode(benchmark::State& state, bool base64, bool permissive) {
  auto expected = fixture(static_cast<std::size_t>(state.range(0)));
  auto encoded = base64 ? encode_base64(expected) : encode_hex(expected);
  if (permissive) {
    if (base64) {
      // Missing padding and a nonalphabet suffix terminate decoding without an error.
      while (encoded.ends_with('=')) encoded.pop_back();
      encoded += "!ignored";
    } else {
      // Invalid hex nibbles map to zero; the final odd nibble is ignored.
      encoded[0] = '?'; expected[0] = char(static_cast<unsigned char>(expected[0]) & 15);
      encoded += 'f';
    }
  }
  Star::String text{std::string_view(encoded)};
  auto evaluate = [&]() { return base64 ? Star::base64Decode(text) : Star::hexDecode(text); };
  auto validation = evaluate();
  if (!checked(state, validation) || !require(state, equal_bytes(validation->span(), expected), "complete decoded bytes differ from independent fixture")) return;
  label(state, std::span<char const>(encoded), validation->span(), batch_size);
  for (auto _ : state) {
    (void)_;
    for (std::size_t i = 0; i != batch_size; ++i) {
      auto output = evaluate();
      if (!checked(state, output)) return;
      benchmark::DoNotOptimize(output->data()); benchmark::DoNotOptimize(output->size());
      benchmark::ClobberMemory();
    }
  }
  account(state, encoded.size(), batch_size);
}

// Native one-shot codecs are independent untimed fixture producers/validators.
// Full decompressed golden bytes are fixture(size), not a production round trip.
void measure_codec(benchmark::State& state, bool zstd, bool decompress) {
  auto golden = fixture(static_cast<std::size_t>(state.range(0)));
  std::string compressed(zstd ? ZSTD_compressBound(golden.size()) : zng_compressBound(golden.size()), '\0');
  if (zstd) {
    auto size = ZSTD_compress(compressed.data(), compressed.size(), golden.data(), golden.size(), 3);
    if (!require(state, !ZSTD_isError(size), "independent zstd fixture compression failed")) return;
    compressed.resize(size);
  } else {
    auto size = compressed.size();
    auto status = zng_compress2(reinterpret_cast<unsigned char*>(compressed.data()), &size,
        reinterpret_cast<unsigned char const*>(golden.data()), golden.size(), Star::MediumCompression);
    if (!require(state, status == Z_OK, "independent zlib fixture compression failed")) return;
    compressed.resize(size);
  }
  auto input = ByteArray::from_string(decompress ? std::string_view(compressed) : std::string_view(golden));
  auto evaluate = [&]() -> std::expected<ByteArray, StarException> {
    if (zstd) return decompress ? Star::ZstdCompression::decompress(input) : Star::ZstdCompression::compress(input, 3);
    return decompress ? Star::uncompressData(input) : Star::compressData(input);
  };
  auto validation = evaluate();
  if (!checked(state, validation)) return;
  if (decompress) {
    if (!require(state, equal_bytes(validation->span(), golden), "complete decompressed bytes differ from independent golden")) return;
  } else {
    std::string decoded(golden.size(), '\0');
    if (zstd) {
      auto size = ZSTD_decompress(decoded.data(), decoded.size(), validation->data(), validation->size());
      if (!require(state, !ZSTD_isError(size) && size == golden.size(), "independent zstd result validation failed")) return;
    } else {
      auto size = decoded.size();
      auto status = zng_uncompress(reinterpret_cast<unsigned char*>(decoded.data()), &size,
          reinterpret_cast<unsigned char const*>(validation->data()), validation->size());
      if (!require(state, status == Z_OK && size == golden.size(), "independent zlib result validation failed")) return;
    }
    if (!require(state, decoded == golden, "complete compressed result does not decode to independent golden")) return;
  }
  state.SetLabel(std::format("fixture-fnv1a={:016x}; complete-result-fnv1a={:016x}; result-bytes={}; decompressed-golden-fnv1a={:016x}; batch={}; ownership/destruction timed",
      fingerprint(input.span()), fingerprint(validation->span()), validation->size(),
      fingerprint(std::span<char const>(golden)), codec_batch_size));
  for (auto _ : state) {
    (void)_;
    for (std::size_t i = 0; i != codec_batch_size; ++i) {
      auto output = evaluate();
      if (!checked(state, output)) return;
      benchmark::DoNotOptimize(output->data()); benchmark::DoNotOptimize(output->size());
      benchmark::ClobberMemory();
    }
  }
  account(state, golden.size(), codec_batch_size);
}

template <operation Operation>
void register_bytes(char const* name) {
  auto entry = benchmark::RegisterBenchmark(name, measure_bytes<Operation>);
  entry->Arg(17)->Arg(4096);
  if constexpr (Operation == operation::append || Operation == operation::append_reserved)
    for (auto size : {51, 52, 53, 3327, 3328, 3329}) entry->Arg(size);
}
class checked_reporter final : public benchmark::ConsoleReporter {
public:
  bool failed = false;
  void ReportRuns(std::vector<Run> const& reports) override {
    for (auto const& report : reports) failed |= report.skipped == benchmark::internal::SkippedWithError;
    ConsoleReporter::ReportRuns(reports);
  }
};
}

int main(int argc, char** argv) {
  auto runtime = Star::NativeRuntime::create();
  if (!runtime) { (void)Star::print(stderr, "{}\n", runtime.error().what()); return 1; }
  register_bytes<operation::append>("ByteArray/AppendChunks13");
  register_bytes<operation::append_reserved>("ByteArray/AppendChunks13Reserved");
  register_bytes<operation::push_back>("ByteArray/PushBack");
  register_bytes<operation::push_back_reserved>("ByteArray/PushBackReserved");
  register_bytes<operation::terminated_text>("ByteArray/TerminatedText");
  register_bytes<operation::insert>("ByteArray/InsertMiddle");
  register_bytes<operation::combine_extend>("ByteArray/CombineExtend");
  register_bytes<operation::compare_equal>("ByteArray/CompareEqual");
  register_bytes<operation::compare_unequal>("ByteArray/CompareUnequalLast");
  register_bytes<operation::owning_substr>("ByteArray/OwningSubstring");
  register_bytes<operation::borrowed_subspan>("ByteArray/BorrowedSubspan");
  for (bool base64 : {false, true}) for (bool permissive : {false, true}) {
    auto name = std::format("ByteArray/Decode/{}/{}", base64 ? "Base64" : "Hex", permissive ? "Permissive" : "Valid");
    benchmark::RegisterBenchmark(name.c_str(), [base64, permissive](benchmark::State& state) { measure_decode(state, base64, permissive); })->Arg(17)->Arg(4096);
  }
  for (bool zstd : {false, true}) for (bool decompress : {false, true}) {
    auto name = std::format("ByteArray/{}/{}", zstd ? "Zstd" : "Zlib", decompress ? "Decompress" : "Compress");
    benchmark::RegisterBenchmark(name.c_str(), [zstd, decompress](benchmark::State& state) { measure_codec(state, zstd, decompress); })->Arg(17)->Arg(4096);
  }
  benchmark::Initialize(&argc, argv);
  if (benchmark::ReportUnrecognizedArguments(argc, argv)) { benchmark::Shutdown(); return 1; }
  checked_reporter reporter;
  auto matched = benchmark::RunSpecifiedBenchmarks(&reporter);
  benchmark::Shutdown();
  return reporter.failed || matched == 0 ? 1 : 0;
}

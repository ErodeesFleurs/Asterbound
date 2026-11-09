#include <cstdio>
#include <benchmark/benchmark.h>

import std;
import star.format;
import star.hash;
import star.json;
import star.native_runtime;
import star.string;
import star.unicode;

namespace {
using Star::StarException;
using Star::String;
constexpr std::uint64_t fnv_basis = 14695981039346656037ull;
constexpr std::size_t batch_size = 32;

void fingerprint_word(std::uint64_t& hash, std::uint64_t word) {
  for (unsigned i = 0; i != 8; ++i) {
    hash = (hash ^ (word & 255)) * 1099511628211ull;
    word >>= 8;
  }
}
std::uint64_t fingerprint_bytes(std::string_view bytes) {
  auto hash = fnv_basis;
  for (unsigned char byte : bytes) hash = (hash ^ byte) * 1099511628211ull;
  return hash;
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
bool require(benchmark::State& state, bool condition, char const* message) {
  if (!condition) state.SkipWithError(message);
  return condition;
}
void label(benchmark::State& state, std::string_view bytes, std::uint64_t expected) {
  state.SetLabel(std::format("input-fnv1a={:016x}; complete-result={:016x}; batch={}; destruction timed",
      fingerprint_bytes(bytes), expected, batch_size));
}
void account(benchmark::State& state, std::size_t bytes) {
  state.SetItemsProcessed(state.iterations() * batch_size);
  state.SetBytesProcessed(state.iterations() * batch_size * bytes);
}

struct fixture {
  std::string bytes;
  std::u32string points;
  std::vector<std::size_t> offsets;
};
fixture make_fixture(bool mixed, std::size_t count) {
  fixture result;
  constexpr std::array<std::string_view, 6> units{"A", "z", "é", "水", "🚀", std::string_view("\0", 1)};
  constexpr std::array<char32_t, 6> points{U'A', U'z', U'é', U'水', U'🚀', U'\0'};
  for (std::size_t i = 0; i != count; ++i) {
    auto index = mixed ? i % units.size() : i % 2;
    result.offsets.push_back(result.bytes.size());
    result.bytes += units[index];
    result.points += points[index];
  }
  result.offsets.push_back(result.bytes.size());
  return result;
}
std::uint64_t expected_walk(fixture const& input, bool reverse) {
  auto hash = fnv_basis;
  for (std::size_t n = 0; n != input.points.size(); ++n) {
    auto i = reverse ? input.points.size() - n - 1 : n;
    fingerprint_word(hash, input.points[i]);
    fingerprint_word(hash, input.offsets[i]);
    fingerprint_word(hash, input.offsets[i + 1] - input.offsets[i]);
  }
  return hash;
}
std::expected<std::uint64_t, StarException> walk(std::string_view bytes, bool reverse) {
  auto cursor = reverse ? Star::Utf8Cursor::end(bytes) : Star::Utf8Cursor(bytes);
  auto hash = fnv_basis;
  while (reverse ? cursor.offset() != 0 : !cursor.at_end()) {
    auto before = cursor.offset();
    if (reverse) {
      auto moved = cursor.retreat();
      if (!moved) return std::unexpected(std::move(moved.error()));
    }
    auto point = cursor.read();
    if (!point) return std::unexpected(std::move(point.error()));
    auto offset = cursor.offset();
    if (!reverse) {
      auto moved = cursor.advance();
      if (!moved) return std::unexpected(std::move(moved.error()));
    }
    fingerprint_word(hash, *point);
    fingerprint_word(hash, offset);
    fingerprint_word(hash, reverse ? before - offset : cursor.offset() - offset);
  }
  return hash;
}

enum class scan_operation { length, forward, reverse, linked };
template <scan_operation Operation>
void measure_scan(benchmark::State& state, bool mixed) {
  auto input = make_fixture(mixed, static_cast<std::size_t>(state.range(0)));
  std::list<char> linked(input.bytes.begin(), input.bytes.end());
  using iterator = Star::U8ToU32Iterator<std::list<char>::const_iterator>;
  auto expected = Operation == scan_operation::length ? input.points.size()
      : expected_walk(input, Operation == scan_operation::reverse);
  auto evaluate = [&]() -> std::expected<std::uint64_t, StarException> {
    if constexpr (Operation == scan_operation::length) return Star::utf8_length(std::string_view(input.bytes));
    else if constexpr (Operation == scan_operation::linked) {
      auto position = iterator::create(linked.cbegin(), linked.cend());
      if (!position) return std::unexpected(std::move(position.error()));
      auto hash = fnv_basis;
      std::size_t offset = 0;
      for (auto end = iterator::end(linked.cend()); *position != end; ++*position) {
        auto point = **position;
        auto width = point < 0x80 ? 1u : point < 0x800 ? 2u : point < 0x10000 ? 3u : 4u;
        fingerprint_word(hash, point);
        fingerprint_word(hash, offset);
        fingerprint_word(hash, width);
        offset += width;
      }
      return hash;
    } else return walk(input.bytes, Operation == scan_operation::reverse);
  };
  auto validation = evaluate();
  if (!checked(state, validation) || !require(state, *validation == expected, "complete scan differs from independent codepoints/offsets")) return;
  label(state, input.bytes, expected);
  for (auto _ : state) {
    (void)_;
    for (std::size_t i = 0; i != batch_size; ++i) {
      benchmark::ClobberMemory();
      auto result = evaluate();
      if (!checked(state, result)) return;
      benchmark::DoNotOptimize(*result);
    }
  }
  account(state, input.bytes.size());
}

void measure_malformed(benchmark::State& state, std::string const& bytes, bool reverse, char const* reason) {
  auto validation = walk(bytes, reverse);
  if (!require(state, !validation && std::string_view(validation.error().what()).contains(reason), "malformed cursor diagnosis differs")) return;
  label(state, bytes, fingerprint_bytes(reason));
  for (auto _ : state) {
    (void)_;
    for (std::size_t i = 0; i != batch_size; ++i) {
      auto result = walk(bytes, reverse);
      if (!require(state, !result, "malformed cursor unexpectedly succeeded")) return;
      benchmark::DoNotOptimize(result.error());
    }
  }
  account(state, bytes.size());
}

enum class consumer_operation { at, substr, ci_hash };
template <consumer_operation Operation>
void measure_consumer(benchmark::State& state, bool mixed) {
  auto input = make_fixture(mixed, static_cast<std::size_t>(state.range(0)));
  String text(std::string_view(input.bytes));
  auto middle = input.points.size() / 2;
  auto substring = std::string_view(input.bytes).substr(input.offsets[middle], input.offsets[middle + 5] - input.offsets[middle]);
  Star::PLHasher expected_hash;
  for (auto point : input.points) expected_hash.put(point >= U'A' && point <= U'Z' ? char32_t(point + U'a' - U'A') : point);
  auto evaluate = [&]() -> std::expected<std::uint64_t, StarException> {
    if constexpr (Operation == consumer_operation::at) {
      auto result = text.at(middle);
      if (!result) return std::unexpected(std::move(result.error()));
      return *result;
    } else if constexpr (Operation == consumer_operation::substr) {
      auto result = text.substr(middle, 5);
      if (!result) return std::unexpected(std::move(result.error()));
      // Consume real owning output, not its stack address. Destruction stays timed.
      return fingerprint_bytes(result->utf8());
    } else {
      // create() validates and computes the real codepoint hash each operation.
      auto result = Star::CaseInsensitiveStringView::create(text);
      if (!result) return std::unexpected(std::move(result.error()));
      return result->hash();
    }
  };
  auto expected = Operation == consumer_operation::at ? std::uint64_t(input.points[middle])
      : Operation == consumer_operation::substr ? fingerprint_bytes(substring) : std::uint64_t(expected_hash.hash());
  auto validation = evaluate();
  if (!checked(state, validation) || !require(state, *validation == expected, "String consumer differs from independent result")) return;
  label(state, input.bytes, expected);
  for (auto _ : state) {
    (void)_;
    for (std::size_t i = 0; i != batch_size; ++i) {
      benchmark::ClobberMemory();
      auto result = evaluate();
      if (!checked(state, result)) return;
      benchmark::DoNotOptimize(*result);
    }
  }
  account(state, input.bytes.size());
}

void measure_hex(benchmark::State& state, bool encode) {
  constexpr std::array<char32_t, 8> points{0, U'A', U'é', U'水', 0xd800, 0xdfff, U'🚀', 0x10ffff};
  constexpr std::array<std::string_view, 8> hex{"0000", "0041", "00e9", "6c34", "d800", "dfff", "d83dde80", "dbffdfff"};
  auto expected = fnv_basis;
  for (std::size_t i = 0; i != points.size(); ++i) {
    if (encode) fingerprint_word(expected, fingerprint_bytes(hex[i]));
    else fingerprint_word(expected, points[i]);
  }
  auto evaluate = [&]() -> std::expected<std::uint64_t, StarException> {
    auto hash = fnv_basis;
    for (std::size_t i = 0; i != points.size(); ++i) {
      if (encode) {
        auto result = Star::hex_string_from_utf32(points[i]);
        if (!result) return std::unexpected(std::move(result.error()));
        fingerprint_word(hash, fingerprint_bytes(*result));
      } else {
        auto first = Star::hex_string_to_utf32(hex[i].substr(0, 4));
        if (!first) return std::unexpected(std::move(first.error()));
        if (hex[i].size() == 8) {
          auto second = Star::hex_string_to_utf32(hex[i].substr(4), *first);
          if (!second) return std::unexpected(std::move(second.error()));
          first = std::move(second);
        }
        fingerprint_word(hash, *first);
      }
    }
    return hash;
  };
  auto validation = evaluate();
  if (!checked(state, validation) || !require(state, *validation == expected, "complete hex output differs from literal golden")) return;
  label(state, "0000/0041/00e9/6c34/d800/dfff/d83dde80/dbffdfff", expected);
  for (auto _ : state) {
    (void)_;
    for (std::size_t i = 0; i != batch_size; ++i) {
      benchmark::ClobberMemory();
      auto result = evaluate();
      if (!checked(state, result)) return;
      benchmark::DoNotOptimize(*result);
    }
  }
  state.SetItemsProcessed(state.iterations() * batch_size * points.size());
}

void measure_json(benchmark::State& state, unsigned kind) {
  constexpr std::array<std::string_view, 3> spelling{"\"Azé水🚀\"", "\"\\u0041\\u007a\\u00e9\\u6c34\\ud83d\\ude80\"", "\"\\ud83d\\ude80\""};
  auto value = kind == 2 ? std::string_view("🚀") : std::string_view("Azé水🚀");
  std::string bytes = "[", canonical = "[";
  Star::JsonArray values;
  for (std::int64_t i = 0; i != state.range(0); ++i) {
    if (i) { bytes += ','; canonical += ','; }
    bytes += spelling[kind];
    canonical += '"'; canonical += value; canonical += '"';
    values.push_back(Star::Json(String(value)));
  }
  bytes += ']'; canonical += ']';
  String text{std::string_view(bytes)};
  Star::Json expected(std::move(values));
  auto validation = Star::Json::parse(text);
  if (!checked(state, validation) || !require(state, *validation == expected, "JSON full semantic result differs from independent array")) return;
  auto represented = validation->repr();
  if (!checked(state, represented) || !require(state, represented->utf8() == canonical, "JSON complete repr differs from independent canonical bytes")) return;
  auto expected_fingerprint = fingerprint_bytes(canonical);
  label(state, bytes, expected_fingerprint);
  for (auto _ : state) {
    (void)_;
    for (std::size_t i = 0; i != batch_size; ++i) {
      benchmark::ClobberMemory();
      auto parsed = Star::Json::parse(text);
      if (!checked(state, parsed)) return;
      auto output = parsed->repr();
      if (!checked(state, output)) return;
      // Deliberate complete public parse/repr consumer, including both destructors.
      benchmark::DoNotOptimize(fingerprint_bytes(output->utf8()));
    }
  }
  account(state, bytes.size());
}

template <scan_operation Operation>
void register_scan(char const* name) {
  for (bool mixed : {false, true}) {
    auto label = std::format("Unicode/{}/{}", name, mixed ? "Mixed2_3_4_Nul" : "ASCII");
    auto* registered = benchmark::RegisterBenchmark(label.c_str(), [mixed](benchmark::State& state) { measure_scan<Operation>(state, mixed); });
    if constexpr (Operation == scan_operation::length)
      for (int size : {0, 1, 7, 8, 9, 15, 16, 17, 31, 32, 33, 63, 64, 65, 255, 256, 257, 4095, 4096, 4097}) registered->Arg(size);
    else registered->Arg(17)->Arg(4097);
  }
}
template <consumer_operation Operation>
void register_consumer(char const* name) {
  for (bool mixed : {false, true}) {
    auto label = std::format("Unicode/String/{}/{}", name, mixed ? "Mixed2_3_4_Nul" : "ASCII");
    benchmark::RegisterBenchmark(label.c_str(), [mixed](benchmark::State& state) { measure_consumer<Operation>(state, mixed); })->Arg(17)->Arg(4097);
  }
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
  if (!runtime) {
    (void)Star::print(stderr, "{}\n", runtime.error().what());
    return 1;
  }
  register_scan<scan_operation::length>("Length");
  register_scan<scan_operation::forward>("CursorForward");
  register_scan<scan_operation::reverse>("CursorReverse");
  register_scan<scan_operation::linked>("LinkedValidateTraverse");
  register_consumer<consumer_operation::at>("AtMiddle");
  register_consumer<consumer_operation::substr>("SubstringMiddle");
  register_consumer<consumer_operation::ci_hash>("CaseInsensitiveHashCreate");
  benchmark::RegisterBenchmark("Unicode/Hex/Parse", [](benchmark::State& state) { measure_hex(state, false); });
  benchmark::RegisterBenchmark("Unicode/Hex/Encode", [](benchmark::State& state) { measure_hex(state, true); });
  for (unsigned kind = 0; kind != 3; ++kind) {
    auto name = std::format("Unicode/Json/ParseRepr/{}", std::array{"Plain", "Escaped", "SurrogatePair"}[kind]);
    benchmark::RegisterBenchmark(name.c_str(), [kind](benchmark::State& state) { measure_json(state, kind); })->Arg(8)->Arg(128);
  }
  for (auto const& [name, bytes, reason] : std::array{
      std::tuple{"Truncated", std::string("ok\xf0\x9f", 4), "missing trailing code units"},
      std::tuple{"Overlong", std::string("ok\xc0\x80", 4), "Invalid UTF-8 code point"},
      std::tuple{"LongContinuationTail", std::string("ok") + std::string(256, '\x80'), "Invalid UTF-8 code unit sequence"}}) {
    for (bool reverse : {false, true}) {
      auto label = std::format("Unicode/Malformed/{}/{}", reverse ? "Reverse" : "Forward", name);
      auto diagnosis = reverse && std::string_view(name) == "LongContinuationTail" ? "Invalid UTF-8 boundary" : reason;
      benchmark::RegisterBenchmark(label.c_str(), [bytes, reverse, diagnosis](benchmark::State& state) { measure_malformed(state, bytes, reverse, diagnosis); });
    }
  }
  benchmark::Initialize(&argc, argv);
  if (benchmark::ReportUnrecognizedArguments(argc, argv)) { benchmark::Shutdown(); return 1; }
  checked_reporter reporter;
  auto matched = benchmark::RunSpecifiedBenchmarks(&reporter);
  benchmark::Shutdown();
  return reporter.failed || matched == 0 ? 1 : 0;
}

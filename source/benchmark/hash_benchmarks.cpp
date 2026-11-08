#include <benchmark/benchmark.h>
#define XXH_INLINE_ALL
#include "xxhash.h"

import std;
import star.array;
import star.byte_array;
import star.flat_hash_map;
import star.hash;
import star.json;
import star.list;
import star.native_runtime;
import star.static_random;
import star.string;
import star.string_view;
import star.xx_hash;

namespace {

using Star::ByteArray;
using Star::Json;
using Star::JsonArray;
using Star::JsonObject;
using Star::String;
using Star::StringView;

// Use this identical consumer for genuine old/current modules. Private baseline
// adaptations are spelling substitutions, hash_append overloads forwarding to
// the old xxHash*Push, and the owning conversion in set_contains_borrowed below.
// No implementation or compatibility interface belongs in production modules.
std::uint64_t runtime_salt = 0;
constexpr std::size_t batch_size = 64;

bool require(benchmark::State& state, bool condition, char const* message) {
  if (!condition)
    state.SkipWithError(message);
  return condition;
}

std::size_t reference_combine(std::size_t first, std::size_t second) {
  return first ^ (second * 2654435761 + 0x9e3779b9 + (first << 6) + (first >> 2));
}

std::size_t reference_pl(std::string_view bytes) {
  std::size_t result = 0;
  for (char byte : bytes)
    result = result * 101 + static_cast<std::size_t>(byte);
  return result;
}

// A real borrowed lookup in current modules. The private old-source copy uses
// set.contains(String(query)) here, explicitly charging its owning conversion.
bool set_contains_borrowed(Star::StringSet const& set, std::string_view query) { return set.contains(query); }

enum class byte_input { short_path, long_ascii, utf8, embedded_nul };
enum class byte_owner { string, view, byte_array };
enum class byte_algorithm { pl, xx32, xx64, xx3 };

std::string make_bytes(byte_input kind, std::size_t index) {
  auto result = std::format("/items/materials/{:03}/{}", index, runtime_salt % 97);
  if (kind == byte_input::long_ascii)
    result.append(4096 + index % 17, static_cast<char>('a' + index % 26));
  else if (kind == byte_input::utf8)
    for (std::size_t i = 0; i != 40 + index % 7; ++i)
      result += "\xc3\xa9\xe4\xb8\x96\xf0\x9f\x8c\x8d";
  else if (kind == byte_input::embedded_nul) {
    result.push_back('\0');
    result.append(80 + index % 7, static_cast<char>('A' + index % 26));
    result += "/after-nul";
  }
  return result;
}

template <byte_algorithm Algorithm> std::uint64_t reference_byte_hash(std::string_view bytes) {
  if constexpr (Algorithm == byte_algorithm::pl) {
    return reference_pl(bytes);
  } else {
    // Independent incremental input segmentation, not the measured one-shot
    // wrapper, checks all bytes including UTF-8 continuation bytes and NULs.
    using Hasher =
      std::conditional_t<Algorithm == byte_algorithm::xx32, Star::XXHash32,
                         std::conditional_t<Algorithm == byte_algorithm::xx64, Star::XXHash64, Star::XXHash3>>;
    Hasher hasher;
    for (char const& byte : bytes)
      hasher.push(std::span<char const>(&byte, 1));
    return hasher.digest();
  }
}

template <byte_algorithm Algorithm, byte_owner Owner> void byte_hash(benchmark::State& state, byte_input kind) {
  std::vector<std::string> input;
  std::vector<String> strings;
  std::vector<ByteArray> arrays;
  std::uint64_t byte_count = 0;
  for (std::size_t i = 0; i != batch_size; ++i) {
    input.push_back(make_bytes(kind, i));
    byte_count += input.back().size();
    strings.emplace_back(input.back());
    arrays.emplace_back(std::span<char const>(input.back()));
  }
  auto hash_one = [&](std::size_t i) -> std::uint64_t {
    if constexpr (Algorithm == byte_algorithm::pl) {
      if constexpr (Owner == byte_owner::string)
        return Star::hash<String>{}(strings[i]);
      else if constexpr (Owner == byte_owner::view)
        return Star::hash<String>{}(StringView(std::string_view(input[i])));
      else
        return Star::hash<ByteArray>{}(arrays[i]);
    } else {
      auto const& value = [&]() -> auto const& {
        if constexpr (Owner == byte_owner::string)
          return strings[i];
        else if constexpr (Owner == byte_owner::byte_array)
          return arrays[i];
        else
          return input[i];
      }();
      if constexpr (Owner == byte_owner::view) {
        auto bytes = std::span<char const>(value);
        if constexpr (Algorithm == byte_algorithm::xx32)
          return Star::xx_hash32(bytes);
        else if constexpr (Algorithm == byte_algorithm::xx64)
          return Star::xx_hash64(bytes);
        else
          return Star::xx_hash3(bytes);
      } else if constexpr (Algorithm == byte_algorithm::xx32) {
        return Star::xx_hash32(value);
      } else if constexpr (Algorithm == byte_algorithm::xx64) {
        return Star::xx_hash64(value);
      } else {
        return Star::xx_hash3(value);
      }
    }
  };
  for (std::size_t i = 0; i != batch_size; ++i)
    if (!require(state, hash_one(i) == reference_byte_hash<Algorithm>(input[i]),
                 "byte hash differs from independent byte expectation"))
      return;
  state.SetLabel("64 runtime inputs; hash only; owners/destruction untimed; bytes include NULs");
  for (auto _ : state) {
    (void)_;
    benchmark::ClobberMemory();
    for (std::size_t i = 0; i != batch_size; ++i) {
      auto output = hash_one(i);
      benchmark::DoNotOptimize(output);
    }
  }
  state.SetItemsProcessed(state.iterations() * batch_size);
  state.SetBytesProcessed(state.iterations() * static_cast<std::int64_t>(byte_count));
}

using string_map = Star::FlatHashMap<String, std::uint64_t>;
enum class map_operation { insert_grow, insert_reserved, duplicate, reserve_grow };

std::vector<String> map_keys(std::size_t size) {
  std::vector<String> keys;
  for (std::size_t i = 0; i != size; ++i)
    keys.emplace_back(std::format("/objects/benchmark/{:05}/{}-owned-string-key", i, runtime_salt % 97));
  return keys;
}

bool validate_map(benchmark::State& state, string_map const& map, std::vector<String> const& keys) {
  if (!require(state, map.size() == keys.size(), "map size differs from independent key fixture"))
    return false;
  for (std::size_t i = 0; i != keys.size(); ++i) {
    auto found = map.find(keys[i]);
    if (!require(state, found != map.end() && found->second == i + 11, "map lost key or changed mapped value"))
      return false;
  }
  return true;
}

template <map_operation Operation> void flat_map(benchmark::State& state) {
  auto count = static_cast<std::size_t>(state.range(0));
  auto keys = map_keys(count);
  auto fill = [&](string_map& map) {
    std::size_t inserted = 0;
    for (std::size_t i = 0; i != count; ++i)
      inserted += map.try_emplace(keys[i], i + 11).second;
    return inserted;
  };
  string_map fixture;
  if (!require(state, fill(fixture) == count, "new map insertion did not insert every key")
      || !validate_map(state, fixture, keys))
    return;
  if constexpr (Operation == map_operation::duplicate) {
    if (!require(state, fill(fixture) == 0, "duplicate map insertion inserted an existing key")
        || !validate_map(state, fixture, keys))
      return;
    state.SetLabel("duplicate try_emplace only; resident map and key owners untimed; mapped values remain unchanged");
    for (auto _ : state) {
      (void)_;
      benchmark::ClobberMemory();
      auto inserted = fill(fixture);
      benchmark::DoNotOptimize(inserted);
      benchmark::ClobberMemory();
    }
    if (!validate_map(state, fixture, keys))
      return;
  } else {
    auto factory = [&] {
      if constexpr (Operation == map_operation::reserve_grow) {
        auto result = fixture;
        result.reserve(count * 4);
        return result;
      } else {
        string_map result;
        if constexpr (Operation == map_operation::insert_reserved)
          result.reserve(count);
        auto inserted = fill(result);
        benchmark::DoNotOptimize(inserted);
        return result;
      }
    };
    {
      auto result = factory();
      if (!validate_map(state, result, keys))
        return;
    }
    state.SetLabel(Operation == map_operation::reserve_grow
                     ? "copy populated map + reserve 4N + full owning destruction timed"
                     : "empty construction + all insertions + optional reserve + full owning destruction timed");
    for (auto _ : state) {
      (void)_;
      benchmark::ClobberMemory();
      auto output = factory();
      benchmark::DoNotOptimize(output);
      benchmark::ClobberMemory();
    }
  }
  state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(count));
}

void string_set_lookup(benchmark::State& state) {
  auto keys = map_keys(static_cast<std::size_t>(state.range(0)));
  Star::StringSet set;
  for (auto const& key : keys)
    set.insert(key);
  std::vector<std::string> queries;
  for (std::size_t i = 0; i != keys.size(); ++i) {
    queries.push_back(keys[i].utf8());
    queries.push_back(std::format("/objects/benchmark/missing/{}/{}", i, runtime_salt));
  }
  for (std::size_t i = 0; i != queries.size(); ++i)
    if (!require(state, set_contains_borrowed(set, queries[i]) == (i % 2 == 0),
                 "StringSet borrowed hit/miss differs from fixture"))
      return;
  state.SetLabel("alternating hit/miss; lookup only; baseline private adapter charges owning conversion/destruction");
  for (auto _ : state) {
    (void)_;
    benchmark::ClobberMemory();
    for (auto const& query : queries) {
      auto output = set_contains_borrowed(set, query);
      benchmark::DoNotOptimize(output);
    }
  }
  state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(queries.size()));
}

// JSON's pre-existing hash uses native scalar bytes, not the canonical integer
// encoding of static_random. Build a separate, known fixture stream without
// traversing Json or calling its hash/getHash implementation.
template <typename T> void append_native(std::vector<char>& bytes, T value) {
  auto representation = std::bit_cast<std::array<char, sizeof(T)>>(value);
  bytes.insert(bytes.end(), representation.begin(), representation.end());
}

struct json_fixture {
  Json value;
  std::vector<char> expected;
};

json_fixture make_json(std::size_t count, bool nested, bool reverse) {
  JsonObject object;
  std::vector<char> expected{static_cast<char>(Json::Type::Object)};
  // Zero-padded keys ensure this independent construction order is lexical.
  for (std::size_t i = 0; i != count; ++i) {
    auto key = std::format("key-{:03}-{}", i, runtime_salt % 97);
    expected.insert(expected.end(), key.begin(), key.end());
    if (nested) {
      expected.push_back(static_cast<char>(Json::Type::Object));
      expected.push_back('a');
      expected.push_back(static_cast<char>(Json::Type::Array));
    }
    expected.push_back(static_cast<char>(Json::Type::Int));
    append_native(expected, static_cast<std::int64_t>(i * 13 + runtime_salt % 101));
    if (nested) {
      expected.push_back(static_cast<char>(Json::Type::String));
      auto text = make_bytes(byte_input::embedded_nul, i);
      expected.insert(expected.end(), text.begin(), text.end());
      expected.push_back(static_cast<char>(Json::Type::Bool));
      expected.push_back(static_cast<char>(i % 2));
    }
  }
  for (std::size_t position = 0; position != count; ++position) {
    auto i = reverse ? count - position - 1 : position;
    auto key = String(std::format("key-{:03}-{}", i, runtime_salt % 97));
    Json value(static_cast<std::int64_t>(i * 13 + runtime_salt % 101));
    if (nested) {
      JsonArray array;
      array.push_back(std::move(value));
      array.push_back(Json(make_bytes(byte_input::embedded_nul, i)));
      array.push_back(Json(i % 2 != 0));
      JsonObject inner;
      inner.insert({String("a"), Json(std::move(array))});
      value = Json(std::move(inner));
    }
    object.insert({std::move(key), std::move(value)});
  }
  return {Json(std::move(object)), std::move(expected)};
}

void json_hash(benchmark::State& state, bool nested, bool reverse) {
  auto count = static_cast<std::size_t>(state.range(0));
  auto input = make_json(count, nested, reverse);
  auto permutation = make_json(count, nested, !reverse);
  auto expected = Star::xx_hash3(std::span<char const>(input.expected));
  if (!require(state, input.value == permutation.value && input.expected == permutation.expected,
               "independently built JSON order permutations differ")
      || !require(state,
                  Star::hash<Json>{}(input.value) == expected && Star::hash<Json>{}(permutation.value) == expected,
                  "JSON hash differs from independent sorted-key byte stream"))
    return;
  // Equality-compatible numeric normalization remains a separate untimed check.
  if (!require(state,
               Star::hash<Json>{}(Json(-0.0)) == Star::hash<Json>{}(Json(std::int64_t(0)))
                 && Star::hash<Json>{}(Json(42.0)) == Star::hash<Json>{}(Json(std::int64_t(42))),
               "JSON integral-double/signed-zero hash contract changed"))
    return;
  state.SetLabel("one hash per iteration; scratch allocations/destruction timed; immutable JSON lifetime untimed");
  for (auto _ : state) {
    (void)_;
    benchmark::ClobberMemory();
    benchmark::DoNotOptimize(input.value);
    auto output = Star::hash<Json>{}(input.value);
    benchmark::DoNotOptimize(output);
  }
  state.SetItemsProcessed(state.iterations());
  state.counters["members_per_hash"] = static_cast<double>(count);
}

enum class composite_kind { primitive, hash_of, pair, tuple, optional, array, list };

template <composite_kind Kind> void composite_hash(benchmark::State& state) {
  std::array<std::array<std::int64_t, 4>, batch_size> values;
  std::vector<Star::Array<std::int64_t, 4>> arrays;
  std::vector<Star::List<std::int64_t>> lists;
  for (std::size_t i = 0; i != batch_size; ++i) {
    values[i] = {static_cast<std::int64_t>(runtime_salt + i * 31), -static_cast<std::int64_t>(i + 7),
                 static_cast<std::int64_t>(i * 997), static_cast<std::int64_t>(runtime_salt ^ (i + 19))};
    arrays.emplace_back(values[i][0], values[i][1], values[i][2], values[i][3]);
    lists.push_back({values[i][0], values[i][1], values[i][2], values[i][3]});
  }
  auto hash_one = [&](std::size_t i) {
    auto const& v = values[i];
    if constexpr (Kind == composite_kind::primitive)
      return Star::hash_of(v[0]);
    else if constexpr (Kind == composite_kind::hash_of)
      return Star::hash_of(v[0], v[1], v[2], v[3]);
    else if constexpr (Kind == composite_kind::pair)
      return Star::hash_of(std::pair(v[0], v[1]));
    else if constexpr (Kind == composite_kind::tuple)
      return Star::hash_of(std::tuple(v[0], v[1], v[2], v[3]));
    else if constexpr (Kind == composite_kind::optional)
      return Star::hash_of(i % 2 ? std::optional<std::int64_t>(v[0]) : std::nullopt);
    else if constexpr (Kind == composite_kind::array)
      return Star::hash_of(arrays[i]);
    else
      return Star::hash_of(lists[i]);
  };
  for (std::size_t i = 0; i != batch_size; ++i) {
    std::array<std::size_t, 4> h;
    for (std::size_t j = 0; j != 4; ++j)
      h[j] = std::hash<std::int64_t>{}(values[i][j]);
    auto expected = h[0];
    if constexpr (Kind == composite_kind::hash_of)
      expected = reference_combine(h[0], reference_combine(h[1], reference_combine(h[2], h[3])));
    else if constexpr (Kind == composite_kind::pair)
      expected = reference_combine(h[0], h[1]);
    else if constexpr (Kind == composite_kind::tuple)
      expected = reference_combine(h[0], reference_combine(h[1], reference_combine(h[2], reference_combine(h[3], 0))));
    else if constexpr (Kind == composite_kind::optional)
      expected = i % 2 ? h[0] : 0;
    else if constexpr (Kind == composite_kind::array || Kind == composite_kind::list) {
      expected = 0;
      for (auto element : h)
        expected = reference_combine(expected, element);
    }
    if (!require(state, hash_one(i) == expected,
                 "composite hash differs from independent association/fold expectation"))
      return;
  }
  state.SetLabel("64 runtime inputs; primitive/composite hash only; fixture lifetimes untimed");
  for (auto _ : state) {
    (void)_;
    benchmark::ClobberMemory();
    for (std::size_t i = 0; i != batch_size; ++i) {
      auto output = hash_one(i);
      benchmark::DoNotOptimize(output);
    }
  }
  state.SetItemsProcessed(state.iterations() * batch_size);
}

struct coordinate {
  std::int32_t x;
  std::int32_t y;
  std::int64_t world;
  char label[24]{};
};

std::array<coordinate, batch_size> coordinates() {
  std::array<coordinate, batch_size> result{};
  for (std::size_t i = 0; i != result.size(); ++i) {
    auto& value = result[i];
    value.x = static_cast<std::int32_t>(i * 117 + runtime_salt % 997);
    value.y = -static_cast<std::int32_t>(i * 731 + 1);
    value.world = static_cast<std::int64_t>((runtime_salt << 32) ^ (i * 12345678901ULL));
    auto length = std::array<std::size_t, 4>{0, 1, 7, 23}[i % 4];
    for (std::size_t j = 0; j != length; ++j)
      value.label[j] = static_cast<char>('a' + (j + i) % 26);
    if (i % 8 == 7)
      value.label[3] = '\0';// char-array append stops at its first NUL.
  }
  result[0].x = std::numeric_limits<std::int32_t>::min();
  result[0].y = std::numeric_limits<std::int32_t>::max();
  result[0].world = std::numeric_limits<std::int64_t>::min();
  result[1].world = std::numeric_limits<std::int64_t>::max();
  return result;
}

// Private candidate only: explicit int32/int32/int64 little-endian bytes,
// followed by the char-array label prefix. No padding, terminator or host ABI.
template <std::integral Integer>
  requires(!std::same_as<Integer, bool>)
void pack_little(std::array<char, 40>& bytes, std::size_t& size, Integer value) {
  using Unsigned = std::make_unsigned_t<Integer>;
  auto bits = static_cast<Unsigned>(value);
  for (std::size_t i = 0; i != sizeof(Integer); ++i)
    bytes[size++] = static_cast<char>((bits >> (i * 8)) & 0xffu);
}

std::size_t pack_coordinate(std::array<char, 40>& bytes, coordinate const& value) {
  std::size_t size = 0;
  pack_little(bytes, size, value.x);
  pack_little(bytes, size, value.y);
  pack_little(bytes, size, value.world);
  for (std::size_t i = 0; i != sizeof(value.label) && value.label[i] != '\0'; ++i)
    bytes[size++] = value.label[i];
  return size;
}

// Deliberately independent reference encoder: native bit-cast + endian reversal
// rather than candidate shifts. Used only before timing for byte-for-byte proof.
template <std::integral Integer>
  requires(!std::same_as<Integer, bool>)
void reference_little(std::vector<char>& bytes, Integer value) {
  auto representation = std::bit_cast<std::array<char, sizeof(Integer)>>(value);
  if constexpr (std::endian::native == std::endian::big)
    std::reverse(representation.begin(), representation.end());
  bytes.insert(bytes.end(), representation.begin(), representation.end());
}

std::vector<char> reference_coordinate(coordinate const& value) {
  std::vector<char> bytes;
  reference_little(bytes, value.x);
  reference_little(bytes, value.y);
  reference_little(bytes, value.world);
  auto label = std::string_view(value.label, sizeof(value.label));
  label = label.substr(0, label.find('\0'));
  bytes.insert(bytes.end(), label.begin(), label.end());
  return bytes;
}

template <bool Wide, bool Candidate> std::uint64_t deterministic_hash(coordinate const& value) {
  if constexpr (Candidate) {
    std::array<char, 40> bytes;
    auto size = pack_coordinate(bytes, value);
    if constexpr (Wide)
      return XXH64(bytes.data(), size, 1997293021376312589ULL);
    else
      return XXH32(bytes.data(), size, 2938728349u);
  } else if constexpr (Wide) {
    return Star::static_random_hash64(value.x, value.y, value.world, value.label);
  } else {
    return Star::static_random_hash32(value.x, value.y, value.world, value.label);
  }
}

template <bool Wide, bool Candidate> void deterministic_short(benchmark::State& state) {
  auto input = coordinates();
  std::uint64_t byte_count = 0;
  for (auto const& value : input) {
    auto reference = reference_coordinate(value);
    std::array<char, 40> packed;
    auto size = pack_coordinate(packed, value);
    byte_count += size;
    if (!require(state, size == reference.size() && std::equal(reference.begin(), reference.end(), packed.begin()),
                 "private stack candidate differs from canonical coordinate/label bytes"))
      return;
    auto expected = [&]() -> std::uint64_t {
      if constexpr (Wide) {
        Star::XXHash64 hash(1997293021376312589ULL);
        hash.push(reference);
        return hash.digest();
      } else {
        Star::XXHash32 hash(2938728349u);
        hash.push(reference);
        return hash.digest();
      }
    }();
    if (!require(state,
                 deterministic_hash<Wide, false>(value) == expected
                   && deterministic_hash<Wide, true>(value) == expected,
                 "streaming/static_random/stack candidate digest equivalence failed"))
      return;
    // Also cover the new append interface independently of static_random.
    if constexpr (Wide) {
      Star::XXHash64 hash(1997293021376312589ULL);
      Star::hash_append(hash, value.x);
      Star::hash_append(hash, value.y);
      Star::hash_append(hash, value.world);
      Star::hash_append(hash, value.label);
      if (!require(state, hash.digest() == expected, "hash_append64 byte contract differs"))
        return;
    } else {
      Star::XXHash32 hash(2938728349u);
      Star::hash_append(hash, value.x);
      Star::hash_append(hash, value.y);
      Star::hash_append(hash, value.world);
      Star::hash_append(hash, value.label);
      if (!require(state, hash.digest() == expected, "hash_append32 byte contract differs"))
        return;
    }
  }
  state.SetLabel(
    "64 coordinate+label hashes; canonical LE int32/int32/int64; seeds and complete bytes matched untimed");
  for (auto _ : state) {
    (void)_;
    benchmark::ClobberMemory();
    for (auto const& value : input) {
      auto output = deterministic_hash<Wide, Candidate>(value);
      benchmark::DoNotOptimize(output);
    }
  }
  state.SetItemsProcessed(state.iterations() * batch_size);
  state.SetBytesProcessed(state.iterations() * static_cast<std::int64_t>(byte_count));
}

template <byte_algorithm Algorithm, byte_owner Owner> void register_bytes(char const* algorithm, char const* owner) {
  for (auto [kind, name] :
       {std::pair(byte_input::short_path, "ShortAssetPath"), std::pair(byte_input::long_ascii, "LongASCII"),
        std::pair(byte_input::utf8, "UTF8"), std::pair(byte_input::embedded_nul, "EmbeddedNUL")}) {
    auto name_string = std::format("Hash/Bytes/{}/{}/{}/HashOnly", algorithm, owner, name);
    benchmark::RegisterBenchmark(name_string.c_str(),
                                 [kind](benchmark::State& state) { byte_hash<Algorithm, Owner>(state, kind); });
  }
}

template <byte_algorithm Algorithm> void register_byte_family(char const* algorithm) {
  register_bytes<Algorithm, byte_owner::string>(algorithm, "String");
  register_bytes<Algorithm, byte_owner::view>(algorithm, "View");
  register_bytes<Algorithm, byte_owner::byte_array>(algorithm, "ByteArray");
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
  auto native_runtime = Star::NativeRuntime::create();
  if (!native_runtime) {
    std::cerr << native_runtime.error().what() << '\n';
    return 1;
  }
  // Same deterministic runtime salt in before/after even if binary paths differ.
  // Its value crosses a compiler barrier before any fixture construction.
  runtime_salt = static_cast<std::uint64_t>(argc + 73);
  benchmark::DoNotOptimize(runtime_salt);
  register_byte_family<byte_algorithm::pl>("PL");
  register_byte_family<byte_algorithm::xx32>("XXH32");
  register_byte_family<byte_algorithm::xx64>("XXH64");
  register_byte_family<byte_algorithm::xx3>("XXH3");
  benchmark::RegisterBenchmark("Hash/FlatMap/NewInsertGrowConstructDestroy", &flat_map<map_operation::insert_grow>)
    ->Arg(64)
    ->Arg(1024);
  benchmark::RegisterBenchmark("Hash/FlatMap/NewInsertReservedConstructDestroy",
                               &flat_map<map_operation::insert_reserved>)
    ->Arg(64)
    ->Arg(1024);
  benchmark::RegisterBenchmark("Hash/FlatMap/DuplicateInsertOnly", &flat_map<map_operation::duplicate>)
    ->Arg(64)
    ->Arg(1024);
  benchmark::RegisterBenchmark("Hash/FlatMap/CopyReserveGrowDestroy", &flat_map<map_operation::reserve_grow>)
    ->Arg(64)
    ->Arg(1024);
  benchmark::RegisterBenchmark("Hash/StringSet/BorrowedLookupHitMiss", &string_set_lookup)->Arg(64)->Arg(1024);
  for (auto [nested, reverse, name] :
       {std::tuple(false, false, "Object"), std::tuple(false, true, "ObjectOrderPermuted"),
        std::tuple(true, false, "Nested"), std::tuple(true, true, "NestedOrderPermuted")}) {
    auto name_string = std::format("Hash/Json/{}/HashOnly", name);
    benchmark::RegisterBenchmark(name_string.c_str(),
                                 [nested, reverse](benchmark::State& state) { json_hash(state, nested, reverse); })
      ->Arg(0)
      ->Arg(1)
      ->Arg(8)
      ->Arg(64);
  }
  benchmark::RegisterBenchmark("Hash/Composite/Int64/HashOnly", &composite_hash<composite_kind::primitive>);
  benchmark::RegisterBenchmark("Hash/Composite/HashOf4/HashOnly", &composite_hash<composite_kind::hash_of>);
  benchmark::RegisterBenchmark("Hash/Composite/Pair2/HashOnly", &composite_hash<composite_kind::pair>);
  benchmark::RegisterBenchmark("Hash/Composite/Tuple4TerminalZero/HashOnly", &composite_hash<composite_kind::tuple>);
  benchmark::RegisterBenchmark("Hash/Composite/OptionalInt64/HashOnly", &composite_hash<composite_kind::optional>);
  benchmark::RegisterBenchmark("Hash/Composite/Array4LeftFoldZero/HashOnly", &composite_hash<composite_kind::array>);
  benchmark::RegisterBenchmark("Hash/Composite/List4LeftFoldZero/HashOnly", &composite_hash<composite_kind::list>);
  benchmark::RegisterBenchmark("Hash/StaticRandom/Hash32/CoordinateLabelStreaming", &deterministic_short<false, false>);
  benchmark::RegisterBenchmark("Hash/StaticRandom/Hash64/CoordinateLabelStreaming", &deterministic_short<true, false>);
  benchmark::RegisterBenchmark("Hash/StaticRandom/PrivateCandidate/Hash32/CoordinateLabelStackOneShot",
                               &deterministic_short<false, true>);
  benchmark::RegisterBenchmark("Hash/StaticRandom/PrivateCandidate/Hash64/CoordinateLabelStackOneShot",
                               &deterministic_short<true, true>);
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

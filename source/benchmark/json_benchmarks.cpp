#include <benchmark/benchmark.h>

import std;
import star.format;
import star.formatted_json;
import star.json;
import star.json_patch;
import star.json_path;
import star.list;
import star.map;
import star.native_runtime;
import star.string;

namespace {

using Star::FormattedJson;
using Star::Json;
using Star::JsonArray;
using Star::JsonObject;
using Star::String;

// The same source and deterministic fixtures are used against both production
// versions. Fixed complete batches bound the old flat FormattedJson builder's
// quadratic work: calibration must not repeatedly reconstruct 4096-entry trees.
// Setup and independent semantic checks precede State iteration. Each timed
// operation includes its output (where named) and destruction of all results.
template <typename T> bool checked(benchmark::State& state, std::expected<T, Star::StarException> const& result) {
  if (result)
    return true;
  std::string message(result.error().what());
  state.SkipWithError(message.c_str());
  return false;
}

bool require(benchmark::State& state, bool condition, char const* message) {
  if (!condition)
    state.SkipWithError(message);
  return condition;
}

void label(benchmark::State& state, String const& input, std::size_t batch) {
  // Byte FNV-1a is intentionally unrelated to Json's numeric equality/hash fix.
  std::uint64_t fingerprint = 14695981039346656037ull;
  for (unsigned char byte : input.utf8()) {
    fingerprint ^= byte;
    fingerprint *= 1099511628211ull;
  }
  state.SetLabel(std::format("fixture-fnv1a={:016x}; bytes={}; batch={}; includes destruction", fingerprint,
                             input.utf8Size(), batch));
}

struct Fixture {
  String text;
  Json expected;
};

enum class ParseKind { Numbers, Unicode, Comments };

template <ParseKind Kind> Fixture parseFixture(std::size_t count) {
  std::string text = "[";
  JsonArray expected;
  expected.reserve(count);
  for (std::size_t i = 0; i < count; ++i) {
    if (i)
      text += ',';
    if constexpr (Kind == ParseKind::Numbers) {
      constexpr std::array<std::string_view, 6> spelling{"0", "-17", "1.25", "6.25e2", "-0.0", "9223372036854775807"};
      std::array<Json, 6> values{Json(0),     Json(-17),  Json(1.25),
                                 Json(625.0), Json(-0.0), Json(static_cast<std::int64_t>(9223372036854775807ll))};
      text += spelling[i % spelling.size()];
      expected.append(values[i % values.size()]);
    } else if constexpr (Kind == ParseKind::Unicode) {
      constexpr std::array<std::string_view, 3> spelling{"\"日本語🌟\"", "\"caf\\u00e9\\n\\t\\\"\\\\\"",
                                                         "\"\\ud83d\\ude80\""};
      std::array<Json, 3> values{Json(String("日本語🌟")), Json(String("café\n\t\"\\")), Json(String("🚀"))};
      text += spelling[i % spelling.size()];
      expected.append(values[i % values.size()]);
    } else {
      text += " /* config 注釈 */ " + std::to_string(i) + " // setting\n";
      expected.append(Json(i));
    }
  }
  text += ']';
  return {String(std::move(text)), Json(std::move(expected))};
}

template <ParseKind Kind> void BM_ParseDestroy(benchmark::State& state) {
  auto fixture = parseFixture<Kind>(static_cast<std::size_t>(state.range(0)));
  auto validation = Json::parse(fixture.text);
  if (!checked(state, validation)
      || !require(state, *validation == fixture.expected, "parse semantic output differs from independent values"))
    return;
  // Numeric conversion equality alone would miss a lost floating-point tag.
  if constexpr (Kind == ParseKind::Numbers) {
    auto values = validation->arrayPtr();
    if (!checked(state, values))
      return;
    for (std::size_t i = 0; i < (*values)->size(); ++i) {
      auto expectedType = i % 6 >= 2 && i % 6 <= 4 ? Json::Type::Float : Json::Type::Int;
      if (!require(state, (**values)[i].type() == expectedType, "parsed numeric type tag changed"))
        return;
    }
  }
  constexpr std::size_t Batch = 64;
  label(state, fixture.text, Batch);
  for (auto _ : state) {
    (void)_;
    for (std::size_t i = 0; i < Batch; ++i) {
      auto result = Json::parse(fixture.text);
      if (!checked(state, result))
        return;
      benchmark::DoNotOptimize(*result);
    }
  }
  state.SetItemsProcessed(state.iterations() * Batch);
  state.SetBytesProcessed(state.iterations() * Batch * fixture.text.utf8Size());
}

// Padded keys make independently generated compact object text lexically sorted.
// Noncanonical spelling and comments are only used by the preservation fixture.
Fixture flatFixture(bool object, std::size_t count, bool preserve) {
  std::string text = object ? "{" : "[";
  JsonArray array;
  JsonObject entries;
  array.reserve(count);
  entries.reserve(count);
  for (std::size_t i = 0; i < count; ++i) {
    if (i)
      text += preserve ? ", /* retained */ " : ",";
    auto key = std::format("k{:06}", i);
    if (object)
      text += '"' + key + (preserve ? "\" : " : "\":");
    Json value;
    if (preserve) {
      constexpr std::array<std::string_view, 3> spellings{"-0", "1.00", "6.25e2"};
      text += spellings[i % 3];
      value = i % 3 == 0 ? Json(0) : i % 3 == 1 ? Json(1.0) : Json(625.0);
    } else {
      text += std::to_string(i);
      value = Json(i);
    }
    if (object)
      entries[String(key)] = std::move(value);
    else
      array.append(std::move(value));
  }
  text += object ? '}' : ']';
  return {String(std::move(text)), object ? Json(std::move(entries)) : Json(std::move(array))};
}

bool validateFormatted(benchmark::State& state, FormattedJson const& value, Fixture const& fixture, bool preserved) {
  if (!require(state, value.toJson() == fixture.expected, "formatted semantic output differs from independent values"))
    return false;
  auto output = value.repr();
  if (!checked(state, output))
    return false;
  if (preserved)
    return require(state, *output == fixture.text, "formatted output lost comments, spacing, or numeric spelling");
  // create() is free to use the production object's iteration order. Compare a
  // sorted semantic rendering with independently generated lexically sorted text.
  auto reparsed = Json::parse(*output);
  if (!checked(state, reparsed))
    return false;
  auto canonical = reparsed->repr(0, true);
  return checked(state, canonical)
    && require(state, *canonical == fixture.text, "formatted create/serialize produced unexpected values");
}

template <bool Object, bool Create> void BM_FormattedBuildSerializeDestroy(benchmark::State& state) {
  auto fixture = flatFixture(Object, static_cast<std::size_t>(state.range(0)), !Create);
  auto snapshot = fixture.expected;
  auto validation = Create ? FormattedJson::create(fixture.expected) : FormattedJson::parse(fixture.text);
  if (!checked(state, validation) || !validateFormatted(state, *validation, fixture, !Create))
    return;
  label(state, fixture.text, 1);
  for (auto _ : state) {
    (void)_;
    auto result = Create ? FormattedJson::create(fixture.expected) : FormattedJson::parse(fixture.text);
    if (!checked(state, result))
      return;
    auto output = result->repr();
    if (!checked(state, output))
      return;
    benchmark::DoNotOptimize(*output);
  }
  require(state, fixture.expected == snapshot, "formatted operation changed its old Json snapshot");
  state.SetItemsProcessed(state.iterations());
}

template <bool Object> void BM_FormattedSerializeDestroy(benchmark::State& state) {
  auto fixture = flatFixture(Object, 1024, true);
  auto formatted = FormattedJson::parse(fixture.text);
  if (!checked(state, formatted) || !validateFormatted(state, *formatted, fixture, true))
    return;
  label(state, fixture.text, 16);
  for (auto _ : state) {
    (void)_;
    for (std::size_t i = 0; i < 16; ++i) {
      auto output = formatted->repr();
      if (!checked(state, output))
        return;
      benchmark::DoNotOptimize(*output);
    }
  }
  state.SetItemsProcessed(state.iterations() * 16);
}

Json pathDocument(std::size_t depth, std::size_t width, int value) {
  Json document = JsonObject{{"value", value}, {"keep", 11}};
  for (std::size_t level = 0; level < depth; ++level) {
    JsonObject object;
    object.reserve(width + 1);
    for (std::size_t i = 0; i < width; ++i)
      object[String(std::format("sibling{:06}", i))] = Json(i);
    object["next"] = std::move(document);
    document = Json(std::move(object));
  }
  return document;
}

void BM_PathSetSerializeDestroy(benchmark::State& state) {
  auto depth = static_cast<std::size_t>(state.range(0));
  auto width = static_cast<std::size_t>(state.range(1));
  auto base = pathDocument(depth, width, 7);
  auto oldSnapshot = base;
  auto expected = pathDocument(depth, width, 99);
  std::string path;
  for (std::size_t i = 0; i < depth; ++i)
    path += "/next";
  path += "/value";
  Star::JsonPath::Pointer pointer{String(path)};
  Json replacement(99);
  auto oldOutput = base.repr(0, true);
  auto expectedOutput = expected.repr(0, true);
  auto validation = pointer.set(base, replacement);
  if (!checked(state, oldOutput) || !checked(state, expectedOutput) || !checked(state, validation)
      || !require(state, *validation == expected, "path set changed the wrong subtree"))
    return;
  auto validationOutput = validation->repr(0, true);
  auto snapshotOutput = oldSnapshot.repr(0, true);
  auto baseOutput = base.repr(0, true);
  if (!checked(state, validationOutput) || !checked(state, snapshotOutput) || !checked(state, baseOutput)
      || !require(state,
                  *validationOutput == *expectedOutput && *snapshotOutput == *oldOutput && *baseOutput == *oldOutput,
                  "path set output or immutable old snapshot differs"))
    return;
  label(state, *oldOutput, 16);
  for (auto _ : state) {
    (void)_;
    for (std::size_t i = 0; i < 16; ++i) {
      auto result = pointer.set(base, replacement);
      if (!checked(state, result))
        return;
      auto output = result->repr(0, true);
      if (!checked(state, output))
        return;
      benchmark::DoNotOptimize(*output);
    }
  }
  state.SetItemsProcessed(state.iterations() * 16);
}

Json searchDocument(std::size_t count, bool changed) {
  JsonArray rows;
  rows.reserve(count);
  for (std::size_t i = 0; i < count; ++i) {
    bool target = changed && i + 1 == count;
    rows.append(JsonObject{{"id", Json(i)},
                           {"config", JsonObject{{"enabled", target}, {"keep", 23}, {"level", target ? 9 : 1}}}});
  }
  return JsonObject{{"rows", std::move(rows)}, {"untouched", "snapshot"}};
}

template <bool Hit> void BM_PatchSearchMergeSerializeDestroy(benchmark::State& state) {
  auto count = static_cast<std::size_t>(state.range(0));
  auto base = searchDocument(count, false);
  auto oldSnapshot = base;
  auto expected = searchDocument(count, Hit);
  JsonArray patch{JsonObject{{"op", "merge"},
                             {"path", "/rows"},
                             {"search", JsonObject{{"id", Json(Hit ? count - 1 : count + 1)}}},
                             {"value", JsonObject{{"config", JsonObject{{"enabled", true}, {"level", 9}}}}}}};
  auto oldOutput = base.repr(0, true);
  auto expectedOutput = expected.repr(0, true);
  auto validation = Star::jsonPatch(base, patch);
  if (!checked(state, oldOutput) || !checked(state, expectedOutput) || !checked(state, validation)
      || !require(state, *validation == expected, "patch search/merge differs from independent expected rows"))
    return;
  auto output = validation->repr(0, true);
  auto snapshotOutput = oldSnapshot.repr(0, true);
  auto baseOutput = base.repr(0, true);
  if (!checked(state, output) || !checked(state, snapshotOutput) || !checked(state, baseOutput)
      || !require(state, *output == *expectedOutput && *snapshotOutput == *oldOutput && *baseOutput == *oldOutput,
                  "patch search/merge output or old snapshot changed"))
    return;
  label(state, *oldOutput, 8);
  for (auto _ : state) {
    (void)_;
    for (std::size_t i = 0; i < 8; ++i) {
      auto result = Star::jsonPatch(base, patch);
      if (!checked(state, result))
        return;
      auto text = result->repr(0, true);
      if (!checked(state, text))
        return;
      benchmark::DoNotOptimize(*text);
    }
  }
  state.SetItemsProcessed(state.iterations() * 8);
}

void BM_PatchSequenceSerializeDestroy(benchmark::State& state) {
  Json base = JsonObject{{"rows", JsonArray{10, 20, 30}}, {"meta", JsonObject{{"name", "old"}}}};
  auto oldSnapshot = base;
  Json expected = JsonObject{{"rows", JsonArray{10, 15, 30, 10}}, {"meta", JsonObject{{"name", "new"}}}};
  JsonArray patch{JsonObject{{"op", "test"}, {"path", "/rows/1"}, {"value", 20}},
                  JsonObject{{"op", "add"}, {"path", "/rows/1"}, {"value", 15}},
                  JsonObject{{"op", "remove"}, {"path", "/rows/2"}},
                  JsonObject{{"op", "copy"}, {"from", "/rows/0"}, {"path", "/rows/-"}},
                  JsonObject{{"op", "replace"}, {"path", "/meta/name"}, {"value", "new"}}};
  auto validation = Star::jsonPatch(base, patch);
  if (!checked(state, validation)
      || !require(state, *validation == expected && base == oldSnapshot,
                  "patch sequence output or old snapshot differs"))
    return;
  auto output = validation->repr(0, true);
  auto oldOutput = oldSnapshot.repr(0, true);
  auto baseOutput = base.repr(0, true);
  if (!checked(state, output) || !checked(state, oldOutput) || !checked(state, baseOutput)
      || !require(state,
                  *output == "{\"meta\":{\"name\":\"new\"},\"rows\":[10,15,30,10]}"
                    && *oldOutput == "{\"meta\":{\"name\":\"old\"},\"rows\":[10,20,30]}" && *baseOutput == *oldOutput,
                  "patch sequence canonical bytes or immutable snapshot differ"))
    return;
  label(state, *oldOutput, 64);
  for (auto _ : state) {
    (void)_;
    for (std::size_t i = 0; i < 64; ++i) {
      auto result = Star::jsonPatch(base, patch);
      if (!checked(state, result))
        return;
      auto text = result->repr(0, true);
      if (!checked(state, text))
        return;
      benchmark::DoNotOptimize(*text);
    }
  }
  state.SetItemsProcessed(state.iterations() * 64);
}

BENCHMARK_TEMPLATE(BM_ParseDestroy, ParseKind::Numbers)
  ->Name("Json/Parse/AsciiNumbersDestroy")
  ->Arg(128)
  ->Arg(1024)
  ->Iterations(1);
BENCHMARK_TEMPLATE(BM_ParseDestroy, ParseKind::Unicode)
  ->Name("Json/Parse/UnicodeDestroy")
  ->Arg(128)
  ->Arg(1024)
  ->Iterations(1);
BENCHMARK_TEMPLATE(BM_ParseDestroy, ParseKind::Comments)
  ->Name("Json/Parse/CommentHeavyDestroy")
  ->Arg(128)
  ->Arg(1024)
  ->Iterations(1);
BENCHMARK_TEMPLATE(BM_FormattedBuildSerializeDestroy, false, false)
  ->Name("Json/Formatted/FlatArrayParseSerializeDestroy")
  ->Arg(128)
  ->Arg(1024)
  ->Arg(4096)
  ->Iterations(1);
BENCHMARK_TEMPLATE(BM_FormattedBuildSerializeDestroy, true, false)
  ->Name("Json/Formatted/FlatObjectParseSerializeDestroy")
  ->Arg(128)
  ->Arg(1024)
  ->Arg(4096)
  ->Iterations(1);
BENCHMARK_TEMPLATE(BM_FormattedBuildSerializeDestroy, false, true)
  ->Name("Json/Formatted/FlatArrayCreateSerializeDestroy")
  ->Arg(128)
  ->Arg(1024)
  ->Arg(4096)
  ->Iterations(1);
BENCHMARK_TEMPLATE(BM_FormattedBuildSerializeDestroy, true, true)
  ->Name("Json/Formatted/FlatObjectCreateSerializeDestroy")
  ->Arg(128)
  ->Arg(1024)
  ->Arg(4096)
  ->Iterations(1);
BENCHMARK_TEMPLATE(BM_FormattedSerializeDestroy, false)
  ->Name("Json/Formatted/ArraySerializeDestroy/1024")
  ->Iterations(1);
BENCHMARK_TEMPLATE(BM_FormattedSerializeDestroy, true)
  ->Name("Json/Formatted/ObjectSerializeDestroy/1024")
  ->Iterations(1);
BENCHMARK(BM_PathSetSerializeDestroy)
  ->Name("Json/Path/SetSerializeDestroy")
  ->Args({4, 8})
  ->Args({16, 4})
  ->Args({4, 128})
  ->Iterations(1);
BENCHMARK_TEMPLATE(BM_PatchSearchMergeSerializeDestroy, true)
  ->Name("Json/Patch/SearchMergeLastHitSerializeDestroy")
  ->Arg(128)
  ->Arg(1024)
  ->Iterations(1);
BENCHMARK_TEMPLATE(BM_PatchSearchMergeSerializeDestroy, false)
  ->Name("Json/Patch/SearchMergeMissSerializeDestroy")
  ->Arg(128)
  ->Arg(1024)
  ->Iterations(1);
BENCHMARK(BM_PatchSequenceSerializeDestroy)->Name("Json/Patch/SequenceSerializeDestroy")->Iterations(1);

}// namespace

class CheckedReporter : public benchmark::ConsoleReporter {
public:
  void ReportRuns(std::vector<Run> const& reports) override {
    for (auto const& run : reports)
      failed |= run.skipped == benchmark::internal::SkippedWithError;
    benchmark::ConsoleReporter::ReportRuns(reports);
  }
  bool failed = false;
};

int main(int argc, char** argv) {
  auto nativeRuntime = Star::NativeRuntime::create();
  if (!nativeRuntime) {
    std::cerr << nativeRuntime.error().what() << '\n';
    return 1;
  }
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

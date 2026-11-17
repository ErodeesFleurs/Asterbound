#include <cstdio>
#include <benchmark/benchmark.h>

import std;
import star.format;
import star.formatted_json;
import star.json;
import star.json_patch;
import star.json_path;
import star.list;
import star.map;
import star.benchmark_runner;
import star.string;
import star.text_formatter;

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
                             input.utf8_size(), batch));
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
      expected.push_back(values[i % values.size()]);
    } else if constexpr (Kind == ParseKind::Unicode) {
      constexpr std::array<std::string_view, 3> spelling{"\"日本語🌟\"", "\"caf\\u00e9\\n\\t\\\"\\\\\"",
                                                         "\"\\ud83d\\ude80\""};
      std::array<Json, 3> values{Json(String("日本語🌟")), Json(String("café\n\t\"\\")), Json(String("🚀"))};
      text += spelling[i % spelling.size()];
      expected.push_back(values[i % values.size()]);
    } else {
      text += " /* config 注釈 */ " + std::to_string(i) + " // setting\n";
      expected.push_back(Json(i));
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
    auto values = validation->array_ptr();
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
  state.SetBytesProcessed(state.iterations() * Batch * fixture.text.utf8_size());
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
      array.push_back(std::move(value));
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
    rows.push_back(JsonObject{{"id", Json(i)},
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
  auto validation = Star::json_patch(base, patch);
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
      auto result = Star::json_patch(base, patch);
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
  auto validation = Star::json_patch(base, patch);
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
      auto result = Star::json_patch(base, patch);
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

// Additional bounded batches isolate the production entry points being changed.
// Expected bytes below are assembled from fixture rules, never from edit output.
std::uint64_t byte_fingerprint(std::string_view text) {
  std::uint64_t fingerprint = 14695981039346656037ull;
  for (unsigned char byte : text) {
    fingerprint ^= byte;
    fingerprint *= 1099511628211ull;
  }
  return fingerprint;
}

void label_result(benchmark::State& state, String const& input, String const& expected, std::size_t batch) {
  state.SetLabel(std::format("fixture-fnv1a={:016x}; result-fnv1a={:016x}; bytes={}; batch={}; includes destruction",
                             byte_fingerprint(input.utf8()), byte_fingerprint(expected.utf8()), input.utf8_size(),
                             batch));
}

void bm_object_write_text_destroy(benchmark::State& state) {
  auto fixture = flatFixture(true, static_cast<std::size_t>(state.range(0)), false);
  auto object = fixture.expected.toObject();
  if (!checked(state, object))
    return;
  auto snapshot = *object;
  Star::TextBuffer validation;
  auto written = Star::write_text(validation, *object);
  if (!checked(state, written))
    return;
  auto parsed = Json::parse(String(validation.view()));
  if (!checked(state, parsed)
      || !require(state, *parsed == fixture.expected, "direct object writer changed complete semantic output"))
    return;
  // Unsorted output follows this map's real iteration order. These fixtures have
  // safe ASCII keys and integer values, so independent byte assembly is exact.
  std::string expected = "{";
  for (auto const& [key, value] : *object) {
    if (expected.size() != 1)
      expected += ',';
    auto integer = value.toInt();
    if (!checked(state, integer))
      return;
    expected += '"' + key.utf8() + "\":" + std::to_string(*integer);
  }
  expected += '}';
  if (!require(state, validation.view() == expected, "direct object writer bytes differ from map traversal"))
    return;
  constexpr std::size_t batch = 64;
  label_result(state, fixture.text, String(expected), batch);
  for (auto _ : state) {
    (void)_;
    for (std::size_t index = 0; index < batch; ++index) {
      Star::TextBuffer output;
      auto result = Star::write_text(output, *object);
      if (!checked(state, result))
        return;
      benchmark::DoNotOptimize(output.view());
    }
  }
  require(state, *object == snapshot, "direct object writer changed its input map");
  state.SetItemsProcessed(state.iterations() * batch);
}

enum class ArrayEdit { Append, Insert, Erase };

template <ArrayEdit Edit>
std::expected<Json, Star::StarException> edit_array(Json const& base, Json const& replacement, std::size_t index) {
  if constexpr (Edit == ArrayEdit::Append)
    return base.append(replacement);
  else if constexpr (Edit == ArrayEdit::Insert)
    return base.insert(index, replacement);
  else
    return base.erase(index);
}

template <ArrayEdit Edit> void bm_array_edit_destroy(benchmark::State& state) {
  auto count = static_cast<std::size_t>(state.range(0));
  auto fixture = flatFixture(false, count, false);
  auto snapshot = fixture.expected;
  auto edit_index = count / 2;
  Json replacement = JsonObject{{"inserted", JsonArray{Json(), "日本語", 91}}};
  JsonArray expected_values;
  for (std::size_t index = 0; index < count; ++index) {
    if constexpr (Edit == ArrayEdit::Insert) {
      if (index == edit_index)
        expected_values.push_back(replacement);
    }
    if (Edit != ArrayEdit::Erase || index != edit_index)
      expected_values.push_back(Json(index));
  }
  if constexpr (Edit == ArrayEdit::Append)
    expected_values.push_back(replacement);
  Json expected(std::move(expected_values));
  // Text oracle is independent of both the edit and the expected DOM renderer.
  std::string expected_text = "[";
  auto append_expected = [&](std::string_view value) {
    if (expected_text.size() != 1)
      expected_text += ',';
    expected_text += value;
  };
  constexpr std::string_view replacement_text = "{\"inserted\":[null,\"日本語\",91]}";
  for (std::size_t index = 0; index < count; ++index) {
    if (Edit == ArrayEdit::Insert && index == edit_index)
      append_expected(replacement_text);
    if (Edit != ArrayEdit::Erase || index != edit_index)
      append_expected(std::to_string(index));
  }
  if constexpr (Edit == ArrayEdit::Append)
    append_expected(replacement_text);
  expected_text += ']';
  auto validation = edit_array<Edit>(fixture.expected, replacement, edit_index);
  if (!checked(state, validation)
      || !require(state, *validation == expected, "array edit differs from independent complete values"))
    return;
  auto output = validation->repr(0, true);
  auto old_output = snapshot.repr(0, true);
  if (!checked(state, output) || !checked(state, old_output)
      || !require(state, *output == String(expected_text) && *old_output == fixture.text,
                  "array edit bytes or old snapshot differ"))
    return;
  constexpr std::size_t batch = 64;
  label_result(state, fixture.text, String(expected_text), batch);
  for (auto _ : state) {
    (void)_;
    for (std::size_t index = 0; index < batch; ++index) {
      auto result = edit_array<Edit>(fixture.expected, replacement, edit_index);
      if (!checked(state, result))
        return;
      benchmark::DoNotOptimize(*result);
    }
  }
  auto base_output = fixture.expected.repr(0, true);
  auto snapshot_output = snapshot.repr(0, true);
  if (!checked(state, base_output) || !checked(state, snapshot_output))
    return;
  require(state, *base_output == fixture.text && *snapshot_output == fixture.text,
          "array edit mutated its immutable source or retained snapshot");
  state.SetItemsProcessed(state.iterations() * batch);
}

enum class FormattedEdit { ArrayInsert, ArrayErase, ObjectInsert, ObjectEraseDuplicate };

constexpr std::size_t formatted_count = 128;
constexpr std::size_t formatted_tail = 32;
constexpr std::string_view edit_whitespace = " \t/* adjacent 注釈 */\n ";

// Parsing coalesces a whitespace run into one token. Erasing final entries
// retains each preceding pre-comma token, producing 33 genuinely adjacent
// comment/whitespace tokens through public immutable production operations.
template <bool Object> Fixture formatted_edit_fixture(std::size_t count, std::size_t tail, bool insert, bool erase) {
  std::string text = Object ? "{" : "[";
  JsonArray array;
  JsonObject object;
  auto append_entry = [&](std::string const& key, std::string_view spelling, Json value) {
    if (text.size() != 1) {
      text += edit_whitespace;
      text += ',';
    }
    text += edit_whitespace;
    if constexpr (Object) {
      text += '"' + key + '"';
      text += edit_whitespace;
      text += ':';
      text += edit_whitespace;
      object[String(key)] = value;
    } else {
      array.push_back(value);
    }
    text += spelling;
  };
  if (insert)
    append_entry("new", "-0.0", Json(-0.0));
  for (std::size_t index = 0; index < count; ++index) {
    bool duplicate = Object && (index == 0 || index == formatted_count / 2);
    if (erase && index == (Object ? formatted_count / 2 : 0))
      continue;
    auto key = duplicate ? std::string("duplicate") : std::format("k{:06}", index);
    append_entry(key, std::to_string(index), Json(index));
  }
  for (std::size_t index = 0; index <= tail; ++index)
    text += edit_whitespace;
  text += Object ? '}' : ']';
  return {String(std::move(text)), Object ? Json(std::move(object)) : Json(std::move(array))};
}

template <FormattedEdit Edit>
std::expected<FormattedJson, Star::StarException> edit_formatted(FormattedJson const& base,
                                                                 FormattedJson const& replacement) {
  if constexpr (Edit == FormattedEdit::ArrayInsert)
    return base.insert(0, replacement);
  else if constexpr (Edit == FormattedEdit::ArrayErase)
    return base.erase(0);
  else if constexpr (Edit == FormattedEdit::ObjectInsert)
    return base.prepend("new", replacement);
  else
    return base.erase("duplicate");
}

template <FormattedEdit Edit, bool Serialize> void bm_formatted_edit_destroy(benchmark::State& state) {
  constexpr bool object = Edit == FormattedEdit::ObjectInsert || Edit == FormattedEdit::ObjectEraseDuplicate;
  constexpr bool insert = Edit == FormattedEdit::ArrayInsert || Edit == FormattedEdit::ObjectInsert;
  auto seed_fixture = formatted_edit_fixture<object>(formatted_count + formatted_tail, 0, false, false);
  auto fixture = formatted_edit_fixture<object>(formatted_count, formatted_tail, false, false);
  auto expected = formatted_edit_fixture<object>(formatted_count, formatted_tail, insert, !insert);
  auto base = FormattedJson::parse(seed_fixture.text);
  auto replacement = FormattedJson::parse("-0.0");
  if (!checked(state, base) || !checked(state, replacement))
    return;
  for (std::size_t index = formatted_count + formatted_tail; index > formatted_count; --index) {
    auto trimmed = [&]() {
      if constexpr (object)
        return base->erase(String(std::format("k{:06}", index - 1)));
      else
        return base->erase(index - 1);
    }();
    if (!checked(state, trimmed))
      return;
    base = std::move(trimmed);
  }
  std::size_t adjacent = 0;
  std::size_t longest = 0;
  for (auto const& token : base->elements()) {
    adjacent = std::holds_alternative<Star::WhitespaceElement>(token) ? adjacent + 1 : 0;
    longest = std::max(longest, adjacent);
  }
  if (!require(state, longest >= formatted_tail + 1, "formatted fixture lacks adjacent whitespace tokens")
      || !validateFormatted(state, *base, fixture, true))
    return;
  auto snapshot = *base;
  auto validation = edit_formatted<Edit>(*base, *replacement);
  if (!checked(state, validation) || !validateFormatted(state, *validation, expected, true)
      || !validateFormatted(state, snapshot, fixture, true))
    return;
  constexpr std::size_t batch = 16;
  label_result(state, fixture.text, expected.text, batch);
  for (auto _ : state) {
    (void)_;
    for (std::size_t index = 0; index < batch; ++index) {
      auto result = edit_formatted<Edit>(*base, *replacement);
      if (!checked(state, result))
        return;
      if constexpr (Serialize) {
        auto output = result->repr();
        if (!checked(state, output))
          return;
        benchmark::DoNotOptimize(*output);
      } else {
        benchmark::DoNotOptimize(*result);
      }
    }
  }
  if (!validateFormatted(state, *base, fixture, true) || !validateFormatted(state, snapshot, fixture, true))
    return;
  state.SetItemsProcessed(state.iterations() * batch);
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
BENCHMARK(bm_object_write_text_destroy)->Name("Json/Object/WriteTextBufferDestroy")->Arg(8)->Arg(1024)->Iterations(1);
BENCHMARK_TEMPLATE(bm_array_edit_destroy, ArrayEdit::Append)
  ->Name("Json/Array/AppendDestroy")
  ->Arg(32)
  ->Arg(1024)
  ->Iterations(1);
BENCHMARK_TEMPLATE(bm_array_edit_destroy, ArrayEdit::Insert)
  ->Name("Json/Array/InsertMiddleDestroy")
  ->Arg(32)
  ->Arg(1024)
  ->Iterations(1);
BENCHMARK_TEMPLATE(bm_array_edit_destroy, ArrayEdit::Erase)
  ->Name("Json/Array/EraseMiddleDestroy")
  ->Arg(32)
  ->Arg(1024)
  ->Iterations(1);
BENCHMARK_TEMPLATE(bm_formatted_edit_destroy, FormattedEdit::ArrayInsert, false)
  ->Name("Json/Formatted/ArrayInsertAdjacentTriviaDestroy/128")
  ->Iterations(1);
BENCHMARK_TEMPLATE(bm_formatted_edit_destroy, FormattedEdit::ArrayInsert, true)
  ->Name("Json/Formatted/ArrayInsertAdjacentTriviaSerializeDestroy/128")
  ->Iterations(1);
BENCHMARK_TEMPLATE(bm_formatted_edit_destroy, FormattedEdit::ArrayErase, false)
  ->Name("Json/Formatted/ArrayEraseAdjacentTriviaDestroy/128")
  ->Iterations(1);
BENCHMARK_TEMPLATE(bm_formatted_edit_destroy, FormattedEdit::ArrayErase, true)
  ->Name("Json/Formatted/ArrayEraseAdjacentTriviaSerializeDestroy/128")
  ->Iterations(1);
BENCHMARK_TEMPLATE(bm_formatted_edit_destroy, FormattedEdit::ObjectInsert, false)
  ->Name("Json/Formatted/ObjectInsertAdjacentTriviaDestroy/128")
  ->Iterations(1);
BENCHMARK_TEMPLATE(bm_formatted_edit_destroy, FormattedEdit::ObjectInsert, true)
  ->Name("Json/Formatted/ObjectInsertAdjacentTriviaSerializeDestroy/128")
  ->Iterations(1);
BENCHMARK_TEMPLATE(bm_formatted_edit_destroy, FormattedEdit::ObjectEraseDuplicate, false)
  ->Name("Json/Formatted/ObjectEraseRevealDuplicateAdjacentTriviaDestroy/128")
  ->Iterations(1);
BENCHMARK_TEMPLATE(bm_formatted_edit_destroy, FormattedEdit::ObjectEraseDuplicate, true)
  ->Name("Json/Formatted/ObjectEraseRevealDuplicateAdjacentTriviaSerializeDestroy/128")
  ->Iterations(1);

}// namespace

int main(int argc, char** argv) {
  return Star::run_benchmarks(argc, argv);
}

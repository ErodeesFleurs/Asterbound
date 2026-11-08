#include <benchmark/benchmark.h>

import std;
import star.format;
import star.lexical_cast;
import star.native_runtime;
import star.unicode;

namespace {
constexpr std::size_t micro_batch = 64;

std::uint64_t fingerprint(std::string_view bytes) {
  std::uint64_t result = 14695981039346656037ull;
  for (unsigned char byte : bytes) {
    result ^= byte;
    result *= 1099511628211ull;
  }
  return result;
}

void append_field(std::string& output, std::string_view field) {
  output += std::format("{}:", field.size());
  output.append(field);
}

// Include every summary field, not just the user-visible first reason line.
std::string summary_bytes(Star::ExceptionSummary const& summary) {
  std::string output = std::format("{};", static_cast<int>(summary.kind));
  for (auto const* field : {&summary.category, &summary.reason, &summary.operation,
           &summary.asset, &summary.assetProvider, &summary.patch, &summary.patchProvider})
    append_field(output, *field);
  output += std::format("{};", summary.locations.size());
  for (auto const& location : summary.locations) {
    append_field(output, location.source);
    output += std::format("{};", location.line);
    append_field(output, location.provider);
  }
  return output;
}

Star::ExceptionSummary native_summary(std::string category, std::string reason) {
  Star::ExceptionSummary summary;
  summary.category = std::move(category);
  summary.reason = std::move(reason);
  return summary;
}

Star::ExceptionSummary replacement_summary() {
  auto summary = native_summary("fixture-category", "brief reason\nretained detail");
  summary.kind = Star::ExceptionSummaryKind::Resource;
  summary.operation = "load";
  summary.asset = "/fixture.asset";
  summary.assetProvider = "base";
  summary.patch = "/fixture.patch";
  summary.patchProvider = "mod";
  summary.locations.push_back({"/fixture.lua", 17, "scripts"});
  summary.locations.push_back({"/caller.lua", 29, "scripts"});
  return summary;
}

constexpr std::string_view replacement_brief =
    "Resource error: brief reason\n  Operation: load\n  Asset: /fixture.asset"
    "\n  Asset provider: base\n  Patch: /fixture.patch\n  Patch provider: mod"
    "\n  Script: /fixture.lua:17\n    Provider: scripts\n  Called from: /caller.lua:29";

template <class Category>
  requires std::derived_from<Category, Star::StarException>
bool verify_error(benchmark::State& state, Star::StarException const& error,
    Star::ExceptionSummary const& summary, std::string_view short_text,
    std::string_view brief_text, bool no_trace, std::string& complete) {
  auto const& metadata = error.diagnostic_summary();
  if (!error.is<Category>() || !error.is<Star::StarException>()
      || !metadata || summary_bytes(*metadata) != summary_bytes(summary)
      || Star::print_exception(error, false) != short_text
      || std::string_view(error.what()) != short_text
      || Star::exception_summary(error) != brief_text) {
    state.SkipWithError("error category, complete summary or short diagnostic differs from frozen fixture");
    return false;
  }
  complete = summary_bytes(*metadata);
  append_field(complete, short_text);
  append_field(complete, brief_text);
  append_field(complete, "category+StarException");
  if (no_trace) {
    auto full = Star::print_exception(error, true);
    if (full != short_text) {
      state.SkipWithError("explicit no-trace full diagnostic differs from frozen fixture");
      return false;
    }
    append_field(complete, full);
  }
  return true;
}

void set_fingerprint(benchmark::State& state, std::string_view complete, std::string_view scope) {
  state.SetLabel(std::format("verified_bytes={}; fnv1a={:016x}; {}", complete.size(), fingerprint(complete), scope));
}

template <class Operation>
  requires std::invocable<Operation&>
      && std::same_as<std::invoke_result_t<Operation&>, Star::StarException>
void register_no_trace(char const* name, Operation operation, std::string expected_text,
    Star::ExceptionSummary expected_summary, std::string expected_brief) {
  benchmark::RegisterBenchmark(name,
      [operation = std::move(operation), expected_text = std::move(expected_text),
          expected_summary = std::move(expected_summary), expected_brief = std::move(expected_brief)]
      (benchmark::State& state) mutable {
    std::string complete;
    {
      auto result = operation();
      if (!verify_error<Star::OutOfRangeException>(state, result, expected_summary,
              expected_text, expected_brief, true, complete))
        return;
      if (result.template is<Star::IOException>()) {
        state.SkipWithError("primary diagnostic acquired related error category");
        return;
      }
    }
    set_fingerprint(state, complete, "explicit no-trace; construction+destruction; no rendering timed");
    for (auto _ : state) {
      benchmark::DoNotOptimize(operation);
      auto result = operation();
      benchmark::DoNotOptimize(result);
    }
  });
}

// These are deliberately small expected-value/handle microbenchmarks, not
// whole-operation latency. Opaque input/output barriers keep materialization
// and reference-count/destruction work inside the timed batch.
void register_expected_micro() {
  benchmark::RegisterBenchmark("Error/Expected/SmallSuccess/Micro64", [](benchmark::State& state) {
    std::array<std::uint32_t, micro_batch> inputs;
    std::string complete;
    for (std::size_t i = 0; i != inputs.size(); ++i) {
      inputs[i] = static_cast<std::uint32_t>(i * 17 + 3);
      std::expected<std::uint32_t, Star::StarException> result(inputs[i]);
      if (!result || *result != i * 17 + 3) {
        state.SkipWithError("small-success expected changed payload");
        return;
      }
      complete += std::format("{};", *result);
    }
    set_fingerprint(state, complete, "micro; 64 materializations+destructions");
    for (auto _ : state) {
      for (auto input : inputs) {
        benchmark::DoNotOptimize(input);
        std::expected<std::uint32_t, Star::StarException> result(input);
        benchmark::DoNotOptimize(result);
      }
    }
    state.SetItemsProcessed(state.iterations() * micro_batch);
  });
  for (bool move : {false, true}) {
    benchmark::RegisterBenchmark(move ? "Error/Expected/MoveFromFreshCopy/NoTrace/Micro64"
                                     : "Error/Expected/CopyPropagation/NoTrace/Micro64",
        [move](benchmark::State& state) {
      std::expected<int, Star::StarException> source = std::unexpected(Star::OutOfRangeException("origin", false));
      auto copy = source;
      auto moved = std::move(copy);
      std::string complete;
      if (moved) {
        state.SkipWithError("error propagation unexpectedly produced success");
        return;
      }
      if (!verify_error<Star::OutOfRangeException>(state, moved.error(),
              native_summary("OutOfRangeException", "origin"), "(OutOfRangeException) origin",
              "OutOfRangeException: origin", true, complete))
        return;
      set_fingerprint(state, complete, move ? "micro; 64 copy+move+destruction; explicit no-trace seed"
                                           : "micro; 64 copy+destruction; explicit no-trace seed");
      for (auto _ : state) {
        for (std::size_t i = 0; i != micro_batch; ++i) {
          auto const* input = &source;
          benchmark::DoNotOptimize(input);
          auto result = *input;
          if (move) {
            benchmark::DoNotOptimize(result);
            auto forwarded = std::move(result);
            benchmark::DoNotOptimize(forwarded);
          } else {
            benchmark::DoNotOptimize(result);
          }
        }
      }
      state.SetItemsProcessed(state.iterations() * micro_batch);
    });
  }
}

void register_diagnostics() {
  auto summary = native_summary("OutOfRangeException", "origin");
  register_no_trace("Error/Origin/NoTrace", []() -> Star::StarException {
    return Star::OutOfRangeException("origin", false);
  }, "(OutOfRangeException) origin", summary, "OutOfRangeException: origin");
  for (unsigned depth : {1u, 8u, 64u}) {
    std::string text = "(OutOfRangeException) origin";
    for (unsigned i = 0; i != depth; ++i)
      text = "operation\nCaused by: " + text;
    register_no_trace(std::format("Error/ContextChain/NoTrace/{}", depth).c_str(),
        [depth]() -> Star::StarException {
      Star::StarException error = Star::OutOfRangeException("origin", false);
      for (unsigned i = 0; i != depth; ++i)
        error = std::move(error).with_context("operation");
      return error;
    }, std::move(text), summary, "OutOfRangeException: origin");
  }
  // Seeds are deliberately reused; each iteration measures just the named graph
  // extension and its destruction, not origin construction or lazy rendering.
  auto primary = Star::StarException(Star::OutOfRangeException("origin", false));
  auto secondary = Star::StarException(Star::IOException("secondary", false));
  register_no_trace("Error/SummaryReplacement/NoTrace", [primary]() mutable {
    return primary.with_summary(replacement_summary());
  }, "(OutOfRangeException) origin", replacement_summary(), std::string(replacement_brief));
  register_no_trace("Error/Related/NoTrace", [primary, secondary]() mutable {
    return primary.with_related(secondary, "cleanup");
  }, "(OutOfRangeException) origin\ncleanup\nRelated error: (IOException) secondary",
      summary, "OutOfRangeException: origin");
}

template <class Category, class Operation>
  requires std::derived_from<Category, Star::StarException>
      && std::invocable<Operation&, std::string_view>
      && std::same_as<std::invoke_result_t<Operation&, std::string_view>, std::expected<std::size_t, Star::StarException>>
void register_checked(char const* name, std::string input, Operation operation,
    std::optional<std::size_t> expected_value, std::string category = {}, std::string reason = {}) {
  benchmark::RegisterBenchmark(name,
      [input = std::move(input), operation = std::move(operation), expected_value,
          category = std::move(category), reason = std::move(reason)](benchmark::State& state) mutable {
    std::string complete;
    auto verify = [&](auto const& result) {
      if (expected_value) {
        if (!result || *result != *expected_value) {
          state.SkipWithError("checked success differs from frozen value");
          return false;
        }
        complete = std::format("success:{}", *result);
        return true;
      }
      if (result) {
        state.SkipWithError("checked malformed input unexpectedly succeeded");
        return false;
      }
      return verify_error<Category>(state, result.error(), native_summary(category, reason),
          std::format("({}) {}", category, reason), std::format("{}: {}", category, reason), false, complete);
    };
    if (!verify(operation(input)))
      return;
    set_fingerprint(state, complete, expected_value ? "public checked path; result destruction timed"
        : "production trace capture enabled; short text+complete summary verified; trace addresses excluded");
    for (auto _ : state) {
      auto* bytes = input.data();
      auto size = input.size();
      benchmark::DoNotOptimize(bytes);
      benchmark::DoNotOptimize(size);
      auto result = operation(std::string_view(bytes, size));
      benchmark::DoNotOptimize(result);
    }
    // Independent full result check after timed iterations as well as setup.
    if (!verify(operation(input)))
      return;
    state.SetItemsProcessed(state.iterations());
    state.SetBytesProcessed(state.iterations() * input.size());
  });
}

void register_checked_paths() {
  auto length = [](std::string_view bytes) { return Star::utf8_length(bytes.data(), bytes.size()); };
  for (unsigned repeats : {8u, 256u}) {
    std::string ascii;
    std::string mixed;
    for (unsigned i = 0; i != repeats; ++i) {
      ascii += "abcdefgh";
      mixed.append("a水é🙂\0z", 12);
    }
    register_checked<Star::UnicodeException>(std::format("Error/Utf8Length/Ascii/{}", repeats).c_str(),
        std::move(ascii), length, repeats * 8);
    register_checked<Star::UnicodeException>(std::format("Error/Utf8Length/MixedNul/{}", repeats).c_str(),
        std::move(mixed), length, repeats * 6);
  }
  for (auto const& [suffix, bytes, reason] : std::array{
      std::tuple{"InvalidLead", std::string("abc\xff", 4), "Invalid UTF-8 code unit sequence"},
      std::tuple{"Truncated", std::string("abc\xe6\xb0", 5), "UTF-8 string missing trailing code units"},
      std::tuple{"Overlong", std::string("abc\xc0\x80", 5), "Invalid UTF-8 code point"}}) {
    register_checked<Star::UnicodeException>(std::format("Error/Utf8Length/Malformed/{}", suffix).c_str(),
        bytes, length, {}, "UnicodeException", reason);
  }
  auto parse = [](std::string_view bytes) { return Star::lexical_cast<std::size_t>(bytes); };
  register_checked<Star::BadLexicalCast>("Error/Numeric/Checked/Zero", "0", parse, 0);
  register_checked<Star::BadLexicalCast>("Error/Numeric/Checked/Success", "123456789", parse, 123456789);
  register_checked<Star::BadLexicalCast>("Error/Numeric/Checked/RangePolicy", "999999999999999999999999999999999999", parse, 0);
  register_checked<Star::BadLexicalCast>("Error/Numeric/Checked/Invalid", "not-a-number", parse, {},
      "BadLexicalCast", "Lexical cast failed on 'not-a-number' (invalid argument)");
  register_checked<Star::BadLexicalCast>("Error/Numeric/Checked/Trailing", "123x", parse, {},
      "BadLexicalCast", "Lexical cast failed on '123x'");
}

class checked_reporter : public benchmark::ConsoleReporter {
public:
  bool failed = false;
  void ReportRuns(std::vector<Run> const& reports) override {
    for (auto const& run : reports)
      failed |= run.skipped == benchmark::internal::SkippedWithError;
    ConsoleReporter::ReportRuns(reports);
  }
};
}

int main(int argc, char** argv) {
  auto runtime = Star::NativeRuntime::create();
  if (!runtime)
    return 1;
  register_expected_micro();
  register_diagnostics();
  register_checked_paths();
  benchmark::Initialize(&argc, argv);
  if (benchmark::ReportUnrecognizedArguments(argc, argv)) { benchmark::Shutdown(); return 1; }
  checked_reporter reporter;
  auto matched = benchmark::RunSpecifiedBenchmarks(&reporter);
  benchmark::Shutdown();
  return reporter.failed || matched == 0 ? 1 : 0;
}

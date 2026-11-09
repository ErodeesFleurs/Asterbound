#include <benchmark/benchmark.h>

import std;
import star.format;
import star.native_runtime;
import star.string;
import star.list;
import star.map;
import star.ordered_map;
import star.line;
import star.vector;
import star.matrix3;
import star.optional;
import star.output_proxy;
import star.json;

namespace {
std::uint64_t fingerprint(std::string_view bytes) {
  std::uint64_t result = 14695981039346656037ull;
  for (unsigned char byte : bytes) {
    result ^= byte;
    result *= 1099511628211ull;
  }
  return result;
}

template <typename Value>
  requires std::formattable<Value, char>
void register_format(char const* name, Value value) {
  benchmark::RegisterBenchmark(name, [value = std::move(value)](benchmark::State& state) {
    auto reference = std::format("{}", value);
    state.SetLabel(std::format("bytes={}; fnv1a={:016x}; includes destruction", reference.size(), fingerprint(reference)));
    for (auto _ : state) {
      auto text = std::format("{}", value);
      benchmark::DoNotOptimize(text.data());
      benchmark::DoNotOptimize(text.size());
    }
  });
}

template <typename Value>
  requires std::formattable<Value, char>
void register_padded(char const* name, Value value) {
  benchmark::RegisterBenchmark(name, [value = std::move(value)](benchmark::State& state) {
    auto reference = std::format("{:>80.40}", value);
    state.SetLabel(std::format("bytes={}; fnv1a={:016x}; includes destruction", reference.size(), fingerprint(reference)));
    for (auto _ : state) {
      auto text = std::format("{:>80.40}", value);
      benchmark::DoNotOptimize(text.data());
      benchmark::DoNotOptimize(text.size());
    }
  });
}

void register_workloads() {
  register_format("Format/Line/Integer", Star::Line2I(Star::Vec2I(1, 2), Star::Vec2I(3, 4)));
  register_format("Format/Line/Float", Star::Line2F(Star::Vec2F(1.2345678f, -0.00012345678f), Star::Vec2F(1e12f, 0.0f)));
  register_format("Format/Matrix/Float", Star::Mat3F(1.2345678f, 2, 3, 4, -0.00012345678f, 6, 7, 8, 9));
  register_format("Format/List/Bool", Star::List<bool>{false, true, false});
  register_format("Format/List/Optional", Star::List<std::optional<int>>{1, std::nullopt, -42});
  register_format("Format/Map/Ordered", Star::OrderedMap<int, Star::String>{{1, "水"}, {2, "text"}, {3, "a\n\""}});
  register_padded("Format/Line/Padded", Star::Line2I(Star::Vec2I(1, 2), Star::Vec2I(3, 4)));
  register_padded("Format/Matrix/Padded", Star::Mat3F(1.2345678f, 2, 3, 4, -0.00012345678f, 6, 7, 8, 9));
  for (std::size_t count : {3u, 32u, 256u, 1024u}) {
    Star::List<double> values;
    Star::StringList strings;
    for (std::size_t i = 0; i != count; ++i) {
      values.push_back((static_cast<double>(i) + 1.0) / 7.0);
      strings.push_back(Star::String(std::string_view("水\0x", 5)));
    }
    register_format(std::format("Format/List/Float/{}", count).c_str(), std::move(values));
    register_format(std::format("Format/StringList/Binary/{}", count).c_str(), std::move(strings));
  }
  benchmark::RegisterBenchmark("Format/Diagnostic/ColdChain", [](benchmark::State& state) {
    for (auto _ : state) {
      Star::StarException error("root", false);
      for (unsigned i = 0; i != 64; ++i)
        error = std::move(error).with_context("operation");
      benchmark::DoNotOptimize(error.what());
    }
  });
  auto diagnostic = Star::ExceptionDiagnostic(Star::StarException("水\ncontext", false).with_context("operation"));
  register_format("Format/Diagnostic/CachedProxy", Star::output_exception(diagnostic, false));
  benchmark::RegisterBenchmark("Format/Json/Utf8", [](benchmark::State& state) {
    auto value = Star::Json(Star::String(std::string(700, 'x') + "水\n\""));
    auto reference = value.repr();
    if (!reference) { state.SkipWithError(reference.error().what()); return; }
    state.SetLabel(std::format("bytes={}; fnv1a={:016x}", reference->utf8_size(), fingerprint(reference->utf8())));
    for (auto _ : state) {
      auto text = value.repr();
      if (!text) { state.SkipWithError(text.error().what()); break; }
      benchmark::DoNotOptimize(text->utf8_data());
      benchmark::DoNotOptimize(text->utf8_size());
    }
  });
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
  register_workloads();
  benchmark::Initialize(&argc, argv);
  if (benchmark::ReportUnrecognizedArguments(argc, argv)) { benchmark::Shutdown(); return 1; }
  checked_reporter reporter;
  auto matched = benchmark::RunSpecifiedBenchmarks(&reporter);
  benchmark::Shutdown();
  return reporter.failed || matched == 0 ? 1 : 0;
}

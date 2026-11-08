#include <benchmark/benchmark.h>

import std;
import star.format;
import star.native_runtime;
import star.string;

namespace {
using Star::String;
using Star::StringList;
enum class operation { replace_dense, replace_missing, replace_sparse, rsplit, rsplit_any, split_any,
                       join, join_with, lower, upper, rot13, at, substr, erase, trim };

struct fixture {
  String source;
  String token;
  StringList parts;
  std::string replaced;
  std::string joined;
  std::size_t codepoints;
};

fixture make_fixture(std::string_view kind, std::size_t count) {
  std::string token = kind == "ASCII" ? "aZ" : kind == "UTF8" ? "é水aZ" : std::string("a\0Z", 3);
  std::string source, replaced, joined;
  StringList parts;
  parts.reserve(count);
  for (std::size_t i = 0; i != count; ++i) {
    source += token;
    source += ',';
    replaced += "Q,";
    if (i) joined += '|';
    joined += token;
    parts.push_back(String(std::string_view(token)));
  }
  return {String(std::move(source)), String(std::move(token)), std::move(parts),
          std::move(replaced), std::move(joined), count * (kind == "ASCII" ? 3 : kind == "UTF8" ? 5 : 4)};
}

template <operation Op>
auto evaluate(fixture const& f) {
  if constexpr (Op == operation::replace_dense) return f.source.replace(f.token, "Q");
  else if constexpr (Op == operation::replace_missing) return f.source.replace("not-present", "Q");
  else if constexpr (Op == operation::replace_sparse) return f.source.replace("Za", "Q", String::CaseInsensitive);
  else if constexpr (Op == operation::rsplit) return f.source.rsplit(String(","), 4);
  else if constexpr (Op == operation::rsplit_any) return f.source.rsplit_any(",", 4);
  else if constexpr (Op == operation::split_any) return f.source.split_any(",");
  else if constexpr (Op == operation::join) return f.parts.join("|");
  else if constexpr (Op == operation::join_with)
    return String::join_with("|", f.source, f.source, f.source, f.source, f.source, f.source, f.source, f.source);
  else if constexpr (Op == operation::lower) return f.source.to_lower();
  else if constexpr (Op == operation::upper) return f.source.to_upper();
  else if constexpr (Op == operation::rot13) return f.source.rot13();
  else if constexpr (Op == operation::at) return f.source.at(f.codepoints / 2);
  else if constexpr (Op == operation::substr) return f.source.substr(f.codepoints / 2, 5);
  else if constexpr (Op == operation::erase) {
    String copy(f.source);
    auto result = copy.erase(f.codepoints / 2, 5);
    if (!result) return std::expected<String, Star::StarException>(std::unexpected(std::move(result.error())));
    return std::expected<String, Star::StarException>(std::move(copy));
  } else return f.source.trim(",");
}

void escape(String const& text) {
  benchmark::DoNotOptimize(text.utf8().data());
  benchmark::DoNotOptimize(text.utf8().size());
}
void escape(StringList const& parts) {
  for (auto const& part : parts) escape(part);
}
void escape(char32_t point) { benchmark::DoNotOptimize(point); }

template <operation Op>
bool validate(fixture const& f) {
  auto output = evaluate<Op>(f);
  if constexpr (Op == operation::join) return output.utf8() == f.joined;
  else {
    if (!output) return false;
    if constexpr (Op == operation::replace_dense) return output->utf8() == f.replaced;
    else if constexpr (Op == operation::replace_missing || Op == operation::replace_sparse)
      return *output == f.source;
    else if constexpr (Op == operation::rsplit || Op == operation::rsplit_any) {
      if (output->size() != 5) return false;
      std::string prefix;
      auto retained = f.parts.size() - (Op == operation::rsplit ? 3 : 4);
      for (std::size_t i = 0; i != retained; ++i) { if (i) prefix += ','; prefix += f.token.utf8(); }
      if ((*output)[0].utf8() != prefix) return false;
      for (std::size_t i = 1; i < output->size(); ++i)
        if ((*output)[i] != ((Op == operation::rsplit && i == 4) ? String() : f.token)) return false;
      return true;
    } else if constexpr (Op == operation::split_any) return *output == f.parts;
    else if constexpr (Op == operation::join_with) {
      std::string expected;
      for (int i = 0; i != 8; ++i) { if (i) expected += '|'; expected += f.source.utf8(); }
      return output->utf8() == expected;
    } else if constexpr (Op == operation::lower || Op == operation::upper || Op == operation::rot13) {
      auto expected = f.source.utf8();
      for (auto& c : expected) {
        if constexpr (Op == operation::lower) { if (c >= 'A' && c <= 'Z') c += 'a' - 'A'; }
        else if constexpr (Op == operation::upper) { if (c >= 'a' && c <= 'z') c -= 'a' - 'A'; }
        else if ((c >= 'a' && c <= 'm') || (c >= 'A' && c <= 'M')) c += 13;
        else if ((c >= 'n' && c <= 'z') || (c >= 'N' && c <= 'Z')) c -= 13;
      }
      return output->utf8() == expected;
    } else if constexpr (Op == operation::trim) {
      auto expected = f.source.utf8(); expected.pop_back(); return output->utf8() == expected;
    } else {
      auto wide = f.source.wide_string();
      if (!wide) return false;
      if constexpr (Op == operation::at) return *output == (*wide)[f.codepoints / 2];
      else if constexpr (Op == operation::substr) {
        auto expected = String::create(std::u32string_view(*wide).substr(f.codepoints / 2, 5));
        return expected && *output == *expected;
      } else {
        wide->erase(f.codepoints / 2, 5);
        auto expected = String::create(std::u32string_view(*wide));
        return expected && *output == *expected;
      }
    }
  }
}

template <operation Op>
void measure(benchmark::State& state, std::string_view kind) {
  auto f = make_fixture(kind, static_cast<std::size_t>(state.range(0)));
  if (!validate<Op>(f)) { state.SkipWithError("String output differs from independent expected bytes/codepoints"); return; }
  state.SetLabel("complete owning output/destruction timed; fixture and full validation untimed");
  for (auto _ : state) {
    (void)_;
    benchmark::ClobberMemory();
    auto output = evaluate<Op>(f);
    if constexpr (Op == operation::join) escape(output);
    else {
      if (!output) { state.SkipWithError("String operation failed"); return; }
      escape(*output);
    }
  }
  state.SetBytesProcessed(state.iterations() * static_cast<std::int64_t>(f.source.utf8().size()));
}

template <operation Op>
void register_operation(char const* name) {
  for (auto kind : {"ASCII", "UTF8", "EmbeddedNUL"}) {
    auto label = std::format("String/{}/{}/Fragments", name, kind);
    benchmark::RegisterBenchmark(label.c_str(), [kind](benchmark::State& state) { measure<Op>(state, kind); })
      ->Arg(8)->Arg(256);
  }
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
  if (!runtime) { std::cerr << runtime.error().what() << '\n'; return 1; }
  register_operation<operation::replace_dense>("ReplaceDense");
  register_operation<operation::replace_missing>("ReplaceMissing");
  register_operation<operation::replace_sparse>("ReplaceInsensitiveMiss");
  register_operation<operation::rsplit>("ReverseSplit");
  register_operation<operation::rsplit_any>("ReverseSplitAny");
  register_operation<operation::split_any>("SplitAny");
  register_operation<operation::join>("Join");
  register_operation<operation::join_with>("JoinWith");
  register_operation<operation::lower>("Lower");
  register_operation<operation::upper>("Upper");
  register_operation<operation::rot13>("Rot13");
  register_operation<operation::at>("AtMiddle");
  register_operation<operation::substr>("SubstringMiddle");
  register_operation<operation::erase>("EraseMiddle");
  register_operation<operation::trim>("Trim");
  benchmark::Initialize(&argc, argv);
  if (benchmark::ReportUnrecognizedArguments(argc, argv)) { benchmark::Shutdown(); return 1; }
  checked_reporter reporter;
  auto matched = benchmark::RunSpecifiedBenchmarks(&reporter);
  benchmark::Shutdown();
  return reporter.failed || matched == 0 ? 1 : 0;
}

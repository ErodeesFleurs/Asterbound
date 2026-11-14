#include <cstdio>
#include <benchmark/benchmark.h>

import std;
import star.format;
import star.native_runtime;
import star.optional;
import star.string;

namespace {

constexpr std::int64_t known_identity = 0x123456789abcdef;
constexpr std::string_view short_payload("seed\0\xc3\xa9", 7);
constexpr auto long_bytes = std::to_array(
    "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
    "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
    "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
    "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
    "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
    "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
    "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
    "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
    "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
    "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
    "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
    "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
    "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
    "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
    "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
    "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
    "/asset\0\xc3\xa9");
constexpr std::string_view long_payload(long_bytes.data(), long_bytes.size() - 1);

struct OwningRecord {
  std::int64_t identity;
  Star::String payload;
  std::array<std::int64_t, 4> coordinates;
};

template <typename T>
concept bench_value = std::same_as<T, std::int64_t> || std::same_as<T, std::string>
    || std::same_as<T, Star::String> || std::same_as<T, OwningRecord>;

template <typename T>
concept bench_owner = std::same_as<T, std::optional<std::int64_t>>
    || std::same_as<T, std::unique_ptr<std::int64_t>>
    || std::same_as<T, std::shared_ptr<std::int64_t>>;

template <typename Factory>
concept diagnostic_factory = std::invocable<Factory&>
    && requires(std::invoke_result_t<Factory&>& source) {
      requires std::formattable<decltype(Star::output_optional(source)), char>;
    };

template <bench_value T>
T make_value(bool long_text) {
  auto bytes = long_text ? long_payload : short_payload;
  if constexpr (std::same_as<T, std::int64_t>)
    return known_identity;
  else if constexpr (std::same_as<T, std::string>)
    return std::string(bytes);
  else if constexpr (std::same_as<T, Star::String>)
    return Star::String(bytes);
  else
    return {known_identity, Star::String(bytes), {17, -23, 0, 91}};
}

template <bench_value T>
bool validate_value(T const& value, bool long_text) {
  auto expected = long_text ? long_payload : short_payload;
  if constexpr (std::same_as<T, std::int64_t>)
    return value == known_identity;
  else if constexpr (std::same_as<T, std::string>)
    return std::string_view(value) == expected;
  else if constexpr (std::same_as<T, Star::String>)
    return std::string_view(value.utf8()) == expected;
  else
    return value.identity == known_identity && std::string_view(value.payload.utf8()) == expected
        && value.coordinates == std::array<std::int64_t, 4>{17, -23, 0, 91};
}

template <bench_value T>
void extract_engaged(benchmark::State& state, bool long_text) {
  std::optional<T> fixture(std::in_place, make_value<T>(long_text));
  if (!validate_value(*fixture, long_text)) {
    state.SkipWithError("extraction fixture differs from complete known value");
    return;
  }
  {
    // Validate after the copied optional's destruction, not merely while its
    // storage is still alive. The timed kernel below is identical for all types.
    bool source_reset = false;
    auto result = [&] {
      auto source = fixture;
      auto extracted = Star::take_optional(source);
      source_reset = !source;
      return extracted;
    }();
    if (!source_reset || !result || !validate_value(*result, long_text)
        || !validate_value(*fixture, long_text)) {
      state.SkipWithError("extraction did not reset source and retain independent complete owning value");
      return;
    }
  }
  state.SetLabel("source optional copy/setup + extraction/reset + result/source destruction timed; fixture/full owning-value validation untimed");
  for (auto _ : state) {
    (void)_;
    benchmark::ClobberMemory();
    auto source = fixture;
    benchmark::DoNotOptimize(source);
    auto result = Star::take_optional(source);
    benchmark::DoNotOptimize(result);
    benchmark::DoNotOptimize(source);
    benchmark::ClobberMemory();
  }
  state.SetItemsProcessed(state.iterations());
}

void extract_empty(benchmark::State& state) {
  std::optional<std::int64_t> source;
  {
    auto result = Star::take_optional(source);
    if (result || source || !result.error().is<Star::OutOfRangeException>()
        || std::string_view(result.error().what())
            != "(OutOfRangeException) takeOptional called on empty optional") {
      state.SkipWithError("empty extraction changed presence, error category or complete short diagnostic");
      return;
    }
  }
  state.SetLabel("empty extraction + real production traced error construction/destruction timed; fixture/category/exact short-diagnostic validation and rendering untimed");
  for (auto _ : state) {
    (void)_;
    benchmark::DoNotOptimize(source);
    auto result = Star::take_optional(source);
    benchmark::DoNotOptimize(result);
    benchmark::ClobberMemory();
  }
  state.SetItemsProcessed(state.iterations());
}

template <bench_owner Owner>
Owner make_owner(bool present) {
  if (!present)
    return {};
  if constexpr (std::same_as<Owner, std::optional<std::int64_t>>)
    return std::optional<std::int64_t>(0);
  else if constexpr (std::same_as<Owner, std::unique_ptr<std::int64_t>>)
    return std::make_unique<std::int64_t>(0);
  else
    return std::make_shared<std::int64_t>(0);
}

template <bench_owner Owner>
void borrow_owner(benchmark::State& state, bool present) {
  auto source = make_owner<Owner>(present);
  {
    auto result = Star::optional_ref(source);
    static_assert(std::same_as<decltype(result), std::optional<std::reference_wrapper<std::int64_t>>>);
    if (result.has_value() != present || static_cast<bool>(source) != present) {
      state.SkipWithError("nullable borrow changed owner/result presence");
      return;
    }
    if (present) {
      if (std::addressof(result->get()) != std::addressof(*source) || result->get() != 0) {
        state.SkipWithError("borrow did not preserve exact zero payload and owner slot identity");
        return;
      }
      *source = 73;
      if (result->get() != 73) {
        state.SkipWithError("borrow did not observe live owner mutation");
        return;
      }
      *source = 0;
    }
    if constexpr (std::same_as<Owner, std::shared_ptr<std::int64_t>>) {
      if (source.use_count() != (present ? 1 : 0)) {
        state.SkipWithError("shared-owner borrow unexpectedly acquired ownership");
        return;
      }
      auto owner = std::make_shared<std::int64_t>(0);
      std::weak_ptr<std::int64_t> weak(owner);
      auto borrowed = Star::optional_ref(owner);
      owner.reset();
      if (!weak.expired()) {
        state.SkipWithError("borrow retained shared owner after its final owning handle reset");
        return;
      }
      // A dangling reference_wrapper can be destroyed but must not be accessed.
      (void)borrowed;
    }
  }
  state.SetLabel("nullable reference_wrapper borrow construction/destruction only timed; owner allocation/lifetime/value/identity validation untimed");
  for (auto _ : state) {
    (void)_;
    benchmark::DoNotOptimize(source);
    auto result = Star::optional_ref(source);
    benchmark::DoNotOptimize(result);
    benchmark::ClobberMemory();
  }
  state.SetItemsProcessed(state.iterations());
}

void borrow_wrapper_slot(benchmark::State& state, bool present) {
  std::int64_t first = 0;
  std::int64_t second = 73;
  std::optional<std::reference_wrapper<std::int64_t>> source;
  if (present)
    source.emplace(first);
  {
    auto result = Star::optional_ref(source);
    static_assert(std::same_as<decltype(result),
        std::optional<std::reference_wrapper<std::reference_wrapper<std::int64_t>>>>);
    if (result.has_value() != present || source.has_value() != present) {
      state.SkipWithError("reference_wrapper slot borrow changed presence");
      return;
    }
    if (present) {
      if (std::addressof(result->get()) != std::addressof(*source)
          || std::addressof(result->get().get()) != std::addressof(first)
          || result->get().get() != 0) {
        state.SkipWithError("reference_wrapper output unwrapped or changed the borrowed slot");
        return;
      }
      *source = std::reference_wrapper<std::int64_t>(second);
      if (std::addressof(result->get().get()) != std::addressof(second) || result->get().get() != 73) {
        state.SkipWithError("reference_wrapper borrow failed to observe slot rebinding");
        return;
      }
      *source = std::reference_wrapper<std::int64_t>(first);
    }
  }
  state.SetLabel("nullable wrapper-slot borrow construction/destruction only timed; fixture/value/slot-identity/rebinding validation untimed");
  for (auto _ : state) {
    (void)_;
    benchmark::DoNotOptimize(source);
    auto result = Star::optional_ref(source);
    benchmark::DoNotOptimize(result);
    benchmark::ClobberMemory();
  }
  state.SetItemsProcessed(state.iterations());
}

std::uint64_t fingerprint(std::string_view bytes) {
  std::uint64_t hash = 14695981039346656037ull;
  for (unsigned char byte : bytes) {
    hash ^= byte;
    hash *= 1099511628211ull;
  }
  return hash;
}

template <diagnostic_factory Factory>
void diagnostic_output(benchmark::State& state, Factory make_source, std::string_view expected) {
  auto source = make_source();
  {
    auto result = std::format("{}", Star::output_optional(source));
    if (std::string_view(result) != expected) {
      state.SkipWithError("optional complete diagnostic bytes differ from independent literal representation");
      return;
    }
    // Check that the view follows the named owner's state instead of copying it.
    auto owner = source;
    auto view = Star::output_optional(owner);
    owner.reset();
    if (std::format("{}", view) != "Nothing") {
      state.SkipWithError("diagnostic view did not retain the live optional owner slot");
      return;
    }
  }
  state.SetLabel(std::format("verified_bytes={}; fnv1a={:016x}; borrowed view + full standard-format output allocation/render/destruction timed; fixture/complete-byte validation untimed",
      expected.size(), fingerprint(expected)));
  for (auto _ : state) {
    (void)_;
    benchmark::DoNotOptimize(source);
    auto result = std::format("{}", Star::output_optional(source));
    benchmark::DoNotOptimize(result);
    benchmark::ClobberMemory();
  }
  state.SetItemsProcessed(state.iterations());
}

template <bench_value T>
void register_extraction(std::string_view element, bool long_text) {
  auto name = std::format("Optional/Take/{}/SourceCopyExtractDestroy", element);
  benchmark::RegisterBenchmark(name.c_str(), [long_text](benchmark::State& state) {
    extract_engaged<T>(state, long_text);
  });
}

template <bench_owner Owner>
void register_borrow(std::string_view owner_name) {
  for (bool present : {false, true}) {
    auto name = std::format("Optional/Borrow/{}/{}/BorrowDestroy", owner_name, present ? "PresentZero" : "Empty");
    benchmark::RegisterBenchmark(name.c_str(), [present](benchmark::State& state) {
      borrow_owner<Owner>(state, present);
    });
  }
}

template <diagnostic_factory Factory>
void register_diagnostic(std::string_view label, Factory make_source, std::string expected) {
  auto name = std::format("Optional/Diagnostic/{}/FormatDestroy", label);
  benchmark::RegisterBenchmark(name.c_str(),
      [make_source, expected = std::move(expected)](benchmark::State& state) {
        diagnostic_output(state, make_source, expected);
      });
}

void register_workloads() {
  register_extraction<std::int64_t>("Int64", false);
  register_extraction<std::string>("StdStringShort", false);
  register_extraction<std::string>("StdStringLong", true);
  register_extraction<Star::String>("StarStringShort", false);
  register_extraction<Star::String>("StarStringLong", true);
  register_extraction<OwningRecord>("OwningRecordLong", true);
  benchmark::RegisterBenchmark("Optional/Take/EmptyInt64/TracedErrorConstructDestroy", [](benchmark::State& state) {
    extract_empty(state);
  });
  register_borrow<std::optional<std::int64_t>>("OptionalInt64");
  register_borrow<std::unique_ptr<std::int64_t>>("UniqueOwnerInt64");
  register_borrow<std::shared_ptr<std::int64_t>>("SharedOwnerInt64");
  for (bool present : {false, true}) {
    auto name = std::format("Optional/Borrow/ReferenceWrapperSlot/{}/BorrowDestroy", present ? "PresentZero" : "Empty");
    benchmark::RegisterBenchmark(name.c_str(), [present](benchmark::State& state) {
      borrow_wrapper_slot(state, present);
    });
  }
  register_diagnostic("NestedOuterEmpty", [] {
    return std::optional<std::optional<bool>>{};
  }, "Nothing");
  register_diagnostic("NestedInnerEmpty", [] {
    return std::optional<std::optional<bool>>(std::in_place, std::nullopt);
  }, "Just (Nothing)");
  register_diagnostic("NestedPresentFalse", [] {
    return std::optional<std::optional<bool>>(std::in_place, std::in_place, false);
  }, "Just (Just (0))");
  register_diagnostic("StdStringShort", [] {
    return std::optional<std::string>(std::in_place, short_payload);
  }, std::string("Just (") + std::string(short_payload) + ")");
  register_diagnostic("StdStringLong", [] {
    return std::optional<std::string>(std::in_place, long_payload);
  }, std::string("Just (") + std::string(long_payload) + ")");
  register_diagnostic("StarStringShort", [] {
    return std::optional<Star::String>(std::in_place, short_payload);
  }, std::string("Just (") + std::string(short_payload) + ")");
  register_diagnostic("StarStringLong", [] {
    return std::optional<Star::String>(std::in_place, long_payload);
  }, std::string("Just (") + std::string(long_payload) + ")");
}

class CheckedReporter : public benchmark::ConsoleReporter {
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
  auto runtime = Star::NativeRuntime::create();
  if (!runtime) {
    if (!Star::print(stderr, "{}\n", runtime.error().what()))
      return 1;
    return 1;
  }
  register_workloads();
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

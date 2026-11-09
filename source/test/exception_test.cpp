#ifdef STAR_SYSTEM_LINUX
#include <unistd.h>
#endif

import std;
import star.format;
import star.output_proxy;
import star.logging;
import star.text_formatter;
import star.list;
import star.vector;
import star.line;
import star.poly;
import star.string;

#include "gtest/gtest.h"

using namespace Star;

static_assert(!std::is_base_of_v<std::exception, StarException>);
static_assert(!std::is_constructible_v<ExceptionDiagnostic, std::exception const&>);
static_assert(!std::is_constructible_v<StarException, std::string, std::exception const&>);
static_assert(sizeof(StarException) == sizeof(std::shared_ptr<void const>));
static_assert(sizeof(ExceptionDiagnostic) == sizeof(std::shared_ptr<void const>));

struct LogFormattingProbe {
  unsigned* calls;
  std::function<void()> const* on_format = nullptr;
};

template <typename Context>
concept log_probe_format_context = requires(std::formatter<std::string_view> const& formatter, Context& context) {
  { formatter.format(std::string_view{}, context) } -> std::same_as<typename Context::iterator>;
};

template <> struct std::formatter<LogFormattingProbe> : std::formatter<std::string_view> {
  template <log_probe_format_context Context>
  auto format(LogFormattingProbe const& value, Context& context) const -> typename Context::iterator {
    ++*value.calls;
    if (value.on_format)
      (*value.on_format)();
    return std::formatter<std::string_view>::format("probe", context);
  }
};

static_assert(format_argument<LogFormattingProbe>);

namespace {
template <typename Error>
concept has_error_query = requires(StarException const& error) { error.template is<Error>(); };

template <typename Tag, typename Base>
concept has_error_category = requires { typename StarError<Tag, Base>; };

struct NestedIoTag {
  static constexpr std::string_view name() { return "NestedIoError"; }
};
using NestedIoError = StarError<NestedIoTag, IOException>;
struct DeepIoTag {
  static constexpr std::string_view name() { return "DeepIoError"; }
};
using DeepIoError = StarError<DeepIoTag, NestedIoError>;

static_assert(has_error_query<StarException> && has_error_query<DeepIoError>);
static_assert(!has_error_query<int> && !has_error_query<std::exception>);
static_assert(has_error_query<IOException const> && has_error_query<IOException&>);
static_assert(has_error_query<DeepIoError const&>);
static_assert(!has_error_query<int const&> && !has_error_query<std::exception const&>);
static_assert(has_error_category<NestedIoTag, IOException>);
static_assert(!has_error_category<NestedIoTag, IOException const&>);
static_assert(!has_error_category<NestedIoTag, std::exception>);
static_assert(!has_error_category<int, StarException>);
}

TEST(ExceptionTest, OwningCausePreservesKindsAndBothTraceModes) {
  std::string expectedShort;
  std::string expectedFull;
  bool sourceDestroyed = false;
  struct SourceException : OutOfRangeException {
    explicit SourceException(bool& destroyed)
      : OutOfRangeException("inner range failure"), destroyed(destroyed) {}
    ~SourceException() { destroyed = true; }
    bool& destroyed;
  };
  auto cause = [&] {
    SourceException source(sourceDestroyed);
    expectedShort = Star::print_exception(source, false);
    expectedFull = Star::print_exception(source, true);
    return ExceptionDiagnostic(source);
  }();
  ASSERT_TRUE(sourceDestroyed);
  EXPECT_EQ(Star::print_exception(cause, false), expectedShort);
  EXPECT_EQ(Star::print_exception(cause, true), expectedFull);
  auto wrapped = [&] {
    IOException source("outer io failure", cause);
    expectedShort = Star::print_exception(source, false);
    expectedFull = Star::print_exception(source, true);
    return ExceptionDiagnostic(source);
  }();
  EXPECT_EQ(Star::print_exception(wrapped, false), expectedShort);
  EXPECT_EQ(Star::print_exception(wrapped, true), expectedFull);
  auto output = [&] {
    IOException source("proxy owner", wrapped);
    return Star::output_exception(source, false);
  }();
  auto text = std::format("{}", output);
  auto outer = text.find("outer io failure");
  auto inner = text.find("inner range failure");
  ASSERT_NE(outer, std::string::npos);
  ASSERT_NE(inner, std::string::npos);
  EXPECT_LT(text.find("proxy owner"), outer);
  EXPECT_LT(outer, inner);
}

TEST(ExceptionTest, DisabledTraceSurvivesItsSource) {
  auto withoutTrace = [] {
    OutOfRangeException source("no trace", false);
    return ExceptionDiagnostic(source);
  }();
  EXPECT_EQ(Star::print_exception(withoutTrace, true), Star::print_exception(withoutTrace, false));
  EXPECT_NE(Star::print_exception(withoutTrace, false).find("no trace"), std::string::npos);
}

TEST(ExceptionTest, IndependentDiagnosticValuesRetainExplicitCauses) {
  ExceptionDiagnostic const outer(IOException("outer captured object", false));
  ExceptionDiagnostic const inner(StarException("different diagnostic object", false));
  auto outerText = Star::print_exception(outer, true);
  auto innerText = Star::print_exception(inner, true);
  EXPECT_NE(outerText.find("outer captured object"), std::string::npos);
  EXPECT_EQ(outerText.find("different diagnostic object"), std::string::npos);
  EXPECT_NE(innerText.find("different diagnostic object"), std::string::npos);
  EXPECT_EQ(innerText.find("outer captured object"), std::string::npos);
  OutOfRangeException wrapped("explicit outer cause", outer);
  auto wrappedText = Star::print_exception(wrapped, false);
  EXPECT_NE(wrappedText.find("outer captured object"), std::string::npos);
  EXPECT_EQ(wrappedText.find("different diagnostic object"), std::string::npos);
  IOException detached("independent third object", false);
  auto detachedText = Star::print_exception(detached, true);
  EXPECT_NE(detachedText.find("independent third object"), std::string::npos);
  EXPECT_EQ(detachedText.find("outer captured object"), std::string::npos);
  EXPECT_EQ(detachedText.find("different diagnostic object"), std::string::npos);
}

TEST(ExceptionTest, ExpectedRetainsProjectErrorClassificationAndOwnership) {
  std::expected<int, StarException> captured = std::unexpected(OutOfRangeException("stored project error", false));
  ASSERT_FALSE(captured);
  EXPECT_TRUE(captured.error().is<OutOfRangeException>());
  EXPECT_TRUE(captured.error().is<StarException>());
  EXPECT_FALSE(captured.error().is<IOException>());
  auto text = Star::print_exception(ExceptionDiagnostic(captured.error()), true);
  EXPECT_NE(text.find("OutOfRangeException"), std::string::npos);
  EXPECT_NE(text.find("stored project error"), std::string::npos);
  auto retained = std::move(captured);
  ASSERT_FALSE(retained);
  EXPECT_TRUE(retained.error().is<OutOfRangeException>());
  EXPECT_NE(std::string_view(retained.error().what()).find("stored project error"), std::string_view::npos);
}

TEST(ExceptionTest, SummaryKeepsDeepestCauseAcrossOwnedWrappersAndContexts) {
  auto captured = [] {
    OutOfRangeException inner("deepest-cause-marker", false);
    IOException outer("outer-wrapper-marker", ExceptionDiagnostic(inner));
    return ExceptionDiagnostic(std::move(outer).with_context("execution-context-marker"));
  }();
  ASSERT_TRUE(captured.diagnostic_summary());
  EXPECT_EQ(captured.diagnostic_summary()->reason, "deepest-cause-marker");
  auto brief = Star::exception_summary(captured);
  EXPECT_NE(brief.find("deepest-cause-marker"), std::string::npos);
  EXPECT_EQ(brief.find("outer-wrapper-marker"), std::string::npos);
  auto details = Star::print_exception(captured, true);
  EXPECT_NE(details.find("execution-context-marker"), std::string::npos);
  EXPECT_NE(details.find("outer-wrapper-marker"), std::string::npos);
  EXPECT_NE(details.find("deepest-cause-marker"), std::string::npos);
}

TEST(ExceptionTest, StructuredSummaryDoesNotReplaceTypedOriginalDiagnostic) {
  OutOfRangeException original("original-detail-marker\nsecond-detail-marker", false);
  auto originalText = Star::print_exception(original, true);
  ExceptionSummary summary;
  summary.kind = ExceptionSummaryKind::ScriptRuntime;
  summary.reason = "brief-reason-marker\nretained-multiline-reason";
  summary.operation = "init";
  summary.locations.push_back({"/scripts/failure.lua", 42, "actual-provider"});
  auto enriched = original.with_summary(std::move(summary));
  EXPECT_TRUE(enriched.is<OutOfRangeException>());
  EXPECT_EQ(Star::print_exception(enriched, true), originalText);
  ASSERT_TRUE(enriched.diagnostic_summary());
  EXPECT_EQ(enriched.diagnostic_summary()->kind, ExceptionSummaryKind::ScriptRuntime);
  EXPECT_EQ(enriched.diagnostic_summary()->locations.front().line, 42);
  auto brief = Star::exception_summary(enriched);
  EXPECT_NE(brief.find("brief-reason-marker"), std::string::npos);
  EXPECT_NE(brief.find("/scripts/failure.lua:42"), std::string::npos);
  EXPECT_NE(brief.find("actual-provider"), std::string::npos);
  EXPECT_EQ(brief.find("retained-multiline-reason"), std::string::npos);
  EXPECT_EQ(original.diagnostic_summary()->reason, "original-detail-marker\nsecond-detail-marker");
  auto owned = ExceptionDiagnostic(enriched);
  enriched = StarException("replacement-marker", false);
  EXPECT_EQ(owned.diagnostic_summary()->operation, "init");
  EXPECT_EQ(Star::print_exception(owned, true), originalText);
}

TEST(ExceptionTest, LoggerKeepsSeverityAndDefersFullDiagnosticsToDebug) {
  class RecordingSink : public LogSink {
  public:
    std::expected<void, StarException> log(std::string_view message, LogLevel level) override {
      if (failLevel == level)
        return std::unexpected(IOException("diagnostic-sink-failure", false));
      records.emplace_back(level, message);
      return {};
    }
    std::optional<LogLevel> failLevel;
    std::vector<std::pair<LogLevel, std::string>> records;
  };
  Logger logger;
  logger.remove_stdout_sink();
  auto sink = std::make_shared<RecordingSink>();
  logger.add_sink(sink);
  auto error = OutOfRangeException("diagnostic-reason", false).with_context("full-only-context");
  auto summary = *error.diagnostic_summary();
  summary.locations.push_back({"/scripts/diagnostic.lua", 17, "diagnostic-provider"});
  summary.locations.push_back({"/scripts/diagnostic-caller.lua", 29, "diagnostic-provider"});
  error = std::move(error).with_summary(std::move(summary));
  auto diagnostic = ExceptionDiagnostic(error);

  ASSERT_TRUE(logger.error_exception("error-operation", diagnostic));
  ASSERT_TRUE(logger.warn_exception("warning-operation", diagnostic));
  ASSERT_EQ(sink->records.size(), 2u);
  EXPECT_EQ(sink->records[0].first, LogLevel::Error);
  EXPECT_EQ(sink->records[1].first, LogLevel::Warn);
  for (auto const& record : sink->records) {
    EXPECT_NE(record.second.find("diagnostic-reason"), std::string::npos);
    EXPECT_NE(record.second.find("diagnostic-provider"), std::string::npos);
    EXPECT_NE(record.second.find("/scripts/diagnostic.lua:17"), std::string::npos);
    EXPECT_NE(record.second.find("/scripts/diagnostic-caller.lua:29"), std::string::npos);
    auto provider = record.second.find("diagnostic-provider");
    EXPECT_EQ(record.second.find("diagnostic-provider", provider + 1), std::string::npos);
    EXPECT_EQ(record.second.find("full-only-context"), std::string::npos);
  }

  sink->records.clear();
  sink->set_level(LogLevel::Debug);
  ASSERT_TRUE(logger.error_exception("debug-operation", diagnostic));
  ASSERT_EQ(sink->records.size(), 2u);
  EXPECT_EQ(sink->records[0].first, LogLevel::Error);
  EXPECT_EQ(sink->records[1].first, LogLevel::Debug);
  EXPECT_NE(sink->records[1].second.find("full-only-context"), std::string::npos);
  EXPECT_NE(sink->records[1].second.find("diagnostic-reason"), std::string::npos);

  for (auto failLevel : {LogLevel::Error, LogLevel::Debug}) {
    sink->failLevel = failLevel;
    auto failed = logger.error_exception("failing-operation", diagnostic);
    ASSERT_FALSE(failed);
    EXPECT_TRUE(failed.error().is<IOException>());
    EXPECT_EQ(failed.error().diagnostic_summary()->reason, "diagnostic-sink-failure");
  }
}

namespace {

class CallbackLogSink : public LogSink {
public:
  std::expected<void, StarException> log(std::string_view message, LogLevel level) override {
    return callback ? callback(message, level) : std::expected<void, StarException>{};
  }
  std::function<std::expected<void, StarException>(std::string_view, LogLevel)> callback;
};

#ifdef STAR_SYSTEM_LINUX
void exercise_reentrant_logging() {
  Logger logger;
  logger.remove_stdout_sink();
  auto sink = std::make_shared<CallbackLogSink>();
  auto added = std::make_shared<CallbackLogSink>();
  unsigned oldCalls = 0;
  unsigned newCalls = 0;
  std::weak_ptr<LogSink> oldLifetime = sink;
  added->callback = [&](std::string_view, LogLevel) -> std::expected<void, StarException> {
    ++newCalls;
    return {};
  };
  sink->callback = [&](std::string_view, LogLevel) -> std::expected<void, StarException> {
    ++oldCalls;
    logger.remove_sink(sink);
    sink.reset();
    EXPECT_FALSE(oldLifetime.expired());
    logger.add_sink(added);
    logger.add_sink(added);// Duplicate registration is still a set operation.
    return logger.info("nested");
  };
  logger.add_sink(sink);
  ASSERT_TRUE(logger.info("outer"));
  EXPECT_EQ(oldCalls, 1u);
  EXPECT_EQ(newCalls, 1u);// Nested call sees the new registry, outer does not.
  EXPECT_TRUE(oldLifetime.expired());
  ASSERT_TRUE(logger.info("future"));
  EXPECT_EQ(newCalls, 2u);

  // Discover the registry's actual order instead of assuming pointer hashes.
  Logger retaining;
  retaining.remove_stdout_sink();
  auto a = std::make_shared<CallbackLogSink>();
  auto b = std::make_shared<CallbackLogSink>();
  std::vector<unsigned> order;
  a->callback = [&](std::string_view, LogLevel) -> std::expected<void, StarException> {
    order.push_back(0);
    return {};
  };
  b->callback = [&](std::string_view, LogLevel) -> std::expected<void, StarException> {
    order.push_back(1);
    return {};
  };
  retaining.add_sink(a);
  retaining.add_sink(b);
  ASSERT_TRUE(retaining.info("discover"));
  ASSERT_EQ(order.size(), 2u);
  auto first = order.front() == 0 ? a : b;
  auto& removed = order.front() == 0 ? b : a;
  std::weak_ptr<LogSink> removedLifetime = removed;
  unsigned removedCalls = 0;
  removed->callback = [&](std::string_view, LogLevel) -> std::expected<void, StarException> {
    ++removedCalls;
    return {};
  };
  first->callback = [&](std::string_view, LogLevel) -> std::expected<void, StarException> {
    retaining.remove_sink(removed);
    removed.reset();
    EXPECT_FALSE(removedLifetime.expired());
    return {};
  };
  ASSERT_TRUE(retaining.info("retained"));
  EXPECT_EQ(removedCalls, 1u);
  EXPECT_TRUE(removedLifetime.expired());
  first->callback = {};
  ASSERT_TRUE(retaining.info("retired"));
  EXPECT_EQ(removedCalls, 1u);

  // Recursive dispatch must not deadlock this Logger's serialization lock.
  bool nested = false;
  first->callback = [&](std::string_view, LogLevel) -> std::expected<void, StarException> {
    if (!std::exchange(nested, true))
      return retaining.info("same sink recursion");
    return {};
  };
  ASSERT_TRUE(retaining.info("same sink outer"));

  std::atomic<bool> active{false};
  std::atomic<bool> overlap{false};
  std::atomic<unsigned> concurrentCalls{0};
  first->callback = [&](std::string_view, LogLevel) -> std::expected<void, StarException> {
    if (active.exchange(true))
      overlap = true;
    for (unsigned i = 0; i != 32; ++i)
      std::atomic_signal_fence(std::memory_order_seq_cst);
    ++concurrentCalls;
    active = false;
    return {};
  };
  std::atomic<bool> start{false};
  std::atomic<bool> failed{false};
  auto writer = [&](Logger& owner) {
    while (!start.load())
      std::this_thread::yield();
    for (unsigned i = 0; i != 1000; ++i) {
      if (!owner.info("shared sink"))
        failed = true;
    }
  };
  std::thread one(writer, std::ref(retaining));
  std::thread two(writer, std::ref(retaining));
  start = true;
  one.join();
  two.join();
  EXPECT_FALSE(failed.load());
  EXPECT_FALSE(overlap.load());
  EXPECT_EQ(concurrentCalls.load(), 2000u);

  Logger formatting;
  formatting.remove_stdout_sink();
  auto captured = std::make_shared<CallbackLogSink>();
  auto replacement = std::make_shared<CallbackLogSink>();
  unsigned capturedCalls = 0;
  unsigned replacementCalls = 0;
  unsigned formats = 0;
  captured->callback = [&](std::string_view, LogLevel) -> std::expected<void, StarException> {
    ++capturedCalls;
    return {};
  };
  replacement->callback = [&](std::string_view, LogLevel) -> std::expected<void, StarException> {
    ++replacementCalls;
    return {};
  };
  formatting.add_sink(captured);
  std::function<void()> mutate = [&] {
    formatting.remove_sink(captured);
    formatting.add_sink(replacement);
    captured->set_level(LogLevel::Error);
  };
  ASSERT_TRUE(formatting.info("{}", LogFormattingProbe{&formats, &mutate}));
  EXPECT_EQ(formats, 1u);
  EXPECT_EQ(capturedCalls, 0u);// Formatting-time threshold changes still apply.
  EXPECT_EQ(replacementCalls, 0u);
  ASSERT_TRUE(formatting.info("next snapshot"));
  EXPECT_EQ(replacementCalls, 1u);

  Logger waiting;
  waiting.remove_stdout_sink();
  auto gated = std::make_shared<CallbackLogSink>();
  std::latch entered(1);
  std::latch release(1);
  std::latch formatted(1);
  unsigned waitingCalls = 0;
  gated->callback = [&](std::string_view message, LogLevel) -> std::expected<void, StarException> {
    ++waitingCalls;
    if (message == "holding sink") {
      entered.count_down();
      release.wait();
    }
    return {};
  };
  waiting.add_sink(gated);
  std::atomic<bool> waitingFailed{false};
  std::thread holder([&] {
    if (!waiting.info("holding sink"))
      waitingFailed = true;
  });
  entered.wait();
  std::function<void()> markFormatted = [&] { formatted.count_down(); };
  unsigned waitingFormats = 0;
  std::thread waiter([&] {
    if (!waiting.info("{}", LogFormattingProbe{&waitingFormats, &markFormatted}))
      waitingFailed = true;
  });
  formatted.wait();
  gated->set_level(LogLevel::Error);
  release.count_down();
  holder.join();
  waiter.join();
  EXPECT_FALSE(waitingFailed.load());
  EXPECT_EQ(waitingFormats, 1u);
  EXPECT_EQ(waitingCalls, 1u);
}
#endif

}// namespace

#ifdef STAR_SYSTEM_LINUX
TEST(LoggingDeathTest, ReentrantRegistryAndSnapshotsRetainRemovedSinks) {
  EXPECT_EXIT(
    {
      ::alarm(5);
      exercise_reentrant_logging();
      std::_Exit(::testing::Test::HasFailure() ? 1 : 0);
    },
    ::testing::ExitedWithCode(0), "");
}
#endif

TEST(LoggingTest, DynamicLevelsFilterBeforeFormattingAndFirstFailureStopsDispatch) {
  Logger logger;
  logger.remove_stdout_sink();
  auto a = std::make_shared<CallbackLogSink>();
  auto b = std::make_shared<CallbackLogSink>();
  std::vector<unsigned> order;
  a->callback = [&](std::string_view, LogLevel) -> std::expected<void, StarException> {
    order.push_back(0);
    return {};
  };
  b->callback = [&](std::string_view, LogLevel) -> std::expected<void, StarException> {
    order.push_back(1);
    return {};
  };
  logger.add_sink(a);
  logger.add_sink(b);
  unsigned formats = 0;
  a->set_level(LogLevel::Error);
  b->set_level(LogLevel::Error);
  EXPECT_FALSE(logger.loggable(LogLevel::Info));
  ASSERT_TRUE(logger.info("{}", LogFormattingProbe{&formats}));
  EXPECT_EQ(formats, 0u);
  EXPECT_TRUE(order.empty());
  a->set_level(LogLevel::Info);
  EXPECT_TRUE(logger.loggable(LogLevel::Info));
  ASSERT_TRUE(logger.info("{}", LogFormattingProbe{&formats}));
  EXPECT_EQ(formats, 1u);
  EXPECT_EQ(order, (std::vector<unsigned>{0}));
  a->set_level(LogLevel::Error);
  EXPECT_FALSE(logger.loggable(LogLevel::Info));
  b->set_level(LogLevel::Info);
  EXPECT_TRUE(logger.loggable(LogLevel::Info));
  order.clear();
  ASSERT_TRUE(logger.info("{}", LogFormattingProbe{&formats}));
  EXPECT_EQ(formats, 2u);
  EXPECT_EQ(order, (std::vector<unsigned>{1}));

  a->set_level(LogLevel::Info);
  order.clear();
  ASSERT_TRUE(logger.info("discover failure order"));
  ASSERT_EQ(order.size(), 2u);
  auto first = order.front() == 0 ? a : b;
  first->callback = [&](std::string_view, LogLevel) -> std::expected<void, StarException> {
    order.push_back(2);
    return std::unexpected(IOException("ordered sink failure", false));
  };
  order.clear();
  auto failed = logger.info("failure-message-marker");
  ASSERT_FALSE(failed);
  EXPECT_TRUE(failed.error().is<IOException>());
  EXPECT_EQ(order, (std::vector<unsigned>{2}));
  auto report = Star::exception_report(failed.error());
  EXPECT_NE(report.find("ordered sink failure"), std::string::npos);
  EXPECT_NE(report.find("While writing log message: failure-message-marker"), std::string::npos);

  Logger lowering;
  lowering.remove_stdout_sink();
  auto x = std::make_shared<CallbackLogSink>();
  auto y = std::make_shared<CallbackLogSink>();
  std::vector<unsigned> loweredOrder;
  x->callback = [&](std::string_view, LogLevel) -> std::expected<void, StarException> {
    loweredOrder.push_back(0);
    return {};
  };
  y->callback = [&](std::string_view, LogLevel) -> std::expected<void, StarException> {
    loweredOrder.push_back(1);
    return {};
  };
  lowering.add_sink(x);
  lowering.add_sink(y);
  ASSERT_TRUE(lowering.info("discover threshold order"));
  ASSERT_EQ(loweredOrder.size(), 2u);
  auto initiallyFiltered = loweredOrder.front() == 0 ? x : y;
  initiallyFiltered->set_level(LogLevel::Error);
  loweredOrder.clear();
  std::function<void()> lowerThreshold = [&] { initiallyFiltered->set_level(LogLevel::Info); };
  unsigned loweringFormats = 0;
  ASSERT_TRUE(lowering.info("{}", LogFormattingProbe{&loweringFormats, &lowerThreshold}));
  EXPECT_EQ(loweringFormats, 1u);
  EXPECT_EQ(loweredOrder.size(), 2u);
}

TEST(LoggingTest, SpatialCapsKeepSuffixAndQueriesDrainWithoutChangingObservation) {
  SpatialLogger logger;
  Vec4B color{1, 2, 3, 4};
  EXPECT_FALSE(logger.observed());
  logger.log_point("unobserved", {1, 2}, color);
  logger.log_line("unobserved", {1, 2}, {3, 4}, color);
  logger.log_text("unobserved", "ignored", {1, 2}, color);
  for (unsigned i = 0; i != 64; ++i) {
    auto unknown = Star::format("unknown-{}", i);
    EXPECT_TRUE(logger.get_lines(unknown, false).empty());
    EXPECT_TRUE(logger.get_points(unknown, true).empty());
    EXPECT_TRUE(logger.get_text(unknown, false).empty());
  }
  EXPECT_TRUE(logger.get_points("unobserved", false).empty());
  EXPECT_TRUE(logger.get_lines("unobserved", false).empty());
  EXPECT_TRUE(logger.get_text("unobserved", false).empty());
  logger.set_observed(true);
  for (std::size_t i = 0; i != SpatialLogger::MaximumPoints + 3; ++i)
    logger.log_point("points", {static_cast<float>(i), 2}, color);
  auto copied = logger.get_points("points", false);
  ASSERT_EQ(copied.size(), SpatialLogger::MaximumPoints);
  EXPECT_EQ(copied.front().position, Vec2F(3, 2));
  EXPECT_EQ(copied.back().position, Vec2F(SpatialLogger::MaximumPoints + 2, 2));
  auto drained = logger.get_points("points", true);
  EXPECT_EQ(drained.size(), copied.size());
  EXPECT_TRUE(logger.get_points("points", true).empty());
  for (std::size_t i = 0; i != SpatialLogger::MaximumText + 2; ++i)
    logger.log_text("text", Star::format("text-{}", i), {static_cast<float>(i), 0}, color);
  auto text = logger.get_text("text", true);
  ASSERT_EQ(text.size(), SpatialLogger::MaximumText);
  EXPECT_EQ(text.front().text, "text-2");
  EXPECT_EQ(text.back().text, String(Star::format("text-{}", SpatialLogger::MaximumText + 1)));
  EXPECT_TRUE(logger.get_text("text", false).empty());
  for (std::size_t i = 0; i != SpatialLogger::MaximumLines + 2; ++i)
    logger.log_line("lines", {static_cast<float>(i), 0}, {static_cast<float>(i), 1}, color);
  auto lines = logger.get_lines("lines", true);
  ASSERT_EQ(lines.size(), SpatialLogger::MaximumLines);
  EXPECT_EQ(lines.front().begin, Vec2F(2, 0));
  EXPECT_EQ(lines.back().begin, Vec2F(SpatialLogger::MaximumLines + 1, 0));
  EXPECT_TRUE(logger.get_lines("lines", false).empty());

  List<Vec2F> vertices;
  for (std::size_t i = 0; i != SpatialLogger::MaximumLines + 3; ++i)
    vertices.push_back({static_cast<float>(i), static_cast<float>(i % 2)});
  PolyF polygon(std::move(vertices));
  logger.log_line("polygon", {-1, -1}, {-2, -2}, color);
  logger.log_poly("polygon", polygon, color);
  auto polygonLines = logger.get_lines("polygon", true);
  ASSERT_EQ(polygonLines.size(), SpatialLogger::MaximumLines);
  auto firstSide = polygon.side(3);
  auto lastSide = polygon.side(polygon.sides() - 1);
  EXPECT_EQ(polygonLines.front().begin, firstSide.min());
  EXPECT_EQ(polygonLines.front().end, firstSide.max());
  EXPECT_EQ(polygonLines.back().begin, lastSide.min());
  EXPECT_EQ(polygonLines.back().end, lastSide.max());
  logger.log_line("line-overload", Line2F({1, 2}, {3, 4}), color);
  ASSERT_EQ(logger.get_lines("line-overload", false).size(), 1u);
  logger.set_observed(false);
  EXPECT_EQ(logger.get_lines("line-overload", true).size(), 1u);
  logger.clear();
  EXPECT_FALSE(logger.observed());
  logger.set_observed(true);
  logger.log_point("clear", {1, 2}, color);
  logger.log_text("clear", "clear", {1, 2}, color);
  logger.log_line("clear", {1, 2}, {3, 4}, color);
  logger.clear();
  EXPECT_TRUE(logger.observed());
  EXPECT_TRUE(logger.get_points("clear", false).empty());
  EXPECT_TRUE(logger.get_text("clear", false).empty());
  EXPECT_TRUE(logger.get_lines("clear", false).empty());
}

TEST(ExceptionTest, CopiesAndMovesRetainTypedContextsAfterSourcesAreReplaced) {
  auto retained = [] {
    StarException source = OutOfRangeException("copy-lifetime-root", false);
    for (int i = 0; i < 128; ++i)
      source = std::move(source).with_context(Star::format("typed-context-{}", i));
    auto expected = Star::print_exception(source, false);
    auto copied = source;
    StarException assigned("replaced-before-copy", false);
    assigned = copied;
    auto moved = std::move(copied);
    StarException moveAssigned("replaced-before-move", false);
    moveAssigned = std::move(assigned);
    source = IOException("replacement-after-copy", false);
    EXPECT_TRUE(moved.is<OutOfRangeException>());
    EXPECT_TRUE(moveAssigned.is<OutOfRangeException>());
    EXPECT_EQ(Star::print_exception(moved, false), expected);
    EXPECT_EQ(Star::print_exception(moveAssigned, false), expected);
    EXPECT_EQ(std::string(moveAssigned.what()), expected);
    return std::move(moveAssigned).with_context("retained-final-context");
  }();
  EXPECT_TRUE(retained.is<OutOfRangeException>());
  EXPECT_TRUE(retained.is<StarException>());
  EXPECT_FALSE(retained.is<IOException>());
  EXPECT_EQ(retained.diagnostic_summary()->reason, "copy-lifetime-root");
  auto text = Star::print_exception(retained, true);
  EXPECT_NE(text.find("copy-lifetime-root"), std::string::npos);
  EXPECT_NE(text.find("typed-context-0"), std::string::npos);
  EXPECT_NE(text.find("typed-context-127"), std::string::npos);
  EXPECT_NE(text.find("retained-final-context"), std::string::npos);
  EXPECT_EQ(text.find("replacement-after-copy"), std::string::npos);
}

TEST(ExceptionTest, OriginOwnsItsTypeNameAndMessage) {
  struct NamedError : StarException {
    NamedError(std::string_view type, std::string message)
      : StarException(type, std::move(message), false) {}
  };
  auto owned = [] {
    std::string name = "dynamic-error-kind";
    NamedError source(name, "dynamic-error-message");
    return ExceptionDiagnostic(source);
  }();
  auto text = Star::print_exception(owned, true);
  EXPECT_NE(text.find("dynamic-error-kind"), std::string::npos);
  EXPECT_NE(text.find("dynamic-error-message"), std::string::npos);
}

TEST(ExceptionTest, RelatedDiagnosticsRetainPrimaryClassificationAndBothTraceModes) {
  OutOfRangeException primary("primary-related-marker");
  IOException secondary("secondary-related-marker");
  auto primaryShort = Star::print_exception(primary, false);
  auto secondaryShort = Star::print_exception(secondary, false);
  auto primaryFull = Star::print_exception(primary, true);
  auto secondaryFull = Star::print_exception(secondary, true);
  auto related = primary.with_related(secondary, "related-operation-marker");
  auto shortText = Star::print_exception(related, false);
  auto fullText = Star::print_exception(related, true);
  EXPECT_TRUE(related.is<OutOfRangeException>());
  EXPECT_TRUE(related.is<StarException>());
  EXPECT_FALSE(related.is<IOException>());
  ASSERT_TRUE(related.diagnostic_summary());
  EXPECT_EQ(related.diagnostic_summary()->reason, "primary-related-marker");
  EXPECT_NE(shortText.find(primaryShort), std::string::npos);
  EXPECT_NE(shortText.find(secondaryShort), std::string::npos);
  EXPECT_NE(shortText.find("related-operation-marker"), std::string::npos);
  EXPECT_NE(fullText.find(primaryFull), std::string::npos);
  EXPECT_NE(fullText.find(secondaryFull), std::string::npos);
  EXPECT_NE(fullText.find("0x"), std::string::npos);
  EXPECT_EQ(std::string(related.what()), shortText);
  auto owned = ExceptionDiagnostic(std::move(related).with_related(
      StarException("third-related-marker", false), "second-related-operation"));
  primary = OutOfRangeException("replaced-primary", false);
  secondary = IOException("replaced-secondary", false);
  auto ownedFull = Star::print_exception(owned, true);
  EXPECT_NE(ownedFull.find(primaryFull), std::string::npos);
  EXPECT_NE(ownedFull.find(secondaryFull), std::string::npos);
  EXPECT_NE(ownedFull.find("third-related-marker"), std::string::npos);
}

TEST(ExceptionTest, RelatedDiagnosticCanOwnItsRvaluePrimaryAsSecondary) {
  StarException primary = OutOfRangeException("self-related-marker", false);
  auto related = std::move(primary).with_related(primary, "self-related-context");
  EXPECT_TRUE(related.is<OutOfRangeException>());
  auto text = Star::print_exception(related, true);
  auto first = text.find("self-related-marker");
  ASSERT_NE(first, std::string::npos);
  EXPECT_NE(text.find("self-related-marker", first + 1), std::string::npos);
  EXPECT_EQ(Star::print_exception(related, false), text);
}

TEST(ExceptionTest, AliasedRvalueSummaryKeepsOriginalDiagnosticsIndependent) {
  StarException source = OutOfRangeException("unchanged-original-details", false);
  auto copy = source;
  auto original = ExceptionDiagnostic(source);
  auto proxy = Star::output_exception(original, false);
  auto originalText = std::string(copy.what());
  auto originalWhat = copy.what();
  auto summary = *source.diagnostic_summary();
  summary.reason = "independent-summary";
  summary.operation = "summary-operation";
  auto enriched = std::move(source).with_summary(std::move(summary));
  EXPECT_TRUE(enriched.is<OutOfRangeException>());
  EXPECT_EQ(enriched.diagnostic_summary()->reason, "independent-summary");
  EXPECT_EQ(copy.diagnostic_summary()->reason, "unchanged-original-details");
  EXPECT_EQ(original.diagnostic_summary()->reason, "unchanged-original-details");
  EXPECT_EQ(std::string(copy.what()), originalWhat);
  EXPECT_EQ(Star::print_exception(enriched, true), originalText);
  EXPECT_EQ(std::format("{}", proxy), originalText);

  auto related = std::move(enriched).with_related(enriched, "self-related-summary");
  EXPECT_TRUE(related.is<OutOfRangeException>());
  EXPECT_EQ(related.diagnostic_summary()->operation, "summary-operation");
  EXPECT_EQ(copy.diagnostic_summary()->operation, "");
  auto text = Star::print_exception(related, false);
  auto first = text.find(originalText);
  ASSERT_NE(first, std::string::npos);
  EXPECT_NE(text.find(originalText, first + originalText.size()), std::string::npos);
  EXPECT_EQ(text.find("independent-summary"), std::string::npos);
}

TEST(ExceptionTest, NestedCategoriesAreFinalBeforeSlicingOrEnrichment) {
  auto check = [](StarException const& error) {
    EXPECT_TRUE(error.is<DeepIoError>());
    EXPECT_TRUE(error.is<DeepIoError const&>());
    EXPECT_TRUE(error.is<NestedIoError>());
    EXPECT_TRUE(error.is<NestedIoError const&>());
    EXPECT_TRUE(error.is<IOException>());
    EXPECT_TRUE(error.is<StarException>());
    EXPECT_FALSE(error.is<OutOfRangeException>());
    EXPECT_FALSE(error.is<OutOfRangeException const&>());
  };
  check(DeepIoError());
  check(DeepIoError("typed-message", false));
  check(DeepIoError::format("typed-format-{}", 7));
  ExceptionDiagnostic cause(OutOfRangeException("deepest-nested-cause", false));
  check(DeepIoError(cause));
  DeepIoError leaf("typed-outer", cause);
  check(leaf);
  IOException sliced = leaf;
  check(sliced);
  std::expected<int, StarException> result = std::unexpected(sliced);
  ASSERT_FALSE(result);
  check(result.error());
  auto summary = *result.error().diagnostic_summary();
  EXPECT_EQ(summary.reason, "deepest-nested-cause");
  summary.operation = "nested-operation";
  auto enriched = std::move(result.error()).with_summary(std::move(summary))
      .with_context("nested-context")
      .with_related(IOException("related-base", false), "nested-related");
  check(enriched);
  EXPECT_EQ(enriched.diagnostic_summary()->reason, "deepest-nested-cause");
  EXPECT_EQ(enriched.diagnostic_summary()->operation, "nested-operation");
  EXPECT_EQ(sliced.diagnostic_summary()->operation, "");
}

TEST(ExceptionTest, ConcurrentFirstRenderingAndCopiesAreDeterministic) {
  auto shared = OutOfRangeException("concurrent-origin-marker")
      .with_context("concurrent-context-marker")
      .with_related(IOException("concurrent-secondary-marker"), "concurrent-related-marker");
  constexpr std::size_t ThreadCount = 8;
  std::array<std::string, ThreadCount> shorts;
  std::array<std::string, ThreadCount> fulls;
  std::array<std::string, ThreadCount> whats;
  std::array<bool, ThreadCount> stable{};
  std::barrier start(static_cast<std::ptrdiff_t>(ThreadCount));
  std::vector<std::thread> threads;
  threads.reserve(ThreadCount);
  for (std::size_t i = 0; i < ThreadCount; ++i) {
    threads.emplace_back([&, i, copy = shared] {
      start.arrive_and_wait();
      // Race both cold modes rather than warming either cache on the owner.
      if (i % 2 == 0) {
        shorts[i] = Star::print_exception(copy, false);
        fulls[i] = Star::print_exception(copy, true);
      } else {
        fulls[i] = Star::print_exception(copy, true);
        shorts[i] = Star::print_exception(copy, false);
      }
      whats[i] = copy.what();
      stable[i] = true;
      for (int repeat = 0; repeat < 2; ++repeat) {
        auto diagnostic = ExceptionDiagnostic(copy);
        stable[i] = stable[i] && Star::print_exception(diagnostic, true) == fulls[i]
            && Star::print_exception(diagnostic, false) == shorts[i] && copy.what() == whats[i]
            && diagnostic.diagnostic_summary()->reason == "concurrent-origin-marker";
      }
    });
  }
  for (auto& thread : threads)
    thread.join();
  for (std::size_t i = 0; i < ThreadCount; ++i) {
    EXPECT_TRUE(stable[i]);
    EXPECT_EQ(shorts[i], shorts[0]);
    EXPECT_EQ(fulls[i], fulls[0]);
    EXPECT_EQ(whats[i], shorts[0]);
  }
  EXPECT_NE(fulls[0].find("concurrent-origin-marker"), std::string::npos);
  EXPECT_NE(fulls[0].find("concurrent-secondary-marker"), std::string::npos);
  EXPECT_NE(fulls[0].find("0x"), std::string::npos);
}

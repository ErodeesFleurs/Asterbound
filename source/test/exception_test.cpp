import std;
import star.format;
import star.output_proxy;
import star.logging;
import star.text_formatter;

#include "gtest/gtest.h"

using namespace Star;

static_assert(!std::is_base_of_v<std::exception, StarException>);
static_assert(!std::is_constructible_v<ExceptionDiagnostic, std::exception const&>);
static_assert(!std::is_constructible_v<StarException, std::string, std::exception const&>);
static_assert(sizeof(StarException) == sizeof(std::shared_ptr<void const>));
static_assert(sizeof(ExceptionDiagnostic) == sizeof(std::shared_ptr<void const>));

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
  logger.removeStdoutSink();
  auto sink = std::make_shared<RecordingSink>();
  logger.addSink(sink);
  auto error = OutOfRangeException("diagnostic-reason", false).with_context("full-only-context");
  auto summary = *error.diagnostic_summary();
  summary.locations.push_back({"/scripts/diagnostic.lua", 17, "diagnostic-provider"});
  summary.locations.push_back({"/scripts/diagnostic-caller.lua", 29, "diagnostic-provider"});
  error = std::move(error).with_summary(std::move(summary));
  auto diagnostic = ExceptionDiagnostic(error);

  ASSERT_TRUE(logger.errorException("error-operation", diagnostic));
  ASSERT_TRUE(logger.warnException("warning-operation", diagnostic));
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
  sink->setLevel(LogLevel::Debug);
  ASSERT_TRUE(logger.errorException("debug-operation", diagnostic));
  ASSERT_EQ(sink->records.size(), 2u);
  EXPECT_EQ(sink->records[0].first, LogLevel::Error);
  EXPECT_EQ(sink->records[1].first, LogLevel::Debug);
  EXPECT_NE(sink->records[1].second.find("full-only-context"), std::string::npos);
  EXPECT_NE(sink->records[1].second.find("diagnostic-reason"), std::string::npos);

  for (auto failLevel : {LogLevel::Error, LogLevel::Debug}) {
    sink->failLevel = failLevel;
    auto failed = logger.errorException("failing-operation", diagnostic);
    ASSERT_FALSE(failed);
    EXPECT_TRUE(failed.error().is<IOException>());
    EXPECT_EQ(failed.error().diagnostic_summary()->reason, "diagnostic-sink-failure");
  }
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

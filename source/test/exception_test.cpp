import std;
import star.format;
import star.output_proxy;
import star.logging;

#include "gtest/gtest.h"

using namespace Star;

static_assert(!std::is_base_of_v<std::exception, StarException>);
static_assert(!std::is_constructible_v<ExceptionDiagnostic, std::exception const&>);
static_assert(!std::is_constructible_v<StarException, std::string, std::exception const&>);

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
    expectedShort = printException(source, false);
    expectedFull = printException(source, true);
    return ExceptionDiagnostic(source);
  }();
  ASSERT_TRUE(sourceDestroyed);
  EXPECT_EQ(printException(cause, false), expectedShort);
  EXPECT_EQ(printException(cause, true), expectedFull);
  auto wrapped = [&] {
    IOException source("outer io failure", cause);
    expectedShort = printException(source, false);
    expectedFull = printException(source, true);
    return ExceptionDiagnostic(source);
  }();
  EXPECT_EQ(printException(wrapped, false), expectedShort);
  EXPECT_EQ(printException(wrapped, true), expectedFull);
  auto output = [&] {
    IOException source("proxy owner", wrapped);
    return outputException(source, false);
  }();
  std::ostringstream os;
  os << output;
  auto text = os.str();
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
  EXPECT_EQ(printException(withoutTrace, true), printException(withoutTrace, false));
  EXPECT_NE(printException(withoutTrace, false).find("no trace"), std::string::npos);
}

TEST(ExceptionTest, IndependentDiagnosticValuesRetainExplicitCauses) {
  ExceptionDiagnostic const outer(IOException("outer captured object", false));
  ExceptionDiagnostic const inner(StarException("different diagnostic object", false));
  auto outerText = printException(outer, true);
  auto innerText = printException(inner, true);
  EXPECT_NE(outerText.find("outer captured object"), std::string::npos);
  EXPECT_EQ(outerText.find("different diagnostic object"), std::string::npos);
  EXPECT_NE(innerText.find("different diagnostic object"), std::string::npos);
  EXPECT_EQ(innerText.find("outer captured object"), std::string::npos);
  OutOfRangeException wrapped("explicit outer cause", outer);
  auto wrappedText = printException(wrapped, false);
  EXPECT_NE(wrappedText.find("outer captured object"), std::string::npos);
  EXPECT_EQ(wrappedText.find("different diagnostic object"), std::string::npos);
  IOException detached("independent third object", false);
  auto detachedText = printException(detached, true);
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
  auto text = printException(ExceptionDiagnostic(captured.error()), true);
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
    return ExceptionDiagnostic(std::move(outer).withContext("execution-context-marker"));
  }();
  ASSERT_TRUE(captured.diagnosticSummary());
  EXPECT_EQ(captured.diagnosticSummary()->reason, "deepest-cause-marker");
  auto brief = exceptionSummary(captured);
  EXPECT_NE(brief.find("deepest-cause-marker"), std::string::npos);
  EXPECT_EQ(brief.find("outer-wrapper-marker"), std::string::npos);
  auto details = printException(captured, true);
  EXPECT_NE(details.find("execution-context-marker"), std::string::npos);
  EXPECT_NE(details.find("outer-wrapper-marker"), std::string::npos);
  EXPECT_NE(details.find("deepest-cause-marker"), std::string::npos);
}

TEST(ExceptionTest, StructuredSummaryDoesNotReplaceTypedOriginalDiagnostic) {
  OutOfRangeException original("original-detail-marker\nsecond-detail-marker", false);
  auto originalText = printException(original, true);
  ExceptionSummary summary;
  summary.kind = ExceptionSummaryKind::ScriptRuntime;
  summary.reason = "brief-reason-marker\nretained-multiline-reason";
  summary.operation = "init";
  summary.locations.push_back({"/scripts/failure.lua", 42, "actual-provider"});
  auto enriched = original.withSummary(std::move(summary));
  EXPECT_TRUE(enriched.is<OutOfRangeException>());
  EXPECT_EQ(printException(enriched, true), originalText);
  ASSERT_TRUE(enriched.diagnosticSummary());
  EXPECT_EQ(enriched.diagnosticSummary()->kind, ExceptionSummaryKind::ScriptRuntime);
  EXPECT_EQ(enriched.diagnosticSummary()->locations.front().line, 42);
  auto brief = exceptionSummary(enriched);
  EXPECT_NE(brief.find("brief-reason-marker"), std::string::npos);
  EXPECT_NE(brief.find("/scripts/failure.lua:42"), std::string::npos);
  EXPECT_NE(brief.find("actual-provider"), std::string::npos);
  EXPECT_EQ(brief.find("retained-multiline-reason"), std::string::npos);
  EXPECT_EQ(original.diagnosticSummary()->reason, "original-detail-marker\nsecond-detail-marker");
  auto owned = ExceptionDiagnostic(enriched);
  enriched = StarException("replacement-marker", false);
  EXPECT_EQ(owned.diagnosticSummary()->operation, "init");
  EXPECT_EQ(printException(owned, true), originalText);
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
  auto error = OutOfRangeException("diagnostic-reason", false).withContext("full-only-context");
  auto summary = *error.diagnosticSummary();
  summary.locations.push_back({"/scripts/diagnostic.lua", 17, "diagnostic-provider"});
  summary.locations.push_back({"/scripts/diagnostic-caller.lua", 29, "diagnostic-provider"});
  error = std::move(error).withSummary(std::move(summary));
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
    EXPECT_EQ(failed.error().diagnosticSummary()->reason, "diagnostic-sink-failure");
  }
}

TEST(ExceptionTest, CopiesAndMovesRetainTypedContextsAfterSourcesAreReplaced) {
  auto retained = [] {
    StarException source = OutOfRangeException("copy-lifetime-root", false);
    for (int i = 0; i < 128; ++i)
      source = std::move(source).withContext(strf("typed-context-{}", i));
    auto expected = printException(source, false);
    auto copied = source;
    StarException assigned("replaced-before-copy", false);
    assigned = copied;
    auto moved = std::move(copied);
    StarException moveAssigned("replaced-before-move", false);
    moveAssigned = std::move(assigned);
    source = IOException("replacement-after-copy", false);
    EXPECT_TRUE(moved.is<OutOfRangeException>());
    EXPECT_TRUE(moveAssigned.is<OutOfRangeException>());
    EXPECT_EQ(printException(moved, false), expected);
    EXPECT_EQ(printException(moveAssigned, false), expected);
    EXPECT_EQ(std::string(moveAssigned.what()), expected);
    return std::move(moveAssigned).withContext("retained-final-context");
  }();
  EXPECT_TRUE(retained.is<OutOfRangeException>());
  EXPECT_TRUE(retained.is<StarException>());
  EXPECT_FALSE(retained.is<IOException>());
  EXPECT_EQ(retained.diagnosticSummary()->reason, "copy-lifetime-root");
  auto text = printException(retained, true);
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
  auto text = printException(owned, true);
  EXPECT_NE(text.find("dynamic-error-kind"), std::string::npos);
  EXPECT_NE(text.find("dynamic-error-message"), std::string::npos);
}

TEST(ExceptionTest, RelatedDiagnosticsRetainPrimaryClassificationAndBothTraceModes) {
  OutOfRangeException primary("primary-related-marker");
  IOException secondary("secondary-related-marker");
  auto primaryShort = printException(primary, false);
  auto secondaryShort = printException(secondary, false);
  auto primaryFull = printException(primary, true);
  auto secondaryFull = printException(secondary, true);
  auto related = primary.withRelated(secondary, "related-operation-marker");
  auto shortText = printException(related, false);
  auto fullText = printException(related, true);
  EXPECT_TRUE(related.is<OutOfRangeException>());
  EXPECT_TRUE(related.is<StarException>());
  EXPECT_FALSE(related.is<IOException>());
  ASSERT_TRUE(related.diagnosticSummary());
  EXPECT_EQ(related.diagnosticSummary()->reason, "primary-related-marker");
  EXPECT_NE(shortText.find(primaryShort), std::string::npos);
  EXPECT_NE(shortText.find(secondaryShort), std::string::npos);
  EXPECT_NE(shortText.find("related-operation-marker"), std::string::npos);
  EXPECT_NE(fullText.find(primaryFull), std::string::npos);
  EXPECT_NE(fullText.find(secondaryFull), std::string::npos);
  EXPECT_NE(fullText.find("0x"), std::string::npos);
  EXPECT_EQ(std::string(related.what()), shortText);
  auto owned = ExceptionDiagnostic(std::move(related).withRelated(
      StarException("third-related-marker", false), "second-related-operation"));
  primary = OutOfRangeException("replaced-primary", false);
  secondary = IOException("replaced-secondary", false);
  auto ownedFull = printException(owned, true);
  EXPECT_NE(ownedFull.find(primaryFull), std::string::npos);
  EXPECT_NE(ownedFull.find(secondaryFull), std::string::npos);
  EXPECT_NE(ownedFull.find("third-related-marker"), std::string::npos);
}

TEST(ExceptionTest, RelatedDiagnosticCanOwnItsRvaluePrimaryAsSecondary) {
  StarException primary = OutOfRangeException("self-related-marker", false);
  auto related = std::move(primary).withRelated(primary, "self-related-context");
  EXPECT_TRUE(related.is<OutOfRangeException>());
  auto text = printException(related, true);
  auto first = text.find("self-related-marker");
  ASSERT_NE(first, std::string::npos);
  EXPECT_NE(text.find("self-related-marker", first + 1), std::string::npos);
  EXPECT_EQ(printException(related, false), text);
}

TEST(ExceptionTest, ConcurrentFirstRenderingAndCopiesAreDeterministic) {
  auto shared = OutOfRangeException("concurrent-origin-marker")
      .withContext("concurrent-context-marker")
      .withRelated(IOException("concurrent-secondary-marker"), "concurrent-related-marker");
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
        shorts[i] = printException(copy, false);
        fulls[i] = printException(copy, true);
      } else {
        fulls[i] = printException(copy, true);
        shorts[i] = printException(copy, false);
      }
      whats[i] = copy.what();
      stable[i] = true;
      for (int repeat = 0; repeat < 2; ++repeat) {
        auto diagnostic = ExceptionDiagnostic(copy);
        stable[i] = stable[i] && printException(diagnostic, true) == fulls[i]
            && printException(diagnostic, false) == shorts[i] && copy.what() == whats[i]
            && diagnostic.diagnosticSummary()->reason == "concurrent-origin-marker";
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

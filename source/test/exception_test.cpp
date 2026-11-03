import std;
import star.format;
import star.output_proxy;

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

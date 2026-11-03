#include <stdlib.h>
#ifdef STAR_USE_CPPTRACE
#include "cpptrace/basic.hpp"
#include "cpptrace/formatting.hpp"
#else
#include <execinfo.h>
#endif
import star.format;
import star.logging;

import std;

import star.output_proxy;
import star.array;
namespace Star {

#ifdef STAR_USE_CPPTRACE

static inline std::string captureBacktrace() {
  auto formatter = cpptrace::formatter{}
    .paths(cpptrace::formatter::path_mode::basename);
  std::ostringstream out;
  formatter.print(out, cpptrace::generate_trace());
  return out.str();
}

#else

static std::size_t const StackLimit = 256;

typedef std::pair<Array<void*, StackLimit>, std::size_t> StackCapture;

inline StackCapture captureStack() {
  StackCapture stackCapture;
  stackCapture.second = backtrace(stackCapture.first.ptr(), StackLimit);
  return stackCapture;
}

OutputProxy outputStack(StackCapture stack) {
  return OutputProxy([stack = std::move(stack)](std::ostream & os) {
      char** symbols = backtrace_symbols(stack.first.ptr(), stack.second);
      for (std::size_t i = 0; i < stack.second; ++i) {
        os << symbols[i];
        if (i + 1 < stack.second)
          os << std::endl;
      }

      if (stack.second == StackLimit)
        os << std::endl << "[Stack Output Limit Reached]";

      std::free(symbols);
    });
}
  
#endif

StarException::StarException() noexcept
  : StarException(std::string("StarException")) {}

StarException::~StarException() noexcept {}

StarException::StarException(std::string message, bool genStackTrace) noexcept
  : StarException("StarException", std::move(message), genStackTrace) {}

StarException::StarException(ExceptionDiagnostic cause) noexcept
  : StarException("StarException", std::string(), std::move(cause)) {}

StarException::StarException(std::string message, ExceptionDiagnostic cause) noexcept
  : StarException("StarException", std::move(message), std::move(cause)) {}

const char* StarException::what() const noexcept {
  if (m_whatBuffer.empty()) {
    std::ostringstream os;
    m_printException(os, false);
    m_whatBuffer = os.str();
  }
  return m_whatBuffer.c_str();
}

StarException::StarException(std::string_view type, std::string message, bool genStackTrace) noexcept
  : StarException(type, std::move(message), genStackTrace, {}) {}

StarException::StarException(std::string_view type, std::string message, bool genStackTrace,
    std::shared_ptr<ExceptionSummary const> inheritedSummary) noexcept
  : m_summary(std::move(inheritedSummary)) {
  if (!m_summary) {
    ExceptionSummary summary;
    summary.category = type;
    summary.reason = message;
    m_summary = std::make_shared<ExceptionSummary const>(std::move(summary));
  }
#ifdef STAR_USE_CPPTRACE
  auto printException = [](std::ostream& os, bool fullStacktrace, std::string_view type, std::string const& message, std::string const& stack) {
#else
  auto printException = [](std::ostream& os, bool fullStacktrace, std::string_view type, std::string const& message, std::optional<StackCapture> const& stack) {
#endif
    os << "(" << type << ")";
    if (!message.empty())
      os << " " << message;

#ifdef STAR_USE_CPPTRACE
    if (fullStacktrace && !stack.empty()) {
      os << std::endl;
      os << stack;
    }
#else
    if (fullStacktrace && stack) {
      os << std::endl;
      os << outputStack(*stack);
    }
#endif
  };
#ifdef STAR_USE_CPPTRACE
  m_printException = std::bind(printException, std::placeholders::_1, std::placeholders::_2, type, std::move(message), genStackTrace ? captureBacktrace() : std::string());
#else
  m_printException = std::bind(printException, std::placeholders::_1, std::placeholders::_2, type, std::move(message), genStackTrace ? captureStack() : std::optional<StackCapture>());
#endif
}

StarException::StarException(std::string_view type, std::string message, ExceptionDiagnostic cause) noexcept
  : StarException(type, std::move(message), true, std::move(cause.m_summary)) {
  auto printException = [](std::ostream& os, bool fullStacktrace, std::function<void(std::ostream&, bool)> const& self, std::function<void(std::ostream&, bool)> const& cause) {
    self(os, fullStacktrace);
    os << std::endl << "Caused by: ";
    cause(os, fullStacktrace);
  };

  m_printException = std::bind(printException, std::placeholders::_1, std::placeholders::_2, m_printException, std::move(cause.m_printException));
}

ExceptionDiagnostic::ExceptionDiagnostic(StarException const& exception)
  : m_printException(exception.m_printException), m_summary(exception.m_summary) {}


std::string printException(ExceptionDiagnostic const& e, bool fullStacktrace) {
  std::ostringstream os;
  printException(os, e, fullStacktrace);
  return os.str();
}

void printException(std::ostream& os, ExceptionDiagnostic const& e, bool fullStacktrace) {
  e.m_printException(os, fullStacktrace);
}

OutputProxy outputException(ExceptionDiagnostic const& e, bool fullStacktrace) {
  return OutputProxy(std::bind(e.m_printException, std::placeholders::_1, fullStacktrace));
}

void printStack(std::string_view message) {
#ifdef STAR_USE_CPPTRACE
  EmergencyLog::info("Stack Trace ({})...\n{}", message, captureBacktrace());
#else
  EmergencyLog::info("Stack Trace ({})...\n{}", message, outputStack(captureStack()));
#endif
}

void fatalError(std::string_view message, bool showStackTrace) {
  if (showStackTrace)
#ifdef STAR_USE_CPPTRACE
    EmergencyLog::error("Fatal Error: {}\n{}", message, captureBacktrace());
#else
    EmergencyLog::error("Fatal Error: {}\n{}", message, outputStack(captureStack()));
#endif
  else
    EmergencyLog::error("Fatal Error: {}", message);

  std::abort();
}


}

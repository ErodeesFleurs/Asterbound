module;
import std;

import star.type_identity;

namespace Star {
struct OutputProxy;
#ifndef NDEBUG
inline constexpr bool DebugEnabled = true;
#else
inline constexpr bool DebugEnabled = false;
#endif



template <typename... T>
std::string strf(std::format_string<T...> fmt, T&&... args);


template <typename Char, typename... T>
std::string strfChecked(std::basic_format_string<Char, T...> fmt, T const&... args);

class StarException;

// A diagnostic is an immutable graph. Only its lazily rendered views mutate;
// child emission deliberately bypasses whole-chain caches so rendering a deep
// chain does not materialize and retain every suffix of that chain.
class ExceptionPayload {
public:
  explicit ExceptionPayload(std::function<void(std::ostream&, bool)> printer)
    : m_printer(std::move(printer)) {}

  void emit(std::ostream& os, bool fullStacktrace) const {
    m_printer(os, fullStacktrace);
  }

  std::string const& rendered(bool fullStacktrace) const {
    auto index = fullStacktrace ? 1u : 0u;
    std::call_once(m_renderOnce[index], [this, index, fullStacktrace] {
      std::ostringstream os;
      emit(os, fullStacktrace);
      m_rendered[index] = os.str();
    });
    return m_rendered[index];
  }

private:
  std::function<void(std::ostream&, bool)> const m_printer;
  mutable std::array<std::once_flag, 2> m_renderOnce;
  mutable std::array<std::string, 2> m_rendered;
};

enum class ExceptionSummaryKind { Native, ScriptRuntime, ScriptSyntax, PatchRuntime, PatchSyntax, Resource };

struct ExceptionSummary {
  struct Location {
    std::string source;
    int line = -1;
    std::string provider;
  };
  ExceptionSummaryKind kind = ExceptionSummaryKind::Native;
  std::string category;
  std::string reason;
  std::string operation;
  std::vector<Location> locations;
  std::string asset;
  std::string assetProvider;
  std::string patch;
  std::string patchProvider;
};

// Owning diagnostics remain valid after the originating error value is gone.
class ExceptionDiagnostic {
public:
  ExceptionDiagnostic(StarException const& exception);
  std::shared_ptr<ExceptionSummary const> const& diagnosticSummary() const noexcept { return m_summary; }

  friend void printException(std::ostream& os, ExceptionDiagnostic const& e, bool fullStacktrace);
  friend std::string printException(ExceptionDiagnostic const& e, bool fullStacktrace);
  friend OutputProxy outputException(ExceptionDiagnostic const& e, bool fullStacktrace);

private:
  friend class StarException;
  std::shared_ptr<ExceptionPayload const> m_payload;
  std::shared_ptr<ExceptionSummary const> m_summary;
};

class StarException {
public:
  template <typename... Args>
  static StarException format(std::format_string<Args...> fmt, Args const&... args);

  StarException() noexcept : StarException(std::string("StarException")) {}
  ~StarException() noexcept = default;
  StarException(StarException const&) = default;
  StarException& operator=(StarException const&) = default;
  StarException(StarException&&) noexcept = default;
  StarException& operator=(StarException&&) noexcept = default;

  explicit StarException(std::string message, bool genStackTrace = true) noexcept
    : StarException("StarException", std::move(message), genStackTrace) {}
  explicit StarException(ExceptionDiagnostic cause) noexcept
    : StarException("StarException", std::string(), std::move(cause)) {}
  StarException(std::string message, ExceptionDiagnostic cause) noexcept
    : StarException("StarException", std::move(message), std::move(cause)) {}

  char const* what() const noexcept { return m_payload->rendered(false).c_str(); }

  template <typename Error>
  bool is() const noexcept {
    return m_matchesErrorType(typeKey<Error>());
  }

  static bool matchesErrorType(TypeKey key) noexcept {
    return key == typeKey<StarException>();
  }

  std::shared_ptr<ExceptionSummary const> const& diagnosticSummary() const noexcept { return m_summary; }

  StarException withSummary(ExceptionSummary summary) const& {
    auto result = *this;
    result.m_summary = std::make_shared<ExceptionSummary const>(std::move(summary));
    return result;
  }

  StarException withSummary(ExceptionSummary summary) && {
    m_summary = std::make_shared<ExceptionSummary const>(std::move(summary));
    return std::move(*this);
  }

  StarException withContext(std::string context) const& {
    return StarException(
        [context = std::move(context), cause = m_payload](std::ostream& os, bool fullStacktrace) {
          os << context << "\nCaused by: ";
          cause->emit(os, fullStacktrace);
        },
        m_matchesErrorType, m_summary);
  }

  StarException withContext(std::string context) && {
    return StarException(
        [context = std::move(context), cause = std::move(m_payload)](std::ostream& os, bool fullStacktrace) {
          os << context << "\nCaused by: ";
          cause->emit(os, fullStacktrace);
        },
        m_matchesErrorType, std::move(m_summary));
  }

  StarException withRelated(StarException const& secondary, std::string context) const& {
    return StarException(
        [primary = m_payload, secondary = secondary.m_payload, context = std::move(context)](
            std::ostream& os, bool fullStacktrace) {
          primary->emit(os, fullStacktrace);
          os << "\n" << context << "\nRelated error: ";
          secondary->emit(os, fullStacktrace);
        },
        m_matchesErrorType, m_summary);
  }

  StarException withRelated(StarException const& secondary, std::string context) && {
    // Copy the secondary handle first: secondary may alias this value.
    auto related = secondary.m_payload;
    return StarException(
        [primary = std::move(m_payload), secondary = std::move(related), context = std::move(context)](
            std::ostream& os, bool fullStacktrace) {
          primary->emit(os, fullStacktrace);
          os << "\n" << context << "\nRelated error: ";
          secondary->emit(os, fullStacktrace);
        },
        m_matchesErrorType, std::move(m_summary));
  }

  friend class ExceptionDiagnostic;

protected:
  StarException(std::string_view type, std::string message, bool genStackTrace = true) noexcept
    : StarException(type, std::move(message), genStackTrace, {}) {}
  StarException(std::string_view type, std::string message, ExceptionDiagnostic cause) noexcept
    : StarException(type, std::move(message), true, std::move(cause.m_summary)) {
    m_payload = std::make_shared<ExceptionPayload const>(
        [self = std::move(m_payload), cause = std::move(cause.m_payload)](std::ostream& os, bool fullStacktrace) {
          self->emit(os, fullStacktrace);
          os << "\nCaused by: ";
          cause->emit(os, fullStacktrace);
        });
  }
  StarException(std::string_view type, std::string message, bool genStackTrace,
      std::shared_ptr<ExceptionSummary const> inheritedSummary) noexcept
    : m_summary(std::move(inheritedSummary)) {
    if (!m_summary) {
      ExceptionSummary summary;
      summary.category = type;
      summary.reason = message;
      m_summary = std::make_shared<ExceptionSummary const>(std::move(summary));
    }
    auto stack = genStackTrace ? std::make_shared<ExceptionPayload const>(captureOriginStack()) : nullptr;
    m_payload = std::make_shared<ExceptionPayload const>(
        [type = std::string(type), message = std::move(message), stack = std::move(stack)](
            std::ostream& os, bool fullStacktrace) {
          os << "(" << type << ")";
          if (!message.empty())
            os << " " << message;
          if (fullStacktrace && stack)
            os << '\n' << stack->rendered(true);
        });
  }

  template <typename Error>
  void setErrorType() noexcept {
    m_matchesErrorType = &Error::matchesErrorType;
  }

private:
  static std::function<void(std::ostream&, bool)> captureOriginStack();
  bool (*m_matchesErrorType)(TypeKey) noexcept = &StarException::matchesErrorType;
  StarException(std::function<void(std::ostream&, bool)> printer,
      bool (*matcher)(TypeKey) noexcept, std::shared_ptr<ExceptionSummary const> summary) noexcept
    : m_matchesErrorType(matcher), m_payload(std::make_shared<ExceptionPayload const>(std::move(printer))),
      m_summary(std::move(summary)) {}
  std::shared_ptr<ExceptionPayload const> m_payload;
  std::shared_ptr<ExceptionSummary const> m_summary;
};

inline ExceptionDiagnostic::ExceptionDiagnostic(StarException const& exception)
  : m_payload(exception.m_payload), m_summary(exception.m_summary) {}

inline void printException(std::ostream& os, ExceptionDiagnostic const& e, bool fullStacktrace) {
  os << e.m_payload->rendered(fullStacktrace);
}

inline std::string printException(ExceptionDiagnostic const& e, bool fullStacktrace) {
  return e.m_payload->rendered(fullStacktrace);
}
OutputProxy outputException(ExceptionDiagnostic const& e, bool fullStacktrace);

inline std::string exceptionSummary(std::shared_ptr<ExceptionSummary const> const& summary) {
  if (!summary)
    return "Error";
  std::string_view category = summary->category;
  switch (summary->kind) {
    case ExceptionSummaryKind::Native: break;
    case ExceptionSummaryKind::ScriptRuntime: category = "Lua runtime error"; break;
    case ExceptionSummaryKind::ScriptSyntax: category = "Lua syntax error"; break;
    case ExceptionSummaryKind::PatchRuntime: category = "Lua patch runtime error"; break;
    case ExceptionSummaryKind::PatchSyntax: category = "Lua patch syntax error"; break;
    case ExceptionSummaryKind::Resource: category = "Resource error"; break;
  }
  std::ostringstream out;
  out << category;
  auto reason = std::string_view(summary->reason);
  reason = reason.substr(0, reason.find_first_of("\r\n"));
  if (!reason.empty())
    out << ": " << reason;
  if (!summary->operation.empty())
    out << "\n  Operation: " << summary->operation;
  if (!summary->asset.empty())
    out << "\n  Asset: " << summary->asset;
  if (!summary->assetProvider.empty() && summary->assetProvider != summary->patchProvider)
    out << "\n  Asset provider: " << summary->assetProvider;
  if (!summary->patch.empty())
    out << "\n  Patch: " << summary->patch;
  if (!summary->patchProvider.empty())
    out << "\n  Patch provider: " << summary->patchProvider;
  for (std::size_t i = 0; i < summary->locations.size(); ++i) {
    auto const& location = summary->locations[i];
    out << (i == 0 ? "\n  Script: " : "\n  Called from: ") << location.source;
    if (location.line > 0)
      out << ":" << location.line;
    if (!location.provider.empty() && location.provider != summary->patchProvider)
      out << "\n    Provider: " << location.provider;
  }
  return out.str();
}

inline std::string exceptionSummary(StarException const& error) {
  return exceptionSummary(error.diagnosticSummary());
}

inline std::string exceptionSummary(ExceptionDiagnostic const& error) {
  return exceptionSummary(error.diagnosticSummary());
}
inline std::string exceptionReport(ExceptionDiagnostic const& error) {
  std::ostringstream out;
  out << exceptionSummary(error) << "\nDetails:\n";
  printException(out, error, true);
  return std::move(out).str();
}


void printStack(std::string_view message);

// Log error and stack-trace and possibly show a dialog box if available, then
// abort.
void fatalError(std::string_view message, bool showStackTrace);

// The file and line of an assertion are a language construct now
// (std::source_location) instead of __FILE__/__LINE__ stringification.
// Callers guard this function with `if constexpr (Star::DebugEnabled)` so a
// release build never evaluates the assertion operand.
// The condition is a forwarding reference, not a bool: it has to go through the
// same contextual conversion the old macro's `if (COND)` performed, otherwise
// types with an explicit operator bool (std::unique_ptr, for instance) stop
// compiling.
template <typename T>
inline void starAssertFailed(T&& condition, std::source_location location = std::source_location::current()) {
#ifndef NDEBUG
  if (condition)
    return;

  std::string const message = strf("assert failure in file {} line {}", location.file_name(), location.line());
  Star::fatalError(message, true);
#else
  (void)condition;
  (void)location;
#endif
}

inline void debugPrintStack(std::source_location location = std::source_location::current()) {
#ifndef NDEBUG
  // The message outlives the call: printStack may hand it on before returning.
  std::string const message = strf("Debug: file {} line {}", location.file_name(), location.line());
  Star::printStack(message);
#else
  (void)location;
#endif
}


// Domain error values preserve their category when stored as StarException.
template <typename Tag, typename Base>
class StarError : public Base {
public:
  template <typename... Args>
  static StarError format(std::format_string<Args...> fmt, Args const&... args) {
    return StarError(strfChecked(fmt, args...));
  }


  static bool matchesErrorType(TypeKey key) noexcept {
    return key == typeKey<StarError>() || Base::matchesErrorType(key);
  }

  StarError() : Base(Tag::name(), std::string(), true) { this->template setErrorType<StarError>(); }
  explicit StarError(std::string message, bool genStackTrace = true)
    : Base(Tag::name(), std::move(message), genStackTrace) { this->template setErrorType<StarError>(); }
  explicit StarError(ExceptionDiagnostic cause) : Base(Tag::name(), std::string(), std::move(cause)) {
    this->template setErrorType<StarError>();
  }
  StarError(std::string message, ExceptionDiagnostic cause) : Base(Tag::name(), std::move(message), std::move(cause)) {
    this->template setErrorType<StarError>();
  }

protected:
  StarError(std::string_view type, std::string message, bool genStackTrace = true)
    : Base(type, std::move(message), genStackTrace) { this->template setErrorType<StarError>(); }
  StarError(std::string_view type, std::string message, ExceptionDiagnostic cause)
    : Base(type, std::move(message), std::move(cause)) { this->template setErrorType<StarError>(); }
};

struct OutOfRangeExceptionTag {
  static constexpr std::string_view name() { return "OutOfRangeException"; }
};
using OutOfRangeException = StarError<OutOfRangeExceptionTag, StarException>;
struct IOExceptionTag {
  static constexpr std::string_view name() { return "IOException"; }
};
using IOException = StarError<IOExceptionTag, StarException>;
struct MemoryExceptionTag {
  static constexpr std::string_view name() { return "MemoryException"; }
};
using MemoryException = StarError<MemoryExceptionTag, StarException>;

template <typename... Args>
StarException StarException::format(std::format_string<Args...> fmt, Args const&... args) {
  return StarException(strfChecked(fmt, args...));
}


}
namespace Star {

struct FormatExceptionTag {
  static constexpr std::string_view name() { return "FormatException"; }
};
using FormatException = StarError<FormatExceptionTag, StarException>;

// Tags are UTF-8 byte sequences; values are copied verbatim, including NULs.
// A doubled '<' is literal. Bindings use the first matching name.
[[nodiscard]] inline std::expected<std::string, StarException> formatTags(
    std::string_view text,
    std::span<std::pair<std::string_view, std::string_view> const> tags) {
  std::string output;
  output.reserve(text.size());
  std::size_t position = 0;
  while (position < text.size()) {
    auto open = text.find('<', position);
    if (open == std::string_view::npos) {
      output.append(text.substr(position));
      break;
    }
    output.append(text.substr(position, open - position));
    if (open + 1 < text.size() && text[open + 1] == '<') {
      output.push_back('<');
      position = open + 2;
      continue;
    }
    auto close = text.find('>', open + 1);
    if (close == std::string_view::npos)
      return std::unexpected(FormatException("Unterminated template tag"));
    auto name = text.substr(open + 1, close - open - 1);
    if (name.empty())
      return std::unexpected(FormatException("Empty template tag"));
    auto binding = std::find_if(tags.begin(), tags.end(),
        [name](auto const& entry) { return entry.first == name; });
    if (binding == tags.end())
      return std::unexpected(FormatException::format("Unknown template tag '{}'", name));
    output.append(binding->second);
    position = close + 1;
  }
  return output;
}

namespace Detail {
  // Only callers holding a compile-time checked format string use this path.
  template <typename... T>
  std::string vformatChecked(std::string_view fmt, T const&... args) {
    return std::vformat(fmt, std::make_format_args(args...));
  }
}

// Consteval validation happens at the caller, before a literal is forwarded.
template <typename... T>
std::string strf(std::format_string<T...> fmt, T&&... args) {
  return Detail::vformatChecked(fmt.get(), args...);
}

// Keep the argument types used to check a wrapper's format string.
template <typename Char, typename... T>
std::string strfChecked(std::basic_format_string<Char, T...> fmt, T const&... args) {
  return Detail::vformatChecked(fmt.get(), args...);
}


template <typename... T>
void format(std::ostream& out, std::format_string<T...> fmt, T&&... args) {
  // Preserve ostream field padding when the caller has set a width.
  if (out.width() != 0) {
    out << Detail::vformatChecked(fmt.get(), args...);
    return;
  }
  std::ostream::sentry sentry(out);
  if (sentry) {
    auto output = std::format_to(std::ostreambuf_iterator<char>(out), fmt, std::forward<T>(args)...);
    if (output.failed())
      out.setstate(std::ios_base::badbit);
  }
}


// Automatically flushes; use format to avoid flushing.
template <typename... T>
void coutf(std::format_string<T...> fmt, T&&... args) {
  Star::format(std::cout, fmt, std::forward<T>(args)...);
  std::cout.flush();
}


// Automatically flushes; use format to avoid flushing.
template <typename... T>
void cerrf(std::format_string<T...> fmt, T&&... args) {
  Star::format(std::cerr, fmt, std::forward<T>(args)...);
  std::cerr.flush();
}


template <class Type>
inline std::string toString(Type const& t) {
  return std::format("{}", t);
}

}

export module star.format;

export namespace Star {
using ::Star::DebugEnabled;
using ::Star::ExceptionDiagnostic;
using ::Star::ExceptionSummaryKind;
using ::Star::ExceptionSummary;
using ::Star::exceptionSummary;
using ::Star::exceptionReport;
using ::Star::StarException;
using ::Star::StarError;
using ::Star::OutOfRangeExceptionTag;
using ::Star::OutOfRangeException;
using ::Star::IOExceptionTag;
using ::Star::IOException;
using ::Star::MemoryExceptionTag;
using ::Star::MemoryException;
using ::Star::FormatExceptionTag;
using ::Star::FormatException;
using ::Star::printException;
using ::Star::outputException;
using ::Star::printStack;
using ::Star::fatalError;
using ::Star::starAssertFailed;
using ::Star::debugPrintStack;
using ::Star::strf;
using ::Star::strfChecked;
using ::Star::format;
using ::Star::coutf;
using ::Star::cerrf;
using ::Star::toString;
using ::Star::formatTags;
}

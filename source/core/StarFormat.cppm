module;
#include <stdio.h>
import std;

import star.type_identity;
import star.text_formatter;

namespace Star {
struct OutputProxy;
#ifndef NDEBUG
inline constexpr bool DebugEnabled = true;
#else
inline constexpr bool DebugEnabled = false;
#endif

// Formatting reads the same const lvalue arguments used by make_format_args.
// std::formattable checks the mapped formatter's real parse/format operations,
// including character arrays, rather than an unconstrained format call.
template <typename T>
concept format_argument = std::formattable<std::remove_reference_t<T> const&, char>;

template <typename... T>
  requires (format_argument<T> && ...)
std::string format(std::format_string<T...> fmt, T&&... args);


template <typename Char, typename... T>
  requires std::same_as<Char, char> && (format_argument<T> && ...)
std::string format_checked(std::basic_format_string<Char, T...> fmt, T const&... args);

class StarException;

// A diagnostic is an immutable graph. Only its lazily rendered views mutate;
// child emission deliberately bypasses whole-chain caches so rendering a deep
// chain does not materialize and retain every suffix of that chain.
class ExceptionPayload {
public:
  explicit ExceptionPayload(std::function<void(TextBuffer&, bool)> printer)
    : m_printer(std::move(printer)) {}

  void emit(TextBuffer& buffer, bool fullStacktrace) const {
    m_printer(buffer, fullStacktrace);
  }

  std::string const& rendered(bool fullStacktrace) const {
    auto index = fullStacktrace ? 1u : 0u;
    std::call_once(m_renderOnce[index], [this, index, fullStacktrace] {
      TextBuffer buffer;
      emit(buffer, fullStacktrace);
      m_rendered[index] = std::move(buffer).str();
    });
    return m_rendered[index];
  }

private:
  std::function<void(TextBuffer&, bool)> const m_printer;
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

  friend void print_exception(TextBuffer& buffer, ExceptionDiagnostic const& e, bool fullStacktrace);
  friend std::string print_exception(ExceptionDiagnostic const& e, bool fullStacktrace);
  friend OutputProxy output_exception(ExceptionDiagnostic const& e, bool fullStacktrace);

private:
  friend class StarException;
  std::shared_ptr<ExceptionPayload const> m_payload;
  std::shared_ptr<ExceptionSummary const> m_summary;
};

class StarException {
public:
  template <typename... Args>
    requires (format_argument<Args> && ...)
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
        [context = std::move(context), cause = m_payload](TextBuffer& buffer, bool fullStacktrace) {
          buffer.append(context);
          buffer.append("\nCaused by: ");
          cause->emit(buffer, fullStacktrace);
        },
        m_matchesErrorType, m_summary);
  }

  StarException withContext(std::string context) && {
    return StarException(
        [context = std::move(context), cause = std::move(m_payload)](TextBuffer& buffer, bool fullStacktrace) {
          buffer.append(context);
          buffer.append("\nCaused by: ");
          cause->emit(buffer, fullStacktrace);
        },
        m_matchesErrorType, std::move(m_summary));
  }

  StarException withRelated(StarException const& secondary, std::string context) const& {
    return StarException(
        [primary = m_payload, secondary = secondary.m_payload, context = std::move(context)](
            TextBuffer& buffer, bool fullStacktrace) {
          primary->emit(buffer, fullStacktrace);
          buffer.push_back('\n');
          buffer.append(context);
          buffer.append("\nRelated error: ");
          secondary->emit(buffer, fullStacktrace);
        },
        m_matchesErrorType, m_summary);
  }

  StarException withRelated(StarException const& secondary, std::string context) && {
    // Copy the secondary handle first: secondary may alias this value.
    auto related = secondary.m_payload;
    return StarException(
        [primary = std::move(m_payload), secondary = std::move(related), context = std::move(context)](
            TextBuffer& buffer, bool fullStacktrace) {
          primary->emit(buffer, fullStacktrace);
          buffer.push_back('\n');
          buffer.append(context);
          buffer.append("\nRelated error: ");
          secondary->emit(buffer, fullStacktrace);
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
        [self = std::move(m_payload), cause = std::move(cause.m_payload)](TextBuffer& buffer, bool fullStacktrace) {
          self->emit(buffer, fullStacktrace);
          buffer.append("\nCaused by: ");
          cause->emit(buffer, fullStacktrace);
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
    auto stack = genStackTrace ? std::make_shared<ExceptionPayload const>(capture_origin_stack()) : nullptr;
    m_payload = std::make_shared<ExceptionPayload const>(
        [type = std::string(type), message = std::move(message), stack = std::move(stack)](
            TextBuffer& buffer, bool fullStacktrace) {
          buffer.push_back('(');
          buffer.append(type);
          buffer.push_back(')');
          if (!message.empty()) {
            buffer.push_back(' ');
            buffer.append(message);
          }
          if (fullStacktrace && stack) {
            buffer.push_back('\n');
            buffer.append(stack->rendered(true));
          }
        });
  }

  template <typename Error>
  void setErrorType() noexcept {
    m_matchesErrorType = &Error::matchesErrorType;
  }

private:
  static std::function<void(TextBuffer&, bool)> capture_origin_stack();
  bool (*m_matchesErrorType)(TypeKey) noexcept = &StarException::matchesErrorType;
  StarException(std::function<void(TextBuffer&, bool)> printer,
      bool (*matcher)(TypeKey) noexcept, std::shared_ptr<ExceptionSummary const> summary) noexcept
    : m_matchesErrorType(matcher), m_payload(std::make_shared<ExceptionPayload const>(std::move(printer))),
      m_summary(std::move(summary)) {}
  std::shared_ptr<ExceptionPayload const> m_payload;
  std::shared_ptr<ExceptionSummary const> m_summary;
};

inline ExceptionDiagnostic::ExceptionDiagnostic(StarException const& exception)
  : m_payload(exception.m_payload), m_summary(exception.m_summary) {}

inline void print_exception(TextBuffer& buffer, ExceptionDiagnostic const& e, bool fullStacktrace) {
  buffer.append(e.m_payload->rendered(fullStacktrace));
}

inline std::string print_exception(ExceptionDiagnostic const& e, bool fullStacktrace) {
  return e.m_payload->rendered(fullStacktrace);
}
OutputProxy output_exception(ExceptionDiagnostic const& e, bool fullStacktrace);

inline std::string exception_summary(std::shared_ptr<ExceptionSummary const> const& summary) {
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
  TextBuffer buffer;
  buffer.append(category);
  auto reason = std::string_view(summary->reason);
  reason = reason.substr(0, reason.find_first_of("\r\n"));
  auto append_field = [&](std::string_view prefix, std::string_view value) {
    if (!value.empty()) {
      buffer.append(prefix);
      buffer.append(value);
    }
  };
  append_field(": ", reason);
  append_field("\n  Operation: ", summary->operation);
  append_field("\n  Asset: ", summary->asset);
  if (summary->assetProvider != summary->patchProvider)
    append_field("\n  Asset provider: ", summary->assetProvider);
  append_field("\n  Patch: ", summary->patch);
  append_field("\n  Patch provider: ", summary->patchProvider);
  for (std::size_t i = 0; i < summary->locations.size(); ++i) {
    auto const& location = summary->locations[i];
    buffer.append(i == 0 ? "\n  Script: " : "\n  Called from: ");
    buffer.append(location.source);
    if (location.line > 0) {
      buffer.push_back(':');
      append_text(buffer, location.line);
    }
    if (!location.provider.empty() && location.provider != summary->patchProvider
        && location.provider != summary->assetProvider
        && std::none_of(summary->locations.begin(), summary->locations.begin() + i,
            [&](auto const& previous) { return previous.provider == location.provider; })) {
      buffer.append("\n    Provider: ");
      buffer.append(location.provider);
    }
  }
  return std::move(buffer).str();
}

inline std::string exception_summary(StarException const& error) {
  return exception_summary(error.diagnosticSummary());
}

inline std::string exception_summary(ExceptionDiagnostic const& error) {
  return exception_summary(error.diagnosticSummary());
}
inline std::string exception_report(ExceptionDiagnostic const& error) {
  TextBuffer buffer;
  buffer.append(exception_summary(error));
  buffer.append("\nDetails:\n");
  print_exception(buffer, error, true);
  return std::move(buffer).str();
}


void print_stack(std::string_view message);

// Log error and stack-trace and possibly show a dialog box if available, then
// abort.
void fatal_error(std::string_view message, bool showStackTrace);

// The file and line of an assertion are a language construct now
// (std::source_location) instead of __FILE__/__LINE__ stringification.
// Callers guard this function with `if constexpr (Star::DebugEnabled)` so a
// release build never evaluates the assertion operand.
// The condition is a forwarding reference, not a bool: it has to go through the
// same contextual conversion the old macro's `if (COND)` performed, otherwise
// types with an explicit operator bool (std::unique_ptr, for instance) stop
// compiling.
template <typename T>
  requires (!DebugEnabled || requires(T& value) { value ? true : false; })
inline void star_assert_failed(T&& condition, std::source_location location = std::source_location::current()) {
#ifndef NDEBUG
  if (condition)
    return;

  std::string const message = format("assert failure in file {} line {}", location.file_name(), location.line());
  Star::fatal_error(message, true);
#else
  (void)condition;
  (void)location;
#endif
}

inline void debug_print_stack(std::source_location location = std::source_location::current()) {
#ifndef NDEBUG
  // The message outlives the call: print_stack may hand it on before returning.
  std::string const message = format("Debug: file {} line {}", location.file_name(), location.line());
  Star::print_stack(message);
#else
  (void)location;
#endif
}


// Domain error values preserve their category when stored as StarException.
template <typename Tag, typename Base>
class StarError : public Base {
public:
  template <typename... Args>
    requires (format_argument<Args> && ...)
  static StarError format(std::format_string<Args...> fmt, Args const&... args) {
    return StarError(format_checked(fmt, args...));
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
  requires (format_argument<Args> && ...)
StarException StarException::format(std::format_string<Args...> fmt, Args const&... args) {
  return StarException(format_checked(fmt, args...));
}


}
namespace Star {

struct FormatExceptionTag {
  static constexpr std::string_view name() { return "FormatException"; }
};
using FormatException = StarError<FormatExceptionTag, StarException>;

// Tags are UTF-8 byte sequences; values are copied verbatim, including NULs.
// A doubled '<' is literal. Bindings use the first matching name.
[[nodiscard]] inline std::expected<std::string, StarException> format_tags(
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

namespace format_detail {
  // Only callers holding a compile-time checked format string use this path.
  template <typename... T>
    requires (format_argument<T> && ...)
  std::string format_checked_text(std::string_view fmt, T const&... args) {
    return std::vformat(fmt, std::make_format_args(args...));
  }

  template <typename... T>
  concept console_format_arguments = (format_argument<T> && ...)
      && requires(TextBuffer& buffer, std::format_string<T...> fmt, T&... args) {
        std::format_to(std::back_inserter(buffer), fmt, std::forward<T>(args)...);
      };

  inline std::mutex console_mutex;

  // The caller owns console_mutex through writing and flushing, so concurrent
  // messages cannot interleave and the original automatic flush is preserved.
  [[nodiscard]] inline std::expected<void, StarException> write_console(std::FILE* file, std::string_view text) {
    auto written = text.empty() ? 0 : std::fwrite(text.data(), 1, text.size(), file);
    bool write_failed = written != text.size() || std::ferror(file);
    auto flush_status = std::fflush(file);
    if (write_failed)
      return std::unexpected(IOException("Failed to write console output"));
    if (flush_status != 0)
      return std::unexpected(IOException("Failed to flush console output"));
    return {};
  }
}

// Consteval validation happens at the caller, before a literal is forwarded.
template <typename... T>
  requires (format_argument<T> && ...)
std::string format(std::format_string<T...> fmt, T&&... args) {
  return format_detail::format_checked_text(fmt.get(), args...);
}

// Keep the argument types used to check a wrapper's format string.
template <typename Char, typename... T>
  requires std::same_as<Char, char> && (format_argument<T> && ...)
std::string format_checked(std::basic_format_string<Char, T...> fmt, T const&... args) {
  return format_detail::format_checked_text(fmt.get(), args...);
}

template <typename... T>
  requires format_detail::console_format_arguments<T...>
[[nodiscard]] std::expected<void, StarException> print(std::FILE* file, std::format_string<T...> fmt, T&&... args) {
  TextBuffer buffer;
  std::format_to(std::back_inserter(buffer), fmt, std::forward<T>(args)...);
  std::lock_guard lock(format_detail::console_mutex);
  return format_detail::write_console(file, buffer.view());
}

template <typename... T>
  requires format_detail::console_format_arguments<T...>
[[nodiscard]] std::expected<void, StarException> print(std::format_string<T...> fmt, T&&... args) {
  return Star::print(stdout, fmt, std::forward<T>(args)...);
}

template <typename... T>
  requires format_detail::console_format_arguments<T...>
[[nodiscard]] std::expected<void, StarException> println(std::FILE* file, std::format_string<T...> fmt, T&&... args) {
  TextBuffer buffer;
  std::format_to(std::back_inserter(buffer), fmt, std::forward<T>(args)...);
  buffer.push_back('\n');
  std::lock_guard lock(format_detail::console_mutex);
  return format_detail::write_console(file, buffer.view());
}

template <typename... T>
  requires format_detail::console_format_arguments<T...>
[[nodiscard]] std::expected<void, StarException> println(std::format_string<T...> fmt, T&&... args) {
  return Star::println(stdout, fmt, std::forward<T>(args)...);
}

[[nodiscard]] inline std::expected<void, StarException> flush(std::FILE* file) {
  std::lock_guard lock(format_detail::console_mutex);
  if (std::fflush(file) != 0 || std::ferror(file))
    return std::unexpected(IOException("Failed to flush console output"));
  return {};
}

[[nodiscard]] inline std::expected<void, StarException> flush() {
  return Star::flush(stdout);
}

template <class Type>
  requires format_argument<Type>
inline std::string to_string(Type const& t) {
  return std::format("{}", t);
}

}

export module star.format;

export namespace Star {
using ::Star::format_argument;
using ::Star::DebugEnabled;
using ::Star::ExceptionDiagnostic;
using ::Star::ExceptionSummaryKind;
using ::Star::ExceptionSummary;
using ::Star::exception_summary;
using ::Star::exception_report;
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
using ::Star::print_exception;
using ::Star::output_exception;
using ::Star::print_stack;
using ::Star::fatal_error;
using ::Star::star_assert_failed;
using ::Star::debug_print_stack;
using ::Star::format;
using ::Star::format_checked;
using ::Star::print;
using ::Star::println;
using ::Star::flush;
using ::Star::to_string;
using ::Star::format_tags;
}

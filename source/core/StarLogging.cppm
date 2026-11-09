module;

import std;
import star.set;
import star.string;
import star.map;
import star.list;
import star.format;
import star.poly;
import star.bi_map;
import star.file;

import star.array;
import star.vector;
import star.line;
namespace Star {

enum class LogLevel {
  Debug,
  Info,
  Warn,
  Error
};
inline constexpr auto LogLevelNames = sortedEnumNames(std::to_array<std::pair<LogLevel, std::string_view>>({
  {LogLevel::Debug, "Debug"},
  {LogLevel::Info, "Info"},
  {LogLevel::Warn, "Warn"},
  {LogLevel::Error, "Error"}
}));
static_assert(validEnumNames<LogLevelNames>());

// A sink for Logger messages.
class LogSink {
public:
  LogSink();
  virtual ~LogSink();

  [[nodiscard]] virtual std::expected<void, StarException> log(std::string_view msg, LogLevel level) = 0;

  void set_level(LogLevel level);
  LogLevel level();

private:
  std::atomic<LogLevel> m_level;
};

class StdoutLogSink : public LogSink {
public:
  [[nodiscard]] std::expected<void, StarException> log(std::string_view msg, LogLevel level) override;

private:
  std::mutex m_logMutex;
};

class FileLogSink : public LogSink {
public:
  [[nodiscard]] static std::expected<std::shared_ptr<FileLogSink>, StarException> create(
      String const& filename, LogLevel level, bool truncate);

  [[nodiscard]] std::expected<void, StarException> log(std::string_view msg, LogLevel level) override;

private:
  explicit FileLogSink(String filename) : m_output(std::move(filename)) {}
  File m_output;
  std::mutex m_logMutex;
};

// A fixed-address log domain. Its sinks never affect another owner.
// Accepted calls retain one immutable registry snapshot. Mutations from callbacks
// affect later calls; removed sinks live until their in-flight snapshots retire.
// Dispatch through one Logger is serialized across all its sinks, preserving
// their message order. Custom sinks shared across Loggers must synchronize
// themselves; the built-in sinks already do. Same-thread recursive dispatch is
// supported; callbacks must not wait on another thread dispatching to this Logger.
class Logger {
public:
  Logger();
  void add_sink(std::shared_ptr<LogSink> s);
  void remove_sink(std::shared_ptr<LogSink> s);
  std::shared_ptr<LogSink> stdout_sink();
  void remove_stdout_sink();
  [[nodiscard]] std::expected<void, StarException> log(LogLevel level, std::string_view msg);

  template <typename... Args>
    requires (format_argument<Args> && ...)
  [[nodiscard]] std::expected<void, StarException> logf(LogLevel level, std::format_string<Args...> msg, Args const&... args);
  template <typename... Args>
    requires (format_argument<Args> && ...)
  [[nodiscard]] std::expected<void, StarException> debug(std::format_string<Args...> msg, Args const&... args);
  template <typename... Args>
    requires (format_argument<Args> && ...)
  [[nodiscard]] std::expected<void, StarException> info(std::format_string<Args...> msg, Args const&... args);
  template <typename... Args>
    requires (format_argument<Args> && ...)
  [[nodiscard]] std::expected<void, StarException> warn(std::format_string<Args...> msg, Args const&... args);
  template <typename... Args>
    requires (format_argument<Args> && ...)
  [[nodiscard]] std::expected<void, StarException> error(std::format_string<Args...> msg, Args const&... args);

  // Summary at the requested severity; complete diagnostics only for Debug sinks.
  [[nodiscard]] std::expected<void, StarException> log_exception(LogLevel level, std::string_view operation,
                                                                 ExceptionDiagnostic const& error);
  [[nodiscard]] std::expected<void, StarException> error_exception(std::string_view operation,
                                                                   ExceptionDiagnostic const& error);
  [[nodiscard]] std::expected<void, StarException> warn_exception(std::string_view operation,
                                                                  ExceptionDiagnostic const& error);
  bool loggable(LogLevel level);

private:
  using SinkRegistry = HashSet<std::shared_ptr<LogSink>>;
  std::shared_ptr<SinkRegistry const> sink_snapshot(std::optional<LogLevel> level);
  static SinkRegistry::const_iterator first_loggable(SinkRegistry const& sinks, LogLevel level);
  [[nodiscard]] std::expected<void, StarException> dispatch(SinkRegistry const& sinks, LogLevel level,
                                                            std::string_view message);

  std::shared_ptr<StdoutLogSink> s_stdoutSink;
  std::shared_ptr<SinkRegistry const> s_sinks;
  std::mutex s_mutex;
  std::recursive_mutex s_dispatchMutex;
};

// Stateless diagnostics for process/bootstrap code without a business owner.
// This path has no registry, sink configuration, or shared mutable cache.
// Failed emergency output terminates with failure rather than recursively logging.
class EmergencyLog {
public:
  static void log(LogLevel level, std::string_view message);
  template <typename... Args>
    requires (format_argument<Args> && ...)
  static void debug(std::format_string<Args...> format, Args const&... args) {
    auto message = Star::format_checked(format, args...);
    log(LogLevel::Debug, message);
  }
  template <typename... Args>
    requires (format_argument<Args> && ...)
  static void info(std::format_string<Args...> format, Args const&... args) {
    auto message = Star::format_checked(format, args...);
    log(LogLevel::Info, message);
  }
  template <typename... Args>
    requires (format_argument<Args> && ...)
  static void warn(std::format_string<Args...> format, Args const&... args) {
    auto message = Star::format_checked(format, args...);
    log(LogLevel::Warn, message);
  }
  template <typename... Args>
    requires (format_argument<Args> && ...)
  static void error(std::format_string<Args...> format, Args const&... args) {
    auto message = Star::format_checked(format, args...);
    log(LogLevel::Error, message);
  }
};

// For logging data that is very high frequency. It is a map of debug values to
// be displayed every frame, or in a debug output window, etc.
class LogMap {
public:
  String get_value(String const& key);
  void set_value(String const& key, String const& value);

  template <typename T>
    requires format_argument<T>
  void set(String const& key, T const& t);

  Map<String, String> get_values();
  void clear();

private:
  HashMap<String, String> s_logMap;
  std::mutex s_logMapMutex;
};

// Logging for spatial data.  Divided into multiple named coordinate spaces.
class SpatialLogger {
public:
  // Maximum count of objects stored per space
  static std::size_t const MaximumLines = 200000;
  static std::size_t const MaximumPoints = 200000;
  static std::size_t const MaximumText = 10000;

  struct Line {
    Vec2F begin;
    Vec2F end;
    Vec4B color;
  };

  struct Point {
    Vec2F position;
    Vec4B color;
  };

  struct LogText {
    String text;
    Vec2F position;
    Vec4B color;
  };

  void log_poly(std::string_view space, PolyF const& poly, Vec4B const& color);
  void log_line(std::string_view space, Line2F const& line, Vec4B const& color);
  void log_line(std::string_view space, Vec2F const& begin, Vec2F const& end, Vec4B const& color);
  void log_point(std::string_view space, Vec2F const& position, Vec4B const& color);
  void log_text(std::string_view space, String text, Vec2F const& position, Vec4B const& color);

  Deque<Line> get_lines(std::string_view space, bool andClear);
  Deque<Point> get_points(std::string_view space, bool andClear);
  Deque<LogText> get_text(std::string_view space, bool andClear);
  void clear();
  bool observed();
  void set_observed(bool observed);

private:
  std::mutex s_mutex;
  StringMap<Deque<Line>> s_lines;
  StringMap<Deque<Point>> s_points;
  StringMap<Deque<LogText>> s_logText;
  std::atomic<bool> s_observed{false};
};

template <typename... Args>
  requires (format_argument<Args> && ...)
[[nodiscard]] std::expected<void, StarException> Logger::logf(LogLevel level, std::format_string<Args...> msg, Args const&... args) {
  auto sinks = sink_snapshot(level);
  if (!sinks)
    return {};
  auto output = Star::format_checked(msg, args...);
  return dispatch(*sinks, level, output);
}

template <typename... Args>
  requires (format_argument<Args> && ...)
[[nodiscard]] std::expected<void, StarException> Logger::debug(std::format_string<Args...> msg, Args const&... args) {
  return logf(LogLevel::Debug, msg, args...);
}

template <typename... Args>
  requires (format_argument<Args> && ...)
[[nodiscard]] std::expected<void, StarException> Logger::info(std::format_string<Args...> msg, Args const&... args) {
  return logf(LogLevel::Info, msg, args...);
}

template <typename... Args>
  requires (format_argument<Args> && ...)
[[nodiscard]] std::expected<void, StarException> Logger::warn(std::format_string<Args...> msg, Args const&... args) {
  return logf(LogLevel::Warn, msg, args...);
}

template <typename... Args>
  requires (format_argument<Args> && ...)
[[nodiscard]] std::expected<void, StarException> Logger::error(std::format_string<Args...> msg, Args const&... args) {
  return logf(LogLevel::Error, msg, args...);
}

template <typename T>
  requires format_argument<T>
void LogMap::set(String const& key, T const& t) {
  set_value(key, Star::to_string(t));
}

}

export module star.logging;

export namespace Star {
  using ::Star::LogLevel;
  using ::Star::LogLevelNames;
  using ::Star::LogSink;
  using ::Star::StdoutLogSink;
  using ::Star::FileLogSink;
  using ::Star::Logger;
  using ::Star::EmergencyLog;
  using ::Star::LogMap;
  using ::Star::SpatialLogger;
}

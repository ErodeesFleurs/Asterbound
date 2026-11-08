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

  void setLevel(LogLevel level);
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
  FileLogSink() = default;
  std::shared_ptr<File> m_output;
  std::mutex m_logMutex;
};

// A fixed-address log domain. Its sinks never affect another owner.
class Logger {
public:
  Logger();
  void addSink(std::shared_ptr<LogSink> s);
  void removeSink(std::shared_ptr<LogSink> s);
  std::shared_ptr<LogSink> stdoutSink();
  void removeStdoutSink();
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
  [[nodiscard]] std::expected<void, StarException> logException(
      LogLevel level, std::string_view operation, ExceptionDiagnostic const& error);
  [[nodiscard]] std::expected<void, StarException> errorException(
      std::string_view operation, ExceptionDiagnostic const& error);
  [[nodiscard]] std::expected<void, StarException> warnException(
      std::string_view operation, ExceptionDiagnostic const& error);
  bool loggable(LogLevel level);

private:
  std::shared_ptr<StdoutLogSink> s_stdoutSink;
  HashSet<std::shared_ptr<LogSink>> s_sinks;
  std::mutex s_mutex;
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
  String getValue(String const& key);
  void setValue(String const& key, String const& value);

  template <typename T>
    requires format_argument<T>
  void set(String const& key, T const& t);

  Map<String, String> getValues();
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

  void logPoly(std::string_view space, PolyF const& poly, Vec4B const& color);
  void logLine(std::string_view space, Line2F const& line, Vec4B const& color);
  void logLine(std::string_view space, Vec2F const& begin, Vec2F const& end, Vec4B const& color);
  void logPoint(std::string_view space, Vec2F const& position, Vec4B const& color);
  void logText(std::string_view space, String text, Vec2F const& position, Vec4B const& color);

  Deque<Line> getLines(std::string_view space, bool andClear);
  Deque<Point> getPoints(std::string_view space, bool andClear);
  Deque<LogText> getText(std::string_view space, bool andClear);
  void clear();
  bool observed();
  void setObserved(bool observed);

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
  if (loggable(level)) {
    std::string output = Star::format_checked(msg, args...);
    return log(level, output);
  }
  return {};
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
  setValue(key, Star::to_string(t));
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

#include <stdio.h>
import std;
import star.logging;
import star.string;
import star.set;
import star.map;
import star.list;
import star.bi_map;
import star.file;
import star.io_device;
import star.format;
import star.output_proxy;
import star.time;

import star.array;
import star.vector;
import star.line;
import star.poly;
import star.algorithm;
namespace Star {

LogSink::LogSink()
  : m_level(LogLevel::Info) {}

LogSink::~LogSink() = default;

void LogSink::set_level(LogLevel level) { m_level = level; }

LogLevel LogSink::level() {
  return m_level;
}

std::expected<void, StarException> StdoutLogSink::log(std::string_view msg, LogLevel level) {
  std::lock_guard locker(m_logMutex);
  auto name = enumName<LogLevelNames>(level);
  if (!name)
    return std::unexpected(std::move(name.error()));
  return Star::print("[{}] {}\n", *name, msg);
}

std::expected<std::shared_ptr<FileLogSink>, StarException> FileLogSink::create(
    String const& filename, LogLevel level, bool truncate) {
  auto sink = std::shared_ptr<FileLogSink>(new FileLogSink(filename));
  auto opened =
    sink->m_output.open(truncate ? IOMode::Write | IOMode::Append | IOMode::Truncate : IOMode::Write | IOMode::Append);
  if (!opened)
    return std::unexpected(std::move(opened.error()));
  sink->set_level(level);
  return sink;
}

std::expected<void, StarException> FileLogSink::log(std::string_view msg, LogLevel level) {
  std::lock_guard locker(m_logMutex);
  auto timestamp = Time::localTime(std::chrono::system_clock::now());
  if (!timestamp)
    return std::unexpected(std::move(timestamp.error()));
  auto name = enumName<LogLevelNames>(level);
  if (!name)
    return std::unexpected(std::move(name.error()));
  auto line = Star::format("[{:%H:%M:%S}] [{}] {}\n", *timestamp, *name, msg);
  return m_output.write_full(line);
}

void Logger::add_sink(std::shared_ptr<LogSink> sink) {
  std::shared_ptr<SinkRegistry const> retired;
  {
    std::lock_guard locker(s_mutex);
    if (s_sinks->contains(sink))
      return;
    auto next = std::make_shared<SinkRegistry>(*s_sinks);
    next->insert(std::move(sink));
    retired = std::exchange(s_sinks, std::move(next));
  }
}

void Logger::remove_sink(std::shared_ptr<LogSink> sink) {
  std::shared_ptr<SinkRegistry const> retired;
  {
    std::lock_guard locker(s_mutex);
    if (!s_sinks->contains(sink))
      return;
    auto next = std::make_shared<SinkRegistry>(*s_sinks);
    next->erase(sink);
    retired = std::exchange(s_sinks, std::move(next));
  }
}

std::shared_ptr<LogSink> Logger::stdout_sink() { return s_stdoutSink; }

void Logger::remove_stdout_sink() { remove_sink(s_stdoutSink); }

std::shared_ptr<Logger::SinkRegistry const> Logger::sink_snapshot(std::optional<LogLevel> level) {
  std::lock_guard locker(s_mutex);
  if (level && first_loggable(*s_sinks, *level) == s_sinks->end())
    return {};
  return s_sinks;
}

Logger::SinkRegistry::const_iterator Logger::first_loggable(SinkRegistry const& sinks, LogLevel level) {
  return std::find_if(sinks.begin(), sinks.end(),
                      [level](std::shared_ptr<LogSink> const& sink) { return sink->level() <= level; });
}

std::expected<void, StarException> Logger::dispatch(SinkRegistry const& sinks, LogLevel level,
                                                    std::string_view message) {
  std::lock_guard locker(s_dispatchMutex);
  // The pre-format check is only a hint. Threshold changes during
  // formatting or while waiting for another dispatch remain effective.
  for (auto const& sink : sinks) {
    if (sink->level() <= level) {
      auto result = sink->log(message, level);
      if (!result)
        return std::unexpected(
          std::move(result.error()).with_context(Star::format("While writing log message: {}", message)));
    }
  }
  return {};
}

std::expected<void, StarException> Logger::log(LogLevel level, std::string_view msg) {
  auto sinks = sink_snapshot(level);
  if (!sinks)
    return {};
  return dispatch(*sinks, level, msg);
}

std::expected<void, StarException> Logger::log_exception(LogLevel level, std::string_view operation,
                                                         ExceptionDiagnostic const& diagnostic) {
  auto sinks = sink_snapshot(std::nullopt);
  auto first = first_loggable(*sinks, level);
  if (first != sinks->end()) {
    auto summary = Star::format("{}: {}", operation, Star::exception_summary(diagnostic));
    if (auto result = dispatch(*sinks, level, summary); !result)
      return std::unexpected(std::move(result.error()));
  }
  auto debugSink = first_loggable(*sinks, LogLevel::Debug);
  if (debugSink != sinks->end()) {
    auto details = Star::format("{} details:\n{}", operation, Star::output_exception(diagnostic, true));
    return dispatch(*sinks, LogLevel::Debug, details);
  }
  return {};
}

std::expected<void, StarException> Logger::error_exception(std::string_view operation,
                                                           ExceptionDiagnostic const& diagnostic) {
  return log_exception(LogLevel::Error, operation, diagnostic);
}

std::expected<void, StarException> Logger::warn_exception(std::string_view operation,
                                                          ExceptionDiagnostic const& diagnostic) {
  return log_exception(LogLevel::Warn, operation, diagnostic);
}

Logger::Logger()
    : s_stdoutSink(std::make_shared<StdoutLogSink>()),
      s_sinks(std::make_shared<SinkRegistry>(std::initializer_list<std::shared_ptr<LogSink>>{s_stdoutSink})) {}

bool Logger::loggable(LogLevel level) {
  std::lock_guard locker(s_mutex);
  return first_loggable(*s_sinks, level) != s_sinks->end();
}

void EmergencyLog::log(LogLevel, std::string_view message) {
  if (!Star::println(stderr, "{}", message))
    std::_Exit(1);
}

String LogMap::get_value(String const& key) {
  std::lock_guard locker(s_logMapMutex);
  return s_logMap.value(key);
}

void LogMap::set_value(String const& key, String const& value) {
  std::lock_guard locker(s_logMapMutex);
  s_logMap[key] = value;
}

Map<String, String> LogMap::get_values() {
  std::lock_guard locker(s_logMapMutex);
  return Map<String, String>::from(s_logMap);
}

void LogMap::clear() {
  std::lock_guard locker(s_logMapMutex);
  s_logMap.clear();
}


std::size_t const SpatialLogger::MaximumLines;
std::size_t const SpatialLogger::MaximumPoints;
std::size_t const SpatialLogger::MaximumText;

namespace {

template <typename T>
concept spatial_log_value = std::copy_constructible<T>;

template <spatial_log_value T> void append_capped(Deque<T>& values, T value, std::size_t maximum) {
  values.push_back(std::move(value));
  while (values.size() > maximum)
    values.pop_front();
}

template <spatial_log_value T>
Deque<T> query_spatial(StringMap<Deque<T>>& spaces, std::string_view space, bool andClear) {
  auto found = spaces.find(space);
  if (found == spaces.end())
    return {};
  if (andClear)
    return std::exchange(found->second, {});
  return found->second;
}

}// namespace

void SpatialLogger::log_poly(std::string_view space, PolyF const& poly, Vec4B const& color) {
  if (!observed()) return;

  std::lock_guard locker(s_mutex);
  auto& lines = s_lines[space];

  auto sides = poly.sides();
  auto first = sides > MaximumLines ? sides - MaximumLines : 0;
  if (first != 0)
    lines.clear();
  for (std::size_t i = first; i < sides; ++i) {
    auto side = poly.side(i);
    append_capped(lines, Line{.begin = side.min(), .end = side.max(), .color = color}, MaximumLines);
  }
}

void SpatialLogger::log_line(std::string_view space, Line2F const& line, Vec4B const& color) {
  log_line(space, line.min(), line.max(), color);
}

void SpatialLogger::log_line(std::string_view space, Vec2F const& begin, Vec2F const& end, Vec4B const& color) {
  if (!observed()) return;

  std::lock_guard locker(s_mutex);
  auto& lines = s_lines[space];

  append_capped(lines, Line{.begin = begin, .end = end, .color = color}, MaximumLines);
}

void SpatialLogger::log_point(std::string_view space, Vec2F const& position, Vec4B const& color) {
  if (!observed()) return;

  std::lock_guard locker(s_mutex);
  auto& points = s_points[space];

  append_capped(points, Point{.position = position, .color = color}, MaximumPoints);
}

void SpatialLogger::log_text(std::string_view space, String text, Vec2F const& position, Vec4B const& color) {
  if (!observed()) return;

  std::lock_guard locker(s_mutex);
  auto& texts = s_logText[space];

  append_capped(texts, LogText{.text = std::move(text), .position = position, .color = color}, MaximumText);
}

Deque<SpatialLogger::Line> SpatialLogger::get_lines(std::string_view space, bool andClear) {
  std::lock_guard locker(s_mutex);
  return query_spatial(s_lines, space, andClear);
}

Deque<SpatialLogger::Point> SpatialLogger::get_points(std::string_view space, bool andClear) {
  std::lock_guard locker(s_mutex);
  return query_spatial(s_points, space, andClear);
}

Deque<SpatialLogger::LogText> SpatialLogger::get_text(std::string_view space, bool andClear) {
  std::lock_guard locker(s_mutex);
  return query_spatial(s_logText, space, andClear);
}

void SpatialLogger::clear() {
  decltype(s_lines) lines;
  decltype(s_points) points;
  decltype(s_logText) logText;
  {
    std::lock_guard locker(s_mutex);
    lines = std::move(s_lines);
    points = std::move(s_points);
    logText = std::move(s_logText);
  } // Move while locked to deallocate contents while unlocked.
}

bool SpatialLogger::observed() {
  return s_observed;
}

void SpatialLogger::set_observed(bool observed) { s_observed = observed; }
}

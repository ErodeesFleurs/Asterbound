#include <stdio.h>
#ifdef STAR_SYSTEM_FAMILY_WINDOWS
#include "StarWindowsCrt.hpp"
#include <windows.h>
#else
#include <syslog.h>
#endif
import std;
import star.logging;
import star.text_formatter;
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

LogSink::LogSink() : m_level(LogLevel::Info) {}

LogSink::~LogSink() = default;

void LogSink::set_level(LogLevel level) { m_level = level; }

LogLevel LogSink::level() { return m_level; }

std::expected<void, StarException> StdoutLogSink::log(std::string_view msg, LogLevel level) {
  std::lock_guard locker(m_logMutex);
  auto name = enumName<LogLevelNames>(level);
  if (!name)
    return std::unexpected(std::move(name.error()));
  return Star::print("[{}] {}\n", *name, msg);
}

std::expected<std::shared_ptr<FileLogSink>, StarException> FileLogSink::create(
    String const& filename, LogLevel level, bool truncate, std::uint64_t sizeLimit, unsigned backups,
    std::optional<String> backupFilename) {
  auto sink = std::make_shared<FileLogSink>(
      ConstructionKey{}, filename, sizeLimit, backups, backupFilename.value_or(filename));
  auto opened =
    sink->m_output.open(truncate ? IOMode::Write | IOMode::Append | IOMode::Truncate : IOMode::Write | IOMode::Append);
  if (!opened)
    return std::unexpected(std::move(opened.error()));
  if (sizeLimit && !truncate) {
    auto size = sink->m_output.size();
    if (!size)
      return std::unexpected(std::move(size.error()));
    sink->m_size = static_cast<std::uint64_t>(*size);
  }
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
  if (m_sizeLimit) {
    if (!m_size) {
      auto size = m_output.size();
      if (!size)
        return std::unexpected(std::move(size.error()));
      m_size = static_cast<std::uint64_t>(*size);
    }
    if (*m_size && (*m_size >= m_sizeLimit || line.size() > m_sizeLimit - *m_size)) {
      auto backup = File::backup_file_in_sequence(m_output.file_name(), m_backupFilename, m_backups);
      if (!backup)
        return std::unexpected(std::move(backup.error()));
      auto opened = m_output.open(IOMode::Write | IOMode::Append | IOMode::Truncate);
      if (!opened)
        return std::unexpected(std::move(opened.error()));
      m_size = 0;
    }
  }
  auto written = m_output.write_full(line);
  if (m_sizeLimit) {
    if (written)
      *m_size += line.size();
    else
      m_size.reset();
  }
  return written;
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
  bool debug = first_loggable(*sinks, LogLevel::Debug) != sinks->end();
  if (first == sinks->end() && !debug)
    return {};

  // Serialize the decision with emission, including same-thread sink reentry.
  std::lock_guard locker(s_dispatchMutex);
  auto const& summary = diagnostic.diagnostic_summary();
  bool limited = summary->kind == ExceptionSummaryKind::ScriptRuntime
      || summary->kind == ExceptionSummaryKind::ScriptSyntax
      || summary->kind == ExceptionSummaryKind::PatchRuntime
      || summary->kind == ExceptionSummaryKind::PatchSyntax;
  auto matches = [&](RepeatedException const& entry) {
    auto const& other = *entry.summary;
    return entry.level == level && entry.debug == debug && entry.operation == operation
        && other.kind == summary->kind && other.category == summary->category
        && other.reason == summary->reason && other.operation == summary->operation
        && other.asset == summary->asset && other.assetProvider == summary->assetProvider
        && other.patch == summary->patch && other.patchProvider == summary->patchProvider
        && std::ranges::equal(other.locations, summary->locations, [](auto const& a, auto const& b) {
             return a.source == b.source && a.line == b.line && a.provider == b.provider;
           });
  };
  std::uint64_t repeats = 0;
  if (limited) {
    auto now = std::chrono::steady_clock::now();
    auto entry = std::ranges::find_if(s_repeatedExceptions, matches);
    if (entry != s_repeatedExceptions.end()) {
      if (entry->repeats != std::numeric_limits<std::uint64_t>::max())
        ++entry->repeats;
      if (now - entry->reported < std::chrono::seconds(5))
        return {};
      repeats = std::exchange(entry->repeats, 0);
      entry->reported = now;
    } else {
      // Bound retained identities; never retain full exception stacks.
      if (s_repeatedExceptions.size() == 128) {
        auto oldest = std::ranges::min_element(s_repeatedExceptions, {}, &RepeatedException::reported);
        s_repeatedExceptions.erase(oldest);
      }
      s_repeatedExceptions.push_back({level, debug, std::string(operation), summary, now});
    }
  }
  auto emit = [&]() -> std::expected<void, StarException> {
    if (repeats != 0)
      return dispatch(*sinks, level, Star::format("{}: repeated {} additional times: {}",
          operation, repeats, Star::exception_summary(diagnostic)));
    if (first != sinks->end()) {
      auto message = Star::format("{}: {}", operation, Star::exception_summary(diagnostic));
      if (auto result = dispatch(*sinks, level, message); !result)
        return std::unexpected(std::move(result.error()));
    }
    if (debug) {
      auto details = Star::format("{} details:\n{}", operation, Star::output_exception(diagnostic, true));
      return dispatch(*sinks, LogLevel::Debug, details);
    }
    return {};
  };
  auto result = emit();
  // A failed write must not make the next attempt silently succeed.
  // Sink callbacks may have changed the vector: do not retain an iterator.
  if (!result && limited)
    std::erase_if(s_repeatedExceptions, matches);
  return result;
}

void Logger::reset_exception_repeats() {
  std::lock_guard locker(s_dispatchMutex);
  s_repeatedExceptions.clear();
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
  auto emitted = [&] {
#ifdef STAR_SYSTEM_FAMILY_WINDOWS
    // An invalid descriptor must reach the fallback, not the CRT's default
    // invalid-parameter termination. Leave the caller's thread policy intact.
    auto previous = ::_set_thread_local_invalid_parameter_handler(
      [](wchar_t const*, wchar_t const*, wchar_t const*, unsigned, std::uintptr_t) {});
    auto restore = finally([previous] { ::_set_thread_local_invalid_parameter_handler(previous); });
#endif
    return Star::println(stderr, "{}", message);
  }();
  if (emitted)
    return;

  // Diagnostics cannot decide the caller's control flow or prevent its error
  // dialog. Keep the write failure and original message on a stdio-independent
  // system channel; ordinary Logger sinks still propagate their I/O errors.
  auto diagnostic =
    Star::format("Asterbound emergency output failed: {}\n{}", Star::exception_summary(emitted.error()), message);
#ifdef STAR_SYSTEM_FAMILY_WINDOWS
  String text(std::move(diagnostic));
  auto wide = text.wstring();
  if (wide) {
    OutputDebugStringW(wide->c_str());
    return;
  }
  TextBuffer escaped;
  for (unsigned char byte : text.utf8()) {
    if ((byte >= 0x20 && byte < 0x7f) || byte == '\n' || byte == '\r' || byte == '\t')
      escaped.push_back(static_cast<char>(byte));
    else
      std::format_to(std::back_inserter(escaped), "\\x{:02X}", byte);
  }
  OutputDebugStringA(std::move(escaped).str().c_str());
#else
  ::syslog(LOG_ERR, "%s", diagnostic.c_str());
#endif
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
  if (!observed())
    return;

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
  if (!observed())
    return;

  std::lock_guard locker(s_mutex);
  auto& lines = s_lines[space];

  append_capped(lines, Line{.begin = begin, .end = end, .color = color}, MaximumLines);
}

void SpatialLogger::log_point(std::string_view space, Vec2F const& position, Vec4B const& color) {
  if (!observed())
    return;

  std::lock_guard locker(s_mutex);
  auto& points = s_points[space];

  append_capped(points, Point{.position = position, .color = color}, MaximumPoints);
}

void SpatialLogger::log_text(std::string_view space, String text, Vec2F const& position, Vec4B const& color) {
  if (!observed())
    return;

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
  }// Move while locked to deallocate contents while unlocked.
}

bool SpatialLogger::observed() { return s_observed; }

void SpatialLogger::set_observed(bool observed) { s_observed = observed; }
}// namespace Star

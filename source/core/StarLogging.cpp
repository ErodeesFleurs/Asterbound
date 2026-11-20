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

LogLevel LogSink::level() const noexcept { return m_level; }

std::expected<void, StarException> StdoutLogSink::log(std::string_view msg, LogLevel level) {
  std::lock_guard locker(m_log_mutex);
  auto name = enum_name<log_level_names>(level);
  if (!name)
    return std::unexpected(std::move(name.error()));
  return Star::print("[{}] {}\n", *name, msg);
}

std::expected<std::shared_ptr<FileLogSink>, StarException> FileLogSink::create(
    String const& filename, LogLevel level, bool truncate, std::uint64_t size_limit, unsigned backups,
    std::optional<String> backup_filename) {
  auto sink = std::make_shared<FileLogSink>(
      ConstructionKey{}, filename, size_limit, backups, std::move(backup_filename).value_or(filename));
  auto opened =
    sink->m_output.open(truncate ? IOMode::Write | IOMode::Append | IOMode::Truncate : IOMode::Write | IOMode::Append);
  if (!opened)
    return std::unexpected(std::move(opened.error()));
  if (size_limit && !truncate) {
    auto size = sink->m_output.size();
    if (!size)
      return std::unexpected(std::move(size.error()));
    sink->m_size = static_cast<std::uint64_t>(*size);
  }
  sink->set_level(level);
  return sink;
}

std::expected<void, StarException> FileLogSink::log(std::string_view msg, LogLevel level) {
  std::lock_guard locker(m_log_mutex);
  auto timestamp = Time::localTime(std::chrono::system_clock::now());
  if (!timestamp)
    return std::unexpected(std::move(timestamp.error()));
  auto name = enum_name<log_level_names>(level);
  if (!name)
    return std::unexpected(std::move(name.error()));
  // Keep ordinary records in the sink, not an occasional oversized diagnostic.
  constexpr std::size_t max_cached_message = 8 * 1024;
  constexpr std::size_t line_framing = std::string_view("[00:00:00.000] [] \n").size();
  std::string oversized;
  auto& line = msg.size() <= max_cached_message ? m_line : oversized;
  line.clear();
  line.reserve(line_framing + name->size() + msg.size());
  std::format_to(std::back_inserter(line), "[{:%H:%M:%S}] [{}] ", *timestamp, *name);
  line.append(msg);
  line.push_back('\n');
  if (m_size_limit) {
    if (!m_size) {
      auto size = m_output.size();
      if (!size)
        return std::unexpected(std::move(size.error()));
      m_size = static_cast<std::uint64_t>(*size);
    }
    if (*m_size && (*m_size >= m_size_limit || line.size() > m_size_limit - *m_size)) {
      auto backup = File::backup_file_in_sequence(m_output.file_name(), m_backup_filename, m_backups);
      if (!backup)
        return std::unexpected(std::move(backup.error()));
      auto opened = m_output.open(IOMode::Write | IOMode::Append | IOMode::Truncate);
      if (!opened)
        return std::unexpected(std::move(opened.error()));
      m_size = 0;
    }
  }
  auto written = m_output.write_full(line);
  if (m_size_limit) {
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
    std::lock_guard locker(m_registry_mutex);
    if (m_sinks->contains(sink))
      return;
    auto next = std::make_shared<SinkRegistry>(*m_sinks);
    next->insert(std::move(sink));
    retired = std::exchange(m_sinks, std::move(next));
  }
}

void Logger::remove_sink(std::shared_ptr<LogSink> sink) {
  std::shared_ptr<SinkRegistry const> retired;
  {
    std::lock_guard locker(m_registry_mutex);
    if (!m_sinks->contains(sink))
      return;
    auto next = std::make_shared<SinkRegistry>(*m_sinks);
    next->erase(sink);
    retired = std::exchange(m_sinks, std::move(next));
  }
}

std::shared_ptr<LogSink> Logger::stdout_sink() { return m_stdout_sink; }

void Logger::remove_stdout_sink() { remove_sink(m_stdout_sink); }

std::shared_ptr<Logger::SinkRegistry const> Logger::sink_snapshot(LogLevel level) {
  std::lock_guard locker(m_registry_mutex);
  if (!accepts(*m_sinks, level))
    return {};
  return m_sinks;
}

bool Logger::accepts(SinkRegistry const& sinks, LogLevel level) {
  return std::any_of(sinks.begin(), sinks.end(),
                     [level](std::shared_ptr<LogSink> const& sink) { return sink->level() <= level; });
}

std::expected<void, StarException> Logger::dispatch(SinkRegistry const& sinks, LogLevel level,
                                                    std::string_view message) {
  std::lock_guard locker(m_dispatch_mutex);
  return dispatch_locked(sinks, level, message);
}

std::expected<void, StarException> Logger::dispatch_locked(SinkRegistry const& sinks, LogLevel level,
                                                           std::string_view message) {
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
  auto sinks = sink_snapshot(level);
  if (!sinks)
    return {};

  // Serialize the decision with emission, including same-thread sink reentry.
  std::lock_guard locker(m_dispatch_mutex);
  if (!accepts(*sinks, level))
    return {};
  auto const& summary = diagnostic.diagnostic_summary();
  bool limited = summary->kind == ExceptionSummaryKind::ScriptRuntime
      || summary->kind == ExceptionSummaryKind::ScriptSyntax
      || summary->kind == ExceptionSummaryKind::PatchRuntime
      || summary->kind == ExceptionSummaryKind::PatchSyntax;
  std::uint64_t delivery_id = 0;
  std::uint64_t repeats = 0;
  if (limited) {
    bool debug = accepts(*sinks, LogLevel::Debug);
    auto matches = [&](RepeatedException const& entry) {
      auto const& other = *entry.summary;
      return entry.level == level && entry.debug == debug && entry.operation == operation
          && other.kind == summary->kind && other.category == summary->category
          && other.reason == summary->reason && other.operation == summary->operation
          && other.asset == summary->asset && other.assetProvider == summary->assetProvider
          && other.patch == summary->patch && other.patchProvider == summary->patchProvider
          && std::ranges::equal(other.locations, summary->locations,
              [](ExceptionSummary::Location const& a, ExceptionSummary::Location const& b) {
                return a.source == b.source && a.line == b.line && a.provider == b.provider;
              });
    };
    auto now = std::chrono::steady_clock::now();
    auto entry = std::ranges::find_if(m_repeated_exceptions, matches);
    if (entry != m_repeated_exceptions.end()) {
      if (entry->repeats != std::numeric_limits<std::uint64_t>::max())
        ++entry->repeats;
      if (now - entry->reported < std::chrono::seconds(5))
        return {};
      repeats = std::exchange(entry->repeats, 0);
      entry->reported = now;
    } else {
      // Bound retained identities; never retain full exception stacks.
      if (m_repeated_exceptions.size() == 128) {
        auto oldest = std::ranges::min_element(m_repeated_exceptions, {}, &RepeatedException::reported);
        m_repeated_exceptions.erase(oldest);
      }
      m_repeated_exceptions.push_back({level, debug, std::string(operation), summary, now});
      entry = std::prev(m_repeated_exceptions.end());
    }
    delivery_id = ++m_delivery_sequence;
    entry->delivery_id = delivery_id;
  }
  auto result = dispatch_locked(*sinks, level, repeats == 0
      ? Star::format("{}: {}", operation, Star::exception_summary(diagnostic))
      : Star::format("{}: repeated {} additional times: {}", operation, repeats, Star::exception_summary(diagnostic)));
  // A summary callback can enable or disable details on the retained snapshot.
  if (result && repeats == 0 && accepts(*sinks, LogLevel::Debug))
    result = dispatch_locked(*sinks, LogLevel::Debug,
        Star::format("{} details:\n{}", operation, Star::output_exception(diagnostic, true)));

  // Retry a failed delivery, but don't erase a newer one installed by a callback.
  if (!result && limited)
    std::erase_if(m_repeated_exceptions, [delivery_id](RepeatedException const& entry) {
      return entry.delivery_id == delivery_id;
    });
  return result;
}

void Logger::reset_exception_repeats() {
  std::lock_guard locker(m_dispatch_mutex);
  m_repeated_exceptions.clear();
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
    : m_stdout_sink(std::make_shared<StdoutLogSink>()),
      m_sinks(std::make_shared<SinkRegistry>(std::initializer_list<std::shared_ptr<LogSink>>{m_stdout_sink})) {}

bool Logger::loggable(LogLevel level) {
  std::lock_guard locker(m_registry_mutex);
  return accepts(*m_sinks, level);
}

void EmergencyLog::log(LogLevel, std::string_view message) {
  auto emitted = [&] {
#ifdef STAR_SYSTEM_FAMILY_WINDOWS
    // An invalid descriptor must reach the fallback, not the CRT's default
    // invalid-parameter termination. Leave the caller's thread policy intact.
    auto previous = ::_set_thread_local_invalid_parameter_handler(
      [](wchar_t const*, wchar_t const*, wchar_t const*, unsigned, std::uintptr_t) {});
    auto restore = make_scope_exit([previous] { ::_set_thread_local_invalid_parameter_handler(previous); });
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

String LogMap::get_value(String const& key) const {
  std::lock_guard lock(m_mutex);
  return m_values.value(key);
}

void LogMap::set_value(String const& key, String const& value) {
  std::lock_guard lock(m_mutex);
  m_values.insert_or_assign(key, value);
}

Map<String, String> LogMap::snapshot() const {
  std::lock_guard lock(m_mutex);
  return Map<String, String>::from(m_values);
}

Map<String, String> LogMap::drain() {
  auto detached = [&] {
    std::lock_guard lock(m_mutex);
    return std::move(m_values);
  }();
  Map<String, String> sorted;
  for (auto&& [key, value] : detached)
    sorted.emplace(key, std::move(value));
  return sorted;
}

void LogMap::restore_missing(Map<String, String> const& drained) {
  std::lock_guard lock(m_mutex);
  for (auto const& [key, value] : drained)
    m_values.try_emplace(key, value);
}

void LogMap::clear() {
  std::lock_guard lock(m_mutex);
  m_values.clear();
}


namespace {

template <typename T>
concept spatial_log_value = std::copy_constructible<T>;

template <spatial_log_value T> void append_capped(Deque<T>& values, T value, std::size_t maximum) {
  values.push_back(std::move(value));
  while (values.size() > maximum)
    values.pop_front();
}

template <spatial_log_value T>
Deque<T> query_spatial(StringMap<Deque<T>>& spaces, std::string_view space, bool and_clear) {
  auto found = spaces.find(space);
  if (found == spaces.end())
    return {};
  if (and_clear)
    return std::exchange(found->second, {});
  return found->second;
}

}// namespace

void SpatialLogger::log_poly(std::string_view space, PolyF const& poly, Vec4B const& color) {
  if (!observed())
    return;

  std::lock_guard locker(m_mutex);
  auto& lines = m_lines[space];

  auto sides = poly.sides();
  auto first = sides > max_lines ? sides - max_lines : 0;
  if (first != 0)
    lines.clear();
  for (std::size_t i = first; i < sides; ++i) {
    auto side = poly.side(i);
    append_capped(lines, Line{.begin = side.min(), .end = side.max(), .color = color}, max_lines);
  }
}

void SpatialLogger::log_line(std::string_view space, Line2F const& line, Vec4B const& color) {
  log_line(space, line.min(), line.max(), color);
}

void SpatialLogger::log_line(std::string_view space, Vec2F const& begin, Vec2F const& end, Vec4B const& color) {
  if (!observed())
    return;

  std::lock_guard locker(m_mutex);
  auto& lines = m_lines[space];

  append_capped(lines, Line{.begin = begin, .end = end, .color = color}, max_lines);
}

void SpatialLogger::log_point(std::string_view space, Vec2F const& position, Vec4B const& color) {
  if (!observed())
    return;

  std::lock_guard locker(m_mutex);
  auto& points = m_points[space];

  append_capped(points, Point{.position = position, .color = color}, max_points);
}

void SpatialLogger::log_text(std::string_view space, String text, Vec2F const& position, Vec4B const& color) {
  if (!observed())
    return;

  std::lock_guard locker(m_mutex);
  auto& texts = m_text[space];

  append_capped(texts, LogText{.text = std::move(text), .position = position, .color = color}, max_text);
}

Deque<SpatialLogger::Line> SpatialLogger::get_lines(std::string_view space, bool and_clear) {
  std::lock_guard locker(m_mutex);
  return query_spatial(m_lines, space, and_clear);
}

Deque<SpatialLogger::Point> SpatialLogger::get_points(std::string_view space, bool and_clear) {
  std::lock_guard locker(m_mutex);
  return query_spatial(m_points, space, and_clear);
}

Deque<SpatialLogger::LogText> SpatialLogger::get_text(std::string_view space, bool and_clear) {
  std::lock_guard locker(m_mutex);
  return query_spatial(m_text, space, and_clear);
}

void SpatialLogger::clear() {
  decltype(m_lines) lines;
  decltype(m_points) points;
  decltype(m_text) text;
  {
    std::lock_guard locker(m_mutex);
    lines = std::move(m_lines);
    points = std::move(m_points);
    text = std::move(m_text);
  }// Move while locked to deallocate contents while unlocked.
}

bool SpatialLogger::observed() const noexcept { return m_observed; }

void SpatialLogger::set_observed(bool observed) { m_observed = observed; }
}// namespace Star

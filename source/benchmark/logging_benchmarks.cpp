#include <benchmark/benchmark.h>
#include <cstdio>

import std;
import star.algorithm;
import star.array;
import star.byte_array;
import star.file;
import star.format;
import star.list;
import star.logging;
import star.memory;
import star.native_runtime;
import star.random;
import star.string;
import star.thread;
import star.vector;

namespace {
constexpr std::size_t batch_size = 64;
constexpr std::size_t caller_count = 4;
constexpr std::size_t capture_capacity = batch_size * caller_count;
constexpr std::string_view space = "logging-benchmark-space";
constexpr Star::Vec4B color{17, 43, 127, 255};

std::array<std::string, batch_size> fixture() {
  std::array<std::string, batch_size> messages;
  for (std::size_t i = 0; i != batch_size; ++i)
    messages[i] = std::format("logging-fixture seq={:02} payload=0123456789abcdef", i);
  return messages;
}

bool require(benchmark::State& state, bool valid) {
  if (!valid)
    state.SkipWithError("Complete logging output differs from deterministic fixture");
  return valid;
}

void account(benchmark::State& state, std::size_t count, std::size_t bytes, std::string_view detail) {
  state.SetItemsProcessed(state.iterations() * count);
  state.SetBytesProcessed(state.iterations() * bytes);
  state.SetLabel(std::format("fixture=logging-seq00-63-v1; batch={}; {}; setup/validation untimed", count, detail));
}

// Capture complete bytes into preallocated slots; no formatting, allocation or
// correctness comparisons in the timed sink. Atomic reservation also permits
// concurrent dispatch if the logger releases its registry lock before delivery.
class capture_sink : public Star::LogSink {
public:
  struct entry {
    std::array<char, 96> bytes;
    std::size_t size;
    Star::LogLevel level;
  };

  std::expected<void, Star::StarException> log(std::string_view message, Star::LogLevel level) override {
    auto slot = count.fetch_add(1, std::memory_order_relaxed);
    if (slot < entries.size()) {
      auto& output = entries[slot];
      output.size = message.size();
      output.level = level;
      std::ranges::copy(message.substr(0, output.bytes.size()), output.bytes.begin());
    }
    return {};
  }

  bool validate(std::array<std::string, batch_size> const& expected, std::size_t repetitions = 1) const {
    if (count.load(std::memory_order_relaxed) != batch_size * repetitions)
      return false;
    std::array<std::size_t, batch_size> seen{};
    for (std::size_t i = 0; i != batch_size * repetitions; ++i) {
      auto const& output = entries[i];
      if (output.size > output.bytes.size() || output.level != Star::LogLevel::Info)
        return false;
      auto message = std::string_view(output.bytes.data(), output.size);
      auto found = std::ranges::find(expected, message);
      if (found == expected.end())
        return false;
      ++seen[static_cast<std::size_t>(found - expected.begin())];
    }
    return std::ranges::all_of(seen, [&](std::size_t value) { return value == repetitions; });
  }

  void reset() { count.store(0, std::memory_order_relaxed); }
  std::atomic<std::size_t> count{0};
  std::array<entry, capture_capacity> entries;
};

void logger_messages(benchmark::State& state, bool formatted, std::size_t sinks, bool disabled = false) {
  auto messages = fixture();
  Star::Logger logger;
  logger.remove_stdout_sink();
  std::vector<std::shared_ptr<capture_sink>> outputs;
  for (std::size_t i = 0; i != sinks; ++i) {
    auto sink = std::make_shared<capture_sink>();
    sink->set_level(disabled ? Star::LogLevel::Warn : Star::LogLevel::Info);
    logger.add_sink(sink);
    outputs.push_back(std::move(sink));
  }
  for (auto _ : state) {
    (void)_;
    bool success = true;
    for (std::size_t i = 0; i != batch_size; ++i) {
      auto result = formatted ? logger.info("logging-fixture seq={:02} payload=0123456789abcdef", i)
                              : logger.log(Star::LogLevel::Info, messages[i]);
      if (!result) {
        state.SkipWithError(result.error().what());
        success = false;
        break;
      }
    }
    state.PauseTiming();
    for (auto const& output : outputs) {
      success &= disabled ? output->count.load() == 0 : output->validate(messages);
      output->reset();
    }
    state.ResumeTiming();
    if (!require(state, success))
      break;
  }
  account(state, batch_size, batch_size * messages[0].size(),
          disabled ? "threshold rejects messages; no sink calls" : "preallocated complete-byte capture sinks");
}

void concurrent_callers(benchmark::State& state) {
  auto messages = fixture();
  Star::Logger logger;
  logger.remove_stdout_sink();
  auto sink = std::make_shared<capture_sink>();
  logger.add_sink(sink);
  std::atomic<unsigned> generation{0};
  std::atomic<unsigned> completed{0};
  std::atomic<bool> stop{false};
  std::atomic<bool> success{true};
  std::array<Star::ThreadFunction<void>, caller_count> threads;
  auto cleanup = Star::finally([&] {
    stop.store(true, std::memory_order_relaxed);
    generation.fetch_add(1, std::memory_order_release);
    generation.notify_all();
    for (auto& thread : threads) {
      auto finished = thread.finish();
      if (!finished)
        state.SkipWithError(finished.error().what());
    }
  });
  for (auto& thread : threads) {
    auto started = Star::Thread::invoke("logging-benchmark", [&]() -> std::expected<void, Star::StarException> {
      unsigned previous = 0;
      while (true) {
        generation.wait(previous, std::memory_order_acquire);
        auto current = generation.load(std::memory_order_acquire);
        if (stop.load(std::memory_order_relaxed))
          return {};
        previous = current;
        for (auto const& message : messages) {
          auto result = logger.log(Star::LogLevel::Info, message);
          if (!result) {
            success.store(false, std::memory_order_relaxed);
            completed.fetch_add(1, std::memory_order_release);
            completed.notify_one();
            return std::unexpected(std::move(result.error()));
          }
        }
        completed.fetch_add(1, std::memory_order_release);
        completed.notify_one();
      }
    });
    if (!started) {
      state.SkipWithError(started.error().what());
      return;
    }
    thread = std::move(*started);
  }
  for (auto _ : state) {
    (void)_;
    generation.fetch_add(1, std::memory_order_release);
    generation.notify_all();
    auto done = completed.load(std::memory_order_acquire);
    while (done != caller_count) {
      completed.wait(done, std::memory_order_acquire);
      done = completed.load(std::memory_order_acquire);
    }
    state.PauseTiming();
    bool valid = success.load() && sink->validate(messages, caller_count);
    sink->reset();
    completed.store(0, std::memory_order_relaxed);
    state.ResumeTiming();
    if (!require(state, valid))
      break;
  }
  account(state, batch_size * caller_count, batch_size * caller_count * messages[0].size(),
          "4 persistent native callers; includes wakeup/completion; process CPU time");
}

bool valid_file(std::string_view contents, std::array<std::string, batch_size> const& messages, std::size_t batches) {
  constexpr std::size_t prefix_size = 22;// [HH:MM:SS.mmm] [Info]
  std::size_t cursor = 0;
  for (std::size_t batch = 0; batch != batches; ++batch) {
    for (auto const& message : messages) {
      auto width = prefix_size + message.size() + 1;
      if (contents.size() - cursor < width)
        return false;
      auto line = contents.substr(cursor, width);
      if (line[0] != '[' || line[3] != ':' || line[6] != ':' || line[9] != '.' || line.substr(13, 9) != "] [Info] "
          || line.substr(prefix_size, message.size()) != message || line.back() != '\n')
        return false;
      for (auto offset : {1, 2, 4, 5, 7, 8, 10, 11, 12})
        if (line[offset] < '0' || line[offset] > '9')
          return false;
      cursor += width;
    }
  }
  return cursor == contents.size();
}

void file_lines(benchmark::State& state) {
  auto random = Star::RandomService::create();
  if (!random) {
    state.SkipWithError(random.error().what());
    return;
  }
  auto temporary = Star::File::temporary_directory(**random);
  if (!temporary) {
    state.SkipWithError(temporary.error().what());
    return;
  }
  auto cleanup = Star::finally([&] {
    auto removed = Star::File::remove_all(*temporary);
    if (!removed)
      state.SkipWithError(removed.error().what());
  });
  auto path = Star::File::relative_to(*temporary, "logging-lines.log");
  auto output = Star::FileLogSink::create(path, Star::LogLevel::Info, true);
  if (!output) {
    state.SkipWithError(output.error().what());
    return;
  }
  Star::Logger logger;
  logger.remove_stdout_sink();
  logger.add_sink(*output);
  auto messages = fixture();
  for (auto _ : state) {
    (void)_;
    bool success = true;
    for (auto const& message : messages) {
      auto result = logger.log(Star::LogLevel::Info, message);
      if (!result) {
        state.SkipWithError(result.error().what());
        success = false;
        break;
      }
    }
    if (!success)
      break;
  }
  logger.remove_sink(*output);
  output->reset();
  auto contents = Star::File::read_file(path);
  if (!contents)
    state.SkipWithError(contents.error().what());
  else
    require(state, valid_file(std::string_view(contents->data(), contents->size()), messages, state.iterations()));
  account(state, batch_size, batch_size * (messages[0].size() + 23),
          "warm private temp file; complete timestamped lines; no durability claim");
}

void spatial_append_drain(benchmark::State& state, bool observed) {
  Star::SpatialLogger logger;
  logger.set_observed(observed);
  auto messages = fixture();
  for (auto _ : state) {
    (void)_;
    for (std::size_t i = 0; i != batch_size; ++i) {
      Star::Vec2F position{static_cast<float>(i), 13.0f};
      logger.log_line(space, position, Star::Vec2F{19.0f, 23.0f}, color);
      logger.log_point(space, position, color);
      logger.log_text(space, Star::String(messages[i]), position, color);
    }
    if (!observed)
      state.PauseTiming();
    auto lines = logger.get_lines(space, true);
    auto points = logger.get_points(space, true);
    auto texts = logger.get_text(space, true);
    benchmark::DoNotOptimize(lines.size());
    benchmark::DoNotOptimize(points.size());
    benchmark::DoNotOptimize(texts.size());
    if (observed)
      state.PauseTiming();
    bool valid = lines.size() == (observed ? batch_size : 0) && points.size() == (observed ? batch_size : 0)
      && texts.size() == (observed ? batch_size : 0);
    if (valid && observed) {
      for (std::size_t i = 0; i != batch_size; ++i) {
        Star::Vec2F position{static_cast<float>(i), 13.0f};
        valid &= lines[i].begin == position && lines[i].end == Star::Vec2F{19.0f, 23.0f} && lines[i].color == color
          && points[i].position == position && points[i].color == color && texts[i].text == Star::String(messages[i])
          && texts[i].position == position && texts[i].color == color;
      }
    }
    valid &= logger.get_lines(space, true).empty() && logger.get_points(space, true).empty()
      && logger.get_text(space, true).empty();
    state.ResumeTiming();
    if (!require(state, valid))
      break;
  }
  account(state, batch_size * 3, 0,
          observed ? "line/point/text append and owning drain"
                   : "unobserved line/point/text append; empty drain untimed");
}

void spatial_capacity(benchmark::State& state) {
  Star::SpatialLogger logger;
  logger.set_observed(true);
  for (std::size_t i = 0; i != Star::SpatialLogger::MaximumPoints; ++i)
    logger.log_point(space, Star::Vec2F{static_cast<float>(i), -1.0f}, color);
  std::uint64_t batches = 0;
  for (auto _ : state) {
    (void)_;
    for (std::size_t i = 0; i != batch_size; ++i)
      logger.log_point(space, Star::Vec2F{static_cast<float>(i), 13.0f}, color);
    ++batches;
  }
  auto points = logger.get_points(space, true);
  bool valid = points.size() == Star::SpatialLogger::MaximumPoints;
  auto appended = batches * batch_size;
  auto initial_remaining =
    appended < Star::SpatialLogger::MaximumPoints ? Star::SpatialLogger::MaximumPoints - appended : 0;
  if (valid) {
    for (std::size_t i = 0; i != points.size(); ++i) {
      auto expected = i < initial_remaining
        ? Star::Vec2F{static_cast<float>(i + appended), -1.0f}
        : Star::Vec2F{static_cast<float>((appended - (points.size() - i)) % batch_size), 13.0f};
      valid &= points[i].position == expected && points[i].color == color;
    }
  }
  require(state, valid && logger.get_points(space, true).empty());
  account(state, batch_size, 0, "200000 retained points; capacity-bound append/evict; final full drain untimed");
}

void disabled_formatted(benchmark::State& state) { logger_messages(state, true, 1, true); }
void disabled_raw(benchmark::State& state) { logger_messages(state, false, 1, true); }
void raw_one(benchmark::State& state) { logger_messages(state, false, 1); }
void formatted_one(benchmark::State& state) { logger_messages(state, true, 1); }
void raw_four(benchmark::State& state) { logger_messages(state, false, 4); }
void formatted_four(benchmark::State& state) { logger_messages(state, true, 4); }
void spatial_unobserved(benchmark::State& state) { spatial_append_drain(state, false); }
void spatial_observed(benchmark::State& state) { spatial_append_drain(state, true); }

BENCHMARK(disabled_formatted)->Name("Logging/DisabledFormatted");
BENCHMARK(disabled_raw)->Name("Logging/DisabledRaw");
BENCHMARK(raw_one)->Name("Logging/RawOneSink");
BENCHMARK(formatted_one)->Name("Logging/FormattedOneSink");
BENCHMARK(raw_four)->Name("Logging/RawFourSinks");
BENCHMARK(formatted_four)->Name("Logging/FormattedFourSinks");
BENCHMARK(concurrent_callers)->Name("Logging/ConcurrentFourCallers")->MeasureProcessCPUTime();
BENCHMARK(file_lines)->Name("Logging/FileCompleteLines");
BENCHMARK(spatial_unobserved)->Name("Logging/SpatialUnobserved");
BENCHMARK(spatial_observed)->Name("Logging/SpatialObservedDrain");
BENCHMARK(spatial_capacity)->Name("Logging/SpatialCapacitySteady");

class checked_reporter : public benchmark::ConsoleReporter {
public:
  void ReportRuns(std::vector<Run> const& reports) override {
    for (auto const& run : reports)
      failed |= run.skipped == benchmark::internal::SkippedWithError;
    benchmark::ConsoleReporter::ReportRuns(reports);
  }
  bool failed = false;
};
}// namespace

int main(int argc, char** argv) {
  auto runtime = Star::NativeRuntime::create();
  if (!runtime) {
    if (!Star::print(stderr, "{}\n", runtime.error().what()))
      return 1;
    return 1;
  }
  benchmark::Initialize(&argc, argv);
  if (benchmark::ReportUnrecognizedArguments(argc, argv)) {
    benchmark::Shutdown();
    return 1;
  }
  checked_reporter reporter;
  auto matched = benchmark::RunSpecifiedBenchmarks(&reporter);
  benchmark::Shutdown();
  return reporter.failed || matched == 0 ? 1 : 0;
}

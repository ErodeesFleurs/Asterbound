#include <benchmark/benchmark.h>

import std;
import star.algorithm;
import star.btree_database;
import star.byte_array;
import star.file;
import star.format;
import star.io_device;
import star.logging;
import star.random;
import star.string;

namespace {

struct phase_metrics {
  std::uint64_t write_calls = 0;
  std::uint64_t write_bytes = 0;
  std::uint64_t sync_calls = 0;
  double write_ns = 0;
  double sync_ns = 0;
};

// Investigation-only instrumentation: every measured operation delegates to the
// real File. The three actual sync calls delimit block, inactive-root and
// selector phases; no production hook, simulated latency or durability shortcut.
class commit_diagnostic_file : public Star::File {
public:
  using File::File;

  bool measuring = false;
  unsigned phase = 0;
  std::array<phase_metrics, 3> metrics{};
  std::uint64_t adjacent_block_writes = 0;
  std::optional<Star::StreamOffset> previous_block_end;

  void begin_measurement() {
    phase = 0;
    metrics = {};
    adjacent_block_writes = 0;
    previous_block_end.reset();
    measuring = true;
  }

  std::expected<std::size_t, Star::StarException> write(std::span<char const> data) override {
    if (!measuring || phase >= metrics.size())
      return File::write(data);
    auto start = std::chrono::steady_clock::now();
    auto result = File::write(data);
    auto elapsed = std::chrono::steady_clock::now() - start;
    auto& metric = metrics[phase];
    ++metric.write_calls;
    metric.write_ns += std::chrono::duration<double, std::nano>(elapsed).count();
    if (result)
      metric.write_bytes += *result;
    return result;
  }

  std::expected<std::size_t, Star::StarException> write_absolute(Star::StreamOffset position,
                                                                 std::span<char const> data) override {
    if (!measuring || phase >= metrics.size())
      return File::write_absolute(position, data);
    auto start = std::chrono::steady_clock::now();
    auto result = File::write_absolute(position, data);
    auto elapsed = std::chrono::steady_clock::now() - start;
    auto& metric = metrics[phase];
    ++metric.write_calls;
    metric.write_ns += std::chrono::duration<double, std::nano>(elapsed).count();
    if (result) {
      metric.write_bytes += *result;
      if (phase == 0) {
        if (previous_block_end == position)
          ++adjacent_block_writes;
        previous_block_end = position + static_cast<Star::StreamOffset>(*result);
      }
    }
    return result;
  }

  std::expected<void, Star::StarException> sync() override {
    if (!measuring || phase >= metrics.size())
      return File::sync();
    auto start = std::chrono::steady_clock::now();
    auto result = File::sync();
    auto elapsed = std::chrono::steady_clock::now() - start;
    auto& metric = metrics[phase];
    ++metric.sync_calls;
    metric.sync_ns += std::chrono::duration<double, std::nano>(elapsed).count();
    if (result)
      ++phase;
    return result;
  }
};

Star::ByteArray diagnostic_payload(std::size_t size, std::size_t key, unsigned generation) {
  Star::ByteArray bytes(size, '\0');
  for (std::size_t i = 0; i != size; ++i)
    bytes[i] = static_cast<char>((i * 17 + key * 31 + generation * 67) % 251);
  return bytes;
}

// Match File/BTree/Commit's 32 seeded + 32 new keys and two 64-key
// mutation passes, but keep this instrumented workload separate from its A/B
// throughput measurement. Snapshot restore, staging and readback are untimed.
struct commit_diagnostic_fixture {
  Star::Logger logger;
  Star::String path;
  std::shared_ptr<commit_diagnostic_file> device;
  std::unique_ptr<Star::BTreeDatabase> database;
  Star::ByteArray seed_file;
  std::array<Star::ByteArray, 64> keys;
  std::array<Star::ByteArray, 64> inserted;
  std::array<Star::ByteArray, 64> overwritten;

  std::expected<void, Star::StarException> close() {
    if (database && database->isOpen()) {
      if (auto result = database->close(true); !result)
        return result;
    } else if (device && device->is_open()) {
      if (auto result = device->close(); !result)
        return result;
    }
    database.reset();
    device.reset();
    return {};
  }

  std::expected<void, Star::StarException> open() {
    device = std::make_shared<commit_diagnostic_file>(path);
    database = std::make_unique<Star::BTreeDatabase>(logger, "FileBenchmark", 4);
    if (auto result = database->setAutoCommit(false); !result)
      return result;
    if (auto result = database->setBlockSize(4096); !result)
      return result;
    if (auto result = database->setIODevice(device); !result)
      return result;
    auto result = database->open();
    if (!result)
      return std::unexpected(std::move(result.error()));
    return {};
  }

  std::expected<void, Star::StarException> initialize(Star::String file_path, std::size_t value_size) {
    path = std::move(file_path);
    for (std::size_t i = 0; i != keys.size(); ++i) {
      keys[i] = Star::ByteArray(4, '\0');
      for (unsigned byte = 0; byte != 4; ++byte)
        keys[i][byte] = static_cast<char>((i >> ((3 - byte) * 8)) & 255);
      inserted[i] = diagnostic_payload(value_size, i, 1);
      overwritten[i] = diagnostic_payload(value_size, i, 2);
    }
    if (auto result = open(); !result)
      return result;
    for (std::size_t i = 0; i != 32; ++i) {
      auto result = database->insert(keys[i], diagnostic_payload(12288, i, 0));
      if (!result)
        return std::unexpected(std::move(result.error()));
    }
    if (auto result = database->commit(); !result)
      return result;
    if (auto result = close(); !result)
      return result;
    auto bytes = Star::File::read_file(path);
    if (!bytes)
      return std::unexpected(std::move(bytes.error()));
    seed_file = std::move(*bytes);
    return {};
  }

  std::expected<void, Star::StarException> restore_and_stage() {
    if (auto result = close(); !result)
      return result;
    if (auto result = Star::File::write_file(seed_file, path); !result)
      return result;
    if (auto result = open(); !result)
      return result;
    // Do not charge restoration's dirty pages to the commit being diagnosed.
    if (auto result = device->sync(); !result)
      return result;
    for (std::size_t i = 0; i != keys.size(); ++i) {
      auto result = database->insert(keys[i], inserted[i]);
      if (!result)
        return std::unexpected(std::move(result.error()));
      if (*result != (i < 32))
        return std::unexpected(Star::StarException("Diagnostic insert status mismatch"));
    }
    for (std::size_t i = 0; i != keys.size(); ++i) {
      auto result = database->insert(keys[i], overwritten[i]);
      if (!result)
        return std::unexpected(std::move(result.error()));
      if (!*result)
        return std::unexpected(Star::StarException("Diagnostic overwrite status mismatch"));
    }
    return {};
  }

  std::expected<void, Star::StarException> verify_reopen() {
    if (auto result = close(); !result)
      return result;
    if (auto result = open(); !result)
      return result;
    auto count = database->recordCount();
    if (!count)
      return std::unexpected(std::move(count.error()));
    if (*count != keys.size())
      return std::unexpected(Star::StarException("Diagnostic record count mismatch"));
    for (std::size_t i = 0; i != keys.size(); ++i) {
      auto found = database->find(keys[i]);
      if (!found)
        return std::unexpected(std::move(found.error()));
      if (!*found || **found != overwritten[i])
        return std::unexpected(Star::StarException("Diagnostic bytes did not survive commit/reopen"));
    }
    return {};
  }
};

void btree_commit_phase_diagnostic(benchmark::State& state) {
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
  commit_diagnostic_fixture fixture;
  auto cleanup = Star::finally([&] {
    if (auto result = fixture.close(); !result)
      state.SkipWithError(result.error().what());
    if (auto result = Star::File::remove_all(*temporary); !result)
      state.SkipWithError(result.error().what());
  });
  auto initialized =
    fixture.initialize(Star::File::relative_to(*temporary, "commit.btree"), static_cast<std::size_t>(state.range(0)));
  if (!initialized) {
    state.SkipWithError(initialized.error().what());
    return;
  }
  std::array<phase_metrics, 3> totals{};
  std::uint64_t adjacent = 0;
  std::uint64_t measured = 0;
  for (auto _ : state) {
    state.PauseTiming();
    auto staged = fixture.restore_and_stage();
    if (!staged) {
      state.SkipWithError(staged.error().what());
      state.ResumeTiming();
      break;
    }
    fixture.device->begin_measurement();
    state.ResumeTiming();
    auto committed = fixture.database->commit();
    state.PauseTiming();
    fixture.device->measuring = false;
    if (!committed) {
      state.SkipWithError(committed.error().what());
      state.ResumeTiming();
      break;
    }
    if (fixture.device->phase != 3 || fixture.device->metrics[1].write_bytes != 17
        || fixture.device->metrics[2].write_bytes != 1) {
      state.SkipWithError("Diagnostic did not observe three commit barriers and exact root/selector bytes");
      state.ResumeTiming();
      break;
    }
    for (std::size_t i = 0; i != totals.size(); ++i) {
      auto const& metric = fixture.device->metrics[i];
      totals[i].write_calls += metric.write_calls;
      totals[i].write_bytes += metric.write_bytes;
      totals[i].sync_calls += metric.sync_calls;
      totals[i].write_ns += metric.write_ns;
      totals[i].sync_ns += metric.sync_ns;
    }
    adjacent += fixture.device->adjacent_block_writes;
    ++measured;
    auto verified = fixture.verify_reopen();
    state.ResumeTiming();
    if (!verified) {
      state.SkipWithError(verified.error().what());
      break;
    }
  }
  if (measured) {
    std::array<std::string, 3> const names{"blocks", "root", "selector"};
    for (std::size_t i = 0; i != totals.size(); ++i) {
      auto const& metric = totals[i];
      state.counters[names[i] + "_write_calls"] = static_cast<double>(metric.write_calls) / measured;
      state.counters[names[i] + "_write_bytes"] = static_cast<double>(metric.write_bytes) / measured;
      state.counters[names[i] + "_sync_calls"] = static_cast<double>(metric.sync_calls) / measured;
      state.counters[names[i] + "_write_us"] = metric.write_ns / measured / 1000;
      state.counters[names[i] + "_sync_us"] = metric.sync_ns / measured / 1000;
    }
    state.counters["adjacent_block_pairs"] = static_cast<double>(adjacent) / measured;
  }
  state.SetLabel("INSTRUMENTED real File; per-commit call/byte totals and summed IO us by sync-delimited phase; "
                 "128 mutations, 4096-byte blocks; not an uninstrumented throughput A/B");
}

BENCHMARK(btree_commit_phase_diagnostic)->Name("File/BTree/CommitPhaseDiagnostic")->Arg(64)->Arg(12288)->UseRealTime();

}// namespace

#include <cstdio>
#include <benchmark/benchmark.h>
#ifdef __linux__
#include <fcntl.h>
#include <unistd.h>
#endif

import std;
import star.format;
import star.byte_array;
import star.file;
import star.io_device;
import star.lock_file;
import star.benchmark_runner;
import star.random;
import star.string;

namespace {

using Star::File;
using Star::String;

bool require(benchmark::State& state, bool condition, std::string_view message) {
  if (!condition)
    state.SkipWithError(std::string(message));
  return condition;
}

// Each invocation owns its directory, including replacement staging files.
// This object's lifetime encloses the State range: setup and checked cleanup
// are untimed, while returned read buffers are destroyed inside that range.
struct FileFixture {
  benchmark::State& state;
  String directory;
  String path;
  std::vector<char> bytes;
  std::shared_ptr<File> handle;

  explicit FileFixture(benchmark::State& benchmark_state) : state(benchmark_state) {}

  ~FileFixture() {
    if (handle) {
      auto closed = handle->close();
      if (!closed)
        state.SkipWithError(closed.error().what());
      handle.reset();
    }
    if (!directory.empty()) {
      auto removed = File::remove_all(directory);
      if (!removed) {
        state.SkipWithError(removed.error().what());
        return;
      }
      auto remaining = File::exists(directory);
      if (!remaining)
        state.SkipWithError(remaining.error().what());
      else if (*remaining)
        state.SkipWithError("Private benchmark directory survived cleanup");
    }
  }

  bool prepare_directory() {
    auto random = Star::RandomService::create();
    if (!random) {
      state.SkipWithError(random.error().what());
      return false;
    }
    auto temporary = File::temporary_directory(**random);
    if (!temporary) {
      state.SkipWithError(temporary.error().what());
      return false;
    }
    directory = std::move(*temporary);
    path = directory + "/fixture.bin";
    return true;
  }

  bool prepare(std::size_t length) {
    if (!prepare_directory())
      return false;
    bytes.resize(length);
    // Valid UTF-8 ASCII, including embedded NULs; identical for both readers.
    for (std::size_t i = 0; i < length; ++i)
      bytes[i] = static_cast<char>((i * 37 + i / 127) % 128);
    auto written = File::write_file(std::span<char const>(bytes), path);
    if (!written) {
      state.SkipWithError(written.error().what());
      return false;
    }
    return validate_bytes();// Reads every byte, explicitly warming page cache.
  }

  bool validate_bytes() {
    auto read = File::read_file(path);
    if (!read) {
      state.SkipWithError(read.error().what());
      return false;
    }
    return require(state, std::ranges::equal(read->span(), bytes), "File bytes differ from deterministic dataset");
  }

  bool validate_string() {
    auto read = File::read_file_string(path);
    if (!read) {
      state.SkipWithError(read.error().what());
      return false;
    }
    return require(state, std::ranges::equal(read->utf8(), bytes), "File String differs from deterministic dataset");
  }

  bool open_at_cursor() {
    auto opened = File::open(path, Star::IOMode::Read);
    if (!opened) {
      state.SkipWithError(opened.error().what());
      return false;
    }
    handle = std::move(*opened);
    auto sought = handle->seek(137);
    if (!sought) {
      state.SkipWithError(sought.error().what());
      return false;
    }
    return validate_cursor();
  }

  bool validate_cursor() {
    auto position = handle->pos();
    if (!position) {
      state.SkipWithError(position.error().what());
      return false;
    }
    return require(state, *position == 137, "Metadata or positioned read changed the sequential cursor");
  }
};

bool check_lock(benchmark::State& state, Star::LockFile& lock, std::int64_t timeout_ms, bool expected) {
  auto acquired = lock.lock(timeout_ms);
  if (!acquired) {
    state.SkipWithError(acquired.error().what());
    return false;
  }
  return require(state, *acquired == expected, expected ? "Free lock was unavailable" : "Contender acquired held lock");
}

bool check_unlock(benchmark::State& state, Star::LockFile& lock) {
  auto released = lock.unlock();
  if (!released)
    state.SkipWithError(released.error().what());
  return released.has_value();
}

void lock_file_lifecycle(benchmark::State& state) {
  FileFixture fixture(state);
  if (!fixture.prepare_directory())
    return;
  for (auto _ : state) {
    (void)_;
    Star::LockFile lock(fixture.path);
    if (!check_lock(state, lock, 0, true) || !check_unlock(state, lock))
      break;
  }
  state.SetItemsProcessed(state.iterations());
}

void lock_file_contention(benchmark::State& state) {
  FileFixture fixture(state);
  if (!fixture.prepare_directory())
    return;
  // No release/reacquire in the timed loop: all attempts encounter the same owner.
  Star::LockFile owner(fixture.path);
  if (!check_lock(state, owner, 0, true))
    return;
  {
    Star::LockFile contender(fixture.path);
    auto timeout_ms = state.range(0);
    for (auto _ : state) {
      (void)_;
      if (!check_lock(state, contender, timeout_ms, false))
        break;
    }
  }
  check_unlock(state, owner);
  state.SetItemsProcessed(state.iterations());
}

BENCHMARK(lock_file_lifecycle)->Name("LockFile/Lifecycle")->UseRealTime();
BENCHMARK(lock_file_contention)->Name("LockFile/Contention")->ArgName("timeout_ms")->Arg(0)->Arg(2)->UseRealTime();

void file_size(benchmark::State& state) {
  FileFixture fixture(state);
  if (!fixture.prepare(1024 * 1024) || !fixture.open_at_cursor())
    return;
  state.SetLabel("warm page-cache fixture; open handle; cursor preserved");
  for (auto _ : state) {
    (void)_;
    auto size = fixture.handle->size();
    if (!size) {
      state.SkipWithError(size.error().what());
      break;
    }
    if (!require(state, *size == static_cast<Star::StreamOffset>(fixture.bytes.size()), "File size is incorrect"))
      break;
    benchmark::DoNotOptimize(*size);
  }
  fixture.validate_cursor();
  state.SetItemsProcessed(state.iterations());
}

void file_at_end(benchmark::State& state) {
  FileFixture fixture(state);
  auto length = static_cast<std::size_t>(state.range(0));
  if (!fixture.prepare(length) || !fixture.open_at_cursor())
    return;
  state.SetLabel("open handle; fixed cursor at 137");
  for (auto _ : state) {
    (void)_;
    auto ended = fixture.handle->at_end();
    if (!ended) {
      state.SkipWithError(ended.error().what());
      break;
    }
    if (!require(state, *ended == (length == 137), "Unexpected EOF result"))
      break;
    benchmark::DoNotOptimize(*ended);
  }
  fixture.validate_cursor();
  state.SetItemsProcessed(state.iterations());
}

void file_backup_rotation(benchmark::State& state) {
  FileFixture fixture(state);
  if (!fixture.prepare(1))
    return;
  bool sparse = state.range(0) == 0;
  std::array<String, 4> paths{fixture.path, fixture.path + ".1", fixture.path + ".2", fixture.path + ".3"};
  std::array<char, 4> markers{0, 1, 2, 3};
  state.SetLabel(sparse ? "missing first backup; fixture reset and checks untimed"
                        : "populated backups; fixture reset and checks untimed");
  for (auto _ : state) {
    (void)_;
    state.PauseTiming();
    for (std::size_t i = sparse ? 2 : 1; i < paths.size(); ++i) {
      auto written = File::write_file(std::span<char const>(markers).subspan(i, 1), paths[i]);
      if (!written) {
        state.SkipWithError(written.error().what());
        return;
      }
    }
    state.ResumeTiming();
    auto rotated = File::backup_file_in_sequence(fixture.path, 3);
    if (!rotated) {
      state.SkipWithError(rotated.error().what());
      return;
    }
    state.PauseTiming();
    for (std::size_t i = 0; i < paths.size(); ++i) {
      auto read = File::read_file(paths[i]);
      if (!read) {
        state.SkipWithError(read.error().what());
        return;
      }
      auto expected = i == 0 || (i == 2 && sparse) ? i : i - 1;
      if (!require(state, read->size() == 1 && read->span().front() == markers[expected], "Incorrect backup contents"))
        return;
    }
    if (sparse) {
      auto removed = File::remove(paths[1]);
      if (!removed) {
        state.SkipWithError(removed.error().what());
        return;
      }
    }
    state.ResumeTiming();
  }
  state.SetItemsProcessed(state.iterations());
}

BENCHMARK(file_at_end)->Name("File/AtEnd")->Arg(137)->Arg(138)->UseRealTime();
BENCHMARK(file_backup_rotation)->Name("File/BackupRotation")->Arg(0)->Arg(1)->UseRealTime();

void file_read_bytes(benchmark::State& state) {
  FileFixture fixture(state);
  auto length = static_cast<std::size_t>(state.range(0));
  if (!fixture.prepare(length))
    return;
  state.SetLabel("warm page-cache read; open/read/close and buffer destruction");
  for (auto _ : state) {
    (void)_;
    auto read = File::read_file(fixture.path);
    if (!read) {
      state.SkipWithError(read.error().what());
      break;
    }
    if (!require(state, read->size() == length, "read_file returned incorrect length"))
      break;
    benchmark::DoNotOptimize(read->data());
    benchmark::DoNotOptimize(read->size());
    benchmark::ClobberMemory();
  }
  fixture.validate_bytes();
  state.SetItemsProcessed(state.iterations());
  state.SetBytesProcessed(state.iterations() * static_cast<std::int64_t>(length));
}

#ifdef __linux__
void file_read_bytes_evict_advised(benchmark::State& state) {
  FileFixture fixture(state);
  auto length = static_cast<std::size_t>(state.range(0));
  if (!fixture.prepare(length))
    return;
  int descriptor = ::open(fixture.path.utf8().c_str(), O_RDONLY | O_CLOEXEC);
  if (!require(state, descriptor >= 0, "Opening private file for eviction advice failed"))
    return;
  // Clean pages can be discarded by DONTNEED; advice is not a guarantee
  // about page residency or physical storage caches.
  if (::fsync(descriptor) != 0) {
    state.SkipWithError("Syncing private input before eviction advice failed");
    if (::close(descriptor) != 0)
      state.SkipWithError("Closing private eviction-advice descriptor failed");
    return;
  }
  state.SetLabel(
    "evict-advised kernel-page-cache read; NOT guaranteed physical cold disk; per-iteration advice/reset untimed");
  for (auto _ : state) {
    (void)_;
    state.PauseTiming();
    int advised = ::posix_fadvise(descriptor, 0, 0, POSIX_FADV_DONTNEED);
    state.ResumeTiming();
    if (!require(state, advised == 0, "Private-file POSIX_FADV_DONTNEED failed"))
      break;
    auto read = File::read_file(fixture.path);
    if (!read) {
      state.SkipWithError(read.error().what());
      break;
    }
    if (!require(state, read->size() == length, "Evict-advised read returned incorrect length"))
      break;
    benchmark::DoNotOptimize(read->data());
    benchmark::DoNotOptimize(read->size());
    benchmark::ClobberMemory();
  }
  if (::close(descriptor) != 0)
    state.SkipWithError("Closing private eviction-advice descriptor failed");
  fixture.validate_bytes();
  state.SetItemsProcessed(state.iterations());
  state.SetBytesProcessed(state.iterations() * static_cast<std::int64_t>(length));
}

BENCHMARK(file_read_bytes_evict_advised)
  ->Name("File/ReadBytesEvictAdvised")
  ->Arg(1048576)
  ->Arg(16777216)
  ->Arg(67108864)
  ->UseRealTime();
#endif

void file_read_string(benchmark::State& state) {
  FileFixture fixture(state);
  auto length = static_cast<std::size_t>(state.range(0));
  if (!fixture.prepare(length) || !fixture.validate_string())
    return;
  state.SetLabel("warm page-cache read; open/read/close and String destruction");
  for (auto _ : state) {
    (void)_;
    auto read = File::read_file_string(fixture.path);
    if (!read) {
      state.SkipWithError(read.error().what());
      break;
    }
    if (!require(state, read->utf8().size() == length, "read_file_string returned incorrect byte length"))
      break;
    benchmark::DoNotOptimize(read->utf8().data());
    benchmark::DoNotOptimize(read->utf8().size());
    benchmark::ClobberMemory();
  }
  fixture.validate_string();
  state.SetItemsProcessed(state.iterations());
  state.SetBytesProcessed(state.iterations() * static_cast<std::int64_t>(length));
}

void file_positioned_read(benchmark::State& state) {
  FileFixture fixture(state);
  if (!fixture.prepare(1024 * 1024) || !fixture.open_at_cursor())
    return;
  std::array<char, 4096> buffer;
  constexpr Star::StreamOffset offset = 8193;
  auto validate_read = [&] {
    auto read = fixture.handle->read_absolute(offset, buffer);
    if (!read) {
      state.SkipWithError(read.error().what());
      return false;
    }
    return require(state,
                   *read == buffer.size()
                     && std::ranges::equal(buffer, std::span<char const>(fixture.bytes).subspan(offset, buffer.size())),
                   "Positioned read returned incorrect bytes or length")
      && fixture.validate_cursor();
  };
  if (!validate_read())
    return;
  state.SetLabel("warm page-cache 4KiB positioned read; open handle; cursor preserved");
  for (auto _ : state) {
    (void)_;
    auto read = fixture.handle->read_absolute(offset, buffer);
    if (!read) {
      state.SkipWithError(read.error().what());
      break;
    }
    if (!require(state, *read == buffer.size(), "Positioned read returned incorrect length"))
      break;
    benchmark::DoNotOptimize(buffer.data());
    benchmark::ClobberMemory();
  }
  require(state, std::ranges::equal(buffer, std::span<char const>(fixture.bytes).subspan(offset, buffer.size())),
          "Timed positioned read returned incorrect bytes");
  fixture.validate_cursor();
  state.SetItemsProcessed(state.iterations());
  state.SetBytesProcessed(state.iterations() * static_cast<std::int64_t>(buffer.size()));
}

void file_overwrite_rename(benchmark::State& state) {
  FileFixture fixture(state);
  auto length = static_cast<std::size_t>(state.range(0));
  if (!fixture.prepare(length))
    return;
  // A different deterministic payload proves the replacement reached the path.
  for (auto& byte : fixture.bytes)
    byte = static_cast<char>(byte ^ 0x55);
  state.SetLabel("durable replacement; staging write/flush/rename and platform directory durability");
  std::array<std::int64_t, 4096> latencies;
  std::size_t sampled = 0;
  for (auto _ : state) {
    (void)_;
    bool sample = sampled < latencies.size();
    auto started = sample ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
    auto written = File::overwrite_file_with_rename(std::span<char const>(fixture.bytes), fixture.path);
    if (sample)
      latencies[sampled++] =
        std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - started).count();
    if (!written) {
      state.SkipWithError(written.error().what());
      break;
    }
    benchmark::ClobberMemory();
  }
  fixture.validate_bytes();
  auto staging = File::exists(fixture.path + ".new");
  if (!staging)
    state.SkipWithError(staging.error().what());
  else
    require(state, !*staging, "Successful replacement left a staging file");
  if (sampled) {
    std::sort(latencies.begin(), latencies.begin() + sampled);
    state.counters["write_sample_count"] = static_cast<double>(sampled);
    state.counters["write_p50_ns"] = static_cast<double>(latencies[(sampled * 50 + 99) / 100 - 1]);
    state.counters["write_p95_ns"] = static_cast<double>(latencies[(sampled * 95 + 99) / 100 - 1]);
    state.counters["write_p99_ns"] = static_cast<double>(latencies[(sampled * 99 + 99) / 100 - 1]);
  }
  state.SetItemsProcessed(state.iterations());
  state.SetBytesProcessed(state.iterations() * static_cast<std::int64_t>(length));
}

void whole_read_arguments(benchmark::internal::Benchmark* benchmark) {
  // Preserve the original inputs, add just-over/under growth boundaries and
  // files well beyond the original 16MiB workload. Both readers use the exact
  // same deterministic fixture and open/read/close/destruction operation.
  for (std::int64_t size : {0ll, 4096ll, 1048575ll, 1048576ll, 1048577ll, 16777215ll, 16777216ll, 16777217ll,
                            67108863ll, 67108864ll, 67108865ll, 268435456ll})
    benchmark->Arg(size);
}

BENCHMARK(file_size)->Name("File/Size")->UseRealTime();
BENCHMARK(file_read_bytes)->Name("File/ReadBytes")->Apply(whole_read_arguments)->UseRealTime();
BENCHMARK(file_read_string)->Name("File/ReadString")->Apply(whole_read_arguments)->UseRealTime();
BENCHMARK(file_positioned_read)->Name("File/PositionedRead")->UseRealTime();
BENCHMARK(file_overwrite_rename)->Name("File/OverwriteRename")->Arg(4096)->Arg(1048576)->UseRealTime();

}// namespace

int main(int argc, char** argv) {
  return Star::run_benchmarks(argc, argv);
}

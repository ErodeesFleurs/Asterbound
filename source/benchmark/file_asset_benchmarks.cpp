#include <benchmark/benchmark.h>

import std;
import star.algorithm;
import star.asset_source;
import star.btree_database;
import star.byte_array;
import star.directory_asset_source;
import star.file;
import star.format;
import star.io_device;
import star.logging;
import star.memory_asset_source;
import star.packed_asset_source;
import star.random;
import star.string;
#include "asset_provider_fixture.hpp"

namespace {

std::expected<Star::StreamOffset, Star::StarException> asset_length(Star::AssetSource& source,
                                                                    Star::String const& path) {
  return source.file_size(path);
}

using BenchmarkFixtures::AssetKind;
using BenchmarkFixtures::deterministic_bytes;

void asset_lengths(benchmark::State& state, AssetKind kind) {
  auto count = static_cast<std::size_t>(state.range(0));
  BenchmarkFixtures::with_asset_provider(
    state, kind, count, "file-benchmark",
    "Asset provider indexed an incorrect number of deterministic files",
    [&](auto const& source, auto const& paths, auto const& payloads) {
      std::vector<Star::StreamOffset> lengths(count);
      Star::StreamOffset expected_total = 0;
      for (auto const& payload : payloads)
        expected_total += static_cast<Star::StreamOffset>(payload.size());
      // Confirm consumer-visible bytes and metadata before sampling. Neither file
      // creation, packing, scanning nor these reads are part of the length query.
      Star::StreamOffset verified_total = 0;
      for (std::size_t i = 0; i != count; ++i) {
        auto bytes = source->read(paths[i]);
        auto length = asset_length(*source, paths[i]);
        if (!bytes || !length) {
          state.SkipWithError(!bytes ? bytes.error().what() : length.error().what());
          return;
        }
        if (*bytes != payloads[i] || *length != static_cast<Star::StreamOffset>(payloads[i].size())) {
          state.SkipWithError("Asset provider returned incorrect bytes or metadata length");
          return;
        }
        verified_total += *length;
      }
      if (verified_total != expected_total) {
        state.SkipWithError("Asset provider returned an incorrect total length");
        return;
      }
      for (auto _ : state) {
        (void)_;
        bool valid = true;
        for (std::size_t i = 0; i != count; ++i) {
          auto length = asset_length(*source, paths[i]);
          if (!length) {
            state.SkipWithError(length.error().what());
            valid = false;
            break;
          }
          lengths[i] = *length;
        }
        if (!valid)
          break;
        benchmark::ClobberMemory();
        state.PauseTiming();
        Star::StreamOffset total = 0;
        char const* validation_error = nullptr;
        for (std::size_t i = 0; i != count; ++i) {
          if (lengths[i] != static_cast<Star::StreamOffset>(payloads[i].size())) {
            validation_error = "Timed asset length query returned an incorrect length";
            break;
          }
          total += lengths[i];
        }
        if (!validation_error && total != expected_total)
          validation_error = "Timed asset length batch returned an incorrect total";
        state.ResumeTiming();
        if (validation_error) {
          state.SkipWithError(validation_error);
          break;
        }
      }
      state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(count));
      state.SetLabel("warm deterministic size queries; scan/pack/readback untimed; not whole Assets startup");
    });
}

constexpr std::size_t btree_key_count = 64;
constexpr std::size_t btree_seed_count = btree_key_count / 2;
constexpr std::size_t btree_block_size = 4096;
constexpr std::size_t btree_seed_value_size = 12288;

struct BTreeFixture {
  Star::Logger logger;
  Star::String path;
  std::shared_ptr<Star::File> device;
  std::unique_ptr<Star::BTreeDatabase> database;
  Star::ByteArray seed_file;
  std::array<Star::ByteArray, btree_key_count> keys;
  std::array<Star::ByteArray, btree_key_count> seed_values;
  std::array<Star::ByteArray, btree_key_count> inserted_values;
  std::array<Star::ByteArray, btree_key_count> overwritten_values;
  std::array<bool, btree_key_count * 2> overwrite_results{};

  std::expected<void, Star::StarException> open_database() {
    device = std::make_shared<Star::File>(path);
    database = std::make_unique<Star::BTreeDatabase>(logger, "FileBenchmark", 4);
    if (auto result = database->set_auto_commit(false); !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = database->set_block_size(btree_block_size); !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = database->set_io_device(device); !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = database->open(); !result)
      return std::unexpected(std::move(result.error()));
    return {};
  }

  std::expected<void, Star::StarException> close_database() {
    if (database && database->is_open()) {
      if (auto result = database->close(true); !result)
        return std::unexpected(std::move(result.error()));
    } else if (device && device->is_open()) {
      if (auto result = device->close(); !result)
        return std::unexpected(std::move(result.error()));
    }
    database.reset();
    device.reset();
    return {};
  }

  std::expected<void, Star::StarException> initialize(Star::String file_path, std::size_t value_size) {
    path = std::move(file_path);
    for (std::size_t i = 0; i != btree_key_count; ++i) {
      keys[i] = Star::ByteArray(4, '\0');
      for (unsigned byte = 0; byte != 4; ++byte)
        keys[i][byte] = static_cast<char>((i >> ((3 - byte) * 8)) & 255);
      seed_values[i] = deterministic_bytes(btree_seed_value_size, i, 0);
      inserted_values[i] = deterministic_bytes(value_size, i, 1);
      overwritten_values[i] = deterministic_bytes(value_size, i, 2);
    }
    if (auto result = open_database(); !result)
      return result;
    for (std::size_t i = 0; i != btree_seed_count; ++i) {
      auto inserted = database->insert(keys[i], seed_values[i]);
      if (!inserted)
        return std::unexpected(std::move(inserted.error()));
      if (*inserted)
        return std::unexpected(Star::StarException("Fresh BTree fixture unexpectedly overwrote a key"));
    }
    if (auto result = database->commit(); !result)
      return result;
    if (auto result = verify(false); !result)
      return result;
    if (auto result = close_database(); !result)
      return result;
    auto bytes = Star::File::read_file(path);
    if (!bytes)
      return std::unexpected(std::move(bytes.error()));
    seed_file = std::move(*bytes);
    return reset();
  }

  std::expected<void, Star::StarException> reset() {
    if (auto result = close_database(); !result)
      return result;
    if (auto result = Star::File::write_file(seed_file, path); !result)
      return result;
    if (auto result = open_database(); !result)
      return result;
    // Flush restoration outside timing: the measured commit must not also
    // absorb dirty pages from writing the seed snapshot.
    return device->sync();
  }

  std::expected<void, Star::StarException> reopen() {
    if (auto result = close_database(); !result)
      return result;
    return open_database();
  }

  std::expected<void, Star::StarException> stage_batch() {
    for (std::size_t i = 0; i != btree_key_count; ++i) {
      auto result = database->insert(keys[i], inserted_values[i]);
      if (!result)
        return std::unexpected(std::move(result.error()));
      overwrite_results[i] = *result;
    }
    for (std::size_t i = 0; i != btree_key_count; ++i) {
      auto result = database->insert(keys[i], overwritten_values[i]);
      if (!result)
        return std::unexpected(std::move(result.error()));
      overwrite_results[btree_key_count + i] = *result;
    }
    return {};
  }

  std::expected<void, Star::StarException> verify_overwrite_results() const {
    for (std::size_t i = 0; i != btree_key_count; ++i) {
      if (overwrite_results[i] != (i < btree_seed_count) || !overwrite_results[btree_key_count + i])
        return std::unexpected(Star::StarException("BTree insert/overwrite status did not match the fixed dataset"));
    }
    return {};
  }

  std::expected<void, Star::StarException> verify(bool staged) {
    auto count = database->record_count();
    if (!count)
      return std::unexpected(std::move(count.error()));
    if (*count != (staged ? btree_key_count : btree_seed_count))
      return std::unexpected(Star::StarException("BTree record count did not match the fixed dataset"));
    for (std::size_t i = 0; i != btree_key_count; ++i) {
      auto found = database->find(keys[i]);
      if (!found)
        return std::unexpected(std::move(found.error()));
      bool expected_present = staged || i < btree_seed_count;
      if (found->has_value() != expected_present)
        return std::unexpected(Star::StarException("BTree key presence did not survive stage/rollback/commit/reopen"));
      if (expected_present && **found != (staged ? overwritten_values[i] : seed_values[i]))
        return std::unexpected(Star::StarException("BTree value bytes did not survive stage/rollback/commit/reopen"));
    }
    return {};
  }

  std::expected<void, Star::StarException> preflight() {
    if (auto result = stage_batch(); !result)
      return result;
    if (auto result = verify_overwrite_results(); !result)
      return result;
    if (auto result = verify(true); !result)
      return result;
    if (auto result = database->rollback(); !result)
      return result;
    if (auto result = verify(false); !result)
      return result;
    if (auto result = reopen(); !result)
      return result;
    if (auto result = verify(false); !result)
      return result;
    if (auto result = stage_batch(); !result)
      return result;
    if (auto result = verify_overwrite_results(); !result)
      return result;
    if (auto result = database->commit(); !result)
      return result;
    if (auto result = verify(true); !result)
      return result;
    if (auto result = reopen(); !result)
      return result;
    if (auto result = verify(true); !result)
      return result;
    return reset();
  }
};

enum class BTreeOperation { commit, rollback, concurrent_lookup };

void concurrent_lookup(benchmark::State& state, BTreeFixture& fixture) {
  constexpr unsigned sweeps = 8;
  auto lookup = [&](std::size_t offset) {
    for (unsigned pass = 0; pass < sweeps; ++pass) {
      for (std::size_t i = 0; i < btree_key_count; ++i) {
        auto index = (i + offset) % btree_key_count;
        auto found = fixture.database->contains(fixture.keys[index]);
        if (!found || *found != (index < btree_seed_count))
          return false;
      }
    }
    return true;
  };
  if (!lookup(0)) {
    state.SkipWithError("Incorrect B-tree lookup during cache warmup");
    return;
  }
  std::barrier rendezvous(2);
  bool stop = false;
  bool worker_ok = true;
  std::jthread reader([&] {
    for (;;) {
      rendezvous.arrive_and_wait();
      if (stop)
        return;
      worker_ok = lookup(btree_seed_count);
      rendezvous.arrive_and_wait();
    }
  });
  for (auto _ : state) {
    rendezvous.arrive_and_wait();
    auto caller_ok = lookup(0);
    rendezvous.arrive_and_wait();
    if (!caller_ok || !worker_ok) {
      state.SkipWithError("Concurrent B-tree lookup returned wrong membership");
      break;
    }
  }
  stop = true;
  rendezvous.arrive_and_wait();
  state.SetItemsProcessed(state.iterations() * 2 * sweeps * btree_key_count);
}

void btree_transactions(benchmark::State& state, BTreeOperation operation) {
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
  auto cleanup = Star::make_scope_exit([&] {
    auto removed = Star::File::remove_all(*temporary);
    if (!removed)
      state.SkipWithError(removed.error().what());
  });
  BTreeFixture fixture;
  auto initialized =
    fixture.initialize(Star::File::relative_to(*temporary, "database.db"), static_cast<std::size_t>(state.range(0)));
  if (!initialized) {
    state.SkipWithError(initialized.error().what());
    return;
  }
  if (operation == BTreeOperation::concurrent_lookup) {
    concurrent_lookup(state, fixture);
    if (auto closed = fixture.close_database(); !closed)
      state.SkipWithError(closed.error().what());
    return;
  }
  bool commit = operation == BTreeOperation::commit;
  if (auto result = fixture.preflight(); !result) {
    state.SkipWithError(result.error().what());
    return;
  }
  for (auto _ : state) {
    (void)_;
    state.PauseTiming();
    // Restore the exact committed file, reopen with empty caches and no staged
    // buffers. Every batch reaches first-unstaged full-block replacement, not
    // just subsequent writes to an already materialized transaction overlay.
    auto result = fixture.reset();
    state.ResumeTiming();
    if (!result) {
      state.SkipWithError(result.error().what());
      break;
    }
    result = fixture.stage_batch();
    state.PauseTiming();
    if (result)
      result = fixture.verify_overwrite_results();
    if (result)
      result = fixture.verify(true);
    state.ResumeTiming();
    if (!result) {
      state.SkipWithError(result.error().what());
      break;
    }
    result = commit ? fixture.database->commit() : fixture.database->rollback();
    state.PauseTiming();
    if (result)
      result = fixture.verify(commit);
    if (result)
      result = fixture.reopen();
    if (result)
      result = fixture.verify(commit);
    state.ResumeTiming();
    if (!result) {
      state.SkipWithError(result.error().what());
      break;
    }
  }
  if (auto result = fixture.close_database(); !result)
    state.SkipWithError(result.error().what());
  state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(btree_key_count * 2));
  state.SetLabel(commit ? "128 mutations: 32 insert + 96 overwrite; real commit/sync timed; 4096-byte blocks; snapshot "
                          "reset/readback/reopen untimed"
                        : "128 mutations: 32 insert + 96 overwrite; rollback/root-read/resize timed, no fsync; "
                          "4096-byte blocks; snapshot reset/readback/reopen untimed");
}

void asset_lengths_directory(benchmark::State& state) { asset_lengths(state, AssetKind::directory); }
void asset_lengths_packed(benchmark::State& state) { asset_lengths(state, AssetKind::packed); }
void asset_lengths_memory(benchmark::State& state) { asset_lengths(state, AssetKind::memory); }
void btree_stage_rollback(benchmark::State& state) { btree_transactions(state, BTreeOperation::rollback); }
void btree_commit(benchmark::State& state) { btree_transactions(state, BTreeOperation::commit); }
void btree_cache_readers(benchmark::State& state) { btree_transactions(state, BTreeOperation::concurrent_lookup); }

BENCHMARK(asset_lengths_directory)->Name("File/AssetLengths/Directory")->Arg(256)->Arg(4096)->UseRealTime();
BENCHMARK(asset_lengths_packed)->Name("File/AssetLengths/Packed")->Arg(256)->Arg(4096)->UseRealTime();
BENCHMARK(asset_lengths_memory)->Name("File/AssetLengths/Memory")->Arg(256)->Arg(4096)->UseRealTime();
BENCHMARK(btree_stage_rollback)->Name("File/BTree/StageRollback")->Arg(64)->Arg(12288)->UseRealTime();
BENCHMARK(btree_commit)->Name("File/BTree/Commit")->Arg(64)->Arg(12288)->UseRealTime();
BENCHMARK(btree_cache_readers)->Name("File/BTree/ConcurrentLookup")->Arg(64)->UseRealTime();

}// namespace

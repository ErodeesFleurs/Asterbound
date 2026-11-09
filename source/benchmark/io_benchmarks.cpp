#include <benchmark/benchmark.h>
#include <cstdio>

import std;
import star.algorithm;
import star.asset_source;
import star.buffer;
import star.byte_array;
import star.data_stream;
import star.data_stream_devices;
import star.data_stream_extra;
import star.directory_asset_source;
import star.file;
import star.format;
import star.io_device;
import star.logging;
import star.native_runtime;
import star.packed_asset_source;
import star.random;
import star.string;
import star.uuid;

namespace {
constexpr std::size_t batch_size = 64;

std::uint64_t fingerprint(std::span<char const> bytes) {
  std::uint64_t result = 14695981039346656037ull;
  for (unsigned char byte : bytes)
    result = (result ^ byte) * 1099511628211ull;
  return result;
}

Star::ByteArray fixture(std::size_t size) {
  Star::ByteArray bytes;
  bytes.resize(size);
  for (std::size_t i = 0; i != size; ++i)
    bytes[i] = static_cast<char>((i * 37 + i / 127) % 251);
  return bytes;
}

bool require(benchmark::State& state, bool valid) {
  if (!valid)
    state.SkipWithError("Complete result or cursor differs from deterministic fixture");
  return valid;
}

void account(benchmark::State& state, std::size_t count, std::size_t bytes, std::uint64_t hash,
             std::string_view detail) {
  state.SetItemsProcessed(state.iterations() * count);
  state.SetBytesProcessed(state.iterations() * count * bytes);
  state.SetLabel(std::format("fixture-fnv1a={:016x}; batch={}; {}; setup/validation untimed", hash, count, detail));
}

void deserialize_string(benchmark::State& state) {
  auto size = static_cast<std::size_t>(state.range(0));
  std::string text(size, ' ');
  for (std::size_t i = 0; i != size; ++i)
    text[i] = static_cast<char>('!' + (i * 17) % 90);
  auto encoded = Star::DataStreamBuffer::serialize(text);
  if (!encoded) {
    state.SkipWithError(encoded.error().what());
    return;
  }
  std::array<std::string, batch_size> decoded;
  for (auto _ : state) {
    (void)_;
    bool valid = true;
    for (auto& value : decoded) {
      auto read = Star::DataStreamBuffer::deserialize(value, encoded->span());
      if (!read) {
        state.SkipWithError(read.error().what());
        valid = false;
        break;
      }
      benchmark::DoNotOptimize(value.data());
    }
    if (!valid)
      break;
    benchmark::ClobberMemory();
    state.PauseTiming();
    valid = std::ranges::all_of(decoded, [&](std::string const& value) { return value == text; });
    state.ResumeTiming();
    if (!require(state, valid))
      break;
  }
  account(state, batch_size, size, fingerprint(encoded->span()), "lvalue encoded input; owning decoded strings");
}

enum class stream_kind { external, owning, functions };

void vlq_read(benchmark::State& state, stream_kind kind) {
  auto width = static_cast<unsigned>(state.range(0));
  std::array<std::uint64_t, batch_size> expected;
  Star::DataStreamBuffer encoder;
  for (std::size_t i = 0; i != batch_size; ++i) {
    expected[i] = (std::uint64_t{1} << ((width - 1) * 7)) + i;
    auto written = encoder.write_vlq_u(expected[i]);
    if (!written) {
      state.SkipWithError(written.error().what());
      return;
    }
    if (!require(state, *written == width))
      return;
  }
  auto bytes = encoder.take_data();
  Star::DataStreamReader external(bytes.span());
  Star::DataStreamBuffer owning(bytes);
  std::size_t cursor = 0;
  Star::DataStreamFunctions functions(
    [&](std::span<char> output) -> std::expected<std::size_t, Star::StarException> {
      auto count = std::min(output.size(), bytes.size() - cursor);
      std::ranges::copy(bytes.span().subspan(cursor, count), output.begin());
      cursor += count;
      return count;
    },
    {});
  Star::DataStream& stream = kind == stream_kind::external ? static_cast<Star::DataStream&>(external)
    : kind == stream_kind::owning                          ? static_cast<Star::DataStream&>(owning)
                                                           : static_cast<Star::DataStream&>(functions);
  std::array<std::uint64_t, batch_size> output{};
  for (auto _ : state) {
    (void)_;
    auto moved = kind == stream_kind::owning ? owning.seek(0) : external.seek(0);
    if (!moved) {
      state.SkipWithError(moved.error().what());
      break;
    }
    cursor = 0;
    bool valid = true;
    for (auto& value : output) {
      auto read = stream.read_vlq_u(value);
      if (!read) {
        state.SkipWithError(read.error().what());
        valid = false;
        break;
      }
    }
    benchmark::DoNotOptimize(output.data());
    benchmark::ClobberMemory();
    if (!valid)
      break;
    state.PauseTiming();
    auto position = kind == stream_kind::external ? external.pos() : owning.pos();
    valid = output == expected
      && (kind == stream_kind::functions ? cursor == bytes.size() : position && *position == bytes.size());
    state.ResumeTiming();
    if (!require(state, valid))
      break;
  }
  account(state, batch_size, width, fingerprint(bytes.span()), "DataStream& dispatch; includes one rewind per batch");
}

void uuid_read(benchmark::State& state) {
  auto bytes = fixture(batch_size * Star::UuidSize);
  Star::DataStreamReader stream(bytes.span());
  std::vector<Star::Uuid> output(batch_size, Star::Uuid::nil());
  for (auto _ : state) {
    (void)_;
    auto moved = stream.seek(0);
    if (!moved) {
      state.SkipWithError(moved.error().what());
      break;
    }
    bool valid = true;
    for (auto& value : output) {
      auto read = stream >> value;
      if (!read) {
        state.SkipWithError(read.error().what());
        valid = false;
        break;
      }
    }
    benchmark::DoNotOptimize(output.data());
    benchmark::ClobberMemory();
    if (!valid)
      break;
    state.PauseTiming();
    for (std::size_t i = 0; i != batch_size; ++i)
      valid &= std::ranges::equal(output[i].span(), bytes.span().subspan(i * Star::UuidSize, Star::UuidSize));
    auto position = stream.pos();
    valid &= position && *position == bytes.size();
    state.ResumeTiming();
    if (!require(state, valid))
      break;
  }
  account(state, batch_size, Star::UuidSize, fingerprint(bytes.span()), "owning fixed UUID results");
}

enum class file_operation { write, directory, packed };

void file_io(benchmark::State& state, file_operation operation) {
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
  auto bytes = fixture(static_cast<std::size_t>(state.range(0)));
  auto path = Star::File::relative_to(*temporary, "payload.bin");
  auto written = Star::File::write_file(bytes, path);
  if (!written) {
    state.SkipWithError(written.error().what());
    return;
  }
  Star::Logger logger;
  std::shared_ptr<Star::AssetSource> source;
  if (operation != file_operation::write) {
    auto directory = Star::DirectoryAssetSource::create(logger, *temporary);
    if (!directory) {
      state.SkipWithError(directory.error().what());
      return;
    }
    source = *directory;
    if (operation == file_operation::packed) {
      auto packed_path = Star::File::relative_to(*temporary, "fixture.pak");
      auto built = Star::PackedAssetSource::build(**directory, packed_path);
      if (!built) {
        state.SkipWithError(built.error().what());
        return;
      }
      auto packed = Star::PackedAssetSource::create(packed_path);
      if (!packed) {
        state.SkipWithError(packed.error().what());
        return;
      }
      source = std::move(*packed);
    }
  }
  Star::ByteArray output;
  for (auto _ : state) {
    (void)_;
    if (operation == file_operation::write) {
      auto result = Star::File::write_file(bytes, path);
      if (!result) {
        state.SkipWithError(result.error().what());
        break;
      }
    } else {
      auto result = source->read("/payload.bin");
      if (!result) {
        state.SkipWithError(result.error().what());
        break;
      }
      output = std::move(*result);
      benchmark::DoNotOptimize(output.data());
    }
    benchmark::ClobberMemory();
    state.PauseTiming();
    bool valid;
    if (operation == file_operation::write) {
      auto actual = Star::File::read_file(path);
      valid = actual && *actual == bytes;
    } else {
      valid = output == bytes;
    }
    state.ResumeTiming();
    if (!require(state, valid))
      break;
  }
  account(state, 1, bytes.size(), fingerprint(bytes.span()),
          operation == file_operation::write ? "warm open/truncate/write/close; no durability sync"
                                             : "warm complete asset payload read; not metadata query");
}

void external_vlq(benchmark::State& state) { vlq_read(state, stream_kind::external); }
void owning_vlq(benchmark::State& state) { vlq_read(state, stream_kind::owning); }
void functions_vlq(benchmark::State& state) { vlq_read(state, stream_kind::functions); }
void write_file(benchmark::State& state) { file_io(state, file_operation::write); }
void directory_read(benchmark::State& state) { file_io(state, file_operation::directory); }
void packed_read(benchmark::State& state) { file_io(state, file_operation::packed); }

BENCHMARK(deserialize_string)->Name("IO/DeserializeString")->Arg(64)->Arg(4096)->Arg(65536);
BENCHMARK(external_vlq)->Name("IO/VlqExternal")->Arg(1)->Arg(5)->Arg(10);
BENCHMARK(owning_vlq)->Name("IO/VlqOwning")->Arg(1)->Arg(5)->Arg(10);
BENCHMARK(functions_vlq)->Name("IO/VlqFunctions")->Arg(1)->Arg(5)->Arg(10);
BENCHMARK(uuid_read)->Name("IO/UuidRead");
BENCHMARK(write_file)->Name("IO/WriteFile")->Arg(0)->Arg(64)->Arg(4096);
BENCHMARK(directory_read)->Name("IO/DirectoryRead")->Arg(0)->Arg(64)->Arg(4096)->Arg(65536);
BENCHMARK(packed_read)->Name("IO/PackedRead")->Arg(0)->Arg(64)->Arg(4096)->Arg(65536);

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
  benchmark::RunSpecifiedBenchmarks(&reporter);
  benchmark::Shutdown();
  return reporter.failed ? 1 : 0;
}

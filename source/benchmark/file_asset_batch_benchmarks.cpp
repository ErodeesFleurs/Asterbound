#include <benchmark/benchmark.h>

import std;
import star.algorithm;
import star.asset_source;
import star.byte_array;
import star.data_stream_devices;
import star.directory_asset_source;
import star.file;
import star.format;
import star.io_device;
import star.logging;
import star.memory_asset_source;
import star.packed_asset_source;
import star.random;
import star.sha256;
import star.string;

namespace {

using SizeResults = std::vector<std::expected<Star::StreamOffset, Star::StarException>>;
using SizeRequests = std::span<std::reference_wrapper<Star::String const> const>;

// Kept separate so a historical-source projection can replace only this call
// with the equivalent scalar loop, without changing the measured workload.
SizeResults batch_size_query(Star::AssetSource& source, SizeRequests paths) { return source.file_sizes(paths); }

SizeResults scalar_size_query(Star::AssetSource& source, SizeRequests paths) {
  SizeResults sizes;
  sizes.reserve(paths.size());
  for (auto path : paths)
    sizes.push_back(source.file_size(path.get()));
  return sizes;
}

std::expected<Star::ByteArray, Star::StarException> whole_metadata_digest(Star::AssetSource& source, bool batch) {
  auto paths = source.assetPaths();
  for (auto& path : paths) {
    auto lower = path.to_lower();
    if (!lower)
      return std::unexpected(std::move(lower.error()));
    path = std::move(*lower);
  }
  paths.sort();
  SizeResults sizes;
  if (batch) {
    std::vector<std::reference_wrapper<Star::String const>> requests;
    requests.reserve(paths.size());
    for (auto const& path : paths)
      requests.emplace_back(path);
    sizes = batch_size_query(source, requests);
  }
  Star::Sha256Hasher digest;
  for (std::size_t i = 0; i != paths.size(); ++i) {
    digest.push(paths[i]);
    auto size = batch ? std::move(sizes[i]) : source.file_size(paths[i]);
    if (!size)
      return std::unexpected(std::move(size.error()));
    auto bytes = Star::DataStreamBuffer::serialize(*size);
    if (!bytes)
      return std::unexpected(std::move(bytes.error()));
    digest.push(*bytes);
  }
  return digest.compute();
}

enum class AssetKind { directory, packed, memory };

void asset_batch(benchmark::State& state, AssetKind kind, bool batch, bool digest_workload) {
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
  Star::Logger logger;
  auto directory_path = Star::File::relative_to(*temporary, "assets");
  if (auto result = Star::File::create_directory(directory_path); !result) {
    state.SkipWithError(result.error().what());
    return;
  }
  auto count = static_cast<std::size_t>(state.range(0));
  std::vector<Star::String> paths;
  std::vector<Star::ByteArray> payloads;
  paths.reserve(count);
  payloads.reserve(count);
  auto memory = std::make_shared<Star::MemoryAssetSource>("batch-benchmark");
  for (std::size_t i = 0; i != count; ++i) {
    auto name = Star::format("asset-{:05}.bin", i);
    paths.push_back("/" + name);
    Star::ByteArray bytes(1 + (i * 37) % 512, '\0');
    for (std::size_t j = 0; j != bytes.size(); ++j)
      bytes[j] = static_cast<char>((j * 17 + i * 31) % 251);
    payloads.push_back(std::move(bytes));
    if (auto result = Star::File::write_file(payloads.back(), Star::File::relative_to(directory_path, name)); !result) {
      state.SkipWithError(result.error().what());
      return;
    }
    memory->set(paths.back(), payloads.back());
  }
  auto directory = Star::DirectoryAssetSource::create(logger, directory_path);
  if (!directory) {
    state.SkipWithError(directory.error().what());
    return;
  }
  std::shared_ptr<Star::AssetSource> source;
  if (kind == AssetKind::directory) {
    source = *directory;
  } else if (kind == AssetKind::memory) {
    source = memory;
  } else {
    auto packed_path = Star::File::relative_to(*temporary, "assets.pak");
    if (auto result = Star::PackedAssetSource::build(**directory, packed_path); !result) {
      state.SkipWithError(result.error().what());
      return;
    }
    auto packed = Star::PackedAssetSource::create(packed_path);
    if (!packed) {
      state.SkipWithError(packed.error().what());
      return;
    }
    source = std::move(*packed);
  }
  if (source->assetPaths().size() != count) {
    state.SkipWithError("Batch fixture indexed an incorrect number of files");
    return;
  }
  std::vector<std::reference_wrapper<Star::String const>> requests;
  requests.reserve(count);
  Star::Sha256Hasher reader_digest;
  // Paths are already in canonical lowercase sorted order. Validate real reader
  // sizes and payloads, then use their serialized bytes as the digest oracle.
  for (std::size_t i = 0; i != count; ++i) {
    requests.emplace_back(paths[i]);
    auto bytes = source->read(paths[i]);
    auto reader = source->open(paths[i]);
    if (!bytes || !reader) {
      state.SkipWithError(!bytes ? bytes.error().what() : reader.error().what());
      return;
    }
    auto size = (*reader)->size();
    if (!size) {
      state.SkipWithError(size.error().what());
      return;
    }
    if (*bytes != payloads[i] || *size != static_cast<Star::StreamOffset>(payloads[i].size())) {
      state.SkipWithError("Batch fixture returned incorrect reader bytes or size");
      return;
    }
    auto serialized = Star::DataStreamBuffer::serialize(*size);
    if (!serialized) {
      state.SkipWithError(serialized.error().what());
      return;
    }
    reader_digest.push(paths[i]);
    reader_digest.push(*serialized);
  }
  auto expected_digest = reader_digest.compute();
  auto validate_sizes = [&](SizeResults const& sizes) -> char const* {
    if (sizes.size() != count)
      return "Batch query returned an incorrect result count";
    for (std::size_t i = 0; i != count; ++i) {
      if (!sizes[i])
        return sizes[i].error().what();
      if (*sizes[i] != static_cast<Star::StreamOffset>(payloads[i].size()))
        return "Batch query returned an incorrect indexed length";
    }
    return nullptr;
  };
  for (bool use_batch : {false, true}) {
    auto sizes = use_batch ? batch_size_query(*source, requests) : scalar_size_query(*source, requests);
    if (auto error = validate_sizes(sizes)) {
      state.SkipWithError(error);
      return;
    }
    auto digest = whole_metadata_digest(*source, use_batch);
    if (!digest || *digest != expected_digest) {
      state.SkipWithError(!digest ? digest.error().what() : "Metadata digest differs from reader-size oracle");
      return;
    }
  }
  for (auto _ : state) {
    (void)_;
    if (digest_workload) {
      auto digest = whole_metadata_digest(*source, batch);
      benchmark::DoNotOptimize(digest);
      state.PauseTiming();
      bool valid = digest && *digest == expected_digest;
      if (!valid)
        state.SkipWithError(!digest ? digest.error().what() : "Timed metadata digest differs from oracle");
      state.ResumeTiming();
      if (!valid)
        break;
    } else {
      auto sizes = batch ? batch_size_query(*source, requests) : scalar_size_query(*source, requests);
      benchmark::DoNotOptimize(sizes);
      state.PauseTiming();
      auto error = validate_sizes(sizes);
      if (error)
        state.SkipWithError(error);
      state.ResumeTiming();
      if (error)
        break;
    }
  }
  state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(count));
  state.SetLabel(
    digest_workload
      ? "whole single-provider metadata digest: enumerate/lower/sort + queries + DataStream lengths + SHA256; not "
        "Assets startup"
      : "query primitive only; ordered per-entry results; fixture/readback/validation untimed; not whole digest");
}

BENCHMARK_CAPTURE(asset_batch, DirectoryScalarQueries, AssetKind::directory, false, false)->Arg(256)->Arg(4096);
BENCHMARK_CAPTURE(asset_batch, DirectoryBatchQueries, AssetKind::directory, true, false)->Arg(256)->Arg(4096);
BENCHMARK_CAPTURE(asset_batch, PackedScalarQueries, AssetKind::packed, false, false)->Arg(256)->Arg(4096);
BENCHMARK_CAPTURE(asset_batch, PackedBatchQueries, AssetKind::packed, true, false)->Arg(256)->Arg(4096);
BENCHMARK_CAPTURE(asset_batch, MemoryScalarQueries, AssetKind::memory, false, false)->Arg(256)->Arg(4096);
BENCHMARK_CAPTURE(asset_batch, MemoryBatchQueries, AssetKind::memory, true, false)->Arg(256)->Arg(4096);
BENCHMARK_CAPTURE(asset_batch, DirectoryScalarDigest, AssetKind::directory, false, true)->Arg(256)->Arg(4096);
BENCHMARK_CAPTURE(asset_batch, DirectoryBatchDigest, AssetKind::directory, true, true)->Arg(256)->Arg(4096);
BENCHMARK_CAPTURE(asset_batch, PackedScalarDigest, AssetKind::packed, false, true)->Arg(256)->Arg(4096);
BENCHMARK_CAPTURE(asset_batch, PackedBatchDigest, AssetKind::packed, true, true)->Arg(256)->Arg(4096);
BENCHMARK_CAPTURE(asset_batch, MemoryScalarDigest, AssetKind::memory, false, true)->Arg(256)->Arg(4096);
BENCHMARK_CAPTURE(asset_batch, MemoryBatchDigest, AssetKind::memory, true, true)->Arg(256)->Arg(4096);

}// namespace

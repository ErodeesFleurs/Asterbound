module;

#include <benchmark/benchmark.h>

export module star.benchmark_asset_provider;

import std;
import star.algorithm;
import star.asset_source;
import star.byte_array;
import star.directory_asset_source;
import star.file;
import star.format;
import star.logging;
import star.memory_asset_source;
import star.packed_asset_source;
import star.random;
import star.string;

export namespace BenchmarkFixtures {

enum class AssetKind { directory, packed, memory };

inline Star::ByteArray deterministic_bytes(std::size_t size, std::size_t key, unsigned generation) {
  Star::ByteArray bytes(size, '\0');
  for (std::size_t i = 0; i != size; ++i)
    bytes[i] = static_cast<char>((i * 17 + key * 31 + generation * 67) % 251);
  return bytes;
}

// The callback keeps every provider, logger, payload and private directory alive
// through the consumer; sources are destroyed before directory cleanup.
template <class Consumer>
  requires requires(Consumer& consume, std::shared_ptr<Star::AssetSource> const& source,
                    std::vector<Star::String> const& paths, std::vector<Star::ByteArray> const& payloads) {
    { std::invoke(consume, source, paths, payloads) } -> std::same_as<void>;
  }
void with_asset_provider(benchmark::State& state, AssetKind kind, std::size_t count,
                         Star::String const& memory_name, std::string_view index_error, Consumer&& consume) {
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
  Star::Logger logger;
  auto directory_path = Star::File::relative_to(*temporary, "assets");
  auto created = Star::File::create_directory(directory_path);
  if (!created) {
    state.SkipWithError(created.error().what());
    return;
  }
  std::vector<Star::String> paths;
  std::vector<Star::ByteArray> payloads;
  paths.reserve(count);
  payloads.reserve(count);
  auto memory = std::make_shared<Star::MemoryAssetSource>(memory_name);
  for (std::size_t i = 0; i != count; ++i) {
    auto name = Star::format("asset-{:05}.bin", i);
    paths.push_back("/" + name);
    payloads.push_back(deterministic_bytes(1 + (i * 37) % 512, i, 0));
    auto written = Star::File::write_file(payloads.back(), Star::File::relative_to(directory_path, name));
    if (!written) {
      state.SkipWithError(written.error().what());
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
  if (source->assetPaths().size() != count) {
    state.SkipWithError(std::string(index_error).c_str());
    return;
  }
  std::invoke(consume, std::as_const(source), std::as_const(paths), std::as_const(payloads));
}

}// namespace BenchmarkFixtures

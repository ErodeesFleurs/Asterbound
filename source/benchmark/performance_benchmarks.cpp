#include <cstdio>
#include <benchmark/benchmark.h>

import std;
import star.file;
import star.format;
import star.logging;
import star.native_runtime;
import star.benchmark_runner;
import star.performance_benchmarks;
import star.root;
import star.root_loader;
import star.string;

namespace {

std::expected<void, Star::StarException> requirePrivateStorage(Star::String const& storage) {
  for (std::string_view name : {"player", "universe"}) {
    auto exists = Star::File::exists(Star::File::relative_to(storage, name));
    if (!exists)
      return std::unexpected(std::move(exists.error()));
    if (*exists)
      return std::unexpected(Star::StarException("Performance benchmarks require private storage, not saves"));
  }
  return {};
}

}// namespace

int main(int argc, char** argv) {
  auto nativeRuntime = Star::NativeRuntime::create();
  if (!nativeRuntime) {
    if (!Star::print(*stderr, "{}\n", nativeRuntime.error().what()))
      return 1;
    return 1;
  }
  Star::BenchmarkLifetime benchmark_lifetime(argc, argv);
  auto loader = Star::RootLoader::create({{}, {}, {}, Star::LogLevel::Error, true, {}});
  if (!loader) {
    if (!Star::print(*stderr, "{}\n", loader.error().what()))
      return 1;
    return 1;
  }
  auto parsed = loader->command_parse_or_die({argv, static_cast<std::size_t>(argc)});
  if (!parsed) {
    if (!Star::print(*stderr, "{}\n", parsed.error().what()))
      return 1;
    return 1;
  }
  auto& settings = parsed->first;
  if (auto privateStorage = requirePrivateStorage(settings.storageDirectory); !privateStorage) {
    if (!Star::print(*stderr, "{}\n", privateStorage.error().what()))
      return 1;
    return 1;
  }
  settings.runtimeConfigFile.reset();
  settings.logFile.reset();
  auto root = Star::Root::create(std::move(settings));
  if (!root) {
    if (!Star::print(*stderr, "{}\n", root.error().what()))
      return 1;
    return 1;
  }
  Star::PerformanceBenchmarkSession session{**root};
  auto registered = Star::registerCollisionPerformanceBenchmarks(session)
                      .and_then([&] { return Star::registerLightingPerformanceBenchmarks(session); })
                      .and_then([&] { return Star::registerNetworkPerformanceBenchmarks(session); })
                      .and_then([&] { return Star::register_liquid_performance_benchmarks(session); })
                      .and_then([&] { return Star::registerAssetsPerformanceBenchmarks(session); });
  if (!registered) {
    if (!Star::print(*stderr, "{}\n", registered.error().what()))
      return 1;
    benchmark::ClearRegisteredBenchmarks();
    return 1;
  }
  auto result = Star::run_registered_benchmarks();
  benchmark::ClearRegisteredBenchmarks();
  return session.failed || result != 0 ? 1 : 0;
}

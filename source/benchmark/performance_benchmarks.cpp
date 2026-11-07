#include <benchmark/benchmark.h>

import std;
import star.file;
import star.format;
import star.logging;
import star.native_runtime;
import star.performance_benchmarks;
import star.root;
import star.root_loader;
import star.string;

namespace {

struct BenchmarkLifetime {
  ~BenchmarkLifetime() {
    benchmark::ClearRegisteredBenchmarks();
    benchmark::Shutdown();
  }
};

class CheckedReporter : public benchmark::ConsoleReporter {
public:
  void ReportRuns(std::vector<Run> const& runs) override {
    for (auto const& run : runs)
      failed |= run.skipped == benchmark::internal::SkippedWithError;
    ConsoleReporter::ReportRuns(runs);
  }

  bool failed = false;
};

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
    std::fprintf(stderr, "%s\n", nativeRuntime.error().what());
    return 1;
  }
  benchmark::Initialize(&argc, argv);
  BenchmarkLifetime lifetime;
  auto loader = Star::RootLoader::create({{}, {}, {}, Star::LogLevel::Error, true, {}});
  if (!loader) {
    std::fprintf(stderr, "%s\n", loader.error().what());
    return 1;
  }
  auto parsed = loader->commandParseOrDie(argc, argv);
  if (!parsed) {
    std::fprintf(stderr, "%s\n", parsed.error().what());
    return 1;
  }
  auto& settings = parsed->first;
  if (auto privateStorage = requirePrivateStorage(settings.storageDirectory); !privateStorage) {
    std::fprintf(stderr, "%s\n", privateStorage.error().what());
    return 1;
  }
  settings.runtimeConfigFile.reset();
  settings.logFile.reset();
  auto root = Star::Root::create(std::move(settings));
  if (!root) {
    std::fprintf(stderr, "%s\n", root.error().what());
    return 1;
  }
  Star::PerformanceBenchmarkSession session{**root};
  auto registered = Star::registerCollisionPerformanceBenchmarks(session)
                      .and_then([&] { return Star::registerLightingPerformanceBenchmarks(session); })
                      .and_then([&] { return Star::registerNetworkPerformanceBenchmarks(session); })
                      .and_then([&] { return Star::registerLiquidPerformanceBenchmarks(session); })
                      .and_then([&] { return Star::registerAssetsPerformanceBenchmarks(session); });
  if (!registered) {
    std::fprintf(stderr, "%s\n", registered.error().what());
    benchmark::ClearRegisteredBenchmarks();
    return 1;
  }
  CheckedReporter reporter;
  auto matched = benchmark::RunSpecifiedBenchmarks(&reporter);
  benchmark::ClearRegisteredBenchmarks();
  return session.failed || reporter.failed || matched == 0 ? 1 : 0;
}

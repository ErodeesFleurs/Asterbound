#include <benchmark/benchmark.h>

import std;
import star.algorithm;
import star.assets;
import star.casting;
import star.file;
import star.format;
import star.json;
import star.list;
import star.logging;
import star.lua;
import star.performance_benchmarks;
import star.ref_ptr;
import star.root;
import star.string;

namespace Star {

struct AssetWorkerBenchmarkAccess {
  static std::expected<std::size_t, StarException> finish(Assets& assets) {
    std::unique_lock lock(assets.m_assetsMutex);
    if (!assets.m_assetsDone.wait_for(lock, std::chrono::seconds(60), [&] {
          return assets.m_queue.empty() || assets.m_workerError || assets.m_stopThreads;
        }))
      return std::unexpected(StarException("Asset worker benchmark did not finish queued work"));
    if (assets.m_workerError)
      return std::unexpected(*assets.m_workerError);
    if (assets.m_stopThreads)
      return std::unexpected(StarException("Asset worker stopped before benchmark completion"));
    lock.unlock();
    std::lock_guard luaLock(assets.m_luaMutex);
    return asRef<LuaEngine>(*assets.m_luaEngine)->get().memoryUsage();
  }

  static std::expected<std::size_t, StarException> collect(Assets& assets) {
    std::lock_guard lock(assets.m_luaMutex);
    auto& engine = asRef<LuaEngine>(*assets.m_luaEngine)->get();
    auto collected = engine.collectGarbage();
    if (!collected)
      return std::unexpected(std::move(collected.error()));
    return engine.memoryUsage();
  }
};

namespace {

void assetWorkers(benchmark::State& state, PerformanceBenchmarkSession& session, bool mixed = false) {
  auto directory = File::temporaryDirectory(session.root.random());
  if (!directory) {
    session.fail(state, directory.error());
    return;
  }
  auto cleanup = finally([&] {
    auto removed = File::removeDirectoryRecursive(*directory);
    if (!removed)
      session.fail(state, removed.error());
  });
  auto count = static_cast<unsigned>(state.range(0));
  StringList paths;
  auto prepared = File::writeFile(String("[]"), File::relativeTo(*directory, "preload.config"));
  if (!prepared) {
    session.fail(state, prepared.error());
    return;
  }
  for (unsigned i = 0; i != count; ++i) {
    auto name = strf("asset{}.json", i);
    auto written = File::writeFile(String(strf("{{\"index\":{}}}", i)), File::relativeTo(*directory, name));
    if (!written) {
      session.fail(state, written.error());
      return;
    }
    if (!mixed || i % 2 == 0) {
      auto patch = File::writeFile(String(R"lua(function patch(asset)
  local garbage = {}
  for i = 1, 256 do
    local node = {value = i, text = "asset-temporary-" .. i}
    node.self = node
    garbage[i] = node
  end
  asset.checksum = garbage[1].value + garbage[256].value + asset.index
  return asset
end
)lua"),
                                   File::relativeTo(*directory, name + ".patch.lua"));
      if (!patch) {
        session.fail(state, patch.error());
        return;
      }
    }
    paths.append("/" + name);
  }
  Assets::Settings settings{};
  settings.workerPoolSize = static_cast<unsigned>(state.range(1));
  settings.assetTimeToLive = 600;
  std::size_t heapAfterQueue = 0;
  std::size_t heapAfterCollect = 0;
  for (auto _ : state) {
    state.PauseTiming();
    auto assets =
      Assets::create(session.root.logger(), session.root.random(), session.root.logMap(), settings, {*directory});
    if (!assets) {
      state.ResumeTiming();
      session.fail(state, assets.error());
      break;
    }
    state.ResumeTiming();
    auto queued = (*assets)->queueJsons(paths);
    auto completed = queued ? AssetWorkerBenchmarkAccess::finish(**assets)
                            : std::expected<std::size_t, StarException>(std::unexpected(std::move(queued.error())));
    if (!completed) {
      session.fail(state, completed.error());
      break;
    }
    heapAfterQueue = std::max(heapAfterQueue, *completed);
    bool valid = true;
    for (unsigned i = 0; i != count; ++i) {
      auto json = (*assets)->json(paths[i]);
      if (!json) {
        session.fail(state, json.error());
        valid = false;
        break;
      }
      auto patched = !mixed || i % 2 == 0;
      auto checksum = patched ? json->getInt("checksum") : json->getInt("index");
      if (!checksum || *checksum != (patched ? 257 + i : i)) {
        session.fail(state, "Asset worker produced an incorrect JSON result");
        valid = false;
        break;
      }
    }
    auto collected = AssetWorkerBenchmarkAccess::collect(**assets);
    if (!collected) {
      session.fail(state, collected.error());
      valid = false;
    } else {
      heapAfterCollect = std::max(heapAfterCollect, *collected);
    }
    assets->reset();
    if (!valid)
      break;
  }
  state.counters["lua_heap_after_queue_bytes"] = static_cast<double>(heapAfterQueue);
  state.counters["lua_heap_after_full_collect_bytes"] = static_cast<double>(heapAfterCollect);
  state.SetItemsProcessed(state.iterations() * count);
  state.SetLabel(mixed
      ? "256 JSONs, alternating plain/Lua; queue/readback/full-GC/destruction timed; source/setup untimed; heap is not peak"
      : "real async JSON workers; cyclic Lua patch garbage; queue/readback/full-GC/destruction timed; source/setup untimed; heap is not peak");
}

}// namespace

std::expected<void, StarException> registerAssetsPerformanceBenchmarks(PerformanceBenchmarkSession& session) {
  benchmark::RegisterBenchmark("Assets/QueuedLuaPatchesAndReclaim",
                               [&](benchmark::State& state) { assetWorkers(state, session); })
    ->Args({64, 1})
    ->Args({256, 1})
    ->Args({256, 4})
    ->UseRealTime();
  benchmark::RegisterBenchmark("Assets/MixedQueuedJsonPatchesAndReclaim",
                               [&](benchmark::State& state) { assetWorkers(state, session, true); })
    ->Args({256, 1})
    ->Args({256, 2})
    ->Args({256, 4})
    ->UseRealTime();
  return {};
}

}// namespace Star

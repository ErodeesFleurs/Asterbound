#include <benchmark/benchmark.h>

import std;
import star.algorithm;
import star.assets;
import star.buffer;
import star.byte_array;
import star.file;
import star.format;
import star.image;
import star.json;
import star.list;
import star.logging;
import star.lua;
import star.performance_benchmarks;
import star.ref_ptr;
import star.root;
import star.asset_path;
import star.string;
import star.vector;

namespace Star {

struct AssetWorkerBenchmarkAccess {
  static std::size_t queued_count(Assets& assets) {
    std::lock_guard lock(assets.m_assets_mutex);
    return assets.m_queue.size();
  }

  static auto hold_lua(Assets& assets) {
    return std::unique_lock(assets.m_lua_mutex);
  }

  static bool await_queue_size(Assets& assets, std::size_t size) {
    std::unique_lock lock(assets.m_assets_mutex);
    return assets.m_assets_done.wait_for(lock, std::chrono::seconds(10), [&] {
      return assets.m_queue.size() == size;
    });
  }

  static std::expected<std::size_t, StarException> finish(Assets& assets) {
    std::unique_lock lock(assets.m_assets_mutex);
    if (!assets.m_assets_done.wait_for(lock, std::chrono::seconds(60), [&] {
          return assets.m_queue.empty() || assets.m_worker_error || assets.m_worker_stop_source.stop_requested();
        }))
      return std::unexpected(StarException("Asset worker benchmark did not finish queued work"));
    if (assets.m_worker_error)
      return std::unexpected(*assets.m_worker_error);
    if (assets.m_worker_stop_source.stop_requested())
      return std::unexpected(StarException("Asset worker stopped before benchmark completion"));
    lock.unlock();
    std::lock_guard lua_lock(assets.m_lua_mutex);
    return assets.m_lua_engine->memory_usage();
  }

  static std::expected<std::size_t, StarException> collect(Assets& assets) {
    std::lock_guard lock(assets.m_lua_mutex);
    auto& engine = *assets.m_lua_engine;
    auto collected = engine.collect_garbage();
    if (!collected)
      return std::unexpected(std::move(collected.error()));
    return engine.memory_usage();
  }
};

namespace {

void assetWorkers(benchmark::State& state, PerformanceBenchmarkSession& session, bool mixed = false) {
  auto directory = File::temporary_directory(session.root.random());
  if (!directory) {
    session.fail(state, directory.error());
    return;
  }
  auto cleanup = make_scope_exit([&] {
    auto removed = File::remove_all(*directory);
    if (!removed)
      session.fail(state, removed.error());
  });
  auto count = static_cast<unsigned>(state.range(0));
  StringList paths;
  auto prepared = File::write_file(String("[]"), File::relative_to(*directory, "preload.config"));
  if (!prepared) {
    session.fail(state, prepared.error());
    return;
  }
  for (unsigned i = 0; i != count; ++i) {
    auto name = Star::format("asset{}.json", i);
    auto written = File::write_file(String(Star::format("{{\"index\":{}}}", i)), File::relative_to(*directory, name));
    if (!written) {
      session.fail(state, written.error());
      return;
    }
    if (!mixed || i % 2 == 0) {
      auto patch = File::write_file(String(R"lua(function patch(asset)
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
                                    File::relative_to(*directory, name + ".patch.lua"));
      if (!patch) {
        session.fail(state, patch.error());
        return;
      }
    }
    paths.push_back("/" + name);
  }
  Assets::Settings settings{};
  settings.worker_pool_size = static_cast<unsigned>(state.range(1));
  settings.asset_time_to_live = 600;
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
    auto queued = (*assets)->queue_jsons(paths);
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
      auto checksum = patched ? json->get_int("checksum") : json->get_int("index");
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

// Hold genuine synchronous JSON patches at the Lua VM. A background worker
// must still dispatch newly queued plain JSON and signal completion.
void idle_worker_dispatch(benchmark::State& state, PerformanceBenchmarkSession& session) {
  auto directory = File::temporary_directory(session.root.random());
  if (!directory)
    return session.fail(state, directory.error());
  auto remove_fixture = make_scope_exit([&] {
    if (auto result = File::remove_all(*directory); !result)
      session.fail(state, result.error());
  });
  auto store = [&](String const& name, String const& data) {
    return File::write_file(data, File::relative_to(*directory, name));
  };
  if (auto preload = store("preload.config", "[]"); !preload)
    return session.fail(state, preload.error());
  auto in_flight = static_cast<std::size_t>(state.range(0));
  StringList patched, plain;
  for (std::size_t i = 0; i != in_flight; ++i) {
    auto name = String(Star::format("patched{}.json", i));
    if (auto written = store(name, "{}"); !written)
      return session.fail(state, written.error());
    if (auto written = store(name + ".patch.lua", "function patch(a) a.ok = true; return a end"); !written)
      return session.fail(state, written.error());
    patched.push_back("/" + name);
  }
  for (unsigned i = 0; i != 64; ++i) {
    auto name = String(Star::format("plain{}.json", i));
    if (auto written = store(name, R"({"ok":true})"); !written)
      return session.fail(state, written.error());
    plain.push_back("/" + name);
  }
  for (auto _ : state) {
    state.PauseTiming();
    Assets::Settings settings{};
    // One worker may already be collecting Lua when the VM is held.
    settings.worker_pool_size = 2;
    auto assets = Assets::create(session.root.logger(), session.root.random(), session.root.logMap(), settings, {*directory});
    if (!assets)
      return session.fail(state, assets.error());
    std::vector<std::future<std::expected<Json, StarException>>> readers;
    readers.reserve(in_flight);
    auto lua_lock = AssetWorkerBenchmarkAccess::hold_lua(**assets);
    if (in_flight == 0)
      lua_lock.unlock();
    for (auto const& path : patched)
      readers.push_back(std::async(std::launch::async, [&assets, &path] { return (*assets)->json(path); }));
    // A synchronous load entering Working does not signal the completion CV.
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (AssetWorkerBenchmarkAccess::queued_count(**assets) != in_flight) {
      if (std::chrono::steady_clock::now() >= deadline)
        return session.fail(state, "Synchronous loads did not all enter the queue");
      std::this_thread::yield();
    }
    state.ResumeTiming();
    for (auto const& path : plain) {
      if (auto queued = (*assets)->queue_jsons(StringList{path}); !queued)
        return session.fail(state, queued.error());
      if (!AssetWorkerBenchmarkAccess::await_queue_size(**assets, in_flight))
        return session.fail(state, "Idle worker did not complete a queued plain JSON");
    }
    state.PauseTiming();
    if (lua_lock.owns_lock())
      lua_lock.unlock();
    for (auto& reader : readers) {
      auto result = reader.get();
      if (!result)
        return session.fail(state, result.error());
      if (result->get_bool("ok") != true)
        return session.fail(state, "Synchronous Lua patch did not finish");
    }
    for (auto const& path : plain) {
      auto json = (*assets)->json(path);
      if (!json || json->get_bool("ok") != true)
        return session.fail(state, "Dispatched JSON readback failed");
    }
    assets->reset();
    state.ResumeTiming();
  }
  state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(plain.size()));
}

void narrow_catalog_scan(benchmark::State& state, PerformanceBenchmarkSession& session) {
  auto assets = session.root.assets();
  if (!assets)
    return session.fail(state, assets.error());
  String const prefix = "/interface/title/", suffix = ".png";
  auto indexed = (*assets)->scan_extension(suffix);
  if (!indexed)
    return session.fail(state, indexed.error());
  std::size_t expected_count = 0;
  for (auto const& key : indexed->get()) {
    auto matches = key.string().starts_with(prefix, String::CaseInsensitive);
    if (!matches)
      return session.fail(state, matches.error());
    expected_count += *matches;
  }
  if (expected_count == 0 || expected_count == indexed->get().size())
    return session.fail(state, "Expected a nonempty narrow prefix of the PNG catalog");
  for (auto _ : state) {
    auto matches = (*assets)->scan(prefix, suffix);
    if (!matches)
      return session.fail(state, matches.error());
    if (matches->size() != expected_count)
      return session.fail(state, "Narrow scan dropped or added paths");
    benchmark::DoNotOptimize(*matches);
  }
  state.counters["indexed"] = static_cast<double>(indexed->get().size());
  state.counters["matched"] = static_cast<double>(expected_count);
}

enum class CatalogWork { QueueList, QueueSet, SharedFrames, ReferenceCleanup };

void catalog_work(benchmark::State& state, PerformanceBenchmarkSession& session, CatalogWork work) {
  auto directory = File::temporary_directory(session.root.random());
  if (!directory)
    return session.fail(state, directory.error());
  auto remove_fixture = make_scope_exit([&] {
    if (auto result = File::remove_all(*directory); !result)
      session.fail(state, result.error());
  });
  if (auto written = File::write_file(String("[]"), File::relative_to(*directory, "preload.config")); !written)
    return session.fail(state, written.error());
  auto png = std::make_shared<Buffer>();
  if (auto encoded = Image::filled({2, 2}, {255, 255, 255, 255}).write_png(session.root.logger(), png); !encoded)
    return session.fail(state, encoded.error());
  auto bytes = png->take_data();
  auto count = static_cast<std::size_t>(state.range(0));
  StringList paths;
  paths.reserve(count);
  for (std::size_t i = 0; i != count; ++i) {
    auto name = String(Star::format("image{}.png", i));
    if (auto stored = File::write_file(bytes, File::relative_to(*directory, name)); !stored)
      return session.fail(state, stored.error());
    paths.push_back("/" + name);
  }
  if (work == CatalogWork::SharedFrames) {
    auto stored = File::write_file(String(R"({"frameGrid":{"size":[2,2],"dimensions":[1,1]}})"),
                                   File::relative_to(*directory, "default.frames"));
    if (!stored)
      return session.fail(state, stored.error());
  }
  Assets::Settings settings{};
  settings.asset_time_to_live = -1;
  settings.reference_image_pixel_budget = 4 * (count / 2);
  auto assets = Assets::create(session.root.logger(), session.root.random(), session.root.logMap(), settings, {*directory});
  if (!assets)
    return session.fail(state, assets.error());
  auto indexed = (*assets)->scan_extension("png");
  if (!indexed)
    return session.fail(state, indexed.error());
  if (indexed->get().size() != count)
    return session.fail(state, "Fixture PNG catalog count mismatch");
  std::vector<std::shared_ptr<FramesSpecification const>> frames(count);
  std::vector<std::weak_ptr<Image const>> references(count);
  for (auto _ : state) {
    state.PauseTiming();
    (*assets)->hot_reload();
    if (work == CatalogWork::ReferenceCleanup) {
      for (std::size_t i = 0; i != count; ++i) {
        auto image = (*assets)->image(paths[i]);
        if (!image)
          return session.fail(state, image.error());
        references[i] = *image;
        auto blended = (*assets)->image(paths.front() + "?blendmult=" + paths[i]);
        if (!blended)
          return session.fail(state, blended.error());
      }
    }
    state.ResumeTiming();
    if (work == CatalogWork::SharedFrames) {
      for (std::size_t i = 0; i != count; ++i) {
        auto spec = (*assets)->image_frames(paths[i]);
        if (!spec)
          return session.fail(state, spec.error());
        frames[i] = std::move(*spec);
      }
    } else if (work == CatalogWork::ReferenceCleanup) {
      (*assets)->cleanup();
    } else {
      auto queued = work == CatalogWork::QueueSet ? (*assets)->queue_images(indexed->get()) : (*assets)->queue_images(paths);
      if (!queued)
        return session.fail(state, queued.error());
    }
    state.PauseTiming();
    if (work == CatalogWork::SharedFrames) {
      std::set<std::shared_ptr<FramesSpecification const>> distinct;
      for (auto const& spec : frames) {
        if (!spec || spec->frames.size() != 1)
          return session.fail(state, "Missing frame grid");
        distinct.insert(spec);
      }
      state.counters["distinct_specs"] = static_cast<double>(distinct.size());
      for (auto& spec : frames)
        spec.reset();
    } else if (work == CatalogWork::ReferenceCleanup) {
      auto alive = std::ranges::count_if(references, [](std::weak_ptr<Image const> const& ref) { return !ref.expired(); });
      if (static_cast<std::size_t>(alive) != count / 2)
        return session.fail(state, "Reference image cleanup did not retain its budget");
    } else if (AssetWorkerBenchmarkAccess::queued_count(**assets) != count) {
      return session.fail(state, "Batch did not enqueue every image");
    }
    state.ResumeTiming();
  }
  state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(count));
}

}// namespace

std::expected<void, StarException> registerAssetsPerformanceBenchmarks(PerformanceBenchmarkSession& session) {
  benchmark::RegisterBenchmark("Assets/IdleWorkerDispatch",
                               [&](benchmark::State& state) { idle_worker_dispatch(state, session); })
    ->Arg(0)->Arg(32)->UseRealTime();
  benchmark::RegisterBenchmark("Assets/NarrowPngScan",
                               [&](benchmark::State& state) { narrow_catalog_scan(state, session); })->UseRealTime();
  for (auto [name, work] : {std::pair{"Assets/QueueImageList", CatalogWork::QueueList},
                            std::pair{"Assets/QueueImageSet", CatalogWork::QueueSet},
                            std::pair{"Assets/SharedDefaultFrames", CatalogWork::SharedFrames},
                            std::pair{"Assets/ReferenceBudgetCleanup", CatalogWork::ReferenceCleanup}}) {
    benchmark::RegisterBenchmark(name, [&, work](benchmark::State& state) { catalog_work(state, session, work); })
      ->Arg(128)->UseRealTime();
  }
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

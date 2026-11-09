#include <benchmark/benchmark.h>

import std;
import star.algorithm;
import star.file;
import star.format;
import star.json;
import star.player;
import star.player_factory;
import star.player_storage;
import star.root;
import star.string;
import star.uuid;

namespace {

using namespace Star;

enum class storage_operation { metadata, order };

std::expected<std::shared_ptr<Player>, StarException> create_benchmark_player(Root& root, String const& name) {
  auto factory = root.playerFactory();
  if (!factory)
    return std::unexpected(std::move(factory.error()));
  auto player = (*factory)->create();
  if (!player)
    return std::unexpected(std::move(player.error()));
  if (auto named = (*player)->setName(name); !named)
    return std::unexpected(std::move(named.error()));
  if (auto finalized = (*player)->finalizeCreation(); !finalized)
    return std::unexpected(std::move(finalized.error()));
  return std::move(*player);
}

void storage_updates(benchmark::State& state, Root& root, bool& failed, storage_operation operation, bool changed) {
  auto fail = [&](StarException const& error) {
    failed = true;
    state.SkipWithError(error.what());
  };
  auto temporary = File::temporary_directory(root.random());
  if (!temporary) {
    fail(temporary.error());
    return;
  }
  auto cleanup = finally([&] {
    if (auto removed = File::remove_all(*temporary); !removed)
      fail(removed.error());
  });
  auto directory = File::relative_to(*temporary, "player");
  auto storage = PlayerStorage::create(root, directory);
  if (!storage) {
    fail(storage.error());
    return;
  }
  std::array<Uuid, 2> uuids{Uuid::nil(), Uuid::nil()};
  for (std::size_t i = 0; i != uuids.size(); ++i) {
    auto player =
      create_benchmark_player(root, i == 0 ? String("Storage benchmark left") : String("Storage benchmark right"));
    if (!player) {
      fail(player.error());
      return;
    }
    auto uuid = (*player)->uuid();
    if (!uuid) {
      fail(uuid.error());
      return;
    }
    uuids[i] = *uuid;
    if (auto saved = (*storage)->savePlayer(*player); !saved) {
      fail(saved.error());
      return;
    }
  }
  if (uuids[0] == uuids[1]) {
    fail(StarException("Real benchmark players unexpectedly share an identity"));
    return;
  }
  String const key = "selection";
  std::array<Json, 2> const values{Json("left"), Json("right")};
  std::size_t selected = 0;
  if (auto ordered = (*storage)->moveToFront(uuids[selected]); !ordered) {
    fail(ordered.error());
    return;
  }
  if (auto seeded = (*storage)->setMetadata(key, values[selected]); !seeded) {
    fail(seeded.error());
    return;
  }
  auto verify_bytes = [&]() -> std::expected<void, StarException> {
    auto contents = File::read_file_string(File::relative_to(directory, "metadata"));
    if (!contents)
      return std::unexpected(std::move(contents.error()));
    auto json = Json::parseJson(*contents);
    if (!json)
      return std::unexpected(std::move(json.error()));
    auto value = json->get(key);
    if (!value)
      return std::unexpected(std::move(value.error()));
    auto order = json->get("order");
    if (!order)
      return std::unexpected(std::move(order.error()));
    auto value_index = operation == storage_operation::metadata ? selected : 0;
    auto front_index = operation == storage_operation::order ? selected : 0;
    if (*value != values[value_index]
        || *order != Json(JsonArray{uuids[front_index].hex(), uuids[1 - front_index].hex()}))
      return std::unexpected(StarException("Storage benchmark durable metadata readback mismatch"));
    return {};
  };
  if (auto verified = verify_bytes(); !verified) {
    fail(verified.error());
    return;
  }
  state.SetLabel(changed ? "real PlayerStorage; changed update includes durable replacement; setup/readback untimed"
                         : "real PlayerStorage; acknowledged unchanged update; setup/readback untimed");
  for (auto _ : state) {
    (void)_;
    if (changed)
      selected = 1 - selected;
    auto updated = operation == storage_operation::metadata ? (*storage)->setMetadata(key, values[selected])
                                                            : (*storage)->moveToFront(uuids[selected]);
    benchmark::DoNotOptimize(updated.has_value());
    if (!updated) {
      fail(updated.error());
      break;
    }
    state.PauseTiming();
    auto verified = verify_bytes();
    state.ResumeTiming();
    if (!verified) {
      fail(verified.error());
      break;
    }
  }
  state.SetItemsProcessed(state.iterations());
  // A new storage owner validates persisted state independently of the writer's
  // acknowledgement cache. Real assets/player setup and this reload are untimed.
  storage->reset();
  auto reloaded = PlayerStorage::create(root, directory);
  if (!reloaded) {
    fail(reloaded.error());
    return;
  }
  auto value = (*reloaded)->getMetadata(key);
  if (!value) {
    fail(value.error());
    return;
  }
  auto value_index = operation == storage_operation::metadata ? selected : 0;
  auto front_index = operation == storage_operation::order ? selected : 0;
  if ((*reloaded)->playerCount() != uuids.size() || *value != values[value_index]
      || (*reloaded)->playerUuidAt(0) != uuids[front_index])
    fail(StarException("Storage benchmark new-owner reload mismatch"));
}

}// namespace

void register_player_storage_benchmarks(Star::Root& root, bool& failed) {
  benchmark::RegisterBenchmark("PlayerStorage/MetadataUnchanged",
                               [&root, &failed](benchmark::State& state) {
                                 storage_updates(state, root, failed, storage_operation::metadata, false);
                               })
    ->MinTime(0.1)
    ->UseRealTime()
    ->Unit(benchmark::kMicrosecond);
  benchmark::RegisterBenchmark("PlayerStorage/MetadataChangedDurable",
                               [&root, &failed](benchmark::State& state) {
                                 storage_updates(state, root, failed, storage_operation::metadata, true);
                               })
    ->MinTime(0.1)
    ->UseRealTime()
    ->Unit(benchmark::kMicrosecond);
  benchmark::RegisterBenchmark("PlayerStorage/OrderUnchanged",
                               [&root, &failed](benchmark::State& state) {
                                 storage_updates(state, root, failed, storage_operation::order, false);
                               })
    ->MinTime(0.1)
    ->UseRealTime()
    ->Unit(benchmark::kMicrosecond);
  benchmark::RegisterBenchmark(
    "PlayerStorage/OrderChangedDurable",
    [&root, &failed](benchmark::State& state) { storage_updates(state, root, failed, storage_operation::order, true); })
    ->MinTime(0.1)
    ->UseRealTime()
    ->Unit(benchmark::kMicrosecond);
}

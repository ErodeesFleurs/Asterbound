#include <benchmark/benchmark.h>

import std;
import star.assets;
import star.asset_path;
import star.audio;
import star.buffer;
import star.byte_array;
import star.data_stream;
import star.data_stream_devices;
import star.data_stream_extra;
import star.file;
import star.format;
import star.image;
import star.item;
import star.item_bag;
import star.item_database;
import star.item_descriptor;
import star.json;
import star.list;
import star.logging;
import star.native_runtime;
import star.root;
import star.root_loader;
import star.string;

using namespace Star;

namespace {

struct CacheFixture;

struct BenchmarkSession {
  Root& root;
  bool failed = false;
  std::map<std::size_t, std::unique_ptr<CacheFixture>> cacheFixtures{};

  void fail(benchmark::State& state, std::string_view message) {
    failed = true;
    state.SkipWithError(message.data());
  }

  void fail(benchmark::State& state, StarException const& error) {
    fail(state, error.what());
  }
};

struct BenchmarkLifetime {
  ~BenchmarkLifetime() {
    benchmark::ClearRegisteredBenchmarks();
    benchmark::Shutdown();
  }
};

class CheckedReporter : public benchmark::ConsoleReporter {
public:
  void ReportRuns(std::vector<Run> const& reports) override {
    for (auto const& run : reports)
      failed |= run.skipped == benchmark::internal::SkippedWithError;
    benchmark::ConsoleReporter::ReportRuns(reports);
  }
  bool failed = false;
};

std::expected<void, StarException> requirePrivateStorage(String const& storage) {
  for (std::string_view name : {"player", "universe"}) {
    auto exists = File::exists(File::relative_to(storage, name));
    if (!exists)
      return std::unexpected(std::move(exists.error()));
    if (*exists)
      return std::unexpected(StarException("Ownership benchmarks require private bootconfig storage without player/universe directories"));
  }
  return {};
}

// Stable integer-only FNV-1a, never addresses or iteration-dependent totals.
std::uint64_t mix(std::uint64_t hash, std::uint64_t value) {
  for (unsigned shift = 0; shift != 64; shift += 8) {
    hash ^= (value >> shift) & 255;
    hash *= 1099511628211ULL;
  }
  return hash;
}

constexpr std::uint64_t HashSeed = 14695981039346656037ULL;

std::uint64_t hashBytes(ByteArray const& bytes) {
  auto hash = HashSeed;
  for (char byte : bytes) {
    hash ^= static_cast<unsigned char>(byte);
    hash *= 1099511628211ULL;
  }
  return hash;
}

void label(benchmark::State& state, std::string const& definition, std::uint64_t checksum) {
  state.SetLabel(std::format("{}; checksum={:016x}; setup/validation untimed", definition, checksum));
}

using SmallRecord = std::tuple<std::uint64_t, std::uint32_t, bool, String>;

std::expected<void, StarException> streamBatch(std::vector<SmallRecord> const& input, std::vector<SmallRecord>& output) {
  for (std::size_t i = 0; i != input.size(); ++i) {
    auto bytes = DataStreamBuffer::serialize(input[i]);
    if (!bytes)
      return std::unexpected(std::move(bytes.error()));
    auto decoded = DataStreamBuffer::deserialize<SmallRecord>(std::move(*bytes));
    if (!decoded)
      return std::unexpected(std::move(decoded.error()));
    output[i] = std::move(*decoded);
  }
  return {};
}

void streamSerializeReadback(benchmark::State& state, BenchmarkSession& session) {
  auto count = static_cast<std::size_t>(state.range(0));
  std::vector<SmallRecord> input;
  input.reserve(count);
  auto checksum = HashSeed;
  for (std::size_t i = 0; i != count; ++i) {
    input.emplace_back(i * 7919 + 17, static_cast<std::uint32_t>(i * 31), i % 2 != 0, String("inventory-slot"));
    checksum = mix(checksum, i * 7919 + 17);
    checksum = mix(checksum, i * 31);
    checksum = mix(checksum, i % 2);
  }
  std::vector<SmallRecord> output(count);
  auto checked = streamBatch(input, output);
  if (!checked || input.empty() || output != input) {
    if (!checked)
      session.fail(state, checked.error());
    else
      session.fail(state, "Small-record serialization failed complete round-trip validation");
    return;
  }
  label(state, "per record: serialize + owning deserialize + temporary stream/bytes reclamation", checksum);
  for (auto _ : state) {
    (void)_;
    auto result = streamBatch(input, output);
    benchmark::ClobberMemory();
    state.PauseTiming();
    if (!result || output != input) {
      if (!result)
        session.fail(state, result.error());
      else
        session.fail(state, "Timed small-record round trip changed data");
      break;
    }
    state.ResumeTiming();
  }
  state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(count));
}

struct ItemFixture {
  std::vector<std::shared_ptr<Item>> prototypes;
  std::vector<ItemDescriptor> descriptors;
};

std::expected<ItemFixture, StarException> prepareItems(Root& root) {
  auto database = root.itemDatabase();
  if (!database)
    return std::unexpected(std::move(database.error()));
  if (!(*database)->hasItem("perfectlygenericitem"))
    return std::unexpected(StarException("Real perfectlygenericitem fixture is absent"));
  ItemFixture fixture;
  for (unsigned variant = 0; variant != 5; ++variant) {
    auto descriptor = ItemDescriptor::create("perfectlygenericitem", 8, JsonObject{{"ownershipVariant", variant}});
    if (!descriptor)
      return std::unexpected(std::move(descriptor.error()));
    auto item = (*database)->item(*descriptor);
    if (!item)
      return std::unexpected(std::move(item.error()));
    if (!*item || (*item)->name() != descriptor->name() || (*item)->count() != 8 || (*item)->maxStack() < 64)
      return std::unexpected(StarException("Real item fixture was substituted, truncated, or is not stackable"));
    fixture.descriptors.push_back((*item)->descriptor().singular());
    fixture.prototypes.push_back(std::move(*item));
  }
  return fixture;
}

struct BagBatch {
  ItemBag bag;
  std::shared_ptr<Item> incoming;
};

std::expected<BagBatch, StarException> prepareBag(Root& root, ItemFixture const& fixture, std::size_t slots) {
  ItemBag bag(root, slots);
  for (std::size_t i = 0; i != slots; ++i) {
    auto item = fixture.prototypes[i % 4]->clone();
    if (!item)
      return std::unexpected(std::move(item.error()));
    auto set = bag.setItem(i, std::move(*item));
    if (!set)
      return std::unexpected(std::move(set.error()));
  }
  auto incoming = fixture.prototypes[3]->clone();
  if (!incoming)
    return std::unexpected(std::move(incoming.error()));
  if ((*incoming)->setCount(1) != 0)
    return std::unexpected(StarException("Incoming item fixture could not hold one item"));
  return BagBatch{std::move(bag), std::move(*incoming)};
}

std::expected<std::array<std::uint64_t, 5>, StarException> bagBatch(BagBatch& batch, ItemFixture const& fixture) {
  // These are ownership-transfer APIs in both revisions. The benchmark never
  // calls Item comparison or the query-only ItemBag pointer APIs directly.
  auto leftover = batch.bag.stackItems(std::move(batch.incoming));
  if (!leftover)
    return std::unexpected(std::move(leftover.error()));
  batch.incoming = std::move(*leftover);
  std::array<std::uint64_t, 5> available{};
  for (std::size_t i = 0; i != available.size(); ++i)
    available[i] = batch.bag.available(fixture.descriptors[i], true);
  return available;
}

bool validateBag(BagBatch const& batch, std::array<std::uint64_t, 5> const& available, std::size_t slots) {
  if (batch.incoming)
    return false;
  for (std::size_t i = 0; i != available.size(); ++i) {
    auto expected = i == 4 ? 0 : slots / 4 * 8 + (i == 3 ? 1 : 0);
    if (available[i] != expected)
      return false;
  }
  std::uint64_t total = 0;
  for (auto const& item : batch.bag.items()) {
    if (!item || item->count() == 0 || item->count() > item->maxStack())
      return false;
    total += item->count();
  }
  return total == slots * 8 + 1;
}

void itemStackAndMatch(benchmark::State& state, BenchmarkSession& session) {
  auto fixture = prepareItems(session.root);
  if (!fixture) {
    session.fail(state, fixture.error());
    return;
  }
  auto slots = static_cast<std::size_t>(state.range(0));
  auto reference = prepareBag(session.root, *fixture, slots);
  if (!reference) {
    session.fail(state, reference.error());
    return;
  }
  auto expected = bagBatch(*reference, *fixture);
  if (!expected || !validateBag(*reference, *expected, slots)) {
    if (!expected)
      session.fail(state, expected.error());
    else
      session.fail(state, "Real ItemBag must stack exactly one item and match all four variants plus one miss");
    return;
  }
  auto checksum = HashSeed;
  for (auto count : *expected)
    checksum = mix(checksum, count);
  label(state, "perfectlygenericitem, four parameter variants; stackItems + five exact available scans", checksum);
  for (auto _ : state) {
    (void)_;
    state.PauseTiming();
    {
      auto batch = prepareBag(session.root, *fixture, slots);
      if (!batch) {
        session.fail(state, batch.error());
        break;
      }
      state.ResumeTiming();
      auto result = bagBatch(*batch, *fixture);
      benchmark::ClobberMemory();
      state.PauseTiming();
      if (!result || *result != *expected || !validateBag(*batch, *result, slots)) {
        if (!result)
          session.fail(state, result.error());
        else
          session.fail(state, "Timed ItemBag stacking/matching changed inventory counts");
        break;
      }
      // Bag allocation and item reclamation are both outside timing.
    }
    state.ResumeTiming();
  }
  state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(slots * 6));
}

ByteArray makeWave(std::size_t frames) {
  ByteArray bytes;
  auto text = [&](std::string_view value) { bytes.append(std::span(value).first(4)); };
  auto le = [&](std::uint32_t value, unsigned width) {
    for (unsigned i = 0; i != width; ++i)
      bytes.appendByte(static_cast<char>((value >> (8 * i)) & 255));
  };
  text("RIFF"); le(static_cast<std::uint32_t>(36 + frames * 2), 4);
  text("WAVE"); text("fmt "); le(16, 4); le(1, 2); le(1, 2);
  le(22050, 4); le(44100, 4); le(2, 2); le(16, 2);
  text("data"); le(static_cast<std::uint32_t>(frames * 2), 4);
  for (std::size_t i = 0; i != frames; ++i)
    le(static_cast<std::uint16_t>((i * 997) % 60001 - 30000), 2);
  return bytes;
}

struct AudioFixture {
  Audio source;
  std::uint64_t frames;
  std::string name;
};

std::expected<AudioFixture, StarException> prepareAudio(Root& root, bool ogg, std::size_t frames) {
  std::expected<Audio, StarException> audio = [&]() -> std::expected<Audio, StarException> {
    if (!ogg)
      return Audio::create(std::make_shared<Buffer>(makeWave(frames)), "ownership-generated-pcm.wav");
    auto assets = root.assets();
    if (!assets)
      return std::unexpected(std::move(assets.error()));
    auto files = (*assets)->scan("/sfx/", ".ogg");
    if (!files)
      return std::unexpected(std::move(files.error()));
    std::ranges::sort(*files);
    if (files->empty())
      return std::unexpected(StarException("Real /sfx/ Ogg fixture catalog is empty"));
    auto device = (*assets)->openFile((*files)[0]);
    if (!device)
      return std::unexpected(std::move(device.error()));
    return Audio::create(std::move(*device), (*files)[0]);
  }();
  if (!audio)
    return std::unexpected(std::move(audio.error()));
  auto samples = audio->totalSamples();
  if (!samples)
    return std::unexpected(std::move(samples.error()));
  if (*samples < 128 || audio->channels() == 0 || audio->channels() > 8 || audio->compressed() != ogg)
    return std::unexpected(StarException("Audio fixture is empty, too short, or has the wrong encoding/channel shape"));
  auto name = audio->name().utf8();
  return AudioFixture{std::move(*audio), *samples, std::move(name)};
}

struct AudioOutput {
  std::vector<std::int16_t> samples;
  std::vector<std::size_t> counts;
};

std::expected<void, StarException> audioBatch(AudioFixture& fixture, AudioOutput& output, bool clone) {
  auto width = static_cast<std::size_t>(64) * fixture.source.channels();
  for (std::size_t i = 0; i != output.counts.size(); ++i) {
    auto position = (i * 97) % (fixture.frames - 64);
    auto readOne = [&](Audio& audio) -> std::expected<void, StarException> {
      auto seek = audio.seekSample(position);
      if (!seek)
        return std::unexpected(std::move(seek.error()));
      auto read = audio.read(std::span(output.samples).subspan(i * width, width));
      if (!read)
        return std::unexpected(std::move(read.error()));
      output.counts[i] = *read;
      return {};
    };
    if (clone) {
      auto cursor = fixture.source.clone();
      if (!cursor)
        return std::unexpected(std::move(cursor.error()));
      if (auto read = readOne(*cursor); !read)
        return read;
      // The independent cursor/decoder is reclaimed inside the timed batch.
    } else if (auto read = readOne(fixture.source); !read) {
      return read;
    }
  }
  return {};
}

void audioCloneSeekRead(benchmark::State& state, BenchmarkSession& session, bool ogg) {
  auto count = static_cast<std::size_t>(state.range(0));
  auto frames = static_cast<std::size_t>(state.range(1));
  auto fixture = prepareAudio(session.root, ogg, frames);
  if (!fixture) {
    session.fail(state, fixture.error());
    return;
  }
  auto width = static_cast<std::size_t>(64) * fixture->source.channels();
  AudioOutput expected{.samples = std::vector<std::int16_t>(count * width), .counts = std::vector<std::size_t>(count)};
  auto checked = audioBatch(*fixture, expected, false);
  auto reset = fixture->source.seekSample(0);
  if (!checked || !reset || !std::ranges::all_of(expected.counts, [&](auto n) { return n == width; })
      || std::ranges::all_of(expected.samples, [](auto n) { return n == 0; })) {
    if (!checked)
      session.fail(state, checked.error());
    else if (!reset)
      session.fail(state, reset.error());
    else
      session.fail(state, "Audio fixture must produce complete, non-silent short reads");
    return;
  }
  auto checksum = HashSeed;
  for (auto sample : expected.samples)
    checksum = mix(checksum, static_cast<std::uint16_t>(sample));
  label(state, std::format("{}; {} frames; clone + seekSample + read 64 frames + cursor reclamation", fixture->name, fixture->frames), checksum);
  AudioOutput output{std::vector<std::int16_t>(count * width), std::vector<std::size_t>(count)};
  for (auto _ : state) {
    (void)_;
    auto result = audioBatch(*fixture, output, true);
    benchmark::ClobberMemory();
    state.PauseTiming();
    auto position = fixture->source.currentSample();
    if (!result || !position || *position != 0 || output.counts != expected.counts || output.samples != expected.samples) {
      if (!result)
        session.fail(state, result.error());
      else if (!position)
        session.fail(state, position.error());
      else
        session.fail(state, "Cloned audio changed readback or moved its source cursor");
      break;
    }
    state.ResumeTiming();
  }
  state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(count));
}

struct CacheFixture {
  std::shared_ptr<Assets> assets;
  std::vector<AssetPath> images;
  std::vector<String> paths;
  std::vector<std::uint64_t> content;
  std::uint64_t checksum = HashSeed;
};

std::expected<CacheFixture, StarException> prepareCache(BenchmarkSession& session, std::size_t count) {
  if (auto cached = session.cacheFixtures.find(count); cached != session.cacheFixtures.end())
    return *cached->second;
  auto& root = session.root;
  auto rootAssets = root.assets();
  if (!rootAssets)
    return std::unexpected(std::move(rootAssets.error()));
  auto files = (*rootAssets)->scan("/items/", ".png");
  if (!files)
    return std::unexpected(std::move(files.error()));
  std::ranges::sort(*files);
  if (files->size() < count || count == 0)
    return std::unexpected(StarException("Real /items/ PNG catalog is too small for the requested mixed cache fixture"));
  auto settings = root.settings().assetsSettings;
  // Expiry is deterministic without sleeping or touching private cache state.
  // This independent real Assets instance is not serviced by Root maintenance.
  settings.assetTimeToLive = -1.0f;
  settings.missingImage.reset();
  settings.missingAudio.reset();
  auto assets = Assets::create(root.logger(), root.random(), root.logMap(), settings, (*rootAssets)->assetSources());
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  CacheFixture fixture{std::move(*assets), {}, {}, {}, HashSeed};
  for (std::size_t i = 0; i != count; ++i) {
    auto path = (*files)[i];
    auto image = fixture.assets->image(AssetPath(path));
    if (!image)
      return std::unexpected(std::move(image.error()));
    auto bytes = fixture.assets->bytes(path);
    if (!bytes)
      return std::unexpected(std::move(bytes.error()));
    if (!*image || (*image)->empty() || !*bytes || (*bytes)->empty())
      return std::unexpected(StarException("Mixed cache PNG/bytes fixture is empty"));
    auto content = mix(hashBytes(**bytes), (static_cast<std::uint64_t>((*image)->width()) << 32) | (*image)->height());
    fixture.content.push_back(content);
    fixture.checksum = mix(fixture.checksum, content);
    fixture.images.emplace_back(path);
    fixture.paths.push_back(std::move(path));
  }
  fixture.assets->clearCache();
  session.cacheFixtures.emplace(count, std::make_unique<CacheFixture>(fixture));
  return fixture;
}

struct CacheHandles {
  std::vector<std::shared_ptr<Image const>> images;
  std::vector<std::shared_ptr<ByteArray const>> bytes;
};

std::expected<void, StarException> cacheGetters(CacheFixture& fixture, CacheHandles& handles) {
  for (std::size_t i = 0; i != fixture.paths.size(); ++i) {
    auto image = fixture.assets->image(fixture.images[i]);
    if (!image)
      return std::unexpected(std::move(image.error()));
    auto bytes = fixture.assets->bytes(fixture.paths[i]);
    if (!bytes)
      return std::unexpected(std::move(bytes.error()));
    handles.images[i] = std::move(*image);
    handles.bytes[i] = std::move(*bytes);
  }
  return {};
}

bool validateCache(CacheFixture const& fixture, CacheHandles const& handles) {
  for (std::size_t i = 0; i != fixture.paths.size(); ++i) {
    if (!handles.images[i] || handles.images[i]->empty() || !handles.bytes[i] || handles.bytes[i]->empty())
      return false;
    auto content = mix(hashBytes(*handles.bytes[i]),
                       (static_cast<std::uint64_t>(handles.images[i]->width()) << 32) | handles.images[i]->height());
    if (content != fixture.content[i])
      return false;
  }
  return true;
}

void assetsWarmGetters(benchmark::State& state, BenchmarkSession& session) {
  auto count = static_cast<std::size_t>(state.range(0));
  auto fixture = prepareCache(session, count);
  if (!fixture) {
    session.fail(state, fixture.error());
    return;
  }
  CacheHandles retained{std::vector<std::shared_ptr<Image const>>(count), std::vector<std::shared_ptr<ByteArray const>>(count)};
  auto warmed = cacheGetters(*fixture, retained);
  if (!warmed || !validateCache(*fixture, retained)) {
    if (!warmed)
      session.fail(state, warmed.error());
    else
      session.fail(state, "Real warm image/bytes fixture failed content validation");
    return;
  }
  CacheHandles output{std::vector<std::shared_ptr<Image const>>(count), std::vector<std::shared_ptr<ByteArray const>>(count)};
  label(state, "warm Assets image + bytes getters; returned handles retained/replaced", fixture->checksum);
  for (auto _ : state) {
    (void)_;
    auto result = cacheGetters(*fixture, output);
    benchmark::ClobberMemory();
    state.PauseTiming();
    if (!result || !validateCache(*fixture, output)) {
      if (!result)
        session.fail(state, result.error());
      else
        session.fail(state, "Timed Assets getters changed real image/bytes content");
      break;
    }
    state.ResumeTiming();
  }
  state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(count * 2));
}

void assetsMixedCleanup(benchmark::State& state, BenchmarkSession& session) {
  auto count = static_cast<std::size_t>(state.range(0));
  auto fixture = prepareCache(session, count);
  if (!fixture) {
    session.fail(state, fixture.error());
    return;
  }
  CacheHandles handles{std::vector<std::shared_ptr<Image const>>(count), std::vector<std::shared_ptr<ByteArray const>>(count)};
  std::vector<std::weak_ptr<Image const>> images(count);
  std::vector<std::weak_ptr<ByteArray const>> bytes(count);
  auto prepare = [&]() -> std::expected<void, StarException> {
    for (std::size_t i = 0; i != count; ++i) {
      handles.images[i].reset();
      handles.bytes[i].reset();
    }
    fixture->assets->clearCache();
    auto loaded = cacheGetters(*fixture, handles);
    if (!loaded)
      return loaded;
    if (!validateCache(*fixture, handles))
      return std::unexpected(StarException("Mixed cache setup changed real PNG/bytes content"));
    // Include a third real asset type, released before the cleanup pass.
    auto json = fixture->assets->json("/player.config");
    if (!json)
      return std::unexpected(std::move(json.error()));
    if (!json->isType(Json::Type::Object))
      return std::unexpected(StarException("Real player configuration fixture is not an object"));
    for (std::size_t i = 0; i != count; ++i) {
      images[i] = handles.images[i];
      bytes[i] = handles.bytes[i];
      if (i % 2 != 0) {
        handles.images[i].reset();
        handles.bytes[i].reset();
      }
    }
    return {};
  };
  auto validate = [&]() {
    for (std::size_t i = 0; i != count; ++i) {
      bool expired = i % 2 != 0;
      if (images[i].expired() != expired || bytes[i].expired() != expired)
        return false;
    }
    return true;
  };
  auto checked = prepare();
  if (!checked) {
    session.fail(state, checked.error());
    return;
  }
  fixture->assets->cleanup();
  if (!validate()) {
    session.fail(state, "Mixed cache must evict every released PNG/bytes handle and retain every live handle");
    return;
  }
  label(state, "Assets cleanup only; expired image/bytes + JSON; half image/bytes resources externally retained", fixture->checksum);
  for (auto _ : state) {
    (void)_;
    state.PauseTiming();
    auto prepared = prepare();
    if (!prepared) {
      session.fail(state, prepared.error());
      break;
    }
    state.ResumeTiming();
    fixture->assets->cleanup();
    benchmark::ClobberMemory();
    state.PauseTiming();
    if (!validate()) {
      session.fail(state, "Timed cleanup did not preserve the mixed live/expired resource boundary");
      break;
    }
    state.ResumeTiming();
  }
  state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(count * 2 + 1));
}

}

int main(int argc, char** argv) {
  auto nativeRuntime = NativeRuntime::create();
  if (!nativeRuntime) {
    std::fprintf(stderr, "%s\n", nativeRuntime.error().what());
    return 1;
  }
  benchmark::Initialize(&argc, argv);
  BenchmarkLifetime benchmarkLifetime;
  auto loader = RootLoader::create({{}, {}, {}, LogLevel::Error, true, {}});
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
  if (auto storage = requirePrivateStorage(settings.storageDirectory); !storage) {
    std::fprintf(stderr, "%s\n", storage.error().what());
    return 1;
  }
  settings.runtimeConfigFile.reset();
  settings.logFile.reset();
  auto root = Root::create(std::move(settings));
  if (!root) {
    std::fprintf(stderr, "%s\n", root.error().what());
    return 1;
  }
  BenchmarkSession session{**root};
  benchmark::RegisterBenchmark("Ownership/DataStreamBuffer/SerializeReadback", [&](benchmark::State& state) {
    streamSerializeReadback(state, session);
  })->Arg(64)->Arg(4096)->UseRealTime();
  benchmark::RegisterBenchmark("Ownership/ItemBag/StackAndMatch", [&](benchmark::State& state) {
    itemStackAndMatch(state, session);
  })->Arg(16)->Arg(512)->UseRealTime();
  benchmark::RegisterBenchmark("Ownership/Audio/WavCloneSeekReadReclaim", [&](benchmark::State& state) {
    audioCloneSeekRead(state, session, false);
  })->Args({8, 4096})->Args({128, 262144})->UseRealTime();
  benchmark::RegisterBenchmark("Ownership/Audio/OggCloneSeekReadReclaim", [&](benchmark::State& state) {
    audioCloneSeekRead(state, session, true);
  })->Args({8, 0})->Args({128, 0})->UseRealTime();
  benchmark::RegisterBenchmark("Ownership/Assets/WarmImageBytesGetters", [&](benchmark::State& state) {
    assetsWarmGetters(state, session);
  })->Arg(8)->Arg(128)->UseRealTime();
  benchmark::RegisterBenchmark("Ownership/Assets/MixedExpiredCleanup", [&](benchmark::State& state) {
    assetsMixedCleanup(state, session);
  })->Arg(8)->Arg(128)->UseRealTime();
  CheckedReporter reporter;
  auto matched = benchmark::RunSpecifiedBenchmarks(&reporter);
  benchmark::ClearRegisteredBenchmarks();
  return session.failed || reporter.failed || matched == 0 ? 1 : 0;
}

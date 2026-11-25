#include <cstdio>
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
import star.mixer;
import star.native_runtime;
import star.benchmark_runner;
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
    auto decoded = DataStreamBuffer::deserialize<SmallRecord>(bytes->span());
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
  if (!(*database)->has_item("perfectlygenericitem"))
    return std::unexpected(StarException("Real perfectlygenericitem fixture is absent"));
  ItemFixture fixture;
  for (unsigned variant = 0; variant != 5; ++variant) {
    auto descriptor = ItemDescriptor::create("perfectlygenericitem", 8, JsonObject{{"ownershipVariant", variant}});
    if (!descriptor)
      return std::unexpected(std::move(descriptor.error()));
    auto item = (*database)->item(*descriptor);
    if (!item)
      return std::unexpected(std::move(item.error()));
    if (!*item || (*item)->name() != descriptor->name() || (*item)->count() != 8 || (*item)->max_stack() < 64)
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
    auto set = bag.set_item(i, std::move(*item));
    if (!set)
      return std::unexpected(std::move(set.error()));
  }
  auto incoming = fixture.prototypes[3]->clone();
  if (!incoming)
    return std::unexpected(std::move(incoming.error()));
  if ((*incoming)->set_count(1) != 0)
    return std::unexpected(StarException("Incoming item fixture could not hold one item"));
  return BagBatch{std::move(bag), std::move(*incoming)};
}

std::expected<std::array<std::uint64_t, 5>, StarException> bagBatch(BagBatch& batch, ItemFixture const& fixture) {
  // These are ownership-transfer APIs in both revisions. The benchmark never
  // calls Item comparison or the query-only ItemBag pointer APIs directly.
  auto leftover = batch.bag.stack_items(std::move(batch.incoming));
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
    if (!item || item->count() == 0 || item->count() > item->max_stack())
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
  label(state, "perfectlygenericitem, four parameter variants; stack_items + five exact available scans", checksum);
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

std::int16_t wave_sample(std::size_t frame, unsigned channel) {
  auto phase = (frame * 997 + static_cast<std::size_t>(channel) * 7919) % 60001;
  return static_cast<std::int16_t>(static_cast<std::int32_t>(phase) - 30000);
}

ByteArray make_wave(std::size_t frames, unsigned channels = 1, unsigned sample_rate = 22050) {
  ByteArray bytes;
  auto text = [&](std::string_view value) { bytes.append(std::span(value).first(4)); };
  auto le = [&](std::uint32_t value, unsigned width) {
    for (unsigned i = 0; i != width; ++i)
      bytes.push_back(static_cast<char>((value >> (8 * i)) & 255));
  };
  auto data_size = static_cast<std::uint32_t>(frames * channels * 2);
  text("RIFF");
  le(36 + data_size, 4);
  text("WAVE");
  text("fmt ");
  le(16, 4);
  le(1, 2);
  le(channels, 2);
  le(sample_rate, 4);
  le(sample_rate * channels * 2, 4);
  le(channels * 2, 2);
  le(16, 2);
  text("data");
  le(data_size, 4);
  for (std::size_t frame = 0; frame != frames; ++frame)
    for (unsigned channel = 0; channel != channels; ++channel)
      le(static_cast<std::uint16_t>(wave_sample(frame, channel)), 2);
  return bytes;
}

struct AudioFixture {
  Audio source;
  std::uint64_t frames;
  std::string name;
};

std::expected<AudioFixture, StarException> prepare_audio(Root& root, bool ogg, std::size_t frames) {
  std::expected<Audio, StarException> audio = [&]() -> std::expected<Audio, StarException> {
    if (!ogg)
      return Audio::create(std::make_shared<Buffer>(make_wave(frames)), "ownership-generated-pcm.wav");
    auto assets = root.assets();
    if (!assets)
      return std::unexpected(std::move(assets.error()));
    auto files = (*assets)->scan("/sfx/", ".ogg");
    if (!files)
      return std::unexpected(std::move(files.error()));
    std::ranges::sort(*files);
    if (files->empty())
      return std::unexpected(StarException("Real /sfx/ Ogg fixture catalog is empty"));
    auto device = (*assets)->open_file((*files)[0]);
    if (!device)
      return std::unexpected(std::move(device.error()));
    return Audio::create(std::move(*device), (*files)[0]);
  }();
  if (!audio)
    return std::unexpected(std::move(audio.error()));
  auto samples = audio->total_samples();
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

std::expected<void, StarException> audio_batch(AudioFixture& fixture, AudioOutput& output, bool clone) {
  auto width = static_cast<std::size_t>(64) * fixture.source.channels();
  for (std::size_t i = 0; i != output.counts.size(); ++i) {
    auto position = (i * 97) % (fixture.frames - 64);
    auto read_one = [&](Audio& audio) -> std::expected<void, StarException> {
      auto seek = audio.seek_sample(position);
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
      if (auto read = read_one(*cursor); !read)
        return read;
      // The independent cursor/decoder is reclaimed inside the timed batch.
    } else if (auto read = read_one(fixture.source); !read) {
      return read;
    }
  }
  return {};
}

void audio_clone_seek_read(benchmark::State& state, BenchmarkSession& session, bool ogg) {
  auto count = static_cast<std::size_t>(state.range(0));
  auto frames = static_cast<std::size_t>(state.range(1));
  auto fixture = prepare_audio(session.root, ogg, frames);
  if (!fixture) {
    session.fail(state, fixture.error());
    return;
  }
  auto width = static_cast<std::size_t>(64) * fixture->source.channels();
  AudioOutput expected{.samples = std::vector<std::int16_t>(count * width), .counts = std::vector<std::size_t>(count)};
  auto checked = audio_batch(*fixture, expected, false);
  auto reset = fixture->source.seek_sample(0);
  if (!checked || !reset || !std::ranges::all_of(expected.counts, [&](std::size_t count) { return count == width; })
      || std::ranges::all_of(expected.samples, [](std::int16_t sample) { return sample == 0; })) {
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
    auto result = audio_batch(*fixture, output, true);
    benchmark::ClobberMemory();
    state.PauseTiming();
    auto position = fixture->source.current_sample();
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

std::uint64_t pcm_fingerprint(std::span<std::int16_t const> samples) {
  auto checksum = HashSeed;
  for (auto sample : samples)
    checksum = mix(checksum, static_cast<std::uint16_t>(sample));
  return checksum;
}

std::vector<std::int16_t> wave_pcm(std::size_t frames, unsigned channels) {
  std::vector<std::int16_t> samples(frames * channels);
  for (std::size_t frame = 0; frame != frames; ++frame)
    for (unsigned channel = 0; channel != channels; ++channel)
      samples[frame * channels + channel] = wave_sample(frame, channel);
  return samples;
}

// Eight rational-time probes per destination frame. Split whole-frame time
// from fractional phase instead of using the implementation's chunk indices.
std::vector<std::int16_t> resample_reference(unsigned source_channels, unsigned source_rate,
                                             unsigned destination_channels, unsigned destination_rate,
                                             std::size_t frames) {
  std::vector<std::int16_t> expected(frames * destination_channels);
  for (std::size_t frame = 0; frame != frames; ++frame) {
    auto whole_frame = frame * source_rate / destination_rate;
    auto phase = frame * source_rate % destination_rate;
    for (unsigned channel = 0; channel != destination_channels; ++channel) {
      auto source_channel = std::min(channel, source_channels - 1);
      std::int32_t sum = 0;
      for (unsigned tap = 0; tap != 8; ++tap) {
        auto source_frame = whole_frame + (phase * 8 + tap * source_rate) / (destination_rate * 8);
        sum += wave_sample(source_frame, source_channel);
      }
      expected[frame * destination_channels + channel] = static_cast<std::int16_t>(sum / 8);
    }
  }
  return expected;
}

void audio_resample(benchmark::State& state, BenchmarkSession& session) {
  auto source_channels = static_cast<unsigned>(state.range(0));
  auto destination_channels = static_cast<unsigned>(state.range(1));
  auto source_rate = static_cast<unsigned>(state.range(2));
  auto destination_rate = static_cast<unsigned>(state.range(3));
  constexpr std::size_t output_frames = 512;
  constexpr std::size_t source_frames = 4096;
  auto bytes = make_wave(source_frames, source_channels, source_rate);
  auto fixture_checksum = hashBytes(bytes);
  auto source = Audio::create(std::make_shared<Buffer>(std::move(bytes)), "ownership-resample.wav");
  if (!source) {
    session.fail(state, source.error());
    return;
  }
  auto total_frames = source->total_samples();
  if (!total_frames || *total_frames != source_frames || source->channels() != source_channels
      || source->sample_rate() != source_rate || source->compressed()) {
    if (!total_frames)
      session.fail(state, total_frames.error());
    else
      session.fail(state, "Resampling WAV metadata differs from the deterministic fixture");
    return;
  }
  auto expected =
    resample_reference(source_channels, source_rate, destination_channels, destination_rate, output_frames);
  auto expected_cursor = (output_frames * source_rate + destination_rate - 1) / destination_rate;
  std::vector<std::int16_t> output(expected.size());
  auto run_batch = [&]() -> std::expected<std::size_t, StarException> {
    auto reset = source->seek_sample(0);
    if (!reset)
      return std::unexpected(std::move(reset.error()));
    return source->resample(destination_channels, destination_rate, output);
  };
  auto validate = [&](std::expected<std::size_t, StarException> const& result) {
    auto cursor = source->current_sample();
    if (!result || !cursor || *result != expected.size() || *cursor != expected_cursor || output != expected) {
      if (!result)
        session.fail(state, result.error());
      else if (!cursor)
        session.fail(state, cursor.error());
      else
        session.fail(state, "Resampling differs from the complete rational PCM/count/frame-cursor oracle");
      return false;
    }
    return true;
  };
  if (!validate(run_batch()))
    return;
  label(state,
        std::format("seek0 + resample 512 output frames; {}ch {}Hz -> {}ch {}Hz; fixture={:016x}; tolerance=0 LSB",
                    source_channels, source_rate, destination_channels, destination_rate, fixture_checksum),
        pcm_fingerprint(output));
  for (auto _ : state) {
    (void)_;
    auto result = run_batch();
    benchmark::ClobberMemory();
    state.PauseTiming();
    if (!validate(result))
      break;
    state.ResumeTiming();
  }
  state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(output_frames));
}

std::expected<void, StarException> mix_callbacks(Mixer& mixer, std::span<std::int16_t> output) {
  constexpr std::size_t callback_frames = 512;
  constexpr unsigned channels = 2;
  for (std::size_t callback = 0; callback != 8; ++callback) {
    auto result =
      mixer.read(output.subspan(callback * callback_frames * channels, callback_frames * channels), callback_frames);
    if (!result)
      return result;
  }
  return {};
}

void audio_mix(benchmark::State& state, BenchmarkSession& session) {
  auto voice_count = static_cast<std::size_t>(state.range(0));
  constexpr std::size_t frames = 4096;
  constexpr unsigned channels = 2;
  constexpr unsigned sample_rate = 44100;
  auto input = wave_pcm(frames, channels);
  auto expected = input;
  for (auto& sample : expected)
    sample = static_cast<std::int16_t>(
      std::clamp(static_cast<std::int32_t>(sample) * static_cast<std::int32_t>(voice_count), -32767, 32767));
  Mixer mixer(session.root.logger(), sample_rate, channels);
  constexpr auto groups =
    std::array{MixerGroup::Effects, MixerGroup::Music, MixerGroup::Cinematic, MixerGroup::Instruments};
  for (auto group : groups)
    mixer.set_group_volume(group, 1.0f);
  std::vector<std::shared_ptr<AudioInstance>> voices;
  std::vector<std::weak_ptr<AudioInstance>> weak_voices;
  {
    auto source =
      Audio::create(std::make_shared<Buffer>(make_wave(frames, channels, sample_rate)), "ownership-mix.wav");
    if (!source) {
      session.fail(state, source.error());
      return;
    }
    for (std::size_t voice_index = 0; voice_index != voice_count; ++voice_index) {
      auto voice = AudioInstance::create(*source);
      if (!voice) {
        session.fail(state, voice.error());
        return;
      }
      (*voice)->set_loops(-1);
      (*voice)->set_volume(1.0f);
      (*voice)->set_pitch_multiplier(1.0f);
      (*voice)->set_mixer_group(groups[voice_index % groups.size()]);
      auto played = mixer.play(*voice);
      if (!played) {
        session.fail(state, played.error());
        return;
      }
      weak_voices.push_back(*voice);
      voices.push_back(std::move(*voice));
    }
    auto cursor = source->current_sample();
    if (!cursor || *cursor != 0) {
      if (!cursor)
        session.fail(state, cursor.error());
      else
        session.fail(state, "AudioInstance cloning moved the original source cursor");
      return;
    }
    // Destroy the original Audio and input device before any playback.
  }
  std::vector<std::int16_t> output(input.size());
  auto validate = [&](std::expected<void, StarException> const& result) {
    if (!result) {
      session.fail(state, result.error());
      return false;
    }
    if (output != expected) {
      session.fail(state, "Mixed output differs from independently clamped voice-count times fixture PCM");
      return false;
    }
    for (auto const& voice : voices) {
      auto cursor = voice->current_time();
      if (!cursor || *cursor != static_cast<double>(frames) / sample_rate || voice->finished()
          || voice->loops() != -1) {
        if (!cursor)
          session.fail(state, cursor.error());
        else
          session.fail(state, "Looping voice lost its independent cursor, lifetime, or active state");
        return false;
      }
    }
    return true;
  };
  // Warm through one whole clip, so every measured batch includes an EOF loop.
  if (!validate(mix_callbacks(mixer, output)))
    return;
  label(state,
        std::format("{} looping stereo voices; 8 callbacks x 512 output frames; gain/pitch/groups=1; fixture={:016x}; "
                    "tolerance=0 LSB",
                    voice_count, pcm_fingerprint(input)),
        pcm_fingerprint(output));
  for (auto _ : state) {
    (void)_;
    auto result = mix_callbacks(mixer, output);
    benchmark::ClobberMemory();
    state.PauseTiming();
    if (!validate(result))
      break;
    state.ResumeTiming();
  }
  state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(frames));
  // Outside the measured range, verify stop/finished and queue reclamation.
  for (auto const& voice : voices)
    voice->stop();
  auto stopped = mixer.read(std::span(output).first(512 * channels), 512);
  if (!stopped
      || !std::ranges::all_of(voices, [](std::shared_ptr<AudioInstance> const& voice) { return voice->finished(); })) {
    if (!stopped)
      session.fail(state, stopped.error());
    else
      session.fail(state, "Stopped voices were not marked finished");
    return;
  }
  auto reclaimed = mixer.update(0.0f);
  if (!reclaimed) {
    session.fail(state, reclaimed.error());
    return;
  }
  voices.clear();
  if (!std::ranges::all_of(weak_voices, [](std::weak_ptr<AudioInstance> const& voice) { return voice.expired(); }))
    session.fail(state, "Finished voices remained owned by the mixer after update");
}

std::vector<std::int16_t> effect_reference(std::span<std::int16_t const> input, std::size_t window, bool echo) {
  constexpr unsigned channels = 2;
  constexpr std::size_t delay_frames = 11025;
  std::vector<std::int16_t> expected(input.size());
  for (std::size_t frame = 0; frame != input.size() / channels; ++frame) {
    for (unsigned channel = 0; channel != channels; ++channel) {
      auto index = frame * channels + channel;
      if (echo) {
        // dry=3/4, wet=1/4; feed back the quantized processed output.
        auto delayed = frame < delay_frames ? 0 : expected[(frame - delay_frames) * channels + channel];
        expected[index] = static_cast<std::int16_t>((3 * static_cast<std::int32_t>(input[index]) + delayed) / 4);
      } else {
        // Independent finite convolution: zero-padded start, fixed divisor.
        std::int64_t sum = 0;
        for (std::size_t lag = 0; lag != window; ++lag)
          if (lag <= frame)
            sum += input[(frame - lag) * channels + channel];
        expected[index] = static_cast<std::int16_t>(sum / static_cast<std::int64_t>(window));
      }
    }
  }
  return expected;
}

std::expected<void, StarException> effect_cold_lifecycle(Mixer const& mixer, std::span<std::int16_t const> input,
                                                         std::span<std::int16_t> output, std::size_t window,
                                                         bool echo) {
  std::ranges::copy(input, output.begin());
  auto effect = echo ? mixer.echo(0.25f, 0.75f, 0.25f) : mixer.low_pass(window);
  constexpr std::size_t block_frames = 512;
  constexpr unsigned channels = 2;
  for (std::size_t offset = 0; offset != output.size(); offset += block_frames * channels) {
    auto result = effect(output.subspan(offset, block_frames * channels), block_frames, channels);
    if (!result)
      return result;
  }
  // The callable and all delay/filter storage are reclaimed before returning.
  return {};
}

void audio_effect_cold_lifecycle(benchmark::State& state, BenchmarkSession& session, bool echo) {
  auto window = echo ? std::size_t{0} : static_cast<std::size_t>(state.range(0));
  constexpr std::size_t frames = 64 * 512;
  auto input = wave_pcm(frames, 2);
  auto expected = effect_reference(input, window, echo);
  std::vector<std::int16_t> output(input.size());
  Mixer mixer(session.root.logger(), 44100, 2);
  auto tolerance = echo ? 0 : 1;
  auto validate = [&](std::expected<void, StarException> const& result) {
    if (!result) {
      session.fail(state, result.error());
      return false;
    }
    for (std::size_t index = 0; index != expected.size(); ++index) {
      if (std::abs(static_cast<int>(output[index]) - static_cast<int>(expected[index])) > tolerance) {
        session.fail(state,
                     "Effect output exceeded the independent full-output convolution/quantized-recurrence tolerance");
        return false;
      }
    }
    return true;
  };
  if (!validate(effect_cold_lifecycle(mixer, input, output, window, echo)))
    return;
  auto definition =
    echo ? std::string("echo delay=11025 frames dry=0.75 wet=0.25") : std::format("low-pass window={}", window);
  label(state,
        std::format("{}; cold lifecycle: buffer reset + factory + 64 stereo blocks x 512 frames + reclaim; "
                    "tolerance={} LSB; fixture={:016x}; reference={:016x}",
                    definition, tolerance, pcm_fingerprint(input), pcm_fingerprint(expected)),
        pcm_fingerprint(output));
  for (auto _ : state) {
    (void)_;
    auto result = effect_cold_lifecycle(mixer, input, output, window, echo);
    benchmark::ClobberMemory();
    state.PauseTiming();
    if (!validate(result))
      break;
    state.ResumeTiming();
  }
  state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(frames));
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
  settings.asset_time_to_live = -1.0f;
  settings.missing_image.reset();
  settings.missing_audio.reset();
  auto assets = Assets::create(root.logger(), root.random(), root.logMap(), settings, (*rootAssets)->asset_sources());
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
  fixture.assets->clear_cache();
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
    fixture->assets->clear_cache();
    auto loaded = cacheGetters(*fixture, handles);
    if (!loaded)
      return loaded;
    if (!validateCache(*fixture, handles))
      return std::unexpected(StarException("Mixed cache setup changed real PNG/bytes content"));
    // Include a third real asset type, released before the cleanup pass.
    auto json = fixture->assets->json("/player.config");
    if (!json)
      return std::unexpected(std::move(json.error()));
    if (!json->is_type(Json::Type::Object))
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
    if (!Star::print(*stderr, "{}\n", nativeRuntime.error().what()))
      return 1;
    return 1;
  }
  Star::BenchmarkLifetime benchmark_lifetime(argc, argv);
  auto loader = RootLoader::create({{}, {}, {}, LogLevel::Error, true, {}});
  if (!loader) {
    if (!Star::print(*stderr, "{}\n", loader.error().what()))
      return 1;
    return 1;
  }
  auto parsed = loader->command_parse_or_die(Star::native_arguments(argc, argv));
  if (!parsed) {
    if (!Star::print(*stderr, "{}\n", parsed.error().what()))
      return 1;
    return 1;
  }
  auto& settings = parsed->first;
  if (auto storage = requirePrivateStorage(settings.storageDirectory); !storage) {
    if (!Star::print(*stderr, "{}\n", storage.error().what()))
      return 1;
    return 1;
  }
  settings.runtimeConfigFile.reset();
  settings.logFile.reset();
  auto root = Root::create(std::move(settings));
  if (!root) {
    if (!Star::print(*stderr, "{}\n", root.error().what()))
      return 1;
    return 1;
  }
  BenchmarkSession session{**root};
  benchmark::RegisterBenchmark("Ownership/DataStreamBuffer/SerializeReadback", [&](benchmark::State& state) {
    streamSerializeReadback(state, session);
  })->Arg(64)->Arg(4096)->UseRealTime();
  benchmark::RegisterBenchmark("Ownership/ItemBag/StackAndMatch", [&](benchmark::State& state) {
    itemStackAndMatch(state, session);
  })->Arg(16)->Arg(512)->UseRealTime();
  benchmark::RegisterBenchmark("Ownership/Audio/WavCloneSeekReadReclaim",
                               [&](benchmark::State& state) { audio_clone_seek_read(state, session, false); })
    ->Args({8, 4096})
    ->Args({128, 262144})
    ->UseRealTime();
  benchmark::RegisterBenchmark("Ownership/Audio/OggCloneSeekReadReclaim",
                               [&](benchmark::State& state) { audio_clone_seek_read(state, session, true); })
    ->Args({8, 0})
    ->Args({128, 0})
    ->UseRealTime();
  benchmark::RegisterBenchmark("Ownership/Audio/Resample",
                               [&](benchmark::State& state) { audio_resample(state, session); })
    ->Args({1, 2, 22050, 44100})
    ->Args({2, 2, 22050, 44100})
    ->Args({2, 1, 44100, 22050})
    ->Args({2, 2, 44100, 44100})
    ->Args({1, 2, 44100, 44100})
    ->Args({2, 1, 44100, 44100})
    ->UseRealTime();
  benchmark::RegisterBenchmark("Ownership/Audio/Mix", [&](benchmark::State& state) { audio_mix(state, session); })
    ->Arg(1)
    ->Arg(16)
    ->Arg(64)
    ->UseRealTime();
  benchmark::RegisterBenchmark("Ownership/Audio/LowPass/ColdLifecycle",
                               [&](benchmark::State& state) { audio_effect_cold_lifecycle(state, session, false); })
    ->Arg(32)
    ->Arg(128)
    ->UseRealTime();
  benchmark::RegisterBenchmark("Ownership/Audio/Echo/ColdLifecycle", [&](benchmark::State& state) {
    audio_effect_cold_lifecycle(state, session, true);
  })->UseRealTime();
  benchmark::RegisterBenchmark("Ownership/Assets/WarmImageBytesGetters", [&](benchmark::State& state) {
    assetsWarmGetters(state, session);
  })->Arg(8)->Arg(128)->UseRealTime();
  benchmark::RegisterBenchmark("Ownership/Assets/MixedExpiredCleanup", [&](benchmark::State& state) {
    assetsMixedCleanup(state, session);
  })->Arg(8)->Arg(128)->UseRealTime();
  auto result = Star::run_registered_benchmarks();
  benchmark::ClearRegisteredBenchmarks();
  return session.failed || result != 0 ? 1 : 0;
}

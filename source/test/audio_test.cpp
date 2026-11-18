import std;
import star.audio;
import star.buffer;
import star.byte_array;
import star.format;
import star.logging;
import star.mixer;
import star.vector;

#include "gtest/gtest.h"

using namespace Star;

namespace {
std::expected<Audio, StarException> make_audio(std::span<std::int16_t const> samples, unsigned channels = 1,
                                               unsigned sample_rate = 8) {
  ByteArray bytes;
  auto append_text = [&](std::string_view text) { bytes.append(std::span(text)); };
  auto append_le = [&](std::uint32_t value, unsigned width) {
    for (unsigned i = 0; i != width; ++i)
      bytes.push_back(static_cast<char>(value >> (8 * i)));
  };
  auto data_size = static_cast<std::uint32_t>(samples.size_bytes());
  bytes.reserve(44 + data_size);
  append_text("RIFF");
  append_le(36 + data_size, 4);
  append_text("WAVEfmt ");
  append_le(16, 4);
  append_le(1, 2);
  append_le(channels, 2);
  append_le(sample_rate, 4);
  append_le(sample_rate * channels * 2, 4);
  append_le(channels * 2, 2);
  append_le(16, 2);
  append_text("data");
  append_le(data_size, 4);
  for (auto sample : samples)
    append_le(std::bit_cast<std::uint16_t>(sample), 2);
  return Audio::create(std::make_shared<Buffer>(std::move(bytes)), "audio-regression.wav");
}
}// namespace

TEST(AudioTest, PartialStereoReadAndCloneRetainScalarCursor) {
  std::array<std::int16_t, 6> input{100, 200, 300, 400, 500, 600};
  auto audio = make_audio(input, 2);
  ASSERT_TRUE(audio);
  std::array<std::int16_t, 3> first{};
  ASSERT_EQ(audio->read(first), 3u);
  EXPECT_EQ(first, (std::array<std::int16_t, 3>{100, 200, 300}));
  EXPECT_EQ(audio->current_sample(), 1u);
  auto cloned = audio->clone();
  ASSERT_TRUE(cloned);
  std::array<std::int16_t, 3> tail{};
  ASSERT_EQ(cloned->read(tail), 3u);
  EXPECT_EQ(tail, (std::array<std::int16_t, 3>{400, 500, 600}));
  EXPECT_EQ(audio->current_sample(), 1u);
  std::array<std::int16_t, 1> next{};
  ASSERT_EQ(audio->read(next), 1u);
  EXPECT_EQ(next[0], 400);
  ASSERT_TRUE(audio->seek_sample(2));
  ASSERT_EQ(audio->read(tail), 2u);
  EXPECT_EQ(tail[0], 500);
  EXPECT_EQ(tail[1], 600);
  EXPECT_EQ(audio->read(tail), 0u);
}

TEST(AudioTest, RejectsNonfiniteSeekAndVelocityWithoutConsumingInput) {
  std::array<std::int16_t, 16> input{};
  auto audio = make_audio(input);
  ASSERT_TRUE(audio);
  ASSERT_TRUE(audio->seek_sample(1));
  std::array<std::int16_t, 4> output{};
  for (auto value : {std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity(),
                     -std::numeric_limits<double>::infinity()}) {
    EXPECT_FALSE(audio->seek_time(value));
    EXPECT_FALSE(audio->resample(1, 8, output, value));
    EXPECT_EQ(audio->current_sample(), 1u);
  }
  EXPECT_FALSE(audio->resample(1, 8, output, 0));
  EXPECT_FALSE(audio->resample(1, 8, output, -1));
  EXPECT_EQ(audio->current_sample(), 1u);
}

TEST(MixerTest, EmptyInfiniteLoopFinishesWithoutBlockingCallback) {
  auto audio = make_audio({});
  ASSERT_TRUE(audio);
  auto instance = AudioInstance::create(*audio);
  ASSERT_TRUE(instance);
  (*instance)->set_loops(-1);
  Logger logger;
  logger.remove_stdout_sink();
  Mixer mixer(logger, 8, 1);
  ASSERT_TRUE(mixer.play(*instance));
  std::array<std::int16_t, 8> output;
  output.fill(123);
  ASSERT_TRUE(mixer.read(output, output.size()));
  EXPECT_EQ(output, (std::array<std::int16_t, 8>{}));
  EXPECT_TRUE((*instance)->finished());
}

TEST(MixerTest, InstanceAmplitudeIsAppliedOnce) {
  std::array<std::int16_t, 32> input;
  input.fill(12000);
  auto audio = make_audio(input);
  ASSERT_TRUE(audio);
  auto instance = AudioInstance::create(*audio);
  ASSERT_TRUE(instance);
  (*instance)->set_volume(0.5f);
  Logger logger;
  logger.remove_stdout_sink();
  Mixer mixer(logger, 8, 1);
  ASSERT_TRUE(mixer.play(*instance));
  std::array<std::int16_t, 4> output{};
  ASSERT_TRUE(mixer.read(output, output.size()));
  EXPECT_EQ(output, (std::array<std::int16_t, 4>{6000, 6000, 6000, 6000}));
  mixer.set_volume(0.5f, 0);
  mixer.set_group_volume(MixerGroup::Effects, 0.5f);
  ASSERT_TRUE(mixer.read(output, output.size()));
  EXPECT_EQ(output, (std::array<std::int16_t, 4>{3000, 2625, 2250, 1875}));
  ASSERT_TRUE(mixer.read(output, output.size()));
  EXPECT_EQ(output, (std::array<std::int16_t, 4>{1500, 1500, 1500, 1500}));
}

TEST(MixerTest, PitchRampAdvancesWithoutAccumulatingGlobalSpeed) {
  std::array<std::int16_t, 64> input;
  input.fill(1000);
  for (auto const& [speed, consumed] : std::array<std::pair<float, unsigned>, 2>{{{1, 14}, {2, 26}}}) {
    auto audio = make_audio(input);
    ASSERT_TRUE(audio);
    auto instance = AudioInstance::create(*audio);
    ASSERT_TRUE(instance);
    (*instance)->set_pitch_multiplier(2, 1);
    Logger logger;
    logger.remove_stdout_sink();
    Mixer mixer(logger, 8, 1);
    mixer.set_speed(speed);
    ASSERT_TRUE(mixer.play(*instance));
    std::array<std::int16_t, 2> output{};
    for (unsigned i = 0; i != 4; ++i)
      ASSERT_TRUE(mixer.read(output, output.size()));
    auto time = (*instance)->current_time();
    ASSERT_TRUE(time);
    EXPECT_DOUBLE_EQ(*time, consumed / 8.0);
    EXPECT_FALSE((*instance)->finished());
  }
}

TEST(MixerTest, ZeroSpeedPausesEffectsButNotMusicAndCanResume) {
  std::array<std::int16_t, 32> input;
  input.fill(1000);
  auto audio = make_audio(input);
  ASSERT_TRUE(audio);
  auto effect = AudioInstance::create(*audio);
  auto music = AudioInstance::create(*audio);
  ASSERT_TRUE(effect);
  ASSERT_TRUE(music);
  (*music)->set_mixer_group(MixerGroup::Music);
  Logger logger;
  logger.remove_stdout_sink();
  Mixer mixer(logger, 8, 1);
  ASSERT_TRUE(mixer.play(*effect));
  ASSERT_TRUE(mixer.play(*music));
  mixer.set_speed(0);
  std::array<std::int16_t, 4> output{};
  ASSERT_TRUE(mixer.read(output, output.size()));
  EXPECT_EQ(output, (std::array<std::int16_t, 4>{1000, 1000, 1000, 1000}));
  EXPECT_EQ((*effect)->current_time(), 0.0);
  EXPECT_EQ((*music)->current_time(), 0.5);
  EXPECT_FALSE((*effect)->finished());
  mixer.set_speed(1);
  ASSERT_TRUE(mixer.read(output, output.size()));
  EXPECT_EQ(output, (std::array<std::int16_t, 4>{2000, 2000, 2000, 2000}));
  EXPECT_EQ((*effect)->current_time(), 0.5);
  EXPECT_EQ((*music)->current_time(), 1.0);
}

TEST(MixerTest, BatchedPositionAttenuationControlsEachChannel) {
  std::array<std::int16_t, 32> input;
  input.fill(1000);
  auto audio = make_audio(input);
  ASSERT_TRUE(audio);
  auto instance = AudioInstance::create(*audio);
  ASSERT_TRUE(instance);
  (*instance)->set_position(Vec2F(3, 4));
  (*instance)->set_range_multiplier(2);
  Logger logger;
  logger.remove_stdout_sink();
  Mixer mixer(logger, 8, 2);
  ASSERT_TRUE(mixer.play(*instance));
  ASSERT_TRUE(mixer.update(
    0, [](Vec2F position, float range_multiplier, std::span<float> attenuation) -> std::expected<void, StarException> {
      EXPECT_FLOAT_EQ(position[0], 3);
      EXPECT_FLOAT_EQ(position[1], 4);
      EXPECT_FLOAT_EQ(range_multiplier, 2);
      if (attenuation.size() != 2)
        return std::unexpected(AudioException("Unexpected attenuation channel count"));
      attenuation[0] = 0;
      attenuation[1] = 0.5f;
      return {};
    }));
  std::array<std::int16_t, 4> output{};
  ASSERT_TRUE(mixer.read(output, 2));
  EXPECT_EQ(output, (std::array<std::int16_t, 4>{1000, 500, 1000, 500}));
}

TEST(MixerTest, FiltersPreserveHistoryAcrossBlockBoundaries) {
  constexpr unsigned channels = 2;
  constexpr std::size_t frames = 24;
  constexpr std::size_t window = 5;
  std::array<std::int16_t, frames * channels> input{};
  for (std::size_t i = 0; i != input.size(); ++i)
    input[i] = static_cast<std::int16_t>(static_cast<int>((i * 7919) % 20001) - 10000);
  Logger logger;
  logger.remove_stdout_sink();
  Mixer mixer(logger, 12, channels);
  for (bool echo : {false, true}) {
    auto whole = input;
    auto split = input;
    auto expected = input;
    if (echo) {
      for (std::size_t frame = 0; frame != frames; ++frame)
        for (unsigned channel = 0; channel != channels; ++channel) {
          auto index = frame * channels + channel;
          expected[index] = static_cast<std::int16_t>(
            input[index] * 0.5f + (frame < 3 ? 0.0f : expected[(frame - 3) * channels + channel] * 0.25f));
        }
    } else {
      for (std::size_t frame = 0; frame != frames; ++frame)
        for (unsigned channel = 0; channel != channels; ++channel) {
          std::int64_t sum = 0;
          for (std::size_t back = 0; back != window && back <= frame; ++back)
            sum += input[(frame - back) * channels + channel];
          expected[frame * channels + channel] = static_cast<std::int16_t>(sum / static_cast<std::int64_t>(window));
        }
    }
    auto full_filter = echo ? mixer.echo(0.25f, 0.5f, 0.25f) : mixer.low_pass(window);
    auto split_filter = echo ? mixer.echo(0.25f, 0.5f, 0.25f) : mixer.low_pass(window);
    ASSERT_TRUE(full_filter(whole, frames, channels));
    std::size_t offset = 0;
    for (auto count : {1u, 7u, 3u, 13u}) {
      ASSERT_TRUE(split_filter(std::span(split).subspan(offset * channels, count * channels), count, channels));
      offset += count;
    }
    EXPECT_EQ(whole, split);
    for (std::size_t i = 0; i != expected.size(); ++i)
      EXPECT_LE(std::abs(static_cast<int>(whole[i]) - expected[i]), echo ? 0 : 1) << "sample " << i;
  }
}

TEST(AudioTest, ResamplingShortInputPreservesTailAndChannelMapping) {
  auto mono = make_audio(std::array<std::int16_t, 2>{100, 300});
  ASSERT_TRUE(mono);
  std::array<std::int16_t, 14> output;
  output.fill(-777);
  auto count = mono->resample(2, 16, std::span(output).subspan(2, 10));
  ASSERT_EQ(count, 8u);
  EXPECT_EQ(output,
            (std::array<std::int16_t, 14>{-777, -777, 100, 100, 150, 150, 300, 300, 300, 300, -777, -777, -777, -777}));
  EXPECT_EQ(mono->current_sample(), 2u);
  EXPECT_EQ(mono->read(output), 0u);

  auto stereo = make_audio(std::array<std::int16_t, 6>{10, 110, 20, 120, 30, 130}, 2);
  ASSERT_TRUE(stereo);
  output.fill(-777);
  ASSERT_EQ(stereo->resample(1, 8, std::span(output).first(5)), 3u);
  EXPECT_EQ(output[0], 10);
  EXPECT_EQ(output[1], 20);
  EXPECT_EQ(output[2], 30);
  EXPECT_TRUE(std::ranges::all_of(std::span(output).subspan(3), [](std::int16_t sample) { return sample == -777; }));
}

TEST(MixerTest, FailedEffectDoesNotPublishItsScratchOutput) {
  std::array<std::int16_t, 32> input;
  input.fill(1000);
  auto audio = make_audio(input);
  ASSERT_TRUE(audio);
  auto instance = AudioInstance::create(*audio);
  ASSERT_TRUE(instance);
  Logger logger;
  logger.remove_stdout_sink();
  Mixer mixer(logger, 8, 1);
  ASSERT_TRUE(mixer.play(*instance));
  ASSERT_TRUE(mixer.add_effect(
    "broken",
    [](std::span<std::int16_t> buffer, std::size_t, unsigned) -> std::expected<void, StarException> {
      std::ranges::fill(buffer, 777);
      return std::unexpected(AudioException("Expected filter failure"));
    },
    0));
  std::array<std::int16_t, 4> output{};
  EXPECT_FALSE(mixer.read(output, output.size()));
  EXPECT_EQ(output, (std::array<std::int16_t, 4>{1000, 1000, 1000, 1000}));
  EXPECT_EQ((*instance)->current_time(), 0.5);
}

TEST(MixerTest, EffectCanRemoveItselfAndChangeNextBufferVolume) {
  std::array<std::int16_t, 32> input;
  input.fill(1000);
  auto audio = make_audio(input);
  ASSERT_TRUE(audio);
  auto instance = AudioInstance::create(*audio);
  ASSERT_TRUE(instance);
  Logger logger;
  logger.remove_stdout_sink();
  Mixer mixer(logger, 8, 1);
  ASSERT_TRUE(mixer.play(*instance));
  ASSERT_TRUE(mixer.add_effect(
    "self",
    [&mixer](std::span<std::int16_t>, std::size_t, unsigned) -> std::expected<void, StarException> {
      mixer.remove_effect("self", 0);
      mixer.set_volume(0.5f, 0);
      return {};
    },
    0));
  std::array<std::int16_t, 4> output{};
  ASSERT_TRUE(mixer.read(output, output.size()));
  EXPECT_EQ(output, (std::array<std::int16_t, 4>{1000, 1000, 1000, 1000}));
  ASSERT_TRUE(mixer.read(output, output.size()));
  EXPECT_EQ(output, (std::array<std::int16_t, 4>{1000, 875, 750, 625}));
  ASSERT_TRUE(mixer.update(0));
  ASSERT_TRUE(mixer.read(output, output.size()));
  EXPECT_EQ(output, (std::array<std::int16_t, 4>{500, 500, 500, 500}));
}

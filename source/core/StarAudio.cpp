// Native Vorbis callbacks report errors through their datasource, never unwind.
#define OV_EXCLUDE_STATIC_CALLBACKS

#include <ctype.h>
#include "vorbis/codec.h"
#include "vorbis/vorbisfile.h"

import std;
import star.memory;
import star.audio;
import star.io_device;
import star.io_device_callbacks;
import star.string;
import star.buffer;
import star.format;
import star.byte_array;
import star.bytes;
import star.math_common;
namespace Star {

extern float const DefaultPerceptualRangeDb = 40.f;
extern float const DefaultPerceptualBoostRangeDb = 6.f;
// https://github.com/discord/perceptual
float perceptualToAmplitude(float perceptual, float normalizedMax, float range, float boostRange) {
  if (perceptual == 0.f) return 0.f;
  float dB = perceptual > normalizedMax
    ? ((perceptual - normalizedMax) / normalizedMax) * boostRange
    : (perceptual / normalizedMax) * range - range;
  return normalizedMax * std::pow(10.f, dB / 20.f);
}

float amplitudeToPerceptual(float amp, float normalizedMax, float range, float boostRange) {
  if (amp == 0.f) return 0.f;
  float const dB = 20.f * std::log10(amp / normalizedMax);
  float perceptual = dB > 0.f
    ? dB / boostRange + 1
    : (range + dB) / range;
  return normalizedMax * perceptual;
}

namespace {
  struct WaveData {
#ifdef STAR_STREAM_AUDIO
    std::shared_ptr<IODevice> stream;
#else
    mutable ExternalBuffer memory;
#endif
    std::shared_ptr<ByteArray const> bytes;
    unsigned channels;
    unsigned sampleRate;
    std::size_t dataSize;
    StreamOffset dataStart;

    IODevice& device() const {
#ifdef STAR_STREAM_AUDIO
      return *stream;
#else
      return memory;
#endif
    }
  };

  template <typename T>
  [[nodiscard]] std::expected<T, StarException> readLEType(IODevice& device) {
    T value;
    auto read = device.read_full({reinterpret_cast<char*>(&value), sizeof(value)});
    if (!read)
      return std::unexpected(std::move(read.error()));
    fromByteOrder(ByteOrder::LittleEndian, reinterpret_cast<char*>(&value), sizeof(value));
    return value;
  }

  [[nodiscard]] std::expected<bool, StarException> isUncompressed(IODevice& device) {
    std::array<char, 12> signature{};
    auto previous = device.pos();
    if (!previous)
      return std::unexpected(std::move(previous.error()));
    auto seek = device.seek(0);
    if (!seek)
      return std::unexpected(std::move(seek.error()));
    auto read = device.read_full(signature);
    auto restore = device.seek(*previous);
    if (!read)
      return std::unexpected(std::move(read.error()));
    if (!restore)
      return std::unexpected(std::move(restore.error()));
    return std::memcmp(signature.data(), "RIFF", 4) == 0
        && std::memcmp(signature.data() + 8, "WAVE", 4) == 0;
  }

  [[nodiscard]] std::expected<WaveData, StarException> parseWav(std::shared_ptr<IODevice> device) {
    auto seek = device->seek(0);
    if (!seek)
      return std::unexpected(std::move(seek.error()));
    std::array<char, 4> signature{};
    auto read = device->read_full(signature);
    if (!read)
      return std::unexpected(std::move(read.error()));
    if (std::memcmp(signature.data(), "RIFF", 4) != 0)
      return std::unexpected(AudioException("Wav file has invalid RIFF magic bytes"));
    auto fileSize = readLEType<std::uint32_t>(*device);
    if (!fileSize)
      return std::unexpected(std::move(fileSize.error()));
    auto actualSize = device->size();
    if (!actualSize)
      return std::unexpected(std::move(actualSize.error()));
    if (*actualSize < 0 || std::uint64_t(*fileSize) + 8 != std::uint64_t(*actualSize))
      return std::unexpected(AudioException::format("Wav file is wrong size, reports {} is actually {}", std::uint64_t(*fileSize) + 8, *actualSize));
    read = device->read_full(signature);
    if (!read)
      return std::unexpected(std::move(read.error()));
    if (std::memcmp(signature.data(), "WAVE", 4) != 0)
      return std::unexpected(AudioException("Wav file has invalid WAVE magic bytes"));
    read = device->read_full(signature);
    if (!read)
      return std::unexpected(std::move(read.error()));
    if (std::memcmp(signature.data(), "fmt ", 4) != 0)
      return std::unexpected(AudioException("Wav file fmt subchunk has invalid magic bytes"));
    auto fmtSize = readLEType<std::uint32_t>(*device);
    if (!fmtSize)
      return std::unexpected(std::move(fmtSize.error()));
    if (*fmtSize < 16)
      return std::unexpected(AudioException::format("fmt subchunk is sized wrong, expected at least 16 got {}", *fmtSize));
    auto audioFormat = readLEType<std::uint16_t>(*device);
    if (!audioFormat)
      return std::unexpected(std::move(audioFormat.error()));
    if (*audioFormat != 1)
      return std::unexpected(AudioException("Only PCM wav format is supported"));
    auto channels = readLEType<std::uint16_t>(*device);
    if (!channels)
      return std::unexpected(std::move(channels.error()));
    auto rate = readLEType<std::uint32_t>(*device);
    if (!rate)
      return std::unexpected(std::move(rate.error()));
    auto byteRate = readLEType<std::uint32_t>(*device);
    if (!byteRate)
      return std::unexpected(std::move(byteRate.error()));
    auto alignment = readLEType<std::uint16_t>(*device);
    if (!alignment)
      return std::unexpected(std::move(alignment.error()));
    auto bits = readLEType<std::uint16_t>(*device);
    if (!bits)
      return std::unexpected(std::move(bits.error()));
    if (*bits != 16)
      return std::unexpected(AudioException("Only 16-bit PCM wavs are supported"));
    if (*channels == 0 || *rate == 0)
      return std::unexpected(AudioException("Wav channel count and sample rate must be nonzero"));
    if (std::uint64_t(*byteRate) * 8 != std::uint64_t(*rate) * *channels * *bits)
      return std::unexpected(AudioException("Sanity check failed, ByteRate is wrong"));
    if (std::uint64_t(*alignment) * 8 != std::uint64_t(*channels) * *bits)
      return std::unexpected(AudioException("Sanity check failed, BlockAlign is wrong"));
    seek = device->seek(*fmtSize - 16, IOSeek::Relative);
    if (!seek)
      return std::unexpected(std::move(seek.error()));
    read = device->read_full(signature);
    if (!read)
      return std::unexpected(std::move(read.error()));
    if (std::memcmp(signature.data(), "data", 4) != 0)
      return std::unexpected(AudioException("Wav file data subchunk has invalid magic bytes"));
    auto dataSize = readLEType<std::uint32_t>(*device);
    if (!dataSize)
      return std::unexpected(std::move(dataSize.error()));
    auto dataStart = device->pos();
    if (!dataStart)
      return std::unexpected(std::move(dataStart.error()));
    if (*dataStart < 0 || *dataStart > *actualSize || *dataSize > std::uint64_t(*actualSize - *dataStart))
      return std::unexpected(AudioException("Wav data size is inconsistent with file size"));
    if (*dataSize % *alignment != 0)
      return std::unexpected(AudioException("Wav data ends in an incomplete sample frame"));
#ifdef STAR_STREAM_AUDIO
    return WaveData{std::move(device), {}, *channels, *rate, *dataSize, *dataStart};
#else
    auto bytes = device->read_bytes(*dataSize);
    if (!bytes)
      return std::unexpected(std::move(bytes.error()));
    for (std::size_t i = 0; i < bytes->size() / 2; ++i)
      fromByteOrder(ByteOrder::LittleEndian, bytes->span().data() + i * 2, 2);
    auto shared = std::make_shared<ByteArray>(std::move(*bytes));
    ExternalBuffer memory(shared->span());
    return WaveData{std::move(memory), std::move(shared), *channels, *rate, *dataSize, 0};
#endif
  }
}

class CompressedAudioImpl {
public:
  [[nodiscard]] static std::expected<std::unique_ptr<CompressedAudioImpl>, StarException> create(IODevice& device) {
    auto impl = std::unique_ptr<CompressedAudioImpl>(new CompressedAudioImpl);
#ifdef STAR_STREAM_AUDIO
    auto cloned = device.clone();
    if (!cloned)
      return std::unexpected(std::move(cloned.error()));
    impl->m_device = std::move(*cloned);
    auto open = impl->m_device->open(IOMode::Read);
    if (!open)
      return std::unexpected(std::move(open.error()));
#else
    auto seek = device.seek(0);
    if (!seek)
      return std::unexpected(std::move(seek.error()));
    auto size = device.size();
    if (!size)
      return std::unexpected(std::move(size.error()));
    if (*size < 0 || std::uint64_t(*size) > std::numeric_limits<std::size_t>::max())
      return std::unexpected(AudioException("Audio device size is out of range"));
    auto bytes = device.read_bytes(static_cast<std::size_t>(*size));
    if (!bytes)
      return std::unexpected(std::move(bytes.error()));
    impl->m_bytes = std::make_shared<ByteArray>(std::move(*bytes));
    impl->m_device.reset(impl->m_bytes->span());
#endif
    auto opened = impl->open();
    if (!opened)
      return std::unexpected(std::move(opened.error()));
    return impl;
  }

  [[nodiscard]] std::expected<std::unique_ptr<CompressedAudioImpl>, StarException> clone() const {
    auto impl = std::unique_ptr<CompressedAudioImpl>(new CompressedAudioImpl);
#ifdef STAR_STREAM_AUDIO
    auto device = m_device->clone();
    if (!device)
      return std::unexpected(std::move(device.error()));
    impl->m_device = std::move(*device);
#else
    impl->m_bytes = m_bytes;
    impl->m_device.reset(m_bytes->span());
#endif
    auto opened = impl->open();
    if (!opened)
      return std::unexpected(std::move(opened.error()));
    auto position = currentSample();
    if (!position)
      return std::unexpected(std::move(position.error()));
    auto seek = impl->seekSample(*position);
    if (!seek)
      return std::unexpected(std::move(seek.error()));
    return impl;
  }

  ~CompressedAudioImpl() {
    if (m_open)
      ov_clear(&m_file);
  }

  unsigned channels() const { return m_channels; }
  unsigned sampleRate() const { return m_sampleRate; }
  double totalTime() const { return m_totalTime; }
  std::uint64_t totalSamples() const { return m_totalSamples; }

  [[nodiscard]] std::expected<void, StarException> seekTime(double time) {
    if (!std::isfinite(time) || time < 0 || time > m_totalTime)
      return std::unexpected(AudioException("Cannot seek ogg stream to an invalid time"));
    int result = ov_time_seek(&m_file, time);
    if (auto error = m_callbacks->takeError())
      return std::unexpected(std::move(*error));
    if (result != 0)
      return std::unexpected(AudioException::format("Cannot seek ogg stream by time ({})", result));
    return {};
  }

  [[nodiscard]] std::expected<void, StarException> seekSample(std::uint64_t sample) {
    if (sample > m_totalSamples || sample > std::uint64_t(std::numeric_limits<ogg_int64_t>::max()))
      return std::unexpected(AudioException("Cannot seek ogg stream to an invalid sample"));
    int result = ov_pcm_seek(&m_file, static_cast<ogg_int64_t>(sample));
    if (auto error = m_callbacks->takeError())
      return std::unexpected(std::move(*error));
    if (result != 0)
      return std::unexpected(AudioException::format("Cannot seek ogg stream by sample ({})", result));
    return {};
  }

  [[nodiscard]] std::expected<double, StarException> currentTime() const {
    double time = ov_time_tell(&m_file);
    if (auto error = m_callbacks->takeError())
      return std::unexpected(std::move(*error));
    if (time < 0)
      return std::unexpected(AudioException::format("Cannot determine ogg stream time ({})", time));
    return time;
  }

  [[nodiscard]] std::expected<std::uint64_t, StarException> currentSample() const {
    ogg_int64_t sample = ov_pcm_tell(&m_file);
    if (auto error = m_callbacks->takeError())
      return std::unexpected(std::move(*error));
    if (sample < 0)
      return std::unexpected(AudioException::format("Cannot determine ogg stream sample ({})", sample));
    return static_cast<std::uint64_t>(sample);
  }

  [[nodiscard]] std::expected<std::size_t, StarException> readPartial(std::span<std::int16_t> buffer) {
    int bitstream;
    long count;
    int bytes = static_cast<int>(std::min(buffer.size(), std::size_t(std::numeric_limits<int>::max() / 2)) * 2);
    do {
      count = ov_read(&m_file, reinterpret_cast<char*>(buffer.data()), bytes,
          std::endian::native == std::endian::big, 2, 1, &bitstream);
      if (auto error = m_callbacks->takeError())
        return std::unexpected(std::move(*error));
    } while (count == OV_HOLE);
    if (count < 0)
      return std::unexpected(AudioException::format("Error in Audio::read ({})", count));
    return static_cast<std::size_t>(count) / 2;
  }

private:
  CompressedAudioImpl() = default;

  IODevice& device() const {
#ifdef STAR_STREAM_AUDIO
    return *m_device;
#else
    return m_device;
#endif
  }

  [[nodiscard]] std::expected<void, StarException> open() {
    auto seek = device().seek(0);
    if (!seek)
      return std::unexpected(std::move(seek.error()));
    auto adapter = IODeviceCallbacks::create(device());
    if (!adapter)
      return std::unexpected(std::move(adapter.error()));
    m_callbacks.emplace(std::move(*adapter));
    ov_callbacks callbacks{};
    m_callbacks->setupOggCallbacks(callbacks);
    int result = ov_open_callbacks(&*m_callbacks, &m_file, nullptr, 0, callbacks);
    m_open = result == 0;
    if (auto error = m_callbacks->takeError())
      return std::unexpected(std::move(*error));
    if (result != 0)
      return std::unexpected(AudioException::format("File is not a valid ogg bitstream ({})", result));
    auto* info = ov_info(&m_file, -1);
    if (auto error = m_callbacks->takeError())
      return std::unexpected(std::move(*error));
    if (!info || info->channels <= 0 || info->rate <= 0 || std::uint64_t(info->rate) > std::numeric_limits<unsigned>::max())
      return std::unexpected(AudioException("Ogg stream has invalid audio metadata"));
    auto samples = ov_pcm_total(&m_file, -1);
    if (auto error = m_callbacks->takeError())
      return std::unexpected(std::move(*error));
    auto time = ov_time_total(&m_file, -1);
    if (auto error = m_callbacks->takeError())
      return std::unexpected(std::move(*error));
    if (samples < 0 || !std::isfinite(time) || time < 0)
      return std::unexpected(AudioException("Cannot determine ogg stream duration"));
    m_channels = info->channels;
    m_sampleRate = info->rate;
    m_totalSamples = samples;
    m_totalTime = time;
    return {};
  }

#ifdef STAR_STREAM_AUDIO
  std::shared_ptr<IODevice> m_device;
#else
  mutable ExternalBuffer m_device;
#endif
  std::shared_ptr<ByteArray const> m_bytes;
  mutable std::optional<IODeviceCallbacks> m_callbacks;
  mutable OggVorbis_File m_file{};
  bool m_open = false;
  unsigned m_channels = 0;
  unsigned m_sampleRate = 0;
  std::uint64_t m_totalSamples = 0;
  double m_totalTime = 0;
};

class UncompressedAudioImpl {
public:
  explicit UncompressedAudioImpl(WaveData data)
    : m_data(std::move(data)) {}

  [[nodiscard]] static std::expected<std::unique_ptr<UncompressedAudioImpl>, StarException> create(CompressedAudioImpl& compressed) {
    ByteArray bytes;
    std::int16_t buffer[1024];
    while (true) {
      auto count = compressed.readPartial(buffer);
      if (!count)
        return std::unexpected(std::move(count.error()));
      if (*count == 0)
        break;
      bytes.append({reinterpret_cast<char const*>(buffer), *count * sizeof(std::int16_t)});
    }
    auto shared = std::make_shared<ByteArray>(std::move(bytes));
#ifdef STAR_STREAM_AUDIO
    auto memory = std::make_shared<ExternalBuffer>(shared->span());
#else
    ExternalBuffer memory(shared->span());
#endif
    auto size = shared->size();
    return std::make_unique<UncompressedAudioImpl>(WaveData{
        std::move(memory), std::move(shared), compressed.channels(), compressed.sampleRate(), size, 0});
  }

  [[nodiscard]] std::expected<std::unique_ptr<UncompressedAudioImpl>, StarException> clone() const {
#ifndef STAR_STREAM_AUDIO
    // Copy the borrowed view and cursor, retaining the same immutable byte owner.
    return std::make_unique<UncompressedAudioImpl>(m_data);
#else
    auto device = m_data.stream->clone();
    if (!device)
      return std::unexpected(std::move(device.error()));
    auto position = m_data.stream->pos();
    if (!position)
      return std::unexpected(std::move(position.error()));
    auto seek = (*device)->seek(*position);
    if (!seek)
      return std::unexpected(std::move(seek.error()));
    return std::make_unique<UncompressedAudioImpl>(WaveData{
        std::move(*device), m_data.bytes, m_data.channels, m_data.sampleRate, m_data.dataSize, m_data.dataStart});
#endif
  }

  unsigned channels() const { return m_data.channels; }
  unsigned sampleRate() const { return m_data.sampleRate; }
  std::uint64_t totalSamples() const { return m_data.dataSize / 2 / channels(); }
  double totalTime() const { return static_cast<double>(totalSamples()) / sampleRate(); }

  [[nodiscard]] std::expected<void, StarException> seekTime(double time) {
    if (!std::isfinite(time) || time < 0 || time > totalTime())
      return std::unexpected(AudioException("Cannot seek PCM audio to an invalid time"));
    long double sample = static_cast<long double>(time) * sampleRate();
    if (sample >= static_cast<long double>(totalSamples()))
      return seekSample(totalSamples());
    return seekSample(static_cast<std::uint64_t>(sample));
  }

  [[nodiscard]] std::expected<void, StarException> seekSample(std::uint64_t sample) {
    if (sample > totalSamples())
      return std::unexpected(AudioException("Cannot seek PCM audio past end of stream"));
    return m_data.device().seek(m_data.dataStart + static_cast<StreamOffset>(sample * 2 * channels()));
  }

  [[nodiscard]] std::expected<std::uint64_t, StarException> currentSample() const {
    auto position = m_data.device().pos();
    if (!position)
      return std::unexpected(std::move(position.error()));
    if (*position < m_data.dataStart || std::uint64_t(*position - m_data.dataStart) > m_data.dataSize)
      return std::unexpected(AudioException("PCM audio device position is outside the data chunk"));
    return static_cast<std::uint64_t>(*position - m_data.dataStart) / 2 / channels();
  }

  [[nodiscard]] std::expected<double, StarException> currentTime() const {
    auto sample = currentSample();
    if (!sample)
      return std::unexpected(std::move(sample.error()));
    return static_cast<double>(*sample) / sampleRate();
  }

  [[nodiscard]] std::expected<std::size_t, StarException> readPartial(std::span<std::int16_t> buffer) {
    auto position = m_data.device().pos();
    if (!position)
      return std::unexpected(std::move(position.error()));
    if (*position < m_data.dataStart || std::uint64_t(*position - m_data.dataStart) > m_data.dataSize)
      return std::unexpected(AudioException("PCM audio device position is outside the data chunk"));
    std::size_t remaining = m_data.dataSize - static_cast<std::size_t>(*position - m_data.dataStart);
    std::size_t requested = std::min(buffer.size(), remaining / 2) * 2;
    if (requested == 0)
      return 0;
    auto read = m_data.device().read({reinterpret_cast<char*>(buffer.data()), requested});
    if (!read)
      return std::unexpected(std::move(read.error()));
    if (*read == 0)
      return std::unexpected(AudioException("PCM audio data is truncated"));
    if (*read % 2 != 0) {
      auto tail = m_data.device().read_full({reinterpret_cast<char*>(buffer.data()) + *read, 1});
      if (!tail)
        return std::unexpected(std::move(tail.error()));
      ++*read;
    }
    if (!m_data.bytes) {
      for (std::size_t i = 0; i < *read / 2; ++i)
        fromByteOrder(ByteOrder::LittleEndian, reinterpret_cast<char*>(buffer.data()) + i * 2, 2);
    }
    return *read / 2;
  }

private:
  WaveData m_data;
};

std::expected<Audio, StarException> Audio::create(std::shared_ptr<IODevice> device, String name) {
  if (!device)
    return std::unexpected(AudioException("Cannot load audio from a null device"));
  if (!device->is_open()) {
    auto open = device->open(IOMode::Read);
    if (!open)
      return std::unexpected(std::move(open.error()));
  }
  Audio audio;
  audio.m_name = std::move(name);
  auto wav = isUncompressed(*device);
  if (!wav)
    return std::unexpected(std::move(wav.error()));
  if (*wav) {
    auto data = parseWav(std::move(device));
    if (!data)
      return std::unexpected(std::move(data.error()));
    audio.m_uncompressed = std::make_unique<UncompressedAudioImpl>(std::move(*data));
  } else {
    auto compressed = CompressedAudioImpl::create(*device);
    if (!compressed)
      return std::unexpected(std::move(compressed.error()));
    audio.m_compressed = std::move(*compressed);
  }
  return audio;
}

Audio::Audio() = default;
Audio::~Audio() = default;
Audio::Audio(Audio&& audio) noexcept = default;
Audio& Audio::operator=(Audio&& audio) noexcept = default;

std::expected<Audio, StarException> Audio::clone() const {
  Audio audio;
  audio.m_name = m_name;
  if (m_uncompressed) {
    auto cloned = m_uncompressed->clone();
    if (!cloned)
      return std::unexpected(std::move(cloned.error()));
    audio.m_uncompressed = std::move(*cloned);
  } else {
    auto cloned = m_compressed->clone();
    if (!cloned)
      return std::unexpected(std::move(cloned.error()));
    audio.m_compressed = std::move(*cloned);
  }
  return audio;
}

unsigned Audio::channels() const {
  return m_uncompressed ? m_uncompressed->channels() : m_compressed->channels();
}

unsigned Audio::sampleRate() const {
  return m_uncompressed ? m_uncompressed->sampleRate() : m_compressed->sampleRate();
}

std::expected<double, StarException> Audio::totalTime() const {
  return m_uncompressed ? m_uncompressed->totalTime() : m_compressed->totalTime();
}

std::expected<std::uint64_t, StarException> Audio::totalSamples() const {
  return m_uncompressed ? m_uncompressed->totalSamples() : m_compressed->totalSamples();
}

bool Audio::compressed() const {
  return static_cast<bool>(m_compressed);
}

std::expected<void, StarException> Audio::uncompress() {
  if (m_compressed) {
    auto clone = m_compressed->clone();
    if (!clone)
      return std::unexpected(std::move(clone.error()));
    auto uncompressed = UncompressedAudioImpl::create(**clone);
    if (!uncompressed)
      return std::unexpected(std::move(uncompressed.error()));
    m_uncompressed = std::move(*uncompressed);
    m_compressed.reset();
  }
  return {};
}

std::expected<void, StarException> Audio::seekTime(double time) {
  return m_uncompressed ? m_uncompressed->seekTime(time) : m_compressed->seekTime(time);
}

std::expected<void, StarException> Audio::seekSample(std::uint64_t sample) {
  return m_uncompressed ? m_uncompressed->seekSample(sample) : m_compressed->seekSample(sample);
}

std::expected<double, StarException> Audio::currentTime() const {
  return m_uncompressed ? m_uncompressed->currentTime() : m_compressed->currentTime();
}

std::expected<std::uint64_t, StarException> Audio::currentSample() const {
  return m_uncompressed ? m_uncompressed->currentSample() : m_compressed->currentSample();
}

std::expected<std::size_t, StarException> Audio::readPartial(std::span<std::int16_t> buffer) {
  if (buffer.empty())
    return 0;
  return m_uncompressed ? m_uncompressed->readPartial(buffer) : m_compressed->readPartial(buffer);
}

std::expected<std::size_t, StarException> Audio::read(std::span<std::int16_t> buffer) {
  std::size_t total = 0;
  while (total < buffer.size()) {
    auto count = readPartial(buffer.subspan(total));
    if (!count)
      return std::unexpected(std::move(count.error()));
    total += *count;
    if (*count == 0)
      break;
  }
  return total;
}

std::expected<std::size_t, StarException> Audio::resample(unsigned destinationChannels, unsigned destinationSampleRate, std::span<std::int16_t> destinationBuffer, double velocity) {
  if (destinationChannels == 0 || destinationSampleRate == 0 || !std::isfinite(velocity) || velocity <= 0)
    return std::unexpected(AudioException("Invalid audio resampling channels, rate, or velocity"));
  if (destinationBuffer.size() > std::numeric_limits<unsigned>::max()
      || destinationBuffer.size() / destinationChannels > std::numeric_limits<unsigned>::max() / 8)
    return std::unexpected(AudioException("Audio resampling buffer is too large"));
  unsigned destinationSamples = static_cast<unsigned>(destinationBuffer.size() / destinationChannels);
  if (destinationSamples == 0)
    return 0;

  unsigned sourceChannels = channels();
  unsigned sourceSampleRate = sampleRate();

  if (velocity != 1.0) {
    double effectiveRate = sourceSampleRate * velocity;
    if (!std::isfinite(effectiveRate) || effectiveRate < 1 || effectiveRate > std::numeric_limits<unsigned>::max())
      return std::unexpected(AudioException("Audio resampling velocity produces an invalid sample rate"));
    sourceSampleRate = static_cast<unsigned>(effectiveRate);
  }

  if (destinationChannels == sourceChannels && destinationSampleRate == sourceSampleRate) {
    // If the destination and source channel count and sample rate are the
    // same, this is the same as a read.

    return read(destinationBuffer);

  } else if (destinationSampleRate == sourceSampleRate) {
    // If the destination and source sample rate are the same, then we can skip
    // the super-sampling math.

    if (destinationSamples > std::numeric_limits<unsigned>::max() / sourceChannels
        || std::uint64_t(destinationSamples) * sourceChannels > std::numeric_limits<std::size_t>::max() / sizeof(std::int16_t))
      return std::unexpected(AudioException("Audio resampling source buffer is too large"));
    unsigned sourceBufferSize = destinationSamples * sourceChannels;

    m_workingBuffer.resize(sourceBufferSize * sizeof(std::int16_t));
    std::span<std::int16_t> sourceBuffer(reinterpret_cast<std::int16_t*>(m_workingBuffer.span().data()), sourceBufferSize);

    auto count = read(sourceBuffer);
    if (!count)
      return std::unexpected(std::move(count.error()));
    unsigned readSamples = static_cast<unsigned>(*count / sourceChannels);

    for (unsigned sample = 0; sample < readSamples; ++sample) {
      unsigned sourceBufferIndex = sample * sourceChannels;
      unsigned destinationBufferIndex = sample * destinationChannels;

      for (unsigned destinationChannel = 0; destinationChannel < destinationChannels; ++destinationChannel) {
        // If the destination channel count is greater than the source
        // channels, simply copy the last channel
        unsigned sourceChannel = std::min(destinationChannel, sourceChannels - 1);
        destinationBuffer[destinationBufferIndex + destinationChannel] =
            sourceBuffer[sourceBufferIndex + sourceChannel];
      }
    }

    return readSamples * destinationChannels;

  } else {
    // Otherwise, we have to do a full resample.

    std::uint64_t sourceSamplesWide = (std::uint64_t(sourceSampleRate) * destinationSamples + destinationSampleRate - 1) / destinationSampleRate;
    if (sourceSamplesWide > std::numeric_limits<unsigned>::max() / sourceChannels
        || sourceSamplesWide * sourceChannels > std::numeric_limits<std::size_t>::max() / sizeof(std::int16_t))
      return std::unexpected(AudioException("Audio resampling source buffer is too large"));
    unsigned sourceSamples = static_cast<unsigned>(sourceSamplesWide);
    unsigned sourceBufferSize = sourceSamples * sourceChannels;

    m_workingBuffer.resize(sourceBufferSize * sizeof(std::int16_t));
    std::span<std::int16_t> sourceBuffer(reinterpret_cast<std::int16_t*>(m_workingBuffer.span().data()), sourceBufferSize);

    auto count = read(sourceBuffer);
    if (!count)
      return std::unexpected(std::move(count.error()));
    unsigned readSamples = static_cast<unsigned>(*count / sourceChannels);

    if (readSamples == 0)
      return 0;

    unsigned writtenSamples = 0;

    for (unsigned destinationSample = 0; destinationSample < destinationSamples; ++destinationSample) {
      unsigned destinationBufferIndex = destinationSample * destinationChannels;

      for (unsigned destinationChannel = 0; destinationChannel < destinationChannels; ++destinationChannel) {
        static int const SuperSampleFactor = 8;

        // If the destination channel count is greater than the source
        // channels, simply copy the last channel
        unsigned sourceChannel = std::min(destinationChannel, sourceChannels - 1);

        int sample = 0;
        int sampleCount = 0;
        for (int superSample = 0; superSample < SuperSampleFactor; ++superSample) {
          unsigned sourceSample = static_cast<unsigned>((std::uint64_t(destinationSample) * SuperSampleFactor + superSample) * sourceSamples / destinationSamples / SuperSampleFactor);
          if (sourceSample < readSamples) {
            unsigned sourceBufferIndex = sourceSample * sourceChannels;
            if constexpr (Star::DebugEnabled) {
              Star::star_assert_failed((sourceBufferIndex + sourceChannel < sourceBufferSize));
            }
            sample += sourceBuffer[sourceBufferIndex + sourceChannel];
            ++sampleCount;
          }
        }

        // If sampleCount is zero, then we are past the end of our read data
        // completely, and can stop
        if (sampleCount == 0)
          return writtenSamples * destinationChannels;

        sample /= sampleCount;
        destinationBuffer[destinationBufferIndex + destinationChannel] = (std::int16_t)sample;
        writtenSamples = destinationSample + 1;
      }
    }

    return writtenSamples * destinationChannels;
  }
}

String const& Audio::name() const {
  return m_name;
}

void Audio::setName(String name) {
  m_name = std::move(name);
}

}

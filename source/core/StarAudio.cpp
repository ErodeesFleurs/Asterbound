// Native Vorbis callbacks report errors through their datasource, never unwind.
#define OV_EXCLUDE_STATIC_CALLBACKS

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

extern float const default_perceptual_range_db = 40.f;
extern float const default_perceptual_boost_range_db = 6.f;
// https://github.com/discord/perceptual
float perceptual_to_amplitude(float perceptual, float normalized_max, float range, float boost_range) {
  if (perceptual == 0.f) return 0.f;
  float db = perceptual > normalized_max ? ((perceptual - normalized_max) / normalized_max) * boost_range
                                         : (perceptual / normalized_max) * range - range;
  return normalized_max * std::pow(10.f, db / 20.f);
}

float amplitude_to_perceptual(float amp, float normalized_max, float range, float boost_range) {
  if (amp == 0.f) return 0.f;
  float const db = 20.f * std::log10(amp / normalized_max);
  float perceptual = db > 0.f ? db / boost_range + 1 : (range + db) / range;
  return normalized_max * perceptual;
}

namespace {
  struct WaveData {
#ifdef STAR_STREAM_AUDIO
    std::shared_ptr<IODevice> stream;
#endif
    std::shared_ptr<ByteArray const> bytes;
    unsigned channels;
    unsigned sample_rate;
    std::size_t data_size;
    StreamOffset data_start;
  };

  template <byte_order_value T> [[nodiscard]] std::expected<T, StarException> read_le_type(IODevice& device) {
    std::array<char, sizeof(T)> bytes;
    auto read = device.read_full(bytes);
    if (!read)
      return std::unexpected(std::move(read.error()));
    return from_byte_order(ByteOrder::LittleEndian, std::bit_cast<T>(bytes));
  }

  [[nodiscard]] std::expected<bool, StarException> is_uncompressed(IODevice& device) {
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

  [[nodiscard]] std::expected<WaveData, StarException> parse_wav(std::shared_ptr<IODevice> device) {
    auto seek = device->seek(0);
    if (!seek)
      return std::unexpected(std::move(seek.error()));
    std::array<char, 4> signature{};
    auto read = device->read_full(signature);
    if (!read)
      return std::unexpected(std::move(read.error()));
    if (std::memcmp(signature.data(), "RIFF", 4) != 0)
      return std::unexpected(AudioException("Wav file has invalid RIFF magic bytes"));
    auto file_size = read_le_type<std::uint32_t>(*device);
    if (!file_size)
      return std::unexpected(std::move(file_size.error()));
    auto actual_size = device->size();
    if (!actual_size)
      return std::unexpected(std::move(actual_size.error()));
    if (*actual_size < 0 || std::uint64_t(*file_size) + 8 != std::uint64_t(*actual_size))
      return std::unexpected(AudioException::format("Wav file is wrong size, reports {} is actually {}",
                                                    std::uint64_t(*file_size) + 8, *actual_size));
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
    auto fmt_size = read_le_type<std::uint32_t>(*device);
    if (!fmt_size)
      return std::unexpected(std::move(fmt_size.error()));
    if (*fmt_size < 16)
      return std::unexpected(
        AudioException::format("fmt subchunk is sized wrong, expected at least 16 got {}", *fmt_size));
    auto audio_format = read_le_type<std::uint16_t>(*device);
    if (!audio_format)
      return std::unexpected(std::move(audio_format.error()));
    if (*audio_format != 1)
      return std::unexpected(AudioException("Only PCM wav format is supported"));
    auto channels = read_le_type<std::uint16_t>(*device);
    if (!channels)
      return std::unexpected(std::move(channels.error()));
    auto rate = read_le_type<std::uint32_t>(*device);
    if (!rate)
      return std::unexpected(std::move(rate.error()));
    auto byte_rate = read_le_type<std::uint32_t>(*device);
    if (!byte_rate)
      return std::unexpected(std::move(byte_rate.error()));
    auto alignment = read_le_type<std::uint16_t>(*device);
    if (!alignment)
      return std::unexpected(std::move(alignment.error()));
    auto bits = read_le_type<std::uint16_t>(*device);
    if (!bits)
      return std::unexpected(std::move(bits.error()));
    if (*bits != 16)
      return std::unexpected(AudioException("Only 16-bit PCM wavs are supported"));
    if (*channels == 0 || *rate == 0)
      return std::unexpected(AudioException("Wav channel count and sample rate must be nonzero"));
    if (std::uint64_t(*byte_rate) * 8 != std::uint64_t(*rate) * *channels * *bits)
      return std::unexpected(AudioException("Sanity check failed, ByteRate is wrong"));
    if (std::uint64_t(*alignment) * 8 != std::uint64_t(*channels) * *bits)
      return std::unexpected(AudioException("Sanity check failed, BlockAlign is wrong"));
    seek = device->seek(*fmt_size - 16, IOSeek::Relative);
    if (!seek)
      return std::unexpected(std::move(seek.error()));
    read = device->read_full(signature);
    if (!read)
      return std::unexpected(std::move(read.error()));
    if (std::memcmp(signature.data(), "data", 4) != 0)
      return std::unexpected(AudioException("Wav file data subchunk has invalid magic bytes"));
    auto data_size = read_le_type<std::uint32_t>(*device);
    if (!data_size)
      return std::unexpected(std::move(data_size.error()));
    auto data_start = device->pos();
    if (!data_start)
      return std::unexpected(std::move(data_start.error()));
    if (*data_start < 0 || *data_start > *actual_size || *data_size > std::uint64_t(*actual_size - *data_start))
      return std::unexpected(AudioException("Wav data size is inconsistent with file size"));
    if (*data_size % *alignment != 0)
      return std::unexpected(AudioException("Wav data ends in an incomplete sample frame"));
#ifdef STAR_STREAM_AUDIO
    return WaveData{std::move(device), {}, *channels, *rate, *data_size, *data_start};
#else
    auto bytes = device->read_bytes(*data_size);
    if (!bytes)
      return std::unexpected(std::move(bytes.error()));
    for (std::size_t i = 0; i < bytes->size() / 2; ++i)
      from_byte_order(ByteOrder::LittleEndian, bytes->span().data() + i * 2, 2);
    auto shared = std::make_shared<ByteArray>(std::move(*bytes));
    return WaveData{std::move(shared), *channels, *rate, *data_size, 0};
#endif
  }
}

class CompressedAudioImpl {
public:
  CompressedAudioImpl() = default;

  [[nodiscard]] static std::expected<std::unique_ptr<CompressedAudioImpl>, StarException> create(IODevice& device) {
    auto impl = std::make_unique<CompressedAudioImpl>();
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

  [[nodiscard]] std::expected<std::unique_ptr<CompressedAudioImpl>, StarException>
  clone(std::optional<std::uint64_t> frame = {}) const {
    auto impl = std::make_unique<CompressedAudioImpl>();
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
    if (!frame) {
      auto position = current_sample();
      if (!position)
        return std::unexpected(std::move(position.error()));
      frame = *position;
    }
    if (*frame != 0) {
      auto seek = impl->seek_sample(*frame);
      if (!seek)
        return std::unexpected(std::move(seek.error()));
    }
    return impl;
  }

  ~CompressedAudioImpl() {
    if (m_open)
      ov_clear(&m_file);
  }

  unsigned channels() const { return m_channels; }
  unsigned sample_rate() const { return m_sample_rate; }
  double total_time() const { return m_total_time; }
  std::uint64_t total_samples() const { return m_total_samples; }

  [[nodiscard]] std::expected<void, StarException> seek_time(double time) {
    if (!is_finite(time) || time < 0 || time > m_total_time)
      return std::unexpected(AudioException("Cannot seek ogg stream to an invalid time"));
    int result = ov_time_seek(&m_file, time);
    if (auto error = m_callbacks->takeError())
      return std::unexpected(std::move(*error));
    if (result != 0)
      return std::unexpected(AudioException::format("Cannot seek ogg stream by time ({})", result));
    return {};
  }

  [[nodiscard]] std::expected<void, StarException> seek_sample(std::uint64_t sample) {
    if (sample > m_total_samples || sample > std::uint64_t(std::numeric_limits<ogg_int64_t>::max()))
      return std::unexpected(AudioException("Cannot seek ogg stream to an invalid sample"));
    int result = ov_pcm_seek(&m_file, static_cast<ogg_int64_t>(sample));
    if (auto error = m_callbacks->takeError())
      return std::unexpected(std::move(*error));
    if (result != 0)
      return std::unexpected(AudioException::format("Cannot seek ogg stream by sample ({})", result));
    return {};
  }

  [[nodiscard]] std::expected<double, StarException> current_time() const {
    double time = ov_time_tell(&m_file);
    if (auto error = m_callbacks->takeError())
      return std::unexpected(std::move(*error));
    if (time < 0)
      return std::unexpected(AudioException::format("Cannot determine ogg stream time ({})", time));
    return time;
  }

  [[nodiscard]] std::expected<std::uint64_t, StarException> current_sample() const {
    ogg_int64_t sample = ov_pcm_tell(&m_file);
    if (auto error = m_callbacks->takeError())
      return std::unexpected(std::move(*error));
    if (sample < 0)
      return std::unexpected(AudioException::format("Cannot determine ogg stream sample ({})", sample));
    return static_cast<std::uint64_t>(sample);
  }

  [[nodiscard]] std::expected<std::size_t, StarException> read_partial(std::span<std::int16_t> buffer) {
    auto count = read_pcm_bytes(std::as_writable_bytes(buffer));
    if (!count)
      return std::unexpected(std::move(count.error()));
    return *count / sizeof(std::int16_t);
  }

  [[nodiscard]] std::expected<std::size_t, StarException> read_pcm_bytes(std::span<std::byte> buffer) {
    int bitstream;
    long count;
    int bytes = static_cast<int>(std::min(buffer.size() / 2, std::size_t(std::numeric_limits<int>::max() / 2)) * 2);
    do {
      count = ov_read(&m_file, reinterpret_cast<char*>(buffer.data()), bytes,
          std::endian::native == std::endian::big, 2, 1, &bitstream);
      if (auto error = m_callbacks->takeError())
        return std::unexpected(std::move(*error));
    } while (count == OV_HOLE);
    if (count < 0)
      return std::unexpected(AudioException::format("Error in Audio::read ({})", count));
    return static_cast<std::size_t>(count);
  }

private:
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
    if (samples < 0 || !is_finite(time) || time < 0)
      return std::unexpected(AudioException("Cannot determine ogg stream duration"));
    m_channels = info->channels;
    m_sample_rate = info->rate;
    m_total_samples = samples;
    m_total_time = time;
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
  unsigned m_sample_rate = 0;
  std::uint64_t m_total_samples = 0;
  double m_total_time = 0;
};

class UncompressedAudioImpl {
public:
  explicit UncompressedAudioImpl(WaveData data)
    : m_data(std::move(data)) {}

  [[nodiscard]] static std::expected<std::unique_ptr<UncompressedAudioImpl>, StarException> create(CompressedAudioImpl& compressed) {
    constexpr std::size_t decode_chunk_bytes = 8 * 1024;
    constexpr std::size_t hint_limit = 1024 * 1024 - decode_chunk_bytes;
    std::uint64_t frame_bytes = std::uint64_t(compressed.channels()) * sizeof(std::int16_t);
    std::size_t hint = compressed.total_samples() > hint_limit / frame_bytes
      ? hint_limit
      : static_cast<std::size_t>(compressed.total_samples() * frame_bytes);
    ByteArray bytes = ByteArray::with_reserve(hint + decode_chunk_bytes);
    std::size_t decoded_size = 0;
    while (true) {
      if (decoded_size > ByteArray::max_size() - decode_chunk_bytes)
        return std::unexpected(AudioException("Decoded audio is too large"));
      bytes.resize(decoded_size + decode_chunk_bytes);
      auto count = compressed.read_pcm_bytes(std::as_writable_bytes(bytes.span().subspan(decoded_size)));
      if (!count)
        return std::unexpected(std::move(count.error()));
      if (*count == 0)
        break;
      decoded_size += *count;
    }
    bytes.resize(decoded_size);
    auto shared = std::make_shared<ByteArray>(std::move(bytes));
#ifdef STAR_STREAM_AUDIO
    auto memory = std::make_shared<ExternalBuffer>(shared->span());
#endif
    auto size = shared->size();
    return std::make_unique<UncompressedAudioImpl>(WaveData{
#ifdef STAR_STREAM_AUDIO
      std::move(memory),
#endif
      std::move(shared), compressed.channels(), compressed.sample_rate(), size, 0});
  }

  [[nodiscard]] std::expected<std::unique_ptr<UncompressedAudioImpl>, StarException> clone() const {
#ifndef STAR_STREAM_AUDIO
    auto impl = std::make_unique<UncompressedAudioImpl>(m_data);
    impl->m_sample_cursor = m_sample_cursor;
    return impl;
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
    return std::make_unique<UncompressedAudioImpl>(WaveData{std::move(*device), m_data.bytes, m_data.channels,
                                                            m_data.sample_rate, m_data.data_size, m_data.data_start});
#endif
  }

  unsigned channels() const { return m_data.channels; }
  unsigned sample_rate() const { return m_data.sample_rate; }
  std::uint64_t total_samples() const { return m_data.data_size / 2 / channels(); }
  double total_time() const { return static_cast<double>(total_samples()) / sample_rate(); }

  [[nodiscard]] std::expected<void, StarException> seek_time(double time) {
    if (!is_finite(time) || time < 0 || time > total_time())
      return std::unexpected(AudioException("Cannot seek PCM audio to an invalid time"));
    long double sample = static_cast<long double>(time) * sample_rate();
    if (sample >= static_cast<long double>(total_samples()))
      return seek_sample(total_samples());
    return seek_sample(static_cast<std::uint64_t>(sample));
  }

  [[nodiscard]] std::expected<void, StarException> seek_sample(std::uint64_t sample) {
    if (sample > total_samples())
      return std::unexpected(AudioException("Cannot seek PCM audio past end of stream"));
#ifdef STAR_STREAM_AUDIO
    return m_data.stream->seek(m_data.data_start + static_cast<StreamOffset>(sample * 2 * channels()));
#else
    m_sample_cursor = static_cast<std::size_t>(sample * channels());
    return {};
#endif
  }

  [[nodiscard]] std::expected<std::uint64_t, StarException> current_sample() const {
#ifdef STAR_STREAM_AUDIO
    auto position = m_data.stream->pos();
    if (!position)
      return std::unexpected(std::move(position.error()));
    if (*position < m_data.data_start || std::uint64_t(*position - m_data.data_start) > m_data.data_size)
      return std::unexpected(AudioException("PCM audio device position is outside the data chunk"));
    return static_cast<std::uint64_t>(*position - m_data.data_start) / 2 / channels();
#else
    return m_sample_cursor / channels();
#endif
  }

  [[nodiscard]] std::expected<double, StarException> current_time() const {
    auto sample = current_sample();
    if (!sample)
      return std::unexpected(std::move(sample.error()));
    return static_cast<double>(*sample) / sample_rate();
  }

  [[nodiscard]] std::expected<std::size_t, StarException> read_partial(std::span<std::int16_t> buffer) {
#ifdef STAR_STREAM_AUDIO
    auto position = m_data.stream->pos();
    if (!position)
      return std::unexpected(std::move(position.error()));
    if (*position < m_data.data_start || std::uint64_t(*position - m_data.data_start) > m_data.data_size)
      return std::unexpected(AudioException("PCM audio device position is outside the data chunk"));
    std::size_t remaining = m_data.data_size - static_cast<std::size_t>(*position - m_data.data_start);
    std::size_t requested = std::min(buffer.size(), remaining / 2) * 2;
    if (requested == 0)
      return 0;
    auto read = m_data.stream->read({reinterpret_cast<char*>(buffer.data()), requested});
    if (!read)
      return std::unexpected(std::move(read.error()));
    if (*read == 0)
      return std::unexpected(AudioException("PCM audio data is truncated"));
    if (*read % 2 != 0) {
      auto tail = m_data.stream->read_full({reinterpret_cast<char*>(buffer.data()) + *read, 1});
      if (!tail)
        return std::unexpected(std::move(tail.error()));
      ++*read;
    }
    if (!m_data.bytes) {
      for (std::size_t i = 0; i < *read / 2; ++i)
        from_byte_order(ByteOrder::LittleEndian, reinterpret_cast<char*>(buffer.data()) + i * 2, 2);
    }
    return *read / 2;
#else
    std::size_t count = std::min(buffer.size(), m_data.data_size / sizeof(std::int16_t) - m_sample_cursor);
    if (count != 0) {
      auto source = m_data.bytes->span().subspan(m_sample_cursor * sizeof(std::int16_t), count * sizeof(std::int16_t));
      std::memcpy(buffer.data(), source.data(), source.size());
      m_sample_cursor += count;
    }
    return count;
#endif
  }

private:
  WaveData m_data;
#ifndef STAR_STREAM_AUDIO
  std::size_t m_sample_cursor = 0;
#endif
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
  auto wav = is_uncompressed(*device);
  if (!wav)
    return std::unexpected(std::move(wav.error()));
  if (*wav) {
    auto data = parse_wav(std::move(device));
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

unsigned Audio::sample_rate() const {
  return m_uncompressed ? m_uncompressed->sample_rate() : m_compressed->sample_rate();
}

std::expected<double, StarException> Audio::total_time() const {
  return m_uncompressed ? m_uncompressed->total_time() : m_compressed->total_time();
}

std::expected<std::uint64_t, StarException> Audio::total_samples() const {
  return m_uncompressed ? m_uncompressed->total_samples() : m_compressed->total_samples();
}

bool Audio::compressed() const {
  return static_cast<bool>(m_compressed);
}

std::expected<void, StarException> Audio::uncompress() {
  if (m_compressed) {
    auto position = m_compressed->current_sample();
    if (!position)
      return std::unexpected(std::move(position.error()));
    auto staging = m_compressed->clone(0);
    if (!staging)
      return std::unexpected(std::move(staging.error()));
    auto uncompressed = UncompressedAudioImpl::create(**staging);
    if (!uncompressed)
      return std::unexpected(std::move(uncompressed.error()));
    auto restore = (*uncompressed)->seek_sample(*position);
    if (!restore)
      return std::unexpected(std::move(restore.error()));
    m_uncompressed = std::move(*uncompressed);
    m_compressed.reset();
  }
  return {};
}

std::expected<void, StarException> Audio::seek_time(double time) {
  return m_uncompressed ? m_uncompressed->seek_time(time) : m_compressed->seek_time(time);
}

std::expected<void, StarException> Audio::seek_sample(std::uint64_t sample) {
  return m_uncompressed ? m_uncompressed->seek_sample(sample) : m_compressed->seek_sample(sample);
}

std::expected<double, StarException> Audio::current_time() const {
  return m_uncompressed ? m_uncompressed->current_time() : m_compressed->current_time();
}

std::expected<std::uint64_t, StarException> Audio::current_sample() const {
  return m_uncompressed ? m_uncompressed->current_sample() : m_compressed->current_sample();
}

std::expected<std::size_t, StarException> Audio::read_partial(std::span<std::int16_t> buffer) {
  if (buffer.empty())
    return 0;
  return m_uncompressed ? m_uncompressed->read_partial(buffer) : m_compressed->read_partial(buffer);
}

std::expected<std::size_t, StarException> Audio::read(std::span<std::int16_t> buffer) {
  std::size_t total = 0;
  while (total < buffer.size()) {
    auto count = read_partial(buffer.subspan(total));
    if (!count)
      return std::unexpected(std::move(count.error()));
    total += *count;
    if (*count == 0)
      break;
  }
  return total;
}

std::expected<std::size_t, StarException> Audio::resample(unsigned destination_channels,
                                                          unsigned destination_sample_rate,
                                                          std::span<std::int16_t> destination_buffer, double velocity) {
  if (destination_channels == 0 || destination_sample_rate == 0 || !is_finite(velocity) || velocity <= 0)
    return std::unexpected(AudioException("Invalid audio resampling channels, rate, or velocity"));
  if (destination_buffer.size() > std::numeric_limits<unsigned>::max()
      || destination_buffer.size() / destination_channels > std::numeric_limits<unsigned>::max() / 8)
    return std::unexpected(AudioException("Audio resampling buffer is too large"));
  unsigned destination_samples = static_cast<unsigned>(destination_buffer.size() / destination_channels);
  if (destination_samples == 0)
    return 0;

  unsigned source_channels = channels();
  unsigned source_sample_rate = sample_rate();
  if (velocity != 1.0) {
    double effective_rate = source_sample_rate * velocity;
    if (!is_finite(effective_rate) || effective_rate < 1 || effective_rate > std::numeric_limits<unsigned>::max())
      return std::unexpected(AudioException("Audio resampling velocity produces an invalid sample rate"));
    source_sample_rate = static_cast<unsigned>(effective_rate);
  }

  if (destination_channels == source_channels && destination_sample_rate == source_sample_rate)
    return read(destination_buffer);

  // Preserve the per-call ceiling and consumption of the complete source chunk.
  std::uint64_t source_samples_wide = destination_samples;
  if (destination_sample_rate != source_sample_rate)
    source_samples_wide =
      (std::uint64_t(source_sample_rate) * destination_samples + destination_sample_rate - 1) / destination_sample_rate;
  if (source_samples_wide > std::numeric_limits<unsigned>::max() / source_channels
      || source_samples_wide * source_channels > std::numeric_limits<std::size_t>::max() / sizeof(std::int16_t))
    return std::unexpected(AudioException("Audio resampling source buffer is too large"));
  unsigned source_samples = static_cast<unsigned>(source_samples_wide);
  unsigned source_buffer_size = source_samples * source_channels;
  if (source_buffer_size > m_working_capacity) {
    constexpr std::size_t max_capacity = std::min<std::size_t>(
      std::numeric_limits<unsigned>::max(), std::numeric_limits<std::size_t>::max() / sizeof(std::int16_t));
    std::size_t grown_capacity = m_working_capacity > max_capacity / 2 ? max_capacity : m_working_capacity * 2;
    std::size_t capacity = std::max<std::size_t>(source_buffer_size, grown_capacity);
    m_working_buffer = std::make_unique_for_overwrite<std::int16_t[]>(capacity);
    m_working_capacity = capacity;
  }
  std::span<std::int16_t> source_buffer(m_working_buffer.get(), source_buffer_size);
  auto count = read(source_buffer);
  if (!count)
    return std::unexpected(std::move(count.error()));
  unsigned read_samples = static_cast<unsigned>(*count / source_channels);
  unsigned converted_channels = std::min(source_channels, destination_channels);

  if (destination_sample_rate == source_sample_rate) {
    auto source_position = source_buffer.first(std::size_t(read_samples) * source_channels).begin();
    auto destination_position = destination_buffer.first(std::size_t(read_samples) * destination_channels).begin();
    for (unsigned sample = 0; sample < read_samples; ++sample) {
      for (unsigned channel = 0; channel < destination_channels; ++channel)
        *destination_position++ = source_position[std::min(channel, source_channels - 1)];
      source_position += source_channels;
    }
    return read_samples * destination_channels;
  }

  if (read_samples == 0)
    return 0;

  constexpr unsigned super_sample_factor = 8;
  for (unsigned destination_sample = 0; destination_sample < destination_samples; ++destination_sample) {
    std::array<unsigned, super_sample_factor> source_indices;
    unsigned sample_count = 0;
    int first_sample = 0;
    for (unsigned super_sample = 0; super_sample < super_sample_factor; ++super_sample) {
      unsigned source_sample =
        static_cast<unsigned>((std::uint64_t(destination_sample) * super_sample_factor + super_sample) * source_samples
                              / destination_samples / super_sample_factor);
      if (source_sample < read_samples) {
        unsigned source_offset = source_sample * source_channels;
        if (converted_channels > 1)
          source_indices[sample_count] = source_offset;
        first_sample += source_buffer[source_offset];
        ++sample_count;
      }
    }
    if (sample_count == 0)
      return destination_sample * destination_channels;

    auto destination_frame =
      destination_buffer.subspan(destination_sample * destination_channels, destination_channels);
    destination_frame[0] = static_cast<std::int16_t>(first_sample / static_cast<int>(sample_count));
    for (unsigned channel = 1; channel < converted_channels; ++channel) {
      int sample = 0;
      for (unsigned index = 0; index < sample_count; ++index) {
        if constexpr (Star::DebugEnabled) {
          Star::star_assert_failed((source_indices[index] + channel < source_buffer_size));
        }
        sample += source_buffer[source_indices[index] + channel];
      }
      sample /= static_cast<int>(sample_count);
      destination_frame[channel] = static_cast<std::int16_t>(sample);
    }
    if (destination_channels > converted_channels)
      std::ranges::fill(destination_frame.subspan(converted_channels), destination_frame[converted_channels - 1]);
  }
  return destination_samples * destination_channels;
}

String const& Audio::name() const {
  return m_name;
}

void Audio::set_name(String name) { m_name = std::move(name); }
}

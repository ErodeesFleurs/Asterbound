module;
import std;
import star.string;
import star.format;
import star.io_device;
//#define STAR_STREAM_AUDIO

namespace Star {

extern float const default_perceptual_range_db;
extern float const default_perceptual_boost_range_db;

float perceptual_to_amplitude(float perceptual, float normalized_max = 1.f, float range = default_perceptual_range_db,
                              float boost_range = default_perceptual_boost_range_db);

float amplitude_to_perceptual(float amp, float normalized_max = 1.f, float range = default_perceptual_range_db,
                              float boost_range = default_perceptual_boost_range_db);

class CompressedAudioImpl;
class UncompressedAudioImpl;

struct AudioExceptionTag {
  static constexpr std::string_view name() { return "AudioException"; }
};
using AudioException = StarError<AudioExceptionTag, StarException>;

// Simple class for reading audio files in ogg/vorbis and wav format.
// Reads and allows for decompression of a limited subset of ogg/vorbis.  Does
// not handle multiple bitstreams, sample rate or channel number changes.
// Entire stream is kept in memory by default. clone() explicitly creates an
// independent cursor while sharing immutable audio bytes.
class Audio {
public:
  [[nodiscard]] static std::expected<Audio, StarException> create(std::shared_ptr<IODevice> device, String name = "");
  Audio(Audio const&) = delete;
  Audio& operator=(Audio const&) = delete;
  Audio(Audio&& audio) noexcept;
  Audio& operator=(Audio&& audio) noexcept;
  ~Audio();
  [[nodiscard]] std::expected<Audio, StarException> clone() const;

  // This function returns the number of channels that this file has.  Channels
  // are static throughout file.
  unsigned channels() const;

  // This function returns the sample rate that this file has.  Sample rates
  // are static throughout file.
  unsigned sample_rate() const;

  // This function returns the playtime duration of the file.
  [[nodiscard]] std::expected<double, StarException> total_time() const;

  // Returns the total number of frames (one scalar sample per channel).
  [[nodiscard]] std::expected<std::uint64_t, StarException> total_samples() const;

  // This function returns true when the datastream or file being read from is
  // a vorbis compressed file.  False otherwise.
  bool compressed() const;

  // If compressed, permanently uncompresses audio for faster reading.
  // Decodes the complete clip while preserving the current frame position.
  // On failure the original decoder and its cursor are retained. Decoded bytes
  // are shared with all further clones.
  [[nodiscard]] std::expected<void, StarException> uncompress();

  // This function seeks the data stream to the given time in seconds.
  [[nodiscard]] std::expected<void, StarException> seek_time(double time);

  // Seeks to the given frame number.
  [[nodiscard]] std::expected<void, StarException> seek_sample(std::uint64_t sample);

  // This function converts the current offset of the file to the time value of
  // that offset in seconds.
  [[nodiscard]] std::expected<double, StarException> current_time() const;

  // Returns the current frame number, rounding down partial interleaved frames.
  [[nodiscard]] std::expected<std::uint64_t, StarException> current_sample() const;

  // Reads into 16 bit signed buffer with channels interleaved.  Returns total
  // number of samples read (counting each channel individually).  0 indicates
  // end of stream.
  [[nodiscard]] std::expected<std::size_t, StarException> read_partial(std::span<std::int16_t> buffer);

  // Same as read_partial, but repeats read attempting to fill buffer as much as
  // possible
  [[nodiscard]] std::expected<std::size_t, StarException> read(std::span<std::int16_t> buffer);

  // Read into a given buffer, while also converting into the given number of
  // channels at the given sample rate and playback velocity.  If the number of
  // channels in the file is higher, only populates lower channels, if it is
  // lower, the last channel is copied to the remaining channels.  Attempts to
  // fill the buffer as much as possible up to end of stream.  May fail to fill
  // an entire buffer depending on the destination_sample_rate / velocity /
  // available samples.
  [[nodiscard]] std::expected<std::size_t, StarException> resample(unsigned destination_channels,
                                                                   unsigned destination_sample_rate,
                                                                   std::span<std::int16_t> destination_buffer,
                                                                   double velocity = 1.0);

  String const& name() const;
  void set_name(String name);

private:
  Audio();
  // If audio is uncompressed, this will be null.
  std::unique_ptr<CompressedAudioImpl> m_compressed;
  std::unique_ptr<UncompressedAudioImpl> m_uncompressed;

  std::unique_ptr<std::int16_t[]> m_working_buffer;
  std::size_t m_working_capacity = 0;
  String m_name;
};

}

export module star.audio;

export namespace Star {
using ::Star::amplitude_to_perceptual;
using ::Star::Audio;
using ::Star::AudioException;
using ::Star::AudioExceptionTag;
using ::Star::default_perceptual_boost_range_db;
using ::Star::default_perceptual_range_db;
using ::Star::perceptual_to_amplitude;
}

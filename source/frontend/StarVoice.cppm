module;
import std;
import star.json;
import star.string;
import star.list;
import star.map;
import star.set;
import star.byte_array;
import star.array;
import star.bi_map;
import star.format;
import star.vector;
import star.game_types;
import star.thread;
import star.data_stream_devices;




struct OpusDecoder;
struct OpusEncoder;

namespace Star {
class Root;


inline String const VoiceBroadcastPrefix = std::string("Voice\0", sizeof("Voice\0") - 1);

struct VoiceExceptionTag {
  static constexpr std::string_view name() { return "VoiceException"; }
};
using VoiceException = StarError<VoiceExceptionTag, StarException>;

enum class VoiceInputMode : std::uint8_t { VoiceActivity, PushToTalk };
inline constexpr auto VoiceInputModeNames = sortedEnumNames(std::to_array<std::pair<VoiceInputMode, std::string_view>>(
  {{VoiceInputMode::VoiceActivity, "VoiceActivity"}, {VoiceInputMode::PushToTalk, "PushToTalk"}}));

enum class VoiceChannelMode: std::uint8_t { Mono = 1, Stereo = 2 };
inline constexpr auto VoiceChannelModeNames =
  sortedEnumNames(std::to_array<std::pair<VoiceChannelMode, std::string_view>>(
    {{VoiceChannelMode::Mono, "Mono"}, {VoiceChannelMode::Stereo, "Stereo"}}));

class VoiceAudioStream;
class ApplicationController;

struct VoiceAudioChunk {
  std::unique_ptr<std::int16_t[]> data;
  std::size_t remaining;
  std::size_t offset = 0;

  VoiceAudioChunk(std::unique_ptr<std::int16_t[]> ptr, std::size_t size)
    : data(std::move(ptr)), remaining(size) {}

  inline std::size_t takeSamples(std::vector<std::int16_t>& out, std::size_t count) {
    std::size_t toRead = std::min<std::size_t>(count, remaining);
    auto samples = std::span<std::int16_t>(data.get(), offset + remaining).subspan(offset, toRead);
    out.insert(out.end(), samples.begin(), samples.end());
    offset += toRead;
    remaining -= toRead;
    return toRead;
  }

  //this one's unsafe
  inline std::int16_t takeSample() {
    --remaining;
    return data[offset++];
  }

  inline bool exhausted() { return remaining == 0; }
};


class Voice {
public:
  // Individual speakers are represented by their connection ID.
  using SpeakerId = ConnectionId;

  class Speaker {
  public:
    SpeakerId speakerId = 0;
    EntityId entityId = 0;

    Vec2F position = Vec2F();
    String name = "Unnamed";

    std::unique_ptr<::OpusDecoder, void(*)(::OpusDecoder*)> decoderMono;
    std::unique_ptr<::OpusDecoder, void(*)(::OpusDecoder*)> decoderStereo;
    std::shared_ptr<VoiceAudioStream> audioStream;
    std::mutex mutex;

    std::chrono::time_point<std::chrono::steady_clock, std::chrono::milliseconds> lastReceiveTime{};
    std::chrono::time_point<std::chrono::steady_clock, std::chrono::milliseconds> lastPlayTime{};
    float smoothDb = -96.0f;
    Array<float, 10> dbHistory = Array<float, 10>::filled(0);

    std::atomic<bool> muted = false;
    std::atomic<bool> playing = 0;
    std::atomic<float> decibelLevel = -96.0f;
    std::atomic<float> volume = 1.0f;
    Vec2F channelVolumes = Vec2F::filled(1.f);

    unsigned int minimumPlaySamples = 4096;

    [[nodiscard]] static std::expected<std::shared_ptr<Speaker>, StarException> create(SpeakerId speakerId);
    Json toJson() const;
  private:
    Speaker(SpeakerId speakerId);
  };

  [[nodiscard]] static std::expected<std::shared_ptr<Voice>, StarException> create(Root& root, std::shared_ptr<ApplicationController> appController);
  ~Voice();

  Voice(Voice const&) = delete;
  Voice& operator=(Voice const&) = delete;

  [[nodiscard]] std::expected<void, StarException> init();

  [[nodiscard]] std::expected<void, StarException> loadJson(Json const& config, bool skipSave = false);
  Json saveJson() const;

  [[nodiscard]] std::expected<void, StarException> save() const;
  void scheduleSave();

  // Sets the local speaker ID and returns the local speaker. Must be called upon loading into a world.
  std::shared_ptr<Speaker> setLocalSpeaker(SpeakerId speakerId);
  std::shared_ptr<Speaker> localSpeaker();
  [[nodiscard]] std::expected<std::shared_ptr<Speaker>, StarException> speaker(SpeakerId speakerId);
  HashMap<SpeakerId, std::shared_ptr<Speaker>>& speakers();
  List<std::shared_ptr<Voice::Speaker>> sortedSpeakers(bool onlyPlaying);
  void clearSpeakers();

  // Called when receiving input audio data from SDL, on its own thread.
  void readAudioData(std::span<std::uint8_t> stream);

  // Called to mix voice audio with the game.
  void mix(std::span<std::int16_t> buffer, std::size_t frames, unsigned channels);

  using PositionalAttenuationFunction = std::function<std::expected<float, StarException>(unsigned, Vec2F, float)>;
  [[nodiscard]] std::expected<void, StarException> update(float dt, PositionalAttenuationFunction positionalAttenuationFunction = {});

  [[nodiscard]] std::expected<void, StarException> setDeviceName(std::optional<String> device);
  StringList availableDevices();

  [[nodiscard]] std::expected<int, StarException> send(DataStreamBuffer& out, std::size_t budget = 0);
  [[nodiscard]] std::expected<bool, StarException> receive(std::shared_ptr<Speaker> speaker, std::string_view view);

  // Must be called every frame with input state, expires after 1s.
  void setInput(bool input = true);

  inline int encoderChannels() const { return static_cast<int>(m_channelMode); }

  [[nodiscard]] static std::expected<std::unique_ptr<OpusDecoder, void(*)(OpusDecoder*)>, StarException> createDecoder(int channels);
  [[nodiscard]] static std::expected<std::unique_ptr<OpusEncoder, void(*)(OpusEncoder*)>, StarException> createEncoder(int channels);
private:
  Voice(Root& root, std::shared_ptr<ApplicationController> appController);
  [[nodiscard]] std::expected<bool, StarException> decode(std::shared_ptr<Speaker> const& speaker, std::string_view view);
  Root& m_root;
  [[nodiscard]] std::expected<void, StarException> resetEncoder();
  [[nodiscard]] std::expected<void, StarException> resetDevice();
  [[nodiscard]] std::expected<void, StarException> openDevice();
  void closeDevice();
  inline bool shouldEnableInput() const { return m_enabled && m_inputEnabled; }

  bool playSpeaker(std::shared_ptr<Speaker> const& speaker, int channels);

  [[nodiscard]] std::expected<void, StarException> thread(std::stop_token stopToken);

  SpeakerId m_speakerId = 0;
  std::shared_ptr<Speaker> m_clientSpeaker;
  HashMap<SpeakerId, std::shared_ptr<Speaker>> m_speakers;

  std::mutex m_activeSpeakersMutex;
  HashSet<std::shared_ptr<Speaker>> m_activeSpeakers;



  std::unique_ptr<::OpusEncoder, void(*)(::OpusEncoder*)> m_encoder;
  
  float m_outputVolume = 1.0f;
  float m_inputVolume = 1.0f;

  float m_outputAmplitude = 1.0f;
  float m_inputAmplitude = 1.0f;

  float m_threshold = -50.0f;
  
  std::chrono::time_point<std::chrono::steady_clock, std::chrono::milliseconds> m_lastSentTime{};
  std::chrono::time_point<std::chrono::steady_clock, std::chrono::milliseconds> m_lastInputTime{};
  std::chrono::time_point<std::chrono::steady_clock, std::chrono::milliseconds> m_lastThresholdTime{};
  std::chrono::time_point<std::chrono::steady_clock, std::chrono::milliseconds> m_nextSaveTime{};
  bool m_enabled = true;
  bool m_inputEnabled = false;
  bool m_loopback = false;

  int m_deviceChannels = 1;
  bool m_deviceOpen = false;
  std::optional<String> m_deviceName;
  VoiceInputMode m_inputMode;
  VoiceChannelMode m_channelMode;
  unsigned m_bitrate = 0;

  ThreadFunction<void> m_thread;
  std::mutex m_threadMutex;
  std::condition_variable m_threadCond;
  std::stop_source m_threadStopSource{std::nostopstate};

  std::vector<std::int16_t> m_decodeBuffer;
  std::vector<std::int16_t> m_resampleBuffer;
  std::vector<std::int16_t> m_mixFinalBuffer;
  std::vector<std::int16_t> m_mixSpeakerBuffer;
  std::vector<std::int32_t> m_mixSharedBuffer;

  std::shared_ptr<ApplicationController> m_applicationController;


  std::mutex m_encodeMutex;
  std::vector<ByteArray> m_encodedChunks;
  std::size_t m_encodedChunksLength = 0;

  std::mutex m_captureMutex;
  std::queue<VoiceAudioChunk> m_capturedChunks;
  std::size_t m_capturedChunksFrames = 0;
};
  
}

export module star.voice;

export namespace Star {
  using ::Star::VoiceExceptionTag;
  using ::Star::VoiceException;
  using ::Star::VoiceInputMode;
  using ::Star::VoiceInputModeNames;
  using ::Star::VoiceChannelMode;
  using ::Star::VoiceChannelModeNames;
  using ::Star::Voice;
  using ::Star::VoiceAudioStream;
  using ::Star::ApplicationController;
  using ::Star::VoiceAudioChunk;
  using ::Star::VoiceBroadcastPrefix;
}

export using ::OpusDecoder;
export using ::OpusEncoder;

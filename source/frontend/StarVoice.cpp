#include "opus/opus.h"
#include "SDL3/SDL.h"
import std;
import star.json;
import star.json_extra;
import star.bi_map;
import star.format;
import star.vector;
import star.game_types;
import star.thread;
import star.data_stream_devices;
import star.logging;
import star.audio;
import star.ordered_map;
import star.map;
import star.string;
import star.io_device;
import star.list;
import star.image_processing;
import star.hash;
import star.data_stream;
import star.string_view;
import star.directives;
import star.asset_path;
import star.ref_ptr;
import star.listener;
import star.version;

import star.byte_array;
import star.bytes;
import star.memory;
struct OpusDecoder;
struct OpusEncoder;
import star.voice;
import star.input_event;
import star.application;
import star.statistics_service;
import star.p2p_networking_service;
import star.user_generated_content_service;
import star.desktop_service;
import star.image;
import star.application_controller;
import star.asset_source;
import star.assets;
import star.root_base;
import star.configuration;
import star.root;


constexpr int VOICE_SAMPLE_RATE = 48000;
constexpr int VOICE_FRAME_SIZE = 960;

constexpr int VOICE_MAX_PACKET_SIZE = 3 * 1276;

constexpr std::uint16_t VOICE_VERSION = 1;

import star.math_common;
namespace Star {

inline float getAudioChunkLoudness(std::span<std::int16_t const> data, float volume) {
  if (data.empty())
    return 0.f;

  double rms = 0.;
  for (std::size_t i = 0; i != data.size(); ++i) {
    float sample = (static_cast<float>(data[i]) / 32767.f) * volume;
    rms += static_cast<double>(sample * sample);
  }

  float fRms = std::sqrtf(static_cast<float>(rms / data.size()));

  if (fRms > 0)
    return std::clamp<float>(20.f * std::log10f(fRms), -127.f, 0.f);
  else
    return -127.f;
}

float getAudioLoudness(std::span<std::int16_t const> data, float volume = 1.0f) {
  constexpr std::size_t CHUNK_SIZE = 50;

  float highest = -127.f;
  for (std::size_t i = 0; i < data.size(); i += CHUNK_SIZE) {
    float level = getAudioChunkLoudness(data.subspan(i, std::min(CHUNK_SIZE, data.size() - i)), volume);
    if (level > highest)
      highest = level;
  }

  return highest;
}

class VoiceAudioStream {
public:
  // TODO: This should really be a ring buffer instead.
  std::queue<std::int16_t> samples;
  std::unique_ptr<SDL_AudioStream, decltype(&SDL_DestroyAudioStream)> sdlAudioStreamMono{nullptr, SDL_DestroyAudioStream};
  std::unique_ptr<SDL_AudioStream, decltype(&SDL_DestroyAudioStream)> sdlAudioStreamStereo{nullptr, SDL_DestroyAudioStream};
  std::mutex mutex;

  [[nodiscard]] static std::expected<std::shared_ptr<VoiceAudioStream>, StarException> create() {
    auto audio = std::make_shared<VoiceAudioStream>();
    SDL_AudioSpec srcSpec = {SDL_AUDIO_S16LE, 1, 48000};
    SDL_AudioSpec dstSpec = {SDL_AUDIO_S16,   1, 44100};
    audio->sdlAudioStreamMono.reset(SDL_CreateAudioStream(&srcSpec, &dstSpec));
    if (!audio->sdlAudioStreamMono)
      return std::unexpected(VoiceException::format("Could not create mono audio stream: {}", SDL_GetError()));
    srcSpec.channels = dstSpec.channels = 2;
    audio->sdlAudioStreamStereo.reset(SDL_CreateAudioStream(&srcSpec, &dstSpec));
    if (!audio->sdlAudioStreamStereo)
      return std::unexpected(VoiceException::format("Could not create stereo audio stream: {}", SDL_GetError()));
    return audio;
  }

  inline std::int16_t take() {
    std::int16_t sample = 0;
    if (!samples.empty()) {
      sample = samples.front();
      samples.pop();
    }
    return sample;
  }

  [[nodiscard]] std::expected<void, StarException> resample(std::span<std::int16_t const> in, std::vector<std::int16_t>& out, bool mono) {
    auto& stream = mono ? *sdlAudioStreamMono : *sdlAudioStreamStereo;
    if (!SDL_PutAudioStreamData(&stream, in.data(), in.size_bytes()))
      return std::unexpected(VoiceException::format("Could not resample voice data: {}", SDL_GetError()));
    int available = SDL_GetAudioStreamAvailable(&stream);
    if (available < 0)
      return std::unexpected(VoiceException::format("Could not inspect resampled voice data: {}", SDL_GetError()));
    out.resize(available / sizeof(std::int16_t));
    if (available) {
      int received = SDL_GetAudioStreamData(&stream, out.data(), available);
      if (received < 0)
        return std::unexpected(VoiceException::format("Could not read resampled voice data: {}", SDL_GetError()));
      out.resize(received / sizeof(std::int16_t));
    }
    return {};
  }
};

Voice::Speaker::Speaker(SpeakerId id)
  : decoderMono(nullptr, opus_decoder_destroy), decoderStereo(nullptr, opus_decoder_destroy) {
  speakerId = id;
}

std::expected<std::shared_ptr<Voice::Speaker>, StarException> Voice::Speaker::create(SpeakerId id) {
  auto speaker = std::shared_ptr<Speaker>(new Speaker(id));
  auto mono = createDecoder(1);
  if (!mono)
    return std::unexpected(std::move(mono.error()));
  speaker->decoderMono = std::move(*mono);
  auto stereo = createDecoder(2);
  if (!stereo)
    return std::unexpected(std::move(stereo.error()));
  speaker->decoderStereo = std::move(*stereo);
  auto stream = VoiceAudioStream::create();
  if (!stream)
    return std::unexpected(std::move(stream.error()));
  speaker->audioStream = std::move(*stream);
  return speaker;
}

Json Voice::Speaker::toJson() const {
  return JsonObject{{"speakerId", speakerId},
                    {"entityId", entityId},
                    {"name", name},
                    {"playing", static_cast<bool>(playing)},
                    {"muted", static_cast<bool>(muted)},
                    {"decibels", static_cast<float>(decibelLevel)},
                    {"smoothDecibels", static_cast<float>(smoothDb)},
                    {"position", jsonFromVec2F(position)}};
}

Voice::Voice(Root& root, std::shared_ptr<ApplicationController> appController)
  : m_root(root), m_encoder(nullptr, opus_encoder_destroy) {

  m_inputMode = VoiceInputMode::PushToTalk;
  m_channelMode = VoiceChannelMode::Mono;
  m_applicationController = std::move(appController);
}

std::expected<std::shared_ptr<Voice>, StarException> Voice::create(Root& root, std::shared_ptr<ApplicationController> appController) {
  auto voice = std::shared_ptr<Voice>(new Voice(root, std::move(appController)));
  auto speaker = Speaker::create(voice->m_speakerId);
  if (!speaker)
    return std::unexpected(std::move(speaker.error()));
  voice->m_clientSpeaker = std::move(*speaker);
  voice->m_threadStopSource = std::stop_source{};
  auto thread = Thread::invoke("Voice::thread", std::mem_fn(&Voice::thread), std::ref(*voice), voice->m_threadStopSource.get_token());
  if (!thread)
    return std::unexpected(std::move(thread.error()));
  voice->m_thread = std::move(*thread);
  return voice;
}

Voice::~Voice() {
  // Detach and finish capture callbacks before stopping their consumer.
  closeDevice();

  {
    std::lock_guard locker(m_threadMutex);
    m_threadStopSource.request_stop();
    m_threadCond.notify_all();
  }

  if (m_thread) {
    if (auto result = m_thread.finish(); !result) {
      auto logged = m_root.logger().error("Voice: Error finishing capture thread: {}", result.error().what());
      if (!logged) EmergencyLog::error("Voice: Error logging capture thread failure: {}", logged.error().what());
    }
  }
  if (m_nextSaveTime != std::chrono::time_point<std::chrono::steady_clock, std::chrono::milliseconds>{}) {
    if (auto result = save(); !result) {
      auto logged = m_root.logger().error("Voice: Error saving settings: {}", result.error().what());
      if (!logged) EmergencyLog::error("Voice: Error logging save failure: {}", logged.error().what());
    }
  }
}

std::expected<void, StarException> Voice::init() {
  if (auto result = resetEncoder(); !result)
    return result;
  if (shouldEnableInput())
    return openDevice();
  return {};
}


template <typename T>
inline bool change(T& value, T newValue, bool& out) {
  bool changed = value != newValue;
  out |= changed;
  value = std::move(newValue);
  return changed;
}

std::expected<void, StarException> Voice::loadJson(Json const& config, bool skipSave) {
  auto enabled = config.getBool("enabled", m_enabled);
  if (!enabled) return std::unexpected(std::move(enabled.error()));
  auto inputEnabled = config.getBool("inputEnabled", m_inputEnabled);
  if (!inputEnabled) return std::unexpected(std::move(inputEnabled.error()));
  auto deviceName = config.optString("deviceName");
  if (!deviceName) return std::unexpected(std::move(deviceName.error()));
  auto threshold = config.getFloat("threshold", m_threshold);
  if (!threshold) return std::unexpected(std::move(threshold.error()));
  auto inputVolume = config.getFloat("inputVolume", m_inputVolume);
  if (!inputVolume) return std::unexpected(std::move(inputVolume.error()));
  auto outputVolume = config.getFloat("outputVolume", m_outputVolume);
  if (!outputVolume) return std::unexpected(std::move(outputVolume.error()));
  auto loopback = config.getBool("loopback", m_loopback);
  if (!loopback) return std::unexpected(std::move(loopback.error()));
  auto inputModeName = config.optString("inputMode");
  if (!inputModeName) return std::unexpected(std::move(inputModeName.error()));
  auto channelModeName = config.optString("channelMode");
  if (!channelModeName) return std::unexpected(std::move(channelModeName.error()));
  auto inputMode = m_inputMode;
  if (*inputModeName) {
    auto parsed = enumValue<VoiceInputModeNames>(**inputModeName);
    if (!parsed) return std::unexpected(std::move(parsed.error()));
    inputMode = *parsed;
  }
  auto channelMode = m_channelMode;
  if (*channelModeName) {
    auto parsed = enumValue<VoiceChannelModeNames>(**channelModeName);
    if (!parsed) return std::unexpected(std::move(parsed.error()));
    channelMode = *parsed;
  }
  auto bitrate = config.opt("bitrate");
  if (!bitrate) return std::unexpected(std::move(bitrate.error()));
  unsigned newBitrate = m_bitrate;
  if (*bitrate) {
    newBitrate = 0;
    if ((**bitrate).canConvert(Json::Type::Int)) {
      auto value = (**bitrate).toUInt();
      if (!value) return std::unexpected(std::move(value.error()));
      newBitrate = std::clamp(static_cast<unsigned>(*value), 6000u, 510000u);
    }
  }
  bool changed = false;
  bool wasEnabled = shouldEnableInput();
  m_enabled = *enabled;
  m_inputEnabled = *inputEnabled;
  bool shouldResetDevice = wasEnabled != shouldEnableInput();
  changed |= shouldResetDevice;
  auto hasDeviceName = config.contains("deviceName");
  if (!hasDeviceName) return std::unexpected(std::move(hasDeviceName.error()));
  if (*hasDeviceName && change(m_deviceName, std::move(*deviceName), changed))
    shouldResetDevice = true;
  m_threshold = *threshold;
  m_inputAmplitude = perceptualToAmplitude(m_inputVolume = *inputVolume);
  m_outputAmplitude = perceptualToAmplitude(m_outputVolume = *outputVolume);
  if (change(m_loopback, *loopback, changed))
    m_clientSpeaker->playing = false;
  if (change(m_inputMode, inputMode, changed))
    m_lastInputTime = {};
  bool shouldResetEncoder = change(m_bitrate, newBitrate, changed);
  if (change(m_channelMode, channelMode, changed)) {
    closeDevice();
    shouldResetEncoder = shouldResetDevice = true;
  }
  if (shouldResetEncoder) {
    if (auto result = resetEncoder(); !result) return result;
  }
  if (shouldResetDevice) {
    if (auto result = resetDevice(); !result) return result;
  }
  if (changed && !skipSave)
    scheduleSave();
  return {};
}



Json Voice::saveJson() const {
  return JsonObject{
    {"enabled",      m_enabled},
    {"deviceName",   m_deviceName ? Json(*m_deviceName) : Json()},
    {"inputEnabled", m_inputEnabled},
    {"threshold",    m_threshold},
    {"inputVolume",  m_inputVolume},
    {"outputVolume", m_outputVolume},
    {"inputMode",    m_inputMode == VoiceInputMode::PushToTalk ? std::string_view("PushToTalk") : std::string_view("VoiceActivity")},
    {"channelMode",  m_channelMode == VoiceChannelMode::Mono ? std::string_view("Mono") : std::string_view("Stereo")},
    {"loopback",     m_loopback},
    {"version",      1}
  };
}

std::expected<void, StarException> Voice::save() const {
  auto configuration = m_root.configuration();
  if (!configuration)
    return std::unexpected(std::move(configuration.error()));
  return (*configuration)->set("voice", saveJson());
}

void Voice::scheduleSave() {
  if (m_nextSaveTime == std::chrono::time_point<std::chrono::steady_clock, std::chrono::milliseconds>{})
    m_nextSaveTime = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() + std::chrono::microseconds(500)) + std::chrono::milliseconds(2000);
}

std::shared_ptr<Voice::Speaker> Voice::setLocalSpeaker(SpeakerId speakerId) {
  if (m_speakers.contains(m_speakerId))
    m_speakers.remove(m_speakerId);

  m_clientSpeaker->speakerId = m_speakerId = speakerId;
  return m_speakers.insert(m_speakerId, m_clientSpeaker).first->second;
}

std::shared_ptr<Voice::Speaker> Voice::localSpeaker() {
  return m_clientSpeaker;
}

std::expected<std::shared_ptr<Voice::Speaker>, StarException> Voice::speaker(SpeakerId speakerId) {
  if (m_speakerId == speakerId)
    return m_clientSpeaker;
  if (auto speaker = m_speakers.find_ref(speakerId))
    return speaker->get();
  auto created = Speaker::create(speakerId);
  if (!created)
    return std::unexpected(std::move(created.error()));
  return m_speakers.emplace(speakerId, std::move(*created)).first->second;
}

HashMap<Voice::SpeakerId, std::shared_ptr<Voice::Speaker>>& Voice::speakers() {
  return m_speakers;
}

List<std::shared_ptr<Voice::Speaker>> Voice::sortedSpeakers(bool onlyPlaying) {
  List<std::shared_ptr<Speaker>> result;

  auto sorter = [](std::shared_ptr<Speaker> const& a, std::shared_ptr<Speaker> const& b) -> bool {
    if (a->lastPlayTime != b->lastPlayTime)
      return a->lastPlayTime < b->lastPlayTime;
    else
      return a->speakerId < b->speakerId;
  };

  for (auto&& p : m_speakers) {
    if (!onlyPlaying || p.second->playing)
      result.insertSorted(p.second, sorter);
  }

  return result;
}

void Voice::clearSpeakers() {
  auto it = m_speakers.begin();
  while (it != m_speakers.end()) {
    if (it->second == m_clientSpeaker)
      it = ++it;
    else
      it = m_speakers.erase(it);
  }
}

void Voice::readAudioData(std::span<std::uint8_t> stream) {
  auto now = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() + std::chrono::microseconds(500));
  bool active = m_encoder && m_encodedChunksLength < 2048
       && (m_inputMode == VoiceInputMode::VoiceActivity || now < m_lastInputTime);

  std::size_t sampleCount = stream.size() / sizeof(std::int16_t);
  // The application capture boundary supplies native signed 16-bit samples.
  std::span<std::int16_t const> samples(reinterpret_cast<std::int16_t const*>(stream.data()), sampleCount);

  if (active) {
    float decibels = getAudioLoudness(samples);

    if (m_inputMode == VoiceInputMode::VoiceActivity) {
      if (decibels > m_threshold)
        m_lastThresholdTime = now;
      active = now - m_lastThresholdTime < std::chrono::milliseconds(50);
    }
  }

  m_clientSpeaker->decibelLevel = getAudioLoudness(samples, m_inputAmplitude);

  if (!m_loopback) {
    if (active && !m_clientSpeaker->playing)
      m_clientSpeaker->lastPlayTime = now;

    m_clientSpeaker->playing = active;
  }

  std::lock_guard captureLock(m_captureMutex);
  if (active) {
    auto data = std::make_unique_for_overwrite<std::int16_t[]>(sampleCount);
    std::memcpy(data.get(), stream.data(), sampleCount * sizeof(std::int16_t));
    m_capturedChunks.emplace(std::move(data), sampleCount);
    m_capturedChunksFrames += sampleCount / m_deviceChannels;
    m_threadCond.notify_one();
  }
  else { // Clear out any residual data so they don't manifest at the start of the next encode, whenever that is
    while (!m_capturedChunks.empty())
      m_capturedChunks.pop();

    m_capturedChunksFrames = 0;
  }
}

void Voice::mix(std::span<std::int16_t> buffer, std::size_t frameCount, unsigned channels) {
  std::size_t samples = frameCount * channels;
  auto& finalBuffer = m_mixFinalBuffer;
  auto& speakerBuffer = m_mixSpeakerBuffer;
  auto& sharedBuffer = m_mixSharedBuffer; // int32 to reduce clipping
  speakerBuffer.resize(samples);
  sharedBuffer.resize(samples);

  bool mix = false;
  {
    std::lock_guard lock(m_activeSpeakersMutex);
    auto it = m_activeSpeakers.begin();
    while (it != m_activeSpeakers.end()) {
      std::shared_ptr<Speaker> const& speaker = *it;
      auto& audio = *speaker->audioStream;
      std::lock_guard audioLock(audio.mutex);
      if (speaker->playing && !audio.samples.empty()) {
        for (std::size_t i = 0; i != samples; ++i)
          speakerBuffer[i] = audio.take();

        if (speaker != m_clientSpeaker)
          speaker->decibelLevel = getAudioLoudness(speakerBuffer);

        if (!speaker->muted) {
          mix = true;

          float volume = speaker->volume;
          std::array<float, 2> levels;
          {
            std::lock_guard locker(speaker->mutex);
            std::ranges::copy(speaker->channelVolumes, levels.begin());
          }
          for (std::size_t i = 0; i != samples; ++i)
            sharedBuffer[i] += static_cast<std::int32_t>(speakerBuffer[i]) * levels[i % 2] * volume;
        }
        ++it;
      }
      else {
        speaker->playing = false;
        if (speaker != m_clientSpeaker)
          speaker->decibelLevel = -96.0f;
        it = m_activeSpeakers.erase(it);
      }
    }
  }

  if (mix) {
    finalBuffer.resize(sharedBuffer.size(), 0);

    float vol = m_outputAmplitude;
    for (std::size_t i = 0; i != sharedBuffer.size(); ++i)
      finalBuffer[i] = static_cast<std::int16_t>(std::clamp<int>(sharedBuffer[i] * vol, INT16_MIN, INT16_MAX));

    SDL_MixAudio(reinterpret_cast<Uint8*>(buffer.data()), reinterpret_cast<Uint8 const*>(finalBuffer.data()), SDL_AUDIO_S16LE, finalBuffer.size() * sizeof(std::int16_t), 1.0f);
    std::memset(sharedBuffer.data(), 0, sharedBuffer.size() * sizeof(std::int32_t));
  }
}

std::expected<void, StarException> Voice::update(float, PositionalAttenuationFunction positionalAttenuationFunction) {
  for (auto&& entry : m_speakers) {
    if (std::shared_ptr<Speaker>& speaker = entry.second) {
      Vec2F newChannelVolumes{1.0f, 1.0f};
      if (positionalAttenuationFunction) {
        auto left = positionalAttenuationFunction(0, speaker->position, 1.0f);
        if (!left) return std::unexpected(std::move(left.error()));
        auto right = positionalAttenuationFunction(1, speaker->position, 1.0f);
        if (!right) return std::unexpected(std::move(right.error()));
        newChannelVolumes = Vec2F{1.0f - *left, 1.0f - *right};
      }
      {
        std::lock_guard locker(speaker->mutex);
        speaker->channelVolumes = newChannelVolumes;
      }
      auto& dbHistory = speaker->dbHistory;
      std::memmove(&dbHistory[1], &dbHistory[0], (dbHistory.size() - 1) * sizeof(float));
      dbHistory[0] = speaker->decibelLevel;
      float smoothDb = 0.0f;
      for (float dB : dbHistory)
        smoothDb += dB;

      speaker->smoothDb = smoothDb / dbHistory.size();
    }
  }

  if (m_nextSaveTime != std::chrono::time_point<std::chrono::steady_clock, std::chrono::milliseconds>{} && std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() + std::chrono::microseconds(500)) > m_nextSaveTime) {
    m_nextSaveTime = {};
    if (auto result = save(); !result)
      return result;
  }
  return {};
}


std::expected<void, StarException> Voice::setDeviceName(std::optional<String> deviceName) {
  if (m_deviceName == deviceName)
    return {};
  m_deviceName = std::move(deviceName);
  if (m_deviceOpen)
    return openDevice();
  return {};
}

StringList Voice::availableDevices() {
  StringList list;
  int numDevices;
  std::unique_ptr<SDL_AudioDeviceID, decltype(&SDL_free)> devices(SDL_GetAudioRecordingDevices(&numDevices), SDL_free);
  if (devices) {
    list.reserve(numDevices);
    for (auto device : std::span<SDL_AudioDeviceID const>(devices.get(), numDevices))
      list.emplace_back(std::string(std::string_view(SDL_GetAudioDeviceName(device))));
  }
  list.sort();
  return list;
}

std::expected<int, StarException> Voice::send(DataStreamBuffer& out, std::size_t budget) {
  out.setByteOrder(ByteOrder::LittleEndian);
  if (auto result = out.write<std::uint16_t>(VOICE_VERSION); !result)
    return std::unexpected(std::move(result.error()));

  std::unique_lock encodeLock(m_encodeMutex);

  if (m_encodedChunks.empty())
    return 0;

  std::vector<ByteArray> encodedChunks = std::move(m_encodedChunks);
  m_encodedChunksLength = 0;

  encodeLock.unlock();

  for (auto& chunk : encodedChunks) {
    if (auto result = out.write<std::uint32_t>(chunk.size()); !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = out.write_bytes(chunk); !result)
      return std::unexpected(std::move(result.error()));
    if (budget && (budget -= std::min<std::size_t>(budget, chunk.size())) == 0)
      break;
  }

  m_lastSentTime = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() + std::chrono::microseconds(500));
  if (m_loopback) {
    auto received = receive(m_clientSpeaker, std::string_view(out.span()));
    if (!received) return std::unexpected(std::move(received.error()));
  }
  return 1;
}

std::expected<bool, StarException> Voice::receive(std::shared_ptr<Speaker> speaker, std::string_view view) {
  if (!m_enabled || !speaker || view.empty())
    return false;

  auto decoded = decode(speaker, view);
  if (!decoded) {
    auto logged = m_root.logger().error("Voice: Error receiving voice data for speaker #{} ('{}'): {}", speaker->speakerId, speaker->name, decoded.error().what());
    if (!logged) return std::unexpected(std::move(logged.error()));
    return false;
  }
  return *decoded;
}

std::expected<bool, StarException> Voice::decode(std::shared_ptr<Speaker> const& speaker, std::string_view view) {
    DataStreamExternalBuffer reader{std::span<char const>(view)};
    reader.setByteOrder(ByteOrder::LittleEndian);
    auto version = reader.read<std::uint16_t>();
    if (!version) return std::unexpected(std::move(version.error()));
    if (*version > VOICE_VERSION)
      return false;
    while (true) {
      auto atEnd = reader.at_end();
      if (!atEnd) return std::unexpected(std::move(atEnd.error()));
      if (*atEnd) break;
      auto opusLength = reader.read<std::uint32_t>();
      if (!opusLength) return std::unexpected(std::move(opusLength.error()));
      auto position = reader.pos();
      if (!position) return std::unexpected(std::move(position.error()));
      if (*position > reader.size() || *opusLength > reader.size() - *position)
        return std::unexpected(VoiceException("Opus packet length goes past end of buffer", false));
      auto packet = reader.span().subspan(*position, *opusLength);
      if (auto result = reader.seek(*opusLength, IOSeek::Relative); !result)
        return std::unexpected(std::move(result.error()));
      if (!*opusLength)
        return std::unexpected(VoiceException("Empty Opus packet", false));
      int channels = opus_packet_get_nb_channels(reinterpret_cast<unsigned char const*>(packet.data()));
      if (channels == OPUS_INVALID_PACKET)
        continue;
      bool mono = channels == 1;
      OpusDecoder* decoder = mono ? speaker->decoderMono.get() : speaker->decoderStereo.get();
      int samples = opus_decoder_get_nb_samples(decoder, reinterpret_cast<unsigned char const*>(packet.data()), packet.size());
      if (samples < 0)
        return std::unexpected(VoiceException::format("Decoder error: {}", opus_strerror(samples)));

      m_decodeBuffer.resize(samples * static_cast<std::size_t>(channels));

      int decodedSamples = opus_decode(decoder, reinterpret_cast<unsigned char const*>(packet.data()), packet.size(), m_decodeBuffer.data(), samples, 0);
      if (decodedSamples < 0)
        return std::unexpected(VoiceException::format("Decoder error: {}", opus_strerror(decodedSamples)));
      if (!decodedSamples)
        return true;
      auto resampled = speaker->audioStream->resample(
        std::span<std::int16_t const>(m_decodeBuffer).first(static_cast<std::size_t>(decodedSamples) * channels),
        m_resampleBuffer, mono);
      if (!resampled)
        return std::unexpected(std::move(resampled.error()));

      {
        std::lock_guard lock(speaker->audioStream->mutex);
        auto& samples = speaker->audioStream->samples;

        auto now = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() + std::chrono::microseconds(500));
        if (now - speaker->lastReceiveTime < std::chrono::milliseconds(1000)) {
          auto limit = static_cast<std::size_t>(speaker->minimumPlaySamples) + 22050;
          if (samples.size() > limit) { // skip ahead if we're getting too far
            for (std::size_t i = samples.size(); i >= limit; --i)
              samples.pop();
          }
        }
        else
          samples = std::queue<std::int16_t>();

        speaker->lastReceiveTime = now;

        if (mono) {
          for (std::int16_t sample : m_resampleBuffer) {
            samples.push(sample);
            samples.push(sample);
          }
        }
        else {
          for (std::int16_t sample : m_resampleBuffer)
            samples.push(sample);
        }
      }
      playSpeaker(speaker, channels);
    }
    return true;
}

void Voice::setInput(bool input) {
  m_lastInputTime = (m_deviceOpen && input) ? std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() + std::chrono::microseconds(500)) + std::chrono::milliseconds(1000) : std::chrono::time_point<std::chrono::steady_clock, std::chrono::milliseconds>{};
}

std::expected<std::unique_ptr<OpusDecoder, void(*)(OpusDecoder*)>, StarException> Voice::createDecoder(int channels) {
  int error;
  OpusDecoder* decoder = opus_decoder_create(VOICE_SAMPLE_RATE, channels, &error);
  if (error != OPUS_OK)
    return std::unexpected(VoiceException::format("Could not create decoder: {}", opus_strerror(error)));
  else
    return std::unique_ptr<OpusDecoder, void(*)(OpusDecoder*)>(decoder, opus_decoder_destroy);
}

std::expected<std::unique_ptr<OpusEncoder, void(*)(OpusEncoder*)>, StarException> Voice::createEncoder(int channels) {
  int error;
  OpusEncoder* encoder = opus_encoder_create(VOICE_SAMPLE_RATE, channels, OPUS_APPLICATION_AUDIO, &error);
  if (error != OPUS_OK)
    return std::unexpected(VoiceException::format("Could not create encoder: {}", opus_strerror(error)));
  else
    return std::unique_ptr<OpusEncoder, void(*)(OpusEncoder*)>(encoder, opus_encoder_destroy);
}

std::expected<void, StarException> Voice::resetEncoder() {
  int channels = encoderChannels();
  std::lock_guard locker(m_threadMutex);
  auto created = createEncoder(channels);
  if (!created) return std::unexpected(std::move(created.error()));
  auto encoder = std::move(*created);
  int bitrate = m_bitrate > 0 ? static_cast<int>(m_bitrate) : (channels == 2 ? 50000 : 24000);
  int error = opus_encoder_ctl(encoder.get(), OPUS_SET_BITRATE(bitrate));
  if (error != OPUS_OK)
    return std::unexpected(VoiceException::format("Could not configure encoder: {}", opus_strerror(error)));
  m_encoder = std::move(encoder);
  return {};
}

std::expected<void, StarException> Voice::resetDevice() {
  closeDevice();
  if (shouldEnableInput())
    return openDevice();
  return {};
}

std::expected<void, StarException> Voice::openDevice() {
  if (m_deviceOpen)
    return {};
  closeDevice();

  std::uint32_t deviceId = SDL_AUDIO_DEVICE_DEFAULT_RECORDING;
  if (m_deviceName) {
    int numDevices;
    std::unique_ptr<SDL_AudioDeviceID, decltype(&SDL_free)> devices(SDL_GetAudioRecordingDevices(&numDevices), SDL_free);
    if (devices) {
      for (auto device : std::span<SDL_AudioDeviceID const>(devices.get(), numDevices)) {
        if (*m_deviceName == std::string_view(SDL_GetAudioDeviceName(device))) {
          deviceId = device;
          break;
        }
      }
    }
  }
  auto opened = m_applicationController->openAudioInputDevice(
    deviceId,
    VOICE_SAMPLE_RATE,
    m_deviceChannels = encoderChannels(),
    [this](std::span<std::uint8_t> stream) {
      readAudioData(stream);
    }
  );
  if (!opened) return std::unexpected(std::move(opened.error()));
  if (!*opened)
    return std::unexpected(VoiceException::format("Could not open input device: {}", SDL_GetError()));

  m_deviceOpen = true;
  return {};
}

void Voice::closeDevice() {
  if (!m_deviceOpen)
    return;

  m_applicationController->closeAudioInputDevice();
  m_clientSpeaker->playing = false;
  m_clientSpeaker->decibelLevel = -96.0f;
  m_deviceOpen = false;
}

bool Voice::playSpeaker(std::shared_ptr<Speaker> const& speaker, int) {
  if (speaker->playing || speaker->audioStream->samples.size() < speaker->minimumPlaySamples)
    return false;

  if (!speaker->playing) {
    speaker->lastPlayTime = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() + std::chrono::microseconds(500));
    speaker->playing = true;
    std::lock_guard lock(m_activeSpeakersMutex);
    m_activeSpeakers.insert(speaker);
  }
  return true;
}

std::expected<void, StarException> Voice::thread(std::stop_token stopToken) {
  while (true) {
    std::unique_lock locker(m_threadMutex);
    if (stopToken.stop_requested())
      return {};

    m_threadCond.wait(locker);
    if (stopToken.stop_requested())
      return {};

    {
      std::lock_guard locker(m_captureMutex);
      ByteArray encoded(VOICE_MAX_PACKET_SIZE, 0);
      std::size_t frameSamples = VOICE_FRAME_SIZE * static_cast<std::size_t>(m_deviceChannels);
      while (m_capturedChunksFrames >= VOICE_FRAME_SIZE) {
        std::vector<opus_int16> samples;
        samples.reserve(frameSamples);
        std::size_t samplesLeft = frameSamples;
        while (samplesLeft && !m_capturedChunks.empty()) {
          auto& front = m_capturedChunks.front();
          if (front.exhausted())
            m_capturedChunks.pop();
          else
            samplesLeft -= front.takeSamples(samples, samplesLeft);
        }
        m_capturedChunksFrames -= VOICE_FRAME_SIZE;

        if (m_inputAmplitude != 1.0f) {
          for (std::size_t i = 0; i != samples.size(); ++i)
            samples[i] *= m_inputAmplitude;
        }

        int encodedSize = opus_encode(m_encoder.get(), samples.data(), VOICE_FRAME_SIZE, reinterpret_cast<unsigned char*>(encoded.span().data()), encoded.size());
        if (encodedSize > 0) {
          if (encodedSize == 1)
            continue;

          encoded.resize(encodedSize);

          {
            std::lock_guard locker(m_encodeMutex);
            m_encodedChunks.emplace_back(std::move(encoded));
            m_encodedChunksLength += encodedSize;

            encoded = ByteArray(VOICE_MAX_PACKET_SIZE, 0);
          }

        }
        else if (encodedSize < 0) {
          auto logged = m_root.logger().error("Voice: Opus encode error {}", opus_strerror(encodedSize));
          if (!logged) return std::unexpected(std::move(logged.error()));
        }
      }
    }

    continue;

    locker.unlock();
    std::this_thread::yield();
  }
  return {};
}

}// namespace Star
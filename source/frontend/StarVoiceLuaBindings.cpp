module;
import std;
import star.vector;
import star.json;
import star.lua;
import star.string;
import star.list;
import star.lua_converters;
import star.bi_map;
import star.format;
import star.game_types;
import star.thread;
import star.data_stream_devices;
import star.input_event;
import star.application;
import star.statistics_service;
import star.p2p_networking_service;
import star.user_generated_content_service;
import star.desktop_service;
import star.image;
import star.application_controller;
struct OpusDecoder;
struct OpusEncoder;
import star.voice;

module star.voice_lua_bindings;

namespace Star {

using SpeakerId = Voice::SpeakerId;
std::expected<LuaCallbacks, StarException> LuaBindings::makeVoiceCallbacks(Voice& voice) {
  LuaCallbacks callbacks;
  if (auto r = callbacks.registerCallback("devices", [&voice]() { return voice.availableDevices(); }); !r)
    return std::unexpected(std::move(r.error()));
  if (auto r = callbacks.registerCallback("getSettings", [&voice]() { return voice.saveJson(); }); !r)
    return std::unexpected(std::move(r.error()));
  if (auto r = callbacks.registerCallback("mergeSettings", [&voice](Json const& settings) { return voice.loadJson(settings); }); !r)
    return std::unexpected(std::move(r.error()));
  if (auto r = callbacks.registerCallback("setSpeakerMuted", [&voice](SpeakerId id, bool muted) -> std::expected<void, StarException> {
    auto speaker = voice.speaker(id);
    if (!speaker) return std::unexpected(std::move(speaker.error()));
    (*speaker)->muted = muted;
    return {};
  }); !r) return std::unexpected(std::move(r.error()));
  if (auto r = callbacks.registerCallback("speakerMuted", [&voice](SpeakerId id) -> std::expected<bool, StarException> {
    auto speaker = voice.speaker(id);
    if (!speaker) return std::unexpected(std::move(speaker.error()));
    return static_cast<bool>((*speaker)->muted);
  }); !r) return std::unexpected(std::move(r.error()));
  if (auto r = callbacks.registerCallback("setSpeakerVolume", [&voice](SpeakerId id, float volume) -> std::expected<void, StarException> {
    auto speaker = voice.speaker(id);
    if (!speaker) return std::unexpected(std::move(speaker.error()));
    (*speaker)->volume = volume;
    return {};
  }); !r) return std::unexpected(std::move(r.error()));
  if (auto r = callbacks.registerCallback("speakerVolume", [&voice](SpeakerId id) -> std::expected<float, StarException> {
    auto speaker = voice.speaker(id);
    if (!speaker) return std::unexpected(std::move(speaker.error()));
    return static_cast<float>((*speaker)->volume);
  }); !r) return std::unexpected(std::move(r.error()));
  if (auto r = callbacks.registerCallback("speakerPosition", [&voice](SpeakerId id) -> std::expected<Vec2F, StarException> {
    auto speaker = voice.speaker(id);
    if (!speaker) return std::unexpected(std::move(speaker.error()));
    return (*speaker)->position;
  }); !r) return std::unexpected(std::move(r.error()));
  if (auto r = callbacks.registerCallback("speaker", [&voice](std::optional<SpeakerId> id) -> std::expected<Json, StarException> {
    if (!id) return voice.localSpeaker()->toJson();
    auto speaker = voice.speaker(*id);
    if (!speaker) return std::unexpected(std::move(speaker.error()));
    return (*speaker)->toJson();
  }); !r) return std::unexpected(std::move(r.error()));
  if (auto r = callbacks.registerCallback("speakers", [&voice](std::optional<bool> onlyPlaying) -> List<Json> {
    List<Json> list;
    for (auto& speaker : voice.sortedSpeakers(onlyPlaying.value_or(true)))
      list.push_back(speaker->toJson());
    return list;
  }); !r) return std::unexpected(std::move(r.error()));
  return callbacks;
}

}

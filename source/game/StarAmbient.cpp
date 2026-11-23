import star.format;
import star.json;
import star.json_extra;
import star.thread;
import star.audio;
import star.list;
import star.map;
import star.set;
import star.bi_map;
import star.ordered_map;
import star.io_device;
import star.image_processing;
import star.hash;
import star.data_stream;
import star.string_view;
import star.directives;
import star.asset_path;
import star.ref_ptr;
import star.logging;
import star.listener;
import std;
import star.string;
import star.random;
import star.game_types;

import star.mixer;
import star.asset_source;
import star.assets;
import star.root_base;
import star.configuration;
import star.root;

import star.ambient;

namespace Star {

AmbientTrackGroup::AmbientTrackGroup(StringList tracks) : tracks(std::move(tracks)) {}

std::expected<AmbientTrackGroup, StarException> AmbientTrackGroup::create(Json const& config, String const& directory) {
  auto track_config = config.find("tracks");
  if (!track_config)
    return std::unexpected(std::move(track_config.error()));
  AmbientTrackGroup group;
  if (!*track_config)
    return group;
  auto values = (*track_config)->get().array_ptr();
  if (!values)
    return std::unexpected(std::move(values.error()));
  group.tracks.reserve((*values)->size());
  for (auto const& value : **values) {
    auto track = value.string_ptr();
    if (!track)
      return std::unexpected(std::move(track.error()));
    group.tracks.push_back(AssetPath::relative_to(directory, **track));
  }
  return group;
}

Json AmbientTrackGroup::to_json() const {
  return JsonObject{{"tracks", json_from_string_list(tracks)}};
}

AmbientNoisesDescription::AmbientNoisesDescription(AmbientTrackGroup day, AmbientTrackGroup night, int loops)
  : day_sounds(std::move(day)), night_sounds(std::move(night)), track_loops(loops) {}

std::expected<AmbientNoisesDescription, StarException> AmbientNoisesDescription::create(Json const& config, String const& directory) {
  AmbientNoisesDescription result;
  auto day = config.opt("day");
  if (!day)
    return std::unexpected(std::move(day.error()));
  if (*day) {
    auto sounds = AmbientTrackGroup::create(**day, directory);
    if (!sounds)
      return std::unexpected(std::move(sounds.error()));
    result.day_sounds = std::move(*sounds);
  }
  auto night = config.opt("night");
  if (!night)
    return std::unexpected(std::move(night.error()));
  if (*night) {
    auto sounds = AmbientTrackGroup::create(**night, directory);
    if (!sounds)
      return std::unexpected(std::move(sounds.error()));
    result.night_sounds = std::move(*sounds);
  }
  auto loops = config.opt_int("loops");
  if (!loops)
    return std::unexpected(std::move(loops.error()));
  if (*loops)
    result.track_loops = **loops;
  return result;
}

Json AmbientNoisesDescription::to_json() const {
  return JsonObject{{"day", day_sounds.to_json()}, {"night", night_sounds.to_json()}, {"loops", track_loops}};
}

AmbientManager::AmbientManager(Root& root) : m_root(root) {}

AmbientManager::~AmbientManager() {
  cancel_all();
}

void AmbientManager::set_track_switch_grace(float grace) {
  m_track_switch_grace = std::chrono::duration<float>(grace);
}

void AmbientManager::set_track_fade_in_time(float fade_in_time) {
  m_track_fade_in_time = fade_in_time;
}

std::expected<std::shared_ptr<AudioInstance>, StarException> AmbientManager::update_ambient(std::shared_ptr<AmbientNoisesDescription const> const& current, bool day_time) {
  std::span<String const> tracks;
  if (current)
    tracks = day_time ? current->day_sounds.tracks : current->night_sounds.tracks;

  if (m_current_track) {
    if (m_current_track->finished()) {
      m_current_track.reset();
    } else {
      auto now = std::chrono::steady_clock::now();
      if (std::ranges::contains(tracks, m_current_track_name))
        m_track_grace_timestamp = now;
      else if (now - m_track_grace_timestamp >= m_track_switch_grace)
        stop_ambient(m_track_fade_in_time);
    }
  }
  if (!m_current_track && !tracks.empty()) {
    while (m_recent_tracks.size() / 2 >= tracks.size()) {
      auto removed = m_recent_tracks.pop_front_checked();
      if (!removed)
        return std::unexpected(std::move(removed.error()));
    }
    while (true) {
      auto sampled = m_root.random().sample_ref(tracks);
      if (!sampled)
        return std::unexpected(std::move(sampled.error()));
      auto const& track = sampled->get();
      if (track.empty())
        break;
      if (m_recent_tracks.contains(track)) {
        auto removed = m_recent_tracks.pop_front_checked();
        if (!removed)
          return std::unexpected(std::move(removed.error()));
        continue;
      }
      auto assets = m_root.assets();
      if (!assets)
        return std::unexpected(std::move(assets.error()));
      auto audio = (*assets)->try_audio(track);
      if (!audio)
        return std::unexpected(std::move(audio.error()));
      if (!*audio)
        break;
      m_recent_tracks.push_back(track);
      auto instance = AudioInstance::create(**audio);
      if (!instance)
        return std::unexpected(std::move(instance.error()));
      m_current_track = std::move(*instance);
      m_current_track_name = track;
      m_current_track->set_loops(current->track_loops);
      m_current_track->set_volume(0.0f);
      m_current_track->set_volume(m_volume, m_track_fade_in_time);
      m_delay = 0;
      m_duration = 0;
      m_volume_changed = false;
      return m_current_track;
    }
  }
  if (m_volume_changed) {
    if (m_delay > 0)
      m_delay -= m_root.timing().time_step;
    else {
      m_volume_changed = false;
      if (m_current_track)
        m_current_track->set_volume(m_volume, m_duration);
    }
  }

  return {};
}

std::expected<std::shared_ptr<AudioInstance>, StarException> AmbientManager::update_weather(std::span<String const> tracks) {
  if (m_weather_track) {
    if (m_weather_track->finished()) {
      m_weather_track.reset();
    } else if (!std::ranges::contains(tracks, m_weather_track_name)) {
      m_weather_track->stop(10.0f);
      m_weather_track.reset();
    }
  }

  if (!m_weather_track && !tracks.empty()) {
    auto sampled = m_root.random().sample_ref(tracks);
    if (!sampled)
      return std::unexpected(std::move(sampled.error()));
    auto const& track = sampled->get();
    if (!track.empty()) {
      auto assets = m_root.assets();
      if (!assets)
        return std::unexpected(std::move(assets.error()));
      auto audio = (*assets)->try_audio(track);
      if (!audio)
        return std::unexpected(std::move(audio.error()));
      if (*audio) {
        auto instance = AudioInstance::create(**audio);
        if (!instance)
          return std::unexpected(std::move(instance.error()));
        m_weather_track = std::move(*instance);
        m_weather_track_name = track;
        m_weather_track->set_loops(-1);
        m_weather_track->set_volume(0.0f);
        m_weather_track->set_volume(1, m_track_fade_in_time);
        return m_weather_track;
      }
    }
  }

  return {};
}

void AmbientManager::stop_ambient(float fade_time) {
  if (m_current_track) {
    m_current_track->stop(fade_time);
    m_current_track.reset();
  }
}

void AmbientManager::cancel_all() {
  if (m_weather_track) {
    m_weather_track->stop();
    m_weather_track.reset();
  }
  stop_ambient(0.0f);
}

void AmbientManager::set_volume(float volume, float delay, float duration) {
  if (m_volume == volume)
    return;
  m_volume = volume;
  m_delay = delay;
  m_duration = duration;
  m_volume_changed = true;
}

}

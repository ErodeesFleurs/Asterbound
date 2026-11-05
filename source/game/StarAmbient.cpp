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
import star.version;
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

AmbientTrackGroup::AmbientTrackGroup() {
  tracks = {};
}

AmbientTrackGroup::AmbientTrackGroup(StringList tracks) : tracks(std::move(tracks)) {}

std::expected<AmbientTrackGroup, StarException> AmbientTrackGroup::create(Json const& config, String const& directory) {
  auto trackConfig = config.get("tracks", JsonArray());
  if (!trackConfig)
    return std::unexpected(std::move(trackConfig.error()));
  auto values = jsonToStringList(*trackConfig);
  if (!values)
    return std::unexpected(std::move(values.error()));
  AmbientTrackGroup group;
  for (auto const& track : *values)
    group.tracks.append(AssetPath::relativeTo(directory, track));
  return group;
}

Json AmbientTrackGroup::toJson() const {
  return JsonObject{{"tracks", jsonFromStringList(tracks)}};
}

AmbientNoisesDescription::AmbientNoisesDescription() : trackLoops(-1) {}

AmbientNoisesDescription::AmbientNoisesDescription(AmbientTrackGroup day, AmbientTrackGroup night, int loops)
  : daySounds(std::move(day)), nightSounds(std::move(night)), trackLoops(loops) {}

std::expected<AmbientNoisesDescription, StarException> AmbientNoisesDescription::create(Json const& config, String const& directory) {
  AmbientNoisesDescription result;
  auto day = config.opt("day");
  if (!day)
    return std::unexpected(std::move(day.error()));
  if (*day) {
    auto sounds = AmbientTrackGroup::create(**day, directory);
    if (!sounds)
      return std::unexpected(std::move(sounds.error()));
    result.daySounds = std::move(*sounds);
  }
  auto night = config.opt("night");
  if (!night)
    return std::unexpected(std::move(night.error()));
  if (*night) {
    auto sounds = AmbientTrackGroup::create(**night, directory);
    if (!sounds)
      return std::unexpected(std::move(sounds.error()));
    result.nightSounds = std::move(*sounds);
  }
  auto loops = config.optInt("loops");
  if (!loops)
    return std::unexpected(std::move(loops.error()));
  if (*loops)
    result.trackLoops = **loops;
  return result;
}

Json AmbientNoisesDescription::toJson() const {
  return JsonObject{{"day", daySounds.toJson()}, {"night", nightSounds.toJson()}, {"loops", trackLoops}};
}

AmbientManager::AmbientManager(Root& root) : m_root(root) {}

AmbientManager::~AmbientManager() {
  cancelAll();
}

void AmbientManager::setTrackSwitchGrace(float grace) {
  m_trackSwitchGrace = std::chrono::duration<float>(grace);
}

void AmbientManager::setTrackFadeInTime(float fadeInTime) {
  m_trackFadeInTime = fadeInTime;
}

std::expected<std::shared_ptr<AudioInstance>, StarException> AmbientManager::updateAmbient(std::shared_ptr<AmbientNoisesDescription> current, bool dayTime) {
  auto assets = m_root.assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));

  if (m_currentTrack) {
    if (m_currentTrack->finished())
      m_currentTrack = {};
  }
  StringList tracks;
  if (current)
    tracks = dayTime ? current->daySounds.tracks : current->nightSounds.tracks;

  if (m_currentTrack) {
    bool switchTrack = m_currentTrack->finished();
    if (!switchTrack) {
      auto present = tracks.contains(m_currentTrackName);
      if (!present)
        return std::unexpected(std::move(present.error()));
      switchTrack = !*present;
    }
    if (switchTrack) {
      if (m_trackSwitchGrace <= std::chrono::time_point_cast<std::chrono::duration<double>>(std::chrono::steady_clock::now()) - m_trackGraceTimestamp) {
        m_currentTrack->stop(m_trackFadeInTime);
        m_currentTrack = {};
      }
    } else {
      m_trackGraceTimestamp = std::chrono::time_point_cast<std::chrono::duration<double>>(std::chrono::steady_clock::now());
    }
  }
  if (!m_currentTrack) {
    m_currentTrackName = "";
    if (tracks.size() > 0) {
      while ((m_recentTracks.size() / 2) >= tracks.size()) {
        auto removed = m_recentTracks.removeFirst();
        if (!removed)
          return std::unexpected(std::move(removed.error()));
      }
      while (true) {
        m_currentTrackName = m_root.random().randValueFrom(tracks);
        if (m_currentTrackName.empty() || !m_recentTracks.contains(m_currentTrackName))
          break;
        auto removed = m_recentTracks.removeFirst();
        if (!removed)
          return std::unexpected(std::move(removed.error()));
      }
    }
    if (!m_currentTrackName.empty()) {
      auto audio = (*assets)->tryAudio(m_currentTrackName);
      if (!audio)
        return std::unexpected(std::move(audio.error()));
      if (*audio) {
        m_recentTracks.append(m_currentTrackName);
        auto instance = AudioInstance::create(**audio);
        if (!instance)
          return std::unexpected(std::move(instance.error()));
        m_currentTrack = std::move(*instance);
        m_currentTrack->setLoops(current ? current->trackLoops : -1);
        // Slowly fade the music track in
        m_currentTrack->setVolume(0.0f);
        m_currentTrack->setVolume(m_volume, m_trackFadeInTime);
        m_delay = 0;
        m_duration = 0;
        m_volumeChanged = false;
        return m_currentTrack;
      }
    }
  }
  if (m_volumeChanged) {
    if (m_delay > 0)
      m_delay -= m_root.timing().timestep;
    else {
      m_volumeChanged = false;
      if (m_currentTrack) {
        m_currentTrack->setVolume(m_volume, m_duration);
      }
    }
  }

  return {};
}

std::expected<std::shared_ptr<AudioInstance>, StarException> AmbientManager::updateWeather(std::shared_ptr<WeatherNoisesDescription> current) {
  auto assets = m_root.assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));

  if (m_weatherTrack) {
    if (m_weatherTrack->finished())
      m_weatherTrack = {};
  }

  StringList tracks;
  if (current)
    tracks = current->tracks;

  if (m_weatherTrack) {
    auto present = tracks.contains(m_weatherTrackName);
    if (!present)
      return std::unexpected(std::move(present.error()));
    if (!*present) {
      m_weatherTrack->stop(10.0f);
      m_weatherTrack = {};
    }
  }

  if (!m_weatherTrack) {
    m_weatherTrackName = m_root.random().randValueFrom(tracks);
    if (!m_weatherTrackName.empty()) {
      auto audio = (*assets)->tryAudio(m_weatherTrackName);
      if (!audio)
        return std::unexpected(std::move(audio.error()));
      if (*audio) {
        auto instance = AudioInstance::create(**audio);
        if (!instance)
          return std::unexpected(std::move(instance.error()));
        m_weatherTrack = std::move(*instance);
        m_weatherTrack->setLoops(-1);
        m_weatherTrack->setVolume(0.0f);
        m_weatherTrack->setVolume(1, m_trackFadeInTime);
        return m_weatherTrack;
      }
    }
  }

  return {};
}

void AmbientManager::cancelAll() {
  if (m_weatherTrack) {
    m_weatherTrack->stop();
    m_weatherTrack = {};
  }
  if (m_currentTrack) {
    m_currentTrack->stop();
    m_currentTrack = {};
  }
}

void AmbientManager::setVolume(float volume, float delay, float duration) {
  if (m_volume == volume)
    return;
  m_volume = volume;
  m_delay = delay;
  m_duration = duration;
  m_volumeChanged = true;
}

}

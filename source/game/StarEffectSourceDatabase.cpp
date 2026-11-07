import star.json;
import star.json_extra;
import star.game_types;
import star.color;
import star.bi_map;
import star.list;
import star.image_processing;
import star.hash;
import star.data_stream;
import star.string_view;
import star.thread;
import star.directives;
import star.asset_path;
import star.ordered_map;
import star.io_device;
import star.ref_ptr;
import star.logging;
import star.listener;
import std;
import star.string;
import star.format;
import star.version;
import star.random;
import star.audio;
import star.map;
import star.set;
import star.vector;

import star.animation;
import star.particle;
import star.asset_source;
import star.assets;
import star.root_base;
import star.configuration;
import star.root;
import star.mixer;

import star.effect_source_database;
import star.particle_database;

namespace Star {

std::expected<std::shared_ptr<EffectSourceDatabase>, StarException> EffectSourceDatabase::create(Root& root) {
  auto result = std::shared_ptr<EffectSourceDatabase>(new EffectSourceDatabase());
  auto assets = root.assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  auto files = (*assets)->scanExtension("effectsource");
  if (!files)
    return std::unexpected(std::move(files.error()));
  auto queued = (*assets)->queueJsons(files->get());
  if (!queued)
    return std::unexpected(std::move(queued.error()));
  for (auto const& file : files->get()) {
    auto loaded = [&]() -> std::expected<void, StarException> {
      auto json = (*assets)->json(file.string());
      if (!json)
        return std::unexpected(std::move(json.error()));
      auto config = EffectSourceConfig::create(root, *json);
      if (!config)
        return std::unexpected(std::move(config.error()));
      if (result->m_sourceConfigs.contains((*config)->kind()))
        return std::unexpected(StarException(strf("Duplicate effect source asset kind Name {}. configfile {}", (*config)->kind(), file.string())));
      auto key = (*config)->kind().toLower();
      if (!key)
        return std::unexpected(std::move(key.error()));
      result->m_sourceConfigs[*key] = std::move(*config);
      return {};
    }();
    if (!loaded)
      return std::unexpected((*assets)->withAssetContext(std::move(loaded.error()), file.string(), "Loading effect source"));
  }
  return result;
}

std::expected<std::shared_ptr<EffectSourceConfig>, StarException> EffectSourceDatabase::effectSourceConfig(String const& kind) const {
  auto key = kind.toLower();
  if (!key)
    return std::unexpected(std::move(key.error()));
  if (auto config = m_sourceConfigs.find_ref(*key))
    return config->get();
  return std::unexpected(StarException(strf("Unknown effect source definition with kind '{}'.", kind)));
}

std::expected<std::shared_ptr<EffectSourceConfig>, StarException> EffectSourceConfig::create(Root& root, Json const& config) {
  auto result = std::shared_ptr<EffectSourceConfig>(new EffectSourceConfig(root));
  auto kind = config.getString("kind");
  if (!kind)
    return std::unexpected(std::move(kind.error()));
  result->m_kind = std::move(*kind);
  result->m_config = config;
  return result;
}

String const& EffectSourceConfig::kind() {
  return m_kind;
}

std::expected<std::shared_ptr<EffectSource>, StarException> EffectSourceConfig::instance(String const& suggestedSpawnLocation) {
  auto definition = m_config.getObject("definition");
  if (!definition)
    return std::unexpected(std::move(definition.error()));
  return EffectSource::create(m_root, kind(), suggestedSpawnLocation, *definition);
}

std::expected<std::shared_ptr<EffectSource>, StarException> EffectSource::create(Root& root, String const& kind, String suggestedSpawnLocation, Json const& definition) {
  auto result = std::shared_ptr<EffectSource>(new EffectSource(root));
  result->m_kind = kind;
  result->m_config = definition;
  result->m_expired = false;
  auto duration = definition.getFloat("duration", 0);
  if (!duration)
    return std::unexpected(std::move(duration.error()));
  result->m_loopDuration = *duration;
  auto variance = definition.getFloat("durationVariance", 0);
  if (!variance)
    return std::unexpected(std::move(variance.error()));
  result->m_durationVariance = *variance;
  auto loops = definition.getBool("loops", *duration != 0);
  if (!loops)
    return std::unexpected(std::move(loops.error()));
  result->m_loops = *loops;
  result->m_timer = root.random().randf() * (*duration + 0.5 * *variance);
  result->m_stop = false;
  result->m_initialTick = true;
  result->m_loopTick = false;
  result->m_finalTick = false;
  auto location = definition.getString("location", "normal");
  if (!location)
    return std::unexpected(std::move(location.error()));
  result->m_effectSpawnLocation = std::move(*location);
  result->m_suggestedSpawnLocation = std::move(suggestedSpawnLocation);
  return result;
}

String const& EffectSource::kind() const {
  return m_kind;
}

bool EffectSource::expired() const {
  return m_expired;
}

void EffectSource::stop() {
  m_stop = true;
}

void EffectSource::tick(float dt) {
  m_timer -= dt;
  if ((m_timer <= 0) && m_loops) {
    m_timer = m_loopDuration + m_durationVariance * (m_root.random().randf() - 0.5f);
    m_loopTick = true;
  }
  if (m_stop || (m_timer <= 0))
    if (!m_expired)
      m_finalTick = true;
}

std::expected<List<String>, StarException> EffectSource::particles() {
  List<String> result;
  auto pick = [&](std::size_t stage, auto&& loadConfig) -> std::expected<void, StarException> {
    auto& choices = m_particleChoices[stage];
    if (!choices) {
      auto config = std::invoke(loadConfig);
      if (!config)
        return std::unexpected(std::move(config.error()));
      auto options = config->arrayPtr();
      if (!options)
        return std::unexpected(std::move(options.error()));
      auto initialized = std::make_unique<ParticleChoices>();
      initialized->options = std::move(*options);
      initialized->decoded.resize(initialized->options->size());
      choices = std::move(initialized);
    }
    if (choices->options->empty())
      return {};
    // Match randValueFrom's single inclusive draw, including the one-option case.
    auto index = static_cast<std::size_t>(m_root.random().randInt(choices->options->size() - 1));
    auto& decoded = choices->decoded[index];
    if (!decoded) {
      auto selected = jsonToStringList((*choices->options)[index]);
      if (!selected)
        return std::unexpected(std::move(selected.error()));
      decoded = std::move(*selected);
    }
    result.appendAll(*decoded);
    return {};
  };
  if (m_initialTick) {
    auto selected = pick(0, [&] {
      return m_config.get("start", JsonObject()).and_then([](Json config) {
        return config.get("particles", JsonArray());
      });
    });
    if (!selected)
      return std::unexpected(std::move(selected.error()));
  }
  if (m_loopTick) {
    auto selected = pick(1, [&] { return m_config.get("particles", JsonArray()); });
    if (!selected)
      return std::unexpected(std::move(selected.error()));
  }
  if (m_finalTick) {
    auto selected = pick(2, [&] {
      return m_config.get("stop", JsonObject()).and_then([](Json config) {
        return config.get("particles", JsonArray());
      });
    });
    if (!selected)
      return std::unexpected(std::move(selected.error()));
  }
  return result;
}

std::expected<List<std::shared_ptr<AudioInstance>>, StarException> EffectSource::sounds(Vec2F offset) {
  List<std::shared_ptr<AudioInstance>> result;
  if (m_initialTick) {
    auto start = m_config.get("start", JsonObject());
    if (!start)
      return std::unexpected(std::move(start.error()));
    auto config = start->get("sounds", Json());
    if (!config)
      return std::unexpected(std::move(config.error()));
    auto initial = soundsFromDefinition(m_root, *config, offset);
    if (!initial)
      return std::unexpected(std::move(initial.error()));
    result.appendAll(*initial);
    auto mainConfig = m_config.get("sounds", Json());
    if (!mainConfig)
      return std::unexpected(std::move(mainConfig.error()));
    auto main = soundsFromDefinition(m_root, *mainConfig, offset);
    if (!main)
      return std::unexpected(std::move(main.error()));
    m_mainSounds = std::move(*main);
    result.appendAll(m_mainSounds);
  }
  if (m_finalTick) {
    for (auto& sound : m_mainSounds)
      sound->stop();
    auto stop = m_config.get("stop", JsonObject());
    if (!stop)
      return std::unexpected(std::move(stop.error()));
    auto config = stop->get("sounds", Json());
    if (!config)
      return std::unexpected(std::move(config.error()));
    auto final = soundsFromDefinition(m_root, *config, offset);
    if (!final)
      return std::unexpected(std::move(final.error()));
    result.appendAll(*final);
  }
  return result;
}

void EffectSource::finishEventCollection() {
  m_initialTick = false;
  m_loopTick = false;
  if (m_finalTick) {
    m_finalTick = false;
    m_expired = true;
  }
}

String EffectSource::effectSpawnLocation() const {
  if ((m_effectSpawnLocation == "normal") && (!m_suggestedSpawnLocation.empty()))
    return m_suggestedSpawnLocation;
  return m_effectSpawnLocation;
}

String EffectSource::suggestedSpawnLocation() const {
  return m_suggestedSpawnLocation;
}

std::expected<List<Particle>, StarException> particlesFromDefinition(Root& root, Json const& config, Vec2F const& position) {
  Json particles = config;
  if (config.type() == Json::Type::Array) {
    auto options = config.arrayPtr();
    if (!options)
      return std::unexpected(std::move(options.error()));
    particles = root.random().randValueFrom(**options, Json());
  }
  List<Particle> result;
  if (particles.isNull())
    return result;
  if (particles.type() != Json::Type::Array)
    particles = JsonArray{particles};
  auto entries = particles.iterateArray();
  if (!entries)
    return std::unexpected(std::move(entries.error()));
  for (auto const& entry : *entries) {
    if (entry.type() != Json::Type::Object) {
      auto name = entry.toString();
      if (!name)
        return std::unexpected(std::move(name.error()));
      auto database = root.particleDatabase();
      if (!database)
        return std::unexpected(std::move(database.error()));
      auto particle = (*database)->particle(*name);
      if (!particle)
        return std::unexpected(std::move(particle.error()));
      result.append(std::move(*particle));
    } else {
      auto particle = Particle::create(root, entry);
      if (!particle)
        return std::unexpected(std::move(particle.error()));
      auto varianceConfig = entry.getObject("variance", {});
      if (!varianceConfig)
        return std::unexpected(std::move(varianceConfig.error()));
      auto variance = Particle::create(root, *varianceConfig);
      if (!variance)
        return std::unexpected(std::move(variance.error()));
      particle->applyVariance(root, *variance);
      particle->position += position;
      result.append(std::move(*particle));
    }
  }
  return result;
}

std::expected<List<std::shared_ptr<AudioInstance>>, StarException> soundsFromDefinition(Root& root, Json const& config, Vec2F const& position) {
  Json sound = config;
  if (config.type() == Json::Type::Array) {
    auto options = config.arrayPtr();
    if (!options)
      return std::unexpected(std::move(options.error()));
    sound = root.random().randValueFrom(**options, Json());
  }
  List<std::shared_ptr<AudioInstance>> result;
  if (sound.isNull())
    return result;
  if (sound.type() != Json::Type::Array)
    sound = JsonArray{sound};
  auto assets = root.assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  auto entries = sound.iterateArray();
  if (!entries)
    return std::unexpected(std::move(entries.error()));
  for (auto entry : *entries) {
    if (entry.type() != Json::Type::Object) {
      auto resource = entry.toString();
      if (!resource)
        return std::unexpected(std::move(resource.error()));
      entry = JsonObject{{"resource", std::move(*resource)}};
    }
    auto resource = entry.getString("resource");
    if (!resource)
      return std::unexpected(std::move(resource.error()));
    auto audio = (*assets)->audio(*resource);
    if (!audio)
      return std::unexpected(std::move(audio.error()));
    auto sample = AudioInstance::create(**audio);
    if (!sample)
      return std::unexpected(std::move(sample.error()));
    auto loops = entry.getInt("loops", 0);
    if (!loops)
      return std::unexpected(std::move(loops.error()));
    (*sample)->setLoops(*loops);
    auto volume = entry.getFloat("volume", 1.0f);
    if (!volume)
      return std::unexpected(std::move(volume.error()));
    (*sample)->setVolume(*volume);
    auto pitch = entry.getFloat("pitch", 1.0f);
    if (!pitch)
      return std::unexpected(std::move(pitch.error()));
    auto randomPitch = root.random().randf() * 2.0f - 1.0f;
    auto variability = entry.getFloat("pitchVariability", 0.0f);
    if (!variability)
      return std::unexpected(std::move(variability.error()));
    (*sample)->setPitchMultiplier(*pitch + randomPitch * *variability);
    auto range = entry.getFloat("audioRangeMultiplier", 1.0f);
    if (!range)
      return std::unexpected(std::move(range.error()));
    (*sample)->setRangeMultiplier(*range);
    (*sample)->setPosition(position);
    result.append(std::move(*sample));
  }
  return result;
}

}

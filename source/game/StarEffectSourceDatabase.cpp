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
  auto files = (*assets)->scan_extension("effectsource");
  if (!files)
    return std::unexpected(std::move(files.error()));
  auto queued = (*assets)->queue_jsons(files->get());
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
      auto key = (*config)->kind().to_lower();
      if (!key)
        return std::unexpected(std::move(key.error()));
      if (!result->m_sourceConfigs.try_emplace(std::move(*key), std::move(*config)).second)
        return std::unexpected(StarException(Star::format("Duplicate effect source asset kind Name {}. configfile {}", (*config)->kind(), file.string())));
      return {};
    }();
    if (!loaded)
      return std::unexpected((*assets)->with_asset_context(std::move(loaded.error()), file.string(), "Loading effect source"));
  }
  return result;
}

std::expected<std::shared_ptr<EffectSourceConfig>, StarException> EffectSourceDatabase::effectSourceConfig(String const& kind) const {
  auto key = kind.to_lower();
  if (!key)
    return std::unexpected(std::move(key.error()));
  if (auto config = m_sourceConfigs.find_ref(*key))
    return config->get();
  return std::unexpected(StarException(Star::format("Unknown effect source definition with kind '{}'.", kind)));
}

std::expected<std::shared_ptr<EffectSourceConfig>, StarException> EffectSourceConfig::create(Root& root, Json const& config) {
  auto result = std::shared_ptr<EffectSourceConfig>(new EffectSourceConfig(root));
  auto kind = config.get_string("kind");
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
  auto definition = m_config.get_object("definition");
  if (!definition)
    return std::unexpected(std::move(definition.error()));
  return EffectSource::create(m_root, suggestedSpawnLocation, *definition);
}

std::expected<std::shared_ptr<EffectSource>, StarException> EffectSource::create(Root& root, String suggested_spawn_location, Json const& definition) {
  auto result = std::shared_ptr<EffectSource>(new EffectSource(root));
  result->m_config = definition;
  result->m_expired = false;
  auto duration = definition.get_float("duration", 0);
  if (!duration)
    return std::unexpected(std::move(duration.error()));
  result->m_loopDuration = *duration;
  auto variance = definition.get_float("durationVariance", 0);
  if (!variance)
    return std::unexpected(std::move(variance.error()));
  result->m_durationVariance = *variance;
  auto loops = definition.get_bool("loops", *duration != 0);
  if (!loops)
    return std::unexpected(std::move(loops.error()));
  result->m_loops = *loops;
  result->m_timer = root.random().uniform_float() * (*duration + 0.5 * *variance);
  result->m_stop = false;
  auto location = definition.get_string("location", "normal");
  if (!location)
    return std::unexpected(std::move(location.error()));
  result->m_effectSpawnLocation = *location == "normal" && !suggested_spawn_location.empty()
      ? std::move(suggested_spawn_location) : std::move(*location);
  return result;
}

EffectSource::~EffectSource() {
  stop_main_sounds();
}

void EffectSource::stop_main_sounds() {
  for (auto const& sound : m_mainSounds)
    sound->stop();
  m_mainSounds.clear();
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
    m_timer = m_loopDuration + m_durationVariance * (m_root.random().uniform_float() - 0.5f);
    m_stages[1] = true;
  }
  if (m_stop || (m_timer <= 0))
    if (!m_expired)
      m_stages[2] = true;
}

std::expected<Json, StarException> EffectSource::stage_config(std::size_t stage) const {
  if (stage == 1)
    return m_config;
  return stage == 0 ? m_config.get("start", JsonObject()) : m_config.get("stop", JsonObject());
}

std::expected<std::span<String const>, StarException> EffectSource::select_particles(std::size_t stage, std::optional<std::size_t>& selection) {
  auto& choices = m_particleChoices[stage];
  if (!choices) {
    auto config = stage_config(stage);
    if (!config)
      return std::unexpected(std::move(config.error()));
    auto particles = config->get("particles", JsonArray());
    if (!particles)
      return std::unexpected(std::move(particles.error()));
    auto options = particles->array_ptr();
    if (!options)
      return std::unexpected(std::move(options.error()));
    choices = std::make_unique<ParticleChoices>();
    choices->options = std::move(*options);
    choices->decoded.resize(choices->options->size());
  }
  if (choices->options->empty())
    return std::span<String const>{};
  if (!selection) {
    // Preserve the original inclusive draw, including the one-option case.
    auto index = m_root.random().uniform_int(choices->options->size() - 1);
    if (!index)
      return std::unexpected(std::move(index.error()));
    selection = static_cast<std::size_t>(*index);
  }
  auto& decoded = choices->decoded[*selection];
  if (!decoded) {
    auto selected = json_to_string_list((*choices->options)[*selection]);
    if (!selected)
      return std::unexpected(std::move(selected.error()));
    decoded = std::move(*selected);
  }
  return std::span<String const>(*decoded);
}

std::expected<List<String>, StarException> EffectSource::particles() {
  List<String> result;
  for (std::size_t stage = 0; stage < m_stages.size(); ++stage) {
    if (!m_stages[stage])
      continue;
    std::optional<std::size_t> selection;
    auto names = select_particles(stage, selection);
    if (!names)
      return std::unexpected(std::move(names.error()));
    result.append_range(*names);
  }
  return result;
}

bool EffectSource::has_pending_events() const {
  return m_stages[0] || m_stages[1] || m_stages[2];
}

EffectSource::Stages EffectSource::take_stages() {
  return std::exchange(m_stages, Stages{});
}

void EffectSource::finish_stages(Stages const& stages) {
  if (stages[2]) {
    m_stages = {};
    m_expired = true;
  }
}

void EffectSource::finishEventCollection() {
  finish_stages(take_stages());
}

String const& EffectSource::effect_spawn_location() const {
  return m_effectSpawnLocation;
}

std::expected<List<Particle>, StarException> particlesFromDefinition(Root& root, Json const& config, Vec2F const& position) {
  Json particles = config;
  if (config.type() == Json::Type::Array) {
    auto options = config.array_ptr();
    if (!options)
      return std::unexpected(std::move(options.error()));
    auto selected = root.random().sample_value(**options, Json());
    if (!selected)
      return std::unexpected(std::move(selected.error()));
    particles = std::move(*selected);
  }
  List<Particle> result;
  if (particles.is_null())
    return result;
  if (particles.type() != Json::Type::Array)
    particles = JsonArray{particles};
  auto entries = particles.iterate_array();
  if (!entries)
    return std::unexpected(std::move(entries.error()));
  for (auto const& entry : *entries) {
    if (entry.type() != Json::Type::Object) {
      auto name = entry.to_string();
      if (!name)
        return std::unexpected(std::move(name.error()));
      auto database = root.particle_database();
      if (!database)
        return std::unexpected(std::move(database.error()));
      auto particle = (*database)->particle(*name);
      if (!particle)
        return std::unexpected(std::move(particle.error()));
      result.push_back(std::move(*particle));
    } else {
      auto particle = Particle::create(root, entry);
      if (!particle)
        return std::unexpected(std::move(particle.error()));
      auto varianceConfig = entry.get_object("variance", {});
      if (!varianceConfig)
        return std::unexpected(std::move(varianceConfig.error()));
      auto variance = Particle::create(root, *varianceConfig);
      if (!variance)
        return std::unexpected(std::move(variance.error()));
      particle->apply_variance(root, *variance);
      particle->position += position;
      result.push_back(std::move(*particle));
    }
  }
  return result;
}

namespace {
std::expected<std::shared_ptr<JsonArray const>, StarException> select_sound_entries(Root& root, Json const& config) {
  Json sound = config;
  if (config.type() == Json::Type::Array) {
    auto options = config.array_ptr();
    if (!options)
      return std::unexpected(std::move(options.error()));
    auto selected = root.random().sample_value(**options, Json());
    if (!selected)
      return std::unexpected(std::move(selected.error()));
    sound = std::move(*selected);
  }
  if (sound.is_null())
    return std::shared_ptr<JsonArray const>{};
  if (sound.type() == Json::Type::Array)
    return sound.array_ptr();
  return std::make_shared<JsonArray const>(JsonArray{std::move(sound)});
}

std::expected<std::shared_ptr<AudioInstance>, StarException> prepare_sound(Root& root, Assets const& assets, Json entry, Vec2F const& position) {
  if (entry.type() != Json::Type::Object) {
    auto resource = entry.to_string();
    if (!resource)
      return std::unexpected(std::move(resource.error()));
    entry = JsonObject{{"resource", std::move(*resource)}};
  }
  auto resource = entry.get_string("resource");
  if (!resource)
    return std::unexpected(std::move(resource.error()));
  auto audio = assets.audio(*resource);
  if (!audio)
    return std::unexpected(std::move(audio.error()));
  auto sample = AudioInstance::create(**audio);
  if (!sample)
    return std::unexpected(std::move(sample.error()));
  auto loops = entry.get_int("loops", 0);
  if (!loops)
    return std::unexpected(std::move(loops.error()));
  (*sample)->set_loops(*loops);
  auto volume = entry.get_float("volume", 1.0f);
  if (!volume)
    return std::unexpected(std::move(volume.error()));
  (*sample)->set_volume(*volume);
  auto pitch = entry.get_float("pitch", 1.0f);
  if (!pitch)
    return std::unexpected(std::move(pitch.error()));
  auto variability = entry.get_float("pitchVariability", 0.0f);
  if (!variability)
    return std::unexpected(std::move(variability.error()));
  auto range = entry.get_float("audioRangeMultiplier", 1.0f);
  if (!range)
    return std::unexpected(std::move(range.error()));
  auto random_pitch = root.random().uniform_float() * 2.0f - 1.0f;
  (*sample)->set_pitch_multiplier(*pitch + random_pitch * *variability);
  (*sample)->set_range_multiplier(*range);
  (*sample)->set_position(position);
  return std::move(*sample);
}
}

std::expected<void, StarException> EffectSource::prepare_sounds(Stages const& stages, SoundBatch& batch, Vec2F offset) {
  for (; batch.stage < stages.size(); ++batch.stage) {
    // Main looping audio starts once, alongside the start audio, not on loop ticks.
    if (!(batch.stage == 2 ? stages[2] : stages[0]))
      continue;
    if (!batch.selection) {
      if (batch.stage == 2)
        stop_main_sounds();
      auto config = stage_config(batch.stage);
      if (!config)
        return std::unexpected(std::move(config.error()));
      auto sounds = config->get("sounds", Json());
      if (!sounds)
        return std::unexpected(std::move(sounds.error()));
      auto entries = select_sound_entries(m_root, *sounds);
      if (!entries)
        return std::unexpected(std::move(entries.error()));
      batch.selection = std::move(*entries);
    }
    if (batch.selection && !batch.selection->empty()) {
      auto assets = m_root.assets();
      if (!assets)
        return std::unexpected(std::move(assets.error()));
      for (; batch.entry < batch.selection->size(); ++batch.entry) {
        auto sound = prepare_sound(m_root, **assets, (*batch.selection)[batch.entry], offset);
        if (!sound)
          return std::unexpected(std::move(sound.error()));
        if (batch.stage == 1)
          m_mainSounds.push_back(*sound);
        batch.sounds.push_back(std::move(*sound));
      }
    }
    batch.selection.reset();
    batch.entry = 0;
  }
  return {};
}

std::expected<List<std::shared_ptr<AudioInstance>>, StarException> soundsFromDefinition(Root& root, Json const& config, Vec2F const& position) {
  auto entries = select_sound_entries(root, config);
  if (!entries)
    return std::unexpected(std::move(entries.error()));
  List<std::shared_ptr<AudioInstance>> result;
  if (!*entries || (*entries)->empty())
    return result;
  auto assets = root.assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  result.reserve((*entries)->size());
  for (auto const& entry : **entries) {
    auto sound = prepare_sound(root, **assets, entry, position);
    if (!sound)
      return std::unexpected(std::move(sound.error()));
    result.push_back(std::move(*sound));
  }
  return result;
}

}

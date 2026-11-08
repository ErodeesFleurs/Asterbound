import star.json_extra;
import star.json;
import star.game_types;
import star.ordered_map;
import star.io_device;
import star.ref_ptr;
import star.logging;
import star.thread;
import star.listener;
import std;
import star.format;
import star.version;
import star.audio;
import star.list;
import star.map;
import star.set;
import star.vector;
import star.bi_map;
import star.color;
import star.string;
import star.data_stream;
import star.image_processing;
import star.hash;
import star.string_view;
import star.directives;
import star.asset_path;
import star.data_stream_extra;

import star.asset_source;
import star.assets;
import star.root_base;
import star.configuration;
import star.root;
import star.mixer;
import star.drawable;
import star.entity_rendering_types;
import star.animation;
import star.particle;

import star.light_source;
import star.entity_rendering;

import star.effect_emitter;
import star.effect_source_database;
import star.particle_database;

import star.algorithm;
namespace Star {

std::expected<std::shared_ptr<EffectEmitter>, StarException> EffectEmitter::create(Root& root) {
  auto emitter = std::shared_ptr<EffectEmitter>(new EffectEmitter(root));
  auto registered = emitter->addNetElement(emitter->m_activeSources);
  if (!registered)
    return std::unexpected(std::move(registered.error()));
  return emitter;
}

void EffectEmitter::addEffectSources(String const& position, StringSet effectSources) {
  for (auto& e : effectSources)
    m_newSources.add({position, std::move(e)});
}

void EffectEmitter::setSourcePosition(String name, Vec2F const& position) {
  m_positions[std::move(name)] = position;
}

void EffectEmitter::setDirection(Direction direction) {
  m_direction = direction;
}

void EffectEmitter::setBaseVelocity(Vec2F const& velocity) {
  m_baseVelocity = velocity;
}

std::expected<void, StarException> EffectEmitter::tick(float dt, EntityMode mode) {
  if (mode == EntityMode::Master) {
    m_activeSources.set(std::move(m_newSources));
    m_newSources.clear();
  } else {
    if (!m_newSources.empty())
      return std::unexpected(StarException("EffectEmitters can only be added to the master entity."));
  }
  if (m_collectsEvents) {
    m_sources.erase(std::remove_if(m_sources.begin(), m_sources.end(),
        [](std::shared_ptr<EffectSource> const& source) { return source->expired(); }), m_sources.end());

    for (auto& ps : m_sources)
      ps->tick(dt);

    Set<std::pair<String, String>> current;
    for (auto& ps : m_sources) {
      std::pair<String, String> entry = {ps->suggestedSpawnLocation(), ps->kind()};
      current.add(entry);
      if (!m_activeSources.get().contains(entry)) {
        ps->stop();
      }
    }
    for (auto& c : m_activeSources.get()) {
      if (!current.contains(c)) {
        auto database = m_root.effectSourceDatabase();
        if (!database)
          return std::unexpected(std::move(database.error()));
        auto config = (*database)->effectSourceConfig(c.second);
        if (!config)
          return std::unexpected(std::move(config.error()));
        auto source = (*config)->instance(c.first);
        if (!source)
          return std::unexpected(std::move(source.error()));
        m_sources.push_back(std::move(*source));
      }
    }
  }
  return {};
}

void EffectEmitter::reset() {
  m_sources.clear();
  m_newSources.clear();
  m_activeSources.set({});
}

std::expected<void, StarException> EffectEmitter::collectRenderEvents(RenderCallback& renderCallback) {
  m_collectsEvents = true;
  if (m_sources.empty())
    return {};
  for (auto& source : m_sources) {
    auto position = m_positions.get(source->effectSpawnLocation());
    if (!position)
      return std::unexpected(std::move(position.error()));
    auto particles = source->particles();
    if (!particles)
      return std::unexpected(std::move(particles.error()));
    for (auto const& definition : *particles) {
      auto database = m_root.particleDatabase();
      if (!database)
        return std::unexpected(std::move(database.error()));
      auto particle = (*database)->particle(definition);
      if (!particle)
        return std::unexpected(std::move(particle.error()));
      if (m_direction == Direction::Left) {
        particle->flip = true;
        particle->position[0] = -particle->position[0];
        particle->velocity[0] = -particle->velocity[0];
        particle->finalVelocity[0] = -particle->finalVelocity[0];
      }
      particle->velocity += m_baseVelocity;
      particle->finalVelocity += m_baseVelocity;
      particle->position += position->get();
      auto added = renderCallback.addParticle(*particle);
      if (!added)
        return std::unexpected(std::move(added.error()));
    }
    auto sounds = source->sounds(position->get());
    if (!sounds)
      return std::unexpected(std::move(sounds.error()));
    for (auto& sound : *sounds) {
      auto added = renderCallback.addAudio(sound);
      if (!added)
        return std::unexpected(std::move(added.error()));
    }
  }
  for (auto& source : m_sources)
    source->finishEventCollection();
  return {};
}

std::expected<Json, StarException> EffectEmitter::toJson() const {
  auto sources = jsonFromSet<Set<std::pair<String, String>>>(m_activeSources.get(),
      [](std::pair<String, String> const& entry) {
        return JsonObject{{"position", entry.first}, {"source", entry.second}};
      });
  if (!sources)
    return std::unexpected(std::move(sources.error()));
  return JsonObject{{"activeSources", std::move(*sources)}};
}

std::expected<void, StarException> EffectEmitter::fromJson(Json const& diskStore) {
  auto config = diskStore.get("activeSources");
  if (!config)
    return std::unexpected(std::move(config.error()));
  auto sources = jsonToSet<Set<std::pair<String, String>>>(*config,
      [](Json const& value) -> std::expected<std::pair<String, String>, StarException> {
        auto position = value.getString("position");
        if (!position)
          return std::unexpected(std::move(position.error()));
        auto source = value.getString("source");
        if (!source)
          return std::unexpected(std::move(source.error()));
        return std::pair{std::move(*position), std::move(*source)};
      });
  if (!sources)
    return std::unexpected(std::move(sources.error()));
  m_activeSources.set(std::move(*sources));
  return {};
}

}

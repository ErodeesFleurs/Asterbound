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

// Keep this on the stack on the normal path. Only a failed batch needs to
// outlive collection. Its source cannot expire until its final stage is acknowledged.
struct EffectEmitter::PendingEvents {
  EffectSource& source;
  EffectSource::Stages stages;
  Vec2F position;
  Vec2F base_velocity;
  Direction direction;
  std::array<std::span<String const>, 3> definitions{};
  std::size_t selected_stages = 0;
  std::optional<std::size_t> choice;
  std::size_t particle_stage = 0;
  std::size_t particle_index = 0;
  std::optional<Particle> rejected_particle;
  EffectSource::SoundBatch audio;
  std::size_t audio_index = 0;

  std::expected<void, StarException> submit(Root& root, RenderCallback& sink, std::shared_ptr<ParticleDatabase const>& database) {
    // Select every enabled particle stage before drawing any variance, just as
    // in the original emission order. These spans borrow immutable decoded options.
    for (; selected_stages < stages.size(); ++selected_stages) {
      if (stages[selected_stages]) {
        auto selected = source.select_particles(selected_stages, choice);
        if (!selected)
          return std::unexpected(std::move(selected.error()));
        definitions[selected_stages] = *selected;
        choice.reset();
      }
    }
    for (; particle_stage < stages.size(); ++particle_stage) {
      auto names = definitions[particle_stage];
      for (; particle_index < names.size(); ++particle_index) {
        if (rejected_particle) {
          auto added = sink.add_particle(std::move(*rejected_particle));
          if (!added)
            return std::unexpected(std::move(added.error()));
          rejected_particle.reset();
        } else {
          if (!database) {
            auto loaded = root.particleDatabase();
            if (!loaded)
              return std::unexpected(std::move(loaded.error()));
            database = std::move(*loaded);
          }
          auto config = database->config(names[particle_index]);
          if (!config)
            return std::unexpected(std::move(config.error()));
          auto particle = (*config)->instance();
          if (direction == Direction::Left) {
            particle.flip = true;
            particle.position[0] = -particle.position[0];
            particle.velocity[0] = -particle.velocity[0];
            particle.finalVelocity[0] = -particle.finalVelocity[0];
          }
          particle.velocity += base_velocity;
          particle.finalVelocity += base_velocity;
          particle.position += position;
          auto added = sink.add_particle(std::move(particle));
          if (!added) {
            rejected_particle.emplace(std::move(particle));
            return std::unexpected(std::move(added.error()));
          }
        }
      }
      particle_index = 0;
    }
    auto prepared = source.prepare_sounds(stages, audio, position);
    if (!prepared)
      return std::unexpected(std::move(prepared.error()));
    for (; audio_index < audio.sounds.size(); ++audio_index) {
      auto added = sink.add_audio(audio.sounds[audio_index]);
      if (!added)
        return std::unexpected(std::move(added.error()));
    }
    source.finish_stages(stages);
    return {};
  }
};

EffectEmitter::EffectEmitter(Root& root) : m_root(root) {}
EffectEmitter::~EffectEmitter() = default;

std::expected<std::shared_ptr<EffectEmitter>, StarException> EffectEmitter::create(Root& root) {
  auto emitter = std::shared_ptr<EffectEmitter>(new EffectEmitter(root));
  auto registered = emitter->add_net_element(emitter->m_active_sources);
  if (!registered)
    return std::unexpected(std::move(registered.error()));
  return emitter;
}

void EffectEmitter::add_effect_sources(String const& position, StringSet const& effect_sources) {
  for (auto const& name : effect_sources)
    m_new_sources.emplace(position, name);
}

void EffectEmitter::set_source_position(String name, Vec2F const& position) {
  m_positions[std::move(name)] = position;
}

void EffectEmitter::set_direction(Direction direction) {
  m_direction = direction;
}

void EffectEmitter::set_base_velocity(Vec2F const& velocity) {
  m_base_velocity = velocity;
}

std::expected<void, StarException> EffectEmitter::tick(float dt, EntityMode mode) {
  if (mode == EntityMode::Master) {
    if (m_active_sources.get() != m_new_sources)
      m_active_sources.push(std::move(m_new_sources));
    m_new_sources.clear();
  } else if (!m_new_sources.empty()) {
    return std::unexpected(StarException("EffectEmitters can only be added to the master entity."));
  }
  if (!m_collects_events)
    return {};

  if (m_active_sources.pull_updated())
    m_reconcile = true;
  bool expired = false;
  for (auto const& running : m_sources) {
    if (running.source->expired())
      expired = true;
    else
      running.source->tick(dt);
  }
  if (expired) {
    m_sources.erase(std::remove_if(m_sources.begin(), m_sources.end(),
        [](RunningSource const& running) { return running.source->expired(); }), m_sources.end());
    m_reconcile = true;
  }
  if (m_reconcile) {
    auto reconciled = reconcile_sources();
    if (!reconciled)
      return reconciled;
    m_reconcile = false;
  }
  return {};
}

std::expected<void, StarException> EffectEmitter::reconcile_sources() {
  Map<SourceKey, bool> requested;
  struct Request {
    Map<SourceKey, bool>::value_type& entry;
    String const& spelling;
  };
  List<Request> ordered;
  ordered.reserve(m_active_sources.get().size());
  for (auto const& [position, name] : m_active_sources.get()) {
    auto kind = name.to_lower();
    if (!kind)
      return std::unexpected(std::move(kind.error()));
    auto [entry, inserted] = requested.try_emplace(SourceKey{position, std::move(*kind)}, false);
    if (inserted)
      ordered.push_back({*entry, name});
  }
  for (auto const& running : m_sources) {
    auto wanted = requested.find(running.key);
    if (wanted == requested.end())
      running.source->stop();
    else
      wanted->second = true;
  }
  std::shared_ptr<EffectSourceDatabase const> database;
  // Visit raw keys in wire order, but match only (exact position, canonical kind).
  for (auto const& request : ordered) {
    auto const& [key, running] = request.entry;
    if (running)
      continue;
    if (!database) {
      auto loaded = m_root.effectSourceDatabase();
      if (!loaded)
        return std::unexpected(std::move(loaded.error()));
      database = std::move(*loaded);
    }
    auto config = database->effectSourceConfig(request.spelling);
    if (!config)
      return std::unexpected(std::move(config.error()));
    auto source = (*config)->instance(key.first);
    if (!source)
      return std::unexpected(std::move(source.error()));
    m_sources.push_back({key, std::move(*source)});
  }
  return {};
}

void EffectEmitter::reset() {
  m_pending_events.reset();
  m_reconcile = true;
  m_sources.clear();
  m_new_sources.clear();
  m_active_sources.set({});
}

std::expected<void, StarException> EffectEmitter::collect_render_events(RenderCallback& render_callback) {
  m_collects_events = true;
  std::shared_ptr<ParticleDatabase const> database;
  if (m_pending_events) {
    auto submitted = m_pending_events->submit(m_root, render_callback, database);
    if (!submitted)
      return submitted;
    m_pending_events.reset();
  }
  for (auto const& [key, source] : m_sources) {
    auto position = m_positions.get(source->effect_spawn_location());
    if (!position)
      return std::unexpected(std::move(position.error()));
    if (!source->has_pending_events())
      continue;
    PendingEvents batch{*source, source->take_stages(), position->get(), m_base_velocity, m_direction};
    auto submitted = batch.submit(m_root, render_callback, database);
    if (!submitted) {
      m_pending_events = std::make_unique<PendingEvents>(std::move(batch));
      return submitted;
    }
  }
  return {};
}

std::expected<Json, StarException> EffectEmitter::to_json() const {
  auto sources = json_from_set<Set<std::pair<String, String>>>(m_active_sources.get(),
      [](std::pair<String, String> const& entry) {
        return JsonObject{{"position", entry.first}, {"source", entry.second}};
      });
  if (!sources)
    return std::unexpected(std::move(sources.error()));
  return JsonObject{{"activeSources", std::move(*sources)}};
}

std::expected<void, StarException> EffectEmitter::from_json(Json const& disk_store) {
  auto config = disk_store.get("activeSources");
  if (!config)
    return std::unexpected(std::move(config.error()));
  auto sources = json_to_set<Set<std::pair<String, String>>>(*config,
      [](Json const& value) -> std::expected<std::pair<String, String>, StarException> {
        auto position = value.get_string("position");
        if (!position)
          return std::unexpected(std::move(position.error()));
        auto source = value.get_string("source");
        if (!source)
          return std::unexpected(std::move(source.error()));
        return std::pair{std::move(*position), std::move(*source)};
      });
  if (!sources)
    return std::unexpected(std::move(sources.error()));
  m_active_sources.set(std::move(*sources));
  return {};
}

}

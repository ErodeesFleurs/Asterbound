import std;
import star.string;
import star.format;
import star.json;
import star.json_extra;
import star.ordered_map;
import star.matrix3;
import star.vector;
import star.animated_part_set;
import star.math_common;
import star.interpolation;

namespace Star {

AnimatedPartSet::SampleIdentity::SampleIdentity() {
  static std::atomic<std::uint64_t> next_id{1};
  value = next_id.fetch_add(1, std::memory_order_relaxed);
}

AnimatedPartSet::AnimatedPartSet() = default;

AnimatedPartSet& AnimatedPartSet::operator=(AnimatedPartSet const& other) {
  if (this != &other) {
    m_sample_identity.value = SampleIdentity{}.value;
    m_mutable_parts_exposed = m_mutable_parts_exposed || other.m_mutable_parts_exposed;
    m_state_types = other.m_state_types;
    m_parts = other.m_parts;
    m_animator_version = other.m_animator_version;
    m_sample_configuration = other.m_sample_configuration;
  }
  return *this;
}

AnimatedPartSet& AnimatedPartSet::operator=(AnimatedPartSet&& other) {
  if (this != &other) {
    m_sample_identity.value = SampleIdentity{}.value;
    m_mutable_parts_exposed = m_mutable_parts_exposed || other.m_mutable_parts_exposed;
    m_state_types = std::move(other.m_state_types);
    m_parts = std::move(other.m_parts);
    m_animator_version = other.m_animator_version;
    m_sample_configuration = std::move(other.m_sample_configuration);
  }
  return *this;
}

std::expected<AnimatedPartSet, StarException> AnimatedPartSet::create(Json config, std::uint8_t animator_version) {
  AnimatedPartSet result;
  result.m_animator_version = animator_version;
  auto state_types = config.get("stateTypes", JsonObject());
  if (!state_types) return std::unexpected(std::move(state_types.error()));
  auto state_type_entries = state_types->iterate_object();
  if (!state_type_entries) return std::unexpected(std::move(state_type_entries.error()));
  for (auto const& [state_type_name, state_type_config] : *state_type_entries) {
    if (animator_version > 0 && !state_type_config.is_type(Json::Type::Object))
      continue;
    StateType new_state_type;
    auto priority = state_type_config.get_float("priority", 0.0f);
    if (!priority) return std::unexpected(std::move(priority.error()));
    new_state_type.priority = *priority;
    auto enabled = state_type_config.get_bool("enabled", true);
    if (!enabled) return std::unexpected(std::move(enabled.error()));
    new_state_type.enabled = *enabled;
    auto default_state = state_type_config.get_string("default", "");
    if (!default_state) return std::unexpected(std::move(default_state.error()));
    new_state_type.default_state = std::move(*default_state);
    auto properties = state_type_config.get_object("properties", {});
    if (!properties) return std::unexpected(std::move(properties.error()));
    new_state_type.state_type_properties = std::move(*properties);
    auto states = state_type_config.get("states", JsonObject());
    if (!states) return std::unexpected(std::move(states.error()));
    auto state_entries = states->iterate_object();
    if (!state_entries) return std::unexpected(std::move(state_entries.error()));
    for (auto const& [state_name, state_config] : *state_entries) {
      if (animator_version > 0 && !state_config.is_type(Json::Type::Object))
        continue;
      auto new_state = std::make_shared<State>();
      auto frames = state_config.get_int("frames", 1);
      if (!frames) return std::unexpected(std::move(frames.error()));
      if (*frames < 1 || *frames > std::numeric_limits<int>::max())
        return std::unexpected(AnimatedPartSetException("Animation frame count out of range"));
      new_state->frames = *frames;
      auto cycle = state_config.get_double("cycle", 1.0);
      if (!cycle) return std::unexpected(std::move(cycle.error()));
      // Check the original binary64 representation before narrowing under fast-math.
      constexpr auto maximum_cycle = std::bit_cast<std::uint64_t>(static_cast<double>(std::numeric_limits<float>::max()));
      if (std::bit_cast<std::uint64_t>(*cycle) > maximum_cycle)
        return std::unexpected(AnimatedPartSetException("Animation cycle must be nonnegative and finite"));
      new_state->cycle = static_cast<float>(*cycle);
      auto mode_name = state_config.get_string("mode", "end");
      if (!mode_name) return std::unexpected(std::move(mode_name.error()));
      auto mode = string_to_animation_mode(*mode_name);
      if (!mode) return std::unexpected(std::move(mode.error()));
      new_state->animation_mode = *mode;
      if (new_state->cycle == 0 && (*frames != 1 || *mode != End))
        return std::unexpected(AnimatedPartSetException("Zero cycle requires a single static end frame"));
      auto transition = state_config.get_string("transition", "");
      if (!transition) return std::unexpected(std::move(transition.error()));
      new_state->transition_state = std::move(*transition);
      auto state_properties = state_config.get_object("properties", {});
      if (!state_properties) return std::unexpected(std::move(state_properties.error()));
      new_state->state_properties = std::move(*state_properties);
      auto frame_properties = state_config.get_object("frameProperties", {});
      if (!frame_properties) return std::unexpected(std::move(frame_properties.error()));
      new_state->state_frame_properties = std::move(*frame_properties);
      new_state_type.states[state_name] = std::move(new_state);
    }
    new_state_type.states.sort_by_key();
    new_state_type.active_state.state_type_name = state_type_name;
    new_state_type.active_state.reverse = false;
    new_state_type.active_state_dirty = true;
    if (new_state_type.default_state.empty() && !new_state_type.states.empty())
      new_state_type.default_state = new_state_type.states.begin()->first;
    result.m_state_types[state_type_name] = std::move(new_state_type);
  }
  result.m_state_types.sort([](auto const& a, auto const& b) {
    return b.second.priority < a.second.priority;
  });
  auto parts = config.get("parts", JsonObject());
  if (!parts) return std::unexpected(std::move(parts.error()));
  auto part_entries = parts->iterate_object();
  if (!part_entries) return std::unexpected(std::move(part_entries.error()));
  for (auto const& [part_name, part_config] : *part_entries) {
    if (animator_version > 0 && !part_config.is_type(Json::Type::Object))
      continue;
    Part new_part;
    auto properties = part_config.get_object("properties", {});
    if (!properties) return std::unexpected(std::move(properties.error()));
    new_part.part_properties = std::move(*properties);
    auto part_states = part_config.get("partStates", JsonObject());
    if (!part_states) return std::unexpected(std::move(part_states.error()));
    auto part_state_types = part_states->iterate_object();
    if (!part_state_types) return std::unexpected(std::move(part_state_types.error()));
    for (auto const& [state_type_name, state_type_config] : *part_state_types) {
      auto states = state_type_config.iterate_object();
      if (!states) return std::unexpected(std::move(states.error()));
      for (auto const& [state_name, initial_state_config] : *states) {
        auto state_config = initial_state_config;
        if (animator_version > 0 && state_config.is_type(Json::Type::String)) {
          auto name = state_config.to_string();
          if (!name) return std::unexpected(std::move(name.error()));
          auto target = state_type_config.get(*name);
          if (!target) return std::unexpected(std::move(target.error()));
          state_config = std::move(*target);
        }
        if (animator_version > 0 && !state_config.is_type(Json::Type::Object))
          continue;
        auto properties = state_config.get_object("properties", {});
        if (!properties) return std::unexpected(std::move(properties.error()));
        auto frames = state_config.get_object("frameProperties", {});
        if (!frames) return std::unexpected(std::move(frames.error()));
        new_part.part_states[state_type_name][state_name] =
          PartState{.part_state_properties = std::move(*properties), .part_state_frame_properties = std::move(*frames)};
      }
    }
    new_part.active_part.part_name = part_name;
    result.m_parts[part_name] = std::move(new_part);
  }
  std::size_t state_index = 0;
  for (auto& [name, type] : result.m_state_types) {
    type.presentation_index = state_index++;
    auto state = type.states.get(type.default_state);
    if (!state) return std::unexpected(std::move(state.error()));
    type.active_state.state_name = type.default_state;
    type.active_state_reference = state->get();
  }
  std::size_t part_index = 0;
  for (auto&& entry : result.m_parts)
    entry.second.presentation_index = part_index++;
  auto source = config.object_ptr();
  if (!source) return std::unexpected(std::move(source.error()));
  result.m_sample_configuration = std::move(*source);
  return result;
}

StringList AnimatedPartSet::state_types() const {
  return m_state_types.keys();
}

std::expected<void, StarException> AnimatedPartSet::set_state_type_enabled(String const& name, bool enabled) {
  auto state_type = m_state_types.get(name);
  if (!state_type) return std::unexpected(std::move(state_type.error()));
  if (state_type->get().enabled != enabled) {
    state_type->get().enabled = enabled;
    for (auto&& pair : m_parts)
      pair.second.active_part_dirty = true;
  }
  return {};
}

std::expected<void, StarException> AnimatedPartSet::set_enabled_state_types(StringList const& names) {
  for (auto const& name : names) {
    auto found = m_state_types.get(name);
    if (!found) return std::unexpected(std::move(found.error()));
  }
  for (auto& pair : m_state_types)
    pair.second.enabled = false;
  for (auto const& name : names) {
    auto state_type = m_state_types.get(name);
    if (!state_type) return std::unexpected(std::move(state_type.error()));
    state_type->get().enabled = true;
  }
  for (auto&& pair : m_parts)
    pair.second.active_part_dirty = true;
  return {};
}

std::expected<bool, StarException> AnimatedPartSet::state_type_enabled(String const& name) const {
  auto state_type = m_state_types.get(name);
  if (!state_type) return std::unexpected(std::move(state_type.error()));
  return state_type->get().enabled;
}

std::expected<StringList, StarException> AnimatedPartSet::states(String const& name) const {
  auto state_type = m_state_types.get(name);
  if (!state_type) return std::unexpected(std::move(state_type.error()));
  return state_type->get().states.keys();
}

StringList AnimatedPartSet::part_names() const {
  return m_parts.keys();
}

bool AnimatedPartSet::sample_prepared(Sample const& sample) const {
  return sample.owner_id == m_sample_identity.value && sample.states.size() == m_state_types.size()
      && sample.parts.size() == m_parts.size();
}

void AnimatedPartSet::prepare_sample(Sample& sample, float interpolation) const {
  sample.owner_id = m_sample_identity.value;
  sample.interpolation = std::clamp(interpolation, 0.f, 1.f);
  sample.states.resize(m_state_types.size());
  sample.parts.resize(m_parts.size());
  bool same_configuration = !sample.logical_reset && sample.configuration == m_sample_configuration && sample.animator_version == m_animator_version;
  sample.logical_reset = false;
  sample.configuration = m_sample_configuration;
  sample.animator_version = m_animator_version;
  for (auto& cache : sample.states) {
    cache.dirty = true;
    cache.initialized = false;
    cache.override_state.reset();
    if (!same_configuration)
      cache.property_state.reset();
  }
  std::size_t index = 0;
  for (auto const& entry : m_parts) {
    if (m_mutable_parts_exposed)
      entry.second.presentation_index = index++;
    auto& cache = sample.parts[entry.second.presentation_index];
    if (!same_configuration || m_mutable_parts_exposed || cache.part.part_name != entry.first) {
      cache.properties_valid = false;
      cache.part.part_name = entry.first;
    }
    cache.dirty = true;
  }
}

std::expected<bool, StarException> AnimatedPartSet::set_presentation_state(String const& type, String const& name, bool always_start, bool reverse, Sample& sample) const {
  if (!sample_prepared(sample)) return std::unexpected(AnimatedPartSetException("Unprepared animation sample"));
  auto entry = m_state_types.get(type);
  if (!entry) return std::unexpected(std::move(entry.error()));
  auto& state_type = entry->get();
  if (!sample.logical_reset) {
    auto fresh = freshen_active_state(state_type, std::ref(sample));
    if (!fresh) return std::unexpected(std::move(fresh.error()));
  }
  auto& active = sample.states[state_type.presentation_index].state;
  if (active.state_name == name && active.reverse == reverse && !always_start)
    return false;
  auto state = state_type.states.get(name);
  if (!state) return std::unexpected(std::move(state.error()));
  active.state_name = name;
  active.timer = 0;
  active.reverse = reverse;
  sample.states[state_type.presentation_index].override_state = state->get();
  sample.states[state_type.presentation_index].dirty = true;
  for (auto& cache : sample.parts)
    cache.dirty = true;
  return true;
}

std::expected<bool, StarException> AnimatedPartSet::set_active_state(String const& state_type_name, String const& state_name, bool always_start, bool reverse) {
  auto found = m_state_types.get(state_type_name);
  if (!found) return std::unexpected(std::move(found.error()));
  auto& state_type = found->get();
  if (state_type.active_state.state_name != state_name || always_start || state_type.active_state.reverse != reverse) {
    auto state = state_type.states.get(state_name);
    if (!state) return std::unexpected(std::move(state.error()));
    state_type.active_state.state_name = state_name;
    state_type.active_state.timer = 0.0f;
    state_type.previous_timer = state_type.timer_advance = 0;
    state_type.active_state.frame_progress = 0.0f;
    state_type.active_state.reverse = reverse;
    state_type.active_state_reference = state->get();
    state_type.active_state_dirty = true;
    for (auto&& pair : m_parts)
      pair.second.active_part_dirty = true;
    return true;
  }
  return false;
}

std::expected<void, StarException> AnimatedPartSet::restart_state(String const& name) {
  auto state_type = m_state_types.get(name);
  if (!state_type) return std::unexpected(std::move(state_type.error()));
  state_type->get().active_state.timer = 0.0f;
  state_type->get().previous_timer = state_type->get().timer_advance = 0;
  state_type->get().active_state_dirty = true;
  for (auto&& pair : m_parts)
    pair.second.active_part_dirty = true;
  return {};
}

std::expected<std::reference_wrapper<AnimatedPartSet::ActiveStateInformation const>, StarException> AnimatedPartSet::active_state(String const& name) const {
  auto entry = m_state_types.get(name);
  if (!entry) return std::unexpected(std::move(entry.error()));
  return resolve_state(entry->get(), std::nullopt);
}

std::expected<std::reference_wrapper<AnimatedPartSet::ActiveStateInformation const>, StarException> AnimatedPartSet::active_state(String const& name, Sample& sample) const {
  if (!sample_prepared(sample)) return std::unexpected(AnimatedPartSetException("Unprepared animation sample"));
  auto entry = m_state_types.get(name);
  if (!entry) return std::unexpected(std::move(entry.error()));
  return resolve_state(entry->get(), std::ref(sample));
}

std::expected<std::reference_wrapper<AnimatedPartSet::ActivePartInformation const>, StarException> AnimatedPartSet::active_part(String const& name) const {
  auto entry = m_parts.get(name);
  if (!entry) return std::unexpected(std::move(entry.error()));
  return resolve_part(entry->get(), std::nullopt);
}

std::expected<std::reference_wrapper<AnimatedPartSet::ActivePartInformation const>, StarException> AnimatedPartSet::active_part(String const& name, Sample& sample) const {
  if (!sample_prepared(sample)) return std::unexpected(AnimatedPartSetException("Unprepared animation sample"));
  auto entry = m_parts.get(name);
  if (!entry) return std::unexpected(std::move(entry.error()));
  return resolve_part(entry->get(), std::ref(sample));
}

std::expected<std::reference_wrapper<AnimatedPartSet::ActivePartInformation const>, StarException> AnimatedPartSet::active_part(Part const& part) const {
  return resolve_part(part, std::nullopt);
}

std::expected<std::reference_wrapper<AnimatedPartSet::ActivePartInformation const>, StarException> AnimatedPartSet::active_part(Part const& part, Sample& sample) const {
  if (!sample_prepared(sample)) return std::unexpected(AnimatedPartSetException("Unprepared animation sample"));
  return resolve_part(part, std::ref(sample));
}

std::expected<std::reference_wrapper<AnimatedPartSet::ActiveStateInformation const>, StarException> AnimatedPartSet::resolve_state(StateType const& entry, std::optional<std::reference_wrapper<Sample>> sample) const {
  if (sample && entry.presentation_index >= sample->get().states.size())
    return std::unexpected(AnimatedPartSetException("Invalid animation state index"));
  auto fresh = freshen_active_state(entry, sample);
  if (!fresh) return std::unexpected(std::move(fresh.error()));
  return std::cref(sample ? sample->get().states[entry.presentation_index].state : entry.active_state);
}

std::expected<std::reference_wrapper<AnimatedPartSet::ActivePartInformation const>, StarException> AnimatedPartSet::resolve_part(Part const& entry, std::optional<std::reference_wrapper<Sample>> sample) const {
  if (sample && entry.presentation_index >= sample->get().parts.size())
    return std::unexpected(AnimatedPartSetException("Invalid animation part index"));
  auto fresh = freshen_active_part(entry, sample);
  if (!fresh) return std::unexpected(std::move(fresh.error()));
  return std::cref(sample ? sample->get().parts[entry.presentation_index].part : entry.active_part);
}

std::expected<std::reference_wrapper<AnimatedPartSet::State const>, StarException> AnimatedPartSet::get_state(String const& type, String const& name) const {
  auto state_type = m_state_types.get(type);
  if (!state_type) return std::unexpected(std::move(state_type.error()));
  auto state = state_type->get().states.get(name);
  if (!state) return std::unexpected(std::move(state.error()));
  return std::cref(*state->get());
}

StringMap<AnimatedPartSet::Part> const& AnimatedPartSet::const_parts() const {
  return m_parts;
}

StringMap<AnimatedPartSet::Part>& AnimatedPartSet::parts() {
  // The caller may retain this reference and edit it without another parts()
  // call. Never reuse composed part properties across prepares after exposure.
  m_mutable_parts_exposed = true;
  return m_parts;
}

std::expected<std::size_t, StarException> AnimatedPartSet::active_state_index(String const& name) const {
  auto state_type = m_state_types.get(name);
  if (!state_type) return std::unexpected(std::move(state_type.error()));
  auto index = state_type->get().states.index_of(state_type->get().active_state.state_name);
  if (!index) return std::unexpected(AnimatedPartSetException("Active animation state is missing"));
  return *index;
}

std::expected<bool, StarException> AnimatedPartSet::active_state_reverse(String const& name) const {
  auto state_type = m_state_types.get(name);
  if (!state_type) return std::unexpected(std::move(state_type.error()));
  return state_type->get().active_state.reverse;
}

std::expected<bool, StarException> AnimatedPartSet::set_active_state_index(String const& name, std::size_t index, bool always_start, bool reverse) {
  auto state_type = m_state_types.get(name);
  if (!state_type) return std::unexpected(std::move(state_type.error()));
  auto state_name = state_type->get().states.key_at(index);
  if (!state_name) return std::unexpected(std::move(state_name.error()));
  return set_active_state(name, state_name->get(), always_start, reverse);
}

std::expected<void, StarException> AnimatedPartSet::update(float dt) {
  for (auto& pair : m_state_types) {
    auto& state_type = pair.second;
    auto const& state = *state_type.active_state_reference;

    state_type.previous_timer = state_type.active_state.timer;
    state_type.timer_advance = dt;
    state_type.active_state.timer += dt;
    if (state_type.active_state.timer > state.cycle) {
      if (state.animation_mode == End) {
        state_type.active_state.timer = state.cycle;
      } else if (state.animation_mode == Loop) {
        state_type.active_state.timer = std::fmod(state_type.active_state.timer, state.cycle);
      } else if (state.animation_mode == Transition) {
        auto next = state_type.states.get(state.transition_state);
        if (!next) return std::unexpected(std::move(next.error()));
        state_type.active_state.state_name = state.transition_state;
        state_type.active_state.timer = 0.0f;
        state_type.active_state_reference = next->get();
        state_type.previous_timer = state_type.timer_advance = 0;
      }
    }

    state_type.active_state_dirty = true;
  }

  for (auto&& pair : m_parts)
    pair.second.active_part_dirty = true;
  return {};
}

static std::expected<void, StarException> finish_cursor(AnimatedPartSet::StateType const& type,
    String& name, float& timer, std::shared_ptr<AnimatedPartSet::State const>& definition) {
  auto remaining = type.states.size();
  while (definition->animation_mode == AnimatedPartSet::Transition) {
    if (remaining-- == 0)
      return std::unexpected(AnimatedPartSetException("Cyclic animation transition"));
    auto next = type.states.get(definition->transition_state);
    if (!next) return std::unexpected(std::move(next.error()));
    name = definition->transition_state;
    timer = 0;
    definition = next->get();
  }
  if (definition->animation_mode == AnimatedPartSet::End)
    timer = definition->cycle;
  return {};
}

std::expected<void, StarException> AnimatedPartSet::stage_finished_states(Sample& sample) const {
  sample.logical_reset = true;
  for (auto& part : sample.parts)
    part.properties_valid = false;
  for (auto const& [name, type] : m_state_types) {
    auto& cache = sample.states[type.presentation_index];
    cache.state.state_type_name = name;
    cache.state.state_name = type.active_state.state_name;
    cache.state.reverse = type.active_state.reverse;
    cache.state.timer = type.active_state.timer;
    cache.override_state = type.active_state_reference;
    cache.initialized = true;
    auto finished = finish_cursor(type, cache.state.state_name, cache.state.timer, cache.override_state);
    if (!finished) return finished;
  }
  return {};
}

void AnimatedPartSet::commit_finished_states(Sample const& sample) {
  for (auto& [name, type] : m_state_types) {
    auto const& cache = sample.states[type.presentation_index];
    type.active_state.state_name = cache.state.state_name;
    type.active_state.reverse = cache.state.reverse;
    type.active_state.timer = type.previous_timer = cache.state.timer;
    type.timer_advance = 0;
    type.active_state_reference = cache.override_state;
    type.active_state_dirty = true;
  }
  for (auto&& entry : m_parts)
    entry.second.active_part_dirty = true;
}

std::expected<void, StarException> AnimatedPartSet::finish_animations() {
  for (auto& [name, type] : m_state_types) {
    auto finished = finish_cursor(type, type.active_state.state_name, type.active_state.timer, type.active_state_reference);
    if (!finished) return finished;
    type.previous_timer = type.active_state.timer;
    type.timer_advance = 0;
    type.active_state_dirty = true;
  }

  for (auto&& pair : m_parts)
    pair.second.active_part_dirty = true;
  return {};
}

std::expected<AnimatedPartSet::AnimationMode, StarException> AnimatedPartSet::string_to_animation_mode(String const& string) {
  auto end = string.equals("end", String::CaseInsensitive);
  if (!end) return std::unexpected(std::move(end.error()));
  if (*end) return End;
  auto loop = string.equals("loop", String::CaseInsensitive);
  if (!loop) return std::unexpected(std::move(loop.error()));
  if (*loop) return Loop;
  auto transition = string.equals("transition", String::CaseInsensitive);
  if (!transition) return std::unexpected(std::move(transition.error()));
  if (*transition) return Transition;
  return std::unexpected(AnimatedPartSetException(Star::format("No such AnimationMode '{}'", string)));
}

static std::expected<void, StarException> compose_frame_properties(JsonObject& current, JsonObject& next,
    JsonObject const& base, JsonObject const& overrides, JsonObject const& frames, unsigned frame, unsigned next_frame) {
  current = base;
  current.merge(overrides, true);
  if (frame != next_frame)
    next = current;
  for (auto const& [name, values] : frames) {
    auto size = values.size();
    if (!size) return std::unexpected(std::move(size.error()));
    if (frame < *size) {
      auto value = values.get(frame);
      if (!value) return std::unexpected(std::move(value.error()));
      current[name] = std::move(*value);
    }
    if (frame != next_frame && next_frame < *size) {
      auto value = values.get(next_frame);
      if (!value) return std::unexpected(std::move(value.error()));
      next[name] = std::move(*value);
    }
  }
  if (frame == next_frame)
    next = current;
  return {};
}

std::expected<void, StarException> AnimatedPartSet::freshen_active_state(StateType const& state_type,
    std::optional<std::reference_wrapper<Sample>> sample) const {
  auto cache = sample.transform([&](std::reference_wrapper<Sample> view) {
    return std::ref(view.get().states[state_type.presentation_index]);
  });
  auto& dirty = cache ? cache->get().dirty : state_type.active_state_dirty;
  if (!dirty)
    return {};
  auto& active = cache ? cache->get().state : state_type.active_state;
  auto const& definition = cache && cache->get().override_state ? cache->get().override_state : state_type.active_state_reference;
  auto const& state = *definition;
  if (cache && !cache->get().initialized) {
    active.state_type_name = state_type.active_state.state_type_name;
    active.state_name = state_type.active_state.state_name;
    active.reverse = state_type.active_state.reverse;
    active.timer = state_type.active_state.timer;
    float interpolation = sample->get().interpolation;
    if (interpolation < 1) {
      active.timer = state_type.previous_timer + state_type.timer_advance * interpolation;
      if (state.animation_mode == Loop && active.timer > state.cycle)
        active.timer = std::fmod(active.timer, state.cycle);
      else
        active.timer = std::min(active.timer, state.cycle);
    }
    cache->get().initialized = true;
  }

  // Preserve the original float timing calculation, then bound before the integer conversion.
  double progress = state.cycle == 0 ? 0 : active.timer / state.cycle * state.frames;
  if (!is_finite(progress))
    return std::unexpected(AnimatedPartSetException("Animation frame progress must be finite"));
  unsigned last = state.frames - 1;
  active.frame_progress = std::fmod(progress, 1.0);
  active.frame = static_cast<unsigned>(std::clamp(progress, 0.0, static_cast<double>(last)));
  if (active.reverse) {
    active.frame = last - active.frame;
    active.next_frame = active.frame ? active.frame - 1 : state.animation_mode == Loop ? last : 0;
  } else {
    active.next_frame = active.frame < last ? active.frame + 1 : state.animation_mode == Loop ? 0 : last;
  }

  if (!cache || cache->get().property_state != definition || cache->get().property_frame != active.frame
      || cache->get().property_next_frame != active.next_frame) {
    if (cache)
      cache->get().property_state.reset();
    auto result = compose_frame_properties(active.properties, active.next_properties, state_type.state_type_properties,
        state.state_properties, state.state_frame_properties, active.frame, active.next_frame);
    if (!result) return result;
    if (cache) {
      cache->get().property_state = definition;
      cache->get().property_frame = active.frame;
      cache->get().property_next_frame = active.next_frame;
    }
  }
  dirty = false;
  return {};
}

static std::expected<Mat3F, StarException> apply_transforms(Mat3F mat, Json const& transforms, JsonObject const& properties) {
  auto entries = transforms.iterate_array();
  if (!entries)
    return std::unexpected(std::move(entries.error()));
  for (auto const& v : *entries) {
    auto action = v.get_string(0);
    if (!action)
      return std::unexpected(std::move(action.error()));
    if (*action == "reset") {
      mat = Mat3F::identity();
    } else if (*action == "translate") {
      auto translation = v.get(1).and_then(json_to_vec2f);
      if (!translation)
        return std::unexpected(std::move(translation.error()));
      mat.translate(*translation);
    } else if (*action == "rotate" || *action == "rotateDegrees" || *action == "scale") {
      auto center_property = properties.find_ref(*action == "scale" ? std::string_view("scalingCenter")
                                                                   : std::string_view("rotationCenter"));
      // Preserve validation of a supplied default even when an explicit center wins.
      if (center_property) {
        auto default_center = center_property->get().array_ptr();
        if (!default_center)
          return std::unexpected(std::move(default_center.error()));
      }
      auto center_value = v.find(2);
      if (!center_value)
        return std::unexpected(std::move(center_value.error()));
      std::expected<Vec2F, StarException> center = Vec2F();
      if (*center_value)
        center = json_to_vec2f((*center_value)->get());
      else if (center_property)
        center = json_to_vec2f(center_property->get());
      if (!center)
        return std::unexpected(std::move(center.error()));
      if (*action == "scale") {
        auto scale = v.get(1).and_then(json_to_vec2f);
        if (!scale)
          return std::unexpected(std::move(scale.error()));
        mat.scale(*scale, *center);
      } else {
        auto angle = v.get_float(1);
        if (!angle)
          return std::unexpected(std::move(angle.error()));
        mat.rotate(*action == "rotateDegrees" ? *angle * std::numbers::pi / 180 : *angle, *center);
      }
    } else if (*action == "transform") {
      std::array<float, 6> values;
      for (std::size_t i = 0; i < values.size(); ++i) {
        auto value = v.get_float(i + 1);
        if (!value)
          return std::unexpected(std::move(value.error()));
        values[i] = *value;
      }
      mat = Mat3F(values[0], values[1], values[2], values[3], values[4], values[5], 0, 0, 1) * mat;
    }
  }
  return mat;
}

static std::array<std::uint32_t, 6> pose_key(AnimatedPartSet::AffinePose const& pose) {
  // atan2 distinguishes signed zeros. Compare float representations rather than numeric equality.
  return std::bit_cast<std::array<std::uint32_t, 6>>(std::array{pose.x_translation_animation, pose.y_translation_animation,
      pose.x_scale_animation, pose.y_scale_animation, pose.x_shear_animation, pose.y_shear_animation});
}

static std::expected<AnimatedPartSet::Sample::TransformEndpoints, StarException> transform_endpoints(
    AnimatedPartSet::AffinePose const& base, AnimatedPartSet::ActivePartInformation const& part, bool sampled, bool use_transforms) {
  AnimatedPartSet::Sample::TransformEndpoints result{base, base, std::nullopt};
  // A sampled base historically passes through a matrix decomposition before applying actions.
  if (sampled)
    result.current.set_animation_affine_transform(base.animation_affine_transform());
  if (!use_transforms)
    return result;
  auto transforms = part.properties.find_ref("transforms");
  if (!transforms)
    return result;
  auto matrix = result.current.animation_affine_transform();
  auto current = apply_transforms(matrix, transforms->get(), part.properties);
  if (!current) return std::unexpected(std::move(current.error()));
  result.current.set_animation_affine_transform(*current);
  if (auto value = part.properties.find_ref("interpolated")) {
    auto interpolated = value->get().to_bool();
    if (!interpolated) return std::unexpected(std::move(interpolated.error()));
    if (*interpolated) {
      if (auto transforms = part.next_properties.find_ref("transforms")) {
        auto next = apply_transforms(matrix, transforms->get(), part.next_properties);
        if (!next) return std::unexpected(std::move(next.error()));
        result.next.emplace().set_animation_affine_transform(*next);
      }
    }
  }
  return result;
}

static void apply_endpoints(AnimatedPartSet::AffinePose& pose, AnimatedPartSet::Sample::TransformEndpoints const& endpoints, float progress) {
  auto const& current = endpoints.current;
  if (!endpoints.next) {
    pose = current;
    return;
  }
  auto const& next = *endpoints.next;
  pose.x_translation_animation = lerp(progress, current.x_translation_animation, next.x_translation_animation);
  pose.y_translation_animation = lerp(progress, current.y_translation_animation, next.y_translation_animation);
  pose.x_scale_animation = lerp(progress, current.x_scale_animation, next.x_scale_animation);
  pose.y_scale_animation = lerp(progress, current.y_scale_animation, next.y_scale_animation);
  pose.x_shear_animation = angle_lerp(progress, current.x_shear_animation, next.x_shear_animation);
  pose.y_shear_animation = angle_lerp(progress, current.y_shear_animation, next.y_shear_animation);
}

std::expected<void, StarException> AnimatedPartSet::freshen_active_part(Part const& part,
    std::optional<std::reference_wrapper<Sample>> sample) const {
  auto cache = sample.transform([&](std::reference_wrapper<Sample> view) {
    return std::ref(view.get().parts[part.presentation_index]);
  });
  auto& dirty = cache ? cache->get().dirty : part.active_part_dirty;
  if (!dirty)
    return {};
  auto& active = cache ? cache->get().part : part.active_part;
  if (!cache)
    part.active_part_base_pose = active;
  bool logical_reset = sample && sample->get().logical_reset;
  AffinePose const& base = cache && (logical_reset || part.active_part_dirty) ? static_cast<AffinePose const&>(part.active_part) : part.active_part_base_pose;

  struct Selection {
    ActiveStateInformation const& state;
    PartState const& part_state;
    std::shared_ptr<State const> const& definition;
  };
  std::optional<Selection> selected;
  // Resolve only enabled types with a part-state table, in priority order.
  for (auto const& [name, type] : m_state_types) {
    if (!type.enabled)
      continue;
    auto matches = part.part_states.find_ref(name);
    if (!matches)
      continue;
    auto state = resolve_state(type, sample);
    if (!state) return std::unexpected(std::move(state.error()));
    if (auto match = matches->get().find_ref(state->get().state_name)) {
      auto const& definition = sample ? sample->get().states[type.presentation_index].property_state : type.active_state_reference;
      selected.emplace(state->get(), match->get(), definition);
      break;
    }
  }

  unsigned frame = selected ? selected->state.frame : 0;
  unsigned next_frame = selected ? selected->state.next_frame : 0;
  bool compose = !cache || !cache->get().properties_valid || m_mutable_parts_exposed
      || (selected ? cache->get().state != selected->definition : bool(cache->get().state))
      || cache->get().frame != frame || cache->get().next_frame != next_frame;
  if (compose) {
    if (cache)
      cache->get().properties_valid = false;
    if (selected) {
      active.active_state = selected->state;
      auto result = compose_frame_properties(active.properties, active.next_properties, part.part_properties,
          selected->part_state.part_state_properties, selected->part_state.part_state_frame_properties, frame, next_frame);
      if (!result) return result;
    } else {
      active.active_state.reset();
      active.properties = part.part_properties;
      active.next_properties = part.part_properties;
    }
  } else if (selected) {
    active.active_state->timer = selected->state.timer;
    active.active_state->frame_progress = selected->state.frame_progress;
    active.active_state->reverse = selected->state.reverse;
  }

  float progress = selected ? selected->state.frame_progress : 1;
  if (!compose && pose_key(base) == pose_key(cache->get().transforms.base)) {
    apply_endpoints(active, cache->get().transforms, progress);
  } else {
    if (cache)
      cache->get().properties_valid = false;
    auto endpoints = transform_endpoints(base, active, sample.has_value() && !logical_reset, version() > 0);
    if (!endpoints) return std::unexpected(std::move(endpoints.error()));
    apply_endpoints(active, *endpoints, progress);
    // A retained parts() reference can mutate any definition between prepares.
    // Only immutable, fully validated property/transform pairs are reusable.
    if (cache && !m_mutable_parts_exposed) {
      auto& entry = cache->get();
      entry.transforms = std::move(*endpoints);
      if (compose) {
        if (selected)
          entry.state = selected->definition;
        else
          entry.state.reset();
        entry.frame = frame;
        entry.next_frame = next_frame;
      }
      entry.properties_valid = true;
    }
  }
  dirty = false;
  return {};
}

void AnimatedPartSet::AffinePose::set_animation_affine_transform(Mat3F const& matrix) {
  x_translation_animation = matrix[0][2];
  y_translation_animation = matrix[1][2];
  x_scale_animation = std::sqrt(square(matrix[0][0]) + square(matrix[0][1]));
  y_scale_animation = std::sqrt(square(matrix[1][0]) + square(matrix[1][1]));
  x_shear_animation = std::atan2(matrix[0][1], matrix[0][0]);
  y_shear_animation = std::atan2(matrix[1][0], matrix[1][1]);
}

Mat3F AnimatedPartSet::AffinePose::animation_affine_transform() const {
  return Mat3F(
      x_scale_animation * std::cos(x_shear_animation), x_scale_animation * std::sin(x_shear_animation), x_translation_animation,
      y_scale_animation * std::sin(y_shear_animation), y_scale_animation * std::cos(y_shear_animation), y_translation_animation,
      0, 0, 1
    );
}

std::uint8_t AnimatedPartSet::version() const {
  return m_animator_version;
}

static std::expected<Json, StarException> frame_property(JsonObject const& frames, JsonObject const& overrides,
    JsonObject const& base, String const& key, int frame) {
  if (auto property = frames.find_ref(key)) {
    auto size = property->get().size();
    if (!size) return std::unexpected(std::move(size.error()));
    if (frame >= 0 && static_cast<std::size_t>(frame) < *size)
      return property->get().get(frame);
  }
  if (auto value = overrides.find_ref(key))
    return value->get();
  if (auto value = base.find_ref(key))
    return value->get();
  return Json();
}

std::expected<Json, StarException> AnimatedPartSet::get_state_frame_property(String const& type_name, String const& property_name, String const& state_name, int frame) const {
  auto type = m_state_types.get(type_name);
  if (!type) return std::unexpected(std::move(type.error()));
  auto state = type->get().states.get(state_name);
  if (!state) return std::unexpected(std::move(state.error()));
  return frame_property(state->get()->state_frame_properties, state->get()->state_properties,
      type->get().state_type_properties, property_name, frame);
}

std::expected<Json, StarException> AnimatedPartSet::get_part_state_frame_property(String const& part_name, String const& property_name, String const& type_name, String const& state_name, int frame) const {
  auto part = m_parts.get(part_name);
  if (!part) return std::unexpected(std::move(part.error()));
  auto type = part->get().part_states.get(type_name);
  if (!type) return std::unexpected(std::move(type.error()));
  auto state = type->get().get(state_name);
  if (!state) return std::unexpected(std::move(state.error()));
  return frame_property(state->get().part_state_frame_properties, state->get().part_state_properties,
      part->get().part_properties, property_name, frame);
}


}

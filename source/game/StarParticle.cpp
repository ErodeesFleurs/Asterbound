import star.format;
import star.memory;
import std;
import star.optional;
import star.json;
import star.json_extra;
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
import star.string;
import star.data_stream_extra;
import star.random;

import star.animation;
import star.particle;
import star.asset_source;
import star.assets;
import star.drawable;
import star.animation;
import star.root;

import star.vector;
import star.math_common;
import star.world_geometry;
namespace Star {

namespace {

std::expected<Vec2F, StarException> read_vec2f(Json const& config, String const& key, Vec2F fallback = {}) {
  auto value = config.find(key);
  if (!value)
    return std::unexpected(std::move(value.error()));
  if (*value)
    return json_to_vec2f((*value)->get());
  return fallback;
}

} // namespace

std::expected<Particle, StarException> Particle::create(Root& root, Json const& config, String const& path) {
  Particle particle;
  auto type_name = config.get_string("type", "variance");
  if (!type_name)
    return std::unexpected(std::move(type_name.error()));
  auto type = enum_value<type_names>(*type_name);
  if (!type)
    return std::unexpected(std::move(type.error()));
  particle.type = *type;
  particle.color = particle.type == Type::Variance ? Color::palette_clear : Color::palette_white;
  auto size = config.get_float("size", particle.type == Type::Variance ? 0.0f : 1.0f);
  if (!size)
    return std::unexpected(std::move(size.error()));
  particle.size = particle.base_size = *size;
  auto string = config.get_string("string", "");
  if (!string)
    return std::unexpected(std::move(string.error()));
  auto animation = config.get_string("animation", std::move(*string));
  if (!animation)
    return std::unexpected(std::move(animation.error()));
  auto text = config.get_string("text", std::move(*animation));
  if (!text)
    return std::unexpected(std::move(text.error()));
  auto image = config.get_string("image", std::move(*text));
  if (!image)
    return std::unexpected(std::move(image.error()));
  particle.string = std::move(*image);
  if ((particle.type == Type::Textured || particle.type == Type::Animated) && !particle.string.utf8().starts_with('/'))
    particle.string = AssetPath::relative_to(path, particle.string);
  if (particle.type == Type::Animated) {
    auto length = particle.string.size();
    if (!length)
      return std::unexpected(std::move(length.error()));
  }
  particle.rebuild_payload();
  if (particle.type == Type::Animated) {
    auto initialized = particle.initialize_animation(root);
    if (!initialized)
      return std::unexpected(std::move(initialized.error()));
  }
  auto color_config = config.find("color");
  if (!color_config)
    return std::unexpected(std::move(color_config.error()));
  if (*color_config) {
    auto color = json_to_color((*color_config)->get());
    if (!color)
      return std::unexpected(std::move(color.error()));
    particle.color = *color;
  }
  auto light_config = config.find("light");
  if (!light_config)
    return std::unexpected(std::move(light_config.error()));
  if (*light_config) {
    auto light = json_to_color((*light_config)->get());
    if (!light)
      return std::unexpected(std::move(light.error()));
    particle.light = *light;
  }
  auto fade = config.get_float("fade", 0.0f);
  if (!fade)
    return std::unexpected(std::move(fade.error()));
  particle.fade = *fade;
  auto fullbright = config.get_bool("fullbright", false);
  if (!fullbright)
    return std::unexpected(std::move(fullbright.error()));
  particle.fullbright = *fullbright;
  auto position = read_vec2f(config, "position");
  if (!position)
    return std::unexpected(std::move(position.error()));
  particle.position = *position;
  auto initial_velocity = config.find("initialVelocity");
  if (!initial_velocity)
    return std::unexpected(std::move(initial_velocity.error()));
  auto velocity = *initial_velocity ? json_to_vec2f((*initial_velocity)->get()) : read_vec2f(config, "velocity");
  if (!velocity)
    return std::unexpected(std::move(velocity.error()));
  particle.velocity = *velocity;
  auto final_velocity = read_vec2f(config, "finalVelocity", particle.type == Type::Variance ? Vec2F() : particle.velocity);
  if (!final_velocity)
    return std::unexpected(std::move(final_velocity.error()));
  particle.final_velocity = *final_velocity;
  auto approach = read_vec2f(config, "approach");
  if (!approach)
    return std::unexpected(std::move(approach.error()));
  particle.approach = *approach;
  auto flip = config.get_bool("flip", false);
  if (!flip)
    return std::unexpected(std::move(flip.error()));
  particle.flip = *flip;
  auto flippable = config.get_bool("flippable", true);
  if (!flippable)
    return std::unexpected(std::move(flippable.error()));
  particle.flippable = *flippable;
  auto rotation = config.get_float("rotation", 0.0f);
  if (!rotation)
    return std::unexpected(std::move(rotation.error()));
  particle.rotation = *rotation * std::numbers::pi / 180.0f;
  auto angular_velocity = config.get_float("angularVelocity", 0.0f);
  if (!angular_velocity)
    return std::unexpected(std::move(angular_velocity.error()));
  particle.angular_velocity = *angular_velocity * std::numbers::pi / 180.0f;
  auto length = config.get_float("length", 10.0f);
  if (!length)
    return std::unexpected(std::move(length.error()));
  particle.length = *length;
  auto action_name = config.get_string("destructionAction", "None");
  if (!action_name)
    return std::unexpected(std::move(action_name.error()));
  auto action = enum_value<destruction_action_names>(*action_name);
  if (!action)
    return std::unexpected(std::move(action.error()));
  particle.destruction_action = *action;
  auto destruction_image = config.get_string("destructionImage", "");
  if (!destruction_image)
    return std::unexpected(std::move(destruction_image.error()));
  if (particle.destruction_action == DestructionAction::Image && !destruction_image->utf8().starts_with('/'))
    *destruction_image = AssetPath::relative_to(path, *destruction_image);
  particle.destruction_image = std::move(*destruction_image);
  auto destruction_time = config.get_float("destructionTime", 0.0f);
  if (!destruction_time)
    return std::unexpected(std::move(destruction_time.error()));
  particle.destruction_time = *destruction_time;
  auto time_to_live = config.get_float("timeToLive", 0.0f);
  if (!time_to_live)
    return std::unexpected(std::move(time_to_live.error()));
  particle.time_to_live = *time_to_live;
  auto layer_name = config.get_string("layer", "middle");
  if (!layer_name)
    return std::unexpected(std::move(layer_name.error()));
  auto layer = enum_value<layer_names>(*layer_name);
  if (!layer)
    return std::unexpected(std::move(layer.error()));
  particle.layer = *layer;
  auto underwater_only = config.get_bool("underwaterOnly", false);
  if (!underwater_only)
    return std::unexpected(std::move(underwater_only.error()));
  particle.underwater_only = *underwater_only;
  auto collides_foreground = config.get_bool("collidesForeground", particle.layer != Layer::Front || particle.underwater_only);
  if (!collides_foreground)
    return std::unexpected(std::move(collides_foreground.error()));
  particle.collides_foreground = *collides_foreground;
  auto collides_liquid = config.get_bool("collidesLiquid", particle.type == Type::Ember);
  if (!collides_liquid)
    return std::unexpected(std::move(collides_liquid.error()));
  particle.collides_liquid = *collides_liquid;
  auto ignore_wind = config.get_bool("ignoreWind", true);
  if (!ignore_wind)
    return std::unexpected(std::move(ignore_wind.error()));
  particle.ignore_wind = *ignore_wind;
  auto trail = config.get_bool("trail", false);
  if (!trail)
    return std::unexpected(std::move(trail.error()));
  particle.trail = *trail;
  return particle;
}

Particle::Particle(Particle&& other) noexcept(std::is_nothrow_move_constructible_v<ParticleFields>)
  : ParticleFields(std::move(other)), presentation_identity(std::exchange(other.presentation_identity, 0)) {
  other.animation.reset();
}

Particle& Particle::operator=(Particle&& other) noexcept(std::is_nothrow_move_assignable_v<ParticleFields>) {
  if (this != std::addressof(other)) {
    ParticleFields::operator=(std::move(other));
    presentation_identity = std::exchange(other.presentation_identity, 0);
    other.animation.reset();
  }
  return *this;
}

void Particle::rebuild_payload() {
  image = AssetPath::split(string);
  if (type == Type::Animated)
    directives = image.directives;
}

std::expected<Json, StarException> Particle::to_json() const {
  auto type_name = enum_name<type_names>(type);
  if (!type_name)
    return std::unexpected(std::move(type_name.error()));
  auto action_name = enum_name<destruction_action_names>(destruction_action);
  if (!action_name)
    return std::unexpected(std::move(action_name.error()));
  auto layer_name = enum_name<layer_names>(layer);
  if (!layer_name)
    return std::unexpected(std::move(layer_name.error()));
  return JsonObject{{"type", *type_name},
                    {"size", size},
                    {"string", string},
                    {"color", json_from_color(color)},
                    {"light", json_from_color(light)},
                    {"fade", fade},
                    {"fullbright", fullbright},
                    {"position", json_from_vec2f(position)},
                    {"velocity", json_from_vec2f(velocity)},
                    {"finalVelocity", json_from_vec2f(final_velocity)},
                    {"approach", json_from_vec2f(approach)},
                    {"flip", flip},
                    {"flippable", flippable},
                    {"rotation", rotation * 180.0f / std::numbers::pi},
                    {"angularVelocity", angular_velocity * 180.0f / std::numbers::pi},
                    {"length", length},
                    {"destructionAction", *action_name},
                    {"destructionImage", AssetPath::join(destruction_image)},
                    {"destructionTime", destruction_time},
                    {"timeToLive", time_to_live},
                    {"layer", *layer_name},
                    {"collidesForeground", collides_foreground},
                    {"collidesLiquid", collides_liquid},
                    {"underwaterOnly", underwater_only},
                    {"ignoreWind", ignore_wind},
                    {"trail", trail}};
}

void Particle::translate(Vec2F const& pos) {
  position += pos;
}

std::expected<void, StarException> Particle::update(Root& root, float dt, Vec2F const& wind) {
  auto motion = next_motion(dt, wind);
  if (type == Type::Animated && !animation && !(motion.time_to_live < -destruction_time)
      && !will_use_destruction_image(motion.time_to_live, false)) {
    auto initialized = initialize_animation(root);
    if (!initialized)
      return std::unexpected(std::move(initialized.error()));
  }
  advance(dt, motion);
  return {};
}

Particle::Motion Particle::next_motion(float dt, Vec2F const& wind) const {
  auto target = ignore_wind ? final_velocity : final_velocity + wind;
  Vec2F next_velocity(
      Star::approach<float>(target[0], velocity[0], approach[0] * dt),
      Star::approach<float>(target[1], velocity[1], approach[1] * dt));
  return {next_velocity, position + (velocity + next_velocity) * 0.5f * dt, time_to_live - dt};
}

void Particle::advance(float dt, Motion const& motion) {
  velocity = motion.velocity;
  position = motion.position;
  rotation += angular_velocity * dt;

  if (light != Color::palette_clear)
    light.fade(fade * dt);

  time_to_live = motion.time_to_live;
  if (time_to_live < 0.0f)
    destruction_update();

  if (type == Type::Animated && animation)
    animation->update(dt);
}

bool Particle::will_use_destruction_image(float lifetime, bool foreground_collision) const {
  return (lifetime < 0.0f || (foreground_collision && lifetime >= 0.0f))
      && destruction_time > 0.0f && destruction_action == DestructionAction::Image && !destruction_set;
}

bool Particle::dead() const {
  return time_to_live < -destruction_time;
}

Particle::PresentationState Particle::presentation_state() const {
  return {position, velocity, rotation, size, color, light, time_to_live, type, destruction_set,
      animation ? std::optional(animation->playback_state()) : std::nullopt};
}

void Particle::sample_presentation(Particle const& current, PresentationState const& previous,
    WorldGeometry const& geometry, float interpolation, float timestep, std::uint64_t identity) {
  float alpha = std::clamp(interpolation, 0.0f, 1.0f);
  bool same_state = previous.type == current.type && previous.destruction_set == current.destruction_set;
  if (presentation_identity != identity || type != current.type || destruction_set != current.destruction_set
      || (type == Type::Animated && animation.has_value() != current.animation.has_value())) {
    type = current.type;
    string = type == Type::Text ? current.string : String();
    image = type == Type::Textured ? current.image : AssetPath();
    directives = type == Type::Textured ? current.directives : DirectivesGroup();
    animation = type == Type::Animated ? current.animation : std::nullopt;
    fullbright = current.fullbright;
    flip = current.flip;
    flippable = current.flippable;
    layer = current.layer;
    length = current.length;
    presentation_identity = identity;
    destruction_set = current.destruction_set;
  }

  position = previous.position + geometry.diff(current.position, previous.position) * alpha;
  velocity = previous.velocity + (current.velocity - previous.velocity) * alpha;
  // Rotation is deliberately unwrapped: a particle can turn more than half a
  // revolution in one tick, and its angular velocity defines that motion.
  rotation = same_state
      ? previous.rotation + (current.rotation - previous.rotation) * alpha : current.rotation;
  size = same_state
      ? previous.size + (current.size - previous.size) * alpha : current.size;
  color = same_state ? previous.color.mix(current.color, alpha) : current.color;
  light = previous.light.mix(current.light, alpha);
  time_to_live = previous.time_to_live + (current.time_to_live - previous.time_to_live) * alpha;

  // Destruction begins at the sampled lifetime boundary, not at the beginning
  // of the tick that crosses it. Image changes remain discrete tick events.
  if (same_state && current.destruction_time > 0) {
    float factor = (time_to_live + current.destruction_time) / current.destruction_time;
    if (current.destruction_action == DestructionAction::Shrink)
      size = time_to_live < 0 ? current.base_size * factor : previous.size;
    else if (current.destruction_action == DestructionAction::Fade) {
      color = previous.color;
      if (time_to_live < 0)
        color.set_alpha_f(factor);
    }
  }
  if (animation && previous.animation && same_state)
    animation->sample_playback(*previous.animation, timestep * alpha);
}

void Particle::apply_variance(Root& root, Particle const& variance) {
  std::array<float, 11> draws;
  root.random().fill_uniform(draws);
  size += variance.size * (draws[0] * 2.0f - 1.0f);
  position += {variance.position[0] * (draws[1] * 2.0f - 1.0f), variance.position[1] * (draws[2] * 2.0f - 1.0f)};
  velocity += {variance.velocity[0] * (draws[3] * 2.0f - 1.0f), variance.velocity[1] * (draws[4] * 2.0f - 1.0f)};
  final_velocity +=
      {variance.final_velocity[0] * (draws[5] * 2.0f - 1.0f), variance.final_velocity[1] * (draws[6] * 2.0f - 1.0f)};
  rotation += variance.rotation * (draws[7] * 2.0f - 1.0f);
  angular_velocity += variance.angular_velocity * (draws[8] * 2.0f - 1.0f);
  length += variance.length * (draws[9] * 2.0f - 1.0f);
  time_to_live += variance.time_to_live * (draws[10] * 2.0f - 1.0f);
}

void Particle::collide(Vec2F const& collision_position) {
  position = collision_position;
  approach = Vec2F();
  velocity = final_velocity = Vec2F();
  destroy(true);
}

void Particle::destroy(bool with_destruction) {
  if (with_destruction) {
    if (time_to_live >= 0.0f) {
      time_to_live = 0.0f;
      destruction_update();
    }
  } else {
    time_to_live = -destruction_time - 1.0f;
  }
}

void Particle::destruction_update() {
  if (destruction_time > 0) {
    float destruction_factor = (time_to_live + destruction_time) / destruction_time;
    if (destruction_action == DestructionAction::Shrink) {
      size = base_size * destruction_factor;
    } else if (destruction_action == DestructionAction::Fade) {
      color.set_alpha_f(destruction_factor);
    } else if (destruction_action == DestructionAction::Image) {
      if (!destruction_set) {
        size = 1.0f;
        color = Color::palette_white;
        type = Particle::Type::Textured;
        image = destruction_image;
        angular_velocity = 0.0f;
        length = 0.0f;
        rotation = 0.0f;
        destruction_set = true;
      }
    }
  }
}

std::expected<void, StarException> Particle::initialize_animation(Root& root) {
  if (animation)
    return {};
  auto prepared = prepare_animation(root);
  if (!prepared)
    return std::unexpected(std::move(prepared.error()));
  activate_animation(std::move(*prepared));
  return {};
}

std::expected<Particle::PreparedAnimation, StarException> Particle::prepare_animation(Root& root) const {
  return Animation::prepare(root, AssetPath::remove_directives(string));
}

void Particle::activate_animation(PreparedAnimation&& prepared) {
  prepared.initialize_variant();
  prepared.animation.set_processing(directives);
  animation.emplace(std::move(prepared.animation));
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, Particle const& particle) {
  auto type = ds.viwrite(particle.type);
  if (!type)
    return std::unexpected(std::move(type.error()));
  auto fields = ds << particle.size << particle.string << particle.color << particle.light << particle.fade
      << particle.position << particle.velocity << particle.final_velocity << particle.approach << particle.rotation
      << particle.flippable << particle.flip << particle.angular_velocity << particle.length;
  if (!fields)
    return std::unexpected(std::move(fields.error()));
  auto action = ds.viwrite(particle.destruction_action);
  if (!action)
    return std::unexpected(std::move(action.error()));
  return ds << particle.destruction_image << particle.destruction_time << particle.time_to_live << particle.layer
      << particle.collides_foreground << particle.collides_liquid << particle.underwater_only << particle.ignore_wind;
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, Particle& particle) {
  Particle decoded;
  auto type = ds.viread(decoded.type);
  if (!type)
    return std::unexpected(std::move(type.error()));
  auto fields = ds >> decoded.size >> decoded.string >> decoded.color >> decoded.light >> decoded.fade
      >> decoded.position >> decoded.velocity >> decoded.final_velocity >> decoded.approach >> decoded.rotation
      >> decoded.flippable >> decoded.flip >> decoded.angular_velocity >> decoded.length;
  if (!fields)
    return std::unexpected(std::move(fields.error()));
  auto action = ds.viread(decoded.destruction_action);
  if (!action)
    return std::unexpected(std::move(action.error()));
  fields = ds >> decoded.destruction_image >> decoded.destruction_time >> decoded.time_to_live >> decoded.layer
      >> decoded.collides_foreground >> decoded.collides_liquid >> decoded.underwater_only >> decoded.ignore_wind;
  if (!fields)
    return fields;

  // The old record stores neither original shrink size nor live animation or
  // destruction state. Start from the transmitted size and rebuild the payload;
  // animation is loaded on demand, and other omitted fields keep fresh defaults.
  decoded.base_size = decoded.size;
  decoded.rebuild_payload();
  particle = std::move(decoded);
  return fields;
}

ParticleVariantCreator make_particle_variant_creator(Root& root, Particle particle, Particle variance) {
  return [&root, particle = std::move(particle), variance = std::move(variance)] {
    auto result = particle;
    result.apply_variance(root, variance);
    return result;
  };
}

}

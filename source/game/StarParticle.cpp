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

Particle::Particle() {
  type = Type::Variance;
  size = 0;
  baseSize = 0;
  color = Color::White;
  light = Color::Clear;
  fade = 0;
  fullbright = false;
  position = velocity = finalVelocity = approach = Vec2F();
  rotation = 0.0f;
  angularVelocity = 0.0f;
  timeToLive = 0.0f;
  layer = Layer::Middle;
  collidesForeground = true;
  collidesLiquid = true;
  underwaterOnly = false;
  ignoreWind = true;
  length = 0;
  destructionAction = DestructionAction::None;
  destructionTime = 0.0f;
  destructionSet = false;
  trail = false;
  flippable = true;
  flip = false;
}

std::expected<Particle, StarException> Particle::create(Root& root, Json const& config, String const& path) {
  Particle particle;
  auto typeName = config.getString("type", "variance");
  if (!typeName)
    return std::unexpected(std::move(typeName.error()));
  auto type = enumValue<TypeNames>(*typeName);
  if (!type)
    return std::unexpected(std::move(type.error()));
  particle.type = *type;
  particle.color = particle.type == Type::Variance ? Color::Clear : Color::White;
  auto size = config.getFloat("size", particle.type == Type::Variance ? 0.0f : 1.0f);
  if (!size)
    return std::unexpected(std::move(size.error()));
  particle.size = particle.baseSize = *size;
  auto string = config.getString("string", "");
  if (!string)
    return std::unexpected(std::move(string.error()));
  auto animation = config.getString("animation", *string);
  if (!animation)
    return std::unexpected(std::move(animation.error()));
  auto text = config.getString("text", *animation);
  if (!text)
    return std::unexpected(std::move(text.error()));
  auto image = config.getString("image", *text);
  if (!image)
    return std::unexpected(std::move(image.error()));
  particle.string = std::move(*image);
  if (particle.type == Type::Textured || particle.type == Type::Animated)
    particle.string = AssetPath::relativeTo(path, particle.string);
  particle.image = particle.string;
  if (particle.type == Type::Animated) {
    auto pathEnd = particle.string.find('?');
    if (!pathEnd)
      return std::unexpected(std::move(pathEnd.error()));
    if (*pathEnd == NPos) {
      particle.directives.clear();
    } else {
      auto directives = particle.string.substr(*pathEnd);
      if (!directives)
        return std::unexpected(std::move(directives.error()));
      particle.directives = std::move(*directives);
    }
    auto initialized = particle.initializeAnimation(root);
    if (!initialized)
      return std::unexpected(std::move(initialized.error()));
  }
  auto hasColor = config.contains("color");
  if (!hasColor)
    return std::unexpected(std::move(hasColor.error()));
  if (*hasColor) {
    auto value = config.get("color");
    if (!value)
      return std::unexpected(std::move(value.error()));
    auto color = jsonToColor(*value);
    if (!color)
      return std::unexpected(std::move(color.error()));
    particle.color = *color;
  }
  auto lightConfig = config.get("light", JsonArray{0, 0, 0, 0});
  if (!lightConfig)
    return std::unexpected(std::move(lightConfig.error()));
  auto light = jsonToColor(*lightConfig);
  if (!light)
    return std::unexpected(std::move(light.error()));
  particle.light = *light;
  auto fade = config.getFloat("fade", 0.0f);
  if (!fade)
    return std::unexpected(std::move(fade.error()));
  particle.fade = *fade;
  auto fullbright = config.getBool("fullbright", false);
  if (!fullbright)
    return std::unexpected(std::move(fullbright.error()));
  particle.fullbright = *fullbright;
  auto positionConfig = config.get("position", JsonArray{0.0f, 0.0f});
  if (!positionConfig)
    return std::unexpected(std::move(positionConfig.error()));
  auto position = jsonToVec2F(*positionConfig);
  if (!position)
    return std::unexpected(std::move(position.error()));
  particle.position = *position;
  auto velocityDefault = config.get("velocity", JsonArray{0.0f, 0.0f});
  if (!velocityDefault)
    return std::unexpected(std::move(velocityDefault.error()));
  auto velocityConfig = config.get("initialVelocity", *velocityDefault);
  if (!velocityConfig)
    return std::unexpected(std::move(velocityConfig.error()));
  auto velocity = jsonToVec2F(*velocityConfig);
  if (!velocity)
    return std::unexpected(std::move(velocity.error()));
  particle.velocity = *velocity;
  particle.finalVelocity = particle.type == Type::Variance ? Vec2F() : particle.velocity;
  auto finalVelocityConfig = config.get("finalVelocity", JsonArray::from(particle.finalVelocity));
  if (!finalVelocityConfig)
    return std::unexpected(std::move(finalVelocityConfig.error()));
  auto finalVelocity = jsonToVec2F(*finalVelocityConfig);
  if (!finalVelocity)
    return std::unexpected(std::move(finalVelocity.error()));
  particle.finalVelocity = *finalVelocity;
  auto approachConfig = config.get("approach", JsonArray{0.0f, 0.0f});
  if (!approachConfig)
    return std::unexpected(std::move(approachConfig.error()));
  auto approach = jsonToVec2F(*approachConfig);
  if (!approach)
    return std::unexpected(std::move(approach.error()));
  particle.approach = *approach;
  auto flip = config.getBool("flip", false);
  if (!flip)
    return std::unexpected(std::move(flip.error()));
  particle.flip = *flip;
  auto flippable = config.getBool("flippable", true);
  if (!flippable)
    return std::unexpected(std::move(flippable.error()));
  particle.flippable = *flippable;
  auto rotation = config.getFloat("rotation", 0.0f);
  if (!rotation)
    return std::unexpected(std::move(rotation.error()));
  particle.rotation = *rotation * std::numbers::pi / 180.0f;
  auto angularVelocity = config.getFloat("angularVelocity", 0.0f);
  if (!angularVelocity)
    return std::unexpected(std::move(angularVelocity.error()));
  particle.angularVelocity = *angularVelocity * std::numbers::pi / 180.0f;
  auto length = config.getFloat("length", 10.0f);
  if (!length)
    return std::unexpected(std::move(length.error()));
  particle.length = *length;
  auto actionName = config.getString("destructionAction", "None");
  if (!actionName)
    return std::unexpected(std::move(actionName.error()));
  auto action = enumValue<DestructionActionNames>(*actionName);
  if (!action)
    return std::unexpected(std::move(action.error()));
  particle.destructionAction = *action;
  auto destructionImage = config.getString("destructionImage", "");
  if (!destructionImage)
    return std::unexpected(std::move(destructionImage.error()));
  if (particle.destructionAction == DestructionAction::Image)
    *destructionImage = AssetPath::relativeTo(path, *destructionImage);
  particle.destructionImage = std::move(*destructionImage);
  auto destructionTime = config.getFloat("destructionTime", 0.0f);
  if (!destructionTime)
    return std::unexpected(std::move(destructionTime.error()));
  particle.destructionTime = *destructionTime;
  auto timeToLive = config.getFloat("timeToLive", 0.0f);
  if (!timeToLive)
    return std::unexpected(std::move(timeToLive.error()));
  particle.timeToLive = *timeToLive;
  auto layerName = config.getString("layer", "middle");
  if (!layerName)
    return std::unexpected(std::move(layerName.error()));
  auto layer = enumValue<LayerNames>(*layerName);
  if (!layer)
    return std::unexpected(std::move(layer.error()));
  particle.layer = *layer;
  auto underwaterOnly = config.getBool("underwaterOnly", false);
  if (!underwaterOnly)
    return std::unexpected(std::move(underwaterOnly.error()));
  particle.underwaterOnly = *underwaterOnly;
  auto collidesForeground = config.getBool("collidesForeground", particle.layer != Layer::Front || particle.underwaterOnly);
  if (!collidesForeground)
    return std::unexpected(std::move(collidesForeground.error()));
  particle.collidesForeground = *collidesForeground;
  auto collidesLiquid = config.getBool("collidesLiquid", particle.type == Type::Ember);
  if (!collidesLiquid)
    return std::unexpected(std::move(collidesLiquid.error()));
  particle.collidesLiquid = *collidesLiquid;
  auto ignoreWind = config.getBool("ignoreWind", true);
  if (!ignoreWind)
    return std::unexpected(std::move(ignoreWind.error()));
  particle.ignoreWind = *ignoreWind;
  auto trail = config.getBool("trail", false);
  if (!trail)
    return std::unexpected(std::move(trail.error()));
  particle.trail = *trail;
  return particle;
}

Particle::Particle(Particle&& other)
  : type(other.type),
    size(other.size),
    baseSize(other.baseSize),
    string(std::move(other.string)),
    image(std::move(other.image)),
    directives(std::move(other.directives)),
    color(std::move(other.color)),
    light(std::move(other.light)),
    fade(other.fade),
    fullbright(other.fullbright),
    position(std::move(other.position)),
    velocity(std::move(other.velocity)),
    finalVelocity(std::move(other.finalVelocity)),
    approach(std::move(other.approach)),
    flippable(other.flippable),
    flip(other.flip),
    rotation(other.rotation),
    angularVelocity(other.angularVelocity),
    length(other.length),
    destructionAction(other.destructionAction),
    destructionImage(std::move(other.destructionImage)),
    destructionTime(other.destructionTime),
    destructionSet(other.destructionSet),
    timeToLive(other.timeToLive),
    layer(other.layer),
    collidesForeground(other.collidesForeground),
    collidesLiquid(other.collidesLiquid),
    underwaterOnly(other.underwaterOnly),
    ignoreWind(other.ignoreWind),
    trail(other.trail),
    animation(std::move(other.animation)),
    m_presentationIdentity(other.m_presentationIdentity) {
  other.animation.reset();
}

Particle& Particle::operator=(Particle&& other) {
  if (this != &other) {
    type = other.type;
    size = other.size;
    baseSize = other.baseSize;
    string = std::move(other.string);
    image = std::move(other.image);
    directives = std::move(other.directives);
    color = std::move(other.color);
    light = std::move(other.light);
    fade = other.fade;
    fullbright = other.fullbright;
    position = std::move(other.position);
    velocity = std::move(other.velocity);
    finalVelocity = std::move(other.finalVelocity);
    approach = std::move(other.approach);
    flippable = other.flippable;
    flip = other.flip;
    rotation = other.rotation;
    angularVelocity = other.angularVelocity;
    length = other.length;
    destructionAction = other.destructionAction;
    destructionImage = std::move(other.destructionImage);
    destructionTime = other.destructionTime;
    destructionSet = other.destructionSet;
    timeToLive = other.timeToLive;
    layer = other.layer;
    collidesForeground = other.collidesForeground;
    collidesLiquid = other.collidesLiquid;
    underwaterOnly = other.underwaterOnly;
    ignoreWind = other.ignoreWind;
    trail = other.trail;
    animation = std::move(other.animation);
    m_presentationIdentity = other.m_presentationIdentity;
    other.animation.reset();
  }
  return *this;
}

std::expected<Json, StarException> Particle::toJson() const {
  auto typeName = enumName<TypeNames>(type);
  if (!typeName)
    return std::unexpected(std::move(typeName.error()));
  auto actionName = enumName<DestructionActionNames>(destructionAction);
  if (!actionName)
    return std::unexpected(std::move(actionName.error()));
  auto layerName = enumName<LayerNames>(layer);
  if (!layerName)
    return std::unexpected(std::move(layerName.error()));
  return JsonObject{{"type", *typeName},
                    {"size", size},
                    {"string", string},
                    {"color", jsonFromColor(color)},
                    {"light", jsonFromColor(light)},
                    {"fade", fade},
                    {"fullbright", fullbright},
                    {"position", jsonFromVec2F(position)},
                    {"velocity", jsonFromVec2F(velocity)},
                    {"finalVelocity", jsonFromVec2F(finalVelocity)},
                    {"approach", jsonFromVec2F(approach)},
                    {"flip", flip},
                    {"flippable", flippable},
                    {"rotation", rotation * 180.0f / std::numbers::pi},
                    {"angularVelocity", angularVelocity * 180.0f / std::numbers::pi},
                    {"length", length},
                    {"destructionAction", *actionName},
                    {"destructionImage", AssetPath::join(destructionImage)},
                    {"destructionTime", destructionTime},
                    {"timeToLive", timeToLive},
                    {"layer", *layerName},
                    {"collidesForeground", collidesForeground},
                    {"collidesLiquid", collidesLiquid},
                    {"underwaterOnly", underwaterOnly},
                    {"ignoreWind", ignoreWind},
                    {"trail", trail}};
}

void Particle::translate(Vec2F const& pos) {
  position += pos;
}

std::expected<void, StarException> Particle::update(Root& root, float dt, Vec2F const& wind) {
  auto prevVelocity = velocity;
  if (ignoreWind) {
    velocity[0] = Star::approach<float>(finalVelocity[0], velocity[0], approach[0] * dt);
    velocity[1] = Star::approach<float>(finalVelocity[1], velocity[1], approach[1] * dt);
  } else {
    velocity[0] = Star::approach<float>(finalVelocity[0] + wind[0], velocity[0], approach[0] * dt);
    velocity[1] = Star::approach<float>(finalVelocity[1] + wind[1], velocity[1], approach[1] * dt);
  }
  position += (prevVelocity + velocity) * 0.5f * dt;

  rotation += angularVelocity * dt;

  if (light != Color::Clear)
    light.fade(fade * dt);

  timeToLive -= dt;

  if (timeToLive < 0.0f)
    destructionUpdate();

  if (type == Type::Animated) {
    auto initialized = initializeAnimation(root);
    if (!initialized)
      return std::unexpected(std::move(initialized.error()));
    animation->update(dt);
  }
  return {};
}

bool Particle::dead() const {
  return timeToLive < -destructionTime;
}

Particle::PresentationState Particle::presentationState() const {
  return {position, velocity, rotation, size, color, light, timeToLive, type, destructionSet,
      animation ? std::optional(animation->playbackState()) : std::nullopt};
}

void Particle::samplePresentation(Particle const& current, PresentationState const& previous,
    WorldGeometry const& geometry, float interpolation, float timestep, std::uint64_t identity) {
  float alpha = std::clamp(interpolation, 0.0f, 1.0f);
  bool sameState = previous.type == current.type && previous.destructionSet == current.destructionSet;
  if (m_presentationIdentity != identity || type != current.type || destructionSet != current.destructionSet) {
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
    m_presentationIdentity = identity;
    destructionSet = current.destructionSet;
  }

  position = previous.position + geometry.diff(current.position, previous.position) * alpha;
  velocity = previous.velocity + (current.velocity - previous.velocity) * alpha;
  // Rotation is deliberately unwrapped: a particle can turn more than half a
  // revolution in one tick, and its angular velocity defines that motion.
  rotation = sameState
      ? previous.rotation + (current.rotation - previous.rotation) * alpha : current.rotation;
  size = sameState
      ? previous.size + (current.size - previous.size) * alpha : current.size;
  color = sameState ? previous.color.mix(current.color, alpha) : current.color;
  light = previous.light.mix(current.light, alpha);
  timeToLive = previous.timeToLive + (current.timeToLive - previous.timeToLive) * alpha;

  // Destruction begins at the sampled lifetime boundary, not at the beginning
  // of the tick that crosses it. Image changes remain discrete tick events.
  if (sameState && current.destructionTime > 0) {
    float factor = (timeToLive + current.destructionTime) / current.destructionTime;
    if (current.destructionAction == DestructionAction::Shrink)
      size = timeToLive < 0 ? current.baseSize * factor : previous.size;
    else if (current.destructionAction == DestructionAction::Fade) {
      color = previous.color;
      if (timeToLive < 0)
        color.setAlphaF(factor);
    }
  }
  if (animation && previous.animation && sameState)
    animation->samplePlayback(*previous.animation, timestep * alpha);
}

void Particle::applyVariance(Root& root, Particle const& variance) {
  size += variance.size * (root.random().randf() * 2.0f - 1.0f);
  position += {variance.position[0] * (root.random().randf() * 2.0f - 1.0f), variance.position[1] * (root.random().randf() * 2.0f - 1.0f)};
  velocity += {variance.velocity[0] * (root.random().randf() * 2.0f - 1.0f), variance.velocity[1] * (root.random().randf() * 2.0f - 1.0f)};
  finalVelocity +=
      {variance.finalVelocity[0] * (root.random().randf() * 2.0f - 1.0f), variance.finalVelocity[1] * (root.random().randf() * 2.0f - 1.0f)};
  rotation += variance.rotation * (root.random().randf() * 2.0f - 1.0f);
  angularVelocity += variance.angularVelocity * (root.random().randf() * 2.0f - 1.0f);
  length += variance.length * (root.random().randf() * 2.0f - 1.0f);
  timeToLive += variance.timeToLive * (root.random().randf() * 2.0f - 1.0f);
}

void Particle::collide(Vec2F const& collisionPosition) {
  position = collisionPosition;
  approach = Vec2F();
  velocity = finalVelocity = Vec2F();
  destroy(true);
}

void Particle::destroy(bool withDestruction) {
  if (withDestruction) {
    if (timeToLive >= 0.0f) {
      timeToLive = 0.0f;
      destructionUpdate();
    }
  } else {
    timeToLive = -destructionTime - 1.0f;
  }
}

void Particle::destructionUpdate() {
  if (destructionTime > 0) {
    float destructionFactor = (timeToLive + destructionTime) / destructionTime;
    if (destructionAction == DestructionAction::Shrink) {
      size = baseSize * destructionFactor;
    } else if (destructionAction == DestructionAction::Fade) {
      color.setAlphaF(destructionFactor);
    } else if (destructionAction == DestructionAction::Image) {
      if (!destructionSet) {
        size = 1.0f;
        color = Color::White;
        type = Particle::Type::Textured;
        image = destructionImage;
        angularVelocity = 0.0f;
        length = 0.0f;
        rotation = 0.0f;
        destructionSet = true;
      }
    }
  }
}

std::expected<void, StarException> Particle::initializeAnimation(Root& root) {
  if (!animation) {
    auto created = Animation::create(root, AssetPath::removeDirectives(string));
    if (!created)
      return std::unexpected(std::move(created.error()));
    animation = std::move(*created);
    animation->setProcessing(directives);
  }
  return {};
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, Particle const& particle) {
  auto type = ds.viwrite(particle.type);
  if (!type)
    return std::unexpected(std::move(type.error()));
  auto fields = ds << particle.size << particle.string << particle.color << particle.light << particle.fade
      << particle.position << particle.velocity << particle.finalVelocity << particle.approach << particle.rotation
      << particle.flippable << particle.flip << particle.angularVelocity << particle.length;
  if (!fields)
    return std::unexpected(std::move(fields.error()));
  auto action = ds.viwrite(particle.destructionAction);
  if (!action)
    return std::unexpected(std::move(action.error()));
  return ds << particle.destructionImage << particle.destructionTime << particle.timeToLive << particle.layer
      << particle.collidesForeground << particle.collidesLiquid << particle.underwaterOnly << particle.ignoreWind;
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, Particle& particle) {
  auto type = ds.viread(particle.type);
  if (!type)
    return std::unexpected(std::move(type.error()));
  auto fields = ds >> particle.size >> particle.string >> particle.color >> particle.light >> particle.fade
      >> particle.position >> particle.velocity >> particle.finalVelocity >> particle.approach >> particle.rotation
      >> particle.flippable >> particle.flip >> particle.angularVelocity >> particle.length;
  if (!fields)
    return std::unexpected(std::move(fields.error()));
  auto action = ds.viread(particle.destructionAction);
  if (!action)
    return std::unexpected(std::move(action.error()));
  return ds >> particle.destructionImage >> particle.destructionTime >> particle.timeToLive >> particle.layer
      >> particle.collidesForeground >> particle.collidesLiquid >> particle.underwaterOnly >> particle.ignoreWind;
}

ParticleVariantCreator makeParticleVariantCreator(Root& root, Particle particle, Particle variance) {
  return [&root, particle, variance]() -> Particle {
    auto p = particle;
    p.applyVariance(root, variance);
    return p;
  };
}

}

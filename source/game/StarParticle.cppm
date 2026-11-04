module;
import star.format;
import std;
import star.string;
import star.json;

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
import star.animation;
import star.world_geometry;

import star.vector;
namespace Star {

class Root;

struct Particle {
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, Particle const& particle);
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, Particle& particle);

  enum class Type {
    // Variance is basically a null type, used only for varying other particles
    // by amounts.
    Variance,

    Ember,
    Textured,
    Animated,
    Streak,
    Text
  };
  static std::expected<EnumMap<Type>, StarException> const TypeNames;

  enum class DestructionAction {
    None,
    Image,
    Fade,
    Shrink
  };
  static std::expected<EnumMap<DestructionAction>, StarException> const DestructionActionNames;

  enum class Layer {
    Back,
    Middle,
    Front
  };
  static std::expected<EnumMap<Layer>, StarException> const LayerNames;

  Particle();
  // If particle is type Textured, then the image name is considered relative
  // to the given asset path
  [[nodiscard]] static std::expected<Particle, StarException> create(Root& root, Json const& config, String const& assetsPath = "/");
  Particle(Particle const&) = default;
  Particle& operator=(Particle const&) = default;
  Particle(Particle&& other);
  Particle& operator=(Particle&& other);

  [[nodiscard]] std::expected<Json, StarException> toJson() const;

  void translate(Vec2F const& pos);

  // Updates position, velocity, rotation, and timeToLive.
  [[nodiscard]] std::expected<void, StarException> update(Root& root, float dt, Vec2F const& wind = Vec2F());

  bool dead() const;

  struct PresentationState {
    Vec2F position;
    Vec2F velocity;
    float rotation;
    float size;
    Color color;
    Color light;
    float timeToLive;
    Type type;
    bool destructionSet;
    std::optional<Animation::PlaybackState> animation;
  };

  PresentationState presentationState() const;
  // Writes only display state. Payload is copied once per particle identity,
  // not once per display frame; simulation and animation events are untouched.
  void samplePresentation(Particle const& current, PresentationState const& previous,
      WorldGeometry const& geometry, float interpolation, float timestep, std::uint64_t identity);

  // Apply random variance to this particle based on a "variance" particle that
  // contains the maximum amount of variance for each field.
  void applyVariance(Root& root, Particle const& variance);

  // Stops particle and sets time to live to 0.0 (triggering destruction)
  void collide(Vec2F const& collisionPosition);
  // Immediately triggers destruction of particle with / without destruction
  // action
  void destroy(bool withDestruction);

  // Internally called by update() / collide() / destruct()
  void destructionUpdate();

  [[nodiscard]] std::expected<void, StarException> initializeAnimation(Root& root);

  Type type;

  // Defaults to 1.0, 1.0 will produce a reasonable size particle for whatever
  // the type is.
  float size;
  float baseSize; // track the original size for shrink destruction action

  // Used differently depending on the type of the particle.
  String string;
  AssetPath image;
  DirectivesGroup directives;

  Color color;
  Color light;
  float fade;
  bool fullbright = false;

  Vec2F position;
  Vec2F velocity;
  Vec2F finalVelocity;
  Vec2F approach;

  bool flippable = true;
  bool flip = false;

  float rotation;
  float angularVelocity;

  float length;

  DestructionAction destructionAction = DestructionAction::None;
  AssetPath destructionImage;
  float destructionTime;
  bool destructionSet = false;

  float timeToLive;
  Layer layer = Layer::Middle;

  bool collidesForeground = true;
  bool collidesLiquid = true;
  bool underwaterOnly = false;

  bool ignoreWind = true;

  bool trail = false;

  std::optional<Animation> animation;
  private:
    std::uint64_t m_presentationIdentity = 0;
};

[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, Particle const& particle);
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, Particle& particle);

typedef std::function<Particle()> ParticleVariantCreator;
ParticleVariantCreator makeParticleVariantCreator(Root& root, Particle particle, Particle variance);

}

export module star.particle;

export namespace Star {
  using ::Star::Particle;
  using ::Star::ParticleVariantCreator;
  using ::Star::makeParticleVariantCreator;
}

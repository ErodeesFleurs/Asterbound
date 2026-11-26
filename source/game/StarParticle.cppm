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
class ParticleManager;

// Memberwise movement is separate from clearing the source's cached state.
struct ParticleFields {
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
  inline static constexpr auto type_names = sorted_enum_names(std::to_array<std::pair<Type, std::string_view>>(
    {{Type::Variance, "variance"}, {Type::Ember, "ember"}, {Type::Textured, "textured"},
     {Type::Animated, "animated"}, {Type::Streak, "streak"}, {Type::Text, "text"}}));

  enum class DestructionAction {
    None,
    Image,
    Fade,
    Shrink
  };
  inline static constexpr auto destruction_action_names = sorted_enum_names(
    std::to_array<std::pair<DestructionAction, std::string_view>>({{DestructionAction::None, "none"},
                                                                   {DestructionAction::Image, "image"},
                                                                   {DestructionAction::Fade, "fade"},
                                                                   {DestructionAction::Shrink, "shrink"}}));

  enum class Layer {
    Back,
    Middle,
    Front
  };
  inline static constexpr auto layer_names = sorted_enum_names(std::to_array<std::pair<Layer, std::string_view>>(
    {{Layer::Back, "back"}, {Layer::Middle, "middle"}, {Layer::Front, "front"}}));

  Type type = Type::Variance;
  float size = 0.0f;
  float base_size = 0.0f; // original size for shrink destruction

  // Used differently depending on the type of the particle.
  String string;
  AssetPath image;
  DirectivesGroup directives;

  Color color = Color::palette_white;
  Color light = Color::palette_clear;
  float fade = 0.0f;
  bool fullbright = false;

  Vec2F position;
  Vec2F velocity;
  Vec2F final_velocity;
  Vec2F approach;

  bool flippable = true;
  bool flip = false;
  float rotation = 0.0f;
  float angular_velocity = 0.0f;
  float length = 0.0f;

  DestructionAction destruction_action = DestructionAction::None;
  AssetPath destruction_image;
  float destruction_time = 0.0f;
  bool destruction_set = false;

  float time_to_live = 0.0f;
  Layer layer = Layer::Middle;
  bool collides_foreground = true;
  bool collides_liquid = true;
  bool underwater_only = false;
  bool ignore_wind = true;
  bool trail = false;

  std::optional<Animation> animation;
};

struct Particle : ParticleFields {
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, Particle const& particle);
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, Particle& particle);

  Particle() = default;
  // Textured and animated asset names are relative to the source path.
  [[nodiscard]] static std::expected<Particle, StarException> create(Root& root, Json const& config, String const& assets_path = "/");
  Particle(Particle const&) = default;
  Particle& operator=(Particle const&) = default;
  Particle(Particle&& other) noexcept(std::is_nothrow_move_constructible_v<ParticleFields>);
  Particle& operator=(Particle&& other) noexcept(std::is_nothrow_move_assignable_v<ParticleFields>);

  [[nodiscard]] std::expected<Json, StarException> to_json() const;

  void translate(Vec2F const& pos);

  // Advances the simulation only after any needed animation is available.
  [[nodiscard]] std::expected<void, StarException> update(Root& root, float dt, Vec2F const& wind = Vec2F());

  bool dead() const;

  struct PresentationState {
    Vec2F position;
    Vec2F velocity;
    float rotation;
    float size;
    Color color;
    Color light;
    float time_to_live;
    Type type;
    bool destruction_set;
    std::optional<Animation::PlaybackState> animation;
  };

  PresentationState presentation_state() const;
  // Writes only display state. Payload is copied once per particle identity,
  // not once per display frame; simulation and animation events are untouched.
  void sample_presentation(Particle const& current, PresentationState const& previous,
      WorldGeometry const& geometry, float interpolation, float timestep, std::uint64_t identity);

  // Apply random variance to this particle based on a "variance" particle that
  // contains the maximum amount of variance for each field.
  void apply_variance(Root& root, Particle const& variance);

  // Stops particle and sets time to live to 0.0 (triggering destruction)
  void collide(Vec2F const& collision_position);
  // Immediately triggers destruction of particle with / without destruction
  // action
  void destroy(bool with_destruction);

private:
  friend class ParticleManager;

  struct Motion {
    Vec2F velocity;
    Vec2F position;
    float time_to_live;
  };
  using PreparedAnimation = Animation::Preparation;

  Motion next_motion(float dt, Vec2F const& wind) const;
  void advance(float dt, Motion const& motion);
  bool will_use_destruction_image(float lifetime, bool foreground_collision) const;
  [[nodiscard]] std::expected<PreparedAnimation, StarException> prepare_animation(Root& root) const;
  void activate_animation(PreparedAnimation&& prepared);
  [[nodiscard]] std::expected<void, StarException> initialize_animation(Root& root);

  void destruction_update();
  void rebuild_payload();
  std::uint64_t presentation_identity = 0;
};

[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, Particle const& particle);
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, Particle& particle);

using ParticleVariantCreator = std::function<Particle()>;
ParticleVariantCreator make_particle_variant_creator(Root& root, Particle particle, Particle variance);

}

export module star.particle;

export namespace Star {
  using ::Star::Particle;
  using ::Star::ParticleVariantCreator;
  using ::Star::make_particle_variant_creator;
}

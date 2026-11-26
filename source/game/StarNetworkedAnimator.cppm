module;
import std;
import star.format;
import star.json;
import star.random;
import star.periodic_function;
import star.ordered_map;
import star.matrix3;
import star.string;
import star.data_stream;
import star.poly;
import star.color;
import star.list;
import star.rect;
import star.image_processing;
import star.hash;
import star.string_view;
import star.thread;
import star.directives;
import star.asset_path;
import star.bi_map;
import star.vector;
import star.audio;
import star.map;
import star.set;

import star.animated_part_set;
import star.drawable;
import star.animation;
import star.particle;
import star.mixer;
import star.light_source;

import star.net_element_basic_fields;
import star.net_element_float_fields;
import star.net_element_containers;
import star.net_element_signal;
import star.net_element_sync_group;

namespace Star {

class Root;

struct NetworkedAnimatorExceptionTag {
  static constexpr std::string_view name() { return "NetworkedAnimatorException"; }
};
using NetworkedAnimatorException = StarError<NetworkedAnimatorExceptionTag, StarException>;

template <typename Pose, typename Sample>
concept animator_pose = requires(Pose& pose, Sample& staged) {
  { pose(staged) } -> std::same_as<std::expected<void, StarException>>;
};

// Wraps an AnimatedPartSet with a set of optional light sources and particle
// emitters to produce a network capable animation system.
class NetworkedAnimator : public NetElementSyncGroup {
  struct Presentation;
  struct ConstructionKey { explicit ConstructionKey() = default; };

public:
  // Target for dynamic render data such as sounds and particles that are not
  // persistent and are instead produced during a call to update, and may need
  // to be tracked over time.
  class DynamicTarget {
  public:
    // Calls stop_audio().
    ~DynamicTarget();

    List<std::shared_ptr<AudioInstance>> pull_new_audios();
    List<Particle> pull_new_particles();

    // Stops all looping audio immediately and lets non-looping audio finish
    // normally
    void stop_audio();

    // Updates the base position of all un-pulled particles and all active
    // audio.  Not necessary to call, but if not called all pulled data will be
    // relative to (0, 0).
    void update_position(Vec2F const& position);

  private:
    friend class NetworkedAnimator;

    void clear_finished_audio();

    struct PersistentSound {
      Json sound;
      std::shared_ptr<AudioInstance> audio;
      float stop_ramp_time;
    };

    Vec2F position;
    bool positioned_since_audio_pull = false;
    List<std::shared_ptr<AudioInstance>> pending_audios;
    List<Particle> pending_particles;
    StringMap<PersistentSound> state_persistent_sounds;
    StringMap<Json> state_immediate_sounds;
    StringMap<List<std::shared_ptr<AudioInstance>>> independent_sounds;
    HashMap<std::shared_ptr<AudioInstance>, Vec2F> current_audio_base_positions;
  };

  NetworkedAnimator(ConstructionKey, Root& root);
  [[nodiscard]] static std::expected<std::shared_ptr<NetworkedAnimator>, StarException> create(Root& root);
  // If passed a string as config, NetworkedAnimator will interpret this as a
  // config path, otherwise it is interpreted as the literal config.
  [[nodiscard]] static std::expected<std::shared_ptr<NetworkedAnimator>, StarException> create(Root& root, Json config, String relative_path = String());

  [[nodiscard]] static std::expected<std::shared_ptr<NetworkedAnimator>, StarException> create(NetworkedAnimator const& animator);
  NetworkedAnimator(NetworkedAnimator&&) = delete;
  NetworkedAnimator(NetworkedAnimator const&) = delete;
  NetworkedAnimator& operator=(NetworkedAnimator&&) = delete;
  NetworkedAnimator& operator=(NetworkedAnimator const&) = delete;


  // Borrows the animator and a reusable scratch slot; copies share the same slot.
  // The animator must outlive every sample. Use samples only within one logical
  // update/configuration interval; animator queries themselves remain logical.
  class Sample {
  public:
    Sample(NetworkedAnimator const& animator, float interpolation);
    Sample(Sample const& sample);
    ~Sample();
    Sample& operator=(Sample const&) = delete;
    float interpolation() const { return m_interpolation; }
    [[nodiscard]] std::expected<String, StarException> state(String const& state_type) const;
    [[nodiscard]] std::expected<int, StarException> state_frame(String const& state_type) const;
    [[nodiscard]] std::expected<int, StarException> state_next_frame(String const& state_type) const;
    [[nodiscard]] std::expected<float, StarException> state_frame_progress(String const& state_type) const;
    [[nodiscard]] std::expected<float, StarException> state_timer(String const& state_type) const;
    [[nodiscard]] std::expected<bool, StarException> state_reverse(String const& state_type) const;
    [[nodiscard]] std::expected<float, StarException> state_cycle(String const& state_type, std::optional<String> state) const;
    [[nodiscard]] std::expected<int, StarException> state_frames(String const& state_type, std::optional<String> state) const;
    [[nodiscard]] std::expected<Json, StarException> state_property(String const& state_type, String const& property_name, std::optional<String> state = {}, std::optional<int> frame = {}) const;
    [[nodiscard]] std::expected<Json, StarException> state_next_property(String const& state_type, String const& property_name) const;
    [[nodiscard]] std::expected<Json, StarException> part_property(String const& part_name, String const& property_name, std::optional<String> state_type = {}, std::optional<String> state = {}, std::optional<int> frame = {}) const;
    [[nodiscard]] std::expected<Json, StarException> part_next_property(String const & part_name, String const & property_name) const;
    [[nodiscard]] std::expected<Mat3F, StarException> group_transformation(std::span<String const> groups) const;
    [[nodiscard]] std::expected<Mat3F, StarException> part_transformation(String const& part_name) const;
    [[nodiscard]] std::expected<Mat3F, StarException> final_part_transformation(String const& part_name) const;
    [[nodiscard]] std::expected<std::optional<Vec2F>, StarException> part_point(String const& part_name, String const& property_name) const;
    [[nodiscard]] std::expected<std::optional<PolyF>, StarException> part_poly(String const& part_name, String const& property_name) const;
    [[nodiscard]] std::expected<String, StarException> apply_part_tags(String const& part_name, String apply) const;
    [[nodiscard]] std::expected<float, StarException> current_rotation_angle(String const& rotation_group) const;
    [[nodiscard]] std::expected<List<Drawable>, StarException> drawables(Vec2F const& translate = Vec2F()) const;
    [[nodiscard]] std::expected<List<std::pair<Drawable, float>>, StarException> drawables_with_z_level(Vec2F const& translate = Vec2F()) const;
    [[nodiscard]] std::expected<List<LightSource>, StarException> light_sources(Vec2F const& translate = Vec2F()) const;
    // Appends in animator order, leaving existing entries intact on query failure.
    [[nodiscard]] std::expected<void, StarException> append_light_sources(List<LightSource>& lights, Vec2F const& translate = Vec2F()) const;
    [[nodiscard]] std::expected<bool, StarException> has_state(String const& type, std::optional<String> const& state = {}) const;
    [[nodiscard]] std::expected<bool, StarException> set_local_state(String const& type, String const& state, bool start_new = false, bool reverse = false);
    void set_local_tag(String name, std::optional<String> value = {});
    // A provided list is borrowed until that override is replaced or the last sample copy dies.
    // No list explicitly suppresses the logical attachment for this part.
    void set_part_drawables(String const& name, std::optional<std::reference_wrapper<List<Drawable> const>> drawables);
    [[nodiscard]] std::expected<void, StarException> reset_local_transformation_group(String const& name);
    [[nodiscard]] std::expected<void, StarException> rotate_local_transformation_group(String const& name, float rotation, Vec2F const& center = Vec2F());
    [[nodiscard]] std::expected<void, StarException> translate_local_transformation_group(String const& name, Vec2F const& translation);
    [[nodiscard]] std::expected<Mat3F, StarException> get_local_transformation_group(String const& name) const;
  private:
    friend class NetworkedAnimator;
    std::reference_wrapper<Presentation> m_presentation;
    NetworkedAnimator const& m_animator;
    float m_interpolation;
  };
  [[nodiscard]] Sample sample(float interpolation) const;
  StringList state_types() const;
  [[nodiscard]] std::expected<StringList, StarException> states(String const& state_type) const;

  // Returns whether a state change occurred.  If start_new is true, always
  // forces a state change and starts the state off at the beginning even if
  // this state is already the current state.
  [[nodiscard]] std::expected<bool, StarException> set_state(String const& state_type, String const& state, bool start_new = false, bool reverse = false);
  [[nodiscard]] std::expected<bool, StarException> set_local_state(String const& state_type, String const& state, bool start_new = false, bool reverse = false);
  [[nodiscard]] std::expected<void, StarException> validate_local_state(String const& type, String const& state) const;
  [[nodiscard]] std::expected<String, StarException> state(String const& state_type) const;
  [[nodiscard]] std::expected<int, StarException> state_frame(String const& state_type) const;
  [[nodiscard]] std::expected<int, StarException> state_next_frame(String const& state_type) const;
  [[nodiscard]] std::expected<float, StarException> state_frame_progress(String const& state_type) const;
  [[nodiscard]] std::expected<float, StarException> state_timer(String const& state_type) const;
  [[nodiscard]] std::expected<bool, StarException> state_reverse(String const& state_type) const;

  [[nodiscard]] std::expected<float, StarException> state_cycle(String const& state_type, std::optional<String> state) const;
  [[nodiscard]] std::expected<int, StarException> state_frames(String const& state_type, std::optional<String> state) const;

  [[nodiscard]] std::expected<bool, StarException> has_state(String const& state_type, std::optional<String> const& state = {}) const;

  StringMap<AnimatedPartSet::Part> const& const_parts() const;
  StringMap<AnimatedPartSet::Part>& parts();
  StringList part_names() const;

  // Queries, if it exists, a property value from the underlying
  // AnimatedPartSet for the given state or part.  If the property does not
  // exist, returns null.
  [[nodiscard]] std::expected<Json, StarException> state_property(String const& state_type, String const& property_name, std::optional<String> state = {}, std::optional<int> frame = {}) const;
  [[nodiscard]] std::expected<Json, StarException> state_next_property(String const& state_type, String const& property_name) const;
  [[nodiscard]] std::expected<Json, StarException> part_property(String const& part_name, String const& property_name, std::optional<String> state_type = {}, std::optional<String> state = {}, std::optional<int> frame = {}) const;
  [[nodiscard]] std::expected<Json, StarException> part_next_property(String const & part_name, String const & property_name) const;

  // Returns the transformation from flipping and zooming that is applied to
  // all parts in the NetworkedAnimator.
  Mat3F global_transformation() const;
  // The transformation applied from the given set of transformation groups
  [[nodiscard]] std::expected<Mat3F, StarException> group_transformation(std::span<String const> groups) const;
  // The transformation that is applied to the given part NOT including the
  // global transformation
  [[nodiscard]] std::expected<Mat3F, StarException> part_transformation(String const& part_name) const;
  // Returns the total transformation for the given part, which includes the
  // global_transformation, as well as the part rotation, scaling, and
  // translation.
  [[nodiscard]] std::expected<Mat3F, StarException> final_part_transformation(String const& part_name) const;

  // part_point / part_poly takes a property_name and looks up the associated part
  // property and interprets is a Vec2F or a PolyF, then applies the final part
  // transformation and returns it.
  [[nodiscard]] std::expected<std::optional<Vec2F>, StarException> part_point(String const& part_name, String const& property_name) const;
  [[nodiscard]] std::expected<std::optional<PolyF>, StarException> part_poly(String const& part_name, String const& property_name) const;

  // Every part image can have one or more <tag> directives in it, which if set
  // here will be replaced by the tag value when constructing Drawables.  All
  // Drawables can also have a <frame> tag which will be set to whatever the
  // current state frame is (1 indexed, so the first frame is 1).
  void set_global_tag(String tag_name, std::optional<String> tag_value = {});
  void remove_global_tag(String const& tag_name);
  std::optional<std::reference_wrapper<String const>> global_tag(String const& tag_name) const;
  void set_part_tag(String const& part_type, String tag_name, std::optional<String> tag_value = {});
  void set_local_tag(String tag_name, std::optional<String> tag_value = {});

  void set_part_drawables(String const& part_name, List<Drawable> drawables);
  void add_part_drawables(String const& part_name, List<Drawable> drawables);

  [[nodiscard]] std::expected<String, StarException> apply_part_tags(String const& part_name, String apply) const;

  void set_processing_directives(Directives const& directives);
  void set_zoom(float zoom);
  bool flipped() const;
  float flipped_relative_center_line() const;
  void set_flipped(bool flipped, float relative_center_line = 0.0f);

  // Animation rate defaults to 1.0, which means normal animation speed.  This
  // can be used to globally speed up or slow down all components of
  // NetworkedAnimator together.
  void set_animation_rate(float rate);
  float animation_rate();

  // Given angle is an absolute angle.  Will rotate over time at the configured
  // angular velocity unless the immediate flag is set.
  bool has_rotation_group(String const& rotation_group) const;
  [[nodiscard]] std::expected<void, StarException> rotate_group(String const& rotation_group, float target_angle, bool immediate = false);
  [[nodiscard]] std::expected<float, StarException> current_rotation_angle(String const& rotation_group) const;

  // Transformation groups can be used for arbitrary part transforamtions.
  // They apply immediately, and are optionally interpolated on slaves.
  bool has_transformation_group(String const& transformation_group) const;
  [[nodiscard]] std::expected<void, StarException> translate_transformation_group(String const& transformation_group, Vec2F const& translation);
  [[nodiscard]] std::expected<void, StarException> rotate_transformation_group(String const& transformation_group, float rotation, Vec2F const& rotation_center = Vec2F());
  [[nodiscard]] std::expected<void, StarException> scale_transformation_group(String const& transformation_group, float scale, Vec2F const& scale_center = Vec2F());
  [[nodiscard]] std::expected<void, StarException> scale_transformation_group(String const& transformation_group, Vec2F const& scale, Vec2F const& scale_center = Vec2F());
  [[nodiscard]] std::expected<void, StarException> transform_transformation_group(String const& transformation_group, float a, float b, float c, float d, float tx, float ty);
  [[nodiscard]] std::expected<void, StarException> reset_transformation_group(String const& transformation_group);
  [[nodiscard]] std::expected<void, StarException> set_transformation_group(String const& transformation_group, Mat3F transform);
  [[nodiscard]] std::expected<Mat3F, StarException> get_transformation_group(String const& transformation_group);

  [[nodiscard]] std::expected<void, StarException> translate_local_transformation_group(String const& transformation_group, Vec2F const& translation);
  [[nodiscard]] std::expected<void, StarException> rotate_local_transformation_group(String const& transformation_group, float rotation, Vec2F const& rotation_center = Vec2F());
  [[nodiscard]] std::expected<void, StarException> scale_local_transformation_group(String const& transformation_group, float scale, Vec2F const& scale_center = Vec2F());
  [[nodiscard]] std::expected<void, StarException> scale_local_transformation_group(String const& transformation_group, Vec2F const& scale, Vec2F const& scale_center = Vec2F());
  [[nodiscard]] std::expected<void, StarException> transform_local_transformation_group(String const& transformation_group, float a, float b, float c, float d, float tx, float ty);
  [[nodiscard]] std::expected<void, StarException> reset_local_transformation_group(String const& transformation_group);
  [[nodiscard]] std::expected<void, StarException> set_local_transformation_group(String const& transformation_group, Mat3F transform);
  [[nodiscard]] std::expected<Mat3F, StarException> get_local_transformation_group(String const& transformation_group);

  bool has_particle_emitter(String const& emitter_name) const;
  // Active particle emitters emit over time based on emission rate/variance.
  [[nodiscard]] std::expected<void, StarException> set_particle_emitter_active(String const& emitter_name, bool active);
  // Set the emission rate in particles / sec for a given emitter
  [[nodiscard]] std::expected<void, StarException> set_particle_emitter_emission_rate(String const& emitter_name, float emission_rate);
  // Set the optional particle emitter offset region, which particles will be
  // spread around randomly before being spawned
  [[nodiscard]] std::expected<void, StarException> set_particle_emitter_offset_region(String const& emitter_name, RectF const& offset_region);

  // Number of times to cycle when emitting a burst of particles.
  [[nodiscard]] std::expected<void, StarException> set_particle_emitter_burst_count(String const& emitter_name, unsigned burst_count);

  // Cause one time burst of all types of particles in an emitter looping around
  // burst_count times
  [[nodiscard]] std::expected<void, StarException> burst_particle_emitter(String const& emitter_name);

  bool has_light(String const& light_name) const;
  [[nodiscard]] std::expected<void, StarException> set_light_active(String const& light_name, bool active);
  [[nodiscard]] std::expected<void, StarException> set_light_position(String const& light_name, Vec2F position);
  [[nodiscard]] std::expected<void, StarException> set_light_color(String const& light_name, Color color);
  [[nodiscard]] std::expected<void, StarException> set_light_point_angle(String const& light_name, float angle);

  bool has_sound(String const& sound_name) const;
  [[nodiscard]] std::expected<void, StarException> set_sound_pool(String const& sound_name, StringList sound_pool);
  // Plays a sound from the given independent sound pool.  Multiple sounds may
  // be played as part of this group, and playing a new one will not interrupt
  // an older one.
  [[nodiscard]] std::expected<void, StarException> play_sound(String const& sound_name, int loops = 0);

  // Setting the sound position, volume, and speed will affect future sounds in
  // this group, as well as any still active sounds from this group.
  [[nodiscard]] std::expected<void, StarException> set_sound_position(String const& sound_name, Vec2F const& position);

  [[nodiscard]] std::expected<void, StarException> set_sound_volume(String const& sound_name, float volume, float ramp_time = 0.0f);
  [[nodiscard]] std::expected<void, StarException> set_sound_pitch_multiplier(String const& sound_name, float pitch_multiplier, float ramp_time = 0.0f);

  // Stop all sounds played from this sound group
  [[nodiscard]] std::expected<void, StarException> stop_all_sounds(String const& sound_name, float ramp_time = 0.0f);

  [[nodiscard]] std::expected<void, StarException> set_effect_enabled(String const& effect, bool enabled);

  [[nodiscard]] std::expected<List<Drawable>, StarException> drawables(Vec2F const& translate = Vec2F()) const;
  [[nodiscard]] std::expected<List<std::pair<Drawable, float>>, StarException> drawables_with_z_level(Vec2F const& translate = Vec2F()) const;

  [[nodiscard]] std::expected<List<LightSource>, StarException> light_sources(Vec2F const& translate = Vec2F()) const;
  // Appends in animator order, leaving existing entries intact on query failure.
  [[nodiscard]] std::expected<void, StarException> append_light_sources(List<LightSource>& lights, Vec2F const& translate = Vec2F()) const;

  // Dynamic target is optional, if not given, generated particles and sounds
  // will be discarded
  [[nodiscard]] std::expected<void, StarException> update(float dt, std::optional<std::reference_wrapper<DynamicTarget>> dynamic_target);

  // Run through the current animations until the final frame, including any
  // transition animations.
  [[nodiscard]] std::expected<void, StarException> finish_animations();
  template <animator_pose<Sample> Pose>
  [[nodiscard]] std::expected<void, StarException> reset_local_animation(Pose&& pose) {
    auto staged = sample(1.f);
    if (auto finished = m_animated_parts.stage_finished_states(staged.m_presentation.get().parts); !finished)
      return finished;
    if (auto applied = pose(staged); !applied)
      return applied;
    commit_local_reset(staged);
    return {};
  }
  std::uint8_t version() const;

private:
  using SampleView = std::optional<std::reference_wrapper<Sample const>>;
  struct AnchorPath {
    String const& name;
    std::optional<std::reference_wrapper<AnchorPath const>> parent;
  };

  Presentation& acquire_presentation() const;
  void commit_local_reset(Sample const& staged);
  [[nodiscard]] std::expected<String, StarException> query_state(String const& state_type, SampleView sample) const;
  [[nodiscard]] std::expected<int, StarException> query_state_frame(String const& state_type, SampleView sample) const;
  [[nodiscard]] std::expected<int, StarException> query_state_next_frame(String const& state_type, SampleView sample) const;
  [[nodiscard]] std::expected<float, StarException> query_state_frame_progress(String const& state_type, SampleView sample) const;
  [[nodiscard]] std::expected<float, StarException> query_state_timer(String const& state_type, SampleView sample) const;
  [[nodiscard]] std::expected<bool, StarException> query_state_reverse(String const& state_type, SampleView sample) const;
  [[nodiscard]] std::expected<float, StarException> query_state_cycle(String const& state_type, std::optional<String> state, SampleView sample) const;
  [[nodiscard]] std::expected<int, StarException> query_state_frames(String const& state_type, std::optional<String> state, SampleView sample) const;
  [[nodiscard]] std::expected<Json, StarException> query_state_property(String const& state_type, String const& property_name, std::optional<String> state, std::optional<int> frame, SampleView sample) const;
  [[nodiscard]] std::expected<Json, StarException> query_state_next_property(String const& state_type, String const& property_name, SampleView sample) const;
  [[nodiscard]] std::expected<Json, StarException> query_part_property(String const& part_name, String const& property_name, std::optional<String> state_type, std::optional<String> state, std::optional<int> frame, SampleView sample) const;
  [[nodiscard]] std::expected<Json, StarException> query_part_next_property(String const& part_name, String const& property_name, SampleView sample) const;
  [[nodiscard]] std::expected<Mat3F, StarException> query_group_transformation(std::span<String const> groups, SampleView sample) const;
  [[nodiscard]] std::expected<Mat3F, StarException> query_part_transformation(String const& part_name, SampleView sample,
      std::optional<std::reference_wrapper<AnchorPath const>> path) const;
  [[nodiscard]] std::expected<Mat3F, StarException> query_final_part_transformation(String const& part_name, SampleView sample) const;
  [[nodiscard]] std::expected<std::optional<Vec2F>, StarException> query_part_point(String const& part_name, String const& property_name, SampleView sample) const;
  [[nodiscard]] std::expected<std::optional<PolyF>, StarException> query_part_poly(String const& part_name, String const& property_name, SampleView sample) const;
  [[nodiscard]] std::expected<HashMap<String, String>, StarException> animation_tags(SampleView sample) const;
  [[nodiscard]] std::expected<String, StarException> query_apply_part_tags(String const& part_name, String apply, SampleView sample) const;
  [[nodiscard]] std::expected<float, StarException> query_current_rotation_angle(String const& rotation_group, SampleView sample) const;
  [[nodiscard]] std::expected<List<Drawable>, StarException> query_drawables(Vec2F const& translate, SampleView sample) const;
  [[nodiscard]] std::expected<List<std::pair<Drawable, float>>, StarException> query_drawables_with_z_level(Vec2F const& translate, SampleView sample) const;
  template <typename Output>
    requires (std::same_as<Output, Drawable> || std::same_as<Output, std::pair<Drawable, float>>)
  [[nodiscard]] std::expected<List<Output>, StarException> collect_drawables(Vec2F const& translate, SampleView sample) const;
  [[nodiscard]] std::expected<void, StarException> append_light_sources(List<LightSource>& lights, Vec2F const& translate, SampleView sample) const;

  struct RotationGroup {
    float angular_velocity;
    Vec2F rotation_center;

    NetElementFloat target_angle;
    float current_angle = 0;
    float previous_angle = 0;

    NetElementEvent net_immediate_event;
  };

  struct TransformationGroup {
    Mat3F affine_transform() const;
    void set_affine_transform(Mat3F const& matrix);

    Mat3F local_affine_transform() const;
    void set_local_affine_transform(Mat3F const& matrix);

    Mat3F animation_affine_transform() const;
    void set_animation_affine_transform(Mat3F const& matrix);

    bool interpolated;

    Mat3F local_transform;
    std::size_t presentation_index = 0;
    Mat3F previous_transform;
    Mat3F current_transform;
    Mat3F previous_local_transform;
    Mat3F current_local_transform;
    Mat3F animation_base_transform = Mat3F::identity();
    bool presentation_history = false;

    NetElementFloat x_translation;
    NetElementFloat y_translation;
    NetElementFloat x_scale;
    NetElementFloat y_scale;
    NetElementFloat x_shear;
    NetElementFloat y_shear;

    float x_translation_animation;
    float y_translation_animation;
    float x_scale_animation;
    float y_scale_animation;
    float x_shear_animation;
    float y_shear_animation;

  };

  struct ParticleEmitter {
    struct ParticleConfig {
      ParticleVariantCreator creator;
      unsigned count;
      Vec2F offset;
      bool flip;
    };

    NetElementFloat emission_rate;
    float emission_rate_variance;
    NetElementData<RectF> offset_region;
    std::optional<String> anchor_part;
    StringList transformation_groups;
    std::optional<String> rotation_group;
    std::optional<Vec2F> rotation_center;

    List<ParticleConfig> particle_list;

    NetElementBool active;
    NetElementUInt burst_count;
    NetElementUInt random_select_count;
    NetElementEvent burst_event;

    float timer;
  };

  struct Light {
    NetElementBool active;
    NetElementFloat x_position;
    NetElementFloat y_position;
    NetElementData<Color> color;
    NetElementFloat point_angle;
    std::optional<String> anchor_part;
    StringList transformation_groups;
    std::optional<String> rotation_group;
    std::optional<Vec2F> rotation_center;

    std::optional<PeriodicFunction<float>> flicker;
    bool point_light;
    float point_beam;
    float beam_ambience;
  };

  enum class SoundSignal {
    Play,
    StopAll
  };

  struct Sound {
    float range_multiplier;
    NetElementData<StringList> sound_pool;
    NetElementFloat x_position;
    NetElementFloat y_position;
    NetElementFloat volume_target;
    NetElementFloat volume_ramp_time;
    NetElementFloat pitch_multiplier_target;
    NetElementFloat pitch_multiplier_ramp_time;
    NetElementInt loops;
    NetElementSignal<SoundSignal> signals;
    List<SoundSignal> retry_signals;
    std::size_t next_retry_signal = 0;
  };

  [[nodiscard]] std::expected<std::shared_ptr<AudioInstance>, StarException> load_sound(String const& path) const;
  [[nodiscard]] std::expected<void, StarException> update_state_sounds(DynamicTarget& target, String const& name,
      AnimatedPartSet::ActiveStateInformation const& state) const;
  [[nodiscard]] std::expected<void, StarException> update_independent_sounds(DynamicTarget& target);

  struct Effect {
    String type;
    float time;
    Directives directives;

    NetElementBool enabled;
    float timer;
  };

  struct StateInfo {
    NetElementSize state_index;
    NetElementEvent started_event;
    bool was_updated;
    NetElementBool reverse;
  };

  [[nodiscard]] std::expected<void, StarException> setup_net_states();
  [[nodiscard]] std::expected<void, StarException> load_config(Json config, String relative_path);

  [[nodiscard]] std::expected<void, StarException> net_elements_need_load(bool full) override;
  [[nodiscard]] std::expected<void, StarException> net_elements_need_store() override;

  [[nodiscard]] static std::expected<Json, StarException> merge_includes(Root& root, Json const& config, Json const& includes,
      String const& relative_path, StringList& include_chain);
  [[nodiscard]] std::expected<Mat3F, StarException> animation_group_transformation(String const& name, TransformationGroup const& group, Mat3F const& base, SampleView sample) const;

  std::reference_wrapper<Root> m_root;
  String m_relative_path;
  std::uint8_t m_animator_version;

  AnimatedPartSet m_animated_parts;
  StringList m_animation_state_types;
  OrderedHashMap<String, StateInfo> m_state_info;
  OrderedHashMap<String, RotationGroup> m_rotation_groups;
  OrderedHashMap<String, TransformationGroup> m_transformation_groups;
  OrderedHashMap<String, ParticleEmitter> m_particle_emitters;
  std::vector<std::size_t> m_particle_selection;
  OrderedHashMap<String, Light> m_lights;
  OrderedHashMap<String, Sound> m_sounds;
  OrderedHashMap<String, Effect> m_effects;

  NetElementData<Directives> m_processing_directives;
  NetElementFloat m_zoom;

  NetElementBool m_flipped;
  NetElementFloat m_flipped_relative_center_line;

  NetElementFloat m_animation_rate;

  NetElementHashMap<String, String> m_global_tags;
  StableStringMap<NetElementHashMap<String, String>> m_part_tags;
  HashMap<String, String> m_local_tags;

  HashMap<String,List<Drawable>> m_part_drawables;

  struct CachedPartDrawable {
    // Keep the whole tag-expanded path, including frames and directives.
    // Relative paths resolve against the animator's fixed source path.
    String path;
    bool centered;
    Drawable drawable;
  };
  mutable StringMap<CachedPartDrawable> m_cached_part_drawables;
  struct Presentation {
    struct Group {
      Mat3F local_transform;
      Mat3F transform;
      Mat3F animation_transform;
      std::uint64_t state_generation = 0;
    };
    struct PartTransform {
      Mat3F transform;
      std::uint64_t pose_generation = 0;
    };
    struct Tag {
      bool active = false;
      std::optional<String> value;
    };
    struct Attachment {
      bool active = false;
      std::optional<std::reference_wrapper<List<Drawable> const>> drawables;
    };
    std::size_t users = 0;
    // Zero is reserved for never-computed or invalid cache entries.
    std::uint64_t pose_generation = 0;
    std::uint64_t state_generation = 0;
    AnimatedPartSet::Sample parts;
    std::vector<Group> groups;
    // Local part/anchor transforms only; consumers apply global zoom and flip.
    std::vector<PartTransform> part_transforms;
    HashMap<String, Tag> tags;
    // Borrow attachments; only the final transformed output copies Drawables.
    HashMap<String, Attachment> drawables;
    void invalidate_part_transforms();
    void invalidate_state();
  };
  // Appending slots never invalidates the references held by existing samples.
  mutable std::deque<Presentation> m_presentations;
};

}

export module star.networked_animator;

export namespace Star {
  using ::Star::NetworkedAnimator;
  using ::Star::NetworkedAnimatorExceptionTag;
  using ::Star::NetworkedAnimatorException;
}

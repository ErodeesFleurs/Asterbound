module;
import std;
import star.string;
import star.format;
import star.json;

import star.ordered_map;
import star.matrix3;

namespace Star {

struct AnimatedPartSetExceptionTag {
  static constexpr std::string_view name() { return "AnimatedPartSetException"; }
};
using AnimatedPartSetException = StarError<AnimatedPartSetExceptionTag, StarException>;

template <typename Visitor, typename Value>
concept animation_visitor = requires(Visitor& visitor, String const& name, Value const& value) {
  { std::invoke(visitor, name, value) } -> std::same_as<std::expected<void, StarException>>;
};

// Defines a "animated" data set constructed in such a way that it is very
// useful for doing generic animations with lots of additional animation data.
// It is made up of two concepts, "states" and "parts".
//
// States:
//
// There are N "state types" defined, which each defines a set of mutually
// exclusive states that each "state type" can be in.  For example, one state
// type might be "movement", and the "movement" states might be "idle", "walk",
// and "run.  Another state type might be "attack" which could have as its
// states "idle", and "melee".  Each state type will have exactly one currently
// active state, so this class may, for example,  be in the total state of
// "movement:idle" and "attack:melee".  Each state within each state type is
// animated, so that over time the state frame increases and may loop around,
// or transition into another state so that that state type without interaction
// may go from "melee" to "idle" when the "melee" state animation is finished.
// This is defined by the individual state config in the configuration passed
// into the constructor.
//
// Parts:
//
// Each instance of this class also can have N "Parts" defined, which are
// groups of properties that "listen" to active states.  Each part can "listen"
// to one or more state types, and the first matching state x state type pair
// (in order of state type priority which is specified in the config) is
// chosen, and the properties from that state type and state are merged into
// the part to produce the final active part information.  Rather than having a
// single image or image set for each part, since this class is intended to be
// as generic as possible, all of this data is assumed to be queried from the
// part properties, so that things such as image data as well as other things
// like damage or collision polys can be stored along with the animation
// frames, the part state, the base part, whichever is most applicable.
class AnimatedPartSet {
public:
  struct ActiveStateInformation {
    String state_type_name;
    String state_name;
    float timer = 0;
    unsigned frame = 0;
    float frame_progress = 0;
    JsonObject properties;
    bool reverse = false;
    unsigned next_frame = 0;
    JsonObject next_properties;
  };

  struct AffinePose {
    Mat3F animation_affine_transform() const;
    void set_animation_affine_transform(Mat3F const& matrix);

    float x_translation_animation = 0;
    float y_translation_animation = 0;
    float x_scale_animation = 1;
    float y_scale_animation = 1;
    float x_shear_animation = 0;
    float y_shear_animation = 0;
  };

  struct ActivePartInformation : AffinePose {
    String part_name;
    // If a state match is found, this will be set.
    std::optional<ActiveStateInformation> active_state;
    JsonObject properties;
    JsonObject next_properties;
  };

  enum AnimationMode {
    End,
    Loop,
    Transition
  };

  struct State {
    unsigned frames;
    float cycle;
    AnimationMode animation_mode;
    String transition_state;
    JsonObject state_properties;
    JsonObject state_frame_properties;
  };

  struct StateType {
    float priority;
    bool enabled;
    String default_state;
    JsonObject state_type_properties;
    OrderedHashMap<String, std::shared_ptr<State const>> states;

    // Lazily refreshed frame information; state changes still require nonconst access.
    mutable ActiveStateInformation active_state;
    std::shared_ptr<State const> active_state_reference;
    mutable bool active_state_dirty;
    float previous_timer = 0;
    float timer_advance = 0;
    std::size_t presentation_index = 0;
  };

  struct PartState {
    JsonObject part_state_properties;
    JsonObject part_state_frame_properties;
  };

  struct Part {
    JsonObject part_properties;
    StringMap<StringMap<PartState>> part_states;

    // Derived from part properties and the currently active state.
    mutable ActivePartInformation active_part;
    mutable bool active_part_dirty = true;
    mutable std::size_t presentation_index = std::numeric_limits<std::size_t>::max();
    mutable AffinePose active_part_base_pose;
  };

  // Reusable derived data owned by one visual view, never by logical state.
  struct Sample {
    struct StateCache {
      ActiveStateInformation state;
      bool dirty = true;
      bool initialized = false;
      std::shared_ptr<State const> override_state;
      std::shared_ptr<State const> property_state;
      unsigned property_frame = 0;
      unsigned property_next_frame = 0;
    };
    struct TransformEndpoints {
      AffinePose base;
      AffinePose current;
      std::optional<AffinePose> next;
    };
    struct PartCache {
      ActivePartInformation part;
      bool dirty = true;
      // Keys describe discrete sources only; timing and affine interpolation
      // are refreshed whenever this sample is prepared.
      TransformEndpoints transforms;
      std::shared_ptr<State const> state;
      unsigned frame = 0;
      unsigned next_frame = 0;
      bool properties_valid = false;
    };
    float interpolation = 1;
    bool logical_reset = false;
    std::vector<StateCache> states;
    std::vector<PartCache> parts;
    std::shared_ptr<JsonObject const> configuration;
    std::uint8_t animator_version = 0;
    std::uint64_t owner_id = 0;
  };

  AnimatedPartSet();
  AnimatedPartSet(AnimatedPartSet const&) = default;
  AnimatedPartSet(AnimatedPartSet&&) = default;
  AnimatedPartSet& operator=(AnimatedPartSet const& other);
  AnimatedPartSet& operator=(AnimatedPartSet&& other);
  [[nodiscard]] static std::expected<AnimatedPartSet, StarException> create(Json config, std::uint8_t animator_version);

  // Reuses a view's storage and initializes it from the current logical endpoints.
  void prepare_sample(Sample& sample, float interpolation) const;
  [[nodiscard]] std::expected<bool, StarException> set_presentation_state(String const& state_type_name, String const& state_name, bool always_start, bool reverse, Sample& sample) const;

  // Returns the available state types.
  StringList state_types() const;

  // If a state type is disabled, no parts will match against it even
  // if they have entries for that state type.
  [[nodiscard]] std::expected<void, StarException> set_state_type_enabled(String const& state_type_name, bool enabled);
  [[nodiscard]] std::expected<void, StarException> set_enabled_state_types(StringList const& state_type_names);
  [[nodiscard]] std::expected<bool, StarException> state_type_enabled(String const& state_type_name) const;

  // Returns the available states for the given state type.
  [[nodiscard]] std::expected<StringList, StarException> states(String const& state_type_name) const;

  StringList part_names() const;

  // Sets the active state for this state type.  If the state is different than
  // the previously set state, will start the new states animation off at the
  // beginning.  If always_start is true, then starts the state animation off at
  // the beginning even if no state change has occurred.  Returns true if a
  // state animation reset was done.
  [[nodiscard]] std::expected<bool, StarException> set_active_state(String const& state_type_name, String const& state_name, bool always_start = false, bool reverse = false);

  // Restart this given state type's timer off at the beginning.
  [[nodiscard]] std::expected<void, StarException> restart_state(String const& state_type_name);

  [[nodiscard]] std::expected<std::reference_wrapper<ActiveStateInformation const>, StarException> active_state(String const& state_type_name) const;
  [[nodiscard]] std::expected<std::reference_wrapper<ActiveStateInformation const>, StarException> active_state(String const& state_type_name, Sample& sample) const;
  [[nodiscard]] std::expected<std::reference_wrapper<ActivePartInformation const>, StarException> active_part(String const& part_name) const;
  [[nodiscard]] std::expected<std::reference_wrapper<ActivePartInformation const>, StarException> active_part(String const& part_name, Sample& sample) const;
  // The part must belong to this set, as returned by const_parts().
  [[nodiscard]] std::expected<std::reference_wrapper<ActivePartInformation const>, StarException> active_part(Part const& part) const;
  [[nodiscard]] std::expected<std::reference_wrapper<ActivePartInformation const>, StarException> active_part(Part const& part, Sample& sample) const;
  [[nodiscard]] std::expected<std::reference_wrapper<State const>, StarException> get_state(String const& state_type_name, String const& state_name) const;

  StringMap<Part> const& const_parts() const;
  StringMap<Part>& parts();

  [[nodiscard]] std::expected<void, StarException> for_each_active_state(std::nullptr_t) const {
    return std::unexpected(AnimatedPartSetException("Empty active-state callback"));
  }
  [[nodiscard]] std::expected<void, StarException> for_each_active_state(std::nullptr_t, Sample&) const {
    return std::unexpected(AnimatedPartSetException("Empty active-state callback"));
  }

  // Function will be given the name of each state type, and the
  // ActiveStateInformation for the active state for that state type.
  template <typename Visitor> requires animation_visitor<Visitor, ActiveStateInformation>
  [[nodiscard]] std::expected<void, StarException> for_each_active_state(Visitor&& visitor) const {
    return visit_states(visitor, std::nullopt);
  }
  template <typename Visitor> requires animation_visitor<Visitor, ActiveStateInformation>
  [[nodiscard]] std::expected<void, StarException> for_each_active_state(Visitor&& visitor, Sample& sample) const {
    return visit_states(visitor, std::ref(sample));
  }

  [[nodiscard]] std::expected<void, StarException> for_each_active_part(std::nullptr_t) const {
    return std::unexpected(AnimatedPartSetException("Empty active-part callback"));
  }
  [[nodiscard]] std::expected<void, StarException> for_each_active_part(std::nullptr_t, Sample&) const {
    return std::unexpected(AnimatedPartSetException("Empty active-part callback"));
  }

  // Function will be given the name of each part, and the
  // ActivePartInformation for the active part.
  template <typename Visitor> requires animation_visitor<Visitor, ActivePartInformation>
  [[nodiscard]] std::expected<void, StarException> for_each_active_part(Visitor&& visitor) const {
    return visit_parts(visitor, std::nullopt);
  }
  template <typename Visitor> requires animation_visitor<Visitor, ActivePartInformation>
  [[nodiscard]] std::expected<void, StarException> for_each_active_part(Visitor&& visitor, Sample& sample) const {
    return visit_parts(visitor, std::ref(sample));
  }

  // Useful for serializing state changes.  Since each set of states for a
  // state type is ordered, it is possible to simply serialize and deserialize
  // the state index for that state type.
  [[nodiscard]] std::expected<std::size_t, StarException> active_state_index(String const& state_type_name) const;
  [[nodiscard]] std::expected<bool, StarException> active_state_reverse(String const& state_type_name) const;
  [[nodiscard]] std::expected<bool, StarException> set_active_state_index(String const& state_type_name, std::size_t state_index, bool always_start = false, bool reverse = false);

  // Animate each state type forward 'dt' time, and either change state frames
  // or transition to new states, depending on the config.
  [[nodiscard]] std::expected<void, StarException> update(float dt);

  // Pushes all the animations into their final state
  [[nodiscard]] std::expected<void, StarException> finish_animations();

  std::uint8_t version() const;

  [[nodiscard]] std::expected<Json, StarException> get_state_frame_property(String const& state_type, String const& property_name, String const& state, int frame) const;
  [[nodiscard]] std::expected<Json, StarException> get_part_state_frame_property(String const& part_name, String const& property_name, String const& state_type, String const& state, int frame) const;

private:
  friend class NetworkedAnimator;
  // Prepare all terminal cursors in scratch; publish only after the enclosing pose succeeds.
  [[nodiscard]] std::expected<void, StarException> stage_finished_states(Sample& sample) const;
  void commit_finished_states(Sample const& sample);
  struct SampleIdentity {
    SampleIdentity();
    SampleIdentity(SampleIdentity const&) : SampleIdentity() {}
    std::uint64_t value;
  };
  SampleIdentity m_sample_identity;
  bool sample_prepared(Sample const& sample) const;
  [[nodiscard]] std::expected<std::reference_wrapper<ActiveStateInformation const>, StarException> resolve_state(StateType const& entry, std::optional<std::reference_wrapper<Sample>> sample) const;
  [[nodiscard]] std::expected<std::reference_wrapper<ActivePartInformation const>, StarException> resolve_part(Part const& entry, std::optional<std::reference_wrapper<Sample>> sample) const;

  template <typename Visitor> requires animation_visitor<Visitor, ActiveStateInformation>
  [[nodiscard]] std::expected<void, StarException> visit_states(Visitor& visitor, std::optional<std::reference_wrapper<Sample>> sample) const {
    if constexpr (requires { static_cast<bool>(visitor); }) {
      if (!static_cast<bool>(visitor)) return std::unexpected(AnimatedPartSetException("Empty active-state callback"));
    }
    if (sample && !sample_prepared(sample->get())) return std::unexpected(AnimatedPartSetException("Unprepared animation sample"));
    for (auto const& [name, entry] : m_state_types) {
      auto active = resolve_state(entry, sample);
      if (!active) return std::unexpected(std::move(active.error()));
      auto called = std::invoke(visitor, name, active->get());
      if (!called) return std::unexpected(std::move(called.error()));
    }
    return {};
  }

  template <typename Visitor> requires animation_visitor<Visitor, ActivePartInformation>
  [[nodiscard]] std::expected<void, StarException> visit_parts(Visitor& visitor, std::optional<std::reference_wrapper<Sample>> sample) const {
    if constexpr (requires { static_cast<bool>(visitor); }) {
      if (!static_cast<bool>(visitor)) return std::unexpected(AnimatedPartSetException("Empty active-part callback"));
    }
    if (sample && !sample_prepared(sample->get())) return std::unexpected(AnimatedPartSetException("Unprepared animation sample"));
    for (auto const& [name, entry] : m_parts) {
      auto active = resolve_part(entry, sample);
      if (!active) return std::unexpected(std::move(active.error()));
      auto called = std::invoke(visitor, name, active->get());
      if (!called) return std::unexpected(std::move(called.error()));
    }
    return {};
  }
  [[nodiscard]] static std::expected<AnimationMode, StarException> string_to_animation_mode(String const& string);

  [[nodiscard]] std::expected<void, StarException> freshen_active_state(StateType const& state_type, std::optional<std::reference_wrapper<Sample>> sample) const;
  [[nodiscard]] std::expected<void, StarException> freshen_active_part(Part const& part, std::optional<std::reference_wrapper<Sample>> sample) const;

  OrderedHashMap<String, StateType> m_state_types;
  StringMap<Part> m_parts;

  std::uint8_t m_animator_version = 0;
  // Retain the immutable input for sample identity without allocating an ID.
  std::shared_ptr<JsonObject const> m_sample_configuration;
  // A retained reference to the map survives assignment; exposure is sticky.
  bool m_mutable_parts_exposed = false;
};

}

export module star.animated_part_set;

export namespace Star {
  using ::Star::AnimatedPartSetExceptionTag;
  using ::Star::AnimatedPartSetException;
  using ::Star::AnimatedPartSet;
}

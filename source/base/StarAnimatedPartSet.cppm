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
    String stateTypeName;
    String stateName;
    float timer = 0;
    unsigned frame = 0;
    float frameProgress = 0;
    JsonObject properties;
    bool reverse = false;
    unsigned nextFrame = 0;
    JsonObject nextProperties;
  };

  struct ActivePartInformation {
    String partName;
    // If a state match is found, this will be set.
    std::optional<ActiveStateInformation> activeState;
    JsonObject properties;
    JsonObject nextProperties;

    Mat3F animationAffineTransform() const;
    void setAnimationAffineTransform(Mat3F const& matrix);
    void setAnimationAffineTransform(Mat3F const& mat1, Mat3F const& mat2, float progress);

    float xTranslationAnimation = 0;
    float yTranslationAnimation = 0;
    float xScaleAnimation = 1;
    float yScaleAnimation = 1;
    float xShearAnimation = 0;
    float yShearAnimation = 0;
  };

    enum AnimationMode {
    End,
    Loop,
    Transition
  };

  struct State {
    unsigned frames;
    float cycle;
    AnimationMode animationMode;
    String transitionState;
    JsonObject stateProperties;
    JsonObject stateFrameProperties;
  };

  struct StateType {
    float priority;
    bool enabled;
    String defaultState;
    JsonObject stateTypeProperties;
    OrderedHashMap<String, std::shared_ptr<State const>> states;

    // Lazily refreshed frame information; state changes still require nonconst access.
    mutable ActiveStateInformation activeState;
    std::optional<std::reference_wrapper<State const>> activeStateReference;
    mutable bool activeStateDirty;
    float previousTimer = 0;
    float timerAdvance = 0;
    mutable std::size_t presentationIndex = 0;
  };

  struct PartState {
    JsonObject partStateProperties;
    JsonObject partStateFrameProperties;
  };

  struct Part {
    JsonObject partProperties;
    StringMap<StringMap<PartState>> partStates;

    // Derived from part properties and the currently active state.
    mutable ActivePartInformation activePart;
    mutable bool activePartDirty;
    mutable std::size_t presentationIndex = 0;
    mutable Mat3F activePartBaseTransform = Mat3F::identity();
  };

  // Reusable derived data owned by one visual view, never by logical state.
  struct Sample {
    struct StateCache {
      ActiveStateInformation state;
      bool dirty = true;
      bool initialized = false;
      std::shared_ptr<State const> propertyState;
      unsigned propertyFrame = 0;
      unsigned propertyNextFrame = 0;
    };
    struct PartCache {
      ActivePartInformation part;
      bool dirty = true;
      // Keys describe discrete sources only; timing and affine interpolation
      // are refreshed whenever this sample is prepared.
      Part const* definition = nullptr;
      PartState const* stateDefinition = nullptr;
      State const* state = nullptr;
      unsigned frame = 0;
      unsigned nextFrame = 0;
      bool propertiesValid = false;
    };
    float interpolation = 1;
    std::vector<StateCache> states;
    std::vector<PartCache> parts;
    std::shared_ptr<JsonObject const> configuration;
    std::uint8_t animatorVersion = 0;
  };

  AnimatedPartSet();
  AnimatedPartSet(AnimatedPartSet const&) = default;
  AnimatedPartSet(AnimatedPartSet&&) = default;
  AnimatedPartSet& operator=(AnimatedPartSet const& other);
  AnimatedPartSet& operator=(AnimatedPartSet&& other);
  [[nodiscard]] static std::expected<AnimatedPartSet, StarException> create(Json config, std::uint8_t animatorVersion);

  // Reuses a view's storage and initializes it from the current logical endpoints.
  void prepareSample(Sample& sample, float interpolation) const;
  [[nodiscard]] std::expected<bool, StarException> setPresentationState(String const& stateTypeName, String const& stateName, bool alwaysStart, bool reverse, Sample& sample) const;

  // Returns the available state types.
  StringList stateTypes() const;

  // If a state type is disabled, no parts will match against it even
  // if they have entries for that state type.
  [[nodiscard]] std::expected<void, StarException> setStateTypeEnabled(String const& stateTypeName, bool enabled);
  [[nodiscard]] std::expected<void, StarException> setEnabledStateTypes(StringList const& stateTypeNames);
  [[nodiscard]] std::expected<bool, StarException> stateTypeEnabled(String const& stateTypeName) const;

  // Returns the available states for the given state type.
  [[nodiscard]] std::expected<StringList, StarException> states(String const& stateTypeName) const;

  StringList partNames() const;

  // Sets the active state for this state type.  If the state is different than
  // the previously set state, will start the new states animation off at the
  // beginning.  If alwaysStart is true, then starts the state animation off at
  // the beginning even if no state change has occurred.  Returns true if a
  // state animation reset was done.
  [[nodiscard]] std::expected<bool, StarException> setActiveState(String const& stateTypeName, String const& stateName, bool alwaysStart = false, bool reverse = false);

  // Restart this given state type's timer off at the beginning.
  [[nodiscard]] std::expected<void, StarException> restartState(String const& stateTypeName);

  [[nodiscard]] std::expected<std::reference_wrapper<ActiveStateInformation const>, StarException> activeState(String const& stateTypeName, Sample* sample = nullptr) const;
  [[nodiscard]] std::expected<std::reference_wrapper<ActivePartInformation const>, StarException> activePart(String const& partName, Sample* sample = nullptr) const;
  // The part must belong to this set, as returned by constParts().
  [[nodiscard]] std::expected<std::reference_wrapper<ActivePartInformation const>, StarException> activePart(Part const& part, Sample* sample = nullptr) const;
  [[nodiscard]] std::expected<std::reference_wrapper<State const>, StarException> getState(String const& stateTypeName, String const& stateName) const;

  StringMap<Part> const& constParts() const;
  StringMap<Part>& parts();

  // Function will be given the name of each state type, and the
  // ActiveStateInformation for the active state for that state type.
  [[nodiscard]] std::expected<void, StarException> forEachActiveState(std::function<std::expected<void, StarException>(String const&, ActiveStateInformation const&)> callback, Sample* sample = nullptr) const;

  // Function will be given the name of each part, and the
  // ActivePartInformation for the active part.
  [[nodiscard]] std::expected<void, StarException> forEachActivePart(std::function<std::expected<void, StarException>(String const&, ActivePartInformation const&)> callback, Sample* sample = nullptr) const;

  // Useful for serializing state changes.  Since each set of states for a
  // state type is ordered, it is possible to simply serialize and deserialize
  // the state index for that state type.
  [[nodiscard]] std::expected<std::size_t, StarException> activeStateIndex(String const& stateTypeName) const;
  [[nodiscard]] std::expected<bool, StarException> activeStateReverse(String const& stateTypeName) const;
  [[nodiscard]] std::expected<bool, StarException> setActiveStateIndex(String const& stateTypeName, std::size_t stateIndex, bool alwaysStart = false, bool reverse = false);

  // Animate each state type forward 'dt' time, and either change state frames
  // or transition to new states, depending on the config.
  [[nodiscard]] std::expected<void, StarException> update(float dt);

  // Pushes all the animations into their final state
  [[nodiscard]] std::expected<void, StarException> finishAnimations();

  std::uint8_t version() const;

  [[nodiscard]] std::expected<Json, StarException> getStateFrameProperty(String const& stateType, String const& propertyName, String state, int frame) const;
  [[nodiscard]] std::expected<Json, StarException> getPartStateFrameProperty(String const& partName, String const& propertyName, String const& stateType, String state, int frame) const;

private:
  [[nodiscard]] static std::expected<AnimationMode, StarException> stringToAnimationMode(String const& string);

  [[nodiscard]] std::expected<void, StarException> freshenActiveState(StateType const& stateType, Sample* sample = nullptr) const;
  [[nodiscard]] std::expected<void, StarException> freshenActivePart(Part const& part, Sample* sample = nullptr) const;

  OrderedHashMap<String, StateType> m_stateTypes;
  StringMap<Part> m_parts;

  std::uint8_t m_animatorVersion = 0;
  // Retain the immutable input for sample identity without allocating an ID.
  std::shared_ptr<JsonObject const> m_sampleConfiguration;
  // A retained reference to the map survives assignment; exposure is sticky.
  bool m_mutablePartsExposed = false;
};

}

export module star.animated_part_set;

export namespace Star {
  using ::Star::AnimatedPartSetExceptionTag;
  using ::Star::AnimatedPartSetException;
  using ::Star::AnimatedPartSet;
}

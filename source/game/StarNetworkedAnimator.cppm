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

// Wraps an AnimatedPartSet with a set of optional light sources and particle
// emitters to produce a network capable animation system.
class NetworkedAnimator : public NetElementSyncGroup {
public:
  // Target for dynamic render data such as sounds and particles that are not
  // persistent and are instead produced during a call to update, and may need
  // to be tracked over time.
  class DynamicTarget {
  public:
    // Calls stopAudio()
    ~DynamicTarget();

    List<std::shared_ptr<AudioInstance>> pullNewAudios();
    List<Particle> pullNewParticles();

    // Stops all looping audio immediately and lets non-looping audio finish
    // normally
    void stopAudio();

    // Updates the base position of all un-pulled particles and all active
    // audio.  Not necessary to call, but if not called all pulled data will be
    // relative to (0, 0).
    void updatePosition(Vec2F const& position);

  private:
    friend class NetworkedAnimator;

    void clearFinishedAudio();

    struct PersistentSound {
      Json sound;
      std::shared_ptr<AudioInstance> audio;
      float stopRampTime;
    };

    struct ImmediateSound {
      Json sound;
      std::shared_ptr<AudioInstance> audio;
    };

    Vec2F position;
    List<std::shared_ptr<AudioInstance>> pendingAudios;
    List<Particle> pendingParticles;
    StringMap<PersistentSound> statePersistentSounds;
    StringMap<ImmediateSound> stateImmediateSounds;
    StringMap<List<std::shared_ptr<AudioInstance>>> independentSounds;
    HashMap<std::shared_ptr<AudioInstance>, Vec2F> currentAudioBasePositions;
  };

  [[nodiscard]] static std::expected<std::shared_ptr<NetworkedAnimator>, StarException> create(Root& root);
  // If passed a string as config, NetworkedAnimator will interpret this as a
  // config path, otherwise it is interpreted as the literal config.
  [[nodiscard]] static std::expected<std::shared_ptr<NetworkedAnimator>, StarException> create(Root& root, Json config, String relativePath = String());

  [[nodiscard]] static std::expected<std::shared_ptr<NetworkedAnimator>, StarException> create(NetworkedAnimator const& animator);
  NetworkedAnimator(NetworkedAnimator&&) = delete;
  NetworkedAnimator(NetworkedAnimator const&) = delete;
  NetworkedAnimator& operator=(NetworkedAnimator&&) = delete;
  NetworkedAnimator& operator=(NetworkedAnimator const&) = delete;


  // Read-only semantic sampling for a visual pass. Local render overrides are
  // isolated from logical state. Nested scopes inherit the outer sample.
  class Sample {
  public:
    Sample(NetworkedAnimator const& animator, float interpolation);
    ~Sample();
    Sample(Sample const&) = delete;
    Sample& operator=(Sample const&) = delete;
  private:
    NetworkedAnimator const& m_animator;
  };
  [[nodiscard]] Sample sample(float interpolation) const;
  StringList stateTypes() const;
  [[nodiscard]] std::expected<StringList, StarException> states(String const& stateType) const;

  // Returns whether a state change occurred.  If startNew is true, always
  // forces a state change and starts the state off at the beginning even if
  // this state is already the current state.
  [[nodiscard]] std::expected<bool, StarException> setState(String const& stateType, String const& state, bool startNew = false, bool reverse = false);
  [[nodiscard]] std::expected<bool, StarException> setLocalState(String const& stateType, String const& state, bool startNew = false, bool reverse = false);
  [[nodiscard]] std::expected<String, StarException> state(String const& stateType) const;
  [[nodiscard]] std::expected<int, StarException> stateFrame(String const& stateType) const;
  [[nodiscard]] std::expected<int, StarException> stateNextFrame(String const& stateType) const;
  [[nodiscard]] std::expected<float, StarException> stateFrameProgress(String const& stateType) const;
  [[nodiscard]] std::expected<float, StarException> stateTimer(String const& stateType) const;
  [[nodiscard]] std::expected<bool, StarException> stateReverse(String const& stateType) const;

  [[nodiscard]] std::expected<float, StarException> stateCycle(String const& stateType, std::optional<String> state) const;
  [[nodiscard]] std::expected<int, StarException> stateFrames(String const& stateType, std::optional<String> state) const;

  [[nodiscard]] std::expected<bool, StarException> hasState(String const& stateType, std::optional<String> const& state = {}) const;

  StringMap<AnimatedPartSet::Part> const& constParts() const;
  StringMap<AnimatedPartSet::Part>& parts();
  StringList partNames() const;

  // Queries, if it exists, a property value from the underlying
  // AnimatedPartSet for the given state or part.  If the property does not
  // exist, returns null.
  [[nodiscard]] std::expected<Json, StarException> stateProperty(String const& stateType, String const& propertyName, std::optional<String> state = {}, std::optional<int> frame = {}) const;
  [[nodiscard]] std::expected<Json, StarException> stateNextProperty(String const& stateType, String const& propertyName) const;
  [[nodiscard]] std::expected<Json, StarException> partProperty(String const& partName, String const& propertyName, std::optional<String> stateType = {}, std::optional<String> state = {}, std::optional<int> frame = {}) const;
  [[nodiscard]] std::expected<Json, StarException> partNextProperty(String const & partName, String const & propertyName) const;

  // Returns the transformation from flipping and zooming that is applied to
  // all parts in the NetworkedAnimator.
  Mat3F globalTransformation() const;
  // The transformation applied from the given set of transformation groups
  [[nodiscard]] std::expected<Mat3F, StarException> groupTransformation(StringList const& transformationGroups) const;
  // The transformation that is applied to the given part NOT including the
  // global transformation
  [[nodiscard]] std::expected<Mat3F, StarException> partTransformation(String const& partName) const;
  // Returns the total transformation for the given part, which includes the
  // globalTransformation, as well as the part rotation, scaling, and
  // translation.
  [[nodiscard]] std::expected<Mat3F, StarException> finalPartTransformation(String const& partName) const;

  // partPoint / partPoly takes a propertyName and looks up the associated part
  // property and interprets is a Vec2F or a PolyF, then applies the final part
  // transformation and returns it.
  [[nodiscard]] std::expected<std::optional<Vec2F>, StarException> partPoint(String const& partName, String const& propertyName) const;
  [[nodiscard]] std::expected<std::optional<PolyF>, StarException> partPoly(String const& partName, String const& propertyName) const;

  // Every part image can have one or more <tag> directives in it, which if set
  // here will be replaced by the tag value when constructing Drawables.  All
  // Drawables can also have a <frame> tag which will be set to whatever the
  // current state frame is (1 indexed, so the first frame is 1).
  void setGlobalTag(String tagName, std::optional<String> tagValue = {});
  void removeGlobalTag(String const& tagName);
  std::optional<std::reference_wrapper<String const>> globalTag(String const& tagName) const;
  void setPartTag(String const& partType, String tagName, std::optional<String> tagValue = {});
  void setLocalTag(String tagName, std::optional<String> tagValue = {});

  void setPartDrawables(String const& partName, List<Drawable> drawables);
  void addPartDrawables(String const& partName, List<Drawable> drawables);

  [[nodiscard]] std::expected<String, StarException> applyPartTags(String const& partName, String apply) const;

  void setProcessingDirectives(Directives const& directives);
  void setZoom(float zoom);
  bool flipped() const;
  float flippedRelativeCenterLine() const;
  void setFlipped(bool flipped, float relativeCenterLine = 0.0f);

  // Animation rate defaults to 1.0, which means normal animation speed.  This
  // can be used to globally speed up or slow down all components of
  // NetworkedAnimator together.
  void setAnimationRate(float rate);
  float animationRate();

  // Given angle is an absolute angle.  Will rotate over time at the configured
  // angular velocity unless the immediate flag is set.
  bool hasRotationGroup(String const& rotationGroup) const;
  [[nodiscard]] std::expected<void, StarException> rotateGroup(String const& rotationGroup, float targetAngle, bool immediate = false);
  [[nodiscard]] std::expected<float, StarException> currentRotationAngle(String const& rotationGroup) const;

  // Transformation groups can be used for arbitrary part transforamtions.
  // They apply immediately, and are optionally interpolated on slaves.
  bool hasTransformationGroup(String const& transformationGroup) const;
  [[nodiscard]] std::expected<void, StarException> translateTransformationGroup(String const& transformationGroup, Vec2F const& translation);
  [[nodiscard]] std::expected<void, StarException> rotateTransformationGroup(String const& transformationGroup, float rotation, Vec2F const& rotationCenter = Vec2F());
  [[nodiscard]] std::expected<void, StarException> scaleTransformationGroup(String const& transformationGroup, float scale, Vec2F const& scaleCenter = Vec2F());
  [[nodiscard]] std::expected<void, StarException> scaleTransformationGroup(String const& transformationGroup, Vec2F const& scale, Vec2F const& scaleCenter = Vec2F());
  [[nodiscard]] std::expected<void, StarException> transformTransformationGroup(String const& transformationGroup, float a, float b, float c, float d, float tx, float ty);
  [[nodiscard]] std::expected<void, StarException> resetTransformationGroup(String const& transformationGroup);
  [[nodiscard]] std::expected<void, StarException> setTransformationGroup(String const& transformationGroup, Mat3F transform);
  [[nodiscard]] std::expected<Mat3F, StarException> getTransformationGroup(String const& transformationGroup);

  [[nodiscard]] std::expected<void, StarException> translateLocalTransformationGroup(String const& transformationGroup, Vec2F const& translation);
  [[nodiscard]] std::expected<void, StarException> rotateLocalTransformationGroup(String const& transformationGroup, float rotation, Vec2F const& rotationCenter = Vec2F());
  [[nodiscard]] std::expected<void, StarException> scaleLocalTransformationGroup(String const& transformationGroup, float scale, Vec2F const& scaleCenter = Vec2F());
  [[nodiscard]] std::expected<void, StarException> scaleLocalTransformationGroup(String const& transformationGroup, Vec2F const& scale, Vec2F const& scaleCenter = Vec2F());
  [[nodiscard]] std::expected<void, StarException> transformLocalTransformationGroup(String const& transformationGroup, float a, float b, float c, float d, float tx, float ty);
  [[nodiscard]] std::expected<void, StarException> resetLocalTransformationGroup(String const& transformationGroup);
  [[nodiscard]] std::expected<void, StarException> setLocalTransformationGroup(String const& transformationGroup, Mat3F transform);
  [[nodiscard]] std::expected<Mat3F, StarException> getLocalTransformationGroup(String const& transformationGroup);

  bool hasParticleEmitter(String const& emitterName) const;
  // Active particle emitters emit over time based on emission rate/variance.
  [[nodiscard]] std::expected<void, StarException> setParticleEmitterActive(String const& emitterName, bool active);
  // Set the emission rate in particles / sec for a given emitter
  [[nodiscard]] std::expected<void, StarException> setParticleEmitterEmissionRate(String const& emitterName, float emissionRate);
  // Set the optional particle emitter offset region, which particles will be
  // spread around randomly before being spawned
  [[nodiscard]] std::expected<void, StarException> setParticleEmitterOffsetRegion(String const& emitterName, RectF const& offsetRegion);

  // Number of times to cycle when emitting a burst of particles.
  [[nodiscard]] std::expected<void, StarException> setParticleEmitterBurstCount(String const& emitterName, unsigned burstCount);

  // Cause one time burst of all types of particles in an emitter looping around
  // burstCount times
  [[nodiscard]] std::expected<void, StarException> burstParticleEmitter(String const& emitterName);

  bool hasLight(String const& lightName) const;
  [[nodiscard]] std::expected<void, StarException> setLightActive(String const& lightName, bool active);
  [[nodiscard]] std::expected<void, StarException> setLightPosition(String const& lightName, Vec2F position);
  [[nodiscard]] std::expected<void, StarException> setLightColor(String const& lightName, Color color);
  [[nodiscard]] std::expected<void, StarException> setLightPointAngle(String const& lightName, float angle);

  bool hasSound(String const& soundName) const;
  [[nodiscard]] std::expected<void, StarException> setSoundPool(String const& soundName, StringList soundPool);
  // Plays a sound from the given independent sound pool.  Multiple sounds may
  // be played as part of this group, and playing a new one will not interrupt
  // an older one.
  [[nodiscard]] std::expected<void, StarException> playSound(String const& soundName, int loops = 0);

  // Setting the sound position, volume, and speed will affect future sounds in
  // this group, as well as any still active sounds from this group.
  [[nodiscard]] std::expected<void, StarException> setSoundPosition(String const& soundName, Vec2F const& position);

  [[nodiscard]] std::expected<void, StarException> setSoundVolume(String const& soundName, float volume, float rampTime = 0.0f);
  [[nodiscard]] std::expected<void, StarException> setSoundPitchMultiplier(String const& soundName, float pitchMultiplier, float rampTime = 0.0f);

  // Stop all sounds played from this sound group
  [[nodiscard]] std::expected<void, StarException> stopAllSounds(String const& soundName, float rampTime = 0.0f);

  [[nodiscard]] std::expected<void, StarException> setEffectEnabled(String const& effect, bool enabled);

  [[nodiscard]] std::expected<List<Drawable>, StarException> drawables(Vec2F const& translate = Vec2F()) const;
  [[nodiscard]] std::expected<List<std::pair<Drawable, float>>, StarException> drawablesWithZLevel(Vec2F const& translate = Vec2F()) const;

  [[nodiscard]] std::expected<List<LightSource>, StarException> lightSources(Vec2F const& translate = Vec2F()) const;

  // Dynamic target is optional, if not given, generated particles and sounds
  // will be discarded
  [[nodiscard]] std::expected<void, StarException> update(float dt, std::optional<std::reference_wrapper<DynamicTarget>> dynamicTarget);

  // Run through the current animations until the final frame, including any
  // transition animations.
  [[nodiscard]] std::expected<void, StarException> finishAnimations();
  std::uint8_t version() const;

private:
  explicit NetworkedAnimator(Root& root);

  struct RotationGroup {
    float angularVelocity;
    Vec2F rotationCenter;

    NetElementFloat targetAngle;
    float currentAngle = 0;
    float previousAngle = 0;

    NetElementEvent netImmediateEvent;
  };

  struct TransformationGroup {
    Mat3F affineTransform() const;
    void setAffineTransform(Mat3F const& matrix);

    Mat3F localAffineTransform() const;
    void setLocalAffineTransform(Mat3F const& matrix);

    Mat3F animationAffineTransform() const;
    void setAnimationAffineTransform(Mat3F const& matrix);

    bool interpolated;

    Mat3F localTransform;
    mutable Mat3F presentationLocalTransform;
    mutable Mat3F presentationTransform;
    mutable Mat3F presentationAnimationTransform;
    mutable bool presentationAnimationDirty = true;
    mutable bool sampling = false;
    Mat3F previousTransform;
    Mat3F currentTransform;
    Mat3F previousLocalTransform;
    Mat3F currentLocalTransform;
    Mat3F animationBaseTransform = Mat3F::identity();
    bool presentationHistory = false;

    NetElementFloat xTranslation;
    NetElementFloat yTranslation;
    NetElementFloat xScale;
    NetElementFloat yScale;
    NetElementFloat xShear;
    NetElementFloat yShear;

    float xTranslationAnimation;
    float yTranslationAnimation;
    float xScaleAnimation;
    float yScaleAnimation;
    float xShearAnimation;
    float yShearAnimation;

  };

  struct ParticleEmitter {
    struct ParticleConfig {
      ParticleVariantCreator creator;
      unsigned count;
      Vec2F offset;
      bool flip;
    };

    NetElementFloat emissionRate;
    float emissionRateVariance;
    NetElementData<RectF> offsetRegion;
    std::optional<String> anchorPart;
    StringList transformationGroups;
    std::optional<String> rotationGroup;
    std::optional<Vec2F> rotationCenter;

    List<ParticleConfig> particleList;

    NetElementBool active;
    NetElementUInt burstCount;
    NetElementUInt randomSelectCount;
    NetElementEvent burstEvent;

    float timer;
  };

  struct Light {
    NetElementBool active;
    NetElementFloat xPosition;
    NetElementFloat yPosition;
    NetElementData<Color> color;
    NetElementFloat pointAngle;
    std::optional<String> anchorPart;
    StringList transformationGroups;
    std::optional<String> rotationGroup;
    std::optional<Vec2F> rotationCenter;

    std::optional<PeriodicFunction<float>> flicker;
    bool pointLight;
    float pointBeam;
    float beamAmbience;
  };

  enum class SoundSignal {
    Play,
    StopAll
  };

  struct Sound {
    float rangeMultiplier;
    NetElementData<StringList> soundPool;
    NetElementFloat xPosition;
    NetElementFloat yPosition;
    NetElementFloat volumeTarget;
    NetElementFloat volumeRampTime;
    NetElementFloat pitchMultiplierTarget;
    NetElementFloat pitchMultiplierRampTime;
    NetElementInt loops;
    NetElementSignal<SoundSignal> signals;
  };

  struct Effect {
    String type;
    float time;
    Directives directives;

    NetElementBool enabled;
    float timer;
  };

  struct StateInfo {
    NetElementSize stateIndex;
    NetElementEvent startedEvent;
    bool wasUpdated;
    NetElementBool reverse;
  };

  [[nodiscard]] std::expected<void, StarException> setupNetStates();
  [[nodiscard]] std::expected<void, StarException> loadConfig(Json config, String relativePath);

  [[nodiscard]] std::expected<void, StarException> netElementsNeedLoad(bool full) override;
  [[nodiscard]] std::expected<void, StarException> netElementsNeedStore() override;

  [[nodiscard]] std::expected<Json, StarException> mergeIncludes(Root& root, Json config, Json includes, String relativePath);
  [[nodiscard]] std::expected<Mat3F, StarException> animationGroupTransformation(String const& name, TransformationGroup const& group, Mat3F const& base) const;

  std::reference_wrapper<Root> m_root;
  String m_relativePath;
  std::uint8_t m_animatorVersion;

  AnimatedPartSet m_animatedParts;
  StringList m_animationStateTypes;
  OrderedHashMap<String, StateInfo> m_stateInfo;
  OrderedHashMap<String, RotationGroup> m_rotationGroups;
  OrderedHashMap<String, TransformationGroup> m_transformationGroups;
  OrderedHashMap<String, ParticleEmitter> m_particleEmitters;
  OrderedHashMap<String, Light> m_lights;
  OrderedHashMap<String, Sound> m_sounds;
  OrderedHashMap<String, Effect> m_effects;

  NetElementData<Directives> m_processingDirectives;
  NetElementFloat m_zoom;

  NetElementBool m_flipped;
  NetElementFloat m_flippedRelativeCenterLine;

  NetElementFloat m_animationRate;

  NetElementHashMap<String, String> m_globalTags;
  StableStringMap<NetElementHashMap<String, String>> m_partTags;
  HashMap<String, String> m_localTags;

  HashMap<String,List<Drawable>> m_partDrawables;

  mutable StringMap<std::pair<std::size_t, Drawable>> m_cachedPartDrawables;
  mutable unsigned m_sampleDepth = 0;
  mutable float m_interpolation = 1;
};

}

export module star.networked_animator;

export namespace Star {
  using ::Star::NetworkedAnimator;
  using ::Star::NetworkedAnimatorExceptionTag;
  using ::Star::NetworkedAnimatorException;
}

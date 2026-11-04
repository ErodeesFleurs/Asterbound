module;
import star.format;
import std;
import star.json;
import star.data_stream;
import star.game_types;
import star.string;
import star.color;
import star.list;
import star.image_processing;
import star.hash;
import star.string_view;
import star.thread;
import star.directives;
import star.asset_path;
import star.bi_map;
import star.random;
import star.periodic_function;
import star.ordered_map;
import star.vector;
import star.audio;
import star.map;
import star.set;

import star.drawable;
import star.animation;
import star.particle;
import star.animated_part_set;
import star.mixer;
import star.light_source;
import star.networked_animator;

import star.net_element_containers;
import star.net_element_sync_group;

import star.net_compatibility;
import star.array;
namespace Star {
class Root;
class ItemDatabase;

// Required for renderDummy
class ArmorItem;
class HeadArmor;
class ChestArmor;
class LegsArmor;
class BackArmor;

struct Dance;
struct DanceStep;

enum class HumanoidEmote {
  Idle,
  Blabbering,
  Shouting,
  Happy,
  Sad,
  NEUTRAL,
  Laugh,
  Annoyed,
  Oh,
  OOOH,
  Blink,
  Wink,
  Eat,
  Sleep
};
extern std::expected<EnumMap<HumanoidEmote>, StarException> const HumanoidEmoteNames;
inline constexpr std::size_t EmoteSize = 14;

enum class HumanoidHand {
  Idle,
  Blabbering,
  Shouting,
  Happy,
  Sad,
  NEUTRAL,
  Laugh,
  Annoyed,
  Oh,
  OOOH,
  Blink,
  Wink,
  Eat,
  Sleep
};
extern std::expected<EnumMap<HumanoidEmote>, StarException> const HumanoidEmoteNames;

struct Personality {
  String idle = "idle.1";
  String armIdle = "idle.1";
  Vec2F headOffset = Vec2F();
  Vec2F armOffset = Vec2F();
};

[[nodiscard]] std::expected<Personality, StarException> parsePersonalityArray(Json const& config);

[[nodiscard]] std::expected<std::reference_wrapper<Personality>, StarException> parsePersonality(Personality& personality, Json const& config);
[[nodiscard]] std::expected<Personality, StarException> parsePersonality(Json const& config);

Json jsonFromPersonality(Personality const& personality);

struct HumanoidIdentity {
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, HumanoidIdentity& identity);
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, HumanoidIdentity const& identity);
  explicit HumanoidIdentity();
  [[nodiscard]] static std::expected<HumanoidIdentity, StarException> create(Json config);

  [[nodiscard]] std::expected<Json, StarException> toJson() const;

  String name;
  // Must have :idle[1-5], :sit, :duck, :walk[1-8], :run[1-8], :jump[1-4], and
  // :fall[1-4]
  String species;
  Gender gender;

  String hairGroup;
  // Must have :normal and :climb
  String hairType;
  Directives hairDirectives;
  Directives bodyDirectives;
  Directives emoteDirectives;
  String facialHairGroup;
  String facialHairType;
  Directives facialHairDirectives;
  String facialMaskGroup;
  String facialMaskType;
  Directives facialMaskDirectives;

  Personality personality;
  Vec4B color;

  std::optional<String> imagePath;

};

[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, HumanoidIdentity& identity);
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, HumanoidIdentity const& identity);

class Humanoid {
public:
  enum State {
    Idle, // 1 idle frame
    Walk, // 8 walking frames
    Run, // 8 run frames
    Jump, // 4 jump frames
    Fall, // 4 fall frames
    Swim, // 7 swim frames
    SwimIdle, // 2 swim idle frame
    Duck, // 1 ducking frame
    Sit, // 1 sitting frame
    Lay, // 1 laying frame
    STATESIZE
  };
  static std::expected<EnumMap<State>, StarException> const StateNames;

  [[nodiscard]] static std::expected<std::shared_ptr<Humanoid>, StarException> create(Root& root);
  [[nodiscard]] static std::expected<std::shared_ptr<Humanoid>, StarException> create(Root& root, Json const& config);
  [[nodiscard]] static std::expected<std::shared_ptr<Humanoid>, StarException> create(Root& root, HumanoidIdentity const& identity, JsonObject parameters = JsonObject(), Json config = Json(), std::optional<std::reference_wrapper<ItemDatabase const>> itemCatalog = std::nullopt);
  [[nodiscard]] static std::expected<std::shared_ptr<Humanoid>, StarException> create(Humanoid const& humanoid);
  Humanoid& operator=(Humanoid const&) = delete;

  struct HumanoidTiming {
    HumanoidTiming() = default;
    [[nodiscard]] static std::expected<HumanoidTiming, StarException> create(Root& root, Json config = {});

    static bool cyclicState(State state);
    static bool cyclicEmoteState(HumanoidEmote state);

    int stateSeq(float timer, State state) const;
    int emoteStateSeq(float timer, HumanoidEmote state) const;
    int danceSeq(float timer, std::shared_ptr<Dance> dance) const;
    int genericSeq(float timer, float cycle, unsigned frames, bool cyclic) const;

    Array<float, STATESIZE> stateCycle;
    Array<unsigned, STATESIZE> stateFrames;

    Array<float, EmoteSize> emoteCycle;
    Array<unsigned, EmoteSize> emoteFrames;
  };

  [[nodiscard]] std::expected<void, StarException> setIdentity(HumanoidIdentity const& identity);
  HumanoidIdentity const& identity() const;

  [[nodiscard]] std::expected<bool, StarException> loadConfig(Json merger = JsonObject(), bool forceRefresh = false);
  [[nodiscard]] std::expected<void, StarException> loadAnimation();
  [[nodiscard]] std::expected<void, StarException> setHumanoidParameters(JsonObject parameters);

  // All of the image identifiers here are meant to be image *base* names, with
  // a collection of frames specific to each piece.  If an image is set to
  // empty string, it is disabled.
  struct WornAny {
    Directives directives;
    String frameset;
    bool rotateWithHead = false;
    bool bypassNude = false;
    bool fullbright = false;
    HashMap<String, String> animationTags = {};
  };

  // Must have :normal, climb
  struct WornHead : WornAny {
    Directives maskDirectives;
  };
  // Will have :run, :normal, and :duck
  struct WornChest : WornAny {
    String frontSleeveFrameset;
    String backSleeveFrameset;
  };
  // Must have :idle, :duck, :walk[1-8], :run[1-8], :jump[1-4], :fall[1-4]
  struct WornLegs : WornAny {};
  // Must have :idle, :duck, :walk[1-8], :run[1-8], :jump[1-4], :fall[1-4]
  struct WornBack : WornAny {};

  using Wearable = std::variant<std::monostate, WornHead, WornChest, WornLegs, WornBack>;

  struct Fashion {
    // 8 vanilla + 12 extra slots
    Array<Wearable, 20> wearables;
    // below 3 are recalculated when rendering updated wearables, null-terminated
    Array<std::uint8_t, 20> wornHeads;
    // chests and leg layering is interchangeable
    Array<std::uint8_t, 20> wornChestsLegs;
    Array<std::uint8_t, 20> wornBacks;
    bool wornHeadsChanged = true;
    bool wornChestsLegsChanged = true;
    bool wornBacksChanged = true;
    DirectivesGroup helmetMaskDirectivesGroup;
    bool helmetMasksChanged = true;
  };

  template <typename T>
  inline std::optional<std::reference_wrapper<T const>> getLastWearableOfType() const;

  [[nodiscard]] std::expected<void, StarException> removeWearable(std::uint8_t slot);
  [[nodiscard]] std::expected<void, StarException> setWearableFromHead(std::uint8_t slot, HeadArmor const& head, Gender gender);
  [[nodiscard]] std::expected<void, StarException> setWearableFromChest(std::uint8_t slot, ChestArmor const& chest, Gender gender);
  [[nodiscard]] std::expected<void, StarException> setWearableFromLegs(std::uint8_t slot, LegsArmor const& legs, Gender gender);
  [[nodiscard]] std::expected<void, StarException> setWearableFromBack(std::uint8_t slot, BackArmor const& back, Gender gender);
  void refreshWearables(Fashion& fashion);

  // Legacy getters for all of the above, returns last found
  Directives const& headArmorDirectives() const;
  String const& headArmorFrameset() const;
  Directives const& chestArmorDirectives() const;
  String const& chestArmorFrameset() const;
  String const& backSleeveFrameset() const;
  String const& frontSleeveFrameset() const;
  Directives const& legsArmorDirectives() const;
  String const& legsArmorFrameset() const;
  Directives const& backArmorDirectives() const;
  String const& backArmorFrameset() const;

  void setBodyHidden(bool hidden);

  [[nodiscard]] std::expected<void, StarException> setState(State state);
  [[nodiscard]] std::expected<void, StarException> setEmoteState(HumanoidEmote state);
  [[nodiscard]] std::expected<void, StarException> setDance(std::optional<String> const& dance);
  void setFacingDirection(Direction facingDirection);
  [[nodiscard]] std::expected<void, StarException> setMovingBackwards(bool movingBackwards);
  void setHeadRotation(float headRotation);
  void setRotation(float rotation);
  void setScale(Vec2F scale);

  void setVaporTrail(bool enabled);

  State state() const;
  HumanoidEmote emoteState() const;
  std::optional<String> dance() const;
  [[nodiscard]] std::expected<bool, StarException> danceCyclicOrEnded() const;
  Direction facingDirection() const;
  bool movingBackwards() const;

  // If not rotating, then the arms follow normal movement animation.  The
  // angle parameter should be in the range [-pi/2, pi/2] (the facing direction
  // should not be included in the angle).
  void setHandParameters(ToolHand hand, bool holdingItem, float angle, float itemAngle, bool twoHanded,
      bool recoil, bool outsideOfHand);
  void setHandFrameOverrides(ToolHand hand, StringView back, StringView front);
  void setHandDrawables(ToolHand hand, List<Drawable> drawables);
  void setHandNonRotatedDrawables(ToolHand hand, List<Drawable> drawables);
  bool handHoldingItem(ToolHand hand) const;

  // Updates the animation based on whatever the current animation state is,
  // wrapping or clamping animation time as appropriate.
  [[nodiscard]] std::expected<void, StarException> animate(float dt, std::optional<std::reference_wrapper<NetworkedAnimator::DynamicTarget>> dynamicTarget);

  // Applies the current logical pose outside a sample, or the matching sampled
  // pose inside NetworkedAnimator::sample. Does not advance clocks or events.
  [[nodiscard]] std::expected<void, StarException> prepareAnimatorPose(float interpolation = 1.f, bool withItems = true);

  // Reset animation time to 0.0f
  [[nodiscard]] std::expected<void, StarException> resetAnimation();

  // Renders to centered drawables (centered on the normal image center for the
  // player graphics), (in world space, not pixels)
  [[nodiscard]] std::expected<List<Drawable>, StarException> render(bool withItems = true, bool withRotationAndScale = true, float interpolation = 1.f);

  // Renders to centered drawables (centered on the normal image center for the
  // player graphics), (in pixels, not world space)
  [[nodiscard]] std::expected<List<Drawable>, StarException> renderPortrait(PortraitMode mode) const;

  [[nodiscard]] std::expected<List<Drawable>, StarException> renderSkull() const;

  [[nodiscard]] static std::expected<std::shared_ptr<Humanoid>, StarException> makeDummy(Root& root, Gender gender);
  // Renders to centered drawables (centered on the normal image center for the
  // player graphics), (in pixels, not world space)
  [[nodiscard]] std::expected<List<Drawable>, StarException> renderDummy(Gender gender, std::optional<std::reference_wrapper<HeadArmor const>> head = std::nullopt, std::optional<std::reference_wrapper<ChestArmor const>> chest = std::nullopt,
      std::optional<std::reference_wrapper<LegsArmor const>> legs = std::nullopt, std::optional<std::reference_wrapper<BackArmor const>> back = std::nullopt);

  [[nodiscard]] std::expected<Vec2F, StarException> primaryHandPosition(Vec2F const& offset) const;
  [[nodiscard]] std::expected<Vec2F, StarException> altHandPosition(Vec2F const& offset) const;

  // Finds the arm position in world space if the humanoid was facing the given
  // direction and applying the given arm angle.  The offset given is from the
  // rotation center of the arm.
  [[nodiscard]] std::expected<Vec2F, StarException> primaryArmPosition(Direction facingDirection, float armAngle, Vec2F const& offset) const;
  [[nodiscard]] std::expected<Vec2F, StarException> altArmPosition(Direction facingDirection, float armAngle, Vec2F const& offset) const;

  // Gives the offset of the hand from the arm rotation center
  [[nodiscard]] std::expected<Vec2F, StarException> primaryHandOffset(Direction facingDirection) const;
  [[nodiscard]] std::expected<Vec2F, StarException> altHandOffset(Direction facingDirection) const;

  [[nodiscard]] std::expected<Vec2F, StarException> armAdjustment() const;

  [[nodiscard]] std::expected<Vec2F, StarException> mouthOffset(bool ignoreAdjustments = false) const;
  [[nodiscard]] std::expected<float, StarException> getBobYOffset(float interpolation = 1.f) const;
  [[nodiscard]] std::expected<Vec2F, StarException> feetOffset() const;

  [[nodiscard]] std::expected<Vec2F, StarException> headArmorOffset() const;
  [[nodiscard]] std::expected<Vec2F, StarException> chestArmorOffset() const;
  [[nodiscard]] std::expected<Vec2F, StarException> legsArmorOffset() const;
  [[nodiscard]] std::expected<Vec2F, StarException> backArmorOffset() const;

  String defaultDeathParticles() const;
  [[nodiscard]] std::expected<List<Particle>, StarException> particles(String const& name) const;

  Json const& defaultMovementParameters() const;
  std::optional<Json> const& playerMovementParameters() const;

  String getHeadFromIdentity() const;
  String getBodyFromIdentity() const;
  String getBodyMaskFromIdentity() const;
  String getBodyHeadMaskFromIdentity() const;
  String getFacialEmotesFromIdentity() const;
  String getHairFromIdentity() const;
  String getFacialHairFromIdentity() const;
  String getFacialMaskFromIdentity() const;
  String getBackArmFromIdentity() const;
  String getFrontArmFromIdentity() const;
  String getVaporTrailFrameset() const;

  std::optional<std::reference_wrapper<NetworkedAnimator>> networkedAnimator();
  std::optional<std::reference_wrapper<NetworkedAnimator const>> networkedAnimator() const;
  List<String> animationScripts() const;

  [[nodiscard]] std::expected<Json, StarException> humanoidConfig(bool withOverrides = true);

  // Extracts scalenearest from directives and returns the combined scale and
  // a new Directives without those scalenearest directives.
  [[nodiscard]] static std::expected<std::pair<Vec2F, Directives>, StarException> extractScaleFromDirectives(Directives const& directives);

private:
  explicit Humanoid(Root& root);
  Humanoid(Humanoid const&) = default;
  Root& m_root;
  String m_genderName = "male";
  struct HandDrawingInfo {
    List<Drawable> itemDrawables;
    List<Drawable> nonRotatedDrawables;
    bool holdingItem = false;
    float angle = 0.0f;
    float itemAngle = 0.0f;
    String backFrame;
    String frontFrame;
    Directives backDirectives;
    Directives frontDirectives;
    float frameAngleAdjust = 0.0f;
    bool recoil = false;
    bool outsideOfHand = false;
  };

  HandDrawingInfo const& getHand(ToolHand hand) const;

  void wearableRemoved(Wearable const& wearable);

  [[nodiscard]] std::expected<String, StarException> frameBase(State state) const;
  [[nodiscard]] std::expected<String, StarException> emoteFrameBase(HumanoidEmote state) const;

  Directives const& getBodyDirectives() const;
  Directives const& getHairDirectives() const;
  Directives const& getEmoteDirectives() const;
  Directives const& getFacialHairDirectives() const;
  Directives const& getFacialMaskDirectives() const;
  DirectivesGroup const& getHelmetMaskDirectivesGroup() const;

  int getEmoteStateSequence(float interpolation = 1.f) const;
  [[nodiscard]] std::expected<int, StarException> getArmStateSequence(float interpolation = 1.f) const;
  int getBodyStateSequence(float interpolation = 1.f) const;

  [[nodiscard]] std::expected<std::optional<std::shared_ptr<Dance>>, StarException> getDance() const;

  [[nodiscard]] std::expected<void, StarException> refreshAnimationState(bool startNew = false);
  [[nodiscard]] std::expected<void, StarException> applyAnimatorPose(HandDrawingInfo const& frontHand, HandDrawingInfo const& backHand,
      float frontAngle, float backAngle, float headRotation, DanceStep const* danceStep, bool withItems, bool withDrawables);

  Json m_baseConfig;
  Json m_mergeConfig;

  Vec2F m_globalOffset;
  Vec2F m_headRunOffset;
  Vec2F m_headSwimOffset;
  Vec2F m_headDuckOffset;
  Vec2F m_headSitOffset;
  Vec2F m_headLayOffset;
  float m_runFallOffset = 0.0f;
  float m_duckOffset = 0.0f;
  float m_sitOffset = 0.0f;
  float m_layOffset = 0.0f;
  Vec2F m_recoilOffset;
  Vec2F m_mouthOffset;
  Vec2F m_feetOffset;

  Vec2F m_headArmorOffset;
  Vec2F m_chestArmorOffset;
  Vec2F m_legsArmorOffset;
  Vec2F m_backArmorOffset;

  bool m_useBodyMask = false;
  bool m_useBodyHeadMask = false;

  bool m_bodyHidden = false;

  List<int> m_armWalkSeq;
  List<int> m_armRunSeq;
  List<float> m_walkBob;
  List<float> m_runBob;
  List<float> m_swimBob;
  float m_jumpBob = 0.0f;
  Vec2F m_frontArmRotationCenter;
  Vec2F m_backArmRotationCenter;
  Vec2F m_frontHandPosition;
  Vec2F m_backArmOffset;

  Vec2F m_headRotationCenter;

  String m_headFrameset;
  String m_bodyFrameset;
  String m_bodyMaskFrameset;
  String m_bodyHeadMaskFrameset;
  String m_backArmFrameset;
  String m_frontArmFrameset;
  String m_emoteFrameset;
  String m_hairFrameset;
  String m_facialHairFrameset;
  String m_facialMaskFrameset;

  bool m_bodyFullbright = false;

  String m_vaporTrailFrameset;
  unsigned m_vaporTrailFrames = 0;
  float m_vaporTrailCycle = 0.0f;

  std::shared_ptr<Fashion> m_fashion;

  State m_state;
  HumanoidEmote m_emoteState;
  std::optional<String> m_dance;
  Direction m_facingDirection;
  bool m_movingBackwards = false;
  float m_headRotation = 0.0f;
  float m_headRotationTarget = 0.0f;
  float m_rotation = 0.0f;
  Vec2F m_scale;
  bool m_drawVaporTrail = false;

  HandDrawingInfo m_primaryHand;
  HandDrawingInfo m_altHand;

  bool m_twoHanded = false;

  HumanoidIdentity m_identity;
  HumanoidTiming m_timing;

  float m_animationTimer = 0.0f;
  float m_emoteAnimationTimer = 0.0f;
  float m_danceTimer = 0.0f;
  float m_previousAnimationTimer = 0;
  float m_previousEmoteAnimationTimer = 0;
  float m_previousDanceTimer = 0;
  struct PresentationPose {
    float headRotation = 0;
    float rotation = 0;
    Vec2F scale = Vec2F::filled(1.f);
    float primaryAngle = 0;
    float primaryItemAngle = 0;
    float altAngle = 0;
    float altItemAngle = 0;
    bool primaryHolding = false;
    bool altHolding = false;
    Direction facing = Direction::Left;
    bool twoHanded = false;
  };
  PresentationPose m_previousPose;
  PresentationPose m_currentPose;
  bool m_presentationHistory = false;

  Json m_particleEmitters;
  String m_defaultDeathParticles;

  Json m_defaultMovementParameters;
  std::optional<Json> m_playerMovementParameters;
  bool m_useAnimation = false;

  std::shared_ptr<NetworkedAnimator> m_networkedAnimator;

  List<String> m_animationScripts;

  struct AnimationStateArgs {
    String state;
    bool startNew;
    bool reverse;
  };
  HashMap<Humanoid::State, HashMap<String,AnimationStateArgs>> m_animationStates;
  HashMap<Humanoid::State, HashMap<String,AnimationStateArgs>> m_animationStatesBackwards;
  HashMap<HumanoidEmote, HashMap<String,AnimationStateArgs>> m_emoteAnimationStates;
  HashMap<PortraitMode, HashMap<String,AnimationStateArgs>> m_portraitAnimationStates;

  HashMap<String, String> m_identityFramesetTags;

  std::pair<String, String> m_headRotationPoint;
  std::pair<String, String> m_frontArmRotationPoint;
  std::pair<String, String> m_backArmRotationPoint;

  String m_frontItemPart;
  String m_backItemPart;

  std::pair<String, String> m_mouthOffsetPoint;
  std::pair<String, String> m_headArmorOffsetPoint;
  std::pair<String, String> m_chestArmorOffsetPoint;
  std::pair<String, String> m_legsArmorOffsetPoint;
  std::pair<String, String> m_backArmorOffsetPoint;
  std::pair<String, String> m_feetOffsetPoint;
  std::pair<String, String> m_throwPoint;
  std::pair<String, String> m_interactPoint;

};


// this is because species can be changed on the fly and therefore the humanoid needs to re-initialize as the new species when it changes
// therefore we need to have these in a dynamic group in players and NPCs for the sake of the networked animator not breaking the game
class NetHumanoid : public NetElementSyncGroup {
public:
  [[nodiscard]] static std::expected<std::shared_ptr<NetHumanoid>, StarException> create(Root& root);
  [[nodiscard]] static std::expected<std::shared_ptr<NetHumanoid>, StarException> create(Root& root, HumanoidIdentity identity, JsonObject parameters = JsonObject(), Json config = Json());

  [[nodiscard]] std::expected<void, StarException> netStore(DataStream& ds, NetCompatibilityRules rules = {}) const override;
  [[nodiscard]] std::expected<void, StarException> netLoad(DataStream& ds, NetCompatibilityRules rules) override;

  [[nodiscard]] std::expected<void, StarException> netElementsNeedLoad(bool full) override;
  [[nodiscard]] std::expected<void, StarException> netElementsNeedStore() override;

  std::shared_ptr<Humanoid> humanoid();
  [[nodiscard]] std::expected<void, StarException> setHumanoidParameters(JsonObject parameters);
  JsonObject humanoidParameters();

private:
  explicit NetHumanoid(Root& root);
  Root& m_root;
  [[nodiscard]] std::expected<void, StarException> setupNetElements();

  Json m_config;
  NetElementHashMap<String,Json> m_humanoidParameters;
  std::shared_ptr<Humanoid> m_humanoid;
};

template <typename T>
inline std::optional<std::reference_wrapper<T const>> Humanoid::getLastWearableOfType() const {
  for (std::size_t i = m_fashion->wearables.size(); i != 0; --i) {
    auto const& wearable = m_fashion->wearables[i - 1];
    if (std::holds_alternative<T>(wearable))
      return std::cref(std::get<T>(wearable));
  }
  return std::nullopt;
}

}

export module star.humanoid;

export namespace Star {
  using ::Star::ArmorItem;
  using ::Star::HeadArmor;
  using ::Star::ChestArmor;
  using ::Star::LegsArmor;
  using ::Star::BackArmor;
  using ::Star::Humanoid;
  using ::Star::Dance;
  using ::Star::HumanoidEmote;
  using ::Star::HumanoidEmoteNames;
  using ::Star::EmoteSize;
  using ::Star::HumanoidHand;
  using ::Star::Personality;
  using ::Star::parsePersonalityArray;
  using ::Star::parsePersonality;
  using ::Star::jsonFromPersonality;
  using ::Star::HumanoidIdentity;
  using ::Star::NetHumanoid;
}

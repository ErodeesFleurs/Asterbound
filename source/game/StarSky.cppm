module;
import star.format;
import std;
import star.string;
import star.json;
import star.net_compatibility;
import star.byte_array;

import star.either;
import star.vector;
import star.celestial_coordinate;
import star.game_types;
import star.color;
import star.bi_map;
import star.sky_types;
import star.random;
import star.weighted_pool;
import star.list;
import star.image_processing;
import star.hash;
import star.data_stream;
import star.string_view;
import star.thread;
import star.directives;
import star.asset_path;
import star.animation;
import star.particle;
import star.weather_types;
import star.force_regions;
import star.world_parameters;
import star.celestial_parameters;
import star.sky_parameters;
import star.sky_render_data;

import star.net_element_basic_fields;
import star.net_element_float_fields;
import star.net_element_system;

namespace Star {

class Clock;
class AudioInstance;
class Root;

// Sky objects, such as stars and orbiters, are given in a pseudo screen space,
// "view space", that does not take the pixel ratio into account.  "viewSize"
// is the size of this space,  expected to be the size of the screen *after*
// dividing by the pixel ratio.
class Sky {
public:
  [[nodiscard]] static std::expected<std::shared_ptr<Sky>, StarException> create(Root& root);
  [[nodiscard]] static std::expected<std::shared_ptr<Sky>, StarException> create(Root& root, SkyParameters const& skyParameters, bool inOrbit);

  // Controls the space sky "flight" system
  [[nodiscard]] std::expected<void, StarException> startFlying(bool enterHyperspace, bool startInWarp, Json settings = {});
  // Stops flying animation copying the new pertinant sky data from the given
  // sky, as though the sky as moved to a new world.
  [[nodiscard]] std::expected<void, StarException> stopFlyingAt(std::optional<SkyParameters> SkyParameters);

  [[nodiscard]] std::expected<void, StarException> jumpTo(SkyParameters SkyParameters);

  [[nodiscard]] std::expected<std::pair<ByteArray, std::uint64_t>, StarException> writeUpdate(std::uint64_t fromVersion = 0, NetCompatibilityRules rules = {});
  [[nodiscard]] std::expected<void, StarException> readUpdate(ByteArray data, NetCompatibilityRules rules = {});
  
  // handles flying and warp state transitions
  [[nodiscard]] std::expected<void, StarException> stateUpdate();
  // Retain motion endpoints only for a displaying client, not a headless server.
  void beginUpdate();
  [[nodiscard]] std::expected<void, StarException> update(double dt);

  void setType(SkyType type);
  SkyType type() const;

  bool inSpace() const;

  std::uint64_t seed() const;

  float dayLength() const;
  std::uint32_t day() const;
  float timeOfDay() const;

  // Total time since the 0th day for this world.
  double epochTime() const;
  void setEpochTime(double epochTime);

  // Altitude is used to determine, in Atmospheric skies, the percentage of the
  // atmosphere to draw and how much like space it should appear.
  float altitude() const;
  void setAltitude(float altitude);

  // If a reference clock is set, then the epoch time is driven by the
  // reference clock rather than an internal timer
  void setReferenceClock(std::shared_ptr<Clock const> const& referenceClock);
  std::shared_ptr<Clock const> referenceClock() const;

  [[nodiscard]] std::expected<String, StarException> ambientNoise() const;
  [[nodiscard]] std::expected<List<std::shared_ptr<AudioInstance>>, StarException> pullSounds();

  // How close is the atmosphere to space?
  float spaceLevel() const;

  float orbitAngle() const;
  bool isDayTime() const;

  // Ranges from 0.0 to 1.0  Blended periodic curve with a period of
  // clock.dayLength, and the blend region size is determined by
  // the variant asset "dayTransitionTime"
  float dayLevel() const;

  // Returns a value that cycles through the range [0.0, 4.0).  0.0 / 4.0 is
  // mid-morning, 1.0 is mid-day, 2.0 is mid-evening, and 3.0 is mid-night.
  // Does not cycle through evenly, the value will "stick" to mid-day and
  // mid-night based on the value of the variant asset "dayTransitionTime"
  float dayCycle() const;

  float skyAlpha() const;

  Color environmentLight() const;
  Color mainSkyColor() const;

  // Base sky rect colors, top and bottom, includes calculation based on day /
  // night alpha
  std::pair<Color, Color> skyRectColors() const;
  Color skyFlashColor() const;

  bool flying() const;
  FlyingType flyingType() const;
  float warpProgress() const;
  WarpPhase warpPhase() const;
  bool inHyperspace() const;

  // Samples continuous motion only; simulation clocks, lighting and flight events
  // remain tick-owned.
  [[nodiscard]] std::expected<SkyRenderData, StarException> renderData(float interpolation) const;
  void resetPresentation();

private:
  explicit Sky(Root& root);
  // TODO: This needs to be more explicit/handled better
  static constexpr float DefaultDayLength = 1000.0f;

  [[nodiscard]] std::expected<void, StarException> writeNetStates();
  [[nodiscard]] std::expected<void, StarException> readNetStates();

  [[nodiscard]] std::expected<void, StarException> enterHyperspace();
  [[nodiscard]] std::expected<void, StarException> exitHyperspace();
  [[nodiscard]] std::expected<bool, StarException> controlledMovement(JsonArray const& path, Json const& origin, float timeOffset);
  Vec2F getStarOffset() const;
  float getStarRotation() const;
  Vec2F getWorldOffset() const;
  float getWorldRotation() const;
  float speedupTime() const;
  float slowdownTime() const;

  [[nodiscard]] std::expected<void, StarException> skyParametersUpdated();
 
  Root& m_root;
  Json m_settings;
  // Validated on settings changes; frame-by-frame sky math stays nonfallible.
  struct NumericSettings {
    float flyMaxVelocity = 0;
    float starVelocityFactor = 0;
    float correctionPower = 0;
    float flyingTimer = 0;
    float enterHyperspaceAudioLeadIn = 0;
    float exitHyperspaceAudioLeadIn = 0;
    float dayTransitionTime = 0;
    float skyLevelExponent = 0;
    float flashTimer = 0;
    float hyperspaceSpeedupTime = 0;
    float speedupTime = 0;
    float hyperspaceSlowdownTime = 0;
    float slowdownTime = 0;
  } m_numericSettings;
  SkyParameters m_skyParameters;
  bool m_skyParametersUpdated;

  SkyType m_skyType = SkyType::Orbital;

  double m_time = 0.0;

  struct PresentationMotion {
    double time;
    Vec2F starOffset;
    float starRotation;
    Vec2F worldOffset;
    float worldRotation;
  };
  std::optional<PresentationMotion> m_previousMotion;

  std::shared_ptr<Clock const> m_referenceClock;
  std::optional<double> m_clockTrackingTime;

  float m_altitude = 0.0f;

  FlyingType m_flyingType = FlyingType::None;
  FlyingType m_lastFlyingType = FlyingType::None;
  double m_flyingTimer = 0.0;

  bool m_enterHyperspace = false;
  bool m_startInWarp = false;

  WarpPhase m_warpPhase = WarpPhase::Maintain;
  WarpPhase m_lastWarpPhase = WarpPhase::Maintain;

  double m_flashTimer = 0.0;

  // The star and world offsets and rotations must be different for two
  // reasons: #1, the stars rotate over time, meaning that if they're not
  // different then the world will fly off in a random direction when we leave
  // #2, the stars move at a different, slower rate, controlled by JSON
  // "starVelocityFactor", because they're farther away
  Vec2F m_starOffset;
  double m_starRotation = 0.0;
  Vec2F m_starMoveOffset;

  Vec2F m_worldOffset;
  float m_worldRotation = 0.0f;
  Vec2F m_worldMoveOffset;

  // Finally, these are the offsets for the disembark and arrival paths they
  // are applied to BOTH world and star offsets governed by the
  // starVelocityFactor in the latter case

  Vec2F m_pathOffset;
  float m_pathRotation = 0.0f;

  std::size_t m_starFrames = 0;
  StringList m_starList;
  StringList m_hyperStarList;

  bool m_sentSFX = false;

  std::optional<SkyParameters> m_destWorld;

  bool m_netInit;
  NetElementTopGroup m_netGroup;

  NetElementBytes m_skyParametersNetState;
  NetElementInt m_skyTypeNetState;
  NetElementDouble m_timeNetState;
  NetElementUInt m_flyingTypeNetState;
  NetElementBool m_enterHyperspaceNetState;
  NetElementBool m_startInWarpNetState;
  NetElementInt m_warpPhaseNetState;
  NetElementData<Vec2F> m_worldMoveNetState;
  NetElementData<Vec2F> m_starMoveNetState;
  NetElementFloat m_flyingTimerNetState;
};

}

export module star.sky;

export namespace Star {
using ::Star::Clock;
using ::Star::AudioInstance;
using ::Star::Sky;
}

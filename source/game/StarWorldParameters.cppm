module;
import std;
import star.format;
import star.type_identity;
import star.string;
import star.json;
import star.byte_array;

import star.vector;
import star.game_types;
import star.liquid_types;
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

namespace Star {
class Root;

enum class WorldParametersType : std::uint8_t {
  TerrestrialWorldParameters,
  AsteroidsWorldParameters,
  FloatingDungeonWorldParameters
};
extern std::expected<EnumMap<WorldParametersType>, StarException> const WorldParametersTypeNames;

enum class BeamUpRule : std::uint8_t {
  Nowhere,
  Surface,
  Anywhere,
  AnywhereWithWarning
};
extern std::expected<EnumMap<BeamUpRule>, StarException> const BeamUpRuleNames;

enum class WorldEdgeForceRegionType : std::uint8_t {
  None,
  Top,
  Bottom,
  TopAndBottom
};
extern std::expected<EnumMap<WorldEdgeForceRegionType>, StarException> const WorldEdgeForceRegionTypeNames;

struct VisitableWorldParameters {
  virtual void const* query_type(TypeKey key) const noexcept {
    return query_type_of<VisitableWorldParameters>(this, key);
  }
  virtual std::string_view dynamic_type_name() const noexcept { return Star::type_name<VisitableWorldParameters>(); }

  VisitableWorldParameters() = default;
  VisitableWorldParameters(VisitableWorldParameters const& visitableWorldParameters) = default;
  [[nodiscard]] std::expected<void, StarException> load(Json const& store);

  virtual ~VisitableWorldParameters() = default;

  virtual WorldParametersType type() const = 0;

  [[nodiscard]] virtual std::expected<Json, StarException> store() const;

  [[nodiscard]] virtual std::expected<void, StarException> read(DataStream& ds);
  [[nodiscard]] virtual std::expected<void, StarException> write(DataStream& ds) const;

  String typeName;
  float threatLevel{};
  Vec2U worldSize;
  float gravity{};
  bool airless{false};
  WeatherPool weatherPool;
  StringList environmentStatusEffects;
  std::optional<StringList> overrideTech;
  std::optional<List<Directives>> globalDirectives;
  BeamUpRule beamUpRule;
  bool disableDeathDrops{false};
  bool terraformed{false};
  WorldEdgeForceRegionType worldEdgeForceRegions{WorldEdgeForceRegionType::None};
};

struct TerrestrialWorldParameters : VisitableWorldParameters {
  void const* query_type(TypeKey key) const noexcept override {
    return query_type_of<TerrestrialWorldParameters, VisitableWorldParameters>(this, key);
  }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<TerrestrialWorldParameters>(); }

  struct TerrestrialRegion {
    String biome;

    String blockSelector;
    String fgCaveSelector;
    String bgCaveSelector;
    String fgOreSelector;
    String bgOreSelector;
    String subBlockSelector;

    LiquidId caveLiquid{};
    float caveLiquidSeedDensity{};

    LiquidId oceanLiquid{};
    int oceanLiquidLevel{};

    bool encloseLiquids{false};
    bool fillMicrodungeons{false};
  };

  struct TerrestrialLayer {
    int layerMinHeight;
    int layerBaseHeight;

    StringList dungeons;
    int dungeonXVariance;

    TerrestrialRegion primaryRegion;
    TerrestrialRegion primarySubRegion;

    List<TerrestrialRegion> secondaryRegions;
    List<TerrestrialRegion> secondarySubRegions;

    Vec2F secondaryRegionSizeRange;
    Vec2F subRegionSizeRange;
  };

  TerrestrialWorldParameters() = default;
  TerrestrialWorldParameters(TerrestrialWorldParameters const& terrestrialWorldParameters) = default;
  [[nodiscard]] static std::expected<std::shared_ptr<TerrestrialWorldParameters>, StarException> create(Json const& store);

  TerrestrialWorldParameters &operator=(TerrestrialWorldParameters const& terrestrialWorldParameters);

  WorldParametersType type() const override;

  [[nodiscard]] std::expected<Json, StarException> store() const override;

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  String primaryBiome;
  LiquidId primarySurfaceLiquid{};
  String sizeName;
  float hueShift{};

  SkyColoring skyColoring;
  float dayLength{};

  Json blockNoiseConfig;
  Json blendNoiseConfig;
  float blendSize{};

  TerrestrialLayer spaceLayer{};
  TerrestrialLayer atmosphereLayer{};
  TerrestrialLayer surfaceLayer{};
  TerrestrialLayer subsurfaceLayer{};
  List<TerrestrialLayer> undergroundLayers;
  TerrestrialLayer coreLayer{};
};

struct AsteroidsWorldParameters : VisitableWorldParameters {
  void const* query_type(TypeKey key) const noexcept override {
    return query_type_of<AsteroidsWorldParameters, VisitableWorldParameters>(this, key);
  }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<AsteroidsWorldParameters>(); }

  AsteroidsWorldParameters();
  [[nodiscard]] static std::expected<std::shared_ptr<AsteroidsWorldParameters>, StarException> create(Json const& store);

  WorldParametersType type() const override;

  [[nodiscard]] std::expected<Json, StarException> store() const override;

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  int asteroidTopLevel{};
  int asteroidBottomLevel{};
  float blendSize{};
  String asteroidBiome;
  Color ambientLightLevel;
};

struct FloatingDungeonWorldParameters : VisitableWorldParameters {
  void const* query_type(TypeKey key) const noexcept override {
    return query_type_of<FloatingDungeonWorldParameters, VisitableWorldParameters>(this, key);
  }
  std::string_view dynamic_type_name() const noexcept override {
    return Star::type_name<FloatingDungeonWorldParameters>();
  }

  FloatingDungeonWorldParameters() = default;
  [[nodiscard]] static std::expected<std::shared_ptr<FloatingDungeonWorldParameters>, StarException> create(Json const& store);

  WorldParametersType type() const override;

  [[nodiscard]] std::expected<Json, StarException> store() const override;

  [[nodiscard]] std::expected<void, StarException> read(DataStream& ds) override;
  [[nodiscard]] std::expected<void, StarException> write(DataStream& ds) const override;

  int dungeonBaseHeight;
  int dungeonSurfaceHeight;
  int dungeonUndergroundLevel;
  String primaryDungeon;
  Color ambientLightLevel;
  std::optional<String> biome;
  std::optional<String> dayMusicTrack;
  std::optional<String> nightMusicTrack;
  std::optional<String> dayAmbientNoises;
  std::optional<String> nightAmbientNoises;
};

[[nodiscard]] std::expected<Json, StarException> diskStoreVisitableWorldParameters(std::shared_ptr<VisitableWorldParameters const> const& parameters);
[[nodiscard]] std::expected<std::shared_ptr<VisitableWorldParameters>, StarException> diskLoadVisitableWorldParameters(Json const& store);

[[nodiscard]] std::expected<ByteArray, StarException> netStoreVisitableWorldParameters(std::shared_ptr<VisitableWorldParameters const> const& parameters);
[[nodiscard]] std::expected<std::shared_ptr<VisitableWorldParameters>, StarException> netLoadVisitableWorldParameters(ByteArray data);

[[nodiscard]] std::expected<std::shared_ptr<TerrestrialWorldParameters>, StarException> generateTerrestrialWorldParameters(Root& root, String const& typeName, String const& sizeName, std::uint64_t seed);
[[nodiscard]] std::expected<std::shared_ptr<AsteroidsWorldParameters>, StarException> generateAsteroidsWorldParameters(Root& root, std::uint64_t seed);
[[nodiscard]] std::expected<std::shared_ptr<FloatingDungeonWorldParameters>, StarException> generateFloatingDungeonWorldParameters(Root& root, String const& dungeonWorldName);

}

export module star.world_parameters;

export namespace Star {
  using ::Star::WorldParametersType;
  using ::Star::WorldParametersTypeNames;
  using ::Star::BeamUpRule;
  using ::Star::BeamUpRuleNames;
  using ::Star::WorldEdgeForceRegionType;
  using ::Star::WorldEdgeForceRegionTypeNames;
  using ::Star::diskStoreVisitableWorldParameters;
  using ::Star::diskLoadVisitableWorldParameters;
  using ::Star::netStoreVisitableWorldParameters;
  using ::Star::netLoadVisitableWorldParameters;
  using ::Star::generateTerrestrialWorldParameters;
  using ::Star::generateAsteroidsWorldParameters;
  using ::Star::generateFloatingDungeonWorldParameters;
  using ::Star::VisitableWorldParameters;
  using ::Star::TerrestrialWorldParameters;
  using ::Star::AsteroidsWorldParameters;
  using ::Star::FloatingDungeonWorldParameters;
}

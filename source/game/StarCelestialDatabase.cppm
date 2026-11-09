module;
import std;
import star.format;
import star.type_identity;
import star.string;
import star.map;
import star.json;

import star.rect;
import star.ordered_map;
import star.lru_cache;
import star.thread;
import star.time;
import star.random;
import star.ttl_cache;
import star.weighted_pool;
import star.set;
import star.btree;
import star.data_stream_devices;
import star.btree_database;
import star.either;
import star.vector;
import star.celestial_coordinate;
import star.game_types;
import star.color;
import star.bi_map;
import star.sky_types;
import star.list;
import star.image_processing;
import star.hash;
import star.data_stream;
import star.string_view;
import star.directives;
import star.asset_path;
import star.animation;
import star.particle;
import star.weather_types;
import star.force_regions;
import star.world_parameters;
import star.celestial_parameters;
import star.celestial_types;
import star.perlin;

namespace Star {
class Root;
class Assets;
class RandomService;
struct VersionedJson;


class CelestialDatabase {
public:
  virtual void const* queryType(TypeKey key) const noexcept { return queryTypeOf<CelestialDatabase>(this, key); }
  virtual std::string_view dynamicTypeName() const noexcept { return Star::typeName<CelestialDatabase>(); }

  virtual ~CelestialDatabase();

  // The x/y region of usable worlds.
  RectI xyRange() const;

  // The maximum number of bodies that can orbit a single system center /
  // planetary body.  Orbital numbers are up to this number of levels
  // *inclusive*, so planetary orbit numbers would be 1-N, and planetary orbit
  // "0", in this system, would refer to the center of the planetary system
  // itself, e.g. a star.  In the same way, satellites around a planetary
  // object are numbered 1-N, and 0 refers to the planetary object itself.
  int planetOrbitalLevels() const;
  int satelliteOrbitalLevels() const;

  // The following methods are allowed to return no information even in the
  // case of valid coordinates, due to delayed loading.

  [[nodiscard]] virtual std::expected<std::optional<CelestialParameters>, StarException> parameters(CelestialCoordinate const& coordinate) = 0;
  [[nodiscard]] virtual std::expected<std::optional<String>, StarException> name(CelestialCoordinate const& coordinate) = 0;

  [[nodiscard]] virtual std::expected<std::optional<bool>, StarException> hasChildren(CelestialCoordinate const& coordinate) = 0;
  [[nodiscard]] virtual std::expected<List<CelestialCoordinate>, StarException> children(CelestialCoordinate const& coordinate) = 0;
  [[nodiscard]] virtual std::expected<List<int>, StarException> childOrbits(CelestialCoordinate const& coordinate) = 0;

  // Return all valid system coordinates in the given x/y range.  All systems
  // are guaranteed to have unique x/y coordinates, and are meant to be viewed
  // from the top in 2d.  The z-coordinate is there simpy as a validation
  // parameter.
  [[nodiscard]] virtual std::expected<List<CelestialCoordinate>, StarException> scanSystems(RectI const& region, std::optional<StringSet> const& includedTypes = {}) = 0;
  [[nodiscard]] virtual std::expected<List<std::pair<Vec2I, Vec2I>>, StarException> scanConstellationLines(RectI const& region) = 0;

  // Returns false if part or all of the specified region is not loaded.  This
  // is only relevant for calls to scanSystems and scanConstellationLines, and
  // does not imply that each individual system in the given region is fully
  // loaded with all planets moons etc, only that scanSystem and
  // scanConstellationLines are not waiting on missing data.
  [[nodiscard]] virtual std::expected<bool, StarException> scanRegionFullyLoaded(RectI const& region) = 0;

protected:
  Vec2I chunkIndexFor(CelestialCoordinate const& coordinate) const;
  Vec2I chunkIndexFor(Vec2I const& systemXY) const;

  // Returns the chunk indexes for the given region.
  List<Vec2I> chunkIndexesFor(RectI const& region) const;

  // Returns the region of the given chunk.
  RectI chunkRegion(Vec2I const& chunkIndex) const;

  // m_baseInformation should only be modified in the constructor, as it is not
  // thread protected.
  CelestialBaseInformation m_baseInformation;
};

class CelestialMasterDatabase : public CelestialDatabase {
public:
  void const* queryType(TypeKey key) const noexcept override { return queryTypeOf<CelestialMasterDatabase, CelestialDatabase>(this, key); }
  std::string_view dynamicTypeName() const noexcept override { return Star::typeName<CelestialMasterDatabase>(); }

  [[nodiscard]] static std::expected<std::shared_ptr<CelestialMasterDatabase>, StarException> create(Root& root, std::optional<String> databaseFile = {});

  CelestialBaseInformation baseInformation() const;
  [[nodiscard]] std::expected<CelestialResponse, StarException> respondToRequest(CelestialRequest const& requests);

  // Unload data that has not been used in the configured TTL time, and
  // periodically commit to the underlying database if it is in use.
  [[nodiscard]] std::expected<void, StarException> cleanupAndCommit();
  // Commit remaining changes and close persistent storage at server shutdown.
  [[nodiscard]] std::expected<void, StarException> close();

  // Does this coordinate point to a valid existing object?
  [[nodiscard]] std::expected<bool, StarException> coordinateValid(CelestialCoordinate const& coordinate);

  // Find a planetary or satellite object randomly throughout the entire
  // celestial space that satisfies the given parameters.  May fail to find
  // anything, though with the defaults this is vanishingly unlikely.
  [[nodiscard]] std::expected<std::optional<CelestialCoordinate>, StarException> findRandomWorld(unsigned tries = 10, unsigned trySpatialRange = 50,
      std::function<std::expected<bool, StarException>(CelestialCoordinate)> filter = {}, std::optional<std::uint64_t> seed = {});

  // CelestialMasterDatabase always returns actual data, as it does just in
  // time generation.

  [[nodiscard]] std::expected<std::optional<CelestialParameters>, StarException> parameters(CelestialCoordinate const& coordinate) override;
  [[nodiscard]] std::expected<std::optional<String>, StarException> name(CelestialCoordinate const& coordinate) override;

  [[nodiscard]] std::expected<std::optional<bool>, StarException> hasChildren(CelestialCoordinate const& coordinate) override;
  [[nodiscard]] std::expected<List<CelestialCoordinate>, StarException> children(CelestialCoordinate const& coordinate) override;
  [[nodiscard]] std::expected<List<int>, StarException> childOrbits(CelestialCoordinate const& coordinate) override;

  [[nodiscard]] std::expected<List<CelestialCoordinate>, StarException> scanSystems(RectI const& region, std::optional<StringSet> const& includedTypes = {}) override;
  [[nodiscard]] std::expected<List<std::pair<Vec2I, Vec2I>>, StarException> scanConstellationLines(RectI const& region) override;

  [[nodiscard]] std::expected<bool, StarException> scanRegionFullyLoaded(RectI const& region) override;

  // overwrite the celestial parameters for the world at the given celestial coordinate
  [[nodiscard]] std::expected<void, StarException> updateParameters(CelestialCoordinate const& coordinate, CelestialParameters const& parameters);

  [[nodiscard]] std::expected<void, StarException> markPersistent(CelestialCoordinate const& coordinate);

protected:
  struct SatelliteType {
    String typeName;
    Json baseParameters;
    JsonArray variationParameters;
    JsonObject orbitParameters;
  };

  struct PlanetaryType {
    String typeName;
    float satelliteProbability;
    std::size_t maxSatelliteCount;
    Json baseParameters;
    JsonArray variationParameters;
    JsonObject orbitParameters;
  };

  struct SystemType {
    String typeName;
    bool constellationCapable = false;
    Json baseParameters;
    JsonArray variationParameters;
    List<CelestialOrbitRegion> orbitRegions;
  };

  struct GenerationInformation {
    float systemProbability;
    float constellationProbability;
    Vec2U constellationLineCountRange;
    unsigned constellationMaxTries;
    float maximumConstellationLineLength;
    float minimumConstellationLineLength;
    float minimumConstellationMagnitude;
    float minimumConstellationLineCloseness;

    Map<String, SystemType> systemTypes;

    PerlinD systemTypePerlin;
    Json systemTypeBins;

    StringMap<PlanetaryType> planetaryTypes;
    StringMap<SatelliteType> satelliteTypes;

    StringList planetarySuffixes;
    StringList satelliteSuffixes;

    WeightedPool<String> systemPrefixNames;
    WeightedPool<String> systemNames;
    WeightedPool<String> systemSuffixNames;
  };

  static std::optional<CelestialOrbitRegion> orbitRegion(
      List<CelestialOrbitRegion> const& orbitRegions, int planetaryOrbitNumber);

  using UnlockDuringFunction =
    std::function<std::expected<void, StarException>(std::function<std::expected<void, StarException>()>&&)>&&;
  [[nodiscard]] std::expected<std::reference_wrapper<CelestialChunk const>, StarException> getChunk(Vec2I const& chunkLocation, UnlockDuringFunction unlockDuring = {});

  [[nodiscard]] std::expected<CelestialChunk, StarException> produceChunk(Vec2I const& chunkLocation) const;
  [[nodiscard]] std::expected<std::optional<std::pair<CelestialParameters, HashMap<int, CelestialPlanet>>>, StarException> produceSystem(
      RandomSource& random, Vec3I const& location) const;
  [[nodiscard]] std::expected<List<CelestialConstellation>, StarException> produceConstellations(
      RandomSource& random, List<Vec2I> const& constellationCandidates) const;

  Root& m_root;
  GenerationInformation m_generationInformation;
  
  bool m_chunksAlwaysPersistent = false;

  mutable std::recursive_mutex m_mutex;

  HashTtlCache<Vec2I, CelestialChunk> m_chunkCache;
  BTreeSha256Database m_database;
  bool m_closeAttempted = false;

  float m_commitInterval;
  Timer m_commitTimer;
private:
  explicit CelestialMasterDatabase(Root& root);
  [[nodiscard]] std::expected<void, StarException> init(std::optional<String> const& databaseFile);
  [[nodiscard]] std::expected<void, StarException> storeVersionedChunk(Vec2I const& chunkIndex, VersionedJson const& chunk);
};


class CelestialSlaveDatabase : public CelestialDatabase {
public:
  void const* queryType(TypeKey key) const noexcept override { return queryTypeOf<CelestialSlaveDatabase, CelestialDatabase>(this, key); }
  std::string_view dynamicTypeName() const noexcept override { return Star::typeName<CelestialSlaveDatabase>(); }

  [[nodiscard]] static std::expected<std::shared_ptr<CelestialSlaveDatabase>, StarException> create(
      std::shared_ptr<Assets const> const& assets, RandomService& random, CelestialBaseInformation baseInformation);

  // Signal that the given region should be requested from the master database.
  [[nodiscard]] std::expected<void, StarException> signalRegion(RectI const& region);

  // Signal that the given system should be fully requested from the master
  // database.
  [[nodiscard]] std::expected<void, StarException> signalSystem(CelestialCoordinate const& system);

  // There is an internal activity time for chunk requests to live to prevent
  // repeatedly requesting the same set of chunks.
  List<CelestialRequest> pullRequests();
  [[nodiscard]] std::expected<void, StarException> pushResponses(List<CelestialResponse> responses);

  // Unload data that has not been used in the configured TTL time.
  void cleanup();

  [[nodiscard]] std::expected<std::optional<CelestialParameters>, StarException> parameters(CelestialCoordinate const& coordinate) override;
  [[nodiscard]] std::expected<std::optional<String>, StarException> name(CelestialCoordinate const& coordinate) override;

  [[nodiscard]] std::expected<std::optional<bool>, StarException> hasChildren(CelestialCoordinate const& coordinate) override;
  [[nodiscard]] std::expected<List<CelestialCoordinate>, StarException> children(CelestialCoordinate const& coordinate) override;
  [[nodiscard]] std::expected<List<int>, StarException> childOrbits(CelestialCoordinate const& coordinate) override;

  [[nodiscard]] std::expected<List<CelestialCoordinate>, StarException> scanSystems(RectI const& region, std::optional<StringSet> const& includedTypes = {}) override;
  [[nodiscard]] std::expected<List<std::pair<Vec2I, Vec2I>>, StarException> scanConstellationLines(RectI const& region) override;

  [[nodiscard]] std::expected<bool, StarException> scanRegionFullyLoaded(RectI const& region) override;

  void invalidateCacheFor(CelestialCoordinate const& coordinate);

private:
  explicit CelestialSlaveDatabase(RandomService& random);
  float m_requestTimeout;

  mutable std::recursive_mutex m_mutex;
  HashTtlCache<Vec2I, CelestialChunk> m_chunkCache;
  HashMap<Vec2I, Timer> m_pendingChunkRequests;
  HashMap<Vec3I, Timer> m_pendingSystemRequests;
};

}

export module star.celestial_database;

export namespace Star {
  using ::Star::CelestialDatabase;
  using ::Star::CelestialMasterDatabase;
  using ::Star::CelestialSlaveDatabase;
}

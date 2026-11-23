module;
import std;
import star.format;
import star.type_identity;
import star.string;
import star.map;
import star.json;

import star.rect;
import star.thread;
import star.time;
import star.random;
import star.ttl_cache;
import star.weighted_pool;
import star.btree_database;
import star.vector;
import star.celestial_coordinate;
import star.list;
import star.celestial_parameters;
import star.celestial_types;
import star.perlin;

namespace Star {
class Root;
class Assets;
class RandomService;
struct VersionedJson;


template <typename Visitor, typename Value>
concept celestial_visitor = requires(Visitor& visit, Value const& value) {
  { visit(value) } -> std::same_as<std::expected<void, StarException>>;
};

class CelestialDatabase {
public:
  virtual void query_type(TypeQuery& query) const noexcept { query_type_of<CelestialDatabase>(*this, query); }
  virtual std::string_view dynamic_type_name() const noexcept { return Star::type_name<CelestialDatabase>(); }

  virtual ~CelestialDatabase();

  // The x/y region of usable worlds.
  RectI xy_range() const;

  // The maximum number of bodies that can orbit a single system center /
  // planetary body.  Orbital numbers are up to this number of levels
  // *inclusive*, so planetary orbit numbers would be 1-N, and planetary orbit
  // "0", in this system, would refer to the center of the planetary system
  // itself, e.g. a star.  In the same way, satellites around a planetary
  // object are numbered 1-N, and 0 refers to the planetary object itself.
  int planet_orbital_levels() const;
  int satellite_orbital_levels() const;

  // The following methods are allowed to return no information even in the
  // case of valid coordinates, due to delayed loading.

  [[nodiscard]] virtual std::expected<std::optional<CelestialParameters>, StarException> parameters(CelestialCoordinate const& coordinate) = 0;
  [[nodiscard]] virtual std::expected<std::optional<String>, StarException> name(CelestialCoordinate const& coordinate) = 0;

  [[nodiscard]] virtual std::expected<std::optional<bool>, StarException> has_children(CelestialCoordinate const& coordinate) = 0;
  [[nodiscard]] virtual std::expected<List<CelestialCoordinate>, StarException> children(CelestialCoordinate const& coordinate) = 0;
  [[nodiscard]] virtual std::expected<List<int>, StarException> child_orbits(CelestialCoordinate const& coordinate) = 0;

  // Return all valid system coordinates in the given x/y range.  All systems
  // are guaranteed to have unique x/y coordinates, and are meant to be viewed
  // from the top in 2d.  The z-coordinate is there simply as a validation
  // parameter.
  [[nodiscard]] virtual std::expected<List<CelestialCoordinate>, StarException> scan_systems(RectI const& region, std::optional<StringSet> const& included_types = {}) = 0;
  [[nodiscard]] virtual std::expected<List<std::pair<Vec2I, Vec2I>>, StarException> scan_constellation_lines(RectI const& region) = 0;

  // Returns false if part or all of the specified region is not loaded.  This
  // is only relevant for calls to scan_systems and scan_constellation_lines, and
  // does not imply that each individual system in the given region is fully
  // loaded with all planets moons etc, only that scan_systems and
  // scan_constellation_lines are not waiting on missing data.
  [[nodiscard]] virtual std::expected<bool, StarException> scan_region_fully_loaded(RectI const& region) = 0;

protected:
  Vec2I chunk_index_for(CelestialCoordinate const& coordinate) const;
  Vec2I chunk_index_for(Vec2I const& system_xy) const;

  [[nodiscard]] static std::expected<void, StarException> validate_base_information(CelestialBaseInformation const& info);
  static bool contains_system(RectI const& region, Vec2I const& position);

  // Visit in generation order (x first, then y), without allocating an index list.
  template <celestial_visitor<Vec2I> Visitor>
  [[nodiscard]] std::expected<void, StarException> for_each_chunk_index(RectI const& region, Visitor&& visit) const {
    if (region.is_empty())
      return {};
    auto first = chunk_index_for(region.min());
    auto last = chunk_index_for(region.max() - Vec2I(1, 1));
    for (std::int64_t x = first[0]; x <= last[0]; ++x)
      for (std::int64_t y = first[1]; y <= last[1]; ++y)
        if (auto result = visit(Vec2I(static_cast<int>(x), static_cast<int>(y))); !result)
          return result;
    return {};
  }

  // Returns the region of the given chunk.
  [[nodiscard]] std::expected<RectI, StarException> chunk_region(Vec2I const& chunk_index) const;

  // m_base_information should only be modified in the constructor, as it is not
  // thread protected.
  CelestialBaseInformation m_base_information;
};

class CelestialMasterDatabase : public CelestialDatabase {
  class ConstructionKey {
    friend class CelestialMasterDatabase;
    explicit ConstructionKey() = default;
  };

public:
  CelestialMasterDatabase(ConstructionKey, Root& root);
  void query_type(TypeQuery& query) const noexcept override {
    query_type_of<CelestialMasterDatabase, CelestialDatabase>(*this, query);
  }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<CelestialMasterDatabase>(); }

  [[nodiscard]] static std::expected<std::shared_ptr<CelestialMasterDatabase>, StarException> create(Root& root, std::optional<String> database_file = {});

  CelestialBaseInformation base_information() const;
  [[nodiscard]] std::expected<CelestialResponse, StarException> respond_to_request(CelestialRequest const& request);

  // Unload data that has not been used in the configured TTL time, and
  // periodically commit to the underlying database if it is in use.
  [[nodiscard]] std::expected<void, StarException> cleanup_and_commit();
  // Commit remaining changes and close persistent storage at server shutdown.
  [[nodiscard]] std::expected<void, StarException> close();

  // Does this coordinate point to a valid existing object?
  [[nodiscard]] std::expected<bool, StarException> coordinate_valid(CelestialCoordinate const& coordinate);

  // Find a planetary or satellite object randomly throughout the entire
  // celestial space that satisfies the given parameters.  May fail to find
  // anything, though with the defaults this is vanishingly unlikely.
  [[nodiscard]] std::expected<std::optional<CelestialCoordinate>, StarException> find_random_world(unsigned tries = 10, unsigned try_spatial_range = 50,
      std::function<std::expected<bool, StarException>(CelestialCoordinate)> filter = {}, std::optional<std::uint64_t> seed = {});

  // CelestialMasterDatabase always returns actual data, as it does just in
  // time generation.

  [[nodiscard]] std::expected<std::optional<CelestialParameters>, StarException> parameters(CelestialCoordinate const& coordinate) override;
  [[nodiscard]] std::expected<std::optional<String>, StarException> name(CelestialCoordinate const& coordinate) override;

  [[nodiscard]] std::expected<std::optional<bool>, StarException> has_children(CelestialCoordinate const& coordinate) override;
  [[nodiscard]] std::expected<List<CelestialCoordinate>, StarException> children(CelestialCoordinate const& coordinate) override;
  [[nodiscard]] std::expected<List<int>, StarException> child_orbits(CelestialCoordinate const& coordinate) override;

  [[nodiscard]] std::expected<List<CelestialCoordinate>, StarException> scan_systems(RectI const& region, std::optional<StringSet> const& included_types = {}) override;
  [[nodiscard]] std::expected<List<std::pair<Vec2I, Vec2I>>, StarException> scan_constellation_lines(RectI const& region) override;

  [[nodiscard]] std::expected<bool, StarException> scan_region_fully_loaded(RectI const& region) override;

  // overwrite the celestial parameters for the world at the given celestial coordinate
  [[nodiscard]] std::expected<void, StarException> update_parameters(CelestialCoordinate const& coordinate, CelestialParameters const& parameters);

  [[nodiscard]] std::expected<void, StarException> mark_persistent(CelestialCoordinate const& coordinate);

private:
  struct SatelliteType {
    Json base_parameters;
    JsonArray variation_parameters;
    JsonObject orbit_parameters;
  };

  struct PlanetaryType {
    float satellite_probability;
    std::size_t max_satellite_count;
    Json base_parameters;
    JsonArray variation_parameters;
  };

  struct SystemType {
    Json base_parameters;
    JsonArray variation_parameters;
    List<CelestialOrbitRegion> orbit_regions;
  };

  struct GenerationInformation {
    float system_probability;
    float constellation_probability;
    Vec2U constellation_line_count_range;
    unsigned constellation_max_tries;
    float maximum_constellation_line_length;
    float minimum_constellation_line_length;
    float minimum_constellation_magnitude;
    float minimum_constellation_line_closeness;

    Map<String, SystemType> system_types;

    PerlinD system_type_perlin;
    Json system_type_bins;

    StringMap<PlanetaryType> planetary_types;
    StringMap<SatelliteType> satellite_types;

    StringList planetary_suffixes;
    StringList satellite_suffixes;

    WeightedPool<String> system_prefix_names;
    WeightedPool<String> system_names;
    WeightedPool<String> system_suffix_names;
  };

  using PlanetMap = HashMap<int, CelestialPlanet>;
  using SystemRef = std::reference_wrapper<PlanetMap const>;
  using PlanetRef = std::reference_wrapper<CelestialPlanet>;
  using SatelliteRef = std::reference_wrapper<CelestialParameters>;
  // These cache views never outlive the calling operation's lock.
  struct ResolvedCoordinate {
    CelestialChunk& chunk;
    std::variant<SystemRef, PlanetRef, SatelliteRef> body;
  };
  [[nodiscard]] std::expected<std::optional<ResolvedCoordinate>, StarException> resolve_locked(CelestialCoordinate const& coordinate);
  [[nodiscard]] std::expected<ResolvedCoordinate, StarException> require_locked(CelestialCoordinate const& coordinate);
  [[nodiscard]] std::expected<std::reference_wrapper<CelestialParameters const>, StarException> parameters_locked(CelestialCoordinate const& coordinate);
  [[nodiscard]] std::expected<std::reference_wrapper<CelestialChunk>, StarException> get_chunk_locked(Vec2I const& chunk_index,
      std::optional<std::reference_wrapper<std::unique_lock<std::mutex>>> generation_lock = {});
  [[nodiscard]] std::expected<void, StarException> persist_chunk_locked(Vec2I const& chunk_index, CelestialChunk& chunk);
  [[nodiscard]] std::expected<void, StarException> store_versioned_chunk(Vec2I const& chunk_index, VersionedJson const& chunk);
  [[nodiscard]] std::expected<void, StarException> init(std::optional<String> const& database_file);

  [[nodiscard]] std::expected<CelestialChunk, StarException> produce_chunk(Vec2I const& chunk_location) const;
  [[nodiscard]] std::expected<std::optional<std::pair<CelestialParameters, HashMap<int, CelestialPlanet>>>, StarException> produce_system(
      RandomSource& random, Vec3I const& location) const;
  [[nodiscard]] std::expected<List<CelestialConstellation>, StarException> produce_constellations(
      RandomSource& random, List<Vec2I> const& constellation_candidates) const;

  Root& m_root;
  GenerationInformation m_generation_information;
  
  bool m_chunks_always_persistent = false;

  std::mutex m_mutex;

  HashTtlCache<Vec2I, CelestialChunk> m_chunk_cache;
  BTreeSha256Database m_database;
  bool m_close_attempted = false;

  float m_commit_interval;
  Timer m_commit_timer;
};


class CelestialSlaveDatabase : public CelestialDatabase {
  struct ConstructionKey { explicit ConstructionKey() = default; };
public:
  CelestialSlaveDatabase(ConstructionKey, RandomService& random);
  void query_type(TypeQuery& query) const noexcept override {
    query_type_of<CelestialSlaveDatabase, CelestialDatabase>(*this, query);
  }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<CelestialSlaveDatabase>(); }

  [[nodiscard]] static std::expected<std::shared_ptr<CelestialSlaveDatabase>, StarException> create(
      std::shared_ptr<Assets const> const& assets, RandomService& random, CelestialBaseInformation base_information);

  // Signal that the given region should be requested from the master database.
  [[nodiscard]] std::expected<void, StarException> signal_region(RectI const& region);

  // Signal that the given system should be fully requested from the master
  // database.
  [[nodiscard]] std::expected<void, StarException> signal_system(CelestialCoordinate const& system);

  // There is an internal activity time for chunk requests to live to prevent
  // repeatedly requesting the same set of chunks.
  List<CelestialRequest> pull_requests();
  [[nodiscard]] std::expected<void, StarException> push_responses(List<CelestialResponse> responses);

  // Unload data that has not been used in the configured TTL time.
  void cleanup();

  [[nodiscard]] std::expected<std::optional<CelestialParameters>, StarException> parameters(CelestialCoordinate const& coordinate) override;
  [[nodiscard]] std::expected<std::optional<String>, StarException> name(CelestialCoordinate const& coordinate) override;

  [[nodiscard]] std::expected<std::optional<bool>, StarException> has_children(CelestialCoordinate const& coordinate) override;
  [[nodiscard]] std::expected<List<CelestialCoordinate>, StarException> children(CelestialCoordinate const& coordinate) override;
  [[nodiscard]] std::expected<List<int>, StarException> child_orbits(CelestialCoordinate const& coordinate) override;

  [[nodiscard]] std::expected<List<CelestialCoordinate>, StarException> scan_systems(RectI const& region, std::optional<StringSet> const& included_types = {}) override;
  [[nodiscard]] std::expected<List<std::pair<Vec2I, Vec2I>>, StarException> scan_constellation_lines(RectI const& region) override;

  [[nodiscard]] std::expected<bool, StarException> scan_region_fully_loaded(RectI const& region) override;

  void invalidate_cache_for(CelestialCoordinate const& coordinate);

private:
  using SystemPlanets = HashMap<int, CelestialPlanet>;
  // Callers hold m_mutex while using any of these cache borrows.
  [[nodiscard]] std::expected<std::optional<std::reference_wrapper<CelestialChunk const>>, StarException>
  find_or_request_chunk(Vec2I const& index);
  [[nodiscard]] std::expected<std::optional<std::reference_wrapper<SystemPlanets const>>, StarException>
  find_or_request_objects(CelestialCoordinate const& coordinate);
  [[nodiscard]] std::expected<std::optional<std::reference_wrapper<CelestialParameters const>>, StarException>
  find_or_request_parameters(CelestialCoordinate const& coordinate);

  template <celestial_visitor<CelestialChunk> Visitor>
  [[nodiscard]] std::expected<bool, StarException> scan_chunks(RectI const& region, Visitor&& visit) {
    std::unique_lock locker(m_mutex, std::defer_lock);
    lock_with_diagnostics(locker);
    bool fully_loaded = true;
    auto result = for_each_chunk_index(region, [&](Vec2I const& index) -> std::expected<void, StarException> {
      auto chunk = find_or_request_chunk(index);
      if (!chunk)
        return std::unexpected(std::move(chunk.error()));
      if (*chunk)
        return visit((*chunk)->get());
      fully_loaded = false;
      return {};
    });
    if (!result)
      return std::unexpected(std::move(result.error()));
    return fully_loaded;
  }

  float m_request_timeout;

  std::mutex m_mutex;
  HashTtlCache<Vec2I, CelestialChunk> m_chunk_cache;
  HashMap<Vec2I, Timer> m_pending_chunk_requests;
  HashMap<Vec3I, Timer> m_pending_system_requests;
};

}

export module star.celestial_database;

export namespace Star {
  using ::Star::CelestialDatabase;
  using ::Star::CelestialMasterDatabase;
  using ::Star::CelestialSlaveDatabase;
}

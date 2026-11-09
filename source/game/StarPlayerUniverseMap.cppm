module;
import std;
import star.format;
import star.string;
import star.map;
import star.set;
import star.json;
import star.strong_typedef;
import star.data_stream;
import star.vector;
import star.game_types;
import star.color;
import star.bi_map;
import star.random;
import star.weighted_pool;
import star.list;
import star.image_processing;
import star.hash;
import star.string_view;
import star.thread;
import star.directives;
import star.asset_path;
import star.either;


import star.uuid;
import star.celestial_coordinate;
import star.warping;
import star.sky_types;
import star.animation;
import star.particle;
import star.weather_types;
import star.force_regions;
import star.world_parameters;
import star.celestial_parameters;
import star.sky_parameters;
import star.system_world;

namespace Star {

template <typename T>
[[nodiscard]] std::expected<Json, StarException> jsonFromBookmarkTarget(T const& target);

template <typename T>
[[nodiscard]] std::expected<T, StarException> jsonToBookmarkTarget(Json const& json);

// Bookmark<T> requires T to implement jsonToBookmarkTarget<T> and jsonFromBookmarkTarget<T>
// also operator== and operator!=
template<typename T>
struct Bookmark {
  T target;
  String targetName;
  String bookmarkName;
  String icon;

  [[nodiscard]] static std::expected<Bookmark, StarException> fromJson(Json const& json);
  [[nodiscard]] std::expected<Json, StarException> toJson() const;

  bool operator==(Bookmark<T> const& rhs) const;
  bool operator!=(Bookmark<T> const& rhs) const;
  bool operator<(Bookmark<T> const& rhs) const;
};

using OrbitTarget = std::variant<CelestialCoordinate, Uuid>;
using TeleportTarget = std::pair<WorldId, SpawnTarget>;

using OrbitBookmark = Bookmark<OrbitTarget>;
using TeleportBookmark = Bookmark<TeleportTarget>;

class PlayerUniverseMap {
public:
  struct MappedObject {
    String typeName;
    std::optional<CelestialOrbit> orbit;
    JsonObject parameters;
  };

  PlayerUniverseMap();
  // Decodes every saved universe before publishing the reconstructed map.
  [[nodiscard]] static std::expected<PlayerUniverseMap, StarException> create(Json const& json);

  [[nodiscard]] std::expected<Json, StarException> toJson() const;

  // pair of system location and bookmark, not all orbit bookmarks include the system
  [[nodiscard]] std::expected<List<std::pair<Vec3I, OrbitBookmark>>, StarException> orbitBookmarks() const;
  [[nodiscard]] std::expected<bool, StarException> addOrbitBookmark(CelestialCoordinate const& system, OrbitBookmark const& bookmark);
  [[nodiscard]] std::expected<bool, StarException> removeOrbitBookmark(CelestialCoordinate const& system, OrbitBookmark const& bookmark);

  [[nodiscard]] std::expected<List<TeleportBookmark>, StarException> teleportBookmarks() const;
  [[nodiscard]] std::expected<bool, StarException> addTeleportBookmark(TeleportBookmark bookmark);
  [[nodiscard]] std::expected<bool, StarException> removeTeleportBookmark(TeleportBookmark const& bookmark);
  [[nodiscard]] std::expected<void, StarException> invalidateWarpAction(WarpAction const& bookmark);

  [[nodiscard]] std::expected<std::optional<OrbitBookmark>, StarException> worldBookmark(CelestialCoordinate const& world) const;
  [[nodiscard]] std::expected<List<OrbitBookmark>, StarException> systemBookmarks(CelestialCoordinate const& system) const;
  [[nodiscard]] std::expected<List<OrbitBookmark>, StarException> planetBookmarks(CelestialCoordinate const& planet) const;

  [[nodiscard]] std::expected<bool, StarException> isMapped(CelestialCoordinate const& coordinate);
  [[nodiscard]] std::expected<HashMap<Uuid, MappedObject>, StarException> mappedObjects(CelestialCoordinate const& system);

  [[nodiscard]] std::expected<void, StarException> addMappedCoordinate(CelestialCoordinate const& coordinate);
  [[nodiscard]] std::expected<void, StarException> addMappedObject(CelestialCoordinate const& system, Uuid const& uuid, String const& typeName, std::optional<CelestialOrbit> const& orbit = {}, JsonObject parameters = {});
  [[nodiscard]] std::expected<void, StarException> removeMappedObject(CelestialCoordinate const& system, Uuid const& uuid);
  [[nodiscard]] std::expected<void, StarException> filterMappedObjects(CelestialCoordinate const& system, List<Uuid> const& allowed);

  void setServerUuid(std::optional<Uuid> serverUuid);

private:
  struct SystemMap {
    Set<CelestialCoordinate> mappedPlanets;
    HashMap<Uuid, MappedObject> mappedObjects;
    Set<OrbitBookmark> bookmarks;

    [[nodiscard]] static std::expected<SystemMap, StarException> fromJson(Json const& json);
    [[nodiscard]] std::expected<Json, StarException> toJson() const;
  };
  struct UniverseMap {
    HashMap<Vec3I, SystemMap> systems;
    Set<TeleportBookmark> teleportBookmarks;

    [[nodiscard]] static std::expected<UniverseMap, StarException> fromJson(Json const& json);
    [[nodiscard]] std::expected<Json, StarException> toJson() const;
  };

  [[nodiscard]] std::expected<std::reference_wrapper<UniverseMap const>, StarException> universeMap() const;
  [[nodiscard]] std::expected<std::reference_wrapper<UniverseMap>, StarException> universeMap();

  std::optional<Uuid> m_serverUuid;
  HashMap<Uuid, UniverseMap> m_universeMaps;
};

template <typename T>
bool Bookmark<T>::operator==(Bookmark<T> const& rhs) const {
  return target == rhs.target;
}

template <typename T>
bool Bookmark<T>::operator!=(Bookmark<T> const& rhs) const {
  return target != rhs.target;
}

template <typename T>
bool Bookmark<T>::operator<(Bookmark<T> const& rhs) const {
  return target < rhs.target;
}

}

export module star.player_universe_map;

export namespace Star {
  using ::Star::jsonFromBookmarkTarget;
  using ::Star::jsonToBookmarkTarget;
  using ::Star::Bookmark;
  using ::Star::OrbitTarget;
  using ::Star::TeleportTarget;
  using ::Star::OrbitBookmark;
  using ::Star::TeleportBookmark;
  using ::Star::PlayerUniverseMap;
}

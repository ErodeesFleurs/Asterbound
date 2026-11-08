import std;
import star.string;
import star.map;
import star.format;
import star.json;
import star.json_extra;
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
import star.lexical_cast;

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

import star.player_universe_map;

namespace Star {

template<> [[nodiscard]] std::expected<Json, StarException> jsonFromBookmarkTarget<OrbitTarget>(OrbitTarget const& target) {
  if (std::holds_alternative<Uuid>(target))
    return Json(std::get<Uuid>(target).hex());
  return std::get<CelestialCoordinate>(target).toJson();
}

template<> [[nodiscard]] std::expected<OrbitTarget, StarException> jsonToBookmarkTarget<OrbitTarget>(Json const& json) {
  if (json.type() == Json::Type::String) {
    auto text = json.toString();
    if (!text)
      return std::unexpected(std::move(text.error()));
    auto uuid = Uuid::create(*text);
    if (!uuid)
      return std::unexpected(std::move(uuid.error()));
    return OrbitTarget(std::move(*uuid));
  }
  auto coordinate = CelestialCoordinate::create(json);
  if (!coordinate)
    return std::unexpected(std::move(coordinate.error()));
  return OrbitTarget(std::move(*coordinate));
}

template<> [[nodiscard]] std::expected<Json, StarException> jsonFromBookmarkTarget<TeleportTarget>(TeleportTarget const& target) {
  auto world = printWorldId(target.first);
  if (!world)
    return std::unexpected(std::move(world.error()));
  return JsonArray{std::move(*world), spawnTargetToJson(target.second)};
}

template<> [[nodiscard]] std::expected<TeleportTarget, StarException> jsonToBookmarkTarget<TeleportTarget>(Json const& target) {
  auto worldJson = target.get(0);
  if (!worldJson)
    return std::unexpected(std::move(worldJson.error()));
  auto worldText = worldJson->toString();
  if (!worldText)
    return std::unexpected(std::move(worldText.error()));
  auto world = parseWorldId(*worldText);
  if (!world)
    return std::unexpected(std::move(world.error()));
  auto spawnJson = target.get(1);
  if (!spawnJson)
    return std::unexpected(std::move(spawnJson.error()));
  auto spawn = spawnTargetFromJson(*spawnJson);
  if (!spawn)
    return std::unexpected(std::move(spawn.error()));
  return TeleportTarget(std::move(*world), std::move(*spawn));
}

template <typename T>
[[nodiscard]] std::expected<Bookmark<T>, StarException> Bookmark<T>::fromJson(Json const& json) {
  auto targetJson = json.get("target");
  if (!targetJson)
    return std::unexpected(std::move(targetJson.error()));
  auto target = jsonToBookmarkTarget<T>(*targetJson);
  if (!target)
    return std::unexpected(std::move(target.error()));
  auto targetName = json.getString("targetName");
  if (!targetName)
    return std::unexpected(std::move(targetName.error()));
  auto bookmarkName = json.getString("bookmarkName");
  if (!bookmarkName)
    return std::unexpected(std::move(bookmarkName.error()));
  auto icon = json.getString("icon");
  if (!icon)
    return std::unexpected(std::move(icon.error()));
  return Bookmark<T>{std::move(*target), std::move(*targetName), std::move(*bookmarkName), std::move(*icon)};
}

template <typename T>
[[nodiscard]] std::expected<Json, StarException> Bookmark<T>::toJson() const {
  auto storedTarget = jsonFromBookmarkTarget<T>(target);
  if (!storedTarget)
    return std::unexpected(std::move(storedTarget.error()));
  return JsonObject{{"target", std::move(*storedTarget)}, {"targetName", targetName},
      {"bookmarkName", bookmarkName}, {"icon", icon}};
}

PlayerUniverseMap::PlayerUniverseMap() = default;

std::expected<PlayerUniverseMap, StarException> PlayerUniverseMap::create(Json const& json) {
  PlayerUniverseMap result;
  auto maps = json.optObject();
  if (!maps)
    return std::unexpected(std::move(maps.error()));
  if (*maps) {
    for (auto const& pair : **maps) {
      auto uuid = Uuid::create(pair.first);
      if (!uuid)
        return std::unexpected(std::move(uuid.error()));
      auto map = UniverseMap::fromJson(pair.second);
      if (!map)
        return std::unexpected(std::move(map.error()));
      result.m_universeMaps.set(std::move(*uuid), std::move(*map));
    }
  }
  return result;
}

std::expected<Json, StarException> PlayerUniverseMap::toJson() const {
  JsonObject json;
  for (auto const& pair : m_universeMaps) {
    auto map = pair.second.toJson();
    if (!map)
      return std::unexpected(std::move(map.error()));
    json.set(pair.first.hex(), std::move(*map));
  }
  return json;
}

std::expected<List<std::pair<Vec3I, OrbitBookmark>>, StarException> PlayerUniverseMap::orbitBookmarks() const {
  List<std::pair<Vec3I, OrbitBookmark>> bookmarks;
  if (!m_serverUuid)
    return bookmarks;
  auto map = universeMap();
  if (!map)
    return std::unexpected(std::move(map.error()));
  for (auto const& pair : map->get().systems)
    for (auto const& bookmark : pair.second.bookmarks)
      bookmarks.push_back({pair.first, bookmark});
  return bookmarks;
}

std::expected<bool, StarException> PlayerUniverseMap::addOrbitBookmark(CelestialCoordinate const& system, OrbitBookmark const& bookmark) {
  if (system.isNull())
    return std::unexpected(StarException("Cannot add orbit bookmark to null system"));
  auto map = universeMap();
  if (!map)
    return std::unexpected(std::move(map.error()));
  return map->get().systems[system.location()].bookmarks.add(bookmark);
}

std::expected<bool, StarException> PlayerUniverseMap::removeOrbitBookmark(CelestialCoordinate const& system, OrbitBookmark const& bookmark) {
  if (system.isNull())
    return std::unexpected(StarException("Cannot remove orbit bookmark from null system"));
  auto map = universeMap();
  if (!map)
    return std::unexpected(std::move(map.error()));
  return map->get().systems[system.location()].bookmarks.remove(bookmark);
}

std::expected<List<TeleportBookmark>, StarException> PlayerUniverseMap::teleportBookmarks() const {
  auto map = universeMap();
  if (!map)
    return std::unexpected(std::move(map.error()));
  return map->get().teleportBookmarks.values();
}

std::expected<bool, StarException> PlayerUniverseMap::addTeleportBookmark(TeleportBookmark bookmark) {
  auto map = universeMap();
  if (!map)
    return std::unexpected(std::move(map.error()));
  return map->get().teleportBookmarks.add(std::move(bookmark));
}

std::expected<bool, StarException> PlayerUniverseMap::removeTeleportBookmark(TeleportBookmark const& bookmark) {
  auto map = universeMap();
  if (!map)
    return std::unexpected(std::move(map.error()));
  return map->get().teleportBookmarks.remove(bookmark);
}

std::expected<void, StarException> PlayerUniverseMap::invalidateWarpAction(WarpAction const& warpAction) {
  if (std::holds_alternative<WarpToWorld>(warpAction)) {
    auto const& warp = std::get<WarpToWorld>(warpAction);
    auto removed = removeTeleportBookmark({{warp.world, warp.target}, "", "", ""});
    if (!removed)
      return std::unexpected(std::move(removed.error()));
  }
  return {};
}

std::expected<std::optional<OrbitBookmark>, StarException> PlayerUniverseMap::worldBookmark(CelestialCoordinate const& world) const {
  auto map = universeMap();
  if (!map)
    return std::unexpected(std::move(map.error()));
  if (auto system = map->get().systems.find_ref(world.location())) {
    for (auto const& bookmark : system->get().bookmarks)
      if (std::holds_alternative<CelestialCoordinate>(bookmark.target) && std::get<CelestialCoordinate>(bookmark.target) == world)
        return bookmark;
  }
  return std::optional<OrbitBookmark>();
}

std::expected<List<OrbitBookmark>, StarException> PlayerUniverseMap::systemBookmarks(CelestialCoordinate const& system) const {
  auto map = universeMap();
  if (!map)
    return std::unexpected(std::move(map.error()));
  if (auto entry = map->get().systems.find_ref(system.location()))
    return entry->get().bookmarks.values();
  return List<OrbitBookmark>();
}

std::expected<List<OrbitBookmark>, StarException> PlayerUniverseMap::planetBookmarks(CelestialCoordinate const& planet) const {
  auto map = universeMap();
  if (!map)
    return std::unexpected(std::move(map.error()));
  List<OrbitBookmark> bookmarks;
  if (auto system = map->get().systems.find_ref(planet.location())) {
    for (auto const& bookmark : system->get().bookmarks) {
      if (std::holds_alternative<CelestialCoordinate>(bookmark.target)) {
        auto const& coordinate = std::get<CelestialCoordinate>(bookmark.target);
        auto bookmarkPlanet = coordinate.planet();
        if (!bookmarkPlanet)
          return std::unexpected(std::move(bookmarkPlanet.error()));
        auto bookmarkOrbit = bookmarkPlanet->orbitNumber();
        if (!bookmarkOrbit)
          return std::unexpected(std::move(bookmarkOrbit.error()));
        auto targetPlanet = planet.planet();
        if (!targetPlanet)
          return std::unexpected(std::move(targetPlanet.error()));
        auto targetOrbit = targetPlanet->orbitNumber();
        if (!targetOrbit)
          return std::unexpected(std::move(targetOrbit.error()));
        if (*bookmarkOrbit == *targetOrbit)
          bookmarks.push_back(bookmark);
      }
    }
  }
  return bookmarks;
}

std::expected<bool, StarException> PlayerUniverseMap::isMapped(CelestialCoordinate const& coordinate) {
  if (coordinate.isNull())
    return false;
  auto map = universeMap();
  if (!map)
    return std::unexpected(std::move(map.error()));
  if (auto system = map->get().systems.find_ref(coordinate.location())) {
    if (coordinate.isSystem())
      return true;
    auto planet = coordinate.planet();
    if (!planet)
      return std::unexpected(std::move(planet.error()));
    return system->get().mappedPlanets.contains(*planet);
  }
  return false;
}

std::expected<HashMap<Uuid, PlayerUniverseMap::MappedObject>, StarException> PlayerUniverseMap::mappedObjects(CelestialCoordinate const& system) {
  auto map = universeMap();
  if (!map)
    return std::unexpected(std::move(map.error()));
  if (auto entry = map->get().systems.find_ref(system.location()))
    return entry->get().mappedObjects;
  return HashMap<Uuid, MappedObject>();
}

std::expected<void, StarException> PlayerUniverseMap::addMappedCoordinate(CelestialCoordinate const& coordinate) {
  if (coordinate.isNull())
    return {};
  auto map = universeMap();
  if (!map)
    return std::unexpected(std::move(map.error()));
  auto& system = map->get().systems[coordinate.location()];
  if (!coordinate.isSystem()) {
    auto planet = coordinate.planet();
    if (!planet)
      return std::unexpected(std::move(planet.error()));
    system.mappedPlanets.add(*planet);
  }
  return {};
}

std::expected<void, StarException> PlayerUniverseMap::addMappedObject(CelestialCoordinate const& system, Uuid const& uuid,
    String const& typeName, std::optional<CelestialOrbit> const& orbit, JsonObject parameters) {
  auto map = universeMap();
  if (!map)
    return std::unexpected(std::move(map.error()));
  map->get().systems[system.location()].mappedObjects.set(uuid, MappedObject{typeName, orbit, std::move(parameters)});
  return {};
}

std::expected<void, StarException> PlayerUniverseMap::removeMappedObject(CelestialCoordinate const& system, Uuid const& uuid) {
  auto map = universeMap();
  if (!map)
    return std::unexpected(std::move(map.error()));
  if (auto entry = map->get().systems.find_ref(system.location()))
    entry->get().mappedObjects.remove(uuid);
  return {};
}

std::expected<void, StarException> PlayerUniverseMap::filterMappedObjects(CelestialCoordinate const& system, List<Uuid> const& allowed) {
  auto map = universeMap();
  if (!map)
    return std::unexpected(std::move(map.error()));
  if (auto entry = map->get().systems.find_ref(system.location()))
    for (auto const& uuid : entry->get().mappedObjects.keys())
      if (!allowed.contains(uuid))
        entry->get().mappedObjects.remove(uuid);
  return {};
}

void PlayerUniverseMap::setServerUuid(std::optional<Uuid> serverUuid) {
  m_serverUuid = std::move(serverUuid);
  if (m_serverUuid && !m_universeMaps.contains(*m_serverUuid))
    m_universeMaps.set(*m_serverUuid, UniverseMap());
}

std::expected<PlayerUniverseMap::SystemMap, StarException> PlayerUniverseMap::SystemMap::fromJson(Json const& json) {
  SystemMap map;
  auto planets = json.getArray("mappedPlanets");
  if (!planets)
    return std::unexpected(std::move(planets.error()));
  for (auto const& value : *planets) {
    auto coordinate = CelestialCoordinate::create(value);
    if (!coordinate)
      return std::unexpected(std::move(coordinate.error()));
    map.mappedPlanets.add(std::move(*coordinate));
  }
  auto objects = json.getObject("mappedObjects");
  if (!objects)
    return std::unexpected(std::move(objects.error()));
  for (auto const& pair : *objects) {
    auto typeName = pair.second.getString("typeName");
    if (!typeName)
      return std::unexpected(std::move(typeName.error()));
    auto orbitJson = pair.second.get("orbit");
    if (!orbitJson)
      return std::unexpected(std::move(orbitJson.error()));
    auto orbit = jsonToOptional<CelestialOrbit>(*orbitJson, [](Json const& value) { return CelestialOrbit::fromJson(value); });
    if (!orbit)
      return std::unexpected(std::move(orbit.error()));
    auto parameters = pair.second.getObject("parameters", {});
    if (!parameters)
      return std::unexpected(std::move(parameters.error()));
    auto uuid = Uuid::create(pair.first);
    if (!uuid)
      return std::unexpected(std::move(uuid.error()));
    map.mappedObjects.set(std::move(*uuid), MappedObject{std::move(*typeName), std::move(*orbit), std::move(*parameters)});
  }
  auto bookmarks = json.getArray("bookmarks");
  if (!bookmarks)
    return std::unexpected(std::move(bookmarks.error()));
  for (auto const& value : *bookmarks) {
    auto bookmark = OrbitBookmark::fromJson(value);
    if (!bookmark)
      return std::unexpected(std::move(bookmark.error()));
    map.bookmarks.add(std::move(*bookmark));
  }
  return map;
}

std::expected<Json, StarException> PlayerUniverseMap::SystemMap::toJson() const {
  JsonArray planets;
  for (auto const& planet : mappedPlanets)
    planets.push_back(planet.toJson());
  JsonObject objects;
  for (auto const& pair : mappedObjects) {
    auto orbit = jsonFromOptional<CelestialOrbit>(pair.second.orbit, [](CelestialOrbit const& value) { return value.toJson(); });
    if (!orbit)
      return std::unexpected(std::move(orbit.error()));
    objects.set(pair.first.hex(), JsonObject{{"typeName", pair.second.typeName},
        {"orbit", std::move(*orbit)}, {"parameters", pair.second.parameters}});
  }
  JsonArray storedBookmarks;
  for (auto const& bookmark : bookmarks) {
    auto stored = bookmark.toJson();
    if (!stored)
      return std::unexpected(std::move(stored.error()));
    storedBookmarks.push_back(std::move(*stored));
  }
  return JsonObject{{"mappedPlanets", std::move(planets)}, {"mappedObjects", std::move(objects)},
      {"bookmarks", std::move(storedBookmarks)}};
}

std::expected<PlayerUniverseMap::UniverseMap, StarException> PlayerUniverseMap::UniverseMap::fromJson(Json const& json) {
  UniverseMap map;
  auto systems = json.getArray("systems");
  if (!systems)
    return std::unexpected(std::move(systems.error()));
  for (auto const& value : *systems) {
    auto locationJson = value.get(0);
    if (!locationJson)
      return std::unexpected(std::move(locationJson.error()));
    auto location = jsonToVec3I(*locationJson);
    if (!location)
      return std::unexpected(std::move(location.error()));
    auto systemJson = value.get(1);
    if (!systemJson)
      return std::unexpected(std::move(systemJson.error()));
    auto system = SystemMap::fromJson(*systemJson);
    if (!system)
      return std::unexpected(std::move(system.error()));
    map.systems.set(*location, std::move(*system));
  }
  auto bookmarks = json.getArray("teleportBookmarks");
  if (!bookmarks)
    return std::unexpected(std::move(bookmarks.error()));
  for (auto const& value : *bookmarks) {
    auto bookmark = TeleportBookmark::fromJson(value);
    if (!bookmark)
      return std::unexpected(std::move(bookmark.error()));
    map.teleportBookmarks.add(std::move(*bookmark));
  }
  return map;
}

std::expected<Json, StarException> PlayerUniverseMap::UniverseMap::toJson() const {
  JsonArray storedSystems;
  for (auto const& pair : systems) {
    auto system = pair.second.toJson();
    if (!system)
      return std::unexpected(std::move(system.error()));
    storedSystems.push_back(JsonArray{jsonFromVec3I(pair.first), std::move(*system)});
  }
  JsonArray bookmarks;
  for (auto const& bookmark : teleportBookmarks) {
    auto stored = bookmark.toJson();
    if (!stored)
      return std::unexpected(std::move(stored.error()));
    bookmarks.push_back(std::move(*stored));
  }
  return JsonObject{{"systems", std::move(storedSystems)}, {"teleportBookmarks", std::move(bookmarks)}};
}

std::expected<std::reference_wrapper<PlayerUniverseMap::UniverseMap const>, StarException> PlayerUniverseMap::universeMap() const {
  if (!m_serverUuid)
    return std::unexpected(StarException("Cannot get universe map of null server uuid"));
  return m_universeMaps.get(*m_serverUuid);
}

std::expected<std::reference_wrapper<PlayerUniverseMap::UniverseMap>, StarException> PlayerUniverseMap::universeMap() {
  if (!m_serverUuid)
    return std::unexpected(StarException("Cannot get universe map of null server uuid"));
  return std::ref(m_universeMaps[*m_serverUuid]);
}

}

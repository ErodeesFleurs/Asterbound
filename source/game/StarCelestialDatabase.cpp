import star.json_extra;
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
import star.game_types;
import star.color;
import star.bi_map;
import star.list;
import star.image_processing;
import star.hash;
import star.data_stream;
import star.string_view;
import star.directives;
import star.asset_path;
import star.perlin;
import star.lexical_cast;
import star.casting;
import star.file;
import star.data_stream_extra;
import star.io_device;
import star.ref_ptr;
import star.logging;
import star.listener;
import std;
import star.optional;
import star.string;
import star.map;
import star.format;
import star.version;

import star.celestial_coordinate;
import star.sky_types;
import star.animation;
import star.particle;
import star.weather_types;
import star.force_regions;
import star.world_parameters;
import star.celestial_parameters;
import star.celestial_types;
import star.asset_source;
import star.assets;
import star.root_base;
import star.configuration;
import star.root;

import star.celestial_database;
import star.versioning_database;
import star.compression;

import star.static_random;
import star.line;
import star.math_common;
import star.algorithm;
namespace Star {

CelestialDatabase::~CelestialDatabase() = default;

RectI CelestialDatabase::xyRange() const {
  auto range = m_baseInformation.xyCoordRange;
  return RectI(range[0], range[0], range[1], range[1]);
}

int CelestialDatabase::planetOrbitalLevels() const {
  return m_baseInformation.planetOrbitalLevels;
}

int CelestialDatabase::satelliteOrbitalLevels() const {
  return m_baseInformation.satelliteOrbitalLevels;
}

Vec2I CelestialDatabase::chunkIndexFor(CelestialCoordinate const& coordinate) const {
  return chunkIndexFor(coordinate.location().vec2());
}

Vec2I CelestialDatabase::chunkIndexFor(Vec2I const& systemXY) const {
  return {(systemXY[0] - pmod(systemXY[0], m_baseInformation.chunkSize)) / m_baseInformation.chunkSize,
      (systemXY[1] - pmod(systemXY[1], m_baseInformation.chunkSize)) / m_baseInformation.chunkSize};
}

List<Vec2I> CelestialDatabase::chunkIndexesFor(RectI const& region) const {
  if (region.isEmpty())
    return {};

  List<Vec2I> chunkLocations;
  RectI chunkRegion(chunkIndexFor(region.min()), chunkIndexFor(region.max() - Vec2I(1, 1)));
  for (int x = chunkRegion.xMin(); x <= chunkRegion.xMax(); ++x) {
    for (int y = chunkRegion.yMin(); y <= chunkRegion.yMax(); ++y)
      chunkLocations.push_back({x, y});
  }
  return chunkLocations;
}

RectI CelestialDatabase::chunkRegion(Vec2I const& chunkIndex) const {
  return RectI(chunkIndex * m_baseInformation.chunkSize, (chunkIndex + Vec2I(1, 1)) * m_baseInformation.chunkSize);
}

CelestialMasterDatabase::CelestialMasterDatabase(Root& root)
  : m_root(root), m_chunkCache(root.random()), m_database(root.logger()) {}

std::expected<std::shared_ptr<CelestialMasterDatabase>, StarException> CelestialMasterDatabase::create(Root& root, std::optional<String> databaseFile) {
  auto database = std::shared_ptr<CelestialMasterDatabase>(new CelestialMasterDatabase(root));
  auto initialized = database->init(databaseFile);
  if (!initialized)
    return std::unexpected(std::move(initialized.error()));
  return database;
}

std::expected<void, StarException> CelestialMasterDatabase::init(std::optional<String> const& databaseFile) {
  auto assetsResult = m_root.assets();
  if (!assetsResult)
    return std::unexpected(std::move(assetsResult.error()));
  auto const& assets = *assetsResult;
  auto configResult = assets->json("/celestial.config");
  if (!configResult)
    return std::unexpected(assets->withAssetContext(std::move(configResult.error()), "/celestial.config", "Loading celestial configuration"));
  auto const& config = *configResult;
  auto configured = [&]() -> std::expected<void, StarException> {
    {
      auto value = config.getBool("saveAllChunks", false);
      if (!value)
        return std::unexpected(std::move(value.error()));
      m_chunksAlwaysPersistent = std::move(*value);
    }
    {
      auto value = config.getInt("planetOrbitalLevels");
      if (!value)
        return std::unexpected(std::move(value.error()));
      m_baseInformation.planetOrbitalLevels = std::move(*value);
    }
    {
      auto value = config.getInt("satelliteOrbitalLevels");
      if (!value)
        return std::unexpected(std::move(value.error()));
      m_baseInformation.satelliteOrbitalLevels = std::move(*value);
    }
    {
      auto value = config.getInt("chunkSize");
      if (!value)
        return std::unexpected(std::move(value.error()));
      m_baseInformation.chunkSize = std::move(*value);
    }
    {
      auto value = config.get("xyCoordRange").and_then([](Json const& value) { return jsonToVec2I(value); });
      if (!value)
        return std::unexpected(std::move(value.error()));
      m_baseInformation.xyCoordRange = std::move(*value);
    }
    {
      auto value = config.get("zCoordRange").and_then([](Json const& value) { return jsonToVec2I(value); });
      if (!value)
        return std::unexpected(std::move(value.error()));
      m_baseInformation.zCoordRange = std::move(*value);
    }
    {
      auto value = config.getBool("enforceCoordRange", false);
      if (!value)
        return std::unexpected(std::move(value.error()));
      m_baseInformation.enforceCoordRange = std::move(*value);
    }
    {
      auto value = config.getFloat("systemProbability");
      if (!value)
        return std::unexpected(std::move(value.error()));
      m_generationInformation.systemProbability = std::move(*value);
    }
    {
      auto value = config.getFloat("constellationProbability");
      if (!value)
        return std::unexpected(std::move(value.error()));
      m_generationInformation.constellationProbability = std::move(*value);
    }
    {
      auto value = config.get("constellationLineCountRange").and_then([](Json const& value) { return jsonToVec2U(value); });
      if (!value)
        return std::unexpected(std::move(value.error()));
      m_generationInformation.constellationLineCountRange = std::move(*value);
    }
    {
      auto value = config.getUInt("constellationMaxTries");
      if (!value)
        return std::unexpected(std::move(value.error()));
      m_generationInformation.constellationMaxTries = std::move(*value);
    }
    {
      auto value = config.getFloat("maximumConstellationLineLength");
      if (!value)
        return std::unexpected(std::move(value.error()));
      m_generationInformation.maximumConstellationLineLength = std::move(*value);
    }
    {
      auto value = config.getFloat("minimumConstellationLineLength");
      if (!value)
        return std::unexpected(std::move(value.error()));
      m_generationInformation.minimumConstellationLineLength = std::move(*value);
    }
    {
      auto value = config.getFloat("minimumConstellationMagnitude");
      if (!value)
        return std::unexpected(std::move(value.error()));
      m_generationInformation.minimumConstellationMagnitude = std::move(*value);
    }
    {
      auto value = config.getFloat("minimumConstellationLineCloseness");
      if (!value)
        return std::unexpected(std::move(value.error()));
      m_generationInformation.minimumConstellationLineCloseness = std::move(*value);
    }

    // Sorted maps preserve weighted-pool population order and deterministic generation.
    auto systemTypes = config.getObject("systemTypes");
    if (!systemTypes)
      return std::unexpected(std::move(systemTypes.error()));
    for (auto const& pair : Map<String, Json>::from(*systemTypes)) {
      SystemType type;
      type.typeName = pair.first;
      {
        auto value = pair.second.get("baseParameters");
        if (!value)
          return std::unexpected(std::move(value.error()));
        type.baseParameters = std::move(*value);
      }
      {
        auto value = pair.second.getArray("variationParameters", JsonArray());
        if (!value)
          return std::unexpected(std::move(value.error()));
        type.variationParameters = std::move(*value);
      }
      auto regions = pair.second.getArray("orbitRegions", JsonArray());
      if (!regions)
        return std::unexpected(std::move(regions.error()));
      for (auto const& regionJson : *regions) {
        CelestialOrbitRegion region;
        {
          auto value = regionJson.getString("regionName");
          if (!value)
            return std::unexpected(std::move(value.error()));
          region.regionName = std::move(*value);
        }
        {
          auto value = regionJson.get("orbitRange").and_then([](Json const& value) { return jsonToVec2I(value); });
          if (!value)
            return std::unexpected(std::move(value.error()));
          region.orbitRange = std::move(*value);
        }
        {
          auto value = regionJson.getFloat("bodyProbability");
          if (!value)
            return std::unexpected(std::move(value.error()));
          region.bodyProbability = std::move(*value);
        }
        {
          auto value = regionJson.get("planetaryTypes").and_then([](Json const& value) { return jsonToWeightedPool<String>(value); });
          if (!value)
            return std::unexpected(std::move(value.error()));
          region.planetaryTypes = std::move(*value);
        }
        {
          auto value = regionJson.get("satelliteTypes").and_then([](Json const& value) { return jsonToWeightedPool<String>(value); });
          if (!value)
            return std::unexpected(std::move(value.error()));
          region.satelliteTypes = std::move(*value);
        }
        type.orbitRegions.push_back(std::move(region));
      }
      auto added = m_generationInformation.systemTypes.add(pair.first, std::move(type));
      if (!added)
        return std::unexpected(std::move(added.error()));
    }
    {
      auto value = config.get("systemTypePerlin").and_then([](Json const& value) {
        return PerlinD::create(value, static_random_u64("SystemTypePerlin"));
      });
      if (!value)
        return std::unexpected(std::move(value.error()));
      m_generationInformation.systemTypePerlin = std::move(*value);
    }
    {
      auto value = config.get("systemTypeBins");
      if (!value)
        return std::unexpected(std::move(value.error()));
      m_generationInformation.systemTypeBins = std::move(*value);
    }
    auto planetaryTypes = config.getObject("planetaryTypes");
    if (!planetaryTypes)
      return std::unexpected(std::move(planetaryTypes.error()));
    for (auto const& pair : Map<String, Json>::from(*planetaryTypes)) {
      PlanetaryType type;
      type.typeName = pair.first;
      {
        auto value = pair.second.getFloat("satelliteProbability");
        if (!value)
          return std::unexpected(std::move(value.error()));
        type.satelliteProbability = std::move(*value);
      }
      {
        auto value = pair.second.getUInt("maxSatelliteCount", m_baseInformation.satelliteOrbitalLevels);
        if (!value)
          return std::unexpected(std::move(value.error()));
        type.maxSatelliteCount = std::move(*value);
      }
      {
        auto value = pair.second.get("baseParameters");
        if (!value)
          return std::unexpected(std::move(value.error()));
        type.baseParameters = std::move(*value);
      }
      {
        auto value = pair.second.getArray("variationParameters", JsonArray());
        if (!value)
          return std::unexpected(std::move(value.error()));
        type.variationParameters = std::move(*value);
      }
      {
        auto value = pair.second.getObject("orbitParameters", JsonObject());
        if (!value)
          return std::unexpected(std::move(value.error()));
        type.orbitParameters = std::move(*value);
      }
      m_generationInformation.planetaryTypes[type.typeName] = std::move(type);
    }
    auto satelliteTypes = config.getObject("satelliteTypes");
    if (!satelliteTypes)
      return std::unexpected(std::move(satelliteTypes.error()));
    for (auto const& pair : Map<String, Json>::from(*satelliteTypes)) {
      SatelliteType type;
      type.typeName = pair.first;
      {
        auto value = pair.second.get("baseParameters");
        if (!value)
          return std::unexpected(std::move(value.error()));
        type.baseParameters = std::move(*value);
      }
      {
        auto value = pair.second.getArray("variationParameters", JsonArray());
        if (!value)
          return std::unexpected(std::move(value.error()));
        type.variationParameters = std::move(*value);
      }
      {
        auto value = pair.second.getObject("orbitParameters", JsonObject());
        if (!value)
          return std::unexpected(std::move(value.error()));
        type.orbitParameters = std::move(*value);
      }
      m_generationInformation.satelliteTypes[type.typeName] = std::move(type);
    }
    return {};
  }();
  if (!configured)
    return std::unexpected(assets->withAssetContext(std::move(configured.error()), "/celestial.config", "Loading celestial configuration"));
  auto named = [&]() -> std::expected<void, StarException> {
    auto namesResult = assets->json("/celestial/names.config");
    if (!namesResult)
      return std::unexpected(std::move(namesResult.error()));
    auto const& names = *namesResult;
    {
      auto value = names.get("planetarySuffixes").and_then([](Json const& value) { return jsonToStringList(value); });
      if (!value)
        return std::unexpected(std::move(value.error()));
      m_generationInformation.planetarySuffixes = std::move(*value);
    }
    {
      auto value = names.get("satelliteSuffixes").and_then([](Json const& value) { return jsonToStringList(value); });
      if (!value)
        return std::unexpected(std::move(value.error()));
      m_generationInformation.satelliteSuffixes = std::move(*value);
    }
    {
      auto entries = names.getArray("systemPrefixNames");
      if (!entries)
        return std::unexpected(std::move(entries.error()));
      for (auto const& entry : *entries) {
        auto weight = entry.getFloat(0);
        if (!weight)
          return std::unexpected(std::move(weight.error()));
        auto name = entry.getString(1);
        if (!name)
          return std::unexpected(std::move(name.error()));
        m_generationInformation.systemPrefixNames.add(*weight, std::move(*name));
      }
    }
    {
      auto entries = names.getArray("systemNames");
      if (!entries)
        return std::unexpected(std::move(entries.error()));
      for (auto const& entry : *entries) {
        auto weight = entry.getFloat(0);
        if (!weight)
          return std::unexpected(std::move(weight.error()));
        auto name = entry.getString(1);
        if (!name)
          return std::unexpected(std::move(name.error()));
        m_generationInformation.systemNames.add(*weight, std::move(*name));
      }
    }
    {
      auto entries = names.getArray("systemSuffixNames");
      if (!entries)
        return std::unexpected(std::move(entries.error()));
      for (auto const& entry : *entries) {
        auto weight = entry.getFloat(0);
        if (!weight)
          return std::unexpected(std::move(weight.error()));
        auto name = entry.getString(1);
        if (!name)
          return std::unexpected(std::move(name.error()));
        m_generationInformation.systemSuffixNames.add(*weight, std::move(*name));
      }
    }
    return {};
  }();
  if (!named)
    return std::unexpected(assets->withAssetContext(std::move(named.error()), "/celestial/names.config", "Loading celestial names"));
  if (databaseFile) {
    auto identified = m_database.setContentIdentifier("Celestial2");
    if (!identified)
      return std::unexpected(std::move(identified.error()));
    auto file = File::open(*databaseFile, IOMode::ReadWrite);
    if (!file)
      return std::unexpected(std::move(file.error()));
    auto device = m_database.setIODevice(std::move(*file));
    if (!device)
      return std::unexpected(std::move(device.error()));
    auto opened = m_database.open();
    if (!opened)
      return std::unexpected(std::move(opened.error()));
    if (m_database.contentIdentifier() != "Celestial2") {
      auto logged = m_root.logger().error("CelestialMasterDatabase database content identifier is not 'Celestial2', moving out of the way and recreating");
      if (!logged)
        return std::unexpected(std::move(logged.error()));
      auto closed = m_database.close();
      if (!closed)
        return std::unexpected(std::move(closed.error()));
      auto renamed = File::rename(*databaseFile, strf("{}.{}.fail", *databaseFile, Time::millisecondsSinceEpoch()));
      if (!renamed)
        return std::unexpected(std::move(renamed.error()));
      auto replacement = File::open(*databaseFile, IOMode::ReadWrite);
      if (!replacement)
        return std::unexpected(std::move(replacement.error()));
      auto replaced = m_database.setIODevice(std::move(*replacement));
      if (!replaced)
        return std::unexpected(std::move(replaced.error()));
      auto reopened = m_database.open();
      if (!reopened)
        return std::unexpected(std::move(reopened.error()));
    }
    auto manualCommit = m_database.setAutoCommit(false);
    if (!manualCommit)
      return std::unexpected(std::move(manualCommit.error()));
  }
  {
    auto value = config.getFloat("commitInterval");
    if (!value)
      return std::unexpected(assets->withAssetContext(std::move(value.error()), "/celestial.config", "Loading celestial configuration"));
    m_commitInterval = std::move(*value);
  }
  m_commitTimer.restart(m_commitInterval);
  return {};
}

CelestialBaseInformation CelestialMasterDatabase::baseInformation() const {
  return m_baseInformation;
}

std::expected<CelestialResponse, StarException> CelestialMasterDatabase::respondToRequest(CelestialRequest const& request) {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lockWithDiagnostics(locker);
  if (auto location = request.maybeLeft()) {
    auto loaded = getChunk(*location);
    if (!loaded)
      return std::unexpected(std::move(loaded.error()));
    auto chunk = loaded->get();
    // System objects are sent by separate system requests.
    chunk.systemObjects.clear();
    return makeLeft(std::move(chunk));
  } else if (auto location = request.maybeRight()) {
    auto chunk = getChunk(chunkIndexFor(*location));
    if (!chunk)
      return std::unexpected(std::move(chunk.error()));
    auto planets = chunk->get().systemObjects.get(*location);
    if (!planets)
      return std::unexpected(std::move(planets.error()));
    return makeRight(CelestialSystemObjects{*location, planets->get()});
  }
  return CelestialResponse();
}

std::expected<void, StarException> CelestialMasterDatabase::cleanupAndCommit() {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lockWithDiagnostics(locker);
  m_chunkCache.cleanup();
  if (m_database.isOpen() && m_commitTimer.timeUp()) {
    auto committed = m_database.commit();
    if (!committed)
      return std::unexpected(std::move(committed.error()));
    m_commitTimer.restart(m_commitInterval);
  }
  return {};
}

std::expected<void, StarException> CelestialMasterDatabase::close() {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lockWithDiagnostics(locker);
  if (std::exchange(m_closeAttempted, true))
    return {};
  return m_database.close(true);
}

std::expected<bool, StarException> CelestialMasterDatabase::coordinateValid(CelestialCoordinate const& coordinate) {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lockWithDiagnostics(locker);
  if (!coordinate)
    return false;
  auto chunk = getChunk(chunkIndexFor(coordinate));
  if (!chunk)
    return std::unexpected(std::move(chunk.error()));
  auto system = chunk->get().systemObjects.find_ref(coordinate.location());
  if (!system)
    return false;
  if (coordinate.isSystem())
    return true;
  auto planetCoordinate = coordinate.planet();
  if (!planetCoordinate)
    return std::unexpected(std::move(planetCoordinate.error()));
  auto planetaryOrbit = planetCoordinate->orbitNumber();
  if (!planetaryOrbit)
    return std::unexpected(std::move(planetaryOrbit.error()));
  auto planet = system->get().find_ref(*planetaryOrbit);
  if (!planet)
    return false;
  if (coordinate.isPlanetaryBody())
    return true;
  auto orbit = coordinate.orbitNumber();
  if (!orbit)
    return std::unexpected(std::move(orbit.error()));
  return planet->get().satelliteParameters.contains(*orbit);
}

std::expected<std::optional<CelestialCoordinate>, StarException> CelestialMasterDatabase::findRandomWorld(
    unsigned tries, unsigned trySpatialRange, std::function<std::expected<bool, StarException>(CelestialCoordinate)> filter,
    std::optional<std::uint64_t> seed) {
  RandomSource random(m_root.random().randu64());
  if (seed)
    random.init(*seed);
  for (unsigned i = 0; i < tries; ++i) {
    auto range = xyRange();
    auto x = random.randInt(range.xMin(), range.xMax());
    if (!x)
      return std::unexpected(std::move(x.error()));
    auto y = random.randInt(range.yMin(), range.yMax());
    if (!y)
      return std::unexpected(std::move(y.error()));
    auto systems = scanSystems(RectI::withCenter(Vec2I(*x, *y), Vec2I::filled(trySpatialRange)));
    if (!systems)
      return std::unexpected(std::move(systems.error()));
    for (auto const& system : *systems) {
      auto systemHasChildren = hasChildren(system);
      if (!systemHasChildren)
        return std::unexpected(std::move(systemHasChildren.error()));
      if (!systemHasChildren->value_or(false))
        continue;
      auto planets = children(system);
      if (!planets)
        return std::unexpected(std::move(planets.error()));
      auto selected = random.randFrom(*planets);
      if (!selected)
        return std::unexpected(std::move(selected.error()));
      auto world = selected->get();
      auto worldHasChildren = hasChildren(world);
      if (!worldHasChildren)
        return std::unexpected(std::move(worldHasChildren.error()));
      // Preserve the existing 50% satellite choice and random draw order.
      if (worldHasChildren->value_or(false) && random.randb()) {
        auto satellites = children(world);
        if (!satellites)
          return std::unexpected(std::move(satellites.error()));
        auto selectedSatellite = random.randFrom(*satellites);
        if (!selectedSatellite)
          return std::unexpected(std::move(selectedSatellite.error()));
        world = selectedSatellite->get();
      }
      if (filter) {
        auto accepted = filter(world);
        if (!accepted)
          return std::unexpected(std::move(accepted.error()));
        if (!*accepted)
          continue;
      }
      return world;
    }
  }
  return std::optional<CelestialCoordinate>();
}

std::expected<std::optional<CelestialParameters>, StarException> CelestialMasterDatabase::parameters(CelestialCoordinate const& coordinate) {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lockWithDiagnostics(locker);
  auto valid = coordinateValid(coordinate);
  if (!valid)
    return std::unexpected(std::move(valid.error()));
  if (!*valid)
    return std::unexpected(CelestialException("CelestialMasterDatabase::parameters called on invalid coordinate"));
  auto chunk = getChunk(chunkIndexFor(coordinate));
  if (!chunk)
    return std::unexpected(std::move(chunk.error()));
  if (coordinate.isSystem()) {
    auto parameters = chunk->get().systemParameters.get(coordinate.location());
    if (!parameters)
      return std::unexpected(std::move(parameters.error()));
    return parameters->get();
  }
  auto system = chunk->get().systemObjects.get(coordinate.location());
  if (!system)
    return std::unexpected(std::move(system.error()));
  auto planetCoordinate = coordinate.planet();
  if (!planetCoordinate)
    return std::unexpected(std::move(planetCoordinate.error()));
  auto planetaryOrbit = planetCoordinate->orbitNumber();
  if (!planetaryOrbit)
    return std::unexpected(std::move(planetaryOrbit.error()));
  auto planet = system->get().get(*planetaryOrbit);
  if (!planet)
    return std::unexpected(std::move(planet.error()));
  if (coordinate.isPlanetaryBody())
    return planet->get().planetParameters;
  auto orbit = coordinate.orbitNumber();
  if (!orbit)
    return std::unexpected(std::move(orbit.error()));
  auto satellite = planet->get().satelliteParameters.get(*orbit);
  if (!satellite)
    return std::unexpected(std::move(satellite.error()));
  return satellite->get();
}

std::expected<std::optional<String>, StarException> CelestialMasterDatabase::name(CelestialCoordinate const& coordinate) {
  auto value = parameters(coordinate);
  if (!value)
    return std::unexpected(std::move(value.error()));
  if (*value)
    return (**value).name();
  return std::optional<String>();
}

std::expected<std::optional<bool>, StarException> CelestialMasterDatabase::hasChildren(CelestialCoordinate const& coordinate) {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lockWithDiagnostics(locker);
  auto valid = coordinateValid(coordinate);
  if (!valid)
    return std::unexpected(std::move(valid.error()));
  if (!*valid)
    return std::unexpected(CelestialException("CelestialMasterDatabase::hasChildren called on invalid coordinate"));
  auto chunk = getChunk(chunkIndexFor(coordinate));
  if (!chunk)
    return std::unexpected(std::move(chunk.error()));
  auto system = chunk->get().systemObjects.get(coordinate.location());
  if (!system)
    return std::unexpected(std::move(system.error()));
  if (coordinate.isSystem())
    return !system->get().empty();
  if (coordinate.isPlanetaryBody()) {
    auto orbit = coordinate.orbitNumber();
    if (!orbit)
      return std::unexpected(std::move(orbit.error()));
    auto planet = system->get().get(*orbit);
    if (!planet)
      return std::unexpected(std::move(planet.error()));
    return !planet->get().satelliteParameters.empty();
  }
  return false;
}

std::expected<List<CelestialCoordinate>, StarException> CelestialMasterDatabase::children(CelestialCoordinate const& coordinate) {
  auto orbits = childOrbits(coordinate);
  if (!orbits)
    return std::unexpected(std::move(orbits.error()));
  List<CelestialCoordinate> children;
  for (auto orbit : *orbits) {
    auto child = coordinate.child(orbit);
    if (!child)
      return std::unexpected(std::move(child.error()));
    children.push_back(std::move(*child));
  }
  return children;
}

std::expected<List<int>, StarException> CelestialMasterDatabase::childOrbits(CelestialCoordinate const& coordinate) {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lockWithDiagnostics(locker);
  auto valid = coordinateValid(coordinate);
  if (!valid)
    return std::unexpected(std::move(valid.error()));
  if (!*valid)
    return std::unexpected(CelestialException("CelestialMasterDatabase::childOrbits called on invalid coordinate"));
  auto chunk = getChunk(chunkIndexFor(coordinate));
  if (!chunk)
    return std::unexpected(std::move(chunk.error()));
  auto system = chunk->get().systemObjects.get(coordinate.location());
  if (!system)
    return std::unexpected(std::move(system.error()));
  if (coordinate.isSystem())
    return system->get().keys();
  if (coordinate.isPlanetaryBody()) {
    auto orbit = coordinate.orbitNumber();
    if (!orbit)
      return std::unexpected(std::move(orbit.error()));
    auto planet = system->get().get(*orbit);
    if (!planet)
      return std::unexpected(std::move(planet.error()));
    return planet->get().satelliteParameters.keys();
  }
  return std::unexpected(CelestialException("CelestialMasterDatabase::childOrbits called on improper type!"));
}

std::expected<List<CelestialCoordinate>, StarException> CelestialMasterDatabase::scanSystems(
    RectI const& region, std::optional<StringSet> const& includedTypes) {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lockWithDiagnostics(locker);
  List<CelestialCoordinate> systems;
  for (auto const& location : chunkIndexesFor(region)) {
    auto chunk = getChunk(location, [&](std::function<std::expected<void, StarException>()>&& produce) -> std::expected<void, StarException> {
      locker.unlock();
      auto result = produce();
      lockWithDiagnostics(locker);
      return result;
    });
    if (!chunk)
      return std::unexpected(std::move(chunk.error()));
    for (auto const& pair : chunk->get().systemParameters) {
      if (region.contains(pair.first.vec2())) {
        if (includedTypes) {
          auto type = pair.second.getParameter("typeName", "").and_then([](Json const& value) { return value.toString(); });
          if (!type)
            return std::unexpected(std::move(type.error()));
          if (!includedTypes->contains(*type))
            continue;
        }
        systems.push_back(CelestialCoordinate(pair.first));
      }
    }
  }
  return systems;
}

std::expected<List<std::pair<Vec2I, Vec2I>>, StarException> CelestialMasterDatabase::scanConstellationLines(RectI const& region) {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lockWithDiagnostics(locker);
  List<std::pair<Vec2I, Vec2I>> lines;
  for (auto const& location : chunkIndexesFor(region)) {
    auto chunk = getChunk(location);
    if (!chunk)
      return std::unexpected(std::move(chunk.error()));
    for (auto const& constellation : chunk->get().constellations)
      for (auto const& line : constellation)
        if (region.intersects(Line2I(line.first, line.second)))
          lines.push_back(line);
  }
  return lines;
}

std::expected<bool, StarException> CelestialMasterDatabase::scanRegionFullyLoaded(RectI const&) {
  return true;
}

std::expected<void, StarException> CelestialMasterDatabase::updateParameters(
    CelestialCoordinate const& coordinate, CelestialParameters const& parameters) {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lockWithDiagnostics(locker);
  auto valid = coordinateValid(coordinate);
  if (!valid)
    return std::unexpected(std::move(valid.error()));
  if (!*valid)
    return std::unexpected(CelestialException("CelestialMasterDatabase::updateParameters called on invalid coordinate"));
  auto index = chunkIndexFor(coordinate);
  auto loaded = getChunk(index);
  if (!loaded)
    return std::unexpected(std::move(loaded.error()));
  auto chunk = loaded->get();
  bool updated = false;
  if (coordinate.isSatelliteBody() || coordinate.isPlanetaryBody()) {
    auto system = chunk.systemObjects.get(coordinate.location());
    if (!system)
      return std::unexpected(std::move(system.error()));
    auto planetCoordinate = coordinate.planet();
    if (!planetCoordinate)
      return std::unexpected(std::move(planetCoordinate.error()));
    auto planetOrbit = planetCoordinate->orbitNumber();
    if (!planetOrbit)
      return std::unexpected(std::move(planetOrbit.error()));
    auto planet = system->get().get(*planetOrbit);
    if (!planet)
      return std::unexpected(std::move(planet.error()));
    if (coordinate.isSatelliteBody()) {
      auto orbit = coordinate.orbitNumber();
      if (!orbit)
        return std::unexpected(std::move(orbit.error()));
      planet->get().satelliteParameters.set(*orbit, parameters);
    } else {
      planet->get().planetParameters = parameters;
    }
    updated = true;
  }
  if (!updated || !m_database.isOpen())
    return std::unexpected(CelestialException("CelestialMasterDatabase::updateParameters failed; coordinate is not a valid planet or satellite, or celestial database was not open for writing"));
  auto versioning = m_root.versioningDatabase();
  if (!versioning)
    return std::unexpected(std::move(versioning.error()));
  auto versioned = chunk.toJson().and_then([&](Json const& value) {
    return (*versioning)->makeCurrentVersionedJson("CelestialChunk", value);
  });
  if (!versioned)
    return std::unexpected(std::move(versioned.error()));
  auto stored = storeVersionedChunk(index, *versioned);
  if (!stored)
    return std::unexpected(std::move(stored.error()));
  m_chunkCache.remove(index);
  return {};
}

std::optional<CelestialOrbitRegion> CelestialMasterDatabase::orbitRegion(
    List<CelestialOrbitRegion> const& orbitRegions, int planetaryOrbitNumber) {
  for (auto const& region : orbitRegions) {
    if (planetaryOrbitNumber >= region.orbitRange[0] && planetaryOrbitNumber <= region.orbitRange[1])
      return region;
  }
  return {};
}

std::expected<void, StarException> CelestialMasterDatabase::markPersistent(CelestialCoordinate const& coordinate) {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lockWithDiagnostics(locker);
  auto index = chunkIndexFor(coordinate);
  auto loaded = getChunk(index);
  if (!loaded)
    return std::unexpected(std::move(loaded.error()));
  if (loaded->get().persistent)
    return {};
  if (m_database.isOpen()) {
    auto versioning = m_root.versioningDatabase();
    if (!versioning)
      return std::unexpected(std::move(versioning.error()));
    auto versioned = loaded->get().toJson().and_then([&](Json const& value) {
      return (*versioning)->makeCurrentVersionedJson("CelestialChunk", value);
    });
    if (!versioned)
      return std::unexpected(std::move(versioned.error()));
    auto stored = storeVersionedChunk(index, *versioned);
    if (!stored)
      return std::unexpected(std::move(stored.error()));
  }
  return {};
}

std::expected<void, StarException> CelestialMasterDatabase::storeVersionedChunk(
    Vec2I const& chunkIndex, VersionedJson const& chunk) {
  DataStreamBuffer stream;
  auto written = stream.write(chunk);
  if (!written)
    return std::unexpected(std::move(written.error()));
  auto subVersioning = VersionedJson::writeSubVersioning(stream, chunk);
  if (!subVersioning)
    return std::unexpected(std::move(subVersioning.error()));
  auto key = DataStreamBuffer::serialize(chunkIndex);
  if (!key)
    return std::unexpected(std::move(key.error()));
  auto compressed = compressData(stream.data());
  if (!compressed)
    return std::unexpected(std::move(compressed.error()));
  auto inserted = m_database.insert(*key, *compressed);
  if (!inserted)
    return std::unexpected(std::move(inserted.error()));
  return {};
}

std::expected<std::reference_wrapper<CelestialChunk const>, StarException> CelestialMasterDatabase::getChunk(
    Vec2I const& chunkIndex, UnlockDuringFunction unlockDuring) {
  auto cached = m_chunkCache.get(chunkIndex, [&, &database = *this](Vec2I const& index) -> std::expected<CelestialChunk, StarException> {
    auto versioning = database.m_root.versioningDatabase();
    if (!versioning)
      return std::unexpected(std::move(versioning.error()));
    if (database.m_database.isOpen()) {
      auto key = DataStreamBuffer::serialize(index);
      if (!key)
        return std::unexpected(std::move(key.error()));
      auto data = database.m_database.find(*key);
      if (!data)
        return std::unexpected(std::move(data.error()));
      if (*data) {
        auto decompressed = uncompressData(**data);
        if (!decompressed)
          return std::unexpected(std::move(decompressed.error()));
        auto versioned = DataStreamBuffer::deserialize<VersionedJson>(*decompressed);
        if (!versioned)
          return std::unexpected(std::move(versioned.error()));
        auto current = (*versioning)->versionedJsonCurrent(*versioned);
        if (!current)
          return std::unexpected(std::move(current.error()));
        if (!*current) {
          auto upgraded = (*versioning)->updateVersionedJson(*versioned);
          if (!upgraded)
            return std::unexpected(std::move(upgraded.error()));
          versioned = std::move(upgraded);
          auto stored = database.storeVersionedChunk(index, *versioned);
          if (!stored)
            return std::unexpected(std::move(stored.error()));
        }
        return CelestialChunk::create(versioned->content);
      }
    }
    CelestialChunk chunk;
    auto producer = [&]() -> std::expected<void, StarException> {
      auto produced = database.produceChunk(index);
      if (!produced)
        return std::unexpected(std::move(produced.error()));
      chunk = std::move(*produced);
      return {};
    };
    auto produced = unlockDuring ? unlockDuring(producer) : producer();
    if (!produced)
      return std::unexpected(std::move(produced.error()));
    if (database.m_chunksAlwaysPersistent && database.m_database.isOpen()) {
      auto versioned = chunk.toJson().and_then([&](Json const& value) {
        return (*versioning)->makeCurrentVersionedJson("CelestialChunk", value);
      });
      if (!versioned)
        return std::unexpected(std::move(versioned.error()));
      auto stored = database.storeVersionedChunk(index, *versioned);
      if (!stored)
        return std::unexpected(std::move(stored.error()));
      chunk.persistent = true;
    }
    return chunk;
  });
  if (!cached)
    return std::unexpected(std::move(cached.error()));
  return std::cref(cached->get());
}

std::expected<CelestialChunk, StarException> CelestialMasterDatabase::produceChunk(Vec2I const& chunkIndex) const {
  CelestialChunk chunk;
  chunk.chunkIndex = chunkIndex;
  RectI region = chunkRegion(chunkIndex);
  if (m_baseInformation.enforceCoordRange && !xyRange().contains(region, true))
    return chunk;
  RandomSource random(static_random_u64(chunkIndex[0], chunkIndex[1], "ChunkIndexMix"));
  List<Vec3I> locations;
  for (int x = region.xMin(); x < region.xMax(); ++x) {
    for (int y = region.yMin(); y < region.yMax(); ++y) {
      if (random.randf() < m_generationInformation.systemProbability) {
        auto z = random.randi32() % (m_baseInformation.zCoordRange[1] - m_baseInformation.zCoordRange[0])
            + m_baseInformation.zCoordRange[0];
        locations.push_back(Vec3I(x, y, z));
      }
    }
  }
  List<Vec2I> candidates;
  for (auto const& location : locations) {
    auto information = produceSystem(random, location);
    if (!information)
      return std::unexpected(std::move(information.error()));
    if (*information) {
      auto& system = **information;
      auto magnitude = system.first.getParameter("magnitude").and_then([](Json const& value) { return value.toFloat(); });
      if (!magnitude)
        return std::unexpected(std::move(magnitude.error()));
      chunk.systemParameters[location] = std::move(system.first);
      chunk.systemObjects[location] = std::move(system.second);
      if (*magnitude >= m_generationInformation.minimumConstellationMagnitude)
        candidates.push_back(location.vec2());
    }
  }
  auto constellations = produceConstellations(random, candidates);
  if (!constellations)
    return std::unexpected(std::move(constellations.error()));
  chunk.constellations = std::move(*constellations);
  return chunk;
}

std::expected<std::optional<std::pair<CelestialParameters, HashMap<int, CelestialPlanet>>>, StarException>
CelestialMasterDatabase::produceSystem(RandomSource& random, Vec3I const& location) const {
  auto selector = m_generationInformation.systemTypePerlin.get(location[0], location[1]);
  if (!selector)
    return std::unexpected(std::move(selector.error()));
  float typeSelector = *selector;
  auto typeName = binnedChoiceFromJson(m_generationInformation.systemTypeBins, typeSelector, "")
      .and_then([](Json const& value) { return value.toString(); });
  if (!typeName)
    return std::unexpected(std::move(typeName.error()));
  if (typeName->empty())
    return std::optional<std::pair<CelestialParameters, HashMap<int, CelestialPlanet>>>();
  auto typeResult = m_generationInformation.systemTypes.get(*typeName);
  if (!typeResult)
    return std::unexpected(std::move(typeResult.error()));
  auto const& type = typeResult->get();
  CelestialCoordinate systemCoordinate(location);
  std::uint64_t systemSeed = random.randu64();
  String prefix = m_generationInformation.systemPrefixNames.select(random);
  String mid = m_generationInformation.systemNames.select(random);
  String suffix = m_generationInformation.systemSuffixNames.select(random);
  auto systemName = String(strf("{} {} {}", prefix, mid, suffix)).trim();
  if (!systemName)
    return std::unexpected(std::move(systemName.error()));
  auto oneDigit = systemName->replace("<onedigit>", strf("{:01d}", random.randu32() % 10));
  if (!oneDigit)
    return std::unexpected(std::move(oneDigit.error()));
  auto twoDigits = oneDigit->replace("<twodigit>", strf("{:02d}", random.randu32() % 100));
  if (!twoDigits)
    return std::unexpected(std::move(twoDigits.error()));
  auto threeDigits = twoDigits->replace("<threedigit>", strf("{:03d}", random.randu32() % 1000));
  if (!threeDigits)
    return std::unexpected(std::move(threeDigits.error()));
  auto fourDigits = threeDigits->replace("<fourdigit>", strf("{:04d}", random.randu32() % 10000));
  if (!fourDigits)
    return std::unexpected(std::move(fourDigits.error()));
  systemName = std::move(fourDigits);
  auto systemParameters = CelestialParameters::create(m_root, systemCoordinate, systemSeed, *systemName,
      jsonMerge(type.baseParameters, random.randValueFrom(type.variationParameters)));
  if (!systemParameters)
    return std::unexpected(std::move(systemParameters.error()));
  List<int> planetaryOrbits;
  for (int i = 1; i <= m_baseInformation.planetOrbitalLevels; ++i)
    if (auto region = orbitRegion(type.orbitRegions, i))
      if (random.randf() <= region->bodyProbability)
        planetaryOrbits.push_back(i);
  HashMap<int, CelestialPlanet> objects;
  for (auto [planetIndex, planetOrbit] : std::views::enumerate(planetaryOrbits)) {
    auto region = orbitRegion(type.orbitRegions, planetOrbit);
    auto planetaryTypeName = region->planetaryTypes.select(random);
    if (auto planetType = m_generationInformation.planetaryTypes.find_ref(planetaryTypeName)) {
      auto planetaryParameters = jsonMerge(planetType->get().baseParameters, random.randValueFrom(planetType->get().variationParameters));
      CelestialCoordinate planetCoordinate(location, planetOrbit);
      std::uint64_t planetSeed = random.randu64();
      auto planetarySuffix = m_generationInformation.planetarySuffixes.at(static_cast<std::size_t>(planetIndex));
      if (!planetarySuffix)
        return std::unexpected(std::move(planetarySuffix.error()));
      String planetName = strf("{} {}", *systemName, planetarySuffix->get());
      auto parameters = CelestialParameters::create(m_root, planetCoordinate, planetSeed, planetName, planetaryParameters);
      if (!parameters)
        return std::unexpected(std::move(parameters.error()));
      CelestialPlanet planet;
      planet.planetParameters = std::move(*parameters);
      List<int> satelliteOrbits;
      for (int i = 1; i <= m_baseInformation.satelliteOrbitalLevels; ++i)
        if (satelliteOrbits.size() < planetType->get().maxSatelliteCount && random.randf() < planetType->get().satelliteProbability)
          satelliteOrbits.push_back(i);
      for (auto [satelliteIndex, satelliteOrbit] : std::views::enumerate(satelliteOrbits)) {
        auto satelliteTypeName = region->satelliteTypes.select(random);
        if (auto satelliteType = m_generationInformation.satelliteTypes.find_ref(satelliteTypeName)) {
          auto variation = random.randValueFrom(satelliteType->get().variationParameters);
          auto orbitParameters = satelliteType->get().orbitParameters.value(region->regionName, JsonArray()).toArray();
          if (!orbitParameters)
            return std::unexpected(std::move(orbitParameters.error()));
          auto satelliteParameters = jsonMerge(satelliteType->get().baseParameters, variation, random.randValueFrom(*orbitParameters));
          CelestialCoordinate satelliteCoordinate(location, planetOrbit, satelliteOrbit);
          std::uint64_t satelliteSeed = random.randu64();
          auto satelliteSuffix = m_generationInformation.satelliteSuffixes.at(static_cast<std::size_t>(satelliteIndex));
          if (!satelliteSuffix)
            return std::unexpected(std::move(satelliteSuffix.error()));
          String satelliteName = strf("{} {}", planetName, satelliteSuffix->get());
          auto parameters = CelestialParameters::create(m_root, satelliteCoordinate, satelliteSeed, satelliteName, satelliteParameters);
          if (!parameters)
            return std::unexpected(std::move(parameters.error()));
          planet.satelliteParameters[satelliteOrbit] = std::move(*parameters);
        }
      }
      objects[planetOrbit] = std::move(planet);
    }
  }
  return std::pair<CelestialParameters, HashMap<int, CelestialPlanet>>{std::move(*systemParameters), std::move(objects)};
}

std::expected<List<CelestialConstellation>, StarException> CelestialMasterDatabase::produceConstellations(
    RandomSource& random, List<Vec2I> const& constellationCandidates) const {
  List<CelestialConstellation> constellations;

  if (random.randf() < m_generationInformation.constellationProbability && constellationCandidates.size() > 2) {
    auto targetConstellationLineCount = random.randUInt(
        m_generationInformation.constellationLineCountRange[0], m_generationInformation.constellationLineCountRange[1]);
    if (!targetConstellationLineCount)
      return std::unexpected(std::move(targetConstellationLineCount.error()));
    Set<Vec2I> constellationPoints;
    Set<Line2I> constellationLines;

    unsigned tries = 0;

    while (constellationLines.size() < *targetConstellationLineCount) {
      if (++tries > m_generationInformation.constellationMaxTries)
        break;

      Vec2I start;
      if (constellationPoints.empty())
        start = random.randValueFrom(constellationCandidates);
      else
        start = random.randValueFrom(constellationPoints);

      Vec2I end = random.randValueFrom(constellationCandidates);

      Line2I proposedLine(start, end);
      Line2D proposedLineD(proposedLine);

      if (start == end)
        continue;

      if (constellationLines.contains(proposedLine) || constellationLines.contains(proposedLine.reversed()))
        continue;

      if (proposedLineD.diff().magnitude() > m_generationInformation.maximumConstellationLineLength)
        continue;

      if (proposedLineD.diff().magnitude() < m_generationInformation.minimumConstellationLineLength)
        continue;

      bool valid = true;
      for (auto const& constellationLine : constellationLines) {
        Line2D constellationLineD(constellationLine);
        auto intersection = proposedLineD.intersection(constellationLineD);
        if (intersection.intersects && Vec2I::round(intersection.point) != proposedLine.min()
            && Vec2I::round(intersection.point) != proposedLine.max()) {
          valid = false;
          break;
        }

        if (proposedLine.min() != constellationLine.min() && proposedLine.min() != constellationLine.max()
            && constellationLineD.distanceTo(proposedLineD.min())
                < m_generationInformation.minimumConstellationLineCloseness) {
          valid = false;
          break;
        }

        if (proposedLine.max() != constellationLine.min() && proposedLine.max() != constellationLine.max()
            && constellationLineD.distanceTo(proposedLineD.max())
                < m_generationInformation.minimumConstellationLineCloseness) {
          valid = false;
          break;
        }
      }

      if (valid) {
        constellationLines.add(proposedLine);
        constellationPoints.add(proposedLine.min());
        constellationPoints.add(proposedLine.max());
      }
    }

    if (constellationLines.size() > 1) {
      CelestialConstellation constellation;
      for (auto const& line : constellationLines)
        constellation.push_back({line.min(), line.max()});
      constellations.push_back(constellation);
    }
  }

  return constellations;
}

CelestialSlaveDatabase::CelestialSlaveDatabase(RandomService& random) : m_chunkCache(random) {}

std::expected<std::shared_ptr<CelestialSlaveDatabase>, StarException> CelestialSlaveDatabase::create(
    std::shared_ptr<Assets const> const& assets, RandomService& random, CelestialBaseInformation baseInformation) {
  auto config = assets->json("/celestial.config");
  if (!config)
    return std::unexpected(assets->withAssetContext(std::move(config.error()), "/celestial.config", "Loading celestial configuration"));
  auto timeout = config->getFloat("requestTimeout");
  if (!timeout)
    return std::unexpected(assets->withAssetContext(std::move(timeout.error()), "/celestial.config", "Loading celestial configuration"));
  auto database = std::shared_ptr<CelestialSlaveDatabase>(new CelestialSlaveDatabase(random));
  database->m_baseInformation = std::move(baseInformation);
  database->m_requestTimeout = *timeout;
  return database;
}

std::expected<void, StarException> CelestialSlaveDatabase::signalRegion(RectI const& region) {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lockWithDiagnostics(locker);
  for (auto const& location : chunkIndexesFor(region)) {
    auto chunk = m_chunkCache.find_ref(location);
    if (!chunk)
      return std::unexpected(std::move(chunk.error()));
    if (!*chunk && !m_pendingChunkRequests.contains(location))
      m_pendingChunkRequests[location] = Timer();
  }
  return {};
}

std::expected<void, StarException> CelestialSlaveDatabase::signalSystem(CelestialCoordinate const& system) {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lockWithDiagnostics(locker);
  auto chunk = m_chunkCache.find_ref(chunkIndexFor(system));
  if (!chunk)
    return std::unexpected(std::move(chunk.error()));
  if (*chunk) {
    if (!(*chunk)->get().systemObjects.contains(system.location()))
      m_pendingSystemRequests[system.location()] = Timer();
    return {};
  }
  return signalRegion(RectI::withSize(system.location().vec2(), {1, 1}));
}

List<CelestialRequest> CelestialSlaveDatabase::pullRequests() {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lockWithDiagnostics(locker);

  List<CelestialRequest> requests;

  for (auto chunkIt = m_pendingChunkRequests.begin(); chunkIt != m_pendingChunkRequests.end();) {
    auto&& pair = *chunkIt;
    if (!pair.second.running()) {
      requests.push_back(makeLeft(pair.first));
      pair.second.restart(m_requestTimeout);
      ++chunkIt;
    } else if (pair.second.timeUp()) {
      chunkIt = m_pendingChunkRequests.erase(chunkIt);
    } else {
      ++chunkIt;
    }
  }

  for (auto systemIt = m_pendingSystemRequests.begin(); systemIt != m_pendingSystemRequests.end();) {
    auto&& pair = *systemIt;
    if (!pair.second.running()) {
      requests.push_back(makeRight(pair.first));
      pair.second.restart(m_requestTimeout);
      ++systemIt;
    } else if (pair.second.timeUp()) {
      systemIt = m_pendingSystemRequests.erase(systemIt);
    } else {
      ++systemIt;
    }
  }

  return requests;
}

std::expected<void, StarException> CelestialSlaveDatabase::pushResponses(List<CelestialResponse> responses) {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lockWithDiagnostics(locker);
  for (auto& response : responses) {
    if (auto chunk = response.maybeLeftRef()) {
      m_pendingChunkRequests.remove(chunk->get().chunkIndex);
      auto stored = m_chunkCache.set(chunk->get().chunkIndex, std::move(chunk->get()));
      if (!stored)
        return std::unexpected(std::move(stored.error()));
    } else if (auto objects = response.maybeRightRef()) {
      m_pendingSystemRequests.remove(objects->get().systemLocation);
      auto chunk = m_chunkCache.find_ref(chunkIndexFor(objects->get().systemLocation));
      if (!chunk)
        return std::unexpected(std::move(chunk.error()));
      if (*chunk)
        (*chunk)->get().systemObjects[objects->get().systemLocation] = std::move(objects->get().planets);
    }
  }
  return {};
}

void CelestialSlaveDatabase::cleanup() {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lockWithDiagnostics(locker);
  m_chunkCache.cleanup();
}

std::expected<std::optional<CelestialParameters>, StarException> CelestialSlaveDatabase::parameters(CelestialCoordinate const& coordinate) {
  if (!coordinate)
    return std::unexpected(CelestialException("CelestialSlaveDatabase::parameters called on null coordinate"));
  std::unique_lock locker(m_mutex, std::defer_lock);
  lockWithDiagnostics(locker);
  auto signaled = coordinate.isSystem()
      ? signalRegion(RectI::withSize(coordinate.location().vec2(), {1, 1})) : signalSystem(coordinate);
  if (!signaled)
    return std::unexpected(std::move(signaled.error()));
  auto chunk = m_chunkCache.find_ref(chunkIndexFor(coordinate));
  if (!chunk)
    return std::unexpected(std::move(chunk.error()));
  if (*chunk) {
    if (coordinate.isSystem())
      return (*chunk)->get().systemParameters.maybe(coordinate.location());
    if (auto system = (*chunk)->get().systemObjects.find_ref(coordinate.location())) {
      auto planetCoordinate = coordinate.planet();
      if (!planetCoordinate)
        return std::unexpected(std::move(planetCoordinate.error()));
      auto planetOrbit = planetCoordinate->orbitNumber();
      if (!planetOrbit)
        return std::unexpected(std::move(planetOrbit.error()));
      auto planet = system->get().get(*planetOrbit);
      if (!planet)
        return std::unexpected(std::move(planet.error()));
      if (coordinate.isPlanetaryBody())
        return planet->get().planetParameters;
      auto orbit = coordinate.orbitNumber();
      if (!orbit)
        return std::unexpected(std::move(orbit.error()));
      auto satellite = planet->get().satelliteParameters.get(*orbit);
      if (!satellite)
        return std::unexpected(std::move(satellite.error()));
      return satellite->get();
    }
  }
  return std::optional<CelestialParameters>();
}

std::expected<std::optional<String>, StarException> CelestialSlaveDatabase::name(CelestialCoordinate const& coordinate) {
  auto value = parameters(coordinate);
  if (!value)
    return std::unexpected(std::move(value.error()));
  if (*value)
    return (**value).name();
  return std::optional<String>();
}

std::expected<std::optional<bool>, StarException> CelestialSlaveDatabase::hasChildren(CelestialCoordinate const& coordinate) {
  if (!coordinate)
    return std::unexpected(CelestialException("CelestialSlaveDatabase::hasChildren called on null coordinate"));
  std::unique_lock locker(m_mutex, std::defer_lock);
  lockWithDiagnostics(locker);
  auto signaled = signalSystem(coordinate);
  if (!signaled)
    return std::unexpected(std::move(signaled.error()));
  auto chunk = m_chunkCache.find_ref(chunkIndexFor(coordinate));
  if (!chunk)
    return std::unexpected(std::move(chunk.error()));
  if (*chunk) {
    if (auto system = (*chunk)->get().systemObjects.find_ref(coordinate.location())) {
      if (coordinate.isSystem())
        return !system->get().empty();
      if (coordinate.isPlanetaryBody()) {
        auto orbit = coordinate.orbitNumber();
        if (!orbit)
          return std::unexpected(std::move(orbit.error()));
        auto planet = system->get().get(*orbit);
        if (!planet)
          return std::unexpected(std::move(planet.error()));
        return !planet->get().satelliteParameters.empty();
      }
    }
  }
  return std::optional<bool>();
}

std::expected<List<CelestialCoordinate>, StarException> CelestialSlaveDatabase::children(CelestialCoordinate const& coordinate) {
  auto orbits = childOrbits(coordinate);
  if (!orbits)
    return std::unexpected(std::move(orbits.error()));
  List<CelestialCoordinate> children;
  for (auto orbit : *orbits) {
    auto child = coordinate.child(orbit);
    if (!child)
      return std::unexpected(std::move(child.error()));
    children.push_back(std::move(*child));
  }
  return children;
}

std::expected<List<int>, StarException> CelestialSlaveDatabase::childOrbits(CelestialCoordinate const& coordinate) {
  if (!coordinate)
    return std::unexpected(CelestialException("CelestialSlaveDatabase::childOrbits called on null coordinate"));
  if (coordinate.isSatelliteBody())
    return std::unexpected(CelestialException("CelestialSlaveDatabase::childOrbits called on improper type!"));
  std::unique_lock locker(m_mutex, std::defer_lock);
  lockWithDiagnostics(locker);
  auto signaled = signalSystem(coordinate);
  if (!signaled)
    return std::unexpected(std::move(signaled.error()));
  auto chunk = m_chunkCache.find_ref(chunkIndexFor(coordinate));
  if (!chunk)
    return std::unexpected(std::move(chunk.error()));
  if (*chunk) {
    if (auto system = (*chunk)->get().systemObjects.find_ref(coordinate.location())) {
      if (coordinate.isSystem())
        return system->get().keys();
      if (coordinate.isPlanetaryBody()) {
        auto orbit = coordinate.orbitNumber();
        if (!orbit)
          return std::unexpected(std::move(orbit.error()));
        auto planet = system->get().get(*orbit);
        if (!planet)
          return std::unexpected(std::move(planet.error()));
        return planet->get().satelliteParameters.keys();
      }
    }
  }
  return List<int>();
}

std::expected<List<CelestialCoordinate>, StarException> CelestialSlaveDatabase::scanSystems(
    RectI const& region, std::optional<StringSet> const& includedTypes) {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lockWithDiagnostics(locker);
  auto signaled = signalRegion(region);
  if (!signaled)
    return std::unexpected(std::move(signaled.error()));
  List<CelestialCoordinate> systems;
  for (auto const& location : chunkIndexesFor(region)) {
    auto chunk = m_chunkCache.find_ref(location);
    if (!chunk)
      return std::unexpected(std::move(chunk.error()));
    if (*chunk) {
      for (auto const& pair : (*chunk)->get().systemParameters) {
        if (region.contains(pair.first.vec2())) {
          if (includedTypes) {
            auto type = pair.second.getParameter("typeName", "").and_then([](Json const& value) { return value.toString(); });
            if (!type)
              return std::unexpected(std::move(type.error()));
            if (!includedTypes->contains(*type))
              continue;
          }
          systems.push_back(CelestialCoordinate(pair.first));
        }
      }
    }
  }
  return systems;
}

std::expected<List<std::pair<Vec2I, Vec2I>>, StarException> CelestialSlaveDatabase::scanConstellationLines(RectI const& region) {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lockWithDiagnostics(locker);
  auto signaled = signalRegion(region);
  if (!signaled)
    return std::unexpected(std::move(signaled.error()));
  List<std::pair<Vec2I, Vec2I>> lines;
  for (auto const& location : chunkIndexesFor(region)) {
    auto chunk = m_chunkCache.find_ref(location);
    if (!chunk)
      return std::unexpected(std::move(chunk.error()));
    if (*chunk)
      for (auto const& constellation : (*chunk)->get().constellations)
        for (auto const& line : constellation)
          if (region.intersects(Line2I(line.first, line.second)))
            lines.push_back(line);
  }
  return lines;
}

std::expected<bool, StarException> CelestialSlaveDatabase::scanRegionFullyLoaded(RectI const& region) {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lockWithDiagnostics(locker);
  auto signaled = signalRegion(region);
  if (!signaled)
    return std::unexpected(std::move(signaled.error()));
  for (auto const& location : chunkIndexesFor(region)) {
    auto chunk = m_chunkCache.find_ref(location);
    if (!chunk)
      return std::unexpected(std::move(chunk.error()));
    if (!*chunk)
      return false;
  }
  return true;
}

void CelestialSlaveDatabase::invalidateCacheFor(CelestialCoordinate const& coordinate) {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lockWithDiagnostics(locker);

  m_chunkCache.remove(chunkIndexFor(coordinate));
}

}

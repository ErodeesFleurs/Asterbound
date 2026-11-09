import star.net_element;
import star.json;
import star.json_extra;
import star.data_stream_extra;
import star.interpolation;
import star.ordered_map;
import star.either;
import star.vector;
import star.game_types;
import star.color;
import star.bi_map;
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
import star.casting;
import star.strong_typedef;
import star.io_device;
import star.ref_ptr;
import star.logging;
import star.listener;
import std;
import star.string;
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
import star.uuid;
import star.warping;
import star.sky_parameters;
import star.system_world;
import star.asset_source;
import star.assets;
import star.root_base;
import star.configuration;
import star.root;
import star.system_world_server;
import star.name_generator;


import star.celestial_database;

import star.data_stream_devices;

import star.net_compatibility;
import star.byte_array;
import star.static_random;
import star.math_common;
namespace Star {

std::expected<CelestialOrbit, StarException> CelestialOrbit::fromJson(Json const& json) {
  auto targetData = json.get("target");
  if (!targetData)
    return std::unexpected(std::move(targetData.error()));
  
  auto target = CelestialCoordinate::create(*targetData);
  if (!target)
    return std::unexpected(std::move(target.error()));
  
  auto direction = json.getInt("direction");
  if (!direction)
    return std::unexpected(std::move(direction.error()));
  
  auto time = json.getDouble("enterTime");
  if (!time)
    return std::unexpected(std::move(time.error()));
  
  auto positionData = json.get("enterPosition");
  if (!positionData)
    return std::unexpected(std::move(positionData.error()));
  
  auto position = jsonToVec2F(*positionData);
  if (!position)
    return std::unexpected(std::move(position.error()));

  return CelestialOrbit{std::move(*target), static_cast<int>(*direction), *time, *position};
}

Json CelestialOrbit::toJson() const {
  JsonObject json;
  json.set("target", target.toJson());
  json.set("direction", direction);
  json.set("enterTime", enterTime);
  json.set("enterPosition", jsonFromVec2F(enterPosition));
  return json;
}

std::expected<void, StarException> CelestialOrbit::write(DataStream& ds) const {
  auto field0 = ds.write(target);
  if (!field0)
    return std::unexpected(std::move(field0.error()));
  auto field1 = ds.write(direction);
  if (!field1)
    return std::unexpected(std::move(field1.error()));
  auto field2 = ds.write(enterTime);
  if (!field2)
    return std::unexpected(std::move(field2.error()));
  auto field3 = ds.write(enterPosition);
  if (!field3)
    return std::unexpected(std::move(field3.error()));
  return {};
}

std::expected<void, StarException> CelestialOrbit::read(DataStream& ds) {
  auto field0 = ds.read(target);
  if (!field0)
    return std::unexpected(std::move(field0.error()));
  auto field1 = ds.read(direction);
  if (!field1)
    return std::unexpected(std::move(field1.error()));
  auto field2 = ds.read(enterTime);
  if (!field2)
    return std::unexpected(std::move(field2.error()));
  auto field3 = ds.read(enterPosition);
  if (!field3)
    return std::unexpected(std::move(field3.error()));
  return {};
}

bool CelestialOrbit::operator==(CelestialOrbit const& rhs) const {
  return target == rhs.target
    && direction == rhs.direction
    && enterTime == rhs.enterTime
    && enterPosition == rhs.enterPosition;
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, CelestialOrbit& orbit) {
  auto result = orbit.read(ds);
  if (!result)
    return std::unexpected(std::move(result.error()));
  return std::ref(ds);
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, CelestialOrbit const& orbit) {
  auto result = orbit.write(ds);
  if (!result)
    return std::unexpected(std::move(result.error()));
  return std::ref(ds);
}

std::expected<SystemLocation, StarException> jsonToSystemLocation(Json const& json) {
  auto array = json.optArray();
  if (!array)
    return std::unexpected(std::move(array.error()));
  if (!*array)
    return SystemLocation{};
  Json location(**array);
  auto first = location.get(0);
  if (!first)
    return std::unexpected(std::move(first.error()));
  if (first->type() == Json::Type::String) {
    auto type = first->toString();
    if (!type)
      return std::unexpected(std::move(type.error()));
    if (*type == "coordinate" || *type == "orbit" || *type == "object") {
      auto value = location.get(1);
      if (!value)
        return std::unexpected(std::move(value.error()));
      if (*type == "coordinate") {
        auto coordinate = CelestialCoordinate::create(*value);
        if (!coordinate)
          return std::unexpected(std::move(coordinate.error()));
        return SystemLocation(std::move(*coordinate));
      } else if (*type == "orbit") {
        auto orbit = CelestialOrbit::fromJson(*value);
        if (!orbit)
          return std::unexpected(std::move(orbit.error()));
        return SystemLocation(std::move(*orbit));
      }
      auto uuidString = value->toString();
      if (!uuidString)
        return std::unexpected(std::move(uuidString.error()));
      auto uuid = Uuid::create(*uuidString);
      if (!uuid)
        return std::unexpected(std::move(uuid.error()));
      return SystemLocation(std::move(*uuid));
    }
  } else {
    auto position = jsonToOptional<Vec2F>(location, jsonToVec2F);
    if (!position)
      return std::unexpected(std::move(position.error()));
    if (*position)
      return SystemLocation(**position);
  }
  return SystemLocation{};
}

Json jsonFromSystemLocation(SystemLocation const& location) {
  if (std::holds_alternative<CelestialCoordinate>(location))
    return JsonArray{"coordinate", std::get<CelestialCoordinate>(location).toJson()};
  else if (std::holds_alternative<CelestialOrbit>(location))
    return JsonArray{"orbit", std::get<CelestialOrbit>(location).toJson()};
  else if (std::holds_alternative<Uuid>(location))
    return JsonArray{"object", std::get<Uuid>(location).hex()};
  else if (std::holds_alternative<Vec2F>(location))
    return jsonFromVec2F(std::get<Vec2F>(location));
  else
    return {};
}

std::expected<SystemWorldConfig, StarException> SystemWorldConfig::fromJson(Json const& json) {
  SystemWorldConfig config;
  auto starGravitationalConstant = json.getFloat("starGravitationalConstant");
  if (!starGravitationalConstant)
    return std::unexpected(std::move(starGravitationalConstant.error()));
  config.starGravitationalConstant = *starGravitationalConstant;
  auto planetGravitationalConstant = json.getFloat("planetGravitationalConstant");
  if (!planetGravitationalConstant)
    return std::unexpected(std::move(planetGravitationalConstant.error()));
  config.planetGravitationalConstant = *planetGravitationalConstant;
  auto sizes = json.getArray("planetSizes");
  if (!sizes)
    return std::unexpected(std::move(sizes.error()));
  for (auto const& size : *sizes) {
    auto width = size.getUInt(0);
    if (!width)
      return std::unexpected(std::move(width.error()));
    auto radius = size.getFloat(1);
    if (!radius)
      return std::unexpected(std::move(radius.error()));
    config.planetSizes.set(*width, *radius);
  }
  auto emptyOrbitSize = json.getFloat("emptyOrbitSize");
  if (!emptyOrbitSize)
    return std::unexpected(std::move(emptyOrbitSize.error()));
  config.emptyOrbitSize = *emptyOrbitSize;
  auto unvisitablePlanetSize = json.getFloat("unvisitablePlanetSize");
  if (!unvisitablePlanetSize)
    return std::unexpected(std::move(unvisitablePlanetSize.error()));
  config.unvisitablePlanetSize = *unvisitablePlanetSize;
  auto dungeonSizes = json.getObject("floatingDungeonWorldSizes");
  if (!dungeonSizes)
    return std::unexpected(std::move(dungeonSizes.error()));
  for (auto const& entry : *dungeonSizes) {
    auto size = entry.second.toFloat();
    if (!size)
      return std::unexpected(std::move(size.error()));
    config.floatingDungeonWorldSizes.set(entry.first, *size);
  }
  auto starSize = json.getFloat("starSize");
  if (!starSize)
    return std::unexpected(std::move(starSize.error()));
  config.starSize = *starSize;
  auto planetaryOrbitPaddingData = json.get("planetaryOrbitPadding");
  if (!planetaryOrbitPaddingData)
    return std::unexpected(std::move(planetaryOrbitPaddingData.error()));
  
  auto planetaryOrbitPadding = jsonToVec2F(*planetaryOrbitPaddingData);
  if (!planetaryOrbitPadding)
    return std::unexpected(std::move(planetaryOrbitPadding.error()));
  config.planetaryOrbitPadding = std::move(*planetaryOrbitPadding);
  auto satelliteOrbitPaddingData = json.get("satelliteOrbitPadding");
  if (!satelliteOrbitPaddingData)
    return std::unexpected(std::move(satelliteOrbitPaddingData.error()));
  
  auto satelliteOrbitPadding = jsonToVec2F(*satelliteOrbitPaddingData);
  if (!satelliteOrbitPadding)
    return std::unexpected(std::move(satelliteOrbitPadding.error()));
  config.satelliteOrbitPadding = std::move(*satelliteOrbitPadding);
  auto arrivalRangeData = json.get("arrivalRange");
  if (!arrivalRangeData)
    return std::unexpected(std::move(arrivalRangeData.error()));
  
  auto arrivalRange = jsonToVec2F(*arrivalRangeData);
  if (!arrivalRange)
    return std::unexpected(std::move(arrivalRange.error()));
  config.arrivalRange = std::move(*arrivalRange);
  auto objectSpawnPadding = json.getFloat("objectSpawnPadding");
  if (!objectSpawnPadding)
    return std::unexpected(std::move(objectSpawnPadding.error()));
  config.objectSpawnPadding = *objectSpawnPadding;
  auto clientObjectSpawnPadding = json.getFloat("clientObjectSpawnPadding");
  if (!clientObjectSpawnPadding)
    return std::unexpected(std::move(clientObjectSpawnPadding.error()));
  config.clientObjectSpawnPadding = *clientObjectSpawnPadding;
  auto objectSpawnIntervalData = json.get("objectSpawnInterval");
  if (!objectSpawnIntervalData)
    return std::unexpected(std::move(objectSpawnIntervalData.error()));
  
  auto objectSpawnInterval = jsonToVec2F(*objectSpawnIntervalData);
  if (!objectSpawnInterval)
    return std::unexpected(std::move(objectSpawnInterval.error()));
  config.objectSpawnInterval = std::move(*objectSpawnInterval);
  auto objectSpawnCycle = json.getDouble("objectSpawnCycle");
  if (!objectSpawnCycle)
    return std::unexpected(std::move(objectSpawnCycle.error()));
  config.objectSpawnCycle = *objectSpawnCycle;
  auto minObjectOrbitTime = json.getFloat("minObjectOrbitTime");
  if (!minObjectOrbitTime)
    return std::unexpected(std::move(minObjectOrbitTime.error()));
  config.minObjectOrbitTime = *minObjectOrbitTime;
  auto asteroidBeamDistance = json.getFloat("asteroidBeamDistance");
  if (!asteroidBeamDistance)
    return std::unexpected(std::move(asteroidBeamDistance.error()));
  config.asteroidBeamDistance = *asteroidBeamDistance;
  auto emptySkyParametersData = json.get("emptySkyParameters");
  if (!emptySkyParametersData)
    return std::unexpected(std::move(emptySkyParametersData.error()));
  
  auto emptySkyParameters = SkyParameters::create(*emptySkyParametersData);
  if (!emptySkyParameters)
    return std::unexpected(std::move(emptySkyParameters.error()));
  config.emptySkyParameters = std::move(*emptySkyParameters);
  return config;
}

SystemWorld::SystemWorld(Root& root, std::shared_ptr<Clock const> universeClock, std::shared_ptr<CelestialDatabase> celestialDatabase)
  : m_root(root), m_celestialDatabase(std::move(celestialDatabase)), m_universeClock(std::move(universeClock)) {}

std::expected<void, StarException> SystemWorld::initialize() {
  auto assets = root().assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  auto configJson = (*assets)->json("/systemworld.config");
  if (!configJson)
    return std::unexpected(std::move(configJson.error()));
  auto config = SystemWorldConfig::fromJson(*configJson);
  if (!config)
    return std::unexpected(std::move(config.error()));
  m_config = std::move(*config);
  return {};
}

SystemWorldConfig const& SystemWorld::systemConfig() const {
  return m_config;
}

double SystemWorld::time() const {
  return m_universeClock->time();
}

Vec3I SystemWorld::location() const {
  return m_location;
}

std::expected<List<CelestialCoordinate>, StarException> SystemWorld::planets() const {
  return m_celestialDatabase->children(CelestialCoordinate(m_location));
}

std::expected<std::uint64_t, StarException> SystemWorld::coordinateSeed(CelestialCoordinate const& coordinate, String const& seedMix) const {
  int satellite = 0;
  int planet = 0;
  if (coordinate.isSatelliteBody()) {
    auto satelliteOrbit = coordinate.orbitNumber();
    if (!satelliteOrbit)
      return std::unexpected(std::move(satelliteOrbit.error()));
    satellite = *satelliteOrbit;
    auto parent = coordinate.parent();
    if (!parent)
      return std::unexpected(std::move(parent.error()));
    auto parentOrbit = parent->orbitNumber();
    if (!parentOrbit)
      return std::unexpected(std::move(parentOrbit.error()));
    planet = *parentOrbit;
  } else if (coordinate.isPlanetaryBody()) {
    auto orbit = coordinate.orbitNumber();
    if (!orbit)
      return std::unexpected(std::move(orbit.error()));
    // Preserve the original planetary seed's boolean orbit component.
    planet = bool(*orbit);
  }
  return static_random_u64(coordinate.location()[0], coordinate.location()[1], coordinate.location()[2], planet,
                           satellite, seedMix);
}

std::expected<float, StarException> SystemWorld::planetOrbitDistance(CelestialCoordinate const& coordinate) const {
  auto seed = coordinateSeed(coordinate, "PlanetOrbitDistance");
  if (!seed)
    return std::unexpected(std::move(seed.error()));
  RandomSource random(*seed);
  if (coordinate.isSystem() || coordinate.isNull())
    return 0;
  auto parent = coordinate.parent();
  if (!parent)
    return std::unexpected(std::move(parent.error()));
  auto parentSize = planetSize(*parent);
  if (!parentSize)
    return std::unexpected(std::move(parentSize.error()));
  float distance = *parentSize / 2.0;
  auto orbit = coordinate.orbitNumber();
  if (!orbit)
    return std::unexpected(std::move(orbit.error()));
  for (int i = 0; i < *orbit; ++i) {
    if (i > 0 && i < *orbit) {
      auto child = parent->child(i);
      if (!child)
        return std::unexpected(std::move(child.error()));
      auto size = clusterSize(*child);
      if (!size)
        return std::unexpected(std::move(size.error()));
      distance += *size;
    }
    if (coordinate.isPlanetaryBody()) {
      auto padding = random.randf(m_config.planetaryOrbitPadding[0], m_config.planetaryOrbitPadding[1]);
      if (!padding)
        return std::unexpected(std::move(padding.error()));
      distance += *padding;
    } else if (coordinate.isSatelliteBody()) {
      auto padding = random.randf(m_config.satelliteOrbitPadding[0], m_config.satelliteOrbitPadding[1]);
      if (!padding)
        return std::unexpected(std::move(padding.error()));
      distance += *padding;
    }
  }
  auto size = clusterSize(coordinate);
  if (!size)
    return std::unexpected(std::move(size.error()));
  return distance + *size / 2.0;
}

float SystemWorld::orbitInterval(float distance, bool isMoon) const {
  float gravityConstant = isMoon ? m_config.planetGravitationalConstant : m_config.starGravitationalConstant;
  float speed =  std::sqrt(gravityConstant / distance);
  return (distance * 2 * std::numbers::pi) / speed;
}

std::expected<Vec2F, StarException> SystemWorld::orbitPosition(CelestialOrbit const& orbit) const {
  Vec2F targetPosition(0, 0);
  if (orbit.target.isPlanetaryBody() || orbit.target.isSatelliteBody()) {
    auto position = planetPosition(orbit.target);
    if (!position)
      return std::unexpected(std::move(position.error()));
    targetPosition = *position;
  }
  float distance = orbit.enterPosition.magnitude();
  float interval = orbitInterval(distance, false);
  float timeOffset = std::fmod(time() - orbit.enterTime, interval) / interval;
  float angle = (orbit.enterPosition * -1).angle() + (orbit.direction * timeOffset * (std::numbers::pi * 2));
  return targetPosition + Vec2F::with_angle(angle, distance);
}

std::expected<float, StarException> SystemWorld::clusterSize(CelestialCoordinate const& coordinate) const {
  if (coordinate.isPlanetaryBody()) {
    auto parent = coordinate.parent();
    if (!parent)
      return std::unexpected(std::move(parent.error()));
    auto orbits = m_celestialDatabase->childOrbits(*parent);
    if (!orbits)
      return std::unexpected(std::move(orbits.error()));
    auto orbit = coordinate.orbitNumber();
    if (!orbit)
      return std::unexpected(std::move(orbit.error()));
    if (orbits->contains(*orbit)) {
      auto childOrbits = m_celestialDatabase->childOrbits(coordinate);
      if (!childOrbits)
        return std::unexpected(std::move(childOrbits.error()));
      auto outerOrbit = std::ranges::max_element(*childOrbits);
      if (outerOrbit != childOrbits->end()) {
        auto outer = coordinate.child(*outerOrbit);
        if (!outer)
          return std::unexpected(std::move(outer.error()));
        auto distance = planetOrbitDistance(*outer);
        if (!distance)
          return std::unexpected(std::move(distance.error()));
        auto size = planetSize(*outer);
        if (!size)
          return std::unexpected(std::move(size.error()));
        return *distance * 2 + *size;
      }
    }
  }
  return planetSize(coordinate);
}

std::expected<float, StarException> SystemWorld::planetSize(CelestialCoordinate const& coordinate) const {
  if (coordinate.isNull())
    return 0;
  if (coordinate.isSystem())
    return m_config.starSize;
  auto parent = coordinate.parent();
  if (!parent)
    return std::unexpected(std::move(parent.error()));
  auto orbits = m_celestialDatabase->childOrbits(*parent);
  if (!orbits)
    return std::unexpected(std::move(orbits.error()));
  auto orbit = coordinate.orbitNumber();
  if (!orbit)
    return std::unexpected(std::move(orbit.error()));
  if (!orbits->contains(*orbit))
    return m_config.emptyOrbitSize;
  auto parameters = m_celestialDatabase->parameters(coordinate);
  if (!parameters)
    return std::unexpected(std::move(parameters.error()));
  if (*parameters) {
    if (auto visitable = (**parameters).visitableParameters()) {
      if (is<FloatingDungeonWorldParameters>(visitable))
        if (auto size = m_config.floatingDungeonWorldSizes.maybe(visitable->typeName))
          return *size;
      float size = 0;
      for (auto const& entry : m_config.planetSizes) {
        if (visitable->worldSize[0] < entry.first)
          break;
        size = entry.second;
      }
      return size;
    }
  }
  return m_config.unvisitablePlanetSize;
}

std::expected<Vec2F, StarException> SystemWorld::planetPosition(CelestialCoordinate const& coordinate) const {
  if (coordinate.isNull() || coordinate.isSystem())
    return Vec2F(0, 0);
  auto seed = coordinateSeed(coordinate, "PlanetSystemPosition");
  if (!seed)
    return std::unexpected(std::move(seed.error()));
  RandomSource random(*seed);
  auto parent = coordinate.parent();
  if (!parent)
    return std::unexpected(std::move(parent.error()));
  auto parentPosition = planetPosition(*parent);
  if (!parentPosition)
    return std::unexpected(std::move(parentPosition.error()));
  auto distance = planetOrbitDistance(coordinate);
  if (!distance)
    return std::unexpected(std::move(distance.error()));
  float interval = orbitInterval(*distance, coordinate.isSatelliteBody());
  double start = random.randf();
  double offset = std::fmod(m_universeClock->time(), interval) / interval;
  int direction = random.randf() > 0.5 ? 1 : -1;
  float angle = (start + direction * offset) * (std::numbers::pi * 2);
  return *parentPosition + Vec2F(std::cos(angle), std::sin(angle)) * *distance;
}

std::expected<SystemObjectConfig, StarException> SystemWorld::systemObjectConfig(String const& name, Uuid const&) const {
  auto config = systemObjectTypeConfig(root(), name);
  if (!config)
    return std::unexpected(std::move(config.error()));
  auto orbitData = config->get("orbitRange");
  if (!orbitData)
    return std::unexpected(std::move(orbitData.error()));
  auto orbitRange = jsonToVec2F(*orbitData);
  if (!orbitRange)
    return std::unexpected(std::move(orbitRange.error()));
  auto lifetimeData = config->get("lifeTime");
  if (!lifetimeData)
    return std::unexpected(std::move(lifetimeData.error()));
  auto lifetimeRange = jsonToVec2F(*lifetimeData);
  if (!lifetimeRange)
    return std::unexpected(std::move(lifetimeRange.error()));
  SystemObjectConfig object;
  object.name = name;
  auto moving = config->getBool("moving");
  if (!moving)
    return std::unexpected(std::move(moving.error()));
  object.moving = *moving;
  auto speed = config->getFloat("speed");
  if (!speed)
    return std::unexpected(std::move(speed.error()));
  object.speed = *speed;
  auto orbitDistance = root().random().randf((*orbitRange)[0], (*orbitRange)[1]);
  if (!orbitDistance)
    return std::unexpected(std::move(orbitDistance.error()));
  object.orbitDistance = *orbitDistance;
  auto lifeTime = root().random().randf((*lifetimeRange)[0], (*lifetimeRange)[1]);
  if (!lifeTime)
    return std::unexpected(std::move(lifeTime.error()));
  object.lifeTime = *lifeTime;
  auto permanent = config->getBool("permanent", false);
  if (!permanent)
    return std::unexpected(std::move(permanent.error()));
  object.permanent = *permanent;
  auto warpText = config->getString("warpAction");
  if (!warpText)
    return std::unexpected(std::move(warpText.error()));
  
  auto warp = parseWarpAction(*warpText);
  if (!warp)
    return std::unexpected(std::move(warp.error()));
  object.warpAction = std::move(*warp);
  auto threat = config->optFloat("threatLevel");
  if (!threat)
    return std::unexpected(std::move(threat.error()));
  object.threatLevel = *threat;
  auto skyData = config->get("skyParameters");
  if (!skyData)
    return std::unexpected(std::move(skyData.error()));
  
  auto sky = SkyParameters::create(*skyData);
  if (!sky)
    return std::unexpected(std::move(sky.error()));
  object.skyParameters = std::move(*sky);
  auto parameters = config->getObject("parameters");
  if (!parameters)
    return std::unexpected(std::move(parameters.error()));
  object.parameters = std::move(*parameters);
  auto generated = config->contains("generatedParameters");
  if (!generated)
    return std::unexpected(std::move(generated.error()));
  if (*generated) {
    auto values = config->getObject("generatedParameters");
    if (!values)
      return std::unexpected(std::move(values.error()));
    for (auto const& entry : *values) {
      auto value = entry.second.toString();
      if (!value)
        return std::unexpected(std::move(value.error()));
      object.generatedParameters[entry.first] = std::move(*value);
    }
  }
  return object;
}

std::expected<Json, StarException> SystemWorld::systemObjectTypeConfig(Root& root, String const& name) {
  auto assets = root.assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  return (*assets)->json(Star::format("/system_objects.config:{}", name));
}

std::expected<std::optional<Vec2F>, StarException> SystemWorld::systemLocationPosition(SystemLocation const& location) const {
  if (std::holds_alternative<CelestialCoordinate>(location)) {
    auto position = planetPosition(std::get<CelestialCoordinate>(location));
    if (!position)
      return std::unexpected(std::move(position.error()));
    return std::optional<Vec2F>(*position);
  } else if (std::holds_alternative<CelestialOrbit>(location)) {
    auto position = orbitPosition(std::get<CelestialOrbit>(location));
    if (!position)
      return std::unexpected(std::move(position.error()));
    return std::optional<Vec2F>(*position);
  } else if (std::holds_alternative<Uuid>(location)) {
    if (auto object = getObject(std::get<Uuid>(location)))
      return std::optional<Vec2F>(object->position());
  } else if (std::holds_alternative<Vec2F>(location)) {
    return std::optional<Vec2F>(std::get<Vec2F>(location));
  }
  return std::optional<Vec2F>{};
}

Vec2F SystemWorld::randomArrivalPosition() const {
  RandomSource rand(this->root().random().randu64());
  float range = m_config.arrivalRange[0] + (rand.randf() * (m_config.arrivalRange[1] - m_config.arrivalRange[0]));
  float angle = rand.randf() * std::numbers::pi * 2;
  return Vec2F::with_angle(angle, range);
}

std::expected<std::optional<WarpAction>, StarException> SystemWorld::objectWarpAction(Uuid const& uuid) const {
  auto object = getObject(uuid);
  if (!object)
    return std::optional<WarpAction>{};
  WarpAction warpAction = object->warpAction();
  if (std::holds_alternative<WarpToWorld>(warpAction)) {
    auto& warpToWorld = std::get<WarpToWorld>(warpAction);
    if (std::holds_alternative<InstanceWorldId>(warpToWorld.world)) {
      auto& instanceWorldId = std::get<InstanceWorldId>(warpToWorld.world);
      instanceWorldId.uuid = object->uuid();
      auto parameters = m_celestialDatabase->parameters(CelestialCoordinate(m_location));
      if (!parameters)
        return std::unexpected(std::move(parameters.error()));
      if (!*parameters)
        return std::optional<WarpAction>{};
      auto threatData = (**parameters).getParameter("spaceThreatLevel");
      if (!threatData)
        return std::unexpected(std::move(threatData.error()));
      auto threat = threatData->optFloat();
      if (!threat)
        return std::unexpected(std::move(threat.error()));
      auto objectThreat = object->threatLevel();
      instanceWorldId.level = objectThreat ? objectThreat : *threat;
    }
  }
  return std::optional<WarpAction>(std::move(warpAction));
}

SystemObject::SystemObject(Root& root, SystemObjectConfig config, Uuid uuid, Vec2F const& position, double spawnTime, JsonObject parameters)
  : m_root(root), m_config(std::move(config)), m_uuid(std::move(uuid)), m_spawnTime(spawnTime), m_parameters(std::move(parameters)) {
  setPosition(position);
}

std::expected<std::shared_ptr<SystemObject>, StarException> SystemObject::create(Root& root, SystemObjectConfig config, Uuid uuid, Vec2F const& position, JsonObject parameters) {
  auto object = std::shared_ptr<SystemObject>(new SystemObject(root, std::move(config), std::move(uuid), position, 0.0f, std::move(parameters)));
  auto initialized = object->init();
  if (!initialized)
    return std::unexpected(std::move(initialized.error()));
  return object;
}

std::expected<std::shared_ptr<SystemObject>, StarException> SystemObject::create(Root& root, SystemObjectConfig config, Uuid uuid, Vec2F const& position, double spawnTime, JsonObject parameters) {
  auto object = std::shared_ptr<SystemObject>(new SystemObject(root, std::move(config), std::move(uuid), position, spawnTime, std::move(parameters)));
  for (auto const& entry : object->m_config.generatedParameters) {
    if (!object->m_parameters.contains(entry.first)) {
      auto generator = root.nameGenerator();
      if (!generator)
        return std::unexpected(std::move(generator.error()));
      auto name = (*generator)->generateName(entry.second);
      if (!name)
        return std::unexpected(std::move(name.error()));
      object->m_parameters[entry.first] = std::move(*name);
    }
  }
  auto initialized = object->init();
  if (!initialized)
    return std::unexpected(std::move(initialized.error()));
  return object;
}

std::expected<std::shared_ptr<SystemObject>, StarException> SystemObject::create(Root& root, SystemWorld& system, Json const& diskStore) {
  // Preserve the old disk-load constructor's random identity draw order.
  auto object = std::shared_ptr<SystemObject>(new SystemObject(root, {}, Uuid(root.random()), {}, 0, {}));
  auto uuidText = diskStore.getString("uuid");
  if (!uuidText)
    return std::unexpected(std::move(uuidText.error()));
  auto uuid = Uuid::create(*uuidText);
  if (!uuid)
    return std::unexpected(std::move(uuid.error()));
  object->m_uuid = std::move(*uuid);
  auto name = diskStore.getString("name");
  if (!name)
    return std::unexpected(std::move(name.error()));
  auto config = system.systemObjectConfig(*name, object->m_uuid);
  if (!config)
    return std::unexpected(std::move(config.error()));
  object->m_config = std::move(*config);
  auto parameters = diskStore.getObject("parameters", {});
  if (!parameters)
    return std::unexpected(std::move(parameters.error()));
  object->m_parameters = std::move(*parameters);
  auto orbitData = diskStore.get("orbit");
  if (!orbitData)
    return std::unexpected(std::move(orbitData.error()));
  auto orbit = jsonToOptional<CelestialOrbit>(*orbitData, CelestialOrbit::fromJson);
  if (!orbit)
    return std::unexpected(std::move(orbit.error()));
  object->m_orbit.set(std::move(*orbit));
  auto spawnTime = diskStore.getDouble("spawnTime");
  if (!spawnTime)
    return std::unexpected(std::move(spawnTime.error()));
  object->m_spawnTime = *spawnTime;
  auto positionData = diskStore.get("position");
  if (!positionData)
    return std::unexpected(std::move(positionData.error()));
  auto position = jsonToVec2F(*positionData);
  if (!position)
    return std::unexpected(std::move(position.error()));
  object->setPosition(*position);
  auto initialized = object->init();
  if (!initialized)
    return std::unexpected(std::move(initialized.error()));
  return object;
}


std::expected<void, StarException> SystemObject::init() {
  m_shouldDestroy = false;
  m_xPosition.setInterpolator(lerp<float, float>);
  m_yPosition.setInterpolator(lerp<float, float>);
  for (NetElement& element : std::initializer_list<std::reference_wrapper<NetElement>>{m_xPosition, m_yPosition, m_orbit}) {
    auto added = m_netGroup.addNetElement(element);
    if (!added)
      return std::unexpected(std::move(added.error()));
  }
  return {};
}

Uuid SystemObject::uuid() const {
  return m_uuid;
}

String SystemObject::name() const {
  return m_config.name;
}

bool SystemObject::permanent() const {
  return m_config.permanent;
}

Vec2F SystemObject::position() const {
  return Vec2F(m_xPosition.get(), m_yPosition.get());
}

WarpAction SystemObject::warpAction() const {
  return m_config.warpAction;
}

std::optional<float> SystemObject::threatLevel() const {
  return m_config.threatLevel;
}

SkyParameters SystemObject::skyParameters() const {
  return m_config.skyParameters;
}

std::expected<JsonObject, StarException> SystemObject::parameters() const {
  return jsonMerge(m_config.parameters, m_parameters).toObject();
}

bool SystemObject::shouldDestroy() const {
  return m_shouldDestroy;
}

void SystemObject::enterOrbit(CelestialCoordinate const& target, Vec2F const& targetPosition, double time) {
  int direction = this->root().random().randf() > 0.5 ? 1 : -1; // random direction
  m_orbit.set(CelestialOrbit{target, direction, time, targetPosition - position()});
  m_approach.reset();
}

std::optional<CelestialCoordinate> SystemObject::orbitTarget() const {
  if (m_orbit.get())
    return m_orbit.get()->target;
  else
    return {};
}

std::optional<CelestialOrbit> SystemObject::orbit() const {
  return m_orbit.get();
}

std::expected<void, StarException> SystemObject::clientUpdate(float dt) {
  return m_netGroup.tickNetInterpolation(dt);
}

std::expected<void, StarException> SystemObject::serverUpdate(SystemWorldServer& system, float dt) {
  if (!m_config.permanent && system.time() > m_spawnTime + m_config.lifeTime)
    m_shouldDestroy = true;
  if (m_orbit.get()) {
    auto position = system.orbitPosition(*m_orbit.get());
    if (!position)
      return std::unexpected(std::move(position.error()));
    setPosition(*position);
  } else if (m_config.permanent || !m_config.moving) {
    enterOrbit(CelestialCoordinate(system.location()), {0.0, 0.0}, system.time());
  } else if (m_approach && !m_approach->isNull()) {
    if (!system.shipsAtLocation(m_uuid).empty())
      return {};
    if (m_approach->isPlanetaryBody()) {
      auto approach = system.planetPosition(*m_approach);
      if (!approach)
        return std::unexpected(std::move(approach.error()));
      auto toApproach = *approach - position();
      setPosition(position() + toApproach.normalized() * m_config.speed * dt);
      auto size = system.planetSize(*m_approach);
      if (!size)
        return std::unexpected(std::move(size.error()));
      if ((*approach - position()).magnitude() < *size + m_config.orbitDistance)
        enterOrbit(*m_approach, *approach, system.time());
    } else {
      enterOrbit(*m_approach, {0.0, 0.0}, system.time());
    }
  } else {
    auto planets = system.planets();
    if (!planets)
      return std::unexpected(std::move(planets.error()));
    auto available = planets->filtered([&system](CelestialCoordinate const& p) {
      return std::ranges::none_of(system.objects(), [&p](std::shared_ptr<SystemObject> const& o) { return o->orbitTarget() == p; });
    });
    if (!available.empty()) {
      auto selected = root().random().randFrom(available);
      if (!selected)
        return std::unexpected(std::move(selected.error()));
      m_approach = selected->get();
    }
  }
  return {};
}

std::expected<std::pair<ByteArray, std::uint64_t>, StarException> SystemObject::writeNetState(std::uint64_t fromVersion, NetCompatibilityRules rules) {
  return m_netGroup.writeNetState(fromVersion, rules);
}

std::expected<void, StarException> SystemObject::readNetState(ByteArray data, float interpolationTime, NetCompatibilityRules rules) {
  return m_netGroup.readNetState(std::move(data), interpolationTime, rules);
}

std::expected<ByteArray, StarException> SystemObject::netStore() const {
  DataStreamBuffer ds;
  auto field0 = ds.write(m_uuid);
  if (!field0)
    return std::unexpected(std::move(field0.error()));
  
  auto field1 = ds.write(m_config.name);
  if (!field1)
    return std::unexpected(std::move(field1.error()));
  
  auto field2 = ds.write(position());
  if (!field2)
    return std::unexpected(std::move(field2.error()));
  
  auto field3 = ds.write(m_parameters);
  if (!field3)
    return std::unexpected(std::move(field3.error()));

  return ds.take_data();
}

std::expected<Json, StarException> SystemObject::diskStore() const {
  auto orbit = jsonFromOptional<CelestialOrbit>(m_orbit.get(), [](CelestialOrbit const& o) { return o.toJson(); });
  if (!orbit)
    return std::unexpected(std::move(orbit.error()));
  return JsonObject{{"uuid", m_uuid.hex()}, {"name", m_config.name}, {"orbit", std::move(*orbit)},
    {"spawnTime", m_spawnTime}, {"position", jsonFromVec2F(position())}, {"parameters", m_parameters}};
}

void SystemObject::setPosition(Vec2F const& position) {
  m_xPosition.set(position[0]);
  m_yPosition.set(position[1]);
}

SystemClientShip::SystemClientShip(Uuid uuid) : m_uuid(std::move(uuid)), m_departTimer(0), m_speed(0) {}

std::expected<std::shared_ptr<SystemClientShip>, StarException> SystemClientShip::create(SystemWorld& system, Uuid uuid, float speed, SystemLocation const& location) {
  auto ship = std::shared_ptr<SystemClientShip>(new SystemClientShip(std::move(uuid)));
  auto initialized = ship->initialize(system, speed, location);
  if (!initialized)
    return std::unexpected(std::move(initialized.error()));
  return ship;
}

std::expected<std::shared_ptr<SystemClientShip>, StarException> SystemClientShip::create(SystemWorld& system, Uuid uuid, SystemLocation const& location) {
  return create(system, std::move(uuid), 0.0f, location);
}

std::expected<void, StarException> SystemClientShip::initialize(SystemWorld& system, float speed, SystemLocation const& location) {
  m_systemLocation.set(location);
  auto position = system.systemLocationPosition(location);
  if (!position)
    return std::unexpected(std::move(position.error()));
  setPosition(position->value_or(Vec2F{}));
  auto assets = system.root().assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  auto config = (*assets)->json("/systemworld.config:clientShip");
  if (!config)
    return std::unexpected(std::move(config.error()));
  auto orbitDistance = config->getFloat("orbitDistance");
  if (!orbitDistance)
    return std::unexpected(std::move(orbitDistance.error()));
  auto departTime = config->getFloat("departTime");
  if (!departTime)
    return std::unexpected(std::move(departTime.error()));
  auto spaceDepartTime = config->getFloat("spaceDepartTime");
  if (!spaceDepartTime)
    return std::unexpected(std::move(spaceDepartTime.error()));
  m_config = ClientShipConfig{*orbitDistance, *departTime, *spaceDepartTime};
  m_speed = speed;
  // Do not interpolate locations: stale identities can refer to removed objects.
  auto locationAdded = m_netGroup.addNetElement(m_systemLocation, false);
  if (!locationAdded)
    return std::unexpected(std::move(locationAdded.error()));
  for (NetElement& element : std::initializer_list<std::reference_wrapper<NetElement>>{m_destination, m_xPosition, m_yPosition}) {
    auto added = m_netGroup.addNetElement(element);
    if (!added)
      return std::unexpected(std::move(added.error()));
  }
  auto enabled = m_netGroup.enableNetInterpolation();
  if (!enabled)
    return std::unexpected(std::move(enabled.error()));
  m_xPosition.setInterpolator(lerp<float, float>);
  m_yPosition.setInterpolator(lerp<float, float>);
  return {};
}


Uuid SystemClientShip::uuid() const {
  return m_uuid;
}

Vec2F SystemClientShip::position() const {
  return {m_xPosition.get(), m_yPosition.get()};
}

SystemLocation SystemClientShip::systemLocation() const {
  return m_systemLocation.get();
}

SystemLocation SystemClientShip::destination() const {
  return m_destination.get();
}

void SystemClientShip::setDestination(SystemLocation const& destination) {
  auto location = m_systemLocation.get();
  if (std::holds_alternative<CelestialCoordinate>(location) || std::holds_alternative<Uuid>(location))
    m_departTimer = m_config.departTime;
  else if (std::holds_alternative<std::monostate>(m_destination.get()))
    m_departTimer = m_config.spaceDepartTime;
  m_destination.set(destination);
  m_systemLocation.set({});
}

void SystemClientShip::setSpeed(float speed) {
  m_speed = speed;
}

bool SystemClientShip::flying() const {
  return std::holds_alternative<std::monostate>(m_systemLocation.get());
}

std::expected<void, StarException> SystemClientShip::clientUpdate(float dt) {
  return m_netGroup.tickNetInterpolation(dt);
}

std::expected<void, StarException> SystemClientShip::serverUpdate(SystemWorld& system, float dt) {
  auto nearPlanetOrbit = [this, &system](CelestialCoordinate const& planet) -> std::expected<CelestialOrbit, StarException> {
    auto position = system.planetPosition(planet);
    if (!position)
      return std::unexpected(std::move(position.error()));
    auto size = system.planetSize(planet);
    if (!size)
      return std::unexpected(std::move(size.error()));
    Vec2F toShip = *position - this->position();
    return CelestialOrbit{planet, 1, system.time(),
                          Vec2F::with_angle(toShip.angle(), *size / 2.0 + m_config.orbitDistance)};
  };
  if (std::holds_alternative<CelestialCoordinate>(m_systemLocation.get())) {
    auto const& coordinate = std::get<CelestialCoordinate>(m_systemLocation.get());
    if (!m_orbit || m_orbit->target != coordinate) {
      auto orbit = nearPlanetOrbit(coordinate);
      if (!orbit)
        return std::unexpected(std::move(orbit.error()));
      m_orbit = std::move(*orbit);
    }
  } else if (std::holds_alternative<std::monostate>(m_systemLocation.get())) {
    m_departTimer = std::max(0.0f, m_departTimer - dt);
    if (m_departTimer > 0.0f)
      return {};
    if (std::holds_alternative<CelestialCoordinate>(m_destination.get())) {
      auto const& coordinate = std::get<CelestialCoordinate>(m_destination.get());
      if (!m_orbit || m_orbit->target != coordinate) {
        auto orbit = nearPlanetOrbit(coordinate);
        if (!orbit)
          return std::unexpected(std::move(orbit.error()));
        m_orbit = std::move(*orbit);
      }
    } else {
      m_orbit.reset();
    }
    Vec2F pos = position();
    Vec2F destination;
    if (m_orbit) {
      m_orbit->enterTime = system.time();
      auto position = system.orbitPosition(*m_orbit);
      if (!position)
        return std::unexpected(std::move(position.error()));
      destination = *position;
    } else {
      auto position = system.systemLocationPosition(m_destination.get());
      if (!position)
        return std::unexpected(std::move(position.error()));
      destination = position->value_or(pos);
    }
    auto toTarget = destination - pos;
    pos += toTarget.normalized() * (m_speed * dt);
    if (destination == pos || (destination - pos).normalized() * toTarget.normalized() < 0) {
      m_systemLocation.set(m_destination.get());
      m_destination.set({});
    } else {
      setPosition(pos);
      return {};
    }
  }
  auto position = system.systemLocationPosition(m_orbit ? SystemLocation(*m_orbit) : m_systemLocation.get());
  if (!position)
    return std::unexpected(std::move(position.error()));
  if (!*position)
    return std::unexpected(StarException("System client ship location has no position"));
  setPosition(**position);
  return {};
}

std::expected<std::pair<ByteArray, std::uint64_t>, StarException> SystemClientShip::writeNetState(std::uint64_t fromVersion, NetCompatibilityRules rules) {
  return m_netGroup.writeNetState(fromVersion, rules);
}

std::expected<void, StarException> SystemClientShip::readNetState(ByteArray data, float interpolationTime, NetCompatibilityRules rules) {
  return m_netGroup.readNetState(std::move(data), interpolationTime, rules);
}

std::expected<ByteArray, StarException> SystemClientShip::netStore() const {
  DataStreamBuffer ds;
  auto field0 = ds.write(m_uuid);
  if (!field0)
    return std::unexpected(std::move(field0.error()));
  
  auto field1 = ds.write(m_systemLocation.get());
  if (!field1)
    return std::unexpected(std::move(field1.error()));

  return ds.take_data();
}

void SystemClientShip::setPosition(Vec2F const& position) {
  m_xPosition.set(position[0]);
  m_yPosition.set(position[1]);
}

}

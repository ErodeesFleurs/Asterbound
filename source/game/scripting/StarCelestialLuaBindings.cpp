module;
import star.xx_hash;
import star.json;
import star.json_extra;
import star.lua;
import star.format;
import star.string;
import star.either;
import star.data_stream;
import star.strong_typedef;
import star.ordered_set;
import star.io_device;
import star.thread;
import star.byte_array;
import star.data_stream_devices;

import std;
using ZSTD_CCtx = struct ZSTD_CCtx_s;
using ZSTD_DCtx = struct ZSTD_DCtx_s;
using ZSTD_DStream = ZSTD_DCtx;
using ZSTD_CStream = ZSTD_CCtx;
import star.zstd_compression;
import star.net_compatibility;
import std;
import star.id_map;
import star.list;
import star.version;
import star.ordered_map;
import star.rect;
import star.casting;
import star.set;
import star.vector;
import star.worker_pool;

import star.sector_array_2d;
import star.bi_map;
import star.random;
import star.perlin;
import star.btree;
import star.lru_cache;
import star.btree_database;
import star.rpc_promise;
import star.image;
import star.audio;
import star.map;
import star.game_types;
import star.color;
import star.weighted_pool;
import star.image_processing;
import star.hash;
import star.string_view;
import star.directives;
import star.asset_path;
import star.ref_ptr;
import star.logging;
import star.listener;


// Match client include order for SIMD intrinsics used by xxhash.
import star.lua_game_converters;
import star.host_address;
import star.chat_types;
import star.uuid;
import star.warping;
import star.item_descriptor;
import star.quest_descriptor;
import star.ai_types;
import star.socket;
import star.tcp;
import star.p2p_networking_service;
import star.collision_block;
import star.liquid_types;
import star.tile_damage;
import star.tile_sector_array;
import star.world_layout;
import star.collision_generator;
import star.world_tiles;
import star.celestial_types;
import star.tile_modification;
import star.damage_types;
import star.status_types;
import star.damage;
import star.light_source;
import star.entity;
import star.interaction_types;
import star.world_geometry;
import star.wiring;
import star.interactive_entity;
import star.tile_entity;
import star.inspectable_entity;
import star.plant;
import star.plant_database;
import star.biome_placement;
import star.lua_root;
import star.versioning_database;
import star.world_storage;
import star.player_types;
import star.sky_parameters;
import star.system_world;
import star.damage_manager;
import star.net_packets;
import star.net_packet_socket;
import star.universe_connection;
import star.drawable;
import star.entity_rendering_types;
import star.sky_render_data;
import star.parallax;
import star.cellular_light_array;
import star.cellular_lighting;
import star.world_render_data;
import star.world_structure;
import star.chat_action;
import star.mixer;
import star.entity_rendering;
import star.world;
import star.lua_components;
import star.world_client_state;
import star.interpolation_tracker;
import star.ambient;
import star.weather;
import star.game_timers;
import star.world_client;
import star.world_client_thread;
import star.universe;
// TODO: make this more thread safe
import star.universe_client;
import star.system_world_client;
import star.celestial_coordinate;
import star.sky_types;
import star.animation;
import star.particle;
import star.weather_types;
import star.force_regions;
import star.world_parameters;
import star.celestial_parameters;
import star.celestial_graphics;
import star.asset_source;
import star.assets;
import star.root_base;
import star.configuration;
import star.root;

module star.celestial_lua_bindings;
import star.biome_database;
import star.celestial_database;
import star.sky;


namespace Star {
std::expected<LuaCallbacks, StarException> LuaBindings::makeCelestialCallbacks(Root& root, std::shared_ptr<CelestialDatabase> celestialDatabase) {
  LuaCallbacks callbacks;
  
  if (auto registration = callbacks.registerCallback("planetParameters", [celestialDatabase](Json const& coords) -> std::expected<Json, StarException> {
    auto coordinate = CelestialCoordinate::create(coords);
    if (!coordinate)
      return std::unexpected(std::move(coordinate.error()));
    auto parameters = celestialDatabase->parameters(*coordinate);
    if (!parameters)
      return std::unexpected(std::move(parameters.error()));
    if (*parameters)
      return (*parameters)->parameters();
    return {};
  }); !registration)
      return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.registerCallback("visitableParameters", [celestialDatabase](Json const& coords) -> std::expected<Json, StarException> {
    auto coordinate = CelestialCoordinate::create(coords);
    if (!coordinate)
      return std::unexpected(std::move(coordinate.error()));
    auto parameters = celestialDatabase->parameters(*coordinate);
    if (!parameters)
      return std::unexpected(std::move(parameters.error()));
    if (*parameters)
      if (auto visitable = (*parameters)->visitableParameters())
        return visitable->store();
    return Json();
  }); !registration)
      return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.registerCallback("planetName", [celestialDatabase](Json const& coords) -> std::expected<std::optional<String>, StarException> {
    auto coordinate = CelestialCoordinate::create(coords);
    if (!coordinate)
      return std::unexpected(std::move(coordinate.error()));
    auto parameters = celestialDatabase->parameters(*coordinate);
    if (!parameters)
      return std::unexpected(std::move(parameters.error()));
    if (*parameters)
      return (*parameters)->name();
    return {};
  }); !registration)
      return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.registerCallback("planetSeed", [celestialDatabase](Json const& coords) -> std::expected<std::optional<std::uint64_t>, StarException> {
    auto coordinate = CelestialCoordinate::create(coords);
    if (!coordinate)
      return std::unexpected(std::move(coordinate.error()));
    auto parameters = celestialDatabase->parameters(*coordinate);
    if (!parameters)
      return std::unexpected(std::move(parameters.error()));
    if (*parameters)
      return (*parameters)->seed();
    return {};
  }); !registration)
      return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.registerCallback("planetOres", [&root, celestialDatabase](Json const& coords, float threatLevel) -> std::expected<List<String>, StarException> {
    auto coordinate = CelestialCoordinate::create(coords);
    if (!coordinate)
      return std::unexpected(std::move(coordinate.error()));
    auto parameters = celestialDatabase->parameters(*coordinate);
    if (!parameters)
      return std::unexpected(std::move(parameters.error()));
    if (!*parameters)
      return {};
    auto visitableParameters = (*parameters)->visitableParameters();
    if (!visitableParameters)
      return {};
    auto biomeDatabase = root.biomeDatabase();
    if (!biomeDatabase)
      return std::unexpected(std::move(biomeDatabase.error()));
    StringSet planetOres;
    auto addOres = [&](String const& biomeName) -> std::expected<void, StarException> {
      auto ores = (*biomeDatabase)->biomeOres(biomeName, threatLevel);
      if (!ores)
        return std::unexpected(std::move(ores.error()));
      planetOres.insert_range(*ores);
      return {};
    };
    if (auto terrestrial = as<TerrestrialWorldParameters>(visitableParameters)) {
      auto oreResult0 = addOres(terrestrial->primaryBiome);
      if (!oreResult0)
        return std::unexpected(std::move(oreResult0.error()));
      auto oreResult1 = addOres(terrestrial->surfaceLayer.primaryRegion.biome);
      if (!oreResult1)
        return std::unexpected(std::move(oreResult1.error()));
      auto oreResult2 = addOres(terrestrial->subsurfaceLayer.primaryRegion.biome);
      if (!oreResult2)
        return std::unexpected(std::move(oreResult2.error()));
      for (auto const& layer : terrestrial->undergroundLayers) {
        auto layerOres = addOres(layer.primaryRegion.biome);
        if (!layerOres)
          return std::unexpected(std::move(layerOres.error()));
      }
      auto oreResult3 = addOres(terrestrial->coreLayer.primaryRegion.biome);
      if (!oreResult3)
        return std::unexpected(std::move(oreResult3.error()));
    } else if (auto asteroid = as<AsteroidsWorldParameters>(visitableParameters)) {
      auto asteroidOres = addOres(asteroid->asteroidBiome);
      if (!asteroidOres)
        return std::unexpected(std::move(asteroidOres.error()));
    }
    return planetOres.values();
  }); !registration)
      return std::unexpected(std::move(registration.error()));

  if (auto registration = callbacks.registerCallback("hasChildren", [celestialDatabase](Json const& coords) -> std::expected<std::optional<bool>, StarException> {
    auto coordinate = CelestialCoordinate::create(coords);
    if (!coordinate)
      return std::unexpected(std::move(coordinate.error()));
    return celestialDatabase->hasChildren(*coordinate);
  }); !registration)
      return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.registerCallback("children", [celestialDatabase](Json const& coords) -> std::expected<List<Json>, StarException> {
    auto coordinate = CelestialCoordinate::create(coords);
    if (!coordinate)
      return std::unexpected(std::move(coordinate.error()));
    auto children = celestialDatabase->children(*coordinate);
    if (!children)
      return std::unexpected(std::move(children.error()));
    return children->transformed([](CelestialCoordinate const& c) { return c.toJson(); });
  }); !registration)
      return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.registerCallback("childOrbits", [celestialDatabase](Json const& coords) -> std::expected<List<int>, StarException> {
    auto coordinate = CelestialCoordinate::create(coords);
    if (!coordinate)
      return std::unexpected(std::move(coordinate.error()));
    return celestialDatabase->childOrbits(*coordinate);
  }); !registration)
      return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.registerCallback("scanSystems", [celestialDatabase](RectI const& region, std::optional<StringSet> const& includedTypes) -> std::expected<List<Json>, StarException> {
    auto systems = celestialDatabase->scanSystems(region, includedTypes);
    if (!systems)
      return std::unexpected(std::move(systems.error()));
    return systems->transformed([](CelestialCoordinate const& c) { return c.toJson(); });
  }); !registration)
      return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.registerCallback("scanConstellationLines", [celestialDatabase](RectI const& region) -> std::expected<List<std::pair<Vec2I, Vec2I>>, StarException> {
    return celestialDatabase->scanConstellationLines(region);
  }); !registration)
      return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.registerCallback("scanRegionFullyLoaded", [celestialDatabase](RectI const& region) -> std::expected<bool, StarException> {
    return celestialDatabase->scanRegionFullyLoaded(region);
  }); !registration)
      return std::unexpected(std::move(registration.error()));

  if (auto registration = callbacks.registerCallback("centralBodyImages", [celestialDatabase](Json const& coords) -> std::expected<List<std::pair<String, float>>, StarException> {
    auto coordinate = CelestialCoordinate::create(coords);
    if (!coordinate)
      return std::unexpected(std::move(coordinate.error()));
    return CelestialGraphics::drawSystemCentralBody(celestialDatabase, *coordinate);
  }); !registration)
      return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.registerCallback("planetaryObjectImages", [celestialDatabase](Json const& coords) -> std::expected<List<std::pair<String, float>>, StarException> {
    auto coordinate = CelestialCoordinate::create(coords);
    if (!coordinate)
      return std::unexpected(std::move(coordinate.error()));
    return CelestialGraphics::drawSystemPlanetaryObject(celestialDatabase, *coordinate);
  }); !registration)
      return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.registerCallback("worldImages", [&root, celestialDatabase](Json const& coords) -> std::expected<List<std::pair<String, float>>, StarException> {
    auto coordinate = CelestialCoordinate::create(coords);
    if (!coordinate)
      return std::unexpected(std::move(coordinate.error()));
    return CelestialGraphics::drawWorld(root, celestialDatabase, *coordinate);
  }); !registration)
      return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.registerCallback("starImages", [&root, celestialDatabase](Json const& coords, float twinkleTime) -> std::expected<List<std::pair<String, float>>, StarException> {
    auto coordinate = CelestialCoordinate::create(coords);
    if (!coordinate)
      return std::unexpected(std::move(coordinate.error()));
    return CelestialGraphics::drawSystemTwinkle(root, celestialDatabase, *coordinate, twinkleTime);
  }); !registration)
      return std::unexpected(std::move(registration.error()));
  
  return callbacks;
}

std::expected<LuaCallbacks, StarException> LuaBindings::makeCelestialCallbacks(Root& root, Universe& universe) {
  auto celestialDatabase = universe.celestialDatabase();
  
  auto createdCallbacks = makeCelestialCallbacks(root, celestialDatabase);
  if (!createdCallbacks)
    return std::unexpected(std::move(createdCallbacks.error()));
  LuaCallbacks callbacks = std::move(*createdCallbacks);

  if (auto client = as_ref<UniverseClient>(universe)) {
    auto systemWorld = client->get().systemWorldClient();

    if (auto registration = callbacks.registerCallback("skyFlying", [client]() {
        return client->get().currentSky()->flying();
      }); !registration)
      return std::unexpected(std::move(registration.error()));
    if (auto registration = callbacks.registerCallback("skyFlyingType", [client]() -> std::expected<String, StarException> {
    if (!FlyingTypeNames)
      return std::unexpected(FlyingTypeNames.error());
    auto name = FlyingTypeNames->get_right(client->get().currentSky()->flyingType());
    if (!name)
      return std::unexpected(std::move(name.error()));
    return name->get();
  }); !registration)
      return std::unexpected(std::move(registration.error()));
    if (auto registration = callbacks.registerCallback("skyWarpPhase", [client]() -> std::expected<String, StarException> {
    if (!WarpPhaseNames)
      return std::unexpected(WarpPhaseNames.error());
    auto name = WarpPhaseNames->get_right(client->get().currentSky()->warpPhase());
    if (!name)
      return std::unexpected(std::move(name.error()));
    return name->get();
  }); !registration)
      return std::unexpected(std::move(registration.error()));
    if (auto registration = callbacks.registerCallback("skyWarpProgress", [client]() -> float {
        return client->get().currentSky()->warpProgress();
      }); !registration)
      return std::unexpected(std::move(registration.error()));
    if (auto registration = callbacks.registerCallback("skyInHyperspace", [client]() -> bool {
        return client->get().currentSky()->inHyperspace();
      }); !registration)
      return std::unexpected(std::move(registration.error()));

    if (auto registration = callbacks.registerCallback("flyShip", [client,systemWorld](Vec3I const& system, Json const& destination, Json const& settings)  -> std::expected<void, StarException> {
    auto location = jsonToSystemLocation(destination);
    if (!location)
      return std::unexpected(std::move(location.error()));
    return client->get().flyShip(system, *location, settings);
  }); !registration)
      return std::unexpected(std::move(registration.error()));
    if (auto registration = callbacks.registerCallback("flying", [systemWorld]() {
        return systemWorld->flying();
      }); !registration)
      return std::unexpected(std::move(registration.error()));
    if (auto registration = callbacks.registerCallback("shipSystemPosition", [systemWorld]() -> std::optional<Vec2F> {
        return systemWorld->shipPosition();
      }); !registration)
      return std::unexpected(std::move(registration.error()));
    if (auto registration = callbacks.registerCallback("shipDestination", [systemWorld]() -> std::expected<Json, StarException> {
    return jsonFromSystemLocation(systemWorld->shipDestination());
  }); !registration)
      return std::unexpected(std::move(registration.error()));
    if (auto registration = callbacks.registerCallback("shipLocation", [systemWorld]() -> std::expected<Json, StarException> {
    return jsonFromSystemLocation(systemWorld->shipLocation());
  }); !registration)
      return std::unexpected(std::move(registration.error()));
    if (auto registration = callbacks.registerCallback("currentSystem", [systemWorld]() -> Json {
        return systemWorld->currentSystem().toJson();
      }); !registration)
      return std::unexpected(std::move(registration.error()));

    if (auto registration = callbacks.registerCallback("planetSize", [systemWorld](Json const& coords) -> std::expected<float, StarException> {
    auto coordinate = CelestialCoordinate::create(coords);
    if (!coordinate)
      return std::unexpected(std::move(coordinate.error()));
    return systemWorld->planetSize(*coordinate);
  }); !registration)
      return std::unexpected(std::move(registration.error()));
    if (auto registration = callbacks.registerCallback("planetPosition", [systemWorld](Json const& coords) -> std::expected<Vec2F, StarException> {
    auto coordinate = CelestialCoordinate::create(coords);
    if (!coordinate)
      return std::unexpected(std::move(coordinate.error()));
    return systemWorld->planetPosition(*coordinate);
  }); !registration)
      return std::unexpected(std::move(registration.error()));
    if (auto registration = callbacks.registerCallback("clusterSize", [systemWorld](Json const& coords) -> std::expected<float, StarException> {
    auto coordinate = CelestialCoordinate::create(coords);
    if (!coordinate)
      return std::unexpected(std::move(coordinate.error()));
    return systemWorld->clusterSize(*coordinate);
  }); !registration)
      return std::unexpected(std::move(registration.error()));

    if (auto registration = callbacks.registerCallback("systemPosition", [systemWorld](Json const& l) -> std::expected<std::optional<Vec2F>, StarException> {
    auto location = jsonToSystemLocation(l);
    if (!location)
      return std::unexpected(std::move(location.error()));
    return systemWorld->systemLocationPosition(*location);
  }); !registration)
      return std::unexpected(std::move(registration.error()));
    if (auto registration = callbacks.registerCallback("orbitPosition", [systemWorld](Json const& orbit) -> std::expected<Vec2F, StarException> {
    auto parsedOrbit = CelestialOrbit::fromJson(orbit);
    if (!parsedOrbit)
      return std::unexpected(std::move(parsedOrbit.error()));
    return systemWorld->orbitPosition(*parsedOrbit);
  }); !registration)
      return std::unexpected(std::move(registration.error()));

    if (auto registration = callbacks.registerCallback("systemObjects", [systemWorld]() -> List<String> {
        return systemWorld->objects().transformed([](std::shared_ptr<SystemObject> object) { return object->uuid().hex(); });
      }); !registration)
      return std::unexpected(std::move(registration.error()));
    if (auto registration = callbacks.registerCallback("objectType", [systemWorld](String const& uuid) -> std::expected<std::optional<String>, StarException> {
    auto id = Uuid::create(uuid);
    if (!id)
      return std::unexpected(std::move(id.error()));
    if (auto object = systemWorld->getObject(*id))
      return object->name();
    return {};
  }); !registration)
      return std::unexpected(std::move(registration.error()));
    if (auto registration = callbacks.registerCallback("objectParameters", [systemWorld](String const& uuid) -> std::expected<Json, StarException> {
    auto id = Uuid::create(uuid);
    if (!id)
      return std::unexpected(std::move(id.error()));
    if (auto object = systemWorld->getObject(*id))
      return object->parameters();
    return {};
  }); !registration)
      return std::unexpected(std::move(registration.error()));
    if (auto registration = callbacks.registerCallback("objectWarpActionWorld", [systemWorld](String const& uuid) -> std::expected<std::optional<String>, StarException> {
    auto id = Uuid::create(uuid);
    if (!id)
      return std::unexpected(std::move(id.error()));
    auto action = systemWorld->objectWarpAction(*id);
    if (!action)
      return std::unexpected(std::move(action.error()));
    if (*action)
      if (std::holds_alternative<WarpToWorld>(**action))
        return Star::print_world_id(std::get<WarpToWorld>(**action).world);
    return {};
  }); !registration)
      return std::unexpected(std::move(registration.error()));
    if (auto registration = callbacks.registerCallback("objectOrbit", [systemWorld](String const& uuid) -> std::expected<Json, StarException> {
    auto id = Uuid::create(uuid);
    if (!id)
      return std::unexpected(std::move(id.error()));
    if (auto object = systemWorld->getObject(*id))
      return jsonFromOptional<CelestialOrbit>(object->orbit(), [](CelestialOrbit const& orbit) { return orbit.toJson(); });
    return Json();
  }); !registration)
      return std::unexpected(std::move(registration.error()));
    if (auto registration = callbacks.registerCallback("objectPosition", [systemWorld](String const& uuid) -> std::expected<std::optional<Vec2F>, StarException> {
    auto id = Uuid::create(uuid);
    if (!id)
      return std::unexpected(std::move(id.error()));
    if (auto object = systemWorld->getObject(*id))
      return object->position();
    return {};
  }); !registration)
      return std::unexpected(std::move(registration.error()));
    if (auto registration = callbacks.registerCallback("objectTypeConfig", [&root](String const& typeName) -> std::expected<Json, StarException> {
    return SystemWorld::systemObjectTypeConfig(root, typeName);
  }); !registration)
      return std::unexpected(std::move(registration.error()));
    if (auto registration = callbacks.registerCallback("systemSpawnObject", [systemWorld](String const& typeName, std::optional<Vec2F> const& position, std::optional<String> uuidHex, std::optional<JsonObject> parameters) -> std::expected<String, StarException> {
    std::optional<Uuid> uuid;
    if (uuidHex) {
      auto parsed = Uuid::create(*uuidHex);
      if (!parsed)
        return std::unexpected(std::move(parsed.error()));
      uuid = *parsed;
    }
    return systemWorld->spawnObject(typeName, position, uuid, parameters.value_or(JsonObject{})).hex();
  }); !registration)
      return std::unexpected(std::move(registration.error()));

    if (auto registration = callbacks.registerCallback("playerShips", [systemWorld]() -> List<String> {
        return systemWorld->ships().transformed([](std::shared_ptr<SystemClientShip> ship) { return ship->uuid().hex(); });
      }); !registration)
      return std::unexpected(std::move(registration.error()));
    if (auto registration = callbacks.registerCallback("playerShipPosition", [systemWorld](String const& uuid) -> std::expected<std::optional<Vec2F>, StarException> {
    auto id = Uuid::create(uuid);
    if (!id)
      return std::unexpected(std::move(id.error()));
    if (auto ship = systemWorld->getShip(*id))
      return ship->position();
    return {};
  }); !registration)
      return std::unexpected(std::move(registration.error()));
  }

  return callbacks;
}

}

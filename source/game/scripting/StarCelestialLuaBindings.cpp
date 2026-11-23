module;
import star.xx_hash;
import star.json;
import star.json_extra;
import star.lua;
import star.format;
import star.string;
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
std::expected<LuaCallbacks, StarException> LuaBindings::make_celestial_callbacks(Root& root, std::shared_ptr<CelestialDatabase> celestial_database) {
  LuaCallbacks callbacks;
  
  if (auto registration = callbacks.register_callback("planetParameters", [celestial_database](Json const& coords) -> std::expected<Json, StarException> {
    auto coordinate = CelestialCoordinate::create(coords);
    if (!coordinate)
      return std::unexpected(std::move(coordinate.error()));
    auto parameters = celestial_database->parameters(*coordinate);
    if (!parameters)
      return std::unexpected(std::move(parameters.error()));
    if (*parameters)
      return (*parameters)->parameters();
    return {};
  }); !registration)
      return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback("visitableParameters", [celestial_database](Json const& coords) -> std::expected<Json, StarException> {
    auto coordinate = CelestialCoordinate::create(coords);
    if (!coordinate)
      return std::unexpected(std::move(coordinate.error()));
    auto parameters = celestial_database->parameters(*coordinate);
    if (!parameters)
      return std::unexpected(std::move(parameters.error()));
    if (*parameters)
      if (auto visitable = (*parameters)->visitable_parameters())
        return visitable->store();
    return Json();
  }); !registration)
      return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback("planetName", [celestial_database](Json const& coords) -> std::expected<std::optional<String>, StarException> {
    auto coordinate = CelestialCoordinate::create(coords);
    if (!coordinate)
      return std::unexpected(std::move(coordinate.error()));
    return celestial_database->name(*coordinate);
  }); !registration)
      return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback("planetSeed", [celestial_database](Json const& coords) -> std::expected<std::optional<std::uint64_t>, StarException> {
    auto coordinate = CelestialCoordinate::create(coords);
    if (!coordinate)
      return std::unexpected(std::move(coordinate.error()));
    auto parameters = celestial_database->parameters(*coordinate);
    if (!parameters)
      return std::unexpected(std::move(parameters.error()));
    if (*parameters)
      return (*parameters)->seed();
    return {};
  }); !registration)
      return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback("planetOres", [&root, celestial_database](Json const& coords, float threat_level) -> std::expected<List<String>, StarException> {
    auto coordinate = CelestialCoordinate::create(coords);
    if (!coordinate)
      return std::unexpected(std::move(coordinate.error()));
    auto parameters = celestial_database->parameters(*coordinate);
    if (!parameters)
      return std::unexpected(std::move(parameters.error()));
    if (!*parameters)
      return {};
    auto visitable_parameters = (*parameters)->visitable_parameters();
    if (!visitable_parameters)
      return {};
    auto biome_database = root.biome_database();
    if (!biome_database)
      return std::unexpected(std::move(biome_database.error()));
    StringSet planet_ores;
    auto add_ores = [&](String const& biome_name) -> std::expected<void, StarException> {
      auto ores = (*biome_database)->biome_ores(biome_name, threat_level);
      if (!ores)
        return std::unexpected(std::move(ores.error()));
      planet_ores.insert_range(*ores);
      return {};
    };
    if (auto terrestrial = as<TerrestrialWorldParameters>(visitable_parameters)) {
      auto ore_result0 = add_ores(terrestrial->primaryBiome);
      if (!ore_result0)
        return std::unexpected(std::move(ore_result0.error()));
      auto ore_result1 = add_ores(terrestrial->surfaceLayer.primaryRegion.biome);
      if (!ore_result1)
        return std::unexpected(std::move(ore_result1.error()));
      auto ore_result2 = add_ores(terrestrial->subsurfaceLayer.primaryRegion.biome);
      if (!ore_result2)
        return std::unexpected(std::move(ore_result2.error()));
      for (auto const& layer : terrestrial->undergroundLayers) {
        auto layer_ores = add_ores(layer.primaryRegion.biome);
        if (!layer_ores)
          return std::unexpected(std::move(layer_ores.error()));
      }
      auto ore_result3 = add_ores(terrestrial->coreLayer.primaryRegion.biome);
      if (!ore_result3)
        return std::unexpected(std::move(ore_result3.error()));
    } else if (auto asteroid = as<AsteroidsWorldParameters>(visitable_parameters)) {
      auto asteroid_ores = add_ores(asteroid->asteroidBiome);
      if (!asteroid_ores)
        return std::unexpected(std::move(asteroid_ores.error()));
    }
    return planet_ores.values();
  }); !registration)
      return std::unexpected(std::move(registration.error()));

  if (auto registration = callbacks.register_callback("hasChildren", [celestial_database](Json const& coords) -> std::expected<std::optional<bool>, StarException> {
    auto coordinate = CelestialCoordinate::create(coords);
    if (!coordinate)
      return std::unexpected(std::move(coordinate.error()));
    return celestial_database->has_children(*coordinate);
  }); !registration)
      return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback("children", [celestial_database](Json const& coords) -> std::expected<List<Json>, StarException> {
    auto coordinate = CelestialCoordinate::create(coords);
    if (!coordinate)
      return std::unexpected(std::move(coordinate.error()));
    auto children = celestial_database->children(*coordinate);
    if (!children)
      return std::unexpected(std::move(children.error()));
    return children->transformed([](CelestialCoordinate const& c) { return c.to_json(); });
  }); !registration)
      return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback("childOrbits", [celestial_database](Json const& coords) -> std::expected<List<int>, StarException> {
    auto coordinate = CelestialCoordinate::create(coords);
    if (!coordinate)
      return std::unexpected(std::move(coordinate.error()));
    return celestial_database->child_orbits(*coordinate);
  }); !registration)
      return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback("scanSystems", [celestial_database](RectI const& region, std::optional<StringSet> const& included_types) -> std::expected<List<Json>, StarException> {
    auto systems = celestial_database->scan_systems(region, included_types);
    if (!systems)
      return std::unexpected(std::move(systems.error()));
    return systems->transformed([](CelestialCoordinate const& c) { return c.to_json(); });
  }); !registration)
      return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback("scanConstellationLines", [celestial_database](RectI const& region) -> std::expected<List<std::pair<Vec2I, Vec2I>>, StarException> {
    return celestial_database->scan_constellation_lines(region);
  }); !registration)
      return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback("scanRegionFullyLoaded", [celestial_database](RectI const& region) -> std::expected<bool, StarException> {
    return celestial_database->scan_region_fully_loaded(region);
  }); !registration)
      return std::unexpected(std::move(registration.error()));

  if (auto registration = callbacks.register_callback("centralBodyImages", [celestial_database](Json const& coords) -> std::expected<List<std::pair<String, float>>, StarException> {
    auto coordinate = CelestialCoordinate::create(coords);
    if (!coordinate)
      return std::unexpected(std::move(coordinate.error()));
    return CelestialGraphics::draw_system_central_body(*celestial_database, *coordinate);
  }); !registration)
      return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback("planetaryObjectImages", [celestial_database](Json const& coords) -> std::expected<List<std::pair<String, float>>, StarException> {
    auto coordinate = CelestialCoordinate::create(coords);
    if (!coordinate)
      return std::unexpected(std::move(coordinate.error()));
    return CelestialGraphics::draw_system_planetary_object(*celestial_database, *coordinate);
  }); !registration)
      return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback("worldImages", [&root, celestial_database](Json const& coords) -> std::expected<List<std::pair<String, float>>, StarException> {
    auto coordinate = CelestialCoordinate::create(coords);
    if (!coordinate)
      return std::unexpected(std::move(coordinate.error()));
    return CelestialGraphics::draw_world(root, *celestial_database, *coordinate);
  }); !registration)
      return std::unexpected(std::move(registration.error()));
  if (auto registration = callbacks.register_callback("starImages", [&root, celestial_database](Json const& coords, float twinkle_time) -> std::expected<List<std::pair<String, float>>, StarException> {
    auto coordinate = CelestialCoordinate::create(coords);
    if (!coordinate)
      return std::unexpected(std::move(coordinate.error()));
    return CelestialGraphics::draw_system_twinkle(root, *celestial_database, *coordinate, twinkle_time);
  }); !registration)
      return std::unexpected(std::move(registration.error()));
  
  return callbacks;
}

std::expected<LuaCallbacks, StarException> LuaBindings::make_celestial_callbacks(Root& root, Universe& universe) {
  auto created_callbacks = make_celestial_callbacks(root, universe.celestial_database());
  if (!created_callbacks)
    return std::unexpected(std::move(created_callbacks.error()));
  LuaCallbacks callbacks = std::move(*created_callbacks);

  if (auto client = as_ref<UniverseClient>(universe)) {
    auto system_world = client->get().systemWorldClient();

    if (auto registration = callbacks.register_callback("skyFlying", [client]() {
        return client->get().currentSky()->flying();
      }); !registration)
      return std::unexpected(std::move(registration.error()));
    if (auto registration = callbacks.register_callback("skyFlyingType", [client]() -> std::expected<String, StarException> {
    if (!FlyingTypeNames)
      return std::unexpected(FlyingTypeNames.error());
    auto name = FlyingTypeNames->get_right(client->get().currentSky()->flyingType());
    if (!name)
      return std::unexpected(std::move(name.error()));
    return name->get();
  }); !registration)
      return std::unexpected(std::move(registration.error()));
    if (auto registration = callbacks.register_callback("skyWarpPhase", [client]() -> std::expected<String, StarException> {
    if (!WarpPhaseNames)
      return std::unexpected(WarpPhaseNames.error());
    auto name = WarpPhaseNames->get_right(client->get().currentSky()->warpPhase());
    if (!name)
      return std::unexpected(std::move(name.error()));
    return name->get();
  }); !registration)
      return std::unexpected(std::move(registration.error()));
    if (auto registration = callbacks.register_callback("skyWarpProgress", [client]() -> float {
        return client->get().currentSky()->warpProgress();
      }); !registration)
      return std::unexpected(std::move(registration.error()));
    if (auto registration = callbacks.register_callback("skyInHyperspace", [client]() -> bool {
        return client->get().currentSky()->inHyperspace();
      }); !registration)
      return std::unexpected(std::move(registration.error()));

    if (auto registration = callbacks.register_callback("flyShip", [client](Vec3I const& system, Json const& destination, Json const& settings)  -> std::expected<void, StarException> {
    auto location = jsonToSystemLocation(destination);
    if (!location)
      return std::unexpected(std::move(location.error()));
    return client->get().flyShip(system, *location, settings);
  }); !registration)
      return std::unexpected(std::move(registration.error()));
    if (auto registration = callbacks.register_callback("flying", [system_world]() {
        return system_world->flying();
      }); !registration)
      return std::unexpected(std::move(registration.error()));
    if (auto registration = callbacks.register_callback("shipSystemPosition", [system_world]() -> std::optional<Vec2F> {
        return system_world->shipPosition();
      }); !registration)
      return std::unexpected(std::move(registration.error()));
    if (auto registration = callbacks.register_callback("shipDestination", [system_world]() -> std::expected<Json, StarException> {
    return jsonFromSystemLocation(system_world->shipDestination());
  }); !registration)
      return std::unexpected(std::move(registration.error()));
    if (auto registration = callbacks.register_callback("shipLocation", [system_world]() -> std::expected<Json, StarException> {
    return jsonFromSystemLocation(system_world->shipLocation());
  }); !registration)
      return std::unexpected(std::move(registration.error()));
    if (auto registration = callbacks.register_callback("currentSystem", [system_world]() -> Json {
        return system_world->currentSystem().to_json();
      }); !registration)
      return std::unexpected(std::move(registration.error()));

    if (auto registration = callbacks.register_callback("planetSize", [system_world](Json const& coords) -> std::expected<float, StarException> {
    auto coordinate = CelestialCoordinate::create(coords);
    if (!coordinate)
      return std::unexpected(std::move(coordinate.error()));
    return system_world->planetSize(*coordinate);
  }); !registration)
      return std::unexpected(std::move(registration.error()));
    if (auto registration = callbacks.register_callback("planetPosition", [system_world](Json const& coords) -> std::expected<Vec2F, StarException> {
    auto coordinate = CelestialCoordinate::create(coords);
    if (!coordinate)
      return std::unexpected(std::move(coordinate.error()));
    return system_world->planetPosition(*coordinate);
  }); !registration)
      return std::unexpected(std::move(registration.error()));
    if (auto registration = callbacks.register_callback("clusterSize", [system_world](Json const& coords) -> std::expected<float, StarException> {
    auto coordinate = CelestialCoordinate::create(coords);
    if (!coordinate)
      return std::unexpected(std::move(coordinate.error()));
    return system_world->clusterSize(*coordinate);
  }); !registration)
      return std::unexpected(std::move(registration.error()));

    if (auto registration = callbacks.register_callback("systemPosition", [system_world](Json const& l) -> std::expected<std::optional<Vec2F>, StarException> {
    auto location = jsonToSystemLocation(l);
    if (!location)
      return std::unexpected(std::move(location.error()));
    return system_world->systemLocationPosition(*location);
  }); !registration)
      return std::unexpected(std::move(registration.error()));
    if (auto registration = callbacks.register_callback("orbitPosition", [system_world](Json const& orbit) -> std::expected<Vec2F, StarException> {
    auto parsed_orbit = CelestialOrbit::fromJson(orbit);
    if (!parsed_orbit)
      return std::unexpected(std::move(parsed_orbit.error()));
    return system_world->orbitPosition(*parsed_orbit);
  }); !registration)
      return std::unexpected(std::move(registration.error()));

    if (auto registration = callbacks.register_callback("systemObjects", [system_world]() -> List<String> {
        return system_world->objects().transformed([](std::shared_ptr<SystemObject> const& object) { return object->uuid().hex(); });
      }); !registration)
      return std::unexpected(std::move(registration.error()));
    if (auto registration = callbacks.register_callback("objectType", [system_world](String const& uuid) -> std::expected<std::optional<String>, StarException> {
    auto id = Uuid::from_hex((uuid).utf8());
    if (!id)
      return std::unexpected(std::move(id.error()));
    if (auto object = system_world->getObject(*id))
      return object->name();
    return {};
  }); !registration)
      return std::unexpected(std::move(registration.error()));
    if (auto registration = callbacks.register_callback("objectParameters", [system_world](String const& uuid) -> std::expected<Json, StarException> {
    auto id = Uuid::from_hex((uuid).utf8());
    if (!id)
      return std::unexpected(std::move(id.error()));
    if (auto object = system_world->getObject(*id))
      return object->parameters();
    return {};
  }); !registration)
      return std::unexpected(std::move(registration.error()));
    if (auto registration = callbacks.register_callback("objectWarpActionWorld", [system_world](String const& uuid) -> std::expected<std::optional<String>, StarException> {
    auto id = Uuid::from_hex((uuid).utf8());
    if (!id)
      return std::unexpected(std::move(id.error()));
    auto action = system_world->objectWarpAction(*id);
    if (!action)
      return std::unexpected(std::move(action.error()));
    if (*action)
      if (std::holds_alternative<WarpToWorld>(**action))
        return Star::print_world_id(std::get<WarpToWorld>(**action).world);
    return {};
  }); !registration)
      return std::unexpected(std::move(registration.error()));
    if (auto registration = callbacks.register_callback("objectOrbit", [system_world](String const& uuid) -> std::expected<Json, StarException> {
    auto id = Uuid::from_hex((uuid).utf8());
    if (!id)
      return std::unexpected(std::move(id.error()));
    if (auto object = system_world->getObject(*id))
      return json_from_optional<CelestialOrbit>(object->orbit(), [](CelestialOrbit const& orbit) { return orbit.toJson(); });
    return Json();
  }); !registration)
      return std::unexpected(std::move(registration.error()));
    if (auto registration = callbacks.register_callback("objectPosition", [system_world](String const& uuid) -> std::expected<std::optional<Vec2F>, StarException> {
    auto id = Uuid::from_hex((uuid).utf8());
    if (!id)
      return std::unexpected(std::move(id.error()));
    if (auto object = system_world->getObject(*id))
      return object->position();
    return {};
  }); !registration)
      return std::unexpected(std::move(registration.error()));
    if (auto registration = callbacks.register_callback("objectTypeConfig", [&root](String const& type_name) -> std::expected<Json, StarException> {
    return SystemWorld::systemObjectTypeConfig(root, type_name);
  }); !registration)
      return std::unexpected(std::move(registration.error()));
    if (auto registration = callbacks.register_callback("systemSpawnObject", [system_world](String const& type_name, std::optional<Vec2F> const& position, std::optional<String> uuid_hex, std::optional<JsonObject> parameters) -> std::expected<String, StarException> {
    std::optional<Uuid> uuid;
    if (uuid_hex) {
      auto parsed = Uuid::from_hex((*uuid_hex).utf8());
      if (!parsed)
        return std::unexpected(std::move(parsed.error()));
      uuid = *parsed;
    }
    return system_world->spawnObject(type_name, position, uuid, parameters.value_or(JsonObject{})).hex();
  }); !registration)
      return std::unexpected(std::move(registration.error()));

    if (auto registration = callbacks.register_callback("playerShips", [system_world]() -> List<String> {
        return system_world->ships().transformed([](std::shared_ptr<SystemClientShip> const& ship) { return ship->uuid().hex(); });
      }); !registration)
      return std::unexpected(std::move(registration.error()));
    if (auto registration = callbacks.register_callback("playerShipPosition", [system_world](String const& uuid) -> std::expected<std::optional<Vec2F>, StarException> {
    auto id = Uuid::from_hex((uuid).utf8());
    if (!id)
      return std::unexpected(std::move(id.error()));
    if (auto ship = system_world->getShip(*id))
      return ship->position();
    return {};
  }); !registration)
      return std::unexpected(std::move(registration.error()));
  }

  return callbacks;
}

}

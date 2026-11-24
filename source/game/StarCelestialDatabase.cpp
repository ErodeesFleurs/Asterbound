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

RectI CelestialDatabase::xy_range() const {
  auto range = m_base_information.xy_coord_range;
  return RectI(range[0], range[0], range[1], range[1]);
}

int CelestialDatabase::planet_orbital_levels() const {
  return m_base_information.planet_orbital_levels;
}

int CelestialDatabase::satellite_orbital_levels() const {
  return m_base_information.satellite_orbital_levels;
}

Vec2I CelestialDatabase::chunk_index_for(CelestialCoordinate const& coordinate) const {
  return chunk_index_for(coordinate.location().vec2());
}

Vec2I CelestialDatabase::chunk_index_for(Vec2I const& system_xy) const {
  auto floor_index = [size = m_base_information.chunk_size](int position) {
    return position / size - (position % size < 0);
  };
  return {floor_index(system_xy[0]), floor_index(system_xy[1])};
}

std::expected<void, StarException> CelestialDatabase::validate_base_information(CelestialBaseInformation const& info) {
  if (info.chunk_size <= 0)
    return std::unexpected(CelestialException("chunkSize must be positive"));
  if (info.z_coord_range[0] >= info.z_coord_range[1])
    return std::unexpected(CelestialException("zCoordRange must be a nonempty half-open interval"));
  return {};
}

bool CelestialDatabase::contains_system(RectI const& region, Vec2I const& position) {
  // Chunk enumeration is half-open; within a visited chunk, legacy scans
  // include systems exactly on the upper edge of the requested rectangle.
  return region.contains(position);
}

std::expected<RectI, StarException> CelestialDatabase::chunk_region(Vec2I const& chunk_index) const {
  Vec2I first, end;
  for (std::size_t axis = 0; axis < 2; ++axis) {
    std::int64_t lower = std::int64_t(chunk_index[axis]) * m_base_information.chunk_size;
    auto upper = lower + m_base_information.chunk_size;
    if (!std::in_range<int>(lower) || !std::in_range<int>(upper))
      return std::unexpected(CelestialException("Celestial chunk region exceeds the coordinate range"));
    first[axis] = static_cast<int>(lower);
    end[axis] = static_cast<int>(upper);
  }
  return RectI(first, end);
}

CelestialMasterDatabase::CelestialMasterDatabase(ConstructionKey, Root& root)
  : m_root(root), m_chunk_cache(root.random()), m_database(root.logger()) {}

std::expected<std::shared_ptr<CelestialMasterDatabase>, StarException> CelestialMasterDatabase::create(Root& root, std::optional<String> database_file) {
  auto database = std::make_shared<CelestialMasterDatabase>(ConstructionKey{}, root);
  auto initialized = database->init(database_file);
  if (!initialized)
    return std::unexpected(std::move(initialized.error()));
  return database;
}

std::expected<void, StarException> CelestialMasterDatabase::init(std::optional<String> const& database_file) {
  auto assets_result = m_root.assets();
  if (!assets_result)
    return std::unexpected(std::move(assets_result.error()));
  auto const& assets = *assets_result;
  auto config_result = assets->json("/celestial.config");
  if (!config_result)
    return std::unexpected(assets->with_asset_context(std::move(config_result.error()), "/celestial.config", "Loading celestial configuration"));
  auto const& config = *config_result;
  auto configured = [&]() -> std::expected<void, StarException> {
    {
      auto value = config.get_bool("saveAllChunks", false);
      if (!value)
        return std::unexpected(std::move(value.error()));
      m_chunks_always_persistent = *value;
    }
    {
      auto value = config.get("planetOrbitalLevels").and_then(celestial_integer<>);
      if (!value)
        return std::unexpected(std::move(value.error()));
      m_base_information.planet_orbital_levels = *value;
    }
    {
      auto value = config.get("satelliteOrbitalLevels").and_then(celestial_integer<>);
      if (!value)
        return std::unexpected(std::move(value.error()));
      m_base_information.satellite_orbital_levels = *value;
    }
    {
      auto value = config.get("chunkSize").and_then(celestial_integer<>);
      if (!value)
        return std::unexpected(std::move(value.error()));
      m_base_information.chunk_size = *value;
    }
    {
      auto value = config.get("xyCoordRange").and_then(celestial_integer_vector<2>);
      if (!value)
        return std::unexpected(std::move(value.error()));
      m_base_information.xy_coord_range = *value;
    }
    {
      auto value = config.get("zCoordRange").and_then(celestial_integer_vector<2>);
      if (!value)
        return std::unexpected(std::move(value.error()));
      m_base_information.z_coord_range = *value;
    }
    {
      auto value = config.get_bool("enforceCoordRange", false);
      if (!value)
        return std::unexpected(std::move(value.error()));
      m_base_information.enforce_coord_range = *value;
    }
    auto validated = validate_base_information(m_base_information);
    if (!validated)
      return std::unexpected(std::move(validated.error()));
    {
      auto value = config.get_float("systemProbability");
      if (!value)
        return std::unexpected(std::move(value.error()));
      m_generation_information.system_probability = *value;
    }
    {
      auto value = config.get_float("constellationProbability");
      if (!value)
        return std::unexpected(std::move(value.error()));
      m_generation_information.constellation_probability = *value;
    }
    {
      auto value = config.get("constellationLineCountRange").and_then(celestial_integer_vector<2, unsigned>);
      if (!value)
        return std::unexpected(std::move(value.error()));
      m_generation_information.constellation_line_count_range = *value;
    }
    {
      auto value = config.get("constellationMaxTries").and_then(celestial_integer<unsigned>);
      if (!value)
        return std::unexpected(std::move(value.error()));
      m_generation_information.constellation_max_tries = *value;
    }
    {
      auto value = config.get_float("maximumConstellationLineLength");
      if (!value)
        return std::unexpected(std::move(value.error()));
      m_generation_information.maximum_constellation_line_length = *value;
    }
    {
      auto value = config.get_float("minimumConstellationLineLength");
      if (!value)
        return std::unexpected(std::move(value.error()));
      m_generation_information.minimum_constellation_line_length = *value;
    }
    {
      auto value = config.get_float("minimumConstellationMagnitude");
      if (!value)
        return std::unexpected(std::move(value.error()));
      m_generation_information.minimum_constellation_magnitude = *value;
    }
    {
      auto value = config.get_float("minimumConstellationLineCloseness");
      if (!value)
        return std::unexpected(std::move(value.error()));
      m_generation_information.minimum_constellation_line_closeness = *value;
    }

    // Sorted maps preserve weighted-pool population order and deterministic generation.
    auto system_types = config.get_object("systemTypes");
    if (!system_types)
      return std::unexpected(std::move(system_types.error()));
    for (auto const& pair : Map<String, Json>::from(*system_types)) {
      SystemType type;
      {
        auto value = pair.second.get("baseParameters");
        if (!value)
          return std::unexpected(std::move(value.error()));
        type.base_parameters = std::move(*value);
      }
      {
        auto value = pair.second.get_array("variationParameters", JsonArray());
        if (!value)
          return std::unexpected(std::move(value.error()));
        type.variation_parameters = std::move(*value);
      }
      auto regions = pair.second.get_array("orbitRegions", JsonArray());
      if (!regions)
        return std::unexpected(std::move(regions.error()));
      for (auto const& region_json : *regions) {
        CelestialOrbitRegion region;
        {
          auto value = region_json.get_string("regionName");
          if (!value)
            return std::unexpected(std::move(value.error()));
          region.region_name = std::move(*value);
        }
        {
          auto value = region_json.get("orbitRange").and_then(celestial_integer_vector<2>);
          if (!value)
            return std::unexpected(std::move(value.error()));
          region.orbit_range = std::move(*value);
        }
        {
          auto value = region_json.get_float("bodyProbability");
          if (!value)
            return std::unexpected(std::move(value.error()));
          region.body_probability = std::move(*value);
        }
        {
          auto value = region_json.get("planetaryTypes").and_then([](Json const& value) { return json_to_weighted_pool<String>(value); });
          if (!value)
            return std::unexpected(std::move(value.error()));
          region.planetary_types = std::move(*value);
        }
        {
          auto value = region_json.get("satelliteTypes").and_then([](Json const& value) { return json_to_weighted_pool<String>(value); });
          if (!value)
            return std::unexpected(std::move(value.error()));
          region.satellite_types = std::move(*value);
        }
        type.orbit_regions.push_back(std::move(region));
      }
      auto added = m_generation_information.system_types.add(pair.first, std::move(type));
      if (!added)
        return std::unexpected(std::move(added.error()));
    }
    {
      auto value = config.get("systemTypePerlin").and_then([](Json const& value) {
        return PerlinD::create(value, static_random_u64("SystemTypePerlin"));
      });
      if (!value)
        return std::unexpected(std::move(value.error()));
      m_generation_information.system_type_perlin = std::move(*value);
    }
    {
      auto value = config.get("systemTypeBins");
      if (!value)
        return std::unexpected(std::move(value.error()));
      m_generation_information.system_type_bins = std::move(*value);
    }
    auto planetary_types = config.get_object("planetaryTypes");
    if (!planetary_types)
      return std::unexpected(std::move(planetary_types.error()));
    for (auto const& pair : Map<String, Json>::from(*planetary_types)) {
      PlanetaryType type;
      {
        auto value = pair.second.get_float("satelliteProbability");
        if (!value)
          return std::unexpected(std::move(value.error()));
        type.satellite_probability = std::move(*value);
      }
      {
        auto count = pair.second.get("maxSatelliteCount", Json());
        if (!count)
          return std::unexpected(std::move(count.error()));
        if (count->is_null()) {
          type.max_satellite_count = static_cast<std::size_t>(m_base_information.satellite_orbital_levels);
        } else {
          auto value = celestial_integer<std::size_t>(*count);
          if (!value)
            return std::unexpected(std::move(value.error()));
          type.max_satellite_count = *value;
        }
      }
      {
        auto value = pair.second.get("baseParameters");
        if (!value)
          return std::unexpected(std::move(value.error()));
        type.base_parameters = std::move(*value);
      }
      {
        auto value = pair.second.get_array("variationParameters", JsonArray());
        if (!value)
          return std::unexpected(std::move(value.error()));
        type.variation_parameters = std::move(*value);
      }
      m_generation_information.planetary_types[pair.first] = std::move(type);
    }
    auto satellite_types = config.get_object("satelliteTypes");
    if (!satellite_types)
      return std::unexpected(std::move(satellite_types.error()));
    for (auto const& pair : Map<String, Json>::from(*satellite_types)) {
      SatelliteType type;
      {
        auto value = pair.second.get("baseParameters");
        if (!value)
          return std::unexpected(std::move(value.error()));
        type.base_parameters = std::move(*value);
      }
      {
        auto value = pair.second.get_array("variationParameters", JsonArray());
        if (!value)
          return std::unexpected(std::move(value.error()));
        type.variation_parameters = std::move(*value);
      }
      {
        auto value = pair.second.get_object("orbitParameters", JsonObject());
        if (!value)
          return std::unexpected(std::move(value.error()));
        type.orbit_parameters = std::move(*value);
      }
      m_generation_information.satellite_types[pair.first] = std::move(type);
    }
    return {};
  }();
  if (!configured)
    return std::unexpected(assets->with_asset_context(std::move(configured.error()), "/celestial.config", "Loading celestial configuration"));
  auto named = [&]() -> std::expected<void, StarException> {
    auto names_result = assets->json("/celestial/names.config");
    if (!names_result)
      return std::unexpected(std::move(names_result.error()));
    auto const& names = *names_result;
    {
      auto value = names.get("planetarySuffixes").and_then([](Json const& value) { return json_to_string_list(value); });
      if (!value)
        return std::unexpected(std::move(value.error()));
      m_generation_information.planetary_suffixes = std::move(*value);
    }
    {
      auto value = names.get("satelliteSuffixes").and_then([](Json const& value) { return json_to_string_list(value); });
      if (!value)
        return std::unexpected(std::move(value.error()));
      m_generation_information.satellite_suffixes = std::move(*value);
    }
    {
      auto entries = names.get_array("systemPrefixNames");
      if (!entries)
        return std::unexpected(std::move(entries.error()));
      for (auto const& entry : *entries) {
        auto weight = entry.get_float(0);
        if (!weight)
          return std::unexpected(std::move(weight.error()));
        auto name = entry.get_string(1);
        if (!name)
          return std::unexpected(std::move(name.error()));
        auto inserted = m_generation_information.system_prefix_names.insert(*weight, std::move(*name));
        if (!inserted)
          return std::unexpected(std::move(inserted.error()));
      }
    }
    {
      auto entries = names.get_array("systemNames");
      if (!entries)
        return std::unexpected(std::move(entries.error()));
      for (auto const& entry : *entries) {
        auto weight = entry.get_float(0);
        if (!weight)
          return std::unexpected(std::move(weight.error()));
        auto name = entry.get_string(1);
        if (!name)
          return std::unexpected(std::move(name.error()));
        auto inserted = m_generation_information.system_names.insert(*weight, std::move(*name));
        if (!inserted)
          return std::unexpected(std::move(inserted.error()));
      }
    }
    {
      auto entries = names.get_array("systemSuffixNames");
      if (!entries)
        return std::unexpected(std::move(entries.error()));
      for (auto const& entry : *entries) {
        auto weight = entry.get_float(0);
        if (!weight)
          return std::unexpected(std::move(weight.error()));
        auto name = entry.get_string(1);
        if (!name)
          return std::unexpected(std::move(name.error()));
        auto inserted = m_generation_information.system_suffix_names.insert(*weight, std::move(*name));
        if (!inserted)
          return std::unexpected(std::move(inserted.error()));
      }
    }
    return {};
  }();
  if (!named)
    return std::unexpected(assets->with_asset_context(std::move(named.error()), "/celestial/names.config", "Loading celestial names"));
  if (database_file) {
    auto const block_size = m_database.block_size();
    auto identified = m_database.set_content_identifier("Celestial2");
    if (!identified)
      return std::unexpected(std::move(identified.error()));
    auto file = File::open(*database_file, IOMode::ReadWrite);
    if (!file)
      return std::unexpected(std::move(file.error()));
    auto device = m_database.set_io_device(std::move(*file));
    if (!device)
      return std::unexpected(std::move(device.error()));
    auto opened = m_database.open();
    if (!opened)
      return std::unexpected(std::move(opened.error()));
    if (m_database.content_identifier() != "Celestial2" || m_database.key_size() != 32) {
      auto logged = m_root.logger().warn("Backing up incompatible celestial database '{}' (identifier '{}', key size {})",
          *database_file, m_database.content_identifier(), m_database.key_size());
      if (!logged)
        return std::unexpected(std::move(logged.error()));
      auto closed = m_database.close(true);
      if (!closed)
        return std::unexpected(std::move(closed.error()));
      auto restored_identifier = m_database.set_content_identifier("Celestial2");
      if (!restored_identifier)
        return std::unexpected(std::move(restored_identifier.error()));
      auto restored_keys = m_database.reset_key_size();
      if (!restored_keys)
        return std::unexpected(std::move(restored_keys.error()));
      auto restored_blocks = m_database.set_block_size(block_size);
      if (!restored_blocks)
        return std::unexpected(std::move(restored_blocks.error()));
      auto renamed = File::rename(*database_file, Star::format("{}.{}.fail", *database_file, Time::milliseconds_since_epoch()));
      if (!renamed)
        return std::unexpected(std::move(renamed.error()));
      auto replacement = File::open(*database_file, IOMode::ReadWrite);
      if (!replacement)
        return std::unexpected(std::move(replacement.error()));
      auto replaced = m_database.set_io_device(std::move(*replacement));
      if (!replaced)
        return std::unexpected(std::move(replaced.error()));
      auto reopened = m_database.open();
      if (!reopened)
        return std::unexpected(std::move(reopened.error()));
    }
    auto manual_commit = m_database.set_auto_commit(false);
    if (!manual_commit)
      return std::unexpected(std::move(manual_commit.error()));
  }
  {
    auto value = config.get_float("commitInterval");
    if (!value)
      return std::unexpected(assets->with_asset_context(std::move(value.error()), "/celestial.config", "Loading celestial configuration"));
    m_commit_interval = *value;
  }
  m_commit_timer.restart(m_commit_interval);
  return {};
}

CelestialBaseInformation CelestialMasterDatabase::base_information() const {
  return m_base_information;
}

std::expected<CelestialResponse, StarException> CelestialMasterDatabase::respond_to_request(CelestialRequest const& request) {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lock_with_diagnostics(locker);
  if (auto location = request.maybe_left_ref()) {
    auto loaded = get_chunk_locked(location->get());
    if (!loaded)
      return std::unexpected(std::move(loaded.error()));
    auto const& cached = loaded->get();
    // System objects stay in the cache for separate system requests. Copy the
    // parameter map itself, not its entries, to retain bucket (wire) order.
    std::expected<CelestialResponse, StarException> response(std::in_place);
    auto& chunk = response->left_ref();
    chunk.chunk_index = cached.chunk_index;
    chunk.constellations = cached.constellations;
    chunk.system_parameters = cached.system_parameters;
    chunk.persistent = cached.persistent;
    return response;
  }
  auto const& location = request.right_ref();
  auto chunk = get_chunk_locked(chunk_index_for(location));
  if (!chunk)
    return std::unexpected(std::move(chunk.error()));
  auto planets = chunk->get().system_objects.get(location);
  if (!planets)
    return std::unexpected(std::move(planets.error()));
  return make_right(CelestialSystemObjects{location, planets->get()});
}

std::expected<void, StarException> CelestialMasterDatabase::cleanup_and_commit() {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lock_with_diagnostics(locker);
  m_chunk_cache.cleanup();
  if (m_database.is_open() && m_commit_timer.time_up()) {
    auto committed = m_database.commit();
    if (!committed)
      return std::unexpected(std::move(committed.error()));
    m_commit_timer.restart(m_commit_interval);
  }
  return {};
}

std::expected<void, StarException> CelestialMasterDatabase::close() {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lock_with_diagnostics(locker);
  if (std::exchange(m_close_attempted, true))
    return {};
  return m_database.close(true);
}

std::expected<std::optional<CelestialMasterDatabase::ResolvedCoordinate>, StarException>
CelestialMasterDatabase::resolve_locked(CelestialCoordinate const& coordinate) {
  if (!coordinate)
    return std::nullopt;
  auto chunk = get_chunk_locked(chunk_index_for(coordinate));
  if (!chunk)
    return std::unexpected(std::move(chunk.error()));
  auto system = chunk->get().system_objects.find_ref(coordinate.location());
  if (!system)
    return std::nullopt;
  if (coordinate.is_system())
    return ResolvedCoordinate{chunk->get(), std::cref(system->get())};
  auto planet = system->get().find_ref(coordinate.planetary_orbit_number());
  if (!planet)
    return std::nullopt;
  if (coordinate.is_planetary_body())
    return ResolvedCoordinate{chunk->get(), *planet};
  auto satellite = planet->get().satellite_parameters.find_ref(coordinate.satellite_orbit_number());
  if (!satellite)
    return std::nullopt;
  return ResolvedCoordinate{chunk->get(), *satellite};
}

std::expected<CelestialMasterDatabase::ResolvedCoordinate, StarException>
CelestialMasterDatabase::require_locked(CelestialCoordinate const& coordinate) {
  auto resolved = resolve_locked(coordinate);
  if (!resolved)
    return std::unexpected(std::move(resolved.error()));
  if (!*resolved)
    return std::unexpected(CelestialException::format("Invalid celestial coordinate {}", coordinate));
  return **resolved;
}

std::expected<std::reference_wrapper<CelestialParameters const>, StarException>
CelestialMasterDatabase::parameters_locked(CelestialCoordinate const& coordinate) {
  auto resolved = require_locked(coordinate);
  if (!resolved)
    return std::unexpected(std::move(resolved.error()));
  if (std::holds_alternative<SystemRef>(resolved->body))
    return std::as_const(resolved->chunk).system_parameters.get(coordinate.location());
  if (std::holds_alternative<PlanetRef>(resolved->body))
    return std::cref(std::get<PlanetRef>(resolved->body).get().planet_parameters);
  return std::cref(std::get<SatelliteRef>(resolved->body).get());
}

std::expected<bool, StarException> CelestialMasterDatabase::coordinate_valid(CelestialCoordinate const& coordinate) {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lock_with_diagnostics(locker);
  return resolve_locked(coordinate).transform([](std::optional<ResolvedCoordinate> const& resolved) { return resolved.has_value(); });
}

std::expected<std::optional<CelestialCoordinate>, StarException> CelestialMasterDatabase::find_random_world(
    unsigned tries, unsigned try_spatial_range, std::function<std::expected<bool, StarException>(CelestialCoordinate)> filter,
    std::optional<std::uint64_t> seed) {
  RandomSource random(m_root.random().next_u64());
  if (seed)
    random.seed(*seed);
  auto select_world = [&](CelestialCoordinate const& system) -> std::expected<std::optional<CelestialCoordinate>, StarException> {
    std::unique_lock locker(m_mutex, std::defer_lock);
    lock_with_diagnostics(locker);
    auto resolved = require_locked(system);
    if (!resolved)
      return std::unexpected(std::move(resolved.error()));
    auto const& planets = std::get<SystemRef>(resolved->body).get();
    if (planets.empty())
      return std::nullopt;
    // Sampling map iteration directly has the same order as its keys snapshot.
    auto planet = std::next(planets.begin(), static_cast<std::ptrdiff_t>(random.uniform_uint(planets.size() - 1)));
    auto world = system.child(planet->first);
    if (!world)
      return std::unexpected(std::move(world.error()));
    auto const& satellites = planet->second.satellite_parameters;
    if (!satellites.empty() && random.next_bool()) {
      auto satellite = std::next(satellites.begin(), static_cast<std::ptrdiff_t>(random.uniform_uint(satellites.size() - 1)));
      world = world->child(satellite->first);
      if (!world)
        return std::unexpected(std::move(world.error()));
    }
    return *world;
  };
  auto range = xy_range();
  auto half = static_cast<std::int64_t>(try_spatial_range / 2);
  for (unsigned i = 0; i < tries; ++i) {
    auto x = random.uniform_int(range.x_min(), range.x_max());
    if (!x)
      return std::unexpected(std::move(x.error()));
    auto y = random.uniform_int(range.y_min(), range.y_max());
    if (!y)
      return std::unexpected(std::move(y.error()));
    if (!std::in_range<int>(*x - half) || !std::in_range<int>(*y - half)
        || !std::in_range<int>(*x + half) || !std::in_range<int>(*y + half))
      return std::unexpected(CelestialException("Random search region exceeds the supported coordinate range"));
    auto systems = scan_systems(RectI(static_cast<int>(*x - half), static_cast<int>(*y - half),
        static_cast<int>(*x + half), static_cast<int>(*y + half)));
    if (!systems)
      return std::unexpected(std::move(systems.error()));
    for (auto const& system : *systems) {
      auto selected = select_world(system);
      if (!selected)
        return std::unexpected(std::move(selected.error()));
      if (!*selected)
        continue;
      auto const& world = **selected;
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
  lock_with_diagnostics(locker);
  auto value = parameters_locked(coordinate);
  if (!value)
    return std::unexpected(std::move(value.error()));
  return value->get();
}

std::expected<std::optional<String>, StarException> CelestialMasterDatabase::name(CelestialCoordinate const& coordinate) {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lock_with_diagnostics(locker);
  auto value = parameters_locked(coordinate);
  if (!value)
    return std::unexpected(std::move(value.error()));
  return value->get().name();
}

std::expected<std::optional<bool>, StarException> CelestialMasterDatabase::has_children(CelestialCoordinate const& coordinate) {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lock_with_diagnostics(locker);
  auto resolved = require_locked(coordinate);
  if (!resolved)
    return std::unexpected(std::move(resolved.error()));
  if (std::holds_alternative<SystemRef>(resolved->body))
    return !std::get<SystemRef>(resolved->body).get().empty();
  if (std::holds_alternative<PlanetRef>(resolved->body))
    return !std::get<PlanetRef>(resolved->body).get().satellite_parameters.empty();
  return false;
}

std::expected<List<CelestialCoordinate>, StarException> CelestialMasterDatabase::children(CelestialCoordinate const& coordinate) {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lock_with_diagnostics(locker);
  auto resolved = require_locked(coordinate);
  if (!resolved)
    return std::unexpected(std::move(resolved.error()));
  auto collect = [&](std::ranges::sized_range auto const& orbits) -> std::expected<List<CelestialCoordinate>, StarException> {
    List<CelestialCoordinate> children;
    children.reserve(std::ranges::size(orbits));
    for (int orbit : orbits) {
      auto child = coordinate.child(orbit);
      if (!child)
        return std::unexpected(std::move(child.error()));
      children.push_back(std::move(*child));
    }
    return children;
  };
  if (std::holds_alternative<SystemRef>(resolved->body))
    return collect(std::get<SystemRef>(resolved->body).get().key_view());
  if (std::holds_alternative<PlanetRef>(resolved->body))
    return collect(std::get<PlanetRef>(resolved->body).get().satellite_parameters.key_view());
  return std::unexpected(CelestialException("A satellite has no child orbits"));
}

std::expected<List<int>, StarException> CelestialMasterDatabase::child_orbits(CelestialCoordinate const& coordinate) {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lock_with_diagnostics(locker);
  auto resolved = require_locked(coordinate);
  if (!resolved)
    return std::unexpected(std::move(resolved.error()));
  if (std::holds_alternative<SystemRef>(resolved->body))
    return std::get<SystemRef>(resolved->body).get().keys();
  if (std::holds_alternative<PlanetRef>(resolved->body))
    return std::get<PlanetRef>(resolved->body).get().satellite_parameters.keys();
  return std::unexpected(CelestialException("A satellite has no child orbits"));
}

std::expected<List<CelestialCoordinate>, StarException> CelestialMasterDatabase::scan_systems(
    RectI const& region, std::optional<StringSet> const& included_types) {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lock_with_diagnostics(locker);
  List<CelestialCoordinate> systems;
  auto visited = for_each_chunk_index(region, [&](Vec2I const& location) -> std::expected<void, StarException> {
    auto chunk = get_chunk_locked(location, std::ref(locker));
    if (!chunk)
      return std::unexpected(std::move(chunk.error()));
    for (auto const& pair : chunk->get().system_parameters) {
      if (contains_system(region, pair.first.vec2())) {
        if (included_types) {
          auto type = pair.second.get_parameter("typeName", "").and_then([](Json const& value) { return value.to_string(); });
          if (!type)
            return std::unexpected(std::move(type.error()));
          if (!included_types->contains(*type))
            continue;
        }
        systems.emplace_back(pair.first);
      }
    }
    return {};
  });
  if (!visited)
    return std::unexpected(std::move(visited.error()));
  return systems;
}

std::expected<List<std::pair<Vec2I, Vec2I>>, StarException> CelestialMasterDatabase::scan_constellation_lines(RectI const& region) {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lock_with_diagnostics(locker);
  List<std::pair<Vec2I, Vec2I>> lines;
  auto visited = for_each_chunk_index(region, [&](Vec2I const& location) -> std::expected<void, StarException> {
    auto chunk = get_chunk_locked(location);
    if (!chunk)
      return std::unexpected(std::move(chunk.error()));
    for (auto const& constellation : chunk->get().constellations)
      for (auto const& line : constellation)
        if (region.intersects(Line2I(line.first, line.second)))
          lines.push_back(line);
    return {};
  });
  if (!visited)
    return std::unexpected(std::move(visited.error()));
  return lines;
}

std::expected<bool, StarException> CelestialMasterDatabase::scan_region_fully_loaded(RectI const&) {
  return true;
}

std::expected<void, StarException> CelestialMasterDatabase::update_parameters(
    CelestialCoordinate const& coordinate, CelestialParameters const& parameters) {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lock_with_diagnostics(locker);
  if (!m_database.is_open())
    return std::unexpected(CelestialException("Celestial database is not open for writing"));
  auto resolved = require_locked(coordinate);
  if (!resolved)
    return std::unexpected(std::move(resolved.error()));
  if (std::holds_alternative<SystemRef>(resolved->body))
    return std::unexpected(CelestialException("Only planetary and satellite parameters can be updated"));
  auto& target = std::holds_alternative<PlanetRef>(resolved->body)
      ? std::get<PlanetRef>(resolved->body).get().planet_parameters
      : std::get<SatelliteRef>(resolved->body).get();
  auto previous = std::exchange(target, parameters);
  auto rollback = make_scope_exit([&] { target = std::move(previous); });
  auto stored = persist_chunk_locked(chunk_index_for(coordinate), resolved->chunk);
  if (!stored)
    return stored;
  rollback.release();
  return {};
}

std::expected<void, StarException> CelestialMasterDatabase::mark_persistent(CelestialCoordinate const& coordinate) {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lock_with_diagnostics(locker);
  if (!m_database.is_open())
    return {};
  auto index = chunk_index_for(coordinate);
  auto loaded = get_chunk_locked(index);
  if (!loaded)
    return std::unexpected(std::move(loaded.error()));
  if (loaded->get().persistent)
    return {};
  return persist_chunk_locked(index, loaded->get());
}

std::expected<void, StarException> CelestialMasterDatabase::persist_chunk_locked(Vec2I const& chunk_index, CelestialChunk& chunk) {
  auto versioning = m_root.versioningDatabase();
  if (!versioning)
    return std::unexpected(std::move(versioning.error()));
  auto versioned = chunk.to_json().and_then([&](Json const& value) {
    return (*versioning)->makeCurrentVersionedJson("CelestialChunk", value);
  });
  if (!versioned)
    return std::unexpected(std::move(versioned.error()));
  auto stored = store_versioned_chunk(chunk_index, *versioned);
  if (!stored)
    return stored;
  chunk.persistent = true;
  return {};
}

std::expected<void, StarException> CelestialMasterDatabase::store_versioned_chunk(
    Vec2I const& chunk_index, VersionedJson const& chunk) {
  DataStreamBuffer stream;
  auto written = stream.write(chunk);
  if (!written)
    return std::unexpected(std::move(written.error()));
  auto sub_versioning = VersionedJson::writeSubVersioning(stream, chunk);
  if (!sub_versioning)
    return std::unexpected(std::move(sub_versioning.error()));
  auto key = DataStreamBuffer::serialize(chunk_index);
  if (!key)
    return std::unexpected(std::move(key.error()));
  auto compressed = compress(stream.data());
  if (!compressed)
    return std::unexpected(std::move(compressed.error()));
  return m_database.insert(*key, *compressed).transform([](bool) {});
}

std::expected<std::reference_wrapper<CelestialChunk>, StarException> CelestialMasterDatabase::get_chunk_locked(
    Vec2I const& chunk_index, std::optional<std::reference_wrapper<std::unique_lock<std::mutex>>> generation_lock) {
  auto cached = m_chunk_cache.get(chunk_index, [&](Vec2I const& index) -> std::expected<CelestialChunk, StarException> {
    if (m_database.is_open()) {
      auto key = DataStreamBuffer::serialize(index);
      if (!key)
        return std::unexpected(std::move(key.error()));
      auto data = m_database.find(*key);
      if (!data)
        return std::unexpected(std::move(data.error()));
      if (*data) {
        auto decompressed = decompress(**data);
        if (!decompressed)
          return std::unexpected(std::move(decompressed.error()));
        auto versioned = DataStreamBuffer::deserialize<VersionedJson>(decompressed->span());
        if (!versioned)
          return std::unexpected(std::move(versioned.error()));
        auto versioning = m_root.versioningDatabase();
        if (!versioning)
          return std::unexpected(std::move(versioning.error()));
        auto current = (*versioning)->versionedJsonCurrent(*versioned);
        if (!current)
          return std::unexpected(std::move(current.error()));
        if (!*current) {
          auto upgraded = (*versioning)->updateVersionedJson(*versioned);
          if (!upgraded)
            return std::unexpected(std::move(upgraded.error()));
          versioned = std::move(upgraded);
          auto stored = store_versioned_chunk(index, *versioned);
          if (!stored)
            return std::unexpected(std::move(stored.error()));
        }
        return CelestialChunk::create(versioned->content);
      }
    }
    if (!generation_lock)
      return produce_chunk(index);
    auto& lock = generation_lock->get();
    lock.unlock();
    auto relock = make_scope_exit([&] { lock_with_diagnostics(lock); });
    return produce_chunk(index);
  });
  if (!cached)
    return std::unexpected(std::move(cached.error()));
  // Another request can win the cache insertion during unlocked generation.
  // Persist only the resident version, not the losing producer's result.
  if (m_chunks_always_persistent && !cached->get().persistent && m_database.is_open()) {
    auto stored = persist_chunk_locked(chunk_index, cached->get());
    if (!stored)
      return std::unexpected(std::move(stored.error()));
  }
  return cached;
}

std::expected<CelestialChunk, StarException> CelestialMasterDatabase::produce_chunk(Vec2I const& chunk_index) const {
  CelestialChunk chunk;
  chunk.chunk_index = chunk_index;
  auto bounds = chunk_region(chunk_index);
  if (!bounds)
    return std::unexpected(std::move(bounds.error()));
  auto const& region = *bounds;
  if (m_base_information.enforce_coord_range && !xy_range().contains(region, true))
    return chunk;
  RandomSource random(static_random_u64(chunk_index[0], chunk_index[1], "ChunkIndexMix"));
  auto z_min = static_cast<std::int64_t>(m_base_information.z_coord_range[0]);
  auto z_width = static_cast<std::int64_t>(m_base_information.z_coord_range[1]) - z_min;
  List<Vec3I> locations;
  for (int x = region.x_min(); x < region.x_max(); ++x) {
    for (int y = region.y_min(); y < region.y_max(); ++y) {
      if (random.uniform_float() < m_generation_information.system_probability) {
        // Keep the signed remainder: negative draws are part of existing universe seeds.
        auto z = z_min + random.next_i32() % z_width;
        if (!std::in_range<int>(z))
          return std::unexpected(CelestialException("Generated Z coordinate is outside the supported range"));
        locations.emplace_back(x, y, static_cast<int>(z));
      }
    }
  }
  List<Vec2I> candidates;
  for (auto const& location : locations) {
    auto information = produce_system(random, location);
    if (!information)
      return std::unexpected(std::move(information.error()));
    if (*information) {
      auto& system = **information;
      auto magnitude = system.first.get_parameter("magnitude").and_then([](Json const& value) { return value.to_float(); });
      if (!magnitude)
        return std::unexpected(std::move(magnitude.error()));
      chunk.system_parameters[location] = std::move(system.first);
      chunk.system_objects[location] = std::move(system.second);
      if (*magnitude >= m_generation_information.minimum_constellation_magnitude)
        candidates.push_back(location.vec2());
    }
  }
  auto constellations = produce_constellations(random, candidates);
  if (!constellations)
    return std::unexpected(std::move(constellations.error()));
  chunk.constellations = std::move(*constellations);
  return chunk;
}

std::expected<std::optional<std::pair<CelestialParameters, HashMap<int, CelestialPlanet>>>, StarException>
CelestialMasterDatabase::produce_system(RandomSource& random, Vec3I const& location) const {
  auto selector = m_generation_information.system_type_perlin.get(location[0], location[1]);
  if (!selector)
    return std::unexpected(std::move(selector.error()));
  float type_selector = *selector;
  auto type_name = binned_choice_from_json(m_generation_information.system_type_bins, type_selector, "")
      .and_then([](Json const& value) { return value.to_string(); });
  if (!type_name)
    return std::unexpected(std::move(type_name.error()));
  if (type_name->empty())
    return std::optional<std::pair<CelestialParameters, HashMap<int, CelestialPlanet>>>();
  auto type_result = m_generation_information.system_types.get(*type_name);
  if (!type_result)
    return std::unexpected(std::move(type_result.error()));
  auto const& type = type_result->get();
  CelestialCoordinate system_coordinate(location);
  std::uint64_t system_seed = random.next_u64();
  String const empty_name;
  auto select_name = [&](WeightedPool<String> const& pool) -> String const& {
    return pool.empty() ? empty_name : pool.items()[pool.select_index(random)].second;
  };
  auto const& prefix = select_name(m_generation_information.system_prefix_names);
  auto const& mid = select_name(m_generation_information.system_names);
  auto const& suffix = select_name(m_generation_information.system_suffix_names);
  auto system_name = String(Star::format("{} {} {}", prefix, mid, suffix)).trim();
  if (!system_name)
    return std::unexpected(std::move(system_name.error()));
  auto one_digit = system_name->replace("<onedigit>", Star::format("{:01d}", random.next_u32() % 10));
  if (!one_digit)
    return std::unexpected(std::move(one_digit.error()));
  auto two_digits = one_digit->replace("<twodigit>", Star::format("{:02d}", random.next_u32() % 100));
  if (!two_digits)
    return std::unexpected(std::move(two_digits.error()));
  auto three_digits = two_digits->replace("<threedigit>", Star::format("{:03d}", random.next_u32() % 1000));
  if (!three_digits)
    return std::unexpected(std::move(three_digits.error()));
  auto four_digits = three_digits->replace("<fourdigit>", Star::format("{:04d}", random.next_u32() % 10000));
  if (!four_digits)
    return std::unexpected(std::move(four_digits.error()));
  system_name = std::move(four_digits);
  auto system_variation = random.sample_value(type.variation_parameters);
  if (!system_variation)
    return std::unexpected(std::move(system_variation.error()));
  auto system_parameters = CelestialParameters::create(m_root, system_coordinate, system_seed, *system_name,
      json_merge(type.base_parameters, *system_variation));
  if (!system_parameters)
    return std::unexpected(std::move(system_parameters.error()));
  struct SelectedOrbit {
    int number;
    std::reference_wrapper<CelestialOrbitRegion const> region;
  };
  List<SelectedOrbit> planetary_orbits;
  // Sample all occupancies before drawing any types or variations. The first
  // matching region wins, even when its occupancy draw rejects the orbit.
  for (int slot = 0; slot < m_base_information.planet_orbital_levels; ++slot) {
    int orbit = slot + 1;
    for (auto const& region : type.orbit_regions) {
      if (orbit < region.orbit_range[0] || orbit > region.orbit_range[1])
        continue;
      if (random.uniform_float() <= region.body_probability)
        planetary_orbits.emplace_back(orbit, std::cref(region));
      break;
    }
  }
  HashMap<int, CelestialPlanet> objects;
  for (auto [planet_index, selected] : std::views::enumerate(planetary_orbits)) {
    int planet_orbit = selected.number;
    auto const& region = selected.region.get();
    auto const& planetary_type_name = select_name(region.planetary_types);
    if (auto planet_type = m_generation_information.planetary_types.find_ref(planetary_type_name)) {
      auto planetary_variation = random.sample_value(planet_type->get().variation_parameters);
      if (!planetary_variation)
        return std::unexpected(std::move(planetary_variation.error()));
      auto planetary_parameters = json_merge(planet_type->get().base_parameters, *planetary_variation);
      CelestialCoordinate planet_coordinate(location, planet_orbit);
      std::uint64_t planet_seed = random.next_u64();
      auto planetary_suffix = m_generation_information.planetary_suffixes.at(static_cast<std::size_t>(planet_index));
      if (!planetary_suffix)
        return std::unexpected(std::move(planetary_suffix.error()));
      String planet_name = Star::format("{} {}", *system_name, planetary_suffix->get());
      auto parameters = CelestialParameters::create(m_root, planet_coordinate, planet_seed, planet_name, planetary_parameters);
      if (!parameters)
        return std::unexpected(std::move(parameters.error()));
      CelestialPlanet planet;
      planet.planet_parameters = std::move(*parameters);
      List<int> satellite_orbits;
      for (int slot = 0; slot < m_base_information.satellite_orbital_levels
          && satellite_orbits.size() < planet_type->get().max_satellite_count; ++slot)
        if (random.uniform_float() < planet_type->get().satellite_probability)
          satellite_orbits.push_back(slot + 1);
      for (auto [satellite_index, satellite_orbit] : std::views::enumerate(satellite_orbits)) {
        auto const& satellite_type_name = select_name(region.satellite_types);
        if (auto satellite_type = m_generation_information.satellite_types.find_ref(satellite_type_name)) {
          auto variation = random.sample_value(satellite_type->get().variation_parameters);
          if (!variation)
            return std::unexpected(std::move(variation.error()));
          Json orbit_variation;
          if (auto configured = satellite_type->get().orbit_parameters.find_ref(region.region_name)) {
            auto choices = configured->get().array_ptr();
            if (!choices)
              return std::unexpected(std::move(choices.error()));
            auto sampled = random.sample_value(**choices);
            if (!sampled)
              return std::unexpected(std::move(sampled.error()));
            orbit_variation = std::move(*sampled);
          }
          auto satellite_parameters = json_merge(satellite_type->get().base_parameters, *variation, orbit_variation);
          CelestialCoordinate satellite_coordinate(location, planet_orbit, satellite_orbit);
          std::uint64_t satellite_seed = random.next_u64();
          auto satellite_suffix = m_generation_information.satellite_suffixes.at(static_cast<std::size_t>(satellite_index));
          if (!satellite_suffix)
            return std::unexpected(std::move(satellite_suffix.error()));
          String satellite_name = Star::format("{} {}", planet_name, satellite_suffix->get());
          auto parameters = CelestialParameters::create(m_root, satellite_coordinate, satellite_seed, satellite_name, satellite_parameters);
          if (!parameters)
            return std::unexpected(std::move(parameters.error()));
          planet.satellite_parameters[satellite_orbit] = std::move(*parameters);
        }
      }
      objects[planet_orbit] = std::move(planet);
    }
  }
  return std::pair<CelestialParameters, HashMap<int, CelestialPlanet>>{std::move(*system_parameters), std::move(objects)};
}

std::expected<List<CelestialConstellation>, StarException> CelestialMasterDatabase::produce_constellations(
    RandomSource& random, List<Vec2I> const& constellation_candidates) const {
  List<CelestialConstellation> constellations;

  if (random.uniform_float() < m_generation_information.constellation_probability && constellation_candidates.size() > 2) {
    auto target_constellation_line_count = random.uniform_uint(
        m_generation_information.constellation_line_count_range[0], m_generation_information.constellation_line_count_range[1]);
    if (!target_constellation_line_count)
      return std::unexpected(std::move(target_constellation_line_count.error()));
    Set<Vec2I> constellation_points;
    Set<Line2I> constellation_lines;

    unsigned tries = 0;

    while (constellation_lines.size() < *target_constellation_line_count && tries < m_generation_information.constellation_max_tries) {
      ++tries;

      auto start_point = constellation_points.empty() ? random.sample_value(constellation_candidates) : random.sample_value(constellation_points);
      if (!start_point)
        return std::unexpected(std::move(start_point.error()));
      Vec2I start = *start_point;

      auto end_point = random.sample_value(constellation_candidates);
      if (!end_point)
        return std::unexpected(std::move(end_point.error()));
      Vec2I end = *end_point;

      Line2I proposed_line(start, end);
      Line2D proposed_line_d(proposed_line);

      if (start == end)
        continue;

      if (constellation_lines.contains(proposed_line) || constellation_lines.contains(proposed_line.reversed()))
        continue;

      auto length = proposed_line_d.diff().magnitude();
      if (length > m_generation_information.maximum_constellation_line_length
          || length < m_generation_information.minimum_constellation_line_length)
        continue;

      bool valid = true;
      for (auto const& constellation_line : constellation_lines) {
        Line2D constellation_line_d(constellation_line);
        auto intersection = proposed_line_d.intersection(constellation_line_d);
        if (intersection.intersects && Vec2I::round(intersection.point) != proposed_line.min()
            && Vec2I::round(intersection.point) != proposed_line.max()) {
          valid = false;
          break;
        }

        if (proposed_line.min() != constellation_line.min() && proposed_line.min() != constellation_line.max()
            && constellation_line_d.distance_to(proposed_line_d.min())
              < m_generation_information.minimum_constellation_line_closeness) {
          valid = false;
          break;
        }

        if (proposed_line.max() != constellation_line.min() && proposed_line.max() != constellation_line.max()
            && constellation_line_d.distance_to(proposed_line_d.max())
              < m_generation_information.minimum_constellation_line_closeness) {
          valid = false;
          break;
        }
      }

      if (valid) {
        constellation_lines.add(proposed_line);
        constellation_points.add(proposed_line.min());
        constellation_points.add(proposed_line.max());
      }
    }

    if (constellation_lines.size() > 1) {
      CelestialConstellation constellation;
      for (auto const& line : constellation_lines)
        constellation.push_back({line.min(), line.max()});
      constellations.push_back(std::move(constellation));
    }
  }

  return constellations;
}

CelestialSlaveDatabase::CelestialSlaveDatabase(ConstructionKey, RandomService& random) : m_chunk_cache(random) {}

std::expected<std::shared_ptr<CelestialSlaveDatabase>, StarException> CelestialSlaveDatabase::create(
    std::shared_ptr<Assets const> const& assets, RandomService& random, CelestialBaseInformation base_information) {
  if (auto valid = validate_base_information(base_information); !valid)
    return std::unexpected(std::move(valid.error()));
  auto config = assets->json("/celestial.config");
  if (!config)
    return std::unexpected(assets->with_asset_context(std::move(config.error()), "/celestial.config", "Loading celestial configuration"));
  auto timeout = config->get_float("requestTimeout");
  if (!timeout)
    return std::unexpected(assets->with_asset_context(std::move(timeout.error()), "/celestial.config", "Loading celestial configuration"));
  auto database = std::make_shared<CelestialSlaveDatabase>(ConstructionKey{}, random);
  database->m_base_information = std::move(base_information);
  database->m_request_timeout = *timeout;
  return database;
}

std::expected<std::optional<std::reference_wrapper<CelestialChunk const>>, StarException>
CelestialSlaveDatabase::find_or_request_chunk(Vec2I const& index) {
  auto chunk = m_chunk_cache.find_ref(index);
  if (!chunk)
    return std::unexpected(std::move(chunk.error()));
  if (*chunk)
    return std::cref((*chunk)->get());
  m_pending_chunk_requests.try_emplace(index);
  return std::nullopt;
}

std::expected<std::optional<std::reference_wrapper<CelestialSlaveDatabase::SystemPlanets const>>, StarException>
CelestialSlaveDatabase::find_or_request_objects(CelestialCoordinate const& coordinate) {
  auto chunk = find_or_request_chunk(chunk_index_for(coordinate));
  if (!chunk)
    return std::unexpected(std::move(chunk.error()));
  if (*chunk) {
    auto location = coordinate.location();
    if (auto planets = (*chunk)->get().system_objects.find_ref(location))
      return planets;
    m_pending_system_requests.try_emplace(location);
  }
  return std::nullopt;
}

std::expected<void, StarException> CelestialSlaveDatabase::signal_region(RectI const& region) {
  return scan_region_fully_loaded(region).transform([](bool) {});
}

std::expected<void, StarException> CelestialSlaveDatabase::signal_system(CelestialCoordinate const& system) {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lock_with_diagnostics(locker);
  if (auto found = find_or_request_objects(system); !found)
    return std::unexpected(std::move(found.error()));
  return {};
}

List<CelestialRequest> CelestialSlaveDatabase::pull_requests() {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lock_with_diagnostics(locker);

  List<CelestialRequest> requests;

  for (auto chunk_it = m_pending_chunk_requests.begin(); chunk_it != m_pending_chunk_requests.end();) {
    auto&& pair = *chunk_it;
    if (!pair.second.running()) {
      requests.push_back(make_left(pair.first));
      pair.second.restart(m_request_timeout);
      ++chunk_it;
    } else if (pair.second.time_up()) {
      chunk_it = m_pending_chunk_requests.erase(chunk_it);
    } else {
      ++chunk_it;
    }
  }

  for (auto system_it = m_pending_system_requests.begin(); system_it != m_pending_system_requests.end();) {
    auto&& pair = *system_it;
    if (!pair.second.running()) {
      requests.push_back(make_right(pair.first));
      pair.second.restart(m_request_timeout);
      ++system_it;
    } else if (pair.second.time_up()) {
      system_it = m_pending_system_requests.erase(system_it);
    } else {
      ++system_it;
    }
  }

  return requests;
}

std::expected<void, StarException> CelestialSlaveDatabase::push_responses(List<CelestialResponse> responses) {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lock_with_diagnostics(locker);
  for (auto& response : responses) {
    if (auto chunk = response.maybe_left_ref()) {
      m_pending_chunk_requests.remove(chunk->get().chunk_index);
      auto stored = m_chunk_cache.insert_or_assign(chunk->get().chunk_index, std::move(chunk->get()));
      if (!stored)
        return std::unexpected(std::move(stored.error()));
    } else if (auto objects = response.maybe_right_ref()) {
      m_pending_system_requests.remove(objects->get().system_location);
      auto chunk = m_chunk_cache.find_ref(chunk_index_for(objects->get().system_location.vec2()));
      if (!chunk)
        return std::unexpected(std::move(chunk.error()));
      if (*chunk)
        (*chunk)->get().system_objects[objects->get().system_location] = std::move(objects->get().planets);
    }
  }
  return {};
}

void CelestialSlaveDatabase::cleanup() {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lock_with_diagnostics(locker);
  m_chunk_cache.cleanup();
}

std::expected<std::optional<std::reference_wrapper<CelestialParameters const>>, StarException>
CelestialSlaveDatabase::find_or_request_parameters(CelestialCoordinate const& coordinate) {
  if (!coordinate)
    return std::unexpected(CelestialException("CelestialSlaveDatabase::parameters called on null coordinate"));
  if (coordinate.is_system()) {
    auto chunk = find_or_request_chunk(chunk_index_for(coordinate));
    if (!chunk)
      return std::unexpected(std::move(chunk.error()));
    if (*chunk)
      return (*chunk)->get().system_parameters.find_ref(coordinate.location());
    return std::nullopt;
  }
  auto system = find_or_request_objects(coordinate);
  if (!system)
    return std::unexpected(std::move(system.error()));
  if (!*system)
    return std::nullopt;
  auto planet_coordinate = coordinate.planet();
  if (!planet_coordinate)
    return std::unexpected(std::move(planet_coordinate.error()));
  auto planet_orbit = planet_coordinate->orbit_number();
  if (!planet_orbit)
    return std::unexpected(std::move(planet_orbit.error()));
  auto planet = (*system)->get().get(*planet_orbit);
  if (!planet)
    return std::unexpected(std::move(planet.error()));
  if (coordinate.is_planetary_body())
    return std::cref(planet->get().planet_parameters);
  auto orbit = coordinate.orbit_number();
  if (!orbit)
    return std::unexpected(std::move(orbit.error()));
  auto satellite = planet->get().satellite_parameters.get(*orbit);
  if (!satellite)
    return std::unexpected(std::move(satellite.error()));
  return *satellite;
}

std::expected<std::optional<CelestialParameters>, StarException> CelestialSlaveDatabase::parameters(CelestialCoordinate const& coordinate) {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lock_with_diagnostics(locker);
  auto value = find_or_request_parameters(coordinate);
  if (!value)
    return std::unexpected(std::move(value.error()));
  if (*value)
    return (*value)->get();
  return std::nullopt;
}

std::expected<std::optional<String>, StarException> CelestialSlaveDatabase::name(CelestialCoordinate const& coordinate) {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lock_with_diagnostics(locker);
  auto value = find_or_request_parameters(coordinate);
  if (!value)
    return std::unexpected(std::move(value.error()));
  if (*value)
    return (*value)->get().name();
  return std::nullopt;
}

std::expected<std::optional<bool>, StarException> CelestialSlaveDatabase::has_children(CelestialCoordinate const& coordinate) {
  if (!coordinate)
    return std::unexpected(CelestialException("CelestialSlaveDatabase::has_children called on null coordinate"));
  std::unique_lock locker(m_mutex, std::defer_lock);
  lock_with_diagnostics(locker);
  auto system = find_or_request_objects(coordinate);
  if (!system)
    return std::unexpected(std::move(system.error()));
  if (*system) {
    if (coordinate.is_system())
      return !(*system)->get().empty();
    if (coordinate.is_planetary_body()) {
      auto orbit = coordinate.orbit_number();
      if (!orbit)
        return std::unexpected(std::move(orbit.error()));
      auto planet = (*system)->get().get(*orbit);
      if (!planet)
        return std::unexpected(std::move(planet.error()));
      return !planet->get().satellite_parameters.empty();
    }
  }
  return std::optional<bool>();
}

std::expected<List<CelestialCoordinate>, StarException> CelestialSlaveDatabase::children(CelestialCoordinate const& coordinate) {
  auto orbits = child_orbits(coordinate);
  if (!orbits)
    return std::unexpected(std::move(orbits.error()));
  List<CelestialCoordinate> children;
  children.reserve(orbits->size());
  for (auto orbit : *orbits) {
    auto child = coordinate.child(orbit);
    if (!child)
      return std::unexpected(std::move(child.error()));
    children.push_back(std::move(*child));
  }
  return children;
}

std::expected<List<int>, StarException> CelestialSlaveDatabase::child_orbits(CelestialCoordinate const& coordinate) {
  if (!coordinate)
    return std::unexpected(CelestialException("CelestialSlaveDatabase::child_orbits called on null coordinate"));
  if (coordinate.is_satellite_body())
    return std::unexpected(CelestialException("CelestialSlaveDatabase::child_orbits called on improper type!"));
  std::unique_lock locker(m_mutex, std::defer_lock);
  lock_with_diagnostics(locker);
  auto system = find_or_request_objects(coordinate);
  if (!system)
    return std::unexpected(std::move(system.error()));
  if (*system) {
    if (coordinate.is_system())
      return (*system)->get().keys();
    if (coordinate.is_planetary_body()) {
      auto orbit = coordinate.orbit_number();
      if (!orbit)
        return std::unexpected(std::move(orbit.error()));
      auto planet = (*system)->get().get(*orbit);
      if (!planet)
        return std::unexpected(std::move(planet.error()));
      return planet->get().satellite_parameters.keys();
    }
  }
  return List<int>();
}

std::expected<List<CelestialCoordinate>, StarException> CelestialSlaveDatabase::scan_systems(
    RectI const& region, std::optional<StringSet> const& included_types) {
  List<CelestialCoordinate> systems;
  auto scanned = scan_chunks(region, [&](CelestialChunk const& chunk) -> std::expected<void, StarException> {
    for (auto const& [location, parameters] : chunk.system_parameters) {
      if (!contains_system(region, location.vec2()))
        continue;
      if (included_types) {
        auto type = parameters.get_parameter("typeName", "").and_then([](Json const& value) { return value.to_string(); });
        if (!type)
          return std::unexpected(std::move(type.error()));
        if (!included_types->contains(*type))
          continue;
      }
      systems.emplace_back(location);
    }
    return {};
  });
  if (!scanned)
    return std::unexpected(std::move(scanned.error()));
  return systems;
}

std::expected<List<std::pair<Vec2I, Vec2I>>, StarException> CelestialSlaveDatabase::scan_constellation_lines(RectI const& region) {
  List<std::pair<Vec2I, Vec2I>> lines;
  auto scanned = scan_chunks(region, [&](CelestialChunk const& chunk) -> std::expected<void, StarException> {
    for (auto const& constellation : chunk.constellations)
      for (auto const& line : constellation)
        if (region.intersects(Line2I(line.first, line.second)))
          lines.push_back(line);
    return {};
  });
  if (!scanned)
    return std::unexpected(std::move(scanned.error()));
  return lines;
}

std::expected<bool, StarException> CelestialSlaveDatabase::scan_region_fully_loaded(RectI const& region) {
  return scan_chunks(region, [](CelestialChunk const&) -> std::expected<void, StarException> { return {}; });
}

void CelestialSlaveDatabase::invalidate_cache_for(CelestialCoordinate const& coordinate) {
  std::unique_lock locker(m_mutex, std::defer_lock);
  lock_with_diagnostics(locker);

  m_chunk_cache.erase(chunk_index_for(coordinate));
}

}

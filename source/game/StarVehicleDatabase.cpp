import star.json;
import star.json_extra;
import star.id_map;
import star.casting;
import star.data_stream;
import star.game_types;
import star.strong_typedef;
import star.bi_map;
import star.list;
import star.rpc_promise;
import star.string;
import star.color;
import star.image_processing;
import star.hash;
import star.string_view;
import star.thread;
import star.directives;
import star.asset_path;
import star.lua;
import star.random;
import star.periodic_function;
import star.ordered_map;
import star.audio;
import star.map;
import star.set;
import star.io_device;
import star.ref_ptr;
import star.logging;
import star.listener;
import std;
import star.format;
import star.version;

import star.damage_types;
import star.world_geometry;
import star.status_types;
import star.damage;
import star.light_source;
import star.entity;
import star.tile_damage;
import star.interaction_types;
import star.item_descriptor;
import star.celestial_coordinate;
import star.quest_descriptor;
import star.interactive_entity;
import star.collision_block;
import star.tile_entity;
import star.tile_modification;
import star.lua_root;
import star.force_regions;
import star.world;
import star.physics_entity;
import star.mobile_entity;
import star.animated_part_set;
import star.drawable;
import star.animation;
import star.particle;
import star.mixer;
import star.networked_animator;
import star.movement_controller;
import star.lua_components;
import star.anchorable_entity;
import star.entity_rendering_types;
import star.lounging_entities;
import star.scripted_entity;
import star.lua_animation_component;
import star.vehicle;
import star.asset_source;
import star.assets;
import star.root_base;
import star.configuration;
import star.root;
import star.utility_lua_bindings;
import star.rebuilder;

import star.vehicle_database;

import star.data_stream_devices;

import star.net_compatibility;
import star.byte_array;
namespace Star {

std::expected<std::shared_ptr<VehicleDatabase>, StarException> VehicleDatabase::create(Root& root) {
  struct MakeShared final : VehicleDatabase {
    explicit MakeShared(Root& root) : VehicleDatabase(root) {}
  };
  auto database = std::make_shared<MakeShared>(root);
  auto rebuilder = Rebuilder::create(root, "vehicle");
  if (!rebuilder)
    return std::unexpected(std::move(rebuilder.error()));
  database->m_rebuilder = std::move(*rebuilder);
  auto assets = root.assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  auto files = (*assets)->scanExtension("vehicle");
  if (!files)
    return std::unexpected(std::move(files.error()));
  if (auto queued = (*assets)->queueJsons(files->get()); !queued)
    return std::unexpected(std::move(queued.error()));
  for (auto const& scannedFile : files->get()) {
    String file = scannedFile.string();
    auto load = [&]() -> std::expected<void, StarException> {
      auto config = (*assets)->json(file);
      if (!config)
        return std::unexpected(std::move(config.error()));
      auto name = config->getString("name");
      if (!name)
        return std::unexpected(std::move(name.error()));
      if (database->m_vehicles.contains(*name))
        return std::unexpected(VehicleDatabaseException::format("Repeat vehicle name '{}'", *name));
      auto added = database->m_vehicles.add(std::move(*name), std::make_pair(file, std::move(*config)));
      if (!added)
        return std::unexpected(std::move(added.error()));
      return {};
    }();
    if (!load)
      return std::unexpected(VehicleDatabaseException(Star::format("Error loading vehicle '{}'", file), (*assets)->withAssetContext(std::move(load.error()), file, "Loading vehicle")));
  }
  return database;
}

std::expected<std::shared_ptr<Vehicle>, StarException> VehicleDatabase::create(String const& vehicleName, Json const& extraConfig) const {
  auto configPair = m_vehicles.find_ref(vehicleName);
  if (!configPair)
    return std::unexpected(VehicleDatabaseException::format("No such vehicle named '{}'", vehicleName));
  return Vehicle::create(m_root, configPair->get().second, configPair->get().first, extraConfig);
}

std::expected<ByteArray, StarException> VehicleDatabase::netStore(std::shared_ptr<Vehicle> const& vehicle, NetCompatibilityRules rules) const {
  DataStreamBuffer ds;
  ds.set_stream_compatibility_version(rules);
  auto name = vehicle->baseConfig().getString("name");
  if (!name)
    return std::unexpected(std::move(name.error()));
  if (auto result = ds.write(*name); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = ds.write(vehicle->dynamicConfig()); !result)
    return std::unexpected(std::move(result.error()));
  return ds.take_data();
}

std::expected<std::shared_ptr<Vehicle>, StarException> VehicleDatabase::netLoad(ByteArray const& netStore, NetCompatibilityRules rules) const {
  DataStreamReader ds(netStore.span());
  ds.set_stream_compatibility_version(rules);
  auto name = ds.read<String>();
  if (!name)
    return std::unexpected(std::move(name.error()));
  auto dynamicConfig = ds.read<Json>();
  if (!dynamicConfig)
    return std::unexpected(std::move(dynamicConfig.error()));
  return create(*name, *dynamicConfig);
}

std::expected<Json, StarException> VehicleDatabase::diskStore(std::shared_ptr<Vehicle> const& vehicle) const {
  auto name = vehicle->baseConfig().getString("name");
  if (!name)
    return std::unexpected(std::move(name.error()));
  auto state = vehicle->diskStore();
  if (!state)
    return std::unexpected(std::move(state.error()));
  return JsonObject{
    {"name", std::move(*name)},
    {"dynamicConfig", vehicle->dynamicConfig()},
    {"state", std::move(*state)}
  };
}

std::expected<std::shared_ptr<Vehicle>, StarException> VehicleDatabase::diskLoad(Json const& diskStore) const {
  std::shared_ptr<Vehicle> vehicle;
  auto load = [&, self = std::cref(*this)](Json const& store) -> std::expected<void, StarException> {
    auto name = store.getString("name");
    if (!name)
      return std::unexpected(std::move(name.error()));
    auto config = store.get("dynamicConfig");
    if (!config)
      return std::unexpected(std::move(config.error()));
    auto candidate = self.get().create(*name, *config);
    if (!candidate)
      return std::unexpected(std::move(candidate.error()));
    auto state = store.get("state");
    if (!state)
      return std::unexpected(std::move(state.error()));
    if (auto result = (*candidate)->diskLoad(std::move(*state)); !result)
      return result;
    vehicle = std::move(*candidate);
    return {};
  };
  auto result = load(diskStore);
  if (result)
    return vehicle;
  StarException lastError = std::move(result.error());
  auto rebuilt = m_rebuilder->rebuild(diskStore, lastError, [&](Json const& store) -> std::expected<void, StarException> {
    auto attempt = load(store);
    if (!attempt)
      lastError = attempt.error();
    return attempt;
  });
  if (!rebuilt)
    return std::unexpected(std::move(rebuilt.error()));
  if (!*rebuilt)
    return std::unexpected(std::move(lastError));
  return vehicle;
}

}

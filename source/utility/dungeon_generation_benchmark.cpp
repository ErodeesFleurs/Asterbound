#include <cstdio>
import star.json;
import star.output_proxy;
import star.native_runtime;
import star.file;
import star.xx_hash;
import star.ordered_map;
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
import star.string;
import star.ordered_set;
import star.format;
import star.io_device;
import star.ref_ptr;
import star.logging;
import star.listener;
import star.lru_cache;
import star.perlin;
import star.id_map;
import star.set;
import star.casting;
import star.strong_typedef;
import star.image;
import star.rpc_promise;
import star.worker_pool;

import std;
import star.sector_array_2d;
import star.net_compatibility;
import star.btree;
import star.data_stream_devices;
import star.btree_database;
import star.map;

import star.lua_root;
import star.celestial_coordinate;
import star.sky_types;
import star.animation;
import star.particle;
import star.weather_types;
import star.force_regions;
import star.world_parameters;
import star.celestial_parameters;
import star.celestial_types;
import star.option_parser;
import star.version_option_parser;
import star.asset_source;
import star.assets;
import star.root_base;
import star.configuration;
import star.root;
import star.root_loader;
import star.world_layout;
import star.damage_types;
import star.world_geometry;
import star.status_types;
import star.damage;
import star.light_source;
import star.entity;
import star.interaction_types;
import star.item_descriptor;
import star.quest_descriptor;
import star.interactive_entity;
import star.collision_block;
import star.tile_entity;
import star.tile_damage;
import star.inspectable_entity;
import star.plant;
import star.plant_database;
import star.biome_placement;
import star.sky_parameters;
import star.world_template;
import star.tile_sector_array;
import star.tile_modification;
import star.world;
import star.liquid_types;
import star.collision_generator;
import star.world_tiles;
import star.chat_types;
import star.uuid;
import star.warping;
import star.wiring;
import star.versioning_database;
import star.world_storage;
import star.player_types;
import star.system_world;
import star.damage_manager;
import star.net_packets;
import star.cellular_light_array;
import star.cellular_lighting;
import star.cellular_liquid;
import star.world_structure;
import star.lua_components;
import star.drawable;
import star.entity_rendering_types;
import star.sky_render_data;
import star.parallax;
import star.world_render_data;
import star.world_client_state;
import star.interpolation_tracker;
import star.spawn_type_database;
import star.spawner;
import star.world_server;


import star.celestial_database;

import star.numeric_option;
using namespace Star;

[[nodiscard]]
static std::expected<void, StarException> run(std::span<char const* const> argv) {
  auto nativeRuntime = NativeRuntime::create();
  if (!nativeRuntime)
    return std::unexpected(std::move(nativeRuntime.error()));
  constexpr unsigned default_repetitions = 5;
  constexpr unsigned default_report_every = 1;
  String const default_dungeon_world = "outpost";

  auto loader = RootLoader::create({{}, {}, {}, LogLevel::Error, false, {}});
  if (!loader)
    return std::unexpected(std::move(loader.error()));
  auto& root_loader = *loader;
  auto world_parameter = root_loader.add_parameter("dungeonWorld", "dungeonWorld", OptionParser::Optional, Star::format("dungeonWorld to test, default is {}", default_dungeon_world));
  if (!world_parameter)
    return std::unexpected(std::move(world_parameter.error()));
  auto repetitions_parameter = root_loader.add_parameter("repetitions", "repetitions", OptionParser::Optional, Star::format("number of times to generate, default {}", default_repetitions));
  if (!repetitions_parameter)
    return std::unexpected(std::move(repetitions_parameter.error()));
  auto report_parameter = root_loader.add_parameter("reportevery", "report repetitions", OptionParser::Optional, Star::format("number of repetitions before each progress report, default {}", default_report_every));
  if (!report_parameter)
    return std::unexpected(std::move(report_parameter.error()));

  auto initialized = root_loader.command_init_or_die(argv);
  if (!initialized)
    return std::unexpected(std::move(initialized.error()));
  auto& [root, options] = *initialized;

  if (auto written = Star::print("Fully loading root..."); !written)
    return std::unexpected(std::move(written.error()));
  auto loaded = root->fullyLoad();
  if (!loaded)
    return std::unexpected(std::move(loaded.error()));
  if (auto written = Star::print(" done\n"); !written)
    return std::unexpected(std::move(written.error()));

  auto repetitions = numeric_option(options, "repetitions", default_repetitions, EmptyParameterList::Reject);
  if (!repetitions)
    return std::unexpected(std::move(repetitions.error()));
  auto report_every = numeric_option(options, "reportevery", default_report_every, EmptyParameterList::Reject);
  if (!report_every)
    return std::unexpected(std::move(report_every.error()));
  auto dungeon_world = options.first_parameter("dungeonWorld");
  auto const& dungeon_world_name = dungeon_world ? dungeon_world->get() : default_dungeon_world;

  auto start = std::chrono::steady_clock::now();
  auto lastReport = std::chrono::steady_clock::now();
  if (auto written = Star::print("testing {} generations of dungeonWorld {}\n", *repetitions, dungeon_world_name); !written)
    return std::unexpected(std::move(written.error()));

  for (unsigned i = 0; i < *repetitions; ++i) {
    if (i > 0 && i % *report_every == 0) {
      float gps = *report_every / std::chrono::duration<double>(std::chrono::steady_clock::now() - lastReport).count();
      lastReport = std::chrono::steady_clock::now();
      if (auto written = Star::print("[{}] {}s | Generations Per Second: {}\n", i, std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count(), gps); !written)
        return std::unexpected(std::move(written.error()));
    }

    auto worldParameters = generateFloatingDungeonWorldParameters(*root, dungeon_world_name);
    if (!worldParameters)
      return std::unexpected(std::move(worldParameters.error()));
    auto worldTemplate = WorldTemplate::create(*root, *worldParameters, SkyParameters(), 1234);
    if (!worldTemplate)
      return std::unexpected(std::move(worldTemplate.error()));
    auto storage = File::ephemeral_file(root->random());
    if (!storage)
      return std::unexpected(std::move(storage.error()));
    auto server = WorldServer::create(*root, std::move(*worldTemplate), *storage);
    if (!server)
      return std::unexpected(std::move(server.error()));
    auto closed = (*server)->close();
    if (!closed)
      return std::unexpected(std::move(closed.error()));
  }

  if (auto written = Star::print("Finished {} generations of dungeonWorld {} in {} seconds", *repetitions, dungeon_world_name, std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count()); !written)
    return std::unexpected(std::move(written.error()));
  return {};
}

int main(int argc, char** argv) {
  auto result = run({argv, static_cast<std::size_t>(argc)});
  if (!result) {
    if (auto written = Star::print(*stderr, "Exception caught: {}\n", Star::output_exception(ExceptionDiagnostic(result.error()), true)); !written)
      return 1;
    return 1;
  }
  return 0;
}

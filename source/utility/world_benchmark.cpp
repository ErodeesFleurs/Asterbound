#include <cstdio>
import star.json;
import star.output_proxy;
import star.native_runtime;
import star.file;
import star.xx_hash;
import star.numeric_option;
import star.logging;
import star.string;
import star.ordered_map;
import star.ordered_set;
import star.format;
import star.rect;
import star.bi_map;
import star.thread;
import star.io_device;
import star.list;
import star.image_processing;
import star.hash;
import star.data_stream;
import star.string_view;
import star.directives;
import star.asset_path;
import star.ref_ptr;
import star.listener;
import star.id_map;
import star.casting;
import star.image;
import star.color;
import star.vector;
import star.game_types;
import star.random;
import star.weighted_pool;
import star.strong_typedef;
import star.rpc_promise;
import star.set;
import star.worker_pool;

import std;
import star.sector_array_2d;
import star.net_compatibility;
import star.perlin;
import star.btree;
import star.lru_cache;
import star.data_stream_devices;
import star.btree_database;
import star.map;

import star.option_parser;
import star.version_option_parser;
import star.asset_source;
import star.assets;
import star.root_base;
import star.configuration;
import star.root;
import star.root_loader;
import star.lua_root;
import star.tile_sector_array;
import star.collision_block;
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
import star.tile_entity;
import star.tile_modification;
import star.force_regions;
import star.world;
import star.liquid_types;
import star.tile_damage;
import star.animation;
import star.particle;
import star.weather_types;
import star.celestial_coordinate;
import star.sky_types;
import star.world_parameters;
import star.celestial_parameters;
import star.world_layout;
import star.collision_generator;
import star.world_tiles;
import star.celestial_types;
import star.chat_types;
import star.uuid;
import star.warping;
import star.wiring;
import star.inspectable_entity;
import star.plant;
import star.plant_database;
import star.biome_placement;
import star.versioning_database;
import star.world_storage;
import star.player_types;
import star.sky_parameters;
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
import star.world_template;

using namespace Star;

[[nodiscard]]
static std::expected<void, StarException> run(std::span<char const* const> argv) {
  auto nativeRuntime = NativeRuntime::create();
  if (!nativeRuntime)
    return std::unexpected(std::move(nativeRuntime.error()));
  auto loader = RootLoader::create({{}, {}, {}, LogLevel::Error, false, {}});
  if (!loader)
    return std::unexpected(std::move(loader.error()));
  auto& root_loader = *loader;
  root_loader.add_argument("dungeon", OptionParser::Required, "name of the dungeon to spawn in the world to benchmark");
  auto seed_parameter = root_loader.add_parameter("seed", "seed", OptionParser::Optional, "world seed used to create the WorldTemplate");
  if (!seed_parameter)
    return std::unexpected(std::move(seed_parameter.error()));
  auto steps_parameter = root_loader.add_parameter("steps", "steps", OptionParser::Optional, "number of steps to run the world for, defaults to 5,000");
  if (!steps_parameter)
    return std::unexpected(std::move(steps_parameter.error()));
  auto times_parameter = root_loader.add_parameter("times", "times", OptionParser::Optional, "how many times to perform the run, defaults to once");
  if (!times_parameter)
    return std::unexpected(std::move(times_parameter.error()));
  auto signal_parameter = root_loader.add_parameter("signalevery", "signal steps", OptionParser::Optional, "number of steps to wait between scanning and signaling all entities to stay alive, default 120");
  if (!signal_parameter)
    return std::unexpected(std::move(signal_parameter.error()));
  auto report_parameter = root_loader.add_parameter("reportevery", "report steps", OptionParser::Optional, "number of steps between each progress report, default 0 (do not report progress)");
  if (!report_parameter)
    return std::unexpected(std::move(report_parameter.error()));
  auto fidelity_parameter = root_loader.add_parameter("fidelity", "server fidelity", OptionParser::Optional, "fidelity to run the server with, default high");
  if (!fidelity_parameter)
    return std::unexpected(std::move(fidelity_parameter.error()));
  auto profiling_switch = root_loader.add_switch("profiling", "whether to use lua profiling, prints the profile with info logging");
  if (!profiling_switch)
    return std::unexpected(std::move(profiling_switch.error()));
  auto unsafe_switch = root_loader.add_switch("unsafe", "enables unsafe lua libraries");
  if (!unsafe_switch)
    return std::unexpected(std::move(unsafe_switch.error()));
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

  auto dungeonArgument = options.arguments.first();
  if (!dungeonArgument)
    return std::unexpected(std::move(dungeonArgument.error()));
  auto const& dungeon = dungeonArgument->get();
  auto worldParameters = generateFloatingDungeonWorldParameters(*root, dungeon);
  if (!worldParameters)
    return std::unexpected(std::move(worldParameters.error()));
  auto world_seed = numeric_option<std::uint64_t>(options, "seed", root->random().next_u64(), EmptyParameterList::Reject);
  if (!world_seed)
    return std::unexpected(std::move(world_seed.error()));
  auto worldTemplate = WorldTemplate::create(*root, *worldParameters, SkyParameters(), *world_seed);
  if (!worldTemplate)
    return std::unexpected(std::move(worldTemplate.error()));

  auto configuration = root->configuration();
  if (!configuration)
    return std::unexpected(std::move(configuration.error()));
  auto fidelity = options.first_parameter("fidelity");
  auto configured_fidelity = (*configuration)->set("serverFidelity", fidelity ? Json(fidelity->get()) : Json("high"));
  if (!configured_fidelity)
    return std::unexpected(std::move(configured_fidelity.error()));
  if (options.switches.contains("unsafe")) {
    auto configured = (*configuration)->set("safeScripts", false);
    if (!configured)
      return std::unexpected(std::move(configured.error()));
  }
  if (options.switches.contains("profiling")) {
    auto enabled = (*configuration)->set("scriptProfilingEnabled", true);
    if (!enabled)
      return std::unexpected(std::move(enabled.error()));
    auto interval = (*configuration)->set("scriptInstructionMeasureInterval", 100);
    if (!interval)
      return std::unexpected(std::move(interval.error()));
  }

  auto times = numeric_option<std::uint64_t>(options, "times", 1, EmptyParameterList::Reject);
  if (!times)
    return std::unexpected(std::move(times.error()));
  auto steps = numeric_option<std::uint64_t>(options, "steps", 5000, EmptyParameterList::Reject);
  if (!steps)
    return std::unexpected(std::move(steps.error()));
  auto signal_every = numeric_option<std::uint64_t>(options, "signalevery", 120, EmptyParameterList::Reject);
  if (!signal_every)
    return std::unexpected(std::move(signal_every.error()));
  auto report_every = numeric_option<std::uint64_t>(options, "reportevery", 0, EmptyParameterList::Reject);
  if (!report_every)
    return std::unexpected(std::move(report_every.error()));

  std::chrono::duration<double> sumTime = std::chrono::duration<double>::zero();
  for (std::uint64_t i = 0; i < *times; ++i) {
    auto storage = File::ephemeral_file(root->random());
    if (!storage)
      return std::unexpected(std::move(storage.error()));
    auto server = WorldServer::create(*root, *worldTemplate, *storage);
    if (!server)
      return std::unexpected(std::move(server.error()));
    auto& worldServer = **server;

    if (auto written = Star::print("Starting world simulation for {} steps\n", *steps); !written)
      return std::unexpected(std::move(written.error()));
    auto start = std::chrono::steady_clock::now();
    auto lastReport = std::chrono::steady_clock::now();
    std::uint64_t entityCount = 0;
    for (std::uint64_t j = 0; j < *steps; ++j) {
      if (j % *signal_every == 0) {
        entityCount = 0;
        auto signaled = worldServer.forEachEntity(RectF(Vec2F(), Vec2F(worldServer.geometry().size())), [&](auto const& entity) -> std::expected<void, StarException> {
            ++entityCount;
            auto result = worldServer.signalRegion(RectI::integral(entity->metaBoundBox().translated(entity->position())));
            if (!result)
              return std::unexpected(std::move(result.error()));
            return {};
          });
        if (!signaled)
          return std::unexpected(std::move(signaled.error()));
      }

      if (*report_every != 0 && j % *report_every == 0) {
        float fps = *report_every / std::chrono::duration<double>(std::chrono::steady_clock::now() - lastReport).count();
        lastReport = std::chrono::steady_clock::now();
        if (auto written = Star::print("[{}] {}s | FPS: {} | Entities: {}\n", j, std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count(), fps, entityCount); !written)
          return std::unexpected(std::move(written.error()));
      }
      auto updated = worldServer.update(root->timing().serverTimestep * root->timing().timescale);
      if (!updated)
        return std::unexpected(std::move(updated.error()));
    }
    std::chrono::duration<double> totalTime = std::chrono::steady_clock::now() - start;
    if (auto written = Star::print("Finished run of running dungeon world '{}' with seed {} for {} steps in {} seconds, average FPS: {}\n",
          dungeon, *world_seed, *steps, totalTime.count(), *steps / totalTime.count()); !written)
      return std::unexpected(std::move(written.error()));
    sumTime += totalTime;
    auto closed = worldServer.close();
    if (!closed)
      return std::unexpected(std::move(closed.error()));
  }

  if (*times != 1) {
    if (auto written = Star::print("Average of all runs - time: {}, FPS: {}\n", (sumTime / *times).count(), *steps / (sumTime / *times).count()); !written)
      return std::unexpected(std::move(written.error()));
  }
  return {};
}

int main(int argc, char** argv) {
  auto result = run({argv, static_cast<std::size_t>(argc)});
  if (!result) {
    if (auto written = Star::print(stderr, "Exception caught: {}\n", Star::output_exception(ExceptionDiagnostic(result.error()), true)); !written)
      return 1;
    return 1;
  }
  return 0;
}

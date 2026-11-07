import star.json;
import star.output_proxy;
import star.native_runtime;
import star.file;
import star.xx_hash;
import star.ordered_map;
import star.rect;
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
import star.string;
import star.ordered_set;
import star.format;
import star.version;
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

import star.lexical_cast;
using namespace Star;

[[nodiscard]]
static std::expected<void, StarException> run(int argc, char** argv) {
    auto nativeRuntime = NativeRuntime::create();
    if (!nativeRuntime)
      return std::unexpected(std::move(nativeRuntime.error()));
    auto loader = RootLoader::create({{}, {}, {}, LogLevel::Error, false, {}});
    if (!loader)
      return std::unexpected(std::move(loader.error()));
    auto& rootLoader = *loader;
    auto coordinateParameter = rootLoader.addParameter("coordinate", "coordinate", OptionParser::Optional, "world coordinate to test");
    if (!coordinateParameter)
      return std::unexpected(std::move(coordinateParameter.error()));
    auto regionsParameter = rootLoader.addParameter("regions", "regions", OptionParser::Optional, "number of regions to generate, default 1000");
    if (!regionsParameter)
      return std::unexpected(std::move(regionsParameter.error()));
    auto regionSizeParameter = rootLoader.addParameter("regionsize", "size", OptionParser::Optional, "width / height of each generation region, default 10");
    if (!regionSizeParameter)
      return std::unexpected(std::move(regionSizeParameter.error()));
    auto reportParameter = rootLoader.addParameter("reportevery", "report regions", OptionParser::Optional, "number of generation regions before each progress report, default 20");
    if (!reportParameter)
      return std::unexpected(std::move(reportParameter.error()));

    auto initialized = rootLoader.commandInitOrDie(argc, argv);
    if (!initialized)
      return std::unexpected(std::move(initialized.error()));
    auto& [root, options] = *initialized;

    coutf("Fully loading root...");
    auto loaded = root->fullyLoad();
    if (!loaded)
      return std::unexpected(std::move(loaded.error()));
    coutf(" done\n");

    auto celestialDatabase = CelestialMasterDatabase::create(*root);
    if (!celestialDatabase)
      return std::unexpected(std::move(celestialDatabase.error()));

    CelestialCoordinate coordinate;
    if (auto coordinateOption = options.parameters.maybe("coordinate")) {
      auto text = coordinateOption->first();
      if (!text)
        return std::unexpected(std::move(text.error()));
      auto parsed = CelestialCoordinate::create(Json(text->get()));
      if (!parsed)
        return std::unexpected(std::move(parsed.error()));
      coordinate = std::move(*parsed);
    } else {
      auto found = (*celestialDatabase)->findRandomWorld(100, 50, [&](CelestialCoordinate const& coord) -> std::expected<bool, StarException> {
          auto parameters = (*celestialDatabase)->parameters(coord);
          if (!parameters)
            return std::unexpected(std::move(parameters.error()));
          return *parameters && (**parameters).isVisitable();
        });
      if (!found)
        return std::unexpected(std::move(found.error()));
      if (!*found)
        return std::unexpected(StarException("Could not find world to generate, try again"));
      coordinate = **found;
    }

    unsigned regionsToGenerate = 1000;
    if (auto regionsOption = options.parameters.maybe("regions")) {
      auto text = regionsOption->first();
      if (!text)
        return std::unexpected(std::move(text.error()));
      auto parsed = lexicalCast<unsigned>(text->get());
      if (!parsed)
        return std::unexpected(std::move(parsed.error()));
      regionsToGenerate = *parsed;
    }

    unsigned regionSize = 10;
    if (auto regionSizeOption = options.parameters.maybe("regionsize")) {
      auto text = regionSizeOption->first();
      if (!text)
        return std::unexpected(std::move(text.error()));
      auto parsed = lexicalCast<unsigned>(text->get());
      if (!parsed)
        return std::unexpected(std::move(parsed.error()));
      regionSize = *parsed;
    }

    unsigned reportEvery = 20;
    if (auto reportEveryOption = options.parameters.maybe("reportevery")) {
      auto text = reportEveryOption->first();
      if (!text)
        return std::unexpected(std::move(text.error()));
      auto parsed = lexicalCast<unsigned>(text->get());
      if (!parsed)
        return std::unexpected(std::move(parsed.error()));
      reportEvery = *parsed;
    }

    coutf("testing generation on coordinate {}\n", coordinate);

    auto worldParameters = (*celestialDatabase)->parameters(coordinate);
    if (!worldParameters)
      return std::unexpected(std::move(worldParameters.error()));
    if (!*worldParameters)
      return std::unexpected(StarException("Could not find world to generate, try again"));
    auto createdTemplate = WorldTemplate::create(*root, (**worldParameters).visitableParameters(), SkyParameters(), (**worldParameters).seed());
    if (!createdTemplate)
      return std::unexpected(std::move(createdTemplate.error()));
    auto worldTemplate = std::move(*createdTemplate);

    auto rand = RandomSource(worldTemplate->worldSeed());

    auto storage = File::ephemeral_file(root->random());
    if (!storage)
      return std::unexpected(std::move(storage.error()));
    auto server = WorldServer::create(*root, std::move(worldTemplate), *storage);
    if (!server)
      return std::unexpected(std::move(server.error()));
    auto& worldServer = **server;
    Vec2U worldSize = worldServer.geometry().size();

    auto start = std::chrono::steady_clock::now();
    auto lastReport = std::chrono::steady_clock::now();

    coutf("Starting world generation for {} regions\n", regionsToGenerate);

    for (unsigned i = 0; i < regionsToGenerate; ++i) {
      if (i != 0 && i % reportEvery == 0) {
        float gps = reportEvery / std::chrono::duration<double>(std::chrono::steady_clock::now() - lastReport).count();
        lastReport = std::chrono::steady_clock::now();
        coutf("[{}] {}s | Generatons Per Second: {}\n", i, std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count(), gps);
      }

      auto x = rand.randInt(0, worldSize[0]);
      if (!x)
        return std::unexpected(std::move(x.error()));
      auto y = rand.randInt(0, worldSize[1]);
      if (!y)
        return std::unexpected(std::move(y.error()));
      RectI region = RectI::withCenter(Vec2I(*x, *y), Vec2I::filled(regionSize));
      auto generated = worldServer.generateRegion(region);
      if (!generated)
        return std::unexpected(std::move(generated.error()));
    }

    coutf("Finished generating {} regions with size {}x{} in world '{}' in {} seconds", regionsToGenerate, regionSize, regionSize, coordinate, std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count());
    return worldServer.close();
}

int main(int argc, char** argv) {
  auto result = run(argc, argv);
  if (!result) {
    cerrf("Exception caught: {}\n", outputException(ExceptionDiagnostic(result.error()), true));
    return 1;
  }
  return 0;
}

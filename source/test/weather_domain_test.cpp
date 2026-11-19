import star.json;
import star.string;
import star.ordered_map;
import star.thread;
import star.io_device;
import star.ref_ptr;
import star.logging;
import star.listener;
import std;
import star.format;
import star.version;
import star.color;
import star.bi_map;
import star.game_types;
import star.random;
import star.weighted_pool;
import star.list;
import star.image_processing;
import star.hash;
import star.data_stream;
import star.string_view;
import star.directives;
import star.asset_path;
import star.lru_cache;
import star.perlin;
import star.id_map;
import star.set;
import star.casting;
import star.strong_typedef;

import star.asset_source;
import star.assets;
import star.root_base;
import star.configuration;
import star.root;
import star.sky_types;
import star.celestial_coordinate;
import star.sky_parameters;
import star.animation;
import star.particle;
import star.weather_types;
import star.force_regions;
import star.world_parameters;
import star.celestial_parameters;
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
import star.celestial_types;
import star.world_template;
import star.game_test;

#include "gtest/gtest.h"


import star.biome_database;

using namespace Star;

namespace {

[[nodiscard]] static std::expected<std::shared_ptr<WorldTemplate>, StarException> makeGardenWorld(
    Root& root, std::shared_ptr<::Star::TerrestrialWorldParameters> parameters, std::uint64_t seed) {
  return WorldTemplate::create(root, std::move(parameters), SkyParameters(), seed);
}

[[nodiscard]] static std::expected<std::optional<std::reference_wrapper<WorldTemplate::WeatherLayer const>>, StarException> layerAt(WorldTemplate const& world, int height) {
  return world.weatherLayerAt({0, height});
}

}

const GameTestRegistration weatherDomainDefaultTerrestrialOwnership("WeatherDomainTest", "DefaultTerrestrialOwnership", [](GameTest& fixture) {
  std::uint64_t seed = 1234;
  auto parameters = generateTerrestrialWorldParameters(fixture.root(), "garden", "small", seed);
  ASSERT_TRUE(parameters);
  auto world = makeGardenWorld(fixture.root(), *parameters, seed);
  ASSERT_TRUE(world);

  auto surface = layerAt(**world, (*parameters)->surfaceLayer.layerMinHeight);
  auto atmosphere = layerAt(**world, (*parameters)->atmosphereLayer.layerMinHeight);
  auto subsurface = layerAt(**world, (*parameters)->subsurfaceLayer.layerMinHeight);
  auto space = layerAt(**world, (*parameters)->spaceLayer.layerMinHeight);
  auto core = layerAt(**world, (*parameters)->coreLayer.layerMinHeight);

  ASSERT_TRUE(surface);
  ASSERT_TRUE(atmosphere);
  ASSERT_TRUE(subsurface);
  ASSERT_TRUE(space);
  ASSERT_TRUE(core);
  ASSERT_TRUE(*surface);
  ASSERT_TRUE(*atmosphere);
  ASSERT_TRUE(*subsurface);
  ASSERT_TRUE(*space);
  ASSERT_TRUE(*core);
  EXPECT_EQ((*surface)->get().domain, std::optional<String>(String("surface")));
  EXPECT_EQ((*atmosphere)->get().domain, (*surface)->get().domain);
  EXPECT_EQ((*subsurface)->get().domain, (*surface)->get().domain);
  EXPECT_FALSE((*space)->get().domain);
  EXPECT_FALSE((*core)->get().domain);

  for (auto const& underground : (*parameters)->undergroundLayers) {
    auto layer = layerAt(**world, underground.layerMinHeight);
    ASSERT_TRUE(layer);
    ASSERT_TRUE(*layer);
    EXPECT_FALSE((*layer)->get().domain);
  }

  auto surfaceDomain = (*world)->weatherDomain("surface");
  ASSERT_TRUE(surfaceDomain);
  ASSERT_TRUE(*surfaceDomain);
  EXPECT_EQ((*surfaceDomain)->get().effectsMinHeight, (*parameters)->subsurfaceLayer.layerMinHeight);
});

const GameTestRegistration weatherDomainExplicitLayerWeatherCreatesIndependentDomain("WeatherDomainTest", "ExplicitLayerWeatherCreatesIndependentDomain", [](GameTest& fixture) {
  std::uint64_t seed = 5678;
  auto parameters = generateTerrestrialWorldParameters(fixture.root(), "garden", "small", seed);
  ASSERT_TRUE(parameters);
  (*parameters)->atmosphereLayer.primaryRegion.biome = "garden";
  auto world = makeGardenWorld(fixture.root(), *parameters, seed);
  ASSERT_TRUE(world);

  auto atmosphere = layerAt(**world, (*parameters)->atmosphereLayer.layerMinHeight);
  ASSERT_TRUE(atmosphere);
  ASSERT_TRUE(*atmosphere);
  EXPECT_EQ((*atmosphere)->get().domain, std::optional<String>(String("atmosphere")));

  auto atmosphereDomain = (*world)->weatherDomain("atmosphere");
  ASSERT_TRUE(atmosphereDomain);
  ASSERT_TRUE(*atmosphereDomain);
  EXPECT_FALSE((*atmosphereDomain)->get().pool.empty());
  EXPECT_EQ((*atmosphereDomain)->get().effectsMinHeight, (*parameters)->atmosphereLayer.layerMinHeight);
});

const GameTestRegistration weatherDomainWeatherPropertyPresenceIsDistinctFromOmission("WeatherDomainTest", "WeatherPropertyPresenceIsDistinctFromOmission", [](GameTest& fixture) {
  auto biomes = fixture.root().biomeDatabase();
  ASSERT_TRUE(biomes);
  auto garden = (*biomes)->biomeHasWeather("garden");
  ASSERT_TRUE(garden);
  EXPECT_TRUE(*garden);
  auto atmosphere = (*biomes)->biomeHasWeather("atmosphere");
  ASSERT_TRUE(atmosphere);
  EXPECT_FALSE(*atmosphere);
});

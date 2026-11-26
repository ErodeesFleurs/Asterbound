#include "gtest/gtest.h"

import std;
import star.algorithm;
import star.animation;
import star.asset_path;
import star.assets;
import star.bi_map;
import star.byte_array;
import star.collision_block;
import star.color;
import star.data_stream_devices;
import star.directives;
import star.drawable;
import star.file;
import star.game_test;
import star.image_metadata_database;
import star.json;
import star.list;
import star.liquid_types;
import star.map;
import star.particle;
import star.particle_manager;
import star.rect;
import star.root;
import star.string;
import star.tile_sector_array;
import star.vector;
import star.world_geometry;
import star.unicode;
import star.world_tiles;
import star.random;

using namespace Star;

namespace {

Particle moving_particle() {
  Particle particle;
  particle.type = Particle::Type::Ember;
  particle.position = Vec2F(99.5f, 20);
  particle.velocity = particle.final_velocity = Vec2F(2, 0);
  particle.size = particle.base_size = 2;
  particle.time_to_live = 10;
  particle.angular_velocity = 8;
  particle.light = Color::palette_white;
  particle.fade = 0.2f;
  particle.collides_foreground = particle.collides_liquid = false;
  return particle;
}

const GameTestRegistration
  particle_json_names("ParticleConfig", "EnumNamesPreserveDefaultsCaseAndJsonRoundTrip", [](GameTest& fixture) {
    auto defaults = Particle::create(fixture.root(), JsonObject{});
    ASSERT_TRUE(defaults) << defaults.error().what();
    EXPECT_EQ(defaults->type, Particle::Type::Variance);
    EXPECT_EQ(defaults->destruction_action, Particle::DestructionAction::None);
    EXPECT_EQ(defaults->layer, Particle::Layer::Middle);
    auto default_json = defaults->to_json();
    ASSERT_TRUE(default_json) << default_json.error().what();
    EXPECT_EQ(default_json->get_string("type"), String("variance"));
    EXPECT_EQ(default_json->get_string("destructionAction"), String("none"));
    EXPECT_EQ(default_json->get_string("layer"), String("middle"));

    auto configured = Particle::create(fixture.root(),
                                       JsonObject{{"type", "EmBeR"},
                                                  {"destructionAction", "ShRiNk"},
                                                  {"layer", "FrOnT"},
                                                  {"size", 2.0},
                                                  {"timeToLive", 3.0},
                                                  {"destructionTime", 1.0}});
    ASSERT_TRUE(configured) << configured.error().what();
    EXPECT_EQ(configured->type, Particle::Type::Ember);
    EXPECT_EQ(configured->destruction_action, Particle::DestructionAction::Shrink);
    EXPECT_EQ(configured->layer, Particle::Layer::Front);
    EXPECT_FALSE(configured->collides_foreground);
    EXPECT_TRUE(configured->collides_liquid);
    auto stored = configured->to_json();
    ASSERT_TRUE(stored) << stored.error().what();
    EXPECT_EQ(stored->get_string("type"), String("ember"));
    EXPECT_EQ(stored->get_string("destructionAction"), String("shrink"));
    EXPECT_EQ(stored->get_string("layer"), String("front"));
    auto restored = Particle::create(fixture.root(), *stored);
    ASSERT_TRUE(restored) << restored.error().what();
    auto restored_json = restored->to_json();
    ASSERT_TRUE(restored_json) << restored_json.error().what();
    EXPECT_EQ(*restored_json, *stored);
  });

const GameTestRegistration
  particle_json_name_errors("ParticleConfig", "EnumNamesRejectUnknownUnicodeAndInvalidEnumValues", [](GameTest& fixture) {
    for (String const& key : {String("type"), String("destructionAction"), String("layer")}) {
      for (String const& name : {String("unknown"), String("日本語"), String(std::string_view("none\0", 5))}) {
        auto parsed = Particle::create(fixture.root(), JsonObject{{key, name}});
        ASSERT_FALSE(parsed);
        EXPECT_TRUE(parsed.error().is<MapException>());
      }
      auto invalid = Particle::create(fixture.root(), JsonObject{{key, String(std::string_view("none\xff", 5))}});
      ASSERT_FALSE(invalid);
      EXPECT_TRUE(invalid.error().is<UnicodeException>());
    }

    for (unsigned field = 0; field < 3; ++field) {
      Particle invalid;
      if (field == 0)
        invalid.type = static_cast<Particle::Type>(-1);
      else if (field == 1)
        invalid.destruction_action = static_cast<Particle::DestructionAction>(-1);
      else
        invalid.layer = static_cast<Particle::Layer>(-1);
      auto stored = invalid.to_json();
      ASSERT_FALSE(stored);
      EXPECT_TRUE(stored.error().is<MapException>());
    }
  });

const GameTestRegistration particle_json_defaults(
  "ParticleConfig", "TypedDefaultsPreserveNullsAndVelocityPrecedence", [](GameTest& fixture) {
    auto& root = fixture.root();
    auto variance = Particle::create(root, JsonObject{{"velocity", JsonArray{4, -2}}});
    ASSERT_TRUE(variance) << variance.error().what();
    EXPECT_EQ(variance->color, Color::palette_clear);
    EXPECT_EQ(variance->light, Color::palette_clear);
    EXPECT_EQ(variance->size, 0);
    EXPECT_EQ(variance->base_size, 0);
    EXPECT_EQ(variance->length, 10);
    EXPECT_EQ(variance->position, Vec2F());
    EXPECT_EQ(variance->approach, Vec2F());
    EXPECT_EQ(variance->velocity, Vec2F(4, -2));
    EXPECT_EQ(variance->final_velocity, Vec2F());
    EXPECT_FALSE(variance->collides_liquid);

    auto textured = Particle::create(root, JsonObject{
      {"type", "textured"}, {"string", "unused.png"}, {"animation", Json()}, {"text", "also-unused.png"},
      {"image", "spark.png?flipx"}, {"size", Json()}, {"velocity", "ignored"},
      {"initialVelocity", JsonArray{2, -5}}, {"layer", "front"}, {"underwaterOnly", true},
      {"destructionAction", "image"}, {"destructionImage", "blast.png?flipy"}}, "/fx/particles.config");
    ASSERT_TRUE(textured) << textured.error().what();
    EXPECT_EQ(textured->size, 1);
    EXPECT_EQ(textured->base_size, 1);
    EXPECT_EQ(textured->color, Color::palette_white);
    EXPECT_EQ(textured->velocity, Vec2F(2, -5));
    EXPECT_EQ(textured->final_velocity, textured->velocity);
    EXPECT_TRUE(textured->collides_foreground);
    EXPECT_EQ(textured->string, String("/fx/spark.png?flipx"));
    EXPECT_EQ(textured->image, AssetPath("/fx/spark.png?flipx"));
    EXPECT_EQ(textured->destruction_image, AssetPath("/fx/blast.png?flipy"));

    auto explicit_final = Particle::create(root, JsonObject{
      {"type", "ember"}, {"velocity", Json()}, {"initialVelocity", JsonArray{1, 2}}, {"finalVelocity", JsonArray{3, 4}}});
    ASSERT_TRUE(explicit_final) << explicit_final.error().what();
    EXPECT_EQ(explicit_final->velocity, Vec2F(1, 2));
    EXPECT_EQ(explicit_final->final_velocity, Vec2F(3, 4));

    for (String const& key : {String("color"), String("light"), String("position"), String("velocity"),
                              String("initialVelocity"), String("finalVelocity"), String("approach")}) {
      auto parsed = Particle::create(root, JsonObject{{key, Json()}});
      EXPECT_FALSE(parsed) << key.utf8();
    }
    EXPECT_FALSE(Particle::create(root, JsonObject{{"velocity", JsonArray{1, 2}}, {"initialVelocity", Json()}}));
    EXPECT_FALSE(Particle::create(root, JsonObject{{"string", 42}, {"image", "valid.png"}}));
  });

const GameTestRegistration legacy_particle_wire(
  "ParticleWire", "FixedBytesTruncateAtomicallyAndRebuildDrawableAndShrinkState", [](GameTest& fixture) {
    constexpr char legacy[] =
      "\x04\x40\x40\0\0"                              // Textured (signed VLQ), size = 3
      "\x22" "/rendering/sprites/error.png?flipx"      // length-prefixed texture
      "\x3f\x80\0\0\x3f\0\0\0\x3e\x80\0\0\x3f\x80\0\0" // color = (1, .5, .25, 1)
      "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"              // clear light
      "\x3e\0\0\0"                                   // fade = .125
      "\x41\0\0\0\x41\x40\0\0"                       // position = (8, 12)
      "\x3f\x80\0\0\xc0\0\0\0"                       // velocity = (1, -2)
      "\x40\x40\0\0\xc0\x80\0\0"                     // final velocity = (3, -4)
      "\x3e\x80\0\0\x3f\0\0\0"                       // approach = (.25, .5)
      "\x3f\0\0\0\0\x01"                             // rotation = .5, flippable = false, flip = true
      "\x3f\x80\0\0\x41\x20\0\0"                     // angular velocity = 1, length = 10
      "\x06\x0a" "/after.png"                        // Shrink (signed VLQ), destruction image
      "\x40\0\0\0\x3e\x80\0\0"                       // destruction time = 2, lifetime = .25
      "\0\0\0\x02\0\x01\0\x01";                      // Front (fixed int), fg/liquid/underwater/wind
    ByteArray bytes({legacy, sizeof(legacy) - 1});
    auto& root = fixture.root();
    auto animation = Animation::create(root, JsonObject{{"animationCycle", 1.0}, {"mode", "LoopForever"}});
    ASSERT_TRUE(animation) << animation.error().what();
    animation->update(0.25f);

    auto source = moving_particle();
    source.type = Particle::Type::Textured;
    source.image = AssetPath("/old.png");
    source.directives = String("?flipy");
    source.fullbright = true;
    Particle decoded;
    constexpr std::uint64_t identity = 17;
    decoded.sample_presentation(source, source.presentation_state(), WorldGeometry(), 1, 0, identity);
    decoded.string = "/old.png";
    decoded.animation.emplace(std::move(*animation));
    decoded.destruction_set = decoded.trail = true;
    decoded.base_size = 91;
    auto before = DataStreamBuffer::serialize(decoded);
    ASSERT_TRUE(before) << before.error().what();

    for (std::size_t count = 0; count < bytes.size(); ++count) {
      SCOPED_TRACE(count);
      ASSERT_FALSE(DataStreamBuffer::deserialize(decoded, bytes.span().first(count)));
      EXPECT_EQ(DataStreamBuffer::serialize(decoded), *before);
      EXPECT_EQ(decoded.image, source.image);
      EXPECT_EQ(decoded.directives, source.directives);
      EXPECT_TRUE(decoded.fullbright && decoded.trail && decoded.destruction_set);
      EXPECT_EQ(decoded.base_size, 91);
      ASSERT_TRUE(decoded.animation);
      EXPECT_FLOAT_EQ(decoded.animation->playback_state().timer, 0.25f);
    }

    ASSERT_TRUE(DataStreamBuffer::deserialize(decoded, bytes.span()));
    EXPECT_EQ(DataStreamBuffer::serialize(decoded), bytes);
    EXPECT_EQ(decoded.type, Particle::Type::Textured);
    EXPECT_EQ(decoded.string, String("/rendering/sprites/error.png?flipx"));
    EXPECT_EQ(decoded.position, Vec2F(8, 12));
    EXPECT_EQ(decoded.velocity, Vec2F(1, -2));
    EXPECT_EQ(decoded.final_velocity, Vec2F(3, -4));
    EXPECT_EQ(decoded.layer, Particle::Layer::Front);
    EXPECT_EQ(decoded.destruction_action, Particle::DestructionAction::Shrink);
    EXPECT_EQ(decoded.destruction_image, AssetPath("/after.png"));
    EXPECT_FALSE(decoded.animation || decoded.fullbright || decoded.trail || decoded.destruction_set || decoded.directives);
    EXPECT_FLOAT_EQ(decoded.base_size, 3.0f);

    auto metadata = root.image_metadata_database();
    ASSERT_TRUE(metadata) << metadata.error().what();
    auto drawable = Drawable::make_image(**metadata, decoded.image, 1, true, decoded.position);
    ASSERT_TRUE(drawable) << drawable.error().what();
    ASSERT_TRUE(drawable->is_image());
    EXPECT_EQ(AssetPath::join(drawable->image_part().image), decoded.string);
    auto assets = root.assets();
    ASSERT_TRUE(assets) << assets.error().what();
    auto texture = (*assets)->image(drawable->image_part().image);
    ASSERT_TRUE(texture) << texture.error().what();
    ASSERT_TRUE(decoded.update(root, 0.75f));
    EXPECT_FLOAT_EQ(decoded.time_to_live, -0.5f);
    EXPECT_FLOAT_EQ(decoded.size, 2.25f);

    decoded.sample_presentation(source, source.presentation_state(), WorldGeometry(), 1, 0, identity);
    EXPECT_EQ(decoded.image, source.image);

    auto animated_bytes = std::to_array(legacy);
    animated_bytes.front() = '\x06';
    ASSERT_TRUE(DataStreamBuffer::deserialize(decoded, std::span(animated_bytes).first(bytes.size())));
    EXPECT_EQ(decoded.type, Particle::Type::Animated);
    EXPECT_FALSE(decoded.animation);
    EXPECT_TRUE(decoded.directives);
    EXPECT_EQ(decoded.directives, AssetPath("/rendering/sprites/error.png?flipx").directives);
  });

const GameTestRegistration particle_sampling(
  "ParticlePresentation", "RepeatedSamplesDoNotAdvanceSimulationAndResetSnaps", [](GameTest& fixture) {
    WorldGeometry geometry(100, 100);
    auto tiles = std::make_shared<ClientTileSectorArray>(geometry.size());
    ParticleManager manager(fixture.root(), geometry, tiles);
    List<Particle> samples;
    manager.add(moving_particle());
    manager.sample_particles(0, samples);
    ASSERT_EQ(samples.size(), 1u);
    EXPECT_EQ(samples[0].position, Vec2F(99.5f, 20));

    ASSERT_TRUE(manager.update(0.5f, RectF(0, 0, 100, 100), 0));
    ASSERT_EQ(manager.count(), 1u);
    auto logical_position = manager.particles()[0].position;
    auto logical_rotation = manager.particles()[0].rotation;
    auto logical_lifetime = manager.particles()[0].time_to_live;
    auto logical_light = manager.particles()[0].light;
    auto* allocation = samples.data();
    auto capacity = samples.capacity();
    for (float alpha : {0.0f, 0.25f, 1.0f, 0.5f, 0.5f, -1.0f, 2.0f}) {
      manager.sample_particles(alpha, samples);
      float clamped = std::clamp(alpha, 0.0f, 1.0f);
      EXPECT_FLOAT_EQ(samples[0].position[0], 99.5f + clamped);
      EXPECT_FLOAT_EQ(samples[0].rotation, 4 * clamped);
      EXPECT_EQ(samples[0].light, Color::palette_white.mix(logical_light, clamped));
      EXPECT_EQ(samples.data(), allocation);
      EXPECT_EQ(samples.capacity(), capacity);
      EXPECT_EQ(manager.particles()[0].position, logical_position);
      EXPECT_EQ(manager.particles()[0].rotation, logical_rotation);
      EXPECT_EQ(manager.particles()[0].time_to_live, logical_lifetime);
      EXPECT_EQ(manager.particles()[0].light, logical_light);
      EXPECT_EQ(manager.count(), 1u);
    }

    // Skip all display frames for the next tick: history must still advance.
    ASSERT_TRUE(manager.update(0.5f, RectF(0, 0, 100, 100), 0));
    manager.sample_particles(0, samples);
    EXPECT_EQ(samples[0].position, logical_position);
    manager.reset_presentation();
    manager.sample_particles(0, samples);
    EXPECT_EQ(samples[0].position, manager.particles()[0].position);
    EXPECT_EQ(samples[0].rotation, manager.particles()[0].rotation);
  });

const GameTestRegistration wrapped_particle_sampling(
  "ParticlePresentation", "WrappedEndpointsAndDestructionUseSemanticState", [](GameTest& fixture) {
    Particle current = moving_particle();
    auto previous = current.presentation_state();
    current.position = Vec2F(0.5f, 20);
    current.rotation = 4;
    Particle sample;
    sample.sample_presentation(current, previous, WorldGeometry(100, 100), 0.5f, 0.5f, 1);
    EXPECT_EQ(sample.position, Vec2F(100, 20));
    EXPECT_FLOAT_EQ(sample.rotation, 2);

    for (auto action : {Particle::DestructionAction::Shrink, Particle::DestructionAction::Fade}) {
      current = moving_particle();
      current.time_to_live = 0.25f;
      current.destruction_action = action;
      current.destruction_time = 1;
      previous = current.presentation_state();
      ASSERT_TRUE(current.update(fixture.root(), 0.5f));
      sample.sample_presentation(current, previous, WorldGeometry(), 0.25f, 0.5f, 2);
      EXPECT_FLOAT_EQ(sample.size, 2);
      EXPECT_FLOAT_EQ(sample.color.alpha_f(), 1);
      sample.sample_presentation(current, previous, WorldGeometry(), 0.75f, 0.5f, 2);
      if (action == Particle::DestructionAction::Shrink)
        EXPECT_FLOAT_EQ(sample.size, 1.75f);
      else
        EXPECT_FLOAT_EQ(sample.color.alpha_f(), 0.875f);
    }

    // A textured destruction image is a discrete payload change even when
    // the type does not change. Do not interpolate its reset size/rotation.
    current = moving_particle();
    current.type = Particle::Type::Textured;
    current.image = AssetPath("/before.png");
    current.destruction_action = Particle::DestructionAction::Image;
    current.destruction_image = AssetPath("/after.png");
    current.destruction_time = 1;
    current.time_to_live = 0.25f;
    previous = current.presentation_state();
    sample.sample_presentation(current, previous, WorldGeometry(), 1, 0, 3);
    EXPECT_EQ(sample.image, AssetPath("/before.png"));
    ASSERT_TRUE(current.update(fixture.root(), 0.5f));
    sample.sample_presentation(current, previous, WorldGeometry(), 0, 0.5f, 3);
    EXPECT_EQ(sample.image, AssetPath("/after.png"));
    EXPECT_FLOAT_EQ(sample.size, 1);
    EXPECT_FLOAT_EQ(sample.rotation, 0);
  });

const GameTestRegistration particle_lifetime_sampling(
  "ParticlePresentation", "BirthDeathCompactionAndClearKeepStableIdentities", [](GameTest& fixture) {
    WorldGeometry geometry(100, 100);
    auto tiles = std::make_shared<ClientTileSectorArray>(geometry.size());
    ParticleManager manager(fixture.root(), geometry, tiles);
    List<Particle> samples;
    auto particle = moving_particle();
    particle.type = Particle::Type::Text;
    particle.string = "first";
    particle.time_to_live = 0.1f;
    manager.add(particle);
    particle.string = "second";
    particle.position = Vec2F(10, 20);
    particle.time_to_live = 10;
    manager.add(particle);
    manager.sample_particles(1, samples);
    ASSERT_EQ(samples.size(), 2u);
    EXPECT_EQ(samples[0].string, String("first"));
    ASSERT_TRUE(manager.update(0.5f, RectF(0, 0, 100, 100), 0));
    for (float alpha : {0.0f, 1.0f, 0.25f}) {
      manager.sample_particles(alpha, samples);
      ASSERT_EQ(samples.size(), 1u);
      EXPECT_EQ(samples[0].string, String("second"));
    }

    particle.string = "newborn";
    particle.position = Vec2F(30, 20);
    manager.add(particle);
    manager.sample_particles(0, samples);
    ASSERT_EQ(samples.size(), 2u);
    EXPECT_EQ(samples[1].position, particle.position);
    manager.clear();
    manager.sample_particles(0, samples);
    EXPECT_TRUE(samples.empty());
    manager.add(particle);
    manager.sample_particles(0, samples);
    ASSERT_EQ(samples.size(), 1u);
    EXPECT_EQ(samples[0].string, String("newborn"));

    // The same reusable display list can outlive the world/manager that filled
    // it: identities must not alias a new manager's first particle.
    ParticleManager replacement(fixture.root(), geometry, tiles);
    particle.string = "replacement";
    List<Particle> batch{particle};
    particle.string = "replacement second";
    batch.push_back(particle);
    replacement.add_particles(std::move(batch));
    replacement.sample_particles(0, samples);
    ASSERT_EQ(samples.size(), 2u);
    EXPECT_EQ(samples[0].string, String("replacement"));
    EXPECT_EQ(samples[1].string, String("replacement second"));
    ASSERT_TRUE(replacement.update(0.5f, RectF(50, 50, 60, 60), 0));
    replacement.sample_particles(0, samples);
    EXPECT_TRUE(samples.empty());
  });

const GameTestRegistration particle_trail_sampling(
  "ParticlePresentation", "RenderingNeverSpawnsTrailsAndBatchCapacityIsRetained", [](GameTest& fixture) {
    WorldGeometry geometry(100, 100);
    auto tiles = std::make_shared<ClientTileSectorArray>(geometry.size());
    ParticleManager manager(fixture.root(), geometry, tiles);
    auto particle = moving_particle();
    particle.trail = true;
    List<Particle> batch;
    batch.reserve(8);
    batch.push_back(particle);
    auto capacity = batch.capacity();
    auto* allocation = batch.data();
    manager.add_particles(std::move(batch));
    EXPECT_TRUE(batch.empty());
    EXPECT_EQ(batch.capacity(), capacity);
    EXPECT_EQ(batch.data(), allocation);
    ASSERT_TRUE(manager.update(0.5f, RectF(0, 0, 100, 100), 0));
    ASSERT_EQ(manager.count(), 2u);
    List<Particle> samples;
    for (float alpha : {0.0f, 0.25f, 0.75f, 1.0f, 0.25f}) {
      manager.sample_particles(alpha, samples);
      ASSERT_EQ(samples.size(), 2u);
      EXPECT_EQ(manager.count(), 2u);
      EXPECT_EQ(samples[0].position, manager.particles()[0].position);
      EXPECT_FALSE(manager.particles()[0].trail);
      EXPECT_TRUE(manager.particles()[1].trail);
    }
    ASSERT_TRUE(manager.update(0.5f, RectF(0, 0, 100, 100), 0));
    EXPECT_EQ(manager.count(), 2u);
  });

void expect_frame_path(Animation const& animation, String const& path) {
  auto drawable = animation.drawable(1);
  ASSERT_TRUE(drawable) << drawable.error().what();
  if (path.empty()) {
    EXPECT_FALSE(drawable->is_image());
  } else {
    ASSERT_TRUE(drawable->is_image());
    EXPECT_EQ(AssetPath::join(drawable->image_part().image), path);
  }
}

const GameTestRegistration particle_move_ownership(
  "ParticleValue", "MovesTransferPayloadAndAnimationAndPreserveSelfAssignment", [](GameTest& fixture) {
    auto source = moving_particle();
    source.type = Particle::Type::Animated;
    source.string = "/source.animation";
    source.image = AssetPath("/source.animation?flipx");
    source.directives = source.image.directives;
    source.destruction_image = AssetPath("/after.png");
    source.fullbright = source.trail = source.destruction_set = true;
    auto animation = Animation::create(fixture.root(), JsonObject{
      {"frames", "/sheet.png"}, {"frameNumber", 8}, {"animationCycle", 1.0}, {"mode", "LoopForever"}, {"centered", false}});
    ASSERT_TRUE(animation) << animation.error().what();
    animation->update(0.375f);
    // Logical updates select a frame using the clock before advancing it.
    animation->update(0);
    expect_frame_path(*animation, "/sheet.png:3");
    source.animation.emplace(std::move(*animation));
    auto payload = source.to_json();
    ASSERT_TRUE(payload) << payload.error().what();
    auto image = source.image;
    auto directives = source.directives;

    auto assigned = source;
    assigned.animation->update(0.25f);
    assigned.animation->update(0);
    expect_frame_path(*assigned.animation, "/sheet.png:5");
    Particle moved(std::move(source));
    EXPECT_FALSE(source.animation);
    ASSERT_TRUE(moved.animation);
    expect_frame_path(*moved.animation, "/sheet.png:3");

    assigned = std::move(moved);
    EXPECT_FALSE(moved.animation);
    auto& alias = assigned;
    assigned = std::move(alias);
    ASSERT_TRUE(assigned.animation);
    EXPECT_FLOAT_EQ(assigned.animation->playback_state().timer, 0.375f);
    expect_frame_path(*assigned.animation, "/sheet.png:3");
    EXPECT_EQ(assigned.to_json(), *payload);
    EXPECT_EQ(assigned.image, image);
    EXPECT_EQ(assigned.directives, directives);
    EXPECT_FLOAT_EQ(assigned.base_size, 2.0f);
    EXPECT_TRUE(assigned.trail && assigned.destruction_set);
  });

const GameTestRegistration particle_tick_retries(
  "ParticlePresentation", "LazyAnimationFailureKeepsBatchAndHistoryUntilRepaired", [](GameTest& fixture) {
    auto directory = File::temporary_directory(fixture.root().random());
    ASSERT_TRUE(directory) << directory.error().what();
    auto cleanup = make_scope_exit([&] { EXPECT_TRUE(File::remove_all(*directory)); });
    auto overlay = File::relative_to(*directory, "assets");
    ASSERT_TRUE(File::create_directory(overlay));
    auto write_asset = [&](String const& name, String const& text) {
      return File::write_file(text, File::relative_to(overlay, name));
    };
    ASSERT_TRUE(write_asset("preload.config", "[]"));
    String valid_config = R"({"mode":"LoopForever","frameNumber":4,"variants":2,"frames":"/tick-sheet.png","centered":false})";
    ASSERT_TRUE(write_asset("ready.animation", valid_config));
    ASSERT_TRUE(write_asset("repair.animation", R"({"frameNumber":0})"));
    auto settings = fixture.rootSettings();
    settings.assetDirectories.clear();
    settings.assetSources = {overlay};
    settings.includeUGC = false;
    settings.mix_random_entropy = false;
    settings.storageDirectory = File::relative_to(*directory, "storage");
    settings.runtimeConfigFile = {};
    settings.logDirectory = {};
    settings.logFile = {};
    auto isolated = Root::create(std::move(settings));
    ASSERT_TRUE(isolated) << isolated.error().what();
    auto& root = **isolated;
    ASSERT_TRUE(root.assets());

    WorldGeometry geometry(100, 100);
    auto tiles = std::make_shared<ClientTileSectorArray>(geometry.size());
    ParticleManager manager(root, geometry, tiles);
    RectF region(0, 0, 100, 100);
    auto source = moving_particle();
    source.type = Particle::Type::Text;
    source.position = {10, 20};
    source.string = "trail source";
    source.trail = true;
    manager.add(std::move(source));
    ASSERT_TRUE(manager.update(0.25f, region, 0));
    ASSERT_EQ(manager.count(), 2u);

    auto lazy = moving_particle();
    lazy.type = Particle::Type::Animated;
    lazy.position = {20, 20};
    lazy.string = "/ready.animation";
    manager.add(lazy);
    lazy.position = {30, 20};
    lazy.string = "/repair.animation";
    auto standalone = lazy;
    ASSERT_FALSE(standalone.update(root, 0.5f));
    EXPECT_EQ(standalone.position, lazy.position);
    EXPECT_EQ(standalone.light, lazy.light);
    EXPECT_FLOAT_EQ(standalone.rotation, lazy.rotation);
    EXPECT_FLOAT_EQ(standalone.time_to_live, lazy.time_to_live);

    manager.add(std::move(lazy));
    List<Particle> display;
    manager.sample_particles(0.5f, display);
    ASSERT_EQ(display.size(), 4u);
    ASSERT_EQ(display[1].position, Vec2F(10.25f, 20));
    ASSERT_FALSE(display[2].animation);
    ASSERT_FALSE(display[3].animation);

    constexpr std::uint64_t seed = 73;
    root.random().seed(seed);
    auto failed = manager.update(0.5f, region, 0);
    ASSERT_FALSE(failed);
    EXPECT_TRUE(std::string_view(failed.error().what()).contains("frameNumber")) << failed.error().what();
    EXPECT_EQ(root.random().next_u64(), RandomSource(seed).next_u64());
    ASSERT_EQ(manager.count(), 4u);
    auto const& unchanged = manager.particles();
    EXPECT_EQ(unchanged[0].position, Vec2F(10.5f, 20));
    EXPECT_FLOAT_EQ(unchanged[0].time_to_live, 0);
    EXPECT_EQ(unchanged[1].position, Vec2F(10.5f, 20));
    EXPECT_FLOAT_EQ(unchanged[1].time_to_live, 9.75f);
    EXPECT_EQ(unchanged[1].string, String("trail source"));
    EXPECT_TRUE(unchanged[1].trail);
    EXPECT_EQ(unchanged[2].position, Vec2F(20, 20));
    EXPECT_EQ(unchanged[3].position, Vec2F(30, 20));
    for (auto index : {2u, 3u}) {
      EXPECT_FLOAT_EQ(unchanged[index].time_to_live, 10);
      EXPECT_FALSE(unchanged[index].animation);
    }
    manager.sample_particles(0.5f, display);
    ASSERT_EQ(display.size(), 4u);
    EXPECT_EQ(display[1].position, Vec2F(10.25f, 20));
    EXPECT_EQ(display[1].string, String("trail source"));

    ASSERT_TRUE(write_asset("repair.animation", valid_config));
    ASSERT_TRUE(root.reload());
    ASSERT_TRUE(root.assets());
    root.random().seed(seed);
    RandomSource oracle(seed);
    auto retried = manager.update(0.5f, region, 0);
    ASSERT_TRUE(retried) << retried.error().what();
    ASSERT_EQ(manager.count(), 4u);
    auto const& committed = manager.particles();
    EXPECT_FALSE(committed[0].trail);
    EXPECT_EQ(committed[0].string, String("trail source"));
    EXPECT_EQ(committed[0].position, Vec2F(11.5f, 20));
    EXPECT_FLOAT_EQ(committed[0].time_to_live, 0);
    EXPECT_TRUE(committed[1].trail);
    EXPECT_EQ(committed[1].position, committed[0].position);
    EXPECT_FLOAT_EQ(committed[1].time_to_live, 9.25f);
    EXPECT_EQ(committed[2].position, Vec2F(21, 20));
    EXPECT_EQ(committed[3].position, Vec2F(31, 20));
    manager.sample_particles(0.5f, display);
    ASSERT_EQ(display.size(), 4u);
    EXPECT_EQ(display[0].position, committed[0].position);
    EXPECT_EQ(display[1].position, Vec2F(11, 20));
    for (auto index : {2u, 3u}) {
      EXPECT_FLOAT_EQ(committed[index].time_to_live, 9.5f);
      ASSERT_TRUE(committed[index].animation);
      EXPECT_FLOAT_EQ(committed[index].animation->playback_state().timer, 0.5f);
      ASSERT_TRUE(display[index].animation);
      auto variant = oracle.uniform_uint(1);
      expect_frame_path(*display[index].animation, "/tick-sheet.png:" + String(std::to_string(variant * 4)));
    }
    EXPECT_EQ(root.random().next_u64(), oracle.next_u64());
  });

const GameTestRegistration particle_tick_skips_obsolete_assets(
  "ParticlePresentation", "CullingAndDestructionDoNotLoadUnusedAnimations", [](GameTest& fixture) {
    WorldGeometry geometry(100, 100);
    auto tiles = std::make_shared<ClientTileSectorArray>(geometry.size());
    tiles->load_default_sector(tiles->sector_for({41, 20}));
    auto solid = tiles->modify_tile({41, 20});
    auto water = tiles->modify_tile({43, 20});
    ASSERT_TRUE(solid && water);
    solid->get().collision = CollisionKind::Block;
    solid->get().liquid.level = 1;
    water->get().collision = CollisionKind::None;
    water->get().liquid.level = 1;

    ParticleManager manager(fixture.root(), geometry, tiles);
    auto lazy = moving_particle();
    lazy.type = Particle::Type::Animated;
    lazy.string = "/particle-manager-unused-test.animation";
    lazy.position = {80, 80};
    manager.add(lazy);

    lazy.position = {10.25f, 20.25f};
    lazy.time_to_live = 0.25f;
    lazy.destruction_action = Particle::DestructionAction::Image;
    lazy.destruction_image = AssetPath("/natural-destruction.png");
    lazy.destruction_time = 1;
    manager.add(lazy);

    lazy.position = {40.25f, 20.25f};
    lazy.time_to_live = 10;
    lazy.destruction_image = AssetPath("/collision-destruction.png");
    lazy.collides_foreground = lazy.collides_liquid = true;
    manager.add(lazy);

    lazy.position = {42.25f, 20.25f};
    manager.add(std::move(lazy));
    auto free = moving_particle();
    free.position = {40.25f, 20.25f};
    manager.add(std::move(free));
    auto updated = manager.update(0.5f, RectF(0, 0, 60, 60), 0);
    ASSERT_TRUE(updated) << updated.error().what();
    ASSERT_EQ(manager.count(), 3u);

    auto const& particles = manager.particles();
    ASSERT_EQ(particles[0].type, Particle::Type::Textured);
    EXPECT_EQ(particles[0].image, AssetPath("/natural-destruction.png"));
    EXPECT_FLOAT_EQ(particles[0].time_to_live, -0.25f);
    EXPECT_FALSE(particles[0].animation);
    ASSERT_EQ(particles[1].type, Particle::Type::Textured);
    EXPECT_EQ(particles[1].image, AssetPath("/collision-destruction.png"));
    EXPECT_FLOAT_EQ(particles[1].time_to_live, 0);
    EXPECT_EQ(particles[1].velocity, Vec2F());
    EXPECT_FALSE(particles[1].animation);
    EXPECT_EQ(particles[2].type, Particle::Type::Ember);
    EXPECT_EQ(particles[2].position, Vec2F(41.25f, 20.25f));
    EXPECT_EQ(particles[2].velocity, Vec2F(2, 0));
  });

const GameTestRegistration animated_particle_sampling(
  "ParticlePresentation", "AnimationClockSamplesAreIndependentAndCrossLoopBoundaries", [](GameTest& fixture) {
    WorldGeometry geometry(100, 100);
    auto tiles = std::make_shared<ClientTileSectorArray>(geometry.size());
    ParticleManager manager(fixture.root(), geometry, tiles);
    auto particle = moving_particle();
    particle.type = Particle::Type::Animated;
    auto animation = Animation::create(fixture.root(), JsonObject{
        {"mode", "LoopForever"}, {"animationCycle", 1.0}, {"frameNumber", 8}, {"frames", "/sheet.png"}, {"centered", false}});
    ASSERT_TRUE(animation);
    particle.animation = std::move(*animation);
    manager.add(std::move(particle));
    ASSERT_TRUE(manager.update(0.75f, RectF(0, 0, 100, 100), 0));
    ASSERT_TRUE(manager.update(0.5f, RectF(0, 0, 100, 100), 0));
    List<Particle> samples;
    for (float alpha : {0.0f, 0.25f, 0.75f, 1.0f, 0.0f}) {
      manager.sample_particles(alpha, samples);
      ASSERT_EQ(samples.size(), 1u);
      ASSERT_TRUE(samples[0].animation);
      float phase = std::fmod(0.75f + alpha * 0.5f, 1.0f);
      EXPECT_FLOAT_EQ(samples[0].animation->playback_state().timer, phase);
      expect_frame_path(*samples[0].animation, "/sheet.png:" + String(std::to_string(static_cast<int>(phase * 8))));
      EXPECT_FLOAT_EQ(manager.particles()[0].animation->playback_state().timer, 0.25f);
      EXPECT_FALSE(manager.particles()[0].animation->is_complete());
      expect_frame_path(*manager.particles()[0].animation, "/sheet.png:6");
    }
    manager.reset_presentation();
    manager.sample_particles(0, samples);
    EXPECT_FLOAT_EQ(samples[0].animation->playback_state().timer, 0.25f);

    for (std::string_view mode : {"Stop", "EndAndDisappear"}) {
      auto finite = Animation::create(fixture.root(), JsonObject{
          {"mode", String(mode)}, {"animationCycle", 1.0}, {"frameNumber", 8}, {"frames", "/sheet.png"}, {"centered", false}});
      ASSERT_TRUE(finite);
      auto previous = finite->playback_state();
      Animation display = *finite;
      display.sample_playback(previous, 1.25f);
      EXPECT_TRUE(display.is_complete());
      expect_frame_path(display, mode == "Stop" ? String("/sheet.png:7") : String());
      display.sample_playback(previous, 0.25f);
      EXPECT_FALSE(display.is_complete());
      EXPECT_FLOAT_EQ(display.playback_state().timer, 0.25f);
      expect_frame_path(display, "/sheet.png:2");
      EXPECT_FALSE(finite->is_complete());
      EXPECT_FLOAT_EQ(finite->playback_state().timer, 0);
      finite->update(1.25f);
      EXPECT_TRUE(finite->is_complete());
      expect_frame_path(*finite, mode == "Stop" ? String("/sheet.png:0") : String());
    }
  });

const GameTestRegistration animation_tag_paths(
  "SpriteAnimation", "TagChangesAndSameFrameIndexOwnership", [](GameTest& fixture) {
    auto& root = fixture.root();
    auto tagged = Animation::create(root, JsonObject{
        {"frames", "<image>:<index>.<unknown>"}, {"centered", false}, {"frameNumber", 4}}, "/ai/");
    ASSERT_TRUE(tagged) << tagged.error().what();
    auto start = tagged->playback_state();
    expect_frame_path(*tagged, "/ai/<image>:0.<unknown>");
    tagged->set_tag("image", "human.png");
    tagged->set_tag("index", "idle");
    expect_frame_path(*tagged, "/ai/human.png:idle.<unknown>");
    tagged->sample_playback(start, 0.125f);
    expect_frame_path(*tagged, "/ai/human.png:idle.<unknown>");
    tagged->update(0);
    expect_frame_path(*tagged, "/ai/human.png:0.<unknown>");
    tagged->set_tag("image", "avian<index>.png");
    expect_frame_path(*tagged, "/ai/avian<index>.png:0.<unknown>");
    tagged->clear_tags();
    expect_frame_path(*tagged, "/ai/<image>:<index>.<unknown>");
    tagged->sample_playback(start, 0);
    expect_frame_path(*tagged, "/ai/<image>:<index>.<unknown>");
    tagged->update(0);
    expect_frame_path(*tagged, "/ai/<image>:0.<unknown>");
    tagged->set_tag("index", "overridden");
    tagged->sample_playback(start, 0.25f);
    expect_frame_path(*tagged, "/ai/<image>:1.<unknown>");
    tagged->set_tag("index", "overridden");
    tagged->reset();
    expect_frame_path(*tagged, "/ai/<image>:0.<unknown>");

    // A real vanilla frame: implicit suffixes always follow the playback
    // number, not an override of the template's index tag.
    auto implicit = Animation::create(root, JsonObject{
        {"frames", "<image>"}, {"frameNumber", 2}}, "/animations/chatting/");
    ASSERT_TRUE(implicit) << implicit.error().what();
    implicit->set_tag("image", "menu.png");
    implicit->set_tag("index", "manual");
    implicit->sample_playback(implicit->playback_state(), 0.5f);
    implicit->set_tag("index", "manual");
    expect_frame_path(*implicit, "/animations/chatting/menu.png:1");
    auto empty = Animation::create(root, JsonObject{}, "/ai/");
    ASSERT_TRUE(empty);
    empty->update(0.5f);
    expect_frame_path(*empty, "");
  });

const GameTestRegistration animation_config_domains(
  "SpriteAnimation", "InvalidFieldsFailBeforeVariantDraw", [](GameTest& fixture) {
    auto& root = fixture.root();
    ASSERT_TRUE(root.assets());
    root.random().seed(2);
    RandomSource oracle(2);
    auto invalid = [&](JsonObject config, std::string_view field) {
      auto animation = Animation::create(root, std::move(config));
      ASSERT_FALSE(animation) << field;
      EXPECT_TRUE(std::string_view(animation.error().what()).contains(field)) << animation.error().what();
    };
    // A fast-math caller need not materialize a constant NaN/Inf argument.
    // Build the IEEE payload at runtime, before packing it into JSON.
    volatile std::uint64_t nan_bits = 0x7ff8000000000001ull;
    volatile std::uint64_t infinity_bits = 0x7ff0000000000000ull;
    auto nan = std::bit_cast<double>(static_cast<std::uint64_t>(nan_bits));
    auto infinity = std::bit_cast<double>(static_cast<std::uint64_t>(infinity_bits));
    invalid({{"frameNumber", 0}}, "frameNumber");
    invalid({{"frameNumber", -1}}, "frameNumber");
    invalid({{"frameNumber", 2.5}}, "frameNumber");
    invalid({{"frameNumber", std::int64_t{1} << 31}}, "frameNumber");
    invalid({{"animationCycle", 0}}, "animationCycle");
    invalid({{"animationCycle", -1}}, "animationCycle");
    invalid({{"animationCycle", nan}}, "animationCycle");
    invalid({{"animationCycle", infinity}}, "animationCycle");
    invalid({{"animationCycle", std::numeric_limits<float>::denorm_min()}, {"frameNumber", 4}}, "animationCycle");
    invalid({{"variants", std::numeric_limits<std::int64_t>::min()}}, "variants");
    invalid({{"variants", 0}}, "variants");
    invalid({{"variants", 1.5}}, "variants");
    invalid({{"variants", std::numeric_limits<std::int64_t>::max()}, {"frameNumber", 2}}, "variants");
    invalid({{"variants", 3}, {"frameNumber", 1 << 30}}, "variants");
    invalid({{"loops", -1}}, "loops");
    invalid({{"loops", infinity}}, "loops");
    invalid({{"angle", infinity}}, "angle");
    invalid({{"offset", JsonArray{0, nan}}}, "offset");
    invalid({{"offset", JsonArray{std::numeric_limits<double>::max(), 0}}}, "offset");
    invalid({{"animationCycle", std::numeric_limits<float>::max()}, {"loops", 2}}, "loops");
    EXPECT_EQ(root.random().next_u64(), oracle.next_u64());
  });

const GameTestRegistration animation_frame_bounds(
  "SpriteAnimation", "LargestVariantFrameResetAndFractionalLifetime", [](GameTest& fixture) {
    auto& root = fixture.root();
    ASSERT_TRUE(root.assets());
    root.random().seed(2);
    RandomSource oracle(2);
    ASSERT_EQ(oracle.uniform_int(1), 1);
    auto variant = Animation::create(root, JsonObject{{"frames", "/sheet.png"}, {"frameNumber", 1 << 30},
        {"variants", 2}, {"mode", "Stop"}, {"loops", 0.5}, {"centered", false}});
    ASSERT_TRUE(variant) << variant.error().what();
    auto start = variant->playback_state();
    expect_frame_path(*variant, "/sheet.png:0");
    variant->update(0.5f);
    EXPECT_TRUE(variant->is_complete());
    expect_frame_path(*variant, "/sheet.png:1073741824");
    variant->sample_playback(start, 0.5f);
    expect_frame_path(*variant, "/sheet.png:2147483647");
    variant->reset();
    expect_frame_path(*variant, "/sheet.png:0");
    variant->update(0);
    expect_frame_path(*variant, "/sheet.png:1073741824");

    ASSERT_EQ(oracle.uniform_int(0), 0);
    auto zero = Animation::create(root, JsonObject{{"loops", 0}});
    ASSERT_TRUE(zero);
    EXPECT_FALSE(zero->is_complete());
    zero->update(0);
    EXPECT_TRUE(zero->is_complete());
    EXPECT_EQ(root.random().next_u64(), oracle.next_u64());
  });

const GameTestRegistration animation_clock_limits(
  "SpriteAnimation", "RepresentableFramesAndTimersSurviveIntermediateOverflow", [](GameTest& fixture) {
    auto huge_sheet = Animation::create(fixture.root(), JsonObject{
        {"animationCycle", 0x1.4p-118f}, {"frameNumber", std::numeric_limits<int>::max()},
        {"frames", "/sheet.png"}, {"centered", false}});
    ASSERT_TRUE(huge_sheet) << huge_sheet.error().what();
    // Float frame duration rounds down to denorm_min. The raw quotient exceeds
    // INT_MAX even though the sampled time is still inside the first cycle.
    huge_sheet->sample_playback(huge_sheet->playback_state(), 0x1.2p-118f);
    expect_frame_path(*huge_sheet, "/sheet.png:2147483646");

    const float cycle = std::numeric_limits<float>::max();
    const float step = cycle * 0.75f;
    auto clock = Animation::create(fixture.root(), JsonObject{{"animationCycle", cycle}, {"mode", "LoopForever"}});
    ASSERT_TRUE(clock) << clock.error().what();
    clock->update(step);
    clock->update(step);
    EXPECT_FLOAT_EQ(clock->playback_state().timer, static_cast<float>(2.0 * step - cycle));
  });

const GameTestRegistration particle_variance_draw_order(
  "ParticleConfig", "VariancePreservesFieldMappingAliasingAndBothRandomStreams", [](GameTest& fixture) {
    auto& root = fixture.root();
    enum class Mode { Mixed, Self, Zero };
    for (auto mode : {Mode::Mixed, Mode::Self, Mode::Zero}) {
      Particle particle;
      particle.size = 7;
      particle.base_size = 9;
      particle.position = {1, 2};
      particle.velocity = {3, -4};
      particle.final_velocity = {5, 6};
      particle.rotation = 0.25f;
      particle.angular_velocity = -0.75f;
      particle.length = 10;
      particle.time_to_live = 20;
      particle.approach = {11, 12};
      Particle variance;
      variance.size = 0.5f;
      variance.position = {1, 2};
      variance.velocity = {3, 0};
      variance.final_velocity = {0, 0};
      variance.rotation = 0.125f;
      variance.angular_velocity = 0.25f;
      variance.length = 2;
      variance.time_to_live = 3;
      if (mode == Mode::Zero)
        variance = Particle();
      auto original = particle;
      auto const& deviations = mode == Mode::Self ? original : variance;
      RandomSource oracle(73);
      RandomSource presentation_oracle(29);
      std::array<float, 11> draws;
      for (auto& value : draws)
        value = oracle.uniform_float();
      auto expected = original;
      expected.size += deviations.size * (draws[0] * 2 - 1);
      expected.position += Vec2F(deviations.position[0] * (draws[1] * 2 - 1),
          deviations.position[1] * (draws[2] * 2 - 1));
      expected.velocity += Vec2F(deviations.velocity[0] * (draws[3] * 2 - 1),
          deviations.velocity[1] * (draws[4] * 2 - 1));
      expected.final_velocity += Vec2F(deviations.final_velocity[0] * (draws[5] * 2 - 1),
          deviations.final_velocity[1] * (draws[6] * 2 - 1));
      expected.rotation += deviations.rotation * (draws[7] * 2 - 1);
      expected.angular_velocity += deviations.angular_velocity * (draws[8] * 2 - 1);
      expected.length += deviations.length * (draws[9] * 2 - 1);
      expected.time_to_live += deviations.time_to_live * (draws[10] * 2 - 1);
      root.random().seed(73);
      root.presentationRandom().seed(29);
      particle.apply_variance(root, mode == Mode::Self ? particle : variance);
      EXPECT_FLOAT_EQ(particle.size, expected.size);
      for (unsigned axis = 0; axis != 2; ++axis) {
        EXPECT_FLOAT_EQ(particle.position[axis], expected.position[axis]);
        EXPECT_FLOAT_EQ(particle.velocity[axis], expected.velocity[axis]);
        EXPECT_FLOAT_EQ(particle.final_velocity[axis], expected.final_velocity[axis]);
      }
      EXPECT_FLOAT_EQ(particle.rotation, expected.rotation);
      EXPECT_FLOAT_EQ(particle.angular_velocity, expected.angular_velocity);
      EXPECT_FLOAT_EQ(particle.length, expected.length);
      EXPECT_FLOAT_EQ(particle.time_to_live, expected.time_to_live);
      EXPECT_EQ(particle.base_size, original.base_size);
      EXPECT_EQ(particle.approach, original.approach);
      EXPECT_EQ(root.random().next_u64(), oracle.next_u64());
      EXPECT_EQ(root.presentationRandom().next_u64(), presentation_oracle.next_u64());
    }
  });

}

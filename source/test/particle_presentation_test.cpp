#include "gtest/gtest.h"

import std;
import star.animation;
import star.asset_path;
import star.bi_map;
import star.color;
import star.drawable;
import star.game_test;
import star.json;
import star.list;
import star.map;
import star.particle;
import star.particle_manager;
import star.rect;
import star.root;
import star.string;
import star.vector;
import star.world_geometry;
import star.unicode;
import star.world_tiles;
import star.random;

using namespace Star;

namespace {

Particle movingParticle() {
  Particle particle;
  particle.type = Particle::Type::Ember;
  particle.position = Vec2F(99.5f, 20);
  particle.velocity = particle.finalVelocity = Vec2F(2, 0);
  particle.size = particle.baseSize = 2;
  particle.timeToLive = 10;
  particle.angularVelocity = 8;
  particle.light = Color::palette_white;
  particle.fade = 0.2f;
  particle.collidesForeground = particle.collidesLiquid = false;
  return particle;
}

const GameTestRegistration
  particleJsonNames("ParticleConfig", "EnumNamesPreserveDefaultsCaseAndJsonRoundTrip", [](GameTest& fixture) {
    auto defaults = Particle::create(fixture.root(), JsonObject{});
    ASSERT_TRUE(defaults) << defaults.error().what();
    EXPECT_EQ(defaults->type, Particle::Type::Variance);
    EXPECT_EQ(defaults->destructionAction, Particle::DestructionAction::None);
    EXPECT_EQ(defaults->layer, Particle::Layer::Middle);
    auto defaultJson = defaults->toJson();
    ASSERT_TRUE(defaultJson) << defaultJson.error().what();
    EXPECT_EQ(defaultJson->get_string("type"), String("variance"));
    EXPECT_EQ(defaultJson->get_string("destructionAction"), String("none"));
    EXPECT_EQ(defaultJson->get_string("layer"), String("middle"));

    auto configured = Particle::create(fixture.root(),
                                       JsonObject{{"type", "EmBeR"},
                                                  {"destructionAction", "ShRiNk"},
                                                  {"layer", "FrOnT"},
                                                  {"size", 2.0},
                                                  {"timeToLive", 3.0},
                                                  {"destructionTime", 1.0}});
    ASSERT_TRUE(configured) << configured.error().what();
    EXPECT_EQ(configured->type, Particle::Type::Ember);
    EXPECT_EQ(configured->destructionAction, Particle::DestructionAction::Shrink);
    EXPECT_EQ(configured->layer, Particle::Layer::Front);
    EXPECT_FALSE(configured->collidesForeground);
    EXPECT_TRUE(configured->collidesLiquid);
    auto stored = configured->toJson();
    ASSERT_TRUE(stored) << stored.error().what();
    EXPECT_EQ(stored->get_string("type"), String("ember"));
    EXPECT_EQ(stored->get_string("destructionAction"), String("shrink"));
    EXPECT_EQ(stored->get_string("layer"), String("front"));
    auto restored = Particle::create(fixture.root(), *stored);
    ASSERT_TRUE(restored) << restored.error().what();
    auto restoredJson = restored->toJson();
    ASSERT_TRUE(restoredJson) << restoredJson.error().what();
    EXPECT_EQ(*restoredJson, *stored);
  });

const GameTestRegistration
  particleJsonNameErrors("ParticleConfig", "EnumNamesRejectUnknownUnicodeAndInvalidEnumValues", [](GameTest& fixture) {
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
        invalid.destructionAction = static_cast<Particle::DestructionAction>(-1);
      else
        invalid.layer = static_cast<Particle::Layer>(-1);
      auto stored = invalid.toJson();
      ASSERT_FALSE(stored);
      EXPECT_TRUE(stored.error().is<MapException>());
    }
  });

const GameTestRegistration particleSampling(
  "ParticlePresentation", "RepeatedSamplesDoNotAdvanceSimulationAndResetSnaps", [](GameTest& fixture) {
    WorldGeometry geometry(100, 100);
    auto tiles = std::make_shared<ClientTileSectorArray>(geometry.size());
    ParticleManager manager(fixture.root(), geometry, tiles);
    List<Particle> samples;
    manager.add(movingParticle());
    manager.sampleParticles(0, samples);
    ASSERT_EQ(samples.size(), 1u);
    EXPECT_EQ(samples[0].position, Vec2F(99.5f, 20));

    ASSERT_TRUE(manager.update(0.5f, RectF(0, 0, 100, 100), 0));
    ASSERT_EQ(manager.count(), 1u);
    auto logicalPosition = manager.particles()[0].position;
    auto logicalRotation = manager.particles()[0].rotation;
    auto logicalLifetime = manager.particles()[0].timeToLive;
    auto logicalLight = manager.particles()[0].light;
    auto* allocation = samples.data();
    auto capacity = samples.capacity();
    for (float alpha : {0.0f, 0.25f, 1.0f, 0.5f, 0.5f, -1.0f, 2.0f}) {
      manager.sampleParticles(alpha, samples);
      float clamped = std::clamp(alpha, 0.0f, 1.0f);
      EXPECT_FLOAT_EQ(samples[0].position[0], 99.5f + clamped);
      EXPECT_FLOAT_EQ(samples[0].rotation, 4 * clamped);
      EXPECT_EQ(samples[0].light, Color::palette_white.mix(logicalLight, clamped));
      EXPECT_EQ(samples.data(), allocation);
      EXPECT_EQ(samples.capacity(), capacity);
      EXPECT_EQ(manager.particles()[0].position, logicalPosition);
      EXPECT_EQ(manager.particles()[0].rotation, logicalRotation);
      EXPECT_EQ(manager.particles()[0].timeToLive, logicalLifetime);
      EXPECT_EQ(manager.particles()[0].light, logicalLight);
      EXPECT_EQ(manager.count(), 1u);
    }

    // Skip all display frames for the next tick: history must still advance.
    ASSERT_TRUE(manager.update(0.5f, RectF(0, 0, 100, 100), 0));
    manager.sampleParticles(0, samples);
    EXPECT_EQ(samples[0].position, logicalPosition);
    manager.resetPresentation();
    manager.sampleParticles(0, samples);
    EXPECT_EQ(samples[0].position, manager.particles()[0].position);
    EXPECT_EQ(samples[0].rotation, manager.particles()[0].rotation);
  });

const GameTestRegistration wrappedParticleSampling(
  "ParticlePresentation", "WrappedEndpointsAndDestructionUseSemanticState", [](GameTest& fixture) {
    Particle current = movingParticle();
    auto previous = current.presentationState();
    current.position = Vec2F(0.5f, 20);
    current.rotation = 4;
    Particle sample;
    sample.samplePresentation(current, previous, WorldGeometry(100, 100), 0.5f, 0.5f, 1);
    EXPECT_EQ(sample.position, Vec2F(100, 20));
    EXPECT_FLOAT_EQ(sample.rotation, 2);

    for (auto action : {Particle::DestructionAction::Shrink, Particle::DestructionAction::Fade}) {
      current = movingParticle();
      current.timeToLive = 0.25f;
      current.destructionAction = action;
      current.destructionTime = 1;
      previous = current.presentationState();
      ASSERT_TRUE(current.update(fixture.root(), 0.5f));
      sample.samplePresentation(current, previous, WorldGeometry(), 0.25f, 0.5f, 2);
      EXPECT_FLOAT_EQ(sample.size, 2);
      EXPECT_FLOAT_EQ(sample.color.alpha_f(), 1);
      sample.samplePresentation(current, previous, WorldGeometry(), 0.75f, 0.5f, 2);
      if (action == Particle::DestructionAction::Shrink)
        EXPECT_FLOAT_EQ(sample.size, 1.75f);
      else
        EXPECT_FLOAT_EQ(sample.color.alpha_f(), 0.875f);
    }

    // A textured destruction image is a discrete payload change even when
    // the type does not change. Do not interpolate its reset size/rotation.
    current = movingParticle();
    current.type = Particle::Type::Textured;
    current.image = AssetPath("/before.png");
    current.destructionAction = Particle::DestructionAction::Image;
    current.destructionImage = AssetPath("/after.png");
    current.destructionTime = 1;
    current.timeToLive = 0.25f;
    previous = current.presentationState();
    sample.samplePresentation(current, previous, WorldGeometry(), 1, 0, 3);
    EXPECT_EQ(sample.image, AssetPath("/before.png"));
    ASSERT_TRUE(current.update(fixture.root(), 0.5f));
    sample.samplePresentation(current, previous, WorldGeometry(), 0, 0.5f, 3);
    EXPECT_EQ(sample.image, AssetPath("/after.png"));
    EXPECT_FLOAT_EQ(sample.size, 1);
    EXPECT_FLOAT_EQ(sample.rotation, 0);
  });

const GameTestRegistration particleLifetimeSampling(
  "ParticlePresentation", "BirthDeathCompactionAndClearKeepStableIdentities", [](GameTest& fixture) {
    WorldGeometry geometry(100, 100);
    auto tiles = std::make_shared<ClientTileSectorArray>(geometry.size());
    ParticleManager manager(fixture.root(), geometry, tiles);
    List<Particle> samples;
    auto particle = movingParticle();
    particle.type = Particle::Type::Text;
    particle.string = "first";
    particle.timeToLive = 0.1f;
    manager.add(particle);
    particle.string = "second";
    particle.position = Vec2F(10, 20);
    particle.timeToLive = 10;
    manager.add(particle);
    manager.sampleParticles(1, samples);
    ASSERT_EQ(samples.size(), 2u);
    EXPECT_EQ(samples[0].string, String("first"));
    ASSERT_TRUE(manager.update(0.5f, RectF(0, 0, 100, 100), 0));
    for (float alpha : {0.0f, 1.0f, 0.25f}) {
      manager.sampleParticles(alpha, samples);
      ASSERT_EQ(samples.size(), 1u);
      EXPECT_EQ(samples[0].string, String("second"));
    }

    particle.string = "newborn";
    particle.position = Vec2F(30, 20);
    manager.add(particle);
    manager.sampleParticles(0, samples);
    ASSERT_EQ(samples.size(), 2u);
    EXPECT_EQ(samples[1].position, particle.position);
    manager.clear();
    manager.sampleParticles(0, samples);
    EXPECT_TRUE(samples.empty());
    manager.add(particle);
    manager.sampleParticles(0, samples);
    ASSERT_EQ(samples.size(), 1u);
    EXPECT_EQ(samples[0].string, String("newborn"));

    // The same reusable display list can outlive the world/manager that filled
    // it: identities must not alias a new manager's first particle.
    ParticleManager replacement(fixture.root(), geometry, tiles);
    particle.string = "replacement";
    replacement.add(particle);
    replacement.sampleParticles(0, samples);
    EXPECT_EQ(samples[0].string, String("replacement"));
    ASSERT_TRUE(replacement.update(0.5f, RectF(50, 50, 60, 60), 0));
    replacement.sampleParticles(0, samples);
    EXPECT_TRUE(samples.empty());
  });

const GameTestRegistration particleTrailSampling(
  "ParticlePresentation", "RenderingNeverSpawnsTrailsAndBatchCapacityIsRetained", [](GameTest& fixture) {
    WorldGeometry geometry(100, 100);
    auto tiles = std::make_shared<ClientTileSectorArray>(geometry.size());
    ParticleManager manager(fixture.root(), geometry, tiles);
    auto particle = movingParticle();
    particle.trail = true;
    List<Particle> batch;
    batch.reserve(8);
    batch.push_back(particle);
    auto capacity = batch.capacity();
    auto* allocation = batch.data();
    manager.addParticles(std::move(batch));
    EXPECT_TRUE(batch.empty());
    EXPECT_EQ(batch.capacity(), capacity);
    EXPECT_EQ(batch.data(), allocation);
    ASSERT_TRUE(manager.update(0.5f, RectF(0, 0, 100, 100), 0));
    ASSERT_EQ(manager.count(), 2u);
    List<Particle> samples;
    for (float alpha : {0.0f, 0.25f, 0.75f, 1.0f, 0.25f}) {
      manager.sampleParticles(alpha, samples);
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
    EXPECT_FALSE(drawable->isImage());
  } else {
    ASSERT_TRUE(drawable->isImage());
    EXPECT_EQ(AssetPath::join(drawable->imagePart().image), path);
  }
}

const GameTestRegistration animated_particle_sampling(
  "ParticlePresentation", "AnimationClockSamplesAreIndependentAndCrossLoopBoundaries", [](GameTest& fixture) {
    WorldGeometry geometry(100, 100);
    auto tiles = std::make_shared<ClientTileSectorArray>(geometry.size());
    ParticleManager manager(fixture.root(), geometry, tiles);
    auto particle = movingParticle();
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
      manager.sampleParticles(alpha, samples);
      ASSERT_EQ(samples.size(), 1u);
      ASSERT_TRUE(samples[0].animation);
      float phase = std::fmod(0.75f + alpha * 0.5f, 1.0f);
      EXPECT_FLOAT_EQ(samples[0].animation->playback_state().timer, phase);
      expect_frame_path(*samples[0].animation, "/sheet.png:" + String(std::to_string(static_cast<int>(phase * 8))));
      EXPECT_FLOAT_EQ(manager.particles()[0].animation->playback_state().timer, 0.25f);
      EXPECT_FALSE(manager.particles()[0].animation->is_complete());
      expect_frame_path(*manager.particles()[0].animation, "/sheet.png:6");
    }
    manager.resetPresentation();
    manager.sampleParticles(0, samples);
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
    for (bool self_variance : {false, true}) {
      Particle particle;
      particle.size = 7;
      particle.baseSize = 9;
      particle.position = {1, 2};
      particle.velocity = {3, -4};
      particle.finalVelocity = {5, 6};
      particle.rotation = 0.25f;
      particle.angularVelocity = -0.75f;
      particle.length = 10;
      particle.timeToLive = 20;
      particle.approach = {11, 12};
      Particle variance;
      variance.size = 0.5f;
      variance.position = {1, 2};
      variance.velocity = {3, 0};
      variance.finalVelocity = {0, 0};
      variance.rotation = 0.125f;
      variance.angularVelocity = 0.25f;
      variance.length = 2;
      variance.timeToLive = 3;
      auto original = particle;
      auto deviations = self_variance ? original : variance;
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
      expected.finalVelocity += Vec2F(deviations.finalVelocity[0] * (draws[5] * 2 - 1),
          deviations.finalVelocity[1] * (draws[6] * 2 - 1));
      expected.rotation += deviations.rotation * (draws[7] * 2 - 1);
      expected.angularVelocity += deviations.angularVelocity * (draws[8] * 2 - 1);
      expected.length += deviations.length * (draws[9] * 2 - 1);
      expected.timeToLive += deviations.timeToLive * (draws[10] * 2 - 1);
      root.random().seed(73);
      root.presentationRandom().seed(29);
      particle.applyVariance(root, self_variance ? particle : variance);
      EXPECT_FLOAT_EQ(particle.size, expected.size);
      for (unsigned axis = 0; axis != 2; ++axis) {
        EXPECT_FLOAT_EQ(particle.position[axis], expected.position[axis]);
        EXPECT_FLOAT_EQ(particle.velocity[axis], expected.velocity[axis]);
        EXPECT_FLOAT_EQ(particle.finalVelocity[axis], expected.finalVelocity[axis]);
      }
      EXPECT_FLOAT_EQ(particle.rotation, expected.rotation);
      EXPECT_FLOAT_EQ(particle.angularVelocity, expected.angularVelocity);
      EXPECT_FLOAT_EQ(particle.length, expected.length);
      EXPECT_FLOAT_EQ(particle.timeToLive, expected.timeToLive);
      EXPECT_EQ(particle.baseSize, original.baseSize);
      EXPECT_EQ(particle.approach, original.approach);
      EXPECT_EQ(root.random().next_u64(), oracle.next_u64());
      EXPECT_EQ(root.presentationRandom().next_u64(), presentation_oracle.next_u64());
    }
  });

}

#include "gtest/gtest.h"

import std;
import star.animation;
import star.asset_path;
import star.bi_map;
import star.color;
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
  particle.light = Color::White;
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
    EXPECT_EQ(defaultJson->getString("type"), String("variance"));
    EXPECT_EQ(defaultJson->getString("destructionAction"), String("none"));
    EXPECT_EQ(defaultJson->getString("layer"), String("middle"));

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
    EXPECT_EQ(stored->getString("type"), String("ember"));
    EXPECT_EQ(stored->getString("destructionAction"), String("shrink"));
    EXPECT_EQ(stored->getString("layer"), String("front"));
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
      EXPECT_EQ(samples[0].light, Color::White.mix(logicalLight, clamped));
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
      EXPECT_FLOAT_EQ(sample.color.alphaF(), 1);
      sample.samplePresentation(current, previous, WorldGeometry(), 0.75f, 0.5f, 2);
      if (action == Particle::DestructionAction::Shrink)
        EXPECT_FLOAT_EQ(sample.size, 1.75f);
      else
        EXPECT_FLOAT_EQ(sample.color.alphaF(), 0.875f);
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
    batch.append(particle);
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

const GameTestRegistration animatedParticleSampling(
  "ParticlePresentation", "AnimationClockSamplesAreIndependentAndCrossLoopBoundaries", [](GameTest& fixture) {
    WorldGeometry geometry(100, 100);
    auto tiles = std::make_shared<ClientTileSectorArray>(geometry.size());
    ParticleManager manager(fixture.root(), geometry, tiles);
    auto particle = movingParticle();
    particle.type = Particle::Type::Animated;
    auto animation = Animation::create(fixture.root(), JsonObject{
        {"mode", "LoopForever"}, {"animationCycle", 1.0}, {"frameNumber", 8}});
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
      EXPECT_FLOAT_EQ(samples[0].animation->playbackState().timer, std::fmod(0.75f + alpha * 0.5f, 1.0f));
      EXPECT_FLOAT_EQ(manager.particles()[0].animation->playbackState().timer, 0.25f);
      EXPECT_FALSE(manager.particles()[0].animation->isComplete());
    }
    manager.resetPresentation();
    manager.sampleParticles(0, samples);
    EXPECT_FLOAT_EQ(samples[0].animation->playbackState().timer, 0.25f);

    for (std::string_view mode : {"Stop", "EndAndDisappear"}) {
      auto finite = Animation::create(fixture.root(), JsonObject{
          {"mode", String(mode)}, {"animationCycle", 1.0}, {"frameNumber", 8}});
      ASSERT_TRUE(finite);
      auto previous = finite->playbackState();
      Animation display = *finite;
      display.samplePlayback(previous, 1.25f);
      EXPECT_TRUE(display.isComplete());
      display.samplePlayback(previous, 0.25f);
      EXPECT_FALSE(display.isComplete());
      EXPECT_FLOAT_EQ(display.playbackState().timer, 0.25f);
      EXPECT_FALSE(finite->isComplete());
      EXPECT_FLOAT_EQ(finite->playbackState().timer, 0);
    }
  });

}

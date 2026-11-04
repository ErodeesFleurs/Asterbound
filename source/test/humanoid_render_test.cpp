#include "gtest/gtest.h"

import std;
import star.actor_movement_controller;
import star.animated_part_set;
import star.color;
import star.algorithm;
import star.armors;
import star.asset_path;
import star.assets;
import star.casting;
import star.client_context;
import star.drawable;
import star.file;
import star.entity_rendering;
import star.entity_rendering_types;
import star.format;
import star.list;
import star.light_source;
import star.mixer;
import star.movement_controller_lua_bindings;
import star.particle;
import star.status_controller;
import star.status_types;
import star.game_test;
import star.game_types;
import star.humanoid;
import star.item;
import star.item_database;
import star.item_descriptor;
import star.json;
import star.line;
import star.matrix3;
import star.networked_animator;
import star.lua_root;
import star.net_compatibility;
import star.player;
import star.player_factory;
import star.player_lua_bindings;
import star.root;
import star.random;
import star.string;
import star.uuid;
import star.vector;
import star.world_server;

using namespace Star;

const GameTestRegistration
  sleeveLighting("HumanoidRenderTest", "BackSleeveRetainsArmorLightingWhileHolding", [](GameTest& fixture) {
    auto assets = fixture.root().assets();
    ASSERT_TRUE(assets);
    auto config = (*assets)->json("/humanoid.config");
    ASSERT_TRUE(config);
    // Exercise the legacy humanoid path still used by species without an animator.
    auto legacy = config->eraseKey("animation");
    ASSERT_TRUE(legacy);
    auto humanoid = Humanoid::create(fixture.root(), *legacy);
    ASSERT_TRUE(humanoid) << humanoid.error().what();
    HumanoidIdentity identity;
    ASSERT_TRUE((*humanoid)->setIdentity(identity));
    auto database = fixture.root().itemDatabase();
    ASSERT_TRUE(database);
    auto names = (*database)->allItems();
    std::sort(names.begin(), names.end());
    String chestName;
    for (auto const& name : names) {
      auto type = (*database)->itemType(name);
      ASSERT_TRUE(type);
      if (*type != ItemType::ChestArmor)
        continue;
      auto descriptor = ItemDescriptor::create(name, 1);
      ASSERT_TRUE(descriptor);
      auto item = (*database)->item(*descriptor);
      ASSERT_TRUE(item);
      auto chest = asRef<ChestArmor>(**item);
      ASSERT_TRUE(chest);
      if (!chest->get().backSleeveFrameset(identity.gender).empty()) {
        chestName = name;
        break;
      }
    }
    ASSERT_FALSE(chestName.empty());
    for (bool fullbright : {false, true}) {
      auto descriptor = ItemDescriptor::create(chestName, 1, JsonObject{{"fullbright", fullbright}});
      ASSERT_TRUE(descriptor);
      auto item = (*database)->item(*descriptor);
      ASSERT_TRUE(item);
      auto chest = asRef<ChestArmor>(**item);
      ASSERT_TRUE(chest);
      ASSERT_EQ(chest->get().fullbright(), fullbright);
      ASSERT_TRUE((*humanoid)->setWearableFromChest(0, chest->get(), identity.gender));
      auto sleeve = AssetPath::split(chest->get().backSleeveFrameset(identity.gender)).basePath;
      for (auto direction : {Direction::Left, Direction::Right}) {
        (*humanoid)->setFacingDirection(direction);
        for (bool holding : {false, true}) {
          (*humanoid)->setHandParameters(ToolHand::Primary, holding, 0, 0, false, false, false);
          (*humanoid)->setHandParameters(ToolHand::Alt, holding, 0, 0, false, false, false);
          auto drawables = (*humanoid)->render();
          ASSERT_TRUE(drawables) << drawables.error().what();
          unsigned sleeves = 0;
          for (auto const& drawable : *drawables) {
            if (drawable.isImage() && drawable.imagePart().image.basePath == sleeve) {
              ++sleeves;
              EXPECT_EQ(drawable.fullbright, fullbright)
                << "holding=" << holding << ", armor fullbright=" << fullbright;
            }
          }
          EXPECT_EQ(sleeves, 1u);
        }
      }
    }
  });

const GameTestRegistration transientMovementAnimation(
  "HumanoidRenderTest", "PlayerMovementRequestIsTransientRotatedAndNetworked", [](GameTest& fixture) {
    auto directory = File::temporaryDirectory(fixture.root().random());
    ASSERT_TRUE(directory);
    auto cleanup = finally([&] { EXPECT_TRUE(File::removeDirectoryRecursive(*directory)); });
    auto overlay = File::relativeTo(*directory, "assets");
    ASSERT_TRUE(File::makeDirectoryRecursive(overlay));
    ASSERT_TRUE(File::writeFile(String(R"([
      {"op":"replace","path":"/genericScriptContexts","value":{"movementPoseFixture":"/pose.lua"}}
    ])"), File::relativeTo(overlay, "player.config.patch")));
    ASSERT_TRUE(File::writeFile(String(R"LUA(
      function update()
        local state = player.getProperty("poseFixture")
        if state then player.controlMovementState(state) end
        if player.getProperty("poseRevoke") then player.controlMovementState(nil) end
      end
      function postUpdate()
        if player.getProperty("poseLate") then player.controlMovementState("run") end
      end
    )LUA"), File::relativeTo(overlay, "pose.lua")));
    auto settings = fixture.rootSettings();
    settings.assetSources.append(overlay);
    settings.storageDirectory = File::relativeTo(*directory, "storage");
    settings.runtimeConfigFile = {};
    settings.logDirectory = {};
    settings.logFile = {};
    settings.includeUGC = false;
    auto root = Root::create(std::move(settings));
    ASSERT_TRUE(root) << root.error().what();
    auto storage = File::ephemeralFile((*root)->random());
    ASSERT_TRUE(storage);
    auto world = WorldServer::create(**root, Vec2U(2048, 2048), *storage);
    ASSERT_TRUE(world) << world.error().what();
    auto close = finally([&] { EXPECT_TRUE((*world)->close()); });
    (*world)->setSpawningEnabled(false);
    auto factory = (*root)->playerFactory();
    ASSERT_TRUE(factory);
    auto made = (*factory)->create();
    ASSERT_TRUE(made) << made.error().what();
    auto& player = **made;
    ASSERT_TRUE(player.finalizeCreation());
    auto playerUuid = player.uuid();
    ASSERT_TRUE(playerUuid);
    auto client = ClientContext::create(**root, Uuid((*root)->random()), *playerUuid);
    ASSERT_TRUE(client) << client.error().what();
    player.setClientContext(*client);
    ASSERT_TRUE(player.init(**world, 100, EntityMode::Master));
    auto uninit = finally([&] { EXPECT_TRUE(player.uninit()); });
    ASSERT_TRUE(player.refreshHumanoid());
    auto& movement = player.movementController();
    ActorMovementParameters parameters;
    parameters.gravityEnabled = false;
    parameters.collisionEnabled = false;
    parameters.frictionEnabled = false;
    ASSERT_TRUE(movement.updateBaseParameters(parameters));
    movement.setPosition(Vec2F(1024, 1024));

    // The real Lua callback rejects other humanoid states, not just unknown
    // names, and accepts nil. No fixture implementation stands in for it.
    auto lua = LuaRoot::create(**root);
    ASSERT_TRUE(lua);
    auto context = (*lua)->createContext(StringList{});
    ASSERT_TRUE(context);
    auto callbacks = LuaBindings::makePlayerCallbacks(**root, player);
    ASSERT_TRUE(callbacks);
    ASSERT_TRUE(context->setCallbacks("player", *callbacks));
    auto valid = context->eval<bool>(R"LUA(
      for _, state in ipairs({"idle", "walk", "run", "jump", "fall"}) do
        assert(pcall(player.controlMovementState, state))
      end
      for _, state in ipairs({"swim", "duck", "sit", "lay", "RUN", "invalid"}) do
        assert(not pcall(player.controlMovementState, state))
      end
      assert(pcall(player.controlMovementState, nil))
      return true
    )LUA");
    ASSERT_TRUE(valid) << valid.error().what();
    EXPECT_TRUE(*valid);

    std::uint64_t step = 0;
    constexpr float dt = 1.f / 60;
    auto tick = [&] { return player.update(dt, step++); };
    std::array rules{NetCompatibilityRules(LegacyVersion), NetCompatibilityRules(OpenProtocolVersion)};
    std::array<std::shared_ptr<Player>, 2> remotes;
    std::array<std::uint64_t, 2> versions{};
    auto uninitRemotes = finally([&] {
      for (auto const& remote : remotes)
        if (remote)
          EXPECT_TRUE(remote->uninit());
    });
    for (std::size_t i = 0; i != remotes.size(); ++i) {
      auto store = player.netStore(rules[i]);
      ASSERT_TRUE(store);
      auto remote = (*factory)->netLoadPlayer(*store, rules[i]);
      ASSERT_TRUE(remote) << remote.error().what();
      remotes[i] = *remote;
      ASSERT_TRUE(remotes[i]->init(**world, 101 + i, EntityMode::Slave));
    }
    // This tests the actual legacy compatibility filter, not a custom animation
    // parameter or a new tech state. It is not a vanilla-executable smoke test.
    auto sync = [&](Humanoid::State state, Direction networkDirection) {
      for (std::size_t i = 0; i != remotes.size(); ++i) {
        SCOPED_TRACE(rules[i].version());
        auto packet = player.writeNetState(versions[i], rules[i]);
        ASSERT_TRUE(packet);
        versions[i] = packet->second;
        ASSERT_TRUE(remotes[i]->readNetState(packet->first, 0.f, rules[i]));
        ASSERT_TRUE(remotes[i]->update(dt, step));
        EXPECT_EQ(remotes[i]->humanoid()->state(), state);
        EXPECT_EQ(remotes[i]->humanoid()->facingDirection(), player.humanoid()->facingDirection());
        EXPECT_EQ(remotes[i]->humanoid()->movingBackwards(), player.humanoid()->movingBackwards());
        EXPECT_EQ(remotes[i]->movementController().movingDirection(), networkDirection);
        EXPECT_NEAR(remotes[i]->movementController().rotation(), movement.rotation(), .011f);
        EXPECT_FALSE(remotes[i]->movementController().onGround());
        auto rendered = remotes[i]->humanoid()->render();
        ASSERT_TRUE(rendered) << rendered.error().what();
        EXPECT_FALSE(rendered->empty());
      }
    };
    std::array states{Humanoid::Idle, Humanoid::Walk, Humanoid::Run, Humanoid::Jump, Humanoid::Fall};
    std::array<std::string_view, 5> names{"idle", "walk", "run", "jump", "fall"};
    auto nativeDirection = movement.movingDirection();
    for (float rotation : {0.f, std::numbers::pi_v<float> / 2, std::numbers::pi_v<float>, -std::numbers::pi_v<float> / 2}) {
      movement.setRotation(rotation);
      auto tangent = Vec2F::withAngle(rotation);
      for (std::size_t i = 0; i != states.size(); ++i) {
        player.setGenericProperty("poseFixture", Json(String(names[i])));
        for (auto facing : {Direction::Left, Direction::Right}) {
          for (auto direction : {Direction::Left, Direction::Right}) {
            movement.controlFace(facing);
            movement.setVelocity(tangent * (direction == Direction::Left ? -14.f : 14.f));
            ASSERT_TRUE(tick());
            EXPECT_EQ(player.humanoid()->state(), states[i]);
            EXPECT_EQ(player.humanoid()->facingDirection(), facing);
            EXPECT_EQ(player.humanoid()->movingBackwards(), facing != direction);
            EXPECT_EQ(movement.movingDirection(), nativeDirection);
            EXPECT_FALSE(movement.onGround());
            EXPECT_TRUE(movement.zeroG());
            sync(states[i], direction);
          }
        }
      }
    }

    // Native direction must keep updating underneath an active wire override.
    // In particular ceiling rightward motion has negative world-X velocity.
    movement.setRotation(std::numbers::pi_v<float>);
    player.setGenericProperty("poseFixture", "run");
    for (auto direction : {Direction::Right, Direction::Left}) {
      movement.controlMove(direction);
      movement.controlFace(Direction::Right);
      movement.setVelocity(Vec2F(-14.f, 0.f));
      ASSERT_TRUE(tick());
      EXPECT_EQ(movement.movingDirection(), direction);
      sync(Humanoid::Run, Direction::Right);
    }
    nativeDirection = movement.movingDirection();
    movement.setVelocity(Vec2F());
    ASSERT_TRUE(tick());
    EXPECT_FALSE(player.humanoid()->movingBackwards());
    sync(Humanoid::Run, player.humanoid()->facingDirection());

    player.setGenericProperty("poseFixture", Json());
    ASSERT_TRUE(tick());
    EXPECT_EQ(player.humanoid()->state(), Humanoid::SwimIdle);
    EXPECT_EQ(movement.movingDirection(), nativeDirection);
    sync(Humanoid::SwimIdle, nativeDirection);
    player.setGenericProperty("poseFixture", "walk");
    player.setGenericProperty("poseRevoke", true);
    ASSERT_TRUE(tick());
    EXPECT_EQ(player.humanoid()->state(), Humanoid::SwimIdle);
    sync(Humanoid::SwimIdle, nativeDirection);
    player.setGenericProperty("poseFixture", Json());
    player.setGenericProperty("poseRevoke", false);
    player.setGenericProperty("poseLate", true);
    ASSERT_TRUE(tick());
    ASSERT_TRUE(tick());
    EXPECT_EQ(player.humanoid()->state(), Humanoid::SwimIdle);
    sync(Humanoid::SwimIdle, nativeDirection);
    player.setGenericProperty("poseLate", false);
    player.setGenericProperty("poseFixture", "run");
    ASSERT_TRUE(tick());
    player.setGenericProperty("poseFixture", Json());
    ASSERT_TRUE(player.uninit());
    ASSERT_TRUE(player.init(**world, 100, EntityMode::Master));
    ASSERT_TRUE(player.refreshHumanoid());
    ASSERT_TRUE(movement.updateBaseParameters(parameters));
    ASSERT_TRUE(tick());
    EXPECT_EQ(player.humanoid()->state(), Humanoid::SwimIdle);
    sync(Humanoid::SwimIdle, movement.movingDirection());
    player.setGenericProperty("poseFixture", "run");
    movement.setVelocity(Vec2F());
    movement.controlFace(Direction::Right);
    ASSERT_TRUE(tick());
    sync(Humanoid::Run, Direction::Right);
    ASSERT_TRUE(player.teleportOut());
    // A higher-priority state can start between update and packet generation.
    for (std::size_t i = 0; i != remotes.size(); ++i) {
      auto packet = player.writeNetState(versions[i], rules[i]);
      ASSERT_TRUE(packet);
      versions[i] = packet->second;
      ASSERT_TRUE(remotes[i]->readNetState(packet->first, 0.f, rules[i]));
      EXPECT_TRUE(remotes[i]->isTeleporting());
      EXPECT_EQ(remotes[i]->movementController().movingDirection(), movement.movingDirection());
    }
    ASSERT_TRUE(tick());
    EXPECT_TRUE(player.isTeleporting());
    // Teleport freezes the previous humanoid cycle, but must send its native
    // teleport state and restore native movingDirection, not the request.
    sync(player.humanoid()->state(), movement.movingDirection());
    for (auto const& remote : remotes)
      EXPECT_TRUE(remote->isTeleporting());
    ASSERT_TRUE(player.teleportAbort());
    // Abort starts TeleportIn; it does not end teleportation immediately.
    // Finish its real configured timer before testing the independent death
    // transition, otherwise the native teleport freeze retains the old cycle.
    auto assets = (*root)->assets();
    ASSERT_TRUE(assets);
    auto abortTime = (*assets)->json("/player.config:teleportInTime");
    ASSERT_TRUE(abortTime);
    auto abortSeconds = abortTime->toFloat();
    ASSERT_TRUE(abortSeconds);
    auto abortTicks = static_cast<unsigned>(std::ceil(*abortSeconds / dt)) + 1;
    for (unsigned i = 0; i < abortTicks && player.isTeleporting(); ++i)
      ASSERT_TRUE(tick());
    ASSERT_FALSE(player.isTeleporting());
    ASSERT_EQ(player.humanoid()->state(), Humanoid::Run);
    sync(Humanoid::Run, Direction::Right);
    ASSERT_TRUE(player.kill());
    ASSERT_TRUE(player.isDead());
    ASSERT_TRUE(tick());
    ASSERT_TRUE(player.isDead());
    EXPECT_NE(player.humanoid()->state(), Humanoid::Run);
    sync(player.humanoid()->state(), movement.movingDirection());
  });

const GameTestRegistration refreshedMovementCycles(
  "HumanoidRenderTest", "RefreshedRotatedWalkRunAdvanceVisibleFrames", [](GameTest& fixture) {
    auto assets = fixture.root().assets();
    ASSERT_TRUE(assets);
    auto config = (*assets)->json("/humanoid.config");
    ASSERT_TRUE(config);
    auto legacy = config->eraseKey("animation");
    ASSERT_TRUE(legacy);
    for (auto const& mode : {*legacy, *config}) {
      auto made = Humanoid::create(fixture.root(), mode);
      ASSERT_TRUE(made) << made.error().what();
      auto humanoid = *made;
      ASSERT_TRUE(humanoid->setIdentity(HumanoidIdentity{}));
      for (auto state : {Humanoid::Walk, Humanoid::Run}) {
        for (float rotation : {std::numbers::pi_v<float> / 2, std::numbers::pi_v<float>}) {
          humanoid->setRotation(rotation);
          for (bool backwards : {false, true}) {
            ASSERT_TRUE(humanoid->setMovingBackwards(backwards));
            std::set<String> frames;
            for (int frame = 0; frame != 60; ++frame) {
              ASSERT_TRUE(humanoid->setState(state));
              ASSERT_TRUE(humanoid->animate(1.f / 60, {}));
              auto drawables = humanoid->render();
              ASSERT_TRUE(drawables) << drawables.error().what();
              String renderedFrames;
              for (auto const& drawable : *drawables) {
                if (drawable.isImage() && drawable.imagePart().image.subPath)
                  renderedFrames += AssetPath::join(drawable.imagePart().image);
              }
              frames.insert(std::move(renderedFrames));
            }
            EXPECT_GT(frames.size(), 1u) << "state=" << state << ", backwards=" << backwards;
          }
        }
      }
    }
  });

const GameTestRegistration semanticPartSampling(
  "HumanoidRenderTest", "PartSamplingWrapsAndResetsWithoutChangingLogicalFrames", [](GameTest&) {
    auto config = Json::parse(R"({
      "stateTypes":{"movement":{"default":"walk","states":{
        "walk":{"frames":2,"cycle":1,"mode":"loop"},
        "stop":{"frames":1,"cycle":1,"mode":"end"},
        "change":{"frames":2,"cycle":0.1,"mode":"transition","transition":"stop"}
      }}},
      "parts":{"body":{"partStates":{"movement":{"walk":{
        "properties":{"interpolated":true},
        "frameProperties":{"transforms":[[["reset"],["translate",[0,0]]],[["reset"],["translate",[8,0]]]]}
      }}}}}
    })");
    ASSERT_TRUE(config);
    auto parts = AnimatedPartSet::create(*config, 1);
    ASSERT_TRUE(parts);
    ASSERT_TRUE(parts->update(.4f));
    auto logical = parts->activeState("movement");
    ASSERT_TRUE(logical);
    EXPECT_FLOAT_EQ(logical->get().timer, .4f);
    AnimatedPartSet::Sample sample;
    parts->prepareSample(sample, .5f);
    auto sampled = parts->activeState("movement", &sample);
    ASSERT_TRUE(sampled);
    EXPECT_FLOAT_EQ(sampled->get().timer, .2f);
    auto body = parts->activePart("body", &sample);
    ASSERT_TRUE(body);
    EXPECT_NEAR(body->get().animationAffineTransform().transformVec2(Vec2F{})[0], 3.2f, 1e-5f);
    logical = parts->activeState("movement");
    ASSERT_TRUE(logical);
    EXPECT_FLOAT_EQ(logical->get().timer, .4f);
    ASSERT_TRUE(parts->update(.5f));
    ASSERT_TRUE(parts->update(.2f));
    parts->prepareSample(sample, .25f);
    sampled = parts->activeState("movement", &sample);
    ASSERT_TRUE(sampled);
    EXPECT_NEAR(sampled->get().timer, .95f, 1e-5f);
    parts->prepareSample(sample, .75f);
    sampled = parts->activeState("movement", &sample);
    ASSERT_TRUE(sampled);
    EXPECT_NEAR(sampled->get().timer, .05f, 1e-5f);
    ASSERT_TRUE(parts->restartState("movement"));
    parts->prepareSample(sample, 0);
    sampled = parts->activeState("movement", &sample);
    ASSERT_TRUE(sampled);
    EXPECT_FLOAT_EQ(sampled->get().timer, 0);
    ASSERT_TRUE(parts->setActiveState("movement", "change"));
    ASSERT_TRUE(parts->update(.2f));
    parts->prepareSample(sample, .5f);
    sampled = parts->activeState("movement", &sample);
    ASSERT_TRUE(sampled);
    EXPECT_EQ(sampled->get().stateName, "stop");
    EXPECT_FLOAT_EQ(sampled->get().timer, 0);
    ASSERT_TRUE(parts->setActiveState("movement", "walk", true, true));
    ASSERT_TRUE(parts->update(.4f));
    parts->prepareSample(sample, .5f);
    sampled = parts->activeState("movement", &sample);
    ASSERT_TRUE(sampled);
    EXPECT_EQ(sampled->get().frame, 1u);
    EXPECT_EQ(sampled->get().nextFrame, 0u);
    EXPECT_NEAR(sampled->get().frameProgress, .4f, 1e-5f);
  });

const GameTestRegistration animatorSemanticSampling(
  "HumanoidRenderTest", "AnimatorSampleDrivesGeometryLightsAndScopedOverrides", [](GameTest& fixture) {
    auto config = Json::parse(R"({
      "version":1,
      "transformationGroups":{"moving":{}},
      "rotationGroups":{"aim":{"angularVelocity":2}},
      "animatedParts":{
        "stateTypes":{"movement":{"default":"walk","states":{
          "walk":{"frames":2,"cycle":1,"mode":"loop"},
          "stop":{"frames":1,"cycle":1}
        }}},
        "parts":{"body":{
          "properties":{"transformationGroups":["moving"],"testPoint":[0,0]},
          "partStates":{"movement":{"walk":{
            "properties":{"interpolated":true},
            "frameProperties":{"transforms":[[["reset"],["translate",[0,0]]],[["reset"],["translate",[4,0]]]]}
          }}}
        }}
      },
      "lights":{"glow":{"active":true,"position":[0,0],"color":[255,255,255],"anchorPart":"body"}}
    })");
    ASSERT_TRUE(config);
    auto made = NetworkedAnimator::create(fixture.root(), *config);
    ASSERT_TRUE(made) << made.error().what();
    auto& animator = **made;
    auto line = Drawable::makeLine(Line2F({0, 0}, {1, 0}), 1, Color::White);
    animator.setPartDrawables("body", {line});
    NetworkedAnimator::DynamicTarget events;
    ASSERT_TRUE(animator.update(.1f, std::ref(events)));
    ASSERT_TRUE(animator.translateTransformationGroup("moving", {2, 0}));
    ASSERT_TRUE(animator.rotateGroup("aim", 1.f));
    ASSERT_TRUE(animator.update(.25f, std::ref(events)));
    auto logicalTimer = animator.stateTimer("movement");
    ASSERT_TRUE(logicalTimer);
    animator.setLocalTag("pose", "logical");
    for (int repeat = 0; repeat != 3; ++repeat) {
      auto sample = animator.sample(.5f);
      auto other = animator.sample(.1f);
      auto otherPoint = other.partPoint("body", "testPoint");
      ASSERT_TRUE(otherPoint);
      ASSERT_TRUE(*otherPoint);
      EXPECT_NEAR((**otherPoint)[0], 1.2f, 1e-5f);
      auto timer = sample.stateTimer("movement");
      ASSERT_TRUE(timer);
      EXPECT_NEAR(*timer, .225f, 1e-5f);
      auto angle = sample.currentRotationAngle("aim");
      ASSERT_TRUE(angle);
      EXPECT_NEAR(*angle, .25f, 1e-5f);
      auto point = sample.partPoint("body", "testPoint");
      ASSERT_TRUE(point);
      ASSERT_TRUE(*point);
      EXPECT_NEAR((**point)[0], 2.8f, 1e-5f);
      auto drawables = sample.drawables();
      ASSERT_TRUE(drawables) << drawables.error().what();
      ASSERT_EQ(drawables->size(), 1u);
      auto expected = line;
      expected.transform(Mat3F::translation({2.8f, 0}));
      EXPECT_NEAR((*drawables)[0].position[0], expected.position[0], 1e-5f);
      auto lights = sample.lightSources();
      ASSERT_TRUE(lights);
      ASSERT_EQ(lights->size(), 1u);
      EXPECT_NEAR((*lights)[0].position[0], 2.8f, 1e-5f);
      ASSERT_TRUE(sample.setLocalState("movement", "stop"));
      EXPECT_FALSE(sample.setLocalState("movement", "missing"));
      sample.setLocalTag("pose", "sampled");
      auto sampledTag = sample.applyPartTags("body", "<pose>");
      auto otherTag = other.applyPartTags("body", "<pose>");
      auto logicalTag = animator.applyPartTags("body", "<pose>");
      ASSERT_TRUE(sampledTag);
      ASSERT_TRUE(otherTag);
      ASSERT_TRUE(logicalTag);
      EXPECT_EQ(*sampledTag, "sampled");
      EXPECT_EQ(*otherTag, "logical");
      EXPECT_EQ(*logicalTag, "logical");
      ASSERT_TRUE(sample.translateLocalTransformationGroup("moving", {10, 0}));
      auto state = sample.state("movement");
      ASSERT_TRUE(state);
      EXPECT_EQ(*state, "stop");
      {
        auto nested = sample;
        auto inherited = nested.state("movement");
        ASSERT_TRUE(inherited);
        EXPECT_EQ(*inherited, "stop");
        EXPECT_FLOAT_EQ(nested.interpolation(), .5f);
      }
      auto untouched = other.partPoint("body", "testPoint");
      ASSERT_TRUE(untouched);
      ASSERT_TRUE(*untouched);
      EXPECT_NEAR((**untouched)[0], 1.2f, 1e-5f);
      auto logicalState = animator.state("movement");
      auto logicalLocal = animator.getLocalTransformationGroup("moving");
      ASSERT_TRUE(logicalState);
      ASSERT_TRUE(logicalLocal);
      EXPECT_EQ(*logicalState, "walk");
      EXPECT_EQ(*logicalLocal, Mat3F::identity());
      sample.setPartDrawables("body", nullptr);
      auto hidden = sample.drawables();
      auto untouchedDrawables = other.drawables();
      ASSERT_TRUE(hidden);
      ASSERT_TRUE(untouchedDrawables);
      EXPECT_TRUE(hidden->empty());
      EXPECT_EQ(untouchedDrawables->size(), 1u);
    }
    auto timer = animator.stateTimer("movement");
    ASSERT_TRUE(timer);
    EXPECT_FLOAT_EQ(*timer, *logicalTimer);
    auto state = animator.state("movement");
    ASSERT_TRUE(state);
    EXPECT_EQ(*state, "walk");
    auto local = animator.getLocalTransformationGroup("moving");
    ASSERT_TRUE(local);
    EXPECT_FLOAT_EQ(local->transformVec2(Vec2F{})[0], 0);
    EXPECT_TRUE(events.pullNewAudios().empty());
    EXPECT_TRUE(events.pullNewParticles().empty());
    ASSERT_TRUE(animator.rotateGroup("aim", -2.f, true));
    auto sample = animator.sample(0);
    auto angle = sample.currentRotationAngle("aim");
    ASSERT_TRUE(angle);
    EXPECT_FLOAT_EQ(*angle, -2.f);
  });

const GameTestRegistration humanoidSemanticSampling(
  "HumanoidRenderTest", "LegacyAndAnimatorHumanoidsSamplePoseWithoutAdvancing", [](GameTest& fixture) {
    auto assets = fixture.root().assets();
    ASSERT_TRUE(assets);
    auto config = (*assets)->json("/humanoid.config");
    ASSERT_TRUE(config);
    auto legacy = config->eraseKey("animation");
    ASSERT_TRUE(legacy);
    for (auto const& mode : {*legacy, *config}) {
      auto made = Humanoid::create(fixture.root(), mode);
      ASSERT_TRUE(made);
      auto& humanoid = **made;
      ASSERT_TRUE(humanoid.setIdentity(HumanoidIdentity{}));
      ASSERT_TRUE(humanoid.animate(1.f / 60, {}));
      humanoid.setRotation(1.f);
      ASSERT_TRUE(humanoid.animate(1.f / 60, {}));
      auto previous = humanoid.render(false, true, 0);
      auto middle = humanoid.render(false, true, .5f);
      auto current = humanoid.render(false, true, 1);
      auto repeated = humanoid.render(false, true, .5f);
      ASSERT_TRUE(previous);
      ASSERT_TRUE(middle);
      ASSERT_TRUE(current);
      ASSERT_TRUE(repeated);
      EXPECT_EQ(*middle, *repeated);
      EXPECT_NE(*previous, *current);
      // Compare with a semantic rotation of the same fixed idle pose, not a
      // drawable-by-index blend. Sprite identity remains authored by the pose.
      Drawable::rotateAll(*previous, .5f);
      Drawable::rebaseAll(*previous);
      ASSERT_EQ(previous->size(), middle->size());
      for (std::size_t i = 0; i != previous->size(); ++i) {
        if ((*previous)[i].isImage() && (*middle)[i].isImage()) {
          EXPECT_EQ((*previous)[i].imagePart().image, (*middle)[i].imagePart().image);
          EXPECT_NEAR((*previous)[i].position[0], (*middle)[i].position[0], 1e-5f);
          EXPECT_NEAR((*previous)[i].position[1], (*middle)[i].position[1], 1e-5f);
        }
      }
      EXPECT_EQ(humanoid.state(), Humanoid::Idle);
      EXPECT_EQ(humanoid.emoteState(), HumanoidEmote::Idle);
    }
  });

const GameTestRegistration playerPresentationEvents(
  "HumanoidRenderTest", "PlayerEventsAreTickOwnedAndInvisibleEventsAreDiscarded", [](GameTest& fixture) {
    auto& root = fixture.root();
    auto storage = File::ephemeralFile(root.random());
    ASSERT_TRUE(storage);
    auto world = WorldServer::create(root, Vec2U(2048, 2048), *storage);
    ASSERT_TRUE(world) << world.error().what();
    auto close = finally([&] { EXPECT_TRUE((*world)->close()); });
    (*world)->setSpawningEnabled(false);
    auto factory = root.playerFactory();
    ASSERT_TRUE(factory);
    auto made = (*factory)->create();
    ASSERT_TRUE(made) << made.error().what();
    auto& player = **made;
    ASSERT_TRUE(player.finalizeCreation());
    auto uuid = player.uuid();
    ASSERT_TRUE(uuid);
    auto client = ClientContext::create(root, Uuid(root.random()), *uuid);
    ASSERT_TRUE(client);
    player.setClientContext(*client);
    ASSERT_TRUE(player.init(**world, 100, EntityMode::Master));
    auto uninit = finally([&] { EXPECT_TRUE(player.uninit()); });
    ASSERT_TRUE(player.refreshHumanoid());
    ASSERT_TRUE(player.moveTo(Vec2F(1024, 1024)));

    struct Events : NullRenderCallback {
      unsigned particles = 0;
      unsigned audios = 0;
      float alpha = 1.0f;
      PresentationContext presentation() const override { return PresentationContext(alpha); }
      std::expected<void, StarException> addParticle(Particle particle) override {
        if (particle.string == "presentation-event-fixture")
          ++particles;
        return {};
      }
      std::expected<void, StarException> addAudio(std::shared_ptr<AudioInstance>) override {
        ++audios;
        return {};
      }
    } events;
    NullRenderCallback discard;
    ASSERT_TRUE(player.collectRenderEvents(discard));
    Particle particle;
    particle.string = "presentation-event-fixture";
    auto assets = root.assets();
    ASSERT_TRUE(assets);
    auto sounds = (*assets)->scan("/sfx/", ".ogg");
    ASSERT_TRUE(sounds);
    ASSERT_FALSE(sounds->empty());
    std::sort(sounds->begin(), sounds->end());
    ASSERT_TRUE(player.addParticles({particle}));
    ASSERT_TRUE(player.addSound(sounds->front()));
    constexpr std::uint64_t gameplaySeed = 0x50524553454e54;
    root.random().init(gameplaySeed);
    RandomSource untouchedGameplay(gameplaySeed);
    for (float alpha : {0.0f, 0.25f, 0.75f, 1.0f, 0.5f}) {
      events.alpha = alpha;
      auto geometry = player.render(events);
      ASSERT_TRUE(geometry) << geometry.error().what();
      ASSERT_EQ(root.random().randu64(), untouchedGameplay.randu64()) << "geometry alpha=" << alpha;
      auto lighting = player.renderLightSources(events);
      ASSERT_TRUE(lighting) << lighting.error().what();
      ASSERT_EQ(root.random().randu64(), untouchedGameplay.randu64()) << "lighting alpha=" << alpha;
    }
    EXPECT_EQ(events.particles, 0u);
    EXPECT_EQ(events.audios, 0u);
    ASSERT_TRUE(player.collectRenderEvents(events));
    EXPECT_EQ(events.particles, 1u);
    EXPECT_EQ(events.audios, 1u);
    ASSERT_TRUE(player.render(events));
    ASSERT_TRUE(player.collectRenderEvents(events));
    EXPECT_EQ(events.particles, 1u);
    EXPECT_EQ(events.audios, 1u);

    // A tick with no display render still delivers its effects exactly once.
    ASSERT_TRUE(player.addParticles({particle}));
    ASSERT_TRUE(player.addSound(sounds->front()));
    ASSERT_TRUE(player.collectRenderEvents(events));
    EXPECT_EQ(events.particles, 2u);
    EXPECT_EQ(events.audios, 2u);
    ASSERT_TRUE(player.statusController().setPersistentEffects(
      "presentationFixture", {StatModifier{StatValueModifier{"invisible", 1.0f}}}));
    ASSERT_TRUE(player.invisible());
    ASSERT_TRUE(player.addParticles({particle}));
    ASSERT_TRUE(player.addSound(sounds->front()));
    ASSERT_TRUE(player.collectRenderEvents(events));
    EXPECT_EQ(events.particles, 2u);
    EXPECT_EQ(events.audios, 2u);
    ASSERT_TRUE(player.statusController().clearPersistentEffects("presentationFixture"));
    ASSERT_FALSE(player.invisible());
    ASSERT_TRUE(player.render(events));
    ASSERT_TRUE(player.collectRenderEvents(events));
    EXPECT_EQ(events.particles, 2u);
    EXPECT_EQ(events.audios, 2u);

    auto lua = LuaRoot::create(root);
    ASSERT_TRUE(lua);
    auto context = (*lua)->createContext(StringList{});
    ASSERT_TRUE(context);
    auto callbacks = LuaBindings::makeMovementControllerCallbacks(player.movementController());
    ASSERT_TRUE(callbacks);
    ASSERT_TRUE(context->setCallbacks("mcontroller", *callbacks));
    auto epoch = player.presentationEpoch();
    auto moved = context->eval<bool>(R"LUA(
      mcontroller.translate({0.1, 0})
      mcontroller.setXPosition(mcontroller.xPosition() + 0.1)
      return true
    )LUA");
    ASSERT_TRUE(moved);
    EXPECT_TRUE(*moved);
    EXPECT_EQ(player.presentationEpoch(), epoch);
    auto reset = context->eval<bool>("mcontroller.resetPresentation(); return true");
    ASSERT_TRUE(reset);
    EXPECT_TRUE(*reset);
    EXPECT_GT(player.presentationEpoch(), epoch);
    epoch = player.presentationEpoch();
    ASSERT_TRUE(player.moveTo(Vec2F(1200, 1100)));
    EXPECT_GT(player.presentationEpoch(), epoch);
  });

const GameTestRegistration hiddenLoopingPresentationAudio(
  "HumanoidRenderTest", "HiddenPresentationDiscardsOneShotsButRetainsLoops", [](GameTest& fixture) {
    auto assets = fixture.root().assets();
    ASSERT_TRUE(assets);
    auto sounds = (*assets)->scan("/sfx/", ".ogg");
    ASSERT_TRUE(sounds);
    ASSERT_FALSE(sounds->empty());
    std::sort(sounds->begin(), sounds->end());
    auto sample = (*assets)->audio(sounds->front());
    ASSERT_TRUE(sample);
    auto oneShot = AudioInstance::create(**sample);
    auto loop = AudioInstance::create(**sample);
    ASSERT_TRUE(oneShot);
    ASSERT_TRUE(loop);
    (*loop)->setLoops(-1);
    List<std::shared_ptr<AudioInstance>> retained;
    NullRenderCallback hidden(&retained);
    ASSERT_TRUE(hidden.addAudio(*oneShot));
    EXPECT_TRUE(retained.empty());
    ASSERT_TRUE(hidden.addAudio(*loop));
    ASSERT_EQ(retained.size(), 1u);
    EXPECT_EQ(retained[0], *loop);
    // The original instance survives so animator position/volume/stop updates
    // while hidden still affect the instance eventually submitted on reveal.
    (*loop)->setPosition(Vec2F(7, 9));
    EXPECT_EQ(*retained[0]->position(), Vec2F(7, 9));
    (*loop)->stop();
  });

const GameTestRegistration authoredAnimationGroupSampling(
  "HumanoidRenderTest", "SampledGroupCurvesRespectDiscreteAndInterpolatedFrames", [](GameTest& fixture) {
    auto config = Json::parse(R"({
      "version":1,
      "transformationGroups":{"discrete":{"interpolated":false},"smooth":{"interpolated":true}},
      "animatedParts":{
        "stateTypes":{"body":{"default":"walk","states":{
          "walk":{"frames":2,"cycle":1,"mode":"loop","frameProperties":{
            "discrete":[[["reset"],["translate",[0,0]]],[["reset"],["translate",[8,0]]]],
            "smooth":[[["reset"],["translate",[0,0]]],[["reset"],["translate",[8,0]]]]
          }}
        }}},
        "parts":{"body":{"properties":{"transformationGroups":["discrete"],"point":[0,0]},
          "partStates":{"body":{"walk":{}}}}}
      }
    })");
    ASSERT_TRUE(config);
    auto made = NetworkedAnimator::create(fixture.root(), *config);
    ASSERT_TRUE(made) << made.error().what();
    auto& animator = **made;
    ASSERT_TRUE(animator.update(.4f, {}));
    ASSERT_TRUE(animator.update(.2f, {}));
    for (float alpha : {.25f, .75f, .25f}) {
      auto sample = animator.sample(alpha);
      auto opposite = animator.sample(1.f - alpha);
      auto otherFrame = opposite.stateFrame("body");
      ASSERT_TRUE(otherFrame);
      EXPECT_EQ(*otherFrame, alpha < .5f ? 1 : 0);
      auto frame = sample.stateFrame("body");
      ASSERT_TRUE(frame);
      EXPECT_EQ(*frame, alpha < .5f ? 0 : 1);
      auto discrete = sample.partPoint("body", "point");
      ASSERT_TRUE(discrete);
      ASSERT_TRUE(*discrete);
      EXPECT_FLOAT_EQ((**discrete)[0], alpha < .5f ? 0 : 8);
      auto smooth = sample.groupTransformation({"smooth"});
      ASSERT_TRUE(smooth);
      EXPECT_NEAR(smooth->transformVec2(Vec2F{})[0], 7.2f, 1e-5f);
    }
    auto logical = animator.groupTransformation({"discrete"});
    ASSERT_TRUE(logical);
    EXPECT_FLOAT_EQ(logical->transformVec2(Vec2F{})[0], 8);
    auto timer = animator.stateTimer("body");
    ASSERT_TRUE(timer);
    EXPECT_NEAR(*timer, .6f, 1e-5f);
  });

const GameTestRegistration humanoidTickOwnedPose(
  "HumanoidRenderTest", "TickPoseAndFreshLightSamplesDoNotDependOnPreviousRender", [](GameTest& fixture) {
    // Vanilla species are legacy by default. Exercise the actual authored
    // builder rather than using its incomplete base animation by itself.
    auto directory = File::temporaryDirectory(fixture.root().random());
    ASSERT_TRUE(directory);
    auto cleanup = finally([&] { EXPECT_TRUE(File::removeDirectoryRecursive(*directory)); });
    auto overlay = File::relativeTo(*directory, "assets");
    ASSERT_TRUE(File::makeDirectoryRecursive(File::relativeTo(overlay, "species")));
    ASSERT_TRUE(File::writeFile(String(R"([
      {"op":"add","path":"/buildScripts","value":["/humanoid/opensb/build.lua"]}
    ])"), File::relativeTo(overlay, "species/human.species.patch")));
    auto settings = fixture.rootSettings();
    settings.assetSources.append(overlay);
    settings.storageDirectory = File::relativeTo(*directory, "storage");
    settings.runtimeConfigFile = {};
    settings.logDirectory = {};
    settings.logFile = {};
    settings.includeUGC = false;
    auto createdRoot = Root::create(std::move(settings));
    ASSERT_TRUE(createdRoot) << createdRoot.error().what();
    auto& root = **createdRoot;
    auto baseHumanoid = Humanoid::create(root, HumanoidIdentity{}, JsonObject{{"useAnimation", true}});
    ASSERT_TRUE(baseHumanoid) << baseHumanoid.error().what();
    auto config = (*baseHumanoid)->humanoidConfig(false);
    ASSERT_TRUE(config) << config.error().what();
    auto animation = config->get("animation");
    ASSERT_TRUE(animation) << animation.error().what();
    auto lights = Json::parse(R"({
      "headProbe":{"position":[1,0],"anchorPart":"head","color":[255,255,255]},
      "armProbe":{"position":[1,0],"anchorPart":"frontArm","color":[255,255,255]}
    })");
    ASSERT_TRUE(lights);
    auto augmentedAnimation = animation->set("lights", *lights);
    ASSERT_TRUE(augmentedAnimation);
    auto augmentedConfig = config->set("animation", *augmentedAnimation);
    ASSERT_TRUE(augmentedConfig);
    auto made = Humanoid::create(root, *augmentedConfig);
    ASSERT_TRUE(made) << made.error().what();
    auto& humanoid = **made;
    ASSERT_TRUE(humanoid.setIdentity(HumanoidIdentity{}));
    humanoid.setFacingDirection(Direction::Right);
    humanoid.setHandFrameOverrides(ToolHand::Primary, "rotation", "rotation");
    humanoid.setHandParameters(ToolHand::Primary, true, 0, 0, true, false, false);
    auto firstTick = humanoid.animate(1.f / 60, {});
    ASSERT_TRUE(firstTick) << firstTick.error().what();
    auto beforeMouth = humanoid.mouthOffset();
    auto beforeArmor = humanoid.headArmorOffset();
    ASSERT_TRUE(beforeMouth);
    ASSERT_TRUE(beforeArmor);
    humanoid.setHeadRotation(1.f);
    humanoid.setHandParameters(ToolHand::Primary, true, 1.f, 1.f, true, false, false);
    auto secondTick = humanoid.animate(1.f / 60, {});
    ASSERT_TRUE(secondTick) << secondTick.error().what();
    auto mouth = humanoid.mouthOffset();
    auto armor = humanoid.headArmorOffset();
    ASSERT_TRUE(mouth);
    ASSERT_TRUE(armor);
    EXPECT_NE(*mouth, *beforeMouth);
    EXPECT_NE(*armor, *beforeArmor);
    auto animatorRef = humanoid.networkedAnimator();
    ASSERT_TRUE(animatorRef);
    auto& animator = animatorRef->get();
    auto logicalHead = animator.getLocalTransformationGroup("headRotation");
    auto logicalArm = animator.getLocalTransformationGroup("frontArmRotation");
    ASSERT_TRUE(logicalHead);
    ASSERT_TRUE(logicalArm);
    for (float alpha : {0.f, .5f, 1.f, .5f}) {
      List<LightSource> sampledLights;
      {
        auto sample = animator.sample(alpha);
        ASSERT_TRUE(humanoid.prepareAnimatorPose(sample));
        auto freshLights = sample.lightSources();
        ASSERT_TRUE(freshLights);
        ASSERT_EQ(freshLights->size(), 2u);
        sampledLights = *freshLights;
      }
      {
        auto sample = animator.sample(alpha);
        auto drawables = humanoid.render(sample, true, true);
        ASSERT_TRUE(drawables) << drawables.error().what();
        auto afterGeometry = sample.lightSources();
        ASSERT_TRUE(afterGeometry);
        ASSERT_EQ(afterGeometry->size(), sampledLights.size());
        for (std::size_t i = 0; i != sampledLights.size(); ++i) {
          EXPECT_NEAR((*afterGeometry)[i].position[0], sampledLights[i].position[0], 1e-5f);
          EXPECT_NEAR((*afterGeometry)[i].position[1], sampledLights[i].position[1], 1e-5f);
        }
      }
      auto preservedMouth = humanoid.mouthOffset();
      auto preservedArmor = humanoid.headArmorOffset();
      auto preservedHead = animator.getLocalTransformationGroup("headRotation");
      auto preservedArm = animator.getLocalTransformationGroup("frontArmRotation");
      ASSERT_TRUE(preservedMouth);
      ASSERT_TRUE(preservedArmor);
      ASSERT_TRUE(preservedHead);
      ASSERT_TRUE(preservedArm);
      EXPECT_EQ(*preservedMouth, *mouth);
      EXPECT_EQ(*preservedArmor, *armor);
      EXPECT_EQ(*preservedHead, *logicalHead);
      EXPECT_EQ(*preservedArm, *logicalArm);
    }
  });

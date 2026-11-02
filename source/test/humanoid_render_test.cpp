#include "gtest/gtest.h"

import std;
import star.actor_movement_controller;
import star.algorithm;
import star.armors;
import star.asset_path;
import star.assets;
import star.casting;
import star.client_context;
import star.drawable;
import star.file;
import star.game_test;
import star.game_types;
import star.humanoid;
import star.item;
import star.item_database;
import star.item_descriptor;
import star.json;
import star.lua_root;
import star.net_compatibility;
import star.player;
import star.player_factory;
import star.player_lua_bindings;
import star.root;
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

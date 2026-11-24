#include "gtest/gtest.h"

import std;
import star.actor_movement_controller;
import star.animated_part_set;
import star.color;
import star.algorithm;
import star.armor_wearer;
import star.armors;
import star.asset_path;
import star.assets;
import star.casting;
import star.client_context;
import star.damage;
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
import star.status_controller_lua_bindings;
import star.status_types;
import star.game_test;
import star.game_types;
import star.humanoid;
import star.image_metadata_database;
import star.item;
import star.item_database;
import star.item_descriptor;
import star.json;
import star.line;
import star.matrix3;
import star.networked_animator;
import star.lua;
import star.lua_root;
import star.net_compatibility;
import star.player;
import star.poly;
import star.player_factory;
import star.player_lua_bindings;
import star.root;
import star.random;
import star.string;
import star.string_view;
import star.liquid_types;
import star.material_types;
import star.uuid;
import star.vector;
import star.world_server;

using namespace Star;

namespace {
TEST(EntityRenderingTypes, LayerSpellingsAndUnsignedPacking) {
  EXPECT_EQ(parse_render_layer("pReViEwObJeCt"), render_layer_preview_object);
  EXPECT_EQ(parse_render_layer("Player+3"), render_layer_player + 3u);
  EXPECT_EQ(parse_render_layer("Player-3"), render_layer_player - 3u);
  EXPECT_EQ(parse_render_layer("Player+-3"), render_layer_player - 3u);
  EXPECT_EQ(parse_render_layer("Player--3"), render_layer_player + 3u);
  EXPECT_EQ(parse_render_layer("Player--2147483648"), render_layer_player + 0x80000000u);
  EXPECT_EQ(parse_render_layer("Player+-2147483648"), render_layer_player - 0x80000000u);
  EXPECT_FALSE(parse_render_layer("Player-3+2"));
  EXPECT_FALSE(parse_render_layer("Player++3"));
  EXPECT_FALSE(parse_render_layer("Player-"));
  EXPECT_FALSE(parse_render_layer("unknown"));
  EXPECT_EQ(vehicle_layer_offset(std::numeric_limits<EntityId>::min(), 1), 1u);
  EXPECT_EQ(vehicle_layer_offset(std::numeric_limits<EntityId>::max(), 2), render_layer_lower_mask - 1u);
  EXPECT_EQ(vehicle_layer_offset(-1, 0), render_layer_lower_mask - 3u);
}

TEST(EntityRenderingTypes, PreviewDefaultsAndBarSchema) {
  PreviewTile blank;
  EXPECT_FALSE(blank.foreground);
  EXPECT_FALSE(blank.update_mat_id);
  EXPECT_FALSE(blank.update_light);
  EXPECT_EQ(blank.mat_id, NullMaterialId);
  EXPECT_EQ(blank.liq_id, EmptyLiquidId);
  EXPECT_EQ(blank.hue_shift, 0);
  EXPECT_EQ(blank.color_variant, DefaultMaterialColorVariant);
  PreviewTile liquid(Vec2I(2, 3), LiquidId{5});
  EXPECT_TRUE(liquid.foreground);
  EXPECT_EQ(liquid.liq_id, 5);
  EXPECT_EQ(liquid.mat_id, NullMaterialId);

  auto bar = OverheadBar::create(JsonObject{{"percentage", .25f}, {"color", JsonArray{12, 34, 56}},
      {"position", JsonArray{4, 5}}, {"detailOnly", true}});
  ASSERT_TRUE(bar) << bar.error().what();
  EXPECT_EQ(bar->entity_position, Vec2F(4, 5));
  EXPECT_EQ(bar->percentage, .25f);
  EXPECT_TRUE(bar->detail_only);
  EXPECT_FALSE(bar->icon);
}

struct PlayerWorldContext {
  PlayerWorldContext() = default;
  PlayerWorldContext(PlayerWorldContext const&) = delete;
  PlayerWorldContext& operator=(PlayerWorldContext const&) = delete;

  ~PlayerWorldContext() {
    if (initialized)
      EXPECT_TRUE(player->uninit());
    player.reset();
    client.reset();
    if (world)
      EXPECT_TRUE(world->close());
  }

  std::expected<void, StarException> init(Root& root) {
    auto opened = File::ephemeral_file(root.random());
    if (!opened)
      return std::unexpected(std::move(opened.error()));
    storage = std::move(*opened);
    auto created = WorldServer::create(root, Vec2U(2048, 2048), storage);
    if (!created)
      return std::unexpected(std::move(created.error()));
    world = std::move(*created);
    world->setSpawningEnabled(false);
    auto acquired = root.playerFactory();
    if (!acquired)
      return std::unexpected(std::move(acquired.error()));
    factory = std::move(*acquired);
    auto made = factory->create();
    if (!made)
      return std::unexpected(std::move(made.error()));
    player = std::move(*made);
    if (auto finalized = player->finalizeCreation(); !finalized)
      return std::unexpected(std::move(finalized.error()));
    auto uuid = player->uuid();
    if (!uuid)
      return std::unexpected(std::move(uuid.error()));
    auto connected = ClientContext::create(root.logger(), Uuid(root.random()), *uuid, 1, {});
    if (!connected)
      return std::unexpected(std::move(connected.error()));
    client = std::move(*connected);
    player->set_client_context(client);
    if (auto entered = player->init(*world, 100, EntityMode::Master); !entered)
      return std::unexpected(std::move(entered.error()));
    initialized = true;
    return player->refreshHumanoid();
  }

  std::shared_ptr<File> storage;
  std::shared_ptr<WorldServer> world;
  std::shared_ptr<PlayerFactory const> factory;
  std::shared_ptr<ClientContext> client;
  std::shared_ptr<Player> player;
  bool initialized = false;
};

// Vanilla species use the legacy renderer; opt in through the real builder,
// which supplies the state tables and attachment points for animator rendering.
std::expected<std::unique_ptr<Root>, StarException> human_animation_root(GameTest& fixture, String const& directory) {
  auto overlay = File::relative_to(directory, "assets");
  if (auto made = File::create_directories(File::relative_to(overlay, "species")); !made)
    return std::unexpected(std::move(made.error()));
  if (auto wrote = File::write_file(String(R"([
    {"op":"add","path":"/buildScripts","value":["/humanoid/asterbound/build.lua"]}
  ])"), File::relative_to(overlay, "species/human.species.patch")); !wrote)
    return std::unexpected(std::move(wrote.error()));
  auto settings = fixture.rootSettings();
  settings.assetSources.push_back(overlay);
  settings.storageDirectory = File::relative_to(directory, "storage");
  settings.runtimeConfigFile = {};
  settings.logDirectory = {};
  settings.logFile = {};
  settings.includeUGC = false;
  return Root::create(std::move(settings));
}
}// namespace

const GameTestRegistration self_damage_history(
  "StatusControllerTest", "SelfDamageRecordedOnceForObserversAndPendingDelivery", [](GameTest& fixture) {
    PlayerWorldContext setup;
    auto initialized = setup.init(fixture.root());
    ASSERT_TRUE(initialized) << initialized.error().what();
    auto& player = *setup.player;
    auto lua = LuaRoot::create(fixture.root());
    ASSERT_TRUE(lua) << lua.error().what();
    auto context = (*lua)->create_context(StringList{});
    ASSERT_TRUE(context) << context.error().what();
    auto callbacks = LuaBindings::makeStatusControllerCallbacks(player.statusController());
    ASSERT_TRUE(callbacks) << callbacks.error().what();
    ASSERT_TRUE(context->set_callbacks("status", *callbacks));
    ASSERT_TRUE(context->set_path("player_id", player.entityId()));

    // Use the same request as the primary player script's fall-damage path.
    auto observed = context->eval<Json>(R"LUA(
      local _, before = status.damageTakenSince()
      local _, hits_before = status.inflictedHitsSince()
      local _, dealt_before = status.inflictedDamageSince()
      status.applySelfDamageRequest({
        damageType = "IgnoresDef", damage = 1, damageSourceKind = "falling", sourceEntityId = player_id
      })
      local events, after = status.damageTakenSince(before)
      assert(#events == 1 and after == before + 1)
      local unseen, cursor = status.damageTakenSince(after)
      assert(#unseen == 0 and cursor == after)
      local hits, hits_after = status.inflictedHitsSince(hits_before)
      local dealt, dealt_after = status.inflictedDamageSince(dealt_before)
      assert(#hits == 0 and hits_after == hits_before and #dealt == 0 and dealt_after == dealt_before)
      return events[1]
    )LUA");
    ASSERT_TRUE(observed) << observed.error().what();
    EXPECT_EQ(observed->get_int("targetEntityId"), player.entityId());
    EXPECT_EQ(observed->get_string("damageSourceKind"), "falling");
    auto pending = player.selfDamageNotifications();
    ASSERT_TRUE(pending) << pending.error().what();
    ASSERT_EQ(pending->size(), 1u);
    EXPECT_EQ(pending->front().to_json(), *observed);
    auto drained = player.selfDamageNotifications();
    ASSERT_TRUE(drained) << drained.error().what();
    EXPECT_TRUE(drained->empty());

    auto request = DamageRequest::create(JsonObject{{"damage", 3}});
    ASSERT_TRUE(request) << request.error().what();
    ASSERT_TRUE(player.hitOther(41, *request));
    ASSERT_TRUE(player.hitOther(42, *request));
    ASSERT_TRUE(player.damagedOther(pending->front()));
    auto inflicted = context->eval<Json>(R"LUA(
      local hits, cursor = status.inflictedHitsSince()
      assert(#hits == 2 and hits[1].targetEntityId == 41 and hits[2].targetEntityId == 42)
      assert(hits[1].damage == 3 and hits[2].hitType == "Hit")
      local unseen, next_cursor = status.inflictedHitsSince(cursor)
      assert(#unseen == 0 and next_cursor == cursor)
      local events, after = status.inflictedDamageSince()
      assert(#events == 1 and #status.inflictedDamageSince(after) == 0)
      return events[1]
    )LUA");
    ASSERT_TRUE(inflicted) << inflicted.error().what();
    EXPECT_EQ(*inflicted, *observed);
  });

const GameTestRegistration armor_visibility_transitions(
  "ArmorWearerTest", "CosmeticBoundsMaskingNudityAndConfigRetry", [](GameTest& fixture) {
    auto& root = fixture.root();
    auto assets = root.assets();
    ASSERT_TRUE(assets);
    auto config = (*assets)->json("/humanoid.config");
    ASSERT_TRUE(config);
    auto humanoid = Humanoid::create(root, *config);
    ASSERT_TRUE(humanoid);
    ASSERT_TRUE((*humanoid)->setIdentity(HumanoidIdentity{}));
    auto owner = ArmorWearer::create(root);
    ASSERT_TRUE(owner);
    auto& wearer = **owner;
    auto database = root.itemDatabase();
    ASSERT_TRUE(database);
    String name;
    for (auto const& candidate : (*database)->allItems()) {
      auto type = (*database)->itemType(candidate);
      ASSERT_TRUE(type);
      if (*type == ItemType::HeadArmor) {
        name = candidate;
        break;
      }
    }
    ASSERT_FALSE(name.empty());
    auto make_armor = [&](JsonObject overrides) -> std::shared_ptr<ArmorItem> {
      auto descriptor = ItemDescriptor::create(name, 1, std::move(overrides));
      if (!descriptor)
        return {};
      auto item = (*database)->item(*descriptor);
      return item ? as<ArmorItem>(std::move(*item)) : std::shared_ptr<ArmorItem>{};
    };
    auto base = (*humanoid)->defaultMovementParameters();
    auto functional = make_armor({{"hideInVanillaSlots", false}, {"bypassNude", false},
        {"humanoidConfig", JsonObject{{"movementParameters", JsonObject{{"armorProbe", 1}}}}}});
    auto cosmetic = make_armor({{"armorTypesToHide", JsonArray{"Head"}}, {"humanoidConfig", JsonObject{}}});
    ASSERT_TRUE(functional);
    ASSERT_TRUE(cosmetic);
    ASSERT_TRUE(wearer.set_item(0, functional));
    for (unsigned invalid : {12u, 248u, 255u})
      EXPECT_FALSE(wearer.set_cosmetic_item(invalid, cosmetic));
    EXPECT_EQ(wearer.item_descriptor(0), functional->descriptor());
    auto refresh = [&](bool nude = false) { return wearer.setup_humanoid(**humanoid, nude); };
    ASSERT_EQ(refresh(), true);
    auto equipped = (*humanoid)->defaultMovementParameters();
    ASSERT_NE(equipped, base);
    EXPECT_EQ(refresh(), false);
    for (unsigned relative : {0u, 11u}) {
      ASSERT_TRUE(wearer.set_cosmetic_item(relative, cosmetic));
      EXPECT_EQ(wearer.item_descriptor(8 + relative), cosmetic->descriptor());
      ASSERT_EQ(refresh(), true);
      EXPECT_EQ((*humanoid)->defaultMovementParameters(), base);
      ASSERT_TRUE(wearer.set_cosmetic_item(relative, {}));
      ASSERT_EQ(refresh(), true);
      EXPECT_EQ((*humanoid)->defaultMovementParameters(), equipped);
    }
    ASSERT_EQ(refresh(true), true);
    EXPECT_EQ((*humanoid)->defaultMovementParameters(), base);
    ASSERT_EQ(refresh(), true);
    auto broken = make_armor({{"humanoidConfig", JsonObject{{"headRotationCenter", JsonArray{1}}}},
                               {"hideInVanillaSlots", false}});
    ASSERT_TRUE(broken);
    ASSERT_TRUE(wearer.set_item(0, broken));
    EXPECT_FALSE(refresh().has_value());
    EXPECT_FALSE(refresh().has_value());
    ASSERT_TRUE(wearer.set_item(0, {}));
    EXPECT_EQ(refresh(), true);
    EXPECT_EQ((*humanoid)->defaultMovementParameters(), base);
  });

const GameTestRegistration
  sleeveLighting("HumanoidRenderTest", "BackSleeveRetainsArmorLightingWhileHolding", [](GameTest& fixture) {
    auto assets = fixture.root().assets();
    ASSERT_TRUE(assets);
    auto config = (*assets)->json("/humanoid.config");
    ASSERT_TRUE(config);
    // Exercise the legacy humanoid path still used by species without an animator.
    auto legacy = config->erase("animation");
    ASSERT_TRUE(legacy);
    auto humanoid = Humanoid::create(fixture.root(), *legacy);
    ASSERT_TRUE(humanoid) << humanoid.error().what();
    HumanoidIdentity identity;
    ASSERT_TRUE((*humanoid)->setIdentity(identity));
    auto database = fixture.root().itemDatabase();
    ASSERT_TRUE(database) << database.error().what();
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
      auto chest = as_ref<ChestArmor>(**item);
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
      auto chest = as_ref<ChestArmor>(**item);
      ASSERT_TRUE(chest);
      ASSERT_EQ(chest->get().fullbright(), fullbright);
      ASSERT_TRUE((*humanoid)->setWearableFromChest(0, chest->get(), identity.gender));
      auto sleeve = AssetPath::split(chest->get().backSleeveFrameset(identity.gender)).base_path;
      for (auto direction : {Direction::Left, Direction::Right}) {
        (*humanoid)->setFacingDirection(direction);
        for (bool holding : {false, true}) {
          (*humanoid)->setHandParameters(ToolHand::Primary, holding, 0, 0, false, false, false);
          (*humanoid)->setHandParameters(ToolHand::Alt, holding, 0, 0, false, false, false);
          auto drawables = (*humanoid)->render();
          ASSERT_TRUE(drawables) << drawables.error().what();
          unsigned sleeves = 0;
          for (auto const& drawable : *drawables) {
            if (drawable.is_image() && drawable.image_part().image.base_path == sleeve) {
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
    auto directory = File::temporary_directory(fixture.root().random());
    ASSERT_TRUE(directory);
    auto cleanup = make_scope_exit([&] { EXPECT_TRUE(File::remove_all(*directory)); });
    auto overlay = File::relative_to(*directory, "assets");
    ASSERT_TRUE(File::create_directories(overlay));
    ASSERT_TRUE(File::write_file(String(R"([
      {"op":"replace","path":"/genericScriptContexts","value":{"movementPoseFixture":"/pose.lua"}}
    ])"),
                                 File::relative_to(overlay, "player.config.patch")));
    ASSERT_TRUE(File::write_file(String(R"LUA(
      function update()
        local state = player.getProperty("poseFixture")
        if state then player.controlMovementState(state) end
        if player.getProperty("poseRevoke") then player.controlMovementState(nil) end
      end
      function postUpdate()
        if player.getProperty("poseLate") then player.controlMovementState("duck") end
      end
    )LUA"),
                                 File::relative_to(overlay, "pose.lua")));
    auto settings = fixture.rootSettings();
    settings.assetSources.push_back(overlay);
    settings.storageDirectory = File::relative_to(*directory, "storage");
    settings.runtimeConfigFile = {};
    settings.logDirectory = {};
    settings.logFile = {};
    settings.includeUGC = false;
    auto root = Root::create(std::move(settings));
    ASSERT_TRUE(root) << root.error().what();
    PlayerWorldContext setup;
    auto initialized = setup.init(**root);
    ASSERT_TRUE(initialized) << initialized.error().what();
    auto& player = *setup.player;
    auto& movement = player.movementController();
    ActorMovementParameters parameters;
    parameters.gravity_enabled = false;
    parameters.collision_enabled = false;
    parameters.friction_enabled = false;
    ASSERT_TRUE(movement.update_base_parameters(parameters));
    movement.setPosition(Vec2F(1024, 1024));
    auto nativeCollisionPoly = movement.collisionPoly();

    // The real Lua callback rejects other humanoid states, not just unknown
    // names, and accepts nil. No fixture implementation stands in for it.
    auto lua = LuaRoot::create(**root);
    ASSERT_TRUE(lua);
    auto context = (*lua)->create_context(StringList{});
    ASSERT_TRUE(context);
    auto callbacks = LuaBindings::makePlayerCallbacks(**root, player);
    ASSERT_TRUE(callbacks);
    ASSERT_TRUE(context->set_callbacks("player", *callbacks));
    auto valid = context->eval<bool>(R"LUA(
      for _, state in ipairs({"idle", "walk", "run", "jump", "fall", "duck"}) do
        assert(pcall(player.controlMovementState, state))
      end
      for _, state in ipairs({"swim", "crouch", "sit", "lay", "RUN", "invalid"}) do
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
    std::array rules{NetCompatibilityRules(legacy_version), NetCompatibilityRules(open_protocol_version)};
    std::array<std::shared_ptr<Player>, 2> remotes;
    std::array<std::uint64_t, 2> versions{};
    auto uninitRemotes = make_scope_exit([&] {
      for (auto const& remote : remotes)
        if (remote)
          EXPECT_TRUE(remote->uninit());
    });
    for (std::size_t i = 0; i != remotes.size(); ++i) {
      auto store = player.net_store(rules[i]);
      ASSERT_TRUE(store);
      auto remote = setup.factory->net_load_player(*store, rules[i]);
      ASSERT_TRUE(remote) << remote.error().what();
      remotes[i] = *remote;
      ASSERT_TRUE(remotes[i]->init(*setup.world, 101 + i, EntityMode::Slave));
    }
    // This tests the actual legacy compatibility filter, not a custom animation
    // parameter or a new tech state. It is not a vanilla-executable smoke test.
    auto sync = [&](Humanoid::State state, Direction networkDirection, bool full = false) {
      for (std::size_t i = 0; i != remotes.size(); ++i) {
        SCOPED_TRACE(rules[i].version());
        auto packet = player.write_net_state(full ? 0 : versions[i], rules[i]);
        ASSERT_TRUE(packet);
        versions[i] = packet->second;
        ASSERT_TRUE(remotes[i]->read_net_state(packet->first, 0.f, rules[i]));
        ASSERT_TRUE(remotes[i]->update(dt, step));
        EXPECT_EQ(remotes[i]->humanoid()->state(), state);
        EXPECT_EQ(remotes[i]->humanoid()->facingDirection(), player.humanoid()->facingDirection());
        EXPECT_EQ(remotes[i]->humanoid()->movingBackwards(), player.humanoid()->movingBackwards());
        EXPECT_EQ(remotes[i]->movementController().moving_direction(), networkDirection);
        EXPECT_NEAR(remotes[i]->movementController().rotation(), movement.rotation(), .011f);
        EXPECT_FALSE(remotes[i]->movementController().onGround());
        EXPECT_FALSE(remotes[i]->movementController().crouching());
        EXPECT_EQ(remotes[i]->movementController().collisionPoly(), nativeCollisionPoly);
        auto rendered = remotes[i]->humanoid()->render();
        ASSERT_TRUE(rendered) << rendered.error().what();
        EXPECT_FALSE(rendered->empty());
      }
    };
    std::array states{Humanoid::Idle, Humanoid::Walk, Humanoid::Run, Humanoid::Jump, Humanoid::Fall, Humanoid::Duck};
    std::array<std::string_view, 6> names{"idle", "walk", "run", "jump", "fall", "duck"};
    auto nativeDirection = movement.moving_direction();
    for (float rotation : {0.f, std::numbers::pi_v<float> / 2, std::numbers::pi_v<float>, -std::numbers::pi_v<float> / 2}) {
      movement.setRotation(rotation);
      auto tangent = Vec2F::with_angle(rotation);
      for (std::size_t i = 0; i != states.size(); ++i) {
        player.setGenericProperty("poseFixture", Json(String(names[i])));
        for (auto facing : {Direction::Left, Direction::Right}) {
          for (auto direction : {Direction::Left, Direction::Right}) {
            movement.control_face(facing);
            movement.setVelocity(tangent * (direction == Direction::Left ? -14.f : 14.f));
            ASSERT_TRUE(tick());
            EXPECT_EQ(player.humanoid()->state(), states[i]);
            EXPECT_EQ(player.humanoid()->facingDirection(), facing);
            EXPECT_EQ(player.humanoid()->movingBackwards(), facing != direction);
            EXPECT_EQ(movement.moving_direction(), nativeDirection);
            EXPECT_FALSE(movement.onGround());
            EXPECT_TRUE(movement.zeroG());
            EXPECT_FALSE(movement.crouching());
            EXPECT_EQ(movement.collisionPoly(), nativeCollisionPoly);
            sync(states[i], direction);
            // Exercise Duck both on the existing incremental stream and a
            // complete legacy/current packet, without a new presentation field.
            if (states[i] == Humanoid::Duck)
              sync(states[i], direction, true);
          }
        }
      }
    }

    // Native direction must keep updating underneath an active wire override.
    // In particular ceiling rightward motion has negative world-X velocity.
    movement.setRotation(std::numbers::pi_v<float>);
    player.setGenericProperty("poseFixture", "run");
    for (auto direction : {Direction::Right, Direction::Left}) {
      movement.control_move(direction);
      movement.control_face(Direction::Right);
      movement.setVelocity(Vec2F(-14.f, 0.f));
      ASSERT_TRUE(tick());
      EXPECT_EQ(movement.moving_direction(), direction);
      sync(Humanoid::Run, Direction::Right);
    }
    nativeDirection = movement.moving_direction();
    movement.setVelocity(Vec2F());
    ASSERT_TRUE(tick());
    EXPECT_FALSE(player.humanoid()->movingBackwards());
    sync(Humanoid::Run, player.humanoid()->facingDirection());
    player.setGenericProperty("poseFixture", "duck");
    ASSERT_TRUE(tick());
    sync(Humanoid::Duck, player.humanoid()->facingDirection());

    player.setGenericProperty("poseFixture", Json());
    ASSERT_TRUE(tick());
    EXPECT_EQ(player.humanoid()->state(), Humanoid::SwimIdle);
    EXPECT_EQ(movement.moving_direction(), nativeDirection);
    sync(Humanoid::SwimIdle, nativeDirection);
    player.setGenericProperty("poseFixture", "duck");
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
    player.setGenericProperty("poseFixture", "duck");
    ASSERT_TRUE(tick());
    player.setGenericProperty("poseFixture", Json());
    ASSERT_TRUE(player.uninit());
    ASSERT_TRUE(player.init(*setup.world, 100, EntityMode::Master));
    ASSERT_TRUE(player.refreshHumanoid());
    ASSERT_TRUE(movement.update_base_parameters(parameters));
    ASSERT_TRUE(tick());
    EXPECT_EQ(player.humanoid()->state(), Humanoid::SwimIdle);
    sync(Humanoid::SwimIdle, movement.moving_direction());
    player.setGenericProperty("poseFixture", "duck");
    movement.setVelocity(Vec2F());
    movement.control_face(Direction::Right);
    ASSERT_TRUE(tick());
    sync(Humanoid::Duck, Direction::Right);
    ASSERT_TRUE(player.teleportOut());
    // A higher-priority state can start between update and packet generation.
    for (std::size_t i = 0; i != remotes.size(); ++i) {
      auto packet = player.write_net_state(versions[i], rules[i]);
      ASSERT_TRUE(packet);
      versions[i] = packet->second;
      ASSERT_TRUE(remotes[i]->read_net_state(packet->first, 0.f, rules[i]));
      EXPECT_TRUE(remotes[i]->isTeleporting());
      EXPECT_EQ(remotes[i]->movementController().moving_direction(), movement.moving_direction());
    }
    ASSERT_TRUE(tick());
    EXPECT_TRUE(player.isTeleporting());
    // Teleport freezes the previous humanoid cycle, but must send its native
    // teleport state and restore native movingDirection, not the request.
    sync(player.humanoid()->state(), movement.moving_direction());
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
    auto abortSeconds = abortTime->to_float();
    ASSERT_TRUE(abortSeconds);
    auto abortTicks = static_cast<unsigned>(std::ceil(*abortSeconds / dt)) + 1;
    for (unsigned i = 0; i < abortTicks && player.isTeleporting(); ++i)
      ASSERT_TRUE(tick());
    ASSERT_FALSE(player.isTeleporting());
    ASSERT_EQ(player.humanoid()->state(), Humanoid::Duck);
    sync(Humanoid::Duck, Direction::Right);
    ASSERT_TRUE(player.kill());
    ASSERT_TRUE(player.isDead());
    ASSERT_TRUE(tick());
    ASSERT_TRUE(player.isDead());
    EXPECT_NE(player.humanoid()->state(), Humanoid::Duck);
    sync(player.humanoid()->state(), movement.moving_direction());
  });

const GameTestRegistration refreshedMovementCycles(
  "HumanoidRenderTest", "RefreshedRotatedWalkRunAdvanceVisibleFrames", [](GameTest& fixture) {
    auto assets = fixture.root().assets();
    ASSERT_TRUE(assets);
    auto config = (*assets)->json("/humanoid.config");
    ASSERT_TRUE(config);
    auto legacy = config->erase("animation");
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
                if (drawable.is_image() && drawable.image_part().image.sub_path)
                  renderedFrames += AssetPath::join(drawable.image_part().image);
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
    auto logical = parts->active_state("movement");
    ASSERT_TRUE(logical);
    EXPECT_FLOAT_EQ(logical->get().timer, .4f);
    AnimatedPartSet::Sample sample;
    parts->prepare_sample(sample, .5f);
    auto sampled = parts->active_state("movement", sample);
    ASSERT_TRUE(sampled);
    EXPECT_FLOAT_EQ(sampled->get().timer, .2f);
    auto body = parts->active_part("body", sample);
    ASSERT_TRUE(body);
    EXPECT_NEAR(body->get().animation_affine_transform().transform_point(Vec2F{})[0], 3.2f, 1e-5f);
    logical = parts->active_state("movement");
    ASSERT_TRUE(logical);
    EXPECT_FLOAT_EQ(logical->get().timer, .4f);
    ASSERT_TRUE(parts->update(.5f));
    ASSERT_TRUE(parts->update(.2f));
    parts->prepare_sample(sample, .25f);
    sampled = parts->active_state("movement", sample);
    ASSERT_TRUE(sampled);
    EXPECT_NEAR(sampled->get().timer, .95f, 1e-5f);
    parts->prepare_sample(sample, .75f);
    sampled = parts->active_state("movement", sample);
    ASSERT_TRUE(sampled);
    EXPECT_NEAR(sampled->get().timer, .05f, 1e-5f);
    ASSERT_TRUE(parts->restart_state("movement"));
    parts->prepare_sample(sample, 0);
    sampled = parts->active_state("movement", sample);
    ASSERT_TRUE(sampled);
    EXPECT_FLOAT_EQ(sampled->get().timer, 0);
    ASSERT_TRUE(parts->set_active_state("movement", "change"));
    ASSERT_TRUE(parts->update(.2f));
    parts->prepare_sample(sample, .5f);
    sampled = parts->active_state("movement", sample);
    ASSERT_TRUE(sampled);
    EXPECT_EQ(sampled->get().state_name, "stop");
    EXPECT_FLOAT_EQ(sampled->get().timer, 0);
    ASSERT_TRUE(parts->set_active_state("movement", "walk", true, true));
    ASSERT_TRUE(parts->update(.4f));
    parts->prepare_sample(sample, .5f);
    sampled = parts->active_state("movement", sample);
    ASSERT_TRUE(sampled);
    EXPECT_EQ(sampled->get().frame, 1u);
    EXPECT_EQ(sampled->get().next_frame, 0u);
    EXPECT_NEAR(sampled->get().frame_progress, .4f, 1e-5f);
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
    auto line = Drawable::make_line(Line2F({0, 0}, {1, 0}), 1, Color::palette_white);
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
      sample.set_part_drawables("body", std::nullopt);
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
    EXPECT_FLOAT_EQ(local->transform_point(Vec2F{})[0], 0);
    EXPECT_TRUE(events.pullNewAudios().empty());
    EXPECT_TRUE(events.pullNewParticles().empty());
    ASSERT_TRUE(animator.rotateGroup("aim", -2.f, true));
    auto sample = animator.sample(0);
    auto angle = sample.currentRotationAngle("aim");
    ASSERT_TRUE(angle);
    EXPECT_FLOAT_EQ(*angle, -2.f);
  });

const GameTestRegistration sampledPartTransformOverrides(
  "HumanoidRenderTest", "SharedSampleTransformOverridesReachAnchorsAndConsumers", [](GameTest& fixture) {
    auto config = Json::parse(R"({
      "version":1,
      "transformationGroups":{"moving":{}},
      "animatedParts":{
        "stateTypes":{"pose":{"default":"idle","states":{
          "idle":{"frames":1,"cycle":1,"properties":{"moving":[["reset"],["translate",[1,0]]]}},
          "shifted":{"frames":1,"cycle":1,"properties":{"moving":[["reset"],["translate",[3,0]]]}},
          "badGroup":{"frames":1,"cycle":1},
          "badAnchor":{"frames":1,"cycle":1}
        }}},
        "parts":{
          "parent":{
            "properties":{"offset":[2,0],"transformationGroups":["moving"]},
            "partStates":{"pose":{
              "idle":{},
              "shifted":{"properties":{"offset":[4,0]}},
              "badGroup":{"properties":{"transformationGroups":["missing"]}},
              "badAnchor":{}
            }}
          },
          "child":{
            "properties":{"anchorPart":"parent","offset":[0,3],"point":[1,0],"poly":[[0,0],[1,0],[0,1]]},
            "partStates":{"pose":{
              "idle":{},"shifted":{},"badGroup":{},
              "badAnchor":{"properties":{"anchorPart":"missing"}}
            }}
          }
        }
      },
      "lights":{"glow":{"active":true,"position":[0,0],"color":[255,255,255],"anchorPart":"child"}}
    })");
    ASSERT_TRUE(config);
    auto made = NetworkedAnimator::create(fixture.root(), *config);
    ASSERT_TRUE(made) << made.error().what();
    auto& animator = **made;
    ASSERT_TRUE(animator.update(0, {}));
    auto line = Drawable::make_line(Line2F({0, 0}, {1, 0}), 1, Color::palette_white);
    animator.setPartDrawables("child", {line});
    auto expectVec = [](Vec2F const& actual, Vec2F const& expected) {
      EXPECT_NEAR(actual[0], expected[0], 1e-5f);
      EXPECT_NEAR(actual[1], expected[1], 1e-5f);
    };
    auto expectPose = [&](NetworkedAnimator::Sample const& view, Mat3F const& parent, Mat3F const& global = Mat3F::identity()) {
      auto child = parent * Mat3F::translation({0, 3});
      // Query the child first so the parent's transformation is also reached recursively.
      auto childTransform = view.partTransformation("child");
      auto parentTransform = view.partTransformation("parent");
      ASSERT_TRUE(childTransform);
      ASSERT_TRUE(parentTransform);
      for (auto point : {Vec2F{}, Vec2F{1, 0}, Vec2F{0, 1}}) {
        expectVec(childTransform->transform_point(point), child.transform_point(point));
        expectVec(parentTransform->transform_point(point), parent.transform_point(point));
      }
      auto final = global * child;
      auto point = view.partPoint("child", "point");
      auto poly = view.partPoly("child", "poly");
      auto lights = view.lightSources();
      auto drawables = view.drawables();
      ASSERT_TRUE(point);
      ASSERT_TRUE(*point);
      ASSERT_TRUE(poly);
      ASSERT_TRUE(*poly);
      ASSERT_TRUE(lights);
      ASSERT_EQ(lights->size(), 1u);
      ASSERT_TRUE(drawables);
      ASSERT_EQ(drawables->size(), 1u);
      expectVec(**point, final.transform_point(Vec2F{1, 0}));
      ASSERT_EQ((**poly).sides(), 3u);
      expectVec((**poly)[0], final.transform_point(Vec2F{0, 0}));
      expectVec((**poly)[1], final.transform_point(Vec2F{1, 0}));
      expectVec((**poly)[2], final.transform_point(Vec2F{0, 1}));
      expectVec((*lights)[0].position, final.transform_point(Vec2F{0, 0}));
      auto expectedLine = line;
      expectedLine.transform(final);
      ASSERT_TRUE((*drawables)[0].is_line());
      expectVec((*drawables)[0].position, expectedLine.position);
      expectVec((*drawables)[0].line_part().line.min(), expectedLine.line_part().line.min());
      expectVec((*drawables)[0].line_part().line.max(), expectedLine.line_part().line.max());
    };
    auto idle = Mat3F::translation({3, 0});
    for (int reuse = 0; reuse != 2; ++reuse) {
      auto sample = animator.sample(.5f);
      auto independent = animator.sample(.5f);
      expectPose(sample, idle);
      expectPose(independent, idle);
      {
        auto shared = sample;
        ASSERT_TRUE(shared.translateLocalTransformationGroup("moving", {5, 2}));
        auto translated = Mat3F::translation({5, 2}) * idle;
        expectPose(sample, translated);
        expectPose(shared, translated);
        ASSERT_TRUE(shared.rotateLocalTransformationGroup("moving", std::numbers::pi_v<float> / 2));
        expectPose(sample, Mat3F::rotation(std::numbers::pi_v<float> / 2) * translated);
        ASSERT_TRUE(shared.resetLocalTransformationGroup("moving"));
        expectPose(sample, idle);
        ASSERT_TRUE(shared.setLocalState("pose", "shifted"));
        expectPose(sample, Mat3F::translation({7, 0}));
        EXPECT_FALSE(shared.setLocalState("pose", "missing"));
        EXPECT_FALSE(shared.translateLocalTransformationGroup("missing", {5, 2}));
        EXPECT_FALSE(shared.rotateLocalTransformationGroup("missing", 1));
        EXPECT_FALSE(shared.resetLocalTransformationGroup("missing"));
        EXPECT_FALSE(shared.partTransformation("missing"));
        expectPose(sample, Mat3F::translation({7, 0}));
        ASSERT_TRUE(shared.setLocalState("pose", "badGroup"));
        EXPECT_FALSE(sample.partTransformation("child"));
        ASSERT_TRUE(shared.setLocalState("pose", "badAnchor"));
        EXPECT_FALSE(sample.partTransformation("child"));
        ASSERT_TRUE(shared.setLocalState("pose", "idle"));
        expectPose(sample, idle);
      }
      expectPose(independent, idle);
      auto logical = animator.partTransformation("child");
      auto logicalState = animator.state("pose");
      auto logicalGroup = animator.getLocalTransformationGroup("moving");
      ASSERT_TRUE(logical);
      ASSERT_TRUE(logicalState);
      ASSERT_TRUE(logicalGroup);
      expectVec(logical->transform_point(Vec2F{0, 0}), {3, 3});
      EXPECT_EQ(*logicalState, "idle");
      EXPECT_EQ(*logicalGroup, Mat3F::identity());
      animator.setZoom(2);
      animator.setFlipped(true);
      auto global = Mat3F::scaling(Vec2F{-1, 1}) * Mat3F::scaling(2.f);
      expectPose(sample, idle, global);
      expectPose(independent, idle, global);
      animator.setFlipped(false);
      animator.setZoom(1);
      // Leave both state and local group overrides behind before the slot is reused.
      ASSERT_TRUE(sample.setLocalState("pose", "shifted"));
      ASSERT_TRUE(sample.translateLocalTransformationGroup("moving", {20, 0}));
      expectPose(sample, Mat3F::translation({27, 0}));
    }
  });

const GameTestRegistration humanoidSemanticSampling(
  "HumanoidRenderTest", "LegacyAndAnimatorHumanoidsSamplePoseWithoutAdvancing", [](GameTest& fixture) {
    auto assets = fixture.root().assets();
    ASSERT_TRUE(assets);
    auto config = (*assets)->json("/humanoid.config");
    ASSERT_TRUE(config);
    auto legacy = config->erase("animation");
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
      Drawable::rotate_all(*previous, .5f);
      Drawable::rebase_all(*previous);
      ASSERT_EQ(previous->size(), middle->size());
      for (std::size_t i = 0; i != previous->size(); ++i) {
        if ((*previous)[i].is_image() && (*middle)[i].is_image()) {
          EXPECT_EQ((*previous)[i].image_part().image, (*middle)[i].image_part().image);
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
    PlayerWorldContext setup;
    auto initialized = setup.init(root);
    ASSERT_TRUE(initialized) << initialized.error().what();
    auto& player = *setup.player;
    ASSERT_TRUE(player.moveTo(Vec2F(1024, 1024)));

    struct Events : NullRenderCallback {
      unsigned particles = 0;
      unsigned audios = 0;
      float alpha = 1.0f;
      PresentationContext presentation() const override { return PresentationContext(alpha); }
      std::expected<void, StarException> add_particle(Particle&& particle) override {
        if (particle.string == "presentation-event-fixture")
          ++particles;
        return {};
      }
      std::expected<void, StarException> add_audio(std::shared_ptr<AudioInstance>) override {
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
    root.random().seed(gameplaySeed);
    RandomSource untouchedGameplay(gameplaySeed);
    for (float alpha : {0.0f, 0.25f, 0.75f, 1.0f, 0.5f}) {
      events.alpha = alpha;
      auto geometry = player.render(events);
      ASSERT_TRUE(geometry) << geometry.error().what();
      ASSERT_EQ(root.random().next_u64(), untouchedGameplay.next_u64()) << "geometry alpha=" << alpha;
      auto lighting = player.renderLightSources(events);
      ASSERT_TRUE(lighting) << lighting.error().what();
      ASSERT_EQ(root.random().next_u64(), untouchedGameplay.next_u64()) << "lighting alpha=" << alpha;
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
    auto context = (*lua)->create_context(StringList{});
    ASSERT_TRUE(context);
    auto callbacks = LuaBindings::make_movement_controller_callbacks(player.movementController());
    ASSERT_TRUE(callbacks);
    ASSERT_TRUE(context->set_callbacks("mcontroller", *callbacks));
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
    (*loop)->set_loops(-1);
    List<std::shared_ptr<AudioInstance>> retained;
    NullRenderCallback hidden(retained);
    EXPECT_FALSE((*oneShot)->position());
    ASSERT_TRUE(hidden.add_audios({*oneShot}));
    ASSERT_TRUE((*oneShot)->position());
    EXPECT_EQ(*(*oneShot)->position(), Vec2F());
    EXPECT_TRUE(retained.empty());
    ASSERT_TRUE(hidden.add_audio(*loop));
    ASSERT_EQ(retained.size(), 1u);
    EXPECT_EQ(retained[0], *loop);
    // The original instance survives so animator position/volume/stop updates
    // while hidden still affect the instance eventually submitted on reveal.
    (*loop)->set_position(Vec2F(7, 9));
    EXPECT_EQ(*retained[0]->position(), Vec2F(7, 9));
    (*loop)->stop();
  });

const GameTestRegistration arm_rotation_group_order(
  "HumanoidRenderTest", "ArmRotationPreservesGroupsOnBothSides", [](GameTest& fixture) {
    auto assets = fixture.root().assets();
    ASSERT_TRUE(assets);
    auto base = (*assets)->json("/humanoid.config");
    ASSERT_TRUE(base) << base.error().what();
    auto animation = Json::parse(R"({
      "version":1,
      "transformationGroups":{"before":{},"after":{},"frontArmRotation":{},"backArmRotation":{}},
      "animatedParts":{
        "stateTypes":{
          "frontArm":{"default":"rotation","states":{"rotation":{},"plain":{},"broken":{}}},
          "backArm":{"default":"rotation","states":{"rotation":{}}}
        },
        "parts":{
          "anchor":{"properties":{"offset":[5,7]}},
          "frontArm":{"properties":{"anchorPart":"anchor","rotationCenter":[1,2]},
            "partStates":{"frontArm":{
              "rotation":{"properties":{"transformationGroups":["before","frontArmRotation","after"]}},
              "plain":{"properties":{"transformationGroups":["before","after"]}},
              "broken":{"properties":{"transformationGroups":["before","frontArmRotation","missing"]}}
            }}},
          "backArm":{"properties":{"anchorPart":"anchor","rotationCenter":[-1,1]},
            "partStates":{"backArm":{
              "rotation":{"properties":{"transformationGroups":["before","backArmRotation","after"]}}
            }}}
        }
      }
    })");
    ASSERT_TRUE(animation) << animation.error().what();
    auto config = base->set("animation", *animation);
    ASSERT_TRUE(config) << config.error().what();
    auto made = Humanoid::create(fixture.root(), *config);
    ASSERT_TRUE(made) << made.error().what();
    auto& humanoid = **made;
    auto animator = humanoid.networkedAnimator();
    ASSERT_TRUE(animator);
    ASSERT_TRUE(animator->get().translateTransformationGroup("before", {2, 3}));
    ASSERT_TRUE(animator->get().scaleTransformationGroup("after", Vec2F(2, 3)));
    // The supplied angle replaces each arm's stored transform rather than composing with it.
    ASSERT_TRUE(animator->get().translateTransformationGroup("frontArmRotation", {100, 0}));
    ASSERT_TRUE(animator->get().translateTransformationGroup("backArmRotation", {0, 100}));

    constexpr float angle = std::numbers::pi_v<float> / 2;
    auto expect_position = [&](ToolHand hand, Direction facing, Vec2F expected) {
      auto actual = humanoid.arm_position(hand, facing, angle, {3, 4});
      ASSERT_TRUE(actual) << actual.error().what();
      EXPECT_NEAR((*actual)[0], expected[0], 1e-5f);
      EXPECT_NEAR((*actual)[1], expected[1], 1e-5f);
    };
    // Offset + pre-translation gives (5,7) relative to either center.
    // Quarter-turn to (-7,5), restore the center, scale, translate by anchor, then mirror.
    expect_position(ToolHand::Primary, Direction::Left, {7, 28});
    expect_position(ToolHand::Alt, Direction::Right, {-7, 28});
    expect_position(ToolHand::Primary, Direction::Right, {-11, 25});
    expect_position(ToolHand::Alt, Direction::Left, {11, 25});

    humanoid.setHandFrameOverrides(ToolHand::Primary, "rotation", "plain");
    expect_position(ToolHand::Alt, Direction::Right, {17, 34}); // No rotation marker.
    humanoid.setHandFrameOverrides(ToolHand::Primary, "rotation", "broken");
    EXPECT_FALSE(humanoid.arm_position(ToolHand::Alt, Direction::Right, angle, {3, 4}));
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
    std::array const smooth_groups{String("smooth")};
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
      auto smooth = sample.group_transformation(smooth_groups);
      ASSERT_TRUE(smooth);
      EXPECT_NEAR(smooth->transform_point(Vec2F{})[0], 7.2f, 1e-5f);
    }
    auto logical = animator.group_transformation(std::array{String("discrete")});
    ASSERT_TRUE(logical);
    EXPECT_FLOAT_EQ(logical->transform_point(Vec2F{})[0], 8);
    auto timer = animator.stateTimer("body");
    ASSERT_TRUE(timer);
    EXPECT_NEAR(*timer, .6f, 1e-5f);
  });

const GameTestRegistration emote_transaction(
  "HumanoidRenderTest", "EmoteTransitionsAndForcedResetAreRetryable", [](GameTest& fixture) {
    auto directory = File::temporary_directory(fixture.root().random());
    ASSERT_TRUE(directory);
    auto cleanup = make_scope_exit([&] { EXPECT_TRUE(File::remove_all(*directory)); });
    auto root = human_animation_root(fixture, *directory);
    ASSERT_TRUE(root) << root.error().what();
    auto built = Humanoid::create(**root, HumanoidIdentity{}, JsonObject{{"useAnimation", true}});
    ASSERT_TRUE(built) << built.error().what();
    auto config = (*built)->humanoidConfig(false);
    ASSERT_TRUE(config);
    // Try both targets as the broken one: iteration order is not a contract.
    for (bool break_body : {false, true}) {
      JsonObject group{{"body", JsonArray{break_body ? Json("missing") : Json("walk"), true, true}},
                       {"emote", JsonArray{break_body ? Json("happy") : Json("missing"), true, false}}};
      auto broken = config->set_path("emoteAnimations.happy", group);
      ASSERT_TRUE(broken);
      auto made = Humanoid::create(**root, *broken);
      ASSERT_TRUE(made) << made.error().what();
      auto& model = **made;
      ASSERT_TRUE(model.setIdentity(HumanoidIdentity{}));
      ASSERT_TRUE(model.setState(Humanoid::Run));
      ASSERT_TRUE(model.setEmoteState(HumanoidEmote::Laugh));
      ASSERT_TRUE(model.animate(.05f, {}));
      ASSERT_TRUE(model.animate(.06f, {}));
      auto handle = model.networkedAnimator();
      ASSERT_TRUE(handle);
      {
        auto& animator = handle->get();
        auto before = animator.sample(0.f).stateTimer("emote");
        auto after = animator.stateTimer("emote");
        auto body = animator.stateTimer("body");
        ASSERT_TRUE(before && after && body);
        EXPECT_GT(*before, 0.f);
        EXPECT_GT(*after, *before);
        for (int attempt = 0; attempt < 2; ++attempt) {
          EXPECT_FALSE(model.setEmoteState(HumanoidEmote::Happy));
          EXPECT_EQ(model.emoteState(), HumanoidEmote::Laugh);
          EXPECT_EQ(animator.state("emote").value(), "laugh");
          EXPECT_EQ(animator.state("body").value(), "run");
          EXPECT_EQ(animator.sample(0.f).stateTimer("emote").value(), *before);
          EXPECT_EQ(animator.stateTimer("emote").value(), *after);
          EXPECT_EQ(animator.stateTimer("body").value(), *body);
        }
        EXPECT_FALSE(model.setEmoteState(static_cast<HumanoidEmote>(-1)));
      }

      JsonObject repaired{{"body", JsonArray{"run", false, false}}, {"emote", JsonArray{"laugh", false, false}}};
      ASSERT_TRUE(model.setHumanoidParameters(JsonObject{{"emoteAnimations", JsonObject{{"happy", repaired}}}}));
      ASSERT_TRUE(model.loadAnimation());
      auto& animator = model.networkedAnimator().value().get();
      ASSERT_TRUE(animator.setLocalState("emote", "laugh"));
      ASSERT_TRUE(model.animate(.05f, {}));
      auto timer = animator.stateTimer("emote").value();
      ASSERT_GT(timer, 0.f);
      ASSERT_TRUE(model.setEmoteState(HumanoidEmote::Happy));
      EXPECT_EQ(model.emoteState(), HumanoidEmote::Happy);
      EXPECT_EQ(animator.stateTimer("emote").value(), timer); // Authored startNew=false.

      // A late pose error follows both state groups and local transform writes.
      // It must not finish or restart any logical cursor, or clear a transform.
      ASSERT_TRUE(animator.translateLocalTransformationGroup("headRotation", {2, 3}));
      auto transform = animator.getLocalTransformationGroup("headRotation").value();
      auto arm = animator.parts().get("frontArm");
      ASSERT_TRUE(arm);
      auto& properties = arm->get().part_properties;
      auto center = properties.get("rotationCenter").value().get();
      properties.set("rotationCenter", false);
      EXPECT_FALSE(model.resetAnimation());
      EXPECT_EQ(animator.stateTimer("emote").value(), timer);
      EXPECT_EQ(animator.getLocalTransformationGroup("headRotation").value(), transform);
      properties.set("rotationCenter", center);
      ASSERT_TRUE(model.resetAnimation());
      EXPECT_EQ(animator.sample(0.f).stateTimer("emote").value(), 0.f);
      EXPECT_EQ(animator.stateTimer("emote").value(), 0.f); // Forced restart even for the same state.
    }
  });

const GameTestRegistration humanoidTickOwnedPose(
  "HumanoidRenderTest", "TickPoseAndFreshLightSamplesDoNotDependOnPreviousRender", [](GameTest& fixture) {
    auto directory = File::temporary_directory(fixture.root().random());
    ASSERT_TRUE(directory);
    auto cleanup = make_scope_exit([&] { EXPECT_TRUE(File::remove_all(*directory)); });
    auto createdRoot = human_animation_root(fixture, *directory);
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
        ASSERT_TRUE(humanoid.prepare_animator_pose(sample));
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

const GameTestRegistration humanoid_dance_playback(
  "HumanoidRenderTest", "DancePoseItemsTimingAndReloadOnBothRenderers", [](GameTest& fixture) {
    auto directory = File::temporary_directory(fixture.root().random());
    ASSERT_TRUE(directory);
    auto cleanup = make_scope_exit([&] { EXPECT_TRUE(File::remove_all(*directory)); });
    auto overlay = File::relative_to(*directory, "assets");
    ASSERT_TRUE(File::create_directories(overlay));
    auto dance_path = File::relative_to(overlay, "renderprobe.dance");
    ASSERT_TRUE(File::write_file(String(R"({
      "name":"renderprobe","states":["idle"],"cycle":1,"duration":2,"cyclic":false,
      "steps":[{"bodyFrame":"idle.2","frontArmFrame":"idle.3","backArmFrame":"rotation?flipx",
                "headOffset":[5,-3],"frontArmOffset":[2,3],"backArmOffset":[-3,2],
                "frontArmRotation":0.5,"backArmRotation":-0.25},{}]
    })"), dance_path));
    ASSERT_TRUE(File::write_file(String(R"({
      "name":"emptyprobe","states":["idle"],"cycle":1,"duration":1,"cyclic":true,"steps":[]
    })"), File::relative_to(overlay, "emptyprobe.dance")));
    ASSERT_TRUE(File::write_file(String(R"({
      "name":"loopprobe","states":["idle"],"cycle":1,"duration":4,"cyclic":true,
      "steps":[{"bodyFrame":"idle.2"},{"bodyFrame":"idle.3"}]
    })"), File::relative_to(overlay, "loopprobe.dance")));
    auto root = human_animation_root(fixture, *directory);
    ASSERT_TRUE(root) << root.error().what();
    auto metadata = (*root)->imageMetadataDatabase();
    ASSERT_TRUE(metadata);
    HumanoidIdentity identity;
    identity.personality.headOffset = {1, 2};
    std::array<std::shared_ptr<Humanoid>, 2> models;
    auto find_sprite = [](List<Drawable> const& drawables, String const& path)
        -> std::optional<std::reference_wrapper<Drawable::ImagePart const>> {
      for (auto const& drawable : drawables)
        if (drawable.is_image() && drawable.image_part().image.base_path == path)
          return std::cref(drawable.image_part());
      return {};
    };
    auto check_pose = [&](List<Drawable> const& drawables, String const& body_frame, bool held) {
      auto body = find_sprite(drawables, "/humanoid/human/malebody.png");
      ASSERT_TRUE(body);
      EXPECT_EQ(body->get().image.sub_path, body_frame);
      EXPECT_EQ(std::ranges::count_if(drawables, &Drawable::is_line), held ? 2 : 0);
    };
    auto near = [](Vec2F actual, Vec2F expected) {
      EXPECT_NEAR(actual[0], expected[0], 1e-5f);
      EXPECT_NEAR(actual[1], expected[1], 1e-5f);
    };
    for (std::size_t i = 0; i != models.size(); ++i) {
      bool animated = i != 0;
      SCOPED_TRACE(animated ? "animator" : "legacy");
      auto made = Humanoid::create(**root, identity, JsonObject{{"useAnimation", animated}});
      ASSERT_TRUE(made) << made.error().what();
      models[i] = std::move(*made);
      auto& model = *models[i];
      auto config = model.humanoidConfig(false);
      ASSERT_TRUE(config);
      EXPECT_EQ(config->contains("animation"), animated);
      model.setFacingDirection(Direction::Right);
      for (auto hand : {ToolHand::Primary, ToolHand::Alt}) {
        model.setHandFrameOverrides(hand, "rotation", "rotation");
        model.setHandParameters(hand, true, 0, 0, false, false, false);
        model.setHandDrawables(hand, {Drawable::make_line(Line2F({0, 0}, {1, 0}), 1, Color::palette_white)});
      }
      auto idle = model.render();
      ASSERT_TRUE(idle) << idle.error().what();
      check_pose(*idle, "idle.1", true);
      auto idle_head = find_sprite(*idle, "/humanoid/human/malehead.png");
      ASSERT_TRUE(idle_head);

      ASSERT_TRUE(model.set_dance("renderprobe"));
      ASSERT_TRUE(model.animate(.25f, {}));
      auto animator = model.networkedAnimator();
      if (animated) {
        ASSERT_TRUE(animator);
        EXPECT_EQ(animator->get().state("bodyDance"), "idle.2"); // exact authored state
        EXPECT_EQ(animator->get().state("backArmDance"), "dance"); // custom frame tag
      }
      ASSERT_TRUE(model.animate(.75f, {})); // presentation spans [.25, 1]
      for (float alpha : {.25f, .75f, .25f}) {
        auto shown = model.render(true, true, alpha);
        ASSERT_TRUE(shown) << shown.error().what();
        bool first = alpha == .25f;
        check_pose(*shown, first ? "idle.2" : "idle.1", false);
        auto head = find_sprite(*shown, "/humanoid/human/malehead.png");
        ASSERT_TRUE(head);
        auto delta = head->get().transformation.transform_point(Vec2F{}) - idle_head->get().transformation.transform_point(Vec2F{});
        near(delta, ((first ? Vec2F(5, -3) : Vec2F{}) - identity.personality.headOffset) / tile_pixels);
        if (first) {
          auto body = find_sprite(*shown, "/humanoid/human/malebody.png");
          ASSERT_TRUE(body);
          auto body_size = (*metadata)->imageSize(body->get().image);
          ASSERT_TRUE(body_size);
          auto body_center = body->get().transformation.transform_point(Vec2F(*body_size) / 2);
          for (bool front : {false, true}) {
            auto arm = find_sprite(*shown, front ? String("/humanoid/human/frontarm.png") : String("/humanoid/human/backarm.png"));
            ASSERT_TRUE(arm);
            EXPECT_EQ(arm->get().image.sub_path, front ? String("idle.3") : String("rotation"));
            auto const& matrix = arm->get().transformation;
            float rotation = front ? .5f : -.25f;
            near(matrix.transform_direction(Vec2F(1, 0)), Vec2F(1.f / tile_pixels, 0).rotate(rotation));
            auto arm_size = (*metadata)->imageSize(arm->get().image);
            ASSERT_TRUE(arm_size);
            // The offset is translated before the arm is rotated about the origin.
            auto offset = (front ? Vec2F(2, 3) : Vec2F(-3, 2)) / tile_pixels;
            near(matrix.transform_point(Vec2F(*arm_size) / 2) - body_center, offset.rotate(rotation));
          }
        }
        if (animated) {
          auto sample = animator->get().sample(alpha);
          ASSERT_TRUE(model.prepare_animator_pose(sample));
          EXPECT_EQ(sample.applyPartTags("body", "<bodyDanceFrame>|<frontArmDanceFrame>|<backArmDanceFrame>"),
                    first ? String("idle.2|idle.3|rotation?flipx") : String("default|default|default"));
          EXPECT_EQ(sample.state("bodyDance"), first ? String("idle.2") : String("idle"));
          EXPECT_EQ(sample.state("frontArmDance"), first ? String("idle.3") : String("idle"));
          EXPECT_EQ(sample.state("backArmDance"), first ? String("dance") : String("idle"));
          EXPECT_EQ(animator->get().state("bodyDance"), "idle");
        }
      }
      ASSERT_TRUE(model.set_dance("renderprobe")); // same name preserves phase
      ASSERT_TRUE(model.animate(1.f, {}));
      EXPECT_EQ(model.dance_cyclic_or_ended(), false); // duration, not cycle
      ASSERT_TRUE(model.animate(.25f, {}));
      EXPECT_EQ(model.dance_cyclic_or_ended(), true);
      auto final_pose = model.render();
      ASSERT_TRUE(final_pose);
      check_pose(*final_pose, "idle.1", false); // last step, not wrapped
      ASSERT_TRUE(model.setState(Humanoid::Duck));
      ASSERT_TRUE(model.animate(0.f, {}));
      auto filtered = model.render();
      ASSERT_TRUE(filtered);
      check_pose(*filtered, "duck.1", true);
      ASSERT_TRUE(model.setState(Humanoid::Idle));
      ASSERT_TRUE(model.set_dance("emptyprobe"));
      ASSERT_TRUE(model.animate(0.f, {}));
      auto empty = model.render();
      ASSERT_TRUE(empty);
      check_pose(*empty, "idle.1", true);
      ASSERT_TRUE(model.set_dance("loopprobe"));
      ASSERT_TRUE(model.animate(.5f, {}));
      auto second = model.render();
      ASSERT_TRUE(second);
      check_pose(*second, "idle.3", false);
      ASSERT_TRUE(model.animate(.5f, {}));
      auto wrapped = model.render();
      ASSERT_TRUE(wrapped);
      check_pose(*wrapped, "idle.2", false);
      ASSERT_TRUE(model.set_dance("renderprobe"));
      ASSERT_TRUE(model.animate(.25f, {}));
    }
    // The selected name stays unchanged across a Root reload. Both models have
    // already cached and played the old definition; do not call set_dance again.
    ASSERT_TRUE(File::write_file(String(R"({
      "name":"renderprobe","states":["idle"],"cycle":1,"duration":2,"cyclic":false,
      "steps":[{"bodyFrame":"idle.4"}]
    })"), dance_path));
    ASSERT_TRUE((*root)->reload());
    for (auto& model : models) {
      ASSERT_TRUE(model->animate(0.f, {}));
      auto replaced = model->render();
      ASSERT_TRUE(replaced) << replaced.error().what();
      check_pose(*replaced, "idle.4", false);
      ASSERT_TRUE(model->set_dance({}));
      auto stopped = model->render();
      ASSERT_TRUE(stopped);
      check_pose(*stopped, "idle.1", true);
    }
  });

#include "gtest/gtest.h"

import std;
import star.actor_movement_controller;
import star.animated_part_set;
import star.anchorable_entity;
import star.color;
import star.algorithm;
import star.armor_wearer;
import star.armors;
import star.asset_path;
import star.byte_array;
import star.assets;
import star.casting;
import star.client_context;
import star.damage;
import star.drawable;
import star.file;
import star.entity;
import star.entity_rendering;
import star.entity_rendering_types;
import star.format;
import star.force_regions;
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
import star.interaction_types;
import star.inspectable_entity;
import star.interactive_entity;
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
import star.object;
import star.object_database;
import star.physics_entity;
import star.player;
import star.player_inventory;
import star.inventory_types;
import star.poly;
import star.player_factory;
import star.player_lua_bindings;
import star.quest_descriptor;
import star.quest_manager;
import star.quests;
import star.rect;
import star.root;
import star.random;
import star.string;
import star.string_view;
import star.liquid_types;
import star.material_types;
import star.collision_block;
import star.tool_user;
import star.type_identity;
import star.uuid;
import star.vector;
import star.world_server;
import star.world_tiles;

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
  EXPECT_EQ(blank.mat_id, null_material_id);
  EXPECT_EQ(blank.liq_id, empty_liquid_id);
  EXPECT_EQ(blank.hue_shift, 0);
  EXPECT_EQ(blank.color_variant, default_material_color_variant);
  PreviewTile liquid(Vec2I(2, 3), LiquidId{5});
  EXPECT_TRUE(liquid.foreground);
  EXPECT_EQ(liquid.liq_id, 5);
  EXPECT_EQ(liquid.mat_id, null_material_id);

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
      EXPECT_TRUE(registered ? world->removeEntity(player->entityId(), false) : player->uninit());
    player.reset();
    client.reset();
    if (world)
      EXPECT_TRUE(world->close());
  }

  std::expected<void, StarException> init(Root& root, std::optional<Vec2F> indexed_position = {}) {
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
    if (auto finalized = player->finalize_creation(); !finalized)
      return std::unexpected(std::move(finalized.error()));
    auto uuid = player->uuid();
    if (!uuid)
      return std::unexpected(std::move(uuid.error()));
    auto connected = ClientContext::create(root.logger(), Uuid(root.random()), *uuid, 1, {});
    if (!connected)
      return std::unexpected(std::move(connected.error()));
    client = std::move(*connected);
    player->set_client_context(client);
    if (indexed_position)
      player->movement_controller().set_position(*indexed_position);
    auto entered = indexed_position ? world->addEntity(player) : player->init(*world, 100, EntityMode::Master);
    if (!entered)
      return std::unexpected(std::move(entered.error()));
    registered = indexed_position.has_value();
    initialized = true;
    return player->refresh_humanoid();
  }

  std::shared_ptr<File> storage;
  std::shared_ptr<WorldServer> world;
  std::shared_ptr<PlayerFactory const> factory;
  std::shared_ptr<ClientContext> client;
  std::shared_ptr<Player> player;
  bool initialized = false;
  bool registered = false;
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

namespace {
std::expected<std::unique_ptr<Root>, StarException> headless_player_root(GameTest& fixture, String const& directory) {
  auto overlay = File::relative_to(directory, "assets");
  // Standalone WorldServer fixtures have no client-only input callbacks.
  auto patch = File::write_file(String(R"([{"op":"replace","path":"/genericScriptContexts","value":{}}])"),
      File::relative_to(overlay, "player.config.patch"));
  if (!patch)
    return std::unexpected(std::move(patch.error()));
  // Skip the vanilla tutorial, which automatically seats the player at a nearby object.
  auto quests = File::relative_to(overlay, "quests");
  if (auto made = File::create_directory(quests); !made)
    return std::unexpected(std::move(made.error()));
  auto initial_quests = File::write_file(String(R"([{"op":"replace","path":"/initialquests/human","value":[]}])"),
      File::relative_to(quests, "quests.config.patch"));
  if (!initial_quests)
    return std::unexpected(std::move(initial_quests.error()));
  auto settings = fixture.rootSettings();
  settings.assetSources.push_back(overlay);
  settings.storageDirectory = File::relative_to(directory, "storage");
  settings.runtimeConfigFile = {};
  settings.logDirectory = {};
  settings.logFile = {};
  settings.includeUGC = false;
  return Root::create(std::move(settings));
}

struct UseTarget final : InteractiveEntity, PhysicsEntity {
  UseTarget(Root& root, Player& player, Vec2F position)
    : Entity(root), player(player), target_position(position) {}

  void query_type(TypeQuery& query) const noexcept override {
    query_type_of<UseTarget, InteractiveEntity, PhysicsEntity>(*this, query);
  }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<UseTarget>(); }
  EntityType entity_type() const override { return EntityType::Object; }
  Vec2F position() const override { return target_position; }
  RectF meta_bound_box() const override { return {-2, -2, 2, 2}; }

  void press() const {
    player.end_trigger();
    player.begin_trigger();
  }

  std::expected<List<PhysicsForceRegion>, StarException> force_regions() const override {
    if (std::exchange(press_during_movement, false))
      press();
    if (std::exchange(fail_movement, false))
      return std::unexpected(StarException("before use dispatch"));
    return List<PhysicsForceRegion>{};
  }

  std::expected<InteractAction, StarException> interact(InteractRequest const& request) override {
    ++deliveries;
    if (std::exchange(press_during_interaction, false))
      press();
    if (std::exchange(fail_interaction, false))
      return std::unexpected(StarException("after use dispatch"));
    return InteractAction{InteractActionType::Message, request.target_id, request.source_id};
  }

  Player& player;
  Vec2F target_position;
  mutable bool press_during_movement = false;
  mutable bool fail_movement = false;
  bool press_during_interaction = false;
  bool fail_interaction = false;
  unsigned deliveries = 0;
};

const GameTestRegistration use_edges("PlayerUse", "EdgeRetriesAndDispatchesOnce", [](GameTest& fixture) {
  auto directory = File::temporary_directory(fixture.root().random());
  if (!fixture.check(bool(directory), "create private use fixture directory")) return;
  auto cleanup = make_scope_exit([&] { fixture.check(bool(File::remove_all(*directory)), "remove private use fixture"); });
  if (!fixture.check(bool(File::create_directory(File::relative_to(*directory, "assets"))), "create use asset overlay")) return;
  auto root = headless_player_root(fixture, *directory);
  if (!fixture.check(bool(root), root ? "create private asset root" : root.error().what())) return;
  PlayerWorldContext setup;
  auto initialized = setup.init(**root);
  if (!initialized) {
    fixture.check(false, initialized.error().what());
    return;
  }
  auto& player = *setup.player;
  auto& movement = player.movement_controller();
  ActorMovementParameters parameters;
  parameters.gravity_enabled = false;
  parameters.collision_enabled = false;
  if (!fixture.check(bool(movement.update_base_parameters(parameters)), "configure movement")) return;
  movement.set_position({1024, 1024});
  player.aim(player.position());

  auto target = std::make_shared<UseTarget>(**root, player, player.position());
  if (!fixture.check(bool(setup.world->addEntity(target, 101)), "insert use target")) return;
  auto remove = make_scope_exit([&] { fixture.check(bool(setup.world->removeEntity(target->entityId(), false)), "remove use target"); });

  std::uint64_t step = 0;
  auto tick = [&](unsigned deliveries, std::string_view failure = {},
      std::source_location at = std::source_location::current()) {
    auto result = player.update(1.0f / 60, ++step);
    if (failure.empty()) {
      if (!result)
        return fixture.check(false, result.error().what(), at);
    } else if (!fixture.check(!result && std::string_view(result.error().what()).contains(failure), failure, at)) {
      return false;
    }
    return fixture.check(target->deliveries == deliveries, "interaction delivery count", at);
  };

  // A press from movement is too late for this tick, and holding it does not repeat.
  target->press_during_movement = true;
  if (!tick(0) || !tick(1)) return;
  player.begin_trigger();
  if (!tick(1)) return;

  // Neither the retry nor the later press from the failed tick may be lost.
  target->press();
  target->press_during_movement = true;
  target->fail_movement = true;
  if (!tick(1, "before use dispatch") || !tick(2) || !tick(3) || !tick(3)) return;

  // A failing handler has already received its press but can enqueue another one.
  target->press();
  target->press_during_interaction = true;
  target->fail_interaction = true;
  if (!tick(4, "after use dispatch") || !tick(5) || !tick(5)) return;

  auto actions = player.pull_interact_actions();
  if (!fixture.check(actions && actions->size() == 4, "four successful interactions returned exactly four actions")) return;
  for (auto const& action : *actions)
    fixture.check(action.type == InteractActionType::Message && action.entity_id == target->entityId()
        && action.data == Json(player.entityId()), "action preserves source and target");
});

const GameTestRegistration quest_focus("PlayerQuestFocus", "RetainsPublishesAndSnapshotsHighlights", [](GameTest& fixture) {
  auto directory = File::temporary_directory(fixture.root().random());
  if (!fixture.check(bool(directory), "create private quest fixture directory")) return;
  auto cleanup = make_scope_exit([&] { fixture.check(bool(File::remove_all(*directory)), "remove private quest fixture"); });
  auto overlay = File::relative_to(*directory, "assets");
  if (!fixture.check(bool(File::create_directory(overlay)), "create quest asset overlay")) return;
  if (!fixture.check(bool(File::write_file(String(R"LUA(
    function init()
      message.setHandler("interestingObjects", function()
        local mode = player.getProperty("zzFocusMode")
        if mode == "clear" then return jarray() end
        local template = quest.templateId()
        if template == "zz-focus-peer" then return {"peer"} end
        if template == "zz-focus-replacement" then return {"replacement"} end
        if mode == "invalid" then return {"replacement", false} end
        if mode == "repaired" then return {"replacement"} end
        if mode == "replaceQuest" then
          assert(player.startQuest({quests = {{
            questId = quest.questId(), templateId = "zz-focus-replacement", parameters = jobject()
          }}}) == "zz-focus-driver")
        end
        return {"original"}
      end)
    end
  )LUA"), File::relative_to(overlay, "zz-focus.lua"))), "write quest message handler")) return;
  for (auto const& id : {String("zz-focus-original"), String("zz-focus-peer"), String("zz-focus-replacement")}) {
    auto config = Json(JsonObject{{"id", id}, {"script", "/zz-focus.lua"}, {"showAcceptDialog", false}}).print_json();
    if (!fixture.check(config && File::write_file(*config, File::relative_to(overlay, id + ".questtemplate")),
        "write quest template")) return;
  }
  auto root = headless_player_root(fixture, *directory);
  if (!fixture.check(bool(root), root ? "create private asset root" : root.error().what())) return;
  PlayerWorldContext setup;
  auto initialized = setup.init(**root, Vec2F(1024, 1024));
  if (!initialized) {
    fixture.check(false, initialized.error().what());
    return;
  }
  auto& player = *setup.player;
  auto& movement = player.movement_controller();
  ActorMovementParameters movement_parameters;
  movement_parameters.gravity_enabled = false;
  movement_parameters.collision_enabled = false;
  if (!fixture.check(bool(movement.update_base_parameters(movement_parameters)), "configure stationary player")) return;
  player.aim({1030, 1024});

  auto item_database = (*root)->itemDatabase();
  auto scanner_descriptor = ItemDescriptor::create("scanmode", 1, JsonObject{
      {"requireLineOfSight", false}, {"inspectionAngles", JsonArray{3.2, 3.3}}});
  if (!fixture.check(item_database && scanner_descriptor, "resolve scan tool descriptor")) return;
  auto scanner = (*item_database)->item(*scanner_descriptor);
  if (!fixture.check(scanner && *scanner, "create scan tool")) return;
  auto inventory = player.inventory();
  inventory->setEssentialItem(EssentialItem::InspectionTool, std::move(*scanner));
  if (!fixture.check(bool(inventory->select_action_bar_location(EssentialItem::InspectionTool)), "select scan tool")) return;

  auto object_database = (*root)->object_database();
  if (!fixture.check(bool(object_database), "load object assets")) return;
  constexpr std::array<std::string_view, 3> probe_names{"original", "replacement", "peer"};
  std::array<std::shared_ptr<InspectableEntity>, probe_names.size()> probes;
  std::array<EntityId, probe_names.size()> probe_ids{};
  auto& world = *setup.world;
  auto remove_probes = make_scope_exit([&] {
    for (auto id : probe_ids)
      if (id != null_entity_id)
        fixture.check(bool(world.removeEntity(id, false)), "remove inspectable object");
  });
  Vec2I const first_probe(1027, 1024);
  if (!fixture.check(bool(world.generateRegion(RectI::with_size(first_probe - Vec2I(4, 4), {17, 8}))),
      "generate scan probe region")) return;
  for (int x = -2; x <= 9; ++x) {
    auto floor = world.modifyServerTile(first_probe + Vec2I(x, -1));
    if (!fixture.check(floor && *floor, "load support tile under scan probe")) return;
    (*floor)->get().foreground = structure_material_id;
    (*floor)->get().update_collision(CollisionKind::Block);
  }
  for (std::size_t i = 0; i < probes.size(); ++i) {
    auto object = (*object_database)->create_object("woodenchair", JsonObject{{"inspectionLogName", String(probe_names[i])}});
    if (!fixture.check(object && *object && (*object)->set_tile_position(first_probe + Vec2I(3 * static_cast<int>(i), 0))
        && world.addEntity(*object), "place inspectable object")) return;
    probe_ids[i] = (*object)->entityId();
    if (!fixture.check((*object)->inspectable(), "placed object is scannable")
        || !fixture.check(!(*object)->spaces().empty(), "placed object has an anchored orientation")) return;
    probes[i] = std::move(*object);
  }

  std::uint64_t step = 0;
  auto warm = player.update(1.0f / 60, ++step);
  if (!fixture.check(bool(warm), warm ? "equip the scanner" : warm.error().what())
      || !fixture.check(player.inspecting() && player.can_use_tool() && !player.lounging_in(),
          "standing player can use the scanner")) return;
  for (std::size_t i = 0; i < probes.size(); ++i) {
    auto name = probes[i]->inspection_log_name();
    if (!fixture.check(name && *name && **name == String(probe_names[i]), "scan probe has its authored log name")) return;
    auto highlight = player.inspection_highlight(probes[i]);
    if (!fixture.check(bool(highlight), highlight ? "read baseline scan highlight" : highlight.error().what())
        || !fixture.check(highlight->level > 0 && highlight->type == EntityHighlightEffectType::Inspectable,
            std::format("baseline scan for {} has level {} and type {}", probe_names[i], highlight->level,
                std::to_underlying(highlight->type)))) return;
  }

  auto manager = player.quest_manager();
  auto offer = [&](String id, String template_id) -> std::expected<void, StarException> {
    auto arc = QuestArcDescriptor::fromJson(**root, JsonObject{{"quests", JsonArray{JsonObject{
        {"questId", std::move(id)}, {"templateId", std::move(template_id)}, {"parameters", JsonObject{}}}}}});
    if (!arc)
      return std::unexpected(std::move(arc.error()));
    auto quest = Quest::create(**root, *arc, 0, player);
    if (!quest)
      return std::unexpected(std::move(quest.error()));
    return manager->offer(*quest);
  };
  auto offered = offer("zz-focus-driver", "zz-focus-original").and_then([&] {
    return offer("zz-focus-peer", "zz-focus-peer");
  });
  if (!offered) {
    fixture.check(false, offered.error().what());
    return;
  }

  auto check_tick = [&](String const& mode, std::array<bool, probe_names.size()> interesting, bool bad = false,
      std::source_location at = std::source_location::current()) {
    player.set_generic_property("zzFocusMode", mode);
    auto result = player.update(1.0f / 60, ++step);
    if (bad) {
      if (!fixture.check(!result && result.error().is<JsonException>(),
          result ? "expected malformed quest focus to fail" : result.error().what(), at)) return false;
    } else if (!fixture.check(bool(result), result ? "publish quest focus" : result.error().what(), at)) {
      return false;
    }
    for (std::size_t i = 0; i < probes.size(); ++i) {
      auto highlight = player.inspection_highlight(probes[i]);
      auto expected = interesting[i] ? EntityHighlightEffectType::Interesting : EntityHighlightEffectType::Inspectable;
      if (!fixture.check(bool(highlight), highlight ? "read scan highlight" : highlight.error().what(), at)
          || !fixture.check(highlight->level > 0 && highlight->type == expected,
              std::format("{}: {} highlight type {}, expected {} (level {})", mode.utf8(), probe_names[i],
                  std::to_underlying(highlight->type), std::to_underlying(expected), highlight->level), at))
        return false;
    }
    return true;
  };
  if (!check_tick("initial", {true, false, true})
      || !check_tick("invalid", {true, false, true}, true)
      || !check_tick("repaired", {false, true, true})
      || !check_tick("clear", {false, false, false})) return;

  // The callback replaces its own quest while the pass holds the old quest.
  // Its old result and the peer's result both land; the replacement waits a tick.
  if (!check_tick("replaceQuest", {true, false, true})) return;
  auto replacement = manager->getQuest("zz-focus-driver");
  if (!fixture.check(replacement && (*replacement)->templateId() == "zz-focus-replacement", "Lua replaced the active quest")) return;
  check_tick("replaceQuest", {false, true, true});
});
}

const GameTestRegistration handheld_light_units(
  "LightSourceTest", "EquippedToolsKeepTheirAuthoredOffsetUnits", [](GameTest& fixture) {
    PlayerWorldContext setup;
    auto& root = fixture.root();
    if (!fixture.check(bool(setup.init(root)), "initialize player world")) return;
    auto& player = *setup.player;
    if (!fixture.check(bool(player.move_to(Vec2F(100, 100))), "position player")) return;
    player.aim(player.position() + Vec2F(20, 0));
    auto tools = ToolUser::create(root);
    if (!fixture.check(tools && (*tools)->init(player), "initialize tool hands")) return;
    auto cleanup = make_scope_exit([&] { fixture.check(bool((*tools)->uninit()), "release tool hands"); });
    auto database = root.itemDatabase();
    if (!fixture.check(bool(database), "load item database")) return;

    using ToolScale = std::pair<std::string_view, float>;
    for (auto const& [name, units_per_tile] : {ToolScale{"flashlight", tile_pixels}, ToolScale{"scanmode", 1.f}}) {
      auto descriptor = ItemDescriptor::create(name, 1, JsonObject{
        {"handPosition", JsonArray{1, 2}}, {"lightPosition", JsonArray{5, 3}},
        {"beamLevel", .75}, {"beamAmbience", .125}});
      if (!fixture.check(bool(descriptor), "create tool descriptor")) return;
      auto item = (*database)->item(*descriptor);
      if (!fixture.check(item && (*tools)->setItems(*item, {}), "equip tool")) return;
      List<LightSource> lights;
      if (!fixture.check(bool((*tools)->append_light_sources(lights)) && lights.size() == 1, name)) return;
      auto expected_hand = player.hand_position(ToolHand::Primary, Vec2F(4, 1) / units_per_tile);
      if (!fixture.check(bool(expected_hand), "resolve hand anchor")) return;
      auto const& light = lights.front();
      fixture.check(light.position == player.position() + *expected_hand, name);
      fixture.check(light.type == LightType::Point && light.point_beam == .75f &&
          light.beam_ambience == .125f && light.beam_angle == 0.f, "keep authored beam and aim");
    }
  });

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
    auto callbacks = LuaBindings::makeStatusControllerCallbacks(player.status_controller());
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
    auto pending = player.self_damage_notifications();
    ASSERT_TRUE(pending) << pending.error().what();
    ASSERT_EQ(pending->size(), 1u);
    EXPECT_EQ(pending->front().to_json(), *observed);
    auto drained = player.self_damage_notifications();
    ASSERT_TRUE(drained) << drained.error().what();
    EXPECT_TRUE(drained->empty());

    auto request = DamageRequest::create(JsonObject{{"damage", 3}});
    ASSERT_TRUE(request) << request.error().what();
    ASSERT_TRUE(player.hit_other(41, *request));
    ASSERT_TRUE(player.hit_other(42, *request));
    ASSERT_TRUE(player.damaged_other(pending->front()));
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

const GameTestRegistration armor_retry("ArmorWearerTest", "BadLegacyCosmeticCanRetrySameRevision", [](GameTest& fixture) {
  PlayerWorldContext setup;
  if (!fixture.check(bool(setup.init(fixture.root())), "create player world")) return;
  auto& sender = *setup.player;
  NetCompatibilityRules rules(legacy_version);
  auto initial = sender.net_store(rules);
  if (!fixture.check(bool(initial), "serialize master")) return;
  auto replica = setup.factory->net_load_player(*initial, rules);
  if (!fixture.check(replica && (*replica)->init(*setup.world, 101, EntityMode::Slave), "initialize legacy replica")) return;
  auto cleanup = make_scope_exit([&] { fixture.check(bool((*replica)->uninit()), "release replica"); });
  std::uint64_t cursor = 0;
  auto sync = [&]() -> std::expected<void, StarException> {
    auto packet = sender.write_net_state(cursor, rules);
    if (!packet)
      return std::unexpected(std::move(packet.error()));
    cursor = packet->second;
    return (*replica)->read_net_state(packet->first, 0, rules);
  };
  auto database = fixture.root().itemDatabase();
  if (!fixture.check(bool(database), "load catalog")) return;
  String armor;
  for (auto const& name : (*database)->all_items()) {
    if ((*database)->item_type(name) == ItemType::HeadArmor) {
      armor = name;
      break;
    }
  }
  if (!fixture.check(!armor.empty(), "find real head armor")) return;
  auto descriptor = ItemDescriptor::create(armor);
  if (!fixture.check(bool(descriptor), "create armor descriptor")) return;
  auto item = (*database)->item(*descriptor);
  if (!fixture.check(item && *item, "instantiate head armor")) return;
  auto stored = (*database)->disk_store(*item);
  if (!fixture.check(stored && sender.set_secret_property("armorWearer.replicating", true)
      && sender.set_secret_property("armorWearer.cosmetic1.version", 1)
      && sender.set_secret_property("armorWearer.cosmetic1.data", *stored)
      && sync(), "apply first cosmetic packet")) return;
  auto read_slot = [&] { return (*replica)->inventory()->itemsAt(EquipmentSlot::Cosmetic1); };
  auto previous = read_slot();
  if (!fixture.check(previous && *previous, "replica received armor")) return;
  if (!fixture.check(sender.set_secret_property("armorWearer.cosmetic1.version", 2)
      && sender.set_secret_property("armorWearer.cosmetic1.data", JsonObject{{"bad", true}}), "set bad save")) return;
  fixture.check(!sync(), "bad cosmetic save rejected");
  fixture.check(read_slot() == *previous, "bad save preserves previous cosmetic");
  if (!fixture.check(sender.set_secret_property("armorWearer.cosmetic1.data", Json()) && sync(), "retry same revision")) return;
  fixture.check(read_slot() == nullptr, "corrected revision clears cosmetic");
});

const GameTestRegistration player_lounge_cosmetics(
  "HumanoidRenderTest", "PlayerLoungeCosmeticsRestoreAndKeepEmitting", [](GameTest& fixture) {
    auto directory = File::temporary_directory(fixture.root().random());
    if (!fixture.check(bool(directory), "create an isolated lounge asset directory")) return;
    auto cleanup = make_scope_exit([&] { fixture.check(bool(File::remove_all(*directory)), "remove lounge assets"); });
    auto overlay = File::relative_to(*directory, "assets");
    if (!fixture.check(bool(File::create_directory(overlay)), "create lounge asset overlay")) return;
    // The headless WorldServer has no input callbacks for generic client scripts.
    if (!fixture.check(bool(File::write_file(String(R"([
          {"op":"replace","path":"/genericScriptContexts","value":{}}
        ])"), File::relative_to(overlay, "player.config.patch"))), "exclude client scripts from lounge fixture")) return;
    // The Protectorate tutorial automatically seats the player in the nearest chair.
    auto quests = File::relative_to(overlay, "quests");
    if (!fixture.check(bool(File::create_directory(quests))
          && File::write_file(String(R"([
            {"op":"replace","path":"/initialquests/human","value":[]}
          ])"), File::relative_to(quests, "quests.config.patch")), "exclude the auto-seating tutorial")) return;
    if (!fixture.check(bool(File::write_file(String(R"({
          "kind":"player-lounge-probe",
          "definition":{"duration":0.001,"particles":[["player-lounge-probe"]],
                        "start":{"particles":[["player-lounge-probe"]]}}
        })"), File::relative_to(overlay, "lounge.effectsource")))
        && File::write_file(String(R"({
          "kind":"player-lounge-probe","definition":{"type":"text","text":"player-lounge-probe"}
        })"), File::relative_to(overlay, "lounge.particle")), "write a looping lounge effect")) return;
    auto settings = fixture.rootSettings();
    settings.assetSources.push_back(overlay);
    settings.storageDirectory = File::relative_to(*directory, "storage");
    settings.runtimeConfigFile = {};
    settings.logDirectory = {};
    settings.logFile = {};
    settings.includeUGC = false;
    auto root = Root::create(std::move(settings));
    if (!fixture.check(bool(root), root ? "create lounge root" : root.error().what())) return;

    Vec2I first_seat(1024, 1024);
    PlayerWorldContext setup;
    auto initialized = setup.init(**root, Vec2F(first_seat) + Vec2F(0, 2));
    if (!fixture.check(bool(initialized), initialized ? "initialize player" : initialized.error().what())) return;
    auto& player = *setup.player;
    auto& world = *setup.world;
    if (!fixture.check(bool(world.generateRegion(RectI::with_size(first_seat - Vec2I(4, 4), {24, 16}))),
          "generate chair region")) return;
    for (int x = -2; x <= 14; ++x) {
      auto floor = world.modifyServerTile(first_seat + Vec2I(x, -1));
      if (!fixture.check(floor && *floor, "load support tile under chair")) return;
      (*floor)->get().foreground = structure_material_id;
      (*floor)->get().update_collision(CollisionKind::Block);
    }
    auto items = (*root)->itemDatabase();
    auto objects = (*root)->object_database();
    if (!fixture.check(items && objects, "load armor and chair catalogs")) return;
    String const inventory_marker = "lounge-inventory-head";
    String const override_marker = "lounge-override-head";
    auto descriptor = [](String const& marker) {
      return ItemDescriptor::create("apextier1head", 1,
        JsonObject{{"humanoidConfig", JsonObject{{"deathParticles", marker}}},
                   {"hideInVanillaSlots", false}, {"bypassNude", true},
                   {"effectSources", JsonArray{"player-lounge-probe"}}});
    };
    auto inventory_descriptor = descriptor(inventory_marker);
    auto override_descriptor = descriptor(override_marker);
    if (!fixture.check(inventory_descriptor && override_descriptor, "create cosmetic head descriptions")) return;
    auto inventory_armor = (*items)->item(*inventory_descriptor);
    if (!fixture.check(inventory_armor && *inventory_armor
          && player.inventory()->setItem(EquipmentSlot::HeadCosmetic, *inventory_armor) == true,
          "equip the inventory cosmetic")) return;

    std::array<JsonObject, 4> overrides{{
      {{"head", override_descriptor->to_json()}}, {}, {{"unrelated", false}}, {{"head", "apextier1chest"}}
    }};
    std::array<EntityId, overrides.size()> seats{};
    auto remove_chairs = make_scope_exit([&] {
      for (auto id : seats)
        if (id != null_entity_id)
          fixture.check(bool(world.removeEntity(id, false)), "remove test chair");
    });
    for (std::size_t i = 0; i != overrides.size(); ++i) {
      auto chair = (*objects)->create_object("woodenchair", JsonObject{
        {"sitArmorCosmeticOverrides", std::move(overrides[i])},
        {"sitEffectEmitters", JsonArray{"player-lounge-probe"}},
        {"sitStatusEffects", JsonArray{JsonObject{{"stat", "loungeProbe"}, {"amount", 2}}}}
      });
      if (!fixture.check(chair && *chair && (*chair)->set_tile_position(first_seat + Vec2I(4 * static_cast<int>(i), 0))
            && world.addEntity(*chair), "place a real loungeable chair")) return;
      seats[i] = (*chair)->entityId();
    }

    struct Events : NullRenderCallback {
      unsigned particles = 0;
      std::expected<void, StarException> add_particle(Particle&& particle) override {
        if (particle.string == "player-lounge-probe")
          ++particles;
        return {};
      }
    } events;
    if (!fixture.check(bool(player.collect_render_events(events)), "enable player effect collection")) return;
    std::uint64_t step = 0;
    auto tick = [&] {
      events.particles = 0;
      ++step;
      auto updated = player.update(1.f / 60, step);
      if (!fixture.check(bool(updated), updated ? "update player" : updated.error().what())) return false;
      auto collected = player.collect_render_events(events);
      return fixture.check(bool(collected), collected ? "collect player effects" : collected.error().what());
    };
    auto check_cosmetic = [&](std::string_view phase, EntityId seat, String const& marker) {
      auto anchor = player.lounging_in();
      EntityId actual_seat = anchor ? anchor->entityId : null_entity_id;
      if (!fixture.check(actual_seat == seat, std::format("{}: expected seat {}, anchored to {}", phase, seat, actual_seat)))
        return false;
      auto equipped = player.inventory()->equipment(EquipmentSlot::HeadCosmetic);
      auto presented = player.humanoid()->default_death_particles();
      return fixture.check(equipped && equipped->descriptor() == (*inventory_armor)->descriptor(),
               "lounge must not replace the inventory cosmetic")
        && fixture.check(presented == marker,
             std::format("{}: tick {}, seat {}: expected '{}', presented '{}'; seat stat={}, particles={}",
               phase, step, actual_seat, marker.utf8(), presented.utf8(),
               player.status_controller().stat("loungeProbe"), events.particles));
    };
    auto sit = [&](std::string_view phase, EntityId seat, String const& marker) {
      if (!fixture.check(player.lounge(seat, 0) == true, "enter or switch to chair")) return false;
      for (unsigned i = 0; i != 2; ++i)
        if (!tick() || !check_cosmetic(phase, seat, marker)
            || !fixture.check(player.status_controller().stat("loungeProbe") == 2.f && events.particles == 2,
              "submit seat and armor effects on every lounge tick")) return false;
      return true;
    };

    if (!fixture.check(!player.lounging_in(), "begin standing before any gameplay tick")) return;
    if (!tick() || !check_cosmetic("standing", null_entity_id, inventory_marker)
        || !fixture.check(events.particles == 1, "emit equipped armor effect while standing")) return;
    constexpr std::array<std::string_view, 3> switch_phases{"switch to empty", "switch to unknown", "switch to incompatible"};
    for (std::size_t i = 0; i != switch_phases.size(); ++i)
      if (!sit("override", seats.front(), override_marker) || !sit(switch_phases[i], seats[i + 1], inventory_marker)) return;
    if (!sit("override before leaving", seats.front(), override_marker)
        || !fixture.check(bool(player.stop_lounging()), "leave the override chair")
        || !tick() || !check_cosmetic("leave override", null_entity_id, inventory_marker)
        || !fixture.check(player.status_controller().stat("loungeProbe") == 0.f, "clear persistent lounge status on leave")
        || !tick() || !tick()) return;
    fixture.check(events.particles == 1, "retire the seat emitter while keeping the restored armor emitter");
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
    ASSERT_TRUE((*humanoid)->set_identity(HumanoidIdentity{}));
    auto owner = ArmorWearer::create(root);
    ASSERT_TRUE(owner);
    auto& wearer = **owner;
    auto database = root.itemDatabase();
    ASSERT_TRUE(database);
    String name;
    for (auto const& candidate : (*database)->all_items()) {
      auto type = (*database)->item_type(candidate);
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
    auto base = (*humanoid)->default_movement_parameters();
    auto functional = make_armor({{"hideInVanillaSlots", false}, {"bypassNude", false},
        {"humanoidConfig", JsonObject{{"movementParameters", JsonObject{{"armorProbe", 1}}},
                                       {"mouthOffset", JsonArray{8, 16}}, {"deathParticles", "equipped-a"}}}});
    auto cosmetic = make_armor({{"armorTypesToHide", JsonArray{"Head"}}, {"humanoidConfig", JsonObject{}}});
    ASSERT_TRUE(functional);
    ASSERT_TRUE(cosmetic);
    ASSERT_TRUE(wearer.set_item(0, functional));
    for (unsigned invalid : {12u, 248u, 255u})
      EXPECT_FALSE(wearer.set_cosmetic_item(invalid, cosmetic));
    EXPECT_EQ(wearer.item_descriptor(0), functional->descriptor());
    auto refresh = [&](bool nude = false) { return wearer.setup_humanoid(**humanoid, nude); };
    ASSERT_EQ(refresh(), true);
    auto equipped = (*humanoid)->default_movement_parameters();
    ASSERT_NE(equipped, base);
    EXPECT_EQ(refresh(), false);
    for (unsigned relative : {0u, 11u}) {
      ASSERT_TRUE(wearer.set_cosmetic_item(relative, cosmetic));
      EXPECT_EQ(wearer.item_descriptor(8 + relative), cosmetic->descriptor());
      ASSERT_EQ(refresh(), true);
      EXPECT_EQ((*humanoid)->default_movement_parameters(), base);
      ASSERT_TRUE(wearer.set_cosmetic_item(relative, {}));
      ASSERT_EQ(refresh(), true);
      EXPECT_EQ((*humanoid)->default_movement_parameters(), equipped);
    }
    ASSERT_EQ(refresh(true), true);
    EXPECT_EQ((*humanoid)->default_movement_parameters(), base);
    ASSERT_EQ(refresh(), true);
    auto equipped_config = (*humanoid)->humanoid_config(true);
    ASSERT_TRUE(equipped_config);
    auto broken = make_armor({{"hideInVanillaSlots", false},
        {"humanoidConfig", JsonObject{{"mouthOffset", JsonArray{32, 48}}, {"deathParticles", "rejected-b"},
                                       {"movementParameters", JsonObject{{"armorProbe", 2}}}, {"headRotationCenter", JsonArray{1}}}}});
    ASSERT_TRUE(broken);
    ASSERT_TRUE(wearer.set_item(0, broken));
    for (unsigned attempt = 0; attempt != 2; ++attempt) {
      EXPECT_FALSE(refresh().has_value());
      EXPECT_EQ((*humanoid)->mouth_offset(true), Vec2F(1, 2));
      EXPECT_EQ((*humanoid)->default_death_particles(), "equipped-a");
      EXPECT_EQ((*humanoid)->humanoid_config(true), *equipped_config);
      EXPECT_EQ((*humanoid)->default_movement_parameters(), equipped);
    }
    ASSERT_TRUE(wearer.set_item(0, functional));
    EXPECT_EQ(refresh(), false);
    ASSERT_TRUE(wearer.set_item(0, {}));
    EXPECT_EQ(refresh(), true);
    EXPECT_EQ((*humanoid)->default_movement_parameters(), base);

    // A parameter failure must not publish either its base config or its network map,
    // and a later successful base edit must still merge the previously accepted overlay.
    auto net = NetHumanoid::create(root);
    ASSERT_TRUE(net) << net.error().what();
    auto& wrapper = **net;
    auto& model = *wrapper.humanoid();
    JsonObject accepted{{"deathParticles", "base-a"}, {"playerMovementParameters", JsonObject{{"probe", 1}}}};
    ASSERT_TRUE(wrapper.set_humanoid_parameters(accepted));
    Json overlay = JsonObject{{"mouthOffset", JsonArray{16, -8}}, {"movementParameters", JsonObject{{"armorProbe", 7}}}};
    ASSERT_EQ(model.load_config(overlay), true);
    auto accepted_base = model.humanoid_config(false);
    auto accepted_config = model.humanoid_config(true);
    ASSERT_TRUE(accepted_base);
    ASSERT_TRUE(accepted_config);
    auto accepted_movement = model.default_movement_parameters();
    auto accepted_player_movement = model.player_movement_parameters();
    JsonObject invalid{{"deathParticles", "base-b"}, {"playerMovementParameters", JsonObject{{"probe", 2}}},
                       {"headRotationCenter", JsonArray{1}}};
    for (unsigned attempt = 0; attempt != 2; ++attempt) {
      EXPECT_FALSE(wrapper.set_humanoid_parameters(invalid).has_value());
      EXPECT_EQ(wrapper.humanoid_parameters(), accepted);
      EXPECT_EQ(model.humanoid_config(false), *accepted_base);
      EXPECT_EQ(model.humanoid_config(true), *accepted_config);
      EXPECT_EQ(model.default_death_particles(), "base-a");
      EXPECT_EQ(model.mouth_offset(true), Vec2F(2, -1));
      EXPECT_EQ(model.default_movement_parameters(), accepted_movement);
      EXPECT_EQ(model.player_movement_parameters(), accepted_player_movement);
    }
    ASSERT_TRUE(wrapper.set_humanoid_parameters({{"deathParticles", "base-c"}}));
    EXPECT_EQ(model.default_death_particles(), "base-c");
    EXPECT_EQ(model.mouth_offset(true), Vec2F(2, -1));
    EXPECT_EQ(model.default_movement_parameters(), accepted_movement);
    EXPECT_EQ(model.load_config(overlay), false);
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
    ASSERT_TRUE((*humanoid)->set_identity(identity));
    auto database = fixture.root().itemDatabase();
    ASSERT_TRUE(database) << database.error().what();
    auto names = (*database)->all_items();
    std::sort(names.begin(), names.end());
    String chestName;
    for (auto const& name : names) {
      auto type = (*database)->item_type(name);
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
      ASSERT_TRUE((*humanoid)->set_wearable_from_chest(0, chest->get(), identity.gender));
      auto sleeve = AssetPath::split(chest->get().backSleeveFrameset(identity.gender)).base_path;
      for (auto direction : {Direction::Left, Direction::Right}) {
        (*humanoid)->set_facing_direction(direction);
        for (bool holding : {false, true}) {
          (*humanoid)->set_hand_parameters(ToolHand::Primary, holding, 0, 0, false, false, false);
          (*humanoid)->set_hand_parameters(ToolHand::Alt, holding, 0, 0, false, false, false);
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
    auto& movement = player.movement_controller();
    ActorMovementParameters parameters;
    parameters.gravity_enabled = false;
    parameters.collision_enabled = false;
    parameters.friction_enabled = false;
    ASSERT_TRUE(movement.update_base_parameters(parameters));
    movement.set_position(Vec2F(1024, 1024));
    auto nativeCollisionPoly = movement.collision_poly();

    // The real Lua callback rejects other humanoid states, not just unknown
    // names, and accepts nil. No fixture implementation stands in for it.
    auto lua = LuaRoot::create(**root);
    ASSERT_TRUE(lua);
    auto context = (*lua)->create_context(StringList{});
    ASSERT_TRUE(context);
    auto callbacks = LuaBindings::make_player_callbacks(**root, player);
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
        EXPECT_EQ(remotes[i]->humanoid()->facing_direction(), player.humanoid()->facing_direction());
        EXPECT_EQ(remotes[i]->humanoid()->moving_backwards(), player.humanoid()->moving_backwards());
        EXPECT_EQ(remotes[i]->movement_controller().moving_direction(), networkDirection);
        EXPECT_NEAR(remotes[i]->movement_controller().rotation(), movement.rotation(), .011f);
        EXPECT_FALSE(remotes[i]->movement_controller().on_ground());
        EXPECT_FALSE(remotes[i]->movement_controller().crouching());
        EXPECT_EQ(remotes[i]->movement_controller().collision_poly(), nativeCollisionPoly);
        auto rendered = remotes[i]->humanoid()->render();
        ASSERT_TRUE(rendered) << rendered.error().what();
        EXPECT_FALSE(rendered->empty());
      }
    };
    std::array states{Humanoid::Idle, Humanoid::Walk, Humanoid::Run, Humanoid::Jump, Humanoid::Fall, Humanoid::Duck};
    std::array<std::string_view, 6> names{"idle", "walk", "run", "jump", "fall", "duck"};
    auto nativeDirection = movement.moving_direction();
    for (float rotation : {0.f, std::numbers::pi_v<float> / 2, std::numbers::pi_v<float>, -std::numbers::pi_v<float> / 2}) {
      movement.set_rotation(rotation);
      auto tangent = Vec2F::with_angle(rotation);
      for (std::size_t i = 0; i != states.size(); ++i) {
        player.set_generic_property("poseFixture", Json(String(names[i])));
        for (auto facing : {Direction::Left, Direction::Right}) {
          for (auto direction : {Direction::Left, Direction::Right}) {
            movement.control_face(facing);
            movement.set_velocity(tangent * (direction == Direction::Left ? -14.f : 14.f));
            ASSERT_TRUE(tick());
            EXPECT_EQ(player.humanoid()->state(), states[i]);
            EXPECT_EQ(player.humanoid()->facing_direction(), facing);
            EXPECT_EQ(player.humanoid()->moving_backwards(), facing != direction);
            EXPECT_EQ(movement.moving_direction(), nativeDirection);
            EXPECT_FALSE(movement.on_ground());
            EXPECT_TRUE(movement.zero_g());
            EXPECT_FALSE(movement.crouching());
            EXPECT_EQ(movement.collision_poly(), nativeCollisionPoly);
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
    movement.set_rotation(std::numbers::pi_v<float>);
    player.set_generic_property("poseFixture", "run");
    for (auto direction : {Direction::Right, Direction::Left}) {
      movement.control_move(direction);
      movement.control_face(Direction::Right);
      movement.set_velocity(Vec2F(-14.f, 0.f));
      ASSERT_TRUE(tick());
      EXPECT_EQ(movement.moving_direction(), direction);
      sync(Humanoid::Run, Direction::Right);
    }
    nativeDirection = movement.moving_direction();
    movement.set_velocity(Vec2F());
    ASSERT_TRUE(tick());
    EXPECT_FALSE(player.humanoid()->moving_backwards());
    sync(Humanoid::Run, player.humanoid()->facing_direction());
    player.set_generic_property("poseFixture", "duck");
    ASSERT_TRUE(tick());
    sync(Humanoid::Duck, player.humanoid()->facing_direction());

    player.set_generic_property("poseFixture", Json());
    ASSERT_TRUE(tick());
    EXPECT_EQ(player.humanoid()->state(), Humanoid::SwimIdle);
    EXPECT_EQ(movement.moving_direction(), nativeDirection);
    sync(Humanoid::SwimIdle, nativeDirection);
    player.set_generic_property("poseFixture", "duck");
    player.set_generic_property("poseRevoke", true);
    ASSERT_TRUE(tick());
    EXPECT_EQ(player.humanoid()->state(), Humanoid::SwimIdle);
    sync(Humanoid::SwimIdle, nativeDirection);
    player.set_generic_property("poseFixture", Json());
    player.set_generic_property("poseRevoke", false);
    player.set_generic_property("poseLate", true);
    ASSERT_TRUE(tick());
    ASSERT_TRUE(tick());
    EXPECT_EQ(player.humanoid()->state(), Humanoid::SwimIdle);
    sync(Humanoid::SwimIdle, nativeDirection);
    player.set_generic_property("poseLate", false);
    player.set_generic_property("poseFixture", "duck");
    ASSERT_TRUE(tick());
    player.set_generic_property("poseFixture", Json());
    ASSERT_TRUE(player.uninit());
    ASSERT_TRUE(player.init(*setup.world, 100, EntityMode::Master));
    ASSERT_TRUE(player.refresh_humanoid());
    ASSERT_TRUE(movement.update_base_parameters(parameters));
    ASSERT_TRUE(tick());
    EXPECT_EQ(player.humanoid()->state(), Humanoid::SwimIdle);
    sync(Humanoid::SwimIdle, movement.moving_direction());
    player.set_generic_property("poseFixture", "duck");
    movement.set_velocity(Vec2F());
    movement.control_face(Direction::Right);
    ASSERT_TRUE(tick());
    sync(Humanoid::Duck, Direction::Right);
    ASSERT_TRUE(player.teleport_out());
    // A higher-priority state can start between update and packet generation.
    for (std::size_t i = 0; i != remotes.size(); ++i) {
      auto packet = player.write_net_state(versions[i], rules[i]);
      ASSERT_TRUE(packet);
      versions[i] = packet->second;
      ASSERT_TRUE(remotes[i]->read_net_state(packet->first, 0.f, rules[i]));
      EXPECT_TRUE(remotes[i]->is_teleporting());
      EXPECT_EQ(remotes[i]->movement_controller().moving_direction(), movement.moving_direction());
    }
    ASSERT_TRUE(tick());
    EXPECT_TRUE(player.is_teleporting());
    // Teleport freezes the previous humanoid cycle, but must send its native
    // teleport state and restore native movingDirection, not the request.
    sync(player.humanoid()->state(), movement.moving_direction());
    for (auto const& remote : remotes)
      EXPECT_TRUE(remote->is_teleporting());
    ASSERT_TRUE(player.teleport_abort());
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
    for (unsigned i = 0; i < abortTicks && player.is_teleporting(); ++i)
      ASSERT_TRUE(tick());
    ASSERT_FALSE(player.is_teleporting());
    ASSERT_EQ(player.humanoid()->state(), Humanoid::Duck);
    sync(Humanoid::Duck, Direction::Right);
    ASSERT_TRUE(player.kill());
    ASSERT_TRUE(player.is_dead());
    ASSERT_TRUE(tick());
    ASSERT_TRUE(player.is_dead());
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
      ASSERT_TRUE(humanoid->set_identity(HumanoidIdentity{}));
      for (auto state : {Humanoid::Walk, Humanoid::Run}) {
        for (float rotation : {std::numbers::pi_v<float> / 2, std::numbers::pi_v<float>}) {
          humanoid->set_rotation(rotation);
          for (bool backwards : {false, true}) {
            ASSERT_TRUE(humanoid->set_moving_backwards(backwards));
            std::set<String> frames;
            for (int frame = 0; frame != 60; ++frame) {
              ASSERT_TRUE(humanoid->set_state(state));
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

const GameTestRegistration animator_include_chain(
  "HumanoidRenderTest", "AnimatorAllowsSharedIncludesAndRejectsCycles", [](GameTest& fixture) {
    auto directory = File::temporary_directory(fixture.root().random());
    if (!fixture.check(bool(directory), "create private include fixture")) return;
    auto cleanup = make_scope_exit([&] { fixture.check(bool(File::remove_all(*directory)), "remove include fixture"); });
    auto overlay = File::relative_to(*directory, "assets");
    if (!fixture.check(bool(File::create_directories(overlay)), "create include asset directory")) return;
    auto asset = [&](String const& file, String const& contents) {
      return fixture.check(bool(File::write_file(contents, File::relative_to(overlay, file))), "write animator include");
    };
    if (!asset("cycle-a.config", R"({"includes":["cycle-b.config"]})") ||
        !asset("cycle-b.config", R"({"includes":["cycle-a.config"]})") ||
        !asset("shared.config", R"({"transformationGroups":{"inherited":{}}})") ||
        !asset("left.config", R"({"includes":["shared.config"]})") ||
        !asset("right.config", R"({"includes":["shared.config"]})")) return;
    auto settings = fixture.rootSettings();
    settings.assetSources.push_back(overlay);
    settings.storageDirectory = File::relative_to(*directory, "storage");
    settings.runtimeConfigFile = {};
    settings.logDirectory = {};
    settings.logFile = {};
    settings.includeUGC = false;
    auto root = Root::create(std::move(settings));
    if (!fixture.check(bool(root), "open private include root")) return;
    auto cyclic = NetworkedAnimator::create(**root, JsonObject{{"version", 1}, {"includes", JsonArray{"cycle-a.config"}}});
    if (!fixture.check(!cyclic && std::string_view(cyclic.error().what()).contains("/cycle-a.config -> /cycle-b.config -> /cycle-a.config"),
                       "report the active include chain")) return;
    auto shared = NetworkedAnimator::create(**root, JsonObject{{"version", 1}, {"includes", JsonArray{"left.config", "right.config"}}});
    fixture.check(shared && (*shared)->has_transformation_group("inherited"), "shared includes contribute their groups");
  });

const GameTestRegistration animatorSemanticSampling(
  "HumanoidRenderTest", "AnimatorSampleDrivesGeometryLightsAndScopedOverrides", [](GameTest& fixture) {
    auto config = Json::parse(R"({
      "version":1,
      "transformationGroups":{"moving":{}},
      "rotationGroups":{"aim":{"angularVelocity":2,"rotationCenter":[1,2]}},
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
      "lights":{
        "glow":{"position":[0,0],"color":[255,255,255],"anchorPart":"body"},
        "beam":{"position":[2,1],"pointLight":true,"pointAngle":-30,"pointBeam":0.75,"beamAmbience":0.25,
                "rotationGroup":"aim","rotationCenter":[2,0]},
        "rim":{"position":[2,2],"pointLight":true,"pointAngle":30,"rotationGroup":"aim"},
        "zBroken":{"active":false,"anchorPart":"missing"}
      }
    })");
    ASSERT_TRUE(config);
    auto made = NetworkedAnimator::create(fixture.root(), *config);
    ASSERT_TRUE(made) << made.error().what();
    auto& animator = **made;
    auto line = Drawable::make_line(Line2F({0, 0}, {1, 0}), 1, Color::palette_white);
    animator.set_part_drawables("body", {line});
    NetworkedAnimator::DynamicTarget events;
    ASSERT_TRUE(animator.update(.1f, std::ref(events)));
    ASSERT_TRUE(animator.translate_transformation_group("moving", {2, 0}));
    ASSERT_TRUE(animator.rotate_group("aim", 1.f));
    ASSERT_TRUE(animator.update(.25f, std::ref(events)));
    auto logicalTimer = animator.state_timer("movement");
    ASSERT_TRUE(logicalTimer);
    animator.set_local_tag("pose", "logical");
    for (int repeat = 0; repeat != 3; ++repeat) {
      auto sample = animator.sample(.5f);
      auto other = animator.sample(.1f);
      auto otherPoint = other.part_point("body", "testPoint");
      ASSERT_TRUE(otherPoint);
      ASSERT_TRUE(*otherPoint);
      EXPECT_NEAR((**otherPoint)[0], 1.2f, 1e-5f);
      auto timer = sample.state_timer("movement");
      ASSERT_TRUE(timer);
      EXPECT_NEAR(*timer, .225f, 1e-5f);
      auto angle = sample.current_rotation_angle("aim");
      ASSERT_TRUE(angle);
      EXPECT_NEAR(*angle, .25f, 1e-5f);
      auto point = sample.part_point("body", "testPoint");
      ASSERT_TRUE(point);
      ASSERT_TRUE(*point);
      EXPECT_NEAR((**point)[0], 2.8f, 1e-5f);
      auto drawables = sample.drawables();
      ASSERT_TRUE(drawables) << drawables.error().what();
      ASSERT_EQ(drawables->size(), 1u);
      auto expected = line;
      expected.transform(Mat3F::translation({2.8f, 0}));
      EXPECT_NEAR((*drawables)[0].position[0], expected.position[0], 1e-5f);
      constexpr float pi = std::numbers::pi_v<float>;
      Vec2F const translation{3, -4};
      auto lights = sample.light_sources(translation);
      if (!fixture.check(bool(lights), "sampled animator lights")
          || !fixture.check(lights->size() == 3u, "three active lights in animator order"))
        return;
      auto const& beam = (*lights)[0];
      auto check_near = [&](float actual, float expected, std::string_view what,
                            std::source_location at = std::source_location::current()) {
        fixture.check(std::abs(actual - expected) <= 1e-5f, what, at);
      };
      fixture.check(beam.type == LightType::Point, "beam is a point light");
      fixture.check(beam.point_beam == .75f, "configured beam strength");
      fixture.check(beam.beam_ambience == .25f, "configured beam ambience");
      check_near(beam.position[0], 5.f - std::sin(.25f), "beam x uses the overridden pivot");
      check_near(beam.position[1], -4.f + std::cos(.25f), "beam y uses the overridden pivot");
      check_near(beam.beam_angle, -pi / 6 + .25f, "beam angle includes sampled rotation");
      check_near((*lights)[1].position[0], 5.8f, "glow tracks the animated anchor");
      check_near((*lights)[2].position[0], 4.f + std::cos(.25f), "rim x uses the inherited pivot");
      check_near((*lights)[2].position[1], -2.f + std::sin(.25f), "rim y uses the inherited pivot");
      check_near((*lights)[2].beam_angle, pi / 6 + .25f, "rim angle includes sampled rotation");

      animator.set_zoom(2);
      animator.set_flipped(true, 1);
      List<LightSource> appended{beam};
      if (!fixture.check(bool(sample.append_light_sources(appended, translation)), "append flipped sampled lights")
          || !fixture.check(appended.size() == 4u, "three lights were appended"))
        return;
      fixture.check(appended[0].position == beam.position, "append preserves the existing light");
      check_near(appended[1].position[0], 1.f + 2.f * std::sin(.25f), "flipped beam x uses the global center and zoom");
      check_near(appended[1].position[1], -4.f + 2.f * std::cos(.25f), "flipped beam y uses global zoom");
      check_near(appended[1].beam_angle, -pi - beam.beam_angle, "flipped negative beam angle");
      check_near(appended[2].position[0], -.6f, "flipped glow still tracks the animated anchor");
      check_near(appended[3].beam_angle, pi - (*lights)[2].beam_angle, "flipped positive beam angle");
      auto logical_lights = animator.light_sources(translation);
      if (!fixture.check(bool(logical_lights), "logical animator lights")
          || !fixture.check(logical_lights->size() == 3u, "three active logical lights"))
        return;
      check_near((*logical_lights)[0].position[0], 1.f + 2.f * std::sin(.5f), "logical beam uses current rather than sampled rotation");
      check_near((*logical_lights)[0].beam_angle, -pi + pi / 6 - .5f, "logical flipped beam angle");
      if (!fixture.check(bool(animator.set_light_active("zBroken", true)), "enable the invalid light"))
        return;
      fixture.check(!sample.append_light_sources(appended, translation), "invalid anchor fails the append");
      fixture.check(appended.size() == 4u, "failed append rolls back new lights");
      fixture.check(!appended.empty() && appended[0].position == beam.position, "failed append preserves the original light");
      if (!fixture.check(bool(animator.set_light_active("zBroken", false)), "disable the invalid light"))
        return;
      animator.set_flipped(false);
      animator.set_zoom(1);
      ASSERT_TRUE(sample.set_local_state("movement", "stop"));
      EXPECT_FALSE(sample.set_local_state("movement", "missing"));
      sample.set_local_tag("pose", "sampled");
      auto sampledTag = sample.apply_part_tags("body", "<pose>");
      auto otherTag = other.apply_part_tags("body", "<pose>");
      auto logicalTag = animator.apply_part_tags("body", "<pose>");
      ASSERT_TRUE(sampledTag);
      ASSERT_TRUE(otherTag);
      ASSERT_TRUE(logicalTag);
      EXPECT_EQ(*sampledTag, "sampled");
      EXPECT_EQ(*otherTag, "logical");
      EXPECT_EQ(*logicalTag, "logical");
      ASSERT_TRUE(sample.translate_local_transformation_group("moving", {10, 0}));
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
      auto untouched = other.part_point("body", "testPoint");
      ASSERT_TRUE(untouched);
      ASSERT_TRUE(*untouched);
      EXPECT_NEAR((**untouched)[0], 1.2f, 1e-5f);
      auto logicalState = animator.state("movement");
      auto logicalLocal = animator.get_local_transformation_group("moving");
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
    auto timer = animator.state_timer("movement");
    ASSERT_TRUE(timer);
    EXPECT_FLOAT_EQ(*timer, *logicalTimer);
    auto state = animator.state("movement");
    ASSERT_TRUE(state);
    EXPECT_EQ(*state, "walk");
    auto local = animator.get_local_transformation_group("moving");
    ASSERT_TRUE(local);
    EXPECT_FLOAT_EQ(local->transform_point(Vec2F{})[0], 0);
    EXPECT_TRUE(events.pull_new_audios().empty());
    EXPECT_TRUE(events.pull_new_particles().empty());
    ASSERT_TRUE(animator.rotate_group("aim", -2.f, true));
    auto sample = animator.sample(0);
    auto angle = sample.current_rotation_angle("aim");
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
    animator.set_part_drawables("child", {line});
    auto expectVec = [](Vec2F const& actual, Vec2F const& expected) {
      EXPECT_NEAR(actual[0], expected[0], 1e-5f);
      EXPECT_NEAR(actual[1], expected[1], 1e-5f);
    };
    auto expectPose = [&](NetworkedAnimator::Sample const& view, Mat3F const& parent, Mat3F const& global = Mat3F::identity()) {
      auto child = parent * Mat3F::translation({0, 3});
      // Query the child first so the parent's transformation is also reached recursively.
      auto childTransform = view.part_transformation("child");
      auto parentTransform = view.part_transformation("parent");
      ASSERT_TRUE(childTransform);
      ASSERT_TRUE(parentTransform);
      for (auto point : {Vec2F{}, Vec2F{1, 0}, Vec2F{0, 1}}) {
        expectVec(childTransform->transform_point(point), child.transform_point(point));
        expectVec(parentTransform->transform_point(point), parent.transform_point(point));
      }
      auto final = global * child;
      auto point = view.part_point("child", "point");
      auto poly = view.part_poly("child", "poly");
      auto lights = view.light_sources();
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
        ASSERT_TRUE(shared.translate_local_transformation_group("moving", {5, 2}));
        auto translated = Mat3F::translation({5, 2}) * idle;
        expectPose(sample, translated);
        expectPose(shared, translated);
        ASSERT_TRUE(shared.rotate_local_transformation_group("moving", std::numbers::pi_v<float> / 2));
        expectPose(sample, Mat3F::rotation(std::numbers::pi_v<float> / 2) * translated);
        ASSERT_TRUE(shared.reset_local_transformation_group("moving"));
        expectPose(sample, idle);
        ASSERT_TRUE(shared.set_local_state("pose", "shifted"));
        expectPose(sample, Mat3F::translation({7, 0}));
        EXPECT_FALSE(shared.set_local_state("pose", "missing"));
        EXPECT_FALSE(shared.translate_local_transformation_group("missing", {5, 2}));
        EXPECT_FALSE(shared.rotate_local_transformation_group("missing", 1));
        EXPECT_FALSE(shared.reset_local_transformation_group("missing"));
        EXPECT_FALSE(shared.part_transformation("missing"));
        expectPose(sample, Mat3F::translation({7, 0}));
        ASSERT_TRUE(shared.set_local_state("pose", "badGroup"));
        EXPECT_FALSE(sample.part_transformation("child"));
        ASSERT_TRUE(shared.set_local_state("pose", "badAnchor"));
        EXPECT_FALSE(sample.part_transformation("child"));
        ASSERT_TRUE(shared.set_local_state("pose", "idle"));
        expectPose(sample, idle);
      }
      expectPose(independent, idle);
      auto logical = animator.part_transformation("child");
      auto logicalState = animator.state("pose");
      auto logicalGroup = animator.get_local_transformation_group("moving");
      ASSERT_TRUE(logical);
      ASSERT_TRUE(logicalState);
      ASSERT_TRUE(logicalGroup);
      expectVec(logical->transform_point(Vec2F{0, 0}), {3, 3});
      EXPECT_EQ(*logicalState, "idle");
      EXPECT_EQ(*logicalGroup, Mat3F::identity());
      animator.set_zoom(2);
      animator.set_flipped(true);
      auto global = Mat3F::scaling(Vec2F{-1, 1}) * Mat3F::scaling(2.f);
      expectPose(sample, idle, global);
      expectPose(independent, idle, global);
      animator.set_flipped(false);
      animator.set_zoom(1);
      // Leave both state and local group overrides behind before the slot is reused.
      ASSERT_TRUE(sample.set_local_state("pose", "shifted"));
      ASSERT_TRUE(sample.translate_local_transformation_group("moving", {20, 0}));
      expectPose(sample, Mat3F::translation({27, 0}));
    }
  });

const GameTestRegistration animator_anchor_cycles(
  "HumanoidRenderTest", "DynamicAnimatorAnchorsDetectOnlyActiveCyclesAndRetry", [](GameTest& fixture) {
    auto config = Json::parse(R"({
      "version":1,
      "animatedParts":{
        "stateTypes":{"pose":{"default":"idle","states":{
          "idle":{"frames":1,"cycle":1},
          "self":{"frames":1,"cycle":1},
          "looping":{"frames":2,"cycle":1,"mode":"loop"}
        }}},
        "parts":{
          "root":{"properties":{"offset":[3,0]}},
          "pivot":{"properties":{"offset":[1,0],"anchorPart":"root"},"partStates":{"pose":{
            "self":{"properties":{"anchorPart":"pivot"}},
            "looping":{"frameProperties":{"anchorPart":["root","tail"]}}
          }}},
          "tail":{"properties":{"offset":[0,2],"anchorPart":"pivot","point":[0,0]}},
          "sibling":{"properties":{"offset":[0,4],"anchorPart":"pivot"}}
        }
      },
      "lights":{"glow":{"active":true,"position":[0,0],"color":[255,255,255],"anchorPart":"tail"}}
    })");
    if (!fixture.check(bool(config), "parse dynamic anchor animation"))
      return;
    auto made = NetworkedAnimator::create(fixture.root(), *config);
    if (!fixture.check(bool(made), "create dynamic anchor animator"))
      return;
    auto& animator = **made;
    auto check_position = [&](std::expected<Mat3F, StarException> const& transform, Vec2F expected, std::string_view context) {
      if (fixture.check(bool(transform), context))
        fixture.check(transform->transform_point(Vec2F{}) == expected, "correct anchored position");
    };
    {
      std::optional<NetworkedAnimator::Sample> shared;
      {
        auto sample = animator.sample(1.f);
        shared.emplace(sample);
        check_position(sample.part_transformation("tail"), {4, 2}, "cache acyclic tail");
        check_position(sample.part_transformation("sibling"), {4, 4}, "shared parent is not a cycle");
        if (!fixture.check(shared->set_local_state("pose", "self") == true, "enter self anchor through sample copy"))
          return;
        auto failed = sample.part_transformation("tail");
        fixture.check(!failed && std::string_view(failed.error().what()).contains("tail -> pivot -> pivot"),
            "self cycle invalidates cached descendant and reports the full path");
      }
      auto independent = animator.sample(1.f);
      check_position(independent.part_transformation("tail"), {4, 2}, "independent sample remains acyclic");
      auto failed = shared->part_transformation("tail");
      fixture.check(!failed && std::string_view(failed.error().what()).contains("tail -> pivot -> pivot"),
          "copied sample keeps its slot after the original is destroyed");
      if (!fixture.check(shared->set_local_state("pose", "idle") == true, "restore acyclic sampled pose"))
        return;
      check_position(shared->part_transformation("tail"), {4, 2}, "same sample retries self cycle successfully");
    }
    if (!fixture.check(animator.set_state("pose", "looping") == true, "start frame-driven anchor animation")
        || !fixture.check(bool(animator.update(.6f, {})), "advance to cyclic frame"))
      return;
    auto logical_failed = animator.part_transformation("tail");
    fixture.check(!logical_failed && std::string_view(logical_failed.error().what()).contains("tail -> pivot -> tail"),
        "logical frame reports the mutual anchor cycle");
    {
      auto previous = animator.sample(.25f);
      auto cyclic = animator.sample(1.f);
      check_position(previous.part_transformation("tail"), {4, 2}, "earlier sampled frame is acyclic");
      auto failed = cyclic.part_point("tail", "point");
      fixture.check(!failed && std::string_view(failed.error().what()).contains("tail -> pivot -> tail"),
          "sample point query reports its cyclic frame");
      auto lights = cyclic.light_sources();
      fixture.check(!lights && std::string_view(lights.error().what()).contains("tail -> pivot -> tail"),
          "light query retries and reports the same path");
      if (!fixture.check(cyclic.set_local_state("pose", "idle") == true, "restore acyclic sampled frame"))
        return;
      check_position(cyclic.part_transformation("sibling"), {4, 4}, "valid sibling after sampled cycle");
      check_position(cyclic.part_transformation("tail"), {4, 2}, "valid descendant after sampled cycle");
      fixture.check(!animator.part_transformation("tail"), "sample repair does not rewrite logical pose");
    }
    if (!fixture.check(animator.set_state("pose", "idle") == true, "restore acyclic logical pose"))
      return;
    check_position(animator.part_transformation("tail"), {4, 2}, "same animator retries cycle successfully");
    check_position(animator.sample(1.f).part_transformation("tail"), {4, 2}, "reused sample slot remains valid");
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
      ASSERT_TRUE(humanoid.set_identity(HumanoidIdentity{}));
      ASSERT_TRUE(humanoid.animate(1.f / 60, {}));
      humanoid.set_rotation(1.f);
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
      EXPECT_EQ(humanoid.emote_state(), HumanoidEmote::Idle);
    }
  });

const GameTestRegistration playerPresentationEvents(
  "HumanoidRenderTest", "PlayerEventsAreTickOwnedAndInvisibleEventsAreDiscarded", [](GameTest& fixture) {
    auto& root = fixture.root();
    PlayerWorldContext setup;
    auto initialized = setup.init(root);
    ASSERT_TRUE(initialized) << initialized.error().what();
    auto& player = *setup.player;
    ASSERT_TRUE(player.move_to(Vec2F(1024, 1024)));

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
    ASSERT_TRUE(player.collect_render_events(discard));
    Particle particle;
    particle.string = "presentation-event-fixture";
    auto assets = root.assets();
    ASSERT_TRUE(assets);
    auto sounds = (*assets)->scan("/sfx/", ".ogg");
    ASSERT_TRUE(sounds);
    ASSERT_FALSE(sounds->empty());
    std::sort(sounds->begin(), sounds->end());
    ASSERT_TRUE(player.add_particles({particle}));
    ASSERT_TRUE(player.add_sound(sounds->front()));
    constexpr std::uint64_t gameplaySeed = 0x50524553454e54;
    root.random().seed(gameplaySeed);
    RandomSource untouchedGameplay(gameplaySeed);
    for (float alpha : {0.0f, 0.25f, 0.75f, 1.0f, 0.5f}) {
      events.alpha = alpha;
      auto geometry = player.render(events);
      ASSERT_TRUE(geometry) << geometry.error().what();
      ASSERT_EQ(root.random().next_u64(), untouchedGameplay.next_u64()) << "geometry alpha=" << alpha;
      auto lighting = player.render_light_sources(events);
      ASSERT_TRUE(lighting) << lighting.error().what();
      ASSERT_EQ(root.random().next_u64(), untouchedGameplay.next_u64()) << "lighting alpha=" << alpha;
    }
    EXPECT_EQ(events.particles, 0u);
    EXPECT_EQ(events.audios, 0u);
    ASSERT_TRUE(player.collect_render_events(events));
    EXPECT_EQ(events.particles, 1u);
    EXPECT_EQ(events.audios, 1u);
    ASSERT_TRUE(player.render(events));
    ASSERT_TRUE(player.collect_render_events(events));
    EXPECT_EQ(events.particles, 1u);
    EXPECT_EQ(events.audios, 1u);

    // A tick with no display render still delivers its effects exactly once.
    ASSERT_TRUE(player.add_particles({particle}));
    ASSERT_TRUE(player.add_sound(sounds->front()));
    ASSERT_TRUE(player.collect_render_events(events));
    EXPECT_EQ(events.particles, 2u);
    EXPECT_EQ(events.audios, 2u);
    ASSERT_TRUE(player.status_controller().setPersistentEffects(
      "presentationFixture", {StatModifier{StatValueModifier{"invisible", 1.0f}}}));
    ASSERT_TRUE(player.invisible());
    ASSERT_TRUE(player.add_particles({particle}));
    ASSERT_TRUE(player.add_sound(sounds->front()));
    ASSERT_TRUE(player.collect_render_events(events));
    EXPECT_EQ(events.particles, 2u);
    EXPECT_EQ(events.audios, 2u);
    ASSERT_TRUE(player.status_controller().clearPersistentEffects("presentationFixture"));
    ASSERT_FALSE(player.invisible());
    ASSERT_TRUE(player.render(events));
    ASSERT_TRUE(player.collect_render_events(events));
    EXPECT_EQ(events.particles, 2u);
    EXPECT_EQ(events.audios, 2u);

    auto lua = LuaRoot::create(root);
    ASSERT_TRUE(lua);
    auto context = (*lua)->create_context(StringList{});
    ASSERT_TRUE(context);
    auto callbacks = LuaBindings::make_movement_controller_callbacks(player.movement_controller());
    ASSERT_TRUE(callbacks);
    ASSERT_TRUE(context->set_callbacks("mcontroller", *callbacks));
    auto epoch = player.presentation_epoch();
    auto moved = context->eval<bool>(R"LUA(
      mcontroller.translate({0.1, 0})
      mcontroller.setXPosition(mcontroller.xPosition() + 0.1)
      return true
    )LUA");
    ASSERT_TRUE(moved);
    EXPECT_TRUE(*moved);
    EXPECT_EQ(player.presentation_epoch(), epoch);
    auto reset = context->eval<bool>("mcontroller.resetPresentation(); return true");
    ASSERT_TRUE(reset);
    EXPECT_TRUE(*reset);
    EXPECT_GT(player.presentation_epoch(), epoch);
    epoch = player.presentation_epoch();
    ASSERT_TRUE(player.move_to(Vec2F(1200, 1100)));
    EXPECT_GT(player.presentation_epoch(), epoch);
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
          "frontHandItem":{"properties":{"offset":[5,9]},
            "partStates":{"frontArm":{"plain":{"properties":{"offset":[8,9]}}}}},
          "backHandItem":{"properties":{"offset":[-4,6]}},
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
    auto animator = humanoid.networked_animator();
    ASSERT_TRUE(animator);
    ASSERT_TRUE(animator->get().translate_transformation_group("before", {2, 3}));
    ASSERT_TRUE(animator->get().scale_transformation_group("after", Vec2F(2, 3)));
    // The supplied angle replaces each arm's stored transform rather than composing with it.
    ASSERT_TRUE(animator->get().translate_transformation_group("frontArmRotation", {100, 0}));
    ASSERT_TRUE(animator->get().translate_transformation_group("backArmRotation", {0, 100}));

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

    humanoid.set_hand_frame_overrides(ToolHand::Primary, "rotation", "plain");
    expect_position(ToolHand::Alt, Direction::Right, {17, 34}); // No rotation marker.
    humanoid.set_hand_frame_overrides(ToolHand::Primary, "rotation", "broken");
    EXPECT_FALSE(humanoid.arm_position(ToolHand::Alt, Direction::Right, angle, {3, 4}));

    auto expect_offset = [&](ToolHand hand, Direction facing, Vec2F expected) {
      EXPECT_EQ(humanoid.hand_offset(hand, facing), expected);
    };
    expect_offset(ToolHand::Primary, Direction::Right, {-3, 5});
    expect_offset(ToolHand::Alt, Direction::Right, {4, 7});
    ASSERT_TRUE(animator->get().set_local_state("frontArm", "plain"));
    expect_offset(ToolHand::Alt, Direction::Right, {7, 7});
    humanoid.set_hand_parameters(ToolHand::Primary, true, 0, 0, true, false, false);
    expect_offset(ToolHand::Primary, Direction::Right, {7, 7});
    expect_offset(ToolHand::Alt, Direction::Right, {-3, 5});
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
      auto otherFrame = opposite.state_frame("body");
      ASSERT_TRUE(otherFrame);
      EXPECT_EQ(*otherFrame, alpha < .5f ? 1 : 0);
      auto frame = sample.state_frame("body");
      ASSERT_TRUE(frame);
      EXPECT_EQ(*frame, alpha < .5f ? 0 : 1);
      auto discrete = sample.part_point("body", "point");
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
    auto timer = animator.state_timer("body");
    ASSERT_TRUE(timer);
    EXPECT_NEAR(*timer, .6f, 1e-5f);
  });

const GameTestRegistration animator_drawable_cache(
  "HumanoidRenderTest", "AnimatorDrawableCacheRespectsCenterAndLiteralDirectivesInBothOrders", [](GameTest& fixture) {
    auto config = Json::parse(R"({
      "version":1,
      "animatedParts":{
        "stateTypes":{"pose":{"default":"center","states":{
          "center":{"frames":1,"cycle":1},
          "corner":{"frames":1,"cycle":1,"properties":{"processingDirectives":"?flipy"}},
          "tagged":{"frames":1,"cycle":1,"properties":{"processingDirectives":"?<frame>"}}
        }}},
        "parts":{"sprite":{
          "properties":{"image":"<sheet>","zLevel":3},
          "partStates":{"pose":{
            "center":{"properties":{"centered":true,"processingDirectives":"?flipx"}},
            "corner":{"properties":{"centered":false}},
            "tagged":{"properties":{"centered":false}}
          }}
        }}
      }
    })");
    if (!GameTest::check(bool(config), "parse animator fixture")) return;
    auto metadata = fixture.root().image_metadata_database();
    if (!GameTest::check(bool(metadata), "load image metadata")) return;
    String const path = "/humanoid/human/malehead.png";
    auto size = (*metadata)->image_size(AssetPath(path));
    if (!GameTest::check(bool(size) && (*size)[0] > 0 && (*size)[1] > 0, "load actual sprite dimensions")) return;
    Vec2F const extent = Vec2F(*size) / static_cast<float>(tile_pixels);

    for (bool center_first : {true, false}) {
      auto made = NetworkedAnimator::create(fixture.root(), *config);
      if (!GameTest::check(bool(made), "create animator")) return;
      auto& animator = **made;
      animator.set_global_tag("sheet", path);
      animator.set_local_tag("frame", "flipx");
      animator.set_part_tag("sprite", "frame", "flipy");
      auto sample = animator.sample(1);
      for (bool centered : {center_first, !center_first, center_first}) {
        if (!GameTest::check(bool(sample.set_local_state("pose", centered ? "center" : "corner")), "switch sample state")) return;
        auto tags = sample.apply_part_tags("sprite", "<frame>/<frameIndex>/<pose_state>");
        GameTest::check(bool(tags) && *tags == (centered ? "1/0/center" : "1/0/corner"), "image frame tags use the active frame");

        auto plain = sample.drawables();
        if (!GameTest::check(bool(plain) && plain->size() == 1 && (*plain)[0].is_image(), "render one sprite")) return;
        auto layered = sample.drawables_with_z_level();
        if (!GameTest::check(bool(layered) && layered->size() == 1, "render one sprite with a Z level")) return;
        auto const& sprite = (*plain)[0];
        GameTest::check((*layered)[0].first == sprite && (*layered)[0].second == 3.f, "plain and Z output retain the same sprite");
        auto const& image = sprite.image_part();
        GameTest::check(AssetPath::join(image.image) == path + (centered ? "?flipx" : "?flipy"),
            "part and state literal directives survive independently");
        Vec2F const low = centered ? -extent / 2.f : Vec2F{};
        GameTest::check(image.transformation.transform_point(Vec2F{}) + sprite.position == low
            && image.transformation.transform_point(Vec2F(*size)) + sprite.position == low + extent,
            "both image vertices use the active state's centering");
      }
      if (!GameTest::check(bool(sample.set_local_state("pose", "tagged")), "switch to a directive using the frame tag")) return;
      for (bool remove_local : {false, true}) {
        if (remove_local)
          sample.set_local_tag("frame", std::nullopt);
        auto rendered = sample.drawables();
        if (!GameTest::check(bool(rendered) && rendered->size() == 1 && (*rendered)[0].is_image(), "render sprite with a tagged directive")) return;
        GameTest::check(AssetPath::join((*rendered)[0].image_part().image) == path + (remove_local ? "?flipy" : "?flipx"),
            "directive frame tags follow normal precedence, including sample removal");
      }
    }
  });

static void exercise_movement_transitions(Root& root, Json const& config) {
  auto bob_config = config.set_path("runBob", JsonArray{0, 1, 2, 3, 4, 5, 6, 7});
  ASSERT_TRUE(bob_config);
  for (bool break_body : {true, false}) {
    JsonObject broken{{"body", JsonArray{break_body ? Json("missing") : Json("walk"), true, true}},
                      {"bodyHighPriority", JsonArray{break_body ? Json("walk") : Json("missing"), true, true}}};
    auto forward = bob_config->set_path("stateAnimations.jump", broken);
    ASSERT_TRUE(forward);
    auto both = forward->set_path("stateAnimationsBackwards.run", broken);
    ASSERT_TRUE(both);
    auto made = Humanoid::create(root, *both);
    ASSERT_TRUE(made) << made.error().what();
    auto& model = **made;
    ASSERT_TRUE(model.set_state(Humanoid::Run));
    {
      auto& animator = model.networked_animator().value().get();
      auto cycle = animator.state_cycle("body", {}).value();
      ASSERT_TRUE(model.animate(cycle * .26f, {}));
      ASSERT_TRUE(model.animate(cycle * .28f, {}));
      auto previous = animator.sample(0.f).state_timer("body").value();
      auto current = animator.state_timer("body").value();
      auto previous_bob = model.bob_y_offset(0.f).value();
      auto current_bob = model.bob_y_offset(1.f).value();
      ASSERT_NE(previous, current);
      ASSERT_NE(previous_bob, current_bob);
      for (int attempt = 0; attempt < 2; ++attempt) {
        for (bool backwards : {false, true}) {
          EXPECT_FALSE(backwards ? model.set_moving_backwards(true) : model.set_state(Humanoid::Jump));
          EXPECT_EQ(model.state(), Humanoid::Run);
          EXPECT_FALSE(model.moving_backwards());
          EXPECT_EQ(model.bob_y_offset(0.f).value(), previous_bob);
          EXPECT_EQ(model.bob_y_offset(1.f).value(), current_bob);
          for (auto const& type : {String("body"), String("bodyHighPriority")}) {
            EXPECT_EQ(animator.state(type).value(), "run");
            EXPECT_FALSE(animator.state_reverse(type).value());
            EXPECT_EQ(animator.sample(0.f).state_timer(type).value(), previous);
            EXPECT_EQ(animator.state_timer(type).value(), current);
          }
        }
      }
      EXPECT_FALSE(model.set_state(Humanoid::state_size));
    }
    JsonObject repaired{{"body", JsonArray{"run", false, false}},
                        {"bodyHighPriority", JsonArray{"run", true, false}}};
    ASSERT_TRUE(model.set_humanoid_parameters(JsonObject{{"stateAnimations", JsonObject{{"jump", repaired}}},
                                                       {"stateAnimationsBackwards", JsonObject{{"run", repaired}}}}));
    ASSERT_TRUE(model.load_animation());
    auto& animator = model.networked_animator().value().get();
    ASSERT_TRUE(animator.set_local_state("body", "run"));
    ASSERT_TRUE(animator.set_local_state("bodyHighPriority", "run"));
    ASSERT_TRUE(model.animate(.05f, {}));
    auto timer = animator.state_timer("body").value();
    ASSERT_GT(timer, 0.f);
    ASSERT_TRUE(model.set_moving_backwards(true));
    EXPECT_EQ(animator.state_timer("body").value(), timer);
    EXPECT_EQ(animator.state_timer("bodyHighPriority").value(), 0.f);
    ASSERT_TRUE(model.animate(.04f, {}));
    timer = animator.state_timer("body").value();
    auto high_timer = animator.state_timer("bodyHighPriority").value();
    ASSERT_GT(high_timer, 0.f);
    ASSERT_TRUE(model.set_moving_backwards(true)); // Same direction never restarts.
    EXPECT_EQ(animator.state_timer("bodyHighPriority").value(), high_timer);
    ASSERT_TRUE(model.set_state(Humanoid::Jump)); // No backwards group: use forward.
    EXPECT_EQ(animator.state_timer("body").value(), timer);
    EXPECT_EQ(animator.state_timer("bodyHighPriority").value(), 0.f);
  }
}

static void exercise_dummy_preview(Root& root, Json const& config) {
  auto database = root.itemDatabase();
  ASSERT_TRUE(database);
  String head_name, back_name;
  for (auto const& name : (*database)->all_items()) {
    auto type = (*database)->item_type(name);
    ASSERT_TRUE(type);
    if (*type == ItemType::HeadArmor && head_name.empty())
      head_name = name;
    if (*type == ItemType::BackArmor && back_name.empty())
      back_name = name;
    if (!head_name.empty() && !back_name.empty()) break;
  }
  ASSERT_FALSE(head_name.empty() || back_name.empty());
  auto item = [&](String const& name, JsonObject parameters) {
    auto descriptor = ItemDescriptor::create(name, 1, std::move(parameters));
    return (*database)->item(descriptor.value()).value();
  };
  auto back = as<BackArmor>(item(back_name, {}));
  ASSERT_TRUE(back);
  auto gender = Gender::Male;
  auto back_image = AssetPath::split(back->frameset(gender)).base_path;
  auto shows_back = [&](List<Drawable> const& drawables) {
    return std::ranges::any_of(drawables, [&](Drawable const& d) {
      return d.is_image() && d.image_part().image.base_path == back_image;
    });
  };
  auto legacy = config.erase("animation");
  ASSERT_TRUE(legacy);
  for (bool animated : {true, false}) {
    auto made = Humanoid::create(root, animated ? config : *legacy);
    ASSERT_TRUE(made) << made.error().what();
    auto& model = **made;
    ASSERT_TRUE(model.set_identity(HumanoidIdentity{}));
    auto animator = model.networked_animator();
    ASSERT_TRUE(animator);
    // Legacy humanoids own an empty animator for the shared network path.
    ASSERT_EQ(animator->get().parts().contains("frontArm"), animated);
    auto head_a = as<HeadArmor>(item(head_name, {{"mask", "?multiply=ffbbbb"}}));
    JsonObject tags;
    if (animated)
      tags = JsonObject{{"previewProbe", "preview"}};
    auto head_b = as<HeadArmor>(item(head_name, {{"mask", "?multiply=bbbbff"}, {"humanoidAnimationTags", tags}}));
    auto bad = as<HeadArmor>(item(head_name, {{"mask", ""}, {"humanoidAnimationTags", false}}));
    ASSERT_TRUE(head_a && head_b && bad);
    ASSERT_TRUE(model.set_wearable_from_head(3, *head_a, gender));
    ASSERT_TRUE(model.set_wearable_from_back(0, *back, gender));
    ASSERT_TRUE(model.set_state(Humanoid::Walk));
    model.set_head_rotation(.21f);
    ASSERT_TRUE(model.animate(.08f, {}));
    auto real = model.render();
    ASSERT_TRUE(real) << real.error().what();
    ASSERT_TRUE(shows_back(*real));
    String real_tags;
    String tags_query = "<backCosmetic1Frameset>|<headCosmetic4Frameset>|<helmetMaskDirectives>|<hairFrameset>|<previewProbe>";
    if (animated) {
      animator->get().set_local_tag("previewProbe", String("real"));
      real_tags = animator->get().apply_part_tags("body", tags_query).value();
    }
    auto check_restored = [&] {
      EXPECT_EQ(model.state(), Humanoid::Walk);
      EXPECT_EQ(model.back_armor_frameset(), back->frameset(gender));
      if (animated) // Observe logical tags before a render could repair them.
        EXPECT_EQ(animator->get().apply_part_tags("body", tags_query).value(), real_tags);
      EXPECT_EQ(model.render().value(), *real);
    };
    auto preview_a = [&] { return model.render_dummy(gender, std::cref(*head_a), {}, {}, std::cref(*back)); };
    auto a = preview_a();
    ASSERT_TRUE(a) << a.error().what();
    EXPECT_TRUE(shows_back(*a));
    check_restored();
    auto b = model.render_dummy(gender, std::cref(*head_b));
    ASSERT_TRUE(b) << b.error().what();
    EXPECT_FALSE(shows_back(*b));
    check_restored();
    EXPECT_EQ(preview_a().value(), *a);
    check_restored();
    EXPECT_FALSE(model.render_dummy(gender, std::cref(*bad)));
    check_restored();
    if (animated) {
      // Fail after the sample received B's tags, not during armor parsing.
      auto& props = animator->get().parts().get("frontArm").value().get().part_properties;
      auto center = props.get("rotationCenter").value().get();
      props.set("rotationCenter", false);
      EXPECT_FALSE(model.render_dummy(gender, std::cref(*head_b)));
      props.set("rotationCenter", center);
      check_restored();
      EXPECT_EQ(preview_a().value(), *a);
    }
  }
}

const GameTestRegistration emote_transaction(
  "HumanoidRenderTest", "EmoteTransitionsAndForcedResetAreRetryable", [](GameTest& fixture) {
    auto directory = File::temporary_directory(fixture.root().random());
    ASSERT_TRUE(directory);
    auto cleanup = make_scope_exit([&] { EXPECT_TRUE(File::remove_all(*directory)); });
    auto root = human_animation_root(fixture, *directory);
    ASSERT_TRUE(root) << root.error().what();
    auto built = Humanoid::create(**root, HumanoidIdentity{}, JsonObject{{"useAnimation", true}});
    ASSERT_TRUE(built) << built.error().what();
    auto config = (*built)->humanoid_config(false);
    ASSERT_TRUE(config);
    exercise_movement_transitions(**root, *config);
    exercise_dummy_preview(**root, *config);
    // Try both targets as the broken one: iteration order is not a contract.
    for (bool break_body : {false, true}) {
      JsonObject group{{"body", JsonArray{break_body ? Json("missing") : Json("walk"), true, true}},
                       {"emote", JsonArray{break_body ? Json("happy") : Json("missing"), true, false}}};
      auto broken = config->set_path("emoteAnimations.happy", group);
      ASSERT_TRUE(broken);
      auto made = Humanoid::create(**root, *broken);
      ASSERT_TRUE(made) << made.error().what();
      auto& model = **made;
      ASSERT_TRUE(model.set_identity(HumanoidIdentity{}));
      ASSERT_TRUE(model.set_state(Humanoid::Run));
      ASSERT_TRUE(model.set_emote_state(HumanoidEmote::Laugh));
      ASSERT_TRUE(model.animate(.05f, {}));
      ASSERT_TRUE(model.animate(.06f, {}));
      auto handle = model.networked_animator();
      ASSERT_TRUE(handle);
      {
        auto& animator = handle->get();
        auto before = animator.sample(0.f).state_timer("emote");
        auto after = animator.state_timer("emote");
        auto body = animator.state_timer("body");
        ASSERT_TRUE(before && after && body);
        EXPECT_GT(*before, 0.f);
        EXPECT_GT(*after, *before);
        for (int attempt = 0; attempt < 2; ++attempt) {
          EXPECT_FALSE(model.set_emote_state(HumanoidEmote::Happy));
          EXPECT_EQ(model.emote_state(), HumanoidEmote::Laugh);
          EXPECT_EQ(animator.state("emote").value(), "laugh");
          EXPECT_EQ(animator.state("body").value(), "run");
          EXPECT_EQ(animator.sample(0.f).state_timer("emote").value(), *before);
          EXPECT_EQ(animator.state_timer("emote").value(), *after);
          EXPECT_EQ(animator.state_timer("body").value(), *body);
        }
        EXPECT_FALSE(model.set_emote_state(static_cast<HumanoidEmote>(-1)));
      }

      JsonObject repaired{{"body", JsonArray{"run", false, false}}, {"emote", JsonArray{"laugh", false, false}}};
      ASSERT_TRUE(model.set_humanoid_parameters(JsonObject{{"emoteAnimations", JsonObject{{"happy", repaired}}}}));
      ASSERT_TRUE(model.load_animation());
      auto& animator = model.networked_animator().value().get();
      ASSERT_TRUE(animator.set_local_state("emote", "laugh"));
      ASSERT_TRUE(model.animate(.05f, {}));
      auto timer = animator.state_timer("emote").value();
      ASSERT_GT(timer, 0.f);
      ASSERT_TRUE(model.set_emote_state(HumanoidEmote::Happy));
      EXPECT_EQ(model.emote_state(), HumanoidEmote::Happy);
      EXPECT_EQ(animator.state_timer("emote").value(), timer); // Authored startNew=false.

      // A late pose error follows both state groups and local transform writes.
      // It must not finish or restart any logical cursor, or clear a transform.
      ASSERT_TRUE(animator.translate_local_transformation_group("headRotation", {2, 3}));
      auto transform = animator.get_local_transformation_group("headRotation").value();
      auto arm = animator.parts().get("frontArm");
      ASSERT_TRUE(arm);
      auto& properties = arm->get().part_properties;
      auto center = properties.get("rotationCenter").value().get();
      properties.set("rotationCenter", false);
      EXPECT_FALSE(model.reset_animation());
      EXPECT_EQ(animator.state_timer("emote").value(), timer);
      EXPECT_EQ(animator.get_local_transformation_group("headRotation").value(), transform);
      properties.set("rotationCenter", center);
      ASSERT_TRUE(model.reset_animation());
      EXPECT_EQ(animator.sample(0.f).state_timer("emote").value(), 0.f);
      EXPECT_EQ(animator.state_timer("emote").value(), 0.f); // Forced restart even for the same state.
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
    auto config = (*baseHumanoid)->humanoid_config(false);
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
    ASSERT_TRUE(humanoid.set_identity(HumanoidIdentity{}));
    humanoid.set_facing_direction(Direction::Right);
    humanoid.set_hand_frame_overrides(ToolHand::Primary, "rotation", "rotation");
    humanoid.set_hand_parameters(ToolHand::Primary, true, 0, 0, true, false, false);
    auto firstTick = humanoid.animate(1.f / 60, {});
    ASSERT_TRUE(firstTick) << firstTick.error().what();
    auto beforeMouth = humanoid.mouth_offset();
    auto beforeArmor = humanoid.head_armor_offset();
    ASSERT_TRUE(beforeMouth);
    ASSERT_TRUE(beforeArmor);
    humanoid.set_head_rotation(1.f);
    humanoid.set_hand_parameters(ToolHand::Primary, true, 1.f, 1.f, true, false, false);
    auto secondTick = humanoid.animate(1.f / 60, {});
    ASSERT_TRUE(secondTick) << secondTick.error().what();
    auto mouth = humanoid.mouth_offset();
    auto armor = humanoid.head_armor_offset();
    ASSERT_TRUE(mouth);
    ASSERT_TRUE(armor);
    EXPECT_NE(*mouth, *beforeMouth);
    EXPECT_NE(*armor, *beforeArmor);
    auto animatorRef = humanoid.networked_animator();
    ASSERT_TRUE(animatorRef);
    auto& animator = animatorRef->get();
    auto logicalHead = animator.get_local_transformation_group("headRotation");
    auto logicalArm = animator.get_local_transformation_group("frontArmRotation");
    ASSERT_TRUE(logicalHead);
    ASSERT_TRUE(logicalArm);
    for (float alpha : {0.f, .5f, 1.f, .5f}) {
      List<LightSource> sampled_lights;
      {
        auto sample = animator.sample(alpha);
        ASSERT_TRUE(humanoid.prepare_animator_pose(sample));
        if (!fixture.check(bool(sample.append_light_sources(sampled_lights)), "append lights before rendering")
            || !fixture.check(sampled_lights.size() == 2u, "two lights before rendering"))
          return;
      }
      {
        auto sample = animator.sample(alpha);
        auto drawables = humanoid.render(sample, true, true);
        ASSERT_TRUE(drawables) << drawables.error().what();
        auto after_geometry = sample.light_sources();
        if (!fixture.check(bool(after_geometry), "query lights after rendering")
            || !fixture.check(after_geometry->size() == sampled_lights.size(), "rendering preserves the light count"))
          return;
        for (std::size_t i = 0; i != sampled_lights.size(); ++i) {
          fixture.check(std::abs((*after_geometry)[i].position[0] - sampled_lights[i].position[0]) <= 1e-5f, "rendering preserves the light x position");
          fixture.check(std::abs((*after_geometry)[i].position[1] - sampled_lights[i].position[1]) <= 1e-5f, "rendering preserves the light y position");
        }
      }
      auto preservedMouth = humanoid.mouth_offset();
      auto preservedArmor = humanoid.head_armor_offset();
      auto preservedHead = animator.get_local_transformation_group("headRotation");
      auto preservedArm = animator.get_local_transformation_group("frontArmRotation");
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
    auto metadata = (*root)->image_metadata_database();
    ASSERT_TRUE(metadata);
    HumanoidIdentity identity;
    identity.personality.head_offset = {1, 2};
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
      auto config = model.humanoid_config(false);
      ASSERT_TRUE(config);
      EXPECT_EQ(config->contains("animation"), animated);
      ASSERT_TRUE(model.set_state(Humanoid::Walk));
      for (String key : {String("armWalkSeq"), String("walkBob")}) {
        ASSERT_TRUE(model.load_config(JsonObject{{key, JsonArray{}}}));
        auto drawn = model.render(false);
        if (animated) {
          ASSERT_TRUE(drawn) << drawn.error().what();
          EXPECT_TRUE(find_sprite(*drawn, "/humanoid/human/malebody.png"));
        } else {
          EXPECT_FALSE(drawn) << key.utf8();
        }
      }
      if (!animated) {
        // Reverse arms use the arm sequence's length, but body and bob use stateFrames.
        ASSERT_TRUE(model.load_config(JsonObject{{"armWalkSeq", JsonArray{1,1,1,1,1,1,1,2,3}},
                                                 {"walkBob", JsonArray{0,0,0,0,0,0,0,5}}}));
        ASSERT_TRUE(model.set_moving_backwards(true));
        auto backwards = model.render(false);
        ASSERT_TRUE(backwards) << backwards.error().what();
        auto arm = find_sprite(*backwards, "/humanoid/human/frontarm.png");
        auto body = find_sprite(*backwards, "/humanoid/human/malebody.png");
        ASSERT_TRUE(arm);
        ASSERT_TRUE(body);
        EXPECT_EQ(arm->get().image.sub_path, "walk.3");
        EXPECT_EQ(body->get().image.sub_path, "walk.8");
        EXPECT_EQ(model.bob_y_offset(), 5.f / tile_pixels);
        ASSERT_TRUE(model.set_moving_backwards(false));
      }
      ASSERT_TRUE(model.set_state(Humanoid::Idle));
      ASSERT_TRUE(model.load_config(JsonObject{{"globalOffset", JsonArray{2, -3}}}));
      model.set_facing_direction(Direction::Left);
      model.set_scale({2, 3});
      model.set_rotation(.4f);
      auto unprefixed = model.render(true);
      ASSERT_TRUE(unprefixed) << unprefixed.error().what();
      auto alt = Drawable::make_line(Line2F({0, 0}, {1, 0}), 2, Color::palette_white, {1, 2});
      auto primary = Drawable::make_line(Line2F({0, 0}, {0, 1}), 3, Color::palette_white, {3, 4});
      model.set_hand_non_rotated_drawables(ToolHand::Alt, {alt});
      model.set_hand_non_rotated_drawables(ToolHand::Primary, {primary});
      auto prefixed = model.render(true);
      ASSERT_TRUE(prefixed) << prefixed.error().what();
      ASSERT_EQ(prefixed->size(), unprefixed->size() + 2);
      EXPECT_TRUE(std::equal(prefixed->begin() + 2, prefixed->end(), unprefixed->begin()));
      for (auto& expected : {std::ref(alt), std::ref(primary)}) {
        auto& layer = expected.get();
        if (!animated) {
          layer.translate(Vec2F(2, -3) / tile_pixels);
          layer.scale(Vec2F(2, 3));
          layer.rotate(.4f);
        }
        layer.rebase();
      }
      EXPECT_EQ((*prefixed)[0], alt);
      EXPECT_EQ((*prefixed)[1], primary);
      model.set_hand_non_rotated_drawables(ToolHand::Alt, {});
      model.set_hand_non_rotated_drawables(ToolHand::Primary, {});
      model.set_rotation(0);
      model.set_scale({1, 1});
      ASSERT_TRUE(model.load_config(JsonObject{}));
      model.set_facing_direction(Direction::Right);
      for (auto hand : {ToolHand::Primary, ToolHand::Alt}) {
        model.set_hand_frame_overrides(hand, "rotation", "rotation");
        model.set_hand_parameters(hand, true, 0, 0, false, false, false);
        model.set_hand_drawables(hand, {Drawable::make_line(Line2F({0, 0}, {1, 0}), 1, Color::palette_white)});
      }
      auto idle = model.render();
      ASSERT_TRUE(idle) << idle.error().what();
      check_pose(*idle, "idle.1", true);
      auto idle_head = find_sprite(*idle, "/humanoid/human/malehead.png");
      ASSERT_TRUE(idle_head);

      ASSERT_TRUE(model.set_dance("renderprobe"));
      ASSERT_TRUE(model.animate(.25f, {}));
      auto animator = model.networked_animator();
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
        near(delta, ((first ? Vec2F(5, -3) : Vec2F{}) - identity.personality.head_offset) / tile_pixels);
        if (first) {
          auto body = find_sprite(*shown, "/humanoid/human/malebody.png");
          ASSERT_TRUE(body);
          auto body_size = (*metadata)->image_size(body->get().image);
          ASSERT_TRUE(body_size);
          auto body_center = body->get().transformation.transform_point(Vec2F(*body_size) / 2);
          for (bool front : {false, true}) {
            auto arm = find_sprite(*shown, front ? String("/humanoid/human/frontarm.png") : String("/humanoid/human/backarm.png"));
            ASSERT_TRUE(arm);
            EXPECT_EQ(arm->get().image.sub_path, front ? String("idle.3") : String("rotation"));
            auto const& matrix = arm->get().transformation;
            float rotation = front ? .5f : -.25f;
            near(matrix.transform_direction(Vec2F(1, 0)), Vec2F(1.f / tile_pixels, 0).rotate(rotation));
            auto arm_size = (*metadata)->image_size(arm->get().image);
            ASSERT_TRUE(arm_size);
            // The offset is translated before the arm is rotated about the origin.
            auto offset = (front ? Vec2F(2, 3) : Vec2F(-3, 2)) / tile_pixels;
            near(matrix.transform_point(Vec2F(*arm_size) / 2) - body_center, offset.rotate(rotation));
          }
        }
        if (animated) {
          auto sample = animator->get().sample(alpha);
          ASSERT_TRUE(model.prepare_animator_pose(sample));
          EXPECT_EQ(sample.apply_part_tags("body", "<bodyDanceFrame>|<frontArmDanceFrame>|<backArmDanceFrame>"),
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
      ASSERT_TRUE(model.set_state(Humanoid::Duck));
      ASSERT_TRUE(model.animate(0.f, {}));
      auto filtered = model.render();
      ASSERT_TRUE(filtered);
      check_pose(*filtered, "duck.1", true);
      ASSERT_TRUE(model.set_state(Humanoid::Idle));
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

const GameTestRegistration animator_sound_retry(
  "HumanoidRenderTest", "AnimatorSoundSwitchesAndSignalOrderSurviveFailedLoads", [](GameTest& fixture) {
    auto directory = File::temporary_directory(fixture.root().random());
    if (!GameTest::check(bool(directory), "create private sound fixture")) return;
    auto cleanup = make_scope_exit([&] { GameTest::check(bool(File::remove_all(*directory)), "remove sound fixture"); });
    auto overlay = File::relative_to(*directory, "assets");
    auto sound_directory = File::relative_to(overlay, "eventprobe");
    if (!GameTest::check(bool(File::create_directories(sound_directory)), "create sound overlay")) return;
    auto write_wave = [&](String const& name, std::uint32_t frames) {
      ByteArray bytes;
      auto text = [&](std::string_view value) { bytes.append(std::span(value)); };
      auto little_endian = [&](std::uint32_t value, unsigned width) {
        for (unsigned i = 0; i < width; ++i)
          bytes.push_back(static_cast<char>(value >> (8 * i)));
      };
      text("RIFF");
      little_endian(36 + frames * 2, 4);
      text("WAVEfmt ");
      little_endian(16, 4);
      little_endian(1, 2);
      little_endian(1, 2);
      little_endian(8000, 4);
      little_endian(16000, 4);
      little_endian(2, 2);
      little_endian(16, 2);
      text("data");
      little_endian(frames * 2, 4);
      for (std::uint32_t frame = 0; frame < frames; ++frame)
        little_endian(12000, 2);
      return File::write_file(bytes, File::relative_to(sound_directory, name));
    };
    if (!GameTest::check(write_wave("base.wav", 64) && write_wave("long.wav", 128), "write distinct PCM sounds")) return;
    auto settings = fixture.rootSettings();
    settings.assetSources.push_back(overlay);
    settings.assetsSettings.missing_audio.reset();
    settings.storageDirectory = File::relative_to(*directory, "storage");
    settings.runtimeConfigFile = {};
    settings.logDirectory = {};
    settings.logFile = {};
    settings.includeUGC = false;
    auto root = Root::create(std::move(settings));
    if (!GameTest::check(bool(root), "open private asset root")) return;
    if (!GameTest::check(bool((*root)->assets()), "warm the asset source before seeding sound randomness")) return;

    auto config = Json::parse(R"({
      "animatedParts":{"stateTypes":{"motion":{"default":"old","states":{
        "old":{"properties":{
          "persistentSound":["base.wav","long.wav"],"immediateSound":["base.wav","long.wav"],
          "persistentSoundRangeMultiplier":2,"immediateSoundRangeMultiplier":3}},
        "replacement":{"properties":{
          "persistentSound":"replacement.wav","immediateSound":"late.wav",
          "persistentSoundRangeMultiplier":4,"immediateSoundRangeMultiplier":5}},
        "invalid":{"properties":{"persistentSound":"base.wav","persistentSoundStopTime":"invalid"}}
      }}}}
    })");
    if (!GameTest::check(bool(config), "parse state sound fixture")) return;
    auto made = NetworkedAnimator::create(**root, *config, "/eventprobe/animator.animation");
    if (!GameTest::check(bool(made), made ? "load state sound fixture" : made.error().what())) return;
    auto& animator = **made;
    NetworkedAnimator::DynamicTarget target;
    auto tick = [&] { return animator.update(0, std::ref(target)); };
    auto mix = [&](std::shared_ptr<AudioInstance> const& audio) {
      Mixer mixer((*root)->logger(), 8000, 1);
      std::array<std::int16_t, 8> output{};
      return mixer.play(audio) && mixer.read(output, output.size());
    };
    auto& random = (*root)->random();
    constexpr std::uint64_t seed = 0x534f554e44504f4f;
    random.seed(seed);
    RandomService expected(seed);
    StringList pool{"base.wav", "long.wav"};
    auto matches = [](std::shared_ptr<AudioInstance> const& audio, String const& selected) {
      auto total = audio->total_time();
      return total && std::abs(*total - (selected == "base.wav" ? 64.0 : 128.0) / 8000) < 1e-8;
    };
    auto persistent = expected.sample_value(pool);
    auto immediate = expected.sample_value(pool);
    if (!GameTest::check(persistent && immediate && tick(), "start both state sounds")) return;
    auto first = target.pull_new_audios();
    if (!GameTest::check(first.size() == 2 && first[0]->loops() == -1 && first[1]->loops() == 0 &&
          first[0]->range_multiplier() == 2 && first[1]->range_multiplier() == 3 &&
          matches(first[0], *persistent) && matches(first[1], *immediate),
          "persistent then immediate publish the selected relative files")) return;
    GameTest::check(random.next_u64() == expected.next_u64(), "initial sound selection uses the original RNG stream");
    auto stable_persistent = expected.sample_value(pool);
    auto stable_immediate = expected.sample_value(pool);
    if (!GameTest::check(stable_persistent && stable_immediate && tick() && target.pull_new_audios().empty(),
          "unchanged pools do not publish replacements")) return;
    GameTest::check(random.next_u64() == expected.next_u64(), "stable sound pools still consume the legacy draws");

    if (!GameTest::check(animator.set_state("motion", "replacement") && !tick() &&
          target.pull_new_audios().empty() && mix(first[0]) && !first[0]->finished(),
          "failed persistent load keeps the running voice and publishes nothing")) return;
    if (!GameTest::check(write_wave("replacement.wav", 64) && (*root)->reload() && !tick(),
          "same state retries persistent but still lacks immediate audio")) return;
    auto replacement = target.pull_new_audios();
    if (!GameTest::check(replacement.size() == 1 && replacement[0]->loops() == -1 &&
          replacement[0]->range_multiplier() == 4 && mix(first[0]) && first[0]->finished(),
          "committed replacement stops the original once")) return;
    if (!GameTest::check(!tick() && target.pull_new_audios().empty() &&
          write_wave("late.wav", 64) && (*root)->reload() && tick(),
          "unchanged failed immediate sound retries without replaying persistent")) return;
    auto late = target.pull_new_audios();
    if (!GameTest::check(late.size() == 1 && late[0]->loops() == 0 && late[0]->range_multiplier() == 5 &&
          tick() && target.pull_new_audios().empty(),
          "same immediate identity publishes only after successful preparation")) return;
    GameTest::check(animator.set_state("motion", "invalid") && !tick() &&
          target.pull_new_audios().empty() && mix(replacement[0]) && !replacement[0]->finished() &&
          animator.set_state("motion", "replacement") && tick() && target.pull_new_audios().empty(),
          "invalid stop-time cannot publish or replace a prepared live voice");

    auto independent = NetworkedAnimator::create(**root, JsonObject{
      {"sounds", JsonObject{{"voice", JsonArray{"/eventprobe/base.wav"}}}}});
    if (!GameTest::check(bool(independent), "create independent-only animator")) return;
    auto& sounds = **independent;
    NetworkedAnimator::DynamicTarget sound_target;
    if (!GameTest::check(sounds.play_sound("voice", -1) && sounds.update(0, std::ref(sound_target)),
          "start existing independent loop")) return;
    auto existing = sound_target.pull_new_audios();
    StringList interrupted_pool{"/eventprobe/base.wav", "/eventprobe/missing.wav"};
    std::optional<std::uint64_t> interrupted_seed;
    for (std::uint64_t candidate = 1; candidate <= 64; ++candidate) {
      RandomService probe(candidate);
      auto first_pick = probe.sample_value(interrupted_pool);
      auto second_pick = probe.sample_value(interrupted_pool);
      if (first_pick && second_pick && *first_pick == interrupted_pool.front() && *second_pick == interrupted_pool.back()) {
        interrupted_seed = candidate;
        break;
      }
    }
    if (!GameTest::check(existing.size() == 1 && interrupted_seed && sounds.set_sound_pool("voice", interrupted_pool),
          "choose a stream where the first play works and the next fails")) return;
    random.seed(*interrupted_seed);
    if (!GameTest::check(sounds.stop_all_sounds("voice") && sounds.play_sound("voice", -1) &&
          sounds.play_sound("voice", -1) && sounds.stop_all_sounds("voice") &&
          sounds.play_sound("voice", -1) && !sounds.update(0, std::ref(sound_target)) &&
          mix(existing[0]) && existing[0]->finished(),
          "deliver the stop and play prefix; hold the failed play and suffix")) return;
    auto prefix = sound_target.pull_new_audios();
    if (!GameTest::check(prefix.size() == 1 && mix(prefix[0]) && !prefix[0]->finished(),
          "the successful prefix is published without applying the retained stop")) return;
    if (!GameTest::check(sounds.set_sound_pool("voice", {"/eventprobe/base.wav"}) &&
          sounds.play_sound("voice", -1) && sounds.update(0, std::ref(sound_target)),
          "retry the undelivered signals before the new play")) return;
    auto played = sound_target.pull_new_audios();
    if (!GameTest::check(played.size() == 3 && mix(prefix[0]) && prefix[0]->finished() &&
          mix(played[0]) && played[0]->finished() && mix(played[1]) && !played[1]->finished() &&
          mix(played[2]) && !played[2]->finished(),
          "only the voices before the retained stop are stopped, with no duplicate prefix")) return;
    played[1]->stop();
    if (!GameTest::check(mix(played[1]) && played[1]->finished(), "finish a still-tracked independent sound")) return;
    std::weak_ptr<AudioInstance> finished = played[1];
    played[1].reset();
    if (!GameTest::check(sounds.set_sound_position("voice", {3, 5}) && sounds.update(0, std::ref(sound_target)),
          "update the remaining independent voice")) return;
    sound_target.update_position({7, 11});
    if (!GameTest::check(finished.expired() && played[2]->position() == Vec2F(10, 16) && sound_target.pull_new_audios().empty(),
          "position update releases finished voices and still tracks active ones")) return;
    played[2]->stop();
    if (!GameTest::check(mix(played[2]) && played[2]->finished(), "finish another independent voice")) return;
    finished = played[2];
    played[2].reset();
    if (!GameTest::check(sounds.play_sound("voice", -1) && sounds.update(0, std::ref(sound_target)),
          "queue another voice without a position update")) return;
    GameTest::check(sound_target.pull_new_audios().size() == 1 && finished.expired(),
        "event-only consumers release finished voices when publishing new audio");
  });

const GameTestRegistration animator_particle_bursts(
  "HumanoidRenderTest", "AnimatorParticleBurstsPreserveShuffleOrderAndCheckCycleOverflow", [](GameTest& fixture) {
    auto config = Json::parse(R"({
      "particleEmitters":{
        "picked":{"burstCount":2,"randomSelectCount":2,"particles":[
          {"particle":{"type":"text","text":"a"}},
          {"particle":{"type":"text","text":"b"},"count":2},
          {"particle":{"type":"text","text":"c"}}
        ]},
        "silent":{"randomSelectCount":0,"particles":[
          {"particle":{"type":"text","text":"d"}},
          {"particle":{"type":"text","text":"e"}}
        ]}
      }
    })");
    if (!GameTest::check(bool(config), "parse particle burst fixture")) return;
    auto made = NetworkedAnimator::create(fixture.root(), *config);
    if (!GameTest::check(bool(made), made ? "load particle burst fixture" : made.error().what())) return;
    auto& animator = **made;
    NetworkedAnimator::DynamicTarget target;
    constexpr std::uint64_t seed = 0x4255525354494458;
    auto& random = fixture.root().random();
    random.seed(seed);
    RandomService expected(seed);
    std::array<String, 3> ids{"a", "b", "c"};
    for (unsigned bursts : {2u, 1u}) {
      for (unsigned i = 0; i < bursts; ++i) {
        if (!GameTest::check(bool(animator.burst_particle_emitter("picked")), "queue a particle burst")) return;
      }
      if (!GameTest::check(animator.burst_particle_emitter("silent") && animator.update(0, std::ref(target)),
          "deliver accumulated bursts in emitter order")) return;
      StringList expected_ids;
      for (unsigned i = 0; i < 2 * bursts; ++i) {
        auto order = ids;
        expected.shuffle(order);
        for (auto const& id : std::span(order).first(2)) {
          for (unsigned copies = id == "b" ? 2 : 1; copies > 0; --copies) {
            expected_ids.push_back(id);
            std::array<float, 11> variance_draws;
            expected.fill_uniform(variance_draws);
          }
        }
      }
      std::array<unsigned, 2> silent{0, 1};
      expected.shuffle(silent);
      StringList actual_ids;
      for (auto const& emitted : target.pull_new_particles())
        actual_ids.push_back(emitted.string);
      GameTest::check(actual_ids == expected_ids, "each cycle shuffles a fresh authored order and preserves multiplicity");
      GameTest::check(random.next_u64() == expected.next_u64(), "shuffle including zero picks and particle creation preserve RNG");
    }

    constexpr unsigned max_cycles = std::numeric_limits<unsigned>::max();
    if (!GameTest::check(animator.set_particle_emitter_burst_count("picked", max_cycles / 2 + 1) &&
          animator.burst_particle_emitter("picked") && animator.burst_particle_emitter("picked"),
          "queue a burst product that would wrap to zero")) return;
    auto overflow = animator.update(0, std::ref(target));
    if (!GameTest::check(!overflow && overflow.error().is<NetworkedAnimatorException>() && target.pull_new_particles().empty(),
          "reject burst product overflow without publishing particles")) return;
    if (!GameTest::check(animator.set_particle_emitter_active("picked", true) &&
          animator.set_particle_emitter_burst_count("picked", max_cycles) && animator.burst_particle_emitter("picked"),
          "queue a burst whose addition to the first timer cycle would wrap")) return;
    overflow = animator.update(0, std::ref(target));
    if (!GameTest::check(!overflow && overflow.error().is<NetworkedAnimatorException>() && target.pull_new_particles().empty(),
          "reject burst plus timer overflow")) return;
    if (!GameTest::check(animator.set_particle_emitter_active("picked", false) &&
          animator.set_particle_emitter_burst_count("picked", 1) && animator.burst_particle_emitter("picked") &&
          animator.update(0, std::ref(target)), "schedule a valid burst after two errors")) return;
    auto count = target.pull_new_particles().size();
    GameTest::check(count == 2 || count == 3, "a later valid burst is not affected by a dropped overflow");
  });

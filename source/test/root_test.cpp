import star.json;
import star.ordered_map;
import star.bi_map;
import star.thread;
import star.io_device;
import star.list;
import star.image_processing;
import star.image;
import star.hash;
import star.data_stream;
import star.string_view;
import star.directives;
import star.asset_path;
import star.ref_ptr;
import star.logging;
import star.listener;
import std;
import star.format;

import star.asset_source;
import star.assets;
import star.root_base;
import star.configuration;
import star.root;
import star.algorithm;
import star.input;
import star.input_event;
import star.file;
import star.game_test;
import star.lua;
import star.lua_converters;
import star.lua_root;
import star.random;
import star.root_lua_bindings;
import star.string;
import star.tech_controller;
import star.tech_database;
import star.statistics_database;
import star.collision_block;
import star.game_types;
import star.map;
import star.material_database;
import star.material_types;
import star.rect;
import star.codex;
import star.codex_database;
import star.tile_modification;
import star.utility_lua_bindings;
import star.vector;
import star.world_server;
import star.object;
import star.object_database;
import star.world_tiles;
import star.networked_animator;
import star.networked_animator_lua_bindings;
import star.scripted_animator_lua_bindings;
import star.drawable;
import star.image_metadata_database;
import star.dungeon_generator;
import star.item;
import star.item_bag;
import star.item_database;
import star.inventory_types;
import star.interaction_types;
import star.player_inventory;
import star.player;
import star.player_types;
import star.humanoid;
import star.particle;
import star.player_factory;
import star.player_log;
import star.radio_message_database;
import star.npc;
import star.npc_database;
import star.net_compatibility;
import star.name_generator;
import star.species_database;
import star.item_descriptor;
import star.plant;
import star.plant_database;
import star.quest_template_database;
import star.monster_database;
import star.monster;
import star.actor_movement_controller;
import star.damage;
import star.damage_types;
import star.line;
import star.poly;
import star.stored_functions;
import star.status_controller;
import star.status_types;
import star.light_source;
import star.world_lua_bindings;
import star.lua_game_converters;
import star.lua_http_bindings;
import star.tcp;
import star.socket;
import star.host_address;
import star.curve25519;
import star.casting;
import star.entity;
import star.net_packets;
import star.test_universe;
import star.universe_client;
import star.universe_server;
import star.rpc_promise;
import star.message_types;
import star.uuid;
import star.mixer;
import star.world_client;
import star.sky;
import star.sky_parameters;
import star.sky_types;
import star.world_render_data;
import star.entity_rendering_types;
import star.warping;
import star.data_stream_devices;
import star.byte_array;
import star.buffer;
import star.data_stream_extra;
import star.encode;

import star.dance_database;
import star.versioning_database;

#include "gtest/gtest.h"

using namespace Star;

namespace {

const GameTestRegistration input_tick_state("InputBindings", "KeySnapshotsAndMousePositionsBelongToOneLogicTick", [](GameTest& fixture) {
  auto created = Input::create(fixture.root());
  ASSERT_TRUE(created) << created.error().what();
  auto& input = **created;

  input.handle_input(KeyDownEvent{Key::K, KeyMod::Caps});
  input.handle_input(KeyUpEvent{Key::K}, true);
  input.handle_input(KeyUpEvent{Key::K});
  input.handle_input(KeyDownEvent{Key::K, KeyMod::LCtrl});
  input.handle_input(KeyDownEvent{Key::K, KeyMod::LCtrl | KeyMod::Num});
  EXPECT_EQ(input.key_down(Key::K, {}), 3u);
  EXPECT_EQ(input.key_down(Key::K, KeyMod::NoMod), 1u);
  EXPECT_EQ(input.key_down(Key::K, KeyMod::Caps), 1u);
  EXPECT_EQ(input.key_down(Key::K, KeyMod::LCtrl), 2u);
  EXPECT_EQ(input.key_down(Key::K, KeyMod::LCtrl | KeyMod::Num), 1u);
  EXPECT_FALSE(input.key_down(Key::K, KeyMod::RCtrl));
  EXPECT_FALSE(input.key_down(Key::K, KeyMod::LCtrl | KeyMod::Scroll));
  EXPECT_EQ(input.key_up(Key::K), 1u);
  EXPECT_TRUE(input.key_held(Key::K));

  constexpr auto button = MouseButton::Right;
  input.handle_input(MouseButtonDownEvent{button, Vec2F(1, 2)});
  input.handle_input(MouseButtonUpEvent{button, Vec2F(3, 4)}, true);
  input.handle_input(MouseButtonUpEvent{button, Vec2F(5, 6)});
  input.handle_input(MouseButtonDownEvent{button, Vec2F(7, 8)});
  EXPECT_EQ(input.mouse_down(button).size(), 2u);
  EXPECT_EQ(input.mouse_up(button).size(), 1u);
  EXPECT_EQ(input.input_events_this_frame().size(), 9u);
  EXPECT_TRUE(input.input_events_this_frame()[1].second);
  input.finish_tick();
  EXPECT_TRUE(input.key_held(Key::K));
  EXPECT_FALSE(input.key_down(Key::K, {}));
  EXPECT_FALSE(input.key_up(Key::K));
  EXPECT_TRUE(input.mouse_held(button));
  EXPECT_TRUE(input.mouse_down(button).empty());
  EXPECT_TRUE(input.mouse_up(button).empty());
  EXPECT_TRUE(input.input_events_this_frame().empty());
  input.handle_input(MouseButtonUpEvent{button, Vec2F(9, 10)});
  input.handle_input(MouseButtonDownEvent{button, Vec2F(11, 12)});
  ASSERT_EQ(input.mouse_up(button).size(), 1u);
  EXPECT_EQ(input.mouse_up(button).front(), Vec2F(9, 10));
  ASSERT_EQ(input.mouse_down(button).size(), 1u);
  EXPECT_EQ(input.mouse_down(button).front(), Vec2F(11, 12));
  EXPECT_EQ(input.mouse_position(), Vec2F(11, 12));

  input.set_text_input_active(true);
  input.handle_input(KeyDownEvent{Key::X, KeyMod::NoMod});
  input.handle_input(KeyUpEvent{Key::K}, true);
  EXPECT_FALSE(input.key_down(Key::X, {}));
  EXPECT_FALSE(input.key_held(Key::K));
  EXPECT_EQ(input.key_up(Key::K), 1u);
});

const GameTestRegistration input_source_attribution("InputBindings", "PressAttributionSurvivesRepeatsTiesAndOtherDevices", [](GameTest& fixture) {
  auto config = fixture.root().configuration();
  ASSERT_TRUE(config);
  auto previous = (*config)->get("modBindings");
  ASSERT_TRUE(previous);
  auto restore = make_scope_exit([&] { EXPECT_TRUE((*config)->set("modBindings", *previous)); });
  Json chord = JsonObject{{"type", "key"}, {"value", "A"}, {"mods", JsonArray{"LCtrl"}}};
  Json alternate = JsonObject{{"type", "key"}, {"value", "A"}, {"mods", JsonArray{"LShift"}}};
  Json mouse = JsonObject{{"type", "mouse"}, {"value", "MouseLeft"}, {"mods", JsonArray{"LCtrl"}}};
  Json pad = JsonObject{{"type", "controller"}, {"value", "X"}, {"controller", 3}};
  Json other_pad = JsonObject{{"type", "controller"}, {"value", "X"}, {"controller", 4}};
  Json plain = JsonObject{{"type", "key"}, {"value", "A"}};
  // Legacy mod-binding selection ignores lock state even for a named lock.
  Json other_key = JsonObject{{"type", "key"}, {"value", "B"}, {"mods", JsonArray{"Caps"}}};
  ASSERT_TRUE((*config)->set("modBindings", JsonObject{{"opensb", JsonObject{
    {"editingCopyItemJson", JsonArray{chord, chord, alternate, mouse, pad}},
    {"zoomIn", JsonArray{chord, other_pad}}, {"zoomOut", JsonArray{plain, other_key}}}}}));
  auto created = Input::create(fixture.root());
  ASSERT_TRUE(created) << created.error().what();
  auto& input = **created;
  String const category = "opensb";
  String const action = "editingCopyItemJson";
  input.handle_input(KeyDownEvent{Key::B, KeyMod::NoMod});
  input.handle_input(KeyDownEvent{Key::A, KeyMod::LCtrl | KeyMod::LShift});
  input.handle_input(KeyDownEvent{Key::A, KeyMod::NoMod});
  EXPECT_EQ(input.bind_down(category, action), 2u);
  EXPECT_EQ(input.bind_down(category, "zoomIn"), 2u);
  EXPECT_EQ(input.bind_down(category, "zoomOut"), 1u);
  EXPECT_EQ(input.get_tag("clipboard"), 1u);
  input.handle_input(ControllerButtonDownEvent{3, ControllerButton::X});
  input.handle_input(KeyUpEvent{Key::A}, true);
  input.handle_input(KeyUpEvent{Key::A});
  EXPECT_TRUE(input.bind_held(category, action));
  EXPECT_EQ(input.bind_up(category, action), 1u);
  EXPECT_FALSE(input.bind_held(category, "zoomIn"));
  EXPECT_TRUE(input.bind_held(category, "zoomOut"));
  EXPECT_FALSE(input.bind_up(category, "zoomOut"));

  input.finish_tick();
  input.handle_input(ControllerButtonDownEvent{4, ControllerButton::X});
  input.handle_input(ControllerButtonUpEvent{4, ControllerButton::X}, true);
  input.handle_input(ControllerButtonUpEvent{4, ControllerButton::X});
  EXPECT_TRUE(input.bind_held(category, action));
  EXPECT_FALSE(input.bind_up(category, action));
  EXPECT_EQ(input.bind_up(category, "zoomIn"), 1u);
  input.handle_input(ControllerButtonUpEvent{3, ControllerButton::X});
  EXPECT_FALSE(input.bind_held(category, action));
  EXPECT_EQ(input.bind_up(category, action), 1u);
  EXPECT_TRUE(input.clipboard_allowed());
  input.finish_tick();
  EXPECT_FALSE(input.clipboard_allowed());

  input.handle_input(KeyDownEvent{Key::LCtrl, KeyMod::LCtrl}, true);
  input.handle_input(MouseButtonDownEvent{MouseButton::Left, {}});
  EXPECT_TRUE(input.clipboard_allowed());
  ASSERT_TRUE(input.set_binds(category, action, JsonArray{mouse}));
  EXPECT_FALSE(input.clipboard_allowed());
  EXPECT_FALSE(input.mouse_held(MouseButton::Left));
  EXPECT_EQ(input.input_events_this_frame().size(), 2u);
  input.handle_input(MouseButtonDownEvent{MouseButton::Left, {}});
  EXPECT_TRUE(input.bind_held(category, action));
  input.clear_input_state();
  EXPECT_FALSE(input.clipboard_allowed());
  {
    auto unlock = input.unlock_clipboard();
    input.clear_input_state();
    EXPECT_TRUE(input.clipboard_allowed());
  }
  EXPECT_FALSE(input.clipboard_allowed());
  EXPECT_TRUE(input.input_events_this_frame().empty());
  input.handle_input(MouseButtonDownEvent{MouseButton::Left, {}});
  EXPECT_FALSE(input.bind_down(category, action));
});

class RootIsolationErrorSink : public LogSink {
public:
  RootIsolationErrorSink() { set_level(LogLevel::Error); }
  [[nodiscard]] std::expected<void, StarException> log(std::string_view message, LogLevel) override {
    ADD_FAILURE() << "Error was logged: " << message;
    return {};
  }
};

TEST(ConfigurationTest, ProtectsOnlyTheParsedVersionRoot) {
  Json initial = JsonObject{
    {"configurationVersion", JsonObject{{"client", 1}}},
    {"nested", JsonObject{}}
  };
  Configuration config(initial, initial);
  EXPECT_EQ(config.get_path(""), initial);
  EXPECT_FALSE(config.set("configurationVersion", {}));
  for (String const& path : {String(""), String("configurationVersion"), String("configurationVersion.client")}) {
    SCOPED_TRACE(path.utf8());
    EXPECT_FALSE(config.set_path(path, 2));
    EXPECT_FALSE(config.set_path(path, {}));
  }
  for (String const& path : {String(".nested"), String("nested..field"), String("nested[not-an-index]")}) {
    auto expected_error = initial.set_path(path, 2);
    ASSERT_FALSE(expected_error);
    auto result = config.set_path(path, 2);
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().what(), expected_error.error().what());
  }
  EXPECT_EQ(config.snapshot().revision, 0u);
  EXPECT_EQ(config.current_configuration(), initial);

  ASSERT_TRUE(config.set_path("configurationVersion]extra", false));
  EXPECT_EQ(config.get("configurationVersion]extra"), Json(false));
  ASSERT_TRUE(config.set_path("nested.configurationVersion", 0));
  EXPECT_EQ(config.get_path("nested.configurationVersion"), Json(0));
  ASSERT_TRUE(config.set("configurationVersion.client", 2));
  EXPECT_EQ(config.get_path("configurationVersion.client"), Json(1));
  EXPECT_EQ(config.get("configurationVersion.client"), Json(2));
  ASSERT_TRUE(config.set("", 3));
  EXPECT_EQ(config.get(""), Json(3));
  ASSERT_TRUE(config.set("", {}));
  EXPECT_EQ(config.get("", 4), Json(4));
}

TEST(ConfigurationTest, RetainsSnapshotsAndCommitsOnlySuccessfulWrites) {
  Json initial = JsonObject{{"nullable", Json{}}, {"items", JsonArray{1}}};
  Configuration config(initial, initial);
  auto original = config.snapshot();
  EXPECT_EQ(config.get("nullable", 2), Json{});
  EXPECT_EQ(config.get_path("nullable", 2), Json{});
  EXPECT_EQ(config.get("missing", 2), Json(2));
  EXPECT_EQ(config.get_path("items[9]", 2), Json(2));

  ASSERT_TRUE(config.set_path("items[0]", 0));
  auto changed = config.snapshot();
  EXPECT_EQ(changed.revision, original.revision + 1);
  EXPECT_EQ(changed.value.query("items[0]"), Json(0));
  ASSERT_TRUE(config.set_path("items[0]", {}));
  ASSERT_TRUE(config.set("missing", {}));
  EXPECT_FALSE(config.set_path("missing", {}));
  auto after_erases = config.snapshot();
  EXPECT_EQ(after_erases.revision, changed.revision + 2);
  EXPECT_EQ(after_erases.value.get("items"), Json(JsonArray{}));
  EXPECT_EQ(changed.value.query("items[0]"), Json(0));
  EXPECT_EQ(original.value, initial);
  EXPECT_EQ(config.get_default("items"), Json(JsonArray{1}));
}

const GameTestRegistration light_source_defaults("LightSourceTest", "SafeAggregateDefaults", [](GameTest& fixture) {
  static_assert(std::is_aggregate_v<LightSource>);
  LightSource light_source;
  fixture.check(light_source.position == Vec2F(), "default position is the origin");
  fixture.check(light_source.color == Vec3F(), "default color is black");
  fixture.check(light_source.type == LightType::Spread, "default light type is Spread");
  fixture.check(light_source.point_beam == 0.0f, "default beam strength is zero");
  fixture.check(light_source.beam_angle == 0.0f, "default beam angle is zero");
  fixture.check(light_source.beam_ambience == 0.0f, "default beam ambience is zero");
});

const GameTestRegistration light_source_wire("LightSourceTest", "LegacyWireAndTruncatedRead", [](GameTest& fixture) {
  static_assert(stream_readable<LightSource> && stream_writable<LightSource>);

  auto same_light_source = [](LightSource const& actual, LightSource const& expected) {
    return actual.position == expected.position && actual.color == expected.color && actual.type == expected.type
        && actual.point_beam == expected.point_beam && actual.beam_angle == expected.beam_angle
        && actual.beam_ambience == expected.beam_ambience;
  };

  // Two position floats, three color floats, one type byte, then three beam floats.
  LightSource light_source{Vec2F(1.0f, -2.0f), Vec3F(0.25f, 0.5f, 0.75f), LightType::Spread, 2.0f, -1.0f, 0.125f};
  auto check_wire = [&](LightType type, std::string_view hex) {
    light_source.type = type;
    auto legacy_wire = hex_decode(hex);
    auto encoded = DataStreamBuffer::serialize(light_source);
    if (!encoded) {
      fixture.check(false, encoded.error().what());
      return;
    }
    fixture.check(*encoded == legacy_wire, "encoded light source matches legacy bytes");
    auto decoded = DataStreamBuffer::deserialize<LightSource>(legacy_wire.span());
    if (!decoded) {
      fixture.check(false, decoded.error().what());
      return;
    }
    fixture.check(same_light_source(*decoded, light_source), "legacy light source decodes all six fields");
  };
  check_wire(LightType::Spread, "3f800000c00000003e8000003f0000003f4000000040000000bf8000003e000000");
  check_wire(LightType::Point, "3f800000c00000003e8000003f0000003f4000000140000000bf8000003e000000");
  check_wire(LightType::PointAsSpread, "3f800000c00000003e8000003f0000003f4000000240000000bf8000003e000000");

  auto legacy_wire = hex_decode("3f800000c00000003e8000003f0000003f4000000240000000bf8000003e000000");
  LightSource const original{Vec2F(-4.0f, 8.0f), Vec3F(0.75f, 0.25f, 0.5f), LightType::Point, 3.0f, 1.0f, 0.5f};
  for (std::size_t size = 0; size < legacy_wire.size(); ++size) {
    auto retained = original;
    if (DataStreamBuffer::deserialize(retained, legacy_wire.span().first(size)))
      fixture.check(false, std::format("truncated legacy prefix of {} bytes was accepted", size));
    if (!same_light_source(retained, original))
      fixture.check(false, std::format("truncated legacy prefix of {} bytes changed the destination", size));
  }
});

const GameTestRegistration light_source_config("LightSourceTest", "LegacyAssetNamesAndFallback", [](GameTest& fixture) {
  fixture.check(parse_light_type(JsonObject{}) == LightType::Spread, "missing light type defaults to Spread");
  fixture.check(parse_light_type(JsonObject{{"pointLight", false}}) == LightType::Spread, "pointLight false selects Spread");
  fixture.check(parse_light_type(JsonObject{{"pointLight", true}}) == LightType::Point, "pointLight true selects Point");
  fixture.check(parse_light_type(JsonObject{{"lightType", "Spread"}, {"pointLight", true}}) == LightType::Spread,
      "explicit Spread takes precedence over pointLight true");
  fixture.check(parse_light_type(JsonObject{{"lightType", "Point"}}) == LightType::Point, "explicit Point is accepted");
  fixture.check(parse_light_type(JsonObject{{"lightType", "PointAsSpread"}, {"pointLight", "ignored"}}) == LightType::PointAsSpread,
      "explicit PointAsSpread takes precedence over malformed pointLight");
  fixture.check(!parse_light_type(JsonObject{{"lightType", "unknown"}, {"pointLight", true}}),
      "unknown light type does not fall back to pointLight");
  fixture.check(!parse_light_type(JsonObject{{"lightType", false}}), "non-string light type is rejected");
  fixture.check(!parse_light_type(JsonObject{{"pointLight", "invalid"}}), "non-bool pointLight is rejected");
});

const GameTestRegistration warp_target_wire("WarpTargetTest", "LegacyWireAndJson", [](GameTest&) {
  // WorldId monostate, SpawnTarget alternative byte, then big-endian payload.
  auto check = [](SpawnTarget const& target, std::string_view hex) {
    WarpToWorld warp({}, target);
    auto bytes = DataStreamBuffer::serialize(warp);
    ASSERT_TRUE(bytes);
    auto expected = hex_decode(hex);
    EXPECT_EQ(*bytes, expected);
    auto decoded = DataStreamBuffer::deserialize<WarpToWorld>(expected.span());
    ASSERT_TRUE(decoded);
    EXPECT_EQ(*decoded, warp);
    auto json_target = spawn_target_from_json(spawn_target_to_json(target));
    ASSERT_TRUE(json_target);
    EXPECT_EQ(*json_target, target);
  };
  check({}, "0000");
  check(SpawnTargetUniqueEntity(String("abc")), "000103616263");
  check(SpawnTargetPosition(Vec2F(1, -2)), "00023f800000c0000000");
  check(SpawnTargetX{1.5f}, "00033fc00000");

  WarpToWorld retained({}, SpawnTargetX{-2.f});
  auto truncated = hex_decode("00033fc0");
  EXPECT_FALSE(DataStreamBuffer::deserialize(retained, truncated.span()));
  EXPECT_EQ(retained.target, SpawnTarget(SpawnTargetX{-2.f}));

  auto swap = DataStreamBuffer::serialize(InventorySlot(SwapSlot{}));
  auto trash = DataStreamBuffer::serialize(InventorySlot(TrashSlot{}));
  ASSERT_TRUE(swap);
  ASSERT_TRUE(trash);
  EXPECT_EQ(*swap, hex_decode("02"));
  EXPECT_EQ(*trash, hex_decode("03"));
});

TEST(InventorySlotTest, NamesIndicesAndLegacyWire) {
  auto wire = [](InventorySlot const& slot, std::string_view hex) {
    auto bytes = DataStreamBuffer::serialize(slot);
    ASSERT_TRUE(bytes);
    auto expected = hex_decode(hex);
    EXPECT_EQ(*bytes, expected);
    EXPECT_EQ(DataStreamBuffer::deserialize<InventorySlot>(expected.span()), slot);
  };
  wire(EquipmentSlot::Cosmetic12, "0013");
  wire(BagSlot{"bag", 255}, "0103626167ff");
  wire(SwapSlot{}, "02");
  wire(TrashSlot{}, "03");
  CustomBarLink link{TrashSlot{}, BagSlot{"bag", 255}};
  auto bytes = DataStreamBuffer::serialize(link);
  ASSERT_TRUE(bytes);
  EXPECT_EQ(*bytes, hex_decode("0103010103626167ff"));
  EXPECT_EQ(DataStreamBuffer::deserialize<CustomBarLink>(bytes->span()), link);
  EXPECT_LT((BagSlot{"a", 255}), (BagSlot{"b", 0}));
  EXPECT_LT((BagSlot{"a", 0}), (BagSlot{"a", 1}));

  for (auto const& [value, name] : equipment_slot_names) {
    Json encoded = JsonObject{{"type", "equipment"}, {"location", name}};
    EXPECT_EQ(json_to_inventory_slot(encoded), InventorySlot(value));
    EXPECT_EQ(json_from_inventory_slot(value), encoded);
  }
  for (auto const& [value, name] : essential_item_names) {
    EXPECT_EQ(json_to_selected_action_bar_location(Json(name)), SelectedActionBarLocation(value));
    EXPECT_EQ(json_from_selected_action_bar_location(value), Json(name));
  }
  EXPECT_EQ(enum_value<equipment_slot_names>("hEaDcOsMeTiC"), EquipmentSlot::HeadCosmetic);
  EXPECT_EQ(enum_value<essential_item_names>("beamAxe"), EssentialItem::BeamAxe);
  EXPECT_FALSE(enum_value<equipment_slot_names>("unknown"));
  EXPECT_FALSE(maybe_enum_value<equipment_slot_names>("head\xff"));
  EXPECT_FALSE(maybe_enum_value<essential_item_names>("\xff"));
  EXPECT_EQ(json_to_inventory_slot(JsonObject{{"type", "swap"}, {"location", false}}), InventorySlot(SwapSlot{}));
  EXPECT_EQ(json_to_inventory_slot(JsonObject{{"type", "trash"}}), InventorySlot(TrashSlot{}));
  for (String const& name : {String("swap"), String("trash"), String("equipment")}) {
    EXPECT_TRUE(is_reserved_bag_name(name));
    EXPECT_FALSE(json_from_inventory_slot(BagSlot{name, 0}));
  }
  EXPECT_FALSE(is_reserved_bag_name("Swap"));
  for (auto index : {0, 255}) {
    Json encoded = JsonObject{{"type", "bag"}, {"location", index}};
    InventorySlot slot = BagSlot{"bag", static_cast<std::uint8_t>(index)};
    EXPECT_EQ(json_from_inventory_slot(slot), encoded);
    EXPECT_EQ(json_to_inventory_slot(encoded), slot);
    EXPECT_EQ(json_to_selected_action_bar_location(Json(index)), SelectedActionBarLocation(static_cast<CustomBarIndex>(index)));
  }
  for (Json const& index : {Json(-1), Json(256), Json(std::int64_t{4294967296}), Json(1.5), Json(-0.5), Json(1e100)}) {
    EXPECT_FALSE(json_to_inventory_slot(JsonObject{{"type", "bag"}, {"location", index}}));
    EXPECT_FALSE(json_to_selected_action_bar_location(index));
  }
}

TEST(InventorySlotTest, RealLuaSlotConversion) {
  auto engine = LuaEngine::create();
  ASSERT_TRUE(engine);
  auto context = (*engine)->create_context();
  ASSERT_TRUE(context);
  auto bag = InventorySlot(BagSlot{"trash", 255});
  ASSERT_TRUE(context->set("bag", bag));
  EXPECT_EQ(context->eval<bool>("bag[1] == 'trash' and bag[2] == 255"), true);
  EXPECT_EQ(context->eval<InventorySlot>("bag"), bag);
  EXPECT_EQ(context->eval<InventorySlot>("{'items', 0}"), InventorySlot(BagSlot{"items", 0}));
  EXPECT_EQ(context->eval<InventorySlot>("'hEaD'"), InventorySlot(EquipmentSlot::Head));
  EXPECT_EQ(context->eval<InventorySlot>("'SwAp'"), InventorySlot(SwapSlot{}));
  EXPECT_EQ(context->eval<InventorySlot>("'TRASH'"), InventorySlot(TrashSlot{}));
  EXPECT_FALSE(context->eval<InventorySlot>("'unknown'"));
  EXPECT_FALSE(context->eval<InventorySlot>("string.char(255)"));
  for (std::string_view script : {"{'bag', -1}", "{'bag', 256}", "{'bag', 4294967296}", "{'bag', 1.5}", "{'bag', 1e100}",
                                  "{'bag', 0/0}", "{'bag', math.huge}", "{'bag', '255'}"})
    EXPECT_FALSE(context->eval<InventorySlot>(script)) << script;
}

const GameTestRegistration dungeon_part_offsets("DungeonPartTest", "OutpostOffsetsMatchOrderedTileEvents", [](GameTest& fixture) {
  auto definitions = fixture.root().dungeonDefinitions();
  ASSERT_TRUE(definitions) << definitions.error().what();
  auto definition = (*definitions)->get("outpost");
  ASSERT_TRUE(definition) << definition.error().what();
  std::size_t occupied_count = 0;
  for (auto const& [name, part] : (*definition)->parts()) {
    SCOPED_TRACE(name.utf8());
    List<Vec2I> occupied;
    List<Vec2I> modified;
    auto visited = part->forEachTile([&](Vec2I pos, Dungeon::Tile const& tile) -> std::expected<bool, StarException> {
      if (tile.usesPlaces())
        occupied.push_back(pos);
      if (tile.modifiesPlaces())
        modified.push_back(pos);
      return false;
    });
    ASSERT_TRUE(visited) << visited.error().what();
    EXPECT_TRUE(std::ranges::equal(occupied, part->occupied_offsets()));
    EXPECT_TRUE(std::ranges::equal(modified, part->modified_offsets()));
    occupied_count += occupied.size();
  }
  EXPECT_GT(occupied_count, 0u);
});

const GameTestRegistration rootAll("RootTest", "All", [](GameTest& fixture) {
  auto& root = fixture.root();

  auto expectCatalogs = [&](char const* phase) {
    SCOPED_TRACE(phase);
    auto expectCatalog = [](char const* name, auto result) {
      SCOPED_TRACE(name);
      ASSERT_TRUE(result) << result.error().what();
      EXPECT_TRUE(*result);
    };
    ASSERT_NO_FATAL_FAILURE(expectCatalog("assets", root.assets()));
    ASSERT_NO_FATAL_FAILURE(expectCatalog("objectDatabase", root.object_database()));
    ASSERT_NO_FATAL_FAILURE(expectCatalog("plant_database", root.plant_database()));
    ASSERT_NO_FATAL_FAILURE(expectCatalog("projectileDatabase", root.projectileDatabase()));
    ASSERT_NO_FATAL_FAILURE(expectCatalog("monsterDatabase", root.monster_database()));
    ASSERT_NO_FATAL_FAILURE(expectCatalog("npcDatabase", root.npc_database()));
    ASSERT_NO_FATAL_FAILURE(expectCatalog("playerFactory", root.playerFactory()));
    ASSERT_NO_FATAL_FAILURE(expectCatalog("entityFactory", root.entityFactory()));
    ASSERT_NO_FATAL_FAILURE(expectCatalog("nameGenerator", root.name_generator()));
    ASSERT_NO_FATAL_FAILURE(expectCatalog("itemDatabase", root.itemDatabase()));
    ASSERT_NO_FATAL_FAILURE(expectCatalog("materialDatabase", root.material_database()));
    ASSERT_NO_FATAL_FAILURE(expectCatalog("terrainDatabase", root.terrainDatabase()));
    ASSERT_NO_FATAL_FAILURE(expectCatalog("biome_database", root.biome_database()));
    ASSERT_NO_FATAL_FAILURE(expectCatalog("liquids_database", root.liquids_database()));
    ASSERT_NO_FATAL_FAILURE(expectCatalog("statusEffectDatabase", root.statusEffectDatabase()));
    ASSERT_NO_FATAL_FAILURE(expectCatalog("damageDatabase", root.damageDatabase()));
    ASSERT_NO_FATAL_FAILURE(expectCatalog("particle_database", root.particle_database()));
    ASSERT_NO_FATAL_FAILURE(expectCatalog("effectSourceDatabase", root.effectSourceDatabase()));
    ASSERT_NO_FATAL_FAILURE(expectCatalog("functionDatabase", root.functionDatabase()));
    ASSERT_NO_FATAL_FAILURE(expectCatalog("treasureDatabase", root.treasureDatabase()));
    ASSERT_NO_FATAL_FAILURE(expectCatalog("dungeonDefinitions", root.dungeonDefinitions()));
    ASSERT_NO_FATAL_FAILURE(expectCatalog("emoteProcessor", root.emoteProcessor()));
    ASSERT_NO_FATAL_FAILURE(expectCatalog("speciesDatabase", root.speciesDatabase()));
    ASSERT_NO_FATAL_FAILURE(expectCatalog("image_metadata_database", root.image_metadata_database()));
    ASSERT_NO_FATAL_FAILURE(expectCatalog("versioningDatabase", root.versioningDatabase()));
    ASSERT_NO_FATAL_FAILURE(expectCatalog("questTemplateDatabase", root.questTemplateDatabase()));
    ASSERT_NO_FATAL_FAILURE(expectCatalog("aiDatabase", root.ai_database()));
    ASSERT_NO_FATAL_FAILURE(expectCatalog("techDatabase", root.techDatabase()));
    ASSERT_NO_FATAL_FAILURE(expectCatalog("codexDatabase", root.codex_database()));
    ASSERT_NO_FATAL_FAILURE(expectCatalog("stagehandDatabase", root.stagehandDatabase()));
    ASSERT_NO_FATAL_FAILURE(expectCatalog("behavior_database", root.behavior_database()));
    ASSERT_NO_FATAL_FAILURE(expectCatalog("tenantDatabase", root.tenantDatabase()));
    ASSERT_NO_FATAL_FAILURE(expectCatalog("dance_database", root.dance_database()));
    ASSERT_NO_FATAL_FAILURE(expectCatalog("spawnTypeDatabase", root.spawnTypeDatabase()));
  };
  ASSERT_NO_FATAL_FAILURE(expectCatalogs("before reload"));

  ASSERT_TRUE(root.reload());

  ASSERT_NO_FATAL_FAILURE(expectCatalogs("after reload"));
});

const GameTestRegistration codex_catalog("CodexCatalog", "EmptyDefaultsRelativeIconsAndDuplicateSource", [](GameTest& fixture) {
  auto directory = File::temporary_directory(fixture.root().random());
  ASSERT_TRUE(directory);
  auto cleanup = make_scope_exit([&] { EXPECT_TRUE(File::remove_all(*directory)); });
  auto path = [&](String const& name) { return File::relative_to(*directory, name); };
  ASSERT_TRUE(File::create_directories(path("books")));
  ASSERT_TRUE(File::write_file(String("[]"), path("preload.config")));
  ASSERT_TRUE(File::write_file(String("{}"), path("codex.config")));
  Logger logger;
  auto load_catalog = [&]() -> std::expected<std::shared_ptr<CodexDatabase>, StarException> {
    Assets::Settings settings{};
    settings.worker_pool_size = 1;
    auto assets = Assets::create(logger, fixture.root().random(), fixture.root().logMap(), settings, {*directory});
    if (!assets)
      return std::unexpected(std::move(assets.error()));
    return CodexDatabase::create(**assets);
  };
  auto empty = load_catalog();
  ASSERT_TRUE(empty) << empty.error().what();
  EXPECT_TRUE((*empty)->entries().empty());

  String book(R"({"id":"sample","title":"Sample","contentPages":["one","two"]})");
  ASSERT_TRUE(File::write_file(book, path("books/first.codex")));
  auto missing_default = load_catalog();
  ASSERT_FALSE(missing_default);
  EXPECT_NE(std::string_view(missing_default.error().what()).find("/codex.config"), std::string_view::npos);
  ASSERT_TRUE(File::write_file(String(R"({"defaultIcon":"images/book.png"})"), path("codex.config")));
  auto catalog = load_catalog();
  ASSERT_TRUE(catalog) << catalog.error().what();
  auto held = (*catalog)->find("sample");
  ASSERT_TRUE(held);
  EXPECT_EQ(held->icon(), String("/books/images/book.png"));
  EXPECT_EQ(held->page(1), String("two"));

  ASSERT_TRUE(File::write_file(book, path("books/duplicate.codex")));
  auto duplicate = load_catalog();
  ASSERT_FALSE(duplicate);
  auto message = std::string_view(duplicate.error().what());
  EXPECT_NE(message.find("sample"), std::string_view::npos);
  EXPECT_NE(message.find(".codex"), std::string_view::npos);
});

[[nodiscard]] std::expected<void, StarException> writeIsolationAssets(String const& overlay, String const& owner) {
  auto config = Json(JsonObject{{"owner", owner}}).repr();
  if (!config) return std::unexpected(std::move(config.error()));
  auto writeConfig = File::write_file(*config, File::relative_to(overlay, "root-isolation.config"));
  if (!writeConfig) return std::unexpected(std::move(writeConfig.error()));
  auto tech = Json(JsonObject{
      {"name", "root-isolation"},
      {"type", "Head"},
      {"scripts", JsonArray{}},
      {"description", owner},
      {"shortDescription", owner},
      {"rarity", "common"},
      {"icon", "/interface/missing.png"}
    }).repr();
  if (!tech) return std::unexpected(std::move(tech.error()));
  auto writeTech = File::write_file(*tech, File::relative_to(overlay, "root-isolation.tech"));
  if (!writeTech) return std::unexpected(std::move(writeTech.error()));
  auto quotedOwner = Json(owner).repr();
  if (!quotedOwner) return std::unexpected(std::move(quotedOwner.error()));
  return File::write_file("function isolationScriptOwner() return " + *quotedOwner + " end",
                          File::relative_to(overlay, "root-isolation.lua"));
}

[[nodiscard]] std::expected<Root::Settings, StarException> isolation_settings(
    Root::Settings const& base, String const& directory, String const& owner) {
  auto settings = base;
  auto overlay = File::relative_to(directory, "assets");
  auto make_overlay = File::create_directory(overlay);
  if (!make_overlay) return std::unexpected(std::move(make_overlay.error()));
  auto write_assets = writeIsolationAssets(overlay, owner);
  if (!write_assets) return std::unexpected(std::move(write_assets.error()));
  // Keep the fixture's legal base pak and production mod sources; only the
  // test-owned overlay, runtime configuration, storage, and logs are private.
  settings.assetSources.push_back(overlay);
  // Isolate explicit reload invalidation from maintenance-thread TTL eviction
  // while loading the real pak on slower test hosts.
  settings.assetsSettings.asset_time_to_live = std::numeric_limits<float>::max();
  settings.storageDirectory = File::relative_to(directory, "storage");
  settings.runtimeConfigFile = String("root-isolation.config");
  settings.logDirectory = {};
  settings.logFile = {};
  settings.includeUGC = false;
  auto configuration = settings.defaultConfiguration.set("rootIsolationOwner", owner);
  if (!configuration) return std::unexpected(std::move(configuration.error()));
  settings.defaultConfiguration = std::move(*configuration);
  return settings;
}

const GameTestRegistration tech_sync("TechControllerRegression", "StableEquipmentAndForcedOverrideRestoration", [](GameTest& fixture) {
  auto directory = File::temporary_directory(fixture.root().random());
  if (!fixture.check(bool(directory), "create tech test directory")) return;
  auto cleanup = make_scope_exit([&] { (void)File::remove_all(*directory); });
  auto settings = isolation_settings(fixture.rootSettings(), *directory, "tech-sync");
  if (!fixture.check(bool(settings), "prepare isolated tech assets")) return;
  auto body_path = File::relative_to(*directory, "assets/tech-sync-body.tech");
  auto body_asset = File::write_file(String(R"({"name":"tech-sync-body","type":"Body","scripts":[],
    "description":"Body fixture","shortDescription":"Body fixture","rarity":"common","icon":"/interface/missing.png"})"), body_path);
  if (!fixture.check(bool(body_asset), "write body tech asset")) return;
  auto root = Root::create(std::move(*settings));
  if (!fixture.check(bool(root), "create tech test root")) return;
  auto created = TechController::create(**root);
  if (!fixture.check(bool(created), "create tech controller")) return;
  auto& controller = **created;

  HashMap<TechType, String> equipped{{TechType::Head, "root-isolation"}, {TechType::Body, "tech-sync-body"}};
  auto make_save = [](StringList const& names, JsonObject const& storage) {
    JsonArray modules;
    for (auto const& name : names)
      modules.emplace_back(JsonObject{{"module", name}, {"scriptData", storage}});
    return Json(JsonObject{{"techModules", std::move(modules)}});
  };
  auto expected_order = equipped.values();
  auto saved = make_save(expected_order, JsonObject{{"sentinel", 42}});
  if (!fixture.check(bool(controller.disk_load(saved)), "restore tech storage")) return;
  if (!fixture.check(bool(controller.sync_equipped_tech(equipped)), "synchronize unchanged equipment")) return;
  fixture.check(controller.disk_store() == saved, "stable synchronization preserves order and script storage");

  auto restored = make_save(expected_order, {});
  if (!fixture.check(bool(controller.reload_tech()), "force reload unchanged equipment")) return;
  fixture.check(controller.disk_store() == restored, "forced reload clears script storage");
  if (!fixture.check(bool(controller.set_override_tech({"root-isolation"})), "activate tech override")) return;
  if (!fixture.check(bool(controller.set_override_tech({"tech-sync-body"})), "replace tech override")) return;
  equipped.clear();
  if (!fixture.check(bool(controller.sync_equipped_tech(equipped)), "ignore equipment changes under override")) return;
  if (!fixture.check(bool(controller.clear_override_tech()), "clear tech override")) return;
  fixture.check(!controller.tech_overridden() && controller.disk_store() == restored, "clear restores original ordered equipment");
  if (!fixture.check(bool(controller.sync_equipped_tech(equipped)), "apply equipment changes after restoration")) return;
  fixture.check(controller.disk_store() == make_save({}, {}), "synchronization resumes after clearing override");
});

[[nodiscard]] std::expected<void, StarException> expectIsolationConsumers(Root& root, LuaContext& context,
    String const& owner, String const& scriptOwner, String const& configurationOwner) {
  auto assets = root.assets();
  if (!assets) return std::unexpected(std::move(assets.error()));
  auto config = (*assets)->json("/root-isolation.config");
  if (!config) return std::unexpected(std::move(config.error()));
  EXPECT_EQ(config->get_string("owner"), owner);
  auto database = root.techDatabase();
  if (!database) return std::unexpected(std::move(database.error()));
  auto tech = (*database)->tech("root-isolation");
  if (!tech) return std::unexpected(std::move(tech.error()));
  EXPECT_EQ(tech->description, owner);
  auto configuration = root.configuration();
  if (!configuration) return std::unexpected(std::move(configuration.error()));
  auto value = (*configuration)->get("rootIsolationOwner");
  if (!value) return std::unexpected(std::move(value.error()));
  EXPECT_EQ(value->to_string(), configurationOwner);
  EXPECT_EQ(context.eval<String>("root.assetJson('/root-isolation.config').owner"), owner);
  EXPECT_EQ(context.eval<String>("root.techConfig('root-isolation').description"), owner);
  EXPECT_EQ(context.eval<String>("root.getConfiguration('rootIsolationOwner')"), configurationOwner);
  EXPECT_EQ(context.invoke_path<String>("isolationScriptOwner"), scriptOwner);
  return {};
}

[[nodiscard]] std::expected<std::shared_ptr<LuaRoot>, StarException> isolationLua(Root& root, bool utilities = false) {
  auto lua = LuaRoot::create(root);
  if (!lua) return std::unexpected(std::move(lua.error()));
  auto callbacks = LuaBindings::makeRootCallbacks(root);
  if (!callbacks) return std::unexpected(std::move(callbacks.error()));
  (*lua)->add_callbacks("root", *callbacks);
  if (utilities) {
    auto utilityCallbacks = LuaBindings::make_utility_callbacks(root.random(), root.logger(), root.logMap());
    if (!utilityCallbacks) return std::unexpected(std::move(utilityCallbacks.error()));
    (*lua)->add_callbacks("sb", *utilityCallbacks);
  }
  return std::move(*lua);
}

const GameTestRegistration liquid_config_binding("LiquidsDatabaseTest", "LuaLookupPreservesNameAndIdSemantics", [](GameTest& fixture) {
  auto lua = isolationLua(fixture.root());
  if (!fixture.check(bool(lua), "create Lua root")) return;
  auto context = (*lua)->create_context(StringList{});
  if (!fixture.check(bool(context), "create liquid lookup context")) return;
  auto checked = context->eval<bool>(R"LUA(
    local id = root.liquidId("water")
    local named, numbered = root.liquidConfig("water"), root.liquidConfig(id)
    assert(named.path == numbered.path and root.assetJson(named.path).liquidId == id)
    assert(named.config.name == "water" and numbered.config.liquidId == id)
    assert(root.liquidConfig("empty") == nil and root.liquidConfig(0) == nil)
    assert(not pcall(root.liquidConfig, "missing_liquid_lookup_fixture"))
    return root.liquidName(id) == "water"
  )LUA");
  fixture.check(checked && *checked, checked ? "liquid name round-trip" : checked.error().what());
});

const GameTestRegistration lua_name_seed("NameGeneratorConsumerTest", "LuaSeedOnlyDrawsFromRootWhenOmitted", [](GameTest& fixture) {
  auto directory = File::temporary_directory(fixture.root().random());
  ASSERT_TRUE(directory) << directory.error().what();
  auto cleanup = make_scope_exit([&] { EXPECT_TRUE(File::remove_all(*directory)); });
  auto settings = isolation_settings(fixture.rootSettings(), *directory, "lua-name-seed");
  ASSERT_TRUE(settings) << settings.error().what();
  auto created = Root::create(std::move(*settings));
  ASSERT_TRUE(created) << created.error().what();
  auto& root = **created;
  auto species = root.speciesDatabase();
  ASSERT_TRUE(species) << species.error().what();
  auto human = (*species)->species("human");
  ASSERT_TRUE(human) << human.error().what();
  auto generator = root.name_generator();
  ASSERT_TRUE(generator) << generator.error().what();
  auto lua = isolationLua(root);
  ASSERT_TRUE(lua) << lua.error().what();
  auto context = (*lua)->create_context(StringList{});
  ASSERT_TRUE(context) << context.error().what();

  auto const& rules = (*human)->name_gen(Gender::Male);
  constexpr std::uint64_t global_seed = 31097;
  RandomService reference(global_seed);
  root.random().seed(global_seed);
  auto seeded = context->eval<String>(R"LUA(
    return root.generateName(root.assetJson("/species/human.species").nameGen[1], 0)
  )LUA");
  auto expected_seeded = (*generator)->generate_name(rules, std::uint64_t{0});
  ASSERT_TRUE(seeded) << seeded.error().what();
  ASSERT_TRUE(expected_seeded) << expected_seeded.error().what();
  EXPECT_EQ(*seeded, *expected_seeded);
  EXPECT_EQ(root.random().next_u64(), reference.next_u64());

  auto unseeded = context->eval<String>(R"LUA(
    return root.generateName(root.assetJson("/species/human.species").nameGen[1])
  )LUA");
  auto expected_unseeded = (*generator)->generate_name(rules, reference.next_u64());
  ASSERT_TRUE(unseeded) << unseeded.error().what();
  ASSERT_TRUE(expected_unseeded) << expected_unseeded.error().what();
  EXPECT_EQ(*unseeded, *expected_unseeded);
  EXPECT_EQ(root.random().next_u64(), reference.next_u64());
});

const GameTestRegistration material_config_binding("MaterialDatabaseTest", "LuaLookupDistinguishesRealAndMetaAssets", [](GameTest& fixture) {
  auto lua = isolationLua(fixture.root());
  if (!fixture.check(bool(lua), "create Lua root")) return;
  auto context = (*lua)->create_context(StringList{});
  if (!fixture.check(bool(context), "create material lookup context")) return;
  auto checked = context->eval<bool>(R"LUA(
    local dirt = root.materialConfig("dirt")
    assert(root.materialConfig(dirt.config.materialId).path == dirt.path)
    assert(root.assetJson(dirt.path).materialName == "dirt")
    assert(root.materialConfig("metamaterial:empty") == nil and root.modConfig("metamod:none") == nil)
    assert(not pcall(root.materialConfig, "missing_material_lookup_fixture"))
    assert(not pcall(root.modConfig, "missing_mod_lookup_fixture"))
    return root.materialHealth("dirt") > 0
  )LUA");
  fixture.check(checked && *checked, checked ? "material lookup contract" : checked.error().what());
});

[[nodiscard]] std::expected<bool, StarException> isolationLogContains(String const& path, String const& text) {
  auto content = File::read_file_string(path);
  if (!content) return std::unexpected(std::move(content.error()));
  return content->contains(text);
}

[[nodiscard]] std::expected<String, StarException> isolationStoredOwner(String const& directory) {
  auto content = File::read_file_string(File::relative_to(directory, "root-isolation.config"));
  if (!content) return std::unexpected(std::move(content.error()));
  auto json = Json::parse_json(*content);
  if (!json) return std::unexpected(std::move(json.error()));
  return json->get_string("rootIsolationOwner");
}

const GameTestRegistration statistics_achievement_index(
  "StatisticsDatabaseTest", "AtomicIndexFirstDefinitionAndOptionalTriggers", [](GameTest& fixture) {
    auto directory = File::temporary_directory(fixture.root().random());
    ASSERT_TRUE(directory);
    auto cleanup = make_scope_exit([&] { EXPECT_TRUE(File::remove_all(*directory)); });
    auto settings = isolation_settings(fixture.rootSettings(), *directory, "statistics");
    ASSERT_TRUE(settings) << settings.error().what();
    auto overlay = File::relative_to(*directory, "assets");
    settings->assetSources = {overlay};
    auto write_asset = [&](String const& filename, String const& content) {
      return File::write_file(content, File::relative_to(overlay, filename));
    };
    ASSERT_TRUE(write_asset("preload.config", "[]"));
    ASSERT_TRUE(write_asset("alpha.achievement", R"({"name":"shared","triggers":["alpha","both"],"scripts":[]})"));
    ASSERT_TRUE(write_asset("beta.achievement", R"({"name":"shared","triggers":["beta","both"],"scripts":[]})"));
    ASSERT_TRUE(write_asset("invalid.achievement", R"({"name":"invalid","triggers":["leaked",3],"scripts":[]})"));
    ASSERT_TRUE(write_asset("missing.achievement", R"({"name":"missing","scripts":[]})"));
    ASSERT_TRUE(write_asset("null.achievement", R"({"name":"null","triggers":null,"scripts":[]})"));
    auto root = Root::create(std::move(*settings));
    ASSERT_TRUE(root) << root.error().what();
    auto assets = (*root)->assets();
    ASSERT_TRUE(assets) << assets.error().what();
    auto paths = (*assets)->scan_extension("achievement");
    ASSERT_TRUE(paths);
    // Index iteration order, not lexical filename order, determines which definition wins.
    auto first = std::find_if(paths->get().begin(), paths->get().end(), [](CaseInsensitiveString const& path) {
      return path.string() == "/alpha.achievement" || path.string() == "/beta.achievement";
    });
    ASSERT_NE(first, paths->get().end());
    bool alpha_first = first->string() == "/alpha.achievement";
    String winner = alpha_first ? String("alpha") : String("beta");
    String loser = alpha_first ? String("beta") : String("alpha");

    auto loaded = (*root)->statisticsDatabase();
    ASSERT_TRUE(loaded) << loaded.error().what();
    auto const& database = **loaded;
    EXPECT_EQ(database.all_achievements().size(), 3u);
    auto invalid = database.achievement("invalid");
    ASSERT_TRUE(invalid) << invalid.error().what();
    EXPECT_FALSE(*invalid);
    EXPECT_TRUE(database.achievements_for_stat("leaked").empty());
    auto shared = database.achievement("shared");
    ASSERT_TRUE(shared) << shared.error().what();
    ASSERT_TRUE(*shared);
    EXPECT_EQ((*shared)->triggers, (StringList{winner, "both"}));
    auto names = database.achievements_for_stat(winner);
    ASSERT_EQ(names.size(), 1u);
    EXPECT_EQ(names.front(), "shared");
    EXPECT_EQ(names.data(), database.achievements_for_stat(winner).data());
    EXPECT_TRUE(database.achievements_for_stat(loser).empty());
    EXPECT_EQ(database.achievements_for_stat("both").size(), 1u);
    for (auto const& name : StringList{"missing", "null"}) {
      auto achievement = database.achievement(name);
      ASSERT_TRUE(achievement) << achievement.error().what();
      ASSERT_TRUE(*achievement);
      EXPECT_TRUE((*achievement)->triggers.empty());
    }
  });

const GameTestRegistration timed_dance("DanceTest", "RefreshAndFailedReplacement", [](GameTest& fixture) {
  auto directory = File::temporary_directory(fixture.root().random());
  ASSERT_TRUE(directory) << directory.error().what();
  auto cleanup = make_scope_exit([&] { EXPECT_TRUE(File::remove_all(*directory)); });
  auto settings = isolation_settings(fixture.rootSettings(), *directory, "dance");
  ASSERT_TRUE(settings) << settings.error().what();
  auto overlay = File::relative_to(*directory, "assets");
  settings->assetSources = {overlay}; // No assetmissing fallback: exercise a real lookup failure.
  ASSERT_TRUE(File::write_file(String("[]"), File::relative_to(overlay, "preload.config")));
  ASSERT_TRUE(File::write_file(String(R"({"name":"pose","states":[],"steps":[],"duration":2,"cycle":1,"cyclic":false})"),
                               File::relative_to(overlay, "pose.dance")));
  auto root = Root::create(std::move(*settings));
  ASSERT_TRUE(root) << root.error().what();
  TimedDance playback;
  ASSERT_TRUE(playback.set(**root, String("pose")));
  playback.tick(1.5f);
  ASSERT_TRUE(playback.set(**root, String("pose")));
  playback.tick(1.0f);
  ASSERT_EQ(playback.name, std::optional<String>("pose"));
  ASSERT_FALSE(playback.set(**root, String("missing")));
  playback.tick(0.75f);
  EXPECT_EQ(playback.name, std::optional<String>("pose"));
  playback.tick(0.25f);
  EXPECT_FALSE(playback.name);
});

const GameTestRegistration secure_identity("RootTest", "SecureIdentityStability", [](GameTest& fixture) {
  auto directory = File::temporary_directory(fixture.root().random());
  ASSERT_TRUE(directory) << directory.error().what();
  auto cleanup = make_scope_exit([&] { EXPECT_TRUE(File::remove_all(*directory)); });
  auto settings = isolation_settings(fixture.rootSettings(), *directory, "identity");
  ASSERT_TRUE(settings) << settings.error().what();
  auto created = Root::create(std::move(*settings));
  ASSERT_TRUE(created) << created.error().what();
  auto& root = **created;

  auto const public_key = root.identity().public_key();
  std::string_view message("identity\0regression", sizeof("identity\0regression") - 1);
  auto signature = root.identity().sign(std::span<char const>(message));
  EXPECT_TRUE(Curve25519::verify(signature, public_key, std::span<char const>(message)));
  ASSERT_TRUE(root.reload());
  EXPECT_EQ(root.identity().public_key(), public_key);
  EXPECT_EQ(root.identity().sign(std::span<char const>(message)), signature);
});

const GameTestRegistration alt_music_lifecycle("WorldClientTest", "AltMusicStopAndWorldChange", [](GameTest& fixture) {
  auto& root = fixture.root();
  auto universe = TestUniverse::create(root, {100, 100});
  ASSERT_TRUE(universe) << universe.error().what();
  auto close = make_scope_exit([&] { EXPECT_TRUE((*universe)->close()); });
  auto player = (*universe)->client()->mainPlayer();
  auto uuid = player->uuid();
  ASSERT_TRUE(uuid) << uuid.error().what();
  ASSERT_TRUE((*universe)->warpPlayer(ClientShipWorldId(*uuid)));
  ASSERT_TRUE((*universe)->client()->playerOnOwnShip());
  auto world = (*universe)->worldClient();
  ASSERT_TRUE(world);

  String const path = "/sfx/interface/voice_on.ogg";
  auto assets = root.assets();
  ASSERT_TRUE(assets) << assets.error().what();
  // update_ambient uses nonblocking asset reads. Keep the real clip loaded.
  auto clip = (*assets)->audio(path);
  ASSERT_TRUE(clip) << clip.error().what();
  constexpr int alt_loops = 1234;
  JsonArray const play_args{JsonArray{path}, 0, alt_loops};
  auto take_alt_voice = [&]() -> std::shared_ptr<AudioInstance> {
    for (auto& voice : world->pull_pending_music()) {
      if (voice->loops() == alt_loops)
        return std::move(voice);
    }
    return {};
  };
  ASSERT_TRUE(player->receive_message(server_connection_id, "playAltMusic", play_args));
  ASSERT_TRUE(world->update(1.0f / 60.0f));
  auto first = take_alt_voice();
  ASSERT_TRUE(first);

  Mixer mixer(root.logger(), 8000, 1);
  ASSERT_TRUE(mixer.play(first));
  first->set_volume(1.0f); // Start the measured fade at full gain.
  ASSERT_TRUE(player->receive_message(server_connection_id, "stopAltMusic", {0.25}));
  ASSERT_TRUE(world->update(1.0f / 60.0f));
  EXPECT_FALSE(take_alt_voice());
  std::array<std::int16_t, 1000> samples{};
  ASSERT_TRUE(mixer.read(samples, samples.size()));
  EXPECT_FALSE(first->finished());
  ASSERT_TRUE(mixer.read(samples, samples.size()));
  EXPECT_TRUE(first->finished());
  ASSERT_TRUE(world->update(1.0f / 60.0f));
  EXPECT_FALSE(take_alt_voice()) << "The finished alternate track must not restart";

  ASSERT_TRUE(player->receive_message(server_connection_id, "playAltMusic", play_args));
  ASSERT_TRUE(world->update(1.0f / 60.0f));
  auto second = take_alt_voice();
  ASSERT_TRUE(second);
  ASSERT_TRUE(mixer.play(second));
  second->set_volume(1.0f);
  auto destination = InstanceWorldId("outpost");
  ASSERT_TRUE((*universe)->warpPlayer(destination));
  ASSERT_EQ((*universe)->currentPlayerWorld(), WorldId(destination));
  ASSERT_EQ(world, (*universe)->worldClient());
  ASSERT_TRUE(mixer.read(samples, samples.size()));
  EXPECT_TRUE(second->finished()) << "World reset must cancel the active voice";
  ASSERT_TRUE(world->update(1.0f / 60.0f));
  EXPECT_FALSE(take_alt_voice()) << "The new world must not inherit the old playlist";
});

const GameTestRegistration presentation_message_failures("WorldClientTest", "PresentationMessageFailuresSettleBothPeers", [](GameTest& fixture) {
  auto universe = TestUniverse::create(fixture.root(), {100, 100});
  ASSERT_TRUE(universe) << universe.error().what();
  auto close = make_scope_exit([&] { EXPECT_TRUE((*universe)->close()); });
  auto player = (*universe)->client()->mainPlayer();
  auto uuid = player->uuid();
  ASSERT_TRUE(uuid);
  ASSERT_TRUE((*universe)->warpPlayer(ClientShipWorldId(*uuid)));
  auto world = (*universe)->worldClient();
  auto owner = connection_for_entity(player->entityId());
  ASSERT_TRUE(owner);

  String const path = "/sfx/interface/voice_on.ogg";
  std::pair<String, JsonArray> const invalid[] = {
    {"playCinematic", {}}, {"playCinematic", {Json{}}}, {"playCinematic", {JsonObject{}, 1}},
    {"playAltMusic", {Json(JsonArray{path, JsonObject{}})}}, {"playAltMusic", {path, "slow"}},
    {"playAltMusic", {path, 0, 1e30}}, {"playAltMusic", {path + ":subpath"}},
    {"stopAltMusic", {Json(JsonArray{})}},
    {"queueRadioMessage", {JsonObject{{"messageId", "radio-invalid"}, {"text", JsonArray{}}}}},
    {"queueRadioMessage", {JsonObject{{"messageId", "radio-invalid"}, {"speciesMessage", 3}}}}
  };

  // The relay must answer the original calling client's UUID (even for null
  // cinematics), rather than forwarding or losing the request.
  std::vector<RpcPromise<Json>> from_client, from_server;
  for (auto const& [message, args] : invalid) {
    auto sent = world->send_entity_message(String("missing-presentation-test-entity"), message, args);
    ASSERT_TRUE(sent);
    from_client.push_back(std::move(*sent));
  }

  // A server-originated request bypasses the relay check. Exercise the real
  // player and original server keepers, including a success after the errors.
  auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
  while (from_server.empty() && std::chrono::steady_clock::now() < deadline) {
    ASSERT_TRUE((*universe)->update());
    auto sent = (*universe)->server()->executeForClient(*owner, [&](WorldServer& server, std::shared_ptr<Player> remote) -> std::expected<void, StarException> {
      auto send = [&](String const& message, JsonArray const& args) -> std::expected<void, StarException> {
        auto result = server.send_entity_message(remote->entityId(), message, args);
        if (!result)
          return std::unexpected(std::move(result.error()));
        from_server.push_back(std::move(*result));
        return {};
      };
      for (auto const& [message, args] : invalid) {
        if (auto result = send(message, args); !result)
          return result;
      }
      if (auto result = send("unhandled-presentation-test-message", {}); !result)
        return result;
      if (auto result = send("queueRadioMessage", {JsonObject{{"messageId", "radio-after-errors"}, {"important", true}, {"portraitImage", ""}, {"text", "Received"}}}); !result)
        return result;
      return send("playCinematic", {JsonObject{{"test", true}}});
    });
    ASSERT_TRUE(sent) << sent.error().what();
  }
  ASSERT_EQ(from_server.size(), std::size(invalid) + 3);
  while (std::chrono::steady_clock::now() < deadline) {
    if (std::ranges::all_of(from_client, [](auto const& p) { return p.finished() == true; }) &&
        std::ranges::all_of(from_server, [](auto const& p) { return p.finished() == true; }))
      break;
    ASSERT_TRUE((*universe)->update());
  }
  for (std::size_t i = 0; i < std::size(invalid); ++i) {
    SCOPED_TRACE(i);
    auto reason = from_client[i].error();
    ASSERT_TRUE(reason && *reason);
    EXPECT_NE(**reason, String("Unknown entity"));
    EXPECT_NE(**reason, String("Message not handled by entity"));
    auto server_reason = from_server[i].error();
    ASSERT_TRUE(server_reason);
    EXPECT_EQ(*server_reason, *reason);
  }
  auto unhandled = from_server[std::size(invalid)].error();
  ASSERT_TRUE(unhandled && *unhandled);
  EXPECT_EQ(**unhandled, "Message not handled by entity");
  auto radio_response = from_server[std::size(invalid) + 1].result();
  ASSERT_TRUE(radio_response && *radio_response);
  EXPECT_TRUE((**radio_response).is_null());
  auto queued = player->pull_pending_radio_message();
  ASSERT_TRUE(queued && *queued);
  EXPECT_EQ((**queued).message_id, "radio-after-errors");
  auto accepted = from_server.back().result();
  ASSERT_TRUE(accepted && *accepted);
  EXPECT_TRUE((**accepted).is_null());
  EXPECT_TRUE((*universe)->client()->isConnected());

  // Rejection is transactional. A malformed replacement cannot discard a
  // previously queued request; a single string track is not an empty list.
  ASSERT_TRUE(world->send_entity_message(player->entityId(), "playAltMusic", {path, 0.25, 2}));
  auto bad = world->send_entity_message(player->entityId(), "playAltMusic", {Json(JsonArray{path, 7})});
  ASSERT_TRUE(bad);
  EXPECT_EQ(bad->failed(), true);
  auto pending = player->pull_pending_alt_music();
  ASSERT_TRUE(pending && pending->tracks);
  EXPECT_EQ(*pending->tracks, StringList{path});
  EXPECT_EQ(pending->loops, 2);
  EXPECT_EQ(pending->fade_time, 0.25f);
  // Unrelated native errors are not mistaken for a rejected cosmetic RPC.
  EXPECT_FALSE(world->send_entity_message(player->entityId(), "recordEvent", {}));
});

const GameTestRegistration missing_death_burst("WorldClientTest", "MissingPlayerDeathBurstPreservesDropsAndRemoval", [](GameTest& fixture) {
  auto& root = fixture.root();
  auto universe = TestUniverse::create(root, {100, 100});
  ASSERT_TRUE(universe);
  auto close = make_scope_exit([&] { EXPECT_TRUE((*universe)->close()); });
  auto uuid = (*universe)->client()->mainPlayer()->uuid();
  ASSERT_TRUE(uuid);
  ASSERT_TRUE((*universe)->warpPlayer(ClientShipWorldId(*uuid)));
  auto world = (*universe)->worldClient();
  ASSERT_EQ(world->disableDeathDrops(), false);
  auto factory = root.playerFactory();
  ASSERT_TRUE(factory);
  auto created = (*factory)->create();
  ASSERT_TRUE(created);
  auto player = std::move(*created);
  auto const& client = (*universe)->client();
  player->set_client_context(client->client_context());
  player->set_statistics(client->statistics());
  player->set_universe_client(std::ref(*client));
  ASSERT_TRUE(player->finalize_creation());
  ASSERT_TRUE(player->set_mode_type(PlayerMode::Hardcore));
  auto items = root.itemDatabase();
  ASSERT_TRUE(items);
  auto descriptor = ItemDescriptor::create("dirtmaterial", 1);
  ASSERT_TRUE(descriptor);
  auto item = (*items)->item(*descriptor);
  ASSERT_TRUE(item && *item);
  ASSERT_TRUE(player->inventory()->setSwapSlotItem(*item));
  String const burst = "unconfigured-test-death-burst";
  ASSERT_TRUE(player->set_humanoid_parameter("deathParticles", Json(burst)));
  ASSERT_TRUE(player->refresh_humanoid_parameters());
  ASSERT_TRUE(world->addEntity(player));
  ASSERT_EQ(player->humanoid()->default_death_particles(), burst);
  ASSERT_FALSE(player->humanoid()->particles(burst));

  auto const deaths = player->log()->deathCount();
  auto const id = player->entityId();
  std::shared_ptr<EntityCreatePacket> birth;
  for (auto const& packet : world->getOutgoingPackets())
    if (auto create = as<EntityCreatePacket>(packet); create && create->entity_id == id)
      birth = std::move(create);
  ASSERT_TRUE(birth);
  ASSERT_TRUE(world->removeEntity(id, true));
  EXPECT_FALSE(world->entity(id));
  EXPECT_FALSE(player->inWorld());
  EXPECT_EQ(player->log()->deathCount(), deaths + 1);
  EXPECT_FALSE(player->inventory()->swapSlotItem());
  std::shared_ptr<EntityDestroyPacket> death;
  bool sent_drop = false;
  for (auto const& packet : world->getOutgoingPackets()) {
    if (auto spawn = as<SpawnEntityPacket>(packet))
      sent_drop |= spawn->entity_type == EntityType::ItemDrop;
    if (auto create = as<EntityCreatePacket>(packet))
      sent_drop |= create->entity_type == EntityType::ItemDrop;
    if (auto remove = as<EntityDestroyPacket>(packet); remove && remove->entity_id == id && remove->death)
      death = std::move(remove);
  }
  EXPECT_TRUE(sent_drop);
  ASSERT_TRUE(death);

  // Replay the actual master packets through the replica's immediate or
  // interpolated death path, including its final network state.
  ASSERT_TRUE(world->handleIncomingPackets({birth}));
  auto replica = world->entity(id);
  ASSERT_TRUE(replica && replica->isSlave());
  ASSERT_TRUE(world->handleIncomingPackets({death}));
  for (unsigned tick = 0; tick < 120 && replica->inWorld(); ++tick)
    ASSERT_TRUE(world->update(1.0f / 60.0f));
  EXPECT_FALSE(replica->inWorld());
  EXPECT_FALSE(world->entity(id));
});

const GameTestRegistration missing_flight_cues("SkyPresentationTest", "BadCuesAreConsumedAcrossFlightStages", [](GameTest& fixture) {
  auto sky = Sky::create(fixture.root());
  ASSERT_TRUE(sky);
  SkyParameters destination;
  destination.seed = 1234;
  destination.settings = JsonObject{
    {"disembarkPath", JsonArray{}}, {"arrivalPath", JsonArray{}},
    {"hyperspaceSpeedupTime", 1}, {"flyingTimer", 1}, {"hyperspaceSlowdownTime", 1},
    {"enterHyperspaceAudioLeadIn", 1}, {"exitHyperspaceAudioLeadIn", 1},
    {"enterHyperspaceAudio", false}, {"exitHyperspaceAudio", false}, {"arrivalAudio", false}};
  auto consume = [&](String const& cue) {
    auto first = (*sky)->pullSounds();
    ASSERT_TRUE(first);
    EXPECT_TRUE(first->empty());
    // Repair only the cue in the same phase: it must not play late.
    auto repaired = destination;
    repaired.settings = json_merge(destination.settings, JsonObject{{cue, "/sfx/interface/voice_on.ogg"}});
    ASSERT_TRUE((*sky)->jumpTo(repaired));
    auto second = (*sky)->pullSounds();
    ASSERT_TRUE(second);
    EXPECT_TRUE(second->empty());
  };
  ASSERT_TRUE((*sky)->startFlying(true, false, destination.settings));
  ASSERT_TRUE((*sky)->update(0.1));
  ASSERT_EQ((*sky)->warpPhase(), WarpPhase::SpeedingUp);
  consume("enterHyperspaceAudio");
  ASSERT_TRUE((*sky)->update(1));
  ASSERT_EQ((*sky)->warpPhase(), WarpPhase::Maintain);
  consume("exitHyperspaceAudio");
  ASSERT_TRUE((*sky)->stopFlyingAt(destination));
  ASSERT_TRUE((*sky)->update(1));
  ASSERT_EQ((*sky)->warpPhase(), WarpPhase::SlowingDown);
  ASSERT_TRUE((*sky)->update(1));
  ASSERT_EQ((*sky)->flyingType(), FlyingType::Arriving);
  consume("arrivalAudio");
  ASSERT_TRUE((*sky)->update(1));
  EXPECT_FALSE((*sky)->flying());
  EXPECT_EQ((*sky)->seed(), destination.seed);
});

const GameTestRegistration vanished_warp_target("UniverseServerTest", "ReturnsAfterSpawnTargetDisappearsDuringWarp", [](GameTest& fixture) {
  auto universe = TestUniverse::create(fixture.root(), {100, 100});
  ASSERT_TRUE(universe) << universe.error().what();
  auto close = make_scope_exit([&] { EXPECT_TRUE((*universe)->close()); });
  auto client = (*universe)->client();
  auto player = client->mainPlayer();
  player->log()->setIntroComplete(true);
  WorldId const original = InstanceWorldId("outpost");
  ASSERT_TRUE((*universe)->warpPlayer(original));
  ASSERT_EQ((*universe)->currentPlayerWorld(), original);
  auto world = (*universe)->worldClient();
  auto owner = connection_for_entity(player->entityId());
  ASSERT_TRUE(owner);

  // A real client-owned teleporter in the same world passes the spawn check,
  // then removeClient erases it before addClient resolves the unique target.
  String const target_id = "temporary-warp-regression";
  SpawnTarget const target = SpawnTargetUniqueEntity(target_id);
  auto objects = fixture.root().object_database();
  ASSERT_TRUE(objects);
  auto object = (*objects)->create_object("stoneteleporter", JsonObject{{"clientEntityMode", "ClientMasterAllowed"},
      {"uniqueId", target_id}, {"unbreakable", true}, {"scripts", JsonArray{}}});
  ASSERT_TRUE(object) << object.error().what();
  ASSERT_TRUE((*object)->set_tile_position(Vec2I::floor(player->position()) + Vec2I(12, 0)));
  ASSERT_TRUE(world->addEntity(*object));
  ASSERT_TRUE((*object)->isMaster());
  bool available = false;
  auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
  while (!available && std::chrono::steady_clock::now() < deadline) {
    ASSERT_TRUE((*universe)->update());
    auto observed = (*universe)->server()->executeForClient(*owner, [&](WorldServer& server, std::shared_ptr<Player>) -> std::expected<void, StarException> {
      auto valid = server.spawnTargetValid(target);
      if (!valid)
        return std::unexpected(std::move(valid.error()));
      available = *valid;
      return {};
    });
    ASSERT_TRUE(observed) << observed.error().what();
  }
  ASSERT_TRUE(available);
  ASSERT_TRUE(client->warpPlayer(WarpToWorld(original, target), false));
  deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
  while (((*object)->inWorld() || !world->inWorld() || (*universe)->currentPlayerWorld() != original) &&
         std::chrono::steady_clock::now() < deadline) {
    auto updated = (*universe)->update();
    ASSERT_TRUE(updated) << updated.error().what();
  }
  EXPECT_FALSE((*object)->inWorld()) << "The original world was never left";
  EXPECT_TRUE(world->inWorld());
  EXPECT_EQ((*universe)->currentPlayerWorld(), original) << "The queued return position must survive the failed join";
  EXPECT_TRUE(client->isConnected());
});

const GameTestRegistration prediction_liquid_update("WorldClientTest", "LiquidPacketPreservesPredictedTerrain", [](GameTest& fixture) {
  auto& root = fixture.root();
  auto universe = TestUniverse::create(root, {100, 100});
  ASSERT_TRUE(universe) << universe.error().what();
  auto close = make_scope_exit([&] { EXPECT_TRUE((*universe)->close()); });
  (*universe)->client()->mainPlayer()->log()->setIntroComplete(true);
  ASSERT_TRUE((*universe)->warpPlayer(InstanceWorldId("outpost")));
  auto world = (*universe)->worldClient();
  ASSERT_TRUE(world);
  auto materials = root.material_database();
  ASSERT_TRUE(materials) << materials.error().what();
  auto dirt = (*materials)->material_id("dirt");
  ASSERT_TRUE(dirt) << dirt.error().what();
  // A local packet batch gives us a known loaded neighborhood, independently
  // of the outpost layout. No network tick can acknowledge our pending edit.
  NetTile air;
  air.background = *dirt;
  air.foreground = empty_material_id;
  air.collision = CollisionKind::None;
  air.dungeonId = no_dungeon_id;
  auto patch = std::make_shared<TileArrayUpdatePacket>();
  patch->min = {8, 8};
  ASSERT_TRUE(patch->array.set_size({7, 7}, air));
  ASSERT_TRUE(world->handleIncomingPackets({patch}));
  Vec2I const position(11, 11);
  auto change = world->applyTileModifications({{position, PlaceMaterial{TileLayer::Foreground, *dirt, 53, TileCollisionOverride::Block}}}, true);
  ASSERT_TRUE(change) << change.error().what();
  ASSERT_TRUE(change->empty());
  auto inside = Vec2F(position) + Vec2F(0.625f, 0.625f);
  auto cached = world->point_collision(inside);
  ASSERT_TRUE(cached) << cached.error().what();
  ASSERT_TRUE(*cached);

  auto liquid = std::make_shared<TileLiquidUpdatePacket>(position, air.liquid);
  auto neighbor = std::make_shared<TileUpdatePacket>();
  neighbor->position = position + Vec2I(1, 0);
  neighbor->tile = air;
  neighbor->tile.collision = CollisionKind::Platform;
  ASSERT_TRUE(world->handleIncomingPackets({liquid, neighbor}));
  auto rebuilt = world->point_collision(inside);
  ASSERT_TRUE(rebuilt) << rebuilt.error().what();
  EXPECT_TRUE(*rebuilt) << "Neighbor refresh lost a foreground prediction after an unrelated liquid packet";

  auto falling = (*materials)->material_id("redtoyblock");
  auto cascading = (*materials)->material_id("sand");
  ASSERT_TRUE(falling);
  ASSERT_TRUE(cascading);
  Vec2I const falling_pos(10, 11), cascading_pos(12, 11), invalid_pos(11, 12);
  auto placements = world->applyTileModifications({
      {falling_pos, PlaceMaterial{TileLayer::Foreground, *falling, 63}},
      {cascading_pos, PlaceMaterial{TileLayer::Foreground, *cascading, 73}},
      {invalid_pos, PlaceLiquid{1, 1.0f}}}, true);
  ASSERT_TRUE(placements) << placements.error().what();
  ASSERT_TRUE(placements->empty());
  MaterialId missing = 0;
  while (is_real_material(missing) && (*materials)->is_valid_material_id(missing))
    ++missing;
  ASSERT_TRUE(is_real_material(missing));
  EXPECT_FALSE(world->applyTileModifications({{invalid_pos, PlaceMaterial{TileLayer::Foreground, missing, 83}}}, true));

  world->setClientWindow(RectI::with_size({8, 8}, {7, 7}));
  WorldRenderData frame;
  auto rendered = world->render(frame, 0, std::nullopt, PresentationContext());
  ASSERT_TRUE(rendered) << rendered.error().what();
  auto predicted_tile = [&](Vec2I pos) -> RenderTile const& {
    auto index = pos - frame.tile_min_position;
    return frame.tiles(index[0], index[1]);
  };
  EXPECT_EQ(predicted_tile(position).foreground, *dirt);
  EXPECT_EQ(predicted_tile(position).foregroundHueShift, 53);
  for (auto pos : {falling_pos, cascading_pos}) {
    EXPECT_EQ(predicted_tile(pos).foreground, structure_material_id);
    EXPECT_EQ(predicted_tile(pos).foregroundHueShift, air.foregroundHueShift);
  }
  EXPECT_EQ(predicted_tile(invalid_pos).foreground, empty_material_id);
  EXPECT_EQ(predicted_tile(invalid_pos).liquidId, 1);
  EXPECT_EQ(predicted_tile(invalid_pos).liquidLevel, 255);
});

const GameTestRegistration signed_broadcast_lifecycle("RootTest", "SignedBroadcastPublicationAndKeyReplacement", [](GameTest& fixture) {
  auto& root = fixture.root();
  auto universe = TestUniverse::create(root, Vec2U(100, 100));
  ASSERT_TRUE(universe) << universe.error().what();
  auto close = make_scope_exit([&] { EXPECT_TRUE((*universe)->close()); });
  ASSERT_TRUE((*universe)->warpPlayer(InstanceWorldId("outpost")));
  auto ready = (*universe)->waitForWorldReady();
  ASSERT_TRUE(ready) << ready.error().what();
  auto world = (*universe)->worldClient();
  ASSERT_TRUE(world);
  std::shared_ptr<Player> player;
  ASSERT_TRUE(world->forAllEntities([&](std::shared_ptr<Entity> const& entity) -> std::expected<void, StarException> {
    if (auto candidate = as<Player>(entity); candidate && candidate->isMaster())
      player = std::move(candidate);
    return {};
  }));
  ASSERT_TRUE(player);
  String const property = "SecretBroadcastPublicKey";
  String const legacy_tag(std::string("\0SE_VOICE_SIGNING_KEY", sizeof("\0SE_VOICE_SIGNING_KEY") - 1));
  auto const& key = root.identity().public_key();
  String const expected_key(std::string(key.begin(), key.end()));
  DataStreamBuffer encoded_key;
  ASSERT_TRUE(encoded_key.write(Json(expected_key)));
  String const expected_property(std::string(encoded_key.data().span().begin(), encoded_key.data().span().end()));
  auto expect_published = [&] {
    auto published = player->get_secret_property_ref(property);
    ASSERT_TRUE(published);
    EXPECT_EQ(published->get(), expected_property);
    auto legacy = player->effects_animator()->global_tag(legacy_tag);
    ASSERT_TRUE(legacy);
    EXPECT_EQ(legacy->get(), expected_key);
  };
  ASSERT_NO_FATAL_FAILURE(expect_published());
  ASSERT_TRUE(player->set_secret_property(property, Json()));
  player->effects_animator()->remove_global_tag(legacy_tag);
  ASSERT_TRUE(world->update(1.0f / 60.0f));
  ASSERT_NO_FATAL_FAILURE(expect_published());
  ASSERT_TRUE(player->set_secret_property(property, 17));
  player->effects_animator()->set_global_tag(legacy_tag, "replaced");
  ASSERT_TRUE(world->update(1.0f / 60.0f));
  ASSERT_NO_FATAL_FAILURE(expect_published());

  std::vector<std::string> received;
  world->broadcastCallback() = [&](std::shared_ptr<Player> source, StringView message) -> std::expected<bool, StarException> {
    EXPECT_EQ(source, player);
    received.emplace_back(message.utf8());
    return true;
  };
  auto clear_callback = make_scope_exit([&] { world->broadcastCallback() = {}; });
  std::string_view outbound("outbound\0payload", sizeof("outbound\0payload") - 1);
  ASSERT_TRUE(world->sendSecretBroadcast(StringView(outbound), false, false));
  std::size_t outbound_count = 0;
  for (auto const& packet : world->getOutgoingPackets()) {
    auto damage = as<DamageNotificationPacket>(packet);
    if (!damage)
      continue;
    auto wire = damage->remote_damage_notification.damage_notification.target_material_kind.utf8();
    constexpr std::string_view prefix("\0Broadcast\0", sizeof("\0Broadcast\0") - 1);
    if (!wire.starts_with(prefix))
      continue;
    ASSERT_EQ(wire.size(), prefix.size() + Curve25519::signature_size + outbound.size());
    EXPECT_EQ(wire.substr(prefix.size() + Curve25519::signature_size), outbound);
    auto verifier = Curve25519::Verifier::create(key);
    ASSERT_TRUE(verifier);
    EXPECT_TRUE(verifier->verify(
        std::span<char const>(wire.substr(prefix.size(), Curve25519::signature_size)),
        std::span<char const>(outbound)));
    ++outbound_count;
  }
  EXPECT_EQ(outbound_count, 1u);
  Curve25519::SecretKey replacement_seed{};
  replacement_seed[0] = 71;
  auto replacement = Curve25519::KeySet::from_seed(replacement_seed);
  ASSERT_TRUE(replacement) << replacement.error().what();
  auto make_packet = [&](Curve25519::KeySet const& signer, bool legacy) {
    std::string_view payload("signed\0payload", sizeof("signed\0payload") - 1);
    auto signature = signer.sign(std::span<char const>(payload));
    std::string wire = legacy ? std::string("data\0voice\0", sizeof("data\0voice\0") - 1)
                              : std::string("\0Broadcast\0", sizeof("\0Broadcast\0") - 1);
    wire.append(signature.begin(), signature.end());
    wire.append(payload);
    auto packet = std::make_shared<DamageNotificationPacket>();
    packet->remote_damage_notification.source_entity_id = player->entityId();
    packet->remote_damage_notification.damage_notification.target_material_kind = String(std::move(wire));
    return packet;
  };
  for (bool legacy : {false, true}) {
    SCOPED_TRACE(legacy ? "StarExtensions" : "secret broadcast");
    auto publish = [&](String const& public_key) -> std::expected<void, StarException> {
      if (legacy) {
        player->effects_animator()->set_global_tag(legacy_tag, public_key);
        return {};
      }
      return player->set_secret_property(property, public_key);
    };
    ASSERT_TRUE(publish(expected_key));
    auto original_packet = make_packet(root.identity(), legacy);
    auto before = received.size();
    ASSERT_TRUE(world->handleIncomingPackets({original_packet}));
    ASSERT_EQ(received.size(), before + 1);
    EXPECT_EQ(received.back(), legacy ? std::string("Voice\0signed\0payload", sizeof("Voice\0signed\0payload") - 1)
                                     : std::string("signed\0payload", sizeof("signed\0payload") - 1));
    for (std::size_t length : {0u, 1u, 31u, 33u, 32u}) {
      ASSERT_TRUE(publish(String(std::string(length, '\0'))));
      ASSERT_TRUE(world->handleIncomingPackets({original_packet}));
      EXPECT_EQ(received.size(), before + 1);
    }
    if (legacy) {
      player->effects_animator()->remove_global_tag(legacy_tag);
    } else {
      ASSERT_TRUE(player->set_secret_property(property, Json()));
    }
    ASSERT_TRUE(world->handleIncomingPackets({original_packet}));
    EXPECT_EQ(received.size(), before + 1);
    auto const& replacement_key = replacement->public_key();
    ASSERT_TRUE(publish(String(std::string(replacement_key.begin(), replacement_key.end()))));
    ASSERT_TRUE(world->handleIncomingPackets({original_packet}));
    EXPECT_EQ(received.size(), before + 1);
    ASSERT_TRUE(world->handleIncomingPackets({make_packet(*replacement, legacy)}));
    EXPECT_EQ(received.size(), before + 2);
  }
  ASSERT_TRUE(world->update(1.0f / 60.0f));
  ASSERT_NO_FATAL_FAILURE(expect_published());
});


const GameTestRegistration lua_configuration_paths("RootTest", "LuaConfigurationPaths", [](GameTest& fixture) {
  auto directory = File::temporary_directory(fixture.root().random());
  ASSERT_TRUE(directory) << directory.error().what();
  auto cleanup = make_scope_exit([&] { EXPECT_TRUE(File::remove_all(*directory)); });
  auto settings = isolation_settings(fixture.rootSettings(), *directory, "lua-paths");
  ASSERT_TRUE(settings) << settings.error().what();
  auto root = Root::create(std::move(*settings));
  ASSERT_TRUE(root) << root.error().what();
  auto lua = isolationLua(**root);
  ASSERT_TRUE(lua) << lua.error().what();
  auto context = (*lua)->create_context(StringList{});
  ASSERT_TRUE(context) << context.error().what();
  auto configuration = (*root)->configuration();
  ASSERT_TRUE(configuration) << configuration.error().what();
  auto version = (*configuration)->get("configurationVersion");
  ASSERT_TRUE(version) << version.error().what();
  auto exercised = context->eval(R"LUA(
    assert(not pcall(root.getConfigurationPath, ""))
    assert(not pcall(root.getConfigurationPath, "title.multiPlayerAccount"))
    assert(not pcall(root.setConfigurationPath, "safe.luaHttp.enabled", true))
    for _, path in ipairs({"", "configurationVersion", "configurationVersion.client"}) do
      assert(not pcall(root.setConfigurationPath, path, 0))
      assert(not pcall(root.setConfigurationPath, path, nil))
    end
    local key = "configurationVersion]extra"
    root.setConfigurationPath(key, false)
    assert(root.getConfiguration(key) == false)
    root.setConfiguration(key, 0)
    assert(root.getConfigurationPath(key) == 0)
    root.setConfigurationPath(key, nil)
    assert(root.getConfiguration(key) == nil)
  )LUA");
  ASSERT_TRUE(exercised) << exercised.error().what();
  EXPECT_EQ((*configuration)->get("configurationVersion"), *version);
});

const GameTestRegistration mod_storage_metadata_reload("RootTest", "ModStorageMetadataReloadKeepsSourceIdentity", [](GameTest& fixture) {
  auto directory = File::temporary_directory(fixture.root().random());
  ASSERT_TRUE(directory) << directory.error().what();
  auto cleanup = make_scope_exit([&] { EXPECT_TRUE(File::remove_all(*directory)); });
  auto mods = File::relative_to(*directory, "mods");
  auto source = File::relative_to(mods, "source");
  auto metadata = File::relative_to(source, "_metadata");
  ASSERT_TRUE(File::create_directory(mods));
  ASSERT_TRUE(File::create_directory(source));
  ASSERT_TRUE(File::write_file(String(R"({"name":"OriginalOwner"})"), metadata));
  ASSERT_TRUE(File::write_file(String("[]"), File::relative_to(source, "preload.config")));
  auto settings = fixture.rootSettings();
  settings.assetDirectories = {mods};
  settings.assetSources.clear();
  settings.storageDirectory = File::relative_to(*directory, "storage");
  settings.runtimeConfigFile = {};
  settings.logDirectory = {};
  settings.logFile = {};
  settings.includeUGC = false;
  auto root = Root::create(std::move(settings));
  ASSERT_TRUE(root) << root.error().what();
  auto old_assets = (*root)->assets();
  ASSERT_TRUE(old_assets) << old_assets.error().what();
  auto old_path = (*old_assets)->asset_source_path_for_name("OriginalOwner");
  ASSERT_TRUE(old_path);
  EXPECT_EQ(old_path->get(), source);
  EXPECT_FALSE((*old_assets)->asset_source_path_for_name("originalowner"));
  EXPECT_FALSE((*old_assets)->asset_source_path_for_name("source"));
  auto old_id = (*root)->mod_storage_id(**old_assets, old_path->get());
  ASSERT_TRUE(old_id) << old_id.error().what();
  EXPECT_EQ(old_id->get(), "OriginalOwner");

  auto lua = isolationLua(**root);
  ASSERT_TRUE(lua) << lua.error().what();
  auto context = (*lua)->create_context(StringList{});
  ASSERT_TRUE(context) << context.error().what();
  ASSERT_TRUE(context->eval<void>("root.setModStorage('value', 42, 'OriginalOwner')"));
  ASSERT_TRUE(File::write_file(String(R"({"name":"RenamedOwner"})"), metadata));
  ASSERT_TRUE((*root)->hotReload());
  EXPECT_FALSE((*old_assets)->asset_source_path_for_name("RenamedOwner"));
  EXPECT_EQ(context->eval<int>("root.getModStorage('value', 'OriginalOwner')"), 42);

  auto other_source = File::relative_to(mods, "other-source");
  ASSERT_TRUE(File::create_directory(other_source));
  ASSERT_TRUE(File::write_file(String(R"({"name":"SeparateOwner"})"), File::relative_to(other_source, "_metadata")));
  ASSERT_TRUE((*root)->reload());
  auto new_assets = (*root)->assets();
  ASSERT_TRUE(new_assets) << new_assets.error().what();
  EXPECT_FALSE((*new_assets)->asset_source_path_for_name("OriginalOwner"));
  auto new_path = (*new_assets)->asset_source_path_for_name("RenamedOwner");
  ASSERT_TRUE(new_path);
  EXPECT_EQ(new_path->get(), source);
  auto other_id = (*root)->mod_storage_id(**new_assets, other_source);
  ASSERT_TRUE(other_id) << other_id.error().what();
  EXPECT_EQ(other_id->get(), "SeparateOwner");
  auto new_id = (*root)->mod_storage_id(**new_assets, new_path->get());
  ASSERT_TRUE(new_id) << new_id.error().what();
  EXPECT_EQ(std::addressof(old_id->get()), std::addressof(new_id->get()));
  EXPECT_EQ(old_path->get(), source);
  EXPECT_TRUE((*old_assets)->asset_source_path_for_name("OriginalOwner"));
  EXPECT_EQ(context->eval<int>("root.getModStorage('value', 'RenamedOwner')"), 42);
  auto rejected = context->eval<Json>("root.getModStorage('value', 'OriginalOwner')");
  ASSERT_FALSE(rejected);
  EXPECT_NE(std::string_view(rejected.error().what()).find("no loaded mod is named 'OriginalOwner'"), std::string_view::npos);
});

const GameTestRegistration strictLuaTags("RootTest", "StrictLuaTagFormatting", [](GameTest& fixture) {
  auto lua = isolationLua(fixture.root(), true);
  ASSERT_TRUE(lua) << lua.error().what();
  auto context = (*lua)->create_context(StringList{});
  ASSERT_TRUE(context) << context.error().what();
  auto result = context->eval<Json>(R"LUA(
    local rejected = {}
    for i, text in ipairs({"<missing>", "<>", "<worldName"}) do
      rejected[i] = not pcall(sb.formatTags, text, {worldName = "日"})
    end
    return {
      text = sb.formatTags("<<worldName> <worldName> {not:std-format}",
        {worldName = "日%<literal>"}),
      binary = sb.formatTags("a\0<x>b", {x = "z\0q"}),
      rejected = rejected,
      recovered = sb.formatTags("<worldName>", {worldName = "again"})
    }
  )LUA");
  ASSERT_TRUE(result) << result.error().what();
  EXPECT_EQ(result->get_string("text"), String("<worldName> 日%<literal> {not:std-format}"));
  EXPECT_EQ(result->get_string("binary"), String(std::string("a\0z\0qb", 6)));
  EXPECT_EQ(result->get("rejected"), Json(JsonArray{true, true, true}));
  EXPECT_EQ(result->get_string("recovered"), String("again"));
});

const GameTestRegistration lua_http_cancellation("RootTest", "LuaHttpCancellation", [](GameTest& fixture) {
  auto directory = File::temporary_directory(fixture.root().random());
  ASSERT_TRUE(directory) << directory.error().what();
  auto cleanup = make_scope_exit([&] { EXPECT_TRUE(File::remove_all(*directory)); });
  auto settings = isolation_settings(fixture.rootSettings(), *directory, "http");
  ASSERT_TRUE(settings) << settings.error().what();
  auto root = Root::create(std::move(*settings));
  ASSERT_TRUE(root) << root.error().what();
  auto configuration = (*root)->configuration();
  ASSERT_TRUE(configuration) << configuration.error().what();
  ASSERT_TRUE((*configuration)->set_path("safe.luaHttp.trustedSites", JsonArray{}));
  auto session = std::make_shared<LuaHttpSession>(**root);
  auto callbacks = LuaBindings::make_http_callbacks(session, true);
  ASSERT_TRUE(callbacks) << callbacks.error().what();
  auto engine = LuaEngine::create();
  ASSERT_TRUE(engine) << engine.error().what();
  auto context = (*engine)->create_context();
  ASSERT_TRUE(context) << context.error().what();
  ASSERT_TRUE(context->set_callbacks("http", *callbacks));
  auto lua = [&](String const& script) -> testing::AssertionResult {
    if (auto result = context->eval<void>(script); !result)
      return testing::AssertionFailure() << result.error().what();
    return testing::AssertionSuccess();
  };
  auto listener = TcpSocket::listen((*root)->logger(), {HostAddress::localhost(), 0});
  ASSERT_TRUE(listener) << listener.error().what();
  ASSERT_TRUE((*listener)->set_non_blocking(true));
  ASSERT_TRUE(context->set_path("url", Star::format("http://127.0.0.1:{}/", (*listener)->local_address().port())));
  ASSERT_TRUE(lua("unavailable = http.get(url); assert(unavailable:status() == 'failed')"));
  ASSERT_TRUE(session->set_approval_available(true));
  ASSERT_FALSE(session->approval());
  ASSERT_TRUE(lua(R"LUA(
    for _, p in ipairs({
      http.createRequest("HEAD", url),
      http.get("http://"),
      http.get("file:///example"),
      http.get("https://user@example.test/"),
      http.get(url .. "\0suffix"),
      http.get(url, {headers = {hOsT = "other.example"}}),
      http.get(url, {headers = {[":authority"] = "example.test"}}),
      http.get(url, {headers = {["Not A Token"] = "value"}}),
      http.get(url, {headers = {["X-Label"] = "embedded\0byte"}}),
      http.get(url, {headers = {["X-Label"] = "carriage\rreturn"}}),
      http.get(url, {headers = {["X-Label"] = "line\nbreak"}}),
      http.get(url, {timeout = -1}),
      http.get(url, {timeout = 2147483647})
    }) do
      assert(p:status() == "failed" and p:error() ~= nil)
    end
  )LUA"));
  ASSERT_FALSE(session->approval());
  ASSERT_TRUE(lua("ipv6 = http.get('http://[::1]:8080/?a=b')"));
  auto ipv6 = session->approval();
  ASSERT_TRUE(ipv6);
  EXPECT_EQ(ipv6->domain, String("[::1]"));
  ASSERT_TRUE(lua("assert(ipv6:cancel())"));
  ASSERT_FALSE(session->approval());

  ASSERT_TRUE(lua(R"LUA(
    first = http.get(url)
    second = http.get("https://MiXeD.Example:8443?x=1")
    third = http.get("http://third.example/")
    same = http.get("http://mixed.example/another-path")
    assert(first:status() == "pending" and second:status() == "pending")
  )LUA"));
  auto first = session->approval();
  ASSERT_TRUE(first);
  EXPECT_EQ(first->domain, String("127.0.0.1"));
  ASSERT_TRUE(lua(R"LUA(
    assert(first:cancel() and not first:cancel())
    assert(first:finished() and first:failed() and first:cancelled())
    assert(not first:succeeded() and first:result() == nil and first:error() ~= nil)
    reissued = http.get(url)
  )LUA"));
  auto second = session->approval();
  ASSERT_TRUE(second);
  EXPECT_EQ(second->domain, String("mixed.example"));
  ASSERT_TRUE(lua("sibling = http.get('https://MIXED.example/last'); assert(sibling:cancel())"));
  ASSERT_EQ(session->approval(), second);
  ASSERT_TRUE(session->resolve_approval(second->id, false, true));
  ASSERT_TRUE(lua(R"LUA(
    assert(second:status() == "failed" and not second:cancelled())
    assert(same:status() == "failed" and not http.isTrusted("mixed.example"))
    assert(third:status() == "pending" and reissued:status() == "pending")
  )LUA"));
  auto third = session->approval();
  ASSERT_TRUE(third);
  EXPECT_EQ(third->domain, String("third.example"));
  // A preference edit while a prompt is open must not turn a failed Remember
  // into authorization, a stuck queue, or a native UI callback failure.
  ASSERT_TRUE((*configuration)->set_path("safe.luaHttp.trustedSites", Json(false)));
  ASSERT_TRUE(session->resolve_approval(third->id, true, true));
  ASSERT_TRUE(lua("assert(third:failed() and not third:cancelled() and reissued:status() == 'pending')"));
  EXPECT_EQ((*configuration)->get_path("safe.luaHttp.trustedSites"), Json(false));
  ASSERT_TRUE((*configuration)->set_path("safe.luaHttp.trustedSites", JsonArray{}));
  auto reissued = session->approval();
  ASSERT_TRUE(reissued);
  ASSERT_EQ(reissued->domain, first->domain);
  ASSERT_NE(reissued->id, first->id);
  ASSERT_TRUE(session->resolve_approval(first->id, true, true));
  ASSERT_EQ(session->approval(), reissued);
  ASSERT_TRUE(lua("assert(reissued:status() == 'pending' and not http.isTrusted('127.0.0.1'))"));
  ASSERT_TRUE(session->resolve_approval(reissued->id, false, false));
  ASSERT_FALSE(session->approval());

  // A discarded Lua value must neither abandon a live request nor outlive its scope.
  std::stop_source discarded_scope;
  (*engine)->set_promise_lifetime(discarded_scope.get_token());
  ASSERT_TRUE(lua("http.get(url)"));
  ASSERT_TRUE((*engine)->collect_garbage());
  auto discarded = session->approval();
  ASSERT_TRUE(discarded);
  discarded_scope.request_stop();
  ASSERT_FALSE(session->approval());
  ASSERT_TRUE(session->resolve_approval(discarded->id, true, true));
  ASSERT_TRUE(lua("assert(http.get(url):cancelled())"));
  ASSERT_FALSE(session->approval());
  (*engine)->set_promise_lifetime({});

  ASSERT_TRUE(lua("withdrawn = http.get(url); queued = http.get('http://queued.example/')"));
  auto withdrawn = session->approval();
  ASSERT_TRUE(withdrawn);
  ASSERT_TRUE(session->set_approval_available(false));
  ASSERT_FALSE(session->approval());
  ASSERT_TRUE(lua("assert(withdrawn:status() == 'failed' and queued:status() == 'failed')"));
  ASSERT_TRUE(session->set_approval_available(true));
  ASSERT_TRUE(lua("stop_pending = http.get(url)"));
  auto stop_pending = session->approval();
  ASSERT_TRUE(stop_pending);
  ASSERT_TRUE(session->resolve_approval(withdrawn->id, true, true));
  ASSERT_EQ(session->approval(), stop_pending);
  ASSERT_TRUE(session->stop());
  ASSERT_FALSE(session->approval());
  ASSERT_TRUE(session->resolve_approval(stop_pending->id, true, true));
  ASSERT_TRUE(session->poll());
  ASSERT_TRUE(lua("assert(stop_pending:cancelled() and http.get(url):cancelled() and first:cancelled())"));
  EXPECT_EQ((*configuration)->get_path("safe.luaHttp.trustedSites"), Json(JsonArray{}));
  // After joining, a stale approval cannot have left a connection in the backlog.
  auto unexpected_connection = (*listener)->accept((*root)->logger());
  ASSERT_TRUE(unexpected_connection) << unexpected_connection.error().what();
  ASSERT_FALSE(*unexpected_connection);

  session = std::make_shared<LuaHttpSession>(**root);
  auto live_callbacks = LuaBindings::make_http_callbacks(session, true);
  ASSERT_TRUE(live_callbacks) << live_callbacks.error().what();
  ASSERT_TRUE(context->set_path("http", lua_nil));
  ASSERT_TRUE(context->set_callbacks("http", *live_callbacks));
  ASSERT_TRUE(session->set_approval_available(true));
  ASSERT_TRUE((*configuration)->set_path("safe.luaHttp.trustedSites", JsonArray{"Known.Example"}));
  ASSERT_TRUE(lua("assert(http.isTrusted('known.example') and http.isTrusted('KNOWN.EXAMPLE'))"));
  ASSERT_TRUE((*configuration)->set_path("safe.luaHttp.trustedSites", JsonArray{}));
  ASSERT_TRUE(lua("assert(not http.isTrusted('KNOWN.EXAMPLE'))"));
  auto accept_request = [&](std::string_view method = "GET", std::string_view body = {}, std::string_view expected_header = {})
      -> std::expected<std::shared_ptr<TcpSocket>, StarException> {
    auto ready = (*listener)->wait_for_data(false, std::chrono::steady_clock::now() + std::chrono::seconds(5));
    if (!ready)
      return std::unexpected(std::move(ready.error()));
    auto socket = (*listener)->accept((*root)->logger());
    if (!socket)
      return std::unexpected(std::move(socket.error()));
    if (!*socket)
      return std::unexpected(StarException("HTTP loopback request was not accepted"));
    if (auto timeout = (*socket)->set_timeout(5000); !timeout)
      return std::unexpected(std::move(timeout.error()));
    std::string headers;
    while (headers.find("\r\n\r\n") == std::string::npos) {
      std::array<char, 1024> buffer;
      auto received = (*socket)->receive(buffer);
      if (!received)
        return std::unexpected(std::move(received.error()));
      if (!*received)
        return std::unexpected(StarException("HTTP loopback closed before request headers"));
      headers.append(buffer.data(), *received);
    }
    if (!headers.starts_with(std::format("{} / HTTP/", method)))
      return std::unexpected(StarException("Incorrect HTTP request line"));
    auto body_offset = headers.find("\r\n\r\n") + 4;
    if (!expected_header.empty() && !std::string_view(headers).substr(0, body_offset).contains(expected_header))
      return std::unexpected(StarException("Missing expected HTTP request header"));
    while (headers.size() - body_offset < body.size()) {
      std::array<char, 1024> buffer;
      auto received = (*socket)->receive(buffer);
      if (!received)
        return std::unexpected(std::move(received.error()));
      if (!*received)
        return std::unexpected(StarException("Truncated HTTP request body"));
      headers.append(buffer.data(), *received);
    }
    if (std::string_view(headers).substr(body_offset) != body)
      return std::unexpected(StarException("Incorrect HTTP request body"));
    return std::move(*socket);
  };
  auto respond = [](TcpSocket& socket, std::string_view status, std::string_view body,
                    std::string_view headers = {}) -> std::expected<void, StarException> {
    auto response = std::format("HTTP/1.1 {}\r\n{}Content-Length: {}\r\nConnection: close\r\n\r\n{}",
                                status, headers, body.size(), body);
    std::span<char const> remaining(response.data(), response.size());
    while (!remaining.empty()) {
      auto sent = socket.send(remaining);
      if (!sent)
        return std::unexpected(std::move(sent.error()));
      if (!*sent)
        return std::unexpected(StarException("HTTP loopback send made no progress"));
      remaining = remaining.subspan(*sent);
    }
    socket.close();
    return {};
  };
  auto await_response = [&]() -> testing::AssertionResult {
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (std::chrono::steady_clock::now() < deadline) {
      auto finished = context->eval<bool>("return response:finished()");
      if (!finished)
        return testing::AssertionFailure() << finished.error().what();
      if (*finished)
        return testing::AssertionSuccess();
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return testing::AssertionFailure() << "HTTP response did not settle";
  };

  std::stop_source active_scope;
  (*engine)->set_promise_lifetime(active_scope.get_token());
  ASSERT_TRUE(lua("active = http.get(url, {timeout = 5})"));
  auto current = session->approval();
  ASSERT_TRUE(current);
  ASSERT_EQ(current->domain, String("127.0.0.1"));
  ASSERT_TRUE(session->resolve_approval(current->id, true, true));
  ASSERT_TRUE(lua("assert(http.isTrusted('127.0.0.1'))"));
  ASSERT_EQ((*configuration)->get_path("safe.luaHttp.trustedSites"), Json(JsonArray{"127.0.0.1"}));
  auto active = accept_request();
  ASSERT_TRUE(active) << active.error().what();
  (*engine)->set_promise_lifetime({});
  ASSERT_TRUE(lua("response = http.get(url, {timeout = 5}); successful = response"));
  ASSERT_FALSE(session->approval());
  auto successful = accept_request();
  ASSERT_TRUE(successful) << successful.error().what();
  active_scope.request_stop();
  (*active)->close(); // Late native completion cannot overwrite scope cancellation.
  ASSERT_TRUE(respond(**successful, "503 Test", "null"));
  ASSERT_TRUE(await_response());
  ASSERT_TRUE(lua(R"LUA(
    assert(active:cancelled() and active:result() == nil)
    assert(successful:succeeded() and not successful:failed() and not successful:cancel())
    assert(successful:result().status == 503 and successful:result().body == "null")
  )LUA"));
  for (std::string_view body : {std::string_view{}, std::string_view{"a\0b", 3}}) {
    ASSERT_TRUE(lua("response = http.get(url, {timeout = 5})"));
    auto peer = accept_request();
    ASSERT_TRUE(peer) << peer.error().what();
    ASSERT_TRUE(respond(**peer, "200 OK", body));
    ASSERT_TRUE(await_response());
    ASSERT_TRUE(body.empty() ? lua("assert(response:succeeded() and response:result().body == '')")
                             : lua(R"(assert(response:succeeded() and response:result().body == "a\0b"))"));
  }
  for (std::string_view method : {"GET", "POST", "PUT", "DELETE", "PATCH"}) {
    ASSERT_TRUE(lua(Star::format(
      R"(response = http.createRequest("{}", url, {{body = "a\0b", headers = {{["X-Label"] = "one\ttwo"}}}}))", method)));
    auto body = method == "DELETE" || method == "GET" ? std::string_view{} : std::string_view{"a\0b", 3};
    auto peer = accept_request(method, body, "\r\nX-Label: one\ttwo\r\n");
    ASSERT_TRUE(peer) << peer.error().what();
    ASSERT_TRUE(respond(**peer, "200 OK", "ack"));
    ASSERT_TRUE(await_response());
    ASSERT_TRUE(lua("assert(response:succeeded() and response:result().body == 'ack')"));
  }

  // Even another URL on the same trusted loopback server must not be fetched implicitly.
  ASSERT_TRUE(lua("response = http.get(url, {timeout = 3})"));
  auto redirect = accept_request();
  ASSERT_TRUE(redirect) << redirect.error().what();
  ASSERT_TRUE(respond(**redirect, "302 Found", "moved", "Location: /next\r\n"));
  ASSERT_TRUE(await_response());
  ASSERT_TRUE(lua("assert(response:result().status == 302 and response:result().body == 'moved')"));
  auto followed = (*listener)->accept((*root)->logger());
  ASSERT_TRUE(followed) << followed.error().what();
  ASSERT_FALSE(*followed);

  // Keep the peer silent. A regression waiting for the infinite request timeout
  // must fail rather than hang the test: the watchdog closes the peer as a last resort.
  ASSERT_TRUE(lua("hanging = http.get(url, {timeout = 0})"));
  auto hanging = accept_request();
  ASSERT_TRUE(hanging) << hanging.error().what();
  std::promise<void> stopped;
  bool fallback_used = false;
  std::jthread watchdog([&, done = stopped.get_future()] {
    if (done.wait_for(std::chrono::seconds(8)) == std::future_status::timeout) {
      fallback_used = true;
      (*hanging)->close();
    }
  });
  auto stop_result = session->stop();
  stopped.set_value();
  watchdog.join();
  ASSERT_FALSE(fallback_used) << "Session stop needed peer-side EOF";
  ASSERT_TRUE(stop_result) << stop_result.error().what();
  ASSERT_TRUE(session->poll());
  ASSERT_TRUE(lua("assert(hanging:cancelled() and active:cancelled() and successful:succeeded())"));
});

[[nodiscard]] std::expected<void, StarException>
writeIsolationModules(String const& overlay, std::initializer_list<std::pair<String, String>> modules) {
  for (auto const& [name, source] : modules) {
    auto written = File::write_file(source, File::relative_to(overlay, name));
    if (!written)
      return std::unexpected(std::move(written.error()));
  }
  return {};
}

const GameTestRegistration status_lifecycle("StatusControllerTest", "ReentrantLuaRemovalPreservesInstanceIdentity", [](GameTest& fixture) {
  auto directory = File::temporary_directory(fixture.root().random());
  ASSERT_TRUE(directory) << directory.error().what();
  auto cleanup = make_scope_exit([&] { EXPECT_TRUE(File::remove_all(*directory)); });
  auto settings = isolation_settings(fixture.rootSettings(), *directory, "status-lifecycle");
  ASSERT_TRUE(settings) << settings.error().what();
  auto overlay = File::relative_to(*directory, "assets");
  ASSERT_TRUE(writeIsolationModules(overlay, {
    {"fallback.statuseffect", R"({"name":"reply-fallback","defaultDuration":5,"scripts":["fallback.lua"]})"},
    {"fallback.lua", R"LUA(
      function init()
        local function fallback()
          status.setStatusProperty("fallbackVisits", status.statusProperty("fallbackVisits", 0) + 1)
          return "fallback"
        end
        message.setHandler("handledNil", fallback)
        message.setHandler("onlyFallback", fallback)
      end
    )LUA"},
    {"probe-a.statuseffect", R"({"name":"probe-a","defaultDuration":5,"scripts":["probe.lua"],"animationConfig":"probe.animation"})"},
    {"probe-b.statuseffect", R"({"name":"probe-b","defaultDuration":5,"scripts":["probe.lua"],"animationConfig":"probe.animation"})"},
    {"probe.animation", R"({"lights":{"lamp":{}}})"},
    {"probe.lua", R"LUA(
      local function bump(key)
        local n = status.statusProperty(key, 0) + 1
        status.setStatusProperty(key, n)
        return n
      end
      function init()
        self.serial = bump("inits")
        effect.addStatModifierGroup({{stat="probePower", amount=1}})
        effect.setToolUsageSuppressed(self.serial <= 2)
        message.setHandler("broadcast", function()
          if bump("broadcastVisits") == 1 then return nil end
          return "second"
        end)
        message.setHandler("observe", function()
          status.setStatusProperty("source", effect.sourceEntity())
          status.setStatusProperty("duration", effect.duration())
        end)
        message.setHandler("mark", function()
          effect.expire()
          assert(status.uniqueStatusEffectActive(effect.name()))
        end)
        message.setHandler("remove", function()
          status.removeEphemeralEffect(effect.name())
          animator.setLightActive("lamp", false)
          status.setStatusProperty("removedInMessage", effect.name())
        end)
      end
      function update()
        bump("updates")
        if self.serial > 2 then return end
        local name = effect.name()
        status.setStatusProperty("winner", name)
        status.removeEphemeralEffect(name == "probe-a" and "probe-b" or "probe-a")
        status.removeEphemeralEffect(name)
        -- onExpire installed a new namesake. These callbacks still address
        -- the outgoing instance, not its new animator, metadata or stat groups.
        assert(status.uniqueStatusEffectActive(name) and effect.sourceEntity() == 11)
        effect.setDuration(123)
        effect.addStatModifierGroup({{stat="probePower", amount=100}})
        animator.setLightActive("lamp", false)
        status.setStatusProperty("returned", true)
      end
      function onExpire()
        local name = effect.name()
        assert(not status.uniqueStatusEffectActive(name))
        status.removeEphemeralEffect(name)
        bump("expirations")
        if self.serial <= 2 and name == status.statusProperty("winner") then
          status.addEphemeralEffect(name, 8, 77)
        end
      end
      function uninit()
        bump("uninits")
      end
    )LUA"}
  }));
  auto root = Root::create(std::move(*settings));
  ASSERT_TRUE(root) << root.error().what();
  auto storage = File::ephemeral_file((*root)->random());
  ASSERT_TRUE(storage) << storage.error().what();
  auto world = WorldServer::create(**root, Vec2U(2048, 2048), *storage);
  ASSERT_TRUE(world) << world.error().what();
  auto close = make_scope_exit([&] { EXPECT_TRUE((*world)->close()); });
  auto npcs = (*root)->npc_database();
  ASSERT_TRUE(npcs) << npcs.error().what();
  auto variant = (*npcs)->generate_npc_variant("human", "villager", 1.0f, 1234, JsonObject{});
  ASSERT_TRUE(variant) << variant.error().what();
  auto npc = (*npcs)->create_npc(std::move(*variant));
  ASSERT_TRUE(npc) << npc.error().what();
  (*npc)->set_position(Vec2F(100, 100));
  ASSERT_TRUE((*world)->addEntity(*npc));
  auto installed = (*npc)->eval_script(R"(
    status.addEphemeralEffect("reply-fallback")
    message.setHandler("handledNil", function() return nil end)
  )");
  if (!fixture.check(bool(installed), "install NPC and status handlers")) return;
  auto handled = (*world)->send_entity_message((*npc)->entityId(), "handledNil");
  auto fallback = (*world)->send_entity_message((*npc)->entityId(), "onlyFallback");
  if (!fixture.check(handled && fallback, "dispatch both local NPC messages")) return;
  auto first = handled->result();
  auto second = fallback->result();
  if (!fixture.check(first && *first && (**first).is_null() && second && *second && **second == Json("fallback")
          && (*npc)->status_controller().statusProperty("fallbackVisits") == Json(1),
      "handled nil stops dispatch; only an unhandled message visits the status script")) return;
  Json config = JsonObject{{"stats", JsonObject{{"probePower", JsonObject{}}}}, {"minimumLiquidStatusEffectPercentage", 1},
      {"appliesEnvironmentStatusEffects", false}, {"appliesWeatherStatusEffects", false}, {"environmentStatusEffectUpdateTimer", 100}};
  auto created = StatusController::create(**root, config);
  ASSERT_TRUE(created) << created.error().what();
  auto& status = **created;
  ASSERT_TRUE(status.init(**npc, (*npc)->movement_controller()));
  auto stop = make_scope_exit([&] { EXPECT_TRUE(status.uninit()); });
  ASSERT_TRUE(status.addEphemeralEffects({{"probe-a", {}}, {"probe-b", {}}}, 11));
  ASSERT_TRUE(status.toolUsageSuppressed());
  auto broadcast = status.receive_message("broadcast", true);
  if (!fixture.check(broadcast && *broadcast && std::holds_alternative<Json>(**broadcast)
          && std::get<Json>(**broadcast).is_null() && status.statusProperty("broadcastVisits") == Json(2),
      "both effects run, but the first handled reply wins even when it is null")) return;
  ASSERT_TRUE(status.tickMaster(1.0f));
  EXPECT_EQ(status.statusProperty("updates"), Json(1));
  EXPECT_EQ(status.statusProperty("returned"), Json(true));
  EXPECT_EQ(status.statusProperty("inits"), Json(3));
  EXPECT_EQ(status.statusProperty("expirations"), Json(2));
  EXPECT_EQ(status.statusProperty("uninits"), Json(2));
  EXPECT_EQ(status.stat("probePower"), 1.0f);
  EXPECT_FALSE(status.toolUsageSuppressed());
  auto name = status.statusProperty("winner").to_string();
  ASSERT_TRUE(name) << name.error().what();
  auto summary = status.activeUniqueStatusEffectSummary();
  ASSERT_EQ(summary.size(), 1u);
  EXPECT_EQ(summary.front().first, *name);
  EXPECT_EQ(summary.front().second, 1.0f);
  ASSERT_TRUE(status.addEphemeralEffect({*name, 8}, 88)); // Equal duration must not steal the source.
  ASSERT_TRUE(status.receive_message("observe", true));
  EXPECT_EQ(status.statusProperty("duration"), Json(8));
  EXPECT_EQ(status.statusProperty("source"), Json(77));
  ASSERT_TRUE(status.tickMaster(2.0f));
  ASSERT_TRUE(status.addEphemeralEffect({*name, {}}, 90)); // Default 5 < remaining 6.
  ASSERT_TRUE(status.receive_message("observe", true));
  EXPECT_EQ(status.statusProperty("source"), Json(77));
  EXPECT_EQ(status.statusProperty("duration"), Json(6));
  ASSERT_TRUE(status.addEphemeralEffect({*name, 7}, 91)); // Extends duration but not the historical peak.
  ASSERT_TRUE(status.receive_message("observe", true));
  EXPECT_EQ(status.statusProperty("source"), Json(91));
  DataStreamBuffer wire;
  ASSERT_TRUE(status.net_store(wire));
  ASSERT_TRUE(wire.seek(0));
  auto receiver = StatusController::create(**root, config);
  ASSERT_TRUE(receiver) << receiver.error().what();
  ASSERT_TRUE((*receiver)->init(**npc, (*npc)->movement_controller()));
  auto stop_receiver = make_scope_exit([&] { EXPECT_TRUE((*receiver)->uninit()); });
  ASSERT_TRUE((*receiver)->net_load(wire, {}));
  auto replicated = (*receiver)->activeUniqueStatusEffectSummary();
  ASSERT_EQ(replicated.size(), 1u);
  EXPECT_EQ(replicated.front().second, 7.0f / 8.0f);
  List<LightSource> light_sources;
  if (!fixture.check(bool((*receiver)->append_light_sources(light_sources)), "collect replicated effect lights")) return;
  fixture.check(light_sources.size() == 1, "replica emits only the surviving effect's light");

  ASSERT_TRUE(status.receive_message("mark", true));
  EXPECT_TRUE(status.uniqueStatusEffectActive(*name)); // effect.expire schedules, it does not remove.
  ASSERT_TRUE(status.tickMaster(0.5f));
  EXPECT_FALSE(status.uniqueStatusEffectActive(*name));
  EXPECT_EQ(status.statusProperty("expirations"), Json(3));
  EXPECT_EQ(status.statusProperty("uninits"), Json(3));
  ASSERT_TRUE(status.addEphemeralEffect({*name, {}}, {}));
  ASSERT_TRUE(status.receive_message("remove", true));
  EXPECT_EQ(status.statusProperty("removedInMessage"), Json(*name));
  EXPECT_EQ(status.statusProperty("expirations"), Json(4));
  EXPECT_EQ(status.statusProperty("uninits"), Json(4));
  EXPECT_EQ(status.stat("probePower"), 0.0f);
  EXPECT_TRUE(status.activeUniqueStatusEffectSummary().empty());
});

const GameTestRegistration status_generation("StatusControllerTest", "ExistingDefaultDurationSurvivesAssetReload", [](GameTest& fixture) {
  auto directory = File::temporary_directory(fixture.root().random());
  ASSERT_TRUE(directory) << directory.error().what();
  auto cleanup = make_scope_exit([&] { EXPECT_TRUE(File::remove_all(*directory)); });
  auto settings = isolation_settings(fixture.rootSettings(), *directory, "status-generation");
  ASSERT_TRUE(settings) << settings.error().what();
  auto overlay = File::relative_to(*directory, "assets");
  settings->assetSources = {overlay};
  ASSERT_TRUE(writeIsolationModules(overlay, {{"preload.config", "[]"},
      {"probe.statuseffect", R"({"name":"probe","defaultDuration":5})"}}));
  auto root = Root::create(std::move(*settings));
  ASSERT_TRUE(root) << root.error().what();
  auto status = StatusController::create(**root, JsonObject{{"minimumLiquidStatusEffectPercentage", 1},
      {"appliesEnvironmentStatusEffects", false}, {"appliesWeatherStatusEffects", false}});
  ASSERT_TRUE(status) << status.error().what();
  ASSERT_TRUE((*status)->addEphemeralEffect({"probe", 1}));
  ASSERT_TRUE(writeIsolationModules(overlay, {{"probe.statuseffect", R"({"name":"probe","defaultDuration":9})"}}));
  ASSERT_TRUE((*root)->reload());
  ASSERT_TRUE((*status)->addEphemeralEffect({"probe", {}}));
  auto old = (*status)->diskStore();
  ASSERT_TRUE(old) << old.error().what();
  EXPECT_EQ(old->query("ephemeralEffects[0].duration"), Json(5));
  ASSERT_EQ((*status)->removeEphemeralEffect("probe"), true);
  ASSERT_TRUE((*status)->addEphemeralEffect({"probe", {}}));
  auto current = (*status)->diskStore();
  ASSERT_TRUE(current) << current.error().what();
  EXPECT_EQ(current->query("ephemeralEffects[0].duration"), Json(9));
  ASSERT_TRUE((*status)->addPersistentEffect("equipment", UniqueStatusEffect("probe")));
  ASSERT_TRUE((*status)->addEphemeralEffect({"probe", 100}));
  EXPECT_EQ((*status)->removeEphemeralEffect("probe"), false);
});

const GameTestRegistration versioning_migrations("VersioningDatabase", "RunsLuaAtEachReachedVersion",
    [](GameTest& fixture) {
  auto directory = File::temporary_directory(fixture.root().random());
  ASSERT_TRUE(directory) << directory.error().what();
  auto cleanup = make_scope_exit([&] { EXPECT_TRUE(File::remove_all(*directory)); });
  auto settings = isolation_settings(fixture.rootSettings(), *directory, "versioning");
  ASSERT_TRUE(settings) << settings.error().what();
  auto overlay = File::relative_to(*directory, "assets");
  settings->assetSources = {overlay};
  ASSERT_TRUE(File::create_directory(File::relative_to(overlay, "versioning")));
  auto written = writeIsolationModules(overlay, {
    {"preload.config", "[]"},
    // No migration may require a usable celestial catalog until the Lua call.
    {"celestial.config", "{}"},
    {"versioning.config", R"({"Chain":2,"OnlySub":4,"Gap":2,"MainGap":2,"Celestial":1})"},
    {"versioning/subVersioning.config", R"({"Chain":{"steps":3},"OnlySub":{"steps":1},"Gap":{"steps":3}})"},
    {"versioning/Chain_0_steps_0_1.lua", "function update(s) return s .. 'a' end"},
    {"versioning/Chain_0_1.lua", "function update(s) return s .. 'b' end"},
    {"versioning/Chain_0_2.lua", "function update(s) error('skipped ordered main edge') end"},
    {"versioning/Chain_1_steps_1_2.lua", "function update(s) return s .. 'c' end"},
    {"versioning/Chain_1_2.lua", "function update(s) return s .. 'd' end"},
    {"versioning/Chain_2_steps_2_3.lua", "function update(s) return s .. 'e' end"},
    {"versioning/OnlySub_4_steps_0_1.lua", "function update(s) return s .. 'only' end"},
    {"versioning/OnlySub_4_steps_0_2.lua", "function update(s) error('skipped ordered sub edge') end"},
    {"versioning/Gap_2_steps_0_1.lua", "function update(s) return s .. 'partial' end"},
    {"versioning/Gap_2_steps_2_3.lua", "function update(s) error('unreachable') end"},
    {"versioning/MainGap_0_1.lua", "function update(s) return s .. 'partial' end"},
    {"versioning/Celestial_0_1.lua", R"(
      function update(s)
        assert(type(celestial.parameters) == 'function')
        local ok, err = pcall(celestial.parameters, '1:2:3')
        assert(not ok, 'invalid celestial catalog unexpectedly accepted')
        return tostring(err)
      end
    )"}
  });
  ASSERT_TRUE(written) << written.error().what();
  auto root = Root::create(std::move(*settings));
  ASSERT_TRUE(root) << root.error().what();
  auto database = (*root)->versioningDatabase();
  ASSERT_TRUE(database) << database.error().what();
  auto const& versions = **database;
  VersionedJson old{"Chain", 0, "", {{"removed-mod", 17}}};
  auto updated = versions.updateVersionedJson(old);
  ASSERT_TRUE(updated) << updated.error().what();
  EXPECT_EQ(updated->content, Json("abcde"));
  EXPECT_EQ(updated->version, 2u);
  EXPECT_EQ(updated->subVersions.value("steps"), 3u);
  EXPECT_EQ(updated->subVersions.value("removed-mod"), 17u);
  EXPECT_EQ(versions.versionedJsonCurrent(*updated), true);

  VersionedJson only_sub{"OnlySub", 4, "", {}};
  EXPECT_EQ(versions.versionedJsonCurrent(only_sub), false);
  EXPECT_EQ(versions.loadVersionedJson(only_sub, "OnlySub"), Json("only"));
  only_sub.subVersions["steps"] = 5;
  EXPECT_FALSE(versions.updateVersionedJson(only_sub)); // Do not silently downgrade a known sub-version.
  VersionedJson gap{"Gap", 2, "", {}};
  auto broken = versions.updateVersionedJson(gap);
  ASSERT_FALSE(broken);
  EXPECT_NE(std::string_view(broken.error().what()).find("requires sub identifier 'steps' version 3, reached 1"),
            std::string_view::npos);
  EXPECT_FALSE(versions.loadVersionedJson(gap, "Gap"));
  EXPECT_FALSE(versions.updateVersionedJson({"MainGap", 0, "", {}}));
  auto celestial = versions.loadVersionedJson({"Celestial", 0, "", {}}, "Celestial");
  ASSERT_TRUE(celestial) << celestial.error().what();
  auto message = celestial->to_string();
  ASSERT_TRUE(message) << message.error().what();
  EXPECT_EQ(message->contains("celestial.config"), true) << message->utf8();
});

const GameTestRegistration questTemplateItemBoundaries("RootTest", "QuestTemplateItemBoundaries", [](GameTest&) {
  for (auto const& config :
       {Json(JsonObject{{"id", "borrowed-quest"}}),
        Json(JsonObject{{"id", "borrowed-quest"}, {"rewards", Json()}, {"requiredItems", Json()}})}) {
    auto quest = QuestTemplate::create(config);
    ASSERT_TRUE(quest) << quest.error().what();
    EXPECT_TRUE((*quest)->rewards.empty());
    EXPECT_TRUE((*quest)->requiredItems.empty());
  }
  auto config = Json::parse_json(R"JSON({
    "id": "borrowed-quest",
    "parameters": {
      "absent": {"type": "item"},
      "null": {"type": "item", "example": null},
      "value": {"type": "item", "example": {"name": "money", "count": 3}}
    },
    "rewards": [[], ["money", ["torch", 2, {"colorIndex": 4}]]],
    "requiredItems": [null, {"name": "money", "count": 5}]
  })JSON");
  ASSERT_TRUE(config) << config.error().what();
  auto quest = QuestTemplate::create(*config);
  ASSERT_TRUE(quest) << quest.error().what();
  EXPECT_FALSE((*quest)->parameterExamples.contains("absent"));
  auto nullExample = (*quest)->parameterExamples.get("null");
  ASSERT_TRUE(nullExample) << nullExample.error().what();
  EXPECT_TRUE(nullExample->get().is_null());
  auto valueExample = (*quest)->parameterExamples.get("value");
  ASSERT_TRUE(valueExample) << valueExample.error().what();
  EXPECT_EQ(valueExample->get().get_uint("count"), 3u);
  ASSERT_EQ((*quest)->rewards.size(), 2u);
  EXPECT_TRUE((*quest)->rewards[0].empty());
  ASSERT_EQ((*quest)->rewards[1].size(), 2u);
  EXPECT_EQ((*quest)->rewards[1][0].name(), "money");
  EXPECT_EQ((*quest)->rewards[1][1].name(), "torch");
  EXPECT_EQ((*quest)->rewards[1][1].count(), 2u);
  EXPECT_EQ((*quest)->rewards[1][1].parameters().get_int("colorIndex"), 4);
  ASSERT_EQ((*quest)->requiredItems.size(), 2u);
  EXPECT_TRUE((*quest)->requiredItems[0].is_null());
  EXPECT_EQ((*quest)->requiredItems[1].count(), 5u);
  for (auto const& fields : {JsonObject{{"rewards", false}}, JsonObject{{"rewards", JsonArray{Json()}}},
                             JsonObject{{"rewards", JsonArray{JsonArray{false}}}}, JsonObject{{"requiredItems", false}},
                             JsonObject{{"requiredItems", JsonArray{false}}}}) {
    auto invalid = Json(fields).set("id", "borrowed-quest");
    ASSERT_TRUE(invalid) << invalid.error().what();
    EXPECT_FALSE(QuestTemplate::create(*invalid));
  }
});

const GameTestRegistration bushShapeBoundaries("RootTest", "BushShapeBoundaries", [](GameTest& fixture) {
  auto directory = File::temporary_directory(fixture.root().random());
  ASSERT_TRUE(directory) << directory.error().what();
  auto cleanup = make_scope_exit([&] { EXPECT_TRUE(File::remove_all(*directory)); });
  auto settings = isolation_settings(fixture.rootSettings(), *directory, "bush-shapes");
  ASSERT_TRUE(settings) << settings.error().what();
  auto overlay = File::relative_to(*directory, "assets");
  ASSERT_TRUE(writeIsolationModules(
    overlay,
    {{"borrowed-no-mods.bush", R"JSON({"name":"borrowed-no-mods","shapes":[{"base":"base.png"}]})JSON"},
     {"borrowed-mods.bush", R"JSON({"name":"borrowed-mods","shapes":[
      {"base":"first.png","mods":{"flower":["red.png","blue.png"]}},
      {"base":"second.png","mods":{}}
    ]})JSON"},
     {"borrowed-null-mods.bush", R"JSON({"name":"borrowed-null-mods","shapes":[{"base":"base.png","mods":null}]})JSON"},
     {"borrowed-null-selection.bush",
      R"JSON({"name":"borrowed-null-selection","shapes":[{"base":"base.png","mods":{"flower":null}}]})JSON"},
     {"borrowed-invalid-base.bush", R"JSON({"name":"borrowed-invalid-base","shapes":[{"mods":{}}]})JSON"},
     {"borrowed-null-shape.bush", R"JSON({"name":"borrowed-null-shape","shapes":[null]})JSON"}}));
  auto root = Root::create(std::move(*settings));
  ASSERT_TRUE(root) << root.error().what();
  auto database = (*root)->plant_database();
  ASSERT_TRUE(database) << database.error().what();
  for (auto const& name : {"borrowed-no-mods", "borrowed-null-mods"}) {
    auto variant = (*database)->build_bush_variant(String(std::string_view(name)), 12.0f, "", 24.0f);
    ASSERT_TRUE(variant) << variant.error().what();
    ASSERT_EQ(variant->shapes.size(), 1u);
    EXPECT_EQ(variant->shapes[0].image, "base.png");
    EXPECT_TRUE(variant->shapes[0].mods.empty());
    EXPECT_EQ(variant->base_hue_shift, 12.0f);
    EXPECT_EQ(variant->mod_hue_shift, 24.0f);
    EXPECT_FALSE((*database)->build_bush_variant(String(std::string_view(name)), 0.0f, "flower", 0.0f));
  }
  auto variant = (*database)->build_bush_variant("borrowed-mods", 12.0f, "flower", 24.0f);
  ASSERT_TRUE(variant) << variant.error().what();
  ASSERT_EQ(variant->shapes.size(), 2u);
  EXPECT_EQ(variant->shapes[0].image, "first.png");
  EXPECT_EQ(variant->shapes[0].mods, (StringList{"red.png", "blue.png"}));
  EXPECT_EQ(variant->shapes[1].image, "second.png");
  EXPECT_TRUE(variant->shapes[1].mods.empty());
  for (auto const& name : {"borrowed-null-selection", "borrowed-invalid-base", "borrowed-null-shape"})
    EXPECT_FALSE((*database)->build_bush_variant(String(std::string_view(name)), 0.0f, "flower", 0.0f));
  auto assets = (*root)->assets();
  ASSERT_TRUE(assets) << assets.error().what();
  auto store = variant->to_json();
  ASSERT_TRUE(store) << store.error().what();
  EXPECT_EQ(store->get_string("bushName"), String("borrowed-mods"));
  EXPECT_EQ(store->get_string("modName"), String("flower"));
  auto restored = BushVariant::create(**assets, *store);
  ASSERT_TRUE(restored) << restored.error().what();
  EXPECT_EQ(restored->to_json(), *store);
});

const GameTestRegistration plant_catalog_variants("PlantCatalogTest", "TreeIdentityLegacyFlagsAndDuplicatePriority", [](GameTest& fixture) {
  auto directory = File::temporary_directory(fixture.root().random());
  ASSERT_TRUE(directory) << directory.error().what();
  auto cleanup = make_scope_exit([&] { EXPECT_TRUE(File::remove_all(*directory)); });
  auto settings = isolation_settings(fixture.rootSettings(), *directory, "plant-catalog");
  ASSERT_TRUE(settings) << settings.error().what();
  auto overlay = File::relative_to(*directory, "assets");
  ASSERT_TRUE(File::create_directory(File::relative_to(overlay, "catalog-a")));
  ASSERT_TRUE(File::create_directory(File::relative_to(overlay, "catalog-b")));
  ASSERT_TRUE(writeIsolationModules(overlay, {
    {"catalog-a/duplicate.modularstem", R"JSON({
      "name":"catalog-probe","shape":"catalog-probe","ephemeral":true,"allowsBlockPlacement":false,
      "avianDescription":"First fixture", "health":5, "dropConfig":{"catalogKind":"stem"},
      "base":{"base":{"image":"/catalog-stem.png","attachment":{"bx":0,"by":0,"x":0,"y":8}}},
      "middle":{},"middleMinSize":0,"middleMaxSize":0
    })JSON"},
    {"catalog-b/duplicate.modularstem", R"JSON({
      "name":"catalog-probe","shape":"catalog-probe","ephemeral":true,"allowsBlockPlacement":false,
      "avianDescription":"Second fixture", "health":5, "dropConfig":{"catalogKind":"stem"},
      "base":{"base":{"image":"/catalog-stem.png","attachment":{"bx":0,"by":0,"x":0,"y":8}}},
      "middle":{},"middleMinSize":0,"middleMaxSize":0
    })JSON"},
    {"catalog-reverse.modularstem", R"JSON({
      "name":"catalog-reverse","shape":"catalog-probe","ephemeral":false,"allowsBlockPlacement":true
    })JSON"},
    {"catalog-fixture.modularfoliage", R"JSON({
      "name":"catalog-leaf","shape":"catalog-probe","dropConfig":{"catalogKind":"foliage"}
    })JSON"},
    {"catalog-fixture.grass", R"JSON({
      "name":"catalog-grass","images":["first.png","second.png"],"description":"Catalog grass","ephemeral":false
    })JSON"},
  }));
  Image image(tile_pixels, tile_pixels, PixelFormat::RGBA32);
  for (unsigned y = 0; y < tile_pixels; ++y)
    for (unsigned x = 0; x < tile_pixels; ++x)
      ASSERT_TRUE(image.set(x, y, Vec4B(120, 80, 40, 255)));
  auto png = std::make_shared<Buffer>();
  ASSERT_TRUE(image.write_png(fixture.root().logger(), png));
  ASSERT_TRUE(File::write_file(png->take_data(), File::relative_to(overlay, "catalog-stem.png")));

  auto root = Root::create(std::move(*settings));
  ASSERT_TRUE(root) << root.error().what();
  auto assets = (*root)->assets();
  ASSERT_TRUE(assets) << assets.error().what();
  auto scanned = (*assets)->scan_extension("modularstem");
  ASSERT_TRUE(scanned) << scanned.error().what();
  auto const& scan_order = scanned->get();
  auto is_duplicate = [](CaseInsensitiveString const& asset) {
    return asset.string() == "/catalog-a/duplicate.modularstem" || asset.string() == "/catalog-b/duplicate.modularstem";
  };
  ASSERT_EQ(std::ranges::count_if(scan_order, is_duplicate), 2);
  auto first = std::ranges::find_if(scan_order, is_duplicate);
  ASSERT_TRUE(first != scan_order.end());
  auto first_settings = (*assets)->json(first->string());
  ASSERT_TRUE(first_settings) << first_settings.error().what();
  auto first_object = first_settings->object_ptr();
  ASSERT_TRUE(first_object) << first_object.error().what();

  auto database = (*root)->plant_database();
  ASSERT_TRUE(database) << database.error().what();
  auto const& catalog = **database;
  auto winning_directory = AssetPath::directory(first->string());
  EXPECT_EQ(catalog.tree_stem_directory("catalog-probe"), winning_directory);
  EXPECT_EQ(catalog.tree_stem_shape("catalog-probe"), String("catalog-probe"));
  EXPECT_EQ(catalog.tree_foliage_shape("catalog-leaf"), String("catalog-probe"));
  auto listed_config = catalog.tree_stem_config("catalog-probe");
  ASSERT_TRUE(listed_config) << listed_config.error().what();
  EXPECT_EQ(listed_config->get("config"), *first_settings);
  EXPECT_EQ(listed_config->get_string("path"), winning_directory);

  auto leafless = catalog.build_tree_variant("catalog-probe", 12.0f);
  ASSERT_TRUE(leafless) << leafless.error().what();
  auto leafy = catalog.build_tree_variant("catalog-probe", 12.0f, "catalog-leaf", -6.0f);
  ASSERT_TRUE(leafy) << leafy.error().what();
  EXPECT_EQ(leafless->stem_name, "catalog-probe");
  EXPECT_EQ(leafless->stem_directory, winning_directory);
  EXPECT_EQ(leafless->stem_settings.object_ptr(), *first_object);
  auto avian = first_settings->get("avianDescription");
  ASSERT_TRUE(avian) << avian.error().what();
  EXPECT_EQ(leafless->descriptions.get("avianDescription"), *avian);
  EXPECT_EQ(leafless->descriptions.get_string("description"), String("catalog-probe"));
  EXPECT_EQ(leafless->stem_drop_config.get_string("catalogKind"), String("stem"));
  EXPECT_TRUE(leafless->ephemeral);
  EXPECT_TRUE(leafless->foliage_name.empty());
  EXPECT_EQ(leafless->foliage_settings, Json(JsonObject{}));
  EXPECT_EQ(leafless->foliage_drop_config, Json(JsonObject{}));
  EXPECT_EQ(leafy->stem_settings.object_ptr(), *first_object);
  EXPECT_EQ(leafy->foliage_name, "catalog-leaf");
  EXPECT_EQ(leafy->foliage_hue_shift, -6.0f);
  EXPECT_EQ(leafy->descriptions.get_string("description"), String("catalog-probe with catalog-leaf"));
  EXPECT_EQ(leafy->foliage_drop_config.get_string("catalogKind"), String("foliage"));
  EXPECT_FALSE(leafy->ephemeral);

  auto reverse_leafless = catalog.build_tree_variant("catalog-reverse", 0.0f);
  ASSERT_TRUE(reverse_leafless) << reverse_leafless.error().what();
  auto reverse_leafy = catalog.build_tree_variant("catalog-reverse", 0.0f, "catalog-leaf", 0.0f);
  ASSERT_TRUE(reverse_leafy) << reverse_leafy.error().what();
  EXPECT_FALSE(reverse_leafless->ephemeral);
  EXPECT_TRUE(reverse_leafy->ephemeral);

  auto grass = catalog.build_grass_variant("catalog-grass", 17.0f);
  ASSERT_TRUE(grass) << grass.error().what();
  EXPECT_EQ(grass->images, (StringList{"first.png", "second.png"}));
  EXPECT_EQ(grass->descriptions.get_string("description"), String("Catalog grass"));
  EXPECT_FALSE(grass->ephemeral);
  auto grass_store = grass->to_json();
  ASSERT_TRUE(grass_store) << grass_store.error().what();
  EXPECT_EQ(grass_store->get_float("hueShift"), 17.0f);
  auto grass_restored = GrassVariant::create(**assets, *grass_store);
  ASSERT_TRUE(grass_restored) << grass_restored.error().what();
  EXPECT_EQ(grass_restored->to_json(), *grass_store);

  auto stored = leafless->to_json();
  ASSERT_TRUE(stored) << stored.error().what();
  EXPECT_EQ(stored->get_string("stemName"), String("catalog-probe"));
  EXPECT_EQ(stored->get_float("stemHueShift"), 12.0f);
  EXPECT_EQ(stored->query_float("tileDamageParameters.totalHealth"), 5.0f);
  auto restored = TreeVariant::create(**assets, *stored);
  ASSERT_TRUE(restored) << restored.error().what();
  EXPECT_EQ(restored->to_json(), *stored);

  auto plant = catalog.create_plant(*restored, 0x9123ull);
  ASSERT_TRUE(plant) << plant.error().what();
  auto plant_store = (*plant)->disk_store();
  ASSERT_TRUE(plant_store) << plant_store.error().what();
  EXPECT_EQ(plant_store->get("saplingDropConfig"),
      Json(JsonObject{{"stemName", "catalog-probe"}, {"stemHueShift", 12.0f}, {"foliageName", ""}, {"foliageHueShift", 0.0f}}));
});

const GameTestRegistration gender_name_rules("NameGeneratorConsumerTest", "SpeciesAndNpcUseTheirOwnGenderRules", [](GameTest& fixture) {
  auto directory = File::temporary_directory(fixture.root().random());
  ASSERT_TRUE(directory) << directory.error().what();
  auto cleanup = make_scope_exit([&] { EXPECT_TRUE(File::remove_all(*directory)); });
  auto settings = isolation_settings(fixture.rootSettings(), *directory, "gender-name-rules");
  ASSERT_TRUE(settings) << settings.error().what();
  auto overlay = File::relative_to(*directory, "assets");
  ASSERT_TRUE(File::create_directory(File::relative_to(overlay, "species")));
  auto written = writeIsolationModules(overlay, {
    {"species/human.species.patch",
     R"([{"op":"replace","path":"/nameGen","value":["/species-male-fixture.namegen","/species-female-fixture.namegen"]}])"},
    {"species-male-fixture.namegen", R"([{"mode":"serie"},"SpeciesMaleFixture"])"},
    {"species-female-fixture.namegen", R"([{"mode":"serie"},"SpeciesFemaleFixture"])"},
    {"npc-male-fixture.namegen", R"([{"mode":"serie"},"NpcMaleFixture"])"},
    {"npc-female-fixture.namegen", R"([{"mode":"serie"},"NpcFemaleFixture"])"},
  });
  ASSERT_TRUE(written) << written.error().what();
  auto root = Root::create(std::move(*settings));
  ASSERT_TRUE(root) << root.error().what();
  auto species = (*root)->speciesDatabase();
  ASSERT_TRUE(species) << species.error().what();
  auto npcs = (*root)->npc_database();
  ASSERT_TRUE(npcs) << npcs.error().what();

  auto generate_npc = [&](std::uint64_t seed, JsonObject overrides) {
    return (*npcs)->generate_npc_variant("human", "villager", 1.0f, seed, std::move(overrides));
  };
  constexpr std::uint64_t male_seed = 1;
  constexpr std::uint64_t female_seed = 0;
  JsonArray full_rules{"/npc-male-fixture.namegen", "/npc-female-fixture.namegen"};
  for (auto gender : {Gender::Male, Gender::Female}) {
    bool male = gender == Gender::Male;
    // Force the species gender opposite to the one selected by this seed.
    auto humanoid = (*species)->generate_humanoid("human", male ? female_seed : male_seed, gender);
    ASSERT_TRUE(humanoid) << humanoid.error().what();
    EXPECT_EQ(humanoid->identity.gender, gender);
    EXPECT_EQ(humanoid->identity.name, std::string_view(male ? "SpeciesMaleFixture" : "SpeciesFemaleFixture"));

    auto npc = generate_npc(male ? male_seed : female_seed, JsonObject{{"nameGen", full_rules}});
    ASSERT_TRUE(npc) << npc.error().what();
    EXPECT_EQ(npc->humanoid_identity.gender, gender);
    EXPECT_EQ(npc->humanoid_identity.name, std::string_view(male ? "NpcMaleFixture" : "NpcFemaleFixture"));
  }

  JsonArray male_only{"/npc-male-fixture.namegen"};
  auto male_npc = generate_npc(male_seed, JsonObject{{"nameGen", male_only}, {"identity", JsonObject{{"gender", "female"}}}});
  ASSERT_TRUE(male_npc) << male_npc.error().what();
  EXPECT_EQ(male_npc->humanoid_identity.gender, Gender::Female);
  EXPECT_EQ(male_npc->humanoid_identity.name, "NpcMaleFixture");

  auto missing_female = generate_npc(female_seed, JsonObject{{"nameGen", male_only}});
  ASSERT_FALSE(missing_female);
  EXPECT_TRUE(missing_female.error().is<NpcException>());
  EXPECT_NE(std::string_view(missing_female.error().what()).find("gender index 1"), std::string_view::npos);
  auto missing_male = generate_npc(male_seed, JsonObject{{"nameGen", JsonArray{}}});
  ASSERT_FALSE(missing_male);
  EXPECT_TRUE(missing_male.error().is<NpcException>());
  EXPECT_NE(std::string_view(missing_male.error().what()).find("gender index 0"), std::string_view::npos);

  auto literal = generate_npc(female_seed, JsonObject{{"npcname", "LiteralFixture"}, {"nameGen", JsonArray{}}});
  ASSERT_TRUE(literal) << literal.error().what();
  EXPECT_EQ(literal->humanoid_identity.name, "LiteralFixture");
});

const GameTestRegistration npcEquipmentBoundaries("RootTest", "NpcEquipmentBoundaries", [](GameTest& fixture) {
  auto database = fixture.root().npc_database();
  ASSERT_TRUE(database) << database.error().what();
  auto overrides = Json::parse_json(R"JSON({
    "levelVariance":[0,0], "npcname":"Borrowed", "matchColorIndices":true,
    "items":{"override":[
      [0,[{"head":["early"]}]],
      [8,[{"head":["higher"]}]],
      [2,[{
        "head":[{"name":"late-head","parameters":{"colorIndex":[2,4]}}],
        "chest":[{"name":"late-chest","parameters":{"colorIndex":[2,4]}}]
      }]],
      [20,[{"head":["unqualified"]}]]
    ]}
  })JSON");
  ASSERT_TRUE(overrides) << overrides.error().what();
  auto variant = (*database)->generate_npc_variant("human", "villager", 10.0f, 0x12345678, *overrides);
  ASSERT_TRUE(variant) << variant.error().what();
  auto head = variant->items.get("head");
  auto chest = variant->items.get("chest");
  ASSERT_TRUE(head) << head.error().what();
  ASSERT_TRUE(chest) << chest.error().what();
  EXPECT_EQ(head->get().name(), "late-head");
  EXPECT_EQ(chest->get().name(), "late-chest");
  auto color = head->get().parameters().get_int("colorIndex");
  ASSERT_TRUE(color) << color.error().what();
  EXPECT_TRUE(*color == 2 || *color == 4);
  EXPECT_EQ(chest->get().parameters().get_int("colorIndex"), *color);

  // Removing unselected levels cannot change any generated variant field or
  // equipment RNG draw. The last qualifying entry wins, not the greatest level.
  auto items = overrides->get("items");
  ASSERT_TRUE(items) << items.error().what();
  auto levels = items->get("override");
  ASSERT_TRUE(levels) << levels.error().what();
  auto lastSelected = levels->get(2);
  ASSERT_TRUE(lastSelected) << lastSelected.error().what();
  auto collapsed = overrides->set("items", JsonObject{{"override", JsonArray{*lastSelected}}});
  ASSERT_TRUE(collapsed) << collapsed.error().what();
  auto expected = (*database)->generate_npc_variant("human", "villager", 10.0f, 0x12345678, *collapsed);
  ASSERT_TRUE(expected) << expected.error().what();
  expected->overrides = *overrides;
  auto actualJson = (*database)->write_npc_variant_to_json(*variant);
  auto expectedJson = (*database)->write_npc_variant_to_json(*expected);
  ASSERT_TRUE(actualJson) << actualJson.error().what();
  ASSERT_TRUE(expectedJson) << expectedJson.error().what();
  EXPECT_EQ(*actualJson, *expectedJson);
  auto actualBytes = (*database)->write_npc_variant(*variant);
  auto expectedBytes = (*database)->write_npc_variant(*expected);
  ASSERT_TRUE(actualBytes) << actualBytes.error().what();
  ASSERT_TRUE(expectedBytes) << expectedBytes.error().what();
  EXPECT_EQ(*actualBytes, *expectedBytes);

  for (auto const& source :
       {R"JSON({"items":{"override":[[0,[]]]}})JSON", R"JSON({"items":{"override":[[0,[{"head":[]}]]]}})JSON",
        R"JSON({"items":{"override":[[0,[{"head":[{"name":"empty-color","parameters":{"colorIndex":[]}}]}]]]}})JSON"}) {
    auto invalid = Json::parse_json(String(std::string_view(source)));
    ASSERT_TRUE(invalid) << invalid.error().what();
    EXPECT_FALSE((*database)->generate_npc_variant("human", "villager", 10.0f, 0x12345678, *invalid));
  }
});

const GameTestRegistration npc_inheritance_and_legacy_variant("RootTest", "NpcInheritanceAndLegacyVariant", [](GameTest& fixture) {
  auto directory = File::temporary_directory(fixture.root().random());
  ASSERT_TRUE(directory) << directory.error().what();
  auto cleanup = make_scope_exit([&] { EXPECT_TRUE(File::remove_all(*directory)); });
  auto settings = isolation_settings(fixture.rootSettings(), *directory, "npc-inheritance");
  ASSERT_TRUE(settings) << settings.error().what();
  auto written = writeIsolationModules(File::relative_to(*directory, "assets"), {
    {"npc-fixture-base.npctype", R"JSON({"type":"npc-fixture-base","baseType":"villager",
      "probe":{"grandparent":1,"preserve":2},"description":"configured description",
      "humanoidParameters":{"fixtureConfigured":true},"items":{"override":[]}})JSON"},
    {"npc-fixture-mid.npctype", R"JSON({"type":"npc-fixture-mid","baseType":"npc-fixture-base","probe":5})JSON"},
    {"npc-fixture-child.npctype", R"JSON({"type":"npc-fixture-child","baseType":"npc-fixture-mid","probe":{"child":3}})JSON"},
    {"npc-cycle-a.npctype", R"JSON({"type":"npc-cycle-a","baseType":"npc-cycle-b"})JSON"},
    {"npc-cycle-b.npctype", R"JSON({"type":"npc-cycle-b","baseType":"npc-cycle-a"})JSON"},
    {"npc-cycle-tail.npctype", R"JSON({"type":"npc-cycle-tail","baseType":"npc-cycle-a"})JSON"},
    {"npc-self-cycle.npctype", R"JSON({"type":"npc-self-cycle","baseType":"npc-self-cycle"})JSON"},
  });
  ASSERT_TRUE(written) << written.error().what();
  auto root = Root::create(std::move(*settings));
  ASSERT_TRUE(root) << root.error().what();
  auto database = (*root)->npc_database();
  ASSERT_TRUE(database) << database.error().what();
  auto const& catalog = **database;
  for (String const& name : StringList{"npc-cycle-a", "npc-cycle-tail", "npc-self-cycle"}) {
    auto result = catalog.build_config(name, JsonObject{{"baseType", "villager"}});
    ASSERT_FALSE(result);
    EXPECT_TRUE(result.error().is<NpcException>());
    EXPECT_NE(std::string_view(result.error().what()).find("inheritance cycle"), std::string_view::npos);
  }

  Json overrides = JsonObject{{"probe", JsonObject{{"preserve", Json()}, {"override", 4}}}};
  auto config = catalog.build_config("npc-fixture-child", overrides);
  ASSERT_TRUE(config) << config.error().what();
  auto probe = config->get("probe");
  ASSERT_TRUE(probe) << probe.error().what();
  EXPECT_EQ(*probe, Json(JsonObject{{"grandparent", 1}, {"preserve", 2}, {"child", 3}, {"override", 4}}));
  auto mid = catalog.build_config("npc-fixture-mid");
  ASSERT_TRUE(mid) << mid.error().what();
  EXPECT_EQ(mid->get_int("probe"), 5);

  auto generated = catalog.generate_npc_variant("human", "npc-fixture-child", 1.0f, 1234, overrides);
  ASSERT_TRUE(generated) << generated.error().what();
  generated->description = "encoded description";
  generated->humanoid_parameters = JsonObject{{"fixtureEncoded", true}};
  generated->initial_script_delta = 42;
  generated->persistent = true;
  generated->keep_alive = true;
  generated->damage_team = 7;
  for (unsigned version : {10u, 11u}) {
    SCOPED_TRACE(version);
    NetCompatibilityRules rules(version);
    auto bytes = catalog.write_npc_variant(*generated, rules);
    ASSERT_TRUE(bytes) << bytes.error().what();
    auto result = catalog.read_npc_variant(*bytes, rules);
    ASSERT_TRUE(result) << result.error().what();
    auto description = version == 10 ? String("configured description") : String("encoded description");
    auto parameter = version == 10 ? String("fixtureConfigured") : String("fixtureEncoded");
    EXPECT_EQ(result->description, std::optional<String>(std::move(description)));
    EXPECT_EQ(result->humanoid_parameters, (JsonObject{{std::move(parameter), true}}));
    EXPECT_EQ(result->initial_script_delta, 42u);
    EXPECT_TRUE(result->persistent);
    EXPECT_TRUE(result->keep_alive);
    EXPECT_EQ(result->damage_team, 7u);
    EXPECT_EQ(result->damage_team_type, generated->damage_team_type);
    EXPECT_EQ(catalog.write_npc_variant(*result, rules), *bytes);
  }
  auto store = catalog.write_npc_variant_to_json(*generated);
  ASSERT_TRUE(store) << store.error().what();
  auto old_store = store->erase("description").and_then([](Json const& json) { return json.erase("humanoidParameters"); });
  ASSERT_TRUE(old_store) << old_store.error().what();
  auto disk = catalog.read_npc_variant_from_json(*old_store);
  ASSERT_TRUE(disk) << disk.error().what();
  EXPECT_FALSE(disk->description);
  EXPECT_TRUE(disk->humanoid_parameters.empty());
});

const GameTestRegistration monster_numeric_fields("RootTest", "MonsterNumericFieldBounds", [](GameTest& fixture) {
  auto database = fixture.root().monster_database();
  if (!fixture.check(bool(database), "load monster catalog")) return;
  auto variant = [&](String const& field, Json value) {
    return (*database)->monster_variant("poptop", 123, JsonObject{{field, std::move(value)}});
  };
  auto team = variant("damageTeam", 255.0);
  auto delta = variant("initialScriptDelta", 0);
  fixture.check(team && team->damage_team == 255 && delta && delta->initial_script_delta == 0,
      "accept exact team and script interval boundaries");
  for (auto value : std::array<Json, 3>{-1, 1.5, 256})
    fixture.check(!variant("damageTeam", value), "reject team alias before narrowing");
  for (auto value : std::array<Json, 3>{-1, 0.5, std::int64_t{1} << 32})
    fixture.check(!variant("initialScriptDelta", value), "reject invalid script interval before narrowing");
});

const GameTestRegistration monster_drop_pool_boundaries("RootTest", "MonsterDropPoolBoundaries", [](GameTest& fixture) {
  auto database = fixture.root().monster_database();
  ASSERT_TRUE(database) << database.error().what();
  auto pool = Json(JsonObject{{"default", "borrowed-pool"}, {"fire", "borrowed-fire-pool"}});
  for (auto const& config : {Json(JsonArray{}), Json("borrowed-pool"), Json(JsonArray{pool})}) {
    auto variant = (*database)->monster_variant("poptop", 0x12345678, JsonObject{{"dropPools", config}});
    ASSERT_TRUE(variant) << variant.error().what();
    auto expected = config.is_type(Json::Type::Array) ? (config == Json(JsonArray{}) ? Json() : pool) : config;
    EXPECT_EQ(variant->drop_pool_config, expected);
  }
});

const GameTestRegistration monster_part_merge("RootTest", "MonsterPartShallowMerge", [](GameTest& fixture) {
  auto directory = File::temporary_directory(fixture.root().random());
  ASSERT_TRUE(directory) << directory.error().what();
  auto cleanup = make_scope_exit([&] { EXPECT_TRUE(File::remove_all(*directory)); });
  auto settings = isolation_settings(fixture.rootSettings(), *directory, "monster-part-merge");
  ASSERT_TRUE(settings) << settings.error().what();
  auto overlay = File::relative_to(*directory, "assets");
  ASSERT_TRUE(writeIsolationModules(overlay, {
    {"merge-probe.animation", "{}"},
    {"merge-probe.monstertype", R"JSON({
      "type":"merge-probe","categories":["merge-probe"],"parts":["body","tail","eyes","legs"],
      "animation":"merge-probe.animation",
      "baseParameters":{"scripts":["/base.lua"],"baseSkills":null,"specialSkills":["merge-skill-b"],
        "scale":1,"metaBoundBox":[0,0,1,1],"mouthOffset":[0,0],"feetOffset":[0,0],
        "statusSettings":{},"selfDamagePoly":[[0,0],[1,0],[0,1]]},
      "partParameterDescription":{
        "list":["mErGe",[1,2]],"map":["merge",{"left":1,"nested":{"old":1}}],
        "same":["merge",1],"different":["merge",1],"absent":["merge",null],"ignored":["merge",true],
        "reset":["merge",[-1]],"keepNull":["merge",[1]],"unknown":["future",42],"latent":[false,"untouched"],
        "sum":["aDD",10],"product":["MULTIPLY",2],"overrideValue":["oVerride","default"],
        "scripts":["merge",["/default.lua"]],"baseSkills":["merge",["merge-skill-a"]],
        "specialSkills":["merge",null]
      }
    })JSON"},
    {"merge-probe.monsterpart", R"JSON({
      "name":"merge-probe-body","category":"merge-probe","type":"body","frames":{},
      "parameters":{"list":[3,4],"map":{"right":2,"nested":{"new":2}},
        "same":2,"different":"changed","absent":[5],"ignored":null,"reset":[0],"keepNull":[2],
        "unknown":"unused input","sum":3,"product":4,"overrideValue":"body","scripts":["/body.lua"]}
    })JSON"},
    {"merge-tail.monsterpart", R"JSON({
      "name":"tail","category":"merge-probe","type":"tail","frames":{},
      "parameters":{"list":[5],"reset":false,"keepNull":null,"sum":-1,"product":3,"overrideValue":null}
    })JSON"},
    {"merge-eyes.monsterpart", R"JSON({
      "name":"eyes","category":"merge-probe","type":"eyes","frames":{},
      "parameters":{"reset":[1],"scripts":[]}
    })JSON"},
    {"merge-legs.monsterpart", R"JSON({
      "name":"legs","category":"merge-probe","type":"legs","frames":{},
      "parameters":{"list":[6],"reset":[2],"keepNull":[],"scripts":["/legs.lua"]}
    })JSON"},
    {"merge-bad-method.monsterpart", R"JSON({
      "name":"bad-method","category":"merge-probe","type":"body","frames":{},"parameters":{"latent":null}
    })JSON"},
    {"merge-missing-rule.monsterpart", R"JSON({
      "name":"missing-rule","category":"merge-probe","type":"body","frames":{},"parameters":{"missing":1}
    })JSON"},
    {"merge-bad-default.monstertype", R"JSON({
      "type":"merge-bad-default","categories":["merge-probe"],"parts":[],"animation":"merge-probe.animation",
      "partParameterDescription":{"unused":["merge"]}
    })JSON"},
    {"merge-skill-a.monsterskill", R"JSON({
      "name":"merge-skill-a","label":"a","image":"a.png",
      "parameters":{"scripts":["/skill-a.lua"],"baseSkills":["later-a"],"specialSkills":null}
    })JSON"},
    {"merge-skill-b.monsterskill", R"JSON({
      "name":"merge-skill-b","label":"b","image":"b.png",
      "parameters":{"scripts":["/skill-b.lua"],"baseSkills":["later-b"],"specialSkills":["later-special"]}
    })JSON"},
    {"merge-bad-script.monsterskill", R"JSON({
      "name":"merge-bad-script","label":"broken","image":"broken.png","parameters":{"scripts":false}
    })JSON"}
  }));
  auto root = Root::create(std::move(*settings));
  ASSERT_TRUE(root) << root.error().what();
  auto database = (*root)->monster_database();
  ASSERT_TRUE(database) << database.error().what();
  auto selected_body = [](String const& name) {
    return JsonObject{{"selectedParts", JsonObject{{"body", name}}}};
  };
  auto variant = (*database)->monster_variant("merge-probe", 42, selected_body("merge-probe-body"));
  ASSERT_TRUE(variant) << variant.error().what();
  auto expected = Json::parse_json(R"JSON({
    "list":[1,2,3,4,5,6],"map":{"left":1,"nested":{"new":2},"right":2},
    "same":1,"different":"changed","absent":[5],"ignored":true,"reset":[1,2],"keepNull":[1,2],
    "unknown":42,"latent":"untouched","sum":12,"product":24,"overrideValue":"body",
    "scripts":["/base.lua","/default.lua","/body.lua","/legs.lua","/skill-a.lua","/skill-b.lua"],
    "skills":["merge-skill-a","merge-skill-b"],"baseSkills":["merge-skill-a","later-a","later-b"],
    "specialSkills":["merge-skill-b","later-special"]
  })JSON");
  ASSERT_TRUE(expected) << expected.error().what();
  auto fields = expected->iterate_object();
  ASSERT_TRUE(fields) << fields.error().what();
  for (auto const& [key, value] : *fields)
    EXPECT_EQ(variant->parameters.get(key), value) << key.utf8();

  auto bad_method = (*database)->monster_variant("merge-probe", 42, selected_body("bad-method"));
  ASSERT_FALSE(bad_method);
  EXPECT_NE(std::string_view(bad_method.error().what()).find("Cannot convert from bool to string"), std::string_view::npos);
  auto missing_rule = (*database)->monster_variant("merge-probe", 42, selected_body("missing-rule"));
  ASSERT_FALSE(missing_rule);
  EXPECT_NE(std::string_view(missing_rule.error().what()).find("No such key in Json::get(\"missing\")"), std::string_view::npos);
  auto bad_default = (*database)->monster_variant("merge-bad-default", 42);
  ASSERT_FALSE(bad_default);
  EXPECT_NE(std::string_view(bad_default.error().what()).find("Json::get(1) out of range"), std::string_view::npos);
  auto bad_script_parameters = selected_body("merge-probe-body");
  bad_script_parameters["specialSkills"] = JsonArray{"merge-bad-script"};
  auto bad_script = (*database)->monster_variant("merge-probe", 42, std::move(bad_script_parameters));
  ASSERT_FALSE(bad_script);
  EXPECT_NE(std::string_view(bad_script.error().what()).find("Improper conversion to JsonArray from bool"), std::string_view::npos);
});

const GameTestRegistration monster_generation_rng("RootTest", "MonsterGenerationRng", [](GameTest& fixture) {
  auto directory = File::temporary_directory(fixture.root().random());
  ASSERT_TRUE(directory) << directory.error().what();
  auto cleanup = make_scope_exit([&] { EXPECT_TRUE(File::remove_all(*directory)); });
  auto settings = isolation_settings(fixture.rootSettings(), *directory, "monster-generation");
  ASSERT_TRUE(settings) << settings.error().what();
  auto overlay = File::relative_to(*directory, "assets");
  ASSERT_TRUE(writeIsolationModules(overlay, {
    {"generation-probe.animation", "{}"},
    {"generation-probe.monstertype", R"JSON({
      "type":"generation-probe","categories":["generation-probe"],"parts":["body","tail"],
      "animation":"generation-probe.animation","dropPools":["pool0","pool1","pool2"],
      "baseParameters":{"scripts":[],"scale":1,"metaBoundBox":[0,0,1,1],"mouthOffset":[0,0],"feetOffset":[0,0],
        "statusSettings":{"minimumLiquidStatusEffectPercentage":1,"appliesEnvironmentStatusEffects":false,
          "appliesWeatherStatusEffects":false},"selfDamagePoly":[[0,0],[1,0],[0,1]]}
    })JSON"},
    {"generation-empty.monstertype", R"JSON({
      "type":"generation-empty","categories":["generation-empty"],"parts":[],
      "animation":"generation-probe.animation",
      "baseParameters":{"scripts":[],"scale":1,"metaBoundBox":[0,0,1,1],"mouthOffset":[0,0],"feetOffset":[0,0],
        "statusSettings":{},"selfDamagePoly":[[0,0],[1,0],[0,1]]}
    })JSON"},
    {"generation-a.monsterpart", R"JSON({
      "name":"a","category":"generation-probe","type":"body","frames":{"body":"body-a.png","shared":"body-a.png"}
    })JSON"},
    {"generation-b.monsterpart", R"JSON({
      "name":"b","category":"generation-probe","type":"body","frames":{"body":"body-b.png","shared":"body-b.png"}
    })JSON"},
    {"generation-c.monsterpart", R"JSON({
      "name":"c","category":"generation-probe","type":"body","frames":{"body":5}
    })JSON"},
    {"generation-x.monsterpart", R"JSON({
      "name":"x","category":"generation-probe","type":"tail","frames":{"tail":"tail-x.png","shared":"tail-x.png"}
    })JSON"},
    {"generation-y.monsterpart", R"JSON({
      "name":"y","category":"generation-probe","type":"tail","frames":{"tail":"tail-y.png","shared":"tail-y.png"}
    })JSON"},
    {"generation-a.monsterskill", R"JSON({
      "name":"generation-a","label":"a","image":"a.png",
      "parameters":{"generationMarker":"a"},"animationParameters":{"generationAnimation":"a"}
    })JSON"},
    {"generation-d.monsterskill", R"JSON({
      "name":"generation-d","label":"d","image":"d.png",
      "parameters":{"generationMarker":"d"},"animationParameters":{"generationAnimation":"d"}
    })JSON"}
  }));
  auto root = Root::create(std::move(*settings));
  ASSERT_TRUE(root) << root.error().what();
  auto database = (*root)->monster_database();
  ASSERT_TRUE(database) << database.error().what();
  auto& monsters = **database;
  constexpr std::uint64_t seed = 0x8123456789abcdef;

  Json parameters = JsonObject{
      {"seed", "9305357566071262703"},
      {"skills", JsonArray{"generation-a", "generation-b", "generation-c", "generation-d", "generation-e"}},
      {"skillCount", 3}};
  auto generated = monsters.random_monster("generation-probe", parameters);
  ASSERT_TRUE(generated) << generated.error().what();
  EXPECT_EQ(generated->seed, seed);
  EXPECT_EQ(generated->animator_part_tags.value("body"), "/body-b.png");
  EXPECT_EQ(generated->animator_part_tags.value("tail"), "/tail-y.png");
  EXPECT_EQ(generated->animator_part_tags.value("shared"), "/tail-y.png");
  auto generated_skills = generated->parameters.get("skills");
  ASSERT_TRUE(generated_skills) << generated_skills.error().what();
  EXPECT_EQ(*generated_skills, Json(JsonArray{"generation-d", "generation-b", "generation-a"}));
  EXPECT_EQ(generated->parameters.get("generationMarker"), Json("a"));
  EXPECT_EQ(generated->animator_config.get("generationAnimation"), Json("a"));
  EXPECT_EQ(generated->drop_pool_config, Json("pool1"));

  auto numeric_seed = parameters.set("seed", seed);
  ASSERT_TRUE(numeric_seed) << numeric_seed.error().what();
  auto numeric = monsters.random_monster("generation-probe", *numeric_seed);
  ASSERT_TRUE(numeric) << numeric.error().what();
  EXPECT_EQ(numeric->seed, seed);
  EXPECT_EQ(numeric->animator_part_tags, generated->animator_part_tags);
  EXPECT_EQ(numeric->parameters.get("skills"), *generated_skills);

  auto selected_parts = parameters.set("selectedParts", JsonObject{{"body", "a"}, {"tail", "x"}});
  ASSERT_TRUE(selected_parts) << selected_parts.error().what();
  auto selected = monsters.random_monster("generation-probe", *selected_parts);
  ASSERT_TRUE(selected) << selected.error().what();
  EXPECT_EQ(selected->animator_part_tags.value("body"), "/body-a.png");
  EXPECT_EQ(selected->animator_part_tags.value("tail"), "/tail-x.png");
  EXPECT_EQ(selected->animator_part_tags.value("shared"), "/tail-x.png");
  EXPECT_EQ(selected->parameters.get("skills"), *generated_skills);
  EXPECT_EQ(selected->drop_pool_config, generated->drop_pool_config);

  // A binary lookup must reject a non-equal successor as well as the end of the pool.
  for (String const& name : {String("B"), String("z")}) {
    auto missing = monsters.monster_variant("generation-probe", seed,
        JsonObject{{"selectedParts", JsonObject{{"body", name}}}});
    ASSERT_FALSE(missing);
    auto message = std::string_view(missing.error().what());
    EXPECT_NE(message.find(Star::format("Key '{}' not found in Map::get()", name)), std::string_view::npos) << message;
  }

  auto split = monsters.monster_variant("generation-probe", seed,
      JsonObject{{"baseSkills", JsonArray{"base0", "base1"}},
                 {"specialSkills", JsonArray{"generation-a", "generation-b", "generation-c", "generation-d"}},
                 {"skillCount", 5}});
  ASSERT_TRUE(split) << split.error().what();
  EXPECT_EQ(split->parameters.get("skills"), Json(JsonArray{"base1", "base0", "generation-a", "generation-c", "generation-b"}));
  EXPECT_EQ(split->drop_pool_config, Json("pool0"));

  // A malformed frame is parsed only if its part survives selection. Even a
  // category with no parts must not inspect selectedParts.
  constexpr std::uint64_t malformed_seed = 0xffffffffffffffff;
  EXPECT_FALSE(monsters.monster_variant("generation-probe", malformed_seed));
  auto rescued = monsters.monster_variant("generation-probe", malformed_seed, *selected_parts);
  ASSERT_TRUE(rescued) << rescued.error().what();
  auto empty = monsters.monster_variant("generation-empty", seed, JsonObject{{"selectedParts", 7}});
  ASSERT_TRUE(empty) << empty.error().what();
  EXPECT_TRUE(empty->animator_part_tags.empty());

  auto stored = monsters.write_monster_variant_to_json(*selected);
  ASSERT_TRUE(stored) << stored.error().what();
  EXPECT_EQ(stored->get_uint("seed"), seed);
  auto from_disk = monsters.read_monster_variant_from_json(*stored);
  ASSERT_TRUE(from_disk) << from_disk.error().what();
  EXPECT_EQ(from_disk->seed, seed);
  EXPECT_EQ(from_disk->unique_parameters, *selected_parts);
  EXPECT_EQ(from_disk->animator_part_tags, selected->animator_part_tags);

  auto wire = monsters.write_monster_variant(*selected);
  ASSERT_TRUE(wire) << wire.error().what();
  auto from_wire = monsters.read_monster_variant(*wire);
  ASSERT_TRUE(from_wire) << from_wire.error().what();
  EXPECT_EQ(from_wire->seed, seed);
  EXPECT_EQ(from_wire->unique_parameters, *selected_parts);
  EXPECT_EQ(from_wire->animator_part_tags, selected->animator_part_tags);
  auto created = monsters.create_monster(std::move(*from_wire), 1.0f);
  ASSERT_TRUE(created) << created.error().what();
  EXPECT_EQ((*created)->net_store(), *wire);
});

const GameTestRegistration monster_part_pool_duplicate("RootTest", "MonsterPartPoolDuplicate", [](GameTest& fixture) {
  auto directory = File::temporary_directory(fixture.root().random());
  ASSERT_TRUE(directory) << directory.error().what();
  auto cleanup = make_scope_exit([&] { EXPECT_TRUE(File::remove_all(*directory)); });
  auto settings = isolation_settings(fixture.rootSettings(), *directory, "monster-part-pool-duplicate");
  ASSERT_TRUE(settings) << settings.error().what();
  auto overlay = File::relative_to(*directory, "assets");
  String part = R"({"name":"repeated","category":"part-pool-probe","type":"body","frames":{}})";
  ASSERT_TRUE(writeIsolationModules(overlay, {
    {"part-pool-first.monsterpart", part}, {"part-pool-second.monsterpart", part}
  }));
  auto root = Root::create(std::move(*settings));
  ASSERT_TRUE(root) << root.error().what();
  auto assets = (*root)->assets();
  ASSERT_TRUE(assets) << assets.error().what();
  auto parts = (*assets)->scan_extension("monsterpart");
  ASSERT_TRUE(parts) << parts.error().what();
  String duplicate_file;
  for (auto const& file : parts->get()) {
    if (file.string() == "/part-pool-first.monsterpart" || file.string() == "/part-pool-second.monsterpart")
      duplicate_file = file.string();
  }
  ASSERT_FALSE(duplicate_file.empty());
  auto database = (*root)->monster_database();
  ASSERT_FALSE(database);
  auto message = std::string_view(database.error().what());
  EXPECT_NE(message.find(Star::format("Error loading monster part '{}'", duplicate_file)), std::string_view::npos) << message;
  EXPECT_NE(message.find("Repeat monster part name 'repeated' for category 'part-pool-probe'"), std::string_view::npos) << message;
});

void expectIsolationModuleError(Json const& result, String const& field, String const& fragment) {
  auto message = result.get_string(field);
  ASSERT_TRUE(message) << message.error().what();
  EXPECT_TRUE(message->contains(fragment)) << message->utf8();
}

const GameTestRegistration
  luaModulesExecuteOncePerContext("RootTest", "LuaModulesExecuteOncePerContext", [](GameTest& fixture) {
    auto directory = File::temporary_directory(fixture.root().random());
    ASSERT_TRUE(directory) << directory.error().what();
    auto cleanup = make_scope_exit([&] { EXPECT_TRUE(File::remove_all(*directory)); });
    auto settings = isolation_settings(fixture.rootSettings(), *directory, "modules");
    ASSERT_TRUE(settings) << settings.error().what();
    auto overlay = File::relative_to(*directory, "assets");
    ASSERT_TRUE(writeIsolationModules(overlay,
                                      {{"p5-top.lua", R"LUA(
      topRuns = (topRuns or 0) + 1
      state = {value = 10}
      function advance() state.value = state.value + 1; return state.value end
    )LUA"},
                                       {"p5-dependency.lua", "dependencyRuns = (dependencyRuns or 0) + 1"},
                                       {"p5-entry.lua", R"LUA(
      entryRuns = (entryRuns or 0) + 1
      topResults = select("#", require("/p5-top.lua"))
      dependencyResults = select("#", require("/p5-dependency.lua"))
      require("/p5-dependency.lua")
    )LUA"}}));
    auto root = Root::create(std::move(*settings));
    ASSERT_TRUE(root) << root.error().what();
    (*root)->logger().add_sink(std::make_shared<RootIsolationErrorSink>());
    auto lua = isolationLua(**root);
    ASSERT_TRUE(lua) << lua.error().what();
    StringList scripts{"/p5-top.lua", "/p5-entry.lua", "/p5-top.lua", "/p5-dependency.lua", "/p5-entry.lua"};
    auto first = (*lua)->create_context(scripts);
    ASSERT_TRUE(first) << first.error().what();
    auto second = (*lua)->create_context(scripts);
    ASSERT_TRUE(second) << second.error().what();
    for (auto* context : {&*first, &*second}) {
      auto result = context->eval<Json>(R"LUA(
      _SBLOADED = {}
      return {
        top = topRuns, entry = entryRuns, dependency = dependencyRuns,
        topResults = topResults, dependencyResults = dependencyResults,
        repeatedResults = select("#", require("/p5-top.lua")), value = state.value
      }
    )LUA");
      ASSERT_TRUE(result) << result.error().what();
      EXPECT_EQ(*result,
                Json(JsonObject{{"top", 1},
                                {"entry", 1},
                                {"dependency", 1},
                                {"topResults", 0},
                                {"dependencyResults", 0},
                                {"repeatedResults", 0},
                                {"value", 10}}));
      EXPECT_EQ(context->eval<int>("topRuns"), 1);
    }
    EXPECT_EQ(first->invoke_path<int>("advance"), 11);
    EXPECT_EQ(first->invoke_path<int>("advance"), 12);
    EXPECT_EQ(second->eval<int>("state.value"), 10);
    EXPECT_EQ(second->invoke_path<int>("advance"), 11);
    EXPECT_EQ(first->eval<int>("state.value"), 12);
  });

const GameTestRegistration
  luaModulesRetryFailuresAndCycles("RootTest", "LuaModulesRetryFailuresAndCycles", [](GameTest& fixture) {
    auto directory = File::temporary_directory(fixture.root().random());
    ASSERT_TRUE(directory) << directory.error().what();
    auto cleanup = make_scope_exit([&] { EXPECT_TRUE(File::remove_all(*directory)); });
    auto settings = isolation_settings(fixture.rootSettings(), *directory, "module-retries");
    ASSERT_TRUE(settings) << settings.error().what();
    auto overlay = File::relative_to(*directory, "assets");
    ASSERT_TRUE(writeIsolationModules(overlay,
                                      {{"p5-dependency.lua", "dependencyRuns = (dependencyRuns or 0) + 1"},
                                       {"p5-failure.lua", R"LUA(
      failureRuns = (failureRuns or 0) + 1
      require("/p5-dependency.lua")
      if not allowSuccess then error("controlled module failure") end
      failureReady = true
    )LUA"},
                                       {"p5-self.lua", R"LUA(
      selfRuns = (selfRuns or 0) + 1
      if not allowSelf then require("/p5-self.lua") end
      selfReady = true
    )LUA"},
                                       {"p5-cycle-a.lua", R"LUA(
      cycleARuns = (cycleARuns or 0) + 1
      if not allowCycle then require("/p5-cycle-b.lua") end
      cycleAReady = true
    )LUA"},
                                       {"p5-cycle-b.lua", R"LUA(
      cycleBRuns = (cycleBRuns or 0) + 1
      require("/p5-cycle-a.lua")
      cycleBReady = true
    )LUA"}}));
    auto root = Root::create(std::move(*settings));
    ASSERT_TRUE(root) << root.error().what();
    (*root)->logger().add_sink(std::make_shared<RootIsolationErrorSink>());
    auto lua = isolationLua(**root);
    ASSERT_TRUE(lua) << lua.error().what();
    auto context = (*lua)->create_context(StringList{});
    ASSERT_TRUE(context) << context.error().what();
    auto failures = context->eval<Json>(R"LUA(
    local firstOk, firstError = pcall(require, "/p5-failure.lua")
    local secondOk, secondError = pcall(require, "/p5-failure.lua")
    return {
      firstOk = firstOk, secondOk = secondOk,
      firstError = tostring(firstError), secondError = tostring(secondError),
      attempts = failureRuns, dependencies = dependencyRuns, ready = failureReady == true
    }
  )LUA");
    ASSERT_TRUE(failures) << failures.error().what();
    EXPECT_EQ(failures->get_bool("firstOk"), false);
    EXPECT_EQ(failures->get_bool("secondOk"), false);
    expectIsolationModuleError(*failures, "firstError", "controlled module failure");
    expectIsolationModuleError(*failures, "secondError", "controlled module failure");
    EXPECT_EQ(failures->get_int("attempts"), 2);
    EXPECT_EQ(failures->get_int("dependencies"), 1);
    EXPECT_EQ(failures->get_bool("ready"), false);
    auto retry = context->eval<Json>(R"LUA(
    allowSuccess = true
    local results = select("#", require("/p5-failure.lua"))
    require("/p5-failure.lua")
    return {results = results, attempts = failureRuns, dependencies = dependencyRuns, ready = failureReady}
  )LUA");
    ASSERT_TRUE(retry) << retry.error().what();
    EXPECT_EQ(*retry, Json(JsonObject{{"results", 0}, {"attempts", 3}, {"dependencies", 1}, {"ready", true}}));
    auto cycles = context->eval<Json>(R"LUA(
    local selfOk, selfError = pcall(require, "/p5-self.lua")
    local cycleOk, cycleError = pcall(require, "/p5-cycle-a.lua")
    return {
      selfOk = selfOk, selfError = tostring(selfError),
      cycleOk = cycleOk, cycleError = tostring(cycleError),
      selfAttempts = selfRuns, aAttempts = cycleARuns, bAttempts = cycleBRuns,
      selfReady = selfReady == true, aReady = cycleAReady == true, bReady = cycleBReady == true
    }
  )LUA");
    ASSERT_TRUE(cycles) << cycles.error().what();
    EXPECT_EQ(cycles->get_bool("selfOk"), false);
    EXPECT_EQ(cycles->get_bool("cycleOk"), false);
    expectIsolationModuleError(*cycles, "selfError", "/p5-self.lua -> /p5-self.lua");
    expectIsolationModuleError(*cycles, "cycleError", "/p5-cycle-a.lua -> /p5-cycle-b.lua -> /p5-cycle-a.lua");
    EXPECT_EQ(cycles->get_int("selfAttempts"), 1);
    EXPECT_EQ(cycles->get_int("aAttempts"), 1);
    EXPECT_EQ(cycles->get_int("bAttempts"), 1);
    EXPECT_EQ(cycles->get_bool("selfReady"), false);
    EXPECT_EQ(cycles->get_bool("aReady"), false);
    EXPECT_EQ(cycles->get_bool("bReady"), false);
    auto recovered = context->eval<Json>(R"LUA(
    allowSelf = true
    allowCycle = true
    local selfResults = select("#", require("/p5-self.lua"))
    local aResults = select("#", require("/p5-cycle-a.lua"))
    local bResults = select("#", require("/p5-cycle-b.lua"))
    require("/p5-self.lua")
    require("/p5-cycle-a.lua")
    require("/p5-cycle-b.lua")
    return {
      selfResults = selfResults, aResults = aResults, bResults = bResults,
      selfAttempts = selfRuns, aAttempts = cycleARuns, bAttempts = cycleBRuns,
      selfReady = selfReady, aReady = cycleAReady, bReady = cycleBReady
    }
  )LUA");
    ASSERT_TRUE(recovered) << recovered.error().what();
    EXPECT_EQ(*recovered,
              Json(JsonObject{{"selfResults", 0},
                              {"aResults", 0},
                              {"bResults", 0},
                              {"selfAttempts", 2},
                              {"aAttempts", 2},
                              {"bAttempts", 2},
                              {"selfReady", true},
                              {"aReady", true},
                              {"bReady", true}}));

    // Fresh contexts must still fail, without publishing the same native-origin
    // require error and Debug stack for every attempt.
    class CycleLogSink : public LogSink {
    public:
      std::expected<void, StarException> log(std::string_view, LogLevel level) override {
        if (level == LogLevel::Error)
          ++errors;
        else if (level == LogLevel::Debug)
          ++details;
        return {};
      }
      unsigned errors = 0;
      unsigned details = 0;
    };
    Logger logger;
    logger.remove_stdout_sink();
    auto sink = std::make_shared<CycleLogSink>();
    sink->set_level(LogLevel::Debug);
    logger.add_sink(sink);
    for (unsigned attempt = 0; attempt < 3; ++attempt) {
      auto failure = (*lua)->create_context("/p5-cycle-a.lua");
      ASSERT_FALSE(failure);
      EXPECT_TRUE(failure.error().is<LuaException>());
      EXPECT_EQ(failure.error().diagnostic_summary()->kind, ExceptionSummaryKind::ScriptRuntime);
      ASSERT_TRUE(logger.error_exception("Require cycle", ExceptionDiagnostic(failure.error())));
    }
    EXPECT_EQ(sink->errors, 1u);
    EXPECT_EQ(sink->details, 1u);
  });

const GameTestRegistration
  luaModulesRejectStaleContexts("RootTest", "LuaModulesRejectStaleContexts", [](GameTest& fixture) {
    auto directory = File::temporary_directory(fixture.root().random());
    ASSERT_TRUE(directory) << directory.error().what();
    auto cleanup = make_scope_exit([&] { EXPECT_TRUE(File::remove_all(*directory)); });
    auto settings = isolation_settings(fixture.rootSettings(), *directory, "module-revisions");
    ASSERT_TRUE(settings) << settings.error().what();
    auto overlay = File::relative_to(*directory, "assets");
    ASSERT_TRUE(writeIsolationModules(
      overlay,
      {{"p5-version.lua", "versionRuns = (versionRuns or 0) + 1; function version() return 1 end"},
       {"p5-after-reload.lua", "function afterReload() return 1 end"},
       {"p5-after-restart.lua", "function afterRestart() return 1 end"}}));
    auto root = Root::create(std::move(*settings));
    ASSERT_TRUE(root) << root.error().what();
    (*root)->logger().add_sink(std::make_shared<RootIsolationErrorSink>());
    auto lua = isolationLua(**root);
    ASSERT_TRUE(lua) << lua.error().what();
    auto original = (*lua)->create_context("/p5-version.lua");
    ASSERT_TRUE(original) << original.error().what();
    EXPECT_EQ(original->invoke_path<int>("version"), 1);
    ASSERT_TRUE(writeIsolationModules(
      overlay,
      {{"p5-version.lua", "versionRuns = (versionRuns or 0) + 1; function version() return 2 end"},
       {"p5-after-reload.lua", "function afterReload() return 2 end"}}));
    ASSERT_TRUE((*root)->reload());
    auto staleReload = original->eval<Json>(R"LUA(
    local ok, message = pcall(require, "/p5-after-reload.lua")
    return {
      ok = ok, message = tostring(message), version = version(),
      loadedResults = select("#", require("/p5-version.lua")), runs = versionRuns,
      unseenAbsent = afterReload == nil
    }
  )LUA");
    ASSERT_TRUE(staleReload) << staleReload.error().what();
    EXPECT_EQ(staleReload->get_bool("ok"), false);
    expectIsolationModuleError(*staleReload, "message", "/p5-after-reload.lua");
    EXPECT_EQ(staleReload->get_int("version"), 1);
    EXPECT_EQ(staleReload->get_int("loadedResults"), 0);
    EXPECT_EQ(staleReload->get_int("runs"), 1);
    EXPECT_EQ(staleReload->get_bool("unseenAbsent"), true);
    auto reloaded = (*lua)->create_context(StringList{"/p5-version.lua", "/p5-after-reload.lua"});
    ASSERT_TRUE(reloaded) << reloaded.error().what();
    EXPECT_EQ(reloaded->invoke_path<int>("version"), 2);
    EXPECT_EQ(reloaded->invoke_path<int>("afterReload"), 2);
    auto beforeRestart = (*lua)->create_context("/p5-version.lua");
    ASSERT_TRUE(beforeRestart) << beforeRestart.error().what();
    // This asset has never been fetched: restart alone is the invalidation under
    // test, without a hotReload that could independently make this context stale.
    ASSERT_TRUE(writeIsolationModules(overlay, {{"p5-after-restart.lua", "function afterRestart() return 3 end"}}));
    ASSERT_TRUE((*lua)->restart());
    auto staleRestart = beforeRestart->eval<Json>(R"LUA(
    local ok, message = pcall(require, "/p5-after-restart.lua")
    return {
      ok = ok, message = tostring(message), version = version(),
      loadedResults = select("#", require("/p5-version.lua")), runs = versionRuns,
      unseenAbsent = afterRestart == nil
    }
  )LUA");
    ASSERT_TRUE(staleRestart) << staleRestart.error().what();
    EXPECT_EQ(staleRestart->get_bool("ok"), false);
    expectIsolationModuleError(*staleRestart, "message", "/p5-after-restart.lua");
    EXPECT_EQ(staleRestart->get_int("version"), 2);
    EXPECT_EQ(staleRestart->get_int("loadedResults"), 0);
    EXPECT_EQ(staleRestart->get_int("runs"), 1);
    EXPECT_EQ(staleRestart->get_bool("unseenAbsent"), true);
    EXPECT_EQ(original->invoke_path<int>("version"), 1);
    EXPECT_EQ(reloaded->invoke_path<int>("afterReload"), 2);
    auto restarted = (*lua)->create_context(StringList{"/p5-version.lua", "/p5-after-restart.lua"});
    ASSERT_TRUE(restarted) << restarted.error().what();
    EXPECT_EQ(restarted->invoke_path<int>("version"), 2);
    EXPECT_EQ(restarted->invoke_path<int>("afterRestart"), 3);
  });

const GameTestRegistration
  luaModulesRejectReentrantInvalidation("RootTest", "LuaModulesRejectReentrantInvalidation", [](GameTest& fixture) {
    auto directory = File::temporary_directory(fixture.root().random());
    ASSERT_TRUE(directory) << directory.error().what();
    auto cleanup = make_scope_exit([&] { EXPECT_TRUE(File::remove_all(*directory)); });
    auto settings = isolation_settings(fixture.rootSettings(), *directory, "module-reentrancy");
    ASSERT_TRUE(settings) << settings.error().what();
    auto overlay = File::relative_to(*directory, "assets");
    ASSERT_TRUE(writeIsolationModules(overlay, {{"p5-reentrant.lua", R"LUA(
      moduleRuns = (moduleRuns or 0) + 1
      if invalidateDuringLoad then test.invalidate() end
      function retainedValue() return moduleRuns end
    )LUA"}}));
    auto root = Root::create(std::move(*settings));
    ASSERT_TRUE(root) << root.error().what();
    (*root)->logger().add_sink(std::make_shared<RootIsolationErrorSink>());
    auto lua = isolationLua(**root);
    ASSERT_TRUE(lua) << lua.error().what();
    for (bool restart : {false, true}) {
      SCOPED_TRACE(restart ? "native restart" : "native hot reload");
      int invalidations = 0;
      LuaCallbacks callbacks;
      ASSERT_TRUE(callbacks.register_callback("invalidate", [&]() -> std::expected<void, StarException> {
        ++invalidations;
        return restart ? (*lua)->restart() : (*root)->hotReload();
      }));
      auto context = (*lua)->create_context(StringList{});
      ASSERT_TRUE(context) << context.error().what();
      ASSERT_TRUE(context->set_callbacks("test", callbacks));
      auto interrupted = context->eval<Json>(R"LUA(
      invalidateDuringLoad = true
      local ok, message = pcall(require, "/p5-reentrant.lua")
      return {ok = ok, message = tostring(message), runs = moduleRuns, retained = retainedValue()}
    )LUA");
      ASSERT_TRUE(interrupted) << interrupted.error().what();
      EXPECT_EQ(interrupted->get_bool("ok"), false);
      expectIsolationModuleError(*interrupted, "message", "/p5-reentrant.lua");
      EXPECT_EQ(interrupted->get_int("runs"), 1);
      EXPECT_EQ(interrupted->get_int("retained"), 1);
      EXPECT_EQ(invalidations, 1);
      auto staleRetry = context->eval<Json>(R"LUA(
      invalidateDuringLoad = false
      local ok, message = pcall(require, "/p5-reentrant.lua")
      return {ok = ok, message = tostring(message), runs = moduleRuns}
    )LUA");
      ASSERT_TRUE(staleRetry) << staleRetry.error().what();
      EXPECT_EQ(staleRetry->get_bool("ok"), false);
      expectIsolationModuleError(*staleRetry, "message", "/p5-reentrant.lua");
      EXPECT_EQ(staleRetry->get_int("runs"), 1);
      EXPECT_EQ(invalidations, 1);
      auto fresh = (*lua)->create_context("/p5-reentrant.lua");
      ASSERT_TRUE(fresh) << fresh.error().what();
      EXPECT_EQ(fresh->invoke_path<int>("retainedValue"), 1);
      EXPECT_EQ(fresh->eval<int>("select('#', require('/p5-reentrant.lua'))"), 0);
      EXPECT_EQ(fresh->eval<int>("moduleRuns"), 1);
      EXPECT_EQ(context->invoke_path<int>("retainedValue"), 1);
    }
  });

const GameTestRegistration simultaneousRoots("RootTest", "SimultaneousRoots", [](GameTest& fixture) {
  auto directoryA = File::temporary_directory(fixture.root().random());
  ASSERT_TRUE(directoryA) << directoryA.error().what();
  auto cleanupA = make_scope_exit([&] { EXPECT_TRUE(File::remove_all(*directoryA)); });
  auto directoryB = File::temporary_directory(fixture.root().random());
  ASSERT_TRUE(directoryB) << directoryB.error().what();
  auto cleanupB = make_scope_exit([&] { EXPECT_TRUE(File::remove_all(*directoryB)); });
  auto settingsResultA = isolation_settings(fixture.rootSettings(), *directoryA, "A");
  ASSERT_TRUE(settingsResultA) << settingsResultA.error().what();
  auto settingsA = std::move(*settingsResultA);
  auto settingsResultB = isolation_settings(fixture.rootSettings(), *directoryB, "B");
  ASSERT_TRUE(settingsResultB) << settingsResultB.error().what();
  auto settingsB = std::move(*settingsResultB);
  auto overlayA = File::relative_to(*directoryA, "assets");
  auto overlayB = File::relative_to(*directoryB, "assets");
  auto createdA = Root::create(settingsA);
  ASSERT_TRUE(createdA) << createdA.error().what();
  auto rootA = std::move(*createdA);
  rootA->logger().add_sink(std::make_shared<RootIsolationErrorSink>());
  auto createdB = Root::create(settingsB);
  ASSERT_TRUE(createdB) << createdB.error().what();
  auto rootB = std::move(*createdB);
  rootB->logger().add_sink(std::make_shared<RootIsolationErrorSink>());
  auto logPathA = File::relative_to(*directoryA, "isolation.log");
  auto logPathB = File::relative_to(*directoryB, "isolation.log");
  auto sinkA = FileLogSink::create(logPathA, LogLevel::Info, true);
  ASSERT_TRUE(sinkA) << sinkA.error().what();
  auto sinkB = FileLogSink::create(logPathB, LogLevel::Info, true);
  ASSERT_TRUE(sinkB) << sinkB.error().what();
  rootA->logger().add_sink(*sinkA);
  rootB->logger().add_sink(*sinkB);

  EXPECT_NE(rootA->toStoragePath("root-isolation.config"), rootB->toStoragePath("root-isolation.config"));
  {
    auto createdLuaA = isolationLua(*rootA, true);
    ASSERT_TRUE(createdLuaA) << createdLuaA.error().what();
    auto luaA = std::move(*createdLuaA);
    auto createdLuaB = isolationLua(*rootB, true);
    ASSERT_TRUE(createdLuaB) << createdLuaB.error().what();
    auto luaB = std::move(*createdLuaB);
    auto createdContextA = luaA->create_context("/root-isolation.lua");
    ASSERT_TRUE(createdContextA) << createdContextA.error().what();
    auto contextA = std::make_unique<LuaContext>(std::move(*createdContextA));
    auto contextB = luaB->create_context("/root-isolation.lua");
    ASSERT_TRUE(contextB) << contextB.error().what();
    ASSERT_TRUE(expectIsolationConsumers(*rootA, *contextA, "A", "A", "A"));
    ASSERT_TRUE(expectIsolationConsumers(*rootB, *contextB, "B", "B", "B"));

    // Exercise writes through the real Lua root table, not only C++ getters.
    ASSERT_TRUE(contextA->eval("root.setConfiguration('rootIsolationOwner', 'A-written')"));
    ASSERT_TRUE(contextB->eval("root.setConfiguration('rootIsolationOwner', 'B-written')"));
    ASSERT_TRUE(expectIsolationConsumers(*rootA, *contextA, "A", "A", "A-written"));
    ASSERT_TRUE(expectIsolationConsumers(*rootB, *contextB, "B", "B", "B-written"));
    ASSERT_TRUE(contextA->eval("sb.logInfo('root-isolation-log-A'); sb.setLogMap('root-isolation', 'A')"));
    ASSERT_TRUE(contextB->eval("sb.logInfo('root-isolation-log-B'); sb.setLogMap('root-isolation', 'B')"));
    EXPECT_EQ(rootA->logMap().get_value("root-isolation"), "A");
    EXPECT_EQ(rootB->logMap().get_value("root-isolation"), "B");
    EXPECT_EQ(rootA->logMap().drain().value("root-isolation"), "A");
    EXPECT_TRUE(rootA->logMap().snapshot().empty());
    EXPECT_EQ(rootB->logMap().get_value("root-isolation"), "B");
    EXPECT_EQ(isolationLogContains(logPathA, "root-isolation-log-A"), true);
    EXPECT_EQ(isolationLogContains(logPathA, "root-isolation-log-B"), false);
    EXPECT_EQ(isolationLogContains(logPathB, "root-isolation-log-B"), true);
    EXPECT_EQ(isolationLogContains(logPathB, "root-isolation-log-A"), false);

    // A real filesystem failure must leave both live owners usable and must
    // not prevent a subsequent successful Root construction.
    auto failedSettings = settingsA;
    failedSettings.storageDirectory = File::relative_to(*directoryA, "not-a-directory");
    ASSERT_TRUE(File::write_file(String("regular file"), failedSettings.storageDirectory));
    auto failedRoot = Root::create(failedSettings);
    EXPECT_FALSE(failedRoot);
    auto recoveredSettings = settingsA;
    recoveredSettings.storageDirectory = File::relative_to(*directoryA, "recovered-storage");
    {
      auto recovered = Root::create(recoveredSettings);
      ASSERT_TRUE(recovered) << recovered.error().what();
      (*recovered)->logger().add_sink(std::make_shared<RootIsolationErrorSink>());
      auto recoveredLua = isolationLua(**recovered);
      ASSERT_TRUE(recoveredLua) << recoveredLua.error().what();
      auto recoveredContext = (*recoveredLua)->create_context("/root-isolation.lua");
      ASSERT_TRUE(recoveredContext) << recoveredContext.error().what();
      ASSERT_TRUE(expectIsolationConsumers(**recovered, *recoveredContext, "A", "A", "A"));
    }
    ASSERT_TRUE(expectIsolationConsumers(*rootA, *contextA, "A", "A", "A-written"));
    ASSERT_TRUE(expectIsolationConsumers(*rootB, *contextB, "B", "B", "B-written"));

    auto listener_a = std::make_shared<TrackerListener>();
    auto listener_b = std::make_shared<TrackerListener>();
    auto subscription_a = rootA->subscribe_reload(listener_a);
    auto subscription_b = rootB->subscribe_reload(listener_b);
    {
      auto assetsA = rootA->assets();
      ASSERT_TRUE(assetsA) << assetsA.error().what();
      auto assetsB = rootB->assets();
      ASSERT_TRUE(assetsB) << assetsB.error().what();
      auto databaseA = rootA->techDatabase();
      ASSERT_TRUE(databaseA) << databaseA.error().what();
      auto databaseB = rootB->techDatabase();
      ASSERT_TRUE(databaseB) << databaseB.error().what();
      ASSERT_TRUE(writeIsolationAssets(overlayA, "A-reloaded"));
      ASSERT_TRUE(writeIsolationAssets(overlayB, "B-reloaded"));

      ASSERT_TRUE(rootA->reload());
      EXPECT_TRUE(listener_a->consume());
      EXPECT_FALSE(listener_b->consume());
      EXPECT_FALSE(luaA->script_loaded("/root-isolation.lua"));
      EXPECT_TRUE(luaB->script_loaded("/root-isolation.lua"));
      auto reloadedAssetsA = rootA->assets();
      ASSERT_TRUE(reloadedAssetsA) << reloadedAssetsA.error().what();
      auto reloadedDatabaseA = rootA->techDatabase();
      ASSERT_TRUE(reloadedDatabaseA) << reloadedDatabaseA.error().what();
      auto retainedAssetsB = rootB->assets();
      ASSERT_TRUE(retainedAssetsB) << retainedAssetsB.error().what();
      auto retainedDatabaseB = rootB->techDatabase();
      ASSERT_TRUE(retainedDatabaseB) << retainedDatabaseB.error().what();
      EXPECT_NE(reloadedAssetsA->get(), assetsA->get());
      EXPECT_NE(reloadedDatabaseA->get(), databaseA->get());
      EXPECT_EQ(retainedAssetsB->get(), assetsB->get());
      EXPECT_EQ(retainedDatabaseB->get(), databaseB->get());
      auto retainedTechA = (*databaseA)->tech("root-isolation");
      ASSERT_TRUE(retainedTechA) << retainedTechA.error().what();
      EXPECT_EQ(retainedTechA->description, "A");
      // Existing closures follow their own owner's reloaded services, while
      // already instantiated Lua functions keep their original script body.
      ASSERT_TRUE(expectIsolationConsumers(*rootA, *contextA, "A-reloaded", "A", "A-written"));
      auto reloadedContextA = luaA->create_context("/root-isolation.lua");
      ASSERT_TRUE(reloadedContextA) << reloadedContextA.error().what();
      ASSERT_TRUE(expectIsolationConsumers(*rootA, *reloadedContextA, "A-reloaded", "A-reloaded", "A-written"));
      ASSERT_TRUE(expectIsolationConsumers(*rootB, *contextB, "B", "B", "B-written"));
      auto cachedContextB = luaB->create_context("/root-isolation.lua");
      ASSERT_TRUE(cachedContextB) << cachedContextB.error().what();
      ASSERT_TRUE(expectIsolationConsumers(*rootB, *cachedContextB, "B", "B", "B-written"));
    }

    // Destroy all A script borrowers before A. B remains live throughout.
    contextA.reset();
    luaA.reset();
    subscription_a.disconnect();
    rootA.reset();
    EXPECT_FALSE(listener_b->consume());
    ASSERT_TRUE(expectIsolationConsumers(*rootB, *contextB, "B", "B", "B-written"));
    ASSERT_TRUE(contextB->eval("root.setConfiguration('rootIsolationOwner', 'B-survives')"));
    ASSERT_TRUE(expectIsolationConsumers(*rootB, *contextB, "B", "B", "B-survives"));
    EXPECT_EQ(rootB->logMap().get_value("root-isolation"), "B");
    ASSERT_TRUE(contextB->eval("sb.logInfo('root-isolation-log-B-survives'); sb.setLogMap('root-isolation', 'B-survives')"));
    EXPECT_EQ(rootB->logMap().get_value("root-isolation"), "B-survives");
    EXPECT_EQ(isolationLogContains(logPathB, "root-isolation-log-B-survives"), true);
    EXPECT_EQ(isolationLogContains(logPathA, "root-isolation-log-B-survives"), false);

    ASSERT_TRUE(rootB->reload());
    EXPECT_TRUE(listener_b->consume());
    EXPECT_FALSE(luaB->script_loaded("/root-isolation.lua"));
    ASSERT_TRUE(expectIsolationConsumers(*rootB, *contextB, "B-reloaded", "B", "B-survives"));
    auto reloadedContextB = luaB->create_context("/root-isolation.lua");
    ASSERT_TRUE(reloadedContextB) << reloadedContextB.error().what();
    ASSERT_TRUE(expectIsolationConsumers(*rootB, *reloadedContextB, "B-reloaded", "B-reloaded", "B-survives"));
  }
  rootB.reset();
  EXPECT_EQ(isolationStoredOwner(settingsA.storageDirectory), "A-written");
  EXPECT_EQ(isolationStoredOwner(settingsB.storageDirectory), "B-survives");
});

[[nodiscard]] std::expected<void, StarException> writeIsolationMaterial(
    String const& overlay, MaterialId material, String const& collisionKind) {
  // These are the required MaterialDatabase parser fields. Omit the optional
  // render profile: the consumer is the real headless world's collision path.
  auto content = Json(JsonObject{
      {"materialId", material},
      {"materialName", "root-isolation"},
      {"category", "materials"},
      {"collisionKind", collisionKind}
    }).repr();
  if (!content) return std::unexpected(std::move(content.error()));
  return File::write_file(*content, File::relative_to(overlay, "root-isolation.material"));
}

[[nodiscard]] std::expected<void, StarException> placeIsolationMaterial(
    WorldServer& world, Vec2I const& position, MaterialId material, bool collides) {
  auto generated = world.generateRegion(RectI::with_size(position - Vec2I(4, 4), Vec2I(9, 9)));
  if (!generated) return std::unexpected(std::move(generated.error()));
  world.setTileProtectionEnabled(false);
  auto tile = world.modifyServerTile(position, true);
  if (!tile) return std::unexpected(std::move(tile.error()));
  if (!*tile) return std::unexpected(StarException("Isolation material tile is unavailable"));
  EXPECT_EQ((*tile)->get().foreground, empty_material_id);
  if ((*tile)->get().foreground != empty_material_id)
    return std::unexpected(StarException("Isolation material tile is not empty"));
  auto center = Vec2F(position) + Vec2F(0.5f, 0.5f);
  EXPECT_EQ(world.pointTileCollision(center), false);
  // Supply a legitimate attachment surface, then let WorldServer perform the
  // placement and derive its collision from this world's MaterialDatabase.
  (*tile)->get().background = structure_material_id;
  auto placed = world.forceModifyTile(
      position, PlaceMaterial{TileLayer::Foreground, material, MaterialHue(0)}, true);
  if (!placed) return std::unexpected(std::move(placed.error()));
  if (!*placed) return std::unexpected(StarException("Isolation material placement was rejected"));
  EXPECT_EQ(world.material(position, TileLayer::Foreground), material);
  EXPECT_EQ(world.pointTileCollision(center), collides);
  EXPECT_EQ(world.lineTileCollision(center - Vec2F(0.25f, 0), center + Vec2F(0.25f, 0)), collides);
  return {};
}

const GameTestRegistration monster_damage_sources("RootTest", "MonsterDamageSources", [](GameTest& fixture) {
  auto& root = fixture.root();
  auto storage = File::ephemeral_file(root.random());
  ASSERT_TRUE(storage) << storage.error().what();
  auto world = WorldServer::create(root, Vec2U(2048, 2048), *storage);
  ASSERT_TRUE(world) << world.error().what();
  auto close = make_scope_exit([&] { EXPECT_TRUE((*world)->close()); });
  Vec2I origin(1024, 1024);
  ASSERT_TRUE((*world)->generateRegion(RectI::with_size(origin - Vec2I(24, 5), Vec2I(48, 10))));

  auto database = root.monster_database();
  ASSERT_TRUE(database) << database.error().what();
  auto variant = (*database)->monster_variant("poptop", 1234, JsonObject{{"scripts", JsonArray{}}});
  ASSERT_TRUE(variant) << variant.error().what();
  auto animation = Json::parse_json(R"JSON({
    "transformationGroups":{"probe":{}},
    "animatedParts":{"parts":{"body":{"properties":{"transformationGroups":["probe"]}}}}
  })JSON");
  ASSERT_TRUE(animation) << animation.error().what();
  variant->animator_config = std::move(*animation);
  variant->animator_part_tags.clear();
  variant->scale = 1;
  auto parts = Json::parse_json(R"JSON({
    "omitted":{"anchorPart":"body","damageSource":{"damage":4,"line":[[0,0],[18,0]],"damageSourceKind":"omitted"}},
    "unchecked":{"anchorPart":"body","checkLineCollision":false,"bounces":"ignored","damageSource":{"damage":4,"line":[[0,0],[18,0]],"damageSourceKind":"unchecked"}},
    "ray":{"anchorPart":"body","checkLineCollision":true,"bounces":2,"damageSource":{"damage":4,"line":[[0,0],[18,0]],"knockback":[2,1],"sourceEntityId":61,"teamType":"enemy","teamNumber":7,"damageSourceKind":"ray"}},
    "poly":{"anchorPart":"body","checkLineCollision":"ignored","damageSource":{"damage":3,"poly":[[0,0],[1,0],[0,1]],"damageSourceKind":"poly"}},
    "shapeless":{"anchorPart":"not-a-part","damageSource":{"damage":5,"damageArea":null,"damageSourceKind":"shapeless"}},
    "brokenSource":{"anchorPart":"body","damageSource":{"damage":"invalid","damageArea":null}},
    "brokenTransform":{"anchorPart":"not-a-part","checkLineCollision":false,"damageSource":{"damage":4,"line":[[0,0],[18,0]]}},
    "brokenFlag":{"anchorPart":"body","checkLineCollision":"invalid","damageSource":{"damage":4,"line":[[0,0],[18,0]]}},
    "brokenBounces":{"anchorPart":"body","checkLineCollision":true,"bounces":"invalid","damageSource":{"damage":4,"line":[[0,0],[18,0]]}}
  })JSON");
  ASSERT_TRUE(parts) << parts.error().what();
  auto part_map = parts->to_object();
  ASSERT_TRUE(part_map) << part_map.error().what();
  variant->animation_damage_parts = std::move(*part_map);
  StringList part_order;
  for (auto const& [name, config] : variant->animation_damage_parts) {
    if (name == "ray" || name == "poly" || name == "shapeless")
      part_order.push_back(name);
  }
  variant->touch_damage_config = JsonObject{{"damage", 7}, {"poly", JsonArray{JsonArray{0, 0}, JsonArray{1, 0}, JsonArray{0, 1}}},
    {"sourceEntityId", 62}, {"teamType", "enemy"}, {"teamNumber", 8}, {"damageSourceKind", "touch"}};
  variant->touch_damage_multiplier = 2;
  variant->damage_team_type = TeamType::Enemy;
  variant->damage_team = 4;

  auto functions = root.functionDatabase();
  ASSERT_TRUE(functions) << functions.error().what();
  auto power = (*functions)->function(variant->power_level_function);
  ASSERT_TRUE(power) << power.error().what();
  constexpr float level = 2.5f;
  auto level_power = (*power)->evaluate(level);
  ASSERT_TRUE(level_power) << level_power.error().what();
  {
    auto uninitialized = Monster::create(root, MonsterVariant(*variant));
    ASSERT_TRUE(uninitialized) << uninitialized.error().what();
    auto idle_error = (*uninitialized)->damage_sources();
    ASSERT_FALSE(idle_error);
    EXPECT_NE(std::string_view(idle_error.error().what()).find("Monster level is not initialized"), std::string_view::npos);
  }
  auto monster = Monster::create(root, MonsterVariant(*variant), level);
  ASSERT_TRUE(monster) << monster.error().what();
  (*monster)->movement_controller().set_position(Vec2F(origin) + Vec2F(0.5f, 0.5f));
  ASSERT_TRUE((*world)->addEntity(*monster, 17));

  auto script = [&](String const& code) {
    auto result = (*monster)->eval_script(code + "\nreturn true");
    ASSERT_TRUE(result) << result.error().what();
    ASSERT_TRUE(*result);
  };
  auto query = [&] { return (*monster)->damage_sources(); };
  auto sources = query();
  ASSERT_TRUE(sources) << sources.error().what();
  EXPECT_TRUE(sources->empty());
  ASSERT_NO_FATAL_FAILURE(script(R"LUA(
    monster.setDamageParts({"omitted", "unchecked"})
  )LUA"));
  sources = query();
  ASSERT_TRUE(sources) << sources.error().what();
  EXPECT_TRUE(sources->empty());

  // Inactive malformed templates must not poison another source or creation.
  // A false collision flag still requires resolving the line's live anchor.
  for (auto name : {"brokenSource", "brokenTransform", "brokenFlag", "brokenBounces"}) {
    SCOPED_TRACE(name);
    ASSERT_NO_FATAL_FAILURE(script(String(Star::format("monster.setDamageParts({{\"{}\"}})", name))));
    auto failed = query();
    auto retried = query();
    ASSERT_FALSE(failed);
    ASSERT_FALSE(retried);
    EXPECT_EQ(std::string_view(failed.error().what()), std::string_view(retried.error().what()));
    if (std::string_view(name) == "brokenTransform")
      EXPECT_NE(std::string_view(failed.error().what()).find("not-a-part"), std::string_view::npos);
  }
  ASSERT_NO_FATAL_FAILURE(script(R"LUA(
    monster.setDamageOnTouch(true)
    monster.setDamageParts({"omitted", "unchecked", "ray", "poly", "shapeless"})
    monster.setDamageSources({{damage=9, line={{0,0},{1,0}}, damageSourceKind="script", sourceEntityId=60, teamType="enemy", teamNumber=6}})
  )LUA"));

  struct Segment {
    Line2F line;
    Vec2F knockback;
  };
  auto check_sources = [&](List<DamageSource> const& damage, std::span<Segment const> segments,
                           float status_power, bool changed) {
    ASSERT_EQ(damage.size(), 4 + segments.size());
    auto const& script_source = damage[0];
    EXPECT_EQ(script_source.damage_source_kind, std::string_view(changed ? "scriptChanged" : "script"));
    EXPECT_EQ(script_source.damage, changed ? 11 : 9);
    EXPECT_EQ(script_source.source_entity_id, changed ? 63 : 60);
    EXPECT_EQ(script_source.team, changed ? EntityDamageTeam(TeamType::Passive) : EntityDamageTeam(TeamType::Enemy, 6));

    auto const& touch = damage[1];
    EXPECT_EQ(touch.damage_source_kind, "touch");
    EXPECT_FLOAT_EQ(touch.damage, 7.0f * (2.0f * static_cast<float>(*level_power) * status_power));
    EXPECT_EQ(touch.source_entity_id, 17);
    EXPECT_EQ(touch.team, changed ? EntityDamageTeam(TeamType::Friendly, 9) : EntityDamageTeam(TeamType::Enemy, 4));
    ASSERT_TRUE(std::holds_alternative<PolyF>(touch.damage_area));
    auto const& touch_poly = std::get<PolyF>(touch.damage_area);
    EXPECT_TRUE(touch_poly.contains(changed ? Vec2F(-0.25f, 0.25f) : Vec2F(0.25f, 0.25f)));
    EXPECT_FALSE(touch_poly.contains(changed ? Vec2F(0.25f, 0.25f) : Vec2F(-0.25f, 0.25f)));

    std::size_t next = 2;
    for (auto const& part : part_order) {
      if (part == "ray") {
        for (auto const& segment : segments) {
          auto const& ray = damage[next++];
          EXPECT_EQ(ray.damage_source_kind, part);
          EXPECT_FLOAT_EQ(ray.damage, 4.0f * (static_cast<float>(*level_power) * status_power));
          EXPECT_EQ(ray.source_entity_id, 61);
          EXPECT_EQ(ray.team, EntityDamageTeam(TeamType::Enemy, 7));
          ASSERT_TRUE(std::holds_alternative<Line2F>(ray.damage_area));
          ASSERT_TRUE(std::holds_alternative<Vec2F>(ray.knockback));
          auto const& line = std::get<Line2F>(ray.damage_area);
          auto const& knockback = std::get<Vec2F>(ray.knockback);
          EXPECT_NEAR(line.min()[0], segment.line.min()[0], 0.0002f);
          EXPECT_NEAR(line.min()[1], segment.line.min()[1], 0.0002f);
          EXPECT_NEAR(line.max()[0], segment.line.max()[0], 0.0002f);
          EXPECT_NEAR(line.max()[1], segment.line.max()[1], 0.0002f);
          EXPECT_NEAR(knockback[0], segment.knockback[0], 0.00001f);
          EXPECT_NEAR(knockback[1], segment.knockback[1], 0.00001f);
        }
      } else {
        auto const& source = damage[next++];
        EXPECT_EQ(source.damage_source_kind, part);
        EXPECT_FLOAT_EQ(source.damage, (part == "poly" ? 3.0f : 5.0f) * (static_cast<float>(*level_power) * status_power));
        EXPECT_EQ(source.source_entity_id, null_entity_id);
        EXPECT_EQ(source.team, EntityDamageTeam(TeamType::Passive));
        if (part == "poly") {
          ASSERT_TRUE(std::holds_alternative<PolyF>(source.damage_area));
          EXPECT_TRUE(std::get<PolyF>(source.damage_area).contains(changed ? Vec2F(1.75f, 2.25f) : Vec2F(0.25f, 0.25f)));
        } else {
          EXPECT_TRUE(std::holds_alternative<std::monostate>(source.damage_area));
        }
      }
    }
  };
  constexpr float status_power = 1;
  ASSERT_NO_FATAL_FAILURE(script("assert(status.stat('powerMultiplier') == 1)"));
  sources = query();
  ASSERT_TRUE(sources) << sources.error().what();
  std::array clear_segments{Segment{Line2F({0, 0}, {18, 0}), {2, 1}}};
  ASSERT_NO_FATAL_FAILURE(check_sources(*sources, clear_segments, status_power, false));

  auto materials = root.material_database();
  ASSERT_TRUE(materials) << materials.error().what();
  auto dirt = (*materials)->material_id("dirt");
  ASSERT_TRUE(dirt) << dirt.error().what();
  (*world)->setTileProtectionEnabled(false);
  for (int wall_x : {-4, 4}) {
    auto position = origin + Vec2I(wall_x, 0);
    auto tile = (*world)->modifyServerTile(position, true);
    ASSERT_TRUE(tile && *tile);
    (*tile)->get().background = structure_material_id;
    auto placed = (*world)->forceModifyTile(position,
        PlaceMaterial{TileLayer::Foreground, *dirt, MaterialHue(0), TileCollisionOverride::Block}, true);
    ASSERT_TRUE(placed && *placed);
  }
  sources = query();
  ASSERT_TRUE(sources) << sources.error().what();
  std::array bounced_segments{
    Segment{Line2F({0, 0}, {3.5f, 0}), {2, 1}},
    Segment{Line2F({3.5f, 0}, {-3.5f, 0}), {-2, -1}},
    Segment{Line2F({-3.5f, 0}, {3.5f, 0}), {2, 1}}
  };
  ASSERT_NO_FATAL_FAILURE(check_sources(*sources, bounced_segments, status_power, false));

  ASSERT_NO_FATAL_FAILURE(script(R"LUA(
    animator.translateTransformationGroup("probe", {0, 2})
    animator.setFlipped(true, 1)
    mcontroller.setRotation(math.pi / 2)
    monster.setDamageTeam({type="friendly", team=9})
    status.setPersistentEffects("probe", {{stat="powerMultiplier", amount=0.75}})
    assert(status.stat("powerMultiplier") == 1.75)
    monster.setDamageSources({{damage=11, line={{0,0},{1,0}}, damageSourceKind="scriptChanged", sourceEntityId=63}})
  )LUA"));
  sources = query();
  ASSERT_TRUE(sources) << sources.error().what();
  std::array moved_segments{Segment{Line2F({2, 2}, {-16, 2}), {-2, 3}}};
  ASSERT_NO_FATAL_FAILURE(check_sources(*sources, moved_segments, 1.75f, true));

  ASSERT_NO_FATAL_FAILURE(script(R"LUA(
    monster.setDamageOnTouch(false)
    monster.setDamageParts({})
    monster.setDamageSources()
    status.clearPersistentEffects("probe")
  )LUA"));
  sources = query();
  ASSERT_TRUE(sources) << sources.error().what();
  EXPECT_TRUE(sources->empty());

  // A level arriving through the existing wire path must change a warm query's
  // level scaling, even when the receiver reuses the same touch template.
  constexpr float new_level = 6.5f;
  auto new_level_power = (*power)->evaluate(new_level);
  ASSERT_TRUE(new_level_power) << new_level_power.error().what();
  ASSERT_NE(*new_level_power, *level_power);
  variant->touch_damage_config = JsonObject{{"damage", "invalid"}, {"damageArea", Json()}};
  auto donor = Monster::create(root, std::move(*variant), new_level);
  ASSERT_TRUE(donor) << donor.error().what();
  ASSERT_TRUE((*world)->addEntity(*donor, 18));
  auto inactive = (*donor)->damage_sources();
  ASSERT_TRUE(inactive) << inactive.error().what();
  EXPECT_TRUE(inactive->empty());
  auto enabled = (*donor)->eval_script("monster.setDamageOnTouch(true); return true");
  ASSERT_TRUE(enabled && *enabled);
  EXPECT_FALSE((*donor)->damage_sources());
  auto state = (*donor)->write_net_state();
  ASSERT_TRUE(state) << state.error().what();
  ASSERT_TRUE((*monster)->read_net_state(state->first));
  EXPECT_EQ((*monster)->monster_level(), new_level);
  sources = query();
  ASSERT_TRUE(sources) << sources.error().what();
  ASSERT_EQ(sources->size(), 1u);
  EXPECT_EQ(sources->front().damage_source_kind, "touch");
  EXPECT_FLOAT_EQ(sources->front().damage, 7.0f * (2.0f * static_cast<float>(*new_level_power) * status_power));
});

const GameTestRegistration monster_appearance("RootTest", "MonsterAppearanceScaleAndPalettes", [](GameTest& fixture) {
  auto directory = File::temporary_directory(fixture.root().random());
  ASSERT_TRUE(directory) << directory.error().what();
  auto cleanup = make_scope_exit([&] { EXPECT_TRUE(File::remove_all(*directory)); });
  auto settings = isolation_settings(fixture.rootSettings(), *directory, "monster-appearance");
  ASSERT_TRUE(settings) << settings.error().what();
  auto overlay = File::relative_to(*directory, "assets");
  ASSERT_TRUE(writeIsolationModules(overlay, {
    {"appearance-probe.animation", R"JSON({
      "animatedParts":{"parts":{"body":{"properties":{"image":"<partImage>"}}}}
    })JSON"},
    {"appearance-probe.monstertype", R"JSON({
      "type":"appearance-probe","categories":["appearance-probe"],"parts":["body"],
      "animation":"appearance-probe.animation",
      "baseParameters":{
        "scripts":[],"scale":1,"metaBoundBox":[-8,-8,8,8],"mouthOffset":[0,0],"feetOffset":[0,0],
        "colors":"appearance-palette",
        "movementSettings":{
          "standingPoly":[[-1,-2],[1,-2],[1,2],[-1,2]],
          "crouchingPoly":[[-1,-1],[1,-1],[1,1],[-1,1]]
        },
        "statusSettings":{"minimumLiquidStatusEffectPercentage":1,
          "appliesEnvironmentStatusEffects":false,"appliesWeatherStatusEffects":false}
      }
    })JSON"},
    {"appearance-probe.monsterpart", R"JSON({
      "name":"body","category":"appearance-probe","type":"body","frames":{"body":"appearance-probe.png"}
    })JSON"},
    {"appearance-palette.monstercolors", R"JSON({
      "name":"appearance-palette","swaps":[{"ff0000":"0000ff","00ff00":"ffff00"}]
    })JSON"},
    {"appearance-empty.monstercolors", R"JSON({"name":"appearance-empty","swaps":[{}]})JSON"}
  }));

  std::array source_pixels{Vec4B(255, 0, 0, 255), Vec4B(0, 255, 0, 255)};
  std::array palette_pixels{Vec4B(0, 0, 255, 255), Vec4B(255, 255, 0, 255)};
  std::array explicit_pixels{Vec4B(255, 0, 255, 255), source_pixels[1]};
  Image image(2, 1, PixelFormat::RGBA32);
  ASSERT_TRUE(image.set(0, 0, source_pixels[0]));
  ASSERT_TRUE(image.set(1, 0, source_pixels[1]));
  auto png = std::make_shared<Buffer>();
  ASSERT_TRUE(image.write_png(fixture.root().logger(), png));
  ASSERT_TRUE(File::write_file(png->take_data(), File::relative_to(overlay, "appearance-probe.png")));

  auto root = Root::create(std::move(*settings));
  ASSERT_TRUE(root) << root.error().what();
  auto database = (*root)->monster_database();
  ASSERT_TRUE(database) << database.error().what();
  auto assets = (*root)->assets();
  ASSERT_TRUE(assets) << assets.error().what();
  auto metadata = (*root)->image_metadata_database();
  ASSERT_TRUE(metadata) << metadata.error().what();

  auto check_portrait = [&](std::expected<List<Drawable>, StarException> const& portrait,
                            float scale, std::array<Vec4B, 2> const& colors) {
    ASSERT_TRUE(portrait) << portrait.error().what();
    ASSERT_EQ(portrait->size(), 1u);
    auto const& drawable = portrait->front();
    ASSERT_TRUE(drawable.is_image());
    auto const& path = drawable.image_part().image;
    EXPECT_EQ(path.base_path, "/appearance-probe.png");
    EXPECT_EQ(path.directives.empty(), colors == source_pixels);
    auto bounds = drawable.bound_box(**metadata, false);
    ASSERT_TRUE(bounds) << bounds.error().what();
    EXPECT_FLOAT_EQ(bounds->width(), 2 * scale);
    EXPECT_FLOAT_EQ(bounds->height(), scale);
    auto decoded = (*assets)->image(path);
    ASSERT_TRUE(decoded) << decoded.error().what();
    for (unsigned x = 0; x != colors.size(); ++x) {
      auto pixel = (*decoded)->get(x, 0);
      ASSERT_TRUE(pixel) << pixel.error().what();
      EXPECT_EQ(*pixel, colors[x]);
    }
  };

  auto base = (*database)->monster_variant("appearance-probe", 53);
  ASSERT_TRUE(base) << base.error().what();
  ASSERT_NO_FATAL_FAILURE(check_portrait((*database)->monster_portrait(*base), 1.0f, palette_pixels));
  JsonObject overrides{{"scale", 2.5f}, {"colorSwap", JsonObject{{"ff0000", "ff00ff"}}}};
  auto monster = (*database)->create_monster(*base, 1.0f, overrides);
  ASSERT_TRUE(monster) << monster.error().what();
  EXPECT_EQ((*monster)->collision_area(), RectF(-2.5f, -5, 2.5f, 5));
  auto& movement = (*monster)->movement_controller();
  ASSERT_TRUE(movement.load_state(JsonObject{{"position", JsonArray{0, 0}}, {"velocity", JsonArray{0, 0}},
    {"rotation", 0.0f}, {"movingDirection", "left"}, {"facingDirection", "left"}, {"crouching", true}}));
  ASSERT_TRUE(movement.update_base_parameters(ActorMovementParameters{}));
  EXPECT_EQ((*monster)->collision_area(), RectF(-2.5f, -2.5f, 2.5f, 2.5f));
  ASSERT_NO_FATAL_FAILURE(check_portrait((*monster)->portrait(PortraitMode::Full), 2.5f, explicit_pixels));
  auto generated = (*database)->monster_variant("appearance-probe", 53, overrides);
  ASSERT_TRUE(generated) << generated.error().what();
  ASSERT_NO_FATAL_FAILURE(check_portrait((*database)->monster_portrait(*generated), 2.5f, explicit_pixels));

  for (auto const& empty : {JsonObject{{"colorSwap", JsonObject{}}}, JsonObject{{"colors", "appearance-empty"}}}) {
    auto variant = (*database)->monster_variant("appearance-probe", 53, empty);
    ASSERT_TRUE(variant) << variant.error().what();
    ASSERT_NO_FATAL_FAILURE(check_portrait((*database)->monster_portrait(*variant), 1.0f, source_pixels));
    auto entity = (*database)->create_monster(std::move(*variant), 1.0f);
    ASSERT_TRUE(entity) << entity.error().what();
    ASSERT_NO_FATAL_FAILURE(check_portrait((*entity)->portrait(PortraitMode::Full), 1.0f, source_pixels));
  }

  auto cached = (*database)->monster_variant("appearance-probe", 53);
  ASSERT_TRUE(cached) << cached.error().what();
  EXPECT_FALSE(cached->color_swap);
  auto cached_entity = (*database)->create_monster(*cached, 1.0f);
  ASSERT_TRUE(cached_entity) << cached_entity.error().what();
  EXPECT_EQ((*cached_entity)->collision_area(), RectF(-1, -2, 1, 2));
  ASSERT_NO_FATAL_FAILURE(check_portrait((*cached_entity)->portrait(PortraitMode::Full), 1.0f, palette_pixels));

  auto with_default = (*database)->monster_variant("appearance-probe", 53, JsonObject{{"colors", "default"}});
  ASSERT_TRUE(with_default) << with_default.error().what();
  auto with_missing = (*database)->monster_variant("appearance-probe", 53, JsonObject{{"colors", "appearance-missing"}});
  ASSERT_TRUE(with_missing) << with_missing.error().what();
  auto default_portrait = (*database)->monster_portrait(*with_default);
  auto fallback_portrait = (*database)->monster_portrait(*with_missing);
  ASSERT_TRUE(default_portrait) << default_portrait.error().what();
  ASSERT_TRUE(fallback_portrait) << fallback_portrait.error().what();
  ASSERT_EQ(default_portrait->size(), 1u);
  ASSERT_EQ(fallback_portrait->size(), 1u);
  EXPECT_TRUE(default_portrait->front().image_part() == fallback_portrait->front().image_part());

  // An empty list defines no palette; unlike [{}], it cannot supply a fallback.
  // Override the real default asset in the test's private source only.
  auto palette_assets = (*assets)->scan_extension("monstercolors");
  ASSERT_TRUE(palette_assets) << palette_assets.error().what();
  std::optional<String> default_path;
  for (auto const& asset : palette_assets->get()) {
    auto config = (*assets)->json(asset.string());
    ASSERT_TRUE(config) << config.error().what();
    if (!config->is_null() && config->get_string("name") == String("default")) {
      auto relative_path = asset.string().substr(1);
      ASSERT_TRUE(relative_path) << relative_path.error().what();
      default_path = File::relative_to(overlay, *relative_path);
      break;
    }
  }
  ASSERT_TRUE(default_path);
  ASSERT_TRUE(File::create_directories(File::dir_name(*default_path)));
  ASSERT_TRUE(File::write_file(String(R"JSON({"name":"default","swaps":[]})JSON"), *default_path));
  ASSERT_TRUE((*root)->reload());
  auto reloaded = (*root)->monster_database();
  ASSERT_TRUE(reloaded) << reloaded.error().what();
  for (auto const& swaps : {JsonObject{{"ff0000", "ff00ff"}}, JsonObject{}}) {
    auto missing = (*reloaded)->monster_variant("appearance-probe", 53,
        JsonObject{{"colors", "appearance-missing"}, {"colorSwap", swaps}});
    ASSERT_TRUE(missing) << missing.error().what();
    auto portrait_error = (*reloaded)->monster_portrait(*missing);
    auto entity_error = (*reloaded)->create_monster(std::move(*missing), 1.0f);
    ASSERT_FALSE(portrait_error);
    ASSERT_FALSE(entity_error);
    EXPECT_EQ(entity_error.error().what(), portrait_error.error().what());
    EXPECT_NE(std::string_view(entity_error.error().what()).find("default"), std::string_view::npos);
  }

  auto malformed = (*reloaded)->monster_variant("appearance-probe", 53,
      JsonObject{{"colors", false}, {"colorSwap", JsonObject{{"ff0000", "ff00ff"}}}});
  ASSERT_TRUE(malformed) << malformed.error().what();
  auto invalid_colors = malformed->parameters.get_string("colors", "default");
  auto portrait_error = (*reloaded)->monster_portrait(*malformed);
  auto entity_error = (*reloaded)->create_monster(std::move(*malformed), 1.0f);
  ASSERT_FALSE(invalid_colors);
  ASSERT_FALSE(portrait_error);
  ASSERT_FALSE(entity_error);
  EXPECT_EQ(entity_error.error().what(), invalid_colors.error().what());
  EXPECT_EQ(portrait_error.error().what(), invalid_colors.error().what());
});

const GameTestRegistration simultaneousWorldMaterials("RootTest", "SimultaneousWorldMaterials", [](GameTest& fixture) {
  // Select a genuinely unused real ID, rather than assuming a mod collection
  // leaves a hard-coded material slot free.
  auto materials = fixture.root().material_database();
  ASSERT_TRUE(materials) << materials.error().what();
  MaterialId material = 0;
  while (material < first_meta_material_id && (*materials)->is_valid_material_id(material))
    ++material;
  ASSERT_LT(material, first_meta_material_id);

  auto directoryA = File::temporary_directory(fixture.root().random());
  ASSERT_TRUE(directoryA) << directoryA.error().what();
  auto cleanupA = make_scope_exit([&] { EXPECT_TRUE(File::remove_all(*directoryA)); });
  auto directoryB = File::temporary_directory(fixture.root().random());
  ASSERT_TRUE(directoryB) << directoryB.error().what();
  auto cleanupB = make_scope_exit([&] { EXPECT_TRUE(File::remove_all(*directoryB)); });
  auto settingsA = isolation_settings(fixture.rootSettings(), *directoryA, "A");
  ASSERT_TRUE(settingsA) << settingsA.error().what();
  auto settingsB = isolation_settings(fixture.rootSettings(), *directoryB, "B");
  ASSERT_TRUE(settingsB) << settingsB.error().what();
  auto overlayA = File::relative_to(*directoryA, "assets");
  auto overlayB = File::relative_to(*directoryB, "assets");
  ASSERT_TRUE(writeIsolationMaterial(overlayA, material, "block"));
  ASSERT_TRUE(writeIsolationMaterial(overlayB, material, "none"));

  auto createdA = Root::create(*settingsA);
  ASSERT_TRUE(createdA) << createdA.error().what();
  auto rootA = std::move(*createdA);
  rootA->logger().add_sink(std::make_shared<RootIsolationErrorSink>());
  auto createdB = Root::create(*settingsB);
  ASSERT_TRUE(createdB) << createdB.error().what();
  auto rootB = std::move(*createdB);
  rootB->logger().add_sink(std::make_shared<RootIsolationErrorSink>());
  auto storageA = File::ephemeral_file(rootA->random());
  ASSERT_TRUE(storageA) << storageA.error().what();
  auto createdWorldA = WorldServer::create(*rootA, Vec2U(2048, 2048), *storageA);
  ASSERT_TRUE(createdWorldA) << createdWorldA.error().what();
  auto worldA = std::move(*createdWorldA);
  auto closeWorldA = make_scope_exit([&] { if (worldA) EXPECT_TRUE(worldA->close()); });
  auto storageB = File::ephemeral_file(rootB->random());
  ASSERT_TRUE(storageB) << storageB.error().what();
  auto createdWorldB = WorldServer::create(*rootB, Vec2U(2048, 2048), *storageB);
  ASSERT_TRUE(createdWorldB) << createdWorldB.error().what();
  auto worldB = std::move(*createdWorldB);
  auto closeWorldB = make_scope_exit([&] { EXPECT_TRUE(worldB->close()); });
  {
    auto materials_a = rootA->material_database();
    ASSERT_TRUE(materials_a) << materials_a.error().what();
    auto materials_b = rootB->material_database();
    ASSERT_TRUE(materials_b) << materials_b.error().what();
    EXPECT_EQ((*materials_a)->material_id("root-isolation"), material);
    EXPECT_EQ((*materials_b)->material_id("root-isolation"), material);
  }

  // Identical material IDs and placement requests produce different actual
  // world collision behavior, not merely different catalog properties.
  ASSERT_TRUE(placeIsolationMaterial(*worldA, Vec2I(1024, 1024), material, true));
  ASSERT_TRUE(placeIsolationMaterial(*worldB, Vec2I(1024, 1024), material, false));
  ASSERT_TRUE(writeIsolationMaterial(overlayA, material, "none"));
  ASSERT_TRUE(writeIsolationMaterial(overlayB, material, "block"));
  ASSERT_TRUE(rootA->reload());
  ASSERT_TRUE(placeIsolationMaterial(*worldA, Vec2I(1032, 1024), material, false));
  ASSERT_TRUE(placeIsolationMaterial(*worldB, Vec2I(1032, 1024), material, false));

  ASSERT_TRUE(worldA->close());
  worldA.reset();
  rootA.reset();
  ASSERT_TRUE(placeIsolationMaterial(*worldB, Vec2I(1040, 1024), material, false));
  ASSERT_TRUE(rootB->reload());
  ASSERT_TRUE(placeIsolationMaterial(*worldB, Vec2I(1048, 1024), material, true));
});

const GameTestRegistration item_shared_builder_seeds("ItemDatabaseTest", "SharedBuilderSeedsUseRequestIdentity", [](GameTest& fixture) {
  auto directory = File::temporary_directory(fixture.root().random());
  if (!GameTest::check(directory.has_value(), "creating the builder fixture directory"))
    return;
  auto cleanup = make_scope_exit([&] {
    GameTest::check(bool(File::remove_all(*directory)), "removing the builder fixture directory");
  });
  auto settings = isolation_settings(fixture.rootSettings(), *directory, "item-seed-probe");
  if (!GameTest::check(settings.has_value(), "configuring the builder fixture"))
    return;
  auto written = writeIsolationModules(File::relative_to(*directory, "assets"), {
    {"item-seed-probe.item", R"({
      "itemName":"item-seed-probe","rarity":"common","maxStack":10,"builder":"/item-seed-probe.lua"
    })"},
    {"item-seed-probe.lua", R"LUA(
      function build(_, config, parameters, level, seed)
        local builds = root.getConfiguration("itemSeedProbeBuilds") + 1
        root.setConfiguration("itemSeedProbeBuilds", builds)
        parameters.buildLevel = level
        if parameters.seedMode == "retain" then
          parameters.seed = seed
        elseif parameters.seedMode == "generate" then
          parameters.wasSeeded = seed ~= nil
          -- Disjoint ranges let the test observe fresh random draws without collision risk.
          parameters.seed = seed or builds * 1000 + sb.makeRandomSource():randInt(0, 999)
        end
        return config, parameters
      end
    )LUA"}
  });
  if (!GameTest::check(written.has_value(), "writing the builder fixture assets"))
    return;
  auto root = Root::create(std::move(*settings));
  if (!GameTest::check(root.has_value(), "creating the builder fixture root"))
    return;
  auto configuration = (*root)->configuration();
  if (!GameTest::check(configuration && (*configuration)->set("itemSeedProbeBuilds", 0), "initializing the Lua build counter"))
    return;
  auto database = (*root)->itemDatabase();
  if (!GameTest::check(database.has_value(), "loading the builder fixture item database"))
    return;
  auto valid_item = [](std::expected<std::shared_ptr<Item>, StarException> const& result) {
    return result && *result && (*result)->name() == "item-seed-probe";
  };

  auto ignore = ItemDescriptor::create("item-seed-probe", 1);
  auto retain = ItemDescriptor::create("item-seed-probe", 1, JsonObject{{"seedMode", "retain"}});
  auto generate = ItemDescriptor::create("item-seed-probe", 1, JsonObject{{"seedMode", "generate"}});
  if (!GameTest::check(ignore && retain && generate, "creating the seed probe descriptors"))
    return;
  auto& db = **database;
  auto ignored = db.item_shared(*ignore, {}, 0);
  if (!GameTest::check(valid_item(ignored), "running the builder that discards the explicit seed"))
    return;
  GameTest::check(db.item_shared(*ignore, {}, 0) == *ignored && (*configuration)->get("itemSeedProbeBuilds") == Json(1),
      "the same explicit seed hits before invoking a builder that discards it");
  auto unseeded = db.item_shared(*ignore);
  GameTest::check(valid_item(unseeded) && *unseeded != *ignored && db.item_shared(*ignore) == *unseeded,
      "a seedless result remains cacheable but cannot alias explicit seed zero");
  auto other_seed = db.item_shared(*ignore, {}, 1);
  GameTest::check(valid_item(other_seed) && *other_seed != *ignored && db.item_shared(*ignore, {}, 1) == *other_seed,
      "distinct explicit seeds do not alias even when the builder ignores them");
  auto other_count = db.item_shared(ignore->with_count(2), {}, 0);
  GameTest::check(valid_item(other_count) && *other_count != *ignored && (*other_count)->count() == 2 && (*ignored)->count() == 1,
      "shared requests with different counts do not alias");
  auto other_level = db.item_shared(*ignore, 2.0f, 0);
  GameTest::check(valid_item(other_level) && *other_level != *ignored && (*other_level)->parameters().get_double("buildLevel") == 2.0,
      "shared requests with different levels do not alias");

  auto retained = db.item_shared(*retain, {}, 17);
  GameTest::check(valid_item(retained) && (*retained)->parameters().get_uint("seed") == 17u && db.item_shared(*retain, {}, 17) == *retained,
      "an explicit seed still hits when the builder retains it");
  auto first = db.item_shared(*generate);
  auto second = db.item_shared(*generate);
  if (!GameTest::check(valid_item(first) && valid_item(second), "running the unseeded random builder twice"))
    return;
  auto first_seed = (*first)->parameters().get_uint("seed");
  auto second_seed = (*second)->parameters().get_uint("seed");
  if (!GameTest::check(first_seed && second_seed && *first_seed != *second_seed && *first != *second,
      "unseeded generation runs again and preserves its fresh seed"))
    return;
  auto explicit_generated_seed = db.item_shared(*generate, {}, *first_seed);
  if (!GameTest::check(valid_item(explicit_generated_seed) && *explicit_generated_seed != *first
      && (*explicit_generated_seed)->parameters().get_bool("wasSeeded") == true
      && (*explicit_generated_seed)->parameters().get_uint("seed") == *first_seed
      && db.item_shared(*generate, {}, *first_seed) == *explicit_generated_seed,
      "an unseeded result cannot populate the cache entry for the seed it generated"))
    return;
  auto third = db.item_shared(*generate);
  if (!GameTest::check(valid_item(third), "running the unseeded random builder after an explicit cache hit"))
    return;
  auto third_seed = (*third)->parameters().get_uint("seed");
  GameTest::check(third_seed && *third_seed != *first_seed && *third_seed != *second_seed
      && *third != *explicit_generated_seed && (*third)->parameters().get_bool("wasSeeded") == false,
      "caching an explicit result does not stop later unseeded generation");
});

const GameTestRegistration animatorPartDrawableFirstAdd("AnimatorTest", "PartDrawableFirstAdd", [](GameTest& fixture) {
  auto config = Json::parse_json(R"JSON({"animatedParts":{"parts":{"body":{"properties":{}}}}})JSON");
  ASSERT_TRUE(config) << config.error().what();
  for (bool scripted : {false, true}) {
    for (bool emptyFirst : {false, true}) {
      SCOPED_TRACE(scripted ? "scripted" : "networked");
      SCOPED_TRACE(emptyFirst ? "empty first add" : "nonempty first add");
      auto created = NetworkedAnimator::create(fixture.root(), *config);
      ASSERT_TRUE(created) << created.error().what();
      auto animator = std::move(*created);
      auto callbacks = scripted
          ? LuaBindings::make_scripted_animator_callbacks(*animator,
              [](String const&, Json const& fallback) -> std::expected<Json, StarException> { return fallback; })
          : LuaBindings::make_networked_animator_callbacks(*animator);
      ASSERT_TRUE(callbacks) << callbacks.error().what();
      auto engine = LuaEngine::create();
      ASSERT_TRUE(engine) << engine.error().what();
      (*engine)->set_execution_host(std::make_unique<LuaExecutionScope>(fixture.root()));
      auto contextResult = (*engine)->create_context();
      ASSERT_TRUE(contextResult) << contextResult.error().what();
      auto context = std::move(*contextResult);
      ASSERT_TRUE(context.set_callbacks("animator", *callbacks));

      auto widths = [&]() -> std::expected<std::vector<float>, StarException> {
        auto drawables = animator->drawables();
        if (!drawables) return std::unexpected(std::move(drawables.error()));
        std::vector<float> result;
        for (auto const& drawable : *drawables) {
          auto json = drawable.to_json();
          if (!json) return std::unexpected(std::move(json.error()));
          auto width = json->get_float("width");
          if (!width) return std::unexpected(std::move(width.error()));
          result.push_back(*width);
        }
        return result;
      };
      if (emptyFirst) {
        ASSERT_TRUE(context.load("animator.addPartJsonDrawables('body', {})"));
        auto output = widths();
        ASSERT_TRUE(output) << output.error().what();
        EXPECT_TRUE(output->empty());
      }
      ASSERT_TRUE(context.load(R"LUA(animator.addPartJsonDrawables('body', {{line = {{0,0}, {1,1}}, width = 11}}))LUA"));
      auto firstOutput = widths();
      ASSERT_TRUE(firstOutput) << firstOutput.error().what();
      EXPECT_EQ(*firstOutput, (std::vector<float>{11}));
      ASSERT_TRUE(context.load(R"LUA(animator.addPartJsonDrawables('body', {{line = {{0,0}, {2,2}}, width = 22}}))LUA"));
      auto appendedOutput = widths();
      ASSERT_TRUE(appendedOutput) << appendedOutput.error().what();
      EXPECT_EQ(*appendedOutput, (std::vector<float>{11, 22}));

      for (std::string_view operation : {"addPartJsonDrawables", "setPartJsonDrawables"}) {
        String script = String("animator.") + operation
            + R"LUA(('body', {{line = {{0,0}, {3,3}}, width = 33}, {line = {{0,0}, {4,4}}, width = 'invalid'}}))LUA";
        auto failed = context.load(script);
        EXPECT_FALSE(failed);
        EXPECT_EQ(context.eval<int>("6 * 7"), 42);
        auto output = widths();
        ASSERT_TRUE(output) << output.error().what();
        EXPECT_EQ(*output, (std::vector<float>{11, 22}));
      }
    }
  }
});

// Use authored items with actual handedness instead of test-only overrides.
std::expected<std::shared_ptr<Item>, StarException> hudAuthoredItem(
    ItemDatabase const& database, ItemType requiredType, bool twoHanded) {
  auto names = database.all_items();
  names.sort();
  for (auto const& name : names) {
    auto type = database.item_type(name);
    if (!type) return std::unexpected(std::move(type.error()));
    if (*type != requiredType)
      continue;
    auto item = database.from_json(name);
    if (!item) return std::unexpected(std::move(item.error()));
    if (*item && (*item)->two_handed() == twoHanded)
      return *item;
  }
  return std::unexpected(ItemException("HUD inventory regression requires an authored item with the requested type and handedness"));
}

std::expected<BagSlot, StarException> hudItemBagSlot(Root& root, PlayerInventory const& inventory,
    std::shared_ptr<Item> const& item) {
  auto assets = root.assets();
  if (!assets) return std::unexpected(std::move(assets.error()));
  auto config = (*assets)->json("/player.config:inventory.itemBags");
  if (!config) return std::unexpected(std::move(config.error()));
  auto bags = config->to_object();
  if (!bags) return std::unexpected(std::move(bags.error()));
  auto names = bags->keys();
  names.sort();
  for (auto const& name : names) {
    auto bag = inventory.bagContents(name);
    if (!bag || bag->size() < 2)
      continue;
    auto allowed = inventory.itemAllowedInBag(item, name);
    if (!allowed) return std::unexpected(std::move(allowed.error()));
    if (*allowed)
      // An empty earlier slot must not steal the cursor's native return slot.
      return BagSlot{name, static_cast<std::uint8_t>(bag->size() - 1)};
  }
  return std::unexpected(ItemException("HUD inventory regression requires a configured accepting bag"));
}

const GameTestRegistration hudInventoryLinks("HudDomainTest", "InventoryLinksPreserveCursorOwnership", [](GameTest& fixture) {
  auto& root = fixture.root();
  auto database = root.itemDatabase();
  ASSERT_TRUE(database) << database.error().what();
  auto created = PlayerInventory::create(root);
  ASSERT_TRUE(created) << created.error().what();
  auto& inventory = **created;
  ASSERT_GE(inventory.custom_bar_indexes(), 2);
  auto expectSlot = [](std::expected<std::optional<InventorySlot>, StarException> const& actual,
      std::optional<InventorySlot> const& expected) {
    ASSERT_TRUE(actual) << actual.error().what();
    EXPECT_TRUE(*actual == expected);
  };
  auto expectItem = [](std::expected<std::shared_ptr<Item>, StarException> const& actual, Json const& descriptor) {
    ASSERT_TRUE(actual) << actual.error().what();
    if (descriptor.is_null()) {
      EXPECT_FALSE(*actual);
      return;
    }
    ASSERT_TRUE(*actual);
    EXPECT_EQ((*actual)->descriptor().to_json(), descriptor);
  };
  auto oneHanded = hudAuthoredItem(**database, ItemType::Consumable, false);
  ASSERT_TRUE(oneHanded) << oneHanded.error().what();
  auto twoHanded = hudAuthoredItem(**database, ItemType::MiningTool, true);
  ASSERT_TRUE(twoHanded) << twoHanded.error().what();
  Json const oneDescriptor = (*oneHanded)->descriptor().to_json();
  Json const twoDescriptor = (*twoHanded)->descriptor().to_json();
  auto oneSlot = hudItemBagSlot(root, inventory, *oneHanded);
  auto twoSlot = hudItemBagSlot(root, inventory, *twoHanded);
  ASSERT_TRUE(oneSlot) << oneSlot.error().what();
  ASSERT_TRUE(twoSlot) << twoSlot.error().what();
  if (*oneSlot == *twoSlot)
    --twoSlot->index;
  ASSERT_EQ(inventory.setItem(*oneSlot, *oneHanded), true);
  ASSERT_EQ(inventory.setItem(*twoSlot, *twoHanded), true);
  ASSERT_TRUE(inventory.set_custom_bar_secondary_slot(1, InventorySlot(*oneSlot)));
  ASSERT_TRUE(inventory.shiftSwap(*oneSlot));
  expectSlot(inventory.custom_bar_secondary_slot(1),
      std::optional<InventorySlot>(InventorySlot(SwapSlot())));
  ASSERT_TRUE(inventory.link_custom_bar_swap(0, false));
  EXPECT_FALSE(inventory.swapSlotItem());
  expectItem(inventory.itemsAt(*oneSlot), oneDescriptor);
  expectSlot(inventory.custom_bar_secondary_slot(0), std::optional<InventorySlot>(InventorySlot(*oneSlot)));
  expectSlot(inventory.custom_bar_secondary_slot(1), std::optional<InventorySlot>(InventorySlot(*oneSlot)));

  ASSERT_TRUE(inventory.set_custom_bar_primary_slot(1, InventorySlot(*twoSlot)));
  ASSERT_TRUE(inventory.shiftSwap(*twoSlot));
  auto beforeInvalid = inventory.store();
  ASSERT_TRUE(beforeInvalid) << beforeInvalid.error().what();
  for (bool primary : {false, true}) {
    EXPECT_FALSE(inventory.link_custom_bar_swap(inventory.custom_bar_indexes(), primary));
    EXPECT_FALSE(inventory.clear_custom_bar_link(inventory.custom_bar_indexes(), primary));
    auto afterInvalid = inventory.store();
    ASSERT_TRUE(afterInvalid) << afterInvalid.error().what();
    EXPECT_EQ(*afterInvalid, *beforeInvalid);
  }
  // Dropping onto the right half redirects a two-handed tool to primary,
  // returns it to its exact bag slot, and retargets all existing cursor links.
  ASSERT_TRUE(inventory.link_custom_bar_swap(0, false));
  EXPECT_FALSE(inventory.swapSlotItem());
  expectItem(inventory.itemsAt(*twoSlot), twoDescriptor);
  expectSlot(inventory.custom_bar_primary_slot(0), std::optional<InventorySlot>(InventorySlot(*twoSlot)));
  expectSlot(inventory.custom_bar_primary_slot(1), std::optional<InventorySlot>(InventorySlot(*twoSlot)));
  expectSlot(inventory.custom_bar_secondary_slot(0), std::optional<InventorySlot>(InventorySlot(*oneSlot)));
  ASSERT_TRUE(inventory.select_action_bar_location(CustomBarIndex(0)));
  expectItem(inventory.primary_held_item(), twoDescriptor);
  expectItem(inventory.secondary_held_item(), {});
  ASSERT_TRUE(inventory.clear_custom_bar_link(0, false));
  expectSlot(inventory.custom_bar_primary_slot(0), std::optional<InventorySlot>{});
  expectItem(inventory.secondary_held_item(), oneDescriptor);
  expectItem(inventory.itemsAt(*twoSlot), twoDescriptor);
  expectSlot(inventory.custom_bar_primary_slot(1), std::optional<InventorySlot>(InventorySlot(*twoSlot)));
  ASSERT_TRUE(inventory.clear_custom_bar_link(0, false));
  expectSlot(inventory.custom_bar_secondary_slot(0), std::optional<InventorySlot>{});
  expectItem(inventory.itemsAt(*oneSlot), oneDescriptor);
  expectSlot(inventory.custom_bar_secondary_slot(1), std::optional<InventorySlot>(InventorySlot(*oneSlot)));
  auto items = inventory.allItems();
  ASSERT_TRUE(items) << items.error().what();
  EXPECT_EQ(items->size(), 2u);
  // Empty-cursor clicks select a valid cell, but cannot install an invalid
  // selection that would make subsequent held-item queries fail.
  ASSERT_TRUE(inventory.link_custom_bar_swap(1, false));
  EXPECT_TRUE(inventory.selected_action_bar_location() == SelectedActionBarLocation(CustomBarIndex(1)));
  auto beforeEmptyInvalid = inventory.store();
  ASSERT_TRUE(beforeEmptyInvalid) << beforeEmptyInvalid.error().what();
  for (bool primary : {false, true}) {
    EXPECT_FALSE(inventory.link_custom_bar_swap(inventory.custom_bar_indexes(), primary));
    EXPECT_FALSE(inventory.clear_custom_bar_link(inventory.custom_bar_indexes(), primary));
    auto afterEmptyInvalid = inventory.store();
    ASSERT_TRUE(afterEmptyInvalid) << afterEmptyInvalid.error().what();
    EXPECT_EQ(*afterEmptyInvalid, *beforeEmptyInvalid);
  }
});

const GameTestRegistration pending_interaction_completion(
    "InteractionQueueTest", "PendingAtObservationDeliversOnNextPoll", [](GameTest& fixture) {
  auto factory = fixture.root().playerFactory();
  ASSERT_TRUE(factory) << factory.error().what();
  auto created = (*factory)->create();
  ASSERT_TRUE(created) << created.error().what();
  auto& player = **created;

  struct CompleteOnSecondPoll {
    RpcPromiseKeeper<InteractAction> keeper;
    unsigned& polls;
    std::expected<void, StarException> poll() {
      if (++polls == 2)
        return keeper.fulfill({InteractActionType::ShowPopup, 17, "ready"});
      return {};
    }
    std::expected<void, StarException> cancel() { return {}; }
  };
  unsigned polls = 0;
  auto [source, keeper] = RpcPromise<InteractAction>::make_pair();
  auto observation = source.with_cancellation({}, "cancelled", CompleteOnSecondPoll{std::move(keeper), polls});
  ASSERT_TRUE(observation) << observation.error().what();
  player.queue_interact_action(std::move(*observation));

  auto pending = player.pull_interact_actions();
  ASSERT_TRUE(pending) << pending.error().what();
  EXPECT_TRUE(pending->empty());
  EXPECT_EQ(polls, 1u);
  auto completed = player.pull_interact_actions();
  ASSERT_TRUE(completed) << completed.error().what();
  ASSERT_EQ(completed->size(), 1u);
  EXPECT_EQ(completed->front().entity_id, 17);
  EXPECT_EQ(completed->front().data, Json("ready"));
  auto drained = player.pull_interact_actions();
  ASSERT_TRUE(drained) << drained.error().what();
  EXPECT_TRUE(drained->empty());
  EXPECT_EQ(polls, 2u);
});

const GameTestRegistration retry_interaction_batch(
    "InteractionQueueTest", "DiagnosticPreservesSiblingsAndStableOrder", [](GameTest& fixture) {
  auto factory = fixture.root().playerFactory();
  ASSERT_TRUE(factory) << factory.error().what();
  auto created = (*factory)->create();
  ASSERT_TRUE(created) << created.error().what();
  auto& player = **created;
  using Promise = RpcPromise<InteractAction>;
  auto action = [](EntityId id) { return InteractAction{InteractActionType::Message, id, Json(id)}; };

  auto [first, first_keeper] = Promise::make_pair();
  auto [later, later_keeper] = Promise::make_pair();
  auto shared = Promise::make_fulfilled(action(2));
  unsigned attempts = 0;
  auto retry = Promise::make_fulfilled(action(4)).wrap([&](InteractAction value) -> std::expected<InteractAction, StarException> {
    if (++attempts == 1)
      return std::unexpected(StarException("retry interaction"));
    return value;
  });
  ASSERT_TRUE(retry) << retry.error().what();
  auto [cancelled, cancel_keeper] = Promise::make_pair();
  auto did_cancel = cancel_keeper.cancel("cancelled");
  ASSERT_TRUE(did_cancel && *did_cancel);
  player.queue_interact_action(std::move(first));
  player.queue_interact_action(shared);
  player.queue_interact_action(Promise::make_failed("failed"));
  player.queue_interact_action(std::move(*retry));
  player.queue_interact_action(std::move(later));
  player.queue_interact_action(std::move(cancelled));
  ASSERT_TRUE(player.interact(action(7)));

  auto diagnostic = player.pull_interact_actions();
  ASSERT_FALSE(diagnostic);
  EXPECT_NE(std::string_view(diagnostic.error().what()).find("retry interaction"), std::string_view::npos);
  auto delivered = player.pull_interact_actions();
  ASSERT_TRUE(delivered) << delivered.error().what();
  ASSERT_EQ(delivered->size(), 3u);
  EXPECT_EQ((*delivered)[0].entity_id, 2);
  EXPECT_EQ((*delivered)[1].entity_id, 4);
  EXPECT_EQ((*delivered)[2].entity_id, 7);
  EXPECT_EQ(attempts, 2u);
  auto sibling = shared.result();
  ASSERT_TRUE(sibling && *sibling);
  EXPECT_EQ((*sibling)->data, Json(2));

  ASSERT_TRUE(later_keeper.fulfill(action(5)));
  ASSERT_TRUE(first_keeper.fulfill(action(1)));
  auto remaining = player.pull_interact_actions();
  ASSERT_TRUE(remaining) << remaining.error().what();
  ASSERT_EQ(remaining->size(), 2u);
  EXPECT_EQ((*remaining)[0].entity_id, 1);
  EXPECT_EQ((*remaining)[1].entity_id, 5);
  auto drained = player.pull_interact_actions();
  ASSERT_TRUE(drained) << drained.error().what();
  EXPECT_TRUE(drained->empty());
});

const GameTestRegistration hud_radio_dequeue("HudDomainTest", "RadioUniqueAccountingOccursAtDequeue", [](GameTest& fixture) {
  auto factory = fixture.root().playerFactory();
  ASSERT_TRUE(factory) << factory.error().what();
  auto created = (*factory)->create();
  ASSERT_TRUE(created) << created.error().what();
  auto& player = **created;
  auto message = [](String const& id, bool unique, bool important = false) {
    return Json(JsonObject{{"messageId", id}, {"type", "generic"}, {"unique", unique},
        {"important", important}, {"text", id}, {"portraitImage", ""}});
  };
  auto unique = message("hud-domain-unique", true);
  ASSERT_TRUE(player.queue_radio_message(unique));
  ASSERT_TRUE(player.queue_radio_message(unique));
  EXPECT_FALSE(player.log()->has_radio_message("hud-domain-unique"));
  ASSERT_TRUE(player.queue_radio_message(message("hud-domain-normal", false)));
  ASSERT_TRUE(player.queue_radio_message(message("hud-domain-important-a", true, true)));
  ASSERT_TRUE(player.queue_radio_message(message("hud-domain-important-b", true, true)));
  EXPECT_TRUE(player.interrupt_radio_message());
  EXPECT_FALSE(player.interrupt_radio_message());
  for (auto const& id : {String("hud-domain-important-b"), String("hud-domain-important-a"),
      String("hud-domain-unique"), String("hud-domain-normal")}) {
    EXPECT_FALSE(player.log()->has_radio_message(id));
    auto next = player.pull_pending_radio_message();
    ASSERT_TRUE(next) << next.error().what();
    ASSERT_TRUE(*next);
    EXPECT_EQ((**next).message_id, id);
    EXPECT_EQ(player.log()->has_radio_message(id), id != "hud-domain-normal");
  }
  auto empty = player.pull_pending_radio_message();
  ASSERT_TRUE(empty);
  EXPECT_FALSE(*empty); // The duplicate pending unique message was not replayed.
  ASSERT_TRUE(player.queue_radio_message(unique));
  empty = player.pull_pending_radio_message();
  ASSERT_TRUE(empty);
  EXPECT_FALSE(*empty); // Dequeue, not UI completion, suppresses later resends.
  for (int replay = 0; replay != 2; ++replay) {
    ASSERT_TRUE(player.queue_radio_message(message("hud-domain-normal", false)));
    auto next = player.pull_pending_radio_message();
    ASSERT_TRUE(next && *next);
    EXPECT_EQ((**next).message_id, String("hud-domain-normal"));
    EXPECT_FALSE(player.log()->has_radio_message("hud-domain-normal"));
  }
  // The consumed unique set is player-owned, not a shared queue/database cache.
  auto other = (*factory)->create();
  ASSERT_TRUE(other) << other.error().what();
  ASSERT_TRUE((*other)->queue_radio_message(unique));
  auto next = (*other)->pull_pending_radio_message();
  ASSERT_TRUE(next && *next);
  EXPECT_EQ((**next).message_id, String("hud-domain-unique"));
});

const GameTestRegistration hud_radio_delayed("HudDomainTest", "RadioDelayedDispatchPreservesEventOrder", [](GameTest& fixture) {
  auto universe = TestUniverse::create(fixture.root(), {100, 100});
  ASSERT_TRUE(universe) << universe.error().what();
  auto close = make_scope_exit([&] { EXPECT_TRUE((*universe)->close()); });
  auto player = (*universe)->client()->mainPlayer();
  auto uuid = player->uuid();
  ASSERT_TRUE(uuid) << uuid.error().what();
  ASSERT_TRUE((*universe)->warpPlayer(ClientShipWorldId(*uuid)));
  auto world = (*universe)->worldClient();
  ASSERT_TRUE(world);

  auto message = [](String const& id, bool important = false, String const& type = "generic") {
    return Json(JsonObject{{"messageId", id}, {"type", type}, {"unique", false},
        {"important", important}, {"text", id}, {"portraitImage", ""}});
  };
  auto expect_messages = [&](std::initializer_list<String> ids) {
    for (auto const& id : ids) {
      auto next = player->pull_pending_radio_message();
      ASSERT_TRUE(next) << next.error().what();
      ASSERT_TRUE(*next) << id.utf8();
      EXPECT_EQ((**next).message_id, id);
      EXPECT_EQ((**next).text, id); // Re-requests must not replace the original payload.
    }
    auto empty = player->pull_pending_radio_message();
    ASSERT_TRUE(empty) << empty.error().what();
    EXPECT_FALSE(*empty);
  };

  constexpr float dt = 1.0f / 64.0f;
  ASSERT_TRUE(player->queue_radio_message(message("radio-survivor-a"), 4 * dt));
  ASSERT_TRUE(player->queue_radio_message(message("radio-normal-a"), dt));
  ASSERT_TRUE(player->queue_radio_message(message("radio-important-a", true), dt));
  ASSERT_TRUE(player->queue_radio_message(message("radio-survivor-b"), 4 * dt));
  ASSERT_TRUE(player->queue_radio_message(message("radio-rushed"), 32 * dt));
  ASSERT_TRUE(player->queue_radio_message(message("radio-normal-b"), dt));
  ASSERT_TRUE(player->queue_radio_message(message("radio-important-b", true), dt));
  auto mission = message("radio-mission", false, "mission");
  ASSERT_TRUE(player->queue_radio_message(mission, 3 * dt));
  ASSERT_TRUE(player->queue_radio_message(mission)); // Mission repeats never accelerate an accepted request.
  ASSERT_TRUE(player->queue_radio_message(message("radio-survivor-a"), dt)); // A positive duplicate leaves the deadline alone.
  ASSERT_TRUE(player->receive_message(server_connection_id, "queueRadioMessage",
      {JsonObject{{"messageId", "radio-rushed"}, {"type", "generic"}, {"important", true},
          {"text", "replacement"}, {"portraitImage", ""}}}));
  EXPECT_FALSE(player->interrupt_radio_message());
  expect_messages({});

  ASSERT_TRUE(player->queue_radio_message(message("radio-pending")));
  ASSERT_TRUE(world->update(dt));
  EXPECT_TRUE(player->interrupt_radio_message());
  EXPECT_FALSE(player->interrupt_radio_message());
  expect_messages({"radio-important-b", "radio-important-a", "radio-pending", "radio-normal-a", "radio-rushed", "radio-normal-b"});

  ASSERT_TRUE(world->update(dt));
  expect_messages({});
  ASSERT_TRUE(world->update(dt));
  expect_messages({"radio-mission"});
  EXPECT_FALSE(player->log()->has_radio_message("radio-mission"));
  ASSERT_TRUE(player->queue_radio_message(mission));
  expect_messages({});
  ASSERT_TRUE(world->update(dt));
  expect_messages({"radio-survivor-a", "radio-survivor-b"});
  ASSERT_TRUE(world->update(dt));
  expect_messages({});
});

const GameTestRegistration hud_radio_species("HudDomainTest", "RadioSpeciesOverrideUsesShipThenPlayer", [](GameTest& fixture) {
  auto factory = fixture.root().playerFactory();
  ASSERT_TRUE(factory) << factory.error().what();
  auto created = (*factory)->create();
  ASSERT_TRUE(created) << created.error().what();
  auto& player = **created;
  auto const player_species = player.species();
  String const ship_species = player_species == "avian" ? String("human") : String("avian");
  player.set_ship_species(ship_species);
  Json const nested_override = JsonObject{{"text", "nested"}, {"senderName", "nested sender"}};
  Json const ship_override = JsonObject{{"text", "ship"},
      {"speciesMessage", JsonObject{{player_species, nested_override}}}};
  Json const config = JsonObject{{"messageId", "radio-override"}, {"type", "generic"}, {"unique", false},
      {"text", "base"}, {"portraitImage", ""},
      {"speciesMessage", JsonObject{{player_species, JsonObject{{"text", "player"}}}}},
      {"speciesAiMessage", JsonObject{{ship_species, ship_override}}}};

  ASSERT_TRUE(player.queue_radio_message(config));
  auto next = player.pull_pending_radio_message();
  ASSERT_TRUE(next && *next);
  EXPECT_EQ((**next).message_id, "radio-override");
  EXPECT_EQ((**next).text, "nested");
  EXPECT_EQ((**next).sender_name, "nested sender");

  player.set_ship_species(player_species);
  ASSERT_TRUE(player.queue_radio_message(config));
  next = player.pull_pending_radio_message();
  ASSERT_TRUE(next && *next);
  EXPECT_EQ((**next).message_id, "radio-override");
  EXPECT_EQ((**next).text, "player");
});

const GameTestRegistration hudWorldViews("HudDomainTest", "WorldPortraitDeltasFollowNativeEntityIdentity", [](GameTest& fixture) {
  auto& root = fixture.root();
  auto storage = File::ephemeral_file(root.random());
  ASSERT_TRUE(storage) << storage.error().what();
  auto world = WorldServer::create(root, Vec2U(2048, 2048), *storage);
  ASSERT_TRUE(world) << world.error().what();
  auto close = make_scope_exit([&] { EXPECT_TRUE((*world)->close()); });
  auto database = root.npc_database();
  ASSERT_TRUE(database) << database.error().what();
  auto variant = (*database)->generate_npc_variant("human", "villager", 1.0f, 1234, JsonObject());
  ASSERT_TRUE(variant) << variant.error().what();
  auto first = (*database)->create_npc(*variant);
  auto second = (*database)->create_npc(*variant);
  ASSERT_TRUE(first) << first.error().what();
  ASSERT_TRUE(second) << second.error().what();
  (*first)->set_position(Vec2F(2046, 1024));
  (*second)->set_position(Vec2F(100, 1024));
  ASSERT_TRUE((*world)->addEntity(*first));
  ASSERT_TRUE((*world)->addEntity(*second));
  auto luaRoot = LuaRoot::create(root);
  ASSERT_TRUE(luaRoot) << luaRoot.error().what();
  auto context = (*luaRoot)->create_context(StringList{});
  ASSERT_TRUE(context) << context.error().what();
  auto callbacks = LuaBindings::makeWorldCallbacks(root, **world);
  ASSERT_TRUE(callbacks) << callbacks.error().what();
  ASSERT_TRUE(context->set_callbacks("world", *callbacks));
  ASSERT_TRUE(context->set("firstId", (*first)->entityId()));
  ASSERT_TRUE(context->set("secondId", (*second)->entityId()));
  auto discovery = context->eval<bool>(R"LUA(
    local all = world.damageBarEntities()
    local ids = {}
    for _, entry in ipairs(all) do ids[entry.id] = true end
    assert(ids[firstId] and ids[secondId])
    local wrapped = world.damageBarEntities({-4, 1000, 4, 1048}, "Default")
    assert(#wrapped == 1 and wrapped[1].id == firstId)
    assert(#world.damageBarEntities(nil, "Special") == 0)
    assert(#world.damageBarEntities({500, 1000, 510, 1048}) == 0)
    return true
  )LUA");
  ASSERT_TRUE(discovery) << discovery.error().what();
  EXPECT_TRUE(*discovery);
  auto initial = context->eval<bool>(R"LUA(
    initial = world.entityPortraitView(firstId, "full")
    assert(initial and #initial.drawables > 0 and initial.bounds)
    local delta = world.entityPortraitView(firstId, "full", initial.revision)
    return delta.revision == initial.revision and delta.drawables == nil and delta.bounds == nil
  )LUA");
  ASSERT_TRUE(initial) << initial.error().what();
  EXPECT_TRUE(*initial);
  ASSERT_TRUE((*first)->status_controller().setResourcePercentage("health", 0.25f));
  auto health = context->eval<bool>(R"LUA(
    local bar = world.entityDamageBar(firstId)
    assert(bar.maxHealth > 0 and math.abs(bar.health / bar.maxHealth - 0.25) < 0.00001)
    local delta = world.entityPortraitView(firstId, "full", initial.revision)
    return delta.revision == initial.revision and delta.drawables == nil
  )LUA");
  ASSERT_TRUE(health) << health.error().what();
  EXPECT_TRUE(*health);
  ASSERT_TRUE((*first)->set_body_directives("?multiply=ff0000"));
  auto changed = context->eval<bool>(R"LUA(
    changed = world.entityPortraitView(firstId, "full", initial.revision)
    assert(changed.revision ~= initial.revision and changed.drawables and changed.bounds)
    local stable = world.entityPortraitView(firstId, "full", changed.revision)
    assert(stable.revision == changed.revision and stable.drawables == nil)
    local head = world.entityPortraitView(firstId, "head", changed.revision)
    assert(head.revision ~= changed.revision and head.drawables and head.bounds)
    local full = world.entityPortraitView(firstId, "full", head.revision)
    assert(full.revision ~= head.revision and full.drawables)
    other = world.entityPortraitView(secondId, "full", full.revision)
    return other.revision ~= full.revision and other.drawables ~= nil
  )LUA");
  ASSERT_TRUE(changed) << changed.error().what();
  EXPECT_TRUE(*changed);
  auto firstId = (*first)->entityId();
  ASSERT_TRUE((*world)->removeEntity(firstId, false));
  auto missing = context->eval<bool>(
      "return world.entityDamageBar(firstId) == nil and world.entityPortraitView(firstId, 'full') == nil");
  ASSERT_TRUE(missing) << missing.error().what();
  EXPECT_TRUE(*missing);
  auto replacement = (*database)->create_npc(std::move(*variant));
  ASSERT_TRUE(replacement) << replacement.error().what();
  (*replacement)->set_position(Vec2F(2046, 1024));
  ASSERT_TRUE((*world)->addEntity(*replacement, firstId));
  // The two allocations have identical authored portraits; switching providers
  // must still invalidate the token, including a reused entity ID.
  auto replaced = context->eval<bool>(R"LUA(
    local view = world.entityPortraitView(firstId, "full", other.revision)
    assert(view.revision ~= other.revision and view.drawables and view.bounds)
    local stable = world.entityPortraitView(firstId, "full", view.revision)
    return stable.revision == view.revision and stable.drawables == nil and stable.bounds == nil
  )LUA");
  ASSERT_TRUE(replaced) << replaced.error().what();
  EXPECT_TRUE(*replaced);
});

}

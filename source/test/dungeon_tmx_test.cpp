import std;
import star.algorithm;
import star.biome;
import star.collision_block;
import star.file;
import star.format;
import star.game_test;
import star.game_types;
import star.json;
import star.list;
import star.material_database;
import star.material_types;
import star.ordered_map;
import star.rect;
import star.root;
import star.string;
import star.vector;
import star.dungeon_generator;
import star.dungeon_tmx_part;
import star.tileset_database;
import star.world_generation;
import star.world_layout;
import star.world_server;
import star.world_template;
import star.world_tiles;

#include "gtest/gtest.h"

namespace {
using namespace Star;
using namespace Star::Dungeon;

TEST(DungeonTmxLayerTest, ChecksFiniteLayerBoundsAndExactPayloadLength) {
  auto layer = [](std::int64_t w, std::int64_t h, std::int64_t x, std::int64_t y, Json data) {
    return TMXTileLayer::create(JsonObject{{"name", "front"}, {"width", w}, {"height", h}, {"x", x}, {"y", y}, {"data", std::move(data)}});
  };
  constexpr auto last = std::numeric_limits<int>::max();
  EXPECT_TRUE(layer(1, 1, last, last, JsonArray{0}));
  EXPECT_FALSE(layer(2, 1, last, 0, JsonArray{0, 0}));
  EXPECT_FALSE(layer(1, 2, 0, last, JsonArray{0, 0}));
  EXPECT_FALSE(layer(1, 1, 0, static_cast<std::int64_t>(last) + 1, JsonArray{0}));
  EXPECT_FALSE(layer(-1, 1, 0, 0, JsonArray{}));
  EXPECT_FALSE(layer(0, 3, 0, 0, JsonArray{}));
  EXPECT_FALSE(layer(65536, 65536, 0, 0, JsonArray{}));
  EXPECT_FALSE(layer(1, 1, 0, 0, JsonArray{0, 0}));
  EXPECT_FALSE(layer(1, 1, 0, 0, JsonArray{std::int64_t{1} << 32}));
  for (String const& data : {String("AAAA"), String("AQAA4CE="), String("AQAA4CEh"), String("AQAA4CEhIQ==")})
    EXPECT_FALSE(TMXTileLayer::create(JsonObject{{"name", "front"}, {"width", 1}, {"height", 1}, {"compression", ""}, {"data", data}}));
}

const GameTestRegistration tmx_reader("DungeonTmxTest", "OrderedObjectCoordinatesCodecsAndMultiMapStop", [](GameTest& fixture) {
  auto dir = File::temporary_directory(fixture.root().random());
  ASSERT_TRUE(dir);
  auto cleanup = make_scope_exit([&] { EXPECT_TRUE(File::remove_all(*dir)); });
  auto overlay = File::relative_to(*dir, "assets");
  ASSERT_TRUE(File::create_directory(overlay));
  auto write_asset = [&](String const& name, String const& text) { return File::write_file(text, File::relative_to(overlay, name)); };
  ASSERT_TRUE(write_asset("preload.config", "[]"));
  ASSERT_TRUE(write_asset("tileset.json", R"({"tilecount":1,"tileproperties":{"0":{"connector":"layer"}}})"));
  String first = R"({"width":9,"height":12,"tilewidth":8,"tileheight":8,"tilesets":[{"source":"/tileset.json","firstgid":1}],"layers":[
    {"type":"tilelayer","name":"front","x":2,"y":1,"width":1,"height":1,"data":[3758096385]},
    {"type":"tilelayer","name":"front","x":2,"y":1,"width":1,"height":1,"compression":"","data":"AQAA4A=="},
    {"type":"tilelayer","name":"front","x":2,"y":1,"width":1,"height":1,"compression":"gzip","data":"H4sIAAAAAAAC/2NkYHgAAAFa8jkEAAAA"},
    {"type":"tilelayer","name":"front","x":2,"y":1,"width":1,"height":1,"compression":"zlib","data":"eJxjZGB4AAAA6ADi"},
    {"type":"tilelayer","name":"front","x":2,"y":1,"width":1,"height":1,"compression":"zstd","data":"KLUv/SQEIQAAAQAA4Hp2i2s="},
    {"type":"objectgroup","name":"objects","objects":[
      {"id":1,"x":16,"y":16,"gid":1},
      {"id":2,"x":8,"y":8,"width":16,"height":24,"properties":{"stagehand":"test","connector":"odd"}},
      {"id":3,"x":8,"y":16,"width":16,"height":16,"properties":{"stagehand":"test","connector":"even"}},
      {"id":4,"x":16,"y":8,"width":16,"height":16,"properties":{"imagePositionX":"8","connector":"rect"}},
      {"id":5,"x":16,"y":8,"polyline":[{"x":0,"y":0},{"x":8,"y":0},{"x":0,"y":0}],"properties":{"connector":"wire"}}
    ]}]})";
  ASSERT_TRUE(write_asset("first.json", first));
  ASSERT_TRUE(write_asset("second.json", R"({"width":9,"height":12,"tilewidth":8,"tileheight":8,"tilesets":[],"layers":[
    {"type":"tilelayer","name":"front","x":2,"y":1,"width":3,"height":1,"data":[9,9,9]}]})"));
  auto settings = fixture.rootSettings();
  settings.assetDirectories.clear();
  settings.assetSources = {overlay};
  settings.includeUGC = false;
  settings.storageDirectory = File::relative_to(*dir, "storage");
  settings.runtimeConfigFile = {};
  settings.logDirectory = {};
  settings.logFile = {};
  auto root = Root::create(std::move(settings));
  ASSERT_TRUE(root) << root.error().what();
  TMXPartReader reader(**root);
  auto loaded = reader.readAsset("/first.json");
  ASSERT_TRUE(loaded) << loaded.error().what();
  using Event = std::pair<Vec2I, String>;
  std::vector<Event> events;
  auto collect = [&](Vec2I pos, Tile const& tile) {
    events.emplace_back(pos, tile.connector.value().value);
    return false;
  };
  auto traversal = reader.forEachTile(collect);
  ASSERT_TRUE(traversal) << traversal.error().what();
  std::vector<Event> full(6, Event{Vec2I(2, 10), "layer"}); // Five layers and the bottom-anchored tile object.
  full.insert(full.end(), {{{2,9},"odd"}, {{2,9},"even"}, {{1,10},"rect"}, {{1,9},"rect"}, {{2,10},"rect"}, {{2,9},"rect"},
                           {{2,10},"wire"}, {{3,10},"wire"}, {{2,10},"wire"}});
  ASSERT_EQ(events, full);
  for (int y = -1; y <= 12; ++y) {
    for (int x = -1; x <= 9; ++x) {
      Vec2I pos(x, y);
      events.clear();
      ASSERT_TRUE(reader.forEachTileAt(pos, collect));
      std::vector<Event> filtered;
      std::ranges::copy_if(full, std::back_inserter(filtered), [&](Event const& event) { return event.first == pos; });
      EXPECT_EQ(events, filtered);
    }
  }
  ASSERT_TRUE(reader.readAsset("/second.json"));
  unsigned calls = 0;
  auto stop = [&](Vec2I, Tile const&) { ++calls; return true; };
  EXPECT_TRUE(reader.forEachTile(stop));
  EXPECT_EQ(calls, 1u);
  EXPECT_TRUE(reader.forEachTileAt(Vec2I(2, 10), stop));
  EXPECT_EQ(calls, 2u);
  EXPECT_FALSE(reader.forEachTile(collect));
  EXPECT_FALSE(reader.forEachTileAt(Vec2I(2, 10), collect));
  EXPECT_FALSE(reader.forEachTileAt(Vec2I(4, 10), stop)); // First map misses altogether.
  EXPECT_EQ(calls, 2u);
});

const GameTestRegistration brush_boundaries("DungeonTmxTest", "RejectInvalidBrushMaterialFieldsBeforePainting", [](GameTest& fixture) {
  auto& root = fixture.root();
  for (auto const& kind : std::array<String, 2>{"surface", "surfacebackground"}) {
    EXPECT_TRUE(Brush::parse(root, JsonArray{kind}));
    for (int variant = 0; variant <= 5; ++variant)
      EXPECT_TRUE(Brush::parse(root, JsonArray{kind, JsonObject{{"variant", variant}}}));
    for (auto variant : std::array<Json, 5>{-1, 6, 5.5, -0.5, std::int64_t{1} << 32})
      EXPECT_FALSE(Brush::parse(root, JsonArray{kind, JsonObject{{"variant", variant}}}));
  }
  for (auto layer : {TileLayer::Foreground, TileLayer::Background}) {
    auto tiled = [&](Json properties) { return Tiled::Tile::create(root, Tiled::Properties(properties), layer); };
    for (auto const& value : std::array<String, 7>{"", "0", "1", "2", "3", "4", "5"})
      EXPECT_TRUE(tiled(JsonObject{{"surface", value}}));
    for (auto const& value : std::array<String, 5>{"-1", "6", "5.5", "4294967296", "9223372036854775808"})
      EXPECT_FALSE(tiled(JsonObject{{"surface", value}}));
    for (auto const& kind : std::array<String, 3>{"material", "front", "back"}) {
      EXPECT_TRUE(tiled(JsonObject{{kind, "dirt"}, {"hueshift", "-45"}, {"modhueshift", "720"}, {"colorVariant", "255"}}));
      for (auto const& value : std::array<String, 4>{"-1", "256", "9223372036854775808", "18446744073709551616"})
        EXPECT_FALSE(tiled(JsonObject{{kind, "dirt"}, {"colorVariant", value}}));
      EXPECT_FALSE(tiled(JsonObject{{kind, "dirt"}, {"hueshift", "inf"}}));
      EXPECT_FALSE(tiled(JsonObject{{kind, "dirt"}, {"modhueshift", "nan"}}));
    }
  }
});

const GameTestRegistration surface_layers("DungeonTmxTest", "PaintAndResolveSurfaceLayersWithoutChangingOverrides", [](GameTest& fixture) {
  auto& root = fixture.root();
  auto materials = root.material_database();
  ASSERT_TRUE(materials) << materials.error().what();
  auto dirt = (*materials)->material_id("dirt");
  auto sand = (*materials)->material_id("sand");
  auto grass = (*materials)->mod_id("grass");
  ASSERT_TRUE(dirt && sand && grass);
  auto storage = File::ephemeral_file(root.random());
  ASSERT_TRUE(storage) << storage.error().what();
  auto world = WorldServer::create(root, Vec2U(128, 128), *storage);
  ASSERT_TRUE(world) << world.error().what();
  auto close_world = make_scope_exit([&] { EXPECT_TRUE((*world)->close()); });
  ASSERT_TRUE((*world)->generateRegion(RectI::with_size({120, 12}, {10, 6})));

  auto facade = std::make_shared<DungeonGeneratorWorld>(**world, false);
  DungeonGeneratorWriter writer(root, std::move(facade), {}, {});
  constexpr std::array placeholders{biome_material_id, biome1_material_id, biome2_material_id,
      biome3_material_id, biome4_material_id, biome5_material_id};
  for (int variant = 0; variant < static_cast<int>(placeholders.size()); ++variant) {
    auto front = variant == 0 ? Brush::parse(root, JsonArray{"surface"})
                              : Brush::parse(root, JsonArray{"surface", JsonObject{{"variant", variant}}});
    ASSERT_TRUE(front) << front.error().what();
    JsonObject back_properties{{"clear", "false"}, {"surface", format("{}", 5 - variant)}};
    if (variant % 2 == 0)
      back_properties["mod"] = "grass";
    auto back = Tiled::Tile::create(root, Tiled::Properties(back_properties), TileLayer::Background);
    ASSERT_TRUE(back) << back.error().what();
    Vec2I pos(variant - 6, 14); // Flush through the world seam.
    ASSERT_TRUE((*front)->paint(pos, Phase::WallPhase, writer));
    ASSERT_TRUE((*front)->paint(pos, Phase::ModsPhase, writer));
    ASSERT_TRUE((*back)->place(pos, Phase::WallPhase, writer));
    ASSERT_TRUE((*back)->place(pos, Phase::ModsPhase, writer));
  }

  auto tinted = Tiled::Tile::create(root, Tiled::Properties(JsonObject{
      {"front", "dirt"}, {"hueshift", "-45"}, {"mod", "grass"}, {"modhueshift", "90"},
      {"colorVariant", "7"}}), TileLayer::Foreground);
  ASSERT_TRUE(tinted) << tinted.error().what();
  Vec2I tinted_pos(122, 15);
  ASSERT_TRUE((*tinted)->place(tinted_pos, Phase::WallPhase, writer));
  ASSERT_TRUE(writer.flush());

  Biome with_sub_blocks;
  with_sub_blocks.main_block = *dirt;
  with_sub_blocks.material_hue_shift = 191;
  with_sub_blocks.sub_blocks = {empty_material_id, *sand};
  auto with_sub_blocks_json = with_sub_blocks.to_json();
  ASSERT_TRUE(with_sub_blocks_json) << with_sub_blocks_json.error().what();
  with_sub_blocks.sub_blocks.clear();
  auto without_sub_blocks_json = with_sub_blocks.to_json();
  ASSERT_TRUE(without_sub_blocks_json) << without_sub_blocks_json.error().what();
  auto layout = WorldLayout::create(root, JsonObject{
      {"worldSize", JsonArray{128, 128}}, {"biomes", JsonArray{*with_sub_blocks_json, *without_sub_blocks_json}},
      {"terrainSelectors", JsonArray{}}, {"layers", JsonArray{}}, {"regionBlending", 0}, {"playerStartSearchRegions", JsonArray{}}});
  ASSERT_TRUE(layout) << layout.error().what();
  (*world)->worldTemplate()->setWorldLayout(*layout);
  auto clear_layout = make_scope_exit([&] { (*world)->worldTemplate()->setWorldLayout({}); });
  WorldGenerator generator(**world);

  auto tinted_tile = (*world)->getServerTile(tinted_pos);
  ASSERT_TRUE(tinted_tile) << tinted_tile.error().what();
  EXPECT_EQ(tinted_tile->get().foreground, *dirt);
  EXPECT_EQ(tinted_tile->get().foregroundHueShift, 225);
  EXPECT_EQ(tinted_tile->get().foregroundColorVariant, 7);
  EXPECT_EQ(tinted_tile->get().foregroundMod, *grass);
  EXPECT_EQ(tinted_tile->get().foregroundModHueShift, 63);

  std::array<MaterialId, 6> expected{*dirt, empty_material_id, *sand, empty_material_id, *sand, empty_material_id};
  for (std::size_t index = 0; index < placeholders.size(); ++index) {
    SCOPED_TRACE(index);
    auto painted = (*world)->getServerTile(Vec2I(122 + static_cast<int>(index), 14));
    ASSERT_TRUE(painted) << painted.error().what();
    auto original = painted->get();
    auto reverse = placeholders.size() - 1 - index;
    auto const background_mod = index % 2 == 0 ? *grass : biome_mod_id;
    EXPECT_EQ(original.foreground, placeholders[index]);
    EXPECT_EQ(original.background, placeholders[reverse]);
    EXPECT_EQ(original.foregroundHueShift, 0);
    EXPECT_EQ(original.backgroundHueShift, 0);
    EXPECT_EQ(original.foregroundMod, biome_mod_id);
    EXPECT_EQ(original.backgroundMod, background_mod);

    original.foregroundHueShift = 12;
    original.backgroundHueShift = 24;
    original.foregroundModHueShift = 31;
    original.backgroundModHueShift = 42;
    auto tile = original;
    tile.blockBiomeIndex = 1;
    ASSERT_TRUE(generator.replaceBiomeBlocks(tile));
    EXPECT_EQ(tile.foreground, expected[index]);
    EXPECT_EQ(tile.background, expected[reverse]);
    EXPECT_EQ(tile.foregroundHueShift, index == 0 ? 191 : 0);
    EXPECT_EQ(tile.backgroundHueShift, reverse == 0 ? 191 : 0);
    EXPECT_EQ(tile.foregroundMod, biome_mod_id);
    EXPECT_EQ(tile.backgroundMod, background_mod);
    EXPECT_EQ(tile.foregroundModHueShift, 31);
    EXPECT_EQ(tile.backgroundModHueShift, 42);
    EXPECT_EQ(tile.collision, expected[index] == empty_material_id ? CollisionKind::None : CollisionKind::Block);

    tile = original;
    tile.blockBiomeIndex = 2;
    tile.update_collision(CollisionKind::Platform);
    ASSERT_TRUE(generator.replaceBiomeBlocks(tile));
    EXPECT_EQ(tile.foreground, *dirt);
    EXPECT_EQ(tile.background, *dirt);
    EXPECT_EQ(tile.foregroundHueShift, 191);
    EXPECT_EQ(tile.backgroundHueShift, 191);
    EXPECT_EQ(tile.collision, CollisionKind::Platform);

    tile = original;
    tile.blockBiomeIndex = NullBiomeIndex;
    ASSERT_TRUE(generator.replaceBiomeBlocks(tile));
    EXPECT_EQ(tile.foreground, empty_material_id);
    EXPECT_EQ(tile.background, empty_material_id);
    EXPECT_EQ(tile.foregroundHueShift, 0);
    EXPECT_EQ(tile.backgroundHueShift, 0);
    EXPECT_EQ(tile.foregroundMod, no_mod_id);
    EXPECT_EQ(tile.foregroundModHueShift, 0);
    EXPECT_EQ(tile.backgroundMod, index % 2 == 0 ? *grass : no_mod_id);
    EXPECT_EQ(tile.backgroundModHueShift, index % 2 == 0 ? 42 : 0);
    EXPECT_EQ(tile.collision, CollisionKind::None);
  }
});
}

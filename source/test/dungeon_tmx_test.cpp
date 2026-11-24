import std;
import star.algorithm;
import star.file;
import star.format;
import star.game_test;
import star.json;
import star.list;
import star.ordered_map;
import star.root;
import star.string;
import star.vector;
import star.dungeon_generator;
import star.dungeon_tmx_part;

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
}

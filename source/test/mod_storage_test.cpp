#include "gtest/gtest.h"

import std;
import star.algorithm;
import star.assets;
import star.encode;
import star.file;
import star.format;
import star.game_test;
import star.json;
import star.io_device;
import star.logging;
import star.lua;
import star.lua_converters;
import star.lua_root;
import star.mod_storage;
import star.random;
import star.root;
import star.root_lua_bindings;
import star.sha256;
import star.string;
import star.thread;

using namespace Star;

namespace {

String storagePath(String const& directory, String const& mod) {
  return File::relative_to(directory, "mod-" + hex_encode(sha256(mod)) + ".json");
}

std::expected<Json, StarException> storedObject(String const& directory, String const& mod) {
  auto bytes = File::read_file_string(storagePath(directory, mod));
  if (!bytes)
    return std::unexpected(std::move(bytes.error()));
  return Json::parse_json(*bytes);
}

const GameTestRegistration jsonPersistence("ModStorageTest", "JsonKeysAndNamespacesRoundTrip", [](GameTest& fixture) {
  auto directory = File::temporary_directory(fixture.root().random());
  ASSERT_TRUE(directory);
  auto cleanup = make_scope_exit([&] { EXPECT_TRUE(File::remove_all(*directory)); });
  Logger logger;
  auto opened = ModStorage::create(logger, *directory);
  ASSERT_TRUE(opened) << opened.error().what();
  auto storage = std::move(*opened);
  String key(std::string_view("quote\"\\\n\0tail", 13));
  Json value(JsonObject{{key, JsonArray{JsonObject{{key, "中文"}}, JsonArray{}, true, Json()}}});
  std::vector<String> ids{"CON", "Case", "case", "../escape", "", String(std::string(400, 'x'))};
  for (std::size_t i = 0; i != ids.size(); ++i) {
    ASSERT_TRUE(storage->set(ids[i], key, value));
    ASSERT_TRUE(storage->set(ids[i], "index", Json(i)));
  }
  ASSERT_TRUE(storage->flush());
  storage.reset();
  auto reloaded = ModStorage::create(logger, *directory);
  ASSERT_TRUE(reloaded);
  for (std::size_t i = 0; i != ids.size(); ++i) {
    EXPECT_EQ((*reloaded)->get(ids[i], key), value);
    EXPECT_EQ((*reloaded)->get(ids[i], "index"), Json(i));
    auto disk = storedObject(*directory, ids[i]);
    ASSERT_TRUE(disk) << disk.error().what();
    EXPECT_EQ(disk->get(key), value);
  }
  ASSERT_TRUE((*reloaded)->set("CON", key, Json()));
  EXPECT_EQ((*reloaded)->get("CON", key), Json());
  auto keys = (*reloaded)->keys("CON");
  ASSERT_TRUE(keys);
  ASSERT_EQ(keys->size(), 1u);
  EXPECT_EQ((*keys)[0], Json("index"));
  ASSERT_TRUE((*reloaded)->clear("CON"));
  ASSERT_TRUE((*reloaded)->flush());
  EXPECT_EQ(storedObject(*directory, "CON"), Json(JsonObject{}));
});

const GameTestRegistration exactQuota("ModStorageTest", "ExactQuotaRejectsWithoutChangingData", [](GameTest& fixture) {
  auto directory = File::temporary_directory(fixture.root().random());
  ASSERT_TRUE(directory);
  auto cleanup = make_scope_exit([&] { EXPECT_TRUE(File::remove_all(*directory)); });
  Logger logger;
  auto storage = ModStorage::create(logger, *directory);
  ASSERT_TRUE(storage);
  Json boundary(String(std::string(ModStorage::MaxModDataSize - 8, 'x')));
  ASSERT_TRUE((*storage)->set("quota", "k", boundary));
  EXPECT_FALSE((*storage)->set("quota", "extra", 1));
  EXPECT_FALSE((*storage)->set("quota", "k", String(std::string(ModStorage::MaxModDataSize - 7, 'x'))));
  EXPECT_EQ((*storage)->get("quota", "k"), boundary);
  EXPECT_EQ((*storage)->get("quota", "extra"), Json());
  ASSERT_TRUE((*storage)->flush());
  auto file = File::open(storagePath(*directory, "quota"), IOMode::Read);
  ASSERT_TRUE(file);
  EXPECT_EQ((*file)->size(), ModStorage::MaxModDataSize);
  ASSERT_TRUE((*file)->close());
  ASSERT_TRUE((*storage)->set("quota", "k", Json()));
  ASSERT_TRUE((*storage)->set("quota", "quote\"", 2));
  ASSERT_TRUE((*storage)->flush());
  auto disk = storedObject(*directory, "quota");
  ASSERT_TRUE(disk);
  EXPECT_EQ(*disk, Json(JsonObject{{"quote\"", 2}}));
});

const GameTestRegistration writeRetry("ModStorageTest", "FailedFlushRetainsDataAndRetriesIdenticalValue", [](GameTest& fixture) {
  auto directory = File::temporary_directory(fixture.root().random());
  ASSERT_TRUE(directory);
  auto cleanup = make_scope_exit([&] { EXPECT_TRUE(File::remove_all(*directory)); });
  Logger logger;
  auto storage = ModStorage::create(logger, *directory);
  ASSERT_TRUE(storage);
  ASSERT_TRUE((*storage)->set("retry", "value", 1));
  ASSERT_TRUE((*storage)->flush());
  auto stage = storagePath(*directory, "retry") + ".new";
  ASSERT_TRUE(File::write_file(String("owned collision"), stage));
  ASSERT_TRUE((*storage)->set("retry", "value", 2));
  EXPECT_FALSE((*storage)->flush());
  EXPECT_EQ((*storage)->get("retry", "value"), Json(2));
  auto previous = storedObject(*directory, "retry");
  ASSERT_TRUE(previous);
  EXPECT_EQ(previous->get("value"), Json(1));
  EXPECT_EQ(File::read_file_string(stage), String("owned collision"));
  ASSERT_TRUE(File::remove(stage));
  ASSERT_TRUE((*storage)->set("retry", "value", 2));
  ASSERT_TRUE((*storage)->flush());
  auto current = storedObject(*directory, "retry");
  ASSERT_TRUE(current);
  EXPECT_EQ(current->get("value"), Json(2));
});

const GameTestRegistration loadRecovery("ModStorageTest", "InvalidFilesRemainUntouchedAndCanBeRepaired", [](GameTest& fixture) {
  auto directory = File::temporary_directory(fixture.root().random());
  ASSERT_TRUE(directory);
  auto cleanup = make_scope_exit([&] { EXPECT_TRUE(File::remove_all(*directory)); });
  Logger logger;
  auto storage = ModStorage::create(logger, *directory);
  ASSERT_TRUE(storage);
  auto path = storagePath(*directory, "broken");
  ASSERT_TRUE(File::write_file(String("invalid JSON"), path));
  EXPECT_FALSE((*storage)->get("broken", "value"));
  EXPECT_FALSE((*storage)->set("broken", "value", 2));
  EXPECT_FALSE((*storage)->clear("broken"));
  ASSERT_TRUE((*storage)->flush());
  EXPECT_EQ(File::read_file_string(path), String("invalid JSON"));
  ASSERT_TRUE(File::write_file(String("{\"value\":1}"), path));
  EXPECT_EQ((*storage)->get("broken", "value"), Json(1));
  auto oversized = storagePath(*directory, "oversized");
  ASSERT_TRUE(File::write_file(String(std::string(ModStorage::MaxModDataSize + 1, ' ')), oversized));
  EXPECT_FALSE((*storage)->get("oversized", "value"));
  ASSERT_TRUE(File::write_file(String("{}"), oversized));
  EXPECT_EQ((*storage)->get("oversized", "value"), Json());
});

const GameTestRegistration finiteAndDepth("ModStorageTest", "InvalidValuesDoNotMutateAndLargeFloatsReload", [](GameTest& fixture) {
  auto directory = File::temporary_directory(fixture.root().random());
  ASSERT_TRUE(directory);
  auto cleanup = make_scope_exit([&] { EXPECT_TRUE(File::remove_all(*directory)); });
  Logger logger;
  auto storage = ModStorage::create(logger, *directory);
  ASSERT_TRUE(storage);
  ASSERT_TRUE((*storage)->set("values", "number", 1.0e20));
  auto nonfinite = std::bit_cast<double>(std::uint64_t{0x7ff0000000000000});
  EXPECT_FALSE((*storage)->set("values", "number", nonfinite));
  EXPECT_EQ((*storage)->get("values", "number"), Json(1.0e20));
  Json deep(1);
  for (unsigned i = 0; i != 257; ++i)
    deep = JsonArray{std::move(deep)};
  EXPECT_FALSE((*storage)->set("values", "deep", deep));
  EXPECT_EQ((*storage)->get("values", "deep"), Json());
  ASSERT_TRUE((*storage)->flush());
  auto reopened = ModStorage::create(logger, *directory);
  ASSERT_TRUE(reopened);
  EXPECT_EQ((*reopened)->get("values", "number"), Json(1.0e20));
});

const GameTestRegistration concurrentFlush("ModStorageTest", "ConcurrentFlushesPersistLatestWrites", [](GameTest& fixture) {
  auto directory = File::temporary_directory(fixture.root().random());
  ASSERT_TRUE(directory);
  auto cleanup = make_scope_exit([&] { EXPECT_TRUE(File::remove_all(*directory)); });
  Logger logger;
  auto storage = ModStorage::create(logger, *directory);
  ASSERT_TRUE(storage);
  std::barrier start(3);
  auto writer = [&](String key) -> std::expected<void, StarException> {
    start.arrive_and_wait();
    for (int i = 0; i != 8; ++i) {
      if (auto changed = (*storage)->set("concurrent", key, i); !changed)
        return std::unexpected(std::move(changed.error()));
      if (auto flushed = (*storage)->flush(); !flushed)
        return std::unexpected(std::move(flushed.error()));
    }
    return {};
  };
  auto first = Thread::invoke("Mod storage writer A", [&] { return writer("a"); });
  ASSERT_TRUE(first);
  auto second = Thread::invoke("Mod storage writer B", [&] { return writer("b"); });
  if (!second) {
    start.arrive_and_drop();
    start.arrive_and_drop();
    ASSERT_TRUE(first->finish());
    FAIL() << second.error().what();
  }
  start.arrive_and_wait();
  ASSERT_TRUE(first->finish());
  ASSERT_TRUE(second->finish());
  auto reopened = ModStorage::create(logger, *directory);
  ASSERT_TRUE(reopened);
  EXPECT_EQ((*reopened)->get("concurrent", "a"), Json(7));
  EXPECT_EQ((*reopened)->get("concurrent", "b"), Json(7));
});

std::expected<Root::Settings, StarException> privateSettings(GameTest& fixture, String const& directory) {
  auto settings = fixture.rootSettings();
  settings.storageDirectory = File::relative_to(directory, "storage");
  settings.logDirectory = {};
  settings.logFile = {};
  settings.runtimeConfigFile = {};
  settings.includeUGC = false;
  auto safe = settings.defaultConfiguration.set("safeScripts", true);
  if (!safe)
    return std::unexpected(std::move(safe.error()));
  settings.defaultConfiguration = std::move(*safe);
  return settings;
}

std::expected<std::shared_ptr<LuaRoot>, StarException> storageLua(Root& root) {
  auto lua = LuaRoot::create(root);
  if (!lua)
    return std::unexpected(std::move(lua.error()));
  auto callbacks = LuaBindings::makeRootCallbacks(root);
  if (!callbacks)
    return std::unexpected(std::move(callbacks.error()));
  (*lua)->addCallbacks("root", *callbacks);
  return std::move(*lua);
}

const GameTestRegistration luaScopes("ModStorageTest", "LuaAssetScopesExplicitNamesReloadAndRestart", [](GameTest& fixture) {
  auto directory = File::temporary_directory(fixture.root().random());
  ASSERT_TRUE(directory);
  auto cleanup = make_scope_exit([&] { EXPECT_TRUE(File::remove_all(*directory)); });
  auto settings = privateSettings(fixture, *directory);
  ASSERT_TRUE(settings);
  for (auto name : {String("StorageA"), String("StorageB")}) {
    auto source = File::relative_to(*directory, name);
    ASSERT_TRUE(File::create_directory(source));
    auto metadata = Json(JsonObject{{"name", name}}).repr();
    ASSERT_TRUE(metadata);
    ASSERT_TRUE(File::write_file(*metadata, File::relative_to(source, "_metadata")));
    String script = name == "StorageA" ? String("storage-a.lua") : String("storage-b.lua");
    ASSERT_TRUE(File::write_file(String(
      "function save(v) root.setModStorage('value', v) end\n"
      "function read() return root.getModStorage('value') end\n"
      "function erase() root.setModStorage('value', nil) end\n"
      "function count() return #root.modStorageKeys() end\n"
      "function clear() root.clearModStorage() end\n"), File::relative_to(source, script)));
    settings->assetSources.push_back(source);
  }
  ASSERT_TRUE(File::write_file(
    String("function saveViaHelper(v) root.setModStorage('helper', v) end\n"),
    File::relative_to(File::relative_to(*directory, "StorageB"), "storage-helper.lua")));
  {
    auto root = Root::create(*settings);
    ASSERT_TRUE(root) << root.error().what();
    auto lua = storageLua(**root);
    ASSERT_TRUE(lua);
    auto a = (*lua)->createContext("/storage-a.lua");
    auto b = (*lua)->createContext("/storage-b.lua");
    ASSERT_TRUE(a);
    ASSERT_TRUE(b);
    ASSERT_TRUE(a->invokePath<void>("save", 11));
    ASSERT_TRUE(b->invokePath<void>("save", 22));
    EXPECT_EQ(a->invokePath<int>("read"), 11);
    EXPECT_EQ(b->invokePath<int>("read"), 22);
    EXPECT_EQ(a->eval<int>("root.getModStorage('value', 'StorageB')"), 22);
    ASSERT_TRUE(a->eval<void>("root.setModStorage('named', 33, 'StorageB')"));
    EXPECT_EQ(b->eval<int>("root.getModStorage('named', 'StorageB')"), 33);
    ASSERT_TRUE(a->eval<void>("require '/storage-helper.lua'; saveViaHelper(66)"));
    EXPECT_EQ(b->eval<int>("root.getModStorage('helper', 'StorageB')"), 66);
    EXPECT_EQ(a->eval<Json>("root.getModStorage('helper', 'StorageA')"), Json());
    ASSERT_TRUE(a->eval<void>(
      "root.setModStorage('table', {n = 1}, 'StorageA'); "
      "local copy = root.getModStorage('table', 'StorageA'); copy.n = 9; "
      "assert(root.getModStorage('table', 'StorageA').n == 1); "
      "root.setModStorage('table', nil, 'StorageA')"));
    EXPECT_FALSE(a->eval<Json>("root.getModStorage('value')"));
    EXPECT_FALSE(a->eval<Json>("loadstring(\"return root.getModStorage('value')\")()"));
    EXPECT_FALSE(a->eval<Json>("root.getModStorage('value', 'unknown-storage-mod')"));
    EXPECT_EQ(a->eval<bool>("io == nil and debug == nil"), true);
    EXPECT_EQ(a->invokePath<int>("count"), 1);
    ASSERT_TRUE(a->invokePath<void>("erase"));
    EXPECT_EQ(a->invokePath<Json>("read"), Json());
    ASSERT_TRUE(a->invokePath<void>("save", 44));
    auto before = (*root)->modStorage();
    ASSERT_TRUE(before);
    ASSERT_TRUE((*root)->reload());
    auto after = (*root)->modStorage();
    ASSERT_TRUE(after);
    EXPECT_EQ(&before->get(), &after->get());
    EXPECT_EQ(a->invokePath<int>("read"), 44);
    auto current = (*lua)->createContext("/storage-a.lua");
    ASSERT_TRUE(current);
    EXPECT_EQ(current->invokePath<int>("read"), 44);
    ASSERT_TRUE(b->invokePath<void>("clear"));
    EXPECT_EQ(b->invokePath<int>("count"), 0);
    ASSERT_TRUE(a->eval<void>("root.flushModStorage()"));
    ASSERT_TRUE(a->invokePath<void>("save", 55));
    // The final write deliberately relies on Root/service shutdown, not flush.
  }
  auto root = Root::create(*settings);
  ASSERT_TRUE(root);
  auto lua = storageLua(**root);
  ASSERT_TRUE(lua);
  auto a = (*lua)->createContext("/storage-a.lua");
  auto b = (*lua)->createContext("/storage-b.lua");
  ASSERT_TRUE(a);
  ASSERT_TRUE(b);
  EXPECT_EQ(a->invokePath<int>("read"), 55);
  EXPECT_EQ(b->invokePath<int>("count"), 0);
});

const GameTestRegistration sourceCollisions("ModStorageTest", "DuplicateSourcesAndRootsDoNotShareNamespaces", [](GameTest& fixture) {
  auto directory = File::temporary_directory(fixture.root().random());
  ASSERT_TRUE(directory);
  auto cleanup = make_scope_exit([&] { EXPECT_TRUE(File::remove_all(*directory)); });
  auto settings = privateSettings(fixture, *directory);
  ASSERT_TRUE(settings);
  std::vector<String> sources;
  for (unsigned i = 0; i != 4; ++i) {
    auto parent = File::relative_to(*directory, Star::format("parent{}", i));
    ASSERT_TRUE(File::create_directory(parent));
    auto source = File::relative_to(parent, "same");
    ASSERT_TRUE(File::create_directory(source));
    if (i == 3) {
      auto metadata = Json(JsonObject{{"name", "same (same)"}}).repr();
      ASSERT_TRUE(metadata);
      ASSERT_TRUE(File::write_file(*metadata, File::relative_to(source, "_metadata")));
    }
    settings->assetSources.push_back(source);
    sources.push_back(source);
  }
  auto root = Root::create(*settings);
  ASSERT_TRUE(root);
  auto assets = (*root)->assets();
  ASSERT_TRUE(assets);
  std::vector<String> ids;
  auto storage = (*root)->modStorage();
  ASSERT_TRUE(storage);
  for (std::size_t i = 0; i != sources.size(); ++i) {
    auto id = (*root)->modStorageId(**assets, sources[i]);
    ASSERT_TRUE(id);
    for (auto const& previous : ids)
      EXPECT_NE(*id, previous);
    ASSERT_TRUE(storage->get().set(*id, "value", Json(i)));
    ids.push_back(*id);
  }
  for (std::size_t i = 0; i != ids.size(); ++i)
    EXPECT_EQ(storage->get().get(ids[i], "value"), Json(i));
  auto secondSettings = *settings;
  secondSettings.storageDirectory = File::relative_to(*directory, "other-storage");
  ASSERT_TRUE(File::write_file(String("{\"name\":\"NewOwner\"}"), File::relative_to(sources[0], "_metadata")));
  auto secondRoot = Root::create(secondSettings);
  ASSERT_TRUE(secondRoot);
  auto second = (*secondRoot)->modStorage();
  ASSERT_TRUE(second);
  EXPECT_EQ(second->get().get(ids[0], "value"), Json());
  auto secondAssets = (*secondRoot)->assets();
  ASSERT_TRUE(secondAssets);
  auto secondId = (*secondRoot)->modStorageId(**secondAssets, sources[0]);
  ASSERT_TRUE(secondId);
  EXPECT_EQ(*secondId, String("NewOwner"));
  EXPECT_NE(*secondId, ids[0]);
  EXPECT_EQ((*root)->modStorageId(**assets, sources[0]), ids[0]);
});

}

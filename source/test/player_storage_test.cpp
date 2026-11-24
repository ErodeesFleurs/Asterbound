#include "gtest/gtest.h"
#include "gtest/gtest-spi.h"

import std;
import star.algorithm;
import star.byte_array;
import star.codex;
import star.configuration;
import star.file;
import star.format;
import star.game_test;
import star.json;
import star.list;
import star.player;
import star.player_codexes;
import star.player_factory;
import star.player_storage;
import star.root;
import star.string;
import star.uuid;
import star.versioning_database;

using namespace Star;

namespace {

std::expected<std::shared_ptr<Player>, StarException> create_player(Root& root, String const& name) {
  auto factory = root.playerFactory();
  if (!factory)
    return std::unexpected(std::move(factory.error()));
  auto player = (*factory)->create();
  if (!player)
    return std::unexpected(std::move(player.error()));
  if (auto named = (*player)->setName(name); !named)
    return std::unexpected(std::move(named.error()));
  if (auto finalized = (*player)->finalizeCreation(); !finalized)
    return std::unexpected(std::move(finalized.error()));
  return std::move(*player);
}

std::expected<Json, StarException> read_metadata(String const& directory) {
  auto contents = File::read_file_string(File::relative_to(directory, "metadata"));
  if (!contents)
    return std::unexpected(std::move(contents.error()));
  return Json::parse_json(*contents);
}

struct PlayerStorageContext {
  PlayerStorageContext() = default;
  PlayerStorageContext(PlayerStorageContext const&) = delete;
  PlayerStorageContext& operator=(PlayerStorageContext const&) = delete;

  ~PlayerStorageContext() {
    storage.reset();
    if (!temporary.empty())
      EXPECT_TRUE(File::remove_all(temporary));
    if (configuration)
      EXPECT_TRUE(configuration->set("clearPlayerFiles", clear_files));
  }

  std::expected<void, StarException> init(Root& root) {
    auto configured = root.configuration();
    if (!configured)
      return std::unexpected(std::move(configured.error()));
    auto previous = (*configured)->get("clearPlayerFiles");
    if (!previous)
      return std::unexpected(std::move(previous.error()));
    configuration = std::move(*configured);
    clear_files = std::move(*previous);
    if (auto disabled = configuration->set("clearPlayerFiles", false); !disabled)
      return std::unexpected(std::move(disabled.error()));
    auto created = File::temporary_directory(root.random());
    if (!created)
      return std::unexpected(std::move(created.error()));
    temporary = std::move(*created);
    directory = File::relative_to(temporary, "player");
    auto opened = PlayerStorage::create(root, directory);
    if (!opened)
      return std::unexpected(std::move(opened.error()));
    storage = std::move(*opened);
    return {};
  }

  std::shared_ptr<Configuration> configuration;
  Json clear_files;
  String temporary;
  String directory;
  std::shared_ptr<PlayerStorage> storage;
};

const GameTestRegistration
  firstSaveRetry("PlayerStorageTest", "FailedFirstSaveDoesNotPublishAndIdenticalRetryPersists", [](GameTest& fixture) {
    PlayerStorageContext setup;
    auto initialized = setup.init(fixture.root());
    ASSERT_TRUE(initialized) << initialized.error().what();
    auto const& directory = setup.directory;
    auto& storage = setup.storage;
    auto player = create_player(fixture.root(), "First durable player");
    ASSERT_TRUE(player) << player.error().what();
    auto uuid = (*player)->uuid();
    ASSERT_TRUE(uuid);
    auto path = File::relative_to(directory, uuid->hex() + ".player");
    auto stage = path + ".new";
    ASSERT_TRUE(File::write_file(String("owned collision"), stage));
    EXPECT_FALSE(storage->savePlayer(*player));
    EXPECT_EQ(storage->playerCount(), 0u);
    EXPECT_FALSE(storage->maybeGetPlayerData(*uuid));
    EXPECT_FALSE(storage->playerUuidAt(0));
    auto absent = File::exists(path);
    ASSERT_TRUE(absent);
    EXPECT_FALSE(*absent);
    auto collision = File::read_file_string(stage);
    ASSERT_TRUE(collision);
    EXPECT_EQ(*collision, String("owned collision"));
    ASSERT_TRUE(File::remove(stage));
    auto saved = storage->savePlayer(*player);
    ASSERT_TRUE(saved) << saved.error().what();
    EXPECT_EQ(storage->playerCount(), 1u);
    EXPECT_EQ(storage->getPlayerData(*uuid), *saved);
    auto bytes = VersionedJson::read_file(path);
    ASSERT_TRUE(bytes) << bytes.error().what();
    EXPECT_EQ(bytes->content, *saved);
    ASSERT_TRUE(storage->moveToFront(*uuid));
    storage.reset();
    auto reloaded = PlayerStorage::create(fixture.root(), directory);
    ASSERT_TRUE(reloaded) << reloaded.error().what();
    EXPECT_EQ((*reloaded)->getPlayerData(*uuid), *saved);
    auto loaded = (*reloaded)->loadPlayer(*uuid);
    ASSERT_TRUE(loaded) << loaded.error().what();
    ASSERT_TRUE(*loaded);
    EXPECT_EQ((*loaded)->name(), String("First durable player"));
  });

const GameTestRegistration changedSaveRetry(
  "PlayerStorageTest", "FailedChangedSaveKeepsAcknowledgedPlayerAndRetriesIdenticalBytes", [](GameTest& fixture) {
    PlayerStorageContext setup;
    auto initialized = setup.init(fixture.root());
    ASSERT_TRUE(initialized) << initialized.error().what();
    auto const& directory = setup.directory;
    auto& storage = setup.storage;
    auto player = create_player(fixture.root(), "Original durable name");
    ASSERT_TRUE(player) << player.error().what();
    auto uuid = (*player)->uuid();
    ASSERT_TRUE(uuid);
    auto original = storage->savePlayer(*player);
    ASSERT_TRUE(original) << original.error().what();
    auto path = File::relative_to(directory, uuid->hex() + ".player");
    auto stage = path + ".new";
    ASSERT_TRUE(File::write_file(String("owned collision"), stage));
    // An acknowledged, unchanged player does not attempt a replacement.
    EXPECT_TRUE(storage->savePlayer(*player));
    ASSERT_TRUE((*player)->setName("Changed durable name"));
    EXPECT_FALSE(storage->savePlayer(*player));
    EXPECT_EQ(storage->getPlayerData(*uuid), *original);
    auto old_bytes = VersionedJson::read_file(path);
    ASSERT_TRUE(old_bytes);
    EXPECT_EQ(old_bytes->content, *original);
    ASSERT_TRUE(File::remove(stage));
    auto changed = storage->savePlayer(*player);
    ASSERT_TRUE(changed) << changed.error().what();
    EXPECT_NE(*changed, *original);
    auto new_bytes = VersionedJson::read_file(path);
    ASSERT_TRUE(new_bytes);
    EXPECT_EQ(new_bytes->content, *changed);
    storage.reset();
    auto reloaded = PlayerStorage::create(fixture.root(), directory);
    ASSERT_TRUE(reloaded) << reloaded.error().what();
    EXPECT_EQ((*reloaded)->getPlayerData(*uuid), *changed);
    auto loaded = (*reloaded)->loadPlayer(*uuid);
    ASSERT_TRUE(loaded);
    ASSERT_TRUE(*loaded);
    EXPECT_EQ((*loaded)->name(), String("Changed durable name"));
  });

const GameTestRegistration malformedName(
  "PlayerStorageTest", "UnreadableNameDoesNotHideOtherCharactersOrRewriteTheSave", [](GameTest& fixture) {
    PlayerStorageContext setup;
    ASSERT_TRUE(setup.init(fixture.root()));
    auto healthy = create_player(fixture.root(), "Healthy explorer");
    auto damaged = create_player(fixture.root(), "Other explorer");
    ASSERT_TRUE(healthy && damaged);
    auto healthyId = (*healthy)->uuid();
    auto damagedId = (*damaged)->uuid();
    ASSERT_TRUE(healthyId && damagedId);
    ASSERT_TRUE(setup.storage->savePlayer(*healthy));
    ASSERT_TRUE(setup.storage->savePlayer(*damaged));
    setup.storage.reset();

    auto path = File::relative_to(setup.directory, damagedId->hex() + ".player");
    auto saved = VersionedJson::read_file(path);
    ASSERT_TRUE(saved);
    // An otherwise intact binary player with a truncated UTF-8 name.
    auto changed = saved->content.set_path("identity.name", String("Other \xc3"));
    ASSERT_TRUE(changed);
    saved->content = std::move(*changed);
    ASSERT_TRUE(VersionedJson::write_file(*saved, path));
    auto original = File::read_file(path);
    ASSERT_TRUE(original);

    std::expected<std::shared_ptr<PlayerStorage>, StarException> opened;
    EXPECT_NONFATAL_FAILURE(opened = PlayerStorage::create(fixture.root(), setup.directory), damagedId->hex().utf8());
    ASSERT_TRUE(opened) << opened.error().what();
    EXPECT_EQ((*opened)->playerCount(), 1u);
    auto all = (*opened)->playerUuidListByName("");
    ASSERT_TRUE(all) << all.error().what();
    EXPECT_EQ(*all, (List<Uuid>{*healthyId}));
    auto match = (*opened)->playerUuidByName("HEALTHY");
    ASSERT_TRUE(match) << match.error().what();
    EXPECT_EQ(*match, *healthyId);
    auto selected = (*opened)->loadPlayer(*healthyId);
    ASSERT_TRUE(selected && *selected);
    EXPECT_EQ((*selected)->name(), String("Healthy explorer"));
    opened->reset();
    EXPECT_EQ(File::read_file(path), *original);
  });

const GameTestRegistration metadataRetry(
  "PlayerStorageTest", "MetadataAcknowledgesOnlySuccessfulWritesAndPreservesExplicitNull", [](GameTest& fixture) {
    PlayerStorageContext setup;
    auto initialized = setup.init(fixture.root());
    ASSERT_TRUE(initialized) << initialized.error().what();
    auto const& directory = setup.directory;
    auto& storage = setup.storage;
    ASSERT_TRUE(storage->setMetadata("selection", "old"));
    auto stage = File::relative_to(directory, "metadata.new");
    ASSERT_TRUE(File::write_file(String("owned collision"), stage));
    EXPECT_TRUE(storage->setMetadata("selection", "old"));
    EXPECT_FALSE(storage->setMetadata("selection", "new"));
    auto old_bytes = read_metadata(directory);
    ASSERT_TRUE(old_bytes);
    EXPECT_EQ(old_bytes->get_string("selection"), String("old"));
    EXPECT_EQ(storage->getMetadata("selection"), Json("new"));
    ASSERT_TRUE(File::remove(stage));
    ASSERT_TRUE(storage->setMetadata("selection", "new"));
    auto new_bytes = read_metadata(directory);
    ASSERT_TRUE(new_bytes);
    EXPECT_EQ(new_bytes->get_string("selection"), String("new"));
    ASSERT_TRUE(File::write_file(String("owned collision"), stage));
    EXPECT_TRUE(storage->setMetadata("selection", "new"));
    EXPECT_FALSE(storage->setMetadata("explicitNull", Json()));
    ASSERT_TRUE(File::remove(stage));
    ASSERT_TRUE(storage->setMetadata("explicitNull", Json()));
    auto null_bytes = read_metadata(directory);
    ASSERT_TRUE(null_bytes);
    auto object = null_bytes->to_object();
    ASSERT_TRUE(object);
    EXPECT_TRUE(object->contains("explicitNull"));
    EXPECT_FALSE(object->contains("missing"));
    EXPECT_EQ(null_bytes->get("explicitNull"), Json());
    storage.reset();
    auto reloaded = PlayerStorage::create(fixture.root(), directory);
    ASSERT_TRUE(reloaded);
    EXPECT_EQ((*reloaded)->getMetadata("selection"), Json("new"));
    // A successful disk load is also an acknowledged snapshot.
    ASSERT_TRUE(File::write_file(String("owned collision"), stage));
    EXPECT_TRUE((*reloaded)->setMetadata("selection", "new"));
    EXPECT_TRUE((*reloaded)->setMetadata("explicitNull", Json()));
    auto collision = File::read_file_string(stage);
    ASSERT_TRUE(collision);
    EXPECT_EQ(*collision, String("owned collision"));
    ASSERT_TRUE(File::remove(stage));
  });

const GameTestRegistration
  orderRetry("PlayerStorageTest", "OrderDeduplicatesSuccessfulWritesAndRetriesFailedChangesIncludingDeletion",
             [](GameTest& fixture) {
               PlayerStorageContext setup;
               auto initialized = setup.init(fixture.root());
               ASSERT_TRUE(initialized) << initialized.error().what();
               auto const& directory = setup.directory;
               auto& storage = setup.storage;
               auto first = create_player(fixture.root(), "First ordered player");
               auto second = create_player(fixture.root(), "Second ordered player");
               ASSERT_TRUE(first);
               ASSERT_TRUE(second);
               auto first_uuid = (*first)->uuid();
               auto second_uuid = (*second)->uuid();
               ASSERT_TRUE(first_uuid);
               ASSERT_TRUE(second_uuid);
               ASSERT_NE(*first_uuid, *second_uuid);
               ASSERT_TRUE(storage->savePlayer(*first));
               ASSERT_TRUE(storage->savePlayer(*second));
               ASSERT_TRUE(storage->moveToFront(*first_uuid));
               auto stage = File::relative_to(directory, "metadata.new");
               ASSERT_TRUE(File::write_file(String("owned collision"), stage));
               EXPECT_TRUE(storage->moveToFront(*first_uuid));
               EXPECT_FALSE(storage->moveToFront(*second_uuid));
               auto old_bytes = read_metadata(directory);
               ASSERT_TRUE(old_bytes);
               EXPECT_EQ(old_bytes->get("order"), Json(JsonArray{first_uuid->hex(), second_uuid->hex()}));
               ASSERT_TRUE(File::remove(stage));
               ASSERT_TRUE(storage->moveToFront(*second_uuid));
               auto changed_bytes = read_metadata(directory);
               ASSERT_TRUE(changed_bytes);
               EXPECT_EQ(changed_bytes->get("order"), Json(JsonArray{second_uuid->hex(), first_uuid->hex()}));
               storage.reset();
               auto reloaded = PlayerStorage::create(fixture.root(), directory);
               ASSERT_TRUE(reloaded) << reloaded.error().what();
               EXPECT_EQ((*reloaded)->playerUuidAt(0), *second_uuid);
               EXPECT_EQ((*reloaded)->playerUuidAt(1), *first_uuid);
               ASSERT_TRUE(File::write_file(String("owned collision"), stage));
               EXPECT_TRUE((*reloaded)->moveToFront(*second_uuid));
               ASSERT_TRUE((*reloaded)->deletePlayer(*first_uuid));
               EXPECT_FALSE((*reloaded)->moveToFront(*second_uuid));
               ASSERT_TRUE(File::remove(stage));
               ASSERT_TRUE((*reloaded)->moveToFront(*second_uuid));
               auto deleted_bytes = read_metadata(directory);
               ASSERT_TRUE(deleted_bytes);
               EXPECT_EQ(deleted_bytes->get("order"), Json(JsonArray{second_uuid->hex()}));
               reloaded->reset();
               auto final_storage = PlayerStorage::create(fixture.root(), directory);
               ASSERT_TRUE(final_storage);
               EXPECT_EQ((*final_storage)->playerCount(), 1u);
               EXPECT_EQ((*final_storage)->playerUuidAt(0), *second_uuid);
               EXPECT_FALSE((*final_storage)->maybeGetPlayerData(*first_uuid));
             });

const GameTestRegistration codex_state("PlayerCodexes", "DefaultsSaveReadTransitionsAndPinnedCatalog", [](GameTest& fixture) {
  auto temp = File::temporary_directory(fixture.root().random());
  ASSERT_TRUE(temp) << temp.error().what();
  auto cleanup = make_scope_exit([&] { EXPECT_TRUE(File::remove_all(*temp)); });
  auto overlay = File::relative_to(*temp, "assets");
  ASSERT_TRUE(File::create_directory(overlay));
  auto first_path = File::relative_to(overlay, "test-z.codex");
  ASSERT_TRUE(File::write_file(String(R"({"id":"test-z","title":"Zulu","contentPages":["original"]})"), first_path));
  ASSERT_TRUE(File::write_file(String(R"({"id":"test-a","title":"Alpha","contentPages":[]})"),
                               File::relative_to(overlay, "test-a.codex")));
  ASSERT_TRUE(File::write_file(String(R"([
    {"op":"add","path":"/defaultCodexes/testmixed","value":["test-a","test-absent","test-z","test-a"]},
    {"op":"add","path":"/defaultCodexes/testknown","value":["test-z","test-a"]},
    {"op":"add","path":"/defaultCodexes/testempty","value":[]},
    {"op":"add","path":"/defaultCodexes/testinvalid","value":["test-z",false]}
  ])"), File::relative_to(overlay, "player.config.patch")));
  auto settings = fixture.rootSettings();
  settings.assetSources.push_back(overlay);
  settings.storageDirectory = File::relative_to(*temp, "storage");
  settings.runtimeConfigFile = "domain.config";
  settings.logDirectory = {};
  settings.logFile = {};
  settings.includeUGC = false;
  auto root = Root::create(settings);
  ASSERT_TRUE(root) << root.error().what();
  PlayerCodexes domain(**root);
  auto initial = domain.revision();
  EXPECT_FALSE(domain.learn_initial("testinvalid"));
  EXPECT_FALSE(domain.contains("test-z"));
  EXPECT_EQ(domain.learn("test-absent"), CodexLearnResult::NotFound);
  EXPECT_FALSE(domain.set_read("test-absent", false));
  EXPECT_EQ(domain.revision(), initial);

  ASSERT_EQ(domain.learn("test-z"), CodexLearnResult::Learned);
  auto pinned = domain.find("test-z");
  ASSERT_TRUE(pinned);
  EXPECT_EQ(domain.learn("test-z", true), CodexLearnResult::AlreadyKnown);
  ASSERT_TRUE(domain.learn_initial("testmixed"));
  EXPECT_EQ(domain.revision(), initial + 2);
  EXPECT_EQ(domain.first_unread(), pinned);
  auto rows = domain.entries();
  ASSERT_EQ(rows.size(), 2u);
  EXPECT_EQ(rows[0].codex, pinned);
  EXPECT_FALSE(rows[0].read);
  EXPECT_TRUE(rows[1].read);
  ASSERT_TRUE(domain.set_read("test-a", false));
  EXPECT_EQ(domain.entries()[0].codex->id(), String("test-a"));
  EXPECT_TRUE(rows[1].read); // The previous result is a snapshot.
  EXPECT_TRUE(domain.set_read("test-z", true));
  EXPECT_EQ(domain.first_unread(), domain.find("test-a"));
  EXPECT_TRUE(domain.set_read("test-a", true));
  EXPECT_FALSE(domain.set_read("test-a", true));
  EXPECT_FALSE(domain.first_unread());
  EXPECT_TRUE(domain.set_read("test-z", false));
  EXPECT_EQ(domain.revision(), initial + 6);

  Json saved = JsonObject{{"test-z", false}, {"test-a", true}};
  EXPECT_EQ(domain.to_json(), saved);
  auto with_missing = saved.set("test-absent", false);
  ASSERT_TRUE(with_missing);
  auto restored = PlayerCodexes::create(**root, *with_missing);
  ASSERT_TRUE(restored) << restored.error().what();
  EXPECT_NE((*restored)->domain_id(), domain.domain_id());
  EXPECT_EQ((*restored)->to_json(), saved);
  EXPECT_EQ((*restored)->first_unread(), pinned);
  EXPECT_TRUE((*restored)->set_read("test-z", true));
  EXPECT_FALSE((*restored)->first_unread());
  EXPECT_FALSE(PlayerCodexes::create(**root, JsonObject{{"test-absent", 1}}));

  // Even a broken replacement catalog cannot invalidate pinned books or make
  // an empty/all-known default list depend on loading that catalog.
  ASSERT_TRUE(File::write_file(String("{}"), first_path));
  ASSERT_TRUE((*root)->reload());
  EXPECT_TRUE(PlayerCodexes::create(**root, JsonObject{}));
  EXPECT_TRUE(domain.learn_initial("testempty"));
  EXPECT_TRUE(domain.learn_initial("testknown"));
  EXPECT_EQ(domain.learn("test-z"), CodexLearnResult::AlreadyKnown);
  EXPECT_FALSE(domain.learn("test-absent"));
  EXPECT_EQ(domain.revision(), initial + 6);
  EXPECT_EQ(domain.find("test-z"), pinned);
  EXPECT_EQ(pinned->page(0), String("original"));
});

}// namespace

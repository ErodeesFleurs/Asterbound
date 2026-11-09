#include "gtest/gtest.h"

import std;
import star.algorithm;
import star.configuration;
import star.file;
import star.format;
import star.game_test;
import star.json;
import star.player;
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
  return Json::parseJson(*contents);
}

const GameTestRegistration
  firstSaveRetry("PlayerStorageTest", "FailedFirstSaveDoesNotPublishAndIdenticalRetryPersists", [](GameTest& fixture) {
    auto configuration = fixture.root().configuration();
    ASSERT_TRUE(configuration);
    auto clear_files = (*configuration)->get("clearPlayerFiles");
    ASSERT_TRUE(clear_files);
    auto restore_configuration = finally([&] { EXPECT_TRUE((*configuration)->set("clearPlayerFiles", *clear_files)); });
    ASSERT_TRUE((*configuration)->set("clearPlayerFiles", false));
    auto temporary = File::temporary_directory(fixture.root().random());
    ASSERT_TRUE(temporary);
    auto cleanup = finally([&] { EXPECT_TRUE(File::remove_all(*temporary)); });
    auto directory = File::relative_to(*temporary, "player");
    auto storage = PlayerStorage::create(fixture.root(), directory);
    ASSERT_TRUE(storage) << storage.error().what();
    auto player = create_player(fixture.root(), "First durable player");
    ASSERT_TRUE(player) << player.error().what();
    auto uuid = (*player)->uuid();
    ASSERT_TRUE(uuid);
    auto path = File::relative_to(directory, uuid->hex() + ".player");
    auto stage = path + ".new";
    ASSERT_TRUE(File::write_file(String("owned collision"), stage));
    EXPECT_FALSE((*storage)->savePlayer(*player));
    EXPECT_EQ((*storage)->playerCount(), 0u);
    EXPECT_FALSE((*storage)->maybeGetPlayerData(*uuid));
    EXPECT_FALSE((*storage)->playerUuidAt(0));
    auto absent = File::exists(path);
    ASSERT_TRUE(absent);
    EXPECT_FALSE(*absent);
    auto collision = File::read_file_string(stage);
    ASSERT_TRUE(collision);
    EXPECT_EQ(*collision, String("owned collision"));
    ASSERT_TRUE(File::remove(stage));
    auto saved = (*storage)->savePlayer(*player);
    ASSERT_TRUE(saved) << saved.error().what();
    EXPECT_EQ((*storage)->playerCount(), 1u);
    EXPECT_EQ((*storage)->getPlayerData(*uuid), *saved);
    auto bytes = VersionedJson::read_file(path);
    ASSERT_TRUE(bytes) << bytes.error().what();
    EXPECT_EQ(bytes->content, *saved);
    ASSERT_TRUE((*storage)->moveToFront(*uuid));
    storage->reset();
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
    auto configuration = fixture.root().configuration();
    ASSERT_TRUE(configuration);
    auto clear_files = (*configuration)->get("clearPlayerFiles");
    ASSERT_TRUE(clear_files);
    auto restore_configuration = finally([&] { EXPECT_TRUE((*configuration)->set("clearPlayerFiles", *clear_files)); });
    ASSERT_TRUE((*configuration)->set("clearPlayerFiles", false));
    auto temporary = File::temporary_directory(fixture.root().random());
    ASSERT_TRUE(temporary);
    auto cleanup = finally([&] { EXPECT_TRUE(File::remove_all(*temporary)); });
    auto directory = File::relative_to(*temporary, "player");
    auto storage = PlayerStorage::create(fixture.root(), directory);
    ASSERT_TRUE(storage);
    auto player = create_player(fixture.root(), "Original durable name");
    ASSERT_TRUE(player) << player.error().what();
    auto uuid = (*player)->uuid();
    ASSERT_TRUE(uuid);
    auto original = (*storage)->savePlayer(*player);
    ASSERT_TRUE(original) << original.error().what();
    auto path = File::relative_to(directory, uuid->hex() + ".player");
    auto stage = path + ".new";
    ASSERT_TRUE(File::write_file(String("owned collision"), stage));
    // An acknowledged, unchanged player does not attempt a replacement.
    EXPECT_TRUE((*storage)->savePlayer(*player));
    ASSERT_TRUE((*player)->setName("Changed durable name"));
    EXPECT_FALSE((*storage)->savePlayer(*player));
    EXPECT_EQ((*storage)->getPlayerData(*uuid), *original);
    auto old_bytes = VersionedJson::read_file(path);
    ASSERT_TRUE(old_bytes);
    EXPECT_EQ(old_bytes->content, *original);
    ASSERT_TRUE(File::remove(stage));
    auto changed = (*storage)->savePlayer(*player);
    ASSERT_TRUE(changed) << changed.error().what();
    EXPECT_NE(*changed, *original);
    auto new_bytes = VersionedJson::read_file(path);
    ASSERT_TRUE(new_bytes);
    EXPECT_EQ(new_bytes->content, *changed);
    storage->reset();
    auto reloaded = PlayerStorage::create(fixture.root(), directory);
    ASSERT_TRUE(reloaded) << reloaded.error().what();
    EXPECT_EQ((*reloaded)->getPlayerData(*uuid), *changed);
    auto loaded = (*reloaded)->loadPlayer(*uuid);
    ASSERT_TRUE(loaded);
    ASSERT_TRUE(*loaded);
    EXPECT_EQ((*loaded)->name(), String("Changed durable name"));
  });

const GameTestRegistration metadataRetry(
  "PlayerStorageTest", "MetadataAcknowledgesOnlySuccessfulWritesAndPreservesExplicitNull", [](GameTest& fixture) {
    auto configuration = fixture.root().configuration();
    ASSERT_TRUE(configuration);
    auto clear_files = (*configuration)->get("clearPlayerFiles");
    ASSERT_TRUE(clear_files);
    auto restore_configuration = finally([&] { EXPECT_TRUE((*configuration)->set("clearPlayerFiles", *clear_files)); });
    ASSERT_TRUE((*configuration)->set("clearPlayerFiles", false));
    auto temporary = File::temporary_directory(fixture.root().random());
    ASSERT_TRUE(temporary);
    auto cleanup = finally([&] { EXPECT_TRUE(File::remove_all(*temporary)); });
    auto directory = File::relative_to(*temporary, "player");
    auto storage = PlayerStorage::create(fixture.root(), directory);
    ASSERT_TRUE(storage);
    ASSERT_TRUE((*storage)->setMetadata("selection", "old"));
    auto stage = File::relative_to(directory, "metadata.new");
    ASSERT_TRUE(File::write_file(String("owned collision"), stage));
    EXPECT_TRUE((*storage)->setMetadata("selection", "old"));
    EXPECT_FALSE((*storage)->setMetadata("selection", "new"));
    auto old_bytes = read_metadata(directory);
    ASSERT_TRUE(old_bytes);
    EXPECT_EQ(old_bytes->getString("selection"), String("old"));
    EXPECT_EQ((*storage)->getMetadata("selection"), Json("new"));
    ASSERT_TRUE(File::remove(stage));
    ASSERT_TRUE((*storage)->setMetadata("selection", "new"));
    auto new_bytes = read_metadata(directory);
    ASSERT_TRUE(new_bytes);
    EXPECT_EQ(new_bytes->getString("selection"), String("new"));
    ASSERT_TRUE(File::write_file(String("owned collision"), stage));
    EXPECT_TRUE((*storage)->setMetadata("selection", "new"));
    EXPECT_FALSE((*storage)->setMetadata("explicitNull", Json()));
    ASSERT_TRUE(File::remove(stage));
    ASSERT_TRUE((*storage)->setMetadata("explicitNull", Json()));
    auto null_bytes = read_metadata(directory);
    ASSERT_TRUE(null_bytes);
    auto object = null_bytes->toObject();
    ASSERT_TRUE(object);
    EXPECT_TRUE(object->contains("explicitNull"));
    EXPECT_FALSE(object->contains("missing"));
    EXPECT_EQ(null_bytes->get("explicitNull"), Json());
    storage->reset();
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
               auto configuration = fixture.root().configuration();
               ASSERT_TRUE(configuration);
               auto clear_files = (*configuration)->get("clearPlayerFiles");
               ASSERT_TRUE(clear_files);
               auto restore_configuration =
                 finally([&] { EXPECT_TRUE((*configuration)->set("clearPlayerFiles", *clear_files)); });
               ASSERT_TRUE((*configuration)->set("clearPlayerFiles", false));
               auto temporary = File::temporary_directory(fixture.root().random());
               ASSERT_TRUE(temporary);
               auto cleanup = finally([&] { EXPECT_TRUE(File::remove_all(*temporary)); });
               auto directory = File::relative_to(*temporary, "player");
               auto storage = PlayerStorage::create(fixture.root(), directory);
               ASSERT_TRUE(storage);
               auto first = create_player(fixture.root(), "First ordered player");
               auto second = create_player(fixture.root(), "Second ordered player");
               ASSERT_TRUE(first);
               ASSERT_TRUE(second);
               auto first_uuid = (*first)->uuid();
               auto second_uuid = (*second)->uuid();
               ASSERT_TRUE(first_uuid);
               ASSERT_TRUE(second_uuid);
               ASSERT_NE(*first_uuid, *second_uuid);
               ASSERT_TRUE((*storage)->savePlayer(*first));
               ASSERT_TRUE((*storage)->savePlayer(*second));
               ASSERT_TRUE((*storage)->moveToFront(*first_uuid));
               auto stage = File::relative_to(directory, "metadata.new");
               ASSERT_TRUE(File::write_file(String("owned collision"), stage));
               EXPECT_TRUE((*storage)->moveToFront(*first_uuid));
               EXPECT_FALSE((*storage)->moveToFront(*second_uuid));
               auto old_bytes = read_metadata(directory);
               ASSERT_TRUE(old_bytes);
               EXPECT_EQ(old_bytes->get("order"), Json(JsonArray{first_uuid->hex(), second_uuid->hex()}));
               ASSERT_TRUE(File::remove(stage));
               ASSERT_TRUE((*storage)->moveToFront(*second_uuid));
               auto changed_bytes = read_metadata(directory);
               ASSERT_TRUE(changed_bytes);
               EXPECT_EQ(changed_bytes->get("order"), Json(JsonArray{second_uuid->hex(), first_uuid->hex()}));
               storage->reset();
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

}// namespace

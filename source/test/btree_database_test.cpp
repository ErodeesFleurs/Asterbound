import std;
import star.format;
import star.list;
import star.io_device;
import star.set;
import star.btree;
import star.ordered_map;
import star.lru_cache;
import star.data_stream_devices;
import star.thread;
import star.btree_database;
import star.file;
import star.buffer;
import star.random;
import star.logging;

#include "gtest/gtest.h"

import star.byte_array;
import star.bytes;
import star.algorithm;
using namespace Star;

namespace {
  // Fault injection at the actual IO boundary; all database writes, root
  // selection, recovery and serialization still use the production code.
  struct FailingSyncBuffer : Buffer {
    bool failSync = false;
    unsigned syncCalls = 0;

    [[nodiscard]] std::expected<void, StarException> sync() override {
      ++syncCalls;
      if (failSync)
        return std::unexpected(IOException("Injected sync failure"));
      return {};
    }
  };

  struct FailingCloseBuffer : Buffer {
    unsigned closeCalls = 0;

    [[nodiscard]] std::expected<void, StarException> close() override {
      ++closeCalls;
      auto closed = IODevice::close();
      if (!closed)
        return std::unexpected(std::move(closed.error()));
      return std::unexpected(IOException("Injected close failure"));
    }
  };

  struct ReadBudgetFile : File {
    using File::File;

    std::size_t block_size = 256;
    unsigned full_block_reads = 0;
    std::optional<unsigned> remaining_full_reads;

    [[nodiscard]] std::expected<std::size_t, StarException> read_absolute(StreamOffset position,
                                                                          std::span<char> data) override {
      if (position >= 512 && data.size() == block_size) {
        ++full_block_reads;
        if (remaining_full_reads) {
          if (*remaining_full_reads == 0)
            return std::unexpected(IOException("Unexpected old block payload read"));
          --*remaining_full_reads;
        }
      }
      return File::read_absolute(position, data);
    }
  };

  std::size_t const RandFactor = 0xd5a2f037;
  std::size_t const MaxSize = 400;
  std::uint32_t const MaxKey = 100000;

  ByteArray toByteArray(std::uint32_t k) {
    k = toBigEndian(k);
    return ByteArray({reinterpret_cast<char const*>(std::addressof(k)), sizeof(k)});
  }

  ByteArray genBlock(std::uint32_t k) {
    // Make sure not empty, because we test for existence with empty()
    std::size_t size = (RandFactor * k) % (MaxSize - 1) + 1;
    std::uint8_t val = (std::uint8_t)(k % 256);

    ByteArray b(size, 0);

    for (std::size_t i = 0; i < size; ++i) {
      b[i] = val;
      val += 1;
    }

    return b;
  }

  bool checkBlock(std::uint32_t k, ByteArray b) {
    return genBlock(k) == b;
  }

  void putAll(RandomService& random, BTreeDatabase& db, const List<std::uint32_t>& keys) {
    for (std::uint32_t k : keys) {
      ByteArray val = genBlock(k);
      auto inserted = db.insert(toByteArray(k), val);
      ASSERT_TRUE(inserted) << inserted.error().what();

      int i = random.randi32();
      if (i % 23 == 0) {
        auto committed = db.commit();
        ASSERT_TRUE(committed) << committed.error().what();
      }
    }
  }

  void checkAll(BTreeDatabase& db, const List<std::uint32_t>& keys) {
    for (std::uint32_t k : keys) {
      auto res = db.find(toByteArray(k));
      ASSERT_TRUE(res) << res.error().what();
      ASSERT_TRUE(*res) << "Missing record for key " << k;
      EXPECT_TRUE(checkBlock(k, **res));
    }

    // Also check that forAll works.

    Set<ByteArray> keySet;
    for (std::uint32_t k : keys)
      keySet.add(toByteArray(k));

    ASSERT_TRUE(db.forAll([&keySet](ByteArray const& key, ByteArray const&) -> std::expected<void, StarException> {
      EXPECT_TRUE(keySet.remove(key));
      return {};
    }));

    EXPECT_TRUE(keySet.empty());
  }

  [[nodiscard]] std::expected<std::size_t, StarException> removeAll(BTreeDatabase& db, const List<std::uint32_t>& keys) {
    std::size_t totalRemoved = 0;
    for (std::uint32_t k : keys) {
      auto old = db.find(toByteArray(k));
      if (!old)
        return std::unexpected(std::move(old.error()));
      EXPECT_TRUE(!*old || **old == genBlock(k));

      auto removed = db.remove(toByteArray(k));
      if (!removed)
        return std::unexpected(std::move(removed.error()));
      if (*removed) {
        auto found = db.find(toByteArray(k));
        if (!found)
          return std::unexpected(std::move(found.error()));
        EXPECT_FALSE(*found);
        ++totalRemoved;
      }
    }
    return totalRemoved;
  }

  void checkBlockCounts(BTreeDatabase& db) {
    auto total = db.totalBlockCount();
    ASSERT_TRUE(total) << total.error().what();
    auto free = db.freeBlockCount();
    ASSERT_TRUE(free) << free.error().what();
    auto index = db.indexBlockCount();
    ASSERT_TRUE(index) << index.error().what();
    auto leaf = db.leafBlockCount();
    ASSERT_TRUE(leaf) << leaf.error().what();
    EXPECT_EQ(*total, *free + *index + *leaf);
  }

  void testBTreeDatabase(std::size_t testCount, std::size_t writeRepeat, std::size_t randCount, std::size_t rollbackCount, std::size_t blockSize) {
    auto randomResult = RandomService::create();
    ASSERT_TRUE(randomResult);
    auto& random = **randomResult;
    Logger logger;
    auto tmpFileResult = File::temporary_file(random);
    ASSERT_TRUE(tmpFileResult);
    auto tmpFile = std::move(*tmpFileResult);
    auto finallyGuard = finally([&tmpFile]() { EXPECT_TRUE(tmpFile->remove()); });

    Set<std::uint32_t> keySet;
    BTreeDatabase db(logger, "TestDB", 4);
    ASSERT_TRUE(db.setAutoCommit(false));

    while (keySet.size() < testCount) {
      auto key = random.randUInt(0, MaxKey);
      ASSERT_TRUE(key);
      keySet.add(*key);
    }

    List<std::uint32_t> keys;
    for (std::uint32_t k : keySet) {
      for (std::uint32_t j = 0; j < writeRepeat; ++j)
        keys.append(k);
    }

    db.setIndexCacheSize(0);
    ASSERT_TRUE(db.setBlockSize(blockSize));
    ASSERT_TRUE(db.setIODevice(tmpFile));
    ASSERT_TRUE(db.open());

    // record writes/reads repeated writeRepeat times randomly each cycle
    random.shuffle(keys);
    putAll(random, db, keys);

    auto recordCount = db.recordCount();
    ASSERT_TRUE(recordCount) << recordCount.error().what();
    EXPECT_EQ(*recordCount, testCount);

    random.shuffle(keys);
    checkAll(db, keys);

    // Random reads/writes with randCount cycles...
    for (std::uint32_t i = 0; i < randCount; ++i) {
      List<std::uint32_t> keysTemp(keys.begin(), keys.begin() + keys.size() / 2);

      random.shuffle(keysTemp);
      auto removed = removeAll(db, keysTemp);
      ASSERT_TRUE(removed) << removed.error().what();
      ASSERT_GT(*removed, 0u);

      random.shuffle(keysTemp);
      putAll(random, db, keysTemp);

      random.shuffle(keys);
      checkAll(db, keys);
    }

    ASSERT_TRUE(db.commit());

    // Random reads/writes/rollbacks with rollbackCount cycles...
    for (std::uint32_t i = 0; i < rollbackCount ; ++i) {
      List<std::uint32_t> keysTemp(keys.begin(), keys.begin() + keys.size() / 2);
      random.shuffle(keysTemp);

      auto removed = removeAll(db, keysTemp);
      ASSERT_TRUE(removed) << removed.error().what();
      ASSERT_GT(*removed, 0u);
      ASSERT_TRUE(db.rollback());

      checkAll(db, keys);
    }

    checkBlockCounts(db);

    // Now testing closing and reading

    ASSERT_TRUE(db.close());

    // Set the wrong value, should be set to correct value in open()
    ASSERT_TRUE(db.setBlockSize(blockSize + 512));

    ASSERT_TRUE(db.open());

    // Checking values...

    checkAll(db, keys);

    checkBlockCounts(db);

    // Removing all records...

    auto totalRemoved = removeAll(db, keys);
    ASSERT_TRUE(totalRemoved) << totalRemoved.error().what();

    EXPECT_EQ(*totalRemoved, testCount);

    checkBlockCounts(db);

    EXPECT_TRUE(db.close());
  }
}

TEST(BTreeDatabaseTest, Consistency) {
  testBTreeDatabase(500, 3, 5, 5, 512);

  // Test a range of block sizes to make sure there are not off by one errors
  // in maximum index / leaf size calculations.
  for (std::size_t i = 0; i < 16; ++i)
    testBTreeDatabase(30, 2, 2, 2, 200 + i);
}

TEST(BTreeDatabaseTest, Threading) {
  auto randomResult = RandomService::create();
  ASSERT_TRUE(randomResult);
  auto& random = **randomResult;
  Logger logger;
  auto tmpFileResult = File::temporary_file(random);
  ASSERT_TRUE(tmpFileResult);
  auto tmpFile = std::move(*tmpFileResult);
  auto finallyGuard = finally([&tmpFile]() { EXPECT_TRUE(tmpFile->remove()); });

  BTreeDatabase db(logger, "TestDB", 4);
  ASSERT_TRUE(db.setAutoCommit(false));
  ASSERT_TRUE(db.setBlockSize(256));
  ASSERT_TRUE(db.setIODevice(tmpFile));
  ASSERT_TRUE(db.open());

  List<std::uint32_t> writeKeySet;
  List<std::uint32_t> deleteKeySet;

  while (writeKeySet.size() < 5000) {
    std::uint32_t key = random.randu32();
    writeKeySet.append(key);
    if (random.randf() > 0.3)
      deleteKeySet.append(key);
  }
  random.shuffle(writeKeySet);

  {
    auto writer = Thread::invoke("databaseTestWriter",
        [&random, &db, &writeKeySet]() -> std::expected<void, StarException> {
          for (std::uint32_t k : writeKeySet) {
            ByteArray val = genBlock(k);
            auto inserted = db.insert(toByteArray(k), val);
            if (!inserted)
              return std::unexpected(std::move(inserted.error()));
            if (random.randi32() % 23 == 0) {
              auto committed = db.commit();
              if (!committed)
                return std::unexpected(std::move(committed.error()));
            }
          }
          return {};
        });
    ASSERT_TRUE(writer) << writer.error().what();

    auto deleter = Thread::invoke("databaseTestDeleter",
        [&random, &db, &deleteKeySet]() -> std::expected<void, StarException> {
          for (std::uint32_t k : deleteKeySet) {
            auto removed = db.remove(toByteArray(k));
            if (!removed)
              return std::unexpected(std::move(removed.error()));
            if (random.randi32() % 23 == 0) {
              auto committed = db.commit();
              if (!committed)
                return std::unexpected(std::move(committed.error()));
            }
          }
          return {};
        });
    ASSERT_TRUE(deleter) << deleter.error().what();

    auto writerFinished = writer->finish();
    EXPECT_TRUE(writerFinished) << writerFinished.error().what();
    auto deleterFinished = deleter->finish();
    EXPECT_TRUE(deleterFinished) << deleterFinished.error().what();
    ASSERT_TRUE(writerFinished && deleterFinished);

    ASSERT_TRUE(db.close(false));
    ASSERT_TRUE(tmpFile->open(IOMode::Read));
    ASSERT_TRUE(db.open());

    checkBlockCounts(db);

    List<ThreadFunction<void>> readers;
    for (std::size_t i = 0; i < 5; ++i) {
      auto reader = Thread::invoke("databaseTestReader",
          [&db, &writeKeySet, &deleteKeySet]() -> std::expected<void, StarException> {
            for (std::uint32_t k : writeKeySet) {
              auto res = db.find(toByteArray(k));
              if (!res)
                return std::unexpected(std::move(res.error()));
              if (*res)
                EXPECT_TRUE(checkBlock(k, **res));
              else
                EXPECT_TRUE(deleteKeySet.contains(k));
            }
            return {};
          });
      ASSERT_TRUE(reader) << reader.error().what();
      readers.append(std::move(*reader));
    }
    for (auto& reader : readers) {
      auto finished = reader.finish();
      EXPECT_TRUE(finished) << finished.error().what();
    }
  }
  EXPECT_TRUE(db.close());
}

TEST(BTreeDatabaseTest, FullBlockReplacementDoesNotReadDiscardedPayload) {
  auto random_result = RandomService::create();
  ASSERT_TRUE(random_result);
  auto temporary_result = File::temporary_file(**random_result);
  ASSERT_TRUE(temporary_result);
  auto temporary = std::move(*temporary_result);
  auto cleanup = finally([&]() { EXPECT_TRUE(temporary->remove()); });
  auto device = std::make_shared<ReadBudgetFile>(temporary->file_name());
  Logger logger;
  BTreeDatabase db(logger, "TestDB", 4);
  ASSERT_TRUE(db.setAutoCommit(false));
  ASSERT_TRUE(db.setBlockSize(device->block_size));
  ASSERT_TRUE(db.setIODevice(device));

  // Creating the empty root fully replaces its newly allocated block.
  device->remaining_full_reads = 0;
  auto opened = db.open();
  ASSERT_TRUE(opened) << opened.error().what();
  EXPECT_EQ(device->full_block_reads, 0u);

  // Loading the committed leaf is necessary. Reading its old payload again
  // merely to stage its complete replacement is not.
  device->remaining_full_reads = 1;
  auto inserted = db.insert(toByteArray(1), ByteArray(20, 'a'));
  ASSERT_TRUE(inserted) << inserted.error().what();
  EXPECT_EQ(device->full_block_reads, 1u);

  // Both the next leaf load and its replacement use the staged overlay.
  device->remaining_full_reads = 0;
  inserted = db.insert(toByteArray(2), ByteArray(30, 'b'));
  ASSERT_TRUE(inserted) << inserted.error().what();
  inserted = db.insert(toByteArray(1), ByteArray(40, 'c'));
  ASSERT_TRUE(inserted) << inserted.error().what();
  EXPECT_EQ(device->full_block_reads, 1u);
  ASSERT_TRUE(db.commit());
  ASSERT_TRUE(db.close());

  device->remaining_full_reads.reset();
  ASSERT_TRUE(db.open());
  auto first = db.find(toByteArray(1));
  ASSERT_TRUE(first) << first.error().what();
  ASSERT_TRUE(*first);
  EXPECT_EQ(**first, ByteArray(40, 'c'));
  auto second = db.find(toByteArray(2));
  ASSERT_TRUE(second) << second.error().what();
  ASSERT_TRUE(*second);
  EXPECT_EQ(**second, ByteArray(30, 'b'));
  EXPECT_TRUE(db.close());
}

TEST(BTreeDatabaseTest, PartialFreeIndexWritesPreservePayloadAndRollback) {
  auto random_result = RandomService::create();
  ASSERT_TRUE(random_result);
  auto temporary_result = File::temporary_file(**random_result);
  ASSERT_TRUE(temporary_result);
  auto temporary = std::move(*temporary_result);
  auto cleanup = finally([&]() { EXPECT_TRUE(temporary->remove()); });
  auto device = std::make_shared<ReadBudgetFile>(temporary->file_name());
  Logger logger;
  BTreeDatabase db(logger, "TestDB", 4);
  ASSERT_TRUE(db.setAutoCommit(false));
  ASSERT_TRUE(db.setBlockSize(device->block_size));
  ASSERT_TRUE(db.setIODevice(device));
  ASSERT_TRUE(db.open());
  ByteArray large_value(3 * device->block_size, 'x');
  ByteArray small_value(20, 's');
  ByteArray other_value(30, 'o');
  ASSERT_TRUE(db.insert(toByteArray(1), large_value));
  ASSERT_TRUE(db.commit());

  ASSERT_TRUE(db.insert(toByteArray(1), small_value));
  ASSERT_TRUE(db.insert(toByteArray(2), other_value));
  ASSERT_TRUE(db.rollback());
  auto original = db.find(toByteArray(1));
  ASSERT_TRUE(original) << original.error().what();
  ASSERT_TRUE(*original);
  EXPECT_EQ(**original, large_value);
  auto absent = db.find(toByteArray(2));
  ASSERT_TRUE(absent) << absent.error().what();
  EXPECT_FALSE(*absent);

  auto file_size = temporary->size();
  ASSERT_TRUE(file_size) << file_size.error().what();
  auto before = temporary->read_bytes_absolute(0, static_cast<std::size_t>(*file_size));
  ASSERT_TRUE(before) << before.error().what();
  ASSERT_TRUE(db.insert(toByteArray(1), small_value));
  ASSERT_TRUE(db.insert(toByteArray(2), other_value));

  // Shrinking the leaf frees tail blocks. The free-index fields are separate
  // partial writes: acquire the old block once, then overlay all later fields.
  device->full_block_reads = 0;
  device->remaining_full_reads = 1;
  auto committed = db.commit();
  ASSERT_TRUE(committed) << committed.error().what();
  EXPECT_EQ(device->full_block_reads, 1u);
  device->remaining_full_reads.reset();
  auto after = temporary->read_bytes_absolute(0, static_cast<std::size_t>(*file_size));
  ASSERT_TRUE(after) << after.error().what();
  unsigned free_index_blocks = 0;
  for (std::size_t offset = 512; offset < after->size(); offset += device->block_size) {
    auto block = after->span().subspan(offset, device->block_size);
    if (block[0] != 'F' || block[1] != 'F')
      continue;
    ++free_index_blocks;
    DataStreamExternalBuffer fields(block.subspan(6));
    auto count = fields.read<std::uint32_t>();
    ASSERT_TRUE(count) << count.error().what();
    auto untouched_offset = 10 + sizeof(std::uint32_t) * *count;
    ASSERT_LT(untouched_offset, device->block_size);
    auto untouched = block.subspan(untouched_offset);
    EXPECT_TRUE(std::ranges::contains(untouched, 'x'));
    EXPECT_EQ(ByteArray(untouched),
              ByteArray(before->span().subspan(offset + untouched_offset, device->block_size - untouched_offset)));
  }
  EXPECT_EQ(free_index_blocks, 1u);

  ASSERT_TRUE(db.close());
  ASSERT_TRUE(db.open());
  auto first = db.find(toByteArray(1));
  ASSERT_TRUE(first) << first.error().what();
  ASSERT_TRUE(*first);
  EXPECT_EQ(**first, small_value);
  auto second = db.find(toByteArray(2));
  ASSERT_TRUE(second) << second.error().what();
  ASSERT_TRUE(*second);
  EXPECT_EQ(**second, other_value);
  checkBlockCounts(db);
  EXPECT_TRUE(db.close());
}

TEST(BTreeDatabaseTest, FailedCommitIsReportedWithoutDestructorRetry) {
  Logger logger;
  auto device = std::make_shared<FailingSyncBuffer>();
  auto db = std::make_unique<BTreeDatabase>(logger, "TestDB", 4);
  ASSERT_TRUE(db->setAutoCommit(false));
  ASSERT_TRUE(db->setIODevice(device));
  ASSERT_TRUE(db->open());
  ASSERT_TRUE(db->insert(toByteArray(0), genBlock(0)));
  ASSERT_TRUE(db->commit());
  ASSERT_TRUE(db->insert(toByteArray(1), genBlock(1)));
  device->failSync = true;

  auto committedResult = db->commit();
  ASSERT_FALSE(committedResult);
  EXPECT_TRUE(committedResult.error().is<IOException>());
  EXPECT_TRUE(db->isOpen());
  unsigned callsAfterFailure = device->syncCalls;
  db.reset();
  EXPECT_EQ(device->syncCalls, callsAfterFailure);

  // sync() failure reports uncertain durability, not an undo of writes that
  // already reached the device.  The existing allocator may reuse a freed
  // leaf block before writeRoot(), so even the old root selector can expose
  // the new complete leaf.  Require a consistent, usable recovery, not a
  // particular visibility outcome for the failed transaction.
  device->failSync = false;
  BTreeDatabase recovered(logger);
  ASSERT_TRUE(recovered.setIODevice(device));
  ASSERT_TRUE(recovered.open());
  auto committed = recovered.find(toByteArray(0));
  ASSERT_TRUE(committed) << committed.error().what();
  ASSERT_TRUE(*committed);
  EXPECT_EQ(**committed, genBlock(0));
  auto uncertain = recovered.find(toByteArray(1));
  ASSERT_TRUE(uncertain) << uncertain.error().what();
  if (*uncertain)
    EXPECT_EQ(**uncertain, genBlock(1));
  auto recoveredCount = *uncertain ? 2u : 1u;
  auto recordCount = recovered.recordCount();
  ASSERT_TRUE(recordCount) << recordCount.error().what();
  EXPECT_EQ(*recordCount, recoveredCount);
  checkBlockCounts(recovered);

  ASSERT_TRUE(recovered.insert(toByteArray(2), genBlock(2)));
  ASSERT_TRUE(recovered.close());
  ASSERT_TRUE(recovered.open());
  auto subsequent = recovered.find(toByteArray(2));
  ASSERT_TRUE(subsequent) << subsequent.error().what();
  ASSERT_TRUE(*subsequent);
  EXPECT_EQ(**subsequent, genBlock(2));
  auto subsequentCount = recovered.recordCount();
  ASSERT_TRUE(subsequentCount) << subsequentCount.error().what();
  EXPECT_EQ(*subsequentCount, recoveredCount + 1);
  EXPECT_TRUE(recovered.close());
}

TEST(BTreeDatabaseTest, FailedCloseIsReportedWithoutDestructorRetry) {
  Logger logger;
  auto device = std::make_shared<FailingSyncBuffer>();
  auto db = std::make_unique<BTreeDatabase>(logger, "TestDB", 4);
  ASSERT_TRUE(db->setAutoCommit(false));
  ASSERT_TRUE(db->setIODevice(device));
  ASSERT_TRUE(db->open());
  ASSERT_TRUE(db->insert(toByteArray(1), genBlock(1)));
  device->failSync = true;

  auto closed = db->close(true);
  ASSERT_FALSE(closed);
  EXPECT_TRUE(closed.error().is<IOException>());
  EXPECT_TRUE(db->isOpen());
  EXPECT_TRUE(device->is_open());
  unsigned callsAfterFailure = device->syncCalls;
  db.reset();
  EXPECT_EQ(device->syncCalls, callsAfterFailure);
}

TEST(BTreeDatabaseTest, FailedDeviceCloseIsConsumedOnce) {
  Logger logger;
  auto device = std::make_shared<FailingCloseBuffer>();
  BTreeDatabase db(logger, "TestDB", 4);
  ASSERT_TRUE(db.setIODevice(device));
  ASSERT_TRUE(db.open());

  auto closed = db.close(true);
  ASSERT_FALSE(closed);
  EXPECT_TRUE(closed.error().is<IOException>());
  EXPECT_FALSE(db.isOpen());
  EXPECT_FALSE(device->is_open());
  EXPECT_TRUE(db.close(true));
  EXPECT_EQ(device->closeCalls, 1u);
}

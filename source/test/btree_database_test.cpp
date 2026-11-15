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
import star.string;

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

  // Delegate successful operations to a real File. Faults stop one actual
  // commit phase, including a short inactive-root write reported by write_full.
  struct CommitFaultFile : File {
    using File::File;

    unsigned phase = 0;
    unsigned fault_phase = 0;
    bool write_fault = false;
    bool short_root_write = false;
    bool armed = false;

    void arm(unsigned target_phase, bool fail_write, bool partial_root) {
      phase = 0;
      fault_phase = target_phase;
      write_fault = fail_write;
      short_root_write = partial_root;
      armed = true;
    }

    [[nodiscard]] std::expected<std::size_t, StarException> write(std::span<char const> data) override {
      if (armed && write_fault && phase == fault_phase) {
        armed = false;
        if (short_root_write)
          return File::write(data.first(std::min<std::size_t>(8, data.size() / 2)));
        return std::unexpected(IOException("Injected commit write failure"));
      }
      return File::write(data);
    }

    [[nodiscard]] std::expected<std::size_t, StarException> write_absolute(StreamOffset position,
                                                                           std::span<char const> data) override {
      if (armed && write_fault && phase == fault_phase) {
        armed = false;
        return std::unexpected(IOException("Injected commit block write failure"));
      }
      return File::write_absolute(position, data);
    }

    [[nodiscard]] std::expected<void, StarException> sync() override {
      if (armed && !write_fault && phase == fault_phase) {
        armed = false;
        return std::unexpected(IOException("Injected commit barrier failure"));
      }
      auto result = File::sync();
      if (result)
        ++phase;
      return result;
    }
  };

  std::size_t const RandFactor = 0xd5a2f037;
  std::size_t const MaxSize = 400;
  std::uint32_t const MaxKey = 100000;

  ByteArray key_bytes(std::uint32_t k) {
    auto bytes = std::bit_cast<std::array<char, sizeof(k)>>(to_big_endian(k));
    return ByteArray(std::span<char const>(bytes));
  }

  ByteArray make_block(std::uint32_t k) {
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

  bool check_block(std::uint32_t k, ByteArray const& b) {
    return make_block(k) == b;
  }

  void insert_all(RandomService& random, BTreeDatabase& db, const List<std::uint32_t>& keys) {
    for (std::uint32_t k : keys) {
      ByteArray val = make_block(k);
      auto inserted = db.insert(key_bytes(k), val);
      ASSERT_TRUE(inserted) << inserted.error().what();

      int i = random.next_i32();
      if (i % 23 == 0) {
        auto committed = db.commit();
        ASSERT_TRUE(committed) << committed.error().what();
      }
    }
  }

  void check_all(BTreeDatabase& db, const List<std::uint32_t>& keys) {
    for (std::uint32_t k : keys) {
      auto res = db.find(key_bytes(k));
      ASSERT_TRUE(res) << res.error().what();
      ASSERT_TRUE(*res) << "Missing record for key " << k;
      EXPECT_TRUE(check_block(k, **res));
    }

    // Also check that for_all works.

    Set<ByteArray> keySet;
    for (std::uint32_t k : keys)
      keySet.add(key_bytes(k));

    ASSERT_TRUE(db.for_all([&keySet](ByteArray const& key, ByteArray const&) -> std::expected<void, StarException> {
      EXPECT_TRUE(keySet.remove(key));
      return {};
    }));

    EXPECT_TRUE(keySet.empty());
  }

  [[nodiscard]] std::expected<std::size_t, StarException> erase_all(BTreeDatabase& db, const List<std::uint32_t>& keys) {
    std::size_t totalRemoved = 0;
    for (std::uint32_t k : keys) {
      auto old = db.find(key_bytes(k));
      if (!old)
        return std::unexpected(std::move(old.error()));
      EXPECT_TRUE(!*old || **old == make_block(k));

      auto removed = db.erase(key_bytes(k));
      if (!removed)
        return std::unexpected(std::move(removed.error()));
      if (*removed) {
        auto found = db.find(key_bytes(k));
        if (!found)
          return std::unexpected(std::move(found.error()));
        EXPECT_FALSE(*found);
        ++totalRemoved;
      }
    }
    return totalRemoved;
  }

  void check_block_counts(BTreeDatabase& db) {
    auto total = db.total_block_count();
    ASSERT_TRUE(total) << total.error().what();
    auto free = db.free_block_count();
    ASSERT_TRUE(free) << free.error().what();
    auto index = db.index_block_count();
    ASSERT_TRUE(index) << index.error().what();
    auto leaf = db.leaf_block_count();
    ASSERT_TRUE(leaf) << leaf.error().what();
    EXPECT_EQ(*total, *free + *index + *leaf);
  }

  void test_btree_database(std::size_t testCount, std::size_t writeRepeat, std::size_t randCount, std::size_t rollbackCount, std::size_t block_size) {
    auto randomResult = RandomService::create();
    ASSERT_TRUE(randomResult);
    auto& random = **randomResult;
    Logger logger;
    auto tmpFileResult = File::temporary_file(random);
    ASSERT_TRUE(tmpFileResult);
    auto tmpFile = std::move(*tmpFileResult);
    auto finallyGuard = make_scope_exit([&tmpFile]() { EXPECT_TRUE(tmpFile->remove()); });

    Set<std::uint32_t> keySet;
    BTreeDatabase db(logger, "TestDB", 4);
    ASSERT_TRUE(db.set_auto_commit(false));

    while (keySet.size() < testCount) {
      auto key = random.uniform_uint(0, MaxKey);
      ASSERT_TRUE(key);
      keySet.add(*key);
    }

    List<std::uint32_t> keys;
    for (std::uint32_t k : keySet) {
      for (std::uint32_t j = 0; j < writeRepeat; ++j)
        keys.push_back(k);
    }

    db.set_index_cache_size(0);
    ASSERT_TRUE(db.set_block_size(block_size));
    ASSERT_TRUE(db.set_io_device(tmpFile));
    ASSERT_TRUE(db.open());

    // record writes/reads repeated writeRepeat times randomly each cycle
    random.shuffle(keys);
    insert_all(random, db, keys);

    auto record_count = db.record_count();
    ASSERT_TRUE(record_count) << record_count.error().what();
    EXPECT_EQ(*record_count, testCount);

    random.shuffle(keys);
    check_all(db, keys);

    // Random reads/writes with randCount cycles...
    for (std::uint32_t i = 0; i < randCount; ++i) {
      List<std::uint32_t> keysTemp(keys.begin(), keys.begin() + keys.size() / 2);

      random.shuffle(keysTemp);
      auto removed = erase_all(db, keysTemp);
      ASSERT_TRUE(removed) << removed.error().what();
      ASSERT_GT(*removed, 0u);

      random.shuffle(keysTemp);
      insert_all(random, db, keysTemp);

      random.shuffle(keys);
      check_all(db, keys);
    }

    ASSERT_TRUE(db.commit());

    // Random reads/writes/rollbacks with rollbackCount cycles...
    for (std::uint32_t i = 0; i < rollbackCount ; ++i) {
      List<std::uint32_t> keysTemp(keys.begin(), keys.begin() + keys.size() / 2);
      random.shuffle(keysTemp);

      auto removed = erase_all(db, keysTemp);
      ASSERT_TRUE(removed) << removed.error().what();
      ASSERT_GT(*removed, 0u);
      ASSERT_TRUE(db.rollback());

      check_all(db, keys);
    }

    check_block_counts(db);

    // Now testing closing and reading

    ASSERT_TRUE(db.close());

    // Set the wrong value, should be set to correct value in open()
    ASSERT_TRUE(db.set_block_size(block_size + 512));

    ASSERT_TRUE(db.open());

    // Checking values...

    check_all(db, keys);

    check_block_counts(db);

    // Removing all records...

    auto totalRemoved = erase_all(db, keys);
    ASSERT_TRUE(totalRemoved) << totalRemoved.error().what();

    EXPECT_EQ(*totalRemoved, testCount);

    check_block_counts(db);

    EXPECT_TRUE(db.close());
  }
}

TEST(BTreeDatabaseTest, Consistency) {
  test_btree_database(500, 3, 5, 5, 512);

  // Test a range of block sizes to make sure there are not off by one errors
  // in maximum index / leaf size calculations.
  for (std::size_t i = 0; i < 16; ++i)
    test_btree_database(30, 2, 2, 2, 200 + i);
}

TEST(BTreeDatabaseTest, Threading) {
  auto randomResult = RandomService::create();
  ASSERT_TRUE(randomResult);
  auto& random = **randomResult;
  Logger logger;
  auto tmpFileResult = File::temporary_file(random);
  ASSERT_TRUE(tmpFileResult);
  auto tmpFile = std::move(*tmpFileResult);
  auto finallyGuard = make_scope_exit([&tmpFile]() { EXPECT_TRUE(tmpFile->remove()); });

  BTreeDatabase db(logger, "TestDB", 4);
  ASSERT_TRUE(db.set_auto_commit(false));
  ASSERT_TRUE(db.set_block_size(256));
  ASSERT_TRUE(db.set_io_device(tmpFile));
  ASSERT_TRUE(db.open());

  List<std::uint32_t> writeKeySet;
  List<std::uint32_t> deleteKeySet;

  while (writeKeySet.size() < 5000) {
    std::uint32_t key = random.next_u32();
    writeKeySet.push_back(key);
    if (random.uniform_float() > 0.3)
      deleteKeySet.push_back(key);
  }
  random.shuffle(writeKeySet);

  {
    auto writer = Thread::invoke("databaseTestWriter",
        [&random, &db, &writeKeySet]() -> std::expected<void, StarException> {
          for (std::uint32_t k : writeKeySet) {
            ByteArray val = make_block(k);
            auto inserted = db.insert(key_bytes(k), val);
            if (!inserted)
              return std::unexpected(std::move(inserted.error()));
            if (random.next_i32() % 23 == 0) {
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
            auto removed = db.erase(key_bytes(k));
            if (!removed)
              return std::unexpected(std::move(removed.error()));
            if (random.next_i32() % 23 == 0) {
              auto committed = db.commit();
              if (!committed)
                return std::unexpected(std::move(committed.error()));
            }
          }
          return {};
        });
    ASSERT_TRUE(deleter) << deleter.error().what();

    auto cache_setter = Thread::invoke("databaseTestCacheSetter",
        [&db]() -> std::expected<void, StarException> {
          for (unsigned i = 0; i < 10000; ++i) {
            db.set_index_cache_size(i % 17);
            EXPECT_EQ(db.index_cache_size(), std::max(1u, i % 17));
          }
          return {};
        });
    ASSERT_TRUE(cache_setter) << cache_setter.error().what();

    auto writerFinished = writer->finish();
    EXPECT_TRUE(writerFinished) << writerFinished.error().what();
    auto deleterFinished = deleter->finish();
    EXPECT_TRUE(deleterFinished) << deleterFinished.error().what();
    ASSERT_TRUE(writerFinished && deleterFinished);
    auto cache_setter_finished = cache_setter->finish();
    ASSERT_TRUE(cache_setter_finished) << cache_setter_finished.error().what();

    ASSERT_TRUE(db.close(false));
    ASSERT_TRUE(tmpFile->open(IOMode::Read));
    ASSERT_TRUE(db.open());

    check_block_counts(db);

    List<ThreadFunction<void>> readers;
    for (std::size_t i = 0; i < 5; ++i) {
      auto reader = Thread::invoke("databaseTestReader",
          [&db, &writeKeySet, &deleteKeySet]() -> std::expected<void, StarException> {
            for (std::uint32_t k : writeKeySet) {
              auto res = db.find(key_bytes(k));
              if (!res)
                return std::unexpected(std::move(res.error()));
              if (*res)
                EXPECT_TRUE(check_block(k, **res));
              else
                EXPECT_TRUE(deleteKeySet.contains(k));
            }
            return {};
          });
      ASSERT_TRUE(reader) << reader.error().what();
      readers.push_back(std::move(*reader));
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
  auto cleanup = make_scope_exit([&]() { EXPECT_TRUE(temporary->remove()); });
  auto device = std::make_shared<ReadBudgetFile>(temporary->file_name());
  Logger logger;
  BTreeDatabase db(logger, "TestDB", 4);
  ASSERT_TRUE(db.set_auto_commit(false));
  ASSERT_TRUE(db.set_block_size(device->block_size));
  ASSERT_TRUE(db.set_io_device(device));

  // Creating the empty root fully replaces its newly allocated block.
  device->remaining_full_reads = 0;
  auto opened = db.open();
  ASSERT_TRUE(opened) << opened.error().what();
  EXPECT_EQ(device->full_block_reads, 0u);

  // Loading the committed leaf is necessary. Reading its old payload again
  // merely to stage its complete replacement is not.
  device->remaining_full_reads = 1;
  auto inserted = db.insert(key_bytes(1), ByteArray(20, 'a'));
  ASSERT_TRUE(inserted) << inserted.error().what();
  EXPECT_EQ(device->full_block_reads, 1u);

  // Both the next leaf load and its replacement use the staged overlay.
  device->remaining_full_reads = 0;
  inserted = db.insert(key_bytes(2), ByteArray(30, 'b'));
  ASSERT_TRUE(inserted) << inserted.error().what();
  inserted = db.insert(key_bytes(1), ByteArray(40, 'c'));
  ASSERT_TRUE(inserted) << inserted.error().what();
  EXPECT_EQ(device->full_block_reads, 1u);
  ASSERT_TRUE(db.commit());
  ASSERT_TRUE(db.close());

  device->remaining_full_reads.reset();
  ASSERT_TRUE(db.open());
  auto first = db.find(key_bytes(1));
  ASSERT_TRUE(first) << first.error().what();
  ASSERT_TRUE(*first);
  EXPECT_EQ(**first, ByteArray(40, 'c'));
  auto second = db.find(key_bytes(2));
  ASSERT_TRUE(second) << second.error().what();
  ASSERT_TRUE(*second);
  EXPECT_EQ(**second, ByteArray(30, 'b'));
  EXPECT_TRUE(db.close());
}

TEST(BTreeDatabaseTest, FreeIndexStagingPreservesPayloadAndRollback) {
  auto random_result = RandomService::create();
  ASSERT_TRUE(random_result);
  auto temporary_result = File::temporary_file(**random_result);
  ASSERT_TRUE(temporary_result);
  auto temporary = std::move(*temporary_result);
  auto cleanup = make_scope_exit([&]() { EXPECT_TRUE(temporary->remove()); });
  auto device = std::make_shared<ReadBudgetFile>(temporary->file_name());
  Logger logger;
  BTreeDatabase db(logger, "TestDB", 4);
  ASSERT_TRUE(db.set_auto_commit(false));
  ASSERT_TRUE(db.set_block_size(device->block_size));
  ASSERT_TRUE(db.set_io_device(device));
  ASSERT_TRUE(db.open());
  ByteArray large_value(3 * device->block_size, 'x');
  ByteArray small_value(20, 's');
  ByteArray other_value(30, 'o');
  ASSERT_TRUE(db.insert(key_bytes(1), large_value));
  ASSERT_TRUE(db.commit());

  ASSERT_TRUE(db.insert(key_bytes(1), small_value));
  ASSERT_TRUE(db.insert(key_bytes(2), other_value));
  ASSERT_TRUE(db.rollback());
  auto original = db.find(key_bytes(1));
  ASSERT_TRUE(original) << original.error().what();
  ASSERT_TRUE(*original);
  EXPECT_EQ(**original, large_value);
  auto absent = db.find(key_bytes(2));
  ASSERT_TRUE(absent) << absent.error().what();
  EXPECT_FALSE(*absent);

  auto file_size = temporary->size();
  ASSERT_TRUE(file_size) << file_size.error().what();
  auto before = temporary->read_bytes_absolute(0, static_cast<std::size_t>(*file_size));
  ASSERT_TRUE(before) << before.error().what();
  ASSERT_TRUE(db.insert(key_bytes(1), small_value));
  ASSERT_TRUE(db.insert(key_bytes(2), other_value));

  // Shrinking the leaf frees tail blocks. Staging the whole free-index block
  // acquires the old payload once and preserves every untouched padding byte.
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
  auto first = db.find(key_bytes(1));
  ASSERT_TRUE(first) << first.error().what();
  ASSERT_TRUE(*first);
  EXPECT_EQ(**first, small_value);
  auto second = db.find(key_bytes(2));
  ASSERT_TRUE(second) << second.error().what();
  ASSERT_TRUE(*second);
  EXPECT_EQ(**second, other_value);
  check_block_counts(db);
  EXPECT_TRUE(db.close());
}

TEST(BTreeDatabaseTest, CommitPhaseFailuresPreservePendingDataAndRecover) {
  struct Fault {
    unsigned phase;
    bool write;
    bool partial_root;
  };
  std::array const faults{Fault{0, true, false},  Fault{0, false, false}, Fault{1, true, true},
                          Fault{1, false, false}, Fault{2, true, false},  Fault{2, false, false}};

  for (auto fault : faults) {
    for (bool retry : {false, true}) {
      SCOPED_TRACE(::testing::Message() << "phase=" << fault.phase << " write=" << fault.write
                                        << " partial=" << fault.partial_root << " retry=" << retry);
      auto random_result = RandomService::create();
      ASSERT_TRUE(random_result);
      auto temporary_result = File::temporary_file(**random_result);
      ASSERT_TRUE(temporary_result);
      auto temporary = std::move(*temporary_result);
      auto cleanup = make_scope_exit([&]() { EXPECT_TRUE(temporary->remove()); });
      auto device = std::make_shared<CommitFaultFile>(temporary->file_name());
      Logger logger;
      BTreeDatabase db(logger, "TestDB", 4);
      ASSERT_TRUE(db.set_auto_commit(false));
      ASSERT_TRUE(db.set_io_device(device));
      ASSERT_TRUE(db.open());
      ByteArray original(20, 'o');
      ByteArray changed(24, 'c');
      ByteArray retained(20, 'r');
      ByteArray added(20, 'a');
      ASSERT_TRUE(db.insert(key_bytes(0), original));
      ASSERT_TRUE(db.insert(key_bytes(1), retained));
      ASSERT_TRUE(db.commit());

      // Bytes 67..511 are reserved, not part of either 17-byte root. A root
      // write/retry must preserve nonzero legacy padding as well as the prefix.
      std::array<char, 512 - 67> padding;
      padding.fill('\x5a');
      ASSERT_TRUE(temporary->write_full_absolute(67, padding));
      ASSERT_TRUE(temporary->sync());
      auto before = temporary->read_bytes_absolute(0, 512);
      ASSERT_TRUE(before) << before.error().what();
      ASSERT_TRUE(db.insert(key_bytes(0), changed));
      ASSERT_TRUE(db.insert(key_bytes(2), added));
      device->arm(fault.phase, fault.write, fault.partial_root);
      auto committed = db.commit();
      ASSERT_FALSE(committed);
      EXPECT_TRUE(committed.error().is<IOException>());
      EXPECT_FALSE(device->armed);
      EXPECT_TRUE(db.is_open());
      auto pending = db.find(key_bytes(0));
      ASSERT_TRUE(pending) << pending.error().what();
      ASSERT_TRUE(*pending);
      EXPECT_EQ(**pending, changed);
      auto pending_added = db.find(key_bytes(2));
      ASSERT_TRUE(pending_added) << pending_added.error().what();
      ASSERT_TRUE(*pending_added);
      EXPECT_EQ(**pending_added, added);

      auto failed_header = temporary->read_bytes_absolute(0, 512);
      ASSERT_TRUE(failed_header) << failed_header.error().what();
      bool selector_was_written = fault.phase == 2 && !fault.write;
      EXPECT_EQ((*failed_header)[32] != (*before)[32], selector_was_written);
      EXPECT_EQ(ByteArray(failed_header->span().first(32)), ByteArray(before->span().first(32)));
      EXPECT_EQ(ByteArray(failed_header->span().subspan(67)), ByteArray(padding));
      auto old_root_offset = (*before)[32] ? 50u : 33u;
      EXPECT_EQ(ByteArray(failed_header->span().subspan(old_root_offset, 17)),
                ByteArray(before->span().subspan(old_root_offset, 17)));

      if (retry) {
        ASSERT_TRUE(db.commit());
      } else {
        // Failed synchronization is uncertain durability, not a promise of
        // old-value visibility: reused leaf blocks may already be overwritten.
        ASSERT_TRUE(db.rollback());
      }
      ASSERT_TRUE(db.close());
      ASSERT_TRUE(db.open());
      auto recovered = db.find(key_bytes(0));
      ASSERT_TRUE(recovered) << recovered.error().what();
      ASSERT_TRUE(*recovered);
      if (retry)
        EXPECT_EQ(**recovered, changed);
      else
        EXPECT_TRUE(**recovered == original || **recovered == changed);
      auto recovered_added = db.find(key_bytes(2));
      ASSERT_TRUE(recovered_added) << recovered_added.error().what();
      if (retry)
        ASSERT_TRUE(*recovered_added);
      if (*recovered_added)
        EXPECT_EQ(**recovered_added, added);
      auto recovered_retained = db.find(key_bytes(1));
      ASSERT_TRUE(recovered_retained) << recovered_retained.error().what();
      ASSERT_TRUE(*recovered_retained);
      EXPECT_EQ(**recovered_retained, retained);
      check_block_counts(db);
      ASSERT_TRUE(db.insert(key_bytes(3), make_block(3)));
      ASSERT_TRUE(db.close());
      ASSERT_TRUE(db.open());
      auto subsequent = db.find(key_bytes(3));
      ASSERT_TRUE(subsequent) << subsequent.error().what();
      ASSERT_TRUE(*subsequent);
      EXPECT_EQ(**subsequent, make_block(3));
      auto final_padding = temporary->read_bytes_absolute(67, padding.size());
      ASSERT_TRUE(final_padding) << final_padding.error().what();
      EXPECT_EQ(*final_padding, ByteArray(padding));
      EXPECT_TRUE(db.close());
    }
  }
}

TEST(BTreeDatabaseTest, FailedCommitIsReportedWithoutDestructorRetry) {
  Logger logger;
  auto device = std::make_shared<FailingSyncBuffer>();
  auto db = std::make_unique<BTreeDatabase>(logger, "TestDB", 4);
  ASSERT_TRUE(db->set_auto_commit(false));
  ASSERT_TRUE(db->set_io_device(device));
  ASSERT_TRUE(db->open());
  ASSERT_TRUE(db->insert(key_bytes(0), make_block(0)));
  ASSERT_TRUE(db->commit());
  ASSERT_TRUE(db->insert(key_bytes(1), make_block(1)));
  device->failSync = true;

  auto committedResult = db->commit();
  ASSERT_FALSE(committedResult);
  EXPECT_TRUE(committedResult.error().is<IOException>());
  EXPECT_TRUE(db->is_open());
  unsigned callsAfterFailure = device->syncCalls;
  db.reset();
  EXPECT_EQ(device->syncCalls, callsAfterFailure);

  // sync() failure reports uncertain durability, not an undo of writes that
  // already reached the device.  The existing allocator may reuse a freed
  // leaf block before write_root(), so even the old root selector can expose
  // the new complete leaf.  Require a consistent, usable recovery, not a
  // particular visibility outcome for the failed transaction.
  device->failSync = false;
  BTreeDatabase recovered(logger);
  ASSERT_TRUE(recovered.set_io_device(device));
  ASSERT_TRUE(recovered.open());
  auto committed = recovered.find(key_bytes(0));
  ASSERT_TRUE(committed) << committed.error().what();
  ASSERT_TRUE(*committed);
  EXPECT_EQ(**committed, make_block(0));
  auto uncertain = recovered.find(key_bytes(1));
  ASSERT_TRUE(uncertain) << uncertain.error().what();
  if (*uncertain)
    EXPECT_EQ(**uncertain, make_block(1));
  auto recoveredCount = *uncertain ? 2u : 1u;
  auto record_count = recovered.record_count();
  ASSERT_TRUE(record_count) << record_count.error().what();
  EXPECT_EQ(*record_count, recoveredCount);
  check_block_counts(recovered);

  ASSERT_TRUE(recovered.insert(key_bytes(2), make_block(2)));
  ASSERT_TRUE(recovered.close());
  ASSERT_TRUE(recovered.open());
  auto subsequent = recovered.find(key_bytes(2));
  ASSERT_TRUE(subsequent) << subsequent.error().what();
  ASSERT_TRUE(*subsequent);
  EXPECT_EQ(**subsequent, make_block(2));
  auto subsequentCount = recovered.record_count();
  ASSERT_TRUE(subsequentCount) << subsequentCount.error().what();
  EXPECT_EQ(*subsequentCount, recoveredCount + 1);
  EXPECT_TRUE(recovered.close());
}

TEST(BTreeDatabaseTest, FailedCloseIsReportedWithoutDestructorRetry) {
  Logger logger;
  auto device = std::make_shared<FailingSyncBuffer>();
  auto db = std::make_unique<BTreeDatabase>(logger, "TestDB", 4);
  ASSERT_TRUE(db->set_auto_commit(false));
  ASSERT_TRUE(db->set_io_device(device));
  ASSERT_TRUE(db->open());
  ASSERT_TRUE(db->insert(key_bytes(1), make_block(1)));
  device->failSync = true;

  auto closed = db->close(true);
  ASSERT_FALSE(closed);
  EXPECT_TRUE(closed.error().is<IOException>());
  EXPECT_TRUE(db->is_open());
  EXPECT_TRUE(device->is_open());
  unsigned callsAfterFailure = device->syncCalls;
  db.reset();
  EXPECT_EQ(device->syncCalls, callsAfterFailure);
}

TEST(BTreeDatabaseTest, FailedDeviceCloseIsConsumedOnce) {
  Logger logger;
  auto device = std::make_shared<FailingCloseBuffer>();
  BTreeDatabase db(logger, "TestDB", 4);
  ASSERT_TRUE(db.set_io_device(device));
  ASSERT_TRUE(db.open());

  auto closed = db.close(true);
  ASSERT_FALSE(closed);
  EXPECT_TRUE(closed.error().is<IOException>());
  EXPECT_FALSE(db.is_open());
  EXPECT_FALSE(device->is_open());
  EXPECT_TRUE(db.close(true));
  EXPECT_EQ(device->closeCalls, 1u);
}

TEST(BTreeDatabaseTest, IndexInsertionsRespectClampedCacheCapacity) {
  auto random_result = RandomService::create();
  ASSERT_TRUE(random_result);
  auto temporary_result = File::temporary_file(**random_result);
  ASSERT_TRUE(temporary_result);
  auto temporary = std::move(*temporary_result);
  auto cleanup = make_scope_exit([&]() { EXPECT_TRUE(temporary->remove()); });
  auto device = std::make_shared<ReadBudgetFile>(temporary->file_name());
  device->block_size = 64;
  Logger logger;
  BTreeDatabase db(logger, "TestDB", 4);
  ASSERT_TRUE(db.set_auto_commit(false));
  ASSERT_TRUE(db.set_block_size(device->block_size));
  ASSERT_TRUE(db.set_io_device(device));
  db.set_index_cache_size(0);
  EXPECT_EQ(db.index_cache_size(), 1u);
  ASSERT_TRUE(db.open());
  for (std::uint32_t i = 0; i < 512; ++i)
    ASSERT_TRUE(db.insert(key_bytes(i), ByteArray(8, 'v')));
  ASSERT_TRUE(db.commit());
  auto levels = db.index_levels();
  ASSERT_TRUE(levels) << levels.error().what();
  ASSERT_GE(*levels, 2u);

  // The last written path cannot remain entirely cached at capacity one.
  // Count real disk reads, not private cache state or implementation hooks.
  device->full_block_reads = 0;
  auto found = db.find(key_bytes(511));
  ASSERT_TRUE(found) << found.error().what();
  ASSERT_TRUE(*found);
  EXPECT_EQ(**found, ByteArray(8, 'v'));
  auto bounded_reads = device->full_block_reads;

  db.set_index_cache_size(4096);
  ASSERT_TRUE(db.find(key_bytes(511)));
  device->full_block_reads = 0;
  found = db.find(key_bytes(511));
  ASSERT_TRUE(found) << found.error().what();
  ASSERT_TRUE(*found);
  // Leaf overflow blocks are deliberately unchanged by the cache policy.
  EXPECT_LT(device->full_block_reads, bounded_reads);
  ASSERT_TRUE(db.close());
  ASSERT_TRUE(db.open());
  found = db.find(key_bytes(511));
  ASSERT_TRUE(found) << found.error().what();
  ASSERT_TRUE(*found);
  EXPECT_EQ(**found, ByteArray(8, 'v'));
  EXPECT_TRUE(db.close());
}

TEST(BTreeDatabaseTest, BorrowedVisitorsEmptyValuesAndRangeErase) {
  Logger logger;
  auto device = std::make_shared<Buffer>();
  BTreeDatabase db(logger, "TestDB", 4);
  ASSERT_TRUE(db.set_auto_commit(false));
  ASSERT_TRUE(db.set_block_size(128));
  ASSERT_TRUE(db.set_io_device(device));
  ASSERT_TRUE(db.open());
  List<std::pair<ByteArray, ByteArray>> expected;
  for (std::uint32_t i = 0; i < 48; ++i) {
    auto key = key_bytes(i);
    auto value = ByteArray(i % 3 == 0 ? 0 : (i % 3 == 1 ? 127 : 128), static_cast<char>(i));
    ASSERT_TRUE(db.insert(key, value));
    expected.emplace_back(std::move(key), std::move(value));
  }
  ASSERT_TRUE(db.insert(key_bytes(0), ByteArray(409, 'w')));
  ASSERT_TRUE(db.insert(key_bytes(0), ByteArray()));
  ASSERT_TRUE(db.commit());
  ASSERT_TRUE(db.close());
  ASSERT_TRUE(db.open());

  List<std::pair<ByteArray, ByteArray>> visited;
  ASSERT_TRUE(db.for_all([&](ByteArray const& key, ByteArray const& value) -> std::expected<void, StarException> {
    visited.emplace_back(key, value);
    return {};
  }));
  EXPECT_EQ(visited, expected);
  auto empty_value = db.find(key_bytes(0));
  ASSERT_TRUE(empty_value) << empty_value.error().what();
  ASSERT_TRUE(*empty_value);
  EXPECT_TRUE((**empty_value).empty());

  unsigned calls = 0;
  auto fail = [&](ByteArray const&, ByteArray const&) -> std::expected<void, StarException> {
    ++calls;
    return std::unexpected(IOException("Record visitor stopped"));
  };
  auto stopped = db.for_all(fail);
  ASSERT_FALSE(stopped);
  EXPECT_TRUE(stopped.error().is<IOException>());
  EXPECT_EQ(calls, 1u);
  calls = 0;
  stopped = db.for_each(key_bytes(10), key_bytes(30), fail);
  ASSERT_FALSE(stopped);
  EXPECT_TRUE(stopped.error().is<IOException>());
  EXPECT_EQ(calls, 1u);
  calls = 0;
  unsigned errors = 0;
  stopped = db.recover_all(fail, [&](String const&, ExceptionDiagnostic const&) -> std::expected<void, StarException> {
    ++errors;
    return std::unexpected(IOException("Recovery handler stopped"));
  });
  ASSERT_FALSE(stopped);
  EXPECT_TRUE(stopped.error().is<IOException>());
  EXPECT_EQ(calls, 1u);
  EXPECT_EQ(errors, 1u);
  EXPECT_FALSE(db.for_all({}));
  EXPECT_FALSE(db.for_each(key_bytes(0), key_bytes(48), {}));
  EXPECT_FALSE(db.recover_all({}, {}));

  auto erased = db.erase(key_bytes(8), key_bytes(40));
  ASSERT_TRUE(erased) << erased.error().what();
  ASSERT_EQ(erased->size(), 32u);
  for (std::uint32_t i = 0; i < erased->size(); ++i)
    EXPECT_EQ((*erased)[i], key_bytes(i + 8));
  // Visitor-owned copies remain independent of all later node mutations.
  EXPECT_EQ(visited, expected);
  ASSERT_TRUE(db.rollback());
  auto restored = db.find(key_bytes(8), key_bytes(40));
  ASSERT_TRUE(restored) << restored.error().what();
  EXPECT_EQ(restored->size(), 32u);
  auto all_erased = db.erase(key_bytes(0), key_bytes(48));
  ASSERT_TRUE(all_erased) << all_erased.error().what();
  EXPECT_EQ(all_erased->size(), 48u);
  auto records = db.record_count();
  ASSERT_TRUE(records) << records.error().what();
  EXPECT_EQ(*records, 0u);
  ASSERT_TRUE(db.close());
  ASSERT_TRUE(db.open());
  records = db.record_count();
  ASSERT_TRUE(records) << records.error().what();
  EXPECT_EQ(*records, 0u);
  EXPECT_TRUE(db.close());
}

TEST(BTreeDatabaseTest, RangeEraseRetainsPerKeyCommitFailures) {
  Logger logger;
  auto device = std::make_shared<FailingSyncBuffer>();
  BTreeDatabase db(logger, "TestDB", 4);
  ASSERT_TRUE(db.set_io_device(device));
  ASSERT_TRUE(db.open());
  for (std::uint32_t i = 0; i < 3; ++i)
    ASSERT_TRUE(db.insert(key_bytes(i), ByteArray()));
  device->failSync = true;
  auto erased = db.erase(key_bytes(0), key_bytes(3));
  ASSERT_FALSE(erased);
  EXPECT_TRUE(erased.error().is<IOException>());
  EXPECT_TRUE(db.is_open());
  auto first = db.contains(key_bytes(0));
  ASSERT_TRUE(first) << first.error().what();
  EXPECT_FALSE(*first);
  for (std::uint32_t i = 1; i < 3; ++i) {
    auto remaining = db.find(key_bytes(i));
    ASSERT_TRUE(remaining) << remaining.error().what();
    ASSERT_TRUE(*remaining);
    EXPECT_TRUE((**remaining).empty());
  }
  device->failSync = false;
  ASSERT_TRUE(db.commit());
  ASSERT_TRUE(db.close());
  ASSERT_TRUE(db.open());
  first = db.contains(key_bytes(0));
  ASSERT_TRUE(first) << first.error().what();
  EXPECT_FALSE(*first);
  auto count = db.record_count();
  ASSERT_TRUE(count) << count.error().what();
  EXPECT_EQ(*count, 2u);
  EXPECT_TRUE(db.close());
}

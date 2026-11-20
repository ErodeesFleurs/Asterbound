import std;
import star.format;
import star.logging;
import star.map;
import star.string;
import star.thread;

#include "gtest/gtest.h"

using namespace Star;

TEST(LogMapTest, SortedSnapshotsAndFailedDrainsPreserveNewWrites) {
  LogMap log_map;
  log_map.set_value("z", "initial");
  log_map.set_value("a", "retry");
  auto snapshot = log_map.snapshot();
  ASSERT_EQ(snapshot.size(), 2u);
  EXPECT_EQ(snapshot.begin()->first, "a");
  EXPECT_EQ(snapshot.rbegin()->first, "z");

  log_map.set_value("z", "before drain");
  auto drained = log_map.drain();
  ASSERT_EQ(drained.size(), 2u);
  EXPECT_EQ(drained.begin()->first, "a");
  EXPECT_EQ(drained.rbegin()->second, "before drain");
  EXPECT_EQ(snapshot.value("z"), "initial");
  EXPECT_TRUE(log_map.snapshot().empty());

  log_map.set_value("z", "after drain");
  log_map.set_value("b", "new key");
  log_map.restore_missing(drained);
  Map<String, String> expected{{"a", "retry"}, {"b", "new key"}, {"z", "after drain"}};
  EXPECT_EQ(log_map.drain(), expected);
  EXPECT_TRUE(log_map.drain().empty());
}

TEST(LogMapTest, ConcurrentWriterAndDrainsAccountForEveryKeyOnce) {
  LogMap log_map;
  constexpr unsigned rounds = 256;
  std::barrier rendezvous(2);
  auto writer = ThreadFunction<void>::create([&]() -> std::expected<void, StarException> {
    for (unsigned i = 0; i < rounds; ++i) {
      rendezvous.arrive_and_wait();
      log_map.set_value(Star::format("k{}", i), "written");
    }
    return {};
  }, "LogMapWriter");
  ASSERT_TRUE(writer) << writer.error().what();

  Map<String, String> collected;
  for (unsigned i = 0; i < rounds; ++i) {
    rendezvous.arrive_and_wait();
    EXPECT_TRUE(collected.merge(log_map.drain()));
  }
  ASSERT_TRUE(writer->finish());
  EXPECT_TRUE(collected.merge(log_map.drain()));
  EXPECT_EQ(collected.size(), rounds);
  for (unsigned i = 0; i < rounds; ++i)
    EXPECT_EQ(collected.value(Star::format("k{}", i)), "written");
}

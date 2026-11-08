#ifndef STAR_SYSTEM_FAMILY_WINDOWS
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

import std;
import star.set;
import star.list;
import star.memory;
import star.format;
import star.string;
import star.static_vector;
import star.map;
import star.optional;
import star.id_map;
import star.bi_map;
import star.data_stream;
import star.data_stream_devices;
import star.byte_array;
import star.file;
import star.io_device;
import star.random;

#include "gtest/gtest.h"

using namespace Star;

TEST(SetTest, All) {
  Set<int> a = {1, 2, 3, 4};
  Set<int> b = {2, 4, 5};
  EXPECT_EQ(a.difference(b), Set<int>({1, 3}));
  EXPECT_EQ(a.intersection(b), Set<int>({2, 4}));
  EXPECT_EQ(a.combination(b), Set<int>({1, 2, 3, 4, 5}));
}

namespace {

template <typename Container>
void expectFilteredCopyContract(Container const& input) {
  std::vector<int> const original{1, 2, 1, 3};
  auto sorted = input.sorted();
  static_assert(std::same_as<decltype(sorted), Container>);
  EXPECT_EQ(std::vector<int>(sorted.begin(), sorted.end()), (std::vector<int>{1, 1, 2, 3}));
  EXPECT_EQ(std::vector<int>(input.begin(), input.end()), original);
  std::vector<int> visited;
  auto filtered = input.filtered([&](int value) {
    visited.push_back(value);
    return value == 1;
  });
  static_assert(std::same_as<decltype(filtered), Container>);
  EXPECT_EQ(std::vector<int>(filtered.begin(), filtered.end()), (std::vector<int>{1, 1}));
  EXPECT_EQ(visited, original);
  EXPECT_EQ(std::vector<int>(input.begin(), input.end()), original);

  auto modified = input.filtered([](int& value) {
    value *= 10;
    return value != 20;
  });
  EXPECT_EQ(std::vector<int>(modified.begin(), modified.end()), (std::vector<int>{10, 10, 30}));
  EXPECT_EQ(std::vector<int>(input.begin(), input.end()), original);

  visited.clear();
  auto accepted = input.filtered([&](int value) -> std::expected<bool, StarException> {
    visited.push_back(value);
    return value == 1;
  });
  static_assert(std::same_as<decltype(accepted), std::expected<Container, StarException>>);
  ASSERT_TRUE(accepted);
  EXPECT_EQ(std::vector<int>(accepted->begin(), accepted->end()), (std::vector<int>{1, 1}));
  EXPECT_EQ(visited, original);
  EXPECT_EQ(std::vector<int>(input.begin(), input.end()), original);

  visited.clear();
  auto failed = input.filtered([&](int& value) -> std::expected<bool, StarException> {
    visited.push_back(value);
    if (value == 2)
      return std::unexpected(OutOfRangeException("filtered first error", false));
    if (value == 3)
      return std::unexpected(OutOfRangeException("filtered later error", false));
    value += 10;
    return false;
  });
  ASSERT_FALSE(failed);
  EXPECT_TRUE(failed.error().template is<OutOfRangeException>());
  EXPECT_NE(std::string_view(failed.error().what()).find("filtered first error"), std::string_view::npos);
  EXPECT_EQ(visited, (std::vector<int>{1, 2}));
  EXPECT_EQ(std::vector<int>(input.begin(), input.end()), original);

  Container const empty;
  int calls = 0;
  auto emptyFiltered = empty.filtered([&](int) {
    ++calls;
    return true;
  });
  EXPECT_TRUE(emptyFiltered.empty());
  auto emptyAccepted = empty.filtered([&](int) -> std::expected<bool, StarException> {
    ++calls;
    return std::unexpected(OutOfRangeException("empty predicate must not run", false));
  });
  ASSERT_TRUE(emptyAccepted);
  EXPECT_TRUE(emptyAccepted->empty());
  EXPECT_EQ(calls, 0);
}

template <typename Container>
void expectMoveOnlyFilterContract() {
  Container input;
  std::vector<int*> original;
  for (int value : {1, 2, 3, 4, 5}) {
    auto owned = std::make_unique<int>(value);
    original.push_back(owned.get());
    if constexpr (std::is_void_v<decltype(input.push_back(std::move(owned)))>) {
      input.push_back(std::move(owned));
    } else {
      auto added = input.push_back(std::move(owned));
      ASSERT_TRUE(added);
    }
  }
  input.filter([](std::unique_ptr<int> const& value) { return *value % 2 != 0; });
  std::vector<int*> retained;
  for (auto const& value : input)
    retained.push_back(value.get());
  EXPECT_EQ(retained, (std::vector<int*>{original[0], original[2], original[4]}));
}

struct RemovalProbe {
  int value;
  std::reference_wrapper<int> destructions;
  std::reference_wrapper<std::vector<int>> comparisons;

  ~RemovalProbe() { ++destructions.get(); }

  friend bool operator==(RemovalProbe const& left, RemovalProbe const& right) {
    left.comparisons.get().push_back(left.destructions.get());
    return left.value == right.value;
  }
};

}

TEST(ListTest, FilteredCopyPreservesContainerAndPredicateContract) {
  ASSERT_NO_FATAL_FAILURE(expectFilteredCopyContract(List<int>{1, 2, 1, 3}));
  auto bounded = StaticList<int, 4>::create({1, 2, 1, 3});
  ASSERT_TRUE(bounded);
  ASSERT_NO_FATAL_FAILURE(expectFilteredCopyContract(*bounded));
  ASSERT_NO_FATAL_FAILURE(expectFilteredCopyContract(SmallList<int, 2>{1, 2, 1, 3}));
  ASSERT_NO_FATAL_FAILURE(expectFilteredCopyContract(Deque<int>{1, 2, 1, 3}));
  ASSERT_NO_FATAL_FAILURE(expectFilteredCopyContract(LinkedList<int>{1, 2, 1, 3}));
}

TEST(ListTest, FilteringRetainsMoveOnlyOwnershipAndStableOrder) {
  expectMoveOnlyFilterContract<List<std::unique_ptr<int>>>();
  expectMoveOnlyFilterContract<StaticList<std::unique_ptr<int>, 5>>();
  expectMoveOnlyFilterContract<SmallList<std::unique_ptr<int>, 2>>();
  expectMoveOnlyFilterContract<Deque<std::unique_ptr<int>>>();
  expectMoveOnlyFilterContract<LinkedList<std::unique_ptr<int>>>();
}

TEST(ListTest, RemovalObservesAliasingAndImmediateDestruction) {
  List<int> aliased{1, 2, 1};
  EXPECT_EQ(aliased.remove(aliased[0]), 3u);
  EXPECT_TRUE(aliased.empty());

  int destructions = 0;
  std::vector<int> comparisons;
  List<RemovalProbe> input;
  input.reserve(4);
  for (int value : {1, 2, 1, 3})
    input.emplace_back(value, std::ref(destructions), std::ref(comparisons));
  RemovalProbe key{1, std::ref(destructions), std::ref(comparisons)};
  EXPECT_EQ(input.remove(key), 2u);
  EXPECT_EQ(comparisons, (std::vector<int>{0, 1, 1, 2}));
  EXPECT_EQ(destructions, 2);
  ASSERT_EQ(input.size(), 2u);
  EXPECT_EQ(input[0].value, 2);
  EXPECT_EQ(input[1].value, 3);
}

TEST(ListTest, BoundedAppendConsumesInputOnceAndRetainsSuccessfulPrefix) {
  StaticList<int, 2> input;
  std::istringstream stream("1 2 3 4");
  auto values = std::ranges::istream_view<int>(stream);
  auto appended = input.append_range(values);
  ASSERT_FALSE(appended);
  EXPECT_TRUE(appended.error().is<StaticVectorSizeException>());
  EXPECT_EQ(std::vector<int>(input.begin(), input.end()), (std::vector<int>{1, 2}));
  int next = 0;
  stream >> next;
  EXPECT_EQ(next, 4);
}

TEST(ListTest, BoundedEmplacementRejectsBeforeMovingArguments) {
  StaticList<std::unique_ptr<int>, 1> input;
  ASSERT_TRUE(input.push_back(std::make_unique<int>(1)));
  auto pending = std::make_unique<int>(2);
  auto rejected = input.emplace_back(std::move(pending));
  ASSERT_FALSE(rejected);
  ASSERT_TRUE(pending);
  EXPECT_EQ(*pending, 2);
  ASSERT_EQ(input.size(), 1u);
  EXPECT_EQ(*input[0], 1);
}

TEST(ListTest, StringCopiesRetainConcreteTypeAndSourceValues) {
  StringList const input{"b", "a", "b"};
  auto filtered = input.filtered([](String& value) {
    value.append("!");
    return value != "a!";
  });
  static_assert(std::same_as<decltype(filtered), StringList>);
  EXPECT_EQ(filtered.join(",").utf8(), "b!,b!");
  EXPECT_EQ(input.join(",").utf8(), "b,a,b");
  auto sorted = input.sorted();
  static_assert(std::same_as<decltype(sorted), StringList>);
  EXPECT_EQ(sorted.join(",").utf8(), "a,b,b");
  EXPECT_EQ(input.join(",").utf8(), "b,a,b");
}

TEST(ListTest, OwningSequencesOptInWithoutStandardContainerInheritance) {
  static_assert(SequenceContainer<List<int>>);
  static_assert(SequenceContainer<StaticList<int, 4>>);
  static_assert(SequenceContainer<SmallList<int, 2>>);
  static_assert(SequenceContainer<Deque<int>>);
  static_assert(SequenceContainer<LinkedList<int>>);
  static_assert(SequenceContainer<StringList>);
  static_assert(!SequenceContainer<std::vector<int>>);
  static_assert(!std::is_base_of_v<std::vector<int>, List<int>>);
  static_assert(!std::is_base_of_v<std::deque<int>, Deque<int>>);
  static_assert(!std::is_base_of_v<std::list<int>, LinkedList<int>>);

  List<int> list{1, 2, 1};
  SmallList<int, 2> small{1, 2, 1};
  Deque<int> deque{1, 2, 1};
  LinkedList<int> linked{1, 2, 1};
  auto bounded = StaticList<int, 3>::create({1, 2, 1});
  ASSERT_TRUE(bounded);
  EXPECT_EQ(hash<List<int>>{}(list), (hash<SmallList<int, 2>>{}(small)));
  EXPECT_EQ(hash<List<int>>{}(list), hash<Deque<int>>{}(deque));
  EXPECT_EQ(hash<List<int>>{}(list), hash<LinkedList<int>>{}(linked));
  EXPECT_EQ(hash<List<int>>{}(list), (hash<StaticList<int, 3>>{}(*bounded)));
  EXPECT_EQ(strf("{}", list), "(1, 2, 1)");
  EXPECT_EQ(strf("{}", small), "(1, 2, 1)");
  EXPECT_EQ(strf("{}", deque), "(1, 2, 1)");
  EXPECT_EQ(strf("{}", linked), "(1, 2, 1)");
  EXPECT_EQ(strf("{}", *bounded), "(1, 2, 1)");
  EXPECT_EQ(strf("{}", StringList{"a", "b"}), "('a', 'b')");
}

TEST(ListTest, IndexedQueriesRetainDefaultArguments) {
  auto expectDefaults = []<typename Sequence>(Sequence const& input) {
    EXPECT_EQ(input.get(1), 2);
    EXPECT_EQ(input.get(99), 0);
    EXPECT_EQ(input.get(99, 7), 7);
    EXPECT_EQ(input.index_of(1), 0u);
    EXPECT_EQ(input.last_index_of(1), 2u);
    EXPECT_EQ(input.index_of(1, 1), 2u);
    EXPECT_EQ(input.last_index_of(1, 2), 0u);
  };
  expectDefaults(List<int>{1, 2, 1});
  expectDefaults(SmallList<int, 2>{1, 2, 1});
  expectDefaults(Deque<int>{1, 2, 1});
  auto bounded = StaticList<int, 3>::create({1, 2, 1});
  ASSERT_TRUE(bounded);
  expectDefaults(*bounded);
}

TEST(ListTest, IndexOfPreservesTypedFirstMatchAndBounds) {
  auto expectSearch = []<typename Sequence>(Sequence const& input) {
    double needle = 1.5;
    EXPECT_EQ(input.index_of(needle), 0u);
    EXPECT_EQ(input.index_of(needle, 1), 2u);
    EXPECT_EQ(input.index_of(needle, 2), 2u);
    EXPECT_EQ(input.index_of(2, 2), NPos);
    EXPECT_EQ(input.index_of(9), NPos);
    EXPECT_EQ(input.index_of(1, input.size()), NPos);
    EXPECT_EQ(input.index_of(1, input.size() + 1), NPos);
    EXPECT_EQ(input.index_of(1, NPos), NPos);

    Sequence const empty;
    EXPECT_EQ(empty.index_of(1), NPos);
    EXPECT_EQ(empty.index_of(1, 1), NPos);
    EXPECT_EQ(empty.index_of(1, NPos), NPos);
  };
  expectSearch(List<int>{1, 2, 1});
  expectSearch(SmallList<int, 2>{1, 2, 1});
  expectSearch(Deque<int>{1, 2, 1});
  auto bounded = StaticList<int, 3>::create({1, 2, 1});
  ASSERT_TRUE(bounded);
  expectSearch(*bounded);
}

TEST(ContainerBorrowTest, ReferenceEntriesRemainRebindableSlots) {
  int original = 10;
  int replacement = 20;
  Map<int, std::reference_wrapper<int>> entries;
  auto added = entries.add(1, std::ref(original));
  ASSERT_TRUE(added);
  added->get() = std::ref(replacement);
  EXPECT_EQ(entries.begin()->second.get(), 20);
  EXPECT_EQ(original, 10);

  auto checked = entries.get(1);
  ASSERT_TRUE(checked);
  checked->get() = std::ref(original);
  EXPECT_EQ(entries.begin()->second.get(), 10);
  auto nullable = entries.find_ref(1);
  ASSERT_TRUE(nullable);
  nullable->get() = std::ref(replacement);
  replacement = 21;
  auto const& constEntries = entries;
  auto observed = constEntries.at(1);
  ASSERT_TRUE(observed);
  EXPECT_EQ(observed->get().get(), 21);
  auto nullableObserved = constEntries.find_ref(1);
  ASSERT_TRUE(nullableObserved);
  EXPECT_EQ(nullableObserved->get().get(), 21);
  EXPECT_EQ(original, 10);
  EXPECT_FALSE(entries.find_ref(2));
  EXPECT_FALSE(constEntries.find_ref(2));
  auto missing = entries.get(2);
  ASSERT_FALSE(missing);
  EXPECT_TRUE(missing.error().is<MapException>());
  EXPECT_EQ(entries.size(), 1u);

  List<std::reference_wrapper<int>> sequence{std::ref(original)};
  auto slot = sequence.at(0);
  ASSERT_TRUE(slot);
  slot->get() = std::ref(replacement);
  replacement = 22;
  auto const& constSequence = sequence;
  auto first = constSequence.first();
  ASSERT_TRUE(first);
  EXPECT_EQ(first->get().get(), 22);
  EXPECT_EQ(sequence[0].get(), 22);
  EXPECT_EQ(original, 10);
  auto outOfRange = sequence.at(1);
  ASSERT_FALSE(outOfRange);
  EXPECT_TRUE(outOfRange.error().is<OutOfRangeException>());
  EXPECT_EQ(sequence.size(), 1u);

  std::optional<std::reference_wrapper<int>> optional{std::ref(original)};
  auto optionalSlot = optionalRef(optional);
  ASSERT_TRUE(optionalSlot);
  optionalSlot->get() = std::ref(replacement);
  replacement = 23;
  ASSERT_TRUE(optional);
  EXPECT_EQ(optional->get(), 23);
  auto const& constOptional = optional;
  auto optionalObserved = optionalRef(constOptional);
  ASSERT_TRUE(optionalObserved);
  EXPECT_EQ(optionalObserved->get().get(), 23);
  EXPECT_EQ(original, 10);
  optional.reset();
  EXPECT_FALSE(optionalRef(optional));
  EXPECT_FALSE(optionalRef(constOptional));
}

TEST(IdMapTest, InclusiveNarrowRangesExhaustAndRecycleWithoutNarrowing) {
  auto check = []<typename Id, template <typename, typename> typename Container>() {
    Container<Id, int> map;
    for (int value = std::numeric_limits<Id>::lowest(); value <= std::numeric_limits<Id>::max(); ++value) {
      auto added = map.add(value);
      ASSERT_TRUE(added);
      EXPECT_EQ(*added, static_cast<Id>(value));
    }
    auto full = map.next_id();
    ASSERT_FALSE(full);
    EXPECT_TRUE(full.error().template is<IdMapException>());
    EXPECT_TRUE(map.remove(static_cast<Id>(0)));
    auto recycled = map.add(999);
    ASSERT_TRUE(recycled);
    EXPECT_EQ(*recycled, static_cast<Id>(0));
    EXPECT_FALSE(map.add(1000));
    map.clear();
    EXPECT_TRUE(map.empty());
    auto reset = map.next_id();
    ASSERT_TRUE(reset);
    EXPECT_EQ(*reset, std::numeric_limits<Id>::lowest());
  };
  check.template operator()<std::uint8_t, IdMap>();
  check.template operator()<std::int8_t, IdMap>();
  check.template operator()<std::uint8_t, IdHashMap>();
  check.template operator()<std::int8_t, IdHashMap>();
}

TEST(IdMapTest, ExplicitIdsOutsideRangeRemainValidAndDoNotConsumeCursor) {
  auto created = IdMap<int, int>::create(1, 2);
  ASSERT_TRUE(created);
  auto& map = *created;
  ASSERT_TRUE(map.add(-1, 10));
  ASSERT_TRUE(map.add(8, 20));
  ASSERT_TRUE(map.add(9, 30));
  auto duplicate = map.add(8, 99);
  ASSERT_FALSE(duplicate);
  EXPECT_TRUE(duplicate.error().is<IdMapException>());
  EXPECT_EQ(map.value(8), 20);
  auto first = map.add(40);
  auto second = map.add(50);
  ASSERT_TRUE(first);
  ASSERT_TRUE(second);
  EXPECT_EQ(*first, 1);
  EXPECT_EQ(*second, 2);
  EXPECT_FALSE(map.next_id());
  EXPECT_EQ(map.size(), 5u);
  map.clear();
  auto cursor = map.next_id();
  auto advanced = map.next_id();
  ASSERT_TRUE(cursor);
  ASSERT_TRUE(advanced);
  EXPECT_EQ(*cursor, 1);
  EXPECT_EQ(*advanced, 2);
  EXPECT_FALSE((IdMap<int, int>::create(2, 2)));
  EXPECT_FALSE((IdMap<int, int>::create(3, 2)));
}

TEST(IdMapTest, FullWidthSignedAndUnsignedEndpointsWrapSafely) {
  auto check = []<typename Id>() {
    constexpr auto min_id = std::numeric_limits<Id>::lowest();
    constexpr auto max_id = std::numeric_limits<Id>::max();
    IdMap<Id, std::uint8_t> map;
    auto first = map.next_id();
    ASSERT_TRUE(first);
    EXPECT_EQ(*first, min_id);
    auto advanced = map.next_id();
    ASSERT_TRUE(advanced);
    EXPECT_EQ(*advanced, min_id + 1);

    DataStreamBuffer stream;
    Map<Id, std::uint8_t> entries{{min_id, 7}};
    ASSERT_TRUE(stream.writeMapContainer(entries));
    ASSERT_TRUE(stream.write(min_id));
    ASSERT_TRUE(stream.write(max_id));
    ASSERT_TRUE(stream.write(max_id));
    ASSERT_TRUE(stream.write<std::uint8_t>(0xa5));
    ASSERT_TRUE(stream.seek(0));
    ASSERT_TRUE(stream >> map);
    auto sentinel = stream.read<std::uint8_t>();
    ASSERT_TRUE(sentinel);
    EXPECT_EQ(*sentinel, 0xa5);
    auto last = map.add(8);
    ASSERT_TRUE(last);
    EXPECT_EQ(*last, max_id);
    auto wrapped = map.add(9);
    ASSERT_TRUE(wrapped);
    EXPECT_EQ(*wrapped, min_id + 1);
  };
  check.template operator()<std::int64_t>();
  check.template operator()<std::uint64_t>();
}

TEST(IdMapTest, WireOrderAndLegacyEntriesPreserveFollowingSentinel) {
  auto created = IdMap<std::uint8_t, std::uint8_t>::create(1, 3);
  ASSERT_TRUE(created);
  ASSERT_TRUE(created->add(9, 0x99));
  ASSERT_TRUE(created->add(0x11));
  DataStreamBuffer stream;
  ASSERT_TRUE(stream << *created);
  ASSERT_TRUE(stream.write<std::uint8_t>(0xa5));
  EXPECT_EQ(stream.data(), ByteArray({"\x02\x01\x11\x09\x99\x01\x03\x02\xa5", 9}));
  ASSERT_TRUE(stream.seek(0));
  IdMap<std::uint8_t, std::uint8_t> decoded;
  ASSERT_TRUE(stream >> decoded);
  EXPECT_EQ(decoded, *created);
  auto next = decoded.add(0x22);
  ASSERT_TRUE(next);
  EXPECT_EQ(*next, 2);
  auto sentinel = stream.read<std::uint8_t>();
  ASSERT_TRUE(sentinel);
  EXPECT_EQ(*sentinel, 0xa5);

  DataStreamBuffer duplicates(ByteArray({"\x02\x09\x11\x09\x22\x01\x03\x03\xa5", 9}));
  ASSERT_TRUE(duplicates >> decoded);
  EXPECT_EQ(decoded.size(), 1u);
  EXPECT_EQ(decoded.value(9), 0x11);
  auto legacy_cursor = decoded.next_id();
  ASSERT_TRUE(legacy_cursor);
  EXPECT_EQ(*legacy_cursor, 3);
  auto duplicate_sentinel = duplicates.read<std::uint8_t>();
  ASSERT_TRUE(duplicate_sentinel);
  EXPECT_EQ(*duplicate_sentinel, 0xa5);
}

TEST(IdMapTest, MalformedDecodeRetainsExistingEntriesRangeAndCursor) {
  auto created = IdMap<std::uint8_t, std::uint8_t>::create(4, 6);
  ASSERT_TRUE(created);
  ASSERT_TRUE(created->add(0x44));
  auto original = DataStreamBuffer::serialize(*created);
  ASSERT_TRUE(original);
  for (auto const& bytes : {ByteArray({"\x01\x01", 2}), ByteArray({"\x01\x01\x11", 3}),
                            ByteArray({"\x01\x01\x11\x01", 4}), ByteArray({"\x01\x01\x11\x01\x03", 5}),
                            ByteArray({"\x01\x01\x11\x03\x03\x03", 6}), ByteArray({"\x01\x01\x11\x03\x01\x03", 6}),
                            ByteArray({"\x01\x01\x11\x01\x03\x00", 6}), ByteArray({"\x01\x01\x11\x01\x03\x04", 6})}) {
    auto read = DataStreamBuffer::deserialize(*created, bytes);
    EXPECT_FALSE(read);
    auto retained = DataStreamBuffer::serialize(*created);
    ASSERT_TRUE(retained);
    EXPECT_EQ(*retained, *original);
  }
  auto next = created->next_id();
  ASSERT_TRUE(next);
  EXPECT_EQ(*next, 5);
}

TEST(BiMapTest, DuplicateEitherSideLeavesBothIndexesConsistent) {
  auto check = []<typename MapType>() {
    MapType map;
    ASSERT_TRUE(map.add(1, 11));
    ASSERT_TRUE(map.add(2, 22));
    auto duplicate_right = map.insert(typename MapType::value_type{3, 22});
    EXPECT_FALSE(duplicate_right.second);
    EXPECT_EQ((*duplicate_right.first).first, 2);
    EXPECT_EQ((*duplicate_right.first).second, 22);
    EXPECT_FALSE(map.insert(1, 33));
    EXPECT_FALSE(map.add(4, 22));
    EXPECT_EQ(map.size(), 2u);
    EXPECT_FALSE(map.contains_left(3));
    EXPECT_FALSE(map.contains_left(4));
    EXPECT_FALSE(map.contains_right(33));
    EXPECT_EQ(map.maybe_right(1), 11);
    EXPECT_EQ(map.maybe_right(2), 22);
    EXPECT_EQ(map.maybe_left(11), 1);
    EXPECT_EQ(map.maybe_left(22), 2);
  };
  check.template operator()<BiMap<int, int>>();
  check.template operator()<BiHashMap<int, int>>();
}

TEST(BiMapTest, CopiesOwnTheirCrossIndexesAndSelfAssignmentRetainsBorrows) {
  auto check = []<typename MapType>() {
    MapType assigned;
    std::optional<MapType> copied;
    {
      MapType original;
      ASSERT_TRUE(original.add(1, 11));
      ASSERT_TRUE(original.add(2, 22));
      copied.emplace(original);
      ASSERT_TRUE(assigned.add(9, 99));
      assigned = original;
      original.clear();
      ASSERT_TRUE(original.add(3, 33));
    }
    for (auto* map : {&assigned, &*copied}) {
      EXPECT_EQ(map->maybe_right(1), 11);
      EXPECT_EQ(map->maybe_left(22), 2);
      EXPECT_FALSE(map->contains_left(9));
      auto left = map->find_left_ref(11);
      auto right = map->find_right_ref(1);
      ASSERT_TRUE(left);
      ASSERT_TRUE(right);
      auto left_address = &left->get();
      auto right_address = &right->get();
      *map = std::as_const(*map);
      EXPECT_EQ(&map->find_left_ref(11)->get(), left_address);
      EXPECT_EQ(&map->find_right_ref(1)->get(), right_address);
      for (int value = 4; value != 256; ++value)
        ASSERT_TRUE(map->add(value, value + 1000));
      EXPECT_EQ(&map->find_left_ref(11)->get(), left_address);
      EXPECT_EQ(&map->find_right_ref(1)->get(), right_address);
      EXPECT_TRUE(map->erase_right(22));
      EXPECT_FALSE(map->contains_left(2));
      auto taken = map->extract_right(1);
      ASSERT_TRUE(taken);
      EXPECT_EQ(*taken, 11);
      EXPECT_FALSE(map->contains_right(11));
    }
  };
  check.template operator()<BiMap<int, int>>();
  check.template operator()<BiHashMap<int, int>>();
}

TEST(BiMapTest, AliasedOverwriteAndErasePreserveSurvivingNodeAddresses) {
  auto check = []<typename MapType>() {
    MapType map;
    ASSERT_TRUE(map.add(1, 11));
    ASSERT_TRUE(map.add(2, 22));
    ASSERT_TRUE(map.add(3, 33));
    auto left = map.find_left_ref(11);
    auto right = map.find_right_ref(2);
    ASSERT_TRUE(left);
    ASSERT_TRUE(right);
    auto left_address = &left->get();
    auto right_address = &right->get();
    map.overwrite(left->get(), right->get());
    EXPECT_EQ(map.size(), 2u);
    EXPECT_EQ(map.maybe_right(1), 22);
    EXPECT_EQ(map.maybe_left(22), 1);
    EXPECT_FALSE(map.contains_left(2));
    EXPECT_FALSE(map.contains_right(11));
    EXPECT_EQ(&map.find_left_ref(22)->get(), left_address);
    EXPECT_EQ(&map.find_right_ref(1)->get(), right_address);
    map.overwrite(*left_address, *right_address);
    EXPECT_EQ(&map.find_left_ref(22)->get(), left_address);
    EXPECT_EQ(&map.find_right_ref(1)->get(), right_address);
    map.overwrite(*left_address, 44);
    EXPECT_EQ(&map.find_left_ref(44)->get(), left_address);
    EXPECT_FALSE(map.contains_right(22));
    auto surviving_right = map.find_right_ref(1);
    ASSERT_TRUE(surviving_right);
    auto surviving_address = &surviving_right->get();
    map.overwrite(5, surviving_right->get());
    EXPECT_EQ(&map.find_right_ref(5)->get(), surviving_address);
    EXPECT_FALSE(map.contains_left(1));
    // Arguments may borrow the opposite index, which is erased first.
    ASSERT_TRUE(map.add(6, 6));
    auto aliased_left = map.find_right_ref(6);
    ASSERT_TRUE(aliased_left);
    EXPECT_TRUE(map.erase_left(aliased_left->get()));
    EXPECT_FALSE(map.contains_left(6));
    EXPECT_FALSE(map.contains_right(6));
    ASSERT_TRUE(map.add(7, 7));
    auto aliased_right = map.find_left_ref(7);
    ASSERT_TRUE(aliased_right);
    EXPECT_TRUE(map.erase_right(aliased_right->get()));
    EXPECT_FALSE(map.contains_left(7));
    EXPECT_FALSE(map.contains_right(7));
    EXPECT_EQ(map.maybe_right(3), 33);
    EXPECT_EQ(map.maybe_left(33), 3);
  };
  check.template operator()<BiMap<int, int>>();
  check.template operator()<BiHashMap<int, int>>();
}

namespace {

bool bimap_reverse_order = false;

struct BiMapStatefulOrder {
  bool reverse = bimap_reverse_order;

  bool operator()(int left, int right) const { return reverse ? left > right : left < right; }
};

}// namespace

TEST(BiMapTest, CopyPreservesSourceIndexComparisonPolicies) {
  using Index = Map<int, int const*, BiMapStatefulOrder>;
  using MapType = BiMap<int, int, Index, Index>;
  MapType original;
  ASSERT_TRUE(original.add(1, 11));
  ASSERT_TRUE(original.add(2, 22));
  bimap_reverse_order = true;
  MapType copied(original);
  MapType assigned;
  assigned = original;
  bimap_reverse_order = false;
  EXPECT_EQ(copied.left_values(), (List<int>{1, 2}));
  EXPECT_EQ(copied.right_values(), (List<int>{11, 22}));
  EXPECT_EQ(assigned.left_values(), (List<int>{1, 2}));
  EXPECT_EQ(assigned.right_values(), (List<int>{11, 22}));
  original.clear();
  EXPECT_EQ(copied.maybe_left(22), 2);
  EXPECT_EQ(assigned.maybe_right(1), 11);
}

TEST(IdMapTest, HashDecodeAcceptsAnOccupiedCursorAndKeepsFraming) {
  DataStreamBuffer stream(ByteArray({"\x01\x01\x11\x01\x03\x01\xa5", 7}));
  IdHashMap<std::uint8_t, std::uint8_t> decoded;
  ASSERT_TRUE(decoded.add(9, 0x99));
  ASSERT_TRUE(stream >> decoded);
  EXPECT_FALSE(decoded.contains(9));
  EXPECT_EQ(decoded.value(1), 0x11);
  auto next = decoded.add(0x22);
  ASSERT_TRUE(next);
  EXPECT_EQ(*next, 2);
  auto sentinel = stream.read<std::uint8_t>();
  ASSERT_TRUE(sentinel);
  EXPECT_EQ(*sentinel, 0xa5);
}

namespace {
std::filesystem::path native_test_path(String const& path) {
  return std::filesystem::path(std::u8string_view(reinterpret_cast<char8_t const*>(path.utf8Ptr()), path.utf8Size()));
}

struct NativePathCleanup {
  String path;
  ~NativePathCleanup() {
    std::error_code ignored;
    std::filesystem::remove_all(native_test_path(path), ignored);
  }
};

class FileNativeTest : public testing::Test {
protected:
  void SetUp() override {
    auto random = RandomService::create();
    ASSERT_TRUE(random);
    auto directory = File::temporary_directory(**random);
    ASSERT_TRUE(directory);
    directory_path = std::move(*directory);
  }

  void TearDown() override {
    if (directory_path.empty())
      return;
    std::error_code ignored;
    std::filesystem::remove_all(native_test_path(directory_path), ignored);
  }

  String child(String const& name) const { return File::relative_to(directory_path, name); }

  String directory_path;
};
}// namespace

TEST_F(FileNativeTest, StatusDistinguishesMissingWrongTypeAndMetadata) {
  auto regular = child("regular");
  ASSERT_TRUE(File::write_file(String("payload"), regular));
  for (auto const& missing : {child("missing"), File::relative_to(regular, "child")}) {
    EXPECT_EQ(File::exists(missing), false);
    EXPECT_EQ(File::is_regular_file(missing), false);
    EXPECT_EQ(File::is_directory(missing), false);
    EXPECT_EQ(File::is_symlink(missing), false);
    EXPECT_FALSE(File::file_size(missing));
  }
  EXPECT_EQ(File::exists(regular), true);
  EXPECT_EQ(File::is_regular_file(regular), true);
  EXPECT_EQ(File::is_directory(regular), false);
  EXPECT_EQ(File::is_regular_file(directory_path), false);
  EXPECT_EQ(File::is_directory(directory_path), true);
  EXPECT_EQ(File::file_size(regular), 7);
  EXPECT_FALSE(File::file_size(directory_path));
}

TEST_F(FileNativeTest, NamedTemporaryAcquisitionRetriesOnlyCollisions) {
  auto entropy = RandomService::create();
  ASSERT_TRUE(entropy);
  auto seed = (*entropy)->randu64();
  RandomService first(seed);
  RandomService repeated(seed);
  auto candidate_file = File::temporary_file(first);
  ASSERT_TRUE(candidate_file);
  auto candidate = (*candidate_file)->file_name();
  NativePathCleanup candidate_cleanup{candidate};
  ASSERT_TRUE((*candidate_file)->write_full(String("do not truncate").utf8()));
  ASSERT_TRUE((*candidate_file)->close());
  auto temporary = File::temporary_file(repeated);
  ASSERT_TRUE(temporary);
  NativePathCleanup temporary_cleanup{(*temporary)->file_name()};
  EXPECT_NE((*temporary)->file_name(), candidate);
  EXPECT_EQ(File::read_file_string(candidate), String("do not truncate"));
  EXPECT_EQ((*temporary)->size(), 0);
  ASSERT_TRUE((*temporary)->close());
}

TEST_F(FileNativeTest, TemporaryDirectoriesDoNotAdoptExistingDirectory) {
  auto entropy = RandomService::create();
  ASSERT_TRUE(entropy);
  auto seed = (*entropy)->randu64();
  RandomService first(seed);
  RandomService repeated(seed);
  auto original = File::temporary_directory(first);
  ASSERT_TRUE(original);
  NativePathCleanup original_cleanup{*original};
  auto marker = File::relative_to(*original, "marker");
  ASSERT_TRUE(File::write_file(String("retained"), marker));
  auto next = File::temporary_directory(repeated);
  ASSERT_TRUE(next);
  NativePathCleanup next_cleanup{*next};
  EXPECT_NE(*original, *next);
  EXPECT_EQ(File::read_file_string(marker), String("retained"));
}

TEST_F(FileNativeTest, ReplacementDoesNotClobberExistingStagingFile) {
  auto target = child("target");
  auto stage = target + ".new";
  ASSERT_TRUE(File::write_file(String("original"), target));
  ASSERT_TRUE(File::write_file(String("unrelated staging data"), stage));
  EXPECT_FALSE(File::overwrite_file_with_rename(String("replacement"), target));
  EXPECT_EQ(File::read_file_string(target), String("original"));
  EXPECT_EQ(File::read_file_string(stage), String("unrelated staging data"));
}

TEST_F(FileNativeTest, ReplacementCompletesAndFailedRenamePreservesTarget) {
  auto target = child("target");
  ASSERT_TRUE(File::write_file(String("original"), target));
  String bytes(std::string(16385, '\0'));
  ASSERT_TRUE(File::overwrite_file_with_rename(bytes, target));
  EXPECT_EQ(File::read_file_string(target), bytes);
  EXPECT_EQ(File::exists(target + ".new"), false);
  EXPECT_FALSE(File::rename(child("missing-source"), target));
  EXPECT_EQ(File::read_file_string(target), bytes);
  // A same-path request may succeed or be rejected by the native API, but must
  // never move the only copy away or delete its contents.
  auto same_path = File::rename(target, target);
  (void)same_path;
  EXPECT_EQ(File::read_file_string(target), bytes);
}

TEST_F(FileNativeTest, FailedReplacementDoesNotFallBackToDirectWrite) {
  auto target = child("target-directory");
  ASSERT_TRUE(File::create_directory(target));
  auto marker = File::relative_to(target, "marker");
  ASSERT_TRUE(File::write_file(String("keep"), marker));
  EXPECT_FALSE(File::overwrite_file_with_rename(String("replacement"), target));
  EXPECT_EQ(File::is_directory(target), true);
  EXPECT_EQ(File::read_file_string(marker), String("keep"));
  EXPECT_EQ(File::exists(target + ".new"), false);
  ASSERT_TRUE(File::remove_all(target));
  ASSERT_TRUE(File::overwrite_file_with_rename(String("recovered"), target));
  EXPECT_EQ(File::read_file_string(target), String("recovered"));
}

#ifndef STAR_SYSTEM_FAMILY_WINDOWS
TEST_F(FileNativeTest, StatusReportsSymlinkLoopsButRecognizesBrokenSymlinks) {
  auto loop = child("loop");
  ASSERT_EQ(::symlink("loop", loop.utf8Ptr()), 0);
  EXPECT_FALSE(File::exists(loop));
  EXPECT_FALSE(File::is_regular_file(loop));
  EXPECT_FALSE(File::is_directory(loop));
  EXPECT_FALSE(File::file_size(loop));
  EXPECT_EQ(File::is_symlink(loop), true);
  auto broken = child("broken");
  ASSERT_EQ(::symlink("absent", broken.utf8Ptr()), 0);
  EXPECT_EQ(File::exists(broken), false);
  EXPECT_EQ(File::is_regular_file(broken), false);
  EXPECT_EQ(File::is_directory(broken), false);
  EXPECT_EQ(File::is_symlink(broken), true);
}

TEST_F(FileNativeTest, ExclusiveStagingNeverFollowsSymlinks) {
  auto target = child("target");
  auto unrelated = child("unrelated");
  ASSERT_TRUE(File::write_file(String("original"), target));
  ASSERT_TRUE(File::write_file(String("untouched"), unrelated));
  ASSERT_EQ(::symlink(unrelated.utf8Ptr(), (target + ".new").utf8Ptr()), 0);
  EXPECT_FALSE(File::overwrite_file_with_rename(String("replacement"), target));
  EXPECT_EQ(File::read_file_string(target), String("original"));
  EXPECT_EQ(File::read_file_string(unrelated), String("untouched"));
}

TEST_F(FileNativeTest, NativeSizeDoesNotMoveCursorAndRejectsNonregularDevices) {
  auto regular = child("regular");
  ASSERT_TRUE(File::write_file(String("payload"), regular));
  auto file = File::open(regular, IOMode::Read);
  ASSERT_TRUE(file);
  ASSERT_TRUE((*file)->seek(3));
  EXPECT_EQ((*file)->size(), 7);
  EXPECT_EQ((*file)->pos(), 3);
  auto device = File::open("/dev/null", IOMode::Read);
  ASSERT_TRUE(device);
  EXPECT_FALSE((*device)->size());
  EXPECT_FALSE(File::file_size("/dev/null"));
  char byte;
  EXPECT_EQ((*device)->read(std::span<char>(&byte, 1)), 0u);
  auto fifo = child("fifo");
  ASSERT_EQ(::mkfifo(fifo.utf8Ptr(), 0600), 0);
  EXPECT_FALSE(File::open(fifo, IOMode::ReadWrite | IOMode::Append));
  EXPECT_FALSE(File::file_size(fifo));
}
#endif

#ifdef STAR_SYSTEM_LINUX
TEST_F(FileNativeTest, NonblockingReadFailureIsNotReportedAsEndOfFile) {
  auto fifo = child("fifo");
  ASSERT_EQ(::mkfifo(fifo.utf8Ptr(), 0600), 0);
  auto file = File::open(fifo, IOMode::ReadWrite);
  ASSERT_TRUE(file);
  // Locate the actual owned descriptor without adding a public native-handle
  // escape hatch solely for the test.
  int descriptor = -1;
  std::error_code error;
  for (std::filesystem::directory_iterator entry("/proc/self/fd", error), end; !error && entry != end;
       entry.increment(error)) {
    auto target = std::filesystem::read_symlink(entry->path(), error);
    if (error)
      break;
    if (target == native_test_path(fifo)) {
      auto name = entry->path().filename().native();
      auto parsed = std::from_chars(name.data(), name.data() + name.size(), descriptor);
      ASSERT_EQ(parsed.ec, std::errc{});
      break;
    }
  }
  ASSERT_FALSE(error);
  ASSERT_GE(descriptor, 0);
  auto flags = ::fcntl(descriptor, F_GETFL);
  ASSERT_GE(flags, 0);
  ASSERT_EQ(::fcntl(descriptor, F_SETFL, flags | O_NONBLOCK), 0);
  char byte;
  auto read = (*file)->read(std::span<char>(&byte, 1));
  ASSERT_FALSE(read);
  EXPECT_TRUE(read.error().is<IOException>());
  EXPECT_FALSE((*file)->at_end());
}
#endif

#ifdef STAR_SYSTEM_FAMILY_WINDOWS
TEST_F(FileNativeTest, StatusRetainsNativeWildcardMatching) {
  ASSERT_TRUE(File::write_file(String("payload"), child("match.txt")));
  EXPECT_EQ(File::exists(child("match.*")), true);
  EXPECT_EQ(File::is_regular_file(child("match.*")), true);
  EXPECT_EQ(File::exists(child("missing.*")), false);
  EXPECT_EQ(File::is_regular_file(child("missing.*")), false);
}
#endif

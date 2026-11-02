import std;
import star.set;
import star.list;
import star.format;
import star.string;
import star.static_vector;
import star.map;
import star.optional;

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
    if constexpr (std::is_void_v<typename Container::AppendResult>) {
      input.append(std::move(owned));
    } else {
      auto added = input.append(std::move(owned));
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
    input.emplaceAppend(value, std::ref(destructions), std::ref(comparisons));
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
  auto appended = input.appendAll(values);
  ASSERT_FALSE(appended);
  EXPECT_TRUE(appended.error().is<StaticVectorSizeException>());
  EXPECT_EQ(std::vector<int>(input.begin(), input.end()), (std::vector<int>{1, 2}));
  int next = 0;
  stream >> next;
  EXPECT_EQ(next, 4);
}

TEST(ListTest, BoundedEmplacementRejectsBeforeMovingArguments) {
  StaticList<std::unique_ptr<int>, 1> input;
  ASSERT_TRUE(input.append(std::make_unique<int>(1)));
  auto pending = std::make_unique<int>(2);
  auto rejected = input.emplaceAppend(std::move(pending));
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
    EXPECT_EQ(input.indexOf(1), 0u);
    EXPECT_EQ(input.lastIndexOf(1), 2u);
    EXPECT_EQ(input.indexOf(1, 1), 2u);
    EXPECT_EQ(input.lastIndexOf(1, 2), 0u);
  };
  expectDefaults(List<int>{1, 2, 1});
  expectDefaults(SmallList<int, 2>{1, 2, 1});
  expectDefaults(Deque<int>{1, 2, 1});
  auto bounded = StaticList<int, 3>::create({1, 2, 1});
  ASSERT_TRUE(bounded);
  expectDefaults(*bounded);
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
  auto nullable = entries.maybeRef(1);
  ASSERT_TRUE(nullable);
  nullable->get() = std::ref(replacement);
  replacement = 21;
  auto const& constEntries = entries;
  auto observed = constEntries.at(1);
  ASSERT_TRUE(observed);
  EXPECT_EQ(observed->get().get(), 21);
  auto nullableObserved = constEntries.maybeRef(1);
  ASSERT_TRUE(nullableObserved);
  EXPECT_EQ(nullableObserved->get().get(), 21);
  EXPECT_EQ(original, 10);
  EXPECT_FALSE(entries.maybeRef(2));
  EXPECT_FALSE(constEntries.maybeRef(2));
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

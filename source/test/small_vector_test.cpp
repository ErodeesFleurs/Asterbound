import star.small_vector;
import std;
import star.format;

#include "gtest/gtest.h"

using namespace Star;

TEST(SmallVectorTest, InsertErase) {
  typedef SmallVector<int, 2> SV;
  SV a = {1, 2, 3, 4};
  EXPECT_EQ(a.size(), 4u);
  EXPECT_EQ(a, SV({1, 2, 3, 4}));
  EXPECT_NE(a, SV({1, 2, 3}));
  a.insert(a.begin(), 0);
  a.insert(a.begin(), -1);
  EXPECT_EQ(a, SV({-1, 0, 1, 2, 3, 4}));
  a.insert(a.begin(), {-3, -2});
  EXPECT_EQ(a, SV({-3, -2, -1, 0, 1, 2, 3, 4}));
  a.erase(a.begin() + 1);
  EXPECT_EQ(a, SV({-3, -1, 0, 1, 2, 3, 4}));
  a.erase(a.begin(), a.begin() + 3);
  EXPECT_EQ(a, SV({1, 2, 3, 4}));
  a.insert(a.end(), {5, 6, 7, 8});
  EXPECT_EQ(a, SV({1, 2, 3, 4, 5, 6, 7, 8}));
  a.erase(a.begin() + 2, a.end() - 2);
  EXPECT_EQ(a, SV({1, 2, 7, 8}));
  a.insert(a.begin() + 2, 6);
  a.insert(a.begin() + 2, 5);
  a.insert(a.begin() + 2, 4);
  a.insert(a.begin() + 2, 3);
  EXPECT_EQ(a, SV({1, 2, 3, 4, 5, 6, 7, 8}));

  SV b(a.begin(), a.end());
  EXPECT_EQ(b, SV({1, 2, 3, 4, 5, 6, 7, 8}));
}

TEST(SmallVectorTest, Comparators) {
  typedef SmallVector<int, 3> SV;

  EXPECT_TRUE(SV({1, 2, 3, 4}) < SV({1, 2, 3, 5}));
  EXPECT_FALSE(SV({1, 2, 3, 4}) < SV({1, 2, 3, 4}));
  EXPECT_FALSE(SV({1, 2, 3, 4}) < SV({1, 2, 3, 3}));
  EXPECT_TRUE(SV({1, 2, 3}) < SV({1, 2, 3, 4}));
  EXPECT_FALSE(SV({1, 2, 3, 4, 5}) < SV({1, 2, 3, 4}));
}

TEST(SmallVectorTest, Destructors) {
  auto i = std::make_shared<int>(0);
  SmallVector<std::shared_ptr<int>, 1> v;
  v.push_back(i);
  v.push_back(i);
  v.push_back(i);
  EXPECT_EQ(i.use_count(), 4);
  ASSERT_TRUE(v.pop_back());
  EXPECT_EQ(i.use_count(), 3);
  ASSERT_TRUE(v.pop_back());
  EXPECT_EQ(i.use_count(), 2);
  v.clear();
  EXPECT_EQ(i.use_count(), 1);
}

namespace {
struct ContextualVectorComparison {
  bool value;
  explicit operator bool() const { return value; }
  bool operator!() const { return value; }
};

struct SmallVectorLifetime {
  int alive = 0;
  int copies = 0;
  int moves = 0;
  int copyAssignments = 0;
  int moveAssignments = 0;
  int destroyed = 0;
};

struct CopyableVectorValue {
  std::reference_wrapper<SmallVectorLifetime> state;
  int value;

  CopyableVectorValue(SmallVectorLifetime& state, int value) : state(state), value(value) { ++state.alive; }
  CopyableVectorValue(CopyableVectorValue const& other) : state(other.state), value(other.value) {
    ++state.get().copies;
    ++state.get().alive;
  }
  CopyableVectorValue(CopyableVectorValue&& other) : state(other.state), value(other.value) {
    ++state.get().moves;
    other.value = -1;
    ++state.get().alive;
  }
  CopyableVectorValue& operator=(CopyableVectorValue const& other) {
    ++state.get().copyAssignments;
    value = other.value;
    return *this;
  }
  CopyableVectorValue& operator=(CopyableVectorValue&& other) {
    ++state.get().moveAssignments;
    value = std::exchange(other.value, -1);
    return *this;
  }
  ~CopyableVectorValue() {
    ++state.get().destroyed;
    --state.get().alive;
  }
};

struct MoveOnlyVectorValue {
  std::reference_wrapper<SmallVectorLifetime> state;
  int value;

  MoveOnlyVectorValue(SmallVectorLifetime& state, int value) : state(state), value(value) { ++state.alive; }
  MoveOnlyVectorValue(MoveOnlyVectorValue const&) = delete;
  MoveOnlyVectorValue(MoveOnlyVectorValue&& other) : state(other.state), value(other.value) {
    ++state.get().moves;
    other.value = -1;
    ++state.get().alive;
  }
  MoveOnlyVectorValue& operator=(MoveOnlyVectorValue&& other) {
    ++state.get().moveAssignments;
    value = std::exchange(other.value, -1);
    return *this;
  }
  ~MoveOnlyVectorValue() {
    ++state.get().destroyed;
    --state.get().alive;
  }
};
}// namespace

TEST(SmallVectorTest, EqualityNeedsOnlyContextualInequality) {
  struct Value {
    int value;
    std::reference_wrapper<int> comparisons;
    ContextualVectorComparison operator!=(Value const& other) const {
      ++comparisons.get();
      return {value != other.value};
    }
  };
  int comparisons = 0;
  SmallVector<Value, 3> left{{1, comparisons}, {2, comparisons}};
  SmallVector<Value, 3> right{{1, comparisons}, {2, comparisons}};
  EXPECT_TRUE(left == right);
  EXPECT_EQ(comparisons, 2);
  right[0].value = 3;
  comparisons = 0;
  EXPECT_FALSE(left == right);
  EXPECT_EQ(comparisons, 1);
  comparisons = 0;
  EXPECT_TRUE(left == left);
  EXPECT_EQ(comparisons, 0);
  ASSERT_TRUE(right.pop_back());
  EXPECT_FALSE(left == right);
  EXPECT_EQ(comparisons, 0);
}

TEST(SmallVectorTest, OrderingNeedsOnlyContextualLess) {
  struct Value {
    int value;
    ContextualVectorComparison operator<(Value const& other) const { return {value < other.value}; }
  };
  SmallVector<Value, 3> prefix{{1}, {2}};
  SmallVector<Value, 3> longer{{1}, {2}, {3}};
  SmallVector<Value, 3> greater{{1}, {4}};
  SmallVector<Value, 3> empty;
  EXPECT_TRUE(prefix < longer);
  EXPECT_FALSE(longer < prefix);
  EXPECT_TRUE(longer < greater);
  EXPECT_FALSE(greater < longer);
  EXPECT_FALSE(prefix < prefix);
  EXPECT_TRUE(empty < prefix);
  EXPECT_FALSE(prefix < empty);
  EXPECT_FALSE(empty < empty);
}

TEST(SmallVectorTest, ZeroCapacityAndOveralignedStorage) {
  struct alignas(64) Value {
    int value;
  };
  static_assert(alignof(SmallVector<Value, 1>) == alignof(Value));
  SmallVector<Value, 0> empty;
  EXPECT_NE(empty.data(), nullptr);
  EXPECT_TRUE(empty.empty());
  empty.emplace_back(42);
  EXPECT_EQ(reinterpret_cast<std::uintptr_t>(empty.data()) % alignof(Value), 0u);
  EXPECT_EQ(empty[0].value, 42);
  SmallVector<Value, 1> values;
  values.emplace_back(1);
  EXPECT_EQ(reinterpret_cast<std::uintptr_t>(values.data()) % alignof(Value), 0u);
  values.emplace_back(2);
  EXPECT_EQ(reinterpret_cast<std::uintptr_t>(values.data()) % alignof(Value), 0u);
  EXPECT_EQ(values[0].value, 1);
  EXPECT_EQ(values[1].value, 2);
}

TEST(SmallVectorTest, NothrowMoveIsPreferredToCopy) {
  struct Value {
    std::reference_wrapper<int> moves;
    std::reference_wrapper<int> copies;

    explicit Value(int& moves, int& copies) : moves(moves), copies(copies) {}
    Value(Value const& other) : moves(other.moves), copies(other.copies) { ++copies.get(); }
    Value(Value&& other) noexcept : moves(other.moves), copies(other.copies) { ++moves.get(); }
  };
  int moves = 0;
  int copies = 0;
  SmallVector<Value, 1> values;
  values.emplace_back(moves, copies);
  values.reserve(2);
  EXPECT_EQ(moves, 1);
  EXPECT_EQ(copies, 0);
  EXPECT_EQ(values.size(), 1u);
}

TEST(SmallVectorTest, InsertReturnsNewStoragePosition) {
  SmallVector<int, 1> values{1};
  auto first = values.insert(values.begin(), 2);
  ASSERT_EQ(first, values.begin());
  EXPECT_EQ(*first, 2);
  std::array<int, 3> more{3, 4, 5};
  auto inserted = values.insert(values.begin() + 1, more.begin(), more.end());
  ASSERT_EQ(inserted, values.begin() + 1);
  EXPECT_EQ(values, (SmallVector<int, 1>{2, 3, 4, 5, 1}));
}

TEST(SmallVectorTest, SinglePassAndMoveOnlyInsertion) {
  std::istringstream stream("2 3 4");
  SmallVector<int, 1> values{1};
  auto inserted = values.insert(values.end(), std::istream_iterator<int>(stream), std::istream_iterator<int>{});
  ASSERT_EQ(inserted, values.begin() + 1);
  EXPECT_EQ(values, (SmallVector<int, 1>{1, 2, 3, 4}));
  SmallVector<std::unique_ptr<int>, 1> owners;
  owners.emplace_back(std::make_unique<int>(7));
  owners.emplace(owners.begin(), std::make_unique<int>(9));
  EXPECT_EQ(*owners[0], 9);
  EXPECT_EQ(*owners[1], 7);
}

TEST(SmallVectorTest, NondefaultAssignmentConstructsOnlyNewElements) {
  SmallVectorLifetime state;
  {
    SmallVector<CopyableVectorValue, 4> source;
    source.emplace_back(state, 10);
    source.emplace_back(state, 20);
    source.emplace_back(state, 30);
    SmallVector<CopyableVectorValue, 4> target;
    target.emplace_back(state, 1);
    target = source;
    EXPECT_EQ(state.copyAssignments, 1);
    EXPECT_EQ(state.copies, 2);
    EXPECT_EQ(state.alive, 6);
    ASSERT_EQ(target.size(), 3u);
    EXPECT_EQ(target[0].value, 10);
    EXPECT_EQ(target[2].value, 30);

    ASSERT_TRUE(source.pop_back());
    ASSERT_TRUE(source.pop_back());
    auto destroyed = state.destroyed;
    target = source;
    EXPECT_EQ(target.size(), 1u);
    EXPECT_EQ(state.copyAssignments, 2);
    EXPECT_EQ(state.copies, 2);
    EXPECT_EQ(state.destroyed - destroyed, 2);
    EXPECT_EQ(state.alive, 2);
    auto& alias = target;
    target = alias;
    EXPECT_EQ(state.copyAssignments, 2);

    target = {CopyableVectorValue(state, 40), CopyableVectorValue(state, 50)};
    ASSERT_EQ(target.size(), 2u);
    EXPECT_EQ(target[0].value, 40);
    EXPECT_EQ(target[1].value, 50);
    EXPECT_EQ(state.copyAssignments, 3);
    EXPECT_EQ(state.copies, 3);
    EXPECT_EQ(state.alive, 3);
    target = {};
    EXPECT_TRUE(target.empty());
    EXPECT_EQ(state.alive, 1);
  }
  EXPECT_EQ(state.alive, 0);
}

TEST(SmallVectorTest, NondefaultCopyAssignmentGrowsHeapAndReusesCapacity) {
  SmallVectorLifetime state;
  {
    SmallVector<CopyableVectorValue, 1> source;
    source.emplace_back(state, 10);
    source.emplace_back(state, 20);
    source.emplace_back(state, 30);
    SmallVector<CopyableVectorValue, 1> target;
    target.emplace_back(state, 1);
    target = source;
    ASSERT_EQ(target.size(), 3u);
    EXPECT_EQ(target[0].value, 10);
    EXPECT_EQ(target[1].value, 20);
    EXPECT_EQ(target[2].value, 30);
    EXPECT_EQ(state.alive, 6);
    auto storage = reinterpret_cast<std::uintptr_t>(target.data());
    ASSERT_TRUE(source.pop_back());
    ASSERT_TRUE(source.pop_back());
    target = source;
    EXPECT_EQ(reinterpret_cast<std::uintptr_t>(target.data()), storage);
    EXPECT_EQ(target.size(), 1u);
    EXPECT_EQ(state.alive, 2);
  }
  EXPECT_EQ(state.alive, 0);
}

TEST(SmallVectorTest, HeapMoveTakesStorageEvenAfterShrinkingAndReleasesTarget) {
  SmallVectorLifetime state;
  {
    SmallVector<MoveOnlyVectorValue, 2> source;
    source.reserve(8);
    source.emplace_back(state, 10);
    source.emplace_back(state, 20);
    source.emplace_back(state, 30);
    ASSERT_TRUE(source.pop_back());
    ASSERT_TRUE(source.pop_back());
    auto storage = reinterpret_cast<std::uintptr_t>(source.data());
    SmallVector<MoveOnlyVectorValue, 2> target;
    target.reserve(9);
    target.emplace_back(state, 40);
    target.emplace_back(state, 50);
    auto destroyed = state.destroyed;
    target = std::move(source);
    EXPECT_EQ(reinterpret_cast<std::uintptr_t>(target.data()), storage);
    ASSERT_EQ(target.size(), 1u);
    EXPECT_EQ(target[0].value, 10);
    EXPECT_EQ(state.moves, 0);
    EXPECT_EQ(state.moveAssignments, 0);
    EXPECT_EQ(state.destroyed - destroyed, 2);
    EXPECT_EQ(state.alive, 1);
    EXPECT_TRUE(source.empty());
    EXPECT_NE(reinterpret_cast<std::uintptr_t>(source.data()), storage);
    source.emplace_back(state, 60);
    EXPECT_EQ(source[0].value, 60);
    auto& alias = target;
    target = std::move(alias);
    EXPECT_EQ(reinterpret_cast<std::uintptr_t>(target.data()), storage);
    EXPECT_EQ(target[0].value, 10);
    EXPECT_EQ(state.alive, 2);
  }
  EXPECT_EQ(state.alive, 0);
}

TEST(SmallVectorTest, HeapMoveConstructionAndEmptyHeapKeepCapacity) {
  SmallVectorLifetime state;
  {
    SmallVector<MoveOnlyVectorValue, 2> source;
    source.reserve(8);
    source.emplace_back(state, 10);
    auto storage = reinterpret_cast<std::uintptr_t>(source.data());
    SmallVector<MoveOnlyVectorValue, 2> target(std::move(source));
    EXPECT_EQ(reinterpret_cast<std::uintptr_t>(target.data()), storage);
    EXPECT_EQ(target[0].value, 10);
    EXPECT_TRUE(source.empty());
    source.emplace_back(state, 20);
    target.clear();
    SmallVector<MoveOnlyVectorValue, 2> empty(std::move(target));
    EXPECT_TRUE(empty.empty());
    EXPECT_EQ(reinterpret_cast<std::uintptr_t>(empty.data()), storage);
    for (int i = 0; i < 8; ++i)
      empty.emplace_back(state, i);
    EXPECT_EQ(reinterpret_cast<std::uintptr_t>(empty.data()), storage);
    EXPECT_EQ(state.moves, 0);
    EXPECT_TRUE(target.empty());
    target.emplace_back(state, 30);
    EXPECT_EQ(state.alive, 10);
  }
  EXPECT_EQ(state.alive, 0);
}

TEST(SmallVectorTest, InlineMoveOnlyAssignmentAndEraseNeedNoCopyOrDefault) {
  SmallVectorLifetime state;
  {
    SmallVector<MoveOnlyVectorValue, 4> source;
    source.emplace_back(state, 10);
    source.emplace_back(state, 20);
    source.emplace_back(state, 30);
    SmallVector<MoveOnlyVectorValue, 4> target;
    target.emplace_back(state, 1);
    auto sourceStorage = reinterpret_cast<std::uintptr_t>(source.data());
    target = std::move(source);
    EXPECT_NE(reinterpret_cast<std::uintptr_t>(target.data()), sourceStorage);
    EXPECT_TRUE(source.empty());
    EXPECT_EQ(state.moveAssignments, 1);
    EXPECT_EQ(state.moves, 2);
    EXPECT_EQ(state.alive, 3);
    ASSERT_EQ(target.size(), 3u);
    EXPECT_EQ(target[0].value, 10);
    EXPECT_EQ(target[2].value, 30);
    auto assignments = state.moveAssignments;
    EXPECT_EQ(target.erase(target.begin() + 1, target.begin() + 1), target.begin() + 1);
    EXPECT_EQ(state.moveAssignments, assignments);
    EXPECT_EQ(target[1].value, 20);
    EXPECT_EQ(target.erase(target.begin()), target.begin());
    EXPECT_EQ(target[0].value, 20);
    EXPECT_EQ(target.erase(target.begin(), target.end()), target.begin());
    EXPECT_TRUE(target.empty());
    EXPECT_EQ(state.alive, 0);
    source.emplace_back(state, 40);
    target.emplace_back(state, 50);
    target.emplace_back(state, 60);
    auto destroyed = state.destroyed;
    target = std::move(source);
    EXPECT_EQ(target.size(), 1u);
    EXPECT_EQ(target[0].value, 40);
    EXPECT_TRUE(source.empty());
    EXPECT_EQ(state.destroyed - destroyed, 2);
  }
  EXPECT_EQ(state.alive, 0);
}

TEST(SmallVectorTest, InlineMoveConstructionOnlyRequiresMoveConstruction) {
  struct Value {
    int value;
    explicit Value(int value) : value(value) {}
    Value(Value const&) = delete;
    Value(Value&& other) : value(std::exchange(other.value, -1)) {}
    Value& operator=(Value&&) = delete;
  };
  SmallVector<Value, 2> source;
  source.emplace_back(10);
  SmallVector<Value, 2> target(std::move(source));
  ASSERT_EQ(target.size(), 1u);
  EXPECT_EQ(target[0].value, 10);
  EXPECT_TRUE(source.empty());
  source.emplace_back(20);
  EXPECT_EQ(source[0].value, 20);
}

TEST(SmallVectorTest, InlineMoveIntoHeapTargetReusesStorageAndDestroysTail) {
  SmallVectorLifetime state;
  {
    SmallVector<MoveOnlyVectorValue, 2> source;
    source.emplace_back(state, 10);
    SmallVector<MoveOnlyVectorValue, 2> target;
    target.reserve(8);
    target.emplace_back(state, 20);
    target.emplace_back(state, 30);
    target.emplace_back(state, 40);
    auto storage = reinterpret_cast<std::uintptr_t>(target.data());
    auto destroyed = state.destroyed;
    target = std::move(source);
    EXPECT_EQ(reinterpret_cast<std::uintptr_t>(target.data()), storage);
    ASSERT_EQ(target.size(), 1u);
    EXPECT_EQ(target[0].value, 10);
    EXPECT_EQ(state.moveAssignments, 1);
    EXPECT_EQ(state.moves, 0);
    EXPECT_EQ(state.destroyed - destroyed, 3);
    EXPECT_EQ(state.alive, 1);
    EXPECT_TRUE(source.empty());
    source.emplace_back(state, 50);
    EXPECT_EQ(source[0].value, 50);
  }
  EXPECT_EQ(state.alive, 0);
}

TEST(SmallVectorTest, ResizePreservesValuesAndObjectLifetime) {
  struct Value {
    static int& live() {
      static int count = 0;
      return count;
    }
    int value = 7;
    Value() { ++live(); }
    Value(Value const& other) : value(other.value) { ++live(); }
    ~Value() { --live(); }
  };
  EXPECT_EQ(Value::live(), 0);
  {
    SmallVector<Value, 4> values;
    values.resize(2);
    ASSERT_EQ(values.size(), 2u);
    EXPECT_EQ(values[0].value, 7);
    EXPECT_EQ(values[1].value, 7);
    EXPECT_EQ(Value::live(), 2);
    values[0].value = 11;
    values.resize(2);
    EXPECT_EQ(values.size(), 2u);
    EXPECT_EQ(values[0].value, 11);
    EXPECT_EQ(values[1].value, 7);
    EXPECT_EQ(Value::live(), 2);
    values.resize(3);
    ASSERT_EQ(values.size(), 3u);
    EXPECT_EQ(values[0].value, 11);
    EXPECT_EQ(values[1].value, 7);
    EXPECT_EQ(values[2].value, 7);
    EXPECT_EQ(Value::live(), 3);
    values.resize(1);
    ASSERT_EQ(values.size(), 1u);
    EXPECT_EQ(values[0].value, 11);
    EXPECT_EQ(Value::live(), 1);
    values.resize(0);
    EXPECT_TRUE(values.empty());
    EXPECT_EQ(Value::live(), 0);
    values.resize(1);
    EXPECT_EQ(Value::live(), 1);
  }
  EXPECT_EQ(Value::live(), 0);
}

TEST(SmallVectorTest, AliasedEmplaceStagesValueBeforeGrowth) {
  SmallVector<std::string, 1> text{"first"};
  text.emplace(text.begin(), text[0]);
  EXPECT_EQ(text, (SmallVector<std::string, 1>{"first", "first"}));
}

TEST(SmallVectorTest, AssignmentPreservesForwardEffectsAndReverseTailDestruction) {
  struct Value {
    std::reference_wrapper<std::vector<int>> events;
    int value;
    Value(std::vector<int>& events, int value) : events(events), value(value) {}
    Value(Value const& other) : events(other.events), value(other.value) { events.get().push_back(300 + value); }
    Value(Value&& other) : events(other.events), value(std::exchange(other.value, -1)) {
      events.get().push_back(400 + value);
    }
    Value& operator=(Value const& other) {
      value = other.value;
      events.get().push_back(100 + value);
      return *this;
    }
    Value& operator=(Value&& other) {
      value = std::exchange(other.value, -1);
      events.get().push_back(200 + value);
      return *this;
    }
    ~Value() { events.get().push_back(-value); }
  };
  std::vector<int> events;
  SmallVector<Value, 4> source;
  source.emplace_back(events, 10);
  source.emplace_back(events, 20);
  SmallVector<Value, 4> target;
  for (int i = 1; i <= 4; ++i)
    target.emplace_back(events, i);
  events.clear();
  EXPECT_EQ(&(target = source), &target);
  EXPECT_EQ(events, (std::vector<int>{110, 120, -4, -3}));
  ASSERT_EQ(target.size(), 2u);
  EXPECT_EQ(target[0].value, 10);
  EXPECT_EQ(target[1].value, 20);

  target.emplace_back(events, 3);
  target.emplace_back(events, 4);
  events.clear();
  EXPECT_EQ(&(target = std::move(source)), &target);
  EXPECT_EQ(events, (std::vector<int>{210, 220, -4, -3, 1, 1}));
  EXPECT_TRUE(source.empty());
  ASSERT_EQ(target.size(), 2u);
  EXPECT_EQ(target[0].value, 10);
  EXPECT_EQ(target[1].value, 20);
}

TEST(SmallVectorTest, EraseIncludesElementsAppendedByMoveAssignment) {
  struct Value {
    std::reference_wrapper<std::function<void()>> afterMove;
    int value;
    Value(std::function<void()>& afterMove, int value) : afterMove(afterMove), value(value) {}
    Value(Value const&) = delete;
    Value(Value&& other) : afterMove(other.afterMove), value(std::exchange(other.value, -1)) {}
    Value& operator=(Value&& other) {
      value = std::exchange(other.value, -1);
      if (afterMove.get())
        afterMove.get()();
      return *this;
    }
  };
  std::function<void()> afterMove;
  SmallVector<Value, 8> values;
  for (int i : {10, 20, 30})
    values.emplace_back(afterMove, i);
  bool appended = false;
  afterMove = [&] {
    if (!std::exchange(appended, true))
      values.emplace_back(afterMove, 40);
  };
  EXPECT_EQ(values.erase(values.begin()), values.begin());
  ASSERT_EQ(values.size(), 3u);
  EXPECT_EQ(values[0].value, 20);
  EXPECT_EQ(values[1].value, 30);
  EXPECT_EQ(values[2].value, 40);

  values.clear();
  for (int i : {10, 20, 30, 40})
    values.emplace_back(afterMove, i);
  appended = false;
  afterMove = [&] {
    if (!std::exchange(appended, true))
      values.emplace_back(afterMove, 50);
  };
  EXPECT_EQ(values.erase(values.begin(), values.begin() + 2), values.begin());
  ASSERT_EQ(values.size(), 3u);
  EXPECT_EQ(values[0].value, 30);
  EXPECT_EQ(values[1].value, 40);
  EXPECT_EQ(values[2].value, 50);
}

#if GTEST_HAS_DEATH_TEST
TEST(SmallVectorTest, RejectsWrappedByteCapacityInsteadOfAllocatingZeroBytes) {
  auto previous_style = testing::GTEST_FLAG(death_test_style);
  testing::GTEST_FLAG(death_test_style) = "threadsafe";
  using Vector = SmallVector<std::uint64_t, 0>;
  EXPECT_DEATH(
    {
      Vector values;
      values.reserve(std::numeric_limits<std::size_t>::max() / sizeof(std::uint64_t) + 1);
    },
    "");
  testing::GTEST_FLAG(death_test_style) = previous_style;
}
#endif

TEST(SmallVectorTest, ReservePublishesReplacementBeforeRetiringOldElements) {
  struct Value {
    std::reference_wrapper<std::function<void()>> retired;
    int value;
    Value(std::function<void()>& retired, int value) : retired(retired), value(value) {}
    Value(Value const&) = delete;
    Value(Value&& other) : retired(other.retired), value(std::exchange(other.value, -1)) {}
    ~Value() {
      if (value == -1)
        retired.get()();
    }
  };
  std::vector<int> observed;
  std::function<void()> retired;
  SmallVector<Value, 1> values;
  values.emplace_back(retired, 81);
  retired = [&] {
    for (auto const& value : values)
      observed.push_back(value.value);
  };
  values.reserve(4);
  EXPECT_EQ(observed, (std::vector<int>{81}));
  ASSERT_EQ(values.size(), 1u);
  EXPECT_EQ(values[0].value, 81);
}

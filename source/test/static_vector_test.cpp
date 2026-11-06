import star.static_vector;
import std;

#include "gtest/gtest.h"

using namespace Star;

TEST(StaticVectorTest, InsertErase) {
  typedef StaticVector<int, 64> SV;
  auto created = SV::create({1, 2, 3, 4});
  ASSERT_TRUE(created);
  auto& a = *created;
  EXPECT_EQ(a.size(), 4u);
  EXPECT_EQ(a, SV::create({1, 2, 3, 4}));
  auto shorter = SV::create({1, 2, 3});
  ASSERT_TRUE(shorter);
  EXPECT_NE(a, *shorter);
  ASSERT_TRUE(a.insert(a.begin(), 0));
  ASSERT_TRUE(a.insert(a.begin(), -1));
  EXPECT_EQ(a, SV::create({-1, 0, 1, 2, 3, 4}));
  ASSERT_TRUE(a.insert(a.begin(), {-3, -2}));
  EXPECT_EQ(a, SV::create({-3, -2, -1, 0, 1, 2, 3, 4}));
  a.erase(a.begin() + 1);
  EXPECT_EQ(a, SV::create({-3, -1, 0, 1, 2, 3, 4}));
  a.erase(a.begin(), a.begin() + 3);
  EXPECT_EQ(a, SV::create({1, 2, 3, 4}));
  ASSERT_TRUE(a.insert(a.end(), {5, 6, 7, 8}));
  EXPECT_EQ(a, SV::create({1, 2, 3, 4, 5, 6, 7, 8}));
  a.erase(a.begin() + 2, a.end() - 2);
  EXPECT_EQ(a, SV::create({1, 2, 7, 8}));
  ASSERT_TRUE(a.insert(a.begin() + 2, 6));
  ASSERT_TRUE(a.insert(a.begin() + 2, 5));
  ASSERT_TRUE(a.insert(a.begin() + 2, 4));
  ASSERT_TRUE(a.insert(a.begin() + 2, 3));
  EXPECT_EQ(a, SV::create({1, 2, 3, 4, 5, 6, 7, 8}));

  auto b = SV::create(a.begin(), a.end());
  ASSERT_TRUE(b);
  EXPECT_EQ(*b, SV::create({1, 2, 3, 4, 5, 6, 7, 8}));
}

TEST(StaticVectorTest, Comparators) {
  typedef StaticVector<int, 64> SV;

  auto base = SV::create({1, 2, 3, 4});
  auto greater = SV::create({1, 2, 3, 5});
  auto equal = SV::create({1, 2, 3, 4});
  auto lesser = SV::create({1, 2, 3, 3});
  auto shorter = SV::create({1, 2, 3});
  auto longer = SV::create({1, 2, 3, 4, 5});
  ASSERT_TRUE(base);
  ASSERT_TRUE(greater);
  ASSERT_TRUE(equal);
  ASSERT_TRUE(lesser);
  ASSERT_TRUE(shorter);
  ASSERT_TRUE(longer);
  EXPECT_TRUE(*base < *greater);
  EXPECT_FALSE(*base < *equal);
  EXPECT_FALSE(*base < *lesser);
  EXPECT_TRUE(*shorter < *base);
  EXPECT_FALSE(*longer < *base);
}

TEST(StaticVectorTest, EqualityNeedsOnlyContextualInequality) {
  struct Result {
    bool value;
    explicit operator bool() const { return value; }
    bool operator!() const { return value; }
  };
  struct Value {
    int value;
    int* comparisons;
    Result operator!=(Value const& other) const {
      ++*comparisons;
      return {value != other.value};
    }
  };
  int comparisons = 0;
  StaticVector<Value, 4> left;
  StaticVector<Value, 4> right;
  EXPECT_TRUE(left == right);
  ASSERT_TRUE(left.emplace_back(1, &comparisons));
  EXPECT_FALSE(left == right);
  EXPECT_EQ(comparisons, 0);
  ASSERT_TRUE(right.emplace_back(1, &comparisons));
  ASSERT_TRUE(left.emplace_back(2, &comparisons));
  ASSERT_TRUE(right.emplace_back(2, &comparisons));
  EXPECT_TRUE(left == right);
  comparisons = 0;
  EXPECT_TRUE(left == left);
  EXPECT_FALSE(left != left);
  EXPECT_EQ(comparisons, 0);
  right[0].value = 3;
  EXPECT_FALSE(left == right);
  EXPECT_EQ(comparisons, 1);
  EXPECT_TRUE(left != right);
}

TEST(StaticVectorTest, OrderingNeedsOnlyContextualLess) {
  struct Result {
    bool value;
    explicit operator bool() const { return value; }
    bool operator!() const { return value; }
  };
  struct Value {
    int value;
    Result operator<(Value const& other) const { return {value < other.value}; }
  };
  StaticVector<Value, 4> left;
  StaticVector<Value, 4> right;
  EXPECT_FALSE(left < right);
  ASSERT_TRUE(right.emplace_back(1));
  EXPECT_TRUE(left < right);
  EXPECT_FALSE(right < left);
  ASSERT_TRUE(left.emplace_back(1));
  EXPECT_FALSE(left < right);
  EXPECT_FALSE(left < left);
  ASSERT_TRUE(right.emplace_back(2));
  EXPECT_TRUE(left < right);
  EXPECT_FALSE(right < left);
  ASSERT_TRUE(left.emplace_back(3));
  EXPECT_FALSE(left < right);
  EXPECT_TRUE(right < left);
}

TEST(StaticVectorTest, SizeLimits) {
  StaticVector<int, 0> a;
  StaticVector<int, 1> b;
  ASSERT_TRUE(b.push_back(0));
  StaticVector<int, 2> c;
  ASSERT_TRUE(c.resize(2, 0));

  auto zeroCapacity = a.push_back(0);
  ASSERT_FALSE(zeroCapacity);
  EXPECT_TRUE(zeroCapacity.error().is<StaticVectorSizeException>());
  auto oneCapacity = b.push_back(0);
  ASSERT_FALSE(oneCapacity);
  EXPECT_TRUE(oneCapacity.error().is<StaticVectorSizeException>());
  auto twoCapacity = c.push_back(0);
  ASSERT_FALSE(twoCapacity);
  EXPECT_TRUE(twoCapacity.error().is<StaticVectorSizeException>());
  EXPECT_TRUE(a.empty());
  EXPECT_EQ(b.size(), 1u);
  EXPECT_EQ(b[0], 0);
  EXPECT_EQ(c.size(), 2u);
  EXPECT_EQ(c[0], 0);
  EXPECT_EQ(c[1], 0);
}

TEST(StaticVectorTest, CapacityRejectionPreservesLivePrefix) {
  struct Value {
    std::reference_wrapper<int> alive;
    int value;

    Value(int& alive, int value) : alive(alive), value(value) {
      ++alive;
    }
    ~Value() {
      --alive.get();
    }
  };
  int alive = 0;
  {
    StaticVector<Value, 2> values;
    ASSERT_TRUE(values.emplace_back(alive, 42));
    EXPECT_EQ(values.size(), 1u);
    EXPECT_EQ(values[0].value, 42);
    EXPECT_EQ(alive, 1);
    ASSERT_TRUE(values.emplace_back(alive, 7));
    EXPECT_EQ(values.size(), 2u);
    EXPECT_EQ(alive, 2);
    auto rejected = values.emplace_back(alive, 9);
    ASSERT_FALSE(rejected);
    EXPECT_TRUE(rejected.error().is<StaticVectorSizeException>());
    EXPECT_EQ(values.size(), 2u);
    EXPECT_EQ(values[0].value, 42);
    EXPECT_EQ(values[1].value, 7);
    EXPECT_EQ(alive, 2);
  }
  EXPECT_EQ(alive, 0);
}

TEST(StaticVectorTest, AlignedStorageAndZeroCapacity) {
  struct alignas(64) Value {
    int value;
  };
  static_assert(alignof(StaticVector<Value, 2>) == alignof(Value));
  static_assert(sizeof(StaticVector<Value, 2>) == 3 * sizeof(Value));
  static_assert(sizeof(StaticVector<int, 2>) == sizeof(std::size_t) + 2 * sizeof(int));
  static_assert(sizeof(StaticVector<Value, 0>) == 2 * sizeof(Value));
  StaticVector<Value, 0> empty;
  EXPECT_NE(empty.ptr(), nullptr);
  EXPECT_EQ(empty.begin(), empty.end());
  auto rejected = empty.emplace_back(42);
  ASSERT_FALSE(rejected);
  EXPECT_TRUE(rejected.error().is<StaticVectorSizeException>());
  EXPECT_TRUE(empty.empty());
  StaticVector<Value, 2> values;
  ASSERT_TRUE(values.emplace_back(1));
  ASSERT_TRUE(values.emplace_back(2));
  EXPECT_EQ(reinterpret_cast<std::uintptr_t>(values.ptr()) % alignof(Value), 0u);
  EXPECT_EQ(values[0].value, 1);
  EXPECT_EQ(values[1].value, 2);
  values.clear();
  ASSERT_TRUE(values.emplace_back(3));
  EXPECT_EQ(values[0].value, 3);
}

TEST(StaticVectorTest, SinglePassAndMoveOnlyInsertion) {
  std::istringstream stream("2 3 4");
  auto created = StaticVector<int, 5>::create({1});
  ASSERT_TRUE(created);
  auto& values = *created;
  auto inserted = values.insert(values.end(), std::istream_iterator<int>(stream), std::istream_iterator<int>{});
  ASSERT_TRUE(inserted);
  EXPECT_EQ(*inserted, values.begin() + 1);
  EXPECT_EQ(values, (StaticVector<int, 5>::create({1, 2, 3, 4})));
  StaticVector<std::unique_ptr<int>, 2> owners;
  ASSERT_TRUE(owners.emplace_back(std::make_unique<int>(7)));
  ASSERT_TRUE(owners.emplace(owners.begin(), std::make_unique<int>(9)));
  EXPECT_EQ(*owners[0], 9);
  EXPECT_EQ(*owners[1], 7);
}

TEST(StaticVectorTest, RangeOverflowRollsBackAppendedElements) {
  auto created = StaticVector<int, 3>::create({1, 2});
  ASSERT_TRUE(created);
  auto& values = *created;
  std::array<int, 2> more{3, 4};
  auto forwardFailure = values.insert(values.begin(), more.begin(), more.end());
  ASSERT_FALSE(forwardFailure);
  EXPECT_TRUE(forwardFailure.error().is<StaticVectorSizeException>());
  EXPECT_EQ(values, (StaticVector<int, 3>::create({1, 2})));
  std::istringstream stream("3 4");
  auto inputFailure = values.insert(values.begin(), std::istream_iterator<int>(stream), std::istream_iterator<int>{});
  ASSERT_FALSE(inputFailure);
  EXPECT_TRUE(inputFailure.error().is<StaticVectorSizeException>());
  EXPECT_EQ(values, (StaticVector<int, 3>::create({1, 2})));
}

TEST(StaticVectorTest, CapacityFailureDoesNotConsumeEmplaceArgument) {
  StaticVector<std::unique_ptr<int>, 1> values;
  ASSERT_TRUE(values.emplace_back(std::make_unique<int>(7)));
  auto candidate = std::make_unique<int>(9);
  auto rejected = values.emplace(values.begin(), std::move(candidate));
  ASSERT_FALSE(rejected);
  EXPECT_TRUE(rejected.error().is<StaticVectorSizeException>());
  EXPECT_EQ(values.size(), 1u);
  ASSERT_TRUE(candidate);
  EXPECT_EQ(*candidate, 9);
  EXPECT_EQ(*values[0], 7);
}

namespace {
struct StaticVectorLifetime {
  int alive = 0;
  int copies = 0;
  int moves = 0;
  int assignments = 0;
  int destroyed = 0;
};

struct NondefaultStaticValue {
  std::reference_wrapper<StaticVectorLifetime> state;
  int value;

  NondefaultStaticValue(StaticVectorLifetime& state, int value) : state(state), value(value) {
    ++state.alive;
  }
  NondefaultStaticValue(NondefaultStaticValue const& other) : state(other.state), value(other.value) {
    ++state.get().copies;
    ++state.get().alive;
  }
  NondefaultStaticValue(NondefaultStaticValue&& other) : state(other.state), value(other.value) {
    ++state.get().moves;
    other.value = -1;
    ++state.get().alive;
  }
  NondefaultStaticValue& operator=(NondefaultStaticValue const& other) {
    ++state.get().assignments;
    value = other.value;
    return *this;
  }
  NondefaultStaticValue& operator=(NondefaultStaticValue&& other) {
    ++state.get().assignments;
    value = std::exchange(other.value, -1);
    return *this;
  }
  ~NondefaultStaticValue() {
    ++state.get().destroyed;
    --state.get().alive;
  }
};
}

TEST(StaticVectorTest, NondefaultAssignmentConstructsOnlyNewElements) {
  StaticVectorLifetime state;
  {
    StaticVector<NondefaultStaticValue, 4> source;
    ASSERT_TRUE(source.emplace_back(state, 10));
    ASSERT_TRUE(source.emplace_back(state, 20));
    ASSERT_TRUE(source.emplace_back(state, 30));
    StaticVector<NondefaultStaticValue, 4> target;
    ASSERT_TRUE(target.emplace_back(state, 1));
    target = source;
    EXPECT_EQ(state.assignments, 1);
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
    EXPECT_EQ(state.assignments, 2);
    EXPECT_EQ(state.copies, 2);
    EXPECT_EQ(state.destroyed - destroyed, 2);
    EXPECT_EQ(state.alive, 2);
    auto& alias = target;
    target = alias;
    target = std::move(alias);
    EXPECT_EQ(state.assignments, 2);
    EXPECT_EQ(target[0].value, 10);

    ASSERT_TRUE(target.assign({NondefaultStaticValue(state, 40), NondefaultStaticValue(state, 50)}));
    ASSERT_EQ(target.size(), 2u);
    EXPECT_EQ(target[0].value, 40);
    EXPECT_EQ(target[1].value, 50);
    EXPECT_EQ(state.assignments, 3);
    EXPECT_EQ(state.copies, 3);
    EXPECT_EQ(state.alive, 3);
    ASSERT_TRUE(target.assign({}));
    EXPECT_TRUE(target.empty());
    EXPECT_EQ(state.alive, 1);
  }
  EXPECT_EQ(state.alive, 0);
}

TEST(StaticVectorTest, MoveOnlyAssignmentAndEraseNeedNoCopyOrDefault) {
  struct Value {
    std::reference_wrapper<int> alive;
    int value;
    Value(int& alive, int value) : alive(alive), value(value) { ++alive; }
    Value(Value const&) = delete;
    Value(Value&& other) : alive(other.alive), value(std::exchange(other.value, -1)) { ++alive.get(); }
    Value& operator=(Value&& other) {
      value = std::exchange(other.value, -1);
      return *this;
    }
    ~Value() { --alive.get(); }
  };
  int alive = 0;
  {
    StaticVector<Value, 4> source;
    ASSERT_TRUE(source.emplace_back(alive, 10));
    ASSERT_TRUE(source.emplace_back(alive, 20));
    ASSERT_TRUE(source.emplace_back(alive, 30));
    StaticVector<Value, 4> target;
    ASSERT_TRUE(target.emplace_back(alive, 1));
    target = std::move(source);
    ASSERT_EQ(target.size(), 3u);
    EXPECT_EQ(target[0].value, 10);
    EXPECT_EQ(target[2].value, 30);
    EXPECT_EQ(alive, 6);
    EXPECT_EQ(target.erase(target.begin() + 1, target.begin() + 1), target.begin() + 1);
    EXPECT_EQ(target[1].value, 20);
    EXPECT_EQ(target.erase(target.begin()), target.begin());
    EXPECT_EQ(target[0].value, 20);
    EXPECT_EQ(alive, 5);
    EXPECT_EQ(target.erase(target.begin(), target.end()), target.begin());
    EXPECT_TRUE(target.empty());
    EXPECT_EQ(alive, 3);
    ASSERT_TRUE(target.emplace_back(alive, 40));
    ASSERT_TRUE(target.emplace_back(alive, 50));
    source.clear();
    ASSERT_TRUE(source.emplace_back(alive, 60));
    target = std::move(source);
    EXPECT_EQ(target.size(), 1u);
    EXPECT_EQ(target[0].value, 60);
    EXPECT_EQ(alive, 2);
  }
  EXPECT_EQ(alive, 0);
}

TEST(StaticVectorTest, AssignmentPreservesForwardEffectsAndReverseTailDestruction) {
  struct Value {
    std::vector<int>* effects;
    int value;
    int slot;
    Value(std::vector<int>& effects, int value) : effects(&effects), value(value), slot(value) {}
    Value(Value const&) = default;
    Value(Value&&) = default;
    Value& operator=(Value const& other) {
      effects->push_back(other.value);
      value = other.value;
      return *this;
    }
    Value& operator=(Value&& other) {
      effects->push_back(100 + other.value);
      value = std::exchange(other.value, -1);
      return *this;
    }
    ~Value() { effects->push_back(-slot); }
  };
  std::vector<int> effects;
  StaticVector<Value, 4> source;
  ASSERT_TRUE(source.emplace_back(effects, 10));
  ASSERT_TRUE(source.emplace_back(effects, 20));
  StaticVector<Value, 4> target;
  for (int value : {1, 2, 3, 4})
    ASSERT_TRUE(target.emplace_back(effects, value));
  target = source;
  EXPECT_EQ(effects, (std::vector<int>{10, 20, -4, -3}));
  ASSERT_TRUE(target.emplace_back(effects, 3));
  ASSERT_TRUE(target.emplace_back(effects, 4));
  effects.clear();
  target = std::move(source);
  EXPECT_EQ(effects, (std::vector<int>{110, 120, -4, -3}));
  ASSERT_EQ(source.size(), 2u);
  EXPECT_EQ(source[0].value, -1);
  EXPECT_EQ(source[1].value, -1);
  ASSERT_TRUE(target.emplace_back(effects, 3));
  ASSERT_TRUE(target.emplace_back(effects, 4));
  std::initializer_list<Value> replacement{Value(effects, 30), Value(effects, 40)};
  effects.clear();
  ASSERT_TRUE(target.assign(replacement));
  EXPECT_EQ(effects, (std::vector<int>{30, 40, -4, -3}));
  ASSERT_EQ(target.size(), 2u);
  EXPECT_EQ(target[0].value, 30);
  EXPECT_EQ(target[1].value, 40);
}

TEST(StaticVectorTest, EraseShiftsElementsAppendedDuringMoveAssignment) {
  struct Value {
    int value;
    std::function<void()>* onMove;
    Value(int value, std::function<void()>& onMove) : value(value), onMove(&onMove) {}
    Value(Value&&) = default;
    Value& operator=(Value&& other) {
      value = std::exchange(other.value, -1);
      (*onMove)();
      return *this;
    }
  };
  for (bool rangeErase : {false, true}) {
    StaticVector<Value, 4> values;
    bool appended = false;
    std::function<void()> onMove = [&] {
      if (!std::exchange(appended, true))
        ASSERT_TRUE(values.emplace_back(4, onMove));
    };
    for (int value : {1, 2, 3})
      ASSERT_TRUE(values.emplace_back(value, onMove));
    auto erased = rangeErase ? values.erase(values.begin(), values.begin() + 1) : values.erase(values.begin());
    EXPECT_EQ(erased, values.begin());
    ASSERT_EQ(values.size(), 3u);
    EXPECT_EQ(values[0].value, 2);
    EXPECT_EQ(values[1].value, 3);
    EXPECT_EQ(values[2].value, 4);
  }
}

TEST(StaticVectorTest, InitializerAssignmentChecksCapacityBeforeMutation) {
  auto created = StaticVector<int, 2>::create({1});
  ASSERT_TRUE(created);
  auto& values = *created;
  auto rejected = values.assign({2, 3, 4});
  ASSERT_FALSE(rejected);
  EXPECT_TRUE(rejected.error().is<StaticVectorSizeException>());
  EXPECT_EQ(values, (StaticVector<int, 2>::create({1})));
}

TEST(StaticVectorTest, ResizePreservesValuesAndObjectLifetime) {
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
    StaticVector<Value, 4> values;
    ASSERT_TRUE(values.resize(2));
    ASSERT_EQ(values.size(), 2u);
    EXPECT_EQ(values[0].value, 7);
    EXPECT_EQ(values[1].value, 7);
    EXPECT_EQ(Value::live(), 2);
    values[0].value = 11;
    ASSERT_TRUE(values.resize(2));
    EXPECT_EQ(values.size(), 2u);
    EXPECT_EQ(values[0].value, 11);
    EXPECT_EQ(values[1].value, 7);
    EXPECT_EQ(Value::live(), 2);
    ASSERT_TRUE(values.resize(3));
    ASSERT_EQ(values.size(), 3u);
    EXPECT_EQ(values[0].value, 11);
    EXPECT_EQ(values[1].value, 7);
    EXPECT_EQ(values[2].value, 7);
    EXPECT_EQ(Value::live(), 3);
    ASSERT_TRUE(values.resize(1));
    ASSERT_EQ(values.size(), 1u);
    EXPECT_EQ(values[0].value, 11);
    EXPECT_EQ(Value::live(), 1);
    ASSERT_TRUE(values.resize(0));
    EXPECT_TRUE(values.empty());
    EXPECT_EQ(Value::live(), 0);
    ASSERT_TRUE(values.resize(1));
    EXPECT_EQ(Value::live(), 1);
  }
  EXPECT_EQ(Value::live(), 0);
}

TEST(StaticVectorTest, AliasedEmplaceStagesValue) {
  auto created = StaticVector<std::string, 4>::create({"first", "second"});
  ASSERT_TRUE(created);
  auto& text = *created;
  ASSERT_TRUE(text.emplace(text.begin() + 1, text[0]));
  EXPECT_EQ(text, (StaticVector<std::string, 4>::create({"first", "first", "second"})));
}


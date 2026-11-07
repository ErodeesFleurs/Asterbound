import star.list;
import star.format;

import std;

#include "gtest/gtest.h"

import star.algorithm;
import star.data_stream;
import star.data_stream_devices;
import star.data_stream_extra;
import star.bytes;
import star.array;
import star.vector;
import star.static_vector;
using namespace Star;

TEST(Take, DefaultConstructionFollowsOwnershipTransfer) {
  struct Value {
    static int& sequence() {
      static int value = 0;
      return value;
    }

    Value() : defaultOrder(++sequence()) {}
    explicit Value(int value) : owner(std::make_unique<int>(value)) {}
    Value(Value&& rhs)
        : owner(std::move(rhs.owner)), defaultOrder(rhs.defaultOrder),
          moveOrder(rhs.moveOrder ? rhs.moveOrder : ++sequence()) {}
    Value& operator=(Value&&) = default;

    std::unique_ptr<int> owner;
    int defaultOrder = 0;
    int moveOrder = 0;
  };

  Value::sequence() = 0;
  Value source(42);
  auto* identity = source.owner.get();
  auto taken = take(source);
  EXPECT_EQ(taken.owner.get(), identity);
  ASSERT_TRUE(taken.owner);
  EXPECT_EQ(*taken.owner, 42);
  EXPECT_FALSE(source.owner);
  EXPECT_EQ(taken.moveOrder, 1);
  EXPECT_EQ(source.defaultOrder, 2);
}

TEST(ListPredicates, MembershipConvertsToElementType) {
  auto expectMembership = [](auto const& values) {
    EXPECT_TRUE(values.contains(1));
    EXPECT_TRUE(values.contains(1.5));
    EXPECT_FALSE(values.contains(2));
    EXPECT_FALSE(values.contains(2.5));
  };

  expectMembership(List<int>{1});
  auto bounded = StaticList<int, 1>::create({1});
  ASSERT_TRUE(bounded);
  expectMembership(*bounded);
  expectMembership(SmallList<int, 1>{1});
  expectMembership(Deque<int>{1});
  expectMembership(LinkedList<int>{1});
}

TEST(ListPredicates, Any) {
  int a = 60;
  List<int> asdf{1, 2, 3, 4, 5, 6};

  EXPECT_TRUE(asdf.any([&](int b) { return b < a; }));
  EXPECT_FALSE(asdf.any([&](int b) { return b > a; }));
  EXPECT_TRUE(asdf.any([&](int b) { return a % b == 0; }));

  List<bool> b{false, false, false, true};
  List<bool> c{false, false, false, false};
  List<bool> d{false, false, true, true};
  List<bool> e{true, true, true, true};
  List<int> f{0, 1, 0, 0, 0, 3};

  EXPECT_TRUE(b.any());
  EXPECT_FALSE(c.any());
  EXPECT_TRUE(d.any());
  EXPECT_TRUE(e.any());
  EXPECT_TRUE(f.any());
}

TEST(ListPredicates, All) {
  int a = 60;
  List<int> asdf{1, 2, 3, 4, 5, 6};

  EXPECT_TRUE(asdf.all([&](int b) { return b < a; }));
  EXPECT_FALSE(asdf.all([&](int b) { return b > a; }));
  EXPECT_TRUE(asdf.all([&](int b) { return a % b == 0; }));

  List<bool> b{false, false, false, true};
  List<bool> c{false, false, false, false};
  List<bool> d{false, false, true, true};
  List<bool> e{true, true, true, true};
  List<int> f{0, 1, 0, 0, 0, 3};

  EXPECT_FALSE(b.all());
  EXPECT_FALSE(c.all());
  EXPECT_FALSE(d.all());
  EXPECT_TRUE(e.all());
  EXPECT_FALSE(f.all());
}

TEST(ListPredicates, ExpectedBoolPayloadErrorsAndShortCircuit) {
  List<std::expected<bool, StarException>> falseValues{false, false};
  List<std::expected<bool, StarException>> trueValues{true, true};
  EXPECT_EQ(falseValues.any(), false);
  EXPECT_EQ(falseValues.all(), false);
  EXPECT_EQ(trueValues.any(), true);
  EXPECT_EQ(trueValues.all(), true);

  List<std::expected<bool, StarException>> anyError{
      false, std::unexpected(OutOfRangeException("any predicate marker", false)), true};
  auto anyResult = anyError.any();
  ASSERT_FALSE(anyResult);
  EXPECT_TRUE(anyResult.error().is<OutOfRangeException>());
  EXPECT_NE(std::string_view(anyResult.error().what()).find("any predicate marker"), std::string_view::npos);
  // The false first payload settles all() before reaching the stored error.
  EXPECT_EQ(anyError.all(), false);

  List<std::expected<bool, StarException>> allError{
      true, std::unexpected(OutOfRangeException("all predicate marker", false)), false};
  auto allResult = allError.all();
  ASSERT_FALSE(allResult);
  EXPECT_TRUE(allResult.error().is<OutOfRangeException>());
  EXPECT_NE(std::string_view(allResult.error().what()).find("all predicate marker"), std::string_view::npos);
  // The true first payload settles any() before reaching the stored error.
  EXPECT_EQ(allError.any(), true);
}

TEST(ContainerOperators, allTests) {
  List<bool> a{false, false, true, false};
  List<int> b{1, 1, 0, 1};
  List<int> c = a.transformed([](bool a) { return a ? 0 : 1; });
  List<int> d{1, 2, 3, 5};
  List<int> e{1, 3, 5};
  List<int> f = d.filtered([](int i) { return i % 2 == 1; });

  EXPECT_TRUE(a.any());
  EXPECT_FALSE(a.all());
  EXPECT_EQ(b, c);
  EXPECT_EQ(e, f);
}

TEST(ListPredicates, NoncopyableExplicitBoolShortCircuit) {
  struct Truth {
    Truth(bool value, int& visits) : value(value), visits(visits) {}
    Truth(Truth const&) = delete;
    explicit operator bool() const {
      ++visits;
      return value;
    }
    bool value;
    int& visits;
  };

  int visits = 0;
  LinkedList<Truth> values;
  EXPECT_FALSE(values.emplaceAppend(false, visits).value);
  EXPECT_TRUE(values.emplaceAppend(true, visits).value);
  EXPECT_FALSE(values.emplaceAppend(false, visits).value);
  EXPECT_TRUE(values.any());
  EXPECT_EQ(visits, 2);
  visits = 0;
  EXPECT_FALSE(values.all());
  EXPECT_EQ(visits, 1);
  visits = 0;
  LinkedList<Truth> empty;
  EXPECT_FALSE(empty.any());
  EXPECT_TRUE(empty.all());
  EXPECT_EQ(visits, 0);

  int calls = 0;
  auto predicate = [&](Truth const& value) {
    ++calls;
    return static_cast<bool>(value);
  };
  EXPECT_TRUE(values.any(predicate));
  EXPECT_EQ(calls, 2);
  calls = 0;
  EXPECT_FALSE(values.all(predicate));
  EXPECT_EQ(calls, 1);
  calls = 0;
  EXPECT_FALSE(empty.any(predicate));
  EXPECT_TRUE(empty.all(predicate));
  EXPECT_EQ(calls, 0);
}

TEST(TupleSerialization, OrderedWireAndEnumWidths) {
  enum class Signed : std::int8_t { Negative = -2 };
  enum class Unsigned : std::uint32_t { High = 0x89abcdefu };
  auto values = std::tuple{Signed::Negative, std::uint16_t{0x1234}, Unsigned::High};
  DataStreamBuffer stream;
  ASSERT_TRUE(stream << values);
  std::array<unsigned char, 7> expected{0xfe, 0x12, 0x34, 0x89, 0xab, 0xcd, 0xef};
  ASSERT_EQ(stream.size(), expected.size());
  for (std::size_t i = 0; i < expected.size(); ++i)
    EXPECT_EQ(static_cast<unsigned char>(stream.span()[i]), expected[i]);

  ASSERT_TRUE(stream.seek(0));
  decltype(values) restored{};
  ASSERT_TRUE(stream >> restored);
  EXPECT_EQ(restored, values);
  auto atEnd = stream.at_end();
  ASSERT_TRUE(atEnd);
  EXPECT_TRUE(*atEnd);

  DataStreamBuffer empty;
  std::tuple<> emptyTuple;
  ASSERT_TRUE(empty << emptyTuple);
  EXPECT_TRUE(empty.empty());
  ASSERT_TRUE(empty >> emptyTuple);
  auto position = empty.pos();
  ASSERT_TRUE(position);
  EXPECT_EQ(*position, 0u);
}

TEST(ListFiltering, MoveOnlyValuesKeepStableOrder) {
  List<std::unique_ptr<int>> values;
  for (int value = 0; value < 8; ++value)
    values.append(std::make_unique<int>(value));
  values.filter([](std::unique_ptr<int>& value) { return *value % 2 != 0; });
  ASSERT_EQ(values.size(), 4u);
  for (std::size_t i = 0; i < values.size(); ++i)
    EXPECT_EQ(*values[i], static_cast<int>(i * 2 + 1));
  values.filter([](auto const&) { return true; });
  EXPECT_EQ(values.size(), 4u);
  values.filter([](auto const&) { return false; });
  EXPECT_TRUE(values.empty());
}

TEST(ListAlgorithms, StatefulMoveOnlyCallbacksRemainBorrowed) {
  struct Transform {
    std::unique_ptr<int> next = std::make_unique<int>(1);
    int operator()(int value) { return value + (*next)++; }
  } transform;
  List<int> values{1, 2, 3};
  values.transform(transform);
  EXPECT_EQ(values, (List<int>{2, 4, 6}));
  EXPECT_EQ(*transform.next, 4);

  struct Accumulate {
    std::unique_ptr<int> total = std::make_unique<int>(0);
    void operator()(int& value) {
      *total += value;
      value = *total;
    }
  } accumulate;
  values.exec(accumulate);
  EXPECT_EQ(values, (List<int>{2, 6, 12}));
  EXPECT_EQ(*accumulate.total, 12);
}

TEST(ComputedSort, MoveOnlyNonDefaultKeysAndStableTies) {
  struct Key {
    explicit Key(int value) : value(std::make_unique<int>(value)) {}
    Key(Key&&) = default;
    Key& operator=(Key&&) = default;
    bool operator<(Key const& other) const { return *value < *other.value; }
    std::unique_ptr<int> value;
  };
  List<int> values{21, 10, 22, 11, 20};
  List<int> visited;
  stableSortByComputedValue(values, [&](int value) {
    visited.append(value);
    return Key(value / 10);
  });
  EXPECT_EQ(visited, (List<int>{21, 10, 22, 11, 20}));
  EXPECT_EQ(values, (List<int>{10, 11, 21, 22, 20}));

  auto result = sortByComputedValue(values, [](int value) -> std::expected<Key, StarException> { return Key(value); });
  ASSERT_TRUE(result);
  EXPECT_EQ(values, (List<int>{10, 11, 20, 21, 22}));
}

TEST(ComputedSort, FirstKeyFailureLeavesInputUnreordered) {
  List<int> const original{4, 1, 3, 2};
  for (std::size_t failure = 0; failure < original.size(); ++failure) {
    auto values = original;
    List<int> visited;
    auto result = stableSortByComputedValue(values, [&](int value) -> std::expected<int, StarException> {
      visited.append(value);
      if (visited.size() == failure + 1)
        return std::unexpected(OutOfRangeException("sort key boundary", false));
      return value;
    });
    ASSERT_FALSE(result);
    EXPECT_TRUE(result.error().is<OutOfRangeException>());
    EXPECT_EQ(values, original);
    ASSERT_EQ(visited.size(), failure + 1);
    for (std::size_t i = 0; i < visited.size(); ++i)
      EXPECT_EQ(visited[i], original[i]);
  }

  std::function<std::expected<int, StarException>(int)> emptyGetter;
  List<int> empty;
  List<int> singleton{3};
  EXPECT_TRUE(sortByComputedValue(empty, emptyGetter));
  EXPECT_TRUE(sortByComputedValue(singleton, emptyGetter));
  EXPECT_EQ(singleton, (List<int>{3}));
}

TEST(ListTransformed, MutableAndConstMoveOnlyCallbacksRemainBorrowed) {
  struct Transform {
    std::unique_ptr<int> next = std::make_unique<int>(1);
    int operator()(int& value) { return value += (*next)++; }
    int operator()(int const& value) { return value + (*next)++; }
  } callback;
  List<int> values{10, 20, 30};
  auto mutableResult = values.transformed(callback);
  EXPECT_EQ(mutableResult, (List<int>{11, 22, 33}));
  EXPECT_EQ(values, mutableResult);
  EXPECT_EQ(*callback.next, 4);
  auto constResult = std::as_const(values).transformed(callback);
  EXPECT_EQ(constResult, (List<int>{15, 27, 39}));
  EXPECT_EQ(values, mutableResult);
  EXPECT_EQ(*callback.next, 7);

  auto temporaryResult = std::as_const(values).transformed(
    [offset = std::make_unique<int>(2)](int value) mutable { return value + (*offset)++; });
  EXPECT_EQ(temporaryResult, (List<int>{13, 25, 37}));
}

TEST(ListTransformed, FirstFailurePreservesCallbackAndInputPrefix) {
  List<int> values{1, 2, 3, 4};
  int calls = 0;
  auto mutableResult = values.transformed([&](int& value) -> std::expected<int, StarException> {
    if (++calls == 3)
      return std::unexpected(OutOfRangeException("mutable transform boundary", false));
    return value *= 10;
  });
  ASSERT_FALSE(mutableResult);
  EXPECT_TRUE(mutableResult.error().is<OutOfRangeException>());
  EXPECT_EQ(calls, 3);
  EXPECT_EQ(values, (List<int>{10, 20, 3, 4}));

  calls = 0;
  auto constResult = std::as_const(values).transformed([&](int const& value) -> std::expected<int, StarException> {
    if (++calls == 2)
      return std::unexpected(OutOfRangeException("const transform boundary", false));
    return value + 1;
  });
  ASSERT_FALSE(constResult);
  EXPECT_TRUE(constResult.error().is<OutOfRangeException>());
  EXPECT_EQ(calls, 2);
  EXPECT_EQ(values, (List<int>{10, 20, 3, 4}));

  auto success =
    std::as_const(values).transformed([](int value) -> std::expected<int, StarException> { return value + 1; });
  ASSERT_TRUE(success);
  EXPECT_EQ(*success, (List<int>{11, 21, 4, 5}));
}

TEST(ContainerTransform, BoundedOutputStopsAtInsertionFailure) {
  List<int> values{1, 2, 3, 4};
  int calls = 0;
  auto result = Star::transform<StaticList<int, 2>>(values, [&](int value) {
    ++calls;
    return value * 10;
  });
  ASSERT_FALSE(result);
  EXPECT_TRUE(result.error().is<StaticVectorSizeException>());
  EXPECT_EQ(calls, 3);

  StaticList<int, 2> output;
  ASSERT_TRUE(output.append(99));
  calls = 0;
  auto appended = transformInto(output, values, [&](int value) {
    ++calls;
    return value * 10;
  });
  ASSERT_FALSE(appended);
  EXPECT_TRUE(appended.error().is<StaticVectorSizeException>());
  EXPECT_EQ(calls, 2);
  ASSERT_EQ(output.size(), 2u);
  EXPECT_EQ(output[0], 99);
  EXPECT_EQ(output[1], 10);
}

TEST(ContainerTransform, UnsizedSinglePassInputIsNotPretraversed) {
  std::istringstream input("3 1 4 2");
  auto values = std::ranges::istream_view<int>(input);
  auto result = Star::transform<std::vector<int>>(values, [](int value) { return value * 2; });
  EXPECT_EQ(result, (std::vector<int>{6, 2, 8, 4}));
}

TEST(ByteOrder, FloatingPayloadAndIntegerRepresentations) {
  auto opposite = platformByteOrder() == ByteOrder::LittleEndian ? ByteOrder::BigEndian : ByteOrder::LittleEndian;
  for (auto bits : {std::uint32_t{0}, std::uint32_t{0x80000000}, std::uint32_t{0x7fc12345}, std::uint32_t{0x3f800000}}) {
    auto value = std::bit_cast<float>(bits);
    EXPECT_EQ(std::bit_cast<std::uint32_t>(toByteOrder(opposite, value)), std::byteswap(bits));
    EXPECT_EQ(std::bit_cast<std::uint32_t>(fromByteOrder(opposite, toByteOrder(opposite, value))), bits);
  }
  EXPECT_EQ(toByteOrder(opposite, std::uint16_t{0x1234}), 0x3412u);
  EXPECT_EQ(toByteOrder(opposite, std::int32_t{-2}), std::byteswap(std::int32_t{-2}));
  EXPECT_EQ(toByteOrder(ByteOrder::NoConversion, std::uint64_t{0x12345678}), 0x12345678u);
}

TEST(FixedContainerCopy, PreservesSinglePassInputPosition) {
  std::istringstream arrayInput("1 2 3 4 5");
  auto array = Array<int, 3>::copy_from(std::istream_iterator<int>(arrayInput), 2);
  EXPECT_EQ(array, (Array<int, 3>{1, 2, 0}));
  int next = 0;
  arrayInput >> next;
  EXPECT_EQ(next, 4);

  std::istringstream vectorInput("1 2 3 4 5");
  auto vector = Vector<int, 2>::copyFrom(std::istream_iterator<int>(vectorInput));
  EXPECT_EQ(vector, (Vector<int, 2>{1, 2}));
  vectorInput >> next;
  EXPECT_EQ(next, 4);
}

TEST(ArrayComparison, EqualityUsesContextualInequalityAndStopsAtFirstDifference) {
  struct Truth {
    bool value;
    explicit operator bool() const { return value; }
    bool operator!() const { return value; }
  };
  struct Value {
    int value = 0;
    std::vector<int>* visited = nullptr;
    Truth operator!=(Value const& other) const {
      visited->push_back(value);
      return {value != other.value};
    }
    bool operator==(Value const&) const = delete;
  };
  std::vector<int> visited;
  Array<Value, 3> left{Value{1, &visited}, Value{2, &visited}, Value{3, &visited}};
  Array<Value, 3> equal{Value{1, &visited}, Value{2, &visited}, Value{3, &visited}};
  EXPECT_TRUE(left == equal);
  EXPECT_EQ(visited, (std::vector<int>{1, 2, 3}));
  visited.clear();
  Array<Value, 3> different{Value{1, &visited}, Value{9, &visited}, Value{3, &visited}};
  EXPECT_FALSE(left == different);
  EXPECT_EQ(visited, (std::vector<int>{1, 2}));
  visited.clear();
  EXPECT_TRUE((Array<Value, 0>{} == Array<Value, 0>{}));
  EXPECT_TRUE(visited.empty());
}

TEST(ArrayComparison, WeakOrderingUsesOnlyLessAndContinuesPastEquivalentElements) {
  struct Value {
    int value = 0;
    std::vector<std::pair<int, int>>* visited = nullptr;
    bool operator<(Value const& other) const {
      visited->emplace_back(value, other.value);
      return value / 10 < other.value / 10;
    }
  };
  std::vector<std::pair<int, int>> visited;
  Array<Value, 2> left{Value{7, &visited}, Value{20, &visited}};
  Array<Value, 2> right{Value{9, &visited}, Value{30, &visited}};
  static_assert(std::same_as<decltype(left <=> right), std::weak_ordering>);
  EXPECT_TRUE((left <=> right) == std::weak_ordering::less);
  EXPECT_EQ(visited, (std::vector<std::pair<int, int>>{{7, 9}, {9, 7}, {20, 30}}));
  visited.clear();
  EXPECT_TRUE((right <=> left) == std::weak_ordering::greater);
  EXPECT_EQ(visited, (std::vector<std::pair<int, int>>{{9, 7}, {7, 9}, {30, 20}, {20, 30}}));
  visited.clear();
  EXPECT_TRUE((left <=> left) == std::weak_ordering::equivalent);
  EXPECT_EQ(visited, (std::vector<std::pair<int, int>>{{7, 7}, {7, 7}, {20, 20}, {20, 20}}));
  visited.clear();
  EXPECT_TRUE((Array<Value, 0>{} <=> Array<Value, 0>{}) == std::weak_ordering::equivalent);
  EXPECT_TRUE(visited.empty());
}

TEST(ArrayComparison, FloatingInfinityNanKeepsLegacyLexicographicContinuation) {
  auto compare = [] [[gnu::noinline]] (Array<double, 2> const& left, Array<double, 2> const& right) {
    return left <=> right;
  };
  volatile std::uint64_t infinityBits = 0x7ff0000000000000ull;
  volatile std::uint64_t nanBits = 0x7ff8000000000001ull;
  Array<double, 2> left{std::bit_cast<double>(std::uint64_t(infinityBits)), 2.0};
  Array<double, 2> right{std::bit_cast<double>(std::uint64_t(nanBits)), 3.0};
  EXPECT_TRUE(compare(left, right) == std::weak_ordering::less);
}

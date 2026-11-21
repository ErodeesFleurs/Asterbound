import std;
import star.format;
import star.text_formatter;
import star.optional;
import star.data_stream_devices;
import star.data_stream_extra;
import star.net_element_dynamic_group;
import star.net_element_group;

import star.either;
import star.list;
#include "gtest/gtest.h"

using namespace Star;

namespace {
struct DiagnosticPayload {
  int identity;
  bool fail;
  std::reference_wrapper<std::vector<int>> visited;
};

[[nodiscard]] std::expected<void, StarException> write_text(TextBuffer& output, DiagnosticPayload const& value) {
  value.visited.get().push_back(value.identity);
  output.push_back(static_cast<char>('0' + value.identity));
  if (value.fail)
    return std::unexpected(OutOfRangeException("optional diagnostic boundary", false));
  return {};
}

template <typename T, std::size_t N>
void expectVariantWire(T const& value, std::array<unsigned char, N> const& expected) {
  DataStreamBuffer stream;
  auto written = stream << value;
  ASSERT_TRUE(written);
  ASSERT_EQ(stream.size(), expected.size());
  for (std::size_t i = 0; i < expected.size(); ++i)
    EXPECT_EQ(static_cast<unsigned char>(stream.span()[i]), expected[i]);

  auto sought = stream.seek(0);
  ASSERT_TRUE(sought);
  T restored{};
  auto read = stream >> restored;
  ASSERT_TRUE(read);
  EXPECT_EQ(restored, value);
  auto atEnd = stream.at_end();
  ASSERT_TRUE(atEnd);
  EXPECT_TRUE(*atEnd);
}
}

TEST(OptionalOperations, TakeTransfersOwnership) {
  auto owner = std::make_shared<int>(42);
  std::weak_ptr<int> weak = owner;
  std::optional<std::shared_ptr<int>> source(owner);
  owner.reset();
  auto extracted = take_optional(source);
  ASSERT_TRUE(extracted);
  EXPECT_FALSE(source);
  auto empty = take_optional(source);
  ASSERT_FALSE(empty);
  EXPECT_TRUE(empty.error().is<OutOfRangeException>());
  EXPECT_EQ(**extracted, 42);
  EXPECT_FALSE(weak.expired());
  extracted->reset();
  EXPECT_TRUE(weak.expired());
}

TEST(OptionalOperations, ExplicitMoveOnlyNonAssignablePayloadTransfersIdentity) {
  struct Value {
    explicit Value(int value) : owner(std::make_unique<int>(value)) {}
    explicit Value(Value&& source) noexcept : owner(std::move(source.owner)) {}
    Value& operator=(Value&&) = delete;
    std::unique_ptr<int> owner;
  };
  std::optional<Value> source(std::in_place, 42);
  auto identity = std::ref(*source->owner);
  auto extracted = take_optional(source);
  ASSERT_TRUE(extracted);
  EXPECT_FALSE(source);
  ASSERT_TRUE(extracted->owner);
  EXPECT_EQ(*extracted->owner, 42);
  EXPECT_EQ(std::addressof(*extracted->owner), std::addressof(identity.get()));
}

TEST(OptionalOperations, NullableBorrowsDoNotRetainOwnershipOrAddConstness) {
  auto owner = std::make_shared<int>(0);
  std::weak_ptr<int> lifetime = owner;
  auto borrowed = optional_ref(std::as_const(owner));
  ASSERT_TRUE(borrowed);
  EXPECT_EQ(borrowed->get(), 0);
  borrowed->get() = 17;
  EXPECT_EQ(*owner, 17);
  EXPECT_EQ(owner.use_count(), 1);
  owner.reset();
  EXPECT_TRUE(lifetime.expired());
  EXPECT_FALSE(optional_ref(owner));
}

TEST(OptionalOperations, NestedDiagnosticsStopAtFirstPayloadFailure) {
  std::vector<int> visited;
  List<std::optional<DiagnosticPayload>> values{
    std::nullopt, DiagnosticPayload{1, false, std::ref(visited)},
    DiagnosticPayload{2, true, std::ref(visited)}, DiagnosticPayload{3, false, std::ref(visited)}};
  TextBuffer output;
  auto written = append_text(output, values);
  ASSERT_FALSE(written);
  EXPECT_TRUE(written.error().is<OutOfRangeException>());
  EXPECT_EQ(visited, (std::vector<int>{1, 2}));
}

namespace {
using MoveOnlyEither = Either<std::optional<std::unique_ptr<int>>, int>;
using DuplicateEither = Either<int, int>;

struct ThrowingReset : std::optional<int> {
  void reset() noexcept(false) { std::optional<int>::reset(); }
};

struct ThrowingMove {
  explicit ThrowingMove(int value) : value(value) {}
  ThrowingMove(ThrowingMove&& source) noexcept(false) : value(source.value) {}
  ThrowingMove& operator=(ThrowingMove&&) = delete;
  int value;
};

template <typename T>
concept has_either_borrow =
    requires(T&& value) { std::forward<T>(value).left(); }
    || requires(T&& value) { std::forward<T>(value).right(); }
    || requires(T&& value) { std::forward<T>(value).left_ref(); }
    || requires(T&& value) { std::forward<T>(value).right_ref(); }
    || requires(T&& value) { std::forward<T>(value).maybe_left_ref(); }
    || requires(T&& value) { std::forward<T>(value).maybe_right_ref(); };

template <typename T>
concept copies_left = requires(T const& value) { value.maybe_left(); };

template <typename T>
concept makes_byte_tag = requires(T value) { make_left<std::uint8_t>(value); };

using ShortEither = Either<std::uint16_t, std::uint16_t>;
static_assert(std::is_trivially_copyable_v<DuplicateEither>);
static_assert(std::is_trivially_move_constructible_v<DuplicateEither>);
static_assert(std::is_trivially_move_assignable_v<DuplicateEither>);
static_assert(!std::is_copy_constructible_v<MoveOnlyEither> && !std::is_copy_assignable_v<MoveOnlyEither>);
static_assert(std::is_nothrow_move_constructible_v<MoveOnlyEither> && std::is_nothrow_move_assignable_v<MoveOnlyEither>);
static_assert(!std::is_nothrow_move_constructible_v<Either<int, ThrowingReset>>);
static_assert(!std::is_nothrow_move_assignable_v<Either<int, ThrowingReset>>);
using ConstructOnlyEither = Either<ThrowingMove, int>;
static_assert(!std::default_initializable<ConstructOnlyEither>);
static_assert(std::is_move_constructible_v<ConstructOnlyEither> && !std::is_nothrow_move_constructible_v<ConstructOnlyEither>);
static_assert(!std::is_move_assignable_v<ConstructOnlyEither> && !std::is_copy_constructible_v<ConstructOnlyEither>);
static_assert(!std::constructible_from<ConstructOnlyEither, decltype(make_left(123))>);
static_assert(!copies_left<MoveOnlyEither> && copies_left<DuplicateEither>);
static_assert(!has_either_borrow<DuplicateEither> && !has_either_borrow<DuplicateEither const>);
static_assert(std::same_as<decltype(std::declval<DuplicateEither const&>().left_ref()), int const&>);
static_assert(std::same_as<decltype(std::declval<DuplicateEither&>().right_ref()), int&>);
static_assert(makes_byte_tag<std::uint8_t> && !makes_byte_tag<int>);
static_assert(std::constructible_from<ShortEither, decltype(make_left(std::uint8_t{}))>);
static_assert(!std::constructible_from<ShortEither, decltype(make_left(std::uint32_t{}))>);
static_assert(!std::is_assignable_v<ShortEither&, decltype(make_right(1.5))>);
}

TEST(EitherOperations, MovesConsumeNullablePayloadWithoutChangingSide) {
  using Value = MoveOnlyEither;
  Value source(make_left(std::optional<std::unique_ptr<int>>(std::in_place, std::make_unique<int>(42))));
  Value moved(std::move(source));
  EXPECT_TRUE(source.is_left());
  EXPECT_FALSE(source.left_ref());
  ASSERT_TRUE(moved.left_ref());
  EXPECT_EQ(**moved.left_ref(), 42);
  Value destination(make_right(7));
  destination = std::move(moved);
  EXPECT_TRUE(moved.is_left());
  EXPECT_FALSE(moved.left_ref());
  ASSERT_TRUE(destination.is_left());
  ASSERT_TRUE(destination.left_ref());
  EXPECT_EQ(**destination.left_ref(), 42);
  auto& alias = destination;
  destination = std::move(alias);
  ASSERT_TRUE(destination.left_ref());
  EXPECT_EQ(**destination.left_ref(), 42);

  using RightNullable = Either<int, std::optional<int>>;
  RightNullable right(make_right(std::optional{12}));
  RightNullable transferred(std::move(right));
  ASSERT_TRUE(right.is_right());
  EXPECT_FALSE(right.right_ref());
  EXPECT_EQ(transferred.right_ref(), std::optional{12});
}

TEST(EitherOperations, DuplicateSidesDistinguishBorrowsAndCopies) {
  DuplicateEither value;
  ASSERT_TRUE(value.is_left());
  EXPECT_EQ(value.left_ref(), 0);
  ASSERT_TRUE(value.left());
  value.left()->get() = 12;
  auto copy = value.maybe_left();
  ASSERT_TRUE(value.maybe_left_ref());
  value.maybe_left_ref()->get() = 34;
  EXPECT_EQ(copy, std::optional{12});
  EXPECT_FALSE(value.maybe_right());
  EXPECT_FALSE(value.maybe_right_ref());
  auto wrong_right = value.right();
  ASSERT_FALSE(wrong_right);
  EXPECT_TRUE(wrong_right.error().is<EitherException>());

  value.set_right(value.left_ref());
  ASSERT_TRUE(value.is_right());
  auto const& readonly = value;
  ASSERT_TRUE(readonly.right());
  EXPECT_EQ(readonly.right()->get(), 34);
  EXPECT_EQ(readonly.maybe_right_ref()->get(), 34);
  EXPECT_FALSE(readonly.maybe_left_ref());
  auto wrong_left = readonly.left();
  ASSERT_FALSE(wrong_left);
  EXPECT_TRUE(wrong_left.error().is<EitherException>());
}

TEST(EitherSerialization, DuplicatePayloadsUseHistoricalSideBytes) {
  for (char tag : {1, 2}) {
    ShortEither value = make_left(std::uint16_t{0x1234});
    if (tag == 2)
      value = make_right(std::uint16_t{0x1234});
    DataStreamBuffer output;
    ASSERT_TRUE(output << value);
    std::array<char, 3> bytes{tag, 0x12, 0x34};
    EXPECT_TRUE(std::ranges::equal(output.span(), bytes));

    std::array<char, 4> frame{tag, 0x12, 0x34, 0x5a};
    DataStreamExternalBuffer input(frame);
    ShortEither restored = make_right(std::uint16_t{0xffff});
    ASSERT_TRUE(input >> restored);
    ASSERT_EQ(restored.is_left(), tag == 1);
    EXPECT_EQ(tag == 1 ? restored.left_ref() : restored.right_ref(), 0x1234);
    auto sentinel = input.read<std::uint8_t>();
    ASSERT_TRUE(sentinel);
    EXPECT_EQ(*sentinel, 0x5a);
  }
}

TEST(EitherSerialization, UnrecognizedTagsConsumeOnlyTheDiscriminant) {
  for (char tag : {char{0}, char{3}, static_cast<char>(0xff)}) {
    std::array<char, 2> frame{tag, 0x5a};
    DataStreamExternalBuffer input(frame);
    ShortEither value = make_right(std::uint16_t{0x1234});
    ASSERT_TRUE(input >> value);
    ASSERT_TRUE(value.is_right());
    EXPECT_EQ(value.right_ref(), 0x1234);
    auto sentinel = input.read<std::uint8_t>();
    ASSERT_TRUE(sentinel);
    EXPECT_EQ(*sentinel, 0x5a);
  }
}

TEST(EitherSerialization, TruncatedReadsDoNotPublishPartialPayloads) {
  using Pair = std::pair<std::uint8_t, std::uint16_t>;
  Pair const original{0xab, 0xcdef};
  for (char tag : {1, 2}) {
    std::array<char, 4> frame{tag, 0x12, 0x34, 0x56};
    for (std::size_t length = 0; length < frame.size(); ++length) {
      DataStreamExternalBuffer input(std::span<char const>(frame).first(length));
      Either<Pair, Pair> value = make_left(original);
      ASSERT_FALSE(input >> value);
      ASSERT_TRUE(value.is_left());
      EXPECT_EQ(value.left_ref(), original);
    }
  }
}

TEST(VariantSerialization, DiscriminantAndPayloadBytes) {
  using Value = std::variant<std::uint16_t, std::uint32_t>;
  expectVariantWire(Value(std::in_place_index<0>, 0x1234),
      std::array<unsigned char, 3>{0x00, 0x12, 0x34});
  expectVariantWire(Value(std::in_place_index<1>, 0x89abcdefu),
      std::array<unsigned char, 5>{0x01, 0x89, 0xab, 0xcd, 0xef});
}

TEST(VariantSerialization, MonostateAndDuplicateAlternativeBytes) {
  using Value = std::variant<std::monostate, std::uint16_t, std::uint16_t>;
  expectVariantWire(Value{}, std::array<unsigned char, 1>{0x00});
  expectVariantWire(Value(std::in_place_index<1>, 0x1234),
      std::array<unsigned char, 3>{0x01, 0x12, 0x34});
  expectVariantWire(Value(std::in_place_index<2>, 0x1234),
      std::array<unsigned char, 3>{0x02, 0x12, 0x34});
}

TEST(VariantSerialization, DynamicGroupResetHasNoPayload) {
  NetElementDynamicGroup<NetElementGroup> master;
  master.init_net_version();

  DataStreamBuffer stream;
  auto written = master.write_net_delta(stream, 0);
  ASSERT_TRUE(written);
  ASSERT_TRUE(*written);
  // Delta flag, change code, reset alternative, terminator: no marker payload.
  std::array<unsigned char, 4> expected{0x00, 0x01, 0x00, 0x00};
  ASSERT_EQ(stream.size(), expected.size());
  for (std::size_t i = 0; i < expected.size(); ++i)
    EXPECT_EQ(static_cast<unsigned char>(stream.span()[i]), expected[i]);
  ASSERT_TRUE(stream.write<std::uint8_t>(0x5a));

  NetElementDynamicGroup<NetElementGroup> slave;
  auto added = slave.add_net_element(std::make_shared<NetElementGroup>());
  ASSERT_TRUE(added);
  ASSERT_FALSE(slave.net_element_ids().empty());
  ASSERT_TRUE(stream.seek(0));
  ASSERT_TRUE(slave.read_net_delta(stream));
  EXPECT_TRUE(slave.net_element_ids().empty());
  auto sentinel = stream.read<std::uint8_t>();
  ASSERT_TRUE(sentinel);
  EXPECT_EQ(*sentinel, 0x5a);
}

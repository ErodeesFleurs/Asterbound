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

TEST(OptionalOperations, EitherMovesConsumeNullablePayloadWithoutChangingSide) {
  using Value = Either<std::optional<std::unique_ptr<int>>, int>;
  Value source(makeLeft(std::optional<std::unique_ptr<int>>(std::in_place, std::make_unique<int>(42))));
  Value moved(std::move(source));
  EXPECT_TRUE(source.isLeft());
  EXPECT_FALSE(source.leftRef());
  ASSERT_TRUE(moved.leftRef());
  EXPECT_EQ(**moved.leftRef(), 42);
  Value destination(makeRight(7));
  destination = std::move(moved);
  EXPECT_TRUE(moved.isLeft());
  EXPECT_FALSE(moved.leftRef());
  ASSERT_TRUE(destination.isLeft());
  ASSERT_TRUE(destination.leftRef());
  EXPECT_EQ(**destination.leftRef(), 42);
  auto& alias = destination;
  destination = std::move(alias);
  ASSERT_TRUE(destination.leftRef());
  EXPECT_EQ(**destination.leftRef(), 42);
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
  master.initNetVersion();

  DataStreamBuffer stream;
  auto written = master.writeNetDelta(stream, 0);
  ASSERT_TRUE(written);
  ASSERT_TRUE(*written);
  // Delta flag, change code, reset alternative, terminator: no marker payload.
  std::array<unsigned char, 4> expected{0x00, 0x01, 0x00, 0x00};
  ASSERT_EQ(stream.size(), expected.size());
  for (std::size_t i = 0; i < expected.size(); ++i)
    EXPECT_EQ(static_cast<unsigned char>(stream.span()[i]), expected[i]);
  ASSERT_TRUE(stream.write<std::uint8_t>(0x5a));

  NetElementDynamicGroup<NetElementGroup> slave;
  auto added = slave.addNetElement(std::make_shared<NetElementGroup>());
  ASSERT_TRUE(added);
  ASSERT_FALSE(slave.netElementIds().empty());
  ASSERT_TRUE(stream.seek(0));
  ASSERT_TRUE(slave.readNetDelta(stream));
  EXPECT_TRUE(slave.netElementIds().empty());
  auto sentinel = stream.read<std::uint8_t>();
  ASSERT_TRUE(sentinel);
  EXPECT_EQ(*sentinel, 0x5a);
}

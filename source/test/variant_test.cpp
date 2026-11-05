import std;
import star.format;
import star.optional;
import star.data_stream_devices;
import star.data_stream_extra;
import star.net_element_dynamic_group;
import star.net_element_group;

#include "gtest/gtest.h"

using namespace Star;

namespace {
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
  auto atEnd = stream.atEnd();
  ASSERT_TRUE(atEnd);
  EXPECT_TRUE(*atEnd);
}
}

TEST(OptionalOperations, TakeTransfersOwnership) {
  auto owner = std::make_shared<int>(42);
  std::weak_ptr<int> weak = owner;
  std::optional<std::shared_ptr<int>> source(owner);
  owner.reset();
  auto extracted = takeOptional(source);
  ASSERT_TRUE(extracted);
  EXPECT_FALSE(source);
  auto empty = takeOptional(source);
  ASSERT_FALSE(empty);
  EXPECT_TRUE(empty.error().is<OutOfRangeException>());
  EXPECT_EQ(**extracted, 42);
  EXPECT_FALSE(weak.expired());
  extracted->reset();
  EXPECT_TRUE(weak.expired());
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

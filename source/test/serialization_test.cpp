import std;
import star.buffer;
import star.format;
import star.data_stream_devices;
import star.string;
import star.map;
import star.list;
import star.data_stream_extra;
import star.byte_array;
import star.multi_array;
import star.bytes;
import star.io_device;
import star.data_stream;

#include "gtest/gtest.h"

using namespace Star;

template <typename T>
void testMap(T const& map) {
  auto byteArray = DataStreamBuffer::serializeMapContainer(map);
  ASSERT_TRUE(byteArray);
  auto mapOut = DataStreamBuffer::deserializeMapContainer<T>(std::move(*byteArray));
  ASSERT_TRUE(mapOut);
  EXPECT_EQ(map, *mapOut);
}

TEST(DataStreamTest, All) {
  Map<int, int> map1 = {
      {1, 2}, {3, 4}, {5, 6},
  };

  Map<String, int> map2 = {
      {"asdf", 1}, {"asdf1", 2}, {"omg", 2},
  };

  Map<String, int> map3 = {};

  testMap(map1);
  testMap(map2);
  testMap(map3);
}

TEST(DataStreamTest, SequenceWireBytesAndTruncatedPrefix) {
  List<std::uint8_t> values{0x10, 0x20};
  auto encoded = DataStreamBuffer::serialize(values);
  ASSERT_TRUE(encoded);
  EXPECT_EQ(*encoded, ByteArray({"\x02\x10\x20", 3}));

  List<std::uint8_t> decoded{0x7f};
  auto read = DataStreamBuffer::deserialize(decoded, ByteArray({"\x03\x10\x20", 3}));
  ASSERT_FALSE(read);
  EXPECT_EQ(decoded, values);
}

TEST(DataStreamTest, BoundedDecodeRetainsAcceptedPrefixOnCapacityError) {
  StaticList<std::uint8_t, 2> decoded;
  auto read = DataStreamBuffer::deserialize(decoded, ByteArray({"\x03\x10\x20\x30", 4}));
  ASSERT_FALSE(read);
  ASSERT_EQ(decoded.size(), 2u);
  EXPECT_EQ(decoded[0], 0x10);
  EXPECT_EQ(decoded[1], 0x20);
}

TEST(DataStreamTest, OrderedMapWireBytesAndDuplicateKeys) {
  Map<std::uint8_t, std::uint8_t> values{{2, 0x22}, {1, 0x11}};
  auto encoded = DataStreamBuffer::serialize(values);
  ASSERT_TRUE(encoded);
  EXPECT_EQ(*encoded, ByteArray({"\x02\x01\x11\x02\x22", 5}));

  Map<std::uint8_t, std::uint8_t> decoded{{9, 9}};
  auto read = DataStreamBuffer::deserialize(decoded, ByteArray({"\x02\x07\x0b\x07\x16", 5}));
  ASSERT_TRUE(read);
  ASSERT_EQ(decoded.size(), 1u);
  auto value = decoded.get(7);
  ASSERT_TRUE(value);
  EXPECT_EQ(value->get(), 0x0b);
}

TEST(DataStreamTest, TupleFailureRetainsDecodedPrefixAndSkipsRemainingFields) {
  DataStreamBuffer stream(ByteArray({"\x12\x34\x56", 3}));
  std::tuple<std::uint8_t, std::uint32_t, std::uint8_t> decoded{0, 0xaabbccddu, 0xee};
  auto read = stream >> decoded;
  ASSERT_FALSE(read);
  EXPECT_TRUE(read.error().is<IOException>());
  EXPECT_EQ(std::get<0>(decoded), 0x12);
  EXPECT_EQ(std::get<1>(decoded), 0xaabbccddu);
  EXPECT_EQ(std::get<2>(decoded), 0xee);
  auto position = stream.pos();
  ASSERT_TRUE(position);
  EXPECT_EQ(*position, 3u);
}

TEST(DataStreamTest, OptionalAndVariantCommitOnlyCompletePayloads) {
  std::optional<std::uint32_t> optionalValue{0xaabbccddu};
  DataStreamBuffer optionalStream(ByteArray({"\x01\x12\x34", 3}));
  auto optionalRead = optionalStream >> optionalValue;
  ASSERT_FALSE(optionalRead);
  ASSERT_TRUE(optionalValue);
  EXPECT_EQ(*optionalValue, 0xaabbccddu);

  using Value = std::variant<std::monostate, std::uint16_t, std::uint32_t>;
  Value variantValue(std::in_place_index<1>, 0xabcd);
  DataStreamBuffer variantStream(ByteArray({"\x02\x12\x34", 3}));
  auto variantRead = variantStream >> variantValue;
  ASSERT_FALSE(variantRead);
  ASSERT_EQ(variantValue.index(), 1u);
  EXPECT_EQ(std::get<1>(variantValue), 0xabcd);

  DataStreamBuffer invalidTag(ByteArray({"\x03", 1}));
  auto invalidRead = invalidTag >> variantValue;
  ASSERT_FALSE(invalidRead);
  EXPECT_TRUE(invalidRead.error().is<DataStreamException>());
  ASSERT_EQ(variantValue.index(), 1u);
  EXPECT_EQ(std::get<1>(variantValue), 0xabcd);
}

TEST(DataStreamTest, OwnedBufferMovePreservesBytesCursorAndWireSettings) {
  DataStreamBuffer source;
  source.setByteOrder(ByteOrder::LittleEndian);
  source.setNullTerminatedStrings(true);
  source.setStreamCompatibilityVersion(17);
  ASSERT_TRUE(source.write<std::uint16_t>(0x1234));
  ASSERT_TRUE(source.writeData({"ab", 2}));
  ASSERT_TRUE(source.seek(2));

  DataStreamBuffer moved(std::move(source));
  source.reset(ByteArray({"x", 1}));
  EXPECT_EQ(moved.data(), ByteArray({"\x34\x12" "ab", 4}));
  EXPECT_EQ(moved.pos(), 2u);
  EXPECT_EQ(moved.byteOrder(), ByteOrder::LittleEndian);
  EXPECT_TRUE(moved.nullTerminatedStrings());
  EXPECT_EQ(moved.streamCompatibilityVersion(), 17u);

  DataStreamBuffer destination(ByteArray({"old", 3}));
  destination = std::move(moved);
  moved.reset(ByteArray({"y", 1}));
  EXPECT_EQ(destination.pos(), 2u);
  EXPECT_EQ(destination.byteOrder(), ByteOrder::LittleEndian);
  EXPECT_TRUE(destination.nullTerminatedStrings());
  EXPECT_EQ(destination.streamCompatibilityVersion(), 17u);
  char output[3]{};
  auto read = destination.readData(output);
  ASSERT_FALSE(read);
  EXPECT_TRUE(read.error().is<EofException>());
  EXPECT_EQ(ByteArray(std::span(output).first(2)), ByteArray({"ab", 2}));
  EXPECT_EQ(destination.pos(), 4u);
  auto atEnd = destination.at_end();
  ASSERT_TRUE(atEnd);
  EXPECT_TRUE(*atEnd);
  EXPECT_EQ(destination.takeData(), ByteArray({"\x34\x12" "ab", 4}));
  EXPECT_TRUE(destination.empty());
  EXPECT_EQ(destination.pos(), 0u);
}

TEST(DataStreamTest, IODeviceAdapterSharesCursorWithoutOwningDevice) {
  Buffer device(ByteArray({"abc", 3}));
  {
    DataStreamIODevice stream(device);
    ASSERT_TRUE(stream.seek(1));
    EXPECT_EQ(device.pos(), 1);
    char output[3]{};
    auto read = stream.readData(output);
    ASSERT_FALSE(read);
    EXPECT_TRUE(read.error().is<EofException>());
    EXPECT_EQ(ByteArray(std::span(output).first(2)), ByteArray({"bc", 2}));
    EXPECT_EQ(stream.pos(), 3);
    EXPECT_EQ(device.pos(), 3);
    ASSERT_TRUE(stream.seek(0));
    ASSERT_TRUE(stream.writeData({"z", 1}));
  }
  EXPECT_TRUE(device.is_open());
  EXPECT_EQ(device.data(), ByteArray({"zbc", 3}));
  EXPECT_EQ(device.pos(), 1);
}

TEST(ExternalBufferTest, ClosedDeviceRejectsZeroLengthRead) {
  ExternalBuffer buffer{std::span<char const>{}};
  auto read = buffer.read({});
  ASSERT_TRUE(read);
  EXPECT_EQ(*read, 0u);
  ASSERT_TRUE(buffer.close());
  auto closedRead = buffer.read({});
  ASSERT_FALSE(closedRead);
  EXPECT_TRUE(closedRead.error().is<IOException>());
}

TEST(BufferTest, SpanTransfersRespectSlicesShortReadsAndAbsolutePositions) {
  Buffer buffer(ByteArray({"abc", 3}));
  std::array<char, 6> output{'!', '!', '!', '!', '!', '!'};
  auto read = buffer.read(std::span(output).subspan(1, 4));
  ASSERT_TRUE(read);
  EXPECT_EQ(*read, 3u);
  EXPECT_EQ(output, (std::array<char, 6>{'!', 'a', 'b', 'c', '!', '!'}));
  EXPECT_EQ(buffer.pos(), 3);
  EXPECT_EQ(buffer.read(output), 0u);

  std::array<char, 4> input{'?', 'x', 'y', '?'};
  auto written = buffer.write_absolute(1, std::span<char const>(input).subspan(1, 2));
  ASSERT_TRUE(written);
  EXPECT_EQ(*written, 2u);
  EXPECT_EQ(buffer.pos(), 3);
  ASSERT_TRUE(buffer.read_full_absolute(0, std::span(output).subspan(2, 3)));
  EXPECT_EQ(output, (std::array<char, 6>{'!', 'a', 'a', 'x', 'y', '!'}));
  EXPECT_EQ(buffer.pos(), 3);
}

TEST(DataStreamTest, ExternalSpanBorrowsSlicesAndResetRewinds) {
  std::array<char, 4> bytes{0x10, 0x20, 0x30, 0x40};
  DataStreamExternalBuffer stream{std::span<char const>(bytes).subspan(1, 2)};
  EXPECT_EQ(stream.ptr(), bytes.data() + 1);
  EXPECT_EQ(stream.size(), 2u);
  bytes[1] = 0x25;
  auto first = stream.read<std::uint8_t>();
  ASSERT_TRUE(first);
  EXPECT_EQ(*first, 0x25);
  auto remaining = stream.remaining();
  ASSERT_TRUE(remaining);
  EXPECT_EQ(*remaining, 1u);
  auto write = stream.write<std::uint8_t>(0x7f);
  ASSERT_FALSE(write);
  EXPECT_EQ(bytes[2], 0x30);

  stream.reset(std::span<char const>(bytes).last(1));
  auto position = stream.pos();
  ASSERT_TRUE(position);
  EXPECT_EQ(*position, 0u);
  auto last = stream.read<std::uint8_t>();
  ASSERT_TRUE(last);
  EXPECT_EQ(*last, 0x40);
  EXPECT_FALSE(stream.read<std::uint8_t>());

  stream.reset(std::span<char const>{});
  auto atEnd = stream.at_end();
  ASSERT_TRUE(atEnd);
  EXPECT_TRUE(*atEnd);
  EXPECT_FALSE(stream.read<std::uint8_t>());
}

TEST(DataStreamTest, MultiArrayPreservesDimensionsAndLastIndexContiguousWireOrder) {
  MultiArray<std::uint8_t, 2> array(2, 3);
  {
    auto view = array.view();
    for (std::size_t x = 0; x < view.extent(0); ++x)
      for (std::size_t y = 0; y < view.extent(1); ++y)
        view[x, y] = static_cast<std::uint8_t>((x + 1) * 0x10 + y);
  }

  auto encoded = DataStreamBuffer::serialize(array);
  ASSERT_TRUE(encoded);
  EXPECT_EQ(*encoded, ByteArray({"\x02\x03\x10\x11\x12\x20\x21\x22", 8}));
  auto decoded = DataStreamBuffer::deserialize<MultiArray<std::uint8_t, 2>>(*encoded);
  ASSERT_TRUE(decoded);
  auto const view = std::as_const(*decoded).view();
  ASSERT_EQ(view.extent(0), 2u);
  ASSERT_EQ(view.extent(1), 3u);
  EXPECT_EQ((view[0, 2]), 0x12);
  EXPECT_EQ((view[1, 0]), 0x20);
  EXPECT_EQ((view[1, 2]), 0x22);
}

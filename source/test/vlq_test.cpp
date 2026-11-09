#include "gtest/gtest.h"

import std;
import star.vlq;
import star.math_common;
import star.data_stream;
import star.data_stream_devices;
import star.byte_array;
import star.buffer;
import star.io_device;
import star.format;
import star.memory;

using namespace Star;

TEST(VlqTest, All) {
  char buffer[10];
  std::int64_t result;

  EXPECT_EQ(writeVlqI(-1, buffer), 1u);
  EXPECT_EQ(readVlqI(result, buffer), 1u);
  EXPECT_EQ(result, -1);

  EXPECT_EQ(writeVlqI(-65, buffer), 2u);
  EXPECT_EQ(readVlqI(result, buffer), 2u);
  EXPECT_EQ(result, -65);

  EXPECT_EQ(writeVlqI(-64, buffer), 1u);
  EXPECT_EQ(readVlqI(result, buffer), 1u);
  EXPECT_EQ(result, -64);

  EXPECT_EQ(writeVlqI((std::int64_t)1 << 63, buffer), 10u);
  EXPECT_EQ(readVlqI(result, buffer), 10u);
  EXPECT_EQ(result, (std::int64_t)1 << 63);

  EXPECT_EQ(writeVlqI(0, buffer), 1u);
  EXPECT_EQ(readVlqI(result, buffer), 1u);
  EXPECT_EQ(result, 0);
}

TEST(VlqTest, UnsignedGroupBoundariesPreserveWireBytesAndFollowingByte) {
  struct Case {
    std::uint64_t value;
    std::initializer_list<unsigned char> bytes;
  };
  Case const cases[]{
    {0, {0x00}},
    {127, {0x7f}},
    {128, {0x81, 0x00}},
    {16384, {0x81, 0x80, 0x00}},
    {std::uint64_t{1} << 63, {0x81, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x00}},
    {std::numeric_limits<std::uint64_t>::max(), {0x81, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f}}};
  for (auto const& test : cases) {
    SCOPED_TRACE(test.value);
    std::array<unsigned char, 11> buffer;
    buffer.fill(0xa5);
    auto written = writeVlqU(test.value, buffer.begin());
    ASSERT_EQ(written, test.bytes.size());
    EXPECT_EQ(vlqUSize(test.value), test.bytes.size());
    EXPECT_TRUE(std::equal(test.bytes.begin(), test.bytes.end(), buffer.begin()));
    EXPECT_EQ(buffer[written], 0xa5);
    std::uint64_t restored = 0;
    EXPECT_EQ(readVlqU(restored, buffer.begin(), written), written);
    EXPECT_EQ(restored, test.value);
  }
}

TEST(VlqTest, StreamUnsignedWidthsTruncationAndFollowingByte) {
  for (unsigned width = 1; width <= 10; ++width) {
    SCOPED_TRACE(width);
    auto value = width == 10 ? std::numeric_limits<std::uint64_t>::max() : (std::uint64_t{1} << (7 * (width - 1)));
    DataStreamBuffer writer;
    ASSERT_EQ(writer.write_vlq_u(value), width);
    ASSERT_TRUE(writer.write<std::uint8_t>(0x5a));
    for (std::size_t length = 0; length < width; ++length) {
      SCOPED_TRACE(length);
      DataStreamReader borrowed(writer.span().first(length));
      DataStream& stream = borrowed;
      std::uint64_t decoded = 0x11223344;
      auto failed = stream.read_vlq_u(decoded);
      ASSERT_FALSE(failed);
      EXPECT_TRUE(failed.error().is<EofException>());
      EXPECT_EQ(decoded, 0x11223344u);
      EXPECT_EQ(borrowed.pos(), length);
      DataStreamBuffer owned(ByteArray(writer.span().first(length)));
      DataStream& ownedStream = owned;
      EXPECT_FALSE(ownedStream.read_vlq_u(decoded));
      EXPECT_EQ(decoded, 0x11223344u);
      EXPECT_EQ(owned.pos(), length);
    }
    DataStreamReader borrowed(writer.span());
    DataStream& stream = borrowed;
    std::uint64_t decoded = 0;
    EXPECT_EQ(stream.read_vlq_u(decoded), width);
    EXPECT_EQ(decoded, value);
    EXPECT_EQ(borrowed.pos(), width);
    EXPECT_EQ(stream.read<std::uint8_t>(), 0x5a);
    ASSERT_TRUE(writer.seek(0));
    DataStream& ownedStream = writer;
    EXPECT_EQ(ownedStream.read_vlq_u(), value);
    EXPECT_EQ(ownedStream.read<std::uint8_t>(), 0x5a);
  }
}

TEST(VlqTest, StreamMalformedNoncanonicalOverflowAndPastEnd) {
  std::array<char, 11> malformed;
  malformed.fill(static_cast<char>(0x80));
  malformed.back() = 0x5a;
  DataStreamReader borrowed(malformed);
  DataStream& stream = borrowed;
  std::uint64_t retained = 0x1234;
  auto result = stream.read_vlq_u(retained);
  ASSERT_FALSE(result);
  EXPECT_TRUE(result.error().is<DataStreamException>());
  EXPECT_EQ(retained, 0x1234u);
  EXPECT_EQ(borrowed.pos(), 10u);
  EXPECT_EQ(stream.read<std::uint8_t>(), 0x5a);
  DataStreamBuffer owned{ByteArray(malformed)};
  DataStream& ownedStream = owned;
  auto ownedResult = ownedStream.read_vlq_u(retained);
  ASSERT_FALSE(ownedResult);
  EXPECT_TRUE(ownedResult.error().is<DataStreamException>());
  EXPECT_EQ(retained, 0x1234u);
  EXPECT_EQ(owned.pos(), 10u);
  EXPECT_EQ(ownedStream.read<std::uint8_t>(), 0x5a);
  for (auto bytes : {ByteArray({"\x80\x00", 2}), ByteArray({"\x82\x80\x80\x80\x80\x80\x80\x80\x80\x00", 10})}) {
    DataStreamReader noncanonical(bytes.span());
    DataStream& base = noncanonical;
    EXPECT_EQ(base.read_vlq_u(), 0u);
    EXPECT_EQ(noncanonical.pos(), bytes.size());
  }
  ASSERT_TRUE(borrowed.seek(30));
  auto pastEnd = stream.read_vlq_u(retained);
  ASSERT_FALSE(pastEnd);
  EXPECT_TRUE(pastEnd.error().is<EofException>());
  EXPECT_EQ(retained, 0x1234u);
  EXPECT_EQ(borrowed.pos(), 30u);
}

TEST(VlqTest, StreamSignedExtremesAndNposUseContiguousBaseDispatch) {
  DataStreamBuffer writer;
  for (auto value : {std::numeric_limits<std::int64_t>::min(), std::int64_t(-1), std::int64_t(0),
                     std::numeric_limits<std::int64_t>::max()})
    ASSERT_TRUE(writer.write_vlq_i(value));
  ASSERT_TRUE(writer.write_vlq_s(NPos));
  ASSERT_TRUE(writer.write_vlq_s(0));
  DataStreamReader reader(writer.span());
  DataStream& stream = reader;
  for (auto value : {std::numeric_limits<std::int64_t>::min(), std::int64_t(-1), std::int64_t(0),
                     std::numeric_limits<std::int64_t>::max()})
    EXPECT_EQ(stream.read_vlq_i(), value);
  EXPECT_EQ(stream.read_vlq_s(), NPos);
  EXPECT_EQ(stream.read_vlq_s(), 0u);
}

TEST(VlqTest, GenericReadersPreserveReadErrorsAndClosedBufferMode) {
  std::size_t position = 0;
  DataStreamFunctions functionReader(
    [&](std::span<char> bytes) -> std::expected<std::size_t, StarException> {
      if (position == 2)
        return std::unexpected(IOException("reader failure"));
      bytes[0] = static_cast<char>(0x80);
      ++position;
      return 1;
    },
    {});
  DataStream& stream = functionReader;
  std::uint64_t retained = 0x1234;
  auto result = stream.read_vlq_u(retained);
  ASSERT_FALSE(result);
  EXPECT_TRUE(result.error().is<IOException>());
  EXPECT_FALSE(result.error().is<EofException>());
  EXPECT_EQ(position, 2u);
  EXPECT_EQ(retained, 0x1234u);
  Buffer buffer(ByteArray({"\x01", 1}));
  ASSERT_TRUE(buffer.close());
  DataStreamIODevice adapter(buffer);
  auto closed = adapter.read_vlq_u(retained);
  ASSERT_FALSE(closed);
  EXPECT_TRUE(closed.error().is<IOException>());
  EXPECT_FALSE(closed.error().is<EofException>());
  EXPECT_EQ(buffer.pos(), 0);
  EXPECT_EQ(retained, 0x1234u);
}

TEST(VlqTest, ExtensibleBufferReadersKeepVirtualReadErrorsAndCursor) {
  struct RejectingBuffer : DataStreamBuffer {
    using DataStreamBuffer::DataStreamBuffer;
    std::expected<void, StarException> read_data(std::span<char>) override {
      return std::unexpected(IOException("owned read rejected"));
    }
  };
  struct RejectingExternalBuffer : DataStreamExternalBuffer {
    using DataStreamExternalBuffer::DataStreamExternalBuffer;
    std::expected<void, StarException> read_data(std::span<char>) override {
      return std::unexpected(IOException("borrowed read rejected"));
    }
  };
  ByteArray bytes({"\x81\x01", 2});
  RejectingBuffer owned(bytes);
  RejectingExternalBuffer borrowed(bytes.span());
  auto verify = [](auto& adapter) {
    DataStream& stream = adapter;
    std::uint64_t unsignedValue = 0x1234;
    auto unsignedResult = stream.read_vlq_u(unsignedValue);
    ASSERT_FALSE(unsignedResult);
    EXPECT_TRUE(unsignedResult.error().is<IOException>());
    EXPECT_FALSE(unsignedResult.error().is<EofException>());
    EXPECT_EQ(unsignedValue, 0x1234u);
    EXPECT_EQ(adapter.pos(), 0u);
    std::int64_t signedValue = -73;
    auto signedResult = stream.read_vlq_i(signedValue);
    ASSERT_FALSE(signedResult);
    EXPECT_TRUE(signedResult.error().is<IOException>());
    EXPECT_EQ(signedValue, -73);
    EXPECT_EQ(adapter.pos(), 0u);
    std::size_t sizeValue = 42;
    auto sizeResult = stream.read_vlq_s(sizeValue);
    ASSERT_FALSE(sizeResult);
    EXPECT_TRUE(sizeResult.error().is<IOException>());
    EXPECT_EQ(sizeValue, 42u);
    EXPECT_EQ(adapter.pos(), 0u);
  };
  verify(owned);
  verify(borrowed);
}

TEST(VlqTest, ExtensibleBufferReadersDecodeTransformedBytesThroughVirtualRead) {
  struct TransformingBuffer : DataStreamBuffer {
    using DataStreamBuffer::DataStreamBuffer;
    std::expected<void, StarException> read_data(std::span<char> bytes) override {
      auto result = DataStreamBuffer::read_data(bytes);
      if (!result)
        return result;
      for (auto& byte : bytes)
        byte ^= 0x20;
      return {};
    }
  };
  struct TransformingExternalBuffer : DataStreamExternalBuffer {
    using DataStreamExternalBuffer::DataStreamExternalBuffer;
    std::expected<void, StarException> read_data(std::span<char> bytes) override {
      auto result = DataStreamExternalBuffer::read_data(bytes);
      if (!result)
        return result;
      for (auto& byte : bytes)
        byte ^= 0x20;
      return {};
    }
  };
  ByteArray bytes({"\xa1\x21\x7a", 3});
  TransformingBuffer owned(bytes);
  TransformingExternalBuffer borrowed(bytes.span());
  auto verify = [](auto& adapter) {
    DataStream& stream = adapter;
    EXPECT_EQ(stream.read_vlq_u(), 129u);
    EXPECT_EQ(adapter.pos(), 2u);
    EXPECT_EQ(stream.read<std::uint8_t>(), 0x5a);
    EXPECT_EQ(adapter.pos(), 3u);
  };
  verify(owned);
  verify(borrowed);
}

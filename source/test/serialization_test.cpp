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
import star.array;
import star.bytes;
import star.io_device;
import star.data_stream;
import star.uuid;
import star.matrix3;
import star.vector;
import star.color;

#include "gtest/gtest.h"

using namespace Star;

namespace {
struct NonDefaultMatrixScalar {
  std::uint16_t value;
  NonDefaultMatrixScalar() = delete;
  explicit NonDefaultMatrixScalar(std::uint16_t v) : value(v) {}
};

[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException>
operator>>(DataStream& stream, NonDefaultMatrixScalar& value);

static_assert(stream_readable<Matrix3<NonDefaultMatrixScalar>>);
static_assert(!stream_read_factory<Matrix3<NonDefaultMatrixScalar>>);

struct StagedOptionalPayload {
  std::uint8_t value = 0;
  StagedOptionalPayload() = default;
  StagedOptionalPayload(StagedOptionalPayload&&) = default;
  StagedOptionalPayload& operator=(StagedOptionalPayload&&) = delete;
  ~StagedOptionalPayload() noexcept(false) {}
};

struct OptionalPayloadReader {
  int calls = 0;
  OptionalPayloadReader() = default;
  OptionalPayloadReader(OptionalPayloadReader&&) = default;
  OptionalPayloadReader(OptionalPayloadReader const&) = delete;
  explicit operator bool() const { return false; }
  std::expected<std::reference_wrapper<DataStream>, StarException>
  operator()(DataStream& source, StagedOptionalPayload& payload) & {
    ++calls;
    return source >> payload.value;
  }
};

struct ImmobileOptionalPayload {
  ImmobileOptionalPayload() = default;
  ImmobileOptionalPayload(ImmobileOptionalPayload&&) = delete;
};

template <typename T, typename Function>
concept callback_optional_readable = requires(DataStream& stream, std::optional<T>& value, Function& function) {
  read_optional(stream, value, function);
};

using OptionalProbeReader = decltype([](DataStream&, auto&) -> std::expected<void, StarException> { return {}; });
static_assert(optional_stream_readable<Uuid>);
static_assert(optional_stream_writable<Uuid>);
static_assert(callback_optional_readable<StagedOptionalPayload, OptionalProbeReader>);
static_assert(!callback_optional_readable<Uuid, OptionalProbeReader>);
static_assert(!callback_optional_readable<ImmobileOptionalPayload, OptionalProbeReader>);
static_assert(!callback_optional_readable<int, decltype([](DataStream&, int&) { return 1; })>);
}

TEST(DataStreamTest, MatrixRowMajorWireBytesAndDefaultReadFactory) {
  using Matrix = Matrix3<std::uint16_t>;
  static_assert(stream_read_factory<Matrix>);
  Matrix const matrix(0x0102, 0x0304, 0x0506, 0x0708, 0x090a, 0x0b0c, 0x0d0e, 0x0f10, 0x1112);
  for (auto order : {ByteOrder::BigEndian, ByteOrder::LittleEndian}) {
    DataStreamBuffer stream;
    stream.set_byte_order(order);
    ASSERT_TRUE(stream << matrix);
    ASSERT_TRUE(stream << std::uint8_t(0x7e));
    ASSERT_EQ(stream.size(), 19u);
    for (std::size_t i = 0; i < 18; ++i) {
      auto expected = order == ByteOrder::BigEndian ? i + 1 : (i ^ 1u) + 1;
      EXPECT_EQ(static_cast<unsigned char>(stream.data()[i]), expected);
    }
    EXPECT_EQ(static_cast<unsigned char>(stream.data()[18]), 0x7e);
    ASSERT_TRUE(stream.seek(0));
    auto decoded = stream.read<Matrix>();
    ASSERT_TRUE(decoded) << decoded.error().what();
    EXPECT_EQ(*decoded, matrix);
    EXPECT_EQ(stream.read<std::uint8_t>(), 0x7e);
  }
}

TEST(DataStreamTest, ColorRetainsFourFloatChannelsInBothByteOrders) {
  std::array<std::uint8_t, 16> const big_endian{
      0x3f, 0x80, 0, 0, 0x3f, 0, 0, 0, 0, 0, 0, 0, 0x3e, 0x80, 0, 0};
  auto const color = Color::rgba_f(1.0f, 0.5f, 0.0f, 0.25f);
  for (auto order : {ByteOrder::BigEndian, ByteOrder::LittleEndian}) {
    DataStreamBuffer stream;
    stream.set_byte_order(order);
    ASSERT_TRUE(stream << color);
    ASSERT_TRUE(stream << std::uint8_t{0x7e});
    ASSERT_EQ(stream.size(), 17u);
    for (std::size_t i = 0; i < big_endian.size(); ++i)
      EXPECT_EQ(static_cast<std::uint8_t>(stream.data()[i]),
                big_endian[order == ByteOrder::BigEndian ? i : i ^ 3u]);
    ASSERT_TRUE(stream.seek(0));
    auto decoded = stream.read<Color>();
    ASSERT_TRUE(decoded);
    EXPECT_EQ(decoded->to_rgba_f(), Vec4F(1.0f, 0.5f, 0.0f, 0.25f));
    EXPECT_EQ(stream.read<std::uint8_t>(), 0x7e);
  }
}

TEST(DataStreamTest, TruncatedColorLeavesDestinationUnchanged) {
  DataStreamBuffer stream(ByteArray({"\x3f\x80\x00\x00\x3f\x00\x00\x00\x00\x00\x00\x00\x3e\x80\x00", 15}));
  stream.set_byte_order(ByteOrder::BigEndian);
  Color color = Color::Blue;
  auto decoded = stream >> color;
  ASSERT_FALSE(decoded);
  EXPECT_TRUE(decoded.error().is<EofException>());
  EXPECT_EQ(color, Color::Blue);
}

TEST(DataStreamTest, MatrixTruncationKeepsCompletedRowPrefixAndReportsFailure) {
  using Matrix = Matrix3<std::uint16_t>;
  DataStreamBuffer stream(ByteArray({"\x01\x02\x03\x04\x05\x06\x07", 7}));
  Matrix matrix(9, 9, 9, 9, 9, 9, 9, 9, 9);
  auto read = stream >> matrix;
  ASSERT_FALSE(read);
  EXPECT_EQ(matrix[0], (Vector<std::uint16_t, 3>(0x0102, 0x0304, 0x0506)));
  EXPECT_EQ(matrix[2], (Vector<std::uint16_t, 3>(9, 9, 9)));
}

template <typename T>
void testMap(T const& map) {
  auto byteArray = DataStreamBuffer::serialize_map_container(map);
  ASSERT_TRUE(byteArray);
  auto mapOut = DataStreamBuffer::deserialize_map_container<T>(byteArray->span());
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
  auto read = DataStreamBuffer::deserialize(decoded, std::span<char const>("\x03\x10\x20", 3));
  ASSERT_FALSE(read);
  EXPECT_EQ(decoded, values);
}

TEST(DataStreamTest, BoundedDecodeRetainsAcceptedPrefixOnCapacityError) {
  StaticList<std::uint8_t, 2> decoded;
  auto read = DataStreamBuffer::deserialize(decoded, std::span<char const>("\x03\x10\x20\x30", 4));
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
  auto read = DataStreamBuffer::deserialize(decoded, std::span<char const>("\x02\x07\x0b\x07\x16", 5));
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
  auto optionalPosition = optionalStream.pos();
  ASSERT_TRUE(optionalPosition);
  EXPECT_EQ(*optionalPosition, 3u);

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

TEST(DataStreamTest, OptionalPresenceFlagsPreserveOnMissingAndResetOnFalse) {
  std::optional<std::uint8_t> value{0x7f};
  DataStreamReader missing;
  auto missingResult = missing >> value;
  ASSERT_FALSE(missingResult);
  EXPECT_TRUE(missingResult.error().is<EofException>());
  ASSERT_TRUE(value);
  EXPECT_EQ(*value, 0x7f);
  EXPECT_EQ(missing.pos(), 0u);

  ByteArray bytes({"\x00\x55", 2});
  DataStreamReader absent(bytes.span());
  ASSERT_TRUE(absent >> value);
  EXPECT_FALSE(value);
  EXPECT_EQ(absent.pos(), 1u);
  EXPECT_EQ(absent.read<std::uint8_t>(), 0x55);
}

TEST(DataStreamTest, OptionalCallbackCommitsOnlySuccessfulPayloads) {
  std::optional<StagedOptionalPayload> value(std::in_place);
  value->value = 0x7f;
  ByteArray bytes({"\x01\x22", 2});
  DataStreamReader rejected(bytes.span());
  auto result = read_optional(rejected, value, [](DataStream& source, StagedOptionalPayload& payload)
      -> std::expected<void, StarException> {
    auto decoded = source.read(payload.value);
    if (!decoded)
      return decoded;
    return std::unexpected(DataStreamException("Rejected payload"));
  });
  ASSERT_FALSE(result);
  EXPECT_TRUE(result.error().is<DataStreamException>());
  ASSERT_TRUE(value);
  EXPECT_EQ(value->value, 0x7f);
  EXPECT_EQ(rejected.pos(), 2u);

  DataStreamReader accepted(bytes.span());
  OptionalPayloadReader reader;
  ASSERT_TRUE(read_optional(accepted, value, std::move(reader)));
  EXPECT_EQ(reader.calls, 1);
  ASSERT_TRUE(value);
  EXPECT_EQ(value->value, 0x22);
  EXPECT_EQ(accepted.pos(), 2u);
}

TEST(DataStreamTest, EmptyOptionalCallbacksAreCheckedOnlyAfterPresenceFlags) {
  std::function<std::expected<void, StarException>(DataStream&, std::uint8_t&)> reader;
  std::optional<std::uint8_t> value{0x7f};
  DataStreamReader missing;
  auto missingResult = read_optional(missing, value, reader);
  ASSERT_FALSE(missingResult);
  EXPECT_TRUE(missingResult.error().is<EofException>());
  ASSERT_TRUE(value);
  EXPECT_EQ(*value, 0x7f);

  ByteArray falseBytes({"\x00", 1});
  DataStreamReader absent(falseBytes.span());
  ASSERT_TRUE(read_optional(absent, value, reader));
  EXPECT_FALSE(value);
  EXPECT_EQ(absent.pos(), 1u);

  value.emplace(0x7f);
  ByteArray trueBytes({"\x01\x22", 2});
  DataStreamReader present(trueBytes.span());
  auto presentResult = read_optional(present, value, reader);
  ASSERT_FALSE(presentResult);
  EXPECT_TRUE(presentResult.error().is<DataStreamException>());
  ASSERT_TRUE(value);
  EXPECT_EQ(*value, 0x7f);
  EXPECT_EQ(present.pos(), 1u);

  std::function<std::expected<void, StarException>(DataStream&, std::uint8_t const&)> writer;
  DataStreamBuffer output;
  ASSERT_TRUE(write_optional(output, std::optional<std::uint8_t>{}, writer));
  auto writeResult = write_optional(output, value, writer);
  ASSERT_FALSE(writeResult);
  EXPECT_TRUE(writeResult.error().is<DataStreamException>());
  EXPECT_EQ(output.data(), ByteArray({"\x00\x01", 2}));
}

TEST(DataStreamTest, OwnedBufferMovePreservesBytesCursorAndWireSettings) {
  DataStreamBuffer source;
  source.set_byte_order(ByteOrder::LittleEndian);
  source.set_null_terminated_strings(true);
  source.set_stream_compatibility_version(17);
  ASSERT_TRUE(source.write<std::uint16_t>(0x1234));
  ASSERT_TRUE(source.write_data({"ab", 2}));
  ASSERT_TRUE(source.seek(2));

  DataStreamBuffer moved(std::move(source));
  source.reset(ByteArray({"x", 1}));
  EXPECT_EQ(moved.data(), ByteArray({"\x34\x12" "ab", 4}));
  EXPECT_EQ(moved.pos(), 2u);
  EXPECT_EQ(moved.byte_order(), ByteOrder::LittleEndian);
  EXPECT_TRUE(moved.null_terminated_strings());
  EXPECT_EQ(moved.stream_compatibility_version(), 17u);

  DataStreamBuffer destination(ByteArray({"old", 3}));
  destination = std::move(moved);
  moved.reset(ByteArray({"y", 1}));
  EXPECT_EQ(destination.pos(), 2u);
  EXPECT_EQ(destination.byte_order(), ByteOrder::LittleEndian);
  EXPECT_TRUE(destination.null_terminated_strings());
  EXPECT_EQ(destination.stream_compatibility_version(), 17u);
  char output[3]{};
  auto read = destination.read_data(output);
  ASSERT_FALSE(read);
  EXPECT_TRUE(read.error().is<EofException>());
  EXPECT_EQ(ByteArray(std::span(output).first(2)), ByteArray({"ab", 2}));
  EXPECT_EQ(destination.pos(), 4u);
  auto atEnd = destination.at_end();
  ASSERT_TRUE(atEnd);
  EXPECT_TRUE(*atEnd);
  EXPECT_EQ(destination.take_data(),
            ByteArray({"\x34\x12"
                       "ab",
                       4}));
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
    auto read = stream.read_data(output);
    ASSERT_FALSE(read);
    EXPECT_TRUE(read.error().is<EofException>());
    EXPECT_EQ(ByteArray(std::span(output).first(2)), ByteArray({"bc", 2}));
    EXPECT_EQ(stream.pos(), 3);
    EXPECT_EQ(device.pos(), 3);
    ASSERT_TRUE(stream.seek(0));
    ASSERT_TRUE(stream.write_data({"z", 1}));
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

TEST(BufferTest, SelfSpanWritesHandleBothOverlapDirectionsAndMovingGrowth) {
  Buffer buffer(ByteArray::from_string("abcdef"));
  auto forward = buffer.write_absolute(1, buffer.data().span().first(5));
  ASSERT_TRUE(forward) << forward.error().what();
  EXPECT_EQ(*forward, 5u);
  EXPECT_EQ(buffer.data(), ByteArray::from_string("aabcde"));
  EXPECT_EQ(buffer.pos(), 0);

  buffer.reset(ByteArray::from_string("abcdef"));
  auto backward = buffer.write_absolute(0, buffer.data().span().subspan(1));
  ASSERT_TRUE(backward) << backward.error().what();
  EXPECT_EQ(*backward, 5u);
  EXPECT_EQ(buffer.data(), ByteArray::from_string("bcdeff"));

  buffer.reset(ByteArray::from_string("abcdef"));
  ASSERT_TRUE(buffer.seek(3));
  auto overlappingGrowth = buffer.write(buffer.data().span().first(5));
  ASSERT_TRUE(overlappingGrowth) << overlappingGrowth.error().what();
  EXPECT_EQ(*overlappingGrowth, 5u);
  EXPECT_EQ(buffer.data(), ByteArray::from_string("abcabcde"));
  EXPECT_EQ(buffer.pos(), 8);

  auto retained = buffer.data();
  auto destination = buffer.data().capacity() + 17;
  auto growing = buffer.write_absolute(destination, buffer.data().span().subspan(2, 4));
  ASSERT_TRUE(growing) << growing.error().what();
  EXPECT_EQ(*growing, 4u);
  EXPECT_EQ(buffer.data().size(), destination + 4);
  EXPECT_EQ(ByteArray(buffer.data().span().first(retained.size())), retained);
  EXPECT_EQ(ByteArray(buffer.data().span().last(4)), ByteArray::from_string("cabc"));
  EXPECT_EQ(buffer.pos(), 8);
}

TEST(BufferTest, SelfSpanWriteKeepsZeroLengthModeAndArithmeticPrecedence) {
  Buffer buffer(ByteArray::from_string("abcdef"));
  auto original = buffer.data();
  ASSERT_TRUE(buffer.open(IOMode::Read));
  auto empty = buffer.write_absolute(std::numeric_limits<StreamOffset>::max(), {});
  ASSERT_TRUE(empty);
  EXPECT_EQ(*empty, 0u);
  auto modeFailure = buffer.write_absolute(std::numeric_limits<StreamOffset>::max(), buffer.data().span());
  ASSERT_FALSE(modeFailure);
  EXPECT_TRUE(modeFailure.error().is<EofException>());
  EXPECT_EQ(buffer.data(), original);
  EXPECT_EQ(buffer.pos(), 0);

  ASSERT_TRUE(buffer.open(IOMode::ReadWrite));
  auto rangeFailure = buffer.write_absolute(std::numeric_limits<StreamOffset>::max(), buffer.data().span());
  ASSERT_FALSE(rangeFailure);
  EXPECT_TRUE(rangeFailure.error().is<IOException>());
  EXPECT_FALSE(rangeFailure.error().is<EofException>());
  EXPECT_EQ(buffer.data(), original);
  EXPECT_EQ(buffer.pos(), 0);
  auto negativeEmpty = buffer.write_absolute(-1, {});
  ASSERT_FALSE(negativeEmpty);
  EXPECT_TRUE(negativeEmpty.error().is<IOException>());
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
  auto decoded = DataStreamBuffer::deserialize<MultiArray<std::uint8_t, 2>>(encoded->span());
  ASSERT_TRUE(decoded);
  auto const view = std::as_const(*decoded).view();
  ASSERT_EQ(view.extent(0), 2u);
  ASSERT_EQ(view.extent(1), 3u);
  EXPECT_EQ((view[0, 2]), 0x12);
  EXPECT_EQ((view[1, 0]), 0x20);
  EXPECT_EQ((view[1, 2]), 0x22);
}

TEST(DataStreamTest, MultiArrayRejectsOverflowingDimensionsAndRecovers) {
  MultiArray<std::uint8_t, 2> array({2, 2}, std::uint8_t{0x42});
  DataStreamBuffer stream;
  ASSERT_TRUE(stream.write_vlq_u(std::numeric_limits<std::size_t>::max()));
  ASSERT_TRUE(stream.write_vlq_u(2));
  ASSERT_TRUE(stream.seek(0));

  auto rejected = stream >> array;
  ASSERT_FALSE(rejected);
  EXPECT_TRUE(rejected.error().is<MultiArrayException>());
  EXPECT_EQ(array.extents(), (Array2S{2, 2}));
  for (auto value : array.elements())
    EXPECT_EQ(value, 0x42);

  stream.reset(ByteArray({"\x01\x02\x7a\x7b", 4}));
  ASSERT_TRUE(stream >> array);
  EXPECT_EQ(array.extents(), (Array2S{1, 2}));
  EXPECT_EQ(array(0, 0), 0x7a);
  EXPECT_EQ(array(0, 1), 0x7b);
}

TEST(UuidTest, HexParsingPreservesPermissiveNibblesOddTailAndLengthErrors) {
  std::array<unsigned char, 16> const expected{
      0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
      0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef};
  for (std::string_view text : {"0123456789abcdef0123456789abcdef", "0123456789ABCDEF0123456789ABCDEFf"}) {
    auto decoded = Uuid::create(String(text));
    ASSERT_TRUE(decoded);
    for (std::size_t i = 0; i < expected.size(); ++i)
      EXPECT_EQ(static_cast<unsigned char>(decoded->span()[i]), expected[i]);
  }
  std::string invalid(32, 'g');
  EXPECT_EQ(Uuid::create(String(std::string_view(invalid))), Uuid::nil());
  invalid[0] = '\0';
  EXPECT_EQ(Uuid::create(String(std::string_view(invalid))), Uuid::nil());
  for (std::size_t length : {0u, 1u, 30u, 31u, 34u, 35u}) {
    auto rejected = Uuid::create(String(std::string(length, '0')));
    ASSERT_FALSE(rejected);
    EXPECT_TRUE(rejected.error().is<UuidException>());
  }
  EXPECT_EQ(Uuid::create(String("00000000000000000000000000000000")), Uuid::nil());
}

TEST(DataStreamTest, UuidTruncatedReadsKeepIdentityAndConsumeAvailablePrefix) {
  std::array<char, 17> bytes;
  for (std::size_t i = 0; i < bytes.size(); ++i)
    bytes[i] = static_cast<char>(i + 1);
  auto identity = Uuid::create(ByteArray(std::span<char const>(bytes).first(16)));
  ASSERT_TRUE(identity);
  for (std::size_t length = 0; length < 16; ++length) {
    SCOPED_TRACE(length);
    auto retained = *identity;
    DataStreamReader borrowed(std::span<char const>(bytes).first(length));
    auto read = borrowed >> retained;
    ASSERT_FALSE(read);
    EXPECT_EQ(retained, *identity);
    EXPECT_EQ(borrowed.pos(), length);
    DataStreamBuffer owned(ByteArray(std::span<char const>(bytes).first(length)));
    auto ownedRead = owned >> retained;
    ASSERT_FALSE(ownedRead);
    EXPECT_EQ(retained, *identity);
    EXPECT_EQ(owned.pos(), length);
  }
  DataStreamReader stream(bytes);
  auto decoded = stream.read<Uuid>();
  ASSERT_TRUE(decoded);
  EXPECT_EQ(*decoded, *identity);
  EXPECT_EQ(stream.pos(), 16u);
  EXPECT_EQ(stream.read<std::uint8_t>(), 17u);
}

TEST(DataStreamTest, NonDefaultUuidPayloadsDecodeThroughContainersPointersAndOptional) {
  auto identity = Uuid::create(String("00112233445566778899aabbccddeeff"));
  ASSERT_TRUE(identity);
  List<Uuid> sequence{*identity};
  auto encoded = DataStreamBuffer::serialize(sequence);
  ASSERT_TRUE(encoded);
  List<Uuid> decoded;
  ASSERT_TRUE(DataStreamBuffer::deserialize_container(decoded, encoded->span()));
  ASSERT_EQ(decoded.size(), 1u);
  EXPECT_EQ(decoded[0], *identity);
  Map<Uuid, Uuid> map{{*identity, *identity}};
  auto encodedMap = DataStreamBuffer::serialize_map_container(map);
  ASSERT_TRUE(encodedMap);
  auto decodedMap = DataStreamBuffer::deserialize_map_container<Map<Uuid, Uuid>>(encodedMap->span());
  ASSERT_TRUE(decodedMap);
  EXPECT_EQ(*decodedMap, map);
  DataStreamBuffer stream;
  ASSERT_TRUE(stream.write(std::make_shared<Uuid>(*identity)));
  ASSERT_TRUE(stream.write(std::optional<Uuid>(*identity)));
  ASSERT_TRUE(stream.seek(0));
  auto pointer = stream.read<std::shared_ptr<Uuid>>();
  ASSERT_TRUE(pointer);
  ASSERT_TRUE(*pointer);
  EXPECT_EQ(**pointer, *identity);
  auto optional = stream.read<std::optional<Uuid>>();
  ASSERT_TRUE(optional);
  ASSERT_TRUE(*optional);
  EXPECT_EQ(**optional, *identity);
}

TEST(DataStreamTest, CallbackResultsAcceptStreamReferencesAndCommitOnlyCompleteElements) {
  ByteArray input({"\x02\x11\x22\x33", 4});
  DataStreamReader stream(input.span());
  List<std::uint16_t> values{0xffff};
  auto result = stream.read_container(values, [](DataStream& source, std::uint16_t& value) { return source >> value; });
  ASSERT_FALSE(result);
  ASSERT_EQ(values.size(), 1u);
  EXPECT_EQ(values[0], 0x1122);
  EXPECT_EQ(stream.pos(), 4u);
  auto encoded = DataStreamBuffer::serialize_container(
    values, [](DataStream& destination, std::uint16_t value) { return destination << value; });
  ASSERT_TRUE(encoded);
  auto decoded = DataStreamBuffer::deserialize_container<List<std::uint16_t>>(
    encoded->span(), [](DataStream& source, std::uint16_t& value) { return source >> value; });
  ASSERT_TRUE(decoded);
  EXPECT_EQ(*decoded, values);
}

TEST(DataStreamTest, MissingContainerCallbackFollowsCountAndClearPrecedence) {
  std::function<std::expected<std::reference_wrapper<DataStream>, StarException>(DataStream&, std::uint8_t&)> absent;
  List<std::uint8_t> values{0xff};
  DataStreamReader missingCount;
  auto missing = missingCount.read_container(values, absent);
  ASSERT_FALSE(missing);
  EXPECT_TRUE(missing.error().is<EofException>());
  EXPECT_TRUE(values.empty());
  ByteArray emptyBytes({"\x00", 1});
  DataStreamReader empty(emptyBytes.span());
  ASSERT_TRUE(empty.read_container(values, absent));
  ByteArray oneBytes({"\x01U", 2});
  DataStreamReader one(oneBytes.span());
  auto oneResult = one.read_container(values, absent);
  ASSERT_FALSE(oneResult);
  EXPECT_TRUE(oneResult.error().is<DataStreamException>());
  EXPECT_EQ(one.pos(), 1u);
  EXPECT_TRUE(values.empty());
}

TEST(DataStreamTest, BorrowedConvenienceResultsOwnDecodedFields) {
  using Value = std::pair<String, ByteArray>;
  Value original{String("owned"), ByteArray({"payload", 7})};
  auto encoded = DataStreamBuffer::serialize(original);
  ASSERT_TRUE(encoded);
  auto decoded = DataStreamBuffer::deserialize<Value>(encoded->span());
  ASSERT_TRUE(decoded);
  std::fill(encoded->span().begin(), encoded->span().end(), '\x00');
  EXPECT_EQ(*decoded, original);
}

TEST(DataStreamTest, BorrowedMapCallbacksRetainCompletePrefixAndDuplicateKeyPolicy) {
  ByteArray bytes({"\x03\x07\x11\x07\x22\x08", 6});
  Map<std::uint8_t, std::uint8_t> map{{9, 0xff}};
  auto callback = [](DataStream& stream, std::uint8_t& key, std::uint8_t& value) { return stream >> key >> value; };
  auto result = DataStreamBuffer::deserialize_map_container(map, bytes.span(), callback);
  ASSERT_FALSE(result);
  ASSERT_EQ(map.size(), 1u);
  auto first = map.get(7);
  ASSERT_TRUE(first);
  EXPECT_EQ(first->get(), 0x11);
  auto encoded = DataStreamBuffer::serialize_map_container(
    map, [](DataStream& stream, std::uint8_t key, std::uint8_t value) { return stream << key << value; });
  ASSERT_TRUE(encoded);
  auto restored =
    DataStreamBuffer::deserialize_map_container<Map<std::uint8_t, std::uint8_t>>(encoded->span(), callback);
  ASSERT_TRUE(restored);
  EXPECT_EQ(*restored, map);
  List<std::uint8_t> sequence;
  ASSERT_TRUE(DataStreamBuffer::deserialize_container(
    sequence, encoded->span(), [](DataStream& stream, std::uint8_t& value) { return stream >> value; }));
  ASSERT_EQ(sequence.size(), 1u);
  EXPECT_EQ(sequence[0], 7);
}

TEST(DataStreamTest, MissingMapCallbackStillClearsBeforeCountAndAcceptsEmptyMap) {
  std::function<std::expected<void, StarException>(DataStream&, std::uint8_t&, std::uint8_t&)> absent;
  Map<std::uint8_t, std::uint8_t> map{{1, 2}};
  DataStreamReader missing;
  auto failed = missing.read_map_container(map, absent);
  ASSERT_FALSE(failed);
  EXPECT_TRUE(failed.error().is<EofException>());
  EXPECT_TRUE(map.empty());
  ByteArray bytes({"\x00\x01\x07\x11", 4});
  DataStreamReader stream(bytes.span());
  ASSERT_TRUE(stream.read_map_container(map, absent));
  auto nonempty = stream.read_map_container(map, absent);
  ASSERT_FALSE(nonempty);
  EXPECT_TRUE(nonempty.error().is<DataStreamException>());
  EXPECT_EQ(stream.pos(), 2u);
  EXPECT_TRUE(map.empty());
}

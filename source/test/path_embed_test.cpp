#include <zlib-ng.h>
#include <zstd.h>

import std;
import star.format;
import star.path_embed;
import star.byte_array;
import star.string;
import star.json;
import star.data_stream_devices;

#include "gtest/gtest.h"

using namespace Star;

namespace {

String pathFor(std::span<char const> bytes, std::string_view type = "bin",
    EmbeddedCompressionMethod method = EmbeddedCompressionMethod::None) {
  auto payload = embedAndCompressData(bytes, method);
  return String("/opensb_data;" + std::string(type) + ";" + payload.value().utf8());
}

std::expected<Json, StarException> decodeJson(std::span<char const> bytes) {
  auto decoded = decodeEmbeddedData(pathFor(bytes, "json"));
  if (!decoded) return std::unexpected(std::move(decoded.error()));
  return embeddedDataToJson(*decoded);
}

// Produce a large logical stream from a small reusable input. Only the codec
// under test allocates its bounded decompressed output.
std::expected<ByteArray, StarException> compressed_zeros(std::size_t size, bool gzip = false) {
  zng_stream stream{};
  if (zng_deflateInit2(&stream, 1, Z_DEFLATED, gzip ? 31 : 15, 8, Z_DEFAULT_STRATEGY) != Z_OK)
    return std::unexpected(StarException("Cannot initialize test deflate"));
  struct StreamCleanup {
    zng_stream& stream;
    ~StreamCleanup() { (void)zng_deflateEnd(&stream); }
  } cleanup{stream};
  std::array<std::uint8_t, 16384> input{};
  std::array<char, 32768> output;
  ByteArray compressed;
  for (;;) {
    if (!stream.avail_in && size) {
      auto chunk = std::min(size, input.size());
      stream.next_in = input.data();
      stream.avail_in = chunk;
      size -= chunk;
    }
    stream.next_out = reinterpret_cast<std::uint8_t*>(output.data());
    stream.avail_out = output.size();
    auto status = zng_deflate(&stream, size ? Z_NO_FLUSH : Z_FINISH);
    if (status != Z_OK && status != Z_STREAM_END)
      return std::unexpected(StarException("Cannot generate test deflate stream"));
    compressed.append(std::span<char const>(output.data(), output.size() - stream.avail_out));
    if (status == Z_STREAM_END) return compressed;
  }
}

std::expected<ByteArray, StarException> compressed_zstd_zeros(std::size_t size) {
  auto cleanup = [](ZSTD_CCtx* context) noexcept { (void)ZSTD_freeCCtx(context); };
  std::unique_ptr<ZSTD_CCtx, decltype(cleanup)> context(ZSTD_createCCtx(), cleanup);
  if (!context || ZSTD_isError(ZSTD_CCtx_setParameter(context.get(), ZSTD_c_contentSizeFlag, 0)))
    return std::unexpected(StarException("Cannot initialize test Zstandard stream"));
  std::array<char, 16384> input{};
  std::array<char, 32768> output;
  ZSTD_inBuffer in{input.data(), 0, 0};
  ByteArray compressed;
  for (;;) {
    if (in.pos == in.size && size) {
      in.size = std::min(size, input.size());
      in.pos = 0;
      size -= in.size;
    }
    ZSTD_outBuffer out{output.data(), output.size(), 0};
    auto status = ZSTD_compressStream2(context.get(), &out, &in, size ? ZSTD_e_continue : ZSTD_e_end);
    if (ZSTD_isError(status))
      return std::unexpected(StarException("Cannot generate test Zstandard stream"));
    compressed.append(std::span<char const>(output.data(), out.pos));
    if (!size && !status && in.pos == in.size) return compressed;
  }
}

}

TEST(PathEmbedTest, CompressionNamesAreAsciiCaseInsensitive) {
  EXPECT_EQ(embeddedCompressionMethod("nOnE").value(), EmbeddedCompressionMethod::None);
  EXPECT_EQ(embeddedCompressionMethod("GZIP").value(), EmbeddedCompressionMethod::Gzip);
  EXPECT_EQ(embeddedCompressionMethod("ZlIb").value(), EmbeddedCompressionMethod::Zlib);
  EXPECT_EQ(embeddedCompressionMethod("zStD").value(), EmbeddedCompressionMethod::Zstd);
  EXPECT_FALSE(embeddedCompressionMethod(""));
  EXPECT_FALSE(embeddedCompressionMethod("zlib "));
  EXPECT_FALSE(embeddedCompressionMethod("brotli"));
}

TEST(PathEmbedTest, GeneratableCompressionModesRoundTripEveryByteAndEmptyInput) {
  ByteArray bytes(256, '\0');
  for (std::size_t i = 0; i < bytes.size(); ++i) bytes[i] = static_cast<char>(i);
  for (auto method : {EmbeddedCompressionMethod::None, EmbeddedCompressionMethod::Zlib, EmbeddedCompressionMethod::Zstd}) {
    for (auto input : std::array<std::span<char const>, 2>{bytes.span(), {}}) {
      auto encoded = embedAndCompressData(input, method);
      ASSERT_TRUE(encoded) << encoded.error().what();
      EXPECT_EQ(encoded->utf8().find('/'), std::string::npos);
      auto decoded = decodeEmbeddedData(String("/opensb_data;bin;" + encoded->utf8()));
      ASSERT_TRUE(decoded) << decoded.error().what();
      EXPECT_EQ(decoded->type, EmbeddedAssetType::Binary);
      EXPECT_EQ(decoded->bytes, ByteArray(input));
    }
  }
  EXPECT_FALSE(embedAndCompressData(bytes.span(), EmbeddedCompressionMethod::Gzip));
  auto explicitNone = decodeEmbeddedData("/opensb_data;bin;NoNe;AA==");
  ASSERT_TRUE(explicitNone);
  ASSERT_EQ(explicitNone->bytes.size(), 1u);
  EXPECT_EQ(explicitNone->bytes[0], '\0');
}

TEST(PathEmbedTest, IndependentGzipFixtureDecodesAndDetectsDamage) {
  // Generated by gzip -n, not by the embedded codec (which cannot emit gzip).
  auto decoded = decodeEmbeddedData("/opensb_data;bin;gzip;H4sIAAAAAAAAA0vNTUpNSUlNUUivyixgSMusKCktSuUCAGSMmgAWAAAA");
  ASSERT_TRUE(decoded) << decoded.error().what();
  EXPECT_EQ(decoded->bytes, ByteArray::from_string(std::string_view("embedded gzip\0fixture\n", 22)));
  EXPECT_FALSE(decodeEmbeddedData("/opensb_data;bin;gzip;H4sIAAAAAAAAA0vNTUpNSUlNUUivyixgSMusKCktSuUCAGSMmgAWAAA="));
  EXPECT_FALSE(decodeEmbeddedData("/opensb_data;bin;gzip;H4sIAAAAAAAAA0vNTUpNSUlNUUivyixgSMusKCktSuUCAGSMmgAXAAAA"));
  EXPECT_FALSE(decodeEmbeddedData("/opensb_data;bin;gzip;"));
}

TEST(PathEmbedTest, SpecialTextRoundTripsIncludingNulAndUtf8) {
  std::string text = "percent% colon: question? semicolon; slash/\n雪";
  text += '\0';
  text += "after NUL";
  auto escaped = embedEscapeText(String(text));
  ASSERT_TRUE(escaped);
  EXPECT_EQ(escaped->utf8().find("%%"), 7u);
  EXPECT_NE(escaped->utf8().find("%c"), std::string::npos);
  EXPECT_NE(escaped->utf8().find("%q"), std::string::npos);
  EXPECT_NE(escaped->utf8().find("%s"), std::string::npos);
  EXPECT_NE(escaped->utf8().find("%b"), std::string::npos);
  auto decoded = decodeEmbeddedData(String("/opensb_data;text;" + escaped->utf8()));
  ASSERT_TRUE(decoded);
  EXPECT_EQ(decoded->type, EmbeddedAssetType::Text);
  EXPECT_EQ(decoded->bytes, ByteArray::from_string(text));
  EXPECT_TRUE(decodeEmbeddedData("/opensb_data;text;"));
  for (auto bad : {"%", "%x", "abc%", "%C", "unescaped:colon", "unescaped?question", "unescaped/slash", "extra;field"})
    EXPECT_FALSE(decodeEmbeddedData(String("/opensb_data;text;" + std::string(bad)))) << bad;
}

TEST(PathEmbedTest, RejectsInvalidBase64AndExtraFieldsWithoutDroppingTokens) {
  for (auto bad : {"A", "A===", "AA=", "AA===", "AA==A", "AB==", "AAB=", "A A=", "AA/+=", "AA\n", "AA==;", "AA==;extra"})
    EXPECT_FALSE(decodeEmbeddedData(String("/opensb_data;bin;" + std::string(bad)))) << bad;
  for (auto good : {"AA", "AAA", "AAAA", "AA==", "AAA=", "", "-_8="})
    EXPECT_TRUE(decodeEmbeddedData(String("/opensb_data;bin;" + std::string(good)))) << good;
  for (auto bad : {"", "/opensb_data", "/opensb_data;", "/opensb_data;bin", "/OPENSB_data;bin;AA==", "/opensb_data;unknown;AA==", "/opensb_data;bin;brotli;AA=="})
    EXPECT_FALSE(decodeEmbeddedData(String(std::string(bad)))) << bad;
}

TEST(PathEmbedTest, BinaryJsonRetainsDataStreamWireForScalarsArraysAndObjects) {
  for (auto const& value : std::array<Json, 10>{Json(), Json(false), Json(true), Json(-1234567),
      Json(std::numeric_limits<std::int64_t>::min()), Json(std::numeric_limits<std::int64_t>::max()),
      Json(0.125), Json(std::string("nul\0text", 8)), Json(JsonArray{1, "two", JsonObject{{"ok", true}}}),
      Json(JsonObject{{"array", JsonArray{false, Json()}}, {"value", 9}})}) {
    auto wire = DataStreamBuffer::serialize(value);
    ASSERT_TRUE(wire);
    auto decoded = decodeJson(wire->span());
    ASSERT_TRUE(decoded) << decoded.error().what();
    EXPECT_EQ(*decoded, value);
  }
  for (char tag : {char{0}, char{1}}) {
    auto decoded = decodeJson(std::span<char const>(&tag, 1));
    ASSERT_TRUE(decoded);
    EXPECT_EQ(*decoded, Json());
  }
}

TEST(PathEmbedTest, BinaryJsonRejectsTruncationCountsOverflowAndTrailingBytes) {
  for (auto const& wire : std::vector<std::vector<unsigned char>>{
      {}, {255}, {2, 0}, {3}, {4, 128}, {5, 127}, {5, 2, 'a'}, {6, 127}, {7, 1},
      {7, 1, 2, 'a'}, {1, 1}, {4, 130, 128, 128, 128, 128, 128, 128, 128, 128, 0},
      {6, 255, 255, 255, 255, 255, 255, 255, 255, 255, 127},
      {5, 255, 255, 255, 255, 255, 255, 255, 255, 255, 127}}) {
    EXPECT_FALSE(decodeJson(std::span<char const>(reinterpret_cast<char const*>(wire.data()), wire.size())));
  }
}

TEST(PathEmbedTest, TextAndBinaryJsonEnforceDepth256WithoutChangingOrdinaryJson) {
  for (auto depth : {256u, 257u}) {
    std::string wire;
    for (unsigned i = 0; i < depth; ++i) wire.append({char{6}, char{1}});
    wire += char{1};
    auto binary = decodeJson(std::span<char const>(wire.data(), wire.size()));
    EXPECT_EQ(static_cast<bool>(binary), depth == 256);
    std::string text(depth, '[');
    text += '0';
    text.append(depth, ']');
    EmbeddedData data{EmbeddedAssetType::Binary, ByteArray::from_string(text), {}, {}};
    EXPECT_EQ(static_cast<bool>(embeddedDataToJson(data)), depth == 256);
  }
  EmbeddedData comments{EmbeddedAssetType::Text,
      ByteArray::from_string("/*[[[*/{\"braces\":\"[\\\"{\",\"value\":true}//[[[\r[[[\n"), {}, {}};
  EXPECT_TRUE(embeddedDataToJson(comments));
  for (auto text : {"null false", "{}x", "[0] trailing", "\"unfinished", "[}"}) {
    EmbeddedData data{EmbeddedAssetType::Binary, ByteArray::from_string(text), {}, {}};
    EXPECT_FALSE(embeddedDataToJson(data)) << text;
  }
  EmbeddedData nul{EmbeddedAssetType::Text, ByteArray::from_string(std::string_view("0\0", 2)), {}, {}};
  EXPECT_FALSE(embeddedDataToJson(nul));
  EXPECT_TRUE(Json::parse("[0]"));
}

TEST(PathEmbedTest, ImagesRequireUnambiguousFramesAndKeepCompleteFramesPath) {
  auto image = decodeEmbeddedData("/opensb_data;image;AA==;frames;text;{}");
  ASSERT_TRUE(image);
  EXPECT_EQ(image->type, EmbeddedAssetType::Image);
  EXPECT_EQ(image->framesData, std::optional<String>("/opensb_data;text;{}"));
  EXPECT_FALSE(image->framesPath);
  auto frames = decodeEmbeddedData(*image->framesData);
  ASSERT_TRUE(frames);
  EXPECT_EQ(embeddedDataToJson(*frames).value(), Json(JsonObject{}));
  auto path = decodeEmbeddedData("/opensb_data;image;AA==;framespath;/images/default.frames");
  ASSERT_TRUE(path);
  EXPECT_EQ(path->framesPath, std::optional<String>("/images/default.frames"));
  EXPECT_FALSE(path->framesData);
  EXPECT_FALSE(decodeEmbeddedData("/opensb_data;image;AA=="));
  EXPECT_TRUE(decodeEmbeddedData("/opensb_data;image;AA==", true));
  EXPECT_TRUE(decodeEmbeddedData("/opensb_data;bin;AA=="));
  EXPECT_FALSE(embeddedDataToJson(*image));

  auto frameWire = DataStreamBuffer::serialize(Json(JsonObject{{"frameGrid", JsonObject{{"size", JsonArray{1, 1}}}}}));
  ASSERT_TRUE(frameWire);
  for (auto method : {EmbeddedCompressionMethod::None, EmbeddedCompressionMethod::Zlib, EmbeddedCompressionMethod::Zstd}) {
    auto payload = embedAndCompressData(std::span<char const>("png", 3), method);
    auto framePayload = embedAndCompressData(frameWire->span(), method);
    ASSERT_TRUE(payload);
    ASSERT_TRUE(framePayload);
    auto decoded = decodeEmbeddedData(String("/opensb_data;image;" + payload->utf8() + ";frames;json;" + framePayload->utf8()));
    ASSERT_TRUE(decoded) << decoded.error().what();
    auto decodedFrames = decodeEmbeddedData(*decoded->framesData);
    ASSERT_TRUE(decodedFrames);
    EXPECT_TRUE(embeddedDataToJson(*decodedFrames));
  }
  for (auto suffix : {"frames", "framespath", "framespath;", "framespath;/a;extra", "unknown;/a",
      "frames;image;AA==;frames;text;{}", "frames;text;{};extra", "frames;json;AA==;extra", "frames;text"}) {
    auto invalidPath = String("/opensb_data;image;AA==;" + std::string(suffix));
    EXPECT_FALSE(decodeEmbeddedData(invalidPath)) << suffix;
    EXPECT_FALSE(decodeEmbeddedData(invalidPath, true)) << suffix;
  }
}

TEST(PathEmbedTest, CompressedStreamsRejectCorruptionTruncationAndConcatenation) {
  auto input = ByteArray::from_string("compressed payload");
  for (auto method : {EmbeddedCompressionMethod::Zlib, EmbeddedCompressionMethod::Zstd}) {
    auto encoded = embedAndCompressData(input.span(), method);
    ASSERT_TRUE(encoded);
    auto separator = encoded->utf8().find(';');
    auto token = encoded->utf8().substr(0, separator + 1);
    auto compressed = decodeEmbeddedData(String("/opensb_data;bin;" + encoded->utf8().substr(separator + 1)));
    ASSERT_TRUE(compressed);
    auto bytes = compressed->bytes;
    bytes.remove_suffix(1);
    EXPECT_FALSE(decodeEmbeddedData(String("/opensb_data;bin;" + token + embedAndCompressData(bytes.span(), EmbeddedCompressionMethod::None).value().utf8())));
    bytes = compressed->bytes;
    bytes[0] ^= 0x7f;
    EXPECT_FALSE(decodeEmbeddedData(String("/opensb_data;bin;" + token + embedAndCompressData(bytes.span(), EmbeddedCompressionMethod::None).value().utf8())));
    bytes = compressed->bytes;
    bytes.append(compressed->bytes);
    EXPECT_FALSE(decodeEmbeddedData(String("/opensb_data;bin;" + token + embedAndCompressData(bytes.span(), EmbeddedCompressionMethod::None).value().utf8())));
    bytes = compressed->bytes;
    bytes.push_back('\0');
    EXPECT_FALSE(decodeEmbeddedData(String("/opensb_data;bin;" + token + embedAndCompressData(bytes.span(), EmbeddedCompressionMethod::None).value().utf8())));
    EXPECT_FALSE(decodeEmbeddedData(String("/opensb_data;bin;" + token)));
  }
}

TEST(PathEmbedTest, GzipStreamsRejectTrailingBytesAndConcatenation) {
  auto compressed = compressed_zeros(17, true);
  ASSERT_TRUE(compressed);
  for (bool concatenate : {false, true}) {
    auto bytes = *compressed;
    if (concatenate)
      bytes.append(*compressed);
    else
      bytes.push_back('\0');
    auto encoded = embedAndCompressData(bytes.span(), EmbeddedCompressionMethod::None);
    ASSERT_TRUE(encoded);
    auto decoded = decodeEmbeddedData(String("/opensb_data;bin;gzip;" + encoded->utf8()));
    ASSERT_FALSE(decoded);
    EXPECT_NE(std::string(decoded.error().what()).find("trailing compressed bytes"), std::string::npos);
  }
}

TEST(PathEmbedTest, ZstandardUnknownSizeStreamsDecodeAndHugeDeclaredSizesFailEarly) {
  auto cleanup = [](ZSTD_CCtx* context) noexcept { (void)ZSTD_freeCCtx(context); };
  std::unique_ptr<ZSTD_CCtx, decltype(cleanup)> context(ZSTD_createCCtx(), cleanup);
  ASSERT_TRUE(context);
  ASSERT_FALSE(ZSTD_isError(ZSTD_CCtx_setParameter(context.get(), ZSTD_c_contentSizeFlag, 0)));
  ByteArray compressed(128, '\0');
  auto size = ZSTD_compress2(context.get(), compressed.data(), compressed.size(), "stream", 6);
  ASSERT_FALSE(ZSTD_isError(size));
  compressed.resize(size);
  auto encoded = embedAndCompressData(compressed.span(), EmbeddedCompressionMethod::None);
  ASSERT_TRUE(encoded);
  auto decoded = decodeEmbeddedData(String("/opensb_data;bin;zstd;" + encoded->utf8()));
  ASSERT_TRUE(decoded);
  EXPECT_EQ(decoded->bytes, ByteArray::from_string("stream"));
  // Single-segment frame header declaring 64MiB + 1, with no body. Rejection
  // must not reserve the attacker-controlled declared size.
  std::array<char, 9> header{char{0x28}, static_cast<char>(0xb5), char{0x2f},
      static_cast<char>(0xfd), static_cast<char>(0xa0), 1, 0, 0, 4};
  auto huge = embedAndCompressData(header, EmbeddedCompressionMethod::None);
  ASSERT_TRUE(huge);
  EXPECT_FALSE(decodeEmbeddedData(String("/opensb_data;bin;zstd;" + huge->utf8())));
}

TEST(PathEmbedTest, StreamingDecompressionAcceptsExact64MiBAndRejectsOneByteMore) {
  for (auto method : {EmbeddedCompressionMethod::Zlib, EmbeddedCompressionMethod::Gzip, EmbeddedCompressionMethod::Zstd}) {
    for (auto size : {MaxEmbeddedDataSize, MaxEmbeddedDataSize + 1}) {
      auto compressed = method == EmbeddedCompressionMethod::Zstd
          ? compressed_zstd_zeros(size)
          : compressed_zeros(size, method == EmbeddedCompressionMethod::Gzip);
      ASSERT_TRUE(compressed);
      auto encoded = embedAndCompressData(compressed->span(), EmbeddedCompressionMethod::None);
      ASSERT_TRUE(encoded);
      std::string_view token = method == EmbeddedCompressionMethod::Zstd ? "zstd;"
          : method == EmbeddedCompressionMethod::Gzip ? "gzip;" : "zlib;";
      auto result = decodeEmbeddedData(String("/opensb_data;bin;" + std::string(token) + encoded->utf8()));
      if (size == MaxEmbeddedDataSize) {
        ASSERT_TRUE(result) << result.error().what();
        EXPECT_EQ(result->bytes.size(), size);
        EXPECT_LE(result->bytes.capacity(), MaxEmbeddedDataSize);
        EXPECT_TRUE(std::ranges::all_of(result->bytes, [](char byte) { return byte == '\0'; }));
      } else {
        ASSERT_FALSE(result);
        EXPECT_NE(std::string(result.error().what()).find("size limit"), std::string::npos);
      }
    }
  }
}

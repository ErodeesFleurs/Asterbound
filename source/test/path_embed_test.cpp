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

std::expected<String, StarException> encoded_payload(std::span<char const> bytes, EmbeddedCompressionMethod method) {
  std::string payload;
  auto appended = append_embedded_data(payload, bytes, method);
  if (!appended) return std::unexpected(std::move(appended.error()));
  return String(std::move(payload));
}

std::expected<Json, StarException> decode_json(std::span<char const> bytes) {
  std::string path = "/opensb_data;json;";
  auto appended = append_embedded_data(path, bytes, EmbeddedCompressionMethod::None);
  if (!appended) return std::unexpected(std::move(appended.error()));
  auto decoded = decode_embedded_data(String(std::move(path)));
  if (!decoded) return std::unexpected(std::move(decoded.error()));
  return embedded_data_to_json(*decoded);
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
  EXPECT_EQ(embedded_compression_method("nOnE").value(), EmbeddedCompressionMethod::None);
  EXPECT_EQ(embedded_compression_method("GZIP").value(), EmbeddedCompressionMethod::Gzip);
  EXPECT_EQ(embedded_compression_method("ZlIb").value(), EmbeddedCompressionMethod::Zlib);
  EXPECT_EQ(embedded_compression_method("zStD").value(), EmbeddedCompressionMethod::Zstd);
  EXPECT_FALSE(embedded_compression_method(""));
  EXPECT_FALSE(embedded_compression_method("zlib "));
  EXPECT_FALSE(embedded_compression_method("brotli"));
}

TEST(PathEmbedTest, GeneratableCompressionModesRoundTripEveryByteAndEmptyInput) {
  ByteArray bytes(256, '\0');
  for (std::size_t i = 0; i < bytes.size(); ++i) bytes[i] = static_cast<char>(i);
  for (auto method : {EmbeddedCompressionMethod::None, EmbeddedCompressionMethod::Zlib, EmbeddedCompressionMethod::Zstd}) {
    for (auto input : std::array<std::span<char const>, 2>{bytes.span(), {}}) {
      auto encoded = encoded_payload(input, method);
      ASSERT_TRUE(encoded) << encoded.error().what();
      EXPECT_EQ(encoded->utf8().find('/'), std::string::npos);
      auto decoded = decode_embedded_data(String("/opensb_data;bin;" + encoded->utf8()));
      ASSERT_TRUE(decoded) << decoded.error().what();
      EXPECT_EQ(decoded->type, EmbeddedAssetType::Binary);
      EXPECT_EQ(decoded->bytes, ByteArray(input));
    }
  }
  EXPECT_FALSE(encoded_payload(bytes.span(), EmbeddedCompressionMethod::Gzip));
  auto explicit_none = decode_embedded_data("/opensb_data;bin;NoNe;AA==");
  ASSERT_TRUE(explicit_none);
  ASSERT_EQ(explicit_none->bytes.size(), 1u);
  EXPECT_EQ(explicit_none->bytes[0], '\0');
}

TEST(PathEmbedTest, AppendingPayloadPreservesPrefixAndCanonicalPadding) {
  constexpr std::array bytes{static_cast<char>(0xfb), static_cast<char>(0xff), static_cast<char>(0xff)};
  constexpr std::array<std::string_view, 4> expected{"", "-w==", "-_8=", "-___"};
  for (std::size_t size = 0; size <= bytes.size(); ++size) {
    std::string path("/opensb_data;bin;");
    ASSERT_TRUE(append_embedded_data(path, std::span(bytes).first(size), EmbeddedCompressionMethod::None));
    EXPECT_EQ(path, std::string("/opensb_data;bin;").append(expected[size]));
    auto unchanged = path;
    EXPECT_FALSE(append_embedded_data(path, bytes, EmbeddedCompressionMethod::Gzip));
    EXPECT_EQ(path, unchanged);
  }
}

TEST(PathEmbedTest, IndependentGzipFixtureDecodesAndDetectsDamage) {
  // Generated by gzip -n, not by the embedded codec (which cannot emit gzip).
  auto decoded = decode_embedded_data("/opensb_data;bin;gzip;H4sIAAAAAAAAA0vNTUpNSUlNUUivyixgSMusKCktSuUCAGSMmgAWAAAA");
  ASSERT_TRUE(decoded) << decoded.error().what();
  EXPECT_EQ(decoded->bytes, ByteArray::from_string(std::string_view("embedded gzip\0fixture\n", 22)));
  EXPECT_FALSE(decode_embedded_data("/opensb_data;bin;gzip;H4sIAAAAAAAAA0vNTUpNSUlNUUivyixgSMusKCktSuUCAGSMmgAWAAA="));
  EXPECT_FALSE(decode_embedded_data("/opensb_data;bin;gzip;H4sIAAAAAAAAA0vNTUpNSUlNUUivyixgSMusKCktSuUCAGSMmgAXAAAA"));
  EXPECT_FALSE(decode_embedded_data("/opensb_data;bin;gzip;"));
}

TEST(PathEmbedTest, SpecialTextRoundTripsIncludingNulAndUtf8) {
  std::string text = "percent% colon: question? semicolon; slash/\n雪";
  text += '\0';
  text += "after NUL";
  std::string path = "/opensb_data;text;";
  ASSERT_TRUE(append_escaped_text(path, text));
  EXPECT_TRUE(path.starts_with("/opensb_data;text;percent%%"));
  EXPECT_NE(path.find("%c"), std::string::npos);
  EXPECT_NE(path.find("%q"), std::string::npos);
  EXPECT_NE(path.find("%s"), std::string::npos);
  EXPECT_NE(path.find("%b"), std::string::npos);
  auto decoded = decode_embedded_data(String(std::move(path)));
  ASSERT_TRUE(decoded);
  EXPECT_EQ(decoded->type, EmbeddedAssetType::Text);
  EXPECT_EQ(decoded->bytes, ByteArray::from_string(text));
  EXPECT_TRUE(decode_embedded_data("/opensb_data;text;"));
  for (std::string_view bad : {"%", "%x", "abc%", "%C", "unescaped:colon", "unescaped?question", "unescaped/slash", "extra;field"})
    EXPECT_FALSE(decode_embedded_data(String("/opensb_data;text;" + std::string(bad)))) << bad;
}

TEST(PathEmbedTest, RejectsInvalidBase64AndExtraFieldsWithoutDroppingTokens) {
  for (std::string_view bad : {"A", "A===", "AA=", "AA===", "AA==A", "AB==", "AAB=", "A A=", "AA/+=", "AA\n", "AA==;", "AA==;extra"})
    EXPECT_FALSE(decode_embedded_data(String("/opensb_data;bin;" + std::string(bad)))) << bad;
  for (std::string_view good : {"AA", "AAA", "AAAA", "AA==", "AAA=", "", "-_8="})
    EXPECT_TRUE(decode_embedded_data(String("/opensb_data;bin;" + std::string(good)))) << good;
  for (std::string_view bad : {"", "/opensb_data", "/opensb_data;", "/opensb_data;bin", "/OPENSB_data;bin;AA==", "/opensb_data;unknown;AA==", "/opensb_data;bin;brotli;AA=="})
    EXPECT_FALSE(decode_embedded_data(String(bad))) << bad;
}

TEST(PathEmbedTest, LayoutErrorsPrecedePayloadDecoding) {
  struct Case { std::string_view body; std::string_view diagnostic; };
  for (auto const& test : {
      Case{"text;%;extra", "extra payload fields"},
      Case{"bin;!;extra", "extra payload fields"},
      Case{"json;zlib;AA==;extra", "extra payload fields"},
      Case{"image;zstd;AA;framespath;", "framespath requires"},
      Case{"image;!;frames;json;AA;extra", "extra embedded frames"},
      Case{"image;!;frames;image;AA", "cannot be nested"},
      Case{"image;!;unknown", "unsupported image frame specification"}}) {
    std::string path = "/opensb_data;";
    path.append(test.body);
    String input(std::move(path));
    for (auto mode : {DecodeMode::Asset, DecodeMode::RawBytes}) {
      auto result = decode_embedded_data(input, mode);
      ASSERT_FALSE(result) << test.body;
      EXPECT_TRUE(std::string_view(result.error().what()).contains(test.diagnostic)) << test.body;
    }
  }
  auto asset = decode_embedded_data("/opensb_data;image;!");
  ASSERT_FALSE(asset);
  EXPECT_TRUE(std::string_view(asset.error().what()).contains("requires frames"));
  auto raw = decode_embedded_data("/opensb_data;image;!", DecodeMode::RawBytes);
  ASSERT_FALSE(raw);
  EXPECT_TRUE(std::string_view(raw.error().what()).contains("Base64URL"));

  // A structurally valid frames payload is the JSON consumer's responsibility.
  auto image = decode_embedded_data("/opensb_data;image;AA;frames;json;AA!!");
  ASSERT_TRUE(image);
  ASSERT_TRUE(std::holds_alternative<EmbeddedFramesPath>(image->frame_source));
  EXPECT_FALSE(decode_embedded_data(std::get<EmbeddedFramesPath>(image->frame_source).path));
}

TEST(PathEmbedTest, TerminalCompressionNameIsPayloadNotToken) {
  // "gzip" is itself four Base64URL digits.
  auto result = decode_embedded_data("/opensb_data;bin;gzip");
  ASSERT_TRUE(result);
  EXPECT_EQ(result->bytes, ByteArray::from_string(std::string_view("\x83\x38\xa9", 3)));
}

TEST(PathEmbedTest, RawSizeLimitPrecedesCharacterDecoding) {
  constexpr auto decoded_size = max_embedded_data_size + 1;
  constexpr auto digits = (decoded_size * 4 + 2) / 3;
  for (std::string_view prefix : {"/opensb_data;bin;none;", "/opensb_data;bin;zlib;"}) {
    std::string path(prefix);
    path.resize(prefix.size() + digits, 'A');
    path[prefix.size()] = '!';
    auto result = decode_embedded_data(String(std::move(path)));
    ASSERT_FALSE(result);
    // Compressed representations retain their larger independent input bound.
    auto diagnostic = prefix.ends_with("none;") ? std::string_view("size limit") : std::string_view("invalid Base64URL character");
    EXPECT_TRUE(std::string_view(result.error().what()).contains(diagnostic)) << prefix;
  }
}

TEST(PathEmbedTest, BinaryJsonRetainsDataStreamWireForScalarsArraysAndObjects) {
  for (auto const& value : std::array<Json, 10>{Json(), Json(false), Json(true), Json(-1234567),
      Json(std::numeric_limits<std::int64_t>::min()), Json(std::numeric_limits<std::int64_t>::max()),
      Json(0.125), Json(std::string("nul\0text", 8)), Json(JsonArray{1, "two", JsonObject{{"ok", true}}}),
      Json(JsonObject{{"array", JsonArray{false, Json()}}, {"value", 9}})}) {
    auto wire = DataStreamBuffer::serialize(value);
    ASSERT_TRUE(wire);
    auto decoded = decode_json(wire->span());
    ASSERT_TRUE(decoded) << decoded.error().what();
    EXPECT_EQ(*decoded, value);
  }
  for (char tag : {char{0}, char{1}}) {
    auto decoded = decode_json(std::array{tag});
    ASSERT_TRUE(decoded);
    EXPECT_EQ(*decoded, Json());
  }
}

TEST(PathEmbedTest, BinaryJsonRejectsTruncationCountsOverflowAndTrailingBytes) {
  using std::literals::operator""sv;
  for (auto wire : {""sv, "\xff"sv, "\x02\0"sv, "\x03"sv, "\x04\x80"sv, "\x05\x7f"sv,
      "\x05\x02\x61"sv, "\x06\x7f"sv, "\x07\x01"sv, "\x07\x01\x02\x61"sv, "\x01\x01"sv,
      "\x04\x82\x80\x80\x80\x80\x80\x80\x80\x80\x00"sv,
      "\x06\xff\xff\xff\xff\xff\xff\xff\xff\xff\x7f"sv,
      "\x05\xff\xff\xff\xff\xff\xff\xff\xff\xff\x7f"sv}) {
    EXPECT_FALSE(decode_json(wire));
  }
}

TEST(PathEmbedTest, TextAndBinaryJsonEnforceDepth256WithoutChangingOrdinaryJson) {
  for (auto depth : {256u, 257u}) {
    std::string wire;
    for (unsigned i = 0; i < depth; ++i) wire.append({char{6}, char{1}});
    wire += char{1};
    auto binary = decode_json(wire);
    EXPECT_EQ(static_cast<bool>(binary), depth == 256);
    std::string text(depth, '[');
    text += '0';
    text.append(depth, ']');
    EmbeddedData data{EmbeddedAssetType::Binary, ByteArray::from_string(text), {}};
    EXPECT_EQ(static_cast<bool>(embedded_data_to_json(data)), depth == 256);
  }
  EmbeddedData comments{EmbeddedAssetType::Text,
      ByteArray::from_string("/*[[[*/{\"braces\":\"[\\\"{\",\"value\":true}//[[[\r[[[\n"), {}};
  EXPECT_TRUE(embedded_data_to_json(comments));
  for (std::string_view text : {"null false", "{}x", "[0] trailing", "\"unfinished", "[}"}) {
    EmbeddedData data{EmbeddedAssetType::Binary, ByteArray::from_string(text), {}};
    EXPECT_FALSE(embedded_data_to_json(data)) << text;
  }
  EmbeddedData nul{EmbeddedAssetType::Text, ByteArray::from_string(std::string_view("0\0", 2)), {}};
  EXPECT_FALSE(embedded_data_to_json(nul));
  EXPECT_TRUE(Json::parse("[0]"));
}

TEST(PathEmbedTest, ImagesRequireUnambiguousFramesAndKeepCompleteFramesPath) {
  auto image = decode_embedded_data("/opensb_data;image;AA==;frames;text;{}");
  ASSERT_TRUE(image);
  EXPECT_EQ(image->type, EmbeddedAssetType::Image);
  ASSERT_TRUE(std::holds_alternative<EmbeddedFramesPath>(image->frame_source));
  auto const& embedded_path = std::get<EmbeddedFramesPath>(image->frame_source).path;
  EXPECT_EQ(embedded_path, String("/opensb_data;text;{}"));
  auto frames = decode_embedded_data(embedded_path);
  ASSERT_TRUE(frames);
  EXPECT_EQ(embedded_data_to_json(*frames).value(), Json(JsonObject{}));
  auto path = decode_embedded_data("/opensb_data;image;AA==;framespath;/images/default.frames");
  ASSERT_TRUE(path);
  ASSERT_TRUE(std::holds_alternative<ExternalFramesPath>(path->frame_source));
  EXPECT_EQ(std::get<ExternalFramesPath>(path->frame_source).path, String("/images/default.frames"));
  EXPECT_FALSE(decode_embedded_data("/opensb_data;image;AA=="));
  auto raw = decode_embedded_data("/opensb_data;image;AA==", DecodeMode::RawBytes);
  ASSERT_TRUE(raw);
  EXPECT_TRUE(std::holds_alternative<std::monostate>(raw->frame_source));
  EXPECT_TRUE(decode_embedded_data("/opensb_data;bin;AA=="));
  EXPECT_FALSE(embedded_data_to_json(*image));

  auto frame_wire = DataStreamBuffer::serialize(Json(JsonObject{{"frameGrid", JsonObject{{"size", JsonArray{1, 1}}}}}));
  ASSERT_TRUE(frame_wire);
  for (auto method : {EmbeddedCompressionMethod::None, EmbeddedCompressionMethod::Zlib, EmbeddedCompressionMethod::Zstd}) {
    auto payload = encoded_payload(std::span<char const>("png", 3), method);
    auto frame_payload = encoded_payload(frame_wire->span(), method);
    ASSERT_TRUE(payload);
    ASSERT_TRUE(frame_payload);
    auto decoded = decode_embedded_data(String("/opensb_data;image;" + payload->utf8() + ";frames;json;" + frame_payload->utf8()));
    ASSERT_TRUE(decoded) << decoded.error().what();
    ASSERT_TRUE(std::holds_alternative<EmbeddedFramesPath>(decoded->frame_source));
    auto decoded_frames = decode_embedded_data(std::get<EmbeddedFramesPath>(decoded->frame_source).path);
    ASSERT_TRUE(decoded_frames);
    EXPECT_TRUE(embedded_data_to_json(*decoded_frames));
  }
  for (auto suffix : {"frames", "framespath", "framespath;", "framespath;/a;extra", "unknown;/a",
      "frames;image;AA==;frames;text;{}", "frames;text;{};extra", "frames;json;AA==;extra", "frames;text"}) {
    auto invalid_path = String("/opensb_data;image;AA==;" + std::string(suffix));
    EXPECT_FALSE(decode_embedded_data(invalid_path)) << suffix;
    EXPECT_FALSE(decode_embedded_data(invalid_path, DecodeMode::RawBytes)) << suffix;
  }
}

TEST(PathEmbedTest, CompressedStreamsRejectCorruptionTruncationAndConcatenation) {
  auto input = ByteArray::from_string("compressed payload");
  for (auto method : {EmbeddedCompressionMethod::Zlib, EmbeddedCompressionMethod::Zstd}) {
    auto encoded = encoded_payload(input.span(), method);
    ASSERT_TRUE(encoded);
    auto separator = encoded->utf8().find(';');
    auto token = encoded->utf8().substr(0, separator + 1);
    auto compressed = decode_embedded_data(String("/opensb_data;bin;" + encoded->utf8().substr(separator + 1)));
    ASSERT_TRUE(compressed);
    auto bytes = compressed->bytes;
    bytes.remove_suffix(1);
    EXPECT_FALSE(decode_embedded_data(String("/opensb_data;bin;" + token + encoded_payload(bytes.span(), EmbeddedCompressionMethod::None).value().utf8())));
    bytes = compressed->bytes;
    bytes[0] ^= 0x7f;
    EXPECT_FALSE(decode_embedded_data(String("/opensb_data;bin;" + token + encoded_payload(bytes.span(), EmbeddedCompressionMethod::None).value().utf8())));
    bytes = compressed->bytes;
    bytes.append(compressed->bytes);
    EXPECT_FALSE(decode_embedded_data(String("/opensb_data;bin;" + token + encoded_payload(bytes.span(), EmbeddedCompressionMethod::None).value().utf8())));
    bytes = compressed->bytes;
    bytes.push_back('\0');
    EXPECT_FALSE(decode_embedded_data(String("/opensb_data;bin;" + token + encoded_payload(bytes.span(), EmbeddedCompressionMethod::None).value().utf8())));
    EXPECT_FALSE(decode_embedded_data(String("/opensb_data;bin;" + token)));
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
    auto encoded = encoded_payload(bytes.span(), EmbeddedCompressionMethod::None);
    ASSERT_TRUE(encoded);
    auto decoded = decode_embedded_data(String("/opensb_data;bin;gzip;" + encoded->utf8()));
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
  auto encoded = encoded_payload(compressed.span(), EmbeddedCompressionMethod::None);
  ASSERT_TRUE(encoded);
  auto decoded = decode_embedded_data(String("/opensb_data;bin;zstd;" + encoded->utf8()));
  ASSERT_TRUE(decoded);
  EXPECT_EQ(decoded->bytes, ByteArray::from_string("stream"));
  // Single-segment frame header declaring 64MiB + 1, with no body. Rejection
  // must not reserve the attacker-controlled declared size.
  std::array<char, 9> header{char{0x28}, static_cast<char>(0xb5), char{0x2f},
      static_cast<char>(0xfd), static_cast<char>(0xa0), 1, 0, 0, 4};
  auto huge = encoded_payload(header, EmbeddedCompressionMethod::None);
  ASSERT_TRUE(huge);
  EXPECT_FALSE(decode_embedded_data(String("/opensb_data;bin;zstd;" + huge->utf8())));
}

TEST(PathEmbedTest, StreamingDecompressionAcceptsExact64MiBAndRejectsOneByteMore) {
  for (auto method : {EmbeddedCompressionMethod::Zlib, EmbeddedCompressionMethod::Gzip, EmbeddedCompressionMethod::Zstd}) {
    for (auto size : {max_embedded_data_size, max_embedded_data_size + 1}) {
      auto compressed = method == EmbeddedCompressionMethod::Zstd
          ? compressed_zstd_zeros(size)
          : compressed_zeros(size, method == EmbeddedCompressionMethod::Gzip);
      ASSERT_TRUE(compressed);
      auto encoded = encoded_payload(compressed->span(), EmbeddedCompressionMethod::None);
      ASSERT_TRUE(encoded);
      std::string_view token = method == EmbeddedCompressionMethod::Zstd ? "zstd;"
          : method == EmbeddedCompressionMethod::Gzip ? "gzip;" : "zlib;";
      auto result = decode_embedded_data(String("/opensb_data;bin;" + std::string(token) + encoded->utf8()));
      if (size == max_embedded_data_size) {
        ASSERT_TRUE(result) << result.error().what();
        EXPECT_EQ(result->bytes.size(), size);
        EXPECT_LE(result->bytes.capacity(), max_embedded_data_size);
        EXPECT_TRUE(std::ranges::all_of(result->bytes, [](char byte) { return byte == '\0'; }));
      } else {
        ASSERT_FALSE(result);
        EXPECT_NE(std::string(result.error().what()).find("size limit"), std::string::npos);
      }
    }
  }
}

#include <zlib-ng.h>
import std;
import star.string;
import star.byte_array;
import star.encode;
import star.format;
import star.compression;

typedef struct ZSTD_CCtx_s ZSTD_CCtx;
typedef struct ZSTD_DCtx_s ZSTD_DCtx;
typedef ZSTD_DCtx ZSTD_DStream;
typedef ZSTD_CCtx ZSTD_CStream;
import star.zstd_compression;

#include "gtest/gtest.h"

import star.sha256;

using namespace Star;

TEST(EncodeTest, Base64) {
  std::string_view data =
      "Man is distinguished, not only by his reason, but by this singular passion from other animals, which is a lust "
      "of the mind, that by a perseverance of delight in the continued and indefatigable generation of knowledge, "
      "exceeds the short vehemence of any carnal pleasure.";
  ByteArray testSource{std::span(data)};
  String testEncoded =
      "TWFuIGlzIGRpc3Rpbmd1aXNoZWQsIG5vdCBvbmx5IGJ5IGhpcyByZWFzb24sIGJ1dCBieSB0aGlzIHNpbmd1bGFyIHBhc3Npb24gZnJvbSBvdGhl"
      "ciBhbmltYWxzLCB3aGljaCBpcyBhIGx1c3Qgb2YgdGhlIG1pbmQsIHRoYXQgYnkgYSBwZXJzZXZlcmFuY2Ugb2YgZGVsaWdodCBpbiB0aGUgY29u"
      "dGludWVkIGFuZCBpbmRlZmF0aWdhYmxlIGdlbmVyYXRpb24gb2Yga25vd2xlZGdlLCBleGNlZWRzIHRoZSBzaG9ydCB2ZWhlbWVuY2Ugb2YgYW55"
      "IGNhcm5hbCBwbGVhc3VyZS4=";

  String encoded = base64Encode(testSource);
  auto result1 = base64Decode(encoded);
  ASSERT_TRUE(result1) << result1.error().what();
  ByteArray decoded = (*result1);

  EXPECT_EQ(encoded, testEncoded);
  EXPECT_EQ(decoded, testSource);
}

TEST(EncodeTest, SHA256) {
  std::string_view data =
      "Man is distinguished, not only by his reason, but by this singular passion from other animals, which is a lust "
      "of the mind, that by a perseverance of delight in the continued and indefatigable generation of knowledge, "
      "exceeds the short vehemence of any carnal pleasure.";
  ByteArray testSource{std::span(data)};
  auto result2 = hexDecode("78fe75026c4390ceccc4e9e6a9428ba8ae5968b458e60b5eebecd682cf24bbf2");
  ASSERT_TRUE(result2) << result2.error().what();
  ByteArray testHash = (*result2);

  EXPECT_EQ(sha256(testSource), testHash);
}

TEST(EncodeTest, BorrowedSpans) {
  char output[8]{};
  EXPECT_EQ(hexEncode(std::span<char const>{}, std::span<char>{}), 0u);
  auto result3 = hexDecode(std::span<char const>{}, std::span<char>{});
  ASSERT_TRUE(result3) << result3.error().what();
  EXPECT_EQ((*result3), 0u);
  auto result4 = nibbleDecode(std::span<char const>{}, std::span<char>{});
  ASSERT_TRUE(result4) << result4.error().what();
  EXPECT_EQ((*result4), 0u);
  EXPECT_EQ(base64Encode(std::span<char const>{}, std::span<char>{}), 0u);
  auto result5 = base64Decode(std::span<char const>{}, std::span<char>{});
  ASSERT_TRUE(result5) << result5.error().what();
  EXPECT_EQ((*result5), 0u);

  EXPECT_EQ(hexEncode({"ab", 2}, {output, 3}), 2u);
  EXPECT_EQ(std::string_view(output, 2), "61");
  auto result6 = hexDecode({"g1f", 3}, output);
  ASSERT_TRUE(result6) << result6.error().what();
  EXPECT_EQ((*result6), 1u);
  EXPECT_EQ(static_cast<unsigned char>(output[0]), 1u);
  auto result7 = nibbleDecode({"gF1", 3}, {output, 2});
  ASSERT_TRUE(result7) << result7.error().what();
  EXPECT_EQ((*result7), 2u);
  EXPECT_EQ(output[0], 0);
  EXPECT_EQ(output[1], 15);

  EXPECT_EQ(base64Encode({"M", 1}, output), 4u);
  EXPECT_EQ(std::string_view(output, 4), "TQ==");
  EXPECT_EQ(base64Encode({"Man", 3}, {output, 2}), 2u);
  EXPECT_EQ(std::string_view(output, 2), "TW");
  auto result8 = base64Decode({"TWE", 3}, output);
  ASSERT_TRUE(result8) << result8.error().what();
  EXPECT_EQ((*result8), 2u);
  EXPECT_EQ(std::string_view(output, 2), "Ma");
  auto result9 = base64Decode({"TQ=ignored", 10}, output);
  ASSERT_TRUE(result9) << result9.error().what();
  EXPECT_EQ((*result9), 1u);
  EXPECT_EQ(output[0], 'M');
  auto result10 = base64Decode({"TQ!ignored", 10}, output);
  ASSERT_TRUE(result10) << result10.error().what();
  EXPECT_EQ((*result10), 1u);
  EXPECT_EQ(output[0], 'M');
  auto result11 = base64Decode({"TWFu", 4}, {output, 1});
  ASSERT_TRUE(result11) << result11.error().what();
  EXPECT_EQ((*result11), 1u);
  EXPECT_EQ(output[0], 'M');
}

TEST(EncodeTest, OwningEncodersPreserveEmptyBinaryAndPaddingBoundaries) {
  struct Case {
    std::string_view input;
    std::string_view hex;
    std::string_view base64;
  };
  Case const cases[]{{"", "", ""},
                     {"M", "4d", "TQ=="},
                     {"Ma", "4d61", "TWE="},
                     {"Man", "4d616e", "TWFu"},
                     {std::string_view("\0\x7f\x80\xff", 4), "007f80ff", "AH+A/w=="}};
  for (auto const& test : cases) {
    SCOPED_TRACE(test.hex);
    auto input = std::span<char const>(test.input.data(), test.input.size());
    EXPECT_EQ(hexEncode(input), String(test.hex));
    EXPECT_EQ(base64Encode(input), String(test.base64));
  }
}

TEST(EncodeTest, OwningHexDecoderPreservesPermissiveNibblesAndOddTails) {
  struct Case {
    String input;
    ByteArray expected;
  };
  Case const cases[]{{"", {}},
                     {"f", {}},
                     {"g1f", ByteArray({"\x01", 1})},
                     {"Fg?fZZ1", ByteArray({"\xf0\x0f\0", 3})},
                     {String(std::string_view("a\0Ff0", 5)), ByteArray({"\xa0\xff", 2})}};
  for (auto const& test : cases) {
    auto decoded = hexDecode(test.input);
    ASSERT_TRUE(decoded) << decoded.error().what();
    EXPECT_EQ(*decoded, test.expected);
  }
}

TEST(EncodeTest, OwningBase64DecoderPublishesOnlyAcceptedPartialGroups) {
  struct Case {
    String input;
    ByteArray expected;
  };
  Case const cases[]{{"", {}},
                     {"T", {}},
                     {"TQ", ByteArray({"M", 1})},
                     {"TWE", ByteArray({"Ma", 2})},
                     {"TWFu", ByteArray({"Man", 3})},
                     {"TWFuT", ByteArray({"Man", 3})},
                     {"TWFuTQ", ByteArray({"ManM", 4})},
                     {"TQ=ignored", ByteArray({"M", 1})},
                     {"TWE!ignored", ByteArray({"Ma", 2})},
                     {"!TWFu", {}},
                     {String(std::string_view("TQ\0TWFu", 7)), ByteArray({"M", 1})},
                     {"AH+A/w==", ByteArray({"\0\x7f\x80\xff", 4})}};
  for (auto const& test : cases) {
    auto decoded = base64Decode(test.input);
    ASSERT_TRUE(decoded) << decoded.error().what();
    EXPECT_EQ(*decoded, test.expected);
  }
}

namespace {

// A single final stored block keeps checksum and truncation failures independent
// of compressor heuristics, while crossing the 32768-byte output-call boundary.
ByteArray storedFrame(ByteArray const& payload, bool gzip) {
  ByteArray frame = gzip ? ByteArray({"\x1f\x8b\x08\0\0\0\0\0\0\xff", 10}) : ByteArray({"\x78\x01", 2});
  auto appendLittle = [&](std::uint32_t value, unsigned bytes) {
    for (unsigned i = 0; i < bytes; ++i)
      frame.push_back(static_cast<char>(value >> (8 * i)));
  };
  frame.push_back(1);
  appendLittle(payload.size(), 2);
  appendLittle(static_cast<std::uint16_t>(~payload.size()), 2);
  frame.append(payload);
  if (gzip) {
    auto crc = zng_crc32(0, reinterpret_cast<std::uint8_t const*>(payload.data()), payload.size());
    appendLittle(crc, 4);
    appendLittle(payload.size(), 4);
  } else {
    auto adler = zng_adler32(1, reinterpret_cast<std::uint8_t const*>(payload.data()), payload.size());
    for (int shift = 24; shift >= 0; shift -= 8)
      frame.push_back(static_cast<char>(adler >> shift));
  }
  return frame;
}

std::expected<ByteArray, StarException> stagedDeflate(ByteArray const& input, CompressionLevel level) {
  zng_stream stream{};
  auto status = zng_deflateInit(&stream, level);
  if (status != Z_OK)
    return std::unexpected(IOException("Cannot initialize reference deflate"));
  auto cleanup = [](zng_stream* stream) { zng_deflateEnd(stream); };
  std::unique_ptr<zng_stream, decltype(cleanup)> owner(&stream, cleanup);
  stream.next_in = reinterpret_cast<std::uint8_t const*>(input.data());
  stream.avail_in = input.size();
  std::array<std::uint8_t, 32768> staging;
  ByteArray result;
  do {
    stream.next_out = staging.data();
    stream.avail_out = staging.size();
    status = zng_deflate(&stream, Z_FINISH);
    if (status != Z_OK && status != Z_STREAM_END)
      return std::unexpected(IOException("Reference deflate failed"));
    result.append({reinterpret_cast<char const*>(staging.data()), staging.size() - stream.avail_out});
  } while (status != Z_STREAM_END);
  return result;
}

}

TEST(CompressionTest, DirectOutputPreservesStagedDeflateBytesAcrossChunkBoundaries) {
  for (auto size : {32767u, 32768u, 32769u, 65537u}) {
    ByteArray input;
    input.resize(size);
    std::uint32_t state = 0x12345678;
    for (char& byte : input) {
      state = state * 1664525u + 1013904223u;
      byte = static_cast<char>(state >> 24);
    }
    for (auto level : {0, 2, 5, 9}) {
      SCOPED_TRACE(size);
      SCOPED_TRACE(level);
      auto expected = stagedDeflate(input, level);
      ASSERT_TRUE(expected) << expected.error().what();
      auto compressed = compressData(input, level);
      ASSERT_TRUE(compressed) << compressed.error().what();
      EXPECT_EQ(*compressed, *expected);
      auto decoded = uncompressData(*compressed);
      ASSERT_TRUE(decoded) << decoded.error().what();
      EXPECT_EQ(*decoded, input);
    }
  }
}

TEST(CompressionTest, InflateFailuresPublishOnlyPreviouslyAcceptedCodecCalls) {
  ByteArray payload(32768 + 37, 'p');
  for (bool gzip : {false, true}) {
    SCOPED_TRACE(gzip);
    auto frame = storedFrame(payload, gzip);
    auto decode = [&](ByteArray const& input, ByteArray& output, std::size_t limit = 0) {
      return gzip ? uncompressDataGzip(input, output, limit) : uncompressData(input, output, limit);
    };
    ByteArray output = ByteArray::from_string("old output");
    ASSERT_TRUE(decode(frame, output));
    EXPECT_EQ(output, payload);
    for (auto limit : {1u, 32767u, 32768u, 32769u}) {
      auto result = decode(frame, output, limit);
      ASSERT_FALSE(result);
      EXPECT_TRUE(result.error().is<IOException>());
      EXPECT_EQ(output, ByteArray(limit < 32768 ? 0 : 32768, 'p'));
    }
    ASSERT_TRUE(decode(frame, output, payload.size()));
    EXPECT_EQ(output, payload);

    auto corrupt = frame;
    auto checksumOffset = corrupt.size() - (gzip ? 8 : 4);
    corrupt[checksumOffset] ^= 1;
    auto checksumFailure = decode(corrupt, output);
    ASSERT_FALSE(checksumFailure);
    EXPECT_TRUE(checksumFailure.error().is<IOException>());
    EXPECT_EQ(output, ByteArray(32768, 'p'));

    auto truncated = frame;
    truncated.resize(checksumOffset);
    auto truncatedFailure = decode(truncated, output);
    ASSERT_FALSE(truncatedFailure);
    EXPECT_TRUE(truncatedFailure.error().is<IOException>());
    EXPECT_EQ(output, payload);

    auto malformed = ByteArray::from_string("not compressed");
    ASSERT_FALSE(decode(malformed, output));
    EXPECT_TRUE(output.empty());
    ASSERT_TRUE(decode(ByteArray(), output));
    EXPECT_TRUE(output.empty());
  }
}

TEST(CompressionTest, InitializationFailureAndEmptyInputPreserveClearOrdering) {
  ByteArray input = ByteArray::from_string("payload");
  ByteArray output = ByteArray::from_string("old output");
  auto failed = compressData(input, output, 100);
  ASSERT_FALSE(failed);
  EXPECT_TRUE(failed.error().is<IOException>());
  EXPECT_TRUE(output.empty());
  output = input;
  EXPECT_TRUE(compressData(ByteArray(), output, 100));
  EXPECT_TRUE(output.empty());
}

TEST(CompressionTest, ZstdOneShotAndStreamErrorsPreserveExistingOutput) {
  ByteArray input(300000, 'z');
  auto compressed = ZstdCompression::compress(input);
  ASSERT_TRUE(compressed) << compressed.error().what();
  auto decompressed = ZstdCompression::decompress(*compressed);
  ASSERT_TRUE(decompressed) << decompressed.error().what();
  EXPECT_EQ(*decompressed, input);
  auto malformed = ByteArray::from_string("not a ZSTD frame");
  ByteArray output = ByteArray::from_string("existing");
  auto original = output;
  EXPECT_FALSE(ZstdCompression::decompress(malformed, output));
  EXPECT_EQ(output, original);
  auto truncated = *compressed;
  truncated.resize(truncated.size() - 1);
  EXPECT_FALSE(ZstdCompression::decompress(truncated, output));
  EXPECT_EQ(output, original);

  auto decoder = DecompressionStream::create();
  ASSERT_TRUE(decoder) << decoder.error().what();
  auto followedByInvalidFrame = *compressed;
  followedByInvalidFrame.append(malformed);
  EXPECT_FALSE((*decoder)->decompress(followedByInvalidFrame, output));
  EXPECT_EQ(output, original);
}

TEST(CompressionTest, OwningSmallResultsCompactScratchSlackButReusableOutputsKeepCapacity) {
  auto input = ByteArray::from_string("seventeen bytes!!");
  auto compressed = compressData(input);
  ASSERT_TRUE(compressed) << compressed.error().what();
  auto retained = std::max(input.size(), compressed->size());
  EXPECT_LE(compressed->capacity() - retained, retained);
  auto decoded = uncompressData(*compressed);
  ASSERT_TRUE(decoded) << decoded.error().what();
  EXPECT_EQ(*decoded, input);
  EXPECT_LE(decoded->capacity() - decoded->size(), decoded->size());

  auto gzip = storedFrame(input, true);
  auto gzipDecoded = uncompressDataGzip(gzip);
  ASSERT_TRUE(gzipDecoded) << gzipDecoded.error().what();
  EXPECT_EQ(*gzipDecoded, input);
  EXPECT_LE(gzipDecoded->capacity() - gzipDecoded->size(), gzipDecoded->size());

  ByteArray reusable;
  ASSERT_TRUE(compressData(input, reusable));
  EXPECT_GE(reusable.capacity(), 32768u);
  auto reusableCapacity = reusable.capacity();
  ASSERT_TRUE(uncompressData(*compressed, reusable));
  EXPECT_EQ(reusable, input);
  EXPECT_GE(reusable.capacity(), reusableCapacity);
  ASSERT_TRUE(uncompressDataGzip(gzip, reusable));
  EXPECT_EQ(reusable, input);
  EXPECT_GE(reusable.capacity(), reusableCapacity);
}

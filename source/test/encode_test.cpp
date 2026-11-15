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

namespace {

void expect_decoding_capacities(std::string_view input, std::string_view expected, bool hex) {
  auto owned = hex ? hex_decode(input) : base64_decode(input);
  EXPECT_TRUE(std::ranges::equal(owned.span(), expected));
  for (std::size_t capacity = 0; capacity <= expected.size() + 2; ++capacity) {
    SCOPED_TRACE(capacity);
    std::string output(expected.size() + 4, '\x5a');
    auto destination = std::span(output).first(capacity);
    auto written = hex ? hex_decode(std::span(input), destination) : base64_decode(std::span(input), destination);
    EXPECT_EQ(written, std::min(capacity, expected.size()));
    EXPECT_EQ(std::string_view(output).substr(0, written), expected.substr(0, written));
    EXPECT_TRUE(std::ranges::all_of(std::span(output).subspan(written), [](char byte) { return byte == '\x5a'; }));

    // Exactly same-start overlap is supported, including truncated destinations.
    std::string inplace(input);
    inplace.resize(std::max(input.size(), capacity) + 2, '\x5a');
    auto before = inplace;
    auto source = std::span<char const>(inplace).first(input.size());
    auto target = std::span(inplace).first(capacity);
    auto inplaceWritten = hex ? hex_decode(source, target) : base64_decode(source, target);
    EXPECT_EQ(inplaceWritten, written);
    EXPECT_EQ(std::string_view(inplace).substr(0, inplaceWritten), expected.substr(0, inplaceWritten));
    EXPECT_EQ(std::string_view(inplace).substr(inplaceWritten), std::string_view(before).substr(inplaceWritten));
  }
}

void expect_encoding_capacities(std::string_view input, std::string_view hex, std::string_view base64) {
  auto source = std::span(input);
  EXPECT_EQ(hex_encode(source), String(hex));
  EXPECT_EQ(base64_encode(source), String(base64));
  for (bool useHex : {true, false}) {
    auto expected = useHex ? hex : base64;
    for (std::size_t capacity = 0; capacity <= expected.size() + 2; ++capacity) {
      SCOPED_TRACE(useHex);
      SCOPED_TRACE(capacity);
      std::string output(expected.size() + 4, '\x5a');
      auto destination = std::span(output).first(capacity);
      auto written = useHex ? hex_encode(source, destination) : base64_encode(source, destination);
      EXPECT_EQ(written, useHex ? std::min(capacity / 2, input.size()) * 2 : std::min(capacity, expected.size()));
      EXPECT_EQ(std::string_view(output).substr(0, written), expected.substr(0, written));
      EXPECT_TRUE(std::ranges::all_of(std::span(output).subspan(written), [](char byte) { return byte == '\x5a'; }));
    }
  }
  expect_decoding_capacities(hex, input, true);
  expect_decoding_capacities(base64, input, false);
}

}

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

  String encoded = base64_encode(testSource);
  ByteArray decoded = base64_decode(encoded.utf8());

  EXPECT_EQ(encoded, testEncoded);
  EXPECT_EQ(decoded, testSource);
}

TEST(EncodeTest, SHA256) {
  std::string_view data =
      "Man is distinguished, not only by his reason, but by this singular passion from other animals, which is a lust "
      "of the mind, that by a perseverance of delight in the continued and indefatigable generation of knowledge, "
      "exceeds the short vehemence of any carnal pleasure.";
  ByteArray testSource{std::span(data)};
  ByteArray testHash = hex_decode("78fe75026c4390ceccc4e9e6a9428ba8ae5968b458e60b5eebecd682cf24bbf2");

  EXPECT_EQ(sha256(testSource), testHash);
}

TEST(EncodeTest, EmptySpans) {
  EXPECT_EQ(hex_encode(std::span<char const>{}, std::span<char>{}), 0u);
  EXPECT_EQ(hex_decode(std::span<char const>{}, std::span<char>{}), 0u);
  EXPECT_EQ(base64_encode(std::span<char const>{}, std::span<char>{}), 0u);
  EXPECT_EQ(base64_decode(std::span<char const>{}, std::span<char>{}), 0u);
}

TEST(EncodeTest, FixedRfcVectorsAndEveryOutputCapacity) {
  struct Case {
    std::string_view input;
    std::string_view hex;
    std::string_view base64;
  };
  Case const cases[]{{"", "", ""},
                     {"f", "66", "Zg=="},
                     {"fo", "666f", "Zm8="},
                     {"foo", "666f6f", "Zm9v"},
                     {"foob", "666f6f62", "Zm9vYg=="},
                     {"fooba", "666f6f6261", "Zm9vYmE="},
                     {"foobar", "666f6f626172", "Zm9vYmFy"},
                     {"M", "4d", "TQ=="},
                     {"Ma", "4d61", "TWE="},
                     {"Man", "4d616e", "TWFu"},
                     {std::string_view("\0\x7f\x80\xff", 4), "007f80ff", "AH+A/w=="}};
  for (auto const& test : cases) {
    SCOPED_TRACE(test.hex);
    expect_encoding_capacities(test.input, test.hex, test.base64);
  }
}

TEST(EncodeTest, AllByteValuesAndEveryOutputCapacity) {
  std::array<char, 256> input;
  for (std::size_t i = 0; i < input.size(); ++i)
    input[i] = static_cast<char>(i);
  std::string_view const hex =
      "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f"
      "202122232425262728292a2b2c2d2e2f303132333435363738393a3b3c3d3e3f"
      "404142434445464748494a4b4c4d4e4f505152535455565758595a5b5c5d5e5f"
      "606162636465666768696a6b6c6d6e6f707172737475767778797a7b7c7d7e7f"
      "808182838485868788898a8b8c8d8e8f909192939495969798999a9b9c9d9e9f"
      "a0a1a2a3a4a5a6a7a8a9aaabacadaeafb0b1b2b3b4b5b6b7b8b9babbbcbdbebf"
      "c0c1c2c3c4c5c6c7c8c9cacbcccdcecfd0d1d2d3d4d5d6d7d8d9dadbdcdddedf"
      "e0e1e2e3e4e5e6e7e8e9eaebecedeeeff0f1f2f3f4f5f6f7f8f9fafbfcfdfeff";
  std::string_view const base64 =
      "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8gISIjJCUmJygpKissLS4v"
      "MDEyMzQ1Njc4OTo7PD0+P0BBQkNERUZHSElKS0xNTk9QUVJTVFVWV1hZWltcXV5f"
      "YGFiY2RlZmdoaWprbG1ub3BxcnN0dXZ3eHl6e3x9fn+AgYKDhIWGh4iJiouMjY6P"
      "kJGSk5SVlpeYmZqbnJ2en6ChoqOkpaanqKmqq6ytrq+wsbKztLW2t7i5uru8vb6/"
      "wMHCw8TFxsfIycrLzM3Oz9DR0tPU1dbX2Nna29zd3t/g4eLj5OXm5+jp6uvs7e7v"
      "8PHy8/T19vf4+fr7/P3+/w==";
  expect_encoding_capacities(std::string_view(input), hex, base64);
}

TEST(EncodeTest, HexPermissiveNibblesAndOddTails) {
  struct Case {
    std::string_view input;
    std::string_view expected;
  };
  Case const cases[]{{"", ""},
                     {"f", ""},
                     {"g1f", std::string_view("\x01", 1)},
                     {"Fg?fZZ1", std::string_view("\xf0\x0f\0", 3)},
                     {std::string_view("a\0Ff0", 5), std::string_view("\xa0\xff", 2)}};
  for (auto const& test : cases)
    expect_decoding_capacities(test.input, test.expected, true);

  std::string_view const digits = "0123456789abcdef0123456789ABCDEF";
  for (unsigned byte = 0; byte < 256; ++byte) {
    SCOPED_TRACE(byte);
    auto position = digits.find(static_cast<char>(byte));
    auto nibble = position == std::string_view::npos ? 0 : position % 16;
    std::array<char, 4> input{static_cast<char>(byte), '1', '1', static_cast<char>(byte)};
    std::array<char, 2> expected{static_cast<char>((nibble << 4) | 1), static_cast<char>(0x10 | nibble)};
    expect_decoding_capacities(std::string_view(input), std::string_view(expected), true);
  }
}

TEST(EncodeTest, Base64PublishesOnlyAcceptedPartialGroups) {
  struct Case {
    std::string_view input;
    std::string_view expected;
  };
  Case const cases[]{{"", ""},
                     {"T", ""},
                     {"TQ", "M"},
                     {"TWE", "Ma"},
                     {"TWFu", "Man"},
                     {"TWFuT", "Man"},
                     {"TWFuTQ", "ManM"},
                     {"TWFuTWE", "ManMa"},
                     {"TWFuTWFu", "ManMan"},
                     {"TQ=ignored", "M"},
                     {"TWE!ignored", "Ma"},
                     {"!TWFu", ""},
                     {"T=TWFu", ""},
                     {"TWFu=TQ", "Man"},
                     {"TWFu TQ", "Man"},
                     {" TWFu", ""},
                     {"TQ\\nTWFu", "M"},
                     {"TQ\nTWFu", "M"},
                     {"TQ\rTWFu", "M"},
                     {"TQ\tTWFu", "M"},
                     {"TQ-TWFu", "M"},
                     {"TQ_TWFu", "M"},
                     {std::string_view("TQ\0TWFu", 7), "M"},
                     {"AH+A/w==", std::string_view("\0\x7f\x80\xff", 4)},
                     {"Zh==", "f"},
                     {"Zm9=", "fo"}};
  for (auto const& test : cases) {
    SCOPED_TRACE(test.input);
    expect_decoding_capacities(test.input, test.expected, false);
  }
}

TEST(EncodeTest, Base64StopsAtEveryNonalphabetByte) {
  std::string_view const alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  for (unsigned byte = 0; byte < 256; ++byte) {
    if (alphabet.find(static_cast<char>(byte)) != std::string_view::npos)
      continue;
    SCOPED_TRACE(byte);
    std::string input = "TWE";
    input.push_back(static_cast<char>(byte));
    input += "TWFu";
    expect_decoding_capacities(input, "Ma", false);
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

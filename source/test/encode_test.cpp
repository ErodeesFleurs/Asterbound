import std;
import star.string;
import star.byte_array;
import star.encode;
import star.format;

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

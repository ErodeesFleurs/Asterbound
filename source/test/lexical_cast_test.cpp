import star.lexical_cast;

import std;

#include "gtest/gtest.h"

using namespace Star;

TEST(LexicalCastTest, BoolRequiresTheExactLiteral) {
  auto cast = [](std::string_view text) {
    return lexicalCast<bool>(text);
  };
  auto tryCast = [](bool& value, std::string_view text) {
    return tryLexicalCast(value, text);
  };

  auto trueResult = cast("true");
  ASSERT_TRUE(trueResult);
  EXPECT_TRUE(*trueResult);
  auto falseResult = cast("false");
  ASSERT_TRUE(falseResult);
  EXPECT_FALSE(*falseResult);

  // Prefixes and trailing bytes must not be accepted as boolean literals.
  for (std::string_view text : {"", "t", "tru", "fals", "truex"}) {
    auto result = cast(text);
    ASSERT_FALSE(result);
    EXPECT_TRUE(result.error().is<BadLexicalCast>());
  }

  bool value = true;
  EXPECT_FALSE(tryCast(value, "tru"));
  EXPECT_TRUE(tryCast(value, "false"));
  EXPECT_FALSE(value);
  EXPECT_TRUE(tryCast(value, "true"));
  EXPECT_TRUE(value);
}

namespace {

template <typename Type>
auto castNumber(std::string_view text) {
  return lexicalCast<Type>(text);
}

template <typename Type>
bool tryNumber(Type& value, std::string_view text) {
  return tryLexicalCast(value, text);
}

template <typename Type>
using FloatingBits = std::conditional_t<sizeof(Type) == sizeof(std::uint32_t), std::uint32_t, std::uint64_t>;

template <typename Type>
constexpr FloatingBits<Type> InfinityBits = sizeof(Type) == sizeof(std::uint32_t)
    ? FloatingBits<Type>(0x7f800000u) : FloatingBits<Type>(0x7ff0000000000000ULL);

template <typename Type>
FloatingBits<Type> magnitudeBits(Type const& value) {
  FloatingBits<Type> bits;
  std::memcpy(&bits, &value, sizeof(bits));
  return bits & (std::numeric_limits<FloatingBits<Type>>::max() >> 1);
}

template <typename Type>
void checkFloatingRanges() {
  for (std::string_view text : {"1e99999", "-1e99999", "1e-99999", "-1e-99999"}) {
    auto result = castNumber<Type>(text);
    ASSERT_TRUE(result);
    EXPECT_EQ(std::signbit(*result), text[0] == '-');
    if (std::string_view(text).find("e-") == std::string_view::npos)
      EXPECT_EQ(magnitudeBits(*result), InfinityBits<Type>);
    else
      EXPECT_EQ(*result, Type(0));
    Type value = Type(37);
    EXPECT_TRUE(tryNumber(value, text));
    EXPECT_EQ(value, *result);
    EXPECT_EQ(std::signbit(value), std::signbit(*result));
  }

  // The mantissa's decimal position may oppose even a huge explicit exponent.
  for (auto text : {std::string(400, '9'),
                   "0." + std::string(400, '0') + "1e999999999999999999999999"}) {
    auto result = castNumber<Type>(text);
    ASSERT_TRUE(result);
    EXPECT_EQ(magnitudeBits(*result), InfinityBits<Type>);
  }
  for (auto text : {"0." + std::string(400, '0') + "1",
                   std::string(400, '9') + "e-999999999999999999999999"}) {
    auto result = castNumber<Type>(text);
    ASSERT_TRUE(result);
    EXPECT_EQ(*result, Type(0));
  }
  for (std::string_view text : {"0e999999999999999999999999", "-0e-999999999999999999999999"}) {
    auto result = castNumber<Type>(text);
    ASSERT_TRUE(result);
    EXPECT_EQ(*result, Type(0));
    EXPECT_EQ(std::signbit(*result), text[0] == '-');
  }

  // Failed full consumption still exposes the parsed prefix through tryCast.
  Type value = Type(37);
  EXPECT_FALSE(tryNumber(value, "-1e99999x"));
  EXPECT_EQ(magnitudeBits(value), InfinityBits<Type>);
  EXPECT_TRUE(std::signbit(value));
  EXPECT_FALSE(tryNumber(value, "-1e-99999x"));
  EXPECT_EQ(value, Type(0));
  EXPECT_TRUE(std::signbit(value));
  EXPECT_FALSE(castNumber<Type>("1e99999x"));
  EXPECT_FALSE(castNumber<Type>("1e-99999x"));
}

}

TEST(LexicalCastTest, IntegersPreserveSignednessAndBoundaryValues) {
  auto minimum = castNumber<std::int64_t>("-9223372036854775808");
  ASSERT_TRUE(minimum);
  EXPECT_EQ(*minimum, std::numeric_limits<std::int64_t>::min());
  auto maximum = castNumber<std::int64_t>("9223372036854775807");
  ASSERT_TRUE(maximum);
  EXPECT_EQ(*maximum, std::numeric_limits<std::int64_t>::max());
  auto unsignedMaximum = castNumber<std::uint64_t>("18446744073709551615");
  ASSERT_TRUE(unsignedMaximum);
  EXPECT_EQ(*unsignedMaximum, std::numeric_limits<std::uint64_t>::max());
  EXPECT_FALSE(castNumber<std::uint64_t>("-1"));
  EXPECT_FALSE(castNumber<std::uint64_t>("-0"));
  auto narrowMinimum = castNumber<std::int8_t>("-128");
  ASSERT_TRUE(narrowMinimum);
  EXPECT_EQ(*narrowMinimum, -128);
  auto narrowMaximum = castNumber<std::uint8_t>("255");
  ASSERT_TRUE(narrowMaximum);
  EXPECT_EQ(*narrowMaximum, 255);
  auto leadingZeros = castNumber<int>("-000001");
  ASSERT_TRUE(leadingZeros);
  EXPECT_EQ(*leadingZeros, -1);
}

TEST(LexicalCastTest, IntegerOverflowSucceedsWithoutChangingTheDestination) {
  for (std::string_view text : {"9223372036854775808", "-9223372036854775809",
                   "18446744073709551616", "999999999999999999999999999999999999"}) {
    std::int64_t value = 37;
    EXPECT_TRUE(tryNumber(value, text));
    EXPECT_EQ(value, 37);
    auto result = castNumber<std::int64_t>(text);
    ASSERT_TRUE(result);
    EXPECT_EQ(*result, 0);
    auto maybe = maybeLexicalCast<std::int64_t>(std::string_view(text));
    ASSERT_TRUE(maybe);
    EXPECT_EQ(*maybe, 0);
  }
  std::uint64_t unsignedValue = 37;
  EXPECT_TRUE(tryNumber(unsignedValue, "18446744073709551616"));
  EXPECT_EQ(unsignedValue, 37u);
  std::int8_t narrowValue = 37;
  EXPECT_TRUE(tryNumber(narrowValue, "128"));
  EXPECT_EQ(narrowValue, 37);
  std::uint8_t narrowUnsignedValue = 37;
  EXPECT_TRUE(tryNumber(narrowUnsignedValue, "256"));
  EXPECT_EQ(narrowUnsignedValue, 37);
  std::int64_t value = 37;
  EXPECT_FALSE(tryNumber(value, "9223372036854775808x"));
  EXPECT_EQ(value, 37);
}

TEST(LexicalCastTest, UnsignedOverflowDoesNotWrapTwentyDigitTokens) {
  for (std::string_view text : {"28446744073709551616", "30000000000000000000",
                   "000030000000000000000000", "99999999999999999999"}) {
    std::uint64_t value = 37;
    EXPECT_TRUE(tryNumber(value, text));
    EXPECT_EQ(value, 37u);
    auto result = castNumber<std::uint64_t>(text);
    ASSERT_TRUE(result);
    EXPECT_EQ(*result, 0u);
  }
  std::uint64_t value = 37;
  EXPECT_FALSE(tryNumber(value, "30000000000000000000x"));
  EXPECT_EQ(value, 37u);
  EXPECT_FALSE(castNumber<std::uint64_t>("30000000000000000000x"));
}

TEST(LexicalCastTest, NumbersRequireTheEntireUnpaddedDecimalToken) {
  for (std::string_view text : {"", "-", "+1", " 1", "1 ", "1x", "0x1", "1e", "1e+", "1,5"}) {
    auto integer = castNumber<int>(text);
    ASSERT_FALSE(integer);
    EXPECT_TRUE(integer.error().is<BadLexicalCast>());
    auto floating = castNumber<double>(text);
    ASSERT_FALSE(floating);
    EXPECT_TRUE(floating.error().is<BadLexicalCast>());
  }
  EXPECT_FALSE(castNumber<int>(std::string_view("1\0", 2)));
  EXPECT_FALSE(castNumber<double>(std::string_view("1\0", 2)));
  int integer = 37;
  EXPECT_FALSE(tryNumber(integer, "12x"));
  EXPECT_EQ(integer, 12);
  double floating = 37;
  EXPECT_FALSE(tryNumber(floating, "1.25x"));
  EXPECT_EQ(floating, 1.25);
}

TEST(LexicalCastTest, FloatingRangeErrorsProduceSignedInfinityAndZero) {
  checkFloatingRanges<float>();
  checkFloatingRanges<double>();
}

TEST(LexicalCastTest, FloatingRoundingPreservesNormalAndSubnormalBoundaries) {
  auto floatMaximum = castNumber<float>("3.4028234663852886e38");
  ASSERT_TRUE(floatMaximum);
  EXPECT_EQ(*floatMaximum, std::numeric_limits<float>::max());
  auto floatSubnormal = castNumber<float>("1.401298464324817e-45");
  ASSERT_TRUE(floatSubnormal);
  EXPECT_EQ(*floatSubnormal, std::numeric_limits<float>::denorm_min());
  auto floatHalf = castNumber<float>("7.006492321624085e-46");
  ASSERT_TRUE(floatHalf);
  EXPECT_EQ(*floatHalf, 0.0f);
  auto floatAboveHalf = castNumber<float>("7.006492321624086e-46");
  ASSERT_TRUE(floatAboveHalf);
  EXPECT_EQ(*floatAboveHalf, std::numeric_limits<float>::denorm_min());
  auto doubleMaximum = castNumber<double>("1.7976931348623157e308");
  ASSERT_TRUE(doubleMaximum);
  EXPECT_EQ(*doubleMaximum, std::numeric_limits<double>::max());
  auto doubleSubnormal = castNumber<double>("4.9406564584124654e-324");
  ASSERT_TRUE(doubleSubnormal);
  EXPECT_EQ(*doubleSubnormal, std::numeric_limits<double>::denorm_min());
  auto doubleHalf = castNumber<double>("2.4703282292062327e-324");
  ASSERT_TRUE(doubleHalf);
  EXPECT_EQ(*doubleHalf, 0.0);
  auto doubleAboveHalf = castNumber<double>("2.4703282292062328e-324");
  ASSERT_TRUE(doubleAboveHalf);
  EXPECT_EQ(*doubleAboveHalf, std::numeric_limits<double>::denorm_min());
}

TEST(LexicalCastTest, FloatingSpecialValuesKeepSignsAndDiscardNanPayloads) {
  for (std::string_view text : {"inf", "-INF", "infinity", "-Infinity"}) {
    auto result = castNumber<double>(text);
    ASSERT_TRUE(result);
    EXPECT_EQ(magnitudeBits(*result), InfinityBits<double>);
    EXPECT_EQ(std::signbit(*result), text[0] == '-');
  }
  for (std::string_view text : {"NaN", "-NaN", "nan()", "nan(123)", "nan(foo_bar)", "-nan(123)"}) {
    auto result = castNumber<double>(text);
    ASSERT_TRUE(result);
    // Observe the object representation without producing NaNs through fast-math FP expressions.
    std::uint64_t bits;
    std::memcpy(&bits, &*result, sizeof(bits));
    EXPECT_EQ(bits, text[0] == '-' ? 0xfff8000000000000ULL : 0x7ff8000000000000ULL);
  }
  for (std::string_view text : {"+inf", "nan(a-b)", "nan(foo", "infinityx"})
    EXPECT_FALSE(castNumber<double>(text));
}
